"""Structured event log and distribution summaries for offline analysis."""

import json
import math
from collections import deque
from pathlib import Path
from statistics import mean
import threading
import time


class Telemetry:
    def __init__(self, path: str | Path | None = None, *, instrument_write_cost: bool = False):
        self.events: list[dict] = []
        self._retain_events = path is None
        self._lock = threading.Lock()
        self.path = Path(path) if path else None
        if self.path:
            self.path.parent.mkdir(parents=True, exist_ok=True)
        self._file = self.path.open("w", encoding="utf-8") if self.path else None
        self._write_cost_samples = deque(maxlen=100_000) if instrument_write_cost else None

    def record(self, event: str, **fields: object) -> None:
        item = {"event": event, **fields}
        with self._lock:
            if self._retain_events:
                self.events.append(item)
            if self._file:
                start_ns = time.monotonic_ns() if self._write_cost_samples is not None else None
                self._file.write(json.dumps(item, ensure_ascii=False) + "\n")
                if self._write_cost_samples is not None:
                    self._write_cost_samples.append((start_ns, time.monotonic_ns() - start_ns))

    def write_cost_distribution(self, start_ns: int, end_ns: int) -> dict:
        with self._lock:
            if self._write_cost_samples is None:
                return {"n": 0, "instrumentation": "disabled"}
            values = [duration / 1e6 for timestamp, duration in self._write_cost_samples
                      if start_ns <= timestamp < end_ns]
        return {**distribution(values), "instrumentation": "enabled; bounded last 100000 writes"}

    def close(self) -> None:
        with self._lock:
            if self._file:
                self._file.close()
                self._file = None

    def flush(self) -> None:
        with self._lock:
            if self._file:
                self._file.flush()

    def __enter__(self) -> "Telemetry":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


def distribution(values: list[int | float]) -> dict:
    if not values:
        return {"n": 0}
    ordered = sorted(values)

    def percentile(p: float) -> float:
        index = (len(ordered) - 1) * p
        lower = int(index)
        fraction = index - lower
        return ordered[lower] * (1 - fraction) + ordered[min(lower + 1, len(ordered) - 1)] * fraction

    return {
        "n": len(values),
        "min": ordered[0],
        "p5": percentile(0.05),
        "p50": percentile(0.5),
        "p95": percentile(0.95),
        "p99": percentile(0.99),
        "max": ordered[-1],
        "mean": mean(values),
        "jitter_p95_minus_p50": percentile(0.95) - percentile(0.5),
        "jitter_p95_minus_p5": percentile(0.95) - percentile(0.05),
        "absolute_deviation_p95": sorted(abs(value - percentile(0.5)) for value in values)[
            min(len(values) - 1, math.ceil(0.95 * len(values)) - 1)],
    }
