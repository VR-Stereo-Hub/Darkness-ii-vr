#define D2VR_CAT ::d2vr::log::Cat::capture
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include "core/framework/capture.h"
#include "core/util/log.h"
#include "core/util/paths.h"

namespace d2vr::capture {
namespace {

volatile LONG g_pending = 0;
char g_tag[64] = "shot";
unsigned long g_count = 0;
char g_last[MAX_PATH] = "";
IDirect3DSurface9* g_sysmem = nullptr;
unsigned g_sysW = 0, g_sysH = 0; D3DFORMAT g_sysFmt = D3DFMT_UNKNOWN;

void release_sysmem() { if (g_sysmem) { g_sysmem->Release(); g_sysmem = nullptr; } }

bool write_bmp(const char* path, unsigned w, unsigned h, const uint8_t* bgra, int pitch)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    const unsigned rowBytes = (w * 3 + 3) & ~3u;
    const unsigned dataSize = rowBytes * h;
    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};
    bfh.bfType = 0x4D42; bfh.bfOffBits = sizeof(bfh) + sizeof(bih); bfh.bfSize = bfh.bfOffBits + dataSize;
    bih.biSize = sizeof(bih); bih.biWidth = (LONG)w; bih.biHeight = (LONG)h; bih.biPlanes = 1; bih.biBitCount = 24;
    bih.biCompression = BI_RGB; bih.biSizeImage = dataSize;
    fwrite(&bfh, sizeof(bfh), 1, f); fwrite(&bih, sizeof(bih), 1, f);
    static uint8_t row[16384 * 3 + 4];
    for (int y = (int)h - 1; y >= 0; --y) {           // BMP rows are bottom-up
        const uint8_t* src = bgra + y * pitch;
        for (unsigned x = 0; x < w; x++) { row[x * 3] = src[x * 4]; row[x * 3 + 1] = src[x * 4 + 1]; row[x * 3 + 2] = src[x * 4 + 2]; }
        for (unsigned x = w * 3; x < rowBytes; x++) row[x] = 0;
        fwrite(row, 1, rowBytes, f);
    }
    fclose(f);
    return true;
}

void take(IDirect3DDevice9* dev)
{
    IDirect3DSurface9* bb = nullptr;
    HRESULT hr = dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (FAILED(hr) || !bb) { D2VR_WARN("shot: GetBackBuffer failed 0x%08lx", (unsigned long)hr); return; }
    D3DSURFACE_DESC d; bb->GetDesc(&d);
    if (d.Format != D3DFMT_X8R8G8B8 && d.Format != D3DFMT_A8R8G8B8) {
        D2VR_WARN("shot: backbuffer format %d is not X8R8G8B8/A8R8G8B8 - not handled yet", (int)d.Format);
        bb->Release(); return;
    }
    if (d.Width > 16384) { D2VR_WARN("shot: width %u too large", d.Width); bb->Release(); return; }
    if (g_sysmem && (g_sysW != d.Width || g_sysH != d.Height || g_sysFmt != d.Format)) release_sysmem();
    if (!g_sysmem) {
        hr = dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &g_sysmem, nullptr);
        if (FAILED(hr)) { D2VR_WARN("shot: CreateOffscreenPlainSurface failed 0x%08lx", (unsigned long)hr); bb->Release(); return; }
        g_sysW = d.Width; g_sysH = d.Height; g_sysFmt = d.Format;
    }
    // A multisampled backbuffer cannot be read back directly; resolve first.
    IDirect3DSurface9* src = bb;
    IDirect3DSurface9* resolved = nullptr;
    if (d.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (SUCCEEDED(dev->CreateRenderTarget(d.Width, d.Height, d.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &resolved, nullptr)) &&
            SUCCEEDED(dev->StretchRect(bb, nullptr, resolved, nullptr, D3DTEXF_NONE))) src = resolved;
    }
    hr = dev->GetRenderTargetData(src, g_sysmem);
    if (resolved) resolved->Release();
    if (FAILED(hr)) { D2VR_WARN("shot: GetRenderTargetData failed 0x%08lx (multisample=%d)", (unsigned long)hr, (int)d.MultiSampleType); bb->Release(); return; }
    D3DLOCKED_RECT lr;
    if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) { D2VR_WARN("shot: LockRect failed"); bb->Release(); return; }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s_%03lu.bmp", d2vr::paths::shots_dir(), g_tag, g_count + 1);
    const bool ok = write_bmp(path, d.Width, d.Height, (const uint8_t*)lr.pBits, lr.Pitch);
    g_sysmem->UnlockRect();
    bb->Release();
    if (ok) {
        g_count++;
        strncpy(g_last, path, sizeof(g_last) - 1);
        // A tiny content measure so the log can say "black" without opening the file.
        unsigned long lum = 0; unsigned n = 0;
        if (SUCCEEDED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            for (unsigned y = 0; y < d.Height; y += 16) {
                const uint8_t* row = (const uint8_t*)lr.pBits + y * lr.Pitch;
                for (unsigned x = 0; x < d.Width; x += 16) { lum += row[x * 4] + row[x * 4 + 1] + row[x * 4 + 2]; n++; }
            }
            g_sysmem->UnlockRect();
        }
        D2VR_INFO("shot #%lu written: %s (%ux%u, mean sample luma %lu/765%s)", g_count, path, d.Width, d.Height,
                  n ? lum / n : 0, (n && lum / n < 6) ? " - looks BLACK" : "");
    } else {
        D2VR_WARN("shot: could not write %s", path);
    }
}

} // namespace

void request(const char* tag)
{
    if (tag && *tag) { strncpy(g_tag, tag, sizeof(g_tag) - 1); g_tag[sizeof(g_tag) - 1] = 0; }
    for (char* p = g_tag; *p; p++) if (*p == ' ' || *p == '\\' || *p == '/' || *p == ':') *p = '_';
    InterlockedExchange(&g_pending, 1);
}

void tick(IDirect3DDevice9* dev)
{
    if (!g_pending || !dev) return;
    InterlockedExchange(&g_pending, 0);
    take(dev);
}

void on_reset() { release_sysmem(); }
unsigned long count() { return g_count; }
const char* last_path() { return g_last; }

} // namespace d2vr::capture
