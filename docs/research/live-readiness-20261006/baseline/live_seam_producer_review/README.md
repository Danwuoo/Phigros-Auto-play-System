# Final shared-seam cost/admission control matrix

Release, Debug and Linux ASan+UBSan each pass **36 seed + 152 admission + 35 input-closure + 14 actual-auditor synthetic controls**. The 152 comprises 123 unchanged-source checks and 29 semantic controls; the 35 comprises 31 negative controls plus positive/immutability checks. All use final source hashes in source-before.sha256, checked again after each mode and final completion.

These are explicit process/auditor test doubles and actual C++ raw-auditor controls, not Windows timing runs. Actual cost readiness and live readiness remain false. Original 65,536-byte native freeze, cost thresholds, diagnostic denominators and historical STOP/noise failures are unchanged. Leak detection remains unavailable under ptrace and explicitly disabled; ASan/UBSan have no final findings.

Synthetic-evidence.zip retains all 1,944 non-native payload files (69,100,473 bytes expanded), with CRC and per-file SHA checks; PAYLOAD_MANIFEST.json and matrix_report.json provide exact identities. The 15 completed native products are recoverably archived with SHA/CRC in ../../capacity/final-cost-binary-lossless-archive.json. They are not contained in the minimal source/evidence delivery.

Attempt01 stopped after four new reviewer controls still called process_stage after source/role coverage moved into the complete load_run caller. The actual admission guard was intact. Original source/report/failure are preserved in ../../review/input-closure/matrix-attempt01-helper-boundary-failure/. A single test call-site changed to the full admission caller; all31 cases and expected errors stayed unchanged. Fresh attempt02 passed all three modes.

The earlier producer_timing_review remains an immutable intermediate checkpoint; its original counts/source tuple are not relabeled as these final results. The new Windows wrappers themselves only have static source checks; no PowerShell parser, MSVC build, native campaign or device was run here.
