#!/usr/bin/env python3
"""pops.py <run dir> [--out DIR] [--tex-px N] -- measures popping in the showcase's cam "path" sequences that only TURN the camera (step 0).

A static scene seen by a camera that only rotates about its position is the previous frame reprojected (a pure-rotation homography), whatever
the depths: so the residual |frame(i + 1) - reproject(frame(i))| contains only what changed on its own - a tree switching LOD, a shadow
appearing or crawling, a billboard changing frame, a texture shimmering - and nothing from the motion. Per sequence (prefix, view, mode) it prints
the mean residual, the 99th percentile, the share of pixels above a pop threshold (after a 3 px box blur, so single-pixel aliasing does not
count) and the worst frame; with --out it saves the residual heat map of the worst pair and a strip of the worst pair's two frames.
The camera model is the showcase's path camera: Quaternion.Euler(-(pitch + bob), yaw + turn * k, 0), vertical fov `fov`. The plan JSON of each
sequence (plan-*.json in the run directory) gives them."""
import argparse, json, math, re
from collections import defaultdict
from pathlib import Path
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("run", type=Path); ap.add_argument("--out", type=Path)
ap.add_argument("--threshold", type=float, default=0.03, help="pop threshold on the blurred residual, 0..1 luma")
ap.add_argument("--blur", type=int, default=9, help="box blur (px) of the residual: leaf-edge aliasing averages out, a changed tree does not")
ap.add_argument("--margin", type=int, default=0, help="extra border (px) to leave out")
ap.add_argument("--roi", default="0,0,1,1", help="x0,y0,x1,y1 as fractions of the frame: only that region counts (e.g. the ground: 0,0.6,1,1)")
a = ap.parse_args()
shots = a.run / "screenshots"
rx = re.compile(r"^(.*?)_([A-Za-z0-9]+)_([a-z0-9-]+)_f(\d\d+)\.png$")
seqs = defaultdict(dict)
for p in sorted(shots.glob("*_f??.png")):
    m = rx.match(p.name)
    if m: seqs[(m.group(1), m.group(2), m.group(3))][int(m.group(4))] = p
views = {}
for pf in a.run.glob("plan-*.json"):
    plan = json.loads(pf.read_text())
    for v in plan.get("views", []):
        views[(plan["prefix"], v["name"])] = v
if a.out: a.out.mkdir(parents=True, exist_ok=True)

def luma(p):
    im = np.asarray(Image.open(p).convert("RGB"), dtype=np.float32) / 255.0
    return 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]

def rot(yaw, xang):
    # Unity (left-handed): Euler(x, y, 0) = Ry(y) Rx(x); camera looks along +z, x right, y up
    cy, sy, cx, sx = math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), math.cos(math.radians(xang)), math.sin(math.radians(xang))
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]); Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    return Ry @ Rx

def reproject(prev, R_prev, R_next, fov, W, H):
    # for every pixel of the next frame, where it was in the previous one
    t = math.tan(math.radians(fov) / 2); asp = W / H
    u, v = np.meshgrid((np.arange(W) + 0.5) / W, (np.arange(H) + 0.5) / H)
    d = np.stack([(u * 2 - 1) * t * asp, (1 - v * 2) * t, np.ones_like(u)], axis=-1)
    dp = d @ (R_prev.T @ R_next).T
    z = dp[..., 2]; ok = z > 1e-4
    x = np.where(ok, dp[..., 0] / np.where(ok, z, 1) / (t * asp), 9); y = np.where(ok, dp[..., 1] / np.where(ok, z, 1) / t, 9)
    px = (x + 1) * 0.5 * W - 0.5; py = (1 - y) * 0.5 * H - 0.5
    inside = ok & (px >= 0) & (py >= 0) & (px <= W - 1) & (py <= H - 1)
    x0 = np.clip(np.floor(px).astype(int), 0, W - 2); y0 = np.clip(np.floor(py).astype(int), 0, H - 2); fx = px - x0; fy = py - y0
    out = (prev[y0, x0] * (1 - fx) * (1 - fy) + prev[y0, x0 + 1] * fx * (1 - fy) + prev[y0 + 1, x0] * (1 - fx) * fy + prev[y0 + 1, x0 + 1] * fx * fy)
    return out, inside

def box(X, k):
    c = np.cumsum(np.cumsum(np.pad(X, ((1, 0), (1, 0))), axis=0), axis=1)
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)

print(f"{'sequence':44s} {'n':>3s} {'mean':>7s} {'p99':>7s} {'pop%':>7s} {'worst':>6s} {'worst%':>7s}")
for key in sorted(seqs):
    prefix, view, mode = key
    v = views.get((prefix, view))
    if not v or v.get("cam") != "path" or v.get("step", 0) != 0: continue   # turning only: a dolly is dolly.py's
    fr = seqs[key]; ks = sorted(fr)
    if len(ks) < 2: continue
    every = max(1, v.get("every", 1))
    L = {k: luma(fr[k]) for k in ks}; H, W = L[ks[0]].shape
    fov = v.get("fov", 60)
    def R(k):
        ph = 2 * math.pi * k / v["bobPeriod"] if v.get("bobPeriod", 0) > 0 else 0
        return rot(v.get("yaw", 0) + v.get("turn", 0) * k + v.get("bobYaw", 0) * math.sin(ph * 1.37), -(v.get("pitch", 0) + v.get("bobPitch", 0) * math.sin(ph)))
    means, p99s, pops, darks, lights = [], [], [], [], []
    worst = (-1, None)
    for k0, k1 in zip(ks[:-1], ks[1:]):
        warped, inside = reproject(L[k0], R(k0), R(k1), fov, W, H)
        res = np.abs(L[k1] - warped); sres = L[k1] - warped   # sres < 0: the new frame is darker (a shadow that was not there)
        res[~inside] = 0; sres[~inside] = 0
        m = 3 + a.margin; inside[:m, :] = False; inside[-m:, :] = False; inside[:, :m] = False; inside[:, -m:] = False
        # leave out a 2 px band around the border of the reprojectable area (resampling)
        k = a.blur; blur = box(np.pad(res, ((k // 2, k - 1 - k // 2), (k // 2, k - 1 - k // 2)), mode="edge"), k)
        sel = inside.copy()
        rx0, ry0, rx1, ry1 = [float(q) for q in a.roi.split(",")]
        roi = np.zeros_like(sel); roi[int(ry0 * H):int(ry1 * H), int(rx0 * W):int(rx1 * W)] = True; sel &= roi
        means.append(float(res[sel].mean())); p99s.append(float(np.percentile(res[sel], 99)))
        pop = float((blur[sel] > a.threshold).mean()) * 100; pops.append(pop)
        sb = box(np.pad(sres, ((k // 2, k - 1 - k // 2), (k // 2, k - 1 - k // 2)), mode="edge"), k)
        darks.append(float((sb[sel] < -a.threshold).mean()) * 100); lights.append(float((sb[sel] > a.threshold).mean()) * 100)
        if pop > worst[0]: worst = (pop, (k0, k1, blur, sel))
    name = f"{prefix}_{view}_{mode}"
    print(f"{name:44s} {len(ks):3d} {np.mean(means):7.4f} {np.mean(p99s):7.4f} {np.mean(pops):7.3f} {worst[1][0]:3d}-{worst[1][1]:<3d} {worst[0]:7.3f}  dark {np.mean(darks):6.3f} (max {np.max(darks):6.3f}) light {np.mean(lights):6.3f}")
    if a.out:
        k0, k1, blur, sel = worst[1]
        heat = np.clip(blur * 6.0, 0, 1) * sel
        Image.fromarray((heat * 255).astype(np.uint8)).save(a.out / f"{name}_worst_f{k0:02d}-{k1:02d}_residualx6.png")
