// The animation of a stunt (a trick) for the front end's trick preview (OpenFrontEnd/OpenStuntPreview): read from the player's install at run time,
// nothing of it is kept in the project.
//
// What the game has: the StuntTable names each trick (Stunt name="BarHop_Air"); the trick's "performance" is the perfdat asset
// "<MX|ATV>Rider_<name>" of the Animations database (fx_stunt.c 006cd050 builds the name with "%sRider_%s"): a BXML <Performance> with <AnimBlocks>,
// each <AnimBlock name start duration looping> a rider clip (an "anim" asset of the same database, e.g. MXRider_ST_Barhop_In / _Hold / _Out) placed on
// the performance's timeline (start, duration in frames of 1/30 s; the Hold block loops). The vehicle moves with it: every rider clip has a vehicle
// clip of the same name with "Vehicle" for "Rider" (MXVehicle_ST_Barhop_In: version 11, the tracks of OBJ_Frame, the vehicle as one piece, and of its
// forks / steering stem; the layout is ReflexPushAnim's). The rider clips run on the MXRider skeleton (ReflexRiderAnim).
//
// A trick without a perfdat asset (Wheelie, Stoppie, Backflip, ThreeSixty, Whip: the StuntTable's Physics ones) has no animation: the bike's own
// physics does it in a ride, so Load returns null for them.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    internal sealed class ReflexStuntAnim
    {
        /// One clip of the performance with the vehicle's track.
        public sealed class Block
        {
            public string name; public float start, duration; public bool looping;
            /// An additive block (the ATV performances' ATVRider_AD_* clips: PelvisDown, PelvisBackward, StuntStart): not a step of the timeline but a clip laid
            /// over the pose for the whole span [start, start + timeScale * duration] (build_perf.c: the block's end), its own clip run once over that span
            /// (PerformanceAnimator sets the clip's AnimationPercent to the fraction of the span), the weight addMax ramped by addIn / addOut.
            public bool additive; public float timeScale = 1f, addMax = 1f, addIn, addOut = 1f;
            public float Span => timeScale * duration;
            public ReflexRiderAnim rider; public VehicleClip vehicle;
            /// The clip's own length (seconds): the rider's (the performance's duration is the same in frames of 1/30 s).
            public float Length => rider != null ? rider.length : duration;
        }

        /// A vehicle clip (the packed-less version 11 layout of ReflexPushAnim.ReadVehicle): the OBJ_Frame translation and rotation keys.
        public sealed class VehicleClip
        {
            public readonly string name;
            float[] posT, rotT; Vector3[] posK; Quaternion[] rotK;
            public bool HasFrame => posT != null && rotT != null;
            VehicleClip(string name) { this.name = name; }

            public static VehicleClip Load(string name)
            {
                try
                {
                    var install = VehicleCatalog.Shared.install;
                    var asset = install.Find("Animations", "Animations", name, "anim");
                    return asset == null ? null : Parse(name, install.Read(asset));
                }
                catch (Exception e) { Debug.LogWarning("[StuntAnim] vehicle clip " + name + ": " + e.Message); return null; }
            }

            static VehicleClip Parse(string name, byte[] b)
            {
                var c = new VehicleClip(name);
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
                p += 4 + 8;
                int nt = BitConverter.ToInt32(b, p), nr = BitConverter.ToInt32(b, p + 4); p += 8;
                int floatTracks = BitConverter.ToInt32(b, p), keyBones = BitConverter.ToInt32(b, p + 4), pairs = BitConverter.ToInt32(b, p + 8); p += 12;
                Rfx.Check(floatTracks == 0 && keyBones == 0 && pairs == 0, "vehicle clip extra tracks");
                p += 12;
                int frame = Array.IndexOf(nodes, "OBJ_Frame");
                for (int k = 0; k < nt; k++)
                {
                    int count = BitConverter.ToInt32(b, p), node = BitConverter.ToInt32(b, p + 4); p += 8;
                    Rfx.Check(count > 0 && p + 16 * count <= b.Length, "vehicle clip translation keys");
                    if (node == frame)
                    {
                        c.posT = new float[count]; c.posK = new Vector3[count];
                        for (int i = 0; i < count; i++) { int o = p + 16 * i; c.posT[i] = BitConverter.ToSingle(b, o); c.posK[i] = new Vector3(BitConverter.ToSingle(b, o + 4), BitConverter.ToSingle(b, o + 8), BitConverter.ToSingle(b, o + 12)); }
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
                        c.rotT = new float[count]; c.rotK = new Quaternion[count];
                        for (int i = 0; i < count; i++)
                        {
                            int o = p + 12 * i; c.rotT[i] = BitConverter.ToSingle(b, o);
                            var q = new Quaternion(BitConverter.ToInt16(b, o + 4) * s, BitConverter.ToInt16(b, o + 6) * s, BitConverter.ToInt16(b, o + 8) * s, BitConverter.ToInt16(b, o + 10) * s);
                            float m = Mathf.Sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
                            c.rotK[i] = m > 1e-6f ? new Quaternion(q.x / m, q.y / m, q.z / m, q.w / m) : Quaternion.identity;
                        }
                    }
                    p += 12 * count;
                }
                return c;
            }

            static int Locate(float[] times, float t, out float frac)
            {
                frac = 0f;
                if (t <= times[0]) return 0;
                int last = times.Length - 1;
                if (t >= times[last]) return last;
                int lo = 0, hi = last;
                while (hi - lo > 1) { int mid = (lo + hi) >> 1; if (times[mid] <= t) lo = mid; else hi = mid; }
                float span = times[lo + 1] - times[lo];
                frac = span > 1e-6f ? (t - times[lo]) / span : 0f;
                return lo;
            }

            /// OBJ_Frame at the time: its translation (feet) and rotation (continuous: each key is turned to the hemisphere of the one before).
            public void Frame(float t, out Vector3 pos, out Quaternion rot)
            {
                int i = Locate(posT, t, out float f);
                pos = i + 1 < posK.Length ? Vector3.Lerp(posK[i], posK[i + 1], f) : posK[i];
                int j = Locate(rotT, t, out float g);
                if (j + 1 < rotK.Length)
                {
                    var a = rotK[j]; var c = rotK[j + 1];
                    if (Quaternion.Dot(a, c) < 0f) c = new Quaternion(-c.x, -c.y, -c.z, -c.w);
                    rot = Quaternion.Slerp(a, c, g);
                }
                else rot = rotK[j];
            }
        }

        public readonly string name;      // the StuntTable name ("BarHop_Air")
        public readonly bool atv;
        /// The blocks in timeline order (at one start the plain steps come before the additive blocks).
        public readonly List<Block> blocks = new List<Block>();
        /// The first plain step of the timeline (its vehicle clip is the vehicle's rest frame of the whole performance).
        public Block First { get { foreach (var b in blocks) if (!b.additive) return b; return null; } }
        /// The seconds the Hold block plays (the loop repeated whole times; 0: the performance as authored, one pass).
        public const float DefaultHoldSeconds = 1.6f;

        ReflexStuntAnim(string name, bool atv) { this.name = name; this.atv = atv; }

        static readonly Dictionary<string, ReflexStuntAnim> cache = new Dictionary<string, ReflexStuntAnim>(StringComparer.Ordinal);
        /// The last Load's problem (null: none; "no performance" for the physics tricks).
        public static string Problem { get; private set; }

        /// The trick's clips, or null when the install has no performance for it (Problem says why). Cached.
        public static ReflexStuntAnim Load(string stunt, bool atv)
        {
            string key = (atv ? "ATV|" : "MX|") + stunt;
            lock (cache) if (cache.TryGetValue(key, out var hit)) { if (hit == null) Problem = "no performance"; return hit; }
            ReflexStuntAnim result = null; Problem = null;
            try
            {
                string prefix = atv ? "ATV" : "MX";
                var install = VehicleCatalog.Shared.install;
                var asset = install.Find("Animations", "Animations", prefix + "Rider_" + stunt, "perfdat");
                if (asset == null) Problem = "no performance";
                else
                {
                    var root = BxmlData.Xml(install.Read(asset));
                    BxmlData.Check(root.tag == "Performance", "Performance root");
                    var list = root.Child("AnimBlocks");
                    var a = new ReflexStuntAnim(stunt, atv);
                    if (list != null)
                        foreach (var n in list.children.Where(x => x.tag == "AnimBlock"))
                        {
                            float Num(string key, float def) => n.attributes.TryGetValue(key, out var o) && o != null ? Convert.ToSingle(o, System.Globalization.CultureInfo.InvariantCulture) : def;
                            var b = new Block
                            {
                                name = n.S("name"), start = Convert.ToSingle(n.attributes.TryGetValue("start", out var s) ? s : 0) / 30f,
                                duration = Convert.ToSingle(n.attributes.TryGetValue("duration", out var d) ? d : 0) / 30f,
                                looping = n.attributes.TryGetValue("looping", out var l) && Convert.ToBoolean(l),
                                additive = n.attributes.TryGetValue("additive", out var ad) && Convert.ToBoolean(ad),
                                timeScale = Num("timeScale", 1f), addMax = Num("addMax", 1f), addIn = Num("addIn", 0f), addOut = Num("addOut", 1f),
                            };
                            b.rider = ReflexRiderAnim.Load("Animations", "Animations", b.name);
                            if (b.rider == null) throw new InvalidOperationException("the install lacks the rider clip " + b.name);
                            // the vehicle's clip: the same name with Vehicle for Rider (MXVehicle_ST_Barhop_In); a block without one leaves the vehicle at rest
                            // (an additive block moves only the rider's bones)
                            int at = b.name.IndexOf("Rider_", StringComparison.Ordinal);
                            if (at >= 0 && !b.additive) b.vehicle = VehicleClip.Load(b.name.Substring(0, at) + "Vehicle_" + b.name.Substring(at + 6));
                            a.blocks.Add(b);
                        }
                    a.blocks.Sort((x, y) => { int c = x.start.CompareTo(y.start); return c != 0 ? c : x.additive.CompareTo(y.additive); });   // the plain steps first at one start
                    if (a.blocks.Count == 0) Problem = "no clips"; else result = a;
                }
            }
            catch (Exception e) { Problem = e.Message; Debug.LogWarning("[StuntAnim] " + stunt + ": " + e.Message); result = null; }
            lock (cache) cache[key] = result;
            return result;
        }

        /// The length of the preview (seconds): the clips in order, the looping one for holdSeconds (rounded to whole loops, at least one).
        public float Length(float holdSeconds = DefaultHoldSeconds)
        {
            float t = 0;
            foreach (var b in blocks) if (!b.additive) t += b.looping ? Mathf.Max(1, Mathf.Round(holdSeconds / Mathf.Max(0.1f, b.Length))) * b.Length : b.Length;
            return t;
        }

        /// The clip and the time inside it at the preview's time t (past the end: the last frame of the last clip).
        public Block At(float t, out float local, float holdSeconds = DefaultHoldSeconds)
        {
            Block last = null;
            foreach (var b in blocks)
            {
                if (b.additive) continue;
                float len = b.looping ? Mathf.Max(1, Mathf.Round(holdSeconds / Mathf.Max(0.1f, b.Length))) * b.Length : b.Length;
                last = b;
                if (t < len) { local = b.looping ? t % Mathf.Max(0.01f, b.Length) : t; return b; }
                t -= len;
            }
            local = last != null ? last.Length : 0f; return last;
        }

        /// The additive blocks active at the performance time (the base block's start plus the time inside it): each clip at its fraction of the block's span,
        /// weighted as the performer does (performance_clip_weight: addMax, ramped in over addIn and out after addOut). `into` is cleared first.
        public void Additives(float performanceTime, List<ReflexRiderAnim.Additive> into)
        {
            into.Clear();
            foreach (var b in blocks)
            {
                if (!b.additive || b.rider == null) continue;
                float span = Mathf.Max(1e-4f, b.Span);
                if (performanceTime < b.start || performanceTime > b.start + span) continue;
                float f = (performanceTime - b.start) / span, w = b.addMax;
                if (b.addIn > f) w = f / b.addIn * w;
                if (f > b.addOut) w = (1f - (f - b.addOut) / Mathf.Max(1e-4f, 1f - b.addOut)) * w;
                into.Add(new ReflexRiderAnim.Additive { anim = b.rider, time = Mathf.Min(f, 0.9999f) * b.rider.length, weight = w });
            }
        }
    }
}
