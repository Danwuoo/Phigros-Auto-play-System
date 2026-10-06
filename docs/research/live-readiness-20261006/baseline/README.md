# Accepted baseline provenance audit, 2026-10-06

This is a new, read-only maintenance audit of the accepted evidence. It does not
rewrite the frozen Windows records, execute Windows binaries, reproduce runtime
cost measurements, operate a device, or establish live readiness.

## Verified result

`provenance_baseline.json` records 61 passing checks with exact paths, SHA-256
identities, observed values, and limitations. `compact_zip_audit.json` records
archive validation and isolated extraction. Both are generated, machine-readable
reports; the old result documents are not used as proof by themselves.

- Baseline Git commit: `e9e305e3aba13f05a786c74540419428610b2f76`.
- Its immediate and only parent is
  `f07dc408385aaf70e1885a2734fc51097d6ed840`. Exactly five files changed:
  README, merge/package records, package documentation, and the compact package
  maintenance script. No candidate or product source changed in that commit.
- All 4,104 final prelive seal entries match exact bytes and SHA-256. Its own SHA
  is `f059078eba9fdf06ed66e9fe08176ce7531081ef44334f5eda2c227cd2518445`.
  The first index remains separately intact. Four historical seals independently
  match all 3,715 entries: 2,111 + 534 + 749 + 321.
- All 182 original closure stages are reconciled against archived receipt/state
  contents, including 164 verified, 17 retained failures, and one preflight.
  Failed native-zero identity STOPs remain failures.
- All fifteen accepted regression reports have their actual rows checked:
  Release attempt 14, Debug 05, and ASan 05 each retain
  113 / 56 / 113 / 68 / 186 passing results. Executable SHA bindings, source
  freeze references, exit receipts, and absence of skipped formal rows are
  checked. This verifies accepted receipts, not a new Windows test execution.
- The 3063-frame summary retains 10,314 raw proposals, 2,771 accepted observations,
  55 Down / 121 Move / 51 Up, 227 joined receipts, 49 current-body observations,
  361 current-terminal observations, 1,773 unknowns, and 9,889 event rows.
  The archived C++ raw-audit receipt is bound and inspected. Fresh C++ raw
  recomputation belongs to the separate current audit, not this hash audit.
- All 22 source identities (15 candidate and seven frozen v3) map to the immutable
  baseline. Three accepted Windows files use CRLF while their Git blobs use LF:
  `research/prelive_current/chain.cpp`, `include/pas/game.hpp`, and `src/game.cpp`.
  Explicit LF-to-CRLF reconstruction matches the accepted bytes and SHA exactly.
  The report never labels their Linux bytes identical to the Windows bytes.
  Working-tree SHA values are an observation at report generation, separate from
  the immutable baseline; concurrent new development may differ legitimately.

## Compact payload audit and extraction

The supplied ZIP is 1,001,668,802 bytes with SHA-256
`e59766f09879b927b26cd0947ce501cea05f16fd838cfb1cce9321a63a0d7530`.
All 13,670 payload entries match manifest bytes/SHA; all 13,679 total entries
pass ZIP CRC. Expanded size is 3,391,145,506 bytes; maximum entry expansion ratio
is 42.9934. Path containment, file/directory conflicts, case-fold collisions,
symlinks/special files, encryption, size bounds, and undeclared files are checked
before extraction. Each extracted file is hashed again and stored without an
executable permission bit.

One Windows reserved-name hazard is deliberately reported and preserved:
`Phigros-Auto-play-System/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p/source-snapshot/apps/runtime_x11_p/aux/CMakeLists.txt`.
Therefore direct Windows extraction is not certified. This harmless historical
source path is retained under a fresh POSIX quarantine root; it is not renamed,
deleted, executed, or restored over a checkout.

Local input: `/workspace/shared/phigros-prelive-input/phigros-prelive-compact-20261006-f07dc40-01.zip`.
Verified isolated extraction: `/workspace/shared/phigros-live-restored-20261006`.
Important relative paths:

- Results and raw events/trace/Journal:
  `PAS-zero-miss-r4-20261006/out/prelive-20261006/`
- Process receipts: `PAS-zero-miss-r4-20261006/out/windows-handoff/`
- Original PNGs:
  `Phigros-Auto-play-System/measurements/game-assist/2026-09-30-m0-manual-continue/full-recording36g-01/sessions/manual-session-22885039263800/full-recording/frames/`

The final external closure accounts for 4,088 references: 749 retained bytes
verified and 3,339 explicitly omitted artifact identities matched to
`EXCLUDED.json`. The complete ZIP has 3,585 deliberate omissions. Final candidate
binary manifests account for 50 retained verified files and 15 omitted libraries.
All 3063 original selected PNGs are independently hashed against the frozen
selection. The package is not a complete standalone rebuilding environment.
Host system DLL bytes are absent; archived backing-file hashes do not measure
loaded memory images.

## Preserved negative results and missing older data

Three original legacy reports each retain 356 layer-cases / 3938 assertions /
36 failures, with identical failed-row content and aggregate false. The 2742-frame
failed prefix remains failed. Writer loss remains declared and prefix evidence
incomplete. The receipt-count corruption control remains rejected. A/A retains
five runs with the fifth Hold run at zero actions/lateness samples; ABBA remains
unrun (36 planned). These negatives are never promoted to success.

The 41 recent observable STOP state files are hash-verified and individually
inventoried. They are separate from the older 71 original STOP identities in
`evidence/external-originals.json`. All 71 identity records are sealed, but none
of those 71 original historical files is included in the supplied compact ZIP.
Their original bytes cannot be independently reverified here. That is a missing
external-payload limit, not an assertion that the old STOPs were lost or changed.

Cost remains `NOT_READY`; physical adoption remains `unknown`; the live CLI hook
is false and the product goal is incomplete.

## Reproduction and maintenance tests

Run from the repository root:

```sh
python docs/research/live-readiness-20261006/baseline/audit_compact_zip.py \
  /path/to/phigros-prelive-compact-20261006-f07dc40-01.zip \
  --output /fresh/report/compact_zip_audit.json \
  --extract /fresh/empty-or-absent-posix-quarantine
python docs/research/live-readiness-20261006/baseline/audit_baseline.py \
  --restored /path/to/verified-quarantine \
  --output /fresh/report/provenance_baseline.json
python docs/research/live-readiness-20261006/baseline/test_maintenance_audits.py
```

The extraction destination must not exist. To audit the ZIP without extracting,
omit `--extract`. Omitting `--restored` from the provenance audit deliberately
reports external bytes as not supplied. Scripts require normal Python execution,
not `-O`. Python is used only for maintenance hashing, archive handling, and
receipt/manifest audits; formal analysis and implementation remain C++20.

Tests cover ordinary paths, fourteen malformed/traversal/Windows-unsafe paths,
explicit AUX quarantine classification, and an assertion-row tamper in a
throwaway copy. The tamper is rejected by both the final seal and accepted-result
checks, and the original frozen file's SHA is confirmed unchanged.
