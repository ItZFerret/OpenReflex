// A scripted "real player" for the MotoSkills lessons 2-6 (w-chevfx), used by OpenTutorialProbe only (diagnostics, off in a normal start).
// It rides the real player's keyboard path (virtual keyboard -> the game's key maps -> the core's lesson checks) and decides each frame from what a
// player sees: the bike (position, heading, speed: the presented state), the ground under it (the Unity terrain) and the lesson's furniture, which
// the level file places: the TUT0<n>_Start / _Finish / _Boundary trigger volumes (TutorialTriggerRSData: a box with its centre, three half-axes), the
// flares / chevron walls / trajectory arrows. Nothing here changes the game's rules; it only presses keys:
//   W throttle, S brake, A / D steer, arrow keys the Rider Reflex lean (Up forward, Down back, Left, Right), Space the trick modifier.
// Tuning knobs (no rebuild): REFLEX_TUTORIAL_PILOT="key=value,key=value" (see Param).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class TutorialPilot
    {
        /// A lesson trigger volume: centre, half-axes (the scene matrix rows, scaled), kind Start / Finish / Boundary.
        public sealed class Trig
        {
            public string name, kind; public Vector3 c, ax, ay, az;
            public bool Contains(Vector3 p)
            {
                var d = p - c;
                foreach (var a in new[] { ax, ay, az })
                {
                    float l2 = a.sqrMagnitude; if (l2 < 1e-6f) continue;
                    if (Mathf.Abs(Vector3.Dot(d, a) / l2) > 1f) return false;
                }
                return true;
            }
            /// Position in the box's own frame, each axis -1..1 inside.
            public Vector3 Local(Vector3 p)
            {
                var d = p - c; float f(Vector3 a) { float l2 = a.sqrMagnitude; return l2 < 1e-6f ? 0 : Vector3.Dot(d, a) / l2; }
                return new Vector3(f(ax), f(ay), f(az));
            }
        }

        public readonly int lesson;                       // 0..5
        public readonly List<Trig> trigs = new List<Trig>();
        public Trig start, finish, boundary;
        public Vector3 startPos; public Vector2 startDir;   // the lesson's start (the Z row of its start trigger is the rider's heading)
        readonly OriginalWorldRuntime rt;
        Terrain terrain;
        readonly Dictionary<string, float> param = new Dictionary<string, float>(StringComparer.OrdinalIgnoreCase);

        public TutorialPilot(OriginalWorldRuntime runtime, ReflexLevelContent content, int lesson)
        {
            rt = runtime; this.lesson = lesson;
            string prefix = $"TUT0{lesson + 1}_";
            foreach (var e in content.entities)
            {
                if (e.Type != "TutorialTriggerRSData") continue;
                string name = e.String("Name"); if (name == null || !name.StartsWith(prefix, StringComparison.Ordinal)) continue;
                var m = e.Matrix; if (m == null || m.Length < 16) continue;
                var t = new Trig
                {
                    name = name, kind = name.Substring(prefix.Length).Split('_')[0], c = new Vector3(m[12], m[13], m[14]),
                    ax = new Vector3(m[0], m[1], m[2]), ay = new Vector3(m[4], m[5], m[6]), az = new Vector3(m[8], m[9], m[10])
                };
                trigs.Add(t);
                if (t.kind == "Start") start = t; else if (t.kind == "Finish") finish = t; else if (t.kind == "Boundary") boundary = t;
            }
            if (start != null) { startPos = start.c; var z = new Vector2(start.az.x, start.az.z); startDir = z.sqrMagnitude > 1e-6f ? z.normalized : Vector2.up; }
            foreach (var kv in (Environment.GetEnvironmentVariable("REFLEX_TUTORIAL_PILOT") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
            {
                var p = kv.Split('=');
                if (p.Length == 2 && float.TryParse(p[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float v)) param[p[0].Trim()] = v;
            }
        }

        public float Param(string key, float def) => param.TryGetValue(key, out float v) ? v : def;

        // ------------------------------------------------------------------------------------------------------------- the bike
        public struct Pose { public Vector3 pos, vel; public Vector2 fwd; public float speed, ground, height; public bool valid; public float lean7, lean6; }

        public Pose Read()
        {
            var p = new Pose();
            if (!rt) return p;
            var b = rt.Bike;
            if (b.renderPosition == null || b.velocity == null) return p;
            Vector3 P(int i) => new Vector3(b.renderPosition[i * 4], b.renderPosition[i * 4 + 1], b.renderPosition[i * 4 + 2]);
            p.pos = P(0); p.vel = new Vector3(b.velocity[0], b.velocity[1], b.velocity[2]); p.speed = b.speed;
            var f = P(2) - P(3); var f2 = new Vector2(f.x, f.z);
            if (f2.sqrMagnitude < 1e-4f) f2 = new Vector2(p.vel.x, p.vel.z);
            p.fwd = f2.sqrMagnitude > 1e-6f ? f2.normalized : startDir;
            if (b.inputs != null && b.inputs.Length >= 8) { p.lean7 = b.inputs[7]; p.lean6 = b.inputs[6]; }
            if (!terrain) terrain = Terrain.activeTerrain;
            p.ground = Ground(p.pos); p.height = p.pos.y - p.ground;
            p.valid = true;
            return p;
        }

        public float Ground(Vector3 p) => terrain ? terrain.SampleHeight(new Vector3(p.x, 0, p.z)) + terrain.transform.position.y : p.y;

        // ------------------------------------------------------------------------------------------------------------- the keys
        public readonly HashSet<Key> keys = new HashSet<Key>();
        public void Press(Key k) => keys.Add(k);
        public string KeyString() => string.Join("+", keys.Select(k => k.ToString()));

        float steerPhase;
        /// A / D toward `desired` (a direction in x, z): all in at a large error, duty-cycled near it (the keyboard steer is on / off).
        public void SteerTo(Vector2 desired, Vector2 fwd)
        {
            float ang = Vector2.SignedAngle(fwd, desired);          // + = to the left of the bike in the x-z plane seen from above (y up, z away)
            float deadband = Param("dead", 2.5f), full = Param("full", 14f);
            float a = Mathf.Abs(ang);
            if (a < deadband) { steerPhase = 0; return; }
            steerPhase += Mathf.Clamp01((a - deadband) / (full - deadband));
            if (steerPhase >= 1f) { steerPhase -= 1f; Press(ang > 0 ? Key.A : Key.D); }
            else if (a >= full) Press(ang > 0 ? Key.A : Key.D);
        }

        /// W below the speed, S well above it.
        public void Speed(float speed, float target)
        {
            if (speed < target) Press(Key.W);
            else if (speed > target + Param("brake", 4f)) Press(Key.S);
        }

        /// The direction toward the point `ahead` units along the line from `origin` in direction `dir`, from the bike.
        public Vector2 LineTarget(Vector3 pos, Vector2 origin, Vector2 dir, float ahead)
        {
            var rel = new Vector2(pos.x, pos.z) - origin;
            float along = Vector2.Dot(rel, dir);
            var target = origin + dir * (along + ahead);
            var d = target - new Vector2(pos.x, pos.z);
            return d.sqrMagnitude > 1e-6f ? d.normalized : dir;
        }
    }
}
