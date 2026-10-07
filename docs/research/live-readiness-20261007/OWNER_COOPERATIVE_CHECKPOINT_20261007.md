# Sole owner checkpoint candidate — 2026-10-07

This candidate starts at `3f23faf8bb91a4a974c33463e7232d3507c1dcc1`.
The journal candidate's AA stopped at Tap 3: Up p99 15.596551 ms exceeds the
unchanged 15 ms gate. Its prior PASS and STOP records remain immutable.
Production Tap's single 2560-frame diagnostic passed; production Hold's
independent audit rejected an accepted-step join. That failure remains open
until its raw receipts are independently explained; it is not a cost PASS.

The new free extractor derives from the frozen v3 implementation without
editing it or changing Candidate layout. It hashes every RGB byte in the
same order, preserves padding exclusion, witnesses, descriptor signatures,
probe counts, capacity limits and relation policy. Every 32 RGB rows, at each
query and every 64 ROI depth samples it invokes the existing sole owner poll.
It never admits new pixels or delegates injection to another thread. A failed
poll stops extraction. The current QPC and execution ledger are re-read before
filtering; completed, cancelled and unknown execution cannot revive.

Checkpoint owner work is included in hook wall time, including its diagnostic
and backend cost. Existing observer, owner, coverage and phase distributions
remain separately reported. No p99s are added and no deadline is relaxed.

First validate exact donor extraction equality and fake-clock pending Up,
expiry, unknown injection and abort regressions in Release/Debug/ASan. Then
validate affected owner/Hold/runtime contracts and the exact retained 3063
pixels prefix. Only after those prerequisites run a fresh AA campaign. Old AA
is stopped permanently. A fresh AA must pass all 12 entries and its noise
predicate before any ABBA comparison. This change gives no live admission.

## Prospective capacity and core reuse

Keep the earlier 4 GiB journal candidate builds intact. Reserve a separate
6 GiB for this candidate's three fresh build/runtime roots plus 256 MiB total
stage metadata; each native stage retains the existing 64 MiB ceiling. Fresh
cost raw is bounded by 12 GiB, all retained prelive artifacts by 32 GiB, free
space by 20 GiB, with 1.5 GiB build and 1 GiB raw reserve. Record totals before
and after stages; stop before a limit is exceeded. No old receipt is redefined.

The formal core's 22 translation units and all files in `src` and `include`
are byte-identical to source 3f23faf. No core class/layout or compiler contract
changes. For each configuration, bind the existing qualified 3f23faf core
library, original compiler/configuration/build receipts and exact source/header
SHA comparison. Compile all new or affected candidate libraries and executables
from this clean commit in fresh roots. The donor is a core object reuse only;
old tests or costs are never called a new candidate run. If any compared file
or compiler contract differs, reject reuse and perform a fresh core build.

Source, configure/build/test receipts, all raw failures and the exact selected
input closure are retained. Device operations stay zero and the public typed
admission remains UNQUALIFIED until the full qualifications actually pass.
