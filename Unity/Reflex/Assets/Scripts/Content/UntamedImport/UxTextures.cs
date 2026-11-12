// MX vs ATV Untamed converter, stage "textures" (S2): the port of the Python reference converter's ux_textures.py. The closure textures of a level
// (work/<Level>/level.json, closure.texture) -> Reflex PC v4 .texture files and textures.json in the work store's textures/ folder
// (textures-max<N>/ with a top-mip limit), byte for byte what the Python stage writes:
//   convert   DXT1 / DXT5 / LIN_*: blocks copied; A8R8G8B8: bytes; DXN -> G16R16F (nearest float16); half-float cubes: copied, kind 2;
//             a greyscale logo stored as DXN -> A8R8G8B8 grey; a mip chain that stops early gets the missing lower mips (2x2 box filter
//             of the level above, re-encoded: DXT1 by Bc1Encode, DXT5 by Pillow's encoder (UxPil.BcEncode) or Bc3Encode)
//   resample  non-power-of-two A8R8G8B8: linear resample of level 0 to the nearest power of two, mips rebuilt by box filter
//   replace   the jumbotron render texture: no file, materials use Editor_Default_Black
//   template  Editor_Default* names the templates hold: no file
//   (rename)  a name a template package also holds is converted as UX_<name>
// and the functions of the module that other stages call: ConvertTexture / CheckTexture / PcLayoutProblems (the builder's extra textures),
// GreyDxnFacts / GenReference / GeneratedTexture / PcRgba (materials), Bc1Encode / PcDecode (screens).
// Not ported: the contact sheet (sheet.png), --names (a given name list), the stock layout self test, full_set_pc_bytes.
//
// What decides bytes here (core/plan/UNTAMED_CONVERT.md, "numpy rules"): everything is float64 element-wise arithmetic, np.round (ties to
// even) and float16 rounding. Box filters add in numpy's order ((top left + top right) + bottom left) + bottom right. Means are numpy's:
// the pairwise sum over a contiguous array or a strided vector; a full reduction of the [..., :3] view of an (h, w, 4) array is buffered
// by numpy's iterator in pieces of 8190 elements (the largest multiple of 3 below its 8192 element buffer), each summed pairwise and the
// pieces added in order (measured on numpy 2.5.4, the reference's).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of ux_textures.convert (ux_build_track: tex_max = --tex-max, everything else the defaults).
    public sealed class UxTexturesOptions
    {
        /// --tex-max N: top mips are dropped while the top mip is larger than N px; 0 = no limit (the 12 current builds).
        public int texMax;
        /// --npot nearest | floor: how a non-power-of-two side is rounded.
        public string npot = "nearest";
        /// check=True: every written file is read back and compared with the source (the 'check' record, the dxn totals).
        public bool check = true;
        /// The builder's rule (BuildError when the manifest has problems): throw a UxException after textures.json is written.
        public bool throwOnProblems = true;
        /// Worker threads (0 = one per processor). The outputs do not depend on it.
        public int threads;
        /// The texture names of the template packages ('sx', 'fr'); null = ctx.stock.templateNames.
        public IDictionary<string, UxTemplateNames> templateNames;
    }

    /// A float64 image (h, w, c), row-major.
    public sealed class UxFImage
    {
        public double[] d; public int h, w, c;
        public UxFImage(double[] d, int h, int w, int c) { this.d = d; this.h = h; this.w = w; this.c = c; }
        /// img.astype(np.float64).
        public static UxFImage Of(UxImage img)
        {
            var d = new double[img.data.Length];
            for (int i = 0; i < d.Length; i++) d[i] = img.data[i];
            return new UxFImage(d, img.h, img.w, img.c);
        }
        /// np.clip(np.round(a), 0, 255).astype(np.uint8).
        public UxImage ToU8()
        {
            var o = new byte[d.Length];
            for (int i = 0; i < o.Length; i++) { double v = UxNp.Round(d[i]); o[i] = (byte)(v < 0 ? 0 : v > 255 ? 255 : v); }
            return new UxImage(o, h, w, c);
        }
    }

    /// One entry of convert_texture's `expect`: the exact bytes meant to be written, or ('dxn', s, f, m) for the G16R16F checks.
    public sealed class UxTexExpect
    {
        public byte[] raw; public bool dxn; public int s, f, m;
        public UxTexExpect(byte[] raw) { this.raw = raw; }
        public UxTexExpect(int s, int f, int m) { dxn = true; this.s = s; this.f = f; this.m = m; }
    }

    /// convert_texture's result: (record, PC bytes or null, expect).
    public sealed class UxTexConverted
    {
        public JObj rec; public byte[] pc; public List<UxTexExpect> expect;
    }

    /// check_texture's stats.
    public sealed class UxTexCheckStats
    {
        public long entries, dxn_not_nearest; public double dxn_max_err;
    }

    /// One entry of a PC texture: (surface, face, mip, w, h, format name, bytes).
    public sealed class UxPcEntry
    {
        public int s, f, m, w, h; public string fmt; public byte[] raw;
    }

    public static class UxTextures
    {
        public const string SCHEMA = "ux_textures/1";
        /// The 'tool' of the manifest, and the versions of the reference modules this file reproduces (tool_md5, uxtex_md5): constants,
        /// not measurements (checked against the reference's files).
        public const string TOOL = "ux_textures.py";
        public const string REFERENCE_TOOL_MD5 = "7bf36a07fe35fb003f5926ad3e6a35f9";
        public const string REFERENCE_UXTEX_MD5 = "77b39dabdbc113f0ba5e53d518bc1a36";
        public const double F16_BOUND = 1.0 / 4096;                // float16 half-ULP in [0.5, 1)
        public const string JUMBO_REPLACEMENT = "Editor_Default_Black";
        public const double GEN_RGB_TOL = 40.0, GEN_ALPHA_TOL = 64.0, GEN_ALPHA1_TOL = 16.0;
        public const double GREY_DXN_MAX_RG = 1.0, GREY_DXN_MIN_STD = 1.0;
        public const double GEN_BAKE_TOL = 12.0;
        public const int GEN_SMALL = 4, GEN_BAKE_MIN = 128;
        /// numpy's iterator buffer (elements) for a reduction it has to buffer.
        const int NpBuffer = 8192;

        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
        static string S(long v) => v.ToString(Inv);

        // ------------------------------------------------------------------------------------------- small helpers
        static bool Pot(long x) => x > 0 && (x & (x - 1)) == 0;
        static int BitLength(long x) { int n = 0; while (x > 0) { n++; x >>= 1; } return n; }

        /// _pot_round(x, mode): x, or the power of two below (floor) / nearest on a log scale.
        public static int PotRound(int x, string mode)
        {
            if (Pot(x)) return x;
            int lo = 1 << (BitLength(x) - 1);
            if (mode == "floor") return lo;
            int hi = lo * 2;
            return Math.Log(x, 2) - Math.Log(lo, 2) < Math.Log(hi, 2) - Math.Log(x, 2) ? lo : hi;
        }

        /// out_dir_for(level, tex_max), relative to the work store.
        public static string OutDirFor(int texMax = 0) => texMax == 0 ? "textures" : "textures-max" + S(texMax);

        /// template_clash(name): {kind: the template's spelling or null}, kinds sorted.
        public static JObj TemplateClash(string name, IDictionary<string, UxTemplateNames> templates)
        {
            if (templates == null) throw new UxException("template_clash: no template name tables");
            var kinds = new List<string>(templates.Keys); kinds.Sort(string.CompareOrdinal);
            var o = new JObj(); string lo = UxCommon.PyLower(name);
            foreach (string k in kinds) o.items.Add(new KeyValuePair<string, object>(k, templates[k].Of("texture").TryGetValue(lo, out string n) ? n : null));
            return o;
        }
        static bool Truthy(object v) => v is string s && s.Length > 0;
        static bool AllValues(JObj d) { foreach (var kv in d.items) if (!Truthy(kv.Value)) return false; return true; }
        static bool AnyValue(JObj d) { foreach (var kv in d.items) if (Truthy(kv.Value)) return true; return false; }

        /// '%.<digits>f' % v (correctly rounded, as Python's).
        static string Fixed(double v, int digits)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            string r = PyFormat.Repr(UxNp.PyRound(v, digits));
            if (r.IndexOf('e') >= 0) return v.ToString("F" + S(digits), Inv);
            int dot = r.IndexOf('.');
            string ip = dot < 0 ? r : r.Substring(0, dot), fp = dot < 0 ? "" : r.Substring(dot + 1);
            if (fp.Length > digits) fp = fp.Substring(0, digits);
            if (digits == 0) return ip;
            return ip + "." + fp.PadRight(digits, '0');
        }
        /// repr of a Python list of numbers / lists (long -> int, double -> float).
        static string PyList(System.Collections.IEnumerable items)
        {
            var sb = new StringBuilder("["); bool first = true;
            foreach (object v in items)
            {
                if (!first) sb.Append(", ");
                first = false;
                if (v is double d) sb.Append(PyFormat.Repr(d));
                else if (v is string s) sb.Append(s);
                else if (v is System.Collections.IEnumerable e) sb.Append(PyList(e));
                else sb.Append(System.Convert.ToString(v, Inv));
            }
            return sb.Append("]").ToString();
        }

        // ------------------------------------------------------------------------------------------- PC helpers
        /// PC_FMT_NAME.get(fmt): the name of the 4 format bytes of a PC surface record, or null.
        public static string PcFmtName(byte[] fmt, int at = 0)
        {
            if (fmt == null || at + 4 > fmt.Length) return null;
            uint v = UxBin.U32LE(fmt, at);
            switch (v)
            {
                case 0x31545844: return "DXT1"; case 0x35545844: return "DXT5"; case 0x33545844: return "DXT3"; case 0x4E545844: return "DXTN";
                case 0x15: return "A8R8G8B8"; case 0x71: return "A16B16G16R16F"; case 0x70: return "G16R16F"; case 0x72: return "R32F";
                case 0x32: return "L8"; case 0x6F: return "R16F";
            }
            return null;
        }
        /// PC_FMT_UNIT[name]: (4 for a block format else 1, bytes per block or pixel).
        public static (int b, int u) PcFmtUnit(string name)
        {
            switch (name)
            {
                case "DXT1": return (4, 8); case "DXT5": case "DXT3": case "DXTN": return (4, 16);
                case "A8R8G8B8": case "G16R16F": case "R32F": return (1, 4); case "A16B16G16R16F": return (1, 8); case "L8": return (1, 1); case "R16F": return (1, 2);
            }
            throw new UxException("PC format " + name);
        }
        /// pc_level_size(fmt_name, w, h).
        public static long PcLevelSize(string name, long w, long h)
        {
            var (b, u) = PcFmtUnit(name);
            return b == 4 ? Math.Max(1, (w + 3) / 4) * Math.Max(1, (h + 3) / 4) * u : w * h * u;
        }
        /// pc_pitch(fmt_name, w).
        public static long PcPitch(string name, long w)
        {
            var (b, u) = PcFmtUnit(name);
            return b == 4 ? Math.Max(1, (w + 3) / 4) * u : w * u;
        }

        /// pc_layout_problems(data, strict): a Reflex PC v4 texture against the stock layout rules (header, one record per surface, a
        /// {size, pitch} table, the pixel data right after the table, every entry the level size of its format, the file ends at the last
        /// entry; strict: kind 0 or 2 and flags 0).
        public static List<string> PcLayoutProblems(byte[] data, bool strict = true)
        {
            var p = new List<string>();
            if (data.Length < 0x28) { p.Add("file shorter than the header"); return p; }
            uint magic = UxBin.U32LE(data, 0), ver = UxBin.U32LE(data, 4), z8 = UxBin.U32LE(data, 8), nsurf = UxBin.U32LE(data, 12), nentry = UxBin.U32LE(data, 16),
                kind = UxBin.U32LE(data, 20), flags = UxBin.U32LE(data, 24);
            if (magic != UxTex.MAGIC) p.Add("magic " + magic.ToString("x8", Inv));
            if (ver != 4) p.Add("version " + S(ver));
            if (z8 != 0) p.Add("word 0x08 = " + S(z8));
            bool any = false; for (int i = 0x1C; i < 0x28; i++) any |= data[i] != 0;
            if (any) p.Add("bytes 0x1C..0x27 not zero");
            if (!(kind == 0 || kind == 2 || (!strict && kind == 1))) p.Add("kind " + S(kind));
            if (strict && flags != 0) p.Add("flags 0x" + flags.ToString("x", Inv));
            if (nsurf < 1) p.Add("nsurf " + S(nsurf));
            if (p.Count > 0) return p;
            long faces = kind == 2 ? 6 : 1;
            if (0x28 + 16L * nsurf > data.Length) { p.Add("surface records run past the file end"); return p; }     // Python: struct.error
            var recs = new List<(long mips, long w, long h, string fn, int at)>();
            long sum = 0;
            for (int i = 0; i < (int)nsurf; i++)
            {
                long mips = UxBin.U32LE(data, 0x28 + 16 * i), w = UxBin.U32LE(data, 0x2C + 16 * i), h = UxBin.U32LE(data, 0x30 + 16 * i);
                recs.Add((mips, w, h, PcFmtName(data, 0x34 + 16 * i), 0x34 + 16 * i)); sum += mips;
            }
            if (nentry != sum * faces) p.Add("nentry " + S(nentry) + " != sum(mips) x faces " + S(sum * faces));
            long tp = 0x28 + 16L * nsurf, off = tp + 8L * nentry;
            if (off > data.Length) { p.Add("table runs past the file end"); return p; }
            long k = 0;
            for (int s = 0; s < recs.Count; s++)
            {
                var (mips, w, h, fn, at) = recs[s];
                if (fn == null) { p.Add("surface " + S(s) + ": unknown PC format " + UxBin.Hex(UxBin.Slice(data, at, 4))); return p; }
                for (long f = 0; f < faces; f++)
                    for (long m = 0; m < mips; m++)
                    {
                        if (k >= nentry) { p.Add("table too short"); return p; }
                        long sz = UxBin.U32LE(data, (int)(tp + 8 * k)), pitch = UxBin.U32LE(data, (int)(tp + 8 * k + 4)); k++;
                        long lw = Math.Max(1, m < 63 ? w >> (int)m : 0), lh = Math.Max(1, m < 63 ? h >> (int)m : 0);
                        if (kind == 1 && sz == 0) continue;                   // render target: no pixel data
                        if (sz != PcLevelSize(fn, lw, lh))
                            p.Add("s" + S(s) + " f" + S(f) + " m" + S(m) + ": size " + S(sz) + " != " + S(PcLevelSize(fn, lw, lh)) + " (" + fn + " " + S(lw) + "x" + S(lh) + ")");
                        if (pitch != PcPitch(fn, lw)) p.Add("s" + S(s) + " f" + S(f) + " m" + S(m) + ": pitch " + S(pitch) + " != " + S(PcPitch(fn, lw)));
                        off += sz;
                    }
            }
            if (off != data.Length) p.Add("data ends at " + S(off) + ", file is " + S(data.Length) + " bytes");
            return p;
        }

        /// _pc_entries(data): the PC texture and its entries (surface, face, mip, w, h, format name, bytes).
        public static List<UxPcEntry> PcEntries(byte[] data, out UxPcTexture t)
        {
            t = new UxPcTexture(data);
            int faces = t.kind == 2 ? 6 : 1; var o = new List<UxPcEntry>(); int k = 0;
            for (int s = 0; s < t.recs.Count; s++)
            {
                var (mips, w, h, fmt) = t.recs[s];
                string fn = PcFmtName(fmt) ?? UxBin.Hex(fmt);
                for (int f = 0; f < faces; f++)
                    for (int m = 0; m < mips; m++)
                    {
                        if (k >= t.entries.Count) throw new UxException("PC texture: " + t.entries.Count + " entries for " + S((long)mips * faces) + " levels");     // IndexError
                        o.Add(new UxPcEntry { s = s, f = f, m = m, w = Math.Max(1, m < 31 ? w >> m : 0), h = Math.Max(1, m < 31 ? h >> m : 0), fmt = fn, raw = t.entries[k] });
                        k++;
                    }
            }
            return o;
        }

        static float F(double v) => (float)v;

        /// pc_decode(fmt, w, h, raw): one PC level -> uint8 RGBA (h, w, 4) (G16R16F: R, G and the reconstructed Z; half floats: tone
        /// mapped through pow, as portable as the C library's; R32F: grey, min..max).
        public static UxImage PcDecode(string fmt, int w, int h, byte[] raw)
        {
            if (fmt == "DXT1" || fmt == "DXT5" || fmt == "DXT3" || fmt == "DXTN")
            {
                int hb = Math.Max(1, (h + 3) / 4), wb = Math.Max(1, (w + 3) / 4);
                UxBin.Require(raw.Length % (hb * wb) == 0, "pc_decode: " + raw.Length + " bytes for " + wb + "x" + hb + " blocks");
                var img = UxTex.DecodeBlocks(new UxBlocks(raw, hb, wb, raw.Length / (hb * wb)), fmt == "DXT1" ? "DXT1" : fmt == "DXT3" ? "DXT2_3" : "DXT4_5");
                UxBin.Require(img.h >= h && img.w >= w, "pc_decode: level larger than its blocks");
                return img.Crop(h, w);
            }
            if (fmt == "A8R8G8B8")
            {
                UxBin.Require(raw.Length == (long)h * w * 4, "pc_decode: " + raw.Length + " bytes for " + w + "x" + h + " A8R8G8B8");
                var o = new byte[raw.Length];
                for (int i = 0; i < o.Length; i += 4) { o[i] = raw[i + 2]; o[i + 1] = raw[i + 1]; o[i + 2] = raw[i]; o[i + 3] = raw[i + 3]; }
                return new UxImage(o, h, w, 4);
            }
            if (fmt == "G16R16F")
            {
                UxBin.Require(raw.Length == (long)h * w * 4, "pc_decode: " + raw.Length + " bytes for " + w + "x" + h + " G16R16F");
                var o = new byte[h * w * 4];
                for (int i = 0; i < h * w; i++)
                {
                    float r = UxNp.HalfToFloat(UxBin.U16LE(raw, 4 * i)), g = UxNp.HalfToFloat(UxBin.U16LE(raw, 4 * i + 2));
                    float x = F(F(r * 2f) - 1f), y = F(F(g * 2f) - 1f);
                    float q = F(F(1f - F(x * x)) - F(y * y));
                    q = q < 0 ? 0 : q > 1 ? 1 : q;
                    float z = F(F(F(Math.Sqrt(q)) * 0.5f) + 0.5f);
                    o[4 * i] = U8(r); o[4 * i + 1] = U8(g); o[4 * i + 2] = U8(z); o[4 * i + 3] = 255;
                }
                return new UxImage(o, h, w, 4);
            }
            if (fmt == "A16B16G16R16F")
            {
                UxBin.Require(raw.Length == (long)h * w * 8, "pc_decode: " + raw.Length + " bytes for " + w + "x" + h + " A16B16G16R16F");
                var o = new byte[h * w * 4]; float gm = F(1 / 2.2);
                for (int i = 0; i < h * w; i++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        float v = UxNp.HalfToFloat(UxBin.U16LE(raw, 8 * i + 2 * c));
                        v = F(v / F(1f + v)); v = v < 0 ? 0 : v > 1 ? 1 : v;
                        v = F(Math.Pow(v, gm));
                        o[4 * i + c] = Trunc8(F(UxNp.Round(F(v * 255f))));
                    }
                    float a = UxNp.HalfToFloat(UxBin.U16LE(raw, 8 * i + 6)); a = a < 0 ? 0 : a > 1 ? 1 : a;
                    o[4 * i + 3] = Trunc8(F(a * 255f));
                }
                return new UxImage(o, h, w, 4);
            }
            if (fmt == "R32F")
            {
                UxBin.Require(raw.Length == (long)h * w * 4 && raw.Length > 0, "pc_decode: " + raw.Length + " bytes for " + w + "x" + h + " R32F");
                float mn = UxBin.F32LE(raw, 0), mx = mn;
                for (int i = 1; i < h * w; i++) { float v = UxBin.F32LE(raw, 4 * i); if (v < mn) mn = v; if (v > mx) mx = v; }
                float div = F(Math.Max(1e-9, (double)F(mx - mn)));
                var o = new byte[h * w * 4];
                for (int i = 0; i < h * w; i++)
                {
                    float v = F(F(UxBin.F32LE(raw, 4 * i) - mn) / div); v = v < 0 ? 0 : v > 1 ? 1 : v;
                    byte g = Trunc8(F(UxNp.Round(F(v * 255f))));
                    o[4 * i] = g; o[4 * i + 1] = g; o[4 * i + 2] = g; o[4 * i + 3] = 255;
                }
                return new UxImage(o, h, w, 4);
            }
            throw new UxException("pc_decode: " + fmt);
        }
        /// np.round(np.clip(v, 0, 1) * 255).astype(np.uint8) of a float32.
        static byte U8(float v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return Trunc8(F(UxNp.Round(F(v * 255f)))); }
        static byte Trunc8(float v) => float.IsNaN(v) ? (byte)0 : unchecked((byte)(int)v);

        /// pc_rgba(data, entry).
        public static UxImage PcRgba(byte[] data, int entry = 0)
        {
            var ents = PcEntries(data, out _);
            UxBin.Require(entry >= 0 && entry < ents.Count, "pc_rgba: no entry " + entry);
            var e = ents[entry];
            return PcDecode(e.fmt, e.w, e.h, e.raw);
        }

        // ------------------------------------------------------------------------------------------- resampling
        /// _resample_linear(img, nw, nh): separable linear resample with pixel-centre alignment (the width first, then the height).
        public static UxFImage ResampleLinear(UxFImage img, int nw, int nh)
        {
            int h = img.h, w = img.w, c = img.c; double[] a = img.d;
            if (w != nw)
            {
                Taps(w, nw, out int[] i0, out int[] i1, out double[] t);
                var o = new double[h * nw * c];
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < nw; x++)
                    {
                        double tt = t[x], u = 1 - tt; int p0 = (y * w + i0[x]) * c, p1 = (y * w + i1[x]) * c, q = (y * nw + x) * c;
                        for (int k = 0; k < c; k++) { double l = a[p0 + k] * u, r = a[p1 + k] * tt; o[q + k] = l + r; }
                    }
                a = o; w = nw;
            }
            if (h != nh)
            {
                Taps(h, nh, out int[] i0, out int[] i1, out double[] t);
                var o = new double[nh * w * c];
                for (int y = 0; y < nh; y++)
                {
                    double tt = t[y], u = 1 - tt; int r0 = i0[y] * w * c, r1 = i1[y] * w * c, q = y * w * c;
                    for (int k = 0; k < w * c; k++) { double l = a[r0 + k] * u, r = a[r1 + k] * tt; o[q + k] = l + r; }
                }
                a = o; h = nh;
            }
            return new UxFImage(a, h, w, c);
        }
        static void Taps(int nOld, int nNew, out int[] i0, out int[] i1, out double[] t)
        {
            i0 = new int[nNew]; i1 = new int[nNew]; t = new double[nNew];
            for (int i = 0; i < nNew; i++)
            {
                double x = (i + 0.5) * nOld; x = x / nNew; x = x - 0.5;
                x = UxNp.Clip(x, 0, nOld - 1);
                double f = Math.Floor(x);
                i0[i] = (int)f; i1[i] = Math.Min(i0[i] + 1, nOld - 1); t[i] = x - f;
            }
        }

        /// _box_half(img): the 2x2 box filter to the next mip (a 1 px side stays). numpy adds the four texels in the order top left,
        /// top right, bottom left, bottom right.
        public static UxFImage BoxHalf(UxFImage img)
        {
            int h = img.h, w = img.w, c = img.c; double[] a = img.d;
            int nh = Math.Max(1, h / 2), nw = Math.Max(1, w / 2);
            if (h > 1 && w > 1)
            {
                var o = new double[nh * nw * c];
                for (int y = 0; y < nh; y++)
                    for (int x = 0; x < nw; x++)
                    {
                        int p = (2 * y * w + 2 * x) * c, q = (y * nw + x) * c;
                        for (int k = 0; k < c; k++)
                        {
                            double s = a[p + k]; s += a[p + c + k]; s += a[p + w * c + k]; s += a[p + w * c + c + k];
                            o[q + k] = s / 4;
                        }
                    }
                return new UxFImage(o, nh, nw, c);
            }
            if (h > 1)
            {
                var o = new double[nh * w * c];
                for (int y = 0; y < nh; y++)
                    for (int k = 0; k < w * c; k++) { double s = a[2 * y * w * c + k]; s += a[(2 * y + 1) * w * c + k]; o[y * w * c + k] = s / 2; }
                return new UxFImage(o, nh, w, c);
            }
            if (w > 1)
            {
                var o = new double[h * nw * c];
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < nw; x++)
                        for (int k = 0; k < c; k++) { double s = a[(y * w + 2 * x) * c + k]; s += a[(y * w + 2 * x + 1) * c + k]; o[(y * nw + x) * c + k] = s / 2; }
                return new UxFImage(o, h, nw, c);
            }
            return img;
        }

        // ------------------------------------------------------------------------------------------- BC1 / BC3 range fit
        /// _to565(c).
        static int To565(double r, double g, double b)
        {
            int R = (int)UxNp.Clip(UxNp.Round(r * 31 / 255.0), 0, 31), G = (int)UxNp.Clip(UxNp.Round(g * 63 / 255.0), 0, 63), B = (int)UxNp.Clip(UxNp.Round(b * 31 / 255.0), 0, 31);
            return R << 11 | G << 5 | B;
        }
        /// _from565(v) into p[at .. at + 3).
        static void From565(int v, double[] p, int at)
        {
            int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            p[at] = (r << 3) | (r >> 2); p[at + 1] = (g << 2) | (g >> 4); p[at + 2] = (b << 3) | (b >> 2);
        }

        /// The 16 texels (R, G, B, A as float64) of block (bx, by) of an image padded by edge replication.
        static void Block(UxImage img, int bx, int by, double[] b)
        {
            for (int j = 0; j < 4; j++)
            {
                int y = Math.Min(by * 4 + j, img.h - 1);
                for (int i = 0; i < 4; i++)
                {
                    int x = Math.Min(bx * 4 + i, img.w - 1), p = (y * img.w + x) * 4, q = (j * 4 + i) * 4;
                    b[q] = img.data[p]; b[q + 1] = img.data[p + 1]; b[q + 2] = img.data[p + 2]; b[q + 3] = img.data[p + 3];
                }
            }
        }
        /// np.argmin(((rgb[:, None, :] - pal[None]) ** 2).sum(2), 1) for one texel.
        static int Nearest(double[] b, int q, double[] pal, int n)
        {
            int best = 0; double bd = 0;
            for (int j = 0; j < n; j++)
            {
                double d0 = b[q] - pal[3 * j], d1 = b[q + 1] - pal[3 * j + 1], d2 = b[q + 2] - pal[3 * j + 2];
                double s = d0 * d0; s += d1 * d1; s += d2 * d2;
                if (j == 0 || s < bd) { bd = s; best = j; }
            }
            return best;
        }
        /// The colour endpoints of the texels chosen by `use` (all when none is chosen): (e_hi, e_lo) = _to565(max), _to565(min).
        static void Endpoints(double[] b, bool[] use, out int eHi, out int eLo)
        {
            bool any = false; for (int i = 0; i < 16; i++) any |= use[i];
            double r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0; bool first = true;
            for (int i = 0; i < 16; i++)
            {
                if (any && !use[i]) continue;
                double r = b[4 * i], g = b[4 * i + 1], bl = b[4 * i + 2];
                if (first) { r0 = r1 = r; g0 = g1 = g; b0 = b1 = bl; first = false; continue; }
                if (r > r1) r1 = r; if (r < r0) r0 = r; if (g > g1) g1 = g; if (g < g0) g0 = g; if (bl > b1) b1 = bl; if (bl < b0) b0 = bl;
            }
            eHi = To565(r1, g1, b1); eLo = To565(r0, g0, b0);
        }
        /// The 4-colour palette of (c0 > c1), or the single colour of equal endpoints; returns the palette size.
        static int Palette4(int eHi, int eLo, double[] pal, out int c0, out int c1)
        {
            if (eHi == eLo) { c0 = c1 = eHi; From565(c0, pal, 0); return 1; }
            c0 = Math.Max(eHi, eLo); c1 = Math.Min(eHi, eLo);
            From565(c0, pal, 0); From565(c1, pal, 3);
            for (int k = 0; k < 3; k++) { pal[6 + k] = (2 * pal[k] + pal[3 + k]) / 3; pal[9 + k] = (pal[k] + 2 * pal[3 + k]) / 3; }
            return 4;
        }

        /// bc1_encode(img): uint8 RGBA (h, w, 4) -> DXT1 blocks (range fit; a block with any alpha below 128 uses the 3-colour mode,
        /// index 3 = transparent; edges replicated into partial blocks).
        public static byte[] Bc1Encode(UxImage img)
        {
            UxBin.Require(img.c == 4, "bc1_encode: RGBA expected");
            int hb = Math.Max(1, (img.h + 3) / 4), wb = Math.Max(1, (img.w + 3) / 4);
            var o = new byte[hb * wb * 8]; var b = new double[64]; var pal = new double[12]; var trans = new bool[16]; var opaque = new bool[16];
            for (int by = 0; by < hb; by++)
                for (int bx = 0; bx < wb; bx++)
                {
                    Block(img, bx, by, b);
                    bool anyTrans = false;
                    for (int i = 0; i < 16; i++) { trans[i] = b[4 * i + 3] < 128; opaque[i] = !trans[i]; anyTrans |= trans[i]; }
                    Endpoints(b, opaque, out int eHi, out int eLo);
                    int c0, c1, n;
                    if (anyTrans)
                    {
                        c0 = Math.Min(eHi, eLo); c1 = Math.Max(eHi, eLo);
                        From565(c0, pal, 0); From565(c1, pal, 3);
                        for (int k = 0; k < 3; k++) pal[6 + k] = (pal[k] + pal[3 + k]) / 2;
                        n = 3;
                    }
                    else n = Palette4(eHi, eLo, pal, out c0, out c1);
                    uint bits = 0;
                    for (int i = 0; i < 16; i++) bits |= (uint)(trans[i] ? 3 : Nearest(b, 4 * i, pal, n)) << (2 * i);
                    int at = (by * wb + bx) * 8;
                    UxBin.PutU16(o, at, (ushort)c0); UxBin.PutU16(o, at + 2, (ushort)c1); UxBin.PutU32(o, at + 4, bits);
                }
            return o;
        }

        /// bc3_encode(img): uint8 RGBA (h, w, 4) -> DXT5 blocks (range fit: 8-step alpha, 4-colour block on the texels that are not
        /// fully transparent).
        public static byte[] Bc3Encode(UxImage img)
        {
            UxBin.Require(img.c == 4, "bc3_encode: RGBA expected");
            int hb = Math.Max(1, (img.h + 3) / 4), wb = Math.Max(1, (img.w + 3) / 4);
            var o = new byte[hb * wb * 16]; var b = new double[64]; var pal = new double[12]; var vis = new bool[16]; var apal = new double[8];
            for (int by = 0; by < hb; by++)
                for (int bx = 0; bx < wb; bx++)
                {
                    Block(img, bx, by, b);
                    int at = (by * wb + bx) * 16;
                    double amax = b[3], amin = b[3];
                    for (int i = 1; i < 16; i++) { double a = b[4 * i + 3]; if (a > amax) amax = a; if (a < amin) amin = a; }
                    int a0 = (int)UxNp.Round(amax), a1 = (int)UxNp.Round(amin);
                    o[at] = (byte)a0; o[at + 1] = (byte)a1;
                    if (a0 != a1)
                    {
                        apal[0] = a0; apal[1] = a1;
                        for (int i = 1; i < 7; i++) apal[1 + i] = ((7 - i) * a0 + i * a1) / 7.0;
                        ulong bits = 0;
                        for (int i = 0; i < 16; i++)
                        {
                            int best = 0; double bd = 0, a = b[4 * i + 3];
                            for (int j = 0; j < 8; j++) { double d = Math.Abs(a - apal[j]); if (j == 0 || d < bd) { bd = d; best = j; } }
                            bits |= (ulong)best << (3 * i);
                        }
                        for (int k = 0; k < 6; k++) o[at + 2 + k] = (byte)(bits >> (8 * k));
                    }
                    for (int i = 0; i < 16; i++) vis[i] = b[4 * i + 3] > 0;
                    Endpoints(b, vis, out int eHi, out int eLo);
                    int n = Palette4(eHi, eLo, pal, out int c0, out int c1);
                    uint idx = 0;
                    for (int i = 0; i < 16; i++) idx |= (uint)Nearest(b, 4 * i, pal, n) << (2 * i);
                    UxBin.PutU16(o, at + 8, (ushort)c0); UxBin.PutU16(o, at + 10, (ushort)c1); UxBin.PutU32(o, at + 12, idx);
                }
            return o;
        }

        // ------------------------------------------------------------------------------------------- numpy means
        /// float(a.mean()) of a contiguous array or a strided vector: the pairwise sum / n.
        static double MeanPairwise(double[] a, int n) => UxNp.Sum(a, 0, n) / n;
        /// float(a[..., :3].mean()) of the (h, w, 3) view of an (h, w, 4) array, given its elements in order: numpy buffers that
        /// reduction in pieces of 8190 elements, each summed pairwise, the pieces added in order.
        static double MeanRgbView(double[] a, int n)
        {
            int piece = NpBuffer / 3 * 3; double acc = 0;
            for (int i = 0; i < n; i += piece) acc += UxNp.Sum(a, i, Math.Min(piece, n - i));
            return acc / n;
        }

        /// gen_alpha_error(got, ref, fmt): (error, tolerance) of the V3.1 alpha measure.
        public static (double error, double tol) GenAlphaError(UxFImage got, UxFImage rf, string fmt)
        {
            SameShape(got, rf);
            int n = got.h * got.w; var d = new double[n];
            if (fmt == "DXT1")
            {
                for (int i = 0; i < n; i++) { double r = rf.d[4 * i + 3], da = Math.Abs(got.d[4 * i + 3] - r); d[i] = da - Math.Min(r, 255.0 - r); }
                return (MeanPairwise(d, n), GEN_ALPHA1_TOL);
            }
            for (int i = 0; i < n; i++) d[i] = Math.Abs(got.d[4 * i + 3] - rf.d[4 * i + 3]);
            return (MeanPairwise(d, n), GEN_ALPHA_TOL);
        }
        static void SameShape(UxFImage a, UxFImage b)
            => UxBin.Require(a.h == b.h && a.w == b.w && a.c == 4 && b.c == 4, "shapes (" + a.h + ", " + a.w + ", " + a.c + ") and (" + b.h + ", " + b.w + ", " + b.c + ") differ");

        /// gen_mip_error(raw, fmt, w, h, parent): (mean |RGB| error on the texels opaque in the reference, the alpha error, the decoded
        /// level) of an encoded generated mip against the 2x2 box filter of its decoded parent.
        public static (double er, double ea, UxFImage got) GenMipError(byte[] raw, string fmt, int w, int h, UxFImage parent)
        {
            var got = UxFImage.Of(PcDecode(fmt, w, h, raw)); var rf = BoxHalf(parent);
            SameShape(got, rf);
            int n = got.h * got.w, k = 0; var d = new double[n * 3];
            for (int i = 0; i < n; i++)
                if (rf.d[4 * i + 3] >= 128)
                    for (int c = 0; c < 3; c++) d[k++] = Math.Abs(got.d[4 * i + c] - rf.d[4 * i + c]);
            double er = k > 0 ? MeanPairwise(d, k) : 0.0;
            return (er, GenAlphaError(got, rf, fmt).error, got);
        }
        /// gen_mip_misses(er, ea, fmt).
        public static bool GenMipMisses(double er, double ea, string fmt) => er > GEN_RGB_TOL || ea > (fmt == "DXT1" ? GEN_ALPHA1_TOL : GEN_ALPHA_TOL);

        static int DropCount(int w, int h, int mips, int texMax)
        {
            if (texMax == 0) return 0;
            int k = 0;
            while (k < mips - 1 && Math.Max(Shr(w, k), Shr(h, k)) > texMax) k++;
            return k;
        }
        static int Shr(int v, int n) => n >= 31 ? 0 : v >> n;
        static int Lv(int v, int m) => Math.Max(1, Shr(v, m));

        // ------------------------------------------------------------------------------------------- grey DXN
        /// grey_dxn_facts' result.
        public sealed class GreyDxn
        {
            public bool grey; public double rg, std;
            public JObj ToDict() => new JObj().Set("grey", grey).Set("rg", rg).Set("std", std);
        }
        /// grey_dxn_facts(data): for a single-surface DXN texture, whether its decoded mip 0 has R == G and is not constant (a greyscale
        /// logo stored as DXN); rg = round(mean |R - G|, 4), std = round(std of R, 3). Null for anything else.
        public static GreyDxn GreyDxnFacts(byte[] data)
        {
            UxTexture t;
            try { t = new UxTexture(data); } catch (Exception e) when (!(e is OperationCanceledException)) { return null; }
            if (t.nsurf != 1 || t.faces != 1 || t.Elem(0).name != "DXN") return null;
            return GreyDxnFacts(t);
        }
        static GreyDxn GreyDxnFacts(UxTexture t)
        {
            var img = t.Rgba(0, 0, 0, false); int n = img.h * img.w;
            var a = new double[n];
            for (int i = 0; i < n; i++) a[i] = Math.Abs((double)img.data[4 * i] - img.data[4 * i + 1]);
            double rg = MeanPairwise(a, n);
            for (int i = 0; i < n; i++) a[i] = img.data[4 * i];
            double mean = MeanPairwise(a, n);
            for (int i = 0; i < n; i++) { double x = a[i] - mean; a[i] = x * x; }
            double sd = Math.Sqrt(MeanPairwise(a, n));
            return new GreyDxn { grey = rg < GREY_DXN_MAX_RG && sd > GREY_DXN_MIN_STD, rg = UxNp.PyRound(rg, 4), std = UxNp.PyRound(sd, 3) };
        }

        // ------------------------------------------------------------------------------------------- generated textures
        static double[] Vec(object v, int n, string what)
        {
            var l = v as List<object>;
            if (l == null || l.Count != n) throw new UxException("generated texture: " + what + " is not a list of " + n + " numbers");
            var o = new double[n]; for (int i = 0; i < n; i++) o[i] = UxJson.Num(l[i]);
            return o;
        }

        /// gen_reference(src_bytes, recipe): the exact float RGBA (0..255) levels of a texture ux_materials asks for (recipe.kind:
        /// composite {bg, k}, channel_alpha {channel, scale, offset}, literal {rgb}, const_alpha {alpha}); fromSource = the number of
        /// leading levels made from source mips (the rest are box filters of the level above).
        public static List<UxFImage> GenReference(byte[] src, JObj recipe, out int fromSource)
        {
            string kind = recipe.Get("kind") as string;
            if (kind != "composite" && kind != "literal" && kind != "const_alpha" && kind != "channel_alpha") throw new UxException("unknown generated kind '" + kind + "'");
            var refs = new List<UxFImage>();
            if (kind == "literal" || kind == "const_alpha")
            {
                double[] rgb = kind == "literal" ? Vec(recipe.Get("rgb"), 3, "rgb") : new[] { 1.0, 1.0, 1.0 };
                double a = kind == "literal" ? 1.0 : UxJson.Num(recipe.Get("alpha"));
                var px = new[] { rgb[0], rgb[1], rgb[2], a };
                for (int i = 0; i < 4; i++) px[i] = UxNp.Round(UxNp.Clip(px[i], 0, 1) * 255);
                int n = BitLength(GEN_SMALL);                       // int(np.log2(4)) + 1
                fromSource = n;
                for (int m = 0; m < n; m++)
                {
                    int s = Math.Max(1, GEN_SMALL >> m); var d = new double[s * s * 4];
                    for (int i = 0; i < s * s; i++) { d[4 * i] = px[0]; d[4 * i + 1] = px[1]; d[4 * i + 2] = px[2]; d[4 * i + 3] = px[3]; }
                    refs.Add(new UxFImage(d, s, s, 4));
                }
                return refs;
            }
            if (src == null) throw new UxException("generated texture: no source texture");
            var t = new UxTexture(src);
            if (t.nsurf < 1 || t.faces != 1) throw new UxException("source is not a 2D texture");
            var sf = t.surfaces[0]; int w = sf.w, h = sf.h;
            if (!(Pot(w) && Pot(h))) throw new UxException("non-power-of-two source " + w + "x" + h);
            int levels = BitLength(Math.Max(w, h));                // int(np.log2(max(w, h))) + 1 of a power of two
            double[] bg = null, kk = null; int ch = 0; double scale = 0, offset = 0;
            if (kind == "composite") { bg = Vec(recipe.Get("bg"), 3, "bg"); kk = Vec(recipe.Get("k"), 3, "k"); }
            else
            {
                ch = "xyzw".IndexOf(recipe.Get("channel") as string ?? "?", StringComparison.Ordinal);
                if (ch < 0 || (recipe.Get("channel") as string).Length != 1) throw new UxException("generated texture: channel " + recipe.Get("channel"));
                scale = UxJson.Num(recipe.Get("scale")); offset = UxJson.Num(recipe.Get("offset"));
            }
            var unit = new double[256]; for (int i = 0; i < 256; i++) unit[i] = i / 255.0;
            for (int m = 0; m < levels; m++)
            {
                if (m >= sf.mips) { refs.Add(BoxHalf(refs[refs.Count - 1])); continue; }
                var img = t.Rgba(0, 0, m, false).Crop(Lv(h, m), Lv(w, m)); int n = img.h * img.w;
                var o = new double[n * 4];
                for (int i = 0; i < n; i++)
                {
                    if (kind == "composite")
                    {
                        double a = unit[img.data[4 * i + 3]];
                        for (int c = 0; c < 3; c++)
                        {
                            double v = unit[img.data[4 * i + c]] + kk[c]; v = v * a; v = v + bg[c];
                            o[4 * i + c] = UxNp.Clip(v, 0, 1) * 255.0;
                        }
                        o[4 * i + 3] = 255.0;
                    }
                    else
                    {
                        double v = scale * unit[img.data[4 * i + ch]]; v = v + offset;
                        v = UxNp.Clip(v, 0, 1) * 255.0;
                        o[4 * i] = v; o[4 * i + 1] = v; o[4 * i + 2] = v; o[4 * i + 3] = v;
                    }
                }
                refs.Add(new UxFImage(o, img.h, img.w, 4));
            }
            fromSource = Math.Min(levels, sf.mips);
            return refs;
        }

        /// generated_texture(src_bytes, recipe): (Reflex PC v4 bytes, info) of a generated texture: composite -> DXT1, channel_alpha ->
        /// DXT5, literal / const_alpha -> A8R8G8B8 4 x 4. DXT levels are encoded by Pillow's encoder (UxPil.BcEncode), or by the range
        /// fit when that does better on a level that misses its tolerance or is at most 128 px. info: format, width, height, mips,
        /// from_source, err, tol, encoder, max_over.
        public static byte[] GeneratedTexture(byte[] src, JObj recipe, out JObj info)
        {
            var refs = GenReference(src, recipe, out int nsrc);
            string kind = recipe.Get("kind") as string;
            byte[] fmt = kind == "composite" ? UxTex.PC_DXT1 : kind == "channel_alpha" ? UxTex.PC_DXT5 : UxTex.PC_A8R8G8B8;
            string fn = PcFmtName(fmt);
            int h = refs[0].h, w = refs[0].w;
            var payload = new List<byte[]>(); var errs = new List<object>(); var encs = new List<object>(); var tols = new List<object>();
            double over = double.NegativeInfinity;
            foreach (var rf in refs)
            {
                int lh = rf.h, lw = rf.w;
                double t0 = Math.Max(lw, lh) >= GEN_BAKE_MIN ? GEN_BAKE_TOL : GEN_RGB_TOL, t1 = Math.Max(lw, lh) >= GEN_BAKE_MIN ? GEN_BAKE_TOL : GEN_ALPHA_TOL;
                var q = rf.ToU8();
                byte[] raw; string enc;
                if (fn == "A8R8G8B8") { raw = Bgra(q); enc = "bytes"; }
                else { raw = UxPil.BcEncode(fn == "DXT1" ? Rgb(q) : q, fn == "DXT5"); enc = "pillow"; }
                GenErrors(PcDecode(fn, lw, lh, raw), rf, out double er, out double ea);
                if (fn != "A8R8G8B8" && (er > t0 || ea > t1 || Math.Max(lw, lh) <= GEN_BAKE_MIN))
                {
                    byte[] raw2 = fn == "DXT1" ? Bc1Encode(q) : Bc3Encode(q);
                    GenErrors(PcDecode(fn, lw, lh, raw2), rf, out double er2, out double ea2);
                    if (PyMax(er2 / t0, ea2 / t1) < PyMax(er / t0, ea / t1)) { raw = raw2; er = er2; ea = ea2; enc = "range-fit"; }
                }
                double re = UxNp.PyRound(er, 3), ra = UxNp.PyRound(ea, 3);
                payload.Add(raw); errs.Add(new List<object> { re, ra }); encs.Add(enc); tols.Add(new List<object> { t0, t1 });
                double ov = PyMax(re / t0, ra / t1);
                if (payload.Count == 1 || ov > over) over = ov;
            }
            var table = new List<(int, int)>();
            for (int i = 0; i < payload.Count; i++) table.Add((payload[i].Length, UxTex.PcPitch(fmt, Lv(w, i))));
            byte[] data = UxTex.BuildPc(new List<(int, int, int, byte[])> { (payload.Count, w, h, fmt) }, table, payload);
            info = new JObj().Set("format", fn).Set("width", (long)w).Set("height", (long)h).Set("mips", (long)payload.Count).Set("from_source", (long)nsrc)
                .Set("err", errs).Set("tol", tols).Set("encoder", encs).Set("max_over", UxNp.PyRound(over, 4));
            return data;
        }
        /// Python's max(a, b): the first unless the second is larger.
        static double PyMax(double a, double b) => b > a ? b : a;
        /// float(np.abs(got - ref)[..., :3].mean()), float(np.abs(got - ref)[..., 3].mean()).
        static void GenErrors(UxImage got, UxFImage rf, out double er, out double ea)
        {
            UxBin.Require(got.h == rf.h && got.w == rf.w && got.c == 4 && rf.c == 4, "generated texture: decoded level size");
            int n = got.h * got.w; var d = new double[n * 3]; var da = new double[n];
            for (int i = 0; i < n; i++)
            {
                for (int c = 0; c < 3; c++) d[3 * i + c] = Math.Abs(got.data[4 * i + c] - rf.d[4 * i + c]);
                da[i] = Math.Abs(got.data[4 * i + 3] - rf.d[4 * i + 3]);
            }
            er = MeanRgbView(d, n * 3); ea = MeanPairwise(da, n);
        }
        /// q[..., [2, 1, 0, 3]] bytes.
        static byte[] Bgra(UxImage q)
        {
            var o = new byte[q.data.Length];
            for (int i = 0; i < o.Length; i += 4) { o[i] = q.data[i + 2]; o[i + 1] = q.data[i + 1]; o[i + 2] = q.data[i]; o[i + 3] = q.data[i + 3]; }
            return o;
        }
        /// q[..., :3].copy().
        static UxImage Rgb(UxImage q)
        {
            int n = q.h * q.w; var o = new byte[n * 3];
            for (int i = 0; i < n; i++) { o[3 * i] = q.data[4 * i]; o[3 * i + 1] = q.data[4 * i + 1]; o[3 * i + 2] = q.data[4 * i + 2]; }
            return new UxImage(o, q.h, q.w, 3);
        }

        // ------------------------------------------------------------------------------------------- conversion
        /// is_jumbotron(t).
        public static bool IsJumbotron(UxTexture t)
        {
            var s = t.surfaces[0];
            return (t.flags & 0x200) != 0 && !(Pot(s.w) && Pot(s.h));
        }

        static byte[] CropBlocks(UxBlocks b, int lh, int lw)
        {
            lh = Math.Min(lh, b.hb); lw = Math.Min(lw, b.wb);
            if (lh == b.hb && lw == b.wb) return b.data;
            var o = new byte[lh * lw * b.bpb];
            for (int y = 0; y < lh; y++) Buffer.BlockCopy(b.data, y * b.wb * b.bpb, o, y * lw * b.bpb, lw * b.bpb);
            return o;
        }

        /// convert_texture(data, name, tex_max, npot, clash): Untamed .texture bytes -> (record, PC bytes or null, expect). clash = the
        /// template_clash dict (null: looked up in `templates`). Throws UxException where Python raises.
        public static UxTexConverted ConvertTexture(byte[] data, string name, int texMax = 0, string npot = "nearest", JObj clash = null,
            IDictionary<string, UxTemplateNames> templates = null)
        {
            var t = new UxTexture(data);
            UxBin.Require(t.nsurf >= 1, name + ": texture without surfaces");
            var s0 = t.surfaces[0];
            JObj cl = clash != null && clash.items.Count > 0 ? clash : TemplateClash(name, templates);
            var rec = new JObj().Set("name", name).Set("fmt_in", s0.name).Set("w_in", (long)s0.w).Set("h_in", (long)s0.h).Set("mips_in", (long)s0.mips)
                .Set("frames_in", (long)t.nsurf).Set("kind_in", (long)t.kind).Set("flags_in", (long)t.flags).Set("src_bytes", (long)data.Length)
                .Set("src_md5", UxBin.Md5(data)).Set("dropped_mips", 0L).Set("template_clash", cl).Set("asset", name).Set("file", null)
                .Set("fmt_out", null).Set("w", null).Set("h", null).Set("mips", null).Set("bytes", 0L).Set("md5", null);
            var res = new UxTexConverted { rec = rec };
            if (t.CheckSize() == null) throw new UxException(name + ": Untamed layout does not end at the file end");
            // ---- replace / template (no file)
            if (IsJumbotron(t))
            {
                rec.Set("action", "replace").Set("asset", JUMBO_REPLACEMENT).Set("reason", "jumbotron render texture (flags 0x200, " + s0.w + "x" + s0.h +
                    " npot, filled at run time by the Untamed jumbotron camera): materials use the template " + JUMBO_REPLACEMENT);
                return res;
            }
            if (UxCommon.PyLower(name).StartsWith("editor_default", StringComparison.Ordinal) && AllValues(cl))
            {
                string sx = cl.Get("sx") as string;
                if (sx == null) throw new UxException(name + ": no 'sx' template name table");         // KeyError
                rec.Set("action", "template").Set("asset", sx).Set("reason", "engine default texture present in both templates (" + sx + "): the template asset is used");
                return res;
            }
            string asset = name;
            if (AnyValue(cl)) { asset = "UX_" + name; rec.Set("asset", asset); }
            byte[] target = t.DefaultPcTarget(0);
            string fn = PcFmtName(target);
            var reasons = new List<string>();
            if (t.nsurf > 1) reasons.Add("animated: surface 0 of " + t.nsurf + " kept");
            int w = s0.w, h = s0.h, mips = Math.Max(1, s0.mips), faces = t.faces, k;
            int rm, rw, rh;                                                     // recs[0]
            var table = new List<(int, int)>(); var payload = new List<byte[]>(); var expect = new List<UxTexExpect>();
            string action;
            if (!(Pot(w) && Pot(h)))
            {
                if (faces != 1 || fn != "A8R8G8B8" || t.Elem(0).name != "8_8_8_8")
                    throw new UxException(name + ": non-power-of-two " + fn + " " + w + "x" + h + " (only A8R8G8B8 2D handled)");
                int nw = PotRound(w, npot), nh = PotRound(h, npot);
                int nm = BitLength(Math.Max(nw, nh));                           // the full chain (rebuilt by box filter)
                var blk = t.LevelBlocks(0, 0, 0);
                UxBin.Require(blk.hb >= h && blk.wb >= w, name + ": level 0 smaller than the surface");
                var img = UxFImage.Of(new UxImage(CropBlocks(blk, h, w), h, w, 4));      // B, G, R, A bytes (PC order)
                var full = ResampleLinear(img, nw, nh);
                var lv = new List<UxFImage> { full };
                for (int m = 1; m < nm; m++) lv.Add(BoxHalf(lv[lv.Count - 1]));
                k = DropCount(nw, nh, nm, texMax);
                rec.Set("dropped_mips", (long)k);
                lv.RemoveRange(0, k);
                int ow = Lv(nw, k), oh = Lv(nh, k);
                rm = lv.Count; rw = ow; rh = oh;
                for (int i = 0; i < lv.Count; i++)
                {
                    byte[] raw = lv[i].ToU8().data;
                    table.Add((raw.Length, UxTex.PcPitch(target, Lv(ow, i))));
                    payload.Add(raw); expect.Add(new UxTexExpect(raw));
                }
                var back = ResampleLinear(full, w, h);
                var diff = new double[back.d.Length];
                for (int i = 0; i < diff.Length; i++) diff[i] = Math.Abs(back.d[i] - img.d[i]);
                rec.Set("resample_mae_back", MeanPairwise(diff, diff.Length));
                reasons.Add("non-power-of-two " + w + "x" + h + " -> " + nw + "x" + nh + " (" + npot + ", linear), " + nm + " mips rebuilt by box filter");
                action = "resample";
            }
            else
            {
                GreyDxn gf = fn == "G16R16F" && faces == 1 && t.nsurf == 1 ? GreyDxnFacts(t) : null;
                if (gf != null && gf.grey)
                {
                    // a greyscale logo stored as DXN is drawn as a diffuse: each source mip -> A8R8G8B8 with B = G = R = the decoded R, A = 255
                    k = DropCount(w, h, mips, texMax);
                    rec.Set("dropped_mips", (long)k);
                    fn = "A8R8G8B8"; target = UxTex.PC_A8R8G8B8;
                    rm = mips - k; rw = Lv(w, k); rh = Lv(h, k);
                    for (int m = k; m < mips; m++)
                    {
                        var img = t.Rgba(0, 0, m, false); int n = img.h * img.w;
                        var raw = new byte[n * 4];
                        for (int i = 0; i < n; i++) { byte r = img.data[4 * i]; raw[4 * i] = r; raw[4 * i + 1] = r; raw[4 * i + 2] = r; raw[4 * i + 3] = 255; }
                        table.Add((raw.Length, UxTex.PcPitch(target, Lv(w, m))));
                        payload.Add(raw); expect.Add(new UxTexExpect(raw));
                    }
                    rec.Set("grey_dxn", gf.ToDict());
                    reasons.Add("greyscale logo stored as DXN (mean |R - G| " + Fixed(gf.rg, 3) + ", std " + Fixed(gf.std, 1) + "): drawn as a diffuse, so each mip -> " +
                        "A8R8G8B8 grey (B = G = R = the decoded R, A = 255)");
                    action = "convert";
                    int full = BitLength(Math.Max(w, h));
                    if (mips < full)
                    {
                        var cur = UxFImage.Of(PcDecode("A8R8G8B8", Lv(w, mips - 1), Lv(h, mips - 1), payload[payload.Count - 1]));
                        int gen = 0;
                        for (int m = mips; m < full; m++)
                        {
                            cur = BoxHalf(cur);
                            byte[] raw = Bgra(cur.ToU8());
                            table.Add((raw.Length, UxTex.PcPitch(target, Lv(w, m))));
                            payload.Add(raw); expect.Add(new UxTexExpect(raw)); gen++;
                        }
                        rm += gen;
                        rec.Set("generated_mips", (long)gen);
                        reasons.Add(gen + " missing lower mip(s) generated (2x2 box filter of the level above)");
                    }
                    reasons.Insert(0, "DXN greyscale -> A8R8G8B8");
                }
                else
                {
                    k = DropCount(w, h, mips, texMax);
                    rec.Set("dropped_mips", (long)k);
                    rm = mips - k; rw = Lv(w, k); rh = Lv(h, k);
                    for (int f = 0; f < faces; f++)
                        for (int m = k; m < mips; m++)
                        {
                            byte[] d = t.PcLevelBytes(0, f, m, target);
                            table.Add((d.Length, UxTex.PcPitch(target, Lv(w, m))));
                            payload.Add(d);
                            if (fn == "G16R16F") expect.Add(new UxTexExpect(0, f, m));
                            else
                            {
                                var blk = t.LevelBlocks(0, f, m);
                                expect.Add(new UxTexExpect(fn == "A8R8G8B8" || fn == "A16B16G16R16F" ? CropBlocks(blk, Lv(h, m), Lv(w, m)) : blk.data));
                            }
                        }
                    action = "convert";
                    // missing lower mips (stock kind-0 textures always carry the full chain)
                    int full = BitLength(Math.Max(w, h));
                    if (faces == 1 && mips < full && (fn == "DXT1" || fn == "DXT5" || fn == "A8R8G8B8" || fn == "G16R16F"))
                    {
                        int lw = Lv(w, mips - 1), lh = Lv(h, mips - 1);
                        byte[] last = payload[payload.Count - 1];
                        UxFImage cur;
                        if (fn == "G16R16F")
                        {
                            UxBin.Require(last.Length == lh * lw * 4, name + ": G16R16F level size");
                            var d = new double[lh * lw * 2];
                            for (int i = 0; i < d.Length; i++) d[i] = UxNp.HalfToFloat(UxBin.U16LE(last, 2 * i));
                            cur = new UxFImage(d, lh, lw, 2);
                        }
                        else cur = UxFImage.Of(PcDecode(fn, lw, lh, last));
                        int gen = 0;
                        var parent = cur;                                       // the decoded level above (the V3.1 reference), DXT1 and DXT5
                        var fallback = new List<object>(); var fallbackMips = new List<object>();
                        for (int m = mips; m < full; m++)
                        {
                            cur = BoxHalf(cur);
                            int mw = Lv(w, m), mh = Lv(h, m);
                            byte[] raw;
                            if (fn == "G16R16F")
                            {
                                raw = new byte[cur.d.Length * 2];
                                for (int i = 0; i < cur.d.Length; i++) UxBin.PutU16(raw, 2 * i, UxNp.DoubleToHalf(cur.d[i]));
                            }
                            else if (fn == "A8R8G8B8") raw = Bgra(cur.ToU8());
                            else
                            {
                                // the float box chain first (DXT1: range fit, DXT5: Pillow's encoder); a level that misses the V3.1 tolerance
                                // against the box filter of its decoded parent is re-encoded from that reference itself by the range fit
                                var q = cur.ToU8();
                                raw = fn == "DXT1" ? Bc1Encode(q) : UxPil.BcEncode(q, true);
                                UxFImage dec;
                                if (Math.Min(mw, mh) >= 4)
                                {
                                    var (er, ea, got) = GenMipError(raw, fn, mw, mh, parent);
                                    dec = got;
                                    if (GenMipMisses(er, ea, fn))
                                    {
                                        var q2 = BoxHalf(parent).ToU8();
                                        byte[] raw2 = fn == "DXT1" ? Bc1Encode(q2) : Bc3Encode(q2);
                                        var (er2, ea2, got2) = GenMipError(raw2, fn, mw, mh, parent);
                                        fallback.Add(new List<object> { (long)m, UxNp.PyRound(er, 1), UxNp.PyRound(ea, 1), UxNp.PyRound(er2, 1), UxNp.PyRound(ea2, 1) });
                                        fallbackMips.Add((long)m);
                                        raw = raw2; dec = got2;
                                    }
                                }
                                else dec = UxFImage.Of(PcDecode(fn, mw, mh, raw));
                                parent = dec;
                            }
                            table.Add((raw.Length, UxTex.PcPitch(target, mw)));
                            payload.Add(raw); expect.Add(new UxTexExpect(raw)); gen++;
                        }
                        if (fallback.Count > 0 && fn == "DXT1")
                        {
                            rec.Set("generated_mips_bc1_fallback", fallback);
                            reasons.Add("generated DXT1 mip(s) " + PyList(fallbackMips) + " re-encoded from the box filter of the decoded parent: the " +
                                "float chain missed the V3.1 tolerance (mip, RGB/alpha err float chain, decoded parent) " + PyList(fallback));
                        }
                        else if (fallback.Count > 0)
                        {
                            rec.Set("generated_mips_bc3_fallback", fallback);
                            reasons.Add("generated DXT5 mip(s) " + PyList(fallbackMips) + " re-encoded with bc3_encode: Pillow missed the V3.1 " +
                                "tolerance (mip, RGB/alpha err Pillow, bc3_encode) " + PyList(fallback));
                        }
                        rm += gen;
                        rec.Set("generated_mips", (long)gen);
                        reasons.Add(gen + " missing lower mip(s) generated (2x2 box filter of the level above, re-encoded " + fn + "; the " + (mips - k) +
                            " source mip(s) bit-exact)");
                    }
                    string how;
                    switch (fn)
                    {
                        case "DXT1": how = "DXT1 blocks copied"; break;
                        case "DXT5": how = "DXT5 blocks copied"; break;
                        case "A8R8G8B8": how = "A8R8G8B8 bytes copied"; break;
                        case "G16R16F": how = "DXN -> G16R16F (BC5/255, nearest float16)"; break;
                        case "A16B16G16R16F": how = "half floats copied"; break;
                        default: throw new UxException(name + ": no conversion rule for " + fn);          // KeyError
                    }
                    reasons.Insert(0, how + (s0.name.StartsWith("LIN_", StringComparison.Ordinal) ? " (linear source)" : ""));
                    if (faces == 6) reasons.Add("cube map: kind 2, faces face-major");
                }
            }
            if (k != 0) reasons.Add("--tex-max " + texMax + ": " + k + " top mip(s) dropped");
            if (t.flags != 0) reasons.Add("Untamed flags 0x" + t.flags.ToString("x", Inv) + " -> PC flags 0");
            if (asset != name)
            {
                var hits = new List<string>();
                var sorted = new List<KeyValuePair<string, object>>(cl.items); sorted.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
                foreach (var kv in sorted) if (Truthy(kv.Value)) hits.Add(kv.Key + " " + (string)kv.Value);
                reasons.Add("name clash with template " + string.Join(",", hits) + ": asset renamed " + asset);
            }
            byte[] pc = UxTex.BuildPc(new List<(int, int, int, byte[])> { (rm, rw, rh, target) }, table, payload, t.kind == 10 ? 2 : 0, 0);
            rec.Set("action", action).Set("reason", string.Join("; ", reasons)).Set("fmt_out", fn).Set("w", (long)rw).Set("h", (long)rh).Set("mips", (long)rm)
                .Set("bytes", (long)pc.Length).Set("md5", UxBin.Md5(pc)).Set("file", asset + ".texture");
            res.pc = pc; res.expect = expect;
            return res;
        }

        // ------------------------------------------------------------------------------------------- the check
        static double[] s_unit, s_nearest;
        static void DxnTables(out double[] unit, out double[] nearest)
        {
            unit = s_unit; nearest = s_nearest;
            if (unit != null && nearest != null) return;
            unit = new double[256]; nearest = new double[256];
            for (int i = 0; i < 256; i++) { unit[i] = i / 255.0; nearest[i] = UxNp.HalfToFloat(UxNp.DoubleToHalf(unit[i])); }
            s_nearest = nearest; s_unit = unit;
        }

        /// '%.6g' % v for the problem lines (never reached by a correct conversion).
        static string G6(double v)
        {
            string s = v.ToString("G6", Inv); int e = s.IndexOf('E');
            if (e < 0) return s;
            string exp = s.Substring(e + 1); char sign = exp[0] == '-' ? '-' : '+'; exp = exp.TrimStart('+', '-').TrimStart('0');
            return s.Substring(0, e) + "e" + sign + exp.PadLeft(2, '0');
        }

        /// check_texture(data, pc, rec, expect): the written PC bytes read back (UxPcTexture) and every entry compared with what the
        /// source holds -> (problems, stats).
        public static List<string> CheckTexture(byte[] data, byte[] pc, JObj rec, List<UxTexExpect> expect, out UxTexCheckStats st)
        {
            var probs = new List<string>(); st = new UxTexCheckStats();
            UxTexture t; UxPcTexture pt; List<UxPcEntry> ents;
            try { t = new UxTexture(data); ents = PcEntries(pc, out pt); }
            catch (Exception e) when (!(e is OperationCanceledException)) { probs.Add("re-read failed: " + e.Message); return probs; }
            foreach (string x in PcLayoutProblems(pc, true)) probs.Add("layout: " + x);
            if (ents.Count != expect.Count) { probs.Add("entries " + ents.Count + " != expected " + expect.Count); return probs; }
            if (pt.nsurf != 1) probs.Add("nsurf " + pt.nsurf);
            if (!(rec.Get("w") is long rw && rec.Get("h") is long rh && rec.Get("mips") is long rm && pt.recs.Count > 0 && pt.recs[0].w == rw && pt.recs[0].h == rh && pt.recs[0].mips == rm))
                probs.Add("header size/mips differ from the record");
            DxnTables(out var unit, out var nearest);
            for (int i = 0; i < ents.Count; i++)
            {
                var e = ents[i]; var ex = expect[i];
                st.entries++;
                if (ex.dxn)
                {
                    var img = t.Rgba(ex.s, ex.f, ex.m, false);
                    UxBin.Require(e.raw.Length == (long)e.h * e.w * 4, "check: G16R16F entry of " + e.raw.Length + " bytes for " + e.w + "x" + e.h);
                    if (img.h != e.h || img.w != e.w) { probs.Add("f" + e.f + " m" + e.m + ": DXN shape (" + img.h + ", " + img.w + ", 2) vs (" + e.h + ", " + e.w + ", 2)"); continue; }
                    double max = 0; long bad = 0; int n = e.h * e.w;
                    for (int p = 0; p < n; p++)
                        for (int c = 0; c < 2; c++)
                        {
                            byte v = img.data[4 * p + c];
                            double got = UxNp.HalfToFloat(UxBin.U16LE(e.raw, 4 * p + 2 * c)), err = Math.Abs(got - unit[v]);
                            if (err > max || double.IsNaN(err)) max = err;
                            if (got != nearest[v]) bad++;
                        }
                    if (max > st.dxn_max_err) st.dxn_max_err = max;
                    st.dxn_not_nearest += bad;
                    if (max > F16_BOUND || bad != 0) probs.Add("f" + e.f + " m" + e.m + ": DXN max err " + G6(max) + ", " + S(bad) + " values not the nearest float16");
                }
                else if (!UxBin.Same(e.raw, ex.raw)) probs.Add("f" + e.f + " m" + e.m + ": " + e.fmt + " bytes differ from the source level");
            }
            if (UxBin.Md5(pc) != rec.Get("md5") as string) probs.Add("md5 differs from the record");
            return probs;
        }

        // ------------------------------------------------------------------------------------------- the level driver
        sealed class Job
        {
            public string name; public JObj cr; public bool done;
            public UxTexConverted conv; public string failed; public List<string> checkProblems; public UxTexCheckStats st;
        }

        /// convert(level, tex_max=..., write=True): the stage. Reads level.json (closure.texture) from the work store and the textures
        /// from the disc; writes <out>/<asset>.texture and <out>/textures.json (out = textures, or textures-max<N>). Returns the
        /// manifest. progress(done, total) is called in texture order, possibly from a worker thread. The work is done on several threads
        /// (options.threads); files and the manifest are written in the reference's order and do not depend on the thread count.
        /// Throws UxException when the manifest lists problems (options.throwOnProblems) and OperationCanceledException on cancel.
        public static JObj Convert(UxContext ctx, UxTexturesOptions options = null, Action<int, int> progress = null)
        {
            var opt = options ?? new UxTexturesOptions();
            if (opt.npot != "nearest" && opt.npot != "floor") throw new UxException("ux_textures: npot '" + opt.npot + "' (nearest or floor)");
            if (opt.texMax < 0) throw new UxException("ux_textures: tex_max " + opt.texMax);
            var templates = opt.templateNames ?? (ctx.stock != null ? ctx.stock.templateNames : null);
            if (templates == null) throw new UxException("ux_textures: no template name tables (options.templateNames or ctx.stock.templateNames)");
            var lv = ctx.lv; var work = ctx.work; var cancel = ctx.cancel;
            byte[] levelJson = work.ReadBytes("level.json");
            var level = UxJson.Obj(UxJson.ParseUtf8(levelJson));
            var clos = UxJson.Obj(UxJson.Get(level, "closure", "texture"));
            // closure_names: sorted(closure); lower = {n.lower(): n} (a later name of the same spelling wins)
            var lower = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int i = 0; i < clos.items.Count; i++) lower[UxCommon.PyLower(clos.items[i].Key)] = i;
            var want = new List<string>(); foreach (var kv in clos.items) want.Add(kv.Key);
            want.Sort(string.CompareOrdinal);
            string outDir = OutDirFor(opt.texMax);
            // the folder holds exactly this run's outputs (the old manifest goes too, so a cancelled run leaves no folder that looks complete;
            // Python keeps it until the new one replaces it)
            foreach (string old in work.List(outDir, "*.texture")) work.Delete(old);
            work.Delete(outDir + "/textures.json");

            var jobs = new Job[want.Count];
            for (int i = 0; i < jobs.Length; i++) { var kv = clos.items[lower[UxCommon.PyLower(want[i])]]; jobs[i] = new Job { name = kv.Key, cr = UxJson.Obj(kv.Value) }; }

            var recs = new JObj(); var recIndex = new Dictionary<string, int>(StringComparer.Ordinal);
            var problems = new List<object>(); var writtenAssets = new HashSet<string>(StringComparer.Ordinal);
            double dxnMax = 0; long dxnNot = 0, entriesChecked = 0;
            int next = 0; object gate = new object();

            // the part of the Python loop that depends on the order: the manifest record, the problems, the file
            void Merge(Job j)
            {
                if (j.failed != null) { problems.Add(j.name + ": " + j.failed); return; }
                var rec = j.conv.rec;
                var mats = j.cr.Get("materials") as List<object> ?? new List<object>();
                bool terrain = PyTruth(j.cr.Get("terrain"));
                rec.Set("materials", mats).Set("terrain", terrain).Set("in_closure", true).Set("terrain_only", terrain && mats.Count == 0);
                if (j.conv.pc != null)
                {
                    if (opt.check)
                    {
                        var pr = new List<object>(); foreach (string x in j.checkProblems) pr.Add(x);
                        rec.Set("check", new JObj().Set("ok", pr.Count == 0).Set("problems", pr).Set("entries", j.st.entries)
                            .Set("dxn_max_err", rec.Get("fmt_out") as string == "G16R16F" ? (object)j.st.dxn_max_err : null));
                        if (j.st.dxn_max_err > dxnMax) dxnMax = j.st.dxn_max_err;
                        dxnNot += j.st.dxn_not_nearest; entriesChecked += j.st.entries;
                        foreach (string x in j.checkProblems) problems.Add(j.name + ": " + x);
                    }
                    string asset = (string)rec.Get("asset");
                    if (writtenAssets.Contains(asset)) problems.Add(j.name + ": duplicate asset name " + asset);
                    work.WriteBytes(outDir + "/" + (string)rec.Get("file"), j.conv.pc);
                    j.conv.pc = null; j.conv.expect = null;
                }
                // recs[name] = rec (a repeated name keeps its place); the assets of the records that hold a file
                if (recIndex.TryGetValue(j.name, out int at))
                {
                    recs.items[at] = new KeyValuePair<string, object>(j.name, rec);
                    writtenAssets.Clear();
                    foreach (var kv in recs.items) if (((JObj)kv.Value).Get("file") != null) writtenAssets.Add((string)((JObj)kv.Value).Get("asset"));
                }
                else
                {
                    recIndex[j.name] = recs.items.Count; recs.items.Add(new KeyValuePair<string, object>(j.name, rec));
                    if (rec.Get("file") != null) writtenAssets.Add((string)rec.Get("asset"));
                }
            }

            void Work(int i)
            {
                cancel.ThrowIfCancellationRequested();
                var j = jobs[i];
                byte[] data = ctx.disc.ReadExtractFile(UxJson.Str(j.cr.Get("file")), cancel);
                try
                {
                    j.conv = ConvertTexture(data, j.name, opt.texMax, opt.npot, null, templates);
                    if (j.conv.pc != null && opt.check) j.checkProblems = CheckTexture(data, j.conv.pc, j.conv.rec, j.conv.expect, out j.st);
                }
                catch (OperationCanceledException) { throw; }
                catch (OutOfMemoryException) { throw; }
                catch (Exception e) { j.conv = null; j.failed = e.Message; }
                lock (gate)
                {
                    j.done = true;
                    while (next < jobs.Length && jobs[next].done)
                    {
                        Merge(jobs[next]); jobs[next].conv = null; next++;
                        progress?.Invoke(next, jobs.Length);
                    }
                }
            }

            int threads = Math.Min(opt.threads > 0 ? opt.threads : Math.Max(1, Environment.ProcessorCount), Math.Max(1, jobs.Length));
            if (threads == 1) for (int i = 0; i < jobs.Length; i++) Work(i);
            else
            {
                // every worker takes the next texture in order, so at most `threads` converted textures wait for the ordered merge
                int cursor = -1; bool stop = false;
                void Worker(int w)
                {
                    try { for (int i; !Volatile.Read(ref stop) && (i = Interlocked.Increment(ref cursor)) < jobs.Length;) Work(i); }
                    catch { Volatile.Write(ref stop, true); throw; }
                }
                try { Parallel.For(0, threads, new ParallelOptions { MaxDegreeOfParallelism = threads, CancellationToken = cancel }, Worker); }
                catch (AggregateException ae)
                {
                    Exception first = null;
                    foreach (var e in ae.Flatten().InnerExceptions) { if (e is OperationCanceledException) throw e; if (first == null) first = e; }
                    if (first is UxException) throw first;
                    throw new UxException("ux_textures: " + (first != null ? first.Message : ae.Message), first ?? ae);
                }
            }
            cancel.ThrowIfCancellationRequested();

            long written = 0, pcBytes = 0, srcBytes = 0, matBytes = 0, terrainOnly = 0;
            var byAction = new JObj(); var byFmt = new JObj();
            foreach (var kv in recs.items)
            {
                var r = (JObj)kv.Value;
                string act = (string)r.Get("action"); byAction.Set(act, (byAction.Get(act) is long c ? c : 0L) + 1);
                if (r.Get("fmt_out") is string fo) { string key = (string)r.Get("fmt_in") + "->" + fo; byFmt.Set(key, (byFmt.Get(key) is long c2 ? c2 : 0L) + 1); }
                srcBytes += (long)r.Get("src_bytes");
                bool to = (bool)r.Get("terrain_only"); if (to) terrainOnly++;
                if (r.Get("file") != null) { written++; pcBytes += (long)r.Get("bytes"); if (!to) matBytes += (long)r.Get("bytes"); }
            }
            var totals = new JObj().Set("textures", (long)recs.items.Count).Set("written", written).Set("pc_bytes", pcBytes).Set("src_bytes", srcBytes)
                .Set("pc_bytes_material_only", matBytes).Set("terrain_only", terrainOnly).Set("by_action", byAction).Set("by_format", byFmt)
                .Set("dxn_max_err", dxnMax).Set("dxn_not_nearest", dxnNot).Set("entries_checked", entriesChecked);
            var man = new JObj().Set("schema", SCHEMA).Set("level", lv.disc).Set("short", lv.shortName).Set("tool", TOOL).Set("tool_md5", REFERENCE_TOOL_MD5)
                .Set("uxtex_md5", REFERENCE_UXTEX_MD5).Set("level_json_md5", UxBin.Md5(levelJson))
                .Set("options", new JObj().Set("tex_max", opt.texMax == 0 ? null : (object)(long)opt.texMax).Set("npot", opt.npot).Set("names", "closure").Set("out_dir", outDir))
                .Set("totals", totals).Set("textures", recs).Set("problems", problems);
            work.WriteJson(outDir + "/textures.json", man);
            ctx.log?.Invoke(lv.disc + ": " + recs.items.Count + " textures, " + written + " written, " + (pcBytes / 1e6).ToString("0.00", Inv) + " MB PC (source " +
                (srcBytes / 1e6).ToString("0.00", Inv) + " MB), " + (problems.Count == 0 ? "OK" : problems.Count + " PROBLEMS"));
            if (problems.Count > 0 && opt.throwOnProblems)
            {
                var head = new List<string>(); for (int i = 0; i < problems.Count && i < 5; i++) head.Add((string)problems[i]);
                throw new UxException("ux_textures: " + string.Join("; ", head) + (problems.Count > 5 ? " (and " + (problems.Count - 5) + " more)" : ""));
            }
            return man;
        }
        /// bool(x) of a JSON value.
        static bool PyTruth(object v)
        {
            switch (v)
            {
                case null: return false; case bool b: return b; case long l: return l != 0; case int i: return i != 0; case double d: return d != 0;
                case string s: return s.Length > 0; case List<object> a: return a.Count > 0; case JObj o: return o.items.Count > 0;
            }
            return true;
        }
    }
}
