// tests/untamed_convert/terrain: the terrain stage (UxTerrainStage, the port of ux_terrain.py) against the Python reference.
// For every level: level.json is seeded from the reference's work/<Level>/, the stage runs from the disc, and every file the
// reference's terrain.json lists (its 'files') is compared with the produced one:
//   hf.npy, footprint.npy, excluded_tris.npy, parts/*, textures/*.texture, *.png, material_requests.json     byte for byte
//   terrain.json    as documents with exact numbers (int / float apart, +0 and -0 apart), without the fields named in Excluded
//                   (environment data of the Python tool: they cannot match by construction)
// A PNG that differs is decoded (both files) and compared as filtered scanlines and as pixels, so a difference in the deflate stream
// alone is told apart from one in the row filters or in the pixels.
// Before the levels: the PNG writer on synthetic images (ref_png.py: Pillow's own files), the PNG / .npy readers, constants of
// numpy's random generator and matplotlib's colour map. After them: one level again on a single worker thread (the same files).
// One deviation is in principle and documented (UxTerrainStage.cs, the stage's report): np.linalg.lstsq (LAPACK) leaves rounding noise of
// about 1e-18 in coefficients whose exact value is 0; Python rounds it to 0.0 or -0.0, the port's solver to its own sign. Those
// entries ("got 0.0, want -0.0" or the reverse, in a least-squares 'coef' list) are printed and counted as DEVIATION; they fail the
// run only with --strict. Everything else that differs fails.
// The reference folders also hold leftovers of older runs (files the current terrain.json does not list): counted, not compared.
// Usage: terrain.dll [--levels norton,fairview] [--jobs N] [--strict] [--mem] [--ref <dir with png/ from ref_png.py>] + UxTest.OpenEnv's
// inputs. --mem prints the live managed memory at every stage step (it forces collections: timings are then not representative).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    // terrain.json fields that hold environment data of the Python tool
    static readonly string[] Excluded = { "source.tool_md5", "source.uxterrain_md5", "files.contact_sheet.png" };

    static byte[] PngScanlines(byte[] png, out int w, out int h, out List<int> idat)
    {
        w = h = 0; idat = new List<int>();
        var z = new MemoryStream(); int p = 8;
        while (p + 12 <= png.Length)
        {
            int n = (int)UxBin.U32BE(png, p); string t = UxBin.Latin1(png, p + 4, 4);
            if (t == "IHDR") { w = (int)UxBin.U32BE(png, p + 8); h = (int)UxBin.U32BE(png, p + 12); }
            if (t == "IDAT") { z.Write(png, p + 8, n); idat.Add(n); }
            p += 12 + n;
        }
        return UxZlib.Decompress(z.ToArray());
    }
    static byte[] Unfilter(byte[] rows, int w, int h)
    {
        int n = 3 * w, stride = n + 1; var o = new byte[h * n];
        for (int y = 0; y < h; y++)
        {
            int tag = rows[y * stride];
            for (int i = 0; i < n; i++)
            {
                int x = rows[y * stride + 1 + i];
                int a = i >= 3 ? o[y * n + i - 3] : 0, b = y > 0 ? o[(y - 1) * n + i] : 0, c = i >= 3 && y > 0 ? o[(y - 1) * n + i - 3] : 0;
                int pr = 0;
                if (tag == 1) pr = a; else if (tag == 2) pr = b; else if (tag == 3) pr = (a + b) / 2;
                else if (tag == 4) { int pa = Math.Abs(b - c), pb = Math.Abs(a - c), pc = Math.Abs(a + b - 2 * c); pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
                o[y * n + i] = (byte)(x + pr);
            }
        }
        return o;
    }
    /// Why two PNG files differ: the deflate stream only, the row filters, or the pixels.
    static string PngDifference(byte[] got, byte[] want, out bool deflateOnly)
    {
        deflateOnly = false;
        byte[] a, b; int w1, h1, w2, h2; List<int> c1, c2;
        try { a = PngScanlines(got, out w1, out h1, out c1); b = PngScanlines(want, out w2, out h2, out c2); }
        catch (Exception e) { return "not decodable (" + e.Message + ")"; }
        if (w1 != w2 || h1 != h2) return "size " + w1 + " x " + h1 + ", want " + w2 + " x " + h2;
        if (UxBin.Same(a, b)) { deflateOnly = true; return "pixels and row filters identical, the deflate stream differs (" + got.Length + " vs " + want.Length + " B; " + c1.Count + " vs " + c2.Count + " IDAT; " + UxTest.Difference(got, want) + ")"; }
        byte[] pa = Unfilter(a, w1, h1), pb = Unfilter(b, w2, h2); long nd = 0; int maxd = 0;
        for (int i = 0; i < pa.Length; i++) if (pa[i] != pb[i]) { nd++; maxd = Math.Max(maxd, Math.Abs(pa[i] - pb[i])); }
        return nd + " of " + pa.Length + " channel values differ (largest step " + maxd + ")" + (nd == 0 ? "; only the row filters differ" : "");
    }
    static void Remove(JObj doc, string path)
    {
        // the first segment is the object, the rest the key (a key may hold dots: "files.contact_sheet.png")
        int dot = path.IndexOf('.'); string head = path.Substring(0, dot), key = path.Substring(dot + 1);
        if (doc.Get(head) is JObj o) o.items.RemoveAll(kv => kv.Key == key);
    }
    static bool ZeroSignOfLstsq(string line) =>
        line.Contains(".coef[") && (line.EndsWith(": got 0.0 (float), want -0.0 (float)", StringComparison.Ordinal) || line.EndsWith(": got -0.0 (float), want 0.0 (float)", StringComparison.Ordinal));

    static int Main(string[] args)
    {
        using (var env = UxTest.OpenEnv(args))
        {
            bool strict = UxTest.Flag(args, "strict");
            // ---- the numpy / matplotlib pieces with a known reference value
            UxTest.Section("constants");
            { var lut = UxImaging.TerrainLut(); var b = new byte[lut.Length * 8]; Buffer.BlockCopy(lut, 0, b, 0, b.Length); UxTest.Check(UxBin.Md5(b) == "096a71175f0ae4ee826f05eeee4af6ea", "matplotlib 'terrain' lookup table"); }
            { var g = new UxImaging.Pcg64(2); ulong a = g.NextUInt64(), b2 = g.NextUInt64(), c = g.NextUInt64(); UxTest.Check(a == 0x42f90348d66b58c1UL && b2 == 0x4c69ea631bfb7164UL && c == 0xd071191f69efb11eUL, "np.random.default_rng(2) raw stream"); }
            { var g = new UxImaging.Pcg64(2); UxTest.Check(g.NextDouble() == 0.2616121342493164 && g.NextDouble() == 0.2984911434141233, "np.random.default_rng(2).random()"); }
            UxTest.Check(UxImaging.PyFixed(0.125, 2) == "0.12" && UxImaging.PyFixed(2.675, 2) == "2.67" && UxImaging.PyFixed(-0.001, 2) == "-0.00" && UxImaging.PyFixed(1.005, 3) == "1.005" && UxImaging.PyFixed(12.5, 0) == "12", "'%.nf' formatting");

            // ---- the PNG writer (Pillow's row filters + zlib-ng's deflate, fed as Pillow feeds it) on synthetic images
            UxTest.Section("PNG writer against Pillow (ref_png.py)");
            string refDirPng = Path.Combine(UxTest.Arg(args, "ref", "UX_REF", Path.Combine(env.scratch, "ref")), "png");
            if (!Directory.Exists(refDirPng)) UxTest.Skip("no reference PNG files in " + refDirPng + " (ref_png.py writes them)");
            else
            {
                var raws = Directory.GetFiles(refDirPng, "*.rgb"); Array.Sort(raws, StringComparer.Ordinal);
                long bytes = 0; int ok = 0;
                foreach (string f in raws)
                {
                    string name = Path.GetFileNameWithoutExtension(f); int us = name.LastIndexOf('_'), xx = name.LastIndexOf('x');
                    int w = int.Parse(name.Substring(us + 1, xx - us - 1)), h = int.Parse(name.Substring(xx + 1));
                    byte[] rgb = File.ReadAllBytes(f);
                    byte[] got = UxImaging.PngRgb(rgb, w, h), want = File.ReadAllBytes(Path.Combine(refDirPng, name + ".png"));
                    bytes += want.Length;
                    if (UxBin.Same(got, want)) { ok++; UxTest.Checks++; }
                    else UxTest.Fail("PNG " + name + ": " + PngDifference(got, want, out _));
                    var back = UxImaging.ReadPngRgb(want);                                      // the reader the later stages use
                    UxTest.Check(back.w == w && back.h == h && back.c == 3 && UxBin.Same(back.data, rgb), "PNG " + name + ": ReadPngRgb gives the image's pixels");
                }
                UxTest.Log("ok   " + ok + " of " + raws.Length + " synthetic images encode to Pillow's bytes (" + bytes + " B)");
            }

            if (env.disc == null) { UxTest.Skip("the disc is needed (UX_DISC_FOLDER / UX_DISC_ISO)"); return UxTest.Result("terrain"); }
            string only = UxTest.Arg(args, "levels"); int jobs = int.Parse(UxTest.Arg(args, "jobs", null, "0"));
            var table = new List<string>(); double totalSec = 0, maxSec = 0; int nLevels = 0, deviations = 0, nStale = 0;
            foreach (var lv in UxCommon.Levels)
            {
                if (only != null && Array.IndexOf(only.Split(','), lv.shortName) < 0) continue;
                UxTest.Section(lv.disc);
                var ctx = env.Level(lv, "terrain");
                ctx.log = s => { };
                env.Seed(ctx, "level.json");
                var sw = Stopwatch.StartNew();
                try { UxTerrainStage.Convert(ctx, new UxTerrainOptions { jobs = jobs }, UxTest.Flag(args, "mem") ? (Action<string, double>)((what, f) => UxTest.Log("     " + what + ": " + (GC.GetTotalMemory(true) >> 20) + " MB live, peak working set " + (Process.GetCurrentProcess().PeakWorkingSet64 >> 20) + " MB")) : null); }
                catch (Exception e) { UxTest.Fail(lv.disc + ": the stage threw " + e); continue; }
                double sec = sw.Elapsed.TotalSeconds; totalSec += sec; maxSec = Math.Max(maxSec, sec); nLevels++;
                string mine = Path.Combine(ctx.work.Root, "terrain"), refDir = Path.Combine(env.PyWork(lv), "terrain");
                int nRef = 0, same = 0, differ = 0;
                var refDoc = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(refDir, "terrain.json"))));
                var listed = new HashSet<string>(StringComparer.OrdinalIgnoreCase); foreach (var kv in UxJson.Obj(refDoc.Get("files")).items) listed.Add(kv.Key);
                var files = Directory.GetFiles(refDir, "*", SearchOption.AllDirectories); Array.Sort(files, StringComparer.OrdinalIgnoreCase);
                int stale = 0;
                foreach (string f in files)
                {
                    string rel = f.Substring(refDir.Length + 1).Replace('\\', '/');
                    if (rel == "contact_sheet.png" || rel == "terrain.json") continue;           // not ported / compared below
                    if (!listed.Contains(rel)) { stale++; continue; }
                    nRef++;
                    string m = Path.Combine(mine, rel.Replace('/', Path.DirectorySeparatorChar));
                    if (!File.Exists(m)) { differ++; UxTest.Fail(lv.shortName + " " + rel + ": not produced"); continue; }
                    byte[] got = File.ReadAllBytes(m), want = File.ReadAllBytes(f);
                    if (UxBin.Same(got, want)) { same++; UxTest.Checks++; continue; }
                    differ++;
                    if (rel.EndsWith(".png", StringComparison.Ordinal)) UxTest.Fail(lv.shortName + " " + rel + ": " + PngDifference(got, want, out _));
                    else if (rel.EndsWith(".json", StringComparison.Ordinal)) UxTest.SameJsonFile(lv.shortName + " " + rel, got, want);
                    else UxTest.Fail(lv.shortName + " " + rel + ": " + UxTest.Difference(got, want));
                }
                foreach (string name in listed)
                    if (name != "contact_sheet.png" && !File.Exists(Path.Combine(refDir, name.Replace('/', Path.DirectorySeparatorChar)))) UxTest.Fail(lv.shortName + " " + name + ": listed by the reference's terrain.json but not in its folder");
                // terrain.json
                nRef++;
                string tj = Path.Combine(mine, "terrain.json"); string jsonState;
                if (!File.Exists(tj)) { differ++; jsonState = "missing"; UxTest.Fail(lv.shortName + " terrain.json: not produced"); }
                else
                {
                    var got = UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(tj))); var want = refDoc;
                    foreach (string path in Excluded) { Remove(got, path); Remove(want, path); }
                    var d = UxTest.JsonDifferences(got, want, false, 2000);
                    var other = new List<string>(); var zero = new List<string>();
                    foreach (string line in d) (ZeroSignOfLstsq(line) ? zero : other).Add(line);
                    deviations += zero.Count;
                    foreach (string line in zero) UxTest.Log("DEVIATION " + lv.shortName + " terrain.json " + line);
                    if (other.Count == 0 && zero.Count == 0) { same++; UxTest.Checks++; jsonState = "equal"; }
                    else if (other.Count == 0)
                    {
                        jsonState = "equal but for " + zero.Count + " signs of zero (least squares)";
                        if (strict) { differ++; UxTest.Fail(lv.shortName + " terrain.json: " + jsonState + " (--strict)"); } else { same++; UxTest.Checks++; }
                    }
                    else
                    {
                        differ++; jsonState = other.Count + " differences" + (zero.Count > 0 ? " + " + zero.Count + " signs of zero" : "");
                        UxTest.Fail(lv.shortName + " terrain.json: " + jsonState);
                        for (int i = 0; i < other.Count && i < UxTest.MaxReports; i++) UxTest.Log("     " + other[i]);
                    }
                }
                {   // np.load of hf.npy, as the later stages read it
                    float[] hf = UxImaging.ReadNpyFloat32(File.ReadAllBytes(Path.Combine(refDir, "hf.npy")), out int hh, out int hw);
                    UxTest.Check(hh == 1024 && hw == 1024 && UxBin.Same(UxImaging.NpyFloat32(hf, hh, hw), File.ReadAllBytes(Path.Combine(refDir, "hf.npy"))), lv.shortName + " hf.npy: ReadNpyFloat32 round trip");
                }
                int extra = 0;
                foreach (string f in Directory.GetFiles(mine, "*", SearchOption.AllDirectories)) { string rel = f.Substring(mine.Length + 1).Replace('\\', '/'); if (rel != "terrain.json" && !listed.Contains(rel)) extra++; }
                if (extra > 0) UxTest.Fail(lv.shortName + ": " + extra + " produced files the reference does not list");
                nStale += stale;
                string row = string.Format(System.Globalization.CultureInfo.InvariantCulture, "{0,-12} {1,3} files compared: {2,3} identical, {3} different; terrain.json {4}; {5:0.0} s{6}",
                    lv.shortName, nRef, same, differ, jsonState, sec, stale > 0 ? "; " + stale + " leftover reference files not compared" : "");
                table.Add(row); UxTest.Log(row);
                ctx.disc.ClearCache();
            }

            // ---- the output does not depend on the number of worker threads
            if (nLevels > 0 && (only == null || Array.IndexOf(only.Split(','), "wilford") >= 0))
            {
                UxTest.Section("one worker thread gives the same files (Wilford)");
                var lv = UxCommon.Level("wilford");
                var ctx = env.Level(lv, "terrain-serial"); ctx.log = s => { };
                env.Seed(ctx, "level.json");
                var sw = Stopwatch.StartNew();
                UxTerrainStage.Convert(ctx, new UxTerrainOptions { jobs = 1 });
                double sec = sw.Elapsed.TotalSeconds;
                string a = Path.Combine(env.scratch, "terrain", lv.disc, "terrain"), b = Path.Combine(ctx.work.Root, "terrain");
                int n = 0, bad = 0;
                foreach (string f in Directory.GetFiles(a, "*", SearchOption.AllDirectories))
                {
                    n++; string other = Path.Combine(b, f.Substring(a.Length + 1));
                    if (!File.Exists(other) || !UxBin.Same(File.ReadAllBytes(f), File.ReadAllBytes(other))) { bad++; UxTest.Log("     differs: " + f.Substring(a.Length + 1)); }
                }
                UxTest.Check(bad == 0 && n > 0, "jobs = 1: " + n + " files, " + bad + " differ (" + sec.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " s serial)");
                ctx.disc.ClearCache();
            }

            // ---- the options no current build uses (no reference output: the documented behaviour of ux_terrain.build only)
            if (nLevels > 0 && (only == null || Array.IndexOf(only.Split(','), "harrispark") >= 0))
            {
                UxTest.Section("options: occ_map layout, dirt tint override, no-write (HarrisPark)");
                var lv = UxCommon.Level("harrispark");
                var ctx = env.Level(lv, "terrain-options"); ctx.log = s => { };
                env.Seed(ctx, "level.json");
                JObj dry = UxTerrainStage.Convert(ctx, new UxTerrainOptions { write = false });
                UxTest.Check(!dry.Has("ground") && !dry.Has("files") && dry.Has("heightfield") && dry.Has("fidelity") && !Directory.Exists(Path.Combine(ctx.work.Root, "terrain")), "write = false: the first half of the manifest, nothing written");
                JObj doc = UxTerrainStage.Convert(ctx, new UxTerrainOptions { occ_map = "layout", dirt_tint = new[] { 120, 100, 80 } });
                var ground = UxJson.Obj(doc.Get("ground")); bool contact = false;
                foreach (var kv in UxJson.Obj(ground.Get("texture_maps")).items) contact |= UxJson.Obj(kv.Value).Has("contact");
                var tint = UxJson.Arr(ground.Get("tint"));
                UxTest.Check(!doc.Has("options") && !contact && (string)ground.Get("tint_source") == "override" && (long)tint[0] == 120 && (long)tint[1] == 100 && (long)tint[2] == 80,
                    "occ_map layout: no contact test, no 'options' entry; the tint override is used");
                var refGround = UxJson.Obj(UxJson.Obj(UxJson.ParseUtf8(File.ReadAllBytes(Path.Combine(env.PyWork(lv), "terrain", "terrain.json")))).Get("ground"));
                // the layout choice of a texture the contact test switched is recorded by the reference as 'map_layout'
                int checkedMaps = 0, sameMaps = 0;
                foreach (var kv in UxJson.Obj(refGround.Get("texture_maps")).items)
                {
                    var r = UxJson.Obj(kv.Value); string layout = (r.Get("map_layout") ?? r.Get("map")) as string;
                    var mine = UxJson.Obj(ground.Get("texture_maps")).Get(kv.Key) as JObj;
                    checkedMaps++; if (mine != null && (string)mine.Get("map") == layout) sameMaps++;
                }
                UxTest.Check(checkedMaps > 0 && sameMaps == checkedMaps, "occ_map layout: " + sameMaps + " of " + checkedMaps + " textures take the reference's layout choice");
                bool threw = false; try { UxTerrainStage.Convert(ctx, new UxTerrainOptions { occ_map = "nearest" }); } catch (UxException) { threw = true; }
                UxTest.Check(threw, "an unknown occ_map is refused");
                ctx.disc.ClearCache();
            }

            UxTest.Section("summary");
            foreach (string r in table) UxTest.Log(r);
            if (nLevels > 0) UxTest.Log(string.Format(System.Globalization.CultureInfo.InvariantCulture, "{0} levels, {1:0.0} s per level (slowest {2:0.0} s, {6} logical processors); {3} documented deviations (signs of zero){4}; {5} leftover reference files not compared; process peak {7} MB",
                nLevels, totalSec / nLevels, maxSec, deviations, strict ? " counted as failures" : "", nStale, Environment.ProcessorCount, Process.GetCurrentProcess().PeakWorkingSet64 >> 20));
        }
        return UxTest.Result("terrain");
    }
}
