"""Capture-first launch handshake. Game UI classification is a later milestone."""

from typing import Protocol

from .capture import CaptureWorker
from .clock import Clock
from .telemetry import Telemetry


class Launcher(Protocol):
    def launch(self, package: str) -> tuple[int, int]: ...


class SessionController:
    def __init__(self, worker: CaptureWorker, launcher: Launcher,
                 clock: Clock, telemetry: Telemetry):
        self.worker, self.launcher, self.clock, self.telemetry = worker, launcher, clock, telemetry
        self.state = "DISCONNECTED"

    def _transition(self, state: str, reason: str, frame_sequence: int | None = None) -> None:
        self.telemetry.record("session_transition", from_state=self.state, to_state=state,
                              reason=reason, source_frame_sequence=frame_sequence,
                              monotonic_ns=self.clock.now_ns())
        self.state = state

    def start(self, package: str, readiness_frames: int = 3,
              readiness_timeout_s: float = 10) -> None:
        if self.state != "DISCONNECTED":
            raise RuntimeError("session already started")
        self._transition("CAPTURING", "capture_start")
        self.worker.start()
        try:
            sequence = self.worker.wait_for_valid_frames(readiness_frames, readiness_timeout_s)
            self._transition("LAUNCHING", "capture_ready", sequence)
            start_ns, return_ns = self.launcher.launch(package)
            self.telemetry.record("app_launch", package=package,
                                  call_start_ns=start_ns, call_return_ns=return_ns)
            self._transition("NAVIGATING", "launch_command_returned", sequence)
        except Exception as error:
            self._transition("ERROR", str(error))
            self.worker.stop()
            raise

    def stop(self) -> None:
        self.worker.stop()
        self._transition("STOPPED", "user_stop")

    def monitor_for(self, duration_s: float, max_frame_age_s: float = 1) -> None:
        if self.state != "NAVIGATING":
            raise RuntimeError("session is not navigating")
        if duration_s <= 0 or max_frame_age_s <= 0:
            raise ValueError("invalid monitor duration")
        deadline_ns = self.clock.now_ns() + round(duration_s * 1e9)
        while self.clock.now_ns() < deadline_ns:
            frame = self.worker.latest.peek()
            if self.worker.error is not None or frame is None or self.clock.now_ns() - frame.capture_complete_ns > round(max_frame_age_s * 1e9):
                self._transition("ERROR", "capture_disconnected_or_stale",
                                 frame.sequence if frame else None)
                raise RuntimeError("capture disconnected or stale")
            self.clock.sleep_until_ns(min(deadline_ns, self.clock.now_ns() + 100_000_000))
