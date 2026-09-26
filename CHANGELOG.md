# CHANGELOG

Все значимые изменения проекта. Формат: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).  
Версионирование: [Semantic Versioning](https://semver.org/).  
Завершённые задачи из Obsidian → сюда.

---

## [Unreleased]

### Planned

#### Internal Mode (новый — ADR-0004)
- `cpp/loader/mapper.cpp` — Manual Map Loader (NtCreateThreadEx, PE reloc, hash imports) · `#61a5e650`
- `cpp/internal/payload.cpp` — Payload DLL (AOB scan, GameState → SharedMemory, 30Hz) · `#417c5de2`
- `src/capture/internal_bridge.py` — Python mmap reader · `#1db34de8`
- `src/capture/capture_service.py` — unified GameState (external|internal|hybrid) · `#1db34de8`

#### Core Logic
- `src/config/loader.py` + Analyzers (ModAnalyzer, RewardPicker, DebuffPicker) · `#0558f2e9`
- `cpp/capture/` DXGI DLL + `cpp/input/` Humanized Input DLL · `#f8f7855e`
- Vision layer: OCR, Minimap, LootScanner, UIParser, HPMonitor · `#de09c158`
- BotFSM + A\* + Combat + LootFilter + StashManager · `#22f73dae`

---

## [0.1.0] — 2026-09-26

### Added
**Obsidian Task:** `Init: Project scaffold + ADR + README + AGENTS.md` · `#67c3d82e`  
**Commit:** `24a6fb5`

#### Документация
- `README.md` — полное описание проекта: архитектура, стек, структура, быстрый старт
- `AGENTS.md` — инструкции для AI-агентов: ограничения, workflow, команды
- `docs/ENGINEERING_RULES.md` — 10 правил разработки (архитектура, типы, тесты, git)
- `docs/CHANGELOG.md` — этот файл

#### Architecture Decision Records
- `docs/adr/ADR-0001-external-architecture.md` — External-only (нет инъекций, нет ReadProcessMemory)
- `docs/adr/ADR-0002-python-cpp-split.md` — Python (логика) + C++ DLL (DXGI capture, input)
- `docs/adr/ADR-0003-pixel-based-vision.md` — Pixel-based OCR/CV вместо memory reading

#### Architecture Docs
- `docs/architecture/overview.md` — полная схема системы, FSM states, perf budget, DLL interface
- `docs/architecture/mod_system.md` — алгоритм оценки модов, fuzzy matching, condition evaluator

#### Config (TOML SSOT)
- `configs/mod_database.toml` — база модов: LETHAL / BUILD_SPECIFIC / BAD / GOOD / NEUTRAL
- `configs/build_configs/lightning_arrow.toml` — конфиг Lightning Arrow Deadeye
- `configs/reward_priorities.toml` — приоритеты наград с build_tags
- `configs/debuff_weights.toml` — pain-scores дебафов, strategy=min_pain
- `configs/loot_filter.toml` — правила подбора лута с приоритетами и условиями

#### Python scaffold
- `pyproject.toml` — uv, ruff, mypy strict, pytest, pydantic v2, transitions, loguru
- `src/config/schema.py` — Pydantic v2 модели для всех TOML (`BotConfig`, `BuildConfig`, `ModDatabase`, ...)
- `src/__main__.py` — entry point
- `src/{capture,vision,analyzers,logic,input,stash,config}/__init__.py`
- `tests/{unit,integration}/__init__.py`
- `.gitignore`

---

<!-- Template для новых записей:

## [X.Y.Z] — YYYY-MM-DD

### Added | Changed | Fixed | Removed | Security

**Obsidian Task:** `<title>` · `#<short-id>`  
**Commit:** `<hash>`

- описание изменения

-->
