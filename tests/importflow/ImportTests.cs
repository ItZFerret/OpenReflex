// TEST ONLY (tests/importflow): the game files import's space, disk-full, mods and install-finding logic on .NET (see build.sh).
//   args: <work dir> [stock install (the folder holding Database)] [pack zips...]
// Synthetic: byte sizes, the free-space test hook, disk-full recognition (Mono's "Win32 IO returned 112", HResults 0x80070070 / 0x80070027,
// ENOSPC), the space verdict and margin, drive names, OneDrive / Program Files / path-length checks, Steam libraryfolders.vdf (Windows,
// Proton's Linux paths, the Steam Deck's microSD card), macOS bottles, the relocation list, Retry.
// With the stock install: the LZX header walk against real decoding (every asset of a sample of packages: equal sizes), Measure of the
// whole game (the real figure), a small scratch install made of clones (APFS) of a few of its databases imported with the plan (measured =
// written, exactly), a disk-full fault at N bytes and a free-space shortfall (ImportException DiskFull, nothing left behind), cancel, a
// resumed leftover stage (damaged, missing and stray files), skipped files, an unreadable database (BadSourceFile naming it).
// With pack zips: a clone of the whole stock install with the packs dropped into Database the original way, a damaged DLC table, a
// beta-slot track, a replaced stock file and a folder: the mod scan's groups, unreadable flags and Move into a scratch Mods folder.
// IMPORTFLOW_FULL=1: also the full import of that install with the plan (measured vs written; minutes), removed afterwards.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading;
using Reflex.Port.Content;

static class ImportTests
{
    static int passed, failed;
    static void Eq<T>(T got, T want, string what)
    {
        if (EqualityComparer<T>.Default.Equals(got, want)) { passed++; return; }
        failed++; Console.WriteLine("FAIL  " + what + "\n      got  " + (got == null ? "(null)" : got.ToString()) + "\n      want " + (want == null ? "(null)" : want.ToString()));
    }
    static void True(bool v, string what) { if (v) passed++; else { failed++; Console.WriteLine("FAIL  " + what); } }
    static void Note(string s) => Console.WriteLine("      " + s);

    static string work, stock;
    static int Main(string[] args)
    {
        work = Path.GetFullPath(args[0]); stock = args.Length > 1 ? args[1] : null;
        var zips = args.Skip(2).ToList();
        if (Directory.Exists(work)) Directory.Delete(work, true);
        Directory.CreateDirectory(work);
        ReflexAssetImport.RetryFirstMs = 5;
        Synthetic(); Finder(); Locations(); RetryTest();
        if (stock != null)
        {
            HeaderWalk();
            var plan = MeasureStock();
            Small();
            if (zips.Count > 0) Mods(zips);
            if (Environment.GetEnvironmentVariable("IMPORTFLOW_FULL") == "1") Full(plan);
        }
        Console.WriteLine($"importflow: {passed} passed, {failed} failed");
        Console.WriteLine(failed == 0 ? "IMPORTFLOW PASS" : "IMPORTFLOW FAIL");
        return failed == 0 ? 0 : 1;
    }

    // ---------------------------------------------------------------------------------------------- pure

    static void Synthetic()
    {
        bool ok = ReflexAssetImport.TryBytes("3.1G", out long n); True(ok && n == (long)(3.1 * (1L << 30)), "3.1G");
        ReflexAssetImport.TryBytes("500M", out n); Eq(n, 500L << 20, "500M");
        ReflexAssetImport.TryBytes("12345", out n); Eq(n, 12345L, "plain bytes");
        ReflexAssetImport.TryBytes("2TB", out n); Eq(n, 2L << 40, "2TB");
        True(!ReflexAssetImport.TryBytes("lots", out _), "not a size");
        Eq(ReflexAssetImport.TestFreeFor("3G", @"C:\x"), 3L << 30, "test free: one value for every folder");
        string spec = @"C:\=3.1G;D:\=400G;D:\Small=1G;*=50G";
        Eq(ReflexAssetImport.TestFreeFor(spec, @"C:\Users\player\AppData\Roaming\mxa-launcher\clients\x"), (long)(3.1 * (1L << 30)), "test free: C:");
        Eq(ReflexAssetImport.TestFreeFor(spec, @"D:\OpenReflex"), 400L << 30, "test free: D:");
        Eq(ReflexAssetImport.TestFreeFor(spec, @"D:\Small\x"), 1L << 30, "test free: the longest prefix wins");
        Eq(ReflexAssetImport.TestFreeFor(spec, @"D:\Smaller"), 400L << 30, "test free: a prefix is a whole folder name");
        Eq(ReflexAssetImport.TestFreeFor(spec, "/Volumes/Games"), 50L << 30, "test free: the default");
        Eq(ReflexAssetImport.TestFreeFor("/tmp/a=1G", "/tmp/b"), (long?)null, "test free: no match");
        Eq(ReflexAssetImport.TestFreeFor("", "/tmp"), (long?)null, "test free: unset");

        // disk full, as each runtime reports it
        True(ReflexAssetImport.IsDiskFull(new IOException("Win32 IO returned 112. Path: C:\\x", unchecked((int)0x80070070))), "Mono on Windows: Win32 IO returned 112");
        True(ReflexAssetImport.IsDiskFull(new IOException("There is not enough space on the disk.", unchecked((int)0x80070070))), "HRESULT 0x80070070");
        True(ReflexAssetImport.IsDiskFull(new IOException("x", unchecked((int)0x80070027))), "HRESULT 0x80070027 (handle disk full)");
        True(ReflexAssetImport.IsDiskFull(new IOException("No space left on device : '/x'", 28)), "ENOSPC (.NET on Unix)");
        True(ReflexAssetImport.IsDiskFull(new IOException("Disk full. Path /x")), "Mono on Unix: Disk full");
        True(ReflexAssetImport.IsDiskFull(new IOException("While extracting a: wrapped", new IOException("Win32 IO returned 112. Path: x", unchecked((int)0x80070070)))), "inside a wrapper");
        True(ReflexAssetImport.IsDiskFull(new AggregateException(new IOException("x", unchecked((int)0x80070070)))), "inside an AggregateException");
        True(!ReflexAssetImport.IsDiskFull(new IOException("Sharing violation", unchecked((int)0x80070020))), "a sharing violation is not disk full");
        True(!ReflexAssetImport.IsDiskFull(new InvalidCastException("Specified cast is not valid.")), "a cast error is not disk full");
        True(!ReflexAssetImport.IsDiskFull(new IOException("Win32 IO returned 1120. Path: x")), "error 1120 is not 112");
        True(ReflexAssetImport.IsTransient(new IOException("Sharing violation", unchecked((int)0x80070020))), "sharing violation: retried");
        True(ReflexAssetImport.IsTransient(new IOException("lock", unchecked((int)0x80070021))), "lock violation: retried");
        True(ReflexAssetImport.IsTransient(new UnauthorizedAccessException("Access to the path is denied.")), "access denied (an antivirus scan): retried");
        True(!ReflexAssetImport.IsTransient(new IOException("x", unchecked((int)0x80070070))), "disk full: not retried");
        True(!ReflexAssetImport.IsTransient(new FileNotFoundException("x")), "a missing file: not retried");

        // the verdict
        long req = 19L << 30;
        Eq(ImportPlan.Margin(req), 1L << 30, "margin: 1 GB for 19 GB");
        Eq(ImportPlan.Margin(100L << 30), 5L << 30, "margin: 5 % of 100 GB");
        True(!ReflexAssetImport.Enough((long)(3.1 * (1L << 30)), req), "3.1 GB free is not enough for 19 GB");
        True(!ReflexAssetImport.Enough(req + (512L << 20), req), "within the margin is not enough");
        True(ReflexAssetImport.Enough(req + (1L << 30), req), "required + margin is enough");
        True(ReflexAssetImport.Enough(null, req), "unknown free space does not block");
        Eq(ReflexAssetImport.GB((long)(9.4 * (1L << 30)), true), "9.4 GB", "GB binary");
        Eq(ReflexAssetImport.GB(9_400_000_000, false), "9.4 GB", "GB decimal (macOS)");
        Eq(ReflexAssetImport.GB(850L << 20, true), "850 MB", "MB");
        Eq(ReflexAssetImport.GB(412L << 30, true), "412 GB", "big drives without decimals");
        Eq(ReflexAssetImport.GB(0, true), "1 MB", "never 0 MB");

        // names and roots
        Eq(ReflexAssetImport.PlaceName(@"C:\Users\x"), "C:", "C:");
        Eq(ReflexAssetImport.PlaceName(@"D:\", "Games"), "D: (Games)", "D: with its label");
        Eq(ReflexAssetImport.PlaceName(@"Z:\run\media\mmcblk0p1\OpenReflex"), "the microSD card", "Steam Deck microSD card");
        Eq(ReflexAssetImport.PlaceName(@"Z:\run\media\deck\SanDisk"), "the drive SanDisk", "a USB drive on SteamOS");
        Eq(ReflexAssetImport.PlaceName(@"Z:\home\deck\x"), "the internal storage", "SteamOS home");
        Eq(ReflexAssetImport.PlaceName("/Volumes/Games/x"), "Games", "macOS volume");
        Eq(ReflexAssetImport.PlaceName("/Users/x/OpenReflex"), "your startup disk", "macOS startup disk");
        Eq(ReflexAssetImport.PlaceName(@"\\nas\share\x"), @"\\nas\share", "network share");
        Eq(ReflexAssetImport.RootOf(@"C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex\game files"), @"C:\", "root of the launcher's folder");
        Eq(ReflexAssetImport.RootOf(@"Z:\run\media\mmcblk0p1\OpenReflex\game files"), @"Z:\run\media\mmcblk0p1", "root of the microSD card");
        Eq(ReflexAssetImport.RootOf(@"Z:\run\media\deck\SanDisk\OpenReflex"), @"Z:\run\media\deck\SanDisk", "root of a user mount");
        Eq(ReflexAssetImport.RootOf("/Volumes/Games/OpenReflex/game files"), "/Volumes/Games", "root of a macOS volume");

        // checks
        Func<string, string> env = k => k == "OneDrive" ? @"C:\Users\Zoë\OneDrive" : k == "ProgramFiles" ? @"C:\Program Files" : k == "ProgramFiles(x86)" ? @"C:\Program Files (x86)" : null;
        True(ReflexAssetImport.InOneDrive(@"C:\Users\Zoë\OneDrive\Desktop\OpenReflex", env), "OneDrive by its variable (non-ASCII user)");
        True(ReflexAssetImport.InOneDrive(@"D:\OneDrive - Contoso\Games", k => null), "OneDrive for work by its folder name");
        True(!ReflexAssetImport.InOneDrive(@"C:\Users\Zoë\AppData\Roaming\mxa-launcher", env), "AppData is not OneDrive");
        True(ReflexAssetImport.InProgramFiles(@"C:\Program Files (x86)\Steam\steamapps\common\MX vs ATV Reflex", env), "Program Files (x86)");
        True(ReflexAssetImport.InProgramFiles(@"E:\Program Files\x", k => null), "Program Files on another drive");
        True(!ReflexAssetImport.InProgramFiles(@"D:\Games\Program Files Stuff", env), "a folder named like it deeper down");
        var p = new ImportPlan { longestRelative = 140 };
        int deep = ReflexAssetImport.DeepestPath(@"C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex\game files", p);
        Eq(deep, @"C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex\.rxi-f0ea4f04".Length + 1 + 140, "deepest path through the stage");
        True(ReflexAssetImport.DeepestPath(@"D:\OpenReflex\game files", p) <= ReflexAssetImport.WindowsMaxPath, "D:\\OpenReflex fits Windows' limit");
        var (lo, hi) = ReflexAssetImport.Minutes(new ImportPlan { looseBytes = 1L << 30, assetBytes = 14L << 30 });
        True(lo >= 1 && hi > lo, "a time range (" + lo + "-" + hi + " min)");

        // Steam libraries
        string deck = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"/home/deck/.local/share/Steam\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"/run/media/mmcblk0p1\"\n\t}\n}\n";
        Eq(string.Join(" | ", ReflexPaths.SteamLibraries(deck, "Z:")), @"Z:\home\deck\.local\share\Steam | Z:\run\media\mmcblk0p1", "Steam Deck libraries through Proton's Z:");
        Eq(string.Join(" | ", ReflexPaths.SteamLibraries(deck)), "/home/deck/.local/share/Steam | /run/media/mmcblk0p1", "Linux / macOS libraries as POSIX paths");
        string mixed = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t}\n}\n";
        Eq(string.Join(" | ", ReflexPaths.SteamLibraries(mixed, "Z:")), @"C:\Program Files (x86)\Steam | D:\SteamLibrary", "Windows paths unchanged with a unix drive");
        Eq(ReflexPaths.UnixToWine("/run/media/deck/My Card//x/"), @"Z:\run\media\deck\My Card\x", "unix path to Wine");
        Eq(ReflexPaths.UnixToWine("/"), @"Z:\", "the Linux root");
    }

    sealed class FakeFs : IFinderFs
    {
        readonly HashSet<string> dirs = new HashSet<string>(StringComparer.OrdinalIgnoreCase); readonly Dictionary<string, string> texts = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        static string K(string p) => p.Replace('/', '\\').TrimEnd('\\');
        public void Dir(string p) { for (string d = K(p); d.Length > 0; d = d.Contains('\\') ? d.Substring(0, d.LastIndexOf('\\')) : "") dirs.Add(d); }
        public void Text(string p, string t) { texts[K(p)] = t; string k = K(p); Dir(k.Substring(0, k.LastIndexOf('\\'))); }
        public bool DirExists(string p) => dirs.Contains(K(p));
        public IEnumerable<string> SubDirs(string p) { string k = K(p) + "\\"; bool posix = p.StartsWith("/"); return dirs.Where(d => d.StartsWith(k, StringComparison.OrdinalIgnoreCase) && d.IndexOf('\\', k.Length) < 0).Select(d => posix ? d.Replace('\\', '/') : d).ToList(); }
        public string ReadText(string p) => texts.TryGetValue(K(p), out var t) ? t : null;
    }

    static void Finder()
    {
        // SteamOS through Proton: the game in the microSD card's library, listed by the internal library's libraryfolders.vdf (Linux paths)
        var fs = new FakeFs();
        fs.Text(@"Z:\home\deck\.local\share\Steam\steamapps\libraryfolders.vdf", "\"libraryfolders\"\n{\n\"0\"\n{\n\"path\" \"/home/deck/.local/share/Steam\"\n}\n\"1\"\n{\n\"path\" \"/run/media/mmcblk0p1\"\n}\n}\n");
        fs.Dir(@"Z:\home\deck\.local\share\Steam\steamapps\common\Some Game");
        fs.Dir(@"Z:\run\media\mmcblk0p1\steamapps\common\MX vs. ATV Reflex\Database");
        fs.Dir(@"Z:\run\media\deck\SanDisk\SteamLibrary\steamapps\common\MX vs ATV Reflex\Database");   // a USB drive's library not in the vdf
        Func<string, bool> isInstall = d => fs.DirExists(d.TrimEnd('\\') + "\\Database");
        var mounts = ReflexAssetImport.ProtonMounts(fs, "/home/deck");
        Eq(string.Join(" | ", mounts), @"Z:\home\deck | Z:\run\media\deck\SanDisk | Z:\run\media\mmcblk0p1", "Proton mounts: home, a user's mount, the card");
        Func<string, string> env = k => k == "HOME" ? "/home/deck" : k == "STEAM_COMPAT_CLIENT_INSTALL_PATH" ? "/home/deck/.local/share/Steam" : null;
        var roots = InstallFinder.ProtonSteamRoots(env, mounts);
        True(roots.Contains(@"Z:\home\deck\.local\share\Steam") && roots.Contains(@"Z:\run\media\deck\SanDisk\SteamLibrary"), "Proton Steam roots: " + string.Join(", ", roots));
        var found = InstallFinder.Find(new[] { @"C:\", @"Z:\" }, fs, isInstall, roots, "Z:").Select(f => f.Path).ToList();
        True(found.Contains(@"Z:\run\media\mmcblk0p1\steamapps\common\MX vs. ATV Reflex"), "the Steam Deck's microSD card library (" + string.Join(", ", found) + ")");
        True(found.Contains(@"Z:\run\media\deck\SanDisk\SteamLibrary\steamapps\common\MX vs ATV Reflex"), "a USB drive's SteamLibrary");
        // macOS: a CrossOver bottle and a Whisky bottle
        var mac = new FakeFs();
        mac.Dir("/Users/zoe/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/MX vs. ATV Reflex/Database");
        mac.Dir("/Users/zoe/Library/Containers/com.isaacmarovitz.Whisky/Bottles/ABCD/drive_c/Program Files (x86)/THQ/MX vs ATV Reflex/Database");
        mac.Dir("/Users/zoe/Games/MX vs ATV Reflex (copy)/Archive/Database");
        var posix = InstallFinder.FindPosix("/Users/zoe", mac, d => mac.DirExists(d + "/Database")).Select(f => f.Source + " " + f.Path).ToList();
        Eq(posix.Count, 3, "macOS: three installs (" + string.Join(" ; ", posix) + ")");
        True(posix.Any(x => x.StartsWith("STEAM /Users/zoe/Library/Application Support/CrossOver")), "CrossOver bottle's Steam");
        True(posix.Any(x => x.Contains("Whisky") && x.Contains("THQ")), "Whisky bottle's THQ folder");
        True(posix.Any(x => x.EndsWith("(copy)/Archive")), "~/Games, its Archive folder");
    }

    static void Locations()
    {
        var drives = new List<DriveEntry> { new DriveEntry { Root = @"C:\", Label = "Windows" }, new DriveEntry { Root = @"D:\", Label = "Games" }, new DriveEntry { Root = @"E:\", Kind = "DISC", Optical = true }, new DriveEntry { Root = @"N:\", Kind = "NETWORK", Network = true } };
        var free = new Dictionary<string, long> { { @"C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex", 3L << 30 }, { @"D:\OpenReflex", 412L << 30 }, { @"N:\OpenReflex", 2L << 40 } };
        var l = ReflexAssetImport.Locations(drives, null, null, @"C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex\game files", f => free.TryGetValue(f, out var v) ? v : (long?)null);
        Eq(string.Join(" | ", l.Select(x => x.label + (x.current ? "*" : "") + " " + x.folder)), @"C: (Windows)* C:\Users\player\AppData\Roaming\mxa-launcher\clients\OpenReflex | D: (Games) D:\OpenReflex | N: N:\OpenReflex", "Windows: drives, no disc, the client's marked (its own folder)");
        True(l.First(x => x.root == @"N:\").network, "a network drive is marked");
        Eq(l[1].free, 412L << 30, "free space per drive");
        // SteamOS: Z: stands for the mounts
        var proton = ReflexAssetImport.Locations(new List<DriveEntry> { new DriveEntry { Root = @"C:\" }, new DriveEntry { Root = @"Z:\" } }, new[] { @"Z:\home\deck", @"Z:\run\media\mmcblk0p1" }, null, @"C:\users\steamuser\AppData\Roaming\mxa-launcher\clients\x\game files", f => 1L << 35);
        Eq(string.Join(" | ", proton.Select(x => x.label + " " + x.folder)), @"C: C:\users\steamuser\AppData\Roaming\mxa-launcher\clients\x | the internal storage Z:\home\deck\OpenReflex | the microSD card Z:\run\media\mmcblk0p1\OpenReflex", "SteamOS: the prefix, the home, the card");
        // macOS
        var macl = ReflexAssetImport.Locations(null, new[] { "/", "/Volumes/Games" }, "/Users/zoe", "/Users/zoe/Games/OpenReflex/game files", f => 1L << 36);
        Eq(string.Join(" | ", macl.Select(x => x.label + (x.current ? "*" : "") + " " + x.folder)), "your startup disk* /Users/zoe/Games/OpenReflex | Games /Volumes/Games/OpenReflex", "macOS: the startup disk (home) and a volume");
    }

    static void RetryTest()
    {
        int calls = 0;
        int r = ReflexAssetImport.Retry(() => { if (++calls < 3) throw new IOException("The process cannot access the file because it is being used by another process.", unchecked((int)0x80070020)); return 7; }, CancellationToken.None);
        True(r == 7 && calls == 3, "a sharing violation (an antivirus scan) is retried until it clears (" + calls + " tries)");
        calls = 0;
        try { ReflexAssetImport.Retry(() => { calls++; if (calls > 0) throw new IOException("x", unchecked((int)0x80070070)); return 0; }, CancellationToken.None); failed++; Console.WriteLine("FAIL  disk full passes through"); }
        catch (IOException) { True(calls == 1, "a full disk is not retried"); }
        calls = 0;
        try { ReflexAssetImport.Retry(() => { calls++; if (calls > 0) throw new UnauthorizedAccessException("denied"); return 0; }, CancellationToken.None, 3); failed++; }
        catch (UnauthorizedAccessException) { Eq(calls, 4, "access denied: tried 1 + 3 times, then the error"); }
    }

    // ---------------------------------------------------------------------------------------------- with the stock install

    static string DbDir => Path.Combine(stock, "Database");

    /// OutputSize (chunk headers only) against the real decode, every asset of a sample of the packages.
    static void HeaderWalk()
    {
        var install = new Rfx.Install(DbDir);
        int assets = 0, lzx = 0, mismatches = 0; long bytes = 0;
        var names = Directory.GetFiles(DbDir, "*.dx9.database").Select(f => Path.GetFileName(f).Replace(".dx9.database", "")).OrderBy(x => x, StringComparer.Ordinal).ToList();
        var sample = names.Where((n, i) => i % 9 == 0 || n == "MXs" || n == "Rider" || n.StartsWith("NAT_Concord", StringComparison.Ordinal)).ToList();
        foreach (var n in sample)
        {
            Rfx.Database db; try { db = install.Db(n); } catch (Exception) { continue; }
            if (db == null || n == "SX_FresnoB") continue;
            foreach (var p in db.packages.Values)
            {
                using var fs = new FileStream(Path.Combine(DbDir, p.file), FileMode.Open, FileAccess.Read, FileShare.Read, 64);
                foreach (var a in p.assets)
                {
                    long walked = ReflexAssetImport.OutputSize(fs, a); byte[] real = install.Read(a);
                    assets++; if (a.lzx) lzx++; bytes += real.Length;
                    if (walked != real.Length) { if (mismatches++ < 5) Note("mismatch " + n + "/" + p.name + "/" + a.name + "." + a.type + ": walked " + walked + " decoded " + real.Length); }
                }
            }
        }
        install.Close();
        Note($"header walk: {sample.Count} databases, {assets} assets ({lzx} LZX), {bytes / 1048576.0:0} MB decoded");
        True(assets > 5000 && lzx > 1000, "a real sample (" + assets + " assets)");
        Eq(mismatches, 0, "the chunk headers give every asset's decoded size exactly");
    }

    static ImportPlan MeasureStock()
    {
        var plan = ReflexAssetImport.Measure(stock, null, CancellationToken.None);
        Note($"stock install: loose {plan.looseFiles} files {plan.looseBytes:N0} B, assets {plan.assetFiles} {plan.assetBytes:N0} B, catalog ~{plan.catalogBytes:N0} B, staging +{plan.stagingExtraBytes:N0} B, folders {plan.folders}");
        Note($"  = {plan.Bytes:N0} B ({ReflexAssetImport.GB(plan.Bytes, true)} binary / {ReflexAssetImport.GB(plan.Bytes, false)} decimal); on 4 KB clusters {ReflexAssetImport.GB(plan.Required(4096), true)} ({plan.Required(4096):N0} B); +margin {ReflexAssetImport.GB(plan.Required(4096) + ImportPlan.Margin(plan.Required(4096)), true)}");
        Note($"  estimated (unreadable headers): {plan.estimated}; unreadable optional: {plan.unreadable.Count}; fatal: {plan.fatalFile ?? "none"}; longest path {plan.longestRelative}: {plan.longestPath}; measured in {plan.seconds:0.0} s");
        True(plan.assetFiles > 100000 && plan.assetBytes > (8L << 30), "the stock game is measured (" + plan.assetFiles + " assets)");
        True(plan.fatalFile == null && plan.unreadable.Count == 0, "the stock install reads");
        True(plan.seconds < 120, "measured in under two minutes (" + plan.seconds.ToString("0.0") + " s)");
        return plan;
    }

    static string Clone(string from, string to)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(to));
        var psi = new ProcessStartInfo("cp") { UseShellExecute = false }; psi.ArgumentList.Add("-cR"); psi.ArgumentList.Add(from); psi.ArgumentList.Add(to);
        var p = Process.Start(psi);
        p.WaitForExit(); if (p.ExitCode != 0) throw new IOException("cp -c failed: " + from);
        return to;
    }

    static long Written(string dir) => new DirectoryInfo(dir).EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length);
    static readonly HashSet<string> Catalog = new HashSet<string> { "asset-index.html", "asset-inventory.tsv", "reflex-assets.json", "import-report.txt", "README - GAME FILES.txt", ReflexAssetImport.ExtractedMarker };
    static long WrittenNoCatalog(string dir) => new DirectoryInfo(dir).EnumerateFiles("*", SearchOption.AllDirectories)
        .Where(f => { string r = Path.GetRelativePath(dir, f.FullName).Replace('\\', '/'); return !r.StartsWith("Browse/") && !Catalog.Contains(r); }).Sum(f => f.Length);
    static string StageOf(string parent) => Directory.Exists(parent) ? Directory.GetDirectories(parent, ReflexAssetImport.StagePrefix + "*").FirstOrDefault() : null;

    static ReflexAssetImport.Result Run(string src, string dest, ReflexAssetImport.Options o, CancellationToken cancel = default, Action<ReflexAssetImport.Progress> progress = null)
    {
        try { return ReflexAssetImport.Import(src, dest, progress, cancel, o).GetAwaiter().GetResult(); }
        finally { ReflexAssetImport.TestWritten = 0; }
    }

    /// A small install of clones: a few databases and loose files of the stock one.
    static void Small()
    {
        string src = Path.Combine(work, "small", "MX vs ATV Reflex");
        Directory.CreateDirectory(Path.Combine(src, "Database"));
        foreach (var db in new[] { "MX_Wheels", "Rider_Goggles_Models", "HR_Rider_Goggles_Models", "MXUI_DLC001", "AIData", "CameraTweaks", "UIScene", "Rider_Jerseys_Models", "MXTables_DLC004", "MX_Forks" })
            foreach (var ext in new[] { ".dx9.database", ".dx9.package" }) Clone(Path.Combine(DbDir, db + ext), Path.Combine(src, "Database", db + ext));
        foreach (var f in new[] { "Engine.xml", "version.xml", "PCControls.bxml", "MXReflex.exe" }) Clone(Path.Combine(stock, f), Path.Combine(src, f));
        Clone(Path.Combine(stock, "Physics"), Path.Combine(src, "Physics"));
        var plan = ReflexAssetImport.Measure(src, null, CancellationToken.None);
        string outRoot = Path.Combine(work, "small-out");
        // 1. the import with the plan: measured = written; true totals in the progress
        var seen = new List<ReflexAssetImport.Progress>();
        var r = Run(src, Path.Combine(outRoot, "a", "game files"), new ReflexAssetImport.Options { plan = plan }, default, p => { lock (seen) seen.Add(p); });
        long got = WrittenNoCatalog(r.directory);
        Eq(got, plan.looseBytes + plan.assetBytes, "measured = written (files and assets, " + plan.looseFiles + " + " + plan.assetFiles + ")");
        long all = Written(r.directory);
        Note($"small: catalog measured ~{plan.catalogBytes:N0} B, written {all - got:N0} B; required on 4 KB {plan.Required(4096):N0} B for {all:N0} B written, du {DiskUsage(r.directory):N0} B");
        True(plan.Required(4096) >= DiskUsage(r.directory), "Required covers the disk usage");
        True(seen.Count > 0 && seen.All(x => x.total == plan.looseBytes + plan.assetBytes || x.phase == 3), "progress has the plan's total in both phases");
        True(seen.Any(x => x.phase == 2), "progress reports the extraction phase");
        Eq(StageOf(Path.Combine(outRoot, "a")), (string)null, "no staging folder left");
        // 2. a disk-full error mid-way (the fault injector): ImportException DiskFull, the stage removed, no destination
        string dest = Path.Combine(outRoot, "b", "game files");
        try { Run(src, dest, new ReflexAssetImport.Options { plan = plan, failAtBytes = plan.Bytes / 2 }); failed++; Console.WriteLine("FAIL  disk full did not stop the import"); }
        catch (ImportException e)
        {
            Eq(e.Kind, ImportException.Failure.DiskFull, "disk full at N bytes: DiskFull (" + e.Message + ")");
            True(e.Message.Contains("Win32 IO returned 112") && e.InnerException != null, "the raw error is kept for COPY ERROR DETAILS");
            True(e.Needed > 0 && e.Folder == Path.GetDirectoryName(dest), "what it still needed and where (" + e.Needed + ")");
        }
        True(!Directory.Exists(dest) && StageOf(Path.GetDirectoryName(dest)) == null, "after disk full: no game files folder, no staging folder");
        // the same through the environment hook
        Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_DISK_FULL_AT", (plan.Bytes / 3).ToString());
        try { Run(src, dest, new ReflexAssetImport.Options()); failed++; Console.WriteLine("FAIL  REFLEX_IMPORT_TEST_DISK_FULL_AT ignored"); }
        catch (ImportException e) { Eq(e.Kind, ImportException.Failure.DiskFull, "REFLEX_IMPORT_TEST_DISK_FULL_AT"); }
        Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_DISK_FULL_AT", null);
        True(!Directory.Exists(dest) && StageOf(Path.GetDirectoryName(dest)) == null, "after the env fault: nothing left");
        // 3. the free space re-check: not enough at the start; the drive filling up while it runs (something else takes space mid-way)
        long need = plan.Required(4096);
        ReflexAssetImport.FreeFloor = 1 << 20;
        try
        {
            try { Run(src, dest, new ReflexAssetImport.Options { plan = plan, freeBytes = _ => need / 2 }); failed++; Console.WriteLine("FAIL  too little space did not stop the start"); }
            catch (ImportException e) { True(e.Kind == ImportException.Failure.DiskFull && e.Step == "starting", "not enough at the start: stops before writing (" + e.Message + ")"); }
            int asks = 0;
            Func<string, long?> filling = _ => { asks++; return asks == 1 ? need + (8L << 20) : (need - ReflexAssetImport.TestWritten) / 2; };   // fine at the start, then something else takes half
            try { Run(src, dest, new ReflexAssetImport.Options { plan = plan, freeBytes = filling }); failed++; Console.WriteLine("FAIL  the re-check did not stop the import"); }
            catch (ImportException e) { True(e.Kind == ImportException.Failure.DiskFull && e.Step != "starting", "the drive filling up while it runs: DiskFull at '" + e.Step + "' (" + e.Message + ")"); }
            True(!Directory.Exists(dest) && StageOf(Path.GetDirectoryName(dest)) == null, "after the re-check: nothing left");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", Path.Combine(outRoot, "b") + "=" + (need / 4));
            Eq(ReflexAssetImport.FreeBytes(Path.Combine(outRoot, "b", "x")), need / 4, "REFLEX_IMPORT_TEST_FREE_BYTES for the folder");
            try { Run(src, dest, new ReflexAssetImport.Options { plan = plan }); failed++; Console.WriteLine("FAIL  env free space ignored"); }
            catch (ImportException e) { Eq(e.Kind, ImportException.Failure.DiskFull, "REFLEX_IMPORT_TEST_FREE_BYTES below the plan: DiskFull"); }
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", null);
        }
        finally { ReflexAssetImport.FreeFloor = 256L << 20; }
        True(ReflexAssetImport.FreeBytes(work) > 0, "the real free space of this disk is read (" + ReflexAssetImport.GB(ReflexAssetImport.FreeBytes(work) ?? 0) + ")");
        // 4. cancel: stops, removes the stage
        using (var cts = new CancellationTokenSource())
        {
            try { Run(src, dest, new ReflexAssetImport.Options { plan = plan, slowMs = 20 }, cts.Token, pr => { if (pr.copied > 0) cts.Cancel(); }); failed++; Console.WriteLine("FAIL  cancel ignored"); }
            catch (OperationCanceledException) { passed++; }
            True(!Directory.Exists(dest) && StageOf(Path.GetDirectoryName(dest)) == null, "after cancel: nothing left");
        }
        // 5. a leftover stage resumed: a finished import's files with one cut short, one missing, one damaged and a stray file
        string resumeParent = Path.Combine(outRoot, "c"); Directory.CreateDirectory(resumeParent);
        string left = Path.Combine(resumeParent, ".rxi-0badf00d");
        Clone(r.directory, left);
        foreach (var f in new[] { "reflex-extracted-v1.txt", "reflex-assets.json", "asset-index.html" }) File.Delete(Path.Combine(left, f));
        var assets = Directory.GetFiles(Path.Combine(left, "Assets"), "*", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.Ordinal).ToList();
        File.WriteAllBytes(assets[3], File.ReadAllBytes(assets[3]).Take(10).ToArray());                // cut short (a crash mid-write)
        File.Delete(assets[7]);                                                                          // never written
        var dmg = File.ReadAllBytes(assets[11]); dmg[dmg.Length / 2] ^= 0x5A; File.WriteAllBytes(assets[11], dmg);   // same length, other bytes
        File.WriteAllText(Path.Combine(left, "Assets", "stray.bin"), "not part of this import");
        var lo = ReflexAssetImport.Leftovers(resumeParent);
        True(lo.Count == 1 && lo[0].bytes > 0 && lo[0].files > 0, "the leftover is found with its size (" + (lo.Count > 0 ? ReflexAssetImport.GB(lo[0].bytes) : "-") + ")");
        var rr = Run(src, Path.Combine(resumeParent, "game files"), new ReflexAssetImport.Options { plan = plan, resumeStage = left });
        True(rr.resumed, "resumed");
        Eq(WrittenNoCatalog(rr.directory), plan.looseBytes + plan.assetBytes, "the resumed import is whole");
        True(Same(r.directory, rr.directory), "the resumed import's files equal a fresh import's");
        True(!File.Exists(Path.Combine(rr.directory, "Assets", "stray.bin")), "the stray file of the leftover is gone");
        Eq(StageOf(resumeParent), (string)null, "the leftover became the game files folder");
        // a leftover the player removes: DeleteTree, read-only files too
        string junk = Path.Combine(resumeParent, ".rxi-deadbeef"); Directory.CreateDirectory(Path.Combine(junk, "Assets")); File.WriteAllText(Path.Combine(junk, "Assets", "x"), "x"); File.SetAttributes(Path.Combine(junk, "Assets", "x"), FileAttributes.ReadOnly);
        True(ReflexAssetImport.DeleteTree(junk) && !Directory.Exists(junk), "CLEAN UP removes a leftover with read-only files");
        // 6. skipped files: left out, said so
        var rs = Run(src, Path.Combine(outRoot, "d", "game files"), new ReflexAssetImport.Options { skipFiles = new[] { "MXTables_DLC004.dx9.database", "MXTables_DLC004.dx9.package" } });
        True(!File.Exists(Path.Combine(rs.directory, "Database", "MXTables_DLC004.dx9.database")) && !Directory.Exists(Path.Combine(rs.directory, "Assets", "tbldata", "MXTables_DLC004")), "skipped files are not imported");
        True(rs.warnings.Any(w => w.Contains("MXTables_DLC004.dx9.database")) && rs.skippedFiles.Length == 2, "the report says what was left out");
        var ps = ReflexAssetImport.Measure(src, new[] { "MXTables_DLC004.dx9.database", "MXTables_DLC004.dx9.package" }, CancellationToken.None);
        Eq(WrittenNoCatalog(rs.directory), ps.looseBytes + ps.assetBytes, "Measure with the same skips = written");
        // 7. a damaged required database: BadSourceFile naming it; an unreadable optional one: skipped with a warning
        string bad = Path.Combine(work, "bad", "MX vs ATV Reflex"); Clone(src, bad);
        File.Delete(Path.Combine(bad, "Database", "MX_Forks.dx9.database"));
        File.WriteAllBytes(Path.Combine(bad, "Database", "MX_Forks.dx9.database"), File.ReadAllBytes(Path.Combine(src, "Database", "MX_Forks.dx9.database")).Take(40).ToArray());
        File.WriteAllBytes(Path.Combine(bad, "Database", "MXTables_DLC008.dx9.database"), new byte[] { 0x42, 0x58, 0x4D, 0x4C, 1, 2, 3 });
        var pb = ReflexAssetImport.Measure(bad, null, CancellationToken.None);
        Eq(pb.fatalFile, "MX_Forks.dx9.database", "Measure names the unreadable required database");
        True(pb.unreadable.Any(u => u.StartsWith("MXTables_DLC008.dx9.database")), "Measure lists the unreadable optional one");
        try { Run(bad, Path.Combine(outRoot, "e", "game files"), null); failed++; Console.WriteLine("FAIL  a damaged database imported"); }
        catch (ImportException e) { True(e.Kind == ImportException.Failure.BadSourceFile && e.SourceFile == "MX_Forks.dx9.database", "BadSourceFile " + e.SourceFile + " (" + e.Message + ")"); }
        True(StageOf(Path.Combine(outRoot, "e")) == null, "after a bad file: nothing left");
        var rb = Run(bad, Path.Combine(outRoot, "f", "game files"), new ReflexAssetImport.Options { skipFiles = new[] { "MX_Forks.dx9.database", "MX_Forks.dx9.package" } });
        True(rb.warnings.Any(w => w.StartsWith("Skipped MXTables_DLC008.dx9.database")), "the unreadable DLC table is skipped with a warning");
        // 8. the destination already there: refused (the pre-flight offers to use it or move it aside)
        try { Run(src, r.directory, null); failed++; Console.WriteLine("FAIL  an existing folder was overwritten"); } catch (IOException e) { True(e.Message.Contains("already exists"), "an existing game files folder is not overwritten"); }
    }

    static bool Same(string a, string b)
    {
        var fa = Directory.GetFiles(Path.Combine(a, "Assets"), "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(a, f)).OrderBy(x => x, StringComparer.Ordinal).ToList();
        var fb = Directory.GetFiles(Path.Combine(b, "Assets"), "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(b, f)).OrderBy(x => x, StringComparer.Ordinal).ToList();
        if (!fa.SequenceEqual(fb)) { Note("file lists differ: " + fa.Count + " / " + fb.Count); return false; }
        foreach (var f in fa) if (!File.ReadAllBytes(Path.Combine(a, f)).SequenceEqual(File.ReadAllBytes(Path.Combine(b, f)))) { Note("differs: " + f); return false; }
        return true;
    }

    // ---------------------------------------------------------------------------------------------- mods in Database

    static void Extract(string zip, string to, Func<string, bool> want = null)
    {
        using var z = ZipFile.OpenRead(zip);
        foreach (var e in z.Entries)
        {
            if (e.FullName.EndsWith("/") || e.FullName.Contains("__MACOSX") || want != null && !want(e.FullName)) continue;
            string name = Path.GetFileName(e.FullName);
            if (name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) { string inner = Path.Combine(work, "zips", Guid.NewGuid().ToString("N") + ".zip"); Directory.CreateDirectory(Path.GetDirectoryName(inner)); e.ExtractToFile(inner); Extract(inner, to, want); continue; }
            e.ExtractToFile(Path.Combine(to, name), true);
        }
    }

    static void Mods(List<string> zips)
    {
        string game = Path.Combine(work, "modded", "MX vs ATV Reflex");
        Clone(stock, game);
        string db = Path.Combine(game, "Database");
        Eq(ImportModScan.Scan(db).Count, 0, "the stock Database folder: no mods");
        string Zip(string part) => zips.FirstOrDefault(z => Path.GetFileName(z).IndexOf(part, StringComparison.OrdinalIgnoreCase) >= 0);
        // dropped in the original way: Distractions (gear, DLC010), BARX (bike kit, DLC014), the Hurrencane Hills track over Beta_Nat_Track_Slot_5,
        // one slot of a track DLC pack (its tables and level), a stock file replaced, a damaged DLC008, a folder unzipped inside Database
        if (Zip("Distractions") != null) Extract(Zip("Distractions"), db);
        if (Zip("BARX") != null) Extract(Zip("BARX"), db);
        if (Zip("hurrencane") != null) Extract(Zip("hurrencane"), db);
        if (Zip("DLC0046") != null) Extract(Zip("DLC0046"), db, n => n.StartsWith("DLC0046/") && !n.EndsWith("/DLC"));
        File.WriteAllBytes(Path.Combine(db, "MXTables_DLC008.dx9.database"), File.ReadAllBytes(Path.Combine(db, Zip("Distractions") != null ? "MXTables_DLC010.dx9.database" : "MXTables_DLC004.dx9.database")).Take(60).ToArray());
        File.WriteAllBytes(Path.Combine(db, "MXTables_DLC008.dx9.package"), new byte[16]);
        File.Delete(Path.Combine(db, "MX_Forks.dx9.package")); File.WriteAllBytes(Path.Combine(db, "MX_Forks.dx9.package"), File.ReadAllBytes(Path.Combine(DbDir, "MX_Forks.dx9.package")).Concat(new byte[100]).ToArray());
        Directory.CreateDirectory(Path.Combine(db, "DLC0031")); File.WriteAllText(Path.Combine(db, "DLC0031", "DLC"), "1");
        var sw = Stopwatch.StartNew();
        var groups = ImportModScan.Scan(db);
        Note($"mod scan: {groups.Count} groups in {sw.ElapsedMilliseconds} ms");
        foreach (var g in groups) Note("  " + ImportModScan.Title(g) + " | " + ImportModScan.Advice(g) + " | " + g);
        var dlc8 = groups.FirstOrDefault(g => g.key == "DLC008");
        True(dlc8 != null && dlc8.unreadable != null && dlc8.unreadable.StartsWith("MXTables_DLC008.dx9.database"), "the damaged DLC008 table is unreadable");
        int firstReadable = groups.FindIndex(g => g.unreadable == null), lastUnreadable = groups.FindLastIndex(g => g.unreadable != null);
        True(lastUnreadable < firstReadable, "unreadable groups come first");
        if (Zip("Distractions") != null)
        {
            var gear = groups.FirstOrDefault(g => g.key == "DLC010");
            True(gear != null && gear.kind == DbModGroup.Kinds.GearPack && gear.unreadable == null && gear.moveTo == "Gear" && gear.files.Count == 8, "Distractions: a readable gear pack of 8 files, movable to Mods/Gear");
        }
        if (Zip("BARX") != null)
        {
            var kit = groups.FirstOrDefault(g => g.key == "DLC014");
            True(kit != null && kit.kind == DbModGroup.Kinds.KitPack && kit.moveTo == "Gear", "BARX: a bike kit pack, movable");
        }
        if (Zip("hurrencane") != null)
        {
            var track = groups.FirstOrDefault(g => g.kind == DbModGroup.Kinds.TrackSlot);
            True(track != null && track.key == "Beta_Nat_Track_Slot_5" && track.moveTo == "Tracks" && track.files.Count == 4, "a beta-slot track, movable to Mods/Tracks");
        }
        if (Zip("DLC0046") != null)
        {
            // (the 2017 supercross DLC packs store their resources with a compression codec this version does not read: the import used to
            // stop at SX_Rd17_2017_LasVegas, a level without "DLC" in its name, so not optional)
            True(groups.Any(g => g.kind == DbModGroup.Kinds.DlcFiles && g.key == "DLC0046" && g.moveTo == null && g.unreadable != null), "a track DLC slot's tables: unreadable, left out");
            True(groups.Any(g => g.kind == DbModGroup.Kinds.Track && g.key.Contains("SX_Rd17_2017_LasVegas") && g.unreadable != null), "its level is a community track, unreadable");
        }
        True(groups.Any(g => g.kind == DbModGroup.Kinds.Changed && g.key == "changed:MX_Forks" && g.replacesStock), "a replaced stock file is found by its size");
        True(groups.Any(g => g.IsFolder && g.files[0] == "DLC0031/"), "a folder inside Database");
        var skip = ImportModScan.FilesOf(groups.Where(g => g.unreadable != null));
        True(skip.Contains("MXTables_DLC008.dx9.database") && skip.Contains("MXTables_DLC008.dx9.package") && skip.All(f => groups.Any(g => g.unreadable != null && g.files.Contains(f))), "the files to leave out: " + string.Join(", ", skip));
        // Move: into a scratch Mods folder, never overwriting
        string mods = Path.Combine(work, "client", "Mods");
        foreach (var g in groups.Where(g => g.moveTo != null).ToList())
        {
            string to = ImportModScan.Move(g, db, mods);
            True(g.files.All(f => File.Exists(Path.Combine(to, f)) && !File.Exists(Path.Combine(db, f))), "moved " + ImportModScan.Title(g) + " to " + Path.GetRelativePath(work, to));
            if (g.moveTo == "Gear") True(KitLike(to), "the moved pack is a pack folder Mods/Gear reads (MXTables + package)");
            var empty = new DbModGroup { key = g.key, kind = g.kind, moveTo = g.moveTo };
            string again = ImportModScan.Move(empty, db, mods);
            True(again != to && Directory.Exists(again), "a second move makes a new folder (" + Path.GetFileName(again) + ")");
            Directory.Delete(again);
        }
        var after = ImportModScan.Scan(db);
        True(!after.Any(g => g.moveTo != null), "after moving: no movable packs left in Database");
        // a failed move leaves everything where it was
        var ghost = new DbModGroup { key = "DLC099", kind = DbModGroup.Kinds.GearPack, moveTo = "Gear" }; ghost.files.Add("MXTables_DLC008.dx9.database"); ghost.files.Add("does-not-exist.dx9.package");
        try { ImportModScan.Move(ghost, db, mods); failed++; Console.WriteLine("FAIL  a move of a missing file succeeded"); }
        catch (IOException) { True(File.Exists(Path.Combine(db, "MXTables_DLC008.dx9.database")), "a failed move puts the moved files back"); }
        // the import of this install, leaving out the unreadable table: Measure skips it too
        var plan = ReflexAssetImport.Measure(game, skip, CancellationToken.None);
        True(plan.fatalFile == null && plan.unreadable.Count == 0, "with the unreadable table left out, everything else reads (" + plan.fatalFile + " " + string.Join(";", plan.unreadable) + ")");
        modded = game; moddedSkip = skip;
    }
    static string modded; static List<string> moddedSkip;
    static bool KitLike(string dir) => Directory.GetFiles(dir, "MXTables_*.dx9.database").Length == 1 && Directory.GetFiles(dir, "MXTables_*.dx9.package").Length == 1;

    // ---------------------------------------------------------------------------------------------- the whole game

    static void Full(ImportPlan stockPlan)
    {
        string src = modded ?? stock; var skip = modded != null ? moddedSkip : null;
        var plan = modded != null ? ReflexAssetImport.Measure(src, skip, CancellationToken.None) : stockPlan;
        string dest = Path.Combine(work, "full", "game files");
        var sw = Stopwatch.StartNew();
        var r = Run(src, dest, new ReflexAssetImport.Options { plan = plan, skipFiles = skip });
        double min = sw.Elapsed.TotalMinutes;
        long all = Written(dest), files = WrittenNoCatalog(dest);
        Note($"full import of {(modded != null ? "the modded clone" : "the stock install")}: {r.files} files, {all:N0} B written ({ReflexAssetImport.GB(all, true)}) in {min:0.0} min ({all / 1048576.0 / sw.Elapsed.TotalSeconds:0} MB/s)");
        Note($"  measured: files+assets {plan.looseBytes + plan.assetBytes:N0} B vs written {files:N0} B (diff {plan.looseBytes + plan.assetBytes - files:N0}); catalog ~{plan.catalogBytes:N0} vs {all - files:N0}; Required(4 KB) {plan.Required(4096):N0}");
        long du = DiskUsage(dest); Note($"  on disk (du): {du:N0} B; Required(4 KB) - du = {plan.Required(4096) - du:N0}");
        foreach (var w in r.warnings) Note("  warning: " + (w.Length > 200 ? w.Substring(0, 200) + "…" : w));
        True(Math.Abs(plan.looseBytes + plan.assetBytes - files) < (64L << 20), "the measured size matches the written files (within the SX_FresnoB repair's difference)");
        True(plan.Required(4096) >= du, "Required covers the disk usage");
        var (lo, hi) = ReflexAssetImport.Minutes(plan); Note($"  estimate shown to the player: {lo}-{hi} min");
        ReflexAssetImport.DeleteTree(Path.Combine(work, "full"));
    }
    static long DiskUsage(string dir)
    {
        var psi = new ProcessStartInfo("du") { UseShellExecute = false, RedirectStandardOutput = true }; psi.ArgumentList.Add("-sk"); psi.ArgumentList.Add(dir);
        var p = Process.Start(psi);
        string o = p.StandardOutput.ReadToEnd(); p.WaitForExit();
        return long.Parse(o.Split('\t')[0]) * 1024;
    }
}
