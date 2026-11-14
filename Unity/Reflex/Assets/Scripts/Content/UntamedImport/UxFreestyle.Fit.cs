// MX vs ATV Untamed converter: the Arena v3 ramp re-pose of ux_freestyle.py (FREESTYLE.md 13): deck_groups, deck_misfit, fit_translation
// and the numpy / scipy pieces they use, written out so the results equal the reference bit for bit:
//   closest_points / _closest_on_tris   the exact closest point on a triangle soup (Ericson 5.1.5), brute force, first minimum
//   tri_samples                         points on a barycentric grid of every triangle
//   Percentile95                        np.percentile(x, 95), method 'linear' (numpy 2.5 _lerp)
//   Powell / LineSearch / Bracket / Brent   scipy 1.18.1 optimize._minimize_powell (unbounded) and its scalar helpers
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    public static partial class UxFreestyle
    {
        public const double DECK_UP_FRAC = 0.9, DECK_NEAR_FRAC = 0.9, DECK_NEAR_FT = 1.5;
        public const double REPOSE_MISFIT_FT = 0.075, REPOSE_GAIN = 0.5, SAMPLE_FT = 0.25, FIT_SAMPLE_FT = 0.75;

        // ================================================================================================ geometry
        /// The exact distance from every point (3 doubles each) to the triangle soup T (9 doubles each): numpy's _closest_on_tris per pair,
        /// norm(q - x, axis=1), argmin = the first minimum.
        internal static double[] ClosestDistances(double[] P, double[] T)
        {
            int n = P.Length / 3, m = T.Length / 9;
            var bd = new double[n];
            // per-triangle constants (A, B, C, ab = B - A, ac = C - A, cb = C - B)
            var K = new double[18 * m];
            for (int j = 0; j < m; j++)
                for (int a = 0; a < 3; a++)
                {
                    double A = T[9 * j + a], B = T[9 * j + 3 + a], C = T[9 * j + 6 + a];
                    K[18 * j + a] = A; K[18 * j + 3 + a] = B; K[18 * j + 6 + a] = C;
                    K[18 * j + 9 + a] = B - A; K[18 * j + 12 + a] = C - A; K[18 * j + 15 + a] = C - B;
                }
            void One(int i)
            {
                double px = P[3 * i], py = P[3 * i + 1], pz = P[3 * i + 2];
                double best = double.PositiveInfinity; bool have = false;
                for (int j = 0; j < m; j++)
                {
                    int o = 18 * j;
                    double Ax = K[o], Ay = K[o + 1], Az = K[o + 2], Bx = K[o + 3], By = K[o + 4], Bz = K[o + 5], Cx = K[o + 6], Cy = K[o + 7], Cz = K[o + 8];
                    double abx = K[o + 9], aby = K[o + 10], abz = K[o + 11], acx = K[o + 12], acy = K[o + 13], acz = K[o + 14];
                    double apx = px - Ax, apy = py - Ay, apz = pz - Az;
                    double d1 = abx * apx + aby * apy + abz * apz;
                    double d2 = acx * apx + acy * apy + acz * apz;
                    double bpx = px - Bx, bpy = py - By, bpz = pz - Bz;
                    double d3 = abx * bpx + aby * bpy + abz * bpz;
                    double d4 = acx * bpx + acy * bpy + acz * bpz;
                    double cpx = px - Cx, cpy = py - Cy, cpz = pz - Cz;
                    double d5 = abx * cpx + aby * cpy + abz * cpz;
                    double d6 = acx * cpx + acy * cpy + acz * cpz;
                    double va = d3 * d6 - d5 * d4, vb = d5 * d2 - d1 * d6, vc = d1 * d4 - d3 * d2;
                    double qx, qy, qz;
                    if (d1 <= 0 && d2 <= 0) { qx = Ax; qy = Ay; qz = Az; }
                    else if (d3 >= 0 && d4 <= d3) { qx = Bx; qy = By; qz = Bz; }
                    else if (d6 >= 0 && d5 <= d6) { qx = Cx; qy = Cy; qz = Cz; }
                    else if (vc <= 0 && d1 >= 0 && d3 <= 0) { double s = Sdiv(d1, d1 - d3); qx = Ax + s * abx; qy = Ay + s * aby; qz = Az + s * abz; }
                    else if (vb <= 0 && d2 >= 0 && d6 <= 0) { double s = Sdiv(d2, d2 - d6); qx = Ax + s * acx; qy = Ay + s * acy; qz = Az + s * acz; }
                    else if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
                    {
                        double s = Sdiv(d4 - d3, (d4 - d3) + (d5 - d6));
                        qx = Bx + s * K[o + 15]; qy = By + s * K[o + 16]; qz = Bz + s * K[o + 17];
                    }
                    else
                    {
                        double den = Sdiv(1.0, va + vb + vc), sb = vb * den, sc = vc * den;
                        qx = Ax + abx * sb + acx * sc; qy = Ay + aby * sb + acy * sc; qz = Az + abz * sb + acz * sc;
                    }
                    double dx = qx - px, dy = qy - py, dz = qz - pz;
                    double d = Math.Sqrt(dx * dx + dy * dy + dz * dz);
                    if (!have || d < best || double.IsNaN(d) && !double.IsNaN(best)) { best = d; have = true; }
                }
                bd[i] = have ? best : double.PositiveInfinity;
            }
            if ((long)n * m > 20000) Parallel.For(0, n, One); else for (int i = 0; i < n; i++) One(i);
            return bd;
        }
        static double Sdiv(double a, double b) => a / (Math.Abs(b) > 1e-30 ? b : 1e-30);

        /// tri_samples(T, step): points on every triangle on a barycentric grid of about `step` ft (vertices and edges included).
        internal static double[] TriSamples(double[] T, double step)
        {
            var o = new List<double>();
            var ba = new double[3]; var ca = new double[3]; var cb = new double[3];
            for (int t = 0; t < T.Length; t += 9)
            {
                for (int a = 0; a < 3; a++) { ba[a] = T[t + 3 + a] - T[t + a]; ca[a] = T[t + 6 + a] - T[t + a]; cb[a] = T[t + 6 + a] - T[t + 3 + a]; }
                double L = Math.Max(Math.Max(UxNp.Norm(ba), UxNp.Norm(ca)), UxNp.Norm(cb));
                int n = Math.Max(1, (int)Math.Ceiling(L / step));
                for (int i = 0; i <= n; i++)
                    for (int j = 0; j <= n; j++)
                    {
                        if (i + j > n) continue;
                        double u = (double)i / n, v = (double)j / n;
                        for (int a = 0; a < 3; a++) o.Add(T[t + a] + u * ba[a] + v * ca[a]);
                    }
            }
            return o.ToArray();
        }

        /// np.percentile(x, 95) (method 'linear').
        internal static double Percentile95(double[] x)
        {
            var s = (double[])x.Clone(); Array.Sort(s);
            int n = s.Length;
            double vi = (n - 1) * 0.95;
            if (vi >= n - 1) return s[n - 1];
            double prev = Math.Floor(vi); int p = (int)prev;
            double g = vi - prev, a = s[p], b = s[p + 1], d = b - a;
            return g >= 0.5 ? b - d * (1 - g) : a + d * g;
        }
        static double Max(double[] x) { double m = double.NegativeInfinity; foreach (double v in x) if (v > m || double.IsNaN(v)) m = v; return m; }

        static double[] Shift(double[] P, double[] t, double sign)
        {
            var o = new double[P.Length];
            for (int i = 0; i < P.Length; i += 3) for (int a = 0; a < 3; a++) o[i + a] = sign > 0 ? P[i + a] + t[a] : P[i + a] - t[a];
            return o;
        }

        /// deck_misfit(D, F, t, step): (p95 deck -> faces, p95 faces -> deck, max deck -> faces, max faces -> deck), the deck moved by t.
        internal static double[] DeckMisfit(double[] D, double[] F, double[] t, double step = SAMPLE_FT)
        {
            var SD = Shift(TriSamples(D, step), t, 1);
            var SF = TriSamples(F, step);
            var a = ClosestDistances(SD, F);
            var b = ClosestDistances(SF, Shift(D, t, 1));
            return new[] { Percentile95(a), Percentile95(b), Max(a), Max(b) };
        }

        /// fit_translation(D, F): the translation of the deck minimising the mean squared exact distance, both directions (Powell from 0),
        /// rounded to 1e-4 ft; and the number of function evaluations.
        internal static (double[] t, int nfev) FitTranslation(double[] D, double[] F, double step = FIT_SAMPLE_FT)
        {
            var SD = TriSamples(D, step); var SF = TriSamples(F, step);
            double Fn(double[] t)
            {
                var a = ClosestDistances(Shift(SD, t, 1), F);
                var b = ClosestDistances(Shift(SF, t, -1), D);
                var sq = new double[a.Length + b.Length];
                for (int i = 0; i < a.Length; i++) sq[i] = a[i] * a[i];
                for (int i = 0; i < b.Length; i++) sq[a.Length + i] = b[i] * b[i];
                return UxNp.Sum(sq) / sq.Length;
            }
            var x = Powell(Fn, 3, 1e-4, 1e-10, 800, out int nfev);
            return (new[] { UxNp.Round(x[0], 4), UxNp.Round(x[1], 4), UxNp.Round(x[2], 4) }, nfev);
        }

        /// deck_groups(lv, ue, M, F): (deck triangles placed by M, group material names) of an Untamed entity's surface.
        internal static (double[] D, List<string> names) DeckGroups(UxContext ctx, JObj ue, double[] M, double[] F)
        {
            var lj = ctx.work.ReadJsonObj("level.json");
            var closure = UxJson.Obj(lj.Get("closure"));
            string sname = Ref(ue, "SurfaceAsset"), mname = Ref(ue, "ModelAsset");
            var surf = closure.Get("surface") as JObj; var mod = closure.Get("model") as JObj;
            var sr = sname != null && surf != null ? surf.Get(sname) as JObj : null;
            var mr = mname != null && mod != null ? mod.Get(mname) as JObj : null;
            var names = new List<string>(); var outT = new List<double>();
            if (sr == null || mr == null) return (new double[0], names);
            var gs = UxMesh.ReadSurface(ctx.disc.ReadExtractFile(UxJson.Str(sr.Get("file")), ctx.cancel));
            var mats = UxMesh.ReadModel(ctx.disc.ReadExtractFile(UxJson.Str(mr.Get("file")), ctx.cancel)).materials;
            var R = Rot(M);
            for (int gi = 0; gi < gs.Count; gi++)
            {
                var g = gs[gi];
                if (g.pos == null || gi >= mats.Length) continue;
                int nv = g.pos.Length / 3;
                var pos = new double[3 * nv]; for (int i = 0; i < pos.Length; i++) pos[i] = g.pos[i];
                var Pw = UxNp.MatMul(pos, nv, 3, R, 3);
                for (int i = 0; i < nv; i++) for (int a = 0; a < 3; a++) Pw[3 * i + a] += M[12 + a];
                int[] tri = UxMesh.StripToTris(g.strip);
                int nt = tri.Length / 3;
                if (nt == 0) continue;
                var T = new double[9 * nt];
                for (int i = 0; i < tri.Length; i++) for (int a = 0; a < 3; a++) T[3 * i + a] = Pw[3 * tri[i] + a];
                var area = new double[nt]; var up = new bool[nt]; var cen = new double[3 * nt];
                for (int i = 0; i < nt; i++)
                {
                    int o = 9 * i;
                    double ux = T[o + 3] - T[o], uy = T[o + 4] - T[o + 1], uz = T[o + 5] - T[o + 2];
                    double vx = T[o + 6] - T[o], vy = T[o + 7] - T[o + 1], vz = T[o + 8] - T[o + 2];
                    double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
                    double nn = Math.Sqrt(cx * cx + cy * cy + cz * cz);
                    area[i] = nn / 2;
                    up[i] = cy / Math.Max(nn, 1e-12) > 0.3;
                    for (int a = 0; a < 3; a++) cen[3 * i + a] = (T[o + a] + T[o + 3 + a] + T[o + 6 + a]) / 3.0;
                }
                double total = UxNp.Sum(area);
                if (!(total > 0)) continue;
                var d = ClosestDistances(cen, F);
                var au = new List<double>(); var an = new List<double>();
                for (int i = 0; i < nt; i++) { if (up[i]) au.Add(area[i]); if (d[i] <= DECK_NEAR_FT) an.Add(area[i]); }
                if (UxNp.Sum(au.ToArray()) / total >= DECK_UP_FRAC && UxNp.Sum(an.ToArray()) / total >= DECK_NEAR_FRAC)
                {
                    outT.AddRange(T);
                    names.Add(mats[gi]);
                }
            }
            return (outT.ToArray(), names);
        }

        static List<object> R4(double[] v) { var l = new List<object>(); foreach (double x in v) l.Add(UxNp.PyRound(x, 4)); return l; }
        static string Signed4(double v) => (v >= 0 || double.IsNaN(v) ? "+" : "") + v.ToString("F4", System.Globalization.CultureInfo.InvariantCulture);

        /// repose(lv, ue, M, F): (translation or null, record) of one carrier.
        internal static (double[] t, JObj rec) Repose(UxContext ctx, JObj ue, double[] M, double[] F, Action<string> log)
        {
            var rec = new JObj().Set("rule", "deck misfit p95 > 0.075 ft -> best-fit translation when it gives <= 0.5 x the misfit")
                .Set("deck_groups", new List<object>()).Set("misfit_before_ft", null).Set("translation_ft", null).Set("misfit_after_ft", null).Set("applied", false);
            if (ue == null) { rec.Set("note", "no Untamed entity"); return (null, rec); }
            var (D, names) = DeckGroups(ctx, ue, M, F);
            var nl = new List<object>(); foreach (string s in names) nl.Add(s);
            rec.Set("deck_groups", nl);
            if (D.Length == 0) { rec.Set("note", "no deck group (nothing drawn coincides with the riding faces)"); return (null, rec); }
            var zero = new double[3];
            var b = DeckMisfit(D, F, zero);
            rec.Set("misfit_before_ft", R4(b));
            if (Math.Max(b[0], b[1]) <= REPOSE_MISFIT_FT) return (null, rec);
            var (t, nfev) = FitTranslation(D, F);
            var a = DeckMisfit(D, F, t);
            rec.Set("translation_ft", new List<object> { t[0], t[1], t[2] });
            rec.Set("misfit_after_ft", R4(a));
            rec.Set("fit_evaluations", (long)nfev);
            if (Math.Max(a[0], a[1]) > REPOSE_GAIN * Math.Max(b[0], b[1])) { rec.Set("note", "best translation does not halve the misfit: pose kept"); return (null, rec); }
            rec.Set("applied", true);
            log?.Invoke("freestyle ramps: re-pose " + ue.Get("name") + " " + UxJson.Int(ue.Get("id")) + " by (" + Signed4(t[0]) + ", " + Signed4(t[1]) + ", " + Signed4(t[2]) +
                ") ft: deck misfit p95 " + Math.Max(b[0], b[1]).ToString("F3", System.Globalization.CultureInfo.InvariantCulture) + " -> " +
                Math.Max(a[0], a[1]).ToString("F3", System.Globalization.CultureInfo.InvariantCulture) + " ft");
            return (t, rec);
        }

        // ================================================================================================ scipy 1.18.1 Powell (unbounded)
        sealed class MaxFunCalls : Exception { }

        /// minimize(f, zeros(n), method='Powell', options=dict(xtol, ftol, maxfev)): x and nfev.
        internal static double[] Powell(Func<double[], double> fun, int N, double xtol, double ftol, int maxfun, out int nfev)
        {
            int ncalls = 0;
            double Fc(double[] x)
            {
                if (ncalls >= maxfun) throw new MaxFunCalls();
                ncalls++;
                return fun((double[])x.Clone());
            }
            var x = new double[N];
            var direc = new double[N][]; for (int i = 0; i < N; i++) { direc[i] = new double[N]; direc[i][i] = 1.0; }
            double fval = Fc(x);
            var x1 = (double[])x.Clone();
            double tol = xtol * 100;
            try
            {
                while (true)
                {
                    double fx = fval; int bigind = 0; double delta = 0.0;
                    for (int i = 0; i < N; i++)
                    {
                        double fx2i = fval;
                        var r = LineSearch(Fc, x, direc[i], tol, fval);
                        fval = r.f; x = r.p;
                        if ((fx2i - fval) > delta) { delta = fx2i - fval; bigind = i; }
                    }
                    double bnd = ftol * (Math.Abs(fx) + Math.Abs(fval)) + 1e-20;
                    if (2.0 * (fx - fval) <= bnd) break;
                    if (ncalls >= maxfun) break;
                    if (double.IsNaN(fx) && double.IsNaN(fval)) break;
                    var d1 = new double[N]; for (int k = 0; k < N; k++) d1[k] = x[k] - x1[k];
                    x1 = (double[])x.Clone();
                    var x2 = new double[N]; for (int k = 0; k < N; k++) x2[k] = x[k] + 1 * d1[k];
                    double fx2 = Fc(x2);
                    if (fx > fx2)
                    {
                        double t = 2.0 * (fx + fx2 - 2.0 * fval);
                        double temp = fx - fval - delta;
                        t *= temp * temp;
                        temp = fx - fx2;
                        t -= delta * temp * temp;
                        if (t < 0.0)
                        {
                            var r = LineSearch(Fc, x, d1, tol, fval);
                            fval = r.f; x = r.p;
                            bool any = false; foreach (double v in r.xi) if (v != 0) any = true;
                            if (any) { direc[bigind] = direc[N - 1]; direc[N - 1] = r.xi; }
                        }
                    }
                }
            }
            catch (MaxFunCalls) { }
            nfev = ncalls;
            return x;
        }

        /// _linesearch_powell (unbounded): (fval, p + alpha xi, alpha xi).
        static (double f, double[] p, double[] xi) LineSearch(Func<double[], double> F, double[] p, double[] xi, double tol, double fval)
        {
            bool any = false; foreach (double v in xi) if (v != 0) any = true;
            if (!any) return (fval, p, xi);
            int n = p.Length;
            double My(double alpha) { var q = new double[n]; for (int k = 0; k < n; k++) q[k] = p[k] + alpha * xi[k]; return F(q); }
            double amin, fret;
            try { (amin, fret) = Brent(My, tol); }
            catch (BracketError e)
            {
                var xs = new[] { e.xa, e.xb, e.xc }; var fs = new[] { e.fa, e.fb, e.fc };
                bool nan = false; foreach (double v in xs) nan |= double.IsNaN(v); foreach (double v in fs) nan |= double.IsNaN(v);
                if (nan) { amin = double.NaN; fret = double.NaN; }
                else { int im = 0; for (int k = 1; k < 3; k++) if (fs[k] < fs[im]) im = k; amin = xs[im]; fret = fs[im]; }
            }
            var x2 = new double[n]; var np2 = new double[n];
            for (int k = 0; k < n; k++) { x2[k] = amin * xi[k]; np2[k] = p[k] + x2[k]; }
            return (fret, np2, x2);
        }

        sealed class BracketError : Exception { public double xa, xb, xc, fa, fb, fc; }

        /// optimize.bracket(func, 0.0, 1.0): (xa, xb, xc, fa, fb, fc).
        static (double xa, double xb, double xc, double fa, double fb, double fc) Bracket(Func<double, double> func, double xa = 0.0, double xb = 1.0, double growLimit = 110.0, int maxiter = 1000)
        {
            const double _gold = 1.618034, _verysmall_num = 1e-21;
            double fa = func(xa), fb = func(xb);
            if (fa < fb) { double t = xa; xa = xb; xb = t; t = fa; fa = fb; fb = t; }
            double xc = xb + _gold * (xb - xa);
            double fc = func(xc);
            int iter = 0;
            while (fc < fb)
            {
                double tmp1 = (xb - xa) * (fb - fc);
                double tmp2 = (xb - xc) * (fb - fa);
                double val = tmp2 - tmp1;
                double denom = Math.Abs(val) < _verysmall_num ? 2.0 * _verysmall_num : 2.0 * val;
                double w = xb - ((xb - xc) * tmp2 - (xb - xa) * tmp1) / denom;
                double wlim = xb + growLimit * (xc - xb);
                if (iter > maxiter) throw new UxException("No valid bracket was found before the iteration limit was reached.");
                iter++;
                double fw;
                if ((w - xc) * (xb - w) > 0.0)
                {
                    fw = func(w);
                    if (fw < fc) { xa = xb; xb = w; fa = fb; fb = fw; break; }
                    else if (fw > fb) { xc = w; fc = fw; break; }
                    w = xc + _gold * (xc - xb);
                    fw = func(w);
                }
                else if ((w - wlim) * (wlim - xc) >= 0.0) { w = wlim; fw = func(w); }
                else if ((w - wlim) * (xc - w) > 0.0)
                {
                    fw = func(w);
                    if (fw < fc)
                    {
                        xb = xc; xc = w; w = xc + _gold * (xc - xb);
                        fb = fc; fc = fw; fw = func(w);
                    }
                }
                else { w = xc + _gold * (xc - xb); fw = func(w); }
                xa = xb; xb = xc; xc = w;
                fa = fb; fb = fc; fc = fw;
            }
            bool cond1 = (fb < fc && fb <= fa) || (fb < fa && fb <= fc);
            bool cond2 = (xa < xb && xb < xc) || (xc < xb && xb < xa);
            bool cond3 = UxNp.Finite(xa) && UxNp.Finite(xb) && UxNp.Finite(xc);
            if (!(cond1 && cond2 && cond3)) throw new BracketError { xa = xa, xb = xb, xc = xc, fa = fa, fb = fb, fc = fc };
            return (xa, xb, xc, fa, fb, fc);
        }

        /// optimize.Brent (brack None, maxiter 500): (xmin, fval).
        static (double x, double f) Brent(Func<double, double> func, double tol, int maxiter = 500)
        {
            const double _mintol = 1.0e-11, _cg = 0.3819660;
            var (xa, xb, xc, fa, fb, fc) = Bracket(func);
            double x = xb, w = xb, v = xb;
            double fw = fb, fv = fb, fx = fb;
            double a, b;
            if (xa < xc) { a = xa; b = xc; } else { a = xc; b = xa; }
            double deltax = 0.0, rat = 0.0;
            int iter = 0;
            while (iter < maxiter)
            {
                double tol1 = tol * Math.Abs(x) + _mintol;
                double tol2 = 2.0 * tol1;
                double xmid = 0.5 * (a + b);
                if (Math.Abs(x - xmid) < (tol2 - 0.5 * (b - a))) break;
                if (Math.Abs(deltax) <= tol1)
                {
                    deltax = x >= xmid ? a - x : b - x;
                    rat = _cg * deltax;
                }
                else
                {
                    double tmp1 = (x - w) * (fx - fv);
                    double tmp2 = (x - v) * (fx - fw);
                    double p = (x - v) * tmp2 - (x - w) * tmp1;
                    tmp2 = 2.0 * (tmp2 - tmp1);
                    if (tmp2 > 0.0) p = -p;
                    tmp2 = Math.Abs(tmp2);
                    double dx_temp = deltax;
                    deltax = rat;
                    if ((p > tmp2 * (a - x)) && (p < tmp2 * (b - x)) && (Math.Abs(p) < Math.Abs(0.5 * tmp2 * dx_temp)))
                    {
                        rat = p * 1.0 / tmp2;
                        double uu = x + rat;
                        if ((uu - a) < tol2 || (b - uu) < tol2) rat = xmid - x >= 0 ? tol1 : -tol1;
                    }
                    else
                    {
                        deltax = x >= xmid ? a - x : b - x;
                        rat = _cg * deltax;
                    }
                }
                double u = Math.Abs(rat) < tol1 ? (rat >= 0 ? x + tol1 : x - tol1) : x + rat;
                double fu = func(u);
                if (fu > fx)
                {
                    if (u < x) a = u; else b = u;
                    if ((fu <= fw) || (w == x)) { v = w; w = u; fv = fw; fw = fu; }
                    else if ((fu <= fv) || (v == x) || (v == w)) { v = u; fv = fu; }
                }
                else
                {
                    if (u >= x) a = x; else b = x;
                    v = w; w = x; x = u;
                    fv = fw; fw = fx; fx = fu;
                }
                iter++;
            }
            return (x, fx);
        }
    }
}
