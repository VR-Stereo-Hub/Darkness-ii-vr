// core/gfx/d3d11_device.h - the mod's own D3D11 device, created on the adapter the
// OpenXR runtime names (D3D_DRIVER_TYPE_UNKNOWN with an explicit adapter; a device
// on any other GPU succeeds at every call and shows nothing in the headset).
// d3d11.dll and dxgi.dll are LoadLibrary'd: the proxy adds no static import that
// could change the game's DLL load order. Adapted from the Dishonored VR mod's
// unity-build fragment; the bodies are its, the API below is this file's.
//
// Order matters: provide() is the runtime layer's device provider and carries the
// LUID; device() must not be called before the first provide(), or the device
// lands on the default adapter for the rest of the session (logged as MISMATCH).
#pragma once
#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace d2vr::d3d11 {
ID3D11Device* provide(const LUID* want);               // the runtime layer's DeviceProviderFn
ID3D11Device* device(ID3D11DeviceContext** ctx);       // the same device for the stereo methods; null = none
bool          created();
const char*   adapter_name();                          // "" until created
void          shutdown();
} // namespace d2vr::d3d11
