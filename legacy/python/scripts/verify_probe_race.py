"""Verify Session health recheck against a live AVD pixel stream."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

from pas.capture import CaptureWorker
from pas.capture_grpc import EmulatorGrpcCapture
from pas.adb import find_adb
from pas.clock import HostClock
from pas.session import SessionController
from pas.telemetry import Telemetry


class NoopLauncher:
    def launch(self, package: str) -> tuple[int, int]:
        now = time.monotonic_ns()
        return now, now


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    if args.log.exists():
        parser.error("log already exists")
    adb = find_adb()
    if not adb:
        raise RuntimeError("adb unavailable")
    activities = subprocess.run([adb, "-s", args.serial, "shell", "dumpsys", "activity", "activities"],
                                capture_output=True, text=True, check=True, timeout=10).stdout
    if not any("topResumedActivity=" in line and "org.pas.capturefixture/.MainActivity" in line
               for line in activities.splitlines()):
        raise RuntimeError("fixture must be foreground")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    clock = HostClock()
    with Telemetry(args.log) as log:
        head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                              text=True, check=True).stdout.strip()
        log.record("run_config", serial=args.serial, command_argv=sys.argv,
                   git_head=head, clock="host_monotonic_ns",
                   probe_result="controlled changed after live newer frame")
        source = EmulatorGrpcCapture(args.serial, clock=clock, telemetry=log)
        worker = CaptureWorker(source, clock, log)
        session = SessionController(worker, NoopLauncher(), clock, log)
        probe_seen = threading.Event()
        recovered_event = threading.Event()
        failures: list[str] = []
        original_transition = session._transition

        def watched_transition(state, reason, frame_sequence=None):
            original_transition(state, reason, frame_sequence)
            if reason == "new_valid_frame_during_probe":
                recovered_event.set()

        session._transition = watched_transition

        def controlled_probe(last_frame):
            probe_seen.set()
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                newest = worker.latest.peek()
                if newest is not None and newest.sequence > last_frame.sequence:
                    return "changed"
                time.sleep(0.001)
            return "failed"

        source.probe_health = controlled_probe
        session.start("fixture-only", readiness_frames=1)

        def monitor():
            try:
                session.monitor_for(0.5, max_frame_age_s=0.012)
            except Exception as error:
                failures.append(str(error))

        thread = threading.Thread(target=monitor)
        try:
            thread.start()
            if not probe_seen.wait(2):
                raise TimeoutError("health probe never started")
            recovered = recovered_event.wait(2)
            summary = {"recovered_after_new_stream_frame": recovered,
                       "monitor_failures_before_stop": failures.copy(),
                       "last_frame_sequence": worker.latest.peek().sequence,
                       "state_before_stop": session.state}
        finally:
            session.stop()
            thread.join(2)
        summary["state_after_stop"] = session.state
        summary["monitor_failures"] = failures
        log.record("summary", **summary)
    print(json.dumps(summary, indent=2))
    if not summary["recovered_after_new_stream_frame"] or summary["monitor_failures"]:
        raise RuntimeError("live stream did not recover during health probe")


if __name__ == "__main__":
    main()
