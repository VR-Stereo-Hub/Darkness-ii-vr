#include "core/util/mem.h"
#include <windows.h>

namespace d2vr::mem {

bool range_readable(const void* p, size_t n)
{
    const uint8_t* cur = (const uint8_t*)p;
    const uint8_t* end = cur + n;
    if (!p || (uintptr_t)p < 0x10000) return false;
    if ((uintptr_t)p + n < (uintptr_t)p) return false;
    while (cur < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(cur, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return false;
        cur = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    }
    return true;
}

bool safe_read32(uintptr_t p, uint32_t* out)
{
    if (p < 0x10000 || (p & 3)) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    *out = *(uint32_t*)p;
    return true;
}

} // namespace d2vr::mem
