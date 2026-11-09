# Renders a pack through the player logic (tests/engine_audio/offline: render.py + after_logic) on a recorded race trace and on a slow
# idle -> max -> idle sweep, next to a reference pack, and prints peak / clipping / loudness / crest. --logic loud (default, the game since
# w-packloud): the load-time loudness gain x pack.ini trim and the predicted-peak control; --logic peak (default, the game since w-packpeak): the same gain, the pack's own mixer and its lookahead limiter on the real output, at the race's Engine channel 0.7 (the WAVs are what the
# game outputs in a race); --logic after: w-enginepacks' MixGain 0.30 x pack.ini gain at channel 1 (round 1's renders).
#
#   python3 verify.py --pack <pack dir> --ref <reference pack dir> --trace <rpmtrace.c csv> [--rider 0] [--seconds 30]
#                     [--out <dir for the WAVs>] [--name onboard450] [--refname HRC_Honda_450] [--match]
import argparse, os, sys, json
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'offline'))
import render as R, after_logic as A, packs as P
import dsp

def ini_gain(d):
    f = os.path.join(d, 'pack.ini')
    if not os.path.exists(f): return 1.0
    b = P.parse(open(f, encoding='utf-8', errors='replace').read())
    return min(4.0, max(0.0, P.F(b, 'gain', 1.0)))

def sweep_trace(idle=1500.0, mx=11500.0, up=16.0, hold=2.0, down=16.0, rest=2.0, fps=60):
    """Idle, a slow linear rise to max on full throttle, a hold on the limiter, a slow closed-throttle fall, idle (the 450F's range)."""
    tr = []
    def add(sec, f):
        n = int(sec * fps)
        for k in range(n): tr.append(f(k / max(1, n - 1)))
    add(rest, lambda u: (idle, idle, mx, 0.0, 0, 3))
    add(up, lambda u: (idle + (mx - idle) * u, idle, mx, 1.0, 0, 3))
    add(hold, lambda u: (mx, idle, mx, 1.0, 0, 3))
    add(down, lambda u: (mx - (mx - idle) * u, idle, mx, 0.0, 0, 3))
    add(rest, lambda u: (idle, idle, mx, 0.0, 0, 3))
    return tr

LOGIC = 'loud'; CHANNEL = 0.7
def run(pack, trace, seconds, gain):
    if LOGIC in ('loud', 'peak'):
        cls = A.Peak if LOGIC == 'peak' else A.Loud
        R.LOGIC['v'] = lambda p: cls(p, 1.0, CHANNEL)
    else:
        R.LOGIC['v'] = lambda p: A.After(p, gain)
    st = {}
    out = R.render(pack, trace, 'v', seconds, 0, stats=st)
    return (out * CHANNEL if LOGIC in ('loud', 'peak') else out), st

def stats(x):
    s = dsp.loudness_short(x, R.OUT_RATE)
    act = s[s > np.max(s) - 40]
    import level_vs_original as LV
    w = LV.a_weight(x); b = int(0.125 * R.OUT_RATE); k = len(w) // b; a125 = np.sqrt(np.mean(w[:k * b].reshape(k, b) ** 2, axis=1))
    return dict(peak=float(np.max(np.abs(x))), clipped_pct=float(np.mean(np.abs(x) > 1.0) * 100), loud=dsp.loudness_db(x, R.OUT_RATE),
                a_level=float(np.sqrt(np.mean(a125 ** 2))), crest_db=float(20 * np.log10(np.max(np.abs(x)) / (np.sqrt(np.mean(x ** 2)) + 1e-12))),
                rms=float(np.sqrt(np.mean(x ** 2))), short_p10=float(np.percentile(act, 10)), short_p90=float(np.percentile(act, 90)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pack', required=True); ap.add_argument('--ref', required=True); ap.add_argument('--trace', required=True)
    ap.add_argument('--rider', type=int, default=0); ap.add_argument('--seconds', type=float, default=30.0)
    ap.add_argument('--out', default=None); ap.add_argument('--name', default='onboard450'); ap.add_argument('--refname', default='HRC_Honda_450')
    ap.add_argument('--match', action='store_true', help='print the pack.ini gain that matches the reference loudness on the trace')
    ap.add_argument('--json', default=None); ap.add_argument('--logic', default='peak', choices=('after', 'loud', 'peak'))
    a = ap.parse_args()
    global LOGIC; LOGIC = a.logic
    trace = R.load_trace(a.trace, a.rider)
    sw = sweep_trace()
    g_new, g_ref = (ini_gain(a.pack), ini_gain(a.ref)) if a.logic == 'after' else (1.0, 1.0)
    res = {}
    for tag, pack, g, nm in (('new', a.pack, g_new, a.name), ('ref', a.ref, g_ref, a.refname)):
        lap, st = run(pack, trace, a.seconds, g)
        swp, _ = run(pack, sw, len(sw) / 60.0, g)
        red = np.array(st.get('reduction_db', [0.0]))
        res[tag] = dict(gain=g, lap=stats(lap), sweep=stats(swp), limiter_starts=st['trig'], limiter_audible_pct=st['limaudible'] / st['frames'] * 100,
                        reduction_mean_db=float(np.mean(red)), reduction_max_db=float(np.min(red)), reduced_pct=float(np.mean(red < (-0.01 if LOGIC == 'peak' else -0.1)) * 100))
        if a.out:
            os.makedirs(a.out, exist_ok=True)
            R.write_wav(os.path.join(a.out, f'{nm}__{"player" if tag == "new" else "compare"}.wav'), lap)
            R.write_wav(os.path.join(a.out, f'{nm}__sweep.wav'), swp)
        res[tag]['_lap'] = lap
    d = res['new']['lap']['loud'] - res['ref']['lap']['loud']
    for tag in ('new', 'ref'):
        r = res[tag]
        print(f"{tag}: {a.logic} | trace: peak {r['lap']['peak']:.3f}, clipped {r['lap']['clipped_pct']:.3f} %, A-weighted {r['lap']['a_level']:.4f}, crest {r['lap']['crest_db']:.1f} dB,"
              f" peak control {r['reduced_pct']:.0f} % of frames (mean {r['reduction_mean_db']:.2f}, max {r['reduction_max_db']:.1f} dB), loudness {r['lap']['loud']:.1f} dB"
              f" (short-term p10 {r['lap']['short_p10']:.1f} / p90 {r['lap']['short_p90']:.1f}), rms {r['lap']['rms']:.3f}, limiter starts {r['limiter_starts']},"
              f" audible {r['limiter_audible_pct']:.1f} % | sweep: peak {r['sweep']['peak']:.3f}, clipped {r['sweep']['clipped_pct']:.3f} %, A-weighted {r['sweep']['a_level']:.4f}, crest {r['sweep']['crest_db']:.1f} dB")
    print(f'new - ref loudness on the trace: {d:+.2f} dB')
    if a.match:
        g = res['new']['gain'] * 10 ** (-d / 20)
        pk = res['new']['lap']['peak'] * g / res['new']['gain']
        print(f'matching gain: {g:.3f} (trace peak at that gain {pk:.3f})')
    if a.json:
        for t in res: res[t].pop('_lap', None)
        json.dump(res, open(a.json, 'w'), indent=1)

if __name__ == '__main__':
    main()
