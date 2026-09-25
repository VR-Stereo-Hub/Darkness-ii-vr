// game/darkness2/camera.cpp - the camera eyetest (see camera.h).
#define D2VR_CAT ::d2vr::log::Cat::camera
#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "game/darkness2/camera.h"
#include "game/darkness2/lua/lane.h"
#include "core/config/config.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/framework/vs_const.h"
#include "core/util/log.h"

namespace d2vr::game::camera {
namespace {

constexpr float kPi = 3.14159265f;
constexpr int kBaselineFrames = 45;
constexpr int kSettleFrames = 30;
constexpr int kJudgeFrames = 120;
constexpr int kRevertFrames = 60;
constexpr int kWriteWaitCap = 600;
constexpr int kHypCount = 3;
const char* kHypNames[kHypCount] = { "vert", "horiz", "h169" };

float deg2rad(float d) { return d * kPi / 180.0f; }
float rad2deg(float r) { return r * 180.0f / kPi; }

enum Phase { Idle, Baseline, Write, Settle, Judge, RevertWrite, RevertSettle, RevertJudge };

struct Eyetest {
    bool  active = false;
    bool  nowrite = false;
    Phase phase = Idle;
    int   frames = 0;          // presents in the current phase
    int   askIdx = 0;
    float asks[2] = { 60.0f, 80.0f };
    uint32_t startPresent = 0;
    // baseline
    float bSum = 0.0f, bSumSq = 0.0f, aSum = 0.0f;
    int   bN = 0, bInvalid = 0;
    float Vb = 0.0f, sigma = 0.0f, aspectB = 0.0f;
    // the predictions and bands for the current ask
    float Vp[kHypCount] = {}, band[kHypCount] = {}, movedThr = 0.0f;
    // judge
    int   inBand[kHypCount] = {}, moved = 0, judged = 0, invalid = 0;
    float vSum = 0.0f, vMin = 0.0f, vMax = 0.0f;
    uint32_t chunksBefore = 0;
    int   waited = 0;
    char  chunkResult[160] = "";
    // revert
    int   revOk = 0, revN = 0;
    // results
    char  verdict[2][200] = { "", "" };
    int   verdictHyp[2] = { -1, -1 };   // -1 none, 0..2 the honoured hypothesis, -2 discarded, -3 unpredicted, -4 invalid/not written
};
Eyetest g_et;
bool g_configApplied = false;

void enter(Phase p)
{
    g_et.phase = p;
    g_et.frames = 0;
}

void predict(float ask)
{
    const float a = g_et.aspectB > 0.0f ? g_et.aspectB : 1.0f;
    g_et.Vp[0] = ask;                                                    // the argument is the vertical FOV
    g_et.Vp[1] = rad2deg(2.0f * atanf(tanf(deg2rad(ask) * 0.5f) / a));  // horizontal at the render aspect
    g_et.Vp[2] = rad2deg(2.0f * atanf(tanf(deg2rad(ask) * 0.5f) * 9.0f / 16.0f));   // horizontal at 16:9
    for (int h = 0; h < kHypCount; h++) {
        const float d = fabsf(g_et.Vp[h] - g_et.Vb);
        g_et.band[h] = 0.10f * d > 0.5f ? 0.10f * d : 0.5f;
    }
    g_et.movedThr = 3.0f * g_et.sigma > 0.25f ? 3.0f * g_et.sigma : 0.25f;
    D2VR_INFO("camera/eyetest: ask %d = SetBaseFovOverride(%.1f): baseline V=%.2f (sigma %.3f, aspect %.4f) -> predictions vert %.2f | horiz %.2f | h169 %.2f "
              "(bands %.2f/%.2f/%.2f deg, moved > %.2f deg); %d presents judged after the chunk RAN and %d settle presents",
              g_et.askIdx + 1, ask, g_et.Vb, g_et.sigma, g_et.aspectB, g_et.Vp[0], g_et.Vp[1], g_et.Vp[2],
              g_et.band[0], g_et.band[1], g_et.band[2], g_et.movedThr, kJudgeFrames, kSettleFrames);
}

void set_verdict(int hyp, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_et.verdict[g_et.askIdx], sizeof(g_et.verdict[0]), fmt, ap);
    va_end(ap);
    g_et.verdictHyp[g_et.askIdx] = hyp;
    D2VR_INFO("camera/eyetest: ask %d (%.1f): %s", g_et.askIdx + 1, g_et.asks[g_et.askIdx], g_et.verdict[g_et.askIdx]);
}

void finish()
{
    const int h0 = g_et.verdictHyp[0], h1 = g_et.verdictHyp[1];
    const char* overall;
    char detail[64] = "";
    if (h0 >= 0 && h0 == h1) { overall = "HONOURED"; snprintf(detail, sizeof(detail), " as %s (both asks agree)", kHypNames[h0]); }
    else if (h0 == -2 && h1 == -2) overall = "DISCARDED";
    else if (h0 >= 0 && h1 >= 0) { overall = "HONOURED-INCONSISTENT"; snprintf(detail, sizeof(detail), " (%s then %s)", kHypNames[h0], kHypNames[h1]); }
    else overall = "MIXED";
    D2VR_INFO("camera/eyetest: DONE candidate lua (SetBaseFovOverride through the Lua lane)%s: %s%s | ask 1: %s | ask 2: %s",
              g_et.nowrite ? " [nowrite: the negative control, expected DISCARDED]" : "", overall, detail, g_et.verdict[0], g_et.verdict[1]);
    if (!strcmp(overall, "HONOURED") && !g_et.nowrite)
        D2VR_INFO("camera/eyetest: the renderer honours SetBaseFovOverride as a %s angle: the camera seam's FOV lever is the Lua lane's write (S1 sets it from the headset's projection)",
                  kHypNames[h0]);
    g_et.active = false;
    g_et.phase = Idle;
}

void next_ask_or_finish()
{
    if (g_et.askIdx + 1 >= 2) { finish(); return; }
    g_et.askIdx++;
    g_et.bSum = g_et.bSumSq = g_et.aSum = 0.0f; g_et.bN = g_et.bInvalid = 0;
    enter(Baseline);
}

void queue_write(float deg, Phase next, Phase onRefuse)
{
    g_et.chunksBefore = lua::chunks_done();
    g_et.waited = 0;
    if (g_et.nowrite) {
        D2VR_INFO("camera/eyetest: nowrite - SetBaseFovOverride(%.1f) NOT queued; the schedule runs unchanged", deg);
        enter(next);
        return;
    }
    if (!lua::fov(deg)) {
        set_verdict(-4, "NOT WRITTEN: the Lua lane refused the chunk (lane off, busy or poisoned; see the lua lines)");
        enter(onRefuse);
        return;
    }
    D2VR_INFO("camera/eyetest: SetBaseFovOverride(%.1f) queued on the Lua lane; waiting for the chunk to RUN", deg);
}

// True once the queued chunk ran (or nowrite); false while waiting; sets a verdict on timeout.
bool wait_chunk(Phase onFail)
{
    if (g_et.nowrite) return true;
    if (lua::chunks_done() > g_et.chunksBefore) {
        snprintf(g_et.chunkResult, sizeof(g_et.chunkResult), "%s", lua::last_result());
        if (strncmp(g_et.chunkResult, "SetBaseFovOverride", 18) != 0) {
            set_verdict(-4, "NOT WRITTEN: the chunk ran but did not reach the controller -> '%s'", g_et.chunkResult);
            enter(onFail);
            return false;
        }
        D2VR_INFO("camera/eyetest: the chunk RAN -> %s; settling %d presents", g_et.chunkResult, kSettleFrames);
        return true;
    }
    if (++g_et.waited >= kWriteWaitCap) {
        set_verdict(-4, "NOT WRITTEN: the chunk did not run within %d presents (lane slot '%s')", kWriteWaitCap, lua::last_result());
        enter(onFail);
        return false;
    }
    return false;
}

void judge_verdict()
{
    const int n = g_et.judged;
    const float mean = n ? g_et.vSum / n : 0.0f;
    int honoured = -1, honouredCount = 0;
    for (int h = 0; h < kHypCount; h++) if (g_et.inBand[h] >= (kJudgeFrames * 4) / 5) { honoured = h; honouredCount++; }
    const int still = n - g_et.moved;
    if (honouredCount == 1) {
        set_verdict(honoured, "HONOURED as %s: V mean %.2f (min %.2f max %.2f) vs predicted %.2f (band %.2f) in %d/%d presents; baseline %.2f; moved in %d; invalid %d%s",
                    kHypNames[honoured], mean, g_et.vMin, g_et.vMax, g_et.Vp[honoured], g_et.band[honoured], g_et.inBand[honoured], n, g_et.Vb, g_et.moved, g_et.invalid,
                    g_et.nowrite ? " [nowrite: a HONOURED here means the instrument cannot fail]" : "");
    } else if (still >= (kJudgeFrames * 4) / 5) {
        set_verdict(-2, "DISCARDED: V stayed at %.2f (baseline %.2f, moved threshold %.2f) in %d/%d presents; in-band vert %d horiz %d h169 %d; invalid %d",
                    mean, g_et.Vb, g_et.movedThr, still, n, g_et.inBand[0], g_et.inBand[1], g_et.inBand[2], g_et.invalid);
    } else {
        const float ratio = tanf(deg2rad(mean) * 0.5f) / tanf(deg2rad(g_et.Vb) * 0.5f);
        set_verdict(-3, "MOVED-UNPREDICTED: V mean %.2f (min %.2f max %.2f) from baseline %.2f, moved in %d/%d; tan ratio %.4f vs vert %.4f horiz %.4f h169 %.4f; in-band %d/%d/%d (a clamp or a multiplier reads as a clean ratio)",
                    mean, g_et.vMin, g_et.vMax, g_et.Vb, g_et.moved, n, ratio,
                    tanf(deg2rad(g_et.Vp[0]) * 0.5f) / tanf(deg2rad(g_et.Vb) * 0.5f),
                    tanf(deg2rad(g_et.Vp[1]) * 0.5f) / tanf(deg2rad(g_et.Vb) * 0.5f),
                    tanf(deg2rad(g_et.Vp[2]) * 0.5f) / tanf(deg2rad(g_et.Vb) * 0.5f),
                    g_et.inBand[0], g_et.inBand[1], g_et.inBand[2]);
    }
}

void step(const d2vr::vsconst::Projection& p)
{
    Eyetest& e = g_et;
    e.frames++;
    switch (e.phase) {
    case Baseline:
        if (p.valid) { e.bSum += p.fovVdeg; e.bSumSq += p.fovVdeg * p.fovVdeg; e.aSum += p.aspect; e.bN++; } else e.bInvalid++;
        if (e.frames < kBaselineFrames) return;
        if (e.bN < (kBaselineFrames * 9) / 10) {
            set_verdict(-4, "INVALID: the projection watch had a perspective mode in only %d of %d baseline presents (is `camera projwatch on`, and is this gameplay?)", e.bN, kBaselineFrames);
            next_ask_or_finish();
            return;
        }
        e.Vb = e.bSum / e.bN;
        e.sigma = sqrtf(fabsf(e.bSumSq / e.bN - e.Vb * e.Vb));
        e.aspectB = e.aSum / e.bN;
        if (e.sigma >= 0.1f) {
            set_verdict(-4, "INVALID: the baseline V is not still (mean %.2f, sigma %.3f >= 0.1 deg over %d presents: stand still, no aim, no sprint)", e.Vb, e.sigma, e.bN);
            next_ask_or_finish();
            return;
        }
        predict(e.asks[e.askIdx]);
        for (int h = 0; h < kHypCount; h++) e.inBand[h] = 0;
        e.moved = e.judged = e.invalid = 0; e.vSum = 0.0f; e.vMin = 1e9f; e.vMax = -1e9f;
        queue_write(e.asks[e.askIdx], Settle, RevertWrite);
        if (e.phase == Baseline) enter(Write);
        return;
    case Write:
        if (wait_chunk(RevertWrite)) enter(Settle);
        return;
    case Settle:
        if (e.frames >= kSettleFrames) enter(Judge);
        return;
    case Judge:
        if (p.valid) {
            const float v = p.fovVdeg;
            e.judged++; e.vSum += v; if (v < e.vMin) e.vMin = v; if (v > e.vMax) e.vMax = v;
            for (int h = 0; h < kHypCount; h++) if (fabsf(v - e.Vp[h]) <= e.band[h]) e.inBand[h]++;
            if (fabsf(v - e.Vb) > e.movedThr) e.moved++;
        } else e.invalid++;
        if (e.frames < kJudgeFrames) return;
        judge_verdict();
        e.revOk = e.revN = 0;
        queue_write(0.0f, RevertSettle, RevertSettle);
        if (e.phase == Judge) enter(RevertWrite);
        return;
    case RevertWrite:
        if (e.verdictHyp[e.askIdx] == -4 && !e.chunkResult[0]) { enter(RevertSettle); return; }   // nothing was written
        if (wait_chunk(RevertSettle)) enter(RevertSettle);
        return;
    case RevertSettle:
        if (e.frames >= kSettleFrames) enter(RevertJudge);
        return;
    case RevertJudge:
        if (p.valid) { e.revN++; if (fabsf(p.fovVdeg - e.Vb) <= 0.25f) e.revOk++; }
        if (e.frames < kRevertFrames) return;
        D2VR_INFO("camera/eyetest: revert after ask %d: V back within 0.25 deg of the baseline %.2f in %d/%d presents -> %s", e.askIdx + 1, e.Vb, e.revOk, e.revN,
                  e.revOk >= (kRevertFrames * 4) / 5 ? "reverted OK" : e.nowrite ? "n/a (nowrite)" : "REVERT FAILED (the write did not come back; check `lua fov 0`)");
        if (e.verdictHyp[e.askIdx] >= 0 && e.revOk < (kRevertFrames * 4) / 5) {
            e.verdictHyp[e.askIdx] = -3;
            strncat(e.verdict[e.askIdx], " [revert FAILED: demoted]", sizeof(e.verdict[0]) - strlen(e.verdict[e.askIdx]) - 1);
        }
        e.chunkResult[0] = 0;
        next_ask_or_finish();
        return;
    default:
        return;
    }
}

bool start(bool nowrite, float askA, float askB)
{
    if (g_et.active) { D2VR_WARN("camera/eyetest: already running (ask %d, phase %d); `camera eyetest stop` first", g_et.askIdx + 1, (int)g_et.phase); return false; }
    if (!d2vr::vsconst::watch()) {
        d2vr::vsconst::set_watch(true);
        D2VR_INFO("camera/eyetest: the projection watch was off; switched ON for the test");
    }
    if (!nowrite && !lua::on()) { D2VR_WARN("camera/eyetest: REFUSED - the Lua lane is off (`lua on`); `camera eyetest nowrite` needs no lane"); return false; }
    g_et = Eyetest();
    g_et.active = true;
    g_et.nowrite = nowrite;
    g_et.asks[0] = askA; g_et.asks[1] = askB;
    g_et.startPresent = (uint32_t)d2vr::frame::presents();
    enter(Baseline);
    D2VR_INFO("camera/eyetest: START candidate lua%s: asks %.1f and %.1f; per ask %d baseline presents (INVALID unless sigma < 0.1 and the mode is present in 90%%), "
              "the write, %d settle, %d judged (HONOURED = >= 96 in exactly one hypothesis's band; DISCARDED = >= 96 unmoved), the revert judged over %d",
              nowrite ? " [nowrite]" : "", askA, askB, kBaselineFrames, kSettleFrames, kJudgeFrames, kRevertFrames);
    return true;
}

void stop(const char* why)
{
    if (!g_et.active) return;
    D2VR_INFO("camera/eyetest: stopped (%s) at ask %d phase %d", why, g_et.askIdx + 1, (int)g_et.phase);
    g_et.active = false; g_et.phase = Idle;
    if (!g_et.nowrite && lua::on()) lua::fov(0.0f);
}

bool on_off(const char* v, bool* out)
{
    if (!v) return false;
    if (!_stricmp(v, "on") || !strcmp(v, "1")) { *out = true; return true; }
    if (!_stricmp(v, "off") || !strcmp(v, "0")) { *out = false; return true; }
    return false;
}

} // namespace

bool eyetest_active() { return g_et.active; }

void init_from_config()
{
    if (g_configApplied) return;
    g_configApplied = true;
    const int want = d2vr::config::get().cameraProjWatch;
    D2VR_INFO("camera: ini asks [Camera] ProjWatch=%d (default OFF; `camera projwatch on` reads the projection live)", want);
    if (want) d2vr::vsconst::set_watch(true);
}

void present_tick(double)
{
    const uint32_t present = (uint32_t)d2vr::frame::presents();
    d2vr::vsconst::present_tick(present);
    if (!g_et.active) return;
    d2vr::vsconst::Projection p;
    d2vr::vsconst::latest(p);
    step(p);
}

bool command(const char* cmd, const char* args)
{
    if (strcmp(cmd, "camera")) return false;
    char a[16] = "", b[16] = "", c[16] = "";
    const int n = sscanf(args, "%15s %15s %15s", a, b, c);
    bool v = false;
    if (n < 1 || !strcmp(a, "status")) {
        d2vr::vsconst::log_status();
        D2VR_INFO("camera/eyetest: %s%s", g_et.active ? "RUNNING" : "idle", g_et.active ? "" : "; camera eyetest [lua|all] [nowrite] [<askA> <askB>]");
        return true;
    }
    if (!strcmp(a, "projwatch") && on_off(b, &v)) { d2vr::vsconst::set_watch(v); return true; }
    if (!strcmp(a, "names") && on_off(b, &v)) { d2vr::vsconst::set_names(v); return true; }
    if (!strcmp(a, "ctab")) {
        float deg = 0.0f;
        if (!strcmp(b, "off")) { d2vr::vsconst::set_substitute(0.0f); return true; }
        if (sscanf(b, "%f", &deg) == 1 && deg > 10.0f && deg < 170.0f) { d2vr::vsconst::set_substitute(deg); return true; }
        D2VR_WARN("camera: usage - camera ctab <vertical deg 10..170>|off");
        return true;
    }
    if (!strcmp(a, "eyetest")) {
        if (!strcmp(b, "stop")) { stop("seam"); return true; }
        bool nowrite = false;
        float askA = 60.0f, askB = 80.0f;
        // tokens after "eyetest": [lua|all] [nowrite] [askA askB] in any order
        char tok[6][16] = {};
        const int m = sscanf(args, "%*s %15s %15s %15s %15s %15s", tok[0], tok[1], tok[2], tok[3], tok[4]);
        float nums[2]; int numCount = 0;
        for (int i = 0; i < m; i++) {
            if (!strcmp(tok[i], "nowrite")) nowrite = true;
            else if (!strcmp(tok[i], "lua") || !strcmp(tok[i], "all")) {}
            else if (numCount < 2 && sscanf(tok[i], "%f", &nums[numCount]) == 1) numCount++;
            else { D2VR_WARN("camera: eyetest token '%s' not understood (lua|all, nowrite, <askA> <askB>)", tok[i]); return true; }
        }
        if (numCount == 2) { askA = nums[0]; askB = nums[1]; }
        start(nowrite, askA, askB);
        return true;
    }
    D2VR_WARN("camera: usage - camera status | projwatch on|off | names on|off | ctab <deg>|off | eyetest [lua|all] [nowrite] [<askA> <askB>] | eyetest stop");
    return true;
}

void status(d2vr::status::Writer& w)
{
    w.obj("projection");
        d2vr::vsconst::status(w);
    w.end_obj();
    w.obj("eyetest");
        w.kv("active", g_et.active);
        w.kv("nowrite", g_et.nowrite);
        w.kv("phase", (int)g_et.phase);
        w.kv("ask", g_et.askIdx + 1);
        w.kv("baselineV", (double)g_et.Vb);
        w.kv("sigma", (double)g_et.sigma);
        w.kv("verdict1", g_et.verdict[0]);
        w.kv("verdict2", g_et.verdict[1]);
    w.end_obj();
}

} // namespace d2vr::game::camera
