// proxy/dllmain.cpp - DllMain of the d3d9.dll proxy.
//
// This runs under the Windows loader lock, so it does as little as it can:
// paths, clock, log, the kill switch, the log-level environment, and the route
// self-report (R0's measurement: which d3d9.dll the loader chose and what was
// already loaded). Nothing that touches the ini, the engine or another DLL:
// profile/file calls that pull in other DLLs here can abort the whole process
// (0xc0000142). Everything else waits for Direct3DCreate9(Ex).
#define D2VR_CAT ::d2vr::log::Cat::proxy
#include "proxy/proxy.h"
#include "core/util/clock.h"
#include "core/util/log.h"
#include "core/util/paths.h"
#include "d2vr_version.h"

#include <winternl.h>
#include <intrin.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

namespace d2vr::proxy {

bool g_disabled = false;
HINSTANCE g_self = nullptr;

namespace {

// The loader's own list, read straight from the PEB. No API call that could
// take a lock we do not already hold: we ARE inside the loader lock here.
void log_ldr_list()
{
    // winternl.h exposes the PEB_LDR_DATA::InMemoryOrderModuleList and the
    // LDR_DATA_TABLE_ENTRY fields DllBase and FullDllName; that is all we need.
    PEB* peb = (PEB*)__readfsdword(0x30);
    if (!peb || !peb->Ldr) { D2VR_WARN("route: no PEB/Ldr"); return; }
    LIST_ENTRY* head = &peb->Ldr->InMemoryOrderModuleList;
    int n = 0;
    for (LIST_ENTRY* e = head->Flink; e && e != head && n < 96; e = e->Flink, n++) {
        LDR_DATA_TABLE_ENTRY* m = CONTAINING_RECORD(e, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        char path[MAX_PATH] = "?";
        if (m->FullDllName.Buffer && m->FullDllName.Length)
            WideCharToMultiByte(CP_ACP, 0, m->FullDllName.Buffer, m->FullDllName.Length / 2, path, MAX_PATH - 1, nullptr, nullptr);
        const bool ours = (HINSTANCE)m->DllBase == g_self;
        D2VR_INFO("route: ldr[%2d] base=0x%08lx %s%s", n, (unsigned long)(uintptr_t)m->DllBase, path,
                  ours ? "   <== THIS MODULE" : "");
    }
    D2VR_INFO("route: %d module(s) in the loader's in-memory-order list at our DllMain", n);
}

} // namespace

void log_route_report()
{
    char cwd[MAX_PATH] = "", dlldir[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, cwd);
    const DWORD dd = GetDllDirectoryA(MAX_PATH, dlldir);
    FILETIME create, exitT, kern, user; ULARGE_INTEGER now, c;
    GetProcessTimes(GetCurrentProcess(), &create, &exitT, &kern, &user);
    GetSystemTimeAsFileTime((FILETIME*)&now);
    c.LowPart = create.dwLowDateTime; c.HighPart = create.dwHighDateTime;
    const double sinceStartMs = (double)(now.QuadPart - c.QuadPart) / 10000.0;

    D2VR_INFO("route: our module   %s", d2vr::paths::module_path());
    D2VR_INFO("route: host exe     %s", d2vr::paths::exe_path());
    D2VR_INFO("route: game dir     %s", d2vr::paths::game_dir());
    D2VR_INFO("route: cwd          %s", cwd);
    D2VR_INFO("route: DllDirectory %s  (GetDllDirectory returned %lu: %s)", dd ? dlldir : "(not set)", (unsigned long)dd,
              dd ? "the exe called SetDllDirectory before our load" : "no SetDllDirectory yet, or it was reset");
    D2VR_INFO("route: DllMain on thread %lu, %.0f ms after process creation, pid %lu",
              (unsigned long)GetCurrentThreadId(), sinceStartMs, (unsigned long)GetCurrentProcessId());
    // The verdict's core line: does the module we are live in the directory of the exe?
    char exeDir[MAX_PATH]; strncpy(exeDir, d2vr::paths::exe_path(), MAX_PATH - 1); exeDir[MAX_PATH - 1] = 0;
    if (char* s = strrchr(exeDir, '\\')) *s = 0;
    const bool appDir = _stricmp(exeDir, d2vr::paths::game_dir()) == 0;
    D2VR_INFO("route: loaded from the exe's own directory: %s%s", appDir ? "YES" : "NO",
              appDir ? " (route (a), the app-dir d3d9.dll, fired)" : " - the app-dir proxy is NOT what the loader chose");
    log_ldr_list();
}

void log_module_census(const char* when)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) { D2VR_WARN("census: snapshot failed (err %lu)", GetLastError()); return; }
    MODULEENTRY32 me; me.dwSize = sizeof(me);
    int n = 0; bool overlay = false, steam = false, d3d9sys = false;
    for (BOOL ok = Module32First(snap, &me); ok; ok = Module32Next(snap, &me), n++) {
        if (!_stricmp(me.szModule, "GameOverlayRenderer.dll")) overlay = true;
        if (!_stricmp(me.szModule, "steam_api.dll") || !_stricmp(me.szModule, "steamclient.dll")) steam = true;
        if (!_stricmp(me.szModule, "d3d9.dll") && (HINSTANCE)me.modBaseAddr != g_self) d3d9sys = true;
        D2VR_DEBUG("census(%s): %-28s 0x%08lx %s", when, me.szModule, (unsigned long)(uintptr_t)me.modBaseAddr, me.szExePath);
    }
    CloseHandle(snap);
    D2VR_INFO("census(%s): %d modules; Steam overlay (GameOverlayRenderer.dll) %s; steam client/api %s; a second d3d9.dll (the system one, our backend) %s",
              when, n, overlay ? "LOADED" : "not loaded", steam ? "loaded" : "not loaded", d3d9sys ? "loaded" : "not loaded");
}

} // namespace d2vr::proxy

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    using namespace d2vr;
    if (reason == DLL_PROCESS_ATTACH) {
        proxy::g_self = hinst;
        DisableThreadLibraryCalls(hinst);
        clock::init();
        paths::init(hinst);

        char path[MAX_PATH];
        paths::in_game_dir(path, "disable_vr.txt");
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) proxy::g_disabled = true;

        // darkness2_vr.log next to the game exe (rotates the previous run to
        // .prev.log ... .prev9.log). Never gated on anything: a run ALWAYS logs.
        log::init(paths::game_dir(), "darkness2_vr");
        {
            char lv[32] = "", cats[512] = "";
            GetEnvironmentVariableA("D2VR_LOG", lv, sizeof(lv));
            GetEnvironmentVariableA("D2VR_LOG_CATS", cats, sizeof(cats));
            log::configure(lv, cats);
        }
        D2VR_LOG(log::Cat::proxy, log::Level::Info,
                 "=== Darkness II VR proxy loaded (darkness2vr %s, build %s, config %s, built %s %s) ===",
                 D2VR_VERSION, D2VR_BUILD_ID, D2VR_BUILD_CONFIG, __DATE__, __TIME__);
#if !D2VR_BUILD_OPTIMISED
        D2VR_LOG(log::Cat::proxy, log::Level::Warn,
                 "build: this is an UNOPTIMISED %s build (/Od, runtime checks, the debug CRT). "
                 "Fine for the harness and the debugger; measure on build.ps1 -Release", D2VR_BUILD_CONFIG);
#endif
        if (proxy::g_disabled)
            D2VR_LOG(log::Cat::proxy, log::Level::Warn,
                     "DISABLED by disable_vr.txt: the log and the route report still run; nothing else will "
                     "(no hooks, no seam, no status.json, no crash handler, no canaries)");
        proxy::log_route_report();
        // NOTE: nothing else here. The ini, the crash handler, the seam and every
        // hook wait for Direct3DCreate9(Ex), outside the loader lock.
    } else if (reason == DLL_PROCESS_DETACH) {
        D2VR_LOG(log::Cat::proxy, log::Level::Info, "=== proxy unloading ===");
        log::shutdown();
    }
    return TRUE;
}
