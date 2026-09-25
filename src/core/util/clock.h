// core/util/clock.h - one monotonic millisecond clock (QueryPerformanceCounter).
#pragma once

namespace d2vr::clock {
void      init();          // reads the QPC frequency; idempotent, loader-lock safe
double    now_ms();        // 0.0 before init()
long long qpc_freq();
} // namespace d2vr::clock
