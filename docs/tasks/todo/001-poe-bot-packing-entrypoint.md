# 001 · Починить пакетинг: entrypoint `poe2bot` + src-layout

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 0 — Фундамент |
| **Блокеры** | — |
| **Блокирует** | 002, все последующие |
| **Obsidian ID** | `390813ad-f093-428b-b15f-90a6cb85e9ad` |

## Контекст

`pyproject.toml` объявляет `poe2bot = "poe2bot.__main__:main"`, но исходники лежат в `src/`, то есть пакет
называется `src` (`[tool.hatch.build.targets.wheel] packages = ["src"]`). README зовёт `uv run python -m poe2bot`
и `uv run python tools/calibrator/calibrate.py`. Сейчас ни одна из этих команд не работает.

## Задача

- [ ] Выбрать layout: `src/poe2bot/` (рекомендуется, совпадает с именем пакета и entrypoint) либо починка текущего.
- [ ] Перенести модули, поправить внутренние импорты и `mypy`-настройки.
- [ ] Реализовать `main()` в `__main__.py`: `--help`, `--version`, парсинг аргументов (полный набор — задача 002).
- [ ] Поправить `packages` в `pyproject.toml` и ссылки в `README.md` / `AGENTS.md`.

## Инварианты

- `src/analyzers/` и `src/logic/` остаются чистыми (I/O не протаскиваем в ядро).
- `mypy --strict` и `ruff` зелёные после переноса.

## Гейт

```bash
uv sync
uv run poe2bot --help
uv run ruff check src/ && uv run mypy src/ && uv run pytest tests/unit/
```
