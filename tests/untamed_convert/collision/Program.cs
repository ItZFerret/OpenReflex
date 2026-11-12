// tests/untamed_convert/collision: the S5 collision stage port (UxCollision, the port of untamed/tools/ux_collision.py) against the Python
// reference, headless.
//   levels     the stage for the 12 levels from the disc, on Python's own inputs (level.json, level_fr.json, terrain/hf.npy seeded from
//              work/<Level>), with the options of the current builds (type9 accept -> collision-t9/) and with the defaults
//              (-> collision/): every .collis byte for byte, collision.json byte for byte. Three manifest fields are environment data of
//              the Python run (tool_md5, readers, options.out_dir: md5s of .py files and an absolute path); the test passes the
//              reference's values in through UxCollisionOptions, so the whole file can be compared as bytes. collision_overlay.png (a
//              preview picture) is not ported and not compared. A serial run in memory and a run with exactMetrics must give the same
//              manifest. Other options (type9 refuse, pop clip off, another max edge) are compared with the manifests the current
//              Python tool returns in memory (ref_collision.py: <ref>/levels/<Level>.<variant>.json; they hold every output's md5).
//   qhull      UxCollision.Qhull2 (the 2-D ConvexHull vertex order) against scipy on the point sets ref_collision.py wrote
//   synthetic  convert_collis on synthetic files (kind 17, 19, tags, sphere / cylinder / box, type 9, negative half) and helper values
//              (cos / sin tables, default_rng(12345), text formatting, 3-D hull planes) against ref_collision.py's records
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

    static int Main(string[] args)
    {
        string what = "all";
        foreach (string a in args) if (a == "levels" || a == "qhull" || a == "synthetic" || a == "all") what = a;
        using (var env = UxTest.OpenEnv(args))
        {
            string refDir = UxTest.Arg(args, "ref", "UX_REF", Path.Combine(env.scratch, "ref"));
            if (what == "all" || what == "qhull") Qhull(refDir);
            if (what == "all" || what == "synthetic") Synthetic(refDir);
            if (what == "all" || what == "levels") Levels(env, args, refDir);
        }
        return UxTest.Result("collision");
    }

    // ------------------------------------------------------------------------------------------------ the 12 levels
    static Dictionary<string, Dictionary<string, string>> TemplateCollis(UxTest.Env env)
    {
        // U.TEMPLATE_DIR: the template slots' collis names (the name-clash rule). Read from the reference's templates, as UxTest.OpenEnv does.
        var tn = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        foreach (var (kind, dir, slot) in new[] { ("sx", "sx-sketch-slot1", "Beta_Sx_Track_Slot_1"), ("fr", "huckit-freeride-slot1", "Beta_Track_Slot_1") })
        {
            string db = Path.Combine(env.py, "templates", dir, slot + ".dx9.database");
            if (File.Exists(db)) tn[kind] = new UxTemplateNames(slot, File.ReadAllBytes(db)).Of("collis");
        }
        return tn;
    }

    /// The options of a run: the three environment fields of the reference manifest are passed in (see the head of this file).
    static UxCollisionOptions Options(JObj pyMan, bool write, int threads, bool exact = false, string outDir = null)
    {
        var pyOpt = UxJson.Obj(pyMan.Get("options"));
        return new UxCollisionOptions
        {
            type9 = (string)pyOpt.Get("type9"), popClip = pyOpt.Get("pop_clip") as string ?? "off", maxEdge = UxJson.Num(pyOpt.Get("max_edge")),
            write = write, threads = threads, exactMetrics = exact, outDir = outDir,
            toolMd5 = (string)pyMan.Get("tool_md5"), readers = UxJson.Obj(pyMan.Get("readers")), outDirText = (string)pyOpt.Get("out_dir"),
        };
    }

    static void Levels(UxTest.Env env, string[] args, string refDir)
    {
        if (env.disc == null) { UxTest.Skip("levels: the disc is needed (UX_DISC_FOLDER / UX_DISC_ISO)"); return; }
        var tn = TemplateCollis(env);
        if (!tn.ContainsKey("sx") || !tn.ContainsKey("fr")) { UxTest.Skip("levels: the reference's template databases are needed (" + env.py + "/templates)"); return; }
        string only = UxTest.Arg(args, "levels");
        int threads = UxTest.Flag(args, "no-threads") ? 1 : 0;
        string toolMd5 = UxBin.Md5File(Path.Combine(env.py, "tools", "ux_collision.py"));
        var table = new List<string>(); double total = 0, totalSerial = 0, totalExact = 0; int nvariants = 0, okVariants = 0;
        foreach (var lv in UxCommon.Levels)
        {
            if (only != null && Array.IndexOf(only.Split(','), lv.shortName) < 0 && Array.IndexOf(only.Split(','), lv.disc) < 0) continue;
            UxTest.Section(lv.disc);
            var ctx = env.Level(lv, "work");
            var seed = new List<string> { "level.json" };
            if (File.Exists(Path.Combine(env.PyWork(lv), "level_fr.json"))) seed.Add("level_fr.json");
            if (File.Exists(Path.Combine(env.PyWork(lv), "terrain", "hf.npy"))) seed.Add("terrain/hf.npy");
            env.Seed(ctx, seed.ToArray());
            string levelMd5 = UxBin.Md5(ctx.work.ReadBytes("level.json")), hfMd5 = ctx.work.Exists("terrain/hf.npy") ? UxBin.Md5(ctx.work.ReadBytes("terrain/hf.npy")) : null;
            string row = lv.shortName.PadRight(12);

            // ---- the folders Python wrote: collision-t9 (the current builds' options) and collision (the defaults)
            foreach (string type9 in new[] { "accept", "refuse" })
            {
                string folder = UxCollision.OutDirFor(false, type9);
                string pyDir = Path.Combine(env.PyWork(lv), folder), pyManFile = Path.Combine(pyDir, "collision.json");
                if (!File.Exists(pyManFile)) { if (type9 == "accept") UxTest.Skip(lv.disc + " " + folder + ": no reference folder"); continue; }
                var pyMan = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(pyManFile)));
                var pyOpt = UxJson.Obj(pyMan.Get("options"));
                // is the reference folder what the current tool writes for these inputs and options?
                bool current = (string)pyMan.Get("tool_md5") == toolMd5 && (string)pyMan.Get("level_json_md5") == levelMd5 &&
                    (pyOpt.Get("pop_clip") as string ?? "off") == "on" && (string)pyOpt.Get("type9") == type9 && !(bool)pyOpt.Get("prims_to_22") &&
                    (pyMan.Get("pop_clip_hf_md5") as string) == hfMd5;
                if (!current)
                {
                    if (type9 == "accept") UxTest.Fail(lv.disc + " " + folder + ": the reference folder is not from the current ux_collision.py / inputs / options");
                    else UxTest.Log("note " + lv.disc + " " + folder + ": the folder on disk is from another tool version or other options (the current tool's result is compared below, in memory)");
                    continue;
                }
                var sw = Stopwatch.StartNew();
                JObj man;
                try { man = UxCollision.Convert(ctx.disc, lv, ctx.work, tn, Options(pyMan, true, threads)); }
                catch (Exception e) { UxTest.Fail(lv.disc + " " + folder + ": " + e.GetType().Name + ": " + e.Message); UxTest.Log(e.StackTrace); continue; }
                double sec = sw.Elapsed.TotalSeconds; total += sec;
                var r = UxTest.CompareFolder(lv.disc + " " + folder + " (" + sec.ToString("0.00", Inv) + " s)", ctx.work.PathOf(folder), pyDir,
                    rel => rel != "collision_overlay.png", allowJsonEqual: false, failExtra: true);
                row += "  " + folder + ": " + r.reference + " files, " + r.identical + " identical, " + r.different + " different, " + r.missing + " missing, " + r.extra + " extra, " +
                    sec.ToString("0.00", Inv) + " s";
                UxTest.Check(UxJson.Arr(man.Get("problems")).Count == 0, lv.disc + " " + folder + ": " + UxJson.Arr(man.Get("problems")).Count + " problems");
                if (type9 != "accept") continue;
                // a serial run in memory, and one with every box-fit sample through the exact chains, give the same manifest
                string written = ctx.work.ReadText(folder + "/collision.json");
                sw.Restart();
                var man2 = UxCollision.Convert(ctx.disc, lv, ctx.work, tn, Options(pyMan, false, 1));
                double secSerial = sw.Elapsed.TotalSeconds; totalSerial += secSerial;
                UxTest.Check(UxJson.DumpsUx(man2) == written, lv.disc + " " + folder + ": serial run in memory = the written manifest");
                sw.Restart();
                var man3 = UxCollision.Convert(ctx.disc, lv, ctx.work, tn, Options(pyMan, false, threads, exact: true));
                double secExact = sw.Elapsed.TotalSeconds; totalExact += secExact;
                UxTest.Check(UxJson.DumpsUx(man3) == written, lv.disc + " " + folder + ": exactMetrics run in memory = the written manifest");
                row += " (serial " + secSerial.ToString("0.00", Inv) + " s, exact metrics " + secExact.ToString("0.00", Inv) + " s)";
            }

            // ---- other options: the manifests the current Python tool returns in memory (ref_collision.py; the md5 of every output is in them)
            string vdir = Path.Combine(refDir, "levels");
            string[] vfiles = Directory.Exists(vdir) ? Directory.GetFiles(vdir, lv.disc + ".*.json") : new string[0];
            Array.Sort(vfiles, StringComparer.Ordinal);
            if (vfiles.Length == 0) UxTest.Skip(lv.disc + ": no option variants in " + vdir + " (ref_collision.py writes them)");
            foreach (string vf in vfiles)
            {
                string variant = Path.GetFileNameWithoutExtension(vf).Substring(lv.disc.Length + 1);
                byte[] want = File.ReadAllBytes(vf);
                var pyMan = UxJson.Obj(UxJson.ParseUtf8(want));
                if ((string)pyMan.Get("level_json_md5") != levelMd5 || (pyMan.Get("pop_clip_hf_md5") as string) != (pyMan.Has("pop_clip") ? hfMd5 : null) || (string)pyMan.Get("tool_md5") != toolMd5)
                { UxTest.Fail(lv.disc + " variant " + variant + ": the reference manifest is not from the current tool / inputs (run with UX_REFS=1)"); continue; }
                nvariants++;
                JObj man;
                try { man = UxCollision.Convert(ctx.disc, lv, ctx.work, tn, Options(pyMan, true, threads, outDir: "variants/" + variant)); }
                catch (Exception e) { UxTest.Fail(lv.disc + " variant " + variant + ": " + e.GetType().Name + ": " + e.Message); continue; }
                int res = UxTest.SameJsonFile(lv.disc + " variant " + variant + " collision.json", ctx.work.ReadBytes("variants/" + variant + "/collision.json"), want);
                // the written files are the manifest's: every source's md5 and size
                bool filesOk = true; int nfiles = 0;
                foreach (var kv in UxJson.Obj(man.Get("sources")).items)
                {
                    var rec = (JObj)kv.Value; string file = rec.Get("file") as string;
                    if (file == null) continue;
                    nfiles++;
                    string rel = "variants/" + variant + "/" + file;
                    if (!ctx.work.Exists(rel) || UxBin.Md5(ctx.work.ReadBytes(rel)) != (string)rec.Get("md5") || ctx.work.Length(rel) != (long)rec.Get("bytes")) filesOk = false;
                }
                UxTest.Check(filesOk && ctx.work.List("variants/" + variant, "*.collis").Count == nfiles, lv.disc + " variant " + variant + ": the written .collis files are the manifest's (" + nfiles + ")");
                if (res == 0 && filesOk) okVariants++;
            }
            table.Add(row);
        }
        UxTest.Log("");
        UxTest.Log("level         per output folder: files compared / identical / different / missing / extra, seconds");
        foreach (string row in table) UxTest.Log(row);
        UxTest.Log("stage time for the listed folders " + total.ToString("0.00", Inv) + " s; collision-t9 serial " + totalSerial.ToString("0.00", Inv) + " s, with exact metrics " + totalExact.ToString("0.00", Inv) + " s");
        UxTest.Report(okVariants == nvariants, "option variants (type9 / pop clip / max edge) against the current Python tool in memory: " + okVariants + " of " + nvariants +
            " manifests byte-identical (each holds the md5 of its outputs)");
    }

    // ------------------------------------------------------------------------------------------------ internals through reflection
    static readonly Type T = typeof(UxCollision);
    static object Call(Type t, string name, params object[] a)
    {
        var m = t.GetMethod(name, BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public);
        if (m == null) throw new Exception("no method " + t.Name + "." + name);
        try { return m.Invoke(null, a); }
        catch (TargetInvocationException e) { throw e.InnerException; }
    }
    static Type Nested(string name) => T.GetNestedType(name, BindingFlags.NonPublic | BindingFlags.Public) ?? throw new Exception("no type " + name);

    // ------------------------------------------------------------------------------------------------ qhull 2-D vertex order
    static void Qhull(string refDir)
    {
        UxTest.Section("qhull 2-D vertex order (scipy.spatial.ConvexHull(P).vertices)");
        string f = Path.Combine(refDir, "qhull2d.json");
        if (!File.Exists(f)) { UxTest.Skip("qhull: no " + f + " (ref_collision.py writes it)"); return; }
        var doc = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(f)));
        var q2 = Nested("Qhull2");
        foreach (var group in doc.items)
        {
            int n = 0, same = 0, refused = 0, wrong = 0, sameSet = 0; var firstWrong = new List<string>();
            foreach (object co in UxJson.Arr(group.Value))
            {
                var c = UxJson.Obj(co); n++;
                var pl = UxJson.Arr(c.Get("P")); var P = new double[pl.Count];
                for (int i = 0; i < P.Length; i++) P[i] = Hex((string)pl[i]);
                var wl = UxJson.Arr(c.Get("v")); var want = new int[wl.Count];
                for (int i = 0; i < want.Length; i++) want[i] = (int)(long)wl[i];
                int[] got;
                try { got = (int[])Call(q2, "Vertices", P); }
                catch (Exception e) when (e.GetType().Name == "HullException") { refused++; if (firstWrong.Count < 3) firstWrong.Add("case " + (n - 1) + " refused: " + e.Message); continue; }
                bool eq = got.Length == want.Length;
                for (int i = 0; eq && i < got.Length; i++) eq = got[i] == want[i];
                if (eq) { same++; continue; }
                wrong++;
                var a = new SortedSet<int>(got); var b = new SortedSet<int>(want);
                if (a.SetEquals(b)) sameSet++;
                if (firstWrong.Count < 3) firstWrong.Add("case " + (n - 1) + ": got [" + string.Join(",", got) + "] want [" + string.Join(",", want) + "]");
            }
            // 'real': the stage's own inputs, 'general': point sets in general position: every one must match. The 'degenerate' groups hold
            // exactly collinear points, where qhull merges facets: there the port may refuse (never answer wrongly without saying so is
            // not promised: a wrong order is counted and reported).
            bool must = group.Key.StartsWith("real", StringComparison.Ordinal) || group.Key.StartsWith("general", StringComparison.Ordinal);
            string line = "qhull " + group.Key + ": " + n + " cases, " + same + " identical order, " + refused + " refused (need merging), " + wrong + " other order (" + sameSet + " of them the same vertex set)";
            if (must) UxTest.Report(same == n, line); else UxTest.Log("info " + line);
            foreach (string s in firstWrong) UxTest.Log("     " + s);
        }
    }

    // ------------------------------------------------------------------------------------------------ synthetic files and helpers
    static void Synthetic(string refDir)
    {
        UxTest.Section("synthetic sources and helper values");
        string f = Path.Combine(refDir, "synthetic.json");
        if (!File.Exists(f)) { UxTest.Skip("synthetic: no " + f + " (ref_collision.py writes it)"); return; }
        var doc = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(f)));

        // cos / sin of the reference machine
        var trig = Nested("Trig"); int nt = 0, okT = 0, sameAsRuntime = 0;
        foreach (object eo in UxJson.Arr(doc.Get("trig")))
        {
            var e = UxJson.Arr(eo);
            double x = Hex((string)e[1]); bool cos = (string)e[0] == "cos";
            double got = (double)Call(trig, cos ? "Cos" : "Sin", x), want = Hex((string)e[2]);
            nt++; if (BitConverter.DoubleToInt64Bits(got) == BitConverter.DoubleToInt64Bits(want)) okT++;
            if (BitConverter.DoubleToInt64Bits(cos ? Math.Cos(x) : Math.Sin(x)) == BitConverter.DoubleToInt64Bits(want)) sameAsRuntime++;
        }
        UxTest.Report(okT == nt, "np.cos / np.sin of the ring angles: " + okT + " of " + nt + " bit-identical (this runtime's Math.Cos / Math.Sin: " + sameAsRuntime + " of " + nt + ")");

        // default_rng(12345)
        var rngT = Nested("Rng12345"); object rng = Activator.CreateInstance(rngT, true);
        var rl = UxJson.Arr(doc.Get("rng_random")); int okR = 0;
        var mRandom = rngT.GetMethod("Random", Type.EmptyTypes);
        foreach (object v in rl) if (BitConverter.DoubleToInt64Bits((double)mRandom.Invoke(rng, null)) == BitConverter.DoubleToInt64Bits(Hex((string)v))) okR++;
        UxTest.Report(okR == rl.Count, "default_rng(12345).random(): " + okR + " of " + rl.Count + " values bit-identical");
        {
            var ch = UxJson.Obj(doc.Get("rng_choice"));
            var pl = UxJson.Arr(ch.Get("p")); var p = new double[pl.Count]; for (int i = 0; i < p.Length; i++) p[i] = Hex((string)pl[i]);
            object rng2 = Activator.CreateInstance(rngT, true);
            var idx = (int[])rngT.GetMethod("Choice").Invoke(rng2, new object[] { p, UxJson.Arr(ch.Get("k")).Count });
            int okC = 0; var kl = UxJson.Arr(ch.Get("k"));
            for (int i = 0; i < idx.Length; i++) if (idx[i] == (long)kl[i]) okC++;
            var after = UxJson.Arr(ch.Get("after")); int okA = 0;
            foreach (object v in after) if (BitConverter.DoubleToInt64Bits((double)mRandom.Invoke(rng2, null)) == BitConverter.DoubleToInt64Bits(Hex((string)v))) okA++;
            UxTest.Report(okC == idx.Length && okA == after.Count, "default_rng(12345).choice(n, size, p): " + okC + " of " + idx.Length + " indices, the stream after it " + okA + " of " + after.Count);
        }

        // '%g' / '%.nf'
        int nf = 0, okF = 0;
        foreach (object eo in UxJson.Arr(doc.Get("format")))
        {
            var e = UxJson.Arr(eo); string kind = (string)e[0]; double x = Hex((string)e[1]); string want = (string)e[3]; int prec = (int)(long)e[2];
            string got = kind == "g" ? (string)Call(T, "PyG", x, prec) : (string)Call(T, "PyF", x, prec);
            nf++; if (got == want) okF++; else if (nf - okF <= 5) UxTest.Log("     %" + (kind == "g" ? "." + prec + "g" : "." + prec + "f") + " of " + PyFormat.Repr(x) + ": got '" + got + "' want '" + want + "'");
        }
        UxTest.Report(okF == nf, "Python '%g' / '%.nf' formatting: " + okF + " of " + nf + " strings");

        // 3-D hull planes: the same set of planes as ConvexHull(V).equations
        int nh = 0, okH = 0; double worst = 0;
        foreach (object co in UxJson.Arr(doc.Get("hull3d")))
        {
            var c = UxJson.Obj(co);
            var pl = UxJson.Arr(c.Get("P")); var P = new double[pl.Count]; for (int i = 0; i < P.Length; i++) P[i] = Hex((string)pl[i]);
            var el = UxJson.Arr(c.Get("eq")); var E = new double[el.Count]; for (int i = 0; i < E.Length; i++) E[i] = Hex((string)el[i]);
            var a = new object[] { P, null, null };
            T.GetMethod("HullPlanes", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, a);
            double[] N = (double[])a[1], D = (double[])a[2];
            // every reference plane (n, d) must be one of mine and the reverse (scipy lists one equation per triangle of a facet)
            double dev = 0; bool all = true;
            for (int i = 0; i < E.Length / 4; i++)
            {
                double best = double.PositiveInfinity;
                for (int j = 0; j < D.Length; j++)
                    best = Math.Min(best, Math.Max(Math.Max(Math.Abs(E[4 * i] - N[3 * j]), Math.Abs(E[4 * i + 1] - N[3 * j + 1])), Math.Max(Math.Abs(E[4 * i + 2] - N[3 * j + 2]), Math.Abs(-E[4 * i + 3] - D[j]))));
                dev = Math.Max(dev, best);
            }
            for (int j = 0; j < D.Length; j++)
            {
                double best = double.PositiveInfinity;
                for (int i = 0; i < E.Length / 4; i++)
                    best = Math.Min(best, Math.Max(Math.Max(Math.Abs(E[4 * i] - N[3 * j]), Math.Abs(E[4 * i + 1] - N[3 * j + 1])), Math.Max(Math.Abs(E[4 * i + 2] - N[3 * j + 2]), Math.Abs(-E[4 * i + 3] - D[j]))));
                dev = Math.Max(dev, best);
            }
            all = dev < 1e-9;
            nh++; if (all) okH++; worst = Math.Max(worst, dev);
            if (!all) UxTest.Log("     hull3d set " + (nh - 1) + " (" + (c.Get("src") ?? "random") + "): " + P.Length / 3 + " points, " + E.Length / 4 + " reference equations, " + D.Length + " planes, largest difference " + dev.ToString("0.0e+0", Inv));
        }
        UxTest.Report(okH == nh, "3-D hull planes: " + okH + " of " + nh + " point sets give the planes of ConvexHull(V).equations (largest difference of a coefficient " + worst.ToString("0.0e+0", Inv) + ")");

        // convert_collis on synthetic files: the record and the output bytes
        int ns = 0, okS = 0;
        foreach (object co in UxJson.Arr(doc.Get("sources")))
        {
            var c = UxJson.Obj(co); ns++;
            byte[] data = UxBin.FromHex((string)c.Get("data")); string name = (string)c.Get("name");
            object conv;
            string err = null;
            try { conv = Call(T, "ConvertCollis", data, name, (string)c.Get("type9"), UxJson.Num(c.Get("max_edge")), (bool)c.Get("dynamic"), default(System.Threading.CancellationToken), false); }
            catch (Exception e) { conv = null; err = e.Message; }
            string label = "synthetic " + name + " (type9 " + c.Get("type9") + ", dynamic " + c.Get("dynamic") + ")";
            if (c.Get("error") != null)
            {   // the exception text goes into the manifest's problems ('<name>: <text>'): it must be Python's
                if (err == (string)c.Get("error")) okS++; else UxTest.Fail(label + ": Python raises '" + c.Get("error") + "', the port " + (err == null ? "does not raise" : "raises '" + err + "'"));
                continue;
            }
            if (err != null) { UxTest.Fail(label + ": " + err); continue; }
            var ct = conv.GetType();
            var rec = (JObj)ct.GetField("rec").GetValue(conv); var outb = (byte[])ct.GetField("data").GetValue(conv);
            if (outb != null)
            {
                Call(T, "CheckSource", conv);
                var probs = (List<string>)ct.GetField("checkProblems").GetValue(conv); var st = (JObj)ct.GetField("checkStats").GetValue(conv);
                var check = new JObj().Set("ok", probs.Count == 0).Set("problems", new List<object>(probs));
                foreach (var kv in st.items) check.Set(kv.Key, kv.Value);
                rec.Set("check", check);
            }
            var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(rec)), c.Get("rec"));
            string wantHex = c.Get("out") as string;
            bool bytesOk = (outb == null) == (wantHex == null) && (outb == null || UxBin.Hex(outb) == wantHex);
            if (d.Count == 0 && bytesOk) okS++;
            else
            {
                UxTest.Fail(label + ": " + d.Count + " record differences" + (bytesOk ? "" : ", output bytes differ"));
                for (int i = 0; i < d.Count && i < 6; i++) UxTest.Log("     " + d[i]);
            }
        }
        UxTest.Report(okS == ns, "convert_collis + check_source on synthetic files: " + okS + " of " + ns + " records and outputs equal");

        // the pop clip's parts: clip_prims15 (shortened / dropped boxes, the ValueError texts) and _sunk_excess (with and without a heightfield)
        var pcT = Nested("PcCollis"); var hfT = Nested("HfGrid");
        object Prims(string hex) => pcT.GetField("prims", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic).GetValue(Call(T, "ParsePcCollis", UxBin.FromHex(hex)));
        int nc = 0, okCl = 0;
        foreach (object co in UxJson.Arr(doc.Get("clip")))
        {
            var c = UxJson.Obj(co); nc++;
            var rep = new List<object>(); object np2 = null; string err = null;
            try { np2 = Call(T, "ClipPrims15", Prims((string)c.Get("data")), Hex((string)c.Get("clip_y")), rep); }
            catch (Exception e) when (e.GetType().Name == "FitException") { err = e.Message; }
            string label = "clip_prims15 " + c.Get("name") + " at y " + PyFormat.Repr(Hex((string)c.Get("clip_y")));
            if (c.Get("error") != null || err != null)
            {
                if (err == c.Get("error") as string) okCl++; else UxTest.Fail(label + ": error '" + err + "', Python '" + c.Get("error") + "'");
                continue;
            }
            string outHex = UxBin.Hex((byte[])Call(T, "WritePrims15", np2));
            var d = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(rep)), c.Get("rep"));
            if (outHex == (string)c.Get("out") && d.Count == 0) okCl++;
            else { UxTest.Fail(label + ": " + (outHex == (string)c.Get("out") ? "" : "bytes differ, ") + d.Count + " report differences"); for (int i = 0; i < d.Count && i < 4; i++) UxTest.Log("     " + d[i]); }
        }
        UxTest.Report(okCl == nc, "clip_prims15: " + okCl + " of " + nc + " cases (bytes, report, error text)");
        int nsu = 0, okSu = 0; var grids = new Dictionary<string, object>();
        string pyRoot = Environment.GetEnvironmentVariable("UX_PY_ROOT"); if (string.IsNullOrEmpty(pyRoot)) pyRoot = "";
        foreach (object co in UxJson.Arr(doc.Get("sunk")))
        {
            var c = UxJson.Obj(co); nsu++;
            string level = c.Get("level") as string; object hf = null;
            if (level != null)
            {
                if (!grids.TryGetValue(level, out hf))
                {
                    string f2 = Path.Combine(pyRoot, "work", level, "terrain", "hf.npy");
                    if (!File.Exists(f2)) { UxTest.Skip("_sunk_excess: no " + f2); continue; }
                    grids[level] = hf = Call(hfT, "Load", File.ReadAllBytes(f2));
                }
            }
            var mats = new List<double[]>();
            foreach (object mo in UxJson.Arr(c.Get("mats"))) { var ml = UxJson.Arr(mo); var m = new double[16]; for (int i = 0; i < 16; i++) m[i] = Hex((string)ml[i]); mats.Add(m); }
            double got = (double)Call(T, "SunkExcess", Prims((string)c.Get("data")), mats, Hex((string)c.Get("lift")), hf), want = Hex((string)c.Get("value"));
            if (BitConverter.DoubleToInt64Bits(got) == BitConverter.DoubleToInt64Bits(want)) okSu++;
            else if (nsu - okSu <= 5) UxTest.Log("     _sunk_excess case " + (nsu - 1) + " (" + (level ?? "no heightfield") + "): got " + PyFormat.Repr(got) + " want " + PyFormat.Repr(want));
        }
        UxTest.Report(okSu == nsu, "_sunk_excess: " + okSu + " of " + nsu + " values bit-identical");
    }

    static double Hex(string h) => BitConverter.Int64BitsToDouble(unchecked((long)ulong.Parse(h, NumberStyles.HexNumber, Inv)));
}
