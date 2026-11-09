# landing_mix.py -- the landing "clank" of the foley emitter, offline (w-landclank): replays OriginalGameSound.FoleyTick's bike Landing on a recorded
# trace (aitrace.c with LANDTRACE_OUT: the manager wheel groups of every rider) and mixes it over the engine bed (field_mix.py: the original
# engine events, or a Steeler pack as the local rider's engine) the way OriginalAudio does (event volume x sound volume x random, the Effects
# channel mix, linear rolloff 10 .. 100 ft to the listener, max playbacks 3 ranked loudest-first, equal-power pan). `Before` = main ef890bb,
# `After` = the fix (keep it in step with OriginalGameSound.FilteredLanding and OriginalAudio.KeyOff / VirtualRamp). numpy only (/usr/bin/python3).
#   python3 landing_mix.py <trace.csv> <wheels.csv> <cmds.tsv> <out dir> --start F [--frames N] [--local RIDER] [--pack NAME <EngineSounds dir>] [--logic before|after] [--wav NAME.wav]
# The trace is aitrace.c's (engine telemetry + positions), wheels.csv its LANDTRACE_OUT, cmds.tsv engine_logic's FMOD command stream of the same trace.
import sys, os, math, wave, argparse, collections
import numpy as np
import render as R, field_mix as F

OUT = F.OUT; PER = F.PER
EV = 'VEHICLE_FOLEY_01/BIKE_FOLEY/LANDING'
CH_EFFECTS = 0.6            # NAT_Concord / Stillwater Effects (AudioVolumeMixEntity)
FPS = 60.0
VIRTUAL_RAMP = 0.03         # OriginalAudio.VirtualRamp

class Before:
    """main ef890bb: every rear-wheel InAir -> ground edge whose suspension force is >= 100 starts the clank at full level; a clank still playing is cut (Stop now)."""
    name = 'before'; ramp = 0.0; keyoff = False
    def __init__(self, pack=False): pass
    def step(self, st, w0, now, local=False):
        if st['prev_air'] == 1 and w0['air'] == 0 and w0['susp'] >= 100: return True, 1.0
        return False, 1.0

# OriginalGameSound.LandingXxx (the same constants)
LandingMinAir = 0.15; LandingWindow = 0.05; LandingForceSpan = 350.0; LandingQuietest = 0.35
LandingGain = 0.85; LandingPackTrim = 1.0; LandingCooldown = 0.30; Threshold = 100.0   # LandingPackTrim: w-packloud 1 (w-landclank 0.35: the packs played 7 .. 12 dB under the original)
LANDING_PACK_TRIM = LandingPackTrim   # --pack-trim overrides it
class After:
    name = 'after'; ramp = VIRTUAL_RAMP; keyoff = True
    def __init__(self, pack=False): self.pack = pack
    def step(self, st, w0, now, local=False):
        land = st['prev_air'] == 1 and w0['air'] == 0
        t = st.get('touch')
        if land and st['last_air'] >= LandingMinAir and not (t and t['on']): t = st['touch'] = dict(on=True, age=0.0, peak=w0['susp'])
        elif t and t['on']:
            if w0['air']: t['on'] = False
            else: t['peak'] = max(t['peak'], w0['susp']); t['age'] += 1 / FPS
        best = -1.0
        if t and t['on'] and (t['age'] >= LandingWindow or t['peak'] >= Threshold + LandingForceSpan):
            t['on'] = False
            if t['peak'] >= Threshold: best = t['peak']
        if best < 0 or now - st['last_clank'] < LandingCooldown: return False, 1.0
        x = min(1.0, max(0.0, (best - Threshold) / LandingForceSpan)); x = x * x * (3 - 2 * x)
        return True, LandingGain * (LandingQuietest + (1 - LandingQuietest) * x) * (LANDING_PACK_TRIM if self.pack else 1.0)

def load_wheels(path):
    W = collections.defaultdict(dict)      # frame -> rider -> [rear, front] {air, susp}
    for ln in open(path):
        p = ln.rstrip('\n').split(',')
        if p[0] != 'W': continue
        f = int(p[1]); r = int(p[2]); b = 4
        W[f][r] = [dict(air=int(p[b + 7 * i]), susp=float(p[b + 7 * i + 1])) for i in range(2)]
    return W

def simulate(logic, W, frames, rng, files, lens, local):
    riders = sorted({r for f in W.values() for r in f})
    state = {r: dict(prev_air=None, last_air=0.0, air_s=0.0, last_clank=-9.0, cur=None, touch=None) for r in riders}
    voices = []; last_file = {}
    for f in frames:
        now = f / FPS
        for r in riders:
            w = W.get(f, {}).get(r)
            if w is None: continue
            st = state[r]; w0 = w[0]
            land = st['prev_air'] == 1 and w0['air'] == 0
            if w0['air']: st['air_s'] += 1 / FPS
            elif land: st['last_air'] = st['air_s']
            if not w0['air']: st['air_s'] = 0.0
            play, scale = logic.step(st, w0, now, local=(r == local))
            st['prev_air'] = w0['air']
            if not play: continue
            cur = st['cur']
            if cur is not None and cur['end'] > f and not logic.keyoff: cur['end'] = f; cur['cut'] = True    # the original port: Stop now + Play
            while True:
                i = int(rng.integers(len(files)))
                if i != last_file.get(r): break
            last_file[r] = i
            v = dict(rider=r, f0=f, file=files[i], rand=1 - rng.random() * 0.1596, scale=scale, end=f + int(math.ceil(lens[files[i]] * FPS)) + 1, cut=False)
            voices.append(v); st['cur'] = v; st['last_clank'] = now
    return voices

def render(lib, logic, voices, pos, locs, start, nfr, right, ev):
    n = nfr * PER; L = np.zeros(n); Rr = np.zeros(n)
    mind, maxd, evvol = ev['min_distance'], ev['max_distance'], ev['volume']
    data = {}
    def get(f):
        if f not in data: data[f] = lib.data(f)
        return data[f]
    live = [v for v in voices if v['f0'] < start + nfr and v['end'] >= start]
    for v in live: v['vg'] = None     # None: not ticked yet (it takes its rank at once)
    steps = []     # click heights: the level step of a cut or of a muted voice (sample units of the final mix)
    for k in range(nfr):
        f = start + k
        act = [v for v in live if v['f0'] <= f <= v['end']]
        if not act: continue
        lis = pos.get(f, {}).get(locs.get(f, 0))
        rows = []
        for v in act:
            p = pos.get(f, {}).get(v['rider'])
            ro = 1.0; pan = 0.0
            if p and lis:
                d_ = math.dist(p, lis); ro = 1.0 if d_ <= mind else 0.0 if d_ >= maxd else 1 - (d_ - mind) / (maxd - mind)
                rel = np.array([p[0] - lis[0], p[2] - lis[2]]); dn = np.hypot(*rel)
                if dn > 1.0 and v['rider'] != locs.get(f, 0): pan = float(np.clip(rel.dot(right) / dn, -1, 1))
            g = evvol * v['rand'] * CH_EFFECTS * v['scale'] * ro
            th = (pan + 1) * math.pi / 4
            rows.append([v, g, math.cos(th), math.sin(th)])
        rows.sort(key=lambda r: -r[1])
        for i, (v, g, gl, gr) in enumerate(rows):
            virt = i >= 3
            if v['vg'] is None: v['vg'] = 0.0 if virt else 1.0
            vg0 = v['vg']
            if logic.ramp > 0: v['vg'] = min(1.0, max(0.0, vg0 + (-1 if virt else 1) * (1 / FPS) / logic.ramp))
            else: v['vg'] = 0.0 if virt else 1.0
            d, rate = get(v['file']); step = rate / OUT
            idx = (f - v['f0']) * PER * step + np.arange(PER) * step
            ok = idx < len(d) - 1
            x = np.zeros(PER); ii = np.floor(idx[ok]).astype(np.int64); fr = idx[ok] - ii; x[ok] = d[ii] * (1 - fr) + d[ii + 1] * fr
            level = x * min(1.0, g)
            if v['cut'] and f == v['end']:                    # cut here: the level this block would have started at is the step
                steps.append(abs(level[0]) * vg0); continue
            y = level * v['vg']
            s = slice(k * PER, (k + 1) * PER); L[s] += y * gl; Rr[s] += y * gr
            if abs(v['vg'] - vg0) * float(np.max(np.abs(level))) > 0.02 and logic.ramp == 0: steps.append(float(np.max(np.abs(level))) * abs(v['vg'] - vg0))
    return L, Rr, steps

def a_weight(x, rate=OUT):
    F_ = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / rate); f2 = f * f
    ra = (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2))
    return np.fft.irfft(F_ * ra / 0.7943, len(x))     # the A curve's response at 1 kHz is 0.7943 of its peak form: normalised to 0 dB there

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('trace'); ap.add_argument('wheels'); ap.add_argument('cmds'); ap.add_argument('out')
    ap.add_argument('--pack', nargs=2); ap.add_argument('--local', type=int, default=0); ap.add_argument('--start', type=int, required=True); ap.add_argument('--frames', type=int, default=1800)
    ap.add_argument('--wav'); ap.add_argument('--logic', default='before'); ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--pack-logic', default='peak', help="the pack player: 'peak' = w-packpeak (the game), 'loud' = w-packloud, 'after' = w-enginepacks (MixGain 0.30)")
    ap.add_argument('--pack-trim', type=float, help='LandingPackTrim (default: the module value)'); ap.add_argument('--size', type=int, default=4, help='the engine size the pack replaces (loudness target)')
    a = ap.parse_args()
    global LANDING_PACK_TRIM
    if a.pack_trim is not None: LANDING_PACK_TRIM = a.pack_trim
    import after_logic; after_logic.ENGINE_SIZE = a.size
    lib = F.Lib(); e = lib.event(EV); sd = list(e['sound_defs'].values())[0]
    files = [en['file'] for en in sd['entries'] if en.get('file')]
    lens = {f: len(lib.data(f)[0]) / lib.data(f)[1] for f in files}
    W = load_wheels(a.wheels); pos, phase, locs = F.load_positions(a.trace)
    logic = {'before': Before, 'after': After}[a.logic](pack=bool(a.pack))
    voices = simulate(logic, W, sorted(W), np.random.default_rng(a.seed), files, lens, a.local)
    start, nfr = a.start, a.frames
    ph0 = min(f for f, p in phase.items() if p == 2); a_, b_ = pos[ph0 + 60][a.local], pos[ph0 + 300][a.local]
    fw = np.array([b_[0] - a_[0], b_[2] - a_[2]]); fw /= max(1e-9, np.hypot(*fw)); right = np.array([fw[1], -fw[0]])
    player, field, act, (pL, pR, fL, fR), trims = F.mix(a.trace, a.cmds, nfr, start)
    if a.pack:
        pk, pdir = a.pack; trace = R.load_trace(a.trace, a.local)
        pk_st = {}; pk_out = R.render(os.path.join(pdir, pk), trace, a.pack_logic, nfr / FPS, start - min(pos), stats=pk_st) * F.CH_ENGINE
        pk_out = pk_out[:len(player)] if len(pk_out) >= len(player) else np.pad(pk_out, (0, len(player) - len(pk_out)))
        pL = pk_out * 0.7071; pR = pk_out * 0.7071; player = pk_out
    lL, lR, steps = render(lib, logic, voices, pos, locs, start, nfr, right, e)
    mixL = pL + fL + lL; mixR = pR + fR + lR
    inwin = [v for v in voices if start <= v['f0'] < start + nfr]
    mine = [v for v in inwin if v['rider'] == a.local]
    eng = np.stack([pL + fL, pR + fR], 1); lan = np.stack([lL, lR], 1); own = np.stack([pL, pR], 1)
    def short(x):
        m = (x[:, 0] + x[:, 1]) / 2; w = a_weight(m); b = int(0.125 * OUT); k = len(w) // b
        return np.sqrt(np.mean(w[:k * b].reshape(k, b) ** 2, axis=1))
    se, sl, so = short(eng), short(lan), short(own)
    eng_ref = float(np.sqrt(np.mean(se ** 2))); own_ref = float(np.sqrt(np.mean(so ** 2))); mx = float(np.max(sl)) if len(sl) else 0.0
    db = lambda x, y: 20 * math.log10(max(x, 1e-9) / max(y, 1e-9))
    print(f"## {a.logic} / bed {'pack ' + a.pack[0] if a.pack else 'original engine'} / local rider {a.local}, frames {start}..{start + nfr} ({nfr / FPS:.0f} s)")
    print(f"clanks started: {len(inwin)} (all riders), local rider {len(mine)} = {len(mine) / (nfr / FPS) * 60:.1f} per minute; cut mid-sample by a newer clank: {sum(1 for v in inwin if v['cut'])}; "
          f"level steps over 0.02 (cuts, muted voices): {sum(1 for s_ in steps if s_ > 0.02)}, largest {max(steps) if steps else 0:.3f}")
    print(f"A-weighted 125 ms levels: engine bed {eng_ref:.4f}, local engine alone {own_ref:.4f}, loudest clank {mx:.4f} -> clank vs engine bed {db(mx, eng_ref):+.1f} dB, vs the local engine alone {db(mx, own_ref):+.1f} dB; "
          f"clank peak sample {np.max(np.abs(lan)):.3f}, mix peak {max(np.max(abs(mixL)), np.max(abs(mixR))):.2f}")
    if a.pack:
        red = np.array(pk_st.get('reduction_db', [0.0]))
        print(f"pack alone: peak {np.max(np.abs(player)):.3f}, clipped {np.mean(np.abs(player) > 1) * 100:.2f}%, peak control: reduced {np.mean(red < -0.05) * 100:.1f}% of frames, mean {np.mean(red):.2f} dB, max {np.min(red):.2f} dB; limiter starts {pk_st['trig']}")
    if a.wav:
        os.makedirs(a.out, exist_ok=True)
        pcm = (np.clip(np.stack([mixL, mixR], 1), -1, 1) * 32767).astype('<i2').tobytes()
        with wave.open(os.path.join(a.out, a.wav), 'wb') as w: w.setnchannels(2); w.setsampwidth(2); w.setframerate(OUT); w.writeframes(pcm)
        print('wrote', os.path.join(a.out, a.wav))

if __name__ == '__main__': main()
