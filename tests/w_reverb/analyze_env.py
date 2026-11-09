#!/usr/bin/env python3
"""analyze_env.py <folder with reverb-*.csv> -- the decay of the probe's burst from the master output's level per frame (time, rms L, rms R)
that AudioListener.GetOutputData reports: the burst's 20 ms and the time the level takes to fall by 10 / 20 / 30 dB below its peak."""
import sys
from pathlib import Path
import numpy as np

for p in sorted(Path(sys.argv[1]).glob("reverb-*.csv")):
    rows = np.array([[float(v) for v in l.split(",")] for l in p.read_text().split() if l.count(",") == 2])
    if rows.size == 0: continue
    t, lvl = rows[:, 0], np.sqrt((rows[:, 1] ** 2 + rows[:, 2] ** 2) / 2)
    k = int(np.argmax(lvl)); peak = lvl[k]
    db = 20 * np.log10(np.maximum(lvl, 1e-9) / max(peak, 1e-9))
    def fall(level):
        for i in range(k, len(t)):
            if db[i] <= level and all(db[i:min(len(t), i + 5)] <= level + 3): return t[i] - t[k]
        return float("nan")
    tail = lvl[t > t[k] + 0.2]
    print(f"{p.name}: peak {peak:.3f} at {t[k]:.2f}s  falls 10 dB {fall(-10):.2f}s  20 dB {fall(-20):.2f}s  30 dB {fall(-30):.2f}s  tail(>0.2s) mean {20*np.log10(max(tail.mean() if tail.size else 1e-9,1e-9)/max(peak,1e-9)):.1f} dB  end {20*np.log10(max(lvl[-1],1e-9)/max(peak,1e-9)):.1f} dB")
