# 012 · `tools/calibrator`: UI-координаты + `configs/ui_regions.toml`

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | medium |
| **Волна** | Wave 1 — Ввод и вывод |
| **Блокеры** | 001 |
| **Блокирует** | 005, 006, 009, 011 |
| **Obsidian ID** | `3b5ea734-387e-4e6f-96cb-59205e44812d` |

## Контекст

`tools/calibrator/` пуст, а README уже обещает `uv run python tools/calibrator/calibrate.py`. Все vision-модули
завязаны на координаты ROI, поэтому калибратор идёт первым в Wave 1. Файл `configs/ui_regions.toml` уже
в `.gitignore` (машинно-зависимые координаты), поэтому коммитится только пример.

## Задача

- [ ] `tools/calibrator/calibrate.py`: CLI и интерактивный режим, детекция границ ROI.
- [ ] ROI: миникарта, полоса HP/ES, фласки, слоты инвентаря, окно модификаторов карты, окно наград/дебаффов.
- [ ] Модель `UiRegionsConfig` в `config/schema.py` + поле в `BotConfig`, привязка к разрешению и UI-scale.
- [ ] Пишет `configs/ui_regions.toml`; рядом поддерживается `configs/ui_regions.example.toml` в репозитории.
- [ ] Проверка при загрузке: кадр другого разрешения → понятная ошибка с инструкцией перекалибровать.

## Инварианты

- Машинно-зависимые координаты не коммитятся (пример — да).
- Валидация ROI идёт через Pydantic, не через ручные проверки.

## Гейт

```bash
uv run python tools/calibrator/calibrate.py
uv run pytest tests/unit/test_ui_regions.py
```
