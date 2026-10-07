# Owned diagnostic writer and bounded lossless archive — 2026-10-08

This candidate starts from frozen `c9783feb2bd8c2a9ccf3fa4f2a328f50c286f20e`.
No emulator, physical touch, model, training, purchase, new goal, or live
admission is part of its cold development. Default admission stays UNQUALIFIED.

## Preserved failures and measured diagnosis

`expaa01` completed all twelve R2 necessary-validity checks, but its dense
capture-to-owner p99 noise was inadequate: 21.175613, 16.678976, 18.254905,
16.475117 ms, with 2d=8.993274 ms. ABBA was NOT_RUN. No unchanged rerun may
replace that result. The preceding cooperative Tap3 STOP and pre-fix negative
expiry test also remain retained under their original source/binary bindings.

The independent 1000-frame `phaserun01` diagnostic from source
`34eddc176c5234e5e4d0d779ad64299a34e8685c` joined all 1000 owned frames, 744
after warmup. Current-scene JSON construction p50 was 1.81875 ms and candidate
batch construction p50 was 1.22735 ms. Their output callbacks p50 were 0.5708
and 0.37275 ms. Same-frame unmetered wall p50 was 0.2502 ms. Thread cycles
corroborate execution work, without conversion to QPC or causal claims about
CPU scheduling. GetThreadTimes samples show coarse jumps and do not explain
individual tail events. Those diagnostics grant no cost qualification.

Literal production `expnormal01` Tap and Hold each completed 2560 attempts,
raw v4 joins, and necessary numerical checks. Dense stopped at published frame
1803, with 32 plain event segments retained and `session archive unavailable`.
The existing writer has a 32 x 16 MiB per-round limit; its original fault is
masked by a subsequent admission exception. That production batch is STOP and
must not resume unchanged. No dense report or complete round summary exists.

## Changed contract

`DiagnosticSnapshot` clones only bounded current scene/batch scalar values.
It never owns pixels, Frame leases, an observer, action history, an owner, or a
callback. Sources are checked before copying (16 lines, 128 targets/candidates,
512 bytes per string). Owned capacities and a conservative allocation charge
are checked after copying. Each immutable snapshot charges 256 KiB. The mailbox
remains at most 16 MiB plus one writer-owned in-flight item; both count and byte
overflow retain the original critical-fault/debug-drop behavior.

The sole existing writer builds the same complete JSON using the unchanged
decision_json/candidate_batch_json functions. Exact values, source QPC fields,
producer order, metadata, current/filtered distinction and allowed flag remain.
No event field is sampled away. Writer output is not a strategy input. Writer
failure can still stop/release through the existing diagnostic fault boundary.
End-to-end wall time, full writer drain, RSS, skips, failures, jitter and all
attempts remain measured; no stage quantiles are added or discounted.

Current-candidate SessionArchive uses streaming gzip level 1. Legacy default
archives remain plain. Compressed rounds have <=256 segments, each <=16 MiB
decoded; <=4 GiB total decoded; <=512 MiB total physical; <=1 MiB per event row.
Before opening another segment it reserves its worst physical bound within the
same 512 MiB cap. The writer retains the existing 8192-item/16 MiB mailbox.
One fixed 64 KiB decoder buffer and one <=1 MiB row are used by readers. Gzip
magic, CRC/truncation, per-segment decoded limits, contiguous segment names,
physical/decoded totals and SHA bindings are independently checked. Compressed
Windows rows reconstruct the original CRLF bytes. No old raw is overwritten.

The 1000-frame profiling journal was 188357163 bytes. A 10000-frame dense
stress journal would exceed its original 1 GiB plain-output bound. Therefore
only the explicitly finite n=10000 research stress journal uses gzip, with
1 GiB physical, 4 GiB decoded/charged-output, 65536 records and the original
262144-byte charged row limit. The standalone auditor reads all decoded rows
and accepts this encoding only for requested=10000. Normal R2 n=2560 keeps its
original plain journal, 1 GiB output bound, protocol and proof predicates.

The first archive error is sticky, subsequent admissions report that cause,
and an independent bounded archive-failure receipt is attempted. Failure to
write that receipt cannot replace the first error or trigger a touch retry.
The game analyzer accepts existing decision_schema 4 and both archive formats.
Its standalone plain inputs and aggregate physical/decoded sizes are bounded.

Fresh v3 literal production boundary probe and v5 independent auditor retain
the real production Packet/Wake/pool/perception/action/supervisor/SessionArchive
topology. They decode every gzip row and retain the v4 receipt/cancellation,
same-source capture, producer denominator, all-phase and numerical predicates.
They cannot grant a production token. All formal src/include changes require
fresh core compilation; donor formal_core objects from c978/3f are prohibited.
The original 22 core translation units remain; source-hash inventory now also
includes the cooperative extractor/checkpoint and new diagnostic/read headers.

## Verification and storage

Tests cover exact values after source mutation, pre-copy capacity rejection,
interleaved event order/metadata, byte-pressure debug/critical behavior and full
drain, >32 lossless segments, CRC/truncation/magic/decode-limit rejection, sticky
plain-segment failure, compressed current decision schema and original round.
Existing six journal encoding cases and affected expiry/owner tests remain.
Test fixtures and all new native receipts must be preserved at fresh paths.

Before AA: all affected Release/Debug/MSVC ASan groups, the unchanged 3063-frame
prefix, current tools and actual full root source build must complete. New AA
keeps exact R2 corpus, cadence, denominator, thresholds and ordering. No prior
source/binary noise, comparison, production costs, physical or live result is
borrowed. ABBA requires this candidate's own adequate AA.

Capacity: fresh candidate build/runtime 6 GiB; fresh cost raw 12 GiB; native
stage metadata 64 MiB; all new metadata 256 MiB; retained prelive total 32 GiB;
minimum free space 20 GiB. Native input closure stays <=8192 per launch and held
identities <=64. No deletion or restoration of unavailable historical raw.

These are implementation changes pending cold verification, not acceptance.
