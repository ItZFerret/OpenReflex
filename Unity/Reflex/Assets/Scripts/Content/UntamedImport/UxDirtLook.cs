// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_dirtlook.py (DESIGN 23.10, the "dirt look"
// round of 2026-11-11): stock Reflex SX ground on the stock base (ux_build_track --dirt-look stock, the default of the sx builds).
// The base's own HF material and its four detail textures stay (stock bytes); this module generates the three maps around OUR track:
//   DetailMask (2048^2, alpha 0) and ChromLum (2048^2) from SX_FortDodge's profile (PROFILE_*: its mask / ChromLum alpha by signed
//   distance to its race corridor edge) on our track region = the race corridor of the build's tdf (every race section, 41 lateral
//   fractions of the outer edges, 0.5 ft steps, closed) U Untamed's painted lanes (the TMask ruts channel > 0.5 on the footprint);
//   ChromLum alpha x Untamed's macro brightness x occlusion (topdown_occ / the layer composite), its spread matched to FortDodge's;
//   an edge line in ChromLum rgb; outside the terrain footprint FortDodge's outside values;
//   TopDown (2048^2 RGB) = the client's near albedo of those maps at the base layers' mean colours.
//   UxDirtLook.StockLook(work, tdir, tj, tdf, means, S)   stock_look: (mask RGBA, chromlum RGBA, topdown RGB, record)
//   UxDirtLook.MaterialTextures / LayerMeans / CorridorMask / ClientIndex / OnClientRgb / Composite   the pieces
// Number rules (they decide bytes): float64 throughout; np.interp with precomputed slopes; scipy's exact Euclidean distance transform
// (sqrt of an integer); np.median / np.std / np.mean with numpy's pairwise sums; uint8 by truncation of clip(x * 255 + 0.5).
// np.hypot (the corridor's segment lengths) is sqrt(x * x + y * y) with every step rounded, as the reference machine's C library.
// exp / log / pow: the reference calls the C library (np.exp, np.log, v ** k). Here they are computed in double-double arithmetic
// (+ - * / only; the logarithm starts from Math.Log and takes one Newton step, so that value's last bits do not matter) and rounded
// once: the correctly rounded value, on every platform and runtime. Where the C library's result is not the correctly rounded one
// (0.1 - 0.5 % of the values) it differs from this by one unit in the last place; that reaches a byte only when a value
// x * 255 + 0.5 lies within about 1e-13 of an integer (see UNTAMED_CONVERT.md; the 12 tracks' maps equal the reference's byte for byte).
// Not ported: the previews the builder saves (assembly/dirtlook_*.png) and main() (prints a build's record).
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_dirtlook.py.
    public static class UxDirtLook
    {
        public const string TOOL = "ux_dirtlook";
        /// The reference module this port mirrors (md5 of ux_dirtlook.py; the builder records it as rollout.dirt_look.tool_md5).
        public const string TOOL_MD5 = "566533afdb855a43d62ec60d4e205e91";
        public static readonly string[] LOOKS = { "stock", "untamed" };
        /// DetailMask / ChromLum / TopDown size (texel j centre: x = -S/2 + (j + 0.5) S / G).
        public const int G = 2048;
        static readonly double[] LUM = { 0.299, 0.587, 0.114 };
        /// maskScale with the neutral deformation (ReflexTerrain.shader 516).
        public static readonly double MASK_SCALE = 1.0 - Math.Min(Math.Abs(128 / 255.0 - 0.5) * 8, 1.0);
        // SX_FortDodge: signed distance (ft) to the race-corridor edge (negative inside) -> mean DetailMask rgb and ChromLum alpha
        public static readonly double[] PROFILE_SD = { -12.0, -6.0, -3.0, -1.5, -0.5, 0.5, 1.5, 3.0, 6.0, 11.5, 22.5 };
        public static readonly double[][] PROFILE_MASK =
        {
            new[] { 0.289, 0.159, 0.059 }, new[] { 0.346, 0.131, 0.063 }, new[] { 0.391, 0.077, 0.061 }, new[] { 0.422, 0.053, 0.061 },
            new[] { 0.442, 0.043, 0.064 }, new[] { 0.472, 0.035, 0.064 }, new[] { 0.513, 0.027, 0.060 }, new[] { 0.578, 0.018, 0.060 },
            new[] { 0.693, 0.009, 0.048 }, new[] { 0.799, 0.010, 0.041 }, new[] { 0.750, 0.057, 0.021 },
        };
        public static readonly double[] PROFILE_A = { 0.411, 0.405, 0.450, 0.552, 0.593, 0.553, 0.473, 0.397, 0.355, 0.329, 0.331 };
        public static readonly double[] OUTSIDE_MASK = { 0.105, 0.781, 0.0 };
        public const double OUTSIDE_A = 0.442, OUTSIDE_AFTER_FT = 30.0;
        public const double LINE_PEAK = 0.40, LINE_CENTRE_FT = -0.6, LINE_SIGMA_FT = 1.1;
        public const double VAR_TARGET = 0.268, VAR_CLIP_LO = 0.55, VAR_CLIP_HI = 1.30, A_CLIP_LO = 0.05, A_CLIP_HI = 0.75;
        /// saturate(2 a) = 0.08 = ux_terrain.UNCLAMP_DARK.
        public const double UNCLAMP_A = 0.04, LANE_MIN = 0.5;
        public const string RUTS_TEX = "ui_indoor_ruts01";
        const string PROFILE_TEXT = "SX_FortDodge (work/combined/dirtlook/probe/profile.json)";
        const string VAR_SOURCE = "topdown_occ.png / layer composite (Untamed macro brightness x occlusion)";

        /// Tests only: true computes every v ** k in double-double arithmetic (the default takes the C library's pow and recomputes the
        /// values whose byte could depend on its last bits). The maps must not depend on it.
        public static bool ExactPow;

        // ================================================================================================ double-double exp / log / pow
        static void TwoSum(double a, double b, out double s, out double e) { s = a + b; double bb = s - a; e = (a - (s - bb)) + (b - bb); }
        static void QuickTwoSum(double a, double b, out double s, out double e) { s = a + b; e = b - (s - a); }
        static void Split(double a, out double hi, out double lo) { double t = 134217729.0 * a; hi = t - (t - a); lo = a - hi; }
        static void TwoProd(double a, double b, out double p, out double e)
        {
            p = a * b;
            Split(a, out double ah, out double al); Split(b, out double bh, out double bl);
            e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
        }
        /// A double-double value hi + lo (|lo| <= ulp(hi) / 2).
        struct Dd
        {
            public double hi, lo;
            public Dd(double hi, double lo = 0) { this.hi = hi; this.lo = lo; }
            public static Dd Add(Dd a, Dd b)
            {
                TwoSum(a.hi, b.hi, out double s, out double e); TwoSum(a.lo, b.lo, out double t, out double f);
                e += t; QuickTwoSum(s, e, out s, out e);
                e += f; QuickTwoSum(s, e, out s, out e);
                return new Dd(s, e);
            }
            public static Dd Mul(Dd a, Dd b)
            {
                TwoProd(a.hi, b.hi, out double p, out double e);
                e += a.hi * b.lo + a.lo * b.hi;
                QuickTwoSum(p, e, out p, out e);
                return new Dd(p, e);
            }
            public static Dd Mul(Dd a, double b)
            {
                TwoProd(a.hi, b, out double p, out double e);
                e += a.lo * b;
                QuickTwoSum(p, e, out p, out e);
                return new Dd(p, e);
            }
            public static Dd Div(Dd a, double b)
            {
                double q1 = a.hi / b;
                TwoProd(q1, b, out double p, out double e);
                double q2 = (((a.hi - p) - e) + a.lo) / b;
                QuickTwoSum(q1, q2, out q1, out q2);
                return new Dd(q1, q2);
            }
            public Dd Scale(double pow2) => new Dd(hi * pow2, lo * pow2);       // exact for a power of two
        }
        static readonly Dd LN2 = new Dd(BitConverter.Int64BitsToDouble(0x3FE62E42FEFA39EFL), BitConverter.Int64BitsToDouble(0x3C7ABC9E3B39803FL));
        static double Pow2(int k) => BitConverter.Int64BitsToDouble((long)(k + 1023) << 52);

        /// e^x as a double-double (|x| < 700): x = k ln 2 + r, e^(r / 512) - 1 by its Taylor series, squared nine times.
        static Dd ExpDd(Dd x)
        {
            int k = (int)Math.Floor(x.hi / LN2.hi + 0.5);
            var r = Dd.Add(x, Dd.Mul(LN2, -(double)k)).Scale(1.0 / 512);
            var s = new Dd(1.0);
            for (int n = 10; n >= 2; n--) s = Dd.Add(new Dd(1.0), Dd.Div(Dd.Mul(r, s), n));
            var p = Dd.Mul(r, s);                                                // e^r - 1
            for (int i = 0; i < 9; i++) p = Dd.Add(p.Scale(2.0), Dd.Mul(p, p));  // (1 + p)^2 - 1
            return Dd.Add(new Dd(1.0), p).Scale(Pow2(k));
        }
        /// ln x as a double-double (x > 0, finite): one Newton step from the C library's value (y0 + d - d^2/2 + d^3/3, d = x e^-y0 - 1;
        /// the result does not depend on y0's last bits).
        static Dd LogDd(double x)
        {
            double y0 = Math.Log(x);
            var d = Dd.Add(Dd.Mul(ExpDd(new Dd(-y0)), x), new Dd(-1.0));
            double dd = d.hi;
            var corr = Dd.Add(d, new Dd(dd * dd * (-0.5 + dd / 3.0)));
            return Dd.Add(new Dd(y0), corr);
        }
        /// exp(x), log(x), x ** k rounded once from the double-double value (see the header).
        public static double ExpExact(double x) { if (x == 0) return 1.0; var e = ExpDd(new Dd(x)); return e.hi + e.lo; }
        public static double LogExact(double x) { if (x == 1.0) return 0.0; var l = LogDd(x); return l.hi + l.lo; }
        public static double PowExact(double x, double k)
        {
            if (k == 0.0 || x == 1.0) return 1.0;
            if (k == 1.0) return x;
            var e = ExpDd(Dd.Mul(LogDd(x), k));
            return e.hi + e.lo;
        }
        /// np.hypot(x, y) for the segment lengths of the corridor (a few ft: no overflow or underflow): the reference machine's C
        /// library returns sqrt(x * x + y * y) with every step rounded (measured on 200,000 pairs: all equal; the correctly rounded
        /// value differs from it by one unit in the last place on 16 % of them).
        public static double Hypot(double x, double y) => Math.Sqrt(x * x + y * y);

        // ================================================================================================ small pieces
        /// material_textures(mat_bytes): HF material bytes -> (shader, the texture names in sampler order, {param: its values}).
        public static (string shader, List<string> textures, List<KeyValuePair<string, double[]>> parameters) MaterialTextures(byte[] material)
        {
            string x = new UxBxml(material).ToXml();
            var sh = Regex.Match(x, "<Shaders asset=\"([^\"]+)\"", RegexOptions.CultureInvariant);
            var pars = new List<KeyValuePair<string, double[]>>();
            foreach (Match m in Regex.Matches(x, "<Parameter name=\"([^\"]+)\"[^>]*>([^<]+)</Parameter>", RegexOptions.CultureInvariant))
            {
                var parts = m.Groups[2].Value.Split(',');
                var v = new double[parts.Length];
                for (int i = 0; i < parts.Length; i++) v[i] = UxJson.ParseDouble(parts[i].Trim());
                int at = pars.FindIndex(kv => kv.Key == m.Groups[1].Value);
                var row = new KeyValuePair<string, double[]>(m.Groups[1].Value, v);
                if (at >= 0) pars[at] = row; else pars.Add(row);
            }
            var texs = new List<string>();
            foreach (Match m in Regex.Matches(x, "<Texture textureName=\"([^\"]+)\"", RegexOptions.CultureInvariant)) texs.Add(m.Groups[1].Value);
            return (sh.Success ? sh.Groups[1].Value : null, texs, pars);
        }

        static readonly byte[] PC_DXT3 = { (byte)'D', (byte)'X', (byte)'T', (byte)'3' };
        /// _decode(data): mip 0 of a DXT texture as (h, w, 4) uint8 (the reference divides by 255).
        static UxImage Decode(byte[] data)
        {
            var t = new UxPcTexture(data);
            var (_, w, h, f) = t.recs[0];
            string name = UxTex.Fmt(f, UxTex.PC_DXT1) ? "DXT1" : UxTex.Fmt(f, UxTex.PC_DXT5) ? "DXT4_5" : UxTex.Fmt(f, PC_DXT3) ? "DXT2_3" : null;
            if (name == null) throw new UxException("texture format " + UxBin.Hex(f) + " is not DXT");
            int bpb = name == "DXT1" ? 8 : 16, need = (w / 4) * (h / 4) * bpb;
            UxBin.Require(t.entries[0].Length >= need, "texture mip 0 is shorter than its blocks");
            return UxTex.DecodeBlocks(new UxBlocks(UxBin.Slice(t.entries[0], 0, need), h / 4, w / 4, bpb), name);
        }
        /// layer_means(texture_bytes): [bytes of slots 1..4] -> 4 x 3 mean colours (mip 0, raw texel values), row-major. The mean along
        /// axis 0 of an (n, 3) array: numpy adds row by row.
        public static double[] LayerMeans(IList<byte[]> textures)
        {
            var o = new double[textures.Count * 3];
            for (int t = 0; t < textures.Count; t++)
            {
                var im = Decode(textures[t]);
                double s0 = 0, s1 = 0, s2 = 0; int n = im.w * im.h; var d = im.data;
                for (int i = 0, p = 0; i < n; i++, p += im.c) { s0 += d[p] / 255.0; s1 += d[p + 1] / 255.0; s2 += d[p + 2] / 255.0; }
                o[t * 3] = s0 / n; o[t * 3 + 1] = s1 / n; o[t * 3 + 2] = s2 / n;
            }
            return o;
        }

        /// client_index(n_src, S): the nearest source index per client texel. The client maps uv 0..1 over the HF pixel centres
        /// (x = -S/2 .. S/2), texel j centre x = -S/2 + (j + 0.5) S / G; the terrain stage grids span 1024 ft.
        public static int[] ClientIndex(int nSrc, double S = 1023.0)
        {
            var o = new int[G]; double res = 1024.0 / nSrc;
            for (int j = 0; j < G; j++)
            {
                double x = -S / 2 + (j + 0.5) * S / G;
                double v = UxNp.Round((x + 512.0) / res - 0.5);
                o[j] = v < 0 ? 0 : v > nSrc - 1 ? nSrc - 1 : (int)v;
            }
            return o;
        }
        /// on_client(_png(path)): a square RGB picture on the client grid as float64 / 255 (G x G x 3, row-major).
        public static double[] OnClientRgb(UxImage im, double S = 1023.0)
        {
            UxBin.Require(im.c == 3 && im.w == im.h, "on_client: a square RGB picture was expected");
            var ix = ClientIndex(im.h, S); var o = new double[G * G * 3];
            for (int r = 0; r < G; r++)
                for (int c = 0; c < G; c++)
                {
                    int s = (ix[r] * im.w + ix[c]) * 3, d = (r * G + c) * 3;
                    o[d] = im.data[s] / 255.0; o[d + 1] = im.data[s + 1] / 255.0; o[d + 2] = im.data[s + 2] / 255.0;
                }
            return o;
        }

        // ---- ux_verify_race.load_track / race_sections / corridor_points, as far as corridor_mask needs them
        sealed class Node { public double[] c, OL, OR; }
        /// np.round(np.linspace(-1, 1, 41), 3).
        static double[] Fractions()
        {
            var f = new double[41]; double step = 2.0 / 40;
            for (int i = 0; i < 41; i++) { double y = i == 40 ? 1.0 : i * step + -1.0; f[i] = UxNp.Round(y * 1000.0) / 1000.0; }
            return f;
        }
        /// corridor_mask(tdf_bytes, S, n): the race corridor (every race section at 41 lateral fractions of the outer edges, 0.5 ft
        /// steps) on the n-grid, closed (ndimage.binary_closing, 3 iterations of the 4-connected structure, border 0).
        public static bool[] CorridorMask(byte[] tdf, double S, int n = G)
        {
            var sp = UxTdf07.Split(tdf);
            UxTdf07.Structure(sp.items, out _, out var secs);
            int ns = secs.Count;
            var kind = new uint[ns]; var next = new List<int>[ns]; var nodes = new List<Node>[ns];
            for (int si = 0; si < ns; si++)
            {
                byte[] k = UxTdf07.First(secs[si].fields, 17);
                kind[si] = k != null ? UxTdf07.U32(k) : 0;
                next[si] = new List<int>();
                foreach (uint x in UxTdf07.Links(secs[si], 14)) if (x < ns) next[si].Add((int)x);
                nodes[si] = new List<Node>();
                foreach (var nd in secs[si].nodes)
                {
                    var nf = UxTdf07.NodeDict(nd.GetRange(1, nd.Count - 1));
                    double[] c = UxTdf07.Vec3(UxTdf07.Field(nf, 21)), L = UxTdf07.Vec3(UxTdf07.Field(nf, 22)), R = UxTdf07.Vec3(UxTdf07.Field(nf, 23));
                    nodes[si].Add(new Node { c = c, OL = nf.TryGetValue(44, out var ol) ? UxTdf07.Vec3(ol) : L, OR = nf.TryGetValue(45, out var or) ? UxTdf07.Vec3(or) : R });
                }
            }
            // race_sections: the chutes (kind bit 4) and everything their next links reach
            var race = new List<int>(); var todo = new List<int>();
            for (int si = 0; si < ns; si++) if ((kind[si] & 4) != 0) todo.Add(si);
            for (int q = 0; q < todo.Count; q++)
            {
                int s = todo[q];
                if (race.Contains(s)) continue;
                race.Add(s);
                foreach (int x in next[s]) if (!race.Contains(x)) todo.Add(x);
            }
            double cs = S / n; const double step = 0.5;
            var t = new bool[n * n];
            void Mark(double x, double z)
            {
                double cc = Math.Floor((x + S / 2) / cs), rr = Math.Floor((S / 2 - z) / cs);
                if (cc >= 0 && cc < n && rr >= 0 && rr < n) t[(int)rr * n + (int)cc] = true;
            }
            var fr = Fractions();
            foreach (int s in race)
            {
                var N = nodes[s];
                if (N.Count < 2) continue;
                var px = new double[N.Count]; var pz = new double[N.Count];
                foreach (double f in fr)
                {
                    for (int i = 0; i < N.Count; i++)
                    {   // lateral_point(n, f, outer)
                        var c = N[i].c;
                        if (f >= 0) { px[i] = c[0] + (N[i].OR[0] - c[0]) * f; pz[i] = c[2] + (N[i].OR[2] - c[2]) * f; }
                        else { px[i] = c[0] + (N[i].OL[0] - c[0]) * -f; pz[i] = c[2] + (N[i].OL[2] - c[2]) * -f; }
                    }
                    for (int i = 0; i + 1 < N.Count; i++)
                    {   // dense(P, step)
                        double dx = px[i + 1] - px[i], dz = pz[i + 1] - pz[i];
                        int k = Math.Max(1, (int)Math.Ceiling(Hypot(dx, dz) / step));
                        for (int j = 0; j < k; j++) { double u = (double)j / k; Mark(px[i] + u * dx, pz[i] + u * dz); }
                    }
                    Mark(px[N.Count - 1], pz[N.Count - 1]);
                }
            }
            for (int i = 0; i < 3; i++) t = Morph(t, n, n, true);
            for (int i = 0; i < 3; i++) t = Morph(t, n, n, false);
            return t;
        }
        /// One binary dilation / erosion with the 4-connected structure, border value 0.
        static bool[] Morph(bool[] a, int h, int w, bool dilate)
        {
            var o = new bool[h * w];
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++)
                {
                    int i = r * w + c;
                    bool up = r > 0 && a[i - w], dn = r + 1 < h && a[i + w], lf = c > 0 && a[i - 1], rt = c + 1 < w && a[i + 1];
                    o[i] = dilate ? a[i] || up || dn || lf || rt : a[i] && up && dn && lf && rt;
                }
            return o;
        }

        /// Squared distance (in cells) of every element to the nearest element where mask is false (scipy's distance_transform_edt is
        /// the square root of it); 0 where mask is false.
        static int[] Edt2(bool[] mask, int n)
        {
            UxNd.DistanceTransformEdt(mask, n, n, out int[] row, out int[] col);
            var o = new int[n * n];
            for (int r = 0; r < n; r++)
                for (int c = 0; c < n; c++)
                {
                    int i = r * n + c;
                    if (row[i] < 0) throw new UxException("distance transform: the mask has no background element");
                    int dr = row[i] - r, dc = col[i] - c;
                    o[i] = dr * dr + dc * dc;
                }
            return o;
        }

        /// np.interp(x, xp, fp) with the slopes numpy precomputes when there are more samples than points.
        static double Interp(double x, double[] xp, double[] fp, double[] slopes)
        {
            int n = xp.Length;
            if (x > xp[n - 1]) return fp[n - 1];
            if (x < xp[0]) return fp[0];
            int j = 0;
            while (j + 1 < n && xp[j + 1] <= x) j++;                                // xp[j] <= x < xp[j + 1]
            if (j == n - 1 || xp[j] == x) return fp[j];
            return slopes[j] * (x - xp[j]) + fp[j];
        }
        static double[] Slopes(double[] xp, double[] fp)
        {
            var s = new double[xp.Length - 1];
            for (int i = 0; i + 1 < xp.Length; i++) s[i] = (fp[i + 1] - fp[i]) / (xp[i + 1] - xp[i]);
            return s;
        }
        static double Clip(double v, double lo, double hi) => Math.Min(Math.Max(v, lo), hi);
        static byte U8(double v) => (byte)Clip(v * 255 + 0.5, 0, 255);

        /// composite(mask, cl, means) of one texel: the client's near albedo with each layer at its mean colour
        /// (ReflexTerrain.shader 578-651, no ruts). m0..m2: the mask bytes, line / alpha: the ChromLum rgb and alpha bytes.
        public static void Composite(byte m0, byte m1, byte m2, byte line, byte alpha, double[] means, double[] rgb)
        {
            double w0 = Clip(m0 / 255.0 * MASK_SCALE, 0, 1), w1 = Clip(m1 / 255.0 * MASK_SCALE, 0, 1), w2 = Clip(m2 / 255.0 * MASK_SCALE, 0, 1);
            double a = alpha / 255.0, cl = line / 255.0;
            double f = Clip(2 * a, 0, 1), g = Clip(a - 0.5, 0, 1);
            for (int c = 0; c < 3; c++)
            {
                double det = means[c];
                det = det * (1 - w0) + means[3 + c] * w0;
                det = det * (1 - w1) + means[6 + c] * w1;
                det = det * (1 - w2) + means[9 + c] * w2;
                rgb[c] = (det + 0.25 * cl) * f + g;
            }
        }

        /// What StockLook returns: the three maps (G x G, row-major) and the record.
        public sealed class Maps
        {
            /// DetailMask RGBA (alpha 0), ChromLum RGBA (rgb = the edge line, a = the luminance factor / 2), TopDown RGB.
            public byte[] mask, chromlum, topdown;
            public JObj rec;
            /// Diagnostics for the tests: the corridor / lane / track masks and the footprint on the client grid, the signed distance (ft).
            /// v = the clipped macro brightness ratio (null without the terrain stage's pictures).
            public bool[] corridor, lanes, track; public byte[] footprint; public double[] sd, v;
            /// ... and the exact variation numbers (the record rounds them): the track median, std(log v), the exponent; NaN = none.
            public double median = double.NaN, stdLog = double.NaN, exponent = double.NaN; public long powExact;
        }

        // ================================================================================================ stock_look
        /// stock_look(tdir, tj, tdf_bytes, means, S): tdir = the terrain stage folder of the work store (footprint.npy, detailmask.png,
        /// topdown_occ.png), tj = its terrain.json, tdf = the build's Reflex tdf, means = LayerMeans of the base's four detail textures.
        public static Maps StockLook(UxWork work, string tdir, JObj tj, byte[] tdf, double[] means, double S = 1023.0, Action<string> log = null, int threads = 0,
            CancellationToken cancel = default, bool diagnostics = false)
        {
            UxBin.Require(means != null && means.Length == 12, "stock_look: the mean colours of four layers are needed");
            const int GG = G * G; double cs = S / G;
            var b255 = new double[256]; for (int i = 0; i < 256; i++) b255[i] = i / 255.0;
            // ---- the footprint and the track
            double[] fpSrc = UxEntities.ReadNpy(work.ReadBytes(tdir + "/footprint.npy"), out int[] fshape);
            UxBin.Require(fshape.Length == 2 && fshape[0] == fshape[1], "footprint.npy: a square grid was expected");
            var fix = ClientIndex(fshape[0], S);
            var fp = new byte[GG];
            for (int r = 0; r < G; r++) for (int c = 0; c < G; c++) fp[r * G + c] = (byte)fpSrc[fix[r] * fshape[0] + fix[c]];
            fpSrc = null;
            cancel.ThrowIfCancellationRequested();
            bool[] corr = CorridorMask(tdf, S);
            var ground = tj?.Get("ground") as JObj;
            string ch = null;
            if (UxJson.Get(ground, "tmask", "channel_layers") is JObj chl)
                foreach (var kv in chl.items) if (kv.Value as string == RUTS_TEX) { ch = kv.Key; break; }
            UxImage dm = work.Exists(tdir + "/detailmask.png") ? UxImaging.ReadPngRgb(work.ReadBytes(tdir + "/detailmask.png")) : null;
            int[] dix = null;
            if (dm != null) { UxBin.Require(dm.w == dm.h, "detailmask.png is not square"); dix = ClientIndex(dm.h, S); }
            bool[] lanes = null;
            if (ch != null && dm != null)
            {
                int k = "RGB".IndexOf(ch, StringComparison.Ordinal);
                if (k < 0 || ch.Length != 1) throw new UxException("the ruts channel '" + ch + "' is not R, G or B");
                lanes = new bool[GG];
                for (int r = 0; r < G; r++)
                    for (int c = 0; c < G; c++)
                        lanes[r * G + c] = b255[dm.data[(dix[r] * dm.w + dix[c]) * 3 + k]] > LANE_MIN && fp[r * G + c] == 1;
            }
            var track = new bool[GG]; long corrPx = 0, lanesPx = 0, lanesOut = 0, trackPx = 0;
            for (int i = 0; i < GG; i++)
            {
                bool l = lanes != null && lanes[i];
                track[i] = corr[i] || l;
                if (corr[i]) corrPx++; if (l) { lanesPx++; if (!corr[i]) lanesOut++; }
                if (track[i]) trackPx++;
            }
            if (trackPx == 0 || trackPx == GG) throw new UxException("dirt look: the track region is " + (trackPx == 0 ? "empty" : "the whole grid"));
            cancel.ThrowIfCancellationRequested();
            // ---- signed distance to the track edge (negative inside), the distance outside the footprint: three transforms
            var notTrack = new bool[GG]; var notFoot = new bool[GG]; long foot = 0;
            for (int i = 0; i < GG; i++) { notTrack[i] = !track[i]; notFoot[i] = fp[i] != 1; if (fp[i] == 1) foot++; }
            if (foot == 0) throw new UxException("dirt look: the terrain footprint is empty");
            int[] d2out = null, d2in = null, d2foot = null;
            var po = new ParallelOptions { MaxDegreeOfParallelism = threads > 0 ? threads : -1, CancellationToken = cancel };
            Parallel.Invoke(po, () => d2out = Edt2(notTrack, G), () => d2in = Edt2(track, G), () => d2foot = Edt2(notFoot, G));
            notTrack = null; notFoot = null;
            var sd = new double[GG];
            for (int i = 0; i < GG; i++) sd[i] = Math.Sqrt(d2out[i]) * cs - Math.Sqrt(d2in[i]) * cs;
            d2out = null; d2in = null;
            // ---- Untamed macro brightness x occlusion: topdown_occ / the layer composite ux_terrain made it from
            JObj varRec = null; double[] v = null; double kexp = 0; bool hasVar = false;
            double medOut = double.NaN, slOut = double.NaN;
            var lm = ground?.Get("layer_means") as JObj;
            if (dm != null && work.Exists(tdir + "/topdown_occ.png") && lm != null && lm.Has("base") && lm.Has("dirt_r") && lm.Has("dirt_g") && lm.Has("dirt_b"))
            {
                var occ = UxImaging.ReadPngRgb(work.ReadBytes(tdir + "/topdown_occ.png"));
                UxBin.Require(occ.w == occ.h, "topdown_occ.png is not square");
                var oix = ClientIndex(occ.h, S);
                double[] V3(string key) { var l = UxJson.Arr(lm.Get(key)); return new[] { UxJson.Num(l[0]), UxJson.Num(l[1]), UxJson.Num(l[2]) }; }
                double[] cb = V3("base"), c0 = V3("dirt_r"), c1 = V3("dirt_g"), c2 = V3("dirt_b");
                v = new double[GG]; long onN = 0, innerN = 0;
                Parallel.For(0, G, po, r =>
                {
                    for (int c = 0; c < G; c++)
                    {
                        int so = (oix[r] * occ.w + oix[c]) * 3, sm = (dix[r] * dm.w + dix[c]) * 3;
                        double o = ((b255[occ.data[so]] + b255[occ.data[so + 1]]) + b255[occ.data[so + 2]]) / 3.0;
                        double d0 = b255[dm.data[sm]], d1 = b255[dm.data[sm + 1]], d2 = b255[dm.data[sm + 2]];
                        double x0 = cb[0], x1 = cb[1], x2 = cb[2];
                        x0 = x0 * (1 - d0) + c0[0] * d0; x1 = x1 * (1 - d0) + c0[1] * d0; x2 = x2 * (1 - d0) + c0[2] * d0;
                        x0 = x0 * (1 - d1) + c1[0] * d1; x1 = x1 * (1 - d1) + c1[1] * d1; x2 = x2 * (1 - d1) + c1[2] * d1;
                        x0 = x0 * (1 - d2) + c2[0] * d2; x1 = x1 * (1 - d2) + c2[1] * d2; x2 = x2 * (1 - d2) + c2[2] * d2;
                        v[r * G + c] = (o + 1e-3) / (((x0 + x1) + x2) / 3.0 + 1e-3);    // the ratio
                    }
                });
                for (int i = 0; i < GG; i++) { if (fp[i] == 1) { if (track[i]) onN++; if (sd[i] < -4) innerN++; } }
                double med = 1.0;
                if (onN > 0)
                {
                    var sel = new double[onN]; long q = 0;
                    for (int i = 0; i < GG; i++) if (track[i] && fp[i] == 1) sel[q++] = v[i];
                    med = UxImaging.MedianInPlace(sel);
                }
                double div = Math.Max(med, 1e-3);
                for (int i = 0; i < GG; i++) v[i] = Clip(v[i] / div, VAR_CLIP_LO, VAR_CLIP_HI);
                double sl = 0.0;
                if (innerN > 0)
                {   // np.log(v[inner]).std(): the mean and the mean square deviation by pairwise sums
                    var lg = new double[innerN]; long q = 0;
                    for (int i = 0; i < GG; i++) if (sd[i] < -4 && fp[i] == 1) lg[q++] = v[i];
                    Parallel.For(0, (int)((innerN + 4095) / 4096), po, blk =>
                    {
                        long hi = Math.Min(innerN, (blk + 1) * 4096L);
                        for (long i = blk * 4096L; i < hi; i++) lg[i] = LogExact(lg[i]);
                    });
                    double mean = UxNp.Sum(lg, 0, lg.Length) / innerN;
                    for (long i = 0; i < innerN; i++) { double x = lg[i] - mean; lg[i] = x * x; }
                    sl = Math.Sqrt(UxNp.Sum(lg, 0, lg.Length) / innerN);
                }
                kexp = sl > 1e-6 ? Math.Min(1.0, VAR_TARGET / sl) : 0.0;
                hasVar = true; medOut = med; slOut = sl;
                varRec = new JObj().Set("source", VAR_SOURCE).Set("track_median", UxNp.PyRound(med, 4)).Set("clip", new List<object> { VAR_CLIP_LO, VAR_CLIP_HI })
                    .Set("std_log", UxNp.PyRound(sl, 4)).Set("exponent", UxNp.PyRound(kexp, 4)).Set("target_std_log", VAR_TARGET);
            }
            cancel.ThrowIfCancellationRequested();
            // ---- the maps
            double[][] mcol = new double[3][], mslope = new double[3][];
            for (int k = 0; k < 3; k++) { mcol[k] = new double[PROFILE_SD.Length]; for (int i = 0; i < PROFILE_SD.Length; i++) mcol[k][i] = PROFILE_MASK[i][k]; mslope[k] = Slopes(PROFILE_SD, mcol[k]); }
            double[] aslope = Slopes(PROFILE_SD, PROFILE_A);
            var m8 = new byte[GG * 4]; var c8 = new byte[GG * 4];
            byte outA = U8(OUTSIDE_A), unclampA = U8(UNCLAMP_A);
            byte[] outM = { U8(OUTSIDE_MASK[0]), U8(OUTSIDE_MASK[1]), U8(OUTSIDE_MASK[2]) };
            long exact = 0; bool allExact = ExactPow;
            Parallel.For(0, G, po, () => 0L, (r, state, nExact) =>
            {
                for (int c = 0; c < G; c++)
                {
                    int i = r * G + c; double s = sd[i];
                    // FortDodge's outside values only well outside the terrain footprint
                    bool outside = fp[i] == 0 && Math.Sqrt(d2foot[i]) * cs > OUTSIDE_AFTER_FT;
                    if (outside) { m8[4 * i] = outM[0]; m8[4 * i + 1] = outM[1]; m8[4 * i + 2] = outM[2]; }
                    else for (int k = 0; k < 3; k++) m8[4 * i + k] = U8(Interp(s, PROFILE_SD, mcol[k], mslope[k]));
                    byte ln = 0;
                    if (fp[i] == 1)
                    {
                        double u = (s - LINE_CENTRE_FT) / LINE_SIGMA_FT, arg = -0.5 * (u * u);
                        if (arg >= -7.0) ln = U8(LINE_PEAK * ExpExact(arg));           // below: 0.4 e^-7 x 255 + 0.5 < 1
                    }
                    c8[4 * i] = ln; c8[4 * i + 1] = ln; c8[4 * i + 2] = ln;
                    byte ab;
                    if (fp[i] == 2) ab = unclampA;
                    else if (outside) ab = outA;
                    else
                    {
                        double a = Interp(s, PROFILE_SD, PROFILE_A, aslope);
                        if (hasVar && kexp != 0.0)
                        {
                            double x = v[i];
                            if (kexp == 1.0) a = a * x;
                            else if (allExact) { a = a * PowExact(x, kexp); nExact++; }
                            else
                            {
                                double t = Clip(a * Math.Pow(x, kexp), A_CLIP_LO, A_CLIP_HI) * 255 + 0.5;
                                if (Math.Abs(t - Math.Round(t)) < 1e-6) { a = a * PowExact(x, kexp); nExact++; }      // the byte could hang on pow's last bits
                                else a = a * Math.Pow(x, kexp);
                            }
                        }
                        ab = U8(Clip(a, A_CLIP_LO, A_CLIP_HI));
                    }
                    c8[4 * i + 3] = ab;
                }
                return nExact;
            }, n => Interlocked.Add(ref exact, n));
            d2foot = null;
            cancel.ThrowIfCancellationRequested();
            // ---- TopDown = the near albedo of the maps; the record's luminances
            var td8 = new byte[GG * 3];
            long nTrack = 0, nOff = 0, nOn = 0;
            for (int i = 0; i < GG; i++)
                if (fp[i] == 1) { nOn++; if (track[i]) nTrack++; else if (sd[i] <= 30) nOff++; }
            var lumTrack = new double[nTrack]; var lumOff = new double[nOff]; var fac = new double[nOn];
            long qt = 0, qo = 0, qn = 0; var rgb = new double[3];
            for (int i = 0; i < GG; i++)
            {
                Composite(m8[4 * i], m8[4 * i + 1], m8[4 * i + 2], c8[4 * i], c8[4 * i + 3], means, rgb);
                td8[3 * i] = U8(rgb[0]); td8[3 * i + 1] = U8(rgb[1]); td8[3 * i + 2] = U8(rgb[2]);
                if (fp[i] != 1) continue;
                fac[qn++] = Clip(b255[unchecked((byte)(2 * c8[4 * i + 3]))], 0, 1);              // 2 * uint8 wraps in numpy
                bool tr = track[i];
                if (!tr && !(sd[i] <= 30)) continue;
                long m = tr ? nTrack : nOff;
                // alb[sel] @ LUM (cblas_dgemv on an (m, 3) matrix: UxNp.MatVec3's chains)
                double l = m == 1 ? UxNp.Fma(rgb[2], LUM[2], UxNp.Fma(rgb[1], LUM[1], rgb[0] * LUM[0])) : UxNp.Fma(rgb[2], LUM[2], UxNp.Fma(rgb[0], LUM[0], rgb[1] * LUM[1]));
                if (tr) lumTrack[qt++] = l; else lumOff[qo++] = l;
            }
            if (nOff == 0 || nOn == 0) throw new UxException("dirt look: no off-track ground on the footprint");
            var lmr = new List<object>();
            for (int t = 0; t < 4; t++) lmr.Add(new List<object> { UxNp.PyRound(means[t * 3], 4), UxNp.PyRound(means[t * 3 + 1], 4), UxNp.PyRound(means[t * 3 + 2], 4) });
            long f0 = 0, f1 = 0, f2 = 0; for (int i = 0; i < GG; i++) { if (fp[i] == 0) f0++; else if (fp[i] == 1) f1++; else if (fp[i] == 2) f2++; }
            var rec = new JObj().Set("look", "stock").Set("grid", (long)G).Set("S_ft", S).Set("profile", PROFILE_TEXT)
                .Set("track", new JObj().Set("corridor_px", corrPx).Set("lanes_channel", ch).Set("lanes_px", lanesPx).Set("lanes_outside_corridor_px", lanesOut).Set("track_px", trackPx))
                .Set("line", new JObj().Set("peak", LINE_PEAK).Set("centre_ft", LINE_CENTRE_FT).Set("sigma_ft", LINE_SIGMA_FT))
                .Set("variation", varRec).Set("layer_means", lmr)
                .Set("footprint_px", new JObj().Set("0", f0).Set("1", f1).Set("2", f2))
                .Set("near_lum_track", nTrack > 0 ? (object)UxNp.PyRound(UxNp.Sum(lumTrack, 0, lumTrack.Length) / nTrack, 4) : null)
                .Set("near_lum_offtrack", UxNp.PyRound(UxNp.Sum(lumOff, 0, lumOff.Length) / nOff, 4))
                .Set("chromlum_factor_footprint", UxNp.PyRound(UxNp.Sum(fac, 0, fac.Length) / nOn, 4));
            string N(long n) => n.ToString(System.Globalization.CultureInfo.InvariantCulture);
            log?.Invoke("dirt look stock: track " + N(trackPx) + " px (corridor " + N(corrPx) + ", lanes " + (ch ?? "None") + " " + N(lanesPx) + "), near lum track " +
                UxJson.Dumps(rec.Get("near_lum_track"), -1) + " off-track " + UxJson.Dumps(rec.Get("near_lum_offtrack"), -1) + ", ChromLum factor " +
                UxJson.Dumps(rec.Get("chromlum_factor_footprint"), -1) + (varRec != null ? ", variation exponent " + UxJson.Dumps(varRec.Get("exponent"), -1) : ""));
            var res = new Maps { mask = m8, chromlum = c8, topdown = td8, rec = rec, median = medOut, stdLog = slOut, exponent = hasVar ? kexp : double.NaN, powExact = exact };
            if (diagnostics) { res.corridor = corr; res.lanes = lanes; res.track = track; res.footprint = fp; res.sd = sd; res.v = v; }
            return res;
        }
    }
}
