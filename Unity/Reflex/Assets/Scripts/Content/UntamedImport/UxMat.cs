// MX vs ATV Untamed converter, foundation: the ports of untamed/tools/uxmat.py (Untamed .material = BXML 1002 with big-endian Xenon
// state blobs; the Xenon -> D3D9 PC state and sampler-state tables; texture slot roles; the template suggestion) and of
// untamed/tools/d3d9sh.py (the Reflex PC .shader container and its D3D9 SM3 constant tables / declarations).
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// One (id, value) pair of a state blob.
    public struct UxStatePair
    {
        public uint id, value;
        public UxStatePair(uint id, uint value) { this.id = id; this.value = value; }
    }
    /// One translated state: (pc id, pc value, name).
    public struct UxPcState
    {
        public uint id, value; public string name;
        public UxPcState(uint id, uint value, string name) { this.id = id; this.value = value; this.name = name; }
    }

    /// One <Parameters> child: (node name, {attribute: value}, node value).
    public sealed class UxMatParam { public string name; public JObj attrs; public object value; }

    /// One pass of read_material's dict.
    public sealed class UxMatPass
    {
        public string vs, vs_entry, ps, ps_entry;
        /// textures: (textureName, sampler state pairs).
        public List<KeyValuePair<string, List<UxStatePair>>> textures = new List<KeyValuePair<string, List<UxStatePair>>>();
        /// null when the pass has no Parameters node.
        public List<UxMatParam> @params;
        public List<UxStatePair> states = new List<UxStatePair>();
    }

    /// read_material's dict: version, passes, bxml.
    public sealed class UxMaterial { public long version; public List<UxMatPass> passes = new List<UxMatPass>(); public UxBxml bxml; }

    public static class UxMat
    {
        // Xbox 360 D3DRENDERSTATETYPE -> (name, PC D3DRENDERSTATETYPE)
        static readonly Dictionary<uint, (string name, uint pc)> s_xrs = new Dictionary<uint, (string, uint)>
        {
            { 40, ("ZENABLE", 7) }, { 44, ("ZFUNC", 23) }, { 48, ("ZWRITEENABLE", 14) }, { 52, ("FILLMODE", 8) }, { 56, ("CULLMODE", 22) },
            { 60, ("ALPHABLENDENABLE", 27) }, { 64, ("SEPARATEALPHABLENDENABLE", 206) }, { 68, ("BLENDFACTOR", 193) },
            { 72, ("SRCBLEND", 19) }, { 76, ("DESTBLEND", 20) }, { 80, ("BLENDOP", 171) }, { 84, ("SRCBLENDALPHA", 207) },
            { 88, ("DESTBLENDALPHA", 208) }, { 92, ("BLENDOPALPHA", 209) }, { 96, ("ALPHATESTENABLE", 15) }, { 100, ("ALPHAREF", 24) },
            { 104, ("ALPHAFUNC", 25) }, { 108, ("STENCILENABLE", 52) }, { 112, ("TWOSIDEDSTENCILMODE", 185) },
            { 116, ("STENCILFAIL", 53) }, { 120, ("STENCILZFAIL", 54) }, { 124, ("STENCILPASS", 55) }, { 128, ("STENCILFUNC", 56) },
            { 132, ("STENCILREF", 57) }, { 136, ("STENCILMASK", 58) }, { 140, ("STENCILWRITEMASK", 59) },
            { 144, ("CCW_STENCILFAIL", 186) }, { 148, ("CCW_STENCILZFAIL", 187) }, { 152, ("CCW_STENCILPASS", 188) },
            { 156, ("CCW_STENCILFUNC", 189) }, { 176, ("COLORWRITEENABLE", 168) }, { 192, ("COLORWRITEENABLE1", 190) },
            { 196, ("COLORWRITEENABLE2", 191) }, { 200, ("COLORWRITEENABLE3", 192) },
        };
        static readonly uint[] s_xrsOrder = { 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 176, 192, 196, 200 };
        // Xbox 360 D3DSAMPLERSTATETYPE -> (name, PC D3DSAMPLERSTATETYPE)
        static readonly Dictionary<uint, (string name, uint pc)> s_xss = new Dictionary<uint, (string, uint)>
        {
            { 0, ("ADDRESSU", 1) }, { 4, ("ADDRESSV", 2) }, { 8, ("ADDRESSW", 3) }, { 12, ("BORDERCOLOR", 4) }, { 16, ("MAGFILTER", 5) },
            { 20, ("MINFILTER", 6) }, { 24, ("MIPFILTER", 7) }, { 28, ("MIPMAPLODBIAS", 8) }, { 32, ("MAXMIPLEVEL", 9) }, { 36, ("MAXANISOTROPY", 10) },
        };
        static Dictionary<uint, uint> Map(params uint[] kv) { var d = new Dictionary<uint, uint>(); for (int i = 0; i < kv.Length; i += 2) d[kv[i]] = kv[i + 1]; return d; }
        static readonly Dictionary<uint, uint> XCMP = Map(0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8);
        static readonly Dictionary<uint, uint> XCULL = Map(0, 1, 2, 2, 6, 3);
        static readonly Dictionary<uint, uint> XBLEND = Map(0, 1, 1, 2, 4, 3, 5, 4, 6, 5, 7, 6, 8, 9, 9, 10, 10, 7, 11, 8, 12, 14, 13, 15, 16, 11);
        static readonly Dictionary<uint, uint> XBLENDOP = Map(0, 1, 1, 2, 2, 4, 3, 5, 4, 3);
        static readonly Dictionary<uint, uint> XFILL = Map(0x1C00, 1, 0x1C01, 2, 0x1C02, 3);
        static readonly Dictionary<uint, uint> XADDR = Map(0, 1, 1, 2, 2, 3, 3, 5, 6, 4);
        static readonly Dictionary<uint, uint> XFILT = Map(0, 1, 1, 2, 2, 0, 4, 3);
        static readonly Dictionary<string, Dictionary<uint, uint>> VALUE_MAP = new Dictionary<string, Dictionary<uint, uint>>
        {
            { "ZFUNC", XCMP }, { "ALPHAFUNC", XCMP }, { "STENCILFUNC", XCMP }, { "CCW_STENCILFUNC", XCMP }, { "CULLMODE", XCULL },
            { "SRCBLEND", XBLEND }, { "DESTBLEND", XBLEND }, { "SRCBLENDALPHA", XBLEND }, { "DESTBLENDALPHA", XBLEND },
            { "BLENDOP", XBLENDOP }, { "BLENDOPALPHA", XBLENDOP }, { "FILLMODE", XFILL },
        };
        static readonly Dictionary<string, Dictionary<uint, uint>> SVALUE_MAP = new Dictionary<string, Dictionary<uint, uint>>
        {
            { "ADDRESSU", XADDR }, { "ADDRESSV", XADDR }, { "ADDRESSW", XADDR }, { "MAGFILTER", XFILT }, { "MINFILTER", XFILT }, { "MIPFILTER", XFILT },
        };

        /// XRS.get(id): (name, pc id) of a Xenon render state; false when unknown.
        public static bool Xrs(uint id, out string name, out uint pc) { bool ok = s_xrs.TryGetValue(id, out var v); name = v.name; pc = v.pc; return ok; }
        /// XSS.get(id): (name, pc id) of a Xenon sampler state.
        public static bool Xss(uint id, out string name, out uint pc) { bool ok = s_xss.TryGetValue(id, out var v); name = v.name; pc = v.pc; return ok; }
        /// The Xenon render-state ids of XRS in the table's order.
        public static IReadOnlyList<uint> XrsIds => s_xrsOrder;

        /// blob_pairs(b, ni, endian): the (id, value) u32 pairs of a state node's pool blob (numStates of them).
        public static List<UxStatePair> BlobPairs(UxBxml b, int ni, bool big = true)
        {
            long cnt = b.Attr(ni, "numStates") is long l ? l : 0;
            long off = b.nodes[ni][1];
            var r = new List<UxStatePair>((int)cnt);
            UxBin.Require(cnt == 0 || off >= 0 && off + 8 * cnt <= b.pool.Length, "material: state blob outside the pool");
            for (int i = 0; i < cnt; i++) r.Add(new UxStatePair(UxBin.U32(b.pool, (int)off + 8 * i, big), UxBin.U32(b.pool, (int)off + 8 * i + 4, big)));
            return r;
        }

        /// read_material(bytes, endian): an Untamed material (big = true) or a Reflex PC one (big = false).
        public static UxMaterial ReadMaterial(byte[] data, bool big = true)
        {
            var b = new UxBxml(data);
            var o = new UxMaterial { version = b.version, bxml = b };
            foreach (int p in b.FindAll(0, "Pass"))
            {
                int sh = b.Find(p, "Shaders");
                int vs = sh >= 0 ? b.Find(sh, "VertexShader") : -1, ps = sh >= 0 ? b.Find(sh, "PixelShader") : -1;
                object shAsset = sh >= 0 ? b.Attr(sh, "asset") : null;         // the Reflex 'crowd' form: <Shaders asset=..>
                var P = new UxMatPass
                {
                    vs = (vs >= 0 ? b.Attr(vs, "asset") : shAsset) as string, vs_entry = vs >= 0 ? b.Attr(vs, "entryPoint") as string : null,
                    ps = (ps >= 0 ? b.Attr(ps, "asset") : shAsset) as string, ps_entry = ps >= 0 ? b.Attr(ps, "entryPoint") as string : null,
                };
                int tx = b.Find(p, "Textures");
                if (tx >= 0)
                    foreach (int t in b.FindAll(tx, "Texture"))
                    {
                        int ss = b.Find(t, "XenonSamplerStates");
                        P.textures.Add(new KeyValuePair<string, List<UxStatePair>>(b.Attr(t, "textureName") as string, ss >= 0 ? BlobPairs(b, ss, big) : new List<UxStatePair>()));
                    }
                int pa = b.Find(p, "Parameters");
                if (pa >= 0)
                {
                    P.@params = new List<UxMatParam>();
                    foreach (int c in b.Children(pa))
                    {
                        var attrs = new JObj();
                        foreach (var kv in b.AttrItems(c)) attrs.Set(kv.Value, b.AttrValue(kv.Key));
                        P.@params.Add(new UxMatParam { name = b.Name(c), attrs = attrs, value = b.NodeValue(c) });
                    }
                }
                int xs = b.Find(p, "XenonStates");
                P.states = xs >= 0 ? BlobPairs(b, xs, big) : new List<UxStatePair>();
                o.passes.Add(P);
            }
            return o;
        }

        /// xenon_states_to_pc(pairs): [(pc id, pc value, name)]; an unknown id or value raises.
        public static List<UxPcState> XenonStatesToPc(IEnumerable<UxStatePair> pairs)
        {
            var res = new List<UxPcState>();
            foreach (var pr in pairs)
            {
                if (!s_xrs.TryGetValue(pr.id, out var x)) throw new UxException("unknown Xenon render state " + pr.id);
                uint v = pr.value;
                if (VALUE_MAP.TryGetValue(x.name, out var vm)) { if (!vm.TryGetValue(pr.value, out v)) throw new UxException("unknown Xenon " + x.name + " value " + pr.value); }
                res.Add(new UxPcState(x.pc, v, x.name));
            }
            return res;
        }
        /// xenon_sampler_to_pc(pairs).
        public static List<UxPcState> XenonSamplerToPc(IEnumerable<UxStatePair> pairs)
        {
            var res = new List<UxPcState>();
            foreach (var pr in pairs)
            {
                if (!s_xss.TryGetValue(pr.id, out var x)) throw new UxException("unknown Xenon sampler state " + pr.id);
                uint v = pr.value;
                if (SVALUE_MAP.TryGetValue(x.name, out var vm)) { if (!vm.TryGetValue(pr.value, out v)) throw new UxException("unknown Xenon " + x.name + " value " + pr.value); }
                res.Add(new UxPcState(x.pc, v, x.name));
            }
            return res;
        }

        /// named_states(pairs, pc): {state name: value} in pair order (a repeated state keeps its first position and its last value);
        /// an id without a name is keyed by its number as text (Python keys it by the int).
        public static JObj NamedStates(IEnumerable<UxStatePair> pairs, bool pc = false)
        {
            var o = new JObj();
            foreach (var pr in pairs)
            {
                string name = null;
                if (pc) { foreach (uint id in s_xrsOrder) if (s_xrs[id].pc == pr.id) name = s_xrs[id].name; }      // {pid: n for n, pid in XRS.values()}
                else if (s_xrs.TryGetValue(pr.id, out var x)) name = x.name;
                o.Set(name ?? pr.id.ToString(System.Globalization.CultureInfo.InvariantCulture), (long)pr.value);
            }
            return o;
        }

        // --------------------------------------------------------------------------------- texture slot roles
        static readonly KeyValuePair<string, Regex>[] ROLE_RX =
        {
            new KeyValuePair<string, Regex>("normal", new Regex(@"(_nm\d*$|_nm_|norm|nrm|normal|_n$|_n\d$|bump)", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("spec", new Regex(@"(spec|gloss)", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("lightmap", new Regex(@"(lightingmap|lightmap|massive_lighting|_lm$|_lm\d|lm_|[a-z0-9]lm$|_light$|_light\d$)", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("ao", new Regex(@"(_ao$|_ao_|^ao_|_ao\d|occ$|_occ|occlusion)", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("ramp", new Regex(@"^(black|white|grey|gray)2(black|white|grey|gray)", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("standin", new Regex(@"templatestandin", RegexOptions.CultureInvariant)),
            new KeyValuePair<string, Regex>("reflection", new Regex(@"(reflection|_refl|cube|_env)", RegexOptions.CultureInvariant)),
        };

        /// texture_role(name): 'normal', 'spec', 'lightmap', 'ao', 'ramp', 'standin', 'reflection' or 'diffuse'.
        public static string TextureRole(string name)
        {
            string n = name.ToLowerInvariant();
            foreach (var kv in ROLE_RX) if (kv.Value.IsMatch(n)) return kv.Key;
            return "diffuse";
        }

        /// guess_roles(texnames): role -> the first texture name with that role, in first-seen order ('diffuse' falls back to the first
        /// texture that is not a ramp, stand-in or normal map).
        public static JObj GuessRoles(IList<string> texnames)
        {
            var roles = new JObj();
            foreach (string t in texnames) { string r = TextureRole(t); if (!roles.Has(r)) roles.Set(r, t); }
            if (!roles.Has("diffuse"))
                foreach (string t in texnames)
                {
                    string r = TextureRole(t);
                    if (r != "ramp" && r != "standin" && r != "normal") { roles.Set("diffuse", t); break; }
                }
            return roles;
        }

        /// blend_class(states): 'opaque' | 'alphatest' | 'blend' from Untamed (Xenon) XenonStates pairs.
        public static string BlendClass(IEnumerable<UxStatePair> states)
        {
            var s = NamedStates(states);
            if (s.Get("ALPHABLENDENABLE") is long a && a != 0) return "blend";
            if (s.Get("ALPHATESTENABLE") is long t && t != 0) return "alphatest";
            return "opaque";
        }

        /// suggest_reflex_template(mat, uses_uv1, vertex_coloured): (template, slots: the texture names in the Reflex material's order;
        /// a slot may be null when the material has no diffuse texture).
        public static (string template, List<string> slots) SuggestReflexTemplate(UxMaterial mat, bool? uses_uv1 = null, bool? vertex_coloured = null)
        {
            var P = mat.passes[0];
            var tex = new List<string>(); foreach (var kv in P.textures) tex.Add(kv.Key);
            string vs = (P.vs ?? "").ToLowerInvariant();
            if (vs == "crowd") return ("crowd", tex);
            if (vs == "pyroparticles") return ("PyroParticles", tex);
            if (vs == "water") return ("Water", tex);
            var roles = GuessRoles(tex);
            string cls = BlendClass(P.states);
            string d = roles.Get("diffuse") as string;
            if (cls != "opaque") return ("Template_Eco_Frond", new List<string> { d });
            if (roles.Has("lightmap") && !string.IsNullOrEmpty(d) && uses_uv1 != false) return ("Template_Prelit", new List<string> { roles.Get("lightmap") as string, d });
            if (roles.Has("normal") && !string.IsNullOrEmpty(d)) return ("Template_Diffuse_Nrm", new List<string> { d, roles.Get("normal") as string });
            if (roles.Has("ao") && !string.IsNullOrEmpty(d) && uses_uv1 != false) return ("Template_Diffuse_AO", new List<string> { roles.Get("ao") as string, d });
            if (vertex_coloured == true) return ("Template_DF_VX", new List<string> { d });
            return ("Template_Diffuse", new List<string> { d });
        }
    }

    /// d3d9sh.py: the Reflex PC .shader container (u32 2, u32 n, n x {u32 0, u32 namelen, name, u32 size, bytecode}) and the D3D9 SM3
    /// bytecode summary.
    public static class UxD3d9sh
    {
        /// One CTAB constant: (name, register set 'b' / 'i' / 'c' / 's' (or its number as text), register index, register count).
        public struct Const { public string name, regset; public int index, count; }
        /// One dcl token: (usage name or number as text, usage index, register type, register number, the 4 bits above the usage).
        public struct Decl { public string usage; public int index, rtype, reg, extra; }
        public sealed class Entry { public string entry, kind; public List<Const> ctab; public List<Decl> decls; public byte[] code; }

        static string Usage(uint u)
        {
            string[] n = { "POSITION", "BLENDWEIGHT", "BLENDINDICES", "NORMAL", "PSIZE", "TEXCOORD", "TANGENT", "BINORMAL", "TESSFACTOR", "POSITIONT", "COLOR", "FOG", "DEPTH", "SAMPLE" };
            return u < n.Length ? n[u] : u.ToString(System.Globalization.CultureInfo.InvariantCulture);
        }
        static string Regset(int rs) => rs == 0 ? "b" : rs == 1 ? "i" : rs == 2 ? "c" : rs == 3 ? "s" : rs.ToString(System.Globalization.CultureInfo.InvariantCulture);

        /// read_container(data): [(entry name, bytecode)].
        public static List<KeyValuePair<string, byte[]>> ReadContainer(byte[] data)
        {
            UxBin.Require(data.Length >= 8, "shader container truncated");
            int n = (int)UxBin.U32LE(data, 4), p = 8;
            var o = new List<KeyValuePair<string, byte[]>>(n);
            for (int i = 0; i < n; i++)
            {
                UxBin.Require(p + 8 <= data.Length, "shader container truncated");
                int nl = (int)UxBin.U32LE(data, p + 4); p += 8;
                UxBin.Require(nl >= 0 && p + nl + 4 <= data.Length, "shader container truncated");
                string name = UxBin.CStr(data, p, nl); p += nl;
                int sz = (int)UxBin.U32LE(data, p); p += 4;
                o.Add(new KeyValuePair<string, byte[]>(name, UxBin.PySlice(data, p, p + sz))); p += sz;
            }
            return o;
        }

        /// ctab(code): the constant table of the CTAB comment.
        public static List<Const> Ctab(byte[] code)
        {
            int nt = code.Length / 4; int i = 1;
            while (i < nt)
            {
                uint t = UxBin.U32LE(code, 4 * i);
                if ((t & 0xFFFF) == 0xFFFE)
                {
                    int n = (int)((t >> 16) & 0x7FFF);
                    int bs = (i + 1) * 4, be = Math.Min(code.Length, (i + 1 + n) * 4);
                    if (be - bs >= 4 && code[bs] == 'C' && code[bs + 1] == 'T' && code[bs + 2] == 'A' && code[bs + 3] == 'B')
                    {
                        int b = bs + 4;
                        UxBin.Require(b + 28 <= be, "CTAB truncated");
                        int nconst = (int)UxBin.U32LE(code, b + 12), cinfo = (int)UxBin.U32LE(code, b + 16);
                        var res = new List<Const>(nconst);
                        for (int c = 0; c < nconst; c++)
                        {
                            int q = b + cinfo + 20 * c;
                            UxBin.Require(q + 20 <= code.Length, "CTAB constant outside the shader");
                            int nm = (int)UxBin.U32LE(code, q);
                            res.Add(new Const { name = b + nm >= be ? "" : UxBin.CStr(code, b + nm, be - (b + nm)), regset = Regset(UxBin.U16LE(code, q + 4)), index = UxBin.U16LE(code, q + 6), count = UxBin.U16LE(code, q + 8) });
                        }
                        return res;
                    }
                    i += 1 + n;
                    continue;
                }
                i++;
            }
            return new List<Const>();
        }

        /// decls(code): the dcl tokens (inputs for a vertex shader, samplers for a pixel shader).
        public static List<Decl> Decls(byte[] code)
        {
            int nt = code.Length / 4; var o = new List<Decl>(); int i = 1;
            while (i < nt)
            {
                uint t = UxBin.U32LE(code, 4 * i); uint op = t & 0xFFFF;
                if (op == 0xFFFE) { i += 1 + (int)((t >> 16) & 0x7FFF); continue; }
                if (op == 0xFFFF) break;
                int ln = (int)((t >> 24) & 0x0F);
                if (op == 0x1F)
                {
                    UxBin.Require(i + 2 < nt, "dcl token truncated");
                    uint d = UxBin.U32LE(code, 4 * (i + 1)), r = UxBin.U32LE(code, 4 * (i + 2));
                    o.Add(new Decl { usage = Usage(d & 0x1F), index = (int)((d >> 16) & 0xF), rtype = (int)(((r >> 28) & 7) | ((r >> 8) & 0x18)), reg = (int)(r & 0x7FF), extra = (int)((d >> 27) & 0xF) });
                }
                i += 1 + ln;
            }
            return o;
        }

        /// summarize(data): per entry point: kind ('vs' / 'ps' / the version word in hex), ctab, decls.
        public static List<Entry> Summarize(byte[] data)
        {
            var res = new List<Entry>();
            foreach (var kv in ReadContainer(data))
            {
                uint ver = kv.Value.Length >= 4 ? UxBin.U32LE(kv.Value, 0) : 0;
                string kind = ver >> 16 == 0xFFFE ? "vs" : ver >> 16 == 0xFFFF ? "ps" : "0x" + ver.ToString("x");
                res.Add(new Entry { entry = kv.Key, kind = kind, ctab = Ctab(kv.Value), decls = Decls(kv.Value), code = kv.Value });
            }
            return res;
        }
    }
}
