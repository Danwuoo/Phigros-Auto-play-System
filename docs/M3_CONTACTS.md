# M3: traceable offline contact compilation

`compile_phigros_contacts(chart, identity, geometry, options)` returns a complete
`CompiledContactPlan` only if every source note compiles and all resource,
lifecycle, geometry, and declared capacity checks pass. Any error returns no
plan. Diagnostics retain the source note ID, JSON pointer through ownership,
line/note index, song time, and offscreen coordinate where available. This is an
offline candidate compiler; it does not discover devices, supply live evidence,
or arm input.

Each note exclusively owns one contact ID. Tap and Drag use configurable short
presses at the note's judgment point. A Hold keeps its contact for the full
native interval and follows the current line pose. A Flick uses a configurable
visible stroke along the note-side normal, with the center at judgment time;
the default stroke starts 16 ms before judgment, travels 48 reference pixels
over 32 ms, and releases 4 ms afterward. These timings, direction, ownership,
and Drag fresh-DOWN behavior require game calibration. No multi-note Hold
sharing, hardcoded ten-finger backend claim, or contact reassignment is used.

The compiler warns for each fresh Drag/Flick DOWN and performs a bounded sweep
for nearby Tap/Hold contacts in the configured temporal and spatial window.
Both source IDs are retained for potential interference. A warning describes
unknown game behavior; it does not invent a proven conflict or a proven safe
interaction. Input uncertainty and judgment windows are not inferred from these
compiler settings.

## Discrete trajectories, bounds, and identity

PAS dispatches discrete MOVE events, so a polyline interpolation bound would be
insufficient. For each indexed native event interval, the compiler bounds
translation speed plus rotational speed times the note/normal displacement,
plus Flick stroke speed. The largest viewport axis scale converts that bound
to input pixels. Motion-sensitive sampling reserves less than `sqrt(2)` pixels
for rasterization and bounds the error of the actual **last held coordinate**
between dispatches. The default limit is four input pixels. Static trajectories
collapse to endpoints; repeated raster coordinates can be omitted. Native
jumps and Flick center/end key times are retained. The bound assumes ideal
scheduled dispatch times; it does not include live scheduling/input latency.

Hold path visibility uses conservative sine/cosine interval enclosures and
bounded subdivision. A proven offscreen point is an error; an enclosure that
cannot be certified within its work budget is also an error. The compiler does
not treat opacity as permission to skip a Hold excursion. Event lookup and
boundary collection begin with binary search, rather than rescanning all pose
events for every note.

Memory points are raster pixels divided by `(input_width-1,input_height-1)`.
This deliberately agrees with the existing `pixel_point` and PAS bridge
rounding contract. The declared viewport and rotation remain in metadata.
Events sort by song time, then UP, MOVE, DOWN, then contact ID. Half-open
lifetimes allow an UP and a new DOWN at the same timestamp to share capacity.
The required peak is returned even when it exceeds declared capacity, provided
every note compiled. Configured capacity tests for 2, 5, and 10 contacts are
compiler checks, not proof of backend support.

`compiled_contact_fingerprint(plan)` computes the canonical v2 cache SHA256.
It includes every semantic plan field: exact chart/audio/layout identity,
compiler/strategy/policy versions, calibration status, geometry, options, all
ownership source pointers/indices/kinds/sides/judgment times/projection evidence,
complete Memory steps and sorted events, peak capacity, and declared counts.
Only the stored `metadata.cache_sha256` itself is excluded. Consistently mutating
both Memory and event coordinates, or changing ownership semantics, changes the
digest. This is a consistency fingerprint, not a signature or authentication of
arbitrary caller-provided chart/audio bytes. V2 intentionally invalidates older
cache identities; historical reports retain their original hashes.

## Offscreen candidates and current real-chart limits

The default policy rejects offscreen targets. The explicitly optional
`bounded_normal_candidate` policy requires a finite distance bound and a pending
`calibration_id`. It selects one bounded normal displacement at note judgment
and retains it throughout that note. The resulting metadata always has
`requires_calibration=true`, even if no note needed projection. Actual game
acceptance remains unknown. An input/export caller must preserve and enforce
this status; the older `Memory` value alone cannot express it. A normal
candidate is not a verified official judgment region.

The extracted Dlyrotz IN chart was compiled offline at 1280x720 and declared
capacity 10. Strict mode rejected the entire chart with these six source-note
errors; it did not silently drop any notes:

| Native source array | Note index | First rejected song time | Finding |
| --- | ---: | ---: | --- |
| line 1, notesAbove | 35 | 65.485714286 s | Judgment point y=-216 |
| line 1, notesAbove | 36 | 65.485714286 s | Judgment point y=-216 |
| line 3, notesAbove | 7 | 82.328571429 s | Hold enters offscreen line pose |
| line 3, notesAbove | 9 | 83.003571429 s | Hold enters offscreen line pose |
| line 4, notesAbove | 1 | 86.442857143 s | Hold enters offscreen line pose |
| line 4, notesAbove | 5 | 87.803571429 s | Hold enters offscreen line pose |

M2's start/end/midpoint queries identified four of these source notes; full Hold
traversal identified line 3/note 7 and line 4/note 1 as additional excursions.
A 250-reference-pixel normal-candidate experiment allowed the two line-1 Holds
with a fixed -216 pixel offset, but still rejected the other four trajectories.
No complete real-song contact plan or peak-capacity claim is available from
that rejected result. Private reports and the probe source are under
`outputs/local-assets/phigros-4.0.1/m3-contacts-validation/` in the task workspace.

The standalone MSVC Release `/W4 /WX` build passed 12 groups and 421 checks,
including all four note kinds, closed lifecycle and source ownership, capacity
2/5/10 and overcapacity rejection, simultaneous phase ordering, moving/rotating
Hold last-held error, 90-degree input rotation with a stretched subviewport,
98-second static Hold compression, gaps and paired zero-duration jumps,
offscreen/candidate policies, between-endpoint path excursions, interference
diagnostics, cache invalidation, NaN/time overflow, and bounded budgets.
Independent code review confirmed geometry/bridge pixel compatibility and the
conservative discrete motion bound; no blocking defect was found. Portable
pose/contact CTest passed 2/2 in the independent test project. No game input or
device compatibility was tested by these compiler checks.

## Versioned per-time normal-intersection candidate

The additional, explicit policy
`OffscreenPolicy::bounded_normal_intersection_candidate_v1` recomputes its
normal correction at each required song time. Strict remains the default, and
the original fixed-normal policy keeps its original contact semantics. The new
policy always has `requires_calibration=true`, including charts where every
native target is already inside. Its policy version, distance bound, pending
calibration identity, generated coordinates, and continuous projection
certificates are included in the v2 fingerprint. It is not a statement that the
game accepts an off-line contact.

For the full intended target `p(t)` and its side normal `n(t)`, the feasible set
is the intersection of the declared finite normal range `[-D,D]` with both
canvas slabs:

```text
S(t) = [-D,D] ∩ { c : 0 <= p.x + c*n.x <= 1280 }
              ∩ { c : 0 <= p.y + c*n.y <= 720 }
c(t) = element of S(t) nearest zero
q(t) = p(t) + c(t)*n(t)
```

An empty set rejects the whole plan. This is a line/canvas intersection, not
independent x/y clipping. For a selected analytic edge, that edge coordinate is
assigned its exact boundary to avoid floating-point cancellation; the other
coordinate must still lie inside its slab. No note is dropped. Flick's intended
stroke is included before projection, and a projected stroke with no observable
raster displacement is rejected.

Continuous native movement/rotation intervals are indexed and split at their
original boundaries and Flick key times. Interval arithmetic encloses both
slab limits. If the greatest possible lower bound is no greater than the least
possible upper bound, feasibility holds throughout that interval. Every
possibly active edge contributes to the velocity bound. An axis whose entire
`[-D,D]` normal tube lies inside its slab cannot constrain the solution.

For an active edge in axis `j`, let `V_p` bound native target speed and
`omega` its line angular speed. With `|n.j| >= eta > 0`:

```text
|c'| <= (V_p + |c|*|omega|) / eta
|q'| <= V_p + |c|*|omega| + (V_p + |c|*|omega|) / eta
```

The compiler uses a conservative correction envelope and the maximum viewport
axis scale. This bounds the actual zero-order held-coordinate error rather
than just a chord approximation. It reserves rasterization error and a finite
floating-point conditioning margin before choosing its motion-dependent sample
spacing. Native keys and jumps remain explicit MOVE events, even if projection
makes their raster coordinates identical. The certified minimum active normal
component is `1e-6`; active near-tangent intersections are refused. Ambiguous
branches, feasibility, or denominators undergo bounded subdivision (maximum
depth 24), then structured rejection if still uncertified. Work, step, and
provenance-span budgets remain finite; they are not bypassed using an unlimited
fixed 1 ms schedule.

Each ownership record preserves the judgment-time correction and declares
`time_varying_normal_projection`. Its `projection_min/max_reference_pixels`
are conservative signed correction bounds. `normal_projection_spans` records
time range, correction envelope, possible edge mask, and input-pixel velocity
bound. Edge bits are left=1, right=2, top=4, bottom=8, unprojected=16. These are
mathematical provenance for later bounded calibration, not verified hit regions.

## Full native-chart candidate validation

With `D=250` reference pixels, the extracted 584-note Dlyrotz IN chart now
compiles offline into **584 closed contacts, 4,911 steps, and peak 4 contacts**.
It carries 1,880 projection certificate spans. All six offscreen source notes
listed above are retained. Maximum observed absolute normal correction is
216 pixels. The candidate retains 255 fresh-DOWN warnings, six unverified
normal-projection warnings, and three nearby Tap/Hold-interference warnings.
This is declared compiler capacity, not measured Android/backend capacity.

The updated standalone MSVC Release `/W4 /WX` tests pass 18 groups and 462 checks.
They retain all original 421 checks and add analytic cardinal/diagonal/corner
intersections, above/below signs, interior Hold excursions, original native
jumps, inside/outside and corner branch switches, rotated stretched input,
empty feasible sets, near-tangent rejection, invisible projected Flicks,
bounded budgets, and canonical fingerprint copy/recompile/mutation cases.
Independent review found no blocking defect in feasibility, active-branch
selection, zero-order speed bounds, native keys, budgets, or v2 fingerprint
coverage.

The independent Python verifier reads the original raw chart bytes and the
exported compiled candidate, without calling the C++ importer, pose evaluator,
or contact compiler. It verifies all 584 source identities and closed
lifecycles, checks 4,911 step positions and 40,805 between-step positions, and
independently sweeps peak capacity. It also checks 32,945 sampled certificate
queries and 2,624 native motion-key occurrences. Maximum sampled held-coordinate
error is **3.543012645 input pixels**, below the declared 4-pixel limit. Maximum
sampled speed-certificate excess is `1.55e-13` pixels, consistent with rounding.
The raw checks identify exactly the same six projected source notes. This
sampled verification complements the continuous compiler bounds; it does not
prove actual game judgment behavior.

Private artifacts are in task-workspace
`outputs/local-assets/phigros-4.0.1/m3-normal-intersection-validation/`:
`compiled-candidate.json`, `check_candidate.py`, and `independent-summary.json`.
The raw chart SHA remains
`aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90`.
Strict mode still rejects six notes, and the old fixed-normal experiment still
rejects four. A candidate compiling all notes does not remove the six pending
normal-region calibration questions, unresolved fresh-DOWN interactions, or
live timing/backend qualification requirements.
