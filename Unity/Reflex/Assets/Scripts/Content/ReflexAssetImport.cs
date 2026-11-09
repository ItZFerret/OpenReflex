// The game files import: the player's own install, extracted into a portable asset library; database identifiers map to organized loose
// files. Nothing here uses Unity (tests/importflow builds it on .NET with RfxData): the client's folders, the pointer to a game files folder
// on another drive and the logs are ReflexAssetImport.Unity.cs; what an import needs on disk, the free space and the disk-full handling are
// ReflexAssetImport.Space.cs; the mods found in the install's Database folder are ImportModScan.cs.
//   staging   the import writes into ".rxi-<8 hex>" next to the destination (the same drive: publication is one folder rename), which a
//             cancel, a failure or a full disk removes; one an earlier run left behind (the game was closed or crashed mid-import) can be
//             resumed (Options.resumeStage: its files are checked against the source, not trusted) or removed (ImportLeftovers)
//   failures  an ImportException: Kind DiskFull (Win32 112 / 39, ENOSPC, or the free space re-check found the drive filling up: the
//             stage is already removed), BadSourceFile (a file of the install could not be read: SourceFile names it) or Other; the
//             message keeps the old "While <step>: <why>" form for COPY ERROR DETAILS
//   retries   an antivirus or indexer holding a file for a moment: sharing / lock violations and access-denied on open / move are retried
//             with a short backoff (Retry)
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Reflex.Port.Content
{
    /// Why an import stopped (see the file header).
    public sealed class ImportException : IOException
    {
        public enum Failure { Other, DiskFull, BadSourceFile }
        public readonly Failure Kind;
        /// The install's file that could not be read (a file name of the Database folder: "MXTables_DLC008.dx9.database"), null when none.
        public readonly string SourceFile;
        /// The step it stopped at ("copying Videos/Intro.bik").
        public readonly string Step;
        /// DiskFull: the folder the import was writing to and what it still needed (0: not known).
        public string Folder; public long Needed;
        public ImportException(Failure kind, string step, string message, Exception inner, string sourceFile = null) : base(message, inner)
        { Kind = kind; Step = step; SourceFile = sourceFile; if (inner != null) HResult = inner.HResult; }
    }

    public static partial class ReflexAssetImport
    {
        public const string Marker = "reflex-assets.json";
        public const string ExtractedMarker = "reflex-extracted-v1.txt";
        /// The prefix of a staging folder (next to the destination).
        public const string StagePrefix = ".rxi-";
        public static string Segment(string value)
        {
            var result = new StringBuilder();
            foreach (byte b in Encoding.UTF8.GetBytes(value ?? ""))
                if (b >= 65 && b <= 90 || b >= 97 && b <= 122 || b >= 48 && b <= 57 || b == 95 || b == 45) result.Append((char)b); else result.Append('%').Append(b.ToString("X2"));
            if (result.Length == 0) throw new InvalidDataException("Empty asset path segment.");
            // Win32 treats these names as devices even with an extension. Escape the
            // first byte, preserving a reversible mapping and ordinary existing paths.
            string segment = result.ToString();
            string upper = segment.ToUpperInvariant();
            bool device = upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
                upper.Length == 4 && (upper.StartsWith("COM", StringComparison.Ordinal) || upper.StartsWith("LPT", StringComparison.Ordinal)) && upper[3] >= '1' && upper[3] <= '9';
            return device ? "%" + ((byte)segment[0]).ToString("X2") + segment.Substring(1) : segment;
        }
        public static string AssetPath(string database, string package, string asset, string type) => "Assets/" + Segment(type) + "/" + Segment(database) + "/" + Segment(package) + "/" + Segment(asset) + "." + Segment(type);

        // Keep the staging name short: a friendly asset path can already be 135 characters.
        // It stays beside the destination so publication is one same-volume directory move.
        internal static string NewStage(string parent) => Path.Combine(parent, StagePrefix + Guid.NewGuid().ToString("N").Substring(0, 8));
        internal static void ValidateWindowsDestination(string destination, string temporaryDirectory)
        {
            if (Inside(temporaryDirectory, destination) || Same(temporaryDirectory, destination))
                throw new IOException("Extract the complete OpenReflex ZIP to a permanent folder, then launch OpenReflex.exe from that folder. The client is currently running from a temporary folder used by an archive program.");
        }
        /// The longest full path Windows (Unity's Mono file APIs) can write: MAX_PATH less the terminator.
        public const int WindowsMaxPath = 259;
        internal static void ValidateWindowsPath(string root, string relative)
        {
            string path = Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar));
            // Unity's Windows Mono file APIs do not consistently support extended paths.
            // Check before copying, rather than leaving players waiting for a late failure.
            if (path.Length > WindowsMaxPath || path.Split(new[] { '/', '\\' }).Any(s => s.Length > 255))
                throw new IOException("The client folder is too deeply nested for this game asset. Move the complete client to a shorter path, such as " + ReflexPaths.ShortFolderExample(root) + ", and retry. Asset: " + relative);
        }
        public sealed class Progress
        {
            /// Bytes written of total (total: the measured plan's, or the loose files' only without a plan; 0 while unknown).
            public long copied, total; public int files, count; public string current;
            /// 1 copying the loose files, 2 extracting the packages' assets, 3 writing the catalog and finishing, 4 packing (Options.pack).
            public int phase;
            /// Seconds since the import started.
            public double seconds;
        }
        public sealed class Result { public string directory, previous; public int files; public long bytes; public string[] warnings = Array.Empty<string>(); public string[] skippedFiles = Array.Empty<string>(); public bool resumed; }

        /// How to run an import (all optional).
        public sealed class Options
        {
            /// File names of the install's Database folder to leave out (any case): mods the player chose not to import (ImportModScan).
            public ICollection<string> skipFiles;
            /// The measured plan (Measure): the progress has a true total over both phases, and the free space is re-checked against it.
            public ImportPlan plan;
            /// The free bytes of the destination's drive (null: FreeBytes; tests replace it).
            public Func<string, long?> freeBytes;
            /// An unfinished staging folder of an earlier import next to the destination, to go on with (null: a new one).
            public string resumeStage;
            /// TEST: an IOException like Mono's "Win32 IO returned 112" once this many bytes are written (-1: REFLEX_IMPORT_TEST_DISK_FULL_AT, else never).
            public long failAtBytes = -1;
            /// TEST: slows the copy down by this many milliseconds per file (the showcase's progress page; -1: REFLEX_IMPORT_TEST_SLOW_MS, else none).
            public int slowMs = -1;
            /// Log lines (warnings of skipped files; ReflexAssetImport.Unity.cs sends them to the player log).
            public Action<string> log;
            /// Pack the new library into Reflex archives as the import's last step (ReflexDataPack: Assets.rxdata + Tracks/<level>.rxtrack,
            /// core/plan/RXARCHIVE.md). null: yes, unless REFLEX_IMPORT_LOOSE=1 (a loose library, as before 2026-11-08).
            public bool? pack;
        }
        /// Whether an import packs its library (Options.pack).
        public static bool PackOnImport(Options options) => options?.pack ?? Environment.GetEnvironmentVariable("REFLEX_IMPORT_LOOSE") != "1";
        /// Where the import's notes go when the options name none (ReflexAssetImport.Unity.cs: the player log).
        public static Action<string> DefaultLog = s => { };

        // Fail on case-only collisions before publication on every host, so a library
        // imported on a case-sensitive disk remains portable to Windows.
        internal static void RegisterPortablePath(Dictionary<string, string> paths, string path)
        {
            if (paths.TryGetValue(path, out string existing) && !string.Equals(existing, path, StringComparison.Ordinal))
                throw new InvalidDataException("Asset paths differ only by letter case and cannot be imported portably: " + existing + " / " + path);
            paths[path] = path;
        }
        static bool Same(string a, string b) => string.Equals(Path.GetFullPath(a).TrimEnd(Path.DirectorySeparatorChar), Path.GetFullPath(b).TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase);
        static bool Inside(string parent, string child) => Path.GetFullPath(child).StartsWith(Path.GetFullPath(parent).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
        static bool Excluded(string name, bool directory)
        {
            if (name.StartsWith(".", StringComparison.Ordinal)) return true;
            // what Windows keeps at the top of a drive (the game may be installed at the root of one: D:\, a mounted disc or ISO, a USB stick)
            if (name.Equals("System Volume Information", StringComparison.OrdinalIgnoreCase) || name.Equals("$RECYCLE.BIN", StringComparison.OrdinalIgnoreCase) || name.Equals("RECYCLER", StringComparison.OrdinalIgnoreCase)
                || name.Equals("Thumbs.db", StringComparison.OrdinalIgnoreCase) || name.Equals("desktop.ini", StringComparison.OrdinalIgnoreCase) || name.Equals("autorun.inf", StringComparison.OrdinalIgnoreCase)
                || name.StartsWith("found.", StringComparison.OrdinalIgnoreCase) && directory || name.Equals("pagefile.sys", StringComparison.OrdinalIgnoreCase) || name.Equals("hiberfil.sys", StringComparison.OrdinalIgnoreCase)) return true;
            if (directory) return name.Equals("Installers", StringComparison.OrdinalIgnoreCase) || name.Equals("_CommonRedist", StringComparison.OrdinalIgnoreCase);
            string ext = Path.GetExtension(name).ToLowerInvariant();
            return new[] { ".exe", ".dll", ".msi", ".bat", ".cmd", ".com", ".ps1", ".sh", ".dylib", ".so" }.Contains(ext) || name.Equals("installscript.vdf", StringComparison.OrdinalIgnoreCase) || name.StartsWith("steam_appid", StringComparison.OrdinalIgnoreCase) || (ext == ".package" || ext == ".asset-package") || name == Marker || name == ExtractedMarker || name == "asset-index.html" || name == "asset-inventory.tsv" || name == "README - GAME FILES.txt";
        }
        static IEnumerable<string> Files(string root, CancellationToken cancel)
        {
            foreach (var entry in Directory.EnumerateFileSystemEntries(root).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                cancel.ThrowIfCancellationRequested(); var flags = File.GetAttributes(entry);
                bool directory = (flags & FileAttributes.Directory) != 0;
                if (Excluded(Path.GetFileName(entry), directory)) continue;
                if ((flags & FileAttributes.ReparsePoint) != 0 && ReflexFs.IsLink(entry)) throw new IOException("The selected folder contains a symbolic link. Select the real game asset folder (Archive) instead: " + entry);
                if (directory) { foreach (string file in Files(entry, cancel)) yield return file; }
                else yield return entry;
            }
        }
        /// A file of the source left out by the options (a Database file the player chose not to import).
        static bool Skipped(string source, string file, HashSet<string> skip)
        {
            if (skip == null || skip.Count == 0) return false;
            return skip.Contains(Path.GetFileName(file)) && Same(Path.GetDirectoryName(file), Path.Combine(source, "Database"));
        }
        /// The source's files the import copies as they are (the loose files: tables, videos, the Database folder's indexes and levels), in order.
        internal static List<string> SourceFiles(string source, HashSet<string> skip, CancellationToken cancel) => Files(source, cancel).Where(f => !Skipped(source, f, skip)).ToList();
        /// The Database folder's databases the import extracts (base names, ordinal order), without the skipped ones.
        internal static List<string> SourceDatabases(string source, HashSet<string> skip) =>
            Directory.EnumerateFiles(Path.Combine(source, "Database"), "*.dx9.database").OrderBy(x => x, StringComparer.Ordinal)
                .Where(d => !Skipped(source, d, skip)).Select(d => Path.GetFileName(d).Substring(0, Path.GetFileName(d).Length - ".dx9.database".Length)).ToList();
        internal static HashSet<string> SkipSet(ICollection<string> files) => files == null || files.Count == 0 ? null : new HashSet<string>(files, StringComparer.OrdinalIgnoreCase);

        public static Task<Result> Import(string source, string destination, Action<Progress> progress, CancellationToken cancel) => Import(source, destination, progress, cancel, null);
        public static Task<Result> Import(string source, string destination, Action<Progress> progress, CancellationToken cancel, Options options)
        {
            // Capture Unity-owned paths on the caller thread; the worker uses ordinary files only.
            source = Path.GetFullPath(source); destination = Path.GetFullPath(destination);
            return Task.Run(() => Copy(source, destination, progress, cancel, options), cancel);
        }
        static bool IsUnreferencedLevel(Rfx.Install install, string level, HashSet<string> skip, string source, CancellationToken cancel)
        {
            bool readDefault = false;
            foreach (string descriptor in Directory.EnumerateFiles(install.dir, "*.dx9.database"))
            {
                if (Skipped(source, descriptor, skip)) continue;
                string name = Path.GetFileName(descriptor).Substring(0, Path.GetFileName(descriptor).Length - ".dx9.database".Length);
                if (name != "MXTables_Default" && name.IndexOf("DLC", StringComparison.OrdinalIgnoreCase) < 0) continue;
                bool optional = name != "MXTables_Default";
                Rfx.Database db;
                try { db = install.Db(name); }
                catch (Exception e) when (optional && !(e is OperationCanceledException)) { continue; }   // (w-gearmods) an unreadable optional DLC table: skipped by the import too
                if (db == null) { if (optional) continue; throw new InvalidDataException("Cannot verify optional level references: " + name); }
                foreach (var package in db.packages.Values) foreach (var asset in package.OfType("tbldata"))
                {
                    cancel.ThrowIfCancellationRequested();
                    Dictionary<string, List<Dictionary<string, object>>> tables;
                    try { tables = Rfx.Tables(install.Read(asset, cancel)); }
                    catch (Exception e) when (optional && !(e is OperationCanceledException)) { continue; }
                    if (name == "MXTables_Default" && tables.ContainsKey("EventLocation")) readDefault = true;
                    if (tables.TryGetValue("EventLocation", out var locations)) foreach (var location in locations)
                        if (location.TryGetValue("Filename", out var value) && value is string filename && (filename.Equals(level, StringComparison.OrdinalIgnoreCase) || filename.StartsWith(level + ".", StringComparison.OrdinalIgnoreCase))) return false;
                }
            }
            if (!readDefault) throw new InvalidDataException("Cannot verify optional level references without the default EventLocation table.");
            return true;
        }

        // ------------------------------------------------------------------------------------------------ retries (antivirus, indexers)

        /// A file error that goes away by itself: another program (an antivirus scan, the search indexer, a cloud client) holds the file for a
        /// moment. Sharing (32) / lock (33) violations; access denied (5) too - Windows Defender reports a file it is scanning that way.
        public static bool IsTransient(Exception e)
        {
            if (e is UnauthorizedAccessException) return true;
            if (!(e is IOException) || e is FileNotFoundException || e is DirectoryNotFoundException || e is PathTooLongException || IsDiskFull(e)) return false;
            int code = e.HResult & 0xFFFF; bool win32 = (e.HResult & 0xFFFF0000) == 0x80070000;
            return win32 && (code == 32 || code == 33 || code == 5);
        }
        /// How long the retries wait first (milliseconds, doubled each time) and how often they try (tests make them short).
        public static int RetryFirstMs = 100, RetryTimes = 6;
        /// f(), tried again after a short wait (100, 200 .. 3200 ms) while it fails with a transient error; then the error.
        public static T Retry<T>(Func<T> f, CancellationToken cancel, int times = -1)
        {
            int wait = RetryFirstMs, left = times < 0 ? RetryTimes : times;
            while (true)
            {
                try { return f(); }
                catch (Exception e) when (left > 0 && IsTransient(e))
                {
                    left--; DefaultLog?.Invoke("[import] busy, retrying in " + wait + " ms: " + e.Message);
                    if (cancel.WaitHandle.WaitOne(wait)) cancel.ThrowIfCancellationRequested();
                    wait *= 2;
                }
            }
        }
        public static void Retry(Action f, CancellationToken cancel, int times = -1) => Retry(() => { f(); return 0; }, cancel, times);

        /// Removes a folder of the import's own (a staging folder) however it is left: read-only files made writable, busy files retried.
        /// False (nothing thrown) when something could not be removed.
        public static bool DeleteTree(string dir)
        {
            try
            {
                if (!Directory.Exists(dir)) return true;
                if (ReflexFs.IsLink(dir)) { Directory.Delete(dir); return true; }   // (never into a link)
                foreach (var f in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories))
                    try { var a = File.GetAttributes(f); if ((a & FileAttributes.ReadOnly) != 0) File.SetAttributes(f, a & ~FileAttributes.ReadOnly); } catch (Exception) { }
                Retry(() => Directory.Delete(dir, true), CancellationToken.None, 4);
                return !Directory.Exists(dir);
            }
            catch (Exception e) { DefaultLog?.Invoke("[import] could not remove " + dir + ": " + e.Message); return false; }
        }

        // ------------------------------------------------------------------------------------------------ the import

        static Result Copy(string source, string destination, Action<Progress> progress, CancellationToken cancel, Options options)
        {
            options = options ?? new Options();
            var log = options.log ?? DefaultLog;
            var skip = SkipSet(options.skipFiles);
            bool windows = Path.DirectorySeparatorChar == '\\';
            if (windows) ValidateWindowsDestination(destination, Path.GetTempPath());
            if (!Directory.Exists(Path.Combine(source, "Database"))) throw new InvalidDataException("Choose the game folder that contains Database.");
            if (Same(source, destination))
            {
                if (!File.Exists(Path.Combine(source, ExtractedMarker))) throw new IOException("This folder contains packed source files. Choose the original install and a separate client folder to extract the assets.");
                return new Result { directory = destination };
            }
            if (Inside(source, destination) || Inside(destination, source)) throw new IOException("The source and client game files folders overlap. Choose the original install folder.");
            // The folders above the install and the client are wherever the player keeps them, reached however their system reaches them
            // (SteamOS / Proton: ~/.steam/steam, /run/media, a drive letter's dosdevices link; macOS: /var): reading through them reads the
            // real files and the new folder lands in the real place. Only a link inside the selected folder (Files) or a stale link where the
            // client folder goes is refused.
            if (ReflexFs.IsLink(destination)) throw new IOException("A symbolic link is in the way of the client game files folder. Remove it and retry: " + destination);
            if (Directory.Exists(destination)) throw new IOException("The client game files folder already exists. Choose it to use those files, or move it before importing another install.");
            string parent = Path.GetDirectoryName(destination);
            Directory.CreateDirectory(parent);   // (a game files folder on another drive: <drive>\OpenReflex\game files)
            bool resume = options.resumeStage != null && Directory.Exists(options.resumeStage);
            if (resume && !Same(Path.GetDirectoryName(Path.GetFullPath(options.resumeStage)), parent)) throw new IOException("An unfinished import can only be resumed in its own folder: " + options.resumeStage);
            string stage = resume ? Path.GetFullPath(options.resumeStage) : NewStage(parent);
            string previous = null; bool published = false, stageCreated = false;
            string context = "preparing the file list", sourceFile = null;
            long failAt = options.failAtBytes >= 0 ? options.failAtBytes : EnvLong("REFLEX_IMPORT_TEST_DISK_FULL_AT", -1);
            int slowMs = options.slowMs >= 0 ? options.slowMs : (int)EnvLong("REFLEX_IMPORT_TEST_SLOW_MS", 0);
            var freeOf = options.freeBytes ?? FreeBytes;
            long written = 0, kept = 0, nextCheck = 0;   // bytes this run wrote; bytes of a resumed stage kept as they were; when the free space is asked next
            var plan = options.plan;
            int cluster = ClusterSize(parent);
            var timer = System.Diagnostics.Stopwatch.StartNew();
            // Before each write: the test fault, and every 128 MB the free space against what the plan says is still to come.
            void Wrote(long n, string target)
            {
                written += n; TestWritten = written;
                if (failAt >= 0 && written >= failAt) throw new IOException("Win32 IO returned 112. Path: " + target, unchecked((int)0x80070070));
                if (written < nextCheck) return;
                nextCheck = written + (128L << 20);
                long? free = freeOf(parent);
                if (free == null) return;
                long still = plan != null ? Math.Max(0, plan.Required(cluster) - written - kept) : 0;
                if (free.Value < Math.Max(still, FreeFloor))
                    throw new ImportException(ImportException.Failure.DiskFull, context, "While " + context + ": the drive is almost full (" + GB(free.Value) + " free, " + GB(still) + " still to write).", null) { Folder = parent, Needed = still };
            }
            try
            {
                var files = SourceFiles(source, skip, cancel);
                if (File.Exists(Path.Combine(source, ExtractedMarker))) throw new IOException("This folder already contains extracted client assets. Choose it as the game folder, or choose the original install to extract again.");
                if (!files.Any(f => Inside(Path.Combine(source, "Database"), f))) throw new InvalidDataException("The source Database folder contains no game assets.");
                if (windows)
                {
                    var checkInstall = new Rfx.Install(Path.Combine(source, "Database"));
                    try
                    {
                        void CheckPath(string path) { ValidateWindowsPath(stage, path); ValidateWindowsPath(destination, path); }
                        foreach (string file in files) CheckPath(file.Substring(source.TrimEnd(Path.DirectorySeparatorChar).Length + 1));
                        foreach (string name in SourceDatabases(source, skip))
                        {
                            cancel.ThrowIfCancellationRequested(); context = "reading " + name + ".dx9.database"; sourceFile = name + ".dx9.database";
                            Rfx.Database db;
                            try { db = checkInstall.Db(name); }
                            catch (Exception e) when (Optional(name) && !(e is OperationCanceledException)) { continue; }   // (w-gearmods) reported and skipped by the extraction below
                            if (db == null) continue;
                            foreach (var package in db.packages.Values) foreach (var asset in package.assets) CheckPath(AssetPath(db.name, package.name, asset.name, asset.type));
                        }
                        sourceFile = null;
                    }
                    finally { checkInstall.Close(); }
                }
                long looseTotal = files.Sum(f => new FileInfo(f).Length), copied = 0; int count = 0;
                long total = plan != null ? plan.looseBytes + plan.assetBytes : looseTotal;
                int totalCount = plan != null ? plan.looseFiles + plan.assetFiles : files.Count;
                if (!resume)
                {
                    if (Directory.Exists(stage) || File.Exists(stage)) throw new IOException("The import staging path already exists. Please retry the import.");
                    Directory.CreateDirectory(stage);
                }
                stageCreated = true;
                if (plan != null)
                {   // the space again, at the start (the pre-flight asked a while ago); a resumed stage already holds part of it
                    long? free = freeOf(parent);
                    long need = Math.Max(0, plan.Required(cluster) - (resume ? TreeBytes(stage) : 0));
                    if (free != null && free.Value < need + FreeFloor)
                        throw new ImportException(ImportException.Failure.DiskFull, "starting", "Not enough free space: " + GB(free.Value) + " free, " + GB(need) + " needed.", null) { Folder = parent, Needed = need };
                }
                var portablePaths = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase); var relative = new List<string>(); var buffer = new byte[256 * 1024];
                long lastReport = -100;
                void Report(int phase, string current, int done, bool force = false)
                {
                    if (!force && timer.ElapsedMilliseconds - lastReport < 100) return;
                    lastReport = timer.ElapsedMilliseconds;
                    progress?.Invoke(new Progress { copied = copied, total = total, files = done, count = totalCount, current = current, phase = phase, seconds = timer.Elapsed.TotalSeconds });
                }
                foreach (string file in files)
                {
                    cancel.ThrowIfCancellationRequested(); string path = file.Substring(source.TrimEnd(Path.DirectorySeparatorChar).Length + 1);
                    context = "copying " + path; sourceFile = Path.GetFileName(file);
                    RegisterPortablePath(portablePaths, path.Replace('\\', '/'));
                    string output = Path.Combine(stage, path); Directory.CreateDirectory(Path.GetDirectoryName(output));
                    long expected = new FileInfo(file).Length;
                    if (resume && File.Exists(output))
                    {   // an unfinished import's copy: kept when it is whole (its length), else made again
                        if (new FileInfo(output).Length == expected) { copied += expected; kept += expected; relative.Add(path.Replace('\\', '/')); count++; Report(1, path, count); continue; }
                        File.Delete(output);
                    }
                    if (slowMs > 0) Thread.Sleep(slowMs);
                    using (var input = Retry(() => new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.Read, buffer.Length, FileOptions.SequentialScan), cancel))
                    using (var target = Retry(() => new FileStream(output, FileMode.CreateNew, FileAccess.Write, FileShare.None, buffer.Length, FileOptions.SequentialScan), cancel))
                    {
                        int read; long actual = 0;
                        while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancel.ThrowIfCancellationRequested(); Wrote(read, output); target.Write(buffer, 0, read); copied += read; actual += read;
                            Report(1, path, count);
                        }
                        if (actual != expected) throw new IOException("The source changed while importing: " + path);
                        target.Flush(true);
                    }
                    relative.Add(path.Replace('\\', '/')); count++;
                }
                sourceFile = null;
                cancel.ThrowIfCancellationRequested();
                var install = new Rfx.Install(Path.Combine(source, "Database"));
                int extracted = 0; var inventory = new StringBuilder("database\tpackage\tasset\ttype\tpath\tbytes\n");
                int skipped = 0; var warnings = new List<string>();
                if (skip != null)
                    warnings.Add("Left out by your choice (mods in the game's Database folder): " + string.Join(", ", skip.OrderBy(x => x, StringComparer.OrdinalIgnoreCase)) + ".");
                Rfx.Install repairInstall = null; string repairDir = null;
                var writtenThisRun = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                try
                {
                    foreach (string dbName in SourceDatabases(source, skip))
                    {
                        cancel.ThrowIfCancellationRequested();
                        context = "reading database " + dbName; sourceFile = dbName + ".dx9.database";
                        Rfx.Database db;
                        // (w-gearmods) an optional DLC database (official DLC, or a community gear / kit / track pack the player put into Database)
                        // that this version cannot read is skipped with a warning naming the file and why; the rest of the game imports. A user's
                        // "Import failed: While reading MXTables_DLC008.dx9.database: Specified cast is not valid" was one such file.
                        try { db = install.Db(dbName); if (db == null) throw new InvalidDataException("the database did not load"); }
                        catch (Exception e) when (Optional(dbName) && !(e is OperationCanceledException))
                        {
                            SkipOptional(dbName, e, stage, relative, ref copied, ref count, warnings, null, log);
                            continue;
                        }
                        if (db == null) throw new InvalidDataException("Game database did not load: " + dbName);
                        skipped += db.skippedResources;
                        var dbInstall = install;   // where this database's assets are read (the repaired SX_FresnoB: its scratch folder)
                        if (dbName.Equals(FresnoBRepair.Level, StringComparison.OrdinalIgnoreCase))
                        {
                            string damage = null;
                            try { foreach (var package in db.packages.Values) foreach (var asset in package.assets) install.Read(asset, cancel); }
                            catch (InvalidDataException error) { damage = error.Message; }
                            if (damage != null)
                            {
                                if (!IsUnreferencedLevel(install, dbName, skip, source, cancel)) throw new InvalidDataException(damage);
                                // RESTORATION (w-fresnob): the shipped package of the cut level ends before its index does and stores its assets a
                                // few bytes off the index; FresnoBRepair rebuilds the index from the package itself and fills the missing assets
                                // from the install's other levels (_re/formats/fresnob_repair.py). The repaired index and scene replace the copies;
                                // the repaired package is only read here, from a scratch folder of the stage (packages are never published).
                                FresnoBRepair.Result fix = null; string why = null;
                                context = "repairing " + dbName;
                                try { if (FresnoBRepair.Needed(Path.Combine(source, "Database"))) fix = FresnoBRepair.Repair(Path.Combine(source, "Database"), cancel); else why = "the package is not the known truncated one"; }
                                catch (Exception e) when (!(e is OperationCanceledException)) { why = e.Message; }
                                if (fix != null)
                                {
                                    repairDir = Path.Combine(stage, ".fresnob-repair"); Directory.CreateDirectory(repairDir);
                                    Wrote(fix.package.Length + fix.database.Length, repairDir);
                                    File.WriteAllBytes(Path.Combine(repairDir, FresnoBRepair.Level + ".dx9.package"), fix.package);
                                    File.WriteAllBytes(Path.Combine(repairDir, FresnoBRepair.Level + ".dx9.database"), fix.database);
                                    foreach (var (ext, data) in new[] { (".dx9.database", fix.database), (".dx9.scene", fix.scene) })
                                    {
                                        string rel = relative.FirstOrDefault(x => x.Equals("Database/" + FresnoBRepair.Level + ext, StringComparison.OrdinalIgnoreCase)) ?? "Database/" + FresnoBRepair.Level + ext;
                                        string full = Path.Combine(stage, rel.Replace('/', Path.DirectorySeparatorChar));
                                        if (File.Exists(full)) copied -= new FileInfo(full).Length; else { relative.Add(rel); count++; }
                                        Wrote(data.Length, full); File.WriteAllBytes(full, data); copied += data.Length;
                                    }
                                    repairInstall = new Rfx.Install(repairDir);
                                    db = repairInstall.Db(dbName); dbInstall = repairInstall;
                                    if (db == null) throw new InvalidDataException("The repaired " + dbName + " database did not load.");
                                    warnings.Add(fix.Summary);
                                }
                                else
                                {
                                    warnings.Add("SX_FresnoB was excluded because its source package is damaged (" + damage + ") and could not be repaired: " + why);
                                    foreach (string file in relative.Where(x => x.StartsWith("Database/SX_FresnoB.", StringComparison.OrdinalIgnoreCase)).ToArray())
                                    {
                                        string full = Path.Combine(stage, file.Replace('/', Path.DirectorySeparatorChar)); copied -= new FileInfo(full).Length; File.Delete(full); relative.Remove(file); count--;
                                    }
                                    continue;
                                }
                            }
                        }
                        var dbWritten = new List<string>(); var dbInventory = new StringBuilder(); int dbExtracted = 0; long dbBytes = 0;
                        bool optionalDb = Optional(dbName);
                        try
                        {
                        foreach (var package in db.packages.Values) foreach (var asset in package.assets)
                        {
                            cancel.ThrowIfCancellationRequested(); string path = AssetPath(db.name, package.name, asset.name, asset.type);
                            context = "extracting " + path + " from " + package.file + " (offset " + asset.offset + ", stored bytes " + asset.size + ")"; sourceFile = package.file;
                            RegisterPortablePath(portablePaths, path);
                            byte[] raw = dbInstall.Read(asset, cancel); string target = Path.Combine(stage, path.Replace('/', Path.DirectorySeparatorChar));
                            if (writtenThisRun.Contains(path))
                            {   // the same asset twice in this install
                                byte[] previousBytes = File.ReadAllBytes(target);
                                if (!previousBytes.SequenceEqual(raw)) throw new InvalidDataException("Conflicting duplicate extracted asset: " + path);
                                continue;
                            }
                            foreach (string value in new[] { db.name, package.name, asset.name, asset.type, path }) if (value.IndexOfAny(new[] { '\t', '\r', '\n' }) >= 0) throw new InvalidDataException("Asset identifier contains a line separator.");
                            bool whole = resume && File.Exists(target) && new FileInfo(target).Length == raw.Length && File.ReadAllBytes(target).SequenceEqual(raw);   // an unfinished import's copy
                            if (whole) kept += raw.Length;
                            else
                            {
                                Directory.CreateDirectory(Path.GetDirectoryName(target));
                                Wrote(raw.Length, target);
                                if (slowMs > 0 && (dbExtracted & 15) == 0) Thread.Sleep(slowMs);
                                Retry(() => File.WriteAllBytes(target, raw), cancel);
                            }
                            dbWritten.Add(path); writtenThisRun.Add(path);
                            dbBytes += raw.Length; dbExtracted++;
                            // One extraction worker yields briefly to keep the importer responsive and CPU use modest.
                            if (((extracted + dbExtracted) & 15) == 0) Thread.Sleep(2);
                            dbInventory.Append(db.name).Append('\t').Append(package.name).Append('\t').Append(asset.name).Append('\t').Append(asset.type).Append('\t').Append(path).Append('\t').Append(raw.Length).Append('\n');
                            if (timer.ElapsedMilliseconds - lastReport >= 100)
                            {
                                lastReport = timer.ElapsedMilliseconds;
                                progress?.Invoke(new Progress { copied = copied + dbBytes, total = plan != null ? total : 0, files = count + extracted + dbExtracted, count = plan != null ? totalCount : 0, current = path, phase = 2, seconds = timer.Elapsed.TotalSeconds });
                            }
                        }
                        }
                        catch (Exception e) when (optionalDb && !(e is OperationCanceledException) && !(e is ImportException) && !IsDiskFull(e))
                        {   // (w-gearmods) an optional DLC database with an unreadable asset: none of its assets are kept
                            foreach (string w in dbWritten) { try { File.Delete(Path.Combine(stage, w.Replace('/', Path.DirectorySeparatorChar))); } catch (IOException) { } writtenThisRun.Remove(w); }
                            SkipOptional(dbName, e, stage, relative, ref copied, ref count, warnings, context, log);
                            continue;
                        }
                        relative.AddRange(dbWritten); copied += dbBytes; extracted += dbExtracted; inventory.Append(dbInventory);
                    }
                    sourceFile = null;
                }
                finally
                {
                    install.Close(); repairInstall?.Close();
                    if (repairDir != null && Directory.Exists(repairDir)) DeleteTree(repairDir);
                }
                if (extracted == 0) throw new InvalidDataException("No package assets were extracted.");
                context = "writing the asset catalog";
                Report(3, "Writing the file index", count + extracted, true);
                if (resume) Sweep(stage, relative, log);   // (files of the unfinished import this one does not have: a mod left out this time)
                File.WriteAllText(Path.Combine(stage, "asset-inventory.tsv"), inventory.ToString()); relative.Add("asset-inventory.tsv");
                File.WriteAllText(Path.Combine(stage, ExtractedMarker), "OPEN REFLEX extracted assets v1\nAssets: " + extracted + "\nMetadata resources without supported heap payloads: " + skipped + "\n"); relative.Add(ExtractedMarker);
                count += extracted;
                // the new library packed (Options.pack) before it is published: nothing reads the stage yet, so each archive's loose copies go as
                // soon as it is checked. A pack that cannot run (no core library) or stops on an error leaves the rest loose: a valid library
                // that GAME LIBRARY can pack later; only a cancel stops the import.
                ReflexDataPack.Result packResult = null;
                if (PackOnImport(options))
                {
                    context = "packing the game files";
                    bool core;
                    try { RxFiles.EnsureCore?.Invoke(); core = Reflex.Port.RxCore.RxArchiveNative.Available; } catch (Exception) { core = false; }
                    if (!core) log("[import] packing skipped: the core library is not available (the game files stay loose; GAME LIBRARY can pack them later)");
                    else
                        try
                        {
                            packResult = ReflexDataPack.PackNow(stage, pp => progress?.Invoke(new Progress { copied = pp.done, total = pp.total, files = pp.files, count = pp.count, current = pp.current, phase = 4, seconds = timer.Elapsed.TotalSeconds }), cancel);
                            log($"[import] packed: {packResult.archives.Count} archives, {packResult.looseBytes >> 20} MB -> {packResult.packedBytes >> 20} MB in {packResult.seconds:0.0} s");
                        }
                        catch (OperationCanceledException) { throw; }
                        catch (Exception e)
                        {
                            log("[import] packing stopped, the rest stays loose: " + e);
                            warnings.Add("Packing the game files stopped (" + e.Message + "). The files that were not packed stay loose; GAME LIBRARY > PACK GAME FILES can finish it.");
                        }
                }
                File.WriteAllText(Path.Combine(stage, Marker), ManifestJson(DateTime.UtcNow.ToString("o"), copied, relative));
                File.WriteAllText(Path.Combine(stage, "import-report.txt"), "Extracted assets: " + extracted + "\nUnsupported/nonheap metadata resources: " + skipped + "\n" + (warnings.Count == 0 ? "No source data warnings.\n" : string.Join("\n", warnings) + "\n")
                                  + (packResult != null && packResult.archives.Count > 0 ? "Packed: " + packResult.archives.Count + " archives (Assets.rxdata, Tracks/*.rxtrack), " + (packResult.looseBytes >> 20) + " MB of files in " + (packResult.packedBytes >> 20) + " MB.\n" : "")); relative.Add("import-report.txt");
                // (a packed library has no loose assets to browse: no index pages)
                if (packResult == null || Directory.Exists(Path.Combine(stage, "Assets"))) WriteIndex(stage, relative, copied, warnings);
                cancel.ThrowIfCancellationRequested();
                context = "publishing game files";
                // the pack read the stage's archives through RxFiles, which keeps them open: on Windows a folder with an open file inside
                // cannot be renamed ("Access to the path '.rxi-...' is denied", 2026-11-08), so they are closed first
                RxFiles.Release(stage);
                // (an antivirus still scanning the last files refuses the rename for a moment: up to about 25 s)
                Retry(() => Directory.Move(stage, destination), cancel, 8); published = true;
                progress?.Invoke(new Progress { copied = copied, total = copied, files = count, count = count, current = "Import complete", phase = 3, seconds = timer.Elapsed.TotalSeconds });
                return new Result { directory = destination, previous = previous, files = count, bytes = copied, warnings = warnings.ToArray(), skippedFiles = skip?.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToArray() ?? Array.Empty<string>(), resumed = resume };
            }
            catch (OperationCanceledException) { throw; }
            catch (ImportException) { throw; }
            catch (Exception e) when (IsDiskFull(e))
            {
                throw new ImportException(ImportException.Failure.DiskFull, context, "While " + context + ": " + e.Message, e) { Folder = parent, Needed = plan != null ? Math.Max(0, plan.Required(cluster) - written - kept) : 0 };
            }
            catch (Exception e)
            {
                bool data = e is InvalidDataException || e is InvalidCastException || e is FormatException || e is EndOfStreamException || e is IndexOutOfRangeException || e is ArgumentOutOfRangeException || e is OverflowException;
                bool unreadable = data || e is UnauthorizedAccessException || e is IOException && !(e is FileNotFoundException) && context.StartsWith("reading", StringComparison.Ordinal);
                var kind = sourceFile != null && unreadable ? ImportException.Failure.BadSourceFile : ImportException.Failure.Other;
                throw new ImportException(kind, context, "While " + context + ": " + e.Message, e, kind == ImportException.Failure.BadSourceFile ? sourceFile : null);
            }
            finally
            {
                // a cancel, a failure, a full disk: the unfinished stage goes (a resumed one too: it was checked and is not worth keeping now)
                if (stageCreated && !published && Directory.Exists(stage)) { RxFiles.Release(stage); DeleteTree(stage); }
            }
        }

        /// Removes the files of a resumed stage that the finished import does not list (not the catalog files written next).
        static void Sweep(string stage, List<string> relative, Action<string> log)
        {
            var keep = new HashSet<string>(relative, StringComparer.OrdinalIgnoreCase);
            int removed = 0;
            foreach (var f in Directory.EnumerateFiles(stage, "*", SearchOption.AllDirectories).ToList())
            {
                string rel = f.Substring(stage.TrimEnd(Path.DirectorySeparatorChar).Length + 1).Replace('\\', '/');
                if (keep.Contains(rel) || rel.StartsWith("Browse/", StringComparison.Ordinal)) continue;
                try { File.Delete(f); removed++; } catch (Exception) { }
            }
            if (removed > 0) log?.Invoke("[import] resumed: " + removed + " files of the unfinished import removed (not part of this import)");
        }

        static string ManifestJson(string importedUtc, long bytes, List<string> files)
        {
            string J(string s)
            {
                var b = new StringBuilder("\"");
                foreach (char c in s)
                    switch (c)
                    {
                        case '"': b.Append("\\\""); break; case '\\': b.Append("\\\\"); break;
                        case '\n': b.Append("\\n"); break; case '\r': b.Append("\\r"); break; case '\t': b.Append("\\t"); break;
                        default: if (c < ' ') b.Append("\\u").Append(((int)c).ToString("x4")); else b.Append(c); break;
                    }
                return b.Append('"').ToString();
            }
            var o = new StringBuilder("{\n    \"version\": 1,\n    \"importedUtc\": ").Append(J(importedUtc)).Append(",\n    \"bytes\": ").Append(bytes.ToString(CultureInfo.InvariantCulture)).Append(",\n    \"files\": [");
            for (int i = 0; i < files.Count; i++) o.Append(i == 0 ? "\n        " : ",\n        ").Append(J(files[i]));
            return o.Append("\n    ]\n}").ToString();
        }

        /// A DLC database: optional content (the game's own DLC, or a community pack put into Database) - the game runs without it.
        internal static bool Optional(string dbName) => dbName.IndexOf("DLC", StringComparison.OrdinalIgnoreCase) >= 0;

        /// (w-gearmods) Leaves an optional database out of the import: its copied descriptor is removed (nothing would read it) and a warning
        /// names the file and why.
        static void SkipOptional(string dbName, Exception e, string stage, List<string> relative, ref long copied, ref int count, List<string> warnings, string where, Action<string> log)
        {
            foreach (string rel in relative.Where(x => x.Equals("Database/" + dbName + ".dx9.database", StringComparison.OrdinalIgnoreCase)).ToArray())
            {
                string full = Path.Combine(stage, rel.Replace('/', Path.DirectorySeparatorChar));
                try { if (File.Exists(full)) { copied -= new FileInfo(full).Length; File.Delete(full); } } catch (IOException) { }
                relative.Remove(rel); count--;
            }
            string why = e is InvalidCastException ? "a value of an unexpected type (" + e.Message + ")" : e.Message;
            warnings.Add("Skipped " + dbName + ".dx9.database: this version cannot read it (" + why + (where != null ? "; " + where : "") + "). It is optional DLC content - "
                + "if it is a community gear or kit pack, put the pack's files into Mods/Gear instead. Everything else was imported.");
            log?.Invoke("[import] " + warnings[warnings.Count - 1]);
        }

        static string Html(string text) => text.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
        static string Friendly(string type)
        {
            switch (type.ToLowerInvariant())
            {
                case "texture": return "Textures"; case "model": return "Models"; case "surface": return "Model surfaces";
                case "terrain": case "heightfield": case "trn": return "Tracks and terrain";
                case "sound": case "soundbnk": case "fev": case "fsb": return "Audio — " + type;
                case "tbldata": return "Game tables"; case "localiz": return "Languages and text";
                case "shader": case "material": return "Materials and shaders — " + type;
                case "anim": case "animation": case "skeleton": return "Rider animation — " + type;
                case "lyr": case "component": case "ui": return "Interface — " + type;
                default: return char.ToUpperInvariant(type[0]) + type.Substring(1) + " assets";
            }
        }
        static string Link(string file) => string.Join("/", file.Split('/').Select(Uri.EscapeDataString));
        static string Page(string title) => "<!doctype html><meta charset=\"utf-8\"><title>" + Html(title) + "</title><style>body{font:16px system-ui;max-width:1000px;margin:40px auto;padding:0 24px;background:#171b20;color:#edf0f2}a{color:#9ccef5}li{margin:8px 0}code{overflow-wrap:anywhere}input{font:inherit;padding:8px;width:90%}summary{cursor:pointer;padding:12px 0}</style><h1>" + Html(title) + "</h1>";
        static void WriteIndex(string stage, List<string> files, long bytes, List<string> warnings)
        {
            var home = new StringBuilder(Page("Reflex game files"));
            if (warnings.Count > 0) { home.Append("<h2>Import notes</h2>"); foreach (string warning in warnings) home.Append("<p>").Append(Html(warning)).Append("</p>"); }
            home.Append("<p>Individual game assets, extracted and decompressed from your own install. Files keep their game-native formats; the client reads them directly. Use this index to browse by category, database and package.</p><p>").Append(files.Count).Append(" files · ").Append((bytes / 1048576d).ToString("N1", CultureInfo.InvariantCulture)).Append(" MiB</p><h2>Asset categories</h2><ul>");
            foreach (var type in files.Where(x => x.StartsWith("Assets/", StringComparison.Ordinal)).GroupBy(x => x.Split('/')[1]).OrderBy(g => g.Key))
            {
                string title = Friendly(Uri.UnescapeDataString(type.Key)), browse = Path.Combine(stage, "Browse", type.Key); Directory.CreateDirectory(browse);
                home.Append("<li><a href=\"").Append(Link("Browse/" + type.Key + "/index.html")).Append("\">").Append(Html(title)).Append("</a> · ").Append(type.Count()).Append(" files</li>");
                var category = new StringBuilder(Page(title)); category.Append("<p><a href=\"../../asset-index.html\">All categories</a></p><p>Choose a database to browse its files. Each page contains at most 500 files.</p><ul>");
                foreach (var database in type.GroupBy(x => x.Split('/')[2]).OrderBy(g => g.Key))
                {
                    var items = database.OrderBy(x => x, StringComparer.Ordinal).ToList(); int pages = (items.Count + 499) / 500;
                    for (int page = 0; page < pages; page++)
                    {
                        string filename = database.Key + "-" + (page + 1) + ".html", label = Uri.UnescapeDataString(database.Key) + (pages > 1 ? " · page " + (page + 1) + " of " + pages : "");
                        category.Append("<li><a href=\"").Append(Uri.EscapeDataString(filename)).Append("\">").Append(Html(label)).Append("</a> · ").Append(Math.Min(500, items.Count - page * 500)).Append(" files</li>");
                        var listing = new StringBuilder(Page(title + " / " + label)); listing.Append("<p><a href=\"index.html\">Databases</a> · <a href=\"../../asset-index.html\">All categories</a></p><input type=\"search\" placeholder=\"Filter filenames on this page\" oninput=\"for(const e of document.querySelectorAll('li'))e.hidden=!e.textContent.toLowerCase().includes(this.value.toLowerCase())\">");
                        foreach (var package in items.Skip(page * 500).Take(500).GroupBy(x => x.Split('/')[3]))
                        {
                            listing.Append("<details><summary>").Append(Html(Uri.UnescapeDataString(package.Key))).Append(" · ").Append(package.Count()).Append(" files</summary><ul>");
                            foreach (string file in package) listing.Append("<li><a href=\"../../").Append(Link(file)).Append("\"><code>").Append(Html(Uri.UnescapeDataString(Path.GetFileName(file)))).Append("</code></a></li>");
                            listing.Append("</ul></details>");
                        }
                        File.WriteAllText(Path.Combine(browse, filename), listing.ToString());
                    }
                }
                category.Append("</ul>"); File.WriteAllText(Path.Combine(browse, "index.html"), category.ToString());
            }
            home.Append("</ul><h2>Configuration, videos and metadata</h2><p>These loose files retain their original paths. Database descriptors are small indexes; original package payloads are excluded.</p><details><summary>Browse supporting files</summary><ul>");
            foreach (string file in files.Where(x => !x.StartsWith("Assets/", StringComparison.Ordinal))) home.Append("<li><a href=\"").Append(Link(file)).Append("\"><code>").Append(Html(file)).Append("</code></a></li>");
            home.Append("</ul></details>"); File.WriteAllText(Path.Combine(stage, "asset-index.html"), home.ToString());
            File.WriteAllText(Path.Combine(stage, "README - GAME FILES.txt"), "OPEN REFLEX — GAME FILES\n\nOpen asset-index.html to browse categories, databases and packages.\nAssets contains decompressed individual game resources in their game-native formats. Database contains only metadata indexes and loose level data; original package payloads are not needed. Loose configuration, fonts and videos keep their original relative paths. No executable, DLL or installer is needed here.\n\nYour original install was not changed. Keep these files in place while playing. Imported assets belong to your own game installation and are not bundled with the client.\n");
        }
    }
}
