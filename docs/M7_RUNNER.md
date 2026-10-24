# M7 single-owner session runner: preflight and FakeTouch

`SessionRunner` connects the real offline contact compiler, `RunSupervisor`,
`PasBridge`, scheduler, and concrete `FakeTouchBackend`. It is an integration
contract and simulated execution path. **This version has no live backend
constructor and cannot inject Android input.** No device discovery, launch,
classifier, timing producer, calibration loader, or new worker framework is
included. Audio research was stopped at the user's request.

## API and ownership

```cpp
auto compiled = compile_phigros_contacts(chart, identity, geometry, options);
// Check compiled.plan, retain all diagnostics, and construct qualification from
// the selected source scope and actual evidence. Do not manufacture qualification.
SessionRunner runner(std::move(*compiled.plan), qualification, clock, fake_touch);
runner.observe_state(actual_state_observation);
runner.observe_sync(actual_sync_observation);
runner.arm(epoch);  // Explicit; observations alone never arm.
auto outcome = runner.tick();
```

The caller owns the clock and FakeTouch backend for the entire runner lifetime.
Only one owner calls observation, arm, status, tick, invalidation, and release-ACK
methods. `request_user_stop()` alone is safe from another thread; the application
must also wake its owner loop. The stop is sticky for this runner's lifetime.
The backend must be used exclusively by that owner.

The immutable full `CompiledContactPlan` and `PlanQualification` remain readable
through const getters. Bridge capacity and input dimensions derive from the plan
metadata. There is no constructor accepting bare `Memory` or generic
`TouchBackend&`, and the supervisor mode must be `fake_touch`. Pure
`preflight_session(plan, qualification, mode)` returns rejection reasons; live
mode is rejected even if a caller sets every qualification flag.

Preflight checks exact chart/audio/layout identity and recomputes the compiler's
canonical `compiled_contact_fingerprint()` against the cache identity, nonzero
geometry version, complete scope attestation, closed memory lifecycle and peak
capacity, one source owner per contact, source hash/JSON-pointer IDs, step totals,
and exact sorted events versus memory. It preserves `requires_calibration` and
rejects a qualification that downgrades that metadata flag or conceals a
projection policy behind a false metadata flag. For an explicit
segment, every note judgment and every DOWN/MOVE/UP must fit inside the declared
interval, including Flick lead and release. A note-selection interval whose
contact envelope extends outside it is rejected.

The in-memory plan is expected to come from the compiler. Fingerprint v2 covers
semantic metadata, ownership, projection provenance and the complete output;
editing memory and events consistently still invalidates the old fingerprint.
This consistency check is not authentication, an untrusted JSON plan loader,
or independent proof that a caller's full-chart coverage assertion includes every
original source note. A future loader must verify source bytes, scope and
calibration artifacts rather than accepting caller-written qualification flags.

## Observation, dispatch, and stop contract

Only actual calls to `observe_state()` / `observe_sync()` advance supervisor
observation sequences. A permitted grant is passed unchanged to the bridge,
and `update()` occurs only when the grant sequence changes. Timer `status()`
never dispatches or creates evidence. Timer `tick()` first checks supervisor
state/expiry and then performs at most one bridge tick. In particular, an aged
`status()` grant has a larger uncertainty with the same sequence; it is not
resent as changed bridge evidence. Repeated `arm()` does not extend a run.

The supervisor grant already intersects state and timing expiry, observation
age, uncertainty expiry, and the finite run deadline. The bridge's 150 ms
evidence guard and dispatch guard remain active. An expired previous lease
cannot be repaired retroactively by a fresh observation. Missing/stale state,
pause/result/unknown, identity/epoch/geometry changes, a late owner tick, bridge
failure, finite completion, and user stop all stop future work and attempt
release. No skipped historical notes, fake `observed_host_ns=now`, automatic
new epoch, or automatic re-arm is introduced.

`SessionStatus.dispatched` contains only work from that call, so repeated status
queries do not replay old receipts. `finished` means this FakeTouch plan
completed, not that the game was played successfully. After revocation or
completion, explicit release acknowledgement is still required. Unknown/failed
release stays latched; a fresh epoch cannot bypass it. A claimed
`known_released` acknowledgement is downgraded to unknown if the actual
FakeTouch contact map is nonempty or the release report contains unknown/failed
IDs. A subsequent actual zero observation can be acknowledged explicitly.

For a future real backend, RPC success alone is insufficient: existing
`GrpcTouch` receipts explicitly say `rpc_returned_effect_unverified`. A real
zero-contact observation adapter is not supplied here. The current FakeTouch
ACK check must not be copied into a live path as an Android release proof.

## Remaining prerequisites for bounded live input

- A qualified time source and playing/paused/result source with real timestamp
  provenance. Existing gRPC `Frame.capture_complete_ns` is arrival time;
  `source_timestamp_us` is separate. Arrival-only frames must not be relabeled
  as mapped-capture evidence, and timers cannot renew a manual confirmation.
- Geometry, judgment-region and note-strategy calibration tied to the exact
  chart/cache/layout/build. Strict full-song Dlyrotz compilation currently
  rejects six offscreen targets/paths. Bounded normal projections and the
  candidate Tap/Drag/Hold/Flick strategies retain calibration requirements.
- The completed patched-gRPC build and real fixture evidence for capacity,
  coordinate rotation, continuous contacts, release, dispatch jitter, and
  simultaneous input. Scheduler ordering does not prove atomic backend batches.
- Measured game input lead/phase and tolerances. Current Chart Offset=0 ms does
  not measure host dispatch latency or turn the earlier visual +130 ms candidate
  into an input-lead value. No such calibration is inferred here.
- A narrow, explicit segment, fixed serial/AVD/profile, finite deadline, user
  stop path, zero-contact verification and honest failure log. The first planned
  DOWN must still be in the future when armed; a late lock must reject rather
  than silently dropping past notes.

These missing facts can be reported by preflight while the FakeTouch integration
is exercised. They do not justify setting `requires_calibration=false`.

## Verification

The independent Windows MSVC 19.50 / C++20 Release build passed eight groups /
298 checks and CTest 1/1 using `/W4 /WX`, linking
the actual compiler, supervisor, bridge and retained PAS core. Tests cover the
full finite three-note subsystem path; explicit arm; timer status preserving
sequence/time without dispatch; late tick; state expiry during Hold; pause and
release; unknown ACK and new epoch; actual nonzero FakeTouch despite a claimed
known ACK; sticky user stop; live/calibration rejection; coordinated memory/event
or semantic ownership edits that retain an old fingerprint; damaged events and full
segment contact envelopes. No device or audio producer runs in these tests.

Root integration target: static `src/rhythm/session_runner.cpp`, public include
directory, link `rhythm_chart_import`, `rhythm_pas_bridge`, `rhythm_supervisor`;
test `tests/session_runner_tests.cpp` links that target. Default/offline targets
remain separate from any eventual live CLI.

The integrated main build subsequently passed 15/15 CTest items. Dlyrotz IN13
with the explicit 250-reference-pixel normal-intersection candidate and declared
capacity four completed the whole `--session-replay`: 584 source notes, 4,911
ordered receipts checked individually, peak four active contacts / eight pending
plans, final zero FakeTouch contacts, and the supervisor's known-release ACK.
The private [full-session report](../../local-assets/phigros-4.0.1/m7-whole-fake-001/full-session.json)
retains `requires_calibration=true`. This replay uses synthetic exact clock and
state observations; it is no device, acoustic, input-latency, or score evidence.
