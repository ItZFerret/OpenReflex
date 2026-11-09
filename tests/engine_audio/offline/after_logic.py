# EnginePackPlayer.Update offline: After = w-enginepacks (MixGain 0.30); Loud = w-packloud (the per-pack loudness gain + the peak control),
# kept line for line in step with the C#.
import math
import render as R
import packs as P
MixGain = 0.30                 # the pack mix's gain (was 0.9): the summed, already full-scale samples stay below full scale
LimiterOnFraction = 0.995      # rpm / the Reflex max rpm at which the engine is on its limiter (the core clamps rpm at max)
LimiterOffFraction = 0.97      # ... and below which it has left it (hysteresis)
LimiterMinThrottle = 0.9       # the throttle held to bounce on the limiter
LimiterReleaseThrottle = 0.6   # ... released below this
LimiterHoldSeconds = 0.25      # sustained at the limiter before the first cut is heard
LimiterReleaseSeconds = 0.08   # the limiter loop fades out over this when the engine leaves the limiter
LimiterInAir = False           # in the air the Reflex engine free-revs to its max: not a limiter bounce
class After:
    name = 'after'
    def __init__(self, p, gain=None):   # gain None: pack.ini gain, as w-enginepacks' C# (g = MixGain x pack.gain)
        self.p = p; self.rpmNorm = -1.0; self.thr = 0.0; self.atLim = False; self.held = 0.0; self.limGain = 0.0
        self.gain = gain if gain is not None else (p.iniGain if getattr(p, 'iniGain', None) is not None else 1.0)
    def frame(self, rpm, idleR, maxR, thrIn, dt, air, audioRpm=None, audioThr=None):
        """audioRpm / audioThr (w-packrpm, EnginePackRpm): what the voices follow; the limiter keeps the true rpm / throttle."""
        p = self.p
        if maxR <= 0: maxR = p.maxRpm
        if idleR <= 0 or idleR >= maxR: idleR = maxR * 0.12
        frac = R.clamp01(((rpm if audioRpm is None else audioRpm) - idleR) / (maxR - idleR))
        idleP = P.idle_norm(p) * p.maxValue; maxP = max(idleP + 1, p.maxRpm)
        n = R.clamp01((idleP + frac * (maxP - idleP)) / p.maxValue)
        self.rpmNorm = n if self.rpmNorm < 0 else R.lerp(self.rpmNorm, n, 1 - math.exp(-dt * 30))
        self.thr = R.lerp(self.thr, R.clamp01(thrIn if audioThr is None else audioThr), 1 - math.exp(-dt * 20))
        on = max(self.thr, p.idleThrottle) ** max(0.01, p.blendExp)
        g = MixGain * self.gain
        vols = []; pitches = []
        for l in range(2):
            for s in p.layers[l]:
                mix = on if l == 0 else 1 - on
                vols.append(R.clamp01(g * mix * R.vol_at(s, self.rpmNorm)))
                pt = 1.0 if (not p.autoPitch or s.ref <= 0) else min(max(self.rpmNorm / s.ref, s.minp), s.maxp)
                pitches.append(min(max(pt, 0.05), 3.0))
        q = rpm / maxR
        grounded = LimiterInAir or not air
        if self.atLim: stay = grounded and q >= LimiterOffFraction and thrIn >= LimiterReleaseThrottle
        else: stay = grounded and q >= LimiterOnFraction and thrIn >= LimiterMinThrottle
        if not self.atLim:
            self.held = self.held + dt if stay else 0.0
            limit = self.held >= LimiterHoldSeconds
        else:
            limit = stay
            if not limit: self.held = 0.0
        rise = limit and not self.atLim; fall = (not limit) and self.atLim
        self.atLim = limit
        self.limGain = 1.0 if limit else max(0.0, self.limGain - dt / LimiterReleaseSeconds)
        return vols, pitches, g * p.limiterVolume * self.limGain, rise, fall, limit


# ---- w-packloud ----
import loudness as LD
PeakCeiling = 0.89            # -1 dBFS: the most the pack's predicted peak may reach at the output (channel gain in, pan <= 1)
PeakKneeDb = 6.0              # the soft knee (dB): reduction starts 3 dB under the ceiling, the output reaches the ceiling 3 dB over it
ENGINE_SIZE = 4               # the engine size the pack stands in for (the loudness target; set by the caller)
PeakReleaseSeconds = 0.30     # the gain reduction's release time constant (attack: at once, the bound is known before the volumes are set)
class Loud(After):
    """w-packloud: g = channel x the pack's loudness gain (loudness.pack_gain) x pack.ini trim (pack.ini gain: the C# fallback only); then the predicted peak
    sum(volume_i x peak_i) + limiter volume x its peak is held under PeakCeiling by a soft-knee gain computer with instant attack and a
    PeakReleaseSeconds release. The renderer multiplies its output by the channel itself: volumes come back divided by it."""
    name = 'loud'
    def __init__(self, p, gain=1.0, channel=0.7):
        super().__init__(p, gain * getattr(p, 'trim', 1.0)); self.channel = channel; self.loud, self.level = LD.pack_gain(p, size=ENGINE_SIZE); self.red = 0.0; self.reductions = []
        self.peaks = None; self.limpeak = 0.0
    def setup(self, peaks, limpeak): self.peaks = peaks; self.limpeak = limpeak
    def frame(self, rpm, idleR, maxR, thrIn, dt, air, audioRpm=None, audioThr=None):
        global MixGain
        keep = MixGain; MixGain = self.channel * self.loud     # After.frame's g = MixGain x gain: here channel x loudness x pack.ini
        try: vols, pitches, limvol, rise, fall, limit = After.frame(self, rpm, idleR, maxR, thrIn, dt, air, audioRpm, audioThr)
        finally: MixGain = keep
        bound = sum(v * pk for v, pk in zip(vols, self.peaks or [1.0] * len(vols))) + min(1.0, limvol) * self.limpeak
        want = 0.0
        if bound > 1e-9:
            x = 20 * math.log10(bound); T = 20 * math.log10(PeakCeiling); W = PeakKneeDb   # threshold = the ceiling, knee T - W/2 .. T + W/2
            if x <= T - W / 2: y = x
            elif x < T + W / 2: y = x - (x - T + W / 2) ** 2 / (2 * W)
            else: y = T
            want = y - x                                      # <= 0
        self.red = want if want < self.red else self.red + (want - self.red) * (1 - math.exp(-dt / PeakReleaseSeconds))
        r = 10 ** (self.red / 20); self.reductions.append(self.red)
        c = self.channel
        return [v * r / c for v in vols], pitches, min(1.0, limvol) * r / c, rise, fall, limit


# ---- w-packpeak ----
class Peak(Loud):
    """w-packpeak (the game): Loud's per-pack loudness gain x trim, no predicted-peak control; the game's mixer (render.py with packmix.py:
    Hermite resampling, the lookahead limiter on the mixed output, channel in) holds the real peaks."""
    name = 'peak'; mixer = True
    def frame(self, rpm, idleR, maxR, thrIn, dt, air):
        # no AudioSource any more: a voice's volume is not clamped at 1 (After.frame clamps: run it at a tiny gain and scale back up)
        global MixGain
        tiny = 1e-4; keep = MixGain; MixGain = tiny
        try: vols, pitches, limvol, rise, fall, limit = After.frame(self, rpm, idleR, maxR, thrIn, dt, air)
        finally: MixGain = keep
        k = self.loud / tiny     # x channel x loud / channel: the renderer multiplies by the channel
        return [v * k for v in vols], pitches, limvol * k, rise, fall, limit
