// core/framework/frame_hooks.cpp - see frame_hooks.h.
#define D2VR_CAT ::d2vr::log::Cat::present
#define INITGUID
#include <windows.h>
#include <d3d9.h>
#include <intrin.h>
#include "core/framework/frame_hooks.h"
#include "core/framework/command.h"
#include "core/framework/status.h"
#include "core/framework/capture.h"
#include "core/hooks/vtable.h"
#include "core/util/clock.h"
#include "core/util/crash.h"
#include "core/util/log.h"

namespace d2vr::frame {
namespace {

// IDirect3D9 / IDirect3D9Ex vtable slots (the D3D9 SDK layout, not a game number).
constexpr int kSlotCreateDevice   = 16;
constexpr int kSlotCreateDeviceEx = 20;
// IDirect3DDevice9 / IDirect3DDevice9Ex slots.
constexpr int kSlotReset     = 16;
constexpr int kSlotPresent   = 17;
constexpr int kSlotEndScene  = 42;
constexpr int kSlotPresentEx = 121;
constexpr int kSlotResetEx   = 132;

typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDeviceEx)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_Present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_PresentEx)(IDirect3DDevice9Ex*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *PFN_Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_ResetEx)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_EndScene)(IDirect3DDevice9*);

PFN_CreateDevice   g_origCreateDevice = nullptr;
PFN_CreateDeviceEx g_origCreateDeviceEx = nullptr;
PFN_Present   g_origPresent = nullptr;
PFN_PresentEx g_origPresentEx = nullptr;
PFN_Reset     g_origReset = nullptr;
PFN_ResetEx   g_origResetEx = nullptr;
PFN_EndScene  g_origEndScene = nullptr;

bool g_disabled = false;
TickFn g_tick = nullptr;
IDirect3DDevice9* g_device = nullptr;
DeviceInfo g_info;
volatile LONG g_presents = 0, g_presentEx = 0, g_endScenes = 0, g_resets = 0;
DWORD g_presentThread = 0;
uintptr_t g_presentCallers[3] = {};
uintptr_t g_endSceneCallers[3] = {};
int g_presentCallerCount = 0, g_endSceneCallerCount = 0;
double g_hzWindowStart = 0.0; unsigned long g_hzCount = 0; double g_hz = 0.0;

uintptr_t g_exeLo = 0, g_exeHi = 0;
void exe_range()
{
    if (g_exeLo) return;
    HMODULE exe = GetModuleHandleA(nullptr);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)((uint8_t*)exe + dos->e_lfanew);
    g_exeLo = (uintptr_t)exe;
    g_exeHi = g_exeLo + nt->OptionalHeader.SizeOfImage;
}
bool in_exe(uintptr_t a) { exe_range(); return a >= g_exeLo && a < g_exeHi; }

void remember_caller(uintptr_t* slots, int& count, uintptr_t ret, const char* what)
{
    if (count >= 3 || !in_exe(ret)) return;
    for (int i = 0; i < count; i++) if (slots[i] == ret) return;
    slots[count++] = ret;
    D2VR_LOG(D2VR_CAT, d2vr::log::Level::Info,
             "%s called from exe 0x%08lx (return address; the engine's frame-end code is just above it)",
             what, (unsigned long)ret);
}

const char* format_name(int f)
{
    switch (f) {
    case D3DFMT_X8R8G8B8: return "X8R8G8B8";
    case D3DFMT_A8R8G8B8: return "A8R8G8B8";
    case D3DFMT_R5G6B5: return "R5G6B5";
    case D3DFMT_A2R10G10B10: return "A2R10G10B10";
    case D3DFMT_D24S8: return "D24S8";
    case D3DFMT_D24X8: return "D24X8";
    case D3DFMT_D16: return "D16";
    case D3DFMT_UNKNOWN: return "UNKNOWN";
    default: return "other";
    }
}

void log_params(const char* which, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                const D3DPRESENT_PARAMETERS* pp, const D3DDISPLAYMODEEX* mode)
{
    if (!pp) { D2VR_WARN("%s: null D3DPRESENT_PARAMETERS", which); return; }
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info,
             "%s: adapter=%u type=%d focus=%p behavior=0x%08lx", which, adapter, (int)type, (void*)focus, (unsigned long)flags);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info,
             "%s: backbuffer %ux%u %s(%d) x%u  multisample=%d(q%lu) swap=%d window=%p windowed=%d "
             "autoDepth=%d(%s) flags=0x%lx refresh=%u interval=0x%lx",
             which, pp->BackBufferWidth, pp->BackBufferHeight, format_name(pp->BackBufferFormat), (int)pp->BackBufferFormat,
             pp->BackBufferCount, (int)pp->MultiSampleType, (unsigned long)pp->MultiSampleQuality, (int)pp->SwapEffect,
             (void*)pp->hDeviceWindow, (int)pp->Windowed, (int)pp->EnableAutoDepthStencil,
             format_name(pp->AutoDepthStencilFormat), (unsigned long)pp->Flags, pp->FullScreen_RefreshRateInHz,
             (unsigned long)pp->PresentationInterval);
    if (mode)
        D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info,
                 "%s: fullscreen mode %ux%u @%uHz fmt %s scanline %d", which, mode->Width, mode->Height,
                 mode->RefreshRate, format_name(mode->Format), (int)mode->ScanLineOrdering);
    g_info.adapter = adapter; g_info.width = pp->BackBufferWidth; g_info.height = pp->BackBufferHeight;
    g_info.format = (int)pp->BackBufferFormat; g_info.backBuffers = (int)pp->BackBufferCount;
    g_info.multisample = (int)pp->MultiSampleType; g_info.swapEffect = (int)pp->SwapEffect;
    g_info.interval = (int)pp->PresentationInterval; g_info.windowed = pp->Windowed != 0;
    g_info.window = pp->hDeviceWindow ? pp->hDeviceWindow : focus; g_info.behavior = (unsigned)flags;
}

void on_present(IDirect3DDevice9* dev, bool ex, uintptr_t ret)
{
    InterlockedIncrement(&g_presents);
    if (ex) InterlockedIncrement(&g_presentEx);
    const DWORD tid = GetCurrentThreadId();
    if (!g_presentThread) {
        g_presentThread = tid;
        d2vr::crash::register_thread("present", tid);
        D2VR_INFO("first %s on thread %lu (the present thread); device %p", ex ? "PresentEx" : "Present",
                  (unsigned long)tid, (void*)dev);
    } else if (tid != g_presentThread) {
        D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Warn, 5000,
            "present from a DIFFERENT thread (%lu, first was %lu) - the present lane is not one thread",
            (unsigned long)tid, (unsigned long)g_presentThread);
    }
    remember_caller(g_presentCallers, g_presentCallerCount, ret, ex ? "PresentEx" : "Present");
    if (dev != g_device) g_device = dev;

    const double now = d2vr::clock::now_ms();
    g_hzCount++;
    if (g_hzWindowStart == 0.0) g_hzWindowStart = now;
    else if (now - g_hzWindowStart >= 1000.0) {
        g_hz = g_hzCount * 1000.0 / (now - g_hzWindowStart);
        g_hzWindowStart = now; g_hzCount = 0;
        D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Debug, 10000, "present: %.1f Hz, %ld total", g_hz, (long)g_presents);
    }
    if (g_disabled) return;
    d2vr::command::poll(now);
    d2vr::status::tick(now);
    if ((g_presents & 255) == 0) d2vr::crash::rearm();
    d2vr::capture::tick(dev);
    if (g_tick) g_tick(dev, now);
}

HRESULT STDMETHODCALLTYPE hkPresent(IDirect3DDevice9* self, const RECT* a, const RECT* b, HWND c, const RGNDATA* d)
{
    on_present(self, false, (uintptr_t)_ReturnAddress());
    return g_origPresent(self, a, b, c, d);
}

HRESULT STDMETHODCALLTYPE hkPresentEx(IDirect3DDevice9Ex* self, const RECT* a, const RECT* b, HWND c, const RGNDATA* d, DWORD flags)
{
    on_present((IDirect3DDevice9*)self, true, (uintptr_t)_ReturnAddress());
    return g_origPresentEx(self, a, b, c, d, flags);
}

HRESULT STDMETHODCALLTYPE hkEndScene(IDirect3DDevice9* self)
{
    InterlockedIncrement(&g_endScenes);
    remember_caller(g_endSceneCallers, g_endSceneCallerCount, (uintptr_t)_ReturnAddress(), "EndScene");
    return g_origEndScene(self);
}

HRESULT STDMETHODCALLTYPE hkReset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* pp)
{
    InterlockedIncrement(&g_resets);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "Reset #%ld", (long)g_resets);
    log_params("Reset", g_info.adapter, D3DDEVTYPE_HAL, g_info.window, g_info.behavior, pp, nullptr);
    d2vr::capture::on_reset();
    HRESULT hr = g_origReset(self, pp);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "Reset -> 0x%08lx", (unsigned long)hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE hkResetEx(IDirect3DDevice9Ex* self, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* mode)
{
    InterlockedIncrement(&g_resets);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "ResetEx #%ld", (long)g_resets);
    log_params("ResetEx", g_info.adapter, D3DDEVTYPE_HAL, g_info.window, g_info.behavior, pp, mode);
    d2vr::capture::on_reset();
    HRESULT hr = g_origResetEx(self, pp, mode);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "ResetEx -> 0x%08lx", (unsigned long)hr);
    return hr;
}

void hook_device(IDirect3DDevice9* dev, bool ex)
{
    if (!dev) return;
    void* o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotPresent, (void*)hkPresent))) g_origPresent = (PFN_Present)o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotReset, (void*)hkReset))) g_origReset = (PFN_Reset)o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotEndScene, (void*)hkEndScene))) g_origEndScene = (PFN_EndScene)o;
    // Is this device a 9Ex device? Ask it, rather than trusting the create path.
    IDirect3DDevice9Ex* devEx = nullptr;
    if (SUCCEEDED(dev->QueryInterface(IID_IDirect3DDevice9Ex, (void**)&devEx)) && devEx) {
        if ((o = d2vr::hooks::patch_vtable(devEx, kSlotPresentEx, (void*)hkPresentEx))) g_origPresentEx = (PFN_PresentEx)o;
        if ((o = d2vr::hooks::patch_vtable(devEx, kSlotResetEx, (void*)hkResetEx))) g_origResetEx = (PFN_ResetEx)o;
        devEx->Release();
        ex = true;
    }
    g_device = dev; g_info.created = true; g_info.ex = ex;
    D2VR_INFO("device hooks installed on %p (Present/Reset/EndScene%s); device is %s",
              (void*)dev, ex ? " + PresentEx/ResetEx" : "", ex ? "IDirect3DDevice9Ex" : "IDirect3DDevice9");
}

HRESULT STDMETHODCALLTYPE hkCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                                         D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
{
    log_params("CreateDevice", adapter, type, focus, flags, pp, nullptr);
    HRESULT hr = g_origCreateDevice(self, adapter, type, focus, flags, pp, out);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "CreateDevice -> 0x%08lx device=%p", (unsigned long)hr, out ? (void*)*out : nullptr);
    if (SUCCEEDED(hr) && out && *out) hook_device(*out, false);
    return hr;
}

HRESULT STDMETHODCALLTYPE hkCreateDeviceEx(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                                           D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* mode, IDirect3DDevice9Ex** out)
{
    log_params("CreateDeviceEx", adapter, type, focus, flags, pp, mode);
    HRESULT hr = g_origCreateDeviceEx(self, adapter, type, focus, flags, pp, mode, out);
    D2VR_LOG(d2vr::log::Cat::d3d, d2vr::log::Level::Info, "CreateDeviceEx -> 0x%08lx device=%p", (unsigned long)hr, out ? (void*)*out : nullptr);
    if (SUCCEEDED(hr) && out && *out) hook_device((IDirect3DDevice9*)*out, true);
    return hr;
}

} // namespace

void hook_d3d9(void* d3d, bool isEx)
{
    if (!d3d) return;
    void* o = d2vr::hooks::patch_vtable(d3d, kSlotCreateDevice, (void*)hkCreateDevice);
    if (o) g_origCreateDevice = (PFN_CreateDevice)o;
    if (isEx) {
        o = d2vr::hooks::patch_vtable(d3d, kSlotCreateDeviceEx, (void*)hkCreateDeviceEx);
        if (o) g_origCreateDeviceEx = (PFN_CreateDeviceEx)o;
    }
    D2VR_INFO("interface hooks installed on %p (CreateDevice%s)", d3d, isEx ? " + CreateDeviceEx" : "");
}

void set_disabled(bool d) { g_disabled = d; }
bool disabled() { return g_disabled; }
void set_tick(TickFn fn) { g_tick = fn; }
IDirect3DDevice9* device() { return g_device; }
const DeviceInfo& info() { return g_info; }
unsigned long presents() { return (unsigned long)g_presents; }
unsigned long present_ex_calls() { return (unsigned long)g_presentEx; }
unsigned long end_scenes() { return (unsigned long)g_endScenes; }
unsigned long resets() { return (unsigned long)g_resets; }
DWORD present_thread() { return g_presentThread; }
uintptr_t present_caller(int i) { return (i >= 0 && i < 3) ? g_presentCallers[i] : 0; }
uintptr_t endscene_caller(int i) { return (i >= 0 && i < 3) ? g_endSceneCallers[i] : 0; }
double present_hz() { return g_hz; }

} // namespace d2vr::frame
