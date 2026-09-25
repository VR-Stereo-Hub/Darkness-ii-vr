"""extract.py - read-only reader for The Darkness II's .toc/.cache pairs (R5, VR-236).

The container (docs/darkness2/GAME_ASSETS.md section 2): a .toc is an 8-byte header
(magic 0x1867C64E, version 16) followed by 96-byte entries {int64 offset (-1 = directory),
int64 FILETIME, int32 compressedLen, int32 len, int32 reserved, int32 parentDirIndex,
char[64] name}; the .cache holds the entries back to back, a compressed entry as blocks of
[u16 BE compLen][u16 BE rawLen][payload], LZF when the two lengths differ, raw when equal.
Three prefixes hold three parts of one asset path: H (header), B (body), F (full-res).

Everything this tool writes is GAME-DERIVED CONTENT and never enters the repository:
tools/cache-out/ and tools/lua/ are gitignored. Findings go to docs/darkness2/.

Usage (from the repo root; the game dir comes from D2VR_GAME_DIR or Steam's libraryfolders.vdf;
under Git Bash set MSYS_NO_PATHCONV=1 or the /D2/... argument becomes a Windows path):
    python tools/cache/extract.py list [--filter TEXT] [--cache NAME]
    python tools/cache/extract.py extract /D2/Scripts/Player/SetFov.lua [--out DIR]
    python tools/cache/extract.py lua-dump [--out tools/lua]
    python tools/cache/extract.py skel /D2/Characters/Camera/FPDJackie_skel.fbx
"""
import argparse
import io
import os
import re
import struct
import sys
from collections import defaultdict

MAGIC = 0x1867C64E
ENTRY = 96
LUA_SIG = b"\x1bLuaQ"
BONE_HINTS = [b"GAME_C1_CAMERA", b"GAME_C1_ROOT", b"GAME_L1_DEMONARM1", b"GAME_R1_DEMONARM1",
              b"WEAPON1", b"GAME_x1_TENTACLE_CLAV", b"CLAV1", b"ARM1", b"ARM2"]
IDENT = re.compile(rb"[A-Za-z0-9_.:\-]+")


# ------------------------------------------------------------------ game dir
def game_dir():
    env = os.environ.get("D2VR_GAME_DIR")
    if env and os.path.exists(os.path.join(env, "DarknessII.exe")):
        return env
    steam = None
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
            steam = winreg.QueryValueEx(k, "SteamPath")[0]
    except Exception:
        steam = r"C:\Program Files (x86)\Steam"
    libs = [steam]
    vdf = os.path.join(steam, "steamapps", "libraryfolders.vdf")
    if os.path.exists(vdf):
        for line in io.open(vdf, encoding="utf-8", errors="replace"):
            m = re.match(r'\s*"path"\s+"(.+)"', line)
            if m:
                libs.append(m.group(1).replace("\\\\", "\\"))
    for lib in libs:
        d = os.path.join(lib, "steamapps", "common", "Darkness II")
        if os.path.exists(os.path.join(d, "DarknessII.exe")):
            return d
    sys.exit("The Darkness II is not installed in any Steam library; set D2VR_GAME_DIR")


# ------------------------------------------------------------------ LZF
def lzf_decompress(src, out_len):
    out = bytearray()
    i, n = 0, len(src)
    while i < n and len(out) < out_len:
        ctrl = src[i]; i += 1
        if ctrl < 32:                      # literal run of ctrl+1 bytes
            out += src[i:i + ctrl + 1]; i += ctrl + 1
        else:                              # back reference
            length = ctrl >> 5
            if length == 7:
                length += src[i]; i += 1
            ref = len(out) - ((ctrl & 0x1F) << 8) - src[i] - 1; i += 1
            for _ in range(length + 2):
                out.append(out[ref]); ref += 1
    return bytes(out)


# ------------------------------------------------------------------ TOC
class Entry:
    __slots__ = ("cache", "index", "offset", "filetime", "clen", "len", "parent", "name", "path", "is_dir")


def read_toc(toc_path):
    data = open(toc_path, "rb").read()
    magic, version = struct.unpack_from("<II", data, 0)
    if magic != MAGIC:
        sys.exit(f"{toc_path}: bad magic 0x{magic:08X}")
    count = (len(data) - 8) // ENTRY
    entries = []
    for i in range(count):
        o = 8 + i * ENTRY
        off, ft, clen, ln, _res, parent = struct.unpack_from("<qqiiii", data, o)
        name = data[o + 32:o + 96].split(b"\0", 1)[0].decode("latin-1")
        e = Entry(); e.index = i; e.offset = off; e.filetime = ft; e.clen = clen; e.len = ln
        e.parent = parent; e.name = name; e.is_dir = off == -1; e.path = None
        entries.append(e)
    # full paths. parentDirIndex is a 1-BASED index into the DIRECTORY entries (offset == -1)
    # in their own numbering, 0 meaning the root: in B.Script.toc `Shaders` has parent 1 (`EE`,
    # the first directory) and `PassThrough.hlsl` has parent 3 (`PostFX`). With all-entry
    # numbering the 102,721 entries gave 83,651 nonsense paths; with 0-based directory numbering
    # 58,564; with this rule the documented 47,425 (GAME_ASSETS.md s3).
    dirs = [e for e in entries if e.is_dir]
    for e in entries:
        parts = []; cur = e; guard = 0
        while cur is not None and guard < 64:
            if cur.name:
                parts.append(cur.name)
            cur = dirs[cur.parent - 1] if 1 <= cur.parent <= len(dirs) else None
            guard += 1
        e.path = "/" + "/".join(reversed(parts))
    return version, entries


def load_all(gdir, only=None):
    """{cache name: (version, entries)} for every Cache.Windows/*.toc (and Cache.DLC)."""
    result = {}
    for sub in ("Cache.Windows", "Cache.DLC"):
        d = os.path.join(gdir, sub)
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if not f.endswith(".toc"):
                continue
            name = f[:-4]
            if only and name != only:
                continue
            version, entries = read_toc(os.path.join(d, f))
            for e in entries:
                e.cache = os.path.join(d, name)
            result[name] = (version, entries)
    return result


def read_entry(e):
    """The decompressed bytes of one file entry."""
    with open(e.cache + ".cache", "rb") as f:
        f.seek(e.offset)
        raw = f.read(e.clen)
    if e.clen == e.len:
        return raw
    out = bytearray(); i = 0
    while i + 4 <= len(raw) and len(out) < e.len:
        clen, rlen = struct.unpack_from(">HH", raw, i); i += 4
        block = raw[i:i + clen]; i += clen
        out += block if clen == rlen else lzf_decompress(block, rlen)
    return bytes(out[:e.len])


# ------------------------------------------------------------------ commands
def cmd_list(a):
    caches = load_all(game_dir(), a.cache)
    paths = defaultdict(list)
    total = 0
    for name, (version, entries) in caches.items():
        for e in entries:
            if e.is_dir:
                continue
            total += 1
            paths[e.path].append((name, e))
    shown = 0
    for p in sorted(paths):
        if a.filter and a.filter.lower() not in p.lower():
            continue
        parts = ", ".join(f"{n}:{e.len}" for n, e in paths[p])
        print(f"{p}  [{parts}]")
        shown += 1
    print(f"# {len(caches)} caches, {total} file entries, {len(paths)} unique paths" +
          (f", {shown} shown for '{a.filter}'" if a.filter else ""), file=sys.stderr)


def find_path(caches, wanted):
    hits = []
    for name, (version, entries) in caches.items():
        for e in entries:
            if e.is_dir:
                continue
            if e.path.lower() == wanted.lower() or e.path.lower().endswith(wanted.lower()):
                hits.append((name, e))
    return hits


def cmd_extract(a):
    caches = load_all(game_dir())
    hits = find_path(caches, a.path)
    if not hits:
        sys.exit(f"no entry matches {a.path}")
    os.makedirs(a.out, exist_ok=True)
    base = os.path.basename(hits[0][1].path)
    for name, e in sorted(hits):
        part = name.split(".")[0]              # H, B or F
        data = read_entry(e)
        dst = os.path.join(a.out, f"{base}.{part}")
        open(dst, "wb").write(data)
        print(f"{name}: {e.path} -> {dst} ({len(data)} bytes; toc len {e.len}, compressed {e.clen})")


def lua_source(blob):
    """The script text embedded in a compiled .lua entry, or None.

    Measured on /D2/Scripts/Player/SetFov.lua: the B part is a Lua 5.1 chunk (12-byte header
    1B 4C 75 61 51 00 01 04 04 04 04 00) whose top-level function's SOURCE NAME field (a
    size_t length, then the bytes, NUL-terminated) holds the entire script text, not a file
    name. The H part carries the asset path and the same text. So the source is read from the
    chunk header exactly; the printable-run heuristic is the fallback for anything else.
    """
    if blob.startswith(LUA_SIG) and len(blob) >= 16:
        n = struct.unpack_from("<I", blob, 12)[0]
        if 0 < n <= len(blob) - 16:
            s = blob[16:16 + n].rstrip(b"\x00")
            if s and all(c in (9, 10, 13) or 0x20 <= c < 0x7F for c in s):
                return s.decode("latin-1")
    best = None
    for m in re.finditer(rb"[\x09\x0a\x0d\x20-\x7e]{40,}", blob):
        s = m.group()
        score = len(s) + (10000 if re.search(rb"\b(function|local|end|if|then|return)\b", s) else 0)
        if best is None or score > best[0]:
            best = (score, s)
    return best[1].decode("latin-1") if best and best[0] >= 10000 else None


def cmd_lua_dump(a):
    caches = load_all(game_dir())
    os.makedirs(a.out, exist_ok=True)
    n_ok = n_fail = 0
    seen = set()
    for name, (version, entries) in caches.items():
        if not name.startswith("B."):
            continue
        for e in entries:
            if e.is_dir or not e.path.lower().endswith(".lua") or e.path in seen:
                continue
            seen.add(e.path)
            blob = read_entry(e)
            src = lua_source(blob)
            dst = os.path.join(a.out, e.path.strip("/").replace("/", os.sep))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if src:
                io.open(dst, "w", encoding="latin-1", newline="\n").write(src)
                n_ok += 1
            else:
                open(dst + ".bin", "wb").write(blob)
                n_fail += 1
    print(f"lua-dump: {n_ok} scripts as source, {n_fail} left as .bin (no readable source found) under {a.out}")


def walk_lp_strings(blob, pos, limit=2000):
    """[u32 len][chars] records from pos while they look like identifiers."""
    out = []
    while pos + 4 <= len(blob) and len(out) < limit:
        n = struct.unpack_from("<I", blob, pos)[0]
        if not (1 <= n <= 64):
            break
        s = blob[pos + 4:pos + 4 + n]
        if len(s) != n or not IDENT.fullmatch(s):
            break
        out.append(s.decode())
        pos += 4 + n
    return out, pos


def cmd_skel(a):
    """The bone table of a _skel.fbx blob.

    Measured on /D2/Characters/Camera/FPDJackie_skel.fbx (2026-09-25): the H part starts with
    [u32 1][u32 len][asset path], then [u32 len][a text block "Materials={...}"], then the bone
    names as consecutive [u32 len][chars] records, then a u32 that equals the record count
    (the bone count), then floats 10, 20, 30 (LOD distances by their values). The B part
    carries no names. Everything after that is reported as raw words for the next session.
    """
    caches = load_all(game_dir())
    hits = find_path(caches, a.path)
    if not hits:
        sys.exit(f"no entry matches {a.path}")
    for name, e in sorted(hits):
        blob = read_entry(e)
        print(f"== {name}: {e.path} ({len(blob)} bytes)")
        found = sorted((blob.find(h), h) for h in BONE_HINTS if blob.find(h) >= 0)
        if not found:
            print("   no known bone names in this part")
            continue
        print("   bone-name hits: " + ", ".join(f"{h.decode()}@0x{o:X}" for o, h in found))
        # The longest run of length-prefixed identifiers that covers the first hit.
        first = found[0][0]
        best = ([], first, first)
        for st in range(max(0, first - 4096), first):
            names, endpos = walk_lp_strings(blob, st)
            if endpos > first and len(names) > len(best[0]):
                best = (names, st, endpos)
        names, st, endpos = best
        print(f"   bone table: {len(names)} length-prefixed names from 0x{st:X} to 0x{endpos:X}")
        for i, s in enumerate(names):
            print(f"   [{i:3}] {s}")
        tail = blob[endpos:endpos + 64]
        words = struct.unpack("<" + "I" * (len(tail) // 4), tail[:len(tail) // 4 * 4])
        floats = struct.unpack("<" + "f" * (len(tail) // 4), tail[:len(tail) // 4 * 4])
        print("   after the table (u32): " + " ".join(f"{v:#x}" for v in words))
        print("   after the table (f32): " + " ".join(f"{v:.3g}" for v in floats))
        if len(names) in words:
            print(f"   the u32 {len(names)} follows the table: the bone COUNT")
        # The hierarchy table, later in the H part: [u32 count] then records of
        # [u32 len][name][u16 parentIndex][u16 descendantCount]. Measured on FPDJackie: 164
        # records, GAME_C1_ROOT first with 163 descendants, GAME_L1_LEG1 parent 1 (HIP1) with 4.
        hier = []
        pos = endpos
        while pos + 8 < len(blob):
            n = struct.unpack_from("<I", blob, pos)[0]
            if 1 <= n <= 64 and IDENT.fullmatch(blob[pos + 4:pos + 4 + n] or b"-"):
                cnt = struct.unpack_from("<I", blob, pos - 4)[0] if pos >= 4 else 0
                q = pos
                while q + 8 <= len(blob) and len(hier) < 4096:
                    n = struct.unpack_from("<I", blob, q)[0]
                    if not (1 <= n <= 64):
                        break
                    s = blob[q + 4:q + 4 + n]
                    if len(s) != n or not IDENT.fullmatch(s):
                        break
                    parent, desc = struct.unpack_from("<HH", blob, q + 4 + n)
                    hier.append((s.decode(), parent, desc))
                    q += 4 + n + 4
                if len(hier) >= 8:
                    print(f"   hierarchy table: {len(hier)} records from 0x{pos:X} (count word before it: {cnt})")
                    for i, (s, parent, desc) in enumerate(hier):
                        print(f"   h[{i:3}] {s:<24} parent={parent:<3} descendants={desc}")
                    break
                hier = []
            pos += 1
        if not hier:
            print("   no hierarchy table found after the bone table")
        has_cam = any(s == "GAME_C1_CAMERA" for s, _, _ in hier) or "GAME_C1_CAMERA" in names
        print(f"   GAME_C1_CAMERA present in this rig: {has_cam}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    l = sub.add_parser("list"); l.add_argument("--filter", default=""); l.add_argument("--cache", default=None); l.set_defaults(fn=cmd_list)
    x = sub.add_parser("extract"); x.add_argument("path"); x.add_argument("--out", default=os.path.join("tools", "cache-out")); x.set_defaults(fn=cmd_extract)
    d = sub.add_parser("lua-dump"); d.add_argument("--out", default=os.path.join("tools", "lua")); d.set_defaults(fn=cmd_lua_dump)
    s = sub.add_parser("skel"); s.add_argument("path"); s.set_defaults(fn=cmd_skel)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
