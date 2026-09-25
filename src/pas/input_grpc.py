"""Emulator sendTouch backend. RPC success is transport acknowledgement, not visible effect."""

from dataclasses import dataclass
import math
import threading
import time

from .capture_grpc import GrpcEndpoint, discover_endpoint
from .contracts import TouchCommand, TouchReceipt


@dataclass(frozen=True)
class ReleaseReport:
    requested_ids: tuple[int, ...]
    failed_ids: tuple[int, ...]
    unknown_ids: tuple[int, ...]
    start_ns: int
    return_ns: int
    effect_unverified_ids: tuple[int, ...] = ()


class EmulatorGrpcTouch:
    def __init__(self, serial: str, width: int, height: int, *, endpoint: GrpcEndpoint | None = None,
                 timeout_s: float = 0.5, max_contacts: int = 2, rpc=None, coordinate_map=None):
        if width < 2 or height < 2 or not 0 < timeout_s <= 2 or not 1 <= max_contacts <= 10:
            raise ValueError("invalid touch configuration")
        self.width, self.height, self.timeout_s, self.max_contacts = width, height, timeout_s, max_contacts
        self.coordinate_map = coordinate_map
        self._positions: dict[int, tuple[int, int]] = {}
        self._unknown: set[int] = set()
        self._owner: int | None = None
        self._closed = False
        self._poisoned = False
        self._channel = None
        if rpc is None:
            try:
                import grpc
                from google.protobuf.empty_pb2 import Empty
                from .emulator_proto import emulator_controller_pb2 as pb
            except ImportError as error:
                raise RuntimeError("touch requires pip install '.[emulator-grpc]'") from error
            selected = endpoint or discover_endpoint(serial)
            if not selected.target.startswith(("127.0.0.1:", "localhost:")) or not selected.token:
                raise ValueError("authenticated loopback endpoint required")
            self._channel = grpc.insecure_channel(selected.target)
            rpc = self._channel.unary_unary(
                "/android.emulation.control.EmulatorController/sendTouch",
                request_serializer=lambda message: message.SerializeToString(),
                response_deserializer=Empty.FromString)
            self._metadata = (("authorization", f"Bearer {selected.token}"),)
        else:
            from .emulator_proto import emulator_controller_pb2 as pb
            self._metadata = ()
        self._pb = pb
        self._rpc = rpc

    def _check_owner(self):
        ident = threading.get_ident()
        if self._owner is None:
            self._owner = ident
        if self._owner != ident:
            raise RuntimeError("touch backend accessed from multiple owner threads")
        if self._closed:
            raise RuntimeError("touch backend closed")

    def _coordinates(self, command: TouchCommand) -> tuple[int, int]:
        if not all(math.isfinite(v) for v in (command.x, command.y)):
            raise ValueError("nonfinite touch coordinate")
        x, y = (self.coordinate_map.map(command.x, command.y) if self.coordinate_map
                else (round(command.x), round(command.y)))
        if not 0 <= x < self.width or not 0 <= y < self.height:
            raise ValueError("touch coordinate outside display")
        return x, y

    def _send(self, changes: list[tuple[int, int, int, int]]) -> tuple[int, int, bool, str]:
        event = self._pb.TouchEvent(touches=[self._pb.Touch(x=x, y=y, identifier=identifier,
                                                         pressure=pressure)
                                             for identifier, x, y, pressure in changes])
        started = time.monotonic_ns()
        try:
            self._rpc(event, timeout=self.timeout_s, metadata=self._metadata)
            return started, time.monotonic_ns(), True, "rpc_returned_effect_unverified"
        except Exception as error:
            # A timed-out/failed RPC can already have reached the emulator.
            self._poisoned = True
            return started, time.monotonic_ns(), False, f"rpc_unknown:{type(error).__name__}"

    def inject_batch(self, commands: tuple[TouchCommand, ...]) -> tuple[TouchReceipt, ...]:
        self._check_owner()
        if self._poisoned:
            raise RuntimeError("touch backend faulted; verify release on Fixture before a new backend")
        if not commands:
            return ()
        if self._unknown:
            raise RuntimeError("contact state unknown; release_all and verify fixture before reuse")
        positions = dict(self._positions)
        changes = []
        used: set[int] = set()
        for command in commands:
            identifier = command.contact_id
            if not 0 <= identifier < self.max_contacts or identifier in self._unknown or identifier in used:
                raise ValueError("contact unavailable or duplicated in batch")
            used.add(identifier)
            x, y = self._coordinates(command)
            if command.phase == "down":
                if identifier in positions or len(positions) >= self.max_contacts:
                    raise ValueError("contact already down or capacity reached")
                positions[identifier] = (x, y)
                pressure = 1
            elif command.phase == "move":
                if identifier not in positions:
                    raise ValueError("move without down")
                positions[identifier] = (x, y)
                pressure = 1
            elif command.phase == "up":
                if identifier not in positions:
                    raise ValueError("up without down")
                positions.pop(identifier)
                pressure = 0
            else:
                raise ValueError("invalid touch phase")
            changes.append((identifier, x, y, pressure))
        start, end, success, reason = self._send(changes)
        if success:
            self._positions = positions
        else:
            self._unknown.update(used)
        return tuple(TouchReceipt(command, start, end, success, reason) for command in commands)

    def inject(self, command: TouchCommand) -> TouchReceipt:
        return self.inject_batch((command,))[0]

    def release_all(self) -> ReleaseReport:
        self._check_owner()
        start = time.monotonic_ns()
        ids = tuple(sorted(set(self._positions) | self._unknown))
        failed = []
        for identifier in ids:
            x, y = self._positions.get(identifier, (self.width // 2, self.height // 2))
            _, _, success, _ = self._send([(identifier, x, y, 0)])
            if success:
                self._positions.pop(identifier, None)
                self._unknown.discard(identifier)
            else:
                failed.append(identifier)
                self._unknown.add(identifier)
        return ReleaseReport(ids, tuple(failed), tuple(sorted(self._unknown)),
                             start, time.monotonic_ns(), ids)

    def emergency_release_all(self) -> ReleaseReport:
        """Best-effort cleanup when a previous owner died with unknown IDs.

        A new channel has no knowledge of Android's state. The caller must
        verify visible zero contacts before opening a new control session.
        """
        self._check_owner()
        self._poisoned = True
        start = time.monotonic_ns()
        ids = tuple(range(self.max_contacts))
        failed = []
        for identifier in ids:
            _, _, success, _ = self._send([(identifier, self.width//2, self.height//2, 0)])
            if success:
                self._positions.pop(identifier, None)
                self._unknown.discard(identifier)
            else:
                failed.append(identifier)
                self._unknown.add(identifier)
        return ReleaseReport(ids, tuple(failed), tuple(sorted(self._unknown)),
                             start, time.monotonic_ns(), ids)

    @property
    def active_ids(self) -> tuple[int, ...]:
        return tuple(sorted(set(self._positions) | self._unknown))

    def close(self) -> None:
        self._check_owner()
        unresolved = self.active_ids
        self._closed = True
        if self._channel is not None:
            self._channel.close()
        if unresolved:
            raise RuntimeError(f"touch close with unverified active contacts: {unresolved}")
