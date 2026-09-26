# Engineering Rules — poe2-bot

> Обязательны для всех контрибьюторов и AI-агентов.  
> Нарушение — повод для ревью-блокера.

---

## 1. Архитектурные инварианты (не нарушаются никогда)

### 1.1 External-only
```
ЗАПРЕЩЕНО:
  OpenProcess(PROCESS_VM_READ, ...)
  WriteProcessMemory(...)
  CreateRemoteThread(...)
  VirtualAllocEx(...)
  Any DLL injection into PathOfExile2.exe
```

### 1.2 Pixel-based
```
ЗАПРЕЩЕНО: читать структуры памяти PathOfExile2.exe
РАЗРЕШЕНО:  DXGI Desktop Duplication → numpy frame → OpenCV/Tesseract
```

### 1.3 Config-driven
```
ЗАПРЕЩЕНО: хардкодить имена модов, наград, итемов, скиллов в Python
РАЗРЕШЕНО:  всё через configs/*.toml, загружаемое через BotConfig
```

### 1.4 Humanized input
```
ЗАПРЕЩЕНО: прямые координаты без jitter, константные задержки
ОБЯЗАТЕЛЬНО: move_mouse() через poe2_input.dll с Bezier + GaussianJitter
```

---

## 2. Чистота слоёв (Hexagonal Architecture)

```
src/analyzers/  →  чистые функции (GameState, BuildConfig) → Decision
src/logic/      →  чистые функции + FSM (transitions)
src/vision/     →  адаптер: frame → GameState (I/O разрешён)
src/capture/    →  адаптер: DLL → numpy frame (I/O разрешён)
src/input/      →  адаптер: Action → SendInput (I/O разрешён)
```

**Правило:** `src/analyzers/` и `src/logic/` **не импортируют** `capture`, `input`, `os`, `ctypes`, `subprocess`.  
Нарушение: ruff lint `E402` + архитектурный ревью.

---

## 3. Типизация

- Все публичные функции **обязательно аннотированы** (mypy strict)
- `Any` — только в `schema.py` для `LootCondition.value` и с комментарием почему
- `cast()` — с объяснением в комментарии
- Нет `# type: ignore` без описания причины в той же строке

```bash
# проверка перед коммитом
uv run mypy src/
```

---

## 4. Тестирование

### Обязательные тесты для каждого Analyzer

```python
# tests/unit/test_<module>.py
# Структура: Arrange → Act → Assert

def test_<scenario>():
    # Arrange: только mock данные, никакого I/O
    build = make_build(regen_dependent=True)
    mods = ["Players cannot Regenerate Life, Mana or Energy Shield"]
    
    # Act
    decision = analyzer.evaluate(mods, build)
    
    # Assert
    assert decision.action == "SKIP"
    assert "regen" in decision.reason
```

### Coverage target

| Модуль | Минимум |
|--------|---------|
| `src/analyzers/` | 90% |
| `src/logic/` | 80% |
| `src/config/` | 85% |
| `src/vision/` | 60% (сложно мокать CV) |

---

## 5. Работа с конфигами

### Добавление нового мода в базу

1. Открыть `configs/mod_database.toml`
2. Добавить запись с `pattern`, `category`, `action`, `weight`
3. Запустить `tools/mod_recorder/` для верификации OCR-совпадения
4. Добавить тест в `tests/unit/test_mod_analyzer.py`

### Добавление нового билда

1. Скопировать `configs/build_configs/lightning_arrow.toml`
2. Изменить `[character]`, `[damage]`, `[defense]`, `[skill_rotation]`
3. Обновить `[stat_priorities]` и `[build_tags]`
4. Запустить `uv run python -m poe2bot --build <new.toml> --dry-run`

---

## 6. Именование

| Что | Стиль | Пример |
|-----|-------|--------|
| Файлы | `snake_case.py` | `mod_analyzer.py` |
| Классы | `PascalCase` | `ModAnalyzer` |
| Функции | `snake_case` | `evaluate_map` |
| Константы | `UPPER_SNAKE` | `DEFAULT_THRESHOLD` |
| Pydantic модели | `PascalCase` + `Config` суффикс | `BuildConfig`, `ModEntry` |
| TOML ключи | `snake_case` | `primary_type`, `min_score` |

---

## 7. Логирование

Используем `loguru`. Запрещено `print()` в production коде.

```python
from loguru import logger

logger.info("Map evaluated: score={score}, action={action}", score=score, action=decision.action)
logger.warning("Unknown mod skipped: {mod!r}", mod=raw_mod)
logger.debug("Fuzzy match: {pattern!r} → {ratio:.1f}%", pattern=pattern, ratio=ratio)
```

Уровни:
- `DEBUG` — детали алгоритмов, fuzzy scores, frame timings
- `INFO` — решения FSM, старт/стоп, picked rewards
- `WARNING` — неизвестные моды, OCR-ошибки, low-confidence
- `ERROR` — сбои DLL, таймауты, невосстанавливаемые ошибки

---

## 8. Git-workflow

```
feat/<module>-<description>   # новая функциональность
fix/<module>-<description>    # баг-фикс
refactor/<description>        # рефакторинг без изменения поведения
docs/<description>            # только документация
chore/<description>           # тулинг, конфиги, deps
```

### Pre-commit checklist

```bash
uv run ruff check src/          # линт
uv run ruff format src/         # форматирование
uv run mypy src/                # типы
uv run pytest tests/unit/       # unit-тесты
```

Все четыре — зелёные перед `git commit`.

---

## 9. Производительность

- Frame pipeline: **< 40ms total** (25 FPS bot loop)
- OCR только на **ROI** — никогда на весь кадр
- A* только при смене цели — не каждый кадр
- Mod matching при **открытии карты** — не в game loop
- Profiling: `uv run python -m cProfile -s cumulative src/__main__.py`

---

## 10. Обновление после патча PoE2

Патч может сломать:
1. **UI layout** → запустить `tools/calibrator/` → обновить `configs/ui_regions.toml`
2. **Шрифт/цвета** → переобучить OCR через `tools/ocr_trainer/`
3. **Имена модов** → добавить новые паттерны в `configs/mod_database.toml`
4. **Новые скиллы** → обновить `[skill_rotation]` в билд-конфиге

Добавить запись в [CHANGELOG.md](../../CHANGELOG.md).
