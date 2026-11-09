# The source takes: the two onboard mics aligned and blended into one channel, and the take's rpm track (cached).
import os, hashlib
import numpy as np
import wavio, track, dsp, loops

PREFIX = 'Dirt Bike 450cc 4 Stroke '
EXHAUST_W, ENGINE_W = 0.75, 0.66        # amplitude weights of the level-matched mics: ~56 % exhaust / 44 % engine power (a chase /
                                        # onboard view: the exhaust note in front, the engine's mechanical top end under it; the engine
                                        # mic also carries the mid / upper range the A-weighted level is made of)

def paths(src, take):
    base = os.path.join(src, PREFIX + take + ' Onboard ')
    if not os.path.exists(base + 'Engine.wav'):     # "... Onboard 01 Engine.wav" style
        raise FileNotFoundError(base + 'Engine.wav')
    return base + 'Engine.wav', base + 'Exhaust.wav'

def loud_rms(x, rate):
    """RMS over the louder half of 100 ms blocks (the take's running level, not its silences)."""
    b = int(0.1 * rate); n = len(x) // b
    r = np.sqrt((x[:n * b].reshape(n, b) ** 2).mean(1))
    return float(np.sqrt(np.mean(np.sort(r)[n // 2:] ** 2)))

class Take:
    def __init__(self, src, take, cache, exhaust_w=EXHAUST_W, engine_w=ENGINE_W, drive=None, maxlag=0.02):
        """drive: the exhaust mic's saturation threshold in units of its running rms (None: clean): x -> drive * tanh(x / drive), a
        symmetric soft clip of the loudest exhaust pulses only (odd harmonics: rasp). maxlag: the mic lag search range (s)."""
        self.name = take
        eng_p, exh_p = paths(src, take)
        if (exhaust_w, engine_w, drive, maxlag) == (EXHAUST_W, ENGINE_W, None, 0.02):
            key = hashlib.sha1(f'{take}|{EXHAUST_W}|{ENGINE_W}|v3'.encode()).hexdigest()[:12]      # round 1's cache key
        else:
            key = hashlib.sha1(f'{take}|{exhaust_w}|{engine_w}|{drive}|{maxlag}|v4'.encode()).hexdigest()[:12]
        cf = os.path.join(cache, f'take_{key}.npz') if cache else None
        if cf and os.path.exists(cf):
            d = np.load(cf)
            self.y, self.rate, self.lag = d['y'], int(d['rate']), int(d['lag'])
            self.t, self.f0, self.conf, self.f0s = d['t'], d['f0'], d['conf'], d['f0s']
            self.lag_n, self.lag_q, self.agree = int(d['lag_n']), float(d['lag_q']), float(d['agree'])
            return
        e, rate = wavio.read(eng_p); x, rate2 = wavio.read(exh_p)
        assert rate == rate2
        n = min(len(e), len(x)); eng = e[:n].mean(1); exh = x[:n].mean(1)
        self.lag, self.lag_n, self.lag_q = dsp.mic_lag(eng, exh, rate, maxlag=maxlag)
        eng = dsp.shift(eng, self.lag)              # the engine mic hears the exhaust ~lag samples later: advance it
        xn = exh / loud_rms(exh, rate)
        if drive: xn = drive * np.tanh(xn / drive)
        self.y = exhaust_w * xn / loud_rms(xn, rate) + engine_w * eng / loud_rms(eng, rate)
        self.y /= np.max(np.abs(self.y)) + 1e-12
        self.rate = rate
        self.t, self.f0, self.conf = track.track(self.y, rate)
        # cross-check: the exhaust mic alone
        te, f0e, _ = track.track(exh, rate)
        m = min(len(self.f0), len(f0e)); self.agree = float(np.mean(np.abs(np.log(self.f0[:m] / f0e[:m])) < 0.03))
        self.f0s = loops.smooth_track(self.t, self.f0, self.conf)
        if cf:
            os.makedirs(cache, exist_ok=True)
            np.savez(cf, y=self.y, rate=rate, lag=self.lag, t=self.t, f0=self.f0, conf=self.conf, f0s=self.f0s,
                     lag_n=self.lag_n, lag_q=self.lag_q, agree=self.agree)

    def rpm_at(self, s): return float(np.interp(s, self.t, self.f0s) * 120)
    def slope_at(self, s, d=0.15): return (self.rpm_at(s + d) - self.rpm_at(s - d)) / (2 * d)

    def crossings(self, rpm, a, b, direction):
        """Times in [a, b] where the smoothed rpm passes rpm going up (+1) or down (-1)."""
        r = self.f0s * 120; t = self.t; out = []
        for i in np.nonzero((t[:-1] >= a) & (t[1:] <= b))[0]:
            if direction > 0 and r[i] < rpm <= r[i + 1] or direction < 0 and r[i] > rpm >= r[i + 1]:
                out.append(float(t[i] + (rpm - r[i]) / (r[i + 1] - r[i]) * (t[i + 1] - t[i])))
        return out

    def smoothness(self, s, half):
        """Rms deviation (%) of the raw track from a straight line over [s - half, s + half]: wobble, tracker trouble."""
        sel = (self.t >= s - half) & (self.t <= s + half)
        if sel.sum() < 5: return 99.0
        tt, ff = self.t[sel], self.f0[sel]
        c = np.polyfit(tt, ff, 1)
        return float(np.sqrt(np.mean((ff / np.polyval(c, tt) - 1) ** 2)) * 100)
