# M6 offline touch evidence analysis

`tools/analyze_touch_probe.py` validates explicitly supplied host probe artifacts
and the opt-in Android timestamp fixture's JSONL. It uses Python's standard
library only. It never discovers a device, runs ADB, changes a property, starts
an app, or injects input.

```powershell
python tools/analyze_touch_probe.py `
  --manifest C:/explicit/probe/manifest.json `
  --events C:/explicit/probe/events.jsonl `
  --android C:/explicit/retrieved/touch-evidence.jsonl `
  --tolerance-pixels 3 `
  --output C:/explicit/new-report.json
```

Omit `--output` for stdout JSON. A named output must not already exist; no input
or output is overwritten. Library API:
`analyze_probe(manifest_path, events_path, android_path=None, tolerance_pixels=3)`.
Exit codes are 0 for a consistent observed fixture path, 2 for incomplete
evidence, 3 for failed checks, and 1 for invalid options or file I/O errors.
Missing Android evidence is incomplete even when every host RPC succeeded.

A separate [host / Android clock diagnostic](M6_CLOCK_PROBE.md) now measures
causal clock-offset intervals. It has not been folded into this analyzer:
sampling must bracket and identify the actual fixture session before any
cross-domain delay can be reported.

## What is checked

The host manifest identifies the current probe schema/package, case, expected
contact count, declared capacity, dimensions, and normal action plan. The
analyzer validates commands against that plan, QPC ordering, required finite
termination and release claims. Held, moved, and after-release visual snapshots
must occur once in that order with the expected active counts, monotonic
visual counters, and consistent DOWN/UP/CANCEL totals. The terminal snapshot
must agree with the manifest's final evidence, including sequence, counts,
active state, decode error/completeness, ring overwrites and available samples.
These checks validate recorded visual structures; the analyzer does not decode
PNG pixels or authenticate an APK from its claimed SHA string.

Android JSONL must contain a supported header, contiguous 1-based MotionEvent
sequence, complete finite pointer snapshots, and a terminal footer. Counts and
`bytes_before_footer` are recomputed from the actual UTF-8 bytes. Footer
faults, limits, first-unrecorded events, lifecycle errors, and active state are
checked. Clock/invalid-event/invalid-pointer terminal reasons cannot be made
successful by writing `fault:false`. An incomplete tail or missing footer
cannot establish a valid pointer lifecycle or known release.

An independent 32-ID ledger checks DOWN/POINTER_DOWN/MOVE/UP/POINTER_UP/CANCEL,
the changed pointer index/raw encoding, complete snapshots, constant gesture
down-time, strict integer IDs, and `active_after`. Android IDs are learned from
unique DOWN coordinates as a one-to-one mapping to logical host contacts;
their numeric values need not match. Batch requests can appear as separate
Android POINTER_DOWN events in a different order. This does not establish
atomic or simultaneous delivery.

Every current snapshot checks existing pointer coordinates, including snapshots
created by another finger's DOWN or UP. The moving pointer must stay on the
declared segment without reversal; all other pointers remain within the explicit
tolerance. The MOVE target must actually be observed in a MOVE event while the
full requested capacity is active. Final UPs and zero active pointers must be
observed. A normal projection/path in this fixture is not a game judgment region.
Default tolerance is three Euclidean pixels; the explicit range is 0–20 pixels.

The intentional transport-loss probe switches a client to an invalid target;
it is not proof of emulator disconnection. Its normal held/MOVE path is checked,
but emergency changed-UP coordinates at the center are treated separately from
endpoint calibration. `release_coordinates_checked=false` preserves that limit.

## Clock and completeness limits

Host RPC duration and dispatch lateness use only host QPC timestamps. Android
event intervals, arrival intervals, and callback age use only their declared
Android time domains. Negative callback age is retained and reported as a
timestamp anomaly, not clamped to zero. Gesture down-time is the beginning of
the gesture, not each newly added finger's start time.

`host_to_android_clock_mapping`, `input_latency_ns`, and `game_input_lead_ns`
remain null. Without a separately measured clock mapping, Android event time
cannot be subtracted from Windows dispatch/return time. No calibration is inferred
from RPC success, a host summary flag, source timestamp magnitude, or one high
quality-looking frame.

`stream_complete` concerns the bounded recorder stream's structure and valid
reconstruction. `pointer_lifecycle_valid` is false when schema, sequence gaps,
truncation, timing or finalization prevents full reconstruction.
`expected_path_verified` additionally requires the declared effect/path checks.
`final_zero_observed` requires complete reconstructable evidence and means zero
at the last observation in this fixture app, not zero everywhere on Android or
at a later time. Missing/failed evidence is never promoted to known release.

MotionEvent sequence, per-pointer visual-ring sequence, and per-command host
RPC logs are different streams and are never joined by equal sequence numbers.
`history_size` is counted, not expanded; historical samples cannot be claimed
fully verified. Visual ring overwrites are separate from JSONL truncation and
Android input loss. A zero `first_unrecorded_events` and contiguous recording
do not prove that Android generated every requested input sample.

Inputs are bounded to 512 KiB manifest, 18 MiB JSONL files, 256 KiB per line,
9,000 records, 32 pointers, and the declared fixture's 8,192-event/16 MiB/60-second
limits. Duplicate JSON keys and nonfinite numeric values in validated fields
are rejected. Source hashes describe the bytes read; a bounded prefix is
explicitly labeled when reading stops early. APK/session association is supplied
by the caller and not cryptographically authenticated by this tool.

## Offline verification

Run:

```powershell
python -m unittest discover -s tests -p test_analyze_touch_probe.py -v
```

Synthetic tests exercise logical/Android ID remapping; batched callback order;
host and Android time-domain separation; missing device evidence, footer and
partial tails; sequence duplicates/gaps; invalid snapshots/IDs/action indexes;
missing UP; false zero claims; stationary drift and primary-pointer drift during
another pointer's DOWN/UP; off-path/reversed motion; unexpanded history; recorder
limits/fault reasons; contradictory or duplicated visual stages; emergency
center releases; output refusal and resource/JSON limits.

No actual fixture input or device evidence is implied by passing this suite.
Real device capacity, latency, release, and game qualification remain separate
M6/M7 work.

The current suite passes 25 tests with Python 3.14 on Windows. Independent review
also reproduced and verified the three regression fixes for pointer movement in
another pointer's DOWN snapshot, fault terminal reasons with a false fault flag,
and contradictory host terminal evidence.

## Probe-local visual snapshot adaptation

The actual four-contact `m6-live-capacity-001` attempt exposed a visual trace
representation mismatch. One Android MOVE carries all current pointers; the
fixture's visual ring emits one MOVE sample for each of those pointers. The
retained expected path requests only the primary pointer's MOVE. After matching
that target, `verify_touch_path` correctly rejects further unmatched MOVE samples
under its original path contract. This produced `extra move after expected path`
even though the raw stationary coordinates had not changed. The attempt remains
failed; its archived manifest/events/native recording are not relabeled.

`apps/touch_probe/fixture_path.hpp` adds the probe-only policy
`stationary-snapshot-move-v1`. Both the pre-release path check and normal full-path
check use it, then call the retained verifier. It constructs a filtered copy and
omits only an explicitly stationary ID's redundant MOVE when all these hold:

- The expected path contains its DOWN anchor and no requested MOVE for that ID.
- Its valid observed DOWN has already appeared and its UP/CANCEL has not appeared.
- Coordinates remain within the supplied per-axis tolerance of both the expected
  DOWN and the actual observed DOWN. Two endpoints on opposite sides of the
  expected tolerance cannot hide larger actual drift.

Primary MOVE samples, unknown IDs and every non-MOVE phase are retained. A MOVE
before DOWN or after release rejects. The adapter is bounded to the fixture's
64-sample visual ring and does not repair incomplete evidence. Raw snapshots,
counts and native JSONL remain unchanged in the logs. The manifest names the
policy, retained-check reason and omitted redundant-MOVE count separately; those
fields do not replace evidence or qualify effect by themselves.

`src/touch_evidence.cpp` remains byte-for-byte unchanged, SHA-256
`cac6121faaca2d24665010701da7a4b907392a421445c86159adfd20be667422` before and
after this adaptation. The native fixture, offline JSONL analyzer and clock mapper
are unchanged. The intentional transport-loss case still checks its normal path
before the fault separately from final emergency UP counts/zero; emergency center
coordinates are not promoted into normal path calibration.

`tests/fixture_path_tests.cpp` passes 20 offline checks with MSVC 19.50 C++20,
Release `/W4 /WX`, and standalone CTest 1/1. Its four-finger fixture-shaped stream
reproduces the original retained-verifier rejection and succeeds through the
adapter. Negative cases retain failures for stationary drift, opposite-tolerance
drift, unknown IDs, wrong phase, primary off-path/extra/missing-target MOVE,
pre-DOWN/post-UP MOVE, CANCEL, incomplete input and bounds/configuration errors.
The optional `fixture_path_adapter_contracts` CTest is available under
`RHYTHM_BUILD_PAS_GRPC` with `BUILD_TESTING`, links only `pas_core`, and adds no
default dependency. An independent new device attempt is required to validate
the changed probe; passing these offline checks is not a device result.
