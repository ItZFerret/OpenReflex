// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_tuff.py (DESIGN 23.2): stock Reflex tuff blocks
// instead of Untamed's hay bales (ux_build_track --bales tuff, the default of the sx builds).
//   UxTuff.Swap(items, lv, collision, wraps, log)   rewrites, in memory after S7, every PhysicsRSData / PopUpBaleRSData /
//                                                   PopDownBaleRSData on one of Untamed's 5 bale shapes -> (record, need)
//   UxTuff.Assets(stock, need, paths)               {(name, type): (bytes, source, origin)} for the keys of need: stock assets through
//                                                   UxStock.Get (ux_stocklib, md5 pinned), HB_<m>_<wrap> materials no stock level
//                                                   holds generated from SX_FortDodge's HB_<m>_DG (GenerateMaterial)
//   UxTuff.SponsorMap / UntamedSponsor / MaterialName / GenerateMaterial / MaterialTexture
// Everything of the Python module is ported (collision stock | untamed, wraps rule | fortdodge). Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_tuff.py.
    public static class UxTuff
    {
        public const string SCHEMA = "ux_tuff/1";
        public static readonly string[] PHYS = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData" };
        /// SHAPES[shape]: the stock model, its kind-15 collision and the material family.
        public sealed class Shape
        {
            public readonly string name, model, collis, mat;
            public Shape(string name, string model, string collis, string mat) { this.name = name; this.model = model; this.collis = collis; this.mat = mat; }
        }
        public static readonly Shape[] SHAPES =
        {
            new Shape("HayBale_Single", "GL_HayBale_Single", "GL_HayBale_Single_COL", "Single"),
            new Shape("HayBale_Double", "GL_HayBale_Double", "GL_HayBale_Double_COL", "Double"),
            new Shape("Haybale_T1", "GL_Haybale_T1", "GL_Haybale_T1_COL", "Double"),
            new Shape("HayBale_Wedge", "GL_HayBale_Wedge", "GL_HayBale_Wedge_COL", "Wedge"),
            new Shape("HayBale_Corner", "GL_HayBale_Corner", "GL_HayBale_Corner_COL", "Corner"),
        };
        public static Shape ShapeOf(string name) { foreach (var s in SHAPES) if (s.name == name) return s; return null; }
        /// NORMAL_MAP[mat] / SHADER[mat].
        public static string NormalMap(string mat) => "GL_HB_" + mat + "_FNL";
        public static string Shader(string mat) => mat == "Single" ? "Lambert_Haybale_Test" : "Template_Trackside_Haybale_FNL";
        static readonly double[] CORNER_FIX = { 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1 };
        public static readonly string[] POOL =
        {
            "DG", "Answer", "Cernics", "Hiper", "Shift", "Sidi", "FMF", "KTM", "TroyLeeDesigns", "Yoshimura", "Bell", "Oakley", "FLY", "PitPassRadio",
            "ProCircuit", "ScaryFast", "AlpineStars", "EVS", "Kenda", "MSR", "PartsUnlimited", "RaceTech", "SixSixOne", "Spy",
            "4PR", "Acerbis", "Alba", "BigGun", "BlingStar", "Blur", "Devol", "DuncanRacing", "DWT", "Excel", "FLU", "FactoryConnection", "FactoryEffex",
            "Fox", "Gaerne", "HydroDynamics", "IMSRoll", "Journeys", "KnK", "LSR", "Laegers", "Leatt", "Maxxis", "OneIndustries", "Pep", "ProAction",
            "ProArmor", "RKRacing", "Renthal", "Sano", "Scott", "Sedona", "SmithOptics", "TagMetals", "Thor",
        };
        public const int N_SX = 24;
        /// ux_stocklib.FD_WRAPS: SX_FortDodge's own bale wraps.
        public static readonly string[] FD_WRAPS = UxStock.FD_WRAPS;
        public const string GEN_TEMPLATE_WRAP = "DG";
        static string Alias(string norm) => norm == "smith" ? "SmithOptics" : norm == "rkchains" ? "RKRacing" : null;

        /// norm(s): lower case, only a-z and 0-9.
        public static string Norm(string s)
        {
            var sb = new StringBuilder();
            foreach (char c in UxCommon.PyLower(s ?? "")) if (c >= 'a' && c <= 'z' || c >= '0' && c <= '9') sb.Append(c);
            return sb.ToString();
        }
        static readonly Regex s_sponsor = new Regex(@"^(?:UX_)?HayBale_(?:Single|Double|Wedge|Corner|T1)_(.+)$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        /// untamed_sponsor(mat): the sponsor part of an Untamed bale material name ('' when it is not one).
        public static string UntamedSponsor(string mat)
        {
            var m = s_sponsor.Match(mat ?? "");
            return m.Success && m.Index == 0 ? m.Groups[1].Value : "";
        }

        static int Md5Mod(string text, int n)
        {
            byte[] h; using (var md5 = MD5.Create()) h = md5.ComputeHash(Encoding.UTF8.GetBytes(text));
            var le = new byte[h.Length + 1];
            for (int i = 0; i < h.Length; i++) le[i] = h[h.Length - 1 - i];          // big-endian digest -> a positive little-endian integer
            return (int)(new BigInteger(le) % n);
        }

        /// sponsor_map(level_short, sponsors, wraps): {Untamed sponsor: (stock wrap, rule)} for one level, in the order assigned.
        public static UxOrdered<string, (string wrap, string rule)> SponsorMap(string levelShort, IEnumerable<string> sponsors, string wraps = "rule")
        {
            var pools = new List<string[]>();
            if (wraps == "rule") { pools.Add(Slice(POOL, 0, N_SX)); pools.Add(Slice(POOL, N_SX, POOL.Length)); } else pools.Add(FD_WRAPS);
            var allw = new List<string>(); foreach (var pl in pools) allw.AddRange(pl);
            var byNorm = new Dictionary<string, string>(StringComparer.Ordinal); foreach (string p in allw) byNorm[Norm(p)] = p;
            var o = new UxOrdered<string, (string, string)>(StringComparer.Ordinal); var used = new HashSet<string>(StringComparer.Ordinal);
            var sorted = new List<string>(new HashSet<string>(sponsors, StringComparer.Ordinal));
            sorted.Sort(string.CompareOrdinal);
            foreach (string s in sorted)
            {
                string n = Norm(s);
                byNorm.TryGetValue(n, out string same);
                string p = !string.IsNullOrEmpty(same) ? same : (Alias(n) != null && allw.Contains(Alias(n)) ? Alias(n) : null);
                if (!string.IsNullOrEmpty(p) && !used.Contains(p)) { o[s] = (p, !string.IsNullOrEmpty(same) ? "same brand" : "alias"); used.Add(p); }
            }
            foreach (string s in sorted)
            {
                if (o.Has(s)) continue;
                bool done = false;
                foreach (var pool in pools)
                {
                    int k = Md5Mod(levelShort + "/" + s, pool.Length);
                    string p = null;
                    for (int d = 0; d < pool.Length; d++) if (!used.Contains(pool[(k + d) % pool.Length])) { p = pool[(k + d) % pool.Length]; break; }
                    if (!string.IsNullOrEmpty(p)) { o[s] = (p, "pool"); used.Add(p); done = true; break; }
                }
                if (!done)
                {   // fortdodge: every FortDodge wrap taken -> reuse the offset one
                    var pool = pools[pools.Count - 1];
                    o[s] = (pool[Md5Mod(levelShort + "/" + s, pool.Length)], "pool (reused)");
                }
            }
            return o;
        }
        static string[] Slice(string[] a, int from, int to) { var r = new string[to - from]; Array.Copy(a, from, r, 0, r.Length); return r; }

        /// material_name(shape, wrap).
        public static string MaterialName(string shape, string wrap) => "HB_" + ShapeOf(shape).mat + "_" + wrap;

        static List<string> TextureNames(UxBxml b)
        {
            var o = new List<string>();
            foreach (var e in b.ToTree().Iter()) if (e.tag == "Texture") o.Add(e.Attr("textureName") as string);
            return o;
        }
        /// generate_material(mat, wrap): HB_<mat>_<wrap> from SX_FortDodge's HB_<mat>_DG, only its textureName 'SP_DG' replaced.
        public static byte[] GenerateMaterial(UxStock stock, string mat, string wrap)
        {
            byte[] t = stock.Get("HB_" + mat + "_" + GEN_TEMPLATE_WRAP, "material");
            if (t == null) throw new UxException("stock-lib has no generation template HB_" + mat + "_" + GEN_TEMPLATE_WRAP);
            var B = new UxBxml(t);
            var texs = TextureNames(B);
            string old = "SP_" + GEN_TEMPLATE_WRAP;
            int count = 0; foreach (string s in B.strings) if (s == old) count++;
            if (texs.Count == 0 || texs[0] != old || count != 1)
                throw new UxException("generation template HB_" + mat + "_DG: first texture " + (texs.Count > 0 ? texs[0] : "none") + ", " + count + " string(s) " + old);
            B.ReplaceStrings(new Dictionary<string, string>(StringComparer.Ordinal) { { old, "SP_" + wrap } });
            return B.ToBytes();
        }
        /// material_texture(data): (first textureName, all textureNames) of a material.
        public static (string first, List<string> all) MaterialTexture(byte[] data)
        {
            var texs = TextureNames(new UxBxml(data));
            return (texs.Count > 0 ? texs[0] : null, texs);
        }

        static JObj AssetRef(string asset, string type) => new JObj().Set("asset", asset).Set("type", type);
        static string AssetOf(object v) => (v as JObj)?.Get("asset") as string;

        /// swap(items, lv, collision, wraps, log): rewrites the bale entities of items [(subscene, Entity Elem)] in place. Returns the
        /// record and need: the ordered asset keys the swapped entities and their materials use.
        public static (JObj record, List<(string name, string type)> need) Swap(IList<KeyValuePair<string, UxElem>> items, UxLevelRow lv, string collision = "stock",
            string wraps = "rule", Action<string> log = null)
        {
            var bales = new List<(UxElem e, UxEntities.PropMap P, string shape, string sponsor)>();
            foreach (var it in items)
            {
                var e = it.Value;
                if (Array.IndexOf(PHYS, UxPcScene.Ns(e)) < 0) continue;
                var P = UxEntities.PropsOf(e);
                string m = P.Has("ModelAsset") ? AssetOf(UxEntities.GetValue(P["ModelAsset"])) : null;
                string bs = m != null && m.StartsWith("UX_", StringComparison.Ordinal) ? m.Substring(3) : m;
                if (bs == null || ShapeOf(bs) == null) continue;
                var ml = P.Has("MaterialList") ? UxEntities.GetValue(P["MaterialList"]) as List<object> : null;
                bales.Add((e, P, bs, UntamedSponsor(ml != null && ml.Count > 0 ? AssetOf(ml[0]) : null)));
            }
            var sponsors = new List<string>(); foreach (var b in bales) sponsors.Add(b.sponsor);
            var smap = SponsorMap(lv.shortName, sponsors, wraps);
            var need = new UxOrdered<(string, string), int>();
            var ents = new List<object>();
            var counts = new SortedDictionary<string, long>(StringComparer.Ordinal);
            foreach (var (e, P, shape, sp) in bales)
            {
                var inf = ShapeOf(shape);
                string wrap = smap[sp].wrap;
                string mat = MaterialName(shape, wrap);
                string ns = UxPcScene.Ns(e), ename = UxEntities.GetValue(P["Name"]) as string;
                var old = new JObj();
                foreach (string k in new[] { "ModelAsset", "MaterialList", "FemaleCollisionAsset" }) if (P.Has(k)) old.Set(k, UxEntities.GetValue(P[k]));
                UxEntities.SetValue(P["ModelAsset"], AssetRef(inf.model, "model"));
                if (!P.Has("SurfaceAsset")) throw new UxException(ns + " " + ename + " has no SurfaceAsset prop");
                UxEntities.SetValue(P["SurfaceAsset"], AssetRef(inf.model, "surface"));
                if (collision == "stock")
                {
                    foreach (string k in new[] { "FemaleCollisionAsset", "MaleCollisionAsset" })
                    {
                        if (!P.Has(k)) throw new UxException(ns + " " + ename + " has no " + k);
                        UxEntities.SetValue(P[k], AssetRef(inf.collis, "collis"));
                    }
                    need[(inf.collis, "collis")] = 1;
                }
                UxEntities.SetValue(P["MaterialList"], new List<object> { AssetRef(mat, "material") });
                if (shape == "HayBale_Corner")
                {   // Mn = CORNER_FIX @ M (float64, the reference's dgemm); the other shapes multiply by the identity: unchanged
                    var ml = UxEntities.GetValue(P["Matrix"]) as List<object>;
                    var M = new double[16]; for (int i = 0; i < 16; i++) M[i] = (double)ml[i];
                    var Mn = UxNp.MatMul(CORNER_FIX, 4, 4, M, 4);
                    bool equal = true; for (int i = 0; i < 16 && equal; i++) equal = Mn[i] == M[i];
                    if (!equal) UxEntities.SetValue(P["Matrix"], Mn);
                }
                foreach (var k in new[] { (inf.model, "model"), (inf.model, "surface"), (mat, "material"), (NormalMap(inf.mat), "texture"), (Shader(inf.mat), "shader"), ("SP_" + wrap, "texture") })
                    need[k] = 1;
                long eid = UxEntities.Py.Int(UxEntities.GetValue(P["Id"]));
                var oldMl = old.Get("MaterialList") as List<object>;
                ents.Add(new JObj().Set("id", eid).Set("untamed_id", eid - UxEntities.ID_OFFSET).Set("type", ns).Set("name", ename).Set("shape", shape).Set("sponsor", sp)
                    .Set("wrap", wrap).Set("material", mat).Set("model", inf.model)
                    .Set("collis", collision == "stock" ? inf.collis : AssetOf(old.Get("FemaleCollisionAsset")))
                    .Set("old_model", AssetOf(old.Get("ModelAsset"))).Set("old_material", oldMl != null && oldMl.Count > 0 ? AssetOf(oldMl[0]) : null)
                    .Set("corner_fix", shape == "HayBale_Corner"));
                string ck = ns + "/" + shape;
                counts[ck] = (counts.TryGetValue(ck, out long c) ? c : 0) + 1;
            }
            var wm = new JObj(); foreach (var kv in smap) wm.Set(kv.Key, new JObj().Set("wrap", kv.Value.wrap).Set("rule", kv.Value.rule));
            var bts = new JObj(); foreach (var kv in counts) bts.Set(kv.Key, kv.Value);
            var shapes = new JObj();
            foreach (var s in SHAPES) shapes.Set(s.name, new JObj().Set("model", s.model).Set("collis", s.collis).Set("mat", s.mat).Set("normal_map", NormalMap(s.mat)).Set("shader", Shader(s.mat)));
            var rec = new JObj().Set("schema", SCHEMA).Set("option", "tuff").Set("collision", collision).Set("wraps", wraps).Set("wrap_map", wm).Set("by_type_shape", bts)
                .Set("bodies", (long)ents.Count).Set("shapes", shapes).Set("entities", ents);
            if (log != null)
            {
                var w = new List<string>(); foreach (var kv in smap) w.Add(kv.Key + ">" + kv.Value.wrap);
                log("tuff blocks (" + collision + " collision, " + wraps + " wraps): " + ents.Count + " bodies; wraps " + string.Join(", ", w));
            }
            return (rec, new List<(string, string)>(need.Keys));
        }

        static readonly Regex s_hb = new Regex(@"^HB_(Single|Double|Wedge|Corner)_(.+)$", RegexOptions.CultureInvariant);
        /// assets(need): {(name, type): (bytes, source, origin)} for the keys of Swap's need list.
        public static UxAssetMap Assets(UxStock stock, IEnumerable<(string name, string type)> need, UxPaths paths = null)
        {
            paths = paths ?? new UxPaths();
            var o = new UxAssetMap();
            foreach (var key in need)
            {
                byte[] b = stock.Get(key.name, key.type);
                if (b != null)
                {
                    o.Set(key.name, key.type, b, paths.StockLib(key.name, key.type), "stock " + UxStock.Record(key.name, key.type).source + " (templates/stock-lib, md5 pinned)");
                    continue;
                }
                if (key.type == "material")
                {
                    var m = s_hb.Match(key.name);
                    if (!m.Success) throw new UxException("no stock-lib asset and no generation rule for " + key.name + "." + key.type);
                    b = GenerateMaterial(stock, m.Groups[1].Value, m.Groups[2].Value);
                    o.Set(key.name, key.type, b, "generated", "ux_tuff.generate_material: SX_FortDodge HB_" + m.Groups[1].Value + "_DG with textureName SP_" + m.Groups[2].Value);
                    continue;
                }
                throw new UxException("stock-lib has no " + key.name + "." + key.type + " (run ux_stocklib.py --fetch; the sponsor rule needs it)");
            }
            return o;
        }
    }
}
