#!/usr/bin/env python3
"""diffmap.py A.png B.png OUT.png [GAIN] -- |A - B| (max over channels) times GAIN (default 8) as an image, and the numbers."""
import sys
import numpy as np
from PIL import Image
A = np.asarray(Image.open(sys.argv[1]).convert("RGB"), dtype=np.int16); B = np.asarray(Image.open(sys.argv[2]).convert("RGB"), dtype=np.int16)
d = np.abs(A - B).max(axis=2); g = float(sys.argv[4]) if len(sys.argv) > 4 else 8.0
Image.fromarray(np.clip(d * g, 0, 255).astype(np.uint8)).save(sys.argv[3])
ys, xs = np.nonzero(d > 2)
print(f"max {int(d.max())}, mean {d.mean():.4f}, pixels over 2: {len(ys)}" + (f", bounding box x {xs.min()}..{xs.max()} y {ys.min()}..{ys.max()}" if len(ys) else ""))
