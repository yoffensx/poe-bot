# 002 · Config loader + CLI (`--build` / `--dry-run` / `--analyze-only`)

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 0 — Фундамент |
| **Блокеры** | 001 |
| **Блокирует** | 003, 005, 007, 008, 009 |
| **Obsidian ID** | `bda64610-c6e2-452f-8923-d0736f960e09` |

## Контекст

Готова только Pydantic-схема `src/config/schema.py` и пять TOML в `configs/`. Загрузчика нет — `BotConfig`
нечем наполнить, все остальные слои не запустятся.

## Задача

- [ ] `config/loader.py`: чтение TOML через `tomllib` → `BotConfig`, dataclass-результат с путями к источникам.
- [ ] Каскад `build_tags` и разрешение путей относительно корня репозитория/конфиг-директории.
- [ ] Понятные ошибки валидации: путь к файлу, проблемное поле, ожидаемое значение.
- [ ] CLI в `__main__.py`: `--build`, `--dry-run` (без ввода), `--analyze-only` (только разбор модов).
- [ ] Логирование через loguru, вывод ошибок через rich.

## Инварианты

- TOML — единственный источник правды; в коде нет имён модов, наград и предметов.
- `BuildConfig.tags` (property) — единственная точка доступа к тегам билда.

## Гейт

```bash
uv run poe2bot --build configs/build_configs/lightning_arrow.toml --dry-run
uv run pytest tests/unit/test_config_loader.py
```
