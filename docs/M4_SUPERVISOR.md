# M4 state and qualification supervisor

`RunSupervisor` is a portable, offline decision contract. It combines actual
state observations, audio clock/lease observations, immutable plan qualification,
and an explicit arm request. It contains no visual classifier, screen capture,
device discovery, input backend, or release implementation. A synthetic test's
qualification flags do not qualify the current real audio matcher or device for
live play.

`StateObservation` explicitly labels `playing`, `paused`, `result`, or `unknown`;
its producer is either visual evidence or a manual user confirmation. It must
carry the exact chart/audio/layout identity, geometry version, epoch, actual
sequence, provenance ID, observed time, delivery time, expiry, and timestamp
kind. `SupervisorSyncObservation` independently carries its actual sequence,
provenance, complete identity, `ClockMap`, `SyncLease`, and delivery time.
Identity fields are producer assertions tied to the selected session/manifests;
the supervisor does not infer a chart SHA from pixels or authenticate arbitrary
caller-supplied provenance.

## Evidence and time

Both producer sequences and observed times must strictly increase. Future
sample/observation times or delivery times, missing provenance, invalid epochs,
nonfinite clocks, unknown audio uncertainty, and incomplete timestamps fail
closed. `status(now)` ages existing evidence; it never increments an observation
sequence, changes an observed time, or extends an expiry. Repeated `arm` does
not extend the bounded run deadline.

State time has explicit semantics:

- `mapped_capture` means an actual frame capture timestamp mapped to the host
  clock, with a supplied uncertainty bound.
- `host_confirmation` means the host time of a real manual confirmation. It is
  not the original screen capture time or a song-time anchor.
- `arrival_only` means the visual producer knows only delivery time; observed
  and received time must be equal. Capture timestamp remains unknown. This can
  be exercised in shadow/FakeTouch mode, but cannot grant live permission.
- `unknown` is refused. Missing uncertainty is never silently replaced by zero.

Manual confirmations are allowed by default but obey a separate finite maximum
age; an effectively infinite expiry is rejected. Producers must not automatically
renew a confirmation with `now` each tick. A new confirmation requires a new
actual user action. Default budgets are 150 ms visual age, 1 s manual age,
150 ms sync age, 250 ms maximum declared sync lease, 150 ms release-ack age,
20 ms state/sync uncertainty, and a 30 s maximum armed run. The configured
clock drift budget defaults to 3,000 ppm. These are engineering test defaults,
not measured game judgment windows or a claim of real timing qualification.

A grant retains the supplied clock. Its observed time is the older of the state
and audio observations. Its expiry is the minimum of both supplied expiries,
both maximum-age cutoffs, the fixed armed-run deadline, and the point where the
configured drift bound exhausts the uncertainty budget. Status may increase
uncertainty conservatively without creating new evidence. This also bounds an
already-issued grant if the owner delays its next check.

Before replacing an observation, each observe method checks the old armed
evidence at delivery time. A newly delivered observation cannot hide a gap in
the previous state/audio lease, an uncertainty violation, or an elapsed run
limit. Those failures revoke first, even if the replacement is fresh.

## Arm, revoke, and actual release acknowledgement

Supplying fresh observations can make the supervisor `ready`; it does not arm
input. `arm(epoch,now)` is explicit. The state and audio epochs must agree and
match the requested epoch. After a revocation the retired epoch cannot be reused.
Paused, result, unknown, stale or lost sync, identity/geometry changes, clock
regressions, explicit invalidation, and run limits revoke permission.

For FakeTouch/live modes, revoking an armed run latches `release_required` and
returns a revocation ID and revoked epoch. The owner must immediately stop the
bridge/backend and perform release-all through its established stop path.
The supervisor does not claim that returning `release_required` physically
released anything.

Changing to a new epoch cannot bypass that latch. The owner must submit a
`ReleaseAcknowledgement` with the matching ID/epoch, real observation/delivery
times at or after revocation, fresh provenance, and `known_released` knowledge.
`unknown`, `failed`, wrong-token, wrong-epoch, stale, future, or pre-revocation
acknowledgements retain the block. An RPC returning success or a local request
being queued is not automatically proof of zero Android contacts. The owner
must preserve the actual scope of its release evidence; when that scope is
unknown it must use `unknown`. Fresh evidence plus a newer epoch still requires
another explicit arm after known release. Shadow mode has no touch to release.

`request_user_stop()` is the only cross-thread method. It sets an atomic sticky
flag. The single owner must wake and check it promptly, and route that stop to
the bridge/backend. Release acknowledgement remains possible after a user stop,
but fresh evidence, known release, and a new epoch cannot clear the sticky flag.
There is no clear/reset method for user stop. Starting a new supervisor object
must be an explicit new user-authorized run, never an automatic retry that
discards an unresolved release or stop.

## Plan qualification and bridge integration

`PlanQualification` retains complete chart/audio/layout identity, geometry
version, compiled-cache SHA256, scope coverage, and explicit segment endpoints
when appropriate. A selected segment cannot be labeled a full chart while
carrying segment bounds. Every note in the declared scope must have compiled.
The caller must carry these values from the complete compiled result rather
than extracting the older `Memory` type and dropping its metadata.

Live mode additionally refuses `requires_calibration`, incomplete timing,
geometry, or backend qualification, and missing qualification evidence IDs.
A bounded normal-projection candidate therefore remains blocked for live even
when all geometric samples compile. The qualification booleans and provenance
are supplied assertions, not measurements performed by this class. Current
real-song synchronization negatives, unmeasured input lead/jitter, or unknown
backend/release capacity must remain unqualified. Shadow and FakeTouch are
explicit modes; their grants never set `live_permitted`.

The owner should follow this sequence:

1. Copy immutable compiled identity/scope/cache/calibration status into the
   qualification and preserve the complete compiled metadata in its run log.
2. Deliver only actual state/sync observations. Feed assembler segment resets,
   seek/restart, and explicit state invalidations into `invalidate` immediately.
3. Arm explicitly after fresh evidence and qualifications permit it. Only map a
   grant to `ExecutionObservation` for the matching execution mode/backend.
4. Check status before every owner dispatch cycle. If permission is revoked,
   stop/release and preserve unknown/failure; acknowledge only real release
   evidence. Never manufacture new observation IDs from polling.

`SupervisorGrant.sequence` advances only after a new accepted source observation.
The owner may use it as the bridge's composite evidence sequence, while keeping
`state_sequence` and `sync_sequence` for audit. A status-only uncertainty update
retains that sequence; it must not be presented to the bridge as a fresh
observation. Its earlier expiry already restricts the old grant, and the owner
must stop if a status check revokes it.

The bridge may impose a stricter age limit. In particular, combining a manual
state timestamp with fresh audio retains the older manual timestamp, so the
PAS bridge's default 150 ms evidence age can reject it before the supervisor's
1 s manual-age limit. The contract does not widen that bridge limit or claim a
single manual confirmation permits one second of bridge dispatch.

The offline tests cover explicit mode/arm, fixed run deadlines, all nonplaying
states, expiry gaps before replacement, unknown/failed release and token replay,
new-epoch recovery, manual evidence, future/repeated/unknown timestamps, live
qualification, identity/geometry/lock mismatches, clock regression, sticky
cross-thread user stop, segment scope, and malformed budgets. Tests do not
invoke a classifier, UI, endpoint, or physical input backend.

The standalone MSVC Release build passed `/W4 /WX` and 89 checks in 11 groups.
Independent review found the old-evidence replacement gap, which was corrected
and covered by regression tests; the final freshness, uncertainty-expiry, and
release-latch review reported no remaining blocking defect. These results are
contract validation only, not visual-classifier or live-song validation.
