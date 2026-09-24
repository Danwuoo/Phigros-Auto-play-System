"""Replaceable producer that publishes only the newest decoded frame."""

import threading
from dataclasses import replace
from typing import Protocol

from .clock import Clock
from .contracts import Frame
from .latest import LatestFrame
from .telemetry import Telemetry


class CaptureSource(Protocol):
    def capture(self) -> tuple[Frame, int]: ...


class CaptureWorker:
    def __init__(self, source: CaptureSource, clock: Clock, telemetry: Telemetry,
                 on_frame=None):
        self.source, self.clock, self.telemetry = source, clock, telemetry
        self.latest = LatestFrame()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.error: Exception | None = None
        self.on_frame = on_frame

    def start(self) -> None:
        if self._thread is not None:
            raise RuntimeError("capture worker already started")
        self._thread = threading.Thread(target=self._run, name="capture")
        self._thread.start()

    def _run(self) -> None:
        try:
            while not self._stop.is_set():
                frame, start_ns = self.source.capture()
                frame = replace(frame, pixels_ready_ns=frame.pixels_ready_ns or self.clock.now_ns(),
                                published_ns=self.clock.now_ns())
                overwritten_before = self.latest.counters()["overwritten"]
                self.latest.publish(frame)
                if self.latest.counters()["overwritten"] > overwritten_before:
                    self.telemetry.record("capture_overwrite", frame_sequence=frame.sequence,
                                          monotonic_ns=frame.published_ns)
                if self.on_frame:
                    self.on_frame(frame)
                self.telemetry.record("capture", frame_sequence=frame.sequence,
                                      capture_wait_start_ns=start_ns,
                                      capture_start_ns=start_ns,
                                      capture_complete_ns=frame.capture_complete_ns,
                                      pixels_ready_ns=frame.pixels_ready_ns,
                                      decode_complete_ns=frame.pixels_ready_ns,
                                      published_ns=frame.published_ns,
                                      produced_ns=frame.produced_ns,
                                      source_sequence=frame.source_sequence,
                                      stream_generation=frame.stream_generation,
                                      source_timestamp_us=frame.source_timestamp_us,
                                      source_rotation=frame.source_rotation,
                                      width=frame.width, height=frame.height,
                                      pixel_format=frame.pixel_format)
        except Exception as error:
            if not self._stop.is_set():
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
        close = getattr(self.source, "close", None)
        close_error = None
        if close:
            try:
                close()
            except Exception as error:
                close_error = error
        if self._thread:
            self._thread.join(join_timeout_s)
            if self._thread.is_alive():
                raise TimeoutError("capture worker did not stop within timeout")
        if close_error is not None:
            raise RuntimeError(f"capture source cleanup failed: {close_error}") from close_error
