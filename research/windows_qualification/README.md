# Windows cold qualification reader and maintenance wrappers

See [2026-10-07 results](../../docs/research/live-readiness-20261007/README.md).

The C++20 reader verifies externally pinned existing Windows **cold build** receipts. Success never enables `CandidateLiveAdmission`, opens a device, launches a process or claims measured cost. The CLI rejects invalid inputs with exit 2; a valid cold receipt returns JSON with `cold_build_verified=true` and `live_ready=false`.

`tests.cpp` verifies a genuine fresh build first, then rehashed malformed receipt controls without altering originals. `corpus.cpp` hashes the immutable tap/dense 12-frame and Hold 81-phase/three-offset RGB generators; it does not drive an owner or device.

PowerShell scripts are maintenance orchestration using the existing unchanged owned process/gate. The C++20 GUI subsystem helper in `functional-gui-source` is limited to endpoint-free functional tests and read-only controls. Its child exit is preserved; the original Job identity, held-handle, stream and quiescence checks remain active. It cannot supply a direct cost-producer identity or a live token.

The extra scripts here are byte-exact source copies of the **recorded 2026-10-07 attempts**, including finite Debug/ASan continuations and the preserved AA maintenance failure. See `recorded-recipe-copies.json` in the results directory for tested OUT paths and SHA joins. They are historical reproducibility recipes, contain deliberately fixed attempt names/paths, and reject existing roots. Do not rerun an old STOP. The original `run-aa.ps1`/`run-aa-gui-controls.ps1` AST import has the recorded empty `PSCommandPath` preparation bug; the actual fresh read-only review and maintenance continuation bind a file-backed caller explicitly. No original frozen recipe was overwritten.

AA Tap1 and Tap2 were qualified; Tap3's analyzer exited natively with 0 but failed the required Job member identity predicate (runner 125). Its numeric PASS is not qualification. Nine later runs, noise and ABBA were not run. Every raw attempt, error and STOP remains preserved. The local migration derivative adds only the explicit Windows JSON fixture encoding delta to the handoff's migrated formal source.
