"""One host monotonic clock for all timestamp and duration arithmetic."""

import time
from typing import Protocol


class Clock(Protocol):
    def now_ns(self) -> int: ...
    def sleep_until_ns(self, deadline_ns: int) -> None: ...


class HostClock:
    def now_ns(self) -> int:
        return time.monotonic_ns()

    def sleep_until_ns(self, deadline_ns: int) -> None:
        remaining = deadline_ns - self.now_ns()
        if remaining > 0:
            time.sleep(remaining / 1_000_000_000)


class VirtualClock:
    """Deterministic clock for reproducible synthetic experiments only."""

    def __init__(self, initial_ns: int = 0):
        self.current_ns = initial_ns

    def now_ns(self) -> int:
        return self.current_ns

    def sleep_until_ns(self, deadline_ns: int) -> None:
        self.current_ns = max(self.current_ns, deadline_ns)

    def advance_ns(self, duration_ns: int) -> None:
        if duration_ns < 0:
            raise ValueError("cannot reverse monotonic time")
        self.current_ns += duration_ns
