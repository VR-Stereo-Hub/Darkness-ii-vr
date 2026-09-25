// core/gfx/frame_id_stub.cpp - the frame-identity trace's staging points the adopted
// runtime layer and capture call. Dishonored's frame_id.cpp stamps a per-frame id
// into the pixels to prove which draw reached which eye; nothing here needs it yet,
// so the calls are no-ops. frame_id.h is the Dishonored header, verbatim.
#include <windows.h>
#include <stdint.h>
#include <d3d9.h>
#include <d3d11.h>
#include "core/gfx/frame_id.h"

namespace d2vr::frameid {
void stage_backbuffer(IDirect3DDevice9*, IDirect3DSurface9*, uint32_t, int) {}
void stage_swapchain(ID3D11Device*, ID3D11DeviceContext*, ID3D11Texture2D*, int, uint32_t) {}
void on_reset() {}
void shutdown() {}
} // namespace d2vr::frameid
