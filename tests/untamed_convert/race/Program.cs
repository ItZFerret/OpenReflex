// tests/untamed_convert/race: the "race" stage (S8, UxRace + UxTdf07 + UxLapPacing + UxStartPack) against the Python reference.
//   numbers    UxRaceNum against the reference machine's numpy / OpenBLAS / CPython results (ref/num.json, bit for bit)
//   levels     the stage for all 12 levels with the current builds' options, from the disc and the Reflex library, the work store seeded
//              with Python's own level.json and terrain files: every output file against ref/work/<Level>/ (race.json, race/*)
//   variants   the options no current build uses, each once (ref/variants)
//   lappacing  UxLapPacing on every level's lap pacing file; startpack: the class report and the reseat map of every level
// The references are written by ref_race.py and ref_num.py (run.sh). JSON is compared as documents with exact numbers after the named
// environment fields are taken out (EnvFields: absolute paths, the research folder, tool md5s), and byte for byte after the reference's
// values are put into those fields.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;
using RaceNum = Reflex.Port.Content.UntamedImport.UxRaceNum;

static class Program
{
    /// The fields of race.json that are environment data by nature (not compared; listed in the report).
    static readonly string[] EnvFields =
    {
        "source.level_json",            // the absolute path of work/<Level>/level.json
        "source.template",              // the absolute path of templates/stock-SX_FortDodge
        "source.hf",                    // 'WP6 <absolute path of hf.npy> (md5 ...)': the md5 part is compared apart
        "source.tool_md5",              // md5 of ux_race.py
        "source.tdf07_to_reflex_md5",   // md5 of tdf07_to_reflex.py
        "tdf.source",                   // the absolute path of the disc's tdf07
        "tdf.file",                     // the absolute path of the written tdf
        "tdf.G5_source",                // 'research/tdf-race/out/summary.csv': the reference's research folder
        "tdf.research_tdf",             // the absolute path of the research converter's tdf
        "tdf.equals_research_tdf",      // the comparison with that file
        "freeride.layout.hf_source",    // = source.hf
        "lappacing.source",             // (--lappacing untamed) the absolute path of the lap pacing file
        "lappacing.file",               // (--lappacing untamed) the absolute path of the written asset
    };

    static double Bits(object hex) => BitConverter.Int64BitsToDouble(Convert.ToInt64((string)hex, 16));
    static double[] BitsOf(object list) { var l = UxJson.Arr(list); var r = new double[l.Count]; for (int i = 0; i < r.Length; i++) r[i] = Bits(l[i]); return r; }
    static bool Same(double a, double b) => BitConverter.DoubleToInt64Bits(a) == BitConverter.DoubleToInt64Bits(b);
    static bool Same(double[] a, double[] b) { if (a.Length != b.Length) return false; for (int i = 0; i < a.Length; i++) if (!Same(a[i], b[i])) return false; return true; }

    static object GetPath(JObj o, string path, out JObj parent, out string leaf)
    {
        var parts = path.Split('.'); parent = o;
        for (int i = 0; i + 1 < parts.Length; i++) { parent = parent?.Get(parts[i]) as JObj; }
        leaf = parts[parts.Length - 1];
        return parent?.Get(leaf);
    }
    static bool HasPath(JObj o, string path) { GetPath(o, path, out var p, out string leaf); return p != null && p.Has(leaf); }
    static void RemovePath(JObj o, string path) { GetPath(o, path, out var p, out string leaf); if (p != null) p.items.RemoveAll(kv => kv.Key == leaf); }

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "all";
        using (var env = UxTest.OpenEnv(args))
        {
            string refDir = UxTest.Arg(args, "ref", "UX_RACE_REF", Path.Combine(env.scratch, "ref"));
            string only = UxTest.Arg(args, "levels");
            if (what == "all" || what == "numbers") Numbers(Path.Combine(refDir, "num.json"));
            if (env.disc == null || env.stock == null)
            {
                UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)");
                return UxTest.Result("race");
            }
            var levels = new List<UxLevelRow>();
            foreach (var lv in UxCommon.Levels) if (only == null || Array.IndexOf(only.Split(','), lv.shortName) >= 0 || Array.IndexOf(only.Split(','), lv.disc) >= 0) levels.Add(lv);
            UxRaceTemplate tp = null;
            if (what == "all" || what == "levels" || what == "variants")
            {
                var sw = Stopwatch.StartNew();
                tp = new UxRaceTemplate(env.stock);
                UxTest.Log("template SX_FortDodge: " + tp.entities.Count + " kept entities with a name and a matrix, " + tp.dupes.Count + " duplicate names, floor " +
                    tp.floor.ToString("R", CultureInfo.InvariantCulture) + " ft (" + sw.Elapsed.TotalSeconds.ToString("0.0", CultureInfo.InvariantCulture) + " s)");
            }
            if (what == "all" || what == "levels") Levels(env, refDir, levels, tp);
            if (what == "all" || what == "variants") Variants(env, refDir, levels, tp);
            if (what == "all" || what == "lappacing") LapPacing(env, refDir, levels, args);
            if (what == "all" || what == "startpack") StartPack(env, refDir, levels);
        }
        return UxTest.Result("race");
    }

    // ------------------------------------------------------------------------------------------------------------ numbers
    static void Numbers(string file)
    {
        UxTest.Section("numbers: UxRaceNum against " + file);
        if (!File.Exists(file)) { UxTest.Skip("no " + file + " (ref_num.py writes it)"); return; }
        var o = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(file)));
        int bad = 0, n = 0;
        foreach (object c in UxJson.Arr(o.Get("inv4"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.Inv4(BitsOf(l[0])), BitsOf(l[1]))) bad++; }
        UxTest.Report(bad == 0, "np.linalg.inv of 4 x 4 matrices: " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("solve2"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.Solve(BitsOf(l[0]), BitsOf(l[1]), 2), BitsOf(l[2]))) bad++; }
        UxTest.Report(bad == 0, "np.linalg.solve of 2 x 2 systems: " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("matmul4"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.MatMul4(BitsOf(l[0]), BitsOf(l[1])), BitsOf(l[2]))) bad++; }
        UxTest.Report(bad == 0, "4 x 4 @ 4 x 4 (with zeros and signs): " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("transform")))
        {
            var l = UxJson.Arr(c); var M = BitsOf(l[0]); var P = BitsOf(l[1]); var W = BitsOf(l[2]);
            for (int i = 0; i < P.Length; i += 3)
            {
                n++; RaceNum.Transform(M, P[i], P[i + 1], P[i + 2], out double x, out double y, out double z);
                if (!Same(x, W[i]) || !Same(y, W[i + 1]) || !Same(z, W[i + 2])) bad++;
            }
        }
        UxTest.Report(bad == 0, "(m, 3) @ M[:3, :3] + M[3, :3] rows: " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("vecmat4"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.VecMat4(BitsOf(l[0]), 12, BitsOf(l[1]), 0), Bits(l[2]))) bad++; }
        UxTest.Report(bad == 0, "(W[3] @ A)[0]: " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("dot")))
        {
            var l = UxJson.Arr(c); var a = BitsOf(l[0]); var b = BitsOf(l[1]); n++;
            double d = a.Length == 2 ? RaceNum.Dot2(a[0], a[1], b[0], b[1]) : RaceNum.Dot3(a[0], a[1], a[2], b[0], b[1], b[2]);
            double nr = a.Length == 2 ? RaceNum.Norm2(a[0], a[1]) : RaceNum.Norm3(a[0], a[1], a[2]);
            if (!Same(d, Bits(l[2])) || !Same(nr, Bits(l[3])) || !Same(RaceNum.Dot(a, b), Bits(l[2]))) bad++;
        }
        UxTest.Report(bad == 0, "a @ b and np.linalg.norm of 2- and 3-vectors: " + (n - bad) + " of " + n + " bit-identical");
        bad = n = 0; int badNp = 0;
        foreach (object c in UxJson.Arr(o.Get("hypot")))
        {
            var l = UxJson.Arr(c); double x = Bits(l[0]), y = Bits(l[1]); n++;
            if (!Same(RaceNum.PyHypot(x, y), Bits(l[2]))) bad++;
            if (n <= 3000 && !Same(RaceNum.NpHypot(x, y), Bits(l[3]))) badNp++;
        }
        UxTest.Report(bad == 0, "math.hypot: " + (n - bad) + " of " + n + " bit-identical");
        UxTest.Report(badNp == 0, "np.hypot: " + (3000 - badNp) + " of 3000 bit-identical");
        bad = n = 0; int badMed = 0, badPct = 0, badNpSum = 0, badMean = 0;
        foreach (object c in UxJson.Arr(o.Get("sums")))
        {
            var l = UxJson.Arr(c); var a = BitsOf(l[0]); n++;
            if (!Same(RaceNum.PySum(a), Bits(l[1]))) bad++;
            if (!Same(RaceNum.Median(a), Bits(l[2]))) badMed++;
            if (!Same(RaceNum.Percentile(a, 90), Bits(l[3]))) badPct++;
            if (!Same(UxNp.Sum(a), Bits(l[4]))) badNpSum++;
            int m = Math.Min(16, a.Length); var head = new double[m]; Array.Copy(a, head, m);
            if (!Same(UxNp.Sum(head) / m, Bits(l[5]))) badMean++;
        }
        UxTest.Report(bad == 0 && badMed == 0 && badPct == 0 && badNpSum == 0 && badMean == 0, "sum() / np.median / np.percentile(90) / np.sum / np.mean of " + n + " lists: " +
            bad + " / " + badMed + " / " + badPct + " / " + badNpSum + " / " + badMean + " differ");
        bad = n = 0; int badText = 0;
        foreach (object c in UxJson.Arr(o.Get("round")))
        {
            var l = UxJson.Arr(c); double x = Bits(l[0]); n++;
            if (!Same(UxNp.PyRound(x, 1), Bits(l[1])) || !Same(UxNp.PyRound(x, 2), Bits(l[2])) || !Same(UxNp.PyRound(x, 3), Bits(l[3])) || !Same(UxNp.Round(x, 2), Bits(l[4])) || !Same(UxNp.Round(x, 4), Bits(l[5]))) bad++;
            if (RaceNum.PyFixed(x, 0) != (string)l[6] || RaceNum.PyFixed(x, 1) != (string)l[7] || RaceNum.PyFixed(x, 2) != (string)l[8] || RaceNum.PyG(x) != (string)l[9])
            { if (badText++ < 5) UxTest.Log("     " + PyFormat.Repr(x) + ": " + RaceNum.PyFixed(x, 0) + " " + RaceNum.PyFixed(x, 1) + " " + RaceNum.PyFixed(x, 2) + " " + RaceNum.PyG(x) + " want " + l[6] + " " + l[7] + " " + l[8] + " " + l[9]); }
        }
        UxTest.Report(bad == 0 && badText == 0, "round() / np.round and '%.0f' '%.1f' '%.2f' '%g' of " + n + " values: " + bad + " / " + badText + " differ");
        {
            var r = UxJson.Obj(o.Get("rng")); var first = BitsOf(r.Get("first"));
            var g = new RaceNum.Rng(7); ulong xor = 0; bool ok = true;
            for (int i = 0; i < 2000; i++) { double v = g.Random(); xor ^= (ulong)BitConverter.DoubleToInt64Bits(v); if (i < first.Length && !Same(v, first[i])) ok = false; }
            ok = ok && xor.ToString("x16") == (string)r.Get("xor");
            var pick = g.ChoiceIndices(BitsOf(r.Get("p")), 300); var want = UxJson.Arr(r.Get("pick"));
            bool okPick = true; for (int i = 0; i < 300; i++) if (pick[i] != UxJson.Int(want[i])) okPick = false;
            ulong uxor = 0; var u8 = BitsOf(r.Get("u")); bool okU = true;
            for (int i = 0; i < 600; i++) { double v = g.Random(); uxor ^= (ulong)BitConverter.DoubleToInt64Bits(v); if (i < u8.Length && !Same(v, u8[i])) okU = false; }
            okU = okU && uxor.ToString("x16") == (string)r.Get("u_xor");
            UxTest.Report(ok && okPick && okU, "np.random.default_rng(7): random() x 2000 " + (ok ? "equal" : "DIFFER") + ", choice(37, 300, p) " + (okPick ? "equal" : "DIFFERS") + ", random((300, 2)) " + (okU ? "equal" : "DIFFERS"));
        }
        bad = n = 0;
        foreach (object c in UxJson.Arr(o.Get("arange"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.Arange(Bits(l[0]), Bits(l[1]), Bits(l[2])), BitsOf(l[3]))) bad++; }
        foreach (object c in UxJson.Arr(o.Get("linspace"))) { var l = UxJson.Arr(c); n++; if (!Same(RaceNum.Linspace01((int)UxJson.Int(l[0])), BitsOf(l[1]))) bad++; }
        UxTest.Report(bad == 0, "np.arange / np.linspace: " + (n - bad) + " of " + n + " bit-identical");
    }

    // ------------------------------------------------------------------------------------------------------------ one run against a reference folder
    sealed class RunStat { public int files, identical, different, missing, extra; public double seconds; public string note = ""; }

    static UxRaceOptions OptionsOf(UxLevelRow lv, JObj overrides)
    {
        var o = UxRaceOptions.ForLevel(lv);
        if (overrides == null) return o;
        foreach (var kv in overrides.items)
            switch (kv.Key)
            {
                case "startgate_visual": o.startgateVisual = (string)kv.Value; break;
                case "gate_patch": o.gatePatch = (string)kv.Value; break;
                case "deck_respawn": o.deckRespawn = (string)kv.Value; break;
                case "minimal": o.minimal = (bool)kv.Value; break;
                case "sx_flags": o.sxFlags = UxJson.Int(kv.Value); break;
                case "tdf_pad": o.tdfPad = (string)kv.Value; break;
                case "dropgate_materials": o.dropgateMaterials = (string)kv.Value; break;
                case "lappacing": o.lapPacing = (string)kv.Value; break;
                case "board_flip": o.boardFlip = (bool)kv.Value; break;
                case "bale_edge_clear": o.baleEdgeClear = UxJson.Num(kv.Value); break;
                default: throw new UxException("variant option " + kv.Key);
            }
        return o;
    }

    /// Runs the stage into <scratch>/<workName>/<Level> and compares race.json and race/ with the reference folder.
    static RunStat RunAndCompare(UxTest.Env env, UxLevelRow lv, string workName, string refLevelDir, UxRaceOptions opt, UxRaceTemplate tp, string label)
    {
        var st = new RunStat();
        var ctx = env.Level(lv, workName);
        ctx.log = s => { };
        env.Seed(ctx, "level.json", "terrain/hf.npy", "terrain/excluded_tris.npy");
        if (opt.lapPacing == "untamed" && opt.lapPacingSource == null)
        {   // an extracted folder does not list the lap pacing file (UxDisc.Open's file filter): hand the bytes over
            string f = UxTest.Arg(new string[0], "folder", "UX_DISC_FOLDER");
            if (f != null && File.Exists(Path.Combine(f, "AI", lv.disc + "-lappacing.bxml"))) opt.lapPacingSource = File.ReadAllBytes(Path.Combine(f, "AI", lv.disc + "-lappacing.bxml"));
        }
        var sw = Stopwatch.StartNew();
        UxRaceResult res;
        try { res = UxRace.Convert(ctx, opt, tp); }
        catch (Exception e) { UxTest.Fail(label + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); return st; }
        st.seconds = sw.Elapsed.TotalSeconds;
        // ---- binary files: byte for byte
        string refRace = Path.Combine(refLevelDir, "race"), myRace = ctx.work.PathOf("race");
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string f in Directory.GetFiles(refRace))
        {
            string name = Path.GetFileName(f); seen.Add(name); st.files++;
            string mine = Path.Combine(myRace, name);
            if (!File.Exists(mine)) { st.missing++; UxTest.Fail(label + " race/" + name + ": not written"); continue; }
            byte[] got = File.ReadAllBytes(mine), want = File.ReadAllBytes(f);
            if (UxBin.Same(got, want)) { st.identical++; UxTest.Checks++; }
            else { st.different++; UxTest.Fail(label + " race/" + name + ": " + UxTest.Difference(got, want)); }
        }
        foreach (string f in Directory.GetFiles(myRace)) if (!seen.Contains(Path.GetFileName(f))) { st.extra++; UxTest.Fail(label + " race/" + Path.GetFileName(f) + ": not in the reference"); }
        // ---- race.json
        st.files++;
        byte[] gotJ = ctx.work.ReadBytes("race.json"), wantJ = File.ReadAllBytes(Path.Combine(refLevelDir, "race.json"));
        var mineDoc = UxJson.Obj(UxJson.ParseUtf8(gotJ)); var refDoc = UxJson.Obj(UxJson.ParseUtf8(wantJ));
        // the md5 inside the height field source strings is content, the path in front of it is not
        foreach (string p in new[] { "source.hf", "freeride.layout.hf_source" })
        {
            string a = GetPath(mineDoc, p, out _, out _) as string, b = GetPath(refDoc, p, out _, out _) as string;
            if (a == null && b == null) continue;
            UxTest.Check(a != null && b != null && a.Substring(a.LastIndexOf("(md5", StringComparison.Ordinal) < 0 ? 0 : a.LastIndexOf("(md5", StringComparison.Ordinal)) ==
                b.Substring(b.LastIndexOf("(md5", StringComparison.Ordinal) < 0 ? 0 : b.LastIndexOf("(md5", StringComparison.Ordinal)), label + " race.json " + p + ": the md5 part: got " + a + ", want " + b);
        }
        // the tool md5s are environment fields; a difference only says the reference tools changed after the port
        foreach (string p in new[] { "source.tool_md5", "source.tdf07_to_reflex_md5" })
        {
            string a = GetPath(mineDoc, p, out _, out _) as string, b = GetPath(refDoc, p, out _, out _) as string;
            if (a != b) st.note += " [" + p + " of the reference is " + b + ", the port was made from " + a + "]";
        }
        // the documents without the environment fields: exact (tdf.research_G5 stays in: the reference reads it from its research
        // folder, the port takes the TDF's own global 5, and the two numbers must be equal)
        var mineCut = UxJson.Obj(UxJson.ParseUtf8(gotJ)); var refCut = UxJson.Obj(UxJson.ParseUtf8(wantJ));
        foreach (string p in EnvFields)
        {
            if (HasPath(mineCut, p) != HasPath(refCut, p)) UxTest.Fail(label + " race.json: field " + p + " is in only one of the documents");
            RemovePath(mineCut, p); RemovePath(refCut, p);
        }
        var diffs = UxTest.JsonDifferences(mineCut, refCut);
        // byte for byte: the reference document with this run's values in the environment fields, written as Python writes it, must be
        // the bytes of the written file (key order, number text, indentation, the final newline)
        foreach (string p in EnvFields)
        {
            object v = GetPath(mineDoc, p, out var mp, out string leaf);
            GetPath(refDoc, p, out var rp, out _);
            if (mp != null && mp.Has(leaf) && rp != null) rp.Set(leaf, v);
        }
        byte[] expect = System.Text.Encoding.UTF8.GetBytes(UxJson.DumpsUx(refDoc));
        bool bytesSame = UxBin.Same(expect, gotJ);
        if (!bytesSame) UxTest.Log("     " + label + " race.json bytes: " + UxTest.Difference(gotJ, expect));
        if (diffs.Count == 0 && bytesSame) { st.identical++; UxTest.Checks++; }
        else
        {
            st.different++;
            UxTest.Fail(label + " race.json: " + diffs.Count + (diffs.Count >= 50 ? "+" : "") + " JSON differences" + (diffs.Count == 0 ? " but the bytes differ after the environment fields are substituted" : ""));
            for (int i = 0; i < diffs.Count && i < UxTest.MaxReports; i++) UxTest.Log("     " + diffs[i]);
        }
        // the in-memory result is what was written
        UxTest.Check(UxBin.Same(res.tdf, ctx.work.ReadBytes("race/" + (opt.slotName ?? "Beta_Sx_Track_Slot_1") + "-sx.tdf")), label + ": the returned tdf bytes are the written file");
        return st;
    }

    static void Levels(UxTest.Env env, string refDir, List<UxLevelRow> levels, UxRaceTemplate tp)
    {
        UxTest.Section("levels: the stage with the current builds' options against " + refDir + "/work");
        if (!Directory.Exists(Path.Combine(refDir, "work"))) { UxTest.Skip("no " + refDir + "/work (ref_race.py writes it)"); return; }
        // the per-track option table against the reference's commands.json (as ref_race.py read it)
        var opts = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(refDir, "options.json"))));
        foreach (var lv in UxCommon.Levels)
        {
            var want = opts.Get(lv.disc) as JObj; var have = UxRaceOptions.ForLevel(lv);
            UxTest.Check(want != null && UxJson.Num(want.Get("bale_edge_clear")) == have.baleEdgeClear && (string)want.Get("startgate_visual") == have.startgateVisual,
                lv.disc + ": UxRaceOptions.ForLevel = the current build's options (" + (want == null ? "none" : UxJson.Dumps(want)) + ")");
        }
        UxTest.Log(string.Format("{0,-22} {1,6} {2,9} {3,9} {4,7} {5,8}  {6}", "level", "files", "identical", "different", "missing", "seconds", "options"));
        double total = 0;
        foreach (var lv in levels)
        {
            string refLevel = Path.Combine(refDir, "work", lv.disc);
            if (!Directory.Exists(refLevel)) { UxTest.Skip(lv.disc + ": no reference output"); continue; }
            var opt = UxRaceOptions.ForLevel(lv);
            var st = RunAndCompare(env, lv, "work", refLevel, opt, tp, lv.disc);
            total += st.seconds;
            UxTest.Log(string.Format(CultureInfo.InvariantCulture, "{0,-22} {1,6} {2,9} {3,9} {4,7} {5,8:0.00}  {6}{7}", lv.disc, st.files, st.identical, st.different, st.missing + st.extra, st.seconds,
                "bale_edge_clear " + opt.baleEdgeClear.ToString(CultureInfo.InvariantCulture) + ", startgate_visual " + opt.startgateVisual, st.note));
        }
        UxTest.Log("levels: " + total.ToString("0.0", CultureInfo.InvariantCulture) + " s for " + levels.Count + " levels (the template is read once before)");
        UxTest.Log("environment fields not compared (" + EnvFields.Length + "): " + string.Join(", ", EnvFields));

        // every level at once on one shared template (what an import of several tracks does): the same files as the runs above
        {
            var sw = Stopwatch.StartNew(); int same = 0, files = 0; var errors = new List<string>();
            var ctxs = new List<UxContext>();
            foreach (var lv in levels) { var c = env.Level(lv, "work-parallel"); c.log = s => { }; env.Seed(c, "level.json", "terrain/hf.npy", "terrain/excluded_tris.npy"); ctxs.Add(c); }
            sw.Restart();
            System.Threading.Tasks.Parallel.ForEach(ctxs, c =>
            {
                try { UxRace.Convert(c, UxRaceOptions.ForLevel(c.lv), tp); }
                catch (Exception e) { lock (errors) errors.Add(c.lv.disc + ": " + e.Message); }
            });
            double par = sw.Elapsed.TotalSeconds;
            foreach (var c in ctxs)
            {
                string a = Path.Combine(env.scratch, "work", c.lv.disc);
                var mine = new List<string> { "race.json" }; foreach (string f in c.work.List("race")) mine.Add(f);
                foreach (string f in mine) { files++; string other = Path.Combine(a, f.Replace('/', Path.DirectorySeparatorChar)); if (File.Exists(other) && UxBin.Same(File.ReadAllBytes(other), c.work.ReadBytes(f))) same++; }
                foreach (string f in Directory.GetFiles(Path.Combine(a, "race"))) if (!c.work.Exists("race/" + Path.GetFileName(f))) files++;
            }
            UxTest.Report(errors.Count == 0 && same == files && files > 0, "all " + ctxs.Count + " levels converted in parallel on one template: " + same + " of " + files + " files equal to the one-by-one runs, " +
                par.ToString("0.0", CultureInfo.InvariantCulture) + " s in all" + (errors.Count > 0 ? "; " + string.Join("; ", errors) : ""));
        }

        // a negative control: the comparison must fail when an option differs (Columbus without its --bale-edge-clear 4)
        var col = UxCommon.Level("columbus");
        if (levels.Contains(col) && Directory.Exists(Path.Combine(refDir, "work", col.disc)))
        {
            int checks = UxTest.Checks, failures = UxTest.Failures;
            var keep = Console.Out; Console.SetOut(TextWriter.Null);
            RunStat st;
            try { st = RunAndCompare(env, col, "negative", Path.Combine(refDir, "work", col.disc), new UxRaceOptions(), tp, "negative control"); }
            finally { Console.SetOut(keep); }
            int caught = UxTest.Failures - failures;
            UxTest.Checks = checks; UxTest.Failures = failures;
            UxTest.Report(caught > 0 && st.different == 2, "negative control: Columbus without its bale edge clear is reported as different (" + st.different + " of " + st.files + " files, " + caught + " failed checks)");
        }
    }

    static void Variants(UxTest.Env env, string refDir, List<UxLevelRow> levels, UxRaceTemplate tp)
    {
        UxTest.Section("variants: the options no current build uses");
        string vf = Path.Combine(refDir, "variants.json");
        if (!File.Exists(vf)) { UxTest.Skip("no " + vf + " (ref_race.py writes it)"); return; }
        var vs = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(vf)));
        foreach (var kv in vs.items)
        {
            var v = UxJson.Obj(kv.Value); var lv = UxCommon.Level((string)v.Get("level"));
            if (!levels.Contains(lv)) continue;
            string refLevel = Path.Combine(refDir, "variants", kv.Key, lv.disc);
            if (!Directory.Exists(refLevel)) { UxTest.Skip("variant " + kv.Key + ": no reference output"); continue; }
            var opt = OptionsOf(lv, UxJson.Obj(v.Get("options")));
            var st = RunAndCompare(env, lv, "variants/" + kv.Key, refLevel, opt, tp, "variant " + kv.Key);
            UxTest.Log(string.Format(CultureInfo.InvariantCulture, "{0,-26} {1,-22} {2} files, {3} identical, {4} different, {5} missing / extra, {6:0.00} s  {7}", kv.Key, lv.disc, st.files, st.identical, st.different,
                st.missing + st.extra, st.seconds, UxJson.Dumps(v.Get("options"))));
        }
    }

    // ------------------------------------------------------------------------------------------------------------ lap pacing
    static void LapPacing(UxTest.Env env, string refDir, List<UxLevelRow> levels, string[] args)
    {
        UxTest.Section("lappacing: UxLapPacing.ToReflex / EngineCheck / Convert on every level's file");
        string dir = Path.Combine(refDir, "lappacing");
        if (!Directory.Exists(dir)) { UxTest.Skip("no " + dir + " (ref_race.py writes it)"); return; }
        int ok = 0, n = 0;
        foreach (var lv in levels)
        {
            string rb = Path.Combine(dir, lv.disc + ".bxml"), rj = Path.Combine(dir, lv.disc + ".json");
            if (!File.Exists(rb) || !File.Exists(rj)) { UxTest.Skip(lv.disc + ": no lap pacing reference"); continue; }
            byte[] raw = null;
            var d = env.disc.Level(lv);
            if (d.root != null)
            {
                var f = d.volume.Find((d.root.Length == 0 ? "" : d.root + "/") + "AI/" + lv.disc + "-lappacing.bxml");
                if (f != null) raw = d.volume.Read(f, CancellationToken.None);
            }
            string folder = UxTest.Arg(args, "folder", "UX_DISC_FOLDER");
            if (raw == null && folder != null && File.Exists(Path.Combine(folder, "AI", lv.disc + "-lappacing.bxml"))) raw = File.ReadAllBytes(Path.Combine(folder, "AI", lv.disc + "-lappacing.bxml"));
            if (raw == null) { UxTest.Skip(lv.disc + ": the source does not list AI/" + lv.disc + "-lappacing.bxml"); continue; }
            n++;
            var want = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(rj)));
            byte[] got = UxLapPacing.ToReflex(raw, out JObj info, "Supercross");
            var tab = UxLapPacing.EngineCheck(got, out List<string> probs, "Supercross", 0);
            var tab0 = UxLapPacing.EngineCheck(raw, out List<string> probs0, "Supercross", 0);
            byte[] conv = UxLapPacing.Convert(raw, out var keys, out var mapping, "Supercross");
            var pl = new List<object>(); foreach (string p in probs) pl.Add(p);
            var pl0 = new List<object>(); foreach (string p in probs0) pl0.Add(p);
            var kl = new List<object>(); foreach (string k in keys) kl.Add(k);
            var mine = new JObj().Set("info", info).Set("table", tab).Set("problems", pl).Set("source_table", tab0).Set("source_problems", pl0).Set("convert_md5", UxBin.Md5(conv))
                .Set("convert_keys", kl).Set("convert_mapping", mapping);
            var diffs = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(mine)), want);
            bool same = UxBin.Same(got, File.ReadAllBytes(rb)) && diffs.Count == 0;
            if (same) ok++;
            UxTest.Check(same, lv.disc + " lap pacing: bytes " + UxTest.Difference(got, File.ReadAllBytes(rb)) + ", " + diffs.Count + " differences in info / table / problems" + (diffs.Count > 0 ? ": " + diffs[0] : ""));
        }
        UxTest.Report(ok == n && n > 0, "lap pacing: " + ok + " of " + n + " levels byte-identical (asset, info, engine check table of the output and of the source)");
    }

    // ------------------------------------------------------------------------------------------------------------ start pack
    static void StartPack(UxTest.Env env, string refDir, List<UxLevelRow> levels)
    {
        UxTest.Section("startpack: the class report and the reseat map of every level (ux_startpack)");
        string dir = Path.Combine(refDir, "startpack");
        if (!Directory.Exists(dir)) { UxTest.Skip("no " + dir + " (ref_race.py writes it)"); return; }
        int ok = 0, n = 0;
        foreach (var lv in levels)
        {
            string rj = Path.Combine(dir, lv.disc + ".json");
            if (!File.Exists(rj)) { UxTest.Skip(lv.disc + ": no start pack reference"); continue; }
            n++;
            var ctx = env.Level(lv, "startpack");
            env.Seed(ctx, "level.json", "terrain/hf.npy");
            JObj mine;
            try { mine = StartPackReport(ctx); }
            catch (Exception e) { UxTest.Fail(lv.disc + " start pack: " + e.Message); continue; }
            var diffs = UxTest.JsonDifferences(UxJson.Parse(UxJson.DumpsUx(mine)), UxJson.ParseUtf8(File.ReadAllBytes(rj)));
            if (diffs.Count == 0) ok++;
            UxTest.Check(diffs.Count == 0, lv.disc + " start pack: " + diffs.Count + " differences" + (diffs.Count > 0 ? ": " + diffs[0] : ""));
        }
        UxTest.Report(ok == n && n > 0, "start pack: " + ok + " of " + n + " levels equal (PopDown footprints, chute turns / outside / frozen, class report, reseat 6 ft)");
    }
    /// What ref_race.py writes into startpack/<Level>.json, from the port.
    static JObj StartPackReport(UxContext ctx)
    {
        var d = ctx.work.ReadJsonObj("level.json");
        var ents = UxRace.Entities(d); var idx = new UxRace.ExtractIndex(d);
        UxRace.TerrainQuery tq = null;
        var hf = new UxRace.HeightField(ctx.lv, ctx.work, () => tq ?? (tq = new UxRace.TerrainQuery(ctx.lv, ctx.disc, d, CancellationToken.None)));
        var cw = new UxRace.CollisionWorld(ctx.lv, ctx.disc, d, ents, idx, new Dictionary<string, double[]>(), null, CancellationToken.None);
        var bales = UxStartPack.Footprints(ents, cw);
        var secs = UxStartPack.TdfSections(ctx.disc);
        var models = UxStartPack.ChuteModels(secs, hf.At);
        var cls = UxStartPack.ClassReport(models, bales, UxStartPack.Classify(models, bales));
        var rs = UxStartPack.Reseat(secs, bales, hf.At, 6.0, out var rcls);
        var fp = new List<object>(); foreach (var b in bales) fp.Add((long)b.px.Length);
        var chutes = new List<object>(); var turn = new List<object>(); var outside = new List<object>(); var frozen = new List<object>();
        foreach (var M in models)
        {
            chutes.Add((long)M.si);
            var t = new List<object>(); foreach (double v in M.turn) t.Add(v); turn.Add(t);
            var o = new List<object>(); foreach (int v in M.outside) o.Add((long)v); outside.Add(o);
            var f = new List<object>(); foreach (bool v in M.frozen) f.Add(v); frozen.Add(f);
        }
        var reseat = new List<object>(); foreach (var kv in rs) reseat.Add(new List<object> { kv.Key, kv.Value });
        return new JObj().Set("popdowns", (long)bales.Count).Set("footprint_points", fp).Set("chutes", chutes).Set("turn", turn).Set("outside", outside).Set("frozen", frozen)
            .Set("cls", cls).Set("reseat", reseat).Set("reseat_class", rcls);
    }
}
