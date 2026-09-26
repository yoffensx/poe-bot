# AGENTS.md — poe2-bot

> AI-агентам: этот файл — локальный SSOT для работы в репозитории `poe2-bot`.  
> Глобальный регламент: `%USERPROFILE%\.aegis\data\plan-keeper\GLOBAL_AGENT_RULES.md`

---

## Контекст проекта

**poe2-bot** — внешний (External) бот для Path of Exile 2.  
Реализует полный Map-цикл: анализ модов → навигация комнат → combat → выбор наград/дебафов → лут → stash.

- **Язык:** Python 3.12 (логика) + C++ (DXGI capture, input humanizer)
- **Obsidian Project:** `poe-bot`
- **Репозиторий:** `D:\git\poe-bot`

---

## Стек и инструменты

| Инструмент | Назначение |
|-----------|-----------|
| `uv` | Управление окружением и зависимостями |
| `ruff` | Линт + форматирование |
| `mypy` | Проверка типов |
| `pytest` | Тестирование |
| CMake + MSVC | Сборка C++ DLL |

### Команды разработчика

```bash
uv run ruff check src/        # линт
uv run ruff format src/       # форматирование
uv run mypy src/               # типы
uv run pytest tests/           # тесты
uv run pytest tests/unit/      # только unit
```

---

## Архитектурные ограничения

1. **External only** — никаких `OpenProcess`, `ReadProcessMemory`, `VirtualAllocEx` к процессу `PathOfExile2.exe`
2. **Pixel-based** — вся информация о состоянии игры — из screen capture, OCR и CV
3. **Build-driven** — логика модулей определяется `build_config.toml`, не хардкодится
4. **Humanized input** — все SendInput-вызовы идут через `src/input/mouse.py` и `src/input/keyboard.py` с обязательной гуманизацией

---

## Структура модулей

```
src/
├── capture/      # Обёртка C++ DXGI DLL → numpy frame
├── vision/       # OpenCV: OCR, minimap, loot, UI
├── analyzers/    # Mod/Reward/Debuff decision logic
├── logic/        # FSM, pathfinding, combat, loot_filter
├── input/        # Humanized mouse + keyboard
├── stash/        # Stash layout management
└── config/       # TOML loader + Pydantic schema
```

### Зависимости между модулями

```
config → все остальные (читают конфиг)
capture → vision (raw frames)
vision → analyzers (parsed game state)
vision → logic (minimap, HP, loot positions)
analyzers → logic (decisions)
logic → input (actions)
logic → stash (inventory operations)
```

---

## Config-файлы (SSOT)

| Файл | Назначение |
|------|-----------|
| `configs/build_configs/*.toml` | Конфиг персонажа/билда |
| `configs/mod_database.toml` | База модов карты (категории, веса, условия) |
| `configs/reward_priorities.toml` | Приоритеты наград под билд |
| `configs/debuff_weights.toml` | "Болезненность" дебафов |
| `configs/loot_filter.toml` | Правила подбора лута |

**Запрещено хардкодить имена модов, наград, итемов в Python-коде.** Всё в TOML.

---

## Правила написания кода

1. **Типизация обязательна** — все публичные функции аннотированы
2. **Pydantic для конфигов** — все TOML-файлы парсятся через Pydantic v2 модели в `src/config/schema.py`
3. **Нет side-effects в ядре** — `src/analyzers/` и `src/logic/` — чистые функции, никакого I/O
4. **Адаптеры для I/O** — screen capture, SendInput, файловый ввод изолированы в `capture/` и `input/`
5. **Тесты для analyzers** — каждый analyzer покрыт unit-тестами с mock-данными

---

## ADR

| ID | Решение |
|----|---------|
| [ADR-0001](docs/adr/ADR-0001-external-architecture.md) | Выбор External-архитектуры (нет инъекций) |
| [ADR-0002](docs/adr/ADR-0002-python-cpp-split.md) | Python + C++ DLL (capture + input) |
| [ADR-0003](docs/adr/ADR-0003-pixel-based-vision.md) | Pixel-based вместо memory reading |

---

## Workflow для агента

1. `obsidian-todo__get_project_overview(project="poe-bot")` — текущие задачи
2. Взять задачу → `update_task(column="In Progress")`
3. Работать строго по ADR-ограничениям
4. `uv run ruff check src/ && uv run mypy src/ && uv run pytest tests/unit/`
5. `obsidian-todo__finalize_task(id, summary)` → закрыть задачу
