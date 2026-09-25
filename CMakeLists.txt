cmake_minimum_required(VERSION 3.21)

# VERSION is the single source of truth for the version the mod logs. Bump it
# here and nowhere else; src/CMakeLists.txt generates d2vr_version.h from it at
# build time, stamped with `git describe`.
project(Darkness2VR VERSION 0.1.0 LANGUAGES C CXX)

# DarknessII.exe is 32-bit x86 (PE32, base 0x400000). Everything we load into it
# must be too: the mod module, the simulated runtime and the smoke client.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "This mod targets a 32-bit game. Configure with the 'win32' preset:\n"
        "  cmake --preset win32\n"
        "(or pass -G \"Visual Studio 17 2022\" -A Win32)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT: no VC redist dependency inside the game process, and no CRT DLL
# added to the game's load order by us.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

option(D2VR_WITH_LEGACY "Compile the retired experiments in src/legacy" OFF)
option(D2VR_WITH_XRSIM  "Build the simulated OpenXR runtime (test tool, not shipped)" ON)

add_subdirectory(third_party)
add_subdirectory(src)
