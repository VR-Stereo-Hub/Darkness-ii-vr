// core/ui/overlay.h - the F10 panel, the simple form (Ref VR-275).
//
// One ImGui window drawn INTO the stereo method's output texture through the
// seam's overlay draw hook, so it shows in the headset in both eyes. Hidden by
// default; nothing runs until F10 (a GetAsyncKeyState edge on the present
// thread) or the `overlay on|toggle` seam word shows it. The mouse comes from
// the desktop cursor scaled into the eye texture; the controller chord and the
// tiered Dishonored-style panel are VR-275's final form, not this.
//
// Only the DX11 backend of ImGui is linked (the Win32 backend pulls gdi32 and
// dwmapi imports the proxy must not add); the backend's D3DCompile is satisfied
// by core/gfx/d3dcompile_fwd.cpp. ImGui is called only from the overlay's own
// draw callback (TRAPS s5), on the present thread.
#pragma once
#include <stdint.h>

struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
namespace d2vr::status { class Writer; }

namespace d2vr::overlay {
// The game side's section (the Lua lane, the canaries): ImGui calls, invoked
// inside the panel between NewFrame and Render. Keeps core free of game headers.
typedef void (*GameSectionFn)();
void set_game_section(GameSectionFn fn);
void init();                       // registers the draw hook with the stereo seam (game::init)
void tick();                       // present thread: the F10 edge
bool command(const char* cmd, const char* args);   // overlay on|off|toggle|status
bool visible();
void set_visible(bool on, const char* why);
void shutdown();                   // present thread, from `quit`, BEFORE the D3D11 device goes
void status(d2vr::status::Writer& w);
// The seam's OverlayDrawFn: draws the panel into rtv (w x h) when visible.
void draw(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h);
} // namespace d2vr::overlay
