// core/util/paths.h - where the mod keeps its files.
//
//   game_dir()  the folder holding d3d9.dll and DarknessII.exe. User-facing
//               files live here: darkness2_vr.ini, darkness2_vr.log (+ the ten
//               rotated .prev*.log), darkness2_vr_crash.txt, disable_vr.txt.
//   data_dir()  %LOCALAPPDATA%\Darkness2VR (override: D2VR_DATA_DIR, then the
//               ini's [Paths] DataDir). Harness and bulk files: command.txt,
//               ack.txt, status.json, dumps\, shots\, xrsim\. Never inside the
//               game folder, which a user might zip up in a bug report.
#pragma once
#include <windows.h>

namespace d2vr::paths {
void        init(HINSTANCE self);      // from DllMain; kernel32 only
const char* game_dir();
const char* exe_path();                // full path of the host exe
const char* module_path();             // full path of OUR module (the route measurement)
const char* data_dir();                // created on first call
void        set_data_dir(const char* dir);
const char* dumps_dir();               // <data_dir>\dumps, created on first call
const char* shots_dir();               // <data_dir>\shots, created on first call
// <dir>\<name> into out (size MAX_PATH); returns out
const char* in_game_dir(char* out, const char* name);
const char* in_data_dir(char* out, const char* name);
} // namespace d2vr::paths
