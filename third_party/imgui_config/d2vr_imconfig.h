// d2vr_imconfig.h - the mod's ImGui user config (IMGUI_USER_CONFIG, applied to
// every ImGui translation unit by third_party/CMakeLists.txt).
//
// IM_ASSERT is the stock assert() upstream: it vanishes under NDEBUG (the
// RelWithDebInfo build the player runs) and pops a dialog inside the game in
// Debug. Inside a game process neither is acceptable: a failed ImGui invariant
// is logged once (with the expression and the site) and execution continues.
#pragma once

#ifdef __cplusplus
extern "C" void d2vr_imgui_assert_failed(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) ((void)((!!(_EXPR)) || (d2vr_imgui_assert_failed(#_EXPR, __FILE__, __LINE__), 0)))
#endif
