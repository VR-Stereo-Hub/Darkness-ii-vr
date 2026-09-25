// core/hooks/detour.h - byte-verified jump detours into engine code.
//
// DarknessII.exe has no ASLR, so every engine hook is an absolute address
// (src/game/darkness2/patterns.h) paired with the bytes expected there. A
// detour refuses to patch when the bytes differ (wrong exe build) - that
// refusal is the mod's build check, so keep it loud. Nothing here runs at
// DllMain; hooks install from the present thread once the game is up, and
// R1's verdict (docs/ROADMAP.md) governs where they may sit.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace d2vr::hooks {

struct Detour {
    uintptr_t   at = 0;         // first byte of the patched instruction(s)
    uint8_t     saved[16] = {}; // the original bytes
    uint8_t     written[16] = {}; // what we wrote (for the 1 Hz re-read)
    size_t      len = 0;        // bytes replaced (>= 5)
    bool        on = false;
};

// Verifies `expected` (len bytes) at `at`, then writes jmp rel32 to `stub`
// (plus nop padding when len > 5). Logs the refusal with the bytes found.
bool detour_install(Detour& d, const char* tag, uintptr_t at, const uint8_t* expected, size_t len, const void* stub);
// Restores the saved bytes. Safe to call when not installed.
void detour_remove(Detour& d, const char* tag);
// The re-read: 0 = bytes are what we wrote, 1 = bytes are the ORIGINAL again
// (a clean revert), 2 = something else. `now` receives the current bytes.
int  detour_check(const Detour& d, uint8_t* now);

// Hex text of n bytes ("83 ec 1c"), for log lines.
void hex_bytes(const uint8_t* p, size_t n, char* out, size_t cap);

} // namespace d2vr::hooks
