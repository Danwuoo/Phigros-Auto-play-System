# Hard-disabled shared candidate owner seam: preliminary focused Release

This implementation connects an explicit current-candidate C++ request to the common
manual-session implementation, while its only constructible live admission remains
`UNQUALIFIED`. There is no qualified constructor, mutable admission bit, CLI enabling
flag, AA/AB adapter, or silent donor fallback. The admission barrier throws before
configuration loading, preflight, endpoint discovery or backend creation. The existing
manual-session CLI continues to choose its original donor branch.

The production candidate branch has not been compiled or executed on Windows. These
Linux cold controls do not qualify source/build/profile/cost/physical permission, do
not establish actual-game adoption, and do not make Chapter Legacy IN Miss=0 complete.
Fresh validated Windows receipts and a future admission validator remain necessary.

## Implementation boundaries

- `OwnerCandidateStep` references the existing sole `SessionGameOwner` and one bounded
  receipt stream. It owns the Hook, execution ledger and sticky finalization state.
  It has no observer, lifecycle, capture, endpoint or device-discovery implementation.
- The research Runtime keeps its original observer, lifecycle, ReplayBackend and owner,
  and forwards the extracted sync/accept/poll/finish/reset work to that same step.
  Compatibility fields are references to shared state, rather than duplicate ledgers.
- Production candidate action uses the shared step with a RecordingBackend around one
  actual GrpcTouch. It retains preflight/profile, stop/revoke/stale, lifecycle and
  dispatch guards. Candidate catch preserves the outer owner/step state and never
  invokes the raw backend emergency-release path after a first uncertain release.
- The 64-event receipt stream records issued and returned commands, and implicit
  scheduler releases. Overflow is sticky and refuses further injections. Release
  still reaches the backend when the diagnostic buffer is full. A first uncertain
  release is stored separately and cannot be retried or overwritten by later success.
  Unknown injection/returned-command mismatch is likewise sticky.
- Candidate packets alone retain the immutable same-consumed Frame lease and complete
  same-observer CandidateBatch. The same frame/batch/scene context, round and fresh
  time must agree. New-round, reset and epoch-rewrite mismatches remain ineligible;
  no context is rewritten to manufacture eligibility. There is one latest packet and
  the existing three physical capture buffers. Capture pool drops, packet overwrites
  and slow action packet skips remain separately counted.
- Candidate raw diagnostics include batch, filtered scene, accepted plan/coordinates,
  source identity, uint64/optional cursor, issued/returned receipt, release and
  packet/owner QPC. They remain one-way and bounded by the existing journal policy.
- Root production links the exact frozen BVI/front/current closure through the sole
  pas_core. No duplicate formal_core was introduced there. Frozen v3/front,
  current.cpp/tail_current.hpp, src/game.cpp and existing oracle files were not edited.

## Actual preliminary tests

Reserve was checked above 20 GiB before compile/test. The 11 unchanged archive members
were individually source-SHA verified against the retained final Release seals; see
`archive-reuse.json`. Changed game_session.cpp and own_bridge.cpp were freshly compiled.
This is a focused overlay, not a substitute for the parent's fresh complete three-mode
matrix and 3063-frame source-bound replay.

| Fixed test | Assertions | Failed |
|---|---:|---:|
| Disabled admission/packet/stream/shared owner controls | 47 | 0 |
| Existing Runtime first-release preservation | 32 | 0 |
| Existing RGB entry and verified-only new rounds | 12 | 0 |
| Unchanged independent typed-tail owner controls | 130 | 0 |
| Unchanged independent typed-tail pixels controls | 57 | 0 |

The original two-fresh evidence separation >=10ms, +20ms normal Up, same old contact
position, and no new Down/Move authority controls remain unchanged. This reused only
fixed fixtures and existing corpus sources; it did not search for scenes or seed old
physical actions.

Source-before/after seals for the extraction regressions are in
`regression-source-before.sha256` and `regression-source-after.log`. Every report rejects
an existing destination. All compile logs, reports and exit codes are retained here.
The focused executable accepts exactly one argument, a fresh output JSON path.

An independent source review caught a forwarding-template braced-initializer compile
error before freeze; the corrected production call explicitly uses GameActionOptions.
Its preserved expected-failing source/log belongs to the independent review package.
Diagnostic callbacks were also moved after ledger/finalization joins and captured with
an outer lifetime, so diagnostic failure cannot suppress first-release safety state.
