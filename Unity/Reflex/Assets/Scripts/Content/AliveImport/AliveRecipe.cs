// The albedo programs of Alive's level pixel programs (_re/formats/alive_recipe.py over xenos_disasm.py's listing): a level shader's
// SharedPixelShader executed symbolically from the disassembled text, its albedo found (|Y| x |Y| x the light buffer, or the unlit
// (Z x fog.w + fog) x gHDRScale form), the three channel trees merged, lerps folded and compiled to the client's float4 op list
// (ReflexOriginalScenery _ALIVE_VM) with the same linear-scan register allocation. The listing text, the expression trees (structural
// equality as Python tuples, including their traversal quirks) and the float arithmetic follow the reference so the sidecar is
// byte-identical.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content.AliveImport
{
    /// An expression node: Python's tuple (tag, items...). Items are X, double (a float) or int. Coordinate pairs are X("()", c0, c1),
    /// a plain tuple in Python: its "tag" there is c0, which the traversals below reproduce (they visit only c1).
    internal sealed class X : IEquatable<X>
    {
        public readonly string t; public readonly object[] a;
        int _hash; bool _hashed;
        public X(string t, params object[] a) { this.t = t; this.a = a; }
        public object this[int i] => a[i - 1];     // x[1].. as in Python (x[0] is the tag)
        public int Len => a.Length + 1;
        public X Sub(int i) => a[i - 1] as X;
        public double D(int i) => Convert.ToDouble(a[i - 1]);
        public int I(int i) => Convert.ToInt32(a[i - 1]);
        public bool IsPair => t == "()";

        static bool ItemEq(object p, object q)
        {
            if (p is X xp) return q is X xq && xp.Equals(xq);
            if (q is X) return false;
            return Convert.ToDouble(p) == Convert.ToDouble(q);
        }
        public bool Equals(X o)
        {
            if (ReferenceEquals(this, o)) return true;
            if (o is null || t != o.t || a.Length != o.a.Length || GetHashCode() != o.GetHashCode()) return false;
            for (int i = 0; i < a.Length; i++) if (!ItemEq(a[i], o.a[i])) return false;
            return true;
        }
        public override bool Equals(object obj) => obj is X x && Equals(x);
        public override int GetHashCode()
        {
            if (_hashed) return _hash;
            int h = t.GetHashCode() * 31 + a.Length;
            foreach (var v in a)
            {
                int k;
                if (v is X x) k = x.GetHashCode();
                else { double d = Convert.ToDouble(v); k = d == 0 ? 0 : d.GetHashCode(); }
                h = unchecked(h * 1000003 + k);
            }
            _hash = h; _hashed = true; return h;
        }
        public static bool operator ==(X p, X q) => p is null ? q is null : p.Equals(q);
        public static bool operator !=(X p, X q) => !(p == q);
    }

    internal static partial class Recipes
    {
        const string XYZW = "xyzw";
        static readonly string[] Vector = { "add", "mul", "max", "min", "seq", "sgt", "sge", "sne", "frc", "trunc", "floor", "mad", "cndeq", "cndge", "cndgt", "dp4", "dp3", "dp2add", "cube", "max4", "setp_eq_push", "setp_ne_push", "setp_gt_push", "setp_ge_push", "kill_eq", "kill_gt", "kill_ge", "kill_ne", "dst", "mova" };
        static readonly string[] Scalar = { "adds", "adds_prev", "muls", "muls_prev", "muls_prev2", "maxs", "mins", "seqs", "sgts", "sges", "snes", "frcs", "truncs", "floors", "exp", "logc", "log", "rcpc", "rcpf", "rcp", "rsqc", "rsqf", "rsq", "movas", "movasf", "subs", "subs_prev", "setp_eq", "setp_ne", "setp_gt", "setp_ge", "setp_inv", "setp_pop", "setp_clr", "setp_rstr", "kills_eq", "kills_gt", "kills_ge", "kills_ne", "kills_one", "sqrt", "op41", "mulsc", "mulsc", "addsc", "addsc", "subsc", "subsc", "sin", "cos", "retain_prev" };
        static readonly int[] VectorArgs = { 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 3, 3, 3, 3, 2, 2, 3, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2 };

        // ------------------------------------------------------------------------------------- xenos_disasm (the listing text)

        internal sealed class Info
        {
            public byte[] blob, ucode; public uint[] w; public bool pixel;
            public List<Shaders.Constant> constants;
        }

        /// alive_shader.container + xenos_disasm.program_info.
        public static Info Container(byte[] blob)
        {
            Bin.Require(blob.Length >= 48, "Not a Xenos shader container");
            var w = new uint[12]; for (int i = 0; i < 12; i++) w[i] = Bin.U32BE(blob, 4 * i);
            Bin.Require(w[0] == 0x102A1100 || w[0] == 0x102A1101, "Not a Xenos shader container");
            long from = Math.Min(w[1], blob.Length), to = Math.Min((long)w[1] + w[2], blob.Length);
            var ucode = Bin.Slice(blob, (int)from, (int)Math.Max(0, to - from));
            var (_, constants) = Shaders.Ctab(blob, (int)w[4] + 4);
            return new Info { blob = blob, ucode = ucode, w = w, pixel = w[0] == 0x102A1100, constants = constants };
        }

        static string Swizzle(int code)
        {
            var sb = new StringBuilder(4);
            for (int i = 0; i < 4; i++) sb.Append(XYZW[((code >> (2 * i)) + i) & 3]);
            string s = sb.ToString();
            return s == "xyzw" ? "" : "." + s;
        }
        static string Mask(int m) { var sb = new StringBuilder(); for (int i = 0; i < 4; i++) if ((m >> i & 1) != 0) sb.Append(XYZW[i]); return sb.Length > 0 ? sb.ToString() : "_"; }

        static List<string> Alu(uint w0, uint w1, uint w2, bool pixel)
        {
            int vdest = (int)(w0 & 63), absc = (int)((w0 >> 7) & 1);
            int sdest = (int)((w0 >> 8) & 63), export = (int)((w0 >> 15) & 1);
            int vmask = (int)((w0 >> 16) & 15), smask = (int)((w0 >> 20) & 15);
            int vsat = (int)((w0 >> 24) & 1), ssat = (int)((w0 >> 25) & 1), sop = (int)((w0 >> 26) & 63);
            int[] swz = { (int)((w1 >> 16) & 255), (int)((w1 >> 8) & 255), (int)(w1 & 255) };
            int[] neg = { (int)((w1 >> 26) & 1), (int)((w1 >> 25) & 1), (int)((w1 >> 24) & 1) };
            int predCond = (int)((w1 >> 27) & 1), predicated = (int)((w1 >> 28) & 1);
            int[] regs = { (int)((w2 >> 16) & 255), (int)((w2 >> 8) & 255), (int)(w2 & 255) };
            int vop = (int)((w2 >> 24) & 31);
            int[] sel = { (int)((w2 >> 31) & 1), (int)((w2 >> 30) & 1), (int)((w2 >> 29) & 1) };
            string Src(int k)
            {
                string s;
                if (sel[k] != 0) { int r = regs[k]; s = "r" + (r & 63); if ((r & 128) != 0) s = "|" + s + "|"; }
                else { s = "c" + regs[k]; if (absc != 0) s = "|" + s + "|"; }
                return (neg[k] != 0 ? "-" : "") + s + Swizzle(swz[k]);
            }
            string Dest(int reg, int m)
            {
                string name;
                if (export != 0)
                {
                    if (pixel) name = reg == 0 ? "oC0" : reg == 1 ? "oC1" : reg == 2 ? "oC2" : reg == 3 ? "oC3" : reg == 61 ? "oDepth" : "export" + reg;
                    else name = reg == 62 ? "oPos" : reg == 63 ? "oPts" : "o" + reg;
                }
                else name = "r" + reg;
                return name + "." + Mask(m);
            }
            string pre = predicated != 0 ? "(" + (predCond != 0 ? "" : "!") + "p) " : "";
            var lines = new List<string>();
            if (vmask != 0 || (vop >= 20 && vop <= 27))
            {
                int n = VectorArgs[vop];
                string name = vop < Vector.Length ? Vector[vop] : "vop" + vop;
                lines.Add(pre + name + (vsat != 0 ? "_sat" : "") + " " + Dest(vdest, vmask) + ", " + string.Join(", ", Enumerable.Range(0, n).Select(Src)));
            }
            if (sop != 50 || smask != 0)
            {
                string name = sop < Scalar.Length ? Scalar[sop] : "sop" + sop;
                string operand;
                if (sop >= 42 && sop <= 47)
                {
                    int sw = swz[2];
                    char ca = XYZW[((sw >> 6) + 3) & 3], cb = XYZW[sw & 3];
                    int treg = (sop & 1) | (sw & 0x3C) | (sel[2] << 1);
                    operand = (neg[2] != 0 ? "-" : "") + "c" + regs[2] + "." + ca + ", r" + treg + "." + cb;
                }
                else operand = Src(2);
                // an exporting ALU instruction writes both results to the vector destination's export (xenos_disasm.alu)
                string sd = smask != 0 ? Dest(export != 0 ? vdest : sdest, smask) : "ps";
                lines.Add(pre + name + (ssat != 0 ? "_sat" : "") + " " + sd + ", " + operand);
            }
            if (lines.Count == 0) lines.Add(pre + "nop");
            return lines;
        }

        static List<string> Fetch(uint w0, uint w1, uint w2)
        {
            int op = (int)(w0 & 31), srcReg = (int)((w0 >> 5) & 63), dstReg = (int)((w0 >> 12) & 63), cst = (int)((w0 >> 20) & 31);
            const string comp = "xyzw01?_";
            int dsw = (int)(w1 & 0xFFF);
            var comps = new StringBuilder(); for (int i = 0; i < 4; i++) comps.Append(comp[(dsw >> (3 * i)) & 7]);
            if (op == 0)
            {
                cst = (int)(((w0 >> 20) & 31) * 3 + ((w0 >> 25) & 3));
                int fmt = (int)((w1 >> 16) & 63), stride = (int)(w2 & 255), offset = (int)((w2 >> 8) & 0x7FFFFF);
                return new List<string> { $"vfetch r{dstReg}.{comps}, r{srcReg}, vf{cst} (format {fmt}, offset {offset} dwords, stride {stride})" };
            }
            string name = op == 1 ? "tfetch" : op == 16 ? "getBCF" : op == 17 ? "getCompLOD" : op == 18 ? "getGradients" : op == 19 ? "getWeights"
                        : op == 24 ? "setTexLOD" : op == 25 ? "setGradientH" : op == 26 ? "setGradientV" : "fetch" + op;
            int ssw = (int)((w0 >> 26) & 63);
            var scomp = new StringBuilder(); for (int i = 0; i < 3; i++) scomp.Append(XYZW[(ssw >> (2 * i)) & 3]);
            int lodBias = (int)((w2 >> 4) & 0x7F);
            return new List<string> { $"{name} r{dstReg}.{comps}, r{srcReg}.{scomp}, tf{cst}" + (lodBias != 0 ? $" (bias {lodBias})" : "") };
        }

        static readonly HashSet<string> Execs = new HashSet<string> { "exec", "exece", "cexec", "cexece", "cexec_pred", "cexece_pred", "cexec_predclean", "cexece_predclean" };

        /// xenos_disasm.disassemble: the instruction lines ("    {at:3d} {text}"); the header / constant / literal lines match none of the
        /// patterns read from the listing and are left out (their literal reads are checked).
        public static List<string> Disassemble(Info info)
        {
            var w = info.w;
            Bin.Require((long)w[6] + 8 <= info.blob.Length && (long)w[5] + 24 <= info.blob.Length, "Shader container blocks");
            uint litBytes = Bin.U32BE(info.blob, (int)w[6]), insBytes = Bin.U32BE(info.blob, (int)w[6] + 4);
            Bin.Require(Math.Min(litBytes, (uint)info.ucode.Length) >= litBytes / 16 * 16, "Literal constants beyond the microcode");
            long from = Math.Min(litBytes, info.ucode.Length), to = Math.Min((long)litBytes + insBytes, info.ucode.Length);
            int n = (int)Math.Max(0, (to - from) / 4);
            var words = new uint[n]; for (int i = 0; i < n; i++) words[i] = Bin.U32BE(info.ucode, (int)from + 4 * i);
            var lines = new List<string>();
            foreach (var cf in Shaders.ControlFlow(words))
            {
                if (!Execs.Contains(cf.op)) continue;
                for (int i = 0; i < cf.count; i++)
                {
                    int at = cf.address + i;
                    if (3L * at + 2 >= words.Length) { lines.Add("    <outside>"); continue; }
                    bool isFetch = ((cf.sequence >> (2 * i)) & 1) != 0;
                    var ins = isFetch ? Fetch(words[3 * at], words[3 * at + 1], words[3 * at + 2]) : Alu(words[3 * at], words[3 * at + 1], words[3 * at + 2], info.pixel);
                    foreach (var l in ins) lines.Add("    " + at.ToString().PadLeft(3) + " " + l);
                }
            }
            return lines;
        }

        // ------------------------------------------------------------------------------------- expressions

        static X K(double v) => new X("k", v);
        static bool IsK(X e, double? v = null) => e.t == "k" && (v == null || e.D(1) == v.Value);

        static X Op(string name, params X[] args)
        {
            switch (name)
            {
                case "neg":
                {
                    var a = args[0];
                    if (IsK(a)) return K(-a.D(1));
                    if (a.t == "neg") return a.Sub(1);
                    return new X("neg", a);
                }
                case "abs":
                {
                    var a = args[0];
                    if (IsK(a)) return K(Math.Abs(a.D(1)));
                    if (a.t == "abs") return a;
                    return new X("abs", a);
                }
                case "add":
                {
                    X a = args[0], b = args[1];
                    if (IsK(a) && IsK(b)) return K(a.D(1) + b.D(1));
                    if (IsK(a, 0.0)) return b;
                    if (IsK(b, 0.0)) return a;
                    return new X("add", a, b);
                }
                case "mul":
                {
                    X a = args[0], b = args[1];
                    if (IsK(a) && IsK(b)) return K(a.D(1) * b.D(1));
                    if (IsK(a, 0.0) || IsK(b, 0.0)) return K(0.0);
                    if (IsK(a, 1.0)) return b;
                    if (IsK(b, 1.0)) return a;
                    return new X("mul", a, b);
                }
                case "mad": return Op("add", Op("mul", args[0], args[1]), args[2]);
            }
            return new X(name, args.Cast<object>().ToArray());
        }

        internal class Machine
        {
            public readonly Dictionary<(int, int), X> r = new Dictionary<(int, int), X>();
            public readonly Dictionary<int, double[]> literals;
            public readonly Dictionary<(string, int), X> outv = new Dictionary<(string, int), X>();
            public readonly List<(string, int)> outOrder = new List<(string, int)>();      // outv's keys in first-write order (Python dict order)
            public X ps = K(0.0);
            public Machine(Dictionary<int, double[]> literals) { this.literals = literals; }

            protected X Reg(int i, int c) => r.TryGetValue((i, c), out var v) ? v : new X("in", i, c);
            X Const(int i, int c) => literals.TryGetValue(i, out var l) ? K(l[c]) : new X("c", i, c);

            static readonly Regex SourceRe = new Regex(@"^(-?)(\|?)([rc])(\d+)\|?(?:\.([xyzw]{1,4}))?\z", RegexOptions.CultureInvariant);
            (char kind, int idx, bool neg, bool ab, string sw) Source(string text)
            {
                var m = SourceRe.Match(text.Trim());
                if (!m.Success) throw new AliveImportException("operand " + text);
                string sw = m.Groups[5].Success && m.Groups[5].Value.Length > 0 ? m.Groups[5].Value : "xyzw";
                sw = (sw + new string(sw[sw.Length - 1], 4)).Substring(0, 4);
                return (m.Groups[3].Value[0], int.Parse(m.Groups[4].Value), m.Groups[1].Value.Length > 0, m.Groups[2].Value.Length > 0, sw);
            }

            X Read((char kind, int idx, bool neg, bool ab, string sw) p, char comp)
            {
                int c = XYZW.IndexOf(comp);
                var e = p.kind == 'r' ? Reg(p.idx, c) : Const(p.idx, c);
                if (p.ab) e = Op("abs", e);
                if (p.neg) e = Op("neg", e);
                return e;
            }

            List<(int, X)> VectorOp(string name, bool sat, string mask, string[] srcs)
            {
                var parsed = srcs.Select(Source).ToList();
                var results = new List<(int, X)>();
                for (int ci = 0; ci < 4; ci++)
                {
                    if (mask.IndexOf(XYZW[ci]) < 0) continue;
                    var a = parsed.Select(p => Read(p, p.sw[ci])).ToList();
                    X v;
                    switch (name)
                    {
                        case "add": v = Op("add", a[0], a[1]); break;
                        case "mul": v = Op("mul", a[0], a[1]); break;
                        case "mad": v = Op("mad", a[0], a[1], a[2]); break;
                        case "max": v = a[0] == a[1] ? a[0] : new X("max", a[0], a[1]); break;
                        case "min": v = a[0] == a[1] ? a[0] : new X("min", a[0], a[1]); break;
                        case "cndeq": case "cndge": case "cndgt":
                        {
                            var cond = a[0];
                            if (IsK(cond))
                            {
                                double cv = cond.D(1);
                                bool tr = name == "cndeq" ? cv == 0.0 : name == "cndge" ? cv >= 0.0 : cv > 0.0;
                                v = tr ? a[1] : a[2];
                            }
                            else v = new X(name, a[0], a[1], a[2]);
                            break;
                        }
                        case "dp3": case "dp4": case "dp2add":
                        {
                            int n = name == "dp3" ? 3 : name == "dp4" ? 4 : 2;
                            v = K(0.0);
                            for (int k = 0; k < n; k++) v = Op("add", v, Op("mul", Read(parsed[0], parsed[0].sw[k]), Read(parsed[1], parsed[1].sw[k])));
                            if (name == "dp2add") v = Op("add", v, Read(parsed[2], parsed[2].sw[0]));
                            break;
                        }
                        default: v = new X(name, a.Cast<object>().ToArray()); break;
                    }
                    results.Add((ci, sat ? Op("sat", v) : v));
                }
                return results;
            }

            public X ScalarOp(string name, bool sat, string operand)
            {
                string bas = name.EndsWith("_sat", StringComparison.Ordinal) ? name.Substring(0, name.Length - 2) : name;
                X v;
                if (bas == "mulsc" || bas == "addsc" || bas == "subsc")
                {
                    var parts = operand.Split(',').Select(s => s.Trim()).ToArray();
                    if (parts.Length != 2) throw new AliveImportException("operand " + operand);
                    var pc = Source(parts[0]); var pr = Source(parts[1]);
                    X a = Read(pc, pc.sw[0]), b = Read(pr, pr.sw[0]);
                    v = bas == "mulsc" ? Op("mul", a, b) : bas == "addsc" ? Op("add", a, b) : Op("add", a, Op("neg", b));
                }
                else
                {
                    var p = Source(operand);
                    X a = Read(p, p.sw[3]), b = Read(p, p.sw[0]);
                    switch (bas)
                    {
                        case "adds": v = Op("add", a, b); break;
                        case "adds_prev": v = Op("add", a, ps); break;
                        case "muls": v = Op("mul", a, b); break;
                        case "muls_prev": v = Op("mul", a, ps); break;
                        case "subs": v = Op("add", a, Op("neg", b)); break;
                        case "maxs": v = a == b ? a : new X("max", a, b); break;
                        case "mins": v = a == b ? a : new X("min", a, b); break;
                        default: v = new X(bas, a); break;
                    }
                }
                v = sat ? Op("sat", v) : v;
                ps = v;
                return v;
            }

            static readonly Regex LineRe = new Regex(@"^\s+(\d+)\s+(.*)$", RegexOptions.CultureInvariant);
            static readonly Regex FetchRe = new Regex(@"^tfetch r(\d+)\.(\S{4}), r(\d+)\.([xyzw]{3}), tf(\d+)", RegexOptions.CultureInvariant);
            static readonly Regex PsRe = new Regex(@"^(\w+?)(_sat)? ps, (.*)$", RegexOptions.CultureInvariant);
            static readonly Regex InsRe = new Regex(@"^(\w+?)(_sat)? (o\w+|r\d+)\.(\w+), (.*)$", RegexOptions.CultureInvariant);
            static readonly Regex SplitRe = new Regex(@",\s*", RegexOptions.CultureInvariant);
            static readonly HashSet<string> ScalarNames = new HashSet<string> { "adds", "adds_prev", "muls", "muls_prev", "maxs", "mins", "subs", "mulsc", "addsc", "subsc",
                "rcp", "rcpc", "rcpf", "rsq", "rsqc", "rsqf", "sqrt", "exp", "log", "logc", "frcs", "floors", "truncs", "sin", "cos", "sgts", "sges", "seqs", "snes" };

            public Machine Run(List<string> lines)
            {
                var groups = new List<(string at, List<string> texts)>();
                foreach (var line in lines)
                {
                    var m = LineRe.Match(line);
                    if (!m.Success) continue;
                    if (groups.Count > 0 && groups[groups.Count - 1].at == m.Groups[1].Value) groups[groups.Count - 1].texts.Add(m.Groups[2].Value);
                    else groups.Add((m.Groups[1].Value, new List<string> { m.Groups[2].Value }));
                }
                foreach (var (_, texts) in groups)
                {
                    var pending = new List<((string, int), X)>();
                    foreach (var text in texts) pending.AddRange(Step(text));
                    foreach (var (key, v) in pending)
                    {
                        if (key.Item1.StartsWith("o", StringComparison.Ordinal)) { if (!outv.ContainsKey(key)) outOrder.Add(key); outv[key] = v; }
                        else r[(int.Parse(key.Item1.Substring(1)), key.Item2)] = v;
                    }
                }
                return this;
            }

            protected virtual List<((string, int), X)> Step(string text)
            {
                var none = new List<((string, int), X)>();
                if (text.StartsWith("(", StringComparison.Ordinal) || text.StartsWith("nop", StringComparison.Ordinal) || text.StartsWith("retain_prev", StringComparison.Ordinal)) return none;
                if (text.StartsWith("tfetch", StringComparison.Ordinal))
                {
                    var fm = FetchRe.Match(text);
                    if (!fm.Success) throw new AliveImportException("fetch " + text);
                    string dst = fm.Groups[1].Value, dsw = fm.Groups[2].Value, ssw = fm.Groups[4].Value; int src = int.Parse(fm.Groups[3].Value), tf = int.Parse(fm.Groups[5].Value);
                    var coord = new X("()", Reg(src, XYZW.IndexOf(ssw[0])), Reg(src, XYZW.IndexOf(ssw[1])));
                    var o = new List<((string, int), X)>();
                    for (int ci = 0; ci < dsw.Length; ci++)
                    {
                        char ch = dsw[ci];
                        if (XYZW.IndexOf(ch) >= 0) o.Add((("r" + dst, ci), new X("tex", tf, coord, XYZW.IndexOf(ch))));
                        else if (ch == '0' || ch == '1') o.Add((("r" + dst, ci), K(ch - '0')));
                    }
                    return o;
                }
                var pm = PsRe.Match(text);
                if (pm.Success) { ScalarOp(pm.Groups[1].Value, pm.Groups[2].Success && pm.Groups[2].Length > 0, pm.Groups[3].Value); return none; }
                var im = InsRe.Match(text);
                if (!im.Success) return none;
                string name = im.Groups[1].Value, dest = im.Groups[3].Value, mask = im.Groups[4].Value, rest = im.Groups[5].Value;
                bool sat = im.Groups[2].Success && im.Groups[2].Length > 0;
                if (mask == "_") mask = "";
                var outl = new List<((string, int), X)>();
                if (ScalarNames.Contains(name))
                {
                    var v = ScalarOp(name, sat, rest);
                    var idx = new List<int>(); foreach (char ch in mask) { int i = XYZW.IndexOf(ch); if (i < 0) throw new AliveImportException("mask " + mask); if (!idx.Contains(i)) idx.Add(i); }
                    foreach (int i in idx) outl.Add(((dest, i), v));
                }
                else foreach (var (ci, v) in VectorOp(name, sat, mask, SplitRe.Split(rest))) outl.Add(((dest, ci), v));
                return outl;
            }
        }

        static List<X> MulFactors(X e)
        {
            if (e.t == "mul") { var l = MulFactors(e.Sub(1)); l.AddRange(MulFactors(e.Sub(2))); return l; }
            return new List<X> { e };
        }

        // the Python tuple traversal "for y in x[1:] if isinstance(y, tuple)": a coordinate pair exposes only its second element
        static IEnumerable<X> Children(X x)
        {
            if (x.IsPair) { if (x.a[1] is X c1) yield return c1; yield break; }
            foreach (var v in x.a) if (v is X c) yield return c;
        }

        const int OP_FETCH = 0, OP_VCOL = 1, OP_CONST = 2, OP_ADD = 3, OP_MUL = 4, OP_NEG = 5, OP_ABS = 6, OP_SAT = 7, OP_LERP = 8, OP_MAX = 9, OP_MIN = 10, OP_PARAM = 11;
        const int MAX_PARAMS = 4;
        const int COORD_UV0 = 0, COORD_UV1 = 1, COORD_DIST_SMOOTH = 2, COORD_DIST_LINEAR = 3, COORD_VIEW = 4, SWIZZLE_RGB = 4, MAX_OPS = 48, MAX_REGS = 12, MAX_SLOTS = 6;

        static double Evaluate(X e, Dictionary<X, double> env)
        {
            switch (e.t)
            {
                case "k": return e.D(1);
                case "in": case "c": if (!env.TryGetValue(e, out var v)) throw new KeyNotFoundException(); return v;
                case "add": return Evaluate(e.Sub(1), env) + Evaluate(e.Sub(2), env);
                case "mul": return Evaluate(e.Sub(1), env) * Evaluate(e.Sub(2), env);
                case "neg": return -Evaluate(e.Sub(1), env);
                case "abs": return Math.Abs(Evaluate(e.Sub(1), env));
                case "sat": return Math.Min(1.0, Math.Max(0.0, Evaluate(e.Sub(1), env)));
                case "sqrt": { double x = Evaluate(e.Sub(1), env); if (x < 0) throw new AliveImportException("math domain error"); return Math.Sqrt(x); }
                case "rsq":
                {
                    double x = Evaluate(e.Sub(1), env);
                    if (x < 0) throw new AliveImportException("math domain error");
                    double r = Math.Sqrt(x);
                    if (r == 0) throw new DivideByZeroException();
                    return 1.0 / r;
                }
                case "max": { double p = Evaluate(e.Sub(1), env), q = Evaluate(e.Sub(2), env); return PyMax(p, q); }
                case "min": { double p = Evaluate(e.Sub(1), env), q = Evaluate(e.Sub(2), env); return PyMin(p, q); }
            }
            throw new AliveImportException("cannot evaluate " + e.t);
        }
        // Python's max(a, b) / min(a, b): the first unless the second compares greater / smaller (NaN-faithful)
        static double PyMax(double p, double q) => q > p ? q : p;
        static double PyMin(double p, double q) => q < p ? q : p;

        /// alive_recipe.distance_coordinate: (mode, k) when the coordinate is smoothstep(sat(k d)) or sat(k d) of the camera distance.
        static (int mode, double k)? DistanceCoordinate(X e)
        {
            var found = new List<double>();
            void Walk(X x)
            {
                if (x.t == "sat" && x.Sub(1) is X m && m.t == "mul" && m.Sub(1) is X k && IsK(k) && m.Sub(2) is X s && s.t == "sqrt") found.Add(k.D(1));
                foreach (var c in Children(x)) Walk(c);
            }
            Walk(e);
            if (found.Count == 0 || found.Distinct().Count() != 1) return null;
            double kk = found[0];
            var rng = new PyRandom(7);
            bool smooth = true, linear = true;
            for (int it = 0; it < 8; it++)
            {
                var env = new Dictionary<X, double>();
                var pos = new double[4]; var cam = new double[4];
                for (int i = 0; i < 4; i++) pos[i] = rng.Uniform(-400, 400);
                for (int i = 0; i < 4; i++) cam[i] = rng.Uniform(-400, 400);
                for (int i = 0; i < 4; i++) { env[new X("in", 0, i)] = pos[i]; env[new X("c", 8, i)] = cam[i]; }
                double sum = 0; for (int i = 0; i < 4; i++) sum += (cam[i] - pos[i]) * (cam[i] - pos[i]);
                double dist = Math.Pow(sum, 0.5);
                double v;
                try { v = Evaluate(e, env); }
                catch (KeyNotFoundException) { return null; }
                catch (AliveImportException) { return null; }
                double sv = Math.Min(1.0, Math.Max(0.0, kk * dist));
                smooth &= Math.Abs(v - sv * sv * (3 - 2 * sv)) < 1e-4;
                linear &= Math.Abs(v - sv) < 1e-4;
            }
            return smooth ? (COORD_DIST_SMOOTH, kk) : linear ? (COORD_DIST_LINEAR, kk) : ((int, double)?)null;
        }

        /// alive_recipe.view_coordinate: a coordinate that is 1 - |N.V| (N the one interpolator other than r0, V the direction to the
        /// camera), checked numerically on random positions, cameras and normals.
        static bool ViewCoordinate(X e)
        {
            var regs = new HashSet<int>();
            void Walk(X x)
            {
                if (x.t == "in" && x.I(1) != 0) regs.Add(x.I(1));
                foreach (var c in Children(x)) Walk(c);
            }
            Walk(e);
            if (regs.Count != 1) return false;
            int n = regs.First();
            var rng = new PyRandom(11);
            for (int it = 0; it < 8; it++)
            {
                var env = new Dictionary<X, double>();
                var pos = new double[4]; var cam = new double[4]; var nrm = new double[4];
                for (int i = 0; i < 4; i++) pos[i] = rng.Uniform(-400, 400);
                for (int i = 0; i < 4; i++) cam[i] = rng.Uniform(-400, 400);
                for (int i = 0; i < 4; i++) nrm[i] = rng.Uniform(-1, 1);
                for (int i = 0; i < 4; i++) { env[new X("in", 0, i)] = pos[i]; env[new X("c", 8, i)] = cam[i]; env[new X("in", n, i)] = nrm[i]; }
                var v = new double[4]; for (int i = 0; i < 4; i++) v[i] = cam[i] - pos[i];
                double sv = 0; for (int i = 0; i < 4; i++) sv += v[i] * v[i];
                double sn = 0; for (int i = 0; i < 3; i++) sn += nrm[i] * nrm[i];
                double lv = Math.Pow(sv, 0.5), ln = Math.Pow(sn, 0.5);
                double dot = 0; for (int i = 0; i < 3; i++) dot += v[i] / lv * nrm[i] / ln;
                double want = 1 - Math.Abs(dot), got;
                try { got = Evaluate(e, env); }
                catch (KeyNotFoundException) { return false; }
                catch (AliveImportException) { return false; }
                catch (DivideByZeroException) { return false; }
                if (Math.Abs(got - want) > 1e-4) return false;
            }
            return true;
        }

        sealed class Compiler
        {
            public readonly List<int> vcolour = new List<int>(); public readonly HashSet<int> uvSeen = new HashSet<int>();
            public string reason;
            // the program's material parameters (CTAB usr_* float4s: register -> name) and those it reads, in first-use order
            public readonly Dictionary<int, string> paramNames = new Dictionary<int, string>();
            public readonly List<string> paramsUsed = new List<string>();

            int ParamIndex(int register)
            {
                if (!paramNames.TryGetValue(register, out var name)) throw new AliveImportException("constant c" + register);
                if (!paramsUsed.Contains(name)) paramsUsed.Add(name);
                if (paramsUsed.Count > MAX_PARAMS) throw new AliveImportException("parameters");
                return paramsUsed.IndexOf(name);
            }

            /// alive_recipe.coordinate: (mode, tile u, tile v, offset parameter + 1); UV coordinates may be offset by a material
            /// parameter's (x, y).
            (double mode, double tu, double tv, double offset)? Coordinate(X coords)
            {
                var offset = new int?[2]; var stripped = new X[2];
                for (int i = 0; i < 2; i++)
                {
                    X c = coords.Sub(i + 1);
                    if (c.t == "add")
                        foreach (var (a, b) in new[] { (c.Sub(1), c.Sub(2)), (c.Sub(2), c.Sub(1)) })
                            if (b.t == "c" && b.I(2) == i) { offset[i] = b.I(1); c = a; break; }
                    stripped[i] = c;
                }
                if (offset[0] != offset[1]) return null;
                if (offset[0] != null)
                {
                    int k;
                    try { k = ParamIndex(offset[0].Value); }
                    catch (AliveImportException) { return null; }
                    var bas = Coordinate(new X("()", stripped[0], stripped[1]));
                    if (bas == null || bas.Value.mode > COORD_UV1 || bas.Value.offset != 0) return null;
                    return (bas.Value.mode, bas.Value.tu, bas.Value.tv, k + 1);
                }
                var uv = new List<(int reg, int set)>(); var tile = new List<double>(); var comps = new List<int>(); bool isUv = true;
                foreach (X c0 in new[] { coords.Sub(1), coords.Sub(2) })
                {
                    X c = c0; double scale = 1.0;
                    while (c.t == "mul" && (IsK(c.Sub(1)) || IsK(c.Sub(2))))
                    {
                        X k; if (IsK(c.Sub(1))) { k = c.Sub(1); c = c.Sub(2); } else { k = c.Sub(2); c = c.Sub(1); }
                        scale *= k.D(1);
                    }
                    if (c.t != "in") { isUv = false; break; }
                    int comp = c.I(2);
                    uv.Add((c.I(1), FloorDiv(comp, 2))); tile.Add(scale); comps.Add(PyMod(comp, 2));
                }
                if (isUv)
                {
                    if (uv[0] != uv[1] || comps[0] != 0 || comps[1] != 1) return null;
                    uvSeen.Add(uv[0].reg);
                    return (COORD_UV0 + uv[0].set, tile[0], tile[1], 0);
                }
                if (coords.Sub(1) == coords.Sub(2))
                {
                    var d = DistanceCoordinate(coords.Sub(1));
                    if (d != null) return (d.Value.mode, d.Value.k, d.Value.k, 0);
                    if (ViewCoordinate(coords.Sub(1))) return (COORD_VIEW, 1.0, 1.0, 0);
                }
                return null;
            }
            static int FloorDiv(int a, int b) => (int)Math.Floor((double)a / b);
            static int PyMod(int a, int b) => ((a % b) + b) % b;

            public sealed class Result { public List<double[]> ops; public int result; public List<int> slots; public bool lit; public List<string> parameters; public int alpha = -1; }

            public Result Compile(X tree, Dictionary<int, int> slots, X alpha)
            {
                var ops = new List<double[]>(); var regs = new Dictionary<X, int>(); var used = new List<int>();
                vcolour.Clear(); uvSeen.Clear();
                int Emit(int code, double a = 0, double b = 0, double p0 = 0, double p1 = 0, double p2 = 0, double p3 = 0)
                {
                    int r = ops.Count; ops.Add(new double[] { code, r, a, b, p0, p1, p2, p3 }); return r;
                }
                int Node(X e)
                {
                    if (regs.TryGetValue(e, out int have)) return have;
                    int r;
                    switch (e.t)
                    {
                        case "tex":
                        {
                            int sampler = e.I(1);
                            if (!slots.ContainsKey(sampler)) throw new AliveImportException("sampler");
                            var coord = Coordinate(e.Sub(2));
                            if (coord == null) throw new AliveImportException("coordinate");
                            int slot = slots[sampler];
                            if (!used.Contains(slot)) used.Add(slot);
                            r = Emit(OP_FETCH, used.IndexOf(slot), e.D(3), coord.Value.mode, coord.Value.tu, coord.Value.tv, coord.Value.offset);
                            break;
                        }
                        case "in":
                            if (vcolour.Count > 0 && vcolour[0] != e.I(1)) throw new AliveImportException($"inputs r{vcolour[0]} and r{e.I(1)}");
                            if (vcolour.Count == 0) vcolour.Add(e.I(1)); else vcolour[0] = e.I(1);
                            r = Emit(OP_VCOL, 0, e.D(2));
                            break;
                        case "k": r = Emit(OP_CONST, 0, 0, e.D(1), 0, 0, 0); break;
                        case "c": { int pi = ParamIndex(e.I(1)); r = Emit(OP_PARAM, pi, e.D(2)); break; }
                        case "add": case "mul": { int a = Node(e.Sub(1)), b = Node(e.Sub(2)); r = Emit(e.t == "add" ? OP_ADD : OP_MUL, a, b); break; }
                        case "lerp": { int a = Node(e.Sub(1)), b = Node(e.Sub(2)), w = Node(e.Sub(3)); r = Emit(OP_LERP, a, b, w, 0, 0, 0); break; }
                        case "max": case "min": { int a = Node(e.Sub(1)), b = Node(e.Sub(2)); r = Emit(e.t == "max" ? OP_MAX : OP_MIN, a, b); break; }
                        case "neg": case "abs": case "sat": { int a = Node(e.Sub(1)); r = Emit(e.t == "neg" ? OP_NEG : e.t == "abs" ? OP_ABS : OP_SAT, a); break; }
                        default: throw new AliveImportException("op " + e.t);
                    }
                    regs[e] = r;
                    return r;
                }
                int result;
                try { result = Node(tree); }
                catch (AliveImportException ex) { reason = ex.Message; return null; }
                int? alphaReg = null;
                if (alpha != null)
                {
                    // the alpha after the colour; one that does not compile leaves the colour as it was (the client keeps its own alpha)
                    int n0 = ops.Count; var regs0 = new Dictionary<X, int>(regs); var used0 = new List<int>(used); var vcolour0 = new List<int>(vcolour); var params0 = new List<string>(paramsUsed);
                    try { alphaReg = Node(alpha); }
                    catch (AliveImportException)
                    {
                        ops.RemoveRange(n0, ops.Count - n0); regs.Clear(); foreach (var kv in regs0) regs[kv.Key] = kv.Value;
                        used.Clear(); used.AddRange(used0); vcolour.Clear(); vcolour.AddRange(vcolour0); paramsUsed.Clear(); paramsUsed.AddRange(params0);
                        alphaReg = null;
                    }
                }
                if (ops.Count > MAX_OPS || used.Count > MAX_SLOTS || (vcolour.Count > 0 && uvSeen.Contains(vcolour[0]))) return null;
                // linear-scan register allocation: a value's register is free after its last reader
                int[] Reads(int code)
                {
                    switch (code)
                    {
                        case OP_ADD: case OP_MUL: case OP_MAX: case OP_MIN: return new[] { 2, 3 };
                        case OP_NEG: case OP_ABS: case OP_SAT: return new[] { 2 };
                        case OP_LERP: return new[] { 2, 3, 4 };
                    }
                    return Array.Empty<int>();
                }
                var last = new Dictionary<int, int>();
                for (int i = 0; i < ops.Count; i++) foreach (int k in Reads((int)ops[i][0])) last[(int)ops[i][k]] = i;
                last[result] = ops.Count;
                if (alphaReg != null) last[alphaReg.Value] = ops.Count;
                var phys = new Dictionary<int, int>(); var free = new List<int>(); int top = 0;
                for (int i = 0; i < ops.Count; i++)
                {
                    var o = ops[i];
                    var positions = Reads((int)o[0]);
                    var values = positions.Select(k => (int)o[k]).ToList();
                    var srcs = values.Select(v => phys[v]).ToList();
                    foreach (int v in values.Distinct()) if (last[v] == i) free.Add(phys[v]);
                    int r;
                    if (free.Count > 0) { r = free.Min(); free.Remove(r); }
                    else { r = top; top++; }
                    if (top > MAX_REGS) return null;
                    phys[(int)o[1]] = r;
                    o[1] = r;
                    for (int j = 0; j < positions.Length; j++) o[positions[j]] = srcs[j];
                }
                return new Result { ops = ops, result = phys[result], slots = used, parameters = new List<string>(paramsUsed), alpha = alphaReg != null ? phys[alphaReg.Value] : -1 };
            }
        }

        static X Merge(X x, X y, X z)
        {
            if (x.t != y.t || x.t != z.t || x.Len != y.Len || x.Len != z.Len) throw new AliveImportException("channel trees differ");
            if (x.t == "tex")
            {
                if (!(x.I(1) == y.I(1) && x.I(1) == z.I(1)) || x.Sub(2) != y.Sub(2) || x.Sub(2) != z.Sub(2)) throw new AliveImportException("channel fetches differ");
                if (x.I(3) == 0 && y.I(3) == 1 && z.I(3) == 2) return new X("tex", x[1], x[2], SWIZZLE_RGB);
                if (x.I(3) == y.I(3) && y.I(3) == z.I(3)) return x;
                throw new AliveImportException("fetch swizzle");
            }
            if (x.t == "in")
            {
                if (x.I(2) == 0 && y.I(2) == 1 && z.I(2) == 2 && x.I(1) == y.I(1) && y.I(1) == z.I(1)) return new X("in", x[1], SWIZZLE_RGB);
                if (x == y && y == z) return x;
                throw new AliveImportException("input swizzle");
            }
            if (x.t == "c")
            {
                if (x.I(1) == y.I(1) && y.I(1) == z.I(1) && x.I(2) == 0 && y.I(2) == 1 && z.I(2) == 2) return new X("c", x[1], SWIZZLE_RGB);
                if (x == y && y == z) return x;
                throw new AliveImportException("per-channel constant");
            }
            if (x.t == "k")
            {
                if (x == y && y == z) return x;
                throw new AliveImportException("per-channel constant");
            }
            var items = new object[x.a.Length];
            for (int i = 0; i < items.Length; i++)
                items[i] = x.a[i] is X xa ? (object)MergeItem(xa, y.a[i], z.a[i]) : x.a[i];
            return new X(x.t, items);
        }
        static X MergeItem(X a, object b, object c)
        {
            if (!(b is X bx) || !(c is X cx)) throw new AliveImportException("channel trees differ");
            if (a.IsPair || bx.IsPair || cx.IsPair) throw new AliveImportException("channel trees differ");
            return Merge(a, bx, cx);
        }

        static X FoldLerps(X e)
        {
            if (e.t == "tex" || e.t == "in" || e.t == "k" || e.t == "c" || e.IsPair) return e;
            e = new X(e.t, e.a.Select(v => v is X c ? (object)FoldLerps(c) : v).ToArray());
            if (e.t == "add")
            {
                foreach (var (m, a) in new[] { (e.Sub(1), e.Sub(2)), (e.Sub(2), e.Sub(1)) })
                {
                    if (m.t != "mul") continue;
                    foreach (var (d, w) in new[] { (m.Sub(1), m.Sub(2)), (m.Sub(2), m.Sub(1)) })
                    {
                        if (d.t != "add") continue;
                        var na = new X("neg", a);
                        if (d.Sub(2) == na) return new X("lerp", a, d.Sub(1), w);
                        if (d.Sub(1) == na) return new X("lerp", a, d.Sub(2), w);
                    }
                }
            }
            return e;
        }

        static X FindAlbedo(X e, int light)
        {
            var stack = new List<X> { e };
            while (stack.Count > 0)
            {
                var x = stack[stack.Count - 1]; stack.RemoveAt(stack.Count - 1);
                if (x.t == "mul")
                {
                    foreach (var (sq, other) in new[] { (x.Sub(1), x.Sub(2)), (x.Sub(2), x.Sub(1)) })
                    {
                        var lit = MulFactors(other).Where(f => !(f.t == "in" && f.I(2) == 3)).ToList();
                        if (sq.t == "mul" && sq.Sub(1).t == "abs" && sq.Sub(1) == sq.Sub(2) && lit.Count == 1 && lit[0].t == "tex" && lit[0].I(1) == light)
                            return sq.Sub(1).Sub(1);
                    }
                }
                stack.AddRange(Children(x));
            }
            return null;
        }

        static X UnlitColour(X e, int channel)
        {
            if (e.t != "mul") return null;
            var g = new X("c", 43, 0);
            X a = e.Sub(2) == g ? e.Sub(1) : e.Sub(1) == g ? e.Sub(2) : null;
            if (a == null) return null;
            if (a.t == "add")
            {
                foreach (var (m, f) in new[] { (a.Sub(1), a.Sub(2)), (a.Sub(2), a.Sub(1)) })
                    if (f.t == "in" && f.I(2) == channel && m.t == "mul")
                        foreach (var (z, w) in new[] { (m.Sub(1), m.Sub(2)), (m.Sub(2), m.Sub(1)) })
                            if (w == new X("in", f[1], 3)) return z;
            }
            return a;
        }

        static readonly Regex VfetchRe = new Regex(@"vfetch r(\d+)\.xyzw,", RegexOptions.CultureInvariant);
        static readonly Regex Dp4Re = new Regex(@"dp4 r\d+\.\w, r(\d+)\.\w+, c6[4-7]", RegexOptions.CultureInvariant);
        static readonly Regex MaxOutRe = new Regex(@"max o(\d+)\.xyzw, r(\d+), r(\d+)$", RegexOptions.CultureInvariant);

        /// alive_recipe.vertex_colour_outputs: interpolators that copy a 4-component vertex element other than the position.
        static HashSet<int> VertexColourOutputs(byte[] vertexBlob)
        {
            var lines = Disassemble(Container(vertexBlob));
            var fetched4 = new HashSet<int>(); var position = new HashSet<int>(); var outs = new HashSet<int>();
            foreach (var line in lines)
            {
                var m = VfetchRe.Match(line); if (m.Success) fetched4.Add(int.Parse(m.Groups[1].Value));
                m = Dp4Re.Match(line); if (m.Success) position.Add(int.Parse(m.Groups[1].Value));
            }
            foreach (var line in lines)
            {
                var m = MaxOutRe.Match(line.Trim());
                if (m.Success && m.Groups[2].Value == m.Groups[3].Value && fetched4.Contains(int.Parse(m.Groups[2].Value)) && !position.Contains(int.Parse(m.Groups[2].Value)))
                    outs.Add(int.Parse(m.Groups[1].Value));
            }
            return outs;
        }

        /// alive_recipe.recipe: the albedo program of a SharedPixelShader container (with its StaticVertexShader), or null.
        public static JObj Recipe(byte[] container, byte[] vertex)
        {
            var info = Container(container);
            var w = info.w;
            Bin.Require((long)w[6] + 8 <= container.Length && (long)w[5] + 24 <= container.Length, "Shader container blocks");
            uint litBytes = Bin.U32BE(container, (int)w[6]);
            int litBase = (int)((Bin.U32BE(container, (int)w[5] + 20) >> 16) & 0xFF);
            var literals = new Dictionary<int, double[]>();
            for (int k = 0; k < litBytes / 16; k++)
            {
                Bin.Require(16 * k + 16 <= info.ucode.Length, "Literal constants beyond the microcode");
                literals[litBase + k] = new double[] { Bin.F32BE(info.ucode, 16 * k), Bin.F32BE(info.ucode, 16 * k + 4), Bin.F32BE(info.ucode, 16 * k + 8), Bin.F32BE(info.ucode, 16 * k + 12) };
            }
            var samplers = info.constants.Where(c => c.regset == "sampler").Select(c => (c.index, c.name))
                .OrderBy(s => s.index).ThenBy(s => s.name, StringComparer.Ordinal).ToList();
            var user = samplers.Where(s => !(s.name.StartsWith("sampler", StringComparison.Ordinal) && s.name.Length > 7 && char.IsUpper(s.name[7]))).ToList();
            var slots = new Dictionary<int, int>();
            for (int k = 0; k < user.Count; k++) slots[user[k].index] = k;
            int? light = samplers.Where(s => s.name == "samplerLightBuffer").Select(s => (int?)s.index).FirstOrDefault();
            var compiler = new Compiler();
            foreach (var c in info.constants)
                if (c.regset == "float4" && c.name.StartsWith("usr_", StringComparison.Ordinal) && c.count == 1) compiler.paramNames[c.index] = c.name;
            var m = new Machine(literals).Run(Disassemble(info));
            var trees = new X[3];
            for (int c = 0; c < 3; c++) { if (!m.outv.TryGetValue(("oC0", c), out trees[c])) return null; }
            var ys = new X[3];
            for (int c = 0; c < 3; c++) ys[c] = light == null ? UnlitColour(trees[c], c) : FindAlbedo(trees[c], light.Value);
            if (ys.Any(y => y == null)) return null;
            X merged;
            try { merged = FoldLerps(Merge(ys[0], ys[1], ys[2])); }
            catch (AliveImportException) { return null; }
            X alpha = m.outv.TryGetValue(("oC0", 3), out var at) ? FoldLerps(at) : null;
            var r = compiler.Compile(merged, slots, alpha);
            if (r == null) return null;
            if (compiler.vcolour.Count > 0 && (vertex == null || !VertexColourOutputs(vertex).Contains(compiler.vcolour[0]))) return null;
            r.lit = light != null;
            var ops = new List<object>();
            foreach (var o in r.ops) foreach (var v in o) ops.Add(v);
            return new JObj().Set("ops", ops).Set("result", (long)r.result).Set("slots", r.slots.Select(s => (object)(long)s).ToList()).Set("lit", r.lit)
                .Set("params", r.parameters.Cast<object>().ToList()).Set("alpha", (long)r.alpha);
        }
    }

    /// Python's random.Random(seed) for an int seed (MT19937, init_by_array) and its random() / uniform().
    internal sealed class PyRandom
    {
        readonly uint[] mt = new uint[624]; int mti;
        public PyRandom(uint seed)
        {
            InitGenrand(19650218u);
            uint[] key = { seed };
            int i = 1, j = 0, k = Math.Max(624, key.Length);
            for (; k > 0; k--)
            {
                mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint)j;
                i++; j++;
                if (i >= 624) { mt[0] = mt[623]; i = 1; }
                if (j >= key.Length) j = 0;
            }
            for (k = 623; k > 0; k--)
            {
                mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941u)) - (uint)i;
                i++;
                if (i >= 624) { mt[0] = mt[623]; i = 1; }
            }
            mt[0] = 0x80000000u;
        }
        void InitGenrand(uint s)
        {
            mt[0] = s;
            for (mti = 1; mti < 624; mti++) mt[mti] = 1812433253u * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + (uint)mti;
        }
        uint Next()
        {
            if (mti >= 624)
            {
                int kk;
                for (kk = 0; kk < 624 - 397; kk++) { uint y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu); mt[kk] = mt[kk + 397] ^ (y >> 1) ^ ((y & 1) != 0 ? 0x9908b0dfu : 0u); }
                for (; kk < 623; kk++) { uint y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu); mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ ((y & 1) != 0 ? 0x9908b0dfu : 0u); }
                uint yy = (mt[623] & 0x80000000u) | (mt[0] & 0x7fffffffu); mt[623] = mt[396] ^ (yy >> 1) ^ ((yy & 1) != 0 ? 0x9908b0dfu : 0u);
                mti = 0;
            }
            uint x = mt[mti++];
            x ^= x >> 11; x ^= (x << 7) & 0x9d2c5680u; x ^= (x << 15) & 0xefc60000u; x ^= x >> 18;
            return x;
        }
        public double Random() { uint a = Next() >> 5, b = Next() >> 6; return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0); }
        public double Uniform(double a, double b) => a + (b - a) * Random();
    }
}
