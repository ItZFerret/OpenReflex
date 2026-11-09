# Seamless engine loops from a tracked recording (numpy only).
#
#   phase      the firing phase phi(t) = integral of the tracked f0 (cycles), per sample of the source;
#   refine     the segment is unwrapped into one row per firing cycle (P points per cycle, low-passed copy), each row is
#              cross-correlated with the mean cycle, and a smooth (quadratic) correction of phi over the cycle index is fitted to the
#              lags and applied; three passes. This fixes the tracker's small f0 bias (which would drift the phase over 50 cycles)
#              but keeps the natural cycle-to-cycle jitter of the combustion;
#   warp       N whole cycles are read out at a constant phase rate (cubic interpolation of the 96 kHz source): a ramp through the
#              target rpm becomes a constant-rpm loop at the segment's mean rpm (the pitch moves at most a few percent), exactly
#              N cycles long;
#   seam       the loop is read X samples past its end; those samples (the same firing phase, N cycles later) are crossfaded into
#              the loop's first X samples, so the last sample runs into the first one without a step;
#   rate       the 96 kHz loop is resampled to 48 kHz as one period (FFT), which keeps it exactly periodic.
import numpy as np
import dsp

P = 256           # points per cycle for the alignment rows

def phase(f0_t, f0, rate, a, b):
    """Sample times [a, b) and the integrated phase (cycles) of the smoothed track over them."""
    ts = np.arange(a, b) / rate
    f = np.interp(ts, f0_t, f0)
    ph = np.concatenate(([0.0], np.cumsum(0.5 * (f[1:] + f[:-1])) / rate))
    return ts, ph

def smooth_track(t, f0, conf, med=7, avg=9):
    """Median then moving-average smoothing of a track (frames)."""
    n = len(f0); h = med // 2
    pad = np.pad(f0, h, mode='edge')
    m = np.array([np.median(pad[i:i + med]) for i in range(n)])
    k = np.ones(avg) / avg
    return np.convolve(np.pad(m, avg // 2, mode='edge'), k, mode='valid')

def rows(ylp, ph, k0, k1):
    """[cycles, P]: the low-passed signal read at phases k0 .. k1 (P per cycle); ph is the phase per sample of ylp."""
    grid = k0 + np.arange((k1 - k0) * P) / P
    pos = np.interp(grid, ph, np.arange(len(ph), dtype=float))
    return dsp.cubic_at(ylp, pos).reshape(k1 - k0, P)

def refine(ylp, ph, k0, k1, passes=3, maxlag=P // 6):
    """Phase correction (see the header). Returns the new phase and (mean correlation, residual lag std in cycles) per pass."""
    stats = []
    for _ in range(passes):
        R = rows(ylp, ph, k0, k1)
        T = R.mean(axis=0)
        F = np.fft.rfft(R, axis=1) * np.conj(np.fft.rfft(T))[None, :]
        cc = np.fft.irfft(F, P, axis=1)
        cc = np.concatenate((cc[:, -maxlag:], cc[:, :maxlag + 1]), axis=1)
        lag = (np.argmax(cc, axis=1) - maxlag).astype(float)
        nr = np.sqrt((R * R).sum(1) * (T * T).sum()) + 1e-12
        corr = cc.max(axis=1) / nr
        # sub-point lag (parabola)
        i = np.argmax(cc, axis=1); j = np.clip(i, 1, cc.shape[1] - 2); r = np.arange(len(i))
        a, b, c = cc[r, j - 1], cc[r, j], cc[r, j + 1]; d = a - 2 * b + c
        lag = lag + np.where(d < 0, 0.5 * (a - c) / np.where(d < 0, d, -1), 0)
        lag /= P
        k = np.arange(k0, k1) + 0.5
        w = np.clip(corr, 0, 1) ** 2 + 1e-6
        cf = np.polyfit(k, lag, 2, w=np.sqrt(w))
        resid = lag - np.polyval(cf, k)
        stats.append((float(np.mean(corr)), float(np.sqrt(np.average(resid ** 2, weights=w)))))
        delta = np.polyval(cf, ph)
        delta -= np.polyval(cf, 0.5 * (k0 + k1))
        ph = ph - delta
    return ph, stats

def lowpass(x, rate, fc=2000.0):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1.0 / rate)
    X[f > fc] *= np.exp(-((f[f > fc] - fc) / 300.0) ** 2)
    return np.fft.irfft(X, len(x))

def cut(y, rate, trk, center, seconds, out_rate=48000, xfade_cycles=2.0, min_xfade=0.010, multiple=1):
    """One loop around time `center` (s) of the 96 kHz mono source y, about `seconds` long, from the track trk = (t, f0 smoothed).
    Returns (loop at out_rate, rpm of the loop, info dict)."""
    t, f0s = trk
    fc = float(np.interp(center, t, f0s))
    N = max(2, int(round(seconds * fc / multiple)) * multiple)   # multiple: e.g. a limiter's cut pattern (every 3rd firing)
    span = N / fc
    a = int(max(0, (center - span / 2 - 0.25) * rate)); b = int(min(len(y), (center + span / 2 + 0.25 + 4 / fc) * rate))
    ts, ph = phase(t, f0s, rate, a, b)
    seg = y[a:b]
    ylp = lowpass(seg, rate)
    pc = float(np.interp(center, ts, ph))
    k0 = int(np.floor(pc - N / 2)); k1 = k0 + N
    ph, rstats = refine(ylp, ph, k0, k1)
    # the seam joins cycle k0 + N (read past the end, crossfaded in) to cycle k0: start where those two cycles (and the next pair)
    # are most alike, within a few cycles of the centre
    pad_c = int(0.2 * fc)
    best = None
    for d in range(-min(4, pad_c), min(4, pad_c) + 1):
        if k0 + d < ph[0] + 1 or k0 + d + N + 3 > ph[-1] - 1: continue
        Rr = rows(seg, ph, k0 + d, k0 + d + 2); Rn = rows(seg, ph, k0 + d + N, k0 + d + N + 2)
        c = float(np.sum(Rr * Rn) / (np.sqrt(np.sum(Rr * Rr) * np.sum(Rn * Rn)) + 1e-12))
        if best is None or c > best[0]: best = (c, d)
    if best is not None: k0 += best[1]; k1 = k0 + N
    # start on the quietest point of the mean cycle (the seam crossfade sits between firing pulses)
    T = rows(seg, ph, k0, k1).mean(axis=0)
    e = np.convolve(T ** 2, np.ones(P // 16) / (P // 16), mode='same')
    phi_a = k0 + np.argmin(e) / P
    # the loop: N cycles at a constant phase rate
    f_mean = N / ((np.interp(phi_a + N, ph, ts)) - np.interp(phi_a, ph, ts))
    L_out = int(round(N * out_rate / f_mean)); L = L_out * (rate // out_rate) if rate % out_rate == 0 else int(round(L_out * rate / out_rate))
    X = int(max(min_xfade * rate, xfade_cycles * rate / f_mean))
    grid = phi_a + np.arange(L + X) * (N / L)
    if grid[-1] > ph[-1]: raise ValueError('segment too short for the loop')
    pos = np.interp(grid, ph, np.arange(len(ph), dtype=float))
    s = dsp.cubic_at(seg, pos)
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(X) / X)       # 0 -> 1
    loop = s[:L].copy()
    loop[:X] = s[:X] * w + s[L:L + X] * (1 - w)
    out = dsp.fft_resample(loop, L_out)
    rpm = 120.0 * N * out_rate / L_out
    # the warp: how far the source's instantaneous rpm strays from the loop's (percent)
    ti = np.interp(grid[:L], ph, ts); fi = np.interp(ti, t, f0s)
    warp = float(np.max(np.abs(fi / f_mean - 1)) * 100)
    info = dict(N=N, start=float(np.interp(phi_a, ph, ts)), seconds=L_out / out_rate, rpm=rpm, warp_pct=warp,
                corr=rstats[-1][0], resid=rstats[-1][1], xfade_ms=X / rate * 1000)
    return out, rpm, info

def seam_check(x, rpm, rate=48000):
    """The discontinuity at the loop point against the rest of the loop: first and second differences across the wrap (x[-1] -> x[0])
    as a percentile of the same differences inside the loop, and the correlation of the cycle before the seam with the cycle after it
    against the median of neighbouring cycles inside the loop."""
    d1 = np.diff(np.concatenate((x, x[:1]))); seam1 = abs(d1[-1])
    xx = np.concatenate((x[-1:], x, x[:1])); d2 = xx[2:] - 2 * xx[1:-1] + xx[:-2]
    seam2 = max(abs(d2[0]), abs(d2[-1]))
    pct1 = float((np.abs(d1[:-1]) < seam1).mean() * 100); pct2 = float((np.abs(d2[1:-1]) < seam2).mean() * 100)
    cyc = rate * 120.0 / rpm; c = int(round(cyc))
    def corr(a, b): return float(np.dot(a, b) / (np.sqrt(np.dot(a, a) * np.dot(b, b)) + 1e-12))
    wrapped = np.concatenate((x[-c:], x[:c]))
    seamc = corr(wrapped[:c], wrapped[c:])
    inner = [corr(x[i:i + c], x[i + c:i + 2 * c]) for i in range(0, len(x) - 2 * c, c)]
    return dict(jump=float(seam1), jump_pct=pct1, curv_pct=pct2, rms_d1=float(np.sqrt(np.mean(d1[:-1] ** 2))),
                seam_corr=seamc, inner_corr=float(np.median(inner)) if inner else float('nan'))

def cycle_stats(x, rpm, rate=48000):
    """Per-cycle view of a loop: the correlation of each cycle with the next (the last pair runs across the seam) and the cycle rms.
    Returns (seam corr, its rank among the inner pairs in %, median inner corr, min inner corr, min / max cycle rms over the median)."""
    c = rate * 120.0 / rpm; n = int(round(len(x) / c)); L = int(c)
    R = np.array([np.take(x, np.arange(int(round(i * c)), int(round(i * c)) + L), mode='wrap') for i in range(n)])
    Rz = R - R.mean(1, keepdims=True); nr = np.sqrt((Rz * Rz).sum(1)) + 1e-12
    cc = (Rz * np.roll(Rz, -1, axis=0)).sum(1) / (nr * np.roll(nr, -1))
    e = np.sqrt((R ** 2).mean(1)); e /= np.median(e)
    inner = cc[:-1]
    return dict(seam_corr=float(cc[-1]), seam_rank=float((inner < cc[-1]).mean() * 100), inner_median=float(np.median(inner)),
                inner_min=float(inner.min()), rms_min=float(e.min()), rms_max=float(e.max()))
