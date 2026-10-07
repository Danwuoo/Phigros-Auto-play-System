# Windows cold qualification reader and maintenance wrappers

See [2026-10-07 results](../../docs/research/live-readiness-20261007/README.md).

The C++20 reader verifies externally pinned existing Windows **cold build** receipts. Success never enables `CandidateLiveAdmission`, opens a device, launches a process or claims measured cost. The CLI rejects invalid inputs with exit 2; a valid cold receipt returns JSON with `cold_build_verified=true` and `live_ready=false`.

`tests.cpp` verifies a genuine fresh build first, then rehashed malformed receipt controls without altering originals. `corpus.cpp` hashes the immutable tap/dense 12-frame and Hold 81-phase/three-offset RGB generators; it does not drive an owner or device.

PowerShell scripts are maintenance orchestration using the existing unchanged owned process/gate. The functional command envelope is limited to endpoint-free functional tests. `run-aa.ps1` uses the original direct-native producer/auditor/analyzer contract; command-envelope functional passes cannot supply its process identity. Every attempt has fresh paths and retained source/input/output hashes. The local migration derivative preserves the handoff and original source bytes, adding only the explicit Windows JSON fixture encoding delta.
