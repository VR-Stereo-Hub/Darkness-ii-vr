// game/darkness2/camera.h - the camera seam's first instrument (VR-242).
//
// Before the camera can be driven per eye, one question: which FOV write does
// the renderer honour? The projection watch (core/framework/vs_const) reads the
// rendered projection back as a number; `camera eyetest` writes a candidate
// (the Lua lane's SetBaseFovOverride, the first and so far only candidate the
// engine carries itself), predicts what the projection's vertical FOV must
// become under each reading of the argument (vertical | horizontal at the
// render aspect | horizontal at 16:9), and judges 120 presents against a
// two-sided band. `nowrite` runs the same schedule without writing and must
// print DISCARDED: the negative control. The direct constant rewrite
// (`camera ctab`) is a separate lever the watch cannot judge.
//
// Seam: camera status | projwatch on|off | names on|off | ctab <deg>|off |
//       eyetest [lua|all] [nowrite] [<askA> <askB>] | eyetest stop
#pragma once
#include <stdint.h>

namespace d2vr::status { class Writer; }

namespace d2vr::game::camera {
void init_from_config();            // present thread, first present: [Camera] ProjWatch
void present_tick(double nowMs);    // present thread, after the game tick
bool command(const char* cmd, const char* args);
void status(d2vr::status::Writer& w);
bool eyetest_active();
} // namespace d2vr::game::camera
