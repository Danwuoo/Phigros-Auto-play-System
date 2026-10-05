# Cold v3 relation and exact-alias contract

Initial expectation freeze: 2026-10-05, before v3 relation implementation. Isolated research candidate; no production tracker, owner, device, physical-identity oracle, or game result. v2 and its historical reports stay unchanged.

## R3-1: one exact observable measurement, preserved indices

Only descriptors with exactly equal query geometry (including tap and depth), current key, support/end/rail/effect/contact facts, measured geometry, and selected relation-line witness may alias. No tolerance, hash-only equality, ROI overlap, or similar appearance establishes an alias. Lowest original index represents the group; output and attachment indices never move. All original descriptors remain in the Observation. Historical matching sees one representative per exact group; a duplicate prior ROI must not manufacture a nearest tie or two contained endpoints. Distinct and near measurements remain separate hypotheses. Current measurements separated by at most the existing 4-pixel tie bound remain unknown rather than being merged.

The new-Down mask may contain at most one member per exact group. A known-Down attachment to any member blocks new Down for the entire group while retaining Move/refresh at that original attachment. Unknown Down and completed Up never replay. This is measurement equivalence, not physical-object ownership.

## R3-2: stationary note and approaching current line

The extractor calls observe_relation_context(Observation&, span<const Line>) after it has measured every current descriptor. Its input lines must be legal measurements from those current pixels, not authoring labels, predicted positions, or stale line state. Per note, only an actual line whose finite segment meets the note's longitudinal axis within 128 pixels of its front supplies the bounded approach witness. More than one qualifying line gives no unique approach witness. Lines outside that corridor cannot supply confirmation.

Existing moving-note confirmation retains its signature/RGB deduplication. A separate stationary path may confirm the same current endpoint in at least 3 independent current RGB samples spanning at least 30 ms when the unique line approaches it by at least 1 current-pixel normal-distance unit per accepted observation. No lower count or shorter span is allowed. Timestamp, sequence number, line ID changes, background-only pixel changes, tangential line movement, and an unrelated line's movement do not themselves create independence. Replayed RGB cannot create a new independent sample. A missing, multiple, nonfinite, receding, or geometrically discontinuous approach witness breaks the stationary chain. Exact same observable endpoints at rest remain one sample.

This is a bounded conditional relative-geometry test, not proof of physical ownership or a new general tracker. It retains at most 6 samples / 90 ms, a 40 ms adjacent gap reset, source <100 ms, original plan/gate deadlines, and unknown/completed receipts. Descriptor <=256 bytes; metadata <=1 MiB; 128 notes / 16 lines. Lines, RGB, and ROI inputs remain separate legal measurement inputs; no song/chart/progress/action-history input.

## Before-patch required controls

Legal-opportunity denominator:
- A stationary Tap with its unique line approaching normally (30, 15, 0 pixels at 20 ms steps) can form one new Down.
- A stationary Hold with line approaching its front from either signed side, without crossing the front, retains visible rails/cap, and can form one new Down.
- Moving-note legacy positive remains legal.
- Exact duplicate prior ROI no longer creates ambiguity; known Down still refreshes.
- Exact duplicate current ROI creates one Down; attachment to a noncanonical alias refreshes and creates no new Down for that group.
- Separated current measurements keep their original indices and can independently act.

Safe-rejection denominator:
- Same RGB with fresh timestamps, background-only change, stationary pair, unrelated-line motion, tangential movement, line ID-only change, receding line, too-short span, only 2 samples, or ambiguous relevant lines do not enable stationary new Down.
- Near-but-distinct current ROI is not silently aliased; unresolved current competition denies new Down.
- True union of distinct historical endpoints remains ambiguous even with a current cap.
- Unknown/completed/expired/source-invalid/context-reset reject new Down; aliases do not bypass them.

Before and after use the same frozen test source and expected outcomes. The before run links untouched v2. Every added case and failed assertion remains in its denominator. Historical legacy oracle differences and remaining unsupported cases are reported separately. Runtime, Windows ABI, real-image semantics, physical ownership, and Chapter Legacy IN Miss=0 remain unverified here.

## Implemented details and explicit conditional boundary

- The interface is in `relation_policy.hpp`; `bvi.cpp` calls it after all descriptors are complete. It stores only one selected line's current center, angle, finite length, signed normal separation, ID, and absent/unique/multiple state. `Line.id` must remain equal in the stationary chain as a conservative continuity condition; ID never creates novelty. Successive selected geometry is limited to 128 px center displacement and 0.6 rad modulo-pi angle change. Line length is not an identity or novelty signal.
- The longitudinal intersection must be within ±128 px of the front and within the finite measured line segment. The approach path accepts either signed side, including the Hold-body side. Absolute normal separation must decrease by at least 1 px from the last independently counted witness; crossing the sign, any observed recession, ID switch, missing/multiple witness, or geometric discontinuity resets that path. Subpixel movement alone does not count. Every intermediate observation is checked for recession even if its displacement is too small to count.
- Legal measured-line metadata is a precondition, not something this helper independently proves from RGB. If supplied line geometry contradicts its source pixels, the upstream measurement contract has failed. Production/real-image use must establish current measured rather than projected geometry before enabling the stationary new-Down branch. The global RGB hash is a bounded replay filter, not a mathematical collision-free or source-authentication proof.
- Distinct current candidates that fall within the existing 4 px tie bound, or choose the same canonical prior measurement, remain ambiguous. They are not merged into an invented physical owner. Exact aliases retain the original `Observation.parts` and `Guard.attachment_query` indices; `canonical[]` supplies the explicit map.
- `constrain` permits Down only at a canonical original index; an active attachment anywhere in that exact group suppresses all its new Down outputs. The number of emitted Down proposals is also bounded by `free_contacts`. The current body/rails/contact guard, unknown/completed receipts, and both deadlines remain required.

### Counterexample discovered after the initial policy patch

The first frozen 35-case set did not cover a stationary Hold whose visible measured depth changes as a line occludes the cap. Before changing that logic, `novelty-probe-frozen.cpp` added two rejection controls. A02 uses fixed front y=500 with current measured lines y=500→501→499. All three endpoint measurements are visible, but changing measured depth changes the inherited descriptor signature; the inherited path counted 3 independent samples and emitted Down despite crossing/reversal. `novelty-before.json` preserves that failure. A01 is the corresponding receding control.

The minimal repair adds exact same-query equality to the existing RGB/signature duplicate test. Identical note query geometry cannot count again solely because measured depth changed underneath a line. A legitimate stationary approach still earns the separate three-observation line-relative path. `novelty-after.json` preserves A02 with current contact supported, independent=1, Down=false; the fix does not rely on losing current contact. These two previously frozen cases were then consolidated into `relation_tests.cpp`, without changing their expectations. The initial frozen source and both addendum sources remain archived.

## Author results, before independent integration acceptance

Same final source/expectations, 37 cases / 74 assertions:

| build | failed assertions | native exit |
|---|---:|---:|
| untouched v2 | 27 | 1 |
| v3 Release | 0 | 0 |
| v3 Debug | 0 | 0 |
| v3 AddressSanitizer+UBSan | 0 | 0 |

The distinct-case denominator is 37/74. The earlier 35/72 runs and the 37/74 superset must not be added together.

- Legal action opportunities: 8 cases / 31 assertions; v2 22 failures, v3 0.
- Safe rejections: 26 cases / 33 assertions; v2 4 failures, v3 0.
- Unknown/completed receipt protection: 2 cases / 6 assertions; both 0 failures.
- Exact-alias capacity boundary: 1 case / 4 assertions; v2 1 failure, v3 0.
- Original v2 independent source, unchanged expectations: 35 cases / 79 assertions, v3 0 failures. This resolves its two prior duplicate-ROI assertions without rewriting that report.
- Original v2 contract source, unchanged expectations: 48 cases / 339 assertions, v3 0 failures. The first invocation used an incorrect CLI argument count and returned 2; corrected invocation returned 0. The error and correction are recorded rather than hidden.
- `sizeof(Descriptor)=256`; `Candidate::metadata_bytes()=236,216` on this GCC/Linux ABI, versus v2 200 and 185,904. At most 128 descriptors, 16 input lines, 6 retained samples, 90 ms retained span. The new relation scratch arrays are fixed-size; no past RGB, new tracker, or unbounded owner state is retained. This is a static bound, not Windows ABI or runtime-cost acceptance.

The full original 3,938-assertion legacy comparison is owned by the central integration run. The frozen historical source/oracle/report is not changed by these results. The original baseline's remaining failures are not relabeled by this small suite.

## Reproduction and remaining scope

From the repository root, compile `relation_tests.cpp` with all four v3 translation units (`bvi.cpp`, `relation_policy.cpp`, `constraint_policy.cpp`, `contact_policy.cpp`), `-std=c++20`, the v3 include directory, and `/tmp/phigros-bvi-round2-deps`. Release uses `-O2`; Debug uses `-O0 -g -D_GLIBCXX_ASSERTIONS`; AddressSanitizer+UBSan uses `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=0`, and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer is disabled; no leak-pass claim. For the v2 before run, compile against v2's `bvi.cpp` and explicitly define `BVI_HEADER` to the absolute v2 header so the test's directory cannot accidentally select the v3 ABI.

Remaining unknowns include: discovery and authentication of current pixel-derived ROI/lines, real physical-object ownership when multiple worlds yield identical measurements, supported return/reversal behavior beyond this bounded rejection contract, exact-alias-to-production-owner mapping, real-image calibration of the geometric thresholds, complete touch-owner/FakeTouch integration, runtime cost and target Windows ABI, full-song outcomes, and Chapter Legacy's verified current IN unlock/completion denominator. No device, emulator, real touch, paid compute, network publish, commit, or push was performed by this work branch.

## Final independent-review correction: observable front displacement

The independent reviewer froze additional controls after reading source. With x=320.10/.11/.12, or tiny width/angle changes, the note+line pixels were byte-identical after removing an unrelated background tag, yet the initial v3 counted 3 samples and allowed Down. Original 44-case results and these new failed controls are retained separately.

Final moving-note confirmation additionally requires at least 1 px Euclidean front displacement from the last **counted** witness. Existing RGB/signature/exact-query replay filters remain. Comparing to the counted witness lets five 0.5 px increments accumulate three confirmations over 80 ms; an intermediate subpixel frame does not permanently reset progress. This rule does not alter stationary measured-line approach, known-Down rotation Move, or 3/30 ms and 6/90 ms limits. Width/depth/angle changes at a stationary front do not themselves grant new acquisition. Pure rotation/late alignment with no translating front and no independently earned line approach therefore remains an explicit capability limit requiring real-pixel review, not silently a solved case.

The final source freeze is `evidence/final-core.sha256`; the author's earlier freeze remains an honest pre-review snapshot. Final independent 44-case regression and the separately frozen noise/accumulation controls are listed in VALIDATION.md.
