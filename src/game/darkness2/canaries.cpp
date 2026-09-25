#define D2VR_CAT ::d2vr::log::Cat::canary
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "game/darkness2/canaries.h"
#include "game/darkness2/game.h"
#include "game/darkness2/patterns.h"
#include "core/config/config.h"
#include "core/hooks/detour.h"
#include "core/hooks/callsite.h"
#include "core/util/log.h"

// The stubs' globals live at file scope, unnamespaced, so MSVC inline asm can
// name them. Counters are incremented with `lock inc`; the pump runs on one
// thread but the cold site's thread is not ours to assume.
volatile LONG d2vr_canary_hits[4] = { 0, 0, 0, 0 };
uintptr_t d2vr_canary_resume_cold = 0;
uintptr_t d2vr_canary_resume_tick = 0;
uintptr_t d2vr_canary_callsite_target = 0;
uintptr_t d2vr_canary_resume_hot = 0;

// cold: replays `sub esp,1Ch ; push esi ; mov esi,ecx` (the 6 verified bytes at kDx9InitFn)
static __declspec(naked) void d2vr_stub_cold()
{
    __asm {
        pushfd
        lock inc dword ptr [d2vr_canary_hits + 0]
        popfd
        sub esp, 1Ch
        push esi
        mov esi, ecx
        jmp dword ptr [d2vr_canary_resume_cold]
    }
}

// tick: replays `sub esp,1Ch ; push ebx ; push ebp` (the 5 verified bytes at kMsgPumpFn)
static __declspec(naked) void d2vr_stub_tick()
{
    __asm {
        pushfd
        lock inc dword ptr [d2vr_canary_hits + 4]
        popfd
        sub esp, 1Ch
        push ebx
        push ebp
        jmp dword ptr [d2vr_canary_resume_tick]
    }
}

// callsite: count, then tail-jump to the original target so its `ret` returns
// to the rewritten call's own return address with eax intact.
static __declspec(naked) void d2vr_stub_callsite()
{
    __asm {
        pushfd
        lock inc dword ptr [d2vr_canary_hits + 8]
        popfd
        jmp dword ptr [d2vr_canary_callsite_target]
    }
}

// hot: replays `push ebp ; push esi ; push edi ; mov edi,ecx` (the 5 verified bytes at kPresentWrapperFn)
static __declspec(naked) void d2vr_stub_hot()
{
    __asm {
        pushfd
        lock inc dword ptr [d2vr_canary_hits + 12]
        popfd
        push ebp
        push esi
        push edi
        mov edi, ecx
        jmp dword ptr [d2vr_canary_resume_hot]
    }
}

namespace d2vr::game::canaries {
namespace {

enum { kCold = 0, kTick, kCallSite, kHot, kCount };
State g_state[kCount] = {
    { "cold", false, false, pat::kDx9InitFn, 0, 0.0, 0, 0 },
    { "tick", false, false, pat::kMsgPumpFn, 0, 0.0, 0, 0 },
    { "callsite", false, false, pat::kPumpCallSite, 0, 0.0, 0, 0 },
    { "hot", false, false, pat::kHotSite, 0, 0.0, 0, 0 },
};
d2vr::hooks::Detour g_detour[kCount];
d2vr::hooks::CallSite g_callsite;
unsigned long g_lastHits[kCount] = {};
double g_lastLineMs = 0.0;
bool g_configApplied = false;

int index_of(const char* name)
{
    for (int i = 0; i < kCount; i++) if (!strcmp(g_state[i].name, name)) return i;
    return -1;
}

bool install(int i)
{
    if (g_state[i].on) return true;
    if (!d2vr::game::code_hooks_allowed()) {
        D2VR_ERROR("canary %s: REFUSED - code hooks are not allowed on this build (see the fingerprint line)", g_state[i].name);
        return false;
    }
    bool ok = false;
    switch (i) {
    case kCold:
        d2vr_canary_resume_cold = pat::kDx9InitFn + pat::kDx9InitDetourLen;
        ok = d2vr::hooks::detour_install(g_detour[i], "canary cold", pat::kDx9InitFn, pat::kDx9InitPrefix,
                                         pat::kDx9InitDetourLen, (const void*)d2vr_stub_cold);
        break;
    case kTick:
        d2vr_canary_resume_tick = pat::kMsgPumpFn + pat::kMsgPumpDetourLen;
        ok = d2vr::hooks::detour_install(g_detour[i], "canary tick", pat::kMsgPumpFn, pat::kMsgPumpPrefix,
                                         pat::kMsgPumpDetourLen, (const void*)d2vr_stub_tick);
        break;
    case kCallSite:
        d2vr_canary_callsite_target = pat::kPumpCallTarget;
        ok = d2vr::hooks::callsite_install(g_callsite, "canary callsite", pat::kPumpCallSite, pat::kPumpCallTarget,
                                           (const void*)d2vr_stub_callsite);
        break;
    case kHot:
        if (!pat::kHotSite) {
            D2VR_ERROR("canary hot: REFUSED - the hot site is not derived yet (launch 1 logs the Present return address)");
            return false;
        }
        d2vr_canary_resume_hot = pat::kPresentWrapperFn + pat::kPresentWrapperDetourLen;
        ok = d2vr::hooks::detour_install(g_detour[i], "canary hot", pat::kPresentWrapperFn, pat::kPresentWrapperPrefix,
                                         pat::kPresentWrapperDetourLen, (const void*)d2vr_stub_hot);
        break;
    }
    g_state[i].on = ok;
    if (ok) D2VR_INFO("canary %s: LIVE at 0x%08lx (hits so far %ld; a per-tick site should count every frame, a cold site should stay at 0)",
                      g_state[i].name, (unsigned long)g_state[i].at, (long)d2vr_canary_hits[i]);
    return ok;
}

void remove(int i)
{
    if (!g_state[i].on) return;
    if (i == kCallSite) d2vr::hooks::callsite_remove(g_callsite, "canary callsite");
    else d2vr::hooks::detour_remove(g_detour[i], g_state[i].name);
    g_state[i].on = false;
}

void set(int i, bool on)
{
    g_state[i].wanted = on;
    if (on) install(i); else remove(i);
}

} // namespace

int count() { return kCount; }
const State& state(int i) { return g_state[i]; }

void init_from_config()
{
    if (g_configApplied) return;
    g_configApplied = true;
    const auto& c = d2vr::config::get();
    D2VR_INFO("canaries: ini asks cold=%d tick=%d callsite=%d hot=%d (all default OFF; R1's soak sets them ON)",
              c.canaryCold, c.canaryTick, c.canaryCallSite, c.canaryHot);
    if (c.canaryCold) set(kCold, true);
    if (c.canaryTick) set(kTick, true);
    if (c.canaryCallSite) set(kCallSite, true);
    if (c.canaryHot) set(kHot, true);
}

bool command(const char* cmd, const char* args)
{
    if (strcmp(cmd, "canary")) return false;
    char a[32] = "", b[32] = "";
    sscanf(args, "%31s %31s", a, b);
    if (!a[0] || !strcmp(a, "status")) {
        for (int i = 0; i < kCount; i++)
            D2VR_INFO("canary %-8s at 0x%08lx: %s, hits=%ld (%.1f/s), reverts=%d, last check=%s", g_state[i].name,
                      (unsigned long)g_state[i].at, g_state[i].on ? "LIVE" : g_state[i].wanted ? "wanted but REFUSED" : "off",
                      (long)d2vr_canary_hits[i], g_state[i].hz, g_state[i].reverts,
                      !g_state[i].on ? "n/a" : g_state[i].lastCheck == 0 ? "intact" : g_state[i].lastCheck == 1 ? "REVERTED" : "CHANGED");
        return true;
    }
    if (!strcmp(a, "all")) {
        const bool on = !strcmp(b, "on");
        for (int i = 0; i < kCount; i++) set(i, on);
        return true;
    }
    const int i = index_of(a);
    if (i < 0) { D2VR_WARN("canary: usage - canary <cold|tick|callsite|hot|all> on|off | canary status"); return true; }
    if (!strcmp(b, "on")) set(i, true);
    else if (!strcmp(b, "off")) set(i, false);
    else D2VR_WARN("canary %s: usage - canary %s on|off", a, a);
    return true;
}

void tick(double nowMs)
{
    const double every = 1000.0 * d2vr::config::get().canaryLogHz;
    if (g_lastLineMs != 0.0 && nowMs - g_lastLineMs < every) return;
    const double dt = g_lastLineMs == 0.0 ? 0.0 : (nowMs - g_lastLineMs) / 1000.0;
    g_lastLineMs = nowMs;
    for (int i = 0; i < kCount; i++) {
        State& s = g_state[i];
        const unsigned long hits = (unsigned long)d2vr_canary_hits[i];
        s.hits = hits;
        s.hz = dt > 0.0 ? (hits - g_lastHits[i]) / dt : 0.0;
        g_lastHits[i] = hits;
        if (!s.on) continue;
        uint8_t now[16]; char hex[64];
        const int check = (i == kCallSite) ? d2vr::hooks::callsite_check(g_callsite, now)
                                           : d2vr::hooks::detour_check(g_detour[i], now);
        const size_t len = (i == kCallSite) ? 5 : g_detour[i].len;
        d2vr::hooks::hex_bytes(now, len, hex, sizeof(hex));
        if (check != 0 && s.lastCheck == 0) {
            s.reverts++;
            D2VR_ERROR("canary %s: bytes at 0x%08lx %s at tick %lu: now %s (revert #%d) - CEG or the engine rewrote our patch",
                       s.name, (unsigned long)s.at, check == 1 ? "REVERTED to the original" : "CHANGED to something else",
                       (unsigned long)GetTickCount(), hex, s.reverts);
            s.on = false;   // the hook is gone; do not claim it is live
        } else {
            D2VR_LOG(D2VR_CAT, d2vr::log::Level::Info, "canary %-8s 0x%08lx: %s (%s) hits=%lu %.1f/s%s",
                     s.name, (unsigned long)s.at, check == 0 ? "intact" : "changed", hex, hits, s.hz,
                     (i == kCold && hits == 0) ? " (0 expected: the init ran before the hook)" : "");
        }
        s.lastCheck = check;
    }
}

} // namespace d2vr::game::canaries
