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
    game/darkness2/game.h
    game/darkness2/patterns.h
    game/darkness2/build_fingerprint.cpp
    game/darkness2/canaries.cpp    game/darkness2/canaries.h
    game/darkness2/commands.cpp
)
set_target_properties(d2vr_proxy PROPERTIES OUTPUT_NAME "d3d9" PREFIX "")
add_dependencies(d2vr_proxy d2vr_version)
target_include_directories(d2vr_proxy PRIVATE . "${PROJECT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/generated")
# Only kernel32/user32 are static imports: the proxy must add no import that
# changes the game's DLL load order. d3d9 itself is LoadLibrary'd (the backend),
# dbghelp is LoadLibrary'd at crash::install.
target_link_libraries(d2vr_proxy PRIVATE user32)
target_compile_options(d2vr_proxy PRIVATE /W3 /permissive-)
target_compile_definitions(d2vr_proxy PRIVATE D2VR_BUILD_CONFIG="$<CONFIG>")
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
