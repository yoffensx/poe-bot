"""
internal_bridge.py — Python-сторона чтения GameState из named shared memory.

Поддержка N инстансов: каждый экземпляр InternalBridge привязан к конкретному
instanceId (= PID целевого процесса). Имя shm: "Local\\poe2_gs_<instanceId>".

Зависимостей нет кроме stdlib (mmap, ctypes, struct).
"""

from __future__ import annotations

import ctypes
import mmap
import struct
import threading
import time
from dataclasses import dataclass, field

# ── ctypes layout (должен ТОЧНО совпадать с game_state.h) ────────────────────

SHM_MAGIC = 0x504F4532  # 'POE2'
SHM_VERSION = 1
SHM_SIZE = 65536
MAX_MONSTERS = 128
MAX_MAP_MODS = 32

_HEADER_FMT = "<IIIIBB6x"  # magic, version, instanceId, frameSeq, writerReady, pad
_HEADER_SIZE = struct.calcsize(_HEADER_FMT)  # 24 bytes


@dataclass
class Vec3f:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class MonsterEntry:
    pos: Vec3f = field(default_factory=Vec3f)
    hp: int = 0
    hp_max: int = 0
    entity_id: int = 0
    monster_type: int = 0  # 0=normal, 1=magic, 2=rare, 3=unique
    is_alive: bool = False


@dataclass
class GameState:
    hp: int = 0
    hp_max: int = 1
    mana: int = 0
    mana_max: int = 1
    es: int = 0
    es_max: int = 0
    player_pos: Vec3f = field(default_factory=Vec3f)
    player_angle: float = 0.0
    flask_charges: list[int] = field(default_factory=lambda: [0] * 6)
    monsters: list[MonsterEntry] = field(default_factory=list)
    map_mod_hashes: list[int] = field(default_factory=list)
    inventory_mask: int = 0
    area_hash: int = 0
    is_in_map: bool = False
    timestamp_ms: int = 0
    frame_seq: int = 0
    instance_id: int = 0

    @property
    def hp_pct(self) -> float:
        return self.hp / self.hp_max if self.hp_max else 0.0

    @property
    def mana_pct(self) -> float:
        return self.mana / self.mana_max if self.mana_max else 0.0

    @property
    def alive_monsters(self) -> list[MonsterEntry]:
        return [m for m in self.monsters if m.is_alive]


# Binary layout для GameStatePacket (из game_state.h #pragma pack(1))
# Порядок: hp, hpMax, mana, manaMax, es, esMax, posX, posY, posZ, angle,
#          flasks[6], pad[2], monCount, monsters[128]*..., modCount, mods[32]*uint32,
#          inventoryMask, areaHash, isInMap, pad[3], timestampMs
_VEC3_FMT = "fff"  # 12 bytes
_MONSTER_FMT = f"{_VEC3_FMT}IIIBBxx"  # 12+4+4+4+1+1+2 = 28 bytes
_PACKET_FMT = (
    "IIIIII"  # hp, hpMax, mana, manaMax, es, esMax  (24)
    + _VEC3_FMT
    + "f"  # playerPos (12) + angle (4)           (16)
    + "6Bxx"  # flaskCharges[6] + pad2               (8)
    + "I"  # monsterCount                          (4)
    + f"{_MONSTER_FMT}" * MAX_MONSTERS  # monsters[128]
    + "I"  # mapModCount                           (4)
    + f"{MAX_MAP_MODS}I"  # mapModHashes[32]                      (128)
    + "Q"  # inventoryMask                         (8)
    + "I"  # areaHash                              (4)
    + "Bxxx"  # isInMap + pad3                        (4)
    + "Q"  # timestampMs                           (8)
)
_PACKET_SIZE = struct.calcsize("<" + _PACKET_FMT)
_MONSTER_SIZE = struct.calcsize("<" + _MONSTER_FMT)


def _parse_packet(data: bytes, frame_seq: int, instance_id: int) -> GameState:
    """Разбирает бинарный GameStatePacket в GameState dataclass."""
    s = struct.unpack_from("<" + _PACKET_FMT, data)
    idx = 0

    hp, hp_max, mana, mana_max, es, es_max = s[0:6]
    idx = 6
    px, py, pz, angle = s[6:10]
    idx = 10
    flasks = list(s[10:16])
    idx = 16
    mon_count = s[16]
    idx = 17

    monsters: list[MonsterEntry] = []
    fields_per_monster = len(struct.unpack("<" + _MONSTER_FMT, b"\x00" * _MONSTER_SIZE))
    for i in range(MAX_MONSTERS):
        base = idx + i * fields_per_monster
        mx, my, mz, mhp, mhp_max, mid, mtype, malive = s[base : base + 8]
        if i < mon_count:
            monsters.append(
                MonsterEntry(
                    pos=Vec3f(mx, my, mz),
                    hp=mhp,
                    hp_max=mhp_max,
                    entity_id=mid,
                    monster_type=mtype,
                    is_alive=bool(malive),
                )
            )
    idx += MAX_MONSTERS * fields_per_monster

    mod_count = s[idx]
    idx += 1
    mod_hashes = list(s[idx : idx + MAX_MAP_MODS])
    idx += MAX_MAP_MODS
    inv_mask = s[idx]
    idx += 1
    area_hash = s[idx]
    idx += 1
    is_in_map = bool(s[idx])
    idx += 1
    ts_ms = s[idx]

    return GameState(
        hp=hp,
        hp_max=hp_max,
        mana=mana,
        mana_max=mana_max,
        es=es,
        es_max=es_max,
        player_pos=Vec3f(px, py, pz),
        player_angle=angle,
        flask_charges=flasks,
        monsters=monsters[:mon_count],
        map_mod_hashes=mod_hashes[:mod_count],
        inventory_mask=inv_mask,
        area_hash=area_hash,
        is_in_map=is_in_map,
        timestamp_ms=ts_ms,
        frame_seq=frame_seq,
        instance_id=instance_id,
    )


class InternalBridge:
    """
    Читает GameState из named shared memory, созданной payload DLL.

    Использование:
        bridge = InternalBridge(instance_id=12345)  # PID PoE2
        bridge.open()
        state = bridge.read_state()
        bridge.close()

    Также поддерживает context manager:
        with InternalBridge(12345) as bridge:
            state = bridge.read_state()
    """

    def __init__(self, instance_id: int, poll_hz: int = 30) -> None:
        self._instance_id = instance_id
        self._poll_hz = poll_hz
        self._shm_name = f"Local\\poe2_gs_{instance_id}"
        self._mm: mmap.mmap | None = None
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._h_mapping: ctypes.c_void_p | None = None
        self._buf: ctypes.c_char_p | None = None
        self._ready = False
        self._lock = threading.Lock()

    def open(self, timeout_s: float = 10.0) -> None:
        """Открывает shm. Ждёт до timeout_s секунд пока payload выставит writerReady=1."""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            try:
                self._mm = mmap.mmap(-1, SHM_SIZE, self._shm_name, access=mmap.ACCESS_READ)
                # Проверяем magic
                self._mm.seek(0)
                hdr_data = self._mm.read(_HEADER_SIZE)
                magic, _version, _inst_id, _frame_seq, writer_ready, _ = struct.unpack_from(
                    "<IIIIBB", hdr_data
                )
                if magic == SHM_MAGIC and writer_ready:
                    self._ready = True
                    return
                self._mm.close()
                self._mm = None
            except Exception:
                pass
            time.sleep(0.1)
        raise TimeoutError(
            f"InternalBridge: payload не выставил writerReady за {timeout_s}s "
            f"(shm={self._shm_name})"
        )

    def close(self) -> None:
        if self._mm:
            self._mm.close()
            self._mm = None
        self._ready = False

    def read_state(self) -> GameState:
        """Читает текущий GameState (zero-copy snapshot)."""
        if not self._mm or not self._ready:
            raise RuntimeError("InternalBridge not open")
        with self._lock:
            self._mm.seek(0)
            raw = self._mm.read(SHM_SIZE)

        # Парсим заголовок
        magic, _version, inst_id, frame_seq, _writer_ready, _ = struct.unpack_from("<IIIIBB", raw)
        if magic != SHM_MAGIC:
            raise ValueError(f"Bad SHM magic: 0x{magic:08X}")

        # Парсим пакет (начинается сразу после ShmHeader = 24 bytes)
        packet_data = raw[_HEADER_SIZE:]
        return _parse_packet(packet_data, frame_seq, inst_id)

    @property
    def is_ready(self) -> bool:
        return self._ready

    @property
    def instance_id(self) -> int:
        return self._instance_id

    def __enter__(self) -> InternalBridge:
        self.open()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


# ── Multi-instance registry ───────────────────────────────────────────────────


class BridgeRegistry:
    """Управляет несколькими InternalBridge для N инстансов PoE2."""

    def __init__(self) -> None:
        self._bridges: dict[int, InternalBridge] = {}

    def add(self, instance_id: int, **kwargs: object) -> InternalBridge:
        if instance_id in self._bridges:
            return self._bridges[instance_id]
        b = InternalBridge(instance_id, **kwargs)  # type: ignore[arg-type]
        b.open()
        self._bridges[instance_id] = b
        return b

    def get(self, instance_id: int) -> InternalBridge | None:
        return self._bridges.get(instance_id)

    def read_all(self) -> dict[int, GameState]:
        result = {}
        for iid, bridge in self._bridges.items():
            try:
                result[iid] = bridge.read_state()
            except Exception:
                pass
        return result

    def close_all(self) -> None:
        for b in self._bridges.values():
            b.close()
        self._bridges.clear()
