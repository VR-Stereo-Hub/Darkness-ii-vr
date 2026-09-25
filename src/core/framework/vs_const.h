// core/framework/vs_const.h - the projection watch (VR-242, R4's first step).
//
// The renderer's projection reaches the GPU as a vertex-shader constant, and
// every shader in this game carries a CTAB constant table (ENGINE_NOTES s2), so
// the register that holds `SS_Projection` can be found by NAME when the shader
// is created and read back when it is uploaded. That readback is the camera
// seam's sensor: the rendered FOV as a number, so "did the write take" is
// arithmetic on the projection diagonal, not an opinion about a picture.
//
// Three device vtable slots (the system d3d9 vtable, not exe code), patched
// with the other device hooks inside CreateDevice(Ex) so no shader predates
// them: 91 CreateVertexShader (the CTAB parse, always: a passive record),
// 92 SetVertexShader (the current shader), 94 SetVertexShaderConstantF (the
// readback, only while the watch is ON; one bool test otherwise).
// `camera projwatch on|off|names` (game/darkness2/camera.cpp) drives it;
// [Camera] ProjWatch= is the default (OFF).
#pragma once
#include <stdint.h>

struct IDirect3DDevice9;
namespace d2vr::status { class Writer; }

namespace d2vr::vsconst {
void hook_device(IDirect3DDevice9* dev);   // from frame_hooks' hook_device

void set_watch(bool on);
bool watch();
void set_names(bool on);                   // log every CTAB constant name once per shader (the falsifiable path)
bool names();

// The mode (most frequent) perspective projection of the last completed
// present, derived on the present thread by present_tick().
struct Projection {
    bool     valid = false;
    float    m[16] = {};       // the four registers as uploaded, row by register
    float    fovVdeg = 0.0f;   // 2*atan(1/|m11|)
    float    fovHdeg = 0.0f;   // 2*atan(1/|m00|)
    float    aspect = 0.0f;    // m11/m00 = tan(H/2)/tan(V/2)
    uint32_t present = 0;      // the present it was voted for
    uint32_t votes = 0;        // uploads of this pair in that present
    uint32_t perspUploads = 0; // perspective uploads in that present
    uint32_t allUploads = 0;   // every SS_Projection upload in that present
    uint32_t distinct = 0;     // distinct perspective pairs in that present
};
bool latest(Projection& out);
void present_tick(uint32_t present);       // present thread: roll the vote

// The direct candidate (`camera ctab <deg>|off`): while set, every perspective
// SS_Projection upload has its m00/m11 replaced for the asked VERTICAL FOV
// (the aspect kept) before it reaches the device. The watch still reads the
// PRE-substitution upload (it measures the engine, not this lever).
void set_substitute(float fovVdeg);        // 0 = off
float substitute();
uint32_t substituted();                    // constants rewritten so far

uint32_t shaders_seen();                   // CreateVertexShader calls
uint32_t shaders_with_projection();        // of which carry SS_Projection
void log_status();
void status(d2vr::status::Writer& w);
} // namespace d2vr::vsconst
