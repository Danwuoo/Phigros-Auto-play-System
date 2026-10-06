# Bounded input-closure admission: independent review

Scope: cloud-only C++20 contract review. No Windows process campaign, device,
emulator, real touch, live entry, push, cost threshold change, or process-core
cap relaxation is included. The seeded core/auditor receipts are explicit test
doubles and never grant measured-cost or live readiness.

## Static finding

No actionable false-admission hole was found in the reviewed bounded closure
joins. The analyzer requires a closure receipt even if the underlying native
core reports COMPLETE. It validates:

- schema/stage, COMPLETE status, pass, before_verified and after_verified;
- exact canonical state and native-verification identity, including SHA;
- a directly frozen root and the root's exact receipt-output path;
- every bounded leaf and member binding, stage/index, canonical uniqueness and
  root/receipt/leaf self-reference exclusions;
- nonempty leaves, <=64 members per leaf, <=128 leaves and <=8192 members;
- root/receipt count agreement and all campaign-role/source-manifest members in
  the direct-plus-indirect union;
- the unchanged 65536-byte native freeze JSON bound in process admission.

The root's immutable SHA recursively seals every leaf's SHA. Requiring every
leaf to be a direct core member would be redundant for analyzer admission. The
Windows wrapper additionally freezes the leaves directly; the analyzer's
minimum requirement remains a directly frozen root plus validated leaf closure.
These are local evidence consistency checks, not remote attestation or trusted
build qualification.

A recipe integration gap was flagged to its owner: source migration allowlists
must include the new input_closure.hpp. The recipe owner confirmed adding it to
both allowed and required lists. No production file was changed by this review.

## Independent controls

Source: research/prelive_current/tests/input_closure_review_tests.cpp.
CLI: input_closure_review_tests GENERATED_UNIT_FIXTURE_ROOT FRESH_REVIEW_ROOT.
The only linked dependency is nlohmann_json. The analyzer translation unit is
included under a renamed test-only entry point, matching existing contract tests.

The original seeded run-1 complete loader is the first positive precondition.
Each negative case then starts from a separate freshly cloned process/closure
fixture and proves that the complete cloned run loader admits before mutation.
Every affected leaf/root/freeze/state/receipt is resealed in dependency order.
Foreign state and verifier controls copy identical bytes to distinct paths so
the failure demonstrates the canonical identity join, not a bad checksum.
Omission cases update both root and receipt counts, and the whole-leaf omission
also renumbers the remaining indexes. They must reach the campaign/source union
check rather than an incidental count, stage or stale-SHA failure.

The negative matrix covers missing closure with core COMPLETE; false pass,
before/after flags, STOP, foreign receipt stage/state/verifier; unfrozen root;
wrong existing output path; root/leaf schema and stage; leaf index; duplicate
leaf and canonical alias input; 65/empty leaf, empty/129-leaf root; both root and
receipt count fields; omitted required header, campaign environment and source
manifest, and an entire leaf; member size tampering; and the unchanged 64 KiB
native freeze cap.

Every original seed artifact, including deliberately failed controls and the
known tampered run-2 sidecar, is snapshotted before and after. All actual source
members, campaign bindings and the independent test's own source are also
SHA/size checked. The test never rewrites or restores any original source or
seed artifact. Mutated test doubles and their observed errors remain retained.

## Validation status

Small Release diagnostic and the parent's final three-configuration frozen-tuple
matrix are reported separately. The initial independent-test compile failure is
retained in review-compile-failure-01 with its exact test source, compiler output,
exit code and source tuple. It was a mixed-type auto declaration in this test;
the corrected source uses separate path and JSON declarations.

No test-double PASS means actual Windows cost READY. Windows qualification,
physical/live gates and the Chapter Legacy complete-IN Miss=0 product goal
remain separate and unfinished.

### Small Release diagnostic

PASS: 35/35 checks, consisting of 31 fixed negative controls and four positive /
immutability checks. All 31 freshly cloned processes admitted before mutation.
202 original seed files and 131 source/campaign/test-source bindings were
unchanged. Compiler exit=0, test exit=0, all frozen source SHA checks=OK.
Retained receipt and full observed-error matrix:
[release-smoke-02/diagnostic_receipt.json](release-smoke-02/diagnostic_receipt.json),
[release-smoke-02/review_result.json](release-smoke-02/review_result.json).

Independent test source SHA256 at this diagnostic:
99f3c0ad5149af78f864ca5da3c4f42f21cb6526b502ae200616a72a959fcbbb.
The final same-source three-mode matrix is still pending in the parent task;
this small diagnostic makes no claim about its outcome or MSVC execution.

### Final-matrix attempt01 helper-boundary failure

The parent moved campaign/source coverage from process_stage into its complete
load_run caller after provenance validation, preserving the original precise
provenance refusal order. Static inspection confirms that both AA and run
operations still call load_run and cannot obtain validity/READY output without
the complete role/source union checks.

Attempt01 passed the 36 seed checks and 152 existing independent review checks.
This new review reported four failures: the required-source, environment,
source-manifest and whole-leaf omissions had still called the narrower
process_stage helper. That helper now returns the verified union instead of
applying campaign/source coverage. The four mutations therefore reached no
refusal in this test despite still being refused by the actual admission API.

The exact previous test source, failed 35-check report, source tuple, compiler
flags and stdout/stderr are retained in
[matrix-attempt01-helper-boundary-failure](matrix-attempt01-helper-boundary-failure/failure_explanation.json).
The correction changes only the admit lambda to call the complete load_run API.
No cases, expected errors, thresholds or production guards change. The earlier
small Release PASS remains a receipt for its earlier production/helper tuple.
A fresh full-caller diagnostic and the parent's fresh final matrix are required.

### Full-caller Release diagnostic after the boundary correction

PASS: 35/35 checks through load_run; all 31 negative cases and their expected
errors are unchanged. Each source/role omission now reaches exactly
"campaign/source input absent from verified process closure". Compile exit=0,
test exit=0 and all frozen source SHA checks=OK. All original seed artifacts and
real source/campaign/test-source bindings remain unchanged.

Retained complete receipt/matrix:
[release-full-caller-smoke-03/diagnostic_receipt.json](release-full-caller-smoke-03/diagnostic_receipt.json),
[release-full-caller-smoke-03/review_result.json](release-full-caller-smoke-03/review_result.json).

Corrected independent test source SHA256:
e28cc5b64f48950caad693cd58a4f4593d097c456004369bc1433c5395e77b35.
The parent's fresh attempt02 three-mode matrix remains pending; earlier
attempt01 failures and prior-source diagnostic receipts are retained unchanged.
