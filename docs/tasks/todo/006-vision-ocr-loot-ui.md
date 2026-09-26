# 006 · Vision: OCR engine + loot scanner + UI parser

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | medium |
| **Волна** | Wave 2 — Vision |
| **Блокеры** | 003, 012 |
| **Блокирует** | 007, 009, 011 |
| **Obsidian ID** | `65d82db8-efa7-4a19-884c-1bccb684da46` |

## Контекст

Текст с экрана — единственный способ узнать моды карты, названия наград и дебаффов, имена предметов.
`pytesseract` уже в зависимостях, кода нет.

## Задача

- [ ] `vision/ocr.py`: обёртка pytesseract, per-region настройки PSM/whitelist, препроцессинг (grayscale, upscale, threshold).
- [ ] Кэш результатов по хэшу ROI, чтобы не OCR-ить один и тот же кадр.
- [ ] Graceful degradation: если Tesseract не установлен — понятная ошибка и режим `--analyze-only` без OCR.
- [ ] `vision/ui_parser.py`: список модов карты, заголовки наград и дебаффов, координаты элементов.
- [ ] `vision/loot_scanner.py`: детекция предметов на земле, распознавание бейджей, координаты для клика.

## Инварианты

- Порог fuzzy-матчинга и нормализация текста — конфигурируемые, не магические числа в коде.
- OCR не падает на пустом/шумном кадре, а возвращает пустой результат с причиной.

## Гейт

```bash
uv run pytest tests/unit/test_ocr.py tests/unit/test_ui_parser.py tests/unit/test_loot_scanner.py
```
