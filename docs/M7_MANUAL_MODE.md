# M7 manual-mode UI lifecycle

`rhythm::ManualLifecycle` restores the manual selection/Play, confirmed result,
and standby workflow as a portable UI state machine. It does not restore the
old vision note planner. The caller supplies actual, independently identified
delivered UI observations; distinct deliveries do not prove distinct rendered
frames or qualify source pixel age. This component produces no input, arming, song clock,
`SyncLease`, release acknowledgement, or device operation.

## Historical behavior and extraction boundary

The audited source is immutable `origin/main` commit
`2d479e17fa1566c089dce019cbb11eb883ba47fd`, not the current file tree. These line
numbers refer to that commit:

| Source | Observed behavior |
| --- | --- |
| `include/pas/game_session.hpp:5–31` | A distinct lifecycle owns standby/starting/playing/result/fault/stopped, monotonic round IDs and result rearm latches. |
| `src/game_session.cpp:105–138` | First fresh HUD starts a round in STARTING; at least three distinct frames over 20 ms plus the old observer gate enters PLAYING. Frame age must be below 100 ms; gaps above 250 ms reset counters; source generation/geometry changes fault. |
| `src/game_session.cpp:71–95` | HUD means pause-bar count at least two and score-glyph count at least four. Result requires all six English static label masks, each similarity at least .72, with no HUD/menu, on exact 1280×720 RGB geometry. |
| `src/game_session.cpp:139–147` | Three fresh result frames spanning at least 60 ms end the round. No-note frames, darkness or fixed song duration are not result confirmation. |
| `src/game_session.cpp:119–130` | Result stays latched against a stray HUD; a manual menu transition, or a non-HUD/non-result transition followed by HUD, permits the next round. |
| `src/game_session.cpp:152–178` | `SessionPerception` directly runs `GameObserver`; it resets that observer at a new round. `SessionGameOwner` creates and accepts into the old `GamePlanOwner`. Those wrappers are coupled to the old vision planner. |
| `src/game.cpp:1068–1083` | The old observer gate separately requires three valid HUD frames and capacity validity; note/line presence does not arm gameplay. |
| `src/game_session.cpp:166–168` | A first result candidate immediately closes `allow_down`, before lifecycle result confirmation. |
| `src/game_session.cpp:181–194` | Suspend cancels current contacts without replacing the round owner. Finish requests stop, cancels once, preserves the first release report, and destroys the owner only after a clean scheduler result. |
| `src/manual_session.cpp:242–267` | Confirmed result releases before archive completion. Default diagnostic mode waits for the next manual Play; one-round/full-recording modes exit. No automatic selection, Play, retry or result dismissal is issued here. |
| `src/manual_session.cpp:292–293,398–430` | Dispatch checks independent stop/freshness/revocation latches; Escape, Ctrl+C/console signals or preview closure stop. Capture stale/inactive/relative-lag changes revoke input. Watchdog is an abort. |
| `include/pas/manual_score_budget.hpp:10–24` | Score-only limits 1–20 confirmed rounds; an aborted round stops without counting. Round watchdog is at most 600 s; single-round standby is 300 s, multi-round standby 1,800 s, and total wall budget is 900 s × requested rounds. |
| `research/manual_score_only/standby.ps1:1–11` | The launcher defaults to 20 rounds, lead 35 ms and baseline binary; lazy/lead 40 are explicit variants. It invokes `manual-session --score-only --rounds`, with the historical five-touch capability file. It implements no lifecycle itself. |

Historical tests in `tests/game_session_tests.cpp:37–79` cover unlimited standby,
two short/long rounds, result latch, pause/loading, distinct fresh result frames,
geometry faults and watchdog abort. Lines 108–140 cover no input before allow,
release on stop, no replay across HUD loss, stale evidence and unknown-release
blocking. Lines 176–222 exercise two rounds and immediate Down closure on the
first result candidate. `research/manual_score_only/budget_tests.cpp:14–31`
checks score-only bounds. These sources were read, not re-executed here; the
historical pixel tests can skip when their deleted local files are unavailable.
Historical references to fresh/distinct frames describe the old software's
observation checks, not independently established renderer/capture-clock proof.

The extracted layer reuses these control semantics, while replacing the old
observer dependency with four exclusive UI booleans (`hud`, `menu`, `result`,
`pause`). It does not copy `SessionPerception`, `SessionGameOwner`, old contact
lead values, five-pointer capability claims or archived scores into this branch.
The current calibrated `screen_state` adapter supplies the booleans separately.

## Portable API and timing contract

Files are `include/rhythm/manual_lifecycle.hpp`, `src/rhythm/manual_lifecycle.cpp`
and `tests/manual_lifecycle_tests.cpp`. Methods are:

```cpp
ManualLifecycle lifecycle(ManualConfig{ /* explicit budgets */ });
ManualStatus observed = lifecycle.observe(actual_delivered_observation, host_now_ns);
ManualStatus aged = lifecycle.poll(host_now_ns); // status(now) has the same meaning
lifecycle.request_user_stop(); // thread-safe and sticky
```

All other calls have one owner. `observe` is the only operation that can add
debounce support. `poll`/`status` first check stop, monotonic host time, per-round
watchdog and evidence expiry; they never change observation sequence, timestamps
or provenance to the timer time. `new_round` and `ended` are one-call edges.
The first terminal outcome/reason is retained, including a confirmed final result;
a cleanup stop or later bad clock cannot replace it.

An observation requires nonzero observation sequence, generation and geometry version;
2–16384 pixel dimensions; quarter-turn rotation 0–3; a nonempty evidence ID of at
most 512 bytes; a valid source flag; and at most one of the UI booleans. All false
means unknown. Receive time must be nonnegative and no later than `now`.

- `ManualTimestampKind::mapped_capture` requires an independently mapped capture
  timestamp in the same host monotonic domain, no later than receive time. The
  caller must supply the mapping evidence; this component does not establish it.
  Mapping a server wall-clock metadata stamp alone does not qualify this mode.
- `ManualTimestampKind::arrival_only` requires `capture_host_ns == -1`. Receipt
  time is used only to debounce and age UI evidence. Status explicitly reports
  `capture_timestamp_known=false`; it does not locate the song or imply timing
  accuracy at the displayed pixels.
- `unknown` is rejected. Source generation, geometry, dimensions, rotation or
  timestamp-kind changes terminate this instance. A new instance is a caller
  decision after any separate input/release requirements have been satisfied.

Sequence/time rollback faults. A repeated sequence, repeated recent evidence ID or
equal observation timestamp cannot refresh freshness or add support and suspends any
active round. Pending debounce retains all supporting IDs, at most 64, until
confirmation; overflow resets and suspends. This includes the case where three
observations arrive before the required 20/60 ms span. A separate bounded recent-ID ring
also rejects replay across UI classes and after PLAYING. This is bounded duplicate
checking, not authentication of caller-assigned provenance; IDs older than the
recent ring cannot be globally authenticated without a stronger source contract.
In the current stream adapter these checks distinguish delivered observations;
neither the local sequence nor emulator stream event sequence proves a new
underlying rendered image.

## Round transitions and limits

The first valid fresh HUD starts `STARTING` and increments a logical round ID.
Three distinct delivered HUD observations spanning at least 20 ms make `ui_playing=true` and enter
`PLAYING`. The flag expresses UI lifecycle only. It is not an input grant and is
not proof of an actual button press, selected chart identity or timing calibration.

Pause, unknown, menu, invalid source or expired evidence suspend an active round
without ending it. Returning HUD must pass the same three-observation confirmation
again. A first result candidate immediately clears `ui_playing`; three distinct
delivered result observations over at least 60 ms confirm `RESULT`, set `ended`, and increment
`completed_rounds`. Missing or interrupted result evidence cannot complete a round.

With rounds remaining, result stays latched against subsequent HUD until a fresh
menu or unknown/non-HUD transition. Explicit pause alone cannot rearm a result.
The caller continues observing while the user selects/retries and presses Play.
There are no automatic menu taps. At the round limit, `session_done=true` and no
more rounds can start. `result_confirmed` retains that terminal result; it never
means contacts were independently observed released.

Portable defaults are one confirmed round, 600 s per-round watchdog, 100 ms
observation age and 250 ms maximum continuity gap. The round limit is 1–20;
watchdog is 0–3600 s (zero disables), age 0–1 s exclusive of zero, and continuity
gap 0–10 s exclusive of zero. Watchdog expiry produces terminal `ABORTED`, never
`RESULT`, and counts no completed round. A user stop produces terminal `STOPPED`.
Fault/abort/stop is sticky. Freshness expiry also resets pending continuity, even
if the configurable gap limit is longer. The object retains bounded scalar state
and at most two 64-ID sets; it keeps no frame pixels, note memory or history log.

`ManualStatus::round` is a UI round counter, **not an execution epoch**. Pause or
unknown keeps that UI round active, while `RunSupervisor` retires the current
execution epoch on loss of playing permission. Input resumption requires a
separate new execution epoch, fresh timing/state observations, known release and
explicit arm. A caller must not recycle the unchanged UI round ID to renew an
expired/revoked lease or bypass a pending release.

This is intentionally stricter than the old wrapper: an explicit pause does not
rearm a latched result; provenance replay is rejected; first-terminal reasons are
sticky; and expiry checks precede processing a new observation. The new UI layer
does not wait for the extra old `GameObserver` gate/reset cycle. Future input still
requires the separate supervisor/runner qualification and release contracts.

## Observe-only CLI

The root-owned Windows entry point is:

```text
chart_manual shadow SERIAL AVD CALIBRATION.json NEW_DIR --observe-only
  [--rounds 1..20] [--watchdog-seconds 1..600] [--session-seconds 1..3600]
```

Arguments following `--observe-only` are optional key/value pairs on the same
command line. Defaults are one round, 180 s per-round watchdog and 600 s total
wall time. This differs from the historical unrestricted diagnostic standby and
the score-only 20-round launcher; the new limits are explicit.

The CLI checks the explicit AVD/serial, foreground Phigros package
`com.PigeonGames.Phigros`, version 4.0.1 / versionCode 157, and calibrated 1280×720,
source-rotation-1 geometry. The user selects the song and presses Play. Escape,
Ctrl+C, or creating `NEW_DIR/STOP` stops observation. No output directory may
already exist. Logs are bounded to 64 MiB / 250,000 JSONL rows, and confirmed
results produce at most 20 PNG images. It records `input_enabled=false`,
`audio_enabled=false`, and `automatic_play_enabled=false`.

The CLI uses host-arrival-only UI timestamps and preserves emulator Unix source
timestamps separately as unmapped. Its stream sequence/arrival identifies delivered
observations, not proven distinct rendered frames. Source pixel age remains
unqualified; the server stamp is assigned before screenshot extraction rather
than supplied by the returned frame's producer. See [capture-clock source findings](M4_CAPTURE_CLOCK.md).
It creates no touch backend, audio capture,
song clock or gameplay arm. It does not yet bind a recognized song to a compiled
chart, establish timing/geometry/backend qualification, or certify result detector
performance on untested UI/localization/version combinations.

## Validation scope

The portable source independently built with MSVC 19.50, C++20, Release and
`/W4 /WX`. Its standalone suite passes **25 groups / 1,311 checks** and CTest 1/1.
Synthetic tests cover standby, HUD/result observation/time bounds, result latch and
manual transitions, pause/resume, exact age/watchdog boundaries, source identity,
unknown/malformed/future time, replay and support budgets, concurrent sticky stop
and preservation of the first terminal reason. They do not operate a device.

Root's earlier optional build/CTest passed 16/16 and default 4/4. The first
Windows manual integration fully configured, built and linked `chart_manual`
against the task-local patched gRPC 1.81.1 installation; all **14/14 CTest** passed,
including manual lifecycle, gRPC patch runtime contract and retained PAS core.
Both audio options are OFF and `VCPKG_MANIFEST_INSTALL=OFF`; this run reuses that
exact dependency installation. The standalone manual suite remains
**25 groups / 1,311 checks**. Vendor TLS deprecation warnings remain visible in
the full build log; no repository warning suppression was added.

The linked executable SHA-256 is
`498142e2174573ac963a0e0e1914a593c9d8b9daa62b20b2119a8ff21eab3454`.
Configure/build/CTest logs and their hashes are archived privately at
`../local-assets/phigros-4.0.1/m7-manual-build-001/`. This proves the manual
target's link and contract checks; gameplay input qualification remains pending.
The later probe-local fixture adapter integration passed **15/15 CTest** and
default **4/4**; its separate [build manifest](../../local-assets/phigros-4.0.1/m6-probe-build-002/manifest.json)
does not change the identity or historical 14-test transcript of the earlier run.
The asynchronous foreground watcher build used for actual round 002 passed
**16/16 CTest**. Its [build002 archive](../../local-assets/phigros-4.0.1/m7-manual-build-002/manifest.json)
contains the exact executable, 14 source snapshots, configure/build/CTest logs,
and a subsequent fresh default **4/4** LastTest log. That default build/test ran
during round 002; no separate configure/build transcript was available to archive.

## Read-only paused-screen device check

The same executable completed a bounded **3-second** observation while the
existing Phigros instance was paused, with input/audio/automatic Play disabled.
All 157 processed observations matched only the paused template and remained
`STANDBY`, round 0, inactive; no round started or result was confirmed. Capture
reported 168 valid / 167 published observations, one source-sequence gap,
10 consumer skips, zero pool drops, invalid or inactive observations, and zero
relative-stale drops. These counters do not prove distinct rendered frames or
source pixel age.

At the session wall limit the manifest records `STOPPED`,
`exit_reason=session_wall_limit_aborted`, `failure=null`, and
`capture_timestamp_known=false`. The private plan, manifest and 157 journal rows
are in `../local-assets/phigros-4.0.1/m7-manual-live-paused-001/`.
This check establishes the limited paused-screen observation/stop path. Two full
manual rounds were subsequently observed in separate processes below; a multi-round lifecycle instance,
source capture-clock qualification and gameplay input remain unverified.
Separate fixture touch/release evidence is described in [M6](M6_DEVICE_VALIDATION.md).

## Actual read-only round 001: completed, with owner-loop stalls

The same `498142…` executable subsequently observed one user-started Phigros
round through confirmed result, with input, audio and automatic Play disabled.
The process exited 0; the final manifest has `failure:null`, state `RESULT`,
round 1, completed rounds 1, `result_confirmed:true`, and `session_done:true`.
No further round started. This is a delivered-UI lifecycle result, not selected
chart identity, song timing or an input qualification.

The raw [plan](../../local-assets/phigros-4.0.1/m7-manual-live-round-001/plan.json),
[manifest](../../local-assets/phigros-4.0.1/m7-manual-live-round-001/manifest.json)
and 7,410-row [journal](../../local-assets/phigros-4.0.1/m7-manual-live-round-001/events.jsonl)
remain unchanged. A bounded offline audit verifies their exact byte counts,
SHA-256, plan/manifest identity, dataset/executable hashes and final journal status.
Its [analysis](../../local-assets/phigros-4.0.1/m7-manual-live-round-audit-001/analysis.json)
SHA-256 is `13e75eb12a30bc9f3abb5fc36fa1c9206406702b690dfe907ecfdcf3d470cf41`.
The raw journal SHA-256 is `1d1e8defc062361bebd38464d87e0189411146a55c3cdf2f4292807d65274147`;
the manifest SHA-256 is `c0ee9c5162c67c4d3ed706015f420f80fec713d5967f49d53ff77f4470c3063c`.

| Measurement | Observed value |
| --- | --- |
| Host session | 166.1205572 s; QPC ns `[91157934532700, 91324055089900]` |
| New-round edge | Journal line 1,225; +35.6302893 s from session start; `STARTING`, `ui_playing=false` |
| Confirmed result edge | Line 7,410; +166.0470834 s; one `ended && result_confirmed` edge |
| First HUD receipt → confirmed result receipt | 130.4268253 s; UI evidence interval, not song duration/zero |
| Template-match distribution | 5,688 playing; 4 result; 1,718 unknown; no paused matches |
| Unknown distribution | 1,224 before the round, 494 during the active round; 52 active-round runs |
| Capture counters | 8,130 valid/published; 718 consumer skips; 14 source-counter gaps; zero invalid, inactive, pool or relative-stale drops |
| Processing cadence p50 / p95 / max | 18.2268 / 46.2041 / 479.8721 ms |
| Receipt → processing age p50 / p95 / max | 1.2332 / 2.0808 / 46.3443 ms |

The lifecycle re-entered `STARTING` 148 times after the initial round start:
97 directly from `PLAYING`, and 51 after `SUSPENDED`. All 97 direct regressions
had the previous observation at least 100 ms old at current processing; their
new observations remained younger than 100 ms. Of the 51 resumes, 48 followed
suspension with a previous observation younger than 100 ms and three were also
compatible with expiry. Two additional long gaps reset pending confirmation
while already in `STARTING`. These are not new rounds. Unknown observations
conservatively suspend the existing round, as designed.

The original CLI performs a synchronous foreground ADB check on the lifecycle
owner every second. Root identified this blocking work as a cause of observation
expiry and debounce resets. The journal supports the 100 ms age symptom but does
not record every poll-only transition or the ADB call's exact start/end, so it
cannot attribute every gap to that call. A bounded asynchronous foreground watcher
was subsequently built and observed in round 002 below; round 001 does not validate
that new binary or claim continuous playing readiness. The 100 ms UI age bound
remains in force.

All observations use `arrival_only`, `capture_timestamp_known=false`; emulator
wall timestamps remain unmapped and no source pixel age is established. Counts
of source-counter gaps on consumed rows also include consumer skips; the 14 above
is the capture manifest's producer-reported counter. The result screenshot is
hashed without score analysis. Audio remains deferred, and no chart input,
`SyncLease`, input lead or timing qualification was produced.

## Actual read-only round 002: asynchronous foreground checks

The separately built executable SHA-256 is
`145fcb9dc65a9853e0ce4f135c97312db493dc38fd5fb3b0b053689754a3ab35`.
The foreground worker owns copied ADB/serial/clock dependencies and performs no
UI lifecycle or journal work. Its policy is a 1 s poll, 2 s callback limit, 4 s
foreground lease measured from successful request start, and 2.5 s bounded join;
foreground evidence is separate from the unchanged **100 ms UI observation age**.
Failure/stale status closes the shadow session. This remains observation-only.

Round 002 began on the previous result screen. Its first 1,154 journal rows stayed
`STANDBY`, round 0, result-latched (1,099 result matches and 55 unknown). The user
retried manually; the first new HUD started one round, and the subsequent result
completed exactly one round. The process exited 0 with `failure:null`, terminal
`RESULT`, round/completed 1, inactive, and `session_done:true`. This verifies an
initial-result latch followed by a manual retry, not two completed rounds inside
one lifecycle instance.

The unchanged [raw manifest](../../local-assets/phigros-4.0.1/m7-manual-live-round-002/manifest.json)
and [independent audit](../../local-assets/phigros-4.0.1/m7-manual-live-round-audit-002/analysis.json)
are separately pinned. Audit SHA-256 is
`a90108b49b4307ae73164d507618472f7475e2e4f5b2a9d9cce8d3dec15c03bc`;
raw journal SHA-256 is `8bec54405bbd897a2c18468762d194a18626cd8b8af76f124f6f5e4b29571f57`.
The exact round001 analysis core is reused; a metadata-only wrapper selects the
archived build002 sources/binary and retains foreground policy/final status.

| Measurement | Round 002 |
| --- | --- |
| Host session | 155.3312095 s |
| New-round / confirmed-result edges | Lines 1,155 / 8,110; +24.6562218 / +155.2753410 s from session start |
| Journal / match distribution | 8,110 rows; 6,408 playing, 1,104 result, 598 unknown; no paused matches |
| Active-round unknown | 543 observations in 57 runs; suspended without completing the round |
| Capture | 8,113 valid/published; 0 consumer skips; 5 source gaps; zero invalid, inactive, pool or relative-stale drops |
| Foreground worker | 137 successful checks including the preflight seed; no failure/stale; stop joined |
| STARTING re-entry | 57: 56 resumes from SUSPENDED; one direct PLAYING regression at line 6,009 |

At that one direct regression the previous observation was 104.8281 ms old and
the new observation only 1.2922 ms old; the 100 ms expiry remained conservative.
The [observed comparison](../../local-assets/phigros-4.0.1/m7-manual-live-round-audit-002/comparison.json)
retains both runs, including the worse maximum gap in 002:

| Metric | Round 001 | Round 002 |
| --- | --- | --- |
| Direct PLAYING → STARTING | 97 | 1 |
| Consumer skips / reported source gaps | 718 / 14 | 0 / 5 |
| Processing cadence p50 / p95 / p99 / max | 18.2268 / 46.2041 / 144.2823 / 479.8721 ms | 17.3326 / 39.4553 / 52.3493 / 594.0041 ms |
| Receipt → processing age p50 / p95 / max | 1.2332 / 2.0808 / 46.3443 ms | 1.1054 / 1.7639 / 9.9833 ms |

These are two actual runs with different manual pre-roll and unmeasured host load,
not a controlled benchmark. Root also ran a roughly 4.66 s default build/test
during 002. Fewer expiry regressions do not qualify continuous playing readiness:
unknown still suspends, a large gap remains, and source pixel age is unknown.

A later independent review found a separate watchdog boundary: when the owner
does not poll during an old lease's expiry, a returning worker could overwrite
the old successful request before detecting the gap. Build003 now checks the old
lease before starting a callback and before accepting its success; expiry stays
latched without relying on an owner poll. **31 foreground-watch checks** and the
full **16/16 CTest** passed offline. The separately archived executable SHA-256 is
`652e1c92727bb6126174e5b0387aa5a678604cd1ba65ff689fa85224d1c58031`;
[build003 manifest/sources/logs](../../local-assets/phigros-4.0.1/m7-manual-build-003/manifest.json)
preserve that identity. Default 4/4 was not rerun for this revision; the earlier
fresh result remains in build002. Round 002's successful observation does not
exercise or retroactively validate this correction; no third song run is claimed.
Input/audio/automatic Play, selected-chart identity and song-time qualification
remain disabled/unverified throughout both observed runs. The user's subsequent
Dlyrotz IN13 AP goal is pending in the [implementation plan](CHART_DRIVEN_IMPLEMENTATION_PLAN.md),
and does not change either recording's observation-only scope.

## Recorded-frame integration

The existing Dlyrotz observation recording was decoded through the actual C++
screen classifier and the frozen lifecycle, using all **4,789 video frames**.
Recording PTS was reused only as a simulated arrival clock. Each actual decoded
frame had a unique ordinal provenance ID. This is an offline replay of a known
recording, not a new live observation, independent heldout detector validation,
host/capture clock calibration, audio processing, input test or score analysis.

The private report is
`../local-assets/phigros-4.0.1/m7-manual-recording-001/summary.json`, SHA-256
`774c3efbf04d3a90d33c4e95dd473e16ae1ef3dbf3639740a23d47859745613e`.
Its source-file hashes and unchanged-during-decode check bind the tested C++
implementation. With an explicit 20-round budget, the replay produced:

- One new round at zero-based frame ordinal 67, PTS 3.123 s, in `STARTING` with
  `ui_playing=false`; recognizing the first HUD did not immediately confirm Play.
- One confirmed result end at ordinal 3,576, PTS 133.846 s. The result tail spans
  1,213 lifecycle `RESULT` frames including confirmation; it did not start another
  round. Final status is round 1, completed 1, inactive, result latched.
- Classification counts: 3,181 playing, 1,236 result, 372 unknown, zero paused.
  Unknown frames suspended the active round without completing it. The recording
  does not test pause; portable synthetic tests cover that transition.

There are repeated HUD recognition losses in this recording. The replay verifies
the start/end/latch integration and its conservative suspension behavior; it does
not establish a continuous reliable playing gate for live input. The final
100 ms expiry poll retained the last frame's sequence/timestamps and result latch.
