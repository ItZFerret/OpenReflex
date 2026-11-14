// MX vs ATV Untamed converter, stage "race" (S8 / WP8): the port of the Python reference converter's ux_race.py (DESIGN 5.9 and the fix rounds
// 16-23): the tdf asset, the gate / board / anchor moves on the stock clone base (SX_FortDodge reduced to the race kit by the ux_base
// policy), the GateStart checks, the start-gate node patch and collision trim, the no-respawn flags over decks and static collision,
// the start-pack bale edges (UxStartPack), lap pacing (UxLapPacing, opt-in). It writes what `ux_race.run_level(write=True, plot=False)`
// writes: work/<Level>/race.json and race/<slot>-sx.tdf (+ the start-gate patch assets, + <slot>-lappacing.bxml).
//
// Bytes depend on numpy / OpenBLAS / CPython number rules, so those are modelled here bit for bit (UxRaceNum): dgemm / ddot / dgemv as
// fused multiply-add chains from +0, np.linalg.inv and solve as OpenBLAS's dgesv (getf2 + the trsm / trsv kernels), math.hypot,
// np.hypot, sum(), np.percentile, np.arange / linspace, np.random.default_rng (PCG64 + SeedSequence). The few ux_entities helpers the
// race kit needs are private copies here (marked "same as UxEntities.X"); ux_base's stock policy likewise ("same as UxBase.X").
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    // ====================================================================================================================== numbers
    /// The number rules of the Python reference that decide the race kit's bytes (CPython 3.12, numpy 2.5, OpenBLAS 0.3.34 Haswell
    /// kernels), each checked bit for bit against the reference.
    internal static class UxRaceNum
    {
        // ---------------------------------------------------------------------------------------- Python text
        /// round_half_even(|x| * 10^n) as an integer, exactly (x finite).
        static System.Numerics.BigInteger ScaledRound(double x, int n)
        {
            long bits = BitConverter.DoubleToInt64Bits(x);
            int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;          // |x| = m * 2^e
            System.Numerics.BigInteger num = m, den = System.Numerics.BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
            if (n >= 0) num *= System.Numerics.BigInteger.Pow(10, n); else den *= System.Numerics.BigInteger.Pow(10, -n);
            var q = System.Numerics.BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            return q;
        }
        /// '%.<n>f' % x: the correctly rounded decimal (round-half-even on the exact binary value), by integer arithmetic.
        public static string PyFixed(double x, int n)
        {
            if (double.IsNaN(x)) return "nan";
            if (double.IsInfinity(x)) return x > 0 ? "inf" : "-inf";
            string d = ScaledRound(x, n).ToString(CultureInfo.InvariantCulture).PadLeft(n + 1, '0');
            return (BitConverter.DoubleToInt64Bits(x) < 0 ? "-" : "") + (n == 0 ? d : d.Substring(0, d.Length - n) + "." + d.Substring(d.Length - n));
        }
        /// '%g' % x: six significant digits (correctly rounded), trailing zeros removed, d.ddde+XX outside 1e-4 <= |x| < 1e6.
        public static string PyG(double x)
        {
            if (double.IsNaN(x)) return "nan";
            if (double.IsInfinity(x)) return x > 0 ? "inf" : "-inf";
            string sign = BitConverter.DoubleToInt64Bits(x) < 0 ? "-" : "";
            if (x == 0) return sign + "0";
            int exp = (int)Math.Floor(Math.Log10(Math.Abs(x)));          // a first guess; the digit count below decides
            var lo = System.Numerics.BigInteger.Pow(10, 5); var hi = System.Numerics.BigInteger.Pow(10, 6);
            var q = ScaledRound(x, 5 - exp);
            for (int guard = 0; guard < 8 && (q >= hi || q < lo); guard++) { exp += q >= hi ? 1 : -1; q = ScaledRound(x, 5 - exp); }
            string digits = q.ToString(CultureInfo.InvariantCulture);     // six digits: d.ddddd * 10^exp
            string body;
            if (exp >= -4 && exp < 6)
            {
                if (exp >= 5) body = digits;
                else if (exp >= 0) body = digits.Substring(0, exp + 1) + "." + digits.Substring(exp + 1);
                else body = "0." + new string('0', -exp - 1) + digits;
                if (body.IndexOf('.') >= 0) body = body.TrimEnd('0').TrimEnd('.');
                return sign + body;
            }
            body = (digits.Substring(0, 1) + "." + digits.Substring(1)).TrimEnd('0').TrimEnd('.');
            return sign + body + "e" + (exp < 0 ? "-" : "+") + Math.Abs(exp).ToString("00", CultureInfo.InvariantCulture);
        }
        /// repr(x) of the JSON-like values the messages hold: str, int, float, bool, None, list, dict (JObj or an integer-keyed
        /// dictionary). ordered: an OrderedDict (Python 3.12 writes OrderedDict({...})).
        public static string PyRepr(object v, bool ordered = false)
        {
            switch (v)
            {
                case null: return "None";
                case bool b: return b ? "True" : "False";
                case string s:
                {
                    bool dq = s.IndexOf('\'') >= 0 && s.IndexOf('"') < 0;
                    var sb = new StringBuilder(); sb.Append(dq ? '"' : '\'');
                    foreach (char c in s)
                    {
                        if (c == '\\') sb.Append("\\\\"); else if (c == '\'' && !dq) sb.Append("\\'"); else if (c == '\n') sb.Append("\\n");
                        else if (c == '\r') sb.Append("\\r"); else if (c == '\t') sb.Append("\\t"); else sb.Append(c);
                    }
                    return sb.Append(dq ? '"' : '\'').ToString();
                }
                case double d: return double.IsNaN(d) ? "nan" : double.IsInfinity(d) ? (d > 0 ? "inf" : "-inf") : PyFormat.Repr(d);
                case float f: return PyRepr((double)f);
                case JObj o:
                {
                    var sb = new StringBuilder(ordered ? "OrderedDict({" : "{");
                    for (int i = 0; i < o.items.Count; i++) { if (i > 0) sb.Append(", "); sb.Append(PyRepr(o.items[i].Key)).Append(": ").Append(PyRepr(o.items[i].Value, ordered)); }
                    return sb.Append(ordered ? "})" : "}").ToString();
                }
                case System.Collections.IDictionary dict:
                {
                    var sb = new StringBuilder(ordered ? "OrderedDict({" : "{"); bool first = true;
                    foreach (System.Collections.DictionaryEntry e in dict) { if (!first) sb.Append(", "); first = false; sb.Append(PyRepr(e.Key)).Append(": ").Append(PyRepr(e.Value, ordered)); }
                    return sb.Append(ordered ? "})" : "}").ToString();
                }
                case System.Collections.IEnumerable list:
                {
                    var sb = new StringBuilder("["); bool first = true;
                    foreach (object x in list) { if (!first) sb.Append(", "); first = false; sb.Append(PyRepr(x, ordered)); }
                    return sb.Append("]").ToString();
                }
                default: return Convert.ToString(v, CultureInfo.InvariantCulture);
            }
        }

        // ---------------------------------------------------------------------------------------- CPython math
        /// math.hypot(x, y) of CPython 3.12 (mathmodule.c vector_norm: scaled squares summed in double-double, one correction step).
        public static double PyHypot(double x, double y)
        {
            x = Math.Abs(x); y = Math.Abs(y);
            double max = x > y ? x : y;
            if (double.IsInfinity(x) || double.IsInfinity(y)) return double.PositiveInfinity;
            if (double.IsNaN(x) || double.IsNaN(y)) return UxNp.NaN;
            if (max == 0.0) return max;
            return VectorNorm(x, y, max);
        }
        static double VectorNorm(double v0, double v1, double max)
        {
            // frexp(max): max = m * 2^max_e, 0.5 <= m < 1
            long bits = BitConverter.DoubleToInt64Bits(max);
            int be = (int)((bits >> 52) & 0x7FF), max_e;
            if (be == 0) { long frac = bits & 0xFFFFFFFFFFFFFL; int top = 0; while ((frac >> top) > 1) top++; max_e = top - 1073; }
            else max_e = be - 1022;
            if (max_e < -1023)
            {   // subnormals: DBL_MIN * vector_norm(vec / DBL_MIN)
                const double DBL_MIN = 2.2250738585072014e-308;
                return DBL_MIN * VectorNorm(v0 / DBL_MIN, v1 / DBL_MIN, max / DBL_MIN);
            }
            double scale = Ldexp1(-max_e);
            double csum = 1.0, frac1 = 0.0, frac2 = 0.0;
            for (int i = 0; i < 2; i++)
            {
                double x = (i == 0 ? v0 : v1) * scale;                  // lossless scaling
                double hi = x * x, lo = UxNp.Fma(x, x, -hi);            // dl_mul: lossless squaring
                double sh = csum + hi, sl = (csum - sh) + hi;           // dl_fast_sum
                csum = sh; frac1 += lo; frac2 += sl;
            }
            double h = Math.Sqrt(csum - 1.0 + (frac1 + frac2));
            {
                double hi = -h * h, lo = UxNp.Fma(-h, h, -hi);
                double sh = csum + hi, sl = (csum - sh) + hi;
                csum = sh; frac1 += lo; frac2 += sl;
            }
            double xx = csum - 1.0 + (frac1 + frac2);
            h += xx / (2.0 * h);                                         // differential correction
            return h / scale;
        }
        /// ldexp(1.0, e) = 2^e (also a subnormal power of two).
        static double Ldexp1(int e) => e >= -1022 ? BitConverter.Int64BitsToDouble((long)(e + 1023) << 52) : e >= -1074 ? BitConverter.Int64BitsToDouble(1L << (e + 1074)) : 0.0;

        /// sum(floats) of CPython 3.12: Neumaier's compensated sum, started as 0 + first.
        public static double PySum(IList<double> v)
        {
            if (v.Count == 0) return 0.0;
            double f = 0 + v[0], c = 0.0;
            for (int i = 1; i < v.Count; i++)
            {
                double x = v[i], t = f + x;
                if (Math.Abs(f) >= Math.Abs(x)) c += (f - t) + x; else c += (x - t) + f;
                f = t;
            }
            if (c != 0 && UxNp.Finite(c)) f += c;
            return f;
        }

        // ---------------------------------------------------------------------------------------- numpy element-wise
        /// np.hypot(x, y) on the reference machine: sqrt(x*x + y*y) (measured: equal on 20,000 random pairs; not the correctly rounded
        /// value in 17 % of them).
        public static double NpHypot(double x, double y) => Math.Sqrt(x * x + y * y);
        /// add.reduce of three values (a row of an (n, 3) array): from +0, left to right.
        public static double Sum3(double a, double b, double c) => ((0.0 + a) + b) + c;
        public static bool AllClose(double a, double b, double atol = 1e-8, double rtol = 1e-5) => Math.Abs(a - b) <= atol + rtol * Math.Abs(b);

        /// np.linspace(0, 1, num).
        public static double[] Linspace01(int num)
        {
            var y = new double[num];
            if (num == 0) return y;
            int div = num - 1;
            if (div > 0) { double step = 1.0 / div; for (int i = 0; i < num; i++) y[i] = i * step; }
            if (num > 1) y[num - 1] = 1.0;
            return y;
        }
        /// np.arange(start, stop, step) for floats: ceil((stop - start) / step) values, start + i * ((start + step) - start).
        public static double[] Arange(double start, double stop, double step)
        {
            double len = Math.Ceiling((stop - start) / step);
            int n = len > 0 ? checked((int)len) : 0;
            var a = new double[n];
            if (n > 0) a[0] = start;
            if (n > 1) a[1] = start + step;
            double delta = (start + step) - start;
            for (int i = 2; i < n; i++) a[i] = start + i * delta;
            return a;
        }
        /// np.median.
        public static double Median(IList<double> v)
        {
            var a = new List<double>(v); a.Sort();
            int n = a.Count;
            return n % 2 == 1 ? a[n / 2] : (a[n / 2 - 1] + a[n / 2]) / 2;
        }
        /// np.percentile(v, q) (method 'linear'; q in percent).
        public static double Percentile(IList<double> v, double q)
        {
            var a = new List<double>(v); a.Sort();
            int n = a.Count;
            double vi = (n - 1) * (q / 100);
            double prev = Math.Floor(vi), g = vi - prev;
            double lo = a[(int)prev], hi = a[Math.Min((int)prev + 1, n - 1)];
            double d = hi - lo;
            double r = lo + d * g;
            if (g >= 0.5) r = hi - d * (1 - g);
            return r;
        }

        // ---------------------------------------------------------------------------------------- OpenBLAS
        /// np.dot / a @ b of two float64 vectors of fewer than 16 elements (cblas_ddot's scalar loop): s = +0; s = fma(a_k, b_k, s).
        public static double Dot(double[] a, double[] b)
        {
            double s = a[0] * b[0] + 0.0;
            for (int k = 1; k < a.Length; k++) s = UxNp.Fma(a[k], b[k], s);
            return s;
        }
        public static double Dot2(double a0, double a1, double b0, double b1) => UxNp.Fma(a1, b1, a0 * b0 + 0.0);
        public static double Dot3(double a0, double a1, double a2, double b0, double b1, double b2) => UxNp.Fma(a2, b2, UxNp.Fma(a1, b1, a0 * b0 + 0.0));
        /// np.linalg.norm of a 1-D float64 vector: sqrt(dot(v, v)).
        public static double Norm2(double a0, double a1) => Math.Sqrt(Dot2(a0, a1, a0, a1));
        public static double Norm3(double a0, double a1, double a2) => Math.Sqrt(Dot3(a0, a1, a2, a0, a1, a2));

        /// A @ B for float64 matrices (m x k) @ (k x n), row-major (cblas_dgemm, also for stacked operands and for m in the 100,000s):
        /// every element is s = +0; s = fma(A[i,t], B[t,j], s) for t = 0 .. k-1. (UxNp.MatMul starts from the first product, which
        /// keeps a -0 where BLAS gives +0.) Not for m = 1 with k = 4 (numpy takes the gemv path there).
        public static double[] MatMul(double[] A, int m, int k, double[] B, int n)
        {
            var C = new double[m * n];
            for (int i = 0; i < m; i++)
                for (int j = 0; j < n; j++)
                {
                    double s = A[i * k] * B[j] + 0.0;
                    for (int t = 1; t < k; t++) s = UxNp.Fma(A[i * k + t], B[t * n + j], s);
                    C[i * n + j] = s;
                }
            return C;
        }
        public static double[] MatMul4(double[] A, double[] B) => MatMul(A, 4, 4, B, 4);
        /// One row of (m, 3) @ M[:3, :3] + M[3, :3] for a 4 x 4 row-major M: the dgemm chain, then the element-wise add.
        public static void Transform(double[] M, double x, double y, double z, out double ox, out double oy, out double oz)
        {
            ox = UxNp.Fma(z, M[8], UxNp.Fma(y, M[4], x * M[0] + 0.0)) + M[12];
            oy = UxNp.Fma(z, M[9], UxNp.Fma(y, M[5], x * M[1] + 0.0)) + M[13];
            oz = UxNp.Fma(z, M[10], UxNp.Fma(y, M[6], x * M[2] + 0.0)) + M[14];
        }
        /// (y @ A)[j] for a float64 4-vector and a 4 x 4 matrix (cblas_dgemv, transposed): the chain 1, 0, 2, 3 from +0.
        public static double VecMat4(double[] y, int yo, double[] A, int j)
            => UxNp.Fma(y[yo + 3], A[12 + j], UxNp.Fma(y[yo + 2], A[8 + j], UxNp.Fma(y[yo], A[j], y[yo + 1] * A[4 + j] + 0.0)));

        /// OpenBLAS dgetf2 on a row-major n x n matrix (in place): the left-looking LU with partial pivoting, the dot / gemv updates as
        /// fma chains and the column scaled by the reciprocal of the pivot. Returns the pivot rows.
        static int[] Getf2(double[] A, int n)
        {
            var piv = new int[n];
            for (int j = 0; j < n; j++)
            {
                for (int i = 0; i < j; i++)
                {
                    int ip = piv[i];
                    if (ip != i) { double t = A[i * n + j]; A[i * n + j] = A[ip * n + j]; A[ip * n + j] = t; }
                }
                for (int i = 1; i < j; i++)
                {
                    double t = 0.0;
                    for (int k = 0; k < i; k++) t = UxNp.Fma(A[i * n + k], A[k * n + j], t);
                    A[i * n + j] = A[i * n + j] - t;
                }
                if (j > 0)
                    for (int r = j; r < n; r++)
                    {
                        double t = 0.0;
                        for (int k = 0; k < j; k++) t = UxNp.Fma(A[r * n + k], A[k * n + j], t);
                        A[r * n + j] = UxNp.Fma(-1.0, t, A[r * n + j]);
                    }
                int jp = j; double best = Math.Abs(A[j * n + j]);
                for (int r = j + 1; r < n; r++) if (Math.Abs(A[r * n + j]) > best) { best = Math.Abs(A[r * n + j]); jp = r; }
                piv[j] = jp;
                double t1 = A[jp * n + j];
                if (t1 == 0.0) throw new UxException("Singular matrix");          // numpy.linalg.LinAlgError
                if (jp != j) for (int c = 0; c <= j; c++) { double t = A[j * n + c]; A[j * n + c] = A[jp * n + c]; A[jp * n + c] = t; }
                double s = 1.0 / t1;
                for (int r = j + 1; r < n; r++) A[r * n + j] = A[r * n + j] * s;
            }
            return piv;
        }
        /// np.linalg.inv of an n x n float64 matrix (row-major): dgesv with the identity as right-hand sides (getf2, the row swaps, the
        /// unit lower solve, the upper solve with reciprocal diagonals: the generic trsm kernels, every update one fused multiply-add).
        public static double[] Inv(double[] M, int n)
        {
            var A = (double[])M.Clone();
            var piv = Getf2(A, n);
            var B = new double[n * n];
            for (int i = 0; i < n; i++) B[i * n + i] = 1.0;
            for (int i = 0; i < n; i++)
            {
                int ip = piv[i];
                if (ip != i) for (int c = 0; c < n; c++) { double t = B[i * n + c]; B[i * n + c] = B[ip * n + c]; B[ip * n + c] = t; }
            }
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                {
                    double bb = B[i * n + j] * 1.0;
                    B[i * n + j] = bb;
                    for (int k = i + 1; k < n; k++) B[k * n + j] = UxNp.Fma(-bb, A[k * n + i], B[k * n + j]);
                }
            for (int i = n - 1; i >= 0; i--)
            {
                double aa = 1.0 / A[i * n + i];
                for (int j = 0; j < n; j++)
                {
                    double bb = B[i * n + j] * aa;
                    B[i * n + j] = bb;
                    for (int k = 0; k < i; k++) B[k * n + j] = UxNp.Fma(-bb, A[k * n + i], B[k * n + j]);
                }
            }
            return B;
        }
        public static double[] Inv4(double[] M) => Inv(M, 4);
        /// np.linalg.solve(A, b) for an n x n matrix (row-major) and one right-hand side: dgesv with the trsv drivers (axpy updates as
        /// fused multiply-adds, a division by the diagonal).
        public static double[] Solve(double[] M, double[] rhs, int n)
        {
            var A = (double[])M.Clone(); var b = (double[])rhs.Clone();
            var piv = Getf2(A, n);
            for (int i = 0; i < n; i++) if (piv[i] != i) { double t = b[i]; b[i] = b[piv[i]]; b[piv[i]] = t; }
            for (int i = 0; i < n; i++) for (int k = i + 1; k < n; k++) b[k] = UxNp.Fma(-b[i], A[k * n + i], b[k]);
            for (int i = n - 1; i >= 0; i--)
            {
                b[i] = b[i] / A[i * n + i];
                for (int k = 0; k < i; k++) b[k] = UxNp.Fma(-b[i], A[k * n + i], b[k]);
            }
            return b;
        }
        /// np.linalg.det of a 2 x 2 matrix, used only against the 1e-9 threshold of "parallel": numpy's value is sign * exp(sum(log|u_ii|))
        /// of the LU; the plain product is within rounding of it.
        public static double Det2(double a00, double a01, double a10, double a11) => a00 * a11 - a01 * a10;

        // ---------------------------------------------------------------------------------------- np.random.default_rng(seed)
        /// numpy's Generator(PCG64(SeedSequence(seed))): random() and choice(a, size, p=...).
        public sealed class Rng
        {
            ulong _hi, _lo, _incHi, _incLo;
            const ulong MulHi = 0x2360ED051FC65DA4UL, MulLo = 0x4385DF649FCCF645UL;

            public Rng(uint seed)
            {
                // SeedSequence(seed).generate_state(4, uint64)
                const uint INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded, MIX_L = 0xca01f9dd, MIX_R = 0x4973f715;
                var pool = new uint[4]; uint hc = INIT_A;
                uint HashMix(uint value) { unchecked { value ^= hc; hc *= MULT_A; value *= hc; value ^= value >> 16; return value; } }
                uint Mix(uint x, uint y) { unchecked { uint r = MIX_L * x - MIX_R * y; r ^= r >> 16; return r; } }
                for (int i = 0; i < 4; i++) pool[i] = HashMix(i == 0 ? seed : 0u);
                for (int s = 0; s < 4; s++) for (int d = 0; d < 4; d++) if (s != d) pool[d] = Mix(pool[d], HashMix(pool[s]));
                var st = new uint[8]; uint hb = INIT_B;
                unchecked
                {
                    for (int i = 0; i < 8; i++) { uint v = pool[i % 4]; v ^= hb; hb *= MULT_B; v *= hb; v ^= v >> 16; st[i] = v; }
                }
                ulong W(int i) => st[2 * i] | (ulong)st[2 * i + 1] << 32;
                ulong seedHi = W(0), seedLo = W(1), incHi = W(2), incLo = W(3);
                // pcg_setseq_128_srandom_r
                _hi = 0; _lo = 0;
                _incHi = incHi << 1 | incLo >> 63; _incLo = incLo << 1 | 1;
                Step();
                unchecked { ulong lo = _lo + seedLo; _hi = _hi + seedHi + (lo < _lo ? 1UL : 0UL); _lo = lo; }
                Step();
            }
            static void Mul64(ulong a, ulong b, out ulong hi, out ulong lo)
            {
                unchecked
                {
                    ulong a0 = (uint)a, a1 = a >> 32, b0 = (uint)b, b1 = b >> 32;
                    ulong p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
                    ulong mid = (p00 >> 32) + (uint)p01 + (uint)p10;
                    lo = (uint)p00 | mid << 32;
                    hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
                }
            }
            void Step()
            {
                unchecked
                {
                    Mul64(_lo, MulLo, out ulong hi, out ulong lo);
                    hi += _lo * MulHi + _hi * MulLo;
                    ulong nlo = lo + _incLo;
                    hi += _incHi + (nlo < lo ? 1UL : 0UL);
                    _hi = hi; _lo = nlo;
                }
            }
            public ulong Next64()
            {
                Step();
                ulong x = _hi ^ _lo; int rot = (int)(_hi >> 58);
                return rot == 0 ? x : x >> rot | x << (64 - rot);
            }
            /// Generator.random(): a double in [0, 1).
            public double Random() => (Next64() >> 11) * (1.0 / 9007199254740992.0);
            /// Generator.choice(n values, size, p=p) with replacement: the indices (cdf = cumsum(p) / its last value, searchsorted right).
            public int[] ChoiceIndices(double[] p, int size)
            {
                var cdf = new double[p.Length]; double s = 0;
                for (int i = 0; i < p.Length; i++) { s = i == 0 ? p[0] : s + p[i]; cdf[i] = s; }
                double last = cdf[p.Length - 1];
                for (int i = 0; i < p.Length; i++) cdf[i] /= last;
                var idx = new int[size];
                for (int k = 0; k < size; k++)
                {
                    double u = Random();
                    int lo = 0, hi = cdf.Length;                      // the first i with cdf[i] > u
                    while (lo < hi) { int mid = (lo + hi) >> 1; if (cdf[mid] <= u) lo = mid + 1; else hi = mid; }
                    idx[k] = lo;
                }
                return idx;
            }
        }
    }

    // ====================================================================================================================== options
    /// The options of ux_race.build_race. The defaults are what ux_build_track passes for a stock-base sx build
    /// (--dropgate-materials template is the stock base's default; the others are the tools' defaults).
    public sealed class UxRaceOptions
    {
        /// slot_name: null = U.slot_name('sx', DEFAULT_SLOT) = Beta_Sx_Track_Slot_1.
        public string slotName;
        /// --minimal: only the size word (no 44-49, no G8).
        public bool minimal;
        /// --sx-flags N: section field 19 of every lap and chute section set to N (null = unchanged).
        public long? sxFlags;
        /// --no-board-flip = false.
        public bool boardFlip = true;
        /// --gate-patch auto | off.
        public string gatePatch = "auto";
        /// --tdf-pad zero | keep.
        public string tdfPad = "zero";
        /// --lappacing none | untamed (untamed: the lap pacing source bytes come from the disc's AI folder or lapPacingSource).
        public string lapPacing = "none";
        /// The bytes of AI/<Level>-lappacing.bxml when the disc source does not list it (an extracted folder opened by UxDisc.Open).
        public byte[] lapPacingSource;
        /// --deck-norespawn engine | auto | off.
        public string deckRespawn = "engine";
        /// --bale-edge-clear FT (0 = off).
        public double baleEdgeClear;
        /// --dropgate-materials stock9 | template.
        public string dropgateMaterials = "template";
        /// --startgate-visual keep | hide.
        public string startgateVisual = "keep";

        /// The options the current builds give the race stage (reference work/combined/commands.json, 2026-11-11): the per-track data.
        public static readonly KeyValuePair<string, (double baleEdgeClear, string startgateVisual)>[] TrackOptions =
        {
            new KeyValuePair<string, (double, string)>("Capital_City_STADIUM", (4.0, "keep")),
            new KeyValuePair<string, (double, string)>("Columbus_STADIUM", (4.0, "keep")),
            new KeyValuePair<string, (double, string)>("Denton_STADIUM", (4.0, "keep")),
            new KeyValuePair<string, (double, string)>("Wilford_STADIUM", (4.0, "keep")),
            new KeyValuePair<string, (double, string)>("Foothills_STADIUM", (0.0, "hide")),
        };
        /// The options of a level's current build (TrackOptions; every other level: the defaults).
        public static UxRaceOptions ForLevel(UxLevelRow lv)
        {
            var o = new UxRaceOptions();
            foreach (var kv in TrackOptions) if (kv.Key == lv.disc) { o.baleEdgeClear = kv.Value.baleEdgeClear; o.startgateVisual = kv.Value.startgateVisual; }
            return o;
        }
    }

    /// What build_race returns besides the race dict: the bytes ux_build_track takes from '_tdf_bytes', '_assets' and '_lappacing'.
    public sealed class UxRaceResult
    {
        /// The race dict as written to race.json (with 'assets' and the tdf 'file').
        public JObj race;
        public byte[] tdf;
        /// The start-gate patch assets in insertion order: (name, type, bytes).
        public readonly List<(string name, string type, byte[] data)> assets = new List<(string, string, byte[])>();
        public byte[] lapPacing;
        public double seconds;
    }

    // ====================================================================================================================== template
    /// ux_race.Template on a stock clone base (ux_base: SX_FortDodge reduced to the race / free-ride kit): the kept scene entities by
    /// name, the GL_StartGate nodes, the base's height field and tdf. Read once and shared by every level (never modified).
    public sealed class UxRaceTemplate
    {
        public sealed class Entity { public string name, type, sub; public double[] matrix; }
        public readonly string level, slot;
        // ---- ux_base.STOCK_POLICY[level]['roles']                                                       (same as UxBase.STOCK_POLICY)
        public readonly string dropgate = "DropGate", startgate = "TO_StartingGate", board_camera = "SX_FD_AF_Camera_30Second";
        public readonly string[] gate_patterns = { @"^Anchor_StartLine$", @"^Anchor_StartLine_(LensFlare|TrophyGirl|FreeStyleGirl)$", @"^Anchor_LoseWalk", @"^Anchor_LoseHeatChief_HumanPlayerLost" };
        public readonly string[] board_patterns = { @"^Anchor_30SecGirl", @"^Anchor_StartLine_(30SecGirl|CardGirl|CheckGirl|CameraGuyStrafe)$" };
        public readonly string[] startgate_followers = { "SX_FD_AF_Camera_Startline" };

        public readonly List<Entity> entities = new List<Entity>();
        readonly Dictionary<string, Entity> _byName = new Dictionary<string, Entity>(StringComparer.Ordinal);
        /// props[name]: the board's world-position props (ShowSignPosition, WalkOnSpeed, WalkOffSpeed).
        public readonly Dictionary<string, JObj> props = new Dictionary<string, JObj>(StringComparer.Ordinal);
        public readonly List<object> dupes = new List<object>();
        public readonly UxTrackSet ts;
        public readonly JObj sg_props = new JObj(), dg_props = new JObj();
        public readonly byte[] gate_model; public readonly string gate_model_md5, scene_md5;
        public readonly double[] hf; public readonly int hfRows, hfCols; public readonly double floor;
        public readonly byte[] tdf; public readonly UxTdfLine line9;

        // ---- ux_base.STOCK_POLICY['SX_FortDodge']: what prepare() keeps                               (same as UxBase.keep_entity)
        static readonly HashSet<string> SingleTypes = new HashSet<string>(StringComparer.Ordinal)
        {
            "HFTerrainRSData", "ShadowMapRSData", "TDFEntityRSData", "AmbientDirectionalLightRSData", "SkyRSData", "HDRSettingsRSData", "MudManagerRSData",
            "VolumeFogRSData", "ReflectionMapGenRSData", "OverlayLightRSData", "ParticleManagerRSData", "AudioCollisionManagerRSData",
            "AudioTerrainZoneManagerRSData", "LevelAmbientAudioZoneRSData", "AudioVolumeLevelMixRSData", "LensFlareRSData", "DropGateRSData",
            "ThirtySecondBoardRSData",
        };
        static readonly Dictionary<string, HashSet<string>> Named = BuildNamed();
        static Dictionary<string, HashSet<string>> BuildNamed()
        {
            var anchors = new HashSet<string>(StringComparer.Ordinal) { "Anchor_Intro", "Anchor_30SecGirl", "Anchor_StartLine", "Anchor_WinFinish", "Anchor_LoseHeatChief_HumanPlayerLost1", "Anchor_LoseWalk" };
            for (int i = 1; i < 12; i++) anchors.Add("Anchor_30SecGirl_CameraHound" + i.ToString("00", CultureInfo.InvariantCulture));
            foreach (string s in new[] { "TrophyGirl", "LensFlare", "FreeStyleGirl", "CheckGirl", "CardGirl", "CameraGuyStrafe", "30SecGirl" }) anchors.Add("Anchor_StartLine_" + s);
            for (int i = 4; i < 13; i++) { anchors.Add("Anchor_LoseWalk_PlayerCC" + i.ToString(CultureInfo.InvariantCulture)); anchors.Add("Anchor_LoseWalk_PlayerFinished" + i.ToString(CultureInfo.InvariantCulture)); }
            var actors = new HashSet<string>(StringComparer.Ordinal) { "CameraPerson01", "CameraPerson02", "CameraGuyStrafe_B", "SC_TrackSideFlagHolder", "30SecGirl", "CardGirl", "CheckGirl", "FreeStyleGirl", "TrophyGirl" };
            for (int i = 1; i < 13; i++)
                foreach (string p in new[] { "CameraHound_", "PitCrewPerson_", "ScoreTowerPerson_" }) actors.Add(p + i.ToString("00", CultureInfo.InvariantCulture));
            for (int i = 1; i < 16; i++) if (i != 8) actors.Add("CrowdPerson" + i.ToString("00", CultureInfo.InvariantCulture));
            return new Dictionary<string, HashSet<string>>(StringComparer.Ordinal)
            {
                { "CollisionRSData", new HashSet<string>(StringComparer.Ordinal) { "TO_StartingGate" } },
                { "AnchorEntityRSData", anchors },
                { "ScenarioAnimEntityRSData", actors },
                { "MXCameraPathRSData", new HashSet<string>(StringComparer.Ordinal) { "SX_FD_AF_Camera_Startline", "SX_FD_AF_Camera_30Second" } },
                { "GeneralStorageEntityRSData", new HashSet<string>(StringComparer.Ordinal) { "CAMANIMS_Intro_StorageEntity", "GLOBAL_CrewChief_ALL_StorageEntity", "SC_EventPeopleStorageEntity" } },
            };
        }
        static readonly (string suffix, int count)[] Expect = { ("Base", 139), ("FreeRide", 12), ("SectionRace", 1) };

        static string NameOf(UxElem e)
        {
            var p = UxPcScene.Prop(e, "Name");
            return p != null && p.children.Count > 0 ? p.children[0].value as string : null;
        }
        /// keep_entity(pol, suffix, e).
        static bool KeepEntity(string suffix, UxElem e)
        {
            string t = UxPcScene.Ns(e), nm = NameOf(e);
            if (suffix == "FreeRide") return t == "StartEntityFreeRideRSData";
            if (suffix == "SectionRace") return t == "LocalizedPackageReferenceEntityRSData";
            if (suffix != "Base") return false;
            if (SingleTypes.Contains(t)) return true;
            if (t == "CrewChiefRSData") return true;
            if (t == "HFBumpModelRSData")
            {
                var p = UxPcScene.Prop(e, "ModelAsset");
                return p != null && p.children.Count > 0 && p.children[0].AttrStr("name") == "SX_FD_MainBump";
            }
            return nm != null && Named.TryGetValue(t, out var names) && names.Contains(nm);
        }

        /// Template(templates/stock-<level>): the base's scene, package assets and tdf from the player's Reflex library.
        public UxRaceTemplate(UxStock stock, string level = UxStock.DEFAULT_STOCK_LEVEL)
        {
            UxBin.Require(level == "SX_FortDodge", "no stock base policy for " + level + " (known: ['SX_FortDodge'])");
            this.level = level; slot = level;
            byte[] scene = stock.BaseFile("scene", level);
            scene_md5 = UxBin.Md5(scene);
            var subs = UxPcScene.Load(scene);
            // ---- ux_base.prepare: the kept sub-scenes and entities (the level file edit and the chunk regrouping are the builder's)
            var kept = new List<(UxSubScene sub, string suffix, List<UxElem> entities)>();
            foreach (var s in subs)
            {
                string suf = s.name.StartsWith(level, StringComparison.Ordinal) ? s.name.Substring(level.Length) : null;
                if (suf != "Base" && suf != "FreeRide" && suf != "SectionRace") continue;
                var list = new List<UxElem>();
                foreach (var e in s.Entities()) if (KeepEntity(suf, e)) list.Add(e);
                kept.Add((s, suf, list));
            }
            foreach (var (suffix, count) in Expect)
            {
                int got = 0; foreach (var k in kept) if (k.suffix == suffix) got += k.entities.Count;
                if (got != count) throw new UxException("stock base " + level + ": kept " + got + " " + suffix + " entities, policy expects " + count + " (stale policy?)");
            }
            List<UxElem> baseEntities = null;
            foreach (var k in kept) if (k.suffix == "Base") { baseEntities = k.entities; break; }
            UxBin.Require(baseEntities != null, "stock base " + level + ": no " + level + "Base sub-scene");
            var tb = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (var e in baseEntities) { string t = UxPcScene.Ns(e); tb[t] = tb.TryGetValue(t, out int c) ? c + 1 : 1; }
            foreach (string t in SingleTypes) if (!tb.TryGetValue(t, out int c) || c != 1) throw new UxException("stock base " + level + ": singletons not exactly once: " + t);
            if (!tb.ContainsKey("DistanceFogRSData"))
            {   // the synthesised DistanceFog (a clone of the stock prototype, named 'DistanceFog') joins the Base sub-scene's entities
                var proto = stock.LibPrototype("DistanceFogRSData").entity;
                var nameProp = UxPcScene.Prop(proto, "Name");
                UxBin.Require(nameProp != null && nameProp.children.Count == 1, "DistanceFogRSData prototype: no Name");
                nameProp.children[0].value = "DistanceFog";
                baseEntities.Add(proto);
            }
            // ---- Template.__init__
            foreach (var k in kept)
                foreach (var e in k.entities)
                {
                    string nm = NameOf(e);
                    if (nm == null || UxPcScene.Prop(e, "Matrix") == null) continue;
                    if (_byName.ContainsKey(nm)) dupes.Add(nm);
                    else { var ent = new Entity { name = nm, type = UxPcScene.Ns(e), matrix = UxPcScene.GetMatrix(e), sub = k.sub.name }; entities.Add(ent); _byName[nm] = ent; }
                    if (UxPcScene.Ns(e) == "ThirtySecondBoardRSData" && !props.ContainsKey(nm))
                    {
                        var P = UxRace.PropsOf(e); var o = new JObj();
                        foreach (var kv in P) if (kv.Key == "ShowSignPosition" || kv.Key == "WalkOnSpeed" || kv.Key == "WalkOffSpeed") o.Set(kv.Key, UxRace.GetValue(kv.Value));
                        props[nm] = o;
                    }
                }
            ts = stock.BaseSet(level);
            // the first entity of that name in each sub-scene; a later sub-scene's replaces an earlier one's (the Python loops)
            foreach (var k in kept)
                foreach (var e in k.entities)
                {
                    if (NameOf(e) != "TO_StartingGate") continue;
                    var P = UxRace.PropsOf(e);
                    sg_props.items.Clear();
                    foreach (string key in new[] { "ModelAsset", "SurfaceAsset", "FemaleCollisionAsset", "MaleCollisionAsset" })
                        foreach (var kv in P) if (kv.Key == key) sg_props.Set(key, UxRace.GetValue(kv.Value));
                    break;
                }
            foreach (var k in kept)
                foreach (var e in k.entities)
                {
                    if (NameOf(e) != dropgate) continue;
                    var P = UxRace.PropsOf(e);
                    dg_props.items.Clear();
                    foreach (string key in new[] { "ModelAsset", "SurfaceAsset", "MaterialList" })
                        foreach (var kv in P) if (kv.Key == key) dg_props.Set(key, UxRace.GetValue(kv.Value));
                    break;
                }
            gate_model = ts.ReadLogical("GL_StartGate", "model");
            gate_model_md5 = UxBin.Md5(gate_model);
            // ---- ux_base.hf_roles: the HFTerrainRSData HeightMapAsset
            UxElem hfe = null; int nhf = 0;
            foreach (var e in baseEntities) if (UxPcScene.Ns(e) == "HFTerrainRSData") { hfe = e; nhf++; }
            if (nhf != 1) throw new UxException(nhf + " HFTerrainRSData in " + level + "Base");
            string hm = null;
            foreach (var kv in UxRace.PropsOf(hfe)) if (kv.Key == "HeightMapAsset") hm = UxJson.Get(UxRace.GetValue(kv.Value), "asset") as string;
            UxBin.Require(!string.IsNullOrEmpty(hm), "stock base " + level + ": the HFTerrainRSData has no HeightMapAsset");
            var tex = new UxPcTexture(ts.ReadLogical(hm, "texture"));
            hfCols = tex.recs[0].w; hfRows = tex.recs[0].h;
            UxBin.Require(tex.entries[0].Length >= 4 * hfRows * hfCols, "stock base height map " + hm + ": truncated");
            hf = new double[hfRows * hfCols]; double mn = double.PositiveInfinity;
            for (int i = 0; i < hf.Length; i++) { hf[i] = (double)UxBin.F32LE(tex.entries[0], 4 * i) * UxCommon.HEIGHT_SCALE; if (hf[i] < mn) mn = hf[i]; }
            floor = mn;
            tdf = ts.ReadLogical(slot + "-sx", "tdf");
            var sp = UxTdf07.Split(tdf);
            UxTdf07.Structure(sp.items, out var glob, out _);
            var r9 = UxTdf07.First(glob, 9);
            line9 = r9 != null ? UxTdf07.LineRec(r9) : null;
        }

        /// ground(x, z): the base's own height field.
        public double Ground(double x, double z)
        {
            int n = hfRows;
            double c = x + n / 2.0 - 0.5, r = n / 2.0 - 0.5 - z;
            int c0 = Math.Min(Math.Max((int)Math.Floor(c), 0), n - 2), r0 = Math.Min(Math.Max((int)Math.Floor(r), 0), n - 2);
            double fc = c - c0, fr = r - r0; var h = hf; int w = hfCols;
            return (h[r0 * w + c0] * (1 - fc) + h[r0 * w + c0 + 1] * fc) * (1 - fr) + (h[(r0 + 1) * w + c0] * (1 - fc) + h[(r0 + 1) * w + c0 + 1] * fc) * fr;
        }
        public Entity Find(string name) => _byName.TryGetValue(name, out var e) ? e : null;
        readonly Dictionary<string, byte[]> _read = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        /// read(name, typ): the logical bytes of a base package asset (kept: levels converted in parallel share the template).
        public byte[] Read(string name, string typ)
        {
            lock (_read)
            {
                if (!_read.TryGetValue(name + "." + typ, out var b)) _read[name + "." + typ] = b = ts.ReadLogical(name, typ);
                return b;
            }
        }
        /// match(patterns): the entity names any pattern finds (re.search), in entity order.
        public List<string> Match(string[] patterns)
        {
            var o = new List<string>();
            foreach (var e in entities)
                foreach (string p in patterns) if (Regex.IsMatch(e.name, p, RegexOptions.CultureInvariant)) { o.Add(e.name); break; }
            return o;
        }
    }

    // ====================================================================================================================== the stage
    /// ux_race.py.
    public static class UxRace
    {
        public const string SCHEMA = "ux_race/1", TOOL = "ux_race.py";
        /// The md5 of the reference tools this file was ported from (race.json 'source'; a changed reference shows as another md5).
        public const string TOOL_MD5 = "9a6a86b44fd92d4d75dd467c4f86c116", TDF07_TOOL_MD5 = "fec92f376f64776ed77e6fbe4171d7a0";
        const double START_GATE_BEHIND = 7.4, NODE_CLEAR_FT = 3.0, HF_TOL_FT = 1.0, GATE_LINE_TOL_FT = 3.0, LAP_TOL_FT = 0.1;
        const double GROUND_FOLLOW_MIN_FT = 5.0, GROUND_STAND_TOL_FT = 0.5, PARKED_Y = -1000.0;
        public const string GATE_MODEL_NAME = "UX_StartGate", GATE_COL_NAME = "UX_StartGate_COL";
        const double LANE_SPACING_FT = 5.0;
        static readonly double[] COL_BAND_FT = { 0.5, 1.5, 3.0, 4.5, 6.0, 8.0, 10.0, 12.0 };
        const double COL_MARGIN_FT = 0.25, COL_REACH_FT = 120.0, CORRIDOR_STEP_FT = 0.5;
        static readonly double[] LANE_BAND_FT = { 0.5, 1.5, 3.0, 4.5, 6.0 };
        const double LANE_HALF_FT = 1.25, LANE_AHEAD_FT = 30.0;
        public static readonly string[] STOCK9_DROPGATE_MATERIALS = { "SX_Sgate_Answer", "SX_Sgate_FMF", "SX_Sgate_ProCircuit", "SX_Sgate_KTM", "SX_Sgate_Yoshimura", "SX_Sgate_HiPer", "SX_Sgate_FMF", "SX_Sgate_Answer", "SX_StartMetal" };
        const double HIDE_Y_FT = -64.0;
        const double DECK_NORESPAWN_FT = 2.75, BAND_LO = 0.25, BAND_HI = 5.25, FOOT_ALONG = 3.0, FOOT_ACROSS = 1.0, FOOT_STEP = 0.5;
        internal static readonly string[] PHYSICS_FAMILY = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData" };   // same as UxEntities.PHYSICS_FAMILY

        static List<object> L(params object[] v) => new List<object>(v);
        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);

        // ================================================================================================== ux_entities helpers (private copies)
        /// props_of(entity): prop name -> prop RSData Elem, in order (a repeated name keeps its first place, the last element).      same as UxEntities.props_of
        internal static List<KeyValuePair<string, UxElem>> PropsOf(UxElem entity)
        {
            var o = new List<KeyValuePair<string, UxElem>>();
            foreach (var c in entity.children[0].children)
            {
                string k = c.Attr("name") as string;
                int at = o.FindIndex(x => x.Key == k);
                if (at >= 0) o[at] = new KeyValuePair<string, UxElem>(k, c); else o.Add(new KeyValuePair<string, UxElem>(k, c));
            }
            return o;
        }
        /// get_value(prop): the JSON-style value of a property RSData Elem.                                                          same as UxEntities.get_value
        internal static object GetValue(UxElem prop)
        {
            string ns = (prop.AttrStr("namespace") ?? "").Replace(UxCommon.NS_PREFIX, "");
            if (ns == "AssetRSData") { var a = prop.children[0]; return new JObj().Set("asset", a.Attr("name")).Set("type", a.Attr("type")); }
            if (ns == "AssetListRSData")
            {
                var l = new List<object>();
                foreach (var k in prop.children) l.Add(new JObj().Set("asset", k.children[0].Attr("name")).Set("type", k.children[0].Attr("type")));
                return l;
            }
            if (ns == "EnumRSData") return new JObj().Set("Enum", prop.Find("Enum").value).Set("EnumValue", (long)UxBin.I32LE((byte[])prop.Find("EnumValue").value, 0));
            if (prop.children.Count != 1) return null;
            var v = prop.children[0];
            if (v.vtype == 1) return v.value;
            var b = v.value as byte[];
            switch (v.vtype)
            {
                case 3: return (long)UxBin.I32LE(b, 0);
                case 4: case 11: return (long)UxBin.U32LE(b, 0);
                case 5: return (double)UxBin.F32LE(b, 0);
                case 7: case 8: case 10:
                {
                    int n = v.vtype == 7 ? 4 : v.vtype == 8 ? 16 : 3; var l = new List<object>(n);
                    for (int i = 0; i < n; i++) l.Add((double)UxBin.F32LE(b, 4 * i));
                    return l;
                }
            }
            return null;
        }

        /// mat4(m): the 4 x 4 float64 of a 16-number matrix, every element rounded to float32 first.                                same as UxEntities.mat4
        internal static double[] Mat4(IList<object> m)
        {
            UxBin.Require(m != null && m.Count == 16, "a matrix needs 16 numbers");
            var r = new double[16];
            for (int i = 0; i < 16; i++) r[i] = (double)(float)UxJson.Num(m[i]);
            return r;
        }
        internal static double[] Mat4(double[] m) { var r = new double[16]; for (int i = 0; i < 16; i++) r[i] = (double)(float)m[i]; return r; }
        /// f32list(a).                                                                                                               same as UxEntities.f32list
        internal static List<object> F32List(double[] a) { var l = new List<object>(a.Length); foreach (double x in a) l.Add((double)(float)x); return l; }
        /// unscaled(M): the rows of the rotation divided by their length.                                                            same as UxEntities.unscaled
        internal static double[] Unscaled(double[] M)
        {
            var R = Mat4(M);
            for (int i = 0; i < 3; i++)
            {
                double n = UxRaceNum.Norm3(R[4 * i], R[4 * i + 1], R[4 * i + 2]);
                if (n > 0) { R[4 * i] /= n; R[4 * i + 1] /= n; R[4 * i + 2] /= n; }
            }
            return R;
        }
        static double[] Eye4() => new double[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        static double[] RotY180() { var R = Eye4(); R[0] = -1; R[10] = -1; return R; }
        /// yaw_deg(M) = degrees(atan2(M[2,0], M[2,2])) of the float32-rounded matrix.
        static double YawDeg(double[] M) { var m = Mat4(M); return Math.Atan2(m[8], m[10]) * (180.0 / Math.PI); }
        /// flat_dir(v): the horizontal unit vector (x, 0, z), or (x, 0, z) itself when shorter than 1e-9.
        static double[] FlatDir(double x, double z)
        {
            double n = UxRaceNum.Norm3(x, 0.0, z);
            return n > 1e-9 ? new[] { x / n, 0.0 / n, z / n } : new[] { x, 0.0, z };
        }
        /// np.cross(a, b) for 3-vectors.
        static double[] Cross(double[] a, double[] b) => new[] { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
        /// line_frame(pos, d): z = the travel direction (horizontal), y up, x = up x z, origin = pos at y 0.
        static double[] LineFrame(double[] pos, double[] d)
        {
            var z = FlatDir(d[0], d[2]); var y = new[] { 0.0, 1.0, 0.0 }; var x = Cross(y, z);
            var M = Eye4();
            M[0] = x[0]; M[1] = x[1]; M[2] = x[2]; M[4] = y[0]; M[5] = y[1]; M[6] = y[2]; M[8] = z[0]; M[9] = z[1]; M[10] = z[2];
            M[12] = pos[0]; M[13] = 0.0; M[14] = pos[2];
            return M;
        }
        /// seg_dist(p, a, b): the xz distance from p to the segment ab.
        static double SegDist(double[] p, double[] a, double[] b)
        {
            double ab0 = b[0] - a[0], ab1 = b[1] - a[1];
            double Lq = UxRaceNum.Dot2(ab0, ab1, ab0, ab1);
            double t;
            if (Lq < 1e-12) t = 0.0;
            else
            {
                double v = UxRaceNum.Dot2(p[0] - a[0], p[1] - a[1], ab0, ab1) / Lq;
                double mx = v > 0.0 ? v : 0.0;                         // max(0.0, v)
                t = mx < 1.0 ? mx : 1.0;                               // min(1.0, ...)
            }
            double d0 = p[0] - (a[0] + t * ab0), d1 = p[1] - (a[1] + t * ab1);
            return UxRaceNum.Norm2(d0, d1);
        }
        static bool InPoly(double x, double z, double[][] poly)
        {
            bool inside = false; int n = poly.Length;
            for (int i = 0; i < n; i++)
            {
                double x1 = poly[i][0], z1 = poly[i][1], x2 = poly[(i + 1) % n][0], z2 = poly[(i + 1) % n][1];
                double dz = z2 - z1; if (dz == 0) dz = 1e-30;
                if ((z1 > z) != (z2 > z) && x < (x2 - x1) * (z - z1) / dz + x1) inside = !inside;
            }
            return inside;
        }

        /// One entity of level.json (the fields the race kit reads).
        internal sealed class LevelEntity
        {
            public long id; public string cls, type, reflex, name; public bool collision; public JObj props, raw; public List<object> matrix;
            public bool Kept => cls == "keep" || cls == "demote";
            public string Asset(string prop) => UxJson.Get(props, prop, "asset") as string;
        }
        internal static List<LevelEntity> Entities(JObj d)
        {
            var o = new List<LevelEntity>();
            foreach (object x in UxJson.Arr(d.Get("entities")))
            {
                var e = UxJson.Obj(x);
                object col = e.Get("collision");
                o.Add(new LevelEntity
                {
                    raw = e, id = UxJson.Int(e.Get("id")), cls = e.Get("cls") as string, type = e.Get("type") as string, reflex = e.Get("reflex") as string, name = e.Get("name") as string,
                    collision = col is bool b ? b : col != null && !(col is long l && l == 0), props = e.Get("props") as JObj ?? new JObj(), matrix = e.Get("matrix") as List<object>,
                });
            }
            return o;
        }
        /// ExtractIndex.rec(name, typ): the closure record, by name ignoring case.                                                   same as UxEntities.ExtractIndex
        internal sealed class ExtractIndex
        {
            readonly Dictionary<string, Dictionary<string, JObj>> _low = new Dictionary<string, Dictionary<string, JObj>>(StringComparer.Ordinal);
            public ExtractIndex(JObj level_json)
            {
                foreach (var t in UxJson.Obj(level_json.Get("closure")).items)
                {
                    var m = new Dictionary<string, JObj>(StringComparer.Ordinal);
                    foreach (var kv in UxJson.Obj(t.Value).items) m[UxCommon.PyLower(kv.Key)] = UxJson.Obj(kv.Value);
                    _low[t.Key] = m;
                }
            }
            public JObj Rec(string name, string typ) => name != null && _low.TryGetValue(typ, out var m) && m.TryGetValue(UxCommon.PyLower(name), out var r) ? r : null;
        }

        /// A uniform xz grid over triangle bounding boxes: the candidates of a point query (numpy tests every triangle; the exact
        /// test is the caller's, so the result is the same set).
        internal sealed class TriGrid
        {
            readonly double _ox, _oz, _inv; readonly int _nx, _nz; readonly int[] _start, _items;
            public TriGrid(double[] xmin, double[] xmax, double[] zmin, double[] zmax, double eps, double cell = 4.0)
            {
                int n = xmin.Length; double x0 = double.PositiveInfinity, x1 = double.NegativeInfinity, z0 = x0, z1 = x1;
                var ok = new bool[n];
                for (int i = 0; i < n; i++)
                {
                    ok[i] = UxNp.Finite(xmin[i], xmax[i], zmin[i], zmax[i]);
                    if (!ok[i]) continue;
                    if (xmin[i] - eps < x0) x0 = xmin[i] - eps; if (xmax[i] + eps > x1) x1 = xmax[i] + eps;
                    if (zmin[i] - eps < z0) z0 = zmin[i] - eps; if (zmax[i] + eps > z1) z1 = zmax[i] + eps;
                }
                if (double.IsInfinity(x0)) { x0 = z0 = 0; x1 = z1 = 1; }
                _ox = x0; _oz = z0; _inv = 1.0 / cell;
                _nx = Math.Max(1, Math.Min(4096, (int)Math.Floor((x1 - x0) * _inv) + 1)); _nz = Math.Max(1, Math.Min(4096, (int)Math.Floor((z1 - z0) * _inv) + 1));
                var count = new int[_nx * _nz + 1];
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int i = 0; i < n; i++)
                    {
                        if (!ok[i]) continue;
                        int cx0 = Cx(xmin[i] - eps), cx1 = Cx(xmax[i] + eps), cz0 = Cz(zmin[i] - eps), cz1 = Cz(zmax[i] + eps);
                        for (int cz = cz0; cz <= cz1; cz++)
                            for (int cx = cx0; cx <= cx1; cx++)
                            {
                                int c = cz * _nx + cx;
                                if (pass == 0) count[c + 1]++; else _items[count[c]++] = i;
                            }
                    }
                    if (pass == 0)
                    {
                        for (int c = 0; c < _nx * _nz; c++) count[c + 1] += count[c];
                        _start = (int[])count.Clone(); _items = new int[count[_nx * _nz]];
                    }
                }
            }
            int Cx(double v) { double f = Math.Floor((v - _ox) * _inv); return f <= 0 ? 0 : f >= _nx - 1 ? _nx - 1 : (int)f; }
            int Cz(double v) { double f = Math.Floor((v - _oz) * _inv); return f <= 0 ? 0 : f >= _nz - 1 ? _nz - 1 : (int)f; }
            /// The candidate triangle indices (ascending) are items[from .. to).
            public int[] Query(double x, double z, out int from, out int to)
            {
                if (double.IsNaN(x) || double.IsNaN(z)) { from = to = 0; return _items; }
                int c = Cz(z) * _nx + Cx(x);
                from = _start[c]; to = _start[c + 1];
                return _items;
            }
        }

        internal struct Surf { public double y; public int tri, mat; public double ny; }

        /// TerrainQuery: point queries on the Untamed terrain collision, in the Untamed frame (no lift).                             same as UxEntities.TerrainQuery
        internal sealed class TerrainQuery
        {
            public readonly List<string> mats = new List<string>();
            public readonly int[] tri_mat; public readonly double[] A, B, C, ny, xmin, xmax, zmin, zmax; public readonly bool[] kept;
            public readonly int Count;
            readonly TriGrid _grid;
            public TerrainQuery(UxLevelRow lv, UxDiscLevel disc, JObj d, CancellationToken cancel)
            {
                var t = UxJson.Obj(d.Get("terrain"));
                var idx = new ExtractIndex(d);
                var tr = idx.Rec(t.Get("terrain") as string, "terrain") ?? throw new UxException("level.json: the terrain is not in the closure");
                var tc = idx.Rec(t.Get("trncol") as string, "trncol") ?? throw new UxException("level.json: the trncol is not in the closure");
                var T = UxTerrain.ReadTerrain(disc.ReadExtractFile(UxJson.Str(tr.Get("file")), cancel));
                var Cc = UxTerrain.ReadTrncol(disc.ReadExtractFile(UxJson.Str(tc.Get("file")), cancel));
                foreach (object m in UxJson.Arr(t.Get("materials"))) mats.Add(UxJson.Str(m));
                tri_mat = UxTerrain.CollisionMaterials(T, Cc);
                // design_exclusions: '*Over*' decks on every level; on the EC levels Rocks / Logs and the collision-only triangles
                bool ec = lv.mode == "ec";
                var excl = new HashSet<int>();
                for (int k = 0; k < mats.Count; k++)
                    if (Regex.IsMatch(mats[k], "over", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant) || ec && Regex.IsMatch(mats[k], "rock|log", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)) excl.Add(k);
                if (ec) excl.Add(-1);
                int n = Cc.tris.Length / 3; Count = n;
                A = new double[3 * n]; B = new double[3 * n]; C = new double[3 * n]; ny = new double[n];
                xmin = new double[n]; xmax = new double[n]; zmin = new double[n]; zmax = new double[n]; kept = new bool[n];
                for (int i = 0; i < n; i++)
                {
                    int ia = 3 * Cc.tris[3 * i], ib = 3 * Cc.tris[3 * i + 1], ic = 3 * Cc.tris[3 * i + 2];
                    for (int k = 0; k < 3; k++) { A[3 * i + k] = Cc.verts[ia + k]; B[3 * i + k] = Cc.verts[ib + k]; C[3 * i + k] = Cc.verts[ic + k]; }
                    var N = Cross(new[] { B[3 * i] - A[3 * i], B[3 * i + 1] - A[3 * i + 1], B[3 * i + 2] - A[3 * i + 2] }, new[] { C[3 * i] - A[3 * i], C[3 * i + 1] - A[3 * i + 1], C[3 * i + 2] - A[3 * i + 2] });
                    double Ln = Math.Sqrt(UxRaceNum.Sum3(N[0] * N[0], N[1] * N[1], N[2] * N[2]));
                    ny[i] = Ln > 0 ? N[1] / Math.Max(Ln, 1e-30) : 0.0;
                    xmin[i] = Math.Min(Math.Min(A[3 * i], B[3 * i]), C[3 * i]); xmax[i] = Math.Max(Math.Max(A[3 * i], B[3 * i]), C[3 * i]);
                    zmin[i] = Math.Min(Math.Min(A[3 * i + 2], B[3 * i + 2]), C[3 * i + 2]); zmax[i] = Math.Max(Math.Max(A[3 * i + 2], B[3 * i + 2]), C[3 * i + 2]);
                    kept[i] = !excl.Contains(tri_mat[i]);
                }
                _grid = new TriGrid(xmin, xmax, zmin, zmax, 1e-5);      // wider than the query's 1e-6: the exact test below decides
            }
            /// surfaces(x, z): every trncol surface over (x, z), the highest first.
            public List<Surf> Surfaces(double x, double z)
            {
                var o = new List<Surf>();
                double xp = x + 1e-6, xm = x - 1e-6, zp = z + 1e-6, zm = z - 1e-6;
                var items = _grid.Query(x, z, out int from, out int to);
                for (int q = from; q < to; q++)
                {
                    int t = items[q];
                    if (!(xmin[t] <= xp && xmax[t] >= xm && zmin[t] <= zp && zmax[t] >= zm)) continue;
                    double a0 = A[3 * t], a1 = A[3 * t + 1], a2 = A[3 * t + 2], b0 = B[3 * t], b1 = B[3 * t + 1], b2 = B[3 * t + 2], c0 = C[3 * t], c1 = C[3 * t + 1], c2 = C[3 * t + 2];
                    double det = (b0 - a0) * (c2 - a2) - (c0 - a0) * (b2 - a2);
                    if (Math.Abs(det) < 1e-12) continue;
                    double l1 = ((x - a0) * (c2 - a2) - (c0 - a0) * (z - a2)) / det;
                    double l2 = ((b0 - a0) * (z - a2) - (x - a0) * (b2 - a2)) / det;
                    double l0 = 1 - l1 - l2;
                    if (Math.Min(Math.Min(l0, l1), l2) < -1e-7) continue;
                    o.Add(new Surf { y = l0 * a1 + l1 * b1 + l2 * c1, tri = t, mat = tri_mat[t], ny = ny[t] });
                }
                o.Sort((p, q) => p.y != q.y ? q.y.CompareTo(p.y) : q.tri.CompareTo(p.tri));      // sorted(out, reverse=True)
                return o;
            }
            /// top(x, z, kept_only): the highest (kept) surface, or null.
            public Surf? Top(double x, double z, bool kept_only = true)
            {
                foreach (var s in Surfaces(x, z)) if (!kept_only || kept[s.tri]) return s;
                return null;
            }
        }

        /// HeightField: the ground for placements (ft, lifted): terrain/hf.npy, bilinear; without it the highest kept trncol surface.   same as UxEntities.HeightField
        internal sealed class HeightField
        {
            public readonly double[] h; public readonly string source; readonly Func<TerrainQuery> _tq; readonly double _lift;
            public HeightField(UxLevelRow lv, UxWork work, Func<TerrainQuery> tq)
            {
                _tq = tq; _lift = UxCommon.YLift(lv);
                if (work.Exists("terrain/hf.npy"))
                {
                    byte[] raw = work.ReadBytes("terrain/hf.npy");
                    var a = Npy.Read(raw);
                    if (a.shape.Length != 2 || a.shape[0] != UxCommon.HF_SIZE || a.shape[1] != UxCommon.HF_SIZE)
                        throw new UxException("terrain/hf.npy: shape (" + string.Join(", ", a.shape) + ")");
                    h = a.AsDouble();
                    source = "WP6 terrain/hf.npy (md5 " + UxBin.Md5(raw) + ")";
                }
                else source = "stub: highest kept trncol surface + " + UxRaceNum.PyFixed(_lift, 0) + " ft (WP6 hf.npy missing)";
            }
            /// hf(x, z): null when the stub finds no surface.
            public double? At(double x, double z)
            {
                if (h != null)
                {
                    int n = UxCommon.HF_SIZE;
                    double c = x + n / 2.0 - 0.5, r = n / 2.0 - 0.5 - z;
                    double fl_c = Math.Floor(c), fl_r = Math.Floor(r);
                    UxBin.Require(UxNp.Finite(fl_c, fl_r), "height field: a non-finite position");
                    int c0 = fl_c < -1e9 ? -1000000000 : fl_c > 1e9 ? 1000000000 : (int)fl_c, r0 = fl_r < -1e9 ? -1000000000 : fl_r > 1e9 ? 1000000000 : (int)fl_r;
                    double fc = c - fl_c, fr = r - fl_r;
                    c0 = Math.Min(Math.Max(c0, 0), n - 2); r0 = Math.Min(Math.Max(r0, 0), n - 2);
                    return (h[r0 * n + c0] * (1 - fc) + h[r0 * n + c0 + 1] * fc) * (1 - fr) + (h[(r0 + 1) * n + c0] * (1 - fc) + h[(r0 + 1) * n + c0 + 1] * fc) * fr;
                }
                var s = _tq().Top(x, z);
                return s == null ? (double?)null : s.Value.y + _lift;
            }
            /// hf_vec(hf, x, z)[i]: the vectorised form (clamped fractions); NaN where the stub has no surface.
            public double AtVec(double x, double z)
            {
                if (h == null) { var g = At(x, z); return g ?? UxNp.NaN; }
                int n = UxCommon.HF_SIZE;
                double c = x + n / 2.0 - 0.5, r = n / 2.0 - 0.5 - z;
                double fl_c = Math.Floor(c), fl_r = Math.Floor(r);
                int c0 = (int)UxNp.Clip(fl_c, 0, n - 2), r0 = (int)UxNp.Clip(fl_r, 0, n - 2);
                double fc = UxNp.Clip(c - c0, 0, 1), fr = UxNp.Clip(r - r0, 0, 1);
                return (h[r0 * n + c0] * (1 - fc) + h[r0 * n + c0 + 1] * fc) * (1 - fr) + (h[(r0 + 1) * n + c0] * (1 - fc) + h[(r0 + 1) * n + c0 + 1] * fc) * fr;
            }
        }

        /// A .npy array (the stages' hf.npy '<f4' and excluded_tris.npy '<i4'): C order, little endian.
        internal sealed class Npy
        {
            public string descr; public int[] shape; public byte[] data; public int offset;
            public static Npy Read(byte[] b)
            {
                UxBin.Require(b.Length >= 10 && b[0] == 0x93 && b[1] == (byte)'N' && b[2] == (byte)'U' && b[3] == (byte)'M' && b[4] == (byte)'P' && b[5] == (byte)'Y', "not a .npy file");
                int hl, at;
                if (b[6] == 1) { hl = UxBin.U16LE(b, 8); at = 10; } else { hl = (int)UxBin.U32LE(b, 8); at = 12; }
                UxBin.Require(at + hl <= b.Length, ".npy: header outside the file");
                string hdr = UxBin.Latin1(b, at, hl);
                var md = Regex.Match(hdr, @"'descr'\s*:\s*'([^']*)'"); var ms = Regex.Match(hdr, @"'shape'\s*:\s*\(([^)]*)\)"); var mf = Regex.Match(hdr, @"'fortran_order'\s*:\s*(True|False)");
                UxBin.Require(md.Success && ms.Success && mf.Success && mf.Groups[1].Value == "False", ".npy: unsupported header " + hdr.Trim());
                var dims = new List<int>();
                foreach (string p in ms.Groups[1].Value.Split(',')) if (p.Trim().Length > 0) dims.Add(int.Parse(p.Trim(), CultureInfo.InvariantCulture));
                return new Npy { descr = md.Groups[1].Value, shape = dims.ToArray(), data = b, offset = at + hl };
            }
            public long Count { get { long n = 1; foreach (int d in shape) n *= d; return n; } }
            int ItemSize => descr == "<f4" || descr == "<i4" || descr == "<u4" ? 4 : descr == "<f8" || descr == "<i8" || descr == "<u8" ? 8 : descr == "|u1" || descr == "|i1" ? 1 : throw new UxException(".npy: dtype " + descr);
            public double[] AsDouble()
            {
                long n = Count; UxBin.Require(offset + n * ItemSize <= data.Length, ".npy: data outside the file");
                var o = new double[n];
                for (int i = 0; i < n; i++)
                    o[i] = descr == "<f4" ? UxBin.F32LE(data, offset + 4 * i) : descr == "<f8" ? BitConverter.Int64BitsToDouble((long)UxBin.U64(data, offset + 8 * i, false)) : AsLongAt(i);
                return o;
            }
            long AsLongAt(int i)
            {
                switch (descr)
                {
                    case "<i4": return UxBin.I32LE(data, offset + 4 * i);
                    case "<u4": return UxBin.U32LE(data, offset + 4 * i);
                    case "<i8": case "<u8": return (long)UxBin.U64(data, offset + 8 * i, false);
                    case "|u1": return data[offset + i];
                    case "|i1": return (sbyte)data[offset + i];
                }
                throw new UxException(".npy: dtype " + descr + " is not an integer type");
            }
            public long[] AsLong()
            {
                long n = Count; UxBin.Require(offset + n * ItemSize <= data.Length, ".npy: data outside the file");
                var o = new long[n];
                for (int i = 0; i < n; i++) o[i] = AsLongAt(i);
                return o;
            }
        }

        /// CollisionWorld: the world (lifted) collision triangles of the kept collision entities.                                    same as UxEntities.CollisionWorld
        internal sealed class CollisionWorld
        {
            public double[] tris = new double[0]; public long[] owner = new long[0]; public double[] lo = new double[0], hi = new double[0];
            public int Count => owner.Length;
            public CollisionWorld(UxLevelRow lv, UxDiscLevel disc, JObj d, List<LevelEntity> ents, ExtractIndex idx, Dictionary<string, double[]> cache, ICollection<long> exclude_ids, CancellationToken cancel)
            {
                double lift = UxCommon.YLift(lv);
                var parts = new List<double[]>(); var own = new List<long>(); int total = 0;
                foreach (var e in ents)
                {
                    if (!e.Kept || !e.collision || exclude_ids != null && exclude_ids.Contains(e.id)) continue;
                    string fem = e.Asset("FemaleCollisionAsset");
                    var rec = fem != null && fem.Length > 0 ? idx.Rec(fem, "collis") : null;
                    if (rec == null) continue;
                    var t = CollisTris(disc, UxJson.Str(rec.Get("file")), cache, cancel);
                    if (t == null || t.Length == 0) continue;
                    var M = Mat4(e.matrix);
                    var w = new double[t.Length];
                    for (int i = 0; i < t.Length; i += 3)
                    {
                        UxRaceNum.Transform(M, t[i], t[i + 1], t[i + 2], out w[i], out w[i + 1], out w[i + 2]);
                        w[i + 1] += lift;
                    }
                    parts.Add(w); total += w.Length;
                    for (int i = 0; i < w.Length / 9; i++) own.Add(e.id);
                }
                if (total == 0) return;
                tris = new double[total]; int at = 0;
                foreach (var p in parts) { Array.Copy(p, 0, tris, at, p.Length); at += p.Length; }
                owner = own.ToArray();
                MinMax(tris, out lo, out hi);
            }
            /// nearest(p, radius): the distance and owner of the nearest kept collision triangle (inf, null when none is in reach).
            public double Nearest(double px, double py, double pz, double radius, out long? who)
            {
                who = null;
                double best = double.PositiveInfinity;
                double xp = px + radius, yp = py + radius, zp = pz + radius, xm = px - radius, ym = py - radius, zm = pz - radius;
                for (int i = 0; i < owner.Length; i++)
                {
                    if (!(lo[3 * i] <= xp && lo[3 * i + 1] <= yp && lo[3 * i + 2] <= zp && hi[3 * i] >= xm && hi[3 * i + 1] >= ym && hi[3 * i + 2] >= zm)) continue;
                    double dd = PointTriDist(px, py, pz, tris, 9 * i);
                    if (who == null || dd < best) { best = dd; who = owner[i]; }           // np.argmin: the first of the smallest
                }
                return best;
            }
            /// clearance(x, ground, z): the smallest distance from a rider-height segment (ground + 0.25 .. ground + 4 ft) to kept collision.
            public double Clearance(double x, double ground, double z, out long? who, double height = 4.0, int steps = 5, double radius = 30.0)
            {
                double best = double.PositiveInfinity; who = null;
                for (int k = 0; k < steps; k++)
                {
                    double y = ground + 0.25 + (height - 0.25) * k / Math.Max(1, steps - 1);
                    double r = Nearest(x, y, z, radius, out long? w);
                    if (r < best) { best = r; who = w; }
                }
                return best;
            }
        }
        /// tris.min(1) / tris.max(1) of (n, 3, 3) triangles.
        internal static void MinMax(double[] tris, out double[] lo, out double[] hi)
        {
            int n = tris.Length / 9; lo = new double[3 * n]; hi = new double[3 * n];
            for (int i = 0; i < n; i++)
                for (int k = 0; k < 3; k++)
                {
                    double a = tris[9 * i + k], b = tris[9 * i + 3 + k], c = tris[9 * i + 6 + k];
                    lo[3 * i + k] = Math.Min(Math.Min(a, b), c); hi[3 * i + k] = Math.Max(Math.Max(a, b), c);
                }
        }
        /// uxmesh.collis_triangles of an extracted collis file, cached by file; null when it cannot be read (Python: except Exception).
        internal static double[] CollisTris(UxDiscLevel disc, string file, Dictionary<string, double[]> cache, CancellationToken cancel)
        {
            if (cache.TryGetValue(file, out var t)) return t;
            try { t = UxMesh.CollisTriangles(disc.ReadExtractFile(file, cancel)); }
            catch (OperationCanceledException) { throw; }
            catch (Exception) { t = null; }
            cache[file] = t;
            return t;
        }
        /// point_tri_dist(p, A, B, C) for one triangle (Ericson 5.1.5), with numpy's element-wise evaluation order.                  same as UxEntities.point_tri_dist
        internal static double PointTriDist(double px, double py, double pz, double[] T, int o)
        {
            double ax = T[o], ay = T[o + 1], az = T[o + 2], bx = T[o + 3], by = T[o + 4], bz = T[o + 5], cx = T[o + 6], cy = T[o + 7], cz = T[o + 8];
            double abx = bx - ax, aby = by - ay, abz = bz - az, acx = cx - ax, acy = cy - ay, acz = cz - az, apx = px - ax, apy = py - ay, apz = pz - az;
            double d1 = UxRaceNum.Sum3(abx * apx, aby * apy, abz * apz), d2 = UxRaceNum.Sum3(acx * apx, acy * apy, acz * apz);
            double bpx = px - bx, bpy = py - by, bpz = pz - bz;
            double d3 = UxRaceNum.Sum3(abx * bpx, aby * bpy, abz * bpz), d4 = UxRaceNum.Sum3(acx * bpx, acy * bpy, acz * bpz);
            double cpx = px - cx, cpy = py - cy, cpz = pz - cz;
            double d5 = UxRaceNum.Sum3(abx * cpx, aby * cpy, abz * cpz), d6 = UxRaceNum.Sum3(acx * cpx, acy * cpy, acz * cpz);
            double va = d3 * d6 - d5 * d4, vb = d5 * d2 - d1 * d6, vc = d1 * d4 - d3 * d2;
            double den = va + vb + vc;
            if (Math.Abs(den) < 1e-30) den = 1e-30;
            double v = vb / den, w = vc / den;
            double rx = ax + abx * v + acx * w, ry = ay + aby * v + acy * w, rz = az + abz * v + acz * w;
            if (vc <= 0 && d1 >= 0 && d3 <= 0)
            {
                double dd = d1 - d3; double t = d1 / (Math.Abs(dd) < 1e-30 ? 1e-30 : dd);
                rx = ax + abx * t; ry = ay + aby * t; rz = az + abz * t;
            }
            if (vb <= 0 && d2 >= 0 && d6 <= 0)
            {
                double dd = d2 - d6; double t = d2 / (Math.Abs(dd) < 1e-30 ? 1e-30 : dd);
                rx = ax + acx * t; ry = ay + acy * t; rz = az + acz * t;
            }
            if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
            {
                double dd = (d4 - d3) + (d5 - d6); double t = (d4 - d3) / (Math.Abs(dd) < 1e-30 ? 1e-30 : dd);
                rx = bx + (cx - bx) * t; ry = by + (cy - by) * t; rz = bz + (cz - bz) * t;
            }
            if (d1 <= 0 && d2 <= 0) { rx = ax; ry = ay; rz = az; }
            if (d3 >= 0 && d4 <= d3) { rx = bx; ry = by; rz = bz; }
            if (d6 >= 0 && d5 <= d6) { rx = cx; ry = cy; rz = cz; }
            double ex = rx - px, ey = ry - py, ez = rz - pz;
            return Math.Sqrt(UxRaceNum.Sum3(ex * ex, ey * ey, ez * ez));
        }

        /// ridable_collision(lv, d, tq): {entity id: info} of the kept CollisionRSData entities whose up-facing collision faces
        /// coincide (within 1 ft) with the driven trncol surface over >= 100 ft^2 (area-weighted samples of default_rng(7)).          same as UxEntities.ridable_collision
        internal static SortedDictionary<long, JObj> RidableCollision(UxDiscLevel disc, List<LevelEntity> ents, ExtractIndex idx, TerrainQuery tq, Dictionary<string, double[]> cache, CancellationToken cancel)
        {
            var o = new SortedDictionary<long, JObj>();
            var rng = new UxRaceNum.Rng(7);
            foreach (var e in ents)
            {
                if (!e.Kept || !e.collision || Array.IndexOf(PHYSICS_FAMILY, e.type) >= 0) continue;
                if (e.reflex != "CollisionRSData") continue;
                string fem = e.Asset("FemaleCollisionAsset");
                var rec = fem != null && fem.Length > 0 ? idx.Rec(fem, "collis") : null;
                if (rec == null) continue;
                var t = CollisTris(disc, UxJson.Str(rec.Get("file")), cache, cancel);
                if (t == null || t.Length == 0) continue;
                var M = Mat4(e.matrix);
                int nt = t.Length / 9; var w = new double[t.Length];
                for (int i = 0; i < t.Length; i += 3) UxRaceNum.Transform(M, t[i], t[i + 1], t[i + 2], out w[i], out w[i + 1], out w[i + 2]);
                var ar = new double[nt]; var up = new List<int>();
                for (int i = 0; i < nt; i++)
                {
                    int q = 9 * i;
                    var N = Cross(new[] { w[q + 3] - w[q], w[q + 4] - w[q + 1], w[q + 5] - w[q + 2] }, new[] { w[q + 6] - w[q], w[q + 7] - w[q + 1], w[q + 8] - w[q + 2] });
                    double Ln = Math.Sqrt(UxRaceNum.Sum3(N[0] * N[0], N[1] * N[1], N[2] * N[2]));
                    ar[i] = Ln / 2; double nyv = Ln > 0 ? N[1] / Math.Max(Ln, 1e-30) : 0;
                    if (nyv > 0.7 && ar[i] > 0.5) up.Add(i);
                }
                if (up.Count == 0) continue;
                var aru = new double[up.Count]; for (int i = 0; i < aru.Length; i++) aru[i] = ar[up[i]];
                double sum = UxNp.Sum(aru);
                if (sum < 100) continue;
                int n = Math.Min(400, Math.Max(50, (int)(sum / 20)));
                var p = new double[aru.Length]; for (int i = 0; i < p.Length; i++) p[i] = aru[i] / sum;
                var pick = rng.ChoiceIndices(p, n);
                var u = new double[2 * n]; for (int i = 0; i < 2 * n; i++) u[i] = rng.Random();
                long hit = 0;
                for (int k = 0; k < n; k++)
                {
                    double u0 = u[2 * k], u1 = u[2 * k + 1];
                    if (u0 + u1 > 1) { u0 = 1 - u0; u1 = 1 - u1; }
                    int q = 9 * up[pick[k]];
                    double x = w[q] + (w[q + 3] - w[q]) * u0 + (w[q + 6] - w[q]) * u1;
                    double y = w[q + 1] + (w[q + 4] - w[q + 1]) * u0 + (w[q + 7] - w[q + 1]) * u1;
                    double z = w[q + 2] + (w[q + 5] - w[q + 2]) * u0 + (w[q + 8] - w[q + 2]) * u1;
                    double mn = double.PositiveInfinity; bool any = false;
                    foreach (var s in tq.Surfaces(x, z)) if (s.ny > 0.3) { double dd = Math.Abs(s.y - y); if (!any || dd < mn) mn = dd; any = true; }
                    if (any && mn <= 1.0) hit++;
                }
                double area = sum * hit / n;
                if (area >= 100)
                    o[e.id] = new JObj().Set("name", e.name).Set("coincident_up_area_ft2", UxNp.PyRound(area, 1)).Set("up_area_ft2", UxNp.PyRound(sum, 1)).Set("samples", (long)n).Set("hits", hit);
            }
            return o;
        }

        // ================================================================================================== GL_StartGate model
        internal sealed class GateNode { public string name; public double[] L; }

        static int FindBytes(byte[] hay, byte[] needle, int start)
        {
            for (int i = Math.Max(start, 0); i + needle.Length <= hay.Length; i++)
            {
                int k = 0; while (k < needle.Length && hay[i + k] == needle[k]) k++;
                if (k == needle.Length) return i;
            }
            return -1;
        }
        /// The node count, the root name bytes (with its NUL) and the offset of the node name table of a multi-node model.
        static void GateTable(byte[] model, bool big, out int n, out byte[] rootNul, out int table)
        {
            UxBin.Require(model.Length >= 0xAC, "model truncated");
            n = (int)UxBin.U32(model, 12, big);
            int e = 0x28; while (e < 0xA8 && model[e] != 0) e++;
            rootNul = new byte[e - 0x28 + 1]; Buffer.BlockCopy(model, 0x28, rootNul, 0, e - 0x28);
            table = FindBytes(model, rootNul, 0xAC + 64 * n);
        }
        static List<string> NodeNames(byte[] model, int table, int n)
        {
            var names = new List<string>(n); int p = table;
            for (int i = 0; i < n; i++)
            {
                int e = p; while (e < model.Length && model[e] != 0) e++;
                if (e >= model.Length) throw new UxException("node name table runs past the model");
                names.Add(UxBin.Latin1(model, p, e - p)); p = e + 1;
            }
            return names;
        }
        /// gate_nodes(model): [(name, 4 x 4 local matrix)] of the GateStart_NN nodes of a Reflex PC multi-node model, by name.
        internal static List<GateNode> GateNodes(byte[] model)
        {
            GateTable(model, false, out int n, out byte[] rootNul, out int table);
            uint magic = UxBin.U32LE(model, 0);
            if (magic != 0x88AABB00 || n < 2) throw new UxException("not a multi-node Reflex model (magic " + magic.ToString("x8") + ", nodes " + n + ")");
            UxBin.Require(0xAC + 64L * n <= model.Length, "model: node matrices outside the file");
            if (table < 0) throw new UxException("node name table not found");
            var names = NodeNames(model, table, n);
            var o = new List<GateNode>();
            for (int i = 0; i < n; i++)
            {
                var M = new double[16]; for (int k = 0; k < 16; k++) M[k] = UxBin.F32LE(model, 0xAC + 64 * i + 4 * k);
                if (i == 0) { var eye = Eye4(); for (int k = 0; k < 16; k++) if (!UxRaceNum.AllClose(M[k], eye[k])) throw new UxException("root node matrix is not identity"); }
                if (Regex.IsMatch(names[i], @"\AGateStart_\d\d$")) o.Add(new GateNode { name = names[i], L = M });
            }
            UxNp.StableSort(o, (a, b) => string.CompareOrdinal(a.name, b.name));
            return o;
        }
        /// patch_gate_model(model, new_x, new_name): the GL_StartGate copy with GateStart_NN translations x replaced and the internal
        /// model name (the name buffer at 0x28 and the node name table root) set to new_name (the same length).
        internal static byte[] PatchGateModel(byte[] model, List<KeyValuePair<string, double>> new_x, string new_name = GATE_MODEL_NAME)
        {
            var b = (byte[])model.Clone();
            GateTable(model, false, out int n, out byte[] oldNul, out int root_at);
            UxBin.Require(root_at >= 0, "node name table not found");
            var names = NodeNames(model, root_at, n);
            foreach (var kv in new_x)
            {
                int ix = names.LastIndexOf(kv.Key);                       // {name: index}: the last of a repeated name
                if (ix < 0) throw new UxException("model has no node " + kv.Key);
                UxBin.PutF32(b, 0xAC + 64 * ix + 48, (float)kv.Value);
            }
            var nw = UxBin.Latin1Bytes(new_name);
            if (nw.Length != oldNul.Length - 1) throw new UxException("patched gate name '" + new_name + "' must have the length of '" + UxBin.Latin1(oldNul, 0, oldNul.Length - 1) + "'");
            var hits = new List<int>();
            for (int p = FindBytes(model, oldNul, 0); p >= 0; p = FindBytes(model, oldNul, p + oldNul.Length)) hits.Add(p);
            if (hits.Count != 2 || hits[0] != 0x28 || hits[1] != root_at)
                throw new UxException("model name '" + UxBin.Latin1(oldNul, 0, oldNul.Length - 1) + "' found at [" + string.Join(", ", hits) + "], expected [40, " + root_at + "]");
            foreach (int hpos in hits) Buffer.BlockCopy(nw, 0, b, hpos, nw.Length);
            var got = new Dictionary<string, double>(StringComparer.Ordinal);
            foreach (var g in GateNodes(b)) got[g.name] = g.L[12];
            foreach (var kv in new_x)
                if (!got.TryGetValue(kv.Key, out double gx) || Math.Abs(gx - (double)(float)kv.Value) > 0) throw new UxException("patched node x read back differs");
            return b;
        }
        /// hidden_surface(data, y): the Reflex PC surface with every FLOAT16 position xyz set to (0, y, 0); everything else is kept.
        internal static byte[] HiddenSurface(byte[] data, double y = HIDE_Y_FT)
        {
            var groups = UxRmesh.ReadSurface(data, out byte[] tail);
            if (!UxBin.Same(UxRmesh.WriteSurface(groups, tail), data)) throw new UxException("surface does not round-trip through rmesh (not the 44-byte static layout)");
            ushort h0 = UxNp.DoubleToHalf(0.0), hy = UxNp.DoubleToHalf(y);
            var o = new List<UxSurfaceGroup>(groups.Count);
            foreach (var g in groups)
            {
                var v = (byte[])g.verts.Clone();
                for (int i = 0; i < v.Length; i += UxRmesh.STRIDE) { UxBin.PutU16(v, i, h0); UxBin.PutU16(v, i + 2, hy); UxBin.PutU16(v, i + 4, h0); }
                o.Add(new UxSurfaceGroup(g.strip, v, g.rec));
            }
            var b = UxRmesh.WriteSurface(o, tail);
            if (b.Length != data.Length) throw new UxException("hidden surface length " + b.Length + " != " + data.Length);
            var allowed = new bool[data.Length];
            int p = 0x10 + groups.Count * 0x160;
            foreach (var g in groups)
            {
                p += 2 * g.strip.Length;
                int nv = g.VertexCount;
                for (int i = 0; i < nv; i++) for (int k = 0; k < 6; k++) allowed[p + UxRmesh.STRIDE * i + k] = true;
                p += UxRmesh.STRIDE * nv;
            }
            if (p + tail.Length != data.Length) throw new UxException("surface layout walk ends at " + (p + tail.Length) + " of " + data.Length);
            for (int i = 0; i < data.Length; i++) if (b[i] != data[i] && !allowed[i]) throw new UxException("hidden surface changed bytes outside the position xyz fields");
            return b;
        }

        // ================================================================================================== kind-15 collis
        internal sealed class K15Prim { public uint type; public double[] matrix, @params; public byte[] raw; }
        /// parse_k15(data): the primitives of a Reflex PC kind-15 collis (and its header's 'zero' word).
        internal static List<K15Prim> ParseK15(byte[] data, out uint zero, out uint ntag)
        {
            UxBin.Require(data.Length >= 0x1C, "kind-15 collis truncated");
            uint mag = UxBin.U32LE(data, 0), ver = UxBin.U32LE(data, 8), kind = UxBin.U32LE(data, 12); zero = UxBin.U32LE(data, 16); uint n = UxBin.U32LE(data, 20); ntag = UxBin.U32LE(data, 24);
            if (mag != 1234 || data[4] != (byte)'c' || data[5] != (byte)'o' || data[6] != (byte)'l' || data[7] != 0 || ver != 3 || kind != 15) throw new UxException("not a Reflex PC kind-15 collis");
            long p = 0x1C + (long)ntag; var prims = new List<K15Prim>();
            for (uint i = 0; i < n; i++)
            {
                UxBin.Require(p + 4 <= data.Length, "kind-15 collis: a primitive outside the file");
                uint ty = UxBin.U32LE(data, (int)p); int size = UxMesh.PrimSize(ty);
                if (size == 0) throw new UxException("kind-15 collis: primitive type " + ty);
                UxBin.Require(p + size <= data.Length, "kind-15 collis: a primitive outside the file");
                var M = new double[16]; for (int k = 0; k < 16; k++) M[k] = UxBin.F32LE(data, (int)p + 4 + 4 * k);
                int nf = (size - 72) / 4; var pr = new double[nf]; for (int k = 0; k < nf; k++) pr[k] = UxBin.F32LE(data, (int)p + 72 + 4 * k);
                prims.Add(new K15Prim { type = ty, matrix = M, @params = pr, raw = UxBin.Slice(data, (int)p, size) });
                p += size;
            }
            if (p != data.Length) throw new UxException("kind-15 collis has " + (data.Length - p) + " trailing bytes");
            return prims;
        }
        /// trimmed_collis(data, keep): a copy holding only the primitives `keep` (their bytes unchanged, no tag bytes).
        internal static byte[] TrimmedCollis(byte[] data, List<int> keep)
        {
            var prims = ParseK15(data, out uint zero, out uint ntag);
            if (ntag != 0) throw new UxException("kind-15 collis with tag bytes");
            var w = new UxWriter(0x1C + 88 * keep.Count);
            w.U32(1234).Bytes(new byte[] { (byte)'c', (byte)'o', (byte)'l', 0 }).U32(3).U32(15).U32(zero).U32((uint)keep.Count).U32(0);
            foreach (int i in keep) w.Bytes(prims[i].raw);
            var o = w.ToArray();
            var back = ParseK15(o, out _, out _);
            bool same = back.Count == keep.Count;
            for (int i = 0; same && i < keep.Count; i++) same = UxBin.Same(back[i].raw, prims[keep[i]].raw);
            if (!same) throw new UxException("trimmed collis does not read back");
            return o;
        }

        // ================================================================================================== start-gate collision check
        internal sealed class Samples { public double[] x, z; public int[] tag; public int Count => x.Length; }

        /// corridor_samples(secs, centre): (x, z, section) on a step grid over every TDF section corridor within reach; start chutes skipped.
        static Samples CorridorSamples(List<UxTdfSection> secs, double cx, double cz, double reach = COL_REACH_FT, double step = CORRIDOR_STEP_FT)
        {
            var xs = new List<double>(); var zs = new List<double>(); var tags = new List<int>();
            (double[] l, double[] r) Ends(List<UxTdfItem> n)
            {
                var nd = UxTdf07.NodeDict(n);
                var c3 = UxTdf07.Vec3(UxTdf07.Field(nd, 21)); var cc = new[] { c3[0], c3[2] };
                double[] l = cc, r = cc;
                if (nd.TryGetValue(22, out var r22)) { var v = UxTdf07.Vec3(r22); l = new[] { v[0], v[2] }; }
                if (nd.TryGetValue(23, out var r23)) { var v = UxTdf07.Vec3(r23); r = new[] { v[0], v[2] }; }
                if (!(UxNp.Finite(l[0], l[1]) && UxNp.Finite(r[0], r[1]))) l = r = cc;
                return (l, r);
            }
            for (int si = 0; si < secs.Count; si++)
            {
                var s = secs[si];
                if (UxTdf07.SectionKind(s) == 4 || s.nodes.Count == 0) continue;
                var E = new List<(double[] l, double[] r)>(); foreach (var n in s.nodes) E.Add(Ends(n));
                var quads = new List<((double[] l, double[] r) a, (double[] l, double[] r) b)>();
                for (int i = 0; i + 1 < E.Count; i++) quads.Add((E[i], E[i + 1]));
                foreach (uint nx in UxTdf07.Links(s, 14))
                    if (nx < secs.Count && secs[(int)nx].nodes.Count > 0) quads.Add((E[E.Count - 1], Ends(secs[(int)nx].nodes[0])));
                foreach (var (qa, qb) in quads)
                {
                    double[] l0 = qa.l, r0 = qa.r, l1 = qb.l, r1 = qb.r;
                    double dmin = double.PositiveInfinity;
                    foreach (var pt in new[] { l0, r0, l1, r1 }) { double ex = pt[0] - cx, ez = pt[1] - cz; double dd = Math.Sqrt(ex * ex + ez * ez); if (dd < dmin || double.IsNaN(dd)) dmin = dd; }
                    if (dmin > reach + 60) continue;
                    double ln = Math.Max(UxRaceNum.Norm2(l1[0] - l0[0], l1[1] - l0[1]), UxRaceNum.Norm2(r1[0] - r0[0], r1[1] - r0[1]));
                    double wd = Math.Max(UxRaceNum.Norm2(r0[0] - l0[0], r0[1] - l0[1]), UxRaceNum.Norm2(r1[0] - l1[0], r1[1] - l1[1]));
                    int ns = (int)Math.Ceiling(ln / step) + 1, nt = (int)Math.Ceiling(wd / step) + 1;
                    var s_ = UxRaceNum.Linspace01(ns); var t_ = UxRaceNum.Linspace01(nt);
                    for (int a = 0; a < ns; a++)
                        for (int b = 0; b < nt; b++)
                        {
                            double ax = l0[0] + (r0[0] - l0[0]) * t_[b], az = l0[1] + (r0[1] - l0[1]) * t_[b];
                            double bx = l1[0] + (r1[0] - l1[0]) * t_[b], bz = l1[1] + (r1[1] - l1[1]) * t_[b];
                            double px = ax + (bx - ax) * s_[a], pz = az + (bz - az) * s_[a];
                            double ex = px - cx, ez = pz - cz;
                            if (Math.Sqrt(ex * ex + ez * ez) <= reach) { xs.Add(px); zs.Add(pz); tags.Add(si); }
                        }
                }
            }
            return new Samples { x = xs.ToArray(), z = zs.ToArray(), tag = tags.ToArray() };
        }

        /// lane_samples(node_local, sg): (x, z, -lane) of each gate lane: 1 ft behind its GateStart node to 30 ft ahead, the node and
        /// +-1.25 ft across.
        static Samples LaneSamples(List<double[]> node_local, double[] sg, double step = CORRIDOR_STEP_FT)
        {
            var xs = new List<double>(); var zs = new List<double>(); var tags = new List<int>();
            for (int k = 0; k < node_local.Count; k++)
            {
                double x = node_local[k][0], y = node_local[k][1], z = node_local[k][2];
                var zz = UxRaceNum.Arange(z - 1.0, z + LANE_AHEAD_FT + 1e-9, step);
                foreach (double dx in new[] { -LANE_HALF_FT, 0.0, LANE_HALF_FT })
                    foreach (double zv in zz)
                    {
                        UxRaceNum.Transform(sg, x + dx, y, zv, out double wx, out _, out double wz);
                        xs.Add(wx); zs.Add(wz); tags.Add(-(k + 1));
                    }
            }
            return new Samples { x = xs.ToArray(), z = zs.ToArray(), tag = tags.ToArray() };
        }

        internal sealed class Blocking { public uint type; public long samples; public SortedDictionary<long, long> sections = new SortedDictionary<long, long>(); public List<object> centre; }

        /// collision_hits(prims, sg, S, g, band): the primitives (local to TO_StartingGate) a sample at HF + band lies inside (+ margin).
        static SortedDictionary<long, Blocking> CollisionHits(List<K15Prim> prims, double[] sg, Samples S, double[] g, double[] band)
        {
            var blocking = new SortedDictionary<long, Blocking>();
            int n = S.Count;
            for (int i = 0; i < prims.Count; i++)
            {
                var p = prims[i];
                var Wm = UxRaceNum.MatMul4(p.matrix, sg);
                var iv = UxRaceNum.Inv4(Wm);
                var hit = new bool[n]; bool any = false;
                double m0 = 0, m1 = 0, m2 = 0;
                if (p.type == 5) { m0 = Math.Abs(p.@params[0]) + COL_MARGIN_FT; m1 = Math.Abs(p.@params[1]) + COL_MARGIN_FT; m2 = Math.Abs(p.@params[2]) + COL_MARGIN_FT; }
                else if (p.type == 8) { m0 = p.@params[0] + COL_MARGIN_FT; m1 = p.@params[1] / 2 + COL_MARGIN_FT; }
                else m0 = p.@params[0] + COL_MARGIN_FT;
                var pr = p; var ivm = iv;
                Parallel.For(0, (n + 4095) / 4096, blk =>
                {
                    int end = Math.Min(n, (blk + 1) * 4096);
                    for (int k = blk * 4096; k < end; k++)
                    {
                        if (!UxNp.Finite(g[k])) continue;
                        foreach (double dy in band)
                            if (Inside(pr.type, ivm, S.x[k], g[k] + dy, S.z[k], m0, m1, m2)) { hit[k] = true; break; }
                    }
                });
                long count = 0; var secs_hit = new SortedDictionary<long, long>();
                for (int k = 0; k < n; k++)
                    if (hit[k]) { any = true; count++; secs_hit[S.tag[k]] = secs_hit.TryGetValue(S.tag[k], out long c) ? c + 1 : 1; }
                if (any)
                    blocking[i] = new Blocking { type = p.type, samples = count, sections = secs_hit, centre = L(UxNp.PyRound(Wm[12], 2), UxNp.PyRound(Wm[13], 2), UxNp.PyRound(Wm[14], 2)) };
            }
            return blocking;
        }
        /// One sample against one primitive: loc = Q @ iv[:3,:3] + iv[3,:3], then the shape's inside test. The plain product decides
        /// when it is clear of every limit by 1e-6 ft; next to a limit the dgemm chain (fused multiply-adds) is evaluated exactly.
        static bool Inside(uint type, double[] iv, double x, double y, double z, double m0, double m1, double m2)
        {
            double lx = x * iv[0] + y * iv[4] + z * iv[8] + iv[12], ly = x * iv[1] + y * iv[5] + z * iv[9] + iv[13], lz = x * iv[2] + y * iv[6] + z * iv[10] + iv[14];
            const double Near = 1e-6;
            if (type == 5)
            {
                double ax = Math.Abs(lx), ay = Math.Abs(ly), az = Math.Abs(lz);
                if (ax > m0 + Near || ay > m1 + Near || az > m2 + Near) return false;
                if (ax < m0 - Near && ay < m1 - Near && az < m2 - Near) return true;
                UxRaceNum.Transform(iv, x, y, z, out lx, out ly, out lz);
                return Math.Abs(lx) <= m0 && Math.Abs(ly) <= m1 && Math.Abs(lz) <= m2;
            }
            if (type == 8)
            {
                double hr = Math.Sqrt(lx * lx + lz * lz), ay = Math.Abs(ly);
                if (hr > m0 + Near || ay > m1 + Near) return false;
                if (hr < m0 - Near && ay < m1 - Near) return true;
                UxRaceNum.Transform(iv, x, y, z, out lx, out ly, out lz);
                return UxRaceNum.NpHypot(lx, lz) <= m0 && Math.Abs(ly) <= m1;
            }
            double nr = Math.Sqrt(lx * lx + ly * ly + lz * lz);
            if (nr > m0 + Near) return false;
            if (nr < m0 - Near) return true;
            UxRaceNum.Transform(iv, x, y, z, out lx, out ly, out lz);
            return Math.Sqrt(UxRaceNum.Sum3(lx * lx, ly * ly, lz * lz)) <= m0;
        }

        internal sealed class ColCheck
        {
            public string asset; public int n_prims; public long corridor_samples, lane_samples; public SortedDictionary<long, Blocking> blocking; public List<long> lane_blocking;
            /// The 'startgate_collision' record (col without 'prims').
            public JObj Json()
            {
                var blk = new SortedDictionary<long, object>();
                foreach (var kv in blocking)
                {
                    var secs = new SortedDictionary<long, object>(); foreach (var s in kv.Value.sections) secs[s.Key] = s.Value;
                    blk[kv.Key] = new JObj().Set("type", (long)kv.Value.type).Set("samples", kv.Value.samples).Set("sections", secs).Set("centre", kv.Value.centre);
                }
                var band = new List<object>(); foreach (double b in COL_BAND_FT) band.Add(b);
                var lband = new List<object>(); foreach (double b in LANE_BAND_FT) lband.Add(b);
                var lb = new List<object>(); foreach (long i in lane_blocking) lb.Add(i);
                return new JObj().Set("asset", asset).Set("n_prims", (long)n_prims).Set("corridor_samples", corridor_samples).Set("lane_samples", lane_samples)
                    .Set("band_ft", band).Set("lane_band_ft", lband).Set("margin_ft", COL_MARGIN_FT).Set("blocking", blk).Set("lane_blocking", lb);
            }
        }
        /// startgate_collision_check(tp, sg_new, secs, hf, lift, collis, node_local).
        static ColCheck StartgateCollisionCheck(UxRaceTemplate tp, double[] sg_new, List<UxTdfSection> secs, HeightField hf, byte[] collis, List<double[]> node_local)
        {
            byte[] data = collis ?? tp.Read("GL_StartGate_COL", "collis");
            var prims = ParseK15(data, out _, out _);
            var S = CorridorSamples(secs, sg_new[12], sg_new[14]);
            var g = new double[S.Count]; for (int k = 0; k < g.Length; k++) g[k] = hf.AtVec(S.x[k], S.z[k]);
            var blocking = CollisionHits(prims, sg_new, S, g, COL_BAND_FT);
            var Ls = LaneSamples(node_local, sg_new);
            var gl = new double[Ls.Count]; for (int k = 0; k < gl.Length; k++) gl[k] = hf.AtVec(Ls.x[k], Ls.z[k]);
            var lane_blk = CollisionHits(prims, sg_new, Ls, gl, LANE_BAND_FT);
            foreach (var kv in lane_blk)
            {
                if (blocking.TryGetValue(kv.Key, out var have)) { have.samples += kv.Value.samples; foreach (var s in kv.Value.sections) have.sections[s.Key] = s.Value; }
                else blocking[kv.Key] = kv.Value;
            }
            return new ColCheck { asset = collis == null ? "GL_StartGate_COL" : "trimmed", n_prims = prims.Count, corridor_samples = S.Count, lane_samples = Ls.Count, blocking = blocking, lane_blocking = new List<long>(lane_blk.Keys) };
        }

        // ================================================================================================== tdf helpers
        /// patch_sx_flags(data, value): section field 19 of every lap (kind 0) and chute (kind 4) section set to `value`.
        static byte[] PatchSxFlags(byte[] data, long value, out long n)
        {
            var sp = UxTdf07.Split(data);
            UxTdf07.Structure(sp.items, out var glob, out var secs);
            n = 0;
            foreach (var s in secs)
            {
                uint k = UxTdf07.SectionKind(s);
                if (k != 0 && k != 4) continue;
                for (int j = 0; j < s.fields.Count; j++)
                {
                    var it = s.fields[j];
                    if (it.id != 19) continue;
                    UxBin.Require(it.raw.Length >= 8 + 128, "tdf: section field 19 shorter than 8 x 4 words");
                    long cnt = UxBin.U32BE(it.raw, 0), el = UxBin.U32BE(it.raw, 4);
                    var raw = (byte[])it.raw.Clone();
                    for (long q = 0; q < Math.Min(cnt * el, 32); q++) UxBin.PutU32(raw, 8 + 4 * (int)q, checked((uint)value), true);
                    s.fields[j] = new UxTdfItem(it.id, it.size, raw);
                    n++;
                }
            }
            return UxTdf07.Join(sp.hdr, sp.path, UxTdf07.Flatten(glob, secs), sp.Reflex);
        }

        /// tdf_facts(data): the walk, the counts and the lap facts of a Reflex tdf.
        static JObj TdfFacts(byte[] data, out List<UxTdfItem> glob, out List<UxTdfSection> secs)
        {
            var sp = UxTdf07.Split(data);
            var probs = UxTdf07.WalkCheck(sp.hdr, sp.items);
            UxTdf07.Structure(sp.items, out glob, out secs);
            long nn = 0; foreach (var s in secs) nn += s.nodes.Count;
            var sm = UxTdf07.Summarize(data);
            var lines = new List<object>(); foreach (long id in ((SortedDictionary<long, object>)sm.Get("lines")).Keys) lines.Add(id);
            var wp = new List<object>(); foreach (string p in probs) wp.Add(p);
            return new JObj().Set("kind", sp.kind).Set("size_word_ok", UxBin.U32LE(data, 0) == (uint)(data.Length - 4)).Set("walk_problems", wp)
                .Set("nodes_header", (long)sp.hdr[0]).Set("nodes_walked", nn).Set("sections_header", (long)sp.hdr[1]).Set("sections_walked", (long)secs.Count)
                .Set("lap_G5", sm.Get("lap_length_G5")).Set("lap_centreline", sm.Get("lap_length_centreline")).Set("lap_closed", sm.Get("lap_closed"))
                .Set("lap_sum_f24", sm.Get("lap_length_sum_f24")).Set("lines", lines).Set("section_kinds", sm.Get("section_kinds"))
                .Set("bytes", (long)data.Length).Set("md5", UxBin.Md5(data));
        }

        /// The race sections in the engine's order: the chutes (kind & 4) and the sections their next links reach.
        static List<int> RaceSections(List<UxTdfSection> secs, out List<List<uint>> nexts)
        {
            nexts = new List<List<uint>>(); foreach (var sc in secs) nexts.Add(UxTdf07.Links(sc, 14));
            var race = new List<int>(); var todo = new List<int>();
            for (int k = 0; k < secs.Count; k++) if ((UxTdf07.SectionKind(secs[k]) & 4) != 0) todo.Add(k);
            while (todo.Count > 0)
            {
                int k = todo[0]; todo.RemoveAt(0);
                if (race.Contains(k)) continue;
                race.Add(k);
                foreach (uint x in nexts[k]) if (x < secs.Count && !race.Contains((int)x)) todo.Add((int)x);
            }
            return race;
        }

        /// A triangle set with its xz bounding boxes and a grid over them (mesh_top / band_hits).
        internal sealed class MeshSet
        {
            public readonly double[] W; public readonly int Count; readonly double[] _x0, _x1, _z0, _z1; readonly TriGrid _grid;
            public MeshSet(double[] W)
            {
                this.W = W; Count = W.Length / 9;
                _x0 = new double[Count]; _x1 = new double[Count]; _z0 = new double[Count]; _z1 = new double[Count];
                for (int i = 0; i < Count; i++)
                {
                    int q = 9 * i;
                    _x0[i] = Math.Min(Math.Min(W[q], W[q + 3]), W[q + 6]); _x1[i] = Math.Max(Math.Max(W[q], W[q + 3]), W[q + 6]);
                    _z0[i] = Math.Min(Math.Min(W[q + 2], W[q + 5]), W[q + 8]); _z1[i] = Math.Max(Math.Max(W[q + 2], W[q + 5]), W[q + 8]);
                }
                _grid = new TriGrid(_x0, _x1, _z0, _z1, 0.0);
            }
            /// mesh_top (band = false): the highest y of the triangles whose xz covers (x, z); band_hits (band = true): the largest
            /// height above the ground g inside the bike band. null when there is none.
            public double? Top(double x, double z, bool band, double g)
            {
                var items = _grid.Query(x, z, out int from, out int to);
                bool any = false; double best = 0;
                for (int k = from; k < to; k++)
                {
                    int t = items[k];
                    if (!(_x0[t] <= x && _x1[t] >= x && _z0[t] <= z && _z1[t] >= z)) continue;
                    int q = 9 * t;
                    double x0 = W[q], y0 = W[q + 1], z0 = W[q + 2], x1 = W[q + 3], y1 = W[q + 4], z1 = W[q + 5], x2 = W[q + 6], y2 = W[q + 7], z2 = W[q + 8];
                    double den = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2);
                    bool ok = Math.Abs(den) > 1e-12;
                    if (!ok) den = 1.0;
                    double l0 = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / den;
                    double l1 = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / den;
                    double l2 = 1.0 - l0 - l1;
                    if (!(ok && l0 >= -1e-6 && l1 >= -1e-6 && l2 >= -1e-6)) continue;
                    double y = l0 * y0 + l1 * y1 + l2 * y2;
                    if (band) { y = y - g; if (!(y > BAND_LO && y < BAND_HI)) continue; }
                    if (!any || y > best) { best = y; any = true; }
                }
                return any ? best : (double?)null;
            }
        }

        /// The terrain part triangles (terrain/excluded_tris.npy + lift), or null when the file is missing.
        static double[] TerrainPartTris(UxWork work, TerrainQuery tq, double lift, out long count)
        {
            count = 0;
            if (!work.Exists("terrain/excluded_tris.npy")) return null;
            var ids = Npy.Read(work.ReadBytes("terrain/excluded_tris.npy")).AsLong();
            var sel = new List<long>(); foreach (long i in ids) if (i >= 0 && i < tq.Count) sel.Add(i);
            count = sel.Count;
            var W = new double[9 * sel.Count];
            for (int k = 0; k < sel.Count; k++)
            {
                int t = (int)sel[k];
                for (int c = 0; c < 3; c++) { W[9 * k + c] = tq.A[3 * t + c]; W[9 * k + 3 + c] = tq.B[3 * t + c]; W[9 * k + 6 + c] = tq.C[3 * t + c]; }
                W[9 * k + 1] += lift; W[9 * k + 4] += lift; W[9 * k + 7] += lift;
            }
            return W;
        }
        static double[] Concat(List<double[]> parts)
        {
            int n = 0; foreach (var p in parts) n += p.Length;
            var o = new double[n]; int at = 0;
            foreach (var p in parts) { Array.Copy(p, 0, o, at, p.Length); at += p.Length; }
            return o;
        }
        /// cw.tris[mask] for the owners (not) in a set.
        static double[] OwnerTris(CollisionWorld cw, ICollection<long> ids, bool inSet, out long count, HashSet<long> owners = null)
        {
            var keep = new List<int>();
            for (int i = 0; i < cw.Count; i++) if (ids.Contains(cw.owner[i]) == inSet) { keep.Add(i); owners?.Add(cw.owner[i]); }
            count = keep.Count;
            var o = new double[9 * keep.Count];
            for (int k = 0; k < keep.Count; k++) Array.Copy(cw.tris, 9 * keep[k], o, 9 * k, 9);
            return o;
        }

        /// OR 0x2 into every word of the grid-28 items of a node. Returns (cells, cells flagged before) per changed item.
        static List<(long cells, long before)> FlagNode(List<UxTdfItem> nd)
        {
            var o = new List<(long, long)>();
            for (int j = 1; j < nd.Count; j++)
            {
                var it = nd[j];
                if (it.id != 28) continue;
                UxBin.Require(it.raw.Length >= 8, "tdf: grid 28 shorter than its counts");
                long rows = UxBin.U32BE(it.raw, 0), cols = UxBin.U32BE(it.raw, 4), nw = rows * cols;
                UxBin.Require(8 + 4 * nw <= it.raw.Length, "tdf: grid 28 shorter than its words");
                var raw = (byte[])it.raw.Clone(); long before = 0;
                for (long q = 0; q < nw; q++) { uint v = UxBin.U32BE(raw, 8 + 4 * (int)q); if ((v & 2) != 0) before++; UxBin.PutU32(raw, 8 + 4 * (int)q, v | 2, true); }
                nd[j] = new UxTdfItem(it.id, it.size, raw);
                o.Add((nw, before));
            }
            return o;
        }

        /// deck_norespawn(data, W, hf, thresh): 0x2 on every race node with a ridable mesh more than `thresh` ft above the HF over one
        /// of its respawn points. Returns `data` itself when no node needs it.
        static byte[] DeckNorespawn(byte[] data, MeshSet W, HeightField hf, List<JObj> recs, double thresh = DECK_NORESPAWN_FT)
        {
            if (W.Count == 0) return data;
            var sp = UxTdf07.Split(data);
            UxTdf07.Structure(sp.items, out var glob, out var secs);
            var race = RaceSections(secs, out _);
            foreach (int si in race)
                for (int ni = 0; ni < secs[si].nodes.Count; ni++)
                {
                    var nd = secs[si].nodes[ni]; var f = UxTdf07.NodeDict(nd);
                    if (!f.ContainsKey(28) || !f.ContainsKey(21) || !f.ContainsKey(22) || !f.ContainsKey(23)) continue;
                    var c = UxTdf07.Vec3(f[21]); var l = UxTdf07.Vec3(f[22]); var r = UxTdf07.Vec3(f[23]);
                    double a0 = r[0] - l[0], a1 = r[2] - l[2];
                    double an = Math.Max(UxRaceNum.NpHypot(a0, a1), 1e-12);
                    a0 = a0 / an; a1 = a1 / an;
                    var pts = new List<(string tag, double x, double z)> { ("centre", c[0], c[2]) };
                    foreach (var (tag, e, sg) in new[] { ("22", l, -1.0), ("23", r, 1.0) })
                    {
                        double k = sg * 0.5 * UxRaceNum.NpHypot(e[0] - c[0], e[2] - c[2]);
                        pts.Add((tag, c[0] + k * a0, c[2] + k * a1));
                    }
                    string worstTag = null; double worst = 0;
                    foreach (var (tag, x, z) in pts)
                    {
                        var top = W.Top(x, z, false, 0);
                        if (top == null) continue;
                        var g = hf.At(x, z);
                        if (g == null) continue;
                        if (worstTag == null || top.Value - g.Value > worst) { worstTag = tag; worst = top.Value - g.Value; }
                    }
                    if (worstTag == null || worst <= thresh) continue;
                    foreach (var (cells, before) in FlagNode(nd))
                        recs.Add(new JObj().Set("section", (long)si).Set("node", (long)ni).Set("centre", L(UxNp.PyRound(c[0], 2), UxNp.PyRound(c[2], 2)))
                            .Set("respawn_point", worstTag).Set("mesh_above_hf_ft", UxNp.PyRound(worst, 2)).Set("cells", cells).Set("cells_flagged_before", before)
                            .Set("changed", before < cells));
                }
            bool any = false; foreach (var r in recs) if ((bool)r.Get("changed")) any = true;
            if (!any) return data;
            var o = UxTdf07.Join(sp.hdr, sp.path, UxTdf07.Flatten(glob, secs), sp.Reflex);
            UxTdf07.Split(o);                                            // walks to EOF or throws
            if (o.Length != data.Length) throw new UxException("deck_norespawn changed the tdf length " + data.Length + " -> " + o.Length);
            return o;
        }

        /// engine_respawn_point(c, l, r, side): the centre moved by half the horizontal centre-to-edge distance along the 3D-normalised
        /// 22 -> 23 axis.
        static void EngineRespawnPoint(double[] c, double[] l, double[] r, int side, out double x, out double z)
        {
            double ex = r[0] - l[0], ey = r[1] - l[1], ez = r[2] - l[2];
            double e3 = Math.Sqrt(ex * ex + ey * ey + ez * ez);
            if (e3 > 0) { ex = ex / e3; ez = ez / e3; }
            var e = side >= 0 ? r : l;
            double m = (side >= 0 ? 1.0 : -1.0) * 0.5 * UxRaceNum.PyHypot(e[0] - c[0], e[2] - c[2]);
            x = c[0] + m * ex; z = c[2] + m * ez;
        }

        internal sealed class Embed { public double h; public string side; public double x, z; }
        static readonly double[] StepsA = UxRaceNum.Arange(-FOOT_ALONG, FOOT_ALONG + 1e-9, FOOT_STEP), StepsB = UxRaceNum.Arange(-FOOT_ACROSS, FOOT_ACROSS + 1e-9, FOOT_STEP);
        /// footprint_embed: the worst height in the bike band over the bike footprint at both engine respawn points of a node.
        static Embed FootprintEmbed(MeshSet Wb, HeightField hf, double[] c, double[] l, double[] r, double[] d)
        {
            double dl = UxRaceNum.PyHypot(d[0], d[2]); if (dl == 0) dl = 1.0;
            double fx = d[0] / dl, fz = d[2] / dl;
            Embed worst = null;
            foreach (int side in new[] { -1, 1 })
            {
                EngineRespawnPoint(c, l, r, side, out double qx, out double qz);
                foreach (double a in StepsA)
                    foreach (double b in StepsB)
                    {
                        double x = qx + fx * a - fz * b, z = qz + fz * a + fx * b;
                        var g = hf.At(x, z);
                        if (g == null) continue;
                        var h = Wb.Top(x, z, true, g.Value);
                        if (h != null && (worst == null || h.Value > worst.h))
                            worst = new Embed { h = h.Value, side = side < 0 ? "L" : "R", x = UxNp.Round(x, 2), z = UxNp.Round(z, 2) };   // round() of np.float64: numpy's
                    }
            }
            return worst;
        }

        /// landing_walk(secs, glob, flagged): rx_race_stub_respawn_point's walk: per race node (not a section head) the node the bike
        /// lands on and how.
        static List<((int s, int n) crash, (int s, int n) landing, string how)> LandingWalk(List<UxTdfSection> secs, List<UxTdfItem> glob, Func<int, int, bool> flagged, out (int s, int n)? fin)
        {
            fin = null;
            foreach (var it in glob)
                if (it.id == 9 && it.raw.Length >= 48)
                {
                    uint a = UxBin.U32BE(it.raw, 40), b = UxBin.U32BE(it.raw, 44);
                    if (a < secs.Count && b < secs[(int)a].nodes.Count) fin = ((int)a, (int)b);
                    break;
                }
            var race = RaceSections(secs, out var nexts);
            var o = new List<((int, int), (int, int), string)>();
            foreach (int si in race)
            {
                int nn = secs[si].nodes.Count;
                for (int ni = 1; ni < nn; ni++)
                {
                    (int s, int n) d18 = (si, ni - 1), node = (si, ni); string how = "walk";
                    if (flagged(si, ni))
                    {
                        int s = si, n = ni;
                        while (true)
                        {
                            if (fin != null && fin.Value.s == s && fin.Value.n == n) { node = d18; how = "finish rule: d+0x18"; break; }
                            if (!flagged(s, n)) { node = (s, n); break; }
                            n++;
                            if (n < secs[s].nodes.Count) continue;
                            if (nexts[s].Count == 0) { node = (s, n - 1); break; }
                            s = checked((int)nexts[s][0]); n = 0;
                        }
                    }
                    if (fin != null && node == fin.Value) { node = node.n > 0 ? (node.s, node.n - 1) : node; how = "finish node -> prev"; }
                    if (node.n == 0) { node = (node.s, secs[node.s].nodes.Count > 1 ? 1 : 0); how = "section head -> head.next"; }
                    o.Add(((si, ni), node, how));
                }
            }
            return o;
        }

        /// deck_norespawn_engine(data, Wd, Wb, hf): the 'auto' deck rule plus every race node whose bike footprint holds static
        /// collision in the bike band; then the landing walk for the record.
        static byte[] DeckNorespawnEngine(byte[] data, MeshSet Wd, MeshSet Wb, HeightField hf, List<JObj> recs, out JObj rep)
        {
            byte[] o = Wd.Count > 0 ? DeckNorespawn(data, Wd, hf, recs) : data;
            var sp = UxTdf07.Split(o);
            UxTdf07.Structure(sp.items, out var glob, out var secs);
            var race = RaceSections(secs, out _);
            var embed = new Dictionary<(int, int), Embed>();
            // the footprints of the nodes are independent: computed in parallel, applied in node order
            var work = new List<(int si, int ni, double[] c, double[] l, double[] r, double[] d)>();
            foreach (int si in race)
                for (int ni = 0; ni < secs[si].nodes.Count; ni++)
                {
                    var f = UxTdf07.NodeDict(secs[si].nodes[ni]);
                    if (!(f.ContainsKey(21) && f.ContainsKey(22) && f.ContainsKey(23) && f.ContainsKey(28))) continue;
                    work.Add((si, ni, UxTdf07.Vec3(f[21]), UxTdf07.Vec3(f[22]), UxTdf07.Vec3(f[23]), f.ContainsKey(26) ? UxTdf07.Vec3(f[26]) : new[] { 0.0, 0.0, 1.0 }));
                }
            var found = new Embed[work.Count];
            if (Wb.Count > 0) Parallel.For(0, work.Count, k => { var w = work[k]; found[k] = FootprintEmbed(Wb, hf, w.c, w.l, w.r, w.d); });
            for (int k = 0; k < work.Count; k++)
            {
                var w = found[k];
                if (w == null) continue;
                var (si, ni, c, _, _, _) = work[k];
                embed[(si, ni)] = w;
                bool have = false;
                foreach (var x in recs) if (UxJson.Int(x.Get("section")) == si && UxJson.Int(x.Get("node")) == ni) { have = true; break; }
                foreach (var (cells, before) in FlagNode(secs[si].nodes[ni]))
                    if (!have)
                        recs.Add(new JObj().Set("section", (long)si).Set("node", (long)ni).Set("centre", L(UxNp.PyRound(c[0], 2), UxNp.PyRound(c[2], 2)))
                            .Set("respawn_point", "footprint " + w.side + " [np.float64(" + PyFormat.Repr(w.x) + "), np.float64(" + PyFormat.Repr(w.z) + ")]")
                            .Set("mesh_above_hf_ft", UxNp.PyRound(w.h, 2)).Set("rule", "engine footprint (bike band)").Set("cells", cells)
                            .Set("cells_flagged_before", before).Set("changed", before < cells));
            }
            bool any = false; foreach (var r in recs) if ((bool)r.Get("changed")) any = true;
            if (any)
            {
                var nw = UxTdf07.Join(sp.hdr, sp.path, UxTdf07.Flatten(glob, secs), sp.Reflex);
                UxTdf07.Split(nw);
                if (nw.Length != data.Length) throw new UxException("deck_norespawn_engine changed the tdf length " + data.Length + " -> " + nw.Length);
                o = nw;
            }
            // the landing walk on the result (cell 0; the flags are set in all cells)
            sp = UxTdf07.Split(o);
            UxTdf07.Structure(sp.items, out glob, out secs);
            var secs2 = secs;
            bool Flagged(int si, int ni)
            {
                var f = UxTdf07.NodeDict(secs2[si].nodes[ni]);
                if (!f.TryGetValue(28, out var raw)) return false;
                long rows = UxBin.U32BE(raw, 0), cols = UxBin.U32BE(raw, 4);
                return rows * cols != 0 && (UxBin.U32BE(raw, 8) & 2) != 0;
            }
            var land = LandingWalk(secs, glob, Flagged, out var fin);
            var moves = new List<double>(); var still = new List<object>();
            foreach (var (crash, ln, how) in land)
            {
                if (embed.TryGetValue(ln, out var em))
                    still.Add(new JObj().Set("crash", L((long)crash.s, (long)crash.n)).Set("landing", L((long)ln.s, (long)ln.n)).Set("how", how).Set("embed_ft", UxNp.PyRound(em.h, 2)));
                double dist = 0.0;
                if (ln.s == crash.s && ln.n >= crash.n)
                {
                    for (int k = crash.n; k < ln.n; k++)
                    {
                        var f = UxTdf07.NodeDict(secs[crash.s].nodes[k]);
                        dist += f.TryGetValue(24, out var r24) ? (double)UxBin.F32BE(r24, 0) : 0.0;
                    }
                    moves.Add(dist);
                }
            }
            if (moves.Count == 0) moves.Add(0.0);
            double mx = moves[0]; foreach (double v in moves) if (v > mx) mx = v;
            rep = new JObj()
                .Set("rule", "static collision " + UxRaceNum.PyG(BAND_LO) + ".." + UxRaceNum.PyG(BAND_HI) + " ft above the HF in the bike footprint (+-" + UxRaceNum.PyG(FOOT_ALONG) +
                    " ft along, +-" + UxRaceNum.PyG(FOOT_ACROSS) + " ft across, " + UxRaceNum.PyG(FOOT_STEP) + " ft grid) at either engine respawn point")
                .Set("static_tris", (long)Wb.Count).Set("embedded_nodes", (long)embed.Count).Set("finish", fin == null ? null : L((long)fin.Value.s, (long)fin.Value.n))
                .Set("forward_move_ft", new JObj().Set("median", UxNp.PyRound(UxRaceNum.Median(moves), 1)).Set("p90", UxNp.PyRound(UxRaceNum.Percentile(moves, 90), 1)).Set("max", UxNp.PyRound(mx, 1)))
                .Set("embedded_landings", still);
            return o;
        }

        // ================================================================================================== lap pacing
        /// lap_pacing(level, slot_name, mode): (bytes or null, record, problems).
        static byte[] LapPacing(UxDiscLevel disc, UxRaceOptions opt, string slot_name, out JObj rec, List<string> probs, CancellationToken cancel)
        {
            string mode = opt.lapPacing;
            if (mode != "none" && mode != "untamed") throw new UxException("lappacing must be one of ('none', 'untamed'), not '" + mode + "'");
            if (mode == "none")
            {
                rec = new JObj().Set("asset", null).Set("included", false).Set("option", "none")
                    .Set("reason", "--lappacing none (default since 2026-11-10): no table = AI pacing off; the Untamed table over-drove the AI (work/headless-race/bisect-norton)");
                return null;
            }
            string src = "AI/" + disc.lv.disc + "-lappacing.bxml";
            byte[] lraw = opt.lapPacingSource;
            if (lraw == null && disc.root != null)
            {
                var f = disc.volume.Find((disc.root.Length == 0 ? "" : disc.root + "/") + src);
                if (f != null) lraw = disc.volume.Read(f, cancel);
            }
            if (lraw == null)
            {
                rec = new JObj().Set("asset", null).Set("included", false).Set("option", mode).Set("reason", "source missing");
                probs.Add("lap pacing source " + src + " missing");
                return null;
            }
            var lp_bytes = UxLapPacing.ToReflex(lraw, out JObj linfo, "Supercross");
            var ltab = UxLapPacing.EngineCheck(lp_bytes, out List<string> lprobs, "Supercross", 0);
            if (lprobs.Count > 0) probs.Add("lap pacing fails the engine-reader check: " + UxRaceNum.PyRepr(lprobs.GetRange(0, Math.Min(3, lprobs.Count))));
            var lpl = new List<object>(); foreach (string p in lprobs) lpl.Add(p);
            rec = new JObj().Set("asset", slot_name + "-lappacing").Set("type", "bxml").Set("included", true).Set("source", src).Set("source_md5", UxBin.Md5(lraw))
                .Set("events_in", linfo.Get("events_in")).Set("renamed", linfo.Get("renamed")).Set("added", linfo.Get("added")).Set("bytes", (long)lp_bytes.Length)
                .Set("md5", UxBin.Md5(lp_bytes)).Set("engine_check", lprobs.Count == 0 ? (object)"PASS" : lpl).Set("table", ltab)
                .Set("reader", "lappacing_to_reflex.engine_check (core build_activity_race.c:964-1040)").Set("option", mode);
            return lp_bytes;
        }

        // ================================================================================================== the race kit
        sealed class Chute { public int ci; public List<double[]> c = new List<double[]>(), dirs = new List<double[]>(), m = new List<double[]>(); public double w0; }
        sealed class NodeRec { public JObj j; public string name; public bool onDirt, inCorr, hfOk; public double? clr2; public double[] local, world; }

        /// The stage for one level: ux_race.run_level(write=True, plot=False) on the stock clone base. Reads level.json and terrain/hf.npy,
        /// terrain/excluded_tris.npy from the work store and the level's assets from the disc; writes race.json and race/.
        public static UxRaceResult Convert(UxContext ctx, UxRaceOptions options = null, UxRaceTemplate template = null, Action<string> progress = null)
            => Convert(ctx.disc, ctx.stock, ctx.work, options, progress ?? ctx.log, ctx.cancel, template);

        public static UxRaceResult Convert(UxDiscLevel disc, UxStock stock, UxWork work, UxRaceOptions options, Action<string> progress, CancellationToken cancel, UxRaceTemplate template = null)
        {
            var clock = System.Diagnostics.Stopwatch.StartNew();
            var opt = options ?? new UxRaceOptions();
            var lv = disc.lv;
            string slot_name = opt.slotName ?? UxCommon.SlotName("sx", UxCommon.DefaultSlot);
            var res = BuildRace(disc, stock, work, opt, slot_name, template, progress, cancel);
            var r = res.race;
            // ---- run_level: the files
            var assets = new JObj();
            foreach (var (name, type, data) in res.assets) assets.Set(name + "." + type, new JObj().Set("bytes", (long)data.Length).Set("md5", UxBin.Md5(data)));
            r.Set("assets", assets);
            var tdf = UxJson.Obj(r.Get("tdf"));
            UxBin.Require(UxBin.Md5(res.tdf) == UxJson.Str(UxJson.Get(tdf, "facts", "md5")), "ux_race: the tdf bytes do not have the recorded md5");
            string tdfRel = "race/" + slot_name + "-sx.tdf";
            foreach (string old in work.List("race")) work.Delete(old);          // the folder holds this run's files only
            work.WriteBytes(tdfRel, res.tdf);
            tdf.Set("file", tdfRel);
            foreach (var (name, type, data) in res.assets) work.WriteBytes("race/" + name + "." + type, data);
            string lpp = "race/" + slot_name + "-lappacing.bxml";
            if (res.lapPacing != null) { work.WriteBytes(lpp, res.lapPacing); UxJson.Obj(r.Get("lappacing")).Set("file", lpp); }
            else work.Delete(lpp);
            work.WriteJson("race.json", r);
            res.seconds = clock.Elapsed.TotalSeconds;
            progress?.Invoke("S8 race " + lv.disc + ": tdf " + res.tdf.Length + " B, " + UxJson.Obj(r.Get("moves")).items.Count + " moves, " + res.assets.Count + " patch assets (" +
                res.seconds.ToString("0.0", CultureInfo.InvariantCulture) + " s)");
            return res;
        }

        /// build_race(level, slot_name, ...): the race dict (see ux_race.py's module doc) and the asset bytes. Writes nothing.
        public static UxRaceResult BuildRace(UxDiscLevel disc, UxStock stock, UxWork work, UxRaceOptions opt, string slot_name, UxRaceTemplate template, Action<string> progress, CancellationToken cancel)
        {
            if (opt.dropgateMaterials != "stock9" && opt.dropgateMaterials != "template") throw new UxException("dropgate_materials must be one of ('stock9', 'template'), not '" + opt.dropgateMaterials + "'");
            if (opt.startgateVisual != "keep" && opt.startgateVisual != "hide") throw new UxException("startgate_visual must be one of ('keep', 'hide'), not '" + opt.startgateVisual + "'");
            var lv = disc.lv;
            byte[] levelBytes = work.ReadBytes("level.json");
            var d = UxJson.Obj(UxJson.ParseUtf8(levelBytes));
            var tp = template ?? new UxRaceTemplate(stock);
            var probs = new List<string>(); var warns = new List<string>();
            double lift = UxCommon.YLift(lv);
            if (d.Has("y_lift") && UxJson.Num(d.Get("y_lift")) != lift) throw new UxException(lv.disc + ": level.json y_lift " + UxRaceNum.PyRepr(d.Get("y_lift")) + " != ux_common.y_lift " + UxRaceNum.PyRepr(lift) + " (rerun ux_level)");
            var ents = Entities(d);
            // ---------------------------------------------------------------- 1. tdf
            byte[] raw = disc.ReadTdf(cancel);
            var rp = UxJson.Obj(d.Get("race_poses"));
            var dg = rp.Get("DropGate") as JObj ?? throw new UxException(lv.disc + ": no DropGate pose in level.json");
            var dgm = UxJson.Arr(dg.Get("matrix"));
            var G = Mat4(dgm);
            var gz = FlatDir(G[8], G[10]);
            byte[] tdfOut; JObj info;
            if (opt.minimal) tdfOut = UxTdf07.Convert(raw, out info, false, false, "off");
            else
            {
                double nz = UxRaceNum.PyHypot(G[8], G[10]); if (nz == 0) nz = 1.0;
                tdfOut = UxTdf07.Convert(raw, out info, true, true, "auto", new[] { G[12], G[14] }, new[] { G[8] / nz, G[10] / nz });
            }
            long flags_n = 0;
            if (opt.sxFlags != null) tdfOut = PatchSxFlags(tdfOut, opt.sxFlags.Value, out flags_n);
            JObj pad_info = null;
            if (opt.tdfPad == "zero") tdfOut = UxTdf07.ZeroLinePadding(tdfOut, out pad_info);
            else if (opt.tdfPad != "keep") throw new UxException("tdf_pad must be zero or keep, not '" + opt.tdfPad + "'");
            var facts = TdfFacts(tdfOut, out var glob, out var secs);
            // the research folder of the reference (summary.csv, the canonical converter output) is not part of an import: the TDF's own G5
            object g5 = facts.Get("lap_G5");
            var tdf = new JObj().Set("asset", slot_name + "-sx").Set("type", "tdf").Set("source", "AI/" + lv.tdf).Set("source_md5", UxBin.Md5(raw)).Set("source_mode", lv.mode)
                .Set("mode", opt.minimal ? "minimal" : "default").Set("sx_flags", opt.sxFlags == null ? null : (object)opt.sxFlags.Value).Set("sx_flags_sections", flags_n)
                .Set("conversion", info).Set("facts", facts).Set("research_G5", g5).Set("G5_source", "TDF global 5 (research summary.csv missing)")
                .Set("research_tdf", null).Set("equals_research_tdf", null).Set("tdf_pad", opt.tdfPad).Set("line_padding", pad_info);
            // ---------------------------------------------------------------- lap pacing (opt-in)
            byte[] lp_bytes = LapPacing(disc, opt, slot_name, out JObj lp, probs, cancel);
            // ---------------------------------------------------------------- terrain, HF, collision
            cancel.ThrowIfCancellationRequested();
            var idx = new ExtractIndex(d);
            var collisCache = new Dictionary<string, double[]>(StringComparer.Ordinal);
            var tq = new TerrainQuery(lv, disc, d, cancel);
            var hf = new HeightField(lv, work, () => tq);
            var cw = new CollisionWorld(lv, disc, d, ents, idx, collisCache, null, cancel);
            // ---------------------------------------------------------------- start-pack bale edges (fix round 'fixrace')
            double bec = opt.baleEdgeClear;
            if (bec < 0) throw new UxException("bale_edge_clear must be >= 0 ft, not " + UxRaceNum.PyRepr(bec));
            if (bec > 0)
            {
                var out2 = UxStartPack.EdgeClear(tdfOut, UxStartPack.Footprints(ents, cw), hf.At, bec, out JObj brep);
                foreach (object p in UxJson.Arr(brep.Get("problems"))) probs.Add("bale edge clear: " + p);
                if (!ReferenceEquals(out2, tdfOut))
                {
                    tdfOut = out2;
                    facts = TdfFacts(tdfOut, out glob, out secs);
                    tdf.Set("facts", facts);
                }
                tdf.Set("bale_edge_clear", brep);
            }
            // ---------------------------------------------------------------- ridable meshes over respawn points (fix round)
            cancel.ThrowIfCancellationRequested();
            if (opt.deckRespawn != "engine" && opt.deckRespawn != "auto" && opt.deckRespawn != "off") throw new UxException("deck_respawn must be one of ('engine', 'auto', 'off'), not '" + opt.deckRespawn + "'");
            var dk = new JObj().Set("option", opt.deckRespawn).Set("threshold_ft", DECK_NORESPAWN_FT).Set("nodes", new List<object>()).Set("changed", 0L);
            if (opt.deckRespawn == "auto" || opt.deckRespawn == "engine")
            {
                // ridable_mesh_tris
                var parts = new List<double[]>(); var winfo = new JObj();
                var tpt = TerrainPartTris(work, tq, lift, out long ntp);
                if (tpt != null) { if (tpt.Length > 0) parts.Add(tpt); winfo.Set("terrain_part_tris", ntp); }
                bool anyColl = false; foreach (var e in ents) if (e.collision && e.Kept && e.reflex == "CollisionRSData") { anyColl = true; break; }
                var rid = anyColl ? RidableCollision(disc, ents, idx, tq, collisCache, cancel) : new SortedDictionary<long, JObj>();
                if (rid.Count > 0 && cw.Count > 0)
                {
                    var rt = OwnerTris(cw, rid.Keys, true, out long nrt);
                    if (nrt > 0) parts.Add(rt);
                    var ids = new List<object>(); foreach (long k in rid.Keys) ids.Add(k);
                    winfo.Set("ridable_entities", ids).Set("ridable_entity_tris", nrt);
                }
                var Wd = new MeshSet(Concat(parts));
                dk.Set("meshes", winfo).Set("mesh_tris", (long)Wd.Count);
                var recs = new List<JObj>(); byte[] out2;
                if (opt.deckRespawn == "auto") out2 = DeckNorespawn(tdfOut, Wd, hf, recs);
                else
                {
                    // static_mesh_tris
                    var sparts = new List<double[]>(); var binfo = new JObj();
                    if (tpt != null) { if (tpt.Length > 0) sparts.Add(tpt); binfo.Set("terrain_part_tris", ntp); }
                    var dyn = new HashSet<long>();
                    foreach (var e in ents) if (Array.IndexOf(PHYSICS_FAMILY, e.reflex) >= 0 || Array.IndexOf(PHYSICS_FAMILY, e.type) >= 0) dyn.Add(e.id);
                    if (cw.Count > 0)
                    {
                        var owners = new HashSet<long>();
                        var st = OwnerTris(cw, dyn, false, out long nst, owners);
                        if (nst > 0) sparts.Add(st);
                        binfo.Set("static_entity_tris", nst).Set("static_entities", (long)owners.Count);
                    }
                    var Wb = new MeshSet(Concat(sparts));
                    dk.Set("static", binfo);
                    out2 = DeckNorespawnEngine(tdfOut, Wd, Wb, hf, recs, out JObj erep);
                    dk.Set("engine", erep);
                }
                long nchanged = 0; var nodesList = new List<object>();
                foreach (var r in recs) { nodesList.Add(r); if ((bool)r.Get("changed")) nchanged++; }
                dk.Set("nodes", nodesList).Set("changed", nchanged);
                if (!ReferenceEquals(out2, tdfOut))
                {
                    dk.Set("md5_before", UxBin.Md5(tdfOut)).Set("md5_after", UxBin.Md5(out2));
                    tdfOut = out2;
                    facts = TdfFacts(tdfOut, out glob, out secs);
                    tdf.Set("facts", facts);
                }
            }
            tdf.Set("deck_norespawn", dk);
            cancel.ThrowIfCancellationRequested();
            // the Untamed start-gate top structure (kept CollisionRSData on the gate pose): reported separately
            var gate_top_ids = new List<long>();
            foreach (var e in ents)
                if (e.Kept && Regex.IsMatch(e.Asset("ModelAsset") ?? "", @"\A(?:(StartingGate|Split_Gate)_\d+_Top$)")) gate_top_ids.Add(e.id);
            var cw_nt = gate_top_ids.Count > 0 ? new CollisionWorld(lv, disc, d, ents, idx, collisCache, gate_top_ids, cancel) : cw;
            // track materials: the trncol materials under the lap and chute centre lines
            var mc = new Dictionary<int, long>(); long nsamp = 0;
            foreach (var s in secs)
            {
                uint k = UxTdf07.SectionKind(s);
                if (k != 0 && k != 4) continue;
                foreach (var n in s.nodes)
                {
                    var c = UxTdf07.Vec3(UxTdf07.Field(UxTdf07.NodeDict(n), 21)); var t = tq.Top(c[0], c[2]);
                    nsamp++;
                    if (t != null) mc[t.Value.mat] = mc.TryGetValue(t.Value.mat, out long cnt) ? cnt + 1 : 1;
                }
            }
            var track_mats = new List<int>();
            foreach (var kv in mc) if (kv.Value >= 0.05 * nsamp) track_mats.Add(kv.Key);
            track_mats.Sort();
            // chute geometry
            var chutes = new List<int>(); for (int k = 0; k < secs.Count; k++) if (UxTdf07.SectionKind(secs[k]) == 4) chutes.Add(k);
            var chute_lines = new List<Chute>(); var chute_polys = new List<double[][]>();
            foreach (int ci in chutes)
            {
                var ch = new Chute { ci = ci }; var l = new List<double[]>(); var r = new List<double[]>();
                foreach (var n in secs[ci].nodes)
                {
                    var P = UxTdf07.NodeDict(n);
                    var c3 = UxTdf07.Vec3(UxTdf07.Field(P, 21)); var l3 = UxTdf07.Vec3(UxTdf07.Field(P, 22)); var r3 = UxTdf07.Vec3(UxTdf07.Field(P, 23));
                    ch.c.Add(new[] { c3[0], c3[2] }); ch.dirs.Add(UxTdf07.Vec3(UxTdf07.Field(P, 26))); l.Add(l3); r.Add(r3);
                    ch.m.Add(new[] { (l3[0] + r3[0]) / 2, (l3[2] + r3[2]) / 2 });
                }
                if (l.Count == 0) throw new UxException("tdf: start chute " + ci + " has no node");
                ch.w0 = UxRaceNum.PyHypot(l[0][0] - r[0][0], l[0][2] - r[0][2]);
                chute_lines.Add(ch);
                for (int i = 0; i + 1 < l.Count; i++)
                    chute_polys.Add(new[] { new[] { l[i][0], l[i][2] }, new[] { l[i + 1][0], l[i + 1][2] }, new[] { r[i + 1][0], r[i + 1][2] }, new[] { r[i][0], r[i][2] } });
            }
            // ---------------------------------------------------------------- 2. gate group
            double[] gpos = { G[12], G[13], G[14] };
            bool haveNear = false; double nearD = 0; int nearCi = 0, nearK = 0; Chute nearCh = null;
            foreach (var cl in chute_lines)
                for (int k = 0; k < cl.c.Count; k++)
                {
                    double dd = UxRaceNum.PyHypot(cl.c[k][0] - gpos[0], cl.c[k][1] - gpos[2]);
                    if (!haveNear || dd < nearD) { haveNear = true; nearD = dd; nearCi = cl.ci; nearK = k; nearCh = cl; }
                }
            var chute_dir = haveNear ? FlatDir(nearCh.dirs[nearK][0], nearCh.dirs[nearK][2]) : gz;
            double gate_dot = UxRaceNum.Dot(gz, chute_dir);
            var Gn = Unscaled(G);
            bool gate_flipped = gate_dot < 0;
            if (gate_flipped)
            {
                Gn = UxRaceNum.MatMul4(RotY180(), Gn);
                warns.Add("Untamed DropGate +Z points up the chute (dot " + UxRaceNum.PyFixed(gate_dot, 2) + "): turned 180 deg");
            }
            var drop_new = (double[])Gn.Clone(); drop_new[12] = G[12]; drop_new[13] = G[13]; drop_new[14] = G[14]; drop_new[13] += lift;
            var want_pose = !gate_flipped ? Unscaled(G) : UxRaceNum.MatMul4(RotY180(), Unscaled(G));
            bool dropgate_pose_ok = true;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (!UxRaceNum.AllClose(drop_new[4 * i + j], want_pose[4 * i + j], 1e-6)) dropgate_pose_ok = false;
            {
                double[] wantT = { G[12] + 0.0, G[13] + lift, G[14] + 0.0 };
                for (int j = 0; j < 3; j++) if (!UxRaceNum.AllClose(drop_new[12 + j], wantT[j], 1e-4)) dropgate_pose_ok = false;
            }
            var sg_new = (double[])drop_new.Clone();
            {
                var f = FlatDir(drop_new[8], drop_new[10]);
                for (int j = 0; j < 3; j++) sg_new[12 + j] = drop_new[12 + j] - START_GATE_BEHIND * f[j];
            }
            // the signed lateral offset (along the gate's x axis) of each chute's centre line where it crosses the gate's lateral line
            var gx2f = FlatDir(G[0], G[2]); double gx0 = gx2f[0], gx1 = gx2f[2];
            double? Lateral(List<double[]> poly)
            {
                double? best = null;
                for (int i = 0; i + 1 < poly.Count; i++)
                {
                    var a = poly[i]; var b = poly[i + 1];
                    double m01 = -(b[0] - a[0]), m11 = -(b[1] - a[1]);
                    if (Math.Abs(UxRaceNum.Det2(gx0, m01, gx1, m11)) < 1e-9) continue;
                    var tu = UxRaceNum.Solve(new[] { gx0, m01, gx1, m11 }, new[] { a[0] - gpos[0], a[1] - gpos[2] }, 2);
                    if (-1e-6 <= tu[1] && tu[1] <= 1 + 1e-6 && (best == null || Math.Abs(tu[0]) < Math.Abs(best.Value))) best = tu[0];
                }
                if (best == null && poly.Count > 1)
                {   // the gate behind the first node: extend the first segment
                    var a = poly[0]; var b = poly[1];
                    double m01 = -(b[0] - a[0]), m11 = -(b[1] - a[1]);
                    if (Math.Abs(UxRaceNum.Det2(gx0, m01, gx1, m11)) > 1e-9) best = UxRaceNum.Solve(new[] { gx0, m01, gx1, m11 }, new[] { a[0] - gpos[0], a[1] - gpos[2] }, 2)[0];
                }
                return best;
            }
            var gate_line_detail = new List<JObj>(); var latTdf = new List<double?>(); var latEdges = new List<double?>();
            foreach (var cl in chute_lines)
            {
                double d00, d01;
                if (cl.c.Count > 1) { d00 = cl.c[1][0] - cl.c[0][0]; d01 = cl.c[1][1] - cl.c[0][1]; } else { d00 = 1.0; d01 = 0.0; }
                double dn = UxRaceNum.Norm2(d00, d01); if (dn == 0) dn = 1;
                d00 = d00 / dn; d01 = d01 / dn;
                double? lt = Lateral(cl.c), le = Lateral(cl.m);
                latTdf.Add(lt); latEdges.Add(le);
                gate_line_detail.Add(new JObj().Set("section", (long)cl.ci).Set("lateral_tdf_ft", lt == null ? null : (object)lt.Value).Set("lateral_edges_ft", le == null ? null : (object)le.Value)
                    .Set("along_from_first_node_ft", UxNp.PyRound(UxRaceNum.Dot2(gpos[0] - cl.c[0][0], gpos[2] - cl.c[0][1], d00, d01), 2))
                    .Set("width_first_node_ft", UxNp.PyRound(cl.w0, 1)));
            }
            double? Offset(List<double?> vals)
            {
                var v = new List<double>(); foreach (var x in vals) if (x != null) v.Add(x.Value);
                if (v.Count == 0) return null;
                if (v.Count >= 2)
                {
                    UxNp.StableSort(v, (a, b) => Math.Abs(a).CompareTo(Math.Abs(b)));
                    double v0 = v[0], v1 = v[1];
                    if (v0 * v1 < 0) return Math.Abs(UxRaceNum.PySum(new[] { v0, v1 }) / 2);
                    return Math.Abs(v0) <= Math.Abs(v1) ? Math.Abs(v0) : Math.Abs(v1);
                }
                return Math.Abs(v[0]);
            }
            double? gate_off_tdf = Offset(latTdf), gate_off_edges = Offset(latEdges);
            string split = chutes.Count > 1 ? " (midline of the 2 chutes)" : "";
            var gate_line = L(L(gate_off_edges == null ? null : (object)UxNp.PyRound(gate_off_edges.Value, 2), "chute centre = edge midline (22/23)" + split),
                L(gate_off_tdf == null ? null : (object)UxNp.PyRound(gate_off_tdf.Value, 2), "TDF centre nodes (21, a racing line)" + split));
            foreach (var g_ in gate_line_detail)
                foreach (string k in new[] { "lateral_tdf_ft", "lateral_edges_ft" })
                    if (g_.Get(k) != null) g_.Set(k, UxNp.PyRound((double)g_.Get(k), 2));
            bool gate_line_ok = gate_off_edges != null && gate_off_edges.Value <= GATE_LINE_TOL_FT;
            bool gate_line_tdf_ok = gate_off_tdf != null && gate_off_tdf.Value <= GATE_LINE_TOL_FT;
            // ---------------------------------------------------------------- frames, moves
            var moves = new JObj();
            UxRaceTemplate.Entity Tm(string name)
            {
                var t = tp.Find(name);
                if (t == null) probs.Add("template entity " + name + " not found");
                return t;
            }
            // follow: 'auto' = ground-follow when > 5 ft from the frame origin, 'always', or 'never'
            void Move(string name, double[] newM, string group, string rule, double[] frame_old, double[] frame_new, string follow = "auto", JObj props = null)
            {
                var t = Tm(name);
                if (t == null) return;
                var old = Mat4(t.matrix);
                double[] local = frame_old != null ? UxRaceNum.MatMul4(old, UxRaceNum.Inv4(frame_old)) : null;
                if (newM == null) newM = UxRaceNum.MatMul4(local, frame_new);
                newM = (double[])newM.Clone();
                double dist = local != null ? UxRaceNum.NpHypot(local[12], local[14]) : 0.0;
                string how = rule;
                if (old[13] < PARKED_Y) { newM[13] = old[13]; how += "; parked y kept"; }
                else if ((follow == "auto" || follow == "always") && t.type == "AnchorEntityRSData" && Math.Abs(old[13] - tp.floor) < GROUND_STAND_TOL_FT && hf.At(newM[12], newM[14]) != null)
                {
                    double g_new = hf.At(newM[12], newM[14]).Value;
                    newM[13] = g_new + (old[13] - tp.floor);
                    how += "; stands on the template floor (y " + UxRaceNum.PyFixed(old[13], 2) + "): on the new ground (" + UxRaceNum.PyFixed(old[13] - tp.floor, 2) + " ft above)";
                }
                else if (follow == "always" || follow == "auto" && dist > GROUND_FOLLOW_MIN_FT ||
                    follow == "auto" && t.type == "AnchorEntityRSData" && Math.Abs(old[13] - tp.Ground(old[12], old[14])) < GROUND_STAND_TOL_FT)
                {
                    double g_old = tp.Ground(old[12], old[14]); var g_new = hf.At(newM[12], newM[14]);
                    if (g_new != null) { newM[13] = g_new.Value + (old[13] - g_old); how += "; ground-follow (" + UxRaceNum.PyFixed(old[13] - g_old, 2) + " ft above ground)"; }
                }
                var om = new List<object>(16); foreach (double x in t.matrix) om.Add(x);
                var rec = new JObj().Set("type", t.type).Set("group", group).Set("rule", how).Set("matrix", F32List(newM)).Set("old_matrix", om)
                    .Set("position", L(UxNp.PyRound(newM[12], 3), UxNp.PyRound(newM[13], 3), UxNp.PyRound(newM[14], 3))).Set("yaw_deg", UxNp.PyRound(YawDeg(newM), 2));
                moves.Set(name, rec);
                if (props != null && props.items.Count > 0)
                {
                    var op = new JObj();
                    foreach (var kv in props.items) op.Set(kv.Key, tp.props.TryGetValue(name, out var tpp) ? tpp.Get(kv.Key) : null);
                    rec.Set("props", props).Set("old_props", op);
                }
            }
            string DG = tp.dropgate;
            var Gt_old = Tm(DG);
            var G_old = Gt_old != null ? Unscaled(Gt_old.matrix) : Eye4();
            Move(DG, drop_new, "gate", "Untamed DropGate matrix + " + UxRaceNum.PyFixed(lift, 0) + " ft", G_old, drop_new, "never");
            Move("TO_StartingGate", sg_new, "gate", "drop gate heading, local z -" + UxRaceNum.PyFixed(START_GATE_BEHIND, 1) + " ft", G_old, drop_new, "never");
            var SG_old = tp.Find("TO_StartingGate") != null ? Unscaled(tp.Find("TO_StartingGate").matrix) : G_old;
            foreach (string nm in tp.startgate_followers) Move(nm, null, "gate", "rigid with TO_StartingGate", SG_old, sg_new, "never");
            foreach (string nm in tp.Match(tp.gate_patterns)) Move(nm, null, "gate", "rigid with TO_DropGate", G_old, drop_new);
            // board: the board matrix = the Untamed home pose, ShowSignPosition = Untamed + lift, the rest of the group rigid with a show frame
            var bp = rp.Get("ThirtySecondBoard") as JObj;
            JObj board = null;
            string tb_name = null; foreach (var e in tp.entities) if (e.type == "ThirtySecondBoardRSData") { tb_name = e.name; break; }
            LevelEntity ub = null;
            if (bp != null) { long bid = UxJson.Int(bp.Get("id")); foreach (var e in ents) if (e.id == bid) { ub = e; break; } }
            if (bp == null || tb_name == null || ub == null) probs.Add("no ThirtySecondBoard pose (level.json) or template board");
            else
            {
                var Bm = Mat4(UxJson.Arr(bp.Get("matrix"))); var Bn = Unscaled(Bm);
                double bdot = UxRaceNum.Dot(FlatDir(Bn[8], Bn[10]), FlatDir(drop_new[8], drop_new[10]));
                bool flip = opt.boardFlip && bdot > 0;
                if (flip) Bn = UxRaceNum.MatMul4(RotY180(), Bn);
                Bn[12] = Bm[12]; Bn[13] = Bm[13]; Bn[14] = Bm[14]; Bn[13] += lift;
                var B_old = Unscaled(tp.Find(tb_name).matrix);
                var t_show = tp.props.TryGetValue(tb_name, out var tprops) ? tprops.Get("ShowSignPosition") as List<object> : null;
                var S_old = (double[])B_old.Clone();
                if (t_show != null) for (int j = 0; j < 3; j++) S_old[12 + j] = UxJson.Num(t_show[j]);
                var u_show = ub.props.Get("ShowSignPosition") as List<object>;
                if (u_show == null) { probs.Add("Untamed ThirtySecondBoard has no ShowSignPosition"); u_show = L(Bm[12], Bm[13], Bm[14]); }
                double[] show = { (double)(float)UxJson.Num(u_show[0]) + 0.0, (double)(float)UxJson.Num(u_show[1]) + lift, (double)(float)UxJson.Num(u_show[2]) + 0.0 };
                var S_new = Eye4(); var fd = FlatDir(drop_new[8], drop_new[10]); double[] zf = { -fd[0], -fd[1], -fd[2] };
                var xf = Cross(new[] { 0.0, 1.0, 0.0 }, zf);
                S_new[8] = zf[0]; S_new[9] = zf[1]; S_new[10] = zf[2]; S_new[4] = 0; S_new[5] = 1; S_new[6] = 0; S_new[0] = xf[0]; S_new[1] = xf[1]; S_new[2] = xf[2];
                S_new[12] = show[0]; S_new[13] = show[1]; S_new[14] = show[2];
                Move(tb_name, Bn, "board", "Untamed ThirtySecondBoard home pose (unscaled" + (flip ? ", turned 180 deg to the stock heading" : "") + ") + " + UxRaceNum.PyFixed(lift, 0) +
                    " ft; ShowSignPosition = Untamed + " + UxRaceNum.PyFixed(lift, 0) + " ft", B_old, Bn, "never", new JObj().Set("ShowSignPosition", F32List(show)));
                Move(tp.board_camera, null, "board", "rigid with the show frame (ShowSignPosition, facing the gate)", S_old, S_new, "never");
                foreach (string nm in tp.Match(tp.board_patterns)) Move(nm, null, "board", "rigid with the show frame (ShowSignPosition, facing the gate)", S_old, S_new);
                double tdot = UxRaceNum.Dot(FlatDir(B_old[8], B_old[10]), FlatDir(G_old[8], G_old[10]));
                double rel0 = show[0] - drop_new[12], rel1 = show[2] - drop_new[14];
                var gzf = FlatDir(drop_new[8], drop_new[10]); var gxf = FlatDir(drop_new[0], drop_new[2]);
                var scale = new List<object>(); for (int i = 0; i < 3; i++) scale.Add(UxNp.PyRound(UxRaceNum.Norm3(Bm[4 * i], Bm[4 * i + 1], Bm[4 * i + 2]), 4));
                board = new JObj().Set("untamed_matrix", bp.Get("matrix")).Set("scale", scale).Set("untamed_dot_gate", UxNp.PyRound(bdot, 3)).Set("template_dot_gate", UxNp.PyRound(tdot, 3))
                    .Set("flipped", flip).Set("new_dot_gate", UxNp.PyRound(UxRaceNum.Dot(FlatDir(Bn[8], Bn[10]), FlatDir(drop_new[8], drop_new[10])), 3))
                    .Set("home_distance_from_gate_ft", UxNp.PyRound(UxRaceNum.NpHypot(Bn[12] - drop_new[12], Bn[14] - drop_new[14]), 1))
                    .Set("show_sign_position", L(UxNp.PyRound(show[0], 3), UxNp.PyRound(show[1], 3), UxNp.PyRound(show[2], 3)))
                    .Set("show_rel_gate", new JObj().Set("along_ft", UxNp.PyRound(UxRaceNum.Dot2(rel0, rel1, gzf[0], gzf[2]), 1)).Set("lateral_ft", UxNp.PyRound(UxRaceNum.Dot2(rel0, rel1, gxf[0], gxf[2]), 1)))
                    .Set("template_show_sign_position", t_show).Set("hf_at_show", hf.At(show[0], show[2]) is double hs ? (object)hs : null).Set("walk_speeds_kept_from_template", true);
            }
            // finish anchor on line 9
            var L9 = UxTdf07.LineRec(UxTdf07.First(glob, 9) ?? throw new UxException("tdf: no finish line (global 9)"));
            JObj win = null;
            if (tp.line9 == null) probs.Add("template tdf has no line 9");
            else
            {
                var F_old = LineFrame(tp.line9.pos, tp.line9.dir); var F_new = LineFrame(L9.pos, L9.dir);
                Move("Anchor_WinFinish", null, "finish", "rigid with TDF finish line 9 (pos + travel direction)", F_old, F_new, "always");
                win = new JObj().Set("line9_pos", L(UxNp.PyRound(L9.pos[0], 2), UxNp.PyRound(L9.pos[2], 2))).Set("line9_dir", L(UxNp.PyRound(L9.dir[0], 3), UxNp.PyRound(L9.dir[2], 3)));
            }
            var unmoved = new List<object>();
            foreach (var e in tp.entities) if (!moves.Has(e.name) && (e.type == "AnchorEntityRSData" || e.type == "MXCameraPathRSData")) unmoved.Add(e.name);
            // ---------------------------------------------------------------- 5. GateStart nodes
            cancel.ThrowIfCancellationRequested();
            NodeRec NodeCheck(string nm, double[] Lm)
            {
                var W = UxRaceNum.MatMul4(Lm, sg_new);
                double x = W[12], y = W[13], z = W[14];
                var g = hf.At(x, z);
                var top = tq.Top(x, z);
                double? surf = top == null ? (double?)null : top.Value.y + lift;
                string mat = top == null ? null : top.Value.mat >= 0 ? tq.mats[top.Value.mat] : "collision-only";
                bool on_track_mat = top != null && track_mats.Contains(top.Value.mat) && top.Value.ny > 0.7;
                bool hf_ok = g != null && surf != null && Math.Abs(g.Value - surf.Value) <= HF_TOL_FT;
                double clr = cw.Clearance(x, g ?? y, z, out long? who);
                double clr2; long? who2;
                if (ReferenceEquals(cw_nt, cw)) { clr2 = clr; who2 = who; } else clr2 = cw_nt.Clearance(x, g ?? y, z, out who2);
                bool clear = clr >= NODE_CLEAR_FT;
                bool inside = false; foreach (var p in chute_polys) if (InPoly(x, z, p)) { inside = true; break; }
                var j = new JObj().Set("name", nm).Set("local", L(UxNp.PyRound(Lm[12], 3), UxNp.PyRound(Lm[13], 3), UxNp.PyRound(Lm[14], 3)))
                    .Set("world", L(UxNp.PyRound(x, 3), UxNp.PyRound(y, 3), UxNp.PyRound(z, 3)))
                    .Set("hf", g == null ? null : (object)UxNp.PyRound(g.Value, 3)).Set("surface", surf == null ? null : (object)UxNp.PyRound(surf.Value, 3))
                    .Set("material", mat).Set("normal_y", top == null ? null : (object)UxNp.PyRound(top.Value.ny, 3))
                    .Set("track_material", on_track_mat).Set("hf_within_1ft", hf_ok)
                    .Set("clearance_ft", !UxNp.Finite(clr) ? null : (object)UxNp.PyRound(clr, 2)).Set("nearest_collision", who == null ? null : (object)who.Value)
                    .Set("clear_3ft", clear).Set("in_chute_corridor", inside)
                    .Set("clearance_without_gate_top_ft", !UxNp.Finite(clr2) ? null : (object)UxNp.PyRound(clr2, 2))
                    .Set("nearest_collision_without_gate_top", who2 == null ? null : (object)who2.Value)
                    .Set("on_chute_dirt", hf_ok && clear).Set("gate_y_minus_hf", g == null ? null : (object)UxNp.PyRound(y - g.Value, 3));
                return new NodeRec
                {
                    j = j, name = nm, onDirt = hf_ok && clear, inCorr = inside, hfOk = hf_ok, clr2 = !UxNp.Finite(clr2) ? (double?)null : UxNp.PyRound(clr2, 2),
                    local = new[] { UxNp.PyRound(Lm[12], 3), UxNp.PyRound(Lm[13], 3), UxNp.PyRound(Lm[14], 3) }, world = new[] { UxNp.PyRound(x, 3), UxNp.PyRound(y, 3), UxNp.PyRound(z, 3) },
                };
            }
            var tnodes = GateNodes(tp.gate_model);
            var nodes = new List<NodeRec>(); foreach (var tn in tnodes) nodes.Add(NodeCheck(tn.name, tn.L));
            var template_nodes = nodes;
            // WP13c start-gate node patch: when a template GateStart node is off chute dirt or outside every start-chute corridor
            JObj node_patch = null;
            var result = new UxRaceResult();
            void SetAsset(string name, string type, byte[] data)
            {
                int at = result.assets.FindIndex(a => a.name == name && a.type == type);
                if (at >= 0) result.assets[at] = (name, type, data); else result.assets.Add((name, type, data));
            }
            byte[] GetAsset(string name, string type) { foreach (var a in result.assets) if (a.name == name && a.type == type) return a.data; return null; }
            var sg_props = new JObj();
            var need = new List<object>(); foreach (var n in nodes) if (!(n.onDirt && n.inCorr)) need.Add(n.name);
            if (need.Count > 0 && opt.gatePatch != "off")
            {
                node_patch = PatchGateNodes(disc, d, ents, lift, tp, sg_new, tnodes, NodeCheck, chute_lines, out List<NodeRec> pnodes, out byte[] pmodel, cancel);
                node_patch.Set("trigger", "template nodes off chute dirt or outside the chute corridors: " + UxRaceNum.PyRepr(need));
                var pp = UxJson.Arr(node_patch.Get("problems"));
                if (pp.Count > 0) foreach (object p in pp) probs.Add("start-gate node patch: " + p);
                else
                {
                    nodes = pnodes;
                    SetAsset(GATE_MODEL_NAME, "model", pmodel);
                    SetAsset(GATE_MODEL_NAME, "surface", tp.Read("GL_StartGate", "surface"));
                    sg_props.Set("ModelAsset", new JObj().Set("asset", GATE_MODEL_NAME).Set("type", "model"));
                    sg_props.Set("SurfaceAsset", new JObj().Set("asset", GATE_MODEL_NAME).Set("type", "surface"));
                }
            }
            else if (need.Count > 0)
                warns.Add("start-gate node patch off (--gate-patch off): " + need.Count + " template GateStart nodes off chute dirt / outside the chutes");
            long n_ok = 0; foreach (var n in nodes) if (n.onDirt) n_ok++;
            // start-gate collision against every TDF section corridor except the start chutes; boxes the lap or a side section runs
            // through are removed in a trimmed copy
            cancel.ThrowIfCancellationRequested();
            var node_local = new List<double[]>(); foreach (var n in nodes) node_local.Add(n.local);
            var col = StartgateCollisionCheck(tp, sg_new, secs, hf, null, node_local);
            JObj col_trim = null; List<long> blocking_after = null;
            if (col.blocking.Count > 0 && opt.gatePatch != "off")
            {
                var keep = new List<int>(); for (int i = 0; i < col.n_prims; i++) if (!col.blocking.ContainsKey(i)) keep.Add(i);
                var data = TrimmedCollis(tp.Read("GL_StartGate_COL", "collis"), keep);
                SetAsset(GATE_COL_NAME, "collis", data);
                sg_props.Set("FemaleCollisionAsset", new JObj().Set("asset", GATE_COL_NAME).Set("type", "collis"));
                sg_props.Set("MaleCollisionAsset", new JObj().Set("asset", GATE_COL_NAME).Set("type", "collis"));
                var after = StartgateCollisionCheck(tp, sg_new, secs, hf, data, node_local);
                blocking_after = new List<long>(after.blocking.Keys);
                var kp = new List<object>(); foreach (int i in keep) kp.Add((long)i);
                var rm = new List<object>(); foreach (long i in col.blocking.Keys) rm.Add(i);
                var ba = new List<object>(); foreach (long i in blocking_after) ba.Add(i);
                col_trim = new JObj().Set("asset", GATE_COL_NAME).Set("kept_prims", kp).Set("removed_prims", rm).Set("md5", UxBin.Md5(data)).Set("bytes", (long)data.Length).Set("blocking_after", ba);
                if (after.blocking.Count > 0) probs.Add("trimmed start-gate collision still blocks " + UxRaceNum.PyRepr(after.Json().Get("blocking"), true));
            }
            else if (col.blocking.Count > 0)
                warns.Add("start-gate collision blocks the lap/side sections at primitives " + UxRaceNum.PyRepr(new List<long>(col.blocking.Keys)) + " (--gate-patch off)");
            // fix round 'gatefix': --startgate-visual hide (the node-bearing model and the collision stay, the surface is hidden)
            JObj hide = null;
            if (opt.startgateVisual == "hide")
            {
                if (GetAsset(GATE_MODEL_NAME, "model") == null)
                {   // no node patch here: a rename-only copy
                    SetAsset(GATE_MODEL_NAME, "model", PatchGateModel(tp.gate_model, new List<KeyValuePair<string, double>>()));
                    sg_props.Set("ModelAsset", new JObj().Set("asset", GATE_MODEL_NAME).Set("type", "model"));
                    sg_props.Set("SurfaceAsset", new JObj().Set("asset", GATE_MODEL_NAME).Set("type", "surface"));
                }
                var src_surf = tp.Read("GL_StartGate", "surface");
                SetAsset(GATE_MODEL_NAME, "surface", HiddenSurface(src_surf));
                string colName = UxJson.Get(sg_props, "FemaleCollisionAsset", "asset") as string;
                if (string.IsNullOrEmpty(colName)) colName = UxJson.Get(tp.sg_props, "FemaleCollisionAsset", "asset") as string;
                hide = new JObj().Set("option", "hide").Set("surface", GATE_MODEL_NAME).Set("model", GATE_MODEL_NAME).Set("point", L(0.0, HIDE_Y_FT, 0.0))
                    .Set("source", "GL_StartGate.surface").Set("source_md5", UxBin.Md5(src_surf)).Set("md5", UxBin.Md5(GetAsset(GATE_MODEL_NAME, "surface")))
                    .Set("model_md5", UxBin.Md5(GetAsset(GATE_MODEL_NAME, "model")))
                    .Set("node_patch", node_patch != null && UxJson.Arr(node_patch.Get("problems")).Count == 0).Set("collision", colName);
            }
            if (sg_props.items.Count > 0)
            {
                if (!moves.Has("TO_StartingGate")) probs.Add("TO_StartingGate move missing: cannot re-point it to the patched assets");
                else
                {
                    var op = new JObj(); foreach (var kv in sg_props.items) op.Set(kv.Key, tp.sg_props.Get(kv.Key));
                    UxJson.Obj(moves.Get("TO_StartingGate")).Set("props", sg_props).Set("old_props", op);
                }
            }
            var tmplMats = new List<object>();
            if (tp.dg_props.Get("MaterialList") is List<object> ml0) foreach (object m in ml0) tmplMats.Add(UxJson.Get(m, "asset"));
            var dgmat = new JObj().Set("option", opt.dropgateMaterials).Set("template", tmplMats);
            if (opt.dropgateMaterials == "stock9")
            {
                if (!moves.Has(DG)) probs.Add(DG + " move missing: cannot write the stock MaterialList");
                else
                {
                    var missing = new List<object>(); foreach (string m in STOCK9_DROPGATE_MATERIALS) if (!tp.ts.Has(m, "material")) missing.Add(m);
                    if (missing.Count > 0) probs.Add("stock TO_DropGate materials not in the template package: " + UxRaceNum.PyRepr(missing));
                    var ml = new List<object>(); var written = new List<object>();
                    foreach (string m in STOCK9_DROPGATE_MATERIALS) { ml.Add(new JObj().Set("asset", m).Set("type", "material")); written.Add(m); }
                    UxJson.Obj(moves.Get(DG)).Set("props", new JObj().Set("MaterialList", ml)).Set("old_props", new JObj().Set("MaterialList", tp.dg_props.Get("MaterialList")));
                    dgmat.Set("written", written).Set("source", "SX_ManchesterB.dx9.scene DropGate (stock)");
                }
            }
            // kept Untamed collision inside the template start-gate box (local x +-49.4, y -1.8..11.2, z -24.8..8.0 around TO_StartingGate)
            var box_hits = new SortedDictionary<long, long>();
            if (cw.Count > 0)
            {
                var inv = UxRaceNum.Inv4(sg_new); var T = cw.tris;
                for (int i = 0; i < cw.Count; i++)
                {
                    int q = 9 * i; bool inside = false;
                    for (int k = 0; k < 7 && !inside; k++)
                    {
                        double px, py, pz;
                        if (k < 3) { px = T[q + 3 * k]; py = T[q + 3 * k + 1]; pz = T[q + 3 * k + 2]; }
                        else if (k == 3) { px = ((T[q] + T[q + 3]) + T[q + 6]) / 3; py = ((T[q + 1] + T[q + 4]) + T[q + 7]) / 3; pz = ((T[q + 2] + T[q + 5]) + T[q + 8]) / 3; }
                        else { int a = 3 * (k - 4), b = 3 * ((k - 4 + 2) % 3); px = (T[q + a] + T[q + b]) / 2; py = (T[q + a + 1] + T[q + b + 1]) / 2; pz = (T[q + a + 2] + T[q + b + 2]) / 2; }
                        UxRaceNum.Transform(inv, px, py, pz, out double lx, out double ly, out double lz);
                        inside = Math.Abs(lx) <= 49.4 && ly >= -1.8 && ly <= 11.2 && lz >= -24.8 && lz <= 8.0;
                    }
                    if (inside) box_hits[cw.owner[i]] = box_hits.TryGetValue(cw.owner[i], out long c) ? c + 1 : 1;
                }
            }
            var names = new Dictionary<long, string>(); foreach (var e in ents) names[e.id] = e.name;
            var gate_box = new List<object>();
            foreach (var kv in box_hits) gate_box.Add(L(kv.Key, names.TryGetValue(kv.Key, out string nm2) ? nm2 : null, kv.Value));
            // does the lap run through the start gate?
            var lap = UxTdf07.LapWalk(glob, secs, out _);
            var lap_cross = new JObj(); object lap_min = null;
            if (nodes.Count > 0)
            {
                double[] ga = { nodes[0].world[0], nodes[0].world[2] }, gb = { nodes[nodes.Count - 1].world[0], nodes[nodes.Count - 1].world[2] };
                double O(double[] a, double[] b, double[] c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
                bool Crosses(double[] p1, double[] p2, double[] q1, double[] q2) => O(p1, p2, q1) * O(p1, p2, q2) < 0 && O(q1, q2, p1) * O(q1, q2, p2) < 0;
                List<double[]> LapPts(uint id)
                {
                    var pts = new List<double[]>(lap.Count);
                    foreach (var (a, b) in lap) { var v = UxTdf07.Vec3(UxTdf07.Field(UxTdf07.NodeDict(secs[a].nodes[b]), id)); pts.Add(new[] { v[0], v[2] }); }
                    return pts;
                }
                foreach (var (nm_, id) in new[] { ("centre", 21u), ("left", 22u), ("right", 23u) })
                {
                    var pts = LapPts(id); long cnt = 0;
                    for (int i = 0; i < pts.Count; i++) if (Crosses(pts[i], pts[(i + 1) % pts.Count], ga, gb)) cnt++;
                    lap_cross.Set(nm_, cnt);
                }
                var cpts = LapPts(21); double mn = double.PositiveInfinity; bool first = true;
                foreach (var p in cpts) { double sd = SegDist(p, ga, gb); if (first || sd < mn) mn = sd; first = false; }
                lap_min = UxNp.PyRound(mn, 1);
            }
            bool anyCross = false; foreach (var kv in lap_cross.items) if ((long)kv.Value != 0) anyCross = true;
            if (anyCross && col_trim == null && col.blocking.Count > 0)
                warns.Add("the lap crosses the start gate (GateStart_01..16 line) " + UxRaceNum.PyRepr(lap_cross) + " and the template TO_StartingGate collision (GL_StartGate_COL) blocks it at primitives " +
                    UxRaceNum.PyRepr(new List<long>(col.blocking.Keys)));
            // Untamed TO_StartingGate vs DropGate (reported, not used)
            var usg = rp.Get("TO_StartingGate") as JObj; JObj usg_off = null;
            if (usg != null)
            {
                var um = UxJson.Arr(usg.Get("matrix"));
                var Lr = UxRaceNum.MatMul4(Mat4(um), UxRaceNum.Inv4(Unscaled(G)));
                usg_off = new JObj().Set("local_xyz", L(UxNp.PyRound(Lr[12], 3), UxNp.PyRound(Lr[13], 3), UxNp.PyRound(Lr[14], 3))).Set("type", usg.Get("type"))
                    .Set("yaw_diff_deg", UxNp.PyRound(YawDeg(Mat4(um)) - YawDeg(G), 2));
            }
            var nodeJ = new List<object>(); foreach (var n in nodes) nodeJ.Add(n.j);
            var tnodeJ = new List<object>(); foreach (var n in template_nodes) tnodeJ.Add(n.j);
            long inCorr = 0, noTop = 0, tOk = 0, tCorr = 0;
            foreach (var n in nodes) { if (n.inCorr) inCorr++; if (n.hfOk && (n.clr2 == null || n.clr2.Value == 0 ? 1e9 : n.clr2.Value) >= NODE_CLEAR_FT) noTop++; }
            foreach (var n in template_nodes) { if (n.onDirt) tOk++; if (n.inCorr) tCorr++; }
            var tmats = new List<object>(); foreach (int m in track_mats) tmats.Add(m >= 0 ? tq.mats[m] : "collision-only");
            var chuteList = new List<object>(); foreach (int c in chutes) chuteList.Add((long)c);
            var gld = new List<object>(); foreach (var g_ in gate_line_detail) gld.Add(g_);
            var gtops = new List<object>();
            foreach (var e in ents) if (gate_top_ids.Contains(e.id)) gtops.Add(L(e.id, e.name, e.Asset("ModelAsset")));
            var gate = new JObj()
                .Set("untamed_dropgate", dg.Get("matrix")).Set("dot_gate_z_chute_dir", UxNp.PyRound(gate_dot, 3)).Set("flipped", gate_flipped)
                .Set("nearest_chute_node", haveNear ? new JObj().Set("section", (long)nearCi).Set("node", (long)nearK).Set("dist_ft", UxNp.PyRound(nearD, 2)) : null)
                .Set("chute_sections", chuteList).Set("distance_to_chute_centreline_ft", gate_line).Set("chute_line_detail", gld).Set("within_3ft", gate_line_ok)
                .Set("dropgate_y", UxNp.PyRound(drop_new[13], 3)).Set("hf_at_dropgate", hf.At(drop_new[12], drop_new[14]) is double hd ? (object)hd : null)
                .Set("startgate_matrix", F32List(sg_new)).Set("track_materials", tmats)
                .Set("gatestart_model_md5", tp.gate_model_md5).Set("gatestart_nodes", nodeJ).Set("gatestart_on_chute_dirt", n_ok).Set("gatestart_in_corridor", inCorr)
                .Set("untamed_startinggate_vs_dropgate", usg_off)
                .Set("lap_crosses_gatestart_line", lap_cross).Set("lap_centre_min_distance_to_gatestart_line_ft", lap_min)
                .Set("kept_collision_in_startgate_box", gate_box).Set("untamed_gate_top_entities", gtops)
                .Set("gatestart_on_chute_dirt_without_gate_top", noTop)
                .Set("template_gatestart_on_chute_dirt", tOk).Set("template_gatestart_in_corridor", tCorr)
                .Set("template_gatestart_nodes", node_patch != null ? tnodeJ : null)
                .Set("node_patch", node_patch).Set("startgate_collision", col.Json()).Set("startgate_collision_trim", col_trim)
                .Set("gate_patch_option", opt.gatePatch).Set("dropgate_materials", dgmat).Set("startgate_visual", hide ?? new JObj().Set("option", "keep"));
            // free-ride starts (WP7 places them; recomputed here for the report)
            JObj fr = null;
            try { fr = FreerideStarts(d, hf, cw, work, tq, lift); }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { warns.Add("free-ride starts not computed: " + ex.Message); }      // Python: except Exception
            bool lapG5 = g5 != null && Math.Abs(UxJson.Num(facts.Get("lap_G5")) - UxJson.Num(g5)) <= LAP_TOL_FT && Math.Abs(UxJson.Num(facts.Get("lap_centreline")) - UxJson.Num(g5)) <= LAP_TOL_FT;
            bool notFound = false; foreach (string p in probs) if (p.Contains("not found")) notFound = true;
            bool allOk = true; foreach (var n in nodes) if (!(n.onDirt && n.inCorr)) allOk = false;
            var checks = new JObj()
                .Set("tdf_walk_to_eof", UxJson.Arr(facts.Get("walk_problems")).Count == 0 && (bool)facts.Get("size_word_ok"))
                .Set("lappacing_engine_check", opt.lapPacing == "untamed" ? (object)(lp.Get("engine_check") as string == "PASS") : null)
                .Set("tdf_counts_match", (long)facts.Get("nodes_header") == (long)facts.Get("nodes_walked") && (long)facts.Get("sections_header") == (long)facts.Get("sections_walked"))
                .Set("lap_closed", (bool)facts.Get("lap_closed"))
                .Set("lap_equals_research_G5", lapG5)
                .Set("dropgate_at_untamed_pose", dropgate_pose_ok)
                .Set("dropgate_within_3ft_of_chute_centreline", gate_line_ok)
                .Set("dropgate_within_3ft_of_tdf_centre_nodes", gate_line_tdf_ok)
                .Set("all_template_moves_found", !notFound)
                .Set("sixteen_gatestart_nodes", nodes.Count == 16)
                .Set("gatestart_all_on_chute_dirt_in_chutes", allOk)
                .Set("startgate_collision_clear_of_lap", col_trim != null ? blocking_after.Count == 0 : col.blocking.Count == 0);
            var options = new JObj().Set("minimal", opt.minimal).Set("sx_flags", opt.sxFlags == null ? null : (object)opt.sxFlags.Value).Set("board_flip", opt.boardFlip)
                .Set("gate_patch", opt.gatePatch).Set("tdf_pad", opt.tdfPad).Set("lappacing", opt.lapPacing).Set("deck_respawn", opt.deckRespawn);
            if (bec != 0) options.Set("bale_edge_clear", bec);
            options.Set("dropgate_materials", opt.dropgateMaterials).Set("startgate_visual", opt.startgateVisual);
            var pl = new List<object>(); foreach (string p in probs) pl.Add(p);
            var wl = new List<object>(); foreach (string w in warns) wl.Add(w);
            result.race = new JObj().Set("schema", SCHEMA).Set("level", lv.disc).Set("short", lv.shortName).Set("mode", lv.mode).Set("slot", slot_name).Set("options", options)
                .Set("y_lift", lift)
                .Set("source", new JObj().Set("level_json", "level.json").Set("level_json_md5", UxBin.Md5(levelBytes)).Set("template", "stock-" + tp.level)
                    .Set("template_scene_md5", tp.scene_md5).Set("tool", TOOL).Set("tool_md5", TOOL_MD5).Set("tdf07_to_reflex_md5", TDF07_TOOL_MD5).Set("hf", hf.source))
                .Set("tdf", tdf).Set("lappacing", lp).Set("moves", moves).Set("unmoved_template_anchors_cameras", unmoved).Set("template_name_dupes", tp.dupes)
                .Set("gate", gate).Set("board", board).Set("win_finish", win).Set("freeride", fr).Set("checks", checks).Set("problems", pl).Set("warnings", wl);
            result.tdf = tdfOut; result.lapPacing = lp_bytes;
            return result;
        }

        // ================================================================================================== start-gate node patch
        /// untamed_gate_nodes(lv, d, lift): the lifted world matrices of the GateStart nodes of the Untamed TO_StartingGate model.
        static List<double[]> UntamedGateNodes(UxDiscLevel disc, JObj d, List<LevelEntity> ents, double lift, out string why, CancellationToken cancel)
        {
            var rp = UxJson.Get(d, "race_poses", "TO_StartingGate") as JObj;
            if (rp == null) { why = "no Untamed TO_StartingGate pose in level.json"; return null; }
            long id = UxJson.Int(rp.Get("id")); LevelEntity e = null;
            foreach (var x in ents) if (x.id == id) { e = x; break; }
            string ma = e?.Asset("ModelAsset");
            if (string.IsNullOrEmpty(ma)) { why = "Untamed TO_StartingGate has no ModelAsset"; return null; }
            string file = "model/" + ma + ".model"; string fname = ma + ".model";
            if (!disc.HasExtractFile(file))
            {
                var a = disc.TryGet(ma, "model");
                if (a == null || a.file == null) { why = "Untamed gate model " + ma + " not in extract/"; return null; }
                file = a.file; fname = a.file.Substring(a.file.Replace('\\', '/').LastIndexOf('/') + 1);
            }
            byte[] data = disc.ReadExtractFile(file, cancel);
            UxBin.Require(data.Length >= 0xAC, "Untamed gate model truncated");
            uint magic = UxBin.U32BE(data, 0); int n = (int)UxBin.U32BE(data, 12);
            if (magic != 0x88AABB00 || n < 2) { why = "Untamed gate model " + ma + " is not multi-node (" + magic.ToString("x8") + ", " + n + " nodes)"; return null; }
            GateTable(data, true, out _, out _, out int table);
            UxBin.Require(table >= 0 && 0xAC + 64L * n <= data.Length, "Untamed gate model: no node name table");
            var names = NodeNames(data, table, n);
            var M = Mat4(UxJson.Arr(rp.Get("matrix")));
            var o = new List<double[]>();
            for (int i = 0; i < n; i++)
            {
                if (!Regex.IsMatch(names[i], @"\AGateStart_\d+$")) continue;
                var Lm = new double[16]; for (int k = 0; k < 16; k++) Lm[k] = UxBin.F32BE(data, 0xAC + 64 * i + 4 * k);
                var W = UxRaceNum.MatMul4(Lm, M);
                W[13] += lift;
                o.Add(W);
            }
            why = ma + " (" + fname + ", " + o.Count + " GateStart nodes)";
            return o;
        }

        /// _cross_lateral(poly, o, ax): the lateral coordinate t where a polyline first crosses the line o + t * ax.
        static double? CrossLateral(List<double[]> P, double o0, double o1, double ax0, double ax1)
        {
            for (int i = 0; i + 1 < P.Count; i++)
            {
                var a = P[i]; var b = P[i + 1];
                double m01 = -(b[0] - a[0]), m11 = -(b[1] - a[1]);
                if (Math.Abs(UxRaceNum.Det2(ax0, m01, ax1, m11)) < 1e-9) continue;
                var tu = UxRaceNum.Solve(new[] { ax0, m01, ax1, m11 }, new[] { a[0] - o0, a[1] - o1 }, 2);
                if (-1e-6 <= tu[1] && tu[1] <= 1 + 1e-6) return tu[0];
            }
            if (P.Count > 1)
            {
                var a = P[0]; var b = P[1];
                double m01 = -(b[0] - a[0]), m11 = -(b[1] - a[1]);
                if (Math.Abs(UxRaceNum.Det2(ax0, m01, ax1, m11)) > 1e-9) return UxRaceNum.Solve(new[] { ax0, m01, ax1, m11 }, new[] { a[0] - o0, a[1] - o1 }, 2)[0];
            }
            return null;
        }

        /// patch_gate_nodes: GateStart positions for a start gate whose template nodes miss the chutes: the Untamed authored lanes split
        /// over the chutes they stand in, 16 slots shared in proportion, each chute taking the consecutive run that passes the node
        /// check nearest its centre (fallback: a stock-spaced row on the chute centre).
        static JObj PatchGateNodes(UxDiscLevel disc, JObj d, List<LevelEntity> ents, double lift, UxRaceTemplate tp, double[] sg_new, List<GateNode> tnodes,
            Func<string, double[], NodeRec> node_check, List<Chute> chute_lines, out List<NodeRec> nodesOut, out byte[] modelOut, CancellationToken cancel)
        {
            nodesOut = null; modelOut = null;
            var problems = new List<object>();
            var res = new JObj().Set("problems", problems);
            var un = UntamedGateNodes(disc, d, ents, lift, out string why, cancel);
            res.Set("untamed_gate", why);
            if (un == null) { problems.Add(why); return res; }
            var inv = UxRaceNum.Inv4(sg_new);
            var lanes = new List<double>(); foreach (var W in un) lanes.Add(UxRaceNum.VecMat4(W, 12, inv, 0));
            lanes.Sort();
            var lr = new List<object>(); foreach (double x in lanes) lr.Add(UxNp.PyRound(x, 3));
            res.Set("untamed_lanes_local_x", lr);
            var L0 = tnodes[0].L;
            foreach (var t in tnodes)
            {
                bool same = Math.Abs(t.L[13] - L0[13]) <= 1e-6 && Math.Abs(t.L[14] - L0[14]) <= 1e-6;
                for (int i = 0; i < 3 && same; i++) for (int j = 0; j < 3; j++) if (!UxRaceNum.AllClose(t.L[4 * i + j], L0[4 * i + j])) same = false;
                if (!same) { problems.Add("template GateStart nodes differ in rotation / y / z"); return res; }
            }
            double[] At(double x) { var Lm = (double[])L0.Clone(); Lm[12] = (double)(float)x; return Lm; }
            double o0 = sg_new[12], o1 = sg_new[14]; var axf = FlatDir(sg_new[0], sg_new[2]);
            var chutes = new List<(int section, double centre)>();
            foreach (var cl in chute_lines)
            {
                var m = CrossLateral(cl.m, o0, o1, axf[0], axf[2]);
                if (m == null) continue;
                chutes.Add((cl.ci, m.Value));
            }
            var ccl = new SortedDictionary<long, object>(); foreach (var c in chutes) ccl[c.section] = UxNp.PyRound(c.centre, 2);
            res.Set("chute_centres_local_x", ccl);
            if (chutes.Count == 0) { problems.Add("no start chute crosses the start-gate line"); return res; }
            var assign = new List<KeyValuePair<int, List<double>>>();
            foreach (var c in chutes) if (assign.FindIndex(a => a.Key == c.section) < 0) assign.Add(new KeyValuePair<int, List<double>>(c.section, new List<double>()));
            List<double> Assigned(int section) => assign[assign.FindIndex(a => a.Key == section)].Value;
            foreach (double x in lanes)
            {
                var best = chutes[0]; double bd = Math.Abs(best.centre - x);
                foreach (var c in chutes) { double dd = Math.Abs(c.centre - x); if (dd < bd) { best = c; bd = dd; } }
                Assigned(best.section).Add(x);
            }
            long tot = 0; foreach (var a in assign) tot += a.Value.Count;
            UxBin.Require(tot > 0, "start-gate node patch: no Untamed lane");
            var alloc = new Dictionary<int, long>(); long allocSum = 0;
            foreach (var a in assign) { alloc[a.Key] = 16L * a.Value.Count / tot; allocSum += alloc[a.Key]; }
            var rem = new List<int>(); foreach (var a in assign) rem.Add(a.Key);
            UxNp.StableSort(rem, (p, q) =>
            {
                long kp = -((16L * Assigned(p).Count) % tot), kq = -((16L * Assigned(q).Count) % tot);
                return kp != kq ? kp.CompareTo(kq) : p.CompareTo(q);
            });
            for (int i = 0; i < Math.Min(16 - allocSum, rem.Count); i++) alloc[rem[i]] += 1;
            var lpc = new SortedDictionary<long, object>(); var spc = new SortedDictionary<long, object>();
            foreach (var a in assign) { lpc[a.Key] = (long)a.Value.Count; spc[a.Key] = alloc[a.Key]; }
            res.Set("lanes_per_chute", lpc).Set("slots_per_chute", spc);
            var final = new List<double>(); var picks = new SortedDictionary<long, object>();
            long Fails(List<double> xs) { long f = 0; foreach (double x in xs) { var r = node_check("t", At(x)); if (!(r.onDirt && r.inCorr)) f++; } return f; }
            foreach (var c in chutes)
            {
                int k = (int)alloc[c.section];
                if (k == 0) continue;
                var cand = new List<double>(Assigned(c.section)); cand.Sort();
                long bestFails = 0; double bestDist = 0; int bestI = 0; List<double> bestXs = null; string bestHow = null;
                void Offer(long fails, double dist, int i, List<double> xs, string how)
                {
                    if (bestXs == null || fails < bestFails || fails == bestFails && (dist < bestDist || dist == bestDist && i < bestI)) { bestFails = fails; bestDist = dist; bestI = i; bestXs = xs; bestHow = how; }
                }
                for (int i = 0; i < cand.Count - k + 1; i++)
                {
                    var xs = cand.GetRange(i, k);
                    double mean = UxNp.Sum(xs.ToArray()) / k;
                    Offer(Fails(xs), Math.Abs(mean - c.centre), i, xs, "Untamed lanes " + (i + 1) + ".." + (i + k));
                }
                var row = new List<double>(); for (int j = 0; j < k; j++) row.Add(c.centre + LANE_SPACING_FT * (j - (k - 1) / 2.0));
                Offer(Fails(row), 0.0, 99, row, "stock-spaced row (" + UxRaceNum.PyFixed(LANE_SPACING_FT, 1) + " ft) on the chute centre");
                var lx = new List<object>(); foreach (double x in bestXs) lx.Add(UxNp.PyRound(x, 3));
                picks[c.section] = new JObj().Set("chute_centre_local_x", UxNp.PyRound(c.centre, 2)).Set("slots", (long)k).Set("choice", bestHow).Set("failing", bestFails).Set("local_x", lx);
                final.AddRange(bestXs);
            }
            res.Set("picks", picks);
            final.Sort();
            if (final.Count != 16) { problems.Add(final.Count + " lanes placed, not 16"); return res; }
            var tnames = new List<string>(); foreach (var t in tnodes) tnames.Add(t.name);
            tnames.Sort(string.CompareOrdinal);
            bool namesOk = tnames.Count == 16;
            for (int i = 0; namesOk && i < 16; i++) namesOk = tnames[i] == "GateStart_" + (i + 1).ToString("00", CultureInfo.InvariantCulture);
            if (!namesOk) { problems.Add("template node names " + UxRaceNum.PyRepr(tnames)); return res; }
            var new_x = new List<KeyValuePair<string, double>>();
            for (int i = 0; i < 16; i++) new_x.Add(new KeyValuePair<string, double>("GateStart_" + (i + 1).ToString("00", CultureInfo.InvariantCulture), (double)(float)final[i]));
            var model = PatchGateModel(tp.gate_model, new_x);
            var nodes = new List<NodeRec>(); foreach (var g in GateNodes(model)) nodes.Add(node_check(g.name, g.L));
            var bad = new List<object>(); long onDirt = 0, inCorr = 0;
            foreach (var n in nodes) { if (!(n.onDirt && n.inCorr)) bad.Add(n.name); if (n.onDirt) onDirt++; if (n.inCorr) inCorr++; }
            if (bad.Count > 0) problems.Add("patched nodes off chute dirt / outside the chutes: " + UxRaceNum.PyRepr(bad));
            double minGap = double.PositiveInfinity; for (int i = 0; i + 1 < final.Count; i++) minGap = Math.Min(minGap, final[i + 1] - final[i]);
            var fl = new List<object>(); foreach (double x in final) fl.Add(UxNp.PyRound(x, 3));
            res.Set("asset", GATE_MODEL_NAME).Set("local_x", fl).Set("md5", UxBin.Md5(model)).Set("min_gap_ft", UxNp.PyRound(minGap, 3)).Set("on_chute_dirt", onDirt).Set("in_corridor", inCorr);
            nodesOut = nodes; modelOut = model;
            return res;
        }

        // ================================================================================================== free-ride starts
        /// Builder.freeride_starts (fr_clearance 'strict') reduced to what the race report records: the chosen layout, the layouts
        /// tried and the 12 starts.                                                                                                  same as UxEntities.Builder.freeride_starts
        static JObj FreerideStarts(JObj d, HeightField hf, CollisionWorld cw, UxWork work, TerrainQuery tq, double lift)
        {
            const double clearance = 2.0, FR_POINT_CLEAR_FT = 3.0;
            if (d.Get("kind") as string != "sx") throw new UxException("level.json is kind " + UxRaceNum.PyRepr(d.Get("kind")) + ", expected 'sx'");
            var fs = d.Get("freeride_start") as JObj;
            var starts = new List<object>();
            if (fs == null || fs.items.Count == 0) throw new UxException("'Builder' object has no attribute 'fr_layout'");      // the Python path: [] and no layout
            var M = Unscaled(Mat4(UxJson.Arr(fs.Get("matrix"))));
            double x0 = M[0], x2 = M[2];
            if (UxRaceNum.Norm3(x0, 0.0, x2) < 1e-6) throw new UxException("'Builder' object has no attribute 'fr_layout'");
            double xn = UxRaceNum.Norm3(x0, 0.0, x2);
            x0 /= xn; x2 /= xn;
            var pos = UxJson.Arr(fs.Get("position"));
            double[] p0 = { (double)(float)UxJson.Num(pos[0]), (double)(float)UxJson.Num(pos[1]), (double)(float)UxJson.Num(pos[2]) };
            // the terrain parts for the strict point clearance
            var W = TerrainPartTris(work, tq, lift, out _) ?? new double[0];
            double[] wlo, whi; MinMax(W, out wlo, out whi);
            double PointClearance(double px, double py, double pz, out object who)
            {
                double d0 = cw.Nearest(px, py, pz, 30.0, out long? w0); who = w0 == null ? null : (object)w0.Value;
                double best = double.PositiveInfinity; bool any = false;
                for (int i = 0; i < W.Length / 9; i++)
                {
                    if (!(wlo[3 * i] <= px + 30.0 && wlo[3 * i + 1] <= py + 30.0 && wlo[3 * i + 2] <= pz + 30.0 && whi[3 * i] >= px - 30.0 && whi[3 * i + 1] >= py - 30.0 && whi[3 * i + 2] >= pz - 30.0)) continue;
                    double dd = PointTriDist(px, py, pz, W, 9 * i);
                    if (!any || dd < best) { best = dd; any = true; }
                }
                if (any && best < d0) { d0 = best; who = "terrain part"; }
                return d0;
            }
            var layouts = new List<(string name, double[] offs)>();
            { var a = new double[12]; var b = new double[12]; var c = new double[12];
              for (int k = 1; k <= 12; k++) { a[k - 1] = -(k - 1) * 3.0; b[k - 1] = (k - 1) * 3.0; c[k - 1] = -(k - 6.5) * 3.0; }
              layouts.Add(("-x", a)); layouts.Add(("+x", b)); layouts.Add(("centred", c)); }
            var tried = new List<object>(); var placed = new Dictionary<string, List<(int index, double x, double y, double z, double clr)>>();
            string name = null;
            foreach (var (nm, offs) in layouts)
            {
                var rows = new List<(int, double, double, double, double)>(); var bad = new List<object>();
                for (int k = 1; k <= 12; k++)
                {
                    double o = offs[k - 1];
                    double px = p0[0] + x0 * o, pz = p0[2] + x2 * o;
                    double g = hf.At(px, pz) ?? p0[1] + lift;
                    double c = cw.Clearance(px, g, pz, out long? who);
                    double pc = PointClearance(px, g + 1.0, pz, out object pwho);
                    rows.Add((k, px, g + 1.0, pz, c));
                    if (c < clearance || pc < FR_POINT_CLEAR_FT)
                        bad.Add(L((long)k, UxNp.PyRound(c, 2), who == null ? null : (object)who.Value, UxNp.PyRound(pc, 2), pwho));
                }
                placed[nm] = rows;
                tried.Add(new JObj().Set("layout", nm).Set("blocked", bad));
                if (bad.Count == 0) { name = nm; break; }
            }
            if (name == null) name = "-x";                                 // every layout blocked: the design layout
            foreach (var (index, x, y, z, clr) in placed[name])
            {
                var m = (double[])M.Clone(); m[12] = x; m[13] = y; m[14] = z;
                starts.Add(new JObj().Set("index", (long)index)
                    .Set("position", L(UxNp.PyRound((double)(float)m[12], 3), UxNp.PyRound((double)(float)m[13], 3), UxNp.PyRound((double)(float)m[14], 3)))
                    .Set("clearance_ft", !UxNp.Finite(clr) ? null : (object)UxNp.PyRound(clr, 2)));
            }
            return new JObj().Set("layout", new JObj().Set("chosen", name).Set("tried", tried).Set("hf_source", hf.source)).Set("starts", starts);
        }
    }
}
