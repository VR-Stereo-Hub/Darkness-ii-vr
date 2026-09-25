// core/hooks/iat.h - import-table hooks (the later XInput bridge: serve the VR
// controllers as the pad the game already understands).
#pragma once
#include <windows.h>

namespace d2vr::hooks {
// Address of the IAT entry `mod` uses to call dllName!funcName, or null.
void** find_iat_slot_in(HMODULE mod, const char* dllName, const char* funcName);
// Same for the main exe.
void** find_iat_slot(const char* dllName, const char* funcName);
// Writes hook into an IAT slot (unprotect, swap, reprotect). Returns the
// previous target, or null on failure. Loader-lock safe (kernel32 only).
void* patch_iat_slot(void** slot, void* hook);
} // namespace d2vr::hooks
