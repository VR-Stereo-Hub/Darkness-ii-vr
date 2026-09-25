// core/gfx/d3d9ex_stub.cpp - see d3d9ex_host.h.
#include <windows.h>
#include <d3d9.h>
#include "core/gfx/d3d9ex.h"
#include "core/gfx/d3d9ex_host.h"

namespace d2vr::d3d9ex {
static LUID g_luid = { 0, 0 };
static bool g_luidOk = false;

void set_adapter_luid(const LUID& luid) { g_luid = luid; g_luidOk = true; }

bool adapter_luid(LUID* out)
{
    if (!g_luidOk) return false;
    if (out) *out = g_luid;
    return true;
}
} // namespace d2vr::d3d9ex
