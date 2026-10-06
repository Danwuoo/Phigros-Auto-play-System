# Independent typed-tail-only review result

## Current verdict

Final v3 Release, Debug and Linux ASan+UBSan each pass130/130 owner checks and57/57
current-pixel checks. All source/archive/test hashes match before and after; both
instrumented test stderr files are empty. Leak checking remains disabled for the
previously verified host ptrace limitation. The final-source3,063-frame replay also
passes the latest independently rebuilt auditor, with complete plan/coordinate/input
joins and no integrity errors. This is not Windows, physical-tail, gameplay adoption,
cost/noise or live readiness qualification.

Fixed source closure tested:
- game.cpp:e6b8412e0c923321b19b8962df9159ca60d23c585f4a0943cc5a61c30e4a48f8
- game.hpp:5334b6d35696d09cb15d2a3482a3116ea3c737286a1bd16e7cd4ec2b48ee23c1
- current.cpp:810c60301604770dbcde12d312a60784489ea0bedcf5db95bbb761761c486691
- tail_current.hpp:45523b5c805569654721a53b3d2a24f016689f761bfa2f613c2f6d8419ac7d0e

The exact source/header/unchanged donor objects, pinned JSON, archive and test inputs
were verified before and after the run. The fresh same-ABI archive was built by the
portable-validation lane. Independent test expectations and earlier failures were
not changed to obtain the pass.

## Counterexamples found and repaired

1. The first draft allowed a fresh body after typed confirmation to add Move, renew
   evidence and retarget Up. Two cases failed four assertions. Closing now stays
   Up-only at the old point under the original evidence bound.
2. Identical or contradictory same-note tails and a mixed tail/body snapshot could
   incorrectly confirm. Three cases failed. Complete-note multiplicity now denies
   the typed path and resets its unconfirmed history.
3. Descriptions retaining held_body_evidence could lose/change the typed enum or lose
   both enum/payload while retaining the reserved reason, then enter ordinary body
   completion. Three cases failed. Reserved role, tag and payload must agree before
   any ordinary alias/body/birth processing.
4. An already established43→42 contact alias bypassed the closing filter when rewritten
   after it. The independent fixture used the real owner's reassociation event, not
   a seeded alias. The closing guard now applies through alias resolution and again
   before per-owner updates.

All failing snapshots/reports remain in evidence/tail-owner-provisional-01,
tail-owner-provisional-addon-01, tail-transport-downgrade-before-01 and
tail-existing-alias-before-01. The final unchanged checks are in
research/live_readiness_review/tail_only_owner_review.cpp and
tail_only_pixels_review.cpp.

## Verified boundaries

- Only same own-active Hold, current context/line, compatible geometry and a bound
  measured terminal payload can contribute to this path.
- No birth, alias acquisition, new Down, Move, body renewal or contact replacement.
- Distinct frames/times, at least10ms between two fresh proofs; absence, invalidity,
  line occlusion and region loss reset the unconfirmed sequence.
- Up remains20ms after confirmation at the last executed point, bounded by original
  body evidence. Confirmed closing is not mistaken for missing body; an unconfirmed
  witness retains the original missing60ms cancellation.
- Unknown Up remains unknown and is not retried. Prior uncertain-release behavior is
  handled separately by the already repaired Runtime finalizer.
- Pure white/yellow effects, missing rail/fill/tail, clipped ROI, multiple terminals,
  competing/duplicate candidates and lines, stale/projection/context mismatches and
  unowned/pending/completed/cancelled/unknown state controls reject.
- Rotated note/current-line geometry works through local coordinates without a new
  Move or fixed-old-line substitution. No physical semantic gold is claimed.

## Actual archived-pixel result and color limitation

Final source replay:3,063/3,063 inputs,55 Down/121 Move/51 Up, verified final release,
active0 and valid ledger. The current auditor source is49fac8a480e9828ada16d8833253071f8d8c21c341073b5f6ef4706dce6d2024;
its independently built verifier SHA is107902c7a21413bacbb16a67db877ea37e7c3fb87daaa4fc5582e92f77f6d92f.
The exact replay report SHA is
c35e755e0da8fcefd5fc40ced4c73bc9f2f7f4dfaff7b40bd19786f7f40c4c01.

The actual archived-pixel event stream contains **zero typed-tail targets, zero
tail-confirmed events and zero successful normal-tail Ups**. This is distinct from
the synthetic family's reported24 normal-tail completions. The separate bounded
C++ event counter has a positive own-receipt join control and two missing/mismatched
receipt rejection controls, all passing. Its inputs are hash-bound and unchanged.

The new terminal producer requires six cyan interior support rows. Ordinary body
continuation also has a neutral/gray contrast fallback; that broader body rule does
not establish terminal eligibility. Gray-gradient or occluded actual tails remain
unknown/rejected where current terminal evidence is absent. Synthetic blue-cap
completion does not establish all sprite-color coverage, legal physical release or
game adoption. No policy/raster was changed to make those actual frames pass.

The final audit and event counts are in evidence/final-typed-tail-prefix. The earlier
safety checkpoint has a different report/hash and remains in evidence/checkpoint-prefix;
it is never relabeled as this final candidate. Storage recovery used by the parent
preceded the final replay; no minimum-free-space guard or failure record was waived.

## Reproduction

Use the source-bound portable archive for the selected mode:

```
research/live_readiness_review/run_tail_reviews.sh <pinned-json-include> <archive> <source-before.sha256> release <fresh-output> <archive-sha256>
```

Use debug or sanitizers only with matching archives and complete source/header/flag
bindings. Sanitizer runs explicitly report leak-check availability. The runner has no
network/device API, checks the full source closure before/after, preserves native exit
and every report, and never substitutes Linux results for Windows qualification.
