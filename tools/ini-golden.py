"""Extract the mod's default darkness2_vr.ini as a golden file.

write_default (src/core/config/config.cpp) writes the default ini from one
fprintf literal. This script unescapes that literal from the WORKING TREE so
tests/golden/darkness2_vr.ini is the file a fresh install gets; a change to the
literal without a regenerated golden fails --check, and tools/install.ps1
diffs every installed ini against the golden before a launch.

Usage (from the repo root):
    python tools/ini-golden.py                # writes tests/golden/darkness2_vr.ini
    python tools/ini-golden.py --check FILE   # diff FILE against the source literal
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden", "darkness2_vr.ini")
SOURCE = os.path.join(ROOT, "src", "core", "config", "config.cpp")


def extract():
    src = io.open(SOURCE, encoding="utf-8").read()
    m = re.search(r"bool write_default\(const char\* path\)\n\{.*?fprintf\(f,\n(.*?),\n\s*kConfigVersion\);", src, re.S)
    if not m:
        sys.exit("write_default literal not found")
    ver = re.search(r"const int kConfigVersion = (\d+);", src).group(1)
    body = m.group(1)
    text = ""
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith('"'):
            continue
        lit = line[1:line.rfind('"')]
        text += re.sub(r'\\(.)', lambda e: {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(e.group(1), e.group(1)), lit)
    # Spend the fprintf conversions the way fprintf does, in ONE left-to-right pass.
    out, i = [], 0
    while i < len(text):
        if text[i] == "%" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "%":
                out.append("%"); i += 2; continue
            if nxt == "d":
                out.append(ver); i += 2; continue
        out.append(text[i]); i += 1
    return "".join(out)


def main():
    text = extract()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        got = io.open(sys.argv[2], encoding="utf-8").read().replace("\r\n", "\n")
        want = text.replace("\r\n", "\n")
        if got == want:
            print("ini golden: MATCH")
            return 0
        import difflib
        sys.stdout.writelines(difflib.unified_diff(want.splitlines(True), got.splitlines(True), "source literal", sys.argv[2]))
        return 1
    os.makedirs(os.path.dirname(GOLDEN), exist_ok=True)
    io.open(GOLDEN, "w", encoding="utf-8", newline="\n").write(text)
    print(f"wrote {GOLDEN}: {text.count(chr(10))} lines, {text.count('[')} sections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
