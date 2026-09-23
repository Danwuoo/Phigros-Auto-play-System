"""Touch backend boundary and an independent-contact test double."""

from typing import Callable, Protocol
from .clock import Clock
from .contracts import TouchCommand, TouchReceipt


class TouchBackend(Protocol):
    def inject(self, command: TouchCommand) -> TouchReceipt: ...
    def release_all(self) -> None: ...


class CoordinateTransform:
    """Map a cropped frame rectangle to a touch rectangle after a quarter rotation."""

    def __init__(self, source: tuple[float, float, float, float],
                 target: tuple[float, float, float, float], rotation_deg: int = 0):
        if source[2] <= 0 or source[3] <= 0 or target[2] <= 0 or target[3] <= 0:
            raise ValueError("rectangles require positive width and height")
        if rotation_deg not in (0, 90, 180, 270):
            raise ValueError("rotation must be a quarter turn")
        self.source, self.target, self.rotation_deg = source, target, rotation_deg

    def map(self, x: float, y: float) -> tuple[float, float]:
        sx, sy, sw, sh = self.source
        tx, ty, tw, th = self.target
        u, v = (x - sx) / sw, (y - sy) / sh
        if not (0 <= u <= 1 and 0 <= v <= 1):
            raise ValueError("point outside visible source rectangle")
        if self.rotation_deg == 90:
            u, v = 1 - v, u
        elif self.rotation_deg == 180:
            u, v = 1 - u, 1 - v
        elif self.rotation_deg == 270:
            u, v = v, 1 - u
        return tx + u * tw, ty + v * th


class FakeTouchBackend:
    """Independent contacts and phase validation; effects are supplied by test world."""

    def __init__(self, clock: Clock,
                 on_touch: Callable[[TouchCommand, int], None] | None = None):
        self.clock = clock
        self.on_touch = on_touch
        self.contacts: dict[int, tuple[float, float]] = {}
        self.receipts: list[TouchReceipt] = []

    def inject(self, command: TouchCommand) -> TouchReceipt:
        start_ns = self.clock.now_ns()
        success = True
        reason = ""
        if command.phase == "down":
            if command.contact_id in self.contacts:
                success, reason = False, "contact_already_down"
            else:
                self.contacts[command.contact_id] = (command.x, command.y)
        elif command.phase == "move":
            if command.contact_id not in self.contacts:
                success, reason = False, "contact_not_down"
            else:
                self.contacts[command.contact_id] = (command.x, command.y)
        elif command.phase == "up":
            if command.contact_id not in self.contacts:
                success, reason = False, "contact_not_down"
            else:
                del self.contacts[command.contact_id]
        else:
            success, reason = False, "unknown_phase"
        if success and self.on_touch:
            self.on_touch(command, start_ns)
        receipt = TouchReceipt(command, start_ns, self.clock.now_ns(), success, reason)
        self.receipts.append(receipt)
        return receipt

    def release_all(self) -> None:
        self.contacts.clear()
