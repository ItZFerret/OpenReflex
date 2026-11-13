// MX vs ATV Untamed converter, stage S5 "collision": the port of the Python reference converter's ux_collision.py (DESIGN 5.6, 15, 16, 19) and of
// the part of ux_collis_to_reflex.py it uses (the primitive field swap: UxMesh.CollisToReflexPrimitive).
// Untamed .collis of the level closure (work store level.json / level_fr.json) -> Reflex PC .collis in the work store, plus collision.json
// (sources, the entity collision map, totals, problems):
//   kind 0 (triangle mesh + BVH)     -> kind 22 clusters: bad triangles dropped, edges over 64 ft split, rmesh.cluster_triangles (48 tris,
//                                        32 ft half size), read back and matched triangle by triangle
//   kind 0 used by a dynamic body     -> kind 15 box set fitted to the convex prism (fit_prism_boxes: the search over slab direction and
//                                        slab count, scored with the seeded sampling of boxfit_metrics)
//   kind 2 (male point set)           -> dropped (the entity's Male becomes its Female)
//   kind 11 (box), 17 (cylinder)      -> kind 15 with one type-5 box / type-8 cylinder
//   kind 15 (primitive list)          -> field swap; |half| for a negative box extent; type 9 (frustum) refused, or with type9 = accept
//                                        written as the enclosing type-8 cylinder
//   kind 19, kind-15 version 2        -> refused
//   pop clip (popClip = on)           -> '<asset>_UXpop' for a kind-15 source of a PopUp / PopDown bale whose sunk pose would stand above
//                                        the heightfield of terrain/hf.npy (clip_prims15 / pop_variants)
// Byte parity with the Python stage is the goal (core/plan/UNTAMED_CONVERT.md), and what decides bytes follows numpy / scipy exactly:
//   * float64 steps in numpy's order; the fused multiply-add chains of the reference's BLAS behind every '@' (dot, gemv, gemm);
//     pairwise sums; round-half-even; Python's '%g' / '%.nf' / repr in the 'reason' texts;
//   * np.cos / np.sin of the ring angles as the reference's C library returns them: the correctly rounded value (exact integer
//     arithmetic, the same on every runtime) with the 8 one-ulp exceptions measured there (Trig);
//   * np.random.default_rng(12345): PCG64 from the seeded state, random() and choice(p) (Rng12345);
//   * scipy.spatial.ConvexHull in 2-D: the hull vertices in qhull's order INCLUDING its first vertex, which decides the order of the
//     fitted boxes. Qhull2 is a port of what Qhull 2020.2 does for a 2-D input that needs no merging (initial simplex, partition,
//     furthest point, horizon, cone, the facet-list moves) plus scipy's get_extremes_2d walk. An input for which qhull would merge
//     facets (a point within about 1e-14 ft of an edge's line) raises instead of answering in another order;
//   * ConvexHull in 3-D: only the facet planes are read (max and all over them): HullPlanes finds them with exact predicates after
//     setting aside the points qhull treats as coplanar (qh.MINoutside). They equal qhull's to about 1e-13, not bit for bit.
// Not modelled bit for bit, because they feed only thresholds and values rounded to 4 - 5 decimals: np.linalg.inv (LAPACK) of the box
// rotations, the 3-D hull planes, np.argsort(-A) among faces of equal area (the axis of a prism that is not world-axis aligned).
// The 12 levels, the synthetic files of the test and three option variants per level give Python's bytes; tests/untamed_convert/check holds the result.
// Unity-free (System only). Not ported: --prims-to-22 (the 'safe' variant no current build uses: primsTo22 = true throws) with
// prim_triangles' face lists and prim_surface_error, the overlay picture (collision_overlay.png), ux_collis_to_reflex.kind0_to_22 /
// swap_collis_kind2 as a tool (--keep-kind2), the CLI and --selftest.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of ux_collision.convert. The current builds (work/combined/commands.json) run all 12 levels with type9 = "accept" and
    /// the defaults otherwise (ux_build_track: prims_to_22 False, pop_clip 'on', max_edge 64).
    public sealed class UxCollisionOptions
    {
        /// --prims-to-22 (DESIGN 15 'safe' variant): not ported; true throws.
        public bool primsTo22 = false;
        /// --type9 refuse | accept: a kind-15 primitive of type 9 (frustum) is refused, or written as the enclosing type-8 cylinder.
        public string type9 = "refuse";
        /// --max-edge: triangles of a kind-0 mesh are split until no edge is longer (0 = no split).
        public double maxEdge = UxCollision.MAX_EDGE;
        /// pop_clip on | off (ux_build_track --pop-clip).
        public string popClip = "on";
        /// write=False: convert in memory only (the manifest is returned, nothing is written or deleted).
        public bool write = true;
        /// The output folder in the work store; null = UxCollision.OutDirFor(primsTo22, type9) ('collision', 'collision-t9').
        public string outDir = null;
        /// Sources converted at a time (0 = the processor count, 1 = serial). The result does not depend on it.
        public int threads = 0;
        /// For tests: evaluate every sample of the box-fit metrics with the fused multiply-add chains of the reference's BLAS. By default
        /// a sample goes through them only where plain arithmetic cannot decide (a distance within 1e-9 ft of the 1e-9 limits, a
        /// maximum or mean within 1e-9 of a rounding boundary of its 4 decimals): the same result, about 8 times faster.
        public bool exactMetrics = false;
        /// Template collis names by slot kind ('sx', 'fr': lower name -> name) for the name-clash rule (asset 'UX_<name>'); null = the
        /// tables of ctx.stock.templateNames (U.TEMPLATE_DIR in Python; see UNTAMED_CONVERT.md, known gaps).
        public Dictionary<string, Dictionary<string, string>> templateCollis = null;
        /// Environment data Python records in collision.json and a port cannot compute: 'tool_md5' (md5 of ux_collision.py), 'readers'
        /// (md5s of uxmesh.py, ux_collis_to_reflex.py, rmesh.py) and options 'out_dir' (an absolute path there; null = the store-relative
        /// folder). They are written as given (null / {} by default).
        public string toolMd5 = null;
        public JObj readers = null;
        public string outDirText = null;
    }

    /// ux_collision.py.
    public static class UxCollision
    {
        public const string SCHEMA = "ux_collision/1";
        public const int MAX_TRIS = 48;
        public const double MAX_HALF = 32.0, MAX_EDGE = 64.0, TOL = 2e-3;
        public const int CYL_SIDES = 16, SPH_SEG = 16, SPH_RING = 8;
        static readonly string[] KEEP_CLS = { "keep", "demote", "synth" };
        static readonly string[] DYNAMIC_TYPES = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData" };
        public const double BOXFIT_SHELL = 0.4, BOXFIT_TOL_IN = 0.05, BOXFIT_TOL_SURF = 0.25, BOXFIT_MIN_VOL = 0.90, BOXFIT_MAX_DENT = 0.15, BOXFIT_SIDE_TOL = 0.05;
        static readonly string[] POP_TYPES = { "PopUpBaleRSData", "PopDownBaleRSData" };
        public const double POP_SINK_FT = 5.0, POP_KEEP_FT = 4.75;

        /// out_dir_for(level, prims_to_22, type9): the folder below work/<Level>.
        public static string OutDirFor(bool primsTo22 = false, string type9 = "refuse") => "collision" + (primsTo22 ? "-prims22" : "") + (type9 != "refuse" ? "-t9" : "");

        // ====================================================================================== Python text formatting
        static BigInteger RoundScaled(double av, int pow10)
        {   // round-half-even(av * 10^pow10) for a finite av >= 0, exact
            long bits = BitConverter.DoubleToInt64Bits(av);
            int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;
            BigInteger num = m, den = BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
            if (pow10 >= 0) num *= BigInteger.Pow(10, pow10); else den *= BigInteger.Pow(10, -pow10);
            var q = BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            return q;
        }
        static bool Negative(double v) => v < 0 || v == 0 && 1 / v < 0;

        /// '%.<decimals>f' % v.
        internal static string PyF(double v, int decimals)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            string s = RoundScaled(Math.Abs(v), decimals).ToString(CultureInfo.InvariantCulture);
            if (decimals > 0)
            {
                if (s.Length < decimals + 1) s = new string('0', decimals + 1 - s.Length) + s;
                s = s.Substring(0, s.Length - decimals) + "." + s.Substring(s.Length - decimals);
            }
            return (Negative(v) ? "-" : "") + s;
        }

        /// '%g' % v ('%.<precision>g').
        internal static string PyG(double v, int precision = 6)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            string sign = Negative(v) ? "-" : "";
            if (v == 0) return sign + "0";
            double av = Math.Abs(v);
            if (precision < 1) precision = 1;
            int k = (int)Math.Floor(Math.Log10(av));
            BigInteger lim = BigInteger.Pow(10, precision);
            BigInteger q;
            while (true)
            {   // the exponent of the rounded value (a carry to 10^precision moves it up; it then reads 10^(precision - 1))
                q = RoundScaled(av, precision - 1 - k);
                if (q >= lim) { k++; continue; }
                if (q < lim / 10) { k--; continue; }
                break;
            }
            string digits = q.ToString(CultureInfo.InvariantCulture);       // 'precision' digits, value = d.ddd * 10^k
            if (k < -4 || k >= precision)
            {
                string frac = digits.Substring(1).TrimEnd('0');
                return sign + digits[0] + (frac.Length > 0 ? "." + frac : "") + "e" + (k < 0 ? "-" : "+") + Math.Abs(k).ToString("00", CultureInfo.InvariantCulture);
            }
            if (k >= 0)
            {
                string frac = digits.Substring(k + 1).TrimEnd('0');
                return sign + digits.Substring(0, k + 1) + (frac.Length > 0 ? "." + frac : "");
            }
            string f2 = (new string('0', -k - 1) + digits).TrimEnd('0');
            return sign + "0." + f2;
        }
        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);
        static double R(double v, int n) => UxNp.PyRound(v, n);

        // ====================================================================================== cos / sin of the reference
        /// np.cos / np.sin as the reference machine's C library returns them for the angles this stage uses (2 pi i / 16, 64, 128 and
        /// pi i / 8, 32, pi / 128): the correctly rounded value (exact integer arithmetic, the same on every runtime), except for 8
        /// arguments where that library is one unit in the last place off (measured against it: 502 values).
        internal static class Trig
        {
            const int P = 320;
            // (cos?, argument bits, ulps added to the correctly rounded result's bit pattern)
            static readonly long[][] s_exceptions =
            {
                new long[] { 1, 0x3FE78FDB9EFFEA46L, 1 },      // cos(2 pi 15 / 128)
                new long[] { 1, 0x4000E2D4DC9DCE4CL, -1 },     // cos(2 pi 43 / 128)
                new long[] { 1, 0x401566F005C20E66L, -1 },     // cos(2 pi 109 / 128)
                new long[] { 1, 0x40166243D70CB829L, -1 },     // cos(2 pi 57 / 64)
                new long[] { 0, 0x3FE921FB54442D18L, 1 },      // sin(pi 8 / 32)
                new long[] { 0, 0x3FEC463ABECCB2BBL, -1 },     // sin(pi 9 / 32)
                new long[] { 0, 0x4001475CC9EEDF00L, -1 },     // sin(pi 22 / 32)
                new long[] { 0, 0x400534AC0F19860CL, -1 },     // sin(pi 27 / 32)
            };
            public static double Cos(double x) => Eval(x, true);
            public static double Sin(double x) => Eval(x, false);
            static readonly Dictionary<(long, int, int), double[]> s_tables = new Dictionary<(long, int, int), double[]>();
            /// cos and sin of mult * i / n for i < count (mult = 2 pi: a ring; pi: the sphere's rings): [cos 0, sin 0, cos 1, sin 1, ...].
            public static double[] Table(double mult, int n, int count)
            {
                var key = (BitConverter.DoubleToInt64Bits(mult), n, count);
                lock (s_tables)
                {
                    if (!s_tables.TryGetValue(key, out var t))
                    {
                        t = new double[2 * count];
                        for (int i = 0; i < count; i++) { double a = mult * i / n; t[2 * i] = Cos(a); t[2 * i + 1] = Sin(a); }
                        s_tables[key] = t;
                    }
                    return t;
                }
            }
            static double Eval(double x, bool cos)
            {
                UxBin.Require(x >= 0 && x < 8, "Trig: argument outside [0, 8)");
                if (x == 0) return cos ? 1.0 : 0.0;
                long bits = BitConverter.DoubleToInt64Bits(x);
                int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
                long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;
                UxBin.Require(e + P >= 0, "Trig: argument too small");
                BigInteger X = new BigInteger(m) << (e + P), one = BigInteger.One << P;
                BigInteger X2 = (X * X) >> P;
                BigInteger term = cos ? one : X, sum = term;
                int k = cos ? 0 : 1;
                while (true)
                {
                    k += 2;
                    term = -((term * X2) >> P) / ((long)(k - 1) * k);
                    if (term.IsZero) break;
                    sum += term;
                }
                double r = sum.IsZero ? 0.0 : sum.Sign < 0 ? -UxJson.Ratio(-sum, one) : UxJson.Ratio(sum, one);
                foreach (var ex in s_exceptions)
                    if ((ex[0] != 0) == cos && ex[1] == bits) return BitConverter.Int64BitsToDouble(BitConverter.DoubleToInt64Bits(r) + ex[2]);
                return r;
            }
        }

        // ====================================================================================== PCG64 of np.random.default_rng(12345)
        /// numpy's PCG64 (XSL-RR 128/64) in the state SeedSequence(12345) seeds: random() and choice(n, size, p).
        internal sealed class Rng12345
        {
            // state and increment after seeding (bit_generator.state of default_rng(12345))
            ulong _sh, _sl, _ih, _il;
            const ulong MH = 0x2360ED051FC65DA4UL, ML = 0x4385DF649FCCF645UL;
            static bool s_checked;
            public Rng12345()
            {
                // bit_generator.state of default_rng(12345): 'state' and 'inc' (128-bit); the first value is checked once
                var st = BigInteger.Parse("33261208707367790463622745601869196757", CultureInfo.InvariantCulture);
                var inc = BigInteger.Parse("268209174141567072605526753992732310247", CultureInfo.InvariantCulture);
                var mask = (BigInteger.One << 64) - 1;
                _sh = (ulong)(st >> 64); _sl = (ulong)(st & mask); _ih = (ulong)(inc >> 64); _il = (ulong)(inc & mask);
                if (!s_checked)
                {   // the first value of default_rng(12345).random(): 0x1.d1958c6d97fe0p-3
                    ulong sh = _sh, sl = _sl;
                    double first = Random();
                    UxBin.Require(BitConverter.DoubleToInt64Bits(first) == 0x3FCD1958C6D97FE0L, "PCG64: the first value of default_rng(12345) is wrong");
                    _sh = sh; _sl = sl; s_checked = true;
                }
            }
            static ulong MulHi(ulong a, ulong b)
            {
                ulong a0 = (uint)a, a1 = a >> 32, b0 = (uint)b, b1 = b >> 32;
                ulong p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
                ulong mid = (p00 >> 32) + (uint)p01 + (uint)p10;
                return p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
            }
            ulong Next64()
            {
                // state = state * MULT + inc (mod 2^128)
                ulong lo = unchecked(_sl * ML);
                ulong hi = unchecked(MulHi(_sl, ML) + _sh * ML + _sl * MH);
                ulong nl = unchecked(lo + _il);
                ulong nh = unchecked(hi + _ih + (nl < lo ? 1UL : 0UL));
                _sl = nl; _sh = nh;
                ulong x = _sh ^ _sl; int rot = (int)(_sh >> 58);
                return rot == 0 ? x : (x >> rot) | (x << (64 - rot));
            }
            /// rng.random(): [0, 1), 53 bits.
            public double Random() => (Next64() >> 11) * (1.0 / 9007199254740992.0);
            public double[] Random(int n) { var r = new double[n]; for (int i = 0; i < n; i++) r[i] = Random(); return r; }
            /// rng.choice(len(p), size, p=p): cdf = p.cumsum(); cdf /= cdf[-1]; cdf.searchsorted(random(size), side='right').
            public int[] Choice(double[] p, int size)
            {
                int n = p.Length; var cdf = new double[n]; double s = 0;
                for (int i = 0; i < n; i++) { s += p[i]; cdf[i] = s; }
                double last = cdf[n - 1];
                for (int i = 0; i < n; i++) cdf[i] /= last;
                var idx = new int[size];
                for (int j = 0; j < size; j++)
                {
                    double u = Random(); int lo = 0, hi = n;
                    while (lo < hi) { int mid = (lo + hi) >> 1; if (cdf[mid] <= u) lo = mid + 1; else hi = mid; }
                    idx[j] = lo;
                }
                return idx;
            }
        }

        // ====================================================================================== triangle meshes
        /// The 9 values of a triangle as a key (== equality: -0.0 and 0.0 are the same value, as np.unique compares them).
        static string Row(double[] t, int p)
        {
            var sb = new System.Text.StringBuilder(9 * 17);
            for (int k = 0; k < 9; k++) { double v = t[p + k]; if (v == 0) v = 0.0; sb.Append(BitConverter.DoubleToInt64Bits(v).ToString("x16")); }
            return sb.ToString();
        }
        /// clean_tris(tris): the finite triangles whose |cross| is above 1e-9 and that repeat no earlier triangle; nonfinite and degenerate
        /// (incl. the duplicates) are the dropped counts.
        public static double[] CleanTris(double[] tris, out int nonfinite, out int degenerate)
        {
            int n = tris.Length / 9; nonfinite = 0; degenerate = 0;
            var keep = new List<int>(n); var seen = new HashSet<string>(StringComparer.Ordinal);
            for (int i = 0; i < n; i++)
            {
                int p = 9 * i; bool fin = true;
                for (int k = 0; k < 9; k++) if (!UxNp.Finite(tris[p + k])) { fin = false; break; }
                if (!fin) { nonfinite++; continue; }
                double ax = tris[p + 3] - tris[p], ay = tris[p + 4] - tris[p + 1], az = tris[p + 5] - tris[p + 2];
                double bx = tris[p + 6] - tris[p], by = tris[p + 7] - tris[p + 1], bz = tris[p + 8] - tris[p + 2];
                double c0 = ay * bz - az * by, c1 = az * bx - ax * bz, c2 = ax * by - ay * bx;
                if (Math.Sqrt((c0 * c0 + c1 * c1) + c2 * c2) > 1e-9 && seen.Add(Row(tris, p))) keep.Add(i); else degenerate++;
                // (Clearwater round) an exact duplicate of an earlier finite triangle (same 9 values, same order; np.unique(axis=0) keeps the
                // first) is dropped too and counted with the degenerate ones. A duplicate of a degenerate triangle is degenerate itself.
            }
            var o = new double[keep.Count * 9];
            for (int i = 0; i < keep.Count; i++) Array.Copy(tris, 9 * keep[i], o, 9 * i, 9);
            return o;
        }

        /// split_long_edges(tris, max_edge): every triangle whose longest edge exceeds max_edge is split at that edge's midpoint (both
        /// halves keep the winding), repeated until no edge is longer.
        public static double[] SplitLongEdges(double[] tris, double maxEdge, out int nsplit)
        {
            nsplit = 0;
            if (maxEdge == 0 || tris.Length == 0) return tris;
            var cur = (double[])tris.Clone();
            bool converged = false;
            for (int pass = 0; pass < 64; pass++)
            {
                int n = cur.Length / 9;
                var bi = new List<int>(); var bk = new List<int>();
                for (int i = 0; i < n; i++)
                {
                    int p = 9 * i; double emax = 0; int kmax = 0;
                    for (int k = 0; k < 3; k++)
                    {
                        int a = p + 3 * k, b = p + 3 * ((k + 1) % 3);
                        double dx = cur[b] - cur[a], dy = cur[b + 1] - cur[a + 1], dz = cur[b + 2] - cur[a + 2];
                        double e = Math.Sqrt((dx * dx + dy * dy) + dz * dz);
                        if (k == 0 || e > emax) { emax = e; kmax = k; }
                    }
                    if (emax > maxEdge) { bi.Add(i); bk.Add(kmax); }
                }
                if (bi.Count == 0) { converged = true; break; }
                var next = new double[cur.Length + 9 * bi.Count];
                Array.Copy(cur, next, cur.Length);
                for (int r = 0; r < bi.Count; r++)
                {
                    int p = 9 * bi[r], k = bk[r], i0 = p + 3 * k, i1 = p + 3 * ((k + 1) % 3), i2 = p + 3 * ((k + 2) % 3);
                    int q = cur.Length + 9 * r;
                    for (int c = 0; c < 3; c++)
                    {
                        double mid = (cur[i0 + c] + cur[i1 + c]) / 2;
                        next[p + c] = cur[i0 + c]; next[p + 3 + c] = mid; next[p + 6 + c] = cur[i2 + c];
                        next[q + c] = mid; next[q + 3 + c] = cur[i1 + c]; next[q + 6 + c] = cur[i2 + c];
                    }
                }
                cur = next; nsplit += bi.Count;
            }
            if (!converged) throw new UxException("edge split did not converge");
            return cur;
        }

        sealed class MeshInfo { public int tris_in, tris, split, nonfinite, degenerate, clusters, max_cluster_tris; public double max_cluster_half; }

        /// kind22_bytes(tris, max_edge): (bytes, info, the cleaned and split triangles).
        static byte[] Kind22Bytes(double[] tris, double maxEdge, out MeshInfo info, out double[] t)
        {
            t = CleanTris(tris, out int nf, out int dg);
            t = SplitLongEdges(t, maxEdge, out int ns);
            var cl = t.Length > 0 ? UxRmesh.ClusterTriangles(t, MAX_TRIS, MAX_HALF) : new List<UxRmesh.Cluster>();
            double mh = 0.0; int mt = 0; bool first = true;
            foreach (var c in cl)
            {
                float m = 0; bool f1 = true;
                foreach (float x in c.t) { float a = Math.Abs(x); if (f1 || a > m) { m = a; f1 = false; } }
                if (first || (double)m > mh) { mh = m; first = false; }
                if (c.TriCount > mt) mt = c.TriCount;
            }
            info = new MeshInfo { tris_in = tris.Length / 9, tris = t.Length / 9, split = ns, nonfinite = nf, degenerate = dg, clusters = cl.Count, max_cluster_tris = mt, max_cluster_half = mh };
            return UxRmesh.WriteCollis(cl);
        }

        /// verify_kind22(data, tris): the file is read back and every triangle matched to its nearest source triangle.
        static List<string> VerifyKind22(byte[] data, double[] reference, out JObj st)
        {
            var probs = new List<string>(); st = new JObj();
            var cl = UxRmesh.ReadCollis(data, out uint kind);
            if (kind != 22) { probs.Add("kind " + kind + ", expected 22"); return probs; }
            int ng = 0; foreach (var c in cl) ng += c.TriCount;
            var got = new double[ng * 9]; int at = 0; int mt = 0; double mh = 0.0; bool first = true;
            foreach (var c in cl)
            {
                float m = 0; bool f1 = true;
                for (int i = 0; i < c.t.Length; i++)
                {
                    got[at++] = (double)c.c[i % 3] + (double)c.t[i];
                    float a = Math.Abs(c.t[i]); if (f1 || a > m) { m = a; f1 = false; }
                }
                if (c.TriCount > mt) mt = c.TriCount;
                if (first || (double)m > mh) { mh = m; first = false; }
            }
            int nr = reference.Length / 9;
            st.Set("tris", (long)ng).Set("clusters", (long)cl.Count).Set("max_err", 0.0).Set("max_cluster_tris", (long)mt).Set("max_cluster_half", mh);
            if (ng != nr) { probs.Add("triangle count " + ng + " != " + nr); return probs; }
            if (nr > 0)
            {
                // cKDTree(ref).query(got): the nearest source triangle in 9 dimensions (the lowest index of equally near ones). The
                // source triangles are visited outward from the query's first coordinate, until that coordinate alone is too far.
                var used = new bool[nr]; int distinct = 0; double err = 0;
                var order = new int[nr]; var xs = new double[nr];
                for (int r = 0; r < nr; r++) { order[r] = r; xs[r] = reference[9 * r]; }
                Array.Sort(xs, order);
                for (int g = 0; g < ng; g++)
                {
                    int gp = 9 * g; double best = double.PositiveInfinity; int bi = -1; double gx = got[gp];
                    int hi = Array.BinarySearch(xs, gx); if (hi < 0) hi = ~hi;
                    int lo = hi - 1;
                    while (lo >= 0 || hi < nr)
                    {
                        double dl = lo >= 0 ? gx - xs[lo] : double.PositiveInfinity, dh = hi < nr ? xs[hi] - gx : double.PositiveInfinity;
                        int r; double dx;
                        if (dl <= dh) { r = order[lo--]; dx = dl; } else { r = order[hi++]; dx = dh; }
                        if (dx * dx > best) break;
                        int rp = 9 * r; double s = 0;
                        for (int k = 0; k < 9; k++) { double dd = got[gp + k] - reference[rp + k]; s += dd * dd; if (s > best) break; }
                        if (s < best || s == best && r < bi) { best = s; bi = r; }
                    }
                    if (bi < 0) bi = 0;
                    if (!used[bi]) { used[bi] = true; distinct++; }
                    for (int k = 0; k < 9; k++) { double a = Math.Abs(got[gp + k] - reference[9 * bi + k]); if (a > err) err = a; }
                }
                st.Set("max_err", err);
                if (distinct != nr) probs.Add("read-back triangles are not a one-to-one match of the source set");
                if (err > TOL) probs.Add("read-back max vertex error " + PyG(err, 4) + " ft > " + PyG(TOL));
            }
            if (mt > MAX_TRIS) probs.Add("cluster with " + mt + " triangles > " + MAX_TRIS);
            if (mh > MAX_HALF + 1e-3) probs.Add("cluster half size " + PyF(mh, 3) + " ft > " + PyG(MAX_HALF));
            return probs;
        }

        // ====================================================================================== primitives
        /// A kind-15 primitive dict {type, matrix, params, m1}.
        internal sealed class Prim
        {
            public uint type, m1; public double[] matrix, prm;
            public Prim Copy() => new Prim { type = type, m1 = m1, matrix = (double[])matrix.Clone(), prm = (double[])prm.Clone() };
        }
        static readonly double[] Eye4 = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        const double TwoPi = 2 * Math.PI;

        /// The vertices of _box(half) (the 8 corners; the triangles only repeat them).
        static double[] BoxCorners(double[] half)
        {
            var c = new double[24]; int n = 0;
            for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2)
            { c[n++] = sx * half[0]; c[n++] = sy * half[1]; c[n++] = sz * half[2]; }
            return c;
        }
        /// The vertices of _solid_of_revolution(r_bottom, r_top, height, n): the bottom ring, then the top ring.
        static double[] RevolutionVertices(double rBottom, double rTop, double height, int n)
        {
            var o = new double[6 * n]; double y0 = -height / 2, y1 = height / 2;
            var ring = Trig.Table(TwoPi, n, n);
            for (int i = 0; i < n; i++)
            {
                double c = ring[2 * i], s = ring[2 * i + 1];
                o[3 * i] = rBottom * c; o[3 * i + 1] = y0; o[3 * i + 2] = rBottom * s;
                o[3 * (n + i)] = rTop * c; o[3 * (n + i) + 1] = y1; o[3 * (n + i) + 2] = rTop * s;
            }
            return o;
        }
        /// The vertices of _sphere(r, seg, ring): the grid P.
        static double[] SphereVertices(double r, int seg, int ring)
        {
            var o = new double[3 * (ring + 1) * seg]; int at = 0;
            double[] tth = Trig.Table(Math.PI, ring, ring + 1), tph = Trig.Table(TwoPi, seg, seg);
            for (int a = 0; a <= ring; a++)
            {
                double ct = tth[2 * a], st = tth[2 * a + 1];
                for (int i = 0; i < seg; i++) { o[at++] = st * tph[2 * i] * r; o[at++] = ct * 1.0 * r; o[at++] = st * tph[2 * i + 1] * r; }
            }
            return o;
        }
        /// _apply(tris, m): points @ m[:3, :3] + m[3, :3] (the BLAS chain of UxNp.MatMul).
        static double[] Apply(double[] pts, double[] m)
        {
            var rot = new double[9];
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) rot[3 * i + j] = m[4 * i + j];
            var w = UxNp.MatMul(pts, pts.Length / 3, 3, rot, 3);
            for (int i = 0; i < w.Length; i++) w[i] += m[12 + i % 3];
            return w;
        }
        /// The vertices of prim_triangles(prim) (world / object space).
        static double[] PrimVertices(Prim p)
        {
            double[] loc;
            if (p.type == 5) loc = BoxCorners(p.prm);
            else if (p.type == 7) loc = SphereVertices(p.prm[0], SPH_SEG, SPH_RING);
            else if (p.type == 8) loc = RevolutionVertices(p.prm[0], p.prm[0], p.prm[1], CYL_SIDES);
            else if (p.type == 9) loc = RevolutionVertices(p.prm[0], p.prm[1], p.prm[2], CYL_SIDES);
            else throw new UxException("primitive type " + p.type);
            return Apply(loc, p.matrix);
        }
        /// prim_bounds(prim): the conservative AABB (a fine tessellation scaled out by 1 / cos(pi / n)).
        static void PrimBounds(Prim p, out double[] lo, out double[] hi)
        {
            double[] loc;
            if (p.type == 5) loc = BoxCorners(p.prm);
            else
            {
                const int n = 128;
                double f = 1 / Trig.Cos(Math.PI / n);
                if (p.type == 7) loc = SphereVertices(p.prm[0] * f, 64, 32);
                else if (p.type == 8) loc = RevolutionVertices(p.prm[0] * f, p.prm[0] * f, p.prm[1], n);
                else if (p.type == 9) loc = RevolutionVertices(p.prm[0] * f, p.prm[1] * f, p.prm[2], n);
                else throw new UxException("primitive type " + p.type);
            }
            var w = Apply(loc, p.matrix);
            lo = new[] { w[0], w[1], w[2] }; hi = new[] { w[0], w[1], w[2] };
            for (int i = 3; i < w.Length; i++) { int c = i % 3; if (w[i] < lo[c]) lo[c] = w[i]; if (w[i] > hi[c]) hi[c] = w[i]; }
        }

        /// src_primitives(d): the primitive list of a kind 11 / 15 / 17 source.
        static List<Prim> SrcPrimitives(UxCollis d)
        {
            var o = new List<Prim>();
            if (d.kind == 11) o.Add(new Prim { type = 5, matrix = (double[])Eye4.Clone(), prm = (double[])d.half.Clone(), m1 = 0xFFFFFFFF });
            else if (d.kind == 17) o.Add(new Prim { type = 8, matrix = (double[])Eye4.Clone(), prm = new[] { d.radius, d.half_height }, m1 = 0xFFFFFFFF });
            else foreach (var p in d.prims) o.Add(new Prim { type = p.type, matrix = (double[])p.matrix.Clone(), prm = (double[])p.@params.Clone(), m1 = p.m1 });
            return o;
        }
        static Prim AbsBox(Prim p)
        {
            if (p.type != 5) return p;
            var q = p.Copy(); for (int i = 0; i < q.prm.Length; i++) q.prm[i] = Math.Abs(q.prm[i]);
            return q;
        }
        static Prim Type9Cylinder(Prim p)
        {
            var q = p.Copy(); q.type = 8;
            q.prm = new[] { p.prm[1] > p.prm[0] ? p.prm[1] : p.prm[0], p.prm[2] };       // max(p0, p1)
            return q;
        }
        /// _write_prims15(prims).
        static byte[] WritePrims15(List<Prim> prims)
        {
            var w = new UxWriter(28 + 88 * prims.Count);
            w.U32(1234).Bytes(new byte[] { (byte)'c', (byte)'o', (byte)'l', 0 }).U32(3).U32(15).U32(0).U32((uint)prims.Count).U32(0);
            foreach (var pr in prims)
            {
                w.U32(pr.type);
                for (int i = 0; i < 16; i++) w.F32((float)pr.matrix[i]);
                w.U32(pr.m1);
                if (pr.type == 5) { for (int i = 0; i < 3; i++) w.F32((float)pr.prm[i]); w.U32(0); }
                else foreach (double x in pr.prm) w.F32((float)x);
            }
            return w.ToArray();
        }

        /// parse_pc_collis(data): Reflex PC (LE) collis of kind 11, 15 (primitives) or 22.
        internal sealed class PcCollis { public uint kind, zero, ntag, pad; public int tail; public double[] half; public List<Prim> prims; public List<UxRmesh.Cluster> clusters; }
        internal static PcCollis ParsePcCollis(byte[] data)
        {
            if (data.Length < 16 || UxBin.U32LE(data, 0) != 1234 || data[4] != 'c' || data[5] != 'o' || data[6] != 'l' || data[7] != 0 || UxBin.U32LE(data, 8) != 3)
                throw new UxException("not a Reflex PC collis");
            var d = new PcCollis { kind = UxBin.U32LE(data, 12) };
            if (d.kind == 11)
            {
                UxBin.Require(data.Length >= 0x24, "collis kind 11: truncated");
                d.zero = UxBin.U32LE(data, 0x10); d.half = new double[] { UxBin.F32LE(data, 0x14), UxBin.F32LE(data, 0x18), UxBin.F32LE(data, 0x1C) };
                d.pad = UxBin.U32LE(data, 0x20); d.tail = data.Length - 0x24;
            }
            else if (d.kind == 15)
            {
                UxBin.Require(data.Length >= 0x1C, "collis kind 15: truncated");
                d.zero = UxBin.U32LE(data, 0x10); int n = (int)UxBin.U32LE(data, 0x14); d.ntag = UxBin.U32LE(data, 0x18);
                int p = 0x1C + (int)d.ntag; d.prims = new List<Prim>();
                for (int i = 0; i < n; i++)
                {
                    UxBin.Require(p + 4 <= data.Length, "collis kind 15: truncated");
                    uint ty = UxBin.U32LE(data, p); int size = UxMesh.PrimSize(ty);
                    if (size == 0) throw new UxException(ty.ToString(CultureInfo.InvariantCulture));            // KeyError(ty)
                    UxBin.Require(p + size <= data.Length, "collis kind 15: primitive outside the file");
                    var mtx = new double[16]; for (int k = 0; k < 16; k++) mtx[k] = UxBin.F32LE(data, p + 4 + 4 * k);
                    int nf = (size - 72) / 4; var prm = new double[nf]; for (int k = 0; k < nf; k++) prm[k] = UxBin.F32LE(data, p + 72 + 4 * k);
                    d.prims.Add(new Prim { type = ty, matrix = mtx, m1 = UxBin.U32LE(data, p + 68), prm = prm });
                    p += size;
                }
                d.tail = data.Length - p;
            }
            else if (d.kind == 22) d.clusters = UxRmesh.ReadCollis(data, out _);
            return d;
        }

        static bool SameF32(double[] a, double[] b, int n)
        {
            if (a.Length < n || b.Length < n) return false;
            for (int i = 0; i < n; i++) if (!((float)a[i] == (float)b[i])) return false;
            return true;
        }

        /// verify_primitive(src, data): the LE output re-parses to the source's primitive parameters exactly.
        static List<string> VerifyPrimitive(uint srcKind, double[] srcHalf, List<Prim> srcPrims, byte[] data)
        {
            var probs = new List<string>();
            PcCollis o;
            try { o = ParsePcCollis(data); }
            catch (Exception e) { probs.Add("re-parse failed: " + e.Message); return probs; }
            if (o.tail != 0) probs.Add(o.tail + " trailing bytes");
            if (srcKind == 11)
            {
                if (o.kind != 15) return new List<string> { "kind " + o.kind + " != 15 (kind 11 is written as kind 15)" };
                if (o.ntag != 0 || o.prims.Count != 1) { probs.Add(o.prims.Count + " primitives / " + o.ntag + " tag bytes, expected 1 / 0"); return probs; }
                var b = o.prims[0];
                if (b.type != 5 || b.m1 != 0xFFFFFFFF || !SameF32(b.matrix, Eye4, 16)) probs.Add("not one identity-matrix type-5 box with m1 0xFFFFFFFF");
                var ah = new[] { Math.Abs(srcHalf[0]), Math.Abs(srcHalf[1]), Math.Abs(srcHalf[2]) };
                if (!SameF32(b.prm, ah, 3)) probs.Add("box half extents differ");
                return probs;
            }
            if (o.kind != 15) { probs.Add("kind " + o.kind + " != 15"); return probs; }
            if (o.ntag != 0) probs.Add("tag bytes kept (" + o.ntag + ")");
            if (srcPrims.Count != o.prims.Count) { probs.Add("primitive count " + o.prims.Count + " != " + srcPrims.Count); return probs; }
            for (int i = 0; i < srcPrims.Count; i++)
            {
                Prim a = srcPrims[i], b = o.prims[i];
                int n = a.type == 5 ? 3 : a.prm.Length;
                if (a.type == 5)
                {
                    a = AbsBox(a);
                    if (b.prm.Length >= 3 && (b.prm[0] < 0 || b.prm[1] < 0 || b.prm[2] < 0)) probs.Add("primitive " + i + ": negative box half extent written");
                }
                if (a.type != b.type || a.m1 != b.m1 || !SameF32(a.matrix, b.matrix, 16) || !SameF32(a.prm, b.prm, n)) probs.Add("primitive " + i + " differs");
            }
            return probs;
        }

        // ====================================================================================== convex hulls (scipy.spatial.ConvexHull)
        sealed class HullException : Exception { public HullException(string m) : base(m) { } }

        /// ConvexHull(P).vertices for 2-D points (n, 2): the hull vertices counter-clockwise, in qhull's order (its first vertex
        /// included). A port of what Qhull 2020.2 ('qhull i Qt', as scipy runs it) does for a 2-D input that needs no merging:
        /// qh_maxmin / qh_maxsimplex, qh_createsimplex / qh_initialhull, qh_partitionall, qh_furthestnext, and per added point
        /// qh_findhorizon, qh_makenewfacets, qh_matchnewfacets, qh_partitionvisible (qh_findbest / qh_findbestnew / qh_findbesthorizon)
        /// with the facet-list moves that decide which facet is first at the end; then scipy's get_extremes_2d walk. Inputs for which
        /// qhull would merge facets (a point within roundoff of an edge's line: coplanar horizon, a new facet not clearly convex, a narrow
        /// or flat initial simplex) throw: their order is not reproduced. Not kept: qhull's coplanar point sets (they only raise
        /// qh.max_outside, a search margin of a few DISTround). Measured against scipy 1.18.1: 3,428 point sets in general position
        /// (3 to 400 points, duplicates, up to 300 hull vertices) and the stage's own 28 inputs give the identical order; of 600 sets
        /// with exactly collinear points 330 do and 270 are refused, none is answered in another order.
        internal sealed class Qhull2
        {
            sealed class Facet
            {
                public int id, v0, v1; public Facet n0, n1; public bool top, newfacet, visible, flipped;
                public double nx, ny, off, furthestdist; public List<int> outside; public uint visitid; public Facet prev, next, replace;
            }
            const double Eps = 2.220446049250313e-16, RealMax = double.MaxValue, RealMin = 2.2250738585072014e-308;
            readonly double[] _p; readonly int _n;
            readonly List<int> _vpoint = new List<int>();
            Facet facet_list, facet_next, newfacet_list, visible_list, facet_tail;
            int facet_id, num_facets, num_visible, num_outside; uint visit_id;
            double DISTround, MINoutside, MINvisible, MAXcoplanar, max_outside, MAXoutside, ONEmerge, MINdenom, MAXwidth, MAXabs, MAXsum, ix, iy;
            readonly double[] NEARzero = new double[2];
            bool findbestnew, findbest_notsharp;

            public static int[] Vertices(double[] points)
            {
                var q = new Qhull2(points);
                q.InitBuild(); q.BuildHull();
                return q.Extremes();
            }
            Qhull2(double[] points) { _p = points; _n = points.Length / 2; }

            double X(int p) => _p[2 * p];
            double Y(int p) => _p[2 * p + 1];
            double Dist(double x, double y, Facet f) => f.off + x * f.nx + y * f.ny;
            double Dist(int p, Facet f) => f.off + _p[2 * p] * f.nx + _p[2 * p + 1] * f.ny;

            // ---- facet list (poly_r.c)
            Facet NewFacet() => new Facet { id = facet_id++, newfacet = true };
            void AppendFacet(Facet facet)
            {
                Facet tail = facet_tail;
                if (tail == newfacet_list) { newfacet_list = facet; if (tail == visible_list) visible_list = facet; }
                if (tail == facet_next) facet_next = facet;
                facet.prev = tail.prev; facet.next = tail;
                if (tail.prev != null) tail.prev.next = facet; else facet_list = facet;
                tail.prev = facet; num_facets++;
            }
            void RemoveFacet(Facet facet)
            {
                Facet next = facet.next, previous = facet.prev;
                if (facet == newfacet_list) newfacet_list = next;
                if (facet == facet_next) facet_next = next;
                if (facet == visible_list) visible_list = next;
                if (previous != null) { previous.next = next; next.prev = previous; }
                else { facet_list = next; facet_list.prev = null; }
                num_facets--;
            }
            void PrependFacetAtNext(Facet facet)
            {   // qh_prependfacet(facet, &qh.facet_next)
                if (facet_next == null) facet_next = facet_tail;
                Facet list = facet_next, prevfacet = list.prev;
                facet.prev = prevfacet;
                if (prevfacet != null) prevfacet.next = facet;
                list.prev = facet; facet.next = list;
                if (facet_list == list) facet_list = facet;
                facet_next = facet;
                num_facets++;
            }
            void ResetLists()
            {   // qh_resetlists(.., qh_RESETvisible)
                for (Facet f = newfacet_list; f != null && f.next != null; f = f.next) f.newfacet = false;
                newfacet_list = null;
                for (Facet f = visible_list; f != null && f.visible; f = f.next) { f.replace = null; f.visible = false; }
                num_visible = 0; visible_list = null;
            }

            // ---- qh_setfacetplane (2-d: qh_sethyperplane_det, qh_normalize2) and qh_checkflipped
            void SetFacetPlane(Facet f)
            {
                int p0 = _vpoint[f.v0], p1 = _vpoint[f.v1];
                double nx = Y(p1) - Y(p0), ny = X(p0) - X(p1);
                double norm = Math.Sqrt(nx * nx + ny * ny);
                if (norm > MINdenom) { if (!f.top) norm = -norm; nx /= norm; ny /= norm; }
                else throw new HullException("qhull: two hull points coincide within roundoff (not reproduced)");
                f.nx = nx; f.ny = ny; f.off = -(X(p0) * nx + Y(p0) * ny);
                if (Dist(ix, iy, f) >= -DISTround) f.flipped = true;        // qh_checkflipped(facet, NULL, qh_ALL)
            }

            // ---- qh_initbuild
            void InitBuild()
            {
                if (_n < 3) throw new HullException("qhull: QH6214 not enough points (" + _n + ") to construct an initial simplex (need 3)");
                // qh_maxmin
                var maxpoints = new int[4];
                MAXabs = 0; MAXwidth = -RealMax; MAXsum = 0; max_outside = 0;
                for (int k = 0; k < 2; k++)
                {
                    int minimum = 0, maximum = 0;
                    for (int p = 0; p < _n; p++)
                    {
                        if (_p[2 * maximum + k] < _p[2 * p + k]) maximum = p;
                        else if (_p[2 * minimum + k] > _p[2 * p + k]) minimum = p;
                    }
                    double mx = _p[2 * maximum + k], mn = _p[2 * minimum + k];
                    double maxcoord = mx < -mn ? -mn : mx;
                    double temp = mx - mn;
                    if (MAXwidth < temp) MAXwidth = temp;
                    if (MAXabs < maxcoord) MAXabs = maxcoord;
                    MAXsum += maxcoord;
                    maxpoints[2 * k] = minimum; maxpoints[2 * k + 1] = maximum;
                    NEARzero[k] = 80 * MAXsum * Eps;
                }
                // qh_detroundoff
                {
                    double maxdistsum = Math.Sqrt(2.0) * MAXabs;
                    if (maxdistsum > MAXsum) maxdistsum = MAXsum;
                    DISTround = Eps * (2 * maxdistsum * 1.01 + MAXabs);
                }
                MINdenom = RealMin * MAXabs;                       // MINdenom_1 = fmax_(1.0 / REALmax, REALmin)
                double premerge_centrum = 2 * DISTround;
                ONEmerge = Math.Sqrt(2.0) * MAXwidth * Math.Sqrt(1.0 - 1.0 * 1.0) + DISTround;
                { double maxrho = 2 * premerge_centrum + DISTround; if (ONEmerge < maxrho) ONEmerge = maxrho; }
                MINvisible = premerge_centrum; MAXcoplanar = MINvisible; MINoutside = 2 * MINvisible;
                DetMaxOutside();
                if (MAXwidth <= 0.0) throw new HullException("qhull: the points coincide");
                // qh_maxsimplex
                int minx = -1, maxx = -1; double maxcoordx = -RealMax, mincoordx = RealMax;
                foreach (int p in maxpoints)
                {
                    if (maxcoordx < X(p)) { maxcoordx = X(p); maxx = p; }
                    if (mincoordx > X(p)) { mincoordx = X(p); minx = p; }
                }
                if (minx == maxx) throw new HullException("qhull: QH6013 input is less than 2-dimensional since all points have the same x coordinate");
                double maxdet = maxcoordx - mincoordx;
                int s0 = minx, s1 = maxx, maxpoint = -1; bool maxnearzero = false;
                {
                    double prevdet = maxdet; maxdet = -1.0;
                    foreach (int p in maxpoints)
                        if (p != s0 && p != s1 && p != maxpoint)
                        {
                            double det = Math.Abs(DetSimplex(p, s0, s1, out bool nz));
                            if (det > maxdet) { maxdet = det; maxpoint = p; maxnearzero = nz; }
                        }
                    bool falsenarrow = false;
                    double targetdet = prevdet * MAXwidth, mindet = 10 * 1.0e-3 * targetdet;
                    if (maxdet > 0.0 && maxdet / targetdet < 1.0e-3) falsenarrow = true;
                    if (maxpoint < 0 || maxnearzero || falsenarrow)
                        for (int p = 0; p < _n; p++)
                            if (Array.IndexOf(maxpoints, p) < 0 && p != s0 && p != s1)
                            {
                                double det = Math.Abs(DetSimplex(p, s0, s1, out bool nz));
                                if (det > maxdet)
                                {
                                    maxdet = det; maxpoint = p; maxnearzero = nz;
                                    if (!maxnearzero && maxdet > mindet) break;
                                }
                            }
                    if (maxpoint < 0) throw new HullException("qhull: QH6014 not enough points available");
                }
                // qh_initialvertices / qh_createsimplex: vertices v1, v2, v3 = the simplex points; the set is [v3, v2, v1]
                facet_id = 0;
                facet_list = newfacet_list = facet_tail = NewFacet();
                num_facets = num_visible = 0;
                _vpoint.Add(-1); _vpoint.Add(s0); _vpoint.Add(s1); _vpoint.Add(maxpoint);
                Facet f1 = NewFacet(), f2 = NewFacet(), f3 = NewFacet();
                f1.v0 = 2; f1.v1 = 1; f1.top = true; AppendFacet(f1);
                f2.v0 = 3; f2.v1 = 1; f2.top = false; AppendFacet(f2);
                f3.v0 = 3; f3.v1 = 2; f3.top = true; AppendFacet(f3);
                f1.n0 = f2; f1.n1 = f3; f2.n0 = f1; f2.n1 = f3; f3.n0 = f1; f3.n1 = f2;
                // qh_initialhull
                ResetLists();
                facet_next = facet_list;
                ix = ((0.0 + X(maxpoint)) + X(s1)) + X(s0); ix /= 3;
                iy = ((0.0 + Y(maxpoint)) + Y(s1)) + Y(s0); iy /= 3;
                Facet firstfacet = facet_list;
                SetFacetPlane(firstfacet); firstfacet.flipped = false;
                if (Dist(ix, iy, firstfacet) > DISTround)
                {
                    for (Facet f = facet_list; f != null && f.next != null; f = f.next) f.top = !f.top;
                    SetFacetPlane(firstfacet);
                }
                for (Facet f = facet_list; f != null && f.next != null; f = f.next) if (f != firstfacet) SetFacetPlane(f);
                for (Facet f = facet_list; f != null && f.next != null; f = f.next)
                {
                    f.flipped = false;
                    if (Dist(ix, iy, f) > DISTround) throw new HullException("qhull: the initial simplex needs reorientation (not reproduced)");
                }
                double minangle = RealMax;
                for (Facet f = facet_list; f != null && f.next != null; f = f.next)
                {
                    if (Dist(ix, iy, f) >= -DISTround) throw new HullException("qhull: QH6154 initial simplex is flat");
                    foreach (Facet nb in new[] { f.n0, f.n1 }) { double angle = (0 + f.nx * nb.nx) + f.ny * nb.ny; if (minangle > angle) minangle = angle; }
                }
                if (minangle < -0.99999999) throw new HullException("qhull: narrow initial simplex (not reproduced)");
                // qh_partitionall
                var pointset = new int[_n]; int point_n = _n;
                for (int i = 0; i < _n; i++) pointset[i] = i;
                pointset[s0] = -1; pointset[s1] = -1; pointset[maxpoint] = -1;
                num_outside = 0;
                double distoutside = DistOutside(), bestdist = -RealMax;
                for (Facet f = facet_list; f != null && f.next != null; f = f.next)
                {
                    f.outside = new List<int>(); int bestpoint = -1, point_end = 0;
                    for (int i = 0; i < point_n; i++)
                    {
                        int point = pointset[i];
                        if (point < 0) continue;
                        double dist = Dist(point, f);
                        if (dist < distoutside) pointset[point_end++] = point;
                        else
                        {
                            num_outside++;
                            if (bestpoint < 0) { bestpoint = point; bestdist = dist; }
                            else if (dist > bestdist) { f.outside.Add(bestpoint); bestpoint = point; bestdist = dist; }
                            else f.outside.Add(point);
                        }
                    }
                    if (bestpoint >= 0) { f.outside.Add(bestpoint); f.furthestdist = bestdist; }
                    else f.outside = null;
                    point_n = point_end;
                }
                findbestnew = true;
                for (int i = 0; i < point_n; i++) if (pointset[i] >= 0) PartitionPoint(pointset[i], facet_list);
                findbestnew = false;
                ResetLists();
                facet_next = facet_list;
                // qh_furthestnext
                {
                    Facet bestfacet = null; double bd = -RealMax;
                    for (Facet f = facet_list; f != null && f.next != null; f = f.next)
                        if (f.outside != null && f.furthestdist > bd) { bestfacet = f; bd = f.furthestdist; }
                    if (bestfacet != null) { RemoveFacet(bestfacet); PrependFacetAtNext(bestfacet); }
                }
            }
            double DetSimplex(int apex, int a, int b, out bool nearzero)
            {
                double r00 = X(a) - X(apex), r01 = Y(a) - Y(apex), r10 = X(b) - X(apex), r11 = Y(b) - Y(apex);
                double det = r00 * r11 - r01 * r10;
                nearzero = Math.Abs(det) < 10 * NEARzero[1];
                return det;
            }
            void DetMaxOutside()
            {
                double m = max_outside < ONEmerge + DISTround ? ONEmerge + DISTround : max_outside;
                if (m < MINoutside) m = MINoutside;
                MAXoutside = m;
            }
            double DistOutside() { double a = 2 * MINoutside; return a < max_outside ? max_outside : a; }                   // qh_DISToutside
            double SearchDist() => max_outside + 2 * DISTround + (MINvisible < MAXcoplanar ? MAXcoplanar : MINvisible);       // qh_SEARCHdist

            // ---- qh_buildhull / qh_addpoint
            void BuildHull()
            {
                facet_next = facet_list;
                while (true)
                {
                    // qh_nextfurthest
                    Facet facet = null; int furthest = -1;
                    while ((facet = facet_next) != facet_tail)
                    {
                        if (facet.outside == null) { facet_next = facet.next; continue; }
                        if (facet.outside.Count == 0) { facet.outside = null; facet_next = facet.next; continue; }
                        furthest = facet.outside[facet.outside.Count - 1]; facet.outside.RemoveAt(facet.outside.Count - 1);
                        break;
                    }
                    if (furthest < 0) break;
                    num_outside--;
                    AddPoint(furthest, facet);
                }
            }
            void AddPoint(int furthest, Facet facet)
            {
                DetMaxOutside();
                // qh_findhorizon
                RemoveFacet(facet); AppendFacet(facet);
                num_visible = 1; visible_list = facet; facet.visible = true; facet.replace = null;
                visit_id++;
                int numhorizon = 0;
                for (Facet visible = visible_list; visible != null && visible.visible; visible = visible.next)
                {
                    visible.visitid = visit_id;
                    for (int i = 0; i < 2; i++)
                    {
                        Facet neighbor = i == 0 ? visible.n0 : visible.n1;
                        if (neighbor == null) break;
                        if (neighbor.visitid == visit_id) continue;
                        neighbor.visitid = visit_id;
                        double dist = Dist(furthest, neighbor);
                        if (dist > MINvisible)
                        {
                            RemoveFacet(neighbor); AppendFacet(neighbor);
                            neighbor.visible = true; neighbor.replace = null; num_visible++;
                        }
                        else
                        {
                            if (dist >= -MAXcoplanar) throw new HullException("qhull: coplanar horizon (a point within roundoff of an edge's line; merging is not reproduced)");
                            numhorizon++;
                        }
                    }
                }
                if (numhorizon == 0) throw new HullException("qhull: QH6168 empty horizon");
                // qh_makenewfacets (simplicial)
                newfacet_list = facet_tail;
                int apex = _vpoint.Count; _vpoint.Add(furthest);
                visit_id++;
                for (Facet visible = visible_list; visible != null && visible.visible; visible = visible.next)
                {
                    Facet made = null;
                    for (int i = 0; i < 2; i++)
                    {
                        Facet neighbor = i == 0 ? visible.n0 : visible.n1;
                        if (neighbor == null) break;
                        if (neighbor.visible) continue;
                        int horizonskip = neighbor.n0 == visible ? 0 : neighbor.n1 == visible ? 1 : -1;
                        if (horizonskip < 0) throw new HullException("qhull: QH6104 facets are not neighbors");
                        Facet nf = NewFacet();
                        nf.v0 = apex; nf.v1 = horizonskip == 0 ? neighbor.v1 : neighbor.v0;
                        nf.top = neighbor.top ? (horizonskip & 1) != 0 : ((horizonskip & 1) ^ 1) != 0;
                        nf.n0 = neighbor; nf.n1 = null;
                        AppendFacet(nf);
                        if (horizonskip == 0) neighbor.n0 = nf; else neighbor.n1 = nf;
                        made = nf;
                    }
                    if (made != null) visible.replace = made;
                    visible.n0 = null;                                   // SETfirst_(visible->neighbors) = NULL
                }
                // qh_matchnewfacets: in 2-d the two new facets share the apex
                Facet a = newfacet_list, b = a != null && a.next != null ? a.next : null;
                if (a == null || a == facet_tail || b == null || b == facet_tail || b.next != facet_tail) throw new HullException("qhull: a cone of other than two facets");
                if (a.top == b.top) throw new HullException("qhull: duplicate ridge");
                a.n1 = b; b.n1 = a;
                // qh_makenewplanes
                SetFacetPlane(a); SetFacetPlane(b);
                // qh_premerge: qh_checkzero(!qh_ALL); anything else would merge
                foreach (Facet nf in new[] { a, b })
                {
                    if (nf.flipped) throw new HullException("qhull: flipped new facet (merging is not reproduced)");
                    Facet other = nf.n1, horizon = nf.n0;
                    if (Dist(_vpoint[nf.v1], other) >= -2 * DISTround) throw new HullException("qhull: new facets are not clearly convex (merging is not reproduced)");
                    int hv = horizon.v0 == nf.v1 ? horizon.v1 : horizon.v0;
                    if (Dist(_vpoint[hv], nf) >= -2 * DISTround) throw new HullException("qhull: a new facet is not clearly convex with its horizon (merging is not reproduced)");
                }
                findbestnew = false;
                // qh_partitionvisible
                for (Facet visible = visible_list; visible != null && visible.visible; visible = visible.next)
                {
                    if (visible.outside == null) continue;
                    Facet newfacet = visible;
                    while (newfacet != null && newfacet.visible) newfacet = newfacet.replace;       // qh_getreplacement
                    if (newfacet == null) newfacet = newfacet_list;
                    num_outside -= visible.outside.Count;
                    foreach (int point in visible.outside) PartitionPoint(point, newfacet);
                }
                findbestnew = false; findbest_notsharp = false;
                // qh_deletevisible
                for (Facet visible = visible_list; visible != null && visible.visible;)
                {
                    Facet next = visible.next; RemoveFacet(visible); visible = next;
                }
                num_visible = 0;
                ResetLists();
            }

            // ---- qh_partitionpoint
            void PartitionPoint(int point, Facet facet)
            {
                Facet bestfacet; double bestdist; bool isoutside;
                if (findbestnew) bestfacet = FindBestNew(point, facet, out bestdist, out isoutside);
                else bestfacet = FindBest(point, facet, out bestdist, out isoutside);
                if (bestfacet.visible) throw new HullException("qhull: QH6293 cannot partition a point into a visible facet");
                if (isoutside)
                {
                    bool isnewoutside = false;
                    if (bestfacet.outside == null || bestfacet.outside.Count == 0)
                    {
                        if (bestfacet.outside == null) bestfacet.outside = new List<int>();
                        bestfacet.outside.Add(point); isnewoutside = true; bestfacet.furthestdist = bestdist;
                    }
                    else if (bestfacet.furthestdist < bestdist) { bestfacet.outside.Add(point); bestfacet.furthestdist = bestdist; }
                    else bestfacet.outside.Insert(bestfacet.outside.Count - 1, point);
                    if (isnewoutside && facet_next != bestfacet)
                    {
                        if (bestfacet.newfacet) { if (facet_next.newfacet) facet_next = newfacet_list; }
                        else
                        {
                            RemoveFacet(bestfacet); AppendFacet(bestfacet);
                            if (newfacet_list != null) bestfacet.newfacet = true;
                        }
                    }
                    num_outside++;
                }
                else if (bestdist >= -MAXcoplanar)
                {   // qh_partitioncoplanar (KEEPnearinside): only qh.max_outside matters here
                    if (bestdist > max_outside)
                    {
                        if (bestdist > MAXoutside * 30) throw new HullException("qhull: a coplanar point is repartitioned as an outside point (not reproduced)");
                        max_outside = bestdist;
                    }
                }
            }

            // ---- qh_findbest(point, startfacet, !bestoutside, qh_ISnewfacets, !qh_NOupper)
            Facet FindBest(int point, Facet startfacet, out double dist, out bool isoutside)
            {
                double bestdist = -RealMax / 2; Facet bestfacet = null;
                uint visitid = ++visit_id; bool testhorizon = true;
                isoutside = true;
                if (!startfacet.flipped)
                {
                    dist = Dist(point, startfacet);
                    if (dist >= MINoutside) return startfacet;
                    bestdist = dist; bestfacet = startfacet;
                }
                dist = bestdist;
                startfacet.visitid = visitid;
                Facet facet = startfacet;
                while (facet != null)
                {
                    Facet switched = null;
                    for (int i = 0; i < 2; i++)
                    {
                        Facet neighbor = i == 0 ? facet.n0 : facet.n1;
                        if (neighbor == null) break;
                        if (!neighbor.newfacet) continue;
                        if (neighbor.visitid == visitid) continue;
                        neighbor.visitid = visitid;
                        if (!neighbor.flipped)
                        {
                            dist = Dist(point, neighbor);
                            if (dist > bestdist)
                            {
                                if (dist >= MINoutside) return neighbor;
                                bestfacet = neighbor; bestdist = dist; switched = neighbor;
                                break;
                            }
                        }
                    }
                    facet = switched;
                }
                if (bestfacet == null)
                {
                    bestfacet = FindBestNew(point, newfacet_list, out bestdist, out isoutside);
                    testhorizon = false;
                }
                else if (!findbest_notsharp && bestdist < -DISTround)
                {
                    if (SharpNewFacets())
                    {
                        bestfacet = FindBestNew(point, bestfacet, out bestdist, out isoutside);
                        testhorizon = false; findbestnew = true;
                    }
                    else findbest_notsharp = true;
                }
                if (testhorizon) bestfacet = FindBestHorizon(point, bestfacet, ref bestdist);
                dist = bestdist;
                if (bestdist < MINoutside) isoutside = false;
                return bestfacet;
            }
            bool SharpNewFacets()
            {
                bool q0 = false, q1 = false;
                for (Facet f = newfacet_list; f != null && f.next != null; f = f.next)
                {
                    if (f == newfacet_list) { q0 = f.nx > 0; q1 = f.ny > 0; }
                    else if (q1 != (f.ny > 0) || q0 != (f.nx > 0)) return true;
                }
                return false;
            }

            // ---- qh_findbestnew(point, startfacet, !bestoutside)
            Facet FindBestNew(int point, Facet startfacet, out double dist, out bool isoutside)
            {
                if (startfacet == null || startfacet.next == null) throw new HullException("qhull: QH6002 no new facets for a point");
                double bestdist = -RealMax / 2, distoutside = DistOutside(); Facet bestfacet = null;
                uint visitid = ++visit_id;
                isoutside = true;
                Facet facet = startfacet;
                for (int i = 0; i < 2; i++, facet = newfacet_list)
                    for (Facet f = facet; f != null && f.next != null; f = f.next)
                    {
                        if (f == startfacet && i != 0) break;
                        f.visitid = visitid;
                        if (!f.flipped)
                        {
                            dist = Dist(point, f);
                            if (dist > bestdist)
                            {
                                bestfacet = f;
                                if (dist >= distoutside) return bestfacet;
                                bestdist = dist;
                            }
                        }
                    }
                bestfacet = FindBestHorizon(point, bestfacet ?? startfacet, ref bestdist);
                dist = bestdist;
                if (dist < MINoutside) isoutside = false;
                return bestfacet;
            }

            // ---- qh_findbesthorizon(!qh_IScheckmax, point, startfacet, noupper, &bestdist)
            Facet FindBestHorizon(int point, Facet startfacet, ref double bestdist)
            {
                Facet bestfacet = startfacet, nextfacet = null;
                uint visitid = ++visit_id;
                double searchdist = SearchDist(), minsearch = bestdist - searchdist;
                var coplanarfacetset = new List<Facet>(); int coplanarfacetset_size = 0;
                startfacet.visitid = visitid;
                Facet facet = startfacet;
                while (true)
                {
                    for (int i = 0; i < 2; i++)
                    {
                        Facet neighbor = i == 0 ? facet.n0 : facet.n1;
                        if (neighbor == null) break;
                        if (neighbor.visitid == visitid) continue;
                        neighbor.visitid = visitid;
                        if (!neighbor.flipped)
                        {
                            double dist = Dist(point, neighbor);
                            if (dist > bestdist)
                            {
                                minsearch = dist - searchdist;
                                if (dist > bestdist + searchdist) coplanarfacetset_size = 0;
                                bestfacet = neighbor; bestdist = dist;
                            }
                            else if (dist < minsearch) continue;
                        }
                        if (nextfacet != null)
                        {
                            if (coplanarfacetset_size++ == 0) { coplanarfacetset.Clear(); coplanarfacetset.Add(nextfacet); }
                            else coplanarfacetset.Add(nextfacet);
                        }
                        nextfacet = neighbor;
                    }
                    facet = nextfacet;
                    if (facet != null) nextfacet = null;
                    else if (coplanarfacetset_size == 0) break;
                    else if (--coplanarfacetset_size == 0) { facet = coplanarfacetset[0]; coplanarfacetset.Clear(); }
                    else { facet = coplanarfacetset[coplanarfacetset.Count - 1]; coplanarfacetset.RemoveAt(coplanarfacetset.Count - 1); }
                }
                return bestfacet;
            }

            // ---- scipy _Qhull.get_extremes_2d
            int[] Extremes()
            {
                var o = new List<int>(); var seenV = new HashSet<int>(); var seenF = new HashSet<Facet>();
                Facet facet = facet_list, startfacet = facet;
                while (facet != null && facet != facet_tail)
                {
                    if (seenF.Contains(facet)) throw new HullException("Qhull internal error: loop in facet list");
                    int va, vb; Facet next;
                    if (facet.top) { va = facet.v0; vb = facet.v1; next = facet.n0; } else { vb = facet.v0; va = facet.v1; next = facet.n1; }
                    if (seenV.Add(va)) o.Add(_vpoint[va]);
                    if (seenV.Add(vb)) o.Add(_vpoint[vb]);
                    seenF.Add(facet);
                    facet = next;
                    if (facet == startfacet) break;
                }
                return o.ToArray();
            }
        }

        /// _hull_planes(V): the outward unit normals N (k, 3) and offsets D (k) of the convex hull's facets of the points (n, 3)
        /// (ConvexHull(V).equations: n . x <= d inside), one plane per facet. Exact orientation tests decide what a facet is, after the
        /// points qhull would not make vertices are set aside: a point that lies outside the hull of the others by no more than
        /// qhull's "clearly outside" distance (qh.MINoutside = 4 qh.DISTround, about 4e-14 ft here) is coplanar for qhull. Without
        /// that rule a midpoint of an edge that float32 rounding left 1e-14 ft proud adds a sliver facet with a normal of its own
        /// (TO_Sign3F). qhull's merged planes are reproduced to about 1e-13, not bit for bit (they feed rounded values only).
        internal static void HullPlanes(double[] pts, out double[] N, out double[] D)
        {
            // distinct points
            int n0 = pts.Length / 3; var uniq = new List<int>(); var seen = new HashSet<(long, long, long)>();
            for (int i = 0; i < n0; i++)
                if (seen.Add((BitConverter.DoubleToInt64Bits(pts[3 * i] + 0.0), BitConverter.DoubleToInt64Bits(pts[3 * i + 1] + 0.0), BitConverter.DoubleToInt64Bits(pts[3 * i + 2] + 0.0)))) uniq.Add(i);
            int n = uniq.Count;
            if (n < 4) throw new HullException("qhull: QH6214 not enough points (" + n + ") to construct an initial simplex (need 4)");
            var P = new double[3 * n];
            for (int i = 0; i < n; i++) for (int c = 0; c < 3; c++) P[3 * i + c] = pts[3 * uniq[i] + c] + 0.0;
            // exact integer coordinates (a common power-of-two scale)
            int emin = int.MaxValue;
            for (int i = 0; i < 3 * n; i++)
            {
                if (P[i] == 0) continue;
                long bits = BitConverter.DoubleToInt64Bits(P[i]); int be = (int)((bits >> 52) & 0x7FF);
                int e = be == 0 ? -1074 : be - 1075; if (e < emin) emin = e;
            }
            var B = new BigInteger[3 * n];
            for (int i = 0; i < 3 * n; i++)
            {
                if (P[i] == 0) { B[i] = BigInteger.Zero; continue; }
                long bits = BitConverter.DoubleToInt64Bits(P[i]); int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
                long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;
                B[i] = (new BigInteger(bits < 0 ? -m : m)) << (e - emin);
            }
            // sign of ((b - a) x (c - a)) . (d - a)
            int Orient(int a, int b, int c, int d)
            {
                double ux = P[3 * b] - P[3 * a], uy = P[3 * b + 1] - P[3 * a + 1], uz = P[3 * b + 2] - P[3 * a + 2];
                double vx = P[3 * c] - P[3 * a], vy = P[3 * c + 1] - P[3 * a + 1], vz = P[3 * c + 2] - P[3 * a + 2];
                double wx = P[3 * d] - P[3 * a], wy = P[3 * d + 1] - P[3 * a + 1], wz = P[3 * d + 2] - P[3 * a + 2];
                double t1 = uy * vz, t2 = uz * vy, t3 = uz * vx, t4 = ux * vz, t5 = ux * vy, t6 = uy * vx;
                double det = (t1 - t2) * wx + (t3 - t4) * wy + (t5 - t6) * wz;
                double perm = (Math.Abs(t1) + Math.Abs(t2)) * Math.Abs(wx) + (Math.Abs(t3) + Math.Abs(t4)) * Math.Abs(wy) + (Math.Abs(t5) + Math.Abs(t6)) * Math.Abs(wz);
                if (Math.Abs(det) > 1e-14 * perm) return det > 0 ? 1 : -1;
                BigInteger Ux = B[3 * b] - B[3 * a], Uy = B[3 * b + 1] - B[3 * a + 1], Uz = B[3 * b + 2] - B[3 * a + 2];
                BigInteger Vx = B[3 * c] - B[3 * a], Vy = B[3 * c + 1] - B[3 * a + 1], Vz = B[3 * c + 2] - B[3 * a + 2];
                BigInteger Wx = B[3 * d] - B[3 * a], Wy = B[3 * d + 1] - B[3 * a + 1], Wz = B[3 * d + 2] - B[3 * a + 2];
                return ((Uy * Vz - Uz * Vy) * Wx + (Uz * Vx - Ux * Vz) * Wy + (Ux * Vy - Uy * Vx) * Wz).Sign;
            }
            bool Collinear(int a, int b, int c)
            {
                double ux = P[3 * b] - P[3 * a], uy = P[3 * b + 1] - P[3 * a + 1], uz = P[3 * b + 2] - P[3 * a + 2];
                double vx = P[3 * c] - P[3 * a], vy = P[3 * c + 1] - P[3 * a + 1], vz = P[3 * c + 2] - P[3 * a + 2];
                double c0 = uy * vz - uz * vy, c1 = uz * vx - ux * vz, c2 = ux * vy - uy * vx;
                double scale = (ux * ux + uy * uy + uz * uz) * (vx * vx + vy * vy + vz * vz);
                if (c0 * c0 + c1 * c1 + c2 * c2 > 1e-20 * scale) return false;
                BigInteger Ux = B[3 * b] - B[3 * a], Uy = B[3 * b + 1] - B[3 * a + 1], Uz = B[3 * b + 2] - B[3 * a + 2];
                BigInteger Vx = B[3 * c] - B[3 * a], Vy = B[3 * c + 1] - B[3 * a + 1], Vz = B[3 * c + 2] - B[3 * a + 2];
                return (Uy * Vz - Uz * Vy).IsZero && (Uz * Vx - Ux * Vz).IsZero && (Ux * Vy - Uy * Vx).IsZero;
            }
            // qh_detroundoff for the set: DISTround, and MINoutside = 2 MINvisible = 4 DISTround
            double tol;
            {
                double maxabs = 0, maxsum = 0;
                for (int c = 0; c < 3; c++)
                {
                    double mx = P[c], mn = P[c];
                    for (int i = 1; i < n; i++) { if (mx < P[3 * i + c]) mx = P[3 * i + c]; if (mn > P[3 * i + c]) mn = P[3 * i + c]; }
                    double maxcoord = mx < -mn ? -mn : mx;
                    if (maxabs < maxcoord) maxabs = maxcoord;
                    maxsum += maxcoord;
                }
                double maxdistsum = Math.Sqrt(3.0) * maxabs; if (maxdistsum > maxsum) maxdistsum = maxsum;
                tol = 4 * (2.220446049250313e-16 * (3 * maxdistsum * 1.01 + maxabs));
            }
            var active = new bool[n]; for (int i = 0; i < n; i++) active[i] = true;
            double scale3 = BitConverter.Int64BitsToDouble((long)(1023 + Math.Max(-1022, 3 * emin)) << 52);        // 2^(3 emin)
            // the distance of point d from the plane of (a, b, c): |exact determinant| / |cross|
            double PlaneDistance(int a, int b, int c, int d)
            {
                BigInteger Ux = B[3 * b] - B[3 * a], Uy = B[3 * b + 1] - B[3 * a + 1], Uz = B[3 * b + 2] - B[3 * a + 2];
                BigInteger Vx = B[3 * c] - B[3 * a], Vy = B[3 * c + 1] - B[3 * a + 1], Vz = B[3 * c + 2] - B[3 * a + 2];
                BigInteger Wx = B[3 * d] - B[3 * a], Wy = B[3 * d + 1] - B[3 * a + 1], Wz = B[3 * d + 2] - B[3 * a + 2];
                double det = (double)BigInteger.Abs((Uy * Vz - Uz * Vy) * Wx + (Uz * Vx - Ux * Vz) * Wy + (Ux * Vy - Uy * Vx) * Wz) * scale3;
                double ux = P[3 * b] - P[3 * a], uy = P[3 * b + 1] - P[3 * a + 1], uz = P[3 * b + 2] - P[3 * a + 2];
                double vx = P[3 * c] - P[3 * a], vy = P[3 * c + 1] - P[3 * a + 1], vz = P[3 * c + 2] - P[3 * a + 2];
                double c0 = uy * vz - uz * vy, c1 = uz * vx - ux * vz, c2 = ux * vy - uy * vx;
                return det / Math.Sqrt((c0 * c0 + c1 * c1) + c2 * c2);
            }
            // set aside, one at a time (the least proud first), the points that are outside the hull of the others by no more than tol
            while (true)
            {
                var cand = new bool[n];
                for (int i = 0; i < n; i++)
                {
                    if (!active[i]) continue;
                    for (int j = i + 1; j < n; j++)
                    {
                        if (!active[j]) continue;
                        for (int k = j + 1; k < n; k++)
                        {
                            if (!active[k] || (cand[i] && cand[j] && cand[k]) || Collinear(i, j, k)) continue;
                            int side = 0; bool one = true;
                            for (int m = 0; m < n && one; m++)
                            {
                                if (!active[m] || m == i || m == j || m == k) continue;
                                int sg = Orient(i, j, k, m);
                                if (sg == 0) continue;
                                if (side == 0) side = sg; else if (sg != side) one = false;
                            }
                            if (one && side != 0) { cand[i] = true; cand[j] = true; cand[k] = true; }
                        }
                    }
                }
                int weakest = -1; double weakestDist = double.PositiveInfinity;
                for (int v = 0; v < n; v++)
                {
                    if (!cand[v]) continue;
                    double maxd = 0; bool outside = false, strong = false;
                    for (int i = 0; i < n && !strong; i++)
                    {
                        if (!active[i] || i == v) continue;
                        for (int j = i + 1; j < n && !strong; j++)
                        {
                            if (!active[j] || j == v) continue;
                            for (int k = j + 1; k < n && !strong; k++)
                            {
                                if (!active[k] || k == v || Collinear(i, j, k)) continue;
                                int sv = Orient(i, j, k, v);
                                if (sv == 0) continue;
                                bool facet = true, any = false;
                                for (int m = 0; m < n && facet; m++)
                                {
                                    if (!active[m] || m == i || m == j || m == k || m == v) continue;
                                    int sg = Orient(i, j, k, m);
                                    if (sg == sv) facet = false; else if (sg != 0) any = true;
                                }
                                if (!facet) continue;
                                if (!any) { strong = true; break; }               // the other points are coplanar: v gives the set its volume
                                double dist = PlaneDistance(i, j, k, v);
                                outside = true;
                                if (dist > maxd) maxd = dist;
                                if (maxd > tol) strong = true;
                            }
                        }
                    }
                    if (!strong && outside && maxd < weakestDist) { weakest = v; weakestDist = maxd; }
                }
                if (weakest < 0) break;
                active[weakest] = false;
            }
            var normals = new List<double>(); var offsets = new List<double>();
            var copl = new List<int>();
            for (int i = 0; i < n; i++)
            {
                if (!active[i]) continue;
                for (int j = i + 1; j < n; j++)
                {
                    if (!active[j]) continue;
                    for (int k = j + 1; k < n; k++)
                    {
                        if (!active[k] || Collinear(i, j, k)) continue;
                        bool pos = false, neg = false; int other = -1; copl.Clear(); bool first = true;
                        for (int m = 0; m < n && !(pos && neg); m++)
                        {
                            if (!active[m] || m == i || m == j || m == k) continue;
                            int s = Orient(i, j, k, m);
                            if (s > 0) { pos = true; if (other < 0) other = m; }
                            else if (s < 0) { neg = true; if (other < 0) other = m; }
                            else
                            {
                                // the representative triple of a facet is its two lowest points and the first point not on their line
                                if (m < j) { first = false; break; }
                                if (m < k && !Collinear(i, j, m)) { first = false; break; }
                                copl.Add(m);
                            }
                        }
                        if (!first || pos && neg) continue;
                        if (!pos && !neg) throw new HullException("qhull: QH6154 initial simplex is flat (the points are coplanar)");
                        // the best conditioned triple of the facet's points gives the plane
                        copl.Add(i); copl.Add(j); copl.Add(k);
                        int ba = i, bb = j, bc = k; double bestArea = -1;
                        for (int x = 0; x < copl.Count; x++) for (int y = x + 1; y < copl.Count; y++) for (int z = y + 1; z < copl.Count; z++)
                        {
                            int a = copl[x], b = copl[y], c = copl[z];
                            double ux = P[3 * b] - P[3 * a], uy = P[3 * b + 1] - P[3 * a + 1], uz = P[3 * b + 2] - P[3 * a + 2];
                            double vx = P[3 * c] - P[3 * a], vy = P[3 * c + 1] - P[3 * a + 1], vz = P[3 * c + 2] - P[3 * a + 2];
                            double c0 = uy * vz - uz * vy, c1 = uz * vx - ux * vz, c2 = ux * vy - uy * vx, ar = c0 * c0 + c1 * c1 + c2 * c2;
                            if (ar > bestArea) { bestArea = ar; ba = a; bb = b; bc = c; }
                        }
                        {
                            double ux = P[3 * bb] - P[3 * ba], uy = P[3 * bb + 1] - P[3 * ba + 1], uz = P[3 * bb + 2] - P[3 * ba + 2];
                            double vx = P[3 * bc] - P[3 * ba], vy = P[3 * bc + 1] - P[3 * ba + 1], vz = P[3 * bc + 2] - P[3 * ba + 2];
                            double c0 = uy * vz - uz * vy, c1 = uz * vx - ux * vz, c2 = ux * vy - uy * vx;
                            int s = Orient(ba, bb, bc, other);                 // the other points are on the inner side
                            double len = Math.Sqrt((c0 * c0 + c1 * c1) + c2 * c2);
                            if (s > 0) len = -len;
                            c0 /= len; c1 /= len; c2 /= len;
                            normals.Add(c0); normals.Add(c1); normals.Add(c2);
                            offsets.Add((P[3 * ba] * c0 + P[3 * ba + 1] * c1) + P[3 * ba + 2] * c2);
                        }
                    }
                }
            }
            N = normals.ToArray(); D = offsets.ToArray();
        }

        // ====================================================================================== box fit (kind 0 -> 15)
        sealed class FitException : Exception { public FitException(string m) : base(m) { } }           // ValueError of fit_prism_boxes

        static double Dot2(double ax, double ay, double bx, double by) => UxNp.Fma(ay, by, ax * bx);         // a @ b of 2-vectors (ddot)
        static double Dot3(double[] a, int ao, double[] b, int bo) => UxNp.Fma(a[ao + 2], b[bo + 2], UxNp.Fma(a[ao + 1], b[bo + 1], a[ao] * b[bo]));
        /// (m, 3) @ vec3, one row (dgemv): the chain 1, 0, 2 with m >= 2 rows, 0, 1, 2 with one.
        static double MatVec3Row(double[] A, int ro, double[] x, bool manyRows)
            => manyRows ? UxNp.Fma(A[ro + 2], x[2], UxNp.Fma(A[ro], x[0], A[ro + 1] * x[1])) : UxNp.Fma(A[ro + 2], x[2], UxNp.Fma(A[ro + 1], x[1], A[ro] * x[0]));
        /// (m, 2) @ vec2, one row (dgemv): the chain 1, 0 with m >= 2 rows, 0, 1 with one (measured on the reference machine).
        static double MatVec2Row(double ax, double ay, double[] x, bool manyRows) => manyRows ? UxNp.Fma(ax, x[0], ay * x[1]) : UxNp.Fma(ay, x[1], ax * x[0]);

        /// _halfplanes(V): the inward unit normals M of a CCW convex polygon's edges (the points are V).
        static double[][] Halfplanes(double[][] V)
        {
            int n = V.Length; var M = new double[n][];
            double cx = 0, cy = 0;
            for (int i = 0; i < n; i++)
            {
                double ex = V[(i + 1) % n][0] - V[i][0], ey = V[(i + 1) % n][1] - V[i][1];
                double mx = -ey, my = ex, len = Math.Sqrt(mx * mx + my * my);
                M[i] = new[] { mx / len, my / len };
            }
            cx = V[0][0]; cy = V[0][1];
            for (int i = 1; i < n; i++) { cx += V[i][0]; cy += V[i][1]; }
            cx /= n; cy /= n;
            double min = double.PositiveInfinity; bool nan = false;
            for (int i = 0; i < n; i++)
            {
                double s = (cx - V[i][0]) * M[i][0] + (cy - V[i][1]) * M[i][1];
                if (double.IsNaN(s)) nan = true;
                if (s < min) min = s;
            }
            if (!nan && min < 0) for (int i = 0; i < n; i++) { M[i][0] = -M[i][0]; M[i][1] = -M[i][1]; }
            return M;
        }

        /// _hull2d(P): the CCW convex hull with collinear (under 0.5 degree turn) and duplicate vertices removed.
        static double[][] Hull2d(double[][] P)
        {
            var flat = new double[2 * P.Length];
            for (int i = 0; i < P.Length; i++) { flat[2 * i] = P[i][0]; flat[2 * i + 1] = P[i][1]; }
            int[] idx = Qhull2.Vertices(flat);
            var V = new double[idx.Length][];
            for (int i = 0; i < idx.Length; i++) V[i] = new[] { P[idx[i]][0], P[idx[i]][1] };
            double limit = 0.5 * (Math.PI / 180.0);
            int rounds = V.Length;
            for (int r = 0; r < rounds; r++)
            {
                int n = V.Length; var keep = new List<int>(n);
                for (int i = 0; i < n; i++)
                {
                    double[] a = V[(i + n - 1) % n], b = V[i], c = V[(i + 1) % n];
                    double e1x = b[0] - a[0], e1y = b[1] - a[1], e2x = c[0] - b[0], e2y = c[1] - b[1];
                    if (Math.Sqrt(Dot2(e1x, e1y, e1x, e1y)) < 1e-3) continue;
                    double ang = Math.Abs(Math.Atan2(e1x * e2y - e1y * e2x, Dot2(e1x, e1y, e2x, e2y)));
                    if (ang > limit) keep.Add(i);
                }
                if (keep.Count == n) break;
                var W = new double[keep.Count][];
                for (int i = 0; i < keep.Count; i++) W[i] = V[keep[i]];
                V = W;
                if (V.Length == 0) break;
            }
            return V;
        }

        /// _clip_convex(P, Q): the intersection of two CCW convex polygons (Sutherland-Hodgman); null if empty.
        static double[][] ClipConvex(double[][] P, double[][] Q)
        {
            var M = Halfplanes(Q);
            var o = new List<double[]>(P);
            for (int j = 0; j < Q.Length; j++)
            {
                if (o.Count == 0) return null;
                var inp = o; o = new List<double[]>();
                double[] m = M[j], b = Q[j];
                for (int i = 0; i < inp.Count; i++)
                {
                    double[] a = inp[(i + inp.Count - 1) % inp.Count], c = inp[i];
                    double da = Dot2(m[0], m[1], a[0] - b[0], a[1] - b[1]), dc = Dot2(m[0], m[1], c[0] - b[0], c[1] - b[1]);
                    if (dc >= -1e-9)
                    {
                        if (da < -1e-9) { double t = da / (da - dc); o.Add(new[] { a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t }); }
                        o.Add(c);
                    }
                    else if (da >= -1e-9) { double t = da / (da - dc); o.Add(new[] { a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t }); }
                }
            }
            return o.Count >= 3 ? o.ToArray() : null;
        }

        /// _line_interval(M, P, o, t): the parameter interval [lo, hi] of o + a t inside the polygon.
        static void LineInterval(double[][] M, double[][] P, double ox, double oy, double tx, double ty, out double lo, out double hi)
        {
            lo = double.NegativeInfinity; hi = double.PositiveInfinity;
            for (int j = 0; j < M.Length; j++)
            {
                double k = Dot2(M[j][0], M[j][1], tx, ty), c = Dot2(M[j][0], M[j][1], ox - P[j][0], oy - P[j][1]);
                if (Math.Abs(k) < 1e-12)
                {
                    if (c < -1e-9) { lo = 1.0; hi = 0.0; return; }
                    continue;
                }
                double r = -c / k;
                if (k > 0) { if (r > lo) lo = r; } else { if (r < hi) hi = r; }
            }
        }

        sealed class Box2 { public double cx, cy, tx, ty, mx, my, ht, hm; }

        /// _boxes2d(V, shell, k, i0): k slabs stacked along the inward normal of edge i0, plus an inscribed box along every polygon edge
        /// that is not inside a slab.
        static List<Box2> Boxes2d(double[][] V, double shell, int k, int i0)
        {
            var M = Halfplanes(V); int n = V.Length;
            var E = new double[n][]; var L = new double[n];
            for (int i = 0; i < n; i++)
            {
                E[i] = new[] { V[(i + 1) % n][0] - V[i][0], V[(i + 1) % n][1] - V[i][1] };
                L[i] = Math.Sqrt(E[i][0] * E[i][0] + E[i][1] * E[i][1]);
            }
            double t0x = E[i0][0] / L[i0], t0y = E[i0][1] / L[i0]; double[] m0 = M[i0];
            double depth = double.NegativeInfinity;
            for (int i = 0; i < n; i++) { double d = MatVec2Row(V[i][0] - V[i0][0], V[i][1] - V[i0][1], m0, n >= 2); if (d > depth) depth = d; }
            // np.linspace(0, depth, k + 1)
            var ys = new double[k + 1]; double step = depth / k;
            for (int j = 0; j <= k; j++) ys[j] = step == 0 ? (double)j / k * depth : j * step;
            for (int j = 0; j <= k; j++) ys[j] += 0.0;
            ys[k] = depth;
            var ivlo = new double[k + 1]; var ivhi = new double[k + 1];
            for (int j = 0; j <= k; j++) LineInterval(M, V, V[i0][0] + ys[j] * m0[0], V[i0][1] + ys[j] * m0[1], t0x, t0y, out ivlo[j], out ivhi[j]);
            var boxes = new List<Box2>(); var slabs = new List<Box2>();
            for (int j = 0; j < k; j++)
            {
                double lo = ivlo[j + 1] > ivlo[j] ? ivlo[j + 1] : ivlo[j], hi = ivhi[j + 1] < ivhi[j] ? ivhi[j + 1] : ivhi[j];     // max / min
                if (hi - lo <= 1e-3) continue;
                double cx = V[i0][0] + t0x * (lo + hi) / 2 + m0[0] * (ys[j] + ys[j + 1]) / 2, cy = V[i0][1] + t0y * (lo + hi) / 2 + m0[1] * (ys[j] + ys[j + 1]) / 2;
                var b = new Box2 { cx = cx, cy = cy, tx = t0x, ty = t0y, mx = m0[0], my = m0[1], ht = (hi - lo) / 2, hm = (ys[j + 1] - ys[j]) / 2 };
                boxes.Add(b); slabs.Add(b);
            }
            for (int i = 0; i < n; i++)
            {
                double tx = E[i][0] / L[i], ty = E[i][1] / L[i]; double[] m = M[i];
                double d = shell < depth ? shell : depth, a0 = 0, a1 = 0; bool found = false;
                while (d >= 0.05)
                {
                    LineInterval(M, V, V[i][0] + d * m[0], V[i][1] + d * m[1], tx, ty, out double lo, out double hi);
                    a0 = lo > 0.0 ? lo : 0.0; a1 = hi < L[i] ? hi : L[i];
                    double need = 0.1 > 0.2 * L[i] ? 0.1 : 0.2 * L[i];
                    if (a1 - a0 >= need) { found = true; break; }
                    d /= 2;
                }
                if (!found) continue;
                double cx = V[i][0] + tx * (a0 + a1) / 2 + m[0] * d / 2, cy = V[i][1] + ty * (a0 + a1) / 2 + m[1] * d / 2;
                var corners = new double[4][]; int q = 0;
                for (int sa = -1; sa <= 1; sa += 2) for (int sb = -1; sb <= 1; sb += 2)
                    corners[q++] = new[] { cx + sa * tx * (a1 - a0) / 2 + sb * m[0] * d / 2, cy + sa * ty * (a1 - a0) / 2 + sb * m[1] * d / 2 };
                bool inside = false;
                foreach (var s in slabs)
                {
                    bool all = true; var tv = new[] { s.tx, s.ty }; var mv = new[] { s.mx, s.my };
                    foreach (var c in corners)
                    {
                        double dx = c[0] - s.cx, dy = c[1] - s.cy;
                        if (!(Math.Abs(MatVec2Row(dx, dy, tv, true)) <= s.ht + 1e-3) || !(Math.Abs(MatVec2Row(dx, dy, mv, true)) <= s.hm + 1e-3)) { all = false; break; }
                    }
                    if (all) { inside = true; break; }
                }
                if (inside) continue;
                boxes.Add(new Box2 { cx = cx, cy = cy, tx = tx, ty = ty, mx = m[0], my = m[1], ht = (a1 - a0) / 2, hm = d / 2 });
            }
            return boxes;
        }

        /// _orient_rows(axes, halves): the rows of a box matrix assigned to the nearest world axes, aligned positively, det +1.
        static void OrientRows(double[][] axes, double[] halves, out double[] R, out double[] h)
        {
            var order = new int[] { -1, -1, -1 }; var used = new bool[3];
            var key = new double[3];
            for (int k = 0; k < 3; k++) { double mx = Math.Abs(axes[k][0]); for (int j = 1; j < 3; j++) if (Math.Abs(axes[k][j]) > mx) mx = Math.Abs(axes[k][j]); key[k] = -mx; }
            var ks = new List<int> { 0, 1, 2 };
            UxNp.StableSort(ks, (x, y) => key[x].CompareTo(key[y]));
            foreach (int k in ks)
            {
                int w = -1;
                for (int j = 0; j < 3; j++) if (!used[j] && (w < 0 || Math.Abs(axes[k][j]) > Math.Abs(axes[k][w]))) w = j;
                order[w] = k; used[w] = true;
            }
            R = new double[9]; h = new double[3];
            for (int j = 0; j < 3; j++)
            {
                double[] ax = axes[order[j]]; double sgn = ax[j] >= 0 ? 1 : -1;
                for (int c = 0; c < 3; c++) R[3 * j + c] = ax[c] * sgn;
                h[j] = halves[order[j]];
            }
            double det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) + R[2] * (R[3] * R[7] - R[4] * R[6]);
            if (det < 0)
            {
                int j = 0; for (int q = 1; q < 3; q++) if (Math.Abs(R[4 * q]) < Math.Abs(R[4 * j])) j = q;
                for (int c = 0; c < 3; c++) R[3 * j + c] = -R[3 * j + c];
            }
        }

        /// np.linalg.inv of a 3 x 3 matrix (row-major): LU with partial pivoting, then the solves (not LAPACK's bits; see the file header).
        static double[] Inv3(double[] a)
        {
            var m = new double[3, 6];
            for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) m[i, j] = a[3 * i + j]; m[i, 3 + i] = 1.0; }
            for (int c = 0; c < 3; c++)
            {
                int piv = c;
                for (int r = c + 1; r < 3; r++) if (Math.Abs(m[r, c]) > Math.Abs(m[piv, c])) piv = r;
                if (m[piv, c] == 0) throw new UxException("Singular matrix");
                if (piv != c) for (int j = 0; j < 6; j++) { double t = m[c, j]; m[c, j] = m[piv, j]; m[piv, j] = t; }
                for (int r = c + 1; r < 3; r++)
                {
                    double f = m[r, c] / m[c, c];
                    if (f == 0) continue;
                    for (int j = c; j < 6; j++) m[r, j] -= f * m[c, j];
                }
            }
            var o = new double[9];
            for (int j = 0; j < 3; j++)
                for (int i = 2; i >= 0; i--)
                {
                    double s = m[i, 3 + j];
                    for (int k = i + 1; k < 3; k++) s -= m[i, k] * o[3 * k + j];
                    o[3 * i + j] = s / m[i, i];
                }
            return o;
        }

        /// What boxfit_metrics needs of a mesh and does not depend on the boxes: the hull planes, the seeded surface samples (plus the
        /// mesh vertices) and the volume samples inside the hull.
        sealed class FitSamples { public double[] S, X; public int nx; }
        sealed class FitMesh
        {
            public double[] t; public double[] N, D; public readonly bool exact;
            readonly Dictionary<long, FitSamples> _samples = new Dictionary<long, FitSamples>();
            public FitMesh(double[] tris, bool exact = false) { t = CleanTris(tris, out _, out _); this.exact = exact; }
            /// _hull_planes(t.reshape(-1, 3)), computed when the first metrics are asked for (as Python does).
            public void EnsureHull() { if (N == null) HullPlanes(t, out N, out D); }
            public FitSamples Samples(int nSurface, int nVolume)
            {
                long key = ((long)nSurface << 32) | (uint)nVolume;
                if (_samples.TryGetValue(key, out var s)) return s;
                EnsureHull();
                int nt = t.Length / 9; var rng = new Rng12345();
                var ar = new double[nt];
                for (int i = 0; i < nt; i++)
                {
                    int p = 9 * i;
                    double ax = t[p + 3] - t[p], ay = t[p + 4] - t[p + 1], az = t[p + 5] - t[p + 2];
                    double bx = t[p + 6] - t[p], by = t[p + 7] - t[p + 1], bz = t[p + 8] - t[p + 2];
                    double c0 = ay * bz - az * by, c1 = az * bx - ax * bz, c2 = ax * by - ay * bx;
                    ar[i] = Math.Sqrt((c0 * c0 + c1 * c1) + c2 * c2) / 2;
                }
                double total = UxNp.Sum(ar);
                var prob = new double[nt]; for (int i = 0; i < nt; i++) prob[i] = ar[i] / total;
                int[] k = rng.Choice(prob, nSurface);
                double[] r1 = rng.Random(nSurface), r2 = rng.Random(nSurface);
                for (int j = 0; j < nSurface; j++) if (r1[j] + r2[j] > 1) { r1[j] = 1 - r1[j]; r2[j] = 1 - r2[j]; }
                var S = new double[3 * (nSurface + 3 * nt)];
                for (int j = 0; j < nSurface; j++)
                {
                    int p = 9 * k[j];
                    for (int c = 0; c < 3; c++) S[3 * j + c] = t[p + c] + r1[j] * (t[p + 3 + c] - t[p + c]) + r2[j] * (t[p + 6 + c] - t[p + c]);
                }
                Array.Copy(t, 0, S, 3 * nSurface, t.Length);
                double[] lo = { t[0], t[1], t[2] }, hi = { t[0], t[1], t[2] };
                for (int i = 3; i < t.Length; i++) { int c = i % 3; if (t[i] < lo[c]) lo[c] = t[i]; if (t[i] > hi[c]) hi[c] = t[i]; }
                var X = new double[3 * nVolume]; int nx = 0; int nf = D.Length;
                var x = new double[3];
                for (int j = 0; j < nVolume; j++)
                {
                    for (int c = 0; c < 3; c++) x[c] = lo[c] + rng.Random() * (hi[c] - lo[c]);
                    bool inside = true;
                    for (int f = 0; f < nf; f++)
                    {
                        if (!exact)
                        {   // plain arithmetic is within 1e-12 of the fused chain here: it decides unless the value is within 1e-9 of the limit
                            double dp = (x[0] * N[3 * f] + x[1] * N[3 * f + 1]) + x[2] * N[3 * f + 2] - D[f];
                            if (dp > 2e-9) { inside = false; break; }
                            if (dp < 0) continue;
                        }
                        if (!(Dot3(x, 0, N, 3 * f) - D[f] <= 1e-9)) { inside = false; break; }
                    }
                    if (inside) { X[3 * nx] = x[0]; X[3 * nx + 1] = x[1]; X[3 * nx + 2] = x[2]; nx++; }
                }
                s = new FitSamples { S = S, X = X, nx = nx };
                _samples[key] = s;
                return s;
            }
        }

        sealed class Metrics
        {
            public int boxes; public double protrusion, surface_max, surface_mean, volume;
            public JObj ToJson() => new JObj().Set("boxes", (long)boxes).Set("protrusion", protrusion).Set("surface_max", surface_max).Set("surface_mean", surface_mean).Set("volume", volume);
        }

        /// boxfit_metrics(tris, prims, n_surface, n_volume).
        static Metrics BoxfitMetrics(FitMesh mesh, List<Prim> prims, int nSurface = 4000, int nVolume = 20000)
        {
            mesh.EnsureHull();
            double[] N = mesh.N, D = mesh.D; int nf = D.Length, np = prims.Count;
            double protr = double.PositiveInfinity;
            if (np > 0)
            {
                protr = double.NegativeInfinity;
                foreach (var p in prims)
                {
                    var w = Apply(BoxCorners(p.prm), p.matrix);
                    for (int i = 0; i < 8; i++) for (int f = 0; f < nf; f++) { double d = Dot3(w, 3 * i, N, 3 * f) - D[f]; if (d > protr) protr = d; }
                }
            }
            var sm = mesh.Samples(nSurface, nVolume);
            // _box_dist setup per box: centre, inverse rotation (column access), |half|
            var inv = new double[np][]; var ctr = new double[np][]; var half = new double[np][];
            for (int b = 0; b < np; b++)
            {
                var m = prims[b].matrix;
                inv[b] = Inv3(new[] { m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10] });
                ctr[b] = new[] { m[12], m[13], m[14] };
                half[b] = new[] { Math.Abs(prims[b].prm[0]), Math.Abs(prims[b].prm[1]), Math.Abs(prims[b].prm[2]) };
            }
            double BoxDist(double[] pts, int at, int b)
            {   // _box_dist: (p - centre) @ inv(R) through the BLAS chain
                double dx = pts[at] - ctr[b][0], dy = pts[at + 1] - ctr[b][1], dz = pts[at + 2] - ctr[b][2]; double[] iv = inv[b];
                double q0 = Math.Abs(UxNp.Fma(dz, iv[6], UxNp.Fma(dy, iv[3], dx * iv[0]))) - half[b][0];
                double q1 = Math.Abs(UxNp.Fma(dz, iv[7], UxNp.Fma(dy, iv[4], dx * iv[1]))) - half[b][1];
                double q2 = Math.Abs(UxNp.Fma(dz, iv[8], UxNp.Fma(dy, iv[5], dx * iv[2]))) - half[b][2];
                if (q0 < 0) q0 = 0; if (q1 < 0) q1 = 0; if (q2 < 0) q2 = 0;
                return Math.Sqrt((q0 * q0 + q1 * q1) + q2 * q2);
            }
            // a value whose 4 decimals do not depend on an error of 1e-9
            bool Decided(double v) => double.IsInfinity(v) || double.IsNaN(v) || R(v - 1e-9, 4) == R(v + 1e-9, 4);
            int ns = sm.S.Length / 3; var ds = new double[ns]; double dsMax = double.NegativeInfinity, dsMean = 0; bool surfaceDone = false;
            if (!mesh.exact)
            {
                for (int j = 0; j < ns; j++)
                {
                    double d = double.PositiveInfinity; int at = 3 * j;
                    for (int b = 0; b < np; b++)
                    {
                        double dx = sm.S[at] - ctr[b][0], dy = sm.S[at + 1] - ctr[b][1], dz = sm.S[at + 2] - ctr[b][2]; double[] iv = inv[b];
                        double q0 = Math.Abs((dx * iv[0] + dy * iv[3]) + dz * iv[6]) - half[b][0];
                        double q1 = Math.Abs((dx * iv[1] + dy * iv[4]) + dz * iv[7]) - half[b][1];
                        double q2 = Math.Abs((dx * iv[2] + dy * iv[5]) + dz * iv[8]) - half[b][2];
                        if (q0 < 0) q0 = 0; if (q1 < 0) q1 = 0; if (q2 < 0) q2 = 0;
                        double x = Math.Sqrt((q0 * q0 + q1 * q1) + q2 * q2);
                        if (x < d) { d = x; if (d == 0) break; }
                    }
                    ds[j] = d; if (d > dsMax) dsMax = d;
                }
                dsMean = UxNp.Sum(ds) / ns;
                surfaceDone = Decided(dsMax) && Decided(dsMean);
            }
            if (!surfaceDone)
            {
                dsMax = double.NegativeInfinity;
                for (int j = 0; j < ns; j++)
                {
                    double d = double.PositiveInfinity;
                    for (int b = 0; b < np; b++) { double x = BoxDist(sm.S, 3 * j, b); if (x < d) d = x; }
                    ds[j] = d; if (d > dsMax) dsMax = d;
                }
                dsMean = UxNp.Sum(ds) / ns;
            }
            double vol = 0.0;
            if (sm.nx > 0)
            {
                int cnt = 0;
                for (int j = 0; j < sm.nx; j++)
                {
                    bool inside = false; int at = 3 * j;
                    if (np == 0) inside = 1.0 <= 1e-9;
                    for (int b = 0; b < np; b++)
                    {
                        if (!mesh.exact)
                        {
                            double dx = sm.X[at] - ctr[b][0], dy = sm.X[at + 1] - ctr[b][1], dz = sm.X[at + 2] - ctr[b][2]; double[] iv = inv[b];
                            double q0 = Math.Abs((dx * iv[0] + dy * iv[3]) + dz * iv[6]) - half[b][0];
                            double q1 = Math.Abs((dx * iv[1] + dy * iv[4]) + dz * iv[7]) - half[b][1];
                            double q2 = Math.Abs((dx * iv[2] + dy * iv[5]) + dz * iv[8]) - half[b][2];
                            if (q0 > 2e-9 || q1 > 2e-9 || q2 > 2e-9) continue;                           // further than 1e-9 from this box
                            if (q0 < -1e-9 && q1 < -1e-9 && q2 < -1e-9) { inside = true; break; }         // inside it
                        }
                        if (BoxDist(sm.X, at, b) <= 1e-9) { inside = true; break; }
                    }
                    if (inside) cnt++;
                }
                vol = (double)cnt / sm.nx;
            }
            return new Metrics { boxes = np, protrusion = R(protr, 5), surface_max = R(dsMax, 4), surface_mean = R(dsMean, 4), volume = R(vol, 4) };
        }

        /// boxfit_problems(m).
        static List<string> BoxfitProblems(Metrics m)
        {
            var p = new List<string>();
            if (m.protrusion > BOXFIT_TOL_IN) p.Add("a box sticks " + PyF(m.protrusion, 3) + " ft out of the mesh (> " + PyG(BOXFIT_TOL_IN) + ")");
            if (m.surface_max > BOXFIT_TOL_SURF) p.Add("mesh surface up to " + PyF(m.surface_max, 3) + " ft from the boxes (> " + PyG(BOXFIT_TOL_SURF) + ")");
            if (m.volume < BOXFIT_MIN_VOL) p.Add("boxes cover " + PyF(100 * m.volume, 1) + "% of the mesh volume (< " + PyF(100 * BOXFIT_MIN_VOL, 0) + "%)");
            return p;
        }

        sealed class BoxFit { public List<Prim> prims; public string section; public int slab_edge, slabs, polygon_vertices; public double[] axis; public double dent_ft, height; }

        /// fit_prism_boxes(tris): a convex prism triangle mesh -> kind-15 type-5 boxes. The slab direction (one per polygon edge) and the
        /// slab count (1..8) are searched: the fewest boxes that pass boxfit_problems, then the best volume share.
        static BoxFit FitPrismBoxes(FitMesh mesh, CancellationToken cancel)
        {
            const double shell = BOXFIT_SHELL; const int maxSlabs = 8;
            double[] t = mesh.t; int nt = t.Length / 9;
            if (nt == 0) throw new FitException("empty mesh");
            // _mesh_planes
            var Nf = new List<double[]>(); var Df = new List<double>(); var Af = new List<double>();
            var ntn = new double[3 * nt];                                   // nt: unit triangle normals (every triangle)
            for (int i = 0; i < nt; i++)
            {
                int p = 9 * i;
                double ax = t[p + 3] - t[p], ay = t[p + 4] - t[p + 1], az = t[p + 5] - t[p + 2];
                double bx = t[p + 6] - t[p], by = t[p + 7] - t[p + 1], bz = t[p + 8] - t[p + 2];
                double c0 = ay * bz - az * by, c1 = az * bx - ax * bz, c2 = ax * by - ay * bx;
                double a = Math.Sqrt((c0 * c0 + c1 * c1) + c2 * c2);
                if (a > 1e-9)
                {
                    var nn = new[] { c0 / a, c1 / a, c2 / a };
                    Nf.Add(nn); Df.Add((nn[0] * t[p] + nn[1] * t[p + 1]) + nn[2] * t[p + 2]); Af.Add(a / 2);
                }
                double den = a > 1e-12 ? a : 1e-12;
                ntn[3 * i] = c0 / den; ntn[3 * i + 1] = c1 / den; ntn[3 * i + 2] = c2 / den;
            }
            int nfaces = Nf.Count;
            // V = np.unique(np.round(t.reshape(-1, 3), 5), axis=0)
            var rows = new List<double[]>(3 * nt);
            for (int i = 0; i < 3 * nt; i++) rows.Add(new[] { UxNp.Round(t[3 * i], 5), UxNp.Round(t[3 * i + 1], 5), UxNp.Round(t[3 * i + 2], 5) });
            rows.Sort((x, y) => { int c = x[0].CompareTo(y[0]); if (c != 0) return c; c = x[1].CompareTo(y[1]); return c != 0 ? c : x[2].CompareTo(y[2]); });
            var V = new List<double[]>();
            foreach (var r in rows) if (V.Count == 0 || !(V[V.Count - 1][0] == r[0] && V[V.Count - 1][1] == r[1] && V[V.Count - 1][2] == r[2])) V.Add(r);
            double dent = double.NegativeInfinity;
            foreach (var v in V) for (int f = 0; f < nfaces; f++) { double d = Dot3(v, 0, Nf[f], 0) - Df[f]; if (d > dent) dent = d; }
            if (dent > BOXFIT_MAX_DENT) throw new FitException("mesh is not convex (a vertex " + PyF(dent, 3) + " ft outside a face plane, > " + PyG(BOXFIT_MAX_DENT) + ")");
            // the prism axis: a world axis, else a face normal (largest faces first)
            var cands = new List<double[]> { new double[] { 1, 0, 0 }, new double[] { 0, 1, 0 }, new double[] { 0, 0, 1 } };
            var byArea = new List<int>(); for (int f = 0; f < nfaces; f++) byArea.Add(f);
            UxNp.StableSort(byArea, (x, y) => (-Af[x]).CompareTo(-Af[y]));        // np.argsort(-A): the order of equal areas is not numpy's
            foreach (int f in byArea) cands.Add(Nf[f]);
            double[] axis = null;
            foreach (var a in cands)
            {
                bool all = true, top = false, bot = false;
                for (int f = 0; f < nfaces; f++)
                {
                    double d = MatVec3Row(Nf[f], 0, a, nfaces >= 2), ad = Math.Abs(d);
                    if (!(ad > 0.995 || ad < BOXFIT_SIDE_TOL)) { all = false; break; }
                    if (d > 0.995) top = true; if (d < -0.995) bot = true;
                }
                if (all && top && bot)
                {
                    double len = Math.Sqrt(Dot3(a, 0, a, 0));
                    axis = new[] { a[0] / len, a[1] / len, a[2] / len };
                    break;
                }
            }
            if (axis == null) throw new FitException("mesh is not a prism (no axis with two caps and side faces parallel to it)");
            int emin = 0; for (int c = 1; c < 3; c++) if (Math.Abs(axis[c]) < Math.Abs(axis[emin])) emin = c;
            var e = new double[3]; e[emin] = 1.0;
            double ea = Dot3(e, 0, axis, 0);
            var u = new[] { e[0] - ea * axis[0], e[1] - ea * axis[1], e[2] - ea * axis[2] };
            { double len = Math.Sqrt(Dot3(u, 0, u, 0)); u[0] /= len; u[1] /= len; u[2] /= len; }
            var w = new[] { axis[1] * u[2] - axis[2] * u[1], axis[2] * u[0] - axis[0] * u[2], axis[0] * u[1] - axis[1] * u[0] };
            // heights of the cap triangles
            double s_hi = double.PositiveInfinity, s_lo = double.NegativeInfinity; bool anyTop = false, anyBot = false;
            var pt = new List<double[]>(); var pb = new List<double[]>();
            for (int i = 0; i < nt; i++)
            {
                double d = MatVec3Row(ntn, 3 * i, axis, nt >= 2);
                bool capTop = d > 0.995, capBot = d < -0.995;
                if (!capTop && !capBot) continue;
                for (int v = 0; v < 3; v++)
                {
                    double s = MatVec3Row(t, 9 * i + 3 * v, axis, true);          // (n, 3, 3) @ axis: a 3-row matrix at a time
                    var pv = new[] { t[9 * i + 3 * v], t[9 * i + 3 * v + 1], t[9 * i + 3 * v + 2] };
                    if (capTop) { if (s < s_hi) s_hi = s; anyTop = true; pt.Add(pv); }
                    if (capBot) { if (s > s_lo) s_lo = s; anyBot = true; pb.Add(pv); }
                }
            }
            if (!anyTop || !anyBot) throw new FitException("zero-size array to reduction operation " + (!anyTop ? "minimum" : "maximum") + " which has no identity");
            double[][] Project(List<double[]> pts)
            {
                var o = new double[pts.Count][]; bool many = pts.Count >= 2;
                for (int i = 0; i < pts.Count; i++) o[i] = new[] { MatVec3Row(pts[i], 0, u, many), MatVec3Row(pts[i], 0, w, many) };
                return o;
            }
            var polys = new List<KeyValuePair<string, double[][]>> { new KeyValuePair<string, double[][]>("all-vertex hull", Hull2d(Project(V))) };
            var cut = ClipConvex(Hull2d(Project(pt)), Hull2d(Project(pb)));
            if (cut != null && cut.Length >= 3) polys.Add(new KeyValuePair<string, double[][]>("cap intersection", Hull2d(cut)));

            List<Prim> To3d(List<Box2> b2)
            {
                var o = new List<Prim>(b2.Count);
                foreach (var b in b2)
                {
                    var T3 = new[] { b.tx * u[0] + b.ty * w[0], b.tx * u[1] + b.ty * w[1], b.tx * u[2] + b.ty * w[2] };
                    var M3 = new[] { b.mx * u[0] + b.my * w[0], b.mx * u[1] + b.my * w[1], b.mx * u[2] + b.my * w[2] };
                    OrientRows(new[] { T3, M3, axis }, new[] { b.ht, b.hm, (s_hi - s_lo) / 2 }, out double[] Rm, out double[] h);
                    var mtx = new double[16];
                    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) mtx[4 * i + j] = (double)(float)Rm[3 * i + j];
                    for (int c = 0; c < 3; c++) mtx[12 + c] = (double)(float)(b.cx * u[c] + b.cy * w[c] + axis[c] * (s_hi + s_lo) / 2);
                    mtx[15] = 1.0;
                    o.Add(new Prim { type = 5, matrix = mtx, prm = new[] { (double)(float)h[0], (double)(float)h[1], (double)(float)h[2] }, m1 = 0xFFFFFFFF });
                }
                return o;
            }
            double[] bestScore = null; List<Prim> bestPrims = null; string bestSection = null; int bestEdge = 0, bestSlabs = 0, bestVerts = 0;
            for (int pj = 0; pj < polys.Count; pj++)
            {
                double[][] poly = polys[pj].Value; var dirs = new List<double[]>();
                for (int i = 0; i < poly.Length; i++)
                {
                    cancel.ThrowIfCancellationRequested();
                    double ex = poly[(i + 1) % poly.Length][0] - poly[i][0], ey = poly[(i + 1) % poly.Length][1] - poly[i][1];
                    double len = Math.Sqrt(Dot2(ex, ey, ex, ey)); ex /= len; ey /= len;
                    bool parallel = false;
                    foreach (var x in dirs) if (Math.Abs(Dot2(ex, ey, x[0], x[1])) > 0.9999) { parallel = true; break; }
                    if (parallel) continue;
                    dirs.Add(new[] { ex, ey });
                    for (int k = 1; k <= maxSlabs; k++)
                    {
                        var prims = To3d(Boxes2d(poly, shell, k, i));
                        var m = BoxfitMetrics(mesh, prims, 1500, 6000);
                        bool ok = BoxfitProblems(m).Count == 0;
                        double[] score = ok
                            ? new double[] { 0, prims.Count, -m.volume, m.surface_max, pj, i, k }
                            : new double[] { 1, m.surface_max + (m.protrusion > 0.0 ? m.protrusion : 0.0), -m.volume, prims.Count, pj, i, k };
                        if (bestScore == null || Less(score, bestScore))
                        { bestScore = score; bestPrims = prims; bestSection = polys[pj].Key; bestEdge = i; bestSlabs = k; bestVerts = poly.Length; }
                    }
                }
            }
            if (bestPrims == null) throw new UxException("box fit: no candidate (an empty section polygon)");
            return new BoxFit
            {
                prims = bestPrims, section = bestSection, slab_edge = bestEdge, slabs = bestSlabs, polygon_vertices = bestVerts,
                axis = new[] { R(axis[0], 4), R(axis[1], 4), R(axis[2], 4) }, dent_ft = R(dent, 4), height = R(s_hi - s_lo, 4),
            };
        }
        static bool Less(double[] a, double[] b)
        {   // tuple comparison
            for (int i = 0; i < a.Length; i++) { if (a[i] < b[i]) return true; if (a[i] > b[i]) return false; }
            return false;
        }

        // ====================================================================================== one source file
        sealed class Converted
        {
            public JObj rec; public byte[] data;
            public string chk;                       // "k22" | "prim" | "boxfit" | null
            public double[] chkTris; public uint chkKind; public double[] chkHalf; public List<Prim> chkPrims; public FitMesh chkMesh;
            public List<string> checkProblems; public JObj checkStats;
            public string error;
        }
        static List<object> Bounds(double[] lo, double[] hi)
            => new List<object> { new List<object> { lo[0], lo[1], lo[2] }, new List<object> { hi[0], hi[1], hi[2] } };

        /// convert_collis(data, name, type9, max_edge, dynamic): the record, the output bytes (or null) and what check_source needs.
        static Converted ConvertCollis(byte[] data, string name, string type9, double maxEdge, bool isDynamic, CancellationToken cancel, bool exactMetrics = false)
        {
            var d = UxMesh.ReadCollis(data);
            uint k = d.kind;
            var rec = new JObj().Set("name", name).Set("kind_in", (long)k).Set("version", (long)d.version).Set("src_bytes", (long)data.Length).Set("src_md5", UxBin.Md5(data))
                .Set("kind_out", null).Set("tris", 0L).Set("clusters", 0L).Set("prims", 0L).Set("prim_types", new JObj()).Set("bounds", null).Set("bytes", 0L).Set("md5", null)
                .Set("file", null).Set("dynamic", isDynamic);
            var r = new Converted { rec = rec };
            if (k == 2)
            {
                rec.Set("action", "drop").Set("reason", "kind-2 male point set: dropped, the entity Male becomes its Female asset");
                return r;
            }
            if (k == 19)
            {
                rec.Set("action", "refuse").Set("reason", "kind 19 (named animated compound, " + d.count + " children) has no Reflex PC equivalent");
                return r;
            }
            if (k == 0 && isDynamic)
            {
                BoxFit fit; FitMesh mesh;
                try
                {
                    mesh = new FitMesh(d.tris, exactMetrics);
                    fit = FitPrismBoxes(mesh, cancel);
                }
                catch (FitException ex)
                {
                    rec.Set("action", "refuse").Set("reason", "kind 0 mesh used by a dynamic body: no kind-15 box fit (" + ex.Message + "); stock dynamic bodies only use kind 15/16");
                    return r;
                }
                var prims = fit.prims;
                byte[] o = WritePrims15(prims);
                double[] lo = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, hi = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                foreach (var pr in prims)
                {
                    PrimBounds(pr, out double[] a, out double[] b);
                    for (int c = 0; c < 3; c++) { if (a[c] < lo[c]) lo[c] = a[c]; if (b[c] > hi[c]) hi[c] = b[c]; }
                }
                var m = BoxfitMetrics(mesh, prims);
                var bf = m.ToJson().Set("section", fit.section).Set("slab_edge", (long)fit.slab_edge).Set("slabs", (long)fit.slabs).Set("polygon_vertices", (long)fit.polygon_vertices)
                    .Set("axis", new List<object> { fit.axis[0], fit.axis[1], fit.axis[2] }).Set("dent_ft", fit.dent_ft).Set("height", fit.height);
                rec.Set("action", "boxfit").Set("kind_out", 15L).Set("prims", (long)prims.Count).Set("prim_types", new JObj().Set("5", (long)prims.Count)).Set("tris_src", (long)d.TriCount)
                    .Set("bounds", Bounds(lo, hi)).Set("boxfit", bf)
                    .Set("reason", "kind 0 mesh used by a dynamic body -> kind 15, " + prims.Count + " type-5 boxes fitted to the convex prism (" + fit.slabs +
                        " slabs + edge boxes; protrusion " + PyF(m.protrusion, 3) + " ft, surface max " + PyF(m.surface_max, 3) + " ft, volume " + PyF(100 * m.volume, 1) +
                        "%); stock dynamic bodies only use kind 15/16");
                r.data = o; r.chk = "boxfit"; r.chkMesh = mesh;
                return r;
            }
            if (k == 0)
            {
                byte[] o = Kind22Bytes(d.tris, maxEdge, out MeshInfo info, out double[] t);
                double[] lo = { 0, 0, 0 }, hi = { 0, 0, 0 };
                if (t.Length > 0)
                {
                    lo = new[] { t[0], t[1], t[2] }; hi = new[] { t[0], t[1], t[2] };
                    for (int i = 3; i < t.Length; i++) { int c = i % 3; if (t[i] < lo[c]) lo[c] = t[i]; if (t[i] > hi[c]) hi[c] = t[i]; }
                }
                rec.Set("action", "kind22").Set("kind_out", 22L).Set("tris", (long)info.tris).Set("clusters", (long)info.clusters).Set("tris_src", (long)info.tris_in)
                    .Set("split", (long)info.split).Set("dropped_degenerate", (long)info.degenerate).Set("dropped_nonfinite", (long)info.nonfinite)
                    .Set("max_cluster_tris", (long)info.max_cluster_tris).Set("max_cluster_half", info.max_cluster_half).Set("bounds", Bounds(lo, hi))
                    .Set("reason", "kind 0 mesh -> kind 22: " + info.tris + " tris (" + info.split + " split over " + PyG(maxEdge) + " ft, " + info.degenerate + " degenerate dropped) in " +
                        info.clusters + " clusters");
                r.data = o; r.chk = "k22"; r.chkTris = t;
                return r;
            }
            if (k != 11 && k != 15 && k != 17)
            {
                rec.Set("action", "refuse").Set("reason", "unknown collis kind " + k);
                return r;
            }
            if (k == 15 && d.version == 2)
            {
                rec.Set("action", "refuse").Set("reason", "kind-15 version-2 layout is not decoded");
                return r;
            }
            {
                var prims = SrcPrimitives(d);
                var types = new JObj(); bool has9 = false;
                foreach (var p in prims)
                {
                    string key = p.type.ToString(CultureInfo.InvariantCulture);
                    types.Set(key, (types.Has(key) ? (long)types.Get(key) : 0L) + 1);
                    if (p.type == 9) has9 = true;
                }
                rec.Set("prims", (long)prims.Count).Set("prim_types", types);
                if (has9 && type9 == "refuse")
                {
                    rec.Set("action", "refuse").Set("reason", "kind-15 primitive type 9 (frustum along local Y: r_bottom, r_top, full height) has no Reflex PC primitive; --type9 accept approximates it");
                    return r;
                }
                double[] lo = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, hi = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                foreach (var p in prims)
                {
                    PrimBounds(p.type != 9 ? p : Type9Cylinder(p), out double[] a, out double[] b);
                    for (int c = 0; c < 3; c++) { if (a[c] < lo[c]) lo[c] = a[c]; if (b[c] > hi[c]) hi[c] = b[c]; }
                }
                rec.Set("bounds", prims.Count > 0 ? Bounds(lo, hi) : null);
                r.chk = "prim"; r.chkKind = k; r.chkHalf = d.half;
                if (has9)
                {
                    var pp = new List<Prim>(); foreach (var p in prims) pp.Add(p.type == 9 ? Type9Cylinder(p) : AbsBox(p));
                    r.data = WritePrims15(pp);
                    rec.Set("action", "swap").Set("kind_out", 15L).Set("reason", "kind 15 field swap; type 9 frustum -> enclosing type-8 cylinder (--type9 accept)");
                    r.chkPrims = pp;
                    return r;
                }
                int nneg = 0;
                foreach (var p in prims) if (p.type == 5 && (p.prm[0] < 0 || p.prm[1] < 0 || p.prm[2] < 0)) nneg++;
                if (nneg > 0)
                {
                    var pp = new List<Prim>(); foreach (var p in prims) pp.Add(AbsBox(p));
                    r.data = WritePrims15(pp);
                    rec.Set("action", "swap").Set("kind_out", 15L).Set("fixed_negative_half", (long)nneg)
                        .Set("reason", "kind 15 field swap; " + nneg + " box(es) with a negative half extent written as |half| (same box; stock PC never stores a negative extent)");
                    r.chkPrims = pp;
                    return r;
                }
                r.chkPrims = prims;
                if (k == 11)
                {
                    var pp = new List<Prim>(); foreach (var p in prims) pp.Add(AbsBox(p));
                    r.data = WritePrims15(pp);
                    var sb = new StringBuilder("[");
                    for (int i = 0; i < 3; i++) { if (i > 0) sb.Append(", "); sb.Append(PyFormat.Repr(R(Math.Abs(d.half[i]), 4))); }
                    sb.Append("]");
                    rec.Set("action", "box15").Set("kind_out", 15L)
                        .Set("reason", "kind 11 box -> kind 15 with one type-5 box (identity matrix, m1 0xFFFFFFFF, half " + sb + "): stock Reflex uses kind 11 only on static signs, every stock dynamic body kind 15/16");
                    return r;
                }
                r.data = UxMesh.CollisToReflexPrimitive(data);
                if (k == 17)
                    rec.Set("action", "cylinder").Set("kind_out", 15L).Set("reason", "kind 17 -> kind 15 with one type-8 cylinder (r " + PyF(d.radius, 3) + ", height " + PyF(d.half_height, 3) + ")");
                else
                    rec.Set("action", "swap").Set("kind_out", (long)k).Set("reason", "kind " + k + " field swap" +
                        (k == 15 && d.tags != null && d.tags.Length > 0 ? "; " + d.tags.Length + " tag bytes dropped" : "") +
                        (k == 15 && prims.Count == 0 ? "; empty primitive list (as stock NAT_CO_WhitePine_S)" : ""));
                return r;
            }
        }

        /// check_source(rec, out, chk): the read-back checks; (problems, stats).
        static void CheckSource(Converted c)
        {
            c.checkProblems = new List<string>(); c.checkStats = new JObj();
            if (c.chk == null) return;
            if (c.chk == "boxfit")
            {
                PcCollis o;
                try { o = ParsePcCollis(c.data); }
                catch (Exception e) { c.checkProblems.Add("re-parse failed: " + e.Message); return; }
                bool clean = o.kind == 15 && o.tail == 0 && o.ntag == 0;
                if (clean) foreach (var pr in o.prims) if (pr.type != 5) clean = false;
                if (!clean) c.checkProblems.Add("not a clean kind-15 box list");
                var m = BoxfitMetrics(c.chkMesh, o.prims ?? new List<Prim>());
                c.checkProblems.AddRange(BoxfitProblems(m));
                c.checkStats = m.ToJson();
                return;
            }
            if (c.chk == "k22") { c.checkProblems = VerifyKind22(c.data, c.chkTris, out c.checkStats); return; }
            c.checkProblems = VerifyPrimitive(c.chkKind, c.chkHalf, c.chkPrims, c.data);
        }

        // ====================================================================================== entity map
        sealed class EMap
        {
            public string id, female, male, female_src, male_src, type, note; public long? kind_out; public long clusters; public object reflex, cls, name;
            public bool Same(EMap o) => female == o.female && male == o.male && female_src == o.female_src && male_src == o.male_src && type == o.type && note == o.note &&
                kind_out == o.kind_out && clusters == o.clusters && Equals(reflex, o.reflex) && Equals(cls, o.cls) && Equals(name, o.name);
            public JObj ToJson() => new JObj().Set("female", female).Set("male", male).Set("female_src", female_src).Set("male_src", male_src)
                .Set("kind_out", kind_out.HasValue ? (object)kind_out.Value : null).Set("clusters", clusters).Set("type", type).Set("reflex", reflex).Set("cls", cls).Set("name", name).Set("note", note);
        }
        sealed class EMaps
        {
            public readonly List<EMap> list = new List<EMap>(); public readonly Dictionary<string, int> index = new Dictionary<string, int>(StringComparer.Ordinal);
            public void Put(EMap m) { if (index.TryGetValue(m.id, out int at)) list[at] = m; else { index[m.id] = list.Count; list.Add(m); } }
            public bool SameAs(EMaps o)
            {
                if (list.Count != o.list.Count) return false;
                foreach (var m in list) if (!o.index.TryGetValue(m.id, out int at) || !m.Same(o.list[at])) return false;
                return true;
            }
            public JObj ToJson() { var j = new JObj(); foreach (var m in list) j.items.Add(new KeyValuePair<string, object>(m.id, m.ToJson())); return j; }
        }
        sealed class ByType { public long entities, clusters, no_female; }

        static bool Truthy(object v) => v is string s ? s.Length > 0 : v is bool b ? b : v is long l ? l != 0 : v is double dd ? dd != 0 : v is JObj j ? j.items.Count > 0 : v is List<object> li ? li.Count > 0 : v != null;
        static string PyStr(object v) => v == null ? "None" : v is string s ? s : v is bool b ? (b ? "True" : "False") : v is double d ? PyFormat.Repr(d) : System.Convert.ToString(v, CultureInfo.InvariantCulture);
        static string AssetOf(JObj props, string prop)
        {
            object p = props.Get(prop);
            if (!Truthy(p)) return null;
            object a = UxJson.Obj(p).Get("asset");
            return Truthy(a) ? UxJson.Str(a) : null;
        }
        static Dictionary<string, string> LowerNames(JObj level)
        {
            var lower = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var kv in UxJson.Obj(UxJson.Get(level, "closure", "collis")).items)
            {
                lower[UxCommon.PyLower(kv.Key)] = kv.Key;
                object rn = UxJson.Obj(kv.Value).Get("ref_names");
                if (rn != null) foreach (object n in UxJson.Arr(rn)) lower[UxCommon.PyLower(UxJson.Str(n))] = kv.Key;
            }
            return lower;
        }

        /// dynamic_sources(*level_jsons): the collis names a kept dynamic body names as Female or Male.
        static HashSet<string> DynamicSources(params JObj[] levels)
        {
            var o = new HashSet<string>(StringComparer.Ordinal);
            foreach (var d in levels)
            {
                if (d == null || d.items.Count == 0) continue;
                var lower = LowerNames(d);
                foreach (object eo in UxJson.Arr(d.Get("entities")))
                {
                    var e = UxJson.Obj(eo);
                    if (Array.IndexOf(KEEP_CLS, e.Get("cls") as string) < 0 || Array.IndexOf(DYNAMIC_TYPES, e.Get("type") as string) < 0) continue;
                    var props = UxJson.Obj(e.Get("props"));
                    foreach (string prop in new[] { "FemaleCollisionAsset", "MaleCollisionAsset" })
                    {
                        string a = AssetOf(props, prop);
                        if (a != null && lower.TryGetValue(UxCommon.PyLower(a), out string cn)) o.Add(cn);
                    }
                }
            }
            return o;
        }

        /// entity_map(level_json, sources).
        static EMaps EntityMap(JObj level, Dictionary<string, JObj> sources, out long placed, out List<KeyValuePair<string, ByType>> byType)
        {
            var lower = LowerNames(level);
            var emap = new EMaps(); placed = 0; byType = new List<KeyValuePair<string, ByType>>();
            foreach (object eo in UxJson.Arr(level.Get("entities")))
            {
                var e = UxJson.Obj(eo);
                if (Array.IndexOf(KEEP_CLS, e.Get("cls") as string) < 0) continue;
                var p = UxJson.Obj(e.Get("props"));
                string fa = AssetOf(p, "FemaleCollisionAsset"), ma = AssetOf(p, "MaleCollisionAsset");
                if (fa == null && ma == null) continue;
                string fs = fa != null && lower.TryGetValue(UxCommon.PyLower(fa), out string f1) ? f1 : null;
                string ms = ma != null && lower.TryGetValue(UxCommon.PyLower(ma), out string m1) ? m1 : null;
                JObj fr = fs != null && sources.TryGetValue(fs, out var fr1) ? fr1 : null;
                JObj mr = ms != null && sources.TryGetValue(ms, out var mr1) ? mr1 : null;
                var notes = new List<string>();
                string female = fr != null && Truthy(fr.Get("file")) ? (string)fr.Get("asset") : null;
                if (fs != null && female == null) notes.Add("Female " + fs + " " + (fr != null ? (string)fr.Get("action") : "missing") + ": " + (fr != null ? (string)fr.Get("reason") : ""));
                string male;
                if (mr != null && Truthy(mr.Get("file"))) male = (string)mr.Get("asset");
                else
                {
                    male = female;
                    if (ms != null && mr != null && (long)mr.Get("kind_in") == 2) notes.Add("Male " + ms + " is kind 2: Male = Female");
                    else if (ms != null && ms != fs) notes.Add("Male " + ms + " not converted (" + (mr != null ? (string)mr.Get("action") : "missing") + "): Male = Female");
                    else if (ms == null) notes.Add("no Male: Male = Female");
                }
                long ncl = fr != null && female != null && fr.Get("kind_out") is long ko && ko == 22 ? (long)fr.Get("clusters") : 0;
                placed += ncl;
                string t = (string)e.Get("type");
                ByType bt = null;
                foreach (var kv in byType) if (kv.Key == t) bt = kv.Value;
                if (bt == null) { bt = new ByType(); byType.Add(new KeyValuePair<string, ByType>(t, bt)); }
                bt.entities++; bt.clusters += ncl; if (female == null) bt.no_female++;
                emap.Put(new EMap
                {
                    id = PyStr(e.Get("id")), female = female, male = male, female_src = fs, male_src = ms,
                    kind_out = fr != null && female != null ? fr.Get("kind_out") as long? : null, clusters = ncl, type = t, reflex = e.Get("reflex"), cls = e.Get("cls"),
                    name = e.Get("name"), note = string.Join("; ", notes),
                });
            }
            return emap;
        }

        // ====================================================================================== pop entities (fix round 2)
        /// clip_prims15(prims, clip_y): the primitives clipped to local y <= clip_y; report entries (prim, action, y, top).
        static List<Prim> ClipPrims15(List<Prim> prims, double clipY, List<object> rep)
        {
            var o = new List<Prim>();
            for (int k = 0; k < prims.Count; k++)
            {
                var p = prims[k];
                var t = PrimVertices(p);
                double lo = t[1], hi = t[1];
                for (int i = 4; i < t.Length; i += 3) { if (t[i] < lo) lo = t[i]; if (t[i] > hi) hi = t[i]; }
                if (hi <= clipY + 1e-6) { o.Add(p); continue; }
                bool aligned = p.type == 5;
                if (aligned)
                    for (int i = 0; i < 3 && aligned; i++) for (int j = 0; j < 3; j++)
                    {
                        double b = i == j ? 1.0 : 0.0;
                        if (!(Math.Abs(p.matrix[4 * i + j] - b) <= 1e-6 + 1e-5 * Math.Abs(b))) { aligned = false; break; }      // np.allclose(atol=1e-6)
                    }
                if (!aligned)
                    throw new FitException("primitive " + k + " (type " + p.type + ") reaches y " + PyF(hi, 3) + " > clip " + PyF(clipY, 3) + " and is not an axis-aligned box");
                if (lo >= clipY - 1e-6)
                {
                    rep?.Add(new JObj().Set("prim", (long)k).Set("action", "dropped").Set("y", new List<object> { R(lo, 4), R(hi, 4) }));
                    continue;
                }
                var q = p.Copy();
                q.prm[1] = (clipY - lo) / 2.0;
                q.matrix[13] = (clipY + lo) / 2.0;
                o.Add(q);
                rep?.Add(new JObj().Set("prim", (long)k).Set("action", "shortened").Set("y", new List<object> { R(lo, 4), R(hi, 4) }).Set("top", R(clipY, 4)));
            }
            return o;
        }

        /// _hf_sampler(lv): work/<Level>/terrain/hf.npy (np.load(...).astype(np.float64)), or null.
        internal sealed class HfGrid
        {
            public double[] H; public int n;
            public static HfGrid Load(byte[] npy)
            {
                UxBin.Require(npy.Length >= 10 && npy[0] == 0x93 && npy[1] == 'N' && npy[2] == 'U' && npy[3] == 'M' && npy[4] == 'P' && npy[5] == 'Y', "hf.npy: not a .npy file");
                int hl, at;
                if (npy[6] == 1) { hl = UxBin.U16LE(npy, 8); at = 10; } else { hl = (int)UxBin.U32LE(npy, 8); at = 12; }
                UxBin.Require(at + hl <= npy.Length, "hf.npy: truncated header");
                string head = UxBin.Latin1(npy, at, hl);
                UxBin.Require(head.Contains("'fortran_order': False"), "hf.npy: Fortran order");
                bool f4 = head.Contains("'<f4'"), f8 = head.Contains("'<f8'");
                UxBin.Require(f4 || f8, "hf.npy: dtype is not <f4 / <f8");
                int sp = head.IndexOf("'shape':", StringComparison.Ordinal); UxBin.Require(sp >= 0, "hf.npy: no shape");
                int op = head.IndexOf('(', sp), cl = head.IndexOf(')', sp); UxBin.Require(op >= 0 && cl > op, "hf.npy: no shape");
                var dims = head.Substring(op + 1, cl - op - 1).Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                UxBin.Require(dims.Length == 2, "hf.npy: not 2-D");
                int rows = int.Parse(dims[0].Trim(), CultureInfo.InvariantCulture), cols = int.Parse(dims[1].Trim(), CultureInfo.InvariantCulture);
                int p = at + hl, size = f4 ? 4 : 8;
                UxBin.Require(rows >= 2 && cols >= 2 && (long)p + (long)rows * cols * size <= npy.Length, "hf.npy: data outside the file");
                UxBin.Require(cols >= rows, "hf.npy: fewer columns than rows");
                var g = new HfGrid { n = rows, H = new double[rows * cols] };
                g._cols = cols;
                for (int i = 0; i < rows * cols; i++) g.H[i] = f4 ? (double)UxBin.F32LE(npy, p + 4 * i) : BitConverter.Int64BitsToDouble((long)UxBin.U64(npy, p + 8 * i, false));
                return g;
            }
            int _cols;
            /// at(x, z): bilinear (1 ft px, row 0 = +Z, x0 = -n / 2, z1 = +n / 2).
            public double At(double x, double z)
            {
                double x0 = -n / 2.0, z1 = n / 2.0;
                double c = (x - x0) - 0.5, r = (z1 - z) - 0.5;
                long c0 = (long)Math.Floor(c), r0 = (long)Math.Floor(r);
                if (c0 < 0) c0 = 0; if (c0 > n - 2) c0 = n - 2; if (r0 < 0) r0 = 0; if (r0 > n - 2) r0 = n - 2;
                double fc = UxNp.Clip(c - c0, 0, 1), fr = UxNp.Clip(r - r0, 0, 1);
                double h00 = H[r0 * _cols + c0], h01 = H[r0 * _cols + c0 + 1], h10 = H[(r0 + 1) * _cols + c0], h11 = H[(r0 + 1) * _cols + c0 + 1];
                return (h00 * (1 - fc) + h01 * fc) * (1 - fr) + (h10 * (1 - fc) + h11 * fc) * fr;
            }
        }

        /// _sunk_excess(prims, mats, lift, hf).
        static double SunkExcess(List<Prim> prims, List<double[]> mats, double lift, HfGrid hf)
        {
            if (prims.Count == 0) throw new FitException("need at least one array to concatenate");
            var parts = new List<double[]>(); int total = 0;
            foreach (var p in prims) { var v = PrimVertices(p); parts.Add(v); total += v.Length; }
            var T = new double[total]; int at = 0;
            foreach (var v in parts) { Array.Copy(v, 0, T, at, v.Length); at += v.Length; }
            double worst = double.NegativeInfinity;
            foreach (var M in mats)
            {
                var W = Apply(T, M);
                int nv = W.Length / 3; double v;
                if (hf == null)
                {
                    double ymin = double.PositiveInfinity, ymax = double.NegativeInfinity;
                    for (int i = 0; i < nv; i++) { double y = W[3 * i + 1] + lift; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
                    v = ymax - ymin - POP_KEEP_FT;
                }
                else
                {
                    v = double.NegativeInfinity;
                    for (int i = 0; i < nv; i++)
                    {
                        double y = W[3 * i + 1] + lift;
                        double ex = y - POP_SINK_FT - (hf.At(W[3 * i], W[3 * i + 2]) - (POP_SINK_FT - POP_KEEP_FT));
                        if (ex > v) v = ex;
                    }
                }
                if (v > worst) worst = v;
            }
            return worst;
        }

        // ====================================================================================== level driver
        /// convert(level, type9, max_edge, pop_clip, write): the stage for one level. Reads level.json, level_fr.json (optional) and
        /// terrain/hf.npy (optional; the pop clip) from the work store and the closure's .collis from the disc; writes
        /// <outDir>/<asset>.collis and <outDir>/collision.json (the returned manifest). progress(name, done, total) per source.
        public static JObj Convert(UxContext ctx, UxCollisionOptions options = null, Action<string, int, int> progress = null)
        {
            var o = options ?? new UxCollisionOptions();
            var tn = o.templateCollis;
            if (tn == null)
            {
                tn = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
                if (ctx.stock != null) foreach (var kv in ctx.stock.templateNames) tn[kv.Key] = kv.Value.Of("collis");
            }
            return Convert(ctx.disc, ctx.lv, ctx.work, tn, o, progress, ctx.cancel, ctx.log);
        }

        public static JObj Convert(UxDiscLevel disc, UxLevelRow lv, UxWork work, Dictionary<string, Dictionary<string, string>> templateCollis,
            UxCollisionOptions options = null, Action<string, int, int> progress = null, CancellationToken cancel = default, Action<string> log = null)
        {
            var o = options ?? new UxCollisionOptions();
            if (o.primsTo22) throw new UxException("ux_collision: --prims-to-22 is not ported");
            if (o.type9 != "refuse" && o.type9 != "accept") throw new UxException("type9 must be one of ('refuse', 'accept')");
            if (o.popClip != "on" && o.popClip != "off") throw new UxException("pop_clip must be one of ('on', 'off')");
            var clock = System.Diagnostics.Stopwatch.StartNew();
            byte[] levelBytes = work.ReadBytes("level.json");
            JObj d = UxJson.Obj(UxJson.ParseUtf8(levelBytes));
            JObj dfr = work.Exists("level_fr.json") ? work.ReadJsonObj("level_fr.json") : null;
            string outDir = o.outDir ?? OutDirFor(o.primsTo22, o.type9);
            if (o.write) foreach (string old in work.List(outDir, "*.collis")) work.Delete(old);      // a work folder holds exactly this run's outputs
            var tnames = new List<KeyValuePair<string, Dictionary<string, string>>>();
            if (templateCollis != null) foreach (var kv in templateCollis) tnames.Add(kv);
            tnames.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
            var problems = new List<string>(); var warnings = new List<string>();
            var dyn = DynamicSources(d, dfr);

            // ---- the sources, in sorted name order (converted side by side, assembled in order)
            var closure = new List<KeyValuePair<string, object>>(UxJson.Obj(UxJson.Get(d, "closure", "collis")).items);
            closure.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
            int ns = closure.Count; var conv = new Converted[ns]; int done = 0;
            void One(int i)
            {
                cancel.ThrowIfCancellationRequested();
                string cn = closure[i].Key; var cr = UxJson.Obj(closure[i].Value);
                byte[] data = disc.ReadExtractFile(UxJson.Str(cr.Get("file")), cancel);
                Converted c;
                try { c = ConvertCollis(data, cn, o.type9, o.maxEdge, dyn.Contains(cn), cancel, o.exactMetrics); }
                catch (OperationCanceledException) { throw; }
                catch (Exception e) { c = new Converted { error = e.Message }; }
                if (c.error == null && c.data != null) CheckSource(c);
                conv[i] = c;
                progress?.Invoke(cn, Interlocked.Increment(ref done), ns);
            }
            int threads = o.threads > 0 ? o.threads : Environment.ProcessorCount;
            if (threads <= 1 || ns <= 1) for (int i = 0; i < ns; i++) One(i);
            else
            {
                try { Parallel.For(0, ns, new ParallelOptions { MaxDegreeOfParallelism = threads, CancellationToken = cancel }, One); }
                catch (AggregateException ae) { throw ae.Flatten().InnerExceptions[0]; }
            }
            var sources = new List<KeyValuePair<string, JObj>>(); var srcIndex = new Dictionary<string, JObj>(StringComparer.Ordinal);
            var outs = new Dictionary<string, byte[]>(StringComparer.Ordinal);
            for (int i = 0; i < ns; i++)
            {
                string cn = closure[i].Key; var cr = UxJson.Obj(closure[i].Value); var c = conv[i];
                if (c.error != null) { problems.Add(cn + ": " + c.error); continue; }
                var rec = c.rec;
                object ents = cr.Get("entities");
                rec.Set("as_female", cr.Get("as_female")).Set("as_male", cr.Get("as_male")).Set("entities_src", ents == null ? 0L : (long)UxJson.Arr(ents).Count);
                var clash = new JObj(); bool any = false;
                foreach (var kv in tnames)
                {
                    string hit = kv.Value != null && kv.Value.TryGetValue(UxCommon.PyLower(cn), out string h) ? h : null;
                    clash.Set(kv.Key, hit); if (!string.IsNullOrEmpty(hit)) any = true;
                }
                rec.Set("template_clash", clash);
                rec.Set("asset", any ? "UX_" + cn : cn);
                if (c.data != null)
                {
                    var check = new JObj().Set("ok", c.checkProblems.Count == 0).Set("problems", new List<object>(c.checkProblems));
                    foreach (var kv in c.checkStats.items) check.Set(kv.Key, kv.Value);
                    rec.Set("check", check);
                    foreach (string x in c.checkProblems) problems.Add(cn + ": " + x);
                    rec.Set("file", (string)rec.Get("asset") + ".collis").Set("bytes", (long)c.data.Length).Set("md5", UxBin.Md5(c.data));
                    outs[cn] = c.data;
                    if (o.write) work.WriteBytes(outDir + "/" + (string)rec.Get("file"), c.data);
                }
                else if ((string)rec.Get("action") == "refuse") warnings.Add(cn + " refused: " + (string)rec.Get("reason"));
                if (any && c.data != null) rec.Set("reason", (string)rec.Get("reason") + "; name clash with template collis: asset " + (string)rec.Get("asset"));
                sources.Add(new KeyValuePair<string, JObj>(cn, rec)); srcIndex[cn] = rec;
            }

            // ---- entity maps
            EMaps emap = EntityMap(d, srcIndex, out long placed, out var byType), emapFr = null; long placedFr = 0;
            if (dfr != null) emapFr = EntityMap(dfr, srcIndex, out placedFr, out _);

            // ---- pop variants
            JObj popRep = null; byte[] hfBytes = work.Exists("terrain/hf.npy") ? work.ReadBytes("terrain/hf.npy") : null;
            if (o.popClip == "on" && !o.primsTo22)
            {
                popRep = new JObj();
                double lift = UxCommon.YLift(lv);
                HfGrid hf = hfBytes != null ? HfGrid.Load(hfBytes) : null;
                var ents = new Dictionary<long, JObj>();
                foreach (var dd in new[] { d, dfr })
                    if (dd != null && dd.items.Count > 0)
                        foreach (object eo in UxJson.Arr(dd.Get("entities"))) { var e = UxJson.Obj(eo); long id = UxJson.Int(e.Get("id")); if (!ents.ContainsKey(id)) ents[id] = e; }
                var users = new Dictionary<string, SortedSet<long>>(StringComparer.Ordinal);
                foreach (var em in new[] { emap, emapFr })
                {
                    if (em == null) continue;
                    foreach (var m in em.list)
                        if (Array.IndexOf(POP_TYPES, m.type) >= 0 && !string.IsNullOrEmpty(m.female_src) && !string.IsNullOrEmpty(m.female))
                        {
                            if (!users.TryGetValue(m.female_src, out var set)) users[m.female_src] = set = new SortedSet<long>();
                            set.Add(long.Parse(m.id, CultureInfo.InvariantCulture));
                        }
                }
                var names = new List<string>(users.Keys); names.Sort(string.CompareOrdinal);
                foreach (string src in names)
                {
                    cancel.ThrowIfCancellationRequested();
                    if (!srcIndex.TryGetValue(src, out JObj r) || !(r.Get("kind_out") is long ko && ko == 15) || !outs.ContainsKey(src)) continue;
                    var prims = ParsePcCollis(outs[src]).prims;
                    if (prims == null || prims.Count == 0) continue;
                    var mats = new List<double[]>();
                    foreach (long eid in users[src])
                    {
                        var ml = UxJson.Arr(ents[eid].Get("matrix")); var mm = new double[16];
                        for (int i = 0; i < 16; i++) mm[i] = UxJson.Num(ml[i]);
                        mats.Add(mm);
                    }
                    double before = SunkExcess(prims, mats, lift, hf);
                    if (before <= 1e-6) continue;
                    double lo = double.PositiveInfinity, hi = double.NegativeInfinity;
                    foreach (var p in prims) { var t = PrimVertices(p); for (int i = 1; i < t.Length; i += 3) { if (t[i] < lo) lo = t[i]; if (t[i] > hi) hi = t[i]; } }
                    double clip; List<Prim> np2; var crep = new List<object>();
                    try
                    {
                        double a = lo, b = hi;
                        if (SunkExcess(ClipPrims15(prims, a + 1e-4, null), mats, lift, hf) > 1e-6)
                        {
                            problems.Add("pop collis " + src + ": even clipped to its lowest part the sunk pose stays above the HF");
                            continue;
                        }
                        for (int it = 0; it < 40; it++)
                        {
                            double c = (a + b) / 2;
                            if (SunkExcess(ClipPrims15(prims, c, null), mats, lift, hf) <= 0) a = c; else b = c;
                        }
                        clip = a;
                        np2 = ClipPrims15(prims, clip, crep);
                    }
                    catch (FitException ex) { problems.Add("pop collis " + src + ": " + ex.Message); continue; }
                    double after;
                    try { after = SunkExcess(np2, mats, lift, hf); }
                    catch (FitException ex) { throw new UxException(ex.Message); }
                    byte[] data = WritePrims15(np2);
                    string asset = (string)r.Get("asset") + "_UXpop";
                    bool clashes = false;
                    foreach (var kv in tnames) if (kv.Value != null && kv.Value.TryGetValue(UxCommon.PyLower(asset), out string h) && !string.IsNullOrEmpty(h)) clashes = true;
                    if (clashes) { problems.Add("pop collis variant " + asset + " clashes with a template collis"); continue; }
                    string key = src + "#pop";
                    var entIds = new List<object>(); foreach (long eid in users[src]) entIds.Add(eid);
                    var prec = new JObj().Set("action", "popclip").Set("variant_of", src).Set("asset", asset).Set("file", asset + ".collis").Set("bytes", (long)data.Length)
                        .Set("md5", UxBin.Md5(data)).Set("kind_in", r.Get("kind_in")).Set("kind_out", 15L).Set("clusters", 0L).Set("clip_y_local", R(clip, 4))
                        .Set("source_y", new List<object> { R(lo, 4), R(hi, 4) }).Set("prims", crep).Set("entities", entIds).Set("sunk_excess_before_ft", R(before, 3))
                        .Set("sunk_excess_after_ft", R(after, 3)).Set("hf", hf != null ? "terrain/hf.npy" : null)
                        .Set("reason", "pop entities (" + string.Join(", ", POP_TYPES) + ") sink " + PyG(POP_SINK_FT) + " ft: collision clipped at local y " + PyF(clip, 4) +
                            " so every sunk vertex lies >= " + PyG(POP_SINK_FT - POP_KEEP_FT) + " ft under the HF (was up to " + PyF(before, 2) + " ft above that)");
                    if (srcIndex.ContainsKey(key)) { for (int i = 0; i < sources.Count; i++) if (sources[i].Key == key) sources[i] = new KeyValuePair<string, JObj>(key, prec); }
                    else sources.Add(new KeyValuePair<string, JObj>(key, prec));
                    srcIndex[key] = prec;
                    if (o.write) work.WriteBytes(outDir + "/" + asset + ".collis", data);
                    foreach (var em in new[] { emap, emapFr })
                    {
                        if (em == null) continue;
                        foreach (var m in em.list)
                            if (Array.IndexOf(POP_TYPES, m.type) >= 0 && m.female_src == src && !string.IsNullOrEmpty(m.female))
                            {
                                if (m.male == m.female) m.male = asset;
                                m.female = asset;
                                m.note = (!string.IsNullOrEmpty(m.note) ? m.note + "; " : "") + "pop clip -> " + asset;
                            }
                    }
                    popRep.Set(src, new JObj().Set("asset", asset).Set("clip_y_local", R(clip, 4)).Set("source_y", new List<object> { R(lo, 4), R(hi, 4) })
                        .Set("entities", (long)users[src].Count).Set("sunk_excess_before_ft", R(before, 3)).Set("sunk_excess_after_ft", R(after, 3)).Set("prims", crep));
                }
            }

            foreach (var m in emap.list)
                if (!string.IsNullOrEmpty(m.female_src) && string.IsNullOrEmpty(m.female))
                    warnings.Add("entity " + m.id + " " + PyStr(m.name) + " (" + m.type + "): no collision (" + m.note + ")");
            // stock dynamic bodies use only kind 15 (16: TagBall): a dynamic body on another kind is a problem
            if (!o.primsTo22)
                foreach (var em in new[] { emap, emapFr })
                {
                    if (em == null) continue;
                    foreach (var m in em.list)
                    {
                        if (Array.IndexOf(DYNAMIC_TYPES, m.type) < 0 || (string.IsNullOrEmpty(m.female_src) && string.IsNullOrEmpty(m.male_src))) continue;
                        var ks = new SortedSet<long>();
                        foreach (string src in new[] { m.female_src, m.male_src })
                        {
                            JObj r = !string.IsNullOrEmpty(src) && srcIndex.TryGetValue(src, out var r1) ? r1 : null;
                            if (r != null && Truthy(r.Get("file")) && r.Get("kind_out") is long kk) ks.Add(kk);
                        }
                        if (string.IsNullOrEmpty(m.female)) continue;
                        var bad = new List<string>(); bool other = false;
                        foreach (long kk in ks) { bad.Add(I(kk)); if (kk != 15 && kk != 16) other = true; }
                        if (other)
                            problems.Add("dynamic entity " + m.id + " " + PyStr(m.name) + " (" + m.type + "): collision kinds [" + string.Join(", ", bad) + "] (stock dynamic bodies: kind 15/16 only)");
                    }
                }
            if (placed > UxCommon.Limits.MaxPlacedClusters) problems.Add("placed kind-22 clusters " + placed + " > " + UxCommon.Limits.MaxPlacedClusters);

            // ---- totals and the manifest
            long nwritten = 0, bytes = 0, uniqueClusters = 0; var byAction = new JObj(); var kindOut = new long[3];
            foreach (var kv in sources)
            {
                var r = kv.Value; string action = (string)r.Get("action");
                byAction.Set(action, (byAction.Has(action) ? (long)byAction.Get(action) : 0L) + 1);
                if (!Truthy(r.Get("file"))) continue;
                nwritten++; bytes += (long)r.Get("bytes"); uniqueClusters += (long)r.Get("clusters");
                if (r.Get("kind_out") is long ko) { if (ko == 11) kindOut[0]++; else if (ko == 15) kindOut[1]++; else if (ko == 22) kindOut[2]++; }
            }
            var dynSorted = new List<string>(dyn); dynSorted.Sort(string.CompareOrdinal);
            long noFemale = 0, maleDiffers = 0;
            foreach (var m in emap.list) { if (string.IsNullOrEmpty(m.female)) noFemale++; if (m.male != m.female) maleDiffers++; }
            var totals = new JObj().Set("sources", (long)sources.Count).Set("written", nwritten).Set("bytes", bytes).Set("by_action", byAction)
                .Set("kind_out", new JObj().Set("11", kindOut[0]).Set("15", kindOut[1]).Set("22", kindOut[2]))
                .Set("dynamic_sources", new List<object>(dynSorted)).Set("unique_clusters", uniqueClusters).Set("entities", (long)emap.list.Count)
                .Set("entities_without_female", noFemale).Set("male_differs", maleDiffers);
            var optJ = new JObj().Set("prims_to_22", o.primsTo22).Set("type9", o.type9).Set("max_edge", o.maxEdge).Set("max_tris", (long)MAX_TRIS).Set("max_half", MAX_HALF)
                .Set("out_dir", o.outDirText ?? outDir);
            if (o.popClip != "off") optJ.Set("pop_clip", o.popClip);
            var srcJ = new JObj(); foreach (var kv in sources) srcJ.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value));
            var btJ = new JObj();
            foreach (var kv in byType) btJ.Set(kv.Key, new JObj().Set("entities", kv.Value.entities).Set("clusters", kv.Value.clusters).Set("no_female", kv.Value.no_female));
            var man = new JObj().Set("schema", SCHEMA).Set("level", lv.disc).Set("short", lv.shortName).Set("tool", "ux_collision.py").Set("tool_md5", o.toolMd5)
                .Set("readers", o.readers ?? new JObj()).Set("level_json_md5", UxBin.Md5(levelBytes)).Set("options", optJ).Set("sources", srcJ)
                .Set("entities", emap.ToJson()).Set("entities_fr", emapFr != null ? emapFr.ToJson() : null)
                .Set("entities_fr_equal", emapFr != null ? (object)emapFr.SameAs(emap) : null)
                .Set("placed_clusters", new JObj().Set("sx", placed).Set("fr", emapFr != null ? (object)placedFr : null)).Set("placed_by_type", btJ).Set("totals", totals)
                .Set("problems", new List<object>(problems)).Set("warnings", new List<object>(warnings));
            if (popRep != null)
            {
                man.Set("pop_clip", popRep);
                man.Set("pop_clip_hf_md5", hfBytes != null ? UxBin.Md5(hfBytes) : null);
            }
            if (o.write) work.WriteJson(outDir + "/collision.json", man);
            log?.Invoke(lv.disc + " collision: " + sources.Count + " src " + nwritten + " out, placed " + placed + (emapFr != null ? " (fr " + placedFr + ")" : "") + ", ents " + emap.list.Count +
                ", nofem " + noFemale + ", " + (problems.Count == 0 ? "OK" : problems.Count + " PROBLEMS") + " " + clock.Elapsed.TotalSeconds.ToString("0.0", CultureInfo.InvariantCulture) + " s");
            return man;
        }
    }
}
