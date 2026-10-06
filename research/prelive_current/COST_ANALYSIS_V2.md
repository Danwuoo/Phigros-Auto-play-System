# Bound cost evidence v2

This is offline evidence admission. `ready=true` / `comparison_allowed=true` mean
only that one frozen cold A/A campaign passed. `live_ready` is always false.
Source compilation, owned-process qualification and physical/live acceptance remain
separate gates. No new Windows cost campaign was executed in this cloud work.

## Why v2 is necessary

The old analyzer admitted the same Tap report twelve times under forged scene
labels, ignored supplied SHA values, and trusted summary metrics. The old raw
auditor accepted fabricated timing summaries and insufficient plan/release joins.
Original negative controls and all old STOPs remain preserved.

V2 requires:

- A pre-run campaign with exact source/build/profile/protocol/environment bindings.
- Twelve unique A runs, four each in frozen Tap/Hold/dense order; report scene/mode,
  run ID, content hash, native producer identity and raw stream hashes must agree.
- A complete v2 raw audit of the same report/attempts/Journal. Distributions and
  denominators are recomputed; plan/source/cursor/contact/coordinates are joined;
  the first release uncertainty remains sticky.
- Identical producer provenance in the report and exactly one Journal event.
  Its campaign/source/binary/profile/environment/run identity must match the freeze.
- Direct candidate execution as the owned Job root. COMPLETE state binds the
  freeze, contract and verification; verification seals command/result; exact
  argv and native PID/creation time/image match the candidate's self-observation.
  An unrelated successful cmd.exe process is not accepted as candidate identity.
- Original 256 warmup + 2304 measurement, 20ms, Release coverage/lateness/RSS/noise
  limits. Every nonempty phase distribution, including dense, obeys the limits.
- Additional v2 adequacy: Tap/Hold measurement-window Down and Up must be nonempty.
  Actions entirely in warmup cannot qualify the later action-cost measurement.
  This does not reinterpret old evidence or change the original protocol file.

Original Tap AA runs have 24/24, 31/31, 23/23 and 29/29 measurement Down/Up in the
rechecked raw data. Their original noise failure remains. Original Hold has zero.
A short FakeClock repeated-raster probe's single action is a separate observation.

## Bound input formats

Input: `schema: pas.current-rails-cost-input.v2`, `campaign: {path, sha256}`.
For `aa`, add `runs` and `normal_stopped`; for `run`, add `run` and zero-based
`plan_index`. Each run has `run_id`, `scene`, `path`, `sha256`, `audit_path`,
`audit_sha256`, and `process_evidence`. Hashes use lowercase SHA-256.

`process_evidence` contains bindings `{path,bytes,sha256}` named `specification`,
`freeze`, `verification`, `result`, `command`, `state`, `contract`, and
`input_closure`. These are actual
files produced by the unchanged qualified process core, not hand-authored receipts.

Campaign:

- `schema: pas.current-rails-campaign-freeze.v2`, nonempty `id`, full 40-hex
  `source_commit`, `configuration: Release`, `platform: windows-x64`
- `scenes: [tap, hold, dense]` for the retained old workload, or explicitly
  `[tap, hold-closed-reset-v2, dense]` for the new frozen streamed family
- `bindings`: unique roles `candidate_binary`, `analyzer_binary`, `auditor_binary`,
  `profile`, `cost_protocol`, `environment`, `source_manifest`, each with actual
  path/bytes/SHA. The analyzer must be the frozen executable itself.
- `source_manifest`: matching `source_commit`, `dirty:false`, `repo_root`, and
  `files` with `relative_path`, path/bytes/SHA. Required core/v3/front/hook sources
  are enforced; also include the entire compiler dependency/build closure.
  Filename hashes alone do not replace a verified build's source-to-binary binding.
- `planned_runs`: twelve `{run_id,scene,mode:A,report_path}` objects, saved before
  the first run. Every report/root/stage is fresh and all attempts remain.

Pinned profile content is checked against the effective offline Runtime constants.
Both known LF and CRLF encodings of the exact existing profile are supported; the
actual file bytes remain hash-bound. Endpoint fields identify a historical profile,
not an authorized or connected device; the running backend remains ReplayBackend.

Original protocol SHA remains mandatory:
`58d6136fcbce1ebf6c25a23795f10aab78013d46318c2b6261b7f6206abe91e6`.

## Execution after a fresh qualified Windows build

```
<exact-current_chain.exe> pipeline A hold 2560 <campaign.json> <planned-run-id> <fresh-report.json>
<exact-prelive_audit.exe> <fresh-report.json> <fresh-audit.json>
<exact-cost_analysis.exe> run <bound-v2-run-input.json> <fresh-gate-output.json>
<exact-cost_analysis.exe> aa <bound-v2-aa-input.json> <fresh-noise-output.json>
```

These describe exact native arguments, not permission to bypass process control.
`run.ps1 -Operation pipeline ... -Campaign ... -CampaignRunId ...` prepares a
DIRECT `tools/zero_miss_windows/invoke.ps1` owned-root spec for those arguments;
all existing process identity/deadline/held/stream rules and capacity checks remain.
The legacy unbound CLI is available for diagnostics and cannot qualify v2 cost.

The old batch controller is historical. It cannot construct v2 inputs and must not
resume the old stopped campaign. The new `hold-closed-reset-v2` scene is explicitly integrated; the original `hold`
pixels remain unchanged. It streams the fixed 81-phase family through offsets
−2/0/+2 across blank-separated cycles, reusing one 2,764,800-byte RGB buffer. All
243 variants were checked byte-for-byte against the frozen family reference. A
measures entry/safety release; B additionally exercises sustained body/normal-tail
work. This difference is reported, never called equal active work or physical gold.

Malformed/mismatched evidence exits 2 without READY. A structurally valid negative
scientific result exits 0 with NOT_READY; native success is never scientific pass.
Unit tests use clearly marked process/auditor doubles; they are not real Windows
runs. The data-producing tools and qualified build/process receipts are the trust
boundary, not a cryptographic remote-attestation system.

## Streamed producer observability

All new pipeline runs retain every attempt's scheduled QPC due, render start/end,
and publication-call return timestamp. `capture_complete` equals synthetic render
end. The writer gets one producer-schedule event; its epoch/cadence/count/generator
must join the report and the complete raw due sequence. The QPC anchor precedes
the steady-clock sleep anchor, so starts before the declared due are invalid.

`producer_render_ms`, `producer_schedule_lateness_ms`, `render_start_to_owner_ms`
and `render_start_to_publication_return_ms` are separately recomputed from raw.
The return timestamp is an upper bound on the completion of the publication call,
not the instant the frame became visible; the consumer can finish before that
producer thread records its return. Unconsumed attempts still have this timestamp.

For the new scene, both renderer-deadline and publication-return overruns must be
zero (finish later than scheduled due + the fixed cadence). This prevents a slow
renderer/publication path from making owner coverage look easier by quietly
reducing the offered frame rate. Original capture→owner/phase/noise/RSS limits are
unchanged. Neither synthetic rendering nor these added host timestamps measure
real capture/RPC cost or source age.

The report plus all raw sidecars must be fresh. Atomic creation of a persistent
`<report>.reservation` directory permits only one concurrent producer for that
report; a failed/successful attempt never reopens its reservation. Partial old raw
and dangling sidecar symlinks are refused. The qualified owned-stage locking and
receipt checks still apply; this is not permission to run outside the wrapper.

Windows `run.ps1 -Operation tests` now invokes the streaming, producer-timing and
output-reservation controls as well as the core/new ownership suites. Nested
`tests/*.cpp` sources participate in the build freeze. These Windows invocations
are prepared but were not executed on the cloud Linux host.

## Bounded complete input closure

The unchanged owned-core freeze has a 65,536-byte limit. The actual historical
ASan freeze is already near that limit; simply appending the new sources, gates,
compiler inputs and source-migration proofs cannot fit. No cap is increased and
no compiler/source input is dropped. The prepared wrappers freeze a small root
and bounded leaves, with the native candidate/spec/campaign still direct anchors.

`input_closure` binds a fresh `pas.current-input-closure-verification.v1` receipt.
Admission requires COMPLETE/pass plus before/after verification, exact original
core state/verifier joins, the root's declared output path, directly frozen root,
all leaf/file SHA/bytes, unique canonical identities and complete bounded counts.
Every campaign role and every source-manifest member must be present in the
verified direct/indirect union. A native core COMPLETE without this successful
post-exit closure receipt is insufficient. Missing/STOP/false/partial receipts
are rejected. Wrappers only emit PASS after native receipt, source post-check and
capacity-after all pass. All failure artifacts remain.

Limits are 128 leaves, 64 files per leaf, 8192 total inputs; wrappers keep leaves
below32KiB and root/receipt below1MiB. The analyzer also enforces the original
64KiB native freeze cap. This is bounded local evidence validation, not remote
attestation. Exact schemas and executable Windows commands are in
[WINDOWS_COMMANDS](windows_recipe_evidence/WINDOWS_COMMANDS.md).

The wrappers now require explicit fresh `-GateReceipt` and `-ToolSnapshot`.
Build additionally requires the delivery's sealed `-SourceMigration`, its
`-SourceMigrationSha256` and exact `-CandidateCommit`. Canonical old/new source
blobs and separately verified LF/CRLF byte variants preserve the protected
historical freeze while admitting only the delivered source delta. A hardcoded
historical auditor or unmatched library can no longer stand for the new build.

## Disabled production API boundary

The endpoint-free producer and production candidate API now share the same
sole-owner action step and bounded receipt stream. The existing donor CLI remains
unchanged. The current candidate C++ API is hard UNQUALIFIED and rejects before
configuration, preflight, discovery or backend construction; no enabling flag,
qualified-state constructor or AA-to-live conversion exists. Its real Win32
perception→immutable-packet→action routing is source-integrated but unexecuted.
The endpoint-free timing pipeline does not measure that additional thread hop,
Win32 wake/pool contention, real capture or RPC. A fresh root production build,
that exact chain's Windows qualification and the evidence-backed live-admission
validator are still required before any separately authorized physical step.
