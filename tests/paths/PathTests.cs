// Path tests: Windows-style paths through the pure path logic on any machine (see build.sh).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using Reflex.Port.Content;

static class PathTests
{
    static int passed, failed;
    static void Eq<T>(T got, T want, string what)
    {
        if (EqualityComparer<T>.Default.Equals(got, want)) { passed++; return; }
        failed++; Console.WriteLine("FAIL  " + what + "\n      got  " + (got == null ? "(null)" : got.ToString()) + "\n      want " + (want == null ? "(null)" : want.ToString()));
    }
    static void True(bool v, string what) => Eq(v, true, what);
    static void SeqEq(IEnumerable<string> got, IEnumerable<string> want, string what) => Eq(string.Join(" | ", got), string.Join(" | ", want), what);

    static string C(string s, bool win = true, string home = null, Func<string, string> env = null) => ReflexPaths.Clean(s, win, home, env);

    static int Main()
    {
        Clean(); Validate(); Operations(); Remap(); Files(); Steam(); Drives(); Finder(); Reparse(); Fs(); Songs();
        Console.WriteLine($"paths: {passed} passed, {failed} failed");
        return failed == 0 ? 0 : 1;
    }

    static void Clean()
    {
        // every drive letter, spaces, Unicode
        foreach (char l in "ABCDEFGHIJKLMNOPQRSTUVWXYZ") Eq(C(l + @":\Games\MX vs ATV Reflex"), l + @":\Games\MX vs ATV Reflex", "drive " + l);
        Eq(C(@"d:\games"), @"d:\games", "lower-case drive letter kept");
        Eq(C("\"E:\\Games\\X\\\""), @"E:\Games\X", "quotes and a trailing separator");
        Eq(C("\u201CE:\\Games\\X\u201D"), @"E:\Games\X", "smart quotes");
        Eq(C("  'F:\\a b\\c'  "), @"F:\a b\c", "single quotes and blanks");
        Eq(C("\"\"E:\\x\"\""), @"E:\x", "doubled quotes");
        Eq(C(@"d:/games/x/"), @"d:\games\x", "forward slashes");
        Eq(C(@"D:\Games//MX\\vs"), @"D:\Games\MX\vs", "doubled separators");
        Eq(C("E:"), @"E:\", "a bare drive is its root");
        Eq(C(@"E:\"), @"E:\", "a drive root stays");
        Eq(C(@"Z:\\\"), @"Z:\", "a drive root with many separators");
        Eq(C(@"E:/"), @"E:\", "drive root with a slash");
        Eq(C(@"D:\Jeux\Réflex ビデオ\Ünï"), @"D:\Jeux\Réflex ビデオ\Ünï", "non-ASCII names");
        Eq(C(@"C:\Users\Zoë\My Games"), @"C:\Users\Zoë\My Games", "non-ASCII user folder");
        // network
        Eq(C(@"\\server\share\"), @"\\server\share", "UNC share root");
        Eq(C("//server/share/Games/"), @"\\server\share\Games", "UNC with forward slashes");
        Eq(C(@"\\nas.local\Games\MX vs ATV"), @"\\nas.local\Games\MX vs ATV", "UNC with spaces");
        Eq(C(@"\\?\UNC\srv\share\g"), @"\\srv\share\g", "extended UNC prefix");
        Eq(C(@"\\?\D:\Games\x"), @"D:\Games\x", "extended drive prefix");
        Eq(C(@"\\.\E:\x"), @"E:\x", "device prefix");
        // URLs
        Eq(C("file:///D:/My%20Games/Reflex"), @"D:\My Games\Reflex", "file URL with a drive");
        Eq(C("file://nas/share/Games%20X"), @"\\nas\share\Games X", "file URL with a host");
        Eq(C("file://localhost/C:/x"), @"C:\x", "file URL with localhost");
        Eq(C("file:/D:/x"), @"D:\x", "file URL with one slash");
        Eq(C("file:///D:/R%C3%A9flex"), @"D:\Réflex", "file URL, UTF-8 escapes");
        Eq(C("file:///Users/ann/Games", false), "/Users/ann/Games", "file URL, POSIX");
        // variables and home
        Func<string, string> env = n => n.Equals("USERPROFILE", StringComparison.OrdinalIgnoreCase) ? @"C:\Users\Zoë" : n == "GAMES" ? @"G:\Games" : null;
        Eq(C(@"%USERPROFILE%\Games", true, null, env), @"C:\Users\Zoë\Games", "environment variable");
        Eq(C(@"%GAMES%\Reflex", true, null, env), @"G:\Games\Reflex", "variable on another drive");
        Eq(C(@"%UNKNOWN%\x", true, null, env), @"%UNKNOWN%\x", "unknown variable stays");
        Eq(C(@"~\Games", true, @"C:\Users\Zoë"), @"C:\Users\Zoë\Games", "home");
        Eq(C("~", true, @"D:\Home"), @"D:\Home", "bare ~");
        // POSIX hosts
        Eq(C("/Volumes/USB DRIVE/Games/", false), "/Volumes/USB DRIVE/Games", "macOS volume");
        Eq(C("/", false), "/", "root");
        Eq(C("", true), "", "empty");
        Eq(C(null, true), "", "null");
        Eq(C("   ", true), "", "blank");
    }

    static void Validate()
    {
        Eq(ReflexPaths.Validate(@"D:\Games\MX vs ATV Reflex", true), null, "a normal path");
        Eq(ReflexPaths.Validate(@"\\server\share\Games", true), null, "UNC");
        Eq(ReflexPaths.Validate(@"D:\Jeux\Réflex ビデオ", true), null, "Unicode");
        Eq(ReflexPaths.Validate(@"D:\", true), null, "a drive root");
        Eq(ReflexPaths.Validate("/Volumes/X", false), null, "POSIX");
        True(ReflexPaths.Validate("", true) != null, "empty");
        True(ReflexPaths.Validate(@"Games\Reflex", true).Contains("full path"), "relative path asks for a full path");
        True(ReflexPaths.Validate("Reflex", false).Contains("full path"), "relative path on macOS");
        True(ReflexPaths.Validate(@"D:\a|b", true).Contains("cannot contain"), "pipe");
        True(ReflexPaths.Validate(@"D:\a?b", true) != null, "question mark");
        True(ReflexPaths.Validate(@"D:\a*b", true) != null, "star");
        True(ReflexPaths.Validate(@"D:\a:b", true).Contains("colon"), "colon inside");
        True(ReflexPaths.Validate(@"D:\CON", true).Contains("reserved"), "device name");
        True(ReflexPaths.Validate(@"D:\Games\nul.txt", true).Contains("reserved"), "device name with extension");
        True(ReflexPaths.Validate(@"D:\Games\COM3", true).Contains("reserved"), "COM3");
        Eq(ReflexPaths.Validate(@"D:\Games\COM0x", true), null, "COM0x is a fine name");
        True(ReflexPaths.Validate(@"D:\" + new string('a', 256), true).Contains("255"), "a name over 255");
        True(ReflexPaths.Validate("D:Games", true).Contains("D:\\Games"), "drive-relative D:Games");
        True(ReflexPaths.Validate("D:\u0001x", true) != null, "control character");
        Eq(ReflexPaths.Validate(@"\\?\D:\x".Length > 0 ? C(@"\\?\D:\Games") : "", true), null, "extended prefix after Clean");
    }

    static void Operations()
    {
        // roots
        Eq(ReflexPaths.RootLength(@"D:\x"), 3, "root length drive");
        Eq(ReflexPaths.RootLength(@"D:"), 2, "root length bare drive");
        Eq(ReflexPaths.RootLength(@"\\srv\share\x"), 12, "root length UNC");
        Eq(ReflexPaths.RootLength(@"\\srv\share"), 11, "root length UNC share");
        Eq(ReflexPaths.RootLength("/x"), 1, "root length POSIX");
        Eq(ReflexPaths.RootLength("x/y"), 0, "root length relative");
        // parent
        Eq(ReflexPaths.Parent(@"C:\Games\Reflex"), @"C:\Games", "parent");
        Eq(ReflexPaths.Parent(@"C:\Games\Reflex\"), @"C:\Games", "parent of trailing separator");
        Eq(ReflexPaths.Parent(@"C:\Games"), @"C:\", "parent is the drive root");
        Eq(ReflexPaths.Parent("C:/Games"), "C:/", "parent keeps the style");
        Eq(ReflexPaths.Parent(@"C:\"), null, "a drive root has no parent");
        Eq(ReflexPaths.Parent(@"\\srv\share\a\b"), @"\\srv\share\a", "UNC parent");
        Eq(ReflexPaths.Parent(@"\\srv\share\a"), @"\\srv\share\", "UNC parent is the share");
        Eq(ReflexPaths.Parent(@"\\srv\share"), null, "a share root has no parent");
        Eq(ReflexPaths.Parent("/a/b"), "/a", "POSIX parent");
        Eq(ReflexPaths.Parent("/a"), "/", "POSIX root");
        Eq(ReflexPaths.Parent("/"), null, "POSIX top");
        Eq(ReflexPaths.Parent("a/b"), "a", "relative parent");
        Eq(ReflexPaths.Parent("a"), null, "relative top");
        // the client root of the player's data path, from the mods tests, plus every letter
        Eq(ReflexPaths.ClientRootFor(@"C:\Games\Open Reflex\OpenReflex_Data", false), @"C:\Games\Open Reflex", "client root");
        Eq(ReflexPaths.ClientRootFor(@"D:\OpenReflex_Data\", false), @"D:\", "client root at a drive root");
        Eq(ReflexPaths.ClientRootFor("Z:/OpenReflex_Data", false), "Z:/", "client root at Z:");
        Eq(ReflexPaths.ClientRootFor(@"\\server\share\Reflex\OpenReflex_Data", false), @"\\server\share\Reflex", "client root on a share");
        Eq(ReflexPaths.ClientRootFor(@"\\server\share\OpenReflex_Data", false), @"\\server\share\", "client root at a share root");
        Eq(ReflexPaths.ClientRootFor("/Users/ann/Games/Open Reflex/OpenReflex.app/Contents", true), "/Users/ann/Games/Open Reflex", "macOS client root");
        Eq(ReflexPaths.ClientRootFor(@"E:\Spiele für mich\Réflex\OpenReflex_Data", false), @"E:\Spiele für mich\Réflex", "client root, Unicode");
        Eq(ReflexPaths.ClientRootFor(@"D:\Old.app\OpenReflex_Data", false), @"D:\Old.app", "a folder named .app on Windows is not a bundle");
        // labels (a drive root used to be blank)
        Eq(ReflexPaths.LabelOf(@"E:\"), @"E:\", "label of a drive root");
        Eq(ReflexPaths.LabelOf("e:"), @"E:\", "label of a bare drive");
        Eq(ReflexPaths.LabelOf(@"E:\Games\MX Reflex\"), "MX Reflex", "label of a folder");
        Eq(ReflexPaths.LabelOf(@"\\srv\share\"), @"\\srv\share", "label of a share");
        Eq(ReflexPaths.LabelOf("/Volumes/USB"), "USB", "label of a volume");
        Eq(ReflexPaths.LabelOf("/"), "/", "label of /");
        Eq(ReflexPaths.LabelOf(@"D:\Mods\Réflex ビデオ.iso"), "Réflex ビデオ.iso", "label of a file");
        // comparison
        True(ReflexPaths.SamePath(@"D:\Games\Reflex", "d:/games/reflex/"), "same path: case, separators, trailing");
        True(ReflexPaths.SamePath(@"E:\", "e:/"), "same drive root");
        True(!ReflexPaths.SamePath(@"D:\Games", @"E:\Games"), "other drive is another path");
        True(ReflexPaths.IsUnder(@"D:\Games", @"d:\games\Reflex\x"), "under");
        True(!ReflexPaths.IsUnder(@"D:\Games", @"D:\Games"), "not under itself");
        True(!ReflexPaths.IsUnder(@"D:\Games", @"D:\Games2\x"), "a sibling with the same prefix is not under");
        True(ReflexPaths.IsUnder(@"D:\", @"D:\Games"), "under a drive root");
        True(!ReflexPaths.IsUnder(@"D:\", @"E:\Games"), "not under another drive's root");
        True(ReflexPaths.IsUnder(@"\\srv\share", @"\\srv\share\a"), "under a share");
        // relative ids (from the bike mods tests)
        Eq(ReflexPaths.RelativeId(@"C:\Games\Mods\Bikes", @"C:\Games\Mods\Bikes\My Stuff\New Bike Models"), "My Stuff/New Bike Models", "Windows id");
        Eq(ReflexPaths.RelativeId(@"C:\Games\Mods\Bikes\", @"c:/games/mods/bikes/Ünïcode Bikes\2010 pack"), "Ünïcode Bikes/2010 pack", "mixed separators and case");
        Eq(ReflexPaths.RelativeId(@"X:\Mods", @"X:\Mods\a"), "a", "id on X:");
        Eq(ReflexPaths.RelativeId("/a/Mods/Bikes", "/a/Mods/Bikes2/x"), null, "sibling");
        Eq(ReflexPaths.RelativeId("/a/Mods/Bikes", "/a/Mods/Bikes"), null, "root itself");
        // not found messages
        var ready = new[] { @"C:\", @"E:\" };
        True(ReflexPaths.NotFound(@"D:\Games", ready).StartsWith("Drive D: is not ready or not connected"), "missing drive named");
        True(ReflexPaths.NotFound(@"D:\Games", ready).Contains("C: E:"), "available drives listed");
        True(ReflexPaths.NotFound(@"E:\Games", ready).StartsWith("Folder not found"), "folder missing on a ready drive");
        True(ReflexPaths.NotFound(@"\\nas\games", ready).Contains("network"), "network location");
    }

    static void Remap()
    {
        var ready = new[] { @"C:\", @"D:\", @"F:\" };
        SeqEq(ReflexPaths.RemapDrive(@"E:\Games\Reflex", ready), new[] { @"C:\Games\Reflex", @"D:\Games\Reflex", @"F:\Games\Reflex" }, "remap to every other ready drive");
        SeqEq(ReflexPaths.RemapDrive(@"d:\Games\Reflex", ready), new[] { @"C:\Games\Reflex", @"F:\Games\Reflex" }, "own drive left out, case-insensitive");
        SeqEq(ReflexPaths.RemapDrive(@"\\srv\share\x", ready), new string[0], "UNC is not remapped");
        SeqEq(ReflexPaths.RemapDrive("/Users/x", ready), new string[0], "POSIX is not remapped");
        SeqEq(ReflexPaths.RemapDrive(@"E:\Spiele für\Réflex", new[] { @"G:\" }), new[] { @"G:\Spiele für\Réflex" }, "remap keeps Unicode and spaces");
    }

    static void Files()
    {
        Eq(ReflexPaths.FirstPathLine("\uFEFFD:\\Games\\Reflex\r\n", true), @"D:\Games\Reflex", "BOM and CRLF");
        Eq(ReflexPaths.FirstPathLine("# the game\r\n\r\n\"E:\\Spiele\\Réflex\"\r\nF:\\other", true), @"E:\Spiele\Réflex", "comments, blank lines, quotes, first line wins");
        Eq(ReflexPaths.FirstPathLine("; x\n\\\\nas\\games\\reflex\n", true), @"\\nas\games\reflex", "UNC in a text file");
        Eq(ReflexPaths.FirstPathLine("\n\n", true), null, "nothing");
        Eq(ReflexPaths.FirstPathLine("D:/Games/Reflex  \n", true), @"D:\Games\Reflex", "trailing blanks, slashes");
        True(ReflexPaths.IsReflexFolderName("MX vs. ATV Reflex"), "Steam folder name");
        True(ReflexPaths.IsReflexFolderName("MX vs ATV Reflex (Steam)"), "name with a suffix");
        True(ReflexPaths.IsReflexFolderName("mx vs atv reflex"), "lower case");
        True(!ReflexPaths.IsReflexFolderName("MX vs. ATV Unleashed"), "Unleashed is another game");
        True(!ReflexPaths.IsReflexFolderName("Reflex"), "a bare word");
    }

    static void Steam()
    {
        string vdf = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t\t\"label\"\t\t\"\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"1\"\t\t\"2\"\n\t\t}\n\t}\n" +
                     "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"E:\\\\Spiele\\\\SteamBibliothek\"\n\t}\n\t\"2\"\n\t{\n\t\t\"path\"\t\t\"\\\\\\\\nas\\\\steam\"\n\t}\n}\n";
        SeqEq(ReflexPaths.SteamLibraries(vdf), new[] { @"C:\Program Files (x86)\Steam", @"E:\Spiele\SteamBibliothek", @"\\nas\steam" }, "libraryfolders.vdf (new format)");
        string old = "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\"1\"\n\t\"1\"\t\t\"D:\\\\SteamLibrary\"\n\t\"2\"\t\t\"G:\\\\Games\\\\Steam\"\n}\n";
        SeqEq(ReflexPaths.SteamLibraries(old), new[] { @"D:\SteamLibrary", @"G:\Games\Steam" }, "libraryfolders.vdf (old format)");
        SeqEq(ReflexPaths.SteamLibraries(null), new string[0], "no vdf");
        SeqEq(ReflexPaths.SteamLibraries("\"0\" \"relative\\\\x\""), new string[0], "a relative entry is ignored");
    }

    // ---------------------------------------------------------------------------------------------- drives

    sealed class FakeDrives : IDriveSource
    {
        public List<DriveEntry> All = new List<DriveEntry>();
        public Dictionary<string, Func<string>> Probes = new Dictionary<string, Func<string>>();
        public int Calls;
        public List<DriveEntry> List() => All.Select(d => new DriveEntry { Root = d.Root, Kind = d.Kind }).ToList();
        public string Probe(DriveEntry d) { Interlocked.Increment(ref Calls); return Probes[d.Root](); }
        public void Add(string root, string kind, Func<string> probe) { All.Add(new DriveEntry { Root = root, Kind = kind }); Probes[root] = probe; }
    }

    static void Drives()
    {
        var f = new FakeDrives();
        f.Add(@"C:\", "DRIVE", () => "Windows");
        f.Add(@"D:\", "DISC", () => null);                                          // empty optical drive
        f.Add(@"E:\", "DISC", () => "MX_ATV_REFLEX");                                // a mounted ISO or a game disc
        f.Add(@"F:\", "USB / REMOVABLE", () => "");                                  // a USB stick without a label
        f.Add(@"G:\", "NETWORK", () => { Thread.Sleep(8000); return "NAS"; });       // an unreachable network drive
        f.Add(@"H:\", "DRIVE", () => throw new System.IO.IOException("device not ready"));
        f.Add(@"Z:\", "NETWORK", () => "Mapped");                                    // the last letter
        var scanner = new DriveScanner(f);
        var sw = Stopwatch.StartNew();
        var ready = scanner.Ready(timeoutMs: 700, ttlMs: 0);
        long ms = sw.ElapsedMilliseconds;
        SeqEq(ready.Select(d => d.Root), new[] { @"C:\", @"E:\", @"F:\", @"Z:\" }, "ready drives on any letters, not-ready / hung / throwing left out");
        True(ms < 1500, "a hung network drive does not hold the scan (" + ms + " ms)");
        Eq(ready.First(d => d.Root == @"E:\").Label, "MX_ATV_REFLEX", "label of the mounted ISO");
        Eq(ready.First(d => d.Root == @"E:\").Kind, "DISC", "kind of the mounted ISO");
        SeqEq(scanner.Skipped.OrderBy(x => x), new[] { @"D:\", @"G:\", @"H:\" }, "skipped drives named");
        // the hung drive is not probed again for a while; the cache is reused
        int calls = f.Calls;
        scanner.Ready(timeoutMs: 700, ttlMs: 60000);
        Eq(f.Calls, calls, "cache reused");
        sw.Restart();
        scanner.Ready(timeoutMs: 700, ttlMs: 0);
        True(sw.ElapsedMilliseconds < 400, "a drive that hung is left out of the next scan at once (" + sw.ElapsedMilliseconds + " ms)");
        // a disc inserted later
        bool inserted = false;
        var g = new FakeDrives(); g.Add(@"D:\", "DISC", () => inserted ? "DISC" : null); g.Add(@"C:\", "DRIVE", () => "C");
        var s2 = new DriveScanner(g);
        Eq(s2.Ready(500, 60000).Count, 1, "disc not in yet");
        inserted = true; s2.Invalidate();
        Eq(s2.Ready(500, 60000).Count, 2, "disc in after Invalidate");
        // all 26 letters answering together
        var big = new FakeDrives();
        foreach (char l in "ABCDEFGHIJKLMNOPQRSTUVWXYZ") { char c = l; big.Add(c + @":\", "DRIVE", () => { Thread.Sleep(150); return "v" + c; }); }
        sw.Restart();
        var all = new DriveScanner(big).Ready(2000, 0);
        Eq(all.Count, 26, "all letters");
        True(sw.ElapsedMilliseconds < 1000, "26 drives are asked in parallel (" + sw.ElapsedMilliseconds + " ms)");
        Eq(all[0].Root, @"A:\", "sorted by letter");
    }

    // ---------------------------------------------------------------------------------------------- install finder

    sealed class FakeFs : IFinderFs
    {
        readonly HashSet<string> dirs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string, string> texts = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        static string N(string p) { p = p.Replace('/', '\\'); return p.Length > 3 ? p.TrimEnd('\\') : p; }
        public void Dir(string p) { p = N(p); for (string x = p; x != null; x = ReflexPaths.Parent(x)) dirs.Add(N(x)); }
        public void Text(string p, string t) { texts[N(p)] = t; Dir(ReflexPaths.Parent(N(p))); }
        public bool DirExists(string p) => dirs.Contains(N(p));
        public IEnumerable<string> SubDirs(string p)
        {
            p = N(p);
            return dirs.Where(d => d.Length > p.Length && ReflexPaths.SamePath(ReflexPaths.Parent(d), p) && !d.Equals(p, StringComparison.OrdinalIgnoreCase)).ToList();
        }
        public string ReadText(string p) => texts.TryGetValue(N(p), out var t) ? t : null;
    }

    static void Finder()
    {
        var fs = new FakeFs();
        fs.Dir(@"C:\Windows"); fs.Dir(@"C:\Program Files (x86)\Steam\steamapps\common\Some Game");
        fs.Text(@"C:\Program Files (x86)\Steam\steamapps\libraryfolders.vdf",
                "\"libraryfolders\"\n{\n\"0\"\n{\n\"path\" \"C:\\\\Program Files (x86)\\\\Steam\"\n}\n\"1\"\n{\n\"path\" \"K:\\\\Meine Spiele\\\\Steam Bibliothek\"\n}\n}\n");
        fs.Dir(@"K:\Meine Spiele\Steam Bibliothek\steamapps\common\MX vs. ATV Reflex\Database");          // Steam library on an odd drive, odd folders
        fs.Dir(@"E:\Database");                                                                         // a mounted game disc / ISO: Database at the drive root
        fs.Dir(@"F:\Games\MX vs ATV Reflex\Database"); fs.Dir(@"F:\Games\MX vs ATV Reflex\MXReflex.exe_");
        fs.Dir(@"G:\Spiele"); fs.Dir(@"Z:\MX vs. ATV Reflex\Archive\Database");                         // the extracted original's Archive folder, last letter
        fs.Dir(@"H:\Games\MX vs. ATV Unleashed\Database");                                               // another game: not Reflex
        fs.Dir(@"D:\SteamLibrary\steamapps\common\MX vs. ATV Reflex");                                   // Steam folder without Database: not an install
        Func<string, bool> isInstall = p => fs.DirExists(p.TrimEnd('\\') + "\\Database");
        var roots = new[] { @"C:\", @"D:\", @"E:\", @"F:\", @"G:\", @"H:\", @"K:\", @"Z:\" };
        var found = InstallFinder.Find(roots, fs, isInstall);
        var paths = found.Select(x => x.Path).ToList();
        True(paths.Contains(@"E:\"), "game disc at a drive root (" + string.Join(", ", paths) + ")");
        True(paths.Contains(@"F:\Games\MX vs ATV Reflex"), "Games folder on F:");
        True(paths.Contains(@"K:\Meine Spiele\Steam Bibliothek\steamapps\common\MX vs. ATV Reflex"), "Steam library listed by libraryfolders.vdf on K:");
        True(paths.Contains(@"Z:\MX vs. ATV Reflex\Archive"), "Archive folder on Z:");
        True(!paths.Any(p => p.Contains("Unleashed")), "another game is not offered");
        True(!paths.Any(p => p.StartsWith(@"D:\")), "a folder without Database is not offered");
        Eq(found.First(x => x.Path == @"E:\").Source, "DRIVE ROOT", "source of the disc");
        Eq(found.First(x => x.Path.StartsWith("K:")).Source, "STEAM", "source of the Steam install");
        Eq(paths.Distinct(StringComparer.OrdinalIgnoreCase).Count(), paths.Count, "no duplicates");
        Eq(paths.Count, 4, "exactly the four installs");
        // a Steam library on a drive that is not in the default list, found through the extra root (the registry's SteamPath)
        var fs2 = new FakeFs();
        fs2.Dir(@"R:\Programme\Steam\steamapps\common\MX vs. ATV Reflex\Database");
        Eq(InstallFinder.Find(new[] { @"C:\" }, fs2, p => fs2.DirExists(p + "\\Database"), new[] { "r:/Programme/Steam" }).Count, 1, "Steam path from the registry on R:");
        Eq(InstallFinder.Find(new string[0], fs2, p => fs2.DirExists(p + "\\Database")).Count, 0, "no drives, nothing found");
    }

    static void Reparse()
    {
        True(ReflexPaths.IsLinkTag(ReflexPaths.ReparseSymlink, false), "symbolic link");
        True(ReflexPaths.IsLinkTag(ReflexPaths.ReparseMountPoint, false), "junction");
        True(!ReflexPaths.IsLinkTag(ReflexPaths.ReparseMountPoint, true), "volume mounted into a folder is a real place");
        True(!ReflexPaths.IsLinkTag(0x9000001A, false), "OneDrive cloud placeholder is a real folder");
        True(!ReflexPaths.IsLinkTag(0x80000017, false), "dedup reparse point is a real folder");
    }

    static void Fs()
    {
        // a folder check that does not answer (an offline network location) is given up on
        var sw = Stopwatch.StartNew();
        int r = ReflexFs.Within(200, () => { Thread.Sleep(3000); return 7; }, out bool to);
        True(to && r == 0 && sw.ElapsedMilliseconds < 1500, "a hung check times out (" + sw.ElapsedMilliseconds + " ms)");
        Eq(ReflexFs.Within(2000, () => 42, out to), 42, "a quick check answers"); True(!to, "no timeout for a quick check");
        try { ReflexFs.Within<int>(2000, () => throw new System.IO.IOException("x"), out _); failed++; Console.WriteLine("FAIL  exception passes through"); }
        catch (System.IO.IOException) { passed++; }
        // links: a symbolic link is one, a real folder / a file / nothing is not
        string dir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "paths-test-" + Guid.NewGuid().ToString("N").Substring(0, 8));
        System.IO.Directory.CreateDirectory(System.IO.Path.Combine(dir, "real", "sub"));
        try
        {
            System.IO.File.WriteAllText(System.IO.Path.Combine(dir, "real", "f.txt"), "x");
            System.IO.Directory.CreateSymbolicLink(System.IO.Path.Combine(dir, "link"), System.IO.Path.Combine(dir, "real"));
            True(ReflexFs.IsLink(System.IO.Path.Combine(dir, "link")), "symbolic link");
            True(!ReflexFs.IsLink(System.IO.Path.Combine(dir, "real")), "real folder");
            True(!ReflexFs.IsLink(System.IO.Path.Combine(dir, "real", "f.txt")), "file");
            True(!ReflexFs.IsLink(System.IO.Path.Combine(dir, "nothing")), "missing path");
            True(!ReflexFs.IsLink(System.IO.Path.GetPathRoot(dir)), "the root of a drive");
            { string la = ReflexFs.LinkedAncestor(System.IO.Path.Combine(dir, "real", "sub")); True(la == null || !la.StartsWith(dir), "no linked ancestor inside the folder (the temp folder itself may sit behind /var)"); }
            True(ReflexFs.LinkedAncestor(System.IO.Path.Combine(dir, "link", "sub")) is string a && a.EndsWith("link"), "a path through a link names it");
        }
        finally { try { System.IO.Directory.Delete(dir, true); } catch (Exception) { } }
    }

    // ------------------------------------------------------------------------------------------------ YOUR MUSIC (Content/ReflexSongLibrary.cs)
    // Synthetic tags only, generated here (no music in the repository): ID3v2.2 / 2.3 / 2.4, ID3v1, Ogg Vorbis comments, file names, the folder scan, the queue.
    static byte[] BE32(int v) => new[] { (byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v };
    static byte[] Sync(int v) => new[] { (byte)(v >> 21 & 0x7f), (byte)(v >> 14 & 0x7f), (byte)(v >> 7 & 0x7f), (byte)(v & 0x7f) };
    static byte[] Cat(params byte[][] parts) => parts.SelectMany(x => x).ToArray();
    static byte[] Asc(string s) => Encoding.ASCII.GetBytes(s);
    static byte[] Frame(int ver, string id, byte enc, byte[] text)
    {
        var data = Cat(new[] { enc }, text);
        byte[] size = ver == 2 ? new[] { (byte)(data.Length >> 16), (byte)(data.Length >> 8), (byte)data.Length } : ver >= 4 ? Sync(data.Length) : BE32(data.Length);
        return Cat(Asc(id), size, ver == 2 ? new byte[0] : new byte[2], data);
    }
    static byte[] Id3Tag(int ver, bool extHeader, params byte[][] frames)
    {
        var body = Cat(frames);
        byte[] ext = !extHeader ? new byte[0] : ver >= 4 ? Cat(Sync(6), new byte[] { 1, 0 }) : Cat(BE32(6), new byte[] { 0, 0 }, BE32(0));
        var all = Cat(ext, body);
        return Cat(Asc("ID3"), new byte[] { (byte)ver, 0, (byte)(extHeader ? 0x40 : 0) }, Sync(all.Length), all);
    }
    static byte[] U16(string s, bool bom = true) => Cat(bom ? new byte[] { 0xFF, 0xFE } : new byte[0], Encoding.Unicode.GetBytes(s));
    static byte[] Id3v1(string title, string artist, string album, string year)
    {
        byte[] F(string v, int n) { var b = new byte[n]; var a = Encoding.Latin1.GetBytes(v); Array.Copy(a, b, Math.Min(n, a.Length)); return b; }
        return Cat(Asc("TAG"), F(title, 30), F(artist, 30), F(album, 30), F(year, 4), new byte[31]);
    }
    static byte[] OggComments(string vendor, params string[] kv)
    {
        var parts = new List<byte[]> { Asc("OggS"), new byte[50], new byte[] { 3 }, Asc("vorbis"), BitConverter.GetBytes(vendor.Length), Asc(vendor), BitConverter.GetBytes(kv.Length) };
        foreach (var c in kv) { var u = Encoding.UTF8.GetBytes(c); parts.Add(BitConverter.GetBytes(u.Length)); parts.Add(u); }
        parts.Add(new byte[] { 1 });
        return Cat(parts.ToArray());
    }
    static ReflexSong Tagged(byte[] file, string ext)
    {
        string path = Path.Combine(Path.GetTempPath(), "rxsong_" + Guid.NewGuid().ToString("N") + ext);
        File.WriteAllBytes(path, file);
        try { return ReflexSongLibrary.ReadTags(path); } finally { File.Delete(path); }
    }
    static void Tags(string what, ReflexSong s, string title, string artist, string album, int year)
    {
        Eq(s.title, title, what + " title"); Eq(s.artist, artist, what + " artist"); Eq(s.album, album, what + " album"); Eq(s.year, year, what + " year");
    }

    static void Songs()
    {
        // file names
        var n = ReflexSongLibrary.FromFileName(@"C:\Music\Rock\Artist Name - Song Title.mp3"); Eq(n.artist, "Artist Name", "name artist"); Eq(n.title, "Song Title", "name title");
        n = ReflexSongLibrary.FromFileName("/m/01 - Some Band - A Song.ogg"); Eq(n.artist, "Some Band", "track number dropped: artist"); Eq(n.title, "A Song", "track number dropped: title");
        n = ReflexSongLibrary.FromFileName("/m/7. Nation Army.wav"); Eq(n.artist, null, "'7. ' dropped, no artist"); Eq(n.title, "Nation Army", "'7. ' dropped");
        n = ReflexSongLibrary.FromFileName("/m/99 Problems.mp3"); Eq(n.title, "99 Problems", "a number that is the title stays");
        n = ReflexSongLibrary.FromFileName("/m/1999 - Prince.mp3"); Eq(n.artist, "1999", "a four-digit number is not a track number");
        n = ReflexSongLibrary.FromFileName("/m/Band_-_Song_Name.mp3"); Eq(n.artist, "Band", "underscores are blanks: artist"); Eq(n.title, "Song Name", "underscores are blanks: title");
        n = ReflexSongLibrary.FromFileName("/m/Just A Title.mp3"); Eq(n.artist, null, "no ' - ': no artist"); Eq(n.title, "Just A Title", "no ' - ': the title is the name");
        n = ReflexSongLibrary.FromFileName("/m/Trailing - .mp3"); Eq(n.title, "Trailing -", "a dash without a title is part of the title");
        Eq(ReflexSongLibrary.IsSong("a.MP3") && ReflexSongLibrary.IsSong("a.Ogg") && ReflexSongLibrary.IsSong("x.wav"), true, "song extensions in any case");
        Eq(ReflexSongLibrary.IsSong("a.flac") || ReflexSongLibrary.IsSong("a.mp3.txt") || ReflexSongLibrary.IsSong("mp3"), false, "other files are not songs");
        Eq(ReflexSongLibrary.FileName(@"C:\a\b.mp3"), "b.mp3", "file name, backslash"); Eq(ReflexSongLibrary.FileName("/a/b.mp3"), "b.mp3", "file name, slash");
        // ID3v2.3, Latin-1 and UTF-16
        Tags("id3v2.3", Tagged(Id3Tag(3, false, Frame(3, "TIT2", 0, Asc("Title One")), Frame(3, "TPE1", 0, Asc("The Artist")), Frame(3, "TALB", 0, Asc("An Album")), Frame(3, "TYER", 0, Asc("1999"))), ".mp3"), "Title One", "The Artist", "An Album", 1999);
        Tags("id3v2.3 utf-16", Tagged(Id3Tag(3, false, Frame(3, "TIT2", 1, U16("Dänish — Ünï")), Frame(3, "TPE1", 1, U16("Zoë"))), ".mp3"), "Dänish — Ünï", "Zoë", null, 0);
        Tags("id3v2.3 latin-1", Tagged(Id3Tag(3, false, Frame(3, "TIT2", 0, new byte[] { 0x43, 0x61, 0x66, 0xE9 })), ".mp3"), "Café", null, null, 0);
        Tags("id3v2.3 utf-16 big-endian BOM", Tagged(Id3Tag(3, false, Frame(3, "TIT2", 1, Cat(new byte[] { 0xFE, 0xFF }, Encoding.BigEndianUnicode.GetBytes("Béé")))), ".mp3"), "Béé", null, null, 0);
        // ID3v2.4: UTF-8, syncsafe sizes, TDRC with a full date, an extended header, a frame that is not text, a multi-value text
        Tags("id3v2.4", Tagged(Id3Tag(4, false, Frame(4, "TIT2", 3, Encoding.UTF8.GetBytes("日本語の曲")), Frame(4, "TPE1", 3, Encoding.UTF8.GetBytes("First\0Second")), Frame(4, "TDRC", 3, Asc("2008-03-01")), Frame(4, "APIC", 0, new byte[300])), ".mp3"),
             "日本語の曲", "First", null, 2008);
        Tags("id3v2.4 extended header", Tagged(Id3Tag(4, true, Frame(4, "TIT2", 3, Asc("Ext"))), ".mp3"), "Ext", null, null, 0);
        Tags("id3v2.3 extended header", Tagged(Id3Tag(3, true, Frame(3, "TIT2", 0, Asc("Ext3"))), ".mp3"), "Ext3", null, null, 0);
        // ID3v2.2
        Tags("id3v2.2", Tagged(Id3Tag(2, false, Frame(2, "TT2", 0, Asc("Old Title")), Frame(2, "TP1", 0, Asc("Old Artist")), Frame(2, "TAL", 0, Asc("Old Album")), Frame(2, "TYE", 0, Asc("1985"))), ".mp3"), "Old Title", "Old Artist", "Old Album", 1985);
        // a big picture before the text frames is skipped, not read
        Tags("big picture first", Tagged(Id3Tag(3, false, Frame(3, "APIC", 0, new byte[3 * 1024 * 1024]), Frame(3, "TIT2", 0, Asc("After Art"))), ".mp3"), "After Art", null, null, 0);
        // damaged tags fall back to the name; ID3v1 at the end; a tag with no text frame falls back to ID3v1
        var named = Path.Combine(Path.GetTempPath(), "rxsong_Fallback Artist - Fallback Title.mp3");
        File.WriteAllBytes(named, Cat(Asc("ID3"), new byte[] { 3, 0, 0 }, Sync(2000), new byte[40]));
        var bad = ReflexSongLibrary.ReadTags(named); File.Delete(named);
        Eq(bad.title, "Fallback Title", "a damaged ID3v2 falls back to the name"); Eq(bad.artist, "rxsong Fallback Artist", "(the whole name before ' - ', underscores as blanks)");
        Tags("id3v1", Tagged(Cat(new byte[2000], Id3v1("V1 Title", "V1 Artist", "V1 Album", "2001")), ".mp3"), "V1 Title", "V1 Artist", "V1 Album", 2001);
        Tags("id3v2 without text, id3v1 after", Tagged(Cat(Id3Tag(3, false, Frame(3, "APIC", 0, new byte[20])), new byte[500], Id3v1("Later", "Tag", "", "")), ".mp3"), "Later", "Tag", null, 0);
        // Ogg Vorbis comments
        Tags("vorbis", Tagged(OggComments("libVorbis 1.3", "TITLE=Ogg Title", "artist=Ogg Artist", "ALBUM=Ogg Album", "DATE=2012-06-30", "TRACKNUMBER=4"), ".ogg"), "Ogg Title", "Ogg Artist", "Ogg Album", 2012);
        var oggNameless = Path.Combine(Path.GetTempPath(), "rxsong_Ogg Name.ogg"); File.WriteAllBytes(oggNameless, new byte[600]);
        var og = ReflexSongLibrary.ReadTags(oggNameless); File.Delete(oggNameless); Eq(og.title, "rxsong Ogg Name", "an ogg without comments: the name");
        // hostile bytes never throw
        var rnd = new Random(7); int survived = 0;
        for (int i = 0; i < 300; i++)
        {
            var junk = new byte[rnd.Next(0, 400)]; rnd.NextBytes(junk);
            if (i % 3 == 0 && junk.Length > 10) { junk[0] = (byte)'I'; junk[1] = (byte)'D'; junk[2] = (byte)'3'; junk[3] = (byte)(2 + i % 3); }
            if (i % 3 == 1 && junk.Length > 20) { Array.Copy(Cat(new byte[] { 3 }, Asc("vorbis"), BitConverter.GetBytes(rnd.Next(-5, 100000))), 0, junk, 2, 11); }
            Tagged(junk, i % 2 == 0 ? ".mp3" : ".ogg"); survived++;
        }
        Eq(survived, 300, "300 random files read without throwing");

        // the folder scan
        string root = Path.Combine(Path.GetTempPath(), "rxsongs_" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(Path.Combine(root, "Sub", "Deeper")); Directory.CreateDirectory(Path.Combine(root, ".git")); Directory.CreateDirectory(Path.Combine(root, "__MACOSX"));
            void W(string rel, byte[] b) => File.WriteAllBytes(Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar)), b);
            W("b - second.mp3", Id3Tag(3, false, Frame(3, "TIT2", 0, Asc("Tagged Title")), Frame(3, "TPE1", 0, Asc("Tagged Artist"))));
            W("A - first.wav", new byte[100]);
            W("Sub/c.ogg", OggComments("v", "TITLE=Sub Ogg"));
            W("Sub/Deeper/d.mp3", new byte[10]);
            W(".hidden.mp3", new byte[10]); W("._applekit.mp3", new byte[10]); W(".git/x.mp3", new byte[10]); W("__MACOSX/y.mp3", new byte[10]);
            W("notes.txt", new byte[5]); W("cover.jpg", new byte[5]); W("Thumbs.db", new byte[5]); W("Sub/song.flac", new byte[5]);
            var cache = new Dictionary<string, ReflexSong>();
            Func<string, bool> junk = name => string.IsNullOrEmpty(name) || name.StartsWith(".") || string.Equals(name, "__MACOSX", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "Thumbs.db", StringComparison.OrdinalIgnoreCase);
            var list = ReflexSongLibrary.Scan(root, junk, cache);
            SeqEq(list.Select(x => ReflexSongLibrary.FileName(x.path)), new[] { "A - first.wav", "b - second.mp3", "c.ogg", "d.mp3" }, "the scan: songs only, files before subfolders, no junk");
            Eq(list[1].title, "Tagged Title", "the scan reads the tags"); Eq(list[2].title, "Sub Ogg", "the scan reads a subfolder's ogg comments");
            Eq(list[0].artist, "A", "the scan falls back to the name");
            Eq(cache.Count, 4, "the cache holds each song");
            var again = ReflexSongLibrary.Scan(root, junk, cache);
            True(ReferenceEquals(again[1], list[1]), "an unchanged file is not read again");
            W("b - second.mp3", Id3Tag(3, false, Frame(3, "TIT2", 0, Asc("Retagged!!"))));
            var third = ReflexSongLibrary.Scan(root, junk, cache);
            Eq(third[1].title, "Retagged!!", "a changed file is read again");
            Eq(ReflexSongLibrary.Scan(Path.Combine(root, "missing"), junk).Count, 0, "a missing folder is empty"); Eq(ReflexSongLibrary.Scan(null).Count, 0, "no folder is empty");
            Eq(ReflexSongLibrary.Scan(Path.Combine(root, "b - second.mp3")).Count, 0, "a file instead of a folder is empty");
            Eq(ReflexSongLibrary.Scan(root).Count, 4, "the default junk rule (dot names)");
        }
        finally { try { Directory.Delete(root, true); } catch (IOException) { } }

        // the queue
        ReflexSong S(string path, string artist) => new ReflexSong { path = path, artist = artist, title = path, source = ReflexSong.SourceCustom };
        var songs = Enumerable.Range(0, 10).Select(i => S("/m/s" + i + ".mp3", "artist" + i)).ToList();
        var q = ReflexSongLibrary.ShuffleQueue(songs, new List<string>(), null, new Random(1));
        Eq(q.Count, 10, "queue: every song once"); Eq(q.Distinct().Count(), 10, "queue: no repeats");
        // the recent ones come last, the longest ago first; the song playing never opens a round
        var recent = new List<string> { songs[4].Key, songs[7].Key, songs[2].Key };
        q = ReflexSongLibrary.ShuffleQueue(songs, recent, songs[0].path, new Random(2));
        SeqEq(q.Skip(7), new[] { songs[4].path, songs[7].path, songs[2].path }, "queue: the recent ones last, oldest first");
        True(q[0] != songs[0].path, "queue: the song playing does not open the next round");
        for (int seed = 0; seed < 50; seed++)
            if (ReflexSongLibrary.ShuffleQueue(songs, new List<string>(), songs[3].path, new Random(seed))[0] == songs[3].path) { Eq(seed, -1, "queue: last first at seed"); break; }
        // no artist twice in a row where another can follow
        var dup = new List<ReflexSong>(); for (int i = 0; i < 12; i++) dup.Add(S("/m/d" + i + ".mp3", i % 3 == 0 ? "Same Band" : "other" + i));
        bool adjacent = false;
        for (int seed = 0; seed < 100 && !adjacent; seed++)
        {
            var dq = ReflexSongLibrary.ShuffleQueue(dup, new List<string>(), null, new Random(seed)).Select(p => dup.First(x => x.path == p).artist).ToList();
            for (int i = 1; i < dq.Count; i++) if (dq[i] == dq[i - 1] && dq[i] == "Same Band") adjacent = true;
        }
        Eq(adjacent, false, "queue: the same artist is not played twice in a row (100 shuffles)");
        Eq(ReflexSongLibrary.ShuffleQueue(new List<ReflexSong>(), null, null, new Random(1)).Count, 0, "queue: no songs, no queue");
        var one = ReflexSongLibrary.ShuffleQueue(new[] { S("/m/only.mp3", "x") }, new List<string>(), "/m/only.mp3", new Random(1)); SeqEq(one, new[] { "/m/only.mp3" }, "queue: a lone song repeats");
        Eq(ReflexSongLibrary.ShuffleQueue(new[] { S("/m/a.mp3", "x"), S("/m/A.MP3", "y"), null }, null, null, new Random(1)).Count, 1, "queue: a path twice (any case) counts once, null is skipped");
        // the memory
        var mem = new List<string> { "a", "b", "c" };
        ReflexSongLibrary.Heard(mem, "a"); SeqEq(mem, new[] { "b", "c", "a" }, "memory: a song heard again goes last");
        for (int i = 0; i < 400; i++) ReflexSongLibrary.Heard(mem, "k" + i);
        Eq(mem.Count, 300, "memory: at most 300 keys"); Eq(mem[299], "k399", "memory: the newest last");
        // keys are the source and the lower-cased file name
        Eq(S(@"C:\Music\Song.MP3", "x").Key, "YOUR MUSIC|song.mp3", "key: source and file name, either separator");
        Eq(new ReflexSong { artist = "A", title = "T" }.Label, "A - T", "label with an artist"); Eq(new ReflexSong { title = "T" }.Label, "T", "label without");
    }
}
