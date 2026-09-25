// core/hooks/callsite.h - byte-verified rewrite of one `call rel32` (E8) site.
//
// The re-entry shape (docs/ARCHITECTURE.md S2): instead of patching a
// function's prologue, redirect ONE call instruction to a stub that calls the
// original target and returns to the same place. Verifies the E8 opcode and
// that its rel32 lands exactly on the expected target before writing.
#pragma once
#include <stdint.h>

namespace d2vr::hooks {

struct CallSite {
    uintptr_t at = 0;            // address of the E8 byte
    uintptr_t target = 0;        // where the original call went
    int32_t   savedRel = 0;
    int32_t   writtenRel = 0;
    bool      on = false;
};

// Verifies `call <expectedTarget>` at `at`, then points it at `stub`.
bool callsite_install(CallSite& c, const char* tag, uintptr_t at, uintptr_t expectedTarget, const void* stub);
void callsite_remove(CallSite& c, const char* tag);
// 0 = still ours, 1 = the original rel32 is back, 2 = something else.
int  callsite_check(const CallSite& c, uint8_t* now5);

} // namespace d2vr::hooks
