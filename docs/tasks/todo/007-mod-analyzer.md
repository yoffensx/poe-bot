# 007 · Mod analyzer: skip/run решение по build_config

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 3 — Решения |
| **Блокеры** | 002, 006 |
| **Блокирует** | 010 |
| **Obsidian ID** | `9e1369ac-e018-4029-9df1-653af3a4121b` |

## Контекст

Центральный компонент ценности бота: из списка распознанных модов и `BuildConfig` принимается решение
`RUN` / `SKIP`. Алгоритм уже спроектирован в [`docs/architecture/mod_system.md`](../architecture/mod_system.md) —
эта задача его реализует.

## Задача

- [ ] `analyzers/mod_analyzer.py`: fuzzy-матчинг (`rapidfuzz`, порог 85) по `pattern` из `mod_database.toml`.
- [ ] Нормализация OCR-текста перед матчингом (пунктуация, регистр, неразрывные пробелы).
- [ ] Безопасный интерпретатор `condition` без `eval()`: whitelist полей `BuildConfig`, операторы `==`, `!=`, `<`, `>`, `and`, `or`.
- [ ] Аккумуляция `weight` → `score`; сравнение с `map_filter.min_score`, учёт `max_danger_mods`.
- [ ] `LETHAL` + `action = SKIP` → hard skip без учёта остальных модов.
- [ ] Неизвестный мод → игнор с записью в лог (не ошибка).

## Инварианты

- Чистая функция: вход `list[str] + BuildConfig`, выход `MapDecision`. Ноль I/O.
- Ни одного имени мода в Python-коде — только TOML.
- `condition` не исполняется как Python-код.

## Гейт

```bash
uv run pytest tests/unit/test_mod_analyzer.py
```

Тест на каждый мод из `mod_database.toml`: `test_lethal_no_regen_causes_skip`,
`test_immune_lightning_skips_lightning_build`, `test_immune_lightning_allows_fire_build`.
