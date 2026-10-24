# Native fixture provenance

The five source/package files listed in `BASELINE.json` were restored byte for
byte from historical `main` commit
`2d479e17fa1566c089dce019cbb11eb883ba47fd`. Their Git blob IDs, byte counts, and
SHA256 values identify that baseline independently of future changes. The
existing C++ NativeActivity and its visual touch trace are reused; no Gradle,
Java framework, or new third-party runtime is required.

The baseline touch package is `org.pas.touchfixture.cpp`, version 2. It reports
DOWN/POINTER_DOWN, MOVE, UP/POINTER_UP, and CANCEL pointer IDs/coordinates, active
pointer state and counts through a 64-slot visual ring. Its overflow count means
old trace slots were overwritten; it is not a claim that Android lost input.
It does not contain input-event or receipt timestamps. The original capture
fixture remains a separate target.

The task-local offline baseline build uses Android NDK `30.0.16248370`, x86_64,
API 29, build-tools `36.0.0`, platform `android-37.0/android.jar`, and the installed
Android Studio JBR for package signing. Source and APK hashes distinguish this
baseline from any timestamp-evidence derivative. No installation, launch, or
device test is implied by successful compilation/signature verification.

## Opt-in timestamp derivative

`evidence/main.cpp` derives from the original visual fixture, with its source
commit/SHA in the file header. `common/main.cpp`, both original manifests, and
`cmake/PackageFixture.cmake` remain exact baseline bytes. The fixture CMake file
only appends a default-OFF `PAS_BUILD_TOUCH_EVIDENCE` option. Its separate target
and package are `pas_touch_fixture_evidence_v3` and
`org.pas.touchfixture.evidence.cpp`; the v2 package is not reused. The derivative
retains the visual trace, observes up to 32 pointer IDs, and adds a bounded
app-private JSONL recorder. A different package/source/APK identity is required
for any comparison.

Both an explicit evidence-target build and the runtime Android property
`debug.pas.touch_evidence=1` **at app start** are required to record. The property
is not read continuously and there is no automatic restart or re-arm. No
property is changed by the build. Without this opt-in, no evidence file is
opened. The recorder exclusively creates a mode-0600 file named
`touch-evidence-PID-MONOTONIC.jsonl` in the activity's private files directory;
an existing file is never overwritten. The package is debuggable to permit
explicit `run-as` retrieval during a future authorized device session.

Each MotionEvent record contains sequence, raw/masked action and pointer index,
all current pointer IDs/coordinates/tool types/pressure, surface dimensions,
source/device IDs, Android event/down timestamps, a CLOCK_MONOTONIC arrival
timestamp, and active IDs after the observed action. The record includes the
number of batched historical samples but does not expand them. Lifecycle
inconsistencies are counted rather than silently repaired into clean evidence.
The [NDK input contract](https://developer.android.com/ndk/reference/group/input)
specifies the event/down times in the Android `System.nanoTime()` timebase.
These timestamps and Android monotonic arrival are not Windows host QPC.
Without a separately measured clock mapping they cannot be subtracted from a
host dispatch/receipt timestamp or called game input latency.

Recording stops at 60 seconds, 8,192 MotionEvents, a 16 MiB file budget, app
pause/destruction, invalid pointer/time data, or I/O/clock failure. A footer
records the stop reason, events written, known first unrecorded event, lifecycle
errors, and active state at the last observation. Later events are explicitly
unobserved; the first-unrecorded counter is not a fabricated total drop count.
Missing/partial footer or I/O failure leaves incomplete evidence. A pause does
not fabricate an Android CANCEL. `zero_active_in_observed_stream` describes only
the captured fixture stream, not transport receipt or a game's judgment.

The original visual ring still overwrites after 64 traces and increments its
own overflow count. That visual overwrite is separate from JSONL limits or
actual Android input loss. Evidence collection performs bounded synchronous
file I/O in the input callback, which may add observation overhead; timing
results must retain that fixture identity and overhead limitation.

Build the timestamp target explicitly with the same NDK/ABI/API parameters and
`-DPAS_BUILD_TOUCH_EVIDENCE=ON`; select only
`--target pas_touch_fixture_evidence_v3 --parallel 2`. Package it using
`cmake/PackageEvidenceFixture.cmake` with explicit `PAS_SO`, `PAS_SDK`,
`PAS_JAVA_HOME`, and a **new** `PAS_OUTPUT` directory. The script only compiles,
packages, signs, and verifies offline; installation, property changes, launch,
input, and evidence retrieval are separate authorized operations.

The standalone portable pointer-ledger tests passed 19 checks with MSVC
`/W4 /WX`, including valid 2/5/10/32-pointer streams and malformed complete
snapshots. A review-found regression where POINTER_DOWN could replace an
unrelated active ID is covered explicitly: later UP events cannot erase that
recorded lifecycle error. The baseline and evidence NativeActivity targets
both compiled with the pinned NDK's Clang 21 and `-Wall -Wextra -Werror`; both
APKs passed offline signature verification. These checks do not establish
Android 16 runtime compatibility, actual multi-touch capacity, host clock
mapping, or release behavior on a device. No fixture was installed or launched at
that build-validation stage.

Later in the interactive session, the evidence v3 APK was installed on the
explicitly verified `phigros` / `emulator-5554` target. Its installed base APK
SHA256 matched the built artifact
`d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2`.
The separate local `m6-touch-fixtures/install-record.json` records this operation;
the earlier build manifest remains historical. Installation did not launch the
activity, enable the runtime recording property, or send any touch input.
