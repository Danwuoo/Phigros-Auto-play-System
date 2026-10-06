# Independent hard-disabled live seam review: fixed plan

Predeclared before the new seam implementation, 2026-10-06. Only bounded
endpoint-free C++20 tests and static source review are authorized. No device,
Windows execution, live run, push, new image generator, or physical gold.

## Initial defects and missing integration

The initial source seal and excerpts remain here even after repair. Initially,
`manual-session` has no candidate selection or early UNQUALIFIED admission. Its
Packet carries only SessionObservation/result metadata, with neither the same
frame lease nor the complete CandidateBatch. Production directly owns
SessionGameOwner and does not invoke current_rails::Hook or ExecutionLedger.
The action worker's catch invokes GrpcTouch::emergency_release_all, so an
already-unknown scheduler release can be retried and its first report replaced.
The cold Runtime filters current inputs and has sticky finish logic, but that
logic is not a shared production owner step.

## Fixed controls and expected results

1. Candidate admission: only UNQUALIFIED exists; requesting the current
candidate rejects before config reading, preflight, endpoint discovery, backend
construction, or device activity. Default donor route remains selectable. A
cold-test construction tag grants no live admission and no enable flag exists.
2. Packet identity and lifetime: the lease is shared_ptr<const Frame>; complete
batch and scene contexts match that exact frame. Missing lease, mismatched
frame/epoch/generation/geometry/capture/dimensions/rotation, round mismatch,
nonmonotonic sequence/time, stale source, incomplete batch and invalid capacity
must not permit a new Down. Retaining the packet retains its frame lease; no
dangling-reference interface. The tests may copy a packet only to perturb one
contract field; they do not manufacture accepted plans, cursors or receipts.
3. Integration: current Runtime and production candidate route call the same
endpoint-free owner-step type, using the sole SessionGameOwner and a bounded
receipt decorator. No research-only reimplementation or second scheduler.
4. Fixed pixels positive/control: reuse the existing 81-phase, offset-0
cost_family_v2 corpus, with the same positive/absence/effect/no-tail-cap controls.
Positive establishes an actual owned Down and preserves active-only typed-tail
release with two fresh observations. Controls cannot counterfeit normal-tail
completion or create extra Down. No oracle changes and no new raster generator.
5. Implicit releases: UI/source/dispatch cancellation and time expiry are
recorded from the backend decorator, observed by the ledger, and streamed once.
No diagnostic-output requirement may suppress receipt handling.
An output callback exception is also a fixed failure control: ledger handling
and sticky release finalization must happen before optional output can throw.
The initial step source showing callbacks before ledger mutation is preserved
as `initial-step-review.hpp.txt`; this finding was sent before the repair.
6. Sticky release: known and unknown final release, preexisting unknown release
before and after receipt drain, repeated finish and production catch. First
unknown report remains byte-equivalent; no retry, empty overwrite, or upgrade
after the backend would later succeed. Missing/failed/unknown receipt remains
fail-closed. Negative receipt-command mismatch controls must invalidate ledger.
7. Overflow: bounded decorator overflow produces a sticky stop/failure rather
than silent loss, continued injection, or false successful finalization.
8. Production compile closure: root CMake defines exactly one pas_core and
links candidate logic into pas_emulator without another formal core. Candidate
source hashes and generated build provenance cover the actual linked closure.
Linux tests never count as a Windows compile/run qualification.

## Evidence and stopping rule

Preserve all initial and subsequent nonzero outputs. Do not edit fixed expected
results to obtain green. Final reports require source manifests checked before
and after execution against a newly compiled same-source archive. Coordinate
with the parent before any compile due to the 20 GiB free-space floor; do not
produce another large closure build. Review ends with the bounded tests and
specific static findings, or an exact blocker, while live remains NOT_READY.

## Explicit fixture-v2 correction before final freeze

The first reviewer runtime execution retained a single failure: its new
assertion expected one Down even for no_tail_cap. The already frozen original
family-v2 report establishes no_tail_cap A/B [0,0,0] with no distinct notes.
Independent review confirmed that corpus omits the cap from approach onward,
so it never supplies entry support. `live_seam_runtime_review_v2.cpp` restores
that existing zero-entry denominator explicitly. The original reviewer source,
report and nonzero exit remain preserved, and neither generator nor original
gold changes. Final tests use the separately sealed v2 source.

## Late expiry-order control, before the corrective source freeze

The parent requested one narrow sequencing check after the first complete
matrix. The unchanged corpus produced a real owned Down at 1620 ms. A fresh
capture at 1640 ms consumed at 1720 ms (age 80 ms) could renew an old 100 ms
lease in direct shared accept before polling, while Runtime's outer poll first
released it. The single paired counterexample, all original outputs, complete
before-91-check source and prepared-source diff remain in sequencing-check/.

The declared correction requires common accept to poll/sync before Hook.
The existing runtime-v2 file preserves all 91 controls and appends three checks
for each of the same two call orders: actual owned-Down boundary fixture;
expiry release before body renewal with zero active contacts; no repeated Down
and verified finish. Total runtime checks become 97, packet checks stay 63.
No renderer, original gold, policy threshold or old oracle changes. The new
source is applied only after the parent's source-boundary authorization and
requires separately named fresh complete-archive evidence.
