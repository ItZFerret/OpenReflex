// MX vs ATV Untamed converter: ux_freestyle.ramps / repose / widen_carriers (FREESTYLE.md), the freestyle arenas' ramp carriers.
//   UxFreestyle.Ramps          every carried ramp face (terrain/carried_tris.npy) goes into the collision of the converted Untamed
//                              CollisionRSData whose placed collision box (+1 ft, +3 ft vertically) is the smallest holding its centroid:
//                              a new kind-22 asset UXR_<short>_<Untamed id> = its converted collis + its faces in its local frame, with the
//                              stock FMX ramp-top values (DriveOn 1, MXBike, MetalFrame, UseSoftCollision 0). Arena v3 re-pose: a carrier
//                              whose drawn deck sits off its riding faces (p95 > 0.075 ft) is moved by the best-fit translation (scipy
//                              Powell, ported exactly in UxFreestyle.Fit.cs) when that at least halves the misfit; the faces stay put.
//   UxFreestyle.WidenCarriers  bounds-only model copies UXR_<model> for carriers whose model bounds do not hold their merged collision.
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    public static partial class UxFreestyle
    {
        public const double BOUNDS_TOL_FT = 0.05;          // = ux_verify V6.2's tolerance
        public const string STOCK_REF = "FMX_DLC03_FunFactory DLC_RampSKickerDouble_top / DLC_Ramp_360_Double / DO_Ramp360_COL_Top " +
            "(DriveOn 1, MXBike 0, MetalFrame 13, UseSoftCollision 0)";
        static readonly string[] OLD_KEYS = { "FemaleCollisionAsset", "MaleCollisionAsset", "DriveOn", "PhysicsType", "CollisionAudioMaterial", "UseSoftCollision" };

        /// RAMP_VALUES (a fresh copy).
        static JObj RampValues() => new JObj().Set("DriveOn", 1L).Set("PhysicsType", new JObj().Set("Enum", "MXBike").Set("EnumValue", 0L))
            .Set("CollisionAudioMaterial", new JObj().Set("Enum", "MetalFrame").Set("EnumValue", 13L)).Set("UseSoftCollision", 0L);

        static string G(double v) => v.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
        static double[] Matrix(UxEntities.PropMap P)
        {
            var l = (System.Collections.IList)UxEntities.GetValue(P["Matrix"]);
            var m = new double[16]; for (int i = 0; i < 16; i++) m[i] = UxJson.Num(l[i]);
            return m;
        }
        static double[] Rot(double[] M) { var R = new double[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[3 * i + j] = M[4 * i + j]; return R; }
        /// The triangles of kind-22 clusters: (c + t) in float32, as float64 (np.concatenate([c + t for c, t in cl]).astype(np.float64)).
        static double[] ClusterTris(List<UxRmesh.Cluster> cl)
        {
            int n = 0; foreach (var c in cl) n += c.t.Length;
            var T = new double[n]; int at = 0;
            foreach (var c in cl) for (int i = 0; i < c.t.Length; i++) T[at++] = (float)(c.c[i % 3] + c.t[i]);
            return T;
        }

        sealed class Cand { public UxElem e; public UxEntities.PropMap P; public double[] T, M, lo, hi; public double vol; public string collis; }

        /// ramps(items, lv, sources, lift): rewrites the ramp carriers of items [(subscene, entity)] in place; returns (record, assets).
        public static (JObj record, UxAssetMap assets) Ramps(List<KeyValuePair<string, UxElem>> items, UxContext ctx, UxSources sources, double lift,
            Action<string> log = null, CancellationToken cancel = default)
        {
            var lv = ctx.lv;
            var (ids, W) = CarriedWorld(ctx);
            var rec = new JObj().Set("schema", SCHEMA).Set("carried_tris", (long)ids.Length)
                .Set("rule", "smallest placed collision box (+1 ft, <= 200 ft wide; 3 ft vertically) holding the face centroid")
                .Set("values", RampValues()).Set("stock_reference", STOCK_REF).Set("carriers", new List<object>());
            var assets = new UxAssetMap();
            if (ids.Length == 0) return (rec, assets);
            int nt = ids.Length;
            var Wl = new double[W.Length];
            for (int i = 0; i < W.Length; i += 3) { Wl[i] = W[i] + 0.0; Wl[i + 1] = W[i + 1] + lift; Wl[i + 2] = W[i + 2] + 0.0; }
            var cen = Centroids(Wl);
            var lj = ctx.work.ReadJsonObj("level.json");
            var untamed = new Dictionary<long, JObj>();
            foreach (object x in UxJson.Arr(lj.Get("entities"))) { var ue = UxJson.Obj(x); untamed[UxJson.Int(ue.Get("id"))] = ue; }
            var cands = new List<Cand>();
            foreach (var it in items)
            {
                var e = it.Value;
                if (it.Key != "Base" || UxPcScene.Ns(e) != "CollisionRSData") continue;
                var P = UxEntities.PropsOf(e);
                if (!untamed.ContainsKey(UxJson.Int(UxEntities.GetValue(P["Id"])) - UxEntities.ID_OFFSET)) continue;   // not the terrain parts UXT_*
                var fa = P.Has("FemaleCollisionAsset") ? UxEntities.GetValue(P["FemaleCollisionAsset"]) as JObj : null;
                string fan = fa?.Get("asset") as string;
                if (string.IsNullOrEmpty(fan)) continue;
                if (!sources.assets.Has((fan, "collis"))) throw new UxException("FreestyleError: collis " + fan + " of " + UxEntities.GetValue(P["Name"]) + " not in the stage outputs");
                byte[] b = sources.Read((fan, "collis"));
                var cl = UxRmesh.ReadCollis(b, out uint k);
                if (k != 22 || cl == null || cl.Count == 0) continue;                  // primitive props never carry a ramp face
                double[] T = ClusterTris(cl), M = Matrix(P);
                var box = PlacedBox(T, M);
                if (box == null) continue;
                cands.Add(new Cand { e = e, P = P, T = T, M = M, lo = box.Value.lo, hi = box.Value.hi, vol = box.Value.vol, collis = fan });
            }
            var boxes = new List<(double[] lo, double[] hi, double vol)>(); foreach (var c in cands) boxes.Add((c.lo, c.hi, c.vol));
            var own = Owners(cen, boxes);
            var bad = new List<string>();
            for (int i = 0; i < nt; i++) if (own[i] < 0) bad.Add("[" + G(UxNp.Round(cen[3 * i], 1)) + ", " + G(UxNp.Round(cen[3 * i + 1], 1)) + ", " + G(UxNp.Round(cen[3 * i + 2], 1)) + "]");
            if (bad.Count > 0) throw new UxException("FreestyleError: " + bad.Count + " carried ramp faces in no ramp collision box, e.g. centroids [" + string.Join(", ", bad.GetRange(0, Math.Min(4, bad.Count))) + "]");
            var carriers = (List<object>)rec.Get("carriers");
            for (int j = 0; j < cands.Count; j++)
            {
                cancel.ThrowIfCancellationRequested();
                var sel = new List<int>(); for (int i = 0; i < nt; i++) if (own[i] == j) sel.Add(i);
                if (sel.Count == 0) continue;
                var c = cands[j]; var P = c.P; double[] M = c.M;
                var F = new double[9 * sel.Count];
                for (int s = 0; s < sel.Count; s++) Array.Copy(Wl, 9 * sel[s], F, 9 * s, 9);
                long eid = UxJson.Int(UxEntities.GetValue(P["Id"])), uid = eid - UxEntities.ID_OFFSET;
                var (t, rp) = Repose(ctx, untamed.TryGetValue(uid, out var uent) ? uent : null, M, F, log);
                if (t != null)
                {
                    var M2 = (double[])M.Clone();
                    for (int a = 0; a < 3; a++) M2[12 + a] += t[a];
                    var vals = new List<object>(); foreach (double v in M2) vals.Add(v);
                    UxEntities.SetValue(P["Matrix"], vals);
                    M = Matrix(P);                                                           // the stored (float32) pose
                    rp.Set("matrix_translation_written", new List<object> { M[12], M[13], M[14] });
                }
                var Rinv = Inv3(Rot(M));
                var rel = new double[F.Length];
                for (int i = 0; i < F.Length; i += 3) for (int a = 0; a < 3; a++) rel[i + a] = F[i + a] - M[12 + a];
                var loc = UxNp.MatMul(rel, F.Length / 3, 3, Rinv, 3);
                var Tall = new double[c.T.Length + loc.Length];
                Array.Copy(c.T, Tall, c.T.Length); Array.Copy(loc, 0, Tall, c.T.Length, loc.Length);
                var clusters = UxTerrainStage.ClusterMesh(Tall, out int nSplit);
                byte[] col = UxRmesh.WriteCollis(clusters);
                var back = UxRmesh.ReadCollis(col, out uint k2);
                int nb = 0; if (back != null) foreach (var q in back) nb += q.TriCount;
                if (k2 != 22 || nb != nSplit) throw new UxException("FreestyleError: carrier collis read-back: kind " + k2 + ", " + nb + " of " + nSplit + " triangles");
                string name = PREFIX + lv.shortName + "_" + uid.ToString(System.Globalization.CultureInfo.InvariantCulture);
                var old = new JObj();
                foreach (string key in OLD_KEYS) if (P.Has(key)) old.Set(key, UxEntities.GetValue(P[key]));
                foreach (string key in new[] { "FemaleCollisionAsset", "MaleCollisionAsset" })
                {
                    if (!P.Has(key)) throw new UxException("FreestyleError: " + UxEntities.GetValue(P["Name"]) + " has no " + key);
                    UxEntities.SetValue(P[key], new JObj().Set("asset", name).Set("type", "collis"));
                }
                foreach (var kv in RampValues().items)
                {
                    if (!P.Has(kv.Key)) throw new UxException("FreestyleError: " + UxEntities.GetValue(P["Name"]) + " has no " + kv.Key + " prop");
                    UxEntities.SetValue(P[kv.Key], kv.Value);
                }
                assets.Set(name, "collis", col, "work/" + lv.disc + "/terrain/carried_tris.npy + " + c.collis + ".collis",
                    "ux_freestyle carrier (converted collis + carried ramp faces, kind 22)");
                double top = double.NegativeInfinity; for (int i = 1; i < F.Length; i += 3) if (F[i] > top) top = F[i];
                carriers.Add(new JObj().Set("id", eid).Set("untamed_id", uid).Set("name", UxEntities.GetValue(P["Name"])).Set("collis", name).Set("source_collis", c.collis)
                    .Set("source_tris", (long)(c.T.Length / 9)).Set("carried_tris", (long)sel.Count).Set("collision_tris", (long)nSplit).Set("clusters", (long)clusters.Count)
                    .Set("md5", UxBin.Md5(col)).Set("carried_top_ft", UxNp.PyRound(top, 3)).Set("old", old).Set("repose", rp));
            }
            rec.Set("carrier_count", (long)carriers.Count);
            var reposed = new List<object>(); foreach (JObj c in carriers) if (Equals(((JObj)c.Get("repose")).Get("applied"), true)) reposed.Add(c.Get("untamed_id"));
            rec.Set("reposed", reposed);
            var byName = new SortedDictionary<string, long>(StringComparer.Ordinal);
            foreach (JObj c in carriers) { string n = c.Get("name") as string; byName[n] = (byName.TryGetValue(n, out long v) ? v : 0) + 1; }
            var bn = new JObj(); var bnText = new List<string>();
            foreach (var kv in byName) { bn.Set(kv.Key, kv.Value); bnText.Add("'" + kv.Key + "': " + kv.Value); }
            rec.Set("by_name", bn);
            log?.Invoke("freestyle ramps: " + nt + " carried faces on " + carriers.Count + " carriers {" + string.Join(", ", bnText) + "}");
            return (rec, assets);
        }

        /// widen_bounds(model, lo, hi): the model bytes with only the bounds changed (radius 0xF4, centre 0xF8, extents 0x108; w kept).
        public static byte[] WidenBounds(byte[] model, double[] lo, double[] hi)
        {
            var b = (byte[])model.Clone();
            var c = new double[3]; var e = new double[3];
            for (int a = 0; a < 3; a++) { c[a] = (lo[a] + hi[a]) / 2; e[a] = (hi[a] - lo[a]) / 2; }
            UxBin.PutF32(b, 0xF4, (float)UxNp.Norm(e));
            for (int a = 0; a < 3; a++) { UxBin.PutF32(b, 0xF8 + 4 * a, (float)c[a]); UxBin.PutF32(b, 0x108 + 4 * a, (float)e[a]); }
            return b;
        }

        /// widen_carriers(entities, rec, read_model, read_collis): bounds-only UXR_<model> copies for carriers whose model bounds (+ 0.05 ft)
        /// do not hold their merged collision; sets bounds_over_ft / model_variant on rec's carriers; returns the new assets.
        public static UxAssetMap WidenCarriers(List<UxElem> entities, JObj rec, Func<string, byte[]> readModel, Func<string, byte[]> readCollis, Action<string> log = null)
        {
            var byId = new Dictionary<long, JObj>();
            if (rec.Get("carriers") is List<object> cs) foreach (JObj c in cs) byId[UxJson.Int(c.Get("id"))] = c;
            var assets = new UxAssetMap();
            foreach (var e in entities)
            {
                var P = UxEntities.PropsOf(e);
                if (!P.Has("Id") || !byId.TryGetValue(UxJson.Int(UxEntities.GetValue(P["Id"])), out var c)) continue;
                string ma = (string)((JObj)UxEntities.GetValue(P["ModelAsset"])).Get("asset");
                byte[] mb = readModel(ma) ?? throw new UxException("FreestyleError: model " + ma + " of " + c.Get("name") + " not found");
                var m = UxRmesh.ReadModel(mb);
                var cl = UxRmesh.ReadCollis(readCollis((string)c.Get("collis")), out _);
                double[] pts = ClusterTris(cl);
                double over = double.NegativeInfinity;
                var lo = new double[3]; var hi = new double[3];
                for (int a = 0; a < 3; a++) { lo[a] = double.PositiveInfinity; hi[a] = double.NegativeInfinity; }
                for (int i = 0; i < pts.Length; i += 3)
                    for (int a = 0; a < 3; a++)
                    {
                        double v = pts[i + a];
                        double d = Math.Abs(v - m.center[a]) - m.extents[a];
                        if (d > over || double.IsNaN(d)) over = d;
                        if (v < lo[a]) lo[a] = v; if (v > hi[a]) hi[a] = v;
                    }
                c.Set("bounds_over_ft", UxNp.PyRound(over, 4));
                c.Set("model_variant", null);
                if (over <= BOUNDS_TOL_FT) continue;
                for (int a = 0; a < 3; a++)
                {
                    double clo = (double)m.center[a] - m.extents[a], chi = (double)m.center[a] + m.extents[a];
                    lo[a] = Math.Min(lo[a], clo); hi[a] = Math.Max(hi[a], chi);
                }
                byte[] vb = WidenBounds(mb, lo, hi);
                string variant = PREFIX + ma;
                if (assets.Has(variant, "model") && !UxBin.Same(assets.Get(variant, "model").data, vb))
                    variant = PREFIX + ma + "_" + UxJson.Int(c.Get("untamed_id")).ToString(System.Globalization.CultureInfo.InvariantCulture);
                assets.Set(variant, "model", vb, ma + ".model (bounds widened)", "ux_freestyle carrier model variant (bounds = render U carried collision)");
                UxEntities.SetValue(P["ModelAsset"], new JObj().Set("asset", variant).Set("type", "model"));
                c.Set("model_variant", variant);
            }
            var names = new SortedSet<string>(StringComparer.Ordinal); int n = 0;
            if (rec.Get("carriers") is List<object> cs2) foreach (JObj c in cs2) if (c.Get("model_variant") is string s) { n++; names.Add(s); }
            var q = new List<string>(); foreach (string s in names) q.Add("'" + s + "'");
            log?.Invoke("freestyle ramps: " + n + " carrier model variants (bounds widened over 0.05 ft): [" + string.Join(", ", q) + "]");
            return assets;
        }

        /// np.linalg.inv of a 3 x 3 float64 matrix: the dgesv port of UxMeshes (LU with partial pivoting, then the triangular solves).
        static double[] Inv3(double[] R)
        {
            const int n = 3;
            var A = (double[])R.Clone(); var piv = new[] { 0, 1, 2 };
            for (int k = 0; k < n; k++)
            {
                int p = k;
                for (int i = k + 1; i < n; i++) if (Math.Abs(A[i * n + k]) > Math.Abs(A[p * n + k])) p = i;
                if (A[p * n + k] == 0) throw new UxException("Singular matrix");
                if (p != k)
                {
                    for (int j = 0; j < n; j++) { double t = A[k * n + j]; A[k * n + j] = A[p * n + j]; A[p * n + j] = t; }
                    int tp = piv[k]; piv[k] = piv[p]; piv[p] = tp;
                }
                for (int i = k + 1; i < n; i++)
                {
                    A[i * n + k] = A[i * n + k] / A[k * n + k];
                    for (int j = k + 1; j < n; j++) A[i * n + j] = A[i * n + j] - A[i * n + k] * A[k * n + j];
                }
            }
            var X = new double[9]; var b = new double[3];
            for (int c = 0; c < n; c++)
            {
                for (int i = 0; i < n; i++) b[i] = piv[i] == c ? 1.0 : 0.0;
                for (int i = 0; i < n; i++) for (int j = 0; j < i; j++) b[i] = b[i] - A[i * n + j] * b[j];
                for (int i = n - 1; i >= 0; i--)
                {
                    for (int j = i + 1; j < n; j++) b[i] = b[i] - A[i * n + j] * b[j];
                    b[i] = b[i] / A[i * n + i];
                }
                for (int i = 0; i < n; i++) X[i * n + c] = b[i];
            }
            return X;
        }
    }
}
