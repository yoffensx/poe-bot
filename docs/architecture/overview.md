# Architecture — Full System Overview

## Режимы работы

```
build_config.toml → [bot] mode = "external" | "internal" | "hybrid"
```

| Режим | Данные | Риск | Применение |
|-------|--------|------|-----------|
| **external** | DXGI capture + OCR/CV | низкий | фарм-бот, default |
| **internal** | manual-mapped DLL + shared memory | средний | точные HP/coords |
| **hybrid** | internal GameState + external для UI | средний | оптимальный баланс |

---

## Stack

| Layer | Tech | Notes |
|-------|------|-------|
| Screen Capture | C++ / DXGI Duplication API | ~1ms latency, compositor-level |
| Vision | Python 3.12 + OpenCV 4.x | HSV masks, template match, contour |
| OCR | Tesseract 5 + pytesseract | Fine-tuned whitelist, ROI-only |
| Logic / FSM | Python + transitions | States: Idle→MapSelect→Running→... |
| Config | TOML + Pydantic v2 | Strict typing, no hardcodes |
| Input | C++ DLL (Bezier + jitter) | SendInput, upgradeable to KMBox |
| Internal Loader | C++ / direct syscalls | Manual map, NtCreateThreadEx |
| Shared Memory IPC | mmap / named mapping | Payload → Python bridge |
| Build | uv + CMake/MSVC | Python env + C++ DLL |

---

## Layer Dependency Graph

```
                         [configs/*.toml]
                               │
                         [src/config/]         ← Pydantic v2 schema + loader
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
    ════ EXTERNAL ════  ════ INTERNAL ════         │
            │                  │                  ▼
  [cpp/capture/]      [cpp/loader/]      [src/analyzers/]
  DXGI → frame        manual mapper       Pure functions
            │                  │          (mod/reward/debuff)
            ▼                  ▼
  [src/vision/]       [cpp/internal/]
  OCR / CV            payload DLL
  HP, minimap,        AOB scan → read
  loot, UI            HP,coords,mobs
            │                  │
            └─────────┬─────────┘
                      ▼
           [src/capture/capture_service.py]
           unified GameState (external | internal | hybrid)
                      │
                      ▼
             [src/logic/]          ← FSM, A*, combat, loot
                      │
             [src/input/]          ← humanized SendInput
                      │
             [cpp/input/]          ← Bezier + jitter DLL
```

---

## FSM States

```
IDLE
  │  bot.start()
  ▼
MAP_SELECT ──── no maps left ──▶ IDLE
  │  pick map from stash
  ▼
MOD_CHECK ──── score < threshold ──▶ MAP_SELECT
  │  ModAnalyzer.evaluate()
  ▼
RUNNING ◀──────────────────────────┐
  │  room loop:                    │
  │  navigate → fight → loot       │
  │  reward? → REWARDING ──────────┘
  │  debuff? → DEBUFFING ──────────┘
  │  all rooms done?
  ▼
LOOTING
  │  final loot sweep
  ▼
STASHING
  │  portal → town → stash
  ▼
MAP_SELECT
```

---

## Performance Budget

| Step | Target | Method |
|------|--------|--------|
| DXGI frame grab | < 2ms | IDXGIOutputDuplication |
| ROI crop | < 1ms | numpy slice |
| HP/ES detect | < 3ms | HSV threshold on orb ROI |
| Minimap parse | < 5ms | HSV blob, color cluster |
| OCR mods | < 20ms | Tesseract PSM 6, mod-area ROI |
| ModAnalyzer | < 1ms | dict + rapidfuzz |
| A* path | < 5ms | 320×320 grid |
| **Total loop** | **< 40ms** | **25 FPS bot loop** |

---

## Directory Map

```
poe2-bot/
├── src/
│   ├── capture/          capture.py — ctypes wrapper над C++ DLL
│   ├── vision/
│   │   ├── ocr.py        Tesseract, ROI-aware, whitelist
│   │   ├── minimap.py    blob detection, A* grid builder
│   │   ├── loot_scanner  Alt+scan → LootItem list
│   │   └── ui_parser.py  screen context, reward/debuff detection
│   ├── analyzers/
│   │   ├── mod_analyzer.py     mods → MapDecision
│   │   ├── reward_picker.py    rewards[] → best index
│   │   └── debuff_picker.py    debuffs[] → min pain index
│   ├── logic/
│   │   ├── fsm.py         BotFSM (transitions)
│   │   ├── pathfinding.py A* on minimap grid
│   │   ├── combat.py      skill rotation + flask triggers
│   │   └── loot_filter.py LootRule engine
│   ├── input/
│   │   ├── mouse.py       ctypes → poe2_input.dll: move, click
│   │   └── keyboard.py    ctypes → poe2_input.dll: key_press
│   ├── stash/
│   │   └── manager.py     stash tab layout, auto-sort
│   └── config/
│       ├── schema.py      Pydantic v2 models
│       └── loader.py      load_bot_config(build_path) → BotConfig
├── cpp/
│   ├── capture/           CMakeLists.txt + dxgi_capture.cpp
│   └── input/             CMakeLists.txt + humanized_input.cpp
├── configs/               TOML files (SSOT)
├── docs/
│   ├── adr/               Architecture Decision Records
│   └── architecture/      System diagrams + module docs
├── tests/
│   ├── unit/              Analyzers + logic (pure, no I/O)
│   └── integration/       Full pipeline with mock frames
└── tools/
    ├── calibrator/        Interactive ROI setup
    ├── ocr_trainer/       Tesseract fine-tune для шрифта PoE2
    └── mod_recorder/      Запись новых модов с экрана → TOML
```

---

## C++ DLL Interface

### `poe2_capture.dll`

```cpp
extern "C" {
    bool init_capture(int adapter_idx);
    bool grab_frame(void* bgra_buf, int* out_stride);  // caller alloc
    void release_capture();
}
```

Python-сторона (`src/capture/capture.py`):
```python
lib = ctypes.CDLL("poe2_capture.dll")
# размеры берём из калибровки (ui_regions.toml), а не хардкодим
buf = (ctypes.c_uint8 * (width * height * 4))()
lib.grab_frame(buf, ctypes.byref(stride))
frame = np.frombuffer(buf, dtype=np.uint8).reshape(height, width, 4)
```

### `poe2_input.dll`

```cpp
extern "C" {
    void move_mouse(int x, int y, int duration_ms);
    void click(int x, int y, int button);  // 0=left, 1=right, 2=middle
    void key_press(int vk, int delay_mean_ms, int delay_std_ms);
}
```

---

## Config SSOT Contract

> **Запрещено хардкодить** имена модов, наград, итемов, скиллов в Python-коде.  
> Всё читается из TOML через `BotConfig`.

| TOML файл | Pydantic модель | Загружается в |
|-----------|----------------|--------------|
| `build_configs/*.toml` | `BuildConfig` | `BotConfig.build` |
| `mod_database.toml` | `ModDatabase` | `BotConfig.mods` |
| `reward_priorities.toml` | `RewardPrioritiesConfig` | `BotConfig.rewards` |
| `debuff_weights.toml` | `DebuffWeightsConfig` | `BotConfig.debuffs` |
| `loot_filter.toml` | `LootFilterConfig` | `BotConfig.loot` |
| `configs/ui_regions.toml` | `UIRegionsConfig` | `CaptureService` |
