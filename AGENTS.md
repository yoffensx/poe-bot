# AGENTS.md — poe2-bot

> AI-агентам: этот файл — локальный SSOT для работы в репозитории `poe2-bot`.  
> Глобальный регламент: `%USERPROFILE%\.aegis\data\plan-keeper\GLOBAL_AGENT_RULES.md`

---

## Контекст проекта

**poe2-bot** — высокопроизводительный **Internal Headless Bot** для Path of Exile 2.  
Архитектурный приоритет №1 — **масштабируемые серверные фермы (десятки и сотни ботов на одном сервере)** без необходимости рендера графики (zero-render / headless GameState).

- **Архитектура по умолчанию:** **Internal Mode (Manual Map Injection)** через direct syscalls (Hell's Gate/Halo's Gate), stealth payload DLL без следов в PEB, чтение GameState напрямую из памяти и передача через zero-copy named shared memory (`Local\poe2_gs_<pid>`).
- **External (OCR / DXGI):** **Минимальный / deprecated fallback.** Для ферм выключен, так как OCR и рендер жрут GPU/CPU и делают невозможным запуск сотен окон на сервере.
- **Язык:** Python 3.12 (логика FSM, анализаторы, конфиги) + C++20 / MASM (Manual Map Loader, no-CRT Payload DLL, direct syscalls, humanized input).
- **Obsidian Project:** `poe-bot`
- **Репозиторий:** корень этого git-репозитория (ветка `master`)

---

## Стек и инструменты

| Инструмент | Назначение |
|-----------|-----------|
| `uv` | Управление Python-окружением и зависимостями |
| `ruff` | Линт + форматирование Python |
| `mypy` | Проверка типов Python |
| `pytest` | Тестирование бизнес-логики и моков GameState |
| CMake + MSVC 2022 + MASM | Сборка C++ `poe2_loader.exe` и `poe2_payload.dll` |

### Команды разработчика

```bash
# Python проверки
uv run ruff check src/        # линт
uv run ruff format src/       # форматирование
uv run mypy src/               # типы
uv run pytest tests/           # тесты
uv run pytest tests/unit/      # только unit

# Сборка C++ компонентов Internal Mode
cmake -B cpp/build cpp/ -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --config Release
```

---

## Архитектурные правила и инварианты

1. **Internal First & Headless SSOT:**
   - Основной рабочий режим бота — `mode = "internal"`.
   - Вся информация о состоянии игры (HP, Mana, ES, координаты игрока, координаты и HP монстров, хэши модов, инвентарь) извлекается payload DLL напрямую из GameState памяти клиента и пишется в shared memory с частотой 30 Hz.
   - **Zero-render:** клиенты PoE2 на ферме работают с минимальным или отключенным рендером; боту не требуется GPU-захват экрана или OCR.
2. **Stealth Manual Mapping:**
   - Никаких стандартных `LoadLibraryA` или инъекций с видимым PEB.
   - Loader (`cpp/loader/mapper.cpp`) использует **direct syscalls** (`NtAllocateVirtualMemory`, `NtWriteVirtualMemory`, `NtProtectVirtualMemory`, `NtCreateThreadEx` со скрытием потока `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER`).
   - DOS/NT-заголовки payload затираются нулями сразу после маппинга.
   - Payload компилируется без CRT (`/NODEFAULTLIB`, `/GS-`), все WinAPI вызовы резолвятся на лету через хеширование строк (djb2) и обход PEB/EAT.
3. **Multi-Instance Isolation (Фермы на сотни окон):**
   - Каждое окно PoE2 имеет свой уникальный PID.
   - Имя сегмента shared memory строго партиционировано: `Local\poe2_gs_<pid>` (или кастомный `instance_id`).
   - `BridgeRegistry` в Python поддерживает независимое чтение сотен инстансов одновременно без блокировок.
4. **Build-driven:** логика решений FSM и ротации скиллов управляется TOML-конфигами (`configs/build_configs/*.toml`), никаких хардкодов.
5. **Humanized input:** все действия мыши и клавиатуры идут через humanizer (кривые Безье, гауссов джиттер, микропаузы).
6. **Чистые функции ядра:** `src/analyzers/` и `src/logic/` не содержат I/O; работают с датаклассом `GameState`.

---

## Структура модулей

```
src/
├── capture/          # Shared memory bridge (InternalBridge) + Unified CaptureService
│   ├── internal_bridge.py   # Zero-copy чтение Local\poe2_gs_<pid>
│   └── capture_service.py   # Фасад для одного или N инстансов
├── analyzers/        # Чистая логика оценки модов, наград, дебафов
│   ├── mod_analyzer.py
│   ├── reward_picker.py
│   └── debuff_picker.py
├── logic/            # FSM, поиск пути A*, combat loop, лут-фильтр
├── input/            # Humanized ввод (мышь, клавиатура)
├── stash/            # Stash layout и раскладка
├── config/           # TOML loader + Pydantic v2 schema
└── vision/           # [DEPRECATED / FALLBACK ONLY] Legacy OCR и CV
cpp/
├── common/           # Shared C++ headers: hash.h, peb_walk.h, game_state.h
├── loader/           # poe2_loader.exe (mapper.cpp, syscalls.h, syscall_stub.asm)
├── internal/         # poe2_payload.dll (payload.cpp, AOB scan, shm writer)
└── CMakeLists.txt    # Сборка loader и payload
```

---

## Config-файлы (SSOT)

| Файл | Назначение |
|------|-----------|
| `configs/build_configs/*.toml` | Конфиг билда + секция `[bot]` (`mode = "internal"`) |
| `configs/mod_database.toml` | База модов карты (категории, веса, условия) |
| `configs/reward_priorities.toml` | Приоритеты наград под билд |
| `configs/debuff_weights.toml` | "Болезненность" дебафов |
| `configs/loot_filter.toml` | Правила подбора лута |

---

## ADR

| ID | Решение |
|----|---------|
| [ADR-0001](docs/adr/ADR-0001-external-architecture.md) | Эволюция архитектуры: переход от External к Internal Headless Farm |
| [ADR-0002](docs/adr/ADR-0002-python-cpp-split.md) | Python + C++ DLL (loader + stealth payload + input) |
| [ADR-0003](docs/adr/ADR-0003-pixel-based-vision.md) | Pixel-based vision (переведено в Deprecated / Legacy fallback) |
| [ADR-0004](docs/adr/ADR-0004-internal-mode-manual-map.md) | Internal Mode — Manual Map Injection & Headless GameState (SSOT) |

---

## Workflow для агента

1. `obsidian-todo__get_project_overview(project="poe-bot")` — текущие задачи
2. Все новые задачи формулировать с учётом **Internal Mode First** и требований к фермам на сотни окон.
3. Проверять типизацию и тесты: `uv run ruff check src/ && uv run mypy src/ && uv run pytest tests/unit/`.
4. Закрывать задачи через `obsidian-todo__finalize_task(id, summary)`.
