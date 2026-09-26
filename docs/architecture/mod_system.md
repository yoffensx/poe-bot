# Architecture — Mod Decision System

## Обзор

Модуль принятия решений по модам карты — центральный компонент бота.  
Читает список модов из OCR → сопоставляет с базой → возвращает `MapDecision`.

---

## Компоненты

```
vision/ui_parser.py       → list[str] (raw mod texts)
         ↓
analyzers/mod_analyzer.py → ModAnalyzer.evaluate(mods, build) → MapDecision
         ↓
logic/fsm.py              → RUN | SKIP → следующее состояние
```

---

## Алгоритм оценки

```python
def evaluate(mods: list[str], build: BuildConfig) -> MapDecision:
    score = 0
    
    for raw_mod in mods:
        entry = db.fuzzy_match(raw_mod)   # fuzz ratio >= 85
        if entry is None:
            continue                       # неизвестный мод — игнорируем
        
        # 1. Lethal → hard SKIP
        if entry.category == LETHAL and entry.action == SKIP:
            return MapDecision(action=SKIP, reason=entry.reason)
        
        # 2. Build-specific → eval condition
        if entry.action == SKIP_IF_CONDITION:
            if eval_condition(entry.condition, build):
                return MapDecision(action=SKIP, reason=entry.reason)
        
        # 3. Accumulate score
        score += entry.weight
    
    # 4. Score threshold
    if score < build.map_filter.min_score:
        return MapDecision(action=SKIP, reason=f"low_score:{score}")
    
    return MapDecision(action=RUN, score=score)
```

---

## Fuzzy Matching

Текст из OCR никогда не будет точно совпадать с паттерном в базе.  
Используем `rapidfuzz`:

```python
from rapidfuzz import fuzz, process

def fuzzy_match(raw_text: str, threshold: int = 85) -> ModEntry | None:
    best = process.extractOne(
        raw_text,
        [entry.pattern for entry in db.mods],
        scorer=fuzz.partial_ratio,
    )
    if best and best[1] >= threshold:
        return db.mods[best[2]]
    return None
```

Порог 85 — баланс между ложными срабатываниями и пропущенными модами.

---

## MapDecision dataclass

```python
@dataclass
class MapDecision:
    action: Literal["RUN", "SKIP"]
    score: int = 0
    reason: str = ""
    matched_mods: list[ModEntry] = field(default_factory=list)
    dangerous_mods: list[str] = field(default_factory=list)
```

---

## Condition Evaluator

Условие из TOML (`condition = "flask_dependent == true"`) вычисляется безопасно:

```python
def eval_condition(condition: str, build: BuildConfig) -> bool:
    ctx = {
        "primary_damage_type": build.damage.primary_type,
        "leech_dependent": build.defense.leech_dependent,
        "flask_dependent": build.defense.flask_dependent,
        "regen_dependent": build.defense.regen_dependent,
        "fire_res": build.defense.resistances.fire,
        "cold_res": build.defense.resistances.cold,
        "lightning_res": build.defense.resistances.lightning,
        "chaos_res": build.defense.resistances.chaos,
    }
    # Whitelist approach — никакого eval() с пользовательским вводом
    return _safe_eval(condition, ctx)
```

`_safe_eval` разбирает только `==`, `!=`, `<`, `>`, `and`, `or` — никакого `eval()`.

---

## Тесты

Каждый мод в базе должен иметь тест в `tests/unit/test_mod_analyzer.py`:

```python
def test_lethal_no_regen_causes_skip():
    build = make_build(regen_dependent=True)
    decision = analyzer.evaluate(
        ["Players cannot Regenerate Life, Mana or Energy Shield"], build
    )
    assert decision.action == "SKIP"

def test_immune_lightning_skips_lightning_build():
    build = make_build(primary_damage_type="lightning")
    decision = analyzer.evaluate(["Monsters are Immune to Lightning Damage"], build)
    assert decision.action == "SKIP"

def test_immune_lightning_allows_fire_build():
    build = make_build(primary_damage_type="fire")
    decision = analyzer.evaluate(["Monsters are Immune to Lightning Damage"], build)
    assert decision.action == "RUN"
```
