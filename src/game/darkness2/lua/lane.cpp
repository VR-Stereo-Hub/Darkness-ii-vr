// game/darkness2/lua/lane.cpp - the Lua lane (R2's in-game half, VR-233).
//
// The design is docs/ARCHITECTURE.md "The Lua lane" and the decision "the Lua
// lane enters at ScriptSystem::Resume, with lua_pcall as the control". The mod
// needs to reach engine objects by NAME the way the shipped scripts do, so it
// runs one chunk of its own text on the engine's main lua_State at the engine's
// own per-tick script entry, on the game thread, and only while that state is
// idle. Nothing here resolves at init: the wraps go in from the present tick,
// byte-verified, and every refusal says why with the values.
#define D2VR_CAT ::d2vr::log::Cat::lua
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "game/darkness2/lua/lane.h"
#include "game/darkness2/game.h"
#include "game/darkness2/patterns.h"
#include "core/config/config.h"
#include "core/hooks/detour.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/util/mem.h"

namespace pat = d2vr::game::pat;

// ---- the stubs (file scope, unnamespaced: MSVC inline asm names them) ---------
// Each stub saves everything, calls its C callback with the entry arguments,
// restores everything, replays the verified prologue bytes and jumps past them.
// After pushad + pushfd the hooked function's return address is [esp+24h] and
// its first stack argument [esp+28h]; `push [esp+28h]` reads with the esp
// BEFORE the push. The replay runs after the pops so a replayed `[esp+N]` read
// sees the original esp.
uintptr_t d2vr_lua_cont_resume = 0;      // kScriptResumeFn + 5
uintptr_t d2vr_lua_cont_luaresume = 0;   // kLuaResume + 5
uintptr_t d2vr_lua_cont_pcall = 0;       // kLuaPCall + 7

extern "C" void __cdecl d2vr_lua_on_script_resume(void* self, void* scriptThread);
extern "C" void __cdecl d2vr_lua_on_lua_resume(void* L);
extern "C" void __cdecl d2vr_lua_on_pcall(uintptr_t ret, void* L);

// ScriptSystem::Resume (thiscall, ecx = ScriptSystem, [esp+4] = the engine's
// script-thread object): replays `sub esp,7Ch ; push ebx ; push ebp`.
static __declspec(naked) void d2vr_stub_script_resume()
{
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 28h]
        push ecx
        call d2vr_lua_on_script_resume
        add esp, 8
        popfd
        popad
        sub esp, 7Ch
        push ebx
        push ebp
        jmp dword ptr [d2vr_lua_cont_resume]
    }
}

// lua_resume(L, narg) (cdecl): replays `push esi ; mov esi,[esp+8]`.
static __declspec(naked) void d2vr_stub_lua_resume()
{
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 28h]
        call d2vr_lua_on_lua_resume
        add esp, 4
        popfd
        popad
        push esi
        mov esi, [esp + 8]
        jmp dword ptr [d2vr_lua_cont_luaresume]
    }
}

// lua_pcall(L, nargs, nresults, errfunc) (cdecl): replays `mov ecx,[esp+10h] ;
// sub esp,8`. The callback gets (return address, L): the first push moves the
// return address from [esp+24h] to [esp+28h].
static __declspec(naked) void d2vr_stub_lua_pcall()
{
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 28h]
        push dword ptr [esp + 28h]
        call d2vr_lua_on_pcall
        add esp, 8
        popfd
        popad
        mov ecx, [esp + 10h]
        sub esp, 8
        jmp dword ptr [d2vr_lua_cont_pcall]
    }
}

namespace d2vr::game::lua {
namespace {

struct lua_State;
typedef int         (__cdecl *PFN_gettop)(lua_State*);
typedef void        (__cdecl *PFN_settop)(lua_State*, int);
typedef int         (__cdecl *PFN_loadbuffer)(lua_State*, const char*, size_t, const char*);
typedef int         (__cdecl *PFN_pcall)(lua_State*, int, int, int);
typedef const char* (__cdecl *PFN_tolstring)(lua_State*, int, size_t*);

// The verified entry points (cast once the prefixes have been checked).
const PFN_gettop     f_gettop     = (PFN_gettop)pat::kLuaGetTop;
const PFN_settop     f_settop     = (PFN_settop)pat::kLuaSetTop;
const PFN_loadbuffer f_loadbuffer = (PFN_loadbuffer)pat::kLuaLLoadBuffer;
const PFN_pcall      f_pcall      = (PFN_pcall)pat::kLuaPCall;
const PFN_tolstring  f_tolstring  = (PFN_tolstring)pat::kLuaToLString;

enum Wrap { kResume = 0, kLuaResume, kPCall, kWrapCount };
struct WrapState { const char* name; uintptr_t at; bool on; int reverts; int lastCheck; double hz; };
WrapState g_wrap[kWrapCount] = {
    { "ScriptSystem::Resume", pat::kScriptResumeFn, false, 0, 0, 0.0 },
    { "lua_resume",           pat::kLuaResume,      false, 0, 0, 0.0 },
    { "lua_pcall",            pat::kLuaPCall,       false, 0, 0, 0.0 },
};
d2vr::hooks::Detour g_detour[kWrapCount];
volatile LONG g_hits[kWrapCount] = { 0, 0, 0 };
unsigned long g_lastHits[kWrapCount] = {};
double g_lastLineMs = 0.0;
bool g_configApplied = false;
bool g_wanted = false;
bool g_verified = false;      // every Lua prefix matched (checked once, before any wrap)
bool g_verifyFailed = false;
volatile LONG g_poisoned = 0; // a fault inside the runner: the wraps stay, nothing runs

// The thread: ScriptSystem::Resume's first-seen thread is the game thread.
volatile LONG g_resumeThread = 0;
volatile LONG g_resumeThreadLogged = 0;
volatile LONG g_foreignResumes = 0;
volatile LONG g_lastForeignThread = 0;

// The l_G cross-check, updated at every lua_resume.
volatile LONG g_lgMatch = 0, g_lgMismatch = 0;
volatile LONG g_lgLastMatchPresent = -1;
volatile LONG g_lgLastMismatchL = 0, g_lgLastMismatchG = 0, g_lgLastHolderG = 0;
constexpr long kMatchFreshPresents = 120;

// The negative control: lua_pcall callers by class.
volatile LONG g_pcallEngine = 0, g_pcallOwn = 0;
uintptr_t g_pcallCallers[4] = {};
volatile LONG g_pcallCallerCount = 0;

// The mailbox: the present thread queues one chunk, the game thread runs it.
enum { kEmpty = 0, kQueued = 1, kRunning = 2 };
volatile LONG g_mail = kEmpty;
char g_text[4096] = "";
char g_tag[32] = "";
char g_result[512] = "";
volatile LONG g_chunksDone = 0, g_chunksFailed = 0;
volatile LONG g_deferrals = 0;
constexpr long kDeferralCap = 600;
volatile LONG g_lastRunThread = 0;

bool in_exe(uintptr_t a) { return a >= pat::kTextBegin && a < pat::kTextEnd; }

bool bytes_match(const char* what, uintptr_t at, const uint8_t* expected, size_t n)
{
    uint8_t now[32]; char hexNow[100], hexWant[100];
    if (n > sizeof(now)) n = sizeof(now);
    if (!d2vr::mem::range_readable((const void*)at, n)) {
        D2VR_ERROR("lua: %s at 0x%08lx is not readable - the lane refuses", what, (unsigned long)at);
        return false;
    }
    memcpy(now, (const void*)at, n);
    if (memcmp(now, expected, n) == 0) return true;
    d2vr::hooks::hex_bytes(now, n, hexNow, sizeof(hexNow));
    d2vr::hooks::hex_bytes(expected, n, hexWant, sizeof(hexWant));
    D2VR_ERROR("lua: %s at 0x%08lx reads %s, expected %s - wrong build or a live patch; the lane refuses",
               what, (unsigned long)at, hexNow, hexWant);
    return false;
}

// Every prefix in ONE pass, before any wrap goes in: once a wrap is live its
// site reads E9 .. .. .. .. 90 90 and a later verify would refuse for our own
// patch. lua_getfield/lua_setfield are not used (their prefixes are ambiguous).
bool verify_all()
{
    if (g_verified) return true;
    if (g_verifyFailed) return false;
    bool ok = true;
    ok &= bytes_match("ScriptSystem::Resume", pat::kScriptResumeFn, pat::kScriptResumePrefix, sizeof(pat::kScriptResumePrefix));
    ok &= bytes_match("lua_resume", pat::kLuaResume, pat::kLuaResumePrefix, sizeof(pat::kLuaResumePrefix));
    ok &= bytes_match("lua_pcall", pat::kLuaPCall, pat::kLuaPCallPrefix, sizeof(pat::kLuaPCallPrefix));
    ok &= bytes_match("luaL_loadbuffer", pat::kLuaLLoadBuffer, pat::kLuaLLoadBufferPrefix, sizeof(pat::kLuaLLoadBufferPrefix));
    ok &= bytes_match("lua_gettop", pat::kLuaGetTop, pat::kLuaGetTopPrefix, sizeof(pat::kLuaGetTopPrefix));
    ok &= bytes_match("lua_settop", pat::kLuaSetTop, pat::kLuaSetTopPrefix, sizeof(pat::kLuaSetTopPrefix));
    ok &= bytes_match("lua_tolstring", pat::kLuaToLString, pat::kLuaToLStringPrefix, sizeof(pat::kLuaToLStringPrefix));
    uint32_t holder = 0;
    if (!d2vr::mem::safe_read32(pat::kLuaStateHolder, &holder)) {
        D2VR_ERROR("lua: the state holder at 0x%08lx is not readable - the lane refuses", (unsigned long)pat::kLuaStateHolder);
        ok = false;
    }
    if (ok) {
        g_verified = true;
        D2VR_INFO("lua: every Lua prefix verified (7 functions); the main state holder reads %p (%s)",
                  (void*)(uintptr_t)holder, holder ? "a state exists" : "NULL: the VM is not created yet, the lane waits");
    } else {
        g_verifyFailed = true;
    }
    return ok;
}

bool install_all()
{
    if (g_wrap[kResume].on && g_wrap[kLuaResume].on && g_wrap[kPCall].on) return true;
    if (!d2vr::game::code_hooks_allowed()) {
        D2VR_ERROR("lua: REFUSED - code hooks are not allowed on this build (see the fingerprint line)");
        return false;
    }
    if (!verify_all()) return false;
    d2vr_lua_cont_pcall = pat::kLuaPCall + pat::kLuaPCallDetourLen;
    d2vr_lua_cont_luaresume = pat::kLuaResume + pat::kLuaResumeDetourLen;
    d2vr_lua_cont_resume = pat::kScriptResumeFn + pat::kScriptResumeDetourLen;
    // The control and the cross-check first, the live entry last: a chunk can
    // never run before the counters that gate it exist.
    bool ok = d2vr::hooks::detour_install(g_detour[kPCall], "lua pcall", pat::kLuaPCall, pat::kLuaPCallPrefix,
                                          pat::kLuaPCallDetourLen, (const void*)d2vr_stub_lua_pcall);
    g_wrap[kPCall].on = ok;
    if (ok) {
        ok = d2vr::hooks::detour_install(g_detour[kLuaResume], "lua resume", pat::kLuaResume, pat::kLuaResumePrefix,
                                         pat::kLuaResumeDetourLen, (const void*)d2vr_stub_lua_resume);
        g_wrap[kLuaResume].on = ok;
    }
    if (ok) {
        ok = d2vr::hooks::detour_install(g_detour[kResume], "lua script-resume", pat::kScriptResumeFn, pat::kScriptResumePrefix,
                                         pat::kScriptResumeDetourLen, (const void*)d2vr_stub_script_resume);
        g_wrap[kResume].on = ok;
    }
    if (!ok) {
        for (int i = 0; i < kWrapCount; i++) if (g_wrap[i].on) { d2vr::hooks::detour_remove(g_detour[i], g_wrap[i].name); g_wrap[i].on = false; }
        D2VR_ERROR("lua: a wrap refused; every wrap removed again (all or nothing)");
        return false;
    }
    D2VR_INFO("lua: LIVE - wraps at ScriptSystem::Resume 0x%08lx, lua_resume 0x%08lx, lua_pcall 0x%08lx; "
              "a chunk runs at the next Resume once l_G has matched (Resume hits/s should equal the script rate, "
              "engine pcalls must stay at 0)",
              (unsigned long)pat::kScriptResumeFn, (unsigned long)pat::kLuaResume, (unsigned long)pat::kLuaPCall);
    return true;
}

void remove_all()
{
    for (int i = 0; i < kWrapCount; i++) {
        if (!g_wrap[i].on) continue;
        d2vr::hooks::detour_remove(g_detour[i], g_wrap[i].name);
        g_wrap[i].on = false;
    }
    D2VR_INFO("lua: wraps removed (hits so far: Resume %ld, lua_resume %ld, lua_pcall %ld)",
              (long)g_hits[kResume], (long)g_hits[kLuaResume], (long)g_hits[kPCall]);
}

void set(bool on)
{
    g_wanted = on;
    if (on) install_all(); else remove_all();
}

const char* mail_name(LONG m) { return m == kEmpty ? "empty" : m == kQueued ? "queued" : "running"; }

// ---- the runner (game thread, inside the ScriptSystem::Resume stub) ----------
struct StateView { uintptr_t L; uint8_t status; uintptr_t ci, baseCi; uint16_t nCcalls; bool ok; };

int runner_filter(unsigned code)
{
    InterlockedExchange(&g_poisoned, 1);
    D2VR_ERROR("lua: EXCEPTION 0x%08x inside the chunk runner - the lane is POISONED for this session "
               "(the wraps stay, nothing runs; darkness2_vr_crash.txt has the fingerprint if the handler saw it)", code);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Reads the main state's idle fields; false (with ok=false) on a fault.
bool read_state(StateView& v)
{
    v.ok = false;
    __try {
        v.L = *(volatile uintptr_t*)pat::kLuaStateHolder;
        if (!v.L) return false;
        v.status  = *(volatile uint8_t*)(v.L + 6);
        v.ci      = *(volatile uintptr_t*)(v.L + pat::kLuaStateCi);
        v.baseCi  = *(volatile uintptr_t*)(v.L + pat::kLuaStateBaseCi);
        v.nCcalls = *(volatile uint16_t*)(v.L + pat::kLuaStateNCcalls);
        v.ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        v.ok = false;
    }
    return v.ok;
}

// The chunk on the main state: load, pcall for one result, read it as a string,
// restore the stack. Returns the pcall/load status (0 = ran).
int run_chunk(lua_State* L, const char* text, size_t len, char* result, size_t cap)
{
    const int top = f_gettop(L);
    int st = f_loadbuffer(L, text, len, "=d2vr");
    if (st == 0) st = f_pcall(L, 0, 1, 0);
    size_t n = 0;
    const char* s = f_tolstring(L, -1, &n);
    if (s) {
        if (n >= cap) n = cap - 1;
        memcpy(result, s, n); result[n] = 0;
    } else {
        snprintf(result, cap, st == 0 ? "(no string returned)" : "(error object is not a string)");
    }
    f_settop(L, top);
    return st;
}

void run_queued()
{
    StateView v;
    const bool readOk = read_state(v);
    const long presents = (long)d2vr::frame::presents();
    const long lastMatch = g_lgLastMatchPresent;
    const bool fresh = lastMatch >= 0 && presents - lastMatch <= kMatchFreshPresents;
    const bool idle = readOk && v.status == 0 && v.ci == v.baseCi && v.nCcalls == 0;
    if (!readOk || !fresh || !idle) {
        const long n = InterlockedIncrement(&g_deferrals);
        D2VR_LOG_FIRST_N(D2VR_CAT, d2vr::log::Level::Info, 3,
            "lua: chunk '%s' DEFERRED at Resume (holder L=%p readable=%d status=%u ci=%p base_ci=%p nCcalls=%u; "
            "l_G match %s, %ld presents ago) - retried at the next Resume (%ld so far, cap %ld)",
            g_tag, (void*)v.L, (int)readOk, (unsigned)v.status, (void*)v.ci, (void*)v.baseCi, (unsigned)v.nCcalls,
            lastMatch >= 0 ? "seen" : "NEVER", lastMatch >= 0 ? presents - lastMatch : -1L, n, kDeferralCap);
        if (n >= kDeferralCap) {
            snprintf(g_result, sizeof(g_result), "deferred out after %ld Resumes (readable=%d status=%u ci==base_ci=%d nCcalls=%u fresh=%d)",
                     n, (int)readOk, (unsigned)v.status, (int)(v.ci == v.baseCi), (unsigned)v.nCcalls, (int)fresh);
            D2VR_WARN("lua: chunk '%s' DROPPED: %s", g_tag, g_result);
            InterlockedIncrement(&g_chunksFailed);
            InterlockedIncrement(&g_chunksDone);
            InterlockedExchange(&g_mail, kEmpty);
            return;
        }
        InterlockedExchange(&g_mail, kQueued);
        return;
    }
    int st = -1;
    __try {
        st = run_chunk((lua_State*)v.L, g_text, strlen(g_text), g_result, sizeof(g_result));
    } __except (runner_filter(GetExceptionCode())) {
        snprintf(g_result, sizeof(g_result), "EXCEPTION (the lane is poisoned)");
    }
    g_lastRunThread = (LONG)GetCurrentThreadId();
    if (st == 0) {
        D2VR_INFO("lua: chunk '%s' RAN on thread %lu at Resume hit %ld (deferred %ld times) -> %s",
                  g_tag, (unsigned long)g_lastRunThread, (long)g_hits[kResume], (long)g_deferrals, g_result);
    } else {
        InterlockedIncrement(&g_chunksFailed);
        D2VR_WARN("lua: chunk '%s' FAILED (status %d) on thread %lu -> %s", g_tag, st, (unsigned long)g_lastRunThread, g_result);
    }
    InterlockedIncrement(&g_chunksDone);
    InterlockedExchange(&g_mail, kEmpty);
}

} // namespace

// ---- the callbacks the stubs call (game thread; nothing here may log per call) --
} // namespace d2vr::game::lua

extern "C" void __cdecl d2vr_lua_on_script_resume(void* self, void* scriptThread)
{
    using namespace d2vr::game::lua;
    (void)self; (void)scriptThread;
    InterlockedIncrement(&g_hits[kResume]);
    const LONG tid = (LONG)GetCurrentThreadId();
    if (!g_resumeThread) InterlockedCompareExchange(&g_resumeThread, tid, 0);
    if (tid != g_resumeThread) {
        InterlockedIncrement(&g_foreignResumes);
        InterlockedExchange(&g_lastForeignThread, tid);
        return;
    }
    if (g_poisoned || g_mail != kQueued) return;
    if (InterlockedCompareExchange(&g_mail, kRunning, kQueued) != kQueued) return;
    run_queued();
}

extern "C" void __cdecl d2vr_lua_on_lua_resume(void* L)
{
    using namespace d2vr::game::lua;
    InterlockedIncrement(&g_hits[kLuaResume]);
    uintptr_t g = 0, gh = 0, Lh = 0;
    __try {
        g = *(volatile uintptr_t*)((uintptr_t)L + pat::kLuaStateGlobal);
        Lh = *(volatile uintptr_t*)pat::kLuaStateHolder;
        if (Lh) gh = *(volatile uintptr_t*)(Lh + pat::kLuaStateGlobal);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g = 0;
    }
    if (g && g == gh) {
        InterlockedIncrement(&g_lgMatch);
        InterlockedExchange(&g_lgLastMatchPresent, (LONG)d2vr::frame::presents());
    } else {
        InterlockedIncrement(&g_lgMismatch);
        InterlockedExchange(&g_lgLastMismatchL, (LONG)(uintptr_t)L);
        InterlockedExchange(&g_lgLastMismatchG, (LONG)g);
        InterlockedExchange(&g_lgLastHolderG, (LONG)gh);
    }
}

extern "C" void __cdecl d2vr_lua_on_pcall(uintptr_t ret, void* L)
{
    using namespace d2vr::game::lua;
    (void)L;
    InterlockedIncrement(&g_hits[kPCall]);
    if (in_exe(ret)) {
        InterlockedIncrement(&g_pcallEngine);
        for (int i = 0; i < 4; i++) {
            if (g_pcallCallers[i] == ret) break;
            if (g_pcallCallers[i] == 0) { g_pcallCallers[i] = ret; InterlockedIncrement(&g_pcallCallerCount); break; }
        }
    } else {
        InterlockedIncrement(&g_pcallOwn);
    }
}

namespace d2vr::game::lua {

bool on() { return g_wrap[kResume].on && g_wrap[kLuaResume].on && g_wrap[kPCall].on; }
void set_on(bool on) { set(on); }
Counters counters()
{
    Counters c;
    c.thread = (unsigned long)g_resumeThread; c.foreignResumes = (unsigned long)g_foreignResumes;
    c.resumeHits = (unsigned long)g_hits[kResume]; c.luaResumeHits = (unsigned long)g_hits[kLuaResume];
    c.lgMatch = (unsigned long)g_lgMatch; c.lgMismatch = (unsigned long)g_lgMismatch;
    c.pcallEngine = (unsigned long)g_pcallEngine; c.pcallOwn = (unsigned long)g_pcallOwn;
    c.chunksDone = (unsigned long)g_chunksDone; c.chunksFailed = (unsigned long)g_chunksFailed;
    c.resumeHz = g_wrap[kResume].hz; c.poisoned = g_poisoned != 0; c.slot = mail_name(g_mail);
    return c;
}
uint32_t chunks_done() { return (uint32_t)g_chunksDone; }
uint32_t chunks_failed() { return (uint32_t)g_chunksFailed; }
const char* last_result() { return g_result; }

void init_from_config()
{
    if (g_configApplied) return;
    g_configApplied = true;
    const int want = d2vr::config::get().luaEnabled;
    D2VR_INFO("lua: ini asks [Lua] Enabled=%d (default OFF; `lua on` installs the wraps live)", want);
    if (want) set(true);
}

bool run(const char* text, const char* tag)
{
    if (!on()) { D2VR_WARN("lua: run '%s' REFUSED - the lane is off (`lua on` first)", tag); return false; }
    if (g_poisoned) { D2VR_WARN("lua: run '%s' REFUSED - the lane is poisoned for this session", tag); return false; }
    if (!text || !text[0]) { D2VR_WARN("lua: run '%s' REFUSED - empty chunk", tag); return false; }
    const size_t len = strlen(text);
    if (len >= sizeof(g_text)) { D2VR_WARN("lua: run '%s' REFUSED - chunk is %u bytes, the slot holds %u", tag, (unsigned)len, (unsigned)sizeof(g_text) - 1); return false; }
    if (g_mail != kEmpty) { D2VR_WARN("lua: run '%s' REFUSED - the slot is %s with '%s'", tag, mail_name(g_mail), g_tag); return false; }
    memcpy(g_text, text, len + 1);
    snprintf(g_tag, sizeof(g_tag), "%s", tag ? tag : "chunk");
    g_result[0] = 0;
    InterlockedExchange(&g_deferrals, 0);
    InterlockedExchange(&g_mail, kQueued);
    D2VR_INFO("lua: queued chunk '%s' (%u bytes); it runs at the next ScriptSystem::Resume on the game thread "
              "(Resume hits so far %ld, l_G match %ld)", g_tag, (unsigned)len, (long)g_hits[kResume], (long)g_lgMatch);
    return true;
}

// The shipped SetFov.lua's object path, as the mod's own text: no Sleep (a
// yield cannot cross the lane's pcall), every step nil-checked, and the value
// read back through GetBaseFovOverride so "the field took it" is a separate
// fact from "the projection moved" (the eyetest's question).
bool fov(float deg)
{
    char chunk[1024];
    snprintf(chunk, sizeof(chunk),
        "if gRegion == nil or IsNull(gRegion) then return 'noregion' end\n"
        "local players = gRegion:GetHumanPlayers()\n"
        "if players == nil or players[1] == nil then return 'noplayers' end\n"
        "local avatar = players[1]:GetAvatar()\n"
        "if IsNull(avatar) then return 'noavatar' end\n"
        "local c = avatar:CameraControl()\n"
        "if IsNull(c) or c:IsNullCameraController() then return 'nocamctrl' end\n"
        "c:SetBaseFovOverride(%.2f)\n"
        "return 'SetBaseFovOverride(%.2f) -> GetBaseFovOverride=' .. tostring(c:GetBaseFovOverride())\n",
        deg, deg);
    char tag[32];
    snprintf(tag, sizeof(tag), "fov %.1f", deg);
    return run(chunk, tag);
}

void tick(double nowMs)
{
    if (g_resumeThread && !g_resumeThreadLogged) {
        g_resumeThreadLogged = 1;
        const DWORD pt = d2vr::frame::present_thread();
        D2VR_INFO("lua: ScriptSystem::Resume first seen on thread %lu; the present thread is %lu -> %s",
                  (unsigned long)g_resumeThread, (unsigned long)pt,
                  (DWORD)g_resumeThread == pt ? "the SAME thread (the game thread presents)" : "a DIFFERENT thread (the lane runs on Resume's thread)");
    }
    const double every = 1000.0 * d2vr::config::get().canaryLogHz;
    if (g_lastLineMs != 0.0 && nowMs - g_lastLineMs < every) return;
    const double dt = g_lastLineMs == 0.0 ? 0.0 : (nowMs - g_lastLineMs) / 1000.0;
    g_lastLineMs = nowMs;
    for (int i = 0; i < kWrapCount; i++) {
        WrapState& s = g_wrap[i];
        const unsigned long hits = (unsigned long)g_hits[i];
        s.hz = dt > 0.0 ? (hits - g_lastHits[i]) / dt : 0.0;
        g_lastHits[i] = hits;
        if (!s.on) continue;
        uint8_t now[16]; char hex[64];
        const int check = d2vr::hooks::detour_check(g_detour[i], now);
        d2vr::hooks::hex_bytes(now, g_detour[i].len, hex, sizeof(hex));
        if (check != 0 && s.lastCheck == 0) {
            s.reverts++;
            s.on = false;
            D2VR_ERROR("lua: %s wrap at 0x%08lx %s: now %s (revert #%d) - CEG or the engine rewrote our patch; the lane is off",
                       s.name, (unsigned long)s.at, check == 1 ? "REVERTED to the original" : "CHANGED to something else", hex, s.reverts);
        } else {
            D2VR_LOG(D2VR_CAT, d2vr::log::Level::Info, "lua %-20s 0x%08lx: %s (%s) hits=%lu %.1f/s%s",
                     s.name, (unsigned long)s.at, check == 0 ? "intact" : "changed", hex, hits, s.hz,
                     i == kPCall ? " (the control: the engine calls lua_pcall at VM creation only)" : "");
        }
        s.lastCheck = check;
    }
    if (!on()) return;
    const long presents = (long)d2vr::frame::presents();
    const long lastMatch = g_lgLastMatchPresent;
    char callers[80] = "";
    for (int i = 0; i < 4 && g_pcallCallers[i]; i++) {
        char t[20]; snprintf(t, sizeof(t), "%s0x%08lx", i ? "," : "", (unsigned long)g_pcallCallers[i]);
        strncat(callers, t, sizeof(callers) - strlen(callers) - 1);
    }
    D2VR_INFO("lua lane: thread %lu, foreign Resumes %ld%s | l_G MATCH %ld MISMATCH %ld (last match %s) | "
              "engine pcalls %ld (must read 0; %s) own %ld | chunks done %ld failed %ld, slot %s%s%s",
              (unsigned long)g_resumeThread, (long)g_foreignResumes,
              g_foreignResumes ? " (a Resume off the game thread: the design's kill finding)" : "",
              (long)g_lgMatch, (long)g_lgMismatch,
              lastMatch < 0 ? "NEVER: no chunk runs" : (presents - lastMatch <= kMatchFreshPresents ? "fresh" : "STALE: no chunk runs"),
              (long)g_pcallEngine, g_pcallEngine ? callers : "0 = no engine caller since the wrap went live",
              (long)g_pcallOwn, (long)g_chunksDone, (long)g_chunksFailed, mail_name(g_mail),
              g_mail != kEmpty ? " " : "", g_mail != kEmpty ? g_tag : "");
    if (g_lgMismatch) {
        D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Warn, 30000,
            "lua: l_G MISMATCH seen %ld times (last: resumed L=%p its l_G=%p, the holder's l_G=%p) - a second VM, or the holder moved",
            (long)g_lgMismatch, (void*)(uintptr_t)g_lgLastMismatchL, (void*)(uintptr_t)g_lgLastMismatchG, (void*)(uintptr_t)g_lgLastHolderG);
    }
}

bool command(const char* cmd, const char* args)
{
    if (strcmp(cmd, "lua")) return false;
    char a[16] = "";
    sscanf(args, "%15s", a);
    if (!a[0] || !strcmp(a, "status")) {
        const long presents = (long)d2vr::frame::presents();
        for (int i = 0; i < kWrapCount; i++)
            D2VR_INFO("lua %-20s at 0x%08lx: %s, hits=%ld (%.1f/s), reverts=%d", g_wrap[i].name, (unsigned long)g_wrap[i].at,
                      g_wrap[i].on ? "LIVE" : g_wanted ? "wanted but REFUSED" : "off", (long)g_hits[i], g_wrap[i].hz, g_wrap[i].reverts);
        D2VR_INFO("lua status: %s%s, verified=%d, thread %lu (present thread %lu), foreign Resumes %ld, l_G MATCH %ld MISMATCH %ld "
                  "(last match %ld presents ago), engine pcalls %ld own %ld, chunks done %ld failed %ld, slot %s, last result '%s'",
                  on() ? "ON" : "OFF", g_poisoned ? " (POISONED)" : "", (int)g_verified, (unsigned long)g_resumeThread,
                  (unsigned long)d2vr::frame::present_thread(), (long)g_foreignResumes, (long)g_lgMatch, (long)g_lgMismatch,
                  g_lgLastMatchPresent >= 0 ? presents - g_lgLastMatchPresent : -1L, (long)g_pcallEngine, (long)g_pcallOwn,
                  (long)g_chunksDone, (long)g_chunksFailed, mail_name(g_mail), g_result);
        return true;
    }
    if (!strcmp(a, "on")) { set(true); return true; }
    if (!strcmp(a, "off")) { set(false); return true; }
    if (!strcmp(a, "run")) {
        const char* text = args + 3;
        while (*text == ' ') text++;
        run(text, "run");
        return true;
    }
    if (!strcmp(a, "fov")) {
        float deg = 0.0f;
        if (sscanf(args, "%*s %f", &deg) != 1 || deg < 0.0f || deg > 179.0f) {
            D2VR_WARN("lua: usage - lua fov <degrees> (0 resets to the game's own FOV)");
            return true;
        }
        fov(deg);
        return true;
    }
    if (!strcmp(a, "swigcheck")) {
        int match = 0;
        for (const auto& m : pat::kSwigModules) {
            uint32_t size = 0, typeInitial = 0;
            const bool r1 = d2vr::mem::safe_read32(m.module + 4, &size);
            const bool r2 = d2vr::mem::safe_read32(m.module + 0xC, &typeInitial);
            const bool ok = r1 && r2 && size == m.types && typeInitial == m.typeInitial;
            if (ok) match++;
            D2VR_INFO("lua swigcheck: %-12s module 0x%08lx size=%u (map %u) type_initial=0x%08lx (map 0x%08lx) -> %s",
                      m.name, (unsigned long)m.module, (unsigned)size, (unsigned)m.types, (unsigned long)typeInitial,
                      (unsigned long)m.typeInitial, ok ? "MATCH" : (!r1 || !r2) ? "UNREADABLE" : "MISMATCH");
        }
        D2VR_INFO("lua swigcheck: %d of 10 swig_module_info structs match docs/darkness2/swig-api.md%s", match,
                  match == 10 ? "" : " - the map does not describe this build's tables");
        return true;
    }
    D2VR_WARN("lua: usage - lua status | on | off | run <text> | fov <deg> | swigcheck");
    return true;
}

void status(d2vr::status::Writer& w)
{
    const long presents = (long)d2vr::frame::presents();
    w.kv("on", on());
    w.kv("wanted", g_wanted);
    w.kv("verified", g_verified);
    w.kv("poisoned", g_poisoned != 0);
    w.kv("thread", (unsigned long)g_resumeThread);
    w.kv("foreignResumes", (unsigned long)g_foreignResumes);
    w.kv("resumeHits", (unsigned long)g_hits[kResume]);
    w.kv("resumeHz", g_wrap[kResume].hz);
    w.kv("luaResumeHits", (unsigned long)g_hits[kLuaResume]);
    w.kv("lgMatch", (unsigned long)g_lgMatch);
    w.kv("lgMismatch", (unsigned long)g_lgMismatch);
    w.kv("lgLastMatchAgo", g_lgLastMatchPresent >= 0 ? (int)(presents - g_lgLastMatchPresent) : -1);
    w.kv("pcallEngine", (unsigned long)g_pcallEngine);
    w.kv("pcallOwn", (unsigned long)g_pcallOwn);
    w.kv("chunksDone", (unsigned long)g_chunksDone);
    w.kv("chunksFailed", (unsigned long)g_chunksFailed);
    w.kv("slot", mail_name(g_mail));
    w.kv("lastResult", g_result);
    int reverts = 0;
    for (int i = 0; i < kWrapCount; i++) reverts += g_wrap[i].reverts;
    w.kv("reverts", reverts);
}

} // namespace d2vr::game::lua
