// Custom tracks downloaded as archives (w-fb-customtrack): a .zip / .rar / .7z put in Mods/Tracks (TrackMods.cs) is installed by the game -
// extracted into a folder of its own next to it - instead of the player having to extract it first.
//
// What a download really is: the community's track sites serve archives whose names do not always say what they are (Mods/Tracks
// "573-2014-monster-energy-cup.zip" is a RAR 4 archive). An archive is told by its first bytes, never its extension:
//   zip   PK 03 04 (PK 05 06: an empty zip)          read by System.IO.Compression (managed, every platform)
//   rar   Rar! 1A 07 00 (RAR 4) / Rar! 1A 07 01 00 (RAR 5)
//   7z    7z BC AF 27 1C                               read by the system's own tar (bsdtar / libarchive), nothing downloaded:
//         macOS /usr/bin/tar (libarchive 3.5+: RAR 4, RAR 5, 7z), Windows 10 1803+ / 11 %SystemRoot%\System32\tar.exe (libarchive 3.3.2 on
//         older Windows 10 builds: RAR 4 and 7z but not RAR 5; Windows 11 23H2+ 3.7: RAR 5 too), Linux bsdtar if installed. A zip the
//         managed reader cannot read (deflate64, LZMA ...) goes to tar as well.
// The install (a worker thread, one at a time): the archive is extracted into Mods/Tracks/.installing/<n>/out (a dot folder: never scanned),
// links are removed, the size is capped (MaxBytes), the result must hold a complete track set (<stem>.dx9.database / .level / .package /
// .scene) - then the folder moves to <the archive's folder>/<archive name>/ ("Mods/Tracks/573-2014-monster-energy-cup/": the menus call it
// 2014 MONSTER ENERGY CUP), a single top folder flattened, and a marker (.reflex-archive: the archive's name and size) says which archive it
// came from, so the archive (left where the player put it) is not installed again; deleting the folder installs it again. Every failure
// keeps the archive and says what is wrong in the browser (TrackMod.problem). Nothing here uses Unity (tests/customtrack builds it alone).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content
{
    public static class TrackArchives
    {
        public enum Kind { None, Zip, Rar4, Rar5, SevenZip, RxMod }   // RxMod: a Reflex archive of kind MODS (a .rxmod kit pack, core/plan/RXMOD.md)

        /// What the file is by its first bytes (None: not an archive this reads, or unreadable).
        public static Kind Sniff(string path)
        {
            try
            {
                var b = new byte[12]; int n;
                using (var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) n = f.Read(b, 0, 12);
                return Sniff(b, n);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is NotSupportedException || e is ArgumentException) { return Kind.None; }
        }
        public static Kind Sniff(byte[] b, int n)
        {
            if (b == null) return Kind.None;
            n = Math.Min(n, b.Length);
            if (n >= 4 && b[0] == 'P' && b[1] == 'K' && (b[2] == 3 && b[3] == 4 || b[2] == 5 && b[3] == 6 || b[2] == 7 && b[3] == 8)) return Kind.Zip;
            if (n >= 7 && b[0] == 'R' && b[1] == 'a' && b[2] == 'r' && b[3] == '!' && b[4] == 0x1A && b[5] == 7)
            {
                if (b[6] == 0) return Kind.Rar4;
                if (n >= 8 && b[6] == 1 && b[7] == 0) return Kind.Rar5;
            }
            if (n >= 6 && b[0] == '7' && b[1] == 'z' && b[2] == 0xBC && b[3] == 0xAF && b[4] == 0x27 && b[5] == 0x1C) return Kind.SevenZip;
            // a .rxmod (RXAR of kind "MODS"); never a .rxtrack / .rxdata (kinds TRAK / DATA: read in place, not unpacked)
            if (n >= 12 && b[0] == 'R' && b[1] == 'X' && b[2] == 'A' && b[3] == 'R' && b[8] == 'M' && b[9] == 'O' && b[10] == 'D' && b[11] == 'S') return Kind.RxMod;
            return Kind.None;
        }
        public static string Name(Kind k) => k == Kind.Zip ? "zip" : k == Kind.Rar4 ? "RAR 4" : k == Kind.Rar5 ? "RAR 5" : k == Kind.SevenZip ? "7z" : k == Kind.RxMod ? "rxmod" : "unknown";
        static string Ext(Kind k) => k == Kind.Zip ? ".zip" : k == Kind.SevenZip ? ".7z" : k == Kind.RxMod ? ".rxmod" : k == Kind.None ? "" : ".rar";

        /// The extensions that make a file an archive note even when its bytes say nothing (an incomplete download).
        public static readonly string[] Extensions = { ".zip", ".rar", ".7z", ".rxmod" };
        public const string MarkerName = ".reflex-archive", StagingName = ".installing";
        /// The most an archive may unpack to (a track is four files).
        public static long MaxBytes = 2L << 30;
        public static int TimeoutMs = 10 * 60 * 1000;
        static readonly string[] Parts = { ".dx9.level", ".dx9.database", ".dx9.package", ".dx9.scene" };

        /// Where the game's logs go (TrackMods: Debug.Log; tests: the console).
        public static Action<string> Log = s => { };

        /// What an archive is installed as (w-gearmods: the same installs serve Mods/Gear's kit packs, KitPacks.cs): the folder named in
        /// the messages, what the archive should hold, the log tag, and the check of what was extracted (null: fine; else what is wrong,
        /// and a detail for the marker).
        public sealed class Profile
        {
            public string folder = "Mods/Tracks", what = "a track", tag = "[TrackMods]";
            public Func<string, (string error, string detail)> validate = dir => { string e = Tracks(dir, out string d); return (e, d); };
        }
        public static readonly Profile TrackProfile = new Profile();

        // ------------------------------------------------------------------------------------------------ installed archives

        /// The folder an archive was installed into (its marker names the archive and its size), null when it was not (pure file system).
        public static string InstalledFolder(string archive)
        {
            try
            {
                var fi = new FileInfo(archive); string dir = fi.DirectoryName; if (dir == null || !fi.Exists) return null;
                foreach (var d in Directory.GetDirectories(dir))
                {
                    string m = Path.Combine(d, MarkerName);
                    if (!File.Exists(m)) continue;
                    var lines = File.ReadAllLines(m);
                    if (lines.Length >= 2 && lines[0] == fi.Name && lines[1] == fi.Length.ToString(System.Globalization.CultureInfo.InvariantCulture)) return d;
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is NotSupportedException || e is ArgumentException) { }
            return null;
        }

        // ------------------------------------------------------------------------------------------------ the installs

        public sealed class Job
        {
            public string archive, key; public Kind kind; public Profile profile = TrackProfile;
            /// "installing", "done", "failed".
            public volatile string state = "installing";
            public string error, folder;
        }
        static readonly Dictionary<string, Job> s_jobs = new Dictionary<string, Job>(StringComparer.Ordinal);
        static readonly object s_lock = new object();
        static readonly SemaphoreSlim s_one = new SemaphoreSlim(1, 1);
        static int s_generation;
        /// Bumped when an install ends (TrackMods.Refresh rescans, the browser shows the result).
        public static int Generation => Volatile.Read(ref s_generation);
        /// An install is running or queued.
        public static bool Busy { get { lock (s_lock) return s_jobs.Values.Any(j => j.state == "installing"); } }

        static string KeyOf(string archive)
        {
            var fi = new FileInfo(archive);
            return Path.GetFullPath(archive) + "|" + (fi.Exists ? fi.Length + "|" + fi.LastWriteTimeUtc.Ticks : "-");
        }

        /// The archive's install: started on a worker the first time it is asked for (once per file version: a failed one is not retried until
        /// the file changes or the game starts again). tracksRoot: Mods/Tracks (its .installing folder holds the work).
        public static Job Ensure(string archive, string tracksRoot) => Ensure(archive, tracksRoot, TrackProfile);
        public static Job Ensure(string archive, string tracksRoot, Profile profile)
        {
            profile ??= TrackProfile;
            string key; try { key = KeyOf(archive); } catch (Exception e) { return new Job { archive = archive, state = "failed", error = e.Message }; }
            Job job;
            lock (s_lock)
            {
                if (s_jobs.TryGetValue(key, out job)) return job;
                job = new Job { archive = archive, key = key, kind = Sniff(archive), profile = profile };
                s_jobs[key] = job;
            }
            ThreadPool.QueueUserWorkItem(_ =>
            {
                s_one.Wait();
                try { Install(job, tracksRoot); }
                catch (Exception e) { job.error = "the install stopped (" + e.Message + ")"; }
                finally
                {
                    job.state = job.error == null ? "done" : "failed";
                    Log(profile.tag + " archive " + archive + " (" + Name(job.kind) + "): " + (job.error == null ? "installed into " + job.folder : "NOT installed: " + job.error));
                    Interlocked.Increment(ref s_generation);
                    s_one.Release();
                }
            });
            return job;
        }

        /// Installs one archive now, on this thread (the workers; tests). Fills job.folder or job.error.
        public static void Install(Job job, string tracksRoot)
        {
            var pr = job.profile ?? TrackProfile;
            string archive = job.archive, file = Path.GetFileName(archive);
            string stem = SafeName(Path.GetFileNameWithoutExtension(file));
            string named = Path.GetExtension(file).ToLowerInvariant();
            if (InstalledFolder(archive) is string already) { job.folder = already; return; }
            if (job.kind == Kind.None)
            {
                long len = 0; try { len = new FileInfo(archive).Length; } catch (Exception) { }
                job.error = len == 0 ? file + " is empty: the download did not finish - download it again"
                          : file + " is not a zip, RAR or 7z archive (its first bytes say otherwise, " + len + " bytes): the download may be incomplete or damaged - download it again, or extract it yourself into " + pr.folder + "/" + stem + "/";
                return;
            }
            string staging = Path.Combine(tracksRoot, StagingName, Guid.NewGuid().ToString("N").Substring(0, 12));
            string outDir = Path.Combine(staging, "out");
            try
            {
                Directory.CreateDirectory(outDir);
                string mismatch = Ext(job.kind) != named && !(job.kind == Kind.Zip && named == ".zip") ? " (a " + Name(job.kind) + " archive named " + (named.Length > 0 ? named : "without an extension") + ")" : "";
                if (mismatch.Length > 0) Log(pr.tag + " " + file + mismatch + ": read by its contents");
                string err = null;
                if (job.kind == Kind.RxMod) err = ExtractRxMod(archive, outDir);
                else if (job.kind == Kind.Zip)
                {
                    err = ExtractZip(archive, outDir, out bool unreadable);
                    if (err != null && unreadable)
                    {   // a zip method the managed reader lacks: the system's tar may have it
                        Log(pr.tag + " " + file + ": " + err + "; trying the system's tar");
                        Clear(outDir);
                        string tarErr = ExtractTar(archive, staging, outDir, job.kind, pr.folder);
                        err = tarErr == null ? null : err;
                    }
                }
                else err = ExtractTar(archive, staging, outDir, job.kind, pr.folder);
                if (err != null) { job.error = file + mismatch + ": " + err; return; }
                RemoveLinks(outDir);
                long bytes = Size(outDir);
                if (bytes > MaxBytes) { job.error = file + " unpacks to " + (bytes >> 20) + " MB, more than " + pr.what + " can be (" + (MaxBytes >> 20) + " MB)"; return; }
                var (what, detail) = pr.validate(outDir);
                if (what != null) { job.error = file + mismatch + " " + what; return; }
                // a single top folder (and nothing else): its contents are the track's folder
                string src = outDir;
                var top = Directory.GetFileSystemEntries(outDir).Where(e => !ReflexModsJunk(Path.GetFileName(e))).ToList();
                if (top.Count == 1 && Directory.Exists(top[0])) src = top[0];
                string parent = Path.GetDirectoryName(Path.GetFullPath(archive));
                string dest = Path.Combine(parent, stem);
                for (int i = 2; Directory.Exists(dest) || File.Exists(dest); i++) dest = Path.Combine(parent, stem + " (" + i + ")");
                Directory.Move(src, dest);
                var fi = new FileInfo(archive);
                File.WriteAllText(Path.Combine(dest, MarkerName), fi.Name + "\n" + fi.Length.ToString(System.Globalization.CultureInfo.InvariantCulture) + "\n" + Name(job.kind) + "\n" + detail + "\n");
                job.folder = dest;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is NotSupportedException || e is ArgumentException || e is InvalidDataException)
            {
                job.error = file + " could not be installed (" + e.Message + ")";
            }
            finally
            {
                try { if (Directory.Exists(staging)) Directory.Delete(staging, true); } catch (Exception) { }
                try { string s = Path.Combine(tracksRoot, StagingName); if (Directory.Exists(s) && !Directory.EnumerateFileSystemEntries(s).Any()) Directory.Delete(s); } catch (Exception) { }
            }
        }

        static bool ReflexModsJunk(string name) =>
            string.IsNullOrEmpty(name) || name.StartsWith(".", StringComparison.Ordinal) || string.Equals(name, "__MACOSX", StringComparison.OrdinalIgnoreCase)
            || string.Equals(name, "Thumbs.db", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "desktop.ini", StringComparison.OrdinalIgnoreCase);

        /// A folder name from the archive's name (no separators, no reserved characters, no trailing dots / spaces).
        public static string SafeName(string raw)
        {
            var sb = new StringBuilder();
            foreach (char c in raw ?? "") sb.Append(c < 32 || "<>:\"/\\|?*".IndexOf(c) >= 0 ? '_' : c);
            string s = sb.ToString().Trim().TrimEnd('.', ' ').TrimStart('.');
            return s.Length == 0 ? "Track" : s.Length > 80 ? s.Substring(0, 80).Trim() : s;
        }

        /// A .rxmod kit pack (RXMOD.md "Kit packs"): every entry inside outDir through the core's archive reader. A bike or skin .rxmod is
        /// refused with where it goes (Mods/Bikes reads those in place). Null: done.
        static string ExtractRxMod(string archive, string outDir)
        {
            var h = Reflex.Port.RxCore.RxArchiveNative.Open(archive);
            if (h == IntPtr.Zero) return "the .rxmod does not read (" + Reflex.Port.RxCore.RxArchiveNative.LastError() + ")";
            try
            {
                string meta = Reflex.Port.RxCore.RxArchiveNative.Meta(h) ?? "";
                if (meta.IndexOf("\"kits\"", StringComparison.Ordinal) < 0)
                    return "is a bike or skin .rxmod, not a kit pack: it goes in Mods/Bikes";
                Reflex.Port.RxCore.RxArchiveNative.List(h, out var paths, out var sizes, out var refs);
                if (paths.Length > 10000) return "the .rxmod holds " + paths.Length + " files, too many";
                string root = Path.GetFullPath(outDir).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                long total = 0;
                for (int i = 0; i < paths.Length; i++)
                {
                    if (refs[i]) return "the .rxmod names a pool entry (" + paths[i] + "): not a shareable mod";
                    string full = Path.GetFullPath(Path.Combine(outDir, paths[i].Replace('/', Path.DirectorySeparatorChar)));
                    if (!full.StartsWith(root, StringComparison.Ordinal)) return "the .rxmod names a file outside its folder (" + paths[i] + "): not installed";
                    total += sizes[i]; if (sizes[i] > int.MaxValue || total > MaxBytes) return "the .rxmod unpacks to more than " + (MaxBytes >> 20) + " MB";
                    var b = new byte[sizes[i]];
                    if (!Reflex.Port.RxCore.RxArchiveNative.Read(h, i, 0, b, 0, b.Length)) return paths[i] + ": " + Reflex.Port.RxCore.RxArchiveNative.LastError();
                    Directory.CreateDirectory(Path.GetDirectoryName(full));
                    File.WriteAllBytes(full, b);
                }
                return null;
            }
            finally { Reflex.Port.RxCore.RxArchiveNative.Close(h); }
        }

        /// The managed zip reader: every entry inside outDir (an entry that would land outside is refused), at most MaxBytes. Null: done.
        static string ExtractZip(string archive, string outDir, out bool unreadable)
        {
            unreadable = false;   // (refused for what it holds: never handed to tar)
            try
            {
                string root = Path.GetFullPath(outDir).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                long total = 0;
                using (var zip = ZipFile.OpenRead(archive))
                {
                    if (zip.Entries.Count > 10000) return "the zip holds " + zip.Entries.Count + " files, too many";
                    foreach (var e in zip.Entries)
                    {
                        string name = e.FullName.Replace('\\', '/');
                        if (name.Length == 0) continue;
                        string full = Path.GetFullPath(Path.Combine(outDir, name.Replace('/', Path.DirectorySeparatorChar)));
                        if (!full.StartsWith(root, StringComparison.Ordinal)) return "the zip names a file outside its folder (" + e.FullName + "): not installed";
                        if (name.EndsWith("/")) { Directory.CreateDirectory(full); continue; }
                        total += e.Length;
                        if (total > MaxBytes) return "the zip unpacks to more than " + (MaxBytes >> 20) + " MB";
                        Directory.CreateDirectory(Path.GetDirectoryName(full));
                        using (var src = e.Open()) using (var dst = new FileStream(full, FileMode.CreateNew, FileAccess.Write)) src.CopyTo(dst);
                    }
                }
                return null;
            }
            catch (Exception e) when (e is InvalidDataException || e is NotSupportedException || e is IOException || e is UnauthorizedAccessException || e is ArgumentException)
            { unreadable = true; return "the zip does not read (" + e.Message + ")"; }
        }

        /// The system's tar (bsdtar / libarchive), null when there is none.
        public static string TarPath()
        {
            try
            {
                if (Path.DirectorySeparatorChar == '\\')
                {
                    string sys = Environment.GetFolderPath(Environment.SpecialFolder.System);
                    foreach (var p in new[] { Path.Combine(sys ?? "", "tar.exe"), Path.Combine(Environment.GetEnvironmentVariable("SystemRoot") ?? @"C:\Windows", "System32", "tar.exe") })
                        if (File.Exists(p)) return p;
                    return null;
                }
                foreach (var p in new[] { "/usr/bin/bsdtar", "/usr/bin/tar", "/usr/local/bin/bsdtar", "/opt/homebrew/bin/bsdtar" })
                    if (File.Exists(p) && (p.EndsWith("bsdtar") || IsBsdTar(p))) return p;
            }
            catch (Exception) { }
            return null;
        }
        static bool IsBsdTar(string p)
        {   // macOS's /usr/bin/tar is bsdtar; GNU tar (most Linux) reads no RAR / 7z
            if (Path.DirectorySeparatorChar == '/' && File.Exists("/System/Library/CoreServices/SystemVersion.plist")) return true;
            var (code, output) = Run(p, new[] { "--version" }, null, 10000);
            return code == 0 && output.IndexOf("libarchive", StringComparison.OrdinalIgnoreCase) >= 0;
        }

        /// The archive through the system's tar into outDir (its working folder). The archive's path is given as is when it is plain
        /// ASCII, else a copy in the staging folder (Windows' tar.exe and odd names). Null: done.
        static string ExtractTar(string archive, string staging, string outDir, Kind kind, string folder = "Mods/Tracks")
        {
            string tar = TarPath();
            bool win = Path.DirectorySeparatorChar == '\\';
            if (tar == null)
                return "this computer has no tar to extract a " + Name(kind) + " archive with (" + (win ? "Windows 10 version 1803 or newer includes one" : "macOS includes one; on Linux install bsdtar / libarchive-tools")
                     + "): extract it yourself into " + folder + "/" + SafeName(Path.GetFileNameWithoutExtension(archive)) + "/";
            string input = Path.GetFullPath(archive);
            if (input.Any(c => c > 126 || c < 32))
            {
                string copy = Path.Combine(staging, "archive" + Ext(kind));
                File.Copy(input, copy);
                input = Path.Combine("..", "archive" + Ext(kind));
            }
            var (code, output) = Run(tar, new[] { "-x", "-f", input }, outDir, TimeoutMs);
            if (code == 0) return null;
            string first = (output ?? "").Split('\n').Select(l => l.Trim()).FirstOrDefault(l => l.Length > 0) ?? "";
            if (first.StartsWith("tar: ", StringComparison.Ordinal)) first = first.Substring(5);
            if (first.Length > 160) first = first.Substring(0, 160) + "...";
            string why = code == -2 ? "extracting took longer than " + TimeoutMs / 60000 + " minutes" : code == -1 ? "the system's tar did not start (" + first + ")" : "the system's tar could not extract it" + (first.Length > 0 ? " (" + first + ")" : "");
            string hint = kind == Kind.Rar5 && win ? " - the tar of older Windows 10 versions reads RAR 4 but not RAR 5 archives: extract it with 7-Zip or WinRAR into " + folder + "/" + SafeName(Path.GetFileNameWithoutExtension(archive)) + "/"
                        : " - the archive may be damaged, incomplete or password-protected: download it again, or extract it yourself into " + folder + "/" + SafeName(Path.GetFileNameWithoutExtension(archive)) + "/";
            return why + hint;
        }

        /// Runs a program without a window (output and errors read together); (-1, why) when it does not start, (-2, ..) on the timeout.
        static (int code, string output) Run(string exe, string[] args, string cwd, int timeoutMs)
        {
            try
            {
                var psi = new ProcessStartInfo(exe) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true };
                foreach (var a in args) psi.ArgumentList.Add(a);
                if (cwd != null) psi.WorkingDirectory = cwd;
                using (var p = Process.Start(psi))
                {
                    if (p == null) return (-1, "no process");
                    try { p.StandardInput.Close(); } catch (Exception) { }   // (a password prompt reads end of input and fails)
                    var err = p.StandardError.ReadToEndAsync(); var outp = p.StandardOutput.ReadToEndAsync();
                    if (!p.WaitForExit(timeoutMs)) { try { p.Kill(); } catch (Exception) { } return (-2, ""); }
                    p.WaitForExit();
                    return (p.ExitCode, (err.Result ?? "") + "\n" + (outp.Result ?? ""));
                }
            }
            catch (Exception e) { return (-1, e.Message); }
        }

        /// Symbolic links / junctions out of what was extracted (never followed).
        static void RemoveLinks(string dir)
        {
            foreach (var e in Directory.GetFileSystemEntries(dir))
            {
                var a = File.GetAttributes(e);
                if ((a & FileAttributes.ReparsePoint) != 0)
                {
                    if ((a & FileAttributes.Directory) != 0) Directory.Delete(e); else File.Delete(e);
                    Log("[Mods] removed the link " + e + " from an archive");
                    continue;
                }
                if ((a & FileAttributes.Directory) != 0) RemoveLinks(e);
            }
        }
        static long Size(string dir)
        {
            long n = 0;
            foreach (var f in Directory.GetFiles(dir, "*", SearchOption.AllDirectories)) n += new FileInfo(f).Length;
            return n;
        }
        static void Clear(string dir) { try { Directory.Delete(dir, true); } catch (Exception) { } Directory.CreateDirectory(dir); }

        /// Null when dir holds at least one complete track set (detail: the stems), else what is wrong ("holds no track ...").
        public static string Tracks(string dir, out string detail)
        {
            detail = "";
            string root = Path.GetFullPath(dir).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            var files = Directory.GetFiles(dir, "*", SearchOption.AllDirectories)
                .Where(f => !Path.GetFullPath(f).Substring(root.Length).Split(Path.DirectorySeparatorChar, '/').Any(ReflexModsJunk)).ToList();   // (the junk below dir: the staging folder is a dot folder itself)
            var sets = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
            foreach (var f in files)
            {
                string n = Path.GetFileName(f), low = n.ToLowerInvariant();
                foreach (var ext in Parts)
                    if (low.EndsWith(ext, StringComparison.Ordinal) && low.Length > ext.Length)
                    {
                        string k = Path.Combine(Path.GetDirectoryName(f) ?? "", n.Substring(0, n.Length - ext.Length));
                        if (!sets.TryGetValue(k, out var s)) sets[k] = s = new HashSet<string>(StringComparer.Ordinal);
                        s.Add(ext);
                    }
            }
            var complete = sets.Where(kv => kv.Value.Count == Parts.Length).Select(kv => Path.GetFileName(kv.Key)).ToList();
            if (complete.Count > 0) { detail = string.Join(", ", complete); return null; }
            var level = sets.Where(kv => kv.Value.Contains(".dx9.level") || kv.Value.Contains(".dx9.scene")).OrderByDescending(kv => kv.Value.Count).ToList();
            if (level.Count > 0)
            {   // a level's files, some missing
                var s = level[0].Value;
                return "holds an incomplete track: " + Path.GetFileName(level[0].Key) + " has no " + string.Join(" / ", Parts.Where(p => !s.Contains(p))) + " file - the download may be incomplete";
            }
            var kinds = files.Select(f => Path.GetExtension(f).ToLowerInvariant()).Where(x => x.Length > 0).GroupBy(x => x).OrderByDescending(g => g.Count()).Take(5).Select(g => g.Key).ToList();
            return "holds no track (no <name>.dx9.database / .level / .package / .scene set)" + (files.Count == 0 ? ": it is empty"
                 : sets.Count > 0 ? ": its .dx9.database / .package files without a level look like a bike mod - those go in Mods/Bikes"
                 : ": it holds " + files.Count + " files (" + string.Join(" ", kinds) + ")");
        }
    }
}
