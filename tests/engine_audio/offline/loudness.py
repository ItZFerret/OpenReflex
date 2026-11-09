# The load-time loudness estimate of a pack (w-packloud), the C# EnginePackLoudness mirrored: the A-weighted mean square of every sample at
# every pitch (from its power spectrum), summed over the reference ride's (rpm fraction, throttle) occupancy with the pack's own volume / pitch
# curves and layer blend.  gain = TargetALevel / sqrt(that).
import math, numpy as np
import render as R, packs as P
# The reference ride (EnginePackPlayer.RefRide, the same numbers): the share of race frames of the 11 computer riders of the NAT_Concord
# default event (landclank's trace_swap, 60 s of racing each) in each (Reflex rpm fraction bin i = i / 25, throttle bin j = j / 4).
REF_RIDE = np.array([
    [0.0287, 0.0041, 0.0010, 0.0002, 0.0056],
    [0.0020, 0.0002, 0.0002, 0.0001, 0.0015],
    [0.0022, 0.0006, 0.0001, 0.0001, 0.0013],
    [0.0026, 0.0006, 0.0002, 0.0000, 0.0011],
    [0.0029, 0.0006, 0.0002, 0.0000, 0.0010],
    [0.0036, 0.0007, 0.0001, 0.0001, 0.0013],
    [0.0044, 0.0006, 0.0001, 0.0000, 0.0011],
    [0.0054, 0.0006, 0.0001, 0.0001, 0.0010],
    [0.0096, 0.0007, 0.0002, 0.0001, 0.0015],
    [0.0111, 0.0008, 0.0002, 0.0000, 0.0012],
    [0.0093, 0.0106, 0.0003, 0.0001, 0.0015],
    [0.0133, 0.0107, 0.0006, 0.0002, 0.0019],
    [0.0139, 0.0159, 0.0040, 0.0008, 0.0042],
    [0.0136, 0.0101, 0.0243, 0.0070, 0.0096],
    [0.0130, 0.0062, 0.0271, 0.0185, 0.0285],
    [0.0073, 0.0024, 0.0268, 0.0189, 0.0466],
    [0.0044, 0.0023, 0.0132, 0.0177, 0.0557],
    [0.0039, 0.0033, 0.0066, 0.0086, 0.0520],
    [0.0031, 0.0036, 0.0040, 0.0054, 0.0526],
    [0.0025, 0.0038, 0.0031, 0.0029, 0.0445],
    [0.0021, 0.0028, 0.0046, 0.0029, 0.0347],
    [0.0014, 0.0024, 0.0026, 0.0029, 0.0272],
    [0.0014, 0.0022, 0.0009, 0.0027, 0.0294],
    [0.0010, 0.0019, 0.0006, 0.0006, 0.0412],
    [0.0003, 0.0012, 0.0003, 0.0005, 0.0674],
    [0.0004, 0.0001, 0.0003, 0.0005, 0.0221]])
# The original Reflex engine's A-weighted rms on that ride: the local engine alone in landing_mix.py (rider 2 as the local rider, race 25 .. 55 s)
# is 0.1057 with the Engine channel 0.7 and the centred equal-power pan 0.7071: 0.1057 / 0.7 / 0.7071 = 0.2136 before the channel and the pan.
TARGET_A = 0.2136
# ... per engine size (EnginePackPlayer.TargetALevelFor): the original emitter on the same ride with the engine size changed (rpm fraction kept),
# race 25 .. 55 s: 2 = MX 250 two-stroke 0.1772, 3 = MX 250F 0.1389, 4 = MX 450F 0.2136 (other sizes: the 450F's)
def target_for(size): return 0.1772 if size == 2 else 0.1389 if size == 3 else TARGET_A
NFFT = 4096; MAXSECONDS = 10.0
def a_resp(f):
    f2 = f * f
    ra = (12194.0 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194.0 ** 2))
    return ra / 0.7943
def spectrum(x, rate):
    x = np.asarray(x[:int(MAXSECONDS * rate)], dtype=np.float64)
    if len(x) < NFFT: x = np.pad(x, (0, NFFT - len(x)))
    w = np.hanning(NFFT); P = np.zeros(NFFT // 2 + 1); n = 0
    for i in range(0, len(x) - NFFT + 1, NFFT // 2): P += np.abs(np.fft.rfft(x[i:i + NFFT] * w)) ** 2; n += 1
    return P, np.fft.rfftfreq(NFFT, 1.0 / rate), float(np.mean(x ** 2))
def a_ms(spec, pitch):
    P, f, ms = spec; tot = P.sum()
    return 0.0 if tot <= 0 else ms * float((P * a_resp(f * pitch) ** 2).sum() / tot)
def pack_level(p, specs, ride=REF_RIDE):
    """the pack's A-weighted rms over the ride at gain 1 (unit Engine channel, before the 3D pan)."""
    NF, NT = ride.shape; idleP = P.idle_norm(p) * p.maxValue; maxP = max(idleP + 1, p.maxRpm); acc = 0.0
    for i in range(NF):
        frac = i / (NF - 1); n = R.clamp01((idleP + frac * (maxP - idleP)) / p.maxValue)
        for j in range(NT):
            w = ride[i, j]
            if w <= 0: continue
            on = max(j / (NT - 1), p.idleThrottle) ** max(0.01, p.blendExp)
            for l in range(2):
                for s in p.layers[l]:
                    v = (on if l == 0 else 1 - on) * R.vol_at(s, n)
                    if v <= 0: continue
                    pt = 1.0 if (not p.autoPitch or s.ref <= 0) else min(max(n / s.ref, s.minp), s.maxp)
                    acc += w * v * v * a_ms(specs[s.wav], min(max(pt, 0.05), 3.0))
    return math.sqrt(acc)

def pack_gain(p, cache=None, size=4):
    """(gain, level at gain 1): the gain that puts the pack at target_for(size) on the reference ride."""
    specs = {}
    for l in p.layers:
        for s in l:
            if s.wav not in specs: specs[s.wav] = (cache or {}).get(s.wav) or spectrum(*R.read_wav(s.wav))
    lv = pack_level(p, specs)
    return (min(4.0, max(0.05, target_for(size) / lv)) if lv > 0 else 0.30 * (p.iniGain if getattr(p, 'iniGain', None) is not None else 1.0)), lv
