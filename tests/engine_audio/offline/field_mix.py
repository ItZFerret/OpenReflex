# field_mix.py -- the engine field of a 12-rider start (w-aiengine): mixes every rider's original engine events the way OriginalAudio does
# (event layers over AIFX, volume / pitch / low-pass envelopes, the Engine channel gain, FMOD linear rolloff to the listener, max playbacks
# kept loudest-first) from the FMOD command stream the real emitter (engine_logic/, OriginalEngineAudio.cs) issued on a recorded trace
# (aitrace.c), and optionally puts a Steeler pack in the local rider's place (render.py). numpy only.
#   python3 field_mix.py <trace.csv> <cmds.tsv> <out dir> [--pack "HRC Honda 250 (Loud)" <EngineSounds dir>] [--tag before] [--ai-trim X] [--no-clip-wav]
import sys, os, csv, json, math, wave, argparse
import numpy as np
import render as R
OUT = 48000; PER = 800
AUD = os.environ.get('REFLEX_AUDIO_EXPORT', '/Users/payton/Documents/Development/REFLEX/_re/formats/out/audio')
CH_ENGINE = 0.7        # the Engine channel in a 12-rider NAT race (AudioVolumeMixEntity, w-sfx: NAT_Concord / Stillwater Engine 0.7)

class Lib:
    def __init__(self):
        self.ev = json.load(open(os.path.join(AUD, 'events.json')))['events']; self.wav = {}
    def event(self, path):
        for k in (path, ): 
            if k in self.ev: return self.ev[k]
        low = path.lower()
        for k in self.ev:
            if k.lower() == low: return self.ev[k]
        raise KeyError(path)
    def data(self, f, cutoff=None):
        key = (f, int(cutoff // 100) if cutoff else 0)
        if key not in self.wav:
            if (f, 0) not in self.wav: self.wav[(f, 0)] = R.read_wav(os.path.join(AUD, f))
            d, r = self.wav[(f, 0)]
            if cutoff and cutoff < 0.45 * r:           # FMOD Lowpass Simple: one pole, zero phase here (magnitude only)
                F = np.fft.rfft(d); fr = np.fft.rfftfreq(len(d), 1.0 / r)
                d = np.fft.irfft(F / np.sqrt(1 + (fr / cutoff) ** 2), len(d))
            self.wav[key] = (d, r)
        return self.wav[key]

def evaluate(env, v):
    pts = env['points']; xs = [p['x'] for p in pts]; ys = [p['y'] for p in pts]; sh = [p['shape'] for p in pts]
    if v <= xs[0]: return ys[0]
    for i in range(1, len(xs)):
        if v > xs[i]: continue
        t = (v - xs[i-1]) / (xs[i] - xs[i-1]) if xs[i] > xs[i-1] else 1.0
        if sh[i] == 1: t = t * t * (3 - 2 * t)
        return ys[i-1] + (ys[i] - ys[i-1]) * t
    return ys[-1]

class Seg:
    pass

voltl = {}   # wrapper -> [(frame, event volume)] posted by the emitter (the field trim)
def parse_cmds(path):
    """-> segments, wrapper -> slot, rider AIFX / local flags"""
    segs = []; cur = {}; wslot = {}; local = set(); voltl.clear()
    for l in open(path):
        c = l.rstrip('\n').split('\t'); f = int(c[0]); k = c[1]
        if k == 'map': wslot[int(c[2])] = int(c[3])
        elif k == 'rider' and c[4] == '1': local.add(int(c[2]))
        elif k == 'play':
            w = int(c[2]); 
            if w in cur: cur[w].end = f
            s = Seg(); s.w = w; s.path = c[3]; s.entry = int(c[4]); s.start = f; s.end = 10**9; s.aifx = 0.0; s.pos = 0.0
            segs.append(s); cur[w] = s
        elif k in ('stop', 'end'):
            w = int(c[2])
            if w in cur: cur[w].end = f; del cur[w]
        elif k == 'vol': voltl.setdefault(int(c[2]), []).append((f, float(c[3])))
        elif k == 'param' and c[3] == 'AIFX':
            w = int(c[2])
            if w in cur: cur[w].aifx = float(c[4])
    for s in segs: s.slot = wslot.get(s.w, -1)
    return segs, local

def load_positions(trace):
    pos = {}   # frame -> slot -> (x,y,z); phase per frame
    phase = {}; locs = {}
    for r in csv.DictReader(open(trace)):
        if r['frame'] in ('C', 'F'): continue
        f = int(r['frame']); pos.setdefault(f, {})[int(r['slot'])] = (float(r['px']), float(r['py']), float(r['pz']))
        phase[f] = int(r['phase'])
        if int(r['flags']) & 4: locs[f] = int(r['slot'])
    return pos, phase, locs

def build(lib, ev, seg, rider_slot_local):
    """per-segment static layer data at its AIFX: list of (file, gain, pitch, cutoff)"""
    out = []
    params = {p['name']: p for p in ev['parameters']}
    ap = params.get('AIFX'); 
    for L in ev['layers']:
        cp = L['control_param']; p = params.get(cp)
        v = 0.0
        if p and p['max'] > p['min']: v = min(1.0, max(0.0, (seg.aifx - p['min']) / (p['max'] - p['min']))) if cp == 'AIFX' else 0.0
        vol = 1.0; pitch = 1.0; cutoff = None
        for e in L['envelopes']:
            ep = params.get(e.get('control_param')) or p
            ev_v = v if (ep is None or ep['name'] == 'AIFX') else 0.0
            y = evaluate(e, ev_v)
            if e['kind'] == 'volume': vol *= y
            elif e['kind'] == 'pitch': pitch *= 2 ** (4 * (2 * y - 1))
            elif e['kind'] == 'dsp':
                d = e['dsp']
                if d['name'] == 'FMOD Lowpass Simple':
                    mn, mx = d['param_min'], d['param_max']; c = mn * (mx / mn) ** y if d.get('log_interpolation') else mn + y * (mx - mn)
                    if c < 19000: cutoff = c
        for si in L['sound_instances']:
            if L['control_param'] and not (si['start'] - 1e-6 <= v <= si['start'] + si['length'] + 1e-6): continue
            sd = ev['sound_defs'].get(si['sound_def']);
            if not sd: continue
            ents = [en for en in sd['entries'] if en.get('file')]
            if not ents: continue
            en = ents[seg.entry % len(ents)]
            g = ev['volume'] * vol * sd.get('volume', 1.0) * si['gain'] * (1 - (1 - sd.get('volume_randomization_min', 1.0)) / 2)
            ps = 2 ** (ev.get('pitch_octaves', 0.0) + sd.get('pitch_octaves', 0.0)) * pitch
            out.append(dict(file=en['file'], g=g, pitch=ps, cutoff=cutoff, loop=si['loop_mode'] == 'loop'))
    return out

def mix(trace, cmds, nframes, start, ai_trim=1.0, budget=None, smooth=0.15, ch=CH_ENGINE, drop_local=False, lib=None, rolloff=True, maxpb=True, label=''):
    lib = lib or Lib()
    segs, local = parse_cmds(cmds); pos, phase, locs = load_positions(trace)
    for s in segs:
        ev = lib.event(s.path); s.ev = ev; s.layers = build(lib, ev, s, None); s.islocal = s.slot in local
        s.maxpb = ev['max_playbacks']; s.mind = ev['min_distance']; s.maxd = ev['max_distance']; s.is3d = ev['mode']['is_3d']
        for l in s.layers: l['pos'] = 0.0
    n = nframes * PER
    player = np.zeros(n); field = np.zeros(n)                 # mono, no panning loss (the pessimistic bound: every source centred at unity)
    pL = np.zeros(n); pR = np.zeros(n); fL = np.zeros(n); fR = np.zeros(n)   # stereo, equal-power pan to the listener's right vector
    # listener forward = the local rider's travel over the first 5 s of the race (horizontal), right = up x forward (Unity's left-handed)
    ph0 = min(f for f, p in phase.items() if p == 2); a_, b_ = pos[ph0 + 60][0], pos[ph0 + 300][0]
    fw = np.array([b_[0] - a_[0], b_[2] - a_[2]]); fw /= max(1e-9, np.hypot(*fw)); right = np.array([fw[1], -fw[0]])
    active_hist = []; trim_hist = []; cur_trim = None
    for k in range(nframes):
        f = start + k
        lis = pos.get(f, {}).get(locs.get(f, 0))
        if budget is not None:     # the field budget (OriginalEngineAudio.FieldTrim): power of the AI engines capped at `budget` engines' worth
            wsum = 0.0
            for sl_, pp in pos.get(f, {}).items():
                if sl_ == locs.get(f, 0): continue
                d = math.dist(pp, lis); ro = 1.0 if d <= 1.0 else 0.0 if d >= 300.0 else 1 - (d - 1.0) / 299.0; wsum += ro * ro
            raw = min(1.0, math.sqrt(budget / wsum)) if wsum > 1e-9 else 1.0
            cur_trim = raw if cur_trim is None else cur_trim + (raw - cur_trim) * (1 - math.exp(-(1 / 60.0) / smooth))
            ai_trim = cur_trim
        trim_hist.append(ai_trim)
        act = [s for s in segs if s.start <= f < s.end]
        vol_now = {}
        for w, tl in voltl.items():
            v = 1.0
            for ff, vv in tl:
                if ff <= f: v = vv
                else: break
            vol_now[w] = v
        aud = []
        for s in act:
            p = pos.get(f, {}).get(s.slot)
            ro = 1.0
            if rolloff and s.is3d and p and lis:
                d = math.dist(p, lis); ro = 1.0 if d <= s.mind else 0.0 if d >= s.maxd else 1 - (d - s.mind) / (s.maxd - s.mind)
            s.cur_ro = ro
            pan = 0.0
            if p and lis:
                rel = np.array([p[0] - lis[0], p[2] - lis[2]]); dn = np.hypot(*rel)
                if dn > 1.0 and not s.islocal: pan = float(np.clip(rel.dot(right) / dn, -1, 1))
            th = (pan + 1) * math.pi / 4; s.gl, s.gr = math.cos(th), math.sin(th)
            tg = sum(l['g'] for l in s.layers)
            s.vol = vol_now.get(s.w, 1.0); s.aud = tg * ro * ch * s.vol * (1.0 if s.islocal else ai_trim)
            aud.append(s)
        if maxpb:
            by = {}
            for s in aud: by.setdefault(s.path, []).append(s)
            for l in by.values():
                if len(l) > l[0].maxpb:
                    l.sort(key=lambda s: -s.aud)
                    for s in l[l[0].maxpb:]: s.virt = True
                    for s in l[:l[0].maxpb]: s.virt = False
                else:
                    for s in l: s.virt = False
        else:
            for s in aud: s.virt = False
        n_aud = 0
        for s in aud:
            for l in s.layers:
                d, rate = lib.data(l['file'], l['cutoff'])
                step = rate / OUT * l['pitch']; idx = l['pos'] + np.arange(PER) * step
                if l['loop']: i = np.floor(idx).astype(np.int64); fr = idx - i; x = d[i % len(d)] * (1 - fr) + d[(i + 1) % len(d)] * fr; l['pos'] = (l['pos'] + PER * step) % len(d)
                else:
                    ok = idx < len(d) - 1; x = np.zeros(PER); i = np.floor(idx[ok]).astype(np.int64); fr = idx[ok] - i; x[ok] = d[i] * (1 - fr) + d[i + 1] * fr; l['pos'] += PER * step
                if s.virt: continue
                g = l['g'] * s.cur_ro * ch * s.vol * (1.0 if s.islocal else ai_trim)
                y = x * min(1.0, g)       # Unity AudioSource.volume is clamped to 1
                seg = slice(k * PER, (k + 1) * PER)
                if s.islocal: player[seg] += y; pL[seg] += y * s.gl; pR[seg] += y * s.gr
                else: field[seg] += y; fL[seg] += y * s.gl; fR[seg] += y * s.gr
            if not s.virt and s.aud > 0.001: n_aud += 1
        active_hist.append(n_aud)
    if drop_local: pass
    return player, field, active_hist, (pL, pR, fL, fR), trim_hist

def stats(x, lo=0, hi=None):
    y = x[lo:hi]; return dict(peak=float(np.max(np.abs(y))), rms=float(np.sqrt(np.mean(y ** 2))), clip=float(np.mean(np.abs(y) >= 1.0) * 100))

def db(a, b): return 20 * math.log10(max(a, 1e-9) / max(b, 1e-9))
