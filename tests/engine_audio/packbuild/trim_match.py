# Round 2 (w-pack450b): the pack.ini trim that makes a pack play as loud as a reference pack in the game. Since w-packloud the game levels
# every pack at load (loudness.pack_gain: an A-weighted estimate on a reference ride) and then holds the predicted peak under -1 dBFS; a
# punchier pack (a higher crest) loses more to the peak control, so it plays quieter than the estimate says. This renders both packs with the
# game's logic (after_logic.Loud, Engine channel 0.7) on recorded 450F riders, compares their A-weighted 125 ms levels, and iterates the
# trim (the peak control is not linear) until they match.
#   python3 trim_match.py --pack <pack dir> --ref <reference pack dir> --trace <rpmtrace csv> <rider> [--trace ...] [--iters 3]
# Prints the trim; build.py --set TRIM=<trim> writes it into pack.ini.
import argparse, os, sys, shutil, tempfile
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'offline'))
import render as R, after_logic as A, packs as P
import level_vs_original as LV

def level(pack, traces, trim):
    lv = []
    for tr, rider in traces:
        def mk(p, t=trim):
            p.trim = t; return A.Peak(p, 1.0, 0.7)   # w-packpeak: the game's mixer + limiter (was A.Loud)
        R.LOGIC['tm'] = mk
        t = R.load_trace(tr, int(rider))
        lv.append(LV.level(R.render(pack, t, 'tm', min(30.0, len(t) / 60.0), 0) * 0.7))
    return np.array(lv)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pack', required=True); ap.add_argument('--ref', required=True)
    ap.add_argument('--trace', nargs=2, action='append', required=True); ap.add_argument('--iters', type=int, default=3)
    a = ap.parse_args()
    ref = level(a.ref, a.trace, P.load(a.ref).trim)
    trim = 1.0
    for i in range(a.iters):
        lv = level(a.pack, a.trace, trim)
        r = float(np.exp(np.mean(np.log(ref / lv))))
        print(f'  trim {trim:.3f}: levels {", ".join(f"{v:.4f}" for v in lv)} vs the reference {", ".join(f"{v:.4f}" for v in ref)} ({20 * np.log10(1 / r):+.2f} dB)')
        if abs(20 * np.log10(r)) < 0.1: break
        trim = min(4.0, trim * r)
    print(f'trim {trim:.3f}')

if __name__ == '__main__':
    main()
