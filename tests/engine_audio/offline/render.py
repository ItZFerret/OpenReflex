# Offline renderer of EnginePackPlayer (Unity/Reflex/Assets/Scripts/OriginalWorld/EnginePackPlayer.cs): mixes a pack from a recorded
# rpm / throttle trace (rpmtrace.c CSV) exactly as the player's Update does per render frame, Unity-style linear-interpolation resampling
# per voice, and writes a 48 kHz 16-bit WAV.  Logic "before" = the shipped 6a31b6f code; "after" = the fix (kept in sync with the C#).
import sys, os, csv, struct, math, json
import numpy as np
import packs as P
OUT_RATE = 48000

def read_wav(path):
    b = open(path, 'rb').read(); p = 12; fmt = ch = rate = bits = 0; data = None
    while p + 8 <= len(b):
        cid = b[p:p+4]; ln = struct.unpack('<I', b[p+4:p+8])[0]; body = p + 8
        if cid == b'fmt ': fmt, ch, rate, _, _, bits = struct.unpack('<HHIIHH', b[body:body+16])
        elif cid == b'data': data = b[body:body+ln]; break
        p = body + ln + (ln & 1)
    assert fmt == 1 and bits == 16, (path, fmt, bits)
    a = np.frombuffer(data[:len(data)//(2*ch)*2*ch], dtype='<i2').astype(np.float64) / 32768.0
    return a.reshape(-1, ch).mean(axis=1), rate

def write_wav(path, x, rate=OUT_RATE):
    y = np.clip(x, -1, 1); pcm = (y * 32767).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16) + b'data' + struct.pack('<I', len(pcm)) + pcm)

def vol_at(s, n):
    k = len(s.val)
    if k == 0: return 0.0
    if n <= s.val[0]: return s.vol[0]
    for i in range(1, k):
        if n <= s.val[i]:
            d = s.val[i] - s.val[i-1]; t = (n - s.val[i-1]) / d if d > 0 else 1.0
            return s.vol[i-1] + (s.vol[i] - s.vol[i-1]) * t
    return s.vol[-1]

def lerp(a, b, t): return a + (b - a) * t
def clamp01(x): return min(1.0, max(0.0, x))

class Voice:
    def __init__(self, data, rate, loop, rng):
        self.d = data; self.rate = rate; self.loop = loop; self.n = len(data)
        self.pos = float(rng.integers(0, max(1, self.n - 1))) if loop else 0.0
        self.playing = loop; self.vol = 0.0; self.pitch = 1.0
    def render(self, count, v0, v1, p0, p1, resampler):
        out = np.zeros(count)
        if not self.playing: return out
        # step per output sample = pitch * clip rate / output rate (pitch ramps across the block, as the volume does)
        steps = np.linspace(p0, p1, count, endpoint=False) * (self.rate / OUT_RATE)
        idx = self.pos + np.concatenate(([0.0], np.cumsum(steps)[:-1]))
        end = self.pos + steps.sum()
        if self.loop:
            i = np.floor(idx).astype(np.int64); f = idx - i
            a = self.d[i % self.n]; b = self.d[(i + 1) % self.n]
            out = a + (b - a) * f if resampler != 'hermite' else PM.hermite(self.d, idx, True)
            self.pos = end % self.n
        else:
            valid = idx < self.n - 1
            i = np.floor(idx[valid]).astype(np.int64); f = idx[valid] - i
            out[valid] = self.d[i] + (self.d[i+1] - self.d[i]) * f if resampler != 'hermite' else PM.hermite(self.d, idx[valid], False)
            self.pos = end
            if end >= self.n - 1: self.playing = False; self.pos = 0.0
        return out * np.linspace(v0, v1, count, endpoint=False)

def load_trace(path, rider):
    rows = [r for r in csv.DictReader(open(path)) if r['frame'] not in ('C', 'F') and int(r['rider']) == rider]   # aitrace.c's C / F rows are cues / rev ends
    return [(float(r['rpm']), float(r['idle']), float(r['max']), float(r['thr']), int(r['air']), int(r['gear'])) for r in rows]

class Before:
    """EnginePackPlayer.Update as shipped (6a31b6f)."""
    name = 'before'
    def __init__(self, p): self.p = p; self.rpmNorm = -1.0; self.thr = 0.0; self.atLim = False
    def frame(self, rpm, idleR, maxR, thrIn, dt, air):
        p = self.p
        if maxR <= 0: maxR = p.maxRpm
        if idleR <= 0 or idleR >= maxR: idleR = maxR * 0.12
        frac = clamp01((rpm - idleR) / (maxR - idleR))
        idleP = P.idle_norm(p) * p.maxValue; maxP = max(idleP + 1, p.maxRpm)
        n = clamp01((idleP + frac * (maxP - idleP)) / p.maxValue)
        self.rpmNorm = n if self.rpmNorm < 0 else lerp(self.rpmNorm, n, 1 - math.exp(-dt * 30))
        self.thr = lerp(self.thr, clamp01(thrIn), 1 - math.exp(-dt * 20))
        on = max(self.thr, p.idleThrottle) ** max(0.01, p.blendExp)
        vols = []; pitches = []
        for l in range(2):
            for s in p.layers[l]:
                mix = on if l == 0 else 1 - on
                vols.append(clamp01(0.9 * mix * vol_at(s, self.rpmNorm)))
                pt = 1.0 if (not p.autoPitch or s.ref <= 0) else min(max(self.rpmNorm / s.ref, s.minp), s.maxp)
                pitches.append(min(max(pt, 0.05), 3.0))
        limit = frac >= 0.985 and thrIn > 0.5
        rise = limit and not self.atLim; fall = (not limit) and self.atLim
        self.atLim = limit
        return vols, pitches, 0.9 * p.limiterVolume, rise, fall, limit

import after_logic
import packmix as PM
LOGIC = {'before': Before, 'after': after_logic.After, 'loud': after_logic.Loud, 'peak': after_logic.Peak}

def render(pack_dir, trace, logic_name, seconds, start_frame=0, fps=60, seed=1, stats=None):
    p = P.load(pack_dir); rng = np.random.default_rng(seed)
    logic = LOGIC[logic_name](p)
    samples = [s for l in range(2) for s in p.layers[l]]
    cache = {}
    def get(path):
        if path not in cache: cache[path] = read_wav(path)
        return cache[path]
    voices = [Voice(*get(s.wav), True, rng) for s in samples]
    lim = Voice(*get(p.limiterWav), False, rng) if p.limiterWav else None
    if lim: lim.playing = False
    if hasattr(logic, 'setup'): logic.setup([float(np.max(np.abs(v.d))) for v in voices], float(np.max(np.abs(lim.d))) if lim else 0.0)   # w-packloud: the peak control's sample peaks
    limLoop = False
    rs = 'hermite' if getattr(logic, 'mixer', False) else None   # w-packpeak: the game's own mixer (packmix.py)
    per = OUT_RATE // fps; frames = int(seconds * fps)
    out = np.zeros(frames * per); dt = 1.0 / fps
    weights = np.zeros(len(samples)); pitchw = np.zeros(len(samples))
    trig = 0; limframes = 0; limaudible = 0
    prev = [(0.0, 1.0)] * len(voices); prevlim = 0.0
    for k in range(frames):
        rpm, idleR, maxR, thr, air, gear = trace[min(start_frame + k, len(trace) - 1)]
        vols, pitches, limvol, rise, fall, limit = logic.frame(rpm, idleR, maxR, thr, dt, air)
        seg = slice(k * per, (k + 1) * per)
        for j, v in enumerate(voices):
            v0, p0 = prev[j]
            out[seg] += v.render(per, v0, vols[j], p0, pitches[j], rs)
            prev[j] = (vols[j], pitches[j]); weights[j] += vols[j] ** 2; pitchw[j] += vols[j] ** 2 * pitches[j]
        if lim:
            if rise: trig += 1; limLoop = True; lim.loop = False
            if rise and not lim.playing: lim.playing = True; lim.pos = 0.0
            if fall: limLoop = False
            if logic.name in ('after', 'loud', 'peak') and not limit and limvol <= 0 and lim.playing: lim.playing = False; lim.pos = 0.0
            if limit: limframes += 1
            if lim.playing:
                limaudible += 1
                buf = lim.render(per, prevlim, limvol, 1.0, 1.0, rs)
                if not lim.playing and limLoop: lim.playing = True; lim.pos = 0.0   # looping: restart at the end
                out[seg] += buf
            prevlim = limvol
    if getattr(logic, 'mixer', False):
        # w-packpeak: the lookahead limiter on the mixed pack output, Engine channel in (the caller multiplies by the channel again)
        ch = logic.channel; y, e = PM.limit(out * ch); out = y / ch
        if stats is not None:
            red = 20 * np.log10(np.maximum(e, 1e-9)); blocks = red[:frames * per].reshape(frames, per).min(axis=1)
            stats.update(limiter_frames_pct=float(np.mean(blocks < -0.01) * 100), limiter_samples_pct=float(np.mean(red < -0.01) * 100),
                         limiter_mean_db=float(np.mean(red)), limiter_max_db=float(np.min(red)), reduction_db=list(blocks))
    if stats is not None:
        stats.update(trig=trig, limframes=limframes, limaudible=limaudible, frames=frames,
                     weights={os.path.basename(s.wav): float(w) for s, w in zip(samples, weights)},
                     meanpitch={os.path.basename(s.wav): float(pw / w) if w > 0 else 0 for s, w, pw in zip(samples, weights, pitchw)},
                     clipped=float(np.mean(np.abs(out) > 1.0)), peak=float(np.max(np.abs(out))), rms=float(np.sqrt(np.mean(out ** 2))))
        if hasattr(logic, 'reductions') and 'reduction_db' not in stats: stats['reduction_db'] = list(logic.reductions)   # w-packloud: the peak control's gain reduction per frame (dB, <= 0)
        stats['raw_hf'] = {}
        for s in samples:
            d, r = get(s.wav); stats['raw_hf'][os.path.basename(s.wav)] = hf_fraction(d, r)
    return out

def hf_fraction(x, rate, cut=4000.0):
    n = 8192; x = np.asarray(x)
    if len(x) < n: x = np.pad(x, (0, n - len(x)))
    w = np.hanning(n); tot = np.zeros(n // 2 + 1)
    for i in range(0, len(x) - n + 1, n // 2): tot += np.abs(np.fft.rfft(x[i:i+n] * w)) ** 2
    f = np.fft.rfftfreq(n, 1.0 / rate)
    return float(tot[f >= cut].sum() / max(1e-30, tot[f >= 20].sum()))
