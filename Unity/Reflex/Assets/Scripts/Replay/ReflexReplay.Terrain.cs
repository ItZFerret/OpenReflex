// The replay's ruts and mud (ReflexReplay, w-replayphoto2): the terrain as it was at the moment shown, and each rider's mud.
//
// Terrain. Every patch the core sends (OriginalTerrain.BeforePatch: rx_session_terrain_tiles' dirty 32 x 32 tiles, heights and the
// deformation surface) is recorded as it arrives, against the terrain's own mirrors, as a sparse delta: for each changed texel its index
// in the grid and the change of the normalized height and of the deformation, as halves (8 bytes a texel; unchanged texels of the tiles
// are not stored). The deltas go into their own ring (TerrainRing) and belong to the next recorded frame; those that arrive after the last
// recorded frame (the race's Post) are the "tail". The live terrain is always known (it is on screen when the viewer opens), so the state
// at any recorded frame is the live one minus the deltas after it: going back undoes frames (subtracts), going forward redoes them (adds),
// whatever the distance, and the ring's oldest frame is the furthest back the viewer can go (a frame whose deltas the ring already wrote
// over stops the rewind there: the terrain then shows that later state). The changed rectangle is uploaded once per viewer frame
// (OriginalTerrain.ReplayUpload). Opening keeps a copy of both mirrors; closing copies the touched rectangle back and uploads it, so the
// live terrain returns bit for bit. Part of REPLAY EFFECTS. Not recorded: the tyre-track ribbons (OriginalTracks: the core's whole
// list every fourth frame, ~1.5 MB each), which show the live moment's.
//
// Mud. ReflexMud.Applied: each rider's mud record (amount, wetness, colour) per recorded frame (7 words an actor), applied again with the
// moment shown (only when it changes) and the live one back at Close.
using System;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Replay
{
    /// The terrain deltas' ring: ushort words; a patch is [x0, z0, w, h, count lo, count hi] then count x [index lo, index hi, dh, dd].
    public sealed class TerrainRing
    {
        ushort[] data = Array.Empty<ushort>();
        long written;
        public int Capacity => data.Length;
        public long Bytes => data.Length * 2L;
        public long Written => written;
        public void Reset(int words) { if (data.Length != words) data = words > 0 ? new ushort[words] : Array.Empty<ushort>(); written = 0; }
        public void Clear() => written = 0;
        public void Put(ushort w) { if (data.Length == 0) return; data[(int)(written % data.Length)] = w; written++; }
        public ushort this[long at] => data[(int)(at % data.Length)];
        public void Set(long at, ushort w) { if (data.Length > 0 && at >= written - data.Length && at < written) data[(int)(at % data.Length)] = w; }
        /// Drops the words written since `to` (a patch that changed nothing).
        public void Rewind(long to) { if (to <= written && to >= 0) written = to; }
        public bool Valid(long at, long count) => data.Length > 0 && at >= 0 && at >= written - data.Length && at + count <= written;
    }

    public sealed partial class ReflexReplay
    {
        public TerrainRing Terrain => Rings.terrain;
        long terrainPending;           // the absolute start of the deltas not yet given to a recorded frame
        long terrainTexels;            // recorded changed texels (the probe's rate)
        OriginalTerrain terrainSeen;

        /// Words the terrain ring holds: 8 MB a minute of REPLAY LENGTH (4x the 2 MB a minute measured on a 12-rider National), 16 .. 64 MB.
        static int TerrainWords => (int)(Math.Clamp((long)(LengthSeconds / 60f * (8L << 20)), 16L << 20, 64L << 20) / 2);

        void TerrainHooks(bool on)
        {
            if (on) { OriginalTerrain.BeforePatch += OnTerrainPatch; ReflexMud.Applied += OnMud; }
            else { OriginalTerrain.BeforePatch -= OnTerrainPatch; ReflexMud.Applied -= OnMud; }
        }

        void TerrainReset()
        {
            Terrain.Reset(RecordEffects ? TerrainWords : 0);
            terrainPending = 0; terrainTexels = 0; terrainFrozen = false;
        }

        // While nothing is recorded (the race over: the results, the end-of-race replay behind them, a scenario) the patches are not recorded
        // either: the terrain as the recording stopped is kept once (terrainFrozen: Rings.frozenH / D) and the viewer starts from it instead
        // of the live terrain. The end-of-race replay puts the whole terrain (2048 x 2048) back at each of its clips, and recording those
        // patches cost 80-95 ms of the main thread every few seconds behind the results (w-gcspikes). When recording starts again the older
        // deltas no longer lead to the live terrain: they are dropped (the rewind stops at the frames after it).
        bool terrainFrozen;
        void TerrainFreeze(OriginalTerrain t)
        {
            if (terrainFrozen) return;
            var hm = t.HeightMirror; var dm = t.DeformMirror; var r = Rings;
            if (r.frozenH.Length != hm.Length) r.frozenH = new float[hm.Length];
            if (r.frozenD.Length != dm.Length) r.frozenD = new float[dm.Length];
            Array.Copy(hm, r.frozenH, hm.Length); Array.Copy(dm, r.frozenD, dm.Length);
            terrainFrozen = true;
        }
        void TerrainThaw()
        {
            if (!terrainFrozen) return;
            terrainFrozen = false; Terrain.Clear(); terrainPending = 0;
        }

        static ushort H(float v) => Mathf.FloatToHalf(v);
        static float F(ushort v) => Mathf.HalfToFloat(v);

        void OnTerrainPatch(OriginalTerrain t, OriginalSimulation.TerrainPatch p)
        {
            if (Active || !Runtime || t != Runtime.terrain || Terrain.Capacity == 0 || p.heights == null) return;
            var hm = t.HeightMirror; var dm = t.DeformMirror;
            int n = t.Samples;
            if (hm.Length != n * n) return;
            if (terrainSeen != t) { terrainSeen = t; Terrain.Clear(); terrainPending = 0; terrainFrozen = false; }
            if (!Recordable(true)) { TerrainFreeze(t); return; }
            TerrainThaw();
            float y0 = t.HeightMin, inv = 1f / Mathf.Max(1e-6f, t.HeightSize);
            bool deform = p.deform != null && dm.Length == n * n;
            long head = Terrain.Written;
            Terrain.Put((ushort)p.x0); Terrain.Put((ushort)p.z0); Terrain.Put((ushort)p.w); Terrain.Put((ushort)p.h);
            Terrain.Put(0); Terrain.Put(0);   // (the count, filled below)
            int count = 0;
            for (int r = 0; r < p.h; r++)
            {
                int o = (p.z0 + r) * n + p.x0, i = r * p.w;
                for (int c = 0; c < p.w; c++)
                {
                    // (an unchanged texel is skipped before any conversion, and the halves are made in managed code: a patch of the whole
                    // terrain - the end-of-race replay puts the ruts back at each clip - ran Mathf.FloatToHalf, a native call, twice for each of
                    // its million texels on the main thread, w-gcspikes)
                    float fh = (p.heights[i + c] - y0) * inv - hm[o + c], fd = deform ? p.deform[i + c] - dm[o + c] : 0f;
                    if (fh == 0f && fd == 0f) continue;
                    ushort dh = ReplayParticleRing.HalfOf(fh);
                    ushort dd = deform ? ReplayParticleRing.HalfOf(fd) : (ushort)0;
                    if ((dh & 0x7fff) == 0 && (dd & 0x7fff) == 0) continue;
                    int idx = o + c;
                    Terrain.Put((ushort)idx); Terrain.Put((ushort)(idx >> 16)); Terrain.Put(dh); Terrain.Put(dd);
                    count++;
                }
            }
            if (count == 0) { RewindRing(head); return; }
            terrainTexels += count;
            // the count into the header (the ring is written in order: the header words are still the newest unless it wrapped meanwhile)
            if (Terrain.Valid(head, 6)) { SetWord(head + 4, (ushort)count); SetWord(head + 5, (ushort)(count >> 16)); }
        }

        void SetWord(long at, ushort w) => Terrain.Set(at, w);
        void RewindRing(long to) => Terrain.Rewind(to);

        // ---------------------------------------------------------------------------------------- viewing

        int terrainShown = -1;             // the frame index whose terrain is on screen; Count = the live terrain (the tail applied)
        float[] terrainKeepH, terrainKeepD;  // the live mirrors at Open
        bool terrainTouched;
        int dirtyX0, dirtyZ0, dirtyX1, dirtyZ1, everX0, everZ0, everX1, everZ1;
        int terrainFloor;                  // the oldest frame the rewind can reach (deltas still in the ring)

        void TerrainOpen()
        {
            terrainShown = Buffer.Count;   // live
            terrainTailStart = terrainPending; terrainTailEnd = Terrain.Written;
            terrainTouched = false;
            everX0 = everZ0 = int.MaxValue; everX1 = everZ1 = int.MinValue;
            terrainFloor = 0;
            for (int k = Buffer.Count - 1; k >= 0; k--)
            {
                var f = Buffer[k];
                if (f.terrainCount > 0 && !Terrain.Valid(f.terrainStart, f.terrainCount)) { terrainFloor = k; break; }
            }
        }

        /// The terrain of frame i (moved there from the one on screen by undoing / redoing the frames between).
        void TerrainShow(int i)
        {
            var t = Runtime ? Runtime.terrain : null;
            if (!t || Terrain.Capacity == 0 || terrainShown < 0 || t != terrainSeen) return;
            i = Mathf.Clamp(i, terrainFloor, Buffer.Count - 1);
            if (i == terrainShown) return;
            if (!terrainTouched)
            {
                terrainTouched = true;
                var hm = t.HeightMirror; var dm = t.DeformMirror;
                if (terrainKeepH == null || terrainKeepH.Length != hm.Length) terrainKeepH = new float[hm.Length];
                if (terrainKeepD == null || terrainKeepD.Length != dm.Length) terrainKeepD = new float[dm.Length];
                Array.Copy(hm, terrainKeepH, hm.Length); Array.Copy(dm, terrainKeepD, dm.Length);
            }
            dirtyX0 = dirtyZ0 = int.MaxValue; dirtyX1 = dirtyZ1 = int.MinValue;
            if (terrainFrozen && !terrainFromFrozen)
            {   // the rewind starts from the terrain as the recording stopped (the live one changed unrecorded since)
                terrainFromFrozen = true;
                var hm = t.HeightMirror; var dm = t.DeformMirror; var r = Rings;
                if (r.frozenH.Length == hm.Length) Array.Copy(r.frozenH, hm, hm.Length);
                if (r.frozenD.Length == dm.Length && dm.Length > 0) Array.Copy(r.frozenD, dm, dm.Length);
                int n = t.Samples; dirtyX0 = dirtyZ0 = 0; dirtyX1 = dirtyZ1 = n;
            }
            if (i < terrainShown) for (int k = terrainShown; k > i; k--) TerrainApply(t, k, -1f);
            else for (int k = terrainShown + 1; k <= i; k++) TerrainApply(t, k, 1f);
            terrainShown = i;
            if (dirtyX1 >= dirtyX0)
            {
                t.ReplayUpload(dirtyX0, dirtyZ0, dirtyX1 - dirtyX0, dirtyZ1 - dirtyZ0);
                everX0 = Math.Min(everX0, dirtyX0); everZ0 = Math.Min(everZ0, dirtyZ0); everX1 = Math.Max(everX1, dirtyX1); everZ1 = Math.Max(everZ1, dirtyZ1);
            }
        }

        /// Adds (sign 1) or subtracts (-1) the deltas of frame k (k = Count: the tail after the last frame).
        void TerrainApply(OriginalTerrain t, int k, float sign)
        {
            long start, end;
            if (k >= Buffer.Count) { start = terrainTailStart; end = terrainTailEnd; }
            else { var f = Buffer[k]; start = f.terrainStart; end = f.terrainStart + f.terrainCount; }
            if (end <= start || !Terrain.Valid(start, end - start)) return;
            var hm = t.HeightMirror; var dm = t.DeformMirror;
            bool deform = dm.Length == hm.Length;
            int n = t.Samples;
            long at = start;
            while (at + 6 <= end)
            {
                int x0 = Terrain[at], z0 = Terrain[at + 1], w = Terrain[at + 2], h = Terrain[at + 3];
                int count = Terrain[at + 4] | Terrain[at + 5] << 16;
                at += 6;
                if (count <= 0 || at + count * 4L > end) break;
                dirtyX0 = Math.Min(dirtyX0, x0); dirtyZ0 = Math.Min(dirtyZ0, z0); dirtyX1 = Math.Max(dirtyX1, x0 + w); dirtyZ1 = Math.Max(dirtyZ1, z0 + h);
                for (int j = 0; j < count; j++, at += 4)
                {
                    int idx = Terrain[at] | Terrain[at + 1] << 16;
                    if ((uint)idx >= (uint)hm.Length) continue;
                    hm[idx] += sign * F(Terrain[at + 2]);
                    if (deform) dm[idx] += sign * F(Terrain[at + 3]);
                }
            }
        }

        long terrainTailStart, terrainTailEnd;   // the deltas after the last recorded frame (the tail), as the viewer opened

        void TerrainClose()
        {
            var t = Runtime ? Runtime.terrain : null;
            if (t && terrainTouched && t == terrainSeen && terrainKeepH != null)
            {
                var hm = t.HeightMirror; var dm = t.DeformMirror;
                if (hm.Length == terrainKeepH.Length) Array.Copy(terrainKeepH, hm, hm.Length);
                if (dm.Length == terrainKeepD.Length) Array.Copy(terrainKeepD, dm, dm.Length);
                if (everX1 >= everX0) t.ReplayUpload(everX0, everZ0, everX1 - everX0, everZ1 - everZ0);
            }
            terrainTouched = false; terrainShown = -1; terrainFromFrozen = false;
        }
        bool terrainFromFrozen;   // the viewer put the frozen terrain in the mirrors (TerrainClose puts the live one back)

        /// (probe) the terrain mirrors' height and deformation at a grid index now.
        internal bool TerrainSample(int idx, out float h, out float d)
        {
            h = d = 0f; var t = Runtime ? Runtime.terrain : null;
            if (!t || (uint)idx >= (uint)t.HeightMirror.Length) return false;
            h = t.HeightMirror[idx]; d = t.DeformMirror.Length > idx ? t.DeformMirror[idx] : 0f; return true;
        }

        // ------------------------------------------------------------------------------------------------ mud

        const int MudWords = 7;   // record (+1: 0 = none), amount, wetness, colour r g b a (halves)
        ushort[] liveMud = Array.Empty<ushort>();

        void OnMud(ReflexBikeModel bike, ReflexRiderModel rider, ReflexMudState s)
        {
            if (Active || bike == null || !bike.chassis) return;
            var ch = bike.chassis.transform;
            for (int k = 0; k < actors.Count; k++)
            {
                var a = actors[k];
                if (a.props || Array.IndexOf(a.full, ch) < 0) continue;
                a.mudBike = bike; a.mudRider = rider;
                if (liveMud.Length != actors.Count * MudWords) liveMud = new ushort[actors.Count * MudWords];
                int o = k * MudWords;
                liveMud[o] = (ushort)Mathf.Clamp(s.record + 1, 0, 65535);
                liveMud[o + 1] = H(s.amount.x); liveMud[o + 2] = H(s.wetness.x);
                liveMud[o + 3] = H(s.color.x); liveMud[o + 4] = H(s.color.y); liveMud[o + 5] = H(s.color.z); liveMud[o + 6] = H(s.color.w);
                return;
            }
        }

        void MudFill(ReplayFrame f)
        {
            if (f.mud.Length != actors.Count * MudWords) f.mud = new ushort[actors.Count * MudWords];
            if (liveMud.Length == f.mud.Length) Array.Copy(liveMud, f.mud, f.mud.Length);
            else Array.Clear(f.mud, 0, f.mud.Length);
        }

        int mudShown = -1;
        void MudShow(int i, bool force = false)
        {
            if (Buffer.Count == 0) return;
            i = Mathf.Clamp(i, 0, Buffer.Count - 1);
            if (i == mudShown && !force) return;
            mudShown = i;
            var m = Buffer[i].mud;
            for (int k = 0; k < actors.Count && (k + 1) * MudWords <= m.Length; k++) ApplyMud(k, m, k * MudWords);
        }

        void MudLive()
        {
            for (int k = 0; k < actors.Count && (k + 1) * MudWords <= liveMud.Length; k++) ApplyMud(k, liveMud, k * MudWords);
            mudShown = -1;
        }

        void ApplyMud(int k, ushort[] m, int o)
        {
            var a = actors[k];
            if (a.mudBike == null || m[o] == 0) return;
            float amount = F(m[o + 1]), wet = F(m[o + 2]);
            var s = new ReflexMudState
            {
                record = m[o] - 1, amount = new Vector4(amount, amount, amount, amount), wetness = new Vector4(wet, wet, wet, wet),
                color = new Vector4(F(m[o + 3]), F(m[o + 4]), F(m[o + 5]), F(m[o + 6])),
            };
            ReflexMud.Apply(a.mudBike, a.mudRider, s);
        }
    }
}
