// tests/untamed_convert/foundation: the shared readers against the digests ref_readers.py wrote with the Python modules
// (<ref>/readers/<Level>.json, common.json). The byte encodings hashed here are the ones ref_readers.py hashes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static partial class Program
{
    // ---------------------------------------------------------------- digest helpers (ref_readers.py md5 / arr / val)
    static readonly byte[] s_none = Encoding.ASCII.GetBytes("<none>");
    static string Md5(params byte[][] parts)
    {
        using (var h = IncrementalHash.CreateHash(HashAlgorithmName.MD5))
        {
            foreach (var p in parts) h.AppendData(p ?? s_none);
            return UxBin.Hex(h.GetHashAndReset());
        }
    }
    static byte[] I4(int[] a) { if (a == null) return null; var b = new byte[a.Length * 4]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static byte[] U4(uint[] a) { if (a == null) return null; var b = new byte[a.Length * 4]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static byte[] I8(long[] a) { if (a == null) return null; var b = new byte[a.Length * 8]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static byte[] F4(float[] a) { if (a == null) return null; var b = new byte[a.Length * 4]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static byte[] F8(double[] a) { if (a == null) return null; var b = new byte[a.Length * 8]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static byte[] U1(bool[] a) { var b = new byte[a.Length]; for (int i = 0; i < a.Length; i++) b[i] = a[i] ? (byte)1 : (byte)0; return b; }
    static byte[] Pack(params uint[] v) => U4(v);
    static byte[] PackQ(params long[] v) => I8(v);
    static byte[] Val(float[] f, int comps) => f == null ? new[] { (byte)'N' } : UxBin.Concat(new[] { (byte)'F', (byte)comps }, F4(f));
    static byte[] Val(UxVal v) => v == null ? new[] { (byte)'N' } : v.u8 != null ? UxBin.Concat(new[] { (byte)'U', (byte)v.comps }, v.u8) : UxBin.Concat(new[] { (byte)'F', (byte)v.comps }, F4(v.f));
    static bool IsErr(object o) => o is string s && s.StartsWith("ERR", StringComparison.Ordinal);
    static List<object> L(params object[] v) => new List<object>(v);
    static List<object> Ints(IEnumerable<uint> v) { var l = new List<object>(); foreach (uint x in v) l.Add((long)x); return l; }
    static List<object> Ints(IEnumerable<int> v) { var l = new List<object>(); foreach (int x in v) l.Add((long)x); return l; }
    static List<object> Dbls(IEnumerable<double> v) { var l = new List<object>(); foreach (double x in v) l.Add(x); return l; }
    static List<object> Strs(IEnumerable<string> v) { var l = new List<object>(); foreach (string x in v) l.Add(x); return l; }

    /// got (a digest computed here, or "ERR") against the reference value (a digest, "ERR:<Python type>", or null).
    static bool Same(object want, Func<string> make, out string got)
    {
        try { got = make(); } catch (Exception e) when (e is UxException || e is IndexOutOfRangeException || e is ArgumentException || e is OverflowException) { got = "ERR:" + e.Message; }
        if (want == null) return got == null;
        if (IsErr(want)) return got != null && got.StartsWith("ERR", StringComparison.Ordinal);
        return (string)want == got;
    }

    sealed class Tally
    {
        public string name; public int n, bad; readonly List<string> first = new List<string>(); readonly object gate = new object();
        public Tally(string name) { this.name = name; }
        public void Add(bool ok, string what) { lock (gate) { n++; if (!ok) { bad++; if (first.Count < 6) first.Add(what); } } }
        public void Report(string level) { foreach (var f in first) UxTest.Log("     " + level + " " + name + ": " + f); UxTest.Report(bad == 0, level + ": " + name + " " + (n - bad) + " of " + n + " equal the Python digests"); }
    }

    // ================================================================================================ readers
    static void Readers(string source, string reflex, List<UxLevelRow> levels)
    {
        UxTest.Section("readers against the Python modules (" + source + ")");
        string dir = Path.Combine(s_ref, "readers");
        if (File.Exists(Path.Combine(s_ref, "pil.json"))) Pil(UxJson.Obj(UxJson.Parse(File.ReadAllText(Path.Combine(s_ref, "pil.json"))))); else UxTest.Skip("Pillow parity: no " + s_ref + "/pil.json (run ref_pil.py)");
        if (File.Exists(Path.Combine(dir, "common.json"))) Common(UxJson.Obj(UxJson.Parse(File.ReadAllText(Path.Combine(dir, "common.json")))));
        else UxTest.Skip("readers: no " + dir + "/common.json (run ref_readers.py)");
        using (var disc = UxDisc.Open(source))
            foreach (var lv in levels)
            {
                string rf = Path.Combine(dir, lv.disc + ".json");
                if (!File.Exists(rf)) { UxTest.Skip("readers: no " + rf + " (run ref_readers.py)"); continue; }
                var r = UxJson.Obj(UxJson.Parse(File.ReadAllText(rf)));
                var level = disc.Level(lv);
                LevelReaders(level, r);
                level.ClearCache();
            }
    }

    static void LevelReaders(UxDiscLevel level, JObj r)
    {
        string name = level.name;
        // ---- surfaces
        var ts = new Tally("surface groups (decode, strips, elements)"); var tv = new Tally("reflex_vertices / crowd vertices"); var tstrip = new Tally("stripify / canon");
        Parallel.ForEach(UxJson.Obj(r.Get("surface")).items, kv =>
        {
            byte[] data = level.ReadExtractFile(kv.Key);
            if (IsErr(kv.Value)) { bool threw = false; try { UxMesh.ReadSurface(data); } catch (UxException) { threw = true; } ts.Add(threw, kv.Key + ": Python raised, C# did not"); return; }
            List<UxGroup> groups;
            try { groups = UxMesh.ReadSurface(data); } catch (UxException e) { ts.Add(false, kv.Key + ": " + e.Message); return; }
            var want = UxJson.Arr(kv.Value);
            if (groups.Count != want.Count) { ts.Add(false, kv.Key + ": " + groups.Count + " groups, want " + want.Count); return; }
            for (int gi = 0; gi < groups.Count; gi++)
            {
                var g = groups[gi]; var w = UxJson.Obj(want[gi]);
                var declBytes = new List<byte[]>(); foreach (var e in g.decl) declBytes.Add(Pack((uint)e.stream, (uint)e.offset, e.type, (uint)e.method, (uint)e.usage, (uint)e.usageIndex));
                string head = Md5(Pack(g.flags, (uint)g.prim, (uint)g.nverts, (uint)g.npal, (uint)g.decl.Count), UxBin.Concat(declBytes.ToArray()), I4(g.strides), g.palette, I4(g.strip), I4(g.tris));
                string elems = Md5(Val(g.pos, 3), Val(g.pos_w, 1), Val(g.nrm, 3), Val(g.uv0), Val(g.uv1), Val(g.col), Val(g.tan, 3), Val(g.bidx), Val(g.bwgt));
                ts.Add(head == (string)w.Get("head") && elems == (string)w.Get("elems") && g.TriCount == UxJson.Int(w.Get("ntris")), kv.Key + " group " + gi + (head != (string)w.Get("head") ? ": header / strip / tris" : ": vertex elements") + " differ");
                tv.Add(Same(w.Get("rv"), () => Md5(UxMesh.ReflexVertices(g)), out string g1), kv.Key + " group " + gi + ": reflex_vertices " + g1 + " want " + w.Get("rv"));
                if (w.Has("rvo"))
                {
                    float[] lo = { float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity }, hi = { float.NegativeInfinity, float.NegativeInfinity, float.NegativeInfinity };
                    for (int i = 0; i < g.pos.Length; i++) { int c = i % 3; if (g.pos[i] < lo[c]) lo[c] = g.pos[i]; if (g.pos[i] > hi[c]) hi[c] = g.pos[i]; }
                    double ox = (lo[0] + hi[0]) / 2f, oy = (lo[1] + hi[1]) / 2f, oz = (lo[2] + hi[2]) / 2f;
                    tv.Add(Same(w.Get("rvo"), () => Md5(UxMesh.ReflexVertices(g, ox, oy, oz)), out string g2), kv.Key + " group " + gi + ": reflex_vertices(origin) " + g2 + " want " + w.Get("rvo"));
                }
                if (w.Has("crowd")) tv.Add(Same(w.Get("crowd"), () => Md5(UxMesh.ReflexCrowdVertices(g)), out string g3), kv.Key + " group " + gi + ": reflex_crowd_vertices " + g3 + " want " + w.Get("crowd"));
                if (w.Has("strip2"))
                {
                    int[] s2 = UxRmesh.Stripify(g.tris);
                    bool back = UxBin.Same(I4(UxRmesh.Canon(UxRmesh.StripToTris(s2))), I4(UxRmesh.Canon(g.tris)));
                    tstrip.Add(Md5(I4(s2)) == (string)w.Get("strip2") && Md5(I4(UxRmesh.Canon(g.tris))) == (string)w.Get("canon") && back == UxJson.Bool(w.Get("strip2_ok")), kv.Key + " group " + gi + ": stripify / canon differ (" + g.TriCount + " triangles)");
                }
            }
        });
        ts.Report(name); tv.Report(name); tstrip.Report(name);

        // ---- models
        var tm = new Tally("models (read_model, model_to_reflex, read_model_rigid)");
        var surfByName = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var kv in UxJson.Obj(r.Get("surface")).items) { string stem = kv.Key.Substring(kv.Key.LastIndexOf('/') + 1); stem = stem.Substring(0, stem.LastIndexOf('.')).ToLowerInvariant(); surfByName[stem] = kv.Key; }
        foreach (var kv in UxJson.Obj(r.Get("model")).items)
        {
            byte[] data = level.ReadExtractFile(kv.Key);
            if (IsErr(kv.Value)) { bool threw = false; try { UxMesh.ReadModel(data); } catch (UxException) { threw = true; } tm.Add(threw, kv.Key + ": Python raised, C# did not"); continue; }
            var d = new JObj();
            try
            {
                var m = UxMesh.ReadModel(data);
                d.Set("skinned", m.skinned).Set("boned", m.boned).Set("name", m.name).Set("ngroups", (long)m.ngroups).Set("hdr", Ints(m.hdr));
                if (!m.skinned)
                {
                    d.Set("draw_distance", (long)m.draw_distance).Set("name2", m.name2).Set("lods", m.lods.Select(x => (object)Ints(x)).ToList()).Set("recs", m.recs.Select(x => (object)Ints(x)).ToList())
                        .Set("flags", UxBin.Hex(m.flags)).Set("order", Ints(m.order)).Set("materials", Strs(m.materials)).Set("tail", (long)m.tail.Length)
                        .Set("num", Md5(F4(m.matrix), F4(new[] { m.radius }), F4(m.center), F4(m.extents)));
                    Same("x", () => Md5(UxMesh.ModelToReflex(data)), out string m2r);
                    d.Set("m2r", m2r.StartsWith("ERR") ? (IsErr(UxJson.Obj(kv.Value).Get("m2r")) ? UxJson.Obj(kv.Value).Get("m2r") : m2r) : m2r);
                }
                else d.Set("nbones", (long)m.nbones);
                if (m.boned)
                {
                    object wr = UxJson.Obj(kv.Value).Get("rigid");
                    try
                    {
                        var rg = UxMesh.ReadModelRigid(data);
                        var rd = new JObj().Set("names", Strs(rg.node_names)).Set("words", Ints(rg.node_words)).Set("name2", rg.name2).Set("materials", Strs(rg.materials))
                            .Set("lods", rg.lods.Select(x => (object)Ints(x)).ToList()).Set("recs", rg.recs.Select(x => (object)Ints(x)).ToList()).Set("order", Ints(rg.order))
                            .Set("num", Md5(UxBin.Concat(rg.node_matrices.Select(F4).ToArray()), F4(new[] { rg.radius }), F4(rg.center), F4(rg.extents)));
                        string stem = kv.Key.Substring(kv.Key.LastIndexOf('/') + 1); stem = stem.Substring(0, stem.LastIndexOf('.')).ToLowerInvariant();
                        if (surfByName.TryGetValue(stem, out string sf)) rd.Set("problems", Strs(UxMesh.RigidStaticProblems(rg, UxMesh.ReadSurface(level.ReadExtractFile(sf)))));
                        d.Set("rigid", rd);
                    }
                    catch (UxException) { d.Set("rigid", IsErr(wr) ? wr : "ERR"); }
                }
            }
            catch (UxException e) { tm.Add(false, kv.Key + ": " + e.Message); continue; }
            var diff = UxTest.JsonDifferences(d, kv.Value);
            tm.Add(diff.Count == 0, kv.Key + ": " + (diff.Count > 0 ? diff[0] : ""));
        }
        tm.Report(name);

        // ---- collis
        var tc = new Tally("collis (read_collis, primitives, triangles, clusters, bounds)");
        Parallel.ForEach(UxJson.Obj(r.Get("collis")).items, kv =>
        {
            byte[] data = level.ReadExtractFile(kv.Key); var w = UxJson.Obj(kv.Value);
            if (w.Has("read")) { bool threw = false; try { UxMesh.ReadCollis(data); } catch (UxException) { threw = true; } tc.Add(threw, kv.Key + ": Python raised, C# did not"); return; }
            var d = new JObj();
            try
            {
                var c = UxMesh.ReadCollis(data);
                d.Set("kind", (long)c.kind).Set("version", (long)c.version).Set("size", (long)c.size).Set("tail", c.kind == 19 || c.kind != 0 && c.kind != 2 && c.kind != 11 && c.kind != 15 && c.kind != 17 ? null : (object)(long)c.tail);
                byte[] NodesRaw()
                {
                    var b = new byte[c.nodes.Length * 32];
                    for (int i = 0; i < c.nodes.Length; i++)
                    {
                        var n = c.nodes[i]; int q = 32 * i;
                        UxBin.PutF32(b, q, n.vol, true); UxBin.PutF32(b, q + 4, n.cx, true); UxBin.PutF32(b, q + 8, n.cy, true); UxBin.PutF32(b, q + 12, n.cz, true);
                        UxBin.PutF32(b, q + 16, n.hx, true); UxBin.PutF32(b, q + 20, n.hy, true); UxBin.PutF32(b, q + 24, n.hz, true); UxBin.PutU16(b, q + 28, n.k0, true); UxBin.PutU16(b, q + 30, n.k1, true);
                    }
                    return b;
                }
                if (c.kind == 0)
                {
                    int nt = c.tri_d.Length; var tb = new byte[nt * 22];
                    for (int i = 0; i < nt; i++)
                    {
                        UxBin.PutF32(tb, 22 * i, c.tri_d[i], true);
                        for (int k = 0; k < 3; k++) { UxBin.PutF32(tb, 22 * i + 4 + 4 * k, c.tri_n[3 * i + k], true); UxBin.PutU16(tb, 22 * i + 16 + 2 * k, (ushort)c.tri_i[3 * i + k], true); }
                    }
                    d.Set("num", Md5(F8(c.matrix), F8(c.verts), F8(c.tris), F8(c.local_tris), NodesRaw(), tb, Pack(c.root, c.z1, c.z2)));
                }
                else if (c.kind == 2) { d.Set("num", Md5(F8(c.matrix), F8(c.points), F8(c.point_ref), NodesRaw(), Pack(c.nleaf))); d.Set("swap", Md5(UxMesh.SwapCollisKind2(data))); }
                else if (c.kind == 11) d.Set("num", Md5(F8(c.half), c.pad));
                else if (c.kind == 15)
                {
                    d.Set("tags", UxBin.Hex(c.tags));
                    d.Set("prims", c.prims.Select(p => (object)L((long)p.type, (long)p.m1, Md5(F8(p.matrix), F8(p.@params), p.raw))).ToList());
                }
                else if (c.kind == 17) d.Set("num", Md5(F8(new[] { c.radius, c.half_height })));
                else if (c.kind == 19) d.Set("count", (long)c.count);
                Same("x", () => Md5(UxMesh.CollisToReflexPrimitive(data)), out string prim);
                d.Set("prim", prim.StartsWith("ERR") && IsErr(w.Get("prim")) ? w.Get("prim") : prim);
                double[] t = UxMesh.CollisTriangles(data);
                d.Set("tris", t == null ? null : Md5(F8(t)));
                if (w.Has("clusters"))
                {
                    var cl = UxRmesh.ClusterTriangles(t);
                    d.Set("clusters", Md5(UxRmesh.WriteCollis(cl))).Set("nclusters", (long)cl.Count);
                    var (radius, bc, be) = UxRmesh.Bounds(t);
                    d.Set("bounds", Md5(F8(new[] { radius }), F8(bc), F8(be)));
                }
            }
            catch (UxException e) { tc.Add(false, kv.Key + ": " + e.Message); return; }
            var diff = UxTest.JsonDifferences(d, w);
            tc.Add(diff.Count == 0, kv.Key + ": " + (diff.Count > 0 ? diff[0] : ""));
        });
        tc.Report(name);

        // ---- materials
        var tmat = new Tally("materials (read_material, state tables, roles, template suggestion)");
        foreach (var kv in UxJson.Obj(r.Get("material")).items)
        {
            byte[] data = level.ReadExtractFile(kv.Key);
            string mine = UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.DumpsUx(MaterialDigest(data))));
            if (mine != (string)kv.Value && tmat.bad < 2) File.WriteAllText(Path.Combine(s_scratch, "material_diff_" + Path.GetFileName(kv.Key) + ".json"), UxJson.DumpsUx(MaterialDigest(data)));
            tmat.Add(mine == (string)kv.Value, kv.Key + ": digest differs (see " + s_scratch + "/material_diff_*.json)");
        }
        tmat.Report(name);

        // ---- textures
        var tt = new Tally("textures (layout, to_pc, PcTexture, rgba)"); var tf = new Tally("half-float textures: rgba (tone mapped through powf)");
        Parallel.ForEach(UxJson.Obj(r.Get("texture")).items, kv =>
        {
            byte[] data = level.ReadExtractFile(kv.Key); var w = UxJson.Obj(kv.Value);
            if (w.Has("read")) { bool threw = false; try { new UxTexture(data); } catch (UxException) { threw = true; } tt.Add(threw, kv.Key + ": Python raised, C# did not"); return; }
            var d = new JObj(); bool isFloat = UxJson.Bool(w.Get("float"));
            try
            {
                var t = new UxTexture(data); var s0 = t.surfaces[0];
                d.Set("info", t.Info()).Set("check", t.CheckSize()).Set("name", s0.name).Set("gpufmt", (long)s0.gpufmt);
                var lay = new List<byte[]>();
                foreach (var key in t.level_keys) { var (off, wa, ha, x0, y0, wb, hb) = t.level_loc[key]; lay.Add(PackQ(key.s, key.f, key.m, off, wa, ha, x0, y0, wb, hb)); }
                d.Set("layout", Md5(UxBin.Concat(lay.ToArray()), PackQ(t.data_end)));
                d.Set("float", t.Elem(0).name == "16_16_16_16_EXPAND" || t.Elem(0).name == "16_16_16_16_FLOAT");
                try
                {
                    byte[] pc = t.ToPc();
                    d.Set("pc", Md5(pc)).Set("pc_size", (long)pc.Length);
                    var p = new UxPcTexture(pc);
                    d.Set("pcread", new JObj().Set("nsurf", (long)p.nsurf).Set("nentry", (long)p.nentry).Set("kind", (long)p.kind).Set("flags", (long)p.flags).Set("end", (long)p.end).Set("data_offset", (long)p.data_offset)
                        .Set("recs", p.recs.Select(x => (object)L((long)x.mips, (long)x.w, (long)x.h, UxBin.Hex(x.fmt))).ToList()).Set("table", p.table.Select(x => (object)L((long)x.size, (long)x.pitch)).ToList()));
                }
                catch (UxException) { d.Set("pc", IsErr(w.Get("pc")) ? w.Get("pc") : "ERR"); }
                try { d.Set("pc2", Md5(t.ToPc(null, null, 2))); } catch (UxException) { d.Set("pc2", IsErr(w.Get("pc2")) ? w.Get("pc2") : "ERR"); }
                try
                {
                    var im = t.Rgba(0, 0, 0);
                    string rgba = Md5(im.data), last = Md5(t.Rgba(0, t.faces - 1, Math.Max(1, s0.mips) - 1).data), raw = Md5(t.Rgba(0, 0, 0, false).data);
                    if (isFloat)
                    {   // counted apart: the display tone mapping goes through powf
                        tf.Add(rgba == (string)w.Get("rgba") && last == (string)w.Get("rgba_last") && raw == (string)w.Get("rgba_raw"), kv.Key + ": rgba of a half-float texture differs");
                        d.Set("rgba", w.Get("rgba")).Set("rgba_last", w.Get("rgba_last")).Set("rgba_raw", w.Get("rgba_raw"));
                    }
                    else d.Set("rgba", rgba).Set("rgba_last", last).Set("rgba_raw", raw);
                    d.Set("rgba_shape", L((long)im.h, (long)im.w, (long)im.c));
                }
                catch (UxException) { d.Set("rgba", IsErr(w.Get("rgba")) ? w.Get("rgba") : "ERR"); }
                if (isFloat) d.Set("floatlevel", Md5(F4(t.FloatLevel(0, 0, 0))));
            }
            catch (UxException e) { tt.Add(false, kv.Key + ": " + e.Message); return; }
            var diff = UxTest.JsonDifferences(d, w);
            tt.Add(diff.Count == 0, kv.Key + ": " + (diff.Count > 0 ? diff[0] : ""));
        });
        tt.Report(name); if (tf.n > 0) tf.Report(name);

        // ---- terrain
        var files = UxJson.Arr(r.Get("terrain_files")); var tfiles = UxJson.Arr(files[0]); var cfiles = UxJson.Arr(files[1]);
        var wt = UxJson.Obj(r.Get("terrain")); var td = new JObj();
        UxTerrainMesh T = null; UxTrnCol C = null;
        if (tfiles.Count > 0)
        {
            byte[] data = level.ReadExtractFile((string)tfiles[0]);
            T = UxTerrain.ReadTerrain(data);
            byte[] pc = UxTerrain.TerrainToPc(data); var back = UxTerrain.ReadTerrain(pc);
            td.Set("terrain", new JObj().Set("version", (long)T.version).Set("nmat", (long)T.nmat).Set("nzx", (long)T.nzx).Set("nzz", (long)T.nzz).Set("header_size", (long)T.header_size).Set("size", (long)T.size)
                .Set("head", Md5(F8(T.centre), F8(T.half), U4(T.w30), T.decl_raw, F8(T.zone_centre), F8(T.zone_half), U4(T.ranges)))
                .Set("mats", T.materials.Select(m => (object)Md5(U4(m.indices), F8(m.pos), F8(m.normal), F4(m.uv0), F4(m.uv1), U4(m.color), F8(m.tangent), I4(m.tris), I4(m.zone))).ToList())
                .Set("pc", Md5(pc))
                .Set("pc_back", Md5(back.materials.Select(m => I4(m.tris)).Concat(back.materials.Select(m => F8(m.normal))).ToArray())));
        }
        if (cfiles.Count > 0)
        {
            byte[] data = level.ReadExtractFile((string)cfiles[0]);
            C = UxTerrain.ReadTrncol(data);
            td.Set("trncol", new JObj().Set("version", (long)C.version).Set("grid", L((long)C.gx, (long)C.gz)).Set("cell", C.cell).Set("bmin", Dbls(C.bmin)).Set("bmax", Dbls(C.bmax)).Set("ncells", (long)C.Cells().Count)
                .Set("num", Md5(F8(C.verts), F4(C.vw), F8(C.vnormals), U4(C.vattr), F8(C.planes), I4(C.tris), U4(C.flags), U4(C.lists))).Set("swap", Md5(UxTerrain.TrncolSwap(data))));
        }
        if (T != null && C != null)
        {
            int[] cm = UxTerrain.CollisionMaterials(T, C);
            td.Set("cm", Md5(I4(cm))).Set("cm_unmatched", (long)cm.Count(x => x < 0));
            var hf = UxTerrain.Heightfield(C, null, null, 1024);
            var R = hf.raster;
            td.Set("hf", new JObj().Set("height", Md5(F4(hf.height))).Set("covered", Md5(U1(hf.covered))).Set("ncovered", (long)hf.covered.Count(x => x)).Set("x0", hf.x0).Set("z1", hf.z1)
                .Set("raster", Md5(F4(R.top), F4(R.up_top), F4(R.bottom), F4(R.down_max), F4(R.down_min), I4(R.nup), F4(R.ny), I8(R.tri), I8(R.tri_bottom))));
            var ex = UxTerrain.Heightfield(C, cm, new HashSet<int> { T.nmat - 1, -1 }, 256, 2.0, 10.0, -20.0, 10.0, false);
            td.Set("hf_ex", new JObj().Set("height", Md5(F4(ex.height))).Set("covered", Md5(U1(ex.covered))).Set("excluded", Md5(I4(ex.excluded))).Set("n", (long)ex.excluded.Length));
            var sub = new List<int>(); for (int i = 0; i < C.tris.Length / 3; i += 7) { sub.Add(C.tris[3 * i]); sub.Add(C.tris[3 * i + 1]); sub.Add(C.tris[3 * i + 2]); }
            var R2 = UxTerrain.Rasterize(C.verts, sub.ToArray(), sub.Count / 3, 4.0);
            td.Set("raster_own", new JObj().Set("shape", L((long)R2.H, (long)R2.W)).Set("x0", R2.x0).Set("z1", R2.z1).Set("num", Md5(F4(R2.top), I4(R2.nup), I8(R2.tri))));
        }
        var tdiff = UxTest.JsonDifferences(td, wt);
        foreach (var x in tdiff.Take(8)) UxTest.Log("     " + name + " terrain: " + x);
        UxTest.Report(tdiff.Count == 0, name + ": terrain / trncol (read, to PC, swap, collision_materials, rasterize, heightfield with scipy's nearest fill): " + tdiff.Count + " differences");

        // ---- scene
        var ws = UxJson.Obj(r.Get("scene")); var sd = new JObj();
        var subs = UxXScene.Load(level.ReadFile("scene"));
        sd.Set("subs", subs.Select(s => (object)L(s.name, (long)s.chunks.Count, s.nent, s.offset, (long)s.Entities().Count)).ToList());
        var all = new JObj(); var per = new JObj();
        foreach (var s in subs)
        {
            var list = s.Entities().Select(e => UxXScene.Jsonable(e.ToDict())).ToList();
            all.Set(s.name, list); per.Set(s.name, UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(list, 1))));
        }
        sd.Set("entities", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(all, 1)))).Set("per_sub", per);
        var me = UxXScene.MergedEntities(subs, level.name);
        sd.Set("merged", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(me.Select(x => (object)L(x.id, x.sub, x.entity.type, x.entity.NameValue)).ToList())))).Set("nmerged", (long)me.Count);
        var inv = new JObj(); foreach (var s in subs) inv.Set(s.name, UxXScene.Inventory(s).items.Select(x => (object)L(x.Key, x.Value)).ToList());
        sd.Set("inventory", inv);
        sd.Set("assets", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(subs.SelectMany(s => s.Entities()).Select(e => (object)e.Assets().Select(a => (object)L(a.prop, a.name, a.type)).ToList()).ToList()))));
        sd.Set("xml", Md5(subs.SelectMany(s => s.chunks).Select(b => Encoding.UTF8.GetBytes(b.ToXml())).ToArray()));
        sd.Set("position", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(subs.SelectMany(s => s.Entities()).Select(e => e.Position == null ? null : (object)Dbls(e.Position)).ToList()))));
        var sdiff = UxTest.JsonDifferences(sd, ws);
        foreach (var x in sdiff.Take(8)) UxTest.Log("     " + name + " scene: " + x);
        if (sdiff.Count > 0) File.WriteAllText(Path.Combine(s_scratch, "scene_" + name + ".json"), UxJson.Dumps(all, 1));
        UxTest.Report(sdiff.Count == 0, name + ": scene (" + subs.Count + " sub-scenes, " + subs.Sum(s => s.Entities().Count) + " entities; values, merged, assets, XML): " + sdiff.Count + " differences");
    }

    static object PyVal(object v)
    {
        switch (v)
        {
            case UxBlobRef b: return L("blob", b.offset);
            case double[] a: return Dbls(a);
            default: return v;
        }
    }
    static List<object> Pairs(IEnumerable<UxStatePair> p) => p.Select(x => (object)L((long)x.id, (long)x.value)).ToList();
    static List<object> PcStates(IEnumerable<UxPcState> p) => p.Select(x => (object)L((long)x.id, (long)x.value, x.name)).ToList();

    static JObj MaterialDigest(byte[] data)
    {
        UxMaterial m;
        try { m = UxMat.ReadMaterial(data); } catch (UxException) { return new JObj().Set("read", "ERR"); }
        var o = new JObj().Set("version", m.version); var passes = new List<object>();
        foreach (var P in m.passes)
        {
            var d = new JObj().Set("vs", P.vs).Set("vs_entry", P.vs_entry).Set("ps", P.ps).Set("ps_entry", P.ps_entry)
                .Set("textures", P.textures.Select(t => (object)L(t.Key, Pairs(t.Value))).ToList()).Set("states", Pairs(P.states))
                .Set("params", P.@params == null ? null : P.@params.Select(p => { var a = new JObj(); foreach (var kv in p.attrs.items) a.Set(kv.Key, PyVal(kv.Value)); return (object)L(p.name, a, PyVal(p.value)); }).ToList());
            try { d.Set("pc", PcStates(UxMat.XenonStatesToPc(P.states))); } catch (UxException) { d.Set("pc", "ERR:KeyError"); }
            try { d.Set("spc", P.textures.Select(t => (object)PcStates(UxMat.XenonSamplerToPc(t.Value))).ToList()); } catch (UxException) { d.Set("spc", "ERR:KeyError"); }
            d.Set("named", UxMat.NamedStates(P.states)).Set("blend", UxMat.BlendClass(P.states));
            var tex = P.textures.Select(t => t.Key).ToList();
            d.Set("roles", UxMat.GuessRoles(tex).items.Select(x => (object)L(x.Key, x.Value)).ToList()).Set("role_each", tex.Select(t => (object)UxMat.TextureRole(t)).ToList());
            passes.Add(d);
        }
        o.Set("passes", passes);
        if (m.passes.Count > 0)
        {
            object Sug((string template, List<string> slots) s) => L(s.template, s.slots.Select(x => (object)x).ToList());
            o.Set("suggest", L(Sug(UxMat.SuggestReflexTemplate(m)), Sug(UxMat.SuggestReflexTemplate(m, false)), Sug(UxMat.SuggestReflexTemplate(m, null, true))));
        }
        return o;
    }

    // ================================================================================================ level-independent
    static uint[] LcgU32(int n, uint seed) { uint x = seed; var o = new uint[n]; for (int i = 0; i < n; i++) { x = unchecked(x * 1664525u + 1013904223u); o[i] = x; } return o; }
    static double[] Dvals(int n, uint seed) { var u = LcgU32(n, seed); var o = new double[n]; for (int i = 0; i < n; i++) o[i] = ((double)(u[i] >> 5) - 67108864.0) / 1024.0; return o; }
    static bool BitsEqual(double[] a, List<object> want) { if (a.Length != want.Count) return false; for (int i = 0; i < a.Length; i++) if (BitConverter.DoubleToInt64Bits(a[i]) != BitConverter.DoubleToInt64Bits(UxJson.Num(want[i]))) return false; return true; }

    static void Common(JObj r)
    {
        // ---- the software fused multiply-add against the processor's
        {
            var a = Dvals(300000, 9001); int bad = 0, badf = 0;
            for (int i = 0; i + 2 < a.Length; i += 3)
            {
                double x = a[i] * 1e-3, y = a[i + 1] / 77.0, z = -x * y * (1 + (i % 7 == 0 ? 1e-15 : i % 5 == 0 ? 0.25 : 1e-9));
                if (BitConverter.DoubleToInt64Bits(UxNp.Fma(x, y, z)) != BitConverter.DoubleToInt64Bits(Math.FusedMultiplyAdd(x, y, z))) bad++;
                if (BitConverter.DoubleToInt64Bits(UxNp.Fma(x, y, a[i + 2])) != BitConverter.DoubleToInt64Bits(Math.FusedMultiplyAdd(x, y, a[i + 2]))) bad++;
                float fx = (float)x, fy = (float)y, fz = (float)z;
                if (BitConverter.SingleToInt32Bits(UxNp.Fma(fx, fy, fz)) != BitConverter.SingleToInt32Bits(MathF.FusedMultiplyAdd(fx, fy, fz))) badf++;
                if (BitConverter.SingleToInt32Bits(UxNp.Fma(fx, fy, -(fx * fy))) != BitConverter.SingleToInt32Bits(MathF.FusedMultiplyAdd(fx, fy, -(fx * fy)))) badf++;
            }
            foreach (var (x, y, z) in new[] { (1e300, 1e10, -1e308), (1e-300, 1e-20, 1e-320), (4.9e-324, 0.5, 4.9e-324), (1.5, 1e-200, 1e-320), (3.0, 1e-170, -3e-170), (1e160, 1e160, -1e308), (0.1, 10.0, -1.0), (1e-200, 1e-200, 0.0), (-0.0, 5.0, 0.0), (2.0, 3.0, 0.0) })
                if (BitConverter.DoubleToInt64Bits(UxNp.Fma(x, y, z)) != BitConverter.DoubleToInt64Bits(Math.FusedMultiplyAdd(x, y, z))) { bad++; UxTest.Log("     fma(" + x + ", " + y + ", " + z + ") = " + UxNp.Fma(x, y, z) + ", processor " + Math.FusedMultiplyAdd(x, y, z)); }
            UxTest.Report(bad == 0 && badf == 0, "UxNp.Fma against the processor's fused multiply-add: " + bad + " float64 and " + badf + " float32 results differ (400,000 cases)");
        }
        // ---- BLAS-backed numpy operations
        {
            int bad = 0, n = 0;
            foreach (JObj c in UxJson.Arr(r.Get("blas")).Cast<JObj>())
            {
                string op = (string)c.Get("op"); uint seed = (uint)UxJson.Int(c.Get("seed")); n++;
                bool ok;
                if (op == "matmul" || op == "matmul32")
                {
                    int m = (int)UxJson.Int(c.Get("m")), k = (int)UxJson.Int(c.Get("k")), nn = (int)UxJson.Int(c.Get("n"));
                    var A = Dvals(m * k, seed); var B = Dvals(k * nn, seed + 1).Select(x => x / 4096.0).ToArray();
                    if (op == "matmul") ok = BitsEqual(UxNp.MatMul(A, m, k, B, nn), UxJson.Arr(c.Get("r")));
                    else ok = BitsEqual(UxNp.MatMul(A.Select(x => (float)x).ToArray(), m, k, B.Select(x => (float)x).ToArray(), nn).Select(x => (double)x).ToArray(), UxJson.Arr(c.Get("r")));
                    if (!ok) UxTest.Log("     " + op + " (" + m + "," + k + ")@(" + k + "," + nn + ") differs");
                }
                else if (op == "dot")
                {
                    int k = (int)UxJson.Int(c.Get("n")); var a = Dvals(k, seed); var b = Dvals(k, seed + 1).Select(x => x / 4096.0).ToArray();
                    ok = BitsEqual(new[] { UxNp.Dot(a, b), UxNp.Norm(a) }, L(c.Get("r"), c.Get("norm")));
                    if (!ok) UxTest.Log("     dot / norm of " + k + " elements differs");
                }
                else if (op == "matvec3")
                {
                    int m = (int)UxJson.Int(c.Get("m")); var A = Dvals(m * 3, seed); var x = Dvals(3, seed + 1).Select(v => v / 4096.0).ToArray();
                    ok = BitsEqual(UxNp.MatVec3(A, m, x), UxJson.Arr(c.Get("r")));
                    if (!ok) UxTest.Log("     (" + m + ",3)@vec3 differs");
                }
                else
                {
                    int k = (int)UxJson.Int(c.Get("n")); var y = Dvals(k, seed); var A = Dvals(k * k, seed + 1).Select(v => v / 4096.0).ToArray();
                    ok = BitsEqual(UxNp.VecMat(y, A, k), UxJson.Arr(c.Get("r")));
                    if (!ok) UxTest.Log("     vec" + k + "@(" + k + "," + k + ") differs");
                }
                if (!ok) bad++;
            }
            UxTest.Report(bad == 0, "numpy's BLAS-backed products (A @ B, np.dot, np.linalg.norm, A @ v, v @ A) reproduced bit for bit: " + (n - bad) + " of " + n + " cases");
        }
        // ---- np.sum / np.mean
        if (r.Has("sums"))
        {
            int bad = 0, n = 0;
            foreach (JObj c in UxJson.Arr(r.Get("sums")).Cast<JObj>())
            {
                int k = (int)UxJson.Int(c.Get("n")); uint seed = (uint)UxJson.Int(c.Get("seed"));
                var d = Dvals(k, seed).Select(x => x / 3.0).ToArray(); var f = d.Select(x => (float)x).ToArray();
                var m2 = Dvals(40 * 9, seed + 1).Select(x => x / 7.0).ToArray();
                var rows = new double[40]; for (int i = 0; i < 40; i++) rows[i] = UxNp.Sum(m2, i * 9, 9);
                var cols = new double[9]; for (int i = 0; i < 40; i++) for (int j = 0; j < 9; j++) cols[j] += m2[i * 9 + j];
                bool ok = BitsEqual(new[] { UxNp.Sum(d), UxNp.Sum(d) / k, (double)UxNp.Sum(f), (double)(float)(UxNp.Sum(f) / k), UxNp.Sum(d, 0, (k + 2) / 3, 3) },
                    L(c.Get("sum"), c.Get("mean"), c.Get("sum32"), c.Get("mean32"), c.Get("strided"))) && BitsEqual(rows, UxJson.Arr(c.Get("rows"))) && BitsEqual(cols, UxJson.Arr(c.Get("cols")));
                n++; if (!ok) { bad++; UxTest.Log("     np.sum / np.mean of " + k + " elements differs"); }
            }
            UxTest.Report(bad == 0, "np.sum / np.mean (pairwise summation; float64 and float32; rows, columns, strided): " + (n - bad) + " of " + n + " cases");
        }
        else UxTest.Skip("np.sum reference (run ref_readers.py again)");
        // ---- argsort of a structured array
        {
            int bad = 0, n = 0;
            foreach (JObj c in UxJson.Arr(r.Get("sorts")).Cast<JObj>())
            {
                int k = (int)UxJson.Int(c.Get("n")); long[] ka = new long[k], kb = new long[k], kc = new long[k];
                if (c.Has("kind"))
                {
                    string kind = (string)c.Get("kind");
                    for (int i = 0; i < k; i++) { long v = kind == "asc" ? i : kind == "desc" ? k - 1 - i : i < k / 2 ? i : (k - k / 2) - 1 - (i - k / 2); ka[i] = v / 3; }
                }
                else
                {
                    var u = LcgU32(3 * k, (uint)UxJson.Int(c.Get("seed"))); uint mod = (uint)UxJson.Int(c.Get("mod"));
                    for (int i = 0; i < k; i++) { ka[i] = (u[3 * i] % mod) % 3; kb[i] = (u[3 * i + 1] % mod) % 2; kc[i] = u[3 * i + 2] % mod; }
                }
                int[] o = UxNp.ArgQuickSort(k, (x, y) => ka[x] != ka[y] ? ka[x].CompareTo(ka[y]) : kb[x] != kb[y] ? kb[x].CompareTo(kb[y]) : kc[x].CompareTo(kc[y]));
                n++; if (Md5(I4(o)) != (string)c.Get("md5")) { bad++; UxTest.Log("     argsort of " + k + " structured elements (" + (c.Get("kind") ?? "mod " + c.Get("mod")) + ") differs"); }
            }
            UxTest.Report(bad == 0, "np.argsort of a structured array (numpy's introsort order among equal keys): " + (n - bad) + " of " + n + " cases");
        }
        // ---- scipy.ndimage
        {
            int bad = 0, n = 0;
            foreach (JObj c in UxJson.Arr(r.Get("ndimage")).Cast<JObj>())
            {
                int h = (int)UxJson.Int(c.Get("h")), w = (int)UxJson.Int(c.Get("w")); long thr = UxJson.Int(c.Get("thr"));
                var u = LcgU32(h * w, (uint)UxJson.Int(c.Get("seed"))); var mask = new bool[h * w]; var inv = new bool[h * w];
                for (int i = 0; i < mask.Length; i++) mask[i] = (long)(u[i] >> 24) < thr;
                if (thr == 256) mask[4 * w + 4] = false;
                for (int i = 0; i < mask.Length; i++) inv[i] = !mask[i];
                UxNd.DistanceTransformEdt(mask, h, w, out int[] ir, out int[] ic);
                bool ok = Md5(I4(ir), I4(ic)) == (string)c.Get("ind") && Md5(F8(UxNd.DistanceTransformEdtDistances(mask, h, w))) == (string)c.Get("dist");
                bool okd = Md5(U1(UxNd.BinaryDilation(inv, h, w, 1))) == (string)c.Get("dil1") && Md5(U1(UxNd.BinaryDilation(inv, h, w, 3))) == (string)c.Get("dil3") && Md5(U1(UxNd.BinaryDilation(inv, h, w, 2, true))) == (string)c.Get("dilc");
                n++; if (!ok || !okd) { bad++; UxTest.Log("     ndimage " + h + "x" + w + " threshold " + thr + ": " + (ok ? "" : "distance_transform_edt ") + (okd ? "" : "binary_dilation ") + "differs"); }
            }
            UxTest.Report(bad == 0, "scipy.ndimage distance_transform_edt (indices and distances) and binary_dilation: " + (n - bad) + " of " + n + " masks");
        }
        // ---- float16, rounding
        {
            var h = UxJson.Obj(r.Get("half"));
            var d = Dvals(20000, 77).Select(x => x / 97.0).ToArray();
            var x2 = d.Concat(d.Select(v => v * 1e-6)).Concat(d.Select(v => v * 300.0)).Concat(new[] { 0.0, -0.0, 65519.99, 65520.0, 1e-8, 5.960464477539063e-08, 2.9802322387695312e-08, 2.98023223876953125e-08 * 1.0000001, double.PositiveInfinity, double.NegativeInfinity, UxNp.NaN }).ToArray();
            var b64 = new byte[x2.Length * 2]; var b32 = new byte[x2.Length * 2];
            for (int i = 0; i < x2.Length; i++) { UxBin.PutU16(b64, 2 * i, UxNp.DoubleToHalf(x2[i])); UxBin.PutU16(b32, 2 * i, UxNp.FloatToHalf((float)x2[i])); }
            UxTest.Report(Md5(b64) == (string)h.Get("f64") && Md5(b32) == (string)h.Get("f32"), "astype('<f2') of " + x2.Length + " float64 / float32 values");
            UxTest.Report(Md5(I8(d.Select(v => (long)UxNp.Round(v * 3.7)).ToArray())) == (string)h.Get("round") && Md5(F8(d.Select(v => UxNp.Round(v, 2)).ToArray())) == (string)h.Get("round2"), "np.round(x) and np.round(x, 2) of 20,000 values");
            int bad = 0; var pr = UxJson.Arr(r.Get("pyround"));
            for (int i = 0; i < pr.Count; i++)
            {
                var row = UxJson.Arr(pr[i]); double v = d[i] / 3.0;
                if (PyFormat.Repr(v) != (string)row[0] || UxNp.PyRound(v, 6) != UxJson.Num(row[1]) || UxNp.PyRound(v, 2) != UxJson.Num(row[2]) || UxNp.Round(v) != UxJson.Num(row[3])) bad++;
            }
            UxTest.Report(bad == 0, "repr(x), round(x, 6), round(x, 2), round(x) of " + pr.Count + " values: " + bad + " differ");
        }
        // ---- shaders (the reflex-lib copies in the reference folder)
        {
            int bad = 0, n = 0;
            foreach (var kv in UxJson.Obj(r.Get("shaders")).items)
            {
                var sum = UxD3d9sh.Summarize(File.ReadAllBytes(Path.Combine(s_py, "templates", "reflex-lib", "shaders", kv.Key)));
                var doc = sum.Select(e => (object)L(e.entry, e.kind, e.ctab.Select(c => (object)L(c.name, c.regset, (long)c.index, (long)c.count)).ToList(),
                    e.decls.Select(c => (object)L(c.usage, (long)c.index, (long)c.rtype, (long)c.reg, (long)c.extra)).ToList())).ToList();
                n++; if (UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.DumpsUx(doc))) != (string)kv.Value) { bad++; UxTest.Log("     d3d9sh.summarize(" + kv.Key + ") differs"); }
            }
            UxTest.Report(bad == 0, "d3d9sh.summarize (container, CTAB, dcl): " + (n - bad) + " of " + n + " shaders");
        }
        // ---- the PC scene, the BXML tree codec, canon_namespaces on the stock base
        {
            string fd = Path.Combine(s_py, "templates", "stock-SX_FortDodge", "SX_FortDodge.dx9.");
            var w = UxJson.Obj(r.Get("pcscene"));
            byte[] sceneBytes = File.ReadAllBytes(fd + "scene");
            var subs = UxPcScene.Load(sceneBytes);
            byte[] saved = UxPcScene.Save(subs);
            var d = new JObj().Set("subs", subs.Select(s => (object)L(s.name, (long)s.chunks.Count, s.nent, (long)s.Entities().Count())).ToList()).Set("save", Md5(saved)).Set("same_as_file", UxBin.Same(saved, sceneBytes));
            d.Set("ns", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(subs.SelectMany(s => s.Entities()).Select(e => { var p = UxPcScene.Prop(e, "Name"); return (object)L(UxPcScene.Ns(e), p != null && p.children.Count > 0 ? p.children[0].value : null); }).ToList()))));
            d.Set("matrices", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(subs.SelectMany(s => s.Entities()).Where(e => UxPcScene.Prop(e, "Matrix") != null).Select(e => (object)Dbls(UxPcScene.GetMatrix(e))).ToList()))));
            var diff = UxTest.JsonDifferences(d, w);
            foreach (var x in diff.Take(5)) UxTest.Log("     pc scene: " + x);
            UxTest.Report(diff.Count == 0, "scene.load / save of SX_FortDodge.dx9.scene (" + subs.Count + " sub-scenes; save " + (UxBin.Same(saved, sceneBytes) ? "gives the file's bytes back" : "differs from the file, as in Python") + ")");
            // stored-deflate save loads to the same trees
            var again = UxPcScene.Load(UxPcScene.Save(subs, true));
            UxTest.Check(UxBin.Same(UxPcScene.Save(again), saved), "scene saved with stored deflate reloads to the same chunks");

            var wb = UxJson.Obj(r.Get("bxml"));
            var lvb = new UxBxml(File.ReadAllBytes(fd + "level")); var dbb = new UxBxml(File.ReadAllBytes(fd + "database"));
            var bd = new JObj().Set("level_xml", UxBin.Md5(Encoding.UTF8.GetBytes(lvb.ToXml()))).Set("level_tree", Md5(UxBxml.EncodeTree(lvb.ToTree(), lvb.version, lvb.unknown)))
                .Set("db_tree", Md5(UxBxml.EncodeTree(dbb.ToTree(), dbb.version, dbb.unknown))).Set("db_bytes", Md5(dbb.ToBytes())).Set("db_xml", UxBin.Md5(Encoding.UTF8.GetBytes(dbb.ToXml())));
            var bdiff = UxTest.JsonDifferences(bd, wb);
            foreach (var x in bdiff.Take(5)) UxTest.Log("     bxml: " + x);
            UxTest.Report(bdiff.Count == 0, "bxml to_xml / to_tree + encode_tree / to_bytes on the stock level file and database");

            var wc = UxJson.Obj(r.Get("canon"));
            var ents = subs[0].Entities().Select(e => e.DeepCopy()).ToList();
            foreach (var e in ents) foreach (var x in e.Iter()) foreach (var a in x.attrs) if (a.name == "namespace" && a.type == 1) a.str = a.str.ToLowerInvariant();
            var changes = UxCommon.CanonNamespaces(ents);
            var cd = new JObj().Set("changes", changes.Select(c => (object)L(c.old, c.canon, (long)c.count)).ToList())
                .Set("after", UxBin.Md5(Encoding.UTF8.GetBytes(UxJson.Dumps(ents.Select(e => (object)e.Iter().SelectMany(x => x.attrs).Where(a => a.name == "namespace").Select(a => a.Value).ToList()).ToList()))));
            var cdiff = UxTest.JsonDifferences(cd, wc);
            foreach (var x in cdiff.Take(5)) UxTest.Log("     canon_namespaces: " + x);
            UxTest.Report(cdiff.Count == 0, "canon_namespaces on the lower-cased base scene: " + changes.Count + " spellings restored");
        }
    }
}
