// MX vs ATV Untamed converter, terrain stage: the numpy / scipy / Pillow / matplotlib pieces ux_terrain.py leans on, bit for bit where
// the reference's bytes depend on them (checked against numpy 2.5.4, scipy 1.18.1, Pillow 13.0.0 with
// zlib-ng 2.3.3, matplotlib 3.11.2):
//   Npy*            np.save of a float32 / int32 / uint8 array (format 1.0); ReadNpyFloat32 = np.load of hf.npy
//   PngRgb          Image.save(fp, 'PNG', optimize=False, compress_level=6) of an RGB image: Pillow's row filter choice (ZipEncode.c)
//                   and its IDAT chunking; the deflate stream is UxZng's (zlib-ng, fed row by row into 64 KiB buffers as Pillow does);
//                   ReadPngRgb = the pixels of such a file (for the stages that read the terrain pictures)
//   GaussianFilter  scipy.ndimage.gaussian_filter(a, sigma) of a 2-D float64 / float32 array (mode 'reflect', truncate 4)
//   TerrainLut      matplotlib.colormaps['terrain'] (256 entries)
//   Pcg64           np.random.default_rng(seed): SeedSequence + PCG64, random() doubles
//   Percentile / Median, PyFixed ('%.nf'), nearest neighbours (scipy.spatial.cKDTree.query for the checks)
// "System only": no UnityEngine, no unsafe code.
using System;
using System.Collections.Generic;
using System.Numerics;
using System.Text;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxImaging
    {
        // ------------------------------------------------------------------------------------------------ np.save
        static byte[] NpyHeader(string descr, string shape)
        {
            string dict = "{'descr': '" + descr + "', 'fortran_order': False, 'shape': " + shape + ", }";
            int len = 10 + dict.Length + 1;                    // magic 6 + version 2 + header length 2 + the text + '\n'
            int pad = (64 - len % 64) % 64;
            var sb = new StringBuilder(dict); sb.Append(' ', pad); sb.Append('\n');
            byte[] text = Encoding.ASCII.GetBytes(sb.ToString());
            var o = new byte[10 + text.Length];
            o[0] = 0x93; o[1] = (byte)'N'; o[2] = (byte)'U'; o[3] = (byte)'M'; o[4] = (byte)'P'; o[5] = (byte)'Y'; o[6] = 1; o[7] = 0;
            o[8] = (byte)text.Length; o[9] = (byte)(text.Length >> 8);
            Buffer.BlockCopy(text, 0, o, 10, text.Length);
            return o;
        }
        static string Inv(int v) => v.ToString(System.Globalization.CultureInfo.InvariantCulture);
        /// np.save of a float32 (h, w) array.
        public static byte[] NpyFloat32(float[] a, int h, int w)
        {
            byte[] head = NpyHeader("<f4", "(" + Inv(h) + ", " + Inv(w) + ")"); var o = new byte[head.Length + 4 * a.Length];
            Buffer.BlockCopy(head, 0, o, 0, head.Length); Buffer.BlockCopy(a, 0, o, head.Length, 4 * a.Length);
            if (!BitConverter.IsLittleEndian) for (int i = head.Length; i < o.Length; i += 4) Array.Reverse(o, i, 4);
            return o;
        }
        /// np.save of an int32 1-D array.
        public static byte[] NpyInt32(int[] a)
        {
            byte[] head = NpyHeader("<i4", "(" + Inv(a.Length) + ",)"); var o = new byte[head.Length + 4 * a.Length];
            Buffer.BlockCopy(head, 0, o, 0, head.Length);
            for (int i = 0; i < a.Length; i++) UxBin.PutI32(o, head.Length + 4 * i, a[i]);
            return o;
        }
        /// np.save of a uint8 (h, w) array.
        public static byte[] NpyUint8(byte[] a, int h, int w)
        {
            byte[] head = NpyHeader("|u1", "(" + Inv(h) + ", " + Inv(w) + ")"); var o = new byte[head.Length + a.Length];
            Buffer.BlockCopy(head, 0, o, 0, head.Length); Buffer.BlockCopy(a, 0, o, head.Length, a.Length);
            return o;
        }

        /// np.load of a float32 (h, w) array written by np.save (format 1.0 / 2.0, little-endian '<f4', C order): hf.npy for the stages
        /// that read the heightfield.
        public static float[] ReadNpyFloat32(byte[] npy, out int h, out int w)
        {
            UxBin.Require(npy.Length >= 12 && npy[0] == 0x93 && npy[1] == 'N' && npy[2] == 'U' && npy[3] == 'M' && npy[4] == 'P' && npy[5] == 'Y', "not an .npy file");
            int hlen = npy[6] == 1 ? npy[8] | npy[9] << 8 : (int)UxBin.U32LE(npy, 8), at = npy[6] == 1 ? 10 : 12;
            UxBin.Require(hlen >= 0 && at + (long)hlen <= npy.Length, ".npy header outside the file");
            string head = Encoding.ASCII.GetString(npy, at, hlen);
            UxBin.Require(head.Contains("'<f4'") && head.Contains("'fortran_order': False"), ".npy: a little-endian float32 C-order array is expected (" + head.Trim() + ")");
            int p = head.IndexOf("'shape': (", StringComparison.Ordinal), q = p < 0 ? -1 : head.IndexOf(')', p);
            UxBin.Require(p >= 0 && q > p, ".npy: no shape");
            string[] dims = head.Substring(p + 10, q - p - 10).Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            UxBin.Require(dims.Length == 2, ".npy: a 2-D array is expected");
            h = int.Parse(dims[0].Trim(), System.Globalization.CultureInfo.InvariantCulture); w = int.Parse(dims[1].Trim(), System.Globalization.CultureInfo.InvariantCulture);
            at += hlen;
            UxBin.Require(h >= 0 && w >= 0 && at + 4L * h * w == npy.Length, ".npy: " + h + " x " + w + " float32 values do not fill the file");
            var a = new float[h * w];
            for (int i = 0; i < a.Length; i++) a[i] = UxBin.F32LE(npy, at + 4 * i);
            return a;
        }

        // ------------------------------------------------------------------------------------------------ PNG
        /// The pixels of an 8-bit RGB PNG without interlace (what PngRgb writes; Image.open(path).convert('RGB') of such a file).
        public static UxImage ReadPngRgb(byte[] png)
        {
            UxBin.Require(png.Length >= 8 + 25 && png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G', "not a PNG file");
            int w = 0, h = 0; var z = new System.IO.MemoryStream(); int p = 8; bool header = false;
            while (p + 12 <= png.Length)
            {
                long n = UxBin.U32BE(png, p); string t = UxBin.Latin1(png, p + 4, 4);
                UxBin.Require(p + 12 + n <= png.Length, "PNG: chunk outside the file");
                if (t == "IHDR")
                {
                    w = (int)UxBin.U32BE(png, p + 8); h = (int)UxBin.U32BE(png, p + 12); header = true;
                    UxBin.Require(n == 13 && png[p + 16] == 8 && png[p + 17] == 2 && png[p + 20] == 0, "PNG: only 8-bit RGB without interlace is read");
                }
                else if (t == "IDAT") z.Write(png, p + 8, (int)n);
                else if (t == "IEND") break;
                p += 12 + (int)n;
            }
            UxBin.Require(header && w > 0 && h > 0, "PNG: no header");
            byte[] rows = UxZlib.Decompress(z.ToArray());
            int stride = 3 * w + 1; UxBin.Require(rows.Length == (long)stride * h, "PNG: " + rows.Length + " bytes of scanlines for " + w + " x " + h);
            var o = new byte[h * 3 * w]; int nrow = 3 * w;
            for (int y = 0; y < h; y++)
            {
                int tag = rows[y * stride];
                UxBin.Require(tag <= 4, "PNG: row filter " + tag);
                for (int i = 0; i < nrow; i++)
                {
                    int a = i >= 3 ? o[y * nrow + i - 3] : 0, b = y > 0 ? o[(y - 1) * nrow + i] : 0, c = i >= 3 && y > 0 ? o[(y - 1) * nrow + i - 3] : 0, pr = 0;
                    if (tag == 1) pr = a; else if (tag == 2) pr = b; else if (tag == 3) pr = (a + b) / 2;
                    else if (tag == 4) { int pa = Math.Abs(b - c), pb = Math.Abs(a - c), pc = Math.Abs(a + b - 2 * c); pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
                    o[y * nrow + i] = (byte)(rows[y * stride + 1 + i] + pr);
                }
            }
            return new UxImage(o, h, w, 3);
        }

        static readonly uint[] s_crc = MakeCrc();
        static uint[] MakeCrc() { var t = new uint[256]; for (uint n = 0; n < 256; n++) { uint c = n; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[n] = c; } return t; }
        static void Chunk(List<byte[]> o, string type, byte[] data, int start, int length)
        {
            var b = new byte[12 + length];
            UxBin.PutU32(b, 0, (uint)length, true);
            for (int i = 0; i < 4; i++) b[4 + i] = (byte)type[i];
            Buffer.BlockCopy(data, start, b, 8, length);
            uint c = 0xFFFFFFFFu; for (int i = 4; i < 8 + length; i++) c = s_crc[(c ^ b[i]) & 0xFF] ^ (c >> 8);
            UxBin.PutU32(b, 8 + length, c ^ 0xFFFFFFFFu, true);
            o.Add(b);
        }

        /// The filtered scanlines Pillow deflates for an RGB image (libImaging/ZipEncode.c, ZIP_PNG, no optimize): per row the filter with
        /// the smallest sum of |signed byte| among none, up, sub ("prior") and Paeth, tried in that order, each only while the best sum
        /// is above 0 and taken only when strictly smaller. (h, 1 + 3 w) bytes.
        public static byte[] PngFilterRows(byte[] rgb, int w, int h)
        {
            UxBin.Require(rgb.Length == w * h * 3, "png: " + rgb.Length + " bytes for " + w + " x " + h + " RGB");
            int n = 3 * w, stride = n + 1; const int bpp = 3;
            var o = new byte[h * stride];
            var prev = new byte[n]; var cur = new byte[n]; var up = new byte[n]; var prior = new byte[n]; var paeth = new byte[n];
            for (int y = 0; y < h; y++)
            {
                Buffer.BlockCopy(rgb, y * n, cur, 0, n);
                int sum = 0; byte[] pick = cur; byte tag = 0;
                for (int i = 0; i < n; i++) { int v = cur[i]; sum += v < 128 ? v : 256 - v; }
                if (sum > 0)
                {
                    int s = 0;
                    for (int i = 0; i < n; i++) { byte v = (byte)(cur[i] - prev[i]); up[i] = v; s += v < 128 ? v : 256 - v; }
                    if (s < sum) { pick = up; tag = 2; sum = s; }
                }
                if (sum > 0)
                {
                    int s = 0, i = 0;
                    for (; i < bpp && i < n; i++) { byte v = cur[i]; prior[i] = v; s += v < 128 ? v : 256 - v; }
                    for (; i < n; i++) { byte v = (byte)(cur[i] - cur[i - bpp]); prior[i] = v; s += v < 128 ? v : 256 - v; }
                    if (s < sum) { pick = prior; tag = 1; sum = s; }
                }
                if (sum > 0)
                {
                    int s = 0, i = 0;
                    for (; i < bpp && i < n; i++) { byte v = (byte)(cur[i] - prev[i]); paeth[i] = v; s += v < 128 ? v : 256 - v; }
                    for (; i < n; i++)
                    {
                        int a = cur[i - bpp], b = prev[i], c = prev[i - bpp];
                        int pa = Math.Abs(b - c), pb = Math.Abs(a - c), pc = Math.Abs(a + b - 2 * c);
                        byte v = (byte)(cur[i] - (pa <= pb && pa <= pc ? a : pb <= pc ? b : c));
                        paeth[i] = v; s += v < 128 ? v : 256 - v;
                    }
                    if (s < sum) { pick = paeth; tag = 4; sum = s; }
                }
                o[y * stride] = tag;
                Buffer.BlockCopy(pick, 0, o, y * stride + 1, n);
                var t = prev; prev = cur; cur = t;
            }
            return o;
        }

        /// Image.fromarray(rgb (h, w, 3) uint8).save(fp, 'PNG', optimize=False, compress_level=6): IHDR, the IDAT chunks Pillow writes
        /// (one per 65,536-byte encoder buffer), IEND.
        public static byte[] PngRgb(byte[] rgb, int w, int h)
        {
            byte[] rows = PngFilterRows(rgb, w, h);
            var parts = new List<byte[]>();
            parts.Add(new byte[] { 0x89, (byte)'P', (byte)'N', (byte)'G', 13, 10, 26, 10 });
            var ihdr = new byte[13]; UxBin.PutU32(ihdr, 0, (uint)w, true); UxBin.PutU32(ihdr, 4, (uint)h, true); ihdr[8] = 8; ihdr[9] = 2;
            Chunk(parts, "IHDR", ihdr, 0, 13);
            foreach (byte[] buf in UxZng.PillowZip(rows, 3 * w + 1, h, Math.Max(65536, w * 4))) Chunk(parts, "IDAT", buf, 0, buf.Length);
            Chunk(parts, "IEND", new byte[0], 0, 0);
            int total = 0; foreach (var p in parts) total += p.Length;
            var o = new byte[total]; int at = 0;
            foreach (var p in parts) { Buffer.BlockCopy(p, 0, o, at, p.Length); at += p.Length; }
            return o;
        }

        // ------------------------------------------------------------------------------------------------ scipy.ndimage.gaussian_filter
        // _gaussian_kernel1d(sigma, 0, int(4 sigma + 0.5)) for the two sigmas the stage uses, as the reference machine's numpy computes it
        // (np.exp, pairwise sum): w[0 .. radius], the kernel is symmetric. Another sigma goes through Math.Exp.
        static readonly long[] s_k1 = { 0x3f218a9c4fd536c6, 0x3f722724cb7eb269, 0x3faba4b99d1799ab, 0x3fcef8eb9ad499ba, 0x3fd9884a307594fb };
        static readonly long[] s_k2 = { 0x3f118aad19e4159b, 0x3f3c98b8c5d0dda5, 0x3f6227362b5fc92d, 0x3f81f30504e20207, 0x3f9ba4d4125ffd2a, 0x3fb0941b71ceef37, 0x3fbef9093fc46e5a, 0x3fc68856f9ab1983, 0x3fc98862a07ae7b4 };
        /// The half kernel w[0 .. radius] (w[radius] = the centre).
        public static double[] GaussianKernel(double sigma)
        {
            long[] k = sigma == 1.0 ? s_k1 : sigma == 2.0 ? s_k2 : null;
            if (k != null) { var r = new double[k.Length]; for (int i = 0; i < k.Length; i++) r[i] = BitConverter.Int64BitsToDouble(k[i]); return r; }
            int radius = (int)(4.0 * sigma + 0.5); double sigma2 = sigma * sigma;
            var phi = new double[2 * radius + 1];
            for (int x = -radius; x <= radius; x++) phi[x + radius] = Math.Exp(-0.5 / sigma2 * (x * x));
            double sum = UxNp.Sum(phi);
            var w = new double[radius + 1]; for (int i = 0; i <= radius; i++) w[i] = phi[i] / sum;
            return w;
        }
        /// correlate1d along one line with a symmetric kernel (ni_filters.c NI_Correlate1D): src[at + i * step], i < len, 'reflect' borders.
        static void Line(double[] ext, int len, double[] w, int radius, double[] o)
        {
            // ext: the line at [radius .. radius + len) with the borders already filled
            double centre = w[radius];
            for (int ll = 0; ll < len; ll++)
            {
                int c = ll + radius;
                double v = ext[c] * centre;
                for (int jj = -radius; jj < 0; jj++) v += (ext[c + jj] + ext[c - jj]) * w[radius + jj];
                o[ll] = v;
            }
        }
        static void Reflect(double[] ext, int len, int radius)
        {
            for (int k = 0; k < radius; k++)
            {   // d c b a | a b c d | d c b a (repeating when the border is longer than the line)
                int m = k % (2 * len); int src = m < len ? m : 2 * len - 1 - m;
                ext[radius - 1 - k] = ext[radius + src];
                ext[radius + len + k] = ext[radius + len - 1 - src];
            }
        }
        /// gaussian_filter(a, sigma) of a float64 (h, w) array.
        public static double[] GaussianFilter(double[] a, int h, int w, double sigma)
        {
            double[] k = GaussianKernel(sigma); int radius = k.Length - 1;
            var o = new double[a.Length];
            System.Threading.Tasks.Parallel.For(0, w, () => (new double[h + 2 * radius], new double[h]), (c, _, buf) =>
            {
                for (int r = 0; r < h; r++) buf.Item1[radius + r] = a[r * w + c];
                Reflect(buf.Item1, h, radius); Line(buf.Item1, h, k, radius, buf.Item2);
                for (int r = 0; r < h; r++) o[r * w + c] = buf.Item2[r];
                return buf;
            }, _ => { });
            var o2 = new double[a.Length];
            System.Threading.Tasks.Parallel.For(0, h, () => (new double[w + 2 * radius], new double[w]), (r, _, buf) =>
            {
                Array.Copy(o, r * w, buf.Item1, radius, w);
                Reflect(buf.Item1, w, radius); Line(buf.Item1, w, k, radius, buf.Item2);
                Array.Copy(buf.Item2, 0, o2, r * w, w);
                return buf;
            }, _ => { });
            return o2;
        }
        /// gaussian_filter(a, sigma) of a float32 (h, w) array: the lines are filtered in float64 and stored as float32 after each axis.
        public static float[] GaussianFilter(float[] a, int h, int w, double sigma)
        {
            double[] k = GaussianKernel(sigma); int radius = k.Length - 1;
            var o = new float[a.Length];
            System.Threading.Tasks.Parallel.For(0, w, () => (new double[h + 2 * radius], new double[h]), (c, _, buf) =>
            {
                for (int r = 0; r < h; r++) buf.Item1[radius + r] = a[r * w + c];
                Reflect(buf.Item1, h, radius); Line(buf.Item1, h, k, radius, buf.Item2);
                for (int r = 0; r < h; r++) o[r * w + c] = (float)buf.Item2[r];
                return buf;
            }, _ => { });
            var o2 = new float[a.Length];
            System.Threading.Tasks.Parallel.For(0, h, () => (new double[w + 2 * radius], new double[w]), (r, _, buf) =>
            {
                for (int c = 0; c < w; c++) buf.Item1[radius + c] = o[r * w + c];
                Reflect(buf.Item1, w, radius); Line(buf.Item1, w, k, radius, buf.Item2);
                for (int c = 0; c < w; c++) o2[r * w + c] = (float)buf.Item2[c];
                return buf;
            }, _ => { });
            return o2;
        }

        // ------------------------------------------------------------------------------------------------ matplotlib 'terrain'
        /// matplotlib.colormaps['terrain']._lut[:256, :3]: LinearSegmentedColormap.from_list of _terrain_data, 256 entries (r, g, b).
        public static double[] TerrainLut()
        {
            double[] vals = { 0.00, 0.15, 0.25, 0.50, 0.75, 1.00 };
            double[][] cols = { new[] { 0.2, 0.2, 0.6 }, new[] { 0.0, 0.6, 1.0 }, new[] { 0.0, 0.8, 0.4 }, new[] { 1.0, 1.0, 0.6 }, new[] { 0.5, 0.36, 0.33 }, new[] { 1.0, 1.0, 1.0 } };
            const int N = 256; var lut = new double[N * 3];
            var x = new double[vals.Length]; for (int i = 0; i < x.Length; i++) x[i] = vals[i] * (N - 1);
            double step = 1.0 / (N - 1);
            for (int ch = 0; ch < 3; ch++)
            {
                lut[ch] = cols[0][ch]; lut[(N - 1) * 3 + ch] = cols[cols.Length - 1][ch];
                for (int i = 1; i < N - 1; i++)
                {
                    double xind = (N - 1) * (i * step);                   // (N - 1) * np.linspace(0, 1, N) ** 1.0
                    int ind = 0; while (ind < x.Length && x[ind] < xind) ind++;        // np.searchsorted(x, xind): the first x >= xind
                    double distance = (xind - x[ind - 1]) / (x[ind] - x[ind - 1]);
                    double v = distance * (cols[ind][ch] - cols[ind - 1][ch]) + cols[ind - 1][ch];
                    lut[i * 3 + ch] = v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
                }
            }
            return lut;
        }

        // ------------------------------------------------------------------------------------------------ np.random.default_rng
        /// np.random.default_rng(seed) for a small non-negative int seed: SeedSequence(seed) -> PCG64 (XSL-RR 128/64).
        public sealed class Pcg64
        {
            ulong _sh, _sl, _ih, _il;                         // state, increment (high, low)
            const ulong MulH = 2549297995355413924UL, MulL = 4865540595714422341UL;

            public Pcg64(uint seed)
            {
                // SeedSequence: mix_entropy of [seed] into a pool of 4 words, generate_state(4, uint64)
                const uint INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded, MIX_L = 0xca01f9dd, MIX_R = 0x4973f715;
                uint hc = INIT_A;
                uint HashMix(uint value) { value ^= hc; hc = unchecked(hc * MULT_A); value = unchecked(value * hc); value ^= value >> 16; return value; }
                uint Mix(uint xx, uint yy) { uint r = unchecked(MIX_L * xx - MIX_R * yy); r ^= r >> 16; return r; }
                var pool = new uint[4];
                for (int i = 0; i < 4; i++) pool[i] = HashMix(i < 1 ? seed : 0u);
                for (int s = 0; s < 4; s++) for (int d = 0; d < 4; d++) if (s != d) pool[d] = Mix(pool[d], HashMix(pool[s]));
                var st = new uint[8]; uint hb = INIT_B;
                for (int i = 0; i < 8; i++)
                {
                    uint v = pool[i % 4]; v ^= hb; hb = unchecked(hb * MULT_B); v = unchecked(v * hb); v ^= v >> 16; st[i] = v;
                }
                ulong W(int i) => st[2 * i] | (ulong)st[2 * i + 1] << 32;
                ulong seedH = W(0), seedL = W(1), incH = W(2), incL = W(3);
                // pcg_setseq_128_srandom_r
                _sh = 0; _sl = 0;
                _ih = (incH << 1) | (incL >> 63); _il = (incL << 1) | 1;
                Step();
                ulong lo = unchecked(_sl + seedL); ulong carry = lo < _sl ? 1UL : 0UL;
                _sh = unchecked(_sh + seedH + carry); _sl = lo;
                Step();
            }
            void Step()
            {   // state = state * multiplier + increment (mod 2^128)
                ulong a0 = _sl & 0xFFFFFFFFUL, a1 = _sl >> 32, b0 = MulL & 0xFFFFFFFFUL, b1 = MulL >> 32;
                ulong p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
                ulong mid = (p00 >> 32) + (p01 & 0xFFFFFFFFUL) + (p10 & 0xFFFFFFFFUL);
                ulong lo = (p00 & 0xFFFFFFFFUL) | (mid << 32);
                ulong hi = unchecked(p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32));
                hi = unchecked(hi + _sl * MulH + _sh * MulL);
                ulong nl = unchecked(lo + _il); ulong carry = nl < lo ? 1UL : 0UL;
                _sh = unchecked(hi + _ih + carry); _sl = nl;
            }
            public ulong NextUInt64()
            {
                Step();
                ulong x = _sh ^ _sl; int rot = (int)(_sh >> 58);
                return rot == 0 ? x : (x >> rot) | (x << (64 - rot));
            }
            /// Generator.random(): a double in [0, 1).
            public double NextDouble() => (NextUInt64() >> 11) * (1.0 / 9007199254740992.0);
        }

        // ------------------------------------------------------------------------------------------------ order statistics
        /// np.percentile(a, p) (method 'linear') of the values in `sorted` (ascending).
        public static double PercentileSorted(double[] sorted, double p)
        {
            int n = sorted.Length; double q = p / 100.0;
            double vi = (n - 1) * q;
            double prev = Math.Floor(vi); double gamma = vi - prev;
            int i0 = (int)prev, i1 = i0 + 1;
            if (vi >= n - 1) { i0 = n - 1; i1 = n - 1; }
            if (vi < 0) { i0 = 0; i1 = 0; }
            double a = sorted[i0], b = sorted[i1], diff = b - a;
            double r = a + diff * gamma;
            if (gamma >= 0.5) r = b - diff * (1 - gamma);
            return r;
        }
        /// np.median of the values (the array is sorted in place).
        public static double MedianInPlace(double[] a)
        {
            int n = a.Length; Array.Sort(a);
            return n % 2 == 1 ? a[n / 2] : (a[n / 2 - 1] + a[n / 2]) / 2.0;
        }

        // ------------------------------------------------------------------------------------------------ '%.nf'
        /// Python '%.<n>f' % x: the exact binary value rounded half to even at n decimals.
        public static string PyFixed(double x, int n)
        {
            if (double.IsNaN(x)) return "nan";
            if (double.IsInfinity(x)) return x > 0 ? "inf" : "-inf";
            long bits = BitConverter.DoubleToInt64Bits(x);
            bool neg = bits < 0; int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;
            BigInteger num = m, den = BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
            num *= BigInteger.Pow(10, n);
            var q = BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            string digits = q.ToString(System.Globalization.CultureInfo.InvariantCulture);
            if (n > 0)
            {
                if (digits.Length <= n) digits = new string('0', n - digits.Length + 1) + digits;
                digits = digits.Substring(0, digits.Length - n) + "." + digits.Substring(digits.Length - n);
            }
            return (neg ? "-" : "") + digits;
        }

        // ------------------------------------------------------------------------------------------------ nearest neighbours
        /// The k nearest of a 3-D point set (scipy.spatial.cKDTree(points).query(x, k)): a uniform grid over x / z. Query returns the
        /// indices by ascending distance (equal distances: lower index first; cKDTree's order among exactly equal distances is its own).
        public sealed class PointGrid
        {
            readonly double[] _p; readonly int _n; readonly double _x0, _z0, _cell; readonly int _nx, _nz; readonly int[] _start, _items;
            public PointGrid(double[] points)
            {
                _p = points; _n = points.Length / 3;
                double x0 = double.PositiveInfinity, x1 = double.NegativeInfinity, z0 = double.PositiveInfinity, z1 = double.NegativeInfinity;
                for (int i = 0; i < _n; i++) { double x = _p[3 * i], z = _p[3 * i + 2]; if (x < x0) x0 = x; if (x > x1) x1 = x; if (z < z0) z0 = z; if (z > z1) z1 = z; }
                if (_n == 0) { x0 = z0 = 0; x1 = z1 = 1; }
                double span = Math.Max(Math.Max(x1 - x0, z1 - z0), 1e-6);
                int side = Math.Max(1, Math.Min(1024, (int)Math.Sqrt(_n / 2.0) + 1));
                _cell = span / side * 1.0000001; _x0 = x0; _z0 = z0; _nx = side + 1; _nz = side + 1;
                var count = new int[_nx * _nz + 1];
                var cellOf = new int[_n];
                for (int i = 0; i < _n; i++) { cellOf[i] = Cell(_p[3 * i], _p[3 * i + 2]); count[cellOf[i] + 1]++; }
                for (int i = 0; i < _nx * _nz; i++) count[i + 1] += count[i];
                _start = count; _items = new int[_n]; var fill = new int[_nx * _nz];
                for (int i = 0; i < _n; i++) { int c = cellOf[i]; _items[_start[c] + fill[c]++] = i; }
            }
            int Clamp(double v, int n) { int i = (int)Math.Floor(v); return i < 0 ? 0 : i >= n ? n - 1 : i; }
            int Cell(double x, double z) => Clamp((x - _x0) / _cell, _nx) * _nz + Clamp((z - _z0) / _cell, _nz);

            /// The k nearest points of (x, y, z): indices into `idx` (ascending distance), squared distances into `d2`. Returns the count.
            public int Query(double x, double y, double z, int k, int[] idx, double[] d2)
            {
                int found = 0;
                int cx = Clamp((x - _x0) / _cell, _nx), cz = Clamp((z - _z0) / _cell, _nz);
                // distance from the query to the grid's box edge on each side decides how far a ring of cells can be
                for (int ring = 0; ; ring++)
                {
                    if (ring > 0 && found == k)
                    {   // every cell of this ring is at least this far in the x / z plane
                        double lim = double.PositiveInfinity;
                        double gx = (x - _x0) / _cell, gz = (z - _z0) / _cell;
                        double dxl = gx - (cx - ring + 1), dxr = (cx + ring) - gx, dzl = gz - (cz - ring + 1), dzr = (cz + ring) - gz;
                        lim = Math.Min(Math.Min(dxl, dxr), Math.Min(dzl, dzr)) * _cell;
                        if (lim > 0 && lim * lim > d2[k - 1]) break;
                    }
                    if (ring > Math.Max(_nx, _nz)) break;
                    for (int ix = cx - ring; ix <= cx + ring; ix++)
                    {
                        if (ix < 0 || ix >= _nx) continue;
                        bool edge = ix == cx - ring || ix == cx + ring;
                        for (int iz = cz - ring; iz <= cz + ring; iz += edge ? 1 : Math.Max(1, 2 * ring))
                        {
                            if (iz < 0 || iz >= _nz) continue;
                            int c = ix * _nz + iz;
                            for (int q = _start[c]; q < _start[c + 1]; q++)
                            {
                                int i = _items[q];
                                double dx = _p[3 * i] - x, dy = _p[3 * i + 1] - y, dz = _p[3 * i + 2] - z;
                                double d = dx * dx + dy * dy + dz * dz;
                                if (found == k && !(d < d2[k - 1] || d == d2[k - 1] && i < idx[k - 1])) continue;
                                int at = found < k ? found++ : k - 1;
                                while (at > 0 && (d2[at - 1] > d || d2[at - 1] == d && idx[at - 1] > i)) { d2[at] = d2[at - 1]; idx[at] = idx[at - 1]; at--; }
                                d2[at] = d; idx[at] = i;
                            }
                        }
                    }
                }
                return found;
            }
        }
    }

    /// The deflate stream Pillow 13 writes into a PNG: zlib-ng 2.3.3 (the zlib its wheels are built with) at compression level 6
    /// (deflate_medium: 4-byte multiplicative hash, a one-position look-ahead with "fizzled" matches), window 15, memLevel 9 (32,768
    /// symbols per block), the zlib wrapper; driven as Pillow's ZipEncode.c drives it: one deflate(Z_NO_FLUSH) call per image row
    /// into 65,536-byte output buffers, then deflate(Z_FINISH). zlib-ng's deflate_medium keeps its look-ahead match in local variables,
    /// so its output depends on where the calls and the output buffers end: both are reproduced here. (The strategy Pillow asks for,
    /// Z_FILTERED, changes nothing at this level.) A port of deflate.c / deflate_medium.c / match_tpl.h / insert_string_tpl.h / trees.c.
    public static class UxZng
    {
        /// The IDAT payloads of ImageFile._save for the filtered rows (h rows of `stride` bytes): one per encoder call.
        public static List<byte[]> PillowZip(byte[] rows, int stride, int h, int bufsize)
        {
            var z = new Deflater(); var chunks = new List<byte[]>();
            int y = 0, state = 1;
            for (;;)
            {
                z.out_buf = new byte[bufsize]; z.out_pos = 0;
                bool end = false;
                if (z.avail_in > 0) z.Deflate(Deflater.Z_NO_FLUSH);            // "We have some data from previous round, deflate it first"
                bool full = false;
                if (state == 1)
                {
                    while (z.AvailOut > 0)
                    {
                        if (y >= h) { state = 2; break; }
                        z.next_in = rows; z.in_pos = y * stride; z.avail_in = stride; y++;
                        z.Deflate(Deflater.Z_NO_FLUSH);
                    }
                    if (z.AvailOut == 0) full = true;
                }
                if (!full)
                    while (z.AvailOut > 0)
                    {
                        int err = z.Deflate(Deflater.Z_FINISH);
                        if (err == Deflater.Z_STREAM_END) { end = true; break; }
                        if (err < 0) throw new UxException("zlib-ng port: deflate returned " + err);
                        if (z.AvailOut == 0) break;
                    }
                var part = new byte[z.out_pos]; Buffer.BlockCopy(z.out_buf, 0, part, 0, z.out_pos);
                chunks.Add(part);
                if (end) break;
            }
            return chunks;
        }

        sealed class Deflater
        {
            public const int Z_NO_FLUSH = 0, Z_FINISH = 4, Z_OK = 0, Z_STREAM_END = 1, Z_BUF_ERROR = -5;
            const int W_SIZE = 1 << 15, W_MASK = W_SIZE - 1, WINDOW_SIZE = 2 * W_SIZE, HASH_SIZE = 65536, HASH_MASK = HASH_SIZE - 1;
            const int STD_MIN_MATCH = 3, WANT_MIN_MATCH = 4, STD_MAX_MATCH = 258, MIN_LOOKAHEAD = STD_MAX_MATCH + STD_MIN_MATCH + 1, MAX_DIST = W_SIZE - MIN_LOOKAHEAD, WIN_INIT = STD_MAX_MATCH;
            const int GOOD_MATCH = 8, MAX_INSERT = 16, NICE_MATCH = 128, MAX_CHAIN = 128;      // configuration_table[6]
            const int LIT_BUFSIZE = 1 << 15, SYM_END = LIT_BUFSIZE - 1;                        // memLevel 9
            const int L_CODES = 286, D_CODES = 30, BL_CODES = 19, HEAP_SIZE = 2 * L_CODES + 1, LITERALS = 256, END_BLOCK = 256;
            const int MAX_BITS = 15, MAX_BL_BITS = 7, REP_3_6 = 16, REPZ_3_10 = 17, REPZ_11_138 = 18;
            const int INIT_STATE = 1, BUSY_STATE = 2, FINISH_STATE = 3;
            const int need_more = 0, block_done = 1, finish_started = 2, finish_done = 3;

            static readonly int[] ExtraL = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
            static readonly int[] ExtraD = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
            static readonly int[] ExtraBl = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7 };
            static readonly byte[] BlOrder = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            static readonly ushort[] StaticLCode = new ushort[L_CODES + 2], StaticLLen = new ushort[L_CODES + 2];
            static readonly ushort[] StaticDCode = new ushort[D_CODES], StaticDLen = new ushort[D_CODES];
            static readonly byte[] DistCode = new byte[512], LengthCode = new byte[256];
            static readonly int[] BaseLength = new int[29], BaseDist = new int[D_CODES];

            static Deflater()
            {
                int length = 0, code;
                for (code = 0; code < 28; code++) { BaseLength[code] = length; for (int n = 0; n < (1 << ExtraL[code]); n++) LengthCode[length++] = (byte)code; }
                LengthCode[length - 1] = (byte)code;
                int dist = 0;
                for (code = 0; code < 16; code++) { BaseDist[code] = dist; for (int n = 0; n < (1 << ExtraD[code]); n++) DistCode[dist++] = (byte)code; }
                dist >>= 7;
                for (; code < D_CODES; code++) { BaseDist[code] = dist << 7; for (int n = 0; n < (1 << (ExtraD[code] - 7)); n++) DistCode[256 + dist++] = (byte)code; }
                var blCount = new ushort[MAX_BITS + 1];
                int i = 0;
                while (i <= 143) { StaticLLen[i++] = 8; blCount[8]++; }
                while (i <= 255) { StaticLLen[i++] = 9; blCount[9]++; }
                while (i <= 279) { StaticLLen[i++] = 7; blCount[7]++; }
                while (i <= 287) { StaticLLen[i++] = 8; blCount[8]++; }
                GenCodes(StaticLCode, StaticLLen, L_CODES + 1, blCount);
                for (i = 0; i < D_CODES; i++) { StaticDLen[i] = 5; StaticDCode[i] = (ushort)BiReverse((uint)i, 5); }
            }
            static uint BiReverse(uint code, int len) { uint res = 0; do { res |= code & 1; code >>= 1; res <<= 1; } while (--len > 0); return res >> 1; }
            static void GenCodes(ushort[] fc, ushort[] dl, int maxCode, ushort[] blCount)
            {
                var next = new ushort[MAX_BITS + 1]; uint code = 0;
                for (int bits = 1; bits <= MAX_BITS; bits++) { code = (code + blCount[bits - 1]) << 1; next[bits] = (ushort)code; }
                for (int n = 0; n <= maxCode; n++) { int len = dl[n]; if (len == 0) continue; fc[n] = (ushort)BiReverse(next[len]++, len); }
            }

            // ---- the stream (z_stream)
            public byte[] next_in; public int in_pos, avail_in; public byte[] out_buf; public int out_pos;
            public int AvailOut => out_buf.Length - out_pos;
            uint _adA = 1, _adB = 0;
            // ---- pending output: every whole byte written and not yet delivered; under 8 bits stay in the bit buffer
            byte[] _pend = new byte[1 << 17]; int _pendLen, _pendStart; ulong _biBuf; int _biValid;
            int Pending => _pendLen - _pendStart;
            // ---- deflate state
            readonly byte[] window = new byte[WINDOW_SIZE + 64];
            readonly ushort[] prev = new ushort[W_SIZE], head = new ushort[HASH_SIZE];
            int strstart, lookahead, match_start, prev_length, insert, block_start, high_water, status = INIT_STATE, wrap = 1, last_flush = -2;
            // ---- trees
            readonly ushort[] lFc = new ushort[HEAP_SIZE], lDl = new ushort[HEAP_SIZE];
            readonly ushort[] dFc = new ushort[2 * D_CODES + 1], dDl = new ushort[2 * D_CODES + 1];
            readonly ushort[] blFc = new ushort[2 * BL_CODES + 1], blDl = new ushort[2 * BL_CODES + 1];
            int lMaxCode, dMaxCode;
            readonly ushort[] bl_count = new ushort[MAX_BITS + 1];
            readonly int[] heap = new int[2 * L_CODES + 1]; int heap_len, heap_max;
            readonly byte[] depth = new byte[2 * L_CODES + 1];
            readonly ushort[] d_buf = new ushort[LIT_BUFSIZE + 8]; readonly byte[] l_buf = new byte[LIT_BUFSIZE + 8]; int sym_next;
            long opt_len, static_len;

            public Deflater() { InitBlock(); }

            void PutByte(int b)
            {
                if (_pendLen == _pend.Length) Array.Resize(ref _pend, _pend.Length * 2);
                _pend[_pendLen++] = (byte)b;
            }
            void SendBits(int value, int length)
            {
                _biBuf |= (ulong)(uint)value << _biValid; _biValid += length;
                while (_biValid >= 8) { PutByte((int)(_biBuf & 0xFF)); _biBuf >>= 8; _biValid -= 8; }
            }
            void BiWindup() { if (_biValid > 0) PutByte((int)(_biBuf & 0xFF)); _biBuf = 0; _biValid = 0; }
            void FlushPending()
            {
                int len = Math.Min(Pending, AvailOut);
                if (len == 0) return;
                Buffer.BlockCopy(_pend, _pendStart, out_buf, out_pos, len);
                out_pos += len; _pendStart += len;
                if (_pendStart == _pendLen) { _pendStart = 0; _pendLen = 0; }
            }
            static int Rank(int f) => f * 2 - (f > 4 ? 9 : 0);

            /// deflate(strm, flush).
            public int Deflate(int flush)
            {
                int old_flush = last_flush; last_flush = flush;
                if (Pending != 0)
                {
                    FlushPending();
                    if (AvailOut == 0) { last_flush = -1; return Z_OK; }
                }
                else if (avail_in == 0 && Rank(flush) <= Rank(old_flush) && flush != Z_FINISH) return Z_BUF_ERROR;
                if (status == FINISH_STATE && avail_in != 0) return Z_BUF_ERROR;
                if (status == INIT_STATE)
                {
                    PutByte(0x78); PutByte(0x9C);              // window 15, level flags 2 (level 6)
                    status = BUSY_STATE;
                    FlushPending();
                    if (Pending != 0) { last_flush = -1; return Z_OK; }
                }
                if (avail_in != 0 || lookahead != 0 || (flush != Z_NO_FLUSH && status != FINISH_STATE))
                {
                    int bstate = DeflateMedium(flush);
                    if (bstate == finish_started || bstate == finish_done) status = FINISH_STATE;
                    if (bstate == need_more || bstate == finish_started) { if (AvailOut == 0) last_flush = -1; return Z_OK; }
                    // block_done is not reached with Z_NO_FLUSH or Z_FINISH
                }
                if (flush != Z_FINISH) return Z_OK;
                if (wrap == 1) { uint ad = (_adB << 16) | _adA; PutByte((int)(ad >> 24)); PutByte((int)(ad >> 16) & 0xFF); PutByte((int)(ad >> 8) & 0xFF); PutByte((int)ad & 0xFF); }
                FlushPending();
                if (wrap > 0) wrap = -wrap;
                return Pending == 0 ? Z_STREAM_END : Z_OK;
            }

            // ------------------------------------------------------------------------------------ deflate.c
            int ReadBuf(int at, int size)
            {
                int len = Math.Min(avail_in, size);
                if (len == 0) return 0;
                Buffer.BlockCopy(next_in, in_pos, window, at, len);
                uint a = _adA, b = _adB;
                for (int i = 0; i < len;)
                {
                    int n = Math.Min(3800, len - i);
                    for (int k = 0; k < n; k++, i++) { a += next_in[in_pos + i]; b += a; }
                    a %= 65521; b %= 65521;
                }
                _adA = a; _adB = b;
                avail_in -= len; in_pos += len;
                return len;
            }
            int QuickInsertString(int str)
            {
                uint val = (uint)(window[str] | window[str + 1] << 8 | window[str + 2] << 16 | window[str + 3] << 24);
                int hm = (int)((unchecked(val * 2654435761u) >> 16) & HASH_MASK);
                int h = head[hm];
                if (h != str) { prev[str & W_MASK] = (ushort)h; head[hm] = (ushort)str; }
                return h;
            }
            void InsertString(int str, int count)
            {
                for (int idx = str; idx < str + count; idx++)
                {
                    uint val = (uint)(window[idx] | window[idx + 1] << 8 | window[idx + 2] << 16 | window[idx + 3] << 24);
                    int hm = (int)((unchecked(val * 2654435761u) >> 16) & HASH_MASK);
                    int h = head[hm];
                    if (h != (idx & 0xFFFF)) { prev[idx & W_MASK] = (ushort)h; head[hm] = (ushort)idx; }
                }
            }
            void FillWindow()
            {
                do
                {
                    int more = WINDOW_SIZE - lookahead - strstart;
                    if (strstart >= W_SIZE + MAX_DIST)
                    {
                        Buffer.BlockCopy(window, W_SIZE, window, 0, W_SIZE);
                        if (match_start >= W_SIZE) match_start -= W_SIZE; else { match_start = 0; prev_length = 0; }
                        strstart -= W_SIZE; block_start -= W_SIZE;
                        if (insert > strstart) insert = strstart;
                        for (int n = 0; n < HASH_SIZE; n++) { int m = head[n]; head[n] = (ushort)(m >= W_SIZE ? m - W_SIZE : 0); }
                        for (int n = 0; n < W_SIZE; n++) { int m = prev[n]; prev[n] = (ushort)(m >= W_SIZE ? m - W_SIZE : 0); }
                        more += W_SIZE;
                    }
                    if (avail_in == 0) break;
                    int got = ReadBuf(strstart + lookahead, more);
                    lookahead += got;
                    if (lookahead + insert >= STD_MIN_MATCH)
                    {
                        int str = strstart - insert;
                        if (str >= 1) QuickInsertString(str + 2 - STD_MIN_MATCH);
                        int count = insert;
                        if (lookahead == 1) count -= 1;
                        if (count > 0) { InsertString(str, count); insert -= count; }
                    }
                } while (lookahead < MIN_LOOKAHEAD && avail_in != 0);
                if (high_water < WINDOW_SIZE)
                {
                    int curr = strstart + lookahead, init;
                    if (high_water < curr)
                    {
                        init = WINDOW_SIZE - curr; if (init > WIN_INIT) init = WIN_INIT;
                        Array.Clear(window, curr, init);
                        high_water = curr + init;
                    }
                    else if (high_water < curr + WIN_INIT)
                    {
                        init = curr + WIN_INIT - high_water;
                        if (init > WINDOW_SIZE - high_water) init = WINDOW_SIZE - high_water;
                        Array.Clear(window, high_water, init);
                        high_water += init;
                    }
                }
            }

            bool Same(int a, int b, int n) { byte[] w = window; for (int i = 0; i < n; i++) if (w[a + i] != w[b + i]) return false; return true; }
            /// longest_match (match_tpl.h, the variant deflate_medium uses): sets match_start, returns the length.
            int LongestMatch(int cur_match)
            {
                byte[] w = window; int scan = strstart;
                int best_len = prev_length != 0 ? prev_length : STD_MIN_MATCH - 1;
                int offset = best_len - 1; if (best_len >= 4) { offset -= 2; if (best_len >= 8) offset -= 4; }
                int chain_length = MAX_CHAIN; if (best_len >= GOOD_MATCH) chain_length >>= 2;
                int limit = strstart > MAX_DIST ? strstart - MAX_DIST : 0;
                for (;;)
                {
                    if (cur_match >= strstart) break;
                    int k = best_len < 4 ? 2 : best_len >= 8 ? 8 : 4;
                    for (;;)
                    {
                        if (Same(cur_match + offset, scan + offset, k) && Same(cur_match, scan, k)) break;
                        if (--chain_length != 0 && (cur_match = prev[cur_match & W_MASK]) > limit) continue;
                        return best_len;
                    }
                    int len = 0, a = scan + 2, b = cur_match + 2;              // compare256
                    while (len < 256 && w[a + len] == w[b + len]) len++;
                    len += 2;
                    if (len > best_len)
                    {
                        match_start = cur_match;
                        if (len >= lookahead) return lookahead;
                        best_len = len;
                        if (best_len >= NICE_MATCH) return best_len;
                        offset = best_len - 1; if (best_len >= 4) { offset -= 2; if (best_len >= 8) offset -= 4; }
                    }
                    if (--chain_length != 0 && (cur_match = prev[cur_match & W_MASK]) > limit) continue;
                    return best_len;
                }
                return best_len;
            }

            int TallyLit(int c)
            {
                d_buf[sym_next] = 0; l_buf[sym_next++] = (byte)c;
                lFc[c]++;
                return sym_next == SYM_END ? 1 : 0;
            }
            int TallyDist(int dist, int len)
            {
                d_buf[sym_next] = (ushort)dist; l_buf[sym_next++] = (byte)len;
                dist--;
                lFc[LengthCode[len] + LITERALS + 1]++;
                dFc[dist < 256 ? DistCode[dist] : DistCode[256 + (dist >> 7)]]++;
                return sym_next == SYM_END ? 1 : 0;
            }
            void FlushBlockOnly(bool last)
            {
                TrFlushBlock(block_start >= 0 ? block_start : -1, strstart - block_start, last);
                block_start = strstart;
                FlushPending();
            }

            // ------------------------------------------------------------------------------------ deflate_medium.c
            struct Match { public int match_start, match_length, strstart, orgstart; }

            int EmitMatch(Match m)
            {
                int bflush = 0;
                if (m.match_length < WANT_MIN_MATCH)
                {
                    while (m.match_length != 0) { bflush += TallyLit(window[m.strstart]); lookahead--; m.strstart++; m.match_length--; }
                    return bflush;
                }
                bflush += TallyDist(m.strstart - m.match_start, m.match_length - STD_MIN_MATCH);
                lookahead -= m.match_length;
                return bflush;
            }
            void InsertMatch(Match m)
            {
                if (lookahead <= m.match_length + WANT_MIN_MATCH) return;
                if (m.match_length < WANT_MIN_MATCH)
                {
                    m.strstart++; m.match_length--;
                    if (m.match_length > 0 && m.strstart >= m.orgstart)
                    {
                        if (m.strstart + m.match_length - 1 >= m.orgstart) InsertString(m.strstart, m.match_length);
                        else InsertString(m.strstart, m.orgstart - m.strstart + 1);
                    }
                    return;
                }
                if (m.match_length <= 16 * MAX_INSERT && lookahead >= WANT_MIN_MATCH)
                {
                    m.match_length--; m.strstart++;
                    if (m.strstart >= m.orgstart)
                    {
                        if (m.strstart + m.match_length - 1 >= m.orgstart) InsertString(m.strstart, m.match_length);
                        else InsertString(m.strstart, m.orgstart - m.strstart + 1);
                    }
                    else if (m.orgstart < m.strstart + m.match_length) InsertString(m.orgstart, m.strstart + m.match_length - m.orgstart);
                }
                else
                {
                    m.strstart += m.match_length;
                    if (m.strstart >= STD_MIN_MATCH - 2) QuickInsertString(m.strstart + 2 - STD_MIN_MATCH);
                }
            }
            void FizzleMatches(ref Match current, ref Match next)
            {
                if (current.match_length <= 1) return;
                if (current.match_length > 1 + next.match_start) return;
                if (current.match_length > 1 + next.strstart) return;
                int match = -current.match_length + 1 + next.match_start, orig = -current.match_length + 1 + next.strstart;
                if (window[match] != window[orig]) return;
                Match c = current, n = next;
                int limit = next.strstart > MAX_DIST ? next.strstart - MAX_DIST : 0;
                match = n.match_start - 1; orig = n.strstart - 1;
                int changed = 0;
                while (window[match] == window[orig])
                {
                    if (c.match_length < 1) break;
                    if (n.strstart <= limit) break;
                    if (n.match_length >= 256) break;
                    if (n.match_start <= 1) break;
                    n.strstart--; n.match_start--; n.match_length++; c.match_length--; match--; orig--; changed++;
                }
                if (changed == 0) return;
                if (c.match_length <= 1 && n.match_length != 2) { n.orgstart++; current = c; next = n; }
            }
            int DeflateMedium(int flush)
            {
                Match cur = default, next = default;
                for (;;)
                {
                    int hash_head; long dist;
                    if (lookahead < MIN_LOOKAHEAD)
                    {
                        FillWindow();
                        if (lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) return need_more;
                        if (lookahead == 0) break;
                        next.match_length = 0;
                    }
                    if (next.match_length > 0) { cur = next; next.match_length = 0; }
                    else
                    {
                        hash_head = 0;
                        if (lookahead >= WANT_MIN_MATCH) hash_head = QuickInsertString(strstart);
                        cur.strstart = strstart; cur.orgstart = cur.strstart;
                        dist = (long)strstart - hash_head;
                        if (dist <= MAX_DIST && dist > 0 && hash_head != 0)
                        {
                            cur.match_length = LongestMatch(hash_head); cur.match_start = match_start;
                            if (cur.match_length < WANT_MIN_MATCH) cur.match_length = 1;
                            if (cur.match_start >= cur.strstart) cur.match_length = 1;
                        }
                        else { cur.match_start = 0; cur.match_length = 1; }
                    }
                    InsertMatch(cur);
                    if (lookahead > MIN_LOOKAHEAD && cur.strstart + cur.match_length < WINDOW_SIZE - MIN_LOOKAHEAD)
                    {
                        strstart = cur.strstart + cur.match_length;
                        hash_head = QuickInsertString(strstart);
                        next.strstart = strstart; next.orgstart = next.strstart;
                        dist = (long)strstart - hash_head;
                        if (dist <= MAX_DIST && dist > 0 && hash_head != 0)
                        {
                            next.match_length = LongestMatch(hash_head); next.match_start = match_start;
                            if (next.match_start >= next.strstart) next.match_length = 1;
                            if (next.match_length < WANT_MIN_MATCH) next.match_length = 1;
                            else FizzleMatches(ref cur, ref next);
                        }
                        else { next.match_start = 0; next.match_length = 1; }
                        strstart = cur.strstart;
                    }
                    else next.match_length = 0;
                    int bflush = EmitMatch(cur);
                    strstart += cur.match_length;
                    if (bflush != 0) { FlushBlockOnly(false); if (AvailOut == 0) return need_more; }
                }
                insert = strstart < STD_MIN_MATCH - 1 ? strstart : STD_MIN_MATCH - 1;
                if (flush == Z_FINISH) { FlushBlockOnly(true); return AvailOut == 0 ? finish_started : finish_done; }
                if (sym_next != 0) { FlushBlockOnly(false); if (AvailOut == 0) return need_more; }
                return block_done;
            }

            // ------------------------------------------------------------------------------------ trees.c
            void InitBlock()
            {
                for (int n = 0; n < L_CODES; n++) lFc[n] = 0;
                for (int n = 0; n < D_CODES; n++) dFc[n] = 0;
                for (int n = 0; n < BL_CODES; n++) blFc[n] = 0;
                lFc[END_BLOCK] = 1;
                opt_len = static_len = 0; sym_next = 0;
            }
            bool Smaller(ushort[] fc, int n, int m) => fc[n] < fc[m] || fc[n] == fc[m] && depth[n] <= depth[m];
            void PqDownHeap(ushort[] fc, int k)
            {
                int v = heap[k], j = k << 1;
                while (j <= heap_len)
                {
                    if (j < heap_len && Smaller(fc, heap[j + 1], heap[j])) j++;
                    if (Smaller(fc, v, heap[j])) break;
                    heap[k] = heap[j]; k = j;
                    j <<= 1;
                }
                heap[k] = v;
            }
            void GenBitlen(ushort[] fc, ushort[] dl, int maxCode, ushort[] stLen, int[] extra, int baseIndex, int maxLength)
            {
                int h, n, m, bits, xbits, overflow = 0;
                for (bits = 0; bits <= MAX_BITS; bits++) bl_count[bits] = 0;
                dl[heap[heap_max]] = 0;
                for (h = heap_max + 1; h < HEAP_SIZE; h++)
                {
                    n = heap[h];
                    bits = dl[dl[n]] + 1;
                    if (bits > maxLength) { bits = maxLength; overflow++; }
                    dl[n] = (ushort)bits;
                    if (n > maxCode) continue;
                    bl_count[bits]++;
                    xbits = 0;
                    if (n >= baseIndex) xbits = extra[n - baseIndex];
                    int f = fc[n];
                    opt_len += (long)f * (bits + xbits);
                    if (stLen != null) static_len += (long)f * (stLen[n] + xbits);
                }
                if (overflow == 0) return;
                do
                {
                    bits = maxLength - 1;
                    while (bl_count[bits] == 0) bits--;
                    bl_count[bits]--;
                    bl_count[bits + 1] += 2;
                    bl_count[maxLength]--;
                    overflow -= 2;
                } while (overflow > 0);
                for (bits = maxLength; bits != 0; bits--)
                {
                    n = bl_count[bits];
                    while (n != 0)
                    {
                        m = heap[--h];
                        if (m > maxCode) continue;
                        if (dl[m] != bits) { opt_len += ((long)bits - dl[m]) * fc[m]; dl[m] = (ushort)bits; }
                        n--;
                    }
                }
            }
            int BuildTree(ushort[] fc, ushort[] dl, ushort[] stLen, int[] extra, int baseIndex, int elems, int maxLength)
            {
                int n, m, max_code = -1, node;
                heap_len = 0; heap_max = HEAP_SIZE;
                for (n = 0; n < elems; n++)
                {
                    if (fc[n] != 0) { heap[++heap_len] = max_code = n; depth[n] = 0; }
                    else dl[n] = 0;
                }
                while (heap_len < 2)
                {
                    node = heap[++heap_len] = max_code < 2 ? ++max_code : 0;
                    fc[node] = 1; depth[node] = 0;
                    opt_len--; if (stLen != null) static_len -= stLen[node];
                }
                for (n = heap_len / 2; n >= 1; n--) PqDownHeap(fc, n);
                node = elems;
                do
                {
                    n = heap[1]; heap[1] = heap[heap_len--]; PqDownHeap(fc, 1);
                    m = heap[1];
                    heap[--heap_max] = n; heap[--heap_max] = m;
                    fc[node] = (ushort)(fc[n] + fc[m]);
                    depth[node] = (byte)((depth[n] >= depth[m] ? depth[n] : depth[m]) + 1);
                    dl[n] = dl[m] = (ushort)node;
                    heap[1] = node++;
                    PqDownHeap(fc, 1);
                } while (heap_len >= 2);
                heap[--heap_max] = heap[1];
                GenBitlen(fc, dl, max_code, stLen, extra, baseIndex, maxLength);
                GenCodes(fc, dl, max_code, bl_count);
                return max_code;
            }
            void ScanTree(ushort[] dl, int max_code)
            {
                int prevlen = -1, curlen, nextlen = dl[0], count = 0, max_count = 7, min_count = 4;
                if (nextlen == 0) { max_count = 138; min_count = 3; }
                dl[max_code + 1] = 0xFFFF;
                for (int n = 0; n <= max_code; n++)
                {
                    curlen = nextlen; nextlen = dl[n + 1];
                    if (++count < max_count && curlen == nextlen) continue;
                    else if (count < min_count) blFc[curlen] += (ushort)count;
                    else if (curlen != 0) { if (curlen != prevlen) blFc[curlen]++; blFc[REP_3_6]++; }
                    else if (count <= 10) blFc[REPZ_3_10]++;
                    else blFc[REPZ_11_138]++;
                    count = 0; prevlen = curlen;
                    if (nextlen == 0) { max_count = 138; min_count = 3; }
                    else if (curlen == nextlen) { max_count = 6; min_count = 3; }
                    else { max_count = 7; min_count = 4; }
                }
            }
            void SendTree(ushort[] dl, int max_code)
            {
                int prevlen = -1, curlen, nextlen = dl[0], count = 0, max_count = 7, min_count = 4;
                if (nextlen == 0) { max_count = 138; min_count = 3; }
                for (int n = 0; n <= max_code; n++)
                {
                    curlen = nextlen; nextlen = dl[n + 1];
                    if (++count < max_count && curlen == nextlen) continue;
                    else if (count < min_count) { do { SendBits(blFc[curlen], blDl[curlen]); } while (--count != 0); }
                    else if (curlen != 0)
                    {
                        if (curlen != prevlen) { SendBits(blFc[curlen], blDl[curlen]); count--; }
                        SendBits(blFc[REP_3_6], blDl[REP_3_6]); SendBits(count - 3, 2);
                    }
                    else if (count <= 10) { SendBits(blFc[REPZ_3_10], blDl[REPZ_3_10]); SendBits(count - 3, 3); }
                    else { SendBits(blFc[REPZ_11_138], blDl[REPZ_11_138]); SendBits(count - 11, 7); }
                    count = 0; prevlen = curlen;
                    if (nextlen == 0) { max_count = 138; min_count = 3; }
                    else if (curlen == nextlen) { max_count = 6; min_count = 3; }
                    else { max_count = 7; min_count = 4; }
                }
            }
            int BuildBlTree()
            {
                ScanTree(lDl, lMaxCode);
                ScanTree(dDl, dMaxCode);
                BuildTree(blFc, blDl, null, ExtraBl, 0, BL_CODES, MAX_BL_BITS);
                int max_blindex;
                for (max_blindex = BL_CODES - 1; max_blindex >= 3; max_blindex--) if (blDl[BlOrder[max_blindex]] != 0) break;
                opt_len += 3 * ((long)max_blindex + 1) + 5 + 5 + 4;
                return max_blindex;
            }
            void CompressBlock(ushort[] lCode, ushort[] lLen, ushort[] dCode, ushort[] dLen)
            {
                for (int sx = 0; sx < sym_next; sx++)
                {
                    int dist = d_buf[sx], lc = l_buf[sx];
                    if (dist == 0) SendBits(lCode[lc], lLen[lc]);
                    else
                    {
                        int code = LengthCode[lc];
                        SendBits(lCode[code + LITERALS + 1], lLen[code + LITERALS + 1]);
                        int extra = ExtraL[code];
                        if (extra != 0) { lc -= BaseLength[code]; SendBits(lc, extra); }
                        dist--;
                        code = dist < 256 ? DistCode[dist] : DistCode[256 + (dist >> 7)];
                        SendBits(dCode[code], dLen[code]);
                        extra = ExtraD[code];
                        if (extra != 0) { dist -= BaseDist[code]; SendBits(dist, extra); }
                    }
                }
                SendBits(lCode[END_BLOCK], lLen[END_BLOCK]);
            }
            /// zng_tr_flush_block: stored, static or dynamic, whichever is smallest; buf = the block's start in the window (-1: too old).
            void TrFlushBlock(int buf, long stored_len, bool last)
            {
                long opt_lenb, static_lenb; int max_blindex = 0;
                if (sym_next == 0) { opt_lenb = static_lenb = 0; static_len = 7; }
                else
                {
                    lMaxCode = BuildTree(lFc, lDl, StaticLLen, ExtraL, LITERALS + 1, L_CODES, MAX_BITS);
                    dMaxCode = BuildTree(dFc, dDl, StaticDLen, ExtraD, 0, D_CODES, MAX_BITS);
                    max_blindex = BuildBlTree();
                    opt_lenb = (opt_len + 3 + 7) >> 3; static_lenb = (static_len + 3 + 7) >> 3;
                    if (static_lenb <= opt_lenb) opt_lenb = static_lenb;
                }
                if (stored_len + 4 <= opt_lenb && buf >= 0)
                {
                    SendBits(last ? 1 : 0, 3);
                    BiWindup();
                    PutByte((int)(stored_len & 0xFF)); PutByte((int)((stored_len >> 8) & 0xFF));
                    PutByte((int)(~stored_len & 0xFF)); PutByte((int)((~stored_len >> 8) & 0xFF));
                    for (long i = 0; i < stored_len; i++) PutByte(window[buf + i]);
                }
                else if (static_lenb == opt_lenb)
                {
                    SendBits((1 << 1) + (last ? 1 : 0), 3);
                    CompressBlock(StaticLCode, StaticLLen, StaticDCode, StaticDLen);
                }
                else
                {
                    SendBits((2 << 1) + (last ? 1 : 0), 3);
                    int lcodes = lMaxCode + 1, dcodes = dMaxCode + 1, blcodes = max_blindex + 1;
                    SendBits(lcodes - 257, 5); SendBits(dcodes - 1, 5); SendBits(blcodes - 4, 4);
                    for (int rank = 0; rank < blcodes; rank++) SendBits(blDl[BlOrder[rank]], 3);
                    SendTree(lDl, lcodes - 1);
                    SendTree(dDl, dcodes - 1);
                    CompressBlock(lFc, lDl, dFc, dDl);
                }
                InitBlock();
                if (last) BiWindup();
            }
        }
    }
}
