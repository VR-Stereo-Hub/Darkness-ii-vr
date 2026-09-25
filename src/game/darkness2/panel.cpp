// game/darkness2/panel.cpp - the game side of the F10 panel: the Lua lane and
// the canaries as ImGui widgets, handed to core/ui/overlay as its game section.
// ImGui only from the overlay's draw callback: this runs inside it.
#define D2VR_CAT ::d2vr::log::Cat::game
#include <windows.h>
#include <stdio.h>
#include "imgui.h"
#include "game/darkness2/panel.h"
#include "game/darkness2/canaries.h"
#include "game/darkness2/lua/lane.h"
#include "core/ui/overlay.h"
#include "core/util/log.h"

namespace d2vr::game::panel {
namespace {

float g_fovDeg = 90.0f;

void section_lua()
{
    if (!ImGui::CollapsingHeader("Lua lane (R2)", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const lua::Counters c = lua::counters();
    bool on = lua::on();
    if (ImGui::Checkbox("lane on (three wraps)", &on)) lua::set_on(on);
    ImGui::SameLine();
    ImGui::Text("%s", c.poisoned ? "POISONED" : on ? "LIVE" : "off");
    ImGui::Text("thread %lu | Resume hits %lu (%.1f/s) | foreign %lu", c.thread, c.resumeHits, c.resumeHz, c.foreignResumes);
    ImGui::Text("l_G match %lu mismatch %lu | engine pcalls %lu (must read 0) own %lu", c.lgMatch, c.lgMismatch, c.pcallEngine, c.pcallOwn);
    ImGui::Text("chunks %lu (failed %lu), slot %s", c.chunksDone, c.chunksFailed, c.slot);
    ImGui::TextWrapped("last: %s", lua::last_result()[0] ? lua::last_result() : "-");
    ImGui::SliderFloat("base FOV (deg)", &g_fovDeg, 30.0f, 150.0f, "%.0f");
    if (ImGui::Button("Apply FOV")) lua::fov(g_fovDeg);
    ImGui::SameLine();
    if (ImGui::Button("Reset FOV (0)")) lua::fov(0.0f);
}

void section_canaries()
{
    if (!ImGui::CollapsingHeader("Canaries (R1)")) return;
    for (int i = 0; i < canaries::count(); i++) {
        const auto& s = canaries::state(i);
        ImGui::Text("%-8s 0x%08lx %s hits %lu (%.1f/s) reverts %d", s.name, (unsigned long)s.at,
                    s.on ? "LIVE" : s.wanted ? "REFUSED" : "off", s.hits, s.hz, s.reverts);
    }
}

void game_section()
{
    section_lua();
    section_canaries();
}

} // namespace

void install()
{
    d2vr::overlay::set_game_section(game_section);
}

} // namespace d2vr::game::panel
