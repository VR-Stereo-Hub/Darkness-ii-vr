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
};

const Config& get();
void load();                 // idempotent; writes the default ini when absent
const char* ini_path();
bool loaded();

} // namespace d2vr::config
