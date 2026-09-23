"""Simple target detector, track and crossing predictor used before game vision."""

import math

from .clock import Clock
from .contracts import Frame, HitIntent, Observation, Track


class GreenTargetDetector:
    """Detect a synthetic green square from current RGB pixels only."""

    def __init__(self, clock: Clock):
        self.clock = clock

    def detect(self, frame: Frame) -> Observation | None:
        count = x_sum = y_sum = 0
        for index in range(0, len(frame.rgb), 3):
            r, g, b = frame.rgb[index:index + 3]
            if g > 180 and r < 80 and b < 80:
                pixel = index // 3
                x_sum += pixel % frame.width
                y_sum += pixel // frame.width
                count += 1
        completed = self.clock.now_ns()
        if not count:
            return None
        return Observation(frame.sequence, frame.capture_complete_ns, completed,
                           x_sum / count, y_sum / count)


class VelocityTracker:
    def __init__(self):
        self.previous: Observation | None = None
        self.history: list[Observation] = []
        self.track_id = 0

    def clear(self) -> None:
        self.previous = None
        self.history.clear()
        self.track_id += 1

    def update(self, observation: Observation | None) -> Track | None:
        if observation is None:
            if self.previous is not None:
                self.clear()
            return None
        previous = self.previous
        self.previous = observation
        if previous is None:
            self.history = [observation]
            return None
        elapsed_s = (observation.capture_complete_ns - previous.capture_complete_ns) / 1e9
        if elapsed_s <= 0 or elapsed_s > 0.2:
            self.clear()
            self.previous = observation
            self.history = [observation]
            return None
        self.history.append(observation)
        self.history = self.history[-8:]
        times = [(item.capture_complete_ns - self.history[0].capture_complete_ns) / 1e9
                 for item in self.history]
        mean_t = sum(times) / len(times)
        denominator = sum((t - mean_t) ** 2 for t in times)
        if denominator == 0:
            return None
        mean_x = sum(item.x for item in self.history) / len(times)
        mean_y = sum(item.y for item in self.history) / len(times)
        vx = sum((t - mean_t) * (item.x - mean_x) for t, item in zip(times, self.history)) / denominator
        vy = sum((t - mean_t) * (item.y - mean_y) for t, item in zip(times, self.history)) / denominator
        residual = math.sqrt(sum((item.y - (mean_y + vy * (t - mean_t))) ** 2
                                 for t, item in zip(times, self.history)) / len(times))
        return Track(self.track_id, observation.frame_sequence,
                     observation.capture_complete_ns,
                     observation.recognition_complete_ns, observation.x, observation.y,
                     vx, vy, residual)


class LineCrossingPredictor:
    def __init__(self, line_y: float, min_speed_px_s: float = 1.0):
        self.line_y = line_y
        self.min_speed_px_s = min_speed_px_s

    def predict(self, track: Track) -> HitIntent | None:
        if track.vy_px_s < self.min_speed_px_s:
            return None
        remaining_s = (self.line_y - track.y) / track.vy_px_s
        if remaining_s < 0 or remaining_s > 2:
            return None
        predicted_ns = track.capture_complete_ns + round(remaining_s * 1e9)
        x = track.x + track.vx_px_s * remaining_s
        # This is a heuristic pixel-quantization bound, not a calibrated interval.
        uncertainty_ns = round(max(0.5, track.residual_px) / track.vy_px_s * 1e9)
        return HitIntent(
            key=f"track-{track.track_id}", track_id=track.track_id,
            frame_sequence=track.frame_sequence, x=x, y=self.line_y,
            predicted_hit_ns=predicted_ns,
            prediction_basis=(f"frame={track.frame_sequence}; x={track.x:.3f}; "
                              f"y={track.y:.3f}; vx={track.vx_px_s:.3f}px/s; "
                              f"vy={track.vy_px_s:.3f}px/s; line_y={self.line_y:.3f}; "
                              f"residual={track.residual_px:.3f}px"),
            confidence=1 / (1 + uncertainty_ns / 10_000_000),
            uncertainty_ns=uncertainty_ns,
        )
