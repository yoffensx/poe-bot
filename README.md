# poe2-bot

> **Path of Exile 2 — High-Performance Internal Headless Bot Farm**  
> Промышленная архитектура для масштабирования бот-ферм на выделенных серверах (**десятки и сотни параллельных ботов на одной машине**) в режиме полного подавления рендера (**Zero-Render Headless**).

---

## Ключевая концепция

Бот спроектирован для максимальной производительности и плотности инстансов:
- **Internal Mode First (SSOT):** Stealth Manual-Mapped DLL внедряется в процесс `PathOfExile2.exe` через прямые системные вызовы (Hell's Gate / Halo's Gate direct syscalls).
- **Zero-Render (Без рендера):** Клиенты игры запускаются без прорисовки полигонов и текстур. Бот не использует захват экрана и не грузит GPU/VRAM.
- **Ultra-low latency (<1 ms):** Состояние игры (HP, Mana, ES, координаты игрока и монстров, моды, инвентарь) передается в Python через партиционированную named shared memory (`Local\poe2_gs_<pid>`).
- **Сотни ботов на сервере:** За счет отсутствия GPU-рендера и нулевой нагрузки на OCR один мощный сервер способен обслуживать 50-100+ параллельных окон.
- **External (OCR/CV):** Переведён в статус **Deprecated / Legacy Fallback**. В боевой эксплуатации отключен для исключения оверхеда.

---

## Архитектура системы

```
+─────────────────────────────────────────────────────────────────────────────+
|                          ВЫДЕЛЕННЫЙ СЕРВЕР (ФЕРМА)                          |
|                                                                             |
|  [PoE2 Client 1 (PID 1024)]      [PoE2 Client 2 (PID 2048)]      ...N окон  |
|    (Zero-Render)                   (Zero-Render)                            |
|          │                               │                                  |
|   poe2_payload.dll                poe2_payload.dll                          |
|   (No-CRT / Stealth)              (No-CRT / Stealth)                        |
|          │ 30 Hz updates                 │ 30 Hz updates                    |
|          ▼                               ▼                                  |
|  [Local\poe2_gs_1024]            [Local\poe2_gs_2048]            ..._pid    |
|          ▲                               ▲                                  |
|          │ zero-copy (<1ms)              │ zero-copy (<1ms)                 |
|  ┌───────┴───────────────────────────────┴───────────────────────────────┐  |
|  │                        PYTHON APPLICATION                             │  |
|  │                                                                       │  |
|  │  BridgeRegistry ──► [InternalBridge 1]     [InternalBridge 2]  ...[N] │  |
|  │                             │                      │                  │  |
|  │                             ▼                      ▼                  │  |
|  │  Orchestrator   ──► [Bot FSM 1]            [Bot FSM 2]         ...[N] │  |
|  │                             │                      │                  │  |
|  │                             ▼                      ▼                  │  |
|  │  Humanized Input ─► [Input Service 1]      [Input Service 2]   ...[N] │  |
|  └───────────────────────────────────────────────────────────────────────┘  |
+─────────────────────────────────────────────────────────────────────────────+
```

---

## Стек компонентов

### C++20 / MASM Core (`cpp/`)
1. **Manual Map Loader (`cpp/loader/mapper.cpp`):**
   - Прямые системные вызовы (Hell's Gate + Halo's Gate) для обхода перехватов EDR/anticheat.
   - Затирка заголовков PE (MZ magic) в целевой памяти.
   - Запуск потока через `NtCreateThreadEx` с `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER`.
   - Изоляция по PID/Instance ID.
2. **Stealth Payload (`cpp/internal/payload.cpp`):**
   - Zero-CRT (`/NODEFAULTLIB`), ручной EAT/PEB walk по djb2-хэшам.
   - Защищенное чтение памяти (SEH `__try / __except`).
   - AOB pattern scanning базовых структур GameState.
   - Обновление состояния в shared memory с частотой 30 Hz.
3. **Shared Memory Protocol (`cpp/common/game_state.h`):**
   - Двоичный POD-пакет `GameStatePacket` (64 КБ, заголовок с `magic = 0x504F4532`).
   - Имя сегмента: `Local\poe2_gs_<instance_id>`.
4. **Humanized Input DLL (`cpp/input/`):**
   - Кривые Безье, гауссов джиттер, микропаузы между нажатиями.

### Python Orchestrator (`src/`)
1. **`src/capture/internal_bridge.py`:** Zero-copy чтение shm через модуль `mmap`, `BridgeRegistry` для N инстансов.
2. **`src/capture/capture_service.py`:** Единый фасад `CaptureService` + фабрика `make_services_for_pids()`.
3. **`src/analyzers/`:** Чистые функции оценки модов карты, выбора наград и минимизации дебафов (100% in-memory тесты).
4. **`src/logic/`:** Машина состояний (FSM), навигация по точным координатам, combat loop, лут-фильтр.
5. **`src/config/`:** Строгие Pydantic v2 схемы TOML-конфигураций.

---

## Сравнение режимов: Internal vs Legacy OCR

| Критерий | Internal Mode (SSOT) | Legacy External (OCR / DXGI) |
|---|---|---|
| **Плотность на сервер** | **100+ параллельных ботов** | 2-4 окна максимум (упирается в GPU) |
| **Рендер графики** | **Не требуется (Zero-Render)** | Обязателен (3D, шрифты, окна) |
| **Нагрузка CPU на бота** | **< 0.5% одного потока** | 15-30% от ядра CPU |
| **Задержка чтения данных** | **< 1 ms (direct memory snapshot)** | 20-40 ms (захват кадра + OCR) |
| **Координаты и монстры** | **Точные float (x, y, z) + точное HP** | Приблизительные пиксельные пятна |
| **Статус** | **ОСНОВНОЙ (SSOT)** | **Deprecated / Fallback** |

---

## Быстрый старт

### Требования
- Windows 10/11 x64 или Windows Server 2022/2025 x64
- Visual Studio Build Tools 2022 (MSVC x64 + MASM)
- Python 3.12+ и [uv](https://github.com/astral-sh/uv)
- CMake 3.20+

### Сборка C++ компонентов
```bash
# Сборка poe2_loader.exe и poe2_payload.dll
cmake -B cpp/build cpp/ -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --config Release
```

Артефакты появятся в `cpp/build/Release/`:
- `poe2_loader.exe` — консольный инжектор
- `poe2_payload.dll` — stealth no-CRT библиотека

### Запуск одного инстанса
```bash
# Запуск бота с автоматическим обнаружением процесса игры
uv run python -m poe2bot --build configs/build_configs/lightning_arrow.toml

# Запуск с явным указанием PID клиента
uv run python -m poe2bot --build configs/build_configs/lightning_arrow.toml --pid 12345
```

### Запуск серверной фермы на N окон
```python
from src.capture.capture_service import make_services_for_pids

# Список PID запущенных клиентов PoE2
pids = [1024, 2048, 4096, 8192]

# Подключение ко всем процессам (автоинжект + открытие shm)
services = make_services_for_pids(pids)

for svc in services:
    state = svc.read_state()
    print(f"[{state.instance_id}] HP: {state.hp}/{state.hp_max} | Monsters: {len(state.monsters)}")
```

---

## Конфигурация (`configs/build_configs/lightning_arrow.toml`)

```toml
[bot]
mode = "internal"           # "internal" (SSOT) | "external" (legacy fallback)
poe2_pid = 0                # 0 = автоопределение через psutil
instance_id = 0             # 0 = использовать poe2_pid
inject_delay_s = 5.0        # задержка перед инжектом
loader_path = "cpp/build/Release/poe2_loader.exe"
payload_path = "cpp/build/Release/poe2_payload.dll"

[character]
name = "Lightning Arrow Deadeye"
class = "Ranger"
ascendancy = "Deadeye"
level = 95

[damage]
primary_type = "lightning"
secondary_type = "physical"

[potions]
life_flask_threshold = 0.60
mana_flask_threshold = 0.30
```

---

## Разработка и тестирование

```bash
# Проверка качества кода
uv run ruff check src/
uv run ruff format src/
uv run mypy src/

# Запуск unit-тестов
uv run pytest tests/unit/
```

---

## Архитектурные решения (ADR)

- [ADR-0004: Internal Mode — Manual Map Injection & Headless GameState](docs/adr/ADR-0004-internal-mode-manual-map.md) — **Действующий SSOT**
- [ADR-0001: Выбор архитектуры и эволюция к фермам](docs/adr/ADR-0001-external-architecture.md)
- [ADR-0002: Python + C++ разделение](docs/adr/ADR-0002-python-cpp-split.md)
- [ADR-0003: Pixel-based vision (Deprecated)](docs/adr/ADR-0003-pixel-based-vision.md)
- [Системный обзор архитектуры](docs/architecture/overview.md)
- [Дорожная карта ферм](docs/roadmap.md)
