// game/darkness2/commands.cpp - the game side of the framework: init, the
// seam's game words, the status.json provider and the present tick.
#define D2VR_CAT ::d2vr::log::Cat::game
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "game/darkness2/game.h"
#include "game/darkness2/patterns.h"
#include "game/darkness2/canaries.h"
#include "game/darkness2/lua/lane.h"
#include "proxy/proxy.h"
#include "core/config/config.h"
#include "core/framework/command.h"
#include "core/framework/status.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/shot.h"
#include "core/gfx/capture.h"
#include "core/gfx/d3d11_device.h"
#include "core/gfx/stereo.h"
#include "core/vr/openxr_runtime.h"
#include "core/input/inject.h"
#include "core/util/crash.h"
#include "core/util/diag.h"
#include "core/util/log.h"
#include "core/util/mem.h"
#include "core/util/paths.h"
#include "d2vr_version.h"

struct IDirect3DDevice9;

namespace d2vr::game {
namespace {

bool g_inited = false;
bool g_canariesArmed = false;
unsigned long g_ticks = 0;

void log_ex_flag()
{
    uint32_t v = 0;
    const uintptr_t aligned = pat::kD3D9ExEnableFlag & ~3u;
    if (d2vr::mem::safe_read32(aligned, &v)) {
        const unsigned byte = (v >> (8 * (pat::kD3D9ExEnableFlag & 3))) & 0xFF;
        D2VR_INFO("Graphics.EnableDirect3D9Ex byte at 0x%08lx = %u (%s)", (unsigned long)pat::kD3D9ExEnableFlag, byte,
                  byte ? "the engine creates through Direct3DCreate9Ex" : "the engine will use Direct3DCreate9");
    } else {
        D2VR_WARN("Graphics.EnableDirect3D9Ex byte at 0x%08lx is not readable", (unsigned long)pat::kD3D9ExEnableFlag);
    }
}

bool on_off(const char* v, bool* out)
{
    if (!v) return false;
    if (!_stricmp(v, "on") || !strcmp(v, "1") || !_stricmp(v, "true")) { *out = true; return true; }
    if (!_stricmp(v, "off") || !strcmp(v, "0") || !_stricmp(v, "false")) { *out = false; return true; }
    return false;
}

bool game_command(const char* cmd, const char* args)
{
    if (canaries::command(cmd, args)) return true;
    if (lua::command(cmd, args)) return true;
    // The stereo seam (VR-241): `stereo` / `stereo status`, `stereo <mono|aer|reentry>`
    // (a refusal leaves the previous method running), `stereo arm on|off`.
    if (!strcmp(cmd, "stereo")) {
        if (!args[0] || !strcmp(args, "status")) { d2vr::stereo::log_status(); return true; }
        char sub[16] = "", v[16] = "";
        bool b = false;
        if (sscanf(args, "%15s %15s", sub, v) == 2 && !strcmp(sub, "arm")) {
            if (on_off(v, &b)) d2vr::stereo::set_armed(b);
            else D2VR_INFO("stereo: arm on|off (now %s, selected '%s')", d2vr::stereo::armed() ? "armed" : "parked", d2vr::stereo::wanted_name());
            return true;
        }
        d2vr::stereo::choose(args);   // logs the refusal itself; an explicit choice is the selection
        return true;
    }
    // The carry: `capture` prints the cost per present; `capture mode sync|deferred|shared|off`
    // is the live A/B; `capture sharedwait on|off`; `capture bbox off|<ms>`; `capture reinit`.
    if (!strcmp(cmd, "capture")) {
        char sub[16] = "", m[16] = "";
        bool b = false;
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "mode")) {
            if (!_stricmp(m, "shared") && !d2vr::config::get().deviceEx) {
                D2VR_WARN("capture: mode shared REFUSED - [Device] Ex=0 forbids the shared-surface path on this run; staying on %s",
                          d2vr::capture::mode_name());
                return true;
            }
            d2vr::capture::set_mode(m);   // logs the refusal itself
            return true;
        }
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "sharedwait") && on_off(m, &b)) { d2vr::capture::set_shared_wait(b); return true; }
        if (!strcmp(args, "reinit")) { d2vr::capture::request_reinit(); return true; }
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "bbox")) {
            if (!strcmp(m, "off")) { d2vr::capture::set_bbox_ms(0); return true; }
            unsigned ms = 0;
            if (sscanf(m, "%u", &ms) == 1) { d2vr::capture::set_bbox_ms(ms); return true; }
            D2VR_WARN("capture: bbox wants off or an interval in ms (capture bbox off|<ms>) - got '%s'", m);
            return true;
        }
        const d2vr::capture::Cost c = d2vr::capture::cost();
        D2VR_INFO("capture: mode=%s probe=%s cost/present rtd=%u lock=%u copy=%u upload=%u blit=%u total=%u us "
                  "(%u grabs in the window) delivered serial %lu of %lu slot=%d sharedWait=%d fenceWaits=%u timeouts=%u "
                  "readWaits=%u readTimeouts=%u reinits=%u bboxEvery=%ums (%u samples) "
                  "(capture mode sync|deferred|shared|off, capture sharedwait on|off, capture bbox off|<ms>, capture reinit)",
                  d2vr::capture::mode_name(),
                  !d2vr::capture::probed() ? "not yet" : d2vr::capture::shared_available() ? "shared AVAILABLE" : "shared REFUSED",
                  c.rtdUs, c.lockUs, c.copyUs, c.uploadUs, c.blitUs, c.totalUs, c.grabsInWindow,
                  (unsigned long)d2vr::capture::delivered_serial(), (unsigned long)d2vr::capture::serial(),
                  d2vr::capture::delivered_slot(), d2vr::capture::shared_wait() ? 1 : 0,
                  d2vr::capture::fence_waits(), d2vr::capture::fence_timeouts(), d2vr::capture::read_waits(),
                  d2vr::capture::read_timeouts(), d2vr::capture::reinits(), d2vr::capture::bbox_ms(),
                  d2vr::capture::bbox_samples());
        return true;
    }
    // The mono screen's geometry, live (the headset A/B lever): `screen <distM> <widthM>`,
    // `screen headlock on|off`.
    if (!strcmp(cmd, "screen")) {
        char sub[16] = "", v[16] = "";
        bool b = false;
        float dist = 0.0f, width = 0.0f;
        if (sscanf(args, "%15s %15s", sub, v) == 2 && !strcmp(sub, "headlock") && on_off(v, &b)) {
            d2vr::vr::set_screen_head_locked(b);
            D2VR_INFO("screen: headlock %s (the quad %s)", b ? "on" : "off", b ? "follows the head" : "stays where it was in the room");
            return true;
        }
        if (sscanf(args, "%f %f", &dist, &width) == 2 && dist > 0.0f && width > 0.0f) {
            d2vr::vr::set_screen(dist, width);
            D2VR_INFO("screen: %.2f m away, %.2f m wide (the runtime clamps; `stereo status` shows the result)", dist, width);
            return true;
        }
        D2VR_INFO("screen: <distM> <widthM> | headlock on|off  ([Screen] DistanceMeters=%.2f WidthMeters=%.2f HeadLocked=%d)",
                  d2vr::config::get().screenDistanceM, d2vr::config::get().screenWidthM, d2vr::config::get().screenHeadLocked);
        return true;
    }
    if (!strcmp(cmd, "xr")) {
        D2VR_INFO("xr: runtime \"%s\" session %s live=%d running=%d everFocused=%d swapchainFmt=%lld displayPeriodNs=%lld "
                  "submits=%lu poisoned=%d d3d11=%s adapter=\"%s\"",
                  d2vr::vr::runtime_name(), d2vr::vr::session_state_name(), (int)d2vr::vr::session_live(),
                  (int)d2vr::vr::session_running(), (int)d2vr::vr::ever_focused(), (long long)d2vr::vr::swapchain_format(),
                  (long long)d2vr::vr::display_period_ns(), d2vr::frame::submits(), (int)d2vr::frame::vr_poisoned(),
                  d2vr::d3d11::created() ? "created" : "none", d2vr::d3d11::adapter_name());
        return true;
    }
    if (!strcmp(cmd, "pace")) { d2vr::vr::handle_pace_command(args); return true; }
    // `quit` reaches here on the present thread before the input lane posts WM_CLOSE:
    // the session comes down on the thread that owns every runtime call, never from
    // DLL_PROCESS_DETACH under the loader lock. Not handled: the word falls through.
    if (!strcmp(cmd, "quit")) {
        D2VR_INFO("quit: tearing the VR session down on the present thread first");
        d2vr::stereo::shutdown();
        d2vr::vr::shutdown("quit");
        return false;
    }
    if (!strcmp(cmd, "route")) { d2vr::proxy::log_route_report(); d2vr::proxy::log_module_census("route"); return true; }
    if (!strcmp(cmd, "fingerprint")) { const Fingerprint& f = fingerprint(); D2VR_INFO("fingerprint ok=%d", (int)f.ok); return true; }
    if (!strcmp(cmd, "callers")) {
        for (int i = 0; i < 3; i++) {
            if (d2vr::frame::present_caller(i)) D2VR_INFO("Present caller[%d] = 0x%08lx", i, (unsigned long)d2vr::frame::present_caller(i));
            if (d2vr::frame::endscene_caller(i)) D2VR_INFO("EndScene caller[%d] = 0x%08lx", i, (unsigned long)d2vr::frame::endscene_caller(i));
        }
        return true;
    }
    if (!strcmp(cmd, "device")) {
        const auto& d = d2vr::frame::info();
        D2VR_INFO("device: created=%d ex=%d %ux%u fmt=%d interval=0x%x windowed=%d window=%p presents=%lu (ex %lu) hz=%.1f thread=%lu",
                  (int)d.created, (int)d.ex, d.width, d.height, d.format, d.interval, (int)d.windowed, (void*)d.window,
                  d2vr::frame::presents(), d2vr::frame::present_ex_calls(), d2vr::frame::present_hz(), (unsigned long)d2vr::frame::present_thread());
        return true;
    }
    return false;
}

void status_provider(d2vr::status::Writer& w)
{
    w.kv("version", D2VR_VERSION);
    w.kv("build", D2VR_BUILD_ID);
    w.kv("config", D2VR_BUILD_CONFIG);
    w.kv("optimised", D2VR_BUILD_OPTIMISED != 0);
    w.kv("pid", (unsigned long)GetCurrentProcessId());
    w.kv("uptimeMs", (unsigned long)GetTickCount());
    w.kv("disabled", d2vr::proxy::g_disabled);
    w.kv("log", d2vr::log::path());
    w.kv("dataDir", d2vr::paths::data_dir());
    w.kv("ini", d2vr::config::ini_path());
    w.obj("route");
        w.kv("module", d2vr::paths::module_path());
        w.kv("exe", d2vr::paths::exe_path());
    w.end_obj();
    const Fingerprint& f = fingerprint();
    w.obj("fingerprint");
        w.kv("ok", f.ok);
        w.kv_hex("timeDateStamp", f.timeDateStamp);
        w.kv_hex("sizeOfImage", f.sizeOfImage);
        w.kv("fileSize", (double)f.fileSize);
    w.end_obj();
    const auto& d = d2vr::frame::info();
    w.obj("device");
        w.kv("created", d.created);
        w.kv("ex", d.ex);
        w.kv("width", (int)d.width);
        w.kv("height", (int)d.height);
        w.kv("format", d.format);
        w.kv("backBuffers", d.backBuffers);
        w.kv("multisample", d.multisample);
        w.kv("swapEffect", d.swapEffect);
        w.kv_hex("interval", (unsigned long)d.interval);
        w.kv("windowed", d.windowed);
        w.kv_hex("window", (unsigned long)(uintptr_t)d.window);
    w.end_obj();
    w.obj("frame");
        w.kv("presents", d2vr::frame::presents());
        w.kv("presentEx", d2vr::frame::present_ex_calls());
        w.kv("endScenes", d2vr::frame::end_scenes());
        w.kv("resets", d2vr::frame::resets());
        w.kv("hz", d2vr::frame::present_hz());
        w.kv("presentThread", (unsigned long)d2vr::frame::present_thread());
        w.arr("presentCallers");
            for (int i = 0; i < 3; i++) if (d2vr::frame::present_caller(i)) { char t[16]; snprintf(t, sizeof(t), "0x%08lx", (unsigned long)d2vr::frame::present_caller(i)); w.item(t); }
        w.end_arr();
        w.arr("endSceneCallers");
            for (int i = 0; i < 3; i++) if (d2vr::frame::endscene_caller(i)) { char t[16]; snprintf(t, sizeof(t), "0x%08lx", (unsigned long)d2vr::frame::endscene_caller(i)); w.item(t); }
        w.end_arr();
    w.end_obj();
    w.obj("canaries");
        for (int i = 0; i < canaries::count(); i++) {
            const auto& s = canaries::state(i);
            w.obj(s.name);
                w.kv("wanted", s.wanted);
                w.kv("on", s.on);
                w.kv_hex("at", (unsigned long)s.at);
                w.kv("hits", s.hits);
                w.kv("hz", s.hz);
                w.kv("reverts", s.reverts);
                w.kv("check", s.lastCheck);
            w.end_obj();
        }
    w.end_obj();
    w.obj("lua");
        lua::status(w);
    w.end_obj();
    w.obj("xr");
        w.kv("runtime", d2vr::vr::runtime_name());
        w.kv("session", d2vr::vr::session_state_name());
        w.kv("live", d2vr::vr::session_live());
        w.kv("everFocused", d2vr::vr::ever_focused());
        w.kv("submits", d2vr::frame::submits());
        w.kv("poisoned", d2vr::frame::vr_poisoned());
        w.kv("d3d11", d2vr::d3d11::created());
        w.kv("adapter", d2vr::d3d11::adapter_name());
    w.end_obj();
    w.obj("stereo");   // the seam writes flat keys (method, framesOut, ...): give them their own object
        d2vr::stereo::status(w);
    w.end_obj();
    {
        const d2vr::capture::Cost c = d2vr::capture::cost();
        w.obj("capture");
            w.kv("mode", d2vr::capture::mode_name());
            w.kv("probed", d2vr::capture::probed());
            w.kv("sharedAvailable", d2vr::capture::shared_available());
            w.kv("grabs", (unsigned long)d2vr::capture::grabs());
            w.kv("width", (int)d2vr::capture::width());
            w.kv("height", (int)d2vr::capture::height());
            w.kv("costTotalUs", (unsigned long)c.totalUs);
            w.kv("costRtdUs", (unsigned long)c.rtdUs);
            w.kv("costLockUs", (unsigned long)c.lockUs);
            w.kv("costUploadUs", (unsigned long)c.uploadUs);
            w.kv("costBlitUs", (unsigned long)c.blitUs);
            w.kv("grabsInWindow", (unsigned long)c.grabsInWindow);
        w.end_obj();
    }
    w.obj("counters");
        w.kv("commands", (unsigned long)d2vr::command::lines());
        w.kv("commandBatches", (unsigned long)d2vr::command::sequence());
        w.kv("statusWrites", d2vr::status::writes());
        w.kv("shots", d2vr::shot::count());
        w.kv("inputActions", d2vr::input::actions_done());
        w.kv("ticks", g_ticks);
    w.end_obj();
    w.kv("lastShot", d2vr::shot::last_path());
    w.kv_hex("window", (unsigned long)(uintptr_t)d2vr::input::game_window());
    w.kv("teardown", d2vr::crash::teardown_seen());
}

void present_tick(IDirect3DDevice9*, double nowMs)
{
    g_ticks++;
    if (!g_canariesArmed) {
        // Nothing resolves at init: the canaries install from the first present,
        // once the game is up, never from DllMain or the create call.
        g_canariesArmed = true;
        if (d2vr::diag::skip("canaries")) D2VR_WARN("canaries: SKIPPED by D2VR_SKIP");
        else canaries::init_from_config();
        // The Lua lane arms the same way: from the first present, never earlier.
        if (d2vr::diag::skip("lua")) D2VR_WARN("lua: SKIPPED by D2VR_SKIP");
        else lua::init_from_config();
    }
    canaries::tick(nowMs);
    lua::tick(nowMs);
    D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Info, 30000,
        "heartbeat: %lu presents at %.1f Hz, %lu commands, %lu status writes, %lu shots, xr=%s/%s stereo=%s submits=%lu",
        d2vr::frame::presents(), d2vr::frame::present_hz(), d2vr::command::lines(), d2vr::status::writes(), d2vr::shot::count(),
        d2vr::vr::runtime_name(), d2vr::vr::session_state_name(), d2vr::stereo::active_name(), d2vr::frame::submits());
}

} // namespace

bool code_hooks_allowed() { return fingerprint().ok && !d2vr::proxy::g_disabled; }

void init()
{
    if (g_inited) return;
    g_inited = true;
    fingerprint();
    log_ex_flag();
    d2vr::command::set_game_handler(game_command);
    d2vr::status::set_provider(status_provider);
    d2vr::frame::set_tick(present_tick);
    d2vr::crash::set_context("flat (no VR runtime yet)");
    D2VR_INFO("game layer ready: seam handler, status provider, present tick registered; canaries arm on the first present");
}

} // namespace d2vr::game
