"""Replaceable producer that publishes only the newest decoded frame."""

import threading
from typing import Protocol

from .clock import Clock
from .contracts import Frame
from .latest import LatestFrame
from .telemetry import Telemetry


class CaptureSource(Protocol):
    def capture(self) -> tuple[Frame, int]: ...


class CaptureWorker:
    def __init__(self, source: CaptureSource, clock: Clock, telemetry: Telemetry):
        self.source, self.clock, self.telemetry = source, clock, telemetry
        self.latest = LatestFrame()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.error: Exception | None = None

    def start(self) -> None:
        if self._thread is not None:
            raise RuntimeError("capture worker already started")
        self._thread = threading.Thread(target=self._run, name="capture", daemon=True)
        self._thread.start()

    def _run(self) -> None:
        try:
            while not self._stop.is_set():
                frame, start_ns = self.source.capture()
                decoded_ns = self.clock.now_ns()
                self.latest.publish(frame)
                self.telemetry.record("capture", frame_sequence=frame.sequence,
                                      capture_start_ns=start_ns,
                                      capture_complete_ns=frame.capture_complete_ns,
                                      decode_complete_ns=decoded_ns,
                                      produced_ns=frame.produced_ns,
                                      width=frame.width, height=frame.height,
                                      pixel_format=frame.pixel_format)
        except Exception as error:
            self.error = error
            self.telemetry.record("capture_error", reason=str(error),
                                  monotonic_ns=self.clock.now_ns())

    def wait_for_valid_frames(self, count: int, timeout_s: float) -> int:
        if count < 1:
            raise ValueError("count must be positive")
        deadline_ns = self.clock.now_ns() + round(timeout_s * 1e9)
        sequence = -1
        seen = 0
        while seen < count:
            if self.error:
                raise RuntimeError(f"capture failed: {self.error}")
            remaining_ns = deadline_ns - self.clock.now_ns()
            if remaining_ns <= 0:
                raise TimeoutError("capture readiness timed out")
            frame = self.latest.read_after(sequence, min(remaining_ns / 1e9, 0.1))
            if frame is not None:
                sequence = frame.sequence
                seen += 1
        return sequence

    def stop(self, join_timeout_s: float = 16) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(join_timeout_s)
            if self._thread.is_alive():
                raise TimeoutError("capture worker did not stop within timeout")
