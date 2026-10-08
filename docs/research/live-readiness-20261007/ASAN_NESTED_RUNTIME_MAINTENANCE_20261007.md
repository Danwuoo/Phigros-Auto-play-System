# Nested ASan executable runtime binding

Candidate source remains `3f23faf8bb91a4a974c33463e7232d3507c1dcc1`.

The fresh journal07 ASan build completed its 22 formal core objects, all selected libraries and executables, and the strict compatibility executable. Its first test failed before entering main: native exit `0xC0000135`, no stdout/stderr, verified natural quiescence, active_final=0. The executable imports `clang_rt.asan_dynamic-x86_64.dll`; the frozen build places that DLL at the build root, while the executable is under `candidate/prelive`. The original failed receipt and input closure remain retained and are not admitted as a test pass.

Journal08 reuses only journal07's verified successful build receipts and unchanged complete file inventory. It binds the root ASan DLL bytes and sets the direct native specification's process PATH to that frozen build root plus Windows system directories. It does not copy into or modify any frozen build, alter compiler instrumentation, suppress sanitizer checks, or change functional expectations. Every test receives a fresh native stage and input closure. Release and Debug proof is collected from the existing verified runs; the one historical original-session oracle failure remains explicit in each configuration.

The accumulated 32 GiB retained-evidence ceiling, 4 GiB journal candidate ceiling, per-stage metadata bound, process identity cap, and all numerical cost gates remain unchanged. Passing these tests is an endpoint-free cost prerequisite; it does not qualify or unlock live execution.
