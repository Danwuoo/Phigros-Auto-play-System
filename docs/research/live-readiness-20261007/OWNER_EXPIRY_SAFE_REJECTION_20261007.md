# Owner source expiry and first failure — 2026-10-07

This candidate derives from frozen cooperative source 45f8a5f. Tap3 raw report
SHA 71120bf3c7863f6aa11123ff2fd88f25ca425873d2179889f454d0e249c5e74a
is preserved in the maintenance root. A bounded C++ diagnosis joins its full
2560 attempts and 9381 journal rows. The last owned attempt is 2181; attempt
2182 was skipped by the latest frame reader. Consumed attempt 2183 has capture
QPC 18013409226100, recognition end 18013436279300 and completion
18013532247100. Its capture-to-complete age is 123.021ms. There is a raw
observer/batch pair and no filtered scene for that attempt; the two retained
release reports are verified and no injection/release is unknown.

The frozen adapter has a fatal return for post-extraction source age >=100ms
while its ledger remains valid with reason `verified`. Thus the old report
cannot represent a unique first cause. The raw narrows the path to extraction /
checkpoint return; it cannot identify an OS scheduling cause or a particular
background process. The previous producer/publication overruns remain a
separate necessary failed gate.

The pre-fix regression in source 2814085bdfe7a3359b1651efa05fe154a3486db4
retains 14 fake-clock checks, with 3 failures: source expiry is treated as an
owner fault at 100ms and above. Its expected native/runner exit 1 and independent
verification exit 0 are preserved in maintenance `expneg04`; no raw is rewritten.

The new adapter distinguishes source expiry from owner checkpoint, ledger or
QPC reversal faults. Expired pixels clear targets and revoke the gate with the
original 100ms boundary. The same owner processes the revoke; no Down/Move is
admitted from expired evidence. A later fresh frame may be processed using the
existing ledger; completed/cancelled/unknown intents never revive. A final QPC
check before owner acceptance also covers delays in one-way diagnostics.
Owner faults and clock reversal stay process-sticky; the first failure stores
one bounded cause/location/QPC/frame/ledger/receipt/scheduler record. Later
release cannot overwrite it. Checkpoint count and maximum QPC gap are fixed
scalars; per-frame diagnostics remain in the existing bounded journal and are
included in complete chain wall time.

Validate the new fake-clock positive/negative cases and all affected owner,
Hold, tail, lifecycle and disabled admission contracts in Release/Debug/MSVC
ASan, then the complete retained 3063-frame prefix. Freeze a fresh AA12/noise
campaign with unchanged R2 denominator, 20ms cadence and original thresholds.
Never resume coopaa01 or repeat an unchanged STOP to select a favorable run.
Only qualified AA/noise permits ABBA36/C5. Source expiry rejection alone does
not establish producer cadence, physical, production pipeline or live readiness.

All src/include and formal core inputs remain byte-identical to the qualified
3f core. Reuse only each exact configuration's core library with before/after
SHA and compiler/receipt joins; candidate libraries/executables are fresh.
Any mismatch forces a fresh core build. Main, 45, 3f and the negative source
remain frozen. Capacity for new build/runtime roots is 6GiB; stage metadata
64MiB, total stage metadata 256MiB, fresh cost raw 12GiB, retained prelive
32GiB, minimum free 20GiB, build reserve 1.5GiB and raw reserve 1GiB. Native
input closure remains <=8192 files and process held identity capacity <=64.
No device operation or live admission is supplied by these cold receipts.
