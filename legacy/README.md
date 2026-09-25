# Frozen pre-migration implementation

This directory preserves the verified 2026-09-25 handoff implementation for
comparison. `python/` contains its host code, tests and scripts;
`android-java/` contains the two Android fixtures; `web-fixture/` contains the
historical browser capture fixture. None is a production entry point for the
C++ implementation.

The exact source bytes and provenance are recorded in
`docs/CPP_SOURCE_SNAPSHOT_MANIFEST.json` (SHA-256
`F22BEE68D66FCED82AC8B0693E101491B465320CC748D19A8CB8A218E7E788A5`).
The import was committed before these files were moved, so Git can also
reconstruct the original paths. The source snapshot was checked file by file:
75/75 size and hash matches. `docs/CPP_EVIDENCE_INDEX.json` lists 217 existing
historical measurement files by absolute path, size and SHA-256. The 164 raw
files of `capture_idle_retest_20260925T052732Z` are absent; its report is only
historical testimony and cannot be recomputed.

Historical Python tests can be run for differential checks by setting
`PYTHONPATH=legacy/python` and discovering `legacy/python/tests`. They are not
part of the C++ runtime, build or acceptance gate.
