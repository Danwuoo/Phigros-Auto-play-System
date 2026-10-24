"""Bound an associated Android event in host time using causal sample order only.

No device access, files, clock-rate/offset fit, extrapolation or midpoint estimate.
Association and each causal acquisition bracket must be established by the caller.
"""
from bisect import bisect_left
from dataclasses import dataclass
import re
from typing import Iterable

MAX_SAMPLES = 8192
MAX_NS = (1 << 63) - 1
SCOPE = (
    "Closed host monotonic interval from same-device/boot causal sample order; "
    "caller-asserted association and clock ordering, not authenticated identity; "
    "no equal-rate, offset, interpolation, extrapolation, latency or game-input-lead claim"
)


def _integer(value: int, name: str, minimum: int = 0) -> None:
    if type(value) is not int or not minimum <= value <= MAX_NS:
        raise ValueError(f"{name} must be an integer in [{minimum}, {MAX_NS}]")


def _text(value: str, name: str, maximum: int) -> None:
    if type(value) is not str or not value or len(value.encode("utf-8")) > maximum:
        raise ValueError(f"{name} must be nonempty bounded UTF-8 text")


@dataclass(frozen=True, slots=True)
class ClockAssociation:
    device_id: str
    boot_id: str
    host_clock_id: str

    def __post_init__(self) -> None:
        _text(self.device_id, "device_id", 256)
        _text(self.host_clock_id, "host_clock_id", 256)
        if type(self.boot_id) is not str or re.fullmatch(
            r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}", self.boot_id
        ) is None:
            raise ValueError("boot_id must be an explicit canonical Android boot UUID")


def _association(value: ClockAssociation) -> None:
    if type(value) is not ClockAssociation:
        raise ValueError("explicit ClockAssociation is required")


@dataclass(frozen=True, slots=True)
class ClockSample:
    sequence: int
    android_monotonic_ns: int
    host_before_ns: int
    host_after_ns: int
    association: ClockAssociation

    def __post_init__(self) -> None:
        _association(self.association)
        _integer(self.sequence, "sample sequence", 1)
        _integer(self.android_monotonic_ns, "Android sample time")
        _integer(self.host_before_ns, "host before")
        _integer(self.host_after_ns, "host after")
        if self.host_before_ns > self.host_after_ns:
            raise ValueError("causal host bracket is reversed")


@dataclass(frozen=True, slots=True)
class AssociatedEvent:
    monotonic_ns: int
    association: ClockAssociation
    evidence_id: str

    def __post_init__(self) -> None:
        _association(self.association)
        _integer(self.monotonic_ns, "Android event time")
        _text(self.evidence_id, "event evidence_id", 512)


@dataclass(frozen=True, slots=True)
class HostInterval:
    lower_ns: int
    upper_ns: int
    lower_support_sequence: int
    upper_support_sequence: int
    exact_sample: bool
    android_event_monotonic_ns: int
    event_evidence_id: str
    association: ClockAssociation
    scope: str = SCOPE

    @property
    def width_ns(self) -> int:
        return self.upper_ns - self.lower_ns

    @property
    def support_sequences(self) -> tuple[int, ...]:
        return ((self.lower_support_sequence,) if self.exact_sample else
                (self.lower_support_sequence, self.upper_support_sequence))


class OrderOnlyClockMap:
    def __init__(self, samples: Iterable[ClockSample]):
        validated: list[ClockSample] = []
        for sample in samples:
            if len(validated) == MAX_SAMPLES:
                raise ValueError("sample count exceeds 8192; no partial map was built")
            if type(sample) is not ClockSample:
                raise ValueError("all samples must be validated ClockSample objects")
            if validated:
                previous = validated[-1]
                if sample.association != previous.association:
                    raise ValueError("mixed device, boot or host clock association")
                if (sample.sequence <= previous.sequence or
                        sample.android_monotonic_ns <= previous.android_monotonic_ns):
                    raise ValueError("sample sequence/Android monotonic time did not strictly increase")
                if (sample.host_before_ns < previous.host_before_ns or
                        sample.host_after_ns < previous.host_after_ns):
                    raise ValueError("host sample clock moved backward")
            validated.append(sample)
        if not validated:
            raise ValueError("at least one causal sample is required")
        self._samples = tuple(validated)
        self._times = tuple(sample.android_monotonic_ns for sample in validated)
        self._association = validated[0].association

    @property
    def association(self) -> ClockAssociation:
        return self._association

    @property
    def sample_count(self) -> int:
        return len(self._samples)

    def map_event(self, event: AssociatedEvent) -> HostInterval:
        if type(event) is not AssociatedEvent or event.association != self.association:
            raise ValueError("event must explicitly match the sample device, boot and host clock")
        index = bisect_left(self._times, event.monotonic_ns)
        exact = index < len(self._times) and self._times[index] == event.monotonic_ns
        if exact:
            lower = upper = self._samples[index]
        else:
            if index == 0 or index == len(self._times):
                raise ValueError("event outside sampled Android range; extrapolation is forbidden")
            lower, upper = self._samples[index - 1], self._samples[index]
        return HostInterval(lower.host_before_ns, upper.host_after_ns,
                            lower.sequence, upper.sequence, exact, event.monotonic_ns,
                            event.evidence_id, self.association)
