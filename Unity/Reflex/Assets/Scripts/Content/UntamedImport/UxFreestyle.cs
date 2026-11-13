// MX vs ATV Untamed converter: the port of the Python reference converter's ux_freestyle.py (FREESTYLE.md), the ramp carriers of the four
// freestyle arenas (RBWCenter, Southbend, THQSpectrum, WestLake).
//   UxFreestyle.CarriedWorld(ctx)     carried_world(lv): the collision-only ramp faces the terrain stage left out of the HF
//                                     (terrain/carried_tris.npy) as Untamed-frame triangles
//   UxFreestyle.RampMaterials(ctx)    ramp_materials(lv): the MaterialList of the Untamed entities that carry ramp faces (the materials
//                                     stage's 'program colour' rule applies to these only)
//   UxFreestyle.Ramps / WidenCarriers ramps() / widen_carriers(): UxFreestyle.Ramps.cs
// Untamed's freestyle arenas place the steel ramps (FS_kicker_Axcal, FS_360_Axcal) and RBW's funbox as CollisionRSData whose own collis
// holds only the side walls; the riding faces are collision-only triangles of the terrain collision (no render twin). The terrain stage
// leaves them out of the HF of a freestyle level and this module gives them to the ramp that stands on them.
// Pure C# (System only).
using System;
using System.Collections.Generic;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_freestyle.py.
    public static partial class UxFreestyle
    {
        public const string SCHEMA = "ux_freestyle/1";
        public const string PREFIX = "UXR_";
        public const double BOX_PAD_FT = 1.0;              // horizontal pad of a carrier's placed collision box
        public const double BOX_PAD_Y_FT = 3.0;            // vertical pad (RBW's funbox: 16 faces 1.6 ft over its collision box top)
        public const double MAX_CARRIER_FT = 200.0;
        public const string CARRIED_FILE = "terrain/carried_tris.npy";

        /// carried_world(lv): (trncol triangle ids, Untamed-frame triangles as 9 doubles each) of the level's carried ramp faces; empty when
        /// the terrain stage carried none.
        public static (long[] ids, double[] tris) CarriedWorld(UxContext ctx)
        {
            var work = ctx.work;
            if (!work.Exists(CARRIED_FILE)) return (new long[0], new double[0]);
            double[] raw = UxEntities.ReadNpy(work.ReadBytes(CARRIED_FILE), out _);
            var ids = new long[raw.Length]; for (int i = 0; i < raw.Length; i++) ids[i] = (long)raw[i];
            var tj = work.ReadJsonObj("terrain/terrain.json");
            var lj = work.ReadJsonObj("level.json");
            var trncols = UxJson.Obj(UxJson.Obj(lj.Get("closure")).Get("trncol"));
            string want = tj.Get("trncol") as string;
            JObj rec = want != null && trncols.Has(want) ? UxJson.Obj(trncols.Get(want)) : UxJson.Obj(trncols.items[0].Value);
            var c = UxTerrain.ReadTrncol(ctx.disc.ReadExtractFile(UxJson.Str(rec.Get("file")), ctx.cancel));
            var T = new double[9 * ids.Length];
            for (int i = 0; i < ids.Length; i++)
            {
                if (ids[i] < 0 || ids[i] >= c.ntris) throw new UxException("carried_tris.npy: triangle " + ids[i] + " outside the trncol (" + c.ntris + ")");
                for (int k = 0; k < 3; k++)
                {
                    int v = c.tris[3 * ids[i] + k];
                    for (int a = 0; a < 3; a++) T[9 * i + 3 * k + a] = c.verts[3 * v + a];
                }
            }
            return (ids, T);
        }

        /// _ref(ue, prop): the first refs entry's name for prop, or null.
        internal static string Ref(JObj ue, string prop)
        {
            if (!(ue.Get("refs") is List<object> refs)) return null;
            foreach (object r in refs) if (r is List<object> l && l.Count >= 2 && l[0] as string == prop) return l[1] as string;
            return null;
        }

        /// The (lo, hi, volume) of triangles (9 doubles each) placed by a row-major 4 x 4 matrix (T @ M[:3,:3] + M[3,:3], BLAS rounding),
        /// padded by BOX_PAD_FT / BOX_PAD_Y_FT; null when the box is wider than MAX_CARRIER_FT.
        internal static (double[] lo, double[] hi, double vol)? PlacedBox(double[] T, double[] M)
        {
            int n = T.Length / 3;
            var R = new double[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[3 * i + j] = M[4 * i + j];
            var W = UxNp.MatMul(T, n, 3, R, 3);
            var lo = new[] { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity };
            var hi = new[] { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
            for (int i = 0; i < n; i++)
                for (int a = 0; a < 3; a++)
                {
                    double v = W[3 * i + a] + M[12 + a];
                    if (v < lo[a]) lo[a] = v;
                    if (v > hi[a]) hi[a] = v;
                }
            var pad = new[] { BOX_PAD_FT, BOX_PAD_Y_FT, BOX_PAD_FT };
            for (int a = 0; a < 3; a++) { lo[a] -= pad[a]; hi[a] += pad[a]; }
            if (Math.Max(hi[0] - lo[0], hi[2] - lo[2]) > MAX_CARRIER_FT) return null;
            double vol = (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]);
            return (lo, hi, vol);
        }

        /// W.mean(1) of triangles (9 doubles each): the centroids ((a + b) + c) / 3.
        internal static double[] Centroids(double[] T)
        {
            int n = T.Length / 9; var c = new double[3 * n];
            for (int i = 0; i < n; i++) for (int a = 0; a < 3; a++) c[3 * i + a] = (T[9 * i + a] + T[9 * i + 3 + a] + T[9 * i + 6 + a]) / 3.0;
            return c;
        }

        /// The owner of every centroid: the index of the smallest box (strictly smaller volume wins, boxes in order) holding it, or -1.
        internal static int[] Owners(double[] cen, IList<(double[] lo, double[] hi, double vol)> boxes)
        {
            int n = cen.Length / 3; var own = new int[n]; var best = new double[n];
            for (int i = 0; i < n; i++) { own[i] = -1; best[i] = double.PositiveInfinity; }
            for (int j = 0; j < boxes.Count; j++)
            {
                var (lo, hi, vol) = boxes[j];
                for (int i = 0; i < n; i++)
                {
                    bool inside = true;
                    for (int a = 0; a < 3 && inside; a++) inside = cen[3 * i + a] >= lo[a] && cen[3 * i + a] <= hi[a];
                    if (inside && vol < best[i]) { own[i] = j; best[i] = vol; }
                }
            }
            return own;
        }

        /// ramp_materials(lv): sorted Untamed material names of the MaterialLists of the kept Untamed CollisionRSData entities whose placed
        /// collision box is the smallest holding a carried ramp face (the Ramps carrier rule on the Untamed data, lift-free). Empty off the
        /// freestyle levels.
        public static List<string> RampMaterials(UxContext ctx)
        {
            var o = new List<string>();
            if (!UxCommon.IsFmx(ctx.lv)) return o;
            var (ids, W) = CarriedWorld(ctx);
            if (ids.Length == 0) return o;
            var lj = ctx.work.ReadJsonObj("level.json");
            var collis = UxJson.Obj(lj.Get("closure")).Get("collis") as JObj;
            var cen = Centroids(W);
            var boxes = new List<(double[] lo, double[] hi, double vol)>(); var ents = new List<JObj>();
            foreach (object x in UxJson.Arr(lj.Get("entities")))
            {
                var ue = UxJson.Obj(x);
                string cls = ue.Get("cls") as string;
                if (ue.Get("type") as string != "CollisionRSData" || (cls != "keep" && cls != "replace")) continue;
                string fname = Ref(ue, "FemaleCollisionAsset");
                var cr = fname != null && collis != null && collis.Has(fname) ? collis.Get(fname) as JObj : null;
                if (cr == null) continue;
                double[] T = UxMesh.CollisTriangles(ctx.disc.ReadExtractFile(UxJson.Str(cr.Get("file")), ctx.cancel));
                if (T == null || T.Length == 0) continue;
                var M = new double[16]; var ml = UxJson.Arr(ue.Get("matrix")); for (int i = 0; i < 16; i++) M[i] = UxJson.Num(ml[i]);
                var box = PlacedBox(T, M);
                if (box == null) continue;
                boxes.Add(box.Value); ents.Add(ue);
            }
            var own = Owners(cen, boxes);
            var mats = new SortedSet<string>(StringComparer.Ordinal);
            var seen = new HashSet<int>();
            foreach (int j in own)
            {
                if (j < 0 || !seen.Add(j)) continue;
                if (ents[j].Get("refs") is List<object> refs)
                    foreach (object r in refs) if (r is List<object> l && l.Count >= 2 && l[0] as string == "MaterialList" && l[1] is string m) mats.Add(m);
            }
            o.AddRange(mats);
            return o;
        }
    }
}
