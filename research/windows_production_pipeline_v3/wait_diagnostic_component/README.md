# Production wait diagnostic component

This is an endpoint-free diagnostic build, separate from the current qualification
candidate. It imports the exact `main55` / `12ec0550e882e3b06751d67fa67c43dda3e9312b`
production `pas_core`, `pas_current_candidate`, and `pas_proto` libraries and the
original generated protocol/manual provenance. It compiles only three TUs: the
new production fixture main, unchanged `src/grpc_transport.cpp`, and the new
poll-gap controls. The controls have their own fixed 16-case denominator.
Original CPU584, wait43/diagnostic20/active12/QoS24, reader81/12/12, 75 regressions,
and production9 remain historical prerequisites; this package does not rerun them.

`source_build_commit` in the literal fixture report stays the base55 manual/session
identity. `diagnostic_component_identity` separately records the actual compiled
delta commit, clean-source flag, base55 commit, and new main/header SHA values.
The generated `wait_diagnostic_component_identity.hpp` supplies those five values;
the base `session_build_provenance.hpp` is not regenerated or relabelled. The build
binding links both identities, base Native proofs, immutable source snapshot,
actual libraries/headers, fresh objects and Native8 before/after closures.

After the owner freezes a clean main commit, the owner may run:

```powershell
pwsh -NoProfile -File research/windows_production_pipeline_v3/wait_diagnostic_component/build.ps1 `
  -FrozenHead <actual-clean-delta-HEAD> -Attempt main56-waitgap `
  -BaseProductionBinding out/prelive-pre-emulator-evaluation-main55-release/build-binding.json `
  -BaseCpuBinding out/prelive-noise-repair-evaluation-main55/build-binding.json `
  -BaseSourceSnapshot out/prelive-pre-emulator-planning/source-12ec055-main55-snapshot/snapshot.json
```

The script configures/builds the two component targets and runs only the synthetic
16-case controls. It never launches the producer measurement, AA, ABBA, reader,
emulator, device, or a thread. Output roots are fresh `out/prelive-wait-diagnostic-*`
directories, leaving all main55 builds, raw and STOP evidence intact. It uses the
existing Main06 offline Native guard, capacity policy, stream limits, immutable
compiler environment and before/after input closures.

A later owner-authorized producer diagnostic keeps the existing CLI operands:
`tap 2560 20 <fresh-output-root>`. Invoke it through the existing offline Native
guard with the new executable and component binding/source/dependency/library/
generated/runtime pins as actual inputs, then read its complete attempted
denominator and the original full-raw auditor. Do not launch it directly from this
README or treat a diagnostic result as a new Normal3 PASS. The collector observes
existing QPC samples; it does not change due times, poll caps, waiting policy,
priority, placement, QoS, workload, R2 formulas or admission thresholds.

All component qualification flags are false and cost is `NOT_READY`. Its schema is
`pas.production-wait-diagnostic-component-build.v1`, which is deliberately separate
from the bindings accepted by `LoadBuilds`, AA and live admission. The eventual
qualification candidate still needs its actual source/build joins, necessary
Normal3, AA12/noise18, ABBA36 and complete readonly verification. This round stops
at the emulator boundary without opening a new thread or operating a device.
