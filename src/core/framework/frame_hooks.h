// core/framework/frame_hooks.h - the D3D9 hooks and the present tick.
//
// The proxy hands the game a real IDirect3D9(Ex) whose CreateDevice(Ex) vtable
// slot is ours; the device that comes back gets Present/PresentEx/Reset/ResetEx/
// EndScene hooks. Present is the mod's heartbeat: the command seam, status.json,
// the crash re-arm, the capture and the game layer's tick all run from it, on
// the thread the game presents from (the "present thread" lane).
//
// Nothing here is an exe code hook. It also measures, for S0.5 and R1:
//   - the CreateDevice(Ex) parameters (format, size, interval, windowed)
//   - whether the game calls Present or PresentEx, and from which thread
//   - the first exe return addresses of Present and EndScene (the engine's own
//     frame-end function: the hot canary site is derived from them)
#pragma once
#include <windows.h>
#include <stdint.h>

struct IDirect3DDevice9;

namespace d2vr::frame {

struct DeviceInfo {
    bool     created = false;
    bool     ex = false;              // created through CreateDeviceEx
    unsigned adapter = 0;
    unsigned width = 0, height = 0;
    int      format = 0;              // D3DFORMAT
    int      backBuffers = 0;
    int      multisample = 0;
    int      swapEffect = 0;
    int      interval = 0;            // PresentationInterval
    bool     windowed = false;
    HWND     window = nullptr;        // hDeviceWindow (or hFocusWindow)
    unsigned behavior = 0;
};

typedef void (*TickFn)(IDirect3DDevice9* dev, double nowMs);

// Patch CreateDevice (and CreateDeviceEx when isEx) on the interface the game got.
void hook_d3d9(void* d3d9Interface, bool isEx);
void set_disabled(bool disabled);     // kill switch: hooks stay, the tick does not run
bool disabled();
void set_tick(TickFn fn);             // the game layer's per-present work

IDirect3DDevice9* device();
const DeviceInfo& info();
unsigned long presents();             // Present + PresentEx calls
unsigned long present_ex_calls();
unsigned long end_scenes();
unsigned long resets();
DWORD         present_thread();       // 0 until the first present
uintptr_t     present_caller(int i);  // first 3 return addresses into the exe (0 = none)
uintptr_t     endscene_caller(int i);
double        present_hz();           // measured over the last second
unsigned long submits();              // presents that handed the runtime a texture
bool          vr_poisoned();          // a fault in the VR work: the game runs flat for the session

} // namespace d2vr::frame
