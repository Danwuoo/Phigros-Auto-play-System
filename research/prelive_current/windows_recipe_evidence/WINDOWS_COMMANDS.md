# Fresh Windows offline recipes

These are prepared commands, not executed Windows results or device authorization.
This cloud work ran source-only checks. It did not run PowerShell, configure this
Windows CMake project, compile with MSVC, execute a Windows candidate, or start a
cost campaign. Original STOPs, negative results, timeouts/skips and the frozen
oracle remain unchanged. No push, emulator, game, capture/RPC or physical touch is
authorized by this file.

## Prerequisites and stopping conditions

1. Use the verified incremental source in a separate Windows worktree. First
   verify the handoff manifest and exact final source hashes. Do not overwrite
   the user's original checkout or old outputs. Do not use an old executable to
   represent this source. Finish the coordinated disabled-live seam source before
   building; its focused test is included in the recipe.
2. Preserve and validate the protected initial/source baseline inputs:
   - out/prelive-20261006/initial-audit.json
   - out/prelive-20261006/formal-source-before.json
   - out/windows-handoff/build-bridge-withdraw-release-01/freeze.json
   - out/windows-handoff/build-bridge-withdraw-debug-01/freeze.json
   - out/windows-handoff/build-bridge-withdraw-asan-01/freeze.json
   - out/windows-handoff/withdrawal-review-01/stage.ps1
   The source-before / bridge guards are retained. Missing or mismatched inputs
   are blockers; do not hand-author replacement receipts or bypass a guard.
3. Verify the actual VS2026 compiler, SDK, patched vcpkg dependencies, flags and
   compiler dependency closure. The compact payload is not a self-contained
   toolchain. Generated protobuf inputs under out/win-r4-ninja-release-01/generated
   must exist with the verified bytes. research/prelive_current/cmake-stage.ps1
   still has workstation-specific CMake/Ninja/VS/vcpkg/ASan paths; verify them
   before proceeding. A path string alone does not establish a valid toolchain.
   Do not install or substitute another toolchain under this recipe.
4. Produce a new native-process qualification in fresh roots using the unchanged
   tools/zero_miss_windows/pretest.ps1 and qualify.ps1. Preserve all four native
   controls and file-only diagnostic receipts. Set $gate to the exact new
   native-gate.json. Both current and auditor wrappers require -GateReceipt; no
   historical gate is silently selected. They verify and freeze its referenced
   hashes, and invoke.ps1 repeats the original qualification checks per launch.
5. Set $tool to the exact verified tool-snapshot.json for this build. Its sibling
   bin directory must contain the pinned compiler tools; copied file hashes and
   the full dependency closure must be verified. Both wrappers require
   -ToolSnapshot; no historical tool snapshot is silently selected. Tool snapshot
   preparation is a separate prerequisite, not manufactured by these wrappers.
6. Set $clips to the verified retained original 60-clip RGB input root, including
   its index.jsonl. The tests verify all indexed hashes before/after and freeze
   the index plus raw files. Do not replace the original tests or their oracle.
7. The original aggregate cap is 12 GiB, metadata seal cap 64 MiB and free-space
   floor at least 20 GiB. These wrappers preserve the original C: free-space
   gate, so the output roots must be on C:. Another output volume requires a
   reviewed adaptation before use. Added tests/family reserve 1 GiB before each stage;
   auditing reserves 64 MiB. Keep old failures; no cleanup is performed here.
   A capacity, process, source, receipt or test failure stops the invocation.
   Choose a new attempt name for a subsequent separately authorized attempt;
   never remove or overwrite the failed attempt.

8. Use the supplied byte-sealed SOURCE_MIGRATION.json and its canonical source
   blobs from the final handoff. Set $migration to its actual local path,
   $migrationSha to the SHA in the verified HANDOFF seal, and $candidateCommit
   to that handoff's full target commit. The wrapper requires all three on build,
   proves the exact old/source delta and new LF/CRLF target bytes, and never treats
   JSON identity alone as Git attestation. Users do not hand-author or rewrite
   this manifest. See the portable materialization format below.

A different computer or absent protected inputs requires a reviewed adaptation;
these commands do not relax or fabricate the historical source guards.

## Fresh process qualification

Run from the new repository root in the verified pinned PowerShell environment.
Here $repo and $pwsh refer to that actual worktree and verified interpreter.
The following names must be unused. This only qualifies offline process control.

```powershell
$repo = (Get-Location).Path
$pretestRoot = Join-Path $repo 'out/windows-handoff/cold-pretest-01'
$qualifyRoot = Join-Path $repo 'out/windows-handoff/cold-native-01'
& $pwsh -NoProfile -File .\tools\zero_miss_windows\pretest.ps1 -FreshRoot $pretestRoot
if ($LASTEXITCODE -ne 0) { throw 'fresh pretest failed' }
& $pwsh -NoProfile -File .\tools\zero_miss_windows\qualify.ps1 `
  -FreshRoot $qualifyRoot -DiagnosticReceipt (Join-Path $pretestRoot 'diagnostic-pretest.json')
if ($LASTEXITCODE -ne 0) { throw 'fresh native qualification failed' }
$gate = Join-Path $qualifyRoot 'native-gate.json'
```

Qualification itself has pinned workstation paths and native controls; check all
prerequisites before running it. Assign $tool and $clips only after their separate
verification. Do not set them to an unverified path merely to satisfy a parameter.

## Fresh candidate / analyzer / auditor build and cold suites

The new CMake tree builds prelive_audit.exe from the current audit.cpp alongside
candidate/analyzer/tests. It also links a test-only OBJECT copy of that exact
source with main=raw_audit_entry into producer_admission_review_tests; the real
producer control runs the auditor in-process and does not launch a hidden binary.

This endpoint-free owner/analyzer/auditor closure is rebuilt here. It rebuilds
formal_core and the pure runtime_contract prefix, not the full production
src/runtime.cpp / src/manual_session.cpp Win32 route bodies. A separate qualified
root production CMake cold build is still required for those changed branches;
source hashing and these research suites do not prove their compilation. No
production binary is executed by this recipe. Omit -CoreDonor. The historical donor
option is not part of this fresh-candidate recipe.

```powershell
foreach ($mode in @('release', 'debug', 'asan')) {
  & $pwsh -NoProfile -File .\research\prelive_current\run.ps1 `
    -Operation build -Mode $mode -BuildAttempt cold01 `
    -SourceMigration $migration -SourceMigrationSha256 $migrationSha `
    -CandidateCommit $candidateCommit `
    -GateReceipt $gate -ToolSnapshot $tool
  if ($LASTEXITCODE -ne 0) { throw "fresh $mode build failed" }

  & $pwsh -NoProfile -File .\research\prelive_current\run.ps1 `
    -Operation tests -Mode $mode -BuildAttempt cold01 -Attempt coldtests01 `
    -ClipRoot $clips -GateReceipt $gate -ToolSnapshot $tool
  if ($LASTEXITCODE -ne 0) { throw "$mode tests failed" }

  & $pwsh -NoProfile -File .\research\prelive_current\run.ps1 `
    -Operation family -Mode $mode -BuildAttempt cold01 -Attempt coldfamily01 `
    -FamilyBefore .\research\prelive_current\workload_family_evidence\before-repair.json `
    -GateReceipt $gate -ToolSnapshot $tool
  if ($LASTEXITCODE -ne 0) { throw "$mode family failed" }
}
```

Fresh build roots are out/prelive-current-<mode>-cold01. After COMPLETE build
receipts, the wrapper writes out/prelive-20261006/current-build-<mode>-cold01.json,
binding actual output bytes, exact root/mode/gate/tool, sealed source migration,
all source inputs and both qualified build/closure receipts. Later
operations require it and recheck all build artifacts and all build-freeze input
paths. This local binding is not a cost admission result.

The tests operation retains original owner/front/own-prefix/current/formal tests
and the affected body/tail/release/diagnostic suites. It includes streaming,
producer-timing, output reservation, independent tail controls and the disabled
live seam test, plus the independent live_seam_packet_review (63 controls) and
live_seam_runtime_review using the preserved v2 test (97 controls: original 91 plus six fixed expiry controls). Both use a fresh
report argument and the same current_rails library; these are prepared Windows
invocations, not Windows pass claims. It then invokes the following exact native arguments:

- cost_analysis_tests.exe FRESH_FIXTURE_ROOT REPOSITORY_ROOT
- cost_analysis_review_tests.exe GENERATED_FIXTURE_ROOT FRESH_REVIEW_ROOT
- input_closure_review_tests.exe GENERATED_FIXTURE_ROOT FRESH_CLOSURE_REVIEW_ROOT
- producer_admission_review_tests.exe FRESH_PRODUCER_REVIEW_ROOT

The review freezes every generated fixture input. Its deliberate fixture-only
mutations restore exact bytes, rechecked by the wrapper and owned core. All
fixtures are marked process/auditor test doubles; their positive controls are
not real Windows cost READY.

The family operation runs the full frozen v1 and v2 functional corpora, preserving
all 12 cells. Exact verifier arguments are:

- cost_workload_family_verify.exe FROZEN_V1_BEFORE FRESH_V1_AFTER FRESH_VERIFY_REPORT
- cost_workload_family_v2_verify.exe FRESH_V2_REPORT FRESH_VERIFY_REPORT
- verify_tail_family_goal.exe FRESH_V2_REPORT FRESH_NORMAL_TAIL_REPORT

The explicit archived before-repair.json must be verified against its preserved
SHA ledger before use. Family output paths are frozen in each downstream native
stage. Normal-tail counts are synthetic software evidence, not a physical tail
opportunity denominator or Phigros adoption. MSVC ASan does not imply UBSan,
LSan or comprehensive third-party instrumentation. Only Release can be used for
later separately approved cost qualification.

## Bound current report / auditor / analyzer plumbing

This is the existing single-run interface, not a campaign controller. Do not
resume research/prelive_batch.ps1 or create an unreviewed campaign. Freeze a new
campaign with all prerequisites/limits from COST_ANALYSIS_V2.md before any cost
run. Every planned run/result remains; the original A/A STOP/noise failures and
ABBA NOT_RUN stay preserved. Set $campaign and $runId only to an actual frozen
new plan. The following example requires its plan to name this exact fresh report.

```powershell
& $pwsh -NoProfile -File .\research\prelive_current\run.ps1 `
  -Operation pipeline -Mode release -BuildAttempt cold01 -Attempt coldcost01 `
  -Variant A -Scene hold-closed-reset-v2 -Frames 2560 `
  -Campaign $campaign -CampaignRunId $runId `
  -GateReceipt $gate -ToolSnapshot $tool
if ($LASTEXITCODE -ne 0) { throw 'bound pipeline failed' }

$report = Join-Path $repo 'out/prelive-20261006/pipeline-release-coldcost01.json'
& $pwsh -NoProfile -File .\research\prelive_audit\run.ps1 `
  -Report $report -Mode release -BuildAttempt cold01 -Attempt coldaudit01 `
  -GateReceipt $gate -ToolSnapshot $tool
if ($LASTEXITCODE -ne 0) { throw 'current auditor failed' }
```

The auditor selects only the sealed
out/prelive-current-release-cold01/prelive_audit.exe. It requires COMPLETE build
receipts plus exact current build/tool/gate bindings, freezes report and raw
sidecars, checks freshness/capacity, and runs the auditor as the direct native Job
root through unchanged invoke.ps1 / process.ps1. Its fresh output is
out/prelive-20261006/audit-release-coldaudit01.json. Exit 0 alone does not mean raw
integrity, numeric admission, noise adequacy or READY passed.

After independently constructing the exact bound-v2 input with report/audit SHA
and all actual process receipts, including process_evidence.input_closure bound to the PASS
closure receipt below, the existing analyzer recipe is:

```powershell
& $pwsh -NoProfile -File .\research\prelive_current\run.ps1 `
  -Operation analysis -Mode release -BuildAttempt cold01 -Attempt coldanalysis01 `
  -Analysis run -AnalysisInput $boundRunInput `
  -GateReceipt $gate -ToolSnapshot $tool
if ($LASTEXITCODE -ne 0) { throw 'bound analyzer failed' }
```

Use -Analysis aa only with a complete separately frozen bound-v2 A/A input.
Synthetic correctness, raw integrity, process qualification, Windows cost and
physical/live acceptance remain separate gates.

## Bounded input closure without changing the owned core

The original owned-core freeze limit remains 65,536 bytes. An actual canonical
CMake source/include-only ASan estimate was already 64,475 bytes, with only
1,061 bytes left before migration blobs, new gate references, outputs and specs.
freeze-size-estimate-01.json records that lower estimate. The wrappers therefore
write bounded input leaves; they do not increase the original core cap or drop
source/dependency identities.

Exact metadata formats:

- Root schema: pas.current-input-closure.v1. Keys: stage,
  verification_output (full local receipt path), files_count, leaf_count,
  leaves (path/bytes/sha256 bindings).
- Leaf schema: pas.current-input-closure-leaf.v1. Keys: stage, zero-based index,
  files (path/bytes/sha256 input bindings).
- Receipt schema: pas.current-input-closure-verification.v1. Keys: stage,
  status (COMPLETE or STOP), pass, before_verified, after_verified, root binding,
  state binding, verification binding, files_count, leaf_count, reason.
  state is the exact owned stage state.json; verification is the exact
  <stage>-verification.json. A pre-native STOP can have null core bindings.

Every path is canonical and deduplicated. At most 64 files per leaf, 128 leaves
and 8192 inputs are allowed. Each leaf is below 32 KiB; root and receipt are below
1 MiB. Input lists contain only existing inputs, never the current root/spec or
current execution outputs. The root declares a fresh verification_output. No
spec-to-root hash cycle is created.

Root/leaves are directly frozen by the unchanged core. Spec and candidate are
also direct core members; a bound campaign stays direct. The wrapper verifies
all root/leaf/input hashes before and after execution. It writes a fresh
CreateNew PASS receipt only after the native owned Receipt and capacity-after
checks also pass. A failure writes STOP/pass=false and blocks a build seal.
Underlying core COMPLETE alone cannot admit a bound cost run: the analyzer
requires process_evidence.input_closure, the declared receipt path, exact
root/core joins, leaf/input hashes and bounded counts. Retain both core and
closure receipts when a controller fails; do not relabel core COMPLETE as full
wrapper success.

For the pipeline example, the closure receipt is:
out/prelive-20261006/pipeline-prelive-release-coldcost01-input-closure-verification.json.
Use its actual lowercase SHA/bytes/path as process_evidence.input_closure in the
bound-v2 run input. This is an additional verified file, not a substitute for
specification/freeze/verification/result/command/state/contract.

## Portable source migration materialization format

The coordinator creates SOURCE_MIGRATION.json only after final source/commit
freeze and seals its exact bytes in the HANDOFF. Users receive the manifest plus
canonical target/original blobs together; no local JSON rewriting is needed.

Top-level keys:

- schema: pas.windows-offline-source-migration.v1
- target_commit: externally verified full final commit
- old_freezes: exactly release/debug/asan entries, each with mode, path, bytes,
  sha256. path is repo-relative out/windows-handoff/build-bridge-withdraw-<mode>-01/freeze.json
- paths: unique explicit source-delta rows described below

Each source row contains relative_path, canonical_target (path/bytes/sha256),
target_encodings (exactly LF and CRLF entries with encoding/bytes/sha256), and
old_entries. canonical_target is the exported, strictly UTF-8, BOM-free canonical
LF source blob. Its SHA/bytes and the separately precomputed CRLF SHA/bytes must
match the packet. Actual candidate source bytes must match one declared encoding
exactly; the wrapper does not normalize arbitrary working source before checking.

Every old_entries item retains path as the exact historical absolute source
identity, plus bytes/sha256 matching the archived bridge freeze. Supply exactly
one proof:

- preserved_original: path/bytes/sha256 of the unchanged archived raw bytes; or
- canonical_original: path/bytes/sha256 of the preserved canonical Git source
  blob, plus old_encoding LF or CRLF. The exact calculated variant must equal
  the archived old bytes/SHA.

previous.path is identity, not an instruction to rewrite historical paths. The
historical repo prefix is anchored by the unchanged formal-source-before.json.
Only exact corresponding source paths can migrate. Nonmapped sources, toolchain,
SDK/library, generated-protobuf and receipt guards still use original SHA checks.

canonical_target, canonical_original and preserved_original paths are relative
to the SOURCE_MIGRATION.json directory. Absolute paths are accepted only within
that same packet directory. old_freezes paths resolve inside the selected repo
and must equal the fixed exact freeze location. Reject .., UNC/streams,
trailing-dot/space aliases and any symlink/junction ancestor. All resolved actual
files are added to the input closure. Keep the manifest's sealed original bytes.

Required target-bound rows, including new files not present in old freezes:

- CMakeLists.txt
- include/pas/game.hpp; src/game.cpp
- include/pas/game_session.hpp; src/game_session.cpp
- include/pas/runtime.hpp; src/manual_session.cpp; src/runtime.cpp
- include/pas/candidate_admission.hpp; include/pas/candidate_packet.hpp
- research/prelive_current/own_bridge.cpp; own_bridge.hpp; runtime.hpp
- research/prelive_current/owner_candidate_step.hpp; receipt_stream.hpp
- research/prelive_current/input_closure.hpp; disabled_live_seam_tests.cpp

The wrapper's explicit source allowlist permits the current cost/auditor/hook
source deltas as well, but never general path remapping. The old game.hpp/game.cpp
source-before exception now also requires an exact target/proof mapping, so it
cannot admit arbitrary replacement source bytes.

source-migration-inventory-01.json records 16 preliminary rows and all eight
old-source entries, with exact Git-blob LF/CRLF proofs found for every old path.
It is an observation, not the executable final migration manifest. The new
input_closure.hpp row and final target hashes must be added by final materialization.

## Static evidence scope

validate.py checks declared targets, inspected CLI arities, fixture ordering and
freezes, preserved source guards, direct native routes, gate/tool/build bindings,
bounded closure and source migration declarations, capacity constants, simple
quote/delimiter sanity and unchanged safety-core SHA. Deliberately corrupted
in-memory recipes must fail focused checks. It never executes any candidate,
compiler, PowerShell or Windows process.

Earlier static-review-01 through static-review-04.json are retained observations
of their exact snapshots. static-review-04 covered 113 source-only checks before
the portable migration path adaptation. static-review-05.json covered 119 checks,
including the independent closure-review target and exact CLI. Neither is a PowerShell parser or
CMake/MSVC execution receipt. Coordinated later C++ changes require another
source receipt after final freeze. A validator quoting-edit SyntaxError is
retained in static-script-syntax-failure-01.txt; no native process ran for it.

Run a new source-only receipt without overwriting an earlier one:

```sh
python3 research/prelive_current/windows_recipe_evidence/validate.py \
  --output research/prelive_current/windows_recipe_evidence/static-review-next.json
```

The actual Windows recipes and candidate remain NOT_RUN / NOT_READY until the
specified independent Windows checks finish. The full Chapter Legacy IN Miss=0
goal is not completed by these recipes or by all-green synthetic tests.

## Frozen execution recipe identity

Final execution-source freeze (the preceding 17:52 UTC snapshot and its receipts remain historical):

- research/prelive_current/CMakeLists.txt: f9ddb751910b7d85e545f9c088b3df6b070fde8a90d4085532a1b333d1fe5afc
- research/prelive_current/run.ps1: 57a272418659b860ebb2f8e8612b2a0714699ae789a503bda2b4aa9591b0ee53
- research/prelive_audit/run.ps1: e9560fa94211bc47c6fe1bcdf6d457254b74590abb43dc8ad52e4966ebcbce21

Documentation/static receipts can have later hashes; they do not change these
execution source bytes. A later execution-source edit requires a new source
freeze and explicit renewed static review. The final static-review-10.json rechecks these exact source bytes; it is source-only evidence, not a Windows execution receipt.
