# M2: native v3 pose and input geometry

`PhigrosPoseEvaluator` indexes validated movement, rotation, and opacity tracks
once. An arbitrary song-time query, including a reverse seek, uses binary search
per track. No note query scans the entire chart. Song nanoseconds already include
the chart offset; the evaluator does not apply that offset again.

The reference canvas is 1280 by 720. Native movement `(x,y)` becomes top-left
origin `(x,1-y)`. Native positive rotation turns a line's positive x direction
toward screen-up: the pixel-space tangent is `(cos(a),-sin(a))` and its above
normal is `(-sin(a),-cos(a))`. A note's judgment point is the line origin plus
`positionX * 72` pixels along that tangent. Below notes reverse the normal,
without reflecting their along-line x position. At zero normal displacement,
above and below notes therefore share the same line-center target. Approach
speed and floor position remain native semantic fields; they do not move this
judgment point.

Intervals are `[start,end)`, and a later source event wins a shared start.
Zero-duration events step to their end value at their timestamp. Before the
first event the evaluator holds its start; between events and after the last
event it holds the preceding end. These endpoint-hold rules are explicit adapter
policies. Missing pose tracks, nonfinite fields, unordered events, and overlaps
are rejected. Raw opacity is preserved rather than used as an input gate.

`PhigrosGeometryMap` declares the actual backend dimensions, optional display
viewport, contain/stretch fit, and clockwise input rotation. Its default fit is
centered 16:9 letterboxing. The viewport uses upright display pixel-edge
coordinates, before rotation. A 90-degree rotation maps `(u,v)` to `(1-v,u)`.
Valid content edges rasterize inward to a pixel inside the content. Offscreen
targets are reported and have no input pixel; there is no implicit clamp or
normal projection. This mathematical point model is not an official judgment
region specification.

The signs and native scale were cross-checked against these primary community
implementations, without copying their algorithms:

- [phisap judge_line.py, pinned stable commit](https://github.com/kvarenzn/phisap/blob/8ac9a67d584ed2c34faccebd9ad35194331ec9d5/judge_line.py)
- [Phira native parser, pinned commit](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/parse/pgr.rs)
- [Phira below-line transform](https://github.com/TeamFlos/phira/blob/191b56e8404e26ab4bdf6c7d38dc577a265510a7/prpr/src/core/line.rs)

## Validation and remaining uncertainty

The standalone MSVC Release build with `/W4 /WX` passed 66 golden checks. Cases
cover static/moving/rotating lines, positive and negative cardinal angles,
above/below, a 60-second Hold, boundary/gap/zero-duration events, missing or invalid
tracks, NaN/infinity, a 33,000-event reverse-seek case, extreme timestamps,
offscreen coordinates, boundary pixels, viewport/letterbox/stretch, and all four
rotations. Normalized synthetic tolerance is at most `1e-6`.

For the privately extracted Dlyrotz IN chart, an independent Python scalar
implementation read the original JSON and compared 1,752 C++ queries: start,
end, and midpoint for all 584 notes. The evaluator indexed 31,267 pose events.
Maximum normalized target and pose-field discrepancy was
`1.1102230246251565e-16`; opacity discrepancy was zero. Four source notes were
offscreen among these particular query times. This is a sampled mathematical
check, not a proof that every Hold stays onscreen; M3's interval traversal found
two additional Hold excursions.

Private reproducibility artifacts are under
`outputs/local-assets/phigros-4.0.1/m2-pose-validation/` in the task workspace:
`probe.cpp`, `native-targets.csv`, `check_pose.py`, and `summary.json`. The
canonical chart SHA256 is
`aeadac362f9caca96eee66153b8c32f5a891113453635d1791d434768138ce90`.
Private chart, audio, queries, and image bytes are not included in Git.

The audio-inferred video frame near song 47.31 seconds has a lower line at about
5.4 degrees centered at y=576 and an upper horizontal line near y=307. Native
pose at 47.44 seconds gives line 0 at 5.454 degrees/y=576 and line 1 at
y=307.602, consistent with a **rough +130 ms video phase candidate**. Comparing
only native line 1 would miss this line transition. The alternatives shifted
by plus/minus 5.4857 seconds fit that frame less well. The independent diagnostic
script and query report are `check_phase_candidate.py` and
`phase-candidate.json` beside the other private artifacts. No geometry was
changed to fit the frame. One frame cannot establish a global offset, timing
uncertainty, input lead, or official judgment zero; the original baseline
overlays remain evidence of that unresolved timing alignment.

A second overlay pass preserved the original selected video frames and added
exactly `+0.130` seconds to the inferred native pose query time. Root's visual
inspection found both line slope and upper-line height close at 47.44 seconds,
stacked-line heights within a few pixels near 66.132 seconds, and upper/bottom
lines close near 105.127 seconds. The static 12-second frame was compatible;
the blank 83.28-second frame was not informative. These are coarse visual
cross-checks under an explicit phase hypothesis. Baseline overlays are retained
under `m2-video-overlay/`; the phase-adjusted pass is a separate artifact. This
does not convert the hypothesis into a calibrated audio/video offset, game
judgment timestamp, or input lead.

## Explicit compatibility import

The strict evaluator rules above remain unchanged. The opt-in import policy `phigros-v3-source-priority-constant-empty-candidate-v1` retains all original declarations separately and supplies disjoint evaluation segments for affected pose tracks. It uses first native source priority for active half-open intervals; constant reversed pose records are empty declarations with provenance. This is an unqualified interpretation, not an official-engine assertion. Unknown/nonconstant reversals and ambiguous mixed instantaneous tracks remain rejected. See [common policies](CHAPTER_LEGACY_COMMON_POLICIES.md).
