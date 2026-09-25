"""Compare *differences* in source Unix and host monotonic times around a pause.

No absolute source-to-host clock subtraction is performed.
"""

import argparse
import json
from pathlib import Path


def analyze(path: Path) -> dict:
    pause = None
    before = None
    after = []
    for line in path.open(encoding="utf-8"):
        event = json.loads(line)
        if event["event"] == "receiver_pause":
            pause = event
        elif event["event"] == "capture" and event.get("source_timestamp_us"):
            if pause is None:
                before = event
            elif event["capture_complete_ns"] >= pause["start_ns"] and len(after) < 20:
                after.append(event)
    if pause is None or before is None or not after:
        raise ValueError("log lacks a receiver pause with source timestamps on both sides")
    rows = []
    for event in after:
        host_delta_ms = (event["capture_complete_ns"] - before["capture_complete_ns"]) / 1e6
        source_delta_ms = (event["source_timestamp_us"] - before["source_timestamp_us"]) / 1e3
        rows.append({"source_sequence": event["source_sequence"],
                     "host_delta_ms": round(host_delta_ms, 3),
                     "source_delta_ms": round(source_delta_ms, 3),
                     "delta_difference_ms": round(host_delta_ms - source_delta_ms, 3)})
    return {"pause_ms": pause["duration_ms"],
            "anchor_source_sequence": before["source_sequence"],
            "first_twenty_after_pause": rows}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.log), indent=2))
