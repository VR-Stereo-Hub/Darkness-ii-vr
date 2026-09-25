// game/darkness2/commands.cpp - the game side of the framework: init, the
// seam's game words, the status.json provider and the present tick.
#define D2VR_CAT ::d2vr::log::Cat::game
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "game/darkness2/game.h"
#include "game/darkness2/patterns.h"
#include "game/darkness2/canaries.h"
#include "proxy/proxy.h"
#include "core/config/config.h"
#include "core/framework/command.h"
#include "core/framework/status.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/shot.h"
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

bool game_command(const char* cmd, const char* args)
{
    if (canaries::command(cmd, args)) return true;
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
    }
    canaries::tick(nowMs);
    D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Info, 30000,
        "heartbeat: %lu presents at %.1f Hz, %lu commands, %lu status writes, %lu shots",
        d2vr::frame::presents(), d2vr::frame::present_hz(), d2vr::command::lines(), d2vr::status::writes(), d2vr::shot::count());
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
