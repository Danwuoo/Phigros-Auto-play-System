# Independent disabled production seam review

Status: latest expired-first source independently passes all three reviewer
modes and the complete 3063-frame raw audit. A late fixed expiry-order
counterexample was repaired in the shared accept boundary and reverified; see
SEQUENCING_FINDING.md. Candidate admission remains hard-disabled.

## Final frozen-source results

- Release: packet 63/63, runtime-v2 97/97
- Debug/assertions: packet 63/63, runtime-v2 97/97
- ASan+UBSan: packet 63/63, runtime-v2 97/97
- Total: 480 check executions, all zero exit, with complete source/dependency/
  archive/test-input manifests checked before and after. LeakSanitizer was
  explicitly disabled under ptrace; there is no leak-free claim.

Latest outputs use expired-first-final-release-01, expired-first-final-debug-01
and expired-first-final-sanitizers-01. Their mode archives are byte-identical
to the complete 13-translation-unit checkpoint. The coordinator separately
verified actual GCC dependency closure, flags, toolchain and all archive-member
bytes unchanged; every reviewer executable is freshly compiled with the new
inline shared-step header and complete new-source manifest. These are verified
same-source archive reuse receipts, not a second full 13-TU recompilation.

The latest runtime-v2 source SHA256 is
6ea212284a2ffc8d170c261bc8d9316834f9e6cff5a7f8616cfac6ff46ad812d.
All original 91 controls remain unchanged; six appended assertions establish
actual owned Down, expiry-first release and no replay for each call order.

The latest original 3063-frame received-pixel prefix has 3063 rows, 9889 events,
27,594,746 event bytes and 227 own injection receipts: 55 Down, 121 Move, 51 Up.
The independently rebuilt unchanged auditor verifies all input processed,
complete plan/coordinate/prefix joins, metrics integrity and 140 release calls
without uncertainty. Final release is verified, active contacts zero and ledger
valid. Raw report, rows, events and input SHA companions remain byte-exact.
Latest audit outputs are in expired-first-raw-audit-3063. Its report SHA256 is
1c41c6a2842284d6c1bc5e28ad92fa2fb41dab55059003aaaffcd3db830d4683.

The unchanged independent tail counter finds zero typed-tail targets, zero
typed-tail plans, zero tail confirmations and zero normal-tail Up events, with
zero failed injection receipts. Those zero event counts do not establish a
tail-opportunity denominator or physical adoption. The physical opportunity
denominator remains unknown.

Prefix auditor producer/provenance joins are false because this received-pixel
report has no native cost-run provenance; its exact source/build/dependency
closure is sealed separately. Neither these diagnostic timings nor the raw
integrity result qualify a native Windows cost or live run.

## Findings and preservation

See FIXED_PLAN.md and FINDINGS.md. The initial production source seal/excerpts,
initial callback-order defect source, and expected-failing construction probe
are retained. Production repairs preserve the unchanged current Hook, tail
filter, formal owner and renderer semantics.

The initial reviewer source remains `live_seam_runtime_review.cpp`. Its first
run is 90/91, retaining the erroneous no-tail-cap one-entry fixture assumption.
The original, already frozen family-v2 report supplies no-tail-cap zero-entry
gold. Separately named `live_seam_runtime_review_v2.cpp` restores that exact
denominator; its preflight run is 91/91. Packet review is 63/63. Both have
before/after exact source/object/archive/dependency seals and zero endpoints.
Preflight uses two explicitly identified fresh objects before the unchanged
archive; it is not final complete-archive evidence.

The first complete matrix (63+91 in all three modes) remains the pre-expiry-fix
checkpoint. Its six completed reviewer ELF files are byte-preserved in ZIP
archives with SHA256, CRC and original-path/executable-mode restoration maps;
only those materialized transient executables were removed. All raw evidence,
source snapshots, reports, original failures and new 97-check preflight remain.
See before-expiry-reviewer-elf-preservation/receipt.json. No commit or push was
performed by this reviewer.

## Scope and remaining qualification

The new route is a production C++ API seam. Existing CLI/default donor remains
unchanged, and candidate CLI integration remains false. The only admission
state is UNQUALIFIED; its wrapper rejects before configuration, preflight,
endpoint discovery or backend construction and cannot silently select donor.

Independent controls cover immutable lease lifetime/three-slot exhaustion,
same-frame/context/round/UI rejection, receipt overflow-stop, implicit release
with diagnostics absent or throwing, original issued-versus-returned commands,
backend exceptions, first-unknown release, repeat finish and verified release
with a still-live faulted owner. Existing typed-tail/alias/body/closing/cursor
matrices remain separately required at their unchanged denominator.

The reviewer does not execute the Windows production adapter or its cross-thread
capture/perception/Packet/action wake path. Cold shared-owner tests cannot
qualify production packet-hop latency, real QPC/RPC/capture/pool contention,
Windows builds or physical game adoption. Windows cost, physical, live and
Chapter Legacy all-track full-IN Miss=0 goals remain NOT_READY.
