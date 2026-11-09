#!/usr/bin/env python3
"""feet.py <motion.csv> [--clip SECONDS] [--table] -- the rider's feet through the walk's hand-over into the idle (needs the probe's lfoot_* / rfoot_*
columns): per frame the horizontal speed of each ankle and its height, and for the settle (walk_t past the clip's length until the walk ends) the
foot-planted check: while an ankle is on the floor (within PLANT ft of its lowest height of the settle) it must not move; the largest planted slide
(ft per frame and in total from where it was planted), and the whole slide of each ankle from the walk's last frame to the idle's."""
import csv, math, sys, argparse
ap = argparse.ArgumentParser(); ap.add_argument("csv"); ap.add_argument("--clip", type=float, default=6.33); ap.add_argument("--table", action="store_true")
ap.add_argument("--plant", type=float, default=0.01)
a = ap.parse_args()
rows = [r for r in csv.DictReader(open(a.csv)) if None not in r.values() and r.get("lfoot_x") not in (None, "")]
f = lambda r, k: float(r[k])
walk = [r for r in rows if r["walking"] == "1"]
if not walk: sys.exit("no walking frames")
# the settle: from the first frame with walk_t >= clip to the first frame after the walk ended (+ a few idle frames)
i0 = next((i for i, r in enumerate(rows) if r["walking"] == "1" and f(r, "walk_t") >= a.clip), None)
if i0 is None: sys.exit("the walk never reached t=%.2f" % a.clip)
i1 = i0
while i1 < len(rows) and rows[i1]["walking"] == "1": i1 += 1
i1 = min(len(rows) - 1, i1 + 6)
seg = rows[i0:i1 + 1]
print(f"settle: {len(seg)} frames, walk_t {f(seg[0],'walk_t'):.3f} -> {f(seg[-1],'walk_t'):.3f}, wall {f(seg[-1],'time') - f(seg[0],'time'):.3f} s")
res = {}
for side in ("lfoot", "rfoot"):
    p = [(f(r, side + "_x"), f(r, side + "_y"), f(r, side + "_z")) for r in seg]
    t = [f(r, "time") for r in seg]
    low = min(q[1] for q in p)
    step = [math.hypot(b[0] - a_[0], b[2] - a_[2]) for a_, b in zip(p, p[1:])]
    dt = [max(1e-6, t[i + 1] - t[i]) for i in range(len(p) - 1)]
    planted = [q[1] < low + a.plant for q in p]
    slide_frame = max([s for s, pl0, pl1 in zip(step, planted, planted[1:]) if pl0 and pl1] or [0])
    # the total slide of a planted run: the distance from where the run began
    worst = 0; start = None
    for i, pl in enumerate(planted):
        if pl and start is None: start = i
        if not pl: start = None
        if pl and start is not None: worst = max(worst, math.hypot(p[i][0] - p[start][0], p[i][2] - p[start][2]))
    total = math.hypot(p[-1][0] - p[0][0], p[-1][2] - p[0][2])
    print(f"{side}: ankle height {low:.3f}..{max(q[1] for q in p):.3f} ft; moved {total:.3f} ft from the walk's last frame to the idle; planted frames {sum(planted)}/{len(p)}; "
          f"largest planted step {slide_frame:.4f} ft/frame, largest slide of a planted run {worst:.3f} ft; fastest {max(s / d for s, d in zip(step, dt)):.2f} ft/s")
    res[side] = (p, step, dt, planted)
rp = [(f(r, "root_x"), f(r, "root_y"), f(r, "root_z")) for r in seg]
rt = [f(r, "time") for r in seg]
rv = [math.dist(x, y) / max(1e-6, t1 - t0) for x, y, t0, t1 in zip(rp, rp[1:], rt, rt[1:])]
ra = [abs(v1 - v0) / max(1e-6, t1 - t0) for v0, v1, t0, t1 in zip(rv, rv[1:], rt, rt[1:])]
print(f"pelvis: speed {rv[0]:.2f} ft/s at the clip's end -> max {max(rv):.2f} -> {rv[-1]:.2f}; largest frame-to-frame change of speed {max(ra):.1f} ft/s^2; height {min(q[1] for q in rp):.3f}..{max(q[1] for q in rp):.3f}")
if a.table:
    print("  walk_t   Lspeed  Ly     Rspeed  Ry    (ft/s, ft)")
    for i in range(len(seg) - 1):
        l = res["lfoot"]; r = res["rfoot"]
        print(f"  {f(seg[i],'walk_t'):6.3f}  {l[1][i]/l[2][i]:6.2f}  {l[0][i][1]:.3f}  {r[1][i]/r[2][i]:6.2f}  {r[0][i][1]:.3f}  {'P' if l[3][i] else '.'}{'P' if r[3][i] else '.'}")
