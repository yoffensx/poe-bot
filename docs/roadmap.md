# Roadmap — poe2-bot (Internal Headless Farm Edition)

> SSOT по плану работ. Карточки задач лежат в [`docs/tasks/`](tasks/) и зеркалятся в Obsidian (`Projects/poe-bot`).  
> Статус карточки определяется её колонкой: `tasks/todo` → `tasks/in-progress` → `tasks/done`.

---

## Текущее состояние (2026-09-27)

Приоритеты проекта перестроены под **масштабирование серверных ферм на сотни параллельных ботов (Zero-Render Headless Architecture)**:

1. **Реализован Internal Core (C++ & Python):**
   - `cpp/loader/mapper.cpp` + `syscalls.h` + `syscall_stub.asm` (Hell's Gate direct syscalls, manual map, PE wipe).
   - `cpp/internal/payload.cpp` (No-CRT stealth DLL, AOB scan, SEH, 30 Hz writer).
   - `cpp/common/game_state.h` (POD GameState layout, `magic = 0x504F4532`).
   - `src/capture/internal_bridge.py` (mmap zero-copy reader, `BridgeRegistry`).
   - `src/capture/capture_service.py` (`CaptureService` + `make_services_for_pids`).
   - `cpp/CMakeLists.txt` (сборка `poe2_loader.exe` и `poe2_payload.dll`).

2. **Архитектурный сдвиг:**
   - [ADR-0004](adr/ADR-0004-internal-mode-manual-map.md) — действующий SSOT проекта.
   - [ADR-0001](adr/ADR-0001-external-architecture.md) и [ADR-0003](adr/ADR-0003-pixel-based-vision.md) переведены в статус **Deprecated / Legacy Fallback**. OCR и захват кадров отключены в боевых сценариях для устранения GPU/CPU оверхеда.

---

## Волны работ (Updated for Headless Farms)

```
Wave 0 ─ Фундамент (Пакетинг & Загрузчик) ──► 001 → 002
   │
   ├─► Wave 1 ─ Ввод/вывод & Internal Core ────► [C++ Loader + Payload + Bridge] (Готово) → 004 (Input)
   │        │
   │        ├─► Wave 2 ─ Решения (Pure Core) ──► 007 → 008 → 009
   │        │         │
   │        │         └─► Wave 3 ─ FSM & Логика ─► 010 → 011
   │        │
   │        └─► Wave 4 ─ Оркестрация Фермы ───► [Multi-Bot Farm Manager: 100+ PIDs, Proxy, Watchdog]
   │
   └─► Wave 5 (Deprecated / Low Priority) ────► 003, 005, 006, 012 (Legacy OCR / DXGI Fallback)
```

---

### Wave 0 — Фундамент
| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 001 | Починить пакетинг: entrypoint `poe2bot` + src-layout | high | `uv run poe2bot --help` работает |
| 002 | Config loader + CLI (`--build` / `--dry-run` / `--analyze-only`) | high | все TOML грузятся в `BotConfig` |

### Wave 1 — Internal Core & Humanized Input
| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| Core | C++ Manual Map Loader + Payload + Python Bridge | DONE | Shm чтение сотен инстансов по PID |
| 004 | C++ Humanized input DLL + `input/mouse.py`, `input/keyboard.py` | high | Весь ввод идет через Bezier/jitter |

### Wave 2 — Решения (Чистое ядро без I/O)
| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 007 | Mod analyzer: skip/run по build_config и хэшам модов | high | Unit-тесты на хэшах и паттернах |
| 008 | Reward picker + debuff picker | medium | Детерминированный выбор под билд |
| 009 | Loot filter evaluator | medium | Оценка предметов по маскам инвентаря/лута |

### Wave 3 — FSM & Поведение
| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 010 | FSM + координатная навигация (по float-координатам GameState) + combat loop | high | Интеграционный прогон на моках |
| 011 | Stash manager: автораскладка по слотам | medium | Unit-тесты раскладки инвентаря |

### Wave 4 — Масштабирование серверной фермы (Multi-Instance Headless)
| # | Задача | Приоритет | Гейт |
|---|--------|-----------|------|
| 020 | Farm Orchestrator: запуск и контроль 100+ клиентов PoE2 без рендера | high | Параллельный мониторинг PIDs и watchdog |
| 021 | Прокси и сетевая изоляция на инстанс | high | Отдельные socks5/VPN туннели на процесс |
| 022 | Central Farm Dashboard / Telemetry | medium | Сводная статистика фарма со всех аккаунтов |

### Wave 5 — Резервный Fallback (Deprecated / Низкий приоритет)
| # | Задача | Приоритет | Статус |
|---|--------|-----------|--------|
| 003, 005, 006, 012 | DXGI Capture, Minimap CV, Tesseract OCR, UI Calibrator | lowest | Резерв на случай отсутствия инжекта |

---

## Ключевые архитектурные инварианты

1. **Internal First (Headless SSOT):** бот ориентирован на zero-render и чтение данных через shared memory.
2. **Stealth Injection:** никаких стандартных LoadLibrary, обход через direct syscalls (Hell's Gate), зачищенные PE-заголовки.
3. **Multi-Instance Isolation:** каждый процесс изолирован по PID (`Local\poe2_gs_<pid>`).
4. **Строгая типизация:** Pydantic v2 + mypy strict.
5. **Тестируемость:** все FSM и анализаторы на 100% тестируются in-memory без живого клиента игры.
