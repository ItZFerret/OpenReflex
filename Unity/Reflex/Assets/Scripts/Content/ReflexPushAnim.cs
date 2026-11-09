// The push-bike walk of the front end's 3D menu (OpenFrontEnd/OpenMenuWorld): the game's own "HiResCrewChiefLoseWalk_PushBike" clips
// (AnimationSets.xml: SC_CC_Anim_PushBike2 / SC_CC_Anim_PushBike3, HasVehicleAnim, so each has a "<clip>_BIKE" vehicle clip the scenario code
// plays on the vehicle, anim_scenario_intro.c 0x00941e88 "%s_BIKE"), read from the player's install at run time (every level database's Shared
// package carries them; nothing is kept in the project) and played on the player's own rider and vehicle. Not part of the original front end
// (its garage draws the rider in RiderUIAnimations' idle only): a presentation of the port's menu.
//
// The crew chief's skeleton is not the rider's (37 nodes, CrewChief_* names, thighs named Bone01 / Bone02, a SpineC where the rider has
// MXRider_Chest, and its own bone frames: SpineB / the shins are turned 180 degrees about the bone against the rider's). Both binds stand in the
// same Biped figure pose (root 3.1 ft, the same leg and arm positions), so the walk is retargeted bone by bone in world space:
//   rider world(t) = crew chief world(t) * inverse(crew chief bind) * rider bind'
// with rider bind' = the rider's bind turned so each bone's axis (+x of its frame) lies along the crew chief's bind bone (the rider's bind has the
// forearms bent forward, the crew chief's are straight). The rider keeps his own bone lengths (bind translations); his root follows the crew
// chief's (same pelvis height); the nodes the walk has no bone for (fingers, flaps, the gear nodes) keep the garage idle clip's local turn.
// The crew chief's bind comes from the level's CrewChief model (NAT_Concord's CrewChief_HIGH_WW: same node names as the clip).
//
// The vehicle clip (version 11, not the packed layout: AnimAsset loader 0082bf10, core/game/build_anim.c): per node a track of (time, x, y, z)
// translation keys and a track of (time, int16 x y z w / 32767) rotation keys; OBJ_Frame (the hierarchy's root: the whole vehicle) rolls 19 ft
// forward along +z in 6.33 s beside the walker (he is at its left, his right hand on the left grip). OBJ_UPR_Forks' track is not used (its keys
// are in a frame the port's fork node does not share; the steer is a few degrees).
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.Content
{
    internal sealed class ReflexPushAnim
    {
        public readonly string name;
        public float Length => body.length;
        readonly ReflexRiderAnim body;
        readonly string[] ccNames; readonly int[] ccParents; readonly Quaternion[] ccBind;
        // the vehicle clip's OBJ_Frame tracks
        float[] posT, rotT; Vector3[] posK; Quaternion[] rotK;

        ReflexPushAnim(string name, ReflexRiderAnim body, Rfx.Model chief)
        {
            this.name = name; this.body = body;
            int n = chief.nodes.Length;
            ccNames = new string[n]; ccParents = new int[n]; ccBind = new Quaternion[n];
            for (int i = 0; i < n; i++)
            {
                ccNames[i] = chief.nodes[i].name; ccParents[i] = chief.nodes[i].parent;
                var w = chief.nodes[i].world;
                ccBind[i] = Rot(w.GetColumn(2), w.GetColumn(1));
            }
        }

        static readonly Dictionary<string, ReflexPushAnim> cache = new Dictionary<string, ReflexPushAnim>(StringComparer.OrdinalIgnoreCase);
        /// The databases tried for the clips and the crew chief (any level's Shared package has the clips; NAT_Concord is always installed).
        public static readonly string[] Databases = { "NAT_Concord", "NAT_Bedford", "FR_Alpine", "SX_Kingston", "FMX_Dover" };
        public static string Error { get; private set; }

        /// The walk (SC_CC_Anim_PushBike2 / 3) with its vehicle clip, or null when the install lacks either (Error says why). Thread-safe, cached.
        public static ReflexPushAnim Load(string clip = "SC_CC_Anim_PushBike2")
        {
            lock (cache) if (cache.TryGetValue(clip, out var hit)) return hit;
            ReflexPushAnim r = null;
            try
            {
                var install = VehicleCatalog.Shared.install;
                foreach (var db in Databases)
                {
                    if (install.Find(db, "Shared", clip, "anim") == null || install.Find(db, "Shared", clip + "_BIKE", "anim") == null) continue;
                    var body = ReflexRiderAnim.Load(db, "Shared", clip);
                    if (body == null) continue;
                    var chiefAsset = install.Find(db, db + "Base", "CrewChief_HIGH_WW", "model") ?? install.Find(db, db + "Base", "CrewChief_MID_WW", "model");
                    if (chiefAsset == null) continue;
                    var chief = Rfx.ParseModel(install.Read(chiefAsset));
                    if (chief.NodeIndex("CrewChief_Root") < 0) continue;
                    r = new ReflexPushAnim(clip, body, chief);
                    r.ReadVehicle(install.Read(install.Find(db, "Shared", clip + "_BIKE", "anim")));
                    break;
                }
                Error = r == null ? "the install has no " + clip + " with its vehicle clip and a crew chief model" : null;
            }
            catch (Exception e) { Error = clip + ": " + e.Message; r = null; }
            lock (cache) cache[clip] = r;
            return r;
        }

        void ReadVehicle(byte[] b)
        {
            Rfx.Check(b.Length > 64 && BitConverter.ToInt32(b, 0) == -11 && BitConverter.ToUInt32(b, 4) != 0x12345678u, "vehicle clip version");
            int names = BitConverter.ToInt32(b, 12), p = 16;
            Rfx.Check(names > 0 && names < 64, "vehicle clip nodes");
            var nodes = new string[names];
            for (int i = 0; i < names; i++)
            {
                int n = BitConverter.ToInt32(b, p); p += 4;
                Rfx.Check(n > 0 && p + n <= b.Length, "vehicle clip node name");
                nodes[i] = System.Text.Encoding.ASCII.GetString(b, p, n - 1); p += n;
            }
            p += 4 + 8;                                            // the length (+0x4c), two words the loader drops
            int nt = BitConverter.ToInt32(b, p), nr = BitConverter.ToInt32(b, p + 4); p += 8;
            int floatTracks = BitConverter.ToInt32(b, p), keyBones = BitConverter.ToInt32(b, p + 4), pairs = BitConverter.ToInt32(b, p + 8); p += 12;
            Rfx.Check(floatTracks == 0 && keyBones == 0 && pairs == 0, "vehicle clip extra tracks");
            p += 12;                                               // two words, the key block size (version > 10)
            int frame = Array.IndexOf(nodes, "OBJ_Frame");
            for (int k = 0; k < nt; k++)
            {
                int count = BitConverter.ToInt32(b, p), node = BitConverter.ToInt32(b, p + 4); p += 8;
                Rfx.Check(count > 0 && p + 16 * count <= b.Length, "vehicle clip translation keys");
                if (node == frame)
                {
                    posT = new float[count]; posK = new Vector3[count];
                    for (int i = 0; i < count; i++) { int o = p + 16 * i; posT[i] = BitConverter.ToSingle(b, o); posK[i] = new Vector3(BitConverter.ToSingle(b, o + 4), BitConverter.ToSingle(b, o + 8), BitConverter.ToSingle(b, o + 12)); }
                }
                p += 16 * count;
            }
            const float s = 3.0518509e-05f;
            for (int k = 0; k < nr; k++)
            {
                int count = BitConverter.ToInt32(b, p), node = BitConverter.ToInt32(b, p + 4); p += 8;
                Rfx.Check(count > 0 && p + 12 * count <= b.Length, "vehicle clip rotation keys");
                if (node == frame)
                {
                    rotT = new float[count]; rotK = new Quaternion[count];
                    for (int i = 0; i < count; i++)
                    {
                        int o = p + 12 * i; rotT[i] = BitConverter.ToSingle(b, o);
                        var q = new Quaternion(BitConverter.ToInt16(b, o + 4) * s, BitConverter.ToInt16(b, o + 6) * s, BitConverter.ToInt16(b, o + 8) * s, BitConverter.ToInt16(b, o + 10) * s);
                        rotK[i] = Norm(q);
                    }
                }
                p += 12 * count;
            }
            Rfx.Check(posT != null, "vehicle clip: no OBJ_Frame track");
        }

        static Quaternion Norm(Quaternion q)
        {
            float m = Mathf.Sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            return m > 1e-6f ? new Quaternion(q.x / m, q.y / m, q.z / m, q.w / m) : Quaternion.identity;
        }
        static int Key(float[] times, float t, out float frac)
        {
            frac = 0;
            if (t <= times[0]) return 0;
            int last = times.Length - 1;
            if (t >= times[last]) return last;
            int lo = 0, hi = last;
            while (hi - lo > 1) { int mid = (lo + hi) >> 1; if (times[mid] <= t) lo = mid; else hi = mid; }
            float span = times[lo + 1] - times[lo];
            frac = span > 1e-6f ? (t - times[lo]) / span : 0f;
            return lo;
        }

        /// OBJ_Frame (the vehicle) at the time in the clip's space (Unity column matrix): the translation track only. The rotation track is read
        /// but not applied: as decoded it is the identity with isolated keys of 180 degrees about y (0, 1, 0, 0) and 22.5 degree rolls (keys 25,
        /// 43-46, 47-56, 187 of PushBike2_BIKE), which spun and tipped the bike for a few frames mid-walk (the lead's "glitching around"; the
        /// per-frame motion log showed 45-degree yaw steps); the pushed bike stays upright and on its heading.
        public Matrix4x4 Frame(float t)
        {
            int i = Key(posT, t, out float f);
            var pos = i + 1 < posK.Length ? Vector3.Lerp(posK[i], posK[i + 1], f) : posK[i];
            return Matrix4x4.Translate(pos);
        }
        /// The decoded rotation keys of OBJ_Frame (diagnostics).
        public int RotationKeys => rotK?.Length ?? 0;

        static Quaternion Rot(Vector3 forward, Vector3 up)
        {
            if (forward.sqrMagnitude < 1e-10f || up.sqrMagnitude < 1e-10f) return Quaternion.identity;
            return Quaternion.LookRotation(forward, up);
        }
        static Quaternion RowRot(float[] m, int o) => Rot(new Vector3(m[o + 8], m[o + 9], m[o + 10]), new Vector3(m[o + 4], m[o + 5], m[o + 6]));
        static Vector3 RowPos(float[] m, int o) => new Vector3(m[o + 12], m[o + 13], m[o + 14]);

        // the crew chief bone each rider bone follows
        static readonly Dictionary<string, string> RiderOfChief = new Dictionary<string, string>
        {
            { "CrewChief_Root", "MXRider_Root" }, { "CrewChief_SpineA", "MXRider_SpineA" }, { "CrewChief_SpineB", "MXRider_SpineB" },
            { "CrewChief_SpineC", "MXRider_Chest" }, { "CrewChief_Neck", "MXRider_Neck" }, { "CrewChief_Head", "MXRider_Head" },
            { "CrewChief_LCollar", "MXRider_LCollar" }, { "CrewChief_LArmA", "MXRider_LArmA" }, { "CrewChief_LArmB", "MXRider_LArmB" }, { "CrewChief_LWrist", "MXRider_LWrist" },
            { "CrewChief_RCollar", "MXRider_RCollar" }, { "CrewChief_RArmA", "MXRider_RArmA" }, { "CrewChief_RArmB", "MXRider_RArmB" }, { "CrewChief_RWrist", "MXRider_RWrist" },
            { "Bone01", "MXRider_LLegA" }, { "CrewChief_LLegB", "MXRider_LLegB" }, { "CrewChief_LFoot", "MXRider_LFoot" },
            { "Bone02", "MXRider_RLegA" }, { "CrewChief_RLegB", "MXRider_RLegB" }, { "CrewChief_RFoot", "MXRider_RFoot" },
        };

        /// One rider skeleton bound to the walk: the bone map, the corrected binds, the bone lengths and the idle clip's turns of the other nodes.
        public sealed class Binding
        {
            public int count; public int[] parents, chief; public Quaternion[] bindFix, idleLocal; public Vector3[] lengths;
            internal float[] ccWorld; internal Quaternion[] w; internal Vector3[] p;
            internal Quaternion[] qa, qb, qm, ql; internal Vector3[] pm;   // (the blend's scratch: no allocation per frame)
            internal bool[] over; internal Quaternion[] qover;
            public int mapped;
            /// The leg bones (thigh, shin, foot) of the left and the right leg: indices into the rider's skeleton (-1: missing, no foot planting).
            public int legLA = -1, legLB = -1, footL = -1, legRA = -1, legRB = -1, footR = -1;
            /// The ankles' velocity (ft / s) and the pelvis's at the walk's last frame (HandOver): the steps of the settle start from them.
            public Vector3 velL, velR, velRoot;
            public bool CanPlant => legLA >= 0 && legLB >= 0 && footL >= 0 && legRA >= 0 && legRB >= 0 && footR >= 0;
        }

        /// The rider's skeleton (OriginalRider: names, parents, bind world matrices) bound to the walk; idle: the clip whose start gives the
        /// unmapped nodes their turn (null: the bind's).
        public Binding Bind(string[] riderNames, int[] parents, float[] bindWorld, ReflexRiderAnim idle)
        {
            int n = riderNames.Length;
            var b = new Binding { count = n, parents = parents, chief = new int[n], bindFix = new Quaternion[n], idleLocal = new Quaternion[n], lengths = new Vector3[n],
                                  ccWorld = new float[ccNames.Length * 16], w = new Quaternion[n], p = new Vector3[n] };
            var bindRot = new Quaternion[n]; var bindPos = new Vector3[n];
            for (int i = 0; i < n; i++) { bindRot[i] = RowRot(bindWorld, i * 16); bindPos[i] = RowPos(bindWorld, i * 16); }
            for (int i = 0; i < n; i++)
            {
                int pr = parents[i];
                b.lengths[i] = pr < 0 ? bindPos[i] : Quaternion.Inverse(bindRot[pr]) * (bindPos[i] - bindPos[pr]);
                b.idleLocal[i] = pr < 0 ? bindRot[i] : Quaternion.Inverse(bindRot[pr]) * bindRot[i];
                b.chief[i] = -1;
            }
            if (idle != null)
            {
                var lr = new Quaternion[n];
                idle.Locals(0f, riderNames, lr, null);
                for (int i = 0; i < n; i++) if (idle.Has(riderNames[i])) b.idleLocal[i] = lr[i];
            }
            b.legLA = Array.IndexOf(riderNames, "MXRider_LLegA"); b.legLB = Array.IndexOf(riderNames, "MXRider_LLegB"); b.footL = Array.IndexOf(riderNames, "MXRider_LFoot");
            b.legRA = Array.IndexOf(riderNames, "MXRider_RLegA"); b.legRB = Array.IndexOf(riderNames, "MXRider_RLegB"); b.footR = Array.IndexOf(riderNames, "MXRider_RFoot");
            foreach (var kv in RiderOfChief)
            {
                int j = Array.IndexOf(ccNames, kv.Key), i = Array.IndexOf(riderNames, kv.Value);
                if (j < 0 || i < 0) continue;
                b.chief[i] = j; b.mapped++;
                // the rider's bind bone laid along the crew chief's (+x is the bone axis of both Biped rigs)
                var fix = Quaternion.FromToRotation(bindRot[i] * Vector3.right, ccBind[j] * Vector3.right);
                b.bindFix[i] = Quaternion.Inverse(ccBind[j]) * (fix * bindRot[i]);
            }
            return b;
        }

        static readonly float[] IdentityRow = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        /// The rider's bone world matrices (row-major, the game's layout) at the time, the clip's space placed by place (Unity column matrix),
        /// lifted by dy (world feet).
        public void Pose(Binding b, float t, Matrix4x4 place, float dy, float[] outWorld)
        {
            body.World(Mathf.Clamp(t, 0f, body.length), ccNames, ccParents, IdentityRow, b.ccWorld);
            var pr = place.rotation;
            for (int i = 0; i < b.count; i++)
            {
                int par = b.parents[i], j = b.chief[i];
                if (j >= 0) b.w[i] = RowRot(b.ccWorld, j * 16) * b.bindFix[i];
                else b.w[i] = (par < 0 ? Quaternion.identity : b.w[par]) * b.idleLocal[i];
                if (par < 0) b.p[i] = j >= 0 ? RowPos(b.ccWorld, j * 16) : b.lengths[i];
                else b.p[i] = b.p[par] + b.w[par] * b.lengths[i];
            }
            for (int i = 0; i < b.count; i++)
            {
                var q = pr * b.w[i]; var pos = place.MultiplyPoint3x4(b.p[i]) + new Vector3(0, dy, 0);
                Write(outWorld, i * 16, q, pos);
            }
        }

        static void Write(float[] m, int o, Quaternion q, Vector3 pos)
        {
            Vector3 r = q * Vector3.right, u = q * Vector3.up, f = q * Vector3.forward;
            m[o] = r.x; m[o + 1] = r.y; m[o + 2] = r.z; m[o + 3] = 0;
            m[o + 4] = u.x; m[o + 5] = u.y; m[o + 6] = u.z; m[o + 7] = 0;
            m[o + 8] = f.x; m[o + 9] = f.y; m[o + 10] = f.z; m[o + 11] = 0;
            m[o + 12] = pos.x; m[o + 13] = pos.y; m[o + 14] = pos.z; m[o + 15] = 1;
        }

        /// Two poses of the rider blended (w 0: a, 1: b): the root's place and turn, every other bone's local turn, the rider's own bone lengths.
        public static void Blend(Binding bd, float[] a, float[] b, float w, float[] outWorld)
        {
            int n = bd.count;
            if (bd.qa == null) { bd.qa = new Quaternion[n]; bd.qb = new Quaternion[n]; bd.qm = new Quaternion[n]; bd.pm = new Vector3[n]; }
            var qa = bd.qa; var qb = bd.qb; var q = bd.qm; var p = bd.pm;
            for (int i = 0; i < n; i++) { qa[i] = RowRot(a, i * 16); qb[i] = RowRot(b, i * 16); }
            for (int i = 0; i < n; i++)
            {
                int par = bd.parents[i];
                if (par < 0)
                {
                    q[i] = Quaternion.Slerp(qa[i], qb[i], w);
                    p[i] = Vector3.Lerp(RowPos(a, i * 16), RowPos(b, i * 16), w);
                }
                else
                {
                    var la = Quaternion.Inverse(qa[par]) * qa[i]; var lb = Quaternion.Inverse(qb[par]) * qb[i];
                    q[i] = q[par] * Quaternion.Slerp(la, lb, w);
                    p[i] = p[par] + q[par] * bd.lengths[i];
                }
                Write(outWorld, i * 16, q[i], p[i]);
            }
        }


        /// The ankles' and the pelvis's velocity at the walk's last frame, from the poses at its end and dt seconds before (the settle's steps start with them).
        public static void HandOver(Binding bd, float[] atEnd, float[] before, float dt)
        {
            if (!bd.CanPlant || dt <= 1e-6f) return;
            Vector3 V(int i) => (RowPos(atEnd, i * 16) - RowPos(before, i * 16)) / dt;
            bd.velL = V(bd.footL); bd.velR = V(bd.footR);
            int root = Array.FindIndex(bd.parents, x => x < 0);
            bd.velRoot = root >= 0 ? V(root) : Vector3.zero;
        }

        /// The settle's steps (fractions of its length): the foot that ends the clip in the air lands first (S1), the planted one steps to its place in
        /// the idle (S2), the first one closes up to its own (S3, only when S1 could not reach it).
        public static Vector2 Step1 = new Vector2(0f, 0.30f), Step2 = new Vector2(0.33f, 0.68f), Step3 = new Vector2(0.70f, 1f);
        /// The furthest apart the feet are planted (ft): the legs are about 3 ft long under a pelvis 2.8 ft high.
        public static float MaxSeparation = 1.8f;
        /// How far a leg may be stretched before the pelvis is lowered (the share of its length).
        public static float ReachFraction = 0.99f;
        /// The stepping foot's lift over the floor at the middle of a step (ft).
        public static float StepLift = 0.28f;
        /// The arch of the first foot's landing over its descent (ft).
        public static float LandingArch = 0.10f;

        static float Sq(float x) => x * x;
        static float Smooth(float x) { x = Mathf.Clamp01(x); return x * x * (3f - 2f * x); }
        static float Frac(float u, Vector2 win) => Mathf.Clamp01((u - win.x) / Mathf.Max(1e-4f, win.y - win.x));
        // a cubic Hermite from a to b over seconds, leaving a at v0 and arriving at rest
        static Vector3 Hermite(Vector3 a, Vector3 v0, Vector3 b, float s, float seconds)
        {
            float s2 = s * s, s3 = s2 * s;
            return (2 * s3 - 3 * s2 + 1) * a + (s3 - 2 * s2 + s) * seconds * v0 + (-2 * s3 + 3 * s2) * b;
        }

        // One foot's walk through the settle: up to two steps (from, to, window, start velocity, lift) and the share of the turn each completes.
        struct Foot
        {
            public Vector3 start; public int steps;
            public Vector3 f0, t0, v0; public Vector2 w0; public float lift0, rot0;
            public Vector3 f1, t1; public Vector2 w1; public float rot1;
            public float landY;   // the planted height of the ankle once it has stepped
            public Vector3 At(float u, float seconds, out float rot)
            {
                rot = 0f;
                if (steps >= 1 && u > w0.x)
                {
                    float raw = Frac(u, w0);
                    if (steps >= 2 && u >= w1.x - 1e-6f)
                    {
                        if (u >= w1.y) { rot = rot1; return t1; }
                        raw = Frac(u, w1);
                        var pos = Hermite(f1, Vector3.zero, t1, raw, (w1.y - w1.x) * seconds);
                        pos.y = Mathf.Lerp(f1.y, t1.y, raw) + StepLift * Sq(Mathf.Sin(Mathf.PI * raw));   // (sin squared: no jerk as the foot leaves or meets the floor)
                        rot = Mathf.Lerp(rot0, rot1, Smooth(raw)); return pos;
                    }
                    if (u >= w0.y) { rot = rot0; return t0; }
                    var p0 = Hermite(f0, v0, t0, raw, (w0.y - w0.x) * seconds);
                    p0.y = Mathf.Lerp(f0.y, t0.y, raw) + lift0 * Sq(Mathf.Sin(Mathf.PI * raw));   // (airborne until it lands: no gliding over the floor on the way in)
                    rot = Mathf.Lerp(0f, rot0, Smooth(raw)); return p0;
                }
                return start;
            }
        }

        /// The settle from the walk's last pose a into the idle b, FOOT-PLANTED (u 0..1 over seconds). The clip ends mid-stride, the idle stance is
        /// 2 - 3 ft along and 2 ft out from where the walk stops, turned 60 degrees, so the old blend (every bone's local turn and the pelvis's place
        /// lerped) skated the ankles 2.3 and 3.5 ft in 0.9 s. Now: the upper body is the same local blend (smooth over the settle); the FEET are
        /// STEPPED, each ankle exactly where it was put until its step: the foot the clip ends with in the air lands first (it leaves with the
        /// clip's own velocity), the planted one steps to its place in the idle, the first closes up; every planted pair no further apart than
        /// MaxSeparation. A two-bone IK puts the legs on the ankles (the knees bend as the FK pose bends them), the pelvis rides the middle of the
        /// two ankles plus the blend of how it stood over them, lowered when a leg would overreach (the knees bend). At u = 1 the pose is b exactly.
        public static void BlendPlanted(Binding bd, float[] a, float[] b, float u, float seconds, float[] outWorld)
        {
            if (!bd.CanPlant) { Blend(bd, a, b, Smooth(u), outWorld); return; }
            int n = bd.count;
            if (bd.qa == null) { bd.qa = new Quaternion[n]; bd.qb = new Quaternion[n]; bd.qm = new Quaternion[n]; bd.pm = new Vector3[n]; }
            if (bd.ql == null || bd.ql.Length != n) { bd.ql = new Quaternion[n]; bd.over = new bool[n]; bd.qover = new Quaternion[n]; }
            var qa = bd.qa; var qb = bd.qb; var q = bd.qm; var p = bd.pm; var ql = bd.ql;
            for (int i = 0; i < n; i++) { qa[i] = RowRot(a, i * 16); qb[i] = RowRot(b, i * 16); }
            float w = Smooth(u);
            int root = Array.FindIndex(bd.parents, x => x < 0);
            Vector3 aL = RowPos(a, bd.footL * 16), bL = RowPos(b, bd.footL * 16), aR = RowPos(a, bd.footR * 16), bR = RowPos(b, bd.footR * 16);
            // the plan: S is the foot in the air at the clip's end (the faster one), P the planted one
            bool sIsR = new Vector2(bd.velR.x, bd.velR.z).sqrMagnitude >= new Vector2(bd.velL.x, bd.velL.z).sqrMagnitude;
            Vector3 aS = sIsR ? aR : aL, bS = sIsR ? bR : bL, aP = sIsR ? aL : aR, bP = sIsR ? bL : bR, vS = sIsR ? bd.velR : bd.velL;
            var toS = new Vector3(bS.x - aP.x, 0, bS.z - aP.z); float dS = toS.magnitude;
            bool direct = dS <= MaxSeparation;
            Vector3 p1 = direct ? bS : new Vector3(aP.x + toS.x / dS * MaxSeparation, bS.y, aP.z + toS.z / dS * MaxSeparation);
            var fs = new Foot { start = aS, steps = direct ? 1 : 2 };
            fs.f0 = aS; fs.t0 = p1; fs.v0 = vS; fs.w0 = Step1; fs.lift0 = LandingArch; fs.rot0 = direct ? 1f : 0.7f;
            fs.f1 = p1; fs.t1 = bS; fs.w1 = Step3; fs.rot1 = 1f;
            var fp = new Foot { start = aP, steps = 1 };
            fp.f0 = aP; fp.t0 = bP; fp.v0 = Vector3.zero; fp.w0 = direct ? new Vector2(Step2.x, 1f) : Step2; fp.lift0 = StepLift; fp.rot0 = 1f;
            float rotS, rotP;
            Vector3 fS = fs.At(u, seconds, out rotS), fP = fp.At(u, seconds, out rotP);
            Vector3 fL = sIsR ? fP : fS, fR = sIsR ? fS : fP;
            float rotL = sIsR ? rotP : rotS, rotR = sIsR ? rotS : rotP;
            // the pelvis over them
            Vector3 rootA = RowPos(a, root * 16), rootB = RowPos(b, root * 16);
            Vector3 relA = rootA - 0.5f * (aL + aR), relB = rootB - 0.5f * (bL + bR);
            Vector3 centre = 0.5f * (fL + fR);
            var rootPos = new Vector3(centre.x + Mathf.Lerp(relA.x, relB.x, w), Mathf.Lerp(rootA.y, rootB.y, w), centre.z + Mathf.Lerp(relA.z, relB.z, w));
            float lowered = 0f;
            for (int pass = 0; pass < 2; pass++)
            {
                // the local blend with that pelvis
                for (int i = 0; i < n; i++)
                {
                    int par = bd.parents[i]; bd.over[i] = false;
                    if (par < 0) { q[i] = Quaternion.Slerp(qa[i], qb[i], w); p[i] = rootPos; ql[i] = q[i]; }
                    else
                    {
                        var la = Quaternion.Inverse(qa[par]) * qa[i]; var lb = Quaternion.Inverse(qb[par]) * qb[i];
                        ql[i] = Quaternion.Slerp(la, lb, w);
                        q[i] = q[par] * ql[i];
                        p[i] = p[par] + q[par] * bd.lengths[i];
                    }
                }
                if (pass == 1) break;
                // a leg that would overreach lowers the pelvis (the knees bend), never more than half a foot
                float need = Mathf.Max(Overreach(bd, bd.legLA, bd.legLB, bd.footL, fL), Overreach(bd, bd.legRA, bd.legRB, bd.footR, fR));
                if (need <= 1e-4f) break;
                lowered = Mathf.Min(need, 0.4f); rootPos.y -= lowered;
            }
            // the legs on the feet
            Leg(bd, bd.legLA, bd.legLB, bd.footL, fL, Quaternion.Slerp(qa[bd.footL], qb[bd.footL], Smooth(rotL)));
            Leg(bd, bd.legRA, bd.legRB, bd.footR, fR, Quaternion.Slerp(qa[bd.footR], qb[bd.footR], Smooth(rotR)));
            for (int i = 0; i < n; i++)
            {
                int par = bd.parents[i];
                if (par >= 0)
                {
                    q[i] = bd.over[i] ? bd.qover[i] : q[par] * ql[i];
                    p[i] = p[par] + q[par] * bd.lengths[i];
                }
                Write(outWorld, i * 16, q[i], p[i]);
            }
        }

        // How much lower the hip must be for the ankle target to be within ReachFraction of the leg's length (0: it is).
        static float Overreach(Binding bd, int A, int B, int F, Vector3 target)
        {
            float reach = ReachFraction * (bd.lengths[B].magnitude + bd.lengths[F].magnitude);
            var hip = bd.pm[A];
            float h2 = (hip.x - target.x) * (hip.x - target.x) + (hip.z - target.z) * (hip.z - target.z);
            float v = hip.y - target.y;
            if (h2 >= reach * reach) return 0f;
            return Mathf.Max(0f, v - Mathf.Sqrt(reach * reach - h2));
        }

        // Two-bone IK of one leg (thigh A, shin B, foot F) onto the ankle target, the knee bending the way the FK pose bends it. Writes the new world
        // turns of A, B and F into qover / over (the caller recomputes everything below them).
        static void Leg(Binding bd, int A, int B, int F, Vector3 target, Quaternion footWorld)
        {
            var q = bd.qm; var p = bd.pm;
            Vector3 hip = p[A], knee0 = p[B];
            float lu = bd.lengths[B].magnitude, ll = bd.lengths[F].magnitude;
            var to = target - hip; float d = to.magnitude;
            if (d < 1e-5f) return;
            var axis = to / d;
            float dc = Mathf.Clamp(d, Mathf.Abs(lu - ll) + 1e-3f, lu + ll - 1e-3f);
            var bend = (knee0 - hip); bend -= axis * Vector3.Dot(bend, axis);
            if (bend.sqrMagnitude < 1e-8f) bend = Vector3.Cross(axis, Vector3.right).sqrMagnitude > 1e-6f ? Vector3.Cross(axis, Vector3.right) : Vector3.Cross(axis, Vector3.forward);
            bend.Normalize();
            float along = (lu * lu - ll * ll + dc * dc) / (2f * dc), h = Mathf.Sqrt(Mathf.Max(0f, lu * lu - along * along));
            Vector3 knee = hip + axis * along + bend * h;
            Vector3 ankle = hip + axis * dc;
            var qA = Quaternion.FromToRotation(q[A] * bd.lengths[B].normalized, (knee - hip).normalized) * q[A];
            var qBpre = qA * bd.ql[B];
            var qB = Quaternion.FromToRotation(qBpre * bd.lengths[F].normalized, (ankle - knee).normalized) * qBpre;
            bd.over[A] = true; bd.qover[A] = qA; bd.over[B] = true; bd.qover[B] = qB; bd.over[F] = true; bd.qover[F] = footWorld;
            // (the bones above the knee / ankle in the order are final for the caller's second pass; this leg's own rotations are in qover, positions follow)
            q[A] = qA; p[B] = hip + qA * bd.lengths[B];
            q[B] = qB; p[F] = p[B] + qB * bd.lengths[F];
            q[F] = footWorld;
        }

        /// The lowest foot bone of a pose (world y), the walk's ground test.
        public static float LowestFoot(Binding bd, string[] riderNames, float[] world)
        {
            float low = float.MaxValue;
            for (int i = 0; i < bd.count; i++)
                if (riderNames[i].EndsWith("Foot", StringComparison.Ordinal)) low = Mathf.Min(low, world[i * 16 + 13]);
            return low;
        }
    }
}
