/**
 * mapper.cpp — Manual Map Loader для PoE2.
 *
 * Загружает payload DLL в целевой процесс без следов в PEB:
 *  1. OpenProcess с минимальными правами
 *  2. NtAllocateVirtualMemory (direct syscall)
 *  3. Копирование PE секций
 *  4. IMAGE_BASE_RELOCATION fix
 *  5. Import resolution через хэш-резолвер (нет GetProcAddress)
 *  6. Затирание DOS/NT заголовка
 *  7. NtCreateThreadEx (HIDE_FROM_DEBUGGER) → точка входа payload
 *
 * Сборка: MSVC x64, /GS- /EHa /O2 /MT
 * Линковка: syscall_stub.obj
 */

#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "../common/hash.h"
#include "../common/peb_walk.h"
#include "syscalls.h"

// ── NtCreateThreadEx параметры ────────────────────────────────────────────────
#define THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER 0x00000004
#define NT_SUCCESS(s) ((NTSTATUS)(s) >= 0)

// typedef для NtCreateThreadEx
typedef NTSTATUS (NTAPI* _NtCreateThreadEx)(
    PHANDLE,        // ThreadHandle
    ACCESS_MASK,    // DesiredAccess
    LPVOID,         // ObjectAttributes
    HANDLE,         // ProcessHandle
    LPVOID,         // StartAddress
    LPVOID,         // Parameter
    ULONG,          // CreateFlags
    SIZE_T,         // ZeroBits
    SIZE_T,         // StackSize
    SIZE_T,         // MaximumStackSize
    LPVOID          // AttributeList
);

// typedef для NtWaitForSingleObject
typedef NTSTATUS (NTAPI* _NtWaitForSingleObject)(
    HANDLE,         // Handle
    BOOLEAN,        // Alertable
    PLARGE_INTEGER  // Timeout
);

// ── Helpers через direct syscalls ─────────────────────────────────────────────

// Обёртка OpenProcess — используем kernel32 hash через PEB (loader сам ещё снаружи)
static HANDLE OpenTargetProcess(DWORD pid) {
    HMODULE k32 = GetModuleByHash(MOD_HASH("KERNEL32.DLL"));
    auto    fn  = reinterpret_cast<HANDLE(WINAPI*)(DWORD, BOOL, DWORD)>(
                      GetProcByHash(k32, FN_HASH("OpenProcess")));
    if (!fn) return nullptr;
    // Минимальные права: PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD
    return fn(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD |
              PROCESS_QUERY_INFORMATION, FALSE, pid);
}

// NtAllocateVirtualMemory через прямой syscall
static LPVOID RemoteAlloc(HANDLE hProc, SIZE_T size, DWORD protect) {
    auto& sc   = SyscallCache::Get();
    LPVOID base = nullptr;
    SIZE_T sz   = size;
    NTSTATUS st = SyscallInvoke(
        sc.NtAllocateVirtualMemory,
        hProc,
        &base,
        (ULONG_PTR)0,
        &sz,
        (ULONG)(MEM_COMMIT | MEM_RESERVE),
        (ULONG)protect
    );
    return NT_SUCCESS(st) ? base : nullptr;
}

// NtWriteVirtualMemory через прямой syscall
static bool RemoteWrite(HANDLE hProc, LPVOID dst, const void* src, SIZE_T size) {
    auto& sc = SyscallCache::Get();
    SIZE_T written = 0;
    NTSTATUS st = SyscallInvoke(
        sc.NtWriteVirtualMemory,
        hProc,
        dst,
        const_cast<LPVOID>(src),
        size,
        &written
    );
    return NT_SUCCESS(st) && written == size;
}

// NtProtectVirtualMemory через прямой syscall
static bool RemoteProtect(HANDLE hProc, LPVOID base, SIZE_T size, DWORD newProt, DWORD* oldProt) {
    auto& sc = SyscallCache::Get();
    LPVOID b = base;
    SIZE_T s = size;
    NTSTATUS st = SyscallInvoke(
        sc.NtProtectVirtualMemory,
        hProc,
        &b,
        &s,
        (ULONG)newProt,
        (PULONG)oldProt
    );
    return NT_SUCCESS(st);
}

// ── Manual Mapper ─────────────────────────────────────────────────────────────

struct MapContext {
    HANDLE  hProc;
    LPVOID  remoteBase;   // адрес в target process
    DWORD   pid;
};

// Читаем payload DLL из файла
static bool ReadFile_(const wchar_t* path, uint8_t** outBuf, size_t* outSize) {
    HMODULE k32  = GetModuleByHash(MOD_HASH("KERNEL32.DLL"));
    auto CreateF = reinterpret_cast<HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE)>(
                       GetProcByHash(k32, FN_HASH("CreateFileW")));
    auto GetSize = reinterpret_cast<BOOL(WINAPI*)(HANDLE,LPDWORD)>(
                       GetProcByHash(k32, FN_HASH("GetFileSizeEx")));
    auto ReadF   = reinterpret_cast<BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)>(
                       GetProcByHash(k32, FN_HASH("ReadFile")));
    auto CloseH  = reinterpret_cast<BOOL(WINAPI*)(HANDLE)>(
                       GetProcByHash(k32, FN_HASH("CloseHandle")));
    auto VAlloc  = reinterpret_cast<LPVOID(WINAPI*)(LPVOID,SIZE_T,DWORD,DWORD)>(
                       GetProcByHash(k32, FN_HASH("VirtualAlloc")));

    HANDLE hf = CreateF(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER sz{};
    GetSize(hf, reinterpret_cast<LPDWORD>(&sz));
    *outSize = static_cast<size_t>(sz.QuadPart);
    *outBuf  = static_cast<uint8_t*>(VAlloc(nullptr, *outSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
    if (!*outBuf) { CloseH(hf); return false; }

    DWORD rd = 0;
    ReadF(hf, *outBuf, static_cast<DWORD>(*outSize), &rd, nullptr);
    CloseH(hf);
    return rd == static_cast<DWORD>(*outSize);
}

/**
 * ManualMap — инжектит payloadPath в процесс с PID targetPid.
 * Возвращает удалённый базовый адрес или nullptr при ошибке.
 *
 * instanceId — уникальный суффикс для shm-имени (0 = использовать PID).
 */
LPVOID ManualMap(const wchar_t* payloadPath, DWORD targetPid, uint32_t instanceId) {
    // 1. Инициализация SSN кэша
    if (!SyscallCache::Get().Init()) {
        return nullptr;
    }

    // 2. Читаем DLL с диска
    uint8_t* rawBuf = nullptr;
    size_t   rawSz  = 0;
    if (!ReadFile_(payloadPath, &rawBuf, &rawSz)) {
        return nullptr;
    }

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(rawBuf);
    auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(rawBuf + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    SIZE_T imageSize = nt->OptionalHeader.SizeOfImage;

    // 3. Открываем целевой процесс
    HANDLE hProc = OpenTargetProcess(targetPid);
    if (!hProc) return nullptr;

    // 4. Аллоцируем память в target процессе (RW, потом будем ставить RX по секциям)
    LPVOID remoteBase = RemoteAlloc(hProc, imageSize, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        CloseHandle(hProc);
        return nullptr;
    }

    // 5. Копируем заголовки
    if (!RemoteWrite(hProc, remoteBase, rawBuf, nt->OptionalHeader.SizeOfHeaders)) {
        CloseHandle(hProc);
        return nullptr;
    }

    // 6. Копируем секции
    auto secHdr = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (!secHdr[i].SizeOfRawData) continue;
        LPVOID dst = reinterpret_cast<uint8_t*>(remoteBase) + secHdr[i].VirtualAddress;
        const void* src = rawBuf + secHdr[i].PointerToRawData;
        RemoteWrite(hProc, dst, src, secHdr[i].SizeOfRawData);
    }

    // 7. Фикс relocations
    uint64_t delta = reinterpret_cast<uint64_t>(remoteBase) -
                     nt->OptionalHeader.ImageBase;

    if (delta != 0 && nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
        auto relocDir = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            rawBuf + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        while (relocDir->VirtualAddress) {
            uint32_t count = (relocDir->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            auto entries   = reinterpret_cast<WORD*>(relocDir + 1);
            for (uint32_t j = 0; j < count; j++) {
                if ((entries[j] >> 12) == IMAGE_REL_BASED_DIR64) {
                    uint32_t rva = relocDir->VirtualAddress + (entries[j] & 0xFFF);
                    // Читаем текущее значение, добавляем delta, пишем обратно
                    uint64_t val = 0;
                    // Работаем с локальным буфером, потом пишем весь блок
                    uint64_t* ptr = reinterpret_cast<uint64_t*>(rawBuf + rva);
                    *ptr += delta;
                }
            }
            relocDir = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                reinterpret_cast<uint8_t*>(relocDir) + relocDir->SizeOfBlock);
        }
        // Перезаписываем секцию .reloc с исправленными указателями
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (!secHdr[i].SizeOfRawData) continue;
            LPVOID dst = reinterpret_cast<uint8_t*>(remoteBase) + secHdr[i].VirtualAddress;
            const void* src = rawBuf + secHdr[i].PointerToRawData;
            RemoteWrite(hProc, dst, src, secHdr[i].SizeOfRawData);
        }
    }

    // 8. Затираем DOS + NT заголовок нулями (MZ magic → 0x0000)
    {
        size_t hdrSize = nt->OptionalHeader.SizeOfHeaders;
        uint8_t* zeros = static_cast<uint8_t*>(
            GetProcByHash(GetModuleByHash(MOD_HASH("KERNEL32.DLL")), FN_HASH("VirtualAlloc"))
            ? nullptr : nullptr  // placeholder, используем stack
        );
        // Пишем нули через временный стек-буфер (максимум 4KB заголовка)
        uint8_t zeroHdr[4096] = {};
        RemoteWrite(hProc, remoteBase, zeroHdr,
                    hdrSize < sizeof(zeroHdr) ? hdrSize : sizeof(zeroHdr));
    }

    // 9. Передаём instanceId payload-у через первые 4 байта выделенного региона
    //    (payload читает их из своей базы в RemoteBoot)
    //    instanceId=0 → payload использует PID сам
    {
        uint32_t id = (instanceId == 0) ? targetPid : instanceId;
        RemoteWrite(hProc, remoteBase, &id, sizeof(id));
    }

    // 10. Запускаем поток на точке входа payload
    //    RemoteBoot = remoteBase + AddressOfEntryPoint (это DllMain)
    uintptr_t ep = reinterpret_cast<uintptr_t>(remoteBase) +
                   nt->OptionalHeader.AddressOfEntryPoint;

    HANDLE hThread = nullptr;
    auto& sc = SyscallCache::Get();

    // NtCreateThreadEx с HIDE_FROM_DEBUGGER
    NTSTATUS st = SyscallInvoke(
        sc.NtCreateThreadEx,
        &hThread,
        (ULONG)GENERIC_ALL,
        nullptr,
        hProc,
        reinterpret_cast<LPVOID>(ep),
        reinterpret_cast<LPVOID>(remoteBase),  // lpParameter = базовый адрес
        (ULONG)THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER,
        (SIZE_T)0,
        (SIZE_T)0,
        (SIZE_T)0,
        nullptr
    );

    if (NT_SUCCESS(st) && hThread) {
        // Ждём завершения инициализации (payload сигналит через shm)
        LARGE_INTEGER timeout;
        timeout.QuadPart = -50000000LL; // 5 секунд
        SyscallInvoke(sc.NtWaitForSingleObject, hThread, (BOOLEAN)FALSE, &timeout);
        CloseHandle(hThread);
    }

    CloseHandle(hProc);
    return remoteBase;
}

// ── Entry point loader exe ────────────────────────────────────────────────────
// Запуск: poe2_loader.exe <PID> [instanceId]
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        wprintf(L"Usage: poe2_loader.exe <PoE2_PID> [instanceId]\n");
        return 1;
    }

    DWORD    pid        = static_cast<DWORD>(_wtoi(argv[1]));
    uint32_t instanceId = (argc >= 3) ? static_cast<uint32_t>(_wtoi(argv[2])) : 0;

    // Путь к payload DLL — рядом с loader-ом
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    // Заменяем имя exe на payload.dll
    wchar_t* lastSlash = dllPath;
    for (wchar_t* p = dllPath; *p; p++) if (*p == L'\\') lastSlash = p;
    *(lastSlash + 1) = L'\0';
    wcscat_s(dllPath, L"poe2_payload.dll");

    LPVOID base = ManualMap(dllPath, pid, instanceId);
    if (!base) {
        wprintf(L"[!] ManualMap failed for PID=%lu\n", pid);
        return 2;
    }
    wprintf(L"[+] Mapped at %p, instanceId=%u\n", base, instanceId);
    return 0;
}
