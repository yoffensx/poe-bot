# 013 · Quality gates: CI (ruff/mypy/pytest + CMake) + pre-commit + ADR

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | medium |
| **Волна** | Wave 5 — Инфраструктура |
| **Блокеры** | 001 |
| **Obsidian ID** | `739690f4-1ee0-45cb-a450-79e97a63d1ef` |

## Контекст

Качество ничем не закреплено: нет ни тестов, ни CI. `mypy = strict` и богатый набор ruff-правил в
`pyproject.toml` не проверяются никем, C++ DLL вообще не собирается.

## Задача

- [ ] `.github/workflows/ci.yml`: `uv sync` → `ruff check` → `mypy --strict` → `pytest --cov`.
- [ ] Отдельный job на Windows: сборка C++ DLL (CMake + MSVC) и smoke-тест ctypes-биндинга.
- [ ] Job на Linux для чисто-Python слоёв (vision/analyzers/logic) — быстрый feedback.
- [ ] Pre-commit: ruff, mypy, проверка наличия ADR/структуры.
- [ ] Локальный ADR о тестовой стратегии: golden-фикстуры, моки портов, что покрываем обязательно.
- [ ] Порог покрытия для `analyzers/` и `logic/` (ориентир: 90%).

## Инварианты

- CI не требует запущенной игры и установленного Tesseract для зелёного статуса.
- Тесты не зависят от реального экрана: только моки и фикстуры.

## Гейт

```bash
uv run ruff check src/ && uv run mypy src/ && uv run pytest tests/unit/
```
