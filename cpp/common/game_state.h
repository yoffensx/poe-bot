/**
 * game_state.h — Shared structs между C++ payload и Python bridge.
 * POD layout должен совпадать с ctypes/struct.pack на Python стороне.
 *
 * Shared memory layout:
 *   [0..3]   uint32_t  magic       = 0x504F4532 ('POE2')
 *   [4..7]   uint32_t  version     = 1
 *   [8..11]  uint32_t  instanceId  (PID или кастомный)
 *   [12..15] uint32_t  frameSeq    (монотонный счётчик кадров)
 *   [16...]  GameStatePacket
 */
#pragma once
#include <cstdint>

#pragma pack(push, 1)

static constexpr uint32_t SHM_MAGIC   = 0x504F4532u; // 'POE2'
static constexpr uint32_t SHM_VERSION = 1u;
static constexpr size_t   SHM_SIZE    = 65536;        // 64 KB — с запасом

struct Vec2f {
    float x, y;
};

struct Vec3f {
    float x, y, z;
};

struct MonsterEntry {
    Vec3f  pos;           // мировые координаты
    uint32_t hp;
    uint32_t hpMax;
    uint32_t id;          // уникальный entity id
    uint8_t  type;        // 0=normal, 1=magic, 2=rare, 3=unique
    uint8_t  isAlive;
    uint8_t  _pad[2];
};

static constexpr uint32_t MAX_MONSTERS  = 128;
static constexpr uint32_t MAX_MAP_MODS  = 32;

struct GameStatePacket {
    // Player
    uint32_t hp;
    uint32_t hpMax;
    uint32_t mana;
    uint32_t manaMax;
    uint32_t es;           // Energy Shield
    uint32_t esMax;
    Vec3f    playerPos;
    float    playerAngle;  // направление взгляда, радианы

    // Flask charges (6 слотов)
    uint8_t  flaskCharges[6];
    uint8_t  _padFlask[2];

    // Monsters
    uint32_t    monsterCount;
    MonsterEntry monsters[MAX_MONSTERS];

    // Map mods (хэши, расшифровка на Python стороне через mod_database.toml)
    uint32_t mapModCount;
    uint32_t mapModHashes[MAX_MAP_MODS];

    // Inventory (занятые слоты: битовая маска 12x5 = 60 бит → uint64_t)
    uint64_t inventoryMask;

    // Area info
    uint32_t areaHash;    // хэш имени акта/зоны
    uint8_t  isInMap;     // 1 = в карте (не в хайде)
    uint8_t  _pad2[3];

    // Временная метка последнего обновления (ms с эпохи)
    uint64_t timestampMs;
};

struct ShmHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t instanceId;
    uint32_t frameSeq;    // инкрементируется при каждом write
    uint8_t  writerReady; // 1 = payload инициализирован и пишет
    uint8_t  _pad[7];
    GameStatePacket state;
};

#pragma pack(pop)

static_assert(sizeof(ShmHeader) < SHM_SIZE, "ShmHeader too large");

// Имя shared memory: "Local\\poe2_gs_<instanceId>"
// instanceId = PID target процесса по умолчанию
inline void BuildShmName(char* buf, size_t bufLen, uint32_t instanceId) {
    // snprintf без CRT: используем простой форматтер
    // (payload не имеет CRT — нужен minimal impl)
    const char prefix[] = "Local\\poe2_gs_";
    size_t i = 0;
    while (i < bufLen - 1 && prefix[i]) { buf[i] = prefix[i]; i++; }
    // Конвертируем uint32 в decimal
    char tmp[12]; int j = 0;
    uint32_t n = instanceId;
    if (n == 0) { tmp[j++] = '0'; }
    while (n > 0) { tmp[j++] = '0' + (n % 10); n /= 10; }
    // Переворачиваем
    for (int k = 0; k < j / 2; k++) {
        char t = tmp[k]; tmp[k] = tmp[j-1-k]; tmp[j-1-k] = t;
    }
    for (int k = 0; k < j && i < bufLen - 1; k++) { buf[i++] = tmp[k]; }
    buf[i] = '\0';
}
