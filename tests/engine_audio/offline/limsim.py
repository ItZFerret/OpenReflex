# python3 limsim.py <pack dir> <trace csv>...: limiter statistics of the before / after logic on recorded traces (no audio): frames at the limiter, triggers per minute, audible time
import sys, csv, numpy as np, render as R, packs as P, after_logic as A
pack = P.load(sys.argv[1]); cyc = len(R.read_wav(pack.limiterWav)[0]) / R.read_wav(pack.limiterWav)[1]
print(f"pack {pack.name}: limiter cycle {cyc:.2f} s")
for fn in sys.argv[2:]:
    rows = list(csv.DictReader(open(fn))); riders = sorted(set(int(r['rider']) for r in rows))
    for rid in riders:
        tr = R.load_trace(fn, rid); res = {}
        for name, L in (('before', R.Before), ('after', A.After)):
            lg = L(pack); trig = lim = aud = 0; playing_until = -1; looping = False
            for k, (rpm, idle, mx, thr, air, gear) in enumerate(tr):
                _, _, _, rise, fall, limit = lg.frame(rpm, idle, mx, thr, 1/60, air)
                t = k / 60
                if rise: trig += 1
                if limit: lim += 1
                if name == 'before':
                    if limit and t >= playing_until: playing_until = t + cyc * np.ceil(1e-9 + (1/60) / cyc)   # at least one full cycle per start
                    if limit: playing_until = max(playing_until, t + 1/60)
                    if t < playing_until: aud += 1
                else:
                    if limit or lg.limGain > 0: aud += 1
            mins = len(tr) / 3600
            res[name] = (lim / len(tr) * 100, trig / mins, aud / len(tr) * 100)
        b, a = res['before'], res['after']
        tag = 'local' if rid == 0 else 'AI'
        print(f"  {fn:16s} r{rid:<2d} {tag:5s} before: at-limiter {b[0]:5.1f}% frames, {b[1]:5.1f} triggers/min, audible {b[2]:5.1f}% | after: {a[0]:4.1f}% frames, {a[1]:4.1f} triggers/min, audible {a[2]:4.1f}%")
