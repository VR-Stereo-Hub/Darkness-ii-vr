// proxy/proxy.h - what the two proxy translation units share.
//
// The mod module is a d3d9.dll next to DarknessII.exe (R0's route). DllMain
// does only what is loader-lock safe (paths, clock, log, the kill switch, the
// route self-report); the first Direct3DCreate9(Ex) call does the rest.
#pragma once
#include <windows.h>

namespace d2vr::proxy {
extern bool g_disabled;                 // disable_vr.txt next to the exe
extern HINSTANCE g_self;
void log_route_report();                // DllMain: where were we loaded from, what was loaded before us
void log_module_census(const char* when); // post-loader-lock: every module with its path (Toolhelp)
void deferred_init();                   // config, crash handler, seam, status, the game layer; idempotent
}

// Loads the system d3d9.dll and resolves every export. cdecl and extern "C" so
// the naked forwarding thunks can call it by name.
extern "C" bool __cdecl EnsureRealD3D9();
