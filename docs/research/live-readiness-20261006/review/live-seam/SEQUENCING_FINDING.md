# One late, parent-requested expiry-order counterexample

No production source was changed for this probe. The source/renderer/archive
before/after seals and complete output are in sequencing-check/. This is one
paired boundary case using unchanged cost_family_v2 positive offset-0 pixels,
not a new generator or a replacement for the frozen reviewer suite.

GamePlanOwner::accept (src/game.cpp:1329–2040) does not poll. Actual backend
dispatch occurs in GamePlanOwner::poll at lines 2042–2043. Current Runtime calls
step.poll/sync before Hook; the disabled production candidate calls step.accept
before its bottom-of-loop poll. The shared accept itself only polls afterward.

The fixed RGB corpus establishes a real sole-owner Down at 1620 ms. Its gate
and body evidence expire at 1720 ms, also the reported old next_due. The next
same-context frame is captured at 1640 ms but consumed at owner time 1720 ms,
so the packet is still valid at age 80 ms. There are no fabricated receipts,
plans, cursors or assumed background touches.

- Production-style accept-before-poll: Hook accepts one current body target;
  the old active plan can refresh evidence and defer its due release before
  polling. active_after=1, ledger_state=ACTIVE, Down=1, Up=0, ledger valid.
- Runtime poll-before: the exact 100 ms expiry first causes backend release;
  active_after=0, ledger_state=CANCELLED, Down=1, Up=0, ledger valid.

The Down receipt already exists in both paths. The expiry release is due but
has not yet executed when accept begins; this does not claim another thread
performed an Up. Ledger and actual execution prefix remain consistent. The
finding is expiry linearization, rather than a false receipt-prefix join.

This initial production order could not be treated as the same expired-first
scheduling semantics as the tested Runtime. After the first complete frozen
global sanitizer run finished, the parent applied the narrower common fix:
OwnerCandidateStep::accept now calls poll/sync before raw/filter/Hook handling.
That guarantees every caller takes the expired-first boundary. Runtime's
existing outer poll remains, and no policy, threshold or original oracle moves.

The complete before-91-check runtime source and before-step header are retained.
The same runtime-v2 source appends six checks for this one paired boundary case,
giving 97. The corrective small Release run passes 97/97; rerunning the original
diagnostic probe now gives active_after=0, Down=1 and ledger CANCELLED for both
call orders. Those fresh corrective outputs are in sequencing-fix-preflight/;
the original active_after=1 counterexample is still in sequencing-check/.

New final-source reviewer and full-prefix lanes are separately named
expired-first-final-* and expired-first-raw-audit-3063. Old evidence is preserved
as the pre-correction checkpoint and cannot serve as final-source qualification.
Current hard-disabled admission is unaffected. Windows production execution and
cost, live and physical qualification remain absent.
