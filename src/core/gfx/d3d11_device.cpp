// core/gfx/d3d11_device.cpp - see d3d11_device.h. The adapter selection and the
// device creation are verbatim from the Dishonored VR mod (its unity fragment
// core/gfx/d3d11_device.cpp + state chunk 04), with the unity `Log(...)` macro
// spelled as D2VR_INFO and the hand-pass common states left out (no hands yet).
#define D2VR_CAT ::d2vr::log::Cat::d3d
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <string.h>
#include "core/gfx/d3d11_device.h"
#include "core/util/log.h"

typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE,
    HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

static HMODULE              g_d3d11mod    = NULL;
static ID3D11Device*        g_dev11       = NULL;
static ID3D11DeviceContext* g_ctx11       = NULL;
static bool                 g_d3d11Failed = false;

// The adapter the OpenXR runtime REQUIRES. xrGetD3D11GraphicsRequirementsKHR
// hands back a LUID; creating our D3D11 device on any other adapter means the
// runtime cannot read the eye textures, every call still returns S_OK, and
// nothing logs an error. The runtime layer publishes the LUID here BEFORE it
// asks for a device; EnsureD3D11 then selects that adapter instead of passing
// NULL (= "whatever DXGI enumerates first"). Fail soft: an unmatched LUID logs
// loudly and falls back to the default adapter, because a flat game beats no game.
static LUID  g_wantAdapterLuid   = { 0, 0 };
static bool  g_wantAdapterLuidOk = false;
// What we actually landed on, read back FROM THE DEVICE (not from what we
// asked for) so the instrument can fail its own hypothesis.
static LUID  g_gotAdapterLuid    = { 0, 0 };
static bool  g_gotAdapterLuidOk  = false;
static char  g_gotAdapterName[128] = "";


// ----------------------------------------------------------------------------
// D3D11 renderer
// ----------------------------------------------------------------------------
// 40.1: LUID helpers. A LUID is two halves and is only ever compared, never
// ordered, so a plain equality is the whole contract.
static bool LuidEq(const LUID& a, const LUID& b)
{
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

static void LuidStr(const LUID& l, char* out, size_t n)
{
    _snprintf(out, n, "%08lX-%08lX", (unsigned long)l.HighPart,
              (unsigned long)l.LowPart);
    out[n - 1] = 0;
}

// Enumerates DXGI adapters and logs every one, because the single most common
// way this goes wrong is silently: the device lands on an adapter nobody named
// and every later call succeeds. If wantLuid is set, returns the matching
// adapter (caller releases it); otherwise returns NULL and the caller uses the
// default. dxgi.dll is loaded dynamically to match how d3d11.dll is treated
// here - the proxy links neither.
static IDXGIAdapter* PickAdapter(const LUID* wantLuid)
{
    typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(REFIID, void**);
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    if (!dxgi) { D2VR_WARN("adapter: no dxgi.dll - cannot enumerate adapters"); return NULL; }
    PFN_CreateDXGIFactory1 mk =
        (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
    if (!mk) { D2VR_WARN("adapter: dxgi.dll has no CreateDXGIFactory1"); return NULL; }

    IDXGIFactory1* fac = NULL;
    HRESULT hr = mk(__uuidof(IDXGIFactory1), (void**)&fac);
    if (FAILED(hr) || !fac) {
        D2VR_WARN("adapter: CreateDXGIFactory1 failed (0x%08lx)", (unsigned long)hr);
        return NULL;
    }

    char want[40] = "(none - runtime did not say)";
    if (wantLuid) LuidStr(*wantLuid, want, sizeof(want));
    D2VR_INFO("adapter: enumerating DXGI adapters; OpenXR wants LUID %s", want);

    IDXGIAdapter* chosen = NULL;
    IDXGIAdapter1* a = NULL;
    for (UINT i = 0; fac->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        memset(&d, 0, sizeof(d));
        if (SUCCEEDED(a->GetDesc1(&d))) {
            char name[128] = "";
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof(name) - 1, NULL, NULL);
            char luid[40];
            LuidStr(d.AdapterLuid, luid, sizeof(luid));
            bool match = (wantLuid && LuidEq(d.AdapterLuid, *wantLuid));
            D2VR_INFO("adapter[%u]: %-40s vendor=%04X device=%04X vram=%luMB luid=%s%s%s",
                i, name, (unsigned)d.VendorId, (unsigned)d.DeviceId,
                (unsigned long)(d.DedicatedVideoMemory / (1024 * 1024)), luid,
                (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? " [SOFTWARE]" : "",
                match ? "  <== the runtime asked for THIS one" : "");
            if (match && !chosen) { chosen = a; a->AddRef(); }
        }
        a->Release();
        a = NULL;
    }
    fac->Release();

    if (wantLuid && !chosen)
        D2VR_WARN("adapter: NO adapter matched the runtime's LUID %s - falling back to "
                 "the default adapter. If the headset shows a black, frozen or "
                 "wrongly-scaled image this is the first suspect.", want);
    return chosen;
}

// Reads the adapter back OUT of the finished device. This is the measurement
// that matters: what we asked for is a hypothesis, what the device is on is
// the fact.
static void LogDeviceAdapter(void)
{
    g_gotAdapterLuidOk = false;
    g_gotAdapterName[0] = 0;
    if (!g_dev11) return;
    IDXGIDevice* dxdev = NULL;
    if (FAILED(g_dev11->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxdev)) || !dxdev) {
        D2VR_WARN("adapter: device has no IDXGIDevice - cannot confirm which adapter it is on");
        return;
    }
    IDXGIAdapter* ad = NULL;
    if (SUCCEEDED(dxdev->GetAdapter(&ad)) && ad) {
        DXGI_ADAPTER_DESC d;
        memset(&d, 0, sizeof(d));
        if (SUCCEEDED(ad->GetDesc(&d))) {
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, g_gotAdapterName,
                                sizeof(g_gotAdapterName) - 1, NULL, NULL);
            g_gotAdapterLuid = d.AdapterLuid;
            g_gotAdapterLuidOk = true;
            char luid[40];
            LuidStr(d.AdapterLuid, luid, sizeof(luid));
            D2VR_INFO("adapter: D3D11 device is ON \"%s\" (vendor=%04X device=%04X luid=%s)",
                g_gotAdapterName, (unsigned)d.VendorId, (unsigned)d.DeviceId, luid);
            if (g_wantAdapterLuidOk) {
                if (LuidEq(d.AdapterLuid, g_wantAdapterLuid)) {
                    D2VR_INFO("adapter: MATCHES the adapter the OpenXR runtime asked for");
                } else {
                    char wl[40];
                    LuidStr(g_wantAdapterLuid, wl, sizeof(wl));
                    D2VR_ERROR("adapter: MISMATCH - the OpenXR runtime asked for LUID %s but "
                              "the D3D11 device is on %s. Shared eye textures cannot cross "
                              "adapters; expect a black, frozen or mis-scaled headset image "
                              "while every call still returns S_OK.", wl, luid);
                }
            }
        }
        ad->Release();
    }
    dxdev->Release();
}

static bool EnsureD3D11()
{
    if (g_dev11) return true;
    if (g_d3d11Failed) return false;
    if (!g_d3d11mod) g_d3d11mod = LoadLibraryA("d3d11.dll");
    if (!g_d3d11mod) { D2VR_INFO("no d3d11.dll"); g_d3d11Failed = true; return false; }
    PFN_D3D11CreateDevice create =
        (PFN_D3D11CreateDevice)GetProcAddress(g_d3d11mod, "D3D11CreateDevice");
    if (!create) { g_d3d11Failed = true; return false; }

    // 40.1: create on the adapter the OpenXR runtime named, when it named one.
    IDXGIAdapter* pick = PickAdapter(g_wantAdapterLuidOk ? &g_wantAdapterLuid : NULL);
    HRESULT hr = E_FAIL;
    if (pick) {
        // D3D11 requires DRIVER_TYPE_UNKNOWN whenever an adapter is supplied;
        // passing HARDWARE with a non-NULL adapter is E_INVALIDARG.
        hr = create(pick, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0,
                    D3D11_SDK_VERSION, &g_dev11, NULL, &g_ctx11);
        if (FAILED(hr) || !g_dev11)
            D2VR_WARN("adapter: D3D11CreateDevice on the requested adapter failed "
                     "(0x%08lx) - retrying on the default adapter", (unsigned long)hr);
        pick->Release();
    }
    if (!g_dev11) {
        hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                    D3D11_SDK_VERSION, &g_dev11, NULL, &g_ctx11);
    }
    if (FAILED(hr) || !g_dev11) {
        D2VR_ERROR("D3D11CreateDevice failed (0x%08lx)", (unsigned long)hr);
        g_d3d11Failed = true;
        return false;
    }
    D2VR_INFO("D3D11 device created");
    LogDeviceAdapter();
    return true;
}

namespace d2vr::d3d11 {

ID3D11Device* provide(const LUID* want)
{
    if (want) { g_wantAdapterLuid = *want; g_wantAdapterLuidOk = true; }
    if (!EnsureD3D11()) return NULL;
    return g_dev11;
}

ID3D11Device* device(ID3D11DeviceContext** ctx)
{
    if (!g_dev11) {
        // The stereo methods reach here from the present tail. Until the runtime
        // has named its adapter there is no session to show a frame to, so a
        // device created now would only ever be the WRONG one: refuse instead.
        if (!g_wantAdapterLuidOk) { if (ctx) *ctx = NULL; return NULL; }
        if (!EnsureD3D11()) { if (ctx) *ctx = NULL; return NULL; }
    }
    if (ctx) *ctx = g_ctx11;
    return g_dev11;
}

bool created() { return g_dev11 != NULL; }
const char* adapter_name() { return g_gotAdapterName; }

void shutdown()
{
    if (g_ctx11) { g_ctx11->Release(); g_ctx11 = NULL; }
    if (g_dev11) { g_dev11->Release(); g_dev11 = NULL; }
}

} // namespace d2vr::d3d11
