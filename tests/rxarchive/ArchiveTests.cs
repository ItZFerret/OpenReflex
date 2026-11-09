// TEST ONLY (tests/rxarchive/build.sh): the client's archive readers and packer against a real librxcore (see build.sh for what each part checks).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using Reflex.Port.Content;
using Reflex.Port.RxCore;

static class ArchiveTests
{
    static int failures;
    static void Fail(string what) { failures++; Console.WriteLine("FAIL " + what); }

    static int Main(string[] args)
    {
        if (args.Length >= 4 && args[1] == "alive") return AliveCheck(args[2], args[3]);
        if (args.Length >= 6 && args[1] == "alivereimport") { RxNativeLoader.Load(args[2]); RxFiles.Log = s => Console.WriteLine(s); AliveReimport(args[0], args[3], args[4], args[5]); Console.WriteLine(failures == 0 ? "REIMPORT PASS" : "REIMPORT FAIL (" + failures + ")"); return failures; }
        if (args.Length >= 4 && args[1] == "alivepack")
        {   // <work> alivepack <core library> <library copy>: its loose Alive levels (a re-import) packed as GAME LIBRARY does, every file read back
            RxNativeLoader.Load(args[2]); RxFiles.Log = s => { if (!s.StartsWith("[RxFiles] released")) Console.WriteLine(s); };
            AlivePack(args[3]);
            Console.WriteLine(failures == 0 ? "ALIVEPACK PASS" : "ALIVEPACK FAIL (" + failures + ")"); return failures == 0 ? 0 : 1;
        }
        if (args.Length >= 5 && args[1] == "packimport")
        {   // C and D alone (no loose library needed): <work> packimport <core library> <packed library> <original install>
            RxNativeLoader.Load(args[2]); RxFiles.Log = s => Console.WriteLine(s);
            Packer(args[0], args[3]); Import(args[0], args[4]);
            Console.WriteLine(failures == 0 ? "PACKIMPORT PASS" : "PACKIMPORT FAIL (" + failures + ")"); return failures == 0 ? 0 : 1;
        }
        if (args.Length < 6) { Console.WriteLine("usage: ArchiveTests <work> <core library> <loose library> <packed library> <mods folder> <packed mods folder> [original install]"); return 2; }
        string work = args[0], core = args[1], loose = args[2], packed = args[3], mods = args[4], packedMods = args[5];
        RxNativeLoader.Load(core);
        RxFiles.Log = s => Console.WriteLine(s);
        Library(loose, packed);
        Mods(mods, packedMods);
        Packer(work, packed);
        if (args.Length > 6) Import(work, args[6]);
        if (Environment.GetEnvironmentVariable("ARCHIVE_REPACK") == "1") Repack(packed);
        Console.WriteLine(failures == 0 ? "RXARCHIVE PASS" : "RXARCHIVE FAIL (" + failures + ")");
        return failures == 0 ? 0 : 1;
    }

    /// What AliveLevels.Read and the Alive importer see in a library: each registry level's database, manifest and sidecars, loose or packed.
    static int AliveCheck(string core, string root)
    {
        RxNativeLoader.Load(core);
        RxFiles.Log = s => Console.WriteLine(s);
        string db = Path.Combine(root, "Database");
        var reg = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(db, "alive-levels.json"))).RootElement.GetProperty("levels");
        int bad = 0;
        foreach (var e in reg.EnumerateArray())
        {
            string l = e.GetProperty("level").GetString();
            bool dbx = RxFiles.Exists(Path.Combine(db, l + ".dx9.database")), lvl = RxFiles.Exists(Path.Combine(db, l + ".dx9.level")), man = RxFiles.Exists(Path.Combine(db, "alive-import-" + l + ".json"));
            string side = string.Join(" ", e.EnumerateObject().Where(p => p.Value.ValueKind == System.Text.Json.JsonValueKind.String && p.Value.GetString().StartsWith("alive-")).Select(p => p.Name + (RxFiles.Exists(Path.Combine(db, p.Value.GetString())) ? "" : "!MISSING")));
            if (!dbx || !lvl) bad++;
            Console.WriteLine($"ALIVE {l}: database {dbx} level {lvl} manifest {man} (loose {File.Exists(Path.Combine(db, "alive-import-" + l + ".json"))}) {side}");
        }
        return bad;
    }

    // ------------------------------------------------------------------------------------------------ A: the library

    // (two libraries of the same import: the Alive levels only when both hold the same Alive import - ARCHIVE_ALIVE=1)
    static readonly string[] Databases = Environment.GetEnvironmentVariable("ARCHIVE_ALIVE") == "1"
        ? new[] { "NAT_Concord", "SX_Kingston", "FR_Alpine", "NAT_Farm", "FR_Dunes", "SX_DLC_SX01", "Rider", "MXs", "EngineDependencies", "MXTables_Default" }
        : new[] { "NAT_Concord", "SX_Kingston", "FR_Alpine", "NAT_Bedford", "SX_Manchester", "TRX_Verde", "Rider", "MXs", "EngineDependencies", "MXTables_Default" };

    static void Library(string loose, string packed)
    {
        var L = new Rfx.Install(Path.Combine(loose, "Database"));
        var P = new Rfx.Install(Path.Combine(packed, "Database"));
        var lf = RxFiles.GetFiles(Path.Combine(loose, "Database"), "*.dx9.database").Select(Path.GetFileName).OrderBy(x => x, StringComparer.Ordinal).ToList();
        var open = Stopwatch.StartNew();
        var pf = RxFiles.GetFiles(Path.Combine(packed, "Database"), "*.dx9.database").Select(Path.GetFileName).OrderBy(x => x, StringComparer.Ordinal).ToList();
        Console.WriteLine($"A  the packed library's archives opened and listed in {open.Elapsed.TotalMilliseconds:0} ms");
        if (!lf.SequenceEqual(pf)) Fail($"Database listing: {lf.Count} loose, {pf.Count} packed");
        else Console.WriteLine($"A  Database listing: {pf.Count} databases, the same loose and packed ({Directory.GetFiles(Path.Combine(packed, "Database"), "*.dx9.database").Length} of them loose in the packed library)");
        long bytes = 0; int assets = 0; double tl = 0, tp = 0;
        foreach (string name in Databases)
        {
            if (!L.Has(name)) { Console.WriteLine("A  (no database " + name + " in this library)"); continue; }
            if (!P.Has(name)) { Fail(name + ": not in the packed library"); continue; }
            var sw = Stopwatch.StartNew();
            var ld = L.Db(name); var pd = P.Db(name);
            if (ld.packages.Count != pd.packages.Count) { Fail(name + ": packages differ"); continue; }
            int n = 0; long b = 0; double dl = 0, dp = 0;
            foreach (var pkg in ld.packages.Values)
                foreach (var a in pkg.assets)
                {
                    var t = Stopwatch.StartNew(); byte[] x = L.Read(a); dl += t.Elapsed.TotalMilliseconds;
                    var pa = pd.Find(pkg.name).Find(a.name, a.type);
                    t.Restart(); byte[] y = P.Read(pa); dp += t.Elapsed.TotalMilliseconds;
                    if (!x.AsSpan().SequenceEqual(y)) Fail($"{name}/{pkg.name}/{a.name}.{a.type} differs");
                    n++; b += x.Length;
                }
            foreach (string ext in new[] { ".dx9.level", ".dx9.scene" })
            {
                string lp = Path.Combine(loose, "Database", name + ext), pp = Path.Combine(packed, "Database", name + ext);
                if (!RxFiles.Exists(lp)) continue;
                if (!RxFiles.ReadAllBytes(lp).AsSpan().SequenceEqual(RxFiles.ReadAllBytes(pp))) Fail(name + ext + " differs");
            }
            Console.WriteLine($"A  {name}: {n} assets, {b / 1048576.0:0.0} MB identical; read loose {dl / 1000:0.00} s, packed {dp / 1000:0.00} s{(RxFiles.Packed(Path.Combine(packed, "Database", name + ".dx9.database")) ? " (level packed in Tracks/)" : "")}");
            assets += n; bytes += b; tl += dl; tp += dp;
        }
        Console.WriteLine($"A  total {assets} assets, {bytes / 1048576.0:0.0} MB: loose {tl / 1000:0.00} s, packed {tp / 1000:0.00} s");
    }

    // ------------------------------------------------------------------------------------------------ B: the mods

    static string Sha(Stream s) { using (s) using (var h = SHA256.Create()) return string.Concat(h.ComputeHash(s).Select(b => b.ToString("x2"))); }

    static void Mods(string mods, string packedMods)
    {
        foreach (string file in Directory.GetFiles(packedMods, "*.rxtrack").OrderBy(x => x, StringComparer.Ordinal))
        {
            string folder = Path.Combine(mods, Path.GetFileNameWithoutExtension(file));
            if (!Directory.Exists(folder)) continue;
            var inside = RxFiles.PathsOf(file).Where(p => p.StartsWith("Database/", StringComparison.Ordinal)).ToList();
            string level = Path.GetFileName(inside.First(p => p.EndsWith(".dx9.level", StringComparison.Ordinal)));
            level = level.Substring(0, level.Length - ".dx9.level".Length);
            var lfiles = Directory.GetFiles(folder).Where(f => Path.GetFileName(f).StartsWith(level + ".", StringComparison.OrdinalIgnoreCase)).ToDictionary(f => Path.GetFileName(f).ToLowerInvariant(), f => f);
            var pfiles = inside.ToDictionary(p => Path.GetFileName(p).ToLowerInvariant(), p => Path.Combine(file, p.Replace('/', Path.DirectorySeparatorChar)));
            const string ns = "modtrack:test>";
            var L = new Rfx.Install(folder, ns, lfiles);
            var P = new Rfx.Install(Path.Combine(file, "Database"), ns, pfiles);
            var ld = L.Db(ns + level); var pd = P.Db(ns + level);
            if (ld == null || pd == null) { Fail(Path.GetFileName(file) + ": database does not read (" + (ld == null ? "loose" : "packed") + ")"); continue; }
            int n = 0, bad = 0; long b = 0; double dl = 0, dp = 0;
            foreach (var pkg in ld.packages.Values)
                foreach (var a in pkg.assets)
                {
                    // a community track's damaged asset fails the same way from both (the game skips what does not read)
                    byte[] x = null, y = null; string ex = null, ey = null;
                    var t = Stopwatch.StartNew();
                    try { x = L.Read(a); } catch (InvalidDataException e) { ex = e.Message; }
                    dl += t.Elapsed.TotalMilliseconds; t.Restart();
                    try { y = P.Read(pd.Find(pkg.name).Find(a.name, a.type)); } catch (InvalidDataException e) { ey = e.Message; }
                    dp += t.Elapsed.TotalMilliseconds;
                    if (ex != null || ey != null) { if (ex != ey) Fail($"{Path.GetFileName(file)}: {a.name}.{a.type}: loose '{ex}', packed '{ey}'"); else bad++; continue; }
                    if (!x.AsSpan().SequenceEqual(y)) Fail($"{Path.GetFileName(file)}: {a.name}.{a.type} differs");
                    n++; b += x.Length;
                }
            bool same = true;
            foreach (var kv in lfiles)
                if (!pfiles.TryGetValue(kv.Key, out var pp) || RxFiles.Length(pp) != new FileInfo(kv.Value).Length || Sha(RxFiles.OpenRead(pp)) != Sha(File.OpenRead(kv.Value))) { same = false; Fail(Path.GetFileName(file) + ": " + kv.Key + " SHA-256 differs"); }
            Console.WriteLine($"B  {Path.GetFileName(file)} ({level}): {n} assets, {b / 1048576.0:0.0} MB identical{(bad > 0 ? ", " + bad + " damaged in the mod itself (the same error both ways)" : "")} (LZX decoded: loose {dl / 1000:0.00} s, packed {dp / 1000:0.00} s){(same ? "; the four files' SHA-256 the same" : "")}");
        }
    }

    // ------------------------------------------------------------------------------------------------ C: the client packer

    static void Copy(string from, string to, byte[] bytes = null)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(to));
        if (bytes != null) File.WriteAllBytes(to, bytes); else File.Copy(from, to, true);
    }

    /// A small library made from a packed one's files (two levels, an Alive one, shared assets: read through RxFiles), loose; then packed.
    static void Packer(string work, string packed)
    {
        string mini = Path.Combine(work, "mini");
        if (Directory.Exists(mini)) Directory.Delete(mini, true);
        Directory.CreateDirectory(Path.Combine(mini, "Database"));
        var originals = new Dictionary<string, string>(StringComparer.Ordinal);   // relative path -> where its bytes are read
        void Take(string rel, string from) { var b = RxFiles.ReadAllBytes(from); Copy(null, Path.Combine(mini, rel), b); originals[rel] = from; }
        File.WriteAllText(Path.Combine(mini, ReflexAssetImport.ExtractedMarker), "OPEN REFLEX extracted assets v1\n");
        Take("Database/alive-levels.json", Path.Combine(packed, "Database", "alive-levels.json"));
        var levels = new[] { "SX_Fresno", "FR_Quarry" };
        var shared = new[] { "EngineDependencies", "AIData" };
        foreach (string l in levels)
        {
            string arch = Path.Combine(packed, "Tracks", l + ".rxtrack");
            foreach (string p in RxFiles.PathsOf(arch)) Take(p, Path.Combine(packed, p));   // (through the root: a deduped level's pool references resolve there)
        }
        string data = Path.Combine(packed, "Assets.rxdata");
        foreach (string p in RxFiles.PathsOf(data))
            if (shared.Any(d => p.Split('/').Length > 2 && p.Split('/')[2] == d)) Take(p, Path.Combine(packed, p));
        foreach (string d in shared) Take("Database/" + d + ".dx9.database", Path.Combine(packed, "Database", d + ".dx9.database"));
        Console.WriteLine($"C  a small loose library: {originals.Count} files ({string.Join(", ", levels)} + shared {string.Join(", ", shared)})");
        var sw = Stopwatch.StartNew();
        var r = ReflexDataPack.Pack(mini, null, CancellationToken.None, 9, true, 4).Result;
        Console.WriteLine($"C  packed {r.levels} levels, {r.files} files: {r.looseBytes / 1048576.0:0.0} MB -> {r.packedBytes / 1048576.0:0.0} MB in {sw.Elapsed.TotalSeconds:0.0} s; archives {string.Join(", ", r.archives.Select(Path.GetFileName))}; loose removed {r.removed}");
        if (r.archives.Count != levels.Length + 1) Fail("expected " + (levels.Length + 1) + " archives");
        // the shared databases' index files and the registry stay loose (only levels and the extracted assets are packed)
        var keptLoose = shared.Select(d => "Database/" + d + ".dx9.database").Append("Database/alive-levels.json").ToList();
        int left = originals.Keys.Count(o => File.Exists(Path.Combine(mini, o)) && !keptLoose.Contains(o));
        if (left != 0) Fail(left + " loose copies left");
        if (keptLoose.Any(k => !File.Exists(Path.Combine(mini, k)))) Fail("a shared database index was removed");
        int same = 0;
        foreach (var kv in originals)
        {
            if (RxFiles.ReadAllBytes(kv.Value).AsSpan().SequenceEqual(RxFiles.ReadAllBytes(Path.Combine(mini, kv.Key)))) same++; else Fail("packed " + kv.Key + " differs");
        }
        Console.WriteLine($"C  {same} / {originals.Count} files read back identical through RxFiles");
        string meta = RxFiles.MetaOf(Path.Combine(mini, "Tracks", "FR_Quarry.rxtrack"));
        if (meta.IndexOf("\"alive\":{", StringComparison.Ordinal) < 0 || meta.IndexOf("\"source\":\"alive\"", StringComparison.Ordinal) < 0) Fail("FR_Quarry's archive has no Alive registry entry");
        else Console.WriteLine("C  FR_Quarry.rxtrack carries its Alive registry entry");
        var again = ReflexDataPack.Pack(mini, null, CancellationToken.None).Result;
        if (again.archives.Count != 0) Fail("a second pack wrote archives again"); else Console.WriteLine("C  a second PACK GAME FILES finds nothing to pack");
        var st = ReflexDataPack.Measure(mini);
        if (!st.Packed) Fail("Measure does not call the packed library packed"); else Console.WriteLine($"C  library state: packed, {st.archives} archives, {st.totalBytes / 1048576.0:0.0} MB");
        Handles(mini, originals);
    }

    /// The archive handles RxFiles keeps (Windows can't rename a folder with an open file inside: the import's publish, 2026-11-08):
    /// open while the library is read, closed by RxFiles.Release (a stream of a released archive fails, a read after it opens again), and
    /// the folder moved and read back from its new place.
    static void Handles(string mini, Dictionary<string, string> originals)
    {
        var stream = RxFiles.OpenRead(Path.Combine(mini, originals.Keys.First(k => k.StartsWith("Assets/", StringComparison.Ordinal))));
        int open = OpenUnder(mini);
        if (open == 0) Fail("no archive handle open under the library after reading it (the check can't see them)");
        RxFiles.Release(mini);
        int after = OpenUnder(mini);
        if (after != 0) Fail(after + " archive handles still open under the library after RxFiles.Release");
        else Console.WriteLine($"C  {open} archive handles open after the reads, 0 after RxFiles.Release");
        try { stream.ReadByte(); Fail("a stream of a released archive still reads"); } catch (IOException) { Console.WriteLine("C  a stream of a released archive fails cleanly (IOException)"); }
        string moved = mini + "-moved";
        if (Directory.Exists(moved)) Directory.Delete(moved, true);
        Directory.Move(mini, moved);
        int same = originals.Count(kv => RxFiles.ReadAllBytes(kv.Value).AsSpan().SequenceEqual(RxFiles.ReadAllBytes(Path.Combine(moved, kv.Key))));
        if (same != originals.Count) Fail((originals.Count - same) + " files differ after the move"); else Console.WriteLine($"C  moved; {same} / {originals.Count} files read back from the new place");
        RxFiles.Release(moved);
        Directory.Move(moved, mini);
    }

    /// A library whose Alive levels were re-imported (loose): every loose file of the library hashed, the library packed (ReflexDataPack, as
    /// GAME LIBRARY > PACK GAME FILES), then every one of those files read back through RxFiles, and what the Alive readers need found.
    static void AlivePack(string root)
    {
        string db = Path.Combine(root, "Database");
        var loose = Directory.GetFiles(root, "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(root, f).Replace('\\', '/'))
                             .Where(r => !r.StartsWith("Tracks/", StringComparison.Ordinal) && !r.EndsWith(".rxdata", StringComparison.Ordinal) && r != ReflexDataPack.Marker).ToList();   // (the pack log grows)
        var before = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (string r in loose) using (var h = SHA256.Create()) before[r] = Convert.ToHexString(h.ComputeHash(File.ReadAllBytes(Path.Combine(root, r))));
        var levels = Directory.GetFiles(db, "alive-import-*.json").Select(f => Path.GetFileNameWithoutExtension(f).Substring("alive-import-".Length)).ToList();
        Console.WriteLine($"F  {loose.Count} loose files, Alive levels loose: {string.Join(", ", levels)}");
        var r0 = ReflexDataPack.Pack(root, null, CancellationToken.None, 9, true, 4).Result;
        Console.WriteLine($"F  packed {r0.levels} levels, {r0.files} files: {r0.looseBytes >> 20} MB -> {r0.packedBytes >> 20} MB; archives {string.Join(", ", r0.archives.Select(Path.GetFileName))}");
        RxFiles.Reset();
        int same = 0, gone = 0, diff = 0;
        foreach (var kv in before)
        {
            string full = Path.Combine(root, kv.Key);
            if (!RxFiles.Exists(full)) { gone++; Fail("lost: " + kv.Key); continue; }
            string now; using (var h = SHA256.Create()) now = Convert.ToHexString(h.ComputeHash(RxFiles.ReadAllBytes(full)));
            if (now == kv.Value) same++; else { diff++; Fail("changed: " + kv.Key); }
        }
        Console.WriteLine($"F  {same} / {before.Count} files read back identical ({gone} lost, {diff} changed); still loose: {before.Keys.Count(k => File.Exists(Path.Combine(root, k)))}");
        foreach (string l in levels)
        {   // the Alive registry entry rides in the level's archive (metadata "alive")
            string meta = RxFiles.MetaOf(Path.Combine(root, "Tracks", l + ".rxtrack"));
            if (meta.IndexOf("\"alive\":{", StringComparison.Ordinal) < 0) Fail(l + ": its archive has no Alive registry entry");
        }
    }

    /// Files this process has open in or under a folder (lsof: the path a descriptor has now, so a renamed folder shows its new name).
    static int OpenUnder(string folder)
    {
        var psi = new ProcessStartInfo("lsof", "-n -P -Fn -p " + Environment.ProcessId) { RedirectStandardOutput = true, UseShellExecute = false };
        using var pr = Process.Start(psi);
        string text = pr.StandardOutput.ReadToEnd(); pr.WaitForExit();
        string real = Real(folder);
        return text.Split('\n').Count(l => l.StartsWith("n/", StringComparison.Ordinal) && (Real(l.Substring(1)) + "/").StartsWith(real + "/", StringComparison.Ordinal));
    }
    static string Real(string p) { p = Path.GetFullPath(p); return p.StartsWith("/private/", StringComparison.Ordinal) ? p.Substring(8) : p; }

    // ------------------------------------------------------------------------------------------------ D: the import packs

    static void Import(string work, string install)
    {
        string src = Path.Combine(work, "install"), dest = Path.Combine(work, "imported");
        foreach (string d in new[] { src, dest }) if (Directory.Exists(d)) Directory.Delete(d, true);
        Directory.CreateDirectory(Path.Combine(src, "Database"));
        var names = new[] { "SX_Fresno", "EngineDependencies", "AIData" };
        foreach (string f in Directory.GetFiles(Path.Combine(install, "Database")))
            if (names.Any(n => Path.GetFileName(f).StartsWith(n + ".dx9.", StringComparison.Ordinal))) File.Copy(f, Path.Combine(src, "Database", Path.GetFileName(f)));
        foreach (string f in new[] { "Engine.bxml", "Engine.xml" }) if (File.Exists(Path.Combine(install, f))) File.Copy(Path.Combine(install, f), Path.Combine(src, f));
        int lastPhase = 0;
        var sw = Stopwatch.StartNew();
        var r = ReflexAssetImport.Import(src, dest, p => { if (p.phase == 4) lastPhase = 4; }, CancellationToken.None, new ReflexAssetImport.Options { pack = true }).Result;
        // the stage was published by a folder rename: nothing of it may still be open (Windows refuses that rename: 2026-11-08)
        int open = OpenUnder(dest);
        if (open != 0) Fail(open + " files of the import's stage still open after it was published");
        else Console.WriteLine("D  no file of the stage open when it was published");
        Console.WriteLine($"D  import of {string.Join(", ", names)}: {r.files} files in {sw.Elapsed.TotalSeconds:0.0} s; warnings: {(r.warnings.Length == 0 ? "none" : string.Join(" | ", r.warnings))}");
        if (lastPhase != 4) Fail("the import reported no packing phase");
        if (Directory.Exists(Path.Combine(dest, "Assets"))) Fail("loose assets left after the import's packing");
        if (!File.Exists(Path.Combine(dest, "Assets.rxdata")) || !File.Exists(Path.Combine(dest, "Tracks", "SX_Fresno.rxtrack"))) Fail("the import wrote no archives");
        if (File.Exists(Path.Combine(dest, "Database", "SX_Fresno.dx9.level"))) Fail("the level's loose files are still there");
        if (File.Exists(Path.Combine(dest, "asset-index.html"))) Fail("a packed library has the loose-file index page");
        var st = ReflexDataPack.Measure(dest);
        Console.WriteLine($"D  library state: packed {st.Packed}, {st.archives} archives, {st.totalBytes / 1048576.0:0.0} MB in {st.totalFiles} files, loose left {st.looseFiles}");
        if (!st.Packed) Fail("Measure does not call the imported library packed");
        var O = new Rfx.Install(Path.Combine(install, "Database"));
        var I = new Rfx.Install(Path.Combine(dest, "Database"));
        int n = 0;
        foreach (string name in names)
        {
            var od = O.Db(name); var id = I.Db(name);
            if (id == null) { Fail(name + " does not read from the packed import"); continue; }
            foreach (var pkg in od.packages.Values)
                foreach (var a in pkg.assets)
                {
                    if (!O.Read(a).AsSpan().SequenceEqual(I.Read(id.Find(pkg.name).Find(a.name, a.type)))) Fail($"{name}/{a.name}.{a.type} differs from the original install");
                    n++;
                }
        }
        Console.WriteLine($"D  {n} assets read from the packed import are the original install's (LZX decoded)");
        string report = File.ReadAllText(Path.Combine(dest, "import-report.txt"));
        if (report.IndexOf("Packed: ", StringComparison.Ordinal) < 0) Fail("the import report does not say it packed");
    }

    // ------------------------------------------------------------------------------------------------ E: dedupe a packed library in place

    static void Repack(string root)
    {
        var paths = RxFiles.PackedPaths(root);
        var before = new Dictionary<string, string>(StringComparer.Ordinal);
        var sw = Stopwatch.StartNew();
        System.Threading.Tasks.Parallel.ForEach(paths, new System.Threading.Tasks.ParallelOptions { MaxDegreeOfParallelism = 6 }, p =>
        {
            string h; using (var s = SHA256.Create()) h = string.Concat(s.ComputeHash(RxFiles.ReadAllBytes(Path.Combine(root, p))).Select(b => b.ToString("x2")));
            lock (before) before[p] = h;
        });
        long sizeBefore = Directory.GetFiles(Path.Combine(root, "Tracks")).Sum(f => new FileInfo(f).Length) + new FileInfo(Path.Combine(root, "Assets.rxdata")).Length;
        Console.WriteLine($"E  {paths.Count} files hashed before in {sw.Elapsed.TotalSeconds:0.0} s; archives {sizeBefore / 1048576.0:0.0} MB; old archives {ReflexDataPack.OldArchives(root)}");
        sw.Restart();
        var r = ReflexDataPack.Pack(root, null, CancellationToken.None).Result;
        long sizeAfter = Directory.GetFiles(Path.Combine(root, "Tracks")).Sum(f => new FileInfo(f).Length) + new FileInfo(Path.Combine(root, "Assets.rxdata")).Length;
        Console.WriteLine($"E  packed again in {sw.Elapsed.TotalSeconds:0.0} s: {r.archives.Count} archives, {sizeBefore / 1048576.0:0.0} MB -> {sizeAfter / 1048576.0:0.0} MB; stored once {(r.dedupedBytes + r.pooledBytes) / 1048576.0:0.0} MB raw (pool references {r.pooledBytes / 1048576.0:0.0} MB)");
        if (ReflexDataPack.OldArchives(root)) Fail("version 1 archives left after packing again");
        RxFiles.Reset();
        var after = RxFiles.PackedPaths(root);
        if (after.Count != paths.Count) Fail($"{after.Count} files after, {paths.Count} before");
        int same = 0;
        System.Threading.Tasks.Parallel.ForEach(after, new System.Threading.Tasks.ParallelOptions { MaxDegreeOfParallelism = 6 }, p =>
        {
            string h; using (var s = SHA256.Create()) h = string.Concat(s.ComputeHash(RxFiles.ReadAllBytes(Path.Combine(root, p))).Select(b => b.ToString("x2")));
            if (before.TryGetValue(p, out var b0) && b0 == h) Interlocked.Increment(ref same); else Fail(p + " differs after packing again");
        });
        Console.WriteLine($"E  {same} / {paths.Count} files identical after packing again");
        var again = ReflexDataPack.Pack(root, null, CancellationToken.None).Result;
        if (again.archives.Count != 0) Fail("a second pack of the deduped library wrote archives"); else Console.WriteLine("E  a second PACK GAME FILES finds nothing to pack");
    }

    // ------------------------------------------------------------------------------------------------ F: an Alive level imported again onto a packed library

    /// A small packed library (Sliderock: FR_DLC_Hills, the old import from `packed`, and the two base programs the installer copies),
    /// imported again from the Alive sources: the old import's manifest is found in its archive (not "by another installer"), the new
    /// files are loose, the old archive is gone (no leftovers under the new files), and packing again gives the same level hash as a
    /// clean import into `fresh` (null: not compared).
    /// SHA-256 over the level's own database files (Database/<level>.dx9.*, as RxFiles reads them, loose or packed), in path order.
    static string LevelHash(string root, string level)
    {
        string db = Path.Combine(root, "Database");
        using (var h = SHA256.Create())
        {
            foreach (string p in RxFiles.GetFiles(db, level + ".dx9.*").OrderBy(p => p, StringComparer.OrdinalIgnoreCase))
            {
                byte[] name = System.Text.Encoding.UTF8.GetBytes(Path.GetFileName(p).ToLowerInvariant() + "\n"), data = RxFiles.ReadAllBytes(p);
                h.TransformBlock(name, 0, name.Length, null, 0); h.TransformBlock(data, 0, data.Length, null, 0);
            }
            h.TransformFinalBlock(new byte[0], 0, 0);
            return string.Concat(h.Hash.Select(b => b.ToString("x2")));
        }
    }

    static void AliveReimport(string work, string packed, string sources, string level)
    {
        string mini = Path.Combine(work, "alive");
        if (Directory.Exists(mini)) Directory.Delete(mini, true);
        Directory.CreateDirectory(Path.Combine(mini, "Database"));
        File.WriteAllText(Path.Combine(mini, ReflexAssetImport.ExtractedMarker), "OPEN REFLEX extracted assets v1\n");
        void Take(string rel) { var b = RxFiles.ReadAllBytes(Path.Combine(packed, rel)); Copy(null, Path.Combine(mini, rel), b); }
        Take("Database/alive-levels.json");
        foreach (string p in RxFiles.PathsOf(Path.Combine(packed, "Tracks", level + ".rxtrack"))) Take(p);
        Take("Assets/shader/FR_Gibson/FR_GibsonBase/Water_Wake.shader");
        Take("Assets/shader/SX_Kingston/SX_KingstonBase/crowd.shader");
        var r = ReflexDataPack.Pack(mini, null, CancellationToken.None, 9, true, 4).Result;
        Console.WriteLine($"F  a packed library with {level}: {string.Join(", ", r.archives.Select(Path.GetFileName))}");
        RxFiles.Reset();
        string before = LevelHash(mini, level);
        using (var found = Reflex.Port.Content.AliveImport.AliveImporter.Scan(new[] { sources }, s => { }, CancellationToken.None))
        {
            var src = found.Importable.Where(l => l.level == level).ToList();
            if (src.Count == 0) { Fail(level + " is not in " + sources); return; }
            var results = Reflex.Port.Content.AliveImport.AliveImporter.Import(src, mini, new Reflex.Port.Content.AliveImport.AliveImportOptions { replace = true }, null, CancellationToken.None);
            foreach (var x in results) Console.WriteLine("F  import: " + x);
            if (!results.All(x => x.ok)) { Fail("the import over the packed level failed"); return; }
        }
        RxFiles.Reset();
        if (File.Exists(Path.Combine(mini, "Tracks", level + ".rxtrack"))) Fail("the old packed copy is still there");
        if (!File.Exists(Path.Combine(mini, "Database", level + ".dx9.database")) || !File.Exists(Path.Combine(mini, "Database", "alive-import-" + level + ".json"))) Fail("the new import isn't loose");
        string imported = LevelHash(mini, level);
        var again = ReflexDataPack.Pack(mini, null, CancellationToken.None, 9, true, 4).Result;
        RxFiles.Reset();
        string repacked = LevelHash(mini, level);
        Console.WriteLine($"F  level hash: old import {before.Substring(0, 12)}, new import {imported.Substring(0, 12)}, packed again {repacked.Substring(0, 12)} ({string.Join(", ", again.archives.Select(Path.GetFileName))})");
        if (imported != repacked) Fail("packing again changed the level hash");
        string reg = File.ReadAllText(Path.Combine(mini, "Database", "alive-levels.json"));
        string entry = reg.Substring(reg.IndexOf("\"level\": \"" + level + "\"", StringComparison.Ordinal));
        Console.WriteLine("F  registry entry now names pictures: " + (entry.Substring(0, Math.Min(entry.Length, 2000)).Contains("\"preview\"") ? "yes" : "no"));
    }
}
