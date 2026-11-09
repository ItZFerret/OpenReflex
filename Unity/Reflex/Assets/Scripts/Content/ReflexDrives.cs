// Drives and places of the install / import code (System only, so tests/paths compiles it outside Unity): which drives are ready
// right now, where a game install may be on any of them, and whether a folder is a link to elsewhere.
//   DriveScanner   every drive the system lists (fixed, USB / removable, mounted ISOs and optical drives, mapped network drives, any
//                  letter), each asked on its own thread: an empty optical drive, a card reader without a card or an unreachable network
//                  drive can block for many seconds, so the answer waits a bounded time and leaves the others out (never a hang); the
//                  answer is cached for a few seconds because a folder browser asks on every step
//   InstallFinder  the installs on the ready drives: the drive root itself (a mounted game disc or ISO), "Games" and "Program Files"
//                  folders, Steam libraries (the default folders of every drive plus the libraries libraryfolders.vdf lists)
//   ReflexFs       whether a folder is a link (symbolic link or junction) to another place, which the importers refuse as a destination
//                  and a source; a volume mounted into a folder, a cloud placeholder (OneDrive) or the root of a drive is a real place
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content
{
    public sealed class DriveEntry
    {
        /// "D:\" (the root of the drive).
        public string Root;
        /// The volume label ("" when it has none).
        public string Label = "";
        /// DRIVE, USB / REMOVABLE, DISC, NETWORK or RAM DISK.
        public string Kind = "DRIVE";
        public bool Network, Optical;
        public override string ToString() => Root + " " + Kind + (Label.Length > 0 ? " " + Label : "");
    }

    public interface IDriveSource
    {
        /// Every drive the system knows (ready or not).
        List<DriveEntry> List();
        /// The volume label when the drive is ready (may be ""), null when it is not. May block for a long time.
        string Probe(DriveEntry drive);
    }

    public sealed class SystemDriveSource : IDriveSource
    {
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetThreadErrorMode(uint newMode, out uint oldMode);
        const uint SEM_FAILCRITICALERRORS = 1;
        readonly Dictionary<string, DriveInfo> _info = new Dictionary<string, DriveInfo>(StringComparer.OrdinalIgnoreCase);

        public List<DriveEntry> List()
        {
            var o = new List<DriveEntry>();
            foreach (var d in DriveInfo.GetDrives())
            {
                try
                {
                    string root = d.Name; var e = new DriveEntry { Root = root };
                    switch (d.DriveType)
                    {
                        case DriveType.Removable: e.Kind = "USB / REMOVABLE"; break;
                        case DriveType.CDRom: e.Kind = "DISC"; e.Optical = true; break;
                        case DriveType.Network: e.Kind = "NETWORK"; e.Network = true; break;
                        case DriveType.Ram: e.Kind = "RAM DISK"; break;
                        case DriveType.NoRootDirectory: continue;
                    }
                    lock (_info) _info[root] = d;
                    o.Add(e);
                }
                catch (Exception) { }
            }
            return o;
        }

        public string Probe(DriveEntry drive)
        {
            DriveInfo d; lock (_info) if (!_info.TryGetValue(drive.Root, out d)) return null;
            uint old = 0; bool set = false;
            // no "There is no disk in the drive" box for an empty optical drive or a card reader
            if (Path.DirectorySeparatorChar == '\\') { try { set = SetThreadErrorMode(SEM_FAILCRITICALERRORS, out old); } catch (Exception) { } }
            try
            {
                if (!d.IsReady) return null;
                try { return d.VolumeLabel ?? ""; } catch (Exception) { return ""; }
            }
            finally { if (set) { try { SetThreadErrorMode(old, out _); } catch (Exception) { } } }
        }
    }

    /// The ready drives, with a bounded wait and a short cache.
    public sealed class DriveScanner
    {
        readonly IDriveSource _src; readonly object _gate = new object();
        List<DriveEntry> _cache; long _cacheAt; readonly Dictionary<string, long> _hung = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        static readonly Stopwatch Clock = Stopwatch.StartNew();
        public DriveScanner(IDriveSource src) { _src = src; }
        /// The roots a drive that did not answer in time is left out for (milliseconds).
        public int HungMs = 60000;
        /// The drives that did not answer in the last scan (not ready, or too slow).
        public List<string> Skipped = new List<string>();

        /// The ready drives. timeoutMs: the longest this waits for the slowest drive; ttlMs: how long a scan is reused (0: scan now).
        public List<DriveEntry> Ready(int timeoutMs = 1500, int ttlMs = 8000)
        {
            lock (_gate)
            {
                long now = Clock.ElapsedMilliseconds;
                if (_cache != null && ttlMs > 0 && now - _cacheAt < ttlMs) return _cache.ToList();
                List<DriveEntry> all;
                try { all = _src.List() ?? new List<DriveEntry>(); } catch (Exception) { all = new List<DriveEntry>(); }
                var probes = new List<Probe>();
                var skipped = new List<string>();
                foreach (var d in all)
                {
                    if (_hung.TryGetValue(d.Root, out long since) && now - since < HungMs) { skipped.Add(d.Root); continue; }
                    var p = new Probe { drive = d };
                    p.thread = new Thread(() =>
                    {
                        try { p.label = _src.Probe(d); } catch (Exception) { p.label = null; }
                        p.done.Set();
                    }) { IsBackground = true, Name = "drive probe " + d.Root };
                    p.thread.Start();
                    probes.Add(p);
                }
                var ready = new List<DriveEntry>();
                var sw = Stopwatch.StartNew();
                foreach (var p in probes)
                {
                    int left = (int)Math.Max(0, timeoutMs - sw.ElapsedMilliseconds);
                    if (!p.done.Wait(left)) { skipped.Add(p.drive.Root); _hung[p.drive.Root] = now; continue; }
                    _hung.Remove(p.drive.Root);
                    if (p.label == null) { skipped.Add(p.drive.Root); continue; }
                    p.drive.Label = p.label; ready.Add(p.drive);
                }
                ready.Sort((a, b) => string.Compare(a.Root, b.Root, StringComparison.OrdinalIgnoreCase));
                _cache = ready; _cacheAt = Clock.ElapsedMilliseconds; Skipped = skipped;
                return ready.ToList();
            }
        }

        /// Forget the cache (a disc was inserted, a drive connected).
        public void Invalidate() { lock (_gate) _cache = null; }

        volatile List<DriveEntry> _last; int _version; int _running; string _sig;
        /// Bumped when a background scan (BeginRefresh) found other drives than the one before: a screen showing the drives shows them again.
        public int Version => Volatile.Read(ref _version);
        /// The drives of the last scan (stale or not), null before the first one finished. Never blocks.
        public List<DriveEntry> Peek() { var l = _last; return l?.ToList(); }
        /// Scans in the background unless the last scan is younger than ttlMs (or one is running); Version changes when it is done.
        public void BeginRefresh(int timeoutMs = 1500, int ttlMs = 8000)
        {
            if (Interlocked.CompareExchange(ref _running, 1, 0) != 0) return;
            var t = new Thread(() =>
            {
                try
                {
                    var now = Ready(timeoutMs, ttlMs); string sig = string.Join("|", now.Select(d => d.Root + d.Label));
                    _last = now;
                    if (sig != _sig) { _sig = sig; Interlocked.Increment(ref _version); }
                }
                catch (Exception) { }
                finally { Volatile.Write(ref _running, 0); }
            }) { IsBackground = true, Name = "drive scan" };
            t.Start();
        }

        sealed class Probe { public DriveEntry drive; public Thread thread; public string label; public readonly ManualResetEventSlim done = new ManualResetEventSlim(false); }

        /// The scanner of the system's drives.
        public static readonly DriveScanner Default = new DriveScanner(new SystemDriveSource());
        /// The roots of the ready drives on Windows ("D:\"); none elsewhere (macOS lists /Volumes itself).
        public static List<string> ReadyRoots(int timeoutMs = 1500) => Path.DirectorySeparatorChar == '\\' ? Default.Ready(timeoutMs).Select(d => d.Root).ToList() : new List<string>();
    }

    public interface IFinderFs
    {
        bool DirExists(string path);
        IEnumerable<string> SubDirs(string path);
        string ReadText(string path);
    }

    public sealed class SystemFinderFs : IFinderFs
    {
        public bool DirExists(string path) { try { return Directory.Exists(path); } catch (Exception) { return false; } }
        public IEnumerable<string> SubDirs(string path) { try { return Directory.GetDirectories(path); } catch (Exception) { return Array.Empty<string>(); } }
        public string ReadText(string path) { try { return File.Exists(path) ? File.ReadAllText(path) : null; } catch (Exception) { return null; } }
    }

    public sealed class FoundInstall { public string Path, Source; public override string ToString() => Source + ": " + Path; }

    public static class InstallFinder
    {
        // Folders of a drive that hold game folders (drive-relative; Windows style).
        static readonly string[] GameBases =
        {
            "", "Games", "Program Files (x86)", "Program Files", "Program Files (x86)\\THQ", "Program Files\\THQ", "Program Files (x86)\\THQ Nordic",
            "Program Files\\THQ Nordic", "THQ", "Games\\THQ", "Old Games", "Spiele"
        };
        // Where Steam (or a Steam library) lives by default on a drive.
        static readonly string[] SteamBases =
        {
            "Program Files (x86)\\Steam", "Program Files\\Steam", "Steam", "SteamLibrary", "Games\\Steam", "Games\\SteamLibrary", "Program Files (x86)\\SteamLibrary"
        };

        static string Join(string root, string rel)
        {
            string r = root.TrimEnd('\\', '/');
            return rel.Length == 0 ? r + "\\" : r + "\\" + rel;
        }

        /// The Reflex installs on the given drive roots ("D:\") and Steam roots: every one that isInstall accepts (or its Archive folder).
        /// unixDrive: Proton / Wine's drive of the Linux root ("Z:"): a library libraryfolders.vdf names by its Linux path ("/run/media/mmcblk0p1",
        /// the Steam Deck's microSD card) is looked for there.
        public static List<FoundInstall> Find(IEnumerable<string> roots, IFinderFs fs, Func<string, bool> isInstall, IEnumerable<string> extraSteamRoots = null, string unixDrive = null)
        {
            var found = new List<FoundInstall>(); var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            void Try(string dir, string source)
            {
                foreach (var c in new[] { dir, Join(dir, "Archive") })
                {
                    string key = c.Replace('/', '\\').TrimEnd('\\');
                    if (seen.Contains(key)) continue;
                    bool ok; try { ok = isInstall(c); } catch (Exception) { ok = false; }
                    if (ok) { string norm = c.Replace('/', '\\'); if (norm.Length > 3) norm = norm.TrimEnd('\\'); seen.Add(key); found.Add(new FoundInstall { Path = norm, Source = source }); }
                }
            }
            var libraries = new List<string>(); var libSeen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            void AddLibrary(string path, int depth)
            {
                string key = path.Replace('/', '\\').TrimEnd('\\');
                if (!libSeen.Add(key) || depth > 3) return;
                libraries.Add(key);
                string vdf = fs.ReadText(key + "\\steamapps\\libraryfolders.vdf");
                foreach (var lib in ReflexPaths.SteamLibraries(vdf, unixDrive)) AddLibrary(lib, depth + 1);
            }
            var rootList = (roots ?? Array.Empty<string>()).ToList();
            foreach (var root in rootList)
            {
                Try(root, "DRIVE ROOT");
                foreach (var b in GameBases)
                {
                    string dir = b.Length == 0 ? root : Join(root, b);
                    if (b.Length != 0 && !fs.DirExists(dir)) continue;
                    foreach (var sub in fs.SubDirs(dir)) if (ReflexPaths.IsReflexFolderName(ReflexPaths.LabelOf(sub))) Try(sub, "GAMES FOLDER");
                }
                foreach (var b in SteamBases) { string st = Join(root, b); if (fs.DirExists(Join(st, "steamapps"))) AddLibrary(st, 0); }
            }
            if (extraSteamRoots != null) foreach (var s in extraSteamRoots) if (!string.IsNullOrEmpty(s)) { string c = ReflexPaths.Clean(s, true); if (fs.DirExists(c)) AddLibrary(c, 0); }
            foreach (var lib in libraries)
            {
                string common = lib + "\\steamapps\\common";
                if (!fs.DirExists(common)) continue;
                foreach (var sub in fs.SubDirs(common)) if (ReflexPaths.IsReflexFolderName(ReflexPaths.LabelOf(sub))) Try(sub, "STEAM");
            }
            return found;
        }

        /// The Steam roots Proton's Wine shows a Windows program on SteamOS / Linux (Z: is the Linux root): the Steam client Proton names
        /// (STEAM_COMPAT_CLIENT_INSTALL_PATH), the usual Steam folders of the home, every library folder of the microSD card and other
        /// drives under /run/media (a library is the mount itself, or a SteamLibrary folder on it). env: the environment; mounts: the folders
        /// under Z:\run\media (ReflexAssetImport.ProtonMounts).
        public static List<string> ProtonSteamRoots(Func<string, string> env, IEnumerable<string> mounts)
        {
            var o = new List<string>();
            void Add(string unix) { if (!string.IsNullOrEmpty(unix) && unix[0] == '/') { string w = ReflexPaths.UnixToWine(unix); if (!o.Contains(w)) o.Add(w); } }
            Add(env("STEAM_COMPAT_CLIENT_INSTALL_PATH"));
            string home = env("HOME");
            if (!string.IsNullOrEmpty(home)) { Add(home.TrimEnd('/') + "/.steam/steam"); Add(home.TrimEnd('/') + "/.local/share/Steam"); Add(home.TrimEnd('/') + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"); }
            if (mounts != null) foreach (var m in mounts) { if (!o.Contains(m)) o.Add(m); string lib = m.TrimEnd('\\') + "\\SteamLibrary"; if (!o.Contains(lib)) o.Add(lib); }
            return o;
        }

        /// Installs on macOS: Windows bottles of CrossOver / Whisky / Heroic (their drive_c's Steam, THQ and Games folders), Steam for macOS's
        /// libraries (the game itself is Windows-only, but a library can hold a copy), ~/Games. home: the user's home folder ("/Users/x").
        public static List<FoundInstall> FindPosix(string home, IFinderFs fs, Func<string, bool> isInstall)
        {
            var found = new List<FoundInstall>(); var seen = new HashSet<string>(StringComparer.Ordinal);
            if (string.IsNullOrEmpty(home)) return found;
            home = home.TrimEnd('/');
            void Try(string dir, string source)
            {
                foreach (var c in new[] { dir, dir + "/Archive" })
                {
                    if (seen.Contains(c)) continue;
                    bool ok; try { ok = isInstall(c); } catch (Exception) { ok = false; }
                    if (ok) { seen.Add(c); found.Add(new FoundInstall { Path = c, Source = source }); }
                }
            }
            void Games(string dir, string source) { if (fs.DirExists(dir)) foreach (var sub in fs.SubDirs(dir)) if (ReflexPaths.IsReflexFolderName(ReflexPaths.LabelOf(sub))) Try(sub.Replace('\\', '/'), source); }
            var libs = new List<string>();
            void Library(string lib, int depth)
            {
                lib = lib.TrimEnd('/');
                if (libs.Contains(lib) || depth > 3) return; libs.Add(lib);
                foreach (var l in ReflexPaths.SteamLibraries(fs.ReadText(lib + "/steamapps/libraryfolders.vdf"))) if (l.StartsWith("/", StringComparison.Ordinal)) Library(l, depth + 1);
            }
            Library(home + "/Library/Application Support/Steam", 0);
            var bottles = new List<string>();
            foreach (var b in new[] { home + "/Library/Application Support/CrossOver/Bottles", home + "/Library/Containers/com.isaacmarovitz.Whisky/Bottles", home + "/Games/Heroic/Prefixes/default" })
                if (fs.DirExists(b)) foreach (var x in fs.SubDirs(b)) bottles.Add(x.Replace('\\', '/').TrimEnd('/') + "/drive_c");
            foreach (var c in bottles)
            {
                foreach (var st in new[] { "/Program Files (x86)/Steam", "/Program Files/Steam" }) if (fs.DirExists(c + st + "/steamapps")) Library(c + st, 0);
                foreach (var g in new[] { "/Program Files (x86)/THQ", "/Program Files/THQ", "/Program Files (x86)", "/Program Files", "/Games", "/GOG Games" }) Games(c + g, "BOTTLE");
            }
            foreach (var lib in libs) Games(lib + "/steamapps/common", "STEAM");
            Games(home + "/Games", "GAMES FOLDER");
            return found;
        }
    }

    /// Links and mount points (see the file header).
    public static class ReflexFs
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct FindData
        {
            public uint attributes; public System.Runtime.InteropServices.ComTypes.FILETIME created, accessed, written; public uint sizeHigh, sizeLow, reserved0, reserved1;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string name;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)] public string alternate;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)] static extern IntPtr FindFirstFileW(string name, out FindData data);
        [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)] static extern bool FindClose(IntPtr handle);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)] static extern bool GetVolumeNameForVolumeMountPointW(string mountPoint, StringBuilder volume, uint length);

        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
        static extern int RegGetValueW(UIntPtr key, string subKey, string value, uint flags, out uint type, StringBuilder data, ref uint length);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)] static extern IntPtr GetModuleHandleW(string name);
        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, BestFitMapping = false)] static extern IntPtr GetProcAddress(IntPtr module, string name);
        static int s_wine = -1;
        /// The Windows build runs under Wine / Proton (SteamOS, Linux, a Mac bottle): ntdll exports wine_get_version.
        public static bool IsWine
        {
            get
            {
                if (s_wine >= 0) return s_wine == 1;
                bool w = false;
                if (Path.DirectorySeparatorChar == '\\') { try { IntPtr nt = GetModuleHandleW("ntdll.dll"); w = nt != IntPtr.Zero && GetProcAddress(nt, "wine_get_version") != IntPtr.Zero; } catch (Exception) { } }
                s_wine = w ? 1 : 0; return w;
            }
        }

        /// Windows: the folder Steam is installed in (HKCU\Software\Valve\Steam SteamPath, on whichever drive), null elsewhere / when absent.
        public static string SteamPath()
        {
            if (Path.DirectorySeparatorChar != '\\') return null;
            try
            {
                var sb = new StringBuilder(1024); uint bytes = 2048;
                if (RegGetValueW(new UIntPtr(0x80000001u), @"Software\Valve\Steam", "SteamPath", 2u, out _, sb, ref bytes) == 0 && sb.Length > 0) return sb.ToString();
            }
            catch (Exception) { }
            return null;
        }

        /// f() on its own thread, waited for at most ms: a folder check on a network location that is offline can block for half a minute, which
        /// must not freeze the window. timedOut: it did not answer (the result is default); the thread is left to finish by itself.
        public static T Within<T>(int ms, Func<T> f, out bool timedOut)
        {
            T result = default; Exception error = null;
            var done = new ManualResetEventSlim(false);
            var t = new Thread(() => { try { result = f(); } catch (Exception e) { error = e; } finally { done.Set(); } }) { IsBackground = true, Name = "file check" };
            t.Start();
            timedOut = !done.Wait(ms);
            if (timedOut) return default;
            if (error != null) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(error).Throw();
            return result;
        }

        /// The folder or file is a symbolic link or a junction (the importers write nothing through one and read nothing from one).
        public static bool IsLink(string path)
        {
            FileAttributes a;
            try { a = File.GetAttributes(path); } catch (Exception) { return false; }
            if ((a & FileAttributes.ReparsePoint) == 0) return false;
            if (Path.DirectorySeparatorChar != '\\') return true;
            try
            {
                string p = path.TrimEnd('\\', '/');
                if (p.Length < 3 || ReflexPaths.RootLength(path) >= path.Length) return false;       // the root of a drive is that drive
                IntPtr h = FindFirstFileW(p, out FindData fd);
                if (h == new IntPtr(-1)) return true;
                FindClose(h);
                bool volume = false;
                if (fd.reserved0 == ReflexPaths.ReparseMountPoint) volume = GetVolumeNameForVolumeMountPointW(p + "\\", new StringBuilder(64), 64);
                return ReflexPaths.IsLinkTag(fd.reserved0, volume);
            }
            catch (Exception) { return true; }
        }

        /// The first part of path (itself or an ancestor) that is a link, null when none is.
        public static string LinkedAncestor(string path)
        {
            for (var d = new DirectoryInfo(Path.GetFullPath(path)); d != null; d = d.Parent)
                if (d.Exists && IsLink(d.FullName)) return d.FullName;
            return null;
        }
    }
}
