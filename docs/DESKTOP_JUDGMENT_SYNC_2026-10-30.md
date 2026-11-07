# Get Ready!! / Re: birth judgment investigation

This record is separate from the completed native013 Random3 evaluation. The
new investigation has not inherited that evaluation's completion or scores.
Private charts, audio, calibration and live recordings remain under ignored
local directories; none are included in this document or committed sources.

**Native021 completed a new consecutive batch of three actual desktop
All Songs/Random rounds.** All2799 source notes /7373 steps completed, each
round passed13 input /6 capture checks, confirmed result and kernel zero,
and retained only its result PNG. Judgment quality remains unresolved:
the first round had24 Bad /149 Miss, the second was FC with one Good, and
the third had1 Bad /3 Miss. This is operational completion, not an all-Bad/
Miss fix, universal stability or AP qualification.

Native021 is now archived and installed with the mixed389-row library
described below, after fresh34/34 Windows and5/5 portable tests. The Incyde
regression acquired its first note and completed all1648 notes /5035 steps
with13+6 audits passed; its result was **1505 Perfect /67 Good /6 Bad /
70 Miss,884099 A93.97%**. The zero-touch failure did not recur in that one
regression, while judgment quality remains unresolved. The
preceding native019 targeted Get Ready!! regression completed
with **812 Perfect / 5 Good /
1 Bad / 1 Miss, 986355, Max Combo741**. Bad/Miss elimination and repeatability
remain unresolved. The earlier native019 Random3 attempt failed: native019
batch2 completed With You and PRAW, but its third draw stopped before touch
because the first-note dispatch lead was below200ms. Those two results remain
historical completions, not a continuing2/3 streak. The recorder reset to
**batch3 / streak0** in the019 recorder. Native020 passed fresh Windows34/34
and portable5/5 tests. Its Random batch1 completed Infinity Heaven with poor
judgment quality, then Incyde stopped before any touch; the recorder reset
to batch2/streak0. The separate native021 Random3 is now **3/3**.
AbsoluTe disoRdeR completed operationally but had
**149 Miss /24 Bad**; もぺもぺ completed with one Good and zero Bad/Miss.
These results do not establish accurate judgment timing or an all-Bad/Miss
fix. The three new native021 results are recorded below.
The focused Khronostasis regression acquired the first note and completed
the full round with13+6 audits passed, but its result was **575 Perfect /
280 Good /1 Bad /5 Miss,848432 B87.92%**. This resolves the previous zero-touch
outcome in that one regression while judgment timing remains poor. It does
not count as a Random draw. Targeted results and the older native013 Random3
do not count toward the new streak.

## Evidence and limits

The native input/capture audits distinguish command queue acceptance, actual
gRPC Write, RPC Finish, kernel contacts, and the game's own result screen.
Passing those audits does not establish AP or identify an individual Miss.
An independent Re: birth kernel trace contained every requested Flick DOWN,
both MOVE vertices, endpoint and UP. This rules out missing device events in
that trace, while game update sampling and callbacks remain unobserved.

An optional numeric HUD observer was added in native014. It reads actual
playing frames in RAM and retains bounded score/combo text and original frame
metadata after release. It neither retains intermediate images nor supplies
input permission, synchronization or game judgments. Normal desktop play
keeps this diagnostic disabled and persists only a confirmed result PNG.

Six conditional reset groups in the Get Ready!! Drag30 control all contained
Flick, but individual members were ambiguous. Only the two Flick/Drag pairs at
68.873239437s had identical native points; the other groups' simultaneous
peers were 566–975 reference pixels apart. Their projection and path geometry
did not distinguish them from the reference population. These observations
do not prove that every failure was a Flick failure or a same-point conflict.

## Controlled experiments before native017

All entries below were actual Computer-started, complete native rounds unless
explicitly excluded. They are controlled regressions, not new Random draws.
All complete entries passed the independent 13 input and 6 capture checks.

| Track / candidate | Perfect | Good | Bad | Miss | Score | Decision |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Re: birth 48px / 8-20-24ms | 934 | 3 | 0 | 2 | 938823 | reference |
| Re: birth 48px / 30-60-24ms | 922 | 5 | 0 | 12 | 918450 | rejected |
| Re: birth 72px / 8-20-24ms | 923 | 14 | 0 | 2 | 957391 | no Miss benefit |
| Get Ready!! 48px / 8-20-24ms, user retest | 796 | 8 | 7 | 8 | 913162 | reference |
| Get Ready!! above-Drag30, 8-20-24ms | 811 | 1 | 0 | 7 | 920495 | reference |
| Same + shared-head Flick delay32 | 759 | 45 | 6 | 9 | 906380 | rejected |
| Same + Flick DOWN32ms earlier only | 776 | 18 | 1 | 24 | 884408 | rejected |
| Same + Flick release24→60ms only | 805 | 5 | 0 | 9 | 929335 | not adopted |
| Native016 strict anchor, Drag30, 8-20-24ms | 805 | 4 | 0 | 10 | 908474 | Miss remains |

The user retest's max combo is **268**, confirmed by its saved result PNG and
score formula. An earlier local score transcription said 258; that original
record was preserved and the forensic report records the correction.

A source clock discontinuity round, paused initial-state rejection, user-stop
round and unarmed accidental retry were excluded from comparisons. The
discontinuity was matched to a Windows system clock adjustment; its guard was
retained. Failed and stopped artifacts were not relabeled as successful runs.

## Opening synchronization correction

In the rejected delay32 round, the last two opening supports advanced by
31.198700ms of host receipt time but only 16.727457ms of visual native time.
Their inferred origins differed by 14.471243ms. Each native interval had about
4.950894ms half-width, so the two original origin intervals did not intersect.
The previous ±60ms compatibility band nevertheless accepted the pair. The
corresponding differences in the two reference rounds were 0.246043ms and
0.118326ms. Some newly observed Good judgments also preceded the first changed
Flick Write, preventing attribution of all additional Good to the Flick change.

Native016 replaces that compatibility band with the actual intersection of
the current native interval and the previous interval translated by the
measured host receipt gap. A disjoint pair continues acquisition from the
latest frame. It does not rewrite clocks, renew old observations, qualify GPU
pixel age, or assume a measured game clock. Source aliases, distinct frames,
motion, freshness, dispatch lead, epoch/geometry and finite budgets remain.
The geometric opening model and existing chart/profile fingerprints are
unchanged. Native016 passed fresh Windows 34/34 and portable 5/5 tests.

## Native017 candidate provenance

Native017 introduces an explicitly selected
`judgment-anchored-diagonal-sampled-v2` policy. It adds one certified middle
vertex to the old discrete certificate, preserving the old enum values,
default paths and absence-of-middle fingerprint transcript. The selected
candidate is 96 reference pixels with lead8/travel36/release24ms: DOWN at J−8,
MOVE at J+4, J+16 and J+28, then UP at J+52. The three MOVE samples span24ms.
This tests sampling and gesture-history hypotheses; it does not claim known
Phigros update frequency or callback coverage. No failed candidate is enabled
merely by adding this opt-in policy.

The declared source changes relative to immutable native016 are
`include/rhythm/contact_compiler.hpp`, `src/rhythm/contact_compiler.cpp`,
`tests/contact_compiler_tests.cpp`, `apps/chart_experiment/main.cpp`,
`apps/chart_experiment/experimental_runtime.hpp`,
`tests/experimental_runtime_tests.cpp`, `apps/chart_experiment/flick_policy.hpp`
and `tests/flick_policy_tests.cpp`. Native016's preceding change was limited to
`adaptive_first_note.hpp` and its tests. Immutable runtime archives retain
exact source snapshots and fresh build/test receipts. The extraction baseline
and main branch remain unchanged.

Native017 completed fresh Windows **34/34** and portable **5/5** tests. The
initial native017 build's new compiler fixture expected a curve-specific
diagnostic for a native anchor already129px outside the inset with only
45.999999px usable tangent budget. Its correct preflight rejection was
`judgment_domain_unreachable`; only the fixture expectation was corrected.
The failed33/34 receipt remains separate from the final passing receipt.

## Native017–019 actual targeted results

These are fixed-song Computer regressions, not Random draws. Timing for all
sampled-v2 entries is lead8/travel36/release24ms, with the exact five deadlines
above. Changing96/64/80px changes the whole certified curve, not its deadlines.
All entries below completed full source/contact execution and passed their
independent **13 input + 6 capture checks**. Those checks establish their
declared transport/lifecycle scope, not per-note game acceptance.

| Runtime / track / candidate | Perfect | Good | Bad | Miss | Score | Max Combo | Early / Late |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 017 Get Ready!!96px, numeric diagnostic, above-Drag30 | 814 | 2 | 1 | 2 | 954420 | 479 | 0 / 2 |
| 017 Re: birth96px, numeric diagnostic | 932 | 6 | 0 | 1 | 977859 | 759 | 4 / 2 |
| 017 Re: birth64px, numeric diagnostic | 923 | 10 | 1 | 5 | 953408 | 587 | 5 / 5 |
| 017 Re: birth96px, normal desktop mode | 935 | 2 | 0 | 2 | 967913 | 662 | 2 / 0 |
| 017 Re: birth80px, normal desktop mode | 932 | 7 | 0 | 0 | 997652 | 939 | 2 / 5 |
| 017 Get Ready!!80px, above-Drag30 | 818 | 0 | 1 | 0 | 977045 | 640 | 0 / 0 |
| 018 Get Ready!!80px, all33 below Drags advanced, Retry | 766 | 45 | 2 | 6 | 934951 | 500 | 7 / 38 |
| 018 Same, fresh selected-song Play | 688 | 124 | 4 | 3 | 898950 | 445 | 16 / 108 |
| 019 Get Ready!!80px, one closed below-Drag source | 812 | 5 | 1 | 1 | 986355 | 741 | 2 / 3 |

The Re: birth80px result is an actual **native017 full combo**, not AP and not
a native019 repeat. Native019 reuses that exact profile/compiled fingerprint
and passes a fresh offline check; this does not transfer its game result to
the newer executable. The64px candidate and both all33-below-Drag rounds were
not adopted. The final one-source regression also retains a Bad and a Miss;
its higher score than one reference round is not proof of a general fix.

## Native018 and019 Drag policies

Native018 appends `advance_below_drags=false` independently of
`advance_above_drags`. Enabling it requires a declared positive pre-judge
window no greater than30ms. The compiler evaluates the earlier endpoint on
the actual native line geometry, retains complete DOWN/UP duration and all
source identities/native judgment times, and preserves the existing native
fallback if the forced earlier geometry is unreachable. The runtime requires
explicit permission and the `advance-below-drags-candidate-v1` provenance
token. It does not require a source with a valid native fallback to be moved.
False adds no fingerprint, strategy or prepared-profile field.

Moving all33 below Drags changed only those33 compiled contacts; the other786
contacts and ownership records matched the native017 reference. The earliest
changed DOWN was `/judgeLineList/0/notesBelow/0`, native27.887323944s to
dispatch27.857323944s, with geometry re-evaluated. Both actual018 results were
worse, particularly in Late Good counts. They remain failures of the proposed
judgment improvement; this policy was not enabled across the desktop library.

Native019 appends the opt-in `below_drag_source_ids` closed filter. A nonempty
filter requires `advance_below_drags=true`; it accepts at most2048 unique
strings of1–512bytes. Every entry must resolve to an exact
`chartSHA256#JSON-pointer` of an original below-side Drag. Unknown IDs, wrong
chart identity, wrong kind/side and duplicates reject rather than being
ignored. Sorting makes the nonempty filter fingerprint canonical. The
`below-drag-source-filter-candidate-v1` fingerprint/provenance token is added
only for a nonempty filter. Empty preserves native018 all-below behavior when
the boolean is true, and old behavior when it is false. The original
geometry-failure fallback remains independent for unselected sources.

The installed Get Ready!! profile selects only:

```text
a0322de283b72be7ddfbf9a646d3b38292ba312988c82f2d18944416f15b393b#/judgeLineList/0/notesBelow/30
```

That Drag's native time31.901408451s is retained; its dispatch is31.871408451s.
The next Tap `/judgeLineList/0/notesBelow/31` remains at32.112676056s, changing
the dispatch separation from211.267605ms to241.267605ms. All other818 contacts
and ownership records match the native01780px control; the full819 notes /
4011 steps remain present. This is a source-isolated candidate, not proof that
the Drag caused the reference Bad. Max Combo640 permits a Bad near source179
or641, and delayed Hold judgments prevent assigning a result from rank alone.

For018/019, changes relative to the preceding archive are limited to
`contact_compiler.hpp/.cpp`, their tests, experimental `main.cpp`,
`experimental_runtime.hpp` and its tests. Profile/CLI/compiled metadata and
runtime validation carry the same opt-in values. The019 archive verifies all
29 critical source pins and preserves199 other source files byte-for-byte.
Both versions completed fresh Windows **34/34** and portable **5/5** tests.
No PAS/proto/third-party sources or main-branch content were changed for these
policies. Old default fingerprints and absent optional fields are preserved;
old binaries, failed preparations, failed tests and game rounds remain saved.

## Forensic bounds retained

The native01796px Re: birth numeric trace has a stable score806629/combo759
before the next two Flick DOWNs. Afterward it has score807588, followed by
low combo3/4/5 and the final single Miss. With the independently observed
first long streak and the matching score arithmetic, this narrows the reset
to the simultaneous Flick pair at127.193877551s:
`/judgeLineList/0/notesAbove/420` and `/421`. It does not identify the missed
side. The earlier short Hold groups end145.061224ms before these DOWNs; no
other contact overlaps the pair.

Both paths have zero projection, separate native anchors604.8px apart, and
static native line geometry. All10 commands were written; the three MOVE
messages each contain both distinct pointers. Maximum transport start
lateness was0.378676ms, and the actual UI receipt ages were22.911–83.0414ms.
The entire gesture lies inside a154.5953ms source-receipt gap, the largest in
that input period. This is a capture-delivery observation, not proof of a
game-render stall: an earlier successful solo Flick also lies entirely inside
an80.9802ms receipt gap. The older72px kernel trace is a different round and
cannot supply missing callbacks or cached-axis state for this round.

Absolute cross-round comparison used
`received_QPC_ns - source_Unix_timestamp_us*1000`, one common native017
Get Ready!! minimum, exact integer subtraction and no per-round fit/reset.
Median offsets above that same minimum were4.3001ms for017 Get Ready!!,
4.1434/4.0041ms for the two018 rounds,4.5211ms for017 Re: birth FC and4.7564ms
for the older rejected delay32 round. Minima differed by less0.22ms. These
records do not show an overall request-stamp-to-receipt latency increase in
the poor018 rounds. Source timestamps are Unix metadata obtained before
pixel readback, not a color-buffer producer timestamp or proof of pixel age;
see [capture clock limits](M4_CAPTURE_CLOCK.md). Several exact opening support
timestamps were not retained by the pre-arm100ms journal, and are reported
as missing. No source-clock mapping or new synchronization qualification was
inferred, and native016's strict opening intersection remains in force.

The optional native014 numeric observer remains diagnostic only: below-normal
worker, bounded RAM-only ROI copies, latest-only nonblocking submission,
100ms sampling cadence and at most7200 metadata rows. Strict parsing does
not map ambiguous letters to digits. It is disabled in the normal desktop
regressions and installed launch flow. The native015 shared-head delay32,
longer Flick travel, earlier DOWN and longer release candidates remain
opt-in/rejected as recorded above. No historical receipt renews a fresh input
permission. Actual UI500ms / step lateness20ms limits, epoch/geometry checks,
full source counts, configured ten-contact capacity, user stop and release-all
remain unchanged.

## Historical019 installation snapshot

At the2026-10-30 installation snapshot, the native019 archive contains226
artifacts /33,635,516bytes. The executable and all DLL/source/build receipt
pins were verified before installation; the previous configuration is saved
under `local/manual-config-history/`. The fixed desktop executable/frontend
remain unchanged. Normal play still requires manual song selection, READY
and game Play, and retains only a confirmed result PNG.

| Installed artifact | SHA256 |
| --- | --- |
| `local/manual-launch.json` | `82929afe73e8608f65013cc7cc2071275d6140c152293450ee1c95f8bdf119ab` |
| `local/library-sampled-v2-below-filter-probe-001/test-library.json` | `c1347a43731834bc7fc8d3163f27427ab3e3018040f6135f7bd64e6d78104187` |
| native019 `bin/chart_experiment.exe` | `50c46df0f7226dd0906389af6abf271fc4e18e0904f309782b4637fb84bfe1d7` |
| native019 archive `manifest.json` | `c5142227bcef316c8d578ecfdb5e11f4900bf4d8334e6e5ce4ebf715289066a7` |
| `source-filter-build-001-receipt.json` | `6be5bcda272f4a41ce96f7c31d73a7e4236939d62679d1f2dd9569ca17e6f263` |

The library remains mixed:374 prepared profiles (326 IN /48 AT), plus15
`blockAreaList` rows marked unavailable. Get Ready!! and Re: birth use sampled
v2 at80px/8/36/24ms; the other372 prepared profiles retain their prior48px
diagonal-snap-v1 data. The other387 library rows, including all15 blocked
rows, remain exact. For019, Get Ready!! had one new offline prepare/check;
Re: birth reused its exact017 profile and compiled file with a fresh019
offline check. No full374-profile re-preparation or live qualification is
claimed by this installation.

The new evaluation is recorded separately in
`three-sampled-source-filter-progress.json`, initially batch1/streak0/target3.
A new Random failure resets this streak; no targeted result or historical
native013 completion can satisfy it. Further game results must be appended
with their actual runtime/profile/round identity. That earlier campaign did
not complete its three-song streak. The later native021 completion below
retains its own runtime/profile identities and does not imply stable FC,
AP or zero Bad/Miss.

## Native019 Random batch2 failure and native020 correction

The real desktop READY → game Play path completed the first two batch2 draws:

| Draw | Track | Score / accuracy | Perfect / Good / Bad / Miss | Max Combo | Early / Late | Outcome |
| --- | --- | --- | --- | ---: | --- | --- |
| 1 | With You IN13 | 958857 S /99.61% | 769 /3 /0 /2 | 483 | 1 /2 | Full round;13+6 audit passed |
| 2 | PRAW IN15 | 925196 S /98.59% | 1146 /13 /1 /11 | 444 | 11 /2 | Full round;13+6 audit passed |
| 3 | Khronostasis Katharsis IN14 | No completed result | 0 DOWN /0 UP | — | — | Opening rejected; kernel zero |

Round directories are respectively
`catalog-manual-panel-20261030-110953-942-11813d`,
`catalog-manual-panel-20261030-111446-558-fca7b7` and
`catalog-manual-panel-20261030-112006-537-ec7eb7`, under
`local/manual-panel-001/runs/`. The third round's actual native failure is
`first_note_trigger_cancelled: first_note_dispatch_lead_below_200ms`.
It did not execute any of the declared861 notes /3545 compiled steps and
must not be counted as a completed song. Earlier batch1's unsettled Random
title also stopped before touches and remains separately recorded.

Recorded opening candidate metadata exposes an earlier viable782→788 pair:
126.6762ms receipt separation and a strict joint native interval
[2851153746,2852147123]ns, leaving a predicted269.891ms lead at processing.
The adjacent-support-only implementation did not acquire until799→801,
when the remaining lead was approximately−28ms. These are metadata/solver
diagnostics, not new game input or a source-pixel-age qualification. The
original late rejection and zero-touch result remain unchanged.

Native020 changes only `adaptive_first_note.hpp` and its tests. The installed
fix keeps at most16 actual supports for at most200ms, retains all compatible
rate-one interval branches across eligible historical/current pairs, and
requires one motion-valid actual pair to cover the full selected union.
Aliases are not discarded to choose a favorable pair, and unexpired supports
are not silently evicted when the history budget is full. Fresh current
evidence,20–200ms pair separation, distinct frames, motion,100ms anchor age,
200ms dispatch lead, epoch/geometry and finite pair/history budgets remain.
Native016's strict interval intersection is preserved. No source clock,
Flick/Drag policy, chart identity, profile fingerprint or input deadline is
relaxed by this correction.

Native020 packaging and fresh Windows **34/34** / portable **5/5** tests have
completed. Its archive contains224 artifacts /33,606,449bytes and verifies29
critical pins. Of205 archived source files,203 match native019 exactly; only
the two opening header/test files changed. Compiler, library/profile data,
Flick/Drag options and the fixed desktop frontend remain unchanged.

| Current020 artifact | SHA256 |
| --- | --- |
| `local/manual-launch.json` / `three-opening-support-configuration-batch01.json` | `9ce07639b811b9e4b135eb97d3c7e7118239c9d2ad2b53f1f1749b4ee44f217a` |
| native020 `bin/chart_experiment.exe` | `900f0969d20602fa2a7fcb837c0ffd54256696667ff1e226428289e33b112e47` |
| native020 archive `manifest.json` | `78796ceaad02f6490f6c9ecd161dced45ec87d8356bc2b308e86f3f42181d99c` |
| `opening-support-build-001-receipt.json` | `2146354d648b9f6fa52d7b8079d061916cafc48e747963304af54b6e15324c0c` |

The focused GUI regression is
`catalog-manual-panel-20261030-113112-286-b5cfff`. It entered first-note
acquired/playing after game Play at11:31:47; an actual Computer observation
at11:32:47 showed ongoing score363444/combo139 and touch circles. Its final
Computer result was **848432 B /87.92%,575 Perfect /280 Good /1 Bad /5 Miss,
Max Combo492, Early278 /Late2**. All861 source notes /3545 steps completed,
and the independent13 input /6 capture checks passed. The earlier
zero-touch opening failure did not recur in this regression, but the large
Early Good count is a material judgment-quality failure. Full input and
release confirmation must not be presented as accurate timing or zero Miss.
This fixed-song regression is explicitly outside the new Random count.

`three_opening_support20.py` initialized the separate
`three-opening-support20-progress.json` recorder at batch1/streak0/target3.
The native020 first Random draw, Infinity Heaven IN13, completed all1029
source notes /4430 steps with13+6 audits passed in
`catalog-manual-panel-20261030-113630-381-122679`. Its actual result was
**752988 C82.14%,582 Perfect /405 Good /6 Bad /36 Miss, Max Combo141,
Early4 /Late401**. This is operational completion with poor timing quality.
The next Incyde draw stopped before touch, so native020 batch1 failed and
the recorder reset to batch2/streak0. Infinity Heaven is retained as a
historical completion, not carried into a new streak. The two completed
native019 batch2 songs likewise remain historical. Operational completion
means full input, result and release confirmation, not elimination of the
recorded Good/Bad/Miss.

## Incyde zero-touch failure and native021 outlined-Drag policy

The native020 Incyde IN round
`catalog-manual-panel-20261030-114335-149-04c8ec` remained unarmed after
game Play. It ended by user STOP with0 DOWN /0 UP and confirmed kernel zero.
Its1648 notes /5035 steps /peak4 plan was prepared, but none executed.
Every one of295 valid/nonempty detector submissions produced zero model
assignments. Earlier invalid/empty detector results were not in the old
journal, so these late failures alone did not identify the original cause.

The cached source identity is exact chartSHA
`948c89c6fa4df274048cd953edaf687ce92a8fa126b9d4459305adb182bc6013`.
The earliest group comprises two Drags on line20 at9.333333333s, positions
±4.2, floor33 and a zero-scroll opening. Their native y is633.6, with
horizontal line movement. The positive model/detector already covers ROI650;
the old y125–474 range is a negative-absence certificate only, and line
opacity0 is not interpreted as note absence. A preview15/actual16 label
difference did not by itself establish a new chart-byte mismatch.

Controlled metadata-only retries used a development executable, not a silent
replacement of the installed020 binary. The new journal records at most1024
pre-input detector diagnostic rows,250ms apart, without RGB/image retention
or input permission. It showed no supported yellow heads early, then
`merged_horizontal_head_component` /`too_many_yellow_centers` during the
actual earliest-head window. Post-window valid candidates could not repair
the already missed opening domain.

Three explicit opt-in detector prototypes preserve the failures separately:

| Prototype | Model / detector | Actual finding | Result |
| --- | --- | --- | --- |
| 1 | v11 /outlined-v1 | Fixed3px rail tube rejected an actual162×13 head as an interior bridge | 0 input; rejected |
| 2 | v12 /outlined-v2 | Two-band proof with height-derived cap tube rejected the wide halo; later halo and inner bar appeared as two centers for one head | 0 input; rejected |
| 3 | v13 /outlined-v3 | Unique original outer-halo /solid-inner ownership, outer-only contour proof, inner center | Full1648/5035;13+6 audits passed;884099 A /70 Miss |

V13 is available only with explicit `outlined_drag_heads=true` and a pure
earliest-Drag group. The default false path retains old detector/model hashes.
It builds bounded connected components from original yellow pixel runs.
A composite requires one uniquely owned solid inner component, occupancy
at least.72, strictly enclosed by one halo. Left/right gaps must each be
4–24px, top/bottom gaps2–6px, width/height differences8–48px /4–12px,
center distance at most1px and per-axis gap asymmetry at most2px.

The halo is proved on its own original component mask: exactly two central
colored row bands, a fully empty central gap, a full long rail per band and
original connected cap paths. Its cap tube is derived from the observed
horizontal gaps plus2px, at most26px; an unsupported wide halo cannot use
that tube without a uniquely matching core. The emitted center is the
original inner component's bbox center. Shared/multiple ownership,
unqualified nesting, clipped/unsupported components and internal bridges
reject the entire frame. No center-distance deduplication, transitive cluster
or extra-source drop is used. Existing standalone solid/closed contours
retain their separate proof, and every qualified component is accounted for.

Bounds remain16384 runs,4096 components,16 qualified components,8 observed
heads and4M contour work. Invalid-frame metrics are bounded numeric bbox /
band diagnostics only; solver centers are cleared. Synthetic tests cover
two halo/core heads, standalone behavior, missing caps, gap/offset bounds,
shared-core ambiguity, bridges and extra components. The focused result is
**50 groups /308 checks**; the fresh full build is **Windows34/34,
portable5/5**. These tests are not real-pixel/game qualification.

Relative to archived020, exactly four source files change:
`adaptive_first_note.hpp`, `tests/adaptive_first_note_tests.cpp`, experimental
`main.cpp` and `adaptive_opening_detector.hpp`;201 other archived sources
remain exact. Native geometry, chart/source identities, compiler/contact
bytes and all freshness/lead/lateness gates remain unchanged. The candidate
Incyde profile changes only the opening model hash and explicit opt-in; its
1648 contacts /5035 steps /CFP
`cedb26644b3e16b9941330d9da6717ce45f0596db82a84e5e1a880b0651e1aa0`
remain byte-exact. The other388 library rows remain unchanged.

| Current021 artifact | SHA256 |
| --- | --- |
| `local/manual-launch.json` / `three-outlined-drag21-configuration-batch01.json` | `f7a6e4731d34a0521a3013a07f0241092c4fbc59d9853b5a96be431aa2ab02cf` |
| native021 archive `manifest.json` | `d926ef579151aa7956e7e27f772970d7644008ccd5184621b8c0b3bda8a36941` |
| Fresh full-build executable | `c387c933a3c4a139bd0432339ee2539b348f7a884afff9a07f3f5e9df9806755` |
| `outlined-drag-build-001-receipt.json` | `7e5576ecf333d85b482843b4809edfd3144441c85c1a7678fb1b58c0d4b60794` |
| Candidate003 library | `b7f437804f1ed44286af2b2759a52809a0a3e9357fd948748f08f8c3d87aec42` |
| Candidate003 Incyde profile | `87d2e500f2ab88b0db3dd2a030589a79c4d61eb6eeb511cea2f959003bc86cc8` |

The actual v13 regression is
`computer-outlined-drag-incyde-prototype3-20261030-122853-d18dde`, game Play
at12:29:14. FIRST NOTE ACQUIRED and an intermediate Computer observation of
score235722 with touch circles first established progress beyond the old
zero-touch opening. The final actual result is **884099 A /93.97%,
1505 Perfect /67 Good /6 Bad /70 Miss, Max Combo633, Early35 /Late32**.
The full1648 source notes /5035 steps, result and release completed, with
13 input /6 capture checks passed. This is a targeted operational recovery,
not accurate judgment timing, FC or an all-Bad/Miss fix. Prototype1/2
source/binary evidence and their failed rounds remain immutable under
separate task-work evidence directories.

Native021's installed archive contains224 artifacts /33,659,649bytes and
verifies29 critical source pins. Current library composition is **371
unchanged baseline profiles +2 sampled-v2 profiles +1 outlined-opening
profile**, plus15 blocked rows. Incyde retains its baseline48px Flick policy;
the outlined change concerns opening recognition only. The new library has
388 rows exact to c134, or386 exact to the original a527 library; the three
changed rows relative to a527 are Get Ready!!, Re: birth and Incyde.

The original unused021 progress file remains empty at0 as historical
metadata. The corrected recorder
`record_three_outlined_drag21_corrected.py` writes
`three-outlined-drag21-corrected-progress.json`; this is a tooling-metadata
correction, not another game run or runtime change. Its new batch1 streak is
**3/3**, completed consecutively in batch1. All three rounds used
the real desktop READY /game Play flow, completed full input/result/release,
confirmed kernel zero and passed13 input /6 capture checks. They retain only
their result PNGs. No targeted or historical completion satisfies this streak.

| Native021 Random draw | Track | Score /accuracy | Perfect /Good /Bad /Miss | Max Combo | Early /Late |
| --- | --- | --- | --- | ---: | --- |
| 1 | AbsoluTe disoRdeR IN16 | 823225 B /86.21% | 1291 /136 /24 /149 | 757 | 70 /66 |
| 2 | もぺもぺ IN11 | 999235 V /99.92% | 411 /1 /0 /0 | 412 | 0 /1 |
| 3 | Dead Soul IN14 | 944441 S /98.82% | 768 /15 /1 /3 | 433 | 1 /14 |

The first round is
`catalog-manual-panel-20261030-123938-134-95d58e`, with1600 contacts and game
Play at12:40:33; its149 Miss and24 Bad are a material judgment-quality failure
despite operational completion. The second is
`catalog-manual-panel-20261030-124813-712-0bcc80`; it is an actual FC, not AP.
The third is `catalog-manual-panel-20261030-125415-030-2c2588`, with787
source notes /1815 steps. Full counts across the three are1600/4660,
412/898 and787/1815, totaling **2799 notes /7373 steps**. The corrected
recorder stores `active_batch=1`, `completed_streak=3`, `target=3`, with
all three actual round records marked passed. The first round's149 Miss /
24 Bad and the third round's1 Bad /3 Miss remain material quality limits.
The completed Incyde v13 target also now passes the stronger outlined-policy
specific13+6 audit, independently of its preserved baseline audit; its70 Miss
remain unchanged. This new Random3 operational evaluation is complete;
judgment-quality improvement and whole-library gameplay coverage are not.

An emulator Google prompt briefly appeared during setup. No login,
verification or authentication data was entered. Closing/reopening the
existing desktop Phigros entry restored the game via its normal startup
path; this does not add an authentication operation or security change.
Timing and Good/Bad/Miss quality remain unresolved.

The following metadata reports remain in the local task work directory
`C:/Users/wurre/Documents/Codex/2026-10-28/new-chat/work/`:

| Evidence | SHA256 |
| --- | --- |
| `getready-below-filter-production-audit-001.json` | `679e4bd5d7a63ff99c542b278da3fa52530c95e251ba83b33129894eac846161` |
| `rebirth-sampled-v2-80-production-audit-001.json` | `526fc934d48710d9de739646682838f3ac17d2d34b0ecb19839b68aca5acefac` |
| `rebirth-sampled-v2-unique-reset-review-001.json` | `17d4893d2a81e3e46f8ccad939b3024fc72739015a8fecb76cbb7bd088fcfdca` |
| `crossround-absolute-source-receipt-offset-001.json` | `53e8a766baa38482f52d6b9ac88270611974408e59c4fd2709d8fec56b7cfed2` |
| `three-sampled-source-filter-batch02-round01-audit.json` | `8e89125fea743acf112f0f8047dee852f17c282b98bece88347185bf8864b566` |
| `three-sampled-source-filter-batch02-round02-audit.json` | `c879519cab74095ff5dbbf906b664d8db808413d6c918a15f4d5c6021fdc85ad` |
| `khronostasis-opening-lead-failure-forensics-001.json` | `aa7081231d43f888bc70b15f5d7b4c9aa5dca9f38c2d194b0e3fc28555d3bb0e` |
| `khronostasis-opening20-regression-audit-001.json` | `c19d681e28d8906616dc5b65ba3ee3b308ba3fe239452d97b2bc1ce57e4f41d5` |
| `three-opening-support20-batch01-round01-audit.json` | `f138b5f57641416fe4fa4d4a3c9ad1bcf3ac1e19bfd01497c25cd70c73511a1d` |
| `incyde-opening-no-input-readonly-review-001.json` | `648cffad48ac5d5a2e0d030c2f507811df1be3d8efccd7a0c54363bf1d19e018` |
| `incyde-outlined-prototype3-baseline-audit-001.json` | `f0bbf29517c04396b15e8f151f632837d1669d135fe5ed0e2e1e067731dfcfc7` |
| `incyde-outlined-prototype3-specific-audit-001.json` | `6bb4336259f6b35a94a4c69da5fbe5feb908183af1c9e69d37c5174569904237` |
| `three-outlined-drag21-batch01-round01-audit.json` | `ed7d8ab4c4e0d85f40c400d471f5ecb37ff1d710ddafb019efcdd90c2f66cdc9` |
| `three-outlined-drag21-batch01-round02-audit.json` | `e6a54f3e2b078382923b3ce2fcec749940a4489be9aac862232a5706541a914d` |
| `three-outlined-drag21-batch01-round03-audit.json` | `18421d6135922c6824c87bbced8f9327e16cbf36e415c1de48cc76cc6ce8080f` |

These local evidence paths and archives are not fetched by a source clone.
This documentation update ran no new build, tests, UI or device actions.
