#!/usr/bin/env python3
"""temporal.py <run dir> [--out DIR] [--ref MODE] -- motion-compensated frame-to-frame difference of the showcase's cam "path" sequences.

The camera path is the same in every mode, so the image motion is the same: per 48 px block the integer shift that best aligns each
consecutive pair is found on the reference mode's sequence (default enhanced-nobumpn: no bump shading, the least flicker) and applied
to every mode. Reported per sequence: the residual mean absolute luma difference in the near-ground ROI (the flicker plus the common
sub-pixel warp error), the per-frame change of the ROI's mean luma and of its contrast (std), and the raw difference. Also the RT_Bump fit
per frame from the player log (PATH lines: raw -> smoothed)."""
import argparse, re
from collections import defaultdict
from pathlib import Path
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("run", type=Path); ap.add_argument("--out", type=Path); ap.add_argument("--ref", default="enhanced-nobumpn")
ap.add_argument("--roi", default="0.20,0.55,0.80,0.97"); ap.add_argument("--shift", type=int, default=10); ap.add_argument("--block", type=int, default=48)
a = ap.parse_args()
shots = a.run / "screenshots"; rx = re.compile(r"^(.*?)_(chase|fpv|[a-z0-9]+)_([a-z0-9-]+)_f(\d\d)\.png$")
seqs = defaultdict(dict)
for p in sorted(shots.glob("*_f??.png")):
    m = rx.match(p.name)
    if m: seqs[(m.group(1), m.group(2), m.group(3))][int(m.group(4))] = p
if a.out: a.out.mkdir(parents=True, exist_ok=True)
x0, y0, x1, y1 = [float(v) for v in a.roi.split(",")]
S, B = a.shift, a.block

def luma(p):
    im = np.asarray(Image.open(p).convert("RGB"), dtype=np.float32)
    return 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]

def load(key):
    fr = seqs[key]; ks = sorted(fr); L = [luma(fr[k]) for k in ks]; H, W = L[0].shape
    r = (slice(int(y0 * H), int(y1 * H)), slice(int(x0 * W), int(x1 * W)))
    return [l[r] for l in L]

def block_shifts(A, Bm):
    # per block the shift (dx, dy) of B that best matches A (search window +-S); blocks tile A's interior
    h, w = A.shape; ny, nx = (h - 2 * S) // B, (w - 2 * S) // B
    best = np.full((ny, nx), np.inf); sh = np.zeros((ny, nx, 2), dtype=int)
    Ac = A[S:S + ny * B, S:S + nx * B].reshape(ny, B, nx, B)
    for dy in range(-S, S + 1):
        for dx in range(-S, S + 1):
            Bc = Bm[S + dy:S + dy + ny * B, S + dx:S + dx + nx * B].reshape(ny, B, nx, B)
            d = np.abs(Ac - Bc).mean(axis=(1, 3))
            m = d < best; best[m] = d[m]; sh[m] = (dx, dy)
    return sh

def apply(A, Bm, sh):
    # A's interior and B shifted per block onto it
    h, w = A.shape; ny, nx = sh.shape[:2]
    Ai = A[S:S + ny * B, S:S + nx * B]; Bs = np.zeros_like(Ai)
    for j in range(ny):
        for i in range(nx):
            dx, dy = sh[j, i]
            Bs[j * B:(j + 1) * B, i * B:(i + 1) * B] = Bm[S + dy + j * B:S + dy + (j + 1) * B, S + dx + i * B:S + dx + (i + 1) * B]
    return Ai, Bs

def box(X, k):
    # k x k box mean (valid region)
    c = np.cumsum(np.cumsum(np.pad(X, ((1, 0), (1, 0))), axis=0), axis=1)
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)

def blockmeans(X):
    h, w = X.shape; ny, nx = h // B, w // B
    return X[:ny * B, :nx * B].reshape(ny, B, nx, B).mean(axis=(1, 3))

views = sorted(set((k[0], k[1]) for k in seqs))
print(f"ROI {a.roi}, blocks {B} px, shifts +-{S} px from the reference mode {a.ref}")
print("comp: block-shift compensated MAD; lp16 / lp48: the same after a 16 / 48 px box blur of both frames (the texture's sub-pixel motion averaged out, shading changes kept);")
print("|dblk|: mean over 48 px blocks of the block's compensated mean change; |dmean| / |dstd|: the ROI's mean / contrast change per frame")
print(f"{'sequence':40s} {'n':>3s} {'raw':>7s} {'comp':>7s} {'lp16':>7s} {'lp48':>7s} {'lp48p95':>8s} {'|dblk|':>7s} {'|dmean|':>8s} {'|dstd|':>7s} {'std':>6s}")
for prefix, view in views:
    refkey = (prefix, view, a.ref)
    if refkey not in seqs: print("no reference", refkey); continue
    R = load(refkey); shifts = [block_shifts(R[i], R[i + 1]) for i in range(len(R) - 1)]
    for key in sorted(k for k in seqs if k[0] == prefix and k[1] == view):
        L = load(key); raw, comp, lp16, lp48, dblk, dm, ds = [], [], [], [], [], [], []
        means = [float(l.mean()) for l in L]; stds = [float(l.std()) for l in L]
        diffs = []
        for i in range(len(L) - 1):
            raw.append(float(np.abs(L[i] - L[i + 1]).mean()))
            Ai, Bs = apply(L[i], L[i + 1], shifts[i]); d = np.abs(Ai - Bs); diffs.append(np.abs(box(Ai, 16) - box(Bs, 16))); comp.append(float(d.mean()))
            lp16.append(float(np.abs(box(Ai, 16) - box(Bs, 16)).mean())); lp48.append(float(np.abs(box(Ai, 48) - box(Bs, 48)).mean()))
            dblk.append(float(np.abs(blockmeans(Ai) - blockmeans(Bs)).mean()))
            dm.append(abs(means[i + 1] - means[i])); ds.append(abs(stds[i + 1] - stds[i]))
        c = np.array(comp); l48 = np.array(lp48)
        print(f"{key[1] + ' ' + key[2]:40s} {len(L):3d} {np.mean(raw):7.3f} {c.mean():7.3f} {np.mean(lp16):7.3f} {l48.mean():7.3f} {np.percentile(l48, 95):8.3f} {np.mean(dblk):7.3f} {np.mean(dm):8.3f} {np.mean(ds):7.3f} {np.mean(stds):6.2f}")
        if a.out:
            i = len(L) // 2; name = f"{key[0]}_{key[1]}_{key[2]}"
            Image.fromarray(np.clip(diffs[i] * 16.0, 0, 255).astype(np.uint8)).save(a.out / f"{name}_lp16diff_f{i:02d}_f{i + 1:02d}_x16.png")
            fr = seqs[key]; ks = sorted(fr); H, W = luma(fr[ks[0]]).shape
            for k in (ks[i], ks[i + 1]): Image.open(fr[k]).crop((int(x0 * W), int(y0 * H), int(x1 * W), int(y1 * H))).save(a.out / f"{name}_roi_f{k:02d}.png")
for log in sorted(a.run.glob("player-*.log")):
    per = defaultdict(list); sm = defaultdict(list)
    for line in log.read_text(errors="replace").splitlines():
        m = re.search(r"PATH (\S+) (\S+) f(\d\d) .*fit near (\S+) a1 (\S+) a2 (\S+) nearHalf (\S+) farHalf (\S+) smooth (.*)$", line)
        if not m: continue
        per[(m.group(1), m.group(2))].append([float(m.group(i)) for i in range(4, 9)])
        r = re.search(r"raw near (\S+) r0 (\S+) l0 (\S+) farR (\S+) farL (\S+) -> near (\S+) r0 (\S+) l0 (\S+)", m.group(9))
        if r: sm[(m.group(1), m.group(2))].append([float(r.group(i)) for i in range(1, 9)])
    for (view, mode), v in sorted(per.items()):
        v = np.array(v); d = np.abs(np.diff(v, axis=0))
        line = f"fit {view:6s} {mode:26s} used: near {v[:, 0].min():7.3f}..{v[:, 0].max():7.3f} |d| max {d[:, 0].max():6.3f}  a1 |d| max {d[:, 1].max():6.3f}  a2 |d| max {d[:, 2].max():6.3f}  nearHalf |d| max {d[:, 3].max():6.3f}"
        if (view, mode) in sm:
            s = np.array(sm[(view, mode)]); ds = np.abs(np.diff(s, axis=0))
            line += f" | raw near |d| max {ds[:, 0].max():6.3f} r0 {ds[:, 1].max():6.3f} l0 {ds[:, 2].max():6.3f} farR {ds[:, 3].max():6.3f} -> smoothed near |d| max {ds[:, 5].max():6.3f} r0 {ds[:, 6].max():6.3f} l0 {ds[:, 7].max():6.3f}"
        print(line)
