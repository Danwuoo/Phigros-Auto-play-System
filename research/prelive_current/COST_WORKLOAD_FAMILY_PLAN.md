# Closed-tail family: predeclared causal pass

2026-10-06, after commit189e1dd. This plan precedes the new family's full tests.
The previous singleton sweep and all failures remain unchanged. No timing/cost,
Windows campaign, device or formal/v3/front edit is authorized by this plan.

Source causes to distinguish:

- `observe_moving_held_front` intentionally clears tail except a current transverse
  closure near the line. The historical generator has side rails but no closing
  tail edge; its Up cannot validate normal tail completion.
- `current.cpp` uses zero body depth when tail is absent, even for active own Hold
  with current measured `held_body_evidence`. Frozen A's adapter rejects no-tail
  Hold queries outright. These are distinct from near-line white terminal choice.
- Fast entry can skip the narrow shared pixel-support phase. Immediate repeat
  can preserve completed identity; input-free scene clearing must precede reuse.

One family only, frozen before full testing:

- 1280x720 RGB black0; unchanged HUD; white line y499..501.
- Blue(40,190,255) rectangular Hold, width101, length161; white2px side rails and
  2px transverse cap at the trailing end. Fill drawn over line, rails/cap last.
  The visible leading end has no white cap, so its cyan→black witness is distinct.
- Each cycle:8 blank frames;8 approach heads460,464,...488;19 near-entry heads489..507;
  22 continuation heads515,523,...683;8 blank frames. Total65frames,20ms FakeClock.
- Vertical translations exactly{-2,0,+2}; all are tested and retained. Slow entry
  crosses the common pixel interval regardless of these offsets. No further
  position/color/shape search will follow this family.
- Eight cycles per positive A/B case (520frames); partition attempts1..256 as
  warmup and257..520 as measurement, retaining all rows. Require actual phase
  activity in measurement to claim the workload exercises that stage; absence
  is a failed capability result, never silently excluded.
- Negative controls at offset0, one cycle each: remove body after the entry
  segment; replace body with yellow effect after entry; remove transverse tail
  cap throughout. Always retain release/unknown/duplicate information.
- Compare own accepted plans, distinct note/intent identities, Down/Move/Up,
  active intervals, current tail proof and Up reason/timing. A safety Up is not
  normal tail completion; repeated same-position Hold need not emit Move.

Success means robust shared own entry/continuation/tail and measurement activity,
not merely a nonempty Down/Up count. Failure may establish an adapter/observer
contract contradiction that Windows timing cannot repair. Any current hook
repair must be separately proposed/approved with paired pixel and ownership
negative controls before editing. Frozen v3/front/thresholds remain immutable.
