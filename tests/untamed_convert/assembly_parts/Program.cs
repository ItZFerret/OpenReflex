// tests/untamed_convert/assembly_parts: the libraries the track builder calls (UxBase, UxBuildAssets, UxTuff, UxWater, UxScreens, UxAds,
// UxSplitGate, UxDirtLook and the constant template name tables of UxStock) against the values the current Python code returns IN MEMORY for the 12
// levels with each track's own build options (ref_parts.py writes them into <ref>/; run.sh runs it).
//   names      UxTemplateNames.Pinned against the reference's full DE sketch / HuCkiT template databases, for every name the 12 levels can ask
//   base       ux_base on SX_FortDodge from the Reflex library: keep policy, prepare (scene bytes, level file bytes, record), hf_roles,
//              script renames, core literals, global tables, animation sets, the asset closure, tex_bc / constant_texture, the water
//              spray kit (ux_water.splash on the base's ParticleManager), Image.resize's pass order
//   assets     ux_build_assets: Sources (index, bytes, md5s), entity_refs, rename_refs, filter_dropped, TemplateAssets
//   closure    ux_base.closure as base_prune runs it: the prepared base + the level's converted entities and stage outputs
//   tuff, water, screens, ads, splitgate      the module's build function (and its option variants) per level
//   dirtlook   ux_dirtlook (the dirt-look rounds, DESIGN 23.10): the base HF material's samplers and parameters, the layer means, the
//              client grid index, the race corridor, lanes, track and signed distance, the variation numbers (median, std(log v), the
//              exponent), the three maps and their textures (tex_bc with straight alpha) and the record, per level, on the tdf of the
//              track's reference build; exp / log / pow / hypot against the C library's values numpy returned (see DirtLookStatic)
// The records are also compared with the ones the reference BUILDS wrote (build/<short>-v<k>/build_info.json: rollout.tuff, ec_water,
// screens, massive_ads, split_gate, assembly.base), and the split gate's material folder with the one the Foothills build left in the
// reference's work store.
// Every comparison is exact: JSON documents are written with ux_common's writer and compared byte for byte (on a difference the first
// JSON paths are printed), entity lists as their encoded SceneChunk documents, assets by md5 and size. The work store of a level is
// <scratch>/work/<Level>, seeded with the reference's stage outputs (level.json, meshes, terrain, collision-t9, materials, textures).
// The path TEXT Python records is reproduced with UxPaths.root = the reference root. ONE normalisation, on the split gate only: the
// reference driver must not write into the reference, so ux_splitgate's material folder is redirected into <ref>/<Level>/splitgate; the
// test maps that prefix back to work/<Level>/splitgate before comparing (what the real builder writes).
//   stages     (on request, not in 'all') the textures, collision and materials stages on the constant name tables = on the full databases
// Arguments: [names|base|assets|closure|tuff|water|screens|ads|splitgate|dirtlook|all|stages] --levels a,b --ref <dir> (+ UxTest.OpenEnv's)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static string Ref;                      // <ref>
    static UxPaths Paths;
    static readonly UTF8Encoding Utf8 = new UTF8Encoding(false);
    static string F(double s) => s.ToString("0.0", CultureInfo.InvariantCulture);
    /// What was compared, summed over the levels (printed at the end).
    static readonly List<KeyValuePair<string, long>> Stats = new List<KeyValuePair<string, long>>();
    static void Stat(string name, long n)
    {
        for (int i = 0; i < Stats.Count; i++) if (Stats[i].Key == name) { Stats[i] = new KeyValuePair<string, long>(name, Stats[i].Value + n); return; }
        Stats.Add(new KeyValuePair<string, long>(name, n));
    }

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "all";
        using (var env = UxTest.OpenEnv(args))
        {
            Ref = UxTest.Arg(args, "ref", "UX_PARTS_REF", Path.Combine(env.scratch, "ref"));
            string only = UxTest.Arg(args, "levels");
            if (env.disc == null || env.stock == null)
            {
                UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)");
                return UxTest.Result("assembly_parts");
            }
            if (!File.Exists(Path.Combine(Ref, "done.txt"))) { UxTest.Skip("no reference values in " + Ref + " (run.sh writes them with ref_parts.py)"); return UxTest.Result("assembly_parts"); }
            Paths = new UxPaths { root = env.py.Replace('\\', '/').TrimEnd('/') };
            if (File.Exists(Path.Combine(Ref, "tools_md5.json")))
            {   // the reference is still being edited: say which module versions the values were taken from, and which changed since
                var then = RefJson("tools_md5.json"); var changed = new List<string>(); var parts = new List<string>();
                foreach (var kv in then.items)
                {
                    string f = Path.Combine(env.py, "tools", kv.Key);
                    if (File.Exists(f) && UxBin.Md5File(f) != (string)kv.Value) changed.Add(kv.Key);
                    if (kv.Key == "ux_base.py" || kv.Key == "ux_build_assets.py" || kv.Key == "ux_tuff.py" || kv.Key == "ux_water.py" || kv.Key == "ux_screens.py" || kv.Key == "ux_ads.py" ||
                        kv.Key == "ux_splitgate.py" || kv.Key == "ux_stocklib.py" || kv.Key == "ux_dirtlook.py" || kv.Key == "ux_build_track.py") parts.Add(kv.Key + " " + ((string)kv.Value).Substring(0, 8));
                }
                UxTest.Log("reference modules of the values: " + string.Join(", ", parts));
                if (changed.Count > 0) UxTest.Log("NOTE the reference changed since the values were written (UX_REFS=1 writes them again): " + string.Join(", ", changed));
            }
            var levels = new List<UxLevelRow>();
            foreach (var lv in UxCommon.Levels) if (only == null || Array.IndexOf(only.Split(','), lv.shortName) >= 0 || Array.IndexOf(only.Split(','), lv.disc) >= 0) levels.Add(lv);
            bool Run(string name) => what == "all" || what == name;
            if (what == "stages") { StagesOnPinned(env, levels); return UxTest.Result("assembly_parts"); }
            if (Run("names")) Names(env, levels);
            // everything below runs on the constant name tables (UxTest.OpenEnv put the reference's template databases there): the parts
            // need only the disc and the Reflex library
            env.stock.templateNames["sx"] = UxTemplateNames.Pinned("sx"); env.stock.templateNames["fr"] = UxTemplateNames.Pinned("fr");
            if (Run("base")) Base(env);
            if (Run("tuff")) TuffStatic(env);
            if (Run("screens")) ScreensLayout(env);
            if (Run("dirtlook")) DirtLookStatic();
            bool perLevel = false;
            foreach (string f in new[] { "assets", "closure", "tuff", "water", "screens", "ads", "splitgate", "dirtlook" }) perLevel |= Run(f);
            if (perLevel)
            {
                UxBase.Prepared prepared = null; UxTemplateAssets TA = null; Dictionary<(string, string), string> globals = null;
                UxBase.AnimSetTable animSets = null; Dictionary<string, string> sess = null;
                var totals = new Dictionary<string, int[]>(StringComparer.Ordinal);
                foreach (var lv in levels)
                {
                    UxTest.Section(lv.disc);
                    var sw = Stopwatch.StartNew();
                    var L = new Level(env, lv);
                    if (!L.ok) continue;
                    if (prepared == null)
                    {
                        prepared = UxBase.LoadPrepared(UxBase.Resolve(), env.stock);
                        TA = new UxTemplateAssets(prepared.ts);
                        globals = UxBuildAssets.GlobalAssets(env.library);
                        animSets = UxBase.AnimSets(env.library); sess = UxBase.SessionAnims(env.library);
                    }
                    void Fam(string name, Action a)
                    {
                        if (!Run(name)) return;
                        int c0 = UxTest.Checks, f0 = UxTest.Failures;
                        try { a(); }
                        catch (Exception e) { UxTest.Fail(lv.disc + " " + name + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                        if (!totals.TryGetValue(name, out var t)) totals[name] = t = new int[2];
                        t[0] += UxTest.Checks - c0; t[1] += UxTest.Failures - f0;
                    }
                    Fam("assets", () => Assets(L, TA));
                    Fam("closure", () => Closure(L, prepared, TA, globals, animSets, sess));
                    Fam("tuff", () => Tuff(L));
                    L.prepared = prepared; L.TA = TA;
                    Fam("water", () => Water(L));
                    Fam("dirtlook", () => DirtLook(L));
                    L.maps = null;
                    Fam("screens", () => Screens(L, env));
                    Fam("ads", () => Ads(L, TA));
                    Fam("splitgate", () => SplitGate(L, TA));
                    L.ctx.disc.ClearCache();
                    UxTest.Log("     " + lv.disc + ": " + F(sw.Elapsed.TotalSeconds) + " s");
                }
                UxTest.Section("per-level families over " + levels.Count + " levels");
                foreach (var kv in totals) UxTest.Log("     " + kv.Key.PadRight(10) + kv.Value[0].ToString().PadLeft(6) + " checks, " + kv.Value[1] + " failed");
                UxTest.Log("     compared (sums over the levels):");
                foreach (var kv in Stats) UxTest.Log("       " + kv.Value.ToString().PadLeft(7) + "  " + kv.Key);
            }
            return UxTest.Result("assembly_parts");
        }
    }

    // ============================================================================================== helpers
    static bool Doc(string what, object doc, string refRel, Func<string, string> mapRef = null)
    {
        string path = Path.Combine(Ref, refRel.Replace('/', Path.DirectorySeparatorChar));
        if (!File.Exists(path)) { UxTest.Fail(what + ": no reference " + path); return false; }
        byte[] want = File.ReadAllBytes(path);
        if (mapRef != null) want = Utf8.GetBytes(mapRef(Utf8.GetString(want)));
        return UxTest.SameJsonFile(what, Utf8.GetBytes(UxJson.DumpsUx(doc, 1)), want) == 0;
    }
    static JObj RefJson(string refRel) => UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(Ref, refRel.Replace('/', Path.DirectorySeparatorChar)))));
    static List<object> Pair(string a, string b) => new List<object> { a, b };
    static List<object> Strs(IEnumerable<string> s) { var l = new List<object>(); foreach (string x in s) l.Add(x); return l; }
    static int Cmp(List<object> a, List<object> b)
    {
        for (int i = 0; i < Math.Min(a.Count, b.Count); i++)
        {
            int c = string.CompareOrdinal(a[i] as string ?? "", b[i] as string ?? "");
            if (c != 0) return c;
        }
        return a.Count.CompareTo(b.Count);
    }

    /// The back-to-back SceneChunk documents of a file -> the entities.
    static List<UxElem> ReadChunks(byte[] data)
    {
        var o = new List<UxElem>();
        int p = 0;
        while (p < data.Length)
        {
            int size = 36 + (int)UxBin.U32LE(data, p + 32);
            o.AddRange(new UxBxml(UxBin.Slice(data, p, size)).ToTree().children);
            p += size;
        }
        return o;
    }
    static byte[] Chunks(IList<UxElem> elems, UxEntities.Prototypes protos) => UxBin.Concat(UxEntities.EncodeChunks(elems, protos).ToArray());

    sealed class Level
    {
        public readonly UxTest.Env env; public readonly UxLevelRow lv; public readonly UxContext ctx; public readonly bool ok;
        public readonly JObj info; public readonly Dictionary<string, string> dirs = new Dictionary<string, string>(StringComparer.Ordinal);
        public readonly UxMaterialsOptions matOpts = new UxMaterialsOptions();
        public readonly UxEntities.Prototypes protos;
        public UxBase.Prepared prepared; public UxTemplateAssets TA;
        /// The dirt-look maps of the level as the builder computes them (UxBuildTrack.DirtLookMaps): the base HF material's four detail
        /// textures, the terrain stage outputs, the tdf of the track's reference build (ref_parts.py: <ref>/<Level>/dirtlook.tdf).
        public UxDirtLook.Maps maps; public double[] means; public (string shader, List<string> textures, List<KeyValuePair<string, double[]>> parameters) material;
        public double mapsSeconds;
        public UxDirtLook.Maps Maps()
        {
            if (maps != null) return maps;
            var ts = prepared.ts;
            string hm = UxBase.HfRoles(prepared.subs, UxBase.Resolve().stem, ts).Role("Custom_HFTRN");
            material = UxDirtLook.MaterialTextures(TA.Logical(ts.Get(hm, "material")));
            var layers = new List<byte[]>(); for (int i = 1; i < 5; i++) layers.Add(TA.Logical(ts.Get(material.textures[i], "texture")));
            means = UxDirtLook.LayerMeans(layers);
            var sw = Stopwatch.StartNew();
            maps = UxDirtLook.StockLook(ctx.work, dirs["terrain"], ctx.work.ReadJsonObj(dirs["terrain"] + "/terrain.json"), File.ReadAllBytes(RefPath("dirtlook.tdf")), means,
                (UxCommon.HF_SIZE - 1) * 1, null, 0, default, true);
            mapsSeconds = sw.Elapsed.TotalSeconds;
            return maps;
        }
        public string Rel(string name) => lv.disc + "/" + name;
        public string RefPath(string name) => Path.Combine(Ref, lv.disc, name.Replace('/', Path.DirectorySeparatorChar));

        public Level(UxTest.Env env, UxLevelRow lv)
        {
            this.env = env; this.lv = lv;
            if (!File.Exists(RefPath("items.json"))) { UxTest.Fail(lv.disc + ": no reference values (" + RefPath("items.json") + ")"); return; }
            info = RefJson(Rel("items.json"));
            foreach (var kv in UxJson.Obj(info.Get("dirs")).items) dirs[kv.Key] = (string)kv.Value;
            var mo = UxJson.Obj(info.Get("mat_opts"));
            matOpts.alpha = (string)mo.Get("alpha"); matOpts.prelit_off = (bool)mo.Get("prelit_off"); matOpts.nrm_off = (bool)mo.Get("nrm_off"); matOpts.crowd_off = (bool)mo.Get("crowd_off");
            matOpts.blend = (string)mo.Get("blend"); matOpts.bale_base = (string)mo.Get("bale_base"); matOpts.mipfilter = (string)mo.Get("mipfilter"); matOpts.composite = (string)mo.Get("composite");
            matOpts.literal_albedo = (string)mo.Get("literal_albedo"); matOpts.alpha_source = (string)mo.Get("alpha_source"); matOpts.diffuse_pick = (string)mo.Get("diffuse_pick");
            ctx = env.Level(lv, "work", false); ctx.log = s => { };
            int n = Seed();
            if (n > 0) UxTest.Log("     seeded " + n + " reference stage files into " + ctx.work.Root);
            protos = new UxEntities.Prototypes(env.stock);
            ok = true;
        }
        /// Copies the reference's stage outputs the parts read into the scratch work store (a file already there with the same size is kept).
        int Seed()
        {
            int n = 0;
            var rels = new List<string> { "level.json" }; foreach (var kv in dirs) rels.Add(kv.Value);
            foreach (string rel in rels)
            {
                string src = Path.Combine(env.PyWork(lv), rel.Replace('/', Path.DirectorySeparatorChar));
                if (File.Exists(src)) { n += CopyIfNew(src, rel); continue; }
                if (!Directory.Exists(src)) throw new UxException("no reference output " + src);
                string top = Path.GetFullPath(src).TrimEnd(Path.DirectorySeparatorChar);
                foreach (string f in Directory.GetFiles(top, "*", SearchOption.AllDirectories))
                {
                    string r = rel.TrimEnd('/') + "/" + f.Substring(top.Length + 1).Replace(Path.DirectorySeparatorChar, '/');
                    if (r.EndsWith(".png", StringComparison.OrdinalIgnoreCase) && !r.StartsWith(dirs["terrain"] + "/", StringComparison.Ordinal)) continue;   // previews
                    n += CopyIfNew(f, r);
                }
            }
            return n;
        }
        int CopyIfNew(string src, string rel)
        {
            string dst = ctx.work.PathOf(rel);
            var si = new FileInfo(src);
            if (File.Exists(dst)) { var di = new FileInfo(dst); if (di.Length == si.Length && di.LastWriteTimeUtc == si.LastWriteTimeUtc) return 0; }
            Directory.CreateDirectory(Path.GetDirectoryName(dst));
            File.Copy(src, dst, true);
            File.SetLastWriteTimeUtc(dst, si.LastWriteTimeUtc);
            return 1;
        }
        JObj _build; bool _buildTried;
        /// build_info.json of the track's current reference build (build/<short>-v<k>), or null.
        public JObj BuildInfo()
        {
            if (_buildTried) return _build;
            _buildTried = true;
            string p = Path.Combine(env.py, "build", (string)info.Get("build"), "build_info.json");
            if (File.Exists(p)) _build = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(p)));
            return _build;
        }
        /// A record against the one the reference BUILD wrote into build_info.json (path a.b.c), as documents with exact numbers.
        public void SameAsBuild(string what, string path, object got, string builderKey = null)
        {
            var b = BuildInfo();
            if (b == null) { UxTest.Skip(lv.disc + ": no build_info.json of " + info.Get("build")); return; }
            object want = b; foreach (string k in path.Split('.')) want = (want as JObj)?.Get(k);
            if (want == null) { UxTest.Fail(lv.disc + " " + what + ": build_info.json of " + info.Get("build") + " has no " + path); return; }
            if (builderKey != null && want is JObj wj) { var c = new JObj(); foreach (var kv in wj.items) if (Array.IndexOf(builderKey.Split(','), kv.Key) < 0) c.Set(kv.Key, kv.Value); want = c; }
            var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(got, 1)), want);
            UxTest.Check(d.Count == 0, lv.disc + " " + what + " = " + path + " of build/" + info.Get("build") + "/build_info.json (" + d.Count + " differences)");
            for (int i = 0; i < d.Count && i < 6; i++) UxTest.Log("        " + d[i]);
        }
        public UxSources Sources() => new UxSources(lv, ctx.work, dirs, Paths);
        /// The S7 items of the build (the reference's in-memory elems, read back from their encoded chunks), in Python's order.
        public List<KeyValuePair<string, UxElem>> Items()
        {
            var q = new Dictionary<string, Queue<UxElem>>(StringComparer.Ordinal);
            foreach (string sub in new[] { "Base", "FreeRide" }) q[sub] = new Queue<UxElem>(ReadChunks(File.ReadAllBytes(RefPath("items_" + sub + ".chunks"))));
            var o = new List<KeyValuePair<string, UxElem>>();
            foreach (object s in UxJson.Arr(info.Get("subs"))) o.Add(new KeyValuePair<string, UxElem>((string)s, q[(string)s].Dequeue()));
            return o;
        }
        /// The (sub, elem) list as '<name>_<Sub>.chunks' against the reference files; returns the sub list.
        public List<object> SameItems(string what, string name, IList<KeyValuePair<string, UxElem>> items)
        {
            foreach (string sub in new[] { "Base", "FreeRide" })
            {
                var l = new List<UxElem>(); foreach (var kv in items) if (kv.Key == sub) l.Add(kv.Value);
                UxTest.SameBytes(lv.disc + " " + what + ": " + name + "_" + sub + ".chunks (" + l.Count + " entities)", Chunks(l, protos), File.ReadAllBytes(RefPath(name + "_" + sub + ".chunks")));
            }
            var subs = new List<object>(); foreach (var kv in items) subs.Add(kv.Key);
            return subs;
        }
        public void SameElems(string what, string file, IList<UxElem> elems)
            => UxTest.SameBytes(lv.disc + " " + what + ": " + file + " (" + elems.Count + " entities)", Chunks(elems, protos), File.ReadAllBytes(RefPath(file)));
    }

    // ============================================================================================== names
    static void Collect(object v, HashSet<string> into)
    {
        switch (v)
        {
            case string s: into.Add(s); break;
            case JObj j: foreach (var kv in j.items) { into.Add(kv.Key); Collect(kv.Value, into); } break;
            case List<object> l: foreach (object x in l) Collect(x, into); break;
        }
    }
    static void Names(UxTest.Env env, List<UxLevelRow> levels)
    {
        UxTest.Section("names: the constant template name tables against the reference's template databases");
        var full = new Dictionary<string, UxTemplateNames>(StringComparer.Ordinal);
        foreach (var (kind, dir, slot) in new[] { ("sx", "sx-sketch-slot1", "Beta_Sx_Track_Slot_1"), ("fr", "huckit-freeride-slot1", "Beta_Track_Slot_1") })
        {
            string db = Path.Combine(env.py, "templates", dir, slot + ".dx9.database");
            if (!File.Exists(db)) { UxTest.Skip("no reference template database " + db); return; }
            full[kind] = new UxTemplateNames(slot, File.ReadAllBytes(db));
        }
        var fd = env.stock.BaseSet();
        foreach (string kind in new[] { "sx", "fr" })
        {
            var pin = UxTemplateNames.Pinned(kind); int n = 0, inFd = 0; var notFd = new List<string>();
            foreach (var t in pin.byType)
                foreach (var kv in t.Value)
                {
                    n++;
                    UxTest.Check(full[kind].Find(t.Key, kv.Value) == kv.Value, "pinned " + kind + " name " + kv.Value + "." + t.Key + " is the template's spelling");
                    if (fd.Has(kv.Value, t.Key)) inFd++; else notFd.Add(kv.Value + "." + t.Key);
                }
            int total = 0; foreach (var t in full[kind].byType) total += t.Value.Count;
            UxTest.Log("     " + kind + ": " + n + " pinned names of the template's " + total + "; " + inFd + " are also SX_FortDodge assets, not in SX_FortDodge: " + string.Join(", ", notFd));
        }
        // every lookup the stages and the parts can make on the 12 levels. They ask by (name, type): ux_textures for the level's
        // texture names, ux_collision for its collis names, ux_materials for material names (the closure's and the terrain
        // requests') and for the texture names its entries, library and generated textures carry, ux_ads / ux_splitgate for panel
        // and gate materials and textures. Asked here: every asset of the level's disc packages under its own type (and with / without
        // the UX_ prefix), and every material / texture / collis name the reference's stage manifests hold.
        var types = new HashSet<string>(StringComparer.Ordinal);
        foreach (var t in full.Values) foreach (string k in t.byType.Keys) types.Add(k);
        long asked = 0, hits = 0, bad = 0;
        foreach (var lv in levels)
        {
            var names = new Dictionary<string, HashSet<string>>(StringComparer.Ordinal);
            void Ask(string type, object name) { if (!(name is string s) || s.Length == 0) return; if (!names.TryGetValue(type, out var h)) names[type] = h = new HashSet<string>(StringComparer.Ordinal); h.Add(s); }
            foreach (var a in env.disc.Level(lv).assets) Ask(a.type, a.name);
            string w = env.PyWork(lv);
            JObj Load(string rel) { string p = Path.Combine(w, rel.Replace('/', Path.DirectorySeparatorChar)); return File.Exists(p) ? UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(p))) : null; }
            foreach (string md in new[] { "materials", "materials-safe", "materials-mip-keep" })
            {
                var mj = Load(md + "/materials.json"); if (mj == null) continue;
                foreach (var kv in mj.items)
                {
                    if (!(kv.Value is JObj e) || !e.Has("action")) continue;
                    Ask("material", kv.Key); Ask("material", e.Get("asset")); Ask("material", e.Get("source"));
                    foreach (string k in new[] { "textures", "source_textures" }) if (e.Get(k) is List<object> l) foreach (object t in l) Ask("texture", t);
                }
                foreach (string k in new[] { "library_textures", "generated_textures", "texture_clash" }) if (mj.Get(k) is JObj o) foreach (var kv in o.items) Ask("texture", kv.Key);
                if (mj.Get("template_textures") is List<object> tt) foreach (object t in tt) Ask("texture", t);
                if (mj.Get("material_clash") is JObj mc) foreach (var kv in mc.items) Ask("material", kv.Key);
            }
            var tj = Load("textures/textures.json");
            if (tj?.Get("textures") is JObj tex) foreach (var kv in tex.items) { Ask("texture", kv.Key); Ask("texture", (kv.Value as JObj)?.Get("asset")); }
            foreach (string cd in new[] { "collision", "collision-t9", "collision-prims22" })
                if (Load(cd + "/collision.json")?.Get("sources") is JObj src) foreach (var kv in src.items) { Ask("collis", kv.Key); Ask("collis", (kv.Value as JObj)?.Get("asset")); }
            var rq = Load("terrain/material_requests.json");
            if (rq?.Get("requests") is JObj reqs) foreach (var kv in reqs.items) { Ask("material", kv.Key); if ((kv.Value as JObj)?.Get("textures") is List<object> l) foreach (object t in l) Ask("texture", t); }
            if (rq?.Get("generated_textures") is JObj gen) foreach (var kv in gen.items) Ask("texture", kv.Key);
            foreach (string fam in new[] { "ads.json", "splitgate.json" })
            {   // the panel and gate assets ux_ads / ux_splitgate convert (they ask asset_name / template_clash): [name, type, ...] rows of
                // the reference values. ux_tuff, ux_water and ux_screens add stock or generated assets without asking the template tables.
                string p = Path.Combine(Ref, lv.disc, fam); if (!File.Exists(p)) continue;
                void Rows(object v)
                {
                    if (v is JObj j) foreach (var kv in j.items) Rows(kv.Value);
                    else if (v is List<object> l) { if (l.Count >= 2 && l[0] is string n && l[1] is string t && types.Contains(t)) Ask(t, n); foreach (object x in l) Rows(x); }
                }
                Rows(UxJson.ParseUtf8(File.ReadAllBytes(p)));
            }
            long levelBad = 0, levelNames = 0;
            foreach (var tn in names)
            {
                var all = new HashSet<string>(tn.Value, StringComparer.Ordinal);
                foreach (string s in tn.Value) { all.Add("UX_" + s); if (s.StartsWith("UX_", StringComparison.Ordinal)) all.Add(s.Substring(3)); }
                levelNames += all.Count;
                foreach (string kind in new[] { "sx", "fr" })
                {
                    var pin = UxTemplateNames.Pinned(kind);
                    foreach (string s in all)
                    {
                        asked++;
                        string a = full[kind].Find(tn.Key, s), b = pin.Find(tn.Key, s);
                        if (a != null) hits++;
                        if (a != b) { levelBad++; if (levelBad <= 5) UxTest.Log("     " + lv.disc + " " + kind + " " + s + "." + tn.Key + ": template " + (a ?? "-") + ", pinned " + (b ?? "-")); }
                    }
                }
            }
            bad += levelBad;
            UxTest.Check(levelBad == 0, lv.disc + ": the pinned tables answer every lookup as the template databases (" + levelNames + " names, " + levelBad + " differ)");
        }
        UxTest.Log("     " + asked + " lookups (name, type, table) over " + levels.Count + " levels, " + hits + " hits, " + bad + " differ");
        UxTest.Check(env.stock != null && new UxStock(env.library).templateNames.Count == 2, "a UxStock made from a library alone carries the two pinned tables");
    }

    /// 'stages' (not part of 'all': about 2 GB of scratch): the three stages that ask the template tables (textures, collision,
    /// materials) run twice per level, on the constant tables and on the reference's full template databases, from the same inputs.
    /// Every output file must be byte-identical: with the stage tests (full tables = Python) this is pinned tables = Python.
    static void StagesOnPinned(UxTest.Env env, List<UxLevelRow> levels)
    {
        UxTest.Section("stages: textures, collision and materials on the constant name tables against the full template databases");
        var full = new Dictionary<string, UxTemplateNames>(env.stock.templateNames, StringComparer.Ordinal);
        if (full["sx"].byType.Count < 10) { UxTest.Skip("the reference's template databases are needed (" + env.py + "/templates)"); return; }
        var pinned = new Dictionary<string, UxTemplateNames>(StringComparer.Ordinal) { { "sx", UxTemplateNames.Pinned("sx") }, { "fr", UxTemplateNames.Pinned("fr") } };
        long files = 0, pyFiles = 0;
        foreach (var lv in levels)
        {
            var sw = Stopwatch.StartNew();
            var dirs = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var (tag, tables) in new[] { ("stages-pinned", pinned), ("stages-full", full) })
            {
                env.stock.templateNames["sx"] = tables["sx"]; env.stock.templateNames["fr"] = tables["fr"];
                var ctx = env.Level(lv, tag); ctx.log = s => { };
                var seed = new List<string> { "level.json" };
                foreach (string rel in new[] { "level_fr.json", "terrain/hf.npy", "terrain/material_requests.json", "terrain/textures" })
                    if (File.Exists(Path.Combine(env.PyWork(lv), rel.Replace('/', Path.DirectorySeparatorChar))) || Directory.Exists(Path.Combine(env.PyWork(lv), rel.Replace('/', Path.DirectorySeparatorChar)))) seed.Add(rel);
                env.Seed(ctx, seed.ToArray());
                try
                {
                    UxTextures.Convert(ctx, new UxTexturesOptions { throwOnProblems = false });
                    UxCollision.Convert(ctx, new UxCollisionOptions { type9 = "accept" });
                    UxMaterials.Convert(ctx, new UxMaterialsOptions());
                }
                catch (Exception e) { UxTest.Fail(lv.disc + " " + tag + ": " + e.GetType().Name + ": " + e.Message); }
                dirs[tag] = ctx.work.Root;
                ctx.disc.ClearCache();
            }
            foreach (string folder in new[] { "textures", "collision-t9", "materials" })
            {
                var r = UxTest.CompareFolder(lv.disc + " " + folder + ": constant tables = full template databases", Path.Combine(dirs["stages-pinned"], folder), Path.Combine(dirs["stages-full"], folder), null, false, true);
                files += r.identical;
                // and the binary outputs against the reference's own stage folders (the materials folder is compared by the materials
                // stage test, which knows how the stored manifests were seeded)
                if (folder == "materials") continue;
                string ext = folder == "textures" ? ".texture" : ".collis";
                var r2 = UxTest.CompareFolder(lv.disc + " " + folder + "/*" + ext + ": constant tables = the Python reference", Path.Combine(dirs["stages-pinned"], folder),
                    Path.Combine(env.PyWork(lv), folder), rel => rel.EndsWith(ext, StringComparison.OrdinalIgnoreCase), false, true);
                pyFiles += r2.identical;
            }
            UxTest.Log("     " + lv.disc + ": " + F(sw.Elapsed.TotalSeconds) + " s");
        }
        UxTest.Log("     " + files + " stage output files byte-identical between the two table sets; " + pyFiles + " textures and collis files byte-identical to the Python reference's");
        env.stock.templateNames["sx"] = full["sx"]; env.stock.templateNames["fr"] = full["fr"];
    }

    // ============================================================================================== base
    static void Base(UxTest.Env env)
    {
        UxTest.Section("base: ux_base on SX_FortDodge from " + env.library.Describe);
        var sw = Stopwatch.StartNew();
        var b = UxBase.Resolve();
        var stock = env.stock;
        // keep policy on the raw scene
        var raw = UxPcScene.Load(stock.BaseFile("scene"));
        var keep = new List<object>(); int nkeep = 0;
        foreach (var s in raw)
        {
            string suf = s.name.StartsWith(b.stem, StringComparison.Ordinal) ? s.name.Substring(b.stem.Length) : null;
            foreach (var e in s.Entities())
            {
                var (ok, why) = UxBase.KeepEntity(b.policy, suf, e);
                if (ok) nkeep++;
                keep.Add(new List<object> { s.name, UxPcScene.EntityId(e), UxPcScene.Ns(e), UxBase.NameOf(e), ok, why });
            }
        }
        Doc("keep_entity on " + keep.Count + " stock entities (" + nkeep + " kept)", keep, "base/keep.json");
        var P = UxBase.LoadPrepared(b, stock);
        UxTest.SameBytes("prepare: the prepared scene (scene.save)", UxPcScene.Save(P.subs), File.ReadAllBytes(Path.Combine(Ref, "base", "prepared.scene")));
        UxTest.SameBytes("prepare: the prepared level file", P.level.ToBytes(), File.ReadAllBytes(Path.Combine(Ref, "base", "prepared.level")));
        Doc("prepare: the record", P.record, "base/prepare.json");
        Doc("Base.record()", b.Record(Paths), "base/record.json");
        var roles = UxBase.HfRoles(P.subs, b.stem, P.ts);
        Doc("hf_roles", roles.Record(), "base/hf_roles.json");
        // scripts
        string slot = UxCommon.SlotName("sx", 1);
        List<object> Ren(UxOrdered<(string name, string type), string> r) { var l = new List<object>(); foreach (var kv in r) l.Add(new List<object> { kv.Key.name, kv.Key.type, kv.Value }); return l; }
        var sc = new JObj();
        foreach (string which in new[] { "stock", "all", "none" })
        {
            var kinds = UxBase.ScriptKinds(b, which);
            sc.Set(which, new JObj().Set("kinds", Strs(kinds)).Set("renames", Ren(UxBase.ScriptRenames(b, P.ts, slot, kinds))));
        }
        sc.Set("default_kinds", Ren(UxBase.ScriptRenames(b, P.ts, slot)));
        var tp = new JObj();
        foreach (string s in new[] { slot, "Beta_Track_Slot_1", "SX_FortDodge", "NoUnderscore", "A_b", "Beta_Sx_Track_Slot_12" }) tp.Set(s, UxBase.IntroTrackPart(s));
        sc.Set("track_part", tp);
        Doc("script_kinds / script_renames / intro_track_part", sc, "base/scripts.json");
        // literals
        var ents = new List<UxElem>(); foreach (var s in P.subs) ents.AddRange(s.Entities());
        var types = new HashSet<string>(StringComparer.Ordinal); foreach (var e in ents) types.Add(UxPcScene.Ns(e));
        var lit = new JObj();
        HashSet<string> With(params string[] extra) { var h = new HashSet<string>(types, StringComparer.Ordinal); foreach (string x in extra) h.Add(x); return h; }
        foreach (var (nm, tt) in new[] { ("base", types), ("water", With("WaterSystemRSData")), ("crowd", With("BillboardCrowdRSData")),
            ("forest", new HashSet<string>(StringComparer.Ordinal) { "ForestSystemRSData" }), ("none", new HashSet<string>(StringComparer.Ordinal)) })
        {
            var st = new List<string>(tt); st.Sort(string.CompareOrdinal);
            var rows = new List<object>(); foreach (var r in UxBase.LiteralRows(tt)) rows.Add(new List<object> { r.feature, Pair(r.name, r.type), r.miss, r.where, r.source });
            var rts = new List<object>(); foreach (var k in UxBase.LiteralRoots(tt)) rts.Add(Pair(k.name, k.type));
            lit.Set(nm, new JObj().Set("types", Strs(st)).Set("rows", rows).Set("roots", rts));
        }
        Doc("literal_rows / literal_roots", lit, "base/literals.json");
        var cg = new List<List<object>>(); foreach (var kv in UxBase.CoreGlobalAssets(env.library)) cg.Add(new List<object> { kv.Key.lower, kv.Key.type, kv.Value });
        cg.Sort(Cmp);
        Doc("core_global_assets (" + cg.Count + ")", cg, "base/core_global.json");
        var gl = UxBuildAssets.GlobalAssets(env.library);
        var gll = new List<List<object>>(); foreach (var kv in gl) gll.Add(new List<object> { kv.Key.name, kv.Key.type, kv.Value });
        gll.Sort(Cmp);
        Doc("ux_build_assets.global_assets (" + gll.Count + ")", gll, "base/global_assets.json");
        var AS = UxBase.AnimSets(env.library);
        List<object> SetList(UxOrdered<string, List<string>> d) { var l = new List<object>(); foreach (var kv in d) l.Add(new List<object> { kv.Key, Strs(kv.Value) }); return l; }
        Doc("anim_sets (" + AS.sets.Count + " sets)", new JObj().Set("sets", SetList(AS.sets)).Set("vehicle", SetList(AS.vehicle)).Set("md5", AS.md5), "base/anim_sets.json");
        var sess = UxBase.SessionAnims(env.library);
        var sj = new JObj(); foreach (var kv in sess) sj.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value));
        Doc("session_anims (" + sess.Count + ")", sj, "base/session_anims.json");
        // closure over the base alone, as base_prune runs it
        var TA = new UxTemplateAssets(P.ts);
        var keys = new List<(string, string)>(); var by = new Dictionary<(string, string), UxPcAsset>();
        foreach (var a in P.ts.assets) { if (!by.ContainsKey((a.name, a.type))) keys.Add((a.name, a.type)); by[(a.name, a.type)] = a; }
        var index = UxBase.IndexOf(keys);
        var ren = UxBase.ScriptRenames(b, P.ts, slot, UxBase.ScriptKinds(b, "stock"));
        var roots = new List<(string name, string type)> { (b.stem + "-sx", "tdf") };
        foreach (var kv in ren) roots.Add(kv.Key);
        foreach (string n in b.policy.implicit_sounds) roots.Add((n, "sound"));
        foreach (var r in b.policy.implicit_roots) if (!roots.Contains(r)) roots.Add(r);
        foreach (var k in UxBase.LiteralRoots(types)) if (!roots.Contains(k)) roots.Add(k);
        var setRoots = new List<string>(); foreach (var (set, type) in UxBase.ENGINE_ANIM_SETS) if (type == null || types.Contains(type)) setRoots.Add(set);
        var c = UxBase.Closure(ents, roots, index, k => TA.Logical(by[k]), gl.Keys, AS.sets, setRoots, sess);
        Doc("closure over the base (" + c.kept.Count + " kept, " + c.unresolved.Count + " unresolved)", ClosureDoc(c, roots, setRoots, null), "base/closure.json");
        var c2 = UxBase.Closure(ents, new List<(string, string)> { (b.stem + "-sx", "tdf") }, index, k => TA.Logical(by[k]), gl.Keys);
        var d2 = ClosureDoc(c2, null, null, null); d2.items.RemoveAll(kv => kv.Key == "sets" || kv.Key == "nowhere");
        Doc("closure without animation sets (" + c2.kept.Count + " kept)", d2, "base/closure_plain.json");
        var es = new List<object>(); foreach (var e in ents) es.Add(new List<object> { UxBase.NameOf(e), Strs(UxBase.EntityStrings(e)) });
        Doc("entity_strings", es, "base/entity_strings.json");
        var ast = new JObj();
        for (int i = 0; i < keys.Count; i += 37) if (UxBase.FOLLOW.ContainsKey(keys[i].Item2)) ast.Set(keys[i].Item1 + "." + keys[i].Item2, Strs(UxBase.AssetStrings(TA.Logical(by[keys[i]]), keys[i].Item2)));
        Doc("asset_strings (" + ast.items.Count + " assets)", ast, "base/asset_strings.json");
        // constant textures: the reference rows give the inputs
        var ct = new List<object>();
        foreach (object ro in UxJson.Arr(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(Ref, "base", "constant_textures.json")))))
        {
            var r = UxJson.Arr(ro);
            var col = new List<int>(); foreach (object x in UxJson.Arr(r[2])) col.Add((int)(long)x);
            int w = (int)(long)r[3], h = (int)(long)r[4]; string fmt = (string)r[5];
            string nm = r[1] as string;
            if ((string)r[0] != "synthetic")
            {   // the builder's two maps: the role's asset of the base, with the stock asset's own size and format
                nm = roles.Role((string)r[0]);
                var old = new UxPcTexture(P.ts.ReadLogical(nm, "texture"));
                w = old.recs[0].w; h = old.recs[0].h; fmt = Encoding.ASCII.GetString(old.recs[0].fmt, 0, 4);
            }
            byte[] tex = UxBase.ConstantTexture(col, w, h, fmt == "DXT5");
            ct.Add(new List<object> { r[0], nm, r[2], (long)w, (long)h, fmt, UxBin.Md5(tex), (long)tex.Length });
        }
        Doc("constant_texture (" + ct.Count + " textures)", ct, "base/constant_textures.json");
        var tb = new List<object>();
        foreach (object ro in UxJson.Arr(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(Ref, "base", "tex_bc.json")))))
        {
            var r = UxJson.Arr(ro);
            int w = (int)(long)r[0], h = (int)(long)r[1]; bool rgba = (string)r[2] == "RGBA", dxt5 = (string)r[3] == "DXT5";
            int ch = rgba ? 4 : 3; var px = new byte[w * h * ch];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int at = (y * w + x) * ch;
                    px[at] = (byte)((x * 7 + y * 13) % 256); px[at + 1] = (byte)(((x * 3) ^ (y * 5)) % 256); px[at + 2] = (byte)((x + y) % 256);
                    if (rgba) px[at + 3] = (byte)((x * y + 17) % 256);
                }
            byte[] tex = UxBase.TexBc(new UxImage(px, h, w, ch), dxt5);
            tb.Add(new List<object> { r[0], r[1], r[2], r[3], UxBin.Md5(tex), (long)tex.Length });
        }
        Doc("tex_bc on patterned images (" + tb.Count + ")", tb, "base/tex_bc.json");
        // tex_bc(straight_alpha=True): the dirt-look maps' mip chain (colour and alpha resized apart); the last column is the plain tex_bc
        var tbs = new List<object>();
        foreach (object ro in UxJson.Arr(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(Ref, "base", "tex_bc_straight.json")))))
        {
            var r = UxJson.Arr(ro);
            int w = (int)(long)r[0], h = (int)(long)r[1]; bool rgba = (string)r[2] == "RGBA", dxt5 = (string)r[3] == "DXT5", zero = r[4] as string == "zero";
            int ch = rgba ? 4 : 3; var px = new byte[w * h * ch];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int at = (y * w + x) * ch;
                    px[at] = (byte)((x * 7 + y * 13) % 256); px[at + 1] = (byte)(((x * 3) ^ (y * 5)) % 256); px[at + 2] = (byte)((x + y) % 256);
                    if (rgba) px[at + 3] = zero ? (byte)0 : (byte)((x * y + 17) % 256);
                }
            byte[] tex = UxBase.TexBc(new UxImage(px, h, w, ch), dxt5, true);
            tbs.Add(new List<object> { r[0], r[1], r[2], r[3], r[4], UxBin.Md5(tex), (long)tex.Length, UxBin.Md5(UxBase.TexBc(new UxImage(px, h, w, ch), dxt5)) });
        }
        Doc("tex_bc with straight alpha on patterned images (" + tbs.Count + ": alpha patterned, alpha 0, RGB)", tbs, "base/tex_bc_straight.json");
        // ux_water.splash on the base's ParticleManager kit, as ux_build_track.water_splash calls it; retarget_particles
        UxElem pm = null; foreach (var e in P.Sub("Base").Entities()) if (UxPcScene.Ns(e) == "ParticleManagerRSData") pm = e;
        var pmRoles = UxWater.ParticleManagerAssets(pm);
        string kit = pmRoles["ParticleEmitterAsset"].name;
        byte[] Kit(string t) => TA.Logical(TA.Clash(kit, t));
        var (srec, sassets) = UxWater.Splash(stock, Kit("particle"), Kit("partattr"), Kit("texture"), kit);
        foreach (var kv in sassets) UxTest.SameBytes("splash: " + kv.Key.name + "." + kv.Key.type, kv.Value.data, File.ReadAllBytes(Path.Combine(Ref, "base", "splash", kv.Key.name + "." + kv.Key.type)));
        var pm2 = pm.DeepCopy(); UxWater.RetargetParticles(pm2, (string)srec.Get("assets"));
        UxTest.SameBytes("splash: the retargeted ParticleManager entity", UxBin.Concat(UxEntities.EncodeChunks(new List<UxElem> { pm2 }, new UxEntities.Prototypes(stock)).ToArray()),
            File.ReadAllBytes(Path.Combine(Ref, "base", "splash_pm.chunks")));
        JObj RolesDoc(Dictionary<string, (string name, string type)> d) { var j = new JObj(); foreach (var (role, _) in UxWater.PM_ROLES) if (d.ContainsKey(role)) j.Set(role, Pair(d[role].name, d[role].type)); return j; }
        long tiw = 0; foreach (var e in new UxBxml(sassets.Get((string)srec.Get("assets"), "partattr").data).ToTree().FindAll("Emitter")) if (UxWater.TireInWater(e)) tiw++;
        Doc("splash: record, 3 assets, ParticleManager roles (" + tiw + " TireInWater records)", new JObj().Set("record", srec).Set("assets", sassets.Rows()).Set("kit", kit)
            .Set("roles", RolesDoc(pmRoles)).Set("roles_after", RolesDoc(UxWater.ParticleManagerAssets(pm2))).Set("tire_in_water", tiw), "base/splash.json");
        // ux_water.reflection_cube on the base's level cube, as ux_build_track.water_reflection calls it; reflection_retarget
        UxElem ambient = null; foreach (var e in P.Sub("Base").Entities()) if (UxPcScene.Ns(e) == "AmbientDirectionalLightRSData") ambient = e;
        var (cubeName, cubeType) = UxWater.ReflectionMap(ambient);
        byte[] cubeBytes = TA.Logical(TA.Clash(cubeName, cubeType));
        var rrecs = new JObj();
        foreach (string mode in new[] { "dim", "nolights" })
        {
            var (cb, rrec) = UxWater.ReflectionCube(cubeBytes, mode);
            UxTest.SameBytes("reflection cube: " + cubeName + " " + mode, cb, File.ReadAllBytes(Path.Combine(Ref, "base", "reflection", mode + ".texture")));
            rrecs.Set(mode, rrec);
        }
        var amb2 = ambient.DeepCopy(); string oldCube = UxWater.ReflectionRetarget(amb2, UxWater.REFLECTION_PREFIX + cubeName);
        UxTest.SameBytes("reflection cube: the retargeted AmbientDirectionalLight entity", UxBin.Concat(UxEntities.EncodeChunks(new List<UxElem> { amb2 }, new UxEntities.Prototypes(stock)).ToArray()),
            File.ReadAllBytes(Path.Combine(Ref, "base", "reflection_amb.chunks")));
        var afterMap = UxWater.ReflectionMap(amb2);
        Doc("reflection cube: records (dim, nolights), the entity's map before and after", new JObj().Set("cube", cubeName).Set("type", cubeType).Set("old", oldCube)
            .Set("after", Pair(afterMap.name, afterMap.type)).Set("records", rrecs), "base/reflection.json");
        // every half through float and back, and the rounding of float -> half at the ties
        int halfBad = 0;
        for (int h = 0; h < 65536; h++) { float f = UxWater.HalfToFloat((ushort)h); if (!float.IsNaN(f) && UxWater.FloatToHalf(f) != (ushort)h) halfBad++; }
        UxTest.Report(halfBad == 0 && UxWater.FloatToHalf(1.00048828125f) == 0x3C00 && UxWater.FloatToHalf(1.00146484375f) == 0x3C02 && UxWater.FloatToHalf(65520f) == 0x7C00
            && UxWater.FloatToHalf(2.98023224e-08f) == 0 && UxWater.FloatToHalf(2.98023259e-08f) == 1 && UxWater.FloatToHalf(-2f) == 0xC000,
            "half <-> float: 65536 halves round trip, ties to even, overflow and underflow (" + halfBad + " differ)");
        // Image.resize pass order (UxPil.Resize: vertical first where Pillow does)
        var rsz = new List<object>(); int vfirst = 0;
        foreach (object rowo in UxJson.Arr(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(Ref, "base", "resize_order.json")))))
        {
            var r = UxJson.Arr(rowo);
            int w = (int)(long)r[0], h = (int)(long)r[1], ow = (int)(long)r[2], oh = (int)(long)r[3]; string mode = (string)r[4], fl = (string)r[5];
            int ch = mode == "L" ? 1 : mode == "RGB" ? 3 : 4; var px = new byte[w * h * ch];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int at = (y * w + x) * ch;
                    px[at] = (byte)((x * 7 + y * 13) % 256);
                    if (ch >= 3) { px[at + 1] = (byte)(((x * 3) ^ (y * 5)) % 256); px[at + 2] = (byte)((x + y) % 256); }
                    if (ch == 4) px[at + 3] = (byte)((x * y + 17) % 256);
                }
            var o = UxPil.Resize(new UxImage(px, h, w, ch), ow, oh, fl == "lanczos" ? UxPil.Filter.Lanczos : fl == "bilinear" ? UxPil.Filter.Bilinear : UxPil.Filter.Box);
            if (ow != w && oh < h && h - oh > 2 * (w - ow)) vfirst++;
            rsz.Add(new List<object> { r[0], r[1], r[2], r[3], r[4], r[5], UxBin.Md5(o.data) });
        }
        Doc("Image.resize on " + rsz.Count + " size / mode / filter cases (" + vfirst + " with the vertical pass first)", rsz, "base/resize_order.json");
        // the record a reference build wrote: build_info.json assembly.base = Base.record() + preparation + hf_roles
        string items = Path.Combine(Ref, "Norton_STADIUM", "items.json");
        string bi = File.Exists(items) ? Path.Combine(env.py, "build", (string)RefJson("Norton_STADIUM/items.json").Get("build"), "build_info.json") : null;
        if (bi != null && File.Exists(bi))
        {
            var mine = b.Record(Paths); mine.Set("preparation", P.record).Set("hf_roles", roles.Record());
            var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(mine, 1)), UxJson.Get(UxJson.ParseUtf8(File.ReadAllBytes(bi)), "assembly", "base"));
            UxTest.Check(d.Count == 0, "Base.record() + preparation + hf_roles = assembly.base of " + bi.Replace('\\', '/') + " (" + d.Count + " differences)");
            for (int i = 0; i < d.Count && i < 6; i++) UxTest.Log("        " + d[i]);
        }
        else UxTest.Skip("no reference build_info.json for the base record");
        // the race stage's private copy of the policy (UxRaceTemplate) keeps the same entities and roles
        var rt = new UxRaceTemplate(stock);
        var mineEnts = new List<string>(); var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var sub in P.subs)
            foreach (var e in sub.Entities())
            {
                string nm = UxBase.NameOf(e);
                if (nm == null || UxPcScene.Prop(e, "Matrix") == null || !seen.Add(nm)) continue;
                mineEnts.Add(sub.name + "/" + UxPcScene.Ns(e) + "/" + nm + "/" + UxBin.Hex((byte[])UxPcScene.Prop(e, "Matrix").children[0].value));
            }
        var raceEnts = new List<string>();
        foreach (var e in rt.entities) { var mb = new byte[64]; for (int i = 0; i < 16; i++) UxBin.PutF32(mb, 4 * i, (float)e.matrix[i]); raceEnts.Add(e.sub + "/" + e.type + "/" + e.name + "/" + UxBin.Hex(mb)); }
        bool sameEnts = mineEnts.Count == raceEnts.Count; for (int i = 0; sameEnts && i < mineEnts.Count; i++) sameEnts = mineEnts[i] == raceEnts[i];
        var br = b.roles;
        bool sameRoles = rt.dropgate == br.dropgate && rt.startgate == br.startgate && rt.board_camera == br.board_camera && string.Join("|", rt.gate_patterns) == string.Join("|", br.gate_patterns)
            && string.Join("|", rt.board_patterns) == string.Join("|", br.board_patterns) && string.Join("|", rt.startgate_followers) == string.Join("|", br.startgate_followers);
        UxTest.Check(sameEnts && sameRoles, "UxRaceTemplate (the race stage's private subset of ux_base) keeps the same " + raceEnts.Count + " named entities, poses and roles as UxBase.LoadPrepared (" + mineEnts.Count + ")");
        UxTest.Log("     base: " + F(sw.Elapsed.TotalSeconds) + " s");
    }

    static JObj ClosureDoc(UxBase.ClosureResult c, List<(string name, string type)> roots, List<string> setRoots, List<(string name, string type)> literal)
    {
        var o = new JObj();
        if (roots != null) { var l = new List<object>(); foreach (var k in roots) l.Add(Pair(k.name, k.type)); o.Set("roots", l); }
        if (setRoots != null) o.Set("set_roots", Strs(setRoots));
        if (literal != null) { var l = new List<object>(); foreach (var k in literal) l.Add(Pair(k.name, k.type)); o.Set("literal_roots", l); }
        var kept = new List<object>(); foreach (var kv in c.kept) kept.Add(new List<object> { kv.Key.name, kv.Key.type, kv.Value });
        var un = new List<object>(); foreach (var u in c.unresolved) un.Add(new List<object> { u.name, u.type, u.why });
        var gl = new List<List<object>>(); foreach (var g in c.glob) gl.Add(Pair(g.name, g.type));
        gl.Sort(Cmp);
        var sets = new List<object>(); foreach (var kv in c.sets) sets.Add(new List<object> { kv.Key, Strs(kv.Value) });
        var nw = new List<object>(); foreach (var x in c.nowhere) nw.Add(Pair(x.member, x.set));
        return o.Set("kept", kept).Set("unresolved", un).Set("glob", gl).Set("sets", sets).Set("nowhere", nw);
    }

    // ============================================================================================== ux_build_assets
    static void Assets(Level L, UxTemplateAssets TA)
    {
        var S = L.Sources();
        var rows = new List<object>(); var clash = new List<object>();
        foreach (var kv in S.assets)
        {
            var r = kv.Value;
            var extra = new List<object>(); if (r.entry != null) extra.Add("entry"); if (r.has_in_template) extra.Add("in_template");
            rows.Add(new List<object> { r.name, r.type, S.PyPath(r.file), r.md5, r.origin, extra, r.entry != null ? UxBin.Md5(Utf8.GetBytes(UxJson.DumpsUx(r.entry, 1))) : null,
                r.in_template, UxBin.Md5(S.Read(kv.Key)) });
            var cl = TA.Clash(r.name, r.type);
            clash.Add(new List<object> { r.name, r.type, cl?.name, TA.Has(r.name, r.type) });
        }
        var mp = new List<string>(S.mesh_parts.Keys); mp.Sort(string.CompareOrdinal);
        var dm = new List<string>(S.dropped_materials); dm.Sort(string.CompareOrdinal);
        var tt = new List<string>(S.template_textures); tt.Sort(string.CompareOrdinal);
        var tr = new List<object>(); foreach (var kv in S.texture_renames) tr.Add(Pair(kv.Key, kv.Value));
        var doc = new JObj().Set("assets", rows).Set("problems", S.problems).Set("warnings", S.warnings).Set("manifests", S.manifests).Set("mesh_parts", Strs(mp))
            .Set("dropped_materials", Strs(dm)).Set("texture_renames", tr).Set("template_textures", Strs(tt)).Set("material_entries", Strs(S.material_entries.Keys))
            .Set("clash", clash).Set("template_keys", (long)TA.keys.Count).Set("template_lower", (long)TA.lower.Count);
        Doc(L.lv.disc + " Sources: " + S.assets.Count + " assets (index, md5 of every file read, clashes with the base)", doc, L.Rel("sources.json"));
        Stat("assets: stage output files indexed and read (Sources)", S.assets.Count);
        var items = L.Items();
        Stat("assets: S7 entities (entity_refs, rename_refs, filter_dropped inputs)", items.Count);
        Stat("assets: dropped materials", S.dropped_materials.Count);
        L.SameItems("items read back", "items", items);
        var refs = new List<object>();
        foreach (var it in items) { var l = new List<object>(); foreach (var r in UxBuildAssets.EntityRefs(it.Value)) l.Add(Pair(r.name, r.type)); refs.Add(l); }
        Doc(L.lv.disc + " entity_refs of " + items.Count + " entities", refs, L.Rel("entity_refs.json"));
        // rename_refs
        var all = new SortedSet<(string, string)>(Comparer<(string, string)>.Create((a, b) => { int c = string.CompareOrdinal(a.Item1, b.Item1); return c != 0 ? c : string.CompareOrdinal(a.Item2, b.Item2); }));
        foreach (var it in items) foreach (var r in UxBuildAssets.EntityRefs(it.Value)) all.Add(r);
        var ren = new Dictionary<(string, string), string>(); var renRows = new List<object>();
        foreach (var r in all) { if (ren.Count == 6) break; ren[r] = "UX_" + r.Item1; renRows.Add(new List<object> { r.Item1, r.Item2, "UX_" + r.Item1 }); }
        var it2 = L.Items(); long changes = 0;
        foreach (var it in it2) changes += UxBuildAssets.RenameRefs(it.Value, ren);
        L.SameItems("rename_refs", "renamed", it2);
        Doc(L.lv.disc + " rename_refs (" + changes + " changes)", new JObj().Set("renames", renRows).Set("changes", changes), L.Rel("rename_refs.json"));
        // filter_dropped
        var it3 = L.Items();
        string fb = TA.Has("Editor_Default", "material") ? "Editor_Default" : null;
        var f = UxBuildAssets.FilterDropped(it3, S.dropped_materials, name => (S.Read((name, "model")), S.Read((name, "surface"))), null, fb);
        var subs = L.SameItems("filter_dropped", "filtered", f.items);
        Stat("assets: entities the dropped-material filter re-pointed", UxJson.Int(f.report.Get("entities_changed")));
        Stat("assets: entities the filter removed", UxJson.Arr(f.report.Get("entities_removed")).Count);
        Stat("assets: group-filtered part variants (model + surface)", f.new_assets.Count);
        var na = new List<object>(); foreach (var kv in f.new_assets) na.Add(new List<object> { kv.Key.name, kv.Key.type, UxBin.Md5(kv.Value), (long)kv.Value.Length });
        Doc(L.lv.disc + " filter_dropped (" + S.dropped_materials.Count + " dropped materials, " + f.new_assets.Count + " variant assets)",
            new JObj().Set("report", f.report).Set("fallback", fb).Set("subs", subs).Set("new_assets", na), L.Rel("filter_dropped.json"));
    }

    static void Closure(Level L, UxBase.Prepared P, UxTemplateAssets TA, Dictionary<(string, string), string> gl, UxBase.AnimSetTable AS, Dictionary<string, string> sess)
    {
        var S = L.Sources();
        var keys = new List<(string, string)>(); var by = new Dictionary<(string, string), UxPcAsset>();
        foreach (var a in P.ts.assets) { if (!by.ContainsKey((a.name, a.type))) keys.Add((a.name, a.type)); by[(a.name, a.type)] = a; }
        foreach (var k in S.assets.Keys) keys.Add(k);
        var index = UxBase.IndexOf(keys);
        var ents = new List<UxElem>(); foreach (var s in P.subs) ents.AddRange(s.Entities());
        foreach (var it in L.Items()) ents.Add(it.Value);
        var types = new HashSet<string>(StringComparer.Ordinal); foreach (var e in ents) types.Add(UxPcScene.Ns(e));
        var b = P.@base;
        var ren = UxBase.ScriptRenames(b, P.ts, UxCommon.SlotName("sx", 1), UxBase.ScriptKinds(b, (string)UxJson.Get(L.info, "options", "base_scripts")));
        var roots = new List<(string name, string type)> { (b.stem + "-sx", "tdf") };
        foreach (var kv in ren) roots.Add(kv.Key);
        foreach (string n in b.policy.implicit_sounds) roots.Add((n, "sound"));
        foreach (var r in b.policy.implicit_roots) if (!roots.Contains(r)) roots.Add(r);
        var lit = UxBase.LiteralRoots(types);
        foreach (var k in lit) if (!roots.Contains(k)) roots.Add(k);
        var setRoots = new List<string>(); foreach (var (set, type) in UxBase.ENGINE_ANIM_SETS) if (type == null || types.Contains(type)) setRoots.Add(set);
        var c = UxBase.Closure(ents, roots, index, k => S.assets.Has(k) ? S.Read(k) : TA.Logical(by[k]), gl.Keys, AS.sets, setRoots, sess);
        Stat("closure: assets kept (base + additions)", c.kept.Count); Stat("closure: unresolved references", c.unresolved.Count); Stat("closure: animation sets expanded", c.sets.Count);
        Doc(L.lv.disc + " closure: " + c.kept.Count + " kept, " + c.unresolved.Count + " unresolved, " + c.glob.Count + " global", ClosureDoc(c, roots, setRoots, lit), L.Rel("closure.json"));
    }

    // ============================================================================================== ux_tuff
    static void TuffStatic(UxTest.Env env)
    {
        UxTest.Section("tuff: generate_material, sponsor_map, material_texture, untamed_sponsor");
        var want = RefJson("tuff_static.json");
        var gm = new List<object>();
        foreach (string m in new[] { "Single", "Double", "Wedge", "Corner" }) foreach (string w in UxTuff.POOL) gm.Add(new List<object> { m, w, UxBin.Md5(UxTuff.GenerateMaterial(env.stock, m, w)) });
        var sm = new JObj();
        foreach (var kv in UxJson.Obj(want.Get("sponsor_map")).items)
        {   // the key gives the inputs: '<short>/<wraps>/<n sponsors>' of the driver's sponsor list
            var parts = kv.Key.Split('/'); int n = int.Parse(parts[2], CultureInfo.InvariantCulture);
            var rows = new List<object>();
            foreach (var r in UxTuff.SponsorMap(parts[0], Sponsors.GetRange(0, n), parts[1])) rows.Add(new List<object> { r.Key, r.Value.wrap, r.Value.rule });
            sm.Set(kv.Key, rows);
        }
        var mt = new List<object>();
        foreach (string m in new[] { "Single", "Double", "Wedge", "Corner" }) { var t = UxTuff.MaterialTexture(UxTuff.GenerateMaterial(env.stock, m, "KTM")); mt.Add(new List<object> { m, t.first, Strs(t.all) }); }
        var us = new List<object>();
        foreach (string s in new[] { "HayBale_Single_Thor", "UX_HayBale_Double_FMF", "haybale_t1_Fox_Racing", "HayBale_Corner_", "Other", null, "HayBale_Wedge_A_B", "UX_UX_HayBale_Single_X" })
            us.Add(new List<object> { s, UxTuff.UntamedSponsor(s) });
        Doc("tuff static (" + gm.Count + " generated materials, " + sm.items.Count + " sponsor maps)", new JObj().Set("generated", gm).Set("sponsor_map", sm).Set("material_texture", mt).Set("untamed_sponsor", us),
            "tuff_static.json");
    }
    static readonly List<string> Sponsors = new List<string> { "Thor", "FMF", "smith", "RK Chains", "Monster", "RedBull", "Yamaha", "Honda", "Suzuki", "Kawasaki", "KTM", "Fox", "ONeal",
        "Dunlop", "Bridgestone", "Pirelli", "Michelin", "Maxxis", "Acerbis", "Answer", "DG", "", "Shift", "sidi", "CERNICS", "Hiper", "Toyota", "THQ", "Rainbow", "Xbox", "AMP", "Makita",
        "Parts Unlimited", "Pro-Circuit", "Scott", "Spy", "Troy Lee Designs" };

    static void Tuff(Level L)
    {
        var doc = new JObj();
        foreach (var (coll, wraps) in new[] { ("stock", "rule"), ("untamed", "rule"), ("stock", "fortdodge") })
        {
            var items = L.Items();
            var (rec, need) = UxTuff.Swap(items, L.lv, coll, wraps);
            string tag = coll + "_" + wraps;
            L.SameItems("tuff " + tag, "tuff_" + tag, items);
            var nl = new List<object>(); foreach (var k in need) nl.Add(Pair(k.name, k.type));
            doc.Set(tag, new JObj().Set("record", rec).Set("need", nl).Set("assets", UxTuff.Assets(L.env.stock, need, Paths).Rows()));
            if (tag == "stock_rule")
            {
                L.SameAsBuild("tuff record", "rollout.tuff", rec);
                Stat("tuff: bale bodies swapped", UxJson.Int(rec.Get("bodies"))); Stat("tuff: assets (stock + generated materials)", need.Count);
            }
        }
        Doc(L.lv.disc + " tuff: record, need and assets of 3 option sets (" + UxJson.Get(doc, "stock_rule", "record", "bodies") + " bodies)", doc, L.Rel("tuff.json"));
    }

    // ============================================================================================== ux_water
    static void Water(Level L)
    {
        var doc = new JObj();
        var opt = UxJson.Obj(L.info.Get("options"));
        double capOpt = opt.Get("ec_water_depth") is string ? 1e9 : UxJson.Num(opt.Get("ec_water_depth"));
        string korder = (string)opt.Get("ec_water_kit_order"), blook = opt.Get("ec_water_look") as string ?? "untamed";
        // the look of the reference build on disk (build_info.json): 'untamed' for the builds made before the water-look round
        string builtLook = UxJson.Get(L.BuildInfo(), "rollout", "ec_water", "look") as string ?? "untamed";
        // ... and its shore colour: 'ground' for the builds of the dirt-look rollout (a level with water records it), else the Untamed one
        bool builtGround = UxJson.Get(L.BuildInfo(), "rollout", "ec_water", "shore", "option") as string == "ground";
        bool builtChecked = false;
        foreach (var (tag, cap, order, look) in new[] { ("cap", capOpt, korder, blook), ("v4look", capOpt, korder, "untamed"), ("untamed", 1e9, "first", "untamed"),
            ("last", 1.25, "last", "untamed"), ("cap05", 0.5, "first", "arena") })
        {
            var r = UxWater.Build(L.ctx, null, 0, cap, order, Paths, null, look);
            L.SameElems("water " + tag, "water_" + tag + ".chunks", r.elems);
            doc.Set(tag, new JObj().Set("record", r.record).Set("assets", r.assets.Rows()).Set("entities", (long)r.elems.Count).Set("cap", cap).Set("kit_order", order).Set("look", look));
            if ((tag == "cap" || tag == "v4look") && look == builtLook && !builtChecked && !builtGround)
            {   // the builder adds 'splash' (--ec-water-splash on) to the record: not this call's
                L.SameAsBuild("water record (look " + look + ")", "rollout.ec_water", r.record, "splash,reflection");
                builtChecked = true;
            }
            if (tag == "cap") { Stat("water: surfaces (WaterSystemRSData)", r.elems.Count); Stat("water: assets (water, material, kit)", r.assets.Count); }
        }
        // the dirt-look rollout (DESIGN 23.11): the shore colour from the built ground, as UxBuildTrack.WaterGround gives it: 'ground' =
        // the build's own call (the dirt-look stock TopDown), 'ground_untamed' = the terrain stage's TopDown (--dirt-look untamed)
        bool hasWater = UxWater.HasWater(L.ctx.work);
        double S = (UxCommon.HF_SIZE - 1) * 1;
        foreach (string tag in new[] { "ground", "ground_untamed" })
        {
            UxWater.Ground ground = null;
            if (hasWater && tag == "ground")
            {
                byte[] td = L.Maps().topdown; var alb = new double[td.Length];
                for (int i = 0; i < td.Length; i++) alb[i] = td[i] / 255.0;
                ground = new UxWater.Ground { albedo = alb, n = UxDirtLook.G, S = S };
            }
            else if (hasWater)
            {
                var tj = L.ctx.work.ReadJsonObj(L.dirs["terrain"] + "/terrain.json");
                string td = UxJson.Get(tj, "ground", "default_topdown") as string ?? "topdown_occ.png";
                ground = new UxWater.Ground { albedo = UxDirtLook.OnClientRgb(UxImaging.ReadPngRgb(L.ctx.work.ReadBytes(L.dirs["terrain"] + "/" + td)), S), n = UxDirtLook.G, S = S };
            }
            var r = UxWater.Build(L.ctx, null, 0, capOpt, korder, Paths, null, "arena", "ground", ground);
            L.SameElems("water " + tag, "water_" + tag + ".chunks", r.elems);
            doc.Set(tag, new JObj().Set("record", r.record).Set("assets", r.assets.Rows()).Set("entities", (long)r.elems.Count).Set("cap", capOpt).Set("kit_order", korder).Set("look", "arena")
                .Set("shore", "ground"));
            if (tag == "ground" && builtGround) { L.SameAsBuild("water record (look arena, shore ground)", "rollout.ec_water", r.record, "splash,reflection"); builtChecked = true; }
            if (tag == "ground") foreach (object so in UxJson.Arr(r.record.Get("surfaces"))) if (UxJson.Obj(so).Has("shore")) Stat("water: shore colours from the built ground", 1);
        }
        if (!builtChecked) UxTest.Fail(L.lv.disc + " water: no call matched the reference build's record (look " + builtLook + ", shore " + (builtGround ? "ground" : "untamed") + ")");
        var wg = new List<object>();
        foreach (var (g, o) in new[] { (new[] { 0.3004, 0.25, 0.1985 }, (double[])null), (new[] { 0.3004, 0.25, 0.1985 }, new[] { 0.22, 0.2, 0.25 }), (new[] { 0.4, 0.35, 0.3 }, new[] { 0.2, 0.25, 0.3 }),
            (new[] { 0.9, 0.9, 0.2 }, (double[])null), (new[] { 0.05, 0.04, 0.03 }, new[] { 0.07, 0.07, 0.07 }), (new[] { 0.268, 0.2159, 0.1641 }, new[] { 0.1, 0.25, 0.2 }) })
        {
            var w = UxWater.WetGround(g, o);
            wg.Add(new List<object> { new List<object> { g[0], g[1], g[2] }, o == null ? null : new List<object> { o[0], o[1], o[2] }, new List<object> { Hex(w[0]), Hex(w[1]), Hex(w[2]) } });
        }
        doc.Set("wet_ground", wg);
        var ps = new JObj();
        foreach (object eo in UxJson.Arr(L.ctx.work.ReadJsonObj("level.json").Get("entities")))
        {
            var e = UxJson.Obj(eo);
            if (e.Get("type") as string != "WaterSystemRSData") continue;
            string un = (string)UxJson.Get(e, "props", "WaterAsset", "asset");
            var p = UxWater.Parse(L.ctx.disc.ReadExtract(un, "water"));
            ps.Set(un, new JObj().Set("endian", p.endian).Set("version", p.version).Set("matrix", p.matrix).Set("extent", new[] { p.extentX, p.extentZ }).Set("has_flow", p.has_flow)
                .Set("has_desc", p.has_desc).Set("desc_float", p.desc_float).Set("desc", p.desc == null ? null : new List<object> { (long)p.descW, (long)p.descH, UxBin.Md5(p.desc) })
                .Set("end", p.end).Set("size", p.size));
        }
        doc.Set("parse", ps);
        Doc(L.lv.disc + " water: 7 option sets (looks arena / untamed, shore untamed / ground), 6 wet_ground colours, " + ps.items.Count + " Untamed water assets parsed (" + UxJson.Arr(UxJson.Get(doc, "cap", "record", "surfaces")).Count + " surfaces)", doc, L.Rel("water.json"));
    }

    // ============================================================================================== ux_dirtlook
    /// float.hex() of Python.
    static string Hex(double v)
    {
        if (v == 0) return BitConverter.DoubleToInt64Bits(v) < 0 ? "-0x0.0p+0" : "0x0.0p+0";
        long bits = BitConverter.DoubleToInt64Bits(v); int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
        string sign = bits < 0 ? "-" : "";
        int e = be == 0 ? -1022 : be - 1023;
        return sign + "0x" + (be == 0 ? "0" : "1") + "." + frac.ToString("x13", CultureInfo.InvariantCulture) + "p" + (e >= 0 ? "+" : "") + e.ToString(CultureInfo.InvariantCulture);
    }
    static double[] F64(string path)
    {
        byte[] b = File.ReadAllBytes(path); var o = new double[b.Length / 8];
        Buffer.BlockCopy(b, 0, o, 0, o.Length * 8);
        return o;
    }
    static double[] MathInputs(int n, double lo, double hi, long mul)
    {
        var o = new double[n];
        for (long i = 0; i < n; i++) o[i] = lo + ((i * mul) % 1000003) / 1000003.0 * (hi - lo);
        return o;
    }
    /// |a - b| in units in the last place of b (0 = the same double).
    static long Ulps(double a, double b) => Math.Abs(BitConverter.DoubleToInt64Bits(a) - BitConverter.DoubleToInt64Bits(b));
    /// got against the C library's values numpy wrote: the count of equal doubles and the largest distance in ulps.
    static (long same, long maxUlp) Against(double[] got, double[] want)
    {
        long same = 0, mx = 0;
        for (int i = 0; i < want.Length; i++) { long u = Ulps(got[i], want[i]); if (u == 0) same++; if (u > mx) mx = u; }
        return (same, mx);
    }
    static string Md5Bools(bool[] a) { var b = new byte[a.Length]; for (int i = 0; i < a.Length; i++) b[i] = a[i] ? (byte)1 : (byte)0; return UxBin.Md5(b); }
    static string Md5Doubles(double[] a) { var b = new byte[a.Length * 8]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return UxBin.Md5(b); }

    /// The level-independent pieces of UxDirtLook. exp / log / pow: the reference reaches the C library through numpy; the port
    /// computes them in double-double arithmetic and rounds once (the same on every platform). The comparison says how often the two
    /// are the same double and requires that they never differ by more than one unit in the last place. hypot: equal (the C library's
    /// is sqrt(x * x + y * y) with every step rounded, which is what the port computes).
    static void DirtLookStatic()
    {
        UxTest.Section("dirtlook: the number rules against numpy");
        string dir = Path.Combine(Ref, "dirtlook");
        if (!File.Exists(Path.Combine(dir, "static.json"))) { UxTest.Fail("dirtlook: no reference values in " + dir); return; }
        var st = RefJson("dirtlook/static.json");
        int n = (int)UxJson.Int(st.Get("n"));
        var x = MathInputs(n, -7.0, 0.0, 7919); var got = new double[n];
        for (int i = 0; i < n; i++) got[i] = UxDirtLook.ExpExact(x[i]);
        var (same, mx) = Against(got, F64(Path.Combine(dir, "exp.f64")));
        UxTest.Report(mx <= 1, "ExpExact = np.exp on " + n + " values in [-7, 0): " + same + " the same double, the others " + mx + " ulp apart");
        long cs = 0; for (int i = 0; i < n; i++) if (got[i] == Math.Exp(x[i])) cs++;
        UxTest.Log("     (this runtime's Math.Exp: " + cs + " of " + n + " the same double as ExpExact)");
        var v = MathInputs(n, 0.5, 1.35, 104729);
        for (int i = 0; i < n; i++) got[i] = UxDirtLook.LogExact(v[i]);
        (same, mx) = Against(got, F64(Path.Combine(dir, "log.f64")));
        UxTest.Report(mx <= 1, "LogExact = np.log on " + n + " values in [0.5, 1.35): " + same + " the same double, the others " + mx + " ulp apart");
        var ks = UxJson.Arr(st.Get("pow_exponents"));
        for (int k = 0; k < ks.Count; k++)
        {
            double e = UxJson.Num(ks[k]);
            for (int i = 0; i < n; i++) got[i] = UxDirtLook.PowExact(v[i], e);
            (same, mx) = Against(got, F64(Path.Combine(dir, "pow_" + k + ".f64")));
            UxTest.Report(mx <= 1, "PowExact = v ** " + UxJson.Dumps(e, -1) + " on " + n + " values: " + same + " the same double, the others " + mx + " ulp apart");
        }
        // ... and against the correctly rounded values (Python's decimal, 45 digits) of the first 50,000 inputs: equal, on any platform
        double[] ecr = F64(Path.Combine(dir, "exp_cr.f64")), lcr = F64(Path.Combine(dir, "log_cr.f64")), pcr = F64(Path.Combine(dir, "pow_cr.f64"));
        double k0 = UxJson.Num(ks[0]); long be = 0, bl = 0, bp = 0;
        for (int i = 0; i < ecr.Length; i++)
        {
            if (UxDirtLook.ExpExact(x[i]) != ecr[i]) be++;
            if (UxDirtLook.LogExact(v[i]) != lcr[i]) bl++;
            if (UxDirtLook.PowExact(v[i], k0) != pcr[i]) bp++;
        }
        UxTest.Report(be == 0 && bl == 0 && bp == 0, "ExpExact, LogExact and PowExact (k " + UxJson.Dumps(k0, -1) + ") are the correctly rounded values on " + ecr.Length + " inputs each (decimal arithmetic; " +
            be + " / " + bl + " / " + bp + " differ)");
        double[] a = F64(Path.Combine(dir, "hypot_a.f64")), b = F64(Path.Combine(dir, "hypot_b.f64"));
        for (int i = 0; i < n; i++) got[i] = UxDirtLook.Hypot(a[i], b[i]);
        (same, mx) = Against(got, F64(Path.Combine(dir, "hypot.f64")));
        UxTest.Report(mx == 0, "Hypot = np.hypot on " + n + " pairs (2,000 on a half-foot lattice, 1,000 with one zero): " + same + " the same double" + (mx > 0 ? ", the others up to " + mx + " ulp apart" : ""));
        UxTest.Check(Hex(UxDirtLook.MASK_SCALE) == (string)st.Get("mask_scale"), "MASK_SCALE = the reference's (" + st.Get("mask_scale") + ")");
        foreach (var kv in UxJson.Obj(st.Get("client_index")).items)
        {
            var ix = UxDirtLook.ClientIndex(int.Parse(kv.Key, CultureInfo.InvariantCulture)); var bytes = new byte[ix.Length * 8];
            for (int i = 0; i < ix.Length; i++) UxBin.PutU32(bytes, 8 * i, (uint)ix[i]);
            UxTest.Check(UxBin.Md5(bytes) == (string)kv.Value, "ClientIndex(" + kv.Key + ") = client_index (2048 indices)");
        }
        // np.interp on the profile and the fractions np.round(np.linspace(-1, 1, 41), 3) are private: checked through the maps and the
        // corridor of every level; composite() on a synthetic 64 x 64 map
        var means = new[] { 0.30, 0.24, 0.18, 0.20, 0.17, 0.12, 0.41, 0.34, 0.27, 0.67, 0.56, 0.40 };
        var comp = new double[4096 * 3]; var rgb = new double[3];
        for (int i = 0; i < 4096; i++)
        {
            UxDirtLook.Composite((byte)(i * 7 % 256), (byte)(i * 13 % 256), (byte)(i * 29 % 256), (byte)(i * 5 % 256), (byte)(i * 11 % 256), means, rgb);
            comp[3 * i] = rgb[0]; comp[3 * i + 1] = rgb[1]; comp[3 * i + 2] = rgb[2];
        }
        UxTest.Check(Md5Doubles(comp) == (string)st.Get("composite"), "Composite = composite() on 4,096 synthetic texels (float64 bytes)");
    }

    static void DirtLook(Level L)
    {
        if (!File.Exists(L.RefPath("dirtlook.json"))) { UxTest.Fail(L.lv.disc + " dirtlook: no reference values"); return; }
        var want = RefJson(L.Rel("dirtlook.json"));
        var m = L.Maps();
        string d = L.lv.disc + " dirtlook: ";
        // the base HF material and the layer means
        var pj = new JObj(); var pars = new List<KeyValuePair<string, double[]>>(L.material.parameters); pars.Sort((p, q) => string.CompareOrdinal(p.Key, q.Key));
        foreach (var kv in pars) pj.Set(kv.Key, new List<object>(Array.ConvertAll(kv.Value, z => (object)z)));
        var mat = new JObj().Set("shader", L.material.shader).Set("textures", Strs(L.material.textures)).Set("params", pj);
        UxTest.Check(UxJson.DumpsUx(mat, 1) == UxJson.DumpsUx(want.Get("material"), 1), d + "material_textures of the base HF material (" + L.material.textures.Count + " samplers, " + pars.Count + " parameters)");
        var mh = new List<object>(); foreach (double z in L.means) mh.Add(Hex(z));
        UxTest.Check(UxJson.Dumps(mh, -1) == UxJson.Dumps(want.Get("means"), -1), d + "layer_means of the four detail textures (12 float64, exact)");
        // the masks and the signed distance
        UxTest.Check(UxBin.Md5(m.footprint) == (string)want.Get("footprint"), d + "the footprint on the client grid");
        UxTest.Check(Md5Bools(m.corridor) == (string)want.Get("corridor"), d + "corridor_mask (the tdf corridor, hypot, binary_closing): " + UxJson.Get(m.rec, "track", "corridor_px") + " px");
        UxTest.Check((m.lanes == null ? null : Md5Bools(m.lanes)) == want.Get("lanes") as string, d + "the painted lanes (" + UxJson.Get(m.rec, "track", "lanes_px") + " px)");
        UxTest.Check(Md5Bools(m.track) == (string)want.Get("track"), d + "the track region");
        UxTest.Check(Md5Doubles(m.sd) == (string)want.Get("sd"), d + "signed_distance (two distance transforms, float64 bytes)");
        // the variation: v, its median, std(log v), the exponent
        UxTest.Check(m.v != null && Md5Doubles(m.v) == (string)want.Get("v"), d + "the clipped macro ratio v (float64 bytes) and its track median " + Hex(m.median));
        UxTest.Check(Hex(m.median) == (string)want.Get("median"), d + "np.median of the ratio on the track (exact)");
        long inner = 0; for (int i = 0; i < m.sd.Length; i++) if (m.sd[i] < -4 && m.footprint[i] == 1) inner++;
        var lg = new double[inner]; long q = 0;
        for (int i = 0; i < m.sd.Length; i++) if (m.sd[i] < -4 && m.footprint[i] == 1) lg[q++] = UxDirtLook.LogExact(m.v[i]);
        var (same, mx) = Against(lg, F64(L.RefPath("dirtlook_log.f64")));
        UxTest.Report(inner == UxJson.Int(want.Get("inner_px")) && mx <= 1, d + "log v on the " + inner + " inner texels: " + same + " the same double as np.log, the others " + mx + " ulp apart");
        Stat("dirtlook: log values (inner texels)", inner); Stat("dirtlook: log values not the same double as numpy's", inner - same);
        bool slSame = Hex(m.stdLog) == (string)want.Get("std_log"), kSame = Hex(m.exponent) == (string)want.Get("exponent");
        long slU = Ulps(m.stdLog, ParseHex((string)want.Get("std_log"))), kU = Ulps(m.exponent, ParseHex((string)want.Get("exponent")));
        UxTest.Report(slU <= 4 && kU <= 4, d + "std(log v) " + Hex(m.stdLog) + (slSame ? " = " : " ~ (" + slU + " ulp) ") + "numpy's, the exponent " + Hex(m.exponent) + (kSame ? " = " : " ~ (" + kU + " ulp) ") + "numpy's");
        Stat("dirtlook: levels whose std(log v) is not numpy's double", slSame ? 0 : 1);
        if (File.Exists(L.RefPath("dirtlook_pow.f64")))
        {
            var pw = F64(L.RefPath("dirtlook_pow.f64")); double kk = ParseHex((string)want.Get("exponent")); var gp = new double[pw.Length];
            System.Threading.Tasks.Parallel.For(0, pw.Length, i => gp[i] = UxDirtLook.PowExact(m.v[i], kk));
            (same, mx) = Against(gp, pw);
            UxTest.Report(mx <= 1, d + "v ** k (numpy's k) on " + pw.Length + " texels: " + same + " the same double as numpy's, the others " + mx + " ulp apart");
            Stat("dirtlook: pow values", pw.Length); Stat("dirtlook: pow values not the same double as numpy's", pw.Length - same);
            Stat("dirtlook: pow values recomputed exactly by the build (their byte was within 1e-6 of a rounding step)", m.powExact);
            // the maps must not depend on the fast path (Math.Pow with the exact recomputation near a rounding step)
            UxDirtLook.ExactPow = true;
            try
            {
                var e2 = UxDirtLook.StockLook(L.ctx.work, L.dirs["terrain"], L.ctx.work.ReadJsonObj(L.dirs["terrain"] + "/terrain.json"), File.ReadAllBytes(L.RefPath("dirtlook.tdf")), L.means,
                    (UxCommon.HF_SIZE - 1) * 1, null, 1);
                UxTest.Report(UxBin.Same(e2.mask, m.mask) && UxBin.Same(e2.chromlum, m.chromlum) && UxBin.Same(e2.topdown, m.topdown) && UxJson.DumpsUx(e2.rec, 1) == UxJson.DumpsUx(m.rec, 1),
                    d + "the maps with every v ** k computed exactly (" + e2.powExact + " values, one thread) = the default (" + m.powExact + " recomputed)");
            }
            finally { UxDirtLook.ExactPow = false; }
        }
        // the maps, their textures, the record
        var maps = new JObj().Set("mask", UxBin.Md5(m.mask)).Set("chromlum", UxBin.Md5(m.chromlum)).Set("topdown", UxBin.Md5(m.topdown));
        UxTest.Report(UxJson.DumpsUx(maps, 1) == UxJson.DumpsUx(want.Get("maps"), 1), d + "the DetailMask, ChromLum and TopDown maps (2048 x 2048, md5 of the pixels; " + F(L.mapsSeconds) + " s)");
        var tex = new JObj();
        foreach (var (nm, arr, c, dxt5) in new[] { ("topdown", m.topdown, 3, false), ("mask", m.mask, 4, true), ("chromlum", m.chromlum, 4, true) })
        {
            byte[] t = UxBase.TexBc(new UxImage(arr, UxDirtLook.G, UxDirtLook.G, c), dxt5, true);
            tex.Set(nm, new List<object> { UxBin.Md5(t), (long)t.Length });
        }
        UxTest.Check(UxJson.DumpsUx(tex, 1) == UxJson.DumpsUx(want.Get("textures"), 1), d + "the three textures (tex_bc with straight alpha: DXT1 / DXT5, 12 mips)");
        var dd = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(m.rec, 1)), want.Get("record"));
        UxTest.Check(dd.Count == 0, d + "the record of stock_look (" + dd.Count + " differences)");
        for (int i = 0; i < dd.Count && i < 6; i++) UxTest.Log("        " + dd[i]);
        // ... and the maps of the reference BUILD (rollout.dirt_look.maps_md5)
        L.SameAsBuild("dirtlook maps", "rollout.dirt_look.maps_md5", maps);
        Stat("dirtlook: maps (2048 x 2048)", 3); Stat("dirtlook: track texels", UxJson.Int(UxJson.Get(m.rec, "track", "track_px")));
    }
    /// float.fromhex of Python (the form Hex writes).
    static double ParseHex(string s)
    {
        bool neg = s.StartsWith("-", StringComparison.Ordinal); if (neg) s = s.Substring(1);
        int p = s.IndexOf('p'); string mant = s.Substring(2, p - 2); int e = int.Parse(s.Substring(p + 1), CultureInfo.InvariantCulture);
        int dot = mant.IndexOf('.'); string frac = dot < 0 ? "" : mant.Substring(dot + 1); string lead = dot < 0 ? mant : mant.Substring(0, dot);
        double v = long.Parse(lead, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        double scale = 1.0 / 16;
        foreach (char c in frac) { v += Convert.ToInt32(c.ToString(), 16) * scale; scale /= 16; }
        v = v * Math.Pow(2.0, e);
        return neg ? -v : v;
    }

    // ============================================================================================== ux_screens
    static void ScreensLayout(UxTest.Env env)
    {
        UxTest.Section("screens: the layout data against the reference's pinned templates/screens/screens_layout.json");
        string p = Path.Combine(env.py, "templates", "screens", "screens_layout.json");
        if (!File.Exists(p)) { UxTest.Skip("no " + p); return; }
        byte[] data = File.ReadAllBytes(p);
        UxTest.Check(UxBin.Md5(data) == UxScreens.LAYOUT_MD5, "the reference's screens_layout.json has the pinned md5 " + UxScreens.LAYOUT_MD5);
        var want = UxJson.Obj(UxJson.ParseUtf8(data)); var got = UxScreens.Layout();
        int mats = 0, bad = 0;
        var d = UxTest.JsonDifferences(got.Get("overrides"), want.Get("overrides"));
        if (d.Count > 0) { bad++; UxTest.Log("     overrides: " + d[0]); }
        var wl = UxJson.Obj(want.Get("layout")); var gl = UxJson.Obj(got.Get("layout"));
        UxTest.Check(gl.items.Count == wl.items.Count, "the layout lists " + wl.items.Count + " levels");
        foreach (var lvl in wl.items)
        {
            var wm = UxJson.Obj(lvl.Value); var gm = gl.Get(lvl.Key) as JObj;
            if (gm == null || gm.items.Count != wm.items.Count) { bad++; UxTest.Log("     " + lvl.Key + ": materials differ"); continue; }
            for (int i = 0; i < wm.items.Count; i++)
            {
                mats++;
                if (wm.items[i].Key != gm.items[i].Key) { bad++; UxTest.Log("     " + lvl.Key + ": material order " + gm.items[i].Key + " / " + wm.items[i].Key); continue; }
                var w = (JObj)wm.items[i].Value; var g = (JObj)gm.items[i].Value;
                // exactly the fields ux_screens reads
                var sub = new JObj().Set("action", w.Get("action"));
                if (w.Get("action") as string != "still") sub.Set("reason", w.Get("reason"));
                else
                {
                    sub.Set("still", new JObj().Set("name", UxJson.Get(w, "still", "name")).Set("recipe", UxJson.Get(w, "still", "recipe"))).Set("address", w.Get("address"));
                    var light = UxJson.Obj(w.Get("light"));
                    if (light.Get("kind") as string == "existing") sub.Set("light", new JObj().Set("kind", "existing"));
                    else
                    {
                        var pr = UxJson.Obj(light.Get("recipe"));
                        sub.Set("light", new JObj().Set("kind", light.Get("kind")).Set("gain", light.Get("gain"))
                            .Set("recipe", new JObj().Set("size", pr.Get("size")).Set("lit_texels", pr.Get("lit_texels")).Set("dark_value", pr.Get("dark_value"))));
                    }
                    sub.Set("light_address", w.Get("light_address")).Set("mirror_u", w.Get("mirror_u")).Set("flip_v", w.Get("flip_v")).Set("canvas", w.Get("canvas")).Set("dst_px", w.Get("dst_px"));
                }
                var dd = UxTest.JsonDifferences(g, sub);
                if (dd.Count > 0) { bad++; UxTest.Log("     " + lvl.Key + "/" + wm.items[i].Key + ": " + dd[0]); }
            }
        }
        UxTest.Check(bad == 0, "UxScreens.Layout() equals the fields ux_screens reads of the pinned plan (" + mats + " materials, overrides; " + bad + " differ)");
    }

    static void Screens(Level L, UxTest.Env env)
    {
        var S = L.Sources();
        var doc = new JObj();
        var opt = UxJson.Obj(L.info.Get("options"));
        foreach (var (tag, gain) in new[] { ("default", UxJson.Num(opt.Get("screen_light"))), ("gain07", 0.7), ("gain1", 1.0) })
        {
            var r = UxScreens.Build(L.ctx, env.disc, n => S.material_entries.Get(n), gain, Paths);
            var mats = new List<object>();
            foreach (var kv in r.materials) mats.Add(new List<object> { kv.Key, UxBin.Md5(kv.Value.data), (long)kv.Value.data.Length, Strs(kv.Value.textures), kv.Value.template });
            doc.Set(tag, new JObj().Set("record", r.record).Set("materials", mats).Set("textures", r.textures.Rows()));
            if (tag != "default") continue;
            L.SameAsBuild("screens record", "rollout.screens", r.record);
            Stat("screens: still materials", r.materials.Count); Stat("screens: generated textures (stills, lights)", r.textures.Count);
            foreach (var kv in r.textures) UxTest.SameBytes(L.lv.disc + " screens: " + kv.Key.name + ".texture", kv.Value.data, File.ReadAllBytes(L.RefPath("screens/" + kv.Key.name + ".texture")));
            foreach (var kv in r.materials) UxTest.SameBytes(L.lv.disc + " screens: " + kv.Key + ".material", kv.Value.data, File.ReadAllBytes(L.RefPath("screens/" + kv.Key + ".material")));
        }
        Doc(L.lv.disc + " screens: record, materials and textures at 3 gains (" + UxJson.Arr(UxJson.Get(doc, "default", "materials")).Count + " materials, " +
            UxJson.Arr(UxJson.Get(doc, "default", "textures")).Count + " textures)", doc, L.Rel("screens.json"));
    }

    // ============================================================================================== ux_ads
    static void Ads(Level L, UxTemplateAssets TA)
    {
        var doc = new JObj();
        double lift = UxJson.Num(L.info.Get("lift"));
        string gn = (string)UxJson.Get(L.info, "options", "group_number");
        foreach (string mode in new[] { "keep", "model", "drop" })
        {
            var r = UxAds.Build(L.ctx, mode, L.matOpts, L.Sources(), TA, lift, gn, Paths);
            L.SameElems("ads " + mode, "ads_" + mode + ".chunks", r.elems);
            doc.Set(mode, new JObj().Set("record", r.record).Set("assets", r.assets.Rows()).Set("entities", (long)r.elems.Count));
            if (mode != "keep") continue;
            L.SameAsBuild("ads record", "rollout.massive_ads", r.record);
            Stat("ads: MassiveAd panels", r.elems.Count); Stat("ads: assets (model, surface, material, texture)", r.assets.Count);
            foreach (var kv in r.assets) UxTest.SameBytes(L.lv.disc + " ads: " + kv.Key.name + "." + kv.Key.type, kv.Value.data, File.ReadAllBytes(L.RefPath("ads/" + kv.Key.name + "." + kv.Key.type)));
        }
        Doc(L.lv.disc + " ads: keep / model / drop (" + UxJson.Arr(UxJson.Get(doc, "keep", "record", "panels")).Count + " panels, " + UxJson.Arr(UxJson.Get(doc, "keep", "assets")).Count + " assets)",
            doc, L.Rel("ads.json"));
    }

    // ============================================================================================== ux_splitgate
    static void SplitGate(Level L, UxTemplateAssets TA)
    {
        var want = RefJson(L.Rel("splitgate.json"));
        string dir = L.ctx.work.PathOf("splitgate");
        if (Directory.Exists(dir)) Directory.Delete(dir, true);
        double lift = UxJson.Num(L.info.Get("lift"));
        string gn = (string)UxJson.Get(L.info, "options", "group_number");
        UxSplitGate.Result r;
        try { r = UxSplitGate.Build(L.ctx, L.matOpts, L.Sources(), TA, lift, gn, Paths); }
        catch (UxException e) when (want.Has("error"))
        {
            UxTest.Report(true, L.lv.disc + " splitgate: both refuse (" + e.Message + " / " + want.Get("error") + ")");
            return;
        }
        if (want.Has("error")) { UxTest.Fail(L.lv.disc + " splitgate: Python raises SplitGateError (" + want.Get("error") + "), the port does not"); return; }
        L.SameElems("splitgate", "splitgate.chunks", r.elems);
        Stat("splitgate: levels built", 1); Stat("splitgate: assets", r.assets.Count);
        foreach (var kv in r.assets) UxTest.SameBytes(L.lv.disc + " splitgate: " + kv.Key.name + "." + kv.Key.type, kv.Value.data, File.ReadAllBytes(L.RefPath("splitgate_assets/" + kv.Key.name + "." + kv.Key.type)));
        // the reference's material folder was redirected into <ref>/<Level>/splitgate (see the header): map that prefix back. Inside
        // materials.json Python writes the folder relative to its root ('work/<Level>/splitgate/...'), in the record as absolute paths.
        string refDir = Path.GetFullPath(Path.Combine(Ref, L.lv.disc, "splitgate")).Replace('\\', '/'), pyDir = Paths.Work(L.lv, "splitgate");
        string od = UxSplitGate.OutDir(L.matOpts);
        string refOd = Path.Combine(Ref, L.lv.disc, od.Replace('/', Path.DirectorySeparatorChar));
        UxTest.CompareFolder(L.lv.disc + " splitgate: the files of " + od, L.ctx.work.PathOf(od), refOd,
            rel => !rel.EndsWith(".png", StringComparison.OrdinalIgnoreCase) && rel != "materials.json", false, true);
        byte[] mineMan = L.ctx.work.ReadBytes(od + "/materials.json"), redirected = File.ReadAllBytes(Path.Combine(refOd, "materials.json"));
        byte[] pyMan = Utf8.GetBytes(Utf8.GetString(redirected).Replace(refDir, "work/" + L.lv.disc + "/splitgate"));
        UxTest.SameJsonFile(L.lv.disc + " splitgate: " + od + "/materials.json", mineMan, pyMan);
        string Map(string s) => s.Replace(refDir, pyDir).Replace(UxBin.Md5(redirected), UxBin.Md5(pyMan));
        Doc(L.lv.disc + " splitgate: record, drop-gate props, " + r.assets.Count + " assets", new JObj().Set("record", r.record).Set("dropgate_props", r.dropgate_props)
            .Set("entities", (long)r.elems.Count).Set("assets", r.assets.Rows()), L.Rel("splitgate.json"), Map);
        // the build that uses the split gate: the folder the real builder wrote into the reference's work store, and its record
        if ((string)UxJson.Get(L.info, "options", "split_gate") != "untamed") return;
        string real = Path.Combine(L.env.PyWork(L.lv), od.Replace('/', Path.DirectorySeparatorChar));
        if (Directory.Exists(real))
            UxTest.CompareFolder(L.lv.disc + " splitgate: " + od + " against the reference work store (written by the reference build)", L.ctx.work.PathOf(od), real,
                rel => !rel.EndsWith(".png", StringComparison.OrdinalIgnoreCase), false, true);
        // rollout.split_gate = the builder's selection of the record's keys (+ its own 'assets_in_package', which is not this module's)
        var sel = new JObj();
        foreach (string k in new[] { "option", "entity_ids", "untamed_dropgate", "untamed_startinggate", "dropgate_model", "base_model", "dropgate_props", "base_entity",
            "materials", "materials_manifest", "assets", "tool_md5", "level_json_md5" }) sel.Set(k, r.record.Get(k));
        L.SameAsBuild("splitgate record", "rollout.split_gate", sel, "assets_in_package");
    }
}
