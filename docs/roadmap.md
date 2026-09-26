# Roadmap — poe2-bot

> SSOT по плану работ. Карточки задач лежат в [`docs/tasks/`](tasks/) и зеркалятся в Obsidian (`Projects/poe-bot`).
> Статус карточки определяется её колонкой: `tasks/todo` → `tasks/in-progress` → `tasks/done`.

---

## Текущее состояние (2026-09-26)

Сделан только scaffold: `pyproject.toml`, `AGENTS.md`, `README.md`, ADR-0001/0002/0003, `docs/architecture/mod_system.md`,
Pydantic-схема `src/config/schema.py` и пять TOML-конфигов. Модули `src/capture`, `src/vision`, `src/analyzers`,
`src/logic`, `src/input`, `src/stash`, `cpp/capture`, `cpp/input`, `tools/*` — **пустые**.

Блокеры, которые нужно снять первыми:

1. Пакетинг сломан: `pyproject.toml` → `poe2bot = "poe2bot.__main__:main"`, но пакет называется `src` (`packages = ["src"]`).
2. Нет загрузчика конфигов — `BotConfig` нечем наполнить.
3. Нет ни одного теста и ни одного CI-гейта.

---

## Волны работ

```
Wave 0 ─ Фундамент          001 → 002
   │
   ├─► Wave 1 ─ Ввод/вывод  012 → 003 → 004
   │        │
   │        └─► Wave 2 ─ Vision      005 → 006
   │                  │
   │                  └─► Wave 3 ─ Решения   007 → 008 → 009
   │                            │
   │                            └─► Wave 4 ─ Поведение  010 → 011
   │
   └─► Wave 5 ─ Инфраструктура  013, 014   (в параллель с любой волной)
```

### Wave 0 — Фундамент (блокер всего)

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 001 | Починить пакетинг: entrypoint `poe2bot` + src-layout | high | `uv run poe2bot --help` работает |
| 002 | Config loader + CLI (`--build` / `--dry-run` / `--analyze-only`) | high | все 5 TOML грузятся в `BotConfig` |

### Wave 1 — Ввод и вывод (порты системы)

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 012 | `tools/calibrator` → UI-координаты | medium | `configs/ui_regions.example.toml` валиден |
| 003 | C++ DXGI capture DLL + ctypes-обёртка | high | кадр в `numpy` без чтения памяти процесса |
| 004 | C++ Humanized input DLL + `input/mouse.py`, `input/keyboard.py` | high | весь SendInput только через эти модули |

Порядок важен: калибратор даёт ROI-координаты, без которых vision-слои не тестируются; capture нужен всем vision-модулям,
input — всем исполнителям действий.

### Wave 2 — Vision (дешёвые, чистые детекторы)

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 005 | Minimap parser + HP/фласки | high | детекторы детерминированы на golden-кадрах |
| 006 | OCR engine + loot scanner + UI parser | medium | OCR деградирует без Tesseract, тесты на ROI-скриншотах |

### Wave 3 — Решения (чистое ядро, без I/O)

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 007 | Mod analyzer: skip/run по build_config | high | реализация `docs/architecture/mod_system.md` |
| 008 | Reward picker + debuff picker | medium | детерминированный выбор при равных оценках |
| 009 | Loot filter evaluator + scanner→pickup pipeline | medium | покрыты приоритеты и `unmatched_action` |

### Wave 4 — Поведение

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 010 | FSM + pathfinding (A* по минимапу) + combat loop | high | интеграционные тесты FSM на mock-портах |
| 011 | Stash manager: автораскладка по вкладкам | medium | планировщик слотов покрыт unit-тестами |

### Wave 5 — Инфраструктура (параллельно)

| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 013 | CI (ruff/mypy/pytest + CMake job) + pre-commit + ADR тест-стратегии | medium | workflow зелёный на Windows и Linux |
| 014 | Коммитнуть `docs/architecture`, синхронизировать README | low | нет битых ссылок, `git status` чистый |

---

## Критический путь до первого рабочего цикла

`001 → 002 → 003 → 004 → 005 → 006 → 007 → 010`

Это минимальная последовательность, дающая end-to-end прогон одного map-цикла:
кадр → распознавание модов карты → skip/run → навигация по минимапу → combat с фласками.
`008`, `009`, `011`, `012` подключаются к этому циклу сбоку и улучшают его.

---

## Гейты каждой волны

```bash
uv run ruff check src/
uv run ruff format --check src/
uv run mypy src/
uv run pytest tests/unit/
```

Общие инварианты (нарушение = блокер PR):

- **External only** — никаких `OpenProcess` / `ReadProcessMemory` / инъекций в процесс игры.
- **Pixel-based** — состояние игры только из screen capture, OCR и CV.
- **Build-driven** — ни одного имени мода, награды или предмета в Python-коде; всё в TOML.
- **Humanized input** — весь ввод через `src/poe2bot/input/*`.
- **Чистое ядро** — `analyzers/` и `logic/` без I/O, 100% тестируемы in-memory.

---

## Риски

| Риск | Митигация |
|------|-----------|
| Патчи PoE2 ломают UI-координаты | ROI в `configs/ui_regions.toml` (в `.gitignore`) + версия и проверка разрешения кадра |
| OCR ошибается на модах → бот фармит lethal-карты | fuzzy-порог 85, `LETHAL → hard SKIP`, тест на каждом моде из базы |
| C++ DLL не собирается в CI | Отдельный Windows-job, ctypes-слой умеет работать без DLL (fallback) |
| Bot detection по паттернам ввода | Bézier + gaussian jitter + случайные паузы (задача 004) |

---

## Связанные документы

- `docs/architecture/mod_system.md` — алгоритм mod-решений (реализуется в задаче 007).
- `docs/adr/` — ADR-0001 (External), ADR-0002 (Python + C++), ADR-0003 (Pixel-based).
- `docs/tasks/README.md` — индекс карточек.
