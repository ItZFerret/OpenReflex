// tests/untamed_convert/meshes: the C# port of the S3 mesh stage (UxMeshes.cs = untamed/tools/ux_meshes.py) against the Python reference.
//   levels   every level: level.json seeded from the Python work folder, UxMeshes.Convert from the disc, then every file of
//            work/<Level>/meshes: *.surface and *.model byte for byte, meshes.json byte for byte, meshes_info.json as a document with
//            'tool_md5s' excluded (the md5s of the Python tool files) and byte for byte once that one value is taken from the reference.
//   threads  the first level again with one thread: the same bytes (the result does not depend on the thread count).
//   models   UxMeshes.ConvertModel with other limits, material lists and the rigid='bound' rule against digests written by
//            ref_meshes.py (run.sh): the split, crowd-split and _pN paths the default limits do not reach.
// Arguments: --iso / --folder (the disc), --py, --scratch, --levels a,b (default all), --threads n (default: every processor),
// --no-threads (skip the one-thread comparison), --ref <folder of ref_meshes.py's output>.
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

    static int Main(string[] args)
    {
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null) { UxTest.Skip("the disc is needed (--iso / UX_DISC_ISO or --folder / UX_DISC_FOLDER)"); return UxTest.Result("meshes"); }
            var levels = new List<UxLevelRow>();
            string only = UxTest.Arg(args, "levels");
            if (only == null) levels.AddRange(UxCommon.Levels); else foreach (string s in only.Split(',')) levels.Add(UxCommon.Level(s));
            var rows = new List<string>(); double total = 0;
            int threads = int.Parse(UxTest.Arg(args, "threads", null, "0"), Inv);
            foreach (var lv in levels)
            {
                UxTest.Section(lv.disc);
                var ctx = env.Level(lv, "meshes");
                env.Seed(ctx, "level.json");
                var sw = Stopwatch.StartNew();
                UxMeshesResult r;
                try { r = UxMeshes.Convert(ctx, new UxMeshesOptions { failOnProblems = false, threads = threads }); }
                catch (Exception e) { UxTest.Fail(lv.disc + ": " + e); continue; }
                double sec = sw.Elapsed.TotalSeconds; total += sec;
                UxTest.Check(r.ChecksPassed, lv.disc + ": read-back checks: " + (r.problems == null ? "not run" : string.Join("; ", r.problems.GetRange(0, Math.Min(5, r.problems.Count)))));
                string produced = ctx.work.PathOf("meshes"), reference = Path.Combine(env.PyWork(lv), "meshes");
                var fr = UxTest.CompareFolder(lv.disc + " meshes (surface, model, meshes.json)", produced, reference, rel => !Same(rel, "meshes_info.json"), false, true);
                string info = InfoCheck(lv.disc, Path.Combine(produced, "meshes_info.json"), Path.Combine(reference, "meshes_info.json"));
                rows.Add(string.Format(Inv, "{0,-22} {1,4} files: {2,4} identical, {3} different, {4} missing, {5} extra; meshes_info.json {6}; {7:0.0} s (convert {8:0.0}, checks {9:0.0})",
                    lv.disc, fr.reference + 1, fr.identical, fr.different + fr.jsonEqual, fr.missing, fr.extra, info, sec, r.convertSeconds, r.checkSeconds));
            }
            if (levels.Count > 0 && !UxTest.Flag(args, "no-threads"))
            {
                var lv = levels[0];
                UxTest.Section(lv.disc + " with one thread");
                var ctx = env.Level(lv, "meshes-1thread");
                env.Seed(ctx, "level.json");
                var sw = Stopwatch.StartNew();
                UxMeshes.Convert(ctx, new UxMeshesOptions { threads = 1, failOnProblems = false });
                var fr = UxTest.CompareFolder(lv.disc + " one thread against " + Environment.ProcessorCount + " threads", ctx.work.PathOf("meshes"),
                    Path.Combine(env.scratch, "meshes", lv.disc, "meshes"), null, false, true);
                rows.Add(string.Format(Inv, "{0,-22} one thread: {1} files identical to the parallel run, {2:0.0} s", lv.disc, fr.identical, sw.Elapsed.TotalSeconds));
            }
            string refDir = UxTest.Arg(args, "ref");
            if (refDir != null && File.Exists(Path.Combine(refDir, "models.json"))) Models(env, Path.Combine(refDir, "models.json"), rows);
            else UxTest.Skip("ConvertModel variants: no reference digests (run.sh writes them with ref_meshes.py)");
            if (refDir != null && File.Exists(Path.Combine(refDir, "neg.json"))) Negative(env, refDir, rows);
            else UxTest.Skip("read-back checks on damaged files: no reference (run.sh writes it with ref_meshes.py)");
            UxTest.Section("summary");
            foreach (string row in rows) UxTest.Log(row);
            UxTest.Log(string.Format(Inv, "{0} levels, {1:0.0} s in the stage, {2} threads, peak working set {3} MB", levels.Count, total,
                threads > 0 ? threads : Environment.ProcessorCount, Process.GetCurrentProcess().PeakWorkingSet64 >> 20));
        }
        return UxTest.Result("meshes");
    }

    static bool Same(string a, string b) => string.Equals(a, b, StringComparison.OrdinalIgnoreCase);

    /// meshes_info.json: the documents are compared without 'tool_md5s' (environment data: the md5s of the Python tool files), then
    /// the bytes with the reference's 'tool_md5s' in place of the port's empty dict.
    static string InfoCheck(string what, string produced, string reference)
    {
        if (!File.Exists(produced)) { UxTest.Fail(what + " meshes_info.json: not written"); return "missing"; }
        byte[] got = File.ReadAllBytes(produced), want = File.ReadAllBytes(reference);
        var g = UxJson.Obj(UxJson.ParseUtf8(got)); var w = UxJson.Obj(UxJson.ParseUtf8(want));
        object tool = w.Get("tool_md5s");
        Strip(g, "tool_md5s"); Strip(w, "tool_md5s");
        var d = UxTest.JsonDifferences(g, w);
        UxTest.Check(d.Count == 0, what + " meshes_info.json without tool_md5s: " + d.Count + " differences");
        for (int i = 0; i < d.Count && i < UxTest.MaxReports; i++) UxTest.Log("     " + d[i]);
        g.Set("tool_md5s", tool);
        bool bytes = UxTest.SameBytes(what + " meshes_info.json with the reference's tool_md5s", Encoding.UTF8.GetBytes(UxJson.DumpsUx(g)), want);
        return d.Count == 0 && bytes ? "identical but tool_md5s" : d.Count == 0 ? "equal document, other bytes" : d.Count + " differences";
    }
    static void Strip(JObj o, string key) { o.items.RemoveAll(kv => kv.Key == key); }

    /// The read-back checks must find what the reference's find: ref_meshes.py damaged six part files of a copy of the reference's
    /// Norton output and recorded ux_meshes.verify_level's problems and statistics; UxMeshes.VerifyLevel on the same folder.
    static void Negative(UxTest.Env env, string refDir, List<string> rows)
    {
        UxTest.Section("read-back checks on damaged files");
        var doc = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(refDir, "neg.json"))));
        var lv = UxCommon.Level(UxJson.Str(doc.Get("level")));
        var L = new UxMeshLevel(env.disc.Level(lv), UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(env.PyWork(lv), "level.json")))));
        var probs = UxMeshes.VerifyLevel(L, new UxWork(Path.Combine(refDir, UxJson.Str(doc.Get("folder")))), new UxMeshesOptions(), out JObj stats);
        var want = UxJson.Arr(doc.Get("problems"));
        var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(new JObj().Set("problems", probs).Set("stats", stats))),
            UxJson.Parse(UxJson.DumpsUx(new JObj().Set("problems", want).Set("stats", doc.Get("stats")))));
        UxTest.Report(d.Count == 0 && want.Count > 0, lv.disc + " with " + UxJson.Arr(doc.Get("damaged")).Count + " damaged files: " + probs.Count + " problems, the reference " + want.Count +
            "; " + d.Count + " differences (problem texts in order, statistics)");
        for (int i = 0; i < d.Count && i < UxTest.MaxReports; i++) UxTest.Log("     " + d[i]);
        rows.Add("read-back checks on " + UxJson.Arr(doc.Get("damaged")).Count + " damaged files: " + probs.Count + " problems, " + (d.Count == 0 ? "the reference's texts and statistics" : d.Count + " differences"));
    }

    /// ConvertModel variants against ref_meshes.py: {cases: [{level, model, limits, collision, materials, rigid, skipped, parts: [{part,
    /// surface_md5, model_md5, json}], info, fixes}]}.
    static void Models(UxTest.Env env, string path, List<string> rows)
    {
        UxTest.Section("ConvertModel variants");
        var doc = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(path)));
        var levels = new Dictionary<string, UxMeshLevel>();
        int cases = 0, same = 0, parts = 0;
        foreach (object co in UxJson.Arr(doc.Get("cases")))
        {
            var c = UxJson.Obj(co);
            string level = UxJson.Str(c.Get("level")), model = UxJson.Str(c.Get("model"));
            if (!levels.TryGetValue(level, out var L))
            {
                var lv = UxCommon.Level(level);
                levels[level] = L = new UxMeshLevel(env.disc.Level(lv), UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(env.PyWork(lv), "level.json")))));
            }
            var lim = UxJson.Arr(c.Get("limits"));
            var o = new UxMeshesOptions { maxGroupVerts = (int)UxJson.Int(lim[0]), maxGroupStripIndices = (int)UxJson.Int(lim[1]), maxGroupsPerModel = (int)UxJson.Int(lim[2]) };
            List<string> mats = null;
            if (c.Get("materials") is List<object> ml) { mats = new List<string>(); foreach (object x in ml) mats.Add(x as string); }
            bool cb = UxJson.Bool(c.Get("collision"));
            double[] lo = null, hi = null;
            if (cb && UxJson.Bool(c.Get("bounds"))) L.CollisionBounds(model, out lo, out hi);
            var fixes = new UxMeshFixes();
            string tag = level + " " + model + " limits " + UxJson.Dumps(lim) + " rigid " + c.Get("rigid") + (mats != null ? " materials" : "");
            cases++;
            UxMeshModel r;
            try { r = UxMeshes.ConvertModel(L, model, cb, lo, hi, o, fixes, mats, UxJson.Str(c.Get("rigid"))); }
            catch (Exception e)
            {   // a case the reference refuses (an exception there) must be refused here too
                if (UxTest.Check(c.Has("error"), tag + ": " + e.Message)) { same++; UxTest.Log("note " + tag + ": refused by both (" + e.Message + " / " + c.Get("error") + ")"); }
                continue;
            }
            if (c.Has("error")) { UxTest.Fail(tag + ": the reference refuses this case (" + c.Get("error") + "), the port converted it"); continue; }
            var uv = new List<object>();
            foreach (var u in r.uv)
                uv.Add(new JObj().Set("group", (long)u.group).Set("ml_index", (long)u.ml_index).Set("verts", (long)u.verts).Set("uv1_in01", u.uv1_in01).Set("uv1_eq_uv0", u.uv1_eq_uv0)
                    .Set("uv0_span", u.uv0_span).Set("uv1_span", u.uv1_span));
            var got = new JObj().Set("skipped", r.skipped).Set("info", r.info).Set("fixes", fixes.ToJson()).Set("uv", uv);
            var gp = new List<object>();
            foreach (var p in r.parts) { gp.Add(new JObj().Set("part", p.part).Set("surface_md5", UxBin.Md5(p.surface_bytes)).Set("model_md5", UxBin.Md5(p.model_bytes)).Set("json", p.ToJson())); parts++; }
            got.Set("parts", gp);
            var want = new JObj().Set("skipped", c.Get("skipped")).Set("info", c.Get("info")).Set("fixes", c.Get("fixes")).Set("uv", c.Get("uv")).Set("parts", c.Get("parts"));
            // through text, so that both sides are parsed documents (the port's floats are written as Python writes them)
            var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(got)), UxJson.Parse(UxJson.DumpsUx(want)));
            if (UxTest.Check(d.Count == 0, tag + ": " + d.Count + " differences")) same++;
            for (int i = 0; i < d.Count && i < 6; i++) UxTest.Log("     " + d[i]);
        }
        rows.Add("ConvertModel variants: " + cases + " cases (" + parts + " parts), " + same + " identical (part md5s, part dicts, info, fixes, skip reasons)");
    }
}
