# Independent live-readiness audit, 2026-10-06

Baseline: exact main `e9e305e3aba13f05a786c74540419428610b2f76`.
This is read-only research plus explicitly assigned local auditor hardening. No device,
provider, browser, emulator, Fixture, GitHub push/PR/merge, or Windows process was used.
Linux checks below are software correctness checks, not Windows/QPC cost qualification.
Original prelive results, all failure/STOP evidence, frozen v3 sources and oracles stay intact.

## Verdict

The candidate is **NOT_READY for a bounded live test**. The original operation packet
correctly says `current_chain` is offline-only. Its current-rails hook is absent from
the donor `pas manual-session` entry. Running donor commands would test a different
candidate and does not close the current candidate's gates.

There are actionable cloud-only correctness fixes. The baseline independent auditor
and AA analyzer admitted reproducible contradictory evidence. The old reports still
remain negative evidence; these defects do not retroactively invalidate the raw data
or turn the failed original AA campaign into a new comparison qualification.

## Proven counterexamples

Reproduction source is `research/live_readiness_review/`. The baseline runner extracts
unaltered source from the exact baseline commit into a fresh output root and compiles
it with official nlohmann/json 3.12.0. No golden files are edited.

1. **One Tap run can counterfeit the full twelve-run AA campaign.** Use the unchanged
   sealed `pipeline-release-aa-tap-1.json` twelve times, label four entries each as Tap,
   Hold and dense, and supply all-zero SHA fields. Original `cost_analysis aa` returns
   `READY` and `comparison_allowed=true`. It uses the list entry's scene, never verifies
   the report's scene or SHA, and counts the same file repeatedly. All calculated noise
   differences become zero. This is also a stale-file/copy-paste correctness hazard.
2. **Actual 500 ms raw latency can pass as green.** A complete synthetic 2,560-attempt
   stream with one 500 ms owner latency, paired with an unchanged 0.0002 ms summary,
   gets original `integrity=true`; original `cost_analysis run` returns `READY`. The
   auditor never reconstructs timing distributions or measurement denominators.
3. **Receipt-to-plan join is only intent existence.** Changed contact coordinates,
   source frame and scheduled time are accepted. An Up for another intent on the same
   finger is accepted. A completed intent can Down again after Up, and a completed note
   can reappear under a new intent. The old auditor does not bind step/cursor or owner.
4. **First uncertain release can be overwritten.** Unknown release followed by a
   successful release is accepted with `release_verified=true`. Although the uncertain
   row is retained in the old output, it does not invalidate completeness or that claim.
5. **Missing input completeness.** Mismatching summary attempts and incomplete pixel
   input can return `integrity=true`; the old tool only exposes a separate false field.
6. **Typed proof is not serialized.** Baseline `decision_json` omits `current_contact`.
   An otherwise identical typed and untyped Hold target has identical serialized
   decision evidence. This is a diagnostic proof-binding defect, not permission to
   make an untyped target actionable.
7. **Historic pipeline receipts omit coordinates.** The pixel entry records x/y;
   pipeline cost receipts do not. Those old rows cannot retrospectively prove their
   exact coordinate join, even when timings and all other joins are valid.

## Assigned auditor-v2 correction

`research/prelive_audit/audit.cpp` now:

- SHA/byte-binds the report, both raw files and verifier binary; checks stability again
  before returning. The historical report/raw files are read only.
- Recomputes all published timing distributions from full attempts/receipts, including
  warmup-separated owner, observer and hook metrics; checks full and measurement
  denominators, coverage, publication, consumer skips, writer and receipt counts.
- Verifies accepted-plan identity/context/revision/source evidence, monotonic prefix,
  receipt phase/source/step/cursor, exact coordinates when present, and sole contact
  ownership. Terminal intents and note-identity rebirth are rejected.
- Handles explicit cancellation/expiry Up separately from a scheduled plan suffix,
  without incorrectly requiring cancellation to use the old planned Up time.
- Keeps first release uncertainty sticky and separates declared writer loss from
  complete prefix evidence. A later successful release cannot qualify an earlier
  unknown release. Legacy coordinate omission is explicit and stays unqualified.
- Reports measurement-window phase counts/lateness, so all actions in warmup cannot
  stand in for activity during the cost measurement window. This is additive v2
  adequacy evidence; the original frozen protocol is not rewritten.
- Exactly joins optional run provenance in the report to one Journal provenance event.
  Legacy absence remains inspectable and is not granted current-campaign provenance.

Integrity is not itself a live/safety/cost pass. A faithful fault report can have valid
integrity while `safe=false`, action count zero, low coverage or invalid cost gates.
Process qualification remains a separate checked receipt, not a claim made by this
Linux auditor. The candidate's pixel semantics and game adoption are not inferred from
a plan join.

## Exact remaining gate order

1. **Finish and independently review cloud software fixes.** Original evidence and
   36 frozen v3 failures remain distinguishable; run unchanged relevant regressions
   and new falsification controls, then freeze the new source and instrumentation.
2. **Establish a useful finite cold workload and campaign.** The original AA stopped
   at Hold1 with no Down/Up; Tap noise was also inadequate. Do not resume or overwrite
   that campaign. A separately frozen workload must demonstrate real own actions in
   the measurement portion before collecting new AA. Preserve the same quantitative
   safety/noise thresholds and all attempted denominators. Four independent A runs
   per Tap/Hold/dense must pass before any 36-run ABBA comparison; a failed necessary
   gate stops its campaign. No silent threshold/corpus/cadence change or quiet retry.
3. **Fresh Windows closure and process qualification.** Build exact source, headers,
   flags, mode, generated files, JSON dependency and other dependencies together;
   release/debug/MSVC-ASan results and current binary/module bindings are needed.
   Native exit 0 is not verification 0. Every stage requires fresh root/report,
   trusted identity and Job ownership, bounded deadline/streams, `active_final=0`
   and signaled retained handles. Preserve old STOPs; no guessed PID/taskkill,
   ignored unknown child identity, guard bypass or reopening stopped attempts.
4. **Physical evidence remains external.** Frame1519 candidate4/5 identity (two notes,
   split note or effect); legal Hold head contact, current body continuation and tail
   adoption; true capture/driver/RPC latency. Human visible-head confirmation at3030
   is not a legal-touch/adoption gold label. Synthetic tests and old touches cannot
   answer these questions.
5. **Prepare and qualify the exact candidate live entry.** The current executable is
   offline-only. An independently reviewed endpoint connection, source/binary freeze,
   single-owner five-contact behavior, unknown/failed injection and verified release
   controls are required. Donor `manual-session` is not an interchangeable entry.
6. **Fresh device identity and explicit authorization.** User-confirmed current serial,
   app/geometry/mapping and profile/capability/APK fingerprints are needed. Historical
   `emulator-5554` and old capability hashes are references only. Device inventory,
   capture preflight, Fixture installation/touch and a bounded game trial need their
   specified authorization before use. Cloud access does not grant any of them.
7. **Bound the first trial.** User chooses the song and presses Play. Freeze one-round
   watchdog, fresh output, data hard limits and free-space reserve. Stop for any gate,
   context/geometry, capacity/writer, RPC unknown/failed, evidence, result or user-stop
   condition. Preserve the first release result and do not start another round until
   release is verified. No auto-song selection, auto-retry or Down retry.
8. **Separate trial readiness from product completion.** Current Chapter Legacy roster
   and IN unlock status remain unknown. Final acceptance needs that current full
   denominator and same-frozen-candidate complete IN Miss=0 settlements; report P/G/B
   and score too. No AP precondition, historical HD inference, version cherry-picking
   or new three-consecutive-clear requirement.

Sources: current `PRELIVE_RESULTS.md`, `PRELIVE_REVIEW.md`,
`PRELIVE_OPERATION_PACKET.md`, `PRELIVE_CURRENT_COST_PROTOCOL.md`,
`PRELIVE_REPRODUCTION.md`, formal owner/scheduler, current hook/runtime and original
process-control wrappers. This review does not authorize device access or erase any
remaining gate.

## Verification and reproduction

Software validation entry points:

```
research/live_readiness_review/reproduce_audit_counterexamples.sh <verified-json-include> <fresh-original-output>
research/live_readiness_review/test_audit_v2.sh <verified-json-include> <fresh-v2-output>
ASAN_OPTIONS=detect_leaks=0 AUDIT_SANITIZERS=1 research/live_readiness_review/test_audit_v2.sh <verified-json-include> <fresh-sanitized-output>
```

The official upstream JSON3.12.0 single header has SHA256
`aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`.
Original source is exported from the exact baseline commit, not copied from the
modified working tree. Reproductions use finite synthetic JSON evidence; they do not
run the candidate, claim physical semantics or measure performance.

The focused suite has35 assertions: six positive controls, thirteen negative cases,
explicit coordinate absence, warmup-only and measurement-phase controls, and thirteen
specific-error checks. One additional malformed INT64 input must exit2 without a
report, exercising checked subtraction rather than undefined signed overflow.
The initial sanitizer attempt hit the host's LeakSanitizer/ptrace incompatibility;
that failure is preserved. Subsequent ASan+UBSan runs explicitly disable only leak
checking. Leak checking, Windows MSVC/ASan and live qualification are not claimed.

Restored original evidence includes16 pipeline and4 pixel reports. The auditor
recomputes them read-only:19 preserve valid integrity; the historical failed prefix01
is rejected for note-identity rebirth and incomplete pixels, as expected. The final
3,063-frame prefix remains valid. The writer-loss run stays incomplete. Old pipeline
runs with actions cannot obtain coordinate completeness; zero-action cases can have
vacuous complete joins but remain subject to separate action/cost gates. All legacy
pipeline runs lack current-campaign provenance. Original post-warmup AA Tap Down/Up
counts are24/24,31/31,23/23 and29/29; Hold remains0/0.

Final auditor source SHA256:
`762d60764a8889290e0e46264e88354321c7a8523e49b25662ee8abfd1e3f986`.
The compact reports and exact input/binary hashes are in this directory's `evidence/`.
All large original raw files remain in the verified restored package; no old evidence
or campaign is replaced by these new audit outputs.
