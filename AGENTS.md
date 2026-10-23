# Chart-driven multigame branch

This branch supersedes the old vision-first development plan, not its historical results.
The immutable extraction baseline is 2d479e17fa1566c089dce019cbb11eb883ba47fd.

- Read README.md, docs/TECHNICAL_DIRECTION.md, docs/MIGRATION.md and docs/VALIDATION.md.
- Keep main untouched. Do not merge this extraction back into main without a new request.
- Preserve listed PAS sources byte-for-byte unless a task explicitly requires changes; record changes in provenance.
- Default builds and tests are offline. No device discovery, game launch, touch injection, automatic selection, or score submission from tests.
- Never label a synthetic dry run as Phigros/Project Sekai gameplay support, or carry old qualification/results over to this branch.
- Native charts, audio and replay recordings must be supplied with authorization; do not commit private charts, copyrighted audio, tokens or local calibration.
- Do not implement anti-detection, integrity bypass, or ranked/multiplayer automation here.
- Separate immutable chart identity from expiring synchronization evidence. A planner tick is not a fresh observation.
- Retain release-all, bounded resource use, epoch/geometry invalidation, actual pointer-capacity checks and a user stop path in future live integration.
- Run the portable CMake/CTest commands after changes. Report Windows, gRPC and device tests as NOT RUN when unavailable.
