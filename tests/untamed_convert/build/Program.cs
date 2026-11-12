// tests/untamed_convert/build: the track builder end to end (UxBuildTrack + UxPackage + UxLzxEncoder) against the reference release of
// each track that the port mirrors (Mirrors: release/<short>-v<k>, built by build/<short>-v<k>). A NEWER release of the reference is
// reported and makes the result INCOMPLETE: the port is then behind the reference for that track.
//   lzx          UxLzxEncoder on synthetic inputs (sizes around the frame and chunk limits; one repeated byte ... noise)
//   options      UxTrackOptions.Table = the build command of every track in the reference's commands files
//   build        every level from the disc and the Reflex library through S1-S8 and the builder into <scratch>/out/<short>, then
//                   .level and .scene      byte for byte
//                   .database              as a document, apart from the fields that describe where a resource is stored (Heap offset /
//                                          size, Package offset / heapOffset; with --packing raw also the Compress elements)
//                   .package               every resource of the reference decoded (LZX) = this build's resource decoded, in the same
//                                          order; this build's stored bytes also through the client's reader (UxPkg.RfxLzx)
//                   mod.ini                byte for byte apart from the author and the description's wording (UxPackage.AUTHOR /
//                                          CONVERTED; the reference's date is passed as the option)
//                   build_info.json        the records (assembly, rollout, assets ...) as documents, apart from the listed environment fields
//                and prints the seconds per step, the peak memory of the process and the sizes
//   determinism  a level built twice, and with one thread, gives the same files (sha256.txt of each run)
//   variants     (not in 'all') the older option values against the older releases that used them: every named level with
//                --dirt-look untamed --ec-water-shore untamed against the release before the dirt-look rounds (OldLook: norton-v6 ...
//                lincoln-v5), and Fairview / Lincoln also with --ec-water-look untamed --ec-water-splash off against fairview-v4 / lincoln-v4
//                and with --ec-water-reflection level against fairview-v6 / lincoln-v6 (the builds before the water-glare round)
//   cancel       (not in 'all') a build cancelled after 0, 1.5, 5 and 9 s ends with an OperationCanceledException within 3 s
//   gates        (run_gates.sh) the tree the reference's Python gates read: this port's release folders, stage outputs and records
// Arguments: [options|build|determinism|gates|all] --levels a,b --packing lzx|stored|raw --threads N --reuse --no-compare --core-blobs
// --same-as <out folder of another run> --tag X --gate-root <dir> (+ UxTest.OpenEnv's).
// Nothing is written outside --scratch.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static readonly UTF8Encoding Utf8 = new UTF8Encoding(false);
    static bool CoreBlobs; static string SameAs;
    static string F(double s, string f = "0.0") => s.ToString(f, CultureInfo.InvariantCulture);
    static string Mb(long b) => F(b / 1048576.0);

    /// The build_info.json paths that are environment data or belong to the stored form (not compared; listed in the report).
    static readonly string[] InfoSkip =
    {
        "argv", "command", "tool_md5s", "stages", "port",          // the command line, the Python tool files, ran / reused, this port's tag
        "options",                                                  // compared apart (the switches this port carries)
        "assembly.seconds", "assembly.base_prune.seconds", "assembly.base_prune.prefetched",
        "assembly.output",                                          // md5 / bytes of the four files: the stored form of the resources differs
        "assembly.base_prune.kept_stored", "assembly.base_prune.removed_stored", "assembly.base_prune.kept_by_package_type", "assembly.base_prune.removed_by_package_type",
        "assembly.base_prune.rule",                                 // the module doc text of ux_base.py
        "source_md5s", "counts.stage",                              // manifests carry tool md5s and paths; stage counts are the stages' own records
        "rollout.split_gate.tool_md5", "rollout.split_gate.materials_manifest", "rollout.split_gate.level_json_md5",
        "entities.freeride.hf_source", "entities.options.manifest_paths", "race.tdf.source",   // absolute paths of the reference machine (the stages' records)
    };

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "all";
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null || env.stock == null)
            {
                UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)");
                return UxTest.Result("build");
            }
            // the builder needs nothing of the reference: the constant template name tables
            env.stock.templateNames["sx"] = UxTemplateNames.Pinned("sx"); env.stock.templateNames["fr"] = UxTemplateNames.Pinned("fr");
            string only = UxTest.Arg(args, "levels");
            var levels = new List<UxLevelRow>();
            foreach (var lv in UxCommon.Levels) if (only == null || Array.IndexOf(only.Split(','), lv.shortName) >= 0 || Array.IndexOf(only.Split(','), lv.disc) >= 0) levels.Add(lv);
            string packing = UxTest.Arg(args, "packing", null, "lzx");
            int threads = int.Parse(UxTest.Arg(args, "threads", null, "0"), CultureInfo.InvariantCulture);
            bool reuse = UxTest.Flag(args, "reuse"), compare = !UxTest.Flag(args, "no-compare");
            string tag = UxTest.Arg(args, "tag", null, "");
            CoreBlobs = UxTest.Flag(args, "core-blobs"); SameAs = UxTest.Arg(args, "same-as");
            UxTest.Log("disc " + (UxTest.Arg(args, "folder", "UX_DISC_FOLDER") ?? UxTest.Arg(args, "iso", "UX_DISC_ISO")) + ", " + env.library.Describe);
            ExpectedPath = UxTest.Arg(args, "expected", "UX_BUILD_EXPECTED"); WriteExpected = UxTest.Arg(args, "write-expected");
            if (ExpectedPath != null && File.Exists(ExpectedPath)) foreach (var kv in UxTest.ReadMd5List(ExpectedPath)) s_expected[kv.Key] = kv.Value;
            if (what == "all" || what == "lzx") LzxCases(env);
            if (what == "all" || what == "options") Options(env);
            var shared = new UxBuildShared(env.stock);
            if (what == "all" || what == "build")
            {
                var table = new List<string>();
                foreach (var lv in levels)
                {
                    UxTest.Section(lv.disc + " (" + packing + ")");
                    try { table.Add(BuildAndCompare(env, shared, lv, packing, threads, reuse, compare, tag)); }
                    catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                }
                if (WriteExpected != null && s_written.Count > 0)
                {
                    File.WriteAllText(WriteExpected, "# tests/untamed_convert/build/expected_sha256.txt: sha256 of the four .dx9 files UxBuildTrack writes for each stadium (default options,\n" +
                        "# packing lzx) from the supported disc and a stock MX vs ATV Reflex library; run.sh compares every build with it. Written by: run.sh <scratch> build --write-expected <file>\n" +
                        string.Join("\n", s_written) + "\n");
                    UxTest.Log("     wrote " + s_written.Count + " hashes to " + WriteExpected);
                }
                UxTest.Section("summary");
                UxTest.Log("     level                 parity  seconds  peak MB  package MB  logical MB  assets");
                foreach (string row in table) UxTest.Log("     " + row);
            }
            if (what == "variants")
            {   // option values no current build uses, against the older reference release that used them
                var table = new List<string>();
                foreach (var lv in levels)
                {
                    // the ground and shore colour of the releases before the dirt-look rounds (DESIGN 23.10 / 23.11)
                    string refName = OldLookOf(lv);
                    if (!File.Exists(Path.Combine(env.py, "release", refName, "sha256.txt"))) UxTest.Skip("no reference release " + refName);
                    else
                    {
                        UxTest.Section(lv.disc + " --dirt-look untamed --ec-water-shore untamed = " + refName);
                        try { table.Add(BuildAndCompare(env, shared, lv, packing, threads, reuse, true, "-oldlook", refName, o => { o.dirtLook = "untamed"; o.ecWaterShore = "untamed"; o.ecWaterReflection = "level"; }) + "  = " + refName); }
                        catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                    }
                    // the two arenas with the old water look and no spray = fairview-v4 / lincoln-v4 (the builds before the water-look round)
                    if (lv.shortName != "fairview" && lv.shortName != "lincoln") continue;
                    refName = lv.shortName + "-v4";
                    if (!File.Exists(Path.Combine(env.py, "release", refName, "sha256.txt"))) { UxTest.Skip("no reference release " + refName); continue; }
                    UxTest.Section(lv.disc + " --dirt-look untamed --ec-water-look untamed --ec-water-splash off = " + refName);
                    try
                    {
                        table.Add(BuildAndCompare(env, shared, lv, packing, threads, reuse, true, "-variants", refName, o => { o.dirtLook = "untamed"; o.ecWaterLook = "untamed"; o.ecWaterSplash = "off"; o.ecWaterReflection = "level"; }) +
                            "  = " + refName);
                    }
                    catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                    // the two arenas reflecting the level cube itself = fairview-v6 / lincoln-v6 (the builds before the water-glare round)
                    refName = lv.shortName + "-v6";
                    if (!File.Exists(Path.Combine(env.py, "release", refName, "sha256.txt"))) { UxTest.Skip("no reference release " + refName); continue; }
                    UxTest.Section(lv.disc + " --ec-water-reflection level = " + refName);
                    try { table.Add(BuildAndCompare(env, shared, lv, packing, threads, reuse, true, "-levelcube", refName, o => { o.ecWaterReflection = "level"; }) + "  = " + refName); }
                    catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                }
                UxTest.Section("summary (older option values)");
                UxTest.Log("     level                 parity  seconds  peak MB  package MB  logical MB  assets");
                foreach (string row in table) UxTest.Log("     " + row);
            }
            if (what == "cancel")
                foreach (var lv in levels)
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
            if (what == "gates")
            {
                string root = UxTest.Arg(args, "gate-root") ?? throw new UxException("gates needs --gate-root <the scratch copy of the reference tree>");
                string suffix = UxTest.Arg(args, "suffix", null, "-cs");
                foreach (var lv in levels)
                {
                    UxTest.Section("gate tree " + lv.disc);
                    try { GateTree(env, shared, lv, packing, root, suffix); }
                    catch (Exception e) { UxTest.Fail(lv.disc + ": " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
                }
            }
            if (what == "determinism")
                foreach (var lv in levels) { UxTest.Section("determinism " + lv.disc); Determinism(env, shared, lv, packing); }
            return UxTest.Result("build");
        }
    }

    // ============================================================================================== UxLzxEncoder on synthetic inputs
    /// Sizes around the frame (32768) and chunk (65536) limits, with contents from one repeated byte to noise: EncodeBlob and StoredBlob
    /// must decode to the input through the client's decoder (UxLzx.Decode = LzxDecoder) and the client's resource reader (RfxLzx);
    /// the blobs also go to <scratch>/coreblobs/_cases.bin for the C core (core_lzx_test.sh).
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

    // ============================================================================================== the reference
    /// The reference releases this port mirrors (UxBuildTrack.REFERENCE_TOOL_MD5: the builder of the dirt-look rounds, DESIGN 23.10 / 23.11).
    static readonly string[] Mirrors = { "norton-v7", "capitalcity-v5", "columbus-v6", "denton-v5", "harrispark-v6", "foothills-v6", "weststone-v5", "jefferson-v5", "wilford-v5",
        "bridgeport-v5", "fairview-v7", "lincoln-v7" };
    /// The releases before those rounds: what --dirt-look untamed --ec-water-shore untamed must still write ('variants').
    static readonly string[] OldLook = { "norton-v6", "capitalcity-v4", "columbus-v5", "denton-v4", "harrispark-v5", "foothills-v5", "weststone-v4", "jefferson-v4", "wilford-v4",
        "bridgeport-v4", "fairview-v5", "lincoln-v5" };
    static string Mirror(UxLevelRow lv) { foreach (string m in Mirrors) if (m.StartsWith(lv.shortName + "-v", StringComparison.Ordinal)) return m; throw new UxException("no mirrored release of " + lv.shortName); }
    static string OldLookOf(UxLevelRow lv) { foreach (string m in OldLook) if (m.StartsWith(lv.shortName + "-v", StringComparison.Ordinal)) return m; throw new UxException("no older release of " + lv.shortName); }

    /// The newest release/<short>-v<k> that has build/<same>/build_info.json, or null.
    static string NewestName(UxTest.Env env, UxLevelRow lv)
    {
        int best = -1; string name = null;
        var rx = new Regex("^" + Regex.Escape(lv.shortName) + @"-v(\d+)$");
        foreach (string d in Directory.GetDirectories(Path.Combine(env.py, "release")))
        {
            var m = rx.Match(Path.GetFileName(d));
            if (!m.Success) continue;
            int k = int.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture);
            if (k > best && File.Exists(Path.Combine(env.py, "build", Path.GetFileName(d), "build_info.json")) && File.Exists(Path.Combine(d, "sha256.txt"))) { best = k; name = Path.GetFileName(d); }
        }
        return name;
    }
    /// (release folder, build folder, name) of the release the port mirrors. A NEWER release of the reference is reported and makes
    /// the result INCOMPLETE: the port is then behind the reference for that track (the comparison below is with the mirrored one).
    static readonly HashSet<string> s_newerSeen = new HashSet<string>(StringComparer.Ordinal);
    static (string release, string build, string name) Newest(UxTest.Env env, UxLevelRow lv)
    {
        string name = Mirror(lv), newest = NewestName(env, lv);
        if (!File.Exists(Path.Combine(env.py, "release", name, "sha256.txt")) || !File.Exists(Path.Combine(env.py, "build", name, "build_info.json")))
            throw new UxException("the reference has no release / build " + name + " (the release this port mirrors)");
        if (newest != null && newest != name && s_newerSeen.Add(newest))
        {
            string why = "";
            try
            {
                var bi = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(env.py, "build", newest, "build_info.json"))));
                string tool = UxJson.Get(bi, "tool_md5s", "ux_build_track.py") as string ?? "?";
                why = " (ux_build_track.py " + tool.Substring(0, Math.Min(8, tool.Length)) + ", dirt_look " + UxJson.Dumps(UxJson.Get(bi, "options", "dirt_look"), -1) + ")";
            }
            catch (Exception) { }
            UxTest.Skip("the reference has a NEWER release " + newest + why + ": not ported; " + lv.disc + " is compared with " + name + ", the release this port mirrors (ux_build_track.py " +
                UxBuildTrack.REFERENCE_TOOL_MD5.Substring(0, 8) + ")");
        }
        return (Path.Combine(env.py, "release", name), Path.Combine(env.py, "build", name), name);
    }

    static void Options(UxTest.Env env)
    {
        UxTest.Section("options: UxTrackOptions.Table = the reference's build commands");
        // the newest command of every level in the reference's commands files (the first file that names the level wins)
        var cmds = new Dictionary<string, KeyValuePair<string, List<string>>>(StringComparer.Ordinal);
        foreach (string rel in new[] { "work/combined/commands.json", "work/combined/waterlook/commands.json" })
        {
            string p = Path.Combine(env.py, rel);
            if (!File.Exists(p)) continue;
            foreach (var kv in UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(p))).items)
            {
                if (!(kv.Value is List<object> l) || l.Count == 0) continue;
                var parts = new List<string>(((string)l[0]).Split(' '));
                int at = parts.FindIndex(s => s.EndsWith("ux_build_track.py", StringComparison.Ordinal)), li = parts.IndexOf("--level");
                if (at < 0 || li < 0 || li + 1 >= parts.Count || parts.Contains("--ec-water-look") || parts.Contains("--dirt-look")) continue;      // (a legacy-proof command)
                if (!cmds.ContainsKey(parts[li + 1])) cmds[parts[li + 1]] = new KeyValuePair<string, List<string>>(rel + " " + kv.Key, parts.GetRange(at + 1, parts.Count - at - 1));
            }
        }
        foreach (var lv in UxCommon.Levels)
        {
            string name; try { name = Newest(env, lv).name; } catch (UxException e) { UxTest.Fail(e.Message); continue; }
            string have = UxJson.Dumps(UxTrackOptions.ForLevel(lv).Record(lv), 1);
            // the command the mirrored build recorded
            var bi = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(env.py, "build", name, "build_info.json"))));
            var argv = new List<string>(); foreach (object a in UxJson.Arr(bi.Get("argv"))) argv.Add((string)a);
            argv.RemoveAt(0);
            UxTest.Report(UxJson.Dumps(UxTrackOptions.Parse(argv).Record(lv), 1) == have, lv.disc + ": UxTrackOptions.ForLevel = the command of " + name + " (" + string.Join(" ", argv.FindAll(a => !a.Contains("/"))) + ")");
            // the level's command in the commands files of today
            if (cmds.TryGetValue(lv.disc, out var cmd))
                UxTest.Report(UxJson.Dumps(UxTrackOptions.Parse(cmd.Value).Record(lv), 1) == have, lv.disc + ": UxTrackOptions.ForLevel = the arguments of " + cmd.Key);
            else UxTest.Skip(lv.disc + ": no command in the reference's commands files");
            // and the options the mirrored build recorded
            var ro = UxJson.Obj(bi.Get("options")); var mine = UxTrackOptions.ForLevel(lv).Record(lv); int bad = 0, n = 0; var absent = new List<string>();
            foreach (var kv in mine.items)
            {
                if (kv.Key == "packing") continue;
                if (!ro.Has(kv.Key)) { absent.Add(kv.Key); continue; }      // a switch younger than that build
                n++;
                if (UxJson.Dumps(ro.Get(kv.Key), -1) != UxJson.Dumps(kv.Value, -1)) { bad++; UxTest.Log("       option " + kv.Key + ": " + UxJson.Dumps(kv.Value, -1) + " vs build_info " + UxJson.Dumps(ro.Get(kv.Key), -1)); }
            }
            UxTest.Report(bad == 0, lv.disc + ": the " + n + " switches of the port equal build_info.json options of " + name + (absent.Count > 0 ? " (not recorded by that build: " + string.Join(", ", absent) + ")" : ""));
        }
    }

    // ============================================================================================== build + compare
    static UxBuildResult Run(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string outDir, string workName, UxTrackOptions o, bool fresh)
    {
        var ctx = env.Level(lv, workName, fresh); ctx.log = s => { };
        if (Directory.Exists(outDir)) Directory.Delete(outDir, true);
        s_heap = 0; s_progress.Clear();
        return UxBuildTrack.Build(ctx, env.disc, o, outDir, (f, text) =>
        {   // the progress callback: fractions must not go back; the managed heap at the step boundaries
            s_progress.Add(f); s_heap = Math.Max(s_heap, GC.GetTotalMemory(false));
        }, default, shared);
    }
    static long s_heap; static readonly List<float> s_progress = new List<float>();

    static string BuildAndCompare(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string packing, int threads, bool reuse, bool compare, string tag,
        string refName = null, Action<UxTrackOptions> change = null)
    {
        var (rel, bld, name) = refName == null ? Newest(env, lv) : (Path.Combine(env.py, "release", refName), Path.Combine(env.py, "build", refName), refName);
        var o = UxTrackOptions.ForLevel(lv);
        change?.Invoke(o);
        o.packing = packing; o.threads = threads; o.reuseStages = reuse;
        // the path TEXT of the records as the reference machine writes it (UX_PY_GAME: its Reflex install, the text of one record)
        o.paths = new UxPaths { root = env.py.Replace('\\', '/').TrimEnd('/'), game = (Environment.GetEnvironmentVariable("UX_PY_GAME") ?? "C:/Program Files (x86)/Steam/steamapps/common/MX vs ATV Reflex").Replace('\\', '/').TrimEnd('/') };
        string refIni = Utf8.GetString(File.ReadAllBytes(Path.Combine(rel, "mod.ini")));
        var dm = Regex.Match(refIni, @"; (\d{4}-\d{2}-\d{2})\. Unofficial");
        o.date = dm.Success ? dm.Groups[1].Value : null;
        string outDir = Path.Combine(env.scratch, "out" + tag, lv.shortName);
        var sw = Stopwatch.StartNew();
        var r = Run(env, shared, lv, outDir, "work" + tag, o, !reuse);
        double sec = sw.Elapsed.TotalSeconds;
        var steps = new List<string>(); foreach (var kv in r.steps) steps.Add(kv.Key + " " + F(kv.Value));
        long peak = Process.GetCurrentProcess().PeakWorkingSet64;
        bool mono = s_progress.Count >= 10 && s_progress[s_progress.Count - 1] == 1f;
        for (int i = 1; i < s_progress.Count; i++) mono &= s_progress[i] >= s_progress[i - 1];
        UxTest.Report(mono, lv.disc + ": progress was reported " + s_progress.Count + " times, rising to 1; managed heap at the step boundaries at most " + Mb(s_heap) + " MB");
        UxTest.Log("     built " + outDir + " in " + F(sec) + " s (" + string.Join(", ", steps) + "); process peak " + Mb(peak) + " MB");
        long total = 0; foreach (var f in r.files) total += f.bytes;
        UxTest.Log("     package " + Mb(r.packageBytes) + " MB (" + r.assets + " resources, " + Mb(r.logicalBytes) + " MB logical; reference " + name + " " +
            Mb(new FileInfo(Path.Combine(rel, r.slot + ".dx9.package")).Length) + " MB), folder " + Mb(total) + " MB");
        foreach (string w in r.warnings) UxTest.Log("     warning: " + w);
        int f0 = UxTest.Failures;
        if (SameAs != null)
        {   // --same-as <folder of another run>: the seven files equal that run's (another disc form / library kind)
            string other = Path.Combine(SameAs, lv.shortName, "sha256.txt");
            UxTest.Report(File.Exists(other) && File.ReadAllText(other) == File.ReadAllText(Path.Combine(outDir, "sha256.txt")),
                lv.disc + ": the files equal those of " + Path.Combine(SameAs, lv.shortName) + " (sha256.txt: the four .dx9 files, mod.ini, preview.png)");
        }
        if (packing == "lzx" && change == null) Expected(env, lv, r);
        if (compare) Compare(env, lv, r, rel, bld, name, packing, Path.Combine(env.scratch, "work" + tag, lv.disc));
        return lv.disc.PadRight(22) + (UxTest.Failures == f0 ? (compare ? "PASS" : "-   ") : "FAIL").PadRight(8) + F(sec).PadLeft(7) + Mb(peak).PadLeft(9) + Mb(r.packageBytes).PadLeft(12) +
            Mb(r.logicalBytes).PadLeft(12) + r.assets.ToString().PadLeft(8);
    }

    /// expected_sha256.txt (next to this file's sources; --expected <path>): '<sha256> *<short>/<file>' for the four .dx9 files of every
    /// level as this port writes them with the default options (packing lzx). The four files depend on the disc, the stock library
    /// and the converter only, so the hashes must be the same on every machine and platform. --write-expected <path> writes the list.
    static readonly Dictionary<string, string> s_expected = new Dictionary<string, string>(StringComparer.Ordinal);
    static readonly List<string> s_written = new List<string>();
    static string ExpectedPath, WriteExpected;
    static void Expected(UxTest.Env env, UxLevelRow lv, UxBuildResult r)
    {
        int n = 0, bad = 0;
        foreach (var f in r.files)
        {
            if (!f.name.Contains(".dx9.")) continue;
            string key = lv.shortName + "/" + f.name;
            s_written.Add(f.sha256 + " *" + key);
            if (!s_expected.TryGetValue(key, out string want)) continue;
            n++;
            if (want != f.sha256) { bad++; UxTest.Log("       " + key + ": sha256 " + f.sha256 + ", expected " + want); }
        }
        if (n > 0) UxTest.Report(bad == 0 && n == 4, lv.disc + ": the four .dx9 files have the expected sha256 (" + Path.GetFileName(ExpectedPath) + ")");
        else if (ExpectedPath != null && WriteExpected == null) UxTest.Skip(lv.disc + ": no expected sha256 in " + ExpectedPath);
    }

    static string Hex(byte[] b, int at, int n) { var sb = new StringBuilder(2 * n); for (int i = 0; i < n; i++) sb.Append(b[at + i].ToString("x2")); return sb.ToString(); }

    /// A BXML document as text: one line per node (name, attributes with type / flag / value, the value), children indented.
    static void Canon(UxBxml b, int ni, int depth, List<string> o, Func<string, string, bool> mask, Func<string, bool> skip)
    {
        string name = b.Name(ni);
        if (skip != null && skip(name)) return;
        var sb = new StringBuilder(); sb.Append(' ', depth).Append(name);
        foreach (var kv in b.AttrItems(ni))
        {
            var a = b.attrs[kv.Key]; string val;
            if (a[3] == 1) val = b.strings[(int)a[1]];
            else if (a[3] == 2) val = "blob@" + a[1];
            else val = Hex(b.pool, (int)a[1], UxBxml.Size(a[3]));
            if (mask != null && mask(name, kv.Value)) val = "*";
            sb.Append(' ').Append(kv.Value).Append(':').Append(a[3]).Append('/').Append(a[2]).Append('=').Append(val);
        }
        var n = b.nodes[ni];
        if (n[1] >= 0)
        {
            if (n[3] == 0 || n[3] == 1) sb.Append(" = '").Append(b.strings[(int)n[1]]).Append('\'');
            else { int len = b.PoolLen(n[3], n[1], n[2]); sb.Append(" = ").Append(n[3]).Append('x').Append(n[2]).Append(':').Append(UxBin.Md5(b.pool, (int)n[1], len)); }
        }
        o.Add(sb.ToString());
        foreach (int c in b.Children(ni)) Canon(b, c, depth + 1, o, mask, skip);
    }
    static List<string> Canon(byte[] doc, Func<string, string, bool> mask = null, Func<string, bool> skip = null)
    {
        var b = new UxBxml(doc); var o = new List<string>();
        o.Add("version " + b.version + " unknown " + b.unknown);
        Canon(b, 0, 0, o, mask, skip);
        return o;
    }
    static bool SameLines(string what, List<string> got, List<string> want)
    {
        int n = Math.Min(got.Count, want.Count), bad = 0, shown = 0;
        for (int i = 0; i < n; i++)
            if (got[i] != want[i]) { bad++; if (shown++ < 6) UxTest.Log("       line " + i + ":\n         got  " + Cut(got[i]) + "\n         want " + Cut(want[i])); }
        if (got.Count != want.Count) { bad++; UxTest.Log("       " + got.Count + " lines, the reference has " + want.Count); }
        return UxTest.Report(bad == 0, what + (bad == 0 ? "" : ": " + bad + " lines differ"));
    }
    static string Cut(string s) => s.Length > 300 ? s.Substring(0, 300) + "..." : s;

    static void Compare(UxTest.Env env, UxLevelRow lv, UxBuildResult r, string rel, string bld, string name, string packing, string workRoot)
    {
        string slot = r.slot;
        byte[] Mine(string kind) => File.ReadAllBytes(Path.Combine(r.folder, slot + ".dx9." + kind));
        byte[] Ref(string kind) => File.ReadAllBytes(Path.Combine(rel, slot + ".dx9." + kind));
        // ---- level, scene
        UxTest.SameBytes(lv.disc + " .level = " + name, Mine("level"), Ref("level"));
        byte[] ms = Mine("scene"), rs = Ref("scene");
        if (!UxBin.Same(ms, rs))
        {
            var A = UxPcScene.RawChunks(ms); var B = UxPcScene.RawChunks(rs); int bad = 0;
            if (A.Count != B.Count) { UxTest.Fail(lv.disc + " .scene: " + A.Count + " sub-scenes, the reference has " + B.Count); bad++; }
            for (int i = 0; i < Math.Min(A.Count, B.Count); i++)
            {
                if (A[i].Key != B[i].Key || A[i].Value.Count != B[i].Value.Count) { UxTest.Fail(lv.disc + " .scene sub-scene " + i + ": " + A[i].Key + " x" + A[i].Value.Count + " vs " + B[i].Key + " x" + B[i].Value.Count); bad++; }
                for (int k = 0; k < Math.Min(A[i].Value.Count, B[i].Value.Count); k++)
                    if (!UxBin.Same(A[i].Value[k], B[i].Value[k]) && bad++ < 3) SameLines(lv.disc + " .scene " + A[i].Key + " chunk " + k, Canon(A[i].Value[k]), Canon(B[i].Value[k]));
            }
            if (bad == 0) UxTest.Fail(lv.disc + " .scene: the chunks are equal but the file differs");
        }
        else UxTest.Report(true, lv.disc + " .scene = " + name + " byte for byte (" + ms.Length + " bytes)");
        // ---- database
        byte[] md = Mine("database"), rd = Ref("database");
        Func<string, string, bool> mask = (node, attr) => node == "Heap" && (attr == "offset" || attr == "size") || node == "Package" && (attr == "offset" || attr == "heapOffset");
        Func<string, bool> skip = packing == "raw" ? (Func<string, bool>)(n => n == "Compress") : null;
        SameLines(lv.disc + " .database = " + name + " as a document (Heap offset / size and Package offsets apart" + (packing == "raw" ? ", Compress elements apart" : "") + ")",
            Canon(md, mask, skip), Canon(rd, mask, skip));
        // ---- package: every resource decoded
        var mine = new UxTrackSet(slot, md, Mine("package")); var theirs = new UxTrackSet(slot, rd, Ref("package"));
        int same = 0, diff = 0, missing = 0, order = 0, client = 0; long logical = 0;
        // --core-blobs: this build's stored bytes + the REFERENCE's decoded bytes of every compressed resource, for the C core's reader
        // (core_lzx_test.sh: core/src/fmt_lzx.c fmt_xmem_resource through foundation/core_stored_test.c)
        BinaryWriter coreBlobs = null;
        if (CoreBlobs) { Directory.CreateDirectory(Path.Combine(env.scratch, "coreblobs")); coreBlobs = new BinaryWriter(File.Create(Path.Combine(env.scratch, "coreblobs", lv.shortName + ".bin"))); }
        if (mine.assets.Count != theirs.assets.Count) UxTest.Fail(lv.disc + " .package: " + mine.assets.Count + " resources, the reference has " + theirs.assets.Count);
        for (int i = 0; i < theirs.assets.Count; i++)
        {
            var t = theirs.assets[i]; var m = i < mine.assets.Count ? mine.assets[i] : null;
            if (m == null || m.name != t.name || m.type != t.type || m.PackageName != t.PackageName) { order++; m = mine.TryGet(t.name, t.type); }
            if (m == null) { missing++; if (missing <= 5) UxTest.Log("       missing " + t.name + "." + t.type); continue; }
            byte[] a = mine.ReadLogical(m), b = theirs.ReadLogical(t);
            logical += b.Length;
            if (UxBin.Same(a, b)) same++; else { diff++; if (diff <= 8) UxTest.Log("       differs " + t.name + "." + t.type + ": " + UxTest.Difference(a, b)); }
            if (m.compressed && coreBlobs != null) { byte[] bl = mine.Blob(m); coreBlobs.Write(bl.Length); coreBlobs.Write(bl); coreBlobs.Write(b.Length); coreBlobs.Write(b); }
            if (m.compressed) { byte[] c = UxPkg.RfxLzx(mine.Blob(m)); if (UxBin.Same(c, a)) client++; else { diff++; UxTest.Log("       the client's reader differs on " + t.name + "." + t.type); } }
        }
        UxTest.Report(diff == 0 && missing == 0 && order == 0 && mine.assets.Count == theirs.assets.Count,
            lv.disc + " .package: " + same + " of " + theirs.assets.Count + " resources decode to the reference's bytes (" + Mb(logical) + " MB), same order" +
            (diff + missing + order > 0 ? "; " + diff + " differ, " + missing + " missing, " + order + " out of order" : "") + "; " + client + " also through the client's reader");
        coreBlobs?.Dispose();
        // ---- mod.ini, sha256.txt, preview
        // (the author and the words after the track's name in the description are this port's own: UxPackage.AUTHOR / CONVERTED)
        string wantIni = Regex.Replace(Utf8.GetString(File.ReadAllBytes(Path.Combine(rel, "mod.ini"))), @"(?m)^author = .*$", "author = " + UxPackage.AUTHOR);
        wantIni = Regex.Replace(wantIni, @"(?m)^(description = " + Regex.Escape(lv.display) + @") from MX vs ATV Untamed \(Xbox 360\), converted [^;\n]*?(?=(; \d{4}-\d{2}-\d{2})?\. Unofficial)", m => m.Groups[1].Value + UxPackage.CONVERTED);
        UxTest.SameBytes(lv.disc + " mod.ini = " + name + " (author and description wording apart)", File.ReadAllBytes(Path.Combine(r.folder, "mod.ini")), Utf8.GetBytes(wantIni));
        var list = UxTest.ReadMd5List(Path.Combine(r.folder, "sha256.txt")); bool okList = list.Count == 6;
        foreach (var kv in list) okList &= UxBin.Sha256File(Path.Combine(r.folder, kv.Key)) == kv.Value;
        UxTest.Report(okList, lv.disc + " sha256.txt lists the six files with their hashes");
        var pv = UxImaging.ReadPngRgb(File.ReadAllBytes(Path.Combine(r.folder, "preview.png")));
        UxTest.Report(pv.w == 512 && pv.h == 512, lv.disc + " preview.png is a 512 x 512 RGB picture (the reference's is its verifier's render: not compared)");
        // ---- build_info.json
        string refInfo = Path.Combine(bld, "build_info.json");
        var want = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(refInfo))); var got = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(workRoot, "build_info.json"))));
        foreach (string p in InfoSkip) { Remove(want, p); Remove(got, p); }
        if (UxJson.Get(want, "rollout", "ec_water") is JObj wr && !wr.Has("look"))
        {   // a reference build older than the water-look round (2026-11-11): its water record carries no look (the 10 supercross builds: a no-op record)
            Remove(got, "rollout.ec_water.look"); Remove(got, "rollout.ec_water.look_params");
        }
        if (UxJson.Get(want, "rollout") is JObj wro && !wro.Has("dirt_look") && UxJson.Dumps(UxJson.Get(got, "rollout", "dirt_look"), -1) == "{\"option\": \"untamed\"}")
            Remove(got, "rollout.dirt_look");      // a reference build older than the dirt-look round has no such record; this build's says 'untamed' and nothing else
        var d = UxTest.JsonDifferences(got, want, false, 400);
        foreach (string line in d.GetRange(0, Math.Min(d.Count, 12))) UxTest.Log("       build_info: " + Cut(line));
        UxTest.Report(d.Count == 0, lv.disc + " build_info.json records = " + name + " (" + InfoSkip.Length + " environment fields apart)" + (d.Count > 0 ? ": " + d.Count + " differences" : ""));
    }
    static void Remove(JObj o, string path)
    {
        var parts = path.Split('.');
        for (int i = 0; i + 1 < parts.Length && o != null; i++) o = o.Get(parts[i]) as JObj;
        o?.items.RemoveAll(kv => kv.Key == parts[parts.Length - 1]);
    }

    // ============================================================================================== the tree the Python gates read
    /// Builds a level into <root>/release/<short><suffix> with the work store <root>/work/<Level> (what ux_verify reads of the stages:
    /// level.json, terrain/, materials/ ...; plus collision/ = the collision stage with --type9 refuse, the folder a release verify of
    /// the reference reads), and <root>/build/<same name>/ = build_info.json + the package and level file, where the verifier takes
    /// the release's rollout record from. root is a scratch copy of the reference tree (tools, templates, extract).
    static void GateTree(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string packing, string root, string suffix)
    {
        root = Path.GetFullPath(root).Replace('\\', '/').TrimEnd('/');
        string name = lv.shortName + suffix, rel = Path.Combine(root, "release", name), bld = Path.Combine(root, "build", name), workRoot = Path.Combine(root, "work");
        if (Directory.Exists(Path.Combine(workRoot, lv.disc))) Directory.Delete(Path.Combine(workRoot, lv.disc), true);
        var ctx = new UxContext { lv = lv, disc = env.disc.Level(lv), library = env.library, stock = env.stock, work = UxWork.ForLevel(workRoot, lv), log = s => { } };
        var o = UxTrackOptions.ForLevel(lv);
        o.packing = packing; o.paths = new UxPaths { root = root, game = (Environment.GetEnvironmentVariable("UX_PY_GAME") ?? "C:/Program Files (x86)/Steam/steamapps/common/MX vs ATV Reflex").Replace('\\', '/') };
        o.date = DateTime.Now.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
        if (Directory.Exists(rel)) Directory.Delete(rel, true);
        var sw = Stopwatch.StartNew();
        var r = UxBuildTrack.Build(ctx, env.disc, o, rel, null, default, shared);
        UxCollision.Convert(ctx, new UxCollisionOptions { type9 = "refuse", popClip = o.popClip });
        if (Directory.Exists(bld)) Directory.Delete(bld, true);
        Directory.CreateDirectory(bld);
        File.Copy(Path.Combine(workRoot, lv.disc, "build_info.json"), Path.Combine(bld, "build_info.json"));
        foreach (string k in new[] { "package", "level" }) File.Copy(Path.Combine(rel, r.slot + ".dx9." + k), Path.Combine(bld, r.slot + ".dx9." + k));
        UxTest.Report(true, lv.disc + " -> " + rel + " (" + Mb(r.packageBytes) + " MB package, " + F(sw.Elapsed.TotalSeconds) + " s)");
    }

    // ============================================================================================== determinism
    static void Determinism(UxTest.Env env, UxBuildShared shared, UxLevelRow lv, string packing)
    {
        var lists = new List<string>(); var names = new[] { "run A (all threads)", "run B (all threads, again)", "run C (one thread)" };
        for (int i = 0; i < 3; i++)
        {
            var o = UxTrackOptions.ForLevel(lv); o.packing = packing; o.threads = i == 2 ? 1 : 0;
            string outDir = Path.Combine(env.scratch, "det" + i, lv.shortName);
            var sw = Stopwatch.StartNew();
            UxBuildResult r;
            if (i == 1)
            {   // the second run through the short entry point: no shared state, the disc taken from the level's own volume
                var ctx = env.Level(lv, "detwork" + i, true); ctx.log = s => { };
                if (Directory.Exists(outDir)) Directory.Delete(outDir, true);
                r = UxBuildTrack.Build(ctx, o, outDir);
            }
            else r = Run(env, shared, lv, outDir, "detwork" + i, o, true);
            lists.Add(File.ReadAllText(Path.Combine(outDir, "sha256.txt")));
            UxTest.Log("     " + names[i] + ": " + F(sw.Elapsed.TotalSeconds) + " s");
        }
        UxTest.Report(lists[0] == lists[1], lv.disc + ": two runs give the same seven files (sha256.txt equal)");
        UxTest.Report(lists[0] == lists[2], lv.disc + ": a one-thread run gives the same seven files");
        if (lists[0] != lists[2] || lists[0] != lists[1]) UxTest.Log(lists[0] + "\n" + lists[1] + "\n" + lists[2]);
    }
}
