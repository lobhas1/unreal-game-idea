#!/usr/bin/env python3
"""Extract the renderer's testimony from a saved Unreal Output Log.

Phase 3, step 1: take a saved Output Log, pull every line containing the
"REPLAY| " marker, strip everything up to and including that marker, and write
the remainder verbatim. The result is the <replay-name>.rendered.txt that the
gate diffs (byte-for-byte) against the C# reference projection.

Usage:
    python scripts/extract-rendered.py <input.log> <output.rendered.txt>

Notes:
  - Output is written with LF line endings and a single trailing newline.
  - We match the LAST occurrence of "REPLAY| " on a line (defensive against a
    log-category prefix that itself somehow contained the token); in practice
    UE_LOG writes "LogReplay: REPLAY| <canonical>" so the first match is fine.
"""
import sys

MARKER = "REPLAY| "

def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    out = []
    with open(src, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            idx = line.find(MARKER)
            if idx == -1:
                continue
            rendered = line[idx + len(MARKER):]
            out.append(rendered.rstrip("\r\n"))
    with open(dst, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(out) + ("\n" if out else ""))
    print(f"{src} -> {dst}: {len(out)} REPLAY| lines")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
