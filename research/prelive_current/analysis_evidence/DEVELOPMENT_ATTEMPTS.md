# Analyzer development attempts (not cost runs)

All executions were Linux C++ contract tests with synthetic process/auditor doubles.
No output in these attempts is a measured Windows campaign or a live qualification.

- Initial unit attempt: 30 checks passed before full producer/process bindings were
  added. Its temporary fixture root is `/workspace/shared/phigros-cost-analysis-unit-20261006-01`.
  This is superseded, not final-source verification.
- Intermediate compile failed on a `std::string != nlohmann::json` comparison in
  process-result SHA linkage. The compile failure was visible in the execution
  transcript; a complete immutable source/log snapshot was not saved. Explicit
  `.get<std::string>()` resolved it. No failing process receipt was rewritten.
- The next source version passed 35 tests in temporary root
  `/workspace/shared/phigros-cost-analysis-unit-20261006-03`; compiler/run logs are
  copied here. It preceded the independent final matrix and is not a final campaign.
- Final authoritative analyzer test matrix is under
  `docs/research/live-readiness-20261006/baseline/analyzer_review/`, including
  positives before semantic attacks, exact failure reasons, source hashes, original
  LSan infrastructure failure and explicit no-leak-check sanitizer rerun.

No intermediate result replaces the final-source matrix. The actual pipeline,
producer QPC/Win32 entry and direct owned Job launch have not run on this Linux host.

Additional preserved integration controls:

- `output-reservation/` records an intermediate compile error caused by a missing
  brace in the first atomic-reservation helper. `failed-header.hpp` and compile
  diagnostics are retained; no tests executed in that attempt. The corrected
  `output-reservation-final/` passed 9 checks in each Linux mode.
- `producer-timing/` preserves the initial unanchored arithmetic behavior.
  Independent review demonstrated a false zero-overrun result by shifting due
  times into the future. Anchored schedule events and start>=due now reject it;
  both the false result and final mutation matrix are preserved separately.
- `producer-publication-final/` has 15 arithmetic controls per mode. The independent
  producer review additionally exercises the actual auditor and qualification gate.
- `streaming-renderer/` has 252 controls per mode, including byte equality of all
  243 phase/offset variants and reuse of a single producer-owned RGB allocation.

These are software controls only. No Windows timing run or device action was made.
