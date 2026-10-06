# Final expired-first source cost/admission replay

All three Linux modes pass 36 seed +152 admission (123 source/29 semantic) +35 input-closure (31 negative controls) +14 actual-auditor synthetic cases. Actual Windows cost and live readiness remain false. Thresholds and the original64KiB native freeze cap are unchanged.

The preceding full compiler matrix built all control binaries from the same C++ sources, then verified them in all three modes. Later changes were two unexecuted Windows recipe files and the runtime-only OwnerCandidateStep header; the control programs read those as source-identity data but do not compile that header. Actual GCC-MM expansion for all five translation units in all three modes proves their non-system dependencies still match the original compile seals. The pinned JSON header also matches. Read-only system headers are not falsely presented as historically byte-sealed.

Only CRC/SHA-verified locally built binaries were restored from their retained archives. All fixtures/metadata/raw controls were recreated against final source bytes in a new root, and every case ran again. The runtime lane separately rebuilds every consumer of the expired-first header and reruns the complete fixed corpus and3063 original frames. This replay never stands in for that behavioral validation.

Source manifests, exact binary reuse/restoration identities, dependency proof, per-mode counts, raw payload ZIP, payload manifest and SHA index are retained here. Cost03 is an earlier successful recipe-data checkpoint retained at /workspace/shared/phigros-producer-seam-final-20261006-03; no original source/failure/STOP was overwritten. Attempt01's obsolete test-helper-boundary failure and all31 unchanged expected controls remain documented in ../../review/input-closure/.

No Windows process, real capture/RPC, device, game, model or paid compute was used. ASan/UBSan pass; leak detection is explicitly unavailable under ptrace.
