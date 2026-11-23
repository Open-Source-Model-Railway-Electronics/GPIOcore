#!/usr/bin/env python3
"""
Makes the program icon from the Open Source Hardware symbol (the gear with the keyhole) in KiCad's footprint
library: ui/oshw.svg (window icon) and ui/oshw.ico (exe, shortcuts, installer). Standard library only.

    python make_icon.py [path to OSHW-Symbol_*.kicad_mod]

Without a path it looks for KiCad in Program Files. The result is committed; run this only to change it.
"""

import glob
import re
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
COLOR = (0x3B, 0x9C, 0x3B)          # green
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
PAD = 0.04                          # margin around the symbol, part of the size
SUB = 4                             # anti-aliasing: SUB x SUB samples per pixel


def find_footprint():
    found = sorted(glob.glob(r"C:\Program Files\KiCad\*\share\kicad\footprints\Symbol.pretty"
                             r"\OSHW-Symbol_44.5x40mm_SilkScreen.kicad_mod"))
    if not found:
        sys.exit("[FAIL] KiCad's OSHW-Symbol footprint not found: give its path")
    return found[-1]


def read_polygons(path):
    text = Path(path).read_text(encoding="utf-8")
    polys = []
    for block in re.findall(r"\(fp_poly\s*\(pts((?:\s*\(xy[^)]*\))+)", text):
        pts = [(float(x), float(y)) for x, y in re.findall(r"\(xy\s+([-\d.]+)\s+([-\d.]+)\)", block)]
        if len(pts) >= 3:
            polys.append(pts)
    if not polys:
        sys.exit(f"[FAIL] no polygons in {path}")
    # into a unit square, centred, keeping the aspect ratio
    xs = [x for p in polys for x, _ in p]
    ys = [y for p in polys for _, y in p]
    w, h = max(xs) - min(xs), max(ys) - min(ys)
    scale = (1 - 2 * PAD) / max(w, h)
    ox = PAD + (1 - 2 * PAD - w * scale) / 2 - min(xs) * scale
    oy = PAD + (1 - 2 * PAD - h * scale) / 2 - min(ys) * scale
    return [[(ox + x * scale, oy + y * scale) for x, y in p] for p in polys]


def rasterize(polys, size):
    """Coverage 0..SUB*SUB per pixel; each polygon filled even-odd, polygons combined."""
    n = size * SUB
    cover = [0] * (size * size)
    edges = [[(p[i], p[(i + 1) % len(p)]) for i in range(len(p))] for p in polys]
    for row in range(n):
        y = (row + 0.5) / n
        inside = bytearray(n)
        for poly in edges:
            xs = []
            for (x1, y1), (x2, y2) in poly:
                if (y1 <= y) != (y2 <= y):
                    xs.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                for c in range(max(0, int(a * n + 0.5)), min(n, int(b * n + 0.5))):
                    inside[c] = 1
        base = (row // SUB) * size
        for c in range(n):
            if inside[c]:
                cover[base + c // SUB] += 1
    return cover


def png(size, cover):
    raw = bytearray()
    for y in range(size):
        raw.append(0)                                   # filter: none
        for x in range(size):
            a = cover[y * size + x] * 255 // (SUB * SUB)
            raw += bytes((*COLOR, a))
    chunk = lambda kind, data: (struct.pack(">I", len(data)) + kind + data
                                + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def ico(images):
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, data = b"", b""
    for size, blob in images:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return head + entries + data


def svg(polys):
    paths = " ".join("M" + " L".join(f"{x * 256:.2f},{y * 256:.2f}" for x, y in p) + " Z" for p in polys)
    color = "#%02x%02x%02x" % COLOR
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">\n'
            f'  <path fill="{color}" fill-rule="evenodd" d="{paths}"/>\n</svg>\n')


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else find_footprint()
    polys = read_polygons(path)
    (HERE / "oshw.svg").write_text(svg(polys), encoding="utf-8")
    (HERE / "oshw.ico").write_bytes(ico([(s, png(s, rasterize(polys, s))) for s in SIZES]))
    print(f"[OK] {len(polys)} polygons from {Path(path).name} -> oshw.svg, oshw.ico ({len(SIZES)} sizes)")


if __name__ == "__main__":
    main()
