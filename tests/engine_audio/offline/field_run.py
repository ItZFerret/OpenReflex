# field_run.py -- runs field_mix on a trace + command stream: the table (peak / clipped % / rms, player vs field) for the countdown revs, the
# launch and the mid-race, and the review WAVs.
#   python3 field_run.py <trace.csv> <cmds.tsv> <out dir> <name prefix> [--tag before] [--ai-trim 1.0] [--pack NAME <EngineSounds dir>]
import sys, os, csv, argparse, numpy as np
import render as R, field_mix as F, after_logic
ap = argparse.ArgumentParser(); ap.add_argument('trace'); ap.add_argument('cmds'); ap.add_argument('out'); ap.add_argument('prefix')
ap.add_argument('--tag', default='before'); ap.add_argument('--ai-trim', type=float, default=1.0); ap.add_argument('--pack', nargs=2); ap.add_argument('--budget', type=float); ap.add_argument('--wav', action='store_true')
a = ap.parse_args()
pos, phase, locs = F.load_positions(a.trace)
f0 = min(f for f, p in phase.items() if p == 2); frames = max(pos); start = max(min(pos), f0 - 600); nfr = min(2400, frames - start)
player, field, act, st, trims = F.mix(a.trace, a.cmds, nfr, start, ai_trim=a.ai_trim, budget=a.budget)
if a.pack:
    pk, pdir = a.pack; trace = R.load_trace(a.trace, 0)
    pk_out = R.render(os.path.join(pdir, pk), trace, 'after', nfr / 60.0, start - min(pos)) * F.CH_ENGINE
    pk_out = pk_out[:len(player)] if len(pk_out) >= len(player) else np.pad(pk_out, (0, len(player) - len(pk_out)))
    player = pk_out; pL = pk_out * 0.7071; pR = pk_out * 0.7071   # the pack voices sit ahead of the camera: centred
pL, pR, fL, fR = st
mix = player + field
W = {'countdown (10 s before the lights)': (0, 600), 'launch (first 5 s)': (600, 900), 'race 5-20 s': (900, 1800), 'race 20-30 s': (1800, nfr)}
print(f"## {a.prefix} / {a.tag}   (race starts at trace frame {f0}; " + (f"field budget {a.budget}: AI trim {min(trims):.2f} .. {max(trims):.2f}, launch {np.mean(trims[600:900]):.2f}, mid-race {np.mean(trims[900:1800]):.2f})" if a.budget else "AI event volumes as the cmds file posts them)"))
print("| window | mix peak (mono bound / stereo) | clipped samples (mono bound / stereo) | mix rms | player rms | field rms | player vs field | audible AI voices (mean / max) |")
print("|---|---|---|---|---|---|---|---|")
for name, (lo, hi) in W.items():
    if hi <= lo: continue
    s = F.stats(mix, lo * F.PER, hi * F.PER); p = F.stats(player, lo * F.PER, hi * F.PER); fd = F.stats(field, lo * F.PER, hi * F.PER)
    ac = act[lo:hi]
    sl = F.stats(pL + fL, lo * F.PER, hi * F.PER); sr = F.stats(pR + fR, lo * F.PER, hi * F.PER)
    spk = max(sl['peak'], sr['peak']); scl = max(sl['clip'], sr['clip'])
    print(f"| {name} | {s['peak']:.2f} / {spk:.2f} | {s['clip']:.2f}% / {scl:.2f}% | {s['rms']:.3f} | {p['rms']:.3f} | {fd['rms']:.3f} | {F.db(p['rms'], fd['rms']):+.1f} dB | {np.mean(ac):.1f} / {max(ac)} |")
if a.wav:
    os.makedirs(a.out, exist_ok=True)
    import wave as W_
    n_ = 1800 * F.PER; pcm = (np.clip(np.stack([(pL + fL)[:n_], (pR + fR)[:n_]], 1), -1, 1) * 32767).astype('<i2').tobytes()
    with W_.open(os.path.join(a.out, f"{a.prefix}__{a.tag}.wav"), 'wb') as w: w.setnchannels(2); w.setsampwidth(2); w.setframerate(F.OUT); w.writeframes(pcm)
