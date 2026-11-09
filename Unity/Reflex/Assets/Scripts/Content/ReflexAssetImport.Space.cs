// What an import needs on disk, and where it can go (pure: System only; tests/importflow builds it on .NET):
//   Measure        the exact output of an import of an install, from the install's own package indexes: every loose file's size, every
//                  asset's decompressed size (an LZX resource's chunk headers say what each chunk decodes to: only the 8-byte headers are
//                  read, never the data), the catalog the import writes (the inventory exactly, the index pages from their names), the
//                  staging overhead (the SX_FresnoB repair's scratch package). Required(cluster) rounds every file up to the destination
//                  drive's cluster (4 KB on NTFS / APFS / ext4: ~150,000 files add ~300 MB), Margin adds room to spare.
//   FreeBytes      the free space of the drive a folder is on (Windows GetDiskFreeSpaceEx, which Proton maps to the Linux mount; elsewhere
//                  the mount with the longest matching path). TEST: REFLEX_IMPORT_TEST_FREE_BYTES = "<bytes>" for every folder, or
//                  "<folder prefix>=<bytes>;..." ("3.1G" works), less what the running import wrote (the drive fills as it goes).
//   IsDiskFull     Win32 112 (ERROR_DISK_FULL, Mono's "Win32 IO returned 112") / 39 (ERROR_HANDLE_DISK_FULL), HResult 0x80070070 /
//                  0x80070027, ENOSPC (errno 28 / "No space left on device" / Mono's "Disk full"), anywhere in the exception's chain.
//   Locations      the places the game files can go instead: every ready drive (Windows), the microSD card and other mounts under /run/media
//                  (SteamOS through Proton), the startup disk and /Volumes (macOS); each with its free space and an "OpenReflex" folder.
//   Leftovers      ".rxi-*" staging folders an earlier import left behind (the game closed or crashed mid-import): their size, to resume or remove.
//   checks         OneDrive folders (Files On-Demand uploads 20 GB and may move files off the disk), Program Files (no write access without
//                  an administrator), Windows' 260-character path limit for the deepest file the import writes.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

namespace Reflex.Port.Content
{
    /// What an import of an install writes (ReflexAssetImport.Measure).
    public sealed class ImportPlan
    {
        public string source;
        /// The files copied as they are (videos, tables, the Database folder's indexes and levels) and the assets extracted from the packages.
        public long looseBytes, assetBytes; public int looseFiles, assetFiles, folders;
        /// The catalog the import writes (asset-inventory.tsv, reflex-assets.json, the index pages, the report).
        public long catalogBytes;
        /// Written while importing and removed before the end (the SX_FresnoB repair's scratch package).
        public long stagingExtraBytes;
        /// The longest path below the game files folder ("Assets/texture/..."), for Windows' path limit.
        public int longestRelative; public string longestPath;
        /// Assets whose size could not be read from their package (counted as four times their stored size).
        public int estimated;
        /// Optional DLC databases this version cannot read (the import skips them too): "MXTables_DLC008.dx9.database: why".
        public readonly List<string> unreadable = new List<string>();
        /// A database the game needs that cannot be read (the import would fail on it): its file and why; null when none.
        public string fatalFile, fatalWhy;
        /// How long the measurement took (seconds).
        public double seconds;
        internal readonly List<long> sizes = new List<long>();
        /// The import's output (bytes of the files, not rounded).
        public long Bytes => looseBytes + assetBytes + catalogBytes;
        /// What the import takes on a drive with this cluster size: every file and folder rounded up to whole clusters, plus the catalog and
        /// the staging overhead.
        public long Required(int cluster)
        {
            if (cluster <= 0) cluster = 4096;
            long n = 0; foreach (long s in sizes) n += (s + cluster - 1) / cluster * cluster;
            return n + (long)folders * cluster + RoundUp(catalogBytes, cluster) + catalogFiles * (long)cluster + stagingExtraBytes;
        }
        internal int catalogFiles;
        static long RoundUp(long v, int c) => (v + c - 1) / c * c;
        /// Room to spare on top of Required: 1 GB, or 5 % of a bigger import.
        public static long Margin(long required) => Math.Max(1L << 30, required / 20);
    }

    /// A place the game files can go (ReflexAssetImport.Locations).
    public sealed class ImportLocation
    {
        /// The drive or mount ("D:\", "Z:\run\media\mmcblk0p1", "/Volumes/Games"); the folder made there ("D:\OpenReflex"); its name.
        public string root, folder, label, kind;
        public long? free;
        /// It is where the game files go now (the client's drive).
        public bool current;
        public bool network;
        public string Destination => Path.Combine(folder, "game files");
    }

    /// An unfinished import's staging folder.
    public sealed class ImportLeftover { public string path; public long bytes; public int files; public DateTime written; }

    public static partial class ReflexAssetImport
    {
        // ------------------------------------------------------------------------------------------------ the plan

        /// Measures what an import of source (the folder holding Database) writes, leaving out skipFiles (Database file names). Reads the
        /// databases and the LZX chunk headers of the packages (a few seconds for the whole game); progress(0..1).
        public static ImportPlan Measure(string source, ICollection<string> skipFiles, CancellationToken cancel, Action<float> progress = null)
        {
            var timer = System.Diagnostics.Stopwatch.StartNew();
            source = Path.GetFullPath(source);
            var skip = SkipSet(skipFiles);
            var plan = new ImportPlan { source = source };
            var dirs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            void Folders(string rel) { for (int i = rel.LastIndexOf('/'); i > 0; i = rel.LastIndexOf('/', i - 1)) if (!dirs.Add(rel.Substring(0, i))) break; }
            void Longest(string rel) { if (rel.Length > plan.longestRelative) { plan.longestRelative = rel.Length; plan.longestPath = rel; } }
            long manifest = 200, pages = 0;
            foreach (string file in SourceFiles(source, skip, cancel))
            {
                string rel = file.Substring(source.TrimEnd(Path.DirectorySeparatorChar).Length + 1).Replace('\\', '/');
                long len = new FileInfo(file).Length;
                plan.looseBytes += len; plan.looseFiles++; plan.sizes.Add(len); Folders(rel); Longest(rel);
                manifest += rel.Length + 12; pages += rel.Length * 2 + 40;
            }
            var install = new Rfx.Install(Path.Combine(source, "Database"));
            var streams = new Dictionary<string, FileStream>(StringComparer.OrdinalIgnoreCase);
            long inventory = 40;
            try
            {
                var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                var names = SourceDatabases(source, skip);
                for (int i = 0; i < names.Count; i++)
                {
                    cancel.ThrowIfCancellationRequested();
                    progress?.Invoke(i / (float)Math.Max(1, names.Count));
                    string name = names[i];
                    Rfx.Database db;
                    try { db = install.Db(name); if (db == null) throw new InvalidDataException("the database did not load"); }
                    catch (Exception e) when (!(e is OperationCanceledException))
                    {
                        string why = e is InvalidCastException ? "a value of an unexpected type" : e.Message;
                        if (Optional(name)) plan.unreadable.Add(name + ".dx9.database: " + why);
                        else if (plan.fatalFile == null) { plan.fatalFile = name + ".dx9.database"; plan.fatalWhy = why; }
                        continue;
                    }
                    foreach (var package in db.packages.Values)
                    {
                        FileStream fs = null;
                        foreach (var asset in package.assets)
                        {
                            string path = AssetPath(db.name, package.name, asset.name, asset.type);
                            if (!seen.Add(path)) continue;
                            long size;
                            try
                            {
                                if (fs == null && !streams.TryGetValue(package.file, out fs))
                                    streams[package.file] = fs = new FileStream(Path.Combine(install.dir, package.file), FileMode.Open, FileAccess.Read, FileShare.Read, 64);
                                size = OutputSize(fs, asset);
                            }
                            catch (Exception e) when (!(e is OperationCanceledException)) { size = 4L * Math.Max(0, asset.size); plan.estimated++; }
                            plan.assetBytes += size; plan.assetFiles++; plan.sizes.Add(size); Folders(path); Longest(path);
                            inventory += db.name.Length + package.name.Length + asset.name.Length + asset.type.Length + path.Length + 16;
                            manifest += path.Length + 12; pages += path.Length + asset.name.Length + 50;
                        }
                    }
                }
            }
            finally { install.Close(); foreach (var s in streams.Values) s.Dispose(); }
            string fresno = Path.Combine(source, "Database", FresnoBRepair.Level + ".dx9.package");
            if (File.Exists(fresno) && (skip == null || !skip.Contains(FresnoBRepair.Level + ".dx9.package"))) plan.stagingExtraBytes = 2 * new FileInfo(fresno).Length;
            // packing at the end (Options.pack, ReflexDataPack): one archive at a time next to its loose copies, the biggest the shared assets'
            // (about a fifth of the extracted bytes at zstd level 9)
            if (PackOnImport(null)) plan.stagingExtraBytes += plan.assetBytes / 5;
            plan.folders = dirs.Count + 2;
            // the catalog: the inventory and the manifest from the names, the index pages (one per type and database, 500 files a page)
            plan.catalogBytes = inventory + manifest + pages + 64 * 1024;
            plan.catalogFiles = 6 + plan.assetFiles / 500 + 200;
            plan.seconds = timer.Elapsed.TotalSeconds;
            progress?.Invoke(1);
            return plan;
        }

        /// What an asset decodes to: its stored size, or for an LZX resource the sum of its chunks' sizes as Rfx.Lzx decodes them (the chunk
        /// headers only: a u32 limit, then per chunk u32 capacity + u32 packed size; the u32 after the limit is the last chunk's length).
        internal static long OutputSize(Stream fs, Rfx.Asset a)
        {
            if (!a.lzx) return a.size;
            var b = new byte[8];
            void At(long offset, int n)
            {
                fs.Position = a.offset + offset; int got = 0;
                while (got < n) { int r = fs.Read(b, got, n - got); if (r <= 0) throw new EndOfStreamException("truncated package"); got += r; }
            }
            At(0, 4); long limit = BitConverter.ToInt32(b, 0);
            if (limit < 4 || limit > a.size) throw new InvalidDataException("LZX container");
            int tail = (int)Math.Min(4, a.size - limit); long last = 0;
            if (tail > 0) { Array.Clear(b, 0, 8); At(limit, tail); last = BitConverter.ToUInt32(b, 0); }
            long p = 4, output = 0;
            while (p < limit)
            {
                At(p, 8); long capacity = BitConverter.ToInt32(b, 0), packed = BitConverter.ToInt32(b, 4); p += 8;
                if (capacity <= 0 || capacity > 65536 || packed <= 0 || p + packed > limit) throw new InvalidDataException("LZX chunk");
                long end = p + packed, size = end == limit && last > 0 ? last : capacity;
                if (end == limit && last > capacity && last > output && last - output <= capacity) size = last - output;
                output += size; p = end;
            }
            return output;
        }

        // ------------------------------------------------------------------------------------------------ free space

        /// The import does not start, and stops, while the drive would have less than this left (Windows and the Steam Deck need some room).
        public static long FreeFloor = 256L << 20;
        /// TEST: bytes the running import wrote (REFLEX_IMPORT_TEST_FREE_BYTES shrinks by them, as a real drive would fill).
        public static long TestWritten;

        static long EnvLong(string name, long def)
        {
            string v = Environment.GetEnvironmentVariable(name);
            return !string.IsNullOrWhiteSpace(v) && TryBytes(v.Trim(), out long n) ? n : def;
        }
        /// "123", "3.1G", "500M", "2T" -> bytes.
        internal static bool TryBytes(string s, out long n)
        {
            n = 0; if (string.IsNullOrEmpty(s)) return false;
            double mul = 1; char u = char.ToUpperInvariant(s[s.Length - 1]);
            if (u == 'B' && s.Length > 1) { s = s.Substring(0, s.Length - 1); u = char.ToUpperInvariant(s[s.Length - 1]); }
            if (u == 'K') mul = 1L << 10; else if (u == 'M') mul = 1L << 20; else if (u == 'G') mul = 1L << 30; else if (u == 'T') mul = 1L << 40;
            if (mul > 1) s = s.Substring(0, s.Length - 1);
            if (!double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out double d) || d < 0) return false;
            n = (long)(d * mul); return true;
        }
        /// The test override for a folder: "<bytes>" for every folder, or "<prefix>=<bytes>;<prefix>=<bytes>;*=<bytes>" (the longest prefix
        /// that matches, any case and separator); null when it names none.
        public static long? TestFreeFor(string spec, string folder)
        {
            if (string.IsNullOrWhiteSpace(spec)) return null;
            if (spec.IndexOf('=') < 0) return TryBytes(spec.Trim(), out long all) ? all : (long?)null;
            string key = (folder ?? "").Replace('\\', '/');
            long? best = null; int bestLen = -1;
            foreach (var part in spec.Split(';'))
            {
                int eq = part.LastIndexOf('='); if (eq <= 0) continue;
                string prefix = part.Substring(0, eq).Trim().Replace('\\', '/'); if (!TryBytes(part.Substring(eq + 1).Trim(), out long v)) continue;
                bool hit = prefix == "*" || key.StartsWith(prefix.TrimEnd('/'), StringComparison.OrdinalIgnoreCase) && (key.Length == prefix.TrimEnd('/').Length || key[prefix.TrimEnd('/').Length] == '/' || prefix.EndsWith("/"));
                int len = prefix == "*" ? 0 : prefix.Length;
                if (hit && len > bestLen) { best = v; bestLen = len; }
            }
            return best;
        }

        /// The free bytes of the drive the folder is on (the nearest existing folder above it); null when it cannot be told.
        public static long? FreeBytes(string folder)
        {
            long? test = TestFreeFor(Environment.GetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES"), folder);
            if (test != null) return Math.Max(0, test.Value - Interlocked.Read(ref TestWritten));
            return RealFreeBytes(folder);
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)]
        static extern bool GetDiskFreeSpaceExW(string directory, out ulong freeToCaller, out ulong total, out ulong totalFree);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)]
        static extern bool GetDiskFreeSpaceW(string root, out uint sectorsPerCluster, out uint bytesPerSector, out uint freeClusters, out uint totalClusters);

        static string Existing(string folder)
        {
            try
            {
                for (string d = Path.GetFullPath(folder); !string.IsNullOrEmpty(d); d = Path.GetDirectoryName(d))
                    if (Directory.Exists(d)) return d;
            }
            catch (Exception) { }
            return null;
        }

        static long? RealFreeBytes(string folder)
        {
            string d = Existing(folder); if (d == null) return null;
            try
            {
                if (Path.DirectorySeparatorChar == '\\')
                {
                    string dir = d.EndsWith("\\", StringComparison.Ordinal) ? d : d + "\\";
                    if (GetDiskFreeSpaceExW(dir, out ulong free, out _, out _)) return (long)Math.Min(free, long.MaxValue);
                    return null;
                }
                DriveInfo best = null; int bestLen = -1;
                foreach (var drive in DriveInfo.GetDrives())
                {
                    string root; try { root = drive.RootDirectory.FullName; } catch (Exception) { continue; }
                    string r = root.TrimEnd('/') + "/";
                    if ((d.TrimEnd('/') + "/").StartsWith(r, StringComparison.Ordinal) && r.Length > bestLen) { best = drive; bestLen = r.Length; }
                }
                return best != null && best.IsReady ? best.AvailableFreeSpace : (long?)null;
            }
            catch (Exception) { return null; }
        }

        /// The cluster size of the folder's drive (Windows; 4096 elsewhere and when it cannot be told).
        public static int ClusterSize(string folder)
        {
            if (Path.DirectorySeparatorChar != '\\') return 4096;
            try
            {
                string d = Existing(folder); string root = d == null ? null : Path.GetPathRoot(d);
                if (!string.IsNullOrEmpty(root) && GetDiskFreeSpaceW(root.EndsWith("\\", StringComparison.Ordinal) ? root : root + "\\", out uint spc, out uint bps, out _, out _))
                {
                    long c = (long)spc * bps; if (c >= 512 && c <= (2 << 20)) return (int)c;
                }
            }
            catch (Exception) { }
            return 4096;
        }

        /// The exception (or one inside it) says the disk is full (see the file header).
        public static bool IsDiskFull(Exception e)
        {
            for (var x = e; x != null; x = x.InnerException)
            {
                if (x is ImportException ie && ie.Kind == ImportException.Failure.DiskFull) return true;
                if (x is AggregateException ae && ae.InnerExceptions.Any(IsDiskFull)) return true;
                if (!(x is IOException)) continue;
                int h = x.HResult, code = h & 0xFFFF;
                if ((h & 0xFFFF0000) == 0x80070000 && (code == 112 || code == 39)) return true;
                if (h == 28) return true;   // ENOSPC (.NET on Linux / macOS: the raw errno)
                string m = x.Message ?? "";
                if (m.IndexOf("returned 112.", StringComparison.Ordinal) >= 0 || m.IndexOf("returned 39.", StringComparison.Ordinal) >= 0 || m.IndexOf("Disk full", StringComparison.OrdinalIgnoreCase) >= 0
                    || m.IndexOf("No space left on device", StringComparison.OrdinalIgnoreCase) >= 0 || m.IndexOf("not enough space on the disk", StringComparison.OrdinalIgnoreCase) >= 0) return true;
            }
            return false;
        }

        /// Bytes as the player's file browser shows them: "9.4 GB", "850 MB" (Windows / Proton: 1 GB = 1024^3 as Explorer counts; macOS: 1000^3
        /// as the Finder counts).
        public static string GB(long bytes) => GB(bytes, Path.DirectorySeparatorChar == '\\');
        public static string GB(long bytes, bool binary)
        {
            double k = binary ? 1024 : 1000, gb = bytes / (k * k * k);
            if (gb >= 100) return Math.Round(gb).ToString("0", CultureInfo.InvariantCulture) + " GB";
            if (gb >= 0.95) return gb.ToString("0.0", CultureInfo.InvariantCulture) + " GB";
            double mb = bytes / (k * k);
            return Math.Max(1, Math.Round(mb)).ToString("0", CultureInfo.InvariantCulture) + " MB";
        }

        /// The verdict on a drive: it has room for the import and the margin.
        public static bool Enough(long? free, long required) => free == null || free.Value >= required + ImportPlan.Margin(required);

        /// "C:", "D: (Games)", "the microSD card", "your startup disk", "Games" (a macOS volume), "\\nas\share": how the player knows the drive.
        public static string PlaceName(string path, string label = null)
        {
            if (string.IsNullOrEmpty(path)) return "the drive";
            string p = path.Replace('/', '\\');
            if (p.StartsWith("Z:\\run\\media\\", StringComparison.OrdinalIgnoreCase) || path.StartsWith("/run/media/", StringComparison.Ordinal))
            {
                string rest = p.Substring(p.IndexOf("media\\", StringComparison.OrdinalIgnoreCase) + 6);
                string first = rest.Split('\\')[0];
                return first.StartsWith("mmcblk", StringComparison.OrdinalIgnoreCase) ? "the microSD card" : "the drive " + rest.Split('\\').Last(x => x.Length > 0);
            }
            if (p.StartsWith("Z:\\home\\", StringComparison.OrdinalIgnoreCase) || path.StartsWith("/home/", StringComparison.Ordinal)) return "the internal storage";
            if (ReflexPaths.HasDrive(path)) return char.ToUpperInvariant(path[0]) + ":" + (string.IsNullOrEmpty(label) ? "" : " (" + label + ")");
            if (ReflexPaths.IsUnc(path)) return path.Substring(0, ReflexPaths.RootLength(path)).TrimEnd('\\', '/');
            if (path.StartsWith("/Volumes/", StringComparison.Ordinal)) return path.Substring(9).Split('/')[0];
            return "your startup disk";
        }

        /// The root of the drive or mount a folder is on, as Locations lists them ("D:\", "Z:\run\media\mmcblk0p1", "/Volumes/Games", "/").
        public static string RootOf(string path)
        {
            if (string.IsNullOrEmpty(path)) return null;
            string p = path.Replace('/', '\\');
            if (p.StartsWith("Z:\\run\\media\\", StringComparison.OrdinalIgnoreCase))
            {
                var parts = p.Substring(13).Split(new[] { '\\' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 0) return null;
                // /run/media/<card> or /run/media/<user>/<card>
                bool user = parts.Length >= 2 && !parts[0].StartsWith("mmcblk", StringComparison.OrdinalIgnoreCase) && parts[0].IndexOf('-') < 0 && parts[0] == parts[0].ToLowerInvariant() && !parts[0].Any(char.IsDigit);
                return "Z:\\run\\media\\" + (user ? parts[0] + "\\" + parts[1] : parts[0]);
            }
            if (ReflexPaths.HasDrive(path)) return char.ToUpperInvariant(path[0]) + ":\\";
            if (ReflexPaths.IsUnc(path)) return path.Substring(0, ReflexPaths.RootLength(path));
            if (path.StartsWith("/Volumes/", StringComparison.Ordinal)) return "/Volumes/" + path.Substring(9).Split('/')[0];
            return "/";
        }

        /// The places the game files can go (see the file header). drives: the ready drives (Windows; null elsewhere); mounts: extra mount
        /// folders (Proton's Z:\run\media\..., macOS /Volumes/...); home: the user's home (macOS: where a startup-disk folder goes);
        /// current: the folder the game files go into now (its place is marked current); free: the free space query.
        public static List<ImportLocation> Locations(IEnumerable<DriveEntry> drives, IEnumerable<string> mounts, string home, string current, Func<string, long?> free)
        {
            var o = new List<ImportLocation>();
            string currentRoot = RootOf(current), currentParent = current == null ? null : ReflexPaths.Parent(current);
            void Add(string root, string folder, string label, string kind, bool network = false)
            {
                if (o.Any(x => ReflexPaths.SamePath(x.root, root))) return;
                // the drive the game files go to now: its folder is where they go now
                bool here = currentRoot != null && ReflexPaths.SamePath(currentRoot, root);
                if (here && currentParent != null) folder = currentParent;
                long? f = null; try { f = free(folder); } catch (Exception) { }
                o.Add(new ImportLocation { root = root, folder = folder, label = label, kind = kind, free = f, network = network, current = here });
            }
            if (drives != null)
                foreach (var d in drives)
                {
                    if (d.Optical) continue;
                    string root = d.Root.EndsWith("\\", StringComparison.Ordinal) ? d.Root : d.Root + "\\";
                    if (root.StartsWith("Z:", StringComparison.OrdinalIgnoreCase) && mounts != null && mounts.Any()) continue;   // (Proton's Z: is the whole Linux file system: its mounts are listed instead)
                    Add(root, root + "OpenReflex", PlaceName(root, d.Label), d.Kind, d.Network);
                }
            if (mounts != null)
                foreach (var m in mounts)
                {
                    if (string.IsNullOrEmpty(m)) continue;
                    bool posix = m.StartsWith("/", StringComparison.Ordinal);
                    string root = m.TrimEnd('\\', '/');
                    if (posix && root.Length == 0) root = "/";
                    string folder = posix && root == "/" && !string.IsNullOrEmpty(home) ? Path.Combine(home, "OpenReflex") : root + (posix ? "/" : "\\") + "OpenReflex";
                    string label = PlaceName(root);
                    Add(root, folder, label, root.IndexOf("mmcblk", StringComparison.OrdinalIgnoreCase) >= 0 ? "MICROSD" : "DRIVE");
                }
            return o;
        }

        /// The mounts under /run/media (SteamOS: the microSD card at /run/media/mmcblk0p1 or /run/media/deck/<label>, USB drives) seen through
        /// Proton's Z: drive, and the home folder (internal storage); empty when Z:\run\media is not there.
        public static List<string> ProtonMounts(IFinderFs fs, string unixHome)
        {
            var o = new List<string>();
            const string media = "Z:\\run\\media";
            if (!fs.DirExists(media)) return o;
            if (!string.IsNullOrEmpty(unixHome)) o.Add("Z:" + unixHome.Replace('/', '\\').TrimEnd('\\'));
            foreach (var d in fs.SubDirs(media).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                string name = ReflexPaths.LabelOf(d);
                // /run/media/<card> is a mount; /run/media/<user>/ holds the mounts of that user
                var inner = fs.SubDirs(d).ToList();
                bool userFolder = !name.StartsWith("mmcblk", StringComparison.OrdinalIgnoreCase) && inner.Count > 0 && !fs.DirExists(d + "\\steamapps") && inner.All(x => !ReflexPaths.LabelOf(x).Equals("steamapps", StringComparison.OrdinalIgnoreCase)) && name == name.ToLowerInvariant() && !name.Any(char.IsDigit);
                if (userFolder) foreach (var m in inner.OrderBy(x => x, StringComparer.OrdinalIgnoreCase)) o.Add(media + "\\" + name + "\\" + ReflexPaths.LabelOf(m));
                else o.Add(media + "\\" + name);
            }
            return o;
        }

        // ------------------------------------------------------------------------------------------------ leftovers

        /// The bytes of the files in a folder (0 when it cannot be read).
        public static long TreeBytes(string dir)
        {
            long n = 0;
            try { foreach (var f in new DirectoryInfo(dir).EnumerateFiles("*", SearchOption.AllDirectories)) n += f.Length; } catch (Exception) { }
            return n;
        }

        /// The staging folders of unfinished imports in parent (newest first; sizes measured: this walks them).
        public static List<ImportLeftover> Leftovers(string parent)
        {
            var o = new List<ImportLeftover>();
            try
            {
                if (string.IsNullOrEmpty(parent) || !Directory.Exists(parent)) return o;
                foreach (var d in Directory.GetDirectories(parent, StagePrefix + "*"))
                {
                    if (ReflexFs.IsLink(d)) continue;
                    var l = new ImportLeftover { path = d, written = Directory.GetLastWriteTime(d) };
                    try { foreach (var f in new DirectoryInfo(d).EnumerateFiles("*", SearchOption.AllDirectories)) { l.bytes += f.Length; l.files++; if (f.LastWriteTime > l.written) l.written = f.LastWriteTime; } } catch (Exception) { }
                    o.Add(l);
                }
                foreach (var f in Directory.GetFiles(parent, StagePrefix + "write-*")) { try { File.Delete(f); } catch (Exception) { } }   // (a write probe left by a crash: ours)
            }
            catch (Exception) { }
            return o.OrderByDescending(x => x.written).ToList();
        }

        // ------------------------------------------------------------------------------------------------ checks

        /// The folder is inside OneDrive (its synced folder, by the OneDrive* variables or a "OneDrive" / "OneDrive - <org>" folder name).
        public static bool InOneDrive(string path, Func<string, string> env)
        {
            if (string.IsNullOrEmpty(path)) return false;
            foreach (var v in new[] { "OneDrive", "OneDriveConsumer", "OneDriveCommercial" })
            {
                string root = env?.Invoke(v);
                if (!string.IsNullOrEmpty(root) && (ReflexPaths.SamePath(root, path) || ReflexPaths.IsUnder(root, path))) return true;
            }
            foreach (var seg in path.Split('\\', '/'))
                if (seg.Equals("OneDrive", StringComparison.OrdinalIgnoreCase) || seg.StartsWith("OneDrive - ", StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }

        /// The folder is under Program Files (writing there needs an administrator; Windows may redirect writes to the VirtualStore).
        public static bool InProgramFiles(string path, Func<string, string> env)
        {
            if (string.IsNullOrEmpty(path)) return false;
            foreach (var v in new[] { "ProgramFiles", "ProgramFiles(x86)", "ProgramW6432" })
            {
                string root = env?.Invoke(v);
                if (!string.IsNullOrEmpty(root) && (ReflexPaths.SamePath(root, path) || ReflexPaths.IsUnder(root, path))) return true;
            }
            int r = ReflexPaths.RootLength(path);
            string first = path.Substring(r).Split('\\', '/').FirstOrDefault() ?? "";
            return ReflexPaths.HasDrive(path) && (first.Equals("Program Files", StringComparison.OrdinalIgnoreCase) || first.Equals("Program Files (x86)", StringComparison.OrdinalIgnoreCase));
        }

        /// The deepest path the import writes under destination, through its staging folder (".rxi-xxxxxxxx" is as long as "game files"):
        /// longer than Windows allows (WindowsMaxPath) means the import cannot finish there.
        public static int DeepestPath(string destination, ImportPlan plan) =>
            plan == null ? 0 : Math.Max(destination.TrimEnd('\\', '/').Length, (ReflexPaths.Parent(destination) ?? "").TrimEnd('\\', '/').Length + 1 + StagePrefix.Length + 8) + 1 + plan.longestRelative;

        /// Minutes an import of this plan takes, roughly (low, high): ~25 to ~110 MB/s of output, a hard disk to an SSD, the decompression
        /// included (measured: the stock game, 15.5 GB written, in 4.8 minutes on an M-series Mac's SSD, 51 MB/s: tests/importflow).
        public static (int low, int high) Minutes(ImportPlan plan)
        {
            if (plan == null) return (5, 20);
            double mb = plan.Bytes / 1048576.0;
            return (Math.Max(1, (int)Math.Round(mb / 110 / 60)), Math.Max(2, (int)Math.Ceiling(mb / 25 / 60)));
        }
    }
}
