// TEST ONLY (tests/untamed_import/import): the MX vs ATV Untamed import (Content/UntamedImport/UntamedImporter.Import.cs) outside Unity,
// from the player's disc and a Reflex library into a scratch Mods/Tracks. Nothing is written outside --scratch; the disc and the library
// are only read (their files' sizes and times are compared at the end).
//   quick   (default) two stadiums, Norton (supercross) and Lincoln (EnduroCross):
//             table        UntamedImporter's tested-build hashes = tests/untamed_convert/build/expected_sha256.txt
//             library      a library without SX_FortDodge, without the EnduroCross water's level, an empty one: the clear error, nothing written
//             scan cancel  a scan cancelled from its progress callback: the disc image is closed at once
//             space, path  too little free space; (Windows) a tracks folder too deep: the clear error, nothing written
//             in the way   Mods/Tracks/Untamed is a file: the clear error before anything is converted; folders of the player's own
//                          where a track goes (an empty one, one next to a hand-installed copy, a file): the scan flags each
//                          (inTheWay) and the import refuses it with the same sentence, without converting
//             cancel       cancelled inside the first stadium: both Cancelled, the tracks folder as it was
//             import       both imported: the folders, the manifest (no path in it), the checks, the scan's view of them, the
//                          hand-installed Untamed/norton-v3 untouched, no staging folder left
//             again        without 'replace': AlreadyImported, nothing rewritten
//             failure      re-import with a failure injected into Norton: Norton Failed and its old track intact, Lincoln replaced
//             cancel 2     re-import cancelled inside the second stadium: the first replaced (the desktops' files in it gone with
//                          it), the second's old track intact
//             in use       (Windows) a re-import of a track whose package is open: refused before it is converted, untouched
//             changed      a track changed by the player, a file / a dot-named file / a dot-named folder of the player's in it, a
//                          folder not made by the importer: refused, untouched
//             crash        a staging folder left by a crashed import (with a replaced track in it, and a link to a folder outside):
//                          removed, the track put back, the folder outside as it was; one whose lock is held: left
//             disc         (with --folder) a copy of Lincoln's files with one package byte changed: refused; without Fairview: refused
//   all     the 12 stadiums into an empty tracks folder: time, size, every read of the library (UntamedImporter's list of needed
//           stock files must name them), the longest staged path against UntamedImporter.WorkPathBudget
// Arguments: [quick|all] --iso <disc image> (or UX_DISC_ISO) --reflex <Reflex install or game files> (or UX_REFLEX / UX_CLIENT_LIB)
//            --scratch <dir> [--folder <extracted disc>] [--expected <expected_sha256.txt>] [--threads N]
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

/// A library that records what is read of it and can hide levels (a player's library without them).
sealed class ProbeLibrary : IUxLibrary
{
    readonly IUxLibrary _inner;
    public readonly HashSet<string> hidden = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    public readonly SortedSet<string> levels = new SortedSet<string>(StringComparer.Ordinal), roots = new SortedSet<string>(StringComparer.Ordinal);
    public ProbeLibrary(IUxLibrary inner) { _inner = inner; }
    public string Describe => _inner.Describe;
    public bool HasLevel(string level) { if (hidden.Contains(level)) return false; bool has = _inner.HasLevel(level); lock (levels) levels.Add((has ? "" : "(absent) ") + level); return has; }
    public byte[] ReadLevelFile(string level, string kind) { UxBin.Require(!hidden.Contains(level), "hidden " + level); lock (levels) levels.Add(level); return _inner.ReadLevelFile(level, kind); }
    public byte[] ReadRootFile(string relative) { if (hidden.Contains(relative)) return null; lock (roots) roots.Add(relative); return _inner.ReadRootFile(relative); }
    public UxTrackSet OpenLevel(string level) { UxBin.Require(!hidden.Contains(level), "stock level " + level + " not in " + Describe); lock (levels) levels.Add(level); return _inner.OpenLevel(level); }
    public List<string> Levels() => _inner.Levels().Where(l => !hidden.Contains(l)).ToList();
    public void Dispose() { }
}

static class Program
{
    static string F(double s) => s.ToString("0.0", CultureInfo.InvariantCulture);
    static string Mb(long b) => F(b / 1048576.0);
    static int Threads;

    /// Every file under a folder (or the file itself): relative path -> size, write time and (hash: true) sha256.
    static SortedDictionary<string, string> Snapshot(string path, bool hash = false)
    {
        var r = new SortedDictionary<string, string>(StringComparer.Ordinal);
        if (File.Exists(path)) { var fi = new FileInfo(path); r[fi.Name] = fi.Length + "|" + fi.LastWriteTimeUtc.Ticks; return r; }
        if (!Directory.Exists(path)) return r;
        string top = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar);
        foreach (string d in Directory.EnumerateDirectories(top, "*", new EnumerationOptions { RecurseSubdirectories = true, IgnoreInaccessible = true, AttributesToSkip = 0 }))
            r[d.Substring(top.Length + 1).Replace('\\', '/') + "/"] = "dir";
        foreach (string f in Directory.EnumerateFiles(top, "*", new EnumerationOptions { RecurseSubdirectories = true, IgnoreInaccessible = true, AttributesToSkip = 0 }))
        {
            var fi = new FileInfo(f);
            r[f.Substring(top.Length + 1).Replace('\\', '/')] = fi.Length + "|" + fi.LastWriteTimeUtc.Ticks + (hash ? "|" + UxBin.Sha256File(f) : "");
        }
        return r;
    }
    static bool Same(SortedDictionary<string, string> a, SortedDictionary<string, string> b, out string first)
    {
        first = null;
        foreach (var kv in a) if (!b.TryGetValue(kv.Key, out string v) || v != kv.Value) { first = kv.Key + (b.ContainsKey(kv.Key) ? " changed" : " gone"); return false; }
        foreach (var kv in b) if (!a.ContainsKey(kv.Key)) { first = kv.Key + " new"; return false; }
        return true;
    }
    static void Unchanged(string what, SortedDictionary<string, string> before, string path, bool hash = false)
    {
        bool same = Same(before, Snapshot(path, hash), out string first);
        UxTest.Check(same, what + (same ? "" : ": " + first));
    }
    static List<string> Staging(string tracks) =>
        Directory.Exists(tracks) ? Directory.GetDirectories(tracks).Select(Path.GetFileName).Where(n => n.StartsWith(UntamedImporter.StagingPrefix, StringComparison.Ordinal)).ToList() : new List<string>();

    static UntamedImportOptions Options(bool replace = false) => new UntamedImportOptions { replace = replace, threads = Threads };

    /// A hand-installed copy as hand-built releases are installed: a folder of files the importer must never touch.
    static void HandInstall(string tracks, string name)
    {
        string d = Path.Combine(tracks, "Untamed", name);
        Directory.CreateDirectory(d);
        foreach (string ext in new[] { "database", "level", "package", "scene" }) File.WriteAllText(Path.Combine(d, "Beta_Sx_Track_Slot_1.dx9." + ext), "hand-installed " + name + " " + ext + "\n");
        File.WriteAllText(Path.Combine(d, "mod.ini"), "name = " + name + " (by hand)\nkind = supercross\n");
    }

    static UntamedImportResult Run(UntamedSources found, IEnumerable<string> shorts, string tracks, IUxLibrary library, UntamedImportOptions options, CancellationToken cancel,
                                   Action<UntamedProgress> progress = null)
    {
        var chosen = shorts.Select(s => found.tracks.First(t => t.known.shortName == s)).ToList();
        return UntamedImporter.Import(found, chosen, tracks, library, options, progress, cancel);
    }

    /// A link to a folder: a junction on Windows (needs no privilege), a symbolic link elsewhere. False when none could be made.
    static bool Link(string link, string target)
    {
        try
        {
            if (Path.DirectorySeparatorChar != '\\') { Directory.CreateSymbolicLink(link, target); return true; }
            var psi = new System.Diagnostics.ProcessStartInfo("cmd.exe", "/c mklink /J \"" + link + "\" \"" + target + "\"") { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
            using (var p = System.Diagnostics.Process.Start(psi)) { p.StandardOutput.ReadToEnd(); p.StandardError.ReadToEnd(); p.WaitForExit(); }
            return Directory.Exists(link) && (File.GetAttributes(link) & FileAttributes.ReparsePoint) != 0;
        }
        catch (Exception) { return false; }
    }

    static string Throws(Action a)
    {
        try { a(); return null; }
        catch (UntamedImportException e) { return e.Message; }
    }

    static int Main(string[] args)
    {
        string what = args.Length > 0 && !args[0].StartsWith("--", StringComparison.Ordinal) ? args[0] : "quick";
        string iso = UxTest.Arg(args, "iso", "UX_DISC_ISO"), reflex = UxTest.Arg(args, "reflex", "UX_REFLEX") ?? UxTest.Arg(args, "client", "UX_CLIENT_LIB");
        string scratch = UxTest.Arg(args, "scratch", "UX_SCRATCH"), folder = UxTest.Arg(args, "folder", "UX_DISC_FOLDER"), expected = UxTest.Arg(args, "expected", "UX_BUILD_EXPECTED");
        Threads = int.Parse(UxTest.Arg(args, "threads", null, "0"), CultureInfo.InvariantCulture);
        if (scratch == null) { Console.WriteLine("--scratch <dir> is needed"); return 2; }
        if (iso == null || !File.Exists(iso)) { UxTest.Skip("no disc image (--iso / UX_DISC_ISO)"); return UxTest.Result("untamed import"); }
        if (reflex == null || !Directory.Exists(reflex)) { UxTest.Skip("no Reflex library (--reflex / UX_REFLEX)"); return UxTest.Result("untamed import"); }
        Directory.CreateDirectory(scratch);
        var isoBefore = Snapshot(iso);
        var libBefore = Snapshot(reflex);
        UxTest.Log("disc " + iso + "; library " + reflex + " (" + libBefore.Count + " files and folders)");
        using (var dir = new UxDirectoryLibrary(reflex))
        {
            if (what == "quick" || what == "both") Quick(iso, dir, reflex, scratch, folder, expected);
            if (what == "all" || what == "both") All(iso, dir, scratch);
            if (what == "hide") Hide(iso, dir, scratch, UxTest.Arg(args, "hide", null, ""), UxTest.Arg(args, "levels", null, "norton"));
        }
        UxTest.Section("sources unchanged");
        Unchanged("the disc image is as it was", isoBefore, iso);
        Unchanged("the library folder is as it was (every file's size and time)", libBefore, reflex);
        return UxTest.Result("untamed import (" + what + ")");
    }

    // ------------------------------------------------------------------------------------------------------- quick
    static void Quick(string iso, UxDirectoryLibrary dir, string reflex, string scratch, string folder, string expected)
    {
        string tracks = Path.Combine(UxTest.FreshFolder(scratch, "quick"), "Mods", "Tracks");
        var none = CancellationToken.None;

        UxTest.Section("table");
        if (expected != null && File.Exists(expected))
        {
            int rows = 0, bad = 0;
            foreach (string raw in File.ReadAllLines(expected))
            {
                string l = raw.Trim(); if (l.Length == 0 || l[0] == '#') continue;
                string sha = l.Substring(0, 64), rest = l.Substring(l.IndexOf('*') + 1);
                string shortName = rest.Substring(0, rest.IndexOf('/')), kind = rest.Substring(rest.LastIndexOf('.') + 1);
                rows++;
                if (UntamedImporter.TestedSha256(shortName, kind) != sha) { bad++; UxTest.Log("     differs: " + rest); }
            }
            UxTest.Report(rows == 48 && bad == 0, "the importer's tested-build table = expected_sha256.txt (" + rows + " rows, " + bad + " differ)");
        }
        else UxTest.Skip("no expected_sha256.txt (--expected)");

        // ---- a cancelled scan holds nothing open
        UxTest.Section("a cancelled scan");
        {
            var cts = new CancellationTokenSource(); bool threw = false; string at = null; var clock = System.Diagnostics.Stopwatch.StartNew();
            try { UntamedImporter.Scan(new[] { iso }, tracks, (s, f) => { if (s.StartsWith("Checking ", StringComparison.Ordinal)) { at = at ?? s; cts.Cancel(); } }, cts.Token).Dispose(); }
            catch (OperationCanceledException) { threw = true; }
            UxTest.Report(threw && at != null, "cancelled at \"" + at + "\" (" + F(clock.Elapsed.TotalSeconds) + " s): the scan threw OperationCanceledException");
            string held = null; clock.Restart();
            try { using (new FileStream(iso, FileMode.Open, FileAccess.Read, FileShare.None)) { } }
            catch (IOException e) { held = e.Message; }
            UxTest.Report(held == null, "the disc image is not held open after it (opened with no sharing " + F(clock.Elapsed.TotalSeconds) + " s later, no collection run)" + (held != null ? ": " + held : ""));
        }

        var scanClock = System.Diagnostics.Stopwatch.StartNew();
        using (var found = UntamedImporter.Scan(new[] { iso }, tracks, null, none))
        {
            UxTest.Section("scan");
            UxTest.Report(found.Ready.Count() == 12 && found.tracks.All(t => t.imported == null && t.handInstalled.Count == 0 && t.inTheWay == null), "12 stadiums ready, none imported, nothing in the way (" + F(scanClock.Elapsed.TotalSeconds) + " s)");

            // ---- preconditions: nothing may be written
            UxTest.Section("library, space, path");
            var probe = new ProbeLibrary(dir);
            probe.hidden.Add("SX_FortDodge");
            string m = Throws(() => Run(found, new[] { "norton", "lincoln" }, tracks, probe, Options(), none));
            UxTest.Report(m != null && m.Contains("SX_FortDodge") && m.Contains("stock supercross stadium"), "a library without SX_FortDodge: \"" + m + "\"");
            probe.hidden.Clear(); probe.hidden.Add("SX_DLC01_FreightYard");
            m = Throws(() => Run(found, new[] { "norton", "lincoln" }, tracks, probe, Options(), none));
            UxTest.Report(m != null && m.Contains("SX_DLC01_FreightYard") && m.Contains("Lincoln Memorial Arena") && m.Contains("supercross stadiums can be imported without it"), "without the EnduroCross water's level: \"" + m + "\"");
            UxTest.Check(UntamedImporter.LibraryProblem(probe, found.tracks.Where(t => t.known.mode == "sx").Select(t => t.known)) == null, "the supercross stadiums need no SX_DLC01_FreightYard");
            probe.hidden.Clear(); probe.hidden.Add("AnimationSets.bxml");
            m = Throws(() => Run(found, new[] { "norton" }, tracks, probe, Options(), none));
            UxTest.Report(m != null && m.Contains("AnimationSets.bxml"), "without AnimationSets.bxml: \"" + m + "\"");
            probe.hidden.Clear();
            string emptyLib = UxTest.FreshFolder(scratch, "empty-library");
            Directory.CreateDirectory(Path.Combine(emptyLib, "Database"));
            m = Throws(() => UntamedImporter.Import(found, found.tracks.Where(t => t.known.shortName == "norton"), tracks, emptyLib, Options(), null, none));
            UxTest.Report(m != null && m.Contains("SX_FortDodge"), "an empty library folder: \"" + m + "\"");
            m = Throws(() => UntamedImporter.Import(found, found.tracks.Where(t => t.known.shortName == "norton"), tracks, Path.Combine(scratch, "no-such-library"), Options(), null, none));
            UxTest.Report(m != null && m.Contains("can't be read"), "a library folder that is not there: \"" + m + "\"");
            UxTest.Check(!Directory.Exists(tracks), "the tracks folder was not created by a refused import");

            var small = Options(); small.freeBytes = f => 200L << 20;
            m = Throws(() => Run(found, new[] { "norton", "lincoln" }, tracks, dir, small, none));
            UxTest.Report(m != null && m.Contains("Not enough free disk space") && m.Contains("200 MB are free"), "200 MB free: \"" + m + "\"");
            UxTest.Check(Staging(tracks).Count == 0 && !Directory.Exists(Path.Combine(tracks, "Untamed")), "nothing written by it");
            if (Path.DirectorySeparatorChar == '\\')
            {
                string deep = Path.Combine(scratch, "deep");
                while (deep.Length < 190) deep = Path.Combine(deep, "a-folder-with-a-long-name");
                m = Throws(() => Run(found, new[] { "norton" }, Path.Combine(deep, "Mods", "Tracks"), dir, Options(), none));
                UxTest.Report(m != null && m.Contains("too deeply nested") && m.Contains(@"\Games\OpenReflex"), "a tracks folder " + (deep.Length + 12) + " characters deep: \"" + m + "\"");
                UxTest.Check(Staging(Path.Combine(deep, "Mods", "Tracks")).Count == 0, "nothing staged there");
            }

            // ---- something of the player's own where the tracks go: said before a stadium is converted, by the scan and by the import
            UxTest.Section("in the way");
            {
                string tracksF = Path.Combine(UxTest.FreshFolder(scratch, "quick-file"), "Mods", "Tracks");
                Directory.CreateDirectory(tracksF); File.WriteAllText(Path.Combine(tracksF, "Untamed"), "a file\n");
                var wayBefore = Snapshot(tracksF, true);
                var clock = System.Diagnostics.Stopwatch.StartNew();
                m = Throws(() => Run(found, new[] { "norton", "lincoln" }, tracksF, dir, Options(), none));
                UxTest.Report(m != null && m.Contains("Mods/Tracks/Untamed is a file") && clock.Elapsed.TotalSeconds < 5, "Mods/Tracks/Untamed is a file, in " + F(clock.Elapsed.TotalSeconds) + " s: \"" + m + "\"");
                Unchanged("nothing written there, the file as it was", wayBefore, tracksF, true);
                using (var scan = UntamedImporter.Scan(new[] { iso }, tracksF, null, none))
                    UxTest.Check(scan.tracks.Count == 12 && scan.tracks.All(t => t.inTheWay != null && t.inTheWay.Contains("Mods/Tracks/Untamed is a file")), "the scan flags every stadium with it");

                string tracksW = Path.Combine(UxTest.FreshFolder(scratch, "quick-way"), "Mods", "Tracks");
                Directory.CreateDirectory(Path.Combine(tracksW, "Untamed", "norton"));                      // no level file in it, no manifest
                File.WriteAllText(Path.Combine(tracksW, "Untamed", "norton", "my notes.txt"), "mine\n");
                HandInstall(tracksW, "lincoln"); HandInstall(tracksW, "lincoln-v2");                         // the player's own, next to a hand-installed copy
                File.WriteAllText(Path.Combine(tracksW, "Untamed", "denton"), "a file\n");
                HandInstall(tracksW, "wilford-v3");                                                          // only a hand-installed copy: nothing in the way
                wayBefore = Snapshot(tracksW, true);
                using (var scan = UntamedImporter.Scan(new[] { iso }, tracksW, null, none))
                {
                    UntamedTrackSource T(string s) => scan.tracks.First(t => t.known.shortName == s);
                    UxTest.Report(T("norton").inTheWay != null && T("norton").inTheWay.Contains("was not made by this importer") && T("norton").installed == null && T("norton").imported == null,
                                  "the scan: a folder without a level file is in the way: \"" + T("norton").inTheWay + "\"");
                    UxTest.Report(T("lincoln").inTheWay != null && T("lincoln").inTheWay.Contains("Mods/Tracks/Untamed/lincoln is already there and was not made by this importer")
                                  && T("lincoln").handInstalled.SequenceEqual(new[] { "Untamed/lincoln-v2" }), "the scan: a folder next to a hand-installed copy is in the way: \"" + T("lincoln").inTheWay + "\"");
                    UxTest.Report(T("denton").inTheWay != null && T("denton").inTheWay.Contains("Mods/Tracks/Untamed/denton is a file"), "the scan: a file is in the way: \"" + T("denton").inTheWay + "\"");
                    UxTest.Check(T("wilford").inTheWay == null && T("wilford").handInstalled.SequenceEqual(new[] { "Untamed/wilford-v3" }) && scan.tracks.Count(t => t.inTheWay != null) == 3,
                                 "the scan: a hand-installed copy alone is not in the way; 3 of 12 stadiums flagged");
                    clock.Restart();
                    var r = Run(scan, new[] { "norton", "lincoln", "denton" }, tracksW, dir, Options(true), none);
                    UxTest.Report(r.tracks.All(t => t.state == UntamedImportState.Failed && t.error == T(t.shortName).inTheWay) && clock.Elapsed.TotalSeconds < 5,
                                  "the import refuses the three with the scan's sentence, in " + F(clock.Elapsed.TotalSeconds) + " s (nothing converted)");
                }
                Unchanged("all of it is untouched", wayBefore, tracksW, true);
            }

            // ---- a hand-installed copy: never touched from here on
            HandInstall(tracks, "norton-v3");
            var hand = Snapshot(Path.Combine(tracks, "Untamed", "norton-v3"), true);
            var tracksBefore = Snapshot(tracks, true);

            // ---- cancel inside the first stadium
            UxTest.Section("cancel inside the first stadium");
            {
                var cts = new CancellationTokenSource(); var clock = System.Diagnostics.Stopwatch.StartNew(); double at = -1; var stages = new List<string>();
                var r = Run(found, new[] { "norton", "lincoln" }, tracks, dir, Options(), cts.Token, p =>
                {
                    lock (stages) if (stages.Count == 0 || stages[stages.Count - 1] != p.stage) stages.Add(p.stage);
                    if (p.stage == "Converting: meshes" && at < 0) { at = clock.Elapsed.TotalSeconds; cts.Cancel(); }
                });
                double took = clock.Elapsed.TotalSeconds;
                UxTest.Report(at >= 0 && r.cancelled && r.tracks.All(t => t.state == UntamedImportState.Cancelled) && took - at < 5,
                              "cancelled at " + F(at) + " s (" + string.Join(" > ", stages) + "), ended " + F(took - at) + " s later: " + string.Join(", ", r.tracks.Select(t => t.shortName + " " + t.state)));
                Unchanged("the tracks folder is as it was (nothing staged, nothing published)", tracksBefore, tracks, true);
            }

            // ---- the import
            UxTest.Section("import Norton and Lincoln");
            var lib = new ProbeLibrary(dir);
            int longest = 0; string longestPath = "";
            var o = Options();
            o.staged = (k, work, outDir) =>
            {
                string top = Path.GetFullPath(work).TrimEnd(Path.DirectorySeparatorChar);
                foreach (string f in Directory.GetFiles(top, "*", SearchOption.AllDirectories)) if (f.Length - top.Length - 1 > longest) { longest = f.Length - top.Length - 1; longestPath = f.Substring(top.Length + 1); }
            };
            var progress = new List<UntamedProgress>();
            var res = Run(found, new[] { "norton", "lincoln" }, tracks, lib, o, none, p => { lock (progress) progress.Add(p); });
            foreach (var t in res.tracks) UxTest.Log("     " + t + "  " + F(t.seconds) + " s, " + Mb(t.bytes) + " MB, " + t.assets + " assets" + (t.warnings.Count > 0 ? "  warnings: " + string.Join(" | ", t.warnings) : ""));
            UxTest.Report(res.tracks.Count == 2 && res.tracks.All(t => t.Ok && !t.replaced) && !res.cancelled, "both imported in " + F(res.seconds) + " s");
            UxTest.Check(res.tracks.All(t => t.testedBuild), "both are the tested build (the four .dx9 files)");
            UxTest.Check(res.tracks[0].folder == "Untamed/norton" && res.tracks[1].folder == "Untamed/lincoln", "installed as Untamed/norton and Untamed/lincoln");
            UxTest.Check(res.tracks[0].olderCopies.SequenceEqual(new[] { "Untamed/norton-v3" }) && res.tracks[1].olderCopies.Count == 0, "Norton's result names the hand-installed Untamed/norton-v3");
            UxTest.Check(Staging(tracks).Count == 0, "no staging folder left");
            Unchanged("the hand-installed Untamed/norton-v3 is untouched", hand, Path.Combine(tracks, "Untamed", "norton-v3"), true);
            bool rising = true; float last = -1;
            lock (progress) foreach (var p in progress) { if (p.overall < last - 1e-6f || p.track < 0 || p.track > 1 || p.count != 2) rising = false; last = p.overall; }
            UxTest.Check(rising && progress.Count > 20 && Math.Abs(last - 1f) < 1e-6f, "progress rises to 1 (" + progress.Count + " calls: " + string.Join(", ", progress.Select(p => p.stage).Distinct()) + ")");
            UxTest.Check(longest > 0 && longest + 10 <= UntamedImporter.WorkPathBudget, "the longest staged path (" + longest + ": " + longestPath + ") + a temporary suffix fits WorkPathBudget " + UntamedImporter.WorkPathBudget);
            foreach (var t in res.tracks) CheckInstalled(tracks, t, found.tracks.First(s => s.known.shortName == t.shortName), iso);
            UxTest.Log("     library reads: " + string.Join(", ", lib.levels) + "; " + string.Join(", ", lib.roots));
            using (var again = UntamedImporter.Scan(new[] { iso }, tracks, null, none))
            {
                var n = again.tracks.First(t => t.known.shortName == "norton"); var l = again.tracks.First(t => t.known.shortName == "lincoln");
                UxTest.Check(n.imported == "Untamed/norton" && n.handInstalled.SequenceEqual(new[] { "Untamed/norton-v3" }) && n.installed != null, "the scan: Norton imported (Untamed/norton), hand-installed Untamed/norton-v3");
                UxTest.Check(l.imported == "Untamed/lincoln" && l.handInstalled.Count == 0 && l.installed == "Untamed/lincoln", "the scan: Lincoln imported (Untamed/lincoln)");
                UxTest.Check(again.tracks.Count(t => t.imported != null) == 2 && UntamedImporter.Installed(tracks).Count == 2, "the scan: two stadiums installed");
            }
            var imported = Snapshot(tracks, true);

            // ---- again without replace
            UxTest.Section("import again, no re-import asked");
            res = Run(found, new[] { "norton", "lincoln" }, tracks, dir, Options(), none);
            UxTest.Report(res.tracks.All(t => t.state == UntamedImportState.AlreadyImported && t.folder != null) && res.seconds < 5, "both AlreadyImported in " + F(res.seconds) + " s");
            Unchanged("nothing was rewritten", imported, tracks, true);

            // ---- re-import: a failure in Norton, Lincoln replaced
            UxTest.Section("re-import with a failure injected into Norton");
            o = Options(true);
            o.staged = (k, work, outDir) => { if (k.shortName == "norton") throw new UxException("injected failure"); };
            var nortonBefore = Snapshot(Path.Combine(tracks, "Untamed", "norton"), true);
            var lincolnBefore = Snapshot(Path.Combine(tracks, "Untamed", "lincoln"), true);
            res = Run(found, new[] { "norton", "lincoln" }, tracks, dir, o, none);
            UxTest.Report(res.tracks[0].state == UntamedImportState.Failed && res.tracks[0].error == "injected failure" && res.tracks[0].folder == null, "Norton: " + res.tracks[0]);
            Unchanged("Norton's track imported before is intact", nortonBefore, Path.Combine(tracks, "Untamed", "norton"), true);
            UxTest.Report(res.tracks[1].Ok && res.tracks[1].replaced && res.tracks[1].testedBuild, "Lincoln went on and was replaced: " + res.tracks[1]);
            {
                var now = Snapshot(Path.Combine(tracks, "Untamed", "lincoln"), true);
                bool sameFiles = now.Count == lincolnBefore.Count && now.All(kv => kv.Key == UntamedImporter.ManifestName || lincolnBefore.TryGetValue(kv.Key, out string v) && v.Substring(v.LastIndexOf('|')) == kv.Value.Substring(kv.Value.LastIndexOf('|')));
                bool rewritten = now.Where(kv => kv.Key != UntamedImporter.ManifestName).All(kv => kv.Value != lincolnBefore[kv.Key]);
                UxTest.Check(sameFiles && rewritten, "the re-imported Lincoln: new files, the same bytes as before (a deterministic conversion)");
                string kind = UxJson.Get(UntamedImporter.ReadManifest(Path.Combine(tracks, "Untamed", "lincoln")), "library", "from") as string;
                UxTest.Check(kind == (dir.IsExtracted ? "game library" : "original install"), "its manifest names the kind of library, not its folder (\"" + kind + "\")");
            }
            UxTest.Check(Staging(tracks).Count == 0, "no staging folder left");
            Unchanged("the hand-installed Untamed/norton-v3 is untouched", hand, Path.Combine(tracks, "Untamed", "norton-v3"), true);

            // ---- re-import cancelled inside the second stadium
            UxTest.Section("re-import cancelled inside the second stadium");
            {
                var cts = new CancellationTokenSource();
                // what the desktops write into a folder does not make a track "changed", and goes with the replaced folder
                string[] desktop = { ".DS_Store", "._mod.ini", "Thumbs.db", "desktop.ini" };
                foreach (string n in desktop) File.WriteAllText(Path.Combine(tracks, "Untamed", "lincoln", n), "desktop\n");
                lincolnBefore = Snapshot(Path.Combine(tracks, "Untamed", "lincoln"), true);
                res = Run(found, new[] { "lincoln", "norton" }, tracks, dir, Options(true), cts.Token, p => { if (p.index == 1 && p.stage == "Converting: collision") cts.Cancel(); });
                UxTest.Report(res.cancelled && res.tracks[0].Ok && res.tracks[0].replaced && res.tracks[1].state == UntamedImportState.Cancelled, string.Join(", ", res.tracks.Select(t => t.shortName + " " + t.state)));
                Unchanged("Norton's track imported before is intact", nortonBefore, Path.Combine(tracks, "Untamed", "norton"), true);
                UxTest.Check(!Same(lincolnBefore, Snapshot(Path.Combine(tracks, "Untamed", "lincoln"), true), out _), "Lincoln was replaced before the cancel");
                UxTest.Check(desktop.All(n => !File.Exists(Path.Combine(tracks, "Untamed", "lincoln", n))), "the desktops' files in it (.DS_Store, ._mod.ini, Thumbs.db, desktop.ini) did not stop that and went with the old folder");
                UxTest.Check(Staging(tracks).Count == 0, "no staging folder left");
                foreach (string s in new[] { "norton", "lincoln" }) UxTest.Check(UntamedImporter.CheckTrack(Path.Combine(tracks, "Untamed", s)).Count == 0, "Untamed/" + s + " passes the track checks");
            }

            // ---- a track whose package is open (the game keeps a ridden track's open): said before it is converted
            if (Path.DirectorySeparatorChar == '\\')
            {
                UxTest.Section("a re-import of a track that is in use");
                var inUse = Snapshot(tracks, true);
                using (new FileStream(Path.Combine(tracks, "Untamed", "lincoln", "Beta_Sx_Track_Slot_1.dx9.package"), FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    res = Run(found, new[] { "lincoln" }, tracks, dir, Options(true), none);
                    UxTest.Report(res.tracks[0].state == UntamedImportState.Failed && res.tracks[0].error.Contains("Mods/Tracks/Untamed/lincoln is in use") && res.tracks[0].error.Contains("restart the game")
                                  && res.tracks[0].steps.Count == 0 && res.seconds < 8, "refused in " + F(res.seconds) + " s, before the disc is read: " + res.tracks[0].error);
                }
                Unchanged("the tracks folder is as it was", inUse, tracks, true);
            }

            // ---- the player's changes are theirs
            UxTest.Section("a changed track, a folder not made by the importer");
            File.AppendAllText(Path.Combine(tracks, "Untamed", "lincoln", "mod.ini"), "; my note\n");
            HandInstall(tracks, "wilford");
            var before = Snapshot(tracks, true);
            res = Run(found, new[] { "lincoln", "wilford" }, tracks, dir, Options(true), none);
            UxTest.Report(res.tracks[0].state == UntamedImportState.Failed && res.tracks[0].error.Contains("was changed since it was imported (mod.ini)"), "the changed Lincoln: " + res.tracks[0].error);
            UxTest.Report(res.tracks[1].state == UntamedImportState.Failed && res.tracks[1].error.Contains("was not made by this importer"), "the hand-made Untamed/wilford: " + res.tracks[1].error);
            File.WriteAllText(Path.Combine(tracks, "Untamed", "norton", "notes.txt"), "mine\n");
            var res2 = Run(found, new[] { "norton" }, tracks, dir, Options(true), none);
            UxTest.Report(res2.tracks[0].state == UntamedImportState.Failed && res2.tracks[0].error.Contains("holds a file the importer did not write (notes.txt)"), "Norton with a file of the player's: " + res2.tracks[0].error);
            File.Delete(Path.Combine(tracks, "Untamed", "norton", "notes.txt"));
            // a dot name is the player's too (only the desktops' own files are passed over)
            File.WriteAllText(Path.Combine(tracks, "Untamed", "norton", ".my-setup.txt"), "mine\n");
            res2 = Run(found, new[] { "norton" }, tracks, dir, Options(true), none);
            UxTest.Report(res2.tracks[0].state == UntamedImportState.Failed && res2.tracks[0].error.Contains("holds a file the importer did not write (.my-setup.txt)"), "Norton with a dot-named file of the player's: " + res2.tracks[0].error);
            UxTest.Check(File.Exists(Path.Combine(tracks, "Untamed", "norton", ".my-setup.txt")), "the file is still there");
            File.Delete(Path.Combine(tracks, "Untamed", "norton", ".my-setup.txt"));
            Directory.CreateDirectory(Path.Combine(tracks, "Untamed", "norton", ".mine")); File.WriteAllText(Path.Combine(tracks, "Untamed", "norton", ".mine", "notes.txt"), "mine\n");
            res2 = Run(found, new[] { "norton" }, tracks, dir, Options(true), none);
            UxTest.Report(res2.tracks[0].state == UntamedImportState.Failed && res2.tracks[0].error.Contains("holds a folder the importer did not write (.mine)"), "Norton with a dot-named folder of the player's: " + res2.tracks[0].error);
            UxTest.Check(File.Exists(Path.Combine(tracks, "Untamed", "norton", ".mine", "notes.txt")), "the folder and its file are still there");
            Directory.Delete(Path.Combine(tracks, "Untamed", "norton", ".mine"), true);
            Unchanged("all three are untouched", before, tracks, true);

            // ---- what a crashed import left
            UxTest.Section("a crashed import's staging folder");
            {
                string stale = Path.Combine(tracks, UntamedImporter.StagingPrefix + "0123abcd"), held = Path.Combine(tracks, UntamedImporter.StagingPrefix + "89abcdef");
                Directory.CreateDirectory(Path.Combine(stale, "w", "meshes")); File.WriteAllText(Path.Combine(stale, "w", "meshes", "x.surface"), "half");
                Directory.CreateDirectory(Path.Combine(stale, "o")); File.WriteAllText(Path.Combine(stale, "o", "Beta_Sx_Track_Slot_1.dx9.package.tmp"), "half");
                File.WriteAllText(Path.Combine(stale, "lock"), "");
                // the crash came after Denton's old track was set aside and before the new one was in; Lincoln's new one was in already
                Directory.CreateDirectory(Path.Combine(stale, "previous", "denton")); File.WriteAllText(Path.Combine(stale, "previous", "denton", UntamedImporter.ManifestName), "{}");
                File.WriteAllText(Path.Combine(stale, "previous", "denton", "mod.ini"), "name = old denton\n");
                Directory.CreateDirectory(Path.Combine(stale, "previous", "lincoln")); File.WriteAllText(Path.Combine(stale, "previous", "lincoln", "mod.ini"), "name = old lincoln\n");
                Directory.CreateDirectory(held); File.WriteAllText(Path.Combine(held, "lock"), "");
                // a link in it to a folder outside (with a read-only file): the clean-up removes the link and never goes through it
                string roLeft = Path.Combine(scratch, "quick-outside", "ro.txt");
                if (File.Exists(roLeft)) File.SetAttributes(roLeft, FileAttributes.Normal);   // (an earlier run's)
                string outside = UxTest.FreshFolder(scratch, "quick-outside");
                File.WriteAllText(Path.Combine(outside, "ro.txt"), "outside, read-only\n"); File.SetAttributes(Path.Combine(outside, "ro.txt"), FileAttributes.ReadOnly);
                Directory.CreateDirectory(Path.Combine(outside, "sub")); File.WriteAllText(Path.Combine(outside, "sub", "x.txt"), "outside\n");
                var outsideBefore = Snapshot(outside, true);
                bool linked = Link(Path.Combine(stale, "w", "link"), outside) && Link(Path.Combine(stale, "previous", "lincoln", ".link"), outside);
                // (and read-only files of its own, which must not stop it)
                File.SetAttributes(Path.Combine(stale, "w", "meshes", "x.surface"), FileAttributes.ReadOnly);
                var lincolnNow = Snapshot(Path.Combine(tracks, "Untamed", "lincoln"), true);
                using (new FileStream(Path.Combine(held, "lock"), FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                {
                    res = Run(found, new[] { "norton" }, tracks, dir, Options(), none);    // (AlreadyImported: only the clean-up runs)
                    UxTest.Check(res.tracks[0].state == UntamedImportState.AlreadyImported, "the import ran");
                    UxTest.Check(!Directory.Exists(stale), "the crashed import's staging folder was removed");
                    if (linked)
                    {
                        Unchanged("the folder outside that two links in it pointed at is as it was (its files, their times)", outsideBefore, outside, true);
                        UxTest.Check((File.GetAttributes(Path.Combine(outside, "ro.txt")) & FileAttributes.ReadOnly) != 0, "its read-only file is still read-only");
                    }
                    else UxTest.Skip("no link could be made here (mklink /J, or a symbolic link): the clean-up through a link is not tested");
                    UxTest.Check(File.Exists(Path.Combine(tracks, "Untamed", "denton", "mod.ini")) && File.ReadAllText(Path.Combine(tracks, "Untamed", "denton", "mod.ini")) == "name = old denton\n", "the track it had set aside is back (Untamed/denton)");
                    Unchanged("the track that had got in is untouched (Untamed/lincoln)", lincolnNow, Path.Combine(tracks, "Untamed", "lincoln"), true);
                    UxTest.Check(Directory.Exists(held), "a staging folder whose lock is held (a running import) is left");
                }
                UxTest.Check(UntamedImporter.CleanStaging(tracks) == 1 && !Directory.Exists(held), "once its lock is free it goes too");
            }
            Unchanged("the hand-installed Untamed/norton-v3 is untouched", hand, Path.Combine(tracks, "Untamed", "norton-v3"), true);
        }

        // ---- a disc that differs
        if (folder != null && Directory.Exists(Path.Combine(folder, "Database")))
        {
            UxTest.Section("a disc folder that differs");
            string copy = UxTest.FreshFolder(scratch, "disc-copy");
            Directory.CreateDirectory(Path.Combine(copy, "Database")); Directory.CreateDirectory(Path.Combine(copy, "AI"));
            foreach (string ext in new[] { "level", "database", "package", "scene" }) File.Copy(Path.Combine(folder, "Database", "Lincoln_STADIUM.xenon." + ext), Path.Combine(copy, "Database", "Lincoln_STADIUM.xenon." + ext));
            File.Copy(Path.Combine(folder, "AI", "Lincoln_STADIUM-ec.tdf07"), Path.Combine(copy, "AI", "Lincoln_STADIUM-ec.tdf07"));
            string tracks2 = Path.Combine(UxTest.FreshFolder(scratch, "quick2"), "Mods", "Tracks");
            using (var found = UntamedImporter.Scan(new[] { copy }, tracks2, null, CancellationToken.None))
            {
                var res = Run(found, new[] { "lincoln" }, tracks2, dir, Options(), CancellationToken.None);
                UxTest.Report(res.tracks[0].state == UntamedImportState.Failed && res.tracks[0].error.Contains("needs Fairview_STADIUM of the same disc"), "Lincoln alone in a folder: " + res.tracks[0].error);
            }
            string pkg = Path.Combine(copy, "Database", "Lincoln_STADIUM.xenon.package");
            using (var f = new FileStream(pkg, FileMode.Open, FileAccess.ReadWrite)) { f.Position = 20_000_000; int b = f.ReadByte(); f.Position = 20_000_000; f.WriteByte((byte)(b ^ 0x40)); }
            using (var found = UntamedImporter.Scan(new[] { copy }, tracks2, null, CancellationToken.None))
            {
                UxTest.Check(found.Ready.Count() == 1, "the scan takes the changed package (it does not read it)");
                var res = Run(found, new[] { "lincoln" }, tracks2, dir, Options(), CancellationToken.None);
                UxTest.Report(res.tracks[0].state == UntamedImportState.Failed && res.tracks[0].error.Contains("Lincoln_STADIUM.xenon.package differs from the supported disc"), "one package byte changed: " + res.tracks[0].error);
            }
            UxTest.Check(Staging(tracks2).Count == 0 && !Directory.Exists(Path.Combine(tracks2, "Untamed")), "nothing was left in the tracks folder");
            Directory.Delete(copy, true);
        }
        else UxTest.Log("(no --folder: the differing-disc test is not run)");
    }

    /// A published track: its files, the manifest against the files and the supported disc, the track checks.
    static void CheckInstalled(string tracks, UntamedTrackResult t, UntamedTrackSource src, string iso)
    {
        string dir = Path.Combine(tracks, "Untamed", t.shortName);
        var names = Directory.GetFiles(dir).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToList();
        var want = new[] { "Beta_Sx_Track_Slot_1.dx9.database", "Beta_Sx_Track_Slot_1.dx9.level", "Beta_Sx_Track_Slot_1.dx9.package", "Beta_Sx_Track_Slot_1.dx9.scene", "mod.ini", "preview.png", "sha256.txt", "untamed-import.json" };
        UxTest.Check(names.SequenceEqual(want) && Directory.GetDirectories(dir).Length == 0, t.shortName + ": the folder holds the four files, mod.ini, preview.png, sha256.txt and the manifest (" + string.Join(", ", names) + ")");
        var probs = UntamedImporter.CheckTrack(dir);
        UxTest.Check(probs.Count == 0, t.shortName + ": passes the track checks" + (probs.Count > 0 ? ": " + string.Join("; ", probs) : ""));
        var ini = File.ReadAllText(Path.Combine(dir, "mod.ini"));
        UxTest.Check(ini.Contains("name = " + UxCommon.Level(t.level).display + " (Untamed)\n") && ini.Contains("\nkind = supercross\n") && ini.Contains("\nthumbnail = preview.png\n"), t.shortName + ": mod.ini names it, kind supercross");
        var m = UntamedImporter.ReadManifest(dir);
        if (!UxTest.Check(m != null, t.shortName + ": the manifest reads")) return;
        var k = src.known;
        bool head = (string)m.Get("importer") == UntamedImporter.ImporterVersion && (string)m.Get("converter") == UxBuildTrack.PORT && (string)m.Get("level") == k.level && (string)m.Get("short") == k.shortName
                    && (string)m.Get("slot") == "Beta_Sx_Track_Slot_1" && m.Get("tested_build") is bool tb && tb && m.Get("imported_utc") is string && m.Get("options") is JObj;
        UxTest.Check(head, t.shortName + ": the manifest names the importer, the converter, the level, the options and the tested build");
        var s = UxJson.Get(m, "source", "sha256") as JObj;
        bool source = s != null && (string)s.Get("database") == k.databaseSha256 && (string)s.Get("scene") == k.sceneSha256 && (string)s.Get("tdf07") == k.tdfSha256 && (string)s.Get("package") == k.packageSha256
                      && ((string)s.Get("level")).Length == 64 && (string)UxJson.Get(m, "source", "from") == Path.GetFileName(iso) && (string)UxJson.Get(m, "source", "form") == "disc image"
                      && (string)UxJson.Get(m, "source", "title_id") == "545107DE";
        UxTest.Check(source, t.shortName + ": the manifest's source hashes are the supported disc's (database, scene, race line, package); it names the disc image");
        // the folder is shared as it is: no path in the manifest (the only '/' are in the importer's and the converter's versions)
        string text = File.ReadAllText(Path.Combine(dir, UntamedImporter.ManifestName)), from = UxJson.Get(m, "library", "from") as string;
        var slashed = text.Split('\n').Where(l => l.Contains('/')).Select(l => l.Trim().Split(':')[0].Trim('"')).ToList();
        bool noPath = !text.Contains('\\') && slashed.SequenceEqual(new[] { "importer", "converter" }) && (from == "library" || from == "original install" || from == "game library");
        UxTest.Check(noPath, t.shortName + ": the manifest holds no path (the disc image by name, the library as \"" + from + "\")");
        var files = (List<object>)m.Get("files");
        bool all = files.Count == 7;
        foreach (JObj f in files)
        {
            string p = Path.Combine(dir, (string)f.Get("name"));
            if (!File.Exists(p) || new FileInfo(p).Length != (long)f.Get("bytes") || UxBin.Sha256File(p) != (string)f.Get("sha256")) all = false;
        }
        UxTest.Check(all, t.shortName + ": the manifest lists the 7 files with their sizes and sha256");
        foreach (string kind in UxCommon.SlotFileKinds)
            UxTest.Check(UxBin.Sha256File(Path.Combine(dir, "Beta_Sx_Track_Slot_1.dx9." + kind)) == UntamedImporter.TestedSha256(t.shortName, kind), t.shortName + ": ." + kind + " = the tested build");
    }

    // ------------------------------------------------------------------------------------------------------- a library without some levels
    /// hide --hide A,B --levels norton,lincoln: imports the stadiums from a library that lacks the named stock levels (what the
    /// importer's library check lets through must then convert, and give the tested build).
    static void Hide(string iso, UxDirectoryLibrary dir, string scratch, string hide, string levels)
    {
        UxTest.Section("import " + levels + " without " + hide);
        string tracks = Path.Combine(UxTest.FreshFolder(scratch, "hide"), "Mods", "Tracks");
        var lib = new ProbeLibrary(dir);
        foreach (string h in hide.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries)) lib.hidden.Add(h);
        using (var found = UntamedImporter.Scan(new[] { iso }, tracks, null, CancellationToken.None))
        {
            UntamedImportResult res = null;
            string m = Throws(() => res = Run(found, levels.Split(','), tracks, lib, Options(), CancellationToken.None));
            if (m != null) { UxTest.Report(false, "refused: " + m); return; }
            foreach (var t in res.tracks) UxTest.Report(t.Ok && t.testedBuild, t.ToString() + (t.Ok ? "  tested build " + t.testedBuild : ""));
            UxTest.Log("     library reads: " + string.Join(", ", lib.levels));
        }
    }

    // ------------------------------------------------------------------------------------------------------- all 12
    static void All(string iso, UxDirectoryLibrary dir, string scratch)
    {
        UxTest.Section("import all 12 stadiums");
        string tracks = Path.Combine(UxTest.FreshFolder(scratch, "all"), "Mods", "Tracks");
        var lib = new ProbeLibrary(dir);
        int longest = 0; string longestPath = ""; long peakStage = 0;
        var o = Options();
        var memory = new Dictionary<string, string>();
        o.staged = (k, work, outDir) =>
        {
            using (var p = System.Diagnostics.Process.GetCurrentProcess()) memory[k.shortName] = "heap " + Mb(GC.GetTotalMemory(true)) + " MB, working set " + Mb(p.WorkingSet64) + " MB";
            string top = Path.GetFullPath(work).TrimEnd(Path.DirectorySeparatorChar); long bytes = 0;
            foreach (string f in Directory.GetFiles(top, "*", SearchOption.AllDirectories)) { bytes += new FileInfo(f).Length; if (f.Length - top.Length - 1 > longest) { longest = f.Length - top.Length - 1; longestPath = f.Substring(top.Length + 1); } }
            foreach (string f in Directory.GetFiles(outDir)) bytes += new FileInfo(f).Length;
            peakStage = Math.Max(peakStage, bytes);
        };
        var proc = System.Diagnostics.Process.GetCurrentProcess();
        using (var found = UntamedImporter.Scan(new[] { iso }, tracks, null, CancellationToken.None))
        {
            long need = UntamedImporter.NeededBytes(found.tracks.Select(t => t.known));
            var res = UntamedImporter.Import(found, found.tracks, tracks, lib, o, null, CancellationToken.None);
            long total = 0;
            foreach (var t in res.tracks)
            {
                UxTest.Log("     " + t.displayName.PadRight(28) + " " + t.state + "  " + F(t.seconds).PadLeft(5) + " s  " + Mb(t.bytes).PadLeft(5) + " MB  " + t.assets + " assets  tested build " + t.testedBuild
                           + "  estimate " + Mb(UntamedImporter.EstimatedBytes(t.known)) + " MB  after it: " + (memory.TryGetValue(t.shortName, out string mem) ? mem : "-") + (t.error != null ? "  " + t.error : "") + (t.warnings.Count > 0 ? "  warnings: " + string.Join(" | ", t.warnings) : ""));
                total += t.bytes;
                UxTest.Log("         " + string.Join("  ", t.steps.Select(st => st.Key + " " + F(st.Value))));
            }
            proc.Refresh();
            UxTest.Report(res.tracks.Count == 12 && res.tracks.All(t => t.Ok), "12 of 12 imported in " + F(res.seconds) + " s, " + Mb(total) + " MB installed; peak working set " + Mb(proc.PeakWorkingSet64) + " MB");
            UxTest.Check(res.tracks.All(t => t.testedBuild), "all 12 are the tested build");
            UxTest.Check(res.tracks.All(t => t.bytes <= UntamedImporter.EstimatedBytes(t.known)), "every track is within its disk space estimate");
            UxTest.Report(total + peakStage <= need, "the space check asks for " + Mb(need) + " MB; used: " + Mb(total) + " MB installed + at most " + Mb(peakStage) + " MB staged");
            UxTest.Check(Staging(tracks).Count == 0, "no staging folder left");
            UxTest.Check(longest > 0 && longest + 10 <= UntamedImporter.WorkPathBudget, "the longest staged path (" + longest + ": " + longestPath + ") + a temporary suffix fits WorkPathBudget " + UntamedImporter.WorkPathBudget);
            foreach (var t in res.tracks)
            {
                string d = Path.Combine(tracks, "Untamed", t.shortName);
                var probs = Directory.Exists(d) ? UntamedImporter.CheckTrack(d) : new List<string> { "not there" };
                UxTest.Check(probs.Count == 0 && UntamedImporter.ReadManifest(d) != null, t.shortName + ": installed, passes the track checks" + (probs.Count > 0 ? ": " + string.Join("; ", probs) : ""));
            }
            UxTest.Log("     library reads: " + string.Join(", ", lib.levels) + "; " + string.Join(", ", lib.roots));
            // every read must be one the importer's library check knows: hide each level read and ask it
            var missed = new List<string>();
            foreach (string level in lib.levels.Where(l => !l.StartsWith("(absent)", StringComparison.Ordinal)).Concat(lib.roots).ToList())
            {
                var hide = new ProbeLibrary(dir); hide.hidden.Add(level);
                if (UntamedImporter.LibraryProblem(hide, found.tracks.Select(t => t.known)) == null) missed.Add(level);
            }
            UxTest.Report(missed.Count == 0, "the library check names every stock file the import read" + (missed.Count > 0 ? "; NOT checked: " + string.Join(", ", missed) : ""));
        }
    }
}
