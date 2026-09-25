#include "core/util/clock.h"
#include <windows.h>

namespace d2vr::clock {
namespace { long long g_freq = 0; }

void init()
{
    if (g_freq) return;
    LARGE_INTEGER f;
    if (QueryPerformanceFrequency(&f)) g_freq = f.QuadPart;
}

long long qpc_freq() { return g_freq; }

double now_ms()
{
    if (!g_freq) return 0.0;
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    return (double)q.QuadPart * 1000.0 / (double)g_freq;
}
} // namespace d2vr::clock
