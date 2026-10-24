# Dlyrotz opening-Tap calibration experiments

These explicit experiments are intended to calibrate one or three opening native
Dlyrotz IN13 Taps on Phigros 4.0.1 / 157. This separate executable does not change the
production SessionRunner's live gate or issue a qualified SyncLease.

The pinned chart SHA is
`aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90`.
The target `/judgeLineList/0/notesAbove/0` is at song time 12.000 s and reference
pixel approximately (352,576). The native importer and pose evaluator derive
this point, then the previously tested landscape mapping sends it through
GrpcTouch. The first-Tap mode permits one DOWN; UP is due 16 ms after the DOWN RPC
returns. The explicit three-Tap mode extends this to the following fixed sequence;
all other notes remain unplayed.

| Native song time | Reference x | Reference y |
| --- | ---: | ---: |
| 12.000000000 s | 352 | 576 |
| 12.171428571 s | 352 | 576 |
| 12.514285714 s | 928 | 576 |

The three-Tap contract consumes the already armed FirstTapTrial output and its
original arm timestamp. It preserves that fixed origin/phase and three-second
horizon; it does not re-solve candidates or apply phase twice. Each DOWN requires
an actual, distinct, still-fresh UI observation. Only contact 0 is used, and the
next DOWN requires the preceding successful UP receipt. DOWN or UP dispatch more
than 20 ms late, UI loss/expiry, changed source context, unknown RPC, duplicate
command or missing UP cancels the sequence and requires release. Successful UP
RPCs do not clear the final release obligation or prove game delivery.

The opening cyan detector supplies centers to the conditional visual candidate
producer. Full unoccluded center coverage, the nominal approach scale, and
short-term rate-one behavior are experimental assumptions. The raw frame's
host receipt is known; its actual rendering/capture age remains unknown.
An explicit phase of -100..100 ms changes the provisional dispatch point.
Positive phase delays it. This is a combined visual-arrival/render/dispatch
phase hypothesis, **not** a measured input-lead value.

The app starts on the manually verified Dlyrotz IN13 result page. The existing
manual-mode lifecycle observes retry and playing; it never clicks Play.
The agent operated retry while the user was away. The user has now returned;
selection/start is interactive again, and the next requested scope is the
[complete chart experiment](M7_WHOLE_SONG_EXPERIMENT.md), not more small trials.
The experiment uses a 90-second software session budget and one round, with
one or three DOWN attempts according to its explicit mode,
and cancels on Esc, Ctrl+C, an output `STOP` file, invalid UI, foreground loss,
geometry changes, or an expired arm/dispatch window. Every entered backend
path attempts release. Failed or unknown release is retained as a failure.

Read-only EVIOCGMTSLOTS snapshots pin the boot, event2 route, name, device
identity and 11 slots established by the actual four-finger fixture test.
Each helper request brackets a newly performed query in host time. A zero
snapshot is instantaneous kernel evidence; it does not prove future absence
of touches or game callback delivery. The one-shot helper's pre-dispatch
snapshot expires after 500 ms from request start and rejects a request/reply
span of 250 ms or more. UI freshness remains 100 ms independently.

Build003 moves this read-only query to a worker so the owner can continue
processing UI observations. The owner checks the completed reply and refreshes
its host time/UI health before claiming DOWN. This changes scheduling of the
query, not either freshness threshold or the meaning of kernel zero.

The retained `pas::adb_call` timeout bounds its ordinary process wait, but its
post-termination wait and pipe-reader join have no hard wall deadline. An
unavailable pre-dispatch reply cannot authorize DOWN; joining its worker or a
blocked final query can still delay the final manifest after release.
The 90-second session and ADB timeout values are software checks, not a hard
whole-process deadline guarantee. Kernel reply validation rejects numeric
coercion, duplicate active tracking IDs, wrong scope/identity and clock rollback;
it does not convert an unreadable or invalid reply into zero.

The output records chart/model/binary/config identities, candidate intervals,
actual receipt and RPC times, the explicit phase, kernel snapshots, cleanup,
and bounded judgment screenshots. Screenshots are written after input and
cleanup so PNG compression does not delay the planned UP. Inspect these
images for game response. A combo change or hit effect alone must not be
reported as Perfect; full result statistics or sufficient visible judgment
evidence are required. All `timing_qualified` / `live_game_qualified` fields
remain false. No AP result is inferred from this short experiment.

Build with `RHYTHM_BUILD_CALIBRATION_TRIAL=ON` plus the explicit
`CHART_IMPORT`, `VISUAL_CANDIDATES`, `SUPERVISOR` and `PAS_GRPC` options.
Default builds/tests remain offline. Run `chart_calibrate --help` for its
explicit opt-in CLI. Private charts, calibration profiles and recordings
remain outside the repository.

```text
chart_calibrate run emulator-5554 phigros CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-first-tap
chart_calibrate run emulator-5554 phigros CHART.json STATE.json KERNEL.json NEW_DIR PHASE_MS --experimental-three-tap
```

Both modes require the pinned native chart and fresh local profiles. The three-Tap
CLI verifies all three imported note times/positions against the fixed sequence.
It records planned taps, actual receipts and completed/attempted counts; successful
sequence status requires all three completions, release RPC knowledge and a valid
final zero snapshot. No mode enables the production SessionRunner live gate.

## Verification and failed trials

The conditional visual candidate solver has **52 checks**, and the opening
detector has **22 checks**. All 47 frozen PNG detector outputs matched the
independent measurements. Dense primary/heldout video replays at nominal
50/75 ms sampling could arm the FirstTapTrial contract under an explicitly
simulated PTS-as-receipt clock. These are offline feasibility results, with
all aliases retained until temporal evidence resolves them; they do not
qualify a live capture clock. See [visual candidate evidence](M4_VISUAL_CANDIDATES.md)
and its [private archive](../../local-assets/phigros-4.0.1/m4-visual-candidates-001/manifest.json).

| Build / attempt | Observed result |
| --- | --- |
| [Build001](../../local-assets/phigros-4.0.1/m7-first-tap-build-001/manifest.json) | Full non-audio **19/19 CTest** passed. The first CLI preflight rejected the missing `ChartSource.label` before device discovery; [failure retained](../../local-assets/phigros-4.0.1/m7-first-tap-build-001/preflight-failure-001.json). |
| [Build002](../../local-assets/phigros-4.0.1/m7-first-tap-build-002/manifest.json) / [live002](../../local-assets/phigros-4.0.1/m7-first-tap-live-002/manifest.json) | Required source label fixed; tested contract sources unchanged. A provisional anchor armed, but a synchronous kernel query took **109.3779 ms** and the previous UI observation expired. Trial cancelled before any DOWN/UP; final pinned kernel route was zero. [Independent audit](../../local-assets/phigros-4.0.1/m7-first-tap-audit-002/analysis.json) preserves the causal times. |
| [Build003](../../local-assets/phigros-4.0.1/m7-first-tap-build-003/manifest.json) / [live003](../../local-assets/phigros-4.0.1/m7-first-tap-live-003/manifest.json) | Asynchronous kernel worker; full **19/19 CTest** passed. Before arm, a frame was already **96.3512 ms** old at UI processing; subsequent detector/hash work crossed the candidate's 100 ms limit. Producer rejected it as stale and the trial stopped. No DOWN/UP; final pinned kernel route was zero. |
| [Build004](../../local-assets/phigros-4.0.1/m7-first-tap-build-004/manifest.json) / [live004](../../local-assets/phigros-4.0.1/m7-first-tap-live-004/manifest.json), phase **0 ms** | One DOWN/UP pair succeeded and the final pinned route was zero. [Result](../../local-assets/phigros-4.0.1/m7-first-tap-result-004/result-1.png): **0 Perfect, 1 Good, 0 Bad, 583 Miss; Late 1**, score 1173. [Independent audit](../../local-assets/phigros-4.0.1/m7-first-tap-audit-004/analysis.json) separates host dispatch, source pixels and actual result. |
| [live005](../../local-assets/phigros-4.0.1/m7-first-tap-live-005/manifest.json), phase **−80 ms**, same Build004 binary | One DOWN/UP pair and final route zero. [Result](../../local-assets/phigros-4.0.1/m7-first-tap-result-005/result-1.png): **1 Perfect, 0 Good, 0 Bad, 583 Miss; Early/Late 0**, score 1712. [Independent audit](../../local-assets/phigros-4.0.1/m7-first-tap-audit-005/analysis.json) preserves host/visual boundaries. This is one observed Perfect in a one-Tap trial. |
| [live006](../../local-assets/phigros-4.0.1/m7-first-tap-live-006/manifest.json), repeated phase **−80 ms**, same binary | One DOWN/UP pair and final route zero. [Result](../../local-assets/phigros-4.0.1/m7-first-tap-result-006/result-1.png): **0 Perfect, 1 Good, 0 Bad, 583 Miss; Late 1**, score 1173. The repeated setting **failed to reproduce Perfect**. |
| [live007](../../local-assets/phigros-4.0.1/m7-first-tap-live-007/manifest.json), phase **−80 ms** | One DOWN/UP pair and final route zero. [Result](../../local-assets/phigros-4.0.1/m7-first-tap-result-007/result-1.png): **1 Perfect, 0 Good, 0 Bad, 583 Miss; Early/Late 0**, score 1712. A second observed Perfect does not erase the intervening Good Late result. |
| [three-Tap live001](../../local-assets/phigros-4.0.1/m7-three-tap-live-001/manifest.json), phase **−80 ms** | Three completed DOWN/UP pairs are present in the [RPC journal](../../local-assets/phigros-4.0.1/m7-three-tap-live-001/events.jsonl), and the final pinned route is zero. No final result original is available for this trial; game judgments remain **unverified**, and **3 Perfect is not claimed**. |

Build004 buffers the journal
within 64 MiB / 16,000 rows, flushes after cleanup, and skips already stale
pre-arm frames before calling the producer. No age gate was relaxed. It was an
incremental CLI rebuild over the unchanged contracts tested 19/19 in Build003.
The execution manifest intentionally records judgment as unreviewed; later
result screenshots and audits above supply the game judgments without rewriting
that original manifest. The remaining 583 notes were outside the one-Tap budget.

The new three-Tap core passed **15 groups / 211 synthetic checks** with MSVC
`/W4 /WX`, and the full non-audio integration passed **20/20 CTest**. The
[three-Tap build001 archive](../../local-assets/phigros-4.0.1/m7-three-tap-build-001/manifest.json)
preserves the source, binary, dependencies and test logs separately from the
Build004 binary used for live004–006. These checks
cover per-DOWN UI freshness, original anchor lifetime, phase/rounding, single
contact ownership, UP ordering, late/unknown/duplicate actions and release
obligation. Its actual three-Tap live001 completed the bounded RPC sequence as
recorded above; the missing final result prevents a judgment claim. The user now
requests a complete-chart run without additional small-range testing. The
[whole-song build001](../../local-assets/phigros-4.0.1/m7-whole-song-build-001/manifest.json)
has **22/22 CTest**, a **11,973-check** experimental runtime suite and a
**19,650-check** private full-chart FakeTouch replay. Its first launch failed in
endpoint discovery before creating a live output or injecting notes. After the
authenticated emulator restart, [whole-song live001](../../local-assets/phigros-4.0.1/m7-whole-song-live-001/manifest.json)
was stopped on a black startup screen with zero DOWN/UP and final kernel zero.
Whole-song live002 is READY and awaits the user's manual start; its result is pending.
These tests are independent of the missing three-Tap result.

The observed single-note Perfect results do not establish a stable phase, input lead/jitter, a qualified
capture clock or multi-note support. The Dlyrotz IN13 **584 Perfect / 0 Good,
Bad or Miss AP goal remains pending**; audio remains deferred.
