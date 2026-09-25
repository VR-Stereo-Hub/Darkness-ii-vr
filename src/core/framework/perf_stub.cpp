// core/framework/perf_stub.cpp - the two perf entry points the adopted runtime
// layer and capture call. Dishonored's perf.cpp is its tick-budget profiler and
// drags in its frame path; this mod has no profiler yet, so the calls are no-ops.
// perf.h is the Dishonored header, verbatim, so the adopted callers compile unchanged.
#include <windows.h>
#include <stdint.h>
#include "core/framework/perf.h"

namespace d2vr::perf {
void desktop_ab_submit(bool, uint32_t, uint32_t) {}
void gpu_mark(GpuPoint) {}
} // namespace d2vr::perf
