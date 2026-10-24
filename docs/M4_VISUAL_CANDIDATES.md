# Opening visual timing candidates

`visual_candidates.hpp/.cpp` implements a portable, offline candidate producer.
It consumes explicitly identified note-centre observations and the native chart
IR; it performs no capture, detection, audio, device or input I/O. It never
constructs `SyncLease`, arms a runner, or sets `timing_qualified` / `live_permitted`.
An experimental calibration caller may use its complete candidate set as an
honestly labelled measurement hypothesis; production qualification is separate.

## Supported model

The model is limited to native-v3 line 0, above Tap/Drag, a 1280x720 normalized
image, horizontal zero rotation, and an opening before the first judgment on
that line. The default Dlyrotz domain is `[1.028571429,12.000000000)` song seconds;
the anchor pool includes relevant notes through 45 s. Hold, Flick, below-side,
moving/rotating-line rendering and general note skins are not modelled.

For each constant-speed piece the producer integrates speed from native zero
(chart offset is already normalized in IR times), retaining the original note
`floor_position` and `speed`. A supplied speed-event floor anchor is rejected:
this version explicitly supports the zero-integral model, not another anchor
policy. Speed coverage must be contiguous from native zero through the domain;
gaps, overlaps and missing coverage reject construction. Zero speeds and negative
speeds remain valid pieces.

The near-static native pose must fit the explicit `pose_tolerance_pixels` box.
Its default `1e-5` pixel envelope includes the actual Dlyrotz raw y interpolation
`0.19999999 -> 0.2` (approximately 7.2e-6 pixel), and is propagated into predicted
positions. Native pose keys and their immediately preceding integer nanosecond
are checked, so a left-limit excursion cannot hide behind a reset at the key.
This is a conditional renderer approximation, not a game accuracy bound.

The approach scale range, pixel allowance, ROI, pose envelope, horizon, and
per-frame source-time allowance are explicit model values. Reference values
remain `720/(2*0.83175)`, 3 px and +/-30 ms. Model/calibration IDs and SHA256
labels are mandatory caller declarations; they are not proof of calibration.

## All branches and visibility

For every speed piece the solver analytically intersects the affine y constraints
for every one-to-one centre/note assignment. It does not choose a greedy match,
nearest seed, midpoint, best score, or monotonic positive-speed branch. Different
assignments retain their own original note IDs/JSON pointers and speed-event
pointer. Time intervals use inclusive integer-nanosecond endpoints; the overall
search domain remains half-open. Zero-speed matches retain their entire domain.

Arithmetic operations enclose results using `nextafter`. Possible-match bounds
round outward; an unmatched note excludes time only where its visibility is
guaranteed. Its x must also lie wholly inside the horizontal completeness ROI.
The default 90 px horizontal margin permits the reference detector's <=180 px
run to fit; offscreen/edge/uncertain x is not treated as a mandatory observation.
No physical uncertainty is inferred from nanosecond output units.

Exact visible-count matching has an explicit **complete, unoccluded centres**
premise. `centers_complete_unoccluded` defaults false and must be declared by the
caller/model. A colour blob detector cannot prove it. Overlapping notes merged
into one blob, missing centres or unsupported shapes invalidate that premise;
report unknown instead of claiming that an absent candidate proves wrong music.
The API does not silently permit occlusion by dropping mandatory interior notes.

## Provenance, clocks and continuity

Call `begin_epoch()` with a strictly newer positive epoch, then `observe(frame,
actual_now_host_ns)`. Frames require source/config SHA, frame artifact SHA,
source/frame/evidence/clock IDs, opening context, generation, geometry version,
sequence, source-valid flag, receive time and centre IDs. These describe actual
input evidence, not authenticated game identity or distinct rendered frames.
Unchanged pixel hashes are preserved and explicitly flagged; they do not prove
that the producer rendered a new framebuffer.

- `video_pts`: an explicit PTS value in its own named domain. Under the declared
  rate-one model and per-frame allowance, all source-minus-song interval unions
  are intersected across frames. PTS is not relabelled host time.
- `mapped_capture`: a caller-provided host interval, with known provenance and
  proper ordering/age. This solver does not establish that mapping or pixel age.
- `arrival_only`: independent pixel candidate sets only. Receive-time differences
  are not used as capture-time or song progress. No offset set is emitted.
- `unknown` or contradictory clock fields reject.

Stale/future/rollback data, duplicate IDs including nonadjacent A/B/A replay,
source/generation/geometry/context changes, gaps, exhausted budgets and lost
temporal consistency clear usable candidates and require a new epoch. Polling
cannot renew evidence; there is no status/timer API. Empty observations remain
unknown. Every result remains unqualified regardless of clock kind or interval
width. The inherited gRPC server stamp / unknown pixel-age limitations in
[M4_CAPTURE_CLOCK.md](M4_CAPTURE_CLOCK.md) continue to apply.

## Resource and numeric bounds

Defaults bound 256 notes, 256 speed events, 8 centres, 1,024 candidate branches,
200,000 work units and 128 retained observation IDs; pose tracks are bounded to
16,384 events. All branches must fit or the whole observation returns resource
failure with no partial best branch. Replay IDs are never evicted into reuse.
The caller can choose stricter limits within the checked maxima. Source/host
clock values must be exact nonnegative integers <=2^53-1, including on MSVC where
`long double` has double precision. Source interval arithmetic is checked for
overflow. Floating-enclosure width does not qualify the original game renderer,
native floor precision, capture path, source age, drift, or input phase.

## Verification

Independent MSVC 19.50 / C++20 Release `/W4 /WX` tests cover analytic positive and
negative roots, full zero-speed intervals, non-greedy assignments, invisible x,
unmatched visible notes, frozen-motion contradictions, raw arrival, signed mapped
intervals, replay/provenance failures, age/gap/rollback, resource exhaustion,
unsupported poses/floor anchors, absent centres and explicit new epochs.
The producer has 52 checks in eight groups. The reusable RGB888 opening detector
in `apps/chart_calibrate/opening_detector.hpp` has 22 further checks, with its
own resource bounds and an always-false `centers_complete_unoccluded` output.
It reproduces the frozen research detector's strict colour/run/row conditions;
47 existing PNG samples match its centre x/y, row count and median width exactly.
Those samples do not establish general detection completeness.

The existing archived measurements are replayed without changing K, the 3 px
allowance or +/-30 ms temporal allowance. The 22 primary samples retain one
conditional source-minus-song interval close to `[4.119856214,4.142167946]` s.
The heldout capture still rejects at **PTS 51.040 s** with
`no_temporally_consistent_branch`, requiring a new epoch; no tolerance is widened
to rescue it. These reproduce the prior experiment's result within the explicit
IR/numeric envelope, not a measured host-clock accuracy or universal detector
validation. The heldout's original rows lacked frame hashes; new private evidence
re-decodes only the existing video stream, checks ordinal/PTS and the unchanged
original detector's centres, and adds actual RGB hashes in a separate artifact.
Original measurements remain untouched, and no audio stream is decoded.

The separate experimental `FirstTapTrial` is also replayed with dense existing
video frames. Actual video PTS is reused only as **SIMULATED receipt time**;
healthy UI and complete/unoccluded centres are explicit simulation assumptions.
Neither QPC receipt/capture mapping nor live delay/jitter is measured. In the
primary PTS 13--16 s window, the 50 ms target sampling cadence arms at PTS
14.998 s (simulated target 16.135793 s); the 75 ms cadence arms at 15.027 s
(target 16.132937 s). All raw per-frame aliases remain in the report, and the
trial's temporal intersection resolves them before applying its 10.8--11.5 s
native-time arming band. No input is sent by either replay.

A fresh, local heldout PTS 50.3--53 s window also arms: 52.198 s at 50 ms cadence,
and 52.231 s at 75 ms cadence. This restarts candidate history rather than
preserving the earlier full-sequence lock. It does not rescind the frozen
full-sequence rejection at 51.040 s, demonstrate continuous synchronization, or
qualify live timing. Together these results support a bounded, separately
labelled one-tap measurement experiment, whose actual game acceptance and
cleanup must still be observed independently.

The tests and fixed replay evidence are archived privately at
`../local-assets/phigros-4.0.1/m4-visual-candidates-001/`. The producer is suitable
for candidate/shadow/calibration measurement work; it does not establish AP,
continuous lock, game input lead, independent pixel freshness or production live
readiness. The whole-song projection and input-strategy calibrations remain
separate requirements.
