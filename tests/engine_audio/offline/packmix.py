# The pack mixer of the game since w-packpeak, offline (C#: EnginePackMixer / EnginePackLimiter in EnginePackPlayer.cs; keep in step):
# every voice is resampled with 4-point Hermite interpolation into one mono sum, and a lookahead peak limiter holds that sum (Engine channel
# included) under Ceiling: g = min(1, ceiling / |x|), its minimum over the next Lookahead samples, an exponential release, then a box average
# over the lookahead window (so the gain is already down when the peak arrives: never over the ceiling), the signal delayed by the lookahead.
import math, numpy as np
from numpy.lib.stride_tricks import sliding_window_view
Ceiling = 0.891            # -1 dBFS
LookaheadSeconds = 0.003   # 3 ms (144 samples at 48 kHz)
ReleaseSeconds = 0.10

def hermite(d, idx, loop):
    n = len(d); i = np.floor(idx).astype(np.int64); t = idx - i
    if loop: xm1, x0, x1, x2 = d[(i - 1) % n], d[i % n], d[(i + 1) % n], d[(i + 2) % n]
    else:
        c = lambda k: d[np.clip(k, 0, n - 1)]
        xm1, x0, x1, x2 = c(i - 1), c(i), c(i + 1), c(i + 2)
    c0 = x0; c1 = 0.5 * (x1 - xm1); c2 = xm1 - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
    return ((c3 * t + c2) * t + c1) * t + c0

def limit(x, rate=48000, ceiling=None, lookahead=None, release=None):
    """-> (y, gain): y[n] = x[n - L] x gain[n]; |y| <= ceiling."""
    ceiling = Ceiling if ceiling is None else ceiling; L = int(round((LookaheadSeconds if lookahead is None else lookahead) * rate))
    a = 1.0 - math.exp(-1.0 / ((ReleaseSeconds if release is None else release) * rate))
    ax = np.abs(x); g = np.where(ax > ceiling, ceiling / np.maximum(ax, 1e-30), 1.0)
    m = sliding_window_view(np.concatenate([np.ones(L), g]), L + 1).min(axis=1)
    r = np.empty_like(m); cur = 1.0
    below = np.flatnonzero(m < 1.0)
    # the release recursion, run only where the gain is (or is still recovering) below 1: elsewhere r = 1
    k = 0; n = len(m); nxt = below[0] if len(below) else n; bi = 0
    while k < n:
        if cur >= 1.0 - 1e-9 and m[k] >= 1.0:
            # jump to the next sample that needs the gain down
            while bi < len(below) and below[bi] < k: bi += 1
            j = below[bi] if bi < len(below) else n
            r[k:j] = 1.0; cur = 1.0; k = j; continue
        v = m[k]; cur = v if v < cur else cur + (v - cur) * a; r[k] = cur; k += 1
    cs = np.concatenate([[0.0], np.cumsum(np.concatenate([np.ones(L), r]))])
    e = (cs[L + 1:] - cs[:-(L + 1)]) / (L + 1)
    xd = np.concatenate([np.zeros(L), x])[:len(x)]
    return xd * e, e
