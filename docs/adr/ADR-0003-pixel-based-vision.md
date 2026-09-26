# ADR-0003: Pixel-based Vision (нет Memory Reading)

- **Статус:** Принято
- **Дата:** 2026-09-26
- **Авторы:** Maik

---

## Контекст

Для принятия решений бот должен знать состояние игры:
- Текущие HP/MP/ES
- Моды карты (текст аффиксов)
- Позиция на минимапе, враги
- Содержимое инвентаря и лута
- Текущий экран (reward, debuff, map screen, town, etc.)

Два способа получить эти данные:
1. **Memory reading** — `ReadProcessMemory` к структурам PoE2
2. **Pixel-based** — OCR и CV на захваченных кадрах

## Решение

**Строго pixel-based** — никакого чтения памяти процесса `PathOfExile2.exe`.

### Что читаем из пикселей

| Игровой элемент | Метод |
|----------------|-------|
| HP/MP/ES орбы | HSV маска → процент заполнения |
| Текст модов карты | Tesseract OCR (whitelist: латиница + цифры + %) |
| Minimap: позиция | Белая точка игрока на минимапе |
| Minimap: враги | Красные/оранжевые точки |
| Minimap: unexplored | Тёмные области с характерным паттерном |
| Лут на земле | Alt → OCR имён; цвет рамки → редкость |
| Инвентарь | Сетка 12×5, template matching по иконкам |
| Экраны UI | Детект характерных элементов (заголовки, кнопки) |
| Skill cooldowns | Pixel-watch иконок скиллбара (затемнение = КД) |

## Обоснование

### Pixel-based: pros
- **Нет `OpenProcess(PROCESS_VM_READ)`** — стандартная ban-wave trigger точка
- **Не зависит от структур памяти PoE2** — патч игры не ломает бота (офсеты меняются)
- **Работает через любой overlay** — захват compositor независим
- **Юридически менее агрессивно** (в контексте ToS)

### Pixel-based: cons
- OCR может ошибаться при нестандартных шрифтах → решено: fine-tuning Tesseract под шрифт PoE2
- Медленнее на слабом железе → решено: ROI-based OCR (не весь экран, только нужные области)
- Не читает off-screen данные → окей: всё нужное видно на экране

### Memory reading: почему отклонено
- `OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION)` — триггер для integrity check
- Структуры памяти меняются при каждом патче → постоянный реверс
- GGG может добавить ring-0 protection → мгновенный брик бота

## Реализация

### ROI (Region of Interest) система

Бот работает не с полным кадром, а с заранее определёнными ROI:

```python
# configs/ui_regions.toml  (генерируется calibrator'ом)
[regions]
hp_orb = { x=48, y=938, w=120, h=120 }
mp_orb = { x=1752, y=938, w=120, h=120 }
minimap = { x=1600, y=20, w=320, h=320 }
skill_bar = { x=600, y=1010, w=720, h=60 }
map_mods_text = { x=300, y=200, w=800, h=600 }
```

Calibrator запускается после каждого патча, меняющего UI.

### OCR pipeline

```
ROI crop → grayscale → threshold (adaptive) → Tesseract → regex clean → match DB
```

Tesseract конфигурация:
- `--psm 6` (block of text) для списков модов
- `--psm 7` (single line) для имён итемов
- Whitelist: `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789%+-(),'`

## Последствия

- Нужен `tools/calibrator/` для настройки ROI после патчей
- Нужен `tools/ocr_trainer/` для fine-tuning Tesseract под шрифт PoE2
- Frame processing pipeline должен укладываться в < 33ms
- Тесты OCR accuracy на реальных скриншотах PoE2

## Связанные ADR

- ADR-0001: External architecture
- ADR-0002: Python + C++ DLL split
