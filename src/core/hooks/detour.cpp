#define D2VR_CAT ::d2vr::log::Cat::hooks
#include "core/hooks/detour.h"
#include "core/util/log.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

namespace d2vr::hooks {

void hex_bytes(const uint8_t* p, size_t n, char* out, size_t cap)
{
    size_t k = 0;
    for (size_t i = 0; i < n && k + 4 < cap; i++)
        k += (size_t)snprintf(out + k, cap - k, "%02x ", p[i]);
    if (k) out[k - 1] = 0; else out[0] = 0;
}

bool detour_install(Detour& d, const char* tag, uintptr_t at, const uint8_t* expected, size_t len, const void* stub)
{
    if (d.on) return true;
    if (!at) { D2VR_ERROR("%s: REFUSING - site address is 0 (not derived yet)", tag); return false; }
    if (len < 5 || len > sizeof(d.saved)) { D2VR_ERROR("%s: bad detour length %u", tag, (unsigned)len); return false; }
    uint8_t* p = (uint8_t*)at;
    if (memcmp(p, expected, len) != 0) {
        char have[64], want[64];
        hex_bytes(p, len, have, sizeof(have)); hex_bytes(expected, len, want, sizeof(want));
        D2VR_ERROR("%s: REFUSING to patch - bytes at 0x%08x are %s, expected %s. Wrong exe build?",
                   tag, (unsigned)at, have, want);
        return false;
    }
    DWORD op = 0;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &op)) {
        D2VR_ERROR("%s: VirtualProtect failed at 0x%08x (err %lu)", tag, (unsigned)at, GetLastError());
        return false;
    }
    memcpy(d.saved, p, len);
    int32_t rel = (int32_t)((uintptr_t)stub - (at + 5));
    p[0] = 0xE9;
    memcpy(p + 1, &rel, 4);
    for (size_t i = 5; i < len; i++) p[i] = 0x90;
    memcpy(d.written, p, len);
    VirtualProtect(p, len, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    d.at = at; d.len = len; d.on = true;
    char wrote[64]; hex_bytes(d.written, len, wrote, sizeof(wrote));
    D2VR_INFO("%s: INSTALLED at 0x%08x -> stub %p (bytes verified; now %s)", tag, (unsigned)at, stub, wrote);
    return true;
}

void detour_remove(Detour& d, const char* tag)
{
    if (!d.on) return;
    uint8_t* p = (uint8_t*)d.at;
    DWORD op = 0;
    if (VirtualProtect(p, d.len, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(p, d.saved, d.len);
        VirtualProtect(p, d.len, op, &op);
        FlushInstructionCache(GetCurrentProcess(), p, d.len);
    }
    d.on = false;
    D2VR_INFO("%s: removed (bytes restored at 0x%08x)", tag, (unsigned)d.at);
}

int detour_check(const Detour& d, uint8_t* now)
{
    if (!d.on) return 2;
    memcpy(now, (const void*)d.at, d.len);
    if (memcmp(now, d.written, d.len) == 0) return 0;
    if (memcmp(now, d.saved, d.len) == 0) return 1;
    return 2;
}

} // namespace d2vr::hooks
