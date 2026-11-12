// tests/untamed_convert/materials: stage S4 (UxMaterials + UxShaderProg) against the Python reference (ux_materials.py, ux_shaderprog.py).
//   shaders    every Untamed .shader of the 12 levels: listing, literals, alpha sampler and the four program questions (alpha output,
//              albedo scale, fetch UV, composite) equal ref_shaderprog.py's digest (<ref>/shaderprog/<Level>.json)
//   materials  the stage run from the disc for the 12 levels on Python's own upstream files (level.json, terrain/material_requests.json
//              + its textures, textures/textures.json); every file of work/<Level>/materials/ compared: binary files byte for byte,
//              materials.json byte for byte (documents compared when the bytes differ). materials_sheet.png (a preview) is not written.
//              Also the entry points the builder will call: Planner.Plan and RebuildEntry against the manifest and the written files.
//   fresh      the same on the reference's current inputs against a fresh in-memory run of the current ux_materials.py
//              (ref_materials.py; the stored outputs were checked against the textures.json of the build before them)
//   variants   non-default switch sets: the reference's stored variant folders that are current (Norton materials-safe) and fresh
//              in-memory runs of the 16 switch sets of the reference's selftest (ref_materials.py --variants)
//   parallel   the 12 levels converted at once: the same bytes as the serial run
//   pins, fmt  the reference tool md5s the manifest records; Python's number formatting ('%g', '%.3f', repr, round) on 20,050 values
// Arguments: [pins|fmt|shaders|materials|fresh|variants|parallel|all] --levels a,b --ref <dir> --jobs N (+ UxTest.OpenEnv's)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static string F(double v) => v.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture);

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--") ? args[0] : "all";
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null || env.stock == null) { UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)"); return UxTest.Result("materials"); }
            var levels = new List<UxLevelRow>();
            string only = UxTest.Arg(args, "levels");
            foreach (var lv in UxCommon.Levels) if (only == null || Array.IndexOf(only.Split(','), lv.shortName) >= 0 || Array.IndexOf(only.Split(','), lv.disc) >= 0) levels.Add(lv);
            string refDir = UxTest.Arg(args, "ref", null, Path.Combine(env.scratch, "ref"));
            UxTest.Log("library: " + env.library.Describe + "; levels: " + levels.Count);
            if (what == "all" || what == "pins") Pins(env);
            if (what == "all" || what == "fmt") Fmt(refDir);
            if (what == "all" || what == "shaders") Shaders(env, levels, refDir);
            if (what == "all" || what == "materials") Materials(env, levels, int.Parse(UxTest.Arg(args, "jobs", null, "0")));
            if (what == "all" || what == "fresh") Fresh(env, levels, refDir);
            if (what == "all" || what == "variants") Variants(env, refDir);
            if (what == "all" || what == "parallel") Parallel12(env, levels);
        }
        return UxTest.Result("materials");
    }

    // ------------------------------------------------------------------------------------------ the reference tools' md5s
    static void Pins(UxTest.Env env)
    {
        UxTest.Section("reference identity (the tool md5s materials.json records)");
        var o = new UxMaterialsOptions();
        string tools = Path.Combine(env.py, "tools");
        if (!File.Exists(Path.Combine(tools, "ux_materials.py"))) { UxTest.Skip("no reference tools folder " + tools); return; }
        bool ok = UxBin.Md5File(Path.Combine(tools, "ux_materials.py")) == o.tool_md5;
        foreach (var kv in o.readers) ok &= UxBin.Md5File(Path.Combine(tools, kv.Key)) == kv.Value;
        ok &= UxBin.Md5File(Path.Combine(env.py, "templates", "reflex-lib", "sm3dis.py")) == o.sm3dis_md5;
        ok &= UxBin.Md5File(Path.Combine(env.py, "templates", "reflex-lib", "index.json")) == o.lib_index_md5;
        UxTest.Report(ok, "the md5s UxMaterialsOptions carries (ux_materials.py, 6 readers, sm3dis.py, reflex-lib index.json) are those of the reference's current files");
    }

    // ------------------------------------------------------------------------------------------ Python number formatting
    /// float.fromhex of float.hex().
    static double FromHex(string h)
    {
        bool neg = h.StartsWith("-"); if (neg) h = h.Substring(1);
        int p = h.IndexOf('p'); int e = int.Parse(h.Substring(p + 1), System.Globalization.CultureInfo.InvariantCulture);
        string m = h.Substring(2, p - 2); int dot = m.IndexOf('.');
        string digits = dot < 0 ? m : m.Substring(0, dot) + m.Substring(dot + 1);
        int frac = dot < 0 ? 0 : m.Length - dot - 1;
        double v = (double)System.Numerics.BigInteger.Parse("0" + digits, System.Globalization.NumberStyles.HexNumber) * Math.Pow(2, e - 4 * frac);
        if (double.IsInfinity(v) || v == 0 && digits.Trim('0').Length > 0) v = (double)System.Numerics.BigInteger.Parse("0" + digits, System.Globalization.NumberStyles.HexNumber) * Math.Pow(2, -4 * frac) * Math.Pow(2, e);
        return neg ? -v : v;
    }
    static void Fmt(string refDir)
    {
        UxTest.Section("Python number formatting ('%g', '%+g', '%.3f', '%.0f', repr, round)");
        string rf = Path.Combine(refDir, "fmt.json");
        if (!File.Exists(rf)) { UxTest.Skip("no reference " + rf + " (ref_fmt.py writes it)"); return; }
        int n = 0, bad = 0; string first = null;
        foreach (object row in UxJson.Arr(UxJson.ParseUtf8(File.ReadAllBytes(rf))))
        {
            var r = UxJson.Arr(row); double x = FromHex((string)r[0]); n++;
            string[] got = { UxPyFmt.G(x), UxPyFmt.G(x, true), UxPyFmt.F(x, 3), UxPyFmt.F(x, 0), UxPyFmt.Repr(x) };
            bool ok = BitConverter.DoubleToInt64Bits(UxNp.PyRound(x, 6)) == BitConverter.DoubleToInt64Bits(FromHex((string)r[6])) && BitConverter.DoubleToInt64Bits(UxNp.PyRound(x, 3)) == BitConverter.DoubleToInt64Bits(FromHex((string)r[7]));
            for (int i = 0; i < got.Length; i++) if (got[i] != (string)r[1 + i]) ok = false;
            if (!ok) { bad++; if (first == null) first = (string)r[0] + ": got " + string.Join(" | ", got) + ", want " + r[1] + " | " + r[2] + " | " + r[3] + " | " + r[4] + " | " + r[5]; }
        }
        UxTest.Report(bad == 0, n + " values: " + bad + " differ from Python" + (first != null ? " (first " + first + ")" : ""));
    }

    // ------------------------------------------------------------------------------------------ ux_shaderprog
    static object Terms(UxSpFetchUv f)
    {
        if (f.terms == null) return null;
        var o = new List<object>(); foreach (var t in f.terms) o.Add(new List<object> { t.a, t.b, t.v });
        return o;
    }
    static JObj Kv(List<KeyValuePair<char, double>> d) { var j = new JObj(); foreach (var kv in d) j.Set("" + kv.Key, kv.Value); return j; }

    /// What ref_shaderprog.py writes for one shader file.
    static JObj ShaderDigest(byte[] data)
    {
        var ent = new JObj();
        try
        {
            var rs = UxShaderProg.ReadShader(data);
            foreach (var pr in UxShaderProg.Programs(data))
            {
                var lines = UxShaderProg.Disassemble(pr.blob);
                ent.Set(pr.name, new JObj().Set("kind", pr.kind).Set("lines", (long)lines.Count).Set("md5", UxBin.Md5(System.Text.Encoding.UTF8.GetBytes(string.Join("\n", lines)))));
            }
            foreach (var e in rs)
            {
                var j = (JObj)ent.Get(e.name);
                var smp = new JObj(); foreach (var kv in e.samplers) smp.Set(kv.Key.ToString(), kv.Value);
                j.Set("pixel", e.pixel).Set("target", e.target).Set("samplers", smp).Set("floats", new List<object>(e.floats));
                if (!e.pixel) continue;
                j.Set("alpha_sampler", e.alpha_sampler_error != null ? (object)"error" : e.alpha_sampler == null ? null : (object)(long)e.alpha_sampler.Value);
                if (e.literals_error != null) j.Set("literals", "error");
                else { var lit = new JObj(); foreach (var kv in e.literals) lit.Set(kv.Key.ToString(), new List<object> { kv.Value[0], kv.Value[1], kv.Value[2], kv.Value[3] }); j.Set("literals", lit); }
                UxShaderInfo info = null;
                try { info = UxShaderProg.ShaderInfo(data, e.name); } catch (UxException) { j.Set("info", "error"); continue; }
                if (info == null) { j.Set("info", null); continue; }
                try
                {
                    var ao = UxShaderProg.AlphaOutput(info);
                    j.Set("alpha_output", ao == null ? null : ao.kind == "const" ? new JObj().Set("kind", "const").Set("value", ao.value).Set("raw", ao.raw)
                        : ao.kind == "texture" ? new JObj().Set("kind", "texture").Set("sampler", (long)ao.sampler).Set("channel", "" + ao.channel).Set("scale", ao.scale).Set("offset", ao.offset).Set("sat", ao.sat).Set("raw", ao.raw)
                        : new JObj().Set("kind", "other").Set("raw", ao.raw));
                }
                catch (UxException) { j.Set("alpha_output", "error"); }
                try
                {
                    var al = UxShaderProg.AlbedoScale(info, new int[0]);
                    j.Set("albedo", new JObj().Set("rgb", al.rgb == null ? null : new List<object> { al.rgb[0], al.rgb[1], al.rgb[2] }).Set("how", al.how).Set("fog_only", al.fog_only));
                }
                catch (UxException) { j.Set("albedo", "error"); }
                var uv = new List<object>();
                for (int k = 0; k < 8; k++)
                {
                    try { var f = UxShaderProg.FetchUv(info, k); uv.Add(f.kind == null ? null : new List<object> { f.kind, Terms(f), f.coord }); }
                    catch (UxException) { uv.Add("error"); }
                }
                j.Set("fetch_uv", uv);
                var comp = new List<object>(); var dest = new List<object>();
                for (int k = 0; k < 4; k++)
                {
                    try
                    {
                        var c = UxShaderProg.AnalyseComposite(info, k, info.literals);
                        comp.Add(c == null ? null : new JObj().Set("bg", Kv(c.bg)).Set("k", Kv(c.k)).Set("lerp", c.lerp).Set("lines", new List<object>(c.lines)).Set("program", c.program));
                    }
                    catch (UxException) { comp.Add("error"); }
                    try { dest.Add(UxShaderProg.FetchDest(info, k)); } catch (UxException) { dest.Add("error"); }
                }
                j.Set("composite", comp).Set("fetch_dest", dest);
            }
        }
        catch (UxException) { ent = new JObj().Set("error", true); }
        return ent;
    }

    static void Shaders(UxTest.Env env, List<UxLevelRow> levels, string refDir)
    {
        UxTest.Section("ux_shaderprog on every Untamed shader");
        int total = 0, programs = 0;
        foreach (var lv in levels)
        {
            string rf = Path.Combine(refDir, "shaderprog", lv.disc + ".json");
            if (!File.Exists(rf)) { UxTest.Skip(lv.disc + ": no reference digest " + rf + " (ref_shaderprog.py writes it)"); continue; }
            var want = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(rf)));
            var disc = env.disc.Level(lv);
            var assets = disc.Find("shader");
            var got = new JObj[assets.Count];
            Parallel.For(0, assets.Count, i => got[i] = ShaderDigest(disc.Read(assets[i])));
            var doc = new JObj();
            for (int i = 0; i < assets.Count; i++)
            {
                string name = assets[i].file.Substring(assets[i].file.LastIndexOf('/') + 1);
                doc.Set(name, got[i]); total++; programs += got[i].items.Count;
            }
            var d = UxTest.JsonDifferences(doc, want);
            UxTest.Check(d.Count == 0, lv.disc + ": " + assets.Count + " shaders: " + d.Count + " differences from ux_shaderprog.py" + (d.Count > 0 ? "\n     " + string.Join("\n     ", d.GetRange(0, Math.Min(8, d.Count))) : ""));
        }
        UxTest.Log("     " + total + " shader files, " + programs + " programs compared");
    }

    // ------------------------------------------------------------------------------------------ the stage
    /// The stage's upstream files, from the reference: level.json, the terrain stage's material requests and their textures, and the
    /// textures stage's manifest (which ux_materials only cross-checks: 'textures.json (WP2) has no entry for X' warnings).
    /// asStored: the reference runs materials BEFORE textures, so a stored materials.json was checked against the textures.json of the
    /// build before it. Where the stored manifest says which entries that older file lacked (its own WP2 warnings), the seeded copy
    /// drops them, so the stage sees the input state the stored output was made from. Returns the number of entries dropped.
    static int SeedInputs(UxTest.Env env, UxContext ctx, bool asStored = false)
    {
        env.Seed(ctx, "level.json");
        foreach (string rel in new[] { "terrain/material_requests.json", "terrain/textures", "textures/textures.json" })
        {
            string src = Path.Combine(env.PyWork(ctx.lv), rel.Replace('/', Path.DirectorySeparatorChar));
            if (File.Exists(src) || Directory.Exists(src)) env.Seed(ctx, rel);
        }
        if (!asStored || !ctx.work.Exists("textures/textures.json")) return 0;
        string stored = Path.Combine(env.PyWork(ctx.lv), "materials", "materials.json");
        if (!File.Exists(stored)) return 0;
        const string mark = "textures.json (WP2) has no entry for ";
        var lacked = new HashSet<string>(StringComparer.Ordinal);
        foreach (object w in UxJson.Arr(UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(stored))).Get("warnings")))
            if (w is string s && s.StartsWith(mark, StringComparison.Ordinal)) lacked.Add(UxCommon.PyLower(s.Substring(mark.Length)));
        if (lacked.Count == 0) return 0;
        var tj = ctx.work.ReadJsonObj("textures/textures.json");
        var tex = UxJson.Obj(tj.Get("textures"));
        int dropped = tex.items.RemoveAll(kv => lacked.Contains(UxCommon.PyLower(kv.Key)));
        ctx.work.WriteJson("textures/textures.json", tj);
        return dropped;
    }
    static JObj RequestOf(UxContext ctx, string name)
    {
        if (!ctx.work.Exists("terrain/material_requests.json")) return null;
        var r = UxJson.Get(ctx.work.ReadJson("terrain/material_requests.json"), "requests", name) as JObj;
        if (r == null) return null;
        var o = new JObj(); foreach (var kv in r.items) o.Set(kv.Key, kv.Value);
        o.Set("name", name); if (o.Get("source") == null) o.Set("source", o.Get("source_material"));
        return o;
    }
    static bool Counted(string rel) => !rel.EndsWith(".png", StringComparison.OrdinalIgnoreCase);     // materials_sheet.png: a preview, not ported

    static void Materials(UxTest.Env env, List<UxLevelRow> levels, int jobs)
    {
        UxTest.Section("materials: the 12 levels from the disc against work/<Level>/materials");
        UxTest.Log("     level                    files  identical  different  missing  extra   materials.json   seconds");
        double sum = 0; int files = 0, same = 0;
        foreach (var lv in levels)
        {
            var ctx = env.Level(lv, "work"); ctx.log = s => { };
            int dropped = SeedInputs(env, ctx, true);
            if (dropped > 0) UxTest.Log("     " + lv.disc + ": the stored materials.json was made against an older textures.json that lacked " + dropped + " entries (its WP2 warnings); seeded so");
            ctx.disc.ClearCache();
            var sw = Stopwatch.StartNew();
            JObj man;
            try { man = UxMaterials.Convert(ctx, new UxMaterialsOptions { jobs = jobs }); }
            catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); continue; }
            double secs = sw.Elapsed.TotalSeconds; sum += secs;
            string produced = ctx.work.PathOf("materials"), reference = Path.Combine(env.PyWork(lv), "materials");
            var r = UxTest.CompareFolder(lv.disc, produced, reference, Counted, false, true);
            byte[] a = File.Exists(Path.Combine(produced, "materials.json")) ? File.ReadAllBytes(Path.Combine(produced, "materials.json")) : null, b = File.ReadAllBytes(Path.Combine(reference, "materials.json"));
            string js = a != null && UxBin.Same(a, b) ? "identical bytes" : "DIFFERENT";
            UxTest.Log("     " + lv.disc.PadRight(24) + r.reference.ToString().PadLeft(6) + r.identical.ToString().PadLeft(11) + r.different.ToString().PadLeft(11) + r.missing.ToString().PadLeft(9) + r.extra.ToString().PadLeft(7) +
                "   " + js.PadRight(15) + F(secs).PadLeft(9));
            files += r.reference; same += r.identical;
            UxTest.Check(UxJson.Arr(man.Get("problems")).Count == 0, lv.disc + ": the manifest has no problems");
            // the entry points the builder will call: rebuild_entry gives each written file back, a plan equals the manifest's entry
            int rebuilt = 0, planned = 0, bad = 0;
            var planner = new UxMaterials.Planner(ctx);
            foreach (var kv in UxMaterials.Entries(man))
            {
                var p = kv.Value.Get("request") is bool rq && rq ? planner.Plan(kv.Key, RequestOf(ctx, kv.Key), UxJson.Get(ctx.work.ReadJson("terrain/material_requests.json"), "generated_textures") as JObj)
                    : planner.Plan(kv.Key);
                planned++;
                foreach (string k in new[] { "template", "textures", "source_textures", "state_patch", "sampler_patch", "swap_uv", "source", "uv_evidence", "warnings" })
                    if (kv.Value.Get("action") as string == "convert" && UxTest.JsonDifferences(p.Get(k), kv.Value.Get(k)).Count > 0) { bad++; break; }
                if (kv.Value.Get("action") as string != "convert") continue;
                rebuilt++;
                if (!UxBin.Same(UxMaterials.RebuildEntry(ctx.stock, kv.Value), File.ReadAllBytes(Path.Combine(produced, (string)kv.Value.Get("file"))))) bad++;
            }
            UxTest.Check(bad == 0, lv.disc + ": Planner.Plan (" + planned + ") and RebuildEntry (" + rebuilt + ") agree with the manifest and the written files (" + bad + " differ)");
        }
        UxTest.Log("     total: " + files + " reference files, " + same + " identical; " + F(sum) + " s for " + levels.Count + " levels (" + F(sum / Math.Max(1, levels.Count)) + " s per level)");
    }

    /// The stage on the reference's CURRENT upstream files against a fresh in-memory run of the current ux_materials.py
    /// (ref_materials.py: <ref>/materials-fresh/<Level>/).
    static void Fresh(UxTest.Env env, List<UxLevelRow> levels, string refDir)
    {
        UxTest.Section("materials: against a fresh run of the current ux_materials.py on the current inputs");
        int n = 0, files = 0, same = 0;
        foreach (var lv in levels)
        {
            string reference = Path.Combine(refDir, "materials-fresh", lv.disc);
            if (!File.Exists(Path.Combine(reference, "materials.json"))) continue;
            var ctx = env.Level(lv, "work-fresh"); ctx.log = s => { };
            SeedInputs(env, ctx);
            try { UxMaterials.Convert(ctx); } catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message); continue; }
            var r = UxTest.CompareFolder(lv.disc + " (fresh)", ctx.work.PathOf("materials"), reference, Counted, false, true);
            n++; files += r.reference; same += r.identical;
        }
        if (n < levels.Count) UxTest.Skip((levels.Count - n) + " levels have no fresh reference in " + Path.Combine(refDir, "materials-fresh") + " (ref_materials.py writes it)");
        UxTest.Log("     " + n + " levels: " + files + " reference files, " + same + " identical");
    }

    static UxMaterialsOptions OptionsOf(JObj o)
    {
        string S(string k, string d) => o.Get(k) as string ?? d;
        bool B(string k) => o.Get(k) is bool b && b;
        return new UxMaterialsOptions { alpha = S("alpha", "eco_frond"), prelit_off = B("prelit_off"), nrm_off = B("nrm_off"), crowd_off = B("crowd_off"), blend = S("blend", "keep"),
            bale_base = S("bale_base", "hay"), mipfilter = S("mipfilter", "linear"), composite = S("composite", "bake"), literal_albedo = S("literal_albedo", "program"),
            alpha_source = S("alpha_source", "program"), diffuse_pick = S("diffuse_pick", "program") };
    }

    /// Non-default switch sets: the reference's stored variant folders that are current (work/<Level>/materials-<tag>/), and the fresh
    /// in-memory runs of ref_materials.py --variants (<ref>/materials-variants/<tag>/<Level>/: the 16 switch sets of the reference's
    /// selftest), each compared file by file.
    static void Variants(UxTest.Env env, string refDir)
    {
        UxTest.Section("non-default switch sets");
        var cases = new List<(UxLevelRow lv, string folder, string reference, bool stored)>();
        foreach (var lv in UxCommon.Levels)
            if (Directory.Exists(env.PyWork(lv)))
                foreach (string d in Directory.GetDirectories(env.PyWork(lv), "materials-*")) cases.Add((lv, Path.GetFileName(d), d, true));
        string vroot = Path.Combine(refDir, "materials-variants");
        if (Directory.Exists(vroot))
            foreach (string t in Directory.GetDirectories(vroot))
                foreach (string d in Directory.GetDirectories(t)) cases.Add((UxCommon.Level(Path.GetFileName(d)), "materials-" + Path.GetFileName(t), d, false));
        else UxTest.Skip("no fresh variant references in " + vroot + " (ref_materials.py --variants writes them)");
        int n = 0, files = 0, same = 0; var tags = new SortedSet<string>(StringComparer.Ordinal);
        foreach (var (lv, folder, reference, stored) in cases)
        {
            string mj = Path.Combine(reference, "materials.json");
            if (!File.Exists(mj)) continue;
            var rj = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(mj)));
            var opts = OptionsOf(UxJson.Obj(rj.Get("options")));
            if (stored && (rj.Get("tool_md5") as string != opts.tool_md5 || UxJson.Get(rj, "source_files", "level_json_md5") as string != UxBin.Md5File(Path.Combine(env.PyWork(lv), "level.json"))))
            { UxTest.Log("     " + lv.disc + " " + folder + " (stored): older than the tools or level.json, not compared"); continue; }
            if (opts.OutDir() != folder) { UxTest.Fail(lv.disc + " " + folder + ": the options give the folder " + opts.OutDir()); continue; }
            var ctx = env.Level(lv, "variants"); ctx.log = s => { };
            SeedInputs(env, ctx, stored);
            try { UxMaterials.Convert(ctx, opts); } catch (Exception e) { UxTest.Fail(lv.disc + " " + folder + ": " + e.GetType().Name + ": " + e.Message); continue; }
            var r = UxTest.CompareFolder(lv.disc + " " + folder + (stored ? " (stored)" : ""), ctx.work.PathOf(folder), reference, Counted, false, true);
            n++; files += r.reference; same += r.identical; tags.Add(folder.Substring("materials-".Length));
        }
        UxTest.Log("     " + n + " runs of " + tags.Count + " switch sets: " + files + " reference files, " + same + " identical (" + string.Join(", ", tags) + ")");
    }

    static void Parallel12(UxTest.Env env, List<UxLevelRow> levels)
    {
        UxTest.Section("the levels converted at once");
        var ctxs = new UxContext[levels.Count];
        for (int i = 0; i < levels.Count; i++) { ctxs[i] = env.Level(levels[i], "work-parallel"); ctxs[i].log = s => { }; SeedInputs(env, ctxs[i], true); ctxs[i].disc.ClearCache(); }
        var errors = new string[levels.Count];
        var sw = Stopwatch.StartNew();
        Parallel.For(0, levels.Count, i => { try { UxMaterials.Convert(ctxs[i]); } catch (Exception e) { errors[i] = e.GetType().Name + ": " + e.Message; } });
        double secs = sw.Elapsed.TotalSeconds;
        int bad = 0, n = 0;
        for (int i = 0; i < levels.Count; i++)
        {
            if (errors[i] != null) { UxTest.Fail(levels[i].disc + ": " + errors[i]); continue; }
            string a = ctxs[i].work.PathOf("materials"), b = Path.Combine(env.scratch, "work", levels[i].disc, "materials");
            if (!Directory.Exists(b)) { UxTest.Skip(levels[i].disc + ": no serial run to compare with (run 'materials' first)"); continue; }
            var fa = Directory.GetFiles(a, "*", SearchOption.AllDirectories); var fb = Directory.GetFiles(b, "*", SearchOption.AllDirectories);
            if (fa.Length != fb.Length) bad++;
            foreach (string f in fb)
            {
                string other = Path.Combine(a, f.Substring(b.Length + 1)); n++;
                if (!File.Exists(other) || !UxBin.Same(File.ReadAllBytes(f), File.ReadAllBytes(other))) bad++;
            }
        }
        UxTest.Report(bad == 0, levels.Count + " levels in parallel: " + n + " files, " + bad + " differ from the serial run; " + F(secs) + " s wall clock");
    }
}
