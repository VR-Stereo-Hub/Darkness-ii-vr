// core/config/config.h - darkness2_vr.ini next to DarknessII.exe.
//
// Read from the first Direct3DCreate9(Ex) call, never from DllMain (the profile
// API under the loader lock can abort the process). A missing ini is written
// from the default literal in config.cpp; tools/ini-golden.py extracts that
// literal into tests/golden/darkness2_vr.ini, and tools/install.ps1 diffs the
// installed ini against it.
#pragma once
#include <windows.h>

namespace d2vr::config {

struct Config {
    int  version = 1;
    char logLevel[16] = "";      // [Log] Level (env D2VR_LOG wins)
    char logCats[256] = "";      // [Log] Cats  (env D2VR_LOG_CATS wins)
    char dataDir[MAX_PATH] = ""; // [Paths] DataDir
    // [Canary]: R1's four hooks, default OFF. A soak run sets them ON in the ini.
    int  canaryCold = 0;
    int  canaryTick = 0;
    int  canaryCallSite = 0;
    int  canaryHot = 0;
    int  canaryLogHz = 1;        // [Canary] LogEverySeconds: the hits/s + re-read line cadence
    // [Lua]: R2's in-game half, the three wraps and the chunk runner. Default OFF.
    int  luaEnabled = 0;
    // [Overlay]: the F10 panel. Enabled arms the key; the panel itself stays hidden until pressed.
    int   overlayEnabled = 1;
    float overlayUiScale = 0.0f;   // 0 = from the eye height
    // [Camera]: the projection watch (the SS_Projection readback). Default OFF.
    int   cameraProjWatch = 0;
    // [VR]: the OpenXR runtime layer (core/vr/openxr_runtime, adopted from Dishonored).
    char  vrRuntime[16] = "auto";          // auto|native|steamvr
    char  vrRuntimeJson[MAX_PATH] = "";    // a manifest for this launch (the simulator; a Steam launch)
    int   vrDisableBadApiLayers = 1;       // the 64-bit implicit API layer guard acts (1) or only reports (0)
    // [Screen]: rung 1, the head-locked quad both eyes see.
    float screenDistanceM = 1.75f;
    float screenWidthM = 2.4f;
    int   screenHeadLocked = 1;
    // [Stereo]: the method the seam starts on and whether it is armed.
    char  stereoMethod[16] = "mono";
    int   stereoArmed = 1;
    // [Capture]: how the D3D9 backbuffer is carried into D3D11 (sync|deferred|shared|off).
    char  captureMode[16] = "deferred";
    int   captureSharedWait = 0;
    int   captureBboxMs = 30000;
    // [Device]: this game's device is already 9Ex; Ex=1 PERMITS the shared-surface capture.
    int   deviceEx = 1;
};

const Config& get();
void load();                 // idempotent; writes the default ini when absent
const char* ini_path();
bool loaded();

} // namespace d2vr::config
