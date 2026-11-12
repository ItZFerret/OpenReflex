// tests/untamed_convert/level: the level stage (UxLevel = ux_level.py) against the Python reference, headless.
//   level [<level> ...] --scratch <dir> --ref <dir> [--folder <extracted disc> | --iso <disc image>] [--also <the other disc source>]
// <ref> holds what the CURRENT ux_level.py writes (ref_level.py: <ref>/<Level>/level.json, level_fr.json, <ref>/rigid-drop/...) and the
// numerics digests (ref_numerics.py: <ref>/numerics/*.json); run.sh writes both. The stored stage outputs work/<Level>/level*.json of
// the reference (UX_PY_ROOT) are compared too.
// Per level and file: (1) the produced document equals the reference's except the ENVIRONMENT fields named in EnvFields (absolute
// paths of the reference machine, md5 of the reference's tool files); (2) with those fields set to the reference's values the produced
// document, written again, equals the reference file byte for byte. Nothing else is excluded.
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
    /// The fields of level.json that are data of the reference's environment (excluded by name from the document comparison).
    static readonly string[][] EnvFields =
    {
        new[] { "source", "scene" }, new[] { "source", "extract_index" }, new[] { "template", "dir" },
        new[] { "source", "tool_md5" }, new[] { "source", "uxmesh_md5" },
        new[] { "source", "readers", "uxscene.py" }, new[] { "source", "readers", "uxmesh.py" }, new[] { "source", "readers", "uxmat.py" },
        new[] { "source", "readers", "uxtex.py" }, new[] { "source", "readers", "ux_common.py" },
    };
    static readonly string[] Files = { "level.json", "level_fr.json" };

    static object At(JObj d, string[] path, out JObj parent)
    {
        parent = d;
        for (int i = 0; i + 1 < path.Length; i++) { parent = parent?.Get(path[i]) as JObj; }
        return parent?.Get(path[path.Length - 1]);
    }
    static bool HasAt(JObj d, string[] path) { At(d, path, out var parent); return parent != null && parent.Has(path[path.Length - 1]); }

    /// Copies the environment fields of `from` into `into` (a field the reference lacks is removed). Returns the fields whose value differed.
    static List<string> PatchEnv(JObj into, JObj from)
    {
        var changed = new List<string>();
        foreach (var path in EnvFields)
        {
            object want = At(from, path, out _); bool has = HasAt(from, path);
            object have = At(into, path, out var parent);
            if (parent == null) continue;
            string key = path[path.Length - 1];
            if (!has) { if (parent.Has(key)) { parent.items.RemoveAll(kv => kv.Key == key); changed.Add(string.Join(".", path) + " (not in the reference)"); } continue; }
            if (!parent.Has(key) || !Equals(have, want)) changed.Add(string.Join(".", path));
            parent.Set(key, want);
        }
        return changed;
    }

    /// One produced file against one reference file. Returns true when it is byte-identical once the environment fields are the reference's.
    static bool CompareFile(string what, byte[] got, byte[] want, out List<string> envDiff)
    {
        envDiff = new List<string>();
        if (got == null || want == null) { UxTest.Fail(what + ": " + UxTest.Difference(got, want)); return false; }
        var mine = UxJson.Obj(UxJson.ParseUtf8(got)); var theirs = UxJson.Obj(UxJson.ParseUtf8(want));
        envDiff = PatchEnv(mine, theirs);
        var d = UxTest.JsonDifferences(mine, theirs);
        bool ok = UxTest.Check(d.Count == 0, what + ": " + d.Count + (d.Count >= 50 ? "+" : "") + " JSON differences outside the environment fields");
        for (int i = 0; i < d.Count && i < UxTest.MaxReports; i++) UxTest.Log("     " + d[i]);
        byte[] again = new UTF8Encoding(false).GetBytes(UxJson.DumpsUx(mine));
        ok &= UxTest.SameBytes(what + " (bytes, environment fields as the reference's)", again, want);
        return ok;
    }

    static byte[] ReadOrNull(string path) => File.Exists(path) ? File.ReadAllBytes(path) : null;
    static string F(double v) => v.ToString("0.00", CultureInfo.InvariantCulture);

    static void Rules(string refDir)
    {
        UxTest.Section("number rules (numpy float32 text, '%.nf', arctan2)");
        string dir = Path.Combine(refDir, "numerics");
        if (!File.Exists(Path.Combine(dir, "fixed.json"))) { UxTest.Skip("no " + dir + "/f32.json, atan2.json, fixed.json (ref_numerics.py writes them)"); return; }
        var f32 = UxJson.Arr(UxJson.Parse(File.ReadAllText(Path.Combine(dir, "f32.json"))));
        int bad = 0;
        foreach (List<object> row in f32)
        {
            float v = BitConverter.Int32BitsToSingle(unchecked((int)(uint)UxJson.Int(row[0])));
            double got = UxLevel.F32Short(v, out string text), want = UxJson.Num(row[1]);
            if (BitConverter.DoubleToInt64Bits(got) != BitConverter.DoubleToInt64Bits(want) || text != PyFormat.Repr(want)) { if (bad++ < 5) UxTest.Log("     float32 0x" + UxJson.Int(row[0]).ToString("x8") + ": got " + PyFormat.Repr(got) + " want " + PyFormat.Repr(want)); }
        }
        UxTest.Report(bad == 0, "float(str(np.float32(x))) and its JSON text: " + (f32.Count - bad) + " of " + f32.Count + " equal the reference's");
        var fx = UxJson.Arr(UxJson.Parse(File.ReadAllText(Path.Combine(dir, "fixed.json"))));
        bad = 0;
        foreach (List<object> row in fx)
        {
            string got = UxLevel.Fixed(UxJson.Num(row[0]), (int)UxJson.Int(row[1]));
            if (got != (string)row[2]) { if (bad++ < 5) UxTest.Log("     '%." + row[1] + "f' % " + PyFormat.Repr(UxJson.Num(row[0])) + ": got " + got + " want " + row[2]); }
        }
        UxTest.Report(bad == 0, "'%.nf' formatting: " + (fx.Count - bad) + " of " + fx.Count + " equal Python's");
        var at = UxJson.Arr(UxJson.Parse(File.ReadAllText(Path.Combine(dir, "atan2.json"))));
        int notExact = 0, notCrt = 0, crtNotExact = 0, mathNotCrt = 0;
        foreach (List<object> row in at)
        {
            double y = UxJson.Num(row[0]), x = UxJson.Num(row[1]), crt = UxJson.Num(row[2]), exact = UxJson.Num(row[3]);
            double got = UxLevel.Atan2(y, x);
            if (BitConverter.DoubleToInt64Bits(got) != BitConverter.DoubleToInt64Bits(exact)) { if (notExact++ < 5) UxTest.Log("     arctan2(" + PyFormat.Repr(y) + ", " + PyFormat.Repr(x) + "): got " + PyFormat.Repr(got) + ", correctly rounded " + PyFormat.Repr(exact)); }
            if (got != crt) notCrt++;
            if (crt != exact) crtNotExact++;
            if (Math.Atan2(y, x) != crt) mathNotCrt++;
        }
        UxTest.Report(notExact == 0, "UxLevel.Atan2 (double-double): " + (at.Count - notExact) + " of " + at.Count + " are the correctly rounded arctan2 (decimal arithmetic)");
        s_libraryAtan2IsReference = mathNotCrt == 0;
        UxTest.Log("note the reference's math.atan2 (C library): " + crtNotExact + " of " + at.Count + " are not correctly rounded; UxLevel.Atan2 differs from it in " + notCrt +
            "; this runtime's Math.Atan2 differs from it in " + mathNotCrt);
    }

    static bool s_libraryAtan2IsReference;     // this runtime's Math.Atan2 gave the reference's math.atan2 on every vector
    static int s_placed, s_elevSame, s_elevUsed;

    static long Ulps(double a, double b) => Math.Abs(BitConverter.DoubleToInt64Bits(a) - BitConverter.DoubleToInt64Bits(b));

    /// The unrounded numbers behind the sky dome / underworld rules and the free-ride yaw against sky_dome_facts, placed_top_y,
    /// terrain_floor and pose_record of the reference. Strict (bit for bit): the terrain floor, the yaw, every radius and top y, and the
    /// elevation percentile computed with the reference's C library arctan2 (when this runtime's Math.Atan2 is that function).
    /// Counted allowance: the elevation percentile through UxLevel.Atan2 (the correctly rounded arctan2) may differ in the last bit,
    /// because the C library's arctan2 is not correctly rounded; such a difference must not change what level.json holds (the
    /// comparison with 60 degrees, the text '%.0f', round(x, 2)).
    static void Numerics(UxContext ctx, string refDir)
    {
        string path = Path.Combine(refDir, "numerics", ctx.lv.disc + ".json");
        if (!File.Exists(path)) { UxTest.Skip(ctx.lv.disc + ": no " + path + " (ref_numerics.py)"); return; }
        var want = UxJson.Obj(UxJson.Parse(File.ReadAllText(path)));
        var got = UxLevel.Numerics(ctx.disc);
        bool Same(object a, object b) => a == null || b == null ? a == null && b == null : BitConverter.DoubleToInt64Bits(UxJson.Num(a)) == BitConverter.DoubleToInt64Bits(UxJson.Num(b)) || double.IsNaN(UxJson.Num(a)) && double.IsNaN(UxJson.Num(b));
        UxTest.Check(Same(got.Get("floor_y"), want.Get("floor_y")), ctx.lv.disc + ": terrain floor " + got.Get("floor_y") + ", reference " + want.Get("floor_y"));
        UxTest.Check(Same(got.Get("yaw_deg"), want.Get("yaw_deg")), ctx.lv.disc + ": free-ride yaw " + got.Get("yaw_deg") + ", reference " + want.Get("yaw_deg"));
        var ge = UxJson.Arr(got.Get("entities")); var we = UxJson.Arr(want.Get("entities"));
        UxTest.Check(ge.Count == we.Count, ctx.lv.disc + ": " + ge.Count + " placed surfaces, reference " + we.Count);
        int n = Math.Min(ge.Count, we.Count), radBad = 0, elBad = 0, elFar = 0, elEffect = 0, libBad = 0, topBad = 0, idBad = 0, used = 0; long maxUlps = 0;
        for (int i = 0; i < n; i++)
        {
            var g = (List<object>)ge[i]; var w = (List<object>)we[i];
            if (UxJson.Int(g[0]) != UxJson.Int(w[0])) { idBad++; continue; }
            double rad = UxJson.Num(w[1]), el = UxJson.Num(g[2]), wel = UxJson.Num(w[2]);
            if (rad > UxLevel.SKY_MIN_RADIUS_FT) used++;
            if (!Same(g[1], w[1])) { if (radBad++ < 3) UxTest.Log("     entity " + g[0] + " rad " + PyFormat.Repr(UxJson.Num(g[1])) + " want " + PyFormat.Repr(rad)); }
            if (!Same(g[3], w[3])) { if (topBad++ < 3) UxTest.Log("     entity " + g[0] + " top " + PyFormat.Repr(UxJson.Num(g[3])) + " want " + PyFormat.Repr(UxJson.Num(w[3]))); }
            if (!Same(g[4], w[2])) { if (libBad++ < 3) UxTest.Log("     entity " + g[0] + " el95 (Math.Atan2) " + PyFormat.Repr(UxJson.Num(g[4])) + " want " + PyFormat.Repr(wel)); }
            if (!Same(g[2], w[2]))
            {
                elBad++; long u = Ulps(el, wel); maxUlps = Math.Max(maxUlps, u);
                if (u > 1) elFar++;
                // what level.json takes from the value (only beyond the sky radius): the threshold, '%.0f', round(x, 2)
                bool effect = (el > UxLevel.SKY_MIN_ELEV_DEG) != (wel > UxLevel.SKY_MIN_ELEV_DEG) || UxLevel.Fixed(el, 0) != UxLevel.Fixed(wel, 0) || UxNp.PyRound(el, 2) != UxNp.PyRound(wel, 2);
                if (effect) elEffect++;
                UxTest.Log("note " + ctx.lv.disc + " entity " + g[0] + ": elevation p95 " + PyFormat.Repr(el) + ", reference " + PyFormat.Repr(wel) + " (" + u + " ulp; surface radius " +
                    PyFormat.Repr(rad) + " ft, " + (rad > UxLevel.SKY_MIN_RADIUS_FT ? "beyond" : "inside") + " the sky radius; " + (effect ? "CHANGES" : "does not change") + " level.json)");
            }
        }
        s_placed += n; s_elevSame += n - elBad; s_elevUsed += used;
        bool strict = idBad == 0 && radBad == 0 && topBad == 0 && elFar == 0 && elEffect == 0 && (libBad == 0 || !s_libraryAtan2IsReference);
        UxTest.Report(strict, ctx.lv.disc + " numerics, unrounded: " + n + " placed surfaces (" + used + " beyond the sky radius); bit-identical to the reference: radius " + (n - radBad) +
            ", top y " + (n - topBad) + ", elevation p95 with the C library's arctan2 " + (n - libBad) + ", with UxLevel.Atan2 " + (n - elBad) + (elBad > 0 ? " (the others within " + maxUlps + " ulp)" : ""));
    }

    static int Main(string[] args)
    {
        var levels = new List<UxLevelRow>();
        for (int i = 0; i < args.Length; i++) { if (args[i].StartsWith("--", StringComparison.Ordinal)) { i++; continue; } levels.Add(UxCommon.Level(args[i])); }
        if (levels.Count == 0) levels.AddRange(UxCommon.Levels);
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null) { UxTest.Skip("the disc is needed (--folder / UX_DISC_FOLDER or --iso / UX_DISC_ISO)"); return UxTest.Result("level"); }
            string refDir = UxTest.Arg(args, "ref", "UX_LEVEL_REF", Path.Combine(env.scratch, "pyref"));
            Rules(refDir);

            UxTest.Section("the level stage, " + levels.Count + " levels from " + env.disc.volume.displayName + " (default options: kinds sx + fr, rigid models static)");
            var rows = new List<string>(); double total = 0; var envNotes = new SortedSet<string>(StringComparer.Ordinal);
            int filesCompared = 0, filesIdentical = 0, storedIdentical = 0, storedCompared = 0;
            foreach (var lv in levels)
            {
                var ctx = env.Level(lv, "work");
                ctx.log = s => { };
                var clock = Stopwatch.StartNew();
                UxLevelResult r;
                try { r = UxLevel.Convert(ctx, new UxLevelOptions(), null); }
                catch (Exception ex) { UxTest.Fail(lv.disc + ": " + ex.GetType().Name + ": " + ex.Message); continue; }
                double seconds = clock.Elapsed.TotalSeconds; total += seconds;
                int same = 0, sameStored = 0;
                foreach (string f in Files)
                {
                    byte[] got = ctx.work.Exists(f) ? ctx.work.ReadBytes(f) : null;
                    byte[] cur = ReadOrNull(Path.Combine(refDir, lv.disc, f));
                    if (cur == null) UxTest.Skip(lv.disc + " " + f + ": no current-Python reference in " + refDir + " (ref_level.py)");
                    else { filesCompared++; if (CompareFile(lv.disc + " " + f + " against the current ux_level.py", got, cur, out var ed)) { same++; filesIdentical++; } foreach (string e in ed) envNotes.Add(e); }
                    byte[] stored = ReadOrNull(Path.Combine(env.PyWork(lv), f));
                    if (stored == null) UxTest.Skip(lv.disc + " " + f + ": no stored stage output in " + env.PyWork(lv));
                    else { storedCompared++; if (CompareFile(lv.disc + " " + f + " against the stored work/" + lv.disc + "/" + f, got, stored, out _)) { sameStored++; storedIdentical++; } }
                }
                // no other file is written
                var extra = ctx.work.List("", "*", true).FindAll(p => Array.IndexOf(Files, p) < 0);
                UxTest.Check(extra.Count == 0, lv.disc + ": files written besides level.json and level_fr.json: " + string.Join(", ", extra));
                var sx = r.kinds.Count > 0 ? r.kinds[0] : null;
                rows.Add(string.Format(CultureInfo.InvariantCulture, "{0,-22} {1,5} {2,5} {3,5} | {4}/2 {5}/2 | {6,6} s (scene {7}, assets {8}, kinds {9}, write {10})", lv.disc, sx?.entities, sx?.by_class.Get("keep"), sx?.warnings, same, sameStored,
                    F(seconds), F(r.secondsScene), F(r.secondsAssets), F(r.secondsBuild), F(r.secondsWrite)));
                Numerics(ctx, refDir);
                ctx.disc.ClearCache();
            }
            UxTest.Log("");
            UxTest.Log("level                   ents  keep  warn | files identical to: current Python, stored work/ | seconds");
            foreach (string row in rows) UxTest.Log(row);
            UxTest.Report(filesIdentical == filesCompared && filesCompared > 0, "level.json / level_fr.json: " + filesIdentical + " of " + filesCompared + " byte-identical to the current ux_level.py output, " +
                storedIdentical + " of " + storedCompared + " to the stored work/ outputs (environment fields as the reference's)");
            UxTest.Log("note environment fields whose default value differs from a reference's: " + (envNotes.Count == 0 ? "none" : string.Join(", ", envNotes)));
            UxTest.Log("note elevation percentile through UxLevel.Atan2: " + s_elevSame + " of " + s_placed + " bit-identical to the reference's (C library arctan2); " + s_elevUsed +
                " surfaces lie beyond the sky radius, where the value is used");
            UxTest.Log("note " + F(total) + " s for " + levels.Count + " levels (" + F(total / Math.Max(1, levels.Count)) + " s per level, both kinds), " + Environment.ProcessorCount + " logical processors");

            // ---- the options: the environment fields through UxLevelOptions (the written file itself is the reference's bytes),
            // one worker thread, the other disc source, rigid models dropped
            UxTest.Section("options, determinism");
            foreach (var lv in levels)
            {
                if (lv.shortName != "norton" && lv.shortName != "lincoln" && lv.shortName != "harrispark") continue;
                byte[] cur = ReadOrNull(Path.Combine(refDir, lv.disc, "level.json"));
                if (cur == null) { UxTest.Skip(lv.disc + ": no current-Python reference"); continue; }
                var want = UxJson.Obj(UxJson.ParseUtf8(cur));
                var wantFr = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(refDir, lv.disc, "level_fr.json"))));
                var src = (JObj)want.Get("source");
                var md5 = new Dictionary<string, string>(StringComparer.Ordinal) { { "ux_level.py", (string)src.Get("tool_md5") } };
                foreach (var kv in ((JObj)src.Get("readers")).items) md5[kv.Key] = (string)kv.Value;
                var opt = new UxLevelOptions
                {
                    scenePath = (string)src.Get("scene"), extractIndexPath = (string)src.Get("extract_index"), toolMd5 = md5, threads = 1,
                    templateDirSx = (string)UxJson.Get(want, "template", "dir"), templateDirFr = (string)UxJson.Get(wantFr, "template", "dir"),
                };
                var ctx = env.Level(lv, "work-options"); ctx.log = s => { };
                var clock = Stopwatch.StartNew();
                UxLevel.Convert(ctx.disc, ctx.work, opt, null, ctx.cancel, null);
                UxTest.Log("note " + lv.disc + " with one worker thread: " + F(clock.Elapsed.TotalSeconds) + " s");
                UxTest.CompareFolder(lv.disc + ": written with the reference's environment in UxLevelOptions, one thread", ctx.work.Root, Path.Combine(refDir, lv.disc), rel => Array.IndexOf(Files, rel) >= 0, false, true);

                string rd = Path.Combine(refDir, "rigid-drop", lv.disc, "level.json");
                if (File.Exists(rd))
                {
                    var c2 = env.Level(lv, "work-rigid-drop"); c2.log = s => { };
                    UxLevel.Convert(c2.disc, c2.work, new UxLevelOptions { kinds = new[] { "sx" }, rigidModels = "drop" }, null, c2.cancel, null);
                    bool same = CompareFile(lv.disc + " level.json with rigid models 'drop' against ux_level.py", c2.work.ReadBytes("level.json"), File.ReadAllBytes(rd), out _);
                    same &= UxTest.Check(!c2.work.Exists("level_fr.json"), lv.disc + ": kinds = sx writes level.json only");
                    UxTest.Report(same, lv.disc + ": kinds = sx, rigidModels = drop (the legacy option): level.json byte-identical to build_level(rigid_models='drop'), no level_fr.json");
                }
                else UxTest.Skip(lv.disc + ": no " + rd);
            }
            // a cancelled token stops the stage before anything is written
            {
                var lv = levels[0]; var ctx = env.Level(lv, "work-cancel");
                bool cancelled = false;
                using (var cts = new System.Threading.CancellationTokenSource())
                {
                    cts.Cancel();
                    try { UxLevel.Convert(ctx.disc, ctx.work, new UxLevelOptions(), null, cts.Token, null); }
                    catch (OperationCanceledException) { cancelled = true; }
                }
                UxTest.Report(cancelled && ctx.work.List("", "*", true).Count == 0, lv.disc + ": a cancelled token throws OperationCanceledException and nothing is written");
            }
            string also = UxTest.Arg(args, "also", "UX_DISC_ALSO");
            if (also != null && (File.Exists(also) || Directory.Exists(also)))
                using (var other = UxDisc.Open(also))
                    foreach (var lv in levels)
                    {
                        if (lv.shortName != "norton" && lv.shortName != "fairview") continue;
                        var work = UxWork.ForLevel(UxTest.FreshFolder(env.scratch, "work-also-" + lv.shortName), lv);
                        UxLevel.Convert(other.Level(lv), work, new UxLevelOptions(), null, default, null);
                        bool same = true;
                        foreach (string f in Files)
                            same &= UxTest.SameBytes(lv.disc + " " + f + " from " + other.volume.displayName + " against the one from " + env.disc.volume.displayName, work.ReadBytes(f),
                                File.ReadAllBytes(Path.Combine(env.scratch, "work", lv.disc, f)));
                        UxTest.Report(same, lv.disc + ": level.json and level_fr.json from " + other.volume.displayName + " equal the ones from " + env.disc.volume.displayName + " byte for byte");
                    }
            else UxTest.Log("note no second disc source (--also / UX_DISC_ALSO): the image-against-folder check did not run");
        }
        return UxTest.Result("level");
    }
}
