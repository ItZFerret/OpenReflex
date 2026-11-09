#!/usr/bin/env python3
"""analyze.py <motion.csv> [--from T --to T] -- the motion's continuity, frame by frame: the frame pacing, the walk's clock (back-steps, stalls), and the
per-frame steps of the rider's root / head, the bike and the camera against their neighbours (spikes: a step more than 3x the median of the 8 around
it and over a floor), repeated positions followed by a jump (a pose that froze), turns that flip."""
import csv, math, sys, statistics

rows = [r for r in csv.DictReader(open(sys.argv[1])) if None not in r.values() and r.get("cam_pitch") not in (None, "")]
f = lambda r, k: float(r[k])
hold = sum(1 for r in rows if r.get("intro") == "Hold")
rows = [r for r in rows if r.get("intro") != "Hold"]   # (the intro's black hold: nothing is seen; the first frames build and compile there)
print(f"{hold} frames under the intro's black hold left out")
print(f"{len(rows)} frames over {f(rows[-1],'time') - f(rows[0],'time'):.2f} s")
dts = [f(r, "dt") * 1000 for r in rows[1:]]
q = sorted(dts)
print(f"dt ms: mean {statistics.mean(dts):.2f}, median {q[len(q)//2]:.2f}, p95 {q[int(len(q)*0.95)]:.2f}, max {q[-1]:.2f}; frames over 1.5x median: {sum(1 for d in dts if d > 1.5*q[len(q)//2])}")
longf = [(r["frame"], round(f(r, "dt") * 1000, 1), r["intro"], r["walking"]) for r in rows[1:] if f(r, "dt") > 0.0125]
print(f"frames over 12.5 ms: {len(longf)}: {longf[:12]}")
walk = [r for r in rows if r["walking"] == "1"]
if walk:
    back = [(r["frame"], r["walk_t"]) for p, r in zip(walk, walk[1:]) if f(r, "walk_t") < f(p, "walk_t") - 1e-6]
    stall = sum(1 for p, r in zip(walk, walk[1:]) if abs(f(r, "walk_t") - f(p, "walk_t")) < 1e-7)
    print(f"walk: {len(walk)} frames, t {f(walk[0],'walk_t'):.3f} -> {f(walk[-1],'walk_t'):.3f}; clock back-steps {len(back)} {back[:5]}; stalled frames {stall}")
held = [r for r in rows if r["walking"] == "0" and r["hold_idle"] == "1"]
print(f"frames with the idle held and no walk (a frozen rider): {len(held)}")
def track(name, keys, floor):
    pts = [tuple(f(r, k) for k in keys) for r in rows]
    steps = [math.dist(a, b) for a, b in zip(pts, pts[1:])]
    spikes = []
    for i in range(len(steps)):
        nb = steps[max(0, i - 4):i] + steps[i + 1:i + 5]
        med = statistics.median(nb) if nb else 0
        if steps[i] > floor and steps[i] > 3 * med: spikes.append((rows[i + 1]["frame"], round(f(rows[i + 1], "time"), 3), round(steps[i], 4), round(med, 4)))
    frozen = 0
    for i in range(1, len(steps) - 1):
        if steps[i - 1] < 1e-7 and steps[i] > floor * 2: frozen += 1
    print(f"{name}: max step {max(steps):.4f} ft, spikes {len(spikes)} {spikes[:8]}, freeze-then-jump {frozen}")
track("rider root", ("root_x", "root_y", "root_z"), 0.02)
track("rider head", ("head_x", "head_y", "head_z"), 0.02)
track("bike", ("bike_x", "bike_y", "bike_z"), 0.02)
track("camera", ("cam_x", "cam_y", "cam_z"), 0.02)
def ang(name, key, floor):
    v = [f(r, key) for r in rows]
    d = [abs((b - a + 180) % 360 - 180) for a, b in zip(v, v[1:])]
    big = [(rows[i + 1]["frame"], round(d[i], 2)) for i in range(len(d)) if d[i] > floor]
    print(f"{name}: max turn per frame {max(d):.2f} deg, over {floor} deg: {len(big)} {big[:8]}")
ang("rider root yaw", "root_yaw", 8); ang("bike yaw", "bike_yaw", 2); ang("bike roll", "bike_roll", 2); ang("bike pitch", "bike_pitch", 2); ang("camera yaw", "cam_yaw", 2)

# the end state: the last second without a walk and without the idle held (the rider is in his idle, nothing frozen)
tail = rows[-120:]
print(f"end state: walking {sum(r['walking'] == '1' for r in tail)} / held {sum(r['hold_idle'] == '1' for r in tail)} of the last {len(tail)} frames; rider idle moving {max(math.dist([f(a,'head_x'),f(a,'head_y'),f(a,'head_z')],[f(b,'head_x'),f(b,'head_y'),f(b,'head_z')]) for a,b in zip(tail, tail[1:])) > 1e-5}")
