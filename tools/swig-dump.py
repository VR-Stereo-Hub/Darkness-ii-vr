# swig-dump.py - walk the SWIG 1.3 Lua registration tables inside DarknessII.exe
# (offline, read-only) and write docs/darkness2/swig-api.md: every module, class,
# base, method and attribute reachable from Lua, with the wrapper VA of each.
#
# R2 (docs/ROADMAP.md, VR-233). Names and addresses only: the game's own
# identifier strings are quotable, and nothing else leaves the exe. The layouts
# were measured with tools/disasm-rva.py on 2026-09-25 (ENGINE_NOTES s3, s8):
#
#   swig_module_info  { swig_type_info** types; size_t size; next; swig_type_info** type_initial;
#                       cast_initial; clientdata }                      (6 dwords)
#   swig_type_info    { char* name; char* str; dcast; cast; void* clientdata; int owndata }
#                                                                     (24 bytes)
#   swig_lua_class    { char* name; swig_type_info** type; ctor; dtor; luaL_Reg* methods;
#                       swig_lua_attribute* attributes; swig_lua_class** bases; char** base_names }
#                                                                     (32 bytes)
#   luaL_Reg          { char* name; lua_CFunction fn }                 (8 bytes, NULL-terminated)
#   swig_lua_attribute{ char* name; getter; setter }                   (12 bytes, NULL-terminated)
#
# The runtime `types` array lives in BSS (filled by SWIG_InitializeModule), so
# the static `type_initial` array is walked instead. A class's clientdata is its
# swig_lua_class; a plain pointer type (`_p_float`) has none and is listed as a type.
#
# Usage:
#   python tools/swig-dump.py                       # finds the exe through Steam, writes the doc
#   python tools/swig-dump.py <exe> [--out <md>] [--check]
# --check re-walks and compares the totals against the doc's header (CI-style).
# Requires nothing beyond the standard library (the PE reader is disasm-rva.py's).
import argparse
import datetime
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
_spec = importlib.util.spec_from_file_location("disasm_rva", os.path.join(os.path.dirname(os.path.abspath(__file__)), "disasm-rva.py"))
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
Pe = _mod.Pe

# The ten SWIG modules (luaopen_<name>): SWIG_init, swig_module_info, type_initial.
# Found by the `"swig_type"` string push each SWIG_init makes (ENGINE_NOTES s8).
MODULES = [
    ("Engine",      0x474E80, 0x10B8F94, 0x10BD308),
    ("GraphicsRes", 0x6221D0, 0x10A4214, 0x10A4810),
    ("Effects",     0x724EF0, 0x10C2BEC, 0x10C3318),
    ("Game",        0x7C20A0, 0x10AD424, 0x10AECE0),
    ("Sound",       0x885C80, 0x10A856C, 0x10A8584),
    ("Npc",         0x97F210, 0x10D374C, 0x10D3F30),
    ("UISys",       0xB4B4B0, 0x10B17C4, 0x10B24C0),
    ("Script",      0xB77C80, 0x10A02CC, 0x10A0B40),
    ("Framework",   0xCE80A0, 0x10A60AC, 0x10A6920),
    ("D2_Game",     0xB3C020, 0x10C5AB4, 0x10C8720),
]
# The Script module's global commands (IsNull, Lerp, Broadcast, ...): a luaL_Reg table.
SCRIPT_GLOBALS_VA = 0xFB9E60
EXPECTED = (0x4F68A875, 0xDEC000, 14291512)


def default_exe():
    for lib in ("D:\\SteamLibrary", "C:\\Program Files (x86)\\Steam"):
        p = os.path.join(lib, "steamapps", "common", "Darkness II", "DarknessII.exe")
        if os.path.exists(p):
            return p
    env = os.environ.get("D2VR_GAME_DIR")
    if env and os.path.exists(os.path.join(env, "DarknessII.exe")):
        return os.path.join(env, "DarknessII.exe")
    raise SystemExit("DarknessII.exe not found: pass its path or set D2VR_GAME_DIR")


class Walker:
    def __init__(self, pe):
        self.pe = pe
        self.base = pe.image_base

    def u32(self, va):
        b = self.pe.read(va - self.base, 4)
        return struct.unpack_from("<I", b)[0] if len(b) == 4 else None

    def cstr(self, va, cap=256):
        if not va:
            return None
        b = self.pe.read(va - self.base, cap)
        if not b:
            return None
        n = b.find(b"\0")
        s = b[: n if n >= 0 else cap]
        try:
            return s.decode("ascii")
        except UnicodeDecodeError:
            return None

    def reg_table(self, va):
        """luaL_Reg {name, fn} until name == NULL. Empty when the table lives in BSS."""
        out = []
        if not va:
            return out
        for i in range(4096):
            name_p = self.u32(va + 8 * i)
            if name_p is None or name_p == 0:
                break
            fn = self.u32(va + 8 * i + 4)
            name = self.cstr(name_p)
            if name is None:
                break
            out.append((name, fn))
        return out

    def attr_table(self, va):
        out = []
        if not va:
            return out
        for i in range(4096):
            name_p = self.u32(va + 12 * i)
            if name_p is None or name_p == 0:
                break
            getter = self.u32(va + 12 * i + 4)
            setter = self.u32(va + 12 * i + 8)
            name = self.cstr(name_p)
            if name is None:
                break
            out.append((name, getter, setter))
        return out

    def str_list(self, va):
        out = []
        if not va:
            return out
        for i in range(64):
            p = self.u32(va + 4 * i)
            if not p:
                break
            s = self.cstr(p)
            if s is None:
                break
            out.append(s)
        return out

    def module(self, name, init_va, mod_va, type_initial_va):
        size = self.u32(mod_va + 4)
        # On disk the module struct's types/type_initial slots are 0 (SWIG_InitializeModule
        # fills them at startup); the static array's VA comes from the table above, and a
        # non-zero on-disk value that disagrees is reported.
        ti = self.u32(mod_va + 12)
        if ti and ti != type_initial_va:
            print("WARNING: %s: swig_module.type_initial on disk is 0x%X, the table says 0x%X - using the disk value" % (name, ti, type_initial_va))
            type_initial_va = ti
        classes, plain = [], []
        for i in range(size or 0):
            tinfo = self.u32(type_initial_va + 4 * i)
            if not tinfo:
                continue
            tname = self.cstr(self.u32(tinfo) or 0) or "?"
            client = self.u32(tinfo + 16) or 0
            if not client:
                plain.append(tname)
                continue
            cname = self.cstr(self.u32(client) or 0) or "?"
            ctor = self.u32(client + 8) or 0
            dtor = self.u32(client + 12) or 0
            methods = self.reg_table(self.u32(client + 16) or 0)
            attrs = self.attr_table(self.u32(client + 20) or 0)
            bases = self.str_list(self.u32(client + 28) or 0)
            classes.append({"name": cname, "type": tname, "class_va": client, "ctor": ctor, "dtor": dtor,
                            "methods": methods, "attributes": attrs, "bases": bases})
        classes.sort(key=lambda c: c["name"])
        return {"name": name, "init": init_va, "module": mod_va, "type_initial": type_initial_va,
                "size": size, "classes": classes, "plain": sorted(plain)}


def hexva(v):
    return "0x%08X" % v if v else "-"


def render(pe, mods, globals_):
    total_c = sum(len(m["classes"]) for m in mods)
    total_m = sum(len(c["methods"]) for m in mods for c in m["classes"])
    total_a = sum(len(c["attributes"]) for m in mods for c in m["classes"])
    out = []
    out.append("# The SWIG engine API reachable from Lua (R2, VR-233)\n")
    out.append("Generated by `tools/swig-dump.py` on %s from `DarknessII.exe` build 2012-03-20 "
               "(TimeDateStamp 0x%X, SizeOfImage 0x%X, %d bytes). Names and wrapper addresses only; "
               "the game's own identifier strings are quotable, nothing else from the exe is here.\n"
               % (datetime.date.today().isoformat(), EXPECTED[0], EXPECTED[1], EXPECTED[2]))
    out.append("**Totals: %d modules, %d classes, %d methods, %d attributes.** The mod's `lua swigcheck` "
               "seam word re-reads the ten `swig_module_info` structs at runtime and compares class counts "
               "and the first method name of each module against these tables (the byte-verify).\n"
               % (len(mods), total_c, total_m, total_a))
    out.append("Each wrapper is a `lua_CFunction` (cdecl, one `lua_State*` argument): the SWIG glue that "
               "converts the Lua arguments and calls the engine method (`SetBaseFovOverride` reads the "
               "object pointer through its swig type, the number with `lua_tonumber`, and calls the "
               "camera controller's vtable slot 0xE4). Calling a wrapper from C with a prepared stack is a "
               "native route that bypasses script; ENGINE_NOTES s3.\n")
    out.append("| Module | `luaopen_` (SWIG_init) | `swig_module_info` | `type_initial` | types | classes | methods | attributes |")
    out.append("|---|---|---|---|---|---|---|---|")
    for m in mods:
        out.append("| %s | %s | %s | %s | %d | %d | %d | %d |" % (
            m["name"], hexva(m["init"]), hexva(m["module"]), hexva(m["type_initial"]), m["size"] or 0,
            len(m["classes"]), sum(len(c["methods"]) for c in m["classes"]),
            sum(len(c["attributes"]) for c in m["classes"])))
    out.append("")
    if globals_:
        out.append("## Global functions (the `Script` module's command table at %s)\n" % hexva(SCRIPT_GLOBALS_VA))
        out.append("| Function | Wrapper |")
        out.append("|---|---|")
        for n, fn in globals_:
            out.append("| `%s` | %s |" % (n, hexva(fn)))
        out.append("")
    for m in mods:
        out.append("## Module `%s`\n" % m["name"])
        if m["plain"]:
            out.append("Plain pointer types (no class): %s\n" % ", ".join("`%s`" % p for p in m["plain"]))
        for c in m["classes"]:
            base = ", ".join("`%s`" % b for b in c["bases"]) if c["bases"] else "-"
            out.append("### `%s`\n" % c["name"])
            out.append("swig type `%s`, class struct %s, bases %s, ctor %s, dtor %s, %d methods, %d attributes\n"
                       % (c["type"], hexva(c["class_va"]), base, hexva(c["ctor"]), hexva(c["dtor"]),
                          len(c["methods"]), len(c["attributes"])))
            if c["methods"]:
                out.append("| Method | Wrapper |")
                out.append("|---|---|")
                for n, fn in c["methods"]:
                    out.append("| `%s` | %s |" % (n, hexva(fn)))
                out.append("")
            if c["attributes"]:
                out.append("| Attribute | Getter | Setter |")
                out.append("|---|---|---|")
                for n, g, s in c["attributes"]:
                    out.append("| `%s` | %s | %s |" % (n, hexva(g), hexva(s)))
                out.append("")
    return "\n".join(out) + "\n", (len(mods), total_c, total_m, total_a)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe", nargs="?", default=None)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                                  "docs", "darkness2", "swig-api.md"))
    ap.add_argument("--check", action="store_true", help="compare the totals with the existing doc, write nothing")
    a = ap.parse_args()
    exe = a.exe or default_exe()
    pe = Pe(exe)
    stamp = struct.unpack_from("<I", pe.data, struct.unpack_from("<I", pe.data, 0x3C)[0] + 8)[0]
    if stamp != EXPECTED[0] or pe.size_of_image != EXPECTED[1] or len(pe.data) != EXPECTED[2]:
        raise SystemExit("this is not the build the table VAs were derived on (stamp 0x%X size 0x%X bytes %d)"
                         % (stamp, pe.size_of_image, len(pe.data)))
    w = Walker(pe)
    mods = [w.module(*m) for m in MODULES]
    globals_ = w.reg_table(SCRIPT_GLOBALS_VA)
    text, totals = render(pe, mods, globals_)
    print("modules %d, classes %d, methods %d, attributes %d, globals %d" % (totals + (len(globals_),)))
    if a.check:
        if not os.path.exists(a.out):
            raise SystemExit("no doc to check at %s" % a.out)
        head = open(a.out, encoding="utf-8").read(2000)
        want = "%d classes, %d methods, %d attributes" % totals[1:]
        if want not in head:
            raise SystemExit("MISMATCH: the doc's header does not say '%s'" % want)
        print("swig-api.md matches the exe")
        return
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote %s" % a.out)


if __name__ == "__main__":
    main()
