// tests/untamed_convert/textures: the "textures" stage (UxTextures.cs, the port of ux_textures.py) against the Python reference.
//   stage      all 12 levels from the disc, the work store seeded with Python's level.json: every <asset>.texture of
//              work/<Level>/textures/ byte for byte, textures.json byte for byte after options.out_dir (an absolute path in the reference,
//              the store-relative folder here) is set to the reference's value; no file the reference does not have
//   threads    one thread against all processors: the same files
//   texmax     --tex-max / --npot floor (no current build uses them) against manifests of the current Python code (ref_textures.py)
//   generated  ux_textures.generated_texture for every recipe of the reference's materials.json (ref_textures.py)
//   rgba       pc_rgba (the PC decoder) on the stage's outputs; grey_dxn_facts on every DXN closure texture (ref_textures.py)
//   profile    (only when asked: profile --level <Level>) where the time goes on one thread: disc reads, conversion, check
// Arguments: [stage|threads|texmax|generated|rgba|all] --scratch <dir> --ref <dir of ref_textures.py's output> + the inputs of UxTest.OpenEnv.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static readonly System.Globalization.CultureInfo Inv = System.Globalization.CultureInfo.InvariantCulture;
    /// The manifest fields that are environment data: excluded from the comparison by name (set to the reference's value).
    static readonly string[] Excluded = { "options.out_dir" };

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "all";
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null || env.stock == null) { UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)"); return UxTest.Result("textures"); }
            if (!env.stock.templateNames.ContainsKey("sx") || !env.stock.templateNames.ContainsKey("fr")) { UxTest.Skip("the template name tables (reference templates folder)"); return UxTest.Result("textures"); }
            string refDir = UxTest.Arg(args, "ref", "UX_REF", Path.Combine(env.scratch, "ref"));
            bool all = what == "all";
            Tools(env, refDir);
            if (all || what == "stage") Stage(env);
            if (all || what == "threads") Threads(env);
            if (all || what == "texmax") TexMax(env, refDir);
            if (all || what == "generated") Generated(env, refDir);
            if (all || what == "rgba") Rgba(env, refDir);
            if (what == "profile") Profile(env, UxTest.Arg(args, "level", null, "Wilford_STADIUM"));
        }
        return UxTest.Result("textures");
    }

    /// The port names the reference versions it reproduces: they must be the reference's current files.
    static void Tools(UxTest.Env env, string refDir)
    {
        UxTest.Section("reference versions");
        string a = Path.Combine(env.py, "tools", "ux_textures.py"), b = Path.Combine(env.py, "tools", "uxtex.py");
        if (!File.Exists(a) || !File.Exists(b)) { UxTest.Skip("the reference tools (" + a + ")"); return; }
        UxTest.Report(UxBin.Md5File(a) == UxTextures.REFERENCE_TOOL_MD5, "ux_textures.py is the version the port reproduces (" + UxTextures.REFERENCE_TOOL_MD5 + ")");
        UxTest.Report(UxBin.Md5File(b) == UxTextures.REFERENCE_UXTEX_MD5, "uxtex.py is the version the port reproduces (" + UxTextures.REFERENCE_UXTEX_MD5 + ")");
    }

    /// The produced manifest with the excluded fields set to the reference's values, as the bytes ux_common.write_json writes.
    static byte[] Patched(JObj mine, JObj reference)
    {
        UxJson.Obj(mine.Get("options")).Set("out_dir", UxJson.Obj(reference.Get("options")).Get("out_dir"));
        return new UTF8Encoding(false).GetBytes(UxJson.DumpsUx(mine, 1));
    }

    static void Stage(UxTest.Env env)
    {
        UxTest.Section("stage: 12 levels against work/<Level>/textures");
        var rows = new List<string>(); double total = 0; long totalBytes = 0;
        foreach (var lv in UxCommon.Levels)
        {
            var ctx = env.Level(lv, "work");
            ctx.log = s => { };
            env.Seed(ctx, "level.json");
            string pyDir = Path.Combine(env.PyWork(lv), "textures");
            if (!File.Exists(Path.Combine(pyDir, "textures.json"))) { UxTest.Skip(lv.disc + ": no reference output " + pyDir); continue; }
            int calls = 0, last = 0, count = 0; bool ordered = true;
            var sw = Stopwatch.StartNew();
            JObj man;
            try { man = UxTextures.Convert(ctx, new UxTexturesOptions { throwOnProblems = false }, (done, n) => { calls++; ordered &= done == last + 1; last = done; count = n; }); }
            catch (Exception e) { UxTest.Fail(lv.disc + ": the stage failed: " + e); continue; }
            double sec = sw.Elapsed.TotalSeconds; total += sec;
            UxTest.Check(ordered && calls == count && last == count, lv.disc + ": progress called once per texture, in order (" + calls + " of " + count + ")");
            string outDir = ctx.work.PathOf("textures");
            var r = UxTest.CompareFolder(lv.disc + " textures/*.texture", outDir, pyDir, rel => rel.EndsWith(".texture", StringComparison.OrdinalIgnoreCase), false, true);
            // textures.json: byte for byte except the excluded fields
            byte[] want = File.ReadAllBytes(Path.Combine(pyDir, "textures.json")), got = ctx.work.ReadBytes("textures/textures.json");
            var refMan = UxJson.Obj(UxJson.ParseUtf8(want));
            byte[] patched = Patched(UxJson.Obj(UxJson.ParseUtf8(got)), refMan);
            bool json = UxTest.SameJsonFile(lv.disc + " textures/textures.json (out_dir excluded)", patched, want) == 0;
            // nothing but the manifest and the textures in the folder
            int others = 0; foreach (string f in Directory.GetFiles(outDir)) if (!f.EndsWith(".texture", StringComparison.OrdinalIgnoreCase) && Path.GetFileName(f) != "textures.json") others++;
            UxTest.Check(others == 0, lv.disc + ": " + others + " unexpected files in textures/");
            var problems = UxJson.Arr(man.Get("problems"));
            UxTest.Check(problems.Count == 0, lv.disc + ": " + problems.Count + " problems in the manifest" + (problems.Count > 0 ? " (" + problems[0] + ")" : ""));
            long bytes = UxJson.Int(UxJson.Get(man, "totals", "pc_bytes")); totalBytes += bytes;
            rows.Add(string.Format(Inv, "{0,-22} {1,4} textures  {2,4}/{3,-4} files identical  {4} different  {5} missing  {6} extra  textures.json {7}  {8,6:0.0} MB  {9,5:0.00} s",
                lv.disc, UxJson.Int(UxJson.Get(man, "totals", "textures")), r.identical, r.reference, r.different, r.missing, r.extra, json ? "identical" : "DIFFERS", bytes / 1e6, sec));
        }
        UxTest.Log("");
        foreach (string row in rows) UxTest.Log("  " + row);
        UxTest.Log(string.Format(Inv, "  total {0:0.0} MB in {1:0.00} s ({2} threads); excluded manifest fields: {3}; not written: sheet.png", totalBytes / 1e6, total, Environment.ProcessorCount, string.Join(", ", Excluded)));
    }

    /// Where the time goes (one thread): reading the textures from the disc, converting, checking.
    static void Profile(UxTest.Env env, string name)
    {
        UxTest.Section("profile " + name);
        var lv = UxCommon.Level(name); var ctx = env.Level(lv, "work-profile"); env.Seed(ctx, "level.json");
        var clos = UxJson.Obj(UxJson.Get(ctx.work.ReadJsonObj("level.json"), "closure", "texture"));
        var sw = new Stopwatch(); double read = 0, conv = 0, check = 0; long bytes = 0;
        var slow = new List<(double, string)>();
        foreach (var kv in clos.items)
        {
            sw.Restart(); byte[] data = ctx.disc.ReadExtractFile(UxJson.Str(UxJson.Obj(kv.Value).Get("file"))); double r = sw.Elapsed.TotalSeconds; read += r; bytes += data.Length;
            sw.Restart(); var c = UxTextures.ConvertTexture(data, kv.Key, 0, "nearest", null, env.stock.templateNames); double cv = sw.Elapsed.TotalSeconds; conv += cv;
            sw.Restart(); if (c.pc != null) UxTextures.CheckTexture(data, c.pc, c.rec, c.expect, out _); double ck = sw.Elapsed.TotalSeconds; check += ck;
            slow.Add((r + cv + ck, string.Format(Inv, "{0} {1} {2}x{3} m{4}: read {5:0.000} convert {6:0.000} check {7:0.000}", kv.Key, c.rec.Get("fmt_in"), c.rec.Get("w_in"), c.rec.Get("h_in"), c.rec.Get("mips_in"), r, cv, ck)));
        }
        slow.Sort((a, b) => b.Item1.CompareTo(a.Item1));
        UxTest.Log(string.Format(Inv, "  {0} textures, {1:0.0} MB source: read {2:0.00} s, convert {3:0.00} s, check {4:0.00} s", clos.items.Count, bytes / 1e6, read, conv, check));
        for (int i = 0; i < 8 && i < slow.Count; i++) UxTest.Log("    " + slow[i].Item2);
    }

    static void Threads(UxTest.Env env)
    {
        UxTest.Section("threads: 1 thread against all processors");
        foreach (string name in new[] { "Capital_City_STADIUM", "HarrisPark_STADIUM" })
        {
            var lv = UxCommon.Level(name);
            var a = env.Level(lv, "work-serial"); a.log = s => { }; env.Seed(a, "level.json");
            var b = env.Level(lv, "work-parallel"); b.log = s => { }; env.Seed(b, "level.json");
            var sw = Stopwatch.StartNew();
            UxTextures.Convert(a, new UxTexturesOptions { threads = 1, throwOnProblems = false });
            double serial = sw.Elapsed.TotalSeconds; sw.Restart();
            UxTextures.Convert(b, new UxTexturesOptions { throwOnProblems = false });
            double parallel = sw.Elapsed.TotalSeconds;
            var r = UxTest.CompareFolder(name + " textures/ with 1 thread = with " + Environment.ProcessorCount, a.work.PathOf("textures"), b.work.PathOf("textures"), null, false, true);
            UxTest.Log(string.Format(Inv, "     {0}: {1:0.00} s on 1 thread, {2:0.00} s on {3}", name, serial, parallel, Environment.ProcessorCount));
        }
        // a cancelled token stops the stage
        {
            var lv = UxCommon.Level("Lincoln_STADIUM");
            var c = env.Level(lv, "work-cancel"); c.log = s => { }; env.Seed(c, "level.json");
            using (var cts = new System.Threading.CancellationTokenSource())
            {
                c.cancel = cts.Token; bool cancelled = false; int seen = 0;
                try { UxTextures.Convert(c, new UxTexturesOptions(), (done, n) => { seen = done; if (done == 5) cts.Cancel(); }); }
                catch (OperationCanceledException) { cancelled = true; }
                UxTest.Check(cancelled && !c.work.Exists("textures/textures.json"), "cancel: the stage stops with OperationCanceledException and writes no manifest (stopped after " + seen + " textures)");
            }
        }
    }

    static void TexMax(UxTest.Env env, string refDir)
    {
        UxTest.Section("texmax: --tex-max / --npot against the current Python code");
        string dir = Path.Combine(refDir, "texmax");
        if (!Directory.Exists(dir) || Directory.GetFiles(dir, "*.json").Length == 0) { UxTest.Skip("no reference manifests in " + dir + " (ref_textures.py)"); return; }
        foreach (string f in Directory.GetFiles(dir, "*.json"))
        {
            string tag = Path.GetFileNameWithoutExtension(f);                    // <Level>-max<N>-<npot>
            var refMan = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(f)));
            var lv = UxCommon.Level(UxJson.Str(refMan.Get("level")));
            int texMax = (int)UxJson.Int(UxJson.Get(refMan, "options", "tex_max")); string npot = UxJson.Str(UxJson.Get(refMan, "options", "npot"));
            var ctx = env.Level(lv, "work-" + tag.Substring(lv.disc.Length + 1)); ctx.log = s => { }; env.Seed(ctx, "level.json");
            UxTextures.Convert(ctx, new UxTexturesOptions { texMax = texMax, npot = npot, throwOnProblems = false });
            string outRel = UxTextures.OutDirFor(texMax);
            byte[] got = ctx.work.ReadBytes(outRel + "/textures.json");
            bool json = UxTest.SameJsonFile(tag + " textures.json (out_dir excluded)", Patched(UxJson.Obj(UxJson.ParseUtf8(got)), refMan), File.ReadAllBytes(f)) == 0;
            // the reference wrote no files (write=False): each file against the md5 and size of its reference record
            int files = 0, bad = 0;
            foreach (var kv in UxJson.Obj(refMan.Get("textures")).items)
            {
                var r = UxJson.Obj(kv.Value);
                if (!(r.Get("file") is string file)) continue;
                files++;
                if (!ctx.work.Exists(outRel + "/" + file)) { bad++; continue; }
                byte[] d = ctx.work.ReadBytes(outRel + "/" + file);
                if (UxBin.Md5(d) != UxJson.Str(r.Get("md5")) || d.Length != UxJson.Int(r.Get("bytes"))) bad++;
            }
            int onDisk = ctx.work.List(outRel, "*.texture").Count;
            UxTest.Report(json && bad == 0 && onDisk == files, tag + ": manifest " + (json ? "identical" : "DIFFERS") + ", " + (files - bad) + " of " + files + " files have the reference md5, " + onDisk + " files written");
        }
    }

    static void Generated(UxTest.Env env, string refDir)
    {
        UxTest.Section("generated: ux_textures.generated_texture on the recipes of the reference's materials.json");
        string dir = Path.Combine(refDir, "generated");
        int levels = 0, n = 0, same = 0, infoSame = 0; var kinds = new SortedDictionary<string, int>(StringComparer.Ordinal); int reported = 0;
        foreach (var lv in UxCommon.Levels)
        {
            string f = Path.Combine(dir, lv.disc + ".json");
            if (!File.Exists(f)) { UxTest.Skip("no " + f + " (ref_textures.py)"); continue; }
            levels++;
            var disc = env.disc.Level(lv);
            foreach (var kv in UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(f))).items)
            {
                var r = UxJson.Obj(kv.Value); var recipe = UxJson.Obj(r.Get("recipe"));
                string kind = UxJson.Str(recipe.Get("kind")); kinds[kind] = (kinds.TryGetValue(kind, out int c) ? c : 0) + 1;
                n++;
                byte[] src = r.Get("source_file") is string sf ? disc.ReadExtractFile(sf) : null;
                byte[] data; JObj info;
                try { data = UxTextures.GeneratedTexture(src, recipe, out info); }
                catch (Exception e) { if (reported++ < UxTest.MaxReports) UxTest.Log("     " + lv.disc + " " + kv.Key + ": " + e.Message); continue; }
                bool okBytes = UxBin.Md5(data) == UxJson.Str(r.Get("md5")) && data.Length == UxJson.Int(r.Get("size"));
                var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(info)), r.Get("info"));
                if (okBytes) same++;
                if (d.Count == 0) infoSame++;
                if ((!okBytes || d.Count > 0) && reported++ < UxTest.MaxReports)
                    UxTest.Log("     " + lv.disc + " " + kv.Key + " (" + kind + "): bytes " + (okBytes ? "identical" : "DIFFER") + (d.Count > 0 ? "; info: " + d[0] : ""));
            }
        }
        var ks = new List<string>(); foreach (var kv in kinds) ks.Add(kv.Key + " " + kv.Value);
        UxTest.Report(n > 0 && same == n && infoSame == n, "generated textures of " + levels + " levels: " + same + " of " + n + " byte-identical (md5, size), " + infoSame + " with the identical info (" + string.Join(", ", ks) + ")");
    }

    static void Rgba(UxTest.Env env, string refDir)
    {
        UxTest.Section("rgba: pc_rgba on the stage's outputs, grey_dxn_facts");
        string dir = Path.Combine(refDir, "rgba");
        foreach (string f in Directory.Exists(dir) ? Directory.GetFiles(dir, "*.json") : new string[0])
        {
            var lv = UxCommon.Level(Path.GetFileNameWithoutExtension(f));
            string pyDir = Path.Combine(env.PyWork(lv), "textures");
            int n = 0, same = 0; var fmts = new SortedDictionary<string, int>(StringComparer.Ordinal); int reported = 0;
            foreach (var kv in UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(f))).items)
            {
                byte[] data = File.ReadAllBytes(Path.Combine(pyDir, kv.Key));
                var ents = UxTextures.PcEntries(data, out _);
                foreach (object o in UxJson.Arr(kv.Value))
                {
                    var pair = UxJson.Arr(o); int e = (int)UxJson.Int(pair[0]);
                    n++; fmts[ents[e].fmt] = (fmts.TryGetValue(ents[e].fmt, out int c) ? c : 0) + 1;
                    string got = UxBin.Md5(UxTextures.PcRgba(data, e).data);
                    if (got == UxJson.Str(pair[1])) same++;
                    else if (reported++ < 6) UxTest.Log("     " + lv.disc + " " + kv.Key + " entry " + e + " (" + ents[e].fmt + " " + ents[e].w + "x" + ents[e].h + "): decoded image differs");
                }
            }
            var fs = new List<string>(); foreach (var kv in fmts) fs.Add(kv.Key + " " + kv.Value);
            UxTest.Report(n > 0 && same == n, lv.disc + ": pc_rgba of " + same + " of " + n + " entries identical (" + string.Join(", ", fs) + ")");
        }
        if (!Directory.Exists(dir)) UxTest.Skip("no " + dir + " (ref_textures.py)");
        string g = Path.Combine(refDir, "grey.json");
        if (!File.Exists(g)) { UxTest.Skip("no " + g + " (ref_textures.py)"); return; }
        {
            int n = 0, same = 0, grey = 0; UxLevelRow cur = null; UxDiscLevel disc = null;
            foreach (var kv in UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(g))).items)
            {
                int slash = kv.Key.IndexOf('/'); var lv = UxCommon.Level(kv.Key.Substring(0, slash));
                if (lv != cur) { cur = lv; disc = env.disc.Level(lv); }
                var facts = UxTextures.GreyDxnFacts(disc.ReadExtractFile(kv.Key.Substring(slash + 1)));
                n++;
                object mine = facts == null ? null : UxJson.Parse(UxJson.DumpsUx(facts.ToDict()));
                if (facts != null && facts.grey) grey++;
                var d = UxTest.JsonDifferences(mine, kv.Value);
                if (d.Count == 0) same++; else if (n - same <= 6) UxTest.Log("     " + kv.Key + ": " + d[0]);
            }
            UxTest.Report(n > 0 && same == n, "grey_dxn_facts of " + same + " of " + n + " DXN closure textures identical (" + grey + " grey)");
        }
    }
}
