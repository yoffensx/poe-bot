# 010 · FSM + pathfinding (A* по минимапу) + combat loop

| | |
|---|---|
| **Статус** | To Do |
| **Приоритет** | high |
| **Волна** | Wave 4 — Поведение |
| **Блокеры** | 004, 005, 007 |
| **Obsidian ID** | `6064edd5-ecd0-46b5-89d4-ddcbd6b09997` |

## Контекст

Связка всего вместе: конечный автомат map-цикла, навигация по миникарте и бой. Это последний блок,
дающий первый сквозной прогон `Idle → MapSelect → ModCheck → Running → Looting → Rewarding → Stashing`.

## Задача

- [ ] `logic/fsm.py`: состояния и переходы на `transitions`, события от анализаторов, guards по решениям.
- [ ] Таймауты состояний, recovery при потере фокуса/смены карты/оконного режима.
- [ ] `logic/pathfinding.py`: A* по сетке комнат из minimap-парсера, сглаживание пути, выбор точки входа в комнату.
- [ ] `logic/combat.py`: ротация скиллов по `SkillRotationConfig.cooldowns` и `hotkeys`.
- [ ] Автопоушн по `PotionsConfig` (0.60 / 0.30 / 0.50) с приоритетом escape > dps.
- [ ] Реакция на критическое HP: отступление, фласк, приоритет выживания над уроном.
- [ ] Async-цикл на `asyncio`/`anyio` с тиками и ограничением частоты кадров.

## Инварианты

- Ядро (`logic/`) не знает о capture/input/capture-деталях — только порты.
- Все действия проходят через `input/*`.
- Порядок переходов детерминирован в тестах.

## Гейт

```bash
uv run pytest tests/integration/test_fsm_cycle.py tests/unit/test_pathfinding.py tests/unit/test_combat.py
```
