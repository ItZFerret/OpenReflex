// tests/untamed_convert/entities: the stage "entities" (UxEntities.cs, the port of ux_entities.py) against the Python reference, for the
// 12 levels, each tested alone on the reference's own upstream outputs (level.json, the meshes / materials / collision / terrain manifests):
//   A  default options            -> work/<Level>/entities.json, entities/sx_Base.chunks, sx_FreeRide.chunks      (files, byte for byte)
//   B  the current build's options (work/combined/commands.json: --type9 accept, Denton's --bale-trigger-move / --bale-reseat)
//                                  -> build/<name>/assembly/entities.json (the document the builder keeps; it holds the chunks' md5)
//   C  kind fr                     -> work/<Level>/entities_fr.json, entities/fr_Base.chunks, fr_FreeRide.chunks
//   D  option sets no stage output covers (legacy values, static bales, renames, stubs for missing manifests ...)
//                                  -> <scratch>/ref/variants/<variant>/<Level>.json + chunks, written by ref_entities.py
// A JSON document is compared as bytes after the reference's environment data is replaced: the absolute work folder prefix
// ('<reference>/work/<Level>/' in source.level_json, source.manifests.*.path, source.hf, freeride.hf_source and
// options.manifest_paths) is removed, and source.tool / source.tool_md5 (the Python file and its md5) take the port's values.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    sealed class Tally { public int files, identical, differing; public List<string> notes = new List<string>(); public double seconds; public string build; public byte[] json, baseChunks, frChunks; }
    static readonly Dictionary<string, Tally> s_tally = new Dictionary<string, Tally>();
    static Tally T(UxLevelRow lv) { if (!s_tally.TryGetValue(lv.disc, out var t)) { t = new Tally(); s_tally[lv.disc] = t; } return t; }

    static int Main(string[] args)
    {
        using (var env = UxTest.OpenEnv(args))
        {
            SelfChecks();
            if (env.disc == null || env.stock == null) { UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)"); return UxTest.Result("entities"); }
            string only = UxTest.Arg(args, "only"), refDir = UxTest.Arg(args, "ref", null, Path.Combine(env.scratch, "ref"));
            var builds = Builds(env.py);
            string toolMd5 = File.Exists(Path.Combine(env.py, "tools", "ux_entities.py")) ? UxBin.Md5File(Path.Combine(env.py, "tools", "ux_entities.py")) : null;
            foreach (var lv in UxCommon.Levels)
            {
                if (only != null && !string.Equals(only, lv.disc, StringComparison.OrdinalIgnoreCase) && !string.Equals(only, lv.shortName, StringComparison.OrdinalIgnoreCase)) continue;
                UxTest.Section(lv.disc);
                var ctx = env.Level(lv, "work");
                ctx.log = s => { };
                int seeded = Seed(env, ctx);
                string py = env.PyWork(lv), prefix = py.Replace('\\', '/').TrimEnd('/') + "/";
                var t = T(lv);

                // ---- A: default options, the stage's files
                var clock = System.Diagnostics.Stopwatch.StartNew();
                var ra = UxEntities.Convert(ctx, new UxEntitiesOptions());
                t.seconds = clock.Elapsed.TotalSeconds;
                UxTest.Check(ra.Ok, lv.disc + " default: problems " + ra.problems.Count + ", failed checks " + string.Join("; ", ra.failedChecks));
                Stale(lv, Path.Combine(py, "entities.json"), toolMd5, "entities.json");
                Json(lv, "default entities.json", ctx.work.ReadBytes("entities.json"), Path.Combine(py, "entities.json"), prefix);
                Bytes(lv, "default entities/sx_Base.chunks", ctx.work.ReadBytes("entities/sx_Base.chunks"), Path.Combine(py, "entities", "sx_Base.chunks"));
                Bytes(lv, "default entities/sx_FreeRide.chunks", ctx.work.ReadBytes("entities/sx_FreeRide.chunks"), Path.Combine(py, "entities", "sx_FreeRide.chunks"));
                t.json = ctx.work.ReadBytes("entities.json"); t.baseChunks = ra.chunks["Base"]; t.frChunks = ra.chunks["FreeRide"];
                // load_elems: the written entities.json rebuilds to the written chunks
                var items = UxEntities.LoadElems(ctx.work, env.stock, "sx", out JObj written);
                foreach (string sub in new[] { "Base", "FreeRide" })
                {
                    var els = new List<UxElem>(); foreach (var kv in items) if (kv.Key == sub) els.Add(kv.Value);
                    byte[] again = UxBin.Concat(UxEntities.EncodeChunks(els, new UxEntities.Prototypes(env.stock)).ToArray());
                    UxTest.Check(UxBin.Same(again, ctx.work.ReadBytes("entities/sx_" + sub + ".chunks")), lv.disc + ": entities.json -> spec_to_elem -> chunks == the written " + sub + " chunks");
                }

                // ---- B: the options of the current build
                if (builds.TryGetValue(lv.disc, out var build))
                {
                    if (build.reference == null) UxTest.Skip(lv.disc + ": no build/" + build.name + "/assembly/entities.json (nor an earlier version's)");
                    else
                    {
                        var o = build.options; o.write = false;
                        var rb = UxEntities.Convert(ctx.disc, ctx.stock, ctx.work, o, null, default);
                        UxTest.Check(rb.Ok, lv.disc + " build: problems " + rb.problems.Count + ", failed checks " + string.Join("; ", rb.failedChecks));
                        string what = "build " + Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(build.reference))) + (build.fallback ? " (the latest build there is; commands.json names " + build.name + ")" : "") +
                            " assembly/entities.json [" + build.text + "]";
                        Stale(lv, build.reference, toolMd5, what);
                        t.build = what;
                        Json(lv, what, Encoding.UTF8.GetBytes(UxJson.DumpsUx(rb.doc)), build.reference, prefix);
                        var rd = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(build.reference)));
                        foreach (string sub in new[] { "Base", "FreeRide" })
                        {
                            t.files++;
                            string want = UxJson.Get(rd, "chunks", sub, "md5") as string;
                            if (UxTest.Check(UxBin.Md5(rb.chunks[sub]) == want && rb.chunks[sub].Length == UxJson.Int(UxJson.Get(rd, "chunks", sub, "bytes")), lv.disc + " build " + sub + " chunks: md5 " + UxBin.Md5(rb.chunks[sub]) + ", want " + want)) t.identical++;
                            else { t.differing++; t.notes.Add("build " + sub + " chunks"); }
                        }
                    }
                }

                // ---- C: kind fr
                if (File.Exists(Path.Combine(py, "entities_fr.json")) && ctx.work.Exists("level_fr.json"))
                {
                    var rc = UxEntities.Convert(ctx, new UxEntitiesOptions { kind = "fr" });
                    UxTest.Check(rc.Ok, lv.disc + " fr: problems " + rc.problems.Count + ", failed checks " + string.Join("; ", rc.failedChecks));
                    Stale(lv, Path.Combine(py, "entities_fr.json"), toolMd5, "entities_fr.json");
                    Json(lv, "fr entities_fr.json", ctx.work.ReadBytes("entities_fr.json"), Path.Combine(py, "entities_fr.json"), prefix);
                    Bytes(lv, "fr entities/fr_Base.chunks", ctx.work.ReadBytes("entities/fr_Base.chunks"), Path.Combine(py, "entities", "fr_Base.chunks"));
                    Bytes(lv, "fr entities/fr_FreeRide.chunks", ctx.work.ReadBytes("entities/fr_FreeRide.chunks"), Path.Combine(py, "entities", "fr_FreeRide.chunks"));
                }

                // ---- D: option sets of ref_entities.py
                foreach (var v in Variants())
                {
                    if (Array.IndexOf(v.levels, lv.disc) < 0) continue;
                    string rj = Path.Combine(refDir, "variants", v.name, lv.disc + ".json");
                    if (!File.Exists(rj)) { UxTest.Skip(lv.disc + " variant " + v.name + ": no reference " + rj + " (ref_entities.py writes it)"); continue; }
                    var o = v.options(); o.write = false;
                    UxEntitiesResult rv;
                    try { rv = UxEntities.Convert(ctx.disc, ctx.stock, ctx.work, o, null, default); }
                    catch (Exception e) { UxTest.Fail(lv.disc + " variant " + v.name + ": " + e.Message); t.files += 3; t.differing += 3; t.notes.Add("variant " + v.name + " threw"); continue; }
                    Json(lv, "variant " + v.name, Encoding.UTF8.GetBytes(UxJson.DumpsUx(rv.doc)), rj, prefix);
                    foreach (string sub in new[] { "Base", "FreeRide" })
                        Bytes(lv, "variant " + v.name + " " + sub + " chunks", rv.chunks[sub], Path.Combine(refDir, "variants", v.name, lv.disc + "." + sub + ".chunks"));
                }
                UxTest.Log("     " + lv.disc + ": " + seeded + " upstream files seeded; " + t.files + " files compared, " + t.identical + " identical, " + t.differing + " differing; default run " +
                    t.seconds.ToString("0.00", CultureInfo.InvariantCulture) + " s (" + UxJson.Int(UxJson.Get(ra.doc, "counts", "specs")) + " specs)");
                ctx.disc.ClearCache();
            }
            // ---- the levels at the same time (the builder may run stages of several tracks in parallel): the same documents and chunks
            UxTest.Section("parallel runs");
            var done = new List<UxLevelRow>(); foreach (var lv in UxCommon.Levels) if (s_tally.ContainsKey(lv.disc)) done.Add(lv);
            var same = new bool[done.Count]; var errors = new string[done.Count];
            var wall = System.Diagnostics.Stopwatch.StartNew();
            System.Threading.Tasks.Parallel.For(0, done.Count, i =>
            {
                try
                {
                    var lv = done[i]; var t = T(lv);
                    var r = UxEntities.Convert(env.disc.Level(lv), env.stock, UxWork.ForLevel(Path.Combine(env.scratch, "work"), lv), new UxEntitiesOptions { write = false }, null, default);
                    same[i] = UxBin.Same(Encoding.UTF8.GetBytes(UxJson.DumpsUx(r.doc)), t.json) && UxBin.Same(r.chunks["Base"], t.baseChunks) && UxBin.Same(r.chunks["FreeRide"], t.frChunks);
                }
                catch (Exception e) { errors[i] = e.Message; }
            });
            for (int i = 0; i < done.Count; i++) UxTest.Check(same[i], done[i].disc + ": the run in parallel with the other levels gives the same document and chunks" + (errors[i] != null ? " (" + errors[i] + ")" : ""));
            UxTest.Log("     " + done.Count + " levels in parallel: " + wall.Elapsed.TotalSeconds.ToString("0.00", CultureInfo.InvariantCulture) + " s");
            UxTest.Section("summary");
            UxTest.Log("level                  compared identical differing  seconds  differing files");
            foreach (var lv in UxCommon.Levels)
                if (s_tally.TryGetValue(lv.disc, out var t))
                    UxTest.Log(lv.disc.PadRight(22) + t.files.ToString().PadLeft(9) + t.identical.ToString().PadLeft(10) + t.differing.ToString().PadLeft(10) +
                        t.seconds.ToString("0.00", CultureInfo.InvariantCulture).PadLeft(9) + "  " + string.Join(", ", t.notes));
            foreach (var lv in UxCommon.Levels) if (s_tally.TryGetValue(lv.disc, out var t) && t.build != null) UxTest.Log("     " + lv.disc + ": " + t.build);
        }
        return UxTest.Result("entities");
    }

    // ------------------------------------------------------------------------------------------ comparisons
    /// The reference document with its environment data replaced (see the header).
    static byte[] Normalise(byte[] reference, string prefix)
    {
        string t = Encoding.UTF8.GetString(reference).Replace(prefix, "");
        t = t.Replace("\"tool\": \"ux_entities.py\"", "\"tool\": \"" + UxEntities.TOOL + "\"");
        t = Regex.Replace(t, "\"tool_md5\": \"[0-9a-f]{32}\"", "\"tool_md5\": null");
        return Encoding.UTF8.GetBytes(t);
    }
    static void Json(UxLevelRow lv, string what, byte[] got, string referenceFile, string prefix)
    {
        var t = T(lv); t.files++;
        if (!File.Exists(referenceFile)) { UxTest.Fail(lv.disc + " " + what + ": no reference " + referenceFile); t.differing++; t.notes.Add(what + " (no reference)"); return; }
        byte[] want = Normalise(File.ReadAllBytes(referenceFile), prefix);
        if (UxTest.SameJsonFile(lv.disc + " " + what, got, want) == 0) t.identical++; else { t.differing++; t.notes.Add(what); }
    }
    static void Bytes(UxLevelRow lv, string what, byte[] got, string referenceFile)
    {
        var t = T(lv); t.files++;
        if (!File.Exists(referenceFile)) { UxTest.Fail(lv.disc + " " + what + ": no reference " + referenceFile); t.differing++; t.notes.Add(what + " (no reference)"); return; }
        if (UxTest.SameBytes(lv.disc + " " + what, got, File.ReadAllBytes(referenceFile))) t.identical++; else { t.differing++; t.notes.Add(what); }
    }
    /// Says when a reference document was written by another version of ux_entities.py than the one in the reference's tools folder.
    static void Stale(UxLevelRow lv, string referenceFile, string toolMd5, string what)
    {
        if (toolMd5 == null || !File.Exists(referenceFile)) return;
        string m = UxJson.Get(UxJson.ParseUtf8(File.ReadAllBytes(referenceFile)), "source", "tool_md5") as string;
        if (m != toolMd5) UxTest.Log("note " + lv.disc + " " + what + ": written by ux_entities.py " + m + ", the current tool is " + toolMd5);
    }

    // ------------------------------------------------------------------------------------------ inputs
    /// The upstream outputs the stage reads, copied from the reference's work/<Level> (not the big .surface / texture files).
    static int Seed(UxTest.Env env, UxContext ctx)
    {
        string src = env.PyWork(ctx.lv); int n = 0;
        void One(string rel) { string p = Path.Combine(src, rel.Replace('/', Path.DirectorySeparatorChar)); if (File.Exists(p)) { ctx.work.WriteBytes(rel, File.ReadAllBytes(p)); n++; } }
        void Glob(string dir, string pattern)
        {
            string p = Path.Combine(src, dir.Replace('/', Path.DirectorySeparatorChar));
            if (!Directory.Exists(p)) return;
            foreach (string f in Directory.GetFiles(p, pattern)) { ctx.work.WriteBytes(dir + "/" + Path.GetFileName(f), File.ReadAllBytes(f)); n++; }
        }
        One("level.json"); One("level_fr.json"); One("meshes/meshes.json"); Glob("meshes", "*.model"); One("materials/materials.json");
        foreach (string c in new[] { "collision", "collision-t9", "collision-prims22" }) { One(c + "/collision.json"); Glob(c, "*.collis"); }
        One("terrain/terrain.json"); One("terrain/hf.npy"); One("terrain/excluded_tris.npy"); Glob("terrain/parts", "*.model"); Glob("terrain/parts", "*.collis");
        return n;
    }

    sealed class Build { public string name, reference, text; public bool fallback; public UxEntitiesOptions options; }
    /// work/combined/commands.json: each track's ux_build_track.py command -> the options the builder passes to ux_entities.build_entities
    /// (ux_build_track.Builder.build: manifest_paths = the collision folder of --prims-to-22 / --type9 and materials/).
    static Dictionary<string, Build> Builds(string pyRoot)
    {
        var r = new Dictionary<string, Build>();
        string file = Path.Combine(pyRoot, "work", "combined", "commands.json");
        if (!File.Exists(file)) { UxTest.Skip("no " + file + ": the build options are not tested"); return r; }
        foreach (var kv in UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(file))).items)
        {
            if (kv.Key.StartsWith("_", StringComparison.Ordinal) || !(kv.Value is List<object> cmds)) continue;
            string cmd = null; foreach (object c in cmds) if (c is string s && s.Contains("ux_build_track.py")) { cmd = s; break; }
            if (cmd == null) continue;
            string[] a = cmd.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            string Val(string flag) { for (int i = 0; i + 1 < a.Length; i++) if (a[i] == flag) return a[i + 1]; return null; }
            bool Has(string flag) => Array.IndexOf(a, flag) >= 0;
            string level = Val("--level"), kind = Val("--kind") ?? "sx";
            if (level == null) continue;
            string cdir = "collision" + (Has("--prims-to-22") ? "-prims22" : "") + ((Val("--type9") ?? "refuse") != "refuse" ? "-t9" : "");
            var o = new UxEntitiesOptions
            {
                kind = kind, static_bales = Has("--static-bales"), static_pop = Val("--static-pop") ?? "all", crowd_off = Has("--crowd-off"), drop_gate_top = Has("--drop-gate-top"),
                crowd_tier = Val("--crowd-tier") ?? "stock", group_number = Val("--group-number") ?? "zero", hb_freeride = Val("--hb-freeride") ?? "drop", buried = Val("--buried") ?? "drop",
                fr_clearance = Val("--fr-clearance") ?? "strict", bale_trigger_move = kind == "sx" ? UxEntitiesOptions.ParseMove(Val("--bale-trigger-move")) : null,
                bale_reseat = kind == "sx" && Val("--bale-reseat") != null ? UxJson.ParseDouble(Val("--bale-reseat")) : 0.0,
                manifest_paths = new Dictionary<string, string> { { "collision", cdir + "/collision.json" }, { "materials", "materials/materials.json" } },
            };
            var b = new Build { name = kv.Key, options = o, text = cdir + (o.bale_trigger_move != null ? ", bale-trigger-move " + Val("--bale-trigger-move") : "") + (o.bale_reseat != 0 ? ", bale-reseat " + Val("--bale-reseat") : "") };
            string p = Path.Combine(pyRoot, "build", kv.Key, "assembly", "entities.json");
            if (File.Exists(p)) b.reference = p;
            else
            {   // the build of commands.json is not there (yet): the latest earlier version of the same track
                int dash = kv.Key.LastIndexOf("-v", StringComparison.Ordinal);
                if (dash > 0 && int.TryParse(kv.Key.Substring(dash + 2), out int ver))
                    for (int v = ver - 1; v >= 1 && b.reference == null; v--)
                    {
                        p = Path.Combine(pyRoot, "build", kv.Key.Substring(0, dash) + "-v" + v, "assembly", "entities.json");
                        if (File.Exists(p)) { b.reference = p; b.fallback = true; }
                    }
            }
            r[UxCommon.Level(level).disc] = b;
        }
        return r;
    }

    sealed class Variant { public string name; public string[] levels; public Func<UxEntitiesOptions> options; }
    static Dictionary<string, string> Missing(params string[] keys)
    {
        var d = new Dictionary<string, string>(); foreach (string k in keys) d[k] = "missing/" + (k == "hf" ? "hf.npy" : k + ".json"); return d;
    }
    /// The table of ref_entities.py (VARIANTS).
    static List<Variant> Variants() => new List<Variant>
    {
        new Variant { name = "legacy", levels = new[] { "Norton_STADIUM", "WestStone_STADIUM", "Lincoln_STADIUM" },
            options = () => new UxEntitiesOptions { crowd_tier = "untamed", group_number = "copy", hb_freeride = "base", buried = "keep", fr_clearance = "legacy" } },
        new Variant { name = "static-bales", levels = new[] { "Norton_STADIUM", "Wilford_STADIUM" }, options = () => new UxEntitiesOptions { static_bales = true } },
        new Variant { name = "static-pop-initial", levels = new[] { "Norton_STADIUM" }, options = () => new UxEntitiesOptions { static_bales = true, static_pop = "initial" } },
        new Variant { name = "static-pop-drop", levels = new[] { "Denton_STADIUM" }, options = () => new UxEntitiesOptions { static_bales = true, static_pop = "drop", crowd_off = true } },
        new Variant { name = "editor-male", levels = new[] { "Wilford_STADIUM", "Fairview_STADIUM" }, options = () => new UxEntitiesOptions { static_male = "editor_default" } },
        new Variant { name = "gate-top", levels = new[] { "Foothills_STADIUM", "Norton_STADIUM" }, options = () => new UxEntitiesOptions { drop_gate_top = true } },
        new Variant { name = "hb-freeride", levels = new[] { "Norton_STADIUM" }, options = () => new UxEntitiesOptions { hb_freeride = "freeride" } },
        new Variant { name = "renames", levels = new[] { "Wilford_STADIUM" }, options = () => new UxEntitiesOptions { renames = new JObj()
            .Set("material", new JObj().Set("MrGray", "UX_MrGray")).Set("model", new JObj().Set("Stadiums_Box", "UX_Stadiums_Box"))
            .Set("surface", new JObj().Set("Stadiums_Box", "UX_Stadiums_Box_S")).Set("collis", new JObj().Set("Ambulance", "UX_Ambulance")) } },
        new Variant { name = "reseat-columbus", levels = new[] { "Columbus_STADIUM" }, options = () => new UxEntitiesOptions { bale_reseat = 6.0, bale_trigger_move = new[] { 12.5, -3.25 } } },
        new Variant { name = "stub-collision", levels = new[] { "Wilford_STADIUM", "Lincoln_STADIUM" }, options = () => new UxEntitiesOptions { manifest_paths = Missing("collision") } },
        new Variant { name = "stub-meshes", levels = new[] { "Norton_STADIUM" }, options = () => new UxEntitiesOptions { manifest_paths = Missing("meshes") } },
        new Variant { name = "stub-hf", levels = new[] { "Wilford_STADIUM", "Lincoln_STADIUM", "Denton_STADIUM" }, options = () => new UxEntitiesOptions { hf_path = "missing/hf.npy", bale_reseat = 6.0 } },
        new Variant { name = "stub-all", levels = new[] { "Wilford_STADIUM", "Fairview_STADIUM", "BridgePort_STADIUM" },
            options = () => new UxEntitiesOptions { manifest_paths = Missing("meshes", "collision", "terrain", "materials", "hf") } },
    };

    // ------------------------------------------------------------------------------------------ the Python rules the port carries itself
    static void SelfChecks()
    {
        UxTest.Section("numpy generator and Python formatting");
        // np.random.default_rng(7): random(4); choice(5, size=6, p=[0.1, 0.2, 0.3, 0.25, 0.15]) then random((2, 2))
        var rng = new UxEntities.Pcg64(7);
        double[] want = { 0.625095466604667, 0.8972138009695755, 0.7756856902451935, 0.22520718999059186 };
        bool ok = true; foreach (double w in want) if (rng.NextDouble() != w) ok = false;
        UxTest.Check(ok, "default_rng(7).random(4)");
        rng = new UxEntities.Pcg64(7);
        int[] pick = rng.Choice(new[] { 0.1, 0.2, 0.3, 0.25, 0.15 }, 6); int[] wantPick = { 3, 4, 3, 1, 2, 4 };
        ok = true; for (int i = 0; i < 6; i++) if (pick[i] != wantPick[i]) ok = false;
        double[] after = { 0.005265304565574724, 0.8212284183827663, 0.7970694287520462, 0.4679349528437208 };
        foreach (double w in after) if (rng.NextDouble() != w) ok = false;
        UxTest.Check(ok, "default_rng(7).choice(p) and the randoms after it");
        // '%g', '%+g', '%.2e', '%.2f', '%.1f', '%.0f' (CPython 3.12)
        object[,] f =
        {
            { 1.9073486328125e-06, "1.90735e-06", "+1.90735e-06", "1.91e-06", "0.00", "0.0", "0" },
            { 0.0, "0", "+0", "0.00e+00", "0.00", "0.0", "0" },
            { 1e-05, "1e-05", "+1e-05", "1.00e-05", "0.00", "0.0", "0" },
            { 123456789.0, "1.23457e+08", "+1.23457e+08", "1.23e+08", "123456789.00", "123456789.0", "123456789" },
            { 0.0001, "0.0001", "+0.0001", "1.00e-04", "0.00", "0.0", "0" },
            { 2.675, "2.675", "+2.675", "2.67e+00", "2.67", "2.7", "3" },
            { 0.25, "0.25", "+0.25", "2.50e-01", "0.25", "0.2", "0" },
            { 0.5, "0.5", "+0.5", "5.00e-01", "0.50", "0.5", "0" },
            { 1.5, "1.5", "+1.5", "1.50e+00", "1.50", "1.5", "2" },
            { 2.0005, "2.0005", "+2.0005", "2.00e+00", "2.00", "2.0", "2" },
            { 413.31, "413.31", "+413.31", "4.13e+02", "413.31", "413.3", "413" },
            { -0.0, "-0", "-0", "-0.00e+00", "-0.00", "-0.0", "-0" },
            { 9.9995, "9.9995", "+9.9995", "1.00e+01", "10.00", "10.0", "10" },
            { 99999.95, "99999.9", "+99999.9", "1.00e+05", "99999.95", "99999.9", "100000" },
            { 999999.5, "1e+06", "+1e+06", "1.00e+06", "999999.50", "999999.5", "1000000" },
            { 0.000123456789, "0.000123457", "+0.000123457", "1.23e-04", "0.00", "0.0", "0" },
            { 66.0, "66", "+66", "6.60e+01", "66.00", "66.0", "66" },
            { 3.0199999809265137, "3.02", "+3.02", "3.02e+00", "3.02", "3.0", "3" },
            { 1e+22, "1e+22", "+1e+22", "1.00e+22", "10000000000000000000000.00", "10000000000000000000000.0", "10000000000000000000000" },
            { 123.456, "123.456", "+123.456", "1.23e+02", "123.46", "123.5", "123" },
        };
        for (int i = 0; i < f.GetLength(0); i++)
        {
            double v = (double)f[i, 0];
            string got = UxEntities.Py.G(v) + " " + UxEntities.Py.G(v, true) + " " + UxEntities.Py.E(v, 2) + " " + UxEntities.Py.F(v, 2) + " " + UxEntities.Py.F(v, 1) + " " + UxEntities.Py.F(v, 0);
            string wantText = f[i, 1] + " " + f[i, 2] + " " + f[i, 3] + " " + f[i, 4] + " " + f[i, 5] + " " + f[i, 6];
            UxTest.Check(got == wantText, "formats of " + PyFormat.Repr(v) + ": " + got + ", want " + wantText);
        }
        string reprs = UxEntities.Py.Repr("PopUpBaleTrigger") + " " + UxEntities.Py.Repr("it's") + " " + UxEntities.Py.Str(new List<object> { 1L, 2L }) + " " + UxEntities.Py.Str(new List<object> { "a", "b" }) + " " +
            UxEntities.Py.Str(new object[] { 1L, 0.18, null, "terrain part" }) + " " +
            UxEntities.Py.Str(new JObj().Set("layout", "-x").Set("blocked", new List<object> { new object[] { 1L, 0.5, 3L } }));
        UxTest.Check(reprs == "'PopUpBaleTrigger' \"it's\" [1, 2] ['a', 'b'] (1, 0.18, None, 'terrain part') {'layout': '-x', 'blocked': [(1, 0.5, 3)]}", "str / repr: " + reprs);
        UxTest.Check(UxEntities.Py.Round(2.675, 2) == 2.67 && UxEntities.Py.Round(0.125, 2) == 0.12 && UxEntities.Py.Round(1e-7, 3) == 0.0 && UxEntities.Py.Round(2489.449999, 1) == 2489.4, "round()");
    }
}
