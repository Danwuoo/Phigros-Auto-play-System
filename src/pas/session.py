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
        self.frame_fresh = False
        self._inactive_since_ns: int | None = None
        self._inactive_frame_sequence: int | None = None

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
            self.frame_fresh = True
        except Exception as error:
            self._transition("ERROR", str(error))
            self.worker.stop()
            raise

    def stop(self) -> None:
        self.worker.stop()
        self.frame_fresh = False
        self._transition("STOPPED", "user_stop")

    def monitor_for(self, duration_s: float, max_frame_age_s: float = 1) -> None:
        if self.state not in ("NAVIGATING", "DEGRADED"):
            raise RuntimeError("session is not navigating")
        if duration_s <= 0 or max_frame_age_s <= 0:
            raise ValueError("invalid monitor duration")
        deadline_ns = self.clock.now_ns() + round(duration_s * 1e9)
        last_probe_ns = -1
        while self.clock.now_ns() < deadline_ns:
            frame = self.worker.latest.peek()
            stale = (frame is not None and self.clock.now_ns() - frame.capture_complete_ns > round(max_frame_age_s * 1e9))
            inactive = getattr(self.worker.source, "inactive", False)
            if self.worker.error is not None or frame is None:
                reason = "capture_disconnected"
                self.frame_fresh = False
                self._transition("ERROR", reason,
                                 frame.sequence if frame else None)
                raise RuntimeError("capture disconnected or stale")
            if inactive:
                self.frame_fresh = False
                if self._inactive_since_ns is None:
                    self._inactive_since_ns = self.clock.now_ns()
                    self._inactive_frame_sequence = frame.sequence
                    self._transition("DEGRADED", "capture_inactive", frame.sequence)
                if self.clock.now_ns() - self._inactive_since_ns >= round(max_frame_age_s * 1e9):
                    self._transition("ERROR", "capture_inactive_timeout", frame.sequence)
                    raise RuntimeError("capture inactive beyond deadline")
                self.clock.sleep_until_ns(min(deadline_ns, self.clock.now_ns() + 100_000_000))
                continue
            self._inactive_since_ns = None
            if self._inactive_frame_sequence is not None:
                if frame.sequence <= self._inactive_frame_sequence:
                    self.frame_fresh = False
                    self.clock.sleep_until_ns(min(deadline_ns, self.clock.now_ns() + 100_000_000))
                    continue
                self._inactive_frame_sequence = None
            if stale:
                self.frame_fresh = False
                probe = getattr(self.worker.source, "probe_health", None)
                if (getattr(self.worker.source, "event_driven", False) and probe and
                        (last_probe_ns < 0 or self.clock.now_ns() - last_probe_ns >= round(max_frame_age_s * 1e9))):
                    result = probe(frame)
                    last_probe_ns = self.clock.now_ns()
                    self.telemetry.record("capture_health_probe", result=result,
                                          source_frame_sequence=frame.sequence,
                                          monotonic_ns=last_probe_ns)
                    if result == "static":
                        if self.state != "DEGRADED":
                            self._transition("DEGRADED", "static_pixels_stream_unconfirmed", frame.sequence)
                    else:
                        self._transition("ERROR", "stream_stalled_or_probe_failed", frame.sequence)
                        raise RuntimeError("capture stream stalled or health probe failed")
                elif not getattr(self.worker.source, "event_driven", False) or not probe:
                    self._transition("ERROR", "capture_stale_unverifiable", frame.sequence)
                    raise RuntimeError("capture stale and unverifiable")
            else:
                self.frame_fresh = True
                if self.state == "DEGRADED":
                    self._transition("NAVIGATING", "new_valid_frame", frame.sequence)
            self.clock.sleep_until_ns(min(deadline_ns, self.clock.now_ns() +
                                          min(100_000_000, round(max_frame_age_s * 1e9 / 2))))
        frame = self.worker.latest.peek()
        if (self.worker.error is not None or frame is None or
                self.clock.now_ns() - frame.capture_complete_ns > round(max_frame_age_s * 1e9)):
            self.frame_fresh = False
            if self.state == "NAVIGATING":
                self._transition("DEGRADED", "monitor_end_unconfirmed", frame.sequence if frame else None)
