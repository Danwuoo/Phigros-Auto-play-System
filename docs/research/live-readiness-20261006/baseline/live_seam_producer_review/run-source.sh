#!/usr/bin/env bash
set -euo pipefail
cd /workspace/shared/phigros-live-readiness-20261006
[[ ${PAS_LIVE_SEAM_SOURCE_FROZEN:-0} == 1 ]] || { echo "source freeze required" >&2; exit 64; }
root=${PAS_COST_MATRIX_ROOT:-/workspace/shared/phigros-producer-seam-final-20261006-01}
[[ $root == /workspace/shared/phigros-producer-seam-final-* ]] || exit 64
python - <<'CAP'
import os
s=os.statvfs(".");free=s.f_bavail*s.f_frsize
assert free >= 20*1024**3+224*1024**2, ("capacity reserve",free)
CAP
mkdir "$root"
sha256sum CMakeLists.txt include/pas/{candidate_admission.hpp,candidate_packet.hpp,runtime.hpp,game_session.hpp} src/{manual_session.cpp,game_session.cpp} research/prelive_current/{input_closure.hpp,owner_candidate_step.hpp,receipt_stream.hpp,own_bridge.cpp,own_bridge.hpp,chain.cpp,producer_timing.hpp,cost_analysis.cpp,campaign_provenance.hpp,evidence_hash.hpp,streaming_workload.hpp,offline_output.hpp,run.ps1,current.cpp,runtime.hpp,tail_current.hpp,cost_workload_family_v2.hpp,CMakeLists.txt} research/prelive_audit/audit.cpp research/prelive_current/tests/{cost_analysis_tests.cpp,cost_analysis_review_tests.cpp,producer_admission_review_tests.cpp,input_closure_review_tests.cpp} > "$root/source-before.sha256"
sha256sum "${BASH_SOURCE[0]}" >> "$root/source-before.sha256"
cp "${BASH_SOURCE[0]}" "$root/run-source.sh"
g++ --version > "$root/compiler.txt"
for mode in release debug sanitizers; do
 mkdir "$root/$mode"
 case "$mode" in release) flags=(-O2 -DNDEBUG);; debug) flags=(-O0 -g1 -D_GLIBCXX_ASSERTIONS);; sanitizers) flags=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer);; esac
 printf '%s\n' "${flags[*]}" > "$root/$mode/flags.txt"
 echo "Compile and run $mode seeded admission"
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched research/prelive_current/tests/cost_analysis_tests.cpp -o "$root/$mode/seed" > "$root/$mode/seed.compile.log" 2>&1
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$root/$mode/seed" "$root/$mode/fixtures" /workspace/shared/phigros-live-readiness-20261006 > "$root/$mode/seed.stdout" 2> "$root/$mode/seed.stderr"
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched research/prelive_current/tests/cost_analysis_review_tests.cpp -o "$root/$mode/admission-review" > "$root/$mode/admission-review.compile.log" 2>&1
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$root/$mode/admission-review" "$root/$mode/fixtures" "$root/$mode/admission-results" > "$root/$mode/admission.stdout" 2> "$root/$mode/admission.stderr"
 echo "Compile and run $mode bounded input-closure independent review"
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched research/prelive_current/tests/input_closure_review_tests.cpp -o "$root/$mode/input-closure-review" > "$root/$mode/input-closure-review.compile.log" 2>&1
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$root/$mode/input-closure-review" "$root/$mode/fixtures" "$root/$mode/input-closure-results" > "$root/$mode/input-closure.stdout" 2> "$root/$mode/input-closure.stderr"
 echo "Compile and run $mode real-auditor synthetic raw review"
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched -Dmain=raw_audit_entry -c research/prelive_audit/audit.cpp -o "$root/$mode/audit.o" > "$root/$mode/audit.compile.log" 2>&1
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched research/prelive_current/tests/producer_admission_review_tests.cpp "$root/$mode/audit.o" -o "$root/$mode/producer-review" > "$root/$mode/producer-review.compile.log" 2>&1
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$root/$mode/producer-review" "$root/$mode/producer-results" > "$root/$mode/producer.stdout" 2> "$root/$mode/producer.stderr"
 sha256sum -c "$root/source-before.sha256" > "$root/$mode/source-after-check.txt"
 printf '0\n' > "$root/$mode/matrix-exit.txt"
 echo "$mode done"
done
