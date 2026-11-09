#!/usr/bin/env python3
"""perfagg.py <run dir or *_perf.txt>... -- the showcase's perf lines (several rounds of the same modes, interleaved so a machine that is busy with
other work or warming up hits every mode alike) per mode: the median of the rounds' mean frame time, the worst and best round, the median p95, and
the GPU time (the median of the rounds' means). First round of each mode is dropped with --skip 1."""
import re, sys
from collections import defaultdict
from pathlib import Path
import numpy as np
skip = 0; paths = []
args = sys.argv[1:]
while args:
    a = args.pop(0)
    if a == "--skip": skip = int(args.pop(0))
    else: paths.append(Path(a))
files = []
for p in paths: files += sorted(p.glob("screenshots/*_perf.txt")) if p.is_dir() else [p]
rx = re.compile(r"^(\S+) (\S+): (\d+) frames, frame ([\d.]+) ms mean, ([\d.]+) ms p95(?:, GPU ([\d.]+) ms mean, ([\d.]+) ms p95)?")
by = defaultdict(list)
for f in files:
    for line in f.read_text().splitlines():
        m = rx.match(line)
        if m: by[(m.group(1), m.group(2))].append([float(m.group(i)) if m.group(i) else float("nan") for i in (4, 5, 6, 7)])
print(f"{'view / mode':32s} {'rounds':>6s} {'frame ms':>9s} {'(min-max)':>13s} {'p95 ms':>7s} {'GPU ms':>7s} {'GPU p95':>8s}")
for key, v in sorted(by.items()):
    a = np.array(v[skip:] if len(v) > skip else v)
    print(f"{key[0] + ' ' + key[1]:32s} {len(a):6d} {np.median(a[:, 0]):9.2f} {a[:, 0].min():6.2f}-{a[:, 0].max():<6.2f} {np.median(a[:, 1]):7.2f} {np.nanmedian(a[:, 2]):7.2f} {np.nanmedian(a[:, 3]):8.2f}")
