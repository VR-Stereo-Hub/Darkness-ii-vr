#define D2VR_CAT ::d2vr::log::Cat::hooks
#include "core/hooks/vtable.h"
#include "core/util/log.h"
#include <windows.h>

namespace d2vr::hooks {

void* patch_vtable(void* comObject, int index, void* newFn)
{
    void** vtbl = *(void***)comObject;
    void* old = vtbl[index];
    if (old == newFn) return nullptr;
    DWORD oldProt;
    if (!VirtualProtect(&vtbl[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
        D2VR_ERROR("VirtualProtect failed on vtable slot %d (err %lu)", index, GetLastError());
        return nullptr;
    }
    vtbl[index] = newFn;
    VirtualProtect(&vtbl[index], sizeof(void*), oldProt, &oldProt);
    return old;
}

} // namespace d2vr::hooks
