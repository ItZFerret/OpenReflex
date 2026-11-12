// MX vs ATV Untamed converter, stage "race" (S8): the port of the Python reference converter's ux_startpack.py (fix round 'fixrace', DESIGN 21).
// Start-pack bales: PopDown bales standing on the berm at the OUTSIDE of a start-chute turn, where the AI start pack ran into them.
//   classify      the class: a PopDown footprint point between 12 ft inside and `out_ft` outside a start-chute edge (TDF 22 / 23) at the
//                 outside of a >= 20 deg turn whose edge lies >= 3 ft over the node centre on the height field
//   EdgeClear     --bale-edge-clear FT (ux_race, before the deck no-respawn pass): the edge points next to the class bales (22 / 23 and
//                 their 44 / 45 copies) move toward the node centre in 0.25 ft steps until each authored footprint is FT ft outside
//   Reseat        --bale-reseat FT (ux_entities): each class bale's move away from the nearest chute centre node
// Pure C# (System only); numpy's element-wise and BLAS rules as in UxRaceNum.
using System;
using System.Collections.Generic;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_startpack.py.
    public static class UxStartPack
    {
        public const double TURN_DEG = 20.0, TURN_SPAN_FT = 40.0, BERM_FT = 3.0, CLASS_OUT_FT = 4.0, CLASS_IN_FT = 12.0, T_TOL = 0.05, STEP_FT = 0.25;
        public const int MAX_STEPS = 400;
        public const double MIN_HALF_FT = 8.0, MIN_HALF_FRAC = 0.5;
        static readonly string[] POP_DOWN = { "PopDownBaleRSData" };

        /// One bale of footprints(): the Untamed id, the unique xz of its collision triangles (rows sorted by x, then z) and the float32
        /// matrix translation x / z.
        internal sealed class Bale { public long id; public double[] px, pz; public double cx, cz; }

        /// footprints(lv, d, cw, types): the kept entities of `types` that carry collision triangles.
        internal static List<Bale> Footprints(List<UxRace.LevelEntity> ents, UxRace.CollisionWorld cw, string[] types = null)
        {
            types = types ?? POP_DOWN;
            var o = new List<Bale>();
            foreach (var e in ents)
            {
                if (!e.Kept || Array.IndexOf(types, e.reflex) < 0 && Array.IndexOf(types, e.type) < 0) continue;
                var b = MakeBale(e.id, cw.tris, cw.owner, (double)(float)UxJson.Num(e.matrix[12]), (double)(float)UxJson.Num(e.matrix[14]));
                if (b != null) o.Add(b);
            }
            return o;
        }
        /// One footprints() row: the unique xz (rounded to 4 decimals, rows in lexicographic order: np.unique(axis=0)) of the world
        /// collision triangles `tris` (n x 9) owned by `id`, and the float32 matrix translation x / z. null when it owns no triangle.
        internal static Bale MakeBale(long id, double[] tris, long[] owner, double cx, double cz)
        {
            var pts = new List<(double x, double z)>();
            for (int i = 0; i < owner.Length; i++)
                if (owner[i] == id)
                    for (int v = 0; v < 3; v++) pts.Add((UxNp.Round(tris[9 * i + 3 * v], 4), UxNp.Round(tris[9 * i + 3 * v + 2], 4)));
            if (pts.Count == 0) return null;
            pts.Sort((a, b) => a.x != b.x ? a.x.CompareTo(b.x) : a.z.CompareTo(b.z));
            var px = new List<double>(); var pz = new List<double>();
            for (int i = 0; i < pts.Count; i++)
                if (i == 0 || pts[i].x != pts[i - 1].x || pts[i].z != pts[i - 1].z) { px.Add(pts[i].x); pz.Add(pts[i].z); }
            return new Bale { id = id, px = px.ToArray(), pz = pz.ToArray(), cx = cx, cz = cz };
        }

        /// One start chute of chute_models(): the centre nodes C, the edges 22 / 23, the path length, the turn at each node, its outside
        /// edge, whether the node is shared with another section, and the berm height of each edge over the centre.
        internal sealed class ChuteModel
        {
            public int si, m; public double[] cx, cz, cum, turn; public double[][] ex = new double[2][], ez = new double[2][];
            public int[] outside; public bool[] frozen; public double?[][] berm = new double?[2][];
            public static int K(int side) => side == 22 ? 0 : 1;
        }

        static (long, long) Key(double x, double z) => (BitConverter.DoubleToInt64Bits(x == 0 ? 0.0 : x), BitConverter.DoubleToInt64Bits(z == 0 ? 0.0 : z));

        /// chute_models(secs, hf). hf: (x, z) -> the dirt height in ft, or null (the height field of the level).
        internal static List<ChuteModel> ChuteModels(List<UxTdfSection> secs, Func<double, double, double?> hf)
        {
            var allc = new Dictionary<(long, long), HashSet<int>>();
            for (int si = 0; si < secs.Count; si++)
                foreach (var nd in secs[si].nodes)
                {
                    var c = UxTdf07.Vec3(UxTdf07.Field(UxTdf07.NodeDict(nd), 21));
                    var key = Key(UxNp.PyRound(c[0], 2), UxNp.PyRound(c[2], 2));
                    if (!allc.TryGetValue(key, out var set)) allc[key] = set = new HashSet<int>();
                    set.Add(si);
                }
            var models = new List<ChuteModel>();
            for (int si = 0; si < secs.Count; si++)
            {
                var sc = secs[si];
                if (UxTdf07.SectionKind(sc) != 4) continue;
                int m = sc.nodes.Count;
                if (m < 2) continue;
                var M = new ChuteModel { si = si, m = m, cx = new double[m], cz = new double[m], cum = new double[m], turn = new double[m], outside = new int[m], frozen = new bool[m] };
                for (int k = 0; k < 2; k++) { M.ex[k] = new double[m]; M.ez[k] = new double[m]; M.berm[k] = new double?[m]; }
                for (int i = 0; i < m; i++)
                {
                    var N = UxTdf07.NodeDict(sc.nodes[i]);
                    var c = UxTdf07.Vec3(UxTdf07.Field(N, 21)); M.cx[i] = c[0]; M.cz[i] = c[2];
                    for (int k = 0; k < 2; k++) { var e = UxTdf07.Vec3(UxTdf07.Field(N, k == 0 ? 22u : 23u)); M.ex[k][i] = e[0]; M.ez[k][i] = e[2]; }
                }
                // concatenate([[0.0], cumsum(hypot(diff(C)))]): np.cumsum starts at its first element (cum[1] = h1, cum[2] = h1 + h2, ...)
                {
                    double s = 0;
                    for (int i = 1; i < m; i++) { double h = UxRaceNum.NpHypot(M.cx[i] - M.cx[i - 1], M.cz[i] - M.cz[i - 1]); s = i == 1 ? h : s + h; M.cum[i] = s; }
                    M.cum[0] = 0.0;
                }
                for (int i = 0; i < m; i++)
                {
                    int a = ArgNear(M.cum, M.cum[i] - TURN_SPAN_FT), b = ArgNear(M.cum, M.cum[i] + TURN_SPAN_FT);
                    double d10, d11, d20, d21;
                    if (i > a) { d10 = M.cx[i] - M.cx[a]; d11 = M.cz[i] - M.cz[a]; } else { int q = Math.Min(i + 1, m - 1); d10 = M.cx[q] - M.cx[i]; d11 = M.cz[q] - M.cz[i]; }
                    if (b > i) { d20 = M.cx[b] - M.cx[i]; d21 = M.cz[b] - M.cz[i]; } else { int q = Math.Max(i - 1, 0); d20 = M.cx[i] - M.cx[q]; d21 = M.cz[i] - M.cz[q]; }
                    double ang = Math.Atan2(d10 * d21 - d11 * d20, UxRaceNum.Dot2(d10, d11, d20, d21)) * (180.0 / Math.PI);
                    M.turn[i] = ang;
                    int qa = Math.Min(i + 1, m - 1), qb = Math.Max(i - 1, 0);
                    double dv0 = M.cx[qa] - M.cx[qb], dv1 = M.cz[qa] - M.cz[qb];
                    double s23 = dv0 * (M.ez[1][i] - M.cz[i]) - dv1 * (M.ex[1][i] - M.cx[i]);
                    M.outside[i] = (s23 > 0) != (ang > 0) ? 23 : 22;                // the turn bends toward +cross: outside = -cross
                    // round() of np.float64 values: numpy's rounding (the keys above are Python's)
                    M.frozen[i] = allc.TryGetValue(Key(UxNp.Round(M.cx[i], 2), UxNp.Round(M.cz[i], 2)), out var set) && set.Count > 1;
                }
                for (int i = 0; i < m; i++)
                {
                    var hc = hf(M.cx[i], M.cz[i]);
                    for (int k = 0; k < 2; k++)
                    {
                        var he = hf(M.ex[k][i], M.ez[k][i]);
                        M.berm[k][i] = hc != null && he != null ? he.Value - hc.Value : (double?)null;
                    }
                }
                models.Add(M);
            }
            return models;
        }
        /// int(np.argmin(np.abs(cum - target))): the first of the nearest.
        static int ArgNear(double[] cum, double target)
        {
            int best = 0; double bd = Math.Abs(cum[0] - target);
            for (int i = 1; i < cum.Length; i++) { double d = Math.Abs(cum[i] - target); if (d < bd) { bd = d; best = i; } }
            return best;
        }

        /// _normal_out(a, b, e, c): the unit normal of segment a-b pointing away from the corridor (the side of edge point e seen from c).
        static void NormalOut(double ax, double az, double bx, double bz, double ex, double ez, double cx, double cz, out double nx, out double nz)
        {
            double ab0 = bx - ax, ab1 = bz - az;
            double len = Math.Max(UxRaceNum.NpHypot(ab0, ab1), 1e-9);
            nx = ab1 / len; nz = -ab0 / len;
            if (UxRaceNum.Dot2(ex - cx, ez - cz, nx, nz) < 0) { nx = -nx; nz = -nz; }
        }
        /// _signed(p, a, b, n): (the distance of p outside segment a-b along its outward normal n, the projection parameter t).
        static void Signed(double px, double pz, double ax, double az, double bx, double bz, double nx, double nz, out double s, out double t)
        {
            double ab0 = bx - ax, ab1 = bz - az;
            double L2 = UxRaceNum.Dot2(ab0, ab1, ab0, ab1);
            t = L2 == 0 ? 0.0 : UxRaceNum.Dot2(px - ax, pz - az, ab0, ab1) / L2;
            s = UxRaceNum.Dot2(px - ax, pz - az, nx, nz);
        }

        internal struct Sel { public long bid; public int mi, i, side; }

        /// classify(models, bales, out_ft): [(bale id, model index, quad i, side k)] of the class.
        internal static List<Sel> Classify(List<ChuteModel> models, List<Bale> bales, double out_ft = CLASS_OUT_FT)
        {
            var sel = new List<Sel>();
            for (int mi = 0; mi < models.Count; mi++)
            {
                var M = models[mi];
                foreach (var bale in bales)
                    for (int i = 0; i < M.m - 1; i++)
                        foreach (int side in new[] { 22, 23 })
                        {
                            int k = ChuteModel.K(side);
                            double ax = M.ex[k][i], az = M.ez[k][i], bx = M.ex[k][i + 1], bz = M.ez[k][i + 1];
                            NormalOut(ax, az, bx, bz, ax, az, M.cx[i], M.cz[i], out double nx, out double nz);
                            bool near = false;
                            for (int q = 0; q < bale.px.Length && !near; q++)
                            {
                                Signed(bale.px[q], bale.pz[q], ax, az, bx, bz, nx, nz, out double s, out double t);
                                if (-T_TOL <= t && t <= 1 + T_TOL && -CLASS_IN_FT <= s && s < out_ft) near = true;
                            }
                            if (!near) continue;
                            bool hit = false;
                            foreach (int j in new[] { i, i + 1 })
                                if (M.outside[j] == side && Math.Abs(M.turn[j]) >= TURN_DEG && M.berm[k][j] != null && M.berm[k][j].Value >= BERM_FT) { hit = true; break; }
                            if (hit) sel.Add(new Sel { bid = bale.id, mi = mi, i = i, side = side });
                        }
            }
            return sel;
        }

        static Bale BaleOf(List<Bale> bales, long id)
        {
            Bale r = null; foreach (var b in bales) if (b.id == id) r = b;      // {b[0]: b[1] for b in bales}: the last of an id
            return r ?? throw new UxException("start pack: no bale " + id);
        }

        /// class_report(models, bales, sel): per class bale {id, section, side, nodes, depth_ft, turn_deg, berm_ft}.
        internal static List<object> ClassReport(List<ChuteModel> models, List<Bale> bales, List<Sel> sel)
        {
            var order = new List<long>(); var recs = new Dictionary<long, JObj>(); var nodeSets = new Dictionary<long, SortedSet<int>>();
            foreach (var x in sel)
            {
                var M = models[x.mi]; int k = ChuteModel.K(x.side), i = x.i;
                double ax = M.ex[k][i], az = M.ez[k][i], bx = M.ex[k][i + 1], bz = M.ez[k][i + 1];
                NormalOut(ax, az, bx, bz, ax, az, M.cx[i], M.cz[i], out double nx, out double nz);
                double? dep = null; var bale = BaleOf(bales, x.bid);
                for (int q = 0; q < bale.px.Length; q++)
                {
                    Signed(bale.px[q], bale.pz[q], ax, az, bx, bz, nx, nz, out double s, out double t);
                    if (-T_TOL <= t && t <= 1 + T_TOL) dep = dep == null ? -s : (-s > dep.Value ? -s : dep.Value);
                }
                if (!recs.TryGetValue(x.bid, out var r))
                {
                    r = new JObj().Set("id", x.bid).Set("section", (long)M.si).Set("side", (long)x.side).Set("nodes", new List<object>()).Set("depth_ft", null).Set("turn_deg", null).Set("berm_ft", null);
                    recs[x.bid] = r; order.Add(x.bid); nodeSets[x.bid] = new SortedSet<int>();
                }
                nodeSets[x.bid].Add(i); nodeSets[x.bid].Add(i + 1);
                var nl = new List<object>(); foreach (int n in nodeSets[x.bid]) nl.Add((long)n);
                r.Set("nodes", nl);
                if (dep != null)
                {
                    object cur = r.Get("depth_ft");
                    r.Set("depth_ft", cur == null ? UxNp.PyRound(dep.Value, 2) : UxNp.PyRound(dep.Value > (double)cur ? dep.Value : (double)cur, 2));
                }
                double tj = Math.Abs(M.turn[i + 1]) > Math.Abs(M.turn[i]) ? M.turn[i + 1] : M.turn[i];        // max(..., key=abs): the first of equals
                double b0 = M.berm[k][i] ?? 0.0, b1 = M.berm[k][i + 1] ?? 0.0;
                if (b0 == 0) b0 = 0.0; if (b1 == 0) b1 = 0.0;                                                 // (x or 0.0)
                double bj = b1 > b0 ? b1 : b0;
                object ct = r.Get("turn_deg");
                if (ct == null || Math.Abs(tj) > Math.Abs((double)ct)) r.Set("turn_deg", UxNp.PyRound(tj, 1));
                object cb = r.Get("berm_ft");
                if (cb == null || bj > (double)cb) r.Set("berm_ft", UxNp.PyRound(bj, 2));
            }
            var o = new List<object>(); foreach (long id in order) o.Add(recs[id]);
            return o;
        }

        /// edge_clear(data, bales, hf, clear_ft): --bale-edge-clear FT on a tdf. Returns `data` itself when nothing moves; rep is the report.
        internal static byte[] EdgeClear(byte[] data, List<Bale> bales, Func<double, double, double?> hf, double clear_ft, out JObj rep)
        {
            var problems = new List<object>(); var nodes = new List<object>();
            rep = new JObj().Set("option", clear_ft)
                .Set("rule", "class: PopDown footprint within " + UxRaceNum.PyG(clear_ft) + " ft outside .. " + UxRaceNum.PyG(CLASS_IN_FT) + " ft inside a start-chute edge at the outside of a >= " +
                    UxRaceNum.PyG(TURN_DEG) + " deg turn (+-" + UxRaceNum.PyG(TURN_SPAN_FT) + " ft) whose edge lies >= " + UxRaceNum.PyG(BERM_FT) +
                    " ft over the centre on the HF; edges pulled toward the centre until the authored footprint is >= " + UxRaceNum.PyG(clear_ft) + " ft outside")
                .Set("bales", new List<object>()).Set("nodes", nodes).Set("problems", problems).Set("changed", 0L);
            if (!(clear_ft > 0)) return data;
            var sp = UxTdf07.Split(data);
            UxTdf07.Structure(sp.items, out var glob, out var secs);
            var models = ChuteModels(secs, hf);
            var sel = Classify(models, bales, clear_ft);
            var report = ClassReport(models, bales, sel);
            rep.Set("bales", report);
            if (sel.Count == 0) return data;
            for (int mi = 0; mi < models.Count; mi++)
            {
                var M = models[mi];
                var msel = new List<Sel>(); foreach (var x in sel) if (x.mi == mi) msel.Add(x);
                if (msel.Count == 0) continue;
                var half = new double[2][]; var trim = new double?[2][];
                for (int k = 0; k < 2; k++)
                {
                    half[k] = new double[M.m]; trim[k] = new double?[M.m];
                    for (int i = 0; i < M.m; i++) half[k][i] = UxRaceNum.NpHypot(M.ex[k][i] - M.cx[i], M.ez[k][i] - M.cz[i]);
                }
                void EdgePt(int i, int k, out double x, out double z)
                {
                    double f = (trim[k][i] ?? 0.0) / Math.Max(half[k][i], 1e-9);
                    x = M.ex[k][i] + (M.cx[i] - M.ex[k][i]) * f; z = M.ez[k][i] + (M.cz[i] - M.ez[k][i]) * f;
                }
                var stuck = new List<(Sel key, double worst)>();
                for (int step = 0; step < MAX_STEPS; step++)
                {
                    bool viol = false;
                    foreach (var x in msel)
                    {
                        int i = x.i, k = ChuteModel.K(x.side);
                        EdgePt(i, k, out double ax, out double az); EdgePt(i + 1, k, out double bx, out double bz);
                        NormalOut(ax, az, bx, bz, ax, az, M.cx[i], M.cz[i], out double nx, out double nz);
                        double? worst = null; var bale = BaleOf(bales, x.bid);
                        for (int q = 0; q < bale.px.Length; q++)
                        {
                            Signed(bale.px[q], bale.pz[q], ax, az, bx, bz, nx, nz, out double s, out double t);
                            if (-T_TOL <= t && t <= 1 + T_TOL && s < clear_ft - 1e-6) worst = worst == null ? s : (s < worst.Value ? s : worst.Value);
                        }
                        if (worst == null) continue;
                        bool moved = false;
                        foreach (int j in new[] { i, i + 1 })
                        {
                            if (M.frozen[j]) continue;
                            if (half[k][j] - (trim[k][j] ?? 0.0) - STEP_FT < Math.Max(MIN_HALF_FT, MIN_HALF_FRAC * half[k][j])) continue;
                            trim[k][j] = (trim[k][j] ?? 0.0) + STEP_FT;
                            moved = true;
                        }
                        if (moved) viol = true;
                        else
                        {
                            int at = stuck.FindIndex(s0 => s0.key.bid == x.bid && s0.key.i == x.i && s0.key.side == x.side);
                            if (at >= 0) stuck[at] = (x, worst.Value); else stuck.Add((x, worst.Value));
                        }
                    }
                    if (!viol) break;
                }
                foreach (var (key, w) in stuck)
                    problems.Add("S" + M.si + " n" + key.i + "-" + (key.i + 1) + " edge " + key.side + ": bale " + key.bid + " cannot be cleared to " + UxRaceNum.PyG(clear_ft) + " ft (" + UxRaceNum.PyFixed(w, 2) + " ft)");
                var sc = secs[M.si];
                for (int i = 0; i < M.m; i++)                                       // sorted(trim.items()): by (node, side)
                    foreach (int side in new[] { 22, 23 })
                    {
                        int k = ChuteModel.K(side);
                        if (trim[k][i] == null) continue;
                        double t = trim[k][i].Value;
                        var nd = sc.nodes[i]; var f = UxTdf07.NodeDict(nd);
                        var c3 = UxTdf07.Vec3(UxTdf07.Field(f, 21));
                        foreach (uint tag in side == 22 ? new uint[] { 22, 44 } : new uint[] { 23, 45 })
                        {
                            if (!f.TryGetValue(tag, out var rawTag)) continue;
                            var e3 = UxTdf07.Vec3(rawTag);
                            double h = UxRaceNum.PyHypot(e3[0] - c3[0], e3[2] - c3[2]);
                            double fr = t / Math.Max(h, 1e-9);
                            var nw = new byte[12];
                            for (int q = 0; q < 3; q++) UxBin.PutF32(nw, 4 * q, (float)(e3[q] + (c3[q] - e3[q]) * fr), true);
                            for (int j = 0; j < nd.Count; j++) if (nd[j].id == tag) nd[j] = new UxTdfItem(nd[j].id, nd[j].size, nw);
                        }
                        nodes.Add(new JObj().Set("section", (long)M.si).Set("node", (long)i).Set("side", (long)side).Set("trim_ft", UxNp.PyRound(t, 2))
                            .Set("half_before_ft", UxNp.PyRound(half[k][i], 2)).Set("half_after_ft", UxNp.PyRound(half[k][i] - t, 2)).Set("turn_deg", UxNp.PyRound(M.turn[i], 1))
                            .Set("berm_ft", M.berm[k][i] == null ? null : (object)UxNp.PyRound(M.berm[k][i].Value, 2)));
                    }
            }
            rep.Set("changed", (long)nodes.Count);
            if (nodes.Count == 0) return data;
            var o = UxTdf07.Join(sp.hdr, sp.path, UxTdf07.Flatten(glob, secs), sp.Reflex);
            UxTdf07.Split(o);                                                       // walks to EOF or throws
            if (o.Length != data.Length) throw new UxException("bale edge clear changed the tdf length " + data.Length + " -> " + o.Length);
            var ids = new List<long>(); foreach (object b in report) ids.Add(UxJson.Int(UxJson.Obj(b).Get("id")));
            rep.Set("md5_before", UxBin.Md5(data)).Set("md5_after", UxBin.Md5(o)).Set("min_clearance_ft", UxNp.PyRound(MinClearance(o, bales, ids), 3));
            return o;
        }

        /// min_clearance(data, bales, ids): the smallest distance of a footprint point of the bales `ids` outside the chute corridor edge
        /// segment it projects on (22 / 23 and 44 / 45; negative = inside); +inf when none projects within reach.
        internal static double MinClearance(byte[] data, List<Bale> bales, List<long> ids)
        {
            var sp = UxTdf07.Split(data);
            UxTdf07.Structure(sp.items, out _, out var secs);
            double? best = null;
            foreach (var sc in secs)
            {
                if (UxTdf07.SectionKind(sc) != 4) continue;
                var N = new List<Dictionary<uint, byte[]>>(); foreach (var n in sc.nodes) N.Add(UxTdf07.NodeDict(n));
                UxBin.Require(N.Count > 0, "tdf: a start chute has no node");
                var cx = new double[N.Count]; var cz = new double[N.Count];
                for (int i = 0; i < N.Count; i++) { var c = UxTdf07.Vec3(UxTdf07.Field(N[i], 21)); cx[i] = c[0]; cz[i] = c[2]; }
                foreach (var tags in new[] { new uint[] { 22, 23 }, new uint[] { 44, 45 } })
                {
                    if (!N[0].ContainsKey(tags[0]) || !N[0].ContainsKey(tags[1])) continue;
                    foreach (uint k in tags)
                    {
                        var ex = new double[N.Count]; var ez = new double[N.Count];
                        for (int i = 0; i < N.Count; i++) { var e = UxTdf07.Vec3(UxTdf07.Field(N[i], k)); ex[i] = e[0]; ez[i] = e[2]; }
                        for (int i = 0; i + 1 < N.Count; i++)
                        {
                            NormalOut(ex[i], ez[i], ex[i + 1], ez[i + 1], ex[i], ez[i], cx[i], cz[i], out double nx, out double nz);
                            foreach (long bid in ids)
                            {
                                var bale = BaleOf(bales, bid);
                                for (int q = 0; q < bale.px.Length; q++)
                                {
                                    Signed(bale.px[q], bale.pz[q], ex[i], ez[i], ex[i + 1], ez[i + 1], nx, nz, out double s, out double t);
                                    if (-T_TOL <= t && t <= 1 + T_TOL && -CLASS_IN_FT <= s && s < CLASS_IN_FT) best = best == null ? s : (s < best.Value ? s : best.Value);
                                }
                            }
                        }
                    }
                }
            }
            return best ?? double.PositiveInfinity;
        }

        /// reseat(secs, bales, hf, dist_ft): --bale-reseat FT: {bale id: {unit (dx, dz), dist_ft, from_node}} for every class bale
        /// (out_ft = CLASS_OUT_FT), in bale order, and the class report. Empty for dist_ft <= 0.
        internal static List<KeyValuePair<long, JObj>> Reseat(List<UxTdfSection> secs, List<Bale> bales, Func<double, double, double?> hf, double dist_ft, out List<object> classReport)
        {
            var o = new List<KeyValuePair<long, JObj>>(); classReport = new List<object>();
            if (!(dist_ft > 0)) return o;
            var models = ChuteModels(secs, hf);
            var sel = Classify(models, bales, CLASS_OUT_FT);
            var ids = new HashSet<long>(); foreach (var s in sel) ids.Add(s.bid);
            var cx = new List<double>(); var cz = new List<double>();
            foreach (var M in models) { cx.AddRange(M.cx); cz.AddRange(M.cz); }
            foreach (var bale in bales)
            {
                if (!ids.Contains(bale.id)) continue;
                UxBin.Require(cx.Count > 0, "attempt to get argmin of an empty sequence");
                int k = 0; double bd = UxRaceNum.NpHypot(cx[0] - bale.cx, cz[0] - bale.cz);
                for (int i = 1; i < cx.Count; i++) { double d = UxRaceNum.NpHypot(cx[i] - bale.cx, cz[i] - bale.cz); if (d < bd) { bd = d; k = i; } }
                double v0 = bale.cx - cx[k], v1 = bale.cz - cz[k];
                double len = Math.Max(UxRaceNum.NpHypot(v0, v1), 1e-9);
                v0 = v0 / len; v1 = v1 / len;
                var rec = new JObj().Set("unit", new List<object> { v0, v1 }).Set("dist_ft", dist_ft).Set("from_node", new List<object> { UxNp.PyRound(cx[k], 2), UxNp.PyRound(cz[k], 2) });
                int at = o.FindIndex(x => x.Key == bale.id);
                if (at >= 0) o[at] = new KeyValuePair<long, JObj>(bale.id, rec); else o.Add(new KeyValuePair<long, JObj>(bale.id, rec));
            }
            classReport = ClassReport(models, bales, sel);
            return o;
        }

        /// tdf_sections(lv): the sections of the level's Untamed tdf07 (the 21 / 22 / 23 vectors equal the converted Reflex tdf's).
        internal static List<UxTdfSection> TdfSections(UxDiscLevel disc, CancellationToken cancel = default)
        {
            var sp = UxTdf07.Split(disc.ReadTdf(cancel));
            UxTdf07.Structure(sp.items, out _, out var secs);
            return secs;
        }
    }
}
