#!/usr/bin/env python3
"""Reference PNG files for the terrain part's PNG writer test (UxImaging.PngRgb = Pillow's PNG encoder with zlib-ng).

  ref_png.py <out dir>

Writes <out dir>/png/<name>_<w>x<h>.rgb (raw RGB rows) and .png (what ux_terrain.save_png writes for that image:
Image.save(fp, 'PNG', optimize=False, compress_level=6)) for a set of synthetic images that reach the encoder's corners: tiny
images, noise (stored deflate blocks, many IDAT chunks), constant and repeated rows (long matches), smooth pictures, a row
wider than the encoder buffer. Run with the reference's Python (-I -B); nothing is written outside <out dir>.
"""
import io, sys
from pathlib import Path
import numpy as np
from PIL import Image


def cases():
    rng = np.random.default_rng(20261111)
    yield 'one', np.array([[[7, 8, 9]]], np.uint8)
    yield 'tiny', rng.integers(0, 256, (3, 5, 3), dtype=np.uint8)
    yield 'noise64', rng.integers(0, 256, (64, 64, 3), dtype=np.uint8)
    y, x = np.mgrid[0:480, 0:640]
    g = np.stack([x * 255 // 639, y * 255 // 479, (x + y) * 255 // 1118], -1) + rng.integers(-1, 2, (480, 640, 3))
    yield 'gradient', np.clip(g, 0, 255).astype(np.uint8)
    yield 'noise1024', rng.integers(0, 256, (1024, 1024, 3), dtype=np.uint8)
    yield 'constant', np.full((512, 512, 3), (111, 87, 66), np.uint8)
    yield 'zeros', np.zeros((300, 300, 3), np.uint8)
    yield 'rows', np.tile(rng.integers(0, 256, (8, 2048, 3), dtype=np.uint8), (32, 1, 1))
    w = np.cumsum(np.cumsum(rng.normal(0, 1, (700, 1500, 3)), 0), 1)
    w = (w - w.min()) / (w.max() - w.min()) * 255 + rng.integers(-2, 3, (700, 1500, 3))
    yield 'smooth', np.clip(w, 0, 255).astype(np.uint8)
    yield 'wide', rng.integers(0, 256, (3, 17000, 3), dtype=np.uint8)
    y, x = np.mgrid[0:4096, 0:256]
    yield 'tall', np.stack([y % 256, x, (y // 16) % 256], -1).astype(np.uint8)
    y, x = np.mgrid[0:2048, 0:2048]
    s = np.stack([np.sin(x / 37.0) * np.cos(y / 53.0), np.sin((x + y) / 91.0), np.cos(x / 11.0) * np.sin(y / 7.0)], -1)
    yield 'waves', ((s * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8)
    yield 'sparse', (rng.random((1024, 1024, 3)) < 0.02).astype(np.uint8) * 200


def main():
    out = Path(sys.argv[1]) / 'png'
    out.mkdir(parents=True, exist_ok=True)
    for name, arr in cases():
        arr = np.ascontiguousarray(arr)
        h, w = arr.shape[:2]
        bio = io.BytesIO()
        Image.fromarray(arr).save(bio, 'PNG', optimize=False, compress_level=6)
        (out / ('%s_%dx%d.rgb' % (name, w, h))).write_bytes(arr.tobytes())
        (out / ('%s_%dx%d.png' % (name, w, h))).write_bytes(bio.getvalue())
        print('%-10s %5d x %5d  %8d B' % (name, w, h, len(bio.getvalue())))


if __name__ == '__main__':
    main()
