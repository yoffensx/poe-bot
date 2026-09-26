import struct

from src.capture.internal_bridge import (
    _PACKET_FMT,
    MAX_MAP_MODS,
    MAX_MONSTERS,
    BridgeRegistry,
    GameState,
    MonsterEntry,
    _parse_packet,
)


def test_game_state_properties() -> None:
    gs = GameState(
        hp=500,
        hp_max=1000,
        mana=150,
        mana_max=300,
        monsters=[
            MonsterEntry(hp=100, is_alive=True),
            MonsterEntry(hp=0, is_alive=False),
        ],
    )
    assert gs.hp_pct == 0.5
    assert gs.mana_pct == 0.5
    assert len(gs.alive_monsters) == 1
    assert gs.alive_monsters[0].hp == 100


def test_parse_packet_binary() -> None:
    # Build a simulated GameStatePacket binary payload
    hp, hp_max = 3500, 4000
    mana, mana_max = 800, 1000
    es, es_max = 500, 500
    px, py, pz = 100.5, 200.25, 0.0
    angle = 1.57
    flasks = (10, 20, 30, 40, 50, 60)
    mon_count = 2

    # Pack header fields up to monsters
    header = struct.pack(
        "<IIIIIIffff6BxxI",
        hp,
        hp_max,
        mana,
        mana_max,
        es,
        es_max,
        px,
        py,
        pz,
        angle,
        *flasks,
        mon_count,
    )

    # Pack 128 monsters
    monsters_data = bytearray()
    # Monster 0: alive
    monsters_data += struct.pack("<fffIIIBBxx", 110.0, 210.0, 0.0, 500, 1000, 1, 2, 1)
    # Monster 1: dead
    monsters_data += struct.pack("<fffIIIBBxx", 120.0, 220.0, 0.0, 0, 1000, 2, 0, 0)
    # Remaining 126 empty monsters
    empty_monster = struct.pack("<fffIIIBBxx", 0.0, 0.0, 0.0, 0, 0, 0, 0, 0)
    for _ in range(MAX_MONSTERS - 2):
        monsters_data += empty_monster

    # Map mods + tail
    mod_count = 3
    mod_hashes = [111, 222, 333] + [0] * (MAX_MAP_MODS - 3)
    tail = struct.pack(
        f"<I{MAX_MAP_MODS}IQIBxxxQ",
        mod_count,
        *mod_hashes,
        0b10101,  # inventoryMask
        9999,  # areaHash
        1,  # isInMap
        123456789,  # timestampMs
    )

    full_packet = header + bytes(monsters_data) + tail
    assert len(full_packet) == struct.calcsize("<" + _PACKET_FMT)

    gs = _parse_packet(full_packet, frame_seq=42, instance_id=1024)

    assert gs.instance_id == 1024
    assert gs.frame_seq == 42
    assert gs.hp == 3500
    assert gs.hp_max == 4000
    assert gs.mana == 800
    assert gs.player_pos.x == 100.5
    assert gs.flask_charges == [10, 20, 30, 40, 50, 60]
    assert len(gs.monsters) == 2
    assert gs.monsters[0].entity_id == 1
    assert gs.monsters[0].is_alive is True
    assert gs.monsters[1].is_alive is False
    assert gs.map_mod_hashes == [111, 222, 333]
    assert gs.inventory_mask == 0b10101
    assert gs.is_in_map is True
    assert gs.timestamp_ms == 123456789


def test_bridge_registry_lifecycle() -> None:
    registry = BridgeRegistry()
    assert registry.get(999) is None
    registry.close_all()
