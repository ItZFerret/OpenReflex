using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    // The player's asset install. Open builds require Database/; the legacy backend also requires MXReflex.exe.
    // Core startup data is embedded in librxcore; models, textures, sound and UI assets remain in the install.
    //
    // Lookup order: environment REFLEX_GAME_DIR, extracted client game files, PlayerPrefs "ReflexGameDir", a "reflex-game-dir.txt" next to the
    // player (or the project), then (editor / development) "game files/Archive" in a parent folder of the project.
    // Any drive: paths are cleaned (ReflexPaths.Clean: quotes, file:// URLs, forward slashes, a bare "E:") before they are used; a saved folder
    // or reflex-game-dir.txt whose drive letter changed (a USB drive back as another letter, a mounted ISO on another letter) is looked
    // for on the other ready drives (Remapped); installs on any drive are offered by Detected (drive roots, Games folders, Steam libraries).
    // The OPEN REFLEX shell's CONNECT FOLDER / GAME FOLDER (OpenShell.Library.cs) picks the folder and saves it with Set; a folder
    // set in this session wins over the lookup until the next start.
    public static class ReflexInstall
    {
        public const string PrefsKey = "ReflexGameDir";
        static string _dir; static Rfx.Install _install;

        public static bool IsInstall(string dir)
        {
            if (string.IsNullOrEmpty(dir)) return false;
            try
            {
                return Directory.Exists(Path.Combine(dir, "Database")) &&
                       (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex || File.Exists(Path.Combine(dir, "MXReflex.exe")));
            }
            catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException || e is IOException || e is UnauthorizedAccessException) { return false; }   // (not a path / not readable)
        }

        /// Why a folder is not an install (IsInstall's rule in words), "" when it is one.
        public static string Why(string dir)
        {
            if (string.IsNullOrEmpty(dir)) return "No folder chosen";
            string bad = ReflexPaths.Validate(dir, WindowsHost);
            if (bad != null) return bad;
            if (!Directory.Exists(dir)) return ReflexPaths.NotFound(dir, WindowsHost ? DriveScanner.Default.Peek()?.Select(d => d.Root) : null);
            bool exe = File.Exists(Path.Combine(dir, "MXReflex.exe")), db = Directory.Exists(Path.Combine(dir, "Database"));
            if (db && (exe || Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)) return "";
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return "No Database folder here";
            if (!exe && !db) return "No MXReflex.exe and no Database folder here";
            return exe ? "MXReflex.exe found, but no Database folder" : "A Database folder, but no MXReflex.exe";
        }

        /// The folder saved by Set (PlayerPrefs), "" when none.
        public static string Saved { get { try { return PlayerPrefs.GetString(PrefsKey, ""); } catch (Exception) { return ""; } } }

        /// Tests: act as if no install were found (the first-run CONNECT FOLDER) until Set is called or this is cleared.
        public static bool SimulateMissing;

        public static bool WindowsHost => Path.DirectorySeparatorChar == '\\';
        /// A typed / saved path as one normal path (ReflexPaths.Clean for this machine).
        public static string Clean(string value) => ReflexPaths.Clean(value, WindowsHost, SafeHome(), n => Environment.GetEnvironmentVariable(n));
        static string SafeHome() { try { return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile); } catch (Exception) { return null; } }

        /// remap: also the saved folders on the other drives (this can wait up to a second and a half for a slow drive: not for the UI thread).
        public static IEnumerable<string> Candidates(bool remap = true)
        {
            string env = Clean(Environment.GetEnvironmentVariable("REFLEX_GAME_DIR"));
            if (!string.IsNullOrEmpty(env)) yield return env;
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && File.Exists(Path.Combine(ReflexAssetImport.Destination, ReflexAssetImport.ExtractedMarker))) yield return ReflexAssetImport.Destination;
            string pref = null;
            try { pref = Clean(PlayerPrefs.GetString(PrefsKey, "")); } catch (Exception) { }
            if (!string.IsNullOrEmpty(pref)) yield return pref;
            string data = Application.dataPath, fromFile = null;
            foreach (string baseDir in new[] { Path.GetDirectoryName(data), data })
            {
                if (string.IsNullOrEmpty(baseDir)) continue;
                string txt = Path.Combine(baseDir, "reflex-game-dir.txt");
                if (!File.Exists(txt)) continue;
                string line = null;
                try { line = ReflexPaths.FirstPathLine(File.ReadAllText(txt), WindowsHost, SafeHome(), n => Environment.GetEnvironmentVariable(n)); } catch (Exception) { }
                if (line == null) continue;
                // a relative line is relative to the file's folder (a game folder beside the player, on whatever drive the player is on)
                if (!ReflexPaths.IsRooted(line)) { try { line = Path.GetFullPath(Path.Combine(baseDir, line)); } catch (Exception) { continue; } }
                fromFile = fromFile ?? line;
                yield return line;
            }
            for (var d = new DirectoryInfo(data); d != null; d = d.Parent)
                yield return Path.Combine(d.FullName, "game files", "Archive");
            // last: the same folder on the other drives (the saved drive letter is gone)
            if (remap) foreach (var saved in new[] { pref, fromFile, Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? ReflexAssetImport.MovedDestination : null })
                if (!string.IsNullOrEmpty(saved)) foreach (var alt in Remapped(saved)) yield return alt;
        }

        static readonly Dictionary<string, (long at, List<string> alts)> s_remap = new Dictionary<string, (long, List<string>)>(StringComparer.OrdinalIgnoreCase);
        /// The saved folder on the other ready drives, when it is not found where it was saved (windows only; remembered for a few seconds).
        public static List<string> Remapped(string saved)
        {
            var none = new List<string>();
            if (!WindowsHost || !ReflexPaths.IsDriveRooted(saved)) return none;
            long now = System.Diagnostics.Stopwatch.GetTimestamp() / (System.Diagnostics.Stopwatch.Frequency / 1000);
            lock (s_remap) if (s_remap.TryGetValue(saved, out var hit) && now - hit.at < 10000) return hit.alts;
            var alts = new List<string>();
            try
            {
                bool there; try { there = IsInstall(saved); } catch (Exception) { there = false; }
                if (!there) foreach (var alt in ReflexPaths.RemapDrive(saved, DriveScanner.ReadyRoots(1500))) { bool ok; try { ok = IsInstall(alt); } catch (Exception) { ok = false; } if (ok) alts.Add(alt); }
            }
            catch (Exception) { }
            lock (s_remap) s_remap[saved] = (now, alts);
            return alts;
        }

        /// The install folder, or null when none is configured / found.
        public static string Dir
        {
            get
            {
                if (SimulateMissing) return null;
                if (_dir != null && IsInstall(_dir)) return _dir;
                foreach (var c in Candidates())
                    try { if (IsInstall(c)) return _dir = Path.GetFullPath(c); }
                    catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException || e is System.Security.SecurityException) { }   // (a candidate that is not a path)
                return null;
            }
        }

        public static void Set(string dir)
        {
            dir = Clean(dir);
            if (!IsInstall(dir)) throw new ArgumentException(Why(dir));
            _dir = Path.GetFullPath(dir); _install?.Close(); _install = null; SimulateMissing = false;
            PlayerPrefs.SetString(PrefsKey, _dir); PlayerPrefs.Save();
        }

        /// After the folder changed: the readers that cache the install's tables read it again (what other caches hold keeps until
        /// the next start).
        public static void ResetReaders()
        {
            _install?.Close(); _install = null;
            try { ReflexEvents.Refresh(); } catch (Exception) { }
        }

        public static string Require()
        {
            var d = Dir;
            if (d == null) throw new FileNotFoundException("MX vs ATV Reflex install not found. Set REFLEX_GAME_DIR, or PlayerPrefs '" + PrefsKey +
                                                           "', or put the folder path in reflex-game-dir.txt next to the player.");
            return d;
        }

        public static string Exe => Path.Combine(Require(), "MXReflex.exe");
        public static string DatabaseDir => Path.Combine(Require(), "Database");
        public static string PathOf(string relative) => Path.Combine(Require(), relative.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar));

        /// The install's package databases (shared, thread-safe reads).
        internal static Rfx.Install Packages
        {
            get
            {
                if (_install == null) _install = new Rfx.Install(DatabaseDir);
                return _install;
            }
        }

        // ------------------------------------------------------------------------------------------------ installs on any drive

        static List<FoundInstall> s_found; static int s_foundVersion, s_finding; static long s_foundAt;
        /// Bumped when a background search for installs (Detect) found a new set.
        public static int DetectedVersion => System.Threading.Volatile.Read(ref s_foundVersion);
        /// The installs the last search found on the ready drives (drive roots, Games / Program Files folders, Steam libraries), empty before
        /// it finished. Call Detect to search (in the background).
        public static List<FoundInstall> Detected { get { lock (s_remap) return s_found == null ? new List<FoundInstall>() : s_found.ToList(); } }

        /// Searches for installs in the background, unless the last search is younger than ttlMs: Windows, the ready drives (drive roots,
        /// Games / Program Files folders, Steam libraries from libraryfolders.vdf on every drive and the registry's Steam); under Proton (SteamOS)
        /// also the Linux Steam folders and the microSD card's libraries through Z:; macOS, Windows bottles and Steam libraries (FindPosix).
        public static void Detect(int ttlMs = 30000)
        {
            long now = Environment.TickCount;
            if (s_found != null && now - s_foundAt < ttlMs) return;
            if (System.Threading.Interlocked.CompareExchange(ref s_finding, 1, 0) != 0) return;
            var t = new System.Threading.Thread(() =>
            {
                try
                {
                    List<FoundInstall> found;
                    var fs = new SystemFinderFs();
                    if (WindowsHost)
                    {
                        var roots = DriveScanner.ReadyRoots(1500);
                        var steam = new List<string> { ReflexFs.SteamPath() };
                        if (ReflexFs.IsWine) steam.AddRange(InstallFinder.ProtonSteamRoots(Environment.GetEnvironmentVariable, ReflexAssetImport.ProtonMounts(fs, Environment.GetEnvironmentVariable("HOME"))));
                        found = InstallFinder.Find(roots, fs, IsInstall, steam, ReflexFs.IsWine ? "Z:" : null);
                    }
                    else found = InstallFinder.FindPosix(SafeHome(), fs, IsInstall);
                    lock (s_remap)
                    {
                        string was = s_found == null ? null : string.Join("|", s_found.Select(f => f.Path)), now2 = string.Join("|", found.Select(f => f.Path));
                        s_found = found; s_foundAt = Environment.TickCount;
                        if (was != now2) System.Threading.Interlocked.Increment(ref s_foundVersion);
                    }
                }
                catch (Exception) { }
                finally { System.Threading.Volatile.Write(ref s_finding, 0); }
            }) { IsBackground = true, Name = "install search" };
            t.Start();
        }

        /// A per-user cache folder for data derived from the install at run time (never shipped).
        public static string CacheDir
        {
            get
            {
                string d = Path.Combine(Application.persistentDataPath, "ReflexCache");
                Directory.CreateDirectory(d);
                return d;
            }
        }
    }
}
