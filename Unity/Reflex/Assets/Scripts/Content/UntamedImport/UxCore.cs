// MX vs ATV Untamed converter, foundation (core/plan/UNTAMED_CONVERT.md): byte access in both byte orders, hashes, and the numpy number
// rules the Python reference converter relies on: float16 conversion, float32 rounding, round-half-even.
// Pure C# (System only): compiled into the client and, outside Unity, into the parity tests (tests/untamed_convert).
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace Reflex.Port.Content.UntamedImport
{
    /// Any failure of the Untamed converter (Python: ValueError / AssertionError / SystemExit of the tools).
    public sealed class UxException : Exception
    {
        public UxException(string message) : base(message) { }
        public UxException(string message, Exception inner) : base(message, inner) { }
    }

    /// struct.unpack_from / pack_into for the fixed layouts ('>' = big endian: Xbox 360 data, '<' = little: Reflex PC).
    public static class UxBin
    {
        public static void Require(bool ok, string message) { if (!ok) throw new UxException(message); }

        public static ushort U16(byte[] b, int p, bool big) => big ? (ushort)(b[p] << 8 | b[p + 1]) : (ushort)(b[p] | b[p + 1] << 8);
        public static short I16(byte[] b, int p, bool big) => (short)U16(b, p, big);
        public static uint U32(byte[] b, int p, bool big)
            => big ? (uint)(b[p] << 24 | b[p + 1] << 16 | b[p + 2] << 8 | b[p + 3]) : (uint)(b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24);
        public static int I32(byte[] b, int p, bool big) => (int)U32(b, p, big);
        public static float F32(byte[] b, int p, bool big) => BitConverter.Int32BitsToSingle((int)U32(b, p, big));
        public static ulong U64(byte[] b, int p, bool big)
            => big ? (ulong)U32(b, p, true) << 32 | U32(b, p + 4, true) : (ulong)U32(b, p + 4, false) << 32 | U32(b, p, false);

        public static uint U32LE(byte[] b, int p) => U32(b, p, false);
        public static uint U32BE(byte[] b, int p) => U32(b, p, true);
        public static ushort U16LE(byte[] b, int p) => U16(b, p, false);
        public static ushort U16BE(byte[] b, int p) => U16(b, p, true);
        public static int I32LE(byte[] b, int p) => (int)U32(b, p, false);
        public static float F32LE(byte[] b, int p) => F32(b, p, false);
        public static float F32BE(byte[] b, int p) => F32(b, p, true);

        public static uint[] U32s(byte[] b, int p, int n, bool big) { var r = new uint[n]; for (int i = 0; i < n; i++) r[i] = U32(b, p + 4 * i, big); return r; }
        public static float[] F32s(byte[] b, int p, int n, bool big) { var r = new float[n]; for (int i = 0; i < n; i++) r[i] = F32(b, p + 4 * i, big); return r; }

        public static void PutU16(byte[] b, int p, ushort v, bool big = false)
        {
            if (big) { b[p] = (byte)(v >> 8); b[p + 1] = (byte)v; } else { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); }
        }
        public static void PutU32(byte[] b, int p, uint v, bool big = false)
        {
            if (big) { b[p] = (byte)(v >> 24); b[p + 1] = (byte)(v >> 16); b[p + 2] = (byte)(v >> 8); b[p + 3] = (byte)v; }
            else { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); b[p + 2] = (byte)(v >> 16); b[p + 3] = (byte)(v >> 24); }
        }
        public static void PutI32(byte[] b, int p, int v, bool big = false) => PutU32(b, p, (uint)v, big);
        public static void PutF32(byte[] b, int p, float v, bool big = false) => PutU32(b, p, (uint)BitConverter.SingleToInt32Bits(v), big);

        public static byte[] Slice(byte[] b, int start, int length)
        {
            Require(start >= 0 && length >= 0 && start + length <= b.Length, "Range outside data");
            var r = new byte[length]; Buffer.BlockCopy(b, start, r, 0, length); return r;
        }
        /// data[start:end] with Python's clamping (a slice past the end is shorter, never an error).
        public static byte[] PySlice(byte[] b, int start, int end)
        {
            if (start < 0) start = 0; if (end > b.Length) end = b.Length; if (end < start) end = start;
            var r = new byte[end - start]; Buffer.BlockCopy(b, start, r, 0, end - start); return r;
        }
        public static byte[] Concat(params byte[][] parts)
        {
            int n = 0; foreach (var p in parts) n += p.Length;
            var r = new byte[n]; int at = 0;
            foreach (var p in parts) { Buffer.BlockCopy(p, 0, r, at, p.Length); at += p.Length; }
            return r;
        }
        public static bool Same(byte[] a, byte[] b)
        {
            if (a == null || b == null) return a == b;
            if (a.Length != b.Length) return false;
            for (int i = 0; i < a.Length; i++) if (a[i] != b[i]) return false;
            return true;
        }
        public static bool Same(byte[] a, int ap, byte[] b, int bp, int n)
        {
            if (ap < 0 || bp < 0 || ap + n > a.Length || bp + n > b.Length) return false;
            for (int i = 0; i < n; i++) if (a[ap + i] != b[bp + i]) return false;
            return true;
        }

        /// data[p:p+max].split(b'\0')[0].decode('latin-1'): the bytes up to the first NUL (or the end of the range).
        public static string CStr(byte[] b, int p, int max = int.MaxValue)
        {
            int end = (int)Math.Min((long)b.Length, (long)p + max), e = p;
            while (e < end && b[e] != 0) e++;
            return Latin1(b, p, e - p);
        }
        public static string Latin1(byte[] b, int p, int n)
        {
            var c = new char[n]; for (int i = 0; i < n; i++) c[i] = (char)b[p + i];
            return new string(c);
        }
        public static byte[] Latin1Bytes(string s)
        {
            var r = new byte[s.Length];
            for (int i = 0; i < s.Length; i++) { Require(s[i] < 256, "not a latin-1 string: " + s); r[i] = (byte)s[i]; }
            return r;
        }
        public static byte[] FromHex(string hex)
        {
            Require(hex.Length % 2 == 0, "hex string length");
            var r = new byte[hex.Length / 2];
            for (int i = 0; i < r.Length; i++) r[i] = (byte)(Nib(hex[2 * i]) << 4 | Nib(hex[2 * i + 1]));
            return r;
        }
        static int Nib(char c) => c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : throw new UxException("hex digit");
        public static string Hex(byte[] b)
        {
            var sb = new StringBuilder(b.Length * 2);
            foreach (byte x in b) { sb.Append("0123456789abcdef"[x >> 4]); sb.Append("0123456789abcdef"[x & 15]); }
            return sb.ToString();
        }

        // ---- ux_common: md5_bytes / md5_file / sha256_file
        public static string Md5(byte[] b) { using (var h = MD5.Create()) return Hex(h.ComputeHash(b)); }
        public static string Md5(byte[] b, int start, int length) { using (var h = MD5.Create()) return Hex(h.ComputeHash(b, start, length)); }
        public static string Sha256(byte[] b) { using (var h = SHA256.Create()) return Hex(h.ComputeHash(b)); }
        public static string Md5File(string path) { using (var h = MD5.Create()) using (var f = File.OpenRead(path)) return Hex(h.ComputeHash(f)); }
        public static string Sha256File(string path) { using (var h = SHA256.Create()) using (var f = File.OpenRead(path)) return Hex(h.ComputeHash(f)); }

        static uint[] s_crc;
        /// zlib.crc32.
        public static uint Crc32(byte[] b, int start, int length)
        {
            var t = s_crc;
            if (t == null)
            {
                t = new uint[256];
                for (uint i = 0; i < 256; i++) { uint c = i; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; }
                s_crc = t;
            }
            uint r = 0xFFFFFFFFu;
            for (int i = 0; i < length; i++) r = t[(r ^ b[start + i]) & 0xFF] ^ (r >> 8);
            return r ^ 0xFFFFFFFFu;
        }
    }

    /// A growing byte buffer with struct.pack-style appends (bytearray += struct.pack(...)).
    public sealed class UxWriter
    {
        byte[] _b; int _n;
        public UxWriter(int capacity = 256) { _b = new byte[Math.Max(16, capacity)]; }
        public int Length => _n;
        void Need(int more)
        {
            if (_n + more <= _b.Length) return;
            int cap = _b.Length; while (cap < _n + more) cap = cap < (1 << 30) ? cap * 2 : checked(_n + more);
            Array.Resize(ref _b, cap);
        }
        public UxWriter Bytes(byte[] v) { Need(v.Length); Buffer.BlockCopy(v, 0, _b, _n, v.Length); _n += v.Length; return this; }
        public UxWriter Bytes(byte[] v, int start, int length) { Need(length); Buffer.BlockCopy(v, start, _b, _n, length); _n += length; return this; }
        public UxWriter Zeros(int n) { Need(n); Array.Clear(_b, _n, n); _n += n; return this; }
        public UxWriter Fill(byte v, int n) { Need(n); for (int i = 0; i < n; i++) _b[_n + i] = v; _n += n; return this; }
        public UxWriter U8(byte v) { Need(1); _b[_n++] = v; return this; }
        public UxWriter U16(ushort v, bool big = false) { Need(2); UxBin.PutU16(_b, _n, v, big); _n += 2; return this; }
        public UxWriter U32(uint v, bool big = false) { Need(4); UxBin.PutU32(_b, _n, v, big); _n += 4; return this; }
        public UxWriter I32(int v, bool big = false) => U32((uint)v, big);
        public UxWriter F32(float v, bool big = false) => U32((uint)BitConverter.SingleToInt32Bits(v), big);
        public UxWriter U32s(params uint[] v) { foreach (uint x in v) U32(x); return this; }
        public UxWriter F32s(params float[] v) { foreach (float x in v) F32(x); return this; }
        /// struct.pack_into('<I', buffer, at, v) on bytes already written.
        public void PatchU32(int at, uint v) { UxBin.Require(at >= 0 && at + 4 <= _n, "patch outside the buffer"); UxBin.PutU32(_b, at, v); }
        public byte[] ToArray() { var r = new byte[_n]; Buffer.BlockCopy(_b, 0, r, 0, _n); return r; }
    }

    /// numpy's number rules, bit for bit (Python floats are C# double; np.float32 is C# float; '<f2' is a ushort of half bits).
    public static class UxNp
    {
        /// np.nan as float64 / float32: the positive quiet NaN (0x7FF8000000000000 / 0x7FC00000). .NET's double.NaN and float.NaN have the
        /// sign bit set: wherever a NaN reaches written bytes (float grids, '<f2' / '<f4' arrays), use these.
        public static readonly double NaN = BitConverter.Int64BitsToDouble(0x7FF8000000000000L);
        public static readonly float NaNf = BitConverter.Int32BitsToSingle(0x7FC00000);

        /// ux_common.f32: a Python float rounded to float32 and back (what BXML stores).
        public static double F32(double x) => (double)(float)x;
        public static bool Finite(double x) => !double.IsNaN(x) && !double.IsInfinity(x);
        public static bool Finite(params double[] xs) { foreach (double x in xs) if (!Finite(x)) return false; return true; }

        /// np.float16 bits -> float32 (exact).
        public static float HalfToFloat(ushort h)
        {
            int s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
            uint bits;
            if (e == 0)
            {
                if (m == 0) bits = 0;
                else
                {   // subnormal: normalise
                    int sh = 0; while ((m & 0x400) == 0) { m <<= 1; sh++; }
                    bits = (uint)((127 - 15 - sh + 1) << 23) | (uint)((m & 0x3FF) << 13);
                }
            }
            else if (e == 31) bits = 0x7F800000u | (uint)(m << 13);
            else bits = (uint)((e - 15 + 127) << 23) | (uint)(m << 13);
            return BitConverter.Int32BitsToSingle((int)(bits | (uint)s << 31));
        }

        /// astype('<f2') of a float64 (npy_double_to_half): round to nearest, ties to even; overflow gives inf; NaN stays NaN.
        public static ushort DoubleToHalf(double v)
        {
            long bits = BitConverter.DoubleToInt64Bits(v);
            ushort sign = (ushort)((bits >> 48) & 0x8000);
            int e = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            if (e == 0x7FF)
            {
                if (frac == 0) return (ushort)(sign | 0x7C00);
                ushort r = (ushort)(0x7C00 + (frac >> 42)); if (r == 0x7C00) r++;
                return (ushort)(sign | r);
            }
            if (e == 0 && frac == 0) return sign;
            // value = m * 2^x with m a 53-bit (or subnormal) integer
            long m = e == 0 ? frac : frac | (1L << 52); int x = (e == 0 ? 1 : e) - 1075;
            // normalise so that m has its top bit at bit 52
            while ((m & (1L << 52)) == 0) { m <<= 1; x--; }
            int unbiased = x + 52;                 // v = 1.xxx * 2^unbiased
            if (unbiased > 15) return (ushort)(sign | 0x7C00);
            int he, shift;                         // half = (m >> shift) rounded, with exponent field he
            if (unbiased >= -14) { he = unbiased + 15; shift = 42; }     // normal: keep 11 bits (1 + 10)
            else { he = 0; shift = 42 + (-14 - unbiased); }             // subnormal: fewer bits
            long q; bool half, sticky;
            if (shift >= 64) { q = 0; half = false; sticky = false; }   // far below half of the smallest subnormal
            else
            {
                q = m >> shift;
                long rem = m & ((1L << shift) - 1), mid = 1L << (shift - 1);
                half = rem == mid; sticky = rem > mid;
                if (rem < mid) { half = false; sticky = false; }
            }
            if (sticky || half && (q & 1) != 0) q++;
            // q carries the implicit bit for normals (bit 10); adding the exponent field makes a rounding carry move up by itself
            int result = he == 0 ? (int)q : ((he - 1) << 10) + (int)q;
            if (result >= 0x7C00) result = 0x7C00;
            return (ushort)(sign | result);
        }
        /// astype('<f2') of a float32 (npy_float_to_half): the same rounding (float -> double is exact).
        public static ushort FloatToHalf(float v)
        {
            if (float.IsNaN(v))
            {
                uint f = (uint)BitConverter.SingleToInt32Bits(v);
                ushort r = (ushort)(0x7C00 + ((f & 0x007FFFFF) >> 13)); if (r == 0x7C00) r++;
                return (ushort)((f >> 16) & 0x8000 | r);
            }
            return DoubleToHalf(v);
        }

        /// np.round / np.rint / Python round(x): to the nearest integer, ties to even.
        public static double Round(double x) => Math.Round(x, MidpointRounding.ToEven);
        /// np.round(x, decimals) for a float64: round(x * 10^d) / 10^d (numpy's own formula, also for negative d).
        public static double Round(double x, int decimals)
        {
            if (decimals == 0) return Round(x);
            if (double.IsNaN(x) || double.IsInfinity(x)) return x;
            double p = Math.Pow(10, Math.Abs(decimals));   // exact for |decimals| <= 22
            return decimals > 0 ? Round(x * p) / p : Round(x / p) * p;
        }
        /// Python's built-in round(x, ndigits) for a float: correctly rounded decimal (round-half-even on the exact binary value).
        public static double PyRound(double x, int ndigits)
        {
            if (double.IsNaN(x) || double.IsInfinity(x) || x == 0) return x;
            if (ndigits > 22 + 308) return x;
            // exact: the decimal expansion of x through System.Decimal is not exact; use BigInteger arithmetic
            long bits = BitConverter.DoubleToInt64Bits(x);
            bool neg = bits < 0; int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;     // |x| = m * 2^e
            System.Numerics.BigInteger num = m, den = System.Numerics.BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
            if (ndigits >= 0) num *= System.Numerics.BigInteger.Pow(10, ndigits); else den *= System.Numerics.BigInteger.Pow(10, -ndigits);
            var q = System.Numerics.BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            // q * 10^-ndigits, as the nearest double
            string text = (neg ? "-" : "") + q.ToString(System.Globalization.CultureInfo.InvariantCulture) + "e" + (-ndigits).ToString(System.Globalization.CultureInfo.InvariantCulture);
            return UxJson.ParseDouble(text);
        }

        // ---- BLAS-backed numpy operations. On the reference machine the OpenBLAS kernels behind numpy use fused multiply-add, so `A @ B`,
        // np.dot and np.linalg.norm of a vector do not round like a*b + c*d (core/plan/UNTAMED_CONVERT.md, "numpy rules"). Fma is a
        // software fused multiply-add (Math.FusedMultiplyAdd is not in .NET Standard 2.1): exact a*b + c, rounded once.

        static void TwoSum(double a, double b, out double s, out double e)
        {
            s = a + b; double bb = s - a; e = (a - (s - bb)) + (b - bb);
        }
        static void Split(double a, out double hi, out double lo) { double t = 134217729.0 * a; hi = t - (t - a); lo = a - hi; }
        /// x + y rounded to odd: an inexact sum gets an odd last bit (Boldo and Melquiond 2008), so a later rounding is not a double one.
        static double OddSum(double x, double y)
        {
            TwoSum(x, y, out double s, out double e);
            if (e == 0) return s;
            long bits = BitConverter.DoubleToInt64Bits(s);
            if ((bits & 1) != 0) return s;
            return BitConverter.Int64BitsToDouble((e > 0) == (s > 0) ? bits + 1 : bits - 1);
        }
        static bool FmaSafe(double v)
        {
            int e = (int)((BitConverter.DoubleToInt64Bits(v) >> 52) & 0x7FF);
            return e > 1023 - 480 && e < 1023 + 480;
        }
        /// fma(a, b, c) for float64: a*b + c with one rounding (to nearest, ties to even).
        public static double Fma(double a, double b, double c)
        {
            if (double.IsNaN(a) || double.IsNaN(b) || double.IsNaN(c) || double.IsInfinity(a) || double.IsInfinity(b) || double.IsInfinity(c)) return a * b + c;
            if (a == 0 || b == 0) return a * b + c;
            if (!FmaSafe(a) || !FmaSafe(b) || (c != 0 && !FmaSafe(c))) return FmaExact(a, b, c);
            double ph = a * b;
            if (c == 0) return ph;
            Split(a, out double ah, out double al); Split(b, out double bh, out double bl);
            double pl = ((ah * bh - ph) + ah * bl + al * bh) + al * bl;
            TwoSum(c, pl, out double th, out double tl);
            TwoSum(ph, th, out double vh, out double vl);
            return vh + OddSum(tl, vl);
        }
        static double FmaExact(double a, double b, double c)
        {
            void Parts(double v, out System.Numerics.BigInteger m, out int e)
            {
                long bits = BitConverter.DoubleToInt64Bits(v); int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
                long mm = be == 0 ? frac : frac | (1L << 52); e = be == 0 ? -1074 : be - 1075;
                m = bits < 0 ? -mm : mm;
            }
            Parts(a, out var ma, out int ea); Parts(b, out var mb, out int eb); Parts(c, out var mc, out int ec);
            var p = ma * mb; int ep = ea + eb;
            int lo = Math.Min(ep, ec);
            var sum = (p << (ep - lo)) + (mc << (ec - lo));
            if (sum.IsZero) return a * b + c;
            bool neg = sum.Sign < 0; if (neg) sum = -sum;
            double r = lo >= 0 ? UxJson.Ratio(sum << lo, System.Numerics.BigInteger.One) : UxJson.Ratio(sum, System.Numerics.BigInteger.One << -lo);
            return neg ? -r : r;
        }
        /// fma for float32 (the product is exact in float64; the sum is rounded to odd there, then to float32).
        public static float Fma(float a, float b, float c)
        {
            if (float.IsNaN(a) || float.IsNaN(b) || float.IsNaN(c) || float.IsInfinity(a) || float.IsInfinity(b) || float.IsInfinity(c)) return a * b + c;
            return (float)OddSum((double)a * b, c);
        }

        /// np.dot(a, b) / a @ b of two float64 vectors of n < 16 elements: the scalar loop of OpenBLAS ddot, s = a0*b0; s = fma(ak, bk, s).
        /// (16 elements and more go through a vector kernel with another summation order: not modelled.)
        public static double Dot(double[] a, int ao, int astep, double[] b, int bo, int bstep, int n)
        {
            if (n == 0) return 0;
            UxBin.Require(n < 16, "UxNp.Dot: " + n + " elements (the BLAS kernel for 16 and more is not modelled)");
            double s = a[ao] * b[bo];
            for (int k = 1; k < n; k++) s = Fma(a[ao + k * astep], b[bo + k * bstep], s);
            return s;
        }
        public static double Dot(double[] a, double[] b) { UxBin.Require(a.Length == b.Length, "dot: lengths"); return Dot(a, 0, 1, b, 0, 1, a.Length); }
        /// np.linalg.norm(v) of a float64 vector (n < 16): sqrt(dot(v, v)).
        public static double Norm(double[] v) => Math.Sqrt(Dot(v, v));
        /// A @ B for float64 matrices (m x k) @ (k x n), row-major (numpy: cblas_dgemm when both operands are 2-D or stacked matrices):
        /// each element is s = A[i,0]*B[0,j]; s = fma(A[i,t], B[t,j], s). Measured up to 30 x 30 x 30.
        public static double[] MatMul(double[] A, int m, int k, double[] B, int n)
        {
            UxBin.Require(A.Length == m * k && B.Length == k * n, "matmul: shapes");
            var C = new double[m * n];
            for (int i = 0; i < m; i++)
                for (int j = 0; j < n; j++)
                {
                    double s = k == 0 ? 0 : A[i * k] * B[j];
                    for (int t = 1; t < k; t++) s = Fma(A[i * k + t], B[t * n + j], s);
                    C[i * n + j] = s;
                }
            return C;
        }
        /// A @ B for float32 matrices (cblas_sgemm): the same chain in float32.
        public static float[] MatMul(float[] A, int m, int k, float[] B, int n)
        {
            UxBin.Require(A.Length == m * k && B.Length == k * n, "matmul: shapes");
            var C = new float[m * n];
            for (int i = 0; i < m; i++)
                for (int j = 0; j < n; j++)
                {
                    float s = k == 0 ? 0 : A[i * k] * B[j];
                    for (int t = 1; t < k; t++) s = Fma(A[i * k + t], B[t * n + j], s);
                    C[i * n + j] = s;
                }
            return C;
        }
        /// A @ x for a float64 (m x 3) matrix and a 3-vector (cblas_dgemv): with m >= 2 rows s = A[i,1]*x[1]; s = fma(A[i,0], x[0], s);
        /// s = fma(A[i,2], x[2], s); with one row the chain 0, 1, 2. Other widths are not modelled (4 columns go through a pairwise kernel).
        public static double[] MatVec3(double[] A, int m, double[] x)
        {
            UxBin.Require(A.Length == m * 3 && x.Length == 3, "matvec: shapes");
            var y = new double[m];
            for (int i = 0; i < m; i++)
                y[i] = m == 1 ? Fma(A[2], x[2], Fma(A[1], x[1], A[0] * x[0])) : Fma(A[i * 3 + 2], x[2], Fma(A[i * 3], x[0], A[i * 3 + 1] * x[1]));
            return y;
        }
        /// y @ A for a float64 vector and a square matrix (cblas_dgemv, transposed): 3 x 3: the chain 0, 1, 2; 4 x 4: the chain 1, 0, 2, 3.
        public static double[] VecMat(double[] y, double[] A, int n)
        {
            UxBin.Require((n == 3 || n == 4) && y.Length == n && A.Length == n * n, "vecmat: only 3 x 3 and 4 x 4 are modelled");
            var r = new double[n];
            for (int j = 0; j < n; j++)
            {
                double s;
                if (n == 3) s = Fma(y[2], A[6 + j], Fma(y[1], A[3 + j], y[0] * A[j]));
                else s = Fma(y[3], A[12 + j], Fma(y[2], A[8 + j], Fma(y[0], A[j], y[1] * A[4 + j])));
                r[j] = s;
            }
            return r;
        }

        /// np.sum / np.add.reduce along a contiguous axis (a 1-D array, or the last axis of a C-ordered array): the pairwise sum of numpy
        /// (fewer than 8 elements: left to right; up to 128: eight accumulators, combined in pairs, then the tail; above: halves).
        /// A reduction along another axis (axis=0 of a 2-D array) adds slice by slice, left to right: write that loop directly.
        public static double Sum(double[] a, int start, int n, int step = 1)
        {
            if (n < 8) { double res = 0; for (int i = 0; i < n; i++) res += a[start + i * step]; return res; }
            if (n <= 128)
            {
                double r0 = a[start], r1 = a[start + step], r2 = a[start + 2 * step], r3 = a[start + 3 * step], r4 = a[start + 4 * step], r5 = a[start + 5 * step], r6 = a[start + 6 * step], r7 = a[start + 7 * step];
                int i;
                for (i = 8; i < n - (n % 8); i += 8)
                {
                    int q = start + i * step;
                    r0 += a[q]; r1 += a[q + step]; r2 += a[q + 2 * step]; r3 += a[q + 3 * step]; r4 += a[q + 4 * step]; r5 += a[q + 5 * step]; r6 += a[q + 6 * step]; r7 += a[q + 7 * step];
                }
                double res = ((r0 + r1) + (r2 + r3)) + ((r4 + r5) + (r6 + r7));
                for (; i < n; i++) res += a[start + i * step];
                return res;
            }
            int n2 = n / 2; n2 -= n2 % 8;
            return Sum(a, start, n2, step) + Sum(a, start + n2 * step, n - n2, step);
        }
        public static double Sum(double[] a) => Sum(a, 0, a.Length);
        /// The same for a float32 array (the accumulators are float32).
        public static float Sum(float[] a, int start, int n, int step = 1)
        {
            if (n < 8) { float res = 0; for (int i = 0; i < n; i++) res = (float)(res + a[start + i * step]); return res; }
            if (n <= 128)
            {
                var r = new float[8]; for (int k = 0; k < 8; k++) r[k] = a[start + k * step];
                int i;
                for (i = 8; i < n - (n % 8); i += 8) for (int k = 0; k < 8; k++) r[k] = (float)(r[k] + a[start + (i + k) * step]);
                float res = (float)((float)((float)(r[0] + r[1]) + (float)(r[2] + r[3])) + (float)((float)(r[4] + r[5]) + (float)(r[6] + r[7])));
                for (; i < n; i++) res = (float)(res + a[start + i * step]);
                return res;
            }
            int n2 = n / 2; n2 -= n2 % 8;
            return (float)(Sum(a, start, n2, step) + Sum(a, start + n2 * step, n - n2, step));
        }
        public static float Sum(float[] a) => Sum(a, 0, a.Length);

        /// np.nan_to_num(v, nan=0, posinf=0, neginf=0).
        public static float NanToZero(float v) => float.IsNaN(v) || float.IsInfinity(v) ? 0f : v;
        /// np.clip.
        public static double Clip(double v, double lo, double hi) => v < lo ? lo : v > hi ? hi : v;

        /// astype(np.int64) / int() of a float64: truncation towards zero.
        public static long Trunc(double v) => (long)v;
        /// Python floor division and modulo for ints (the sign follows the divisor).
        public static long FloorDiv(long a, long b) { long q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }
        public static long Mod(long a, long b) { long r = a % b; if (r != 0 && ((r < 0) != (b < 0))) r += b; return r; }

        /// np.argsort(a) for a dtype without a specialised sort (structured dtypes: keys compared field by field), default kind (and
        /// kind='heapsort', which gives the same order there in the reference's numpy 2.5): numpy's generic introsort (npy_aquicksort:
        /// median of three, insertion sort for 16 elements and fewer, heapsort when the depth limit is hit). Not stable: equal elements
        /// come out in THIS algorithm's order, which is what code that takes "the first of the equal ones" after an argsort
        /// (uxterrain.collision_materials) depends on. cmp(i, j) compares elements i and j. (Plain numeric dtypes use other, SIMD sorts
        /// in numpy 2: for those only kind='stable' is reproducible; see StableArgsort.)
        public static int[] ArgQuickSort(int num, Comparison<int> cmp)
        {
            var tosort = new int[num]; for (int i = 0; i < num; i++) tosort[i] = i;
            if (num < 2) return tosort;
            const int Small = 15;
            int pl = 0, pr = num - 1;
            var stack = new int[128 * 2]; int sptr = 0; var depth = new int[128]; int psdepth = 0;
            int msb = 0; { uint u = (uint)num; while ((u >>= 1) != 0) msb++; }
            int cdepth = msb * 2;
            while (true)
            {
                if (cdepth < 0) ArgHeapSort(tosort, pl, pr - pl + 1, cmp);
                else
                {
                    while (pr - pl > Small)
                    {
                        int pm = pl + ((pr - pl) >> 1), t;
                        if (cmp(tosort[pm], tosort[pl]) < 0) { t = tosort[pm]; tosort[pm] = tosort[pl]; tosort[pl] = t; }
                        if (cmp(tosort[pr], tosort[pm]) < 0) { t = tosort[pr]; tosort[pr] = tosort[pm]; tosort[pm] = t; }
                        if (cmp(tosort[pm], tosort[pl]) < 0) { t = tosort[pm]; tosort[pm] = tosort[pl]; tosort[pl] = t; }
                        int vi = tosort[pm];
                        int pi = pl, pj = pr - 1;
                        t = tosort[pm]; tosort[pm] = tosort[pj]; tosort[pj] = t;
                        for (;;)
                        {
                            do { ++pi; } while (cmp(tosort[pi], vi) < 0 && pi < pj);
                            do { --pj; } while (cmp(vi, tosort[pj]) < 0 && pi < pj);
                            if (pi >= pj) break;
                            t = tosort[pi]; tosort[pi] = tosort[pj]; tosort[pj] = t;
                        }
                        int pk = pr - 1;
                        t = tosort[pi]; tosort[pi] = tosort[pk]; tosort[pk] = t;
                        // push the largest partition on the stack
                        if (pi - pl < pr - pi) { stack[sptr++] = pi + 1; stack[sptr++] = pr; pr = pi - 1; }
                        else { stack[sptr++] = pl; stack[sptr++] = pi - 1; pl = pi + 1; }
                        depth[psdepth++] = --cdepth;
                    }
                    // insertion sort
                    for (int pi = pl + 1; pi <= pr; ++pi)
                    {
                        int vi = tosort[pi], pj = pi, pk = pi - 1;
                        while (pj > pl && cmp(vi, tosort[pk]) < 0) { tosort[pj--] = tosort[pk--]; }
                        tosort[pj] = vi;
                    }
                }
                if (sptr == 0) break;
                pr = stack[--sptr]; pl = stack[--sptr]; cdepth = depth[--psdepth];
            }
            return tosort;
        }
        static void ArgHeapSort(int[] tosort, int start, int n, Comparison<int> cmp)
        {
            // npy_aheapsort: a[1..n] = tosort[start..start + n)
            int A(int i) => tosort[start + i - 1];
            void Set(int i, int v) { tosort[start + i - 1] = v; }
            int tmp, ii, j, l;
            for (l = n >> 1; l > 0; --l)
            {
                tmp = A(l);
                for (ii = l, j = l << 1; j <= n;)
                {
                    if (j < n && cmp(A(j), A(j + 1)) < 0) j += 1;
                    if (cmp(tmp, A(j)) < 0) { Set(ii, A(j)); ii = j; j += j; } else break;
                }
                Set(ii, tmp);
            }
            for (; n > 1;)
            {
                tmp = A(n); Set(n, A(1)); n -= 1;
                for (ii = 1, j = 2; j <= n;)
                {
                    if (j < n && cmp(A(j), A(j + 1)) < 0) j++;
                    if (cmp(tmp, A(j)) < 0) { Set(ii, A(j)); ii = j; j += j; } else break;
                }
                Set(ii, tmp);
            }
        }

        /// sorted(range(n), key=...) / np.argsort(kind='stable'): a stable index sort (Array.Sort is not stable).
        public static int[] StableArgsort<T>(IList<T> keys, Comparison<T> cmp)
        {
            int n = keys.Count; var idx = new int[n]; for (int i = 0; i < n; i++) idx[i] = i;
            if (n < 2) return idx;
            var tmp = new int[n];
            MergeSort(idx, tmp, 0, n, (a, b) => cmp(keys[a], keys[b]));
            return idx;
        }
        /// list.sort(key=...) semantics on a list (stable).
        public static void StableSort<T>(List<T> list, Comparison<T> cmp)
        {
            int n = list.Count; if (n < 2) return;
            var idx = new int[n]; for (int i = 0; i < n; i++) idx[i] = i;
            var tmp = new int[n];
            MergeSort(idx, tmp, 0, n, (a, b) => cmp(list[a], list[b]));
            var copy = list.ToArray();
            for (int i = 0; i < n; i++) list[i] = copy[idx[i]];
        }
        static void MergeSort(int[] a, int[] tmp, int lo, int hi, Comparison<int> cmp)
        {
            if (hi - lo < 2) return;
            if (hi - lo <= 16)
            {   // insertion sort (stable)
                for (int i = lo + 1; i < hi; i++)
                {
                    int v = a[i], j = i - 1;
                    while (j >= lo && cmp(a[j], v) > 0) { a[j + 1] = a[j]; j--; }
                    a[j + 1] = v;
                }
                return;
            }
            int mid = (lo + hi) >> 1;
            MergeSort(a, tmp, lo, mid, cmp); MergeSort(a, tmp, mid, hi, cmp);
            int p = lo, q = mid, k = lo;
            while (p < mid && q < hi) tmp[k++] = cmp(a[q], a[p]) < 0 ? a[q++] : a[p++];
            while (p < mid) tmp[k++] = a[p++];
            while (q < hi) tmp[k++] = a[q++];
            Array.Copy(tmp, lo, a, lo, hi - lo);
        }
    }
}
