#define D2VR_CAT ::d2vr::log::Cat::hooks
#include "core/hooks/callsite.h"
#include "core/hooks/detour.h"
#include "core/util/log.h"
#include <windows.h>
#include <string.h>

namespace d2vr::hooks {

bool callsite_install(CallSite& c, const char* tag, uintptr_t at, uintptr_t expectedTarget, const void* stub)
{
    if (c.on) return true;
    if (!at) { D2VR_ERROR("%s: REFUSING - call site address is 0 (not derived yet)", tag); return false; }
    uint8_t* p = (uint8_t*)at;
    int32_t rel; memcpy(&rel, p + 1, 4);
    uintptr_t actual = at + 5 + (uintptr_t)rel;
    if (p[0] != 0xE8 || actual != expectedTarget) {
        char have[32]; hex_bytes(p, 5, have, sizeof(have));
        D2VR_ERROR("%s: REFUSING to rewrite - bytes at 0x%08x are %s (call -> 0x%08x), expected E8 -> 0x%08x. Wrong exe build?",
                   tag, (unsigned)at, have, (unsigned)actual, (unsigned)expectedTarget);
        return false;
    }
    DWORD op = 0;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &op)) {
        D2VR_ERROR("%s: VirtualProtect failed at 0x%08x (err %lu)", tag, (unsigned)at, GetLastError());
        return false;
    }
    c.savedRel = rel;
    c.writtenRel = (int32_t)((uintptr_t)stub - (at + 5));
    memcpy(p + 1, &c.writtenRel, 4);
    VirtualProtect(p, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    c.at = at; c.target = expectedTarget; c.on = true;
    D2VR_INFO("%s: REWRITTEN call at 0x%08x: was -> 0x%08x, now -> stub %p (which tail-jumps to the original)",
              tag, (unsigned)at, (unsigned)expectedTarget, stub);
    return true;
}

void callsite_remove(CallSite& c, const char* tag)
{
    if (!c.on) return;
    uint8_t* p = (uint8_t*)c.at;
    DWORD op = 0;
    if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(p + 1, &c.savedRel, 4);
        VirtualProtect(p, 5, op, &op);
        FlushInstructionCache(GetCurrentProcess(), p, 5);
    }
    c.on = false;
    D2VR_INFO("%s: call at 0x%08x restored -> 0x%08x", tag, (unsigned)c.at, (unsigned)c.target);
}

int callsite_check(const CallSite& c, uint8_t* now5)
{
    if (!c.on) return 2;
    memcpy(now5, (const void*)c.at, 5);
    int32_t rel; memcpy(&rel, now5 + 1, 4);
    if (now5[0] == 0xE8 && rel == c.writtenRel) return 0;
    if (now5[0] == 0xE8 && rel == c.savedRel) return 1;
    return 2;
}

} // namespace d2vr::hooks
