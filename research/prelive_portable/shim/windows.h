#pragma once
// Research-only Linux compilation shim. Production source is untouched.
// Business-time controls use pas::FakeClock. Runtime diagnostic timings may
// use this Linux steady_clock adapter; they are never native-QPC cost evidence.
#include <chrono>
struct LARGE_INTEGER { long long QuadPart; };
inline int QueryPerformanceFrequency(LARGE_INTEGER* p) { p->QuadPart=1'000'000'000; return 1; }
inline int QueryPerformanceCounter(LARGE_INTEGER* p) {
 p->QuadPart=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); return 1;
}
