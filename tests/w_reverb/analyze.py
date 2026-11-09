#!/usr/bin/env python3
"""analyze.py <folder with reverb-*.wav> -- the room impulse response numbers of the reverb probe's recordings (tests/w_reverb).

Per file: the burst's arrival, the energy of the first 10 ms (direct) against everything after 30 ms (the reverb), the Schroeder decay
(RT60 from the -5 .. -25 dB part of the backward-integrated energy, x3), and the tail's spectral centroid. numpy only."""
import sys, wave
from pathlib import Path
import numpy as np


def load(p):
    with wave.open(str(p), "rb") as w:
        n, ch, rate = w.getnframes(), w.getnchannels(), w.getframerate()
        a = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768.0
    return a.reshape(-1, ch).mean(axis=1), rate


def analyse(p):
    x, rate = load(p)
    if x.size == 0 or np.max(np.abs(x)) < 1e-6:
        return {"file": p.name, "silent": True}
    i0 = int(np.argmax(np.abs(x) > 0.05 * np.max(np.abs(x))))
    d = x[i0:]
    direct = float(np.sum(d[: int(0.010 * rate)] ** 2)) + 1e-18
    tail = d[int(0.030 * rate):]
    e_tail = float(np.sum(tail ** 2)) + 1e-18
    edc = np.cumsum((d ** 2)[::-1])[::-1] + 1e-18
    db = 10 * np.log10(edc / edc[0])
    def at(level):
        k = np.argmax(db <= level)
        return k / rate if db[k] <= level else float("nan")
    t5, t25 = at(-5.0), at(-25.0)
    rt60 = 3 * (t25 - t5) if t25 == t25 and t5 == t5 else float("nan")
    spec = np.abs(np.fft.rfft(tail[: min(tail.size, 2 ** 18)] * np.hanning(min(tail.size, 2 ** 18)))) ** 2
    f = np.fft.rfftfreq(min(tail.size, 2 ** 18), 1 / rate)
    centroid = float(np.sum(f * spec) / np.sum(spec)) if np.sum(spec) > 0 else float("nan")
    return {"file": p.name, "arrival_s": i0 / rate, "peak": float(np.max(np.abs(x))), "reverb_to_direct_dB": 10 * np.log10(e_tail / direct),
            "rt60_s": rt60, "decay_to_-40dB_s": at(-40.0), "tail_centroid_Hz": centroid, "seconds": x.size / rate}


if __name__ == "__main__":
    folder = Path(sys.argv[1])
    for p in sorted(folder.glob("reverb-*.wav")):
        r = analyse(p)
        print(" ".join(f"{k}={v:.3f}" if isinstance(v, float) else f"{k}={v}" for k, v in r.items()))
