// core/ui/ovl_ui_stub.cpp - the five themed ImGui wrappers the adopted runtime layer's
// debug panel calls. Dishonored's ovl_ui.cpp is its parchment theme and fonts; there
// is no F10 overlay in this mod yet (ROADMAP S8), so these are the bare ImGui calls.
// The panel is never drawn until an overlay exists, but the layer must link.
#include <windows.h>
#include <string.h>
#include "imgui.h"
#include "core/ui/ovl_ui.h"

namespace d2vr::ovl {
// The view tier. With no panel there is nothing to hide, so every section shows.
static int g_level = Debug;
int level() { return g_level; }
void set_level(int tier) { g_level = tier < Basic ? Basic : tier > Debug ? Debug : tier; }
const char* level_name(int tier) { return tier <= Basic ? "basic" : tier == Advanced ? "advanced" : "debug"; }
int parse_level(const char* s, int fallback)
{
    if (!s) return fallback;
    if (!_stricmp(s, "basic")) return Basic;
    if (!_stricmp(s, "advanced")) return Advanced;
    if (!_stricmp(s, "debug")) return Debug;
    return fallback;
}
bool section(const char* name, int tier, const char* tipText, bool defaultOpen)
{
    if (!show(tier)) return false;
    const bool open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    tip(tipText);
    return open;
}
bool checkbox(const char* label, bool* value) { return ImGui::Checkbox(label, value); }
bool button(const char* label, const ImVec2& size) { return ImGui::Button(label, size); }
bool slider_float(const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags)
{ return ImGui::SliderFloat(label, value, min, max, format, flags); }
bool slider_int(const char* label, int* value, int min, int max, const char* format, ImGuiSliderFlags flags)
{ return ImGui::SliderInt(label, value, min, max, format, flags); }
void tip(const char* text)
{
    if (text && text[0] && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
}
} // namespace d2vr::ovl
