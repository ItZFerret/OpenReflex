// tests/untamed_convert/foundation: the foundation of the MX vs ATV Untamed converter port against the Python reference
// (UX_PY_ROOT), headless. run.sh passes the inputs; a part whose input is missing is reported as skipped.
//   s0        every asset of the 12 levels from the disc image and from the extracted folder: the extract index (names, offsets, sizes)
//             and each asset's md5 against extract/<Level>/index.json; the scene / level / database / tdf07 files; parallel reads
//   json      every JSON file of work/<Level>/ parsed and written again: the bytes ux_common.write_json wrote
//   zlib      zlib.compress parity: every BXML document of the stock and Untamed scenes / databases / level files recompressed
//   stock     templates/stock-lib, templates/reflex-lib and the SX_FortDodge base from a Reflex library (original install, client game
//             files); the package read whole and written back with stored resources
//   readers   the shared readers (surface / model / collis / material / texture / terrain / scene / shader) against digests the Python
//             modules wrote (tests/untamed_convert/foundation/ref_readers.py)
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static partial class Program
{
    static string s_py, s_scratch, s_ref;

    static int Main(string[] args)
    {
        if (args.Length == 0) { Console.Error.WriteLine("usage: foundation <s0|json|zlib|stock|readers|all> [--iso F] [--folder D] [--py D] [--reflex D] [--client D] [--scratch D] [--ref D] [--levels a,b]"); return 2; }
        string what = args[0];
        s_py = UxTest.Arg(args, "py", "UX_PY_ROOT", "");
        s_scratch = UxTest.Arg(args, "scratch", "UX_SCRATCH", Path.Combine(Path.GetTempPath(), "untamed-port-foundation"));
        s_ref = UxTest.Arg(args, "ref", "UX_REF", Path.Combine(s_scratch, "ref"));
        string iso = UxTest.Arg(args, "iso", "UX_DISC_ISO"), folder = UxTest.Arg(args, "folder", "UX_DISC_FOLDER");
        string reflex = UxTest.Arg(args, "reflex", "UX_REFLEX"), client = UxTest.Arg(args, "client", "UX_CLIENT_LIB");
        string only = UxTest.Arg(args, "levels");
        var levels = only == null ? UxCommon.Levels.ToList() : only.Split(',').Select(UxCommon.Level).ToList();
        Directory.CreateDirectory(s_scratch);
        try
        {
            if (what == "s0" || what == "all")
            {
                if (iso != null && File.Exists(iso)) S0(iso, "disc image", folder, levels); else UxTest.Skip("s0 from the disc image (--iso)");
                if (folder != null && Directory.Exists(folder)) S0(folder, "extracted folder", null, levels); else UxTest.Skip("s0 from the extracted folder (--folder)");
            }
            if (what == "json" || what == "all") JsonParity(levels);
            if (what == "zlib" || what == "all") ZlibParity(reflex, folder);
            if (what == "stock" || what == "all")
            {
                if (reflex != null && Directory.Exists(reflex)) Stock(reflex, "original install", true); else UxTest.Skip("stock library from the original install (--reflex)");
                if (client != null && Directory.Exists(client)) Stock(client, "client game files", false); else UxTest.Skip("stock library from the client's game files (--client)");
            }
            if (what == "readers" || what == "all")
            {
                string src = folder != null && Directory.Exists(folder) ? folder : iso;
                if (src != null) Readers(src, reflex, levels); else UxTest.Skip("readers (--folder or --iso)");
            }
        }
        catch (Exception e) { UxTest.Fail("unexpected " + e.GetType().Name + ": " + e.Message + "\n" + e.StackTrace); }
        return UxTest.Result("foundation " + what);
    }

    // ================================================================================================ S0
    static void S0(string source, string label, string otherFolder, List<UxLevelRow> levels)
    {
        UxTest.Section("S0 from the " + label + ": " + source);
        long peakBefore = GC.GetTotalMemory(true);
        using (var disc = UxDisc.Open(source))
        {
            UxTest.Check(disc.root != null, label + ": the AI folder was found beside Database");
            int assets = 0, lzx = 0, bad = 0; long bytes = 0, peak = 0;
            foreach (var lv in levels)
            {
                string idxPath = Path.Combine(s_py, "extract", lv.disc, "index.json");
                if (!File.Exists(idxPath)) { UxTest.Skip(lv.disc + ": no " + idxPath); continue; }
                var want = UxJson.Obj(UxJson.Parse(File.ReadAllText(idxPath)));
                var wa = UxJson.Arr(want.Get("assets"));
                var level = disc.Level(lv);
                level.CacheBytes = 0;
                UxTest.Check(level.assets.Count == wa.Count, lv.disc + ": " + level.assets.Count + " assets, the extract index lists " + wa.Count);
                int n = Math.Min(level.assets.Count, wa.Count);
                var md5 = new string[n]; var size = new long[n];
                // one pass in parallel: also the thread-safety test (the same reads give the same bytes as a second, serial pass below)
                Parallel.For(0, n, new ParallelOptions { MaxDegreeOfParallelism = Math.Max(2, Environment.ProcessorCount) }, i =>
                {
                    var d = level.Read(level.assets[i]);
                    md5[i] = UxBin.Md5(d); size[i] = d.Length;
                });
                int levelBad = 0;
                for (int i = 0; i < n; i++)
                {
                    var a = level.assets[i]; var w = UxJson.Obj(wa[i]);
                    bool head = a.name == (string)w.Get("name") && a.type == (string)w.Get("type") && a.package == (string)w.Get("package") &&
                        a.AbsOffset == UxJson.Int(w.Get("offset")) && a.size == UxJson.Int(w.Get("stored_size")) && a.compressed == UxJson.Bool(w.Get("compressed")) &&
                        a.file.Replace('/', '\\') == (string)w.Get("file");
                    bool same = head && md5[i] == (string)w.Get("md5") && size[i] == UxJson.Int(w.Get("size"));
                    if (!same) { levelBad++; if (levelBad <= 5) UxTest.Log("     " + lv.disc + " #" + i + " " + a + ": " + (head ? "md5 " + md5[i] + " want " + w.Get("md5") : "index row differs (" + a.file + " vs " + w.Get("file") + ")")); }
                    if (a.compressed) lzx++;
                    bytes += size[i];
                }
                assets += n; bad += levelBad;
                UxTest.Report(levelBad == 0, lv.disc + ": " + levelBad + " of " + n + " assets differ from the extract index");
                // a serial second pass over a sample (every 7th asset) through the name lookups the stages use
                int again = 0;
                for (int i = 0; i < n; i += 7)
                {
                    var a = level.assets[i];
                    var byFile = level.ReadExtractFile(a.file);
                    if (UxBin.Md5(byFile) != md5[i]) again++;
                    var first = level.Get(a.name, a.type);
                    if (first.index == a.index && UxBin.Md5(level.Read(a.name, a.type)) != md5[i]) again++;
                }
                UxTest.Check(again == 0, lv.disc + ": " + again + " serial re-reads differ from the parallel pass");
                // the index.json uxpkg.extract writes (json.dumps(indent=1) through a text-mode file: CRLF line ends on Windows)
                string mine = UxJson.Dumps(level.ExtractIndex(true), 1);
                byte[] refBytes = File.ReadAllBytes(idxPath);
                bool crlf = Encoding.UTF8.GetString(refBytes).Contains("\r\n");
                UxTest.SameBytes(lv.disc + " index.json", Encoding.UTF8.GetBytes(crlf ? mine.Replace("\n", "\r\n") : mine), refBytes);
                // the level's own files and the race line against the extracted folder
                string of = otherFolder ?? (Directory.Exists(source) ? source : null);
                if (of != null)
                {
                    foreach (string kind in new[] { "database", "level", "scene" })
                        UxTest.SameBytes(lv.disc + ".xenon." + kind, level.ReadFile(kind), File.ReadAllBytes(Path.Combine(of, "Database", lv.disc + ".xenon." + kind)));
                    UxTest.SameBytes("AI/" + lv.tdf, level.ReadTdf(), File.ReadAllBytes(Path.Combine(of, "AI", lv.tdf)));
                    UxTest.Check(level.FileSize("package") == new FileInfo(Path.Combine(of, "Database", lv.disc + ".xenon.package")).Length, lv.disc + ".xenon.package size");
                }
                else { UxTest.Check(level.ReadFile("scene").Length > 0 && level.ReadTdf().Length > 0, lv.disc + ": scene and race line readable"); }
                peak = Math.Max(peak, GC.GetTotalMemory(false));
                level.ClearCache();
            }
            UxTest.Log("     " + label + ": " + assets + " assets (" + lzx + " LZX), " + bytes + " logical bytes, " + bad + " differ; managed heap after a level at most " +
                (peak >> 20) + " MB (before: " + (peakBefore >> 20) + " MB), process peak " + (System.Diagnostics.Process.GetCurrentProcess().PeakWorkingSet64 >> 20) + " MB");
        }
    }

    // ================================================================================================ JSON
    static void JsonParity(List<UxLevelRow> levels)
    {
        UxTest.Section("JSON: work/<Level>/**/*.json parsed and written again");
        int files = 0, same = 0, plain = 0, other = 0; long bytes = 0;
        var otherFiles = new List<string>();
        foreach (var lv in levels)
        {
            string root = Path.Combine(s_py, "work", lv.disc);
            if (!Directory.Exists(root)) { UxTest.Skip("no " + root); continue; }
            foreach (string f in Directory.GetFiles(root, "*.json", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                var fi = new FileInfo(f);
                if (fi.Length > 96 << 20) continue;
                byte[] raw = File.ReadAllBytes(f);
                object doc;
                try { doc = UxJson.ParseUtf8(raw); }
                catch (Exception e) { UxTest.Fail(f + ": " + e.Message); continue; }
                files++; bytes += raw.Length;
                if (UxBin.Same(Encoding.UTF8.GetBytes(UxJson.DumpsUx(doc)), raw)) { same++; continue; }
                // not written by ux_common.write_json: json.dump(s) with another indent / no sorting / ASCII escapes
                bool hit = false;
                foreach (int indent in new[] { 1, 2, 4, -1 })
                    foreach (bool sort in new[] { false, true })
                        foreach (bool ascii in new[] { true, false })
                            foreach (string tail in new[] { "", "\n" })
                                if (!hit && UxBin.Same(Encoding.UTF8.GetBytes(UxJson.Dumps(doc, indent, sort, ascii) + tail), raw)) hit = true;
                if (hit) plain++; else { other++; if (otherFiles.Count < 8) otherFiles.Add(f.Substring(s_py.Length + 1) + ": " + UxTest.Difference(Encoding.UTF8.GetBytes(UxJson.DumpsUx(doc)), raw)); }
            }
        }
        foreach (var o in otherFiles) UxTest.Log("     " + o);
        UxTest.Report(other == 0, "JSON files reproduced byte for byte: " + (same + plain) + " of " + files + " (" + same + " as ux_common.write_json, " + plain +
            " as plain json.dumps), " + other + " not reproduced; " + bytes + " bytes");
        // ux_common._json_default and number rules
        var o2 = new JObj().Set("b", new[] { 1.5f, 0.1f }).Set("a", new byte[] { 0xde, 0xad }).Set("é", 1e22).Set("n", 1e16).Set("z", -0.0).Set("i", 12345678901234L)
            .Set("t", true).Set("x", null).Set("m", new double[,] { { 1, 2 }, { 3, 4 } }).Set("s", "a\"b\\c\n\u0001é\u2028");
        string wantUx = "{\n \"a\": \"dead\",\n \"b\": [\n  1.5,\n  0.10000000149011612\n ],\n \"i\": 12345678901234,\n \"m\": [\n  [\n   1.0,\n   2.0\n  ],\n  [\n   3.0,\n   4.0\n  ]\n ],\n \"n\": 1e+16,\n" +
            " \"s\": \"a\\\"b\\\\c\\n\\u0001é\u2028\",\n \"t\": true,\n \"x\": null,\n \"z\": -0.0,\n \"é\": 1e+22\n}\n";
        UxTest.Check(UxJson.DumpsUx(o2) == wantUx, "dumps_json of numpy scalars / bytes / nested arrays / non-ASCII keys:\n" + UxJson.DumpsUx(o2));
        UxTest.Check(UxJson.Dumps(new JObj().Set("k", "é\u2028").Set("f", 5e-324).Set("g", 123456789012345680.0)) == "{\"k\": \"\\u00e9\\u2028\", \"f\": 5e-324, \"g\": 1.2345678901234568e+17}", "json.dumps defaults: " +
            UxJson.Dumps(new JObj().Set("k", "é\u2028").Set("f", 5e-324).Set("g", 123456789012345680.0)));
        bool nan = false; try { UxJson.DumpsUx(new JObj().Set("x", double.NaN)); } catch (UxException) { nan = true; }
        UxTest.Check(nan, "dumps_json refuses NaN (allow_nan=False)");
        // float parsing: the nearest double, every runtime
        foreach (var (text, bits) in new[] { ("0.1", 0x3FB999999999999AL), ("1e-320", 0x7E8L), ("2.2250738585072011e-308", 0x000FFFFFFFFFFFFFL), ("1.7976931348623157e308", 0x7FEFFFFFFFFFFFFFL),
            ("9007199254740993", 0x4340000000000000L), ("9007199254740993.0000000001", 0x4340000000000001L), ("-4.35", unchecked((long)0xC011666666666666UL)), ("123456789e-5", 0x40934A4584F4C6E7L) })
            UxTest.Check(BitConverter.DoubleToInt64Bits(UxJson.ParseDouble(text)) == bits, "float('" + text + "') = " + PyFormat.Repr(UxJson.ParseDouble(text)));
        // numpy helpers
        UxTest.Check(UxNp.Round(0.5) == 0 && UxNp.Round(1.5) == 2 && UxNp.Round(2.5) == 2 && UxNp.Round(-0.5) == 0, "np.round ties to even");
        UxTest.Check(UxNp.PyRound(2.675, 2) == 2.67 && UxNp.PyRound(1.0000005, 6) == 1.000001 && UxNp.PyRound(1.00000049, 6) == 1.0 && UxNp.PyRound(0.5, 0) == 0 && UxNp.PyRound(1234.5678, -2) == 1200.0, "Python round(x, n)");
        UxTest.Check(UxNp.DoubleToHalf(1.0) == 0x3C00 && UxNp.DoubleToHalf(65520.0) == 0x7C00 && UxNp.DoubleToHalf(65519.99) == 0x7BFF && UxNp.DoubleToHalf(-2.9802322387695312e-08) == 0x8000 &&
            UxNp.DoubleToHalf(2.98023223876953125e-08 * 1.0000001) == 0x0001 && UxNp.DoubleToHalf(1.00048828125) == 0x3C00 && UxNp.DoubleToHalf(1.00146484375) == 0x3C02, "float64 -> float16 rounding");
        for (int h = 0; h < 65536; h++)
        {
            float f = UxNp.HalfToFloat((ushort)h);
            if (float.IsNaN(f)) continue;
            if (UxNp.FloatToHalf(f) != h) { UxTest.Fail("float16 round trip of 0x" + h.ToString("x4")); break; }
        }
        UxTest.Checks++;
    }

    // ================================================================================================ zlib
    static uint s_lcg;
    static byte[] Lcg(int n, uint seed, int mask)
    {
        uint x = seed; var o = new byte[n];
        for (int i = 0; i < n; i++) { x = unchecked(x * 1664525u + 1013904223u); o[i] = (byte)((x >> 24) & mask); }
        return o;
    }

    static void ZlibParity(string reflex, string discFolder)
    {
        UxTest.Section("zlib.compress parity (level 6)");
        string refPath = Path.Combine(s_ref, "zlib.json");
        if (File.Exists(refPath))
        {
            var r = UxJson.Obj(UxJson.Parse(File.ReadAllText(refPath)));
            int bad = 0, n = 0;
            foreach (var row in UxJson.Arr(r.Get("synthetic")))
            {
                var l = UxJson.Arr(row);
                byte[] data = Lcg((int)UxJson.Int(l[0]), (uint)UxJson.Int(l[1]), (int)UxJson.Int(l[2]));
                byte[] z = UxZlib.Compress(data);
                n++;
                if (UxBin.Md5(z) != (string)l[3]) { bad++; UxTest.Log("     synthetic n=" + l[0] + " seed " + l[1] + " mask " + l[2] + ": md5 differs"); }
                if (!UxBin.Same(UxZlib.Decompress(z), data)) { bad++; UxTest.Log("     synthetic n=" + l[0] + ": does not decompress to the input"); }
                if (!UxBin.Same(UxZlib.Decompress(UxZlib.Stored(data)), data)) { bad++; UxTest.Log("     synthetic n=" + l[0] + ": stored stream does not decompress to the input"); }
            }
            UxTest.Report(bad == 0, "synthetic inputs: " + (n - bad) + " of " + n + " compress to Python's bytes (zlib " + r.Get("zlib") + ")");
        }
        else UxTest.Skip("zlib synthetic reference " + refPath + " (run ref_zlib.py)");
        int docs = 0, same = 0;
        void Doc(string name, byte[] d, int p)
        {
            int z = (int)UxBin.U32LE(d, p + 32);
            byte[] body = UxZlib.Decompress(d, p + 36, z);
            byte[] c = UxZlib.Compress(body);
            docs++;
            if (UxBin.Same(c, 0, d, p + 36, z) && c.Length == z) same++; else if (docs - same <= 5) UxTest.Log("     " + name + " @" + p + ": recompressed " + c.Length + " B, stored " + z + " B");
        }
        void Scene(string path)
        {
            byte[] d = File.ReadAllBytes(path); bool big = UxXScene.DetectBigEndian(d);
            int n = (int)UxBin.U32(d, 4, big);
            for (int i = 0; i < n; i++)
            {
                int off = (int)UxBin.U32(d, 8 + i * 264 + 260, big), nch = (int)UxBin.U32(d, off, big), q = off + 8;
                for (int k = 0; k < nch; k++) { Doc(Path.GetFileName(path), d, q); q += 36 + (int)UxBin.U32LE(d, q + 32); }
            }
        }
        if (reflex != null && Directory.Exists(Path.Combine(reflex, "Database")))
            foreach (string lv in new[] { "SX_FortDodge", "SX_Kingston" })
            {
                Scene(Path.Combine(reflex, "Database", lv + ".dx9.scene"));
                Doc(lv + ".dx9.database", File.ReadAllBytes(Path.Combine(reflex, "Database", lv + ".dx9.database")), 0);
                Doc(lv + ".dx9.level", File.ReadAllBytes(Path.Combine(reflex, "Database", lv + ".dx9.level")), 0);
            }
        else UxTest.Skip("zlib on the stock scenes (--reflex)");
        if (discFolder != null && Directory.Exists(Path.Combine(discFolder, "Database")))
            foreach (var lv in UxCommon.Levels)
            {
                Scene(Path.Combine(discFolder, "Database", lv.disc + ".xenon.scene"));
                Doc(lv.disc + ".xenon.database", File.ReadAllBytes(Path.Combine(discFolder, "Database", lv.disc + ".xenon.database")), 0);
                Doc(lv.disc + ".xenon.level", File.ReadAllBytes(Path.Combine(discFolder, "Database", lv.disc + ".xenon.level")), 0);
            }
        else UxTest.Skip("zlib on the Untamed scenes (--folder)");
        UxTest.Report(same == docs, "BXML documents recompressed to their stored bytes: " + same + " of " + docs + " (Python: all of them, ref_zlib.py)");
    }

    // ================================================================================================ stock
    static byte[] Bytes(int[] a) { var b = new byte[a.Length * 4]; Buffer.BlockCopy(a, 0, b, 0, b.Length); return b; }
    static void Stock(string root, string label, bool original)
    {
        UxTest.Section("stock library from the " + label + ": " + root);
        string templates = Path.Combine(s_py, "templates");
        if (string.IsNullOrEmpty(s_py) || !File.Exists(Path.Combine(templates, "stock-lib", "md5.txt"))) { UxTest.Skip(label + ": the stock library comparison needs the reference (UX_PY_ROOT)"); return; }
        using (var lib = new UxDirectoryLibrary(root))
        {
            UxTest.Log("     " + lib.Describe + (lib.IsExtracted ? " (extracted assets)" : " (packages)"));
            UxTest.Check(lib.IsExtracted == !original, label + ": extracted marker " + lib.IsExtracted);
            var stock = new UxStock(lib);
            string outDir = UxTest.FreshFolder(s_scratch, "stock-" + (original ? "install" : "client"));
            var written = stock.WriteReferenceFolders(outDir);
            // templates/stock-lib against its md5.txt (index.json is the reference's own record: not a game file)
            var pins = UxTest.ReadMd5List(Path.Combine(templates, "stock-lib", "md5.txt"));
            int ok = 0, total = 0;
            foreach (var kv in pins)
            {
                if (kv.Key == "index.json") continue;
                total++;
                string f = Path.Combine(outDir, "stock-lib", kv.Key.Replace('/', Path.DirectorySeparatorChar));
                if (File.Exists(f) && UxBin.Md5File(f) == kv.Value) ok++; else UxTest.Log("     stock-lib/" + kv.Key + ": " + (File.Exists(f) ? "md5 " + UxBin.Md5File(f) + " want " + kv.Value : "not produced"));
            }
            UxTest.Report(ok == total && total == UxStock.Assets.Length + UxStock.Prototypes.Length, label + ": templates/stock-lib files produced with the pinned md5: " + ok + " of " + total);
            UxTest.CompareFolder(label + ": stock-lib against the reference folder", Path.Combine(outDir, "stock-lib"), Path.Combine(templates, "stock-lib"), rel => rel != "index.json" && rel != "md5.txt", false, true);
            UxTest.CompareFolder(label + ": reflex-lib (shaders, exemplar materials, prototypes)", Path.Combine(outDir, "reflex-lib"), Path.Combine(templates, "reflex-lib"),
                rel => rel.EndsWith(".shader") || rel.EndsWith(".material") || rel.EndsWith(".bxml"), false, true);
            // the table against the reference's index.json
            var sidx = UxJson.Obj(UxJson.Parse(File.ReadAllText(Path.Combine(templates, "stock-lib", "index.json"))));
            var mine = new JObj(); foreach (var a in UxStock.Assets) mine.Set(a.key, a.Record());
            var wantAssets = UxJson.Obj(sidx.Get("assets"));
            bool order = wantAssets.items.Count == mine.items.Count;
            for (int i = 0; order && i < mine.items.Count; i++) order = mine.items[i].Key == wantAssets.items[i].Key;
            UxTest.Report(order && UxTest.JsonDifferences(mine, wantAssets).Count == 0, "UxStock.Assets equals stock-lib/index.json 'assets' (" + mine.items.Count + " rows, same order)");
            var ridx = UxJson.Obj(UxJson.Parse(File.ReadAllText(Path.Combine(templates, "reflex-lib", "index.json"))));
            foreach (var t in UxStock.LibTemplates)
            {
                var w = UxJson.Obj(UxJson.Get(ridx, "templates", t.name)); var m = t.Index(); int d = 0;
                foreach (var kv in m.items) if (UxTest.JsonDifferences(kv.Value, w.Get(kv.Key)).Count != 0) { d++; UxTest.Log("     " + t.name + "." + kv.Key + " differs from reflex-lib/index.json"); }
                UxTest.Check(d == 0, "UxLibTemplate " + t.name + " equals reflex-lib/index.json");
            }
            UxTest.Check(UxJson.Obj(ridx.Get("templates")).items.Count == UxStock.LibTemplates.Length && UxJson.Obj(ridx.Get("prototypes")).items.Count == UxStock.LibPrototypes.Length, "reflex-lib table sizes");
            // prototypes as trees
            foreach (var p in UxStock.Prototypes) { var (e, ver, _) = stock.Prototype(p.rsdata); UxTest.Check(UxCommon.RsdataType(e) == p.rsdata && ver == 66538, "stock-lib prototype " + p.rsdata); }
            foreach (var p in UxStock.LibPrototypes) { var (e, _, _) = stock.LibPrototype(p.rsdata); UxTest.Check(UxCommon.RsdataType(e) == p.rsdata && UxPcScene.EntityId(e) == p.entityId, "reflex-lib prototype " + p.rsdata); }

            // ---- the clone base: the four files and every asset
            string fd = UxStock.DEFAULT_STOCK_LEVEL;
            foreach (string kind in new[] { "database", "level", "scene" })
                UxTest.SameBytes(label + ": " + fd + ".dx9." + kind, stock.BaseFile(kind), File.ReadAllBytes(Path.Combine(templates, "stock-" + fd, fd + ".dx9." + kind)));
            var ts = stock.BaseSet();
            string refFd = Path.Combine(s_ref, "fortdodge.json");
            byte[][] logical = new byte[ts.assets.Count][];
            Parallel.For(0, ts.assets.Count, i => { logical[i] = ts.ReadLogical(ts.assets[i]); });
            if (File.Exists(refFd))
            {
                var wa = UxJson.Arr(UxJson.Obj(UxJson.Parse(File.ReadAllText(refFd))).Get("assets"));
                int bad = ts.assets.Count == wa.Count ? 0 : 1;
                for (int i = 0; i < Math.Min(wa.Count, ts.assets.Count); i++)
                {
                    var w = UxJson.Obj(wa[i]); var a = ts.assets[i];
                    if (a.name != (string)w.Get("name") || a.type != (string)w.Get("type") || a.PackageName != (string)w.Get("package") || UxBin.Md5(logical[i]) != (string)w.Get("md5") || logical[i].Length != UxJson.Int(w.Get("size")))
                    { bad++; if (bad <= 5) UxTest.Log("     " + a + ": md5 " + UxBin.Md5(logical[i]) + " want " + w.Get("md5")); }
                }
                UxTest.Report(bad == 0, label + ": " + fd + " assets with the reference decoder's md5: " + (ts.assets.Count - bad) + " of " + wa.Count);
            }
            else UxTest.Skip("the Python decode of " + fd + " (" + refFd + ": run ref_fortdodge.py)");

            // ---- the shared PC codecs (rmesh, uxtex.PcTexture / build_pc, uxmat, d3d9sh) on every asset of the base: read + write gives the bytes back
            {
                int surf = 0, surfOk = 0, surfOther = 0, mod = 0, modOk = 0, modOther = 0, col = 0, colOk = 0, colOther = 0, tex = 0, texOk = 0, mat = 0, matOk = 0, sh = 0, shOk = 0;
                var firstBad = new List<string>();
                for (int i = 0; i < ts.assets.Count; i++)
                {
                    var a = ts.assets[i]; byte[] b = logical[i];
                    try
                    {
                        if (a.type == "surface")
                        {
                            List<UxSurfaceGroup> g; byte[] tail;
                            try { g = UxRmesh.ReadSurface(b, out tail); } catch (UxException) { surfOther++; continue; }      // skinned / other vertex formats: refused, as in Python
                            surf++; if (UxBin.Same(UxRmesh.WriteSurface(g, tail), b)) surfOk++; else firstBad.Add(a.ToString());
                            foreach (var grp in g) { var t3 = UxRmesh.StripToTris(grp.strip); if (t3.Length > 0 && t3.Length <= 3 * 400 && !UxBin.Same(Bytes(UxRmesh.Canon(UxRmesh.StripToTris(UxRmesh.Stripify(t3)))), Bytes(UxRmesh.Canon(t3)))) firstBad.Add(a + ": stripify changed the triangles"); }
                        }
                        else if (a.type == "model")
                        {
                            UxPcModel m;
                            try { m = UxRmesh.ReadModel(b); } catch (UxException) { modOther++; continue; }
                            mod++; if (UxBin.Same(UxRmesh.WriteModel(m), b)) modOk++; else firstBad.Add(a.ToString());
                        }
                        else if (a.type == "collis")
                        {
                            var cl = UxRmesh.ReadCollis(b, out uint kind);
                            if (kind != 22) { colOther++; continue; }
                            col++; if (UxBin.Same(UxRmesh.WriteCollis(cl), b)) colOk++; else firstBad.Add(a.ToString());
                        }
                        else if (a.type == "texture")
                        {
                            var t = new UxPcTexture(b);
                            tex++; if (t.end == b.Length && UxBin.Same(UxTex.BuildPc(t.recs, t.table, t.entries, t.kind, t.flags), b)) texOk++; else firstBad.Add(a.ToString());
                        }
                        else if (a.type == "material") { mat++; var m = UxMat.ReadMaterial(b, false); if (m.passes.Count > 0) { UxMat.NamedStates(m.passes[0].states, true); matOk++; } else firstBad.Add(a.ToString()); }
                        else if (a.type == "shader") { sh++; if (UxD3d9sh.Summarize(b).Count > 0) shOk++; else firstBad.Add(a.ToString()); }
                    }
                    catch (UxException e) { firstBad.Add(a + ": " + e.Message); }
                }
                foreach (var x in firstBad.Take(8)) UxTest.Log("     " + x);
                UxTest.Report(firstBad.Count == 0, label + ": PC codecs on " + fd + ": surfaces " + surfOk + "/" + surf + " rewritten identically (" + surfOther + " of another vertex format), models " + modOk + "/" + mod + " (" + modOther +
                    " skinned), kind-22 collis " + colOk + "/" + col + " (" + colOther + " other kinds), textures " + texOk + "/" + tex + " rebuilt identically, materials " + matOk + "/" + mat + " read, shaders " + shOk + "/" + sh + " read");
            }

            // ---- the package read whole and written back with stored resources
            if (original)
            {
                byte[] database = lib.ReadLevelFile(fd, "database"), package = File.ReadAllBytes(Path.Combine(lib.Root, "Database", fd + ".dx9.package"));
                UxTest.SameMd5(label + ": " + fd + ".dx9.package", package, UxStock.StockPin(fd, "package"));
                var whole = new UxTrackSet(fd, database, package);
                var repl = new Dictionary<(string, string), byte[]>();
                int compressed = 0;
                for (int i = 0; i < whole.assets.Count; i++)
                {
                    var a = whole.assets[i];
                    if (!UxBin.Same(whole.ReadLogical(a), logical[i])) UxTest.Fail(a + ": whole-package read differs from the range read");
                    if (a.compressed) { repl[a.Key] = UxPkg.EncodeBlob(logical[i]); compressed++; }
                }
                // rebuild() without replacements must give the original package and database back
                var same = new UxTrackSet(fd, database, (byte[])package.Clone());
                byte[] again = same.Rebuild(null);
                UxTest.SameBytes(label + ": rebuild() without changes, package", again, package);
                UxTest.SameBytes(label + ": rebuild() without changes, database", same.db.ToBytes(), database);
                byte[] stored = whole.Rebuild(repl);
                byte[] newDb = whole.db.ToBytes();
                var back = new UxTrackSet(fd, newDb, stored);
                int diff = 0, rfx = 0;
                for (int i = 0; i < back.assets.Count; i++)
                {
                    var a = back.assets[i];
                    if (!UxBin.Same(back.ReadLogical(a), logical[i])) diff++;
                    if (a.compressed && !UxBin.Same(UxPkg.RfxLzx(back.Blob(a)), logical[i])) rfx++;      // the client's reader
                    if (a.compressed && !UxBin.Same(UxBin.PySlice(logical[i], 0, 64), back.ReadHead(a, 64))) diff++;
                }
                UxTest.Report(diff == 0 && back.assets.Count == whole.assets.Count, label + ": " + fd + " written back with stored resources (" + compressed + " of " + back.assets.Count +
                    " re-encoded, package " + package.Length + " -> " + stored.Length + " B): " + diff + " assets decode differently");
                UxTest.Report(rfx == 0, label + ": the client's resource reader (RfxData.Lzx copy) on the stored resources: " + rfx + " differ");
                File.WriteAllBytes(Path.Combine(s_scratch, fd + ".stored.dx9.package"), stored);
                File.WriteAllBytes(Path.Combine(s_scratch, fd + ".stored.dx9.database"), newDb);
                // the blobs and their logical bytes for the C core's reader (core_stored_test.c: fmt_xmem_resource)
                using (var w = new BinaryWriter(File.Create(Path.Combine(s_scratch, "stored_blobs.bin"))))
                    for (int i = 0; i < back.assets.Count; i++)
                    {
                        var a = back.assets[i]; if (!a.compressed) continue;
                        var b = back.Blob(a);
                        w.Write(b.Length); w.Write(b); w.Write(logical[i].Length); w.Write(logical[i]);
                    }
                // rebuild_with_additions: a new asset appended, the database re-encoded through the tree
                var plus = new UxTrackSet(fd, database, (byte[])package.Clone());
                byte[] extra = Encoding.ASCII.GetBytes("untamed converter test asset");
                var (db2, pk2) = plus.RebuildWithAdditions(null, new List<(string, string, byte[], bool)> { ("UxTest_Added", "bxml", UxPkg.EncodeBlob(extra), true), ("UxTest_Raw", "bxml", extra, false) });
                var plus2 = new UxTrackSet(fd, db2, pk2);
                UxTest.Report(plus2.assets.Count == whole.assets.Count + 2 && UxBin.Same(plus2.ReadLogical("UxTest_Added", "bxml"), extra) && UxBin.Same(plus2.ReadLogical("UxTest_Raw", "bxml"), extra) &&
                    UxBin.Same(plus2.ReadLogical(plus2.assets[0]), logical[0]), label + ": rebuild_with_additions adds two assets and keeps the rest readable");
            }
            else
            {
                // an extracted library has no stored blobs: Blob() re-encodes, and that must decode to the same bytes
                int diff = 0;
                for (int i = 0; i < ts.assets.Count; i += 9)
                {
                    var a = ts.assets[i]; var b = ts.Blob(a);
                    if (!UxBin.Same(a.compressed ? UxPkg.Decode(b) : b, logical[i])) diff++;
                }
                UxTest.Report(diff == 0, label + ": re-encoded blobs of the extracted base decode to the asset bytes");
            }
        }
    }
}
