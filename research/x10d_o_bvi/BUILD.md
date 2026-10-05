# Standalone reproduction

No root CMake or original test suite is built. Installed Community MSVC14.50.35717 and SDK are selected through its vcvars64.bat; installed Ninja/CMake and nlohmann headers only. Candidate and renderer/oracle/fake lifecycle are in one standalone executable, no product library/backend. Build jobs2, flags are part of the snapshot. Reproduction uses immutable normalized-execution.json, original oracle.json, typed-execution.json, supplemental.json and a fresh output filename.

Typed fixtures are explicit synthetic measured descriptors, independent of the RGB extractor. Their materialization and input authoring corrections preceded every build/run; initial normalized and typed files remain preserved. The first normalized/oracle/renderer/input contract preceded candidate source. Refinements discovered by source inspection are recorded in author-corrections.json without changing the oracle. This ordering refinement is disclosed rather than called an entirely finalized first specification.

Tap uses an8px rectangle centered at its declared center (normal -4..+4); Hold uses the declared front/back interval. Endpoint confirmation allows a bounded white-line interval when the actual bilateral blue/back and black/front flanks are observable. No line pixel is counted as blue support. RGB hash is a bounded signature, not a stored past image.

run.ps1 requires trusted exit and owned-job quiescence. Its first cmd selfcheck used a quoted shell command that cmd rejected; explicit argv selfcheck passed. There was no runner engineering repair or unknown exit. Build command .cmd wrappers have only fixed paths and native operations; they do not evaluate data or access any touch backend.

After a reproducible core negative, the family stops with no new PNG audit. Debug/ASan runs on the frozen same source are engineering verification of the same negative, never another candidate/threshold or acceptance claim. If any build is untrustworthy, dependent tests are unverified and the package stops at platform partial.
