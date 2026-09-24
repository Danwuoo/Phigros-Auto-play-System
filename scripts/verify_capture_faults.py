"""Reversible AVD fixture-only capture fault checks; never touches game data."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from pas.adb import find_adb
from pas.capture import CaptureWorker
from pas.capture_grpc import EmulatorGrpcCapture
from pas.clock import HostClock
from pas.telemetry import Telemetry


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--screen-off-s", type=float, default=2)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    if args.screen_off_s <= 0 or args.log.exists():
        parser.error("positive screen-off duration and new log path required")
    adb = find_adb()
    if not adb:
        raise RuntimeError("adb unavailable")

    def adb_output(*commands: str) -> str:
        return subprocess.run([adb, "-s", args.serial, *commands], check=True,
                              capture_output=True, text=True, timeout=10).stdout

    def awake() -> bool:
        return "mWakefulness=Awake" in adb_output("shell", "dumpsys", "power")

    def fixture_foreground() -> bool:
        lines = adb_output("shell", "dumpsys", "activity", "activities").splitlines()
        return any("topResumedActivity=" in line and
                   "org.pas.capturefixture/.MainActivity" in line for line in lines)

    if not fixture_foreground() or not awake():
        raise RuntimeError("fixture must be foreground and display awake before fault check")
    clock = HostClock()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with Telemetry(args.log) as log:
        head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                              text=True, check=True).stdout.strip()
        log.record("run_config", serial=args.serial, screen_off_s=args.screen_off_s,
                   command_argv=sys.argv, git_head=head,
                   clock="host_monotonic_ns", fixture_only=True)
        source = EmulatorGrpcCapture(args.serial, clock=clock, telemetry=log)
        worker = CaptureWorker(source, clock, log)
        worker.start()
        try:
            first = worker.latest.read_after(-1, 5)
            if first is None:
                raise TimeoutError("no fixture frame")
            if (first.width, first.height) != (1280, 720):
                raise RuntimeError("expected calibrated 1280x720 fixture")
            before_seq = first.sequence
            off_ns = clock.now_ns()
            try:
                adb_output("shell", "input", "keyevent", "26")
                time.sleep(args.screen_off_s)
                off_awake = awake()
                inactive_off = source.counters()["inactive_frames"]
                error_off = str(worker.error) if worker.error else None
                last_off = worker.latest.peek()
            finally:
                if not awake():
                    adb_output("shell", "input", "keyevent", "26")
                adb_output("shell", "am", "start", "-n", "org.pas.capturefixture/.MainActivity")
            restored = worker.latest.read_after(last_off.sequence if last_off else before_seq, 3)
            on_awake = awake()
            log.record("screen_power_check", off_ns=off_ns, off_awake=off_awake,
                       inactive_frames_while_off=inactive_off, error_while_off=error_off,
                       last_off_sequence=last_off.sequence if last_off else None,
                       restored_frame_sequence=restored.sequence if restored else None,
                       on_awake=on_awake, monotonic_ns=clock.now_ns())
        finally:
            worker.stop(join_timeout_s=2)

        disconnect_source = EmulatorGrpcCapture(args.serial, clock=clock, telemetry=log)
        disconnect_worker = CaptureWorker(disconnect_source, clock, log)
        disconnect_worker.start()
        try:
            disconnect_worker.wait_for_valid_frames(1, 5)
            disconnect_ns = clock.now_ns()
            disconnect_source.close()  # Client-side authenticated gRPC transport loss.
            deadline_ns = disconnect_ns + 2_000_000_000
            while disconnect_worker.error is None and clock.now_ns() < deadline_ns:
                time.sleep(0.005)
            disconnect_error = str(disconnect_worker.error) if disconnect_worker.error else None
        finally:
            disconnect_worker.stop(join_timeout_s=2)
        reconnect_source = EmulatorGrpcCapture(args.serial, clock=clock, telemetry=log)
        try:
            reconnect_frame, _ = reconnect_source.capture()
        finally:
            reconnect_source.close()
        log.record("transport_recovery_check", disconnect_ns=disconnect_ns,
                   disconnect_error=disconnect_error,
                   reconnect_dimensions=[reconnect_frame.width, reconnect_frame.height],
                   reconnect_source_sequence=reconnect_frame.source_sequence,
                   monotonic_ns=clock.now_ns())
        summary = {"serial": args.serial, "fixture_foreground_restored": fixture_foreground(),
                   "screen_awake_restored": awake(),
                   "inactive_frames_while_off": inactive_off,
                   "screen_off_caused_stream_error": error_off,
                   "new_frame_after_screen_on": restored is not None,
                   "client_disconnect_error": disconnect_error,
                   "new_frame_after_reconnect": reconnect_frame is not None}
        log.record("summary", **summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
