# Typed-tail implementation refinements and retained review failures

The source-declared contract in TYPED_TAIL_ONLY_PLAN.md remains unchanged and its
hash retained. These implementation refinements satisfy its negative cases;
they do not change the fixed family-v2 corpus or frozen v3/front/body thresholds.

- Normal current-body targets are evaluated first. A measured terminal-only
  candidate is transported only where that original body path rejects; it does
  not replace an otherwise valid body target or rewrite its tail semantics.
- The new line-aware producer uses the same cached65x15pixels/975probe bound.
  Six behind-edge rows must contain both real side rails and cyan interior,
  rejecting white-only or yellow-effect boxes. This is stricter current support,
  not a lowered rail, contrast, clustering or current-line threshold.
- TailTerminalProof binds full SceneContext, pixels-ready timestamp, note/line/
  candidate IDs, measured boundary, rail rows, single cluster, complete line
  count and producer probe count. Enum or reason string alone is insufficient.
- Owner retains old body evidence, source frame, expiry and last executed point.
  Terminal witness frame/time are recorded separately in confirmation diagnostics
  so an old body evidence timestamp is never falsely joined to a new frame.
- Frame ID and evidence time must both advance. Missing/invalid evidence or a
  >40ms gap resets the unconfirmed terminal history. Typed targets never enter
  alias, birth, visibility or body-Move/lease renewal paths.
- Independent first review reproduced4postconfirmation failures: an ordinary
  shifted or rotated body could Move, renew evidence and retarget the scheduled
  Up. A dedicated terminal_only_closing latch now makes that specific path
  irreversible Up-only. Gate/context/fault revocation still releases; ordinary
  non-terminal identities are unchanged. Closing Up remains bounded by the old
  body100ms lease and its already committed20ms deadline.
- Three further independent duplicate cases (tail/tail, conflicting tails,
  tail/body for the same note) were retained. Note multiplicity!=1 rejects and
  resets terminal confirmation instead of allowing order-dependent acceptance.
- New terminal arithmetic validates nonnegative evidence, MAX−100ms evidence
  limits and the existing scheduler's MAX−350ms horizon before additions.

The provisional independent reports and first archive remain separate; later
passes never overwrite them. Any final matrix is reported only after execution.
- Independent downgrade controls also reproduced3failures where reserved terminal
  markers were stripped or changed and ordinary body flags re-enabled the old
  path. A reserved terminal reason, payload or tag now requires a consistent
  typed channel; malformed combinations are discarded before alias/body/birth.
  Producer output explicitly clears head/body and predictive permission fields.
  Ordinary targets without any new terminal markers remain untouched.
- An existing owner alias43→42 exposed one more closing bypass: alias resolution
  could retarget42 after a typed Up commit. Closing checks now cover existing
  alias owners, exclude new aliases to closing owners, and run again before any
  per-identity expiry/body update. The exact failing alias fixture is retained by
  independent review; no alias was seeded or fabricated in that test.
