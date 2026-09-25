// core/gfx/reentry_stub.cpp - rung 3, SequentialReentry, registered by NAME so the
// seam knows the ladder and `stereo reentry` refuses with the reason instead of
// "no such method". The real method (docs/ROADMAP.md S2, VR-249) re-enters this
// engine's scene draw once per eye and needs the scene-draw seam VR-248 names.
// Dishonored's reentry.cpp is not adopted: its call-site patch is that engine's.
#include <windows.h>
#include "core/gfx/stereo.h"

namespace d2vr::stereo {
namespace {
class ReentryStub final : public IStereo {
public:
    const char* name() const override { return "reentry"; }
    bool implemented() const override { return false; }
    const char* note() const override {
        return "SequentialReentry needs the Darkness II scene-draw seam (ROADMAP S2, VR-248/VR-249); "
               "not adopted from Dishonored, whose call-site patch is that engine's.";
    }
    void begin_frame(const FrameInput&) override {}
    int  eye_for_next_frame() const override { return 0; }
    bool end_frame(const FrameDevices&, FrameOutput&) override { return false; }
    void on_reset() override {}
    void shutdown() override {}
    void status(d2vr::status::Writer&) override {}
};
ReentryStub g_stub;
}
IStereo* create_reentry() { return &g_stub; }
} // namespace d2vr::stereo
