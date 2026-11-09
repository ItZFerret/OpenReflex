# Before / after renders and numbers for the audio rpm / load model (w-packrpm). "before" = the game's pack logic on the core's raw rpm /
# throttle (after_logic.Loud: the w-packloud loudness + peak control); "after" = the same logic with the voices on rpm_model.Model's audio rpm /
# throttle (the limiter still on the true values), "after_stretch" = the same with Stretch > 1.
#   python3 shift_render.py --trace <rpmtrace csv> --rider 2 --pack NAME=<pack dir> [--pack ...] --out <wav dir> [--race-start 0]
#                           [--section auto] [--stretch 1.35]
# Numbers per render (no audio needed): loop changes per second (the loudest loop of a heard layer changes), on / off layer flips per
# second (the louder layer changes), chatter (a loop change undone within 0.25 s), crossfade overlap (time with a second voice of the
# same layer above half the loudest), the per-frame pitch movement of what is heard (semitones of the pack rpm after the player's own easing):
# p95 / max and lurches per second (> 0.5 semitone in one 60 Hz frame), and the load dips.
import argparse, os, sys, math, csv
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'offline'))
import render as R, after_logic as A, packs as P
import rpm_model as M

def load(path, rider):
    rows = [r for r in csv.DictReader(open(path)) if r['frame'] not in ('C', 'F') and int(r['rider']) == rider and r.get('phase', '2') == '2']
    return [(float(r['rpm']), float(r['idle']), float(r['max']), float(r['thr']), int(r['air']), int(r['gear'])) for r in rows]

def shaped(trace, stretch=None, fps=60):
    m = M.Model(stretch); return [m.step(r, i, x, t, g, a, 1.0 / fps) for (r, i, x, t, a, g) in trace]

class ShapedLoud(A.Loud):
    name = 'loud'
    def __init__(self, p, audio, start):
        super().__init__(p, 1.0, 0.7); self.audio = audio; self.k = start
    def frame(self, rpm, idleR, maxR, thrIn, dt, air):
        ar, at = self.audio[min(self.k, len(self.audio) - 1)]; self.k += 1
        return A.Loud.frame(self, rpm, idleR, maxR, thrIn, dt, air, ar, at)

def numbers(pack, trace, audio, start, frames):
    p = P.load(pack)
    lg = A.Loud(p, 1.0, 0.7) if audio is None else ShapedLoud(p, audio, start)
    lg.setup([1.0] * sum(len(l) for l in p.layers), 1.0)
    nl0 = len(p.layers[0]); dom = []; semis = []; overlap = 0; prev = None; ons = []; loopch = 0; flips = 0; last = [None, None]; lastL = None
    chg = [[], []]; chatter = 0                                         # (frame, from, to) per layer: a change undone within 0.25 s = chatter
    for k in range(frames):
        rpm, idle, mx, thr, air, gear = trace[min(start + k, len(trace) - 1)]
        vols, pitches, *_ = lg.frame(rpm, idle, mx, thr, 1 / 60, air)
        v = np.array(vols); i = int(np.argmax(v)); dom.append(i)
        tot = v.sum() + 1e-12
        for li, (a, b) in enumerate(((0, nl0), (nl0, len(v)))):       # a change of loop inside a layer that is heard (>= 30 % of the mix)
            j = a + int(np.argmax(v[a:b]))
            if v[a:b].sum() / tot >= 0.3:
                if last[li] is not None and j != last[li]:
                    loopch += 1
                    if chg[li] and k - chg[li][-1][0] <= 15 and chg[li][-1][1] == j: chatter += 1
                    chg[li].append((k, last[li], j))
                last[li] = j
        L = 0 if v[:nl0].sum() >= v[nl0:].sum() else 1
        if lastL is not None and L != lastL: flips += 1
        lastL = L
        for a, b in ((0, nl0), (nl0, len(v))):
            seg = np.sort(v[a:b])[::-1]
            if len(seg) > 1 and seg[0] > 1e-3 and seg[1] > 0.5 * seg[0]: overlap += 1
        n = lg.rpmNorm
        if prev is not None and prev > 0 and n > 0: semis.append(12 * math.log2(n / prev))
        prev = n; ons.append(lg.thr)
    dom = np.array(dom); s = np.abs(np.array(semis)); sec = frames / 60
    return dict(loop_changes_per_s=loopch / sec, chatter_per_s=chatter / sec, layer_flips_per_s=flips / sec, overlap_pct=overlap / (2 * frames) * 100,
                pitch_p95=float(np.percentile(s, 95)), pitch_max=float(s.max()), lurches_per_s=float(np.sum(s > 0.5) / sec))

def busiest(trace, seconds, fps=60):
    g = np.array([t[5] for t in trace]); ch = np.r_[0, (np.diff(g) != 0).astype(int)]; w = int(seconds * fps)
    c = np.convolve(ch, np.ones(w), 'valid'); i = int(np.argmax(c)); return i, int(c[i])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--trace', required=True); ap.add_argument('--rider', type=int, default=2)
    ap.add_argument('--pack', action='append', required=True); ap.add_argument('--out', default=None)
    ap.add_argument('--race-start', type=int, default=0); ap.add_argument('--race-seconds', type=float, default=30.0)
    ap.add_argument('--section-seconds', type=float, default=20.0); ap.add_argument('--stretch', type=float, default=1.35)
    a = ap.parse_args()
    tr = load(a.trace, a.rider)
    sec0, nsh = busiest(tr, a.section_seconds)
    print(f'trace {os.path.basename(a.trace)} rider {a.rider}: {len(tr) / 60:.0f} s; the {a.section_seconds:.0f} s section with the most shifts starts at {sec0 / 60:.1f} s ({nsh} gear changes)')
    after = shaped(tr); after_s = shaped(tr, a.stretch)
    for spec in a.pack:
        name, d = spec.split('=', 1)
        for label, start, secs in (('race', a.race_start, a.race_seconds), ('section', sec0, a.section_seconds)):
            fr = int(secs * 60)
            for tag, audio in (('before', None), ('after', after), ('after_stretch', after_s)):
                nb = numbers(d, tr, audio, start, fr)
                print(f'  {name:26s} {label:7s} {tag:13s} loop changes {nb["loop_changes_per_s"]:5.1f}/s (chatter {nb["chatter_per_s"]:4.1f}/s), on/off flips {nb["layer_flips_per_s"]:4.1f}/s, crossfade overlap {nb["overlap_pct"]:4.1f} %,'
                      f' pitch / frame p95 {nb["pitch_p95"]:.2f} st, max {nb["pitch_max"]:.1f} st, lurches > 0.5 st {nb["lurches_per_s"]:4.1f}/s')
                if a.out:
                    R.LOGIC['x'] = (lambda p: A.Loud(p, 1.0, 0.7)) if audio is None else (lambda p, au=audio, st=start: ShapedLoud(p, au, st))
                    out = R.render(d, tr, 'x', secs, start) * 0.7
                    suffix = '' if label == 'race' else '__section'
                    R.write_wav(os.path.join(a.out, f'shift__{name}{suffix}__{tag}.wav'), out)

if __name__ == '__main__':
    main()
