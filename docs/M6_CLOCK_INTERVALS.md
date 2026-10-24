# M6 causal order-only clock intervals

`tools/touch_clock_intervals.py` is a pure offline API for bounding an associated
Android monotonic event in a host monotonic clock domain. It performs no device
access, file loading, rate fitting or touch analysis. It does not modify
`analyze_touch_probe.py` or produce an input-lead calibration.

## What is bounded

For each causal sample the caller establishes:

```text
host_before <= actual host time of Android clock read <= host_after
```

The Android value is the `CLOCK_MONOTONIC` read made inside that bracket. For an
event strictly between adjacent Android sample values A and B, clock ordering
gives the closed interval:

```text
host_before(A) <= host time of event <= host_after(B)
```

The bounds are deliberately **not** `host_after(A)` and `host_before(B)`: an event
after A's read can still precede the receipt of A's reply. If the event timestamp
exactly equals a sampled Android stamp, that sample's own `[before, after]`
bracket is used. Equal values are treated as that same monotonic instant under the
caller-declared clock-domain contract; timestamp quantization/resolution error
outside that contract must be accounted for by the producer. A zero-width input
bracket remains zero-width; the mapper does not invent additional precision.

No equal-rate or constant-offset relation is assumed. The mapper does not
interpolate a point, calculate a midpoint estimate, subtract clocks directly,
extend outside the sampled Android range, or narrow a bracket using a fitted
offset. A wide interval remains wide. The approximately 1.614 ms constant-offset
intersection explored by the earlier probe is not a guarantee used here.

The assumptions remain material: the supplied before/after calls causally bracket
the actual read; the event and samples belong to the same Android monotonic clock
instance; and the Android and host clocks preserve physical-time order across the
covered span. A reboot, emulator restore or unobserved clock discontinuity cannot
be repaired or authenticated from these numeric values alone.

## API

```python
from tools.touch_clock_intervals import (
    ClockAssociation, ClockSample, AssociatedEvent, OrderOnlyClockMap,
)

association = ClockAssociation(
    device_id="explicit-device-label",
    boot_id="12345678-1234-1234-1234-123456789abc",
    host_clock_id="explicit-host-boot-and-monotonic-domain",
)
samples = [
    ClockSample(1, 100, 1000, 1100, association),
    ClockSample(2, 200, 2000, 2100, association),
]
mapper = OrderOnlyClockMap(samples)
bounded = mapper.map_event(AssociatedEvent(150, association, "actual-fixture-event-17"))
assert (bounded.lower_ns, bounded.upper_ns) == (1000, 2100)
assert bounded.support_sequences == (1, 2)
```

The example is synthetic. The returned frozen `HostInterval` includes lower and
upper host nanoseconds, both supporting sample sequences, `exact_sample`, the
Android event timestamp, caller's event evidence ID, association and a descriptive
scope. `width_ns` and `support_sequences` are convenience properties. There is no
midpoint, latency, input lead or qualification field. `dataclasses.asdict` can be
used by a caller that needs its own report serialization.

`ClockAssociation` requires an explicit bounded device label, canonical Android
boot UUID, and host clock instance/domain label. All samples and the event must
match all three exactly; no association is inferred from timestamp magnitude,
serial alone, overlapping numeric ranges or a similar estimated offset. These
labels are caller assertions, not cryptographic authentication. Deriving the
event's boot/device association from actual fixture provenance is outside this
pure mapper; missing association is rejected.

## Validation and resource contract

The complete supplied stream is validated before a map is available:

- One to 8,192 `ClockSample` records. A larger stream raises at record 8,193;
  it never silently truncates into a usable partial map.
- Exact Python integers in signed 64-bit nonnegative range; sample sequence is
  positive. Booleans, floats, numeric strings, negative values and overflow reject.
- Sample sequence and Android sample time strictly increase. Gaps in sequence are
  allowed and retained; this API does not claim all producer samples were supplied.
- Every host bracket is ordered. `host_before` and `host_after` must each be
  nondecreasing across samples. Overlapping causal brackets are valid; the mapper
  does not assume the caller used only one outstanding request.
- All sample/event associations are explicit and equal. Records are frozen and
  the mapper's public association is read-only. An invalid sample rejects the
  whole input, rather than being discarded or repaired.
- Event evidence IDs are nonempty and at most 512 UTF-8 bytes; device/host labels
  are at most 256 bytes. An event outside the range rejects, including a one-sample
  map queried anywhere except its exact Android stamp.

Construction takes bounded O(n) storage; each event lookup uses binary search.
The API has no file CLI, implicit discovery, output writes or mutable evidence
refresh. Input schema/provenance acquisition remains the caller's responsibility.

## Connection to the existing probe

`work/probe_android_clock_stream.py` supplies one outstanding PING per persistent
ADB stream. For each row, its `host_before_ns` is recorded before sending/flushing
PING and `host_after_ns` is timestamped immediately after reading the matching
response line, before queue handoff. The nested Android `monotonic_ns` is read
after the request is validated. These fields are the causal inputs; host wall
clock, Android realtime, `monotonic_after_ns`, and offset-candidate fields are not
used as interchangeable stamps.

The existing private rows are at
`outputs/local-assets/phigros-4.0.1/m6-clock-probe-003/samples.jsonl`. A caller must
check each row's producer schema/sequence, preserve its own `android.boot_id`,
and join the explicit device report and host clock provenance. It must not replace
every row's boot ID with the first row's ID before validation, or assign this
probe's association to an unrelated fixture event simply because its time fits.
The event timestamp must be an Android monotonic event/callback timestamp with
its own established association, not a host receipt time relabeled as Android.

## Tests and limits

```powershell
python -m unittest discover -s tests -p test_touch_clock_intervals.py -v
```

Eight synthetic tests pass with Python 3.14. They cover nonlinear clock spacing,
adjacent supports, exact range endpoints, a single exact sample, no extrapolation,
overlapping brackets, duplicate/backward sample order, host rollback, mixed
device/boot/host domains, missing association, invalid integer types, resource
overflow and immutable output. No fixture callback was mapped as a validation
claim by this suite. Actual callback association and effect/latency interpretation
remain separate work; this tool never establishes game input lead.

A separate read-only producer-record smoke check validated all 128 existing
`m6-clock-probe-003` rows after checking row/nested sequence and producer schema,
using each row's actual boot UUID. Mapping sample 1's exact Android stamp returned
its unchanged host bracket `[87800709149000, 87800817888000]` ns, width
108,739,000 ns. This was an exact clock-read query, not a touch event. The wide
result illustrates that no constant-offset intersection or midpoint was silently
substituted for the causal input.
