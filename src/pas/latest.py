"""Capacity-one frame handoff. No frame queue is maintained."""

import threading
from .contracts import Frame


class LatestFrame:
    def __init__(self) -> None:
        self._condition = threading.Condition()
        self._frame: Frame | None = None
        self._consumed_sequence = -1
        self.published = 0
        self.overwritten = 0

    def publish(self, frame: Frame) -> None:
        with self._condition:
            if self._frame is not None:
                if frame.sequence <= self._frame.sequence:
                    raise ValueError("frame sequences must increase")
                if self._frame.sequence > self._consumed_sequence:
                    self.overwritten += 1
            self._frame = frame
            self.published += 1
            self._condition.notify_all()

    def read_after(self, sequence: int, timeout_s: float | None = None) -> Frame | None:
        with self._condition:
            self._condition.wait_for(
                lambda: self._frame is not None and self._frame.sequence > sequence,
                timeout_s,
            )
            if self._frame is None or self._frame.sequence <= sequence:
                return None
            self._consumed_sequence = max(self._consumed_sequence, self._frame.sequence)
            return self._frame

    def peek(self) -> Frame | None:
        with self._condition:
            return self._frame

    def counters(self) -> dict[str, int]:
        with self._condition:
            return {"published": self.published, "overwritten": self.overwritten}
