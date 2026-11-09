# DSP helpers for the pack builder (numpy only): band-limited resampling, cubic interpolation, a K-weighting-like loudness,
# mic alignment, the seamless loop cutter.
import numpy as np

def fft_resample(x, n_out):
    """Band-limited resample of x to n_out samples, treating x as one period (exact for loops; fine for long takes)."""
    n = len(x); X = np.fft.rfft(x); k = n_out // 2 + 1
    Y = np.zeros(k, dtype=complex); m = min(k, len(X)); Y[:m] = X[:m]
    if n_out % 2 == 0 and m == k: Y[-1] = Y[-1].real
    return np.fft.irfft(Y, n_out) * (n_out / n)

def cubic_at(y, pos):
    """Catmull-Rom interpolation of y at fractional sample positions."""
    i = np.floor(pos).astype(np.int64); t = pos - i
    i = np.clip(i, 1, len(y) - 3)
    p0, p1, p2, p3 = y[i - 1], y[i], y[i + 1], y[i + 2]
    return p1 + 0.5 * t * (p2 - p0 + t * (2 * p0 - 5 * p1 + 4 * p2 - p3 + t * (3 * (p1 - p2) + p3 - p0)))

def kweight_mag(f):
    """|H(f)| of a K-weighting-like curve (ITU-R BS.1770 shape: high-pass ~38 Hz, +4 dB shelf above ~1.7 kHz)."""
    f = np.asarray(f, dtype=float)
    hp = (f / 38.0) ** 2 / np.sqrt(1 + (f / 38.0) ** 4)
    shelf = 10 ** ((4.0 * f ** 2 / (f ** 2 + 1700.0 ** 2)) / 20)
    return hp * shelf

def loudness_db(x, rate):
    """K-weighted RMS in dB (full scale sine = -3 dB)."""
    x = np.asarray(x, dtype=float)
    if len(x) == 0: return -120.0
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1.0 / rate)
    y = np.fft.irfft(X * kweight_mag(f), len(x))
    return 10 * np.log10(np.mean(y ** 2) + 1e-20)

def loudness_short(x, rate, win=0.4, hop=0.1):
    """Short-term K-weighted loudness (dB) per window."""
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1.0 / rate)
    y = np.fft.irfft(X * kweight_mag(f), len(x)); w = int(win * rate); h = int(hop * rate)
    c = np.concatenate(([0.0], np.cumsum(y ** 2)))
    st = np.arange(0, max(1, len(y) - w), h)
    return 10 * np.log10((c[st + w] - c[st]) / w + 1e-20)

def mic_lag(a, b, rate, win=1.0, step=1.5, maxlag=0.02):
    """Samples by which a (engine mic) lags b (exhaust mic): a whitened cross-correlation (100 Hz .. 4 kHz) per window, the
    median of the windows with a clear peak. Returns (lag, number of windows used, median peak/rms)."""
    n = int(win * rate); N = 1 << int(np.ceil(np.log2(2 * n))); L = int(maxlag * rate)
    fr = np.fft.rfftfreq(N, 1.0 / rate); band = (fr >= 100) & (fr <= 4000)
    lags, q = [], []
    for s in range(0, len(a) - n, int(step * rate)):
        A = np.fft.rfft(a[s:s + n], N); B = np.fft.rfft(b[s:s + n], N)
        C = A * np.conj(B); C = C / (np.abs(C) ** 0.5 + 1e-12); C[~band] = 0
        cc = np.fft.irfft(C, N); cc = np.concatenate((cc[-L:], cc[:L + 1]))
        i = int(np.argmax(cc)); pr = cc[i] / (cc.std() + 1e-12)
        if pr > 3.0: lags.append(i - L); q.append(pr)
    if not lags: return 0, 0, 0.0
    return int(np.median(lags)), len(lags), float(np.median(q))

def shift(x, lag):
    """x advanced by lag samples (x[t + lag]), zero padded."""
    if lag == 0: return x.copy()
    y = np.zeros_like(x)
    if lag > 0: y[:-lag] = x[lag:]
    else: y[-lag:] = x[:lag]
    return y

def soft_clip(x, t):
    """Memoryless soft knee above t (so a loop stays seamless): |y| < 1 always, y = x below t."""
    a = np.abs(x); y = x.copy(); o = a > t
    y[o] = np.sign(x[o]) * (t + (1 - t) * np.tanh((a[o] - t) / (1 - t)))
    return y

def loop_limiter(x, rate, crest_db, look=0.001, release=0.02, passes=10, ceiling=None):
    """A look-ahead peak limiter run on the loop as a periodic signal (three copies, the middle one kept: the gain is periodic too,
    so the loop stays seamless): peaks are held to crest_db over the loop's rms (or to an absolute ceiling). Attack = the look-ahead (a raised-cosine ramp into
    each peak), release = a one-pole return. Gentler on the sound than a waveshaper (no added harmonics on the firing pulses)."""
    y = x.copy()
    for _ in range(passes):
        rms = np.sqrt(np.mean(y ** 2)); thr = rms * 10 ** (crest_db / 20) if ceiling is None else ceiling   # ceiling: an absolute threshold
        if np.max(np.abs(y)) <= thr * 1.03: break                                        # within 0.25 dB
        n = len(y); xx = np.tile(y, 3)
        need = np.minimum(1.0, thr / (np.abs(xx) + 1e-12))
        La = max(1, int(look * rate))
        g = need.copy()
        for k in range(1, 2 * La + 1): g[:-k] = np.minimum(g[:-k], need[k:])          # min over the next 2 La samples
        a = np.exp(-1.0 / (release * rate)); out = np.empty_like(g); prev = 1.0
        for i in range(len(g)):                                                         # release: back up slowly
            prev = min(g[i], 1.0 - (1.0 - prev) * a)
            out[i] = prev
        w = 0.5 - 0.5 * np.cos(2 * np.pi * (np.arange(La) + 0.5) / La); w /= w.sum()   # attack: smooth the ramp down
        out = np.convolve(out, w)[:len(out)]                                            # causal: only samples inside the look-ahead
        y = (xx * out)[n:2 * n]
    return y

def low_shelf(x, rate, gain_db, fc):
    """Zero-phase low shelf (FFT, periodic: a loop stays seamless): gain_db at DC, half of it (in dB) at fc, 0 dB well above."""
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1.0 / rate)
    return np.fft.irfft(X * 10 ** (gain_db / 20 / (1 + (f / fc) ** 2)), len(x))

def presence(x, rate, gain_db, fc, octaves):
    """Zero-phase broad bell (FFT, periodic): gain_db at fc, a Gaussian over log2 frequency with this many octaves' standard deviation."""
    if not gain_db: return x
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1.0 / rate)
    g = gain_db * np.exp(-0.5 * (np.log2(np.maximum(f, 1.0) / fc) / octaves) ** 2)
    return np.fft.irfft(X * 10 ** (g / 20), len(x))
