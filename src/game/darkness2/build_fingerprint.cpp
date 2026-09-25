// game/darkness2/build_fingerprint.cpp - is this the DarknessII.exe the numbers
// in patterns.h were derived on? A wrong build refuses every code hook, keeps
// whatever needs no address, and stays playable (fail soft).
#define D2VR_CAT ::d2vr::log::Cat::game
#include <windows.h>
#include "game/darkness2/game.h"
#include "game/darkness2/patterns.h"
#include "core/util/log.h"
#include "core/util/paths.h"

namespace d2vr::game {
namespace { Fingerprint g_fp; }

const Fingerprint& fingerprint()
{
    if (g_fp.checked) return g_fp;
    g_fp.checked = true;
    HMODULE exe = GetModuleHandleA(nullptr);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)((uint8_t*)exe + dos->e_lfanew);
    g_fp.timeDateStamp = nt->FileHeader.TimeDateStamp;
    g_fp.sizeOfImage = nt->OptionalHeader.SizeOfImage;
    HANDLE f = CreateFileA(d2vr::paths::exe_path(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz; if (GetFileSizeEx(f, &sz)) g_fp.fileSize = (uint64_t)sz.QuadPart;
        CloseHandle(f);
    }
    const bool baseOk = (uintptr_t)exe == pat::kExeBase;
    const bool tsOk = g_fp.timeDateStamp == pat::kExeTimeDateStamp;
    const bool imgOk = g_fp.sizeOfImage == pat::kExeSizeOfImage;
    const bool fsOk = g_fp.fileSize == pat::kExeFileSize;
    g_fp.ok = baseOk && tsOk && imgOk && fsOk;
    D2VR_LOG(D2VR_CAT, g_fp.ok ? d2vr::log::Level::Info : d2vr::log::Level::Error,
             "fingerprint: base 0x%08lx (%s) timestamp 0x%08lx (%s) sizeOfImage 0x%08lx (%s) fileSize %llu (%s) -> %s",
             (unsigned long)(uintptr_t)exe, baseOk ? "ok" : "MISMATCH",
             (unsigned long)g_fp.timeDateStamp, tsOk ? "ok" : "MISMATCH",
             (unsigned long)g_fp.sizeOfImage, imgOk ? "ok" : "MISMATCH",
             (unsigned long long)g_fp.fileSize, fsOk ? "ok" : "MISMATCH",
             g_fp.ok ? "the build patterns.h was derived on; code hooks allowed"
                     : "NOT the derived build; every code hook REFUSES (the game runs flat)");
    return g_fp;
}

} // namespace d2vr::game
