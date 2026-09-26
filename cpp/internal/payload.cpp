/**
 * payload.cpp — Internal payload DLL для PoE2.
 * Manual-mapped → нет CRT, нет IAT, нет PE заголовка после загрузки.
 *
 * Что делает:
 *  1. AOB scan GameState ptr в памяти процесса
 *  2. Читает HP/Mana/Pos/Monsters/MapMods из GameState
 *  3. Пишет в named shared memory "Local\poe2_gs_<instanceId>"
 *  4. Обновляет 30 Hz через Sleep loop
 */

#include <windows.h>
#include <cstdint>
#include <cstring>
#include "../common/hash.h"
#include "../common/peb_walk.h"
#include "../common/game_state.h"

// ── Типы WinAPI без CRT ──────────────────────────────────────────────────────
using FnCreateFileMappingA = HANDLE(WINAPI*)(HANDLE,LPSECURITY_ATTRIBUTES,DWORD,DWORD,DWORD,LPCSTR);
using FnMapViewOfFile      = LPVOID(WINAPI*)(HANDLE,DWORD,DWORD,DWORD,SIZE_T);
using FnSleep              = void(WINAPI*)(DWORD);
using FnGetTickCount64     = ULONGLONG(WINAPI*)();
using FnQueryPerformanceCounter   = BOOL(WINAPI*)(LARGE_INTEGER*);
using FnQueryPerformanceFrequency = BOOL(WINAPI*)(LARGE_INTEGER*);
using FnVirtualQuery       = SIZE_T(WINAPI*)(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
using FnFlushViewOfFile    = BOOL(WINAPI*)(LPCVOID,SIZE_T);

struct WinApi {
    FnCreateFileMappingA CreateFileMappingA;
    FnMapViewOfFile      MapViewOfFile;
    FnSleep              Sleep;
    FnGetTickCount64     GetTickCount64;
    FnVirtualQuery       VirtualQuery;
    FnFlushViewOfFile    FlushViewOfFile;

    bool Init() {
        HMODULE k32 = GetModuleByHash(MOD_HASH("KERNEL32.DLL"));
        if (!k32) return false;
        CreateFileMappingA = (FnCreateFileMappingA)GetProcByHash(k32, FN_HASH("CreateFileMappingA"));
        MapViewOfFile      = (FnMapViewOfFile)     GetProcByHash(k32, FN_HASH("MapViewOfFile"));
        Sleep              = (FnSleep)             GetProcByHash(k32, FN_HASH("Sleep"));
        GetTickCount64     = (FnGetTickCount64)    GetProcByHash(k32, FN_HASH("GetTickCount64"));
        VirtualQuery       = (FnVirtualQuery)      GetProcByHash(k32, FN_HASH("VirtualQuery"));
        FlushViewOfFile    = (FnFlushViewOfFile)   GetProcByHash(k32, FN_HASH("FlushViewOfFile"));
        return CreateFileMappingA && MapViewOfFile && Sleep;
    }
};

// ── AOB Pattern Scanner ───────────────────────────────────────────────────────
// Сканируем регионы памяти процесса по паттерну с wildcards ('?')
static const uint8_t* AobScan(WinApi& api,
                               const uint8_t* pattern, const char* mask,
                               uintptr_t start, uintptr_t end)
{
    size_t patLen = 0;
    while (mask[patLen]) patLen++;

    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = start;
    while (addr < end) {
        if (!api.VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)))
            break;
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & PAGE_GUARD))
        {
            auto base  = reinterpret_cast<const uint8_t*>(mbi.BaseAddress);
            size_t sz  = mbi.RegionSize;
            for (size_t i = 0; i + patLen <= sz; i++) {
                bool match = true;
                for (size_t j = 0; j < patLen && match; j++) {
                    if (mask[j] == 'x' && base[i + j] != pattern[j])
                        match = false;
                }
                if (match) return base + i;
            }
        }
        addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return nullptr;
}

// ── PoE2 Offsets (обновлять при патчах GGG) ──────────────────────────────────
// Паттерн указывает на структуру GameState.
// Wildcard маска: 'x' = совпадение, '?' = любой байт.
//
// Pattern для PlayerActor → HP/Mana (типично для ARPG: 4-байтные int поля)
// Это примерный шаблон — реальные офсеты определяются через Cheat Engine/ReClass.
namespace Offsets {
    // Офсеты внутри GameState (определяются реверсом)
    constexpr uint32_t HP        = 0x1A4;
    constexpr uint32_t HPMax     = 0x1A8;
    constexpr uint32_t Mana      = 0x1B0;
    constexpr uint32_t ManaMax   = 0x1B4;
    constexpr uint32_t ES        = 0x1BC;
    constexpr uint32_t ESMax     = 0x1C0;
    constexpr uint32_t PosX      = 0x230;
    constexpr uint32_t PosY      = 0x234;
    constexpr uint32_t PosZ      = 0x238;
    constexpr uint32_t Angle     = 0x240;
    // Flask charges array (6 байт с шагом 4)
    constexpr uint32_t Flask0    = 0x2A0;
    constexpr uint32_t FlaskStep = 0x04;

    // Pointer chain к списку монстров (пример)
    constexpr uint32_t MonsterListPtr    = 0x3C0;  // ptr → MonsterArray
    constexpr uint32_t MonsterCountOff   = 0x08;   // MonsterArray+0x08 = count
    constexpr uint32_t MonsterEntrySize  = 0x80;   // sizeof(MonsterEntry) in PoE2
    constexpr uint32_t MonEHPOff         = 0x1A4;
    constexpr uint32_t MonEHPMaxOff      = 0x1A8;
    constexpr uint32_t MonEPosXOff       = 0x230;
    constexpr uint32_t MonEPosYOff       = 0x234;
    constexpr uint32_t MonEPosZOff       = 0x238;
    constexpr uint32_t MonEIdOff         = 0x04;
    constexpr uint32_t MonETypeOff       = 0x110;  // rarity byte
}

// ── Safe read helpers ─────────────────────────────────────────────────────────
template<typename T>
inline T SafeRead(const void* ptr) {
    __try {
        return *reinterpret_cast<const T*>(ptr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
}

template<typename T>
inline T SafeReadOff(uintptr_t base, uint32_t off) {
    return SafeRead<T>(reinterpret_cast<const void*>(base + off));
}

// ── Shared Memory Setup ───────────────────────────────────────────────────────
static HANDLE   g_hMapping = nullptr;
static ShmHeader* g_shm   = nullptr;

static bool SetupShm(WinApi& api, uint32_t instanceId) {
    char name[64];
    BuildShmName(name, sizeof(name), instanceId);

    g_hMapping = api.CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, 0, SHM_SIZE, name);
    if (!g_hMapping) return false;

    g_shm = reinterpret_cast<ShmHeader*>(
        api.MapViewOfFile(g_hMapping, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE));
    if (!g_shm) return false;

    g_shm->magic      = SHM_MAGIC;
    g_shm->version    = SHM_VERSION;
    g_shm->instanceId = instanceId;
    g_shm->frameSeq   = 0;
    g_shm->writerReady = 0;
    return true;
}

// ── Main Worker Thread ────────────────────────────────────────────────────────
static DWORD WINAPI WorkerThread(LPVOID param) {
    WinApi api;
    if (!api.Init()) return 1;

    // instanceId передаётся через param (из loader: remoteBase первые 4 байта)
    uint32_t instanceId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param));
    if (instanceId == 0) {
        // Fallback: используем PID
        HMODULE k32 = GetModuleByHash(MOD_HASH("KERNEL32.DLL"));
        auto GetPid = (DWORD(WINAPI*)())GetProcByHash(k32, FN_HASH("GetCurrentProcessId"));
        if (GetPid) instanceId = GetPid();
    }

    if (!SetupShm(api, instanceId)) return 2;

    // AOB scan: ищем GameState ptr (паттерн подбирается под текущий патч)
    // Pattern: "PlayerActor" structure magic bytes в PoE2 heap
    // ?? ?? ?? ?? заменяются реальным паттерном после реверса
    static const uint8_t kPattern[] = { 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0 };
    static const char    kMask[]    = "xxx????xxx";

    // Диапазон сканирования: основной образ PoE2
    HMODULE hPoe2 = GetModuleByHash(MOD_HASH("PathOfExile2.exe"));
    uintptr_t scanStart = reinterpret_cast<uintptr_t>(hPoe2);
    uintptr_t scanEnd   = scanStart + 0x08000000; // 128MB от базы

    const uint8_t* hit = AobScan(api, kPattern, kMask, scanStart, scanEnd);
    uintptr_t gsBase = 0;
    if (hit) {
        // Разыменовываем rip-relative ptr: инструкция lea rax,[rip+диспл32]
        int32_t disp = SafeRead<int32_t>(hit + 3);
        gsBase = reinterpret_cast<uintptr_t>(hit) + 7 + disp;
        gsBase = SafeRead<uintptr_t>(reinterpret_cast<const void*>(gsBase)); // разыменование
    }

    g_shm->writerReady = 1;

    // 30 Hz update loop
    while (true) {
        GameStatePacket pkt{};
        pkt.timestampMs = api.GetTickCount64();

        if (gsBase) {
            pkt.hp       = SafeReadOff<uint32_t>(gsBase, Offsets::HP);
            pkt.hpMax    = SafeReadOff<uint32_t>(gsBase, Offsets::HPMax);
            pkt.mana     = SafeReadOff<uint32_t>(gsBase, Offsets::Mana);
            pkt.manaMax  = SafeReadOff<uint32_t>(gsBase, Offsets::ManaMax);
            pkt.es       = SafeReadOff<uint32_t>(gsBase, Offsets::ES);
            pkt.esMax    = SafeReadOff<uint32_t>(gsBase, Offsets::ESMax);

            pkt.playerPos.x = SafeReadOff<float>(gsBase, Offsets::PosX);
            pkt.playerPos.y = SafeReadOff<float>(gsBase, Offsets::PosY);
            pkt.playerPos.z = SafeReadOff<float>(gsBase, Offsets::PosZ);
            pkt.playerAngle = SafeReadOff<float>(gsBase, Offsets::Angle);

            for (int i = 0; i < 6; i++)
                pkt.flaskCharges[i] = SafeReadOff<uint8_t>(
                    gsBase, Offsets::Flask0 + i * Offsets::FlaskStep);

            // Monsters
            uintptr_t monListPtr = SafeReadOff<uintptr_t>(gsBase, Offsets::MonsterListPtr);
            if (monListPtr) {
                uint32_t cnt = SafeReadOff<uint32_t>(monListPtr, Offsets::MonsterCountOff);
                cnt = cnt < MAX_MONSTERS ? cnt : MAX_MONSTERS;
                pkt.monsterCount = cnt;
                uintptr_t arr = monListPtr + 0x10;
                for (uint32_t m = 0; m < cnt; m++) {
                    uintptr_t e = arr + m * Offsets::MonsterEntrySize;
                    pkt.monsters[m].hp     = SafeReadOff<uint32_t>(e, Offsets::MonEHPOff);
                    pkt.monsters[m].hpMax  = SafeReadOff<uint32_t>(e, Offsets::MonEHPMaxOff);
                    pkt.monsters[m].pos.x  = SafeReadOff<float>(e, Offsets::MonEPosXOff);
                    pkt.monsters[m].pos.y  = SafeReadOff<float>(e, Offsets::MonEPosYOff);
                    pkt.monsters[m].pos.z  = SafeReadOff<float>(e, Offsets::MonEPosZOff);
                    pkt.monsters[m].id     = SafeReadOff<uint32_t>(e, Offsets::MonEIdOff);
                    pkt.monsters[m].type   = SafeReadOff<uint8_t>(e, Offsets::MonETypeOff);
                    pkt.monsters[m].isAlive = (pkt.monsters[m].hp > 0) ? 1 : 0;
                }
            }
        }

        // Атомарная запись в shm
        g_shm->state    = pkt;
        g_shm->frameSeq++;

        api.Sleep(33); // ~30 Hz
    }

    return 0;
}

// ── DllMain ───────────────────────────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    // lpReserved = remoteBase (loader передаёт базовый адрес)
    // Первые 4 байта = instanceId
    uint32_t instanceId = 0;
    if (lpReserved) {
        __try {
            instanceId = *reinterpret_cast<uint32_t*>(lpReserved);
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    HMODULE k32 = GetModuleByHash(MOD_HASH("KERNEL32.DLL"));
    auto CreateThread_ = (HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD))
                         GetProcByHash(k32, FN_HASH("CreateThread"));
    auto CloseHandle_  = (BOOL(WINAPI*)(HANDLE))
                         GetProcByHash(k32, FN_HASH("CloseHandle"));

    HANDLE hT = CreateThread_(nullptr, 0, WorkerThread,
                               reinterpret_cast<LPVOID>(static_cast<uintptr_t>(instanceId)),
                               0, nullptr);
    if (hT && CloseHandle_) CloseHandle_(hT);
    return TRUE;
}
