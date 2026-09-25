// core/hooks/vtable.h - COM vtable slot patching (the D3D9 interface and device
// hooks: CreateDevice(Ex), Present(Ex), Reset(Ex), EndScene). These are not
// code hooks in the exe: the vtables belong to the system d3d9 module.
#pragma once

namespace d2vr::hooks {
// Replaces slot `index` of comObject's vtable with newFn. Returns the previous
// entry, or null when the slot already held newFn or the page could not be
// unprotected.
void* patch_vtable(void* comObject, int index, void* newFn);
} // namespace d2vr::hooks
