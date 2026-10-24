# HUD reliability: offline investigation

The current playing template loses valid-looking HUDs when moving white judge
lines change its background pixels. This is a feature problem, not evidence that
the manual lifecycle should retain a stale playing state. The investigation below
does not change the frozen classifier, templates, thresholds, manual lifecycle or
supervisor. No device was operated and no audio was processed.

## Inputs and reproducibility

The source is the existing 4,789-frame video
`outputs/recordings/dlyrotz-in13-20261023-02/capture.mkv`, SHA-256
`9ef128e5072eae782d1c4aec373403926bc07af4c6497e9519856e5e2a90bcc2`.
Its full C++ manual-mode replay is
`outputs/local-assets/phigros-4.0.1/m7-manual-recording-001/`.
The frozen dataset is `state-calibration-001/dataset-with-paused-heldout.json`,
SHA-256 `d6b0a2735f556159779347c134598d2b831bed3ba5eb181a31bc0123214538e1`.
PTS identifies positions within this recording only; it is not a host/capture clock
map or a song synchronization result.

Research files remain outside the repository at `work/hud-reliability-001/`:

- `measure.cpp`, `CMakeLists.txt`: MSVC Release C++20 `/W4 /WX` diagnostic DLL.
- `analyze.py`: decodes only the video stream with PyAV 19.0.2; measures both
  playing anchors on every original frame and a literal legacy white-component
  subset. `results/frame-metrics.jsonl`, `summary.json` and `source-hashes.json`
  preserve per-frame measurements, run grouping and source hashes.
- `results/frame-*.png`: unmodified representative decoded frames and ROI crops.
- `candidates.py`, `results/candidate-results.json`: fixed provisional policies
  and each controlled negative/positive outcome. No production policy is selected.

The analyzer requires a fresh `results` directory; retain or rename the prior
research directory before rerunning. The global Python 3.14 runtime uses the
task-local PyAV directory `work/media-python`. No dependency was installed by
this investigation. Example from the workspace root:

```powershell
cmake -S work/hud-reliability-001 -B work/hud-reliability-001/build -A x64
cmake --build work/hud-reliability-001/build --config Release
C:\Python314\python.exe work/hud-reliability-001/analyze.py
C:\Python314\python.exe work/hud-reliability-001/candidates.py
```

The video is the same recording used for earlier pixel calibration/research.
Full-stream coverage adds failure evidence, not an independent heldout benchmark.
There is no manual ground-truth label for every frame; representative frame
inspection and exact glyph/background measurements are distinguished below.

## Where the 372 unknown frames occur

There are 47 contiguous unknown runs. The 372 frames are 7.77% of the decoded
frames, but they do not all represent failed playing recognition.

| Group | Runs / frames | PTS range | Interpretation from representative frames |
| --- | --- | --- | --- |
| Initial transition | 1 / 46 | 0.965–3.072 s | Old result/loading/fade into the new HUD; frame 40 at 1.765 s is black. These should not be force-filled as playing. |
| In-round middle | 45 / 159 | 36.046–123.062 s | Selected frames show normal play with moving judge lines crossing the anchor ROIs. Fixed glyph foreground remains present in all 159 frames. |
| Ending transition | 1 / 167 | 128.065–133.702 s | Starts with a still-visible HUD/background change, then HUD fades to a blank background and the result UI appears. Most of this run is legitimate unknown. |

If each sampled unknown label is held only until the next decoded video frame,
the groups cover 2.158 s, 6.079 s and 5.692 s respectively. These are discrete
recording label intervals, not proven continuous device states or input outages.
For the middle group, frame counts by ten-second bin are:

| Bin start (s) | 30 | 40 | 50 | 60 | 70 | 80 | 110 | 120 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Unknown frames | 6 | 80 | 5 | 11 | 18 | 26 | 5 | 8 |

The largest middle runs are 42.257–42.679 s (13 frames), 42.933–43.389 s
(12), 47.713–48.156 s (12), 47.060–47.474 s (11), and 48.445–48.857 s
(10). They are repeated short interruptions, not one stuck capture.

The frozen lifecycle reported 3,085 PLAYING frames, 96 STARTING frames and 328
SUSPENDED frames. A fresh HUD must be debounced again after an unknown frame, so
classifier interruptions also cost reconfirmation frames. Increasing the 100 ms
evidence age would conceal these observations and would not fix the detector.
Future execution would retire its epoch on revocation; the unchanged UI round
must not be used to bypass that contract.

## Which anchors fail, and why

Current playing anchors are pause bars `[21,23,28,33]` and the bottom-right
`IN Lv.13` label `[1152,673,101,26]`. The frozen classifier requires both whole-ROI
grayscale MAEs ≤8 and each ROI's contrast ≥40. The right-hand anchor is a
difficulty label, not a score glyph detector or proof of chart identity.

Among the 159 middle unknown frames, 96 fail only the difficulty ROI, 57 fail only
the pause ROI, and six fail both. Contrast is not the failure: minimum contrast
among failing pause ROIs is 237 and among failing difficulty ROIs is 223.

For diagnosis only, each template ROI is split into fixed white glyph pixels
(all RGB channels >195 and channel range <35) and remaining background pixels.
These are 350 of 924 pause-ROI pixels and 330 of 2,626 difficulty-ROI pixels.
The metrics use exactly the frozen grayscale formula; no threshold was raised.

| Failed ROI in middle group | Frames | Median foreground MAE | Maximum foreground MAE | Median background MAE | Minimum retained white foreground |
| --- | --- | --- | --- | --- | --- |
| Pause bars | 63 | 0.55 | 0.82 | 33.44 | 100% |
| Difficulty label | 102 | 2.57 | 4.50 | 16.11 | 99.09% |

This measures preserved foreground, not inferred note positions. Representative
unmodified frames were inspected:

| Zero-based ordinal / PTS | Frozen MAE pause / difficulty | Foreground MAE pause / difficulty | Observed image |
| --- | --- | --- | --- |
| 1,032 / 42.207 s | 1.02 / 1.51 | 0.40 / 1.72 | Passing HUD just before the failure. |
| 1,039 / 42.439 s | 35.57 / 1.51 | 0.62 / 1.72 | Several white judge lines run through/behind the left pause bars. Bars and top-right score remain visible. |
| 1,046 / 42.714 s | 0.98 / 1.51 | 0.31 / 1.72 | Passing HUD after the lines move away. |
| 1,153 / 47.289 s | 1.01 / 34.76 | 0.36 / 3.46 | A diagonal white line passes through the bottom-right difficulty-label background. |
| 1,170 / 47.950 s | 1.01 / 34.42 | 0.36 / 4.40 | The difficulty ROI remains crossed by lines; visible glyphs survive. |
| 2,069 / 81.066 s | 1.18 / 10.32 | 0.47 / 2.02 | Smaller background change still exceeds whole-ROI MAE8. |
| 3,407 / 128.065 s | 7.83 / 20.27 | 0.62 / 1.91 | Notes/lines have disappeared; HUD remains while background changes. |
| 3,450 / 129.232 s | 96.14 / 46.22 | 220.71 / 178.38 | Blank background after HUD fade. Keep unknown. |
| 3,573 / 133.702 s | 111.65 / 73.88 | 232.53 / 191.36 | Result UI animates into place; its calibrated result match starts on the following frame. |

Thus there are two distinct failure mechanisms: moving material changes a valid
HUD's background, and genuine UI fade removes the glyphs. Filling the entire tail
or treating a blank/no-note image as playing would merge those cases incorrectly.

## Historical independent HUD measurements

At immutable main commit `2d479e17fa1566c089dce019cbb11eb883ba47fd`:

- `src/game.cpp:44–50` classifies white using RGB >195 and channel range <35.
- Lines 217–245 segment full-frame scale-2, same-color four-neighbor connected
  components, discarding components with fewer than three sampled pixels.
- Lines 872–875 count a pause bar when the white component ends inside the top
  left 12% region, has height >1.3% of image height, height >1.4×width and width
  <2% of image width. Score glyphs begin beyond 78% width, end in the top 12%,
  have height between 1% and 7% of image height and width <3.5% image width.
- Lines 1070–1077 use pause bars ≥2 and score glyphs ≥4, independently of a
  minimum note/line count; three distinct valid frames and global capacity
  validity are required by that old observer. Non-playing classes were explicitly
  unvalidated. It is not OCR or an authenticated score detector.

The scratch DLL reproduces the literal white threshold, full-frame scale-2 white
components and these bounding-box counts. It excludes other-color components,
their global capacity interactions, note/line scans, tracking and planning, so it
is a UI-only subset, not a successful execution of the old `GameObserver`.
No unbounded pixels/history are retained; source geometry is fixed and white
components are capped at 2,048.

That subset gives a candidate on 3,163/3,181 frozen playing frames, 123/159 middle
unknowns, 12/46 initial unknowns, 15/167 tail unknowns, and 0/1,236 result frames.
The 15 tail positives are only 128.065–128.468 s while the HUD is still visible.
The old counts also miss 18 already-passing frames and 36 middle unknowns.
At 42.439 s, lines connect the pause bars into a larger white component and the
bar count becomes zero despite the visible bars. Five middle unknowns have fewer
than four score components. This old segmentation is not a drop-in reliable gate.

A synthetic non-UI image containing two narrow white bars at the left and four
filled white rectangles at the right passes the old counts. The existing paused
reference and its three fresh samples reject, but all four paused RGB files are
byte-identical; these are not four independent visual conditions. Settings rejects,
and the full recorded result tail rejects. More varied paused/menu/loading
negatives and changing-score positives are still needed.

Directly cropping before connected-component segmentation would be a new policy:
it can cut a long crossing component into a fake bar/digit. A future ROI method
needs explicit boundary handling or a full-white-channel comparison, rather than
claiming equivalence with the historical full-frame components.

## Small fixed candidate experiments

The work-only experiments keep MAE8/contrast40 and the historical score count ≥4.
Applying the same numerical bounds to a different metric is a distinct candidate
policy, not an equivalent or qualified classifier.

- **Two-of-three:** the two original anchor pass booleans plus an independent
  top-right score-component count. Any two observations give a playing candidate.
- **Foreground pair:** both fixed white foreground MAEs ≤8 and both original
  ROI contrasts ≥40; background MAE does not vote.
- **Foreground pair plus score:** the foreground pair and score count ≥4 must
  all hold. This is a conjunction, not an OR of the old gameplay gate.

| Frame group | Frozen pair | Two-of-three | Foreground pair | Foreground plus score |
| --- | --- | --- | --- | --- |
| Frozen playing (3,181) | 3,181 | 3,181 | 3,181 | 3,163 |
| Middle unknown (159) | 0 | 148 | 159 | 154 |
| Initial unknown (46) | 0 | 1 | 0 | 0 |
| Tail unknown (167) | 0 | 1 | 2 | 2 |
| Result (1,236) | 0 | 0 | 0 | 0 |

These are candidate-positive counts, not ground-truth recall/precision. Every
candidate passes the seven existing playing samples and rejects the paused,
result and settings samples in the calibration dataset. Targeted controls matter:

| Controlled image / expected state | Frozen pair | Two-of-three | Foreground pair | Foreground plus score |
| --- | --- | --- | --- | --- |
| Uniform white / unknown | reject | reject | reject | reject |
| Uniform black / unknown | reject | reject | reject | reject |
| White diagonal lines only / unknown | reject | reject | reject | reject |
| Generic bars plus four filled rectangles / unknown | reject | reject | reject | reject |
| One copied pause anchor plus four generic score rectangles / unknown | reject | **false positive** | reject | reject |
| Only both template white glyph foregrounds rendered on a non-game black image / unknown | reject | reject | **false positive** | reject |
| Actual playing template with added crossing white lines / playing | miss | miss | candidate | candidate |

Two-of-three trades one real anchor for generic fragments; foreground-only loses
too much scene context. Both therefore have demonstrated false positives. Adding
the score conjunction rejects these particular controls but regresses 18 frozen
positives and still is only an unvalidated component count. None is promoted into
the classifier or into input permission. No age/debounce budget was relaxed, and
the blank/fading tail remains unknown rather than being held as playing.

## Smallest justified next implementation

The evidence supports a **diagnostic HUD feature producer** before another state
policy change: return immutable frame ID/timestamp kind, foreground presence/error,
local glyph-to-surround contrast, pause-pair geometry, score glyph-shape diagnostics
and a bounded-computation validity flag from the same actual frame. It should keep
absolute foreground brightness, because unrestricted brightness normalization can
make a dim HUD under a pause overlay look playing. It must not create a lease.

Foreground-mask measurements address the observed background contamination.
An independent score observation should verify glyph structure/placement, not
accept arbitrary filled fragments; the current video has an unchanged zero score
and cannot validate other digit shapes or score changes. Pixel templates for
denied states must retain priority or ambiguity rejection. A two-of-three vote
without these safeguards has already failed a controlled negative.

A reviewable next change would add these metrics and shadow diagnostics only,
with foreground/negative-space and line-connection controls, while collecting
independent playing, changing-score, pause, menu, loading and result examples.
After freezing a candidate, run both the complete existing stream and independent
examples without tuning thresholds on that validation run. Only then decide how
it supplies a fresh UI state observation to `ManualLifecycle`/`RunSupervisor`.
The existing three-frame debounce, immediate suspension, separate execution epochs,
known-release requirement and explicit rearm must remain intact.
