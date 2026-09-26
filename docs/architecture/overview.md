# Architecture — Full System Overview (Internal Headless Farm SSOT)

## 1. Режимы работы и целевая архитектура

Основная архитектурная цель poe2-bot — **масштабирование крупных ферм (сотни параллельных ботов на одном сервере)** без необходимости графического рендера (Zero-Render Headless Architecture).

```toml
# configs/build_configs/*.toml
[bot]
mode = "internal"           # ОСНОВНОЙ РЕЖИМ (SSOT для ферм)
poe2_pid = 0                # 0 = автоопределение PID
instance_id = 0             # 0 = использовать PID
loader_path = "cpp/build/Release/poe2_loader.exe"
payload_path = "cpp/build/Release/poe2_payload.dll"
```

| Режим | Источник данных | Потребление CPU/GPU | Масштабирование на сервер | Статус |
|---|---|---|---|---|
| **internal** | Manual Map Payload + Shared Memory (`Local\poe2_gs_<pid>`) | <0.5% CPU на поток, **0% GPU (zero-render)** | **100+ ботов на 1 сервер** | **SSOT (Primary)** |
| **external** | DXGI capture + Tesseract OCR + OpenCV | Высокое (требует 3D рендер и GPU compositor) | 2-4 окна максимум | **Deprecated / Fallback** |

---

## 2. Архитектура Multi-Instance фермы

```
+─────────────────────────────────────────────────────────────────────────────────────────+
|                                    СЕРВЕРНАЯ ФЕРМА                                      |
|                                                                                         |
|  [PoE2 Client 1 (PID 1024)]          [PoE2 Client 2 (PID 2048)]       [PoE2 Client N]   |
|   (Zero-Render / No-GFX)              (Zero-Render / No-GFX)           (Zero-Render)    |
|             │                                   │                            │          |
|    poe2_payload.dll                    poe2_payload.dll              poe2_payload.dll   |
|   (Manual Map / No-CRT)               (Manual Map / No-CRT)         (Manual Map/No-CRT) |
|             │ 30 Hz updates                     │ 30 Hz updates              │ 30 Hz    |
|             ▼                                   ▼                            ▼          |
|  [SHM: Local\poe2_gs_1024]            [SHM: Local\poe2_gs_2048]     [SHM: ..._pid]      |
|             ▲                                   ▲                            ▲          |
|             │ zero-copy (<1ms)                  │ zero-copy (<1ms)           │ zero-copy|
|  ┌──────────┴───────────────────────────────────┴────────────────────────────┴───────┐  |
|  │                               PYTHON PROCESS                                      │  |
|  │                                                                                   │  |
|  │   BridgeRegistry ──► [InternalBridge 1]  [InternalBridge 2] ... [InternalBridge N]│  |
|  │                              │                   │                     │          │  |
|  │                              ▼                   ▼                     ▼          │  |
|  │   Multi-FSM Loop ──► [Bot FSM 1]         [Bot FSM 2]            [Bot FSM N]       │  |
|  │                              │                   │                     │          │  |
|  │                              ▼                   ▼                     ▼          │  |
|  │   Humanized Input ─► [Input Queue 1]     [Input Queue 2]        [Input Queue N]   │  |
|  └───────────────────────────────────────────────────────────────────────────────────┘  |
+─────────────────────────────────────────────────────────────────────────────────────────+
```

---

## 3. Стек технологий

| Слой | Технология | Назначение |
|---|---|---|
| **Loader** | C++20 / MASM (`cpp/loader/`) | Manual mapper с direct syscalls (Hell's Gate + Halo's Gate), PE-header wipe, hide from debugger |
| **Payload DLL** | C++20 (`cpp/internal/`) | No-CRT stealth DLL, manual PEB/EAT walking (djb2 hash), SEH-protected memory reading, AOB scan |
| **IPC** | Win32 Named Shared Memory | Партиционированная память `Local\poe2_gs_<pid>` (64 KB, POD-структуры, magic 0x504F4532) |
| **Host Bridge** | Python 3.12 (`mmap` + `ctypes` + `struct`) | `src/capture/internal_bridge.py`: zero-copy чтение сотен инстансов без внешних библиотек |
| **FSM & Logic** | Python (`src/logic/`) | Модульная машина состояний (Idle, MapSelect, Running, Combat, Looting, Stashing) |
| **Decision Core** | Python (`src/analyzers/`) | Чистые функции оценки модов карты, выбора наград и минимизации дебафов |
| **Input Emulation** | C++ DLL (`cpp/input/`) + Python wrappers | Гуманизированный ввод: кривые Безье, гауссов джиттер, микропаузы |
| **Config SSOT** | TOML + Pydantic v2 (`src/config/`) | Строго типизированные схемы конфигураций билдов, модов, фильтров |

---

## 4. Спецификация Shared Memory (`cpp/common/game_state.h`)

Пакет `GameStatePacket` сериализуется C++ payload и парсится Python `InternalBridge`:
- **Player State:** `hp`, `hpMax`, `mana`, `manaMax`, `es`, `esMax`, координаты `playerPos` (x, y, z), угол взгляда `playerAngle`.
- **Flasks:** Заряды 6 слотов фласок (`flaskCharges[6]`).
- **Monsters:** Массив до 128 монстров (`pos`, `hp`, `hpMax`, `id`, `type`, `isAlive`).
- **Map Mods:** До 32 предрассчитанных хэшей модов зоны (`mapModHashes`).
- **Area & Status:** `areaHash`, флаг `isInMap`, маска занятости слотов инвентаря `inventoryMask`, таймстемп `timestampMs`.

---

## 5. Performance Budget (Internal vs External)

| Операция | Старый бюджет (External/OCR) | Новый бюджет (Internal Headless SSOT) |
|---|---|---|
| Получение состояния | ~2-5 ms (DXGI capture) | **< 0.1 ms (mmap zero-copy read)** |
| Парсинг HP / MP / ES | ~3 ms (HSV маски) | **0 ms (прямое чтение полей)** |
| Парсинг модов карты | ~20-30 ms (Tesseract OCR) | **< 0.05 ms (хэш-лукап по базе)** |
| Позиция игрока и врагов | ~5 ms (анализ миникарты) | **0 ms (точные float координаты)** |
| Решение FSM + A* путь | ~5 ms | **< 2 ms (чистый A* по сетке)** |
| **Полный цикл такта** | **~40 ms (25 FPS, лимит GPU)** | **< 2.5 ms (400+ FPS теоретический предел)** |

---

## 6. Дерево директорий

```
poe2-bot/
├── cpp/
│   ├── common/               # Shared C++ headers (game_state.h, hash.h, peb_walk.h)
│   ├── loader/               # poe2_loader.exe (mapper.cpp, syscalls.h, syscall_stub.asm)
│   ├── internal/             # poe2_payload.dll (payload.cpp — AOB scan, SEH, shm write)
│   ├── input/                # poe2_input.dll (humanized mouse/keyboard)
│   └── CMakeLists.txt        # Сборка всех C++ артефактов
├── src/
│   ├── capture/              # InternalBridge, BridgeRegistry, CaptureService
│   ├── analyzers/            # Чистая логика (mod_analyzer, reward_picker, debuff_picker)
│   ├── logic/                # FSM, навигация комнат, combat loop, лут-фильтр
│   ├── input/                # Python обёртки гуманизированного ввода
│   ├── stash/                # Менеджер сундука
│   ├── config/               # Pydantic v2 схемы и загрузчик TOML
│   └── vision/               # [DEPRECATED] Резервный OCR/CV слой
├── configs/                  # TOML-конфигурации билдов, модов и лута
└── docs/                     # Архитектурная документация и ADR
```
