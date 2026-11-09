# The audio rpm / load model (w-packrpm), line for line the C# EnginePackRpm (Unity/Reflex/Assets/Scripts/OriginalWorld/EnginePackRpm.cs):
# what an engine pack hears instead of the core's raw rpm and throttle. Keep the two in step.
#
# Why: the packs are cut for sim-style gearing (MX Bikes: seconds per gear, the rpm sweeps the range), while Reflex's automatic gearbox shifts
# about once a second (450F computer riders: median 0.54 s per gear, 33 up / 31 down shifts a minute, the upshift at ~70 % of the range drops
# to ~55 % in one frame) and the rear wheel spins / hops (the rpm swings 1,000-3,000 rpm in 2-4 frames, ~400 steps a minute over 5 % of the
# range). Fed raw, the pack crossfades between its loops several times a second and its pitch lurches.
#
#   input      the rpm as a fraction of the engine's idle .. max range, optionally stretched about StretchCentre (Stretch > 1: the
#              in-gear climb covers more of the pack, like longer gearing; 1 = off), then a one-pole low-pass (InputSmoothSeconds)
#              against the wheel-spin jitter, then a backlash of +-Backlash (the held value only moves once the input has moved that far
#              from it: small back-and-forth swings no longer cross a loop boundary and back: loop-selection hysteresis);
#   follow     the audio fraction approaches it exponentially (RiseSeconds / FallSeconds) but never faster than RiseRate / FallRate
#              (range per second): an engine spins up at a believable rate and decays naturally; in the air the rise is AirRiseRate
#              (a free rev);
#   upshift    (gear up with the throttle open) for ShiftSeconds the fall is ShiftFallSeconds with no rate cap (the rpm drops fast, but over a
#              few frames, not in one), and the on-throttle blend dips by ShiftLoadDip on a raised-cosine (a brief load drop: the clutch /
#              ignition cut you hear on a real upshift), then returns to the throttle;
#   downshift  (gear down) for ShiftSeconds the rise may be DownshiftRiseRate (a quick blip), no load change;
#   limiter    not here: EnginePackPlayer keeps gating its limiter on the TRUE rpm / throttle (the core clamps rpm exactly at max on the
#              ground; a smoothed rpm would arrive late and leave late).
import math

InputSmoothSeconds = 0.05
Backlash = 0.025                       # fraction of the range (450F: 250 rpm; the packs' loops sit ~700 rpm apart)
RiseSeconds, FallSeconds = 0.07, 0.10
RiseRate, FallRate = 1.2, 1.6          # fractions of the idle .. max range per second (450F: 1.2 = 12,000 rpm/s)
AirRiseRate = 2.5
ShiftSeconds = 0.14
ShiftFallSeconds = 0.045
ShiftLoadDip = 0.75
ShiftMinThrottle = 0.3                 # an upshift with less throttle is a lift: no load dip
DownshiftRiseRate = 3.0
Stretch, StretchCentre = 1.0, 0.62

class Model:
    def __init__(self, stretch=None):
        self.f = -1.0; self.inp = -1.0; self.held = -1.0; self.gear = None; self.shift = 0.0; self.down = 0.0
        self.stretch = Stretch if stretch is None else stretch
    def step(self, rpm, idle, mx, thr, gear, air, dt):
        """-> (audio rpm, audio throttle)."""
        if mx <= 0: return rpm, thr
        if idle <= 0 or idle >= mx: idle = mx * 0.12
        x = min(1.0, max(0.0, (rpm - idle) / (mx - idle)))
        if self.stretch != 1.0: x = min(1.0, max(0.0, StretchCentre + (x - StretchCentre) * self.stretch))
        if self.gear is not None and gear != self.gear and gear > 0 and self.gear > 0:
            if gear > self.gear and thr > ShiftMinThrottle: self.shift = ShiftSeconds
            elif gear < self.gear: self.down = ShiftSeconds
        self.gear = gear
        self.inp = x if self.inp < 0 else self.inp + (x - self.inp) * (1 - math.exp(-dt / InputSmoothSeconds))
        self.held = self.inp if self.held < 0 else min(max(self.held, self.inp - Backlash), self.inp + Backlash)
        if self.f < 0: self.f = self.held
        d = self.held - self.f
        if d > 0:
            rate = DownshiftRiseRate if self.down > 0 else AirRiseRate if air else RiseRate
            self.f += min(d * (1 - math.exp(-dt / RiseSeconds)), rate * dt)
        else:
            if self.shift > 0: self.f += d * (1 - math.exp(-dt / ShiftFallSeconds))
            else: self.f += max(d * (1 - math.exp(-dt / FallSeconds)), -FallRate * dt)
        on = thr
        if self.shift > 0:
            u = 1 - self.shift / ShiftSeconds                      # 0 -> 1 over the shift
            on = thr * (1 - ShiftLoadDip * 0.5 * (1 - math.cos(2 * math.pi * u)))
            self.shift = max(0.0, self.shift - dt)
        self.down = max(0.0, self.down - dt)
        return idle + self.f * (mx - idle), on
