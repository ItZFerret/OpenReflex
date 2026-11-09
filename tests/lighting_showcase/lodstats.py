#!/usr/bin/env python3
"""lodstats.py <run dir> -- summarises the PATH lines of a showcase run made with REFLEX_FOREST_STATS=1 (OriginalForest.collectStats): per sequence
(view, mode) the trees drawn in 3D, the LOD changes of the trees in view per frame (a tree dropping to the next LOD record, or dissolving into its
billboard, or the reverse), and the flips (a tree back in the state it left within 8 frames: the flip-flop), plus the drawing cost of the forest
(ms of the per-camera work), the shadow casters and the trees in a LOD cross-fade."""
import re, sys
from collections import defaultdict
from pathlib import Path
import numpy as np
rx = re.compile(r"PATH (\S+) (\S+) f(\d+) cam \(([^)]*)\) yaw (\S+) 3d (\d+) shadow (\d+) draws (\d+) ms (\S+) .*?lodchg (-?\d+) flips (-?\d+) blend (-?\d+) grass (-?\d+)")
sx = re.compile(r"PATH (\S+) (\S+) f(\d+) .* sc lod (-?\d+) chg (-?\d+) flips (-?\d+) cull (-?\d+) fade (-?\d+) shonly (-?\d+)")
rows = defaultdict(list); scen = defaultdict(list)
for log in sorted(Path(sys.argv[1]).glob("player-*.log")):
    for line in log.read_text(errors="replace").splitlines():
        m = rx.search(line)
        if m: rows[(m.group(1), m.group(2))].append([float(m.group(i)) for i in range(6, 14)])
        m = sx.search(line)
        if m: scen[(m.group(1), m.group(2))].append([float(m.group(i)) for i in range(4, 10)])
print(f"{'sequence':32s} {'n':>4s} {'3d':>6s} {'shadow':>7s} {'blend':>6s} {'chg/fr':>7s} {'chg tot':>8s} {'flips tot':>9s} {'flip %':>7s} {'forest ms':>9s}")
for (view, mode), v in sorted(rows.items()):
    a = np.array(v)
    chg = a[1:, 4].sum(); flips = a[1:, 5].sum()
    print(f"{view + ' ' + mode:32s} {len(a):4d} {a[:, 0].mean():6.0f} {a[:, 1].mean():7.0f} {a[:, 6].mean():6.0f} {a[1:, 4].mean():7.2f} {chg:8.0f} {flips:9.0f} {100 * flips / max(chg, 1):7.1f} {a[:, 3].mean():9.3f}")

if any(v[0][0] >= 0 for v in scen.values()):
    print()
    print(f"{'scenery ' + 'sequence':32s} {'n':>4s} {'lod>0':>6s} {'chg tot':>8s} {'flips':>6s} {'flip %':>7s} {'culled':>7s} {'fading':>7s} {'shadow-only':>11s}")
    for (view, mode), v in sorted(scen.items()):
        a = np.array(v); chg = a[1:, 1].sum(); fl = a[1:, 2].sum()
        print(f"{view + ' ' + mode:32s} {len(a):4d} {a[:, 0].mean():6.0f} {chg:8.0f} {fl:6.0f} {100 * fl / max(chg, 1):7.1f} {a[:, 3].mean():7.0f} {a[:, 4].mean():7.0f} {a[:, 5].mean():11.0f}")
