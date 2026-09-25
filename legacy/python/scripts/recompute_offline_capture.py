"""Recompute bounded offline handoff intervals from raw capture JSONL."""

import json
from pathlib import Path
import sys

from pas.telemetry import distribution


def recompute(path):
    start = end = None
    captures = []
    consumed = 0
    for line in Path(path).open(encoding="utf-8"):
        event = json.loads(line)
        if event["event"] == "bench_phase":
            if event["phase"] == "MEASURING":
                start = event["monotonic_ns"]
            elif event["phase"] == "STOPPING":
                end = event["monotonic_ns"]
        elif event["event"] == "capture":
            captures.append(event["capture_complete_ns"])
        elif event["event"] == "frame_consumed":
            consumed += 1
    if start is None or end is None or end <= start:
        raise ValueError("missing complete measurement window")
    selected = sorted(t for t in captures if start <= t < end)
    return {"window_s": (end - start) / 1e9,
            "capture_events": len(selected), "consumer_events": consumed,
            "arrival_interval_ms": distribution([(b - a) / 1e6
                                                  for a, b in zip(selected, selected[1:])])}


if __name__ == "__main__":
    for name in sys.argv[1:]:
        print(json.dumps({"path": name, **recompute(name)}, indent=2))
