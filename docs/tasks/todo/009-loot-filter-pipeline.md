# 009 · Loot filter evaluator + scanner→pickup pipeline

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | medium |
| **Волна** | Wave 3 — Решения |
| **Блокеры** | 002, 004, 006 |
| **Блокирует** | 011 |
| **Obsidian ID** | `2d4597ee-5136-47c4-8583-b20a8cc2fac1` |

## Контекст

`loot_filter.toml` и схема `LootFilterConfig` готовы, кода нет. Нужно превратить поток предметов с земли
в решение «подобрать / пропустить» и связать его с кликом.

## Задача

- [ ] `logic/loot_filter.py`: правила по убыванию `priority` (сортировка уже в схеме), условия `field/op/value`.
- [ ] Фильтрация по `build_tags` билда.
- [ ] `defaults.unmatched_action` и `alt_quality_pickup`.
- [ ] Нормализация предмета: rarity (normal/magic/rare/unique), тип базы, качество, количество в стеке.
- [ ] Связка `vision/loot_scanner` → `logic/loot_filter` → `input/mouse`: выбор порядка подбора, защита от повторного клика по уже забранному.
- [ ] Фильтрация по расстоянию/видимости, чтобы не бегать за лутом по всей карте.

## Инварианты

- Правила только из TOML; ни одного имени предмета в Python.
- `logic/` остаётся чистым: решение принимает структуры, действие вызывает исполнитель.

## Гейт

```bash
uv run pytest tests/unit/test_loot_filter.py
```
