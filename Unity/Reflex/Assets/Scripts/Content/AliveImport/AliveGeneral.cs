// The general form of Alive's level pixel programs (_re/formats/alive_general.py): the programs AliveRecipe's channel-parallel albedo
// recipe rejects (grass-blended cliffs with a normal-mapped world normal, water planes and glass reflecting a cube map, end-of-world
// backdrops with per-channel constants ...) compiled whole to the client's general scalar / float4 ops (ReflexOriginalScenery _ALIVE_VM,
// general ops 20..46). Ported statement by statement so the sidecar is identical to the reference: the memoisation by object identity
// (id(e) in Python: reference dictionaries here), the dict / list orders, the exact integer LCG and the order the vertex program's
// samples are drawn in, and which failures end a single output (vertex_semantics) or the whole program (analyse).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content.AliveImport
{
    /// alive_general.Unsupported (a ValueError in Python): a program the general form cannot express.
    internal sealed class Unsupported : Exception
    {
        public Unsupported(string message) : base(message) { }
    }

    /// Python's id(e): reference identity of expression nodes.
    internal sealed class RefEq : IEqualityComparer<X>
    {
        public static readonly RefEq I = new RefEq();
        public bool Equals(X a, X b) => ReferenceEquals(a, b);
        public int GetHashCode(X x) => RuntimeHelpers.GetHashCode(x);
    }

    /// (id(x), id(y), id(z)).
    internal struct Ref3 : IEquatable<Ref3>
    {
        readonly X x, y, z;
        public Ref3(X x, X y, X z) { this.x = x; this.y = y; this.z = z; }
        public bool Equals(Ref3 o) => ReferenceEquals(x, o.x) && ReferenceEquals(y, o.y) && ReferenceEquals(z, o.z);
        public override bool Equals(object obj) => obj is Ref3 o && Equals(o);
        public override int GetHashCode() => unchecked((RuntimeHelpers.GetHashCode(x) * 31 + RuntimeHelpers.GetHashCode(y)) * 31 + RuntimeHelpers.GetHashCode(z));
    }

    /// alive_general.Lcg: x = (1103515245 x + 12345) mod 2^31 in exact integer arithmetic.
    internal sealed class Lcg
    {
        long x;
        public Lcg(long seed) { x = seed; }
        public double Uniform(double lo, double hi)
        {
            x = (1103515245L * x + 12345L) % 2147483648L;
            return lo + (hi - lo) * (x / 2147483648.0);
        }
    }

    internal static partial class Recipes
    {
        const int MAX_GOPS = 512, MAX_GREGS = 40, MAX_GSLOTS = 12;
        const int IN_P = 0, IN_N = 1, IN_T = 2, IN_B = 3, IN_UV = 4, IN_VCOL = 5, IN_CAM = 6;
        const int G_IN = 20, G_CH = 21, G_K = 22, G_ADD = 23, G_MUL = 24, G_NEG = 25, G_ABS = 26, G_SAT = 27, G_MAX = 28, G_MIN = 29, G_RCP = 30, G_RSQ = 31,
                  G_SQRT = 32, G_FRC = 33, G_FLOOR = 34, G_EXP = 35, G_LOG = 36, G_TRUNC = 37, G_CND = 38, G_SET = 39, G_TEX = 40, G_CUBE = 41, G_PACK = 42,
                  G_PARAM = 43, G_K4 = 44, G_SWZ = 45, G_DOT = 46;

        static readonly Dictionary<string, int> GUnary = new Dictionary<string, int>
        {
            { "rcp", G_RCP }, { "rcpc", G_RCP }, { "rcpf", G_RCP }, { "rsq", G_RSQ }, { "rsqc", G_RSQ }, { "rsqf", G_RSQ }, { "sqrt", G_SQRT }, { "frc", G_FRC },
            { "frcs", G_FRC }, { "floor", G_FLOOR }, { "floors", G_FLOOR }, { "exp", G_EXP }, { "log", G_LOG }, { "logc", G_LOG }, { "trunc", G_TRUNC },
            { "truncs", G_TRUNC }, { "neg", G_NEG }, { "abs", G_ABS }, { "sat", G_SAT },
        };
        static readonly Dictionary<string, int> GBinary = new Dictionary<string, int> { { "add", G_ADD }, { "mul", G_MUL }, { "max", G_MAX }, { "min", G_MIN } };
        static readonly Dictionary<string, int> GSets = new Dictionary<string, int> { { "seq", 0 }, { "sge", 1 }, { "sgt", 2 }, { "sne", 3 } };
        static readonly Dictionary<string, int> GScalarSets = new Dictionary<string, int> { { "seqs", 0 }, { "sges", 1 }, { "sgts", 2 }, { "snes", 3 } };
        static readonly Dictionary<string, int> GCnds = new Dictionary<string, int> { { "cndeq", 0 }, { "cndge", 1 }, { "cndgt", 2 } };

        /// alive_general.eligible: a level's own scenery program (not a shared T_* / Template_* program, terrain, sky, water, grass or tree program).
        public static bool GeneralEligible(string name)
        {
            foreach (var prefix in new[] { "T_", "Template_", "HFB_", "HFT_", "HFTerrain", "SkyEntityComp", "Water_", "GrassShader", "TreeShader", "Editor_" })
                if (name.StartsWith(prefix, StringComparison.Ordinal)) return false;
            return true;
        }

        /// alive_general.analyse: the general program of a SharedPixelShader container (with its StaticVertexShader and NormalPixelShader), or null.
        public static JObj General(byte[] container, byte[] vertex, byte[] normal)
        {
            try
            {
                // a program whose normal pass does not fit the op / register limits is compiled without it
                if (normal != null)
                {
                    try { return CompileProgram(container, vertex, normal); }
                    catch (Unsupported) { }
                }
                return CompileProgram(container, vertex, null);
            }
            catch (Unsupported) { return null; }
            catch (AliveImportException) { return null; }
            catch (KeyNotFoundException) { return null; }
            catch (IndexOutOfRangeException) { return null; }
            catch (ArgumentException) { return null; }       // list index out of range
            catch (OverflowException) { return null; }
            catch (DivideByZeroException) { return null; }
        }

        // ------------------------------------------------------------------------------------- the machine: vertex fetches and cubes

        sealed class GMachine : Machine
        {
            public int fetches;
            readonly HashSet<int> cubeSamplers;
            public GMachine(Dictionary<int, double[]> literals, IEnumerable<int> cubeSamplers = null) : base(literals)
            {
                this.cubeSamplers = new HashSet<int>(cubeSamplers ?? Enumerable.Empty<int>());
            }
            public X RegAt(int i, int c) => Reg(i, c);

            static readonly Regex VfRe = new Regex(@"^vfetch r(\d+)\.(\S{4}),", RegexOptions.CultureInvariant);
            static readonly Regex CubeRe = new Regex(@"^cube r(\d+)\.(\w+), (-?)r(\d+)\.(\w+), (-?)r(\d+)\.(\w+)$", RegexOptions.CultureInvariant);
            static readonly Regex TfRe = new Regex(@"^tfetch r(\d+)\.(\S{4}), r(\d+)\.([xyzw]{3}), tf(\d+)", RegexOptions.CultureInvariant);

            protected override List<((string, int), X)> Step(string text)
            {
                if (text.StartsWith("setTexLOD", StringComparison.Ordinal) || text.StartsWith("getGradients", StringComparison.Ordinal) || text.StartsWith("setGradient", StringComparison.Ordinal)
                    || text.StartsWith("getCompLOD", StringComparison.Ordinal) || text.StartsWith("getWeights", StringComparison.Ordinal) || text.StartsWith("getBCF", StringComparison.Ordinal))
                    return new List<((string, int), X)>();
                if (text.StartsWith("vfetch", StringComparison.Ordinal))
                {
                    var fm = VfRe.Match(text);
                    if (!fm.Success) throw new Unsupported("vfetch " + text);
                    int k = fetches++;
                    var o = new List<((string, int), X)>();
                    string sw = fm.Groups[2].Value;
                    for (int ci = 0; ci < sw.Length; ci++)
                    {
                        char ch = sw[ci];
                        if (XYZW.IndexOf(ch) >= 0) o.Add((("r" + fm.Groups[1].Value, ci), new X("vin", k, XYZW.IndexOf(ch))));
                        else if (ch == '0' || ch == '1') o.Add((("r" + fm.Groups[1].Value, ci), K(ch - '0')));
                    }
                    return o;
                }
                if (text.StartsWith("cube ", StringComparison.Ordinal))
                {
                    var cm = CubeRe.Match(text);
                    if (!cm.Success || cm.Groups[3].Length > 0 || cm.Groups[6].Length > 0 || cm.Groups[4].Value != cm.Groups[7].Value) throw new Unsupported("cube " + text);
                    int src = int.Parse(cm.Groups[4].Value);
                    var direction = new[] { Reg(src, 0), Reg(src, 1), Reg(src, 2) };
                    var o = new List<((string, int), X)>();
                    foreach (char ch in cm.Groups[2].Value)
                        if (XYZW.IndexOf(ch) >= 0) o.Add((("r" + cm.Groups[1].Value, XYZW.IndexOf(ch)), new X("cubeop", direction[0], direction[1], direction[2])));
                    return o;
                }
                if (text.StartsWith("tfetch", StringComparison.Ordinal))
                {
                    var fm = TfRe.Match(text);
                    if (fm.Success && cubeSamplers.Contains(int.Parse(fm.Groups[5].Value)))
                    {
                        string dst = fm.Groups[1].Value, dsw = fm.Groups[2].Value, ssw = fm.Groups[4].Value; int src = int.Parse(fm.Groups[3].Value), tf = int.Parse(fm.Groups[5].Value);
                        var coord = new List<X>();
                        foreach (char ch in ssw) coord.Add(Reg(src, XYZW.IndexOf(ch)));
                        var cube = FindCubeop(coord);
                        if (cube == null) throw new Unsupported("cube fetch without a cube instruction");
                        var o = new List<((string, int), X)>();
                        for (int ci = 0; ci < dsw.Length; ci++)
                        {
                            char ch = dsw[ci];
                            if (XYZW.IndexOf(ch) >= 0) o.Add((("r" + dst, ci), new X("cubetex", tf, cube.a[0], cube.a[1], cube.a[2], XYZW.IndexOf(ch))));
                            else if (ch == '0' || ch == '1') o.Add((("r" + dst, ci), K(ch - '0')));
                        }
                        return o;
                    }
                }
                return base.Step(text);
            }
        }

        static X FindCubeop(IEnumerable<X> items)
        {
            var stack = new List<X>(items);
            var seen = new HashSet<X>(RefEq.I);
            while (stack.Count > 0)
            {
                var e = stack[stack.Count - 1]; stack.RemoveAt(stack.Count - 1);
                if (!seen.Add(e)) continue;
                if (e.t == "cubeop") return e;
                stack.AddRange(Children(e));
            }
            return null;
        }

        // ------------------------------------------------------------------------------------- numeric evaluation

        // Python's float -> int conversions raise: OverflowError for an infinity, ValueError for a NaN (not an Unsupported)
        static double PyFloor(double x)
        {
            if (double.IsNaN(x)) throw new AliveImportException("cannot convert float NaN to integer");
            if (double.IsInfinity(x)) throw new OverflowException("cannot convert float infinity to integer");
            double r = Math.Floor(x); return r == 0 ? 0.0 : r;
        }
        static double PyTrunc(double x)
        {
            if (double.IsNaN(x)) throw new AliveImportException("cannot convert float NaN to integer");
            if (double.IsInfinity(x)) throw new OverflowException("cannot convert float infinity to integer");
            double r = Math.Truncate(x); return r == 0 ? 0.0 : r;
        }
        // 2.0 ** x: NaN / the infinities are returned, a finite exponent whose result overflows raises OverflowError
        static double PyPow2(double x)
        {
            if (double.IsNaN(x)) return x;
            if (double.IsInfinity(x)) return x > 0 ? double.PositiveInfinity : 0.0;
            double r = Math.Pow(2.0, x);
            if (double.IsInfinity(r)) throw new OverflowException("(34, 'Numerical result out of range')");
            return r;
        }

        // math.log2 of a positive value (netstandard 2.1 has no Math.Log2): exact for powers of two, within an ulp or two elsewhere (the
        // values only decide an interpolator's identity at 1e-5)
        static double PyLog2(double x)
        {
            if (double.IsNaN(x) || double.IsPositiveInfinity(x)) return x;
            long bits = BitConverter.DoubleToInt64Bits(x);
            int e = (int)((bits >> 52) & 0x7FF), bias = 1023;
            if (e == 0) { bits = BitConverter.DoubleToInt64Bits(x * 18014398509481984.0); e = (int)((bits >> 52) & 0x7FF); bias += 54; }
            double m = BitConverter.Int64BitsToDouble((bits & 0x000FFFFFFFFFFFFFL) | 0x3FF0000000000000L);
            return m == 1.0 ? e - bias : (e - bias) + Math.Log(m) * 1.4426950408889634;
        }

        /// alive_general.evaluate: the value of an expression; leaf(e) gives the inputs ("k" handled here), memoised by object identity.
        static double GEval(X e, Func<X, double> leaf, Dictionary<X, double> memo)
        {
            if (memo.TryGetValue(e, out double have)) return have;
            string t = e.t;
            double v;
            if (t == "k") v = e.D(1);
            else if (t == "in" || t == "c" || t == "tex" || t == "vin" || t == "cubetex") v = leaf(e);
            else
            {
                var a = new double[e.a.Length];
                for (int i = 0; i < a.Length; i++) a[i] = e.a[i] is X ex ? GEval(ex, leaf, memo) : Convert.ToDouble(e.a[i]);
                if (t == "add") v = a[0] + a[1];
                else if (t == "mul") v = a[0] * a[1];
                else if (t == "neg") v = -a[0];
                else if (t == "abs") v = Math.Abs(a[0]);
                else if (t == "sat") v = PyMin(1.0, PyMax(0.0, a[0]));
                else if (t == "max") v = PyMax(a[0], a[1]);
                else if (t == "min") v = PyMin(a[0], a[1]);
                else if (t == "lerp") v = a[0] + (a[1] - a[0]) * a[2];
                else if (t == "rcp" || t == "rcpc" || t == "rcpf") v = a[0] != 0 ? 1.0 / a[0] : double.PositiveInfinity;
                else if (t == "rsq" || t == "rsqc" || t == "rsqf") v = a[0] != 0 ? 1.0 / Math.Sqrt(Math.Abs(a[0])) : double.PositiveInfinity;
                else if (t == "sqrt") v = Math.Sqrt(Math.Abs(a[0]));
                else if (t == "frc" || t == "frcs") v = a[0] - PyFloor(a[0]);
                else if (t == "floor" || t == "floors") v = PyFloor(a[0]);
                else if (t == "trunc" || t == "truncs") v = PyTrunc(a[0]);
                else if (t == "exp") v = PyPow2(a[0]);
                else if (t == "log" || t == "logc") v = a[0] != 0 ? PyLog2(Math.Abs(a[0])) : double.NegativeInfinity;
                else if (GCnds.TryGetValue(t, out int ck))
                {
                    double c = a[0];
                    v = (ck == 0 ? c == 0 : ck == 1 ? c >= 0 : c > 0) ? a[1] : a[2];
                }
                else if (GSets.TryGetValue(t, out int sk))
                    v = (sk == 0 ? a[0] == a[1] : sk == 1 ? a[0] >= a[1] : sk == 2 ? a[0] > a[1] : a[0] != a[1]) ? 1.0 : 0.0;
                else if (GScalarSets.TryGetValue(t, out int ssk))
                    v = (ssk == 0 ? a[0] == 0 : ssk == 1 ? a[0] >= 0 : ssk == 2 ? a[0] > 0 : a[0] != 0) ? 1.0 : 0.0;
                else if (t == "cubeop") throw new Unsupported("cube instruction result used outside a cube fetch");
                else throw new Unsupported("op " + t);
            }
            memo[e] = v;
            return v;
        }

        static bool Close(double a, double b) => Math.Abs(a - b) <= 1e-5 * PyMax(PyMax(1.0, Math.Abs(a)), Math.Abs(b));

        // ------------------------------------------------------------------------------------- the vertex program: interpolator semantics

        sealed class Sem { public string kind; public int c; public double k; }

        static readonly Regex VfetchAddrRe = new Regex(@"^\s+(\d+) vfetch", RegexOptions.CultureInvariant);

        /// alive_general.vertex_usages: (D3DDECLUSAGE, usage index) of each vfetch, from the vertex program's fetch table.
        static List<(int usage, int index)> VertexUsages(byte[] blob, uint[] w, List<int> addresses)
        {
            long bytes = (long)w[1] - w[5];
            Bin.Require(bytes >= 0, "Vertex fetch table");
            long count = bytes / 4;
            Bin.Require((long)w[5] + 4 * count <= blob.Length, "Vertex fetch table");
            var words = new uint[count];
            for (long i = 0; i < count; i++) words[i] = Bin.U32BE(blob, (int)(w[5] + 4 * i));
            int n = addresses.Count;
            for (long i = 0; i < count - n + 1; i++)
            {
                bool all = true;
                for (int k = 0; k < n && all; k++) { uint v = words[i + k]; all = (v & 0xFF) == (uint)addresses[k] && (v >> 24) == 0; }
                if (!all) continue;
                var o = new List<(int, int)>();
                for (int k = 0; k < n; k++) { uint v = words[i + k]; o.Add(((int)((v >> 12) & 15), (int)((v >> 16) & 15))); }
                return o;
            }
            throw new Unsupported("vertex fetch table");
        }

        static Dictionary<int, double[]> ProgramLiterals(byte[] blob, Info info)
        {
            var w = info.w;
            Bin.Require((long)w[6] + 8 <= blob.Length && (long)w[5] + 24 <= blob.Length, "Shader container blocks");
            uint litBytes = Bin.U32BE(blob, (int)w[6]);
            int litBase = (int)((Bin.U32BE(blob, (int)w[5] + 20) >> 16) & 0xFF);
            var literals = new Dictionary<int, double[]>();
            for (int k = 0; k < litBytes / 16; k++)
            {
                Bin.Require(16 * k + 16 <= info.ucode.Length, "Literal constants beyond the microcode");
                literals[litBase + k] = new double[] { Bin.F32BE(info.ucode, 16 * k), Bin.F32BE(info.ucode, 16 * k + 4), Bin.F32BE(info.ucode, 16 * k + 8), Bin.F32BE(info.ucode, 16 * k + 12) };
            }
            return literals;
        }

        /// alive_general.vertex_semantics: {(interpolator, component): ("P" | "N" | "T" | "B" | "UV" | "VCOL" | "V", component) or ("K", value)}.
        static Dictionary<(int, int), Sem> VertexSemantics(byte[] vertexBlob)
        {
            var info = Container(vertexBlob);
            var lines = Disassemble(info);
            var addresses = new List<int>();
            foreach (var l in lines) { var am = VfetchAddrRe.Match(l); if (am.Success) addresses.Add(int.Parse(am.Groups[1].Value)); }
            var usages = VertexUsages(vertexBlob, info.w, addresses);
            var literals = ProgramLiterals(vertexBlob, info);
            var m = (GMachine)new GMachine(literals).Run(lines);
            var outKeys = new List<(string, int)>();
            foreach (var key in m.outOrder)
            {
                string rest = key.Item1.Substring(1);
                if (key.Item1.StartsWith("o", StringComparison.Ordinal) && rest.Length > 0 && rest.All(ch => ch >= '0' && ch <= '9')) outKeys.Add(key);
            }
            var samples = new List<(double?[] values, Dictionary<(string, int), double> cand)>();
            var rng = new Lcg(12345);
            for (int sample = 0; sample < 6; sample++)
            {
                var env = new Dictionary<X, double>();
                var world = new double[4][];
                for (int r = 0; r < 3; r++)
                {
                    world[r] = new double[4];
                    for (int c = 0; c < 3; c++) world[r][c] = rng.Uniform(-1, 1);
                    world[r][3] = rng.Uniform(-500, 500);
                }
                world[3] = new[] { 0.0, 0.0, 0.0, 1.0 };
                for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) env[new X("c", 64 + r, c)] = world[r][c];
                var cam = new double[4];
                for (int c = 0; c < 3; c++) cam[c] = rng.Uniform(-500, 500);
                cam[3] = 1.0;
                for (int c = 0; c < 4; c++) env[new X("c", 8, c)] = cam[c];
                var elems = new List<double[]>();
                foreach (var (usage, _) in usages)
                {
                    var v = new double[4];
                    for (int c = 0; c < 4; c++) v[c] = rng.Uniform(-2, 2);
                    if (usage == 0) v[3] = 1.0;
                    elems.Add(v);
                }
                double[] Element(int usage, int index)
                {
                    int k = usages.FindIndex(u => u.usage == usage && u.index == index);
                    return k >= 0 ? elems[k] : null;
                }
                double Leaf(X e)
                {
                    if (e.t == "vin") return elems[e.I(1)][e.I(2)];
                    if (e.t == "c")
                    {
                        double fresh = rng.Uniform(-3, 3);               // env.setdefault(e, rng.uniform(-3, 3)) draws whether or not e is there
                        if (env.TryGetValue(e, out double have)) return have;
                        env[e] = fresh; return fresh;
                    }
                    throw new Unsupported("vertex input " + e.t);
                }
                var memo = new Dictionary<X, double>(RefEq.I);
                var values = new double?[outKeys.Count];
                for (int i = 0; i < outKeys.Count; i++)
                {
                    try { values[i] = GEval(m.outv[outKeys[i]], Leaf, memo); }
                    catch (Unsupported) { values[i] = null; }
                    catch (OverflowException) { values[i] = null; }
                    catch (DivideByZeroException) { values[i] = null; }
                }
                var pos = Element(0, 0); var nrm = Element(3, 0); var tan = Element(6, 0);
                double[] Xf(double[] v, bool w4)
                {
                    var o = new double[3];
                    for (int r = 0; r < 3; r++)
                    {
                        double s = 0;
                        for (int c = 0; c < 3; c++) s += world[r][c] * v[c];
                        o[r] = s + (w4 ? world[r][3] * v[3] : 0.0);
                    }
                    return o;
                }
                var cand = new Dictionary<(string, int), double>();
                double[] P = null, N = null, T = null;
                if (pos != null)
                {
                    P = Xf(pos, true);
                    for (int c = 0; c < 3; c++) { cand[("P", c)] = P[c]; cand[("V", c)] = cam[c] - P[c]; }
                }
                if (nrm != null) { N = Xf(nrm, false); for (int c = 0; c < 3; c++) cand[("N", c)] = N[c]; }
                if (tan != null) { T = Xf(tan, false); for (int c = 0; c < 3; c++) cand[("T", c)] = T[c]; }
                if (nrm != null && tan != null)
                {
                    var B = new[] { T[1] * N[2] - T[2] * N[1], T[2] * N[0] - T[0] * N[2], T[0] * N[1] - T[1] * N[0] };
                    for (int c = 0; c < 3; c++) cand[("B", c)] = B[c];
                }
                for (int idx = 0; idx < 2; idx++)
                {
                    var uv = Element(5, idx);
                    if (uv != null) { cand[("UV", 2 * idx)] = uv[0]; cand[("UV", 2 * idx + 1)] = uv[1]; }
                }
                var col = Element(10, 0);
                if (col != null) for (int c = 0; c < 4; c++) cand[("VCOL", c)] = col[c];
                samples.Add((values, cand));
            }
            var sem = new Dictionary<(int, int), Sem>();
            for (int ki = 0; ki < outKeys.Count; ki++)
            {
                var key = outKeys[ki];
                int reg = int.Parse(key.Item1.Substring(1));
                if (samples.Any(s => s.values[ki] == null)) continue;
                double first = samples[0].values[ki].Value;
                if (samples.All(s => s.values[ki].Value == first)) { sem[(reg, key.Item2)] = new Sem { kind = "K", k = first }; continue; }
                foreach (var name in new[] { "P", "N", "T", "B", "UV", "VCOL", "V" })
                {
                    int hit = -1;
                    for (int c = 0; c < 4 && hit < 0; c++)
                        if (samples.All(s => s.cand.TryGetValue((name, c), out double cv) && Close(s.values[ki].Value, cv))) hit = c;
                    if (hit >= 0) { sem[(reg, key.Item2)] = new Sem { kind = name, c = hit }; break; }
                }
            }
            return sem;
        }

        // ------------------------------------------------------------------------------------- the pixel program

        static List<X> FlattenAdd(X e, List<X> output)
        {
            if (e.t == "add") { FlattenAdd(e.Sub(1), output); FlattenAdd(e.Sub(2), output); }
            else output.Add(e);
            return output;
        }

        static bool Contains(X e, Func<X, bool> pred, HashSet<X> seen = null)
        {
            seen = seen ?? new HashSet<X>(RefEq.I);
            if (!seen.Add(e)) return false;
            if (pred(e)) return true;
            foreach (var c in Children(e)) if (Contains(c, pred, seen)) return true;
            return false;
        }

        static bool IsLight(X e, int light, int? channel = null) => e.t == "tex" && e.I(1) == light && (channel == null || e.I(3) == channel.Value);

        /// alive_general.split_lit: (albedo, emissive) of a channel sum; terms with the light buffer's alpha (its specular) are not drawn.
        static (X albedo, X emissive) SplitLit(X x, int light, int channel)
        {
            var albedo = new List<X>(); var emissive = new List<X>();
            foreach (var term in FlattenAdd(x, new List<X>()))
            {
                if (!Contains(term, e => IsLight(e, light))) { emissive.Add(term); continue; }
                var factors = MulFactors(term);
                var lit = factors.Where(f => IsLight(f, light)).ToList();
                var rest = factors.Where(f => !IsLight(f, light)).ToList();
                if (Contains(term, e => IsLight(e, light, 3))) continue;
                if (lit.Count != 1 || lit[0].I(3) != channel || rest.Any(f => Contains(f, e => IsLight(e, light)))) throw new Unsupported("light buffer use");
                X a = K(1.0);
                foreach (var f in rest) a = Op("mul", a, f);
                albedo.Add(a);
            }
            X Total(List<X> ts)
            {
                if (ts.Count == 0) return null;
                X s = ts[0];
                for (int i = 1; i < ts.Count; i++) s = Op("add", s, ts[i]);
                return s;
            }
            return (Total(albedo), Total(emissive));
        }

        /// alive_general.strip_fog: X of the colour (X' + fog.c) x gHDRScale where the fog factor fog.w multiplies X' or each of its terms.
        static X StripFog(X e, int channel)
        {
            if (e.t != "mul") throw new Unsupported("no x gHDRScale form");
            var g = new X("c", 43, 0);
            X y = e.Sub(2) == g ? e.Sub(1) : e.Sub(1) == g ? e.Sub(2) : null;
            if (y == null) throw new Unsupported("no x gHDRScale form");
            var terms = FlattenAdd(y, new List<X>());
            var fog = terms.Where(t => t.t == "in" && t.I(2) == channel).ToList();
            foreach (var f in fog)
            {
                var fw = new X("in", f[1], 3);
                var rest = terms.Where(t => !ReferenceEquals(t, f)).ToList();
                var output = new List<X>();
                bool broke = false;
                foreach (var t in rest)
                {
                    var factors = MulFactors(t);
                    int at = factors.FindIndex(q => q == fw);
                    if (at < 0) { broke = true; break; }
                    factors.RemoveAt(at);
                    X prod = factors[0];
                    for (int i = 1; i < factors.Count; i++) prod = Op("mul", prod, factors[i]);
                    if (factors.Count == 1) FlattenAdd(prod, output); else output.Add(prod);
                }
                if (!broke)
                {
                    if (output.Count == 0) throw new Unsupported("fog only");
                    X x = output[0];
                    for (int i = 1; i < output.Count; i++) x = Op("add", x, output[i]);
                    return x;
                }
            }
            throw new Unsupported("no (X fog.w + fog) x gHDRScale form");
        }

        sealed class GParts
        {
            public GMachine m; public Dictionary<int, int> slots; public int? light, reflection; public HashSet<int> cubes;
            public Dictionary<int, string> prms; public Dictionary<(int, int), Sem> sem;
            public GParts With(GMachine m, Dictionary<int, int> slots, Dictionary<int, string> prms, int? light, int? reflection) =>
                new GParts { m = m, slots = slots, prms = prms, light = light, reflection = reflection, cubes = cubes, sem = sem };
        }

        static readonly Regex NumberedSamplerRe = new Regex(@"^s_sampler(\d+)\z", RegexOptions.CultureInvariant);

        /// alive_general.program_parts: the program executed symbolically with its sampler / parameter tables and the vertex program's semantics.
        static GParts ProgramParts(byte[] blob, byte[] vertexBlob)
        {
            var info = Container(blob);
            var literals = ProgramLiterals(blob, info);
            var consts = info.constants;
            var samplers = consts.Where(c => c.regset == "sampler").Select(c => (index: c.index, name: c.name, typ: c.typ))
                .OrderBy(s => s.index).ThenBy(s => s.name, StringComparer.Ordinal).ThenBy(s => s.typ).ToList();
            var user = samplers.Where(s => !(s.name.StartsWith("sampler", StringComparison.Ordinal) && s.name.Length > 7 && char.IsUpper(s.name[7]))).ToList();
            var numbered = user.Select(s => NumberedSamplerRe.Match(s.name)).ToList();
            var slots = new Dictionary<int, int>();
            bool allNumbered = numbered.All(mm => mm.Success);
            for (int k = 0; k < user.Count; k++) slots[user[k].index] = allNumbered ? int.Parse(numbered[k].Groups[1].Value) : k;
            int? light = samplers.Where(s => s.name == "samplerLightBuffer").Select(s => (int?)s.index).FirstOrDefault();
            int? reflection = samplers.Where(s => s.name == "samplerReflectionMap").Select(s => (int?)s.index).FirstOrDefault();
            var cubes = new HashSet<int>(samplers.Where(s => s.typ == 14).Select(s => s.index));
            if (reflection != null) cubes.Add(reflection.Value);
            var prms = new Dictionary<int, string>();
            foreach (var c in consts) if (c.regset == "float4" && c.name.StartsWith("usr_", StringComparison.Ordinal) && c.count == 1) prms[c.index] = c.name;
            var m = (GMachine)new GMachine(literals, cubes).Run(Disassemble(info));
            var sem = vertexBlob != null ? VertexSemantics(vertexBlob) : new Dictionary<(int, int), Sem>();
            return new GParts { m = m, slots = slots, light = light, reflection = reflection, cubes = cubes, prms = prms, sem = sem };
        }

        /// alive_general.compile_program.
        static JObj CompileProgram(byte[] container, byte[] vertex, byte[] normalBlob)
        {
            var parts = ProgramParts(container, vertex);
            var m = parts.m; var light = parts.light;
            var trees = new X[3];
            for (int c = 0; c < 3; c++) if (!m.outv.TryGetValue(("oC0", c), out trees[c])) throw new Unsupported("no colour output");
            var xs = new X[3];
            for (int c = 0; c < 3; c++) xs[c] = StripFog(trees[c], c);
            X[] albedo, emissive = null;
            if (light == null) albedo = xs;
            else
            {
                var pairs = new (X albedo, X emissive)[3];
                for (int c = 0; c < 3; c++) pairs[c] = SplitLit(xs[c], light.Value, c);
                albedo = pairs.Select(p => p.albedo ?? K(0.0)).ToArray();
                if (pairs.Any(p => p.emissive != null)) emissive = pairs.Select(p => p.emissive ?? K(0.0)).ToArray();
            }
            m.outv.TryGetValue(("oC0", 3), out X alpha);
            var c0 = new GCompiler(parts);
            int result = c0.Vec(albedo);
            int? emissiveReg = emissive != null ? c0.Vec(emissive) : (int?)null;
            int? normalReg = null;
            if (normalBlob != null && light != null)
            {
                // the material's normal pass (NormalPixelShader: oC0.xyz = 0.5 N + 0.5, the world normal the light buffer is lit with), compiled into
                // the same op list; the client lights the program's albedo with it
                try
                {
                    var np = ProgramParts(normalBlob, null);
                    var ns = new X[3];
                    for (int k = 0; k < 3; k++) if (!np.m.outv.TryGetValue(("oC0", k), out ns[k])) throw new Unsupported("no normal output");
                    var saved = c0.Save();
                    c0.p = parts.With(np.m, np.slots, np.prms, np.light, np.reflection);
                    c0.memo = new Dictionary<X, int>(RefEq.I); c0.vmemo = new Dictionary<Ref3, int>(); c0.cvmemo = new Dictionary<Ref3, bool>();
                    try { normalReg = c0.Vec(ns.Select(t => Op("add", Op("mul", t, K(2.0)), K(-1.0))).ToArray()); }
                    catch (Unsupported) { c0.Restore(saved); normalReg = null; }
                    c0.p = parts;
                }
                catch (Unsupported) { normalReg = null; }
            }
            int? alphaReg = null;
            if (alpha != null)
            {
                try { alphaReg = c0.Node(alpha); }
                catch (Unsupported) { alphaReg = null; }
            }
            return c0.Finish(result, emissiveReg, alphaReg, light != null, normalReg);
        }

        /// alive_general.Compiler.
        sealed class GCompiler
        {
            public GParts p;
            public readonly List<double[]> ops = new List<double[]>();
            public Dictionary<X, int> memo = new Dictionary<X, int>(RefEq.I);        // id(expression) -> value number
            public Dictionary<Ref3, int> vmemo = new Dictionary<Ref3, int>();
            public Dictionary<Ref3, bool> cvmemo = new Dictionary<Ref3, bool>();
            public Dictionary<string, int> leaves = new Dictionary<string, int>();      // structural key of a leaf / fetch -> value number
            public List<int> used = new List<int>();                                    // material texture slots, in binding order
            public List<string> paramsUsed = new List<string>();
            public List<int> cubes = new List<int>();                                   // the material texture slots of its cube maps (bindings 0, 1; 2 is the level's reflection map)
            int nanKeys;

            public GCompiler(GParts parts) { p = parts; }

            public sealed class Snapshot
            {
                public GParts p; public int ops; public Dictionary<X, int> memo; public Dictionary<Ref3, int> vmemo; public Dictionary<Ref3, bool> cvmemo;
                public Dictionary<string, int> leaves; public List<int> used; public List<string> paramsUsed; public List<int> cubes;
            }
            public Snapshot Save() => new Snapshot
            {
                p = p, ops = ops.Count, memo = new Dictionary<X, int>(memo, RefEq.I), vmemo = new Dictionary<Ref3, int>(vmemo), cvmemo = new Dictionary<Ref3, bool>(cvmemo),
                leaves = new Dictionary<string, int>(leaves), used = new List<int>(used), paramsUsed = new List<string>(paramsUsed), cubes = new List<int>(cubes),
            };
            public void Restore(Snapshot s)
            {
                p = s.p; memo = s.memo; vmemo = s.vmemo; cvmemo = s.cvmemo; leaves = s.leaves; used = s.used; paramsUsed = s.paramsUsed; cubes = s.cubes;
                ops.RemoveRange(s.ops, ops.Count - s.ops);
            }

            // a structural key: numbers compare as Python's int / float do (1 == 1.0, 0.0 == -0.0)
            string Key(string tag, params double[] nums)
            {
                var sb = new System.Text.StringBuilder(tag);
                foreach (double d in nums)
                {
                    sb.Append(':');
                    if (d == 0) sb.Append('0');
                    else if (double.IsNaN(d)) sb.Append("nan").Append(nanKeys++);
                    else sb.Append(BitConverter.DoubleToInt64Bits(d));
                }
                return sb.ToString();
            }

            int Emit(int code, double a = 0, double b = 0, double p0 = 0, double p1 = 0, double p2 = 0, double p3 = 0)
            {
                ops.Add(new double[] { code, ops.Count, a, b, p0, p1, p2, p3 });
                if (ops.Count > MAX_GOPS) throw new Unsupported("size");
                return ops.Count - 1;
            }

            int Shared(string key, Func<int> make)
            {
                if (!leaves.TryGetValue(key, out int have)) { have = make(); leaves[key] = have; }
                return have;
            }

            int Input(int which) => Shared(Key("in", which), () => Emit(G_IN, which));
            int Channel(int v, int c) => Shared(Key("ch", v, c), () => Emit(G_CH, v, c));
            int Const(double value) => Shared(Key("k", value), () => Emit(G_K, 0, 0, value, 0, 0, 0));

            int Param(int register)
            {
                if (!p.prms.TryGetValue(register, out string name)) throw new Unsupported("constant c" + register);
                if (!paramsUsed.Contains(name)) paramsUsed.Add(name);
                if (paramsUsed.Count > MAX_PARAMS) throw new Unsupported("parameters");
                int k = paramsUsed.IndexOf(name);
                return Shared(Key("param", k), () => Emit(G_PARAM, k));
            }

            int Interpolant(int reg, int comp)
            {
                if (!p.sem.TryGetValue((reg, comp), out Sem s)) throw new Unsupported("interpolator r" + reg + "." + XYZW[comp]);
                if (s.kind == "K") return Const(s.k);
                if (s.kind == "V")
                {
                    int cam = Channel(Input(IN_CAM), s.c), pos = Channel(Input(IN_P), s.c);
                    return Shared(Key("V", s.c), () => Emit(G_ADD, cam, Shared(Key("negP", s.c), () => Emit(G_NEG, pos))));
                }
                int which = s.kind == "P" ? IN_P : s.kind == "N" ? IN_N : s.kind == "T" ? IN_T : s.kind == "B" ? IN_B : s.kind == "UV" ? IN_UV : IN_VCOL;
                return Channel(Input(which), s.c);
            }

            int Slot(int sampler)
            {
                if (!p.slots.TryGetValue(sampler, out int slot)) throw new Unsupported("sampler s" + sampler);
                if (!used.Contains(slot)) used.Add(slot);
                if (used.Count > MAX_GSLOTS) throw new Unsupported("textures");
                return used.IndexOf(slot);
            }

            /// (uv set, tile u, tile v, parameter + 1) when both coordinates are k x the interpolated UV (optionally + a parameter's (x, y)), else null.
            (int set, double tu, double tv, int k)? UvCoordinate(X coord)
            {
                var offset = new int?[2]; var outs = new List<(int comp, double scale)>();
                for (int i = 0; i < 2; i++)
                {
                    X c = coord.Sub(i + 1);
                    if (c.t == "add")
                        foreach (var (a, b) in new[] { (c.Sub(1), c.Sub(2)), (c.Sub(2), c.Sub(1)) })
                            if (b.t == "c" && b.I(2) == i && p.prms.ContainsKey(b.I(1))) { offset[i] = b.I(1); c = a; break; }
                    double scale = 1.0;
                    while (c.t == "mul" && (IsK(c.Sub(1)) || IsK(c.Sub(2))))
                    {
                        X k;
                        if (IsK(c.Sub(1))) { k = c.Sub(1); c = c.Sub(2); } else { k = c.Sub(2); c = c.Sub(1); }
                        scale *= k.D(1);
                    }
                    if (c.t == "add" && c.Sub(1) == c.Sub(2)) { scale *= 2.0; c = c.Sub(1); }
                    if (c.t != "in") return null;
                    if (!p.sem.TryGetValue((c.I(1), c.I(2)), out Sem s) || s.kind != "UV") return null;
                    outs.Add((s.c, scale));
                }
                if (offset[0] != offset[1] || outs[0].comp / 2 != outs[1].comp / 2 || outs[0].comp % 2 != 0 || outs[1].comp % 2 != 1) return null;
                int kp = 0;
                if (offset[0] != null)
                {
                    Param(offset[0].Value);
                    kp = paramsUsed.IndexOf(p.prms[offset[0].Value]) + 1;
                }
                return (outs[0].comp / 2, outs[0].scale, outs[1].scale, kp);
            }

            int Fetch(X e)
            {
                int tf = e.I(1); X coord = e.Sub(2);
                if (p.light != null && tf == p.light.Value) throw new Unsupported("light buffer use");
                int k = Slot(tf);
                var uv = UvCoordinate(coord);
                if (uv != null)
                {
                    var q = uv.Value;
                    return Shared(Key("tex", k, q.set, q.tu, q.tv, q.k), () => Emit(G_TEX, k, q.set, q.tu, q.tv, q.k, 0));
                }
                int u = Node(coord.Sub(1)), v = Node(coord.Sub(2));
                return Shared(Key("tex", k, u, v), () => Emit(G_TEX, k, 2, u, v, 0, 0));
            }

            int CubeFetch(X e)
            {
                int tf = e.I(1), which;
                if (p.reflection != null && tf == p.reflection.Value) which = 2;
                else
                {
                    if (!p.slots.TryGetValue(tf, out int slot)) throw new Unsupported("cube sampler");
                    if (!cubes.Contains(slot)) cubes.Add(slot);
                    if (cubes.Count > 2) throw new Unsupported("cube maps");
                    which = cubes.IndexOf(slot);
                }
                int vec = Vnode(e.Sub(2), e.Sub(3), e.Sub(4));
                return Shared(Key("cube", which, vec), () => Emit(G_CUBE, which, vec));
            }

            public int Node(X e)
            {
                if (memo.TryGetValue(e, out int have)) return have;
                string t = e.t;
                int r;
                List<X> terms;
                if (t == "k") r = Const(e.D(1));
                else if (t == "in") r = Interpolant(e.I(1), e.I(2));
                else if (t == "c") r = e.I(1) == 8 ? Channel(Input(IN_CAM), e.I(2)) : Channel(Param(e.I(1)), e.I(2));
                else if (t == "tex") r = Channel(Fetch(e), e.I(3));
                else if (t == "cubetex") r = Channel(CubeFetch(e), e.I(5));
                else if (t == "add" && (terms = DotOrder(e)) != null)
                {
                    int n = Math.Min(terms.Count, 3);
                    var av = terms.Take(n).Select(x => x.Sub(1)).ToList(); var bv = terms.Take(n).Select(x => x.Sub(2)).ToList();
                    if (n == 2) { av.Add(av[av.Count - 1]); bv.Add(bv[bv.Count - 1]); }
                    int va = Vnode(av[0], av[1], av[2]), vb = Vnode(bv[0], bv[1], bv[2]);
                    r = Emit(G_DOT, va, vb, n, 0, 0, 0);
                    if (terms.Count == 4) { int t3 = Node(terms[3]); r = Emit(G_ADD, r, t3); }
                }
                else if (GBinary.ContainsKey(t) || GUnary.ContainsKey(t) || t == "lerp" || GCnds.ContainsKey(t) || GSets.ContainsKey(t) || GScalarSets.ContainsKey(t))
                {
                    var args = new List<int>();
                    foreach (var x in e.a) args.Add(Node((X)x));
                    r = Combine(t, args);
                }
                else throw new Unsupported("op " + t);
                memo[e] = r;
                return r;
            }

            int Swizzle(int v, int[] comps)
            {
                if (comps[0] == 0 && comps[1] == 1 && comps[2] == 2) return v;
                int c0 = comps[0], c1 = comps[1], c2 = comps[2], c3 = comps[comps.Length - 1];
                return Shared(Key("swz", v, c0, c1, c2, c3), () => Emit(G_SWZ, v, 0, c0, c1, c2, c3));
            }

            /// A float4 register for three parallel leaves, or null.
            int? Vleaf(X x, X y, X z)
            {
                string t = x.t;
                if (t == "k") return Shared(Key("k4", x.D(1), y.D(1), z.D(1)), () => Emit(G_K4, 0, 0, x.D(1), y.D(1), z.D(1), 0.0));
                if (t == "tex" || t == "cubetex")
                {
                    if (!SameFetch(x, y, z)) return null;
                    int v = t == "tex" ? Fetch(x) : CubeFetch(x);
                    int ch = t == "tex" ? 3 : 5;
                    return Swizzle(v, new[] { x.I(ch), y.I(ch), z.I(ch) });
                }
                if (t == "in")
                {
                    var sems = new Sem[3]; var xyz = new[] { x, y, z };
                    for (int i = 0; i < 3; i++) p.sem.TryGetValue((xyz[i].I(1), xyz[i].I(2)), out sems[i]);
                    if (sems.Any(s => s == null || s.kind == "K") || sems.Select(s => s.kind).Distinct().Count() != 1) return null;
                    string kind = sems[0].kind;
                    int v;
                    if (kind == "V")
                    {
                        int cam = Input(IN_CAM), pos = Input(IN_P);
                        v = Shared(Key("Vvec"), () => Emit(G_ADD, cam, Shared(Key("negPvec"), () => Emit(G_NEG, pos))));
                    }
                    else v = Input(kind == "P" ? IN_P : kind == "N" ? IN_N : kind == "T" ? IN_T : kind == "B" ? IN_B : kind == "UV" ? IN_UV : IN_VCOL);
                    return Swizzle(v, new[] { sems[0].c, sems[1].c, sems[2].c });
                }
                if (t == "c")
                {
                    if (!(x.I(1) == y.I(1) && y.I(1) == z.I(1))) return null;
                    int v = x.I(1) == 8 ? Input(IN_CAM) : Param(x.I(1));
                    return Swizzle(v, new[] { x.I(2), y.I(2), z.I(2) });
                }
                return null;
            }

            // the same texture / cube sampler at coordinates that are the same expression objects
            static bool SameFetch(X x, X y, X z)
            {
                if (!(x.I(1) == y.I(1) && y.I(1) == z.I(1))) return false;
                if (x.t == "tex")
                {
                    X px = x.Sub(2), py = y.Sub(2), pz = z.Sub(2);
                    for (int i = 0; i < 2; i++) if (!(ReferenceEquals(px.a[i], py.a[i]) && ReferenceEquals(py.a[i], pz.a[i]))) return false;
                    return true;
                }
                for (int i = 2; i <= 4; i++) if (!(ReferenceEquals(x[i], y[i]) && ReferenceEquals(y[i], z[i]))) return false;
                return true;
            }

            static bool AllExpressions(X x, X y, X z) => x.a.All(v => v is X) && y.a.All(v => v is X) && z.a.All(v => v is X);

            public int Vnode(X x, X y, X z)
            {
                var key = new Ref3(x, y, z);
                if (vmemo.TryGetValue(key, out int have)) return have;
                int r;
                if (ReferenceEquals(x, y) && ReferenceEquals(y, z)) r = Node(x);
                else
                {
                    int? rr = null;
                    string t = x.t;
                    if (t == y.t && t == z.t)
                    {
                        if (t == "k" || t == "tex" || t == "cubetex" || t == "in" || t == "c") rr = Vleaf(x, y, z);
                        else if (x.Len == y.Len && x.Len == z.Len && AllExpressions(x, y, z))
                        {
                            var args = new List<int>();
                            for (int i = 1; i < x.Len; i++) args.Add(Vnode(x.Sub(i), y.Sub(i), z.Sub(i)));
                            rr = Combine(t, args);
                        }
                    }
                    if (rr == null)
                    {
                        int v0 = Node(x), v1 = Node(y), v2 = Node(z);
                        int one = Const(1.0);
                        rr = Emit(G_PACK, v0, v1, v2, one, 0, 0);
                    }
                    r = rr.Value;
                }
                vmemo[key] = r;
                return r;
            }

            /// The op of tag t over value numbers (Node's emission, shared by Node and Vnode).
            int Combine(string t, List<int> args)
            {
                if (GBinary.TryGetValue(t, out int bc)) return Emit(bc, args[0], args[1]);
                if (GUnary.TryGetValue(t, out int uc)) return Emit(uc, args[0]);
                if (t == "lerp")
                {
                    int a = args[0], b = args[1], w = args[2];
                    int neg = Emit(G_NEG, a);
                    int d = Emit(G_ADD, b, neg);
                    int dw = Emit(G_MUL, d, w);
                    return Emit(G_ADD, a, dw);
                }
                if (GCnds.TryGetValue(t, out int ck)) return Emit(G_CND, args[0], args[1], args[2], ck, 0, 0);
                if (GSets.TryGetValue(t, out int sk)) return Emit(G_SET, args[0], args[1], sk, 0, 0, 0);
                if (GScalarSets.TryGetValue(t, out int ssk)) { int zero = Const(0.0); return Emit(G_SET, args[0], zero, ssk, 0, 0, 0); }
                throw new Unsupported("op " + t);
            }

            /// The products of a sum of 2 to 4 products (the expanded dp2add / dp3 / dp4), in order, or null.
            static List<X> DotTerms(X e)
            {
                var terms = new List<X>();
                X x = e;
                while (x.t == "add" && terms.Count < 4) { terms.Add(x.Sub(2)); x = x.Sub(1); }
                terms.Add(x);
                terms.Reverse();
                if (terms.Count < 2 || terms.Count > 4 || terms.Any(t => t.t != "mul")) return null;
                return terms;
            }

            // itertools.permutations(range(n)) order
            static IEnumerable<int[]> Permutations(int n)
            {
                var perm = Enumerable.Range(0, n).ToArray();
                while (true)
                {
                    yield return (int[])perm.Clone();
                    int i = n - 2;
                    while (i >= 0 && perm[i] > perm[i + 1]) i--;
                    if (i < 0) yield break;
                    int j = n - 1;
                    while (perm[j] < perm[i]) j--;
                    (perm[i], perm[j]) = (perm[j], perm[i]);
                    Array.Reverse(perm, i + 1, n - i - 1);
                }
            }

            /// The products of a sum of 2 to 4 products in an order whose first three (two) run in parallel, or null.
            List<X> DotOrder(X e)
            {
                var terms = DotTerms(e);
                if (terms == null) return null;
                foreach (var perm in Permutations(terms.Count))
                {
                    var ts = perm.Select(i => terms[i]).ToList();
                    int n = Math.Min(ts.Count, 3);
                    var av = ts.Take(n).Select(x => x.Sub(1)).ToList(); var bv = ts.Take(n).Select(x => x.Sub(2)).ToList();
                    if (n == 2) { av.Add(av[av.Count - 1]); bv.Add(bv[bv.Count - 1]); }
                    if (CanVec(av[0], av[1], av[2]) && CanVec(bv[0], bv[1], bv[2])) return ts;
                }
                return null;
            }

            /// Vnode(x, y, z) builds float4 operations without packing scalars.
            bool CanVec(X x, X y, X z)
            {
                var key = new Ref3(x, y, z);
                if (cvmemo.TryGetValue(key, out bool have)) return have;
                bool ok;
                if (ReferenceEquals(x, y) && ReferenceEquals(y, z)) ok = true;
                else
                {
                    string t = x.t;
                    ok = false;
                    if (t == y.t && t == z.t)
                    {
                        if (t == "k") ok = true;
                        else if (t == "tex" || t == "cubetex") ok = SameFetch(x, y, z);
                        else if (t == "in")
                        {
                            var sems = new Sem[3]; var xyz = new[] { x, y, z };
                            for (int i = 0; i < 3; i++) p.sem.TryGetValue((xyz[i].I(1), xyz[i].I(2)), out sems[i]);
                            ok = !sems.Any(q => q == null || q.kind == "K") && sems.Select(q => q.kind).Distinct().Count() == 1;
                        }
                        else if (t == "c") ok = x.I(1) == y.I(1) && y.I(1) == z.I(1);
                        else if (x.Len == y.Len && x.Len == z.Len && AllExpressions(x, y, z))
                        {
                            ok = true;
                            for (int i = 1; i < x.Len && ok; i++) ok = CanVec(x.Sub(i), y.Sub(i), z.Sub(i));
                        }
                    }
                }
                cvmemo[key] = ok;
                return ok;
            }

            public int Vec(X[] channels) => Vnode(channels[0], channels[1], channels[2]);

            /// Linear-scan register allocation; the program as the sidecar's fields.
            public JObj Finish(int result, int? emissive, int? alpha, bool lit, int? normal)
            {
                bool Reads(int code, out int[] positions)
                {
                    switch (code)
                    {
                        case G_CH: case G_NEG: case G_ABS: case G_SAT: case G_RCP: case G_RSQ: case G_SQRT: case G_FRC: case G_FLOOR: case G_EXP: case G_LOG: case G_TRUNC: case G_SWZ:
                            positions = new[] { 2 }; return true;
                        case G_ADD: case G_MUL: case G_MAX: case G_MIN: case G_SET: case G_DOT:
                            positions = new[] { 2, 3 }; return true;
                        case G_CND: positions = new[] { 2, 3, 4 }; return true;
                        case G_CUBE: positions = new[] { 3 }; return true;
                        case G_PACK: positions = new[] { 2, 3, 4, 5 }; return true;
                    }
                    positions = Array.Empty<int>(); return false;
                }
                int[] Positions(double[] o)
                {
                    if ((int)o[0] == G_TEX) return o[3] == 2 ? new[] { 4, 5 } : Array.Empty<int>();
                    Reads((int)o[0], out var ps); return ps;
                }
                var outputs = new List<int>();
                foreach (var v in new int?[] { result, emissive, alpha, normal }) if (v != null) outputs.Add(v.Value);
                var last = new Dictionary<int, int>();
                for (int i = 0; i < ops.Count; i++) foreach (int k in Positions(ops[i])) last[(int)ops[i][k]] = i;
                foreach (int v in outputs) last[v] = ops.Count;
                var phys = new Dictionary<int, int>(); var free = new List<int>(); int top = 0;
                for (int i = 0; i < ops.Count; i++)
                {
                    var o = ops[i];
                    var ps = Positions(o);
                    var values = ps.Select(k => (int)o[k]).ToList();
                    var srcs = values.Select(v => phys[v]).ToList();
                    foreach (int v in values.Distinct().OrderBy(q => q)) if (last[v] == i) free.Add(phys[v]);
                    int r;
                    if (free.Count > 0) { r = free.Min(); free.Remove(r); }
                    else { r = top; top++; }
                    if (top > MAX_GREGS) throw new Unsupported("registers");
                    int value = (int)o[1];
                    phys[value] = r;
                    o[1] = r;
                    for (int j = 0; j < ps.Length; j++) o[ps[j]] = srcs[j];
                    if (!last.ContainsKey(value)) free.Add(r);                 // written and never read
                }
                var flat = new List<object>();
                foreach (var o in ops) foreach (var v in o) flat.Add(v);
                return new JObj().Set("general", 1L).Set("ops", flat).Set("result", (long)phys[result])
                    .Set("slots", used.Select(s => (object)(long)s).ToList()).Set("params", paramsUsed.Cast<object>().ToList()).Set("lit", lit)
                    .Set("emissive", (long)(emissive != null ? phys[emissive.Value] : -1)).Set("alpha", (long)(alpha != null ? phys[alpha.Value] : -1))
                    .Set("normal", (long)(normal != null ? phys[normal.Value] : -1)).Set("cubes", cubes.Select(s => (object)(long)s).ToList());
            }
        }
    }
}
