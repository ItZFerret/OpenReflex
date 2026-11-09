using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The audio rpm / load an engine pack follows (w-packrpm), instead of the core's raw rpm and throttle. One per pack player (per rider).
    // tests/engine_audio/packrpm/rpm_model.py is the same model in Python (keep the two in step); shift_stats.py measures the gearbox,
    // shift_render.py renders before / after.
    //
    // Why: the packs are cut for sim-style gearing (MX Bikes: seconds per gear, the rpm sweeps the range), while Reflex's automatic gearbox
    // shifts about once a second (450F computer riders, NAT_Concord: median 0.54 s per gear, 33 up / 31 down shifts a minute, the upshift at
    // ~70 % of the range drops to ~55 % in one frame) and the rear wheel spins and hops (the rpm swings 1,000-3,000 rpm in 2-4 frames: ~400
    // one-frame steps a minute over 5 % of the range). Fed raw, a pack crossfades between its loops 12-17 times a second and its pitch lurches
    // by up to 6 semitones in a frame.
    //
    //   input      the rpm as a fraction of the engine's idle .. max range, optionally stretched about StretchCentre (Stretch > 1: the
    //              in-gear climb covers more of the pack, like longer gearing; 1 = off), a one-pole low-pass (InputSmoothSeconds) against
    //              the wheel-spin jitter, then a backlash of +-Backlash (the held value only moves once the input has moved that far from it:
    //              small back-and-forth swings no longer cross a loop boundary and back, i.e. loop-selection hysteresis);
    //   follow     the audio fraction approaches it exponentially (RiseSeconds / FallSeconds) but never faster than RiseRate / FallRate
    //              (range per second): the engine spins up at a believable rate and decays naturally; in the air the rise is AirRiseRate
    //              (a free rev);
    //   upshift    (the gear goes up with the throttle open) for ShiftSeconds the fall is ShiftFallSeconds with no rate cap (the rpm drops
    //              fast, but over a few frames, not in one) and the on-throttle blend dips by ShiftLoadDip on a raised cosine (a brief load
    //              drop, like the ignition cut / clutch of a real upshift), then returns to the throttle;
    //   downshift  (the gear goes down) for ShiftSeconds the rise may be DownshiftRiseRate (a quick blip); no load change;
    //   limiter    not here: EnginePackPlayer keeps gating its limiter on the TRUE rpm and throttle (the core clamps the rpm exactly at max on
    //              the ground; a smoothed rpm would arrive late and leave late).
    // The original game smooths a car engine's rpm with RPMSmoothFactor 0.9 a frame (a ~160 ms lag); this lags ~100 ms on a 450F race.
    public sealed class EnginePackRpm
    {
        public const float InputSmoothSeconds = 0.05f;
        public const float Backlash = 0.025f;                    // fraction of the range (450F: 250 rpm; the packs' loops sit ~700 rpm apart)
        public const float RiseSeconds = 0.07f, FallSeconds = 0.10f;
        public const float RiseRate = 1.2f, FallRate = 1.6f;     // fractions of the idle .. max range per second (450F: 1.2 = 12,000 rpm/s)
        public const float AirRiseRate = 2.5f;
        public const float ShiftSeconds = 0.14f;
        public const float ShiftFallSeconds = 0.045f;
        public const float ShiftLoadDip = 0.75f;
        public const float ShiftMinThrottle = 0.3f;              // an upshift with less throttle is a lift: no load dip
        public const float DownshiftRiseRate = 3.0f;
        public const float Stretch = 1.0f, StretchCentre = 0.62f;   // Stretch > 1 widens the in-gear climb (try 1.3 .. 1.4 by ear); 1 = off

        float f = -1f, inp = -1f, held = -1f, shift, down;
        int gear = int.MinValue;

        /// The pack's rpm and throttle for this frame from the rider's telemetry (rpm, the engine's idle / max rpm, throttle, gear, airborne).
        public void Step(float rpm, float idleRpm, float maxRpm, float throttle, int gearNow, bool airborne, float dt, out float audioRpm, out float audioThrottle)
        {
            audioRpm = rpm; audioThrottle = throttle;
            if (maxRpm <= 0f) return;
            if (idleRpm <= 0f || idleRpm >= maxRpm) idleRpm = maxRpm * 0.12f;
            float x = Mathf.Clamp01((rpm - idleRpm) / (maxRpm - idleRpm));
            if (Stretch != 1f) x = Mathf.Clamp01(StretchCentre + (x - StretchCentre) * Stretch);
            if (gear != int.MinValue && gearNow != gear && gearNow > 0 && gear > 0)
            {
                if (gearNow > gear && throttle > ShiftMinThrottle) shift = ShiftSeconds;
                else if (gearNow < gear) down = ShiftSeconds;
            }
            gear = gearNow;
            inp = inp < 0f ? x : inp + (x - inp) * (1f - Mathf.Exp(-dt / InputSmoothSeconds));
            held = held < 0f ? inp : Mathf.Clamp(held, inp - Backlash, inp + Backlash);
            if (f < 0f) f = held;
            float d = held - f;
            if (d > 0f)
            {
                float rate = down > 0f ? DownshiftRiseRate : airborne ? AirRiseRate : RiseRate;
                f += Mathf.Min(d * (1f - Mathf.Exp(-dt / RiseSeconds)), rate * dt);
            }
            else if (shift > 0f) f += d * (1f - Mathf.Exp(-dt / ShiftFallSeconds));
            else f += Mathf.Max(d * (1f - Mathf.Exp(-dt / FallSeconds)), -FallRate * dt);
            float on = throttle;
            if (shift > 0f)
            {
                float u = 1f - shift / ShiftSeconds;                              // 0 -> 1 over the shift
                on = throttle * (1f - ShiftLoadDip * 0.5f * (1f - Mathf.Cos(2f * Mathf.PI * u)));
                shift = Mathf.Max(0f, shift - dt);
            }
            down = Mathf.Max(0f, down - dt);
            audioRpm = idleRpm + f * (maxRpm - idleRpm); audioThrottle = on;
        }

        /// Starts over (a new vehicle / a reset).
        public void Reset() { f = inp = held = -1f; shift = down = 0f; gear = int.MinValue; }
    }
}
