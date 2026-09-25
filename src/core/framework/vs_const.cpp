// core/framework/vs_const.cpp - the projection watch (see vs_const.h).
#define D2VR_CAT ::d2vr::log::Cat::camera
#include <windows.h>
#include <d3d9.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/framework/vs_const.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/hooks/vtable.h"
#include "core/util/log.h"

namespace d2vr::vsconst {
namespace {

constexpr int kSlotCreateVertexShader = 91;
constexpr int kSlotSetVertexShader = 92;
constexpr int kSlotSetVertexShaderConstantF = 94;

typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateVS)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetVS)(IDirect3DDevice9*, IDirect3DVertexShader9*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetVSConstF)(IDirect3DDevice9*, UINT, const float*, UINT);
PFN_CreateVS g_origCreateVS = nullptr;
PFN_SetVS g_origSetVS = nullptr;
PFN_SetVSConstF g_origSetVSConstF = nullptr;

// shader pointer -> the SS_Projection register (open addressing, upsert by
// pointer: a freed address reused by a new shader overwrites the old entry
// before any SetVertexShader can name it; the pointer is never dereferenced).
struct Entry { void* shader; uint16_t reg; uint16_t count; bool hasProj; bool byShape; bool lazyParsed; };
constexpr uint32_t kTableSize = 8192;
Entry g_table[kTableSize] = {};
volatile LONG g_tableUsed = 0;
volatile LONG g_tableOverflow = 0;
volatile LONG g_shadersSeen = 0, g_shadersWithProj = 0, g_ctabMissing = 0, g_ctabParseFail = 0;

volatile LONG g_watch = 0, g_names = 0;
volatile LONG g_namesLogged = 0;
constexpr long kNamesCap = 600;
void* g_current = nullptr;        // the current vertex shader (SetVertexShader)
const Entry* g_currentEntry = nullptr;
DWORD g_callThread = 0;
volatile LONG g_callThreadLogged = 0;

// The per-present vote, written on the call thread, rolled on the present thread.
struct Pair { float m00, m11; uint32_t votes; float m[16]; };
constexpr int kMaxPairs = 8;
Pair g_pairs[kMaxPairs];
int g_pairCount = 0;
uint32_t g_uploadsThisPresent = 0, g_perspThisPresent = 0;
Projection g_latest[2];
volatile LONG g_latestIdx = 0;
float g_lastLoggedV = -1.0f;
volatile LONG g_uploadsTotal = 0, g_perspTotal = 0;

// The substitution lever.
volatile LONG g_substOn = 0;
float g_substV = 0.0f;
volatile LONG g_substituted = 0;

uint32_t hash_ptr(void* p) { uint32_t h = (uint32_t)(uintptr_t)p; h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; return h & (kTableSize - 1); }

Entry* find(void* shader)
{
    if (!shader) return nullptr;
    uint32_t i = hash_ptr(shader);
    for (uint32_t n = 0; n < kTableSize; n++, i = (i + 1) & (kTableSize - 1)) {
        if (g_table[i].shader == shader) return &g_table[i];
        if (!g_table[i].shader) return nullptr;
    }
    return nullptr;
}

Entry* upsert(void* shader, bool hasProj, uint16_t reg, uint16_t count)
{
    uint32_t i = hash_ptr(shader);
    for (uint32_t n = 0; n < kTableSize; n++, i = (i + 1) & (kTableSize - 1)) {
        if (g_table[i].shader == shader || !g_table[i].shader) {
            if (!g_table[i].shader) InterlockedIncrement(&g_tableUsed);
            g_table[i].shader = shader; g_table[i].hasProj = hasProj; g_table[i].reg = reg; g_table[i].count = count;
            g_table[i].byShape = false; g_table[i].lazyParsed = false;
            return &g_table[i];
        }
    }
    InterlockedIncrement(&g_tableOverflow);
    return nullptr;
}

bool contains_nocase(const char* hay, const char* needle)
{
    const size_t n = strlen(needle);
    for (const char* p = hay; *p; p++) if (!_strnicmp(p, needle, n)) return true;
    return false;
}

// ---- the CTAB parse (D3DX constant-table layout; every offset relative to the
// D3DXSHADER_CONSTANTTABLE struct, which begins right after the CTAB fourcc) ----
struct CtabInfo { bool found; uint16_t reg, count; uint32_t names; uint16_t regSet, cls, rows, cols; };

// The engine's enum name is ShaderRegister::SS_Projection; the CTAB may carry the
// HLSL identifier under any spelling. Exact first, then a bare `...Projection`
// suffix that is not a composite (World/View/Inverse/Prev...) whose diagonal
// would not be the FOV. Launch 6 (2026-09-25): 0 of 81 shaders named
// `SS_Projection`; the shape classifier below is the fallback that needs no name.
bool name_matches(const char* name)
{
    if (!strcmp(name, "SS_Projection")) return true;
    const char* colon = strrchr(name, ':');
    if (colon && !strcmp(colon + 1, "SS_Projection")) return true;
    const char* dot = strrchr(name, '.');
    if (dot && !strcmp(dot + 1, "SS_Projection")) return true;
    const size_t n = strlen(name);
    if (n >= 10 && !_stricmp(name + n - 10, "Projection")) {
        static const char* const kComposite[] = { "World", "View", "Inverse", "Inv", "Prev", "Shadow", "Light", "Texture", "Reflect" };
        for (const char* k : kComposite) if (contains_nocase(name, k)) return false;
        return true;
    }
    return false;
}

// A pure symmetric projection, either packing: a diagonal top-left 2x2, zeros
// everywhere else except m22 and the near term, m33 == 0 and the w term +-1.
bool looks_perspective(const float* m)
{
    const float eps = 1e-6f;
    if (fabsf(m[15]) > eps) return false;
    if (!(fabsf(fabsf(m[11]) - 1.0f) < 0.001f || fabsf(fabsf(m[14]) - 1.0f) < 0.001f)) return false;
    if (fabsf(m[0]) < 1e-4f || fabsf(m[5]) < 1e-4f || fabsf(m[0]) > 50.0f || fabsf(m[5]) > 50.0f) return false;
    static const int kZero[] = { 1, 2, 3, 4, 6, 7, 8, 9, 12, 13 };
    for (int z : kZero) if (fabsf(m[z]) > eps) return false;
    return true;
}

bool parse_ctab_table(const uint8_t* tab, size_t size, void* shader, CtabInfo& out)
{
    if (size < 28) return false;
    const uint32_t constants = *(const uint32_t*)(tab + 12);
    const uint32_t infoOff = *(const uint32_t*)(tab + 16);
    if (constants > 4096 || infoOff + constants * 20 > size) return false;
    for (uint32_t i = 0; i < constants; i++) {
        const uint8_t* ci = tab + infoOff + i * 20;
        const uint32_t nameOff = *(const uint32_t*)(ci + 0);
        const uint16_t regSet = *(const uint16_t*)(ci + 4);
        const uint16_t regIdx = *(const uint16_t*)(ci + 6);
        const uint16_t regCnt = *(const uint16_t*)(ci + 8);
        const uint32_t typeOff = *(const uint32_t*)(ci + 12);
        if (nameOff >= size || typeOff + 16 > size) continue;
        const char* name = (const char*)(tab + nameOff);
        size_t nlen = 0;
        while (nameOff + nlen < size && name[nlen]) nlen++;
        if (nameOff + nlen >= size) continue;   // unterminated
        out.names++;
        const uint16_t cls = *(const uint16_t*)(tab + typeOff + 0);
        const uint16_t rows = *(const uint16_t*)(tab + typeOff + 4);
        const uint16_t cols = *(const uint16_t*)(tab + typeOff + 6);
        if (g_names && g_namesLogged < kNamesCap) {
            InterlockedIncrement(&g_namesLogged);
            D2VR_INFO("vsconst: shader %p constant '%s' set=%u reg=c%u count=%u class=%u %ux%u", shader, name,
                      (unsigned)regSet, (unsigned)regIdx, (unsigned)regCnt, (unsigned)cls, (unsigned)rows, (unsigned)cols);
        }
        if (!out.found && name_matches(name)) {
            // D3DXRS_FLOAT4 = 2; D3DXPC_MATRIX_ROWS = 2, D3DXPC_MATRIX_COLUMNS = 3. RegisterCount is
            // NOT required to be 4: fxc trims the registers the shader never reads.
            if (regSet == 2 && (cls == 2 || cls == 3) && rows == 4 && cols == 4 && regCnt >= 2) {
                out.found = true; out.reg = regIdx; out.count = regCnt; out.regSet = regSet; out.cls = cls; out.rows = rows; out.cols = cols;
            } else {
                D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Warn, 5,
                    "vsconst: shader %p has '%s' but not as a float4x4 matrix (set=%u class=%u %ux%u count=%u) - not used",
                    shader, name, (unsigned)regSet, (unsigned)cls, (unsigned)rows, (unsigned)cols, (unsigned)regCnt);
            }
        }
    }
    return true;
}

// Walk the SM3 token stream for the CTAB comment. Returns false when no CTAB
// comment exists (a stripped shader); out.found says whether SS_Projection is in it.
bool parse_shader(const DWORD* fn, void* shader, CtabInfo& out)
{
    out = CtabInfo();
    if (!fn) return false;
    const DWORD version = fn[0];
    if ((version & 0xFFFF0000) != 0xFFFE0000 && (version & 0xFFFF0000) != 0xFFFF0000) return false;
    constexpr size_t kMaxDwords = 128 * 1024;
    size_t i = 1;
    bool sawCtab = false;
    while (i < kMaxDwords) {
        const DWORD tok = fn[i];
        if (tok == 0x0000FFFF) break;
        if ((tok & 0xFFFF) == 0xFFFE) {                  // a comment: (len << 16) | 0xFFFE
            const size_t len = (tok >> 16) & 0x7FFF;
            if (len >= 1 && fn[i + 1] == 0x42415443) {  // 'CTAB'
                sawCtab = true;
                parse_ctab_table((const uint8_t*)(fn + i + 2), len * 4 - 4, shader, out);
            }
            i += 1 + len;
            continue;
        }
        i += 1 + ((tok >> 24) & 0x0F);                   // an instruction: its length in following DWORDs (SM2+)
    }
    return sawCtab;
}

void note_call_thread(const char* what)
{
    const DWORD tid = GetCurrentThreadId();
    if (!g_callThread) g_callThread = tid;
    if (!g_callThreadLogged) {
        g_callThreadLogged = 1;
        const DWORD pt = d2vr::frame::present_thread();
        D2VR_INFO("vsconst: first %s on thread %lu; the present thread is %lu -> %s", what, (unsigned long)tid,
                  (unsigned long)pt, tid == pt ? "the SAME thread" : "a DIFFERENT thread (the vote is rolled across threads)");
    }
}

HRESULT STDMETHODCALLTYPE hkCreateVertexShader(IDirect3DDevice9* self, const DWORD* fn, IDirect3DVertexShader9** out)
{
    const HRESULT hr = g_origCreateVS(self, fn, out);
    if (FAILED(hr) || !out || !*out) return hr;
    InterlockedIncrement(&g_shadersSeen);
    CtabInfo ci;
    bool sawCtab = false;
    __try {
        sawCtab = parse_shader(fn, *out, ci);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedIncrement(&g_ctabParseFail);
        D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Warn, 3, "vsconst: EXCEPTION parsing the CTAB of shader %p - recorded as no projection", (void*)*out);
        ci = CtabInfo();
    }
    if (!sawCtab) InterlockedIncrement(&g_ctabMissing);
    if (ci.found) InterlockedIncrement(&g_shadersWithProj);
    upsert(*out, ci.found, ci.reg, ci.count);
    D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Info, 4,
        "vsconst: CreateVertexShader #%ld -> %p: %s (%u constants%s)", (long)g_shadersSeen, (void*)*out,
        ci.found ? "SS_Projection FOUND" : sawCtab ? "no SS_Projection" : "no CTAB", (unsigned)ci.names,
        ci.found ? "" : "");
    if (ci.found) {
        D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Info, 3,
            "vsconst: shader %p: SS_Projection at c%u, %u registers, class %s", (void*)*out, (unsigned)ci.reg, (unsigned)ci.count,
            ci.cls == 2 ? "MATRIX_ROWS" : "MATRIX_COLUMNS");
    }
    return hr;
}

// The lazy path: a shader that predates the hook or whose names were not logged
// at create is read back through GetFunction (no AddRef; the engine just handed
// it to SetVertexShader, so it is live) and parsed the same way.
void lazy_parse(IDirect3DVertexShader9* sh, Entry* e)
{
    UINT size = 0;
    if (FAILED(sh->GetFunction(nullptr, &size)) || size == 0 || size > (1u << 20)) return;
    DWORD* buf = (DWORD*)malloc(size + 8);
    if (!buf) return;
    memset(buf, 0, size + 8);
    if (SUCCEEDED(sh->GetFunction(buf, &size))) {
        CtabInfo ci;
        bool sawCtab = false;
        __try { sawCtab = parse_shader(buf, sh, ci); } __except (EXCEPTION_EXECUTE_HANDLER) { ci = CtabInfo(); }
        if (ci.found && !e->hasProj) {
            e->hasProj = true; e->reg = ci.reg; e->count = ci.count; e->byShape = false;
            InterlockedIncrement(&g_shadersWithProj);
            D2VR_INFO("vsconst: shader %p (lazy): projection constant at c%u, %u registers", (void*)sh, (unsigned)ci.reg, (unsigned)ci.count);
        }
        D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Info, 8, "vsconst: shader %p (lazy): %s, %u constants named",
                         (void*)sh, sawCtab ? "CTAB read back" : "no CTAB", (unsigned)ci.names);
    }
    free(buf);
}

HRESULT STDMETHODCALLTYPE hkSetVertexShader(IDirect3DDevice9* self, IDirect3DVertexShader9* sh)
{
    g_current = sh;
    Entry* e = find(sh);
    if (sh && !e) e = upsert(sh, false, 0, 0);   // a shader from before the hook (should not happen: hooked at create)
    if (sh && e && g_names && !e->lazyParsed) { e->lazyParsed = true; lazy_parse(sh, e); }
    g_currentEntry = e;
    return g_origSetVS(self, sh);
}

bool is_perspective(const float* m) { return looks_perspective(m); }

void vote(const float* m)
{
    const float m00 = m[0], m11 = m[5];
    for (int i = 0; i < g_pairCount; i++) {
        if (fabsf(g_pairs[i].m00 - m00) < 1e-5f && fabsf(g_pairs[i].m11 - m11) < 1e-5f) { g_pairs[i].votes++; return; }
    }
    if (g_pairCount < kMaxPairs) {
        Pair& p = g_pairs[g_pairCount++];
        p.m00 = m00; p.m11 = m11; p.votes = 1; memcpy(p.m, m, sizeof(p.m));
    }
}

HRESULT STDMETHODCALLTYPE hkSetVertexShaderConstantF(IDirect3DDevice9* self, UINT start, const float* data, UINT count)
{
    if ((g_watch || g_substOn) && data && count) {
        Entry* e = (Entry*)g_currentEntry;
        // The shape classifier: no name known for this shader yet, so look for a
        // projection-shaped block of four registers in this upload (launch 6:
        // the CTABs name 0-3 constants and none is the projection; the engine
        // binds its ShaderRegister enum to fixed registers).
        if (e && !e->hasProj && count >= 4) {
            for (UINT off = 0; off + 4 <= count; off++) {
                if (looks_perspective(data + off * 4)) {
                    e->hasProj = true; e->byShape = true; e->reg = (uint16_t)(start + off); e->count = 4;
                    InterlockedIncrement(&g_shadersWithProj);
                    D2VR_LOG_FIRST_N(D2VR_CAT, ::d2vr::log::Level::Info, 6,
                        "vsconst: shader %p: a projection-SHAPED block at c%u in a %u-register upload starting at c%u (m00 %.5f m11 %.5f m22 %.5f w %.3f/%.3f) - recorded by shape",
                        e->shader, (unsigned)e->reg, count, start, data[off * 4 + 0], data[off * 4 + 5], data[off * 4 + 10], data[off * 4 + 11], data[off * 4 + 14]);
                    break;
                }
            }
        }
        if (e && e->hasProj && start <= e->reg && start + count >= (UINT)e->reg + 2) {
            note_call_thread("SS_Projection upload");
            const float* m = data + (e->reg - start) * 4;
            const UINT have = (start + count) - e->reg;          // registers of the matrix in this upload
            float full[16] = {};
            memcpy(full, m, (have >= 4 ? 4 : have) * 16);
            InterlockedIncrement(&g_uploadsTotal);
            g_uploadsThisPresent++;
            const bool persp = have >= 4 ? is_perspective(full) : (fabsf(full[0]) > 0.0f && fabsf(full[5]) > 0.0f);
            if (persp) {
                InterlockedIncrement(&g_perspTotal);
                g_perspThisPresent++;
                if (g_watch) vote(full);
                if (g_substOn && g_substV > 0.0f) {
                    // The direct candidate: keep the aspect, replace the diagonal for the asked vertical FOV.
                    float copy[4096 / 4 * 4];
                    if (count * 4 <= sizeof(copy) / sizeof(copy[0])) {
                        memcpy(copy, data, count * 16);
                        float* mm = copy + (e->reg - start) * 4;
                        const float aspect = fabsf(mm[5]) > 1e-6f ? mm[5] / mm[0] : 1.0f;
                        const float newM11 = 1.0f / tanf(g_substV * 0.5f * 3.14159265f / 180.0f) * (mm[5] < 0 ? -1.0f : 1.0f);
                        mm[5] = newM11;
                        mm[0] = newM11 / aspect;
                        InterlockedIncrement(&g_substituted);
                        return g_origSetVSConstF(self, start, copy, count);
                    }
                }
            }
        }
    }
    return g_origSetVSConstF(self, start, data, count);
}

} // namespace

void hook_device(IDirect3DDevice9* dev)
{
    void* o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotCreateVertexShader, (void*)hkCreateVertexShader))) g_origCreateVS = (PFN_CreateVS)o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotSetVertexShader, (void*)hkSetVertexShader))) g_origSetVS = (PFN_SetVS)o;
    if ((o = d2vr::hooks::patch_vtable(dev, kSlotSetVertexShaderConstantF, (void*)hkSetVertexShaderConstantF))) g_origSetVSConstF = (PFN_SetVSConstF)o;
    D2VR_INFO("vsconst: device slots 91/92/94 (CreateVertexShader, SetVertexShader, SetVertexShaderConstantF) hooked on %p: %s; "
              "the CTAB parse records every shader, the readback waits for `camera projwatch on`",
              (void*)dev, (g_origCreateVS && g_origSetVS && g_origSetVSConstF) ? "all three" : "NOT all three (see the vtable lines)");
}

void set_watch(bool on)
{
    InterlockedExchange(&g_watch, on ? 1 : 0);
    if (!on) g_lastLoggedV = -1.0f;
    D2VR_INFO("vsconst: projection watch %s (%ld shaders seen, %ld with SS_Projection, %ld without a CTAB, table %ld/%u)",
              on ? "ON - the SS_Projection uploads are read and voted per present" : "OFF",
              (long)g_shadersSeen, (long)g_shadersWithProj, (long)g_ctabMissing, (long)g_tableUsed, kTableSize);
}
bool watch() { return g_watch != 0; }
void set_names(bool on)
{
    InterlockedExchange(&g_names, on ? 1 : 0);
    D2VR_INFO("vsconst: CTAB name logging %s (shaders created from now on list every constant, cap %ld lines; %ld logged so far)",
              on ? "ON" : "OFF", kNamesCap, (long)g_namesLogged);
}
bool names() { return g_names != 0; }

bool latest(Projection& out)
{
    out = g_latest[g_latestIdx];
    return out.valid;
}

void present_tick(uint32_t present)
{
    if (!g_watch) { g_pairCount = 0; g_uploadsThisPresent = 0; g_perspThisPresent = 0; return; }
    Projection p;
    p.present = present;
    p.allUploads = g_uploadsThisPresent;
    p.perspUploads = g_perspThisPresent;
    p.distinct = (uint32_t)g_pairCount;
    int best = -1;
    for (int i = 0; i < g_pairCount; i++) if (best < 0 || g_pairs[i].votes > g_pairs[best].votes) best = i;
    if (best >= 0) {
        const Pair& b = g_pairs[best];
        p.valid = fabsf(b.m00) > 1e-6f && fabsf(b.m11) > 1e-6f;
        memcpy(p.m, b.m, sizeof(p.m));
        p.votes = b.votes;
        p.fovVdeg = 2.0f * atanf(1.0f / fabsf(b.m11)) * 180.0f / 3.14159265f;
        p.fovHdeg = 2.0f * atanf(1.0f / fabsf(b.m00)) * 180.0f / 3.14159265f;
        p.aspect = b.m11 / b.m00;
    }
    const LONG next = g_latestIdx ^ 1;
    g_latest[next] = p;
    InterlockedExchange(&g_latestIdx, next);
    if (p.valid && fabsf(p.fovVdeg - g_lastLoggedV) > 0.05f) {
        char pairs[160] = "";
        for (int i = 0; i < g_pairCount && i < 4; i++) {
            char t[40];
            snprintf(t, sizeof(t), "%s[%.4f %.4f x%u]", i ? " " : "", g_pairs[i].m00, g_pairs[i].m11, g_pairs[i].votes);
            strncat(pairs, t, sizeof(pairs) - strlen(pairs) - 1);
        }
        D2VR_LOG_EVERY_MS(D2VR_CAT, ::d2vr::log::Level::Info, 500,
            "vsconst: projection V=%.2f H=%.2f deg aspect=%.4f (m00 %.5f m11 %.5f, %u of %u perspective uploads, %u total, %u distinct: %s) at present %u",
            p.fovVdeg, p.fovHdeg, p.aspect, p.m[0], p.m[5], p.votes, p.perspUploads, p.allUploads, p.distinct, pairs, present);
        g_lastLoggedV = p.fovVdeg;
    }
    g_pairCount = 0; g_uploadsThisPresent = 0; g_perspThisPresent = 0;
}

void set_substitute(float fovVdeg)
{
    g_substV = fovVdeg;
    InterlockedExchange(&g_substOn, fovVdeg > 0.0f ? 1 : 0);
    D2VR_INFO("vsconst: ctab substitution %s (%.1f deg vertical; %ld constants rewritten so far) - the direct candidate, judged by the picture, "
              "not by the watch (which reads the upload BEFORE the substitution)",
              fovVdeg > 0.0f ? "ON" : "OFF", fovVdeg, (long)g_substituted);
}
float substitute() { return g_substOn ? g_substV : 0.0f; }
uint32_t substituted() { return (uint32_t)g_substituted; }
uint32_t shaders_seen() { return (uint32_t)g_shadersSeen; }
uint32_t shaders_with_projection() { return (uint32_t)g_shadersWithProj; }

void log_status()
{
    Projection p; latest(p);
    int byShape = 0, byName = 0;
    for (uint32_t i = 0; i < kTableSize; i++) if (g_table[i].shader && g_table[i].hasProj) { if (g_table[i].byShape) byShape++; else byName++; }
    D2VR_INFO("vsconst: projection registers known for %d shaders (%d by CTAB name, %d by shape)%s", byName + byShape, byName, byShape,
              g_currentEntry && g_currentEntry->hasProj ? "" : "; the current shader has none");
    D2VR_INFO("vsconst: watch %s, names %s, ctab subst %.1f (%ld rewritten) | shaders %ld (%ld with a projection register, %ld no CTAB, %ld parse faults, table %ld/%u, overflow %ld) "
              "| uploads %ld (%ld perspective) | latest: %s V=%.2f H=%.2f aspect=%.4f from %u/%u persp of %u at present %u | call thread %lu",
              g_watch ? "ON" : "OFF", g_names ? "ON" : "OFF", substitute(), (long)g_substituted, (long)g_shadersSeen, (long)g_shadersWithProj,
              (long)g_ctabMissing, (long)g_ctabParseFail, (long)g_tableUsed, kTableSize, (long)g_tableOverflow, (long)g_uploadsTotal, (long)g_perspTotal,
              p.valid ? "valid" : "NONE (watch off, or no perspective upload)", p.fovVdeg, p.fovHdeg, p.aspect, p.votes, p.perspUploads, p.allUploads, p.present,
              (unsigned long)g_callThread);
}

void status(d2vr::status::Writer& w)
{
    Projection p; latest(p);
    w.kv("watch", g_watch != 0);
    w.kv("names", g_names != 0);
    w.kv("shaders", (unsigned long)g_shadersSeen);
    w.kv("shadersWithProjection", (unsigned long)g_shadersWithProj);
    w.kv("shadersNoCtab", (unsigned long)g_ctabMissing);
    w.kv("uploads", (unsigned long)g_uploadsTotal);
    w.kv("perspectiveUploads", (unsigned long)g_perspTotal);
    w.kv("valid", p.valid);
    w.kv("fovV", (double)p.fovVdeg);
    w.kv("fovH", (double)p.fovHdeg);
    w.kv("aspect", (double)p.aspect);
    w.kv("votes", (unsigned long)p.votes);
    w.kv("distinct", (unsigned long)p.distinct);
    w.kv("ctabSubstitute", (double)substitute());
    w.kv("substituted", (unsigned long)g_substituted);
    w.kv("callThread", (unsigned long)g_callThread);
}

} // namespace d2vr::vsconst
