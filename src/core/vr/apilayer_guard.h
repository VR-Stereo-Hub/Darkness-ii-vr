// core/vr/apilayer_guard.h - the 64-bit implicit OpenXR API layer guard.
//
// A 64-bit implicit API layer (an OBS mirror, measured on the Dishonored dev
// machine) fails xrCreateInstance in a 32-bit process for EVERY runtime. The
// guard reads the layer manifests, checks each library's PE machine type and
// sets the layer's own disable_environment variable for THIS process only. It
// never writes the registry. [VR] DisableBadApiLayers=0 reports without acting.
// Call once from the first Direct3DCreate9(Ex), before vr::init_instance().
#pragma once

namespace d2vr::vr {
void disable_bad_api_layers();
}
