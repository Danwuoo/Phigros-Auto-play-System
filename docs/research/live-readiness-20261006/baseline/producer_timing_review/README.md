# Independent streamed-producer admission review

All three final test modes passed on the same frozen sources:

- Release (`-O2 -DNDEBUG`): 36 seeded-admission checks, 137 independent admission checks, 14 producer/raw mutation cases
- Debug (`-O0 -g1 -D_GLIBCXX_ASSERTIONS`): the same counts passed
- GCC ASan + UBSan: the same counts passed; leak detection remains unsupported under this host's ptrace wrapper

Each admission review first requires the complete simulated positive A/A
campaign. Its 137 checks comprise 21 semantic/attack checks and 116 unchanged
source checks. Each mode records 32 fixture-restoration hashes. All 116 source
bindings are identical across modes, and the 17 explicitly frozen key files
remain unchanged before/after the whole matrix.

These are C++ contract tests. No Windows candidate pipeline, real cost campaign,
physical device, model, or provider was run. The campaign/process receipts are
explicit test doubles. `actual_cost_ready` and `live_ready` remain false.

## Counterexample and fix verification

The first timing helper accepted a uniformly future-shifted due schedule. The
preserved counterexample starts with three genuine renderer overruns, then
moves every due time forward one second. The old helper reports zero overruns
and -999 ms producer lateness.

The historical header snapshot is byte-identical to SHA
`041b374d045be3beed055bf578d60451df035c75c4a6c927cc43333be0e1e559`,
recorded by the pre-existing producer-timing source manifest. It was reconstructed
from the inspected one-predicate revision and accepted only after that exact
hash match. No current product source was changed to run the counterexample.
The initial compilation raced with the implementation fix and rejected the bad order;
that exit-134 attempt is separately retained and is not presented as the old
helper's result.

The final implementation rejects `render_start < due`, anchors the fixed
schedule to a pre-thread QPC epoch, emits one critical schedule Journal event,
and requires the report, first raw due, and event to agree. Publication return
is measured separately for every attempt. Renderer and publication-return
overrun counts are both recomputed and must be zero for the new scene's cost
admission. Publication return is an observed upper bound after `latest.publish`
returns, not a claim about the precise publish instant.

## Actual-auditor synthetic raw cases

The independent C++ program creates fresh 2560-row synthetic attempts and
matching Journal/report inputs, then calls the actual raw auditor and numeric
cost-validity implementation. It establishes a valid baseline before applying
these mutations:

1. Render-summary percentile tamper
2. Missing render timestamp
3. Wrong single due/cadence step
4. Honest renderer overrun: raw integrity passes; the cost gate refuses it
5. Hidden renderer overrun count
6. Honest publication-return overrun: raw integrity passes; the cost gate refuses it
7. Hidden publication-return overrun count
8. Publication-duration summary tamper
9. Missing publication-return timestamp
10. Uniform future-due shift
11. Mismatched schedule anchor event
12. Missing schedule event
13. Missing producer report fields

Failures are matched to the intended audit errors or cost refusal. The honest
publication-overrun case deliberately has consumer/owner completion before the
producer records publication return; the raw auditor correctly accepts that
ordering. It does not impose an invalid consumer-after-return constraint.
Campaign/process qualification is exercised separately by the seeded admission
suite; these synthetic timings are never promoted to a measured campaign.

## Additional integration findings addressed

- Output protection now uses an atomic persistent `.reservation` directory and
  rechecks all five raw/report names. This arbitrates cooperating simultaneous
  launches and preserves failed attempts. Main calls the reservation helper.
- The new helper tests are explicitly invoked by the Windows runner, including
  a fresh output directory for the orphan/concurrency test.
- Nested helper test sources are included in the Windows build-input freeze.
- Publication-return timing closes the previously narrower renderer-only
  deadline accounting. The old renderer count remains explicitly separate.

Read-only inspection found no obvious new producer/consumer data race or MSVC
API defect: sample timing writes precede mutex-protected publication, RGB bytes
are copied before reuse, final aggregation follows producer join, and schedule
metadata is emitted before the producer starts. This is not a Windows build or
process-qualification result. Windows symlink-test coverage is still separately
limited by the test's platform guard.

## Evidence and reproduction

`matrix_report.json` contains the machine-readable result and archive identity.
`synthetic-evidence.zip` preserves every generated synthetic raw input, audit,
fixture, output, and build/test log from the final matrix. Its embedded manifest
lists exact bytes/SHA; every entry was read back with CRC and SHA verification.
Only compiler products are omitted. Readable result summaries remain outside
the archive. Earlier completed baseline and analyzer-review reports are intact.

The tested source is
`research/prelive_current/tests/producer_admission_review_tests.cpp`.
Compile the auditor in a separate translation unit with
`-Dmain=raw_audit_entry`, then link it with this test. `run_final_matrix.sh`
contains the full commands and flags used, including fresh fixture/output roots.
Run from the repository root using the pinned patched nlohmann header. To rerun,
choose a new output root; never reuse archived absolute fixture paths after a
source or location change. Python was used only to hash and archive evidence;
all raw analysis and test logic is C++20.
