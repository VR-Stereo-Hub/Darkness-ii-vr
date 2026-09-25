// proxy/d3d9_exports.cpp - the 23 exports of d3d9.dll, forwarded to the system
// DLL, and the deferred initialisation the two create calls run first.
//
// The game (docs/darkness2/ENGINE_NOTES.md, R0) does LoadLibraryA("D3D9.DLL"),
// GetProcAddress("Direct3DCreate9Ex") and, with Graphics.EnableDirect3D9Ex at
// its default of 1, Direct3DCreate9Ex(0x20, &out); Direct3DCreate9(0x20) is the
// fallback. The two creates are typed wrappers. Every other export is a naked
// thunk that resolves the backend and tail-jumps, so unknown signatures pass
// through untouched.
#define D2VR_CAT ::d2vr::log::Cat::proxy
#include <windows.h>
#include <d3d9.h>
#include <intrin.h>
#include "proxy/proxy.h"
#include "core/config/config.h"
#include "core/framework/frame_hooks.h"
#include "core/util/crash.h"
#include "core/util/log.h"
#include "core/util/paths.h"
#include "game/darkness2/game.h"

namespace {

HMODULE g_real = nullptr;
typedef IDirect3D9* (WINAPI *PFN_Direct3DCreate9)(UINT);
typedef HRESULT (WINAPI *PFN_Direct3DCreate9Ex)(UINT, IDirect3D9Ex**);
PFN_Direct3DCreate9   g_realCreate9 = nullptr;
PFN_Direct3DCreate9Ex g_realCreate9Ex = nullptr;
unsigned g_createCalls = 0;

} // namespace

// The forwarding targets, one per export. Named for the thunks below; global
// (not namespaced) so MSVC inline asm can name them.
void* d2vr_real_ord16 = nullptr;
void* d2vr_real_ord17 = nullptr;
void* d2vr_real_ord18 = nullptr;
void* d2vr_real_ord19 = nullptr;
void* d2vr_real_Direct3DCreate9On12 = nullptr;
void* d2vr_real_Direct3DCreate9On12Ex = nullptr;
void* d2vr_real_ord22 = nullptr;
void* d2vr_real_ord23 = nullptr;
void* d2vr_real_Direct3DShaderValidatorCreate9 = nullptr;
void* d2vr_real_PSGPError = nullptr;
void* d2vr_real_PSGPSampleTexture = nullptr;
void* d2vr_real_D3DPERF_BeginEvent = nullptr;
void* d2vr_real_D3DPERF_EndEvent = nullptr;
void* d2vr_real_D3DPERF_GetStatus = nullptr;
void* d2vr_real_D3DPERF_QueryRepeatFrame = nullptr;
void* d2vr_real_D3DPERF_SetMarker = nullptr;
void* d2vr_real_D3DPERF_SetOptions = nullptr;
void* d2vr_real_D3DPERF_SetRegion = nullptr;
void* d2vr_real_DebugSetLevel = nullptr;
void* d2vr_real_DebugSetMute = nullptr;
void* d2vr_real_Direct3D9EnableMaximizedWindowedModeShim = nullptr;

extern "C" bool __cdecl EnsureRealD3D9()
{
    if (g_real) return true;
    char path[MAX_PATH];
    // Under WOW64 GetSystemDirectory answers SysWOW64: the 32-bit d3d9.dll.
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\d3d9.dll");
    g_real = LoadLibraryA(path);
    if (!g_real) {
        D2VR_ERROR("FATAL: could not load the system d3d9 backend %s (err %lu)", path, GetLastError());
        return false;
    }
    int missing = 0;
#define RESOLVE_NAME(n)  do { d2vr_real_##n = (void*)GetProcAddress(g_real, #n); if (!d2vr_real_##n) { missing++; D2VR_WARN("backend lacks export %s", #n); } } while (0)
#define RESOLVE_ORD(o)   do { d2vr_real_ord##o = (void*)GetProcAddress(g_real, MAKEINTRESOURCEA(o)); if (!d2vr_real_ord##o) { missing++; D2VR_WARN("backend lacks ordinal %d", o); } } while (0)
    RESOLVE_ORD(16); RESOLVE_ORD(17); RESOLVE_ORD(18); RESOLVE_ORD(19); RESOLVE_ORD(22); RESOLVE_ORD(23);
    RESOLVE_NAME(Direct3DCreate9On12); RESOLVE_NAME(Direct3DCreate9On12Ex);
    RESOLVE_NAME(Direct3DShaderValidatorCreate9); RESOLVE_NAME(PSGPError); RESOLVE_NAME(PSGPSampleTexture);
    RESOLVE_NAME(D3DPERF_BeginEvent); RESOLVE_NAME(D3DPERF_EndEvent); RESOLVE_NAME(D3DPERF_GetStatus);
    RESOLVE_NAME(D3DPERF_QueryRepeatFrame); RESOLVE_NAME(D3DPERF_SetMarker); RESOLVE_NAME(D3DPERF_SetOptions);
    RESOLVE_NAME(D3DPERF_SetRegion); RESOLVE_NAME(DebugSetLevel); RESOLVE_NAME(DebugSetMute);
    RESOLVE_NAME(Direct3D9EnableMaximizedWindowedModeShim);
#undef RESOLVE_NAME
#undef RESOLVE_ORD
    g_realCreate9   = (PFN_Direct3DCreate9)GetProcAddress(g_real, "Direct3DCreate9");
    g_realCreate9Ex = (PFN_Direct3DCreate9Ex)GetProcAddress(g_real, "Direct3DCreate9Ex");
    if (!g_realCreate9) missing++;
    if (!g_realCreate9Ex) missing++;
    D2VR_INFO("d3d9 backend ready: %s at 0x%08lx (Create9=%p Create9Ex=%p, %d export(s) missing)",
              path, (unsigned long)(uintptr_t)g_real, (void*)g_realCreate9, (void*)g_realCreate9Ex, missing);
    return true;
}

// Naked forwarding thunk: resolve the backend (preserving the argument registers
// a __fastcall or thiscall caller might use), then tail-jump so the callee sees
// the caller's stack and return address untouched.
#define D2VR_THUNK_TO(fn, realvar)     extern "C" __declspec(naked) void fn() {         __asm { push ecx }         __asm { push edx }         __asm { call EnsureRealD3D9 }         __asm { pop edx }         __asm { pop ecx }         __asm { jmp dword ptr [realvar] }     }
// Named exports get an internal symbol (d3d9.h declares several of these names
// itself); the .def maps <name>=d2vr_exp_<name>.
#define D2VR_THUNK(name) D2VR_THUNK_TO(d2vr_exp_##name, d2vr_real_##name)

// The six ordinal-only exports: the .def names these symbols d2vr_ord<n> @<n> NONAME.
D2VR_THUNK_TO(d2vr_ord16, d2vr_real_ord16)
D2VR_THUNK_TO(d2vr_ord17, d2vr_real_ord17)
D2VR_THUNK_TO(d2vr_ord18, d2vr_real_ord18)
D2VR_THUNK_TO(d2vr_ord19, d2vr_real_ord19)
D2VR_THUNK_TO(d2vr_ord22, d2vr_real_ord22)
D2VR_THUNK_TO(d2vr_ord23, d2vr_real_ord23)
D2VR_THUNK(Direct3DCreate9On12)
D2VR_THUNK(Direct3DCreate9On12Ex)
D2VR_THUNK(Direct3DShaderValidatorCreate9)
D2VR_THUNK(PSGPError)
D2VR_THUNK(PSGPSampleTexture)
D2VR_THUNK(D3DPERF_BeginEvent)
D2VR_THUNK(D3DPERF_EndEvent)
D2VR_THUNK(D3DPERF_GetStatus)
D2VR_THUNK(D3DPERF_QueryRepeatFrame)
D2VR_THUNK(D3DPERF_SetMarker)
D2VR_THUNK(D3DPERF_SetOptions)
D2VR_THUNK(D3DPERF_SetRegion)
D2VR_THUNK(DebugSetLevel)
D2VR_THUNK(DebugSetMute)
D2VR_THUNK(Direct3D9EnableMaximizedWindowedModeShim)
#undef D2VR_THUNK
#undef D2VR_THUNK_TO

namespace d2vr::proxy {

void deferred_init()
{
    static bool done = false;
    if (done) return;
    done = true;
    log_module_census("create");
    if (g_disabled) {
        D2VR_WARN("deferred init SKIPPED: disable_vr.txt is present (log + route report only)");
        d2vr::frame::set_disabled(true);
        return;
    }
    d2vr::config::load();          // safe here (post loader-lock); never in DllMain
    d2vr::crash::install();        // fingerprint VEH + minidump filter, before any hook
    d2vr::game::init();            // the seam handler, status provider, the present tick, the canaries
}

} // namespace d2vr::proxy

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVersion)
{
    const uintptr_t caller = (uintptr_t)_ReturnAddress();
    ++g_createCalls;
    D2VR_INFO("Direct3DCreate9(SDK=%u) entered from our module; caller 0x%08lx (call #%u)", sdkVersion, (unsigned long)caller, g_createCalls);
    d2vr::proxy::deferred_init();
    if (!EnsureRealD3D9() || !g_realCreate9) return nullptr;
    IDirect3D9* d3d = g_realCreate9(sdkVersion);
    D2VR_INFO("Direct3DCreate9 -> %p", (void*)d3d);
    if (d3d && !d2vr::proxy::g_disabled) d2vr::frame::hook_d3d9(d3d, false);
    return d3d;
}

extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out)
{
    const uintptr_t caller = (uintptr_t)_ReturnAddress();
    ++g_createCalls;
    D2VR_INFO("Direct3DCreate9Ex(SDK=%u) entered from our module; caller 0x%08lx (call #%u)", sdkVersion, (unsigned long)caller, g_createCalls);
    d2vr::proxy::deferred_init();
    if (!EnsureRealD3D9() || !g_realCreate9Ex) return E_FAIL;
    HRESULT hr = g_realCreate9Ex(sdkVersion, out);
    D2VR_INFO("Direct3DCreate9Ex -> 0x%08lx, object %p", (unsigned long)hr, out ? (void*)*out : nullptr);
    if (SUCCEEDED(hr) && out && *out && !d2vr::proxy::g_disabled) d2vr::frame::hook_d3d9(*out, true);
    return hr;
}
