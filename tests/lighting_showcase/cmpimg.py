#!/usr/bin/env python3
"""cmpimg.py DIR_A PATTERN_A DIR_B PATTERN_B -- compares matching frames of two runs (PATTERN with {k} for the frame number, e.g.
'P_Alpine_dolly_original_f{k:02d}.png'): the largest and mean absolute difference (0..255) per frame, to show that two renderings are the same picture
(the ORIGINAL draw distance against the build before it) or how far apart they are."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
da, pa, db, pb = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3]), sys.argv[4]
worst, tot, n, bad = 0, 0.0, 0, 0
for k in range(0, 400):
    fa, fb = da / pa.format(k=k), db / pb.format(k=k)
    if not fa.exists() or not fb.exists(): continue
    A = np.asarray(Image.open(fa).convert("RGB"), dtype=np.int16); B = np.asarray(Image.open(fb).convert("RGB"), dtype=np.int16)
    d = np.abs(A - B); worst = max(worst, int(d.max())); tot += float(d.mean()); n += 1; bad += int((d.max(axis=2) > 2).sum())
print(f"{n} frame pairs: largest difference {worst}/255, mean {tot / max(n, 1):.4f}, pixels differing by more than 2: {bad}")
