"""Deterministic pixels-to-touch-to-pixels test world and runner.

The runner decides from Frame.rgb. World target times are used only by the
test fixture and the offline summary, never by detector/tracker/predictor.
"""

from dataclasses import dataclass
from pathlib import Path

from .clock import VirtualClock
from .contracts import Frame, TouchCommand
from .input import FakeTouchBackend
from .latest import LatestFrame
from .scheduler import Scheduler
from .telemetry import Telemetry, distribution
from .vision import GreenTargetDetector, LineCrossingPredictor, VelocityTracker


@dataclass
class TargetTruth:
    index: int
    appearance_ns: int
    crossing_ns: int
    touch_ns: int | None = None
    effect_seen_ns: int | None = None
    first_observed_ns: int | None = None


class TargetWorld:
    width = 64
    height = 64
    line_y = 48
    target_x = 32
    period_ns = 1_000_000_000
    flight_ns = 500_000_000
    hit_window_ns = 25_000_000

    def __init__(self, count: int):
        self.targets = [TargetTruth(i, i * self.period_ns,
                                    i * self.period_ns + self.flight_ns)
                        for i in range(count)]
        self.touch_attempts = 0

    def on_touch(self, command: TouchCommand, now_ns: int) -> None:
        if command.phase != "down":
            return
        self.touch_attempts += 1
        index = now_ns // self.period_ns
        if index >= len(self.targets):
            return
        target = self.targets[index]
        if (target.touch_ns is None and
                abs(now_ns - target.crossing_ns) <= self.hit_window_ns and
                abs(command.x - self.target_x) <= 3 and
                abs(command.y - self.line_y) <= 3):
            target.touch_ns = now_ns

    def render(self, now_ns: int) -> bytes:
        pixels = bytearray(self.width * self.height * 3)

        def square(x: int, y: int, radius: int, color: tuple[int, int, int]) -> None:
            for py in range(max(0, y - radius), min(self.height, y + radius + 1)):
                for px in range(max(0, x - radius), min(self.width, x + radius + 1)):
                    offset = (py * self.width + px) * 3
                    pixels[offset:offset + 3] = bytes(color)

        for x in range(self.width):
            offset = (self.line_y * self.width + x) * 3
            pixels[offset:offset + 3] = b"\x80\x80\x80"
        index = now_ns // self.period_ns
        if index < len(self.targets):
            target = self.targets[index]
            elapsed = now_ns - target.appearance_ns
            if target.touch_ns is not None and now_ns - target.touch_ns < 150_000_000:
                square(self.target_x, self.line_y, 3, (255, 0, 0))
            elif target.touch_ns is None and elapsed <= 650_000_000:
                y = round(8 + 40 * elapsed / self.flight_ns)
                square(self.target_x, y, 1, (0, 255, 0))
        return bytes(pixels)


def _has_red_effect(frame: Frame) -> bool:
    return any(frame.rgb[i] > 200 and frame.rgb[i + 1] < 50
               for i in range(0, len(frame.rgb), 3))


def run_synthetic(count: int = 30, fps: int = 60,
                  recognition_delay_ms: float = 0,
                  log_path: str | Path | None = None) -> dict:
    if count < 1 or fps < 2 or recognition_delay_ms < 0:
        raise ValueError("count, fps or recognition delay out of range")
    clock = VirtualClock()
    world = TargetWorld(count)
    buffer = LatestFrame()
    detector = GreenTargetDetector(clock)
    tracker = VelocityTracker()
    predictor = LineCrossingPredictor(world.line_y)
    frame_interval_ns = round(1e9 / fps)
    recognition_delay_ns = round(recognition_delay_ms * 1e6)
    frame_times: list[int] = []
    frame_ages: list[int] = []
    recognition_durations: list[int] = []
    last_seen_sequence = -1
    sequence = 0
    skipped_capture_deadlines = 0
    next_frame_ns = 0
    end_ns = count * world.period_ns
    with Telemetry(log_path) as telemetry:
        backend = FakeTouchBackend(clock, world.on_touch)
        scheduler = Scheduler(clock, backend, telemetry)
        telemetry.record("run_config", mode="synthetic_virtual_clock", count=count,
                         fps=fps, recognition_delay_ms=recognition_delay_ms,
                         width=world.width, height=world.height,
                         touch_backend="fake_independent_contacts")
        while next_frame_ns < end_ns or scheduler.next_due_ns() is not None:
            due_ns = scheduler.next_due_ns()
            candidates = [next_frame_ns] if next_frame_ns < end_ns else []
            if due_ns is not None:
                candidates.append(due_ns)
            if not candidates:
                break
            clock.sleep_until_ns(min(candidates))
            scheduler.run_due()
            if next_frame_ns >= end_ns or clock.now_ns() < next_frame_ns:
                continue
            capture_ns = clock.now_ns()
            frame = Frame(sequence, world.width, world.height,
                          world.render(capture_ns), capture_ns, capture_ns)
            buffer.publish(frame)
            current = buffer.read_after(last_seen_sequence, 0)
            assert current is not None
            last_seen_sequence = current.sequence
            frame_times.append(current.capture_complete_ns)
            target_index = capture_ns // world.period_ns
            if target_index < count:
                target = world.targets[target_index]
                if _has_red_effect(current) and target.effect_seen_ns is None:
                    target.effect_seen_ns = capture_ns
                    telemetry.record("effect_first_seen", frame_sequence=sequence,
                                     capture_complete_ns=capture_ns)
            recognition_start_ns = clock.now_ns()
            frame_ages.append(recognition_start_ns - current.capture_complete_ns)
            if recognition_delay_ns:
                clock.advance_ns(recognition_delay_ns)
            observation = detector.detect(current)
            recognition_durations.append(clock.now_ns() - recognition_start_ns)
            telemetry.record("frame_processed", frame_sequence=sequence,
                             capture_complete_ns=capture_ns,
                             recognition_start_ns=recognition_start_ns,
                             recognition_complete_ns=clock.now_ns(),
                             target_visible=observation is not None)
            if observation is not None and target_index < count:
                target = world.targets[target_index]
                if target.first_observed_ns is None:
                    target.first_observed_ns = capture_ns
            track = tracker.update(observation)
            if track is not None:
                intent = predictor.predict(track)
                if intent is not None:
                    scheduler.submit(intent)
            scheduler.run_due()
            sequence += 1
            next_frame_ns += frame_interval_ns
            # Capture owns the latest slot. If perception ran longer than a
            # frame interval, count skipped capture deadlines explicitly.
            if clock.now_ns() > next_frame_ns:
                missed = (clock.now_ns() - next_frame_ns) // frame_interval_ns + 1
                next_frame_ns += missed * frame_interval_ns
                skipped_capture_deadlines += missed
                telemetry.record("capture_deadline_skipped", count=missed,
                                 monotonic_ns=clock.now_ns())
        receipts = backend.receipts
        down = [r for r in receipts if r.command.phase == "down"]
        schedule_errors = [(r.injection_start_ns - r.command.scheduled_ns) / 1e6 for r in down]
        injection_durations = [(r.injection_return_ns - r.injection_start_ns) / 1e6
                               for r in receipts]
        successful = [t for t in world.targets if t.touch_ns is not None]
        effect = [t for t in successful if t.effect_seen_ns is not None]
        summary = {
            "mode": "synthetic_virtual_clock",
            "targets": count,
            "hits": len(successful),
            "misses": count - len(successful),
            "touch_attempts": world.touch_attempts,
            "false_touches": world.touch_attempts - len(successful),
            "effects_seen": len(effect),
            "frames_processed": sequence,
            "frames_overwritten": buffer.overwritten,
            "capture_deadlines_skipped": skipped_capture_deadlines,
            "capture_interval_ms": distribution([(b - a) / 1e6 for a, b in zip(frame_times, frame_times[1:])]),
            "frame_age_ms": distribution([value / 1e6 for value in frame_ages]),
            "recognition_duration_ms": distribution([value / 1e6 for value in recognition_durations]),
            "prediction_error_ms": distribution([(r.command.scheduled_ns - world.targets[r.injection_start_ns // world.period_ns].crossing_ns) / 1e6
                                                  for r in down if r.injection_start_ns // world.period_ns < count]),
            "schedule_error_ms": distribution(schedule_errors),
            "injection_duration_ms": distribution(injection_durations),
            "first_observation_to_touch_ms": distribution([(t.touch_ns - t.first_observed_ns) / 1e6
                                                           for t in successful if t.first_observed_ns is not None]),
            "appearance_to_effect_seen_ms": distribution([(t.effect_seen_ns - t.appearance_ns) / 1e6
                                                          for t in effect]),
            "touch_to_effect_seen_ms": distribution([(t.effect_seen_ns - t.touch_ns) / 1e6
                                                     for t in effect]),
            "log_path": str(log_path) if log_path else None,
        }
        telemetry.record("summary", **summary)
        scheduler.cancel_all("run_complete")
    return summary
