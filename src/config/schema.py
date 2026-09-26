"""
Pydantic v2 schema для всех TOML-конфигов poe2-bot.
Загрузка: src/config/loader.py
"""
from __future__ import annotations

from enum import StrEnum
from typing import Any

from pydantic import BaseModel, Field, model_validator


# ─────────────────────────────────────────────────────────────────────────────
# Enums
# ─────────────────────────────────────────────────────────────────────────────

class DamageType(StrEnum):
    PHYSICAL = "physical"
    FIRE = "fire"
    COLD = "cold"
    LIGHTNING = "lightning"
    CHAOS = "chaos"


class ModCategory(StrEnum):
    LETHAL = "LETHAL"
    BUILD_SPECIFIC = "BUILD_SPECIFIC"
    BAD = "BAD"
    NEUTRAL = "NEUTRAL"
    GOOD = "GOOD"


class ModAction(StrEnum):
    SKIP = "SKIP"
    SKIP_IF_CONDITION = "SKIP_IF_CONDITION"
    ALLOW = "ALLOW"


class LootAction(StrEnum):
    PICKUP = "PICKUP"
    SKIP = "SKIP"


# ─────────────────────────────────────────────────────────────────────────────
# Build Config
# ─────────────────────────────────────────────────────────────────────────────

class CharacterConfig(BaseModel):
    name: str
    class_: str = Field(alias="class")
    ascendancy: str = ""
    level: int = Field(ge=1, le=100)

    model_config = {"populate_by_name": True}


class DamageConfig(BaseModel):
    primary_type: DamageType
    secondary_type: DamageType | None = None
    has_fire: bool = False
    has_cold: bool = False
    has_chaos: bool = False


class ResistancesConfig(BaseModel):
    fire: int = Field(default=75, ge=-100, le=90)
    cold: int = Field(default=75, ge=-100, le=90)
    lightning: int = Field(default=75, ge=-100, le=90)
    chaos: int = Field(default=30, ge=-100, le=90)


class DefenseConfig(BaseModel):
    leech_dependent: bool = False
    mana_leech_dependent: bool = False
    regen_dependent: bool = False
    flask_dependent: bool = True
    es_build: bool = False
    resistances: ResistancesConfig = Field(default_factory=ResistancesConfig)


class MapFilterConfig(BaseModel):
    min_score: int = -30
    max_danger_mods: int = 2


class StatPrioritiesConfig(BaseModel):
    crit_chance: int = Field(default=0, ge=0, le=100)
    crit_multiplier: int = Field(default=0, ge=0, le=100)
    attack_speed: int = Field(default=0, ge=0, le=100)
    lightning_damage: int = Field(default=0, ge=0, le=100)
    flat_phys: int = Field(default=0, ge=0, le=100)
    flat_lightning: int = Field(default=0, ge=0, le=100)
    life: int = Field(default=75, ge=0, le=100)
    energy_shield: int = Field(default=30, ge=0, le=100)
    movement_speed: int = Field(default=70, ge=0, le=100)
    resistances: int = Field(default=65, ge=0, le=100)


class SkillRotationConfig(BaseModel):
    main_skill: str
    movement: str | None = None
    curse: str | None = None
    buff: str | None = None
    cooldowns: dict[str, int] = Field(default_factory=dict)  # ms
    hotkeys: dict[str, str] = Field(default_factory=dict)


class PotionsConfig(BaseModel):
    life_flask_threshold: float = Field(default=0.60, ge=0.0, le=1.0)
    mana_flask_threshold: float = Field(default=0.30, ge=0.0, le=1.0)
    es_flask_threshold: float = Field(default=0.50, ge=0.0, le=1.0)


class BuildConfig(BaseModel):
    character: CharacterConfig
    damage: DamageConfig
    defense: DefenseConfig
    map_filter: MapFilterConfig = Field(default_factory=MapFilterConfig)
    stat_priorities: StatPrioritiesConfig = Field(default_factory=StatPrioritiesConfig)
    skill_rotation: SkillRotationConfig
    potions: PotionsConfig = Field(default_factory=PotionsConfig)
    build_tags: dict[str, list[str]] = Field(default_factory=dict)

    @property
    def tags(self) -> list[str]:
        return self.build_tags.get("tags", [])


# ─────────────────────────────────────────────────────────────────────────────
# Mod Database
# ─────────────────────────────────────────────────────────────────────────────

class ModEntry(BaseModel):
    pattern: str
    category: ModCategory
    action: ModAction = ModAction.ALLOW
    reason: str = ""
    condition: str | None = None   # Python-expr evaluated against BuildConfig fields
    weight: int = 0


class ModDatabase(BaseModel):
    mods: list[ModEntry] = Field(default_factory=list)


# ─────────────────────────────────────────────────────────────────────────────
# Reward Priorities
# ─────────────────────────────────────────────────────────────────────────────

class RewardEntry(BaseModel):
    name: str
    priority: int = Field(ge=0, le=100)
    build_tags: list[str] = Field(default_factory=list)


class SelectionConfig(BaseModel):
    prefer_currency_over_equipment: bool = True
    tiebreak: str = "first"


class RewardPrioritiesConfig(BaseModel):
    rewards: list[RewardEntry] = Field(default_factory=list)
    selection: SelectionConfig = Field(default_factory=SelectionConfig)


# ─────────────────────────────────────────────────────────────────────────────
# Debuff Weights
# ─────────────────────────────────────────────────────────────────────────────

class DebuffEntry(BaseModel):
    pattern: str
    pain_score: int = Field(ge=0, le=100)
    condition: str | None = None


class DebuffSelectionConfig(BaseModel):
    strategy: str = "min_pain"
    max_acceptable_pain: int = 80


class DebuffWeightsConfig(BaseModel):
    debuffs: list[DebuffEntry] = Field(default_factory=list)
    selection: DebuffSelectionConfig = Field(default_factory=DebuffSelectionConfig)


# ─────────────────────────────────────────────────────────────────────────────
# Loot Filter
# ─────────────────────────────────────────────────────────────────────────────

class LootCondition(BaseModel):
    field: str
    op: str
    value: Any


class LootRule(BaseModel):
    name: str
    action: LootAction
    conditions: list[LootCondition] = Field(default_factory=list)
    priority: int = 0
    build_tags: list[str] = Field(default_factory=list)


class LootDefaults(BaseModel):
    unmatched_action: LootAction = LootAction.SKIP
    alt_quality_pickup: bool = False


class LootFilterConfig(BaseModel):
    rules: list[LootRule] = Field(default_factory=list)
    defaults: LootDefaults = Field(default_factory=LootDefaults)

    @model_validator(mode="after")
    def sort_rules_by_priority(self) -> "LootFilterConfig":
        self.rules = sorted(self.rules, key=lambda r: r.priority, reverse=True)
        return self


# ─────────────────────────────────────────────────────────────────────────────
# Root config bundle (всё в одном месте)
# ─────────────────────────────────────────────────────────────────────────────

class BotConfig(BaseModel):
    build: BuildConfig
    mods: ModDatabase
    rewards: RewardPrioritiesConfig
    debuffs: DebuffWeightsConfig
    loot: LootFilterConfig
