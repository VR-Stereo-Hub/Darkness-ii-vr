// core/ui/overlay.cpp - the F10 panel, the simple form (Ref VR-275). See overlay.h.
//
// The panel is a window the player reads in the headset, so it is drawn into
// the eye texture the stereo method produced, after the game image, from the
// seam's overlay draw hook on the present thread. Only the DX11 backend of
// ImGui is used: the display is the eye texture (io.DisplaySize = w x h), time
// comes from the mod's clock, and the mouse is the desktop cursor scaled into
// the texture through the event calls (TRAPS s5: never a field write). The
// game keeps reading the same mouse while the panel is open; the controller
// pointer is VR-275's work.
#define D2VR_CAT ::d2vr::log::Cat::overlay
#include <windows.h>
#include <d3d11.h>
#include <string.h>
#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "core/ui/overlay.h"
#include "core/config/config.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/stereo.h"
#include "core/gfx/capture.h"
#include "core/gfx/d3d11_device.h"
#include "core/vr/openxr_runtime.h"
#include "core/input/inject.h"
#include "core/util/clock.h"
#include "core/util/log.h"

// IM_ASSERT (third_party/imgui_config/d2vr_imconfig.h): a failed ImGui
// invariant is logged, never a dialog inside the game and never silent.
extern "C" void d2vr_imgui_assert_failed(const char* expr, const char* file, int line)
{
    D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Error, 8, "imgui: ASSERT failed: %s (%s:%d) - continuing", expr, file, line);
}

namespace d2vr::overlay {
namespace {

bool g_registered = false;
bool g_enabled = true;          // [Overlay] Enabled: the F10 key is armed
bool g_visible = false;
bool g_inited = false;
bool g_initFailed = false;
float g_uiScale = 0.0f;         // [Overlay] UiScale; 0 = from the eye height
float g_scaleUsed = 1.0f;
double g_lastFrameMs = 0.0;
bool g_f10Was = false, g_lbWas = false, g_rbWas = false;
uint32_t g_draws = 0, g_toggles = 0;
// Mirrors of the levers that have no getter, seeded from the ini.
float g_screenDist = 1.75f, g_screenWidth = 2.4f;
bool g_headLocked = true;
GameSectionFn g_gameSection = nullptr;

const char* kStereoNames[3] = { "mono", "aer", "reentry" };
const char* kCaptureNames[4] = { "sync", "deferred", "shared", "off" };

bool ensure_init()
{
    if (g_inited) return true;
    if (g_initFailed) return false;
    ID3D11DeviceContext* ctx = nullptr;
    ID3D11Device* dev = d2vr::d3d11::device(&ctx);
    if (!dev || !ctx) {
        D2VR_LOG_ONCE(D2VR_CAT, ::d2vr::log::Level::Warn, "overlay: no D3D11 device yet - the panel waits");
        return false;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;               // nothing is written into the game directory
    io.LogFilename = nullptr;
    io.ConfigInputTrickleEventQueue = false; // one synthetic event per frame; no deferral
    io.MouseDrawCursor = true;               // the desktop cursor is invisible in the headset
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.6f);   // readable at headset distance
    if (!ImGui_ImplDX11_Init(dev, ctx)) {
        g_initFailed = true;
        ImGui::DestroyContext();
        D2VR_ERROR("overlay: ImGui_ImplDX11_Init failed - no panel this session");
        return false;
    }
    g_inited = true;
    D2VR_INFO("overlay: initialised (ImGui %s, DX11 backend only); F10 or `overlay toggle` shows the panel", IMGUI_VERSION);
    return true;
}

void feed_input(uint32_t w, uint32_t h)
{
    ImGuiIO& io = ImGui::GetIO();
    HWND wnd = d2vr::input::game_window();
    POINT p{}; RECT rc{};
    if (wnd && GetCursorPos(&p) && ScreenToClient(wnd, &p) && GetClientRect(wnd, &rc) && rc.right > 0 && rc.bottom > 0)
        io.AddMousePosEvent((float)p.x * (float)w / (float)rc.right, (float)p.y * (float)h / (float)rc.bottom);
    const bool lb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (lb != g_lbWas) { io.AddMouseButtonEvent(0, lb); g_lbWas = lb; }
    if (rb != g_rbWas) { io.AddMouseButtonEvent(1, rb); g_rbWas = rb; }
}

void section_stereo()
{
    if (!ImGui::CollapsingHeader("Stereo method", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const char* active = d2vr::stereo::active_name();
    const char* wanted = d2vr::stereo::wanted_name();
    for (int i = 0; i < 3; i++) {
        const bool sel = !strcmp(active, kStereoNames[i]);
        if (ImGui::RadioButton(kStereoNames[i], sel) && !sel) d2vr::stereo::choose(kStereoNames[i]);   // a refusal logs and leaves the method
        if (i < 2) ImGui::SameLine();
    }
    if (strcmp(active, wanted)) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "'%s' refused (a design stub): '%s' keeps running", wanted, active);
    else ImGui::Text("active: %s%s", active, d2vr::stereo::armed() ? "" : " (parked: the game runs flat)");
    bool armed = d2vr::stereo::armed();
    if (ImGui::Checkbox("armed", &armed)) d2vr::stereo::set_armed(armed);
    ImGui::SameLine();
    ImGui::Text("frames out %lu", (unsigned long)d2vr::stereo::frames_out());
}

void section_capture()
{
    if (!ImGui::CollapsingHeader("Capture (D3D9 -> D3D11)", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const char* mode = d2vr::capture::mode_name();
    for (int i = 0; i < 4; i++) {
        const bool sel = !strcmp(mode, kCaptureNames[i]);
        if (ImGui::RadioButton(kCaptureNames[i], sel) && !sel) {
            if (!strcmp(kCaptureNames[i], "shared") && !d2vr::config::get().deviceEx)
                D2VR_WARN("overlay: capture mode shared REFUSED - [Device] Ex=0 forbids the shared-surface path this run");
            else d2vr::capture::set_mode(kCaptureNames[i]);
        }
        if (i < 3) ImGui::SameLine();
    }
    const d2vr::capture::Cost c = d2vr::capture::cost();
    ImGui::Text("cost/present %u us (rtd %u lock %u upload %u blit %u) | probe %s | %lu grabs",
                c.totalUs, c.rtdUs, c.lockUs, c.uploadUs, c.blitUs,
                !d2vr::capture::probed() ? "not yet" : d2vr::capture::shared_available() ? "shared AVAILABLE" : "shared REFUSED",
                (unsigned long)d2vr::capture::grabs());
}

void section_screen()
{
    if (!ImGui::CollapsingHeader("Mono screen", ImGuiTreeNodeFlags_DefaultOpen)) return;
    bool changed = false;
    changed |= ImGui::SliderFloat("distance (m)", &g_screenDist, 0.5f, 5.0f, "%.2f");
    changed |= ImGui::SliderFloat("width (m)", &g_screenWidth, 0.5f, 6.0f, "%.2f");
    if (changed) d2vr::vr::set_screen(g_screenDist, g_screenWidth);
    if (ImGui::Checkbox("head-locked (off = the screen stays in the room)", &g_headLocked)) d2vr::vr::set_screen_head_locked(g_headLocked);
}

void section_xr()
{
    if (!ImGui::CollapsingHeader("OpenXR", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::Text("runtime '%s' | session %s | submits %lu | %.1f presents/s | d3d11 %s",
                d2vr::vr::runtime_name(), d2vr::vr::session_state_name(), d2vr::frame::submits(),
                d2vr::frame::present_hz(), d2vr::d3d11::created() ? d2vr::d3d11::adapter_name() : "none");
    if (ImGui::TreeNode("runtime layer (debug)")) {
        d2vr::vr::draw_debug_ui();
        ImGui::TreePop();
    }
}

void panel(uint32_t w, uint32_t h)
{
    ImGuiIO& io = ImGui::GetIO();
    const float scale = g_uiScale > 0.0f ? g_uiScale : (h > 0 ? (float)h / 900.0f : 1.0f);
    g_scaleUsed = scale < 0.8f ? 0.8f : scale > 3.0f ? 3.0f : scale;
    ImGui::GetStyle().FontScaleMain = g_scaleUsed;
    ImGui::SetNextWindowPos(ImVec2(w * 0.28f, h * 0.10f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w * 0.44f, h * 0.78f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Darkness II VR", &g_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextDisabled("F10 closes | the desktop mouse points (the game reads it too) | %ux%u", w, h);
    section_stereo();
    section_capture();
    section_screen();
    if (g_gameSection) g_gameSection();
    section_xr();
    ImGui::End();
    (void)io;
}

} // namespace

void set_game_section(GameSectionFn fn) { g_gameSection = fn; }

void init()
{
    if (g_registered) return;
    g_registered = true;
    const auto& c = d2vr::config::get();
    g_enabled = c.overlayEnabled != 0;
    g_uiScale = c.overlayUiScale;
    g_screenDist = c.screenDistanceM;
    g_screenWidth = c.screenWidthM;
    g_headLocked = c.screenHeadLocked != 0;
    d2vr::stereo::set_overlay_draw(draw);
    D2VR_INFO("overlay: registered with the stereo seam ([Overlay] Enabled=%d UiScale=%.2f); hidden until F10 or `overlay on`",
              (int)g_enabled, g_uiScale);
}

bool visible() { return g_visible; }

void set_visible(bool on, const char* why)
{
    if (on == g_visible) return;
    g_visible = on;
    g_toggles++;
    D2VR_INFO("overlay: %s (%s)", on ? "OPEN - the panel draws into the eye texture from the next present" : "closed", why ? why : "?");
}

void tick()
{
    if (!g_enabled) return;
    const bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (f10 && !g_f10Was) set_visible(!g_visible, "F10");
    g_f10Was = f10;
}

void draw(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h)
{
    if (!g_visible || !ctx || !rtv || w == 0 || h == 0) return;
    if (!ensure_init()) return;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    const double now = d2vr::clock::now_ms();
    float dt = g_lastFrameMs > 0.0 ? (float)((now - g_lastFrameMs) / 1000.0) : 1.0f / 60.0f;
    if (dt < 0.0001f) dt = 0.0001f;
    if (dt > 0.1f) dt = 0.1f;
    io.DeltaTime = dt;
    g_lastFrameMs = now;
    feed_input(w, h);
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    panel(w, h);
    ImGui::Render();
    // The backend restores every state it touches except the render targets:
    // bind the eye texture ourselves (the hand pass may have bound a depth view).
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    ctx->RSSetViewports(1, &vp);
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_draws++;
    D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Info, 1, "overlay: first draw into a %ux%u eye texture (text scale %.2f)", w, h, g_scaleUsed);
    if (!g_visible) g_lastFrameMs = 0.0;   // the window's close box: the next open starts with a fresh delta
}

void shutdown()
{
    if (!g_inited) return;
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    g_inited = false;
    g_visible = false;
    D2VR_INFO("overlay: shut down (%lu draws, %lu toggles)", (unsigned long)g_draws, (unsigned long)g_toggles);
}

bool command(const char* cmd, const char* args)
{
    if (strcmp(cmd, "overlay")) return false;
    if (!args[0] || !strcmp(args, "status")) {
        D2VR_INFO("overlay: %s, key %s, inited=%d, draws=%lu, toggles=%lu, text scale %.2f (overlay on|off|toggle|status)",
                  g_visible ? "VISIBLE" : "hidden", g_enabled ? "armed (F10)" : "disarmed ([Overlay] Enabled=0)",
                  (int)g_inited, (unsigned long)g_draws, (unsigned long)g_toggles, g_scaleUsed);
        return true;
    }
    if (!strcmp(args, "on")) { set_visible(true, "seam"); return true; }
    if (!strcmp(args, "off")) { set_visible(false, "seam"); return true; }
    if (!strcmp(args, "toggle")) { set_visible(!g_visible, "seam"); return true; }
    D2VR_WARN("overlay: usage - overlay on|off|toggle|status");
    return true;
}

void status(d2vr::status::Writer& w)
{
    w.kv("visible", g_visible);
    w.kv("enabled", g_enabled);
    w.kv("inited", g_inited);
    w.kv("draws", (unsigned long)g_draws);
    w.kv("toggles", (unsigned long)g_toggles);
    w.kv("scale", (double)g_scaleUsed);
}

} // namespace d2vr::overlay
