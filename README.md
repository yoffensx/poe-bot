# poe2-bot

> **Path of Exile 2 — Smart Map Runner Bot**  
> Внешний бот (External Architecture) для автоматизации Map-циклов с умным анализом модов, выбором наград/дебафов и смарт-лутом под конкретный билд.

---

## Содержание

- [Обзор](#обзор)
- [Архитектура](#архитектура)
- [Стек технологий](#стек-технологий)
- [Структура проекта](#структура-проекта)
- [Быстрый старт](#быстрый-старт)
- [Конфигурация](#конфигурация)
- [Модули](#модули)
- [Разработка](#разработка)
- [Тестирование](#тестирование)
- [Безопасность](#безопасность)

---

## Обзор

Бот реализует полный цикл Map-фарма в PoE2:

1. **Анализ модов карты** — классификация аффиксов как опасные/плохие/нейтральные/хорошие под билд
2. **Skip/Run решение** — автоматический пропуск опасных карт
3. **Автонавигация** — A* по минимапу, обход комнат
4. **Combat loop** — ротация скиллов, автопоушн
5. **Выбор наград** — приоритизация под билд
6. **Выбор дебафов** — минимизация потерь
7. **Смарт-лут** — pickup только ценного под билд
8. **Stash management** — автораскладка по вкладкам

### Ключевые принципы

- **Dual-mode** — `external` (OCR/CV, по умолчанию) или `internal` (manual-mapped DLL + shared memory)
- **Pixel-based fallback** — внешний режим использует только screen capture, никакого чтения памяти
- **Build-aware** — всё поведение диктует `build_config.toml`
- **Humanized input** — кривые Безье, gaussian jitter, случайные паузы

---

## Архитектура

### Режимы работы

| Режим | Источник данных | Риск детекта | Применение |
|-------|----------------|-------------|-----------|
| **external** | DXGI capture + OCR/CV | низкий | фарм-бот, **по умолчанию** |
| **internal** | manual-mapped DLL + shared memory | средний | точные HP/coords, fast reaction |
| **hybrid** | internal GameState + external для UI | средний | оптимальный баланс |

Режим задаётся в `build_config.toml`: `[bot] mode = "external" | "internal" | "hybrid"`

### Диаграмма слоёв

```
                     [configs/*.toml]
                           │
                     [src/config/]         ← Pydantic v2 schema
                           │
          ┌────────────────┼────────────────┐
          │                │                │
  ══ EXTERNAL ══    ══ INTERNAL ══          ▼
          │                │        [src/analyzers/]
 [cpp/capture/]   [cpp/loader/]      pure functions
 DXGI → frame     manual mapper      mod/reward/debuff
          │                │
          ▼                ▼
 [src/vision/]   [cpp/internal/]
 OCR / CV         payload DLL
 HP, minimap,     AOB scan → read
 loot, UI         HP, coords, mobs
          │                │
          └────────┬────────┘
                   ▼
     [src/capture/capture_service.py]
     unified GameState (any mode)
                   │
                   ▼
         [src/logic/]          ← FSM, A*, combat, loot
                   │
         [src/input/]          ← humanized SendInput
                   │
         [cpp/input/]          ← Bezier + jitter DLL
```

**Подробнее:** [`docs/architecture/overview.md`](docs/architecture/overview.md)

---

## Стек технологий

| Компонент | Технология | Причина выбора |
|-----------|-----------|----------------|
| **Screen Capture** | C++ / DXGI Desktop Duplication API | Захватывает compositor напрямую, минимальный след, ~1ms latency |
| **Vision Core** | Python 3.12 + OpenCV 4.x | Богатые CV-примитивы, шаблонный матчинг, HSV-маски |
| **OCR** | Tesseract 5 + pytesseract | Открытый, настраивается под шрифт PoE2 |
| **Input Emulation** | C++ DLL / SendInput + humanizer | Гибкость + возможность апгрейда на KMBox/driver |
| **FSM / Logic** | Python + transitions | Читаемые состояния, легко отлаживать |
| **Config** | TOML (tomllib stdlib) | Читаемый, строго типизированный |
| **Async loop** | asyncio | Неблокирующий main loop |
| **Build** | uv + pyproject.toml | Быстрый пакетный менеджер |
| **Tests** | pytest + pytest-asyncio | Стандарт |

---

## Структура проекта

```
poe2-bot/
├── src/
│   ├── capture/
│   │   ├── capture.py            # Python-обёртка над C++ DXGI DLL
│   │   ├── capture_service.py    # Unified GameState (external|internal|hybrid)
│   │   └── internal_bridge.py    # Shared memory reader (internal mode)
│   ├── vision/           # OpenCV: HP, minimap, loot, UI
│   │   ├── ocr.py
│   │   ├── minimap.py
│   │   ├── loot_scanner.py
│   │   └── ui_parser.py
│   ├── analyzers/        # Логика принятия решений
│   │   ├── mod_analyzer.py
│   │   ├── reward_picker.py
│   │   └── debuff_picker.py
│   ├── logic/            # FSM, навигация, combat
│   │   ├── fsm.py
│   │   ├── pathfinding.py
│   │   ├── combat.py
│   │   └── loot_filter.py
│   ├── input/            # Humanized input
│   │   ├── mouse.py
│   │   └── keyboard.py
│   ├── stash/            # Stash management
│   │   └── manager.py
│   └── config/           # Config loader + validator
│       ├── loader.py
│       └── schema.py
├── cpp/
│   ├── capture/          # DXGI Desktop Duplication DLL
│   ├── input/            # Humanized input DLL (Bezier + jitter)
│   ├── loader/           # Manual map loader (direct syscalls, no PEB trace)
│   └── internal/         # Payload DLL (AOB scan, shared memory writer)
├── configs/
│   ├── build_configs/
│   │   └── lightning_arrow.toml
│   ├── mod_database.toml
│   ├── reward_priorities.toml
│   ├── debuff_weights.toml
│   └── loot_filter.toml
├── docs/
│   ├── adr/              # Architecture Decision Records
│   └── architecture/     # Диаграммы и описание системы
├── tests/
│   ├── unit/
│   └── integration/
├── tools/
│   ├── calibrator/       # UI-координаты после патчей PoE2
│   ├── ocr_trainer/      # Дообучение OCR под шрифт
│   └── mod_recorder/     # Запись новых модов в базу
├── AGENTS.md
├── pyproject.toml
└── .gitignore
```

---

## Быстрый старт

### Требования

- Python 3.12+
- [uv](https://github.com/astral-sh/uv)
- Tesseract 5 (в PATH)
- Visual Studio Build Tools 2022 (для C++ DLL)
- Windows 10/11 x64

### Установка

```bash
# Клонировать
git clone https://github.com/<owner>/poe2-bot
cd poe2-bot

# Создать окружение и установить зависимости
uv sync

# Собрать C++ DLL
cmake -B build/cpp cpp/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpp --config Release

# Откалибровать UI (запустить PoE2 на нужном разрешении)
uv run python tools/calibrator/calibrate.py
```

### Запуск

```bash
# Запустить с билд-конфигом
uv run python -m poe2bot --build configs/build_configs/lightning_arrow.toml

# Dry-run (только анализ, без ввода)
uv run python -m poe2bot --build configs/build_configs/lightning_arrow.toml --dry-run

# Только анализ модов (не запускать цикл)
uv run python -m poe2bot --analyze-only
```

---

## Конфигурация

### build_config.toml — главный файл

Все поведения бота диктует конфиг билда:

```toml
[character]
name = "Lightning Arrow Deadeye"
class = "Ranger"

[damage]
primary_type = "lightning"
secondary_type = "physical"
has_fire = false
has_cold = false

[defense]
leech_dependent = false
regen_dependent = true
flask_dependent = true

[stat_priorities]
crit_chance = 90
attack_speed = 80
lightning_damage = 95

[skill_rotation]
main_skill = "Lightning Arrow"
movement = "Dash"
curse = "Conductivity"
cooldowns = { Dash = 500, Conductivity = 4000 }
```

### mod_database.toml

База модов с категориями и весами. Подробнее: [`docs/architecture/mod_system.md`](docs/architecture/mod_system.md)

---

## Модули

| Модуль | Описание | Документация |
|--------|---------|-------------|
| Mod Analyzer | Оценка модов карты, решение skip/run | [`docs/architecture/mod_system.md`](docs/architecture/mod_system.md) |
| Reward Picker | Выбор наград под билд | В разработке |
| Room Navigator | A* навигация по минимапу | В разработке |
| Loot Filter | Смарт-подбор лута | В разработке |
| Humanizer | Bezier curves, jitter, паузы | В разработке |

---

## Разработка

```bash
# Линт
uv run ruff check src/

# Форматирование
uv run ruff format src/

# Типы
uv run mypy src/

# Тесты
uv run pytest tests/
```

---

## Тестирование

```bash
# Все тесты
uv run pytest

# Только unit
uv run pytest tests/unit/

# С покрытием
uv run pytest --cov=src --cov-report=html
```

---

## Безопасность

**External mode (default):**
- Бот **не инжектирует** код в процесс PoE2
- Бот **не читает память** процесса PoE2
- Все взаимодействия — screen capture + SendInput

**Internal mode (opt-in):**
- Manual-mapped DLL без следов в PEB module list
- Только **read** из памяти процесса, никаких write в игровую логику
- Данные передаются через named shared memory → Python, нет прямой связи
- Direct syscalls (Hell's Gate), затёртый PE-заголовок, нет экспортов

**Общее:**
- Humanizer минимизирует детектируемость паттернов ввода
- Используй на своё усмотрение

**Использование бота нарушает ToS GGG. Ответственность за последствия несёт пользователь.**

---

## ADR

| ID | Название |
|----|---------|
| [ADR-0001](docs/adr/ADR-0001-external-architecture.md) | Выбор External-архитектуры |
| [ADR-0002](docs/adr/ADR-0002-python-cpp-split.md) | Python + C++ DLL разделение |
| [ADR-0003](docs/adr/ADR-0003-pixel-based-vision.md) | Pixel-based вместо memory reading |
| [ADR-0004](docs/adr/ADR-0004-internal-mode-manual-map.md) | Internal mode — manual map + shared memory IPC |
