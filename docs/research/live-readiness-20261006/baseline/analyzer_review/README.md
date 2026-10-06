# Independent cost-admission review

Scope: the new C++ cost analyzer, provenance helper, SHA helper, and their bound
offline process-entry wiring. No device, Windows candidate pipeline, model, or
provider was run. The generated process and raw-auditor records used by the
contract tests are explicitly test doubles. A simulated positive gate is not a
measured cost result; `actual_cost_ready` remains false.

## Review findings addressed in the current source

1. Campaign metadata previously hashed unrelated files without joining their
   identities to the run. The producer now records campaign/run/source/profile/
   environment identities in the report and first Journal event; the auditor
   joins the two, and the analyzer compares the exact campaign identity.
2. Distinct report paths and hashes alone did not prove independent runs.
   Duplicate native PID plus creation-time identities and duplicate raw attempt
   or Journal hashes now reject repackaged runs.
3. Dense workloads previously bypassed phase-lateness limits even when actions
   existed. Nonempty dense distributions now receive the same hard limits.
4. Repeated SHA finalization/update could overrun the internal block. Finalized
   state now rejects both operations; known-answer and misuse tests cover it.
5. The minimum declared source set omitted frozen v3 and current front/bridge
   dependencies. Those source identities are now required. The pinned profile
   bytes and effective offline Runtime constants are checked together.
6. Separate valid process receipts, freezes, and declared arguments were not a
   joined chain. Bound runs now launch the candidate directly as the owned root
   through the unchanged invocation safety core. The analyzer checks COMPLETE
   state links to freeze, contract, and verification; sealed command/result;
   exact native specification arguments; and native root image, PID, and
   creation time against producer provenance.

These are admission safeguards. They do not substitute for a fresh Windows
build, its dependency/build closure, actual qualified process execution, a valid
representative workload, acceptable A/A noise, or physical adoption.

## Independent adversarial tests

`research/prelive_current/tests/cost_analysis_review_tests.cpp` invokes the actual
analyzer implementation. It first requires a complete simulated positive A/A
campaign. It then checks exact rejection reasons, rather than accepting any
failure that a stale source hash might cause.

The attacks cover campaign ID/SHA/source mismatch, foreign environment/source
manifest/freeze, a rehashed command/spec contradiction, a rehashed device-allowed
contract, a frozen specification omission, foreign root image, rehashed native
PID/creation identities, untrusted process identity, preserved STOP, duplicate
reports, rehashed duplicate producers, and duplicate raw attempts. Downstream
hashes are recomputed where needed so semantic joins are actually exercised.

Each temporary fixture mutation is restored and SHA-verified. Every source in
the generated campaign manifest is hashed before and after the review. Original
accepted Windows evidence and the supplied payload are never changed.

The source was reviewed for C++20/MSVC-facing portability: standard SHA unsigned
arithmetic and filesystem APIs, Windows direct native root image discovery,
FILETIME process identity, and exact string arguments. Linux compiler/sanitizer
results do not establish an MSVC build or Windows process qualification.

## Reproduction

Build and run the existing `cost_analysis_tests.cpp` into a fresh fixture root
first. Then compile and run the independent review program with that fixture
root and a fresh output root. Use the pinned patched nlohmann header, SHA
`f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc`.
The matrix receipt lists exact commands, flags, source hashes, and results.

Do not reuse stale fixtures after a source change. The test calls the campaign
source-closure validator before running its positive baseline or any attack.

## Verified matrix result

- Release (`-O2`): 36 seed and 130 independent checks passed
- Debug (`-O0 -g1 -D_GLIBCXX_ASSERTIONS`): 36 seed and 130 independent checks passed
- GCC ASan + UBSan: 36 seed and 132 independent checks passed, with leak detection disabled

Each independent pass contains 21 semantic/attack checks and 32 separately
recorded fixture-restoration hashes. Release/Debug check 109 source files; the
sanitizer seed additionally includes two newly added isolated study programs.
Every original 109 source identity remained unchanged across all three modes.

The first sanitizer seed passed its 36 assertions but exited with an
environmental LeakSanitizer fatal error under ptrace. That failed attempt is
retained. A fresh retry kept ASan/UBSan enabled and disabled only unsupported
leak detection. No leak-check success or Windows sanitizer success is claimed.

The complete simulated fixtures, positive output, exact-reason negative reports,
compiler/build/test logs, source hashes, and restore receipts are retained here.
Run the reproduction commands from the repository root. Archived absolute
fixture paths describe this run; regenerate fresh fixtures after moving the
repository or changing any source. Do not reuse them as measurement evidence.
