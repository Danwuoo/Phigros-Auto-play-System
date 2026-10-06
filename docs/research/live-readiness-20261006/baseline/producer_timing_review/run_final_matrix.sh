#!/usr/bin/env bash
set -euo pipefail
cd /workspace/shared/phigros-live-readiness-20261006
root=/workspace/shared/phigros-producer-gate-final-20261006-01
mkdir "$root"
sha256sum research/prelive_current/{chain.cpp,producer_timing.hpp,cost_analysis.cpp,campaign_provenance.hpp,evidence_hash.hpp,streaming_workload.hpp,offline_output.hpp,run.ps1,current.cpp,runtime.hpp,tail_current.hpp,cost_workload_family_v2.hpp,CMakeLists.txt} research/prelive_audit/audit.cpp research/prelive_current/tests/{cost_analysis_tests.cpp,cost_analysis_review_tests.cpp,producer_admission_review_tests.cpp} > "$root/source-before.sha256"
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
 echo "Compile and run $mode real-auditor synthetic raw review"
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched -Dmain=raw_audit_entry -c research/prelive_audit/audit.cpp -o "$root/$mode/audit.o" > "$root/$mode/audit.compile.log" 2>&1
 g++ -std=c++20 "${flags[@]}" -Wall -Wextra -Wpedantic -I/workspace/shared/phigros-live-deps-20261006-patched research/prelive_current/tests/producer_admission_review_tests.cpp "$root/$mode/audit.o" -o "$root/$mode/producer-review" > "$root/$mode/producer-review.compile.log" 2>&1
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$root/$mode/producer-review" "$root/$mode/producer-results" > "$root/$mode/producer.stdout" 2> "$root/$mode/producer.stderr"
 sha256sum -c "$root/source-before.sha256" > "$root/$mode/source-after-check.txt"
 printf '0\n' > "$root/$mode/matrix-exit.txt"
 echo "$mode done"
done
