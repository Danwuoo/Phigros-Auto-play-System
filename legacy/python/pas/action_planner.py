"""Game independent contact trajectories. Inputs are live-pixel-derived intents or Fixture tests."""

from dataclasses import dataclass
import math


@dataclass(frozen=True)
class ContactStep:
    phase: str
    x: float
    y: float
    due_ns: int


@dataclass(frozen=True)
class ContactPlan:
    key: str
    revision: int
    epoch: int
    evidence_ns: int
    valid_until_ns: int
    steps: tuple[ContactStep, ...]
    basis: str
    source_frame_sequence: int = -1

    def __post_init__(self):
        if not self.key or self.revision < 0 or self.epoch < 0 or not self.basis:
            raise ValueError("invalid plan identity or basis")
        if not self.steps or len(self.steps) < 2 or self.steps[0].phase != "down" or self.steps[-1].phase != "up":
            raise ValueError("plan must open and close a contact")
        if self.valid_until_ns < self.steps[0].due_ns or self.evidence_ns > self.steps[0].due_ns:
            raise ValueError("invalid evidence or plan validity")
        previous = -1
        for i, step in enumerate(self.steps):
            if step.phase != ("down" if i == 0 else "up" if i == len(self.steps) - 1 else "move"):
                raise ValueError("invalid contact trajectory")
            if step.due_ns < previous or not math.isfinite(step.x) or not math.isfinite(step.y):
                raise ValueError("invalid step time or position")
            previous = step.due_ns


def tap(key: str, revision: int, epoch: int, evidence_ns: int, down_ns: int,
        x: float, y: float, *, duration_ns: int = 20_000_000,
        validity_ns: int = 30_000_000, frame_sequence: int = -1) -> ContactPlan:
    if duration_ns <= 0 or validity_ns < 0:
        raise ValueError("invalid tap duration/validity")
    return ContactPlan(key, revision, epoch, evidence_ns, down_ns + validity_ns,
                       (ContactStep("down", x, y, down_ns),
                        ContactStep("up", x, y, down_ns + duration_ns)),
                       "tap_position_from_current_pixels", frame_sequence)


def trajectory(key: str, revision: int, epoch: int, evidence_ns: int,
               points: tuple[tuple[int, float, float], ...], *,
               validity_ns: int = 30_000_000, basis: str,
               frame_sequence: int = -1) -> ContactPlan:
    if len(points) < 2 or validity_ns < 0:
        raise ValueError("trajectory needs down and up")
    steps = tuple(ContactStep("down" if i == 0 else "up" if i == len(points)-1 else "move",
                              x, y, when) for i, (when, x, y) in enumerate(points))
    return ContactPlan(key, revision, epoch, evidence_ns, steps[0].due_ns + validity_ns,
                       steps, basis, frame_sequence)
