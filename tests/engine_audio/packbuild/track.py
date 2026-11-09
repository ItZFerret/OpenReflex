# RPM tracker for single-cylinder four-stroke recordings (numpy only).
# A four-stroke single fires once every two revolutions: the firing frequency f0 = rpm / 120 (12.5 Hz at 1,500 rpm, 100 Hz at 12,000)
# and every engine sound is a harmonic series of f0 (the crank order rpm / 60 is its 2nd harmonic).
#
#   1. decimate to RATE (6 kHz), frames every HOP s with a window per f0 band (BANDS: ~8 firing cycles, 0.6 s at idle .. 0.16 s at the
#      top), Hann window, magnitude spectrum, cube-root compressed and whitened
#      (divided by a smoothed spectral envelope, so the loud low harmonics do not outvote the rest);
#   2. salience(f) = (mean over k of M(k f) - mean over k of M((k + 1/2) f)) x sqrt(K), K harmonics up to min(FMAX, MAXK f): the true f0 has energy on every
#      harmonic and little in between. Twice f0 scores ~0 (its "in between" points are the odd harmonics of f0); half of f0 scores
#      ~half (half its harmonics fall in between). So the comb picks the firing frequency, not the crank order or a sub-octave;
#   3. Viterbi over the frames on a log-frequency grid: salience minus a cost per frame for log-frequency jumps (an engine does not
#      change rpm by an octave in 10 ms), which removes the remaining isolated octave errors;
#   4. a confidence per frame = the winning salience over the frame's spread (low in silence, starter cranking, wind).
import numpy as np

RATE = 6000
WIN, HOP = 0.30, 0.01
FMIN, FMAX_F0 = 9.0, 115.0      # f0 range: 1,080 .. 13,800 rpm
FMAX = 1500.0                   # harmonics used up to this frequency
NGRID = 600

def decimate(x, rate, to=RATE):
    """FFT resample (band-limited) of a 1-D signal."""
    n = len(x); m = int(round(n * to / rate))
    X = np.fft.rfft(x); keep = m // 2 + 1
    Y = np.zeros(keep, dtype=complex); k = min(keep, len(X)); Y[:k] = X[:k]
    return np.fft.irfft(Y, m) * (m / n)

def resample(x, rate, to):
    return decimate(x, rate, to)

# the f0 grid is split in bands, each with a window of ~8 firing cycles (resolution for idle, little smear on a fast rev)
BANDS = ((9.0, 16.0, 0.60), (16.0, 28.0, 0.42), (28.0, 50.0, 0.26), (50.0, 116.0, 0.16))
MAXK = 40                       # harmonics per candidate: up to min(FMAX, MAXK x f0)

def comb(grid, freqs):
    """[grid, bins] weights: (mean of the harmonics - mean of the half-way points) x sqrt(K) (a t-like score: candidates with few
    harmonics do not win on noise), linear interpolation between bins."""
    df = freqs[1]; W = np.zeros((len(grid), len(freqs)))
    for j, f in enumerate(grid):
        k = np.arange(1, int(min(FMAX, MAXK * f) / f) + 1); s = np.sqrt(len(k)) / len(k)
        for pos, sign in ((k * f, 1.0), ((k + 0.5) * f, -1.0)):
            b = pos / df; i0 = np.floor(b).astype(int); t = b - i0
            np.add.at(W[j], i0, sign * (1 - t) * s); np.add.at(W[j], i0 + 1, sign * t * s)
    return W

def spectra(y, rate=RATE, win=WIN, hop=HOP, t=None):
    """Whitened cube-root magnitude spectra of win-s frames centred on the times t (default: every hop)."""
    w = int(win * rate); h = int(hop * rate); nfft = 1 << int(np.ceil(np.log2(w * 4)))
    if t is None: t = (np.arange(0, max(1, len(y) - w), h) + w / 2) / rate
    starts = np.round(t * rate - w / 2).astype(int)
    yp = np.pad(y, (w, 2 * w)); idx = (starts + w)[:, None] + np.arange(w)[None, :]
    F = np.abs(np.fft.rfft(yp[idx] * np.hanning(w)[None, :], nfft, axis=1)) ** (1 / 3)
    freqs = np.fft.rfftfreq(nfft, 1.0 / rate)
    kw = max(1, int(60 / freqs[1])); ker = np.ones(kw) / kw
    env = np.apply_along_axis(lambda r: np.convolve(r, ker, mode='same'), 1, F) + 1e-9   # 60 Hz envelope
    return t, freqs, F / env

def salience(y, rate=RATE, hop=HOP):
    """(times, grid f0, salience [frames, grid])."""
    t = (np.arange(0, max(1, len(y) - int(0.3 * rate)), int(hop * rate)) + 0.15 * rate) / rate
    grid = np.exp(np.linspace(np.log(FMIN), np.log(FMAX_F0), NGRID))
    S = np.zeros((len(t), NGRID))
    for lo, hi, win in BANDS:
        sel = (grid >= lo) & (grid < hi)
        if not sel.any(): continue
        _, freqs, M = spectra(y, rate, win, hop, t)
        with np.errstate(all='ignore'): S[:, sel] = M @ comb(grid[sel], freqs).T
    return t, grid, S

def viterbi(S, grid, jump=0.03, cost=4.0):
    """Best path through the salience: per frame log-f changes cost `cost` x (|dlogf| / jump)^2."""
    n, g = S.shape; lg = np.log(grid)
    D = (lg[None, :] - lg[:, None]) / jump; T = cost * D * D   # [from, to]
    T[np.abs(D) > 8] = 1e9
    score = S[0].copy(); back = np.zeros((n, g), dtype=np.int32)
    for i in range(1, n):
        c = score[:, None] - T
        back[i] = np.argmax(c, axis=0); score = c[back[i], np.arange(g)] + S[i]
    path = np.zeros(n, dtype=np.int32); path[-1] = int(np.argmax(score))
    for i in range(n - 1, 0, -1): path[i - 1] = back[i, path[i]]
    return path

def refine(S, grid, path):
    """Parabolic interpolation of the path's peak on the log grid."""
    lg = np.log(grid); out = np.zeros(len(path))
    for i, j in enumerate(path):
        if 0 < j < len(grid) - 1:
            a, b, c = S[i, j - 1], S[i, j], S[i, j + 1]; d = a - 2 * b + c
            off = 0.5 * (a - c) / d if d < 0 else 0.0
            out[i] = np.exp(lg[j] + off * (lg[1] - lg[0]))
        else:
            out[i] = grid[j]
    return out

def track(x, rate):
    """(times s, f0 Hz, confidence) of a mono signal."""
    y = decimate(x, rate)
    t, grid, S = salience(y)
    path = viterbi(S, grid)
    f0 = refine(S, grid, path)
    sp = S.std(axis=1) + 1e-9
    conf = S[np.arange(len(path)), path] / sp
    return t, f0, conf

def rpm(f0): return np.asarray(f0) * 120.0
