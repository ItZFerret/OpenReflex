// MX vs ATV Untamed converter, stage S4 (materials): the port of the Python reference converter's ux_materials.py (DESIGN 5.5, 15, 18, 19).
// Untamed .material -> Reflex PC .material on a stock shader template: every output material is the stock exemplar of its template
// with only the texture names, the patched render states and the sampler address modes / MIPFILTER changed; the template's .shader is
// the stock asset (never an Untamed shader). Writes, below the work store's materials/ folder (Python: work/<Level>/materials/):
//     <asset>.material            shaders/<Template>.shader            textures/<name>.texture (GL_Hay; the generated UXC_/UXM_/UXL_/UXA_)
//     materials.json              (the manifest; materials_sheet.png, a preview, is not written)
// Reads: the disc level (materials, shaders, textures, surfaces), the stock library (exemplars, shaders, GL_Hay), level.json,
// terrain/material_requests.json (+ the textures it lists) and, when present, textures/textures.json from the work store.
// Also in this file because only this stage uses them: the stock-layout BXML tree codec of the materials, the D3D9 SM3 data-flow
// tracer of the reference's templates/reflex-lib/sm3dis.py (which mesh UV set does each template sampler read) and the generated
// textures of ux_textures.py (gen_reference, generated_texture, bc1_encode / bc3_encode, grey_dxn_facts).
// Entry points: Convert (the stage); for the builder (ux_build_track, ux_ads, ux_screens, ux_splitgate call these of ux_materials):
// Entries, AssetName, BuildMaterial, RebuildEntry, TemplateSlots, GeneratedTexture and Planner (LevelCtx + plan_material).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_materials.Opts plus the call's other arguments. Every switch's first value is the default of the current builds.
    public sealed class UxMaterialsOptions
    {
        /// eco_frond | diffuse_alpha | opaque | drop
        public string alpha = "eco_frond";
        public bool prelit_off, nrm_off, crowd_off;
        /// keep | test
        public string blend = "keep";
        /// hay | gray | white | logo
        public string bale_base = "hay";
        /// linear | keep
        public string mipfilter = "linear";
        /// bake | off
        public string composite = "bake";
        /// program | default
        public string literal_albedo = "program";
        /// program | diffuse
        public string alpha_source = "program";
        /// program | order
        public string diffuse_pick = "program";
        /// null: every closure material plus the requests; else these names.
        public IList<string> names;
        /// null: terrain/material_requests.json when the work store has it; "none": no requests; else a work-store path.
        public string requests;
        /// null: "materials", or "materials-<tag>" for a non-default switch set (default_out_dir).
        public string out_dir;
        public bool write = true;
        /// Worker threads for the surfaces and the generated textures (0: one per processor). The result does not depend on it.
        public int jobs;

        // The reference tools' identity as materials.json records it (tool_md5, source_files.readers / sm3dis_md5 / lib_index_md5).
        // A port cannot compute the md5 of Python files: these are the md5s of the reference this port mirrors (tools-fmx2, 2026-11-12).
        public string tool_md5 = "019bed15d01ca827d8f03014fda571fd";
        public string lib_index_md5 = "7a769d78fccae4fd9b6e94827122aab6", sm3dis_md5 = "e3f7c1f1ce9f8e12305dc3ebb964509c";
        public KeyValuePair<string, string>[] readers =
        {
            new KeyValuePair<string, string>("uxmat.py", "5db0bfcc1c7b6eb597f353d1385fd8a5"), new KeyValuePair<string, string>("uxmesh.py", "146690511db2f48584732c12c5be876e"),
            new KeyValuePair<string, string>("uxtex.py", "77b39dabdbc113f0ba5e53d518bc1a36"), new KeyValuePair<string, string>("d3d9sh.py", "d15b82b65e25dd2983294d92bc000433"),
            new KeyValuePair<string, string>("ux_shaderprog.py", "c204248ffb114881fd070cfe05182412"), new KeyValuePair<string, string>("ux_textures.py", "502a142691401e33fd7a1763b1b5f8bd"),
        };

        static void In(string v, string what, params string[] choices)
        {
            if (Array.IndexOf(choices, v) < 0) throw new UxException(what + " must be one of (" + string.Join(", ", choices) + ")");
        }
        public void Validate()
        {
            In(alpha, "alpha", "eco_frond", "diffuse_alpha", "opaque", "drop"); In(blend, "blend", "keep", "test");
            In(bale_base, "bale_base", "hay", "gray", "white", "logo"); In(mipfilter, "mipfilter", "linear", "keep");
            In(composite, "composite", "bake", "off"); In(literal_albedo, "literal_albedo", "program", "default");
            In(alpha_source, "alpha_source", "program", "diffuse"); In(diffuse_pick, "diffuse_pick", "program", "order");
        }
        public bool ProgramUsed => !(composite == "off" && literal_albedo == "default" && alpha_source == "diffuse" && diffuse_pick == "order");
        /// Opts.as_dict().
        public JObj AsDict() => new JObj().Set("alpha", alpha).Set("prelit_off", prelit_off).Set("nrm_off", nrm_off).Set("crowd_off", crowd_off).Set("blend", blend)
            .Set("bale_base", bale_base).Set("mipfilter", mipfilter).Set("composite", composite).Set("literal_albedo", literal_albedo)
            .Set("alpha_source", alpha_source).Set("diffuse_pick", diffuse_pick);
        /// Opts.tag(): '' for the defaults, 'safe', or the switches that differ joined by '_'.
        public string Tag()
        {
            if (alpha == "drop" && prelit_off && nrm_off && crowd_off && blend == "keep" && bale_base == "hay" && mipfilter == "linear" && composite == "bake"
                && literal_albedo == "program" && alpha_source == "program" && diffuse_pick == "program") return "safe";
            var parts = new List<string>();
            if (alpha != "eco_frond") parts.Add("alpha-" + alpha.Replace('_', '-'));
            if (prelit_off) parts.Add("prelit-off");
            if (nrm_off) parts.Add("nrm-off");
            if (crowd_off) parts.Add("crowd-off");
            if (blend != "keep") parts.Add("blend-" + blend);
            if (bale_base != "hay") parts.Add("bale-" + bale_base);
            if (mipfilter != "linear") parts.Add("mip-" + mipfilter);
            if (composite != "bake") parts.Add("composite-" + composite);
            if (literal_albedo != "program") parts.Add("albedo-" + literal_albedo);
            if (alpha_source != "program") parts.Add("alphasrc-" + alpha_source);
            if (diffuse_pick != "program") parts.Add("pick-" + diffuse_pick);
            return string.Join("_", parts);
        }
        /// default_out_dir(lv, opts) relative to the level's work store.
        public string OutDir() { if (out_dir != null) return out_dir.TrimEnd('/'); string t = Tag(); return "materials" + (t.Length > 0 ? "-" + t : ""); }
    }

    public static class UxMaterials
    {
        public const string SCHEMA = "ux_materials/1";
        public static readonly string[] TEMPLATES = { "Template_Prelit", "Template_Diffuse", "Template_Diffuse_Nrm", "Template_Diffuse_AO", "Template_Diffuse_Alpha", "Template_Eco_Frond", "crowd" };
        static readonly string[] OPAQUE_TEMPLATES = { "Template_Prelit", "Template_Diffuse", "Template_Diffuse_Nrm", "Template_Diffuse_AO" };
        const double ALBEDO_KEEP = 0.05;                  // a literal albedo within this of the default keeps the default
        const int OPAQUE_ALPHA_MIN = 250;                 // a texture whose alpha never drops below this has no cut
        const string DEFAULT_WHITE = "Editor_Default_White", DEFAULT_BLACK = "Editor_Default_Black", DEFAULT_GRAY = "Editor_Default_Gray";
        const string JUMBO_REPLACEMENT = DEFAULT_BLACK;
        const int MIPFILTER_LINEAR = 2, D3DCMP_GREATER = 5;
        const double MACRO_SCALE = 0.25;                  // a diffuse candidate fetched at |uv scale| below this is a macro tint / dirt map
        static readonly string[] PATCH_SS = { "ADDRESSU", "ADDRESSV", "ADDRESSW" };
        public static readonly string[] META_KEYS = { "schema", "level", "short", "mode", "options", "tool", "tool_md5", "source_files", "out_dir",
            "template_uv", "prelit_texcoord", "shaders_needed", "shader_files", "textures_needed", "texture_assets",
            "template_textures", "texture_substitutions", "library_textures", "generated_textures",
            "material_assets", "histogram", "actions", "counts", "clashes", "problems", "warnings", "meta_keys" };
        // Stock Reflex PC textures a material may reference: name -> the stock levels to look in (the first that holds it), md5, size.
        static readonly (string name, string[] levels, string md5, int size)[] LIB_TEXTURES =
        {
            ("GL_Hay", new[] { "Ctrl_AmbientAnimation", "FR_Gibson", "NAT_Stillwater" }, "e8c12d19558929b940c96ce8f03b63fe", 87536),
        };
        // templates/reflex-lib/index.json template_slots[kind].library_shaders_in_template[T].equals_library (the DE sketch 'sx' and the
        // HuCkiT free-ride slot 'fr'; facts about those two template packages, not about the stock library).
        static readonly string[] IN_TEMPLATE_FR = { "Template_Diffuse", "Template_Diffuse_AO", "Template_Diffuse_Nrm" }, IN_TEMPLATE_SX = { "Template_Diffuse_AO", "Water" };

        static readonly Regex BALE_NM = new Regex(@"^haybale_\w*_nm$", RegexOptions.CultureInvariant);
        static readonly Regex LEADER_DIGIT = new Regex(@"^LPos\d+_Num\d+$", RegexOptions.CultureInvariant | RegexOptions.IgnoreCase);
        static readonly Regex RAMP2 = new Regex(@"^(black|white|grey|gray)_?2_?(black|white|grey|gray)", RegexOptions.CultureInvariant);
        static readonly Regex MASKY_SUFFIX = new Regex(@"_(ra|i|o|p|pl|l|m|mask|msk|alpha|alphamap)$", RegexOptions.CultureInvariant);
        static readonly Regex MASKY_WORD = new Regex(@"(mask|msk|masque|alphamap|helper)", RegexOptions.CultureInvariant);
        static readonly Regex SIBLING_SUFFIX = new Regex(@"^_(ra|i|o|p|pl|l|m|n|nor|nm|norm|mask|msk|spec|ao|occ|alpha)$", RegexOptions.CultureInvariant);
        static readonly Regex SX_FLOOR = new Regex(@"_SX(Wood|Floor|Ground|Plastic)$", RegexOptions.CultureInvariant | RegexOptions.IgnoreCase);
        static readonly Regex UNSAFE_FILE = new Regex(@"[^A-Za-z0-9_.\- ]", RegexOptions.CultureInvariant);
        static readonly Regex MOV_O = new Regex(@"^mov o\d+\.(xy|zw)", RegexOptions.CultureInvariant);

        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);
        static bool Has(string[] a, string v) => Array.IndexOf(a, v) >= 0;
        static string Lower(string s) => UxCommon.PyLower(s);
        static bool IsAlphaTemplate(string T) => T == "Template_Eco_Frond" || T == "Template_Diffuse_Alpha";
        static bool IsTemplateDefault(string t) => t == DEFAULT_WHITE || t == DEFAULT_BLACK || t == DEFAULT_GRAY;
        static int DiffuseSlot(string T)
        {
            switch (T) { case "Template_Prelit": case "Template_Diffuse_AO": return 1; case "Template_Diffuse": case "Template_Diffuse_Nrm": case "Template_Diffuse_Alpha": case "Template_Eco_Frond": case "crowd": return 0; }
            return -1;
        }
        /// repr(str).
        static string Repr(string s)
        {
            if (s == null) return "None";
            bool dq = s.IndexOf('\'') >= 0 && s.IndexOf('"') < 0;
            var sb = new StringBuilder(); sb.Append(dq ? '"' : '\'');
            foreach (char c in s)
            {
                if (c == '\\') sb.Append("\\\\");
                else if (c == '\'' && !dq) sb.Append("\\'");
                else if (c == '\n') sb.Append("\\n"); else if (c == '\r') sb.Append("\\r"); else if (c == '\t') sb.Append("\\t");
                else if (c < 32 || c == 127) sb.Append("\\x").Append(((int)c).ToString("x2"));
                else sb.Append(c);
            }
            return sb.Append(dq ? '"' : '\'').ToString();
        }
        static string ReprList(IEnumerable<string> v) { var p = new List<string>(); foreach (string s in v) p.Add(Repr(s)); return "[" + string.Join(", ", p) + "]"; }
        static string S(string v) => v ?? "None";        // '%s' % a str or None
        static bool Truthy(object v)
        {
            switch (v)
            {
                case null: return false;
                case bool b: return b;
                case string s: return s.Length > 0;
                case long l: return l != 0;
                case double d: return d != 0;
                case JObj j: return j.items.Count > 0;
                case List<object> a: return a.Count > 0;
            }
            return true;
        }
        static long IntOr(JObj rec, string key, long otherwise) { object v = rec?.Get(key); return Truthy(v) && v is long l ? l : otherwise; }
        static List<object> Lst(IEnumerable<string> v) { var o = new List<object>(); foreach (string s in v) o.Add(s); return o; }

        // ============================================================================== BXML: stock-layout tree codec
        /// One attribute: [name, type, flag, value] (a string for type 1, the raw pool bytes otherwise).
        sealed class NAttr
        {
            public string name; public int type; public long flag; public string str; public byte[] raw;
            public NAttr Copy() => new NAttr { name = name, type = type, flag = flag, str = str, raw = raw == null ? null : (byte[])raw.Clone() };
        }
        /// One BXML element. vtype / vflag / value: the element value (vflag = element count, or the byte length of a type-2 blob);
        /// value is null, a string (types 0, 1) or bytes.
        sealed class Node
        {
            public string tag; public List<NAttr> attrs = new List<NAttr>(); public int vtype; public long vflag; public object value; public List<Node> children = new List<Node>();
            public Node Find(string t) { foreach (var c in children) if (c.tag == t) return c; return null; }
            public IEnumerable<Node> Iter() { yield return this; foreach (var c in children) foreach (var d in c.Iter()) yield return d; }
            public string AttrStr(string name) { foreach (var a in attrs) if (a.name == name) return a.type == 1 ? a.str : null; return null; }
            public void SetAttr(string name, string value)
            {
                foreach (var a in attrs) if (a.name == name) { UxBin.Require(a.type == 1, "material: " + name + " is not a string attribute"); a.str = value; return; }
                throw new UxException("KeyError " + name);
            }
            public void SetAttr(string name, long value)
            {
                foreach (var a in attrs)
                    if (a.name == name)
                    {
                        if (a.type == 3 || a.type == 4) { a.raw = new byte[4]; UxBin.PutU32(a.raw, 0, unchecked((uint)value)); }
                        else if (a.type == 11) a.raw = new[] { checked((byte)value) };
                        else throw new UxException("material: attribute " + name + " of type " + a.type + " cannot hold an integer");
                        return;
                    }
                throw new UxException("KeyError " + name);
            }
            public Node DeepCopy()
            {
                var n = new Node { tag = tag, vtype = vtype, vflag = vflag, value = value is byte[] b ? (object)b.Clone() : value };
                foreach (var a in attrs) n.attrs.Add(a.Copy());
                foreach (var c in children) n.children.Add(c.DeepCopy());
                return n;
            }
        }

        /// decode_tree(data): (root, strings, version, unknown). A type-2 (blob) value takes its node / attr flag as its byte length.
        static Node DecodeTree(byte[] data, out List<string> strings, out long version, out long unknown)
        {
            var b = new UxBxml(data);
            byte[] Val(long t, long off, long flag)
            {
                if (t == 2)
                {
                    if (off + flag > b.pool.Length) throw new UxException("blob at " + off + "+" + flag + " past the pool end " + b.pool.Length);
                    return UxBin.PySlice(b.pool, (int)off, (int)(off + flag));
                }
                return UxBin.PySlice(b.pool, (int)off, (int)(off + UxBxml.Size(t) * Math.Max(1, flag)));
            }
            Node Rec(int i)
            {
                var n = b.nodes[i];
                var node = new Node { tag = b.strings[(int)n[0]], vtype = (int)n[3], vflag = n[2] };
                for (long ai = n[6]; ai < n[6] + n[7]; ai++)
                {
                    var a = b.attrs[(int)ai];
                    var na = new NAttr { name = b.strings[(int)a[0]], type = (int)a[3], flag = a[2] };
                    if (a[3] == 1) na.str = b.strings[(int)a[1]]; else na.raw = Val(a[3], a[1], a[2]);
                    node.attrs.Add(na);
                }
                if (n[1] < 0) node.value = null;
                else if (n[3] == 0 || n[3] == 1) node.value = b.strings[(int)n[1]];
                else node.value = Val(n[3], n[1], n[2]);
                for (long c = n[4]; c < n[4] + n[5]; c++) node.children.Add(Rec((int)c));
                return node;
            }
            var root = Rec(0);
            strings = new List<string>(b.strings); version = b.version; unknown = b.unknown;
            return root;
        }

        /// encode_tree(root, strings, version, unknown): the stock Reflex material layout: node table = sibling lists in pre-order (a
        /// childless node's first-child field = the next free index), attr table = depth-first pre-order, pool = attr values in attr
        /// order then node values in node order, string table in the given order (each string once, every string used).
        static byte[] EncodeTree(Node root, List<string> strings, long version, long unknown)
        {
            var order = new List<Node> { root }; var first = new Dictionary<Node, int>(); var astart = new Dictionary<Node, int>();
            void Lay(Node e) { first[e] = order.Count; order.AddRange(e.children); foreach (var c in e.children) Lay(c); }
            Lay(root);
            var attrs = new List<NAttr>();
            void Dfs(Node e) { astart[e] = attrs.Count; attrs.AddRange(e.attrs); foreach (var c in e.children) Dfs(c); }
            Dfs(root);
            var sidx = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int i = 0; i < strings.Count; i++)
            {
                if (sidx.ContainsKey(strings[i])) throw new UxException("duplicate strings in the string table");
                sidx[strings[i]] = i;
            }
            var used = new HashSet<string>(StringComparer.Ordinal);
            uint Sx(string s)
            {
                used.Add(s);
                if (!sidx.TryGetValue(s, out int i)) throw new UxException("KeyError " + s);
                return (uint)i;
            }
            var pool = new UxWriter(256); var arows = new UxWriter(256); var nrows = new UxWriter(256);
            foreach (var a in attrs)
            {
                if (a.type == 1) { uint nm = Sx(a.name), v = Sx(a.str); arows.U32(nm).U32(v).U16((ushort)a.flag).U16(1); }
                else { arows.U32(Sx(a.name)).U32((uint)pool.Length).U16((ushort)a.flag).U16((ushort)a.type); pool.Bytes(a.raw); }
            }
            foreach (var e in order)
            {
                int inner;
                if (e.value == null) inner = -1;
                else if (e.vtype == 0 || e.vtype == 1) inner = (int)Sx((string)e.value);
                else { inner = pool.Length; pool.Bytes((byte[])e.value); }
                nrows.U32(Sx(e.tag)).I32(inner).U32((uint)e.vflag).U32((uint)e.vtype).U32((uint)first[e]).U32((uint)e.children.Count).U32((uint)astart[e]).U32((uint)e.attrs.Count);
            }
            if (used.Count != strings.Count) throw new UxException("unused strings in the string table");
            var raw = new UxWriter(1024);
            foreach (string s in strings) { raw.Bytes(Encoding.UTF8.GetBytes(s)); raw.U8(0); }
            int pp = raw.Length;
            raw.Bytes(pool.ToArray()).Bytes(arows.ToArray()).Bytes(nrows.ToArray());
            return UxBxml.Pack(version, strings.Count, pp, pool.Length, attrs.Count, order.Count, unknown, raw.ToArray());
        }

        static List<KeyValuePair<uint, uint>> BlobPairs(object blob)
        {
            var b = blob as byte[];
            if (b == null) throw new UxException("material: a state node without a blob value");
            var o = new List<KeyValuePair<uint, uint>>(b.Length / 8);
            for (int i = 0; i < b.Length / 8; i++) o.Add(new KeyValuePair<uint, uint>(UxBin.U32LE(b, 8 * i), UxBin.U32LE(b, 8 * i + 4)));
            return o;
        }
        /// set_state_blob(node, pairs): (id, value) pairs into a XenonStates / XenonSamplerStates node (blob, byte-length flag, numStates).
        static void SetStateBlob(Node node, List<KeyValuePair<uint, uint>> pairs)
        {
            var b = new byte[8 * pairs.Count];
            for (int i = 0; i < pairs.Count; i++) { UxBin.PutU32(b, 8 * i, pairs[i].Key); UxBin.PutU32(b, 8 * i + 4, pairs[i].Value); }
            node.value = b; node.vflag = b.Length;
            node.SetAttr("numStates", pairs.Count);
        }

        // D3D9 PC render / sampler state ids by name (the PC side of uxmat's Xenon translation tables)
        static readonly Dictionary<string, uint> PC_RS = new Dictionary<string, uint>(StringComparer.Ordinal), PC_SS = new Dictionary<string, uint>(StringComparer.Ordinal);
        static readonly Dictionary<uint, string> PC_RS_NAME = new Dictionary<uint, string>(), PC_SS_NAME = new Dictionary<uint, string>();
        static UxMaterials()
        {
            foreach (uint id in UxMat.XrsIds) { UxMat.Xrs(id, out string name, out uint pc); PC_RS[name] = pc; }
            foreach (var kv in PC_RS) PC_RS_NAME[kv.Value] = kv.Key;
            for (uint id = 0; id <= 36; id += 4) if (UxMat.Xss(id, out string name, out uint pc)) PC_SS[name] = pc;
            foreach (var kv in PC_SS) PC_SS_NAME[kv.Value] = kv.Key;
        }
        /// named_pc(pairs, sampler): [[name, value]].
        static List<object> NamedPc(List<KeyValuePair<uint, uint>> pairs, bool sampler = false)
        {
            var nm = sampler ? PC_SS_NAME : PC_RS_NAME; var o = new List<object>(pairs.Count);
            foreach (var kv in pairs) o.Add(new List<object> { nm.TryGetValue(kv.Key, out string n) ? n : I(kv.Key), (long)kv.Value });
            return o;
        }

        // ============================================================================== sm3dis: the stock templates' UV sets
        /// The D3D9 shader model 2/3 bytecode reader of templates/reflex-lib/sm3dis.py: every instruction decoded and, per register
        /// component, the set of vertex-shader INPUT components that can reach it; link() then gives, per pixel-shader sampler, the mesh
        /// UV sets behind its coordinates. Control flow is ignored (all writes merged in program order), as in the reference.
        static class Sm3
        {
            static readonly string[] USAGE = { "POSITION", "BLENDWEIGHT", "BLENDINDICES", "NORMAL", "PSIZE", "TEXCOORD", "TANGENT", "BINORMAL", "TESSFACTOR", "POSITIONT", "COLOR", "FOG", "DEPTH", "SAMPLE" };
            static readonly string[] REG = { "r", "v", "c", "a", "rasterout", "attrout", "o", "i", "oC", "oDepth", "s", "c2_", "c3_", "c4_", "b", "aL", "h", "misc", "l", "p" };
            const string COMP = "xyzw";
            static string TexType(uint t) => t == 0 ? "unknown" : t == 2 ? "2d" : t == 3 ? "cube" : t == 4 ? "volume" : null;
            // opcode -> (name, has_dst, kind): 'cw' componentwise, 'red3' / 'red4' reduction, 'tex', 'flow', 'dcl', 'def', 'scalar', 'mat', 'dp2add'
            static readonly Dictionary<int, (string name, bool dst, string kind)> OPS = new Dictionary<int, (string, bool, string)>
            {
                { 0, ("nop", false, "flow") }, { 1, ("mov", true, "cw") }, { 2, ("add", true, "cw") }, { 3, ("sub", true, "cw") }, { 4, ("mad", true, "cw") },
                { 5, ("mul", true, "cw") }, { 6, ("rcp", true, "scalar") }, { 7, ("rsq", true, "scalar") }, { 8, ("dp3", true, "red3") }, { 9, ("dp4", true, "red4") },
                { 10, ("min", true, "cw") }, { 11, ("max", true, "cw") }, { 12, ("slt", true, "cw") }, { 13, ("sge", true, "cw") }, { 14, ("exp", true, "scalar") },
                { 15, ("log", true, "scalar") }, { 16, ("lit", true, "red4") }, { 17, ("dst", true, "red4") }, { 18, ("lrp", true, "cw") }, { 19, ("frc", true, "cw") },
                { 20, ("m4x4", true, "mat") }, { 21, ("m4x3", true, "mat") }, { 22, ("m3x4", true, "mat") }, { 23, ("m3x3", true, "mat") }, { 24, ("m3x2", true, "mat") },
                { 25, ("call", false, "flow") }, { 26, ("callnz", false, "flow") }, { 27, ("loop", false, "flow") }, { 28, ("ret", false, "flow") },
                { 29, ("endloop", false, "flow") }, { 30, ("label", false, "flow") }, { 31, ("dcl", true, "dcl") }, { 32, ("pow", true, "scalar") },
                { 33, ("crs", true, "red3") }, { 34, ("sgn", true, "cw") }, { 35, ("abs", true, "cw") }, { 36, ("nrm", true, "red3") }, { 37, ("sincos", true, "scalar") },
                { 38, ("rep", false, "flow") }, { 39, ("endrep", false, "flow") }, { 40, ("if", false, "flow") }, { 41, ("ifc", false, "flow") }, { 42, ("else", false, "flow") },
                { 43, ("endif", false, "flow") }, { 44, ("break", false, "flow") }, { 45, ("breakc", false, "flow") }, { 46, ("mova", true, "cw") },
                { 47, ("defb", true, "def") }, { 48, ("defi", true, "def") }, { 64, ("texcoord", true, "cw") }, { 65, ("texkill", false, "flow") },
                { 66, ("texld", true, "tex") }, { 67, ("texbem", true, "red4") }, { 68, ("texbeml", true, "red4") }, { 69, ("texreg2ar", true, "red4") },
                { 70, ("texreg2gb", true, "red4") }, { 71, ("texm3x2pad", true, "red4") }, { 72, ("texm3x2tex", true, "red4") }, { 73, ("texm3x3pad", true, "red4") },
                { 74, ("texm3x3tex", true, "red4") }, { 76, ("texm3x3spec", true, "red4") }, { 77, ("texm3x3vspec", true, "red4") }, { 78, ("expp", true, "scalar") },
                { 79, ("logp", true, "scalar") }, { 80, ("cnd", true, "cw") }, { 81, ("def", true, "def") }, { 82, ("texreg2rgb", true, "red4") },
                { 83, ("texdp3tex", true, "red4") }, { 84, ("texm3x2depth", true, "red4") }, { 85, ("texdp3", true, "red4") }, { 86, ("texm3x3", true, "red4") },
                { 87, ("texdepth", true, "red4") }, { 88, ("cmp", true, "cw") }, { 89, ("bem", true, "red4") }, { 90, ("dp2add", true, "dp2add") },
                { 91, ("dsx", true, "cw") }, { 92, ("dsy", true, "cw") }, { 93, ("texldd", true, "tex") }, { 94, ("setp", true, "cw") }, { 95, ("texldl", true, "tex") },
                { 96, ("breakp", false, "flow") },
            };
            static string SrcMod(int mod, string s)
            {
                switch (mod)
                {
                    case 0: return s; case 1: return "-" + s; case 2: return s + "_bias"; case 3: return "-" + s + "_bias"; case 4: return s + "_bx2"; case 5: return "-" + s + "_bx2";
                    case 6: return s + "_x2"; case 7: return "-" + s + "_x2"; case 8: return s + "_dz"; case 9: return s + "_dw"; case 10: return "abs(" + s + ")"; case 11: return "-abs(" + s + ")";
                    case 12: return "!" + s;
                }
                return s + "?mod";
            }

            sealed class Param
            {
                public uint raw; public int type, num; public bool rel, isDst; public uint? reltok; public int mask, rmod, mod; public int[] swz;
                public Param(uint tok, bool isDst, uint? reltok = null)
                {
                    raw = tok; type = (int)(((tok >> 28) & 7) | ((tok >> 8) & 0x18)); num = (int)(tok & 0x7FF); rel = (tok & 0x2000) != 0; this.reltok = reltok; this.isDst = isDst;
                    if (isDst) { mask = (int)((tok >> 16) & 0xF); rmod = (int)((tok >> 20) & 0xF); }
                    else { uint s = (tok >> 16) & 0xFF; swz = new[] { (int)(s & 3), (int)((s >> 2) & 3), (int)((s >> 4) & 3), (int)((s >> 6) & 3) }; mod = (int)((tok >> 24) & 0xF); }
                }
                /// Destination result modifiers as an opcode suffix (_sat, _pp, _centroid).
                public string Suffix() => !isDst ? "" : ((rmod & 1) != 0 ? "_sat" : "") + ((rmod & 2) != 0 ? "_pp" : "") + ((rmod & 4) != 0 ? "_centroid" : "");
                public (string, int) Key() => (type < REG.Length ? REG[type] : "t" + I(type), num);
                public string Text(bool ps)
                {
                    string rn = type < REG.Length ? REG[type] : "?" + I(type), s;
                    if (type == 3 && ps) rn = "t";
                    if (type == 17) { rn = num == 0 ? "vPos" : num == 1 ? "vFace" : "misc" + I(num); s = rn; }
                    else if (type == 9) s = "oDepth";
                    else s = rn + I(num);
                    if (rel)
                    {
                        if (reltok != null) { var rt = new Param(reltok.Value, false); s = rn + "[" + (rt.type < REG.Length ? REG[rt.type] : "?") + I(rt.num) + "." + COMP[rt.swz[0]] + " + " + I(num) + "]"; }
                        else s = rn + "[a0 + " + I(num) + "]";
                    }
                    if (isDst)
                    {
                        if (mask != 0xF) { s += "."; for (int i = 0; i < 4; i++) if (((mask >> i) & 1) != 0) s += COMP[i]; }
                        return s;
                    }
                    if (!(swz[0] == 0 && swz[1] == 1 && swz[2] == 2 && swz[3] == 3))
                    {
                        string sw = "" + COMP[swz[0]] + COMP[swz[1]] + COMP[swz[2]] + COMP[swz[3]];
                        if (swz[0] == swz[1] && swz[1] == swz[2] && swz[2] == swz[3]) sw = "" + COMP[swz[0]];
                        s += "." + sw;
                    }
                    return SrcMod(mod, s);
                }
            }
            sealed class Instr { public int op, ctl; public string name, kind; public bool pred; public Param dst, predsrc; public List<Param> srcs = new List<Param>(); public uint extra; }

            /// parse(code): (version token, instructions); comments (CTAB etc.) are skipped.
            static List<Instr> Parse(byte[] code, out uint ver)
            {
                int n = code.Length / 4;
                var toks = new uint[n]; for (int k = 0; k < n; k++) toks[k] = UxBin.U32LE(code, 4 * k);
                UxBin.Require(n > 0, "sm3dis: empty shader");
                ver = toks[0];
                int major = (int)((ver >> 8) & 0xFF);
                var o = new List<Instr>();
                int i = 1;
                while (i < n)
                {
                    uint t = toks[i]; int op = (int)(t & 0xFFFF);
                    if (op == 0xFFFE) { i += 1 + (int)((t >> 16) & 0x7FFF); continue; }
                    if (op == 0xFFFF) break;
                    if (!OPS.TryGetValue(op, out var d)) throw new UxException("sm3dis: unknown opcode " + op + " at token " + i);
                    if (major < 2) throw new UxException("sm3dis: shader model < 2 not supported");
                    int ln = (int)((t >> 24) & 0x0F);
                    var body = new List<uint>(); for (int k = i + 1; k < i + 1 + ln && k < n; k++) body.Add(toks[k]);
                    var ins = new Instr { op = op, name = d.name, kind = d.kind, ctl = (int)((t >> 16) & 0xFF), pred = (t & 0x10000000) != 0 };
                    uint B(int k) { if (k >= body.Count) throw new UxException("sm3dis: truncated instruction at token " + i); return body[k]; }
                    int j = 0;
                    if (d.kind == "dcl") { ins.extra = B(0); ins.dst = new Param(B(1), true); }
                    else if (d.kind == "def") ins.dst = new Param(B(0), true);
                    else
                    {
                        if (d.dst && op != 65)
                        {
                            uint dt = B(j++); uint? rel = null;
                            if ((dt & 0x2000) != 0 && major >= 3) rel = B(j++);
                            ins.dst = new Param(dt, true, rel);
                        }
                        if (ins.pred) ins.predsrc = new Param(B(j++), false);
                        while (j < body.Count)
                        {
                            uint s = body[j++]; uint? rel = null;
                            if ((s & 0x2000) != 0 && major >= 2) rel = B(j++);
                            ins.srcs.Add(new Param(s, false, rel));
                        }
                    }
                    o.Add(ins);
                    i += 1 + ln;
                }
                return o;
            }

            /// disasm(code), without the 'def' lines (template_uv keeps only dcl_texcoord, texld and 'mov o' lines).
            public static List<string> Disasm(byte[] code)
            {
                var ins = Parse(code, out uint ver);
                bool ps = (ver >> 16) == 0xFFFF;
                var lines = new List<string>();
                foreach (var x in ins)
                {
                    if (x.kind == "def") continue;
                    if (x.kind == "dcl")
                    {
                        uint u = x.extra;
                        if (x.dst.type == 10) lines.Add("dcl_" + (TexType((u >> 27) & 0xF) ?? "?") + " " + x.dst.Text(ps));
                        else lines.Add("dcl_" + ((u & 0x1F) < USAGE.Length ? USAGE[u & 0x1F] : "?").ToLowerInvariant() + I((u >> 16) & 0xF) + x.dst.Suffix() + " " + x.dst.Text(ps));
                        continue;
                    }
                    string nm = x.name;
                    if (x.op == 66) nm = (x.ctl & 3) == 0 ? "texld" : (x.ctl & 3) == 1 ? "texldp" : (x.ctl & 3) == 2 ? "texldb" : "texld?";
                    if (x.op == 41 || x.op == 45 || x.op == 94)
                    {
                        int c = x.ctl & 7;
                        nm += "_" + (c == 1 ? "gt" : c == 2 ? "eq" : c == 3 ? "ge" : c == 4 ? "lt" : c == 5 ? "ne" : c == 6 ? "le" : "?");
                    }
                    var p = new List<string>();
                    if (x.dst != null) p.Add(x.dst.Text(ps));
                    foreach (var s in x.srcs) p.Add(s.Text(ps));
                    string pre = x.predsrc != null ? "(" + x.predsrc.Text(ps) + ") " : "";
                    if (x.dst != null) nm += x.dst.Suffix();
                    lines.Add(pre + nm + " " + string.Join(", ", p));
                }
                return lines;
            }

            sealed class Sample { public int sampler; public List<KeyValuePair<string, HashSet<(string, string)>>> coord = new List<KeyValuePair<string, HashSet<(string, string)>>>(); }
            sealed class Traced
            {
                public bool ps; public Dictionary<(string, char), HashSet<(string, string)>> out_taint = new Dictionary<(string, char), HashSet<(string, string)>>();
                public List<Sample> samples = new List<Sample>();
            }
            static readonly HashSet<(string, string)> s_empty = new HashSet<(string, string)>();

            /// trace(code): the per-component data flow.
            static Traced Trace(byte[] code)
            {
                var ins = Parse(code, out uint ver);
                bool ps = (ver >> 16) == 0xFFFF;
                var taint = new Dictionary<(string, int), HashSet<(string, string)>[]>();
                var outputs = new List<KeyValuePair<int, List<(string sem, int mask)>>>(); var samplers = new Dictionary<int, string>();
                HashSet<(string, string)>[] Get((string, int) key) => taint.TryGetValue(key, out var v) ? v : new[] { s_empty, s_empty, s_empty, s_empty };
                HashSet<(string, string)> SrcComp(Param p, int c)
                {
                    var k = p.Key();
                    if (k.Item1 == "c" || k.Item1 == "i" || k.Item1 == "b" || k.Item1 == "c2_" || k.Item1 == "c3_" || k.Item1 == "c4_" || k.Item1 == "l") return s_empty;
                    if (k.Item1 == "misc") return new HashSet<(string, string)> { (p.num == 0 ? "vPos" : p.num == 1 ? "vFace" : "misc", "" + COMP[p.swz[c]]) };
                    var t = Get(k)[p.swz[c]];
                    if (p.rel && p.reltok != null)
                    {
                        var rt = new Param(p.reltok.Value, false);
                        t = new HashSet<(string, string)>(t); t.UnionWith(Get(rt.Key())[rt.swz[0]]);
                    }
                    return t;
                }
                HashSet<(string, string)> SrcAll(Param p, int n) { var s = new HashSet<(string, string)>(); for (int c = 0; c < n; c++) s.UnionWith(SrcComp(p, c)); return s; }
                var tr = new Traced { ps = ps };
                foreach (var x in ins)
                {
                    if (x.kind == "dcl")
                    {
                        uint u = x.extra; var dd = x.dst;
                        if (dd.type == 10) { samplers[dd.num] = TexType((u >> 27) & 0xF) ?? "?"; continue; }
                        string sem = ((u & 0x1F) < USAGE.Length ? USAGE[u & 0x1F] : "U" + I(u & 0x1F)) + I((u >> 16) & 0xF);
                        if (dd.type == 1 || (dd.type == 3 && ps))
                        {
                            var cur = (HashSet<(string, string)>[])Get(dd.Key()).Clone();
                            for (int c = 0; c < 4; c++) if (((dd.mask >> c) & 1) != 0) cur[c] = new HashSet<(string, string)> { (sem, "" + COMP[c]) };
                            taint[dd.Key()] = cur;
                        }
                        else if (dd.type == 6)
                        {
                            int at = outputs.FindIndex(kv => kv.Key == dd.num);
                            if (at < 0) { outputs.Add(new KeyValuePair<int, List<(string, int)>>(dd.num, new List<(string, int)>())); at = outputs.Count - 1; }
                            outputs[at].Value.Add((sem, dd.mask));
                        }
                        continue;
                    }
                    if (x.kind == "def" || x.kind == "flow" || x.dst == null) continue;
                    var d = x.dst;
                    var vals = new HashSet<(string, string)>[4];
                    HashSet<(string, string)> v;
                    switch (x.kind)
                    {
                        case "cw":
                            for (int c = 0; c < 4; c++) { var s = new HashSet<(string, string)>(); foreach (var p in x.srcs) s.UnionWith(SrcComp(p, c)); vals[c] = s; }
                            break;
                        case "scalar": case "red3": case "red4":
                            v = new HashSet<(string, string)>();
                            foreach (var p in x.srcs) v.UnionWith(SrcAll(p, x.kind == "red3" ? 3 : 4));
                            for (int c = 0; c < 4; c++) vals[c] = v;
                            break;
                        case "dp2add":
                            v = SrcAll(x.srcs[0], 2); v.UnionWith(SrcAll(x.srcs[1], 2)); v.UnionWith(SrcAll(x.srcs[2], 1));
                            for (int c = 0; c < 4; c++) vals[c] = v;
                            break;
                        case "mat":
                            v = SrcAll(x.srcs[0], x.name == "m4x4" || x.name == "m4x3" ? 4 : 3);
                            for (int c = 0; c < 4; c++) vals[c] = v;
                            break;
                        case "tex":
                        {
                            var coord = x.srcs[0]; var samp = x.srcs[1];
                            string tt = samplers.TryGetValue(samp.num, out string st) ? st : "2d";
                            bool useW = x.op == 95 || (x.op == 66 && ((x.ctl & 3) == 1 || (x.ctl & 3) == 2));
                            var sm = new Sample { sampler = samp.num };
                            sm.coord.Add(new KeyValuePair<string, HashSet<(string, string)>>("u", SrcComp(coord, 0)));
                            sm.coord.Add(new KeyValuePair<string, HashSet<(string, string)>>("v", SrcComp(coord, 1)));
                            if (tt != "2d") sm.coord.Add(new KeyValuePair<string, HashSet<(string, string)>>("w", SrcComp(coord, 2)));
                            if (useW) sm.coord.Add(new KeyValuePair<string, HashSet<(string, string)>>("q", SrcComp(coord, 3)));
                            tr.samples.Add(sm);
                            v = new HashSet<(string, string)> { ("sample", "s" + I(samp.num)) };
                            foreach (var kv in sm.coord) v.UnionWith(kv.Value);
                            for (int c = 0; c < 4; c++) vals[c] = v;
                            break;
                        }
                        default:
                            for (int c = 0; c < 4; c++) vals[c] = s_empty;
                            break;
                    }
                    var cur2 = (HashSet<(string, string)>[])Get(d.Key()).Clone();
                    for (int c = 0; c < 4; c++)
                        if (((d.mask >> c) & 1) != 0)
                        {
                            if (x.pred) { var m = new HashSet<(string, string)>(cur2[c]); m.UnionWith(vals[c]); cur2[c] = m; }
                            else cur2[c] = vals[c];
                        }
                    taint[d.Key()] = cur2;
                }
                if (!ps)
                    foreach (var kv in outputs)
                    {
                        var t = Get(("o", kv.Key));
                        foreach (var (sem, mask) in kv.Value)
                            for (int c = 0; c < 4; c++) if (((mask >> c) & 1) != 0) tr.out_taint[(sem, COMP[c])] = t[c];
                    }
                return tr;
            }

            /// link(vs, ps) + uv_sets: per pixel-shader sampler register, for each of its texture reads the sorted mesh UV sets
            /// ('TEXCOORD0' ...) behind its u and v coordinates.
            public static Dictionary<int, List<List<string>>> LinkUv(byte[] vsCode, byte[] psCode)
            {
                var vs = Trace(vsCode); var ps = Trace(psCode);
                var res = new Dictionary<int, List<List<string>>>();
                foreach (var smp in ps.samples)
                {
                    var sets = new SortedSet<string>(StringComparer.Ordinal);
                    foreach (var kv in smp.coord)
                    {
                        if (kv.Key != "u" && kv.Key != "v") continue;
                        foreach (var src in kv.Value)
                        {
                            if (src.Item1 == "sample") continue;                                   // ('dependent-read', s#)
                            if (!vs.out_taint.TryGetValue((src.Item1, src.Item2[0]), out var vt)) continue;   // ('unlinked', 'SEM.c')
                            foreach (var item in vt) if (item.Item1.StartsWith("TEXCOORD", StringComparison.Ordinal)) sets.Add(item.Item1);
                        }
                    }
                    if (!res.TryGetValue(smp.sampler, out var list)) res[smp.sampler] = list = new List<List<string>>();
                    list.Add(new List<string>(sets));
                }
                return res;
            }
        }

        // ============================================================================== library (the reference's reflex-lib)
        sealed class TplUv { public List<string> slots = new List<string>(), registers = new List<string>(), evidence = new List<string>(); public List<List<string>> mesh_uv = new List<List<string>>(); public string vs_entry, ps_entry; }
        sealed class Tpl
        {
            public UxLibTemplate index; public byte[] data, shader; public string md5, shader_md5; public Node root; public List<string> strings; public long version, unknown;
            public List<string> tex_names, slots; public int nslots; public List<KeyValuePair<uint, uint>> states; public List<List<KeyValuePair<uint, uint>>> samplers; public TplUv uv;
        }

        /// template_uv(T, shader_bytes, t): the mesh UV set(s) behind each user sampler of a template, traced through the stock VS -> PS
        /// bytecode, and the instructions behind the answer.
        static TplUv TemplateUv(byte[] shaderBytes, UxLibTemplate t)
        {
            byte[] vs = null, ps = null;
            foreach (var kv in UxD3d9sh.ReadContainer(shaderBytes)) { if (kv.Key == t.vsEntry) vs = kv.Value; if (kv.Key == t.psEntry) ps = kv.Value; }
            if (vs == null || ps == null) throw new UxException("reflex-lib " + t.name + ": entry point " + t.vsEntry + " / " + t.psEntry + " missing");
            var res = Sm3.LinkUv(vs, ps);
            var uv = new TplUv { vs_entry = t.vsEntry, ps_entry = t.psEntry };
            foreach (string s in t.userSamplers)
            {
                int r = -1;
                foreach (var kv in t.samplers) if (kv.Value == "usr_sampler_" + s) r = int.Parse(kv.Key.Substring(1), CultureInfo.InvariantCulture);
                if (r < 0) throw new UxException("reflex-lib " + t.name + ": no register for usr_sampler_" + s);
                uv.slots.Add(s); uv.registers.Add("s" + I(r));
                var sets = new SortedSet<string>(StringComparer.Ordinal);
                if (res.TryGetValue(r, out var reads)) foreach (var read in reads) foreach (string u in read) sets.Add(u);
                uv.mesh_uv.Add(new List<string>(sets));
            }
            foreach (var (name, code) in new[] { (t.vsEntry, vs), (t.psEntry, ps) })
                foreach (string ln in Sm3.Disasm(code))
                {
                    string s = ln.Trim();
                    if (s.StartsWith("dcl_texcoord", StringComparison.Ordinal) || s.StartsWith("texld", StringComparison.Ordinal) || MOV_O.IsMatch(s)) uv.evidence.Add(name + ": " + s);
                }
            return uv;
        }

        static readonly ConditionalWeakTable<UxStock, Dictionary<string, Tpl>> s_libs = new ConditionalWeakTable<UxStock, Dictionary<string, Tpl>>();
        static readonly ConditionalWeakTable<UxStock, Dictionary<string, KeyValuePair<byte[], JObj>>> s_stockTex = new ConditionalWeakTable<UxStock, Dictionary<string, KeyValuePair<byte[], JObj>>>();
        static readonly object s_gate = new object();

        /// lib(): {template: ...} from the stock library (exemplar tree + strings, shader bytes, slots, traced UV sets).
        static Dictionary<string, Tpl> Lib(UxStock stock)
        {
            lock (s_gate)
            {
                if (s_libs.TryGetValue(stock, out var have)) return have;
                var o = new Dictionary<string, Tpl>(StringComparer.Ordinal);
                foreach (string T in TEMPLATES)
                {
                    var t = UxStock.LibTemplate(T) ?? throw new UxException("no reflex-lib template " + T);
                    byte[] data = stock.LibMaterialBytes(T), sh = stock.LibShaderBytes(T);
                    var root = DecodeTree(data, out var strings, out long ver, out long unk);
                    if (!UxBin.Same(EncodeTree(root, strings, ver, unk), data)) throw new UxException("reflex-lib exemplar " + T + " does not re-encode byte-identically");
                    var texn = new List<Node>(); foreach (var n in root.Iter()) if (n.tag == "Texture") texn.Add(n);
                    var texNames = new List<string>(); foreach (var n in texn) texNames.Add(n.AttrStr("textureName"));
                    var slots = new List<string>(t.userSamplers);
                    if (texn.Count != slots.Count || slots.Count != t.numTextures) throw new UxException("reflex-lib " + T + ": " + texn.Count + " Texture nodes, " + slots.Count + " user samplers");
                    foreach (var n in root.Iter())                                   // exemplar texture strings must only name textures
                    {
                        bool bad = texNames.Contains(n.tag) || ((n.vtype == 0 || n.vtype == 1) && n.value is string sv && texNames.Contains(sv));
                        foreach (var a in n.attrs) if (texNames.Contains(a.name) || (a.type == 1 && texNames.Contains(a.str) && a.name != "textureName")) bad = true;
                        if (bad) throw new UxException("reflex-lib " + T + ": exemplar texture name used elsewhere");
                    }
                    var pa = root.Find("Pass") ?? throw new UxException("reflex-lib " + T + ": no Pass");
                    var xs = pa.Find("XenonStates") ?? throw new UxException("reflex-lib " + T + ": no XenonStates");
                    var tp = new Tpl { index = t, data = data, md5 = UxBin.Md5(data), shader = sh, shader_md5 = UxBin.Md5(sh), root = root, strings = strings, version = ver, unknown = unk,
                        tex_names = texNames, slots = slots, nslots = slots.Count, states = BlobPairs(xs.value), samplers = new List<List<KeyValuePair<uint, uint>>>(), uv = TemplateUv(sh, t) };
                    foreach (var n in texn) tp.samplers.Add(BlobPairs((n.Find("XenonSamplerStates") ?? throw new UxException("reflex-lib " + T + ": a Texture without XenonSamplerStates")).value));
                    if (tp.shader_md5 != t.shaderMd5 || tp.md5 != t.exemplarMd5) throw new UxException("reflex-lib " + T + ": md5 differs from index.json");
                    o[T] = tp;
                }
                s_libs.Add(stock, o);
                return o;
            }
        }

        /// stock_texture(name): (bytes, meta) of a stock Reflex PC texture in LIB_TEXTURES, from the first stock level that holds it.
        static KeyValuePair<byte[], JObj> StockTexture(UxStock stock, string name)
        {
            lock (s_gate)
            {
                var cache = s_stockTex.GetOrCreateValue(stock);
                if (cache.TryGetValue(name, out var have)) return have;
                foreach (var row in LIB_TEXTURES)
                {
                    if (row.name != name) continue;
                    foreach (string s in row.levels)
                    {
                        byte[] data;
                        try { if (!stock.library.HasLevel(s)) continue; data = stock.ReadStock(s, name, "texture"); }
                        catch (UxException) { continue; }
                        if (data == null) continue;
                        string md5 = UxBin.Md5(data);
                        if (md5 != row.md5 || data.Length != row.size) throw new UxException("stock texture " + name + " (" + s + ") does not match its pinned md5 " + row.md5 + " (got " + md5 + ": another Reflex version?)");
                        var t = new UxPcTexture(data);
                        var (mips, w, h, fmt) = t.recs[0];
                        var meta = new JObj().Set("name", name).Set("source", s + ".dx9.package (game Database, read-only)").Set("md5", md5).Set("size", (long)data.Length)
                            .Set("width", (long)w).Set("height", (long)h).Set("mips", (long)mips).Set("format", UxTex.FmtText(fmt)).Set("nsurf", (long)t.nsurf).Set("kind", (long)t.kind)
                            .Set("end_ok", t.end == data.Length);
                        return cache[name] = new KeyValuePair<byte[], JObj>(data, meta);
                    }
                    throw new UxException("stock texture " + name + " not found in (" + string.Join(", ", row.levels) + ") of the Reflex library");
                }
                throw new UxException("KeyError " + name);
            }
        }

        /// remap_strings(ex_strings, ex_tex, new_tex): the exemplar string table with the exemplar texture names replaced, position
        /// for position, by the new distinct names (extra names follow the last texture position; names equal to a structural string
        /// reuse it).
        static List<string> RemapStrings(List<string> exStrings, List<string> exTex, List<string> newTex)
        {
            var exSet = new HashSet<string>(exTex, StringComparer.Ordinal);
            var structural = new HashSet<string>(StringComparer.Ordinal); foreach (string s in exStrings) if (!exSet.Contains(s)) structural.Add(s);
            var nw = new List<string>(); foreach (string t in newTex) if (!nw.Contains(t) && !structural.Contains(t)) nw.Add(t);
            int last = -1; for (int i = 0; i < exStrings.Count; i++) if (exSet.Contains(exStrings[i])) last = i;
            if (last < 0) throw new UxException("IndexError: list index out of range");
            var o = new List<string>(); int k = 0;
            for (int i = 0; i < exStrings.Count; i++)
            {
                string s = exStrings[i];
                if (!exSet.Contains(s)) { o.Add(s); continue; }
                if (k < nw.Count) o.Add(nw[k++]);
                if (i == last) { while (k < nw.Count) o.Add(nw[k++]); }
            }
            return o;
        }

        /// build_material(T, textures, state_patch, sampler_patch): the stock exemplar of template T with the textures (template order,
        /// Reflex asset names), the render states {name: PC value} and the sampler states [{name: value} per slot] set. States the
        /// exemplar lacks are appended.
        static byte[] BuildMaterial(Tpl L, string T, List<string> textures, JObj statePatch, List<JObj> samplerPatch,
            out List<KeyValuePair<uint, uint>> states, out List<List<KeyValuePair<uint, uint>>> samplers, out List<string> added)
        {
            if (textures.Count != L.nslots) throw new UxException(T + " needs " + L.nslots + " textures, got " + textures.Count);
            var root = L.root.DeepCopy();
            var texn = new List<Node>(); foreach (var n in root.Iter()) if (n.tag == "Texture") texn.Add(n);
            for (int i = 0; i < texn.Count && i < textures.Count; i++) texn[i].SetAttr("textureName", textures[i]);
            var xs = root.Find("Pass").Find("XenonStates");
            var pairs = BlobPairs(xs.value);
            added = new List<string>();
            if (statePatch != null)
                foreach (var kv in statePatch.items)
                {
                    if (!PC_RS.TryGetValue(kv.Key, out uint pid)) throw new UxException("KeyError " + kv.Key);
                    uint v = unchecked((uint)(long)kv.Value);
                    int at = pairs.FindIndex(p => p.Key == pid);
                    if (at >= 0) pairs[at] = new KeyValuePair<uint, uint>(pid, v); else { pairs.Add(new KeyValuePair<uint, uint>(pid, v)); added.Add(kv.Key); }
                }
            SetStateBlob(xs, pairs);
            samplers = new List<List<KeyValuePair<uint, uint>>>();
            for (int si = 0; si < texn.Count; si++)
            {
                var ss = texn[si].Find("XenonSamplerStates");
                var sp = BlobPairs(ss.value);
                JObj patch = samplerPatch != null && si < samplerPatch.Count ? samplerPatch[si] : null;
                if (sp.Count > 0)                                         // crowd: stock has no sampler states; keep it that way
                {
                    if (patch != null)
                        foreach (var kv in patch.items)
                        {
                            if (!PC_SS.TryGetValue(kv.Key, out uint pid)) throw new UxException("KeyError " + kv.Key);
                            uint v = unchecked((uint)(long)kv.Value);
                            int at = sp.FindIndex(p => p.Key == pid);
                            if (at >= 0) sp[at] = new KeyValuePair<uint, uint>(pid, v); else { sp.Add(new KeyValuePair<uint, uint>(pid, v)); added.Add("s" + I(si) + "." + kv.Key); }
                        }
                    SetStateBlob(ss, sp);
                }
                samplers.Add(sp);
            }
            states = pairs;
            return EncodeTree(root, RemapStrings(L.strings, L.tex_names, textures), L.version, L.unknown);
        }

        /// build_material(T, textures, state_patch, sampler_patch)[0] for callers outside the stage (the builder's screens, ads and
        /// split gate): the material bytes of template T with these textures (template order, Reflex asset names) and patches
        /// ({state name: PC value}; one {sampler state name: value} per slot).
        public static byte[] BuildMaterial(UxStock stock, string template, IList<string> textures, JObj statePatch = null, IList<JObj> samplerPatch = null)
        {
            var lib = Lib(stock);
            if (!lib.TryGetValue(template, out var L)) throw new UxException("no material template " + template);
            return BuildMaterial(L, template, new List<string>(textures), statePatch, samplerPatch == null ? null : new List<JObj>(samplerPatch), out _, out _, out _);
        }
        /// rebuild_entry(entry, rename): a material's bytes re-created from its materials.json entry, optionally renaming textures
        /// {old: new} (the builder's clash renames).
        public static byte[] RebuildEntry(UxStock stock, JObj entry, IDictionary<string, string> rename = null)
        {
            var tex = new List<string>();
            foreach (object t in UxJson.Arr(entry.Get("textures"))) { string n = UxJson.Str(t); tex.Add(rename != null && rename.TryGetValue(n, out string r) ? r : n); }
            List<JObj> sp = null;
            if (entry.Get("sampler_patch") is List<object> spl) { sp = new List<JObj>(); foreach (object x in spl) sp.Add(x as JObj); }
            return BuildMaterial(stock, UxJson.Str(entry.Get("template")), tex, entry.Get("state_patch") as JObj, sp);
        }
        /// The slot names (user samplers in register order) of a template, e.g. ["prelit_light_ao_texture", "diffuse_base_texture"].
        public static List<string> TemplateSlots(UxStock stock, string template) => new List<string>(Lib(stock)[template].slots);

        // ============================================================================== state translation
        /// translate_states(pairs) / translate_sampler(pairs): ({PC name: PC value}, [unknown (Xenon id, value)]).
        static JObj Translate(List<UxStatePair> pairs, bool sampler, out List<object> unknown)
        {
            var o = new JObj(); unknown = new List<object>();
            foreach (var p in pairs)
            {
                try
                {
                    var one = new[] { p };
                    foreach (var st in sampler ? UxMat.XenonSamplerToPc(one) : UxMat.XenonStatesToPc(one)) o.Set(st.name, (long)st.value);
                }
                catch (UxException) { unknown.Add(new List<object> { (long)p.id, (long)p.value }); }
            }
            return o;
        }
        static string ReprPairs(List<object> unk)
        {
            var p = new List<string>();
            foreach (var x in unk) { var l = (List<object>)x; p.Add("[" + I((long)l[0]) + ", " + I((long)l[1]) + "]"); }
            return "[" + string.Join(", ", p) + "]";
        }

        // ============================================================================== texture roles
        /// texture_role(name, rec): uxmat.texture_role with the format and a few name patterns taken into account.
        static string TextureRole(string name, JObj rec)
        {
            string n = Lower(name);
            if (rec != null && rec.Get("kind") is long k && k == 10) return "reflection";
            if (rec != null && rec.Get("format") as string == "DXN") return "normal";
            string r = UxMat.TextureRole(name);
            if (r == "diffuse")
            {
                if (RAMP2.IsMatch(n)) return "ramp";
                if (n.Contains("sepc")) return "spec";
                if (n.Contains("completemap")) return "lightmap";
            }
            return r;
        }
        static bool IsRenderTarget(JObj rec)
        {
            long w = IntOr(rec, "width", 1), h = IntOr(rec, "height", 1);
            return (IntOr(rec, "flags", 0) & 0x200) != 0 && !((w & (w - 1)) == 0 && (h & (h - 1)) == 0);
        }
        /// pick_diffuse(cands): the canonical name of the most diffuse-like candidate (material order).
        static string PickDiffuse(List<KeyValuePair<string, JObj>> cands)
        {
            if (cands.Count == 0) return null;
            var lows = new List<string>(); foreach (var c in cands) lows.Add(Lower(c.Key));
            int best = -1, bestScore = 0;
            for (int i = 0; i < cands.Count; i++)
            {
                string n = lows[i]; var rec = cands[i].Value;
                int s = 0;
                if (MASKY_SUFFIX.IsMatch(n) || MASKY_WORD.IsMatch(n)) s += 2;
                if (Math.Max(IntOr(rec, "width", 64), IntOr(rec, "height", 64)) <= 32) s += 1;
                if (rec != null && IsRenderTarget(rec)) s += 3;
                foreach (string o in lows) if (o != n && o.StartsWith(n, StringComparison.Ordinal) && SIBLING_SUFFIX.IsMatch(o.Substring(n.Length))) { s -= 1; break; }
                if (best < 0 || s < bestScore) { best = i; bestScore = s; }
            }
            return cands[best].Key;
        }
        static string DefaultTexture(string materialName)
        {
            string n = Lower(materialName);
            if (n.Contains("black") || n.Contains("jumbo") || n.Contains("tron")) return DEFAULT_BLACK;
            if (n.Contains("gray") || n.Contains("grey")) return DEFAULT_GRAY;
            return DEFAULT_WHITE;
        }

        // ============================================================================== templates' asset names
        /// template_clash(name, typ): {kind: the template's spelling or null} for the template kinds in sorted order.
        static List<KeyValuePair<string, string>> TemplateClash(UxStock stock, string name, string typ)
        {
            var kinds = new List<string>(stock.templateNames.Keys); kinds.Sort(string.CompareOrdinal);
            var o = new List<KeyValuePair<string, string>>();
            foreach (string k in kinds) o.Add(new KeyValuePair<string, string>(k, stock.templateNames[k].Find(typ, name)));
            return o;
        }
        static bool AnyClash(List<KeyValuePair<string, string>> cl) { foreach (var kv in cl) if (!string.IsNullOrEmpty(kv.Value)) return true; return false; }
        static JObj ClashDict(List<KeyValuePair<string, string>> cl) { var j = new JObj(); foreach (var kv in cl) if (!string.IsNullOrEmpty(kv.Value)) j.Set(kv.Key, kv.Value); return j; }
        static string ReprDict(JObj j)
        {
            var p = new List<string>(); foreach (var kv in j.items) p.Add(Repr(kv.Key) + ": " + Repr(kv.Value as string));
            return "{" + string.Join(", ", p) + "}";
        }
        /// asset_name(name, typ): ('UX_' + name, 'renamed') on a template clash, the template's spelling for Editor_Default* names
        /// both templates hold ('template'), else (name, 'own'). Without template name tables (a plain Reflex library) nothing clashes.
        public static (string asset, string how) AssetName(UxStock stock, string name, string typ)
        {
            var cl = TemplateClash(stock, name, typ);
            bool all = cl.Count > 0; string sx = null;
            foreach (var kv in cl) { if (string.IsNullOrEmpty(kv.Value)) all = false; if (kv.Key == "sx") sx = kv.Value; }
            if (Lower(name).StartsWith("editor_default", StringComparison.Ordinal) && all && sx != null) return (sx, "template");
            if (AnyClash(cl)) return ("UX_" + name, "renamed");
            return (name, "own");
        }

        // ============================================================================== texture measurements
        /// alpha_range(path, cache, size 256): (min, max) alpha of the ~256 px mip of an Untamed texture; null on error.
        static (int min, int max)? AlphaRange(Ctx ctx, string file, int size = 256)
        {
            string key = "range\n" + file;
            lock (ctx.alpha_cache) if (ctx.alpha_cache.TryGetValue(key, out object hit)) return hit as (int, int)?;
            (int, int)? r = null;
            try
            {
                var t = new UxTexture(ctx.disc.ReadExtractFile(file, ctx.cancel));
                var sf = t.surfaces[0];
                int m = 0;
                while (m + 1 < sf.mips && Math.Max(sf.w >> m, sf.h >> m) > size) m++;
                var img = t.Rgba(0, 0, m);
                if (img.h * img.w == 0) throw new UxException("empty level");
                int lo = 255, hi = 0;
                for (int i = 3; i < img.data.Length; i += 4) { int a = img.data[i]; if (a < lo) lo = a; if (a > hi) hi = a; }
                r = (lo, hi);
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception) { r = null; }
            lock (ctx.alpha_cache) ctx.alpha_cache[key] = r;
            return r;
        }
        /// alpha_cut(path, cache, size 64): the fraction of texels with alpha < 128 in the ~64 px mip (rounded to 4 decimals); null on error.
        static double? AlphaCut(Ctx ctx, string file, int size = 64)
        {
            lock (ctx.alpha_cache) if (ctx.alpha_cache.TryGetValue(file, out object hit)) return hit as double?;
            double? r = null;
            try
            {
                var t = new UxTexture(ctx.disc.ReadExtractFile(file, ctx.cancel));
                var sf = t.surfaces[0];
                int m = 0;
                while (m + 1 < sf.mips && Math.Max(sf.w >> m, sf.h >> m) > size) m++;
                var img = t.Rgba(0, 0, m);
                long n = (long)img.h * img.w, cut = 0;
                if (n == 0) throw new UxException("empty level");
                for (int i = 3; i < img.data.Length; i += 4) if (img.data[i] < 128) cut++;
                r = UxNp.PyRound((double)cut / n, 4);
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception) { r = null; }
            lock (ctx.alpha_cache) ctx.alpha_cache[file] = r;
            return r;
        }

        /// ux_textures.grey_dxn_facts(data)['grey']: a single-surface DXN texture whose decoded mip 0 has R == G (mean |R - G| < 1) and is
        /// not constant (std R > 1): a greyscale logo stored as DXN, not a normal map.
        static bool GreyDxnFacts(byte[] data)
        {
            UxTexture t;
            try { t = new UxTexture(data); } catch (Exception) { return false; }
            if (t.nsurf != 1 || t.faces != 1 || t.Elem(0).name != "DXN") return false;
            var img = t.Rgba(0, 0, 0, nm_preview: false);
            int n = img.h * img.w;
            var d = new double[n]; var y = new double[n];
            for (int i = 0; i < n; i++) { y[i] = img.data[4 * i]; d[i] = Math.Abs((double)img.data[4 * i] - img.data[4 * i + 1]); }
            double rg = UxNp.Sum(d) / n;
            double mean = UxNp.Sum(y) / n;
            for (int i = 0; i < n; i++) { double x = y[i] - mean; d[i] = x * x; }
            double sd = Math.Sqrt(UxNp.Sum(d) / n);
            return rg < 1.0 && sd > 1.0;
        }

        // ============================================================================== generated textures (ux_textures.py)
        sealed class GLevel { public int h, w; public double[] d; }     // float64 RGBA (h, w, 4), 0..255

        /// _box_half(img): the 2x2 box filter to the next mip (numpy's order of the four additions; a 1-px side averages pairs).
        static GLevel BoxHalf(GLevel a)
        {
            int h = a.h, w = a.w, nh = Math.Max(1, h / 2), nw = Math.Max(1, w / 2);
            if (h > 1 && w > 1)
            {
                var o = new double[nh * nw * 4];
                for (int i = 0; i < nh; i++)
                    for (int j = 0; j < nw; j++)
                        for (int c = 0; c < 4; c++)
                            o[(i * nw + j) * 4 + c] = (((a.d[((2 * i) * w + 2 * j) * 4 + c] + a.d[((2 * i) * w + 2 * j + 1) * 4 + c]) + a.d[((2 * i + 1) * w + 2 * j) * 4 + c]) + a.d[((2 * i + 1) * w + 2 * j + 1) * 4 + c]) / 4;
                return new GLevel { h = nh, w = nw, d = o };
            }
            if (h > 1)
            {
                var o = new double[nh * w * 4];
                for (int i = 0; i < nh; i++) for (int x = 0; x < w * 4; x++) o[i * w * 4 + x] = (a.d[(2 * i) * w * 4 + x] + a.d[(2 * i + 1) * w * 4 + x]) / 2;
                return new GLevel { h = nh, w = w, d = o };
            }
            if (w > 1)
            {
                var o = new double[h * nw * 4];
                for (int i = 0; i < h; i++) for (int j = 0; j < nw; j++) for (int c = 0; c < 4; c++) o[(i * nw + j) * 4 + c] = (a.d[(i * w + 2 * j) * 4 + c] + a.d[(i * w + 2 * j + 1) * 4 + c]) / 2;
                return new GLevel { h = h, w = nw, d = o };
            }
            return a;
        }

        /// A recipe value error of the reference (ValueError): the caller treats it as "no such texture".
        sealed class GenValueError : Exception { public GenValueError(string m) : base(m) { } }

        static double[] Floats(object list) { var l = UxJson.Arr(list); var o = new double[l.Count]; for (int i = 0; i < o.Length; i++) o[i] = UxJson.Num(l[i]); return o; }
        static bool Pot(int x) => x > 0 && (x & (x - 1)) == 0;

        /// gen_reference(src_bytes, recipe): the exact float RGBA (0..255) levels of a generated texture (level 0 first):
        ///   composite      {source, bg, k}: rgb = sat((t.rgb + k) * t.a + bg), a = 255 (the Untamed banner composite baked)
        ///   channel_alpha  {source, channel, scale, offset}: a = sat(scale * t.channel + offset), rgb = a
        ///   literal        {rgb}: a constant albedo, a = 255          const_alpha {alpha}: rgb 255, a = the constant alpha
        /// fromSource = the number of leading levels made from source mips (the rest are box filters of the level above).
        static List<GLevel> GenReference(byte[] srcBytes, JObj recipe, out int fromSource)
        {
            string kind = recipe.Get("kind") as string;
            if (kind != "composite" && kind != "literal" && kind != "const_alpha" && kind != "channel_alpha" && kind != "tiled_alpha" && kind != "tint") throw new GenValueError("unknown generated kind " + Repr(kind));
            var refs = new List<GLevel>();
            if (kind == "literal" || kind == "const_alpha")
            {
                double[] rgb = kind == "literal" ? Floats(recipe.Get("rgb")) : new[] { 1.0, 1.0, 1.0 };
                double a = kind == "literal" ? 1.0 : UxJson.Num(recipe.Get("alpha"));
                var px = new double[4];
                for (int c = 0; c < 4; c++) px[c] = UxNp.Round(UxNp.Clip(c < 3 ? rgb[c] : a, 0, 1) * 255);
                const int GEN_SMALL = 4;
                for (int m = 0; m < 3; m++)
                {
                    int sz = Math.Max(1, GEN_SMALL >> m); var d = new double[sz * sz * 4];
                    for (int i = 0; i < sz * sz; i++) for (int c = 0; c < 4; c++) d[4 * i + c] = px[c];
                    refs.Add(new GLevel { h = sz, w = sz, d = d });
                }
                fromSource = 3;
                return refs;
            }
            if (srcBytes == null) throw new UxException("generated texture: the recipe has no source texture");
            if (srcBytes.Length >= 0x28 && UxBin.U32BE(srcBytes, 0) != UxTex.MAGIC) throw new GenValueError("not a texture (magic " + UxBin.U32BE(srcBytes, 0).ToString("x8") + ")");
            var t = new UxTexture(srcBytes);
            if (t.nsurf < 1 || t.faces != 1) throw new GenValueError("source is not a 2D texture");
            var s = t.surfaces[0];
            int w = s.w, h = s.h;
            if (!(Pot(w) && Pot(h))) throw new GenValueError("non-power-of-two source " + w + "x" + h);
            if (kind == "tiled_alpha")
            {
                // FREESTYLE.md 11: level 0 texel (i, j) = the source mip 0 `channel`, bilinear with wrap, at u = frac(tu (j + 0.5) / size),
                // v = frac(tv (i + 0.5) / size); rgb = a; lower levels = 2 x 2 box filters of the level above (from_source 1)
                object so = recipe.Get("size");
                long size = so is long sl ? sl : (long)UxJson.Num(so);
                var tl = recipe.Get("tiles") as List<object>;
                if (tl == null || tl.Count != 2 || !(tl[0] is long tu) || !(tl[1] is long tv) || !Pot((int)size) || tu < 1 || tv < 1)
                    throw new GenValueError("tiled_alpha: size " + so + " / tiles " + UxJson.Dumps(recipe.Get("tiles"), -1));
                int tch = "xyzw".IndexOf(UxJson.Str(recipe.Get("channel")), StringComparison.Ordinal);
                if (tch < 0) throw new GenValueError("substring not found");
                var img0 = t.Rgba(0, 0, 0, nm_preview: false);
                int S = (int)size;
                double Px(int y, int x) => img0.data[(y * img0.w + x) * 4 + tch] / 255.0;
                void Axis(int nsrc, long tiles, out int[] k0, out int[] k1, out double[] f)
                {
                    k0 = new int[S]; k1 = new int[S]; f = new double[S];
                    for (int j = 0; j < S; j++)
                    {
                        double x = tiles * (j + 0.5) / size;
                        double md = x % 1.0;                                    // np.mod(x, 1.0), x >= 0
                        if (md < 0) md += 1.0;
                        double p = md * nsrc - 0.5;
                        double fl = Math.Floor(p);
                        f[j] = p - fl;
                        long ki = (long)fl;
                        k0[j] = (int)(((ki % nsrc) + nsrc) % nsrc); k1[j] = (int)((((ki + 1) % nsrc) + nsrc) % nsrc);
                    }
                }
                Axis(w, tu, out int[] x0, out int[] x1, out double[] fx);
                Axis(h, tv, out int[] y0, out int[] y1, out double[] fy);
                var o0 = new double[S * S * 4];
                for (int i = 0; i < S; i++)
                    for (int j = 0; j < S; j++)
                    {
                        double top = Px(y0[i], x0[j]) * (1 - fx[j]) + Px(y0[i], x1[j]) * fx[j];
                        double bot = Px(y1[i], x0[j]) * (1 - fx[j]) + Px(y1[i], x1[j]) * fx[j];
                        double a = (top * (1 - fy[i]) + bot * fy[i]) * 255.0;
                        int q = 4 * (i * S + j); o0[q] = a; o0[q + 1] = a; o0[q + 2] = a; o0[q + 3] = a;
                    }
                refs.Add(new GLevel { h = S, w = S, d = o0 });
                while (refs[refs.Count - 1].h > 1 || refs[refs.Count - 1].w > 1) refs.Add(BoxHalf(refs[refs.Count - 1]));
                fromSource = 1;
                return refs;
            }
            int nlev = UxTex.Log2Ceil(Math.Max(w, h)) + 1;
            double[] bg = null, kk = null, tsc = null, tof = null; int ch = 0; double scale = 0, offset = 0;
            if (kind == "composite") { bg = Floats(recipe.Get("bg")); kk = Floats(recipe.Get("k")); }
            else if (kind == "tint") { tsc = Floats(recipe.Get("scale")); tof = Floats(recipe.Get("offset")); }
            else
            {
                ch = "xyzw".IndexOf(UxJson.Str(recipe.Get("channel")), StringComparison.Ordinal);
                if (ch < 0) throw new GenValueError("substring not found");
                scale = UxJson.Num(recipe.Get("scale")); offset = UxJson.Num(recipe.Get("offset"));
            }
            for (int m = 0; m < nlev; m++)
            {
                if (m >= s.mips) { refs.Add(BoxHalf(refs[refs.Count - 1])); continue; }
                var img = t.Rgba(0, 0, m, nm_preview: false);
                int lh = Math.Min(img.h, Math.Max(1, h >> m)), lw = Math.Min(img.w, Math.Max(1, w >> m));
                var o = new double[lh * lw * 4];
                for (int y = 0; y < lh; y++)
                    for (int x = 0; x < lw; x++)
                    {
                        int p = (y * img.w + x) * 4, q = (y * lw + x) * 4;
                        if (kind == "composite")
                        {
                            double al = img.data[p + 3] / 255.0;
                            for (int c = 0; c < 3; c++) o[q + c] = UxNp.Clip((img.data[p + c] / 255.0 + kk[c]) * al + bg[c], 0, 1) * 255.0;
                            o[q + 3] = 255.0;
                        }
                        else if (kind == "tint")
                        {
                            for (int c = 0; c < 3; c++) o[q + c] = UxNp.Clip(img.data[p + c] / 255.0 * tsc[c] + tof[c], 0, 1) * 255.0;
                            o[q + 3] = 255.0;
                        }
                        else
                        {
                            double a = UxNp.Clip(scale * (img.data[p + ch] / 255.0) + offset, 0, 1) * 255.0;
                            o[q] = a; o[q + 1] = a; o[q + 2] = a; o[q + 3] = a;
                        }
                    }
                refs.Add(new GLevel { h = lh, w = lw, d = o });
            }
            fromSource = Math.Min(nlev, s.mips);
            return refs;
        }

        static int To565(double r, double g, double b)
        {
            int R = (int)UxNp.Clip(UxNp.Round(r * 31 / 255.0), 0, 31), G = (int)UxNp.Clip(UxNp.Round(g * 63 / 255.0), 0, 63), B = (int)UxNp.Clip(UxNp.Round(b * 31 / 255.0), 0, 31);
            return R << 11 | G << 5 | B;
        }
        static void From565(int v, double[] p, int at)
        {
            int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            p[at] = (r << 3) | (r >> 2); p[at + 1] = (g << 2) | (g >> 4); p[at + 2] = (b << 3) | (b >> 2);
        }
        /// The 16 texels of block (by, bx) with the edges replicated into partial blocks (np.pad mode 'edge').
        static void Block(UxImage q, int by, int bx, double[] b)
        {
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++)
                {
                    int y = Math.Min(by * 4 + j, q.h - 1), x = Math.Min(bx * 4 + i, q.w - 1), p = (y * q.w + x) * 4, o = (j * 4 + i) * 4;
                    b[o] = q.data[p]; b[o + 1] = q.data[p + 1]; b[o + 2] = q.data[p + 2]; b[o + 3] = q.data[p + 3];
                }
        }
        /// The colour part shared by bc1_encode / bc3_encode: bounding-box endpoints of the texels with use[i], the palette and the
        /// nearest palette entry of every texel. Returns the number of palette entries (3 = the punch-through mode).
        static int RangeFit(double[] b, bool[] use, bool anyUse, bool punch, out int c0, out int c1, int[] idx)
        {
            double hr = double.NegativeInfinity, hg = hr, hb = hr, lr = double.PositiveInfinity, lg = lr, lb = lr;
            for (int i = 0; i < 16; i++)
            {
                if (anyUse && !use[i]) continue;
                double r = b[4 * i], g = b[4 * i + 1], bl = b[4 * i + 2];
                if (r > hr) hr = r; if (g > hg) hg = g; if (bl > hb) hb = bl; if (r < lr) lr = r; if (g < lg) lg = g; if (bl < lb) lb = bl;
            }
            int eHi = To565(hr, hg, hb), eLo = To565(lr, lg, lb), npal;
            var pal = new double[12];
            if (punch)
            {
                c0 = Math.Min(eHi, eLo); c1 = Math.Max(eHi, eLo);
                From565(c0, pal, 0); From565(c1, pal, 3);
                for (int c = 0; c < 3; c++) pal[6 + c] = (pal[c] + pal[3 + c]) / 2;
                npal = 3;
            }
            else if (eHi == eLo) { c0 = c1 = eHi; From565(c0, pal, 0); npal = 1; }      // equal endpoints: index 0 everywhere
            else
            {
                c0 = Math.Max(eHi, eLo); c1 = Math.Min(eHi, eLo);
                From565(c0, pal, 0); From565(c1, pal, 3);
                for (int c = 0; c < 3; c++) { pal[6 + c] = (2 * pal[c] + pal[3 + c]) / 3; pal[9 + c] = (pal[c] + 2 * pal[3 + c]) / 3; }
                npal = 4;
            }
            for (int i = 0; i < 16; i++)
            {
                int best = 0; double bd = 0;
                for (int j = 0; j < npal; j++)
                {
                    double d0 = b[4 * i] - pal[3 * j], d1 = b[4 * i + 1] - pal[3 * j + 1], d2 = b[4 * i + 2] - pal[3 * j + 2];
                    double d = (d0 * d0 + d1 * d1) + d2 * d2;
                    if (j == 0 || d < bd) { best = j; bd = d; }
                }
                idx[i] = best;
            }
            return npal;
        }
        /// bc1_encode(img): uint8 RGBA -> DXT1 blocks by range fit; a block with any alpha < 128 uses the 3-colour mode (index 3 =
        /// transparent black), every other block the 4-colour mode.
        static byte[] Bc1Encode(UxImage q)
        {
            int hb = Math.Max(4, (q.h + 3) / 4 * 4) / 4, wb = Math.Max(4, (q.w + 3) / 4 * 4) / 4;
            var o = new byte[hb * wb * 8]; var b = new double[64]; var idx = new int[16]; var opaque = new bool[16];
            for (int by = 0; by < hb; by++)
                for (int bx = 0; bx < wb; bx++)
                {
                    Block(q, by, bx, b);
                    bool anyTrans = false, anyOpaque = false;
                    for (int i = 0; i < 16; i++) { opaque[i] = !(b[4 * i + 3] < 128); if (opaque[i]) anyOpaque = true; else anyTrans = true; }
                    RangeFit(b, opaque, anyOpaque, anyTrans, out int c0, out int c1, idx);
                    uint bits = 0;
                    for (int i = 0; i < 16; i++) bits |= (uint)(opaque[i] ? idx[i] : 3) << (2 * i);
                    int at = (by * wb + bx) * 8;
                    UxBin.PutU16(o, at, (ushort)c0); UxBin.PutU16(o, at + 2, (ushort)c1); UxBin.PutU32(o, at + 4, bits);
                }
            return o;
        }
        /// bc3_encode(img): uint8 RGBA -> DXT5 blocks by range fit: the alpha block in the 8-step mode (a0 = max > a1 = min; one
        /// value: both endpoints, index 0), the colour block in the 4-colour mode on the texels that are not fully transparent.
        static byte[] Bc3Encode(UxImage q)
        {
            int hb = Math.Max(4, (q.h + 3) / 4 * 4) / 4, wb = Math.Max(4, (q.w + 3) / 4 * 4) / 4;
            var o = new byte[hb * wb * 16]; var b = new double[64]; var idx = new int[16]; var vis = new bool[16]; var apal = new double[8];
            for (int by = 0; by < hb; by++)
                for (int bx = 0; bx < wb; bx++)
                {
                    Block(q, by, bx, b);
                    int at = (by * wb + bx) * 16;
                    double amax = b[3], amin = b[3]; bool anyVis = false;
                    for (int i = 0; i < 16; i++) { double a = b[4 * i + 3]; if (a > amax) amax = a; if (a < amin) amin = a; vis[i] = a > 0; if (vis[i]) anyVis = true; }
                    int a0 = (int)UxNp.Round(amax), a1 = (int)UxNp.Round(amin);
                    o[at] = (byte)a0; o[at + 1] = (byte)a1;
                    if (a0 != a1)
                    {
                        apal[0] = a0; apal[1] = a1;
                        for (int i = 1; i < 7; i++) apal[1 + i] = ((7 - i) * a0 + i * a1) / 7.0;
                        ulong abits = 0;
                        for (int i = 0; i < 16; i++)
                        {
                            int best = 0; double bd = 0;
                            for (int j = 0; j < 8; j++) { double d = Math.Abs(b[4 * i + 3] - apal[j]); if (j == 0 || d < bd) { best = j; bd = d; } }
                            abits |= (ulong)best << (3 * i);
                        }
                        for (int k = 0; k < 6; k++) o[at + 2 + k] = (byte)(abits >> (8 * k));
                    }
                    RangeFit(b, vis, anyVis, false, out int c0, out int c1, idx);
                    uint bits = 0;
                    for (int i = 0; i < 16; i++) bits |= (uint)idx[i] << (2 * i);
                    UxBin.PutU16(o, at + 8, (ushort)c0); UxBin.PutU16(o, at + 10, (ushort)c1); UxBin.PutU32(o, at + 12, bits);
                }
            return o;
        }

        /// pc_decode(fmt, w, h, raw) for DXT1 / DXT5: uint8 RGBA (h, w, 4).
        static UxImage PcDecodeDxt(bool dxt5, int w, int h, byte[] raw)
        {
            int hb = Math.Max(1, (h + 3) / 4), wb = Math.Max(1, (w + 3) / 4);
            UxBin.Require(raw.Length % (hb * wb) == 0, "pc_decode: " + raw.Length + " bytes for " + hb + " x " + wb + " blocks");
            return UxTex.DecodeBlocks(new UxBlocks(raw, hb, wb, raw.Length / (hb * wb)), dxt5 ? "DXT4_5" : "DXT1").Crop(h, w);
        }
        /// float(np.abs(got - ref)[..., :3].mean()) and float(np.abs(got - ref)[..., 3].mean()) with numpy's summation orders: the
        /// three-channel view is not contiguous, so numpy sums it through its 8192-element buffer in chunks of 8190 (whole pixels),
        /// each chunk pairwise; the alpha view is one strided pairwise sum.
        static void Errors(UxImage got, GLevel rf, out double er, out double ea)
        {
            int n = rf.h * rf.w;
            var rgb = new double[3 * n]; var al = new double[n];
            for (int i = 0; i < n; i++)
            {
                rgb[3 * i] = Math.Abs(got.data[4 * i] - rf.d[4 * i]); rgb[3 * i + 1] = Math.Abs(got.data[4 * i + 1] - rf.d[4 * i + 1]); rgb[3 * i + 2] = Math.Abs(got.data[4 * i + 2] - rf.d[4 * i + 2]);
                al[i] = Math.Abs(got.data[4 * i + 3] - rf.d[4 * i + 3]);
            }
            double acc = 0;
            for (int i = 0; i < rgb.Length; i += 8190) acc += UxNp.Sum(rgb, i, Math.Min(8190, rgb.Length - i));
            er = acc / rgb.Length;
            ea = UxNp.Sum(al) / n;
        }

        const double GEN_BAKE_TOL = 12.0, GEN_RGB_TOL = 40.0, GEN_ALPHA_TOL = 64.0;     // mean |error| (0..255) tolerances of an encoded level
        const int GEN_BAKE_MIN = 128;                                                    // levels below this size use the V3.1 mip tolerances

        /// generated_texture(src_bytes, recipe): (Reflex PC v4 bytes, info) of a generated texture: composite -> DXT1 (opaque),
        /// channel_alpha -> DXT5, literal / const_alpha -> A8R8G8B8 4 x 4 (lossless). DXT levels are encoded by the Pillow encoder
        /// (UxPil.BcEncode), or by the range fit when that does better on a level that misses its tolerance or is at most 128 px.
        public static byte[] GeneratedTexture(byte[] srcBytes, JObj recipe, out JObj info)
        {
            var refs = GenReference(srcBytes, recipe, out int nsrc);
            string kind = recipe.Get("kind") as string;
            bool dxt1 = kind == "composite" || kind == "tint", dxt5 = kind == "channel_alpha" || kind == "tiled_alpha";
            byte[] fmt = dxt1 ? UxTex.PC_DXT1 : dxt5 ? UxTex.PC_DXT5 : UxTex.PC_A8R8G8B8;
            string fn = dxt1 ? "DXT1" : dxt5 ? "DXT5" : "A8R8G8B8";
            int h = refs[0].h, w = refs[0].w;
            var payload = new List<byte[]>(); var errs = new List<object>(); var encs = new List<object>(); var tols = new List<object>();
            double over = double.NegativeInfinity;
            foreach (var rf in refs)
            {
                int lh = rf.h, lw = rf.w;
                double t0 = Math.Max(lw, lh) >= GEN_BAKE_MIN ? GEN_BAKE_TOL : GEN_RGB_TOL, t1 = Math.Max(lw, lh) >= GEN_BAKE_MIN ? GEN_BAKE_TOL : GEN_ALPHA_TOL;
                var q = new UxImage(new byte[lh * lw * 4], lh, lw, 4);
                for (int i = 0; i < q.data.Length; i++) q.data[i] = (byte)UxNp.Clip(UxNp.Round(rf.d[i]), 0, 255);
                byte[] raw; string enc; UxImage got;
                if (fn == "A8R8G8B8")
                {
                    raw = new byte[q.data.Length];
                    for (int i = 0; i < raw.Length; i += 4) { raw[i] = q.data[i + 2]; raw[i + 1] = q.data[i + 1]; raw[i + 2] = q.data[i]; raw[i + 3] = q.data[i + 3]; }
                    enc = "bytes"; got = q;
                }
                else
                {
                    UxImage src = q;
                    if (fn == "DXT1")
                    {
                        src = new UxImage(new byte[lh * lw * 3], lh, lw, 3);
                        for (int i = 0; i < lh * lw; i++) { src.data[3 * i] = q.data[4 * i]; src.data[3 * i + 1] = q.data[4 * i + 1]; src.data[3 * i + 2] = q.data[4 * i + 2]; }
                    }
                    raw = UxPil.BcEncode(src, fn == "DXT5"); enc = "pillow";
                    got = PcDecodeDxt(fn == "DXT5", lw, lh, raw);
                }
                Errors(got, rf, out double er, out double ea);
                if (fn != "A8R8G8B8" && (er > t0 || ea > t1 || Math.Max(lw, lh) <= GEN_BAKE_MIN))
                {
                    byte[] raw2 = fn == "DXT1" ? Bc1Encode(q) : Bc3Encode(q);
                    Errors(PcDecodeDxt(fn == "DXT5", lw, lh, raw2), rf, out double er2, out double ea2);
                    if (UxShaderProg.PyMax(er2 / t0, ea2 / t1) < UxShaderProg.PyMax(er / t0, ea / t1)) { raw = raw2; er = er2; ea = ea2; enc = "range-fit"; }
                }
                double e0 = UxNp.PyRound(er, 3), e1 = UxNp.PyRound(ea, 3);
                payload.Add(raw); errs.Add(new List<object> { e0, e1 }); encs.Add(enc); tols.Add(new List<object> { t0, t1 });
                double ov = UxShaderProg.PyMax(e0 / t0, e1 / t1);
                if (ov > over) over = ov;
            }
            var table = new List<(int size, int pitch)>();
            for (int i = 0; i < payload.Count; i++) table.Add((payload[i].Length, UxTex.PcPitch(fmt, Math.Max(1, w >> i))));
            byte[] data = UxTex.BuildPc(new[] { (payload.Count, w, h, fmt) }, table, payload);
            info = new JObj().Set("format", fn).Set("width", (long)w).Set("height", (long)h).Set("mips", (long)payload.Count).Set("from_source", (long)nsrc)
                .Set("err", errs).Set("tol", tols).Set("encoder", encs).Set("max_over", UxNp.PyRound(over, 4));
            return data;
        }

        // ============================================================================== level context
        sealed class TexL { public string name, canon, role; public JObj rec; public List<UxStatePair> pairs; }
        /// A texture slot while planning: a plain name (an Untamed canonical name or a template default), ('gen', name) or ('lib', name).
        sealed class Slot
        {
            public string name; public char kind;       // 's' plain, 'g' generated, 'l' stock library
            public static Slot Plain(string n) => n == null ? null : new Slot { name = n, kind = 's' };
            public static Slot Gen(string n) => new Slot { name = n, kind = 'g' };
            public static Slot LibTex(string n) => new Slot { name = n, kind = 'l' };
            public bool IsStr => kind == 's';
        }

        sealed class Ctx
        {
            public UxContext ux; public UxLevelRow lv; public UxDiscLevel disc; public UxStock stock; public UxWork work; public CancellationToken cancel; public int jobs;
            public JObj L, closure; public string levelJsonMd5;
            public Dictionary<string, KeyValuePair<string, JObj>> tex = new Dictionary<string, KeyValuePair<string, JObj>>(StringComparer.Ordinal), mat = new Dictionary<string, KeyValuePair<string, JObj>>(StringComparer.Ordinal);
            public HashSet<string> terrain_only = new HashSet<string>(StringComparer.Ordinal);
            public Dictionary<string, object> alpha_cache = new Dictionary<string, object>(StringComparer.Ordinal);
            public JObj gen_recipes = new JObj();          // generated texture name -> recipe
            public Dictionary<string, UxShaderInfo> shaders = new Dictionary<string, UxShaderInfo>(StringComparer.Ordinal);
            public Dictionary<string, bool> grey = new Dictionary<string, bool>(StringComparer.Ordinal);
            public Dictionary<string, JObj> uv;            // material -> evidence
            public Dictionary<string, Tpl> lib;
            HashSet<string> _ramp_mats;
            readonly object _ramp_lock = new object();
            /// ramp_materials(): Untamed material names of the freestyle ramp carriers (UxFreestyle.RampMaterials; empty off fmx).
            public HashSet<string> RampMaterials()
            {
                lock (_ramp_lock)
                {
                    if (_ramp_mats == null)
                        _ramp_mats = UxCommon.IsFmx(lv) ? new HashSet<string>(UxFreestyle.RampMaterials(ux), StringComparer.Ordinal) : new HashSet<string>(StringComparer.Ordinal);
                    return _ramp_mats;
                }
            }

            public Ctx(UxContext c, int jobs)
            {
                ux = c; lv = c.lv; disc = c.disc; stock = c.stock; work = c.work; cancel = c.cancel; this.jobs = jobs;
                if (!work.Exists("level.json")) throw new UxException("level.json missing in the work store: run the level stage first");
                byte[] lj = work.ReadBytes("level.json");
                levelJsonMd5 = UxBin.Md5(lj);
                L = UxJson.Obj(UxJson.ParseUtf8(lj));
                closure = UxJson.Obj(L.Get("closure"));
                foreach (var kv in UxJson.Obj(closure.Get("texture")).items) tex[Lower(kv.Key)] = new KeyValuePair<string, JObj>(kv.Key, (JObj)kv.Value);
                foreach (var kv in UxJson.Obj(closure.Get("material")).items) mat[Lower(kv.Key)] = new KeyValuePair<string, JObj>(kv.Key, (JObj)kv.Value);
                object tid = (L.Get("terrain") as JObj)?.Get("entity_id");
                foreach (var kv in UxJson.Obj(closure.Get("material")).items)
                {
                    var r = (JObj)kv.Value;
                    if (!Truthy(r.Get("terrain"))) continue;
                    bool other = false;
                    if (r.Get("entities") is List<object> ents) foreach (object en in ents) if (!JsonEq(en, tid)) other = true;
                    if (!other) terrain_only.Add(kv.Key);
                }
            }
            static bool JsonEq(object a, object b) => a == null || b == null ? a == b : a is long x && b is long y ? x == y : a.Equals(b);

            /// material_file(name): (canonical, the path below extract/<Level>, closure record or null); (null, null, null) when missing.
            public (string canon, string file, JObj rec) MaterialFile(string name)
            {
                if (mat.TryGetValue(Lower(name), out var hit)) return (hit.Key, UxJson.Str(hit.Value.Get("file")), hit.Value);
                var a = disc.AssetOfFile("material/" + name + ".material");
                if (a == null) return (null, null, null);
                string stem = a.file.Substring(a.file.LastIndexOf('/') + 1); stem = stem.Substring(0, stem.Length - ".material".Length);
                return (stem, a.file, null);
            }
            /// texture(name): (canonical, record) from the closure, or (null, null).
            public (string canon, JObj rec) Texture(string name)
            {
                if (name != null && tex.TryGetValue(Lower(name), out var hit)) return (hit.Key, hit.Value);
                return (null, null);
            }
            public byte[] TextureBytes(string canon)
            {
                var r = Texture(canon).rec;
                return r != null ? disc.ReadExtractFile(UxJson.Str(r.Get("file")), cancel) : null;
            }
            /// shader(ps, entry): ux_shaderprog.shader_info of the Untamed pixel shader asset (case-insensitive), or null (also for an
            /// unreadable program: no program rules).
            public UxShaderInfo Shader(string ps, string entry)
            {
                if (string.IsNullOrEmpty(ps)) return null;
                string key = Lower(ps) + "\n" + (entry ?? "\u0001");
                if (shaders.TryGetValue(key, out var hit)) return hit;
                UxShaderInfo info = null;
                var a = disc.AssetOfFile("shader/" + ps + ".shader");
                if (a != null)
                {
                    try { info = UxShaderProg.ShaderInfo(disc.Read(a, cancel), entry); }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception) { info = null; }
                }
                return shaders[key] = info;
            }
            /// grey_dxn(lv, rec): a greyscale image stored as DXN (Capital City 'ogio'), which the textures stage converts to an
            /// A8R8G8B8 grey diffuse.
            public bool GreyDxn(JObj rec)
            {
                string file = rec.Get("file") as string;
                string key = file ?? "";
                if (grey.TryGetValue(key, out bool hit)) return hit;
                bool g = !string.IsNullOrEmpty(file) && disc.HasExtractFile(file) && GreyDxnFacts(disc.ReadExtractFile(file, cancel));
                return grey[key] = g;
            }

            /// One UV set of a group: counts the rows inside 0..1 (and finite) and gives u[finite].min() / .max(); false when no row is finite.
            static bool Measure(UxVal u, int cc, float lo, float hi, ref long inside01, out float mn, out float mx)
            {
                var fin = new float[u.n * cc]; int k = 0;
                for (int i = 0; i < u.n; i++)
                {
                    bool f = true, inside = true;
                    for (int c = 0; c < cc; c++) { float v = u.f[i * u.comps + c]; if (float.IsNaN(v) || float.IsInfinity(v)) f = false; if (!(v >= lo && v <= hi)) inside = false; }
                    if (inside && f) inside01++;
                    if (f) for (int c = 0; c < cc; c++) fin[k++] = u.f[i * u.comps + c];
                }
                mn = mx = 0;
                if (k == 0) return false;
                mn = NpReduce(fin, k, true); mx = NpReduce(fin, k, false);
                return true;
            }
            /// np.min / np.max of a contiguous float32 array as numpy 2.5 computes it on the reference machine (its 256-bit kernel:
            /// eight lanes start from a[0], each takes the better or equally good later element, the lanes are folded in halves with
            /// the higher lane winning a tie, the tail is folded in the same way). The order only decides the sign of a zero result
            /// when the array holds both +0.0 and -0.0, which the uv ranges of materials.json do show (as 0.0 or -0.0).
            static float NpReduce(float[] a, int n, bool min)
            {
                if (n == 1) return a[0];
                var acc = new float[8]; for (int l = 0; l < 8; l++) acc[l] = a[0];
                int ip = 1;
                for (; n - ip >= 8; ip += 8)
                    for (int l = 0; l < 8; l++) { float x = a[ip + l]; if (!(min ? acc[l] < x : acc[l] > x)) acc[l] = x; }
                float best = acc[0];
                for (int l = 1; l < 8; l++) if (min ? acc[l] < best : acc[l] > best) best = acc[l];
                float r = best;
                foreach (int l in s_lanePriority) if (acc[l] == best) { r = acc[l]; break; }
                for (; ip < n; ip++) { float x = a[ip]; if (!(min ? r < x : r > x)) r = x; }
                return r;
            }
            static readonly int[] s_lanePriority = { 7, 3, 5, 1, 6, 2, 4, 0 };

            sealed class Acc { public long groups, verts, in0, in1, eq, has1; public double r0lo = double.PositiveInfinity, r0hi = double.NegativeInfinity, r1lo = double.PositiveInfinity, r1hi = double.NegativeInfinity; public int valo = 255, vahi = 0; }

            /// uv_evidence(): {material: {groups, verts, uv0_in01, uv1_in01, uv1_eq_uv0, uv0_range, uv1_range, vertex_alpha}} measured on
            /// the surfaces drawn by kept / demoted / synth entities (group i of an entity's surface uses MaterialList[i]).
            public Dictionary<string, JObj> UvEvidence()
            {
                if (uv != null) return uv;
                var sidx = new Dictionary<string, JObj>(StringComparer.Ordinal);
                if (closure.Get("surface") is JObj sc) foreach (var kv in sc.items) sidx[Lower(kv.Key)] = (JObj)kv.Value;
                // the (surface, material list) uses in entity order, and the distinct surfaces
                var uses = new List<KeyValuePair<string, List<object>>>(); var names = new List<string>(); var seen = new HashSet<string>(StringComparer.Ordinal);
                foreach (object eo in UxJson.Arr(L.Get("entities")))
                {
                    var e = (JObj)eo; string cls = e.Get("cls") as string;
                    if (cls != "keep" && cls != "demote" && cls != "synth") continue;
                    var props = e.Get("props") as JObj;
                    object sa = props?.Get("SurfaceAsset"), ml = props?.Get("MaterialList");
                    if (!Truthy(sa) || !Truthy(ml)) continue;
                    string sn = UxJson.Str(UxJson.Obj(sa).Get("asset"));
                    uses.Add(new KeyValuePair<string, List<object>>(sn, UxJson.Arr(ml)));
                    if (seen.Add(sn)) names.Add(sn);
                }
                var surf = new List<UxGroup>[names.Count];
                Parallel.For(0, names.Count, new ParallelOptions { MaxDegreeOfParallelism = jobs, CancellationToken = cancel }, i =>
                {
                    try
                    {
                        if (sidx.TryGetValue(Lower(names[i]), out var r)) surf[i] = UxMesh.ReadSurface(disc.ReadExtractFile(UxJson.Str(r.Get("file")), cancel));
                    }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception) { surf[i] = null; }
                });
                var byName = new Dictionary<string, List<UxGroup>>(StringComparer.Ordinal);
                for (int i = 0; i < names.Count; i++) byName[names[i]] = surf[i];
                var acc = new List<KeyValuePair<string, Acc>>(); var accIdx = new Dictionary<string, Acc>(StringComparer.Ordinal);
                float lo = (float)-0.01, hi = (float)1.01, eps = (float)1e-4;       // numpy compares a float32 array with the float32 of a Python float
                foreach (var use in uses)
                {
                    var groups = byName[use.Key];
                    if (groups == null) continue;
                    for (int gi = 0; gi < groups.Count; gi++)
                    {
                        var g = groups[gi];
                        if (gi >= use.Value.Count || g.uv0 == null || g.nverts == 0) continue;
                        string m = UxJson.Str(UxJson.Obj(use.Value[gi]).Get("asset"));
                        if (!accIdx.TryGetValue(m, out var a)) { a = new Acc(); accIdx[m] = a; acc.Add(new KeyValuePair<string, Acc>(m, a)); }
                        if (g.col != null && g.col.n > 0)
                        {
                            int cmin = 255, cmax = 0;
                            for (int i = 0; i < g.col.n; i++) { int v = g.col.u8[i * g.col.comps + 3]; if (v < cmin) cmin = v; if (v > cmax) cmax = v; }
                            a.valo = Math.Min(a.valo, cmin); a.vahi = Math.Max(a.vahi, cmax);
                        }
                        var u0 = g.uv0; int n = u0.n, c0 = Math.Min(2, u0.comps);
                        a.groups++; a.verts += n;
                        if (Measure(u0, c0, lo, hi, ref a.in0, out float min0, out float max0)) { a.r0lo = UxShaderProg.PyMin(a.r0lo, min0); a.r0hi = UxShaderProg.PyMax(a.r0hi, max0); }
                        if (g.uv1 != null)
                        {
                            var u1 = g.uv1; int c1 = Math.Min(2, u1.comps); int n1 = u1.n;
                            a.has1 += n1;
                            bool any1 = Measure(u1, c1, lo, hi, ref a.in1, out float min1, out float max1);
                            UxBin.Require(n1 == n && c1 == c0, "uv evidence: UV0 and UV1 of a group differ in shape");
                            for (int i = 0; i < n; i++)
                            {
                                bool same = true;
                                for (int c = 0; c < c0; c++) if (!(Math.Abs((float)(u0.f[i * u0.comps + c] - u1.f[i * u1.comps + c])) < eps)) same = false;
                                if (same) a.eq++;
                            }
                            if (any1) { a.r1lo = UxShaderProg.PyMin(a.r1lo, min1); a.r1hi = UxShaderProg.PyMax(a.r1hi, max1); }
                        }
                    }
                }
                var o = new Dictionary<string, JObj>(StringComparer.Ordinal);
                foreach (var kv in acc)
                {
                    var a = kv.Value; long v = Math.Max(1, a.verts);
                    object Rr(double x, double y) => UxNp.Finite(x, y) ? new List<object> { UxNp.PyRound(x, 4), UxNp.PyRound(y, 4) } : null;
                    o[kv.Key] = new JObj().Set("groups", a.groups).Set("verts", a.verts).Set("uv0_in01", UxNp.PyRound((double)a.in0 / v, 4))
                        .Set("uv1_in01", a.has1 != 0 ? (object)UxNp.PyRound((double)a.in1 / v, 4) : null).Set("uv1_eq_uv0", a.has1 != 0 ? (object)UxNp.PyRound((double)a.eq / v, 4) : null)
                        .Set("uv0_range", Rr(a.r0lo, a.r0hi)).Set("uv1_range", Rr(a.r1lo, a.r1hi))
                        .Set("vertex_alpha", a.valo <= a.vahi ? new List<object> { (long)a.valo, (long)a.vahi } : null);
                }
                return uv = o;
            }
        }

        // ============================================================================== requests (the terrain stage's)
        sealed class Requests
        {
            public List<KeyValuePair<string, JObj>> items = new List<KeyValuePair<string, JObj>>(); public JObj src; public JObj gen = new JObj();
            /// generated texture name -> its work-store path (for the md5 check).
            public Dictionary<string, string> genFile = new Dictionary<string, string>(StringComparer.Ordinal);
            public JObj Get(string name) { foreach (var kv in items) if (kv.Key == name) return kv.Value; return null; }
        }
        static JObj CopyObj(JObj j) { var o = new JObj(); foreach (var kv in j.items) o.items.Add(kv); return o; }
        static string NormPath(string p)
        {
            var parts = new List<string>();
            foreach (string s in p.Replace('\\', '/').Split('/')) { if (s.Length == 0 || s == ".") continue; if (s == ".." && parts.Count > 0) parts.RemoveAt(parts.Count - 1); else parts.Add(s); }
            return string.Join("/", parts);
        }

        /// load_requests(spec, lv): (name -> spec, the source description or null, the generated textures the file offers).
        /// Schema 'ux_terrain_material_requests/1': {requests: {name: {template, textures, source_material, uv0, uv1, states, ...}},
        /// generated_textures: {name: {file (relative to the requests file's folder), md5, size, format, width, height, mips, source}}}.
        static Requests LoadRequests(Ctx ctx, string spec)
        {
            var o = new Requests();
            string rel = spec ?? "terrain/material_requests.json";
            if (spec == null && !ctx.work.Exists(rel)) return o;
            if (spec != null && spec.ToLowerInvariant() == "none") return o;
            rel = NormPath(rel);
            byte[] bytes = ctx.work.ReadBytes(rel);
            string root = "work/" + ctx.lv.disc + "/";
            o.src = new JObj().Set("file", root + rel).Set("md5", UxBin.Md5(bytes));
            string baseDir = rel.LastIndexOf('/') >= 0 ? rel.Substring(0, rel.LastIndexOf('/')) : "";
            object doc = UxJson.ParseUtf8(bytes), list = doc;
            if (doc is JObj d)
            {
                if (d.Get("generated_textures") is JObj gt)
                    foreach (var kv in gt.items)
                    {
                        var meta = (JObj)kv.Value; var m = CopyObj(meta);
                        string f = meta.Get("file") as string, path = null;
                        if (!string.IsNullOrEmpty(f)) { string wr = NormPath((baseDir.Length > 0 ? baseDir + "/" : "") + f); path = root + wr; o.genFile[kv.Key] = wr; }
                        else if (meta.Get("path") is string mp && mp.Length > 0)
                        {
                            path = NormPath(mp);
                            if (path.StartsWith(root, StringComparison.Ordinal)) o.genFile[kv.Key] = path.Substring(root.Length);
                        }
                        m.Set("path", path);
                        o.gen.Set(kv.Key, m);
                    }
                o.src.Set("schema", d.Get("schema"));
                foreach (string k in new[] { "materials", "requests" }) if (d.Has(k)) { list = d.Get(k); break; }
            }
            var items = new List<JObj>();
            if (list is JObj dict)
            {
                foreach (var kv in dict.items)
                {
                    if (kv.Value is JObj v) { var it = CopyObj(v); it.Set("name", v.Has("name") ? v.Get("name") : kv.Key); items.Add(it); }
                    else if (kv.Value is string s) items.Add(new JObj().Set("name", kv.Key).Set("source", s));
                }
            }
            else if (list is List<object> arr)
                foreach (object v in arr) items.Add(v is string s ? new JObj().Set("name", s) : CopyObj(UxJson.Obj(v)));
            foreach (var it in items)
            {
                if (!Truthy(it.Get("name"))) continue;
                if (!Truthy(it.Get("source")) && Truthy(it.Get("source_material"))) it.Set("source", it.Get("source_material"));
                string nm = UxJson.Str(it.Get("name"));
                int at = o.items.FindIndex(kv => kv.Key == nm);
                if (at < 0) o.items.Add(new KeyValuePair<string, JObj>(nm, it)); else o.items[at] = new KeyValuePair<string, JObj>(nm, it);
            }
            o.src.Set("count", (long)o.items.Count);
            return o;
        }

        // ============================================================================== planning
        /// One materials.json entry while it is made.
        sealed class Entry
        {
            public string action, reason, asset, template, file, md5; public long? size; public bool swap_uv, request; public double? diffuse_alpha_cut;
            public List<string> textures = new List<string>(), source_textures = new List<string>(), slots = new List<string>(), library_textures = new List<string>(), generated_textures = new List<string>(), warnings = new List<string>();
            public List<List<string>> uv_sets = new List<List<string>>();
            public List<object> states = new List<object>(), samplers = new List<object>();
            public JObj state_patch = new JObj(), changed = new JObj(), substituted = new JObj(), uv_evidence, source; public List<JObj> sampler_patch = new List<JObj>();
            /// diffuse_pick, alpha_evidence, alpha_program, composite, literal_albedo (present only when the rule ran).
            public JObj extra = new JObj();

            public Entry Drop(string why) { action = "drop"; reason = why; asset = null; template = null; textures = new List<string>(); source_textures = new List<string>(); return this; }
            public JObj ToJson()
            {
                var uvs = new List<object>(); foreach (var u in uv_sets) uvs.Add(Lst(u));
                var sp = new List<object>(); foreach (var p in sampler_patch) sp.Add(p);
                var j = new JObj().Set("action", action).Set("reason", reason).Set("asset", asset).Set("template", template).Set("textures", Lst(textures))
                    .Set("source_textures", Lst(source_textures)).Set("slots", Lst(slots)).Set("uv_sets", uvs).Set("swap_uv", swap_uv).Set("states", states)
                    .Set("state_patch", state_patch).Set("samplers", samplers).Set("sampler_patch", sp).Set("changed", changed).Set("substituted", substituted)
                    .Set("library_textures", Lst(library_textures)).Set("generated_textures", Lst(generated_textures))
                    .Set("diffuse_alpha_cut", diffuse_alpha_cut == null ? null : (object)diffuse_alpha_cut.Value).Set("file", file).Set("md5", md5)
                    .Set("size", size == null ? null : (object)size.Value).Set("request", request).Set("warnings", Lst(warnings)).Set("uv_evidence", uv_evidence).Set("source", source);
                foreach (var kv in extra.items) j.Set(kv.Key, kv.Value);
                return j;
            }
        }

        /// _h4(obj): the first four hex digits (upper case) of the md5 of the object's deterministic JSON.
        static string H4(JObj obj) => UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.DumpsUx(obj))).Substring(0, 4).ToUpperInvariant();
        static string Hex2(double v01) => UxPyFmt.RoundInt(UxShaderProg.PyMin(1.0, UxShaderProg.PyMax(0.0, v01)) * 255).ToString("X2", CultureInfo.InvariantCulture);
        /// gen_name(recipe): UXC_<source>_<h> composite, UXM_<source>_<h> channel alpha, UXL_<RRGGBB> literal albedo, UXA_<AA> constant alpha.
        static string GenName(JObj recipe)
        {
            string k = recipe.Get("kind") as string;
            if (k == "composite") return "UXC_" + recipe.Get("source") + "_" + H4(recipe);
            if (k == "channel_alpha" || k == "tiled_alpha") return "UXM_" + recipe.Get("source") + "_" + H4(recipe);
            if (k == "tint") return "UXP_" + recipe.Get("source") + "_" + H4(recipe);
            if (k == "literal") { var rgb = Floats(recipe.Get("rgb")); return "UXL_" + Hex2(rgb[0]) + Hex2(rgb[1]) + Hex2(rgb[2]); }
            return "UXA_" + Hex2(UxJson.Num(recipe.Get("alpha")));
        }
        static List<object> Rounded(IEnumerable<double> v, int digits) { var o = new List<object>(); foreach (double x in v) o.Add(UxNp.PyRound(x, digits)); return o; }
        static IEnumerable<double> Doubles(List<object> l) { foreach (object x in l) yield return UxJson.Num(x); }

        /// program_pick: when the picked diffuse is a macro map (fetched at |scale| < MACRO_SCALE on both axes), take a candidate fetched
        /// on the same interpolant at the mesh scale; among several, the one sharing the longest name prefix (>= 6) with the material's
        /// normal map, else pick_diffuse. Returns (name, why) or (null, null).
        static (string nd, string why) ProgramPick(UxShaderInfo info, List<KeyValuePair<string, JObj>> dcands, string cur, string normal, Dictionary<string, int> sidx)
        {
            var f = UxShaderProg.FetchUv(info, sidx[cur]);
            if (f.kind != "scaled" || f.terms == null || f.terms.Count == 0) return (null, null);
            foreach (var t in f.terms) if (Math.Abs(t.a) >= MACRO_SCALE) return (null, null);
            var regs = new HashSet<string>(StringComparer.Ordinal); foreach (var t in f.terms) regs.Add(t.v.Split('.')[0]);
            var alt = new List<KeyValuePair<string, JObj>>();
            foreach (var c in dcands)
            {
                if (c.Key == cur) continue;
                var f2 = UxShaderProg.FetchUv(info, sidx[c.Key]);
                if ((f2.kind != "identity" && f2.kind != "scaled") || f2.terms == null || f2.terms.Count == 0) continue;
                var r2 = new HashSet<string>(StringComparer.Ordinal); bool big = true;
                foreach (var t in f2.terms) { r2.Add(t.v.Split('.')[0]); if (!(Math.Abs(t.a) >= MACRO_SCALE)) big = false; }
                if (r2.SetEquals(regs) && big) alt.Add(c);
            }
            if (alt.Count == 0) return (null, null);
            string nm = Lower(normal ?? "");
            int Pre(string c) { string l = Lower(c); int n = 0; while (n < l.Length && n < nm.Length && l[n] == nm[n]) n++; return n; }
            int best = 0; for (int i = 1; i < alt.Count; i++) if (Pre(alt[i].Key) > Pre(alt[best].Key)) best = i;
            int same = 0; foreach (var c in alt) if (Pre(c.Key) == Pre(alt[best].Key)) same++;
            string nd, how;
            if (nm.Length > 0 && Pre(alt[best].Key) >= 6 && same == 1) { nd = alt[best].Key; how = "shares the normal map " + normal + " name"; }
            else { nd = PickDiffuse(alt); how = "candidate order"; }
            var terms = new List<string>(); foreach (var t in f.terms) terms.Add(t.v + "*" + UxPyFmt.G(t.a));
            return (nd, cur + " is a macro map (uv " + string.Join(", ", terms) + "); " + nd + " is fetched at the mesh scale on the same UV (" + how + ")");
        }

        sealed class AlphaAlt { public List<Slot> tex; public JObj recipes = new JObj(), output; public string reason, line; }

        /// program_alpha: an alpha-class material whose picked diffuse has no transparent texel takes its alpha from what the Untamed
        /// program writes to oC0.w instead of being dropped:
        ///   constant c          -> Template_Diffuse_Alpha [diffuse, UXA_<c>] (generated 4x4, alpha = c): glass;
        ///   texture k .w as is  -> [diffuse or k, k] (the cut-out texture itself): EC plant leaves;
        ///   sat(a * k.ch + b)   -> [diffuse, UXM_<k>] (generated, alpha = that channel expression): BridgePort truss, press-box windows.
        /// Returns null when the old drop stands.
        static AlphaAlt ProgramAlpha(Ctx ctx, UxShaderInfo info, List<TexL> texl, string d, long aref, long afunc)
        {
            var ao = UxShaderProg.AlphaOutput(info);
            if (ao == null || (ao.kind != "const" && ao.kind != "texture")) return null;
            var alt = new AlphaAlt();
            var sidx = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int k = texl.Count - 1; k >= 0; k--) if (!string.IsNullOrEmpty(texl[k].canon)) sidx[texl[k].canon] = k;
            long lo, hi; string why;
            if (ao.kind == "const")
            {
                long a8 = UxPyFmt.RoundInt(ao.value * 255);
                if (a8 >= OPAQUE_ALPHA_MIN || a8 <= 0) return null;
                var r = new JObj().Set("kind", "const_alpha").Set("alpha", UxNp.PyRound(ao.value, 6));
                string n = GenName(r); alt.recipes.Set(n, r);
                alt.tex = new List<Slot> { Slot.Plain(d), Slot.Gen(n) }; lo = hi = a8;
                why = "Untamed alpha is the constant " + UxPyFmt.F(ao.value, 3) + " (" + ao.raw + ") -> Template_Diffuse_Alpha [" + S(d) + ", " + n + " (generated, alpha " + I(a8) + ")]";
            }
            else
            {
                int k = ao.sampler;
                if (k >= texl.Count || string.IsNullOrEmpty(texl[k].canon) || texl[k].rec == null || IsRenderTarget(texl[k].rec)) return null;
                string kc = texl[k].canon;
                var fa = UxShaderProg.FetchUv(info, k);
                if (fa.kind != "identity")
                {
                    var ta = ProgramTiledAlpha(ctx, info, texl, d, ao, k, fa, sidx);
                    if (ta == null) return null;
                    string n = GenName(ta.Value.r); alt.recipes.Set(n, ta.Value.r);
                    alt.tex = new List<Slot> { Slot.Plain(ta.Value.d), Slot.Gen(n) };
                    if ((afunc == D3DCMP_GREATER || afunc == 7) && ta.Value.hi < aref + (afunc == D3DCMP_GREATER ? 1 : 0)) return null;
                    alt.reason = ta.Value.why.Replace("<GEN>", n); alt.line = ao.raw;
                    alt.output = new JObj().Set("kind", "texture").Set("sampler", (long)ao.sampler).Set("channel", "" + ao.channel).Set("scale", ao.scale).Set("offset", ao.offset).Set("sat", ao.sat);
                    return alt;
                }
                UxSpFetchUv fd = null;
                if (d != null && sidx.TryGetValue(d, out int dk)) fd = UxShaderProg.FetchUv(info, dk);
                string uvd = fd?.kind;
                bool same = uvd == "identity" && UxSpFetchUv.SameTerms(fd.terms, fa.terms);
                bool plain = ao.channel == 'w' && Math.Abs(ao.scale - 1) < 1e-6 && Math.Abs(ao.offset) < 1e-6;
                if (plain)
                {
                    var rng = AlphaRange(ctx, UxJson.Str(texl[k].rec.Get("file")));
                    if (rng == null || rng.Value.min >= OPAQUE_ALPHA_MIN) return null;
                    lo = rng.Value.min; hi = rng.Value.max;
                    string s0 = same ? d : kc;
                    alt.tex = new List<Slot> { Slot.Plain(s0), Slot.Plain(kc) };
                    why = "Untamed alpha = " + kc + ".a (" + ao.raw + ", sampler " + I(k) + " at the mesh UV) -> Template_Diffuse_Alpha [" + S(s0) + ", " + kc + "]" +
                        (same ? "" : " (the diffuse " + S(d) + " is fetched at another UV (" + S(uvd) + "): the alpha texture gives the colour too)");
                }
                else
                {
                    if (!same) return null;
                    var r = new JObj().Set("kind", "channel_alpha").Set("source", kc).Set("channel", "" + ao.channel).Set("scale", UxNp.PyRound(ao.scale, 6)).Set("offset", UxNp.PyRound(ao.offset, 6));
                    string n = GenName(r); alt.recipes.Set(n, r);
                    List<GLevel> rf;
                    try { rf = GenReference(ctx.TextureBytes(kc), r, out _); }
                    catch (GenValueError) { return null; }
                    GLevel lv = rf[rf.Count - 1];
                    foreach (var x in rf) if (Math.Max(x.h, x.w) <= 64) { lv = x; break; }
                    double amin = double.PositiveInfinity, amax = double.NegativeInfinity;
                    for (int i = 3; i < lv.d.Length; i += 4) { if (lv.d[i] < amin) amin = lv.d[i]; if (lv.d[i] > amax) amax = lv.d[i]; }
                    lo = (long)amin; hi = (long)amax;
                    if (lo >= OPAQUE_ALPHA_MIN) return null;
                    alt.tex = new List<Slot> { Slot.Plain(d), Slot.Gen(n) };
                    why = "Untamed alpha = sat(" + UxPyFmt.G(ao.scale) + " * " + kc + "." + "rgba"["xyzw".IndexOf(ao.channel)] + " " + UxPyFmt.G(ao.offset, true) + ") (" + ao.raw +
                        ") -> Template_Diffuse_Alpha [" + S(d) + ", " + n + " (generated)]";
                }
            }
            // something must pass the test (D3DCMP 5 GREATER, 7 GREATEREQUAL; others: assume it passes)
            if ((afunc == D3DCMP_GREATER || afunc == 7) && hi < aref + (afunc == D3DCMP_GREATER ? 1 : 0)) return null;
            alt.reason = why; alt.line = ao.raw;
            alt.output = ao.kind == "const" ? new JObj().Set("kind", "const").Set("value", ao.value)
                : new JObj().Set("kind", "texture").Set("sampler", (long)ao.sampler).Set("channel", "" + ao.channel).Set("scale", ao.scale).Set("offset", ao.offset).Set("sat", ao.sat);
            return alt;
        }

        const int TILED_ALPHA_SIZE = 1024;     // rampfix: size of a baked tiled cut-out (RampGrate x25: 41 px per hole tile)

        /// program_tiled_alpha (rampfix, FREESTYLE.md 11), freestyle arenas only: a plain alpha texture (oC0.w = sat(T.w), unscaled) fetched
        /// at the mesh UV times an integer on each axis (Untamed WRAP), over a different, opaque diffuse fetched at that same mesh UV
        /// (RampGrate). The repeat is baked into one TILED_ALPHA_SIZE texture in mesh-UV space (ux_textures 'tiled_alpha'):
        /// Template_Diffuse_Alpha [diffuse, UXM_<alpha texture>_<h>]. Returns (diffuse, recipe, alpha max, reason with <GEN>) or null.
        static (string d, JObj r, long hi, string why)? ProgramTiledAlpha(Ctx ctx, UxShaderInfo info, List<TexL> texl, string d, UxSpAlpha ao, int k, UxSpFetchUv fa,
            Dictionary<string, int> sidx)
        {
            if (!UxCommon.IsFmx(ctx.lv) || fa.kind != "scaled" || fa.terms == null || fa.terms.Count != 2) return null;
            if (ao.channel != 'w' || Math.Abs(ao.scale - 1) > 1e-6 || Math.Abs(ao.offset) > 1e-6) return null;
            string kc = texl[k].canon;
            if (d == null || d == kc || IsTemplateDefault(d) || !sidx.ContainsKey(d)) return null;
            var tiles = new List<object>();
            foreach (var t in fa.terms)
            {
                double ra = Math.Round(t.a, MidpointRounding.ToEven);
                if (Math.Abs(t.b) > 1e-6 || Math.Abs(t.a - ra) > 1e-6 || ra < 2) return null;
                tiles.Add((long)ra);
            }
            var fd = UxShaderProg.FetchUv(info, sidx[d]);
            if (fd.kind != "identity" || fd.terms == null || fd.terms.Count != fa.terms.Count) return null;
            for (int i = 0; i < fd.terms.Count; i++) if (fd.terms[i].v != fa.terms[i].v) return null;
            var dr = ctx.Texture(d).rec;
            if (dr == null || IsRenderTarget(dr)) return null;
            var rng = AlphaRange(ctx, UxJson.Str(texl[k].rec.Get("file")));
            if (rng == null || rng.Value.min >= OPAQUE_ALPHA_MIN) return null;
            var r = new JObj().Set("kind", "tiled_alpha").Set("source", kc).Set("channel", "w").Set("tiles", tiles).Set("size", (long)TILED_ALPHA_SIZE);
            var ts = new List<string>(); foreach (object x in tiles) ts.Add(I((long)x));
            string why = "Untamed alpha = " + kc + ".a fetched at the mesh UV x " + string.Join("/", ts) + " (" + ao.raw + ", WRAP) over the diffuse " + d + " at the mesh UV: the repeat " +
                "baked into <GEN> (generated " + I(TILED_ALPHA_SIZE) + " px, freestyle arenas, FREESTYLE.md 11) -> Template_Diffuse_Alpha [" + d + ", <GEN>]";
            return (d, r, rng.Value.max, why);
        }

        /// program_colour (Arena v3, FREESTYLE.md 12): ux_shaderprog.diffuse_albedo of a freestyle ramp material, or null off a freestyle
        /// level / off the ramp materials / when the program has no such form.
        static UxSpDiffuseAlbedo ProgramColour(Ctx ctx, UxShaderInfo info, List<TexL> texl, string canon)
        {
            if (!UxCommon.IsFmx(ctx.lv) || !ctx.RampMaterials().Contains(canon)) return null;
            var ao = new List<int>(); for (int k = 0; k < texl.Count; k++) if (texl[k].role == "ao") ao.Add(k);
            var r = UxShaderProg.DiffuseAlbedo(info, ao);
            return r.kind == "affine" || r.kind == "literal" ? r : null;
        }
        static List<object> DList(double[] v) { var o = new List<object>(); foreach (double x in v) o.Add(x); return o; }
        static List<object> SList(List<string> v) { var o = new List<object>(); foreach (string x in v) o.Add(x); return o; }

        /// program_albedo: the albedo a 'no usable diffuse' Untamed program computes. Returns (asset or generated name, recipe or null,
        /// note); `found` false keeps the default: fog-only programs -> Editor_Default_Black; a literal albedo that differs from the
        /// default by more than ALBEDO_KEEP -> UXL_<RRGGBB> (4x4 generated); a literal above 1 is scaled to max 1 (keeps the hue).
        static bool ProgramAlbedo(UxShaderInfo info, List<TexL> texl, string dflt, out string asset, out JObj recipe, out string note)
        {
            asset = null; recipe = null; note = null;
            var cols = new List<int>(); for (int k = 0; k < texl.Count; k++) if (texl[k].role == "diffuse") cols.Add(k);
            var r = UxShaderProg.AlbedoScale(info, cols);
            if (r.rgb == null) return false;
            var rgb = (double[])r.rgb.Clone();
            note = r.how;
            double mx = rgb[0]; if (rgb[1] > mx) mx = rgb[1]; if (rgb[2] > mx) mx = rgb[2];
            if (mx > 1.0)
            {
                note += "; literal " + UxPyFmt.ReprList(Doubles(Rounded(rgb, 3))) + " > 1 scaled by 1/" + UxPyFmt.F(mx, 3) + " (hue kept)";
                for (int c = 0; c < 3; c++) rgb[c] = rgb[c] / mx;
            }
            double? dv = dflt == DEFAULT_WHITE ? 1.0 : dflt == DEFAULT_GRAY ? 0.502 : dflt == DEFAULT_BLACK ? 0.0 : (double?)null;
            if (dv != null)
            {
                double far = Math.Abs(rgb[0] - dv.Value); for (int c = 1; c < 3; c++) { double x = Math.Abs(rgb[c] - dv.Value); if (x > far) far = x; }
                if (far <= ALBEDO_KEEP) return false;
            }
            mx = rgb[0]; if (rgb[1] > mx) mx = rgb[1]; if (rgb[2] > mx) mx = rgb[2];
            if (r.fog_only || mx <= 0.02) { asset = DEFAULT_BLACK; return true; }
            recipe = new JObj().Set("kind", "literal").Set("rgb", Rounded(rgb, 6));
            asset = GenName(recipe);
            return true;
        }

        static List<Slot> SlotFill(string T, string d, string lm, string nm, string ao)
        {
            Slot P(string s) => Slot.Plain(s);
            switch (T)
            {
                case "Template_Prelit": return new List<Slot> { P(lm), P(d) };
                case "Template_Diffuse": return new List<Slot> { P(d) };
                case "Template_Diffuse_Nrm": return new List<Slot> { P(d), P(nm) };
                case "Template_Diffuse_AO": return new List<Slot> { P(ao), P(d) };
                case "Template_Diffuse_Alpha": return new List<Slot> { P(d), P(d) };
                case "Template_Eco_Frond": return new List<Slot> { P(d) };
                case "crowd": return new List<Slot> { P(d), P(d) };
            }
            throw new UxException("KeyError " + T);
        }

        /// plan_material(name, ctx, opts, req, gen): decides template, textures and states for one material (no bytes yet).
        /// req: a material request (the terrain stage's); gen: the generated textures the request file offers {name: meta}.
        static Entry PlanMaterial(string name, Ctx ctx, UxMaterialsOptions opts, JObj req, JObj gen)
        {
            bool isReq = req != null && req.items.Count > 0;
            if (!isReq) req = null;
            string src = req != null && Truthy(req.Get("source")) ? UxJson.Str(req.Get("source")) : name;
            var (canon, path, rec) = ctx.MaterialFile(src);
            var e = new Entry { request = isReq };
            if (canon == null)
            {
                e.warnings.Add("source material " + Repr(src) + " not found in the closure or extract/");
                return e.Drop("source material missing");
            }
            byte[] data = ctx.disc.ReadExtractFile(path, ctx.cancel);
            var m = UxMat.ReadMaterial(data);
            if (m.passes.Count == 0) throw new UxException("material " + canon + " has no pass");
            var P = m.passes[0];
            string shader = P.vs ?? "";
            var states_pc = Translate(P.states, false, out var unk);
            if (unk.Count > 0) e.warnings.Add("Xenon render states outside the translation tables ignored: " + ReprPairs(unk));
            string cls = UxMat.BlendClass(P.states);
            var texl = new List<TexL>();                                 // Untamed textures: name, canonical, rec, role, pairs
            foreach (var tp in P.textures)
            {
                var (c, r) = ctx.Texture(tp.Key);
                string role = r != null ? TextureRole(tp.Key, r) : "missing";
                if (role == "normal" && r.Get("format") as string == "DXN" && ctx.GreyDxn(r))
                {
                    role = "diffuse";                                    // a greyscale logo stored as DXN (fix round)
                    e.warnings.Add("texture " + tp.Key + " is a greyscale logo stored as DXN (R == G): used as a diffuse (ux_textures writes it as A8R8G8B8 grey)");
                }
                texl.Add(new TexL { name = tp.Key, canon = c, rec = r, role = role, pairs = tp.Value });
                if (c == null) e.warnings.Add("texture " + tp.Key + " not in the closure: ignored");
            }
            var roles = new JObj(); var tnames = new List<string>();
            foreach (var x in texl) { tnames.Add(x.name); roles.Set(x.name ?? "None", x.role); }
            e.source = new JObj().Set("material", canon).Set("file", path.Replace('\\', '/')).Set("md5", UxBin.Md5(data)).Set("shader", shader).Set("blend", cls)
                .Set("passes", (long)m.passes.Count).Set("textures", Lst(tnames)).Set("roles", roles).Set("xenon_states", UxMat.NamedStates(P.states));
            if (m.passes.Count > 1) e.warnings.Add(m.passes.Count + " passes: only pass 1 is converted");
            if (ctx.terrain_only.Contains(canon) && req == null)
            {
                e.action = "terrain";
                e.reason = "terrain-only material (TerrainRSData MaterialList): the ground is the heightfield bake (WP6); converted only when requested in terrain/material_requests.json";
                return e;
            }
            string low = Lower(shader);
            if (low == "pyroparticles") return e.Drop("shared PyroParticles shader: dropped in v1 (DESIGN 5.5)");
            if (low == "water") return e.Drop("shared Water shader: dropped in v1 (DESIGN 5.5)");

            // --- role picks
            string First(string role) { foreach (var x in texl) if (!string.IsNullOrEmpty(x.canon) && x.role == role) return x.canon; return null; }
            var dcands = new List<KeyValuePair<string, JObj>>();
            foreach (var x in texl)
                if (!string.IsNullOrEmpty(x.canon) && x.role == "diffuse" && dcands.FindIndex(c => c.Key == x.canon) < 0) dcands.Add(new KeyValuePair<string, JObj>(x.canon, x.rec));
            string pDiffuse = PickDiffuse(dcands), pLightmap = First("lightmap"), pNormal = First("normal"), pAo = First("ao");
            var sinfo = opts.ProgramUsed ? ctx.Shader(P.ps, P.ps_entry) : null;
            var sidx = new Dictionary<string, int>(StringComparer.Ordinal);
            for (int k = texl.Count - 1; k >= 0; k--) if (!string.IsNullOrEmpty(texl[k].canon)) sidx[texl[k].canon] = k;
            var prog_notes = new List<string>();
            if (opts.diffuse_pick == "program" && sinfo != null && dcands.Count > 1 && !string.IsNullOrEmpty(pDiffuse) && !SX_FLOOR.IsMatch(canon))
            {
                var (nd, why) = ProgramPick(sinfo, dcands, pDiffuse, pNormal, sidx);
                if (nd != null)
                {
                    e.extra.Set("diffuse_pick", new JObj().Set("order", pDiffuse).Set("program", nd).Set("why", why));
                    prog_notes.Add("diffuse " + nd + " instead of " + pDiffuse + ": " + why + " (--diffuse-pick program)");
                    pDiffuse = nd;
                }
            }
            string bale_nm = null;
            foreach (var x in texl) if (!string.IsNullOrEmpty(x.canon) && x.role == "normal" && BALE_NM.IsMatch(Lower(x.canon))) { bale_nm = x.canon; break; }
            List<Slot> req_tex = null;
            if (req != null && Truthy(req.Get("textures")))
            {
                req_tex = new List<Slot>();
                foreach (object to in UxJson.Arr(req.Get("textures")))
                {
                    string t = UxJson.Str(to);
                    string c = ctx.Texture(t).canon;
                    if (c == null && gen != null && gen.Has(t)) req_tex.Add(Slot.Gen(t));
                    else
                    {
                        if (c == null) e.warnings.Add("requested texture " + t + " is neither in the closure nor a generated texture");
                        req_tex.Add(Slot.Plain(c));
                    }
                }
                if (req.Get("states") is JObj rs && rs.Has("CULLMODE") && rs.Get("CULLMODE") != null)
                {
                    object xc = UxMat.NamedStates(P.states).Get("CULLMODE");
                    if (xc != null && !(rs.Get("CULLMODE") is long a && xc is long b && a == b))
                        e.warnings.Add("request CULLMODE " + PyStr(rs.Get("CULLMODE")) + " differs from the source material (Xenon " + PyStr(xc) + "); the source is used");
                }
            }
            string dflt = DefaultTexture(req == null ? canon : name);
            string reqTemplate = req != null && Truthy(req.Get("template")) ? req.Get("template") as string : null;

            // --- template choice
            var reasons = new List<string>();
            string T; List<Slot> tex; string eff_cls;
            if (low == "crowd")
            {
                if (opts.crowd_off) return e.Drop("crowd material dropped by --crowd-off");
                T = "crowd";
                var ct = new List<string>(); foreach (var x in texl) if (!string.IsNullOrEmpty(x.canon)) ct.Add(x.canon);
                tex = new List<Slot>();
                if (ct.Count > 0) { var both = new List<string>(ct) { ct[0] }; for (int i = 0; i < 2 && i < both.Count; i++) tex.Add(Slot.Plain(both[i])); }
                else { tex.Add(null); tex.Add(null); }
                reasons.Add("shared crowd shader -> stock crowd [BaseMap, EmptyChair]");
                eff_cls = "opaque";
            }
            else
            {
                T = reqTemplate;
                if (T != null && !Has(TEMPLATES, T))
                {
                    e.warnings.Add("requested template " + Repr(T) + " unknown");
                    return e.Drop("requested template " + Repr(T) + " is not in the library");
                }
                string alpha_cls = T == null ? (cls != "opaque" ? cls : null) : (IsAlphaTemplate(T) ? cls : null);
                eff_cls = "opaque";
                if (alpha_cls != null && opts.alpha == "drop") return e.Drop(alpha_cls + " material dropped by --alpha drop");
                if (alpha_cls != null && (opts.alpha == "eco_frond" || opts.alpha == "diffuse_alpha"))
                {
                    T = opts.alpha == "eco_frond" ? "Template_Eco_Frond" : "Template_Diffuse_Alpha";
                    eff_cls = alpha_cls;
                    Slot ds;
                    if (req_tex != null && req_tex.Count > 0 && reqTemplate != null && DiffuseSlot(reqTemplate) >= 0)
                    {
                        if (DiffuseSlot(reqTemplate) >= req_tex.Count) throw new UxException("IndexError: the request of " + name + " has no diffuse slot texture");
                        ds = req_tex[DiffuseSlot(reqTemplate)];
                    }
                    else ds = Slot.Plain(pDiffuse);
                    string d = ds != null && ds.IsStr ? ds.name : null;      // d when it is a str
                    reasons.Add(alpha_cls + " (Untamed " + (alpha_cls == "alphatest" ? "alpha test" : "blend") + ") -> " + T + " (--alpha " + opts.alpha + ")");
                    // --- does anything pass the alpha test? (fix round 2026-11-09)
                    bool atest = Truthy(states_pc.Get("ALPHATESTENABLE"));
                    long aref = atest && states_pc.Get("ALPHAREF") is long ar0 ? ar0 : 0;
                    ctx.UvEvidence().TryGetValue(canon, out var ev0);
                    var va = ev0?.Get("vertex_alpha") as List<object>;
                    (int min, int max)? ar = null;
                    if (d != null && !IsTemplateDefault(d))
                    {
                        var dr0 = ctx.Texture(d).rec;
                        if (dr0 != null && !IsRenderTarget(dr0)) ar = AlphaRange(ctx, UxJson.Str(dr0.Get("file")));
                    }
                    e.extra.Set("alpha_evidence", new JObj().Set("texture", d).Set("texture_alpha", ar != null ? new List<object> { (long)ar.Value.min, (long)ar.Value.max } : null)
                        .Set("vertex_alpha", va).Set("alpharef", aref));
                    if (LEADER_DIGIT.IsMatch(canon))
                        return e.Drop("leader-board digit card (" + canon + "): it shows a rider number only when the LeaderBoard runtime sets its texture (stock Reflex LPos*_Num* use the blank LRed_B); " +
                            "v1 demotes LeaderBoardRSData to a static prop, so the Untamed placeholder digit " + SlotText(ds) + " would show a fixed made-up number");
                    AlphaAlt alt = null;
                    if (ar != null && ar.Value.min >= OPAQUE_ALPHA_MIN)
                    {
                        if (opts.alpha_source == "program" && sinfo != null && !SX_FLOOR.IsMatch(canon))
                            alt = ProgramAlpha(ctx, sinfo, texl, d, aref, atest ? (states_pc.Get("ALPHAFUNC") is long af ? af : D3DCMP_GREATER) : D3DCMP_GREATER);
                        if (alt == null)
                            return e.Drop(alpha_cls + " material whose diffuse " + d + " has no transparent texel (alpha " + ar.Value.min + ".." + ar.Value.max + "): the Untamed cut-out came from another input " +
                                "(mask texture / shader), which no stock alpha template reproduces; drawn, it would be an opaque sheet");
                        T = "Template_Diffuse_Alpha";
                        reasons.Add(alt.reason + " (--alpha-source program)");
                        var names = new List<string>(); foreach (var t in alt.tex) names.Add(t?.name);
                        e.extra.Set("alpha_program", new JObj().Set("output", alt.output).Set("line", alt.line).Set("textures", Lst(names)));
                        foreach (var kv in alt.recipes.items) ctx.gen_recipes.Set(kv.Key, kv.Value);
                    }
                    if (alt == null && T == "Template_Eco_Frond" && va != null)
                    {
                        long tmax = ar != null ? ar.Value.max : 255, vamax = UxJson.Int(va[1]);
                        if (tmax - vamax <= aref)
                        {
                            T = "Template_Diffuse_Alpha";
                            reasons.Add("Template_Eco_Frond outputs alpha = saturate(texture alpha - vertex alpha) (stock SharedPixelShader add_sat_pp oC0.w, r1.w, -v2.w); these groups carry vertex alpha up to " +
                                I(vamax) + " against texture alpha up to " + I(tmax) + " and ALPHAREF " + I(aref) + ", so no texel would draw -> Template_Diffuse_Alpha (alpha from the texture only)");
                        }
                    }
                    if (alt != null) tex = new List<Slot>(alt.tex);
                    else { tex = new List<Slot>(); for (int i = 0; i < ctx.lib[T].nslots; i++) tex.Add(ds); }
                }
                else
                {
                    if (alpha_cls != null)
                    {
                        reasons.Add(alpha_cls + " material treated as opaque (--alpha opaque)");
                        if (reqTemplate == null || IsAlphaTemplate(T)) T = null;
                    }
                    if (T != null)                                       // requested template (switches still apply)
                    {
                        if (req_tex != null)
                        {
                            if (req_tex.Count != ctx.lib[T].nslots)
                            {
                                e.warnings.Add("request gives " + req_tex.Count + " textures for " + T + " (" + ctx.lib[T].nslots + " slots)");
                                return e.Drop("requested texture count does not match " + T);
                            }
                            tex = new List<Slot>(req_tex);
                        }
                        else tex = SlotFill(T, pDiffuse, pLightmap, pNormal, pAo);
                        reasons.Add("requested template " + T);
                        Slot d = tex[DiffuseSlot(T)];
                        if (T == "Template_Prelit" && opts.prelit_off) { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("--prelit-off -> Template_Diffuse"); }
                        else if ((T == "Template_Diffuse_Nrm" || T == "Template_Diffuse_AO") && opts.nrm_off) { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("--nrm-off -> Template_Diffuse"); }
                    }
                    else
                    {
                        Slot d = Slot.Plain(pDiffuse); string lm = pLightmap, nm = pNormal, ao = pAo;
                        if (bale_nm != null && opts.bale_base != "logo")
                        {
                            string decal = SlotText(d);
                            d = opts.bale_base == "hay" ? Slot.LibTex("GL_Hay") : Slot.Plain(opts.bale_base == "gray" ? DEFAULT_GRAY : DEFAULT_WHITE);
                            nm = bale_nm; lm = null; ao = null;
                            reasons.Add("hay bale: the sponsor decal " + decal + " (alpha-cut, CLAMP, on the Untamed UV1 projection) sits on a shader-made hay colour no stock template reproduces; base " +
                                d.name + " on the UV0 atlas (--bale-base " + opts.bale_base + ")");
                        }
                        if (!string.IsNullOrEmpty(lm) && !opts.prelit_off)
                        {
                            T = "Template_Prelit"; tex = new List<Slot> { Slot.Plain(lm), d };
                            reasons.Add("lightmap " + lm + " (UV1) + diffuse " + SlotText(d) + " -> Template_Prelit");
                        }
                        else if (!string.IsNullOrEmpty(lm)) { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("lightmapped, --prelit-off -> Template_Diffuse"); }
                        else if (!string.IsNullOrEmpty(nm) && d != null)
                        {
                            if (opts.nrm_off) { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("normal map, --nrm-off -> Template_Diffuse"); }
                            else { T = "Template_Diffuse_Nrm"; tex = new List<Slot> { d, Slot.Plain(nm) }; reasons.Add("diffuse + normal -> Template_Diffuse_Nrm"); }
                        }
                        else if (!string.IsNullOrEmpty(ao) && d != null)
                        {
                            if (opts.nrm_off) { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("AO map, --nrm-off -> Template_Diffuse"); }
                            else { T = "Template_Diffuse_AO"; tex = new List<Slot> { Slot.Plain(ao), d }; reasons.Add("diffuse + AO (UV1) -> Template_Diffuse_AO"); }
                        }
                        else { T = "Template_Diffuse"; tex = new List<Slot> { d }; reasons.Add("diffuse -> Template_Diffuse"); }
                    }
                }
            }
            var Lt = ctx.lib[T];
            var gen_src = new Dictionary<int, string>();                 // slot -> Untamed texture a generated slot comes from
            if (opts.composite == "bake" && sinfo != null && Has(OPAQUE_TEMPLATES, T) && bale_nm == null)
            {
                int di = DiffuseSlot(T);
                Slot cs = di < tex.Count ? tex[di] : null;
                if (cs != null && cs.IsStr && !IsTemplateDefault(cs.name) && sidx.ContainsKey(cs.name))
                {
                    string c = cs.name; int k = sidx[c];
                    var cr = ctx.Texture(c).rec;
                    var comp = UxShaderProg.AnalyseComposite(sinfo, k, sinfo.literals);
                    if (comp != null && comp.Has(comp.bg, 'x') && comp.Has(comp.bg, 'y') && comp.Has(comp.bg, 'z') && comp.Has(comp.k, 'x') && comp.Has(comp.k, 'y') && comp.Has(comp.k, 'z')
                        && cr != null && !IsRenderTarget(cr) && (UxShaderProg.FetchDest(sinfo, k) ?? "").StartsWith("xyz", StringComparison.Ordinal))
                    {
                        var ar = AlphaRange(ctx, UxJson.Str(cr.Get("file")));
                        if (ar != null && ar.Value.min < OPAQUE_ALPHA_MIN)
                        {
                            var bg = Rounded(new[] { comp.Get(comp.bg, 'x'), comp.Get(comp.bg, 'y'), comp.Get(comp.bg, 'z') }, 6);
                            var kk = Rounded(new[] { comp.Get(comp.k, 'x'), comp.Get(comp.k, 'y'), comp.Get(comp.k, 'z') }, 6);
                            var rc = new JObj().Set("kind", "composite").Set("source", c).Set("bg", bg).Set("k", kk);
                            string n = GenName(rc);
                            ctx.gen_recipes.Set(n, rc);
                            tex = new List<Slot>(tex); tex[di] = Slot.Gen(n); gen_src[di] = c;
                            e.extra.Set("composite", new JObj().Set("source", c).Set("bg", bg).Set("k", kk).Set("lerp", comp.lerp).Set("lines", Lst(comp.lines)));
                            prog_notes.Add("Untamed composites " + c + " over the literal " + UxPyFmt.ReprList(Doubles(bg)) + " (" + string.Join(" ; ", comp.lines) + "): baked into " + n + " (--composite bake)");
                        }
                    }
                }
            }
            if (sinfo != null && Has(OPAQUE_TEMPLATES, T) && bale_nm == null)          // Arena v3 program colour (tint)
            {
                int di = DiffuseSlot(T);
                Slot cs = di < tex.Count ? tex[di] : null;
                var pc_ = cs != null && cs.IsStr && cs.name != null && sidx.ContainsKey(cs.name) ? ProgramColour(ctx, sinfo, texl, canon) : null;
                if (pc_ != null && pc_.kind == "affine" && texl[pc_.sampler].canon == cs.name
                    && (Math.Abs(pc_.offset[0]) > 1e-6 || Math.Abs(pc_.offset[1]) > 1e-6 || Math.Abs(pc_.offset[2]) > 1e-6))
                {
                    string c = cs.name;
                    var rc = new JObj().Set("kind", "tint").Set("source", c).Set("scale", DList(pc_.scale)).Set("offset", DList(pc_.offset));
                    string n = GenName(rc);
                    ctx.gen_recipes.Set(n, rc);
                    tex = new List<Slot>(tex); tex[di] = Slot.Gen(n); gen_src[di] = c;
                    e.extra.Set("program_colour", new JObj().Set("kind", "tint").Set("source", c).Set("scale", DList(pc_.scale)).Set("offset", DList(pc_.offset))
                        .Set("light_left_out", SList(pc_.light)).Set("how", pc_.how));
                    prog_notes.Add("Untamed diffuse albedo " + pc_.how + " with scale " + UxPyFmt.ReprList(pc_.scale) + ", offset " + UxPyFmt.ReprList(pc_.offset) +
                        " (light factor " + UxShaderProg.PyStrListRepr(pc_.light) + " left out): baked into " + n + " (Arena v3 program colour, freestyle ramp material)");
                }
            }
            if (e.extra.Has("alpha_program"))
                for (int si = 0; si < tex.Count; si++)
                {
                    var t = tex[si];
                    if (t != null && t.kind == 'g' && ctx.gen_recipes.Get(t.name) is JObj gr && Truthy(gr.Get("source"))) gen_src[si] = UxJson.Str(gr.Get("source"));
                }

            // --- texture assets (template defaults, jumbotron, UX_ renames)
            var src_tex = new List<string>(); var assets = new List<string>();
            foreach (var t in tex)
            {
                if (t == null)
                {
                    bool found = false; string a_ = null, note_ = null; JObj rec_ = null;
                    if (opts.literal_albedo == "program" && sinfo != null && Has(OPAQUE_TEMPLATES, T)) found = ProgramAlbedo(sinfo, texl, dflt, out a_, out rec_, out note_);
                    if (!found && sinfo != null && Has(OPAQUE_TEMPLATES, T))           // Arena v3 program colour (literal)
                    {
                        var pc_ = ProgramColour(ctx, sinfo, texl, canon);
                        if (pc_ != null && pc_.kind == "literal" && Math.Max(Math.Max(pc_.rgb[0], pc_.rgb[1]), pc_.rgb[2]) <= 1.0)
                        {
                            rec_ = new JObj().Set("kind", "literal").Set("rgb", DList(pc_.rgb)).Set("rule", "diffuse_albedo");
                            a_ = GenName(rec_);
                            note_ = "Arena v3 program colour (freestyle ramp material): " + pc_.how + ", light factor " + UxShaderProg.PyStrListRepr(pc_.light) + " left out";
                            found = true;
                            e.extra.Set("program_colour", new JObj().Set("kind", "literal").Set("rgb", DList(pc_.rgb)).Set("light_left_out", SList(pc_.light)).Set("how", pc_.how));
                        }
                    }
                    if (!found)
                    {
                        src_tex.Add(null); assets.Add(dflt);
                        reasons.Add("no usable diffuse -> " + dflt);
                        continue;
                    }
                    e.extra.Set("literal_albedo", new JObj().Set("default", dflt).Set("used", a_).Set("rgb", rec_ != null ? rec_.Get("rgb") : new List<object> { 0.0, 0.0, 0.0 }).Set("how", note_));
                    if (rec_ != null)
                    {
                        ctx.gen_recipes.Set(a_, rec_);
                        src_tex.Add(null); assets.Add(a_);
                        if (!e.generated_textures.Contains(a_)) e.generated_textures.Add(a_);
                        reasons.Add("no usable diffuse: the Untamed program's literal albedo " + UxPyFmt.ReprList(Doubles(UxJson.Arr(rec_.Get("rgb")))) + " -> " + a_ +
                            " (generated; --literal-albedo program; default was " + dflt + ")");
                    }
                    else
                    {
                        src_tex.Add(null); assets.Add(a_);
                        reasons.Add("no usable diffuse: Untamed draws only the fog term (albedo 0) -> " + a_ + " (--literal-albedo program; default was " + dflt + ")");
                    }
                    continue;
                }
                if (t.kind == 'g')                                       // ('gen', name): generated here or by the terrain stage (request)
                {
                    src_tex.Add(null); assets.Add(t.name);
                    if (!e.generated_textures.Contains(t.name)) e.generated_textures.Add(t.name);
                    continue;
                }
                if (t.kind == 'l')                                       // ('lib', name): a stock Reflex texture
                {
                    src_tex.Add(null); assets.Add(t.name);
                    if (!e.library_textures.Contains(t.name)) e.library_textures.Add(t.name);
                    continue;
                }
                if (IsTemplateDefault(t.name)) { src_tex.Add(null); assets.Add(t.name); continue; }
                var (c, r) = ctx.Texture(t.name);
                if (r != null && IsRenderTarget(r))
                {
                    src_tex.Add(null); assets.Add(JUMBO_REPLACEMENT);
                    e.substituted.Set(c, JUMBO_REPLACEMENT);
                    reasons.Add(c + " is the jumbotron render texture (flags 0x200, " + PyStr(r.Get("width")) + "x" + PyStr(r.Get("height")) + ") -> " + JUMBO_REPLACEMENT);
                    continue;
                }
                if (c == null) throw new UxException("texture " + t.name + " of material " + name + " is not in the closure");
                var (a, how) = AssetName(ctx.stock, c, "texture");
                src_tex.Add(how != "template" ? c : null);
                assets.Add(a);
            }
            e.template = T; e.textures = assets; e.source_textures = src_tex; e.slots = new List<string>(Lt.slots);
            e.uv_sets = new List<List<string>>(); foreach (var x in Lt.uv.mesh_uv) e.uv_sets.Add(new List<string>(x));

            // --- render states
            var sp = new JObj();
            if (states_pc.Has("CULLMODE")) sp.Set("CULLMODE", states_pc.Get("CULLMODE"));
            if (IsAlphaTemplate(T))
            {
                sp.Set("ALPHATESTENABLE", 1L);
                if (Truthy(states_pc.Get("ALPHATESTENABLE"))) { foreach (string k in new[] { "ALPHAREF", "ALPHAFUNC" }) if (states_pc.Has(k)) sp.Set(k, states_pc.Get(k)); }
                else { sp.Set("ALPHAREF", 0L); sp.Set("ALPHAFUNC", (long)D3DCMP_GREATER); }
                if (eff_cls == "blend" && opts.blend == "keep")
                {
                    sp.Set("ALPHABLENDENABLE", 1L);
                    foreach (string k in new[] { "SRCBLEND", "DESTBLEND", "BLENDOP" }) if (states_pc.Has(k)) sp.Set(k, states_pc.Get(k));
                }
                else sp.Set("ALPHABLENDENABLE", 0L);
            }
            else { sp.Set("ALPHATESTENABLE", 0L); sp.Set("ALPHABLENDENABLE", 0L); }
            // --- sampler address modes from the Untamed sampler of each slot's texture
            var spl = new List<JObj>();
            for (int si = 0; si < src_tex.Count; si++)
            {
                var patch = new JObj();
                string c = src_tex[si] ?? (gen_src.TryGetValue(si, out string gs) ? gs : null);
                if (c != null && Lt.samplers[si].Count > 0)
                {
                    TexL x = null; foreach (var y in texl) if (y.canon == c) { x = y; break; }
                    if (x != null)
                    {
                        var tr = Translate(x.pairs, true, out var unk2);
                        if (unk2.Count > 0) e.warnings.Add("Xenon sampler states outside the translation tables ignored: " + ReprPairs(unk2));
                        foreach (string k in PATCH_SS) if (tr.Has(k)) patch.Set(k, tr.Get(k));
                    }
                    // DESIGN 18: Open Reflex draws trilinear only when MIPFILTER == 2; the exemplars of Prelit / Diffuse / Diffuse_Nrm /
                    // Diffuse_Alpha hold 1 (point mips). Only slots holding a converted texture.
                    if (opts.mipfilter == "linear") patch.Set("MIPFILTER", (long)MIPFILTER_LINEAR);
                }
                spl.Add(patch);
            }
            e.state_patch = sp; e.sampler_patch = spl;
            // --- UV sets: the template's (bytecode) vs Untamed's (lightmap / AO on UV1, the rest on UV0)
            bool swap = false;
            object diffuse_uv = req?.Get("diffuse_uv");
            if (req != null && diffuse_uv == null && Truthy(req.Get("uv0"))) diffuse_uv = "TEXCOORD0";      // the part's UV0 holds the diffuse coordinates
            if (req != null && diffuse_uv != null)
            {
                string du = diffuse_uv is long dn ? "TEXCOORD" + I(dn) : PyStr(diffuse_uv).ToUpperInvariant();
                var tu = Lt.uv.mesh_uv[DiffuseSlot(T)];
                swap = tu.Count > 0 && !tu.Contains(du);
                if (swap) reasons.Add("request: diffuse on " + du + ", template samples " + ReprList(tu) + " -> swap_uv");
            }
            else if ((T == "Template_Prelit" || T == "Template_Diffuse_AO") && Lt.uv.mesh_uv[0].Count == 1 && Lt.uv.mesh_uv[0][0] == "TEXCOORD0") swap = true;    // never true for the stock bytecode
            e.swap_uv = swap;
            string dsrc = DiffuseSlot(T) < src_tex.Count ? src_tex[DiffuseSlot(T)] : null;
            if (dsrc != null)
            {
                var dr = ctx.Texture(dsrc).rec;
                e.diffuse_alpha_cut = AlphaCut(ctx, UxJson.Str(dr.Get("file")));
                if (!IsAlphaTemplate(T) && T != "crowd" && (e.diffuse_alpha_cut ?? 0) > 0.05)
                    e.warnings.Add("diffuse " + dsrc + " is " + UxPyFmt.F(100 * e.diffuse_alpha_cut.Value, 0) + "% alpha-cut on an opaque template: its cut-out texels show their RGB (Untamed composited it over a shader colour)");
            }
            ctx.UvEvidence().TryGetValue(canon, out var ev);
            e.uv_evidence = ev;
            if (ev != null && (T == "Template_Prelit" || T == "Template_Diffuse_AO") && ev.Get("uv1_in01") is double u1 && u1 < 0.75)
                e.warnings.Add(T + ": only " + UxPyFmt.F(100 * u1, 0) + "% of UV1 inside 0..1 (lightmap/AO atlas expected on UV1)");
            e.action = "convert";
            var all = new List<string>(reasons); all.AddRange(prog_notes);
            e.reason = string.Join("; ", all);
            return e;
        }
        /// ux_materials.LevelCtx + plan_material for callers outside the stage (the builder's MassiveAd panels plan materials that are
        /// not in the level's closure): the level context of a work store, to which closure records can be added, and the plan of one
        /// material as its materials.json entry (action, template, textures, state_patch, sampler_patch ...; no bytes, no asset name).
        public sealed class Planner
        {
            readonly Ctx _ctx; readonly UxMaterialsOptions _opts;
            public Planner(UxContext ux, UxMaterialsOptions options = null)
            {
                _opts = options ?? new UxMaterialsOptions(); _opts.Validate();
                _ctx = new Ctx(ux, _opts.jobs > 0 ? _opts.jobs : Math.Max(1, Environment.ProcessorCount));
                _ctx.lib = Lib(_ctx.stock);
            }
            /// ctx.mat[name.lower()] = (name, rec) / ctx.tex[...]: a closure record (level.json's shape: file, format, width, height ...).
            public void AddMaterial(string name, JObj rec) => _ctx.mat[Lower(name)] = new KeyValuePair<string, JObj>(name, rec);
            public void AddTexture(string name, JObj rec) => _ctx.tex[Lower(name)] = new KeyValuePair<string, JObj>(name, rec);
            public bool HasMaterial(string name) => _ctx.mat.ContainsKey(Lower(name));
            public bool HasTexture(string name) => _ctx.tex.ContainsKey(Lower(name));
            /// plan_material(name, ctx, opts, req, gen): request = a material request (name, source, template, textures ...),
            /// generated = the generated textures a request may name {name: meta}.
            public JObj Plan(string name, JObj request = null, JObj generated = null) => PlanMaterial(name, _ctx, _opts, request, generated).ToJson();
            /// ctx.gen_recipes: the generated textures the plans made so far ask for {name: recipe}.
            public JObj GeneratedRecipes => _ctx.gen_recipes;
        }

        /// '%s' % d of a slot value: a name, None, or the tuple ('lib', name) / ('gen', name).
        static string SlotText(Slot s) => s == null ? "None" : s.IsStr ? s.name : "(" + Repr(s.kind == 'g' ? "gen" : "lib") + ", " + Repr(s.name) + ")";
        /// str(v) of a JSON value.
        static string PyStr(object v)
        {
            switch (v)
            {
                case null: return "None";
                case bool b: return b ? "True" : "False";
                case long l: return I(l);
                case double d: return UxPyFmt.Repr(d);
                case string s: return s;
            }
            return System.Convert.ToString(v, CultureInfo.InvariantCulture);
        }

        static string SafeFile(string name) => UNSAFE_FILE.Replace(name, "_");

        // ============================================================================== convert
        /// convert(level, ...): converts the materials of the context's level and returns the manifest (the content of materials.json).
        /// Inputs: ctx.disc, ctx.stock (with its template name tables), ctx.work (level.json; terrain/material_requests.json and
        /// textures/textures.json when present). progress gets (done, total) material counts. Cancelled through ctx.cancel.
        public static JObj Convert(UxContext ux, UxMaterialsOptions options = null, Action<int, int> progress = null)
        {
            var opts = options ?? new UxMaterialsOptions();
            opts.Validate();
            UxBin.Require(ux != null && ux.disc != null && ux.stock != null && ux.work != null, "ux_materials: the disc level, the stock library and the work store are needed");
            int jobs = opts.jobs > 0 ? opts.jobs : Math.Max(1, Environment.ProcessorCount);
            var ctx = new Ctx(ux, jobs);
            var lv = ctx.lv;
            ctx.lib = Lib(ctx.stock);
            string outDir = opts.OutDir();
            var reqs = LoadRequests(ctx, opts.requests);
            var want = new List<string>();
            if (opts.names != null) want.AddRange(opts.names);
            else
            {
                foreach (var kv in UxJson.Obj(ctx.closure.Get("material")).items) want.Add(kv.Key);
                UxNp.StableSort(want, (a, b) => string.CompareOrdinal(Lower(a), Lower(b)));
            }
            foreach (var kv in reqs.items)
                if (!want.Contains(kv.Key) && (opts.names == null || opts.names.Contains(kv.Key))) want.Add(kv.Key);
            var man = new List<KeyValuePair<string, Entry>>(); var manIdx = new Dictionary<string, int>(StringComparer.Ordinal);
            var blobs = new Dictionary<string, byte[]>(StringComparer.Ordinal); long blobBytes = 0;
            var problems = new List<object>(); var warnings = new List<object>();
            int done = 0;
            foreach (string nm in want)
            {
                ctx.cancel.ThrowIfCancellationRequested();
                var e = PlanMaterial(nm, ctx, opts, reqs.Get(nm), reqs.gen);
                if (e.action == "convert")
                {
                    var (a, how) = AssetName(ctx.stock, nm, "material");
                    if (how == "template")
                    {
                        e.action = "template"; e.asset = a;
                        e.reason = e.reason + "; name held by both templates: the template material " + a + " is used";
                    }
                    else
                    {
                        e.asset = a;
                        if (how == "renamed") e.warnings.Add("material name clashes with template " + ReprDict(ClashDict(TemplateClash(ctx.stock, nm, "material"))) + ": asset renamed " + a);
                        var L = ctx.lib[e.template];
                        byte[] data = BuildMaterial(L, e.template, e.textures, e.state_patch, e.sampler_patch, out var st, out var smp, out var added);
                        e.states = NamedPc(st);
                        e.samplers = new List<object>(); foreach (var s in smp) e.samplers.Add(NamedPc(s, true));
                        var exs = new Dictionary<string, long>(StringComparer.Ordinal);
                        foreach (object x in NamedPc(L.states)) { var p = (List<object>)x; exs[(string)p[0]] = (long)p[1]; }
                        e.changed = new JObj();
                        foreach (object x in e.states)
                        {
                            var p = (List<object>)x; string k = (string)p[0]; long v = (long)p[1];
                            bool have = exs.TryGetValue(k, out long old);
                            if (!have || old != v) e.changed.Set(k, new List<object> { have ? (object)old : null, v });
                        }
                        if (added.Count > 0) e.warnings.Add("states appended to the exemplar list: " + ReprList(added));
                        e.md5 = UxBin.Md5(data); e.size = data.Length;
                        if (!blobs.ContainsKey(a)) blobBytes += data.Length; else blobBytes += data.Length - blobs[a].Length;
                        blobs[a] = data;
                    }
                }
                if (manIdx.TryGetValue(nm, out int at)) man[at] = new KeyValuePair<string, Entry>(nm, e);
                else { manIdx[nm] = man.Count; man.Add(new KeyValuePair<string, Entry>(nm, e)); }
                progress?.Invoke(++done, want.Count);
            }
            // file names (unique case-insensitively: NTFS)
            var seenFile = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var kv in man)
            {
                var e = kv.Value;
                if (e.action != "convert") continue;
                string f = SafeFile(e.asset) + ".material";
                if (seenFile.TryGetValue(Lower(f), out string other)) problems.Add("material assets " + other + " and " + kv.Key + " collide on disk / case-insensitively");
                seenFile[Lower(f)] = kv.Key;
                e.file = f;
            }
            var conv = new List<Entry>(); foreach (var kv in man) if (kv.Value.action == "convert") conv.Add(kv.Value);
            // textures
            var needed = new HashSet<string>(StringComparer.Ordinal); var tassets = new List<KeyValuePair<string, string>>(); var subst = new JObj(); var tpl_tex = new SortedSet<string>(StringComparer.Ordinal);
            var libnames = new HashSet<string>(StringComparer.Ordinal); var libset = new SortedSet<string>(StringComparer.Ordinal); var genset = new SortedSet<string>(StringComparer.Ordinal);
            foreach (var e in conv) { foreach (string t in e.library_textures) { libnames.Add(t); libset.Add(t); } foreach (string t in e.generated_textures) { libnames.Add(t); genset.Add(t); } }
            foreach (var e in conv)
                for (int i = 0; i < e.source_textures.Count && i < e.textures.Count; i++)
                {
                    string c = e.source_textures[i], a = e.textures[i];
                    if (c == null) { if (!libnames.Contains(a)) tpl_tex.Add(a); }
                    else
                    {
                        needed.Add(c);
                        int at = tassets.FindIndex(x => x.Key == c);
                        if (at < 0) tassets.Add(new KeyValuePair<string, string>(c, a)); else tassets[at] = new KeyValuePair<string, string>(c, a);
                    }
                }
            foreach (var e in conv) foreach (var kv in e.substituted.items) subst.Set(kv.Key, kv.Value);
            var libtex = new JObj();
            foreach (string name in libset)
            {
                var st = StockTexture(ctx.stock, name);
                if (AnyClash(TemplateClash(ctx.stock, name, "texture")) || ctx.Texture(name).canon != null) problems.Add("library texture " + name + " clashes with a template or Untamed texture name");
                var meta = CopyObj(st.Value); meta.Set("file", "textures/" + name + ".texture");
                libtex.Set(name, meta);
                blobs["textures/" + name + ".texture"] = st.Key; blobBytes += st.Key.Length;
            }
            // generated textures: the ones made here from the Untamed program (in parallel; each depends only on its recipe), the others
            // from the terrain stage's request file
            var gennames = new List<string>(genset);
            var made = new KeyValuePair<byte[], JObj>[gennames.Count]; var failed = new string[gennames.Count];
            Parallel.For(0, gennames.Count, new ParallelOptions { MaxDegreeOfParallelism = jobs, CancellationToken = ctx.cancel }, i =>
            {
                if (!(ctx.gen_recipes.Get(gennames[i]) is JObj rec)) return;
                try
                {
                    byte[] src = Truthy(rec.Get("source")) ? ctx.TextureBytes(UxJson.Str(rec.Get("source"))) : null;
                    byte[] data = GeneratedTexture(src, rec, out JObj info);
                    made[i] = new KeyValuePair<byte[], JObj>(data, info);
                }
                catch (OperationCanceledException) { throw; }
                catch (Exception ex) { failed[i] = ex.GetType().Name + "(" + Repr(ex.Message) + ")"; }
            });
            var gentex = new JObj();
            string rootRel = "work/" + lv.disc + "/" + outDir + "/";
            for (int i = 0; i < gennames.Count; i++)
            {
                string name = gennames[i];
                if (ctx.gen_recipes.Get(name) is JObj rec)                        // fix round 2: made here from the Untamed program
                {
                    if (failed[i] != null) { problems.Add("generated texture " + name + " (" + UxJson.Dumps(rec, -1, true, false) + "): " + failed[i]); continue; }
                    byte[] data = made[i].Key; JObj info = made[i].Value;
                    var meta = new JObj().Set("provided_by", "ux_materials (fix round 2: Untamed pixel program)").Set("recipe", rec).Set("file", "textures/" + name + ".texture")
                        .Set("path", rootRel + "textures/" + name + ".texture").Set("md5", UxBin.Md5(data)).Set("size", (long)data.Length);
                    foreach (var kv in info.items) meta.Set(kv.Key, kv.Value);
                    if (UxJson.Num(info.Get("max_over")) > 1.0)
                        problems.Add("generated texture " + name + ": an encoded level misses its tolerance (errors " + UxJson.Dumps(info.Get("err"), -1) + ", tolerances " + UxJson.Dumps(info.Get("tol"), -1) + ")");
                    if (AnyClash(TemplateClash(ctx.stock, name, "texture")) || ctx.Texture(name).canon != null) problems.Add("generated texture " + name + " clashes with a template or Untamed texture name");
                    blobs["textures/" + name + ".texture"] = data; blobBytes += data.Length;
                    gentex.Set(name, meta);
                    continue;
                }
                if (!(reqs.gen.Get(name) is JObj gm)) throw new UxException("KeyError " + name);
                var m2 = CopyObj(gm); m2.Set("provided_by", "WP6 ux_terrain (terrain/material_requests.json)");
                bool ok = reqs.genFile.TryGetValue(name, out string wf) && ctx.work.Exists(wf) && UxBin.Md5(ctx.work.ReadBytes(wf)) == (m2.Get("md5") as string);
                if (!ok) problems.Add("generated texture " + name + ": file " + PyStr(m2.Get("path")) + " missing or md5 differs from the request file");
                if (AnyClash(TemplateClash(ctx.stock, name, "texture")) || ctx.Texture(name).canon != null) problems.Add("generated texture " + name + " clashes with a template or Untamed texture name");
                gentex.Set(name, m2);
            }
            var neededSorted = new List<string>(needed);
            neededSorted.Sort((a, b) => { int c = string.CompareOrdinal(Lower(a), Lower(b)); return c != 0 ? c : string.CompareOrdinal(a, b); });
            foreach (string c in neededSorted)
            {
                var r = ctx.Texture(c).rec;
                if (r == null || !ctx.disc.HasExtractFile(UxJson.Str(r.Get("file")))) problems.Add("texture " + c + " missing from extract/");
            }
            var lowerAssets = new List<KeyValuePair<string, int>>();
            foreach (var kv in tassets)
            {
                string la = Lower(kv.Value); int at = lowerAssets.FindIndex(x => x.Key == la);
                if (at < 0) lowerAssets.Add(new KeyValuePair<string, int>(la, 1)); else lowerAssets[at] = new KeyValuePair<string, int>(la, lowerAssets[at].Value + 1);
            }
            foreach (var kv in lowerAssets) if (kv.Value > 1) problems.Add(kv.Value + " textures map to the asset name " + kv.Key + " (case-insensitive)");
            var shaderSet = new SortedSet<string>(StringComparer.Ordinal); var hist = new SortedDictionary<string, long>(StringComparer.Ordinal); var acts = new SortedDictionary<string, long>(StringComparer.Ordinal);
            foreach (var e in conv) { shaderSet.Add(e.template); hist[e.template] = (hist.TryGetValue(e.template, out long n) ? n : 0) + 1; }
            foreach (var kv in man) acts[kv.Value.action] = (acts.TryGetValue(kv.Value.action, out long n) ? n : 0) + 1;
            var shaders = new List<string>(shaderSet);
            foreach (var kv in man) foreach (string w in kv.Value.warnings) warnings.Add(kv.Key + ": " + w);
            foreach (string k in META_KEYS) if (manIdx.ContainsKey(k)) problems.Add("material name " + Repr(k) + " collides with a reserved materials.json key");
            var shader_files = new JObj();
            foreach (string T in shaders)
            {
                byte[] sh = ctx.lib[T].shader;
                shader_files.Set(T, new JObj().Set("file", "shaders/" + T + ".shader").Set("md5", UxBin.Md5(sh)).Set("size", (long)sh.Length)
                    .Set("in_template", new JObj().Set("fr", Has(IN_TEMPLATE_FR, T)).Set("sx", Has(IN_TEMPLATE_SX, T))));
            }
            // clashes (both template kinds), cross-check with the textures stage
            var clashMat = new JObj(); var clashTex = new JObj();
            foreach (var kv in man) { var cl = TemplateClash(ctx.stock, kv.Key, "material"); if (AnyClash(cl)) clashMat.Set(kv.Key, ClashDict(cl)); }
            foreach (string c in neededSorted) { var cl = TemplateClash(ctx.stock, c, "texture"); if (AnyClash(cl)) clashTex.Set(c, ClashDict(cl)); }
            if (ctx.work.Exists("textures/textures.json") && UxJson.ParseUtf8(ctx.work.ReadBytes("textures/textures.json")) is JObj tj && tj.Get("textures") is JObj tjt)
            {
                var tt = new Dictionary<string, JObj>(StringComparer.Ordinal);
                foreach (var kv in tjt.items) if (kv.Value is JObj tr) tt[Lower(kv.Key)] = tr;
                var both = new SortedSet<string>(needed, StringComparer.Ordinal); foreach (var kv in subst.items) both.Add(kv.Key);
                foreach (string c in both)
                {
                    object wantA = subst.Has(c) ? subst.Get(c) : null;
                    if (wantA == null) { int at = tassets.FindIndex(x => x.Key == c); wantA = at >= 0 ? tassets[at].Value : null; }
                    if (!tt.TryGetValue(Lower(c), out var r)) warnings.Add("textures.json (WP2) has no entry for " + c);
                    else if (!(r.Get("asset") is string ra && wantA is string wa && ra == wa) && !(r.Get("asset") == null && wantA == null))
                        problems.Add("texture " + c + ": WP2 asset " + ReprAny(r.Get("asset")) + " != materials " + ReprAny(wantA));
                }
            }
            // the manifest
            var doc = new JObj();
            foreach (var kv in man) doc.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value.ToJson()));
            var material_assets = new JObj();
            foreach (var kv in man) if (!Has(META_KEYS, kv.Key)) material_assets.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value.action == "convert" || kv.Value.action == "template" ? kv.Value.asset : null));
            var readers = new JObj();
            foreach (var kv in opts.readers)
                if ((kv.Key != "ux_shaderprog.py" && kv.Key != "ux_textures.py") || opts.ProgramUsed) readers.Set(kv.Key, kv.Value);
            var template_uv = new JObj();
            foreach (string T in TEMPLATES)
            {
                var u = ctx.lib[T].uv; var mu = new List<object>(); foreach (var x in u.mesh_uv) mu.Add(Lst(x));
                template_uv.Set(T, new JObj().Set("slots", Lst(u.slots)).Set("registers", Lst(u.registers)).Set("mesh_uv", mu).Set("vs_entry", u.vs_entry).Set("ps_entry", u.ps_entry));
            }
            List<object> MeshUv(string T) { var mu = new List<object>(); foreach (var x in ctx.lib[T].uv.mesh_uv) mu.Add(Lst(x)); return mu; }
            var texture_assets = new JObj(); foreach (var kv in tassets) texture_assets.Set(kv.Key, kv.Value);
            var histJ = new JObj(); foreach (var kv in hist) histJ.Set(kv.Key, kv.Value);
            var actsJ = new JObj(); foreach (var kv in acts) actsJ.Set(kv.Key, kv.Value);
            long swaps = 0; foreach (var e in conv) if (e.swap_uv) swaps++;
            void Meta(string k, object v) => doc.Set(k, v);
            Meta("schema", SCHEMA); Meta("level", lv.disc); Meta("short", lv.shortName); Meta("mode", lv.mode); Meta("options", opts.AsDict()); Meta("tool", "ux_materials.py");
            Meta("tool_md5", opts.tool_md5);
            Meta("source_files", new JObj().Set("level_json", "work/" + lv.disc + "/level.json").Set("level_json_md5", ctx.levelJsonMd5).Set("lib_index_md5", opts.lib_index_md5)
                .Set("requests", reqs.src).Set("readers", readers).Set("sm3dis_md5", opts.sm3dis_md5));
            Meta("out_dir", "work/" + lv.disc + "/" + outDir);
            Meta("template_uv", template_uv);
            Meta("prelit_texcoord", new JObj().Set("answer", "Template_Prelit samples the lightmap (s0 prelit_light_ao_texture) with mesh TEXCOORD1 and the diffuse (s1 diffuse_base_texture) with TEXCOORD0; " +
                    "Template_Diffuse_AO samples ao_texture with TEXCOORD1. Untamed keeps lightmaps/AO on UV1 -> swap_uv false.")
                .Set("prelit", MeshUv("Template_Prelit")).Set("diffuse_ao", MeshUv("Template_Diffuse_AO")).Set("evidence", Lst(ctx.lib["Template_Prelit"].uv.evidence)));
            Meta("shaders_needed", Lst(shaders)); Meta("shader_files", shader_files); Meta("textures_needed", Lst(neededSorted));
            Meta("texture_assets", texture_assets); Meta("template_textures", Lst(tpl_tex)); Meta("texture_substitutions", subst);
            Meta("library_textures", libtex); Meta("generated_textures", gentex); Meta("material_assets", material_assets);
            Meta("histogram", histJ); Meta("actions", actsJ);
            Meta("counts", new JObj().Set("materials", (long)want.Count).Set("converted", (long)conv.Count).Set("files", (long)conv.Count).Set("textures_needed", (long)needed.Count)
                .Set("library_textures", (long)libtex.items.Count).Set("generated_textures", (long)gentex.items.Count).Set("template_textures", (long)tpl_tex.Count)
                .Set("shaders", (long)shaders.Count).Set("requests", (long)reqs.items.Count).Set("bytes", blobBytes).Set("swap_uv", swaps));
            Meta("clashes", new JObj().Set("material", clashMat).Set("texture", clashTex));
            Meta("problems", problems); Meta("warnings", warnings); Meta("meta_keys", Lst(META_KEYS));
            if (opts.write)
            {
                ctx.cancel.ThrowIfCancellationRequested();
                var w = ctx.work; var keep = new HashSet<string>(StringComparer.Ordinal);
                foreach (var e in conv) { w.WriteBytes(outDir + "/" + e.file, blobs[e.asset]); keep.Add(Lower(e.file)); }
                foreach (string T in shaders) w.WriteBytes(outDir + "/shaders/" + T + ".shader", ctx.lib[T].shader);
                foreach (var kv in libtex.items) { string f = (string)((JObj)kv.Value).Get("file"); w.WriteBytes(outDir + "/" + f, blobs[f]); }
                foreach (var kv in gentex.items) { string f = ((JObj)kv.Value).Get("file") as string; if (f != null && blobs.ContainsKey(f)) w.WriteBytes(outDir + "/" + f, blobs[f]); }
                foreach (string p in w.List(outDir, "*.material"))                // stale files of an earlier run
                    if (!keep.Contains(Lower(p.Substring(p.LastIndexOf('/') + 1)))) w.Delete(p);
                foreach (string p in w.List(outDir + "/shaders", "*.shader")) if (!shaders.Contains(Stem(p))) w.Delete(p);
                foreach (string p in w.List(outDir + "/textures", "*.texture")) if (!libtex.Has(Stem(p)) && !gentex.Has(Stem(p))) w.Delete(p);
                w.WriteJson(outDir + "/materials.json", doc);
            }
            var actText = new List<string>(); foreach (var kv in man) { string a = kv.Value.action; if (!actText.Exists(x => x.StartsWith(a + " ", StringComparison.Ordinal))) actText.Add(a + " " + acts[a]); }
            ux.log?.Invoke("[ux_materials] " + lv.disc + " " + (opts.Tag().Length > 0 ? opts.Tag() : "default") + ": " + want.Count + " materials -> " + conv.Count + " converted (" + string.Join(", ", actText) + "), " +
                needed.Count + " textures needed, shaders " + string.Join(",", shaders).Replace("Template_", "") + (problems.Count > 0 ? "  PROBLEMS " + problems.Count : ""));
            return doc;
        }
        static string Stem(string path) { string f = path.Substring(path.LastIndexOf('/') + 1); int dot = f.LastIndexOf('.'); return dot >= 0 ? f.Substring(0, dot) : f; }
        static string ReprAny(object v) => v is string s ? Repr(s) : PyStr(v);

        /// entries(man): the per-material entries of a manifest (drops the META_KEYS).
        public static List<KeyValuePair<string, JObj>> Entries(JObj man)
        {
            var o = new List<KeyValuePair<string, JObj>>();
            foreach (var kv in man.items) if (!Has(META_KEYS, kv.Key) && kv.Value is JObj j && j.Has("action")) o.Add(new KeyValuePair<string, JObj>(kv.Key, j));
            return o;
        }
    }
}
