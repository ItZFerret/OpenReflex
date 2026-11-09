// The replay's recording (ReflexReplay): a ring of pose frames, one per game frame shown (60 a second, 30 when the length asked for
// would not fit the memory budget), that drops the oldest once it is full. After Open Unleashed's MxaReplayBuffer.
//
// A frame is what the screen showed, not the simulation: playing it back never steps the core. Per frame, for every "actor" (the
// local rider, each other rider of the race, the scenery the game moves such as the start gate):
//  - the actor's top transforms (the chassis, the wheels, the rider's root, the capsule stand-in) in full: world position and rotation
//    as floats; the first one (the chassis) is the actor's anchor;
//  - every other transform the game poses (the bike's MX_Hierarchy parts, the rider's 69 bones) packed to 16 bytes: the world position
//    relative to the anchor as three halves (2.4 mm at 10 ft, 1 cm at 60 ft), the world rotation as four snorm16, one spare word;
//  - the active flags of the actor's objects (a hidden machine panel, the scenario's hidden rider), one bit each;
//  - its place and lap.
// Plus the game camera (the GAME CAMERA replay), the local rider's numbers (the rider card) and, when REPLAY EFFECTS is on, the frame's
// particles (dust, roost, splashes: the core's ReflexParticle structs, kept in their own ring, ReplayParticleRing).
// Sampling plays back by time: two frames around t blend (positions lerp, rotations nlerp), so slow motion is smooth.
using System;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Replay
{
    public sealed class ReplayFrame
    {
        public float time;
        public float[] full = Array.Empty<float>();       // 7 floats per full transform (px py pz qx qy qz qw)
        public ushort[] nodes = Array.Empty<ushort>();     // NodeWords per packed transform
        public ulong[] flags = Array.Empty<ulong>();       // per actor: bit i = flag object i active
        public byte[] place = Array.Empty<byte>(), lap = Array.Empty<byte>();
        public Vector3 camPos;
        public Quaternion camRot = Quaternion.identity;
        public float camFov = 50f, camNear = 0.5f;
        // the local rider's numbers (the rider card)
        public float speedMph, raceTime;
        public int racers, lapsTotal;
        // the frame's particles in the particle ring (start < 0: none recorded)
        public long particleStart = -1;
        public int particleCount;
        public Vector3 particleOrigin, particleStep;
        // w-replayphoto2: the terrain deltas and the sound commands that came with this frame (their rings), each rider's mud, the packs' telemetry
        public long terrainStart; public int terrainCount;
        public long soundStart; public int soundCount;
        public ushort[] mud = Array.Empty<ushort>();
        public ReplayPackSample[] packs = Array.Empty<ReplayPackSample>(); public int packCount;
        public ReflexParticleInfo particleInfo;
    }

    /// The layout of one actor in a frame: where its full transforms, packed transforms and flags live.
    public struct ReplayActorLayout
    {
        public int fullStart, fullCount, nodeStart, nodeCount;
    }

    public sealed class ReflexReplayBuffer
    {
        public const int NodeWords = 8;    // ushorts per packed transform
        public const int FullFloats = 7;   // floats per full transform

        ReplayFrame[] ring = new ReplayFrame[0];
        int start, count;

        public int Count => count;
        public int Capacity => ring.Length;
        public int Actors { get; private set; }
        public ReplayActorLayout[] Layout { get; private set; } = Array.Empty<ReplayActorLayout>();
        public int FullTotal { get; private set; }
        public int NodeTotal { get; private set; }
        public float Interval { get; private set; } = 1f / 60f;
        public long FrameBytes { get; private set; }
        public float StartTime => count > 0 ? this[0].time : 0f;
        public float EndTime => count > 0 ? this[count - 1].time : 0f;
        public float Duration => EndTime - StartTime;

        public ReplayFrame this[int i] => ring[(start + i) % ring.Length];
        public ReplayFrame Last => count > 0 ? this[count - 1] : null;

        /// Starts over with this layout (a new session, another field): seconds of history within budgetBytes.
        public void Reset(ReplayActorLayout[] layout, float seconds, long budgetBytes)
        {
            Layout = layout; Actors = layout.Length;
            FullTotal = 0; NodeTotal = 0;
            foreach (var a in layout) { FullTotal += a.fullCount; NodeTotal += a.nodeCount; }
            // the frame object, its arrays' headers and the fixed fields (~200 bytes), the transforms, the flags and numbers
            FrameBytes = 200 + FullTotal * FullFloats * 4L + NodeTotal * NodeWords * 2L + Actors * (8L + 2L) + 4 * 32;
            // 60 frames a second when the length fits the budget there, else 30 (the blend between frames keeps it smooth; the wheels' spin
            // is the first to show it)
            Interval = 1f / 60f;
            if (FrameBytes * (long)Mathf.CeilToInt(seconds * 60f) > budgetBytes) Interval = 1f / 30f;
            int want = Mathf.CeilToInt(seconds / Interval) + 2;
            int fit = (int)Math.Max(120, budgetBytes / Math.Max(1, FrameBytes));
            int cap = Mathf.Clamp(Mathf.Min(want, fit), 2, 40000);
            // The frames are kept for the next layouts instead of being made again (w-gcspikes): every frame held (the ring's, the spares) goes to
            // `spareFor` when it has this layout's shape, to `spareOther` when it has the one other shape kept (a world records its few actors
            // first, then the whole field: the field's frames, from the last race, come back), else it is dropped. Next() takes the spares.
            long shape = Shape(FullTotal * FullFloats, NodeTotal * NodeWords, Actors);
            var oldFor = spareFor.ToArray(); var oldOther = spareOther.ToArray();
            spareFor.Clear(); spareOther.Clear(); otherShape = -1;
            foreach (var f in ring) Keep(f, shape);
            foreach (var f in oldFor) Keep(f, shape);
            foreach (var f in oldOther) Keep(f, shape);
            if (ring.Length != cap) ring = new ReplayFrame[cap]; else Array.Clear(ring, 0, ring.Length);   // the frames themselves come as the ring fills
            start = count = 0;
        }

        readonly System.Collections.Generic.Stack<ReplayFrame> spareFor = new System.Collections.Generic.Stack<ReplayFrame>(), spareOther = new System.Collections.Generic.Stack<ReplayFrame>();
        long otherShape = -1;
        static long Shape(int full, int nodes, int actors) => ((long)full << 40) ^ ((long)nodes << 16) ^ actors;
        void Keep(ReplayFrame f, long shape)
        {
            if (f == null) return;
            long fs = Shape(f.full.Length, f.nodes.Length, f.flags.Length);
            if (fs == shape) spareFor.Push(f);
            else if (otherShape == -1 || fs == otherShape) { otherShape = fs; spareOther.Push(f); }
        }

        public void Clear() { start = count = 0; }

        /// Seconds of history this layout keeps when full.
        public float Seconds => Math.Max(0, ring.Length - 1) * Interval;

        /// Bytes the ring holds when full.
        public long FullBytes => ring.Length * FrameBytes;

        bool Fits(ReplayFrame f) => f.full.Length == FullTotal * FullFloats && f.nodes.Length == NodeTotal * NodeWords && f.flags.Length == Actors;

        /// The frame to fill next (the oldest one when full): appended before the caller fills it.
        public ReplayFrame Next()
        {
            int slot;
            if (count < ring.Length) slot = (start + count++) % ring.Length;
            else { slot = start; start = (start + 1) % ring.Length; }
            var f = ring[slot];
            if ((f == null || !Fits(f)) && spareFor.Count > 0) { f = ring[slot] = spareFor.Pop(); f.packCount = 0; }
            if (f == null || !Fits(f))
            {
                f = ring[slot] = new ReplayFrame
                {
                    full = new float[FullTotal * FullFloats], nodes = new ushort[NodeTotal * NodeWords],
                    flags = new ulong[Actors], place = new byte[Actors], lap = new byte[Actors],
                };
            }
            f.particleStart = -1; f.particleCount = 0;
            return f;
        }

        /// The frames around t: a + (b - a) * w (a == b at the ends).
        public bool Locate(float t, out int a, out int b, out float w)
        {
            a = b = 0; w = 0f;
            if (count == 0) return false;
            if (t <= this[0].time) return true;
            if (t >= this[count - 1].time) { a = b = count - 1; return true; }
            int lo = 0, hi = count - 1;
            while (hi - lo > 1)
            {
                int mid = (lo + hi) >> 1;
                if (this[mid].time <= t) lo = mid; else hi = mid;
            }
            a = lo; b = hi;
            float ta = this[a].time, tb = this[b].time;
            w = tb > ta ? Mathf.Clamp01((t - ta) / (tb - ta)) : 0f;
            return true;
        }

        /// The frame index at or before t.
        public int IndexAt(float t)
        {
            Locate(t, out int a, out int b, out float w);
            return w >= 1f ? b : a;
        }

        // ------------------------------------------------------------------------------------- packing

        public static void PutFull(float[] dst, int i, Vector3 p, Quaternion q)
        {
            int o = i * FullFloats;
            dst[o] = p.x; dst[o + 1] = p.y; dst[o + 2] = p.z;
            dst[o + 3] = q.x; dst[o + 4] = q.y; dst[o + 5] = q.z; dst[o + 6] = q.w;
        }

        public static Vector3 FullPos(float[] src, int i) { int o = i * FullFloats; return new Vector3(src[o], src[o + 1], src[o + 2]); }
        public static Quaternion FullRot(float[] src, int i) { int o = i * FullFloats; return new Quaternion(src[o + 3], src[o + 4], src[o + 5], src[o + 6]); }

        public static void Pack(ushort[] dst, int i, Vector3 p, Quaternion q, Vector3 anchor)
        {
            int o = i * NodeWords;
            p -= anchor;
            if (q.w < 0f) q = new Quaternion(-q.x, -q.y, -q.z, -q.w);
            dst[o] = Mathf.FloatToHalf(p.x);
            dst[o + 1] = Mathf.FloatToHalf(p.y);
            dst[o + 2] = Mathf.FloatToHalf(p.z);
            dst[o + 3] = Snorm(q.x);
            dst[o + 4] = Snorm(q.y);
            dst[o + 5] = Snorm(q.z);
            dst[o + 6] = Snorm(q.w);
            dst[o + 7] = 0;
        }

        public static void Unpack(ushort[] src, int i, Vector3 anchor, out Vector3 p, out Quaternion q)
        {
            int o = i * NodeWords;
            p = anchor + new Vector3(Mathf.HalfToFloat(src[o]), Mathf.HalfToFloat(src[o + 1]), Mathf.HalfToFloat(src[o + 2]));
            q = new Quaternion(Unsnorm(src[o + 3]), Unsnorm(src[o + 4]), Unsnorm(src[o + 5]), Unsnorm(src[o + 6]));
        }

        static ushort Snorm(float v) => (ushort)(short)Mathf.RoundToInt(Mathf.Clamp(v, -1f, 1f) * 32767f);
        static float Unsnorm(ushort v) => (short)v / 32767f;

        /// Blends two rotations the short way and normalises.
        public static Quaternion Nlerp(Quaternion a, Quaternion b, float t)
        {
            if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0f) b = new Quaternion(-b.x, -b.y, -b.z, -b.w);
            var q = new Quaternion(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
            float n = Mathf.Sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            return n > 1e-6f ? new Quaternion(q.x / n, q.y / n, q.z / n, q.w / n) : Quaternion.identity;
        }
    }

    /// The recorded particles (REPLAY EFFECTS): one ring of the core's ReflexParticle structs packed to 36 bytes (of 64), filled frame after
    /// frame; a frame keeps where its particles start, how many, and the box their positions are packed in. The position is 16-bit fixed
    /// point over that frame's box (a 1000 ft spread: 0.015 ft), the sizes, pivot, scale, rotation (wrapped to one turn) and the atlas
    /// frame halves, the two colours and the flags as they are. The oldest frames' particles are written over first (they then show none).
    public sealed class ReplayParticleRing
    {
        public const int Words = 18;   // ushorts per particle
        ushort[] data = Array.Empty<ushort>();
        int capacity;
        long written;   // particles written since the last Clear (absolute index of the next one)

        public int Capacity => capacity;
        public long Bytes => data.Length * 2L;

        public void Reset(int particles)
        {
            if (capacity != particles) { capacity = Math.Max(0, particles); data = capacity > 0 ? new ushort[capacity * Words] : Array.Empty<ushort>(); }
            written = 0;
        }

        public void Clear() { written = 0; }

        static ushort H(float v) => HalfOf(v);
        static float F(ushort v) => Mathf.HalfToFloat(v);

        /// IEEE half of a float, round to nearest even (managed: the packing runs on a worker thread, w-replayphoto2).
        public static ushort HalfOf(float f)
        {
            uint x = (uint)BitConverter.SingleToInt32Bits(f);
            uint sign = (x >> 16) & 0x8000u;
            int exp = (int)((x >> 23) & 0xff);
            uint mant = x & 0x7fffffu;
            if (exp == 255) return (ushort)(sign | 0x7c00u | (mant != 0 ? 0x200u : 0u));
            int e = exp - 127 + 15;
            if (e >= 31) return (ushort)(sign | 0x7c00u);
            if (e <= 0)
            {
                if (e < -10) return (ushort)sign;
                mant |= 0x800000u;
                int shift = 14 - e;
                uint half = mant >> shift, rem = mant & ((1u << shift) - 1), mid = 1u << (shift - 1);
                if (rem > mid || (rem == mid && (half & 1) != 0)) half++;
                return (ushort)(sign | half);
            }
            uint h = ((uint)e << 10) | (mant >> 13), r = mant & 0x1fffu;
            if (r > 0x1000u || (r == 0x1000u && (h & 1) != 0)) h++;
            return (ushort)(sign | h);
        }

        /// The particles a frame keeps of count (one frame never takes more than a quarter of the ring).
        public int Clamp(int count) => capacity == 0 || count <= 0 ? 0 : Math.Min(count, capacity / 4);

        /// Takes the place of count particles in the ring (main thread); PackInto fills it.
        public long Reserve(int count)
        {
            if (capacity == 0 || count <= 0) return -1;
            long at = written; written += count; return at;
        }

        /// Appends count particles; returns their absolute start (-1: none kept), the box they were packed in (origin, step per unit).
        public long Append(ReflexParticle[] src, int count, out Vector3 origin, out Vector3 step)
        {
            origin = step = Vector3.zero;
            count = Math.Min(Clamp(count), src != null ? src.Length : 0);
            long at = Reserve(count);
            if (at < 0) return -1;
            PackInto(at, src, count, out origin, out step);
            return at;
        }

        /// Packs count particles of src into the reserved place at (any thread: nothing of Unity's).
        public void PackInto(long at, ReflexParticle[] src, int count, out Vector3 origin, out Vector3 step)
        {
            origin = step = Vector3.zero;
            if (capacity == 0 || src == null || count <= 0) return;
            Vector3 lo = new Vector3(float.MaxValue, float.MaxValue, float.MaxValue), hi = -lo;
            for (int i = 0; i < count; i++)
            {
                var q = src[i];
                if (q.px < lo.x) lo.x = q.px; if (q.py < lo.y) lo.y = q.py; if (q.pz < lo.z) lo.z = q.pz;
                if (q.px > hi.x) hi.x = q.px; if (q.py > hi.y) hi.y = q.py; if (q.pz > hi.z) hi.z = q.pz;
            }
            origin = lo;
            step = new Vector3(Mathf.Max(hi.x - lo.x, 1e-3f), Mathf.Max(hi.y - lo.y, 1e-3f), Mathf.Max(hi.z - lo.z, 1e-3f)) / 65535f;
            Vector3 inv = new Vector3(1f / step.x, 1f / step.y, 1f / step.z);
            for (int i = 0; i < count; i++)
            {
                var q = src[i];
                int o = (int)((at + i) % capacity) * Words;
                data[o] = (ushort)Math.Min(65535, Math.Max(0, (int)((q.px - lo.x) * inv.x + 0.5f)));
                data[o + 1] = (ushort)Math.Min(65535, Math.Max(0, (int)((q.py - lo.y) * inv.y + 0.5f)));
                data[o + 2] = (ushort)Math.Min(65535, Math.Max(0, (int)((q.pz - lo.z) * inv.z + 0.5f)));
                data[o + 3] = H(q.sizeX); data[o + 4] = H(q.sizeY); data[o + 5] = H(q.pivotX); data[o + 6] = H(q.pivotY);
                data[o + 7] = H(q.scale); data[o + 8] = H(Repeat(q.rotation));
                data[o + 9] = (ushort)q.color; data[o + 10] = (ushort)(q.color >> 16);
                data[o + 11] = (ushort)q.color1; data[o + 12] = (ushort)(q.color1 >> 16);
                data[o + 13] = H(q.uScale); data[o + 14] = H(q.vScale); data[o + 15] = H(q.uOffset); data[o + 16] = H(q.vOffset);
                data[o + 17] = (ushort)q.flags;
            }
        }

        static float Repeat(float r) { const float T = 6.2831853f; r -= (float)Math.Floor(r / T) * T; return r; }

        /// The particles of a frame are still in the ring (not written over since).
        public bool Valid(long at, int count) => at >= 0 && count > 0 && at >= written - capacity && at + count <= written;

        /// Unpacks a frame's particles into dst (grown when needed); returns how many.
        public int Read(long at, int count, Vector3 origin, Vector3 step, ref ReflexParticle[] dst)
        {
            if (!Valid(at, count)) return 0;
            if (dst == null || dst.Length < count) dst = new ReflexParticle[Math.Max(count, OriginalParticles.MaxParticles)];
            for (int i = 0; i < count; i++)
            {
                int o = (int)((at + i) % capacity) * Words;
                dst[i] = new ReflexParticle
                {
                    px = origin.x + data[o] * step.x, py = origin.y + data[o + 1] * step.y, pz = origin.z + data[o + 2] * step.z,
                    sizeX = F(data[o + 3]), sizeY = F(data[o + 4]), pivotX = F(data[o + 5]), pivotY = F(data[o + 6]),
                    scale = F(data[o + 7]), rotation = F(data[o + 8]),
                    color = data[o + 9] | (uint)data[o + 10] << 16, color1 = data[o + 11] | (uint)data[o + 12] << 16,
                    uScale = F(data[o + 13]), vScale = F(data[o + 14]), uOffset = F(data[o + 15]), vOffset = F(data[o + 16]),
                    flags = data[o + 17],
                };
            }
            return count;
        }
    }
}
