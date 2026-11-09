# The A-weighted level of engine packs against the ORIGINAL Reflex engine, the w-landclank way (tests/engine_audio/offline/landing_mix.py):
# the local rider's engine alone, A-weighted, rms of 125 ms windows, over 30 s of racing. The original engine comes from field_mix.mix
# (the original engine events driven by the engine_logic command stream, Engine channel 0.7); each pack is rendered by render.py with the
# player logic (after_logic.After: MixGain 0.30 x pack.ini gain) x the Engine channel 0.7, centred (x 0.7071 per side, as landing_mix).
#   python3 level_vs_original.py <aitrace trace.csv> <cmds.tsv> <pack dir>... [--seconds 30] [--match <pack dir>] [--logic after|loud]
# --logic loud (default since w-packloud): the game's load-time loudness gain (loudness.pack_gain, 450F target) x pack.ini trim and the peak
# control (after_logic.Loud); --logic peak (default since w-packpeak): the same gain, the game's mixer + lookahead limiter (after_logic.Peak); --logic after: w-enginepacks' MixGain 0.30 x pack.ini gain.
# --match prints the pack.ini gain that puts that pack at the original engine's level, and the trace peak it would have.
import argparse, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'offline'))
import render as R, after_logic as A, field_mix as F, packs as P

def a_weight(x, rate=R.OUT_RATE):
    """landing_mix.a_weight: the A curve, 0 dB at 1 kHz."""
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / rate); f2 = f * f
    ra = (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2))
    return np.fft.irfft(X * ra / 0.7943, len(x))

def level(m, rate=R.OUT_RATE):
    w = a_weight(m, rate); b = int(0.125 * rate); k = len(w) // b
    s = np.sqrt(np.mean(w[:k * b].reshape(k, b) ** 2, axis=1))
    return float(np.sqrt(np.mean(s ** 2)))

def ini_gain(d):
    f = os.path.join(d, 'pack.ini')
    if not os.path.exists(f): return 1.0
    return min(4.0, max(0.0, P.F(P.parse(open(f, encoding='utf-8', errors='replace').read()), 'gain', 1.0)))

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('trace'); ap.add_argument('cmds'); ap.add_argument('packs', nargs='+')
    ap.add_argument('--seconds', type=float, default=30.0); ap.add_argument('--match', default=None)
    ap.add_argument('--logic', default='peak', choices=('after', 'loud', 'peak'))
    a = ap.parse_args()
    pos, phase, locs = F.load_positions(a.trace)
    f0 = min(f for f, p in phase.items() if p == 2); nfr = int(a.seconds * 60); start = f0
    player, field, act, (pL, pR, fL, fR), trims = F.mix(a.trace, a.cmds, nfr, start)
    orig = level((pL + pR) / 2)
    print(f'original engine (local rider, racing frames {start}..{start + nfr}): A-weighted 125 ms level {orig:.4f}')
    trace = R.load_trace(a.trace, 0)
    for d in a.packs + ([a.match] if a.match and a.match not in a.packs else []):
        g = ini_gain(d) if a.logic == 'after' else 1.0
        R.LOGIC['lv'] = (lambda p, g=g: A.After(p, g)) if a.logic == 'after' else (lambda p: (A.Peak if a.logic == 'peak' else A.Loud)(p, 1.0, F.CH_ENGINE))
        st = {}
        out = R.render(d, trace, 'lv', nfr / 60.0, start - min(pos), stats=st) * F.CH_ENGINE
        red = np.array(st.get('reduction_db', [0.0]))
        lv = level(out * 0.7071)
        print(f'{os.path.basename(d.rstrip("/")):28s} gain {g:.3f}: A-weighted {lv:.4f} = {20 * np.log10(lv / orig):+.1f} dB vs the original engine;'
              f' output peak {np.max(np.abs(out)):.3f} (x Engine channel 0.7), {np.max(np.abs(out)) / F.CH_ENGINE:.3f} at Engine channel 1'
              + (f'; peak control: reducing {np.mean(red < -0.1) * 100:.0f} % of frames, mean {np.mean(red):.2f} dB, max {np.min(red):.1f} dB' if a.logic in ('loud', 'peak') else ''))
        if a.match and d == a.match:
            g2 = g * orig / lv
            print(f'  -> pack.ini gain {g2:.3f} matches the original engine; peak at that gain {np.max(np.abs(out)) * g2 / g / F.CH_ENGINE:.3f} at Engine channel 1'
                  f' (volume cap: MixGain 0.30 x gain x Volume <= 1 needs gain <= {1 / A.MixGain:.2f})')

if __name__ == '__main__':
    main()
