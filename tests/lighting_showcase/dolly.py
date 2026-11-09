#!/usr/bin/env python3
"""dolly.py <run dir> [--roi x0,y0,x1,y1] [--blur N] [--out DIR] -- pops of a slow dolly (cam "path", `step` small, no turn) in the far field.

The camera moves a few tenths of a unit per frame, so everything beyond ~100 units moves by under a pixel between two frames: the image of the
far field changes only where something changed on its own - a tree dropping to its next LOD, its dissolve into the billboard, a billboard
changing its frame, a shadow appearing. Per sequence (prefix, view, mode) over the ROI (default: the horizon band) of consecutive frames:
the mean of the |difference| of the N px box-blurred frames (leaf-edge aliasing averages out, a changed tree does not), the 99th percentile,
the share of pixels over --thresh, and the frame pair with the most of them (its blurred difference saved with --out). Also the number of
frame pairs whose changed share exceeds 3x the sequence's median (the "events")."""
import argparse, re
from collections import defaultdict
from pathlib import Path
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("run", type=Path); ap.add_argument("--out", type=Path)
ap.add_argument("--roi", default="0.0,0.10,1.0,0.50"); ap.add_argument("--blur", type=int, default=9); ap.add_argument("--thresh", type=float, default=0.02)
ap.add_argument("--views", default="dolly")
a = ap.parse_args()
shots = a.run / "screenshots"
rx = re.compile(r"^(.*?)_([A-Za-z0-9]+)_([a-z0-9-]+)_f(\d\d+)\.png$")
seqs = defaultdict(dict)
for p in sorted(shots.glob("*_f??.png")):
    m = rx.match(p.name)
    if m and m.group(2) in a.views.split(","): seqs[(m.group(1), m.group(2), m.group(3))][int(m.group(4))] = p
if a.out: a.out.mkdir(parents=True, exist_ok=True)
x0, y0, x1, y1 = [float(v) for v in a.roi.split(",")]

def load(p):
    im = np.asarray(Image.open(p).convert("RGB"), dtype=np.float32) / 255.0
    H, W = im.shape[:2]
    return im[int(y0 * H):int(y1 * H), int(x0 * W):int(x1 * W)]

def box(X, k):
    P = np.pad(X, ((k // 2, k - 1 - k // 2), (k // 2, k - 1 - k // 2), (0, 0)), mode="edge")
    c = np.cumsum(np.cumsum(np.pad(P, ((1, 0), (1, 0), (0, 0))), axis=0), axis=1)
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)

print(f"ROI {a.roi}, {a.blur} px box, threshold {a.thresh}")
print(f"{'sequence':40s} {'n':>3s} {'mean':>8s} {'p99':>7s} {'chg%':>7s} {'max%':>7s} {'worst':>7s} {'events':>6s}")
for key in sorted(seqs):
    fr = seqs[key]; ks = sorted(fr)
    prev = box(load(fr[ks[0]]), a.blur); means, p99, share = [], [], []; worst = (-1, None)
    for k0, k1 in zip(ks[:-1], ks[1:]):
        cur = box(load(fr[k1]), a.blur); d = np.abs(cur - prev).max(axis=2)
        means.append(float(d.mean())); p99.append(float(np.percentile(d, 99))); s = float((d > a.thresh).mean()) * 100; share.append(s)
        if s > worst[0]: worst = (s, (k0, k1, d))
        prev = cur
    med = float(np.median(share)); events = sum(1 for s in share if s > max(3 * med, 0.05))
    name = "_".join(key)
    print(f"{name:40s} {len(ks):3d} {np.mean(means):8.5f} {np.mean(p99):7.4f} {np.mean(share):7.3f} {worst[0]:7.3f} {worst[1][0]:3d}-{worst[1][1]:<3d} {events:6d}")
    if a.out:
        k0, k1, d = worst[1]
        Image.fromarray((np.clip(d * 12.0, 0, 1) * 255).astype(np.uint8)).save(a.out / f"{name}_worst_f{k0:02d}-{k1:02d}_x12.png")
