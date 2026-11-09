#!/usr/bin/env python3
"""aa_compare.py <run dir> [--ref enhanced-ss2] [--out DIR] -- ANTI-ALIASING (w-rmaa) stills against the supersampled reference.

For every view of the run's screenshots (<prefix>_<view>_<mode>.png, single shots, not the _fNN sequences) and every mode, against the
reference mode (default enhanced-ss2: render scale 200 %, no anti-aliasing, the 2 x 2 supersampled picture):
  mae / psnr   luma difference to the reference (aliasing and blur both raise it)
  edge         the edge-aliasing error: the luma difference only where the reference has edges (|gradient| top 10 %)
  sharp        the gradient energy relative to the reference (1 = as detailed; < 1 blurred, > 1 aliased / over-sharpened)
Also a sheet per view: the 3 crops where the no-AA image differs most from the reference, each mode side by side at 3 x (nearest)."""
import argparse, re
from collections import defaultdict
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

ap = argparse.ArgumentParser()
ap.add_argument("run", type=Path); ap.add_argument("--ref", default="enhanced-ss2"); ap.add_argument("--out", type=Path)
ap.add_argument("--crop", type=int, default=96); ap.add_argument("--base", default="enhanced-aaoff")
a = ap.parse_args()
shots = a.run / "screenshots" if (a.run / "screenshots").is_dir() else a.run
out = a.out or (a.run / "aa_compare"); out.mkdir(parents=True, exist_ok=True)
rx = re.compile(r"^(.*?)_([a-z0-9]+)_((?:original|enhanced)[a-z0-9-]*)\.png$")
views = defaultdict(dict)
for p in sorted(shots.glob("*.png")):
    m = rx.match(p.name)
    if m and not re.search(r"_f\d\d\.png$", p.name): views[(m.group(1), m.group(2))][m.group(3)] = p

def rgb(p): return np.asarray(Image.open(p).convert("RGB"), dtype=np.float32)
def luma(im): return 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]
def grad(l): gx = np.zeros_like(l); gy = np.zeros_like(l); gx[:, 1:] = l[:, 1:] - l[:, :-1]; gy[1:, :] = l[1:, :] - l[:-1, :]; return np.hypot(gx, gy)

print(f"{'view':34s} {'mode':30s} {'mae':>6s} {'psnr':>6s} {'edge':>6s} {'sharp':>6s}")
for (prefix, view), modes in sorted(views.items()):
    if a.ref not in modes: print("no reference for", prefix, view); continue
    R = rgb(modes[a.ref]); LR = luma(R); GR = grad(LR); edges = GR > np.percentile(GR, 90)
    for mode in sorted(modes):
        if mode == a.ref: continue
        L = luma(rgb(modes[mode]))
        if L.shape != LR.shape: print(prefix, view, mode, "size differs"); continue
        d = np.abs(L - LR); mse = float((d ** 2).mean())
        psnr = 10 * np.log10(255.0 ** 2 / max(mse, 1e-6))
        print(f"{prefix + '_' + view:34s} {mode:30s} {d.mean():6.2f} {psnr:6.2f} {d[edges].mean():6.2f} {grad(L).mean() / max(GR.mean(), 1e-6):6.3f}")
    # crops: where the base (no AA) differs most from the reference (block maxima, non-overlapping)
    if a.base not in modes: continue
    B = luma(rgb(modes[a.base])); c = a.crop; H, W = B.shape
    diff = np.abs(B - LR); ny, nx = H // c, W // c
    score = diff[:ny * c, :nx * c].reshape(ny, c, nx, c).mean(axis=(1, 3))
    picks = []
    for idx in np.argsort(score.ravel())[::-1]:
        j, i = divmod(int(idx), nx)
        if all(abs(j - pj) > 1 or abs(i - pi) > 1 for pj, pi in picks): picks.append((j, i))
        if len(picks) == 3: break
    order = [m for m in [a.base, "enhanced-msaa4", "enhanced-taa-sharp0", "enhanced-taa-sharp50", "enhanced-taa-upq-sharp50", "enhanced-taa-upb-sharp50", a.ref, "original"] if m in modes]
    s = 3; lab = 18
    sheet = Image.new("RGB", (len(order) * c * s, len(picks) * (c * s + lab) + lab), (24, 24, 24)); dr = ImageDraw.Draw(sheet)
    for k, m in enumerate(order): dr.text((k * c * s + 4, 2), m.replace("enhanced-", ""), fill=(255, 255, 255))
    for r, (j, i) in enumerate(picks):
        for k, m in enumerate(order):
            im = Image.open(modes[m]).convert("RGB").crop((i * c, j * c, i * c + c, j * c + c)).resize((c * s, c * s), Image.NEAREST)
            sheet.paste(im, (k * c * s, lab + r * (c * s + lab)))
        dr.text((4, lab + r * (c * s + lab) + c * s + 2), f"crop x {i * c} y {j * c}", fill=(200, 200, 200))
    f = out / f"{prefix}_{view}_crops.png"; sheet.save(f); print("  sheet", f)
