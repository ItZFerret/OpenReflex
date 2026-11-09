// A custom third-person camera preset over the game's view (w-campresets; the presets and the cycle: Options/ReflexCameraPresets.cs). The game's camera of the
// preset's BASE view gives the pose every frame (OriginalWorldRuntime.ApplyGameCamera); this moves it, client-side only (nothing goes back to the game):
//   place     DISTANCE back / in along the view, HEIGHT up / down, SIDE right / left; the camera then aims again at the point of the game's view ray nearest
//             the bike (so the bike keeps its place in the frame), PITCH tilts that aim (+ down)
//   hold      (w-camfix) the offsets start from the game's camera put back at a steady distance from the bike along its own direction, not from where
//             the game's camera is this frame: the game's chase camera moves in and out on its own (about 12 .. 17 ft behind an MX bike: its speed
//             pull-in and speed FOV, the slope springs; 26 ft while it catches up after the gate drops; the start gate's camera is 6.8 ft away,
//             core/tools/camera_distance_probe.c), so the set DISTANCE used to look further or closer from moment to moment and event to event.
//             The steady distance is the game camera's own when riding settled, fixed per vehicle type and BASE view: MX measured headless
//             (12.4 ft VISUAL, 13.1 ft GAME PLAY / USER), another vehicle the mean of its first 20 s of settled riding in the session
//   horizon   level (ROLL 0) .. the bike's whole lean (1); the game's own roll is replaced (a wreck's tumble is not followed)
//   lag       the camera's place around the bike and its aim trail the target by POSITION LAG / ROTATION LAG seconds (exponential, frame-rate independent)
//   FOV       a scale of the game view's own field of view (tangent space), or fixed horizontal degrees (as the first-person view's); the built-in views keep
//             CHASE CAMERA FOV
//   shake     "buzz" (SPEED: from 17 mph, full at 75; ROUGH: the vertical velocity's quick part while on the ground) and "jolts" (LANDING: a landing after
//             0.12 s in the air by the fall speed, a hard change of velocity, a wreck starting), times SHAKE. Smooth deterministic noise (Perlin) sampled on
//             the presentation clock, so the same ride shakes the same at any frame rate; it only moves the view
//   terrain   the result never goes below the ground (the core's collision grid, else the Unity terrain) + 1.6 ft where the game's own camera is above it
//             (the one pull-in that stays: a slope behind the bike lifts the camera)
//   blend     the adjustments fade in / out over 0.45 s as the driving camera starts / ends: the race running (InProgress .. WindDown, or no race state:
//             Free Ride) and the game's camera within 60 ft of the bike, looking at it. The intro's fly-by, the start gate (Pre / Countdown: the game's
//             gate camera, w-camfix: a preset over it put the camera inside the rider) and the post-race and podium cameras stay the game's; the
//             preset blends in as the gate drops (the camera editor previews it at the gate too)
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    public sealed class ReflexCameraRig
    {
        public struct Frame
        {
            public Vector3 gamePos, gameForward, gameUp;
            /// The game view's own vertical field of view (radians); aspect = the camera's.
            public float gameFov, aspect;
            public Vector3 bike, velocity;
            public Quaternion bikeRot;
            /// ft/s (the game's speed).
            public float speed;
            /// The driving camera is the view (the race runs: InProgress .. WindDown, or no race; the editor's preview also at the gate): the adjustments apply.
            public bool riding;
            /// The steady distance's key (vehicle type and BASE view: ReflexCameraRig.HoldKey).
            public int holdKey;
            /// The onboard contacts (rx_session_onboard_state): grounded = a wheel touches; wrecked = the rider is off / falling.
            public bool contactsKnown, grounded, wrecked;
            public OriginalTerrain terrain;
            /// Seconds since the last frame while the race runs (0 while paused); preview: seconds while the editor previews over a paused race.
            public float dt, preview;
        }

        const float BlendTime = 0.45f, Clearance = 1.6f, BuzzHz = 15f, JoltHz = 7f, NearBike = 60f;

        bool have; float weight, settled;
        Vector3 offset, lastBike; Quaternion aim;
        float clock, trauma, roughEnv, vyLow, airTime, minVy, bank;
        bool lastGrounded = true, lastWrecked;
        Vector3 lastVelocity; float velocityAge; bool haveVelocity;

        /// The shake on screen now (0 = none; tests and the showcase), its jolts (landings / impacts) and the rough-ground part, and the adjustments' blend.
        public float Shake { get; private set; }
        public float Trauma => trauma;
        public float Rough { get; private set; }
        public float Weight => weight;
        /// The frame the rig last moved the camera (Time.frameCount; tests: whether Weight is current).
        public int AppliedFrame { get; private set; } = -1;

        public void Reset()
        {
            have = false; weight = 0f; settled = 0f; trauma = 0f; roughEnv = 0f; airTime = 0f; minVy = 0f; bank = 0f; Shake = 0f; Rough = 0f;
            lastGrounded = true; lastWrecked = false; haveVelocity = false;
        }

        /// The preset over the game's view; false = show the game's view as it is (no preset, a broken pose).
        public bool Apply(Reflex.Port.Options.CameraPreset p, in Frame f, out Vector3 pos, out Quaternion rot, out float fovDeg)
        {
            pos = f.gamePos; rot = Quaternion.identity; fovDeg = 0f;
            if (p == null || f.gameForward.sqrMagnitude < 0.25f) { Reset(); return false; }
            float dt = Mathf.Clamp(f.dt, 0f, 0.1f), step = dt > 0f ? dt : Mathf.Clamp(f.preview, 0f, 0.1f);

            // the blend: in over the driving camera, out for the game's other cameras (the intro's fly-by, a gate or post-race camera: away from the
            // bike or not looking at it). A vehicle / view whose steady distance is not known yet waits until it rides settled (its first ride only)
            Vector3 toBike = f.bike - f.gamePos;
            bool chasing = toBike.sqrMagnitude < NearBike * NearBike && Vector3.Dot(toBike, f.gameForward) > 0f;
            bool cut = !have || (f.bike - lastBike).sqrMagnitude > 30f * 30f;   // (the game put the bike back on the track: cut, do not swoop)
            settled = f.riding && chasing && !f.wrecked && !cut ? settled + dt : 0f;
            Vector3 rel = f.gamePos - f.bike; float gameDist = rel.magnitude;
            float hold = Hold(f.holdKey, gameDist, settled, dt);
            HoldDistance = hold; GameDistance = gameDist;
            float target = f.riding && chasing && (hold > 0f || f.preview > 0f) ? 1f : 0f;
            weight = !have ? target : Mathf.MoveTowards(weight, target, step / BlendTime);
            float w = weight * weight * (3f - 2f * weight);

            // hold: the game's camera at the steady distance along its own direction from the bike (w 0: where the game put it)
            Vector3 F = f.gameForward.normalized, P = f.gamePos;
            if (gameDist > 0.5f && hold > 0f) P = f.bike + rel * (Mathf.Lerp(gameDist, hold, w) / gameDist);

            // place and aim
            Vector3 right = Vector3.Cross(Vector3.up, F);
            if (right.sqrMagnitude < 1e-4f) right = Vector3.Cross(f.gameUp, F);
            right.Normalize();
            Vector3 aimPoint = P + F * Mathf.Clamp(Vector3.Dot(f.bike - P, F), 3f, 150f);
            Vector3 P1 = P - F * (p.distance * w) + Vector3.up * (p.height * w) + right * (p.side * w);
            Vector3 F1 = aimPoint - P1;
            F1 = F1.sqrMagnitude > 0.01f ? F1.normalized : F;
            Vector3 r1 = Vector3.Cross(Vector3.up, F1); r1 = r1.sqrMagnitude > 1e-4f ? r1.normalized : right;
            F1 = Quaternion.AngleAxis(p.pitch * w, r1) * F1;

            // the horizon: the game's own roll (w 0) .. level (ROLL 0) / the bike's lean x ROLL (w 1); a wreck's tumble is not followed
            float gameRoll = RollOf(F, f.gameUp);
            float lean = f.wrecked ? 0f : Mathf.Clamp(BankOf(f.bikeRot), -60f, 60f);
            bank = step > 0f ? Mathf.Lerp(bank, lean, 1f - Mathf.Exp(-step / 0.12f)) : bank;
            if (!have) bank = lean;
            Quaternion R = Quaternion.AngleAxis(Mathf.Lerp(gameRoll, bank * p.rollFollow, w), F1) * Quaternion.LookRotation(F1, Vector3.up);

            // lag: the place around the bike and the aim trail (none while the game's other cameras show)
            Vector3 off = P1 - f.bike;
            float posLag = p.posLag * w, rotLag = p.rotLag * w;
            if (cut) { offset = off; aim = R; }
            else
            {
                offset = posLag > 0.001f ? Vector3.Lerp(offset, off, 1f - Mathf.Exp(-step / posLag)) : off;
                aim = rotLag > 0.001f ? Quaternion.Slerp(aim, R, 1f - Mathf.Exp(-step / rotLag)) : R;
            }
            have = true; lastBike = f.bike; AppliedFrame = Time.frameCount;
            pos = f.bike + offset; rot = aim;

            // shake
            if (dt > 0f) Sources(f, dt);
            clock += step;
            float speedAmt = Mathf.SmoothStep(0f, 1f, Mathf.InverseLerp(25f, 110f, f.speed));
            float buzz = p.shake * w * (p.shakeSpeed * 0.55f * speedAmt + p.shakeRough * Rough);
            float jolt = p.shake * w * p.shakeLanding * trauma * trauma;
            Shake = buzz + jolt;
            if (Shake > 1e-4f)
            {
                float pitch = buzz * 0.55f * N(0, BuzzHz) + jolt * 2.6f * N(3, JoltHz);
                float yaw = buzz * 0.35f * N(1, BuzzHz) + jolt * 1.2f * N(4, JoltHz);
                float roll = buzz * 0.45f * N(2, BuzzHz) + jolt * 1.8f * N(5, JoltHz);
                rot = rot * Quaternion.Euler(pitch, yaw, roll);
                pos += rot * Vector3.up * (buzz * 0.09f * N(6, BuzzHz) + jolt * 0.35f * N(7, JoltHz))
                     + rot * Vector3.right * (buzz * 0.05f * N(8, BuzzHz) + jolt * 0.2f * N(9, JoltHz));
            }

            // never under the ground (where the game's own camera is above it)
            if (f.terrain && Ground(f.terrain, pos, out float h) && pos.y < h + Clearance && (!Ground(f.terrain, P, out float hg) || P.y >= hg))
                pos.y = h + Clearance;

            // the field of view: the built-in's (CHASE CAMERA FOV) blended to the preset's
            float baseFov = Reflex.Port.Options.ReflexClientLive.ChaseFov(f.gameFov) * Mathf.Rad2Deg;
            float own = p.fovMode == 1 ? OriginalOnboardCamera.VerticalFov(p.fovDegrees, f.aspect)
                                       : Mathf.Clamp(2f * Mathf.Atan(Mathf.Tan(f.gameFov * 0.5f) * p.fovScale / 100f), 0.1f, 2.9f) * Mathf.Rad2Deg;
            fovDeg = Mathf.Lerp(baseFov, own, w);
            return true;
        }

        // ---- hold: the game camera's steady distance from the bike, per key (static: kept over the session's events). Fixed once known, so it
        // never drifts: MX from the measurement below, another vehicle / view the mean of its first 20 s of settled riding
        const int HoldKeys = 32;
        const float SettleTime = 2.5f, LearnTime = 20f;
        static readonly float[] s_hold = new float[HoldKeys], s_holdSeconds = new float[HoldKeys];
        /// The key of a vehicle type (ReflexVehicleInfo.type: 0 MX, 1 ATV, 2.. machines; -1 unknown) and the BASE view (DrivingCamera 0..3).
        public static int HoldKey(int vehicleType, int baseView) => (Mathf.Clamp(vehicleType + 1, 0, 7) * 4 + Mathf.Clamp(baseView, 0, 3)) % HoldKeys;
        /// The steady distance now (0: not known yet: the game's own distance is used) and the game camera's distance (tests and the showcase).
        public float HoldDistance { get; private set; }
        public float GameDistance { get; private set; }

        // MX (the runtime passes MX for a world without vehicle info): the game camera's steady distance measured headless (camera_distance_probe,
        // NAT_Concord / NAT_Stillwater / SX_Kingston, 35 ft/s): VISUAL 12.4 ft, GAME PLAY and USER 13.0 .. 13.2 ft (14.1 at 60 ft/s). 0: not measured
        static float Measured(int key) => key / 4 - 1 == 0 ? (key % 4 == 0 ? 12.4f : 13.1f) : 0f;

        // the steady distance for this frame (0: not known yet); learns a vehicle / view without a measurement while it rides settled
        static float Hold(int key, float gameDist, float settledFor, float dt)
        {
            if (key < 0 || key >= HoldKeys) return 0f;
            float m = Measured(key);
            if (m > 0f) return m;
            float h = s_hold[key];
            if (s_holdSeconds[key] < LearnTime && settledFor >= SettleTime && dt > 0f && gameDist > 1f && gameDist < 60f)
            {
                s_holdSeconds[key] += dt;
                h = h <= 0f ? gameDist : h + (gameDist - h) * dt / s_holdSeconds[key];   // the running mean
                s_hold[key] = h;
            }
            return s_holdSeconds[key] >= SettleTime ? h : 0f;   // (a few seconds of riding before it is used)
        }

        // the shake's sources from the presented frames (dt > 0: the race runs)
        void Sources(in Frame f, float dt)
        {
            float vy = f.velocity.y;
            bool grounded = f.contactsKnown ? f.grounded : !f.terrain || !Ground(f.terrain, f.bike, out float g) || f.bike.y - g < 2.5f;
            // a landing: the fall speed after some time in the air
            if (!grounded) { airTime += dt; minVy = Mathf.Min(minVy, vy); }
            else
            {
                if (!lastGrounded && airTime > 0.12f) Kick(Mathf.Clamp01((-minVy - 6f) / 45f));
                airTime = 0f; minVy = 0f;
            }
            // a hard change of velocity (a crash into something, a casing): over the time since the velocity last changed (held frames repeat it)
            velocityAge += dt;
            if (f.velocity != lastVelocity)
            {
                if (haveVelocity && velocityAge > 1e-4f)
                {
                    float accel = (f.velocity - lastVelocity).magnitude / Mathf.Max(velocityAge, 1f / 120f);
                    if (accel > 110f) Kick(Mathf.Clamp01((accel - 110f) / 300f) * 0.8f);
                }
                lastVelocity = f.velocity; velocityAge = 0f; haveVelocity = true;
            }
            if (f.wrecked && !lastWrecked) Kick(0.85f);
            lastGrounded = grounded; lastWrecked = f.wrecked;
            // rough ground: the vertical velocity's quick part while a wheel is down
            vyLow = Mathf.Lerp(vyLow, vy, 1f - Mathf.Exp(-dt / 0.3f));
            roughEnv = Mathf.Lerp(roughEnv, grounded ? Mathf.Abs(vy - vyLow) : 0f, 1f - Mathf.Exp(-dt / 0.2f));
            Rough = Mathf.Clamp01(roughEnv / 5f) * Mathf.Clamp01(f.speed / 30f);
            trauma *= Mathf.Exp(-dt * 2.2f);
        }

        void Kick(float amount) { if (amount > 0f) trauma = Mathf.Clamp01(Mathf.Max(trauma, amount) + amount * 0.25f); }

        // smooth noise -1 .. 1 per channel (Perlin over the presentation clock)
        float N(int channel, float hz) => Mathf.PerlinNoise(channel * 17.31f + 3.7f, clock * hz) * 2f - 1f;

        static bool Ground(OriginalTerrain t, Vector3 p, out float h)
        {
            h = t.CoreHeightAt(p.x, p.z, out bool have);
            if (have) return true;
            var ut = t.UnityTerrain;
            if (!ut || ut.terrainData == null) return false;
            Vector3 o = ut.transform.position, s = ut.terrainData.size;
            if (p.x < o.x || p.z < o.z || p.x > o.x + s.x || p.z > o.z + s.z) return false;
            h = ut.SampleHeight(p) + o.y;
            return true;
        }

        // a view's roll from its level frame (+ raises the right side, as Quaternion.AngleAxis about the view's forward)
        static float RollOf(Vector3 forward, Vector3 up)
        {
            if (up.sqrMagnitude < 0.25f) return 0f;
            Vector3 level = Quaternion.LookRotation(forward, Vector3.up) * Vector3.up;
            return Vector3.SignedAngle(level, up.normalized, forward);
        }

        // the bike's lean the same way (+ right side up)
        static float BankOf(Quaternion bike)
        {
            Vector3 fwd = bike * Vector3.forward;
            if (Mathf.Abs(fwd.y) > 0.95f) return 0f;
            return RollOf(fwd, bike * Vector3.up);
        }
    }
}
