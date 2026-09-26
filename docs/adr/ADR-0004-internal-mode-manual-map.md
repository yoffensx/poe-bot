# ADR-0004: Internal Mode — Manual Map Injection (опциональный слой)

- **Статус:** Принято (опциональный режим)
- **Дата:** 2026-09-26
- **Авторы:** Maik

---

## Контекст

External/OCR-режим покрывает 90% задач бота, но имеет ограничения:
- OCR латентность ~20ms на чтение модов/итемов
- Минимап даёт только цветовые точки — нет точных координат врагов
- HP/Mana в % от орбы, не точные числа
- Нет доступа к GameState (инвентарь, скиллы, флаги)

Для precision-задач (точная навигация, exact HP threshold, GameState-driven решения) нужен internal доступ.

## Решение

**Двурежимная архитектура:**

```
MODE=external  →  DXGI capture + OCR + CV  (по умолчанию, safer)
MODE=internal  →  manual-mapped DLL в PoE2 + GameState reader + shared memory → Python
```

Режим выбирается в `build_config.toml`:
```toml
[bot]
mode = "external"   # "external" | "internal"
```

---

## Internal Mode — Технический стек

### 1. Manual Map Loader (`cpp/loader/mapper.cpp`)

Загружаем payload DLL в процесс PoE2 **без следов** в PEB:

```cpp
// Шаги:
// 1. Открыть процесс — только PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD
// 2. NtAllocateVirtualMemory через direct syscall (Hell's Gate SSN resolution)
// 3. Скопировать PE секции с учётом виртуальных адресов
// 4. Фикс IMAGE_BASE_RELOCATION (delta = new_base - preferred_base)
// 5. Разрешить импорты через хэш-резолвер (djb2), без GetProcAddress
// 6. Затереть DOS/NT заголовок нулями (MZ magic → 0x00)
// 7. NtCreateThreadEx с THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER
// 8. Нет записи в PEB.LdrData → не виден в module list
```

Хэш-резолвер для импортов:
```cpp
constexpr uint32_t hash_name(const char* name) {
    uint32_t h = 5381;
    while (*name) h = ((h << 5) + h) ^ (uint8_t)*name++;
    return h;
}
// Резолвинг: walk EAT по хэшу, без строк в бинарнике
```

### 2. Payload DLL (`cpp/internal/payload.cpp`)

Внутри процесса PoE2 — минимальный footprint:

```cpp
// Что делает payload:
// 1. Найти GameState через pattern scan (AOB) в памяти процесса
// 2. Читать интересующие поля (HP, Mana, PlayerPos, MonsterList, MapMods)
// 3. Упаковать в GameStatePacket (POD struct)
// 4. Записать в named shared memory (CreateFileMappingA → "Local\\poe2_state")
// 5. Сигнализировать Python через named event

// NO: D3D hooks, NO: input hooks — только чтение памяти
// Минимальный след = минимальный риск
```

### 3. Shared Memory Bridge (`src/capture/internal_bridge.py`)

Python читает GameState из shared memory:

```python
import mmap, ctypes, struct

class InternalBridge:
    def __init__(self) -> None:
        self._shm = mmap.mmap(-1, 4096, "Local\\poe2_state")
    
    def read_state(self) -> InternalGameState:
        self._shm.seek(0)
        data = self._shm.read(PACKET_SIZE)
        return InternalGameState.from_bytes(data)

@dataclass
class InternalGameState:
    hp: int
    hp_max: int
    mana: int
    mana_max: int
    es: int
    player_x: float
    player_y: float
    monster_count: int
    monsters: list[MonsterEntry]  # pos + hp
    map_mods_hash: list[int]      # pre-hashed on C++ side
```

---

## Сравнение режимов

| Характеристика | External (OCR) | Internal (Manual Map) |
|---------------|---------------|----------------------|
| HP/Mana | % из пикселей (~2%) | точное число |
| Позиция игрока | пиксель минимапа | float coords |
| Враги | цветные точки на минимапе | позиция + HP каждого |
| Моды карты | OCR ~20ms | pre-hashed GameState |
| Инвентарь | template match | структуры напрямую |
| Латентность данных | 5-20ms | < 1ms (shared mem) |
| Риск детекта | низкий | средний (integrity scan) |
| Зависимость от патча | только UI layout | офсеты меняются |

---

## Антидетект меры (Internal Mode)

### Loader-side
- Direct syscalls (Hell's Gate) — нет ntdll hooking surface
- `OpenProcess` с минимальными правами (только нужные флаги)
- Loader удаляет себя из памяти после инжекта
- Рандомизация времени инжекта (не при старте, через N секунд)

### Payload-side
- Нет PE заголовка (затёрт после загрузки)
- Нет в PEB module list
- Нет экспортов (не DLL с точки зрения ОС)
- Работает в контексте существующего треда (thread hijack вариант)
- SharedMem с обфусцированным именем (hash-based name)
- Payload делает ТОЛЬКО read — никаких write в игровую память

### Timing
- Pattern scan только при инициализации (не loop)
- Shared memory update: 30 Hz (не нужно чаще)
- Named event сигнализирует Python — нет busy polling

---

## Когда использовать какой режим

```toml
# External — для фарм-бота (loot, mods, rewards)
mode = "external"

# Internal — для precision (точные HP-порги, точные координаты, fast reaction)
mode = "internal"
```

Hybrid: External для UI (OCR reward screens, loot names) + Internal для GameState (HP, coords).  
Реализуется через `CaptureService` который отдаёт один унифицированный `GameState` независимо от режима.

---

## Последствия

- `cpp/loader/` — новый C++ компонент (mapper + syscall stubs)
- `cpp/internal/` — payload DLL (pattern scan, shm writer)
- `src/capture/internal_bridge.py` — Python shm reader
- `src/capture/capture_service.py` — unified interface поверх обоих режимов
- Добавить `rapidfuzz` в `pyproject.toml`
- ADR-0001 дополнить: External = default, Internal = opt-in

## Связанные ADR

- ADR-0001: External architecture (базис)
- ADR-0002: Python + C++ DLL split
- ADR-0003: Pixel-based vision
