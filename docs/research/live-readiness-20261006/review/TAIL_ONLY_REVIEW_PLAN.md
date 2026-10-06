# Independent typed-tail-only contracts, fixed before the seam patch

Checkpoint:4d9031c, current.cpp SHA96ef1254fd9045d7f89f1861e6d40c95f0dea6609f02b4c1705dd751435392d9,
runtime.hpp SHAd895b8f59fb844f0d0df361ad0355d39234c6d0312a8a0f1256066d5e5c8c7bd.
Scope: one bounded current-line-aware terminal measurement and own-active-only typed
transport. This review does not change the frozen front/body thresholds, raster,
workload, old oracle, lease lengths or normal-tail timing. No cost/device execution.

## Safety contract

A terminal-only observation may contribute solely to finishing the same already-owned
active Hold. It cannot create Down, Move, body/rails continuation, another contact,
alias ownership, renewed evidence, or a longer lease. Owner validation must hold even
if a caller supplies the typed field directly without passing the hook.

The existing normal-tail rule requires two distinct fresh observations at least10ms
apart; release is scheduled20ms after confirmation, subject to the pre-existing
expiry. An earlier safety release is distinguishable from normal-tail completion.
The original white-line-as-tail counterexample must remain rejected. The original
frozen family-v2 input remains untouched; success on it is software coverage, not
physical tail/adoption gold or equal A/B active-work qualification.

## Predeclared owner controls

1. Own active Hold plus two valid tail-only observations produces one normal Up on
   the original intent/contact, at the last executed coordinate; zero new Down/Move.
2. The first witness alone cannot confirm normal tail or refresh plan evidence/expiry.
3. Duplicate frame, equal timestamp and second witness9ms later cannot satisfy10ms.
4. A distinct witness at exactly10ms can confirm; before +20ms no normal Up occurs.
5. Incoming arbitrary hit position cannot move the active contact or alter release
   position. Malformed/non-finite geometry must not gain permission.
6. The existing body evidence and original bounded release deadline do not advance;
   a tail-only stream cannot keep the Hold alive indefinitely.
7. Unseen identity, pending zero-cursor identity and another active note cannot use
   typed-tail transport to create/borrow a Down or finish the wrong contact.
8. Completed, cancelled, retired and unknown-result identities cannot revive.
9. Wrong line ID, stale evidence, wrong context/geometry/generation, expired target,
   projected line and ambiguous/unsupported evidence cannot confirm normal tail.
10. An intervening contradictory current tail/line observation cannot be silently
    counted as a second positive. Review actual reset behavior without changing
    oracle after seeing results.
11. Unknown Up or final release remains failure with no Down/release retry and the
    first uncertain receipt retained.

## Predeclared pixel/hook controls

12. A true current terminal of the same own-active Hold can be transported when
    terminal supports are visible but the line contact has insufficient body rows.
13. The same raster before own Down supplies no terminal-only permission.
14. Missing current line, invalid/projection-only line and stale/mismatched complete
    frame/batch/scene never supply the terminal proof.
15. A second current line remains a competitor; candidate/line duplication or
    conflicting note identity cannot manufacture unique association.
16. White judgment-line boundary cannot masquerade as the terminal; preserve the
    saved line-boundary counterexample and existing tail-guard controls unchanged.
17. Missing transverse closure, missing one rail, cyan/effect contamination or
    occlusion stays unknown rather than extrapolated into a tail.
18. One-rail/body patch/unknown tail proposals cannot invent terminal identity.
19. Rotated geometry and a line that moves/rotates while held must use current local
    coordinates; a static old screen point/old-line proof cannot substitute.
20. At capacity and after termination, the path stays bounded and fail-closed.

## Evidence and stopping

Tests live under research/live_readiness_review, separately from the developer's
controls. Freeze this plan and test expectations before compiling the new seam.
Keep every initial failure and source hash. Use the shared portable lane only with
its exact pinned dependency/source closure; label all results Linux software-only.
If a positive requires raster or threshold tuning, stop with that negative result.
If an implementation exposes broader permissions, report the smallest counterexample
and require the same fixed controls to reject it before accepting the seam.
