# 004 · C++ Humanized input DLL + `input/mouse.py`, `input/keyboard.py`

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 1 — Ввод и вывод |
| **Блокеры** | 002 |
| **Блокирует** | 010, 011 |
| **Obsidian ID** | `dcab8a20-38c1-44fc-8b56-06456eddbc77` |

## Контекст

`cpp/input/` и `src/poe2bot/input/` пусты. По AGENTS.md весь SendInput обязан идти через эти модули
с обязательной гуманизацией — это ключевой антидетект-слой.

## Задача

- [ ] `cpp/input/CMakeLists.txt` + DLL: SendInput-обёртки, очередь событий, троттлинг.
- [ ] Гуманизация мыши: Bézier-кривые между точками, gaussian jitter по осям, ease-in/out.
- [ ] Случайные паузы, varying inter-click delay, anti-idle микродвижения.
- [ ] Клавиатура: нажатие по кулдаунам из конфига, последовательности хоткеев, отмена по `Escape`.
- [ ] `input/mouse.py` и `input/keyboard.py`: строгая типизация, безопасные дефолты, логирование действий.

## Инварианты

- Весь ввод — только через `input/*`; прямых SendInput в `logic/` нет.
- Точки кликов — из ROI-конфига, а не «магические» константы в коде.

## Гейт

```bash
uv run pytest tests/unit/test_mouse_path.py tests/unit/test_keyboard_cooldowns.py
uv run mypy src/
```
