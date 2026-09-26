"""
capture_service.py — Unified CaptureService поверх Internal и External режимов.

В режиме internal: получает GameState из InternalBridge (shm).
В режиме external: получает GameState из DXGI capture + OCR (заглушка до реализации).

Режим определяется из BotConfig.bot.mode ("internal" | "external").
"""

from __future__ import annotations

import subprocess
from pathlib import Path

from .internal_bridge import BridgeRegistry, GameState, InternalBridge

# ── Unified GameState уже определён в internal_bridge.py ─────────────────────
# Реэкспортируем для единой точки импорта
__all__ = ["CaptureService", "GameState"]


class ExternalCaptureStub:
    """
    Заглушка External capture (DXGI + OCR).
    Будет реализована в задаче 005-vision-minimap-hp-flasks.
    """

    def read_state(self) -> GameState:
        raise NotImplementedError("External capture not yet implemented")

    def open(self) -> None:
        pass

    def close(self) -> None:
        pass


class CaptureService:
    """
    Единый интерфейс получения GameState независимо от режима.

    Internal mode (приоритет):
        - Запускает loader для инжекта payload в PoE2
        - Открывает InternalBridge (shm reader)
        - read_state() → мгновенно из shm (<1ms)

    External mode:
        - Запускает DXGI capture + OCR pipeline
        - read_state() → разобранный OCR/CV результат

    Использование:
        svc = CaptureService(mode="internal", poe2_pid=12345,
                             loader_path=Path("cpp/build/poe2_loader.exe"),
                             instance_id=0)
        svc.open()
        state = svc.read_state()
        svc.close()
    """

    def __init__(
        self,
        mode: str = "internal",
        poe2_pid: int = 0,
        loader_path: Path | None = None,
        payload_path: Path | None = None,
        instance_id: int = 0,
    ) -> None:
        self._mode = mode
        self._poe2_pid = poe2_pid
        self._loader_path = loader_path or Path("cpp/build/Release/poe2_loader.exe")
        self._payload_path = payload_path or Path("cpp/build/Release/poe2_payload.dll")
        self._instance_id = instance_id if instance_id != 0 else poe2_pid
        self._bridge: InternalBridge | None = None
        self._external: ExternalCaptureStub | None = None
        self._registry = BridgeRegistry()

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def open(self, inject: bool = True, timeout_s: float = 15.0) -> None:
        """
        Открывает capture pipeline.

        inject=True → запускает loader.exe для инжекта (нужны права, один раз).
        inject=False → предполагает, что payload уже запущен (повторное подключение).
        """
        if self._mode == "internal":
            if inject:
                self._run_loader()
            self._bridge = self._registry.add(self._instance_id, timeout_s=timeout_s)
        else:
            self._external = ExternalCaptureStub()
            self._external.open()

    def close(self) -> None:
        self._registry.close_all()
        if self._external:
            self._external.close()

    # ── Core API ──────────────────────────────────────────────────────────────

    def read_state(self) -> GameState:
        """Возвращает свежий GameState. Thread-safe."""
        if self._mode == "internal":
            if not self._bridge:
                raise RuntimeError("CaptureService not open")
            return self._bridge.read_state()
        else:
            if not self._external:
                raise RuntimeError("CaptureService not open")
            return self._external.read_state()

    @property
    def mode(self) -> str:
        return self._mode

    @property
    def is_internal(self) -> bool:
        return self._mode == "internal"

    # ── Loader invocation ─────────────────────────────────────────────────────

    def _run_loader(self) -> None:
        """Запускает poe2_loader.exe <pid> <instance_id> и ждёт завершения."""
        if not self._loader_path.exists():
            raise FileNotFoundError(
                f"Loader not found: {self._loader_path}\n"
                "Соберите C++ проект: cmake --build cpp/build --config Release"
            )
        args = [
            str(self._loader_path),
            str(self._poe2_pid),
            str(self._instance_id),
        ]
        result = subprocess.run(args, capture_output=True, text=True, timeout=30)
        if result.returncode not in (0,):
            raise RuntimeError(
                f"Loader failed (code={result.returncode}):\n{result.stdout}\n{result.stderr}"
            )

    # ── Context manager ───────────────────────────────────────────────────────

    def __enter__(self) -> CaptureService:
        self.open()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


# ── Multi-instance factory ────────────────────────────────────────────────────


def make_services_for_pids(
    pids: list[int],
    loader_path: Path | None = None,
    inject: bool = True,
) -> list[CaptureService]:
    """
    Создаёт CaptureService для каждого PID PoE2.
    Каждый инстанс получает уникальный instanceId = PID.

    Пример для 4 окон PoE2:
        pids = [1234, 5678, 9012, 3456]
        services = make_services_for_pids(pids)
        for svc in services:
            state = svc.read_state()
    """
    services = []
    for pid in pids:
        svc = CaptureService(
            mode="internal",
            poe2_pid=pid,
            loader_path=loader_path,
            instance_id=pid,
        )
        svc.open(inject=inject)
        services.append(svc)
    return services
