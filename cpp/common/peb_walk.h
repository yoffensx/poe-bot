/**
 * peb_walk.h — GetModuleHandle и GetProcAddress-аналоги через PEB walk + EAT walk.
 * Нет вызовов kernel32 / ntdll через IAT — всё через TEB→PEB→LdrData.
 */
#pragma once
#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include "hash.h"

// Резолвим HMODULE по хэшу имени модуля через PEB.Ldr
inline HMODULE GetModuleByHash(uint32_t nameHash) {
    // TEB→PEB→Ldr→InMemoryOrderModuleList
    PEB* peb = reinterpret_cast<PEB*>(__readgsqword(0x60));
    auto head = &peb->Ldr->InMemoryOrderModuleList;
    for (auto entry = head->Flink; entry != head; entry = entry->Flink) {
        auto data = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        if (!data->FullDllName.Buffer) continue;

        // Сравниваем по basename (после последнего '\')
        wchar_t* base = data->FullDllName.Buffer;
        wchar_t* p    = base;
        while (*p) p++;
        while (p > base && *(p - 1) != L'\\') p--;

        if (HashWStrRuntime(p) == nameHash)
            return reinterpret_cast<HMODULE>(data->DllBase);
    }
    return nullptr;
}

// EAT walk по хэшу имени функции
inline FARPROC GetProcByHash(HMODULE mod, uint32_t fnHash) {
    if (!mod) return nullptr;
    auto base    = reinterpret_cast<uint8_t*>(mod);
    auto dos     = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt      = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto expDir  = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    auto names   = reinterpret_cast<DWORD*>(base + expDir->AddressOfNames);
    auto ords    = reinterpret_cast<WORD*> (base + expDir->AddressOfNameOrdinals);
    auto funcs   = reinterpret_cast<DWORD*>(base + expDir->AddressOfFunctions);

    for (DWORD i = 0; i < expDir->NumberOfNames; i++) {
        const char* name = reinterpret_cast<const char*>(base + names[i]);
        if (HashStrRuntime(name) == fnHash)
            return reinterpret_cast<FARPROC>(base + funcs[ords[i]]);
    }
    return nullptr;
}

// Удобные макросы
#define MOD_HASH(s)  (HashStr(s))
#define FN_HASH(s)   (HashStr(s))
