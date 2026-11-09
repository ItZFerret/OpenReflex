// A rider animation clip of the player's own install ("anim" assets: the MXRider skeleton's keyframes), sampled on the CPU with the
// game's own rules (core/game/loop_anim.c: AnimKeys_Locate / SampleRotation / SampleTranslation, rx_motion_sample 00840040).
// Used where no game world runs the animation system: the garage's rider (the front end's MXGarage plays RiderUIAnimations/Base
// SC_RU_HC_Anim_NoLoop_0n on the garage rider). The clips are read from the install at run time; nothing of them is kept in the project.
//
// File layout (little endian), recovered from the files and the sampler:
//   i32 -11, u32 0x12345678, u32 key data size, i32 track count (69, one per node of MXRider_Hierarchy);
//   per track: u32 length + the node name (NUL terminated);
//   the key set (64 bytes = 16 dwords, the in-memory structure the sampler reads): [0] data size, [1] the one animated translation
//   track (the root's, 64), [2] frames per second, [3] translation keys per second, [4] length in seconds (float), [7] the animated
//   translation rows (= frames) and the first row of the static translations, [8] frames, [9] rotation tracks, [10] static translation
//   tracks, [11] byte offset of the rotation keys in the data;
//   the data: 16-byte translation rows (animated rows, then one row per track), then per frame (track count rounded up to even) * 8 bytes of
//   int16 xyzw quaternions (x 1/32767).
// Poses are the game's local matrices (Node_SetLocalRotation 007dcdf0: rows of the unit quaternion, translation in row 3, row vectors)
// chained world = local * parentWorld, exactly what OriginalRider.ApplyBoneMatrices takes.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace Reflex.Port.Content
{
    internal sealed class ReflexRiderAnim
    {
        public readonly string name;
        public float length;
        public int fps, frames;
        string[] trackNames;
        byte[] d; int dataOff, trackCount, animTrack, transRate, rowBase, rotTracks, statics, rotBase, stride;
        string[] boundNames; int[] trackOfNode;

        ReflexRiderAnim(string name) { this.name = name; }

        static readonly Dictionary<string, ReflexRiderAnim> cache = new Dictionary<string, ReflexRiderAnim>(StringComparer.OrdinalIgnoreCase);

        /// The clip of a package of the install (RiderUIAnimations / Base / SC_RU_HC_Anim_NoLoop_01), null when the install lacks it.
        public static ReflexRiderAnim Load(string database, string package, string clip)
        {
            string key = database + "/" + package + "/" + clip;
            lock (cache) if (cache.TryGetValue(key, out var hit)) return hit;
            ReflexRiderAnim a = null;
            try
            {
                var install = VehicleCatalog.Shared.install;
                var asset = install.Find(database, package, clip, "anim");
                if (asset != null) a = Parse(clip, install.Read(asset));
            }
            catch (Exception e) { Debug.LogWarning("[RiderAnim] " + key + ": " + e.Message); a = null; }
            lock (cache) cache[key] = a;
            return a;
        }

        static ReflexRiderAnim Parse(string name, byte[] b)
        {
            var a = new ReflexRiderAnim(name) { d = b };
            Rfx.Check(b.Length > 80 && BitConverter.ToInt32(b, 0) == -11 && BitConverter.ToUInt32(b, 4) == 0x12345678u, "anim header");
            int dataSize = BitConverter.ToInt32(b, 8); a.trackCount = BitConverter.ToInt32(b, 12);
            Rfx.Check(a.trackCount > 0 && a.trackCount <= 128, "anim track count");
            int p = 16; a.trackNames = new string[a.trackCount];
            for (int i = 0; i < a.trackCount; i++)
            {
                int n = BitConverter.ToInt32(b, p); p += 4;
                Rfx.Check(n > 0 && n < 256 && p + n <= b.Length, "anim track name");
                a.trackNames[i] = System.Text.Encoding.ASCII.GetString(b, p, n - 1); p += n;
            }
            Rfx.Check(p + 64 + dataSize == b.Length, "anim size");
            a.animTrack = BitConverter.ToInt32(b, p + 4); a.fps = BitConverter.ToInt32(b, p + 8); a.transRate = BitConverter.ToInt32(b, p + 12);
            a.length = BitConverter.ToSingle(b, p + 16); a.rowBase = BitConverter.ToInt32(b, p + 28); a.frames = BitConverter.ToInt32(b, p + 32);
            a.rotTracks = BitConverter.ToInt32(b, p + 36); a.statics = BitConverter.ToInt32(b, p + 40); a.rotBase = BitConverter.ToInt32(b, p + 44);
            a.dataOff = p + 64; a.stride = (a.rotTracks + 1) & ~1;
            Rfx.Check(a.rotTracks == a.trackCount && a.statics == a.trackCount && a.frames > 0 && a.fps > 0 && a.length > 0, "anim key set");
            Rfx.Check(a.rotBase == 16 * (a.rowBase + a.statics) && a.rotBase + 8 * a.stride * a.frames == dataSize, "anim key data");
            return a;
        }

        public bool Has(string node)
        {
            foreach (var t in trackNames) if (string.Equals(t, node, StringComparison.Ordinal)) return true;
            return false;
        }

        static Quaternion Key(byte[] d, int o)
        {
            const float s = 3.0518509e-05f;   // 1 / 32767 (0x92e054)
            return new Quaternion(BitConverter.ToInt16(d, o) * s, BitConverter.ToInt16(d, o + 2) * s, BitConverter.ToInt16(d, o + 4) * s, BitConverter.ToInt16(d, o + 6) * s);
        }
        static Quaternion Slerp(Quaternion a, Quaternion b, float t)
        {
            float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            if (dot < 0f) { b = new Quaternion(-b.x, -b.y, -b.z, -b.w); dot = -dot; }
            float wa, wb;
            if (dot > 0.9995f) { wa = 1f - t; wb = t; }
            else { float th = Mathf.Acos(Mathf.Clamp(dot, -1f, 1f)), s = Mathf.Sin(th); wa = Mathf.Sin((1f - t) * th) / s; wb = Mathf.Sin(t * th) / s; }
            var r = new Quaternion(a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb);
            float m = Mathf.Sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
            return m > 1e-8f ? new Quaternion(r.x / m, r.y / m, r.z / m, r.w / m) : Quaternion.identity;
        }

        /// The local rotation and translation of every track at the time (seconds; wrapped into the clip as AnimKeys_Locate does).
        public void SampleTracks(float t, Quaternion[] rot, Vector3[] pos)
        {
            if (t > length) t %= length;
            if (t < 0f) t = 0f;
            float f = fps * t;
            int frame = Mathf.Clamp((int)f, 0, frames - 1);
            int next = frame + 1 >= frames ? frame : frame + 1;
            float frac = next == frame ? 0f : f - frame;
            int k0 = dataOff + rotBase + 8 * stride * frame, k1 = dataOff + rotBase + 8 * stride * next;
            for (int k = 0; k < trackCount; k++) rot[k] = Slerp(Key(d, k0 + 8 * k), Key(d, k1 + 8 * k), frac);
            for (int k = 0; k < trackCount; k++)
            {
                int o = dataOff + 16 * (rowBase + k);
                pos[k] = new Vector3(BitConverter.ToSingle(d, o), BitConverter.ToSingle(d, o + 4), BitConverter.ToSingle(d, o + 8));
            }
            if (animTrack >= 0 && animTrack < trackCount)
            {
                int i = Mathf.Clamp((int)(transRate * t), 0, rowBase - 1), j = i + 1 >= rowBase ? i : i + 1;
                float g = j == i ? 0f : transRate * t - i;
                int a = dataOff + 16 * i, bb = dataOff + 16 * j;
                pos[animTrack] = new Vector3(
                    BitConverter.ToSingle(d, a) * (1 - g) + BitConverter.ToSingle(d, bb) * g,
                    BitConverter.ToSingle(d, a + 4) * (1 - g) + BitConverter.ToSingle(d, bb + 4) * g,
                    BitConverter.ToSingle(d, a + 8) * (1 - g) + BitConverter.ToSingle(d, bb + 8) * g);
            }
        }

        readonly Quaternion[] qs = new Quaternion[128]; readonly Vector3[] ts = new Vector3[128];
        readonly float[] local = new float[16];

        /// An additive clip laid over the base pose (PerformanceAnimator's additive blocks, 0083e930): the clip at its own time, scaled by the block's weight.
        public struct Additive { public ReflexRiderAnim anim; public float time, weight; }

        int[] addMap; string[] addNames;
        /// The track of each hierarchy node in this clip (-1: none), kept for the node list.
        int[] MapOf(string[] nodeNames)
        {
            if (addNames != nodeNames)
            {
                addMap = new int[nodeNames.Length];
                for (int i = 0; i < nodeNames.Length; i++) addMap[i] = Array.IndexOf(trackNames, nodeNames[i]);
                addNames = nodeNames;
            }
            return addMap;
        }

        // 0083e930, per bone: r = p * t (the additive clip's rotation t), p' = s0 p + s1 r with the weights of (d = p . r, weight) (the unsigned form:
        // theta = acos |d|, s0 = sin((1 - w) theta) / sin theta, s1 = sin(w theta) / sin theta), the translation gets t * weight added
        static Quaternion AddRotation(Quaternion p, Quaternion t, float w)
        {
            var r = p * t;
            float d = p.x * r.x + p.y * r.y + p.z * r.z + p.w * r.w, ad = Mathf.Abs(d);
            float s0 = 1f, s1 = 0f;
            if (ad < 1f)
            {
                float th = Mathf.Acos(ad), sin = Mathf.Sin(th);
                if (sin > 1e-6f) { s0 = Mathf.Sin((1f - w) * th) / sin; s1 = Mathf.Sin(w * th) / sin; }
            }
            var q = new Quaternion(p.x * s0 + r.x * s1, p.y * s0 + r.y * s1, p.z * s0 + r.z * s1, p.w * s0 + r.w * s1);
            float m = Mathf.Sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            return m > 1e-8f ? new Quaternion(q.x / m, q.y / m, q.z / m, q.w / m) : p;
        }

        /// The skeleton's bone world matrices (row-major, row vectors, nodeCount * 16 floats) at the time, hung on rootWorld (16 floats).
        /// nodeNames / parents: the rider hierarchy (node i's parent < i, -1 for the root node, which hangs on rootWorld). A node the clip
        /// has no track for keeps its parent's frame (identity local). Returns the number of nodes without a track.
        /// adds: the additive blocks of a performance active at this time (the ATV performances' ATVRider_AD_* clips).
        public int World(float t, string[] nodeNames, int[] parents, float[] rootWorld, float[] outWorld, IList<Additive> adds = null)
        {
            int n = nodeNames.Length, missing = 0;
            if (boundNames != nodeNames)
            {
                trackOfNode = new int[n];
                for (int i = 0; i < n; i++) trackOfNode[i] = Array.IndexOf(trackNames, nodeNames[i]);
                boundNames = nodeNames;
            }
            SampleTracks(t, qs, ts);
            if (adds != null)
                for (int a = 0; a < adds.Count; a++)
                {
                    var add = adds[a]; if (add.anim == null || add.weight <= 0f) continue;
                    add.anim.SampleTracks(add.time, add.anim.qs, add.anim.ts);
                    var map = add.anim.MapOf(nodeNames);
                    for (int i = 0; i < n; i++)
                    {
                        int k = trackOfNode[i], ka = map[i];
                        if (k < 0 || ka < 0) continue;
                        qs[k] = AddRotation(qs[k], add.anim.qs[ka], add.weight);
                        ts[k] += add.anim.ts[ka] * add.weight;
                    }
                }
            for (int i = 0; i < n; i++)
            {
                int k = trackOfNode[i];
                if (k < 0) { Identity(local); missing++; }
                else
                {
                    var q = qs[k]; var v = ts[k];
                    float x2 = q.x * 2f, y2 = q.y * 2f, z2 = q.z * 2f;
                    float zz = q.z * z2, wx = q.w * x2, wz = q.w * z2, wy = q.w * y2, xx = q.x * x2, yy = q.y * y2, xy = q.y * x2, yz = q.z * y2, xz = q.z * x2;
                    local[0] = 1f - (zz + yy); local[1] = xy + wz; local[2] = xz - wy; local[3] = 0;
                    local[4] = xy - wz; local[5] = 1f - (zz + xx); local[6] = yz + wx; local[7] = 0;
                    local[8] = xz + wy; local[9] = yz - wx; local[10] = 1f - (yy + xx); local[11] = 0;
                    local[12] = v.x; local[13] = v.y; local[14] = v.z; local[15] = 1;
                }
                int pr = parents[i];
                if (pr < 0) Mul(local, rootWorld, 0, outWorld, i * 16);
                else Mul(local, outWorld, pr * 16, outWorld, i * 16);
            }
            return missing;
        }

        /// The largest translation and the largest turn (degrees) any track of the clip makes over its length: what an additive clip adds (diagnostics).
        public void Extent(out float maxTranslation, out float maxTurnDegrees)
        {
            maxTranslation = 0f; maxTurnDegrees = 0f;
            var q = new Quaternion[128]; var v = new Vector3[128];
            for (int s = 0; s <= 8; s++)
            {
                SampleTracks(length * s / 8f, q, v);
                for (int k = 0; k < trackCount; k++)
                {
                    maxTranslation = Mathf.Max(maxTranslation, v[k].magnitude);
                    maxTurnDegrees = Mathf.Max(maxTurnDegrees, Quaternion.Angle(Quaternion.identity, q[k]));
                }
            }
        }

        /// The local rotation and translation of each named node at the time (identity / zero for a node the clip has no track for); returns
        /// how many nodes had a track.
        public int Locals(float t, string[] nodeNames, Quaternion[] outRot, Vector3[] outPos)
        {
            SampleTracks(t, qs, ts);
            int found = 0;
            for (int i = 0; i < nodeNames.Length; i++)
            {
                int k = Array.IndexOf(trackNames, nodeNames[i]);
                if (k < 0) { outRot[i] = Quaternion.identity; if (outPos != null) outPos[i] = Vector3.zero; continue; }
                outRot[i] = qs[k]; if (outPos != null) outPos[i] = ts[k]; found++;
            }
            return found;
        }

        static void Identity(float[] m) { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1f : 0f; }
        // r = a * b (row-major row vectors); a is the separate local matrix, r's rows never overlap b's (a parent comes before its child)
        static void Mul(float[] a, float[] b, int bo, float[] r, int ro)
        {
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    r[ro + i * 4 + j] = a[i * 4] * b[bo + j] + a[i * 4 + 1] * b[bo + 4 + j] + a[i * 4 + 2] * b[bo + 8 + j] + a[i * 4 + 3] * b[bo + 12 + j];
        }
    }
}
