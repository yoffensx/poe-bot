# Tasks — poe2-bot

> Индекс рабочих карточек. Планирование целиком: [`docs/roadmap.md`](../roadmap.md).
> Карточки зеркалятся в Obsidian (проект `poe-bot`, колонки `To Do` / `In Progress` / `Done`) — при переносе папки
> менять колонку: `todo/` → `in-progress/` → `done/`, в таблице карточки проставлять статус и дату закрытия.

## Kanban

| # | Задача | Приоритет | Волна | Блокеры |
|---|--------|-----------|-------|---------|
| 001 | [Починить пакетинг: entrypoint `poe2bot` + src-layout](todo/001-poe-bot-packing-entrypoint.md) | high | 0 | — |
| 002 | [Config loader + CLI](todo/002-config-loader-cli.md) | high | 0 | 001 |
| 012 | [tools/calibrator: UI-координаты](todo/012-calibrator-ui-regions.md) | medium | 1 | 001 |
| 003 | [C++ DXGI capture DLL + ctypes-обёртка](todo/003-cpp-dxgi-capture.md) | high | 1 | 002 |
| 004 | [C++ Humanized input DLL + input/*](todo/004-cpp-humanized-input.md) | high | 1 | 002 |
| 005 | [Vision: minimap parser + HP/фласки](todo/005-vision-minimap-hp-flasks.md) | high | 2 | 003, 012 |
| 006 | [Vision: OCR engine + loot scanner + UI parser](todo/006-vision-ocr-loot-ui.md) | medium | 2 | 003, 012 |
| 007 | [Mod analyzer: skip/run решение](todo/007-mod-analyzer.md) | high | 3 | 002, 006 |
| 008 | [Reward picker + debuff picker](todo/008-reward-debuff-picker.md) | medium | 3 | 002, 006 |
| 009 | [Loot filter evaluator + pipeline](todo/009-loot-filter-pipeline.md) | medium | 3 | 002, 004, 006 |
| 010 | [FSM + pathfinding + combat loop](todo/010-fsm-pathfinding-combat.md) | high | 4 | 004, 005, 007 |
| 011 | [Stash manager: автораскладка](todo/011-stash-manager.md) | medium | 4 | 004, 006, 009 |
| 013 | [Quality gates: CI + pre-commit + ADR](todo/013-quality-gates-ci.md) | medium | 5 | 001 |
| 014 | [Синхронизировать docs и README](todo/014-docs-readme-sync.md) | low | 5 | 001 |

**Критический путь до сквозного цикла:** `001 → 002 → 003 → 004 → 005 → 006 → 007 → 010`.

## Колонки

| Папка | Статус | Правило |
|-------|--------|---------|
| `todo/` | To Do | Карточка готова к взятию, гейт описан |
| `in-progress/` | In Progress | Работа идёт, чекбоксы отмечаются по ходу |
| `done/` | Done | Все гейты пройдены, проставлены дата и коммит |

## Done

| # | Задача | Завершено |
|---|--------|-----------|
| 000 | [Init Project scaffold + ADR + README + AGENTS.md](done/000-init-project-scaffold.md) | 2026-09-26 |

## Формат карточки

1. Заголовок `# <номер> · <название>`.
2. Таблица: статус, приоритет, волна, блокеры, Obsidian ID.
3. `## Контекст` — зачем, 2–5 строк.
4. `## Задача` — чекбоксы объёма работ.
5. `## Инварианты` — что нельзя нарушать (глобальные правила проекта).
6. `## Гейт` — команды, по которым задача считается закрытой.
