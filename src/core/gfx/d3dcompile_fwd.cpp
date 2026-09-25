// core/gfx/d3dcompile_fwd.cpp - D3DCompile without a static d3dcompiler import.
//
// ImGui's DX11 backend calls D3DCompile and carries `#pragma comment(lib,
// "d3dcompiler")`, which would add d3dcompiler_47.dll to d3d9.dll's import
// table; the proxy adds no static import the game exe does not already have
// (tests/golden/d3d9-imports.txt, tools/exports-check.ps1). This definition
// resolves the backend's reference first, so the import library is never
// consulted, and /NODEFAULTLIB:d3dcompiler.lib on the proxy turns a missing
// definition into a link error instead of a silent new import. The DLL is
// loaded the way core/gfx/blit_quad.cpp already loads it.
#define D2VR_CAT ::d2vr::log::Cat::d3d
#include <windows.h>
#include <d3dcompiler.h>
#include "core/util/log.h"

namespace {
pD3DCompile g_compile = nullptr;
bool g_tried = false;
}

HRESULT WINAPI D3DCompile(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, const D3D_SHADER_MACRO* pDefines,
                          ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
                          ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs)
{
    if (!g_tried) {
        g_tried = true;
        HMODULE m = LoadLibraryA("d3dcompiler_47.dll");
        g_compile = m ? (pD3DCompile)GetProcAddress(m, "D3DCompile") : nullptr;
        if (!g_compile) D2VR_ERROR("d3dcompile: d3dcompiler_47.dll or its D3DCompile is missing (err %lu) - the F10 panel cannot build its shaders", GetLastError());
        else D2VR_INFO("d3dcompile: forwarding to d3dcompiler_47.dll (loaded at runtime, not imported)");
    }
    if (!g_compile) return E_FAIL;
    return g_compile(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}
