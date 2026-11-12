// MX vs ATV Untamed converter, stage S6 "terrain": the port of untamed/tools/ux_terrain.py (the reference; its doc comment describes the
// rules). Untamed .terrain / .trncol -> the Reflex heightfield (+ the level's lift), the excluded terrain as mesh parts, the ground
// textures. Reads level.json from the work store and the level's assets from the disc; writes below terrain/ what Python writes below
// work/<Level>/terrain/:
//   hf.npy, footprint.npy, excluded_tris.npy, hf_preview.png, topdown_base.png, topdown_occ.png, topdown_shadow.png, detailmask.png,
//   base.png, dirt_r.png, dirt_g.png, dirt_b.png, textures/UXT_<short>_Dirt*.texture, parts/UXT_<short>_<ix>_<iz>.{surface,model,collis},
//   material_requests.json, terrain.json
// Not ported: contact_sheet.png (Norton and Fairview: a review sheet built from a research picture that is not on the disc, with
// FreeType text; terrain.json 'files' therefore has no contact_sheet.png entry), --lift / --check / --selftest (command line
// reports), the log file. The folder is not emptied first (as in Python): readers take the file list from terrain.json.
// Arithmetic follows numpy's dtypes operation by operation (core/plan/UNTAMED_CONVERT.md, "numpy rules"); every output was compared
// with the reference: all files are byte-identical on the 12 levels, terrain.json is equal as a document
// except for source.tool_md5 / source.uxterrain_md5 (the md5 of Python's own sources) and one in-principle deviation:
// np.linalg.lstsq goes through LAPACK (dgelsd), whose rounding noise (about 1e-18 where the exact coefficient is 0) cannot be
// reproduced; it is solved here by a Householder QR + Jacobi SVD least squares. The fits feed the choice of planar maps (2e-5
// tolerances), thresholds and values rounded to 8 decimals: the only visible effect is the sign of such a zero (0.0 / -0.0) in
// ground.uv_sets_per_material.*.coef (17 values on 4 levels, in UV sets that are constant).
// "System only": no UnityEngine, no unsafe code.
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of ux_terrain.build (ux_build_track passes occ_map; every current build uses the defaults).
    public sealed class UxTerrainOptions
    {
        /// --occ-map: "contact" (default: the object-contact test picks the planar map of occ / shadow textures) or "layout".
        public string occ_map = "contact";
        /// --dirt-tint R G B: null = the level's shader literal, else the default (111, 87, 66).
        public int[] dirt_tint;
        /// --no-write: build the heightfield and the manifest's first half only, write nothing.
        public bool write = true;
        /// terrain.json source.tool_md5 / source.uxterrain_md5: Python writes the md5 of its own source files there; the port names itself.
        public string tool_md5 = "port:UxTerrainStage.cs/1", uxterrain_md5 = "port:UxTerrain.cs/1";
        /// Worker threads for the row-parallel loops (0 = Environment.ProcessorCount). The output does not depend on it.
        public int jobs = 0;
    }

    public static class UxTerrainStage
    {
        public const string TOOL = "ux_terrain", SCHEMA = "ux_terrain/1";
        const int HF_SIZE = UxCommon.HF_SIZE; const double HF_RES = 1.0;
        const int TD_SIZE = 2048; const double TD_RES = 0.5;
        const int DETAIL_SIZE = 1024;
        const double X0 = -HF_SIZE * HF_RES / 2, Z1 = HF_SIZE * HF_RES / 2;
        const double CELL_FT = 128.0;
        const string COLLISION_ONLY = "collision-only";
        const double EDGE_NEAR_FT = 12.0, EDGE_ERR_FT = 0.45; const int EDGE_ROUNDS = 6;
        const double EDGE_BORDER_FT = 3.0, HOLE_MARGIN_FT = 0.10, FILL_BORDER_FT = 8.0, FILL_DROP_FT = 1.0, CLAMP_MARGIN_FT = 0.25, CLAMP_SAFETY_FT = 0.07;
        const int FOOT_RING = 1;
        const double UNCLAMP_DARK = 0.08, CLAMP_REACH_FT = 0.1, RUTS_NEAR_FT = 15.0, RUTS_MIN_ENRICH = 1.2, MACRO_K = 0.35;
        const int TINT_MIN_LUM = 70;
        static readonly int[] DEFAULT_TINT = { 111, 87, 66 };
        const double SLIVER_WIDTH_FT = 1.0 / 16, UP_NY = 0.05;
        const int KIND22_MAX_TRIS = 48; const double KIND22_MAX_HALF = 32.0, KIND22_SPLIT_EDGE = 64.0;
        const string RUTS_TEX = "ui_indoor_ruts01";
        static readonly string[] BASE_TEX_PREF = { "hp_base_helper", "stadium_side_dirt", RUTS_TEX };
        static readonly HashSet<string> TILING_SHARED = new HashSet<string>(StringComparer.Ordinal) { "so_grass01", "hp_trdirt1", "ui_indoor_ruts01", "stadium_side_dirt", "hp_base_helper", "bridgeport_tsand", "ec_watertarp", "cottonwoodbark", "lincoln_rock", "stumptop", "ui_indoor_512mask" };
        static readonly Dictionary<string, string[]> EXCLUDE = new Dictionary<string, string[]>(StringComparer.Ordinal)
        {
            { "BridgePort_STADIUM", new[] { "Bridgeport_Track_Over" } },
            { "Jefferson_STADIUM", new[] { "Jefferson_SXOver" } },
            { "Wilford_STADIUM", new[] { "Wilford_Track_InOVER" } },
            { "Fairview_STADIUM", new[] { "Fairview_Rocks", "Fairview_Logs", "endurocross_Log_Ends", COLLISION_ONLY } },
            { "Lincoln_STADIUM", new[] { "Lincoln_Rocks", "Lincoln_Logs", "endurocross_Log_Ends", COLLISION_ONLY } },
        };
        const string LIFT_RULE = "max(10, ceil(1 - lowest drawn kept geometry over an outside pixel, +-0.1 ft reach))";
        const string REACH_ONLY = "reach +-0.1 ft only";
        const double OCC_HALF_LO = 2.5, OCC_HALF_HI = 40.0, OCC_REACH_FT = 430.0, OCC_RING_FT = 6.0, OCC_SWITCH_A = 0.95, OCC_SWITCH_B = 0.95, OCC_SWITCH_C = 0.05;
        const RegexOptions RX = RegexOptions.CultureInvariant;

        static readonly double[] BS =
        {
            1, 0, 0, 0, 1, 0, 0, 0, 1, .5, .5, 0, 0, .5, .5, .5, 0, .5, 1.0 / 3, 1.0 / 3, 1.0 / 3,
            .75, .25, 0, .25, .75, 0, 0, .75, .25, 0, .25, .75, .25, 0, .75, .75, 0, .25,
            2.0 / 3, 1.0 / 6, 1.0 / 6, 1.0 / 6, 2.0 / 3, 1.0 / 6, 1.0 / 6, 1.0 / 6, 2.0 / 3,
        };

        static List<object> L(params object[] xs) => new List<object>(xs);
        static string Lower(string s) => UxCommon.PyLower(s);
        static bool Finite(double v) => !double.IsNaN(v) && !double.IsInfinity(v);
        static string Inv(long v) => v.ToString(System.Globalization.CultureInfo.InvariantCulture);
        /// '%g' for the numbers the messages print (lifts, margins).
        static string G(double v) => v == Math.Floor(v) && Math.Abs(v) < 1e15 ? Inv((long)v) : PyFormat.Repr(v);

        // ===================================================================================================== rasteriser
        /// raster(V, F, x0, z1, res, H, W, mode): the pixel-centre rasteriser on an XZ grid (row 0 = +Z edge). max = the highest surface
        /// (ties: the lowest triangle index), else the lowest. F null = a triangle soup (V holds 9 values per triangle).
        /// val: NaN = empty; tri: -1 = empty.
        static void Raster(double[] V, int[] F, int nt, double x0, double z1, double res, int H, int W, bool max, out double[] val, out int[] tri)
        {
            int n = H * W; var v = new double[n]; var tr = new int[n];
            double init = max ? double.NegativeInfinity : double.PositiveInfinity;
            for (int i = 0; i < n; i++) { v[i] = init; tr[i] = -1; }
            for (int t = 0; t < nt; t++)
            {
                int ia, ib, ic;
                if (F == null) { ia = 9 * t; ib = ia + 3; ic = ia + 6; } else { ia = 3 * F[3 * t]; ib = 3 * F[3 * t + 1]; ic = 3 * F[3 * t + 2]; }
                double ax = V[ia], ay = V[ia + 1], az = V[ia + 2], bx = V[ib], by = V[ib + 1], bz = V[ib + 2], cx = V[ic], cy = V[ic + 1], cz = V[ic + 2];
                double xmin = Math.Min(Math.Min(ax, bx), cx), xmax = Math.Max(Math.Max(ax, bx), cx), zmin = Math.Min(Math.Min(az, bz), cz), zmax = Math.Max(Math.Max(az, bz), cz);
                double fc0 = Math.Max(Math.Ceiling((xmin - x0) / res - 0.5), 0), fc1 = Math.Min(Math.Floor((xmax - x0) / res - 0.5), W - 1);
                double fr0 = Math.Max(Math.Ceiling((z1 - zmax) / res - 0.5), 0), fr1 = Math.Min(Math.Floor((z1 - zmin) / res - 0.5), H - 1);
                if (!(fc1 >= fc0 && fr1 >= fr0)) continue;
                double det = (bx - ax) * (cz - az) - (cx - ax) * (bz - az);
                if (!(Math.Abs(det) >= 1e-12)) continue;
                int c0 = (int)fc0, c1 = (int)fc1, r0 = (int)fr0, r1 = (int)fr1;
                for (int r = r0; r <= r1; r++)
                {
                    double pz = z1 - (r + 0.5) * res; int row = r * W;
                    for (int c = c0; c <= c1; c++)
                    {
                        double px = x0 + (c + 0.5) * res;
                        double l1 = ((px - ax) * (cz - az) - (cx - ax) * (pz - az)) / det;
                        double l2 = ((bx - ax) * (pz - az) - (px - ax) * (bz - az)) / det;
                        double l0 = 1 - l1 - l2;
                        if (!(l0 >= -1e-9 && l1 >= -1e-9 && l2 >= -1e-9)) continue;
                        double y = l0 * ay + l1 * by + l2 * cy;
                        int k = row + c;
                        if (max ? y > v[k] : y < v[k]) { v[k] = y; tr[k] = t; }
                    }
                }
            }
            for (int i = 0; i < n; i++) if (double.IsInfinity(v[i]) || double.IsNaN(v[i])) v[i] = UxNp.NaN;
            val = v; tri = tr;
        }
        static int[] Subset(int[] F, bool[] pick, bool want)
        {
            int m = 0; for (int i = 0; i < pick.Length; i++) if (pick[i] == want) m++;
            var o = new int[3 * m]; int at = 0;
            for (int i = 0; i < pick.Length; i++) if (pick[i] == want) { o[at++] = F[3 * i]; o[at++] = F[3 * i + 1]; o[at++] = F[3 * i + 2]; }
            return o;
        }

        /// bilinear(g, X0, Z1, HF_RES, x, z) on a (size, size) grid.
        static double Bilinear(double[] g, int size, double x0, double z1, double res, double x, double z)
        {
            double c = (x - x0) / res - 0.5, r = (z1 - z) / res - 0.5;
            double fc0 = Math.Floor(c), fr0 = Math.Floor(r);
            int c0 = fc0 < 0 ? 0 : fc0 > size - 2 ? size - 2 : (int)fc0, r0 = fr0 < 0 ? 0 : fr0 > size - 2 ? size - 2 : (int)fr0;
            double fc = c - c0, fr = r - r0;
            fc = fc < 0 ? 0 : fc > 1 ? 1 : fc; fr = fr < 0 ? 0 : fr > 1 ? 1 : fr;
            double a = g[r0 * size + c0], b = g[r0 * size + c0 + 1], cc = g[(r0 + 1) * size + c0], d = g[(r0 + 1) * size + c0 + 1];
            return (a * (1 - fc) + b * fc) * (1 - fr) + (cc * (1 - fc) + d * fc) * fr;
        }

        /// nearest_fill(values, have): values at the nearest `have` pixel (scipy's choice) and the distance in pixels.
        static double[] NearestFill(double[] values, bool[] have, int size, out double[] dist)
        {
            var not = new bool[have.Length]; for (int i = 0; i < not.Length; i++) not[i] = !have[i];
            UxNd.DistanceTransformEdt(not, size, size, out int[] row, out int[] col);
            var o = new double[values.Length]; var d = new double[values.Length];
            for (int r = 0; r < size; r++)
                for (int c = 0; c < size; c++)
                {
                    int k = r * size + c;
                    o[k] = values[row[k] * size + col[k]];
                    double dr = row[k] - r, dc = col[k] - c;
                    d[k] = Math.Sqrt(dr * dr + dc * dc);
                }
            dist = d;
            return o;
        }

        // ===================================================================================================== sources
        struct Key3 : IComparable<Key3>
        {
            public long a, b, c;
            public int CompareTo(Key3 o) => a != o.a ? a.CompareTo(o.a) : b != o.b ? b.CompareTo(o.b) : c.CompareTo(o.c);
        }
        static Key3[] Keys(double[] V, int[] F)
        {
            int n = F.Length / 3; var o = new Key3[n];
            for (int i = 0; i < n; i++)
            {
                long k0 = 0, k1 = 0, k2 = 0;
                for (int j = 0; j < 3; j++)
                {
                    int v = 3 * F[3 * i + j];
                    long x = (long)UxNp.Round(V[v] * 64), y = (long)UxNp.Round(V[v + 1] * 64), z = (long)UxNp.Round(V[v + 2] * 64);
                    long k = unchecked(x * (1000003L * 1000003L) + y * 1000003L + z);
                    if (j == 0) k0 = k; else if (j == 1) k1 = k; else k2 = k;
                }
                if (k0 > k1) { long t = k0; k0 = k1; k1 = t; }
                if (k1 > k2) { long t = k1; k1 = k2; k2 = t; }
                if (k0 > k1) { long t = k0; k0 = k1; k1 = t; }
                o[i] = new Key3 { a = k0, b = k1, c = k2 };
            }
            return o;
        }

        /// ux_terrain.Source: one level's terrain inputs.
        sealed class Source
        {
            public UxLevelRow lv; public UxDiscLevel disc; public JObj lj, closure; public double lift; public JObj tinfo; public List<string> mats = new List<string>();
            public JObj terrain_rec, trncol_rec; public UxTerrainMesh T; public UxTrnCol C;
            public double[] P; public int[] I; public int nt; public double[] n, ar, ny;
            public double[] rV; public int[] rF, rM, mat_first, rtri, cm; public double orient_agree;
            public CancellationToken cancel;
            readonly Dictionary<string, Dictionary<string, JObj>> _lower = new Dictionary<string, Dictionary<string, JObj>>(StringComparer.Ordinal);
            readonly Dictionary<string, Dictionary<string, JObj>> _exact = new Dictionary<string, Dictionary<string, JObj>>(StringComparer.Ordinal);

            public Source(UxDiscLevel disc, UxWork work, CancellationToken cancel)
            {
                this.disc = disc; lv = disc.lv; this.cancel = cancel;
                lj = work.ReadJsonObj("level.json");
                lift = UxCommon.YLift(lv);
                object ljLift = lj.Get("y_lift");
                if (ljLift != null && UxJson.Num(ljLift) != lift) throw new UxException(lv.disc + ": level.json y_lift " + G(UxJson.Num(ljLift)) + " != ux_common.y_lift " + G(lift) + " (rerun ux_level)");
                tinfo = UxJson.Obj(lj.Get("terrain")); closure = UxJson.Obj(lj.Get("closure"));
                foreach (object m in UxJson.Arr(tinfo.Get("materials"))) mats.Add(UxJson.Str(m));
                terrain_rec = UxJson.Obj(UxJson.Obj(closure.Get("terrain")).Get(UxJson.Str(tinfo.Get("terrain"))) ?? throw new UxException("level.json: no closure record of the terrain"));
                trncol_rec = UxJson.Obj(UxJson.Obj(closure.Get("trncol")).Get(UxJson.Str(tinfo.Get("trncol"))) ?? throw new UxException("level.json: no closure record of the trncol"));
                T = UxTerrain.ReadTerrain(disc.ReadExtractFile(UxJson.Str(terrain_rec.Get("file")), cancel));
                C = UxTerrain.ReadTrncol(disc.ReadExtractFile(UxJson.Str(trncol_rec.Get("file")), cancel));
                UxBin.Require(T.nmat == mats.Count, "terrain has " + T.nmat + " materials, MaterialList " + mats.Count);
                P = C.verts; I = C.tris; nt = I.Length / 3;
                TriGeometry(P, I, out n, out ar, out ny);
                T.Mesh(out rV, out rF, out rM);
                mat_first = new int[T.materials.Count]; { int s = 0; for (int k = 0; k < mat_first.Length; k++) { mat_first[k] = s; s += T.materials[k].TriCount; } }
                // the trncol <-> render match: np.argsort(vr, kind='stable'), np.searchsorted(vr[o], vc)
                var vr = Keys(rV, rF); var vc = Keys(P, I);
                var o = new int[vr.Length]; for (int i = 0; i < o.Length; i++) o[i] = i;
                Array.Sort(o, (x, y) => { int c = vr[x].CompareTo(vr[y]); return c != 0 ? c : x.CompareTo(y); });
                rtri = new int[nt]; cm = new int[nt];
                for (int i = 0; i < nt; i++)
                {
                    int lo = 0, hi = vr.Length;
                    while (lo < hi) { int mid = (lo + hi) >> 1; if (vr[o[mid]].CompareTo(vc[i]) < 0) lo = mid + 1; else hi = mid; }
                    int p = Math.Min(Math.Max(lo, 0), vr.Length - 1);
                    bool hit = vr.Length > 0 && vr[o[p]].CompareTo(vc[i]) == 0;
                    rtri[i] = hit ? o[p] : -1; cm[i] = hit ? rM[o[p]] : -1;
                }
                // orientation agreement (trncol order vs render front face) on matched triangles: np.einsum('ij,ij->i', ...) > 0
                long agree = 0, matched = 0;
                for (int i = 0; i < nt; i++)
                {
                    if (rtri[i] < 0) continue;
                    matched++;
                    Cross(rV, rF, rtri[i], out double rx, out double ry, out double rz);
                    if (EinsumDot(n[3 * i], n[3 * i + 1], n[3 * i + 2], rx, ry, rz) > 0) agree++;
                }
                orient_agree = matched > 0 ? (double)agree / matched : 1.0;
            }

            Dictionary<string, JObj> Table(string typ, bool lower)
            {
                var tab = lower ? _lower : _exact;
                if (tab.TryGetValue(typ, out var d)) return d;
                d = new Dictionary<string, JObj>(StringComparer.Ordinal);
                if (closure.Get(typ) is JObj c)
                    foreach (var kv in c.items)
                    {
                        string key = lower ? Lower(kv.Key) : kv.Key;
                        if (!d.ContainsKey(key)) d[key] = UxJson.Obj(kv.Value);
                    }
                tab[typ] = d;
                return d;
            }
            /// rec(name, typ): the closure record, by name, then ignoring case (the first in the closure's order).
            public JObj Rec(string name, string typ)
            {
                if (Table(typ, false).TryGetValue(name, out var r)) return r;
                return Table(typ, true).TryGetValue(Lower(name), out r) ? r : null;
            }
            public byte[] Data(string name, string typ, out JObj rec)
            {
                rec = Rec(name, typ);
                return rec == null ? null : disc.ReadExtractFile(UxJson.Str(rec.Get("file")), cancel);
            }
            /// material_textures(k): the Untamed textures of terrain material k in sampler order.
            public List<string> MaterialTextures(int k)
            {
                var rec = UxJson.Obj(UxJson.Obj(closure.Get("material")).Get(mats[k]) ?? throw new UxException("level.json: no closure record of material " + mats[k]));
                var o = new List<string>(); foreach (object t in UxJson.Arr(rec.Get("textures"))) o.Add(UxJson.Str(t));
                return o;
            }
            public List<string> MaterialTexturesLower(int k) { var o = MaterialTextures(k); for (int i = 0; i < o.Count; i++) o[i] = Lower(o[i]); return o; }
            public JObj MaterialStates(int k)
            {
                byte[] d = Data(mats[k], "material", out _) ?? throw new UxException("no material " + mats[k]);
                var m = UxMat.ReadMaterial(d);
                return m.passes.Count > 0 ? UxMat.NamedStates(m.passes[0].states) : new JObj();
            }
            public int MatCount(string name) { int c = 0; foreach (string m in mats) if (m == name) c++; return c; }
            public string MatKey(int k) => mats[k] + (MatCount(mats[k]) == 1 ? "" : "#" + Inv(k));
        }

        static void Cross(double[] V, int[] F, int t, out double x, out double y, out double z)
        {
            int a = 3 * F[3 * t], b = 3 * F[3 * t + 1], c = 3 * F[3 * t + 2];
            double ux = V[b] - V[a], uy = V[b + 1] - V[a + 1], uz = V[b + 2] - V[a + 2], vx = V[c] - V[a], vy = V[c + 1] - V[a + 1], vz = V[c + 2] - V[a + 2];
            x = uy * vz - uz * vy; y = uz * vx - ux * vz; z = ux * vy - uy * vx;
        }
        /// np.einsum('ij,ij->i', a, b) of 3-vectors on the reference machine: (a0 b0 + a2 b2) + a1 b1 (two SSE lanes).
        static double EinsumDot(double a0, double a1, double a2, double b0, double b1, double b2) => (a0 * b0 + a2 * b2) + a1 * b1;
        /// tri_geometry: n = cross, ar = |n| / 2, ny = n.y / max(2 ar, 1e-30).
        static void TriGeometry(double[] P, int[] I, out double[] n, out double[] ar, out double[] ny)
        {
            int nt = I.Length / 3; n = new double[3 * nt]; ar = new double[nt]; ny = new double[nt];
            for (int t = 0; t < nt; t++)
            {
                Cross(P, I, t, out double x, out double y, out double z);
                n[3 * t] = x; n[3 * t + 1] = y; n[3 * t + 2] = z;
                ar[t] = Math.Sqrt(x * x + y * y + z * z) / 2;
                ny[t] = y / Math.Max(2 * ar[t], 1e-30);
            }
        }

        // ===================================================================================================== exclusions
        static List<int> ExclusionMaterials(Source src, List<object> problems)
        {
            EXCLUDE.TryGetValue(src.lv.disc, out string[] names); names = names ?? new string[0];
            var idx = new List<int>();
            foreach (string n in names)
            {
                if (n == COLLISION_ONLY) { idx.Add(-1); continue; }
                int before = idx.Count;
                for (int k = 0; k < src.mats.Count; k++) if (src.mats[k] == n) idx.Add(k);
                if (idx.Count == before) problems.Add("excluded material " + n + " not in the MaterialList " + PyList(src.mats));
            }
            bool ec = src.lv.mode == "ec";
            var rule = new List<int>();
            for (int k = 0; k < src.mats.Count; k++)
                if (Regex.IsMatch(src.mats[k], "over", RegexOptions.IgnoreCase | RX) || (ec && Regex.IsMatch(src.mats[k], "rock|log", RegexOptions.IgnoreCase | RX))) rule.Add(k);
            if (ec) rule.Add(-1);
            var a = SortedSet(idx); var b = SortedSet(rule);
            bool same = a.Count == b.Count; for (int i = 0; same && i < a.Count; i++) same = a[i] == b[i];
            if (!same) problems.Add("EXCLUDE table " + PyList(a) + " differs from the DESIGN regex rule " + PyList(b));
            return a;
        }
        static List<int> SortedSet(IEnumerable<int> xs) { var s = new SortedSet<int>(xs); return new List<int>(s); }
        static string PyList(IEnumerable<int> xs) { var sb = new StringBuilder("["); bool first = true; foreach (int x in xs) { if (!first) sb.Append(", "); sb.Append(Inv(x)); first = false; } return sb.Append(']').ToString(); }
        static string PyList(IEnumerable<string> xs) { var sb = new StringBuilder("["); bool first = true; foreach (string x in xs) { if (!first) sb.Append(", "); sb.Append(x == null ? "None" : "'" + x + "'"); first = false; } return sb.Append(']').ToString(); }

        static string MaterialRole(string name)
        {
            if (Regex.IsMatch(name, "rock", RegexOptions.IgnoreCase | RX)) return "rock";
            if (Regex.IsMatch(name, "log_?ends?$", RegexOptions.IgnoreCase | RX)) return "logend";
            if (Regex.IsMatch(name, "logs?$", RegexOptions.IgnoreCase | RX)) return "log";
            return "dirt";
        }

        // ===================================================================================================== heightfield
        sealed class Drawn
        {
            public double[] low, low_c; public int[] low_owner; public List<string> names = new List<string>();
            public int entities, triangles; public JObj skipped = new JObj();
        }

        /// drawn_low: the lowest drawn kept geometry per 1 ft HF pixel (Untamed frame), over every kept / demoted entity with a static
        /// model except BillboardCrowd and sky models; low_c also samples +-CLAMP_REACH_FT around each pixel centre.
        static Drawn DrawnLow(Source src, int jobs)
        {
            var d = new Drawn();
            var tris = new List<double>(1 << 20); var owner = new List<int>(1 << 17);
            var surfCache = new Dictionary<string, List<UxGroup>>(StringComparer.Ordinal);
            foreach (object eo in UxJson.Arr(src.lj.Get("entities")))
            {
                var e = UxJson.Obj(eo);
                string cls = e.Get("cls") as string, type = e.Get("type") as string;
                if ((cls != "keep" && cls != "demote") || type == "BillboardCrowdRSData") continue;
                var pr = e.Get("props") as JObj;
                var ma = pr?.Get("ModelAsset") as JObj; var sa = pr?.Get("SurfaceAsset") as JObj;
                if (ma == null || sa == null) continue;
                string maName = UxJson.Str(ma.Get("asset")), saName = UxJson.Str(sa.Get("asset"));
                string ename = e.Get("name") as string;
                if (Regex.IsMatch((ename ?? "") + " " + maName, "sky", RegexOptions.IgnoreCase | RX)) continue;
                var mrec = src.Rec(maName, "model");
                if (mrec == null || Truthy(mrec.Get("skinned")) || Truthy(mrec.Get("boned"))) { d.skipped.Set(maName, "no static model"); continue; }
                string key = Lower(saName);
                if (!surfCache.TryGetValue(key, out var gs))
                {
                    byte[] sdat = src.Data(saName, "surface", out _);
                    gs = sdat != null ? UxMesh.ReadSurface(sdat) : null;
                    surfCache[key] = gs;
                }
                if (gs == null) { d.skipped.Set(saName, "no surface"); continue; }
                var M = Matrix(e);
                d.entities++;
                foreach (var g in gs)
                {
                    if (g.flags != 0 || g.tris.Length == 0) continue;
                    double[] W = Transform(g.pos, M);
                    for (int t = 0; t < g.tris.Length; t += 3)
                    {
                        int a = 3 * g.tris[t], b = 3 * g.tris[t + 1], c = 3 * g.tris[t + 2];
                        double ax = W[a], ay = W[a + 1], az = W[a + 2], bx = W[b], by = W[b + 1], bz = W[b + 2], cx = W[c], cy = W[c + 1], cz = W[c + 2];
                        if (!(Finite(ax) && Finite(ay) && Finite(az) && Finite(bx) && Finite(by) && Finite(bz) && Finite(cx) && Finite(cy) && Finite(cz))) continue;
                        double lox = Math.Min(Math.Min(ax, bx), cx), hix = Math.Max(Math.Max(ax, bx), cx), loz = Math.Min(Math.Min(az, bz), cz), hiz = Math.Max(Math.Max(az, bz), cz);
                        if (!(hix >= X0 && lox <= -X0 && hiz >= -Z1 && loz <= Z1)) continue;
                        tris.Add(ax); tris.Add(ay); tris.Add(az); tris.Add(bx); tris.Add(by); tris.Add(bz); tris.Add(cx); tris.Add(cy); tris.Add(cz);
                        owner.Add(d.names.Count);
                    }
                }
                d.names.Add((type ?? "None") + " " + (ename ?? "None") + " (" + maName + ")");
            }
            double[] T = tris.ToArray(); int nt = owner.Count;
            d.triangles = nt;
            var vals = new double[5][]; var tr = new int[5][];
            double[] dx = { 0, CLAMP_REACH_FT, -CLAMP_REACH_FT, 0, 0 }, dz = { 0, 0, 0, CLAMP_REACH_FT, -CLAMP_REACH_FT };
            Parallel.For(0, 5, new ParallelOptions { MaxDegreeOfParallelism = jobs }, i =>
                Raster(T, null, nt, X0 + dx[i], Z1 + dz[i], HF_RES, HF_SIZE, HF_SIZE, false, out vals[i], out tr[i]));
            d.low = vals[0];
            d.low_owner = new int[HF_SIZE * HF_SIZE];
            for (int i = 0; i < d.low_owner.Length; i++) d.low_owner[i] = tr[0][i] >= 0 ? owner[tr[0][i]] : -1;
            d.low_c = (double[])vals[0].Clone();
            for (int k = 1; k < 5; k++)
                for (int i = 0; i < d.low_c.Length; i++)
                {   // np.fmin: the smaller, ignoring a NaN
                    double a = d.low_c[i], b = vals[k][i];
                    d.low_c[i] = double.IsNaN(a) ? b : double.IsNaN(b) ? a : (b < a ? b : a);
                }
            return d;
        }
        static bool Truthy(object v) => v is bool b ? b : v is long l ? l != 0 : v is double dd ? dd != 0 : v is string s ? s.Length > 0 : v != null;
        static double[] Matrix(JObj e)
        {
            var m = UxJson.Arr(e.Get("matrix")); UxBin.Require(m.Count == 16, "entity matrix");
            var o = new double[16]; for (int i = 0; i < 16; i++) o[i] = UxJson.Num(m[i]);
            return o;
        }
        /// g.pos @ M[:3, :3] + M[3, :3] (float32 positions, float64 matrix; the product is a BLAS dgemm: UxNp.MatMul's chain).
        static double[] Transform(float[] pos, double[] M)
        {
            int n = pos.Length / 3; var o = new double[3 * n];
            for (int i = 0; i < n; i++)
            {
                double x = pos[3 * i], y = pos[3 * i + 1], z = pos[3 * i + 2];
                for (int j = 0; j < 3; j++)
                {
                    double s = x * M[j]; s = UxNp.Fma(y, M[4 + j], s); s = UxNp.Fma(z, M[8 + j], s);
                    o[3 * i + j] = s + M[12 + j];
                }
            }
            return o;
        }

        sealed class Hf
        {
            public float[] h; public bool[] foot, foot_ext, cov, excl, excl_mat, edge, unclampable; public double lift;
            public double[] low, low_c; public JObj stats;
        }

        static JObj RequiredLift(double[] low_c, bool[] outside)
        {
            double m = double.PositiveInfinity; bool any = false;
            for (int i = 0; i < low_c.Length; i++) if (outside[i] && Finite(low_c[i])) { any = true; if (low_c[i] < m) m = low_c[i]; }
            double lift = !any ? UxCommon.Y_LIFT : Math.Max(UxCommon.Y_LIFT, Math.Ceiling(1.0 - m));
            return new JObj().Set("lift_ft", lift).Set("low_c_min_outside_ft", any ? (object)UxNp.PyRound(m, 3) : null).Set("rule", LIFT_RULE);
        }

        /// build_heightfield: the exclusion set (materials + edge triangles), the lifted heightfield, masks and stats.
        static Hf BuildHeightfield(Source src, List<int> exMat, Action<string> log, int jobs)
        {
            double[] P = src.P; int[] I = src.I; int nt = src.nt; int S = HF_SIZE, N = S * S;
            var excl = new bool[nt];
            if (exMat.Count > 0) { var set = new HashSet<int>(exMat); for (int i = 0; i < nt; i++) excl[i] = set.Contains(src.cm[i]); }
            var exclMat = (bool[])excl.Clone();
            bool anyExcl = false; foreach (bool b in excl) anyExcl |= b;
            Raster(P, I, nt, X0, Z1, HF_RES, S, S, true, out double[] allTop, out _);
            var foot = new bool[N]; for (int i = 0; i < N; i++) foot[i] = Finite(allTop[i]);
            bool[] footExt = UxNd.BinaryDilation(foot, S, S, FOOT_RING, false);
            var outside = new bool[N]; var notFoot = new bool[N];
            for (int i = 0; i < N; i++) { outside[i] = !footExt[i]; notFoot[i] = !foot[i]; }
            double[] dIn = UxNd.DistanceTransformEdtDistances(foot, S, S), dOut = UxNd.DistanceTransformEdtDistances(notFoot, S, S);
            var near = new bool[N];
            for (int i = 0; i < N; i++) near[i] = dIn[i] * HF_RES <= EDGE_BORDER_FT && dOut[i] * HF_RES <= EDGE_BORDER_FT + HF_RES;
            if (anyExcl)
            {
                int[] fe = Subset(I, exclMat, true);
                Raster(P, fe, fe.Length / 3, X0, Z1, HF_RES, S, S, true, out double[] exAny, out _);
                var noEx = new bool[N]; for (int i = 0; i < N; i++) noEx[i] = !Finite(exAny[i]);
                double[] dEx = UxNd.DistanceTransformEdtDistances(noEx, S, S);
                for (int i = 0; i < N; i++) near[i] |= dEx[i] * HF_RES <= EDGE_NEAR_FT;
            }
            Drawn dr = DrawnLow(src, jobs);
            log("  drawn geometry: " + dr.entities + " entities, " + dr.triangles + " triangles over the HF extent");
            var cap = new double[N]; for (int i = 0; i < N; i++) cap[i] = dr.low_c[i] - CLAMP_MARGIN_FT - CLAMP_SAFETY_FT;
            var edge = new bool[nt];
            var rounds = new List<object>();
            double[] hFinal = null; bool[] cov = null, clampPx = null, nearB = null, farB = null; double terrainMin = 0;
            for (int it = 0; it <= EDGE_ROUNDS; it++)
            {
                src.cancel.ThrowIfCancellationRequested();
                int[] fk = Subset(I, excl, false);
                Raster(P, fk, fk.Length / 3, X0, Z1, HF_RES, S, S, true, out double[] top, out _);
                cov = new bool[N]; for (int i = 0; i < N; i++) cov[i] = Finite(top[i]);
                var h = (double[])top.Clone();
                bool anyHole = false; for (int i = 0; i < N; i++) if (foot[i] && !cov[i]) { anyHole = true; break; }
                if (anyHole)
                {   // pixels only excluded triangles cover: just under the lowest excluded surface there; anything else: the nearest kept height
                    var t0 = new double[N]; for (int i = 0; i < N; i++) t0[i] = cov[i] ? top[i] : 0.0;
                    double[] fv = NearestFill(t0, cov, S, out _);
                    int[] fx = Subset(I, excl, true);
                    Raster(P, fx, fx.Length / 3, X0, Z1, HF_RES, S, S, false, out double[] exl, out _);
                    for (int i = 0; i < N; i++) if (Finite(exl[i])) fv[i] = exl[i] - HOLE_MARGIN_FT;
                    for (int i = 0; i < N; i++) if (foot[i] && !cov[i]) h[i] = fv[i];
                }
                terrainMin = double.PositiveInfinity;
                for (int i = 0; i < N; i++) if (foot[i] && !double.IsNaN(h[i]) && h[i] < terrainMin) terrainMin = h[i];
                var hf0 = new double[N]; for (int i = 0; i < N; i++) hf0[i] = foot[i] ? h[i] : 0.0;
                double[] hv = NearestFill(hf0, foot, S, out double[] dist);
                nearB = new bool[N]; farB = new bool[N]; clampPx = new bool[N]; hFinal = new double[N];
                double drop = terrainMin - FILL_DROP_FT;
                for (int i = 0; i < N; i++)
                {
                    nearB[i] = outside[i] && dist[i] * HF_RES <= FILL_BORDER_FT;
                    farB[i] = outside[i] && !nearB[i];
                    double hOut = footExt[i] && !foot[i] ? hv[i] : nearB[i] ? hv[i] : drop;
                    clampPx[i] = outside[i] && Finite(cap[i]) && hOut > cap[i];
                    double v = foot[i] ? h[i] : hOut;
                    hFinal[i] = clampPx[i] ? cap[i] : v;
                }
                if (it == EDGE_ROUNDS) break;
                var cand = new List<int>();
                for (int t = 0; t < nt; t++)
                {
                    if (excl[t] || !(src.ny[t] > UP_NY)) continue;
                    int a = 3 * I[3 * t], b = 3 * I[3 * t + 1], c = 3 * I[3 * t + 2];
                    double cx = ((P[a] + P[b]) + P[c]) / 3, cz = ((P[a + 2] + P[b + 2]) + P[c + 2]) / 3;        // P[I[cand]].mean(1)
                    double fcc = Math.Floor((cx - X0) / HF_RES), frr = Math.Floor((Z1 - cz) / HF_RES);
                    int cc = fcc < 0 ? 0 : fcc > S - 1 ? S - 1 : (int)fcc, rr = frr < 0 ? 0 : frr > S - 1 ? S - 1 : (int)frr;
                    if (near[rr * S + cc]) cand.Add(t);
                }
                if (cand.Count == 0) break;
                var err = new double[cand.Count];
                double[] hfin = hFinal;
                Parallel.For(0, cand.Count, new ParallelOptions { MaxDegreeOfParallelism = jobs }, i => err[i] = TriMaxError(hfin, P, I, cand[i]));
                double maxErr = double.NegativeInfinity; int moved = 0;
                for (int i = 0; i < err.Length; i++) { if (err[i] > maxErr) maxErr = err[i]; if (err[i] > EDGE_ERR_FT) moved++; }
                rounds.Add(new JObj().Set("round", (long)it).Set("candidates", (long)cand.Count).Set("moved", (long)moved).Set("max_err", UxNp.PyRound(maxErr, 3)));
                if (moved == 0) break;
                for (int i = 0; i < err.Length; i++) if (err[i] > EDGE_ERR_FT) { edge[cand[i]] = true; excl[cand[i]] = true; }
            }
            long nExMat = Count(exclMat), nEdge = Count(edge);
            log("  exclusions: " + nExMat + " material triangles, " + nEdge + " edge triangles (" + rounds.Count + " rounds)");
            double lift = src.lift;
            var hOutF = new float[N]; var unclampable = new bool[N];
            long loClip = 0, hiClip = 0, nUnc = 0;
            var uncBy = new JObj();
            for (int i = 0; i < N; i++)
            {
                double lifted = hFinal[i] + lift;
                bool lo = lifted < UxCommon.Limits.HfMinFt, hi = lifted > UxCommon.Limits.HfMaxFt;
                if (lo) loClip++; if (hi) hiClip++;
                unclampable[i] = clampPx[i] && lo;
                if (unclampable[i])
                {
                    nUnc++;
                    string who = dr.low_owner[i] >= 0 ? dr.names[dr.low_owner[i]] : REACH_ONLY;
                    uncBy.Set(who, (long)(uncBy.Get(who) is long c ? c + 1 : 1));
                }
                hOutF[i] = (float)(lo ? UxCommon.Limits.HfMinFt : hi ? UxCommon.Limits.HfMaxFt : lifted);
            }
            double loOut = double.PositiveInfinity; bool anyLo = false;
            for (int i = 0; i < N; i++) if (outside[i] && Finite(dr.low[i])) { anyLo = true; if (dr.low[i] < loOut) loOut = dr.low[i]; }
            JObj req = RequiredLift(dr.low_c, outside);
            long nFoot = Count(foot), nRing = 0, nKeptCov = 0, nHoles = 0;
            for (int i = 0; i < N; i++) { if (footExt[i] && !foot[i]) nRing++; if (foot[i] && cov[i]) nKeptCov++; if (foot[i] && !cov[i]) nHoles++; }
            var stats = new JObj()
                .Set("lift_ft", lift).Set("lift_required_ft", req.Get("lift_ft")).Set("lift_rule", req.Get("rule"))
                .Set("drawn_low_c_min_outside_ft", req.Get("low_c_min_outside_ft"))
                .Set("footprint_px", nFoot).Set("footprint_ring_px", nRing).Set("kept_covered_px", nKeptCov).Set("interior_holes_px", nHoles)
                .Set("outside_px", Count(outside)).Set("outside_border_fill_px", Count(nearB)).Set("outside_drop_px", Count(farB))
                .Set("terrain_min_ft", UxNp.PyRound(terrainMin, 4)).Set("drop_to_ft", UxNp.PyRound(terrainMin - FILL_DROP_FT, 4))
                .Set("clamped_px", Count(clampPx)).Set("clipped_low_px", loClip).Set("clipped_high_px", hiClip)
                .Set("unclampable_px", nUnc).Set("unclampable_by_entity", uncBy)
                .Set("drawn", new JObj().Set("entities", (long)dr.entities).Set("triangles", (long)dr.triangles).Set("skipped", dr.skipped))
                .Set("drawn_low_min_outside_ft", anyLo ? (object)UxNp.PyRound(loOut, 3) : null)
                .Set("edge_rounds", rounds);
            return new Hf { h = hOutF, foot = foot, foot_ext = footExt, cov = cov, excl = excl, excl_mat = exclMat, edge = edge, unclampable = unclampable, lift = lift, low = dr.low, low_c = dr.low_c, stats = stats };
        }
        static long Count(bool[] a) { long n = 0; foreach (bool b in a) if (b) n++; return n; }

        /// tri_max_error: the largest |bilinear HF - surface| over the 16 barycentric sample points of one triangle
        /// (np.einsum('sk,tkd->tsd'): (b0 p0 + b1 p1) + b2 p2).
        static double TriMaxError(double[] h, double[] P, int[] I, int t)
        {
            int a = 3 * I[3 * t], b = 3 * I[3 * t + 1], c = 3 * I[3 * t + 2];
            double e = double.NegativeInfinity;
            for (int s = 0; s < 16; s++)
            {
                double b0 = BS[3 * s], b1 = BS[3 * s + 1], b2 = BS[3 * s + 2];
                double x = (b0 * P[a] + b1 * P[b]) + b2 * P[c], y = (b0 * P[a + 1] + b1 * P[b + 1]) + b2 * P[c + 1], z = (b0 * P[a + 2] + b1 * P[b + 2]) + b2 * P[c + 2];
                double d = Math.Abs(Bilinear(h, HF_SIZE, X0, Z1, HF_RES, x, z) - y);
                if (d > e) e = d;
            }
            return e;
        }

        /// fidelity (V7.1 method): 200k area-weighted samples on kept up-facing trncol triangles, bilinear HF - lift vs the surface.
        static JObj Fidelity(Source src, float[] hf, bool[] excl, int n = 200000, uint seed = 2)
        {
            double[] P = src.P; int[] I = src.I;
            var up = new List<int>();
            for (int t = 0; t < src.nt; t++) if (!excl[t] && src.ny[t] > UP_NY && src.ar[t] > 0) up.Add(t);
            var w = new double[up.Count]; for (int i = 0; i < w.Length; i++) w[i] = src.ar[up[i]];
            double total = UxNp.Sum(w);
            var cdf = new double[w.Length]; double acc = 0;
            for (int i = 0; i < w.Length; i++) { double p = w[i] / total; acc = i == 0 ? p : acc + p; cdf[i] = acc; }
            double last = cdf[cdf.Length - 1]; for (int i = 0; i < cdf.Length; i++) cdf[i] /= last;
            var rng = new UxImaging.Pcg64(seed);
            var pick = new int[n];
            for (int i = 0; i < n; i++)
            {
                double u = rng.NextDouble();
                int lo = 0, hi = cdf.Length;                      // searchsorted(side='right'): the first cdf > u
                while (lo < hi) { int mid = (lo + hi) >> 1; if (cdf[mid] <= u) lo = mid + 1; else hi = mid; }
                pick[i] = up[lo];
            }
            var h = new double[hf.Length]; for (int i = 0; i < h.Length; i++) h[i] = hf[i];
            var err = new double[n];
            for (int i = 0; i < n; i++)
            {
                double u0 = rng.NextDouble(), u1 = rng.NextDouble();
                if (u0 + u1 > 1) { u0 = 1 - u0; u1 = 1 - u1; }
                int a = 3 * I[3 * pick[i]], b = 3 * I[3 * pick[i] + 1], c = 3 * I[3 * pick[i] + 2];
                double x = P[a] + (P[b] - P[a]) * u0 + (P[c] - P[a]) * u1, y = P[a + 1] + (P[b + 1] - P[a + 1]) * u0 + (P[c + 1] - P[a + 1]) * u1, z = P[a + 2] + (P[b + 2] - P[a + 2]) * u0 + (P[c + 2] - P[a + 2]) * u1;
                err[i] = Math.Abs(Bilinear(h, HF_SIZE, X0, Z1, HF_RES, x, z) - src.lift - y);
            }
            var sq = new double[n]; double mx = double.NegativeInfinity; long o5 = 0, o6 = 0;
            for (int i = 0; i < n; i++) { sq[i] = err[i] * err[i]; if (err[i] > mx) mx = err[i]; if (err[i] > 0.5) o5++; if (err[i] > 0.6) o6++; }
            double rms = Math.Sqrt(UxNp.Sum(sq) / n);
            Array.Sort(err);
            return new JObj().Set("n", (long)n).Set("rms", rms).Set("p99", UxImaging.PercentileSorted(err, 99)).Set("max", mx).Set("over_0_5ft", o5).Set("over_0_6ft", o6);
        }

        // ===================================================================================================== least squares
        /// np.linalg.lstsq(X, Y, rcond=None)[0] for X (n, 3), Y (n, m): Householder QR, then a Jacobi SVD of R with numpy's cut-off
        /// (singular values below eps * max(n, 3) * the largest are dropped: the minimum-norm solution). LAPACK's dgelsd rounds
        /// differently: the result agrees with numpy's to about 1e-13 relative, not bit for bit.
        static double[] Lstsq(double[] X, double[] Y, int n, int m)
        {
            var coef = new double[3 * m];
            if (n == 0) return coef;
            var A = (double[])X.Clone(); var B = (double[])Y.Clone();
            int rank = Math.Min(n, 3);
            for (int k = 0; k < rank; k++)
            {
                double norm = 0; for (int i = k; i < n; i++) norm += A[3 * i + k] * A[3 * i + k];
                norm = Math.Sqrt(norm);
                if (norm == 0) continue;
                double alpha = A[3 * k + k] > 0 ? -norm : norm;
                double v0 = A[3 * k + k] - alpha;
                double vnorm2 = v0 * v0; for (int i = k + 1; i < n; i++) vnorm2 += A[3 * i + k] * A[3 * i + k];
                if (vnorm2 == 0) continue;
                for (int j = k + 1; j < 3; j++)
                {
                    double s = v0 * A[3 * k + j]; for (int i = k + 1; i < n; i++) s += A[3 * i + k] * A[3 * i + j];
                    s = 2 * s / vnorm2;
                    A[3 * k + j] -= s * v0; for (int i = k + 1; i < n; i++) A[3 * i + j] -= s * A[3 * i + k];
                }
                for (int j = 0; j < m; j++)
                {
                    double s = v0 * B[m * k + j]; for (int i = k + 1; i < n; i++) s += A[3 * i + k] * B[m * i + j];
                    s = 2 * s / vnorm2;
                    B[m * k + j] -= s * v0; for (int i = k + 1; i < n; i++) B[m * i + j] -= s * A[3 * i + k];
                }
                A[3 * k + k] = alpha; for (int i = k + 1; i < n; i++) A[3 * i + k] = 0;
            }
            // R (3 x 3, zero rows when n < 3) = U S V^T by one-sided Jacobi on R's columns
            var R = new double[9]; for (int i = 0; i < Math.Min(n, 3); i++) for (int j = i; j < 3; j++) R[3 * i + j] = A[3 * i + j];
            var V = new double[] { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
            for (int sweep = 0; sweep < 60; sweep++)
            {
                bool rotated = false;
                for (int p = 0; p < 2; p++)
                    for (int q = p + 1; q < 3; q++)
                    {
                        double app = 0, aqq = 0, apq = 0;
                        for (int i = 0; i < 3; i++) { app += R[3 * i + p] * R[3 * i + p]; aqq += R[3 * i + q] * R[3 * i + q]; apq += R[3 * i + p] * R[3 * i + q]; }
                        if (apq == 0 || Math.Abs(apq) <= 1e-17 * Math.Sqrt(app * aqq)) continue;
                        rotated = true;
                        double zeta = (aqq - app) / (2 * apq);
                        double t = (zeta >= 0 ? 1.0 : -1.0) / (Math.Abs(zeta) + Math.Sqrt(1 + zeta * zeta));
                        double cs = 1 / Math.Sqrt(1 + t * t), sn = cs * t;
                        for (int i = 0; i < 3; i++)
                        {
                            double rp = R[3 * i + p], rq = R[3 * i + q];
                            R[3 * i + p] = cs * rp - sn * rq; R[3 * i + q] = sn * rp + cs * rq;
                            double vp = V[3 * i + p], vq = V[3 * i + q];
                            V[3 * i + p] = cs * vp - sn * vq; V[3 * i + q] = sn * vp + cs * vq;
                        }
                    }
                if (!rotated) break;
            }
            var sv = new double[3]; double smax = 0;
            for (int j = 0; j < 3; j++) { double s = 0; for (int i = 0; i < 3; i++) s += R[3 * i + j] * R[3 * i + j]; sv[j] = Math.Sqrt(s); if (sv[j] > smax) smax = sv[j]; }
            double cut = 2.220446049250313e-16 * Math.Max(n, 3) * smax;
            for (int c = 0; c < m; c++)
                for (int j = 0; j < 3; j++)
                {
                    if (!(sv[j] > cut) || sv[j] == 0) continue;
                    double ub = 0; for (int i = 0; i < Math.Min(n, 3); i++) ub += R[3 * i + j] * B[m * i + c];      // (U^T b)_j * s_j
                    double f = ub / (sv[j] * sv[j]);
                    for (int i = 0; i < 3; i++) coef[m * i + c] += V[3 * i + j] * f;
                }
            return coef;
        }
        /// sqrt(((X @ coef - uv) ** 2).mean()) for X (n, 3), coef (3, m), uv (n, m).
        static double Residual(double[] X, double[] coef, double[] uv, int n, int m)
        {
            if (n == 0) return UxNp.NaN;
            var sq = new double[n * m];
            for (int i = 0; i < n; i++)
                for (int j = 0; j < m; j++)
                {
                    double s = X[3 * i] * coef[j]; s = UxNp.Fma(X[3 * i + 1], coef[m + j], s); s = UxNp.Fma(X[3 * i + 2], coef[2 * m + j], s);
                    double d = s - uv[m * i + j];
                    sq[m * i + j] = d * d;
                }
            return Math.Sqrt(UxNp.Sum(sq) / (n * m));
        }
        static int[] UniqueVerts(int[] tris)
        {
            var seen = new SortedSet<int>(tris);
            var o = new int[seen.Count]; seen.CopyTo(o);
            return o;
        }

        // ===================================================================================================== requests
        /// material_requests: one request per terrain material that has excluded render triangles (named UXT_<material>).
        static JObj MaterialRequests(Source src, Hf hfr, out List<KeyValuePair<string, string>> gen)
        {
            var reqs = new JObj(); gen = new List<KeyValuePair<string, string>>();
            var roles = TextureRoles(src);
            var ks = new SortedSet<int>();
            for (int i = 0; i < src.nt; i++) if (hfr.excl[i] && src.rtri[i] >= 0) ks.Add(src.rM[src.rtri[i]]);
            EXCLUDE.TryGetValue(src.lv.disc, out string[] exNames); exNames = exNames ?? new string[0];
            foreach (int k in ks)
            {
                string name = src.mats[k], role = MaterialRole(name);
                List<string> texs = src.MaterialTexturesLower(k);
                JObj st = src.MaterialStates(k);
                bool fitsUv0 = false;
                var m = src.T.materials[k];
                if (m.tris.Length > 0)
                {
                    int[] used = UniqueVerts(m.tris); int n = used.Length;
                    var X = new double[3 * n]; var uv = new double[2 * n];
                    for (int i = 0; i < n; i++) { X[3 * i] = m.pos[3 * used[i]]; X[3 * i + 1] = m.pos[3 * used[i] + 2]; X[3 * i + 2] = 1; uv[2 * i] = m.uv0[2 * used[i]]; uv[2 * i + 1] = m.uv0[2 * used[i] + 1]; }
                    double[] coef = Lstsq(X, uv, n, 2);
                    double res = Residual(X, coef, uv, n, 2);
                    double s0 = Math.Max(Math.Abs(coef[0]), Math.Abs(coef[2])) * 1024, s1 = Math.Max(Math.Abs(coef[1]), Math.Abs(coef[3])) * 1024;
                    fitsUv0 = res < 0.05 && s0 < 4.0 && s1 < 4.0;
                }
                bool isExclMat = Array.IndexOf(exNames, name) >= 0;
                string template, uv0, note; var textures = new List<string>();
                if (role == "dirt")
                {
                    bool hasRuts = texs.Contains(RUTS_TEX);
                    string gname = "UXT_" + src.lv.shortName + "_Dirt" + (hasRuts ? "Ruts" : "Base");
                    SetPair(gen, gname, hasRuts ? "ruts" : "base");
                    if (hasRuts && texs.Contains("ui_indoor_ruts01_norm")) { template = "Template_Diffuse_Nrm"; textures.Add(gname); textures.Add("ui_indoor_ruts01_norm"); }
                    else { template = "Template_Diffuse"; textures.Add(gname); }
                    uv0 = hasRuts || !fitsUv0 ? "source_uv0" : "world_tiling_16ft";
                    note = (isExclMat ? "bridge deck" : "edge triangles of a kept material") + ": drawn with the generated tinted dirt texture (same look as the HF near the camera)";
                }
                else
                {
                    string diffuse = role == "rock" ? texs.Find(t => t.Contains("rock") && !t.Contains("norm")) : role == "log" ? texs.Find(t => t.Contains("bark")) : texs.Find(t => !t.Contains("norm"));
                    string normal = texs.Find(t => t.Contains("norm"));
                    if (diffuse == null) diffuse = texs.Find(t => !(roles.Get(t) as string == "normal")) ?? texs[0];
                    if (normal != null && role != "logend") { template = "Template_Diffuse_Nrm"; textures.Add(diffuse); textures.Add(normal); }
                    else { template = "Template_Diffuse"; textures.Add(diffuse); }
                    uv0 = "source_uv0";
                    note = "EC " + role + ": diffuse + normal from the Untamed material, UV0 = Untamed UV0";
                }
                var generated = new List<object>(); foreach (string t in textures) if (t.StartsWith("UXT_", StringComparison.Ordinal)) generated.Add(t);
                reqs.Set("UXT_" + name, new JObj().Set("template", template).Set("textures", new List<object>(textures)).Set("source_material", name)
                    .Set("source_textures", new List<object>(texs)).Set("role", role).Set("uv0", uv0)
                    .Set("uv1", role == "dirt" ? "hf_planar (TopDown coordinates)" : "source_uv1")
                    .Set("states", new JObj().Set("CULLMODE", st.Get("CULLMODE")).Set("ALPHATESTENABLE", st.Has("ALPHATESTENABLE") ? st.Get("ALPHATESTENABLE") : 0L).Set("ALPHABLENDENABLE", st.Has("ALPHABLENDENABLE") ? st.Get("ALPHABLENDENABLE") : 0L))
                    .Set("generated_textures", generated).Set("note", note));
            }
            return reqs;
        }
        static void SetPair(List<KeyValuePair<string, string>> d, string key, string value)
        {
            for (int i = 0; i < d.Count; i++) if (d[i].Key == key) { d[i] = new KeyValuePair<string, string>(key, value); return; }
            d.Add(new KeyValuePair<string, string>(key, value));
        }

        // ===================================================================================================== ground: roles, maps
        /// texture_roles: {texture (lower case): role} in first-seen order.
        static JObj TextureRoles(Source src)
        {
            var roles = new JObj();
            for (int k = 0; k < src.mats.Count; k++)
                foreach (string t in src.MaterialTextures(k))
                {
                    string n = Lower(t);
                    if (roles.Has(n)) continue;
                    string role;
                    if (n.Contains("norm")) role = "normal";
                    else if (TILING_SHARED.Contains(n)) role = "tiling";
                    else if (Regex.IsMatch(n, "specmask|trackmask|undershadow|loglines|logmarks", RX)) role = "other";
                    else if (Regex.IsMatch(n, "occ|_ao$", RX)) role = "occ";
                    else if (Regex.IsMatch(n, "tbase|_base$|_tex$", RX)) role = "base";
                    else if (Regex.IsMatch(n, "peaks|jumplips", RX)) role = "peaks";
                    else if (Regex.IsMatch(n, "mask|msk|masque", RX)) role = "mask";
                    else if (Regex.IsMatch(n, "light|shadow", RX)) role = "shadow";
                    else if (n == "editor_default_white") role = "white";
                    else role = "other";
                    roles.Set(n, role);
                }
            return roles;
        }
        /// ruts_material: the kept terrain material that carries the ruts texture with the most triangles (-1 = none).
        static int RutsMaterial(Source src)
        {
            EXCLUDE.TryGetValue(src.lv.disc, out string[] ex); ex = ex ?? new string[0];
            int best = -1;
            for (int k = 0; k < src.mats.Count; k++)
            {
                if (Array.IndexOf(ex, src.mats[k]) >= 0) continue;
                if (src.MaterialTexturesLower(k).Contains(RUTS_TEX) && (best < 0 || src.T.materials[k].TriCount > src.T.materials[best].TriCount)) best = k;
            }
            return best;
        }

        sealed class Fit { public double[] coef; public double residual; public double[] span; public bool planar; public int nverts; }
        sealed class UvMap { public string id; public double[] coef; public List<string> sources = new List<string>(); public int nverts; }

        /// planar_fits: per terrain material and UV set the least-squares planar fit uv = [x, z, 1] @ coef (3, 2).
        static List<Dictionary<string, Fit>> PlanarFits(Source src)
        {
            var o = new List<Dictionary<string, Fit>>();
            foreach (var m in src.T.materials)
            {
                var r = new Dictionary<string, Fit>(StringComparer.Ordinal);
                o.Add(r);
                if (m.tris.Length == 0) continue;
                int[] used = UniqueVerts(m.tris);
                foreach (string nm in new[] { "uv0", "uv1" })
                {
                    float[] uvs = nm == "uv0" ? m.uv0 : m.uv1;
                    var X = new List<double>(3 * used.Length); var uv = new List<double>(2 * used.Length);
                    foreach (int v in used)
                    {
                        double a = uvs[2 * v], b = uvs[2 * v + 1];
                        if (!Finite(a) || !Finite(b)) continue;
                        X.Add(m.pos[3 * v]); X.Add(m.pos[3 * v + 2]); X.Add(1.0); uv.Add(a); uv.Add(b);
                    }
                    int n = uv.Count / 2; double[] Xa = X.ToArray(), ua = uv.ToArray();
                    double[] coef = Lstsq(Xa, ua, n, 2);
                    double res = Residual(Xa, coef, ua, n, 2);
                    double[] span = { Math.Max(Math.Abs(coef[0]), Math.Abs(coef[2])) * (HF_SIZE * HF_RES), Math.Max(Math.Abs(coef[1]), Math.Abs(coef[3])) * (HF_SIZE * HF_RES) };
                    bool planar = res < 1e-3 && 0.5 < span[0] && 0.5 < span[1] && span[0] < 2.0 && span[1] < 2.0;
                    r[nm] = new Fit { coef = coef, residual = res, span = span, planar = planar, nverts = n };
                }
            }
            return o;
        }
        static bool SameMap(double[] a, double[] b)
        {
            double d0 = 0, d1 = 0;
            for (int i = 0; i < 4; i++) d0 = Math.Max(d0, Math.Abs(a[i] - b[i]));
            for (int i = 4; i < 6; i++) d1 = Math.Max(d1, Math.Abs(a[i] - b[i]));
            return d0 < 2e-5 && d1 < 2e-3;
        }
        /// candidate_maps: the distinct planar maps among the material UV sets + the terrain bbox map.
        static List<UvMap> CandidateMaps(Source src, List<Dictionary<string, Fit>> fits)
        {
            var maps = new List<UvMap>();
            for (int k = 0; k < fits.Count; k++)
                foreach (string nm in new[] { "uv0", "uv1" })
                {
                    if (!fits[k].TryGetValue(nm, out Fit f) || !f.planar) continue;
                    UvMap hit = maps.Find(mp => SameMap(mp.coef, f.coef));
                    if (hit == null) { var mp = new UvMap { id = "map" + Inv(maps.Count), coef = f.coef, nverts = f.nverts }; mp.sources.Add(src.mats[k] + ":" + nm); maps.Add(mp); }
                    else hit.sources.Add(src.mats[k] + ":" + nm);
                }
            double[] b0 = src.C.bmin, b1 = src.C.bmax;
            double sx = b1[0] - b0[0], sz = b1[2] - b0[2];
            double[] bbox = { 1 / sx, 0, 0, -1 / sz, -b0[0] / sx, b1[2] / sz };
            if (!maps.Exists(mp => SameMap(mp.coef, bbox))) { var mp = new UvMap { id = "map" + Inv(maps.Count), coef = bbox, nverts = 0 }; mp.sources.Add("trncol bbox"); maps.Add(mp); }
            return maps;
        }

        /// An image for sample_tex: (h, w, c) uint8 texels read as float32 / 255 (texture_rgba) or float64 values (c = 1).
        sealed class FImg
        {
            public int h, w, c; public byte[] b; public double[] d;
            /// The texel as texture_rgba gives it: uint8 / 255.0 in float32 (a table of the 256 values).
            public float F(long index) => s_u8[b[index]];
            static readonly float[] s_u8 = MakeTable();
            static float[] MakeTable() { var t = new float[256]; for (int i = 0; i < 256; i++) t[i] = i / 255f; return t; }
        }

        static FImg TextureRgba(Source src, string name)
        {
            byte[] dat = src.Data(name, "texture", out _);
            if (dat == null) return null;
            UxImage im = new UxTexture(dat).Rgba();
            return new FImg { h = im.h, w = im.w, c = im.c, b = im.data };
        }

        /// sample_tex(img, u, v)[..., ch]: bilinear, wrap; float32 texels are widened, the blend is float64.
        static double Sample(FImg img, int ch, double u, double v)
        {
            int H = img.h, W = img.w, C = img.c;
            double x = u * W - 0.5, y = v * H - 0.5;
            double xf = Math.Floor(x), yf = Math.Floor(y);
            double fx = x - xf, fy = y - yf;
            long x0 = (long)xf % W, y0 = (long)yf % H; if (x0 < 0) x0 += W; if (y0 < 0) y0 += H;
            long x1 = (x0 + 1) % W, y1 = (y0 + 1) % H;
            double a, b, c, d;
            if (img.b != null) { a = img.F((y0 * W + x0) * C + ch); b = img.F((y0 * W + x1) * C + ch); c = img.F((y1 * W + x0) * C + ch); d = img.F((y1 * W + x1) * C + ch); }
            else { a = img.d[(y0 * W + x0) * C + ch]; b = img.d[(y0 * W + x1) * C + ch]; c = img.d[(y1 * W + x0) * C + ch]; d = img.d[(y1 * W + x1) * C + ch]; }
            return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
        }
        /// apply_map(coef, x, z).
        static void ApplyMap(double[] coef, double x, double z, out double u, out double v)
        {
            u = x * coef[0] + z * coef[2] + coef[4];
            v = x * coef[1] + z * coef[3] + coef[5];
        }
        /// sampled(n)[..., ch] on the (size, size) grid of resolution res.
        static double[] SampleGrid(FImg img, double[] coef, int ch, int size, double res, int jobs)
        {
            var o = new double[size * size];
            Parallel.For(0, size, new ParallelOptions { MaxDegreeOfParallelism = jobs }, r =>
            {
                double z = Z1 - (r + 0.5) * res;
                for (int c = 0; c < size; c++)
                {
                    double x = X0 + (c + 0.5) * res;
                    ApplyMap(coef, x, z, out double u, out double v);
                    o[r * size + c] = Sample(img, ch, u, v);
                }
            });
            return o;
        }

        /// sampled(n)[..., :3].mean(-1) at one grid point: (r + g + b) / 3 of the three sampled channels.
        static double SampleRgbMean(FImg img, double[] coef, double x, double z)
        {
            ApplyMap(coef, x, z, out double u, out double v);
            return ((Sample(img, 0, u, v) + Sample(img, 1, u, v)) + Sample(img, 2, u, v)) / 3;
        }

        static double Mean(double[] a) => UxNp.Sum(a) / a.Length;
        /// a.std(): sqrt(mean((a - mean)^2)).
        static double Std(double[] a, double mean)
        {
            var x = new double[a.Length]; for (int i = 0; i < x.Length; i++) { double d = a[i] - mean; x[i] = d * d; }
            return Math.Sqrt(UxNp.Sum(x) / a.Length);
        }

        /// map_references: z-scored 1 ft reference images inside the render footprint (height, ruts-material coverage, every material's
        /// coverage, Gaussian 2 px) and the footprint.
        static List<double[]> MapReferences(Source src, out bool[] foot)
        {
            int S = HF_SIZE, N = S * S;
            Raster(src.rV, src.rF, src.rF.Length / 3, X0, Z1, HF_RES, S, S, true, out double[] top, out int[] tri);
            var ft = new bool[N]; var mat = new int[N]; int nf = 0;
            for (int i = 0; i < N; i++) { ft[i] = Finite(top[i]); if (ft[i]) nf++; mat[i] = tri[i] >= 0 ? src.rM[tri[i]] : -1; }
            foot = ft;
            var refs = new List<double[]>();
            var height = new double[N]; for (int i = 0; i < N; i++) height[i] = ft[i] ? top[i] : 0.0;
            refs.Add(height);
            double[] Coverage(int k)
            {
                var a = new double[N]; bool any = false;
                for (int i = 0; i < N; i++) if (mat[i] == k) { a[i] = 1.0; any = true; }
                return any ? UxImaging.GaussianFilter(a, S, S, 2.0) : null;
            }
            int rk = RutsMaterial(src);
            if (rk >= 0) refs.Add(Coverage(rk) ?? new double[N]);
            for (int k = 0; k < src.mats.Count; k++) { double[] c = Coverage(k); if (c != null) refs.Add(c); }
            var o = new List<double[]>();
            foreach (double[] r in refs)
            {
                var v = new double[nf]; int at = 0;
                for (int i = 0; i < N; i++) if (ft[i]) v[at++] = r[i];
                if (nf == 0) continue;
                double mean = Mean(v), std = Std(v, mean);
                if (!(std > 1e-9)) continue;
                for (int i = 0; i < nf; i++) v[i] = (v[i] - mean) / std;
                o.Add(v);
            }
            return o;
        }

        /// choose_map: the planar map under which the texture correlates best (max |Pearson r| over RGB and the references).
        static string ChooseMap(FImg img, List<UvMap> maps, List<double[]> refs, bool[] foot, int jobs, out JObj scores)
        {
            int S = HF_SIZE; int nf = (int)Count(foot);
            var footIdx = new int[nf]; { int at = 0; for (int i = 0; i < foot.Length; i++) if (foot[i]) footIdx[at++] = i; }
            scores = new JObj();
            var raw = new List<KeyValuePair<string, double>>();
            foreach (var mp in maps)
            {
                double best = 0.0;
                for (int ch = 0; ch < 3; ch++)
                {
                    var a = new double[nf]; double[] coef = mp.coef; int chan = ch;
                    Parallel.For(0, nf, new ParallelOptions { MaxDegreeOfParallelism = jobs }, i =>
                    {
                        int r = footIdx[i] / S, c = footIdx[i] % S;
                        ApplyMap(coef, X0 + (c + 0.5) * HF_RES, Z1 - (r + 0.5) * HF_RES, out double u, out double v);
                        a[i] = Sample(img, chan, u, v);
                    });
                    if (nf == 0) continue;
                    double mean = Mean(a), std = Std(a, mean);
                    if (std < 1e-6) continue;
                    for (int i = 0; i < nf; i++) a[i] = (a[i] - mean) / std;
                    var corr = new double[refs.Count];
                    Parallel.For(0, refs.Count, new ParallelOptions { MaxDegreeOfParallelism = jobs }, k =>
                    {
                        double[] r = refs[k]; var p = new double[nf];
                        for (int i = 0; i < nf; i++) p[i] = a[i] * r[i];
                        corr[k] = Math.Abs(UxNp.Sum(p) / nf);
                    });
                    foreach (double cv in corr) if (cv > best) best = cv;
                }
                raw.Add(new KeyValuePair<string, double>(mp.id, best));
                scores.Set(mp.id, UxNp.PyRound(best, 4));
            }
            // max(sorted(scores), key=...): the first of the largest in name order
            raw.Sort((x, y) => string.CompareOrdinal(x.Key, y.Key));
            string bestId = raw[0].Key; double bs = raw[0].Value;
            foreach (var kv in raw) if (kv.Value > bs) { bs = kv.Value; bestId = kv.Key; }
            return bestId;
        }

        // ===================================================================================================== shader palette
        static int PyFind(byte[] hay, byte[] needle, int start)
        {
            if (start < 0) start = Math.Max(0, hay.Length + start);
            for (int i = start; i + needle.Length <= hay.Length; i++)
            {
                bool ok = true; for (int k = 0; k < needle.Length && ok; k++) ok = hay[i + k] == needle[k];
                if (ok) return i;
            }
            return -1;
        }
        /// shader_palette: the literal colour constants (multiples of 1/255) of terrain material k's Untamed pixel shader.
        static List<int[]> ShaderPalette(Source src, int k)
        {
            var o = new List<int[]>();
            string file = "shader/" + Lower(src.mats[k]) + ".shader";
            if (!src.disc.HasExtractFile(file)) return o;
            byte[] d = src.disc.ReadExtractFile(file, src.cancel);
            try
            {
                if (d.Length < 8) return o;
                uint cnt = UxBin.U32BE(d, 4); long q = 8; byte[] ps = null;
                for (uint i = 0; i < cnt; i++)
                {
                    if (q + 8 > d.Length) return o;
                    long nl = UxBin.U32BE(d, (int)q + 4); q += 8;
                    long end = Math.Min(q + nl, d.Length); int z = (int)q; while (z < end && d[z] != 0) z++;
                    string nm = UxBin.Latin1(d, (int)q, z - (int)q); q += nl;
                    if (q + 4 > d.Length) return o;
                    long sz = UxBin.U32BE(d, (int)q); q += 4;
                    if (nm == "SharedPixelShader") ps = UxBin.PySlice(d, (int)Math.Min(q, d.Length), (int)Math.Min(q + sz, d.Length));
                    q += sz;
                }
                if (ps == null) return o;
                int ia = PyFind(ps, Encoding.ASCII.GetBytes("ps_3_0"), 0);
                int j = PyFind(ps, new byte[] { 0, 0, 1, 0 }, ia);
                if (j + 8 > ps.Length || j + 4 < 0) return o;
                long mcs = UxBin.U32BE(ps, j + 4);
                long start = ps.Length - mcs;
                if (!(j + 8 <= start && start <= ps.Length)) return o;
                long nvec = (start - (j + 8)) / 16;
                for (long r = 0; r < nvec; r++)
                {
                    int at = (int)(start - 16 * nvec + 16 * r);
                    float c0 = UxBin.F32BE(ps, at), c1 = UxBin.F32BE(ps, at + 4), c2 = UxBin.F32BE(ps, at + 8);
                    float[] c = { c0, c1, c2 }; bool ok = true; var rgb = new int[3];
                    float mn = Math.Min(c0, Math.Min(c1, c2)), mx = Math.Max(c0, Math.Max(c1, c2));
                    for (int i = 0; i < 3 && ok; i++)
                    {
                        float s = c[i] * 255f; float rr = (float)Math.Round(s, MidpointRounding.ToEven);
                        ok = c[i] > 0.03f && c[i] < 1.0f && Math.Abs(s - rr) < 0.02f;
                        rgb[i] = (int)rr;
                    }
                    if (ok && (float)(mx - mn) > 0) o.Add(rgb);
                }
            }
            catch (IndexOutOfRangeException) { return new List<int[]>(); }
            catch (ArgumentException) { return new List<int[]>(); }
            return o;
        }
        static string PyTuples(List<int[]> pal)
        {
            var sb = new StringBuilder("[");
            for (int i = 0; i < pal.Count; i++) { if (i > 0) sb.Append(", "); sb.Append('(').Append(Inv(pal[i][0])).Append(", ").Append(Inv(pal[i][1])).Append(", ").Append(Inv(pal[i][2])).Append(')'); }
            return sb.Append(']').ToString();
        }
        static int[] DirtTint(Source src, int rutsK, int[] over, out string source)
        {
            if (over != null && over.Length > 0) { source = "override"; return new[] { over[0], over[1], over[2] }; }
            var pal = new List<int[]>();
            var order = new List<int>(); if (rutsK >= 0) order.Add(rutsK);
            for (int k = 0; k < src.mats.Count; k++) if (k != rutsK) order.Add(k);
            foreach (int k in order)
                foreach (int[] c in ShaderPalette(src, k))
                    if (!pal.Exists(p => p[0] == c[0] && p[1] == c[1] && p[2] == c[2])) pal.Add(c);
            foreach (int[] c in pal)
            {
                double lum = 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2];
                if (c[0] >= c[1] && c[1] >= c[2] && (double)(c[0] - c[2]) / Math.Max(c[0], 1) > 0.15 && TINT_MIN_LUM <= lum && lum <= 200)
                {
                    source = "shader literal of " + (rutsK >= 0 ? "the ruts material" : "a terrain material") + " (palette " + PyTuples(pal) + ")";
                    return c;
                }
            }
            source = "default (shader palette " + PyTuples(pal) + " has no dirt colour)";
            return (int[])DEFAULT_TINT.Clone();
        }

        // ===================================================================================================== ground: lap, ruts
        /// lap_polyline: the lap's node positions (tdf07_to_reflex split / structure / lap_walk), closed?, global 5 (the lap length).
        static double[] LapPolyline(Source src, out bool closed, out double g5)
        {
            byte[] data = src.disc.ReadTdf(src.cancel);
            int bas;
            if (data.Length >= 0x20 && UxBin.U32LE(data, 0) == (uint)(data.Length - 4) && UxBin.U32BE(data, 0x18) == 260) bas = 4;
            else if (data.Length >= 0x1c && UxBin.U32BE(data, 0x14) == 260) bas = 0;
            else throw new UxException("not a TDF: no 260-byte path field at 0x14 (tdf07) or 0x18 (Reflex)");
            int p = bas + 24 + (int)UxBin.U32BE(data, bas + 20);
            // structure(items): globals, then sections (item 11) of fields and nodes (item 20)
            var glob = new List<(uint id, int at, int n)>();
            var secFields = new List<List<(uint id, int at, int n)>>(); var secNodes = new List<List<List<(uint id, int at, int n)>>>();
            List<(uint id, int at, int n)> curNode = null; int curSec = -1;
            while (p + 8 <= data.Length)
            {
                uint id = UxBin.U32BE(data, p), s = UxBin.U32BE(data, p + 4);
                int n = s == 0x10 ? 12 : (int)s;
                if (n < 0 || p + 8 + (long)n > data.Length) throw new UxException("truncated item " + id + " at 0x" + p.ToString("x"));
                var it = (id, p + 8, n);
                if (id == 11) { secFields.Add(new List<(uint, int, int)>()); secNodes.Add(new List<List<(uint, int, int)>>()); curSec = secFields.Count - 1; curNode = null; }
                else if (id == 20) { UxBin.Require(curSec >= 0, "tdf: a node before any section"); curNode = new List<(uint, int, int)> { it }; secNodes[curSec].Add(curNode); }
                else if (curNode != null) curNode.Add(it);
                else if (curSec >= 0) secFields[curSec].Add(it);
                else glob.Add(it);
                p += 8 + n;
            }
            if (p != data.Length) throw new UxException("stream does not end on an item (" + (data.Length - p) + " trailing bytes)");
            int First(List<(uint id, int at, int n)> items, uint id) { foreach (var it in items) if (it.id == id) return it.at; return -1; }
            int g9 = First(glob, 9); UxBin.Require(g9 >= 0, "tdf: no finish line (global 9)");
            int a = (int)UxBin.U32BE(data, g9 + 40), b = (int)UxBin.U32BE(data, g9 + 44);
            var lap = new List<(int s, int n)>(); closed = false;
            int total = 0; foreach (var s in secNodes) total += s.Count;
            int si = a, ni = b;
            for (int step = 0; step < total + 2; step++)
            {
                lap.Add((si, ni));
                ni++;
                if (ni >= secNodes[si].Count)
                {
                    int nx = -1; foreach (var f in secFields[si]) if (f.id == 14) { nx = (int)UxBin.U32BE(data, f.at + 4); break; }
                    if (nx < 0) break;
                    si = nx; ni = 0;
                }
                if (si == a && ni == b) { closed = true; break; }
            }
            var pts = new double[3 * lap.Count];
            for (int i = 0; i < lap.Count; i++)
            {
                int at = -1; foreach (var it in secNodes[lap[i].s][lap[i].n]) if (it.id == 21) at = it.at;        // node_dict: the last one
                UxBin.Require(at >= 0, "tdf: a lap node without a position (item 21)");
                for (int c = 0; c < 3; c++) pts[3 * i + c] = UxBin.F32BE(data, at + 4 * c);
            }
            int g5at = First(glob, 5); UxBin.Require(g5at >= 0, "tdf: no global 5");
            g5 = UxBin.F32BE(data, g5at);
            return pts;
        }

        /// lap_distance: the distance (ft) from every grid pixel centre to the closed lap polyline.
        static double[] LapDistance(double[] pts, int size, double res)
        {
            int np = pts.Length / 3; var m = new bool[size * size];
            for (int i = 0; i < np; i++)
            {
                int j = (i + 1) % np;
                double px = pts[3 * i], pz = pts[3 * i + 2], qx = pts[3 * j], qz = pts[3 * j + 2];
                double Lh = Math.Sqrt((qx - px) * (qx - px) + (qz - pz) * (qz - pz));
                int n = Math.Max(2, (int)(Lh / (res / 2)) + 1);
                double step = 1.0 / (n - 1);
                for (int k = 0; k < n; k++)
                {
                    double t = k == n - 1 ? 1.0 : k * step;         // np.linspace(0, 1, n)
                    double sx = px + (qx - px) * t, sz = pz + (qz - pz) * t;
                    double c = Math.Floor((sx - X0) / res), r = Math.Floor((Z1 - sz) / res);
                    if (c >= 0 && c < size && r >= 0 && r < size) m[(int)r * size + (int)c] = true;
                }
            }
            var not = new bool[m.Length]; for (int i = 0; i < not.Length; i++) not[i] = !m[i];
            double[] d = UxNd.DistanceTransformEdtDistances(not, size, size);
            for (int i = 0; i < d.Length; i++) d[i] *= res;
            return d;
        }

        /// ruts_test of one mask channel: sums over the footprint and over the pixels within RUTS_NEAR_FT of the lap. The channel is
        /// float64 (the sampled TMask) or float32 (the final mask).
        static JObj RutsChannel(char name, double[] d64, float[] f32, bool[] foot, bool[] near, long nFoot, long nNear)
        {
            double tot, nearSum;
            if (d64 != null)
            {
                var a = new double[nFoot]; var b = new double[nNear]; int ia = 0, ib = 0;
                for (int i = 0; i < foot.Length; i++) { if (foot[i]) a[ia++] = d64[i]; if (near[i]) b[ib++] = d64[i]; }
                tot = UxNp.Sum(a); nearSum = UxNp.Sum(b);
            }
            else
            {
                var a = new float[nFoot]; var b = new float[nNear]; int ia = 0, ib = 0;
                for (int i = 0; i < foot.Length; i++) { if (foot[i]) a[ia++] = f32[i]; if (near[i]) b[ib++] = f32[i]; }
                tot = UxNp.Sum(a); nearSum = UxNp.Sum(b);
            }
            double area = (double)nNear / Math.Max(nFoot, 1);
            double frac = tot > 0 ? nearSum / tot : 0.0;
            return new JObj().Set("channel", name.ToString()).Set("mass_fraction_near", UxNp.PyRound(frac, 4)).Set("area_fraction_near", UxNp.PyRound(area, 4))
                .Set("enrichment", area > 0 && tot > 0 ? UxNp.Round(frac / area, 3) : 0.0)             // round(np.float64, 3): numpy's rounding
                .Set("coverage", UxNp.Round(tot / Math.Max(nFoot, 1), 4));
        }

        // ===================================================================================================== tiling textures
        /// is_grey: the channel means (float32, summed row by row) differ by less than 0.05.
        static bool IsGrey(FImg img)
        {
            long n = (long)img.h * img.w; float mn = float.PositiveInfinity, mx = float.NegativeInfinity;
            for (int ch = 0; ch < 3; ch++)
            {
                float s = 0; for (long i = 0; i < n; i++) s = (float)(s + img.F(i * img.c + ch));
                float mean = (float)((double)s / n);
                if (mean < mn) mn = mean; if (mean > mx) mx = mean;
            }
            return (double)(float)(mx - mn) < 0.05;
        }
        /// tint_texture + the uint8 conversion of to_tile: a grey tiling texture tinted by luminance (lum / mean(lum) * tint, clipped),
        /// or the texture's own RGB.
        static UxImage TileRgb(FImg img, int[] tint, bool grey)
        {
            int H = img.h, W = img.w, C = img.c; var o = new byte[H * W * 3];
            if (!grey)
            {
                for (int i = 0; i < H * W; i++) for (int ch = 0; ch < 3; ch++) o[3 * i + ch] = U8(img.F(i * C + ch));
                return new UxImage(o, H, W, 3);
            }
            // lum = img[..., :3] @ float32(0.299, 0.587, 0.114): cblas_sgemv on the reference machine, per row of W pixels
            float w0 = 0.299f, w1 = 0.587f, w2 = 0.114f;
            var lum = new float[H * W];
            for (int i = 0; i < H * W; i++)
            {
                float r = img.F(i * C), g = img.F(i * C + 1), b = img.F(i * C + 2);
                lum[i] = W == 1 ? UxNp.Fma(b, w2, UxNp.Fma(g, w1, (float)(r * w0))) : UxNp.Fma(b, w2, UxNp.Fma(r, w0, (float)(g * w1)));
            }
            float mean = (float)((double)UxNp.Sum(lum) / lum.Length);
            float div = (float)Math.Max((double)mean, 1e-6);
            var t = new float[3]; for (int ch = 0; ch < 3; ch++) t[ch] = (float)((float)tint[ch] / 255f);
            for (int i = 0; i < H * W; i++)
            {
                float k = (float)(lum[i] / div);
                for (int ch = 0; ch < 3; ch++) o[3 * i + ch] = U8((float)(k * t[ch]));
            }
            return new UxImage(o, H, W, 3);
        }
        /// (np.clip(v, 0, 1) * 255 + 0.5).astype(np.uint8) of a float32.
        static byte U8(float v)
        {
            v = v < 0f ? 0f : v > 1f ? 1f : v;
            return (byte)(int)(float)((float)(v * 255f) + 0.5f);
        }
        static byte U8(double v)
        {
            v = v < 0 ? 0 : v > 1 ? 1 : v;
            return (byte)(int)(v * 255 + 0.5);
        }
        /// to_tile: a non-square texture is first tiled along its short axis, then resized to size x size (LANCZOS).
        static UxImage ToTile(UxImage im, int size)
        {
            int H = im.h, W = im.w;
            if (H != W)
            {
                if (H > W && H % W == 0)
                {
                    int rep = H / W; var d = new byte[H * W * rep * 3];
                    for (int y = 0; y < H; y++) for (int k = 0; k < rep; k++) Buffer.BlockCopy(im.data, y * W * 3, d, (y * W * rep + k * W) * 3, W * 3);
                    im = new UxImage(d, H, W * rep, 3);
                }
                else if (W > H && W % H == 0)
                {
                    int rep = W / H; var d = new byte[H * rep * W * 3];
                    for (int k = 0; k < rep; k++) Buffer.BlockCopy(im.data, 0, d, k * H * W * 3, H * W * 3);
                    im = new UxImage(d, H * rep, W, 3);
                }
            }
            if (im.w != size || im.h != size) im = UxPil.Resize(im, size, size, UxPil.Filter.Lanczos);
            return im;
        }
        /// np.asarray(im, np.float32).reshape(-1, 3).mean(0) / 255.0 (float32).
        static float[] TileMean(UxImage im)
        {
            long n = (long)im.w * im.h; var o = new float[3];
            for (int ch = 0; ch < 3; ch++)
            {
                float s = 0; for (long i = 0; i < n; i++) s = (float)(s + im.data[i * 3 + ch]);
                o[ch] = (float)((float)((double)s / n) / 255f);
            }
            return o;
        }

        /// write_pc_dxt1: an RGB image as a Reflex PC v4 DXT1 .texture with a full mip chain (each mip resized from the full image).
        static byte[] PcDxt1(UxImage im)
        {
            int w = im.w, h = im.h; int n = 1; while ((1 << n) <= Math.Max(w, h)) n++;         // int(np.log2(max(w, h))) + 1
            var payload = new List<byte[]>(); var table = new List<(int size, int pitch)>();
            for (int i = 0; i < n; i++)
            {
                int mw = Math.Max(1, w >> i), mh = Math.Max(1, h >> i);
                UxImage cur = i == 0 ? im : UxPil.Resize(im, mw, mh, UxPil.Filter.Lanczos);
                byte[] b = UxPil.BcEncode(cur, false);
                payload.Add(b); table.Add((b.Length, UxTex.PcPitch(UxTex.PC_DXT1, mw)));
            }
            byte[] data = UxTex.BuildPc(new List<(int mips, int w, int h, byte[] fmt)> { (n, w, h, UxTex.PC_DXT1) }, table, payload);
            var t = new UxPcTexture(data);
            UxBin.Require(t.end == data.Length && t.recs[0].mips == n && t.recs[0].w == w && t.recs[0].h == h, "generated texture does not re-read");
            return data;
        }

        // ===================================================================================================== contact test
        sealed class Prop { public double[] M, cen, ex; }
        /// contact_props: the large static placed models (keep / demote CollisionRSData / ModelRSData) for the object-contact test.
        static List<Prop> ContactProps(Source src)
        {
            var surf = new Dictionary<string, JObj>(StringComparer.Ordinal);
            if (src.closure.Get("surface") is JObj sc)
                foreach (var kv in sc.items)
                {
                    var rec = UxJson.Obj(kv.Value);
                    var names = new List<string> { kv.Key };
                    if (rec.Get("ref_names") is List<object> rn) foreach (object x in rn) names.Add(System.Convert.ToString(x, System.Globalization.CultureInfo.InvariantCulture));
                    foreach (string nm in names) { string k = Lower(nm); if (!surf.ContainsKey(k)) surf[k] = rec; }
                }
            var cache = new Dictionary<string, double[]>(StringComparer.Ordinal); var o = new List<Prop>();
            foreach (object eo in UxJson.Arr(src.lj.Get("entities")))
            {
                var e = UxJson.Obj(eo);
                string cls = e.Get("cls") as string, type = e.Get("type") as string;
                if ((cls != "keep" && cls != "demote") || (type != "CollisionRSData" && type != "ModelRSData")) continue;
                var props = UxJson.Obj(e.Get("props") ?? throw new UxException("entity without props"));
                string sa = (props.Get("SurfaceAsset") as JObj)?.Get("asset") as string;
                if (string.IsNullOrEmpty(sa) || !surf.TryGetValue(Lower(sa), out JObj rec)) continue;
                string file = UxJson.Str(rec.Get("file"));
                if (!cache.TryGetValue(file, out double[] b))
                {
                    b = null;
                    try
                    {
                        var gs = UxMesh.ReadSurface(src.disc.ReadExtractFile(file, src.cancel));
                        double[] lo = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, hi = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                        bool any = false;
                        foreach (var g in gs)
                        {
                            if (g.pos == null) continue;
                            for (int i = 0; i + 2 < g.pos.Length; i += 3)
                            {
                                double x = g.pos[i], y = g.pos[i + 1], z = g.pos[i + 2];
                                if (!Finite(x) || !Finite(y) || !Finite(z)) continue;
                                any = true;
                                if (x < lo[0]) lo[0] = x; if (x > hi[0]) hi[0] = x; if (y < lo[1]) lo[1] = y; if (y > hi[1]) hi[1] = y; if (z < lo[2]) lo[2] = z; if (z > hi[2]) hi[2] = z;
                            }
                        }
                        if (any) b = new[] { (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2, (hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2 };
                    }
                    catch (UxException) { b = null; }
                    cache[file] = b;
                }
                if (b == null) continue;
                double mxz = Math.Max(b[3], b[5]);
                if (!(OCC_HALF_LO <= mxz && mxz <= OCC_HALF_HI)) continue;
                double[] M = Matrix(e); for (int i = 0; i < 16; i++) M[i] = (float)M[i];
                var c = new double[3];
                for (int j = 0; j < 3; j++) c[j] = UxNp.Fma(b[2], M[8 + j], UxNp.Fma(b[1], M[4 + j], b[0] * M[j])) + M[12 + j];        // cen @ M[:3, :3] + M[3, :3]
                if (Math.Abs(c[0]) > OCC_REACH_FT || Math.Abs(c[2]) > OCC_REACH_FT) continue;
                o.Add(new Prop { M = M, cen = new[] { b[0], b[1], b[2] }, ex = new[] { b[3], b[4], b[5] } });
            }
            return o;
        }
        static double[] Linspace9(double a, double b)
        {
            double step = (b - a) / 8; var o = new double[9];
            for (int i = 0; i < 8; i++) o[i] = i * step + a;
            o[8] = b;
            return o;
        }
        /// contact_medians: {map id: median over the props of mean(grey inside the footprint) / mean(grey in the ring around it)}.
        static JObj ContactMedians(FImg img, List<UvMap> maps, List<Prop> props)
        {
            var g = new double[img.h * img.w];
            for (int i = 0; i < g.Length; i++) g[i] = (((double)img.F(i * img.c) + img.F(i * img.c + 1)) + img.F(i * img.c + 2)) / 3;
            var grey = new FImg { h = img.h, w = img.w, c = 1, d = g };
            var pts = new List<(double[] x, double[] z, bool[] ins)>();
            foreach (var pr in props)
            {
                double[] xs = Linspace9(-pr.ex[0] - OCC_RING_FT, pr.ex[0] + OCC_RING_FT), zs = Linspace9(-pr.ex[2] - OCC_RING_FT, pr.ex[2] + OCC_RING_FT);
                var x = new double[81]; var z = new double[81]; var ins = new bool[81];
                for (int i = 0; i < 9; i++)
                    for (int j = 0; j < 9; j++)
                    {
                        double lx = xs[j] + pr.cen[0], ly = pr.cen[1], lz = zs[i] + pr.cen[2]; int k = i * 9 + j;
                        // W = L @ M[:3, :3] + M[3, :3] (dgemm chain)
                        x[k] = UxNp.Fma(lz, pr.M[8], UxNp.Fma(ly, pr.M[4], lx * pr.M[0])) + pr.M[12];
                        z[k] = UxNp.Fma(lz, pr.M[10], UxNp.Fma(ly, pr.M[6], lx * pr.M[2])) + pr.M[14];
                        ins[k] = Math.Abs(xs[j]) <= pr.ex[0] && Math.Abs(zs[i]) <= pr.ex[2];
                    }
                pts.Add((x, z, ins));
            }
            var o = new JObj();
            foreach (var mp in maps)
            {
                var rs = new double[pts.Count];
                for (int p = 0; p < pts.Count; p++)
                {
                    var (x, z, ins) = pts[p];
                    var a = new List<double>(); var b = new List<double>();
                    for (int k = 0; k < 81; k++)
                    {
                        ApplyMap(mp.coef, x[k], z[k], out double u, out double v);
                        double sv = Sample(grey, 0, u, v);
                        if (ins[k]) a.Add(sv); else b.Add(sv);
                    }
                    double ma = a.Count > 0 ? UxNp.Sum(a.ToArray()) / a.Count : UxNp.NaN, mb = b.Count > 0 ? UxNp.Sum(b.ToArray()) / b.Count : UxNp.NaN;
                    rs[p] = ma / Math.Max(mb, 1e-3);
                }
                o.Set(mp.id, rs.Length > 0 ? UxNp.PyRound(UxImaging.MedianInPlace(rs), 4) : 1.0);
            }
            return o;
        }
        /// contact_choice: records the medians on a texture_maps record and switches the map when the test is decisive.
        static void ContactChoice(JObj rec, FImg img, List<UvMap> maps, List<Prop> props)
        {
            if (maps.Count < 2 || props.Count == 0) return;
            JObj med = ContactMedians(img, maps, props);
            rec.Set("contact", med); rec.Set("contact_props", (long)props.Count);
            var keys = new List<string>(); foreach (var kv in med.items) keys.Add(kv.Key);
            keys.Sort(string.CompareOrdinal);
            string best = keys[0]; foreach (string k in keys) if (UxJson.Num(med.Get(k)) < UxJson.Num(med.Get(best))) best = k;
            string cur = UxJson.Str(rec.Get("map"));
            double lay = med.Has(cur) ? UxJson.Num(med.Get(cur)) : 1.0, mb = UxJson.Num(med.Get(best));
            if (best != cur && mb < OCC_SWITCH_A && lay > OCC_SWITCH_B && lay - mb >= OCC_SWITCH_C)
            {
                rec.Set("map_layout", cur); rec.Set("map", best);
                rec.Set("map_rule", "contact (median " + UxImaging.PyFixed(mb, 3) + " under " + best + " vs " + UxImaging.PyFixed(UxJson.Num(med.Get(cur)), 3) + " under the layout choice " + cur + ")");
            }
        }

        // ===================================================================================================== ground
        sealed class Ground { public JObj json; public JObj files = new JObj(); public Dictionary<string, UxImage> tiles = new Dictionary<string, UxImage>(StringComparer.Ordinal); }

        static List<object> CoefJson(double[] coef)
        {
            var rows = new List<object>();
            for (int i = 0; i < 3; i++) rows.Add(L(UxNp.PyRound(coef[2 * i], 8), UxNp.PyRound(coef[2 * i + 1], 8)));
            return rows;
        }

        /// build_ground: the TopDown candidates, the DetailMasks and the tiling textures; the terrain.json ground section.
        static Ground BuildGround(Source src, Hf hfr, UxWork outdir, Action<string> log, UxTerrainOptions opt, int jobs, Action<string> writePng)
        {
            if (opt.occ_map != "contact" && opt.occ_map != "layout") throw new UxException("occ_map must be contact or layout, not '" + opt.occ_map + "'");
            var gr = new Ground();
            JObj roles = TextureRoles(src);
            var fits = PlanarFits(src);
            var maps = CandidateMaps(src, fits);
            List<double[]> refs = MapReferences(src, out bool[] foot1);
            var imgs = new Dictionary<string, FImg>(StringComparer.Ordinal); var chosen = new JObj();
            var roleNames = new List<string>(); foreach (var kv in roles.items) roleNames.Add(kv.Key);
            roleNames.Sort(string.CompareOrdinal);
            foreach (string n in roleNames)
            {
                string role = (string)roles.Get(n);
                if (role != "base" && role != "occ" && role != "mask" && role != "peaks" && role != "shadow" && role != "other") continue;
                FImg img = TextureRgba(src, n);
                if (img == null || img.h < 4) continue;
                imgs[n] = img;
                string best = ChooseMap(img, maps, refs, foot1, jobs, out JObj scores);
                chosen.Set(n, new JObj().Set("role", role).Set("map", best).Set("scores", scores).Set("size", L((long)img.w, (long)img.h)));
                src.cancel.ThrowIfCancellationRequested();
            }
            string ssm = src.tinfo.Get("static_shadow_map") as string;
            if (!string.IsNullOrEmpty(ssm) && !imgs.ContainsKey(Lower(ssm)) && Lower(ssm) != "editor_default_white")
            {
                FImg img = TextureRgba(src, ssm);
                if (img != null && img.h >= 4)
                {
                    imgs[Lower(ssm)] = img;
                    string best = ChooseMap(img, maps, refs, foot1, jobs, out JObj scores);
                    chosen.Set(Lower(ssm), new JObj().Set("role", "shadow").Set("map", best).Set("scores", scores).Set("size", L((long)img.w, (long)img.h)).Set("static_shadow_map", true));
                }
            }
            var chosenSorted = new List<string>(); foreach (var kv in chosen.items) chosenSorted.Add(kv.Key);
            chosenSorted.Sort(string.CompareOrdinal);
            if (opt.occ_map == "contact" && maps.Count > 1)
            {
                List<Prop> props = null;
                foreach (string n in chosenSorted)
                {
                    var rec = (JObj)chosen.Get(n); string role = (string)rec.Get("role");
                    if (role != "occ" && role != "shadow") continue;
                    if (props == null) props = ContactProps(src);
                    ContactChoice(rec, imgs[n], maps, props);
                }
            }
            double[] CoefOf(string n) { string id = (string)((JObj)chosen.Get(n)).Get("map"); return maps.Find(mp => mp.id == id).coef; }
            int TS = TD_SIZE, TN = TS * TS;
            // the footprint at 2048 (the kept top surface) + the material per pixel
            var rkSet = new SortedSet<int>();
            for (int i = 0; i < src.nt; i++) if (!hfr.excl[i] && src.rtri[i] >= 0) rkSet.Add(src.rtri[i]);
            var rk = new int[rkSet.Count]; rkSet.CopyTo(rk);
            var frk = new int[3 * rk.Length]; for (int i = 0; i < rk.Length; i++) { frk[3 * i] = src.rF[3 * rk[i]]; frk[3 * i + 1] = src.rF[3 * rk[i] + 1]; frk[3 * i + 2] = src.rF[3 * rk[i] + 2]; }
            Raster(src.rV, frk, rk.Length, X0, Z1, TD_RES, TS, TS, true, out double[] top2, out int[] ttri);
            var foot2 = new bool[TN]; var mat2 = new int[TN]; long nFoot2 = 0;
            for (int i = 0; i < TN; i++) { foot2[i] = Finite(top2[i]); if (foot2[i]) nFoot2++; mat2[i] = ttri[i] >= 0 ? src.rM[rk[ttri[i]]] : -1; }
            top2 = null; ttri = null;
            // ---- layers
            int rutsK = RutsMaterial(src);
            var maskOrder = new List<string>(); string maskTex = null;
            int refK = rutsK >= 0 ? rutsK : 0;
            List<string> texs = src.MaterialTexturesLower(refK);
            var mi = new List<int>(); for (int i = 0; i < texs.Count; i++) if (roles.Get(texs[i]) as string == "mask") mi.Add(i);
            if (mi.Count > 0)
            {
                foreach (int i in mi)
                {
                    var after = new List<string>();
                    for (int j = i + 1; j < texs.Count; j++) if (roles.Get(texs[j]) as string == "tiling" && texs[j] != "hp_base_helper") after.Add(texs[j]);
                    if (after.Count > 0) { maskTex = texs[i]; maskOrder = after.GetRange(0, Math.Min(3, after.Count)); break; }
                }
                if (maskTex == null) maskTex = texs[mi[mi.Count - 1]];
            }
            string baseName = null;
            foreach (string t in BASE_TEX_PREF)
            {
                bool found = false;
                for (int k = 0; k < src.mats.Count && !found; k++) foreach (string x in src.MaterialTextures(k)) if (t == Lower(x)) { found = true; break; }
                if (found) { baseName = t; break; }
            }
            int[] tint = DirtTint(src, rutsK, opt.dirt_tint, out string tintSrc);
            // lap distance + ruts test
            double[] pts = LapPolyline(src, out bool closed, out double g5);
            double[] dist = LapDistance(pts, TS, TD_RES);
            var nearLap = new bool[TN]; long nNear = 0;
            for (int i = 0; i < TN; i++) { nearLap[i] = foot2[i] && dist[i] <= RUTS_NEAR_FT; if (nearLap[i]) nNear++; }
            dist = null;
            List<object> testTmask = null; var mappingNote = new List<object>();
            bool haveTm = maskTex != null && imgs.ContainsKey(maskTex);
            var tmF = new float[3][];
            if (haveTm)
            {
                testTmask = new List<object>();
                for (int ch = 0; ch < 3; ch++)
                {
                    double[] tm = SampleGrid(imgs[maskTex], CoefOf(maskTex), ch, TS, TD_RES, jobs);
                    testTmask.Add(RutsChannel("RGB"[ch], tm, null, foot2, nearLap, nFoot2, nNear));
                    tmF[ch] = new float[TN]; for (int i = 0; i < TN; i++) tmF[ch][i] = (float)tm[i];
                }
            }
            else maskOrder = new List<string>();
            src.cancel.ThrowIfCancellationRequested();
            // three channel slots: slot i = (the layer texture or null, the mask source)
            var slots = new string[3]; var srcs = new[] { "unused", "unused", "unused" };
            var mask = new float[3][];
            string rutsOn = null;
            for (int ci = 0; ci < maskOrder.Count; ci++)
            {
                slots[ci] = maskOrder[ci]; srcs[ci] = "TMask " + maskTex + "." + "RGB"[ci];
                mask[ci] = tmF[ci];
            }
            if (haveTm && Array.IndexOf(slots, RUTS_TEX) >= 0)
            {
                var enr = new double[3]; for (int i = 0; i < 3; i++) enr[i] = UxJson.Num(((JObj)testTmask[i]).Get("enrichment"));
                int ci = Array.IndexOf(slots, RUTS_TEX);
                int bestCh = 0; for (int i = 1; i < 3; i++) if (enr[i] > enr[bestCh]) bestCh = i;
                if (enr[ci] >= RUTS_MIN_ENRICH && ci == bestCh) rutsOn = "TMask channel " + "RGB"[ci] + " (enrichment " + UxImaging.PyFixed(enr[ci], 2) + ", highest)";
                else if (enr[bestCh] >= RUTS_MIN_ENRICH)
                {   // reorder: the ruts layer moves to the concentrated channel, the layer there takes the ruts' old channel
                    string t = slots[ci]; slots[ci] = slots[bestCh]; slots[bestCh] = t;
                    foreach (int i in new[] { ci, bestCh })
                    {
                        srcs[i] = slots[i] != null ? "TMask " + maskTex + "." + "RGB"[i] : "unused";
                        mask[i] = slots[i] != null ? tmF[i] : new float[TN];
                    }
                    mappingNote.Add("ruts layer moved from channel " + "RGB"[ci] + " to " + "RGB"[bestCh] + " (enrichment " + UxImaging.PyFixed(enr[ci], 2) + " vs " + UxImaging.PyFixed(enr[bestCh], 2) + ")");
                    rutsOn = "TMask channel " + "RGB"[bestCh] + " (reordered, enrichment " + UxImaging.PyFixed(enr[bestCh], 2) + ")";
                }
                else mappingNote.Add("no TMask channel concentrates on the lap (enrichments " + PyList(new[] { UxImaging.PyFixed(enr[0], 2), UxImaging.PyFixed(enr[1], 2), UxImaging.PyFixed(enr[2], 2) }) + ")");
            }
            tmF = null;
            for (int ch = 0; ch < 3; ch++) if (mask[ch] == null) mask[ch] = new float[TN];
            float[] Footprint(Predicate<int> pick)
            {
                var a = new float[TN]; for (int i = 0; i < TN; i++) if (pick(mat2[i])) a[i] = 1f;
                return UxImaging.GaussianFilter(a, TS, TS, 1.0);
            }
            if (rutsOn == null && rutsK >= 0)
            {   // synthesise the ruts channel from the ruts-material footprint (where Untamed draws the ruts texture)
                int ci;
                if (Array.IndexOf(slots, RUTS_TEX) >= 0) ci = Array.IndexOf(slots, RUTS_TEX);
                else if (Array.IndexOf(slots, null) >= 0) ci = Array.IndexOf(slots, null);
                else { ci = 2; mappingNote.Add("layer " + slots[2] + " dropped for the ruts layer"); }
                slots[ci] = RUTS_TEX;
                mask[ci] = Footprint(m => m == rutsK);
                srcs[ci] = "ruts-material footprint (" + src.mats[rutsK] + "), Gaussian 0.5 ft";
                rutsOn = "synthesised channel " + "RGB"[ci] + " from the " + src.mats[rutsK] + " footprint";
            }
            var avail = new JObj();
            for (int i = 0; i < 3; i++)
            {
                if (slots[i] == null) { Array.Clear(mask[i], 0, TN); continue; }
                var ks = new List<int>(); for (int k = 0; k < src.mats.Count; k++) if (src.MaterialTexturesLower(k).Contains(slots[i])) ks.Add(k);
                if (ks.Count < src.mats.Count)
                {
                    var set = new HashSet<int>(ks);
                    float[] a = Footprint(m => set.Contains(m));
                    float[] mc = mask[i];
                    for (int p = 0; p < TN; p++) mc[p] = (float)(mc[p] * a[p]);
                    var names = new List<object>(); foreach (int k in ks) names.Add(src.mats[k]);
                    avail.Set("RGB"[i].ToString(), names);
                }
            }
            var finalTest = new List<object>();
            for (int ch = 0; ch < 3; ch++) finalTest.Add(RutsChannel("RGB"[ch], null, mask[ch], foot2, nearLap, nFoot2, nNear));
            src.cancel.ThrowIfCancellationRequested();
            // ---- tiling textures (the Reflex near look)
            var tileSrc = new JObj(); var tileOrder = new List<string>(); var means = new Dictionary<string, float[]>(StringComparer.Ordinal);
            var tileCache = new Dictionary<string, (UxImage im, bool grey)>(StringComparer.Ordinal);
            (UxImage im, bool grey) TileOf(string n)
            {
                if (tileCache.TryGetValue(n, out var hit)) return hit;
                FImg img = TextureRgba(src, n) ?? throw new UxException("no texture " + n + " for a tiling layer");
                bool grey = IsGrey(img);
                var r = (ToTile(TileRgb(img, tint, grey), DETAIL_SIZE), grey);
                tileCache[n] = r;
                return r;
            }
            UxImage baseImg; bool baseGrey;
            if (baseName != null) { var t = TileOf(baseName); baseImg = t.im; baseGrey = t.grey; }
            else
            {
                var d = new byte[DETAIL_SIZE * DETAIL_SIZE * 3]; for (int i = 0; i < d.Length; i += 3) { d[i] = (byte)tint[0]; d[i + 1] = (byte)tint[1]; d[i + 2] = (byte)tint[2]; }
                baseImg = new UxImage(d, DETAIL_SIZE, DETAIL_SIZE, 3); baseGrey = true;
            }
            gr.tiles["base"] = baseImg; tileOrder.Add("base");
            tileSrc.Set("base", new JObj().Set("texture", baseName).Set("tinted", baseGrey).Set("mask", "mask black"));
            for (int ci = 0; ci < 3; ci++)
            {
                string key = "dirt_" + "rgb"[ci];
                if (slots[ci] != null)
                {
                    var t = TileOf(slots[ci]);
                    gr.tiles[key] = t.im; tileSrc.Set(key, new JObj().Set("texture", slots[ci]).Set("tinted", t.grey).Set("mask", srcs[ci]));
                }
                else { gr.tiles[key] = baseImg; tileSrc.Set(key, new JObj().Set("texture", baseName).Set("tinted", baseGrey).Set("mask", "unused (0)")); }
                tileOrder.Add(key);
            }
            foreach (string k in tileOrder) means[k] = TileMean(gr.tiles[k]);
            // ---- TopDown candidates. col is float32 until the float64 macro term multiplies it.
            string baseT = chosenSorted.Find(n => (string)((JObj)chosen.Get(n)).Get("role") == "base");
            string occT = chosenSorted.Find(n => (string)((JObj)chosen.Get(n)).Get("role") == "occ");
            string shT = !string.IsNullOrEmpty(ssm) && chosen.Has(Lower(ssm)) ? Lower(ssm) : null;
            double[] macroR = baseT != null ? SampleGrid(imgs[baseT], CoefOf(baseT), 0, TS, TD_RES, jobs) : null;
            JObj macroInfo = null; double med = 0;
            if (baseT != null)
            {
                double[] sel;
                if (nFoot2 > 0) { sel = new double[nFoot2]; int at = 0; for (int i = 0; i < TN; i++) if (foot2[i]) sel[at++] = macroR[i]; }
                else sel = (double[])macroR.Clone();
                med = UxImaging.MedianInPlace(sel);
                macroInfo = new JObj().Set("texture", baseT).Set("channel", "R").Set("median", UxNp.PyRound(med, 4)).Set("k", MACRO_K);
            }
            // sampled(n)[..., :3].mean(-1) of the occlusion and the shadow texture: sampled per pixel in the loop below
            FImg occCh = occT != null ? imgs[occT] : null, shCh = shT != null ? imgs[shT] : null;
            double[] occCoef = occT != null ? CoefOf(occT) : null, shCoef = shT != null ? CoefOf(shT) : null;
            refs = null;
            bool anyUnc = false; foreach (bool b in hfr.unclampable) anyUnc |= b;
            var pngBase = new byte[TN * 3]; var pngOcc = new byte[TN * 3]; var pngSh = new byte[TN * 3]; var pngMask = new byte[TN * 3];
            float[] mBase = means["base"]; float[][] mDirt = { means["dirt_r"], means["dirt_g"], means["dirt_b"] };
            double medDiv = Math.Max(med, 1e-3);
            Parallel.For(0, TS, new ParallelOptions { MaxDegreeOfParallelism = jobs }, r =>
            {
                for (int c = 0; c < TS; c++)
                {
                    int p = r * TS + c;
                    bool unc = anyUnc && hfr.unclampable[(r / 2) * HF_SIZE + c / 2];                 // np.kron(unclampable, ones((2, 2)))
                    double gx = X0 + (c + 0.5) * TD_RES, gz = Z1 - (r + 0.5) * TD_RES;
                    double occ = occCh != null ? SampleRgbMean(occCh, occCoef, gx, gz) : 1.0;
                    double sh = shCh != null ? SampleRgbMean(shCh, shCoef, gx, gz) : 1.0;
                    double macro = 0;
                    if (macroR != null) { macro = 1 + MACRO_K * (macroR[p] / medDiv - 1); macro = macro < 0.6 ? 0.6 : macro > 1.4 ? 1.4 : macro; }
                    for (int ch = 0; ch < 3; ch++)
                    {
                        float col = mBase[ch];
                        for (int ci = 0; ci < 3; ci++)
                        {
                            float a = mask[ci][p];
                            col = (float)((float)(col * (float)(1f - a)) + (float)(mDirt[ci][ch] * a));
                        }
                        pngMask[3 * p + ch] = U8(mask[ch][p]);
                        if (macroR != null)
                        {   // float64 from here on
                            double v = col * macro;
                            if (unc) v *= UNCLAMP_DARK;
                            double tb = v < 0 ? 0 : v > 1 ? 1 : v;
                            pngBase[3 * p + ch] = U8(tb);
                            double to = tb * occ; to = to < 0 ? 0 : to > 1 ? 1 : to;
                            pngOcc[3 * p + ch] = U8(to);
                            double ts = to * sh; ts = ts < 0 ? 0 : ts > 1 ? 1 : ts;
                            pngSh[3 * p + ch] = U8(ts);
                        }
                        else
                        {   // no base texture: float32 until a float64 factor (occ / shadow sample) comes in
                            float v = col;
                            if (unc) v = (float)(v * (float)UNCLAMP_DARK);
                            float tb = v < 0f ? 0f : v > 1f ? 1f : v;
                            pngBase[3 * p + ch] = U8(tb);
                            if (occCh != null)
                            {
                                double to = tb * occ; to = to < 0 ? 0 : to > 1 ? 1 : to;
                                pngOcc[3 * p + ch] = U8(to);
                                double ts = to * sh; ts = ts < 0 ? 0 : ts > 1 ? 1 : ts;
                                pngSh[3 * p + ch] = U8(ts);
                            }
                            else
                            {
                                float to = tb;                                                       // x float32 ones
                                pngOcc[3 * p + ch] = U8(to);
                                if (shCh != null) { double ts = to * sh; ts = ts < 0 ? 0 : ts > 1 ? 1 : ts; pngSh[3 * p + ch] = U8(ts); }
                                else pngSh[3 * p + ch] = U8(to);
                            }
                        }
                    }
                }
            });
            macroR = null;
            src.cancel.ThrowIfCancellationRequested();
            // the PNG files (encoded in parallel, recorded in Python's order)
            var jobsList = new List<(string name, byte[] rgb, int size)>
            {
                ("topdown_base.png", pngBase, TS), ("topdown_occ.png", pngOcc, TS), ("topdown_shadow.png", pngSh, TS), ("detailmask.png", pngMask, TS),
            };
            foreach (string k in tileOrder) jobsList.Add((k + ".png", gr.tiles[k].data, DETAIL_SIZE));
            var pngs = new byte[jobsList.Count][];
            Parallel.For(0, jobsList.Count, new ParallelOptions { MaxDegreeOfParallelism = jobs }, i => pngs[i] = UxImaging.PngRgb(jobsList[i].rgb, jobsList[i].size, jobsList[i].size));
            for (int i = 0; i < jobsList.Count; i++)
            {
                outdir.WriteBytes(jobsList[i].name, pngs[i]);
                gr.files.Set(jobsList[i].name, UxBin.Md5(pngs[i]));
                writePng?.Invoke(jobsList[i].name);
            }
            log("  ground: tint (" + tint[0] + ", " + tint[1] + ", " + tint[2] + "); layers " + PyList(slots) + "; ruts " + rutsOn);
            var perMat = new JObj();
            for (int k = 0; k < fits.Count; k++)
            {
                string ch = null;
                foreach (string nm in new[] { "uv0", "uv1" }) if (fits[k].TryGetValue(nm, out Fit f) && f.planar) { ch = nm; break; }
                var d = new JObj().Set("chosen", ch);
                foreach (string nm in new[] { "uv0", "uv1" })
                    if (fits[k].TryGetValue(nm, out Fit f))
                        d.Set(nm, new JObj().Set("planar", f.planar).Set("residual", UxNp.PyRound(f.residual, 6))
                            .Set("span_per_1024ft", L(UxNp.PyRound(f.span[0], 4), UxNp.PyRound(f.span[1], 4))).Set("coef", CoefJson(f.coef)));
                perMat.Set(src.MatKey(k), d);
            }
            var uvMaps = new List<object>();
            foreach (var mp in maps) uvMaps.Add(new JObj().Set("id", mp.id).Set("coef", CoefJson(mp.coef)).Set("sources", new List<object>(mp.sources)));
            var layerMeans = new JObj();
            foreach (string k in tileOrder) layerMeans.Set(k, L(UxNp.PyRound((double)means[k][0], 4), UxNp.PyRound((double)means[k][1], 4), UxNp.PyRound((double)means[k][2], 4)));
            gr.json = new JObj()
                .Set("default_topdown", "topdown_occ.png")
                .Set("candidates", new JObj().Set("topdown_base.png", "layer far-field colour x TBase.R macro")
                    .Set("topdown_occ.png", "base x " + (occT ?? "no occlusion texture") + " (default)")
                    .Set("topdown_shadow.png", "occ x " + (shT ?? "no static shadow map (Editor_Default_White)")))
                .Set("tint", L((long)tint[0], (long)tint[1], (long)tint[2])).Set("tint_source", tintSrc).Set("texture_roles", roles)
                .Set("uv_maps", uvMaps).Set("uv_sets_per_material", perMat).Set("texture_maps", chosen)
                .Set("tmask", new JObj().Set("texture", maskTex).Set("layers_after_mask", new List<object>(maskOrder)).Set("reference_material", src.mats[refK])
                    .Set("channel_layers", new JObj().Set("R", slots[0]).Set("G", slots[1]).Set("B", slots[2]))
                    .Set("channel_source", new JObj().Set("R", srcs[0]).Set("G", srcs[1]).Set("B", srcs[2]))
                    .Set("notes", mappingNote).Set("ruts", rutsOn).Set("restricted_to_materials", avail))
                .Set("ruts_test", new JObj().Set("lap_points", (long)(pts.Length / 3)).Set("lap_closed", closed).Set("lap_G5_ft", UxNp.PyRound(g5, 1))
                    .Set("near_ft", RUTS_NEAR_FT).Set("tmask_channels", testTmask).Set("final_mask", finalTest)
                    .Set("ruts_material", rutsK >= 0 ? src.mats[rutsK] : null))
                .Set("tiling", tileSrc).Set("layer_means", layerMeans)
                .Set("macro", macroInfo).Set("occlusion", occT).Set("shadow", shT)
                .Set("footprint_px_2048", nFoot2);
            return gr;
        }

        // ===================================================================================================== parts
        /// split_long_edges: triangles (n, 3, 3) subdivided until no edge is longer than max_edge (midpoint 4-split, winding kept).
        static double[] SplitLongEdges(double[] T, double maxEdge)
        {
            var done = new List<double>(T.Length); double[] todo = T;
            while (todo.Length > 0)
            {
                var next = new List<double>(); var blocks = new List<double>[4]; for (int i = 0; i < 4; i++) blocks[i] = new List<double>();
                for (int t = 0; t < todo.Length; t += 9)
                {
                    double e0 = Len(todo, t + 3, t), e1 = Len(todo, t + 6, t + 3), e2 = Len(todo, t, t + 6);
                    double e = Math.Max(Math.Max(e0, e1), e2);
                    if (e <= maxEdge) { for (int k = 0; k < 9; k++) done.Add(todo[t + k]); continue; }
                    var a = new double[3]; var b = new double[3]; var c = new double[3]; var ab = new double[3]; var bc = new double[3]; var ca = new double[3];
                    for (int k = 0; k < 3; k++) { a[k] = todo[t + k]; b[k] = todo[t + 3 + k]; c[k] = todo[t + 6 + k]; ab[k] = (a[k] + b[k]) / 2; bc[k] = (b[k] + c[k]) / 2; ca[k] = (c[k] + a[k]) / 2; }
                    blocks[0].AddRange(a); blocks[0].AddRange(ab); blocks[0].AddRange(ca);
                    blocks[1].AddRange(ab); blocks[1].AddRange(b); blocks[1].AddRange(bc);
                    blocks[2].AddRange(ca); blocks[2].AddRange(bc); blocks[2].AddRange(c);
                    blocks[3].AddRange(ab); blocks[3].AddRange(bc); blocks[3].AddRange(ca);
                }
                foreach (var bl in blocks) next.AddRange(bl);
                todo = next.ToArray();
            }
            return done.ToArray();
        }
        static double Len(double[] T, int p, int q)
        {
            double x = T[p] - T[q], y = T[p + 1] - T[q + 1], z = T[p + 2] - T[q + 2];
            return Math.Sqrt(x * x + y * y + z * z);
        }
        /// cluster_mesh: kind-22 clusters (edges over 64 ft split, then the median split: <= 48 triangles, <= 32 ft half size).
        static List<UxRmesh.Cluster> ClusterMesh(double[] tris, out int nTris)
        {
            double[] T = SplitLongEdges(tris, KIND22_SPLIT_EDGE);
            var keep = new List<double>(T.Length);
            for (int t = 0; t < T.Length; t += 9)
            {
                double ux = T[t + 3] - T[t], uy = T[t + 4] - T[t + 1], uz = T[t + 5] - T[t + 2], vx = T[t + 6] - T[t], vy = T[t + 7] - T[t + 1], vz = T[t + 8] - T[t + 2];
                double x = uy * vz - uz * vy, y = uz * vx - ux * vz, z = ux * vy - uy * vx;
                if (Math.Sqrt(x * x + y * y + z * z) > 1e-9) for (int k = 0; k < 9; k++) keep.Add(T[t + k]);
            }
            double[] K = keep.ToArray(); nTris = K.Length / 9;
            return UxRmesh.ClusterTriangles(K, KIND22_MAX_TRIS, KIND22_MAX_HALF);
        }

        sealed class GroupPart { public int[] used; public int[] loc; public int[] strip; }
        /// split_group: a triangle list (indices into one material's vertex arrays) partitioned so that every part stays inside the
        /// group limits (vertices, strip indices).
        static List<GroupPart> SplitGroup(int[] trisSrc, double[] pos)
        {
            var o = new List<GroupPart>(); var stack = new List<int[]> { trisSrc };
            while (stack.Count > 0)
            {
                int[] t = stack[stack.Count - 1]; stack.RemoveAt(stack.Count - 1);
                int[] used = UniqueVerts(t);
                var index = new Dictionary<int, int>(used.Length); for (int i = 0; i < used.Length; i++) index[used[i]] = i;
                var loc = new int[t.Length]; for (int i = 0; i < t.Length; i++) loc[i] = index[t[i]];
                if (used.Length <= UxCommon.Limits.MaxGroupVerts)
                {
                    int[] strip = UxRmesh.Stripify(loc);
                    if (strip.Length <= UxCommon.Limits.MaxGroupStripIndices)
                    {
                        int[] back = UxRmesh.Canon(UxRmesh.StripToTris(strip)), want = UxRmesh.Canon(loc);
                        bool same = back.Length == want.Length; for (int i = 0; same && i < back.Length; i++) same = back[i] == want[i];
                        UxBin.Require(same, "stripify changed tris");
                        o.Add(new GroupPart { used = used, loc = loc, strip = strip });
                        continue;
                    }
                }
                int n = t.Length / 3; var cen = new double[3 * n];
                double[] lo = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, hi = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                for (int i = 0; i < n; i++)
                    for (int k = 0; k < 3; k++)
                    {
                        double v = ((pos[3 * t[3 * i] + k] + pos[3 * t[3 * i + 1] + k]) + pos[3 * t[3 * i + 2] + k]) / 3;
                        cen[3 * i + k] = v; if (v < lo[k]) lo[k] = v; if (v > hi[k]) hi[k] = v;
                    }
                int ax = 0; for (int k = 1; k < 3; k++) if (hi[k] - lo[k] > hi[ax] - lo[ax]) ax = k;
                var ord = new int[n]; for (int i = 0; i < n; i++) ord[i] = i;
                Array.Sort(ord, (x, y) => { int c = cen[3 * x + ax].CompareTo(cen[3 * y + ax]); return c != 0 ? c : x.CompareTo(y); });
                int half = n / 2;
                var upper = new int[3 * (n - half)]; var lower = new int[3 * half];
                for (int i = 0; i < half; i++) for (int k = 0; k < 3; k++) lower[3 * i + k] = t[3 * ord[i] + k];
                for (int i = half; i < n; i++) for (int k = 0; k < 3; k++) upper[3 * (i - half) + k] = t[3 * ord[i] + k];
                stack.Add(upper); stack.Add(lower);
            }
            return o;
        }
        static double NanToNum(double v) => double.IsNaN(v) ? 0.0 : double.IsPositiveInfinity(v) ? double.MaxValue : double.IsNegativeInfinity(v) ? -double.MaxValue : v;

        /// build_parts: the excluded trncol triangles as 128 ft cell parts (surface + model + collis). Returns the parts list and the
        /// summary counts.
        static List<object> BuildParts(Source src, Hf hfr, UxWork outdir, JObj matRequests, Action<string> log, out JObj pstats)
        {
            double[] P = src.P; int[] I = src.I; int[] rt = src.rtri; var T = src.T;
            var exIds = new List<int>(); for (int i = 0; i < src.nt; i++) if (hfr.excl[i]) exIds.Add(i);
            var parts = new List<object>();
            if (exIds.Count == 0) { pstats = new JObj().Set("render_tris", 0L).Set("collision_only", 0L).Set("merged_collision_only", 0L); return parts; }
            int ne = exIds.Count; var cenX = new double[ne]; var cenZ = new double[ne]; var ix = new long[ne]; var iz = new long[ne]; var hasR = new bool[ne];
            var cellSet = new SortedSet<(long a, long b)>();
            long nHasR = 0;
            for (int j = 0; j < ne; j++)
            {
                int t = exIds[j]; int a = 3 * I[3 * t], b = 3 * I[3 * t + 1], c = 3 * I[3 * t + 2];
                cenX[j] = ((P[a] + P[b]) + P[c]) / 3; cenZ[j] = ((P[a + 2] + P[b + 2]) + P[c + 2]) / 3;
                ix[j] = (long)Math.Floor((cenX[j] - X0) / CELL_FT); iz[j] = (long)Math.Floor((Z1 - cenZ[j]) / CELL_FT);
                hasR[j] = rt[t] >= 0;
                if (hasR[j]) { cellSet.Add((ix[j], iz[j])); nHasR++; }
            }
            var cells = new List<(long a, long b)>(cellSet);
            if (cells.Count == 0) throw new UxException("excluded triangles without any render triangle");
            var cellOf = new Dictionary<(long, long), List<int>>();
            for (int j = 0; j < ne; j++)
            {
                var key = (ix[j], iz[j]);
                if (!hasR[j] && !cellSet.Contains(key))
                {   // a collision-only triangle goes into the nearest cell that has render triangles (np.argmin of np.hypot)
                    int best = 0; double bd = double.PositiveInfinity;
                    for (int k = 0; k < cells.Count; k++)
                    {
                        double dx = X0 + CELL_FT * (cells[k].a + 0.5) - cenX[j], dz = Z1 - CELL_FT * (cells[k].b + 0.5) - cenZ[j];
                        double d = Math.Sqrt(dx * dx + dz * dz);
                        if (d < bd) { bd = d; best = k; }
                    }
                    key = cells[best];
                }
                if (!cellOf.TryGetValue(key, out var l)) cellOf[key] = l = new List<int>();
                l.Add(exIds[j]);
            }
            long mergedCo = 0; foreach (var kv in cellOf) foreach (int t in kv.Value) if (rt[t] < 0) mergedCo++;
            long totalClusters = 0;
            foreach (var (ca, cb) in cells)
            {
                src.cancel.ThrowIfCancellationRequested();
                var ids = new List<int>(cellOf[(ca, cb)]); ids.Sort();
                string name = "UXT_" + src.lv.shortName + "_" + ca.ToString("00", System.Globalization.CultureInfo.InvariantCulture) + "_" + cb.ToString("00", System.Globalization.CultureInfo.InvariantCulture);
                var W = new double[9 * ids.Count]; double ylo = double.PositiveInfinity, yhi = double.NegativeInfinity;
                for (int i = 0; i < ids.Count; i++)
                    for (int v = 0; v < 3; v++)
                    {
                        int q = 3 * I[3 * ids[i] + v];
                        W[9 * i + 3 * v] = P[q]; W[9 * i + 3 * v + 1] = P[q + 1]; W[9 * i + 3 * v + 2] = P[q + 2];
                        if (P[q + 1] < ylo) ylo = P[q + 1]; if (P[q + 1] > yhi) yhi = P[q + 1];
                    }
                double[] origin = { X0 + CELL_FT * (ca + 0.5), UxNp.Round((ylo + yhi) / 2), Z1 - CELL_FT * (cb + 0.5) };
                // render groups per material
                var rids = new List<int>(); foreach (int t in ids) if (rt[t] >= 0) rids.Add(rt[t]);
                var matSet = new SortedSet<int>(); foreach (int r in rids) matSet.Add(src.rM[r]);
                var groups = new List<UxSurfaceGroup>(); var gmats = new List<string>(); var ginfo = new List<object>(); var rp = new List<double>();
                foreach (int k in matSet)
                {
                    var m = T.materials[k];
                    var lt = new List<int>(); foreach (int r in rids) if (src.rM[r] == k) lt.Add(r - src.mat_first[k]);
                    lt.Sort();
                    var tl = new int[3 * lt.Count]; for (int i = 0; i < lt.Count; i++) { tl[3 * i] = m.tris[3 * lt[i]]; tl[3 * i + 1] = m.tris[3 * lt[i] + 1]; tl[3 * i + 2] = m.tris[3 * lt[i] + 2]; }
                    string mname = "UXT_" + src.mats[k];
                    var req = UxJson.Obj(matRequests.Get(mname) ?? throw new UxException("no material request " + mname));
                    foreach (var gp in SplitGroup(tl, m.pos))
                    {
                        int nv = gp.used.Length; var vb = new byte[nv * UxRmesh.STRIDE];
                        // part_uvs
                        bool dirt = (string)req.Get("role") == "dirt", srcUv0 = (string)req.Get("uv0") == "source_uv0";
                        var uv0 = new double[2 * nv]; var uv1 = new double[2 * nv];
                        for (int i = 0; i < nv; i++)
                        {
                            int v = gp.used[i]; double px = m.pos[3 * v], pz = m.pos[3 * v + 2];
                            if (dirt)
                            {
                                if (srcUv0) { uv0[2 * i] = m.uv0[2 * v]; uv0[2 * i + 1] = m.uv0[2 * v + 1]; }
                                else { uv0[2 * i] = px / 16.0; uv0[2 * i + 1] = -pz / 16.0; }
                                uv1[2 * i] = (px - X0) / (HF_SIZE * HF_RES); uv1[2 * i + 1] = (Z1 - pz) / (HF_SIZE * HF_RES);
                            }
                            else { uv0[2 * i] = m.uv0[2 * v]; uv0[2 * i + 1] = m.uv0[2 * v + 1]; uv1[2 * i] = m.uv1[2 * v]; uv1[2 * i + 1] = m.uv1[2 * v + 1]; }
                            for (int c = 0; c < 2; c++) { uv0[2 * i + c] = NanToNum(uv0[2 * i + c]); uv1[2 * i + c] = NanToNum(uv1[2 * i + c]); }
                        }
                        double[] shift = { 0, 0 };
                        if (nv > 0)
                        {
                            double m0 = double.PositiveInfinity, m1 = double.PositiveInfinity;
                            for (int i = 0; i < nv; i++) { if (uv0[2 * i] < m0) m0 = uv0[2 * i]; if (uv0[2 * i + 1] < m1) m1 = uv0[2 * i + 1]; }
                            shift[0] = Math.Floor(m0); shift[1] = Math.Floor(m1);
                        }
                        for (int i = 0; i < nv; i++)
                        {
                            int v = gp.used[i], o = i * UxRmesh.STRIDE;
                            for (int c = 0; c < 3; c++)
                            {
                                ushort hbits = UxNp.DoubleToHalf(m.pos[3 * v + c] - origin[c]);
                                UxBin.PutU16(vb, o + 2 * c, hbits);
                                rp.Add(UxNp.HalfToFloat(hbits));
                            }
                            UxBin.PutU16(vb, o + 6, 0x3C00);
                            double nx = m.normal[3 * v], nyv = m.normal[3 * v + 1], nz = m.normal[3 * v + 2];
                            double ln = Math.Sqrt(nx * nx + nyv * nyv + nz * nz);
                            if (ln > 1e-6) { double dv = Math.Max(ln, 1e-12); nx /= dv; nyv /= dv; nz /= dv; } else { nx = 0; nyv = 1; nz = 0; }
                            UxBin.PutF32(vb, o + 8, (float)nx); UxBin.PutF32(vb, o + 12, (float)nyv); UxBin.PutF32(vb, o + 16, (float)nz);
                            UxBin.PutU16(vb, o + 20, UxNp.DoubleToHalf(uv0[2 * i] - shift[0])); UxBin.PutU16(vb, o + 22, UxNp.DoubleToHalf(uv0[2 * i + 1] - shift[1]));
                            UxBin.PutU16(vb, o + 24, UxNp.DoubleToHalf(uv1[2 * i])); UxBin.PutU16(vb, o + 26, UxNp.DoubleToHalf(uv1[2 * i + 1]));
                            UxBin.PutU32(vb, o + 28, m.color[v]);
                            for (int c = 0; c < 3; c++) UxBin.PutF32(vb, o + 32 + 4 * c, (float)NanToNum(m.tangent[3 * v + c]));
                        }
                        groups.Add(new UxSurfaceGroup(gp.strip, vb)); gmats.Add(mname);
                        ginfo.Add(new JObj().Set("material", mname).Set("source_material", src.mats[k]).Set("verts", (long)nv).Set("strip_indices", (long)gp.strip.Length)
                            .Set("tris", (long)(gp.loc.Length / 3)).Set("uv0_shift", L(shift[0], shift[1])));
                    }
                }
                UxBin.Require(groups.Count <= UxCommon.Limits.MaxGroupsPerModel, name + ": " + groups.Count + " groups");
                byte[] surf = UxRmesh.WriteSurface(groups);
                // collision (all triangles of the cell, incl. merged collision-only ones), local frame
                var Wl = new double[W.Length]; for (int i = 0; i < W.Length; i++) Wl[i] = W[i] - origin[i % 3];
                var clusters = ClusterMesh(Wl, out int nSplit);
                byte[] col = UxRmesh.WriteCollis(clusters);
                // bounds: union of render and collision (local)
                var allp = new double[rp.Count + Wl.Length]; rp.CopyTo(allp); Array.Copy(Wl, 0, allp, rp.Count, Wl.Length);
                var (radius, bc, be) = UxRmesh.Bounds(allp);
                var model = new UxPcModel(name, gmats, (float)radius, new[] { (float)bc[0], (float)bc[1], (float)bc[2] }, new[] { (float)be[0], (float)be[1], (float)be[2] }, null, 0);
                byte[] mdl = UxRmesh.WriteModel(model);
                var files = new JObj();
                foreach (var (ext, data) in new[] { ("surface", surf), ("model", mdl), ("collis", col) })
                {
                    string rel = "parts/" + name + "." + ext;
                    outdir.WriteBytes(rel, data);
                    files.Set(ext, new JObj().Set("file", rel).Set("md5", UxBin.Md5(data)).Set("size", (long)data.Length));
                }
                long nCo = 0; foreach (int t in ids) if (rt[t] < 0) nCo++;
                var srcMats = new SortedSet<string>(StringComparer.Ordinal); foreach (int k in matSet) srcMats.Add(src.mats[k]);
                if (nCo > 0) srcMats.Add(COLLISION_ONLY);
                totalClusters += clusters.Count;
                parts.Add(new JObj().Set("name", name).Set("cell", L(ca, cb)).Set("type", "CollisionRSData").Set("role", "ridable").Set("drive_on", 1L)
                    .Set("physics_type", "MXBike").Set("physics_type_value", 0L).Set("model", name).Set("surface", name).Set("collis", name)
                    .Set("material_list", new List<object>(gmats)).Set("origin", L(origin[0], origin[1], origin[2]))
                    .Set("matrix", L(1.0, 0L, 0L, 0L, 0L, 1.0, 0L, 0L, 0L, 0L, 1.0, 0L, origin[0], origin[1], origin[2], 1.0))
                    .Set("trncol_tris", (long)ids.Count).Set("render_tris", (long)rids.Count).Set("collision_only_tris", nCo)
                    .Set("clusters", (long)clusters.Count).Set("collision_tris", (long)nSplit)
                    .Set("bounds", new JObj().Set("center", L(bc[0], bc[1], bc[2])).Set("extents", L(be[0], be[1], be[2])).Set("radius", radius))
                    .Set("groups", ginfo).Set("files", files).Set("draw_distance", 0L)
                    .Set("source_materials", new List<object>(srcMats)));
            }
            log("  parts: " + parts.Count + " cells, " + ne + " trncol triangles (" + mergedCo + " collision-only merged), " + totalClusters + " clusters");
            pstats = new JObj().Set("render_tris", nHasR).Set("collision_only", ne - nHasR).Set("merged_collision_only", mergedCo);
            return parts;
        }

        // ===================================================================================================== checks
        static void Centroids(double[] T, out double[] c)
        {
            int n = T.Length / 9; c = new double[3 * n];
            for (int i = 0; i < n; i++) for (int k = 0; k < 3; k++) c[3 * i + k] = ((T[9 * i + k] + T[9 * i + 3 + k]) + T[9 * i + 6 + k]) / 3;
        }
        /// match(A, B): for every triangle of A its best rotation-invariant max-vertex error against the 8 triangles of B whose
        /// centroids are nearest; (err, index into B).
        static void Match(double[] A, double[] B, out double[] err, out int[] idx)
        {
            int na = A.Length / 9, nb = B.Length / 9;
            var e = new double[na]; var bj = new int[na];
            if (na == 0 || nb == 0) { for (int i = 0; i < na; i++) e[i] = double.PositiveInfinity; err = e; idx = bj; return; }
            Centroids(B, out double[] cb); Centroids(A, out double[] ca);
            var grid = new UxImaging.PointGrid(cb);
            int k = Math.Min(8, nb);
            Parallel.For(0, na, () => (new int[k], new double[k]), (i, _, buf) =>
            {
                int found = grid.Query(ca[3 * i], ca[3 * i + 1], ca[3 * i + 2], k, buf.Item1, buf.Item2);
                double best = double.PositiveInfinity; int bi = 0;
                for (int c = 0; c < found; c++)
                {
                    int j = buf.Item1[c]; double em = double.PositiveInfinity;
                    for (int s = 0; s < 3; s++)
                    {   // np.roll(Bc, s, axis=1): vertex v of the rolled triangle is vertex (v - s) mod 3
                        double mx = 0;
                        for (int v = 0; v < 3; v++)
                        {
                            int bv = (v - s + 3) % 3;
                            for (int d = 0; d < 3; d++) { double df = Math.Abs(B[9 * j + 3 * bv + d] - A[9 * i + 3 * v + d]); if (df > mx || double.IsNaN(df)) mx = df; }
                        }
                        if (mx < em) em = mx;
                    }
                    if (em < best) { best = em; bi = j; }
                }
                e[i] = best; bj[i] = bi;
                return buf;
            }, _ => { });
            err = e; idx = bj;
        }

        /// check_parts: every part read back with the Reflex mesh readers and compared with the source triangles (Untamed frame).
        static JObj CheckParts(Source src, Hf hfr, List<object> parts, UxWork outdir)
        {
            double[] P = src.P; int[] I = src.I; int[] rt = src.rtri;
            var res = new JObj().Set("parts", (long)parts.Count); var problems = new List<object>(); res.Set("problems", problems);
            if (parts.Count == 0) { res.Set("collis_match", true).Set("render_match", true).Set("missing_collision", 0L).Set("missing_render", 0L); return res; }
            var exIds = new List<int>(); for (int i = 0; i < src.nt; i++) if (hfr.excl[i]) exIds.Add(i);
            var CT = new List<double>(); var RT = new List<double>(); long groupsBad = 0, nclusters = 0;
            foreach (object po in parts)
            {
                var p = (JObj)po; string pname = (string)p.Get("name");
                var ol = (List<object>)p.Get("origin"); double[] o = { (double)ol[0], (double)ol[1], (double)ol[2] };
                var files = (JObj)p.Get("files");
                var cl = UxRmesh.ReadCollis(outdir.ReadBytes((string)((JObj)files.Get("collis")).Get("file")), out uint kind);
                UxBin.Require(kind == 22, pname + ": collis kind " + kind);
                nclusters += cl.Count;
                int ctStart = CT.Count;
                foreach (var c in cl) for (int i = 0; i < c.t.Length; i++) CT.Add((double)(float)(c.c[i % 3] + c.t[i]) + o[i % 3]);
                var gs = UxRmesh.ReadSurface(outdir.ReadBytes((string)((JObj)files.Get("surface")).Get("file")));
                var m = UxRmesh.ReadModel(outdir.ReadBytes((string)((JObj)files.Get("model")).Get("file")));
                var ml = (List<object>)p.Get("material_list");
                bool sameMats = m.materials.Count == ml.Count; for (int i = 0; sameMats && i < ml.Count; i++) sameMats = m.materials[i] == (string)ml[i];
                if (m.materials.Count != gs.Count || !sameMats || m.draw_distance != 0) problems.Add(pname + ": model/surface mismatch");
                int rtLast = RT.Count;
                foreach (var g in gs)
                {
                    if (g.VertexCount > UxCommon.Limits.MaxGroupVerts || g.strip.Length > UxCommon.Limits.MaxGroupStripIndices) groupsBad++;
                    int[] t = UxRmesh.StripToTris(g.strip);
                    rtLast = RT.Count;
                    foreach (int vi in t) for (int c = 0; c < 3; c++) RT.Add((double)UxNp.HalfToFloat(UxBin.U16LE(g.verts, vi * UxRmesh.STRIDE + 2 * c)) + o[c]);
                }
                if (gs.Count > UxCommon.Limits.MaxGroupsPerModel) problems.Add(pname + ": " + gs.Count + " groups");
                // bounds cover render + collision (the part's collision triangles and its last group's render triangles, as Python)
                var bnd = (JObj)p.Get("bounds"); var bcl = (List<object>)bnd.Get("center"); var bel = (List<object>)bnd.Get("extents");
                bool outsideB = false;
                void Test(List<double> a, int from) { for (int i = from; i < a.Count && !outsideB; i++) { int c = i % 3; if (Math.Abs((a[i] - o[c]) - (double)bcl[c]) > (double)bel[c] + 1e-3) outsideB = true; } }
                Test(CT, ctStart); Test(RT, rtLast);
                if (outsideB) problems.Add(pname + ": bounds do not cover the geometry");
            }
            double[] CTa = CT.ToArray(), RTa = RT.ToArray();
            int ne = exIds.Count; var srcC = new double[9 * ne];
            for (int i = 0; i < ne; i++) for (int v = 0; v < 3; v++) for (int c = 0; c < 3; c++) srcC[9 * i + 3 * v + c] = P[3 * I[3 * exIds[i] + v] + c];
            var degen = new bool[ne]; long nDegen = 0;
            for (int i = 0; i < ne; i++)
            {
                int t = 9 * i;
                double ux = srcC[t + 3] - srcC[t], uy = srcC[t + 4] - srcC[t + 1], uz = srcC[t + 5] - srcC[t + 2], vx = srcC[t + 6] - srcC[t], vy = srcC[t + 7] - srcC[t + 1], vz = srcC[t + 8] - srcC[t + 2];
                double x = uy * vz - uz * vy, y = uz * vx - ux * vz, z = ux * vy - uy * vx;
                degen[i] = Math.Sqrt(x * x + y * y + z * z) <= 1e-9; if (degen[i]) nDegen++;
            }
            long missC = 0, strayC = 0;
            bool splitNeeded = CTa.Length / 9 != ne - nDegen;
            if (splitNeeded)
            {
                var grid = new UxImaging.PointGrid(srcC);
                int nct = CTa.Length / 9; var far = new bool[nct];
                Parallel.For(0, nct, () => (new int[1], new double[1]), (i, _, buf) =>
                {
                    double mx = 0;
                    for (int v = 0; v < 3; v++) { grid.Query(CTa[9 * i + 3 * v], CTa[9 * i + 3 * v + 1], CTa[9 * i + 3 * v + 2], 1, buf.Item1, buf.Item2); double d = Math.Sqrt(buf.Item2[0]); if (d > mx) mx = d; }
                    far[i] = mx > 2e-3;
                    return buf;
                }, _ => { });
                strayC = Count(far);
                res.Set("collision_note", "long-edge split: " + nct + " collision triangles for " + ne + " source triangles");
            }
            else
            {
                var nd = new double[9 * (ne - nDegen)]; int at = 0;
                for (int i = 0; i < ne; i++) if (!degen[i]) { Array.Copy(srcC, 9 * i, nd, at, 9); at += 9; }
                Match(nd, CTa, out double[] e1, out _); Match(CTa, srcC, out double[] e2, out _);
                foreach (double e in e1) if (e > 2e-3) missC++;
                foreach (double e in e2) if (e > 2e-3) strayC++;
            }
            // render: every excluded triangle with a render twin is drawn once, same winding, positions within the FLOAT16 error
            var srcRl = new List<double>();
            for (int i = 0; i < ne; i++) if (rt[exIds[i]] >= 0) for (int k = 0; k < 9; k++) srcRl.Add(srcC[9 * i + k]);
            double[] srcR = srcRl.ToArray(); int nr = srcR.Length / 9;
            Match(srcR, RTa, out double[] err, out int[] jj);
            long flips = 0, sliver = 0, sliverFlips = 0; double maxErr = 0;
            for (int i = 0; i < nr; i++)
            {
                int j = 9 * jj[i], t = 9 * i;
                double ax = 0, ay = 0, az = 0;
                if (RTa.Length > 0)
                {
                    double ux = RTa[j + 3] - RTa[j], uy = RTa[j + 4] - RTa[j + 1], uz = RTa[j + 5] - RTa[j + 2], vx = RTa[j + 6] - RTa[j], vy = RTa[j + 7] - RTa[j + 1], vz = RTa[j + 8] - RTa[j + 2];
                    ax = uy * vz - uz * vy; ay = uz * vx - ux * vz; az = ux * vy - uy * vx;
                }
                double sx, sy, sz;
                {
                    double ux = srcR[t + 3] - srcR[t], uy = srcR[t + 4] - srcR[t + 1], uz = srcR[t + 5] - srcR[t + 2], vx = srcR[t + 6] - srcR[t], vy = srcR[t + 7] - srcR[t + 1], vz = srcR[t + 8] - srcR[t + 2];
                    sx = uy * vz - uz * vy; sy = uz * vx - ux * vz; sz = ux * vy - uy * vx;
                }
                double longest = Math.Max(Math.Max(Len(srcR, t + 3, t), Len(srcR, t + 6, t + 3)), Len(srcR, t, t + 6));
                bool big = Math.Sqrt(sx * sx + sy * sy + sz * sz) / Math.Max(longest, 1e-12) >= SLIVER_WIDTH_FT;
                bool flipped = EinsumDot(ax, ay, az, sx, sy, sz) <= 0;
                if (flipped && big) flips++;
                if (!big) { sliver++; if (flipped) sliverFlips++; }
                if (i == 0 || err[i] > maxErr) maxErr = err[i];
            }
            int nRT = RTa.Length / 9;
            res.Set("sliver_tris", sliver).Set("sliver_flips", sliverFlips)
                .Set("collision_tris", (long)(CTa.Length / 9)).Set("source_tris", (long)ne).Set("degenerate_source_tris", nDegen)
                .Set("collis_missing", missC).Set("collis_stray", strayC).Set("clusters", nclusters)
                .Set("render_tris", (long)nRT).Set("render_source_tris", (long)nr).Set("render_count_equal", nRT == nr)
                .Set("render_max_err_ft", nr > 0 ? maxErr : 0.0).Set("render_winding_flips", flips).Set("groups_over_limit", groupsBad);
            res.Set("ok", problems.Count == 0 && missC == 0 && strayC == 0 && nRT == nr && (nr == 0 || maxErr <= 1.0 / 16) && flips == 0 && groupsBad == 0);
            return res;
        }

        /// check_coverage: every trncol triangle is in the HF (kept) or in the excluded set (written as parts).
        static JObj CheckCoverage(Source src, Hf hfr)
        {
            double[] P = src.P; int[] I = src.I; int S = HF_SIZE;
            var h = new double[hfr.h.Length]; for (int i = 0; i < h.Length; i++) h[i] = hfr.h[i];
            long nUp = 0, over = 0, nSteep = 0, steepBad = 0, nEx = 0, nKept = 0; double emax = 0; bool anyUp = false;
            for (int t = 0; t < src.nt; t++)
            {
                if (hfr.excl[t]) { nEx++; continue; }
                nKept++;
                int a = 3 * I[3 * t], b = 3 * I[3 * t + 1], c = 3 * I[3 * t + 2];
                double cx = ((P[a] + P[b]) + P[c]) / 3, cy = ((P[a + 1] + P[b + 1]) + P[c + 1]) / 3, cz = ((P[a + 2] + P[b + 2]) + P[c + 2]) / 3;
                bool upf = src.ny[t] > UP_NY;
                if (upf && src.ar[t] > 0)
                {
                    double e = Math.Abs(Bilinear(h, S, X0, Z1, HF_RES, cx, cz) - src.lift - cy);
                    nUp++; if (e > 0.6) over++; if (!anyUp || e > emax) emax = e; anyUp = true;
                }
                if (!upf)
                {   // steep kept: the triangle's own y range against the HF range of its centroid's 2 x 2 bilinear support (+-0.6 ft)
                    nSteep++;
                    double cc = (cx - X0) / HF_RES - 0.5, rr = (Z1 - cz) / HF_RES - 0.5;
                    double fc0 = Math.Floor(cc), fr0 = Math.Floor(rr);
                    int c0 = fc0 < 0 ? 0 : fc0 > S - 2 ? S - 2 : (int)fc0, r0 = fr0 < 0 ? 0 : fr0 > S - 2 ? S - 2 : (int)fr0;
                    double q0 = h[r0 * S + c0] - src.lift, q1 = h[r0 * S + c0 + 1] - src.lift, q2 = h[(r0 + 1) * S + c0] - src.lift, q3 = h[(r0 + 1) * S + c0 + 1] - src.lift;
                    double qmin = Math.Min(Math.Min(q0, q1), Math.Min(q2, q3)), qmax = Math.Max(Math.Max(q0, q1), Math.Max(q2, q3));
                    double ymin = Math.Min(Math.Min(P[a + 1], P[b + 1]), P[c + 1]), ymax = Math.Max(Math.Max(P[a + 1], P[b + 1]), P[c + 1]);
                    if (!(qmin <= ymax + 0.6 && qmax >= ymin - 0.6)) steepBad++;
                }
            }
            return new JObj().Set("kept_up", nUp).Set("kept_up_centroid_over_0_6ft", over).Set("kept_up_centroid_max_err", anyUp ? emax : 0.0)
                .Set("kept_steep", nSteep).Set("kept_steep_outside_hf_range", steepBad).Set("excluded", nEx).Set("total", (long)src.nt)
                .Set("accounted", nKept + nEx == src.nt);
        }

        static JObj CheckOutside(Hf hfr)
        {
            int N = hfr.h.Length; double Lf = hfr.lift;
            long have = 0, viol = 0, viol3 = 0, violStrict = 0, strict = 0, unc = 0; double maxExcess = double.NegativeInfinity;
            for (int i = 0; i < N; i++)
            {
                double h = hfr.h[i]; bool outside = !hfr.foot_ext[i]; double low = hfr.low[i], lowc = hfr.low_c[i];
                bool hv = outside && Finite(low); bool u = hfr.unclampable[i];
                if (hv) have++; if (u) unc++;
                bool above = Finite(low) && h > low + Lf - CLAMP_MARGIN_FT + 1e-6;
                if (hv && !u && above) { viol++; double ex = h - low - Lf + CLAMP_MARGIN_FT; if (ex > maxExcess) maxExcess = ex; }
                if (outside && !u && Finite(lowc) && h > lowc + Lf - CLAMP_MARGIN_FT + 1e-6) viol3++;
                if (hv && above) violStrict++;
                if (!hfr.foot[i] && above) strict++;
            }
            return new JObj().Set("footprint", "trncol pixel-centre coverage + " + FOOT_RING + " px ring (footprint.npy code 1)")
                .Set("outside_px_with_geometry", have).Set("violations", viol).Set("below_hf_floor_px", unc).Set("violations_strict", violStrict)
                .Set("ring_px_above_geometry", strict).Set("violations_3x3", viol3).Set("max_excess_ft", viol > 0 ? maxExcess : 0.0);
        }

        /// hf_preview: the heights through matplotlib's 'terrain' colour map (0.5 .. 99.9 percentile), the outside darkened.
        static byte[] HfPreview(Hf hfr)
        {
            int N = hfr.h.Length; var h = new double[N]; for (int i = 0; i < N; i++) h[i] = hfr.h[i];
            var sorted = (double[])h.Clone(); Array.Sort(sorted);
            double lo = UxImaging.PercentileSorted(sorted, 0.5), hi = UxImaging.PercentileSorted(sorted, 99.9);
            double span = Math.Max(hi - lo, 1e-6);
            double[] lut = UxImaging.TerrainLut(); var o = new byte[3 * N];
            for (int i = 0; i < N; i++)
            {
                double x = (h[i] - lo) / span; x = x < 0 ? 0 : x > 1 ? 1 : x;
                double xa = x * 256; if (xa == 256) xa = 255;
                int k = (int)xa; if (k < 0) k = 0; if (k > 255) k = 255;
                for (int c = 0; c < 3; c++)
                {
                    double v = lut[3 * k + c]; if (!hfr.foot[i]) v *= 0.55;
                    o[3 * i + c] = U8(v);
                }
            }
            return o;
        }

        // ===================================================================================================== main build
        /// The stage: ux_terrain.build(level, write, tint, occ_map). Reads ctx.work "level.json" and the level's disc assets; writes
        /// ctx.work "terrain/...". Returns the terrain.json document. progress(step, fraction) is optional.
        public static JObj Convert(UxContext ctx, UxTerrainOptions options = null, Action<string, double> progress = null)
            => Convert(ctx.disc, ctx.work, options, progress, ctx.cancel, ctx.log);

        public static JObj Convert(UxDiscLevel disc, UxWork work, UxTerrainOptions options = null, Action<string, double> progress = null, CancellationToken cancel = default, Action<string> log = null)
        {
            var opt = options ?? new UxTerrainOptions();
            int jobs = opt.jobs > 0 ? opt.jobs : Math.Max(1, Environment.ProcessorCount);
            Action<string> lg = UxCommon.Logger(TOOL + " " + disc.lv.shortName, log ?? (s => { }));
            void Step(string what, double f) { cancel.ThrowIfCancellationRequested(); progress?.Invoke(what, f); }
            Step("terrain: reading", 0.0);
            var src = new Source(disc, work, cancel);
            var lv = src.lv;
            UxWork outdir = work.Sub("terrain");
            lg(lv.disc + ": " + src.nt + " trncol tris, " + src.rF.Length / 3 + " render tris, " + src.mats.Count + " materials " + PyList(src.mats));
            var problems = new List<object>(); var warnings = new List<object>();
            List<int> exMat = ExclusionMaterials(src, problems);
            Step("terrain: heightfield", 0.05);
            Hf hfr = BuildHeightfield(src, exMat, lg, jobs);
            JObj st = hfr.stats;
            if (UxJson.Int(st.Get("unclampable_px")) != 0)
                problems.Add(Inv(UxJson.Int(st.Get("unclampable_px"))) + " outside pixels need the HF below 0 ft (lifted) to clear drawn geometry at the " + G(src.lift) + " ft lift; caused by " +
                    UxJson.Dumps(st.Get("unclampable_by_entity")) + ". DESIGN 16 rule " + LIFT_RULE + " gives " + G(UxJson.Num(st.Get("lift_required_ft"))) + " ft: set ux_common.LEVEL_Y_LIFT['" + lv.disc + "'] and rerun from ux_level");
            if (UxJson.Num(st.Get("lift_required_ft")) != src.lift)
                problems.Add("lift " + G(src.lift) + " ft != the DESIGN 16 rule " + LIFT_RULE + " = " + G(UxJson.Num(st.Get("lift_required_ft"))) + " ft (lowest drawn geometry outside " +
                    (st.Get("drawn_low_c_min_outside_ft") == null ? "None" : PyFormat.Repr(UxJson.Num(st.Get("drawn_low_c_min_outside_ft")))) + " ft): update ux_common.LEVEL_Y_LIFT['" + lv.disc + "']");
            Step("terrain: fidelity", 0.35);
            JObj fid = Fidelity(src, hfr.h, hfr.excl);
            lg("  fidelity: rms " + UxImaging.PyFixed(UxJson.Num(fid.Get("rms")), 4) + " max " + UxImaging.PyFixed(UxJson.Num(fid.Get("max")), 3) + " (>0.6: " + fid.Get("over_0_6ft") + ")");
            JObj reqs = MaterialRequests(src, hfr, out var gen);
            float hmin = float.PositiveInfinity, hmax = float.NegativeInfinity; foreach (float v in hfr.h) { if (v < hmin) hmin = v; if (v > hmax) hmax = v; }
            var perMat = new JObj(); long collisionOnly = 0;
            { var counts = new long[src.mats.Count]; foreach (int c in src.cm) { if (c < 0) collisionOnly++; else counts[c]++; } for (int k = 0; k < src.mats.Count; k++) perMat.Set(src.MatKey(k), counts[k]); }
            string NameOf(int k) => k >= 0 ? src.mats[k] : COLLISION_ONLY;
            JObj PerMaterial(bool[] pick)
            {
                var counts = new SortedDictionary<int, long>();
                for (int i = 0; i < src.nt; i++) if (pick[i]) { counts.TryGetValue(src.cm[i], out long c); counts[src.cm[i]] = c + 1; }
                var o = new JObj(); foreach (var kv in counts) o.Set(NameOf(kv.Key), kv.Value);
                return o;
            }
            var exNames = new List<object>(); var exIdx = new List<object>(); foreach (int k in exMat) { exNames.Add(NameOf(k)); exIdx.Add((long)k); }
            var hfJson = new JObj().Set("file", "hf.npy")
                .Set("footprint_file", "footprint.npy (uint8 1024x1024: 1 = terrain footprint incl. a " + FOOT_RING + " px ring; 0 = outside, clamped under drawn geometry; 2 = outside but the drawn geometry lies below the HF floor (0 ft), HF = 0 there)")
                .Set("min_ft", (double)hmin).Set("max_ft", (double)hmax)
                .Set("rules", new JObj().Set("hole_margin_ft", HOLE_MARGIN_FT).Set("fill_border_ft", FILL_BORDER_FT).Set("fill_drop_ft", FILL_DROP_FT)
                    .Set("clamp_margin_ft", CLAMP_MARGIN_FT).Set("clamp_safety_ft", CLAMP_SAFETY_FT).Set("clamp_reach_ft", CLAMP_REACH_FT));
            foreach (var kv in st.items) hfJson.Set(kv.Key, kv.Value);
            byte[] levelJson = work.ReadBytes("level.json");
            var result = new JObj().Set("schema", SCHEMA).Set("tool", TOOL).Set("level", lv.disc).Set("short", lv.shortName).Set("mode", lv.mode)
                .Set("terrain", src.tinfo.Get("terrain")).Set("trncol", src.tinfo.Get("trncol")).Set("materials", new List<object>(src.mats))
                .Set("source", new JObj().Set("terrain_md5", src.terrain_rec.Get("md5")).Set("trncol_md5", src.trncol_rec.Get("md5"))
                    .Set("level_json_md5", UxBin.Md5(levelJson)).Set("tool_md5", opt.tool_md5).Set("uxterrain_md5", opt.uxterrain_md5))
                .Set("grid", new JObj().Set("size", (long)HF_SIZE).Set("res_ft", HF_RES).Set("x0", X0).Set("z1", Z1).Set("orientation", "row 0 = +Z, col 0 = -X")
                    .Set("y_lift", src.lift).Set("disp_value", "hf / " + G(UxCommon.HEIGHT_SCALE)).Set("grid_resolution", "1 (enum 0)"))
                .Set("counts", new JObj().Set("trncol_tris", (long)src.nt).Set("render_tris", (long)(src.rF.Length / 3)).Set("collision_only_tris", collisionOnly)
                    .Set("per_material", perMat).Set("orientation_agreement", UxNp.PyRound(src.orient_agree, 6)))
                .Set("excluded_materials", exNames).Set("excluded_material_indices", exIdx)
                .Set("excluded", new JObj().Set("material_tris", Count(hfr.excl_mat)).Set("edge_tris", Count(hfr.edge)).Set("total", Count(hfr.excl))
                    .Set("per_material", PerMaterial(hfr.excl)).Set("edge_per_material", PerMaterial(hfr.edge)).Set("file", "excluded_tris.npy")
                    .Set("rule", "EXCLUDE table (DESIGN 5.7) + edge triangles: kept up-facing triangles within " + G(EDGE_NEAR_FT) + " ft of an excluded triangle or " + G(EDGE_BORDER_FT) +
                        " ft of the footprint border whose surface the final HF misses by > " + G(EDGE_ERR_FT) + " ft (16 sample points)"))
                .Set("heightfield", hfJson).Set("fidelity", fid).Set("problems", problems).Set("warnings", warnings);
            if (!opt.write) return result;

            var filesOut = new JObj();
            byte[] npy = UxImaging.NpyFloat32(hfr.h, HF_SIZE, HF_SIZE);
            outdir.WriteBytes("hf.npy", npy); filesOut.Set("hf.npy", UxBin.Md5(npy));
            var exList = new List<int>(); for (int i = 0; i < src.nt; i++) if (hfr.excl[i]) exList.Add(i);
            npy = UxImaging.NpyInt32(exList.ToArray());
            outdir.WriteBytes("excluded_tris.npy", npy); filesOut.Set("excluded_tris.npy", UxBin.Md5(npy));
            var code = new byte[hfr.h.Length]; for (int i = 0; i < code.Length; i++) code[i] = (byte)(hfr.foot_ext[i] ? 1 : hfr.unclampable[i] ? 2 : 0);
            npy = UxImaging.NpyUint8(code, HF_SIZE, HF_SIZE);
            outdir.WriteBytes("footprint.npy", npy); filesOut.Set("footprint.npy", UxBin.Md5(npy));
            byte[] prev = UxImaging.PngRgb(HfPreview(hfr), HF_SIZE, HF_SIZE);
            outdir.WriteBytes("hf_preview.png", prev); filesOut.Set("hf_preview.png", UxBin.Md5(prev));
            Step("terrain: ground textures", 0.45);
            Ground ground = BuildGround(src, hfr, outdir, lg, opt, jobs, null);
            if (opt.occ_map != "layout") result.Set("options", new JObj().Set("occ_map", opt.occ_map));
            foreach (var kv in ground.files.items) filesOut.Set(kv.Key, kv.Value);
            // generated dirt textures for the part materials
            Step("terrain: part textures", 0.8);
            var gfiles = new JObj();
            var chl = (JObj)((JObj)ground.json.Get("tmask")).Get("channel_layers");
            string rutsPng = null;
            foreach (string c in new[] { "B", "G", "R" }) if (chl.Get(c) as string == RUTS_TEX) { rutsPng = "dirt_" + c.ToLowerInvariant() + ".png"; break; }       // sorted(chl.items())
            var genNames = new List<string>(); foreach (var kv in gen) genNames.Add(kv.Key);
            genNames.Sort(string.CompareOrdinal);
            string SrcOf(string g) { string w = gen.Find(kv => kv.Key == g).Value; return w == "ruts" && rutsPng != null ? rutsPng : "base.png"; }
            var alias = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string g in genNames)
            {   // the same image -> one texture (the first name); the requests are renamed
                string first = genNames.Find(h => (string)filesOut.Get(SrcOf(h)) == (string)filesOut.Get(SrcOf(g)));
                if (first != g) alias[g] = first;
            }
            foreach (var kv in reqs.items)
            {
                var r = (JObj)kv.Value;
                foreach (string key in new[] { "textures", "generated_textures" })
                {
                    var l = (List<object>)r.Get(key);
                    for (int i = 0; i < l.Count; i++) if (alias.TryGetValue((string)l[i], out string to)) l[i] = to;
                }
            }
            foreach (string gname in genNames)
            {
                if (alias.ContainsKey(gname)) continue;
                string which = gen.Find(kv => kv.Key == gname).Value, srcpng = SrcOf(gname);
                UxImage im = ground.tiles[srcpng.Substring(0, srcpng.Length - 4)];
                byte[] tex = PcDxt1(im);
                string rel = "textures/" + gname + ".texture";
                outdir.WriteBytes(rel, tex);
                int mips = 1; while ((1 << mips) <= im.w) mips++;
                gfiles.Set(gname, new JObj().Set("file", rel).Set("md5", UxBin.Md5(tex)).Set("size", (long)tex.Length).Set("format", "DXT1")
                    .Set("width", (long)im.w).Set("height", (long)im.h).Set("mips", (long)mips)
                    .Set("source", "copy of " + srcpng + " (the " + which + " layer tiling texture)"));
                filesOut.Set(rel, UxBin.Md5(tex));
            }
            Step("terrain: mesh parts", 0.85);
            List<object> parts = BuildParts(src, hfr, outdir, reqs, lg, out JObj pstats);
            foreach (object po in parts)
                foreach (var kv in ((JObj)((JObj)po).Get("files")).items) filesOut.Set((string)((JObj)kv.Value).Get("file"), ((JObj)kv.Value).Get("md5"));
            var mreq = new JObj().Set("schema", "ux_terrain_material_requests/1").Set("level", lv.disc).Set("requests", reqs).Set("generated_textures", gfiles)
                .Set("note", "Materials for the excluded terrain parts (models list these names). Textures are in the template sampler order (WP0 index). UXT_* textures are generated by WP6 as PC .texture files (work/<Level>/terrain/textures/) and are not in extract/.");
            outdir.WriteJson("material_requests.json", mreq);
            filesOut.Set("material_requests.json", UxBin.Md5(outdir.ReadBytes("material_requests.json")));
            result.Set("ground", ground.json);
            result.Set("parts", parts);
            long sumClusters = 0, sumCol = 0, sumRender = 0, sumSurf = 0;
            foreach (object po in parts)
            {
                var p = (JObj)po;
                sumClusters += (long)p.Get("clusters"); sumCol += (long)p.Get("collision_tris"); sumRender += (long)p.Get("render_tris");
                sumSurf += (long)((JObj)((JObj)p.Get("files")).Get("surface")).Get("size");
            }
            var summary = new JObj().Set("cells", (long)parts.Count).Set("clusters", sumClusters).Set("collision_tris", sumCol).Set("render_tris_total", sumRender);
            foreach (var kv in pstats.items) summary.Set(kv.Key, kv.Value);
            summary.Set("surface_bytes", sumSurf).Set("entity_type", "CollisionRSData").Set("drive_on", 1L).Set("physics_type", "MXBike (0)");
            result.Set("parts_summary", summary);
            result.Set("material_requests", "material_requests.json");
            Step("terrain: checks", 0.92);
            result.Set("checks", new JObj().Set("parts", CheckParts(src, hfr, parts, outdir)).Set("coverage", CheckCoverage(src, hfr)).Set("outside", CheckOutside(hfr))
                .Set("heights", new JObj().Set("min", (double)hmin).Set("max", (double)hmax).Set("ok", hmin >= 0 && hmax <= 2047)));
            result.Set("files", filesOut);
            outdir.WriteJson("terrain.json", result);
            lg("  wrote terrain/terrain.json");
            Step("terrain: done", 1.0);
            return result;
        }
    }
}
