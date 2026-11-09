#!/usr/bin/python3
"""compare_shots.py <before run dir> <after run dir> [--out DIR] -- the matched-view screenshots of two bench runs (the same scene / view /
mode / tier, the *_offscreen.png renders of tests/perf_profile/fixture/PerfBench.cs): per pair the mean absolute difference (0..255), the
share of pixels that differ by more than 8 and 24 levels, PSNR, and a side-by-side image (before | after | difference x4) in --out.
Uses the system Python's numpy / Pillow (/usr/bin/python3); nothing is installed."""
import argparse, json, sys
from pathlib import Path
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("before", type=Path); ap.add_argument("after", type=Path); ap.add_argument("--out", type=Path)
a = ap.parse_args()
rows = []
for b in sorted(a.before.rglob("*_offscreen.png")):
    rel = b.relative_to(a.before)
    m = list(a.after.rglob(b.name))
    if not m: continue
    x = np.asarray(Image.open(b).convert("RGB"), dtype=np.int16); y = np.asarray(Image.open(m[0]).convert("RGB"), dtype=np.int16)
    if x.shape != y.shape: rows.append({"shot": b.name, "error": "size"}); continue
    d = np.abs(x - y); dm = d.max(axis=2)
    mse = float((d.astype(np.float64) ** 2).mean())
    r = {"shot": b.name, "mean_abs": round(float(d.mean()), 3), "over8": round(float((dm > 8).mean()) * 100, 3), "over24": round(float((dm > 24).mean()) * 100, 3),
         "psnr": round(10 * np.log10(255 ** 2 / mse), 2) if mse > 0 else float("inf")}
    rows.append(r)
    if a.out:
        a.out.mkdir(parents=True, exist_ok=True)
        side = np.concatenate([x, y, np.clip(d * 4, 0, 255)], axis=1).astype(np.uint8)
        Image.fromarray(side).save(a.out / b.name.replace("_offscreen.png", "_before-after-diff.jpg"), quality=88)
for r in rows: print(json.dumps(r))
if a.out: (a.out / "compare.json").write_text(json.dumps(rows, indent=1))
