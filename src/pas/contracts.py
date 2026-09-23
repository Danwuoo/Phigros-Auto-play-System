"""Contracts shared by capture, perception, prediction, scheduling and injection."""

from dataclasses import dataclass
from typing import Literal

Phase = Literal["down", "move", "up"]


@dataclass(frozen=True)
class Frame:
    sequence: int
    width: int
    height: int
    rgb: bytes
    capture_complete_ns: int
    # This is never inferred from capture_complete_ns.
    produced_ns: int | None = None
    pixel_format: Literal["RGB24"] = "RGB24"
    pixels_ready_ns: int | None = None
    published_ns: int | None = None
    source_sequence: int | None = None
    stream_generation: int = 0
    source_timestamp_us: int | None = None
    source_rotation: int | None = None

    def __post_init__(self) -> None:
        if self.width <= 0 or self.height <= 0 or len(self.rgb) != self.width * self.height * 3:
            raise ValueError("invalid RGB frame dimensions")


@dataclass(frozen=True)
class Observation:
    frame_sequence: int
    capture_complete_ns: int
    recognition_complete_ns: int
    x: float
    y: float


@dataclass(frozen=True)
class Track:
    track_id: int
    frame_sequence: int
    capture_complete_ns: int
    recognition_complete_ns: int
    x: float
    y: float
    vx_px_s: float
    vy_px_s: float
    residual_px: float = 0.0


@dataclass(frozen=True)
class HitIntent:
    key: str
    track_id: int
    frame_sequence: int
    x: float
    y: float
    predicted_hit_ns: int
    prediction_basis: str
    confidence: float
    uncertainty_ns: int = 0


@dataclass(frozen=True)
class TouchCommand:
    key: str
    contact_id: int
    phase: Phase
    x: float
    y: float
    scheduled_ns: int
    source_frame_sequence: int


@dataclass(frozen=True)
class TouchReceipt:
    command: TouchCommand
    injection_start_ns: int
    injection_return_ns: int
    success: bool
    reason: str = ""
