# Reflex's gearbox as the engine packs hear it (w-packrpm): per rider of a recorded race trace (rpmtrace.c / aitrace.c CSV: every frame,
# rx_session_vehicle_audio as Unity reads it) the time per gear, the shifts a minute, the rpm before / after each up- and downshift, how fast
# the rpm moves, the time at each rpm fraction, and the in-air rev-ups. Groups riders by engine size (2 = 250 two-stroke, 3 = 250F, 4 = 450F)
# and player / computer.
#   python3 shift_stats.py <trace.csv>... [--json out.json]
import sys, csv, json, collections
import numpy as np

def riders(path):
    R = collections.defaultdict(list)
    for r in csv.DictReader(open(path)):
        if r['frame'] in ('C', 'F'): continue
        if 'phase' in r and r['phase'] != '2': continue        # aitrace: racing only
        R[int(r['rider'])].append(r)
    return R

def stats(rows, fps=60.0):
    rpm = np.array([float(r['rpm']) for r in rows]); mx = float(rows[0]['max']); idle = float(rows[0]['idle'])
    gear = np.array([int(r['gear']) for r in rows]); thr = np.array([float(r['thr']) for r in rows]); air = np.array([int(r['air']) for r in rows])
    frac = np.clip((rpm - idle) / (mx - idle), 0, 1); n = len(rows); minutes = n / fps / 60
    ch = np.nonzero(np.diff(gear))[0]                         # the last frame of the old gear
    ups = [i for i in ch if gear[i + 1] > gear[i]]; downs = [i for i in ch if gear[i + 1] < gear[i]]
    stints = np.diff(np.concatenate(([0], ch + 1, [n]))) / fps
    def around(idx, k):
        a = [frac[i] for i in idx if i - 1 >= 0 and i + k < n]; b = [frac[i + k] for i in idx if i - 1 >= 0 and i + k < n]
        return (float(np.median(a)) if a else float('nan'), float(np.median(b)) if b else float('nan'))
    rate = np.diff(rpm) * fps
    ground = air[1:] == 0
    up_rate = rate[(rate > 0) & ground]; dn_rate = rate[(rate < 0) & ground]
    # in-air rev-ups: airborne runs >= 6 frames, the rpm fraction at take-off and its max in the air
    revs = []; i = 0
    while i < n:
        if air[i]:
            j = i
            while j < n and air[j]: j += 1
            if j - i >= 6: revs.append((frac[i], frac[i:j].max(), (j - i) / fps, float(thr[i:j].mean())))
            i = j
        else: i += 1
    hist = np.histogram(frac, bins=10, range=(0, 1))[0] / n * 100
    gears = {int(g): float(np.mean(gear == g) * 100) for g in sorted(set(gear))}
    # the step of each shift: the rpm change from 1 frame before to 3 frames after (as the pack sees it)
    def step(idx, k=3): return [float((rpm[i + k] - rpm[i - 1])) for i in idx if i - 1 >= 0 and i + k < n]
    return dict(frames=n, max=mx, idle=idle, gears_pct=gears, stint_median_s=float(np.median(stints)), stint_p90_s=float(np.percentile(stints, 90)),
                ups_per_min=len(ups) / minutes, downs_per_min=len(downs) / minutes,
                up_before_after=around(ups, 1), up_after6=around(ups, 6)[1], down_before_after=around(downs, 1), down_after6=around(downs, 6)[1],
                up_step_rpm=float(np.median(step(ups))) if ups else 0.0, down_step_rpm=float(np.median(step(downs))) if downs else 0.0,
                rise_rate_p50=float(np.percentile(up_rate, 50)) if len(up_rate) else 0.0, rise_rate_p95=float(np.percentile(up_rate, 95)) if len(up_rate) else 0.0,
                fall_rate_p50=float(np.percentile(dn_rate, 50)) if len(dn_rate) else 0.0, fall_rate_p5=float(np.percentile(dn_rate, 5)) if len(dn_rate) else 0.0,
                big_steps_per_min=float(np.sum(np.abs(np.diff(rpm)) > 0.05 * mx) / minutes),
                frac_hist_pct=[round(float(h), 1) for h in hist], air_pct=float(air.mean() * 100), air_revs=len(revs),
                air_rev_from_to=(float(np.median([r[0] for r in revs])), float(np.median([r[1] for r in revs]))) if revs else (0, 0),
                thr_mean=float(thr.mean()))

def main():
    paths = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = {}
    for p in paths:
        for rid, rows in sorted(riders(p).items()):
            flags = int(rows[0]['flags']); size = int(rows[0]['size']); local = rid == 0
            key = f"{p.split('/')[-1]} r{rid} {'player' if local else 'AI'} size{size}"
            out[key] = stats(rows)
    groups = collections.defaultdict(list)
    for k, s in out.items():
        who = 'player' if ' player ' in k else 'AI'; size = k.split('size')[1]
        groups[(size, who)].append(s)
    name = {'2': '250 2-stroke', '3': '250F', '4': '450F'}
    for (size, who), L in sorted(groups.items()):
        f = lambda key: float(np.median([s[key] for s in L]))
        fb = lambda key, i: float(np.median([s[key][i] for s in L]))
        hist = np.mean([s['frac_hist_pct'] for s in L], axis=0)
        gp = collections.Counter()
        for s in L:
            for g, v in s['gears_pct'].items(): gp[g] += v / len(L)
        print(f"== {name.get(size, size)} {who} ({len(L)} rider-runs, max {L[0]['max']:.0f}, idle {L[0]['idle']:.0f}, throttle mean {f('thr_mean'):.2f})")
        print(f"   time in gear: median stint {f('stint_median_s'):.2f} s (p90 {f('stint_p90_s'):.2f} s); share by gear " + ', '.join(f'{g}:{v:.0f}%' for g, v in sorted(gp.items())))
        print(f"   shifts / min: up {f('ups_per_min'):.1f}, down {f('downs_per_min'):.1f}; big rpm steps (> 5 % of max in one frame) {f('big_steps_per_min'):.1f} / min")
        print(f"   upshift: rpm fraction {fb('up_before_after', 0):.2f} -> {fb('up_before_after', 1):.2f} next frame -> {f('up_after6'):.2f} after 0.1 s; step {f('up_step_rpm'):+.0f} rpm in 4 frames")
        print(f"   downshift: {fb('down_before_after', 0):.2f} -> {fb('down_before_after', 1):.2f} -> {f('down_after6'):.2f}; step {f('down_step_rpm'):+.0f} rpm")
        print(f"   rpm rate on the ground: rising p50 {f('rise_rate_p50'):.0f} / p95 {f('rise_rate_p95'):.0f} rpm/s; falling p50 {f('fall_rate_p50'):.0f} / p5 {f('fall_rate_p5'):.0f} rpm/s")
        print(f"   time at rpm fraction (0-10 % .. 90-100 %): " + ' '.join(f'{h:.0f}' for h in hist))
        print(f"   airborne {f('air_pct'):.0f} % of the time, {f('air_revs'):.0f} jumps >= 0.1 s per run; rpm fraction at take-off {fb('air_rev_from_to', 0):.2f} -> max in the air {fb('air_rev_from_to', 1):.2f}")
    if '--json' in sys.argv:
        json.dump(out, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)

if __name__ == '__main__':
    main()
