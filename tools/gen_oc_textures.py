#!/usr/bin/env python3
"""gen_oc_textures.py -- writes the two halftone pictures the front end's style sheets use (the same pictures OcTex in
Assets/Scripts/OpenFrontEnd/OuCareerKit.cs draws at run time, for the USS rules that cannot call code):
  Resources/OpenFrontEnd/Textures/OcTile.png   16 x 16 dot tile (buttons, plaques; background-repeat)
  Resources/OpenFrontEnd/Textures/OcDots.png   1024 x 576 big halftone blots (stage / dim layers)
White dots on transparent; USS tints them. Pure standard library."""
import math, struct, sys, zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "Unity/Reflex/Assets/OpenFrontEnd/Resources/OpenFrontEnd/Textures"

def png(path, w, h, rgba):
    raw = b"".join(b"\x00" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, d): c = struct.pack(">I", len(d)) + t + d; return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))

def clamp01(v): return 0.0 if v < 0 else 1.0 if v > 1 else v

def tile():
    n = 16; px = bytearray(n * n * 4)
    for y in range(n):
        for x in range(n):
            cx, cy = x + 0.5, y + 0.5
            d1 = math.hypot(cx - n * 0.5, cy - n * 0.5)
            d2 = min(math.hypot(cx, cy), math.hypot(cx - n, cy), math.hypot(cx, cy - n), math.hypot(cx - n, cy - n))
            a = max(clamp01(3.4 - d1 + 0.5), clamp01(3.4 - d2 + 0.5))
            i = (y * n + x) * 4; px[i:i + 4] = bytes((255, 255, 255, int(a * 255)))
    return n, n, px

def dots():
    w, h, pitch = 1024, 576, 15.0
    px = bytearray(w * h * 4)
    c, s = math.cos(math.pi / 4), math.sin(math.pi / 4)
    for y in range(h):
        for x in range(w):
            rx, ry = (x * c - y * s) / pitch, (x * s + y * c) / pitch
            fx, fy = rx - math.floor(rx) - 0.5, ry - math.floor(ry) - 0.5
            d = math.hypot(fx, fy) * pitch
            n = 0.5 + 0.5 * (math.sin(x * 0.0061 + 1.3) * 0.6 + math.sin(y * 0.0083 + x * 0.0021 + 0.4) * 0.5 + math.sin((x + y) * 0.0034 + 2.2) * 0.4) / 1.2
            cover = clamp01((n - 0.34) * 2.2)
            r = pitch * 0.62 * cover
            a = clamp01((r - d) * 1.6 + 0.5)
            i = (y * w + x) * 4; px[i:i + 4] = bytes((255, 255, 255, int(a * 255)))
    return w, h, px

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in (("OcTile.png", tile), ("OcDots.png", dots)):
        w, h, px = fn(); png(OUT / name, w, h, px); print("wrote", OUT / name, w, h)
