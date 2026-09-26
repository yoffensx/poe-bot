# 008 · Reward picker + debuff picker

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | medium |
| **Волна** | Wave 3 — Решения |
| **Блокеры** | 002, 006 |
| **Блокирует** | 010 |
| **Obsidian ID** | `36e0b8c9-dad0-447a-a046-ddedb49d9df7` |

## Контекст

После прохождения карты PoE2 предлагает выбор наград и выбор дебаффов. Бот должен выбирать осознанно,
опираясь на `reward_priorities.toml` и `debuff_weights.toml` (схема уже готова).

## Задача

- [ ] `analyzers/reward_picker.py`: приоритет по `RewardEntry.priority` с учётом `build_tags`.
- [ ] Учёт `prefer_currency_over_equipment` и правила `tiebreak` (`first` / `random`).
- [ ] `analyzers/debuff_picker.py`: стратегия `min_pain`, `pain_score` из `DebuffEntry`, порог `max_acceptable_pain`.
- [ ] Условия `DebuffEntry.condition` — тот же безопасный интерпретатор, что в 007 (общий модуль, без дублирования).
- [ ] Логирование решения с причиной и оценками по вариантам.

## Инварианты

- Чистые функции без I/O, полное детерминированное поведение при равных оценках.
- Все названия наград и дебаффов — только из TOML.

## Гейт

```bash
uv run pytest tests/unit/test_reward_picker.py tests/unit/test_debuff_picker.py
```
