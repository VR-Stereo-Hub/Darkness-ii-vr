// core/gfx/d3d9ex_host.h - the game's D3D9Ex adapter LUID, recorded by the
// CreateDeviceEx hook (frame_hooks.cpp) from the IDirect3D9Ex the game itself
// created. This engine already lives on a 9Ex device (ENGINE_NOTES s2), so none of
// Dishonored's d3d9ex.cpp (which hands the game a 9Ex object in place of a plain
// one) is adopted; d3d9ex.h stays verbatim for the adopted capture's one call,
// adapter_luid(), and this header adds the setter that feeds it.
#pragma once
#include <windows.h>

namespace d2vr::d3d9ex {
void set_adapter_luid(const LUID& luid);   // present thread, once, from hkCreateDeviceEx
}
