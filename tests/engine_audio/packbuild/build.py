# Builds the "Onboard 450 (Honda)" engine sound pack (MX Bikes format: engine.scl + sfx.cfg + pack.ini + 48 kHz 16-bit mono WAVs)
# from the owner's "Dirt Bike 450cc 4 Stroke Onboard" recordings. numpy only (system python3).
#
#   python3 build.py --src "<recordings folder>" --out "<pack folder>" [--variant base|A|B|C] [--cache <scratch dir>] [--gain G] [--report report.json]
#
# Variants (VARIANTS below): base = round 1 "Onboard 450 (Honda)"; A / B / C = the round 2 "Aggressive" packs (the owner: "needs to sound
# more aggressive"): A a mild step (Honda), B a clear step (Honda), C raw race exhaust (Yamaha). Each overrides the constants below.
#
# The recordings are read only. Method: the headers of track.py (rpm), takes.py (mic blend), loops.py (loops), dsp.py; core/plan/QUEUE.md (w-pack450).
# --cache keeps the blended / tracked takes (npz, ~50 MB each) between runs: point it at a scratch folder, never the repo.
import argparse, json, os, sys
import numpy as np
import wavio, dsp, loops, takes

PACK_NAME = 'Onboard 450 (Honda)'
FILE_PREFIX = 'onboard450'
SOURCE_NOTE = 'Honda: Ramp 03 / Ramp 02 / Start, Idle, Rev Ups, Stop 01'
DESCRIPTION = ''                # pack.ini "variant" line
OUT_RATE = 48000
TAKE_ARGS = {}                  # takes.Take keywords: exhaust_w, engine_w, drive, maxlag

# --------------------------------------------------------------------------------------------------- the sources (Honda CRF450)
# Ramp 03: a slow pull in gear 3,600 -> 11,100 rpm (5 .. 14.6 s, ~900 rpm/s) and a long closed-throttle overrun 11,100 -> 3,800
# (15.8 .. 26.2 s, ~700 rpm/s). Ramp 02: the same pull, then 1.4 s on the limiter at ~11,140 rpm (17.4 .. 18.8 s: every third firing
# cut). Start, Idle, Rev Ups, Stop 01: a steady warm idle at ~2,290 rpm (3 .. 25 s).
RAMP3, RAMP2, IDLE = 'Honda Ramp 03', 'Honda Ramp 02', 'Honda Start, Idle, Rev Ups, Stop 01'
# where each layer may cut its loops: (take, from s, to s, direction of the rpm)
ON_WINDOWS = [(RAMP3, 4.9, 14.7, +1), (RAMP2, 3.6, 17.15, +1)]   # Ramp 02 hits the limiter at 17.3 s
OFF_WINDOWS = [(RAMP3, 15.9, 29.0, -1), (RAMP2, 19.3, 26.6, -1)]
PREFER = RAMP3                  # one take for the body of the pack: the same session, mic placement and gear (a coherent sound)
ON_RPM = [3250, 3800, 4400, 5100, 5800, 6500, 7200, 7900, 8600, 9300, 10000, 10450, 11050]
OFF_RPM = [3100, 3900, 4700, 5500, 6300, 7100, 7900, 8700, 9500]
IDLE_AT = (IDLE, 9.0, 2.0)      # (take, centre s, loop seconds): the idle loop (off layer sample 0 and on layer sample 0)
LIMITER_AT = (RAMP2, 18.15, 0.45, 3)   # + the cut pattern: every third firing is cut, so whole 3-cycle groups
MAX_WARP = 8.0                  # % the rpm may move inside one loop before it is straightened (loop length is cut to keep under it)
MIN_LOOP, MAX_LOOP = 0.45, 1.2  # s
OFF_MAX_WARP, OFF_MIN_LOOP = 8.0, 0.45   # the off layer's (round 2: longer overrun loops, so their pops repeat less often)
OFF_POPS = False                # round 2: off loops are scored for no single dominating pop (cycle rms <= POP_MAX x the median)
POP_MAX = 1.8                   # instead of for the cleanest cycles: the overrun keeps its crackle
NUDGE = (0.0, -0.06, 0.06, -0.12, 0.12, -0.18, 0.18)   # s: loop centres tried around each pick

# --------------------------------------------------------------------------------------------------- levels
RISE_MIN_DB, RISE_MAX_DB = 4.0, 5.0   # the on layer's loudness rise from idle to the limiter (the recording's own, clamped)
OFF_MIN_DB, OFF_MAX_DB = 3.0, 8.0     # off throttle under on throttle at the same rpm (the recording's own, clamped)
IDLE_UNDER_OFF_DB = 1.0               # the idle a little under the off layer's line
PEAK = 0.95                           # the loudest WAV's peak
CREST_DB = 7.0                        # each loop's peaks held to this over its rms (periodic look-ahead limiter): the natural firing
                                      # pulses run 9 .. 15 dB; at 8 dB the pack reaches the Steeler packs' loudness with headroom
SOFT_KNEE = 0.92                      # a safety knee only
XFADE = 0.4                           # the share of each rpm gap that crossfades (round 1: 0.4; round 2: narrower, so one voice plays
                                      # most of the time: the game's peak control sums the voices' sample peaks)
TRIM = 1.0                            # pack.ini trim (w-packloud: x the game's load-time loudness gain); round 2 sets it so each variant
                                      # plays at round 1's in-game level despite the peak control taking more off a punchier pack
CREST_TOP = None                      # round 2: sets the global scale (the top on-throttle loop's rms CREST_TOP dB under PEAK) and every file
                                      # is then held under the ceiling PEAK; None: round 1's scaling. (At high rpm the limiter's 20 ms
                                      # release spans several firings, so it lowers the rms with the peaks: the top loops keep ~CREST_DB
                                      # crest, ~9 dB, whatever CREST_TOP says; the quieter low / mid loops keep up to CREST_DB.)
SHELF_DB, SHELF_HZ = -6.0, 140.0      # the onboard mics are bass-heavy: the firing fundamental and its first harmonics carry the peaks but
                                      # little of the heard (A-weighted) loudness; a low shelf buys the headroom to sit at the original
                                      # engine's A-weighted level without clipping (w-landclank: the bass-heavy Steeler packs sound 7 .. 12 dB quiet)
PRESENCE_DB, PRESENCE_HZ, PRESENCE_OCT = 0.0, 2000.0, 1.0   # a broad bell in the bark region (round 2)
LIMITER_VOLUME = 0.40                 # sfx.cfg limiter volume (it plays on top of the top on-throttle loop)

# --------------------------------------------------------------------------------------------------- variants
# Yamaha YZ450F takes (round 2, variant C): Ramp 01 pulls 2,800 -> 12,000 (2.3 .. 12 s; ~1,400 rpm/s low, ~450 high), holds its rev
# limiter 12.0 .. 14.4 s at ~11,970 (a soft limiter: no clean ignition-cut pattern), then a closed-throttle overrun 15 .. 27 s. Ramp 03 / 04:
# pulls to 8,300, then slow climbs 4,400 -> 6,000 in a high gear (~200-300 rpm/s: long loops), overruns 16 .. 29 s / 19 .. 25 s.
# Start, Idle, Rev Ups, Stop: a steady 1,680 idle (2 .. 21 s). Their mic lag search is held to 6 ms (the 20 ms search found cycle aliases).
YR1, YR3, YR4, YIDLE = 'Yamaha Ramp 01', 'Yamaha Ramp 03', 'Yamaha Ramp 04', 'Yamaha Start, Idle, Rev Ups, Stop'
YAMAHA = dict(
    SOURCE_NOTE='Yamaha: Ramp 01 / Ramp 03 / Ramp 04 / Start, Idle, Rev Ups, Stop',
    ON_WINDOWS=[(YR1, 2.3, 12.1, +1), (YR3, 2.4, 7.2, +1), (YR3, 8.0, 13.6, +1), (YR4, 3.2, 8.8, +1), (YR4, 9.5, 18.1, +1)],
    OFF_WINDOWS=[(YR1, 14.6, 27.2, -1), (YR3, 15.2, 29.4, -1), (YR4, 19.0, 25.0, -1)],
    PREFER=YR1,
    ON_RPM=[3300, 3900, 4600, 5300, 6000, 6800, 7600, 8400, 9200, 10000, 10700, 11300, 11750],
    OFF_RPM=[3300, 4000, 4800, 5600, 6400, 7300, 8200, 9100, 10000, 10600],
    IDLE_AT=(YIDLE, 10.0, 2.0), LIMITER_AT=(YR1, 13.2, 0.6, 1))
VARIANTS = {
    'base': {},
    # A: a mild step up from round 1: more crest (punch), a lighter low shelf, a little presence, more exhaust, a bigger rpm climb
    'A': dict(PACK_NAME='Onboard 450 Aggressive A (Honda)', FILE_PREFIX='onb450a',
              DESCRIPTION='A - mild step up from round 1 (Honda): 9 dB crest (round 1: 7), 64 % exhaust, -3 dB low shelf (round 1: -6), +2 dB presence, 8 dB rpm climb (round 1: 5), longer overrun loops',
              TAKE_ARGS=dict(exhaust_w=0.80, engine_w=0.60), CREST_DB=9.0, SHELF_DB=-3.0, SHELF_HZ=120.0,
              PRESENCE_DB=2.0, PRESENCE_HZ=2000.0, PRESENCE_OCT=1.0, RISE_MAX_DB=8.0, OFF_MIN_DB=4.0, XFADE=0.25, CREST_TOP=7.5,
              OFF_MAX_WARP=10.0, OFF_MIN_LOOP=0.6, OFF_POPS=True, LIMITER_VOLUME=0.45),
    # B: a clear step: 75 % exhaust with a gentle drive (rasp), no low shelf, a broad bark lift, 11 dB crest, a 10 dB climb, crackly overrun
    'B': dict(PACK_NAME='Onboard 450 Aggressive B (Honda)', FILE_PREFIX='onb450b',
              DESCRIPTION='B - clear step (Honda): up to 11 dB crest (9 at the top), 75 % exhaust + gentle drive, no low shelf, +4 dB presence, 10 dB rpm climb, crackly overrun',
              TAKE_ARGS=dict(exhaust_w=0.87, engine_w=0.50, drive=3.0), CREST_DB=11.0, SHELF_DB=0.0,
              PRESENCE_DB=4.0, PRESENCE_HZ=2200.0, PRESENCE_OCT=0.9, RISE_MAX_DB=10.0, OFF_MIN_DB=5.0, XFADE=0.2, CREST_TOP=8.0,
              OFF_MAX_WARP=12.0, OFF_MIN_LOOP=0.7, OFF_POPS=True, LIMITER_VOLUME=0.5),
    # C: raw race exhaust: the Yamaha, 85 % exhaust driven harder, the bark lifted, up to 12 dB crest, the recording's own climb (8.8 dB)
    'C': dict(YAMAHA, PACK_NAME='Onboard 450 Aggressive C (Yamaha)', FILE_PREFIX='onb450c',
              DESCRIPTION='C - raw race exhaust (Yamaha YZ450F): up to 12 dB crest (9 at the top), 85 % exhaust + harder drive, no low shelf, +5 dB presence, the recording 9 dB rpm climb, crackly overrun',
              TAKE_ARGS=dict(exhaust_w=0.92, engine_w=0.39, drive=2.2, maxlag=0.006), CREST_DB=12.0, SHELF_DB=0.0,
              PRESENCE_DB=5.0, PRESENCE_HZ=2500.0, PRESENCE_OCT=0.9, RISE_MIN_DB=8.0, RISE_MAX_DB=12.0, OFF_MIN_DB=6.0, XFADE=0.2, CREST_TOP=9.0,
              OFF_MAX_WARP=12.0, OFF_MIN_LOOP=0.7, OFF_POPS=True, LIMITER_VOLUME=0.5),
}

def pick(T, rpm, windows, prefer, layer='on'):
    """Places to cut a loop at rpm: per take, the crossing of rpm in its windows with the best score (the warp the loop needs =
    slope / rpm, the track's wobble there, the tracker's confidence; the preferred take's scores x 0.7). Best first."""
    best = None; per = {}
    for take, a, b, d in windows:
        tk = T[take]
        for s in tk.crossings(rpm, a, b, d):
            slope = abs(tk.slope_at(s)) + 1.0
            mw, ml = (OFF_MAX_WARP, OFF_MIN_LOOP) if layer == 'off' else (MAX_WARP, MIN_LOOP)
            dur = float(np.clip(mw / 100 * rpm / slope, ml, MAX_LOOP))
            dur = min(dur, 2 * (b - s), 2 * (s - a))           # the whole loop inside the window
            if dur < ml: continue
            wob = tk.smoothness(s, dur / 2 + 0.05)
            conf = float(np.interp(s, tk.t, tk.conf))
            score = slope / rpm * 100 + 3 * wob + (2.0 if conf < 3 else 0.0)
            if take == prefer: score *= 0.7
            c = (score, take, s, dur, slope, wob, conf, (a, b))
            if take not in per or score < per[take][0]: per[take] = c
    if not per: raise ValueError(f'no crossing of {rpm} rpm')
    return sorted(per.values())

def fit_line(r, L):
    c = np.polyfit(np.asarray(r, float), np.asarray(L, float), 1); return c

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', required=True); ap.add_argument('--out', required=True)
    ap.add_argument('--cache', default=None); ap.add_argument('--gain', type=float, default=1.0)
    ap.add_argument('--report', default=None); ap.add_argument('--variant', default='base', choices=sorted(VARIANTS))
    ap.add_argument('--set', action='append', default=[], help='KEY=number: override a numeric constant (e.g. CREST_TOP=8.5)')
    A = ap.parse_args()
    globals().update(VARIANTS[A.variant])
    for kv in A.set:
        k, v = kv.split('='); globals()[k.strip()] = float(v)
    os.makedirs(A.out, exist_ok=True)
    names = sorted({w[0] for w in ON_WINDOWS + OFF_WINDOWS} | {IDLE_AT[0], LIMITER_AT[0]})
    T = {n: takes.Take(A.src, n, A.cache, **TAKE_ARGS) for n in names}
    rep = {'takes': {}, 'samples': [], 'limiter': None}
    for n, tk in T.items():
        r = tk.f0s * 120; ok = tk.conf > 3
        rep['takes'][n] = dict(lag_ms=tk.lag / tk.rate * 1000, lag_windows=tk.lag_n, agree_exhaust_pct=tk.agree * 100,
                               rpm_lo=float(np.percentile(r[ok], 2)), rpm_hi=float(np.percentile(r[ok], 98)))
        print(f'take {n}: mic lag {tk.lag / tk.rate * 1000:+.2f} ms, rpm {np.percentile(r[ok], 2):.0f} .. {np.percentile(r[ok], 98):.0f},'
              f' track agrees with the exhaust mic alone {tk.agree * 100:.0f} %')

    def make(kind, rpm_target, take, centre, secs, extra, multiple=1, window=None):
        """The loop near centre with the cleanest cycles: centres within +-NUDGE s are tried (inside the window), each scored by its
        worst cycle-to-cycle correlation (a pop or a bump would repeat every loop), its seam's correlation, and its warp."""
        tk = T[take]; best = None
        for d in NUDGE:
            c = centre + d
            if window and (c - secs / 2 < window[0] or c + secs / 2 > window[1]) and d != 0: continue
            try: x, rpm, info = loops.cut(tk.y, tk.rate, (tk.t, tk.f0s), c, secs, OUT_RATE, multiple=multiple)
            except ValueError: continue
            cs = loops.cycle_stats(x, rpm, OUT_RATE)
            if kind == 'off' and OFF_POPS:   # keep the crackle, avoid one pop that would tick every loop
                q = 0.9 - max(0.0, cs['rms_max'] - POP_MAX) + 0.5 * min(cs['seam_corr'], cs['inner_median']) - info['warp_pct'] / 25 - 0.5 * abs(d)
            else:
                q = min(cs['inner_min'], 0.9) + 0.5 * min(cs['seam_corr'], cs['inner_median']) - info['warp_pct'] / 25 - 0.5 * abs(d)
            if rpm_target: q -= 15 * abs(rpm / rpm_target - 1)          # stay near the planned rpm (the points keep their spacing)
            if best is None or q > best[0]: best = (q, x, rpm, info, cs, c)
        q, x, rpm, info, cs, c = best
        info.update(kind=kind, take=take, target=rpm_target, centre=c, cycles=cs, **extra)
        return dict(x=x, rpm=rpm, info=info, q=q)

    S_on, S_off = [], []
    idle = make('idle', None, IDLE_AT[0], IDLE_AT[1], IDLE_AT[2], {})
    for layer, targets, windows, dst in (('on', ON_RPM, ON_WINDOWS, S_on), ('off', OFF_RPM, OFF_WINDOWS, S_off)):
        for r in targets:
            # the best place in each take is cut; the cleanest loop wins (the preferred take gets a small bonus: one take's sound)
            cands = [make(layer, r, take, s, dur, dict(slope=slope, wobble=wob, conf=conf, score=score), window=w)
                     for score, take, s, dur, slope, wob, conf, w in pick(T, r, windows, PREFER, layer)[:2]]
            dst.append(max(cands, key=lambda c: c['q'] + (0.15 if c['info']['take'] == PREFER else 0.0)))
    lim = make('limiter', None, LIMITER_AT[0], LIMITER_AT[1], LIMITER_AT[2], {}, multiple=LIMITER_AT[3])

    for s in [idle, lim] + S_on + S_off:
        s['crest_raw'] = float(20 * np.log10(np.max(np.abs(s['x'])) / np.sqrt(np.mean(s['x'] ** 2))))
        x = dsp.presence(s['x'], OUT_RATE, PRESENCE_DB, PRESENCE_HZ, PRESENCE_OCT)
        s['x'] = dsp.loop_limiter(dsp.low_shelf(x, OUT_RATE, SHELF_DB, SHELF_HZ) if SHELF_DB else x, OUT_RATE, CREST_DB)
    # ---- levels: K-weighted loudness lines per layer, the recording's own slope / offset (clamped), every loop put on its line
    for s in [idle, lim] + S_on + S_off: s['L'] = dsp.loudness_db(s['x'], OUT_RATE)
    ron = [s['rpm'] for s in S_on]; roff = [s['rpm'] for s in S_off]
    con = fit_line(ron, [s['L'] for s in S_on]); coff = fit_line(roff, [s['L'] for s in S_off])
    r_idle = idle['rpm']; r_top = max(ron)
    rise_nat = np.polyval(con, r_top) - np.polyval(con, r_idle)
    rise = float(np.clip(rise_nat, RISE_MIN_DB, RISE_MAX_DB))
    mid = 0.5 * (min(roff) + max(roff))
    off_nat = np.polyval(con, mid) - np.polyval(coff, mid)
    off_db = float(np.clip(off_nat, OFF_MIN_DB, OFF_MAX_DB))
    def on_line(r): return rise * (r - r_idle) / (r_top - r_idle)            # dB, 0 at idle
    def off_line(r): return on_line(r) - off_db
    for s in S_on: s['target'] = on_line(s['rpm'])
    for s in S_off: s['target'] = off_line(s['rpm'])
    idle['target'] = off_line(r_idle) - IDLE_UNDER_OFF_DB
    lim['target'] = on_line(r_top)
    allS = [idle] + S_on + S_off + [lim]
    for s in allS: s['y'] = s['x'] * 10 ** ((s['target'] - s['L']) / 20)
    if CREST_TOP is None:
        # one global scale: the loudest file's peak at PEAK, with the soft knee taking the crest down where a file goes over the knee
        top = max(np.max(np.abs(s['y'])) for s in allS)
        for s in allS:
            y = s['y'] / top                                   # loudest peak at 1.0
            y = dsp.soft_clip(y, SOFT_KNEE) / dsp.soft_clip(np.array([1.0]), SOFT_KNEE)[0]
            s['y'] = y * PEAK
    else:
        # round 2 (a ceiling): the top on-throttle loop's rms sits CREST_TOP dB under PEAK; every file keeps its own crest (up to
        # CREST_DB) under the same ceiling PEAK. The quieter low / mid rpm loops have headroom: they keep more of the firings' punch,
        # while the loud top, where the mix peaks, is held.
        top_on = max(S_on, key=lambda s: s['target'])
        k = PEAK * 10 ** (-CREST_TOP / 20) / np.sqrt(np.mean(top_on['y'] ** 2))
        for s in allS:
            y = dsp.loop_limiter(s['y'] * k, OUT_RATE, 0.0, ceiling=PEAK * 0.97)
            s['y'] = dsp.soft_clip(y / PEAK, SOFT_KNEE) * PEAK
    for s in allS:
        s['L_final'] = dsp.loudness_db(s['y'], OUT_RATE); s['peak'] = float(np.max(np.abs(s['y'])))
        s['crest_db'] = float(20 * np.log10(s['peak'] / (np.sqrt(np.mean(s['y'] ** 2)) + 1e-12)))
        s['seam'] = loops.seam_check(s['y'], s['rpm'] if s is not lim else lim['rpm'])

    # ---- the files
    def fname(kind, rpm): return f'{FILE_PREFIX}_{kind}_{int(round(rpm)):05d}.wav'
    idle['file'] = f'{FILE_PREFIX}_idle.wav'; lim['file'] = f'{FILE_PREFIX}_limiter.wav'
    for s in S_on: s['file'] = fname('on', s['rpm'])
    for s in S_off: s['file'] = fname('off', s['rpm'])
    for f in os.listdir(A.out):
        if f.startswith(FILE_PREFIX + '_') and f.endswith('.wav'): os.remove(os.path.join(A.out, f))   # our own previous build only
    for s in allS: wavio.write16(os.path.join(A.out, s['file']), s['y'], OUT_RATE)

    max_rpm = round(lim['rpm'] / 10) * 10           # the limiter's own rpm: the pack's max_rpm (the Reflex 450F's 11,500 maps here)
    M = float(max_rpm)
    # layer 0 (on throttle): the idle loop, then the pull; layer 1 (off throttle): the idle loop, then the overrun
    L0 = [idle] + S_on; L1 = [idle] + S_off
    PL = (1.0 - XFADE) / 2                          # each neighbour's plateau share of a gap
    def table(layer):
        rp = [s['rpm'] for s in layer]; out = []
        for i, s in enumerate(layer):
            pts = []
            lo = rp[i - 1] if i > 0 else None; hi = rp[i + 1] if i + 1 < len(layer) else None
            # full volume on a plateau around its own rpm, an equal-power crossfade (cos / sin, 5 points) across the middle XFADE of
            # each gap to the neighbour
            def fade(a, b, rising):
                u = np.linspace(0, 1, 5); g = np.sin(np.pi / 2 * u) if rising else np.cos(np.pi / 2 * u)
                return [(a + (b - a) * uu, gg) for uu, gg in zip(u, g)]
            if lo is None: pts.append((0.0, 1.0))
            else:
                gap = rp[i] - lo; pts += fade(lo + PL * gap, rp[i] - PL * gap, True)
            if hi is None: pts.append((max(M, rp[i]) * 1.05, 1.0))
            else:
                gap = hi - rp[i]; pts += fade(rp[i] + PL * gap, hi - PL * gap, False)
            # dedupe and sort
            pts = sorted(dict((round(v, 3), g) for v, g in pts).items())
            minp = 0.0 if lo is None else (lo + PL * (rp[i] - lo)) / rp[i]
            maxp = (max(M, rp[i]) * 1.05) / rp[i] if hi is None else (hi - PL * (hi - rp[i])) / rp[i]
            out.append(dict(s=s, pts=pts, ref=rp[i] / M, minp=minp, maxp=maxp))
        return out
    T0, T1 = table(L0), table(L1)
    with open(os.path.join(A.out, 'engine.scl'), 'w') as f:
        f.write(f';; {PACK_NAME}: engine sound layers (MX Bikes engine.scl). Built by tests/engine_audio/packbuild/build.py from the\n')
        f.write(f';; 450cc 4-stroke onboard recordings supplied by the project owner ({SOURCE_NOTE}).\n')
        f.write(';; Values are rpm / MaxValue; RefPitch is the rpm each loop was cut at (exact: whole firing cycles at 48 kHz).\n')
        f.write(f'MaxValue = {M:.6f}\nAutoPitch = 1\n')
        for li, tab in enumerate((T0, T1)):
            f.write(f'Layer{li}\n{{\n\tNumSamples = {len(tab)}\n')
            for si, e in enumerate(tab):
                f.write(f'\tSample{si}\n\t{{\n\t\tWav = {e["s"]["file"]}\n\t\tNumPoints = {len(e["pts"])}\n')
                for pi, (v, g) in enumerate(e['pts']):
                    f.write(f'\t\tPoint{pi}\n\t\t{{\n\t\t\tValue = {v / M:.6f}  ;; {v:.0f}\n\t\t\tVolume = {g:.6f}\n\t\t}}\n')
                f.write(f'\t\tRefPitch = {e["ref"]:.6f}  ;; {e["s"]["rpm"]:.1f}\n\t\tMinPitch = {e["minp"]:.6f}\n\t\tMaxPitch = {e["maxp"]:.6f}\n\t}}\n')
            f.write('}\n')
    with open(os.path.join(A.out, 'sfx.cfg'), 'w') as f:
        f.write('engine\n{\n\tscl = engine.scl\n\tmin_distance = 3\n\tpos\n\t{\n\t\tx = 0\n\t\ty = 0.6\n\t\tz = 0\n\t}\n')
        f.write(f'\tmax_rpm = {M:.0f}\n\tidlethrottle = 0.001\n\tblend_exp = 1.5\n')
        f.write(f'\tlimiter\n\t{{\n\t\twav = {lim["file"]}\n\t\tvolume = {LIMITER_VOLUME}\n\t}}\n}}\n')
    with open(os.path.join(A.out, 'pack.ini'), 'w') as f:
        f.write(f'name = {PACK_NAME}\n')
        f.write("author = owner's library\n")
        f.write('source = 450cc 4-stroke onboard recordings (supplied by the project owner)\n')
        f.write(f"library = made from the owner's sound library (Dirt Bike 450cc 4 Stroke Onboard), {SOURCE_NOTE.split(':')[0]} takes only\n")
        if DESCRIPTION: f.write(f'variant = {DESCRIPTION}\n')
        f.write('license = not cleared to ship with the game: the owner decides\n')
        f.write(f'built = tests/engine_audio/packbuild/build.py --variant {A.variant} (OPEN REFLEX, w-pack450)\n')
        f.write(f'gain = {A.gain:.3f}\n')                    # since w-packloud only the fallback when the game cannot measure the pack
        if TRIM != 1.0: f.write(f'trim = {TRIM:.3f}\n')

    for s in allS:
        i = s['info']
        rep['samples'].append(dict(file=s['file'], kind=i['kind'], rpm=s['rpm'], target=i['target'], take=i['take'], start=i['start'],
                                   seconds=i['seconds'], cycles=i['N'], warp_pct=i['warp_pct'], align_corr=i['corr'],
                                   align_resid=i['resid'], xfade_ms=i['xfade_ms'], crest_raw_db=s['crest_raw'], cycles_stats=i['cycles'], loud_raw=s['L'], loud=s['L_final'],
                                   peak=s['peak'], crest_db=s['crest_db'], seam=s['seam']))
    rep['levels'] = dict(rise_natural_db=float(rise_nat), rise_db=rise, off_natural_db=float(off_nat), off_db=off_db, max_rpm=M)
    print(f'levels: on-throttle rise idle->top {rise_nat:.1f} dB in the recording, {rise:.1f} dB in the pack; off under on {off_nat:.1f} dB'
          f' recorded, {off_db:.1f} dB in the pack; max_rpm {M:.0f}')
    print(f'{"file":30s} {"rpm":>7s} {"take":36s} {"start":>6s} {"len s":>6s} {"cyc":>4s} {"warp%":>5s} {"corr":>5s} {"peak":>5s} {"LUFS~":>6s} {"crest":>5s} {"seam d1 pct":>11s} {"seam corr":>9s}')
    for s in rep['samples']:
        sm = s['seam']
        print(f'{s["file"]:30s} {s["rpm"]:7.1f} {s["take"]:36s} {s["start"]:6.2f} {s["seconds"]:6.3f} {s["cycles"]:4d} {s["warp_pct"]:5.1f}'
              f' {s["align_corr"]:5.2f} {s["peak"]:5.2f} {s["loud"]:6.1f} {s["crest_db"]:5.1f} {sm["jump_pct"]:11.1f} {sm["seam_corr"]:5.2f}/{sm["inner_corr"]:4.2f}')
    if A.report:
        with open(A.report, 'w') as f: json.dump(rep, f, indent=1)

if __name__ == '__main__':
    main()
