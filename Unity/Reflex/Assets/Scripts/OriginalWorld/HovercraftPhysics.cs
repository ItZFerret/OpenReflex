using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // THE DRIVABLE HOVERCRAFT (a port extra, not original game logic; core/plan/QUEUE.md "w-hover"): the ground the craft rides on and the
    // craft's own hover physics. Nothing here touches the running game: the terrain is the live Unity heightfield the port already builds
    // from the core's collision heights (OriginalTerrain), the water is the level's flat water plane (ReflexWater: the surface matrix's
    // height inside its extent, where the terrain is below it). Units are the game's (feet, seconds).

    /// The surface under a point: the terrain's collision height, or the level's water plane where it covers the terrain.
    public sealed class HoverGround
    {
        sealed class Plane { public float[] m; public float halfX, halfZ, y; }
        readonly List<Plane> planes = new List<Plane>();
        Terrain terrain; float terrainY; Vector2 min, max;

        /// Whether the terrain is there to be sampled.
        public bool Ready => terrain != null;
        /// The terrain's extent in x / z (the craft does not leave it).
        public Vector2 Min => min; public Vector2 Max => max;

        public HoverGround(OriginalTerrain t, ReflexLevelContent content)
        {
            if (t != null && t.Built)
            {
                terrain = t.UnityTerrain; terrainY = terrain.transform.position.y;
                var p = terrain.transform.position; var s = terrain.terrainData.size;
                min = new Vector2(p.x, p.z); max = new Vector2(p.x + s.x, p.z + s.z);
            }
            try
            {
                var water = content != null ? ReflexWater.Load(content) : null;
                if (water != null)
                    foreach (var s in water.surfaces)
                    {
                        var m = s.asset.matrix;
                        if (Mathf.Abs(m[5]) < 0.9f) continue;   // only level planes (the lake / the ocean); a tilted river is not ridden
                        planes.Add(new Plane { m = m, halfX = s.asset.sizeX * 0.5f, halfZ = s.asset.sizeZ * 0.5f, y = m[13] });
                    }
            }
            catch (Exception e) { Debug.LogWarning("Hovercraft: the level's water was not read: " + e.Message); }
        }

        /// The terrain's collision height at x, z.
        public float Terrain(float x, float z)
        {
            if (terrain == null) return 0f;
            return terrain.SampleHeight(new Vector3(x, 0, z)) + terrainY;
        }

        /// The water plane's height at x, z, or NaN where there is none.
        public float Water(float x, float z)
        {
            foreach (var p in planes)
            {
                float dx = x - p.m[12], dz = z - p.m[14];
                float lx = dx * p.m[0] + dz * p.m[2], lz = dx * p.m[8] + dz * p.m[10];   // the water space's x / z (the level plane: y is not used)
                if (Mathf.Abs(lx) <= p.halfX && Mathf.Abs(lz) <= p.halfZ) return p.y;
            }
            return float.NaN;
        }

        /// What the craft rides: the higher of the terrain and the water. water = the water plane is the surface.
        public float Height(float x, float z, out bool water)
        {
            float t = Terrain(x, z), w = Water(x, z);
            water = !float.IsNaN(w) && w > t;
            return water ? w : t;
        }

        /// The terrain's surface normal's horizontal slope (dh/dx, dh/dz) at x, z, from a 6 ft stencil.
        public Vector2 Slope(float x, float z, float step = 3f)
        {
            float a = Height(x + step, z, out _), b = Height(x - step, z, out _), c = Height(x, z + step, out _), d = Height(x, z - step, out _);
            return new Vector2((a - b) / (2 * step), (c - d) / (2 * step));
        }
    }

    /// One frame of the craft's controls (0..1 pedals, -1..1 steering).
    public struct HoverControls
    {
        public float throttle, brake, steer;
    }

    /// The craft's hover physics: a rigid body kept simple on purpose (planar thrust and drift on a cushion of air, four hover points
    /// that carry the craft up and tilt it with the ground), so it behaves the same at any frame rate and can never explode.
    public sealed class HoverPhysics
    {
        // ---- tuning (feet, seconds) ----
        public float hoverHeight = 2.4f;         // the craft's origin (the skirt's bottom) above the surface when the cushion is full
        public float cushionRange = 7f;          // above this the craft is airborne (no lift)
        public float maxSpeed = 125f;            // forward, level ground (ft/s): about 85 mph
        public float maxReverse = 38f;
        public float thrust = 34f;               // ft/s^2 at full throttle from rest
        public float brakeDecel = 46f;
        public float gravity = 32.2f;
        public float slopeGravity = 1.6f;       // how much of the downhill pull the cushion passes on (a hovercraft slides)
        public float halfLength = 26f, halfWidth = 15f;    // the hover points
        public float bodyHeight = 8f;            // the deck above the origin: terrain above this blocks the craft

        // ---- state ----
        public Vector3 position;                 // the origin (middle of the craft at the skirt's bottom)
        public Vector3 velocity;
        public float yaw, yawRate;               // radians, about +y; the heading is (sin yaw, 0, cos yaw)
        public Vector3 up = Vector3.up;          // the craft's up axis (tilts with the ground)
        public float cushion = 1f;               // 0 deflated .. 1 inflated (scales the lift and the hover height)
        public bool onWater, airborne;
        public float groundHeight;               // the surface under the origin
        public float impact;                     // the last wall hit's strength (read and cleared by the audio / effects)
        public int wallHits; public float maxImpact;   // diagnostics: walls met so far, the hardest (ft/s into the wall)
        public float latSlip, fwdSpeed;          // read-outs
        public float gripWater = 0.5f, gripLand = 1.35f;
        public float dragWater = 0.035f, dragLand = 0.12f, dragLandCoast = 0.26f;   // linear drag per second; off the throttle on land the cushion drags more
        public float deflatedSink = 7.4f;        // how far the origin sits below the surface with the skirt flat (cushion 0): the flat skirt's bottom is ~7.4 ft above the model's origin

        public Vector3 Forward => new Vector3(Mathf.Sin(yaw), 0, Mathf.Cos(yaw));
        public Vector3 Right => new Vector3(Mathf.Cos(yaw), 0, -Mathf.Sin(yaw));
        public Quaternion Rotation
        {
            get
            {
                var f = Vector3.ProjectOnPlane(Forward, up);
                if (f.sqrMagnitude < 1e-6f) f = Forward;
                return Quaternion.LookRotation(f.normalized, up);
            }
        }
        public float Speed => new Vector2(velocity.x, velocity.z).magnitude;

        float bob;

        /// Puts the craft at a pose (parked on the ground, cushion given).
        public void Place(Vector3 pos, float heading, float cushionAmount)
        {
            position = pos; velocity = Vector3.zero; yaw = heading; yawRate = 0; up = Vector3.up; cushion = cushionAmount;
            airborne = false; impact = 0;
        }

        /// The ground below each hover point and what the craft's tilt wants to be.
        public void Step(float dt, HoverControls c, HoverGround ground, float time)
        {
            dt = Mathf.Clamp(dt, 0.0005f, 0.05f);
            var f = Forward; var r = Right;

            // ---- the ground under the four hover points (the craft's own frame) ----
            Vector3 P(float side, float ahead) => position + r * side + f * ahead;
            var pf = P(0, halfLength); var pr = P(0, -halfLength); var pl = P(-halfWidth, 0); var pRight = P(halfWidth, 0);
            float hF = ground.Height(pf.x, pf.z, out bool wF), hR = ground.Height(pr.x, pr.z, out bool wR);
            float hL = ground.Height(pl.x, pl.z, out bool wL), hRt = ground.Height(pRight.x, pRight.z, out bool wRt);
            float hC = ground.Height(position.x, position.z, out onWater);
            groundHeight = hC;
            float gF = (hF - hR) / (2 * halfLength), gR = (hRt - hL) / (2 * halfWidth);        // slope along the craft / across it
            // the cushion rides the mean of the points, nudged to the highest so no corner digs in
            float meanH = (hF + hR + hL + hRt) * 0.25f, maxH = Mathf.Max(Mathf.Max(hF, hR), Mathf.Max(hL, hRt));
            float surface = Mathf.Lerp(meanH, maxH, 0.5f);
            bool water = (wF ? 1 : 0) + (wR ? 1 : 0) + (wL ? 1 : 0) + (wRt ? 1 : 0) >= 2;
            onWater = water;

            // ---- the cushion: a critically damped spring to the hover height; free fall above its range ----
            bob += dt * (water ? 2.1f : 3.3f);
            float wave = water ? Mathf.Sin(bob) * 0.28f + Mathf.Sin(bob * 0.53f + 1.3f) * 0.18f : Mathf.Sin(bob) * 0.06f * Mathf.Clamp01(Speed / 60f);
            float kc = Mathf.Clamp(cushion, 0f, 1.2f);
            float target = surface + hoverHeight * Mathf.Min(1f, kc) - deflatedSink * (1f - kc) + wave * Mathf.Min(1f, kc);
            float gap = position.y - target;
            airborne = gap > cushionRange * Mathf.Max(0.3f, kc) && cushion > 0.5f;
            if (airborne) velocity.y -= gravity * dt;
            else
            {
                float k = 70f * (0.15f + 0.85f * Mathf.Min(1f, kc)), cdamp = 16f;
                velocity.y += (-k * gap - cdamp * velocity.y) * dt;
            }

            // ---- planar motion: thrust, drift, drag, the slope ----
            float vF = Vector3.Dot(velocity, f), vR = Vector3.Dot(velocity, r);
            float lift = Mathf.Clamp01(cushion);                 // a deflated craft has no thrust
            float grip = water ? gripWater : gripLand, drag = water ? dragWater : dragLand + (c.throttle < 0.05f ? dragLandCoast : 0f);
            if (airborne) { grip *= 0.15f; drag *= 0.3f; }
            float aF = 0f;
            if (c.throttle > 0.001f)
            {
                float fall = Mathf.Clamp01(1f - Mathf.Pow(Mathf.Max(0f, vF) / maxSpeed, 2.2f));
                aF += thrust * c.throttle * fall * lift * (airborne ? 0.15f : 1f);
            }
            if (c.brake > 0.001f && lift > 0.5f)
            {
                if (vF > 4f) aF -= brakeDecel * c.brake;                                    // reverse the props: braking
                else aF -= thrust * 0.45f * c.brake * Mathf.Clamp01(1f - Mathf.Max(0f, -vF) / maxReverse);   // then reverse thrust
            }
            // the cushion's slide down the slope, and the thrust it has to climb with
            Vector3 down = -(f * gF + r * gR);                    // downhill, in the horizontal plane
            Vector3 slope = down * (gravity * slopeGravity * (airborne ? 0f : 1f) * (water ? 0f : 1f));
            float dragF = drag * vF + 0.0004f * Mathf.Abs(vF) * vF, dragR = drag * vR + 0.0004f * Mathf.Abs(vR) * vR;
            vF += (aF - dragF) * dt; vR -= dragR * dt;
            vR *= Mathf.Exp(-grip * dt);                          // low sideways grip: the craft drifts through turns
            if (!water && !airborne && c.throttle < 0.01f && c.brake < 0.01f && Mathf.Abs(vF) + Mathf.Abs(vR) < 5f) { vF *= Mathf.Exp(-2.5f * dt); vR *= Mathf.Exp(-2.5f * dt); }   // the cushion settles on land
            Vector3 horizontal = f * vF + r * vR + slope * dt;
            velocity.x = horizontal.x; velocity.z = horizontal.z;

            // ---- steering: the rudders and differential thrust turn the craft, a heavy craft slowly ----
            float speed = Speed;
            float authority = Mathf.Clamp01(0.30f + Mathf.Abs(vF) / 42f) * lift;
            float yawMax = Mathf.Lerp(1.15f, 0.55f, Mathf.Clamp01(speed / maxSpeed));
            float targetYaw = c.steer * yawMax * authority * (vF < -3f ? -1f : 1f);
            yawRate += (targetYaw - yawRate) * (1f - Mathf.Exp(-2.4f * dt));
            yaw += yawRate * dt;

            // ---- integrate ----
            position += velocity * dt;

            // the extent of the terrain: the craft turns back at the edge
            float margin = 90f;
            if (ground.Ready)
            {
                if (position.x < ground.Min.x + margin) { position.x = ground.Min.x + margin; if (velocity.x < 0) velocity.x *= -0.3f; }
                if (position.x > ground.Max.x - margin) { position.x = ground.Max.x - margin; if (velocity.x > 0) velocity.x *= -0.3f; }
                if (position.z < ground.Min.y + margin) { position.z = ground.Min.y + margin; if (velocity.z < 0) velocity.z *= -0.3f; }
                if (position.z > ground.Max.y - margin) { position.z = ground.Max.y - margin; if (velocity.z > 0) velocity.z *= -0.3f; }
            }

            // ---- walls: terrain above the deck stops the craft (a cliff, a ridge) ----
            Collide(ground, gF, gR);

            // never inside the ground
            float floorY = surface - deflatedSink * (1f - Mathf.Min(1f, kc)) + 0.4f;
            if (position.y < floorY) { position.y = floorY; if (velocity.y < 0) velocity.y = 0; }

            // ---- tilt: the ground's normal, with the craft's own lean into the turn and its nose under thrust ----
            var n = (Vector3.up - f * gF - r * gR).normalized;
            float lateral = vR * 0.0035f + yawRate * Mathf.Min(speed, maxSpeed) * -0.0011f;
            float pitchLean = -aF * 0.0012f;
            var wanted = (n + r * lateral + f * pitchLean).normalized;
            if (airborne) wanted = Vector3.Slerp(up, Vector3.up, 0.15f);
            up = Vector3.Slerp(up, wanted, 1f - Mathf.Exp(-5.5f * dt)).normalized;
            fwdSpeed = vF; latSlip = vR;
        }

        void Collide(HoverGround ground, float gF, float gR)
        {
            // eight points on the hull's outline: ground well above the plane the craft rides on (a cliff, a step, a ridge) at one pushes the craft out
            var f = Forward; var r = Right;
            float hx = halfWidth + 6f, hz = halfLength + 8f;
            float inflated = position.y + deflatedSink * (1f - Mathf.Clamp01(cushion));      // the origin's height as if the skirt were full
            for (int i = 0; i < 8; i++)
            {
                float a = i * Mathf.PI * 0.25f, sa = Mathf.Sin(a), ca = Mathf.Cos(a);
                var o = r * (sa * hx) + f * (ca * hz);
                var p = position + o;
                float h = ground.Height(p.x, p.z, out bool w);
                float plane = inflated + gF * (ca * hz) + gR * (sa * hx);
                float over = h - (plane + bodyHeight * 0.75f);
                if (w || over <= 0f) continue;
                var outward = new Vector3(o.x, 0, o.z); if (outward.sqrMagnitude < 1e-4f) continue; outward.Normalize();
                float into = Vector3.Dot(velocity, outward);
                position -= outward * Mathf.Min(2.5f, over * 0.25f + 0.4f);
                if (into > 0)
                {
                    var vh = new Vector3(velocity.x, 0, velocity.z);
                    vh -= outward * into * 1.35f;               // the wall takes the speed into it, with a little bounce
                    velocity.x = vh.x; velocity.z = vh.z;
                    impact = Mathf.Max(impact, into); wallHits++; maxImpact = Mathf.Max(maxImpact, into);
                }
            }
        }
    }
}
