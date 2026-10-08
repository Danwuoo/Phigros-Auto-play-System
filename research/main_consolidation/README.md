# Main-only Windows qualification

The 2026-10-08 user authorization is recorded in
`docs/research/live-readiness-20261007/CAPACITY_APPROVAL_20261008.json`.
External project copies and evidence are consolidated before new candidate
execution. Historical receipts remain byte-identical at their retained paths.

`build.ps1 -Mode release|debug|asan` requires a clean `main` and independently
qualified main-local process guard and MSVC snapshot. It recompiles all 22
formal translation units and the affected research libraries without donor
objects, builds the typed writer and optional diagnostics, then runs the
15-case writer group and the other 14 affected groups. Every stage retains
source/dependency closures, before/after hashes, capacity receipts, bounded Job
ownership, verified stream completion and actual exit predicates.

The 48 GiB retention ledger counts migrated historical evidence, the migration
snapshots and new prelive outputs. Minimum free space and independent candidate,
raw and metadata limits remain. Frozen old recipes are not edited to claim
approval, relocation or new qualification.

`verify-relocation.ps1` accepts a pinned relocation-index SHA, original path,
expected length and expected SHA. It verifies moved raw bytes or an exact
physical source ZIP entry without extraction, launches, or file writes. Its
output is explicitly historical proof and never current-main qualification.

The combined library stage in Release main05 stopped at the fixed 64 held
process identities, after its CMake output reported success but before the
wrapper's natural exit was verified. That attempt remains STOP. main06 builds
the five libraries in dependency order as separate bounded stages and freshly
compiles all 22 core units again. Guard limits and predicates are unchanged.

main06's Configure stage also hit the same identity cap. main07 separates
vcvars initialization into one verified native stage, captures only 14 named
compiler variables with bounded literal values, and binds the resulting file
to the COMPLETE capture receipt. CMake stages recheck this proof and use the
same values without reinitializing vcvars. The process guard is unchanged.

Outputs live in `out/prelive-main-integration-*-main07` and
`out/prelive-main-integration-evaluation-main07-*`; `-BuildAttempt` selects a
fresh attempt namespace and never overwrites an existing attempt. No source worktree, emulator,
ADB, touch or model endpoint is created. Successful cold tests grant no live
admission; prefix, cost, production, profile and physical gates remain separate.
