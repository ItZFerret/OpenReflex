// MX vs ATV Untamed converter, stage S4 (materials): the port of the Python reference converter's ux_shaderprog.py, the reader of Untamed's
// Xenos pixel programs that the material converter asks four questions of (DESIGN 19, fix round 2): is the diffuse composited over a
// literal colour (analyse), at which UV scale is a sampler fetched (fetch_coords / uv_kind), what literal albedo does a program without
// a usable diffuse draw (albedo_scale) and where does oC0.w come from (alpha_output).
// Untamed .shader = u32 BE version 2, u32 BE count, count x {u32 kind, u32 name length, name, u32 size, Xenos container}; texture slot k
// of an Untamed material = sampler tf<k> of its pixel program.
// The Python module works on the TEXT of its own disassembly (regular expressions over the listing), and the listing's lines go into
// materials.json; so the listing is reproduced character for character and the same expressions are applied to it. Where Python raises
// (a short read, an index outside a list, a failed assert) this port throws UxException: the callers catch or let it through as Python does.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// Python's number formatting ('%g', '%+g', '%.3f', repr of a list of floats), exact: the digits come from integer arithmetic on the
    /// binary value, so every runtime writes what CPython writes.
    internal static class UxPyFmt
    {
        static void Split(double v, out BigInteger num, out BigInteger den)
        {
            long bits = BitConverter.DoubleToInt64Bits(Math.Abs(v));
            int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;      // |v| = m * 2^e
            num = m; den = BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
        }
        /// round-half-even(|v| * 10^scale) as an integer.
        static BigInteger Scaled(double v, int scale)
        {
            Split(v, out var num, out var den);
            if (scale >= 0) num *= BigInteger.Pow(10, scale); else den *= BigInteger.Pow(10, -scale);
            var q = BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            return q;
        }
        static bool Negative(double v) => BitConverter.DoubleToInt64Bits(v) < 0;

        /// '%.<digits>f' % v.
        public static string F(double v, int digits)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            string s = Scaled(v, digits).ToString(CultureInfo.InvariantCulture);
            if (digits > 0)
            {
                if (s.Length <= digits) s = new string('0', digits - s.Length + 1) + s;
                s = s.Substring(0, s.Length - digits) + "." + s.Substring(s.Length - digits);
            }
            return (Negative(v) ? "-" : "") + s;
        }

        /// '%g' % v (6 significant digits, trailing zeros removed, exponent form outside 1e-4 <= |v| < 1e6).
        public static string G(double v, bool plus = false)
        {
            string sign = Negative(v) ? "-" : plus ? "+" : "";
            if (double.IsNaN(v)) return (plus ? "+" : "") + "nan";
            if (double.IsInfinity(v)) return sign + "inf";
            if (v == 0) return sign + "0";
            const int P = 6;
            Split(v, out var num, out var den);
            int k = (int)Math.Floor(Math.Log10(Math.Abs(v)));
            while (Cmp10(num, den, k) < 0) k--;
            while (Cmp10(num, den, k + 1) >= 0) k++;
            var q = Scaled(v, P - 1 - k);
            if (q == BigInteger.Pow(10, P)) { q /= 10; k++; }
            string d = q.ToString(CultureInfo.InvariantCulture);       // P digits
            string body;
            if (k >= -4 && k < P)
            {
                if (k >= 0) { string ip = d.Substring(0, k + 1), fp = d.Substring(k + 1).TrimEnd('0'); body = fp.Length > 0 ? ip + "." + fp : ip; }
                else { string fp = (new string('0', -k - 1) + d).TrimEnd('0'); body = "0." + fp; }
            }
            else
            {
                string fp = d.Substring(1).TrimEnd('0');
                body = d.Substring(0, 1) + (fp.Length > 0 ? "." + fp : "") + "e" + (k < 0 ? "-" : "+") + Math.Abs(k).ToString("00", CultureInfo.InvariantCulture);
            }
            return sign + body;
        }
        static int Cmp10(BigInteger num, BigInteger den, int k)
            => k >= 0 ? num.CompareTo(den * BigInteger.Pow(10, k)) : (num * BigInteger.Pow(10, -k)).CompareTo(den);

        /// repr(float).
        public static string Repr(double v)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            return PyFormat.Repr(v);
        }
        /// repr([floats]) = '%s' % list.
        public static string ReprList(IEnumerable<double> v)
        {
            var sb = new StringBuilder("["); bool first = true;
            foreach (double x in v) { if (!first) sb.Append(", "); first = false; sb.Append(Repr(x)); }
            return sb.Append(']').ToString();
        }
        /// int(round(x)) (Python round: half to even) of a small value.
        public static long RoundInt(double x) => (long)Math.Round(x, MidpointRounding.ToEven);
    }

    /// One program of a .shader (programs()).
    public sealed class UxSpProgram { public long kind; public string name; public byte[] blob; }

    /// One entry of read_shader()'s dict.
    public sealed class UxSpEntry
    {
        public string name, target; public long kind; public bool pixel;
        /// samplers: {register index: name}; floats: the sorted float4 constant names.
        public List<KeyValuePair<int, string>> samplers = new List<KeyValuePair<int, string>>(); public List<string> floats = new List<string>();
        /// alpha_sampler (null: None) or its error; literals {register: [x, y, z, w] rounded to 6 decimals} or its error.
        public int? alpha_sampler; public string alpha_sampler_error;
        public List<KeyValuePair<int, double[]>> literals; public string literals_error;
    }

    /// One parsed listing line (parse()): pc, op, the predicate marker (null: none, "" or "!"), the stripped line, the comma-split operands.
    public sealed class UxSpIns { public int pc; public string op, pred, raw; public string[] parts; }

    /// parse_operand(): a source operand of the listing.
    public sealed class UxSpOperand { public bool neg, abs; public char kind; public int idx; public string sw; }

    /// One term of uv_kind(): (coefficient, offset, interpolant lane 'r5.x').
    public struct UxSpUvTerm
    {
        public double a, b; public string v;
        public bool Same(UxSpUvTerm o) => a == o.a && b == o.b && v == o.v;
    }
    /// fetch_uv(): kind 'identity' | 'scaled' | 'complex' (null: the program never fetches the sampler), the terms and the coordinate operand.
    public sealed class UxSpFetchUv
    {
        public string kind, coord; public List<UxSpUvTerm> terms;
        public static bool SameTerms(List<UxSpUvTerm> x, List<UxSpUvTerm> y)
        {
            if (x == null || y == null) return x == y;
            if (x.Count != y.Count) return false;
            for (int i = 0; i < x.Count; i++) if (!x[i].Same(y[i])) return false;
            return true;
        }
    }

    /// analyse()['composite'].
    public sealed class UxSpComposite
    {
        /// bg / k: component ('x', 'y', 'z', 'w') -> value, in insertion order.
        public List<KeyValuePair<char, double>> bg = new List<KeyValuePair<char, double>>(), k = new List<KeyValuePair<char, double>>();
        public bool lerp; public string[] lines; public string program;
        public bool Has(List<KeyValuePair<char, double>> d, char c) { foreach (var kv in d) if (kv.Key == c) return true; return false; }
        public double Get(List<KeyValuePair<char, double>> d, char c) { foreach (var kv in d) if (kv.Key == c) return kv.Value; throw new UxException("KeyError " + c); }
    }

    /// albedo_scale().
    public sealed class UxSpAlbedo { public double[] rgb; public string how; public bool fog_only; }

    /// alpha_output(): kind 'const' (value) | 'texture' (sampler, channel, scale, offset, sat) | 'other'; raw = the listing line.
    public sealed class UxSpAlpha { public string kind, raw; public double value, scale, offset; public int sampler; public char channel; public bool sat; }

    /// shader_info(): the pixel program a material uses.
    public sealed class UxShaderInfo
    {
        public string program; public List<UxSpIns> ins; public Dictionary<int, double[]> literals = new Dictionary<int, double[]>();
        public int? alpha_sampler; public List<KeyValuePair<int, string>> samplers; public byte[] data;
        /// pixel_programs(data), parsed: every pixel program of the shader in file order (a repeated name keeps its first position and
        /// its last listing, as the Python dict).
        public List<KeyValuePair<string, List<UxSpIns>>> pixelPrograms;
    }

    public static class UxShaderProg
    {
        const string XYZW = "xyzw";
        static UxException Raise(string what) => new UxException("ux_shaderprog: " + what);

        static uint U32(byte[] b, long p)
        {
            if (p < 0 || p + 4 > b.Length) throw Raise("struct.error: read of 4 bytes at " + p + " of " + b.Length);
            return UxBin.U32BE(b, (int)p);
        }
        static uint U16(byte[] b, long p)
        {
            if (p < 0 || p + 2 > b.Length) throw Raise("struct.error: read of 2 bytes at " + p + " of " + b.Length);
            return UxBin.U16BE(b, (int)p);
        }
        /// b[p:e]: a Python slice (clamped).
        static byte[] Slice(byte[] b, long start, long end)
        {
            long n = b.Length; start = Math.Max(0, Math.Min(start, n)); end = Math.Max(0, Math.Min(end, n));
            return end <= start ? new byte[0] : UxBin.Slice(b, (int)start, (int)(end - start));
        }
        static string CStr(byte[] b, long p)
        {
            if (p > b.Length) throw Raise("ValueError: subsection not found");
            int e = Array.IndexOf(b, (byte)0, (int)p);
            if (e < 0) throw Raise("ValueError: subsection not found");
            return Encoding.UTF8.GetString(b, (int)p, e - (int)p);          // decode('utf-8', 'replace')
        }
        static readonly UTF8Encoding s_strict = new UTF8Encoding(false, true);

        /// programs(data).
        public static List<UxSpProgram> Programs(byte[] data)
        {
            uint ver = U32(data, 0), cnt = U32(data, 4);
            if (!(ver == 2 && 0 < cnt && cnt <= 64)) throw Raise("AssertionError (version " + ver + ", count " + cnt + ")");
            long p = 8; var o = new List<UxSpProgram>();
            for (int i = 0; i < cnt; i++)
            {
                uint kind = U32(data, p), nl = U32(data, p + 4); p += 8;
                byte[] nb = Slice(data, p, p + nl); int z = Array.IndexOf(nb, (byte)0); if (z < 0) z = nb.Length;
                string name;
                try { name = s_strict.GetString(nb, 0, z); } catch (ArgumentException) { throw Raise("UnicodeDecodeError in a program name"); }
                p += nl;
                uint size = U32(data, p); p += 4;
                o.Add(new UxSpProgram { kind = kind, name = name, blob = Slice(data, p, p + size) }); p += size;
            }
            if (p != data.Length) throw Raise("AssertionError (trailing " + p + " of " + data.Length + ")");
            return o;
        }

        static readonly string[] REGSETS = { "bool", "int4", "float4", "sampler" };
        /// ctab(blob, off): (target, constants as (name, regset, index, count)).
        static string Ctab(byte[] blob, long off, List<(string name, string regset, int index, int count)> consts)
        {
            uint nconst = U32(blob, off + 12), cinfo = U32(blob, off + 16), target = U32(blob, off + 24);
            for (long i = 0; i < nconst; i++)
            {
                long at = off + cinfo + 20 * i;
                string name = CStr(blob, off + U32(blob, at));
                uint rs = U16(blob, at + 4);
                if (rs >= REGSETS.Length) throw Raise("IndexError: register set " + rs);
                consts.Add((name, REGSETS[rs], (int)U16(blob, at + 6), (int)U16(blob, at + 8)));
            }
            return CStr(blob, off + target);
        }

        static readonly string[] CF_OPS = { "nop", "exec", "exece", "cexec", "cexece", "cexec_pred", "cexece_pred", "loop_start", "loop_end", "ccall", "ret", "cjmp", "alloc",
            "cexec_predclean", "cexece_predclean", "vfetch_end" };
        struct Cf { public string op; public int addr, count, seq; }
        static bool IsExec(string op) => op == "exec" || op == "exece" || op == "cexec" || op == "cexece" || op == "cexec_pred" || op == "cexece_pred" || op == "cexec_predclean" || op == "cexece_predclean";

        static Cf Dec(ulong bits)
        {
            int op = (int)((bits >> 44) & 15);
            bool ex = op == 1 || op == 2 || op == 3 || op == 4 || op == 5 || op == 6 || op == 13 || op == 14;
            return new Cf { op = CF_OPS[op], addr = ex ? (int)(bits & 0xFFF) : 0, count = ex ? (int)((bits >> 12) & 7) : 0, seq = ex ? (int)((bits >> 16) & 0xFFF) : 0 };
        }
        /// control_flow(words).
        static List<Cf> ControlFlow(uint[] words)
        {
            var o = new List<Cf>(); int i = 0;
            while (3 * (i / 2) + 2 < words.Length)
            {
                ulong w0 = words[3 * (i / 2)], w1 = words[3 * (i / 2) + 1], w2 = words[3 * (i / 2) + 2];
                ulong bits = i % 2 == 0 ? (w0 | ((w1 & 0xFFFF) << 32)) : ((w1 >> 16) | (w2 << 16));
                var d = Dec(bits); o.Add(d); i++;
                if (d.op == "exece" || d.op == "cexece" || d.op == "cexece_pred" || d.op == "cexece_predclean" || i > 512)
                {
                    if (i % 2 == 1) o.Add(Dec((w1 >> 16) | (w2 << 16)));
                    break;
                }
            }
            return o;
        }

        /// alpha_sampler(blob, w): the port of AliveConverters.cs Shaders.AlphaSampler (straight-line tfetch -> oC0.w tracking).
        static int? AlphaSampler(byte[] blob, uint[] w)
        {
            long litBytes = U32(blob, w[6]), insBytes = U32(blob, (long)w[6] + 4);
            long ucodeEnd = Math.Min((long)w[1] + w[2], blob.Length);
            long frm = Math.Min(w[1] + litBytes, ucodeEnd), to = Math.Min(w[1] + litBytes + insBytes, ucodeEnd);
            long n = Math.Max(0, UxNp.FloorDiv(to - frm, 4));
            var words = new uint[n];
            for (long i = 0; i < n; i++) words[i] = U32(blob, frm + 4 * i);
            var origin = new Dictionary<(int, int), (int, int)>(); (int, int)? result = null;
            foreach (var cf in ControlFlow(words))
            {
                if (!IsExec(cf.op)) continue;
                for (int i = 0; i < cf.count; i++)
                {
                    int at = cf.addr + i;
                    if (3 * at + 2 >= words.Length) return null;
                    uint w0 = words[3 * at], w1 = words[3 * at + 1], w2 = words[3 * at + 2];
                    if (((cf.seq >> (2 * i)) & 1) != 0)
                    {
                        if ((w0 & 31) != 1) continue;
                        int dst = (int)((w0 >> 12) & 63), cst = (int)((w0 >> 20) & 31), dsw = (int)(w1 & 0xFFF);
                        for (int c = 0; c < 4; c++)
                        {
                            int sel = (dsw >> (3 * c)) & 7;
                            if (sel < 4) origin[(dst, c)] = (cst, sel);
                            else if (sel != 7) origin.Remove((dst, c));
                        }
                        continue;
                    }
                    int vdest = (int)(w0 & 63), export = (int)((w0 >> 15) & 1), sdest = (int)((w0 >> 8) & 63);
                    int vmask = (int)((w0 >> 16) & 15), smask = (int)((w0 >> 20) & 15), sop = (int)((w0 >> 26) & 63);
                    int[] swz = { (int)((w1 >> 16) & 255), (int)((w1 >> 8) & 255), (int)(w1 & 255) };
                    int[] neg = { (int)((w1 >> 26) & 1), (int)((w1 >> 25) & 1), (int)((w1 >> 24) & 1) };
                    int[] regs = { (int)((w2 >> 16) & 255), (int)((w2 >> 8) & 255), (int)(w2 & 255) };
                    int vop = (int)((w2 >> 24) & 31);
                    int[] sl = { (int)((w2 >> 31) & 1), (int)((w2 >> 30) & 1), (int)((w2 >> 29) & 1) };
                    int Comp(int k, int lane) => ((swz[k] >> (2 * lane)) + lane) & 3;
                    (int, int)? Src(int k, int lane)
                    {
                        if (sl[k] == 0 || neg[k] != 0) return null;
                        return origin.TryGetValue((regs[k] & 63, Comp(k, lane)), out var v) ? v : ((int, int)?)null;
                    }
                    bool vmove = vop == 2 && sl[0] != 0 && sl[1] != 0 && regs[0] == regs[1] && swz[0] == swz[1] && neg[0] == 0 && neg[1] == 0;
                    var nw = new (int, int)?[4]; var has = new bool[4];
                    for (int lane = 0; lane < 4; lane++) if (((vmask >> lane) & 1) != 0) { has[lane] = true; nw[lane] = vmove ? Src(0, lane) : null; }
                    bool smove = sop == 5 && Comp(2, 0) == Comp(2, 1);
                    (int, int)? sval = smove ? Src(2, 0) : null;
                    if (export != 0 && vdest == 0 && (vmask & 8) != 0) result = nw[3];
                    if (export != 0 && sdest == 0 && (smask & 8) != 0) result = sval;
                    if (export == 0)
                    {
                        for (int lane = 0; lane < 4; lane++)
                            if (has[lane]) { if (nw[lane] == null) origin.Remove((vdest, lane)); else origin[(vdest, lane)] = nw[lane].Value; }
                        for (int lane = 0; lane < 4; lane++)
                            if (((smask >> lane) & 1) != 0) { if (sval == null) origin.Remove((sdest, lane)); else origin[(sdest, lane)] = sval.Value; }
                    }
                }
            }
            if (result == null || result.Value.Item2 != 3) return null;
            return result.Value.Item1;
        }

        /// literals(blob, w): {register: [x, y, z, w]} of the non-zero literal constants, rounded to 6 decimals.
        static List<KeyValuePair<int, double[]>> Literals(byte[] blob, uint[] w)
        {
            long litBytes = U32(blob, w[6]);
            int bas = (int)((U32(blob, (long)w[5] + 20) >> 16) & 0xFF);
            var o = new List<KeyValuePair<int, double[]>>();
            for (long k = 0; k < litBytes / 16; k++)
            {
                var v = new double[4]; bool any = false;
                for (int c = 0; c < 4; c++)
                {
                    float f = BitConverter.Int32BitsToSingle((int)U32(blob, (long)w[1] + 16 * k + 4 * c));
                    v[c] = f; if (f != 0) any = true;
                }
                if (!any) continue;
                for (int c = 0; c < 4; c++) v[c] = UxNp.PyRound(v[c], 6);
                o.Add(new KeyValuePair<int, double[]>(bas + (int)k, v));
            }
            return o;
        }

        static uint[] Words12(byte[] b) { var w = new uint[12]; for (int i = 0; i < 12; i++) w[i] = U32(b, 4 * i); return w; }

        /// read_shader(path) on the file's bytes: the entries in program order (a repeated name keeps its first position, last value).
        public static List<UxSpEntry> ReadShader(byte[] data)
        {
            var res = new List<UxSpEntry>();
            foreach (var p in Programs(data))
            {
                var b = p.blob; var w = Words12(b);
                bool pixel = w[0] == 0x102A1100;
                if (w[0] != 0x102A1100 && w[0] != 0x102A1101) throw Raise("AssertionError 0x" + w[0].ToString("x"));
                var consts = new List<(string name, string regset, int index, int count)>();
                string target = Ctab(b, (long)w[4] + 4, consts);
                var ent = new UxSpEntry { name = p.name, kind = p.kind, pixel = pixel, target = target };
                foreach (var c in consts)
                {
                    if (c.regset == "sampler")
                    {
                        int at = ent.samplers.FindIndex(kv => kv.Key == c.index);
                        if (at < 0) ent.samplers.Add(new KeyValuePair<int, string>(c.index, c.name)); else ent.samplers[at] = new KeyValuePair<int, string>(c.index, c.name);
                    }
                    else if (c.regset == "float4") ent.floats.Add(c.name);
                }
                ent.floats.Sort(string.CompareOrdinal);
                if (pixel)
                {
                    try { ent.alpha_sampler = AlphaSampler(b, w); } catch (Exception e) { ent.alpha_sampler_error = e.Message; }
                    try { ent.literals = Literals(b, w); } catch (Exception e) { ent.literals_error = e.Message; }
                }
                int have = res.FindIndex(x => x.name == p.name);
                if (have < 0) res.Add(ent); else res[have] = ent;
            }
            return res;
        }

        // ---------------------------------------------------------------- the Xenos microcode lister (ux_xenos_disasm / AliveRecipe.cs)
        static readonly string[] VECTOR = { "add", "mul", "max", "min", "seq", "sgt", "sge", "sne", "frc", "trunc", "floor", "mad", "cndeq", "cndge", "cndgt",
            "dp4", "dp3", "dp2add", "cube", "max4", "setp_eq_push", "setp_ne_push", "setp_gt_push", "setp_ge_push", "kill_eq",
            "kill_gt", "kill_ge", "kill_ne", "dst", "mova" };
        static readonly string[] SCALAR = { "adds", "adds_prev", "muls", "muls_prev", "muls_prev2", "maxs", "mins", "seqs", "sgts", "sges", "snes", "frcs",
            "truncs", "floors", "exp", "logc", "log", "rcpc", "rcpf", "rcp", "rsqc", "rsqf", "rsq", "movas", "movasf", "subs",
            "subs_prev", "setp_eq", "setp_ne", "setp_gt", "setp_ge", "setp_inv", "setp_pop", "setp_clr", "setp_rstr", "kills_eq",
            "kills_gt", "kills_ge", "kills_ne", "kills_one", "sqrt", "op41", "mulsc", "mulsc", "addsc", "addsc", "subsc", "subsc",
            "sin", "cos", "retain_prev" };
        static readonly int[] VARGS = { 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 3, 3, 3, 3, 2, 2, 3, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2 };

        static string Swz(int code)
        {
            var c = new char[4];
            for (int i = 0; i < 4; i++) c[i] = XYZW[((code >> (2 * i)) + i) & 3];
            string s = new string(c);
            return s == "xyzw" ? "" : "." + s;
        }
        static string Mask(int m)
        {
            var sb = new StringBuilder(4);
            for (int i = 0; i < 4; i++) if (((m >> i) & 1) != 0) sb.Append(XYZW[i]);
            return sb.Length == 0 ? "_" : sb.ToString();
        }
        static string I(int v) => v.ToString(CultureInfo.InvariantCulture);

        /// alu(w0, w1, w2, pixel): the one or two listing lines of an ALU instruction.
        static List<string> Alu(uint w0, uint w1, uint w2, bool pixel)
        {
            int vdest = (int)(w0 & 63), absc = (int)((w0 >> 7) & 1);
            int sdest = (int)((w0 >> 8) & 63), export = (int)((w0 >> 15) & 1);
            int vmask = (int)((w0 >> 16) & 15), smask = (int)((w0 >> 20) & 15);
            int vsat = (int)((w0 >> 24) & 1), ssat = (int)((w0 >> 25) & 1), sop = (int)((w0 >> 26) & 63);
            int[] sw = { (int)((w1 >> 16) & 255), (int)((w1 >> 8) & 255), (int)(w1 & 255) };
            int[] neg = { (int)((w1 >> 26) & 1), (int)((w1 >> 25) & 1), (int)((w1 >> 24) & 1) };
            int pc = (int)((w1 >> 27) & 1), pr = (int)((w1 >> 28) & 1);
            int[] regs = { (int)((w2 >> 16) & 255), (int)((w2 >> 8) & 255), (int)(w2 & 255) };
            int vop = (int)((w2 >> 24) & 31);
            int[] sel = { (int)((w2 >> 31) & 1), (int)((w2 >> 30) & 1), (int)((w2 >> 29) & 1) };

            string Src(int k)
            {
                string s;
                if (sel[k] != 0)
                {
                    int r = regs[k]; s = "r" + I(r & 63);
                    if ((r & 128) != 0) s = "|" + s + "|";
                }
                else
                {
                    s = "c" + I(regs[k]);
                    if (absc != 0) s = "|" + s + "|";
                }
                return (neg[k] != 0 ? "-" : "") + s + Swz(sw[k]);
            }
            string Dest(int reg, int m)
            {
                string name;
                if (export != 0)
                {
                    if (pixel) name = reg == 0 ? "oC0" : reg == 1 ? "oC1" : reg == 2 ? "oC2" : reg == 3 ? "oC3" : reg == 61 ? "oDepth" : "export" + I(reg);
                    else name = reg == 62 ? "oPos" : reg == 63 ? "oPts" : "o" + I(reg);
                }
                else name = "r" + I(reg);
                return name + "." + Mask(m);
            }
            string pre = pr != 0 ? "(" + (pc != 0 ? "" : "!") + "p) " : "";
            var lines = new List<string>(2);
            if (vmask != 0 || (20 <= vop && vop <= 27))
            {
                int n = VARGS[vop];
                string name = vop < VECTOR.Length ? VECTOR[vop] : "vop" + I(vop);
                var a = new string[n]; for (int k = 0; k < n; k++) a[k] = Src(k);
                lines.Add(pre + name + (vsat != 0 ? "_sat" : "") + " " + Dest(vdest, vmask) + ", " + string.Join(", ", a));
            }
            if (sop != 50 || smask != 0)
            {
                string name = sop < SCALAR.Length ? SCALAR[sop] : "sop" + I(sop);
                string operand;
                if (42 <= sop && sop <= 47)
                {
                    int s2 = sw[2];
                    char ca = XYZW[((s2 >> 6) + 3) & 3], cb = XYZW[s2 & 3];
                    int treg = (sop & 1) | (s2 & 0x3C) | (sel[2] << 1);
                    operand = (neg[2] != 0 ? "-" : "") + "c" + I(regs[2]) + "." + ca + ", r" + I(treg) + "." + cb;
                }
                else operand = Src(2);
                string sd = smask != 0 ? Dest(export != 0 ? vdest : sdest, smask) : "ps";
                lines.Add(pre + name + (ssat != 0 ? "_sat" : "") + " " + sd + ", " + operand);
            }
            if (lines.Count == 0) lines.Add(pre + "nop");
            return lines;
        }

        /// fetch(w0, w1, w2): the listing line of a fetch instruction.
        static string Fetch(uint w0, uint w1, uint w2)
        {
            int op = (int)(w0 & 31), srcr = (int)((w0 >> 5) & 63), dstr = (int)((w0 >> 12) & 63), cst = (int)((w0 >> 20) & 31);
            const string comp = "xyzw01?_";
            int dsw = (int)(w1 & 0xFFF);
            var cc = new char[4]; for (int i = 0; i < 4; i++) cc[i] = comp[(dsw >> (3 * i)) & 7];
            string comps = new string(cc);
            if (op == 0)
            {
                cst = (int)((w0 >> 20) & 31) * 3 + (int)((w0 >> 25) & 3);
                int fmt = (int)((w1 >> 16) & 63), stride = (int)(w2 & 255), off = (int)((w2 >> 8) & 0x7FFFFF);
                return "vfetch r" + I(dstr) + "." + comps + ", r" + I(srcr) + ", vf" + I(cst) + " (format " + I(fmt) + ", offset " + I(off) + " dwords, stride " + I(stride) + ")";
            }
            string name = op == 1 ? "tfetch" : op == 16 ? "getBCF" : op == 17 ? "getCompLOD" : op == 18 ? "getGradients" : op == 19 ? "getWeights" : op == 24 ? "setTexLOD"
                : op == 25 ? "setGradientH" : op == 26 ? "setGradientV" : "fetch" + I(op);
            int ssw = (int)((w0 >> 26) & 63);
            var sc = new char[3]; for (int i = 0; i < 3; i++) sc[i] = XYZW[(ssw >> (2 * i)) & 3];
            int bias = (int)((w2 >> 4) & 0x7F);
            return name + " r" + I(dstr) + "." + comps + ", r" + I(srcr) + "." + new string(sc) + ", tf" + I(cst) + (bias != 0 ? " (bias " + I(bias) + ")" : "");
        }

        /// disassemble(blob): the listing ('    %3d <instruction>' per line).
        public static List<string> Disassemble(byte[] blob)
        {
            var w = Words12(blob);
            bool pixel = w[0] == 0x102A1100;
            byte[] ucode = Slice(blob, w[1], (long)w[1] + w[2]);
            long lit = U32(blob, w[6]), ins = U32(blob, (long)w[6] + 4);
            long frm = Math.Min(lit, ucode.Length), to = Math.Min(lit + ins, ucode.Length);
            long n = Math.Max(0, UxNp.FloorDiv(to - frm, 4));
            var words = new uint[n];
            for (long i = 0; i < n; i++) words[i] = U32(ucode, frm + 4 * i);
            var o = new List<string>();
            foreach (var cf in ControlFlow(words))
            {
                if (!IsExec(cf.op)) continue;
                for (int i = 0; i < cf.count; i++)
                {
                    int at = cf.addr + i;
                    if (3 * at + 2 >= words.Length) { o.Add("    <outside>"); continue; }
                    bool f = ((cf.seq >> (2 * i)) & 1) != 0;
                    string num = I(at).PadLeft(3);
                    if (f) o.Add("    " + num + " " + Fetch(words[3 * at], words[3 * at + 1], words[3 * at + 2]));
                    else foreach (string l in Alu(words[3 * at], words[3 * at + 1], words[3 * at + 2], pixel)) o.Add("    " + num + " " + l);
                }
            }
            return o;
        }

        // ---------------------------------------------------------------- the listing, parsed (shaderpat.py)
        static readonly Regex LINE = new Regex(@"^\s*(\d+)\s+(?:\((!?)p\)\s+)?(\S+)\s+(.*)$", RegexOptions.CultureInvariant);
        static readonly Regex OPND = new Regex(@"^(-?)(\|?)(r|c)(\d+)\|?(?:\.([xyzw_01]+))?$", RegexOptions.CultureInvariant);
        static readonly Regex DREG = new Regex(@"^r(\d+)\.(\S+)", RegexOptions.CultureInvariant);
        static readonly Regex MREG = new Regex(@"^(r\d+|oC\d|oDepth|export\d+)\.(\S+)", RegexOptions.CultureInvariant);

        /// parse_operand(s): null when the text is not a register / constant operand.
        public static UxSpOperand ParseOperand(string s)
        {
            var m = OPND.Match(s.Trim());
            if (!m.Success) return null;
            return new UxSpOperand { neg = m.Groups[1].Length > 0, abs = m.Groups[2].Length > 0, kind = m.Groups[3].Value[0], idx = int.Parse(m.Groups[4].Value, CultureInfo.InvariantCulture),
                sw = m.Groups[5].Success && m.Groups[5].Length > 0 ? m.Groups[5].Value : "xyzw" };
        }

        /// parse(lines).
        public static List<UxSpIns> Parse(IEnumerable<string> lines)
        {
            var ins = new List<UxSpIns>();
            foreach (string l in lines)
            {
                var m = LINE.Match(l);
                if (!m.Success) continue;
                var parts = m.Groups[4].Value.Split(',');
                for (int i = 0; i < parts.Length; i++) parts[i] = parts[i].Trim();
                ins.Add(new UxSpIns { pc = int.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture), op = m.Groups[3].Value, pred = m.Groups[2].Success ? m.Groups[2].Value : null, raw = l.Trim(), parts = parts });
            }
            return ins;
        }

        /// lanes(mask): the lane numbers whose letter is in the mask.
        static List<int> Lanes(string mask)
        {
            if (mask == null) throw Raise("TypeError: argument of type 'NoneType' is not iterable");
            var o = new List<int>(4);
            for (int i = 0; i < 4; i++) if (mask.IndexOf(XYZW[i]) >= 0) o.Add(i);
            return o;
        }

        /// dreg(part): (register, mask) of 'r<n>.<mask>'; (-1, null) when the text is something else (Python: (None, None)).
        public static (int reg, string mask) Dreg(string part)
        {
            var m = DREG.Match(part);
            return m.Success ? (int.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture), m.Groups[2].Value) : (-1, null);
        }

        /// I['parts'][-1].split()[0].
        static string LastWord(UxSpIns ins)
        {
            string last = ins.parts[ins.parts.Length - 1];
            var w = last.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
            if (w.Length == 0) throw Raise("IndexError: list index out of range");
            return w[0];
        }
        static int XyzwIndex(char c)
        {
            int i = XYZW.IndexOf(c);
            if (i < 0) throw Raise("ValueError: substring not found");
            return i;
        }
        static char At(string s, int i)
        {
            if (i < 0 || i >= s.Length) throw Raise("IndexError: string index out of range");
            return s[i];
        }
        /// ins.index(J): the first instruction equal to J (Python compares the dicts: pc, op, pred, raw, parts; all follow from the raw line).
        static int IndexOf(List<UxSpIns> ins, UxSpIns j)
        {
            for (int i = 0; i < ins.Count; i++) if (ReferenceEquals(ins[i], j) || ins[i].raw == j.raw) return i;
            throw Raise("ValueError: instruction is not in the list");
        }

        /// pixel_programs(shader_bytes), parsed.
        public static List<KeyValuePair<string, List<UxSpIns>>> PixelPrograms(byte[] shaderBytes)
        {
            var o = new List<KeyValuePair<string, List<UxSpIns>>>();
            foreach (var p in Programs(shaderBytes))
            {
                if (p.kind != 1) continue;
                var ins = Parse(Disassemble(p.blob));
                int at = o.FindIndex(kv => kv.Key == p.name);
                if (at < 0) o.Add(new KeyValuePair<string, List<UxSpIns>>(p.name, ins)); else o[at] = new KeyValuePair<string, List<UxSpIns>>(p.name, ins);
            }
            return o;
        }

        static double[] Lit(Dictionary<int, double[]> literals, int idx) => literals.TryGetValue(idx, out var v) ? v : null;

        /// analyse(shader_bytes, sampler, literals)['composite']: the banner composite  add rA, rT, cK ; mad rB, rA, rT.w, cM  on one sampler
        /// (null when the programs hold none). As in Python the result is the last one found over every fetch of every pixel program.
        public static UxSpComposite AnalyseComposite(UxShaderInfo info, int sampler, Dictionary<int, double[]> literals)
        {
            UxSpComposite composite = null;
            string want = "tf" + I(sampler);
            foreach (var prog in info.pixelPrograms)
            {
                var ins = prog.Value;
                for (int k = 0; k < ins.Count; k++)
                {
                    var In = ins[k];
                    if (In.op != "tfetch") continue;
                    if (LastWord(In) != want) continue;
                    int T = Dreg(In.parts[0]).reg;
                    if (In.parts.Length < 2) throw Raise("IndexError: list index out of range");
                    var cons = new List<(UxSpIns J, UxSpOperand[] srcs)>();
                    for (int q = k + 1; q < ins.Count; q++)
                    {
                        var J = ins[q];
                        if (J.op == "tfetch")
                        {
                            if (Dreg(J.parts[0]).reg == T) break;
                            continue;
                        }
                        var srcs = new UxSpOperand[J.parts.Length - 1];
                        for (int x = 1; x < J.parts.Length; x++) srcs[x - 1] = ParseOperand(J.parts[x]);
                        bool uses = false;
                        foreach (var s in srcs) if (s != null && s.kind == 'r' && s.idx == T) { uses = true; break; }
                        if (uses) cons.Add((J, srcs));
                        int r = Dreg(J.parts[0]).reg;
                        if (r == T && !uses) break;
                    }
                    foreach (var (J, srcs) in cons)
                    {
                        if (J.op != "add" || srcs.Length != 2) continue;
                        UxSpOperand st = null, sc = null;
                        foreach (var s in srcs) if (s != null && s.kind == 'r' && s.idx == T) { st = s; break; }
                        foreach (var s in srcs) if (s != null && s.kind == 'c') { sc = s; break; }
                        if (st == null || sc == null) continue;
                        int A = Dreg(J.parts[0]).reg;
                        for (int q = IndexOf(ins, J) + 1; q < ins.Count; q++)
                        {
                            var J2 = ins[q];
                            if (J2.op != "mad") continue;
                            var s2 = new UxSpOperand[J2.parts.Length - 1];
                            for (int x = 1; x < J2.parts.Length; x++) s2[x - 1] = ParseOperand(J2.parts[x]);
                            if (s2.Length < 1) throw Raise("IndexError: list index out of range");
                            if (!(s2[0] != null && s2[0].kind == 'r' && s2[0].idx == A)) continue;
                            if (s2.Length < 2) throw Raise("IndexError: list index out of range");
                            if (!(s2[1] != null && s2[1].kind == 'r' && s2[1].idx == T && OnlyW(s2[1].sw))) continue;
                            if (s2.Length < 3) throw Raise("IndexError: list index out of range");
                            if (!(s2[2] != null && s2[2].kind == 'c')) continue;
                            string bmask = Dreg(J2.parts[0]).mask;
                            var c = new UxSpComposite();
                            double[] cM = Lit(literals, s2[2].idx), cK = Lit(literals, sc.idx);
                            foreach (int L in Lanes(bmask))
                            {
                                int la = XyzwIndex(At(s2[0].sw, L));
                                char comp = At(st.sw, la);
                                if (cM == null || cK == null) continue;
                                double vM = cM[XyzwIndex(At(s2[2].sw, L))] * (s2[2].neg ? -1 : 1);
                                double vK = cK[XyzwIndex(At(sc.sw, la))] * (sc.neg ? -1 : 1);
                                SetKv(c.bg, comp, vM); SetKv(c.k, comp, vK);
                            }
                            if (c.bg.Count > 0)
                            {
                                bool lerp = true;
                                foreach (var kv in c.bg) if (!(Math.Abs(c.Get(c.k, kv.Key) + kv.Value) < 2e-3)) lerp = false;
                                c.lerp = lerp; c.lines = new[] { J.raw, J2.raw }; c.program = prog.Key;
                                composite = c;
                            }
                            break;
                        }
                        if (composite != null) break;
                    }
                }
            }
            return composite;
        }
        static bool OnlyW(string sw) { foreach (char ch in sw) if (ch != 'w') return false; return sw.Length > 0; }
        static void SetKv(List<KeyValuePair<char, double>> d, char key, double v)
        {
            for (int i = 0; i < d.Count; i++) if (d[i].Key == key) { d[i] = new KeyValuePair<char, double>(key, v); return; }
            d.Add(new KeyValuePair<char, double>(key, v));
        }

        // ---------------------------------------------------------------- affine tracking of the fetch coordinates
        /// A coordinate lane as an affine form over the interpolant lanes ('r5.x' -> coefficient; a null key = the constant term), or 'X'.
        sealed class Form
        {
            public bool X; public List<KeyValuePair<string, double>> t = new List<KeyValuePair<string, double>>();
            public static readonly Form Complex = new Form { X = true };
            public static Form Const(double v) { var f = new Form(); f.t.Add(new KeyValuePair<string, double>(null, v)); return f; }
            public static Form Var(string name) { var f = new Form(); f.t.Add(new KeyValuePair<string, double>(name, 1.0)); return f; }
            public bool TryGet(string key, out double v) { foreach (var kv in t) if (kv.Key == key) { v = kv.Value; return true; } v = 0; return false; }
            public void Set(string key, double v)
            {
                for (int i = 0; i < t.Count; i++) if (t[i].Key == key) { t[i] = new KeyValuePair<string, double>(key, v); return; }
                t.Add(new KeyValuePair<string, double>(key, v));
            }
            public bool HasVar { get { foreach (var kv in t) if (kv.Key != null) return true; return false; } }
            public bool OnlyConst { get { foreach (var kv in t) if (kv.Key != null) return false; return true; } }
        }
        static string Swz4(string sw)
        {
            if (sw.Length == 4) return sw;
            if (sw.Length == 0) throw Raise("IndexError: string index out of range");
            return (sw + new string(sw[sw.Length - 1], 4)).Substring(0, 4);
        }

        sealed class FetchCoord { public string program, coord; public int sampler; public Form[] forms; }

        /// fetch_coords(shader_bytes, sampler, literals): for the first fetch of `sampler` in each pixel program, the coordinate lanes.
        static List<FetchCoord> FetchCoords(UxShaderInfo info, int sampler, Dictionary<int, double[]> literals)
        {
            var res = new List<FetchCoord>();
            string want = "tf" + I(sampler);
            foreach (var prog in info.pixelPrograms)
            {
                var regs = new Dictionary<(int, char), Form>();
                Form Get(UxSpOperand op, int lane)
                {
                    if (op == null) return Form.Complex;
                    char comp = Swz4(op.sw)[lane];
                    Form f;
                    if (comp == '0' || comp == '1') f = Form.Const(comp - '0');
                    else if (op.kind == 'c')
                    {
                        var v = Lit(literals, op.idx);
                        if (v == null) return Form.Complex;
                        f = Form.Const(v[XyzwIndex(comp)]);
                    }
                    else if (!regs.TryGetValue((op.idx, comp), out f)) f = Form.Var("r" + I(op.idx) + "." + comp);
                    if (f.X) return Form.Complex;
                    if (op.abs)
                    {
                        if (f.HasVar) return Form.Complex;
                        if (!f.TryGet(null, out double c)) throw Raise("KeyError: 1");
                        return Form.Const(Math.Abs(c));
                    }
                    var o = new Form();
                    foreach (var kv in f.t) o.t.Add(new KeyValuePair<string, double>(kv.Key, op.neg ? -kv.Value : kv.Value));
                    return o;
                }
                Form Addf(Form a, Form b)
                {
                    if (a.X || b.X) return Form.Complex;
                    var o = new Form(); o.t.AddRange(a.t);
                    foreach (var kv in b.t) { o.TryGet(kv.Key, out double cur); o.Set(kv.Key, cur + kv.Value); }
                    return o;
                }
                Form Mulf(Form a, Form b)
                {
                    if (a.X || b.X) return Form.Complex;
                    bool ka = a.HasVar, kb = b.HasVar;
                    if (ka && kb) return Form.Complex;
                    if (!ka) { var t = a; a = b; b = t; }
                    b.TryGet(null, out double s);
                    var o = new Form();
                    foreach (var kv in a.t) o.t.Add(new KeyValuePair<string, double>(kv.Key, kv.Value * s));
                    return o;
                }

                var ins = prog.Value;
                foreach (var In in ins)
                {
                    if (In.op == "tfetch")
                    {
                        string tf = LastWord(In);
                        var (r, m) = Dreg(In.parts[0]);
                        if (tf == want)
                        {
                            if (In.parts.Length < 2) throw Raise("IndexError: list index out of range");
                            var (cr, csw) = Dreg(In.parts[1]);
                            if (csw == null) throw Raise("TypeError: 'NoneType' object is not subscriptable");
                            var forms = new List<Form>();
                            for (int x = 0; x < Math.Min(2, csw.Length); x++)
                                forms.Add(regs.TryGetValue((cr, csw[x]), out var have) ? have : Form.Var("r" + I(cr) + "." + csw[x]));
                            res.Add(new FetchCoord { program = prog.Key, sampler = int.Parse(tf.Substring(2), CultureInfo.InvariantCulture), coord = In.parts[1], forms = forms.ToArray() });
                            break;
                        }
                        if (r >= 0)
                            for (int L = 0; L < Math.Min(4, m.Length); L++) if (m[L] != '_') regs[(r, XYZW[L])] = Form.Complex;
                        continue;
                    }
                    if (In.op == "vfetch") continue;
                    var (dr, dm) = Dreg(In.parts[0]);
                    if (dr < 0) continue;
                    var srcs = new UxSpOperand[In.parts.Length - 1];
                    for (int x = 1; x < In.parts.Length; x++) srcs[x - 1] = ParseOperand(In.parts[x]);
                    bool sat = In.op.EndsWith("_sat", StringComparison.Ordinal);
                    string op = In.op.Replace("_sat", "");
                    var nw = new List<KeyValuePair<(int, char), Form>>();
                    for (int L = 0; L < 4; L++)
                    {
                        char c = XYZW[L];
                        if (dm.IndexOf(c) < 0) continue;
                        Form f;
                        if (op == "add" && srcs.Length == 2) f = Addf(Get(srcs[0], L), Get(srcs[1], L));
                        else if (op == "mul" && srcs.Length == 2) f = Mulf(Get(srcs[0], L), Get(srcs[1], L));
                        else if (op == "mad" && srcs.Length == 3) f = Addf(Mulf(Get(srcs[0], L), Get(srcs[1], L)), Get(srcs[2], L));
                        else if (op == "max" && srcs.Length == 2 && In.parts[1] == In.parts[2]) f = Get(srcs[0], L);
                        else if ((op == "adds" || op == "muls") && srcs.Length == 1 && srcs[0] != null && Uniform(Swz4(srcs[0].sw)))
                        {
                            var g = Get(srcs[0], 0);
                            if (op == "adds") f = Addf(g, g);
                            else f = !g.X && g.OnlyConst ? Mulf(g, g) : Form.Complex;
                        }
                        else f = Form.Complex;
                        if (sat && !f.X && f.HasVar) f = Form.Complex;
                        nw.Add(new KeyValuePair<(int, char), Form>((dr, c), f));
                    }
                    foreach (var kv in nw) regs[kv.Key] = kv.Value;
                }
            }
            return res;
        }
        static bool Uniform(string s) { foreach (char c in s) if (c != s[0]) return false; return true; }

        /// uv_kind(forms): 'identity' (an interpolant lane pair, coefficient 1, no offset), 'scaled' (affine on one lane each), 'complex'.
        static string UvKind(Form[] forms, out List<UxSpUvTerm> terms)
        {
            terms = null;
            foreach (var f in forms) if (f.X) return "complex";
            var o = new List<UxSpUvTerm>();
            foreach (var f in forms)
            {
                string only = null; int n = 0;
                foreach (var kv in f.t) if (kv.Key != null) { n++; only = kv.Key; }
                if (n != 1) return "complex";
                f.TryGet(only, out double a); f.TryGet(null, out double b);
                o.Add(new UxSpUvTerm { a = a, b = b, v = only });
            }
            terms = o;
            foreach (var t in o) if (!(Math.Abs(t.a - 1) < 1e-6 && Math.Abs(t.b) < 1e-6)) return "scaled";
            return "identity";
        }

        /// fetch_uv(info, sampler): kind null when the material's program never fetches the sampler.
        public static UxSpFetchUv FetchUv(UxShaderInfo info, int sampler)
        {
            FetchCoord first = null;
            foreach (var f in FetchCoords(info, sampler, info.literals)) if (f.program == info.program) { first = f; break; }
            if (first == null) return new UxSpFetchUv();
            string k = UvKind(first.forms, out var terms);
            return new UxSpFetchUv { kind = k, terms = terms, coord = first.coord };
        }

        // ---------------------------------------------------------------- fix round 2 additions
        /// shader_info(shader bytes, entry): the pixel program a material uses (its PixelShader entryPoint when the shader has it, else
        /// the only pixel program), or null when that is ambiguous. Throws what Python raises on an unreadable shader.
        public static UxShaderInfo ShaderInfo(byte[] data, string entry)
        {
            var progs = PixelPrograms(data);
            int at = -1;
            if (!string.IsNullOrEmpty(entry)) at = progs.FindIndex(kv => kv.Key == entry);
            if (at < 0) { if (progs.Count == 1) at = 0; else return null; }
            var rs = ReadShader(data);
            var info = new UxShaderInfo { program = progs[at].Key, ins = progs[at].Value, data = data, pixelPrograms = progs };
            var ent = rs.Find(x => x.name == info.program);
            if (ent != null)
            {
                if (ent.literals != null) foreach (var kv in ent.literals) info.literals[kv.Key] = kv.Value;
                info.alpha_sampler = ent.alpha_sampler; info.samplers = ent.samplers;
            }
            if (info.samplers == null) info.samplers = new List<KeyValuePair<int, string>>();
            return info;
        }

        // ---------------------------------------------------------------- lane evaluator
        // A lane value is ('A', {sym: coef}, const, sat): an affine form over symbols (a constant when the dict is empty), or
        // ('S', scale, deps): scale x (a quantity built from deps); scale null = unknown.
        // Symbols: ('t', sampler, channel) a texture fetch lane; ('i', reg, lane) an interpolant; ('c', reg, lane) an engine-set
        // constant; '?' an operation outside the tracked set.
        struct Sym : IEquatable<Sym>
        {
            public char kind; public int n; public char c;
            public Sym(char kind, int n, char c) { this.kind = kind; this.n = n; this.c = c; }
            public bool Equals(Sym o) => kind == o.kind && n == o.n && c == o.c;
            public override bool Equals(object o) => o is Sym s && Equals(s);
            public override int GetHashCode() => kind * 7919 + n * 31 + c;
        }
        sealed class Val
        {
            public bool isA; public List<KeyValuePair<Sym, double>> terms; public double c; public bool sat;
            public double? scale; public HashSet<Sym> deps;
            public bool TryGet(Sym s, out double v) { foreach (var kv in terms) if (kv.Key.Equals(s)) { v = kv.Value; return true; } v = 0; return false; }
        }
        static readonly List<KeyValuePair<Sym, double>> s_none = new List<KeyValuePair<Sym, double>>();
        static Val A(double c) => new Val { isA = true, terms = s_none, c = c };
        static Val A(double c, Sym s) => new Val { isA = true, terms = new List<KeyValuePair<Sym, double>> { new KeyValuePair<Sym, double>(s, 1.0) }, c = c };
        static readonly Val UNKNOWN = A(0.0, new Sym('?', 0, ' '));
        static HashSet<Sym> Deps(Val v)
        {
            if (!v.isA) return v.deps;
            var d = new HashSet<Sym>(); foreach (var kv in v.terms) d.Add(kv.Key); return d;
        }
        static bool IsConst(Val v) => v.isA && v.terms.Count == 0;
        const int MATERIAL_SAMPLERS = 12;     // tf0..tf11 = the material's textures; tf12+ = engine image light / shadow maps
        static bool Material(IEnumerable<Sym> d) { foreach (var x in d) if (x.kind == 't' && 0 <= x.n && x.n < MATERIAL_SAMPLERS) return true; return false; }
        static double? Scale(Val v)
        {
            if (!v.isA) return v.scale;
            if (v.terms.Count == 1 && Math.Abs(v.c) < 1e-9) return v.terms[0].Value;
            return Material(Deps(v)) ? (double?)null : 1.0;
        }
        static Val S(double? scale, params Val[] vs)
        {
            var d = new HashSet<Sym>();
            foreach (var v in vs) d.UnionWith(Deps(v));
            if (scale != null && scale.Value == 1.0 && Material(d)) scale = null;
            return new Val { isA = false, scale = scale, deps = d };
        }
        static Val Neg(Val v)
        {
            if (v.isA)
            {
                var t = new List<KeyValuePair<Sym, double>>(v.terms.Count);
                foreach (var kv in v.terms) t.Add(new KeyValuePair<Sym, double>(kv.Key, -kv.Value));
                return new Val { isA = true, terms = t, c = -v.c };
            }
            return new Val { isA = false, scale = v.scale == null ? (double?)null : -v.scale.Value, deps = v.deps };
        }
        static Val Add(Val a, Val b)
        {
            if (a.isA && b.isA)
            {
                var t = new List<KeyValuePair<Sym, double>>(a.terms);
                foreach (var kv in b.terms)
                {
                    int at = t.FindIndex(x => x.Key.Equals(kv.Key));
                    if (at < 0) t.Add(new KeyValuePair<Sym, double>(kv.Key, 0.0 + kv.Value)); else t[at] = new KeyValuePair<Sym, double>(kv.Key, t[at].Value + kv.Value);
                }
                t.RemoveAll(x => !(Math.Abs(x.Value) > 1e-12));
                return new Val { isA = true, terms = t, c = a.c + b.c };
            }
            if (IsConst(b) && Math.Abs(b.c) < 1e-9) return a;
            if (IsConst(a) && Math.Abs(a.c) < 1e-9) return b;
            return S(1.0, a, b);
        }
        static Val Mul(Val a, Val b)
        {
            if (IsConst(a)) { var t = a; a = b; b = t; }
            if (IsConst(b))
            {
                double k = b.c;
                if (a.isA)
                {
                    var t = new List<KeyValuePair<Sym, double>>(a.terms.Count);
                    foreach (var kv in a.terms) t.Add(new KeyValuePair<Sym, double>(kv.Key, kv.Value * k));
                    return new Val { isA = true, terms = t, c = a.c * k };
                }
                return new Val { isA = false, scale = a.scale == null ? (double?)null : a.scale.Value * k, deps = a.deps };
            }
            double? sa = Scale(a), sb = Scale(b);
            var d = new HashSet<Sym>(Deps(a)); d.UnionWith(Deps(b));
            if (sa == null || sb == null) return new Val { isA = false, scale = null, deps = d };
            return new Val { isA = false, scale = sa.Value * sb.Value, deps = d };
        }
        /// a == b on the Python tuples.
        static bool Same(Val a, Val b)
        {
            if (ReferenceEquals(a, b)) return true;
            if (a.isA != b.isA) return false;
            if (a.isA)
            {
                if (a.terms.Count != b.terms.Count || !(a.c == b.c) || a.sat != b.sat) return false;
                foreach (var kv in a.terms) if (!b.TryGet(kv.Key, out double v) || !(v == kv.Value)) return false;
                return true;
            }
            if ((a.scale == null) != (b.scale == null)) return false;
            if (a.scale != null && !(a.scale.Value == b.scale.Value)) return false;
            return a.deps.SetEquals(b.deps);
        }
        static Val Max(Val a, Val b)
        {
            if (Same(a, b)) return a;
            if (IsConst(a) && IsConst(b)) return A(b.c > a.c ? b.c : a.c);
            if (IsConst(b) && b.c <= 0.06) return a;             // lower clamp of a non-negative term
            if (IsConst(a) && a.c <= 0.06) return b;
            return S(1.0, a, b);
        }
        static Val Min(Val a, Val b)
        {
            if (Same(a, b)) return a;
            if (IsConst(a) && IsConst(b)) return A(b.c < a.c ? b.c : a.c);
            if (IsConst(b) && b.c >= 1.0) return a;
            if (IsConst(a) && a.c >= 1.0) return b;
            return S(1.0, a, b);
        }
        static Val Sat(Val v)
        {
            if (v.isA)
            {
                if (v.terms.Count == 0) return A(PyMin(1.0, PyMax(0.0, v.c)));
                return new Val { isA = true, terms = v.terms, c = v.c, sat = true };
            }
            return v;
        }
        /// Python max(a, b) / min(a, b) of two floats: the first argument unless the second is strictly greater / smaller.
        internal static double PyMax(double a, double b) => b > a ? b : a;
        internal static double PyMin(double a, double b) => b < a ? b : a;

        static readonly HashSet<string> SCALAR_OPS = new HashSet<string>(StringComparer.Ordinal) { "adds", "adds_prev", "muls", "muls_prev", "muls_prev2", "maxs", "mins", "seqs", "sgts", "sges", "snes",
            "frcs", "truncs", "floors", "exp", "logc", "log", "rcpc", "rcpf", "rcp", "rsqc", "rsqf", "rsq", "movas",
            "movasf", "subs", "subs_prev", "sqrt", "mulsc", "addsc", "subsc", "sin", "cos", "retain_prev" };

        sealed class Export { public int pc; public string op, reg, raw; public char lane; public Val value; }

        /// _run(ins, literals): (register lanes, every oC* lane write in order).
        static Dictionary<(int, char), Val> Run(IList<UxSpIns> ins, Dictionary<int, double[]> literals, List<Export> exports)
        {
            var regs = new Dictionary<(int, char), Val>();
            Val ps = A(0.0);
            Val Rd(UxSpOperand op, int lane)
            {
                if (op == null) return S(1.0, UNKNOWN);
                char comp = Swz4(op.sw)[lane];
                Val v;
                if (comp == '0' || comp == '1') v = A(comp - '0');
                else if (op.kind == 'c')
                {
                    var lit = Lit(literals, op.idx);
                    v = lit != null ? A(lit[XyzwIndex(comp)]) : A(0.0, new Sym('c', op.idx, comp));
                }
                else if (!regs.TryGetValue((op.idx, comp), out v)) v = A(0.0, new Sym('i', op.idx, comp));
                if (op.abs) v = IsConst(v) ? A(Math.Abs(v.c)) : S(1.0, v);
                return op.neg ? Neg(v) : v;
            }

            // ALU lines: the vector and scalar op printed with one pc issue together; fetches stand alone
            int g = 0;
            while (g < ins.Count)
            {
                int end = g + 1;
                while (end < ins.Count && ins[end].pc == ins[end - 1].pc && ins[end].op != "tfetch" && ins[g].op != "tfetch") end++;
                var writes = new List<(bool ps, (int, char) key, Val v)>();
                for (int q = g; q < end; q++)
                {
                    var In = ins[q];
                    string op = In.op;
                    if (op == "tfetch")
                    {
                        var (r, m) = Dreg(In.parts[0]);
                        string tf = LastWord(In);
                        int smp = tf.StartsWith("tf", StringComparison.Ordinal) ? int.Parse(tf.Substring(2), CultureInfo.InvariantCulture) : -1;
                        if (m == null) throw Raise("TypeError: unsupported operand type(s) for +: 'NoneType' and 'str'");
                        string m4 = (m + "____").Substring(0, 4);
                        for (int L = 0; L < 4; L++)
                        {
                            char c = m4[L];
                            if (c == '_') continue;
                            if (XYZW.IndexOf(c) >= 0) writes.Add((false, (r, XYZW[L]), A(0.0, new Sym('t', smp, c))));
                            else writes.Add((false, (r, XYZW[L]), A(c == '0' || c == '1' ? c - '0' : 0.0)));
                        }
                        continue;
                    }
                    if (op == "vfetch" || op == "nop" || op.StartsWith("set", StringComparison.Ordinal) || op.StartsWith("get", StringComparison.Ordinal) || In.parts.Length == 0) continue;
                    bool sat = op.EndsWith("_sat", StringComparison.Ordinal);
                    string bas = sat ? op.Substring(0, op.Length - 4) : op;
                    string dest = In.parts[0];
                    var srcs = new UxSpOperand[In.parts.Length - 1];
                    for (int x = 1; x < In.parts.Length; x++) srcs[x - 1] = ParseOperand(In.parts[x]);
                    string dkey; List<int> dl;
                    if (dest == "ps") { dkey = "ps"; dl = new List<int>(); }
                    else
                    {
                        var mreg = MREG.Match(dest);
                        if (!mreg.Success) continue;
                        dkey = mreg.Groups[1].Value; dl = Lanes(mreg.Groups[2].Value);
                    }
                    var vals = new List<KeyValuePair<int, Val>>();
                    if (SCALAR_OPS.Contains(bas))
                    {
                        // one value for every lane of the mask; single-operand forms read swizzle positions 3 (a) and 0 (b), the
                        // constant forms print 'cN.a, rM.b'
                        Val v;
                        if (bas == "mulsc" || bas == "addsc" || bas == "subsc")
                        {
                            Val a = srcs.Length > 0 && srcs[0] != null ? Rd(srcs[0], 0) : S(1.0, UNKNOWN);
                            Val b = srcs.Length > 1 && srcs[1] != null ? Rd(srcs[1], 0) : S(1.0, UNKNOWN);
                            v = bas == "mulsc" ? Mul(a, b) : bas == "addsc" ? Add(a, b) : Add(a, Neg(b));
                        }
                        else if (bas == "retain_prev") v = ps;
                        else
                        {
                            var s = srcs.Length > 0 ? srcs[0] : null;
                            Val a = Rd(s, 3), b = Rd(s, 0);
                            if (bas == "maxs") v = Max(a, b);
                            else if (bas == "mins") v = Min(a, b);
                            else if (bas == "adds") v = Add(a, b);
                            else if (bas == "muls") v = Mul(a, b);
                            else if (bas == "muls_prev") v = Mul(a, ps);
                            else if (bas == "adds_prev") v = Add(a, ps);
                            else v = S(1.0, a, b, UNKNOWN);
                        }
                        if (sat) v = Sat(v);
                        writes.Add((true, (0, ' '), v));
                        foreach (int L in dl) vals.Add(new KeyValuePair<int, Val>(L, v));
                    }
                    else
                    {
                        foreach (int L in dl)
                        {
                            Val v;
                            if (bas == "add" && srcs.Length == 2) v = Add(Rd(srcs[0], L), Rd(srcs[1], L));
                            else if (bas == "mul" && srcs.Length == 2) v = Mul(Rd(srcs[0], L), Rd(srcs[1], L));
                            else if (bas == "mad" && srcs.Length == 3) v = Add(Mul(Rd(srcs[0], L), Rd(srcs[1], L)), Rd(srcs[2], L));
                            else if (bas == "max" && srcs.Length == 2) v = Max(Rd(srcs[0], L), Rd(srcs[1], L));
                            else if (bas == "min" && srcs.Length == 2) v = Min(Rd(srcs[0], L), Rd(srcs[1], L));
                            else
                            {
                                var all = new List<Val>();
                                foreach (var s in srcs) if (s != null) all.Add(Rd(s, L));
                                all.Add(UNKNOWN);
                                v = S(1.0, all.ToArray());
                            }
                            vals.Add(new KeyValuePair<int, Val>(L, sat ? Sat(v) : v));
                        }
                    }
                    foreach (var kv in vals)
                    {
                        if (dkey.StartsWith("oC", StringComparison.Ordinal)) exports.Add(new Export { pc = In.pc, op = bas, reg = dkey, lane = XYZW[kv.Key], value = kv.Value, raw = In.raw });
                        else if (dkey.StartsWith("r", StringComparison.Ordinal)) writes.Add((false, (int.Parse(dkey.Substring(1), CultureInfo.InvariantCulture), XYZW[kv.Key]), kv.Value));
                    }
                }
                foreach (var wv in writes) { if (wv.ps) ps = wv.v; else regs[wv.key] = wv.v; }
                g = end;
            }
            return regs;
        }

        sealed class Wrap { public bool fogOnly; public int fog, albedoReg; public string[] lines; public Val[] albedo; }

        /// evaluate(ins, literals): the exports and the fog wrapper  add rA, -rB, rF ; mad oC0.xyz, rA, rF.w, rB  (= lerp(rB, fog.rgb,
        /// fog.w)), or fog_only when oC0.rgb = rF.rgb * rF.w with rF an interpolant never written.
        static Wrap Evaluate(List<UxSpIns> ins, Dictionary<int, double[]> literals, List<Export> exports)
        {
            Run(ins, literals, exports);
            UxSpIns last = null;
            foreach (var In in ins)
            {
                if ((In.op != "mad" && In.op != "mul") || In.parts.Length == 0 || !In.parts[0].StartsWith("oC0.", StringComparison.Ordinal)) continue;
                string mask = In.parts[0].Split('.')[1];
                if (mask.IndexOf('x') >= 0 && mask.IndexOf('y') >= 0 && mask.IndexOf('z') >= 0) last = In;
            }
            if (last == null) return null;
            int k = IndexOf(ins, last);
            var s = new UxSpOperand[last.parts.Length - 1];
            for (int x = 1; x < last.parts.Length; x++) s[x - 1] = ParseOperand(last.parts[x]);
            bool allR = true; foreach (var x in s) if (x == null || x.kind != 'r') allR = false;
            if (last.op == "mad" && s.Length == 3 && allR && OnlyW(Swz4(s[1].sw)))
            {
                int Areg = s[0].idx, F = s[1].idx, B = s[2].idx;
                UxSpIns add = null;
                for (int q = k - 1; q >= 0; q--)
                    if (ins[q].op == "add" && ins[q].parts.Length > 0 && Dreg(ins[q].parts[0]).reg == Areg) { add = ins[q]; break; }
                if (add != null)
                {
                    var a2 = new UxSpOperand[add.parts.Length - 1];
                    for (int x = 1; x < add.parts.Length; x++) a2[x - 1] = ParseOperand(add.parts[x]);
                    if (a2.Length == 2 && a2[0] != null && a2[1] != null && a2[0].kind == 'r' && a2[1].kind == 'r' && a2[0].neg && a2[0].idx == B && a2[1].idx == F)
                    {
                        var before = new List<UxSpIns>();
                        foreach (var J in ins) if (J.pc < add.pc) before.Add(J);
                        var pre = Run(before, literals, new List<Export>());
                        var albedo = new Val[3];
                        for (int c = 0; c < 3; c++) albedo[c] = pre.TryGetValue((B, XYZW[c]), out var have) ? have : A(0.0, new Sym('i', B, XYZW[c]));
                        return new Wrap { fog = F, albedoReg = B, lines = new[] { add.raw, last.raw }, albedo = albedo };
                    }
                }
            }
            else if (last.op == "mul" && s.Length == 2 && allR && s[0].idx == s[1].idx && OnlyW(Swz4(s[1].sw)))
            {
                bool written = false;
                for (int q = 0; q < k; q++)
                    if (ins[q].parts.Length > 0 && ins[q].op != "vfetch" && Dreg(ins[q].parts[0]).reg == s[0].idx) { written = true; break; }
                if (!written) return new Wrap { fogOnly = true, fog = s[0].idx, lines = new[] { last.raw } };
            }
            return null;
        }

        /// albedo_scale(info, texture_samplers): the literal albedo of a pixel program whose material has no usable diffuse texture: the
        /// per-lane literal scale of the colour before Untamed's fog wrapper. rgb null when the program has no fog wrapper or a lane
        /// depends on one of textureSamplers.
        public static UxSpAlbedo AlbedoScale(UxShaderInfo info, ICollection<int> textureSamplers)
        {
            var w = Evaluate(info.ins, info.literals, new List<Export>());
            if (w == null) return new UxSpAlbedo { how = "no fog wrapper on the colour export" };
            if (w.fogOnly)
                return new UxSpAlbedo { rgb = new[] { 0.0, 0.0, 0.0 }, fog_only = true, how = "oC0.rgb = r" + I(w.fog) + ".rgb * r" + I(w.fog) + ".w (fog term only, albedo 0): " + w.lines[0] };
            var rgb = new double[3];
            for (int c = 0; c < 3; c++)
            {
                var v = w.albedo[c];
                foreach (var x in Deps(v)) if (x.kind == 't' && textureSamplers.Contains(x.n)) return new UxSpAlbedo { how = "colour depends on a colour texture" };
                if (IsConst(v)) rgb[c] = PyMax(0.0, v.c);
                else
                {
                    double? sc = Scale(v);
                    if (sc == null) return new UxSpAlbedo { how = "no literal scale (a sum or other op over material textures)" };
                    rgb[c] = PyMax(0.0, sc.Value);
                }
            }
            return new UxSpAlbedo { rgb = rgb, how = "albedo r" + I(w.albedoReg) + " before the fog wrapper (" + string.Join(" ; ", w.lines) + ")" };
        }

        /// alpha_output(info): where oC0.w comes from (the last oC* alpha write); null when the program never writes it.
        public static UxSpAlpha AlphaOutput(UxShaderInfo info)
        {
            var exports = new List<Export>();
            Evaluate(info.ins, info.literals, exports);
            Export last = null;
            foreach (var x in exports) if (x.lane == 'w') last = x;
            if (last == null) return null;
            var v = last.value;
            if (IsConst(v)) return new UxSpAlpha { kind = "const", value = PyMin(1.0, PyMax(0.0, v.c)), raw = last.raw };
            if (v.isA && v.terms.Count == 1 && v.terms[0].Key.kind == 't')
                return new UxSpAlpha { kind = "texture", sampler = v.terms[0].Key.n, channel = v.terms[0].Key.c, scale = v.terms[0].Value, offset = v.c, sat = v.sat, raw = last.raw };
            return new UxSpAlpha { kind = "other", raw = last.raw };
        }

        /// _fetch_dest(info, sampler): the destination swizzle of the first fetch of `sampler` in the material's program ('xyzw' = the
        /// texture channels land on the same lanes); null when there is none.
        public static string FetchDest(UxShaderInfo info, int sampler)
        {
            string want = "tf" + I(sampler);
            foreach (var In in info.ins) if (In.op == "tfetch" && LastWord(In) == want) return Dreg(In.parts[0]).mask;
            return null;
        }
    }
}
