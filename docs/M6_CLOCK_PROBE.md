# M6 host / Android clock observation

The touch fixture records Android `CLOCK_MONOTONIC` callback times, while the
Windows backend records QPC. Raw subtraction across these domains is invalid.
The existing touch analyzer continues to report cross-domain latency as unknown.

## Current local diagnostic

Two bounded read-only probes were executed on the explicitly checked
`phigros` / `emulator-5554` instance. Neither sent touch, launched an activity,
captured audio, or changed game settings. A small native helper was pushed to
a SHA-named file in `/data/local/tmp`, its device SHA was verified, and it exited
after the requested samples. The helper reads its boot ID and samples clocks.

| Probe | Samples | RTT min / median / max | Common constant-offset candidate width |
| --- | ---: | --- | ---: |
| A new ADB command for every sample | 32 | 57.806 / 84.879 / 122.914 ms | 51.628 ms |
| One outstanding PING at a time over persistent `adb shell -T` | 128 | 2.181 / 13.673 / 108.739 ms | 1.614 ms |

The persistent protocol validates the exact increasing request sequence before
reading `CLOCK_MONOTONIC`; its reply also records a bracketed Android realtime
clock sample, boot ID and PID. The host records QPC immediately before writing
and immediately after reading a complete reply, before a queue handoff. The
helper accepts at most 256 requests, at most 31 request bytes, and a 60-second
wall budget. The experiment requested 128 samples with 30 ms between responses.
The first sample includes process startup; it was not removed from statistics.

For sample i, let `h0[i]` and `h1[i]` be those host times and `a[i]` the Android
monotonic sample. Its causal host-minus-Android offset interval is
`[h0[i]-a[i], h1[i]-a[i]]` at that instant. Intersecting all intervals tests a
**constant offset hypothesis over these samples**. The nonempty intersection
does not prove zero drift, bound future offsets, authenticate a different app's
event stream, or establish rendering/song time. In particular, 1.614 ms is not
the measured input latency or a general live synchronization uncertainty.

The persistent sample's native binary SHA256 is
`9b02bc8ccafb3a33809efc72fe34c4a53f030934da9119379c546fc56793763e`.
It was built with NDK 30.0.16248370 / Clang 21, x86_64 API 29,
`-std=c++20 -Wall -Wextra -Werror -O2 -static-libstdc++`.
Python reports the host clocks as `QueryPerformanceCounter()` and
`GetSystemTimePreciseAsFileTime()`; the latter is adjustable and must not be
silently substituted for QPC.

Private results are in the workspace's
`outputs/local-assets/phigros-4.0.1/m6-clock-probe-002/` and
`m6-clock-probe-003/`, each with a report and hashed sample JSONL. The first
attempt (`001`) failed because its helper required an unavailable shared C++
runtime; its failure is retained. Scratch sources are
`work/android_clock_probe.cpp`, `work/android_clock_stream.cpp`, and the paired
Python scripts. These diagnostics are not automatically run by tests or live
input and have not been promoted to a production time-mapping API.

## Next use during fixture verification

Clock samples must bracket the actual fixture run, with explicit serial/AVD,
boot and session association. A later Android callback timestamp can obtain
an order-only host interval from surrounding causal samples without assuming
equal clock rates; this may be wider than a fitted constant-offset interval.
Any fitted/rate-bounded mapping must state and validate that additional model.
Results should retain intervals, unmatched events and mapping gaps rather than
turning their midpoints into a claimed precise input delay.

Android callback receipt, MotionEvent event time, RPC return and the game's
judgment remain separate observations. Even a valid callback-delay interval
does not calibrate Phigros input lead or Perfect timing.
