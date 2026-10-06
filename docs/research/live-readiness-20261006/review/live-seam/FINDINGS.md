# Independent findings, before final freeze

All review work is bounded and endpoint-free. Windows compilation/execution,
device access, touch, cost qualification and physical gold remain NOT_RUN.

## Repaired source findings

1. The first OwnerCandidateStep implementation drained receipts and plans, then
ran diagnostic callbacks before ledger mutation. A throwing callback could
suppress receipt ownership/release handling. It now observes the ledger before
callbacks, and freezes finalization before finish-output callbacks. The initial
step source and seal are retained; independent output-exception controls pass.
2. Initial Packet eligibility trusted allow_down while omitting independent
scene-gate, result-UI and ended-status contradictions. Those three one-field
controls now fail closed, alongside context/round/new-round/staleness checks.
3. Production converted direct SessionGameOwner construction to make_unique
with an untyped braced GameActionOptions argument. Template deduction rejects
that call. The expected-failing C++ counterexample/compiler output is retained;
production now spells GameActionOptions explicitly. This targeted deduction
check is not a Windows production compile receipt.

## Preflight evidence and reviewer fixture failure

The separately sealed implementation overlay (two fresh objects ahead of an
unchanged same-ABI archive) yielded packet review 63/63 and runtime review
90/91. All implicit-release, uncertain-release, diagnostic-exception and
faulted-owner lifetime controls passed. Source seals checked unchanged.

The sole reviewer failure was `no_tail_cap_bounded_one_owned_Down`. The new
reviewer incorrectly assumed every control has one entry. The unchanged fixed
no-tail-cap corpus omits the cap during approach too; its already sealed
historical candidate family report has phases [0,0,0], and the preflight output
also has zero Down/Move/Up and zero normal-tail confirmation. This is an
incorrect reviewer fixture expectation, not a candidate regression. The
original source and nonzero output remain immutable evidence. Any corrected
review must be a separately named version, explicitly tied to that existing
historical zero-action gold; the original result must not be relabeled PASS.

## Source integration observations

- Runtime owns one OwnerCandidateStep and exposes references into its ledger,
  Hook, counters and output callbacks. Production uses that same type with the
  existing SessionGameOwner and a RecordingBackend around one GrpcTouch.
- Production retains Packet as shared_ptr<const Packet>. Candidate RGB is a
  shared_ptr<const Frame> pool lease, and CandidateBatch is copied before
  observer reset from the same consumed frame. New-round and context mismatch
  are ineligible; production suspends instead of submitting the stale scene.
- The outer action-worker lifetimes preserve game/step/receipt state into catch.
  Candidate catch calls sticky finish and never raw emergency_release_all.
- The root declares exactly one pas_core. pas_current_candidate links that core;
  pas_emulator links the candidate closure. Candidate headers, donor policies,
  current filter and receipt implementation enter generated build provenance.
- apps/pas/main.cpp intentionally remains unchanged and exposes only the donor
  manual-session call. This task adds an explicit hard-disabled production C++
  API seam, preserving the CLI/default donor as required. CLI candidate
  integration remains false and must not be claimed complete.

Final results are in RESULTS.md: latest three-mode reviewer checks 63+97 pass,
and the latest original 3063-frame raw audit passes. The implementation overlay
remains preliminary evidence only. A separate late expiry-order counterexample
and corrective shared poll-before-Hook boundary are fully preserved in
SEQUENCING_FINDING.md and sequencing-check/; its new six assertions retain the
original 91 controls and make the final runtime denominator 97.
