// tests/untamed_convert/check: the MX vs ATV Untamed converter end to end, from the player's disc and a Reflex library only. Every file the
// converter writes for a stadium (the seven files of the track folder and every file of its work store: the stage outputs S1 - S8 and the
// builder's records) is hashed and compared with expected.txt beside this file: SHA-256 of what this converter writes, recorded when its
// output was confirmed equal to the conversion it was ported from. The list holds hashes only, no game bytes; the files depend on the disc,
// the stock library and the converter, never on the machine, the platform, the folders or the time.
//   lzx          UxLzxEncoder on synthetic inputs: 616 blobs (sizes around the frame and chunk limits, one repeated byte ... noise) decode to
//                their input through LzxDecoder and the client's resource reader
//   half         float16 <-> float32: every half round trips; ties to even, overflow, underflow
//   build        every level (default options, packing lzx) into <scratch>/out/<short>, work store <scratch>/work/<Level>; each file against
//                expected.txt ('<sha256> *<short>/track/<file>', '<sha256> *<short>/work/<path>'); the build's progress rises to 1
//   variants     (not in 'all') the other option values the converter keeps (the older ground, shore, water and reflection looks; packing
//                stored / raw): the four .dx9 files against expected.txt ('<short>@<variant>/<file>')
//   determinism  (not in 'all') each named level three times (twice with all threads, once with one thread): the same files
//   cancel       (not in 'all') a build cancelled after 0, 1.5, 5 and 9 s ends with an OperationCanceledException within 3 s
// Arguments: [lzx|half|build|variants|determinism|cancel|all] --levels a,b --core-blobs --write-expected <file> (+ UxTest.OpenEnv's: the disc
// with --iso / UX_DISC_ISO or --folder / UX_DISC_FOLDER, the library with --reflex / UX_REFLEX or --client / UX_CLIENT_LIB, --scratch).
// --core-blobs also writes <scratch>/coreblobs/<short>.bin for core_lzx_test.sh (the C core's resource reader on this build's bytes).
// --write-expected writes the hashes of this run (every level, and the variants when they ran) to <file>; nothing is written outside
// --scratch otherwise.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static string F(double s, string f = "0.0") => s.ToString(f, CultureInfo.InvariantCulture);
    static string Mb(long b) => F(b / 1048576.0);
    static string ExpectedPath;
    static readonly Dictionary<string, string> s_expected = new Dictionary<string, string>(StringComparer.Ordinal);
    static readonly SortedDictionary<string, string> s_written = new SortedDictionary<string, string>(StringComparer.Ordinal);
    static bool CoreBlobs, Writing;

    /// The option values the converter keeps besides the defaults, each with the levels it is checked on.
    static readonly (string name, string[] levels, Action<UxTrackOptions> set)[] Variants =
    {
        // the ground, shore and water reflection of the builds before the stock dirt look
        ("oldlook", null, o => { o.dirtLook = "untamed"; o.ecWaterShore = "untamed"; o.ecWaterReflection = "level"; }),
        // the arenas' water before the arena look, without the spray
        ("oldwater", new[] { "fairview", "lincoln" }, o => { o.dirtLook = "untamed"; o.ecWaterLook = "untamed"; o.ecWaterSplash = "off"; o.ecWaterReflection = "level"; }),
        // the arenas' water reflecting the level cube itself (the stadium lights not dimmed)
        ("levelcube", new[] { "fairview", "lincoln" }, o => { o.ecWaterReflection = "level"; }),
        ("nolights", new[] { "fairview" }, o => { o.ecWaterReflection = "nolights"; }),
        // the other package forms
        ("stored", new[] { "norton", "lincoln" }, o => { o.packing = "stored"; }),
        ("raw", new[] { "norton", "lincoln" }, o => { o.packing = "raw"; }),
    };

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "all";
        ExpectedPath = UxTest.Arg(args, "expected", "UX_CHECK_EXPECTED", Path.Combine(AppContext.BaseDirectory, "expected.txt"));
        string write = UxTest.Arg(args, "write-expected");
        CoreBlobs = UxTest.Flag(args, "core-blobs"); Writing = write != null;
        if (File.Exists(ExpectedPath)) foreach (var kv in UxTest.ReadMd5List(ExpectedPath)) s_expected[kv.Key] = kv.Value;
        using (var env = UxTest.OpenEnv(args))
        {
            if (what == "all" || what == "lzx") LzxCases(env);
            if (what == "all" || what == "half") Half();
            if (what == "lzx" || what == "half") return UxTest.Result("check " + what);
            if (env.disc == null || env.stock == null)
            {
                UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_ISO / UX_DISC_FOLDER, UX_REFLEX / UX_CLIENT_LIB)");
                return UxTest.Result("check " + what);
            }
            env.stock.templateNames["sx"] = UxTemplateNames.Pinned("sx"); env.stock.templateNames["fr"] = UxTemplateNames.Pinned("fr");
            if (s_expected.Count == 0 && write == null) UxTest.Skip("no expected hashes (" + ExpectedPath + ")");
            string only = UxTest.Arg(args, "levels");
            var levels = UxCommon.Levels.Where(lv => only == null || only.Split(',').Contains(lv.shortName) || only.Split(',').Contains(lv.disc)).ToList();
            UxTest.Log("disc " + (UxTest.Arg(args, "iso", "UX_DISC_ISO") ?? UxTest.Arg(args, "folder", "UX_DISC_FOLDER")) + ", " + env.library.Describe);
            var shared = new UxBuildShared(env.stock);
            if (what == "all" || what == "build")
            {
                var table = new List<string>();
                foreach (var lv in levels)
                {
                    UxTest.Section(lv.disc);
                    try { table.Add(BuildAndCheck(env, shared, lv)); }
                    catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                }
                UxTest.Section("summary");
                UxTest.Log("     level                 result  seconds  peak MB  package MB  files");
                foreach (string row in table) UxTest.Log("     " + row);
            }
            if (what == "variants")
                foreach (var v in Variants)
                    foreach (var lv in levels)
                    {
                        if (v.levels != null && !v.levels.Contains(lv.shortName)) continue;
                        UxTest.Section(lv.disc + " @" + v.name);
                        try { Variant(env, shared, lv, v.name, v.set); }
                        catch (Exception e) { UxTest.Fail(lv.disc + " @" + v.name + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                    }
            if (what == "determinism")
                foreach (var lv in levels) { UxTest.Section("determinism " + lv.disc); Determinism(env, shared, lv); }
            if (what == "cancel")
                foreach (var lv in levels) { UxTest.Section("cancel " + lv.disc); Cancel(env, shared, lv); }
            if (write != null)
            {
                var sb = new StringBuilder();
                sb.Append("# tests/untamed_convert/check/expected.txt: SHA-256 of every file the MX vs ATV Untamed converter writes for each stadium from the\n");
                sb.Append("# supported disc and a stock MX vs ATV Reflex library: <short>/track/<file> = the track folder, <short>/work/<path> = its work store,\n");
                sb.Append("# <short>@<variant>/<file> = the four track files with other option values (Program.cs Variants). Hashes only, no game bytes.\n");
                sb.Append("# Written by: run.sh <scratch> all --write-expected <file>  (and run.sh <scratch> variants --write-expected <file>, merged).\n");
                var merged = new SortedDictionary<string, string>(s_expected, StringComparer.Ordinal);
                foreach (var kv in s_written) merged[kv.Key] = kv.Value;
                foreach (var kv in merged) sb.Append(kv.Value).Append(" *").Append(kv.Key).Append('\n');
                File.WriteAllText(write, sb.ToString(), new UTF8Encoding(false));
                UxTest.Log("wrote " + merged.Count + " hashes (" + s_written.Count + " from this run) to " + write);
            }
            return UxTest.Result("check " + what);
        }
    }

    // ============================================================================================== a build
    static UxBuildResult Run(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string outDir, string workName, UxTrackOptions o, List<float> progress = null)
    {
        var ctx = env.Level(lv, workName, true); ctx.log = s => { };
        if (Directory.Exists(outDir)) Directory.Delete(outDir, true);
        return UxBuildTrack.Build(ctx, env.disc, o, outDir, (f, text) => progress?.Add(f), default, shared);
    }

    /// Every file of a folder, '/'-separated paths relative to it -> SHA-256 (build_info.json without its step times).
    static SortedDictionary<string, string> Hashes(string dir)
    {
        var d = new SortedDictionary<string, string>(StringComparer.Ordinal);
        if (!Directory.Exists(dir)) return d;
        foreach (string f in Directory.GetFiles(dir, "*", SearchOption.AllDirectories))
        {
            string rel = f.Substring(dir.Length + 1).Replace('\\', '/');
            // build_info.json records how long each step took: its "seconds" fields are hashed as 0
            d[rel] = Path.GetFileName(f) == "build_info.json"
                ? UxBin.Sha256(Encoding.UTF8.GetBytes(System.Text.RegularExpressions.Regex.Replace(File.ReadAllText(f), "\"seconds\": *-?[0-9.eE+-]+", "\"seconds\": 0")))
                : UxBin.Sha256File(f);
        }
        return d;
    }

    /// The files of one build against the expected list: keys "<prefix>/<path>". Reports equal / different / missing / unexpected.
    static bool Check(string what, string prefix, SortedDictionary<string, string> got)
    {
        int same = 0; var diff = new List<string>(); var extra = new List<string>(); var missing = new List<string>();
        foreach (var kv in got)
        {
            string key = prefix + "/" + kv.Key;
            s_written[key] = kv.Value;
            if (!s_expected.TryGetValue(key, out string want)) extra.Add(kv.Key);
            else if (want == kv.Value) same++;
            else diff.Add(kv.Key);
        }
        foreach (var k in s_expected.Keys)
            if (k.StartsWith(prefix + "/", StringComparison.Ordinal) && !got.ContainsKey(k.Substring(prefix.Length + 1))) missing.Add(k.Substring(prefix.Length + 1));
        if (s_expected.Count == 0) return true;
        if (Writing && diff.Count == 0 && missing.Count == 0 && extra.Count > 0)
        {   // --write-expected: files new to the list are recorded, not failed
            UxTest.Log("     " + what + ": " + same + " of " + got.Count + " files have the expected SHA-256, " + extra.Count + " new (recorded)");
            return true;
        }
        bool ok = diff.Count == 0 && extra.Count == 0 && missing.Count == 0;
        UxTest.Report(ok, what + ": " + same + " of " + got.Count + " files have the expected SHA-256" +
            (ok ? "" : " (" + diff.Count + " differ, " + missing.Count + " missing, " + extra.Count + " not expected)"));
        foreach (string k in diff.Take(12)) UxTest.Log("       differs   " + k);
        foreach (string k in missing.Take(12)) UxTest.Log("       missing   " + k);
        foreach (string k in extra.Take(12)) UxTest.Log("       unexpected " + k);
        return ok;
    }

    static string BuildAndCheck(UxTest.Env env, UxBuildShared shared, UxLevelRow lv)
    {
        var o = UxTrackOptions.ForLevel(lv);
        string outDir = Path.Combine(env.scratch, "out", lv.shortName);
        var progress = new List<float>();
        var sw = Stopwatch.StartNew();
        var r = Run(env, shared, lv, outDir, "work", o, progress);
        double sec = sw.Elapsed.TotalSeconds;
        long peak = Process.GetCurrentProcess().PeakWorkingSet64;
        bool rising = progress.Count >= 10 && progress[progress.Count - 1] == 1f;
        for (int i = 1; i < progress.Count; i++) rising &= progress[i] >= progress[i - 1];
        UxTest.Report(rising, lv.disc + ": progress was reported " + progress.Count + " times, rising to 1");
        var steps = string.Join(", ", r.steps.Select(kv => kv.Key + " " + F(kv.Value)));
        UxTest.Log("     built " + outDir + " in " + F(sec) + " s (" + steps + "); package " + Mb(r.packageBytes) + " MB, " + r.assets + " resources");
        foreach (string w in r.warnings) UxTest.Log("     warning: " + w);
        int f0 = UxTest.Failures;
        var track = Hashes(outDir);
        bool listed = track.Count == 7;
        foreach (var f in r.files) listed &= track.TryGetValue(f.name, out string h) && h == f.sha256;
        UxTest.Report(listed, lv.disc + ": the seven track files are on disk as the build listed them");
        var work = Hashes(Path.Combine(env.scratch, "work", lv.disc));
        Check(lv.disc + " track folder", lv.shortName + "/track", track);
        Check(lv.disc + " work store (the stage outputs and the builder's records)", lv.shortName + "/work", work);
        if (CoreBlobs) WriteCoreBlobs(env, lv, outDir, r.slot);
        return lv.disc.PadRight(22) + (UxTest.Failures == f0 ? "PASS" : "FAIL").PadRight(8) + F(sec).PadLeft(7) + Mb(peak).PadLeft(9) + Mb(r.packageBytes).PadLeft(12) +
            (track.Count + work.Count).ToString().PadLeft(7);
    }

    static void Variant(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string name, Action<UxTrackOptions> set)
    {
        var o = UxTrackOptions.ForLevel(lv); set(o);
        string outDir = Path.Combine(env.scratch, "variants", lv.shortName + "@" + name);
        var sw = Stopwatch.StartNew();
        var r = Run(env, shared, lv, outDir, "variantwork", o);
        UxTest.Log("     built in " + F(sw.Elapsed.TotalSeconds) + " s; package " + Mb(r.packageBytes) + " MB");
        var dx9 = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (var kv in Hashes(outDir)) if (kv.Key.Contains(".dx9.")) dx9[kv.Key] = kv.Value;
        Check(lv.disc + " @" + name + ": the four track files", lv.shortName + "@" + name, dx9);
        if (o.packing != "lzx")
        {   // another package form holds the same resources: every one decodes to the lzx build's bytes (when that build is in <scratch>/out)
            string lzxDir = Path.Combine(env.scratch, "out", lv.shortName);
            if (!File.Exists(Path.Combine(lzxDir, r.slot + ".dx9.package"))) { UxTest.Skip(lv.disc + " @" + name + ": no lzx build in " + lzxDir + " to compare the resources with (run build first)"); return; }
            var a = Set(lzxDir, r.slot); var b = Set(outDir, r.slot);
            int same = 0, n = Math.Min(a.assets.Count, b.assets.Count);
            for (int i = 0; i < n; i++) if (a.assets[i].name == b.assets[i].name && a.assets[i].type == b.assets[i].type && UxBin.Same(a.ReadLogical(a.assets[i]), b.ReadLogical(b.assets[i]))) same++;
            UxTest.Report(a.assets.Count == b.assets.Count && same == n, lv.disc + " @" + name + ": " + same + " of " + a.assets.Count + " resources decode to the lzx build's bytes, same order");
        }
    }

    static void Determinism(UxTest.Env env, UxBuildShared shared, UxLevelRow lv)
    {
        var lists = new List<SortedDictionary<string, string>>(); var names = new[] { "run A (all threads)", "run B (all threads, again, no shared state)", "run C (one thread)" };
        for (int i = 0; i < 3; i++)
        {
            var o = UxTrackOptions.ForLevel(lv); o.threads = i == 2 ? 1 : 0;
            string outDir = Path.Combine(env.scratch, "det" + i, lv.shortName);
            var sw = Stopwatch.StartNew();
            if (i == 1)
            {   // the short entry point: no shared state, the disc taken from the level's own volume
                var ctx = env.Level(lv, "detwork" + i, true); ctx.log = s => { };
                if (Directory.Exists(outDir)) Directory.Delete(outDir, true);
                UxBuildTrack.Build(ctx, o, outDir);
            }
            else Run(env, shared, lv, outDir, "detwork" + i, o);
            var all = Hashes(outDir);
            foreach (var kv in Hashes(Path.Combine(env.scratch, "detwork" + i, lv.disc))) all["work/" + kv.Key] = kv.Value;
            lists.Add(all);
            UxTest.Log("     " + names[i] + ": " + F(sw.Elapsed.TotalSeconds) + " s, " + all.Count + " files");
        }
        bool Same(SortedDictionary<string, string> x, SortedDictionary<string, string> y) => x.Count == y.Count && x.All(kv => y.TryGetValue(kv.Key, out string v) && v == kv.Value);
        UxTest.Report(Same(lists[0], lists[1]), lv.disc + ": two runs write the same files (track folder and work store)");
        UxTest.Report(Same(lists[0], lists[2]), lv.disc + ": a one-thread run writes the same files");
        foreach (var kv in lists[0]) foreach (int j in new[] { 1, 2 })
            if (!lists[j].TryGetValue(kv.Key, out string v) || v != kv.Value) UxTest.Log("       " + names[j] + " differs: " + kv.Key);
    }

    static void Cancel(UxTest.Env env, UxBuildShared shared, UxLevelRow lv)
    {
        foreach (int ms in new[] { 0, 1500, 5000, 9000 })
        {   // a build cancelled after ms milliseconds must end soon with an OperationCanceledException (also inside an AggregateException)
            var ctx = env.Level(lv, "cancelwork", true); ctx.log = s => { };
            string outDir = Path.Combine(env.scratch, "cancel", lv.shortName);
            if (Directory.Exists(outDir)) Directory.Delete(outDir, true);
            using (var cts = new System.Threading.CancellationTokenSource())
            {
                var sw = Stopwatch.StartNew(); string how = "finished"; double at = 0;
                if (ms == 0) cts.Cancel(); else cts.CancelAfter(ms);
                cts.Token.Register(() => at = sw.Elapsed.TotalSeconds);
                try { UxBuildTrack.Build(ctx, env.disc, UxTrackOptions.ForLevel(lv), outDir, null, cts.Token, shared); }
                catch (OperationCanceledException) { how = "OperationCanceledException"; }
                catch (AggregateException e) { how = "AggregateException"; foreach (var x in e.Flatten().InnerExceptions) how += " [" + x.GetType().Name + "]"; }
                catch (Exception e) { how = e.GetType().Name + ": " + e.Message; }
                double late = sw.Elapsed.TotalSeconds - at;
                UxTest.Report(how == "OperationCanceledException" && late < 3.0, lv.disc + ": cancelled after " + F(ms / 1000.0) + " s -> " + how + " " + F(late) + " s later");
            }
        }
    }

    /// --core-blobs: per compressed resource of the package, its stored bytes and what the client's reader decodes them to, for the C core's
    /// reader (core_lzx_test.sh). Scratch only: the decoded bytes are game content.
    static UxTrackSet Set(string dir, string slot) =>
        new UxTrackSet(slot, File.ReadAllBytes(Path.Combine(dir, slot + ".dx9.database")), File.ReadAllBytes(Path.Combine(dir, slot + ".dx9.package")));
    static void WriteCoreBlobs(UxTest.Env env, UxLevelRow lv, string dir, string slot)
    {
        Directory.CreateDirectory(Path.Combine(env.scratch, "coreblobs"));
        var p = Set(dir, slot); int n = 0;
        using (var w = new BinaryWriter(File.Create(Path.Combine(env.scratch, "coreblobs", lv.shortName + ".bin"))))
            foreach (var m in p.assets)
            {
                if (!m.compressed) continue;
                byte[] blob = p.Blob(m), b = p.ReadLogical(m);
                w.Write(blob.Length); w.Write(blob); w.Write(b.Length); w.Write(b); n++;
            }
        UxTest.Log("     core blobs: " + n + " compressed resources -> " + Path.Combine(env.scratch, "coreblobs", lv.shortName + ".bin"));
    }

    // ============================================================================================== units without the disc
    static void LzxCases(UxTest.Env env)
    {
        UxTest.Section("UxLzxEncoder: synthetic inputs");
        int[] sizes = { 1, 2, 3, 4, 17, 255, 256, 257, 258, 1000, 4095, 32766, 32767, 32768, 32769, 32770, 40000, 65534, 65535, 65536, 65537, 65538, 98304, 131071, 131072, 131073, 200001, 300000 };
        string[] kinds = { "zeros", "one byte", "period 2", "period 3", "period 257", "ramp", "words", "sparse", "noise", "noise then copy", "runs" };
        Directory.CreateDirectory(Path.Combine(env.scratch, "coreblobs"));
        int cases = 0, bad = 0; long raw = 0, packed = 0;
        using (var w = new BinaryWriter(File.Create(Path.Combine(env.scratch, "coreblobs", "_cases.bin"))))
            foreach (int n in sizes)
                for (int k = 0; k < kinds.Length; k++)
                {
                    var d = new byte[n]; uint x = (uint)(n * 2654435761u + k * 40503u + 1);
                    uint Next() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
                    for (int i = 0; i < n; i++)
                        switch (k)
                        {
                            case 0: d[i] = 0; break;
                            case 1: d[i] = 0xE8; break;
                            case 2: d[i] = (byte)(i % 2 == 0 ? 0x41 : 0x42); break;
                            case 3: d[i] = (byte)(i % 3); break;
                            case 4: d[i] = (byte)(i % 257 * 7); break;
                            case 5: d[i] = (byte)(i >> 3); break;
                            case 6: d[i] = (byte)("the quick brown fox jumps over the lazy dog "[(int)((i + (i / 44) * (Next() % 3)) % 44)]); break;
                            case 7: d[i] = (byte)(Next() % 97 == 0 ? Next() : 0); break;
                            case 8: d[i] = (byte)Next(); break;
                            case 9: d[i] = i < n / 2 || n < 8 ? (byte)Next() : d[i - n / 2]; break;
                            default: d[i] = (byte)(i == 0 || Next() % 23 == 0 ? Next() : d[i - 1]); break;
                        }
                    foreach (bool lzx in new[] { true, false })
                    {
                        cases++;
                        byte[] blob = lzx ? UxLzxEncoder.EncodeBlob(d) : UxLzxEncoder.StoredBlob(d);
                        bool ok;
                        try { ok = UxBin.Same(UxPkg.Decode(blob), d) && UxBin.Same(UxPkg.RfxLzx(blob), d); } catch (Exception e) { ok = false; UxTest.Log("       " + e.Message); }
                        if (!ok) { bad++; if (bad <= 8) UxTest.Log("       " + (lzx ? "lzx" : "stored") + " " + n + " bytes of '" + kinds[k] + "' does not decode to the input"); }
                        if (lzx) { raw += n; packed += blob.Length; }
                        w.Write(blob.Length); w.Write(blob); w.Write(d.Length); w.Write(d);
                    }
                }
        UxTest.Report(bad == 0, "UxLzxEncoder: " + cases + " blobs (" + sizes.Length + " sizes x " + kinds.Length + " contents, LZX and stored) decode to their input through LzxDecoder and the client's reader; LZX " +
            raw + " -> " + packed + " bytes");
    }

    static void Half()
    {
        UxTest.Section("float16 <-> float32");
        int bad = 0;
        for (int h = 0; h < 65536; h++) { float f = UxWater.HalfToFloat((ushort)h); if (!float.IsNaN(f) && UxWater.FloatToHalf(f) != (ushort)h) bad++; }
        UxTest.Report(bad == 0 && UxWater.FloatToHalf(1.00048828125f) == 0x3C00 && UxWater.FloatToHalf(1.00146484375f) == 0x3C02 && UxWater.FloatToHalf(65520f) == 0x7C00
            && UxWater.FloatToHalf(2.98023224e-08f) == 0 && UxWater.FloatToHalf(2.98023259e-08f) == 1 && UxWater.FloatToHalf(-2f) == 0xC000,
            "half <-> float: 65536 halves round trip, ties to even, overflow and underflow (" + bad + " differ)");
    }
}
