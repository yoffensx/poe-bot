# 000 · Init Project scaffold + ADR + README + AGENTS.md

| | |
|---|---|
| **Статус** | Done |
| **Завершено** | 2026-09-26T20:10:19Z |
| **Коммит** | `24a6fb5 feat: initial project scaffold` |
| **Obsidian ID** | `67c3d82e-36e0-455b-a38c-6fee2aa587a5` |

## Что сделано

- `git init`, структура каталогов `src/`, `cpp/`, `configs/`, `docs/`, `tests/`, `tools/`.
- `README.md` и `AGENTS.md` (локальный SSOT для агентов).
- ADR-0001 (External-архитектура), ADR-0002 (Python + C++ DLL), ADR-0003 (Pixel-based vision).
- `pyproject.toml`: uv, ruff, mypy strict, pytest, pydantic v2, hatchling.
- TOML-конфиги: `mod_database.toml`, `reward_priorities.toml`, `debuff_weights.toml`, `loot_filter.toml`,
  `build_configs/lightning_arrow.toml`.
- `src/config/schema.py`: Pydantic v2 модели для всех конфигов.

## Что осталось за рамками задачи

- Все модули кроме `config/schema.py` пусты → задачи 001–014 в `docs/tasks/todo/`.
- `docs/architecture/mod_system.md` не был закоммичен → задача 014.
