# The before / after WAVs and the spectral table (w-enginepacks).
#   python3 batch.py <EngineSounds dir> <out dir> <trace csv (rpmtrace.c)>
# Each pack is rendered for 30 s of the local rider (rider 0, "player") and two of them for computer rider 2 ("ai-lap"), with the
# shipped logic (render.Before) and the fix (after_logic.After); prints a markdown table.
import sys, os, numpy as np, render as R
D = sys.argv[1]; OUT = sys.argv[2]; TRACE = sys.argv[3] if len(sys.argv) > 3 else 'trace_lap5.csv'
jobs = [(p, TRACE, 0, 'player') for p in ('HRC Honda 250 (Loud)', 'HRC Honda 250 (Soft Limiter)', 'HRC Honda 450', 'STAR Yamaha 250', 'STAR Yamaha 450')]
jobs += [(p, TRACE, 2, 'ai-lap') for p in ('HRC Honda 250 (Loud)', 'HRC Honda 450')]
print("| pack | trace | logic | >4 kHz energy vs raw (heard) | peak | samples clipped | rms | limiter starts | limiter audible |")
print("|---|---|---|---|---|---|---|---|---|")
for pack, tr, rider, tag in jobs:
    trace = R.load_trace(tr, rider)
    for logic in ('before', 'after'):
        st = {}; out = R.render(os.path.join(D, pack), trace, logic, 30, 0, stats=st)
        heard = np.clip(out, -1, 1)
        W = st['weights']; tw = sum(W.values()); raw = sum(W[k] * st['raw_hf'][k] for k in W) / tw
        hf_heard = R.hf_fraction(heard, R.OUT_RATE); hf_sum = R.hf_fraction(out, R.OUT_RATE)
        name = f"{pack.replace(' ', '_').replace('(', '').replace(')', '')}__{tag}__{logic}.wav"
        R.write_wav(os.path.join(OUT, name), out)
        print(f"| {pack} | {tag} | {logic} | {hf_heard/raw:.2f} (mix before the output clip {hf_sum/raw:.2f}) | {st['peak']:.2f} | {st['clipped']*100:.1f}% | {st['rms']:.3f} | {st['trig']} in 30 s | {st['limaudible']/st['frames']*100:.1f}% |")
