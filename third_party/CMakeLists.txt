if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR-SDK/include/openxr/openxr.h")
    message(FATAL_ERROR
        "Submodules missing. Run:\n  git submodule update --init --recursive")
endif()

# --- Dear ImGui (MIT) - the F10 overlay, once it exists (M4). The core is linked
# into d3d9.dll now (the adopted runtime layer's debug panel calls ImGui); the
# Win32 + DX11 backends are a SEPARATE target because imgui_impl_win32 pulls in
# imm32 by #pragma comment(lib), and the proxy adds no static import the game
# does not already have (src/CMakeLists.txt). Gamepad input disabled because the
# game's XInput slots will be ours.
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC imgui imgui/backends)
# imgui.cpp's default Win32 IME hooks carry `#pragma comment(lib, "imm32")`; the
# overlay has no text input that needs an IME, and imm32 is not an import of the
# game exe, so the hooks are off and the proxy's import list stays the exe's.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)
# The mod's ImGui config (imgui_config/d2vr_imconfig.h): IM_ASSERT logs and
# continues instead of assert(), which vanishes under NDEBUG and pops a dialog
# inside the game in Debug. PUBLIC so every ImGui translation unit agrees.
target_include_directories(imgui PUBLIC imgui_config)
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=<d2vr_imconfig.h>")
# The DX11 backend ALONE for the F10 panel: the Win32 backend (imgui_backends
# below) pragmas gdi32 and dwmapi, imports the proxy must not add. The DX11
# backend's own `#pragma comment(lib, "d3dcompiler")` is answered by the
# proxy's core/gfx/d3dcompile_fwd.cpp plus /NODEFAULTLIB:d3dcompiler.lib.
add_library(imgui_dx11 STATIC imgui/backends/imgui_impl_dx11.cpp)
target_link_libraries(imgui_dx11 PUBLIC imgui)
add_library(imgui_backends STATIC
    imgui/backends/imgui_impl_win32.cpp
    imgui/backends/imgui_impl_dx11.cpp
)
target_link_libraries(imgui_backends PUBLIC imgui)
target_compile_definitions(imgui_backends PUBLIC IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)

# --- OpenXR SDK (Apache-2.0) --------------------------------------------------
# The static loader will be linked straight into d3d9.dll by the runtime layer
# (VR-241): no openxr_loader.dll ships, and the loader's own discovery
# (XR_RUNTIME_JSON first, then the 32-bit ActiveRuntime key) selects the
# simulator, the SteamVR shim or VDXR. The same lib serves xr_hello32 now.
add_library(openxr_headers INTERFACE)
target_include_directories(openxr_headers INTERFACE OpenXR-SDK/include)

option(D2VR_WITH_OPENXR "Build the static OpenXR loader" ON)
if(D2VR_WITH_OPENXR)
    set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
    add_subdirectory(OpenXR-SDK)
    # Upstream forces the dynamic CRT for static-lib builds; match our /MT.
    set_target_properties(openxr_loader PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
