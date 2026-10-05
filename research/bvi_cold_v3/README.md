# Isolated BVI cold candidate v3

C++20 research-only candidate derived from cold v2 at `a53a9b7`; no production hook, transport or touch backend. See [round4](../../docs/research/zero-miss-20261005/round4/README.md).

Core translation units: `bvi.cpp`, `contact_policy.cpp`, `relation_policy.cpp`, `constraint_policy.cpp`; headers `bvi.hpp`, `contact_policy.hpp`, `relation_policy.hpp`. Never combine v2/v3 headers and objects. Final core SHA is in round4 `evidence/final-core.sha256`.

The input requires legal, same-frame measured current ROI/lines; predicted or selected-only lines do not satisfy the contract. Aliases are exact measurement equivalence, not ownership. Fake Guard states are not production receipt validation. Stationary approach and moving front displacement remain bounded 3 samples/30ms; pure rotation at fixed front is not independently qualified. Capacity is128 ROIs/16 lines/6 samples/90ms, metadata below1MiB.

Run `run_regression.sh` for immutable original3938, v2contract339 and old independent79; `run_new_contracts.sh` for relation74/contact167/line-ambiguity16. Independent101 and5additional controls have independent runners under round4 evidence. Runners require fresh output paths and preserve native failures. All exact commands/dependencies are in round4 VALIDATION.md. A full legacy invocation correctly exits1 because36 old-contract failures remain.

`run_cost_probe.sh` times only synthetic extract+relate+constrain; `run_scheduler_preservation.sh` reruns the existing fake-clock probe with its previously frozen Linux clock shim. Neither qualifies Windows, owner integration, gameplay or Miss=0.
