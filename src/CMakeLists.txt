# --- generated version header --------------------------------------------------
# Regenerated before every build of the proxy, so `darkness2vr X.Y.Z (build id)`
# in darkness2_vr.log always matches the tree it was built from.
set(D2VR_VERSION_HEADER "${CMAKE_BINARY_DIR}/generated/d2vr_version.h")
execute_process(COMMAND ${CMAKE_COMMAND}
        -DD2VR_VERSION=${PROJECT_VERSION}
        -DD2VR_SRC=${PROJECT_SOURCE_DIR}
        -DD2VR_IN=${PROJECT_SOURCE_DIR}/cmake/d2vr_version.h.in
        -DD2VR_OUT=${D2VR_VERSION_HEADER}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake)
add_custom_target(d2vr_version ALL
    BYPRODUCTS "${D2VR_VERSION_HEADER}"
    COMMAND ${CMAKE_COMMAND}
            -DD2VR_VERSION=${PROJECT_VERSION}
            -DD2VR_SRC=${PROJECT_SOURCE_DIR}
            -DD2VR_IN=${PROJECT_SOURCE_DIR}/cmake/d2vr_version.h.in
            -DD2VR_OUT=${D2VR_VERSION_HEADER}
            -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake
    COMMENT "Stamping d2vr_version.h (${PROJECT_VERSION} + git describe)"
    VERBATIM)

# --- d3d9.dll - the mod module (proxy + core + the Darkness II layer) ----------
# Dropped next to DarknessII.exe; the game's LoadLibraryA("D3D9.DLL") finds it
# there (R0). It chains to the system d3d9 and does all the mod's work.
enable_language(RC)
add_library(d2vr_proxy SHARED
    proxy/proxy.h
    proxy/dllmain.cpp
    proxy/d3d9_exports.cpp
    proxy/d3d9.def
    proxy/d3d9.rc
    core/util/log.cpp        core/util/log.h
    core/util/crash.cpp      core/util/crash.h
    core/util/clock.cpp      core/util/clock.h
    core/util/mem.cpp        core/util/mem.h
    core/util/ini.cpp        core/util/ini.h
    core/util/paths.cpp      core/util/paths.h
    core/util/diag.h
    core/config/config.cpp   core/config/config.h
    core/hooks/detour.cpp    core/hooks/detour.h
    core/hooks/callsite.cpp  core/hooks/callsite.h
    core/hooks/iat.cpp       core/hooks/iat.h
    core/hooks/vtable.cpp    core/hooks/vtable.h
    core/framework/command.cpp     core/framework/command.h
    core/framework/status.cpp      core/framework/status.h
    core/framework/frame_hooks.cpp core/framework/frame_hooks.h
    core/framework/shot.cpp        core/framework/shot.h
    core/input/inject.cpp          core/input/inject.h
    # The runtime layer, adopted verbatim from the Dishonored VR mod behind two
    # host seams (docs/ARCHITECTURE.md "The runtime layer"): fixes port between
    # the projects with the mechanical rename recorded there and nothing else.
    core/vr/openxr_runtime.cpp     core/vr/openxr_runtime.h
    core/vr/openxr_input.cpp       core/vr/openxr_input.h
    core/vr/input_snapshot.h
    core/vr/mono_anchor.h
    core/vr/aim_visual.h
    core/vr/hud_stub.cpp           core/vr/hud_stub.h
    core/vr/hud_anchor.h
    core/vr/image_orientation.h
    core/vr/pose_record.cpp        core/vr/pose_record.h
    core/vr/apilayer_guard.cpp     core/vr/apilayer_guard.h
    core/util/xr_math.h
    core/framework/bridge_profile.cpp core/framework/bridge_profile.h core/framework/bridge_profile_policy.h
    core/framework/diagnostic_ab.cpp  core/framework/diagnostic_ab.h  core/framework/fresh_pair.h
    # Dishonored headers kept verbatim so the adopted sources compile against
    # their real signatures; the bodies this mod does not have are stubs.
    core/framework/perf.h          core/framework/perf_stub.cpp
    core/gfx/frame_id.h            core/gfx/frame_id_stub.cpp
    core/gfx/d3d9ex.h              core/gfx/d3d9ex_host.h  core/gfx/d3d9ex_stub.cpp
    core/ui/ovl_ui.h               core/ui/ovl_ui_stub.cpp
    # The stereo seam, the mono method, the D3D9-to-D3D11 carry, the mod's D3D11 device.
    core/gfx/stereo.cpp            core/gfx/stereo.h
    core/gfx/mono_screen.cpp
    core/gfx/aer.cpp
    core/gfx/reentry_stub.cpp
    core/gfx/blit_quad.cpp         core/gfx/blit_quad.h
    core/gfx/capture.cpp           core/gfx/capture.h
    core/gfx/d3d11_device.cpp      core/gfx/d3d11_device.h
    game/darkness2/game.h
    game/darkness2/patterns.h
    game/darkness2/build_fingerprint.cpp
    game/darkness2/canaries.cpp    game/darkness2/canaries.h
    game/darkness2/commands.cpp
    game/darkness2/lua/lane.cpp    game/darkness2/lua/lane.h
)
set_target_properties(d2vr_proxy PROPERTIES OUTPUT_NAME "d3d9" PREFIX "")
add_dependencies(d2vr_proxy d2vr_version)
target_include_directories(d2vr_proxy PRIVATE . "${PROJECT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/generated")
# The proxy must add no static import that changes the game's DLL load order.
# kernel32, user32, advapi32 and shell32 are all static imports of DarknessII.exe
# itself (ENGINE_NOTES s1), so they are already loaded when d3d9 is asked for:
# advapi32 is the runtime layer's and the static loader's registry read of the
# ActiveRuntime key, shell32 the layer's SHGetFolderPathW. d3d9 itself, d3d11,
# dxgi and d3dcompiler are LoadLibrary'd; dbghelp is LoadLibrary'd at
# crash::install. tools\exports-check.ps1 asserts the import list against
# tests/golden/d3d9-imports.txt.
target_link_libraries(d2vr_proxy PRIVATE user32 advapi32 shell32 imgui)
target_compile_options(d2vr_proxy PRIVATE /W3 /permissive-)
target_compile_definitions(d2vr_proxy PRIVATE D2VR_BUILD_CONFIG="$<CONFIG>")
# The OpenXR loader is linked statically (no openxr_loader.dll ships). The
# adopted layer's `#else` block for a no-OpenXR build is incomplete upstream and
# is never compiled here: D2VR_WITH_OPENXR is 1 whenever the proxy builds.
if(NOT D2VR_WITH_OPENXR)
    message(FATAL_ERROR "d3d9.dll needs the OpenXR loader: configure with D2VR_WITH_OPENXR=ON")
endif()
target_link_libraries(d2vr_proxy PRIVATE openxr_loader)
target_compile_definitions(d2vr_proxy PRIVATE D2VR_WITH_OPENXR=1)
if(D2VR_WITH_LEGACY)
    target_compile_definitions(d2vr_proxy PRIVATE D2VR_WITH_LEGACY=1)
else()
    target_compile_definitions(d2vr_proxy PRIVATE D2VR_WITH_LEGACY=0)
endif()

# --- xr_hello32 - standalone OpenXR smoke client -------------------------------
# A 32-bit console app with NO mod code in it. tools\xrsim-selftest.ps1 runs it
# against the simulated runtime: if it fails, the simulator is broken, not the
# mod. Not shipped.
if(D2VR_WITH_OPENXR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/xr_hello32/main.cpp")
    add_executable(xr_hello32 tools/xr_hello32/main.cpp)
    target_link_libraries(xr_hello32 PRIVATE openxr_loader d3d11 dxgi)
endif()

# --- d2vr_xrsim32 - the simulated OpenXR runtime --------------------------------
# A Quest-3-shaped 32-bit OpenXR runtime so the harness can drive head, hands and
# controllers and capture per-eye compositor output with no headset. Selected
# per process with XR_RUNTIME_JSON (tools\xrsim-launch.ps1). NOT part of the
# mod and NOT shipped. It links NOTHING from the mod and must not link
# openxr_loader either - it IS the runtime the loader loads.
if(D2VR_WITH_XRSIM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/xrsim/xrsim_instance.cpp")
    add_library(d2vr_xrsim32 SHARED
        tools/xrsim/xrsim_common.h
        tools/xrsim/xrsim_internal.h
        tools/xrsim/xrsim_math.h
        tools/xrsim/xrsim_log.cpp
        tools/xrsim/xrsim_instance.cpp
        tools/xrsim/xrsim_session.cpp
        tools/xrsim/xrsim_frame.cpp
        tools/xrsim/xrsim_actions.cpp
        tools/xrsim/xrsim_control.cpp
        tools/xrsim/xrsim_compositor.cpp
        tools/xrsim/xrsim.def
    )
    set_target_properties(d2vr_xrsim32 PROPERTIES PREFIX "" OUTPUT_NAME "d2vr_xrsim32")
    target_include_directories(d2vr_xrsim32 PRIVATE tools/xrsim)
    target_link_libraries(d2vr_xrsim32 PRIVATE openxr_headers d3d11 dxgi d3dcompiler windowscodecs)
    # SEH shims around every entry point need unwind semantics off; the shims
    # deliberately hold no unwindable objects.
    target_compile_options(d2vr_xrsim32 PRIVATE /EHa)
endif()
