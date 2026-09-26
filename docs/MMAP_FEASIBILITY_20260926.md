# Emulator MMAP consistency audit (2026-09-26)

Status: **diagnostic-only** for the installed Android Emulator 37.1.11.0,
build 15917651. MMAP is excluded from Session and from eligible capture
recommendations.

The user's latest shortened-test revision leaves this conclusion unchanged.
No new MMAP long run was added; the r4 diagnostic case was not executed.
Earlier diagnostic pixels remain historical evidence, without producer
ownership or fence proof. See the [current comparison](CAPTURE_COMPARISON_20260926.md).

## Version and source evidence

- `emulator.exe -version` on this host reports 37.1.11.0, build 15917651.
- The installed `emulator/lib/emulator_controller.proto` and this repository's
  `proto/emulator_controller.proto` both have SHA-256
  `1D62C6BCAD5F06621F90EC2BF26C661BA769CCD0F1416B5314D25A68E04EEE5F`.
- In that exact distributed proto, `ImageTransport.handle` says that the MMAP
  **can result in tearing** (repository lines 1367–1370). The
  `streamScreenshot` contract says pixels are written directly to shared memory
  and the gRPC `Image.image` field is empty. It does not define a reader
  acknowledgement, fence, or ownership handoff.
- [The public Android Studio emulator proto](https://android.googlesource.com/platform/tools/base/+/refs/heads/mirror-goog-studio-main/emulator/proto/emulator_controller.proto)
  has the same warning, but that moving branch is not proof of the exact
  producer implementation shipped in build 15917651. A matching producer
  commit, including write/reuse ordering, was not established in this audit.

## Implemented diagnostic path

`GrpcCapture` creates a bounded private mapping, receives notifications,
copies the indicated bytes into an owned snapshot, validates capacity and
geometry, normalizes to RGB24, and removes the mapping on stop. The frame
records both notification receipt and snapshot copy completion; its
`capture_complete_ns` is the latter. `source_valid=false` marks that writer
consistency is unproven. The native Fixture's four-region identity and
complement checks can detect some mixed pixels, but a passing check cannot
prove absence of tearing between those regions or between checks.

Reproduce diagnostic acquisition with the installed native Fixture APK:

```powershell
out/release-v145/Release/pas.exe capture-bench --serial emulator-5554 `
  --capture-backend emulator-grpc --grpc-transport mmap --diagnostic-mmap `
  --width 1280 --height 720 --source-rotation 1 --fixture `
  --fixture-apk measurements/fixture_cpp_v2/pas-capture-fixture-v2.apk `
  --warmup-s 10 --duration-s 60 --output-dir measurements/mmap-diagnostic-NEWID
```

This command produces raw JSONL, a diagnostic PNG, a manifest, and a summary.
It remains a diagnostic measurement even if every sampled frame looks valid.
The 2026-09-26 five-second development diagnostic decoded 239 of 239 consumed
Fixture frames and found no visible four-region anomaly; all 240 received
frames still have `source_valid=false` because producer consistency is unproven.
Reconsider eligibility only after identifying the producer source for the
installed binary and demonstrating a synchronization or immutable-buffer
ownership protocol that excludes concurrent reuse while the reader copies.
