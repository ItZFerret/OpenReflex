// PACK GAME FILES (experimental; core/plan/RXARCHIVE.md): the imported game library (ReflexAssetImport: ~150 000 loose decompressed files,
// ~20 GB with the Alive tracks) packed into Reflex archives in the same folder -
//   Tracks/<level>.rxtrack   one per level (Database/<level>.dx9.level): its .dx9.database / .level / .scene (+ .package if the library
//                            has one), its extracted assets (Assets/<type>/<level>/...), an Alive level's sidecars (Database/alive-<level>-*,
//                            alive-import-<level>.json) and its alive-levels.json entry (metadata "alive")
//   Assets.rxdata            every other extracted asset, and the POOL: one copy (Pool/<sha256>) of every file that more than one archive
//                            has (a level's B variant shares most of its level's assets); the level archives hold a reference to it
//                            instead of the bytes (container version 2, RXARCHIVE.md "Dedupe"). The same bytes twice in one archive are
//                            stored once too (the core's writer).
// The core's file system and RxFiles read the archives below the loose files, so nothing that reads the library changes.
//   what is packed   the library's EFFECTIVE files: every loose file, and every packed file no loose file covers (read through RxFiles:
//                    loose first, pool references resolved). So the same code packs a loose library, packs again an archive library
//                    of container version 1 (deduped: nothing has to be unpacked first), and packs a level again whose loose files came
//                    back (an Alive re-import) - the rest of its archive carried over.
//   full / levels    FULL (no archives yet, any version 1 archive, loose shared assets, no Assets.rxdata): everything is packed again,
//                    the pool made from every file's SHA-256 (and what the old pool held). LEVELS (an all-version-2 library with loose
//                    files of some levels only): those levels' archives again, referencing what the existing pool already has.
//   order            the shared assets and the pool first, then each level; one archive at a time: written as <name>.new, every file of
//                    it read back and compared byte for byte with its source (a pool reference: its Pool/ copy), moved into place, then
//                    its loose copies removed (removeLoose). A cancel or a failure removes the archive being written; what was packed
//                    before it stays packed (a valid library: an old level archive still reads, from itself or from the pool kept).
// The import packs its new library this way as its last step (ReflexAssetImport: Options.pack); a library imported before is offered
// packing at startup (OpenShell.Pack.cs) and in GAME LIBRARY. Compression runs on worker threads in the core (zstd level 9); nothing here
// uses Unity.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    public static class ReflexDataPack
    {
        public const string DataName = "Assets.rxdata", TracksDir = "Tracks", Marker = "reflex-packed-v1.txt", PoolPrefix = "Pool/";

        public sealed class Progress
        {
            /// 0 reading (hashing), 1 packing levels, 2 packing shared assets and the pool, 3 checking, 4 removing the loose copies.
            public int phase;
            public int files, count; public long raw, stored; public string current;
            /// Bytes read, written and checked of total (each file counts three times: hashed, packed, checked).
            public long done, total;
            public double seconds;
        }
        public sealed class Result
        {
            public int levels, files; public long looseBytes, packedBytes, pooledBytes, dedupedBytes; public double seconds; public bool removed;
            public readonly List<string> archives = new List<string>();
        }

        /// What the GAME LIBRARY card says about a library (Measure: a walk of the whole folder, a worker thread).
        public sealed class State
        {
            /// Archives in use (Assets.rxdata, Tracks/*.rxtrack) and their bytes; every byte and file of the folder.
            public int archives; public long archiveBytes, totalBytes; public int totalFiles;
            /// Loose files packing would take (extracted assets, levels' files).
            public long looseBytes; public int looseFiles;
            /// Archives of container version 1 (packed before dedupe: PACK GAME FILES makes them smaller).
            public int oldArchives;
            public bool Packed => archives > 0 && looseFiles == 0;
            public bool Partly => archives > 0 && looseFiles > 0;
        }
        public static State Measure(string root)
        {
            var st = new State();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return st;
            root = Path.GetFullPath(root).TrimEnd('/', '\\');
            foreach (string f in Walk(root))
            {
                long n; try { n = new FileInfo(f).Length; } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { continue; }
                st.totalBytes += n; st.totalFiles++;
                string rel = Rel(root, f);
                if (IsArchivePath(rel)) { st.archives++; st.archiveBytes += n; if (HeaderVersion(f) == 1) st.oldArchives++; }
            }
            if (File.Exists(Path.Combine(root, ReflexAssetImport.ExtractedMarker)))
                try { var c = Collect(root, CancellationToken.None); st.looseFiles = c.looseFiles; st.looseBytes = c.looseBytes; }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is InvalidDataException) { }
            return st;
        }
        static bool IsArchivePath(string rel) => rel.Equals(DataName, StringComparison.OrdinalIgnoreCase) || rel.StartsWith(TracksDir + "/", StringComparison.OrdinalIgnoreCase) && rel.EndsWith(".rxtrack", StringComparison.OrdinalIgnoreCase) && rel.IndexOf('/', TracksDir.Length + 1) < 0;
        /// An archive file's container version from its header (0: not an archive), without opening it.
        static int HeaderVersion(string file)
        {
            try
            {
                using (var f = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    var b = new byte[8];
                    if (f.Read(b, 0, 8) != 8 || b[0] != 'R' || b[1] != 'X' || b[2] != 'A' || b[3] != 'R') return 0;
                    return BitConverter.ToInt32(b, 4);
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return 0; }
        }

        /// Whether packing has work (cheap: no walk of the whole library): loose assets or levels, or an archive packed before dedupe.
        public static bool Worth(string dir)
        {
            try
            {
                if (string.IsNullOrEmpty(dir) || !File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker))) return false;
                string assets = Path.Combine(dir, "Assets");
                if (Directory.Exists(assets) && Directory.EnumerateFiles(assets, "*", SearchOption.AllDirectories).Any(f => !Path.GetFileName(f).StartsWith(".", StringComparison.Ordinal))) return true;
                if (Directory.Exists(Path.Combine(dir, "Database")) && Directory.EnumerateFiles(Path.Combine(dir, "Database"), "*.dx9.level").Any()) return true;
                return OldArchives(dir);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return false; }
        }
        /// The library has an archive of container version 1 (packed before dedupe).
        public static bool OldArchives(string dir)
        {
            var files = new List<string>();
            if (File.Exists(Path.Combine(dir, DataName))) files.Add(Path.Combine(dir, DataName));
            string tracks = Path.Combine(dir, TracksDir);
            if (Directory.Exists(tracks)) files.AddRange(Directory.GetFiles(tracks, "*.rxtrack"));
            return files.Any(f => HeaderVersion(f) == 1);
        }

        /// What packing would do: (levels it packs, bytes it reads) - 0 levels and 0 bytes when there is nothing to pack.
        public static (int levels, long bytes) Plan(string root)
        {
            try { var c = Collect(root, CancellationToken.None); return (c.levels.Count, c.Bytes); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is InvalidDataException) { return (0, 0); }
        }

        /// Whether a library can be packed: an imported library (reflex-extracted-v1.txt) with loose files or old archives.
        public static bool CanPack(string root) => !string.IsNullOrEmpty(root) && File.Exists(Path.Combine(root, ReflexAssetImport.ExtractedMarker)) && Plan(root).bytes > 0;

        // ------------------------------------------------------------------------------------------------ what is packed

        sealed class Job
        {
            public string archive; public uint kind; public string meta; public string level;
            /// The files (paths below the root), their sizes, and whether a loose copy is there (removed once the archive is in place).
            public readonly List<(string rel, long bytes, bool loose)> files = new List<(string rel, long bytes, bool loose)>();
        }
        sealed class Collected
        {
            public readonly List<Job> levels = new List<Job>(); public Job shared; public bool full;
            public int looseFiles; public long looseBytes;
            public long Bytes => levels.Sum(j => j.files.Sum(f => f.bytes)) + (shared?.files.Sum(f => f.bytes) ?? 0);
        }

        static IEnumerable<string> Walk(string dir)
        {
            if (!Directory.Exists(dir)) yield break;
            foreach (string f in Directory.GetFiles(dir).OrderBy(x => x, StringComparer.Ordinal)) if (!Path.GetFileName(f).StartsWith(".", StringComparison.Ordinal)) yield return f;
            foreach (string d in Directory.GetDirectories(dir).OrderBy(x => x, StringComparer.Ordinal))
            {
                if (Path.GetFileName(d).StartsWith(".", StringComparison.Ordinal) || ReflexFs.IsLink(d)) continue;
                foreach (string f in Walk(d)) yield return f;
            }
        }
        static string Rel(string root, string full) => full.Substring(root.Length).TrimStart('/', '\\').Replace('\\', '/');
        static string Full(string root, string rel) => Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar));

        /// The level a library path belongs to (null: shared): Database/<level>.dx9.*, Database/alive-<level>-*, alive-import-<level>.json,
        /// Assets/<type>/<level>/...
        static string LevelOf(string rel, HashSet<string> levels)
        {
            var parts = rel.Split('/');
            if (parts.Length >= 3 && parts[0] == "Assets") return levels.TryGetValue(parts[2], out string a) ? a : null;
            if (parts.Length != 2 || parts[0] != "Database") return null;
            string n = parts[1];
            int dx = n.IndexOf(".dx9.", StringComparison.OrdinalIgnoreCase);
            if (dx > 0) return levels.TryGetValue(n.Substring(0, dx), out string b) ? b : null;
            if (n.StartsWith("alive-import-", StringComparison.OrdinalIgnoreCase) && n.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                return levels.TryGetValue(n.Substring(13, n.Length - 18), out string d) ? d : null;
            if (n.StartsWith("alive-", StringComparison.OrdinalIgnoreCase))
                foreach (string l in levels) if (n.Length > 7 + l.Length && n.StartsWith("alive-" + l + "-", StringComparison.OrdinalIgnoreCase)) return l;
            return null;
        }

        static Collected Collect(string root, CancellationToken cancel)
        {
            root = Path.GetFullPath(root).TrimEnd('/', '\\');
            string db = Path.Combine(root, "Database"), assets = Path.Combine(root, "Assets");
            if (!Directory.Exists(db)) throw new InvalidDataException("not a game library (no Database folder): " + root);
            var c = new Collected();
            // the effective files: loose (the extracted assets, the levels' Database files and sidecars) and packed (the pool's own left out)
            var loose = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string f in Walk(assets)) loose.Add(Rel(root, f));
            foreach (string f in Directory.GetFiles(db)) loose.Add(Rel(root, f));
            var packed = RxFiles.PackedPaths(root);
            var levels = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string p in loose.Concat(packed))
                if (p.StartsWith("Database/", StringComparison.OrdinalIgnoreCase) && p.EndsWith(".dx9.level", StringComparison.OrdinalIgnoreCase) && p.IndexOf('/', 9) < 0)
                    levels.Add(p.Substring(9, p.Length - 9 - ".dx9.level".Length));
            var all = new SortedDictionary<string, bool>(StringComparer.Ordinal);   // path -> a loose copy is there
            foreach (string p in loose) if (p.StartsWith("Assets/", StringComparison.Ordinal) || LevelOf(p, levels) != null) all[p] = true;
            foreach (string p in packed) if (!loose.Contains(p)) all[p] = false;
            bool haveData = File.Exists(Path.Combine(root, DataName));
            bool looseShared = all.Any(kv => kv.Value && LevelOf(kv.Key, levels) == null);
            c.full = !haveData || looseShared || OldArchives(root);
            var byLevel = new Dictionary<string, Job>(StringComparer.OrdinalIgnoreCase);
            var shared = new Job { kind = RxArchiveNative.KindData, archive = Path.Combine(root, DataName) };
            foreach (var kv in all)
            {
                cancel.ThrowIfCancellationRequested();
                string l = LevelOf(kv.Key, levels);
                var j = shared;
                if (l != null && !byLevel.TryGetValue(l, out j)) byLevel[l] = j = new Job { level = l, kind = RxArchiveNative.KindTrack, archive = Path.Combine(root, TracksDir, l + ".rxtrack") };
                long n = kv.Value ? new FileInfo(Full(root, kv.Key)).Length : RxFiles.Length(Full(root, kv.Key));
                if (kv.Value) { c.looseFiles++; c.looseBytes += n; }
                j.files.Add((kv.Key, n, kv.Value));
            }
            foreach (var j in byLevel.Values.OrderBy(j => j.level, StringComparer.Ordinal))
            {
                if (!j.files.Any(f => f.rel.EndsWith(".dx9.level", StringComparison.OrdinalIgnoreCase))) continue;   // not a whole level
                if (!c.full && !j.files.Any(f => f.loose)) continue;                                                 // packed already
                c.levels.Add(j);
            }
            if (c.full && shared.files.Count > 0) c.shared = shared;
            if (!c.full && c.levels.Count == 0) { c.looseFiles = 0; c.looseBytes = 0; }   // (loose files of no level: nothing packing takes)
            return c;
        }

        /// The {...} of alive-levels.json's levels[] entry for a level (the registry is the importer's own JSON).
        static string AliveEntry(string json, string level)
        {
            foreach (string key in new[] { "\"level\": \"" + level + "\"", "\"level\":\"" + level + "\"" })
            {
                int hit = json.IndexOf(key, StringComparison.Ordinal);
                if (hit < 0) continue;
                int start = json.LastIndexOf('{', hit);
                if (start < 0) return null;
                int depth = 0; bool str = false;
                for (int i = start; i < json.Length; i++)
                {
                    char ch = json[i];
                    if (str) { if (ch == '\\') i++; else if (ch == '"') str = false; continue; }
                    if (ch == '"') str = true;
                    else if (ch == '{' || ch == '[') depth++;
                    else if ((ch == '}' || ch == ']') && --depth == 0) return json.Substring(start, i - start + 1);
                }
            }
            return null;
        }

        static string Json(string s)
        {
            var sb = new StringBuilder("\"");
            foreach (char ch in s ?? "") { if (ch == '"' || ch == '\\') sb.Append('\\').Append(ch); else if (ch < 0x20) sb.Append("\\u").Append(((int)ch).ToString("x4")); else sb.Append(ch); }
            return sb.Append('"').ToString();
        }
        static string TrackMeta(Job j, string alive, Dictionary<string, string> sha)
        {
            bool package = j.files.Any(f => f.rel.EndsWith(".dx9.package", StringComparison.OrdinalIgnoreCase)), extracted = j.files.Any(f => f.rel.StartsWith("Assets/", StringComparison.Ordinal));
            var sb = new StringBuilder("{\"format\":\"reflex-track-archive\",\"version\":1,\"level\":").Append(Json(j.level)).Append(",\"name\":").Append(Json(j.level))
                .Append(",\"source\":").Append(Json(alive != null ? "alive" : "base")).Append(",\"layout\":").Append(Json(package ? (extracted ? "packed+extracted" : "packed") : "extracted"))
                .Append(",\"created\":").Append(Json(DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"))).Append(",\"files\":[");
            bool first = true;
            foreach (string ext in new[] { ".dx9.database", ".dx9.level", ".dx9.scene", ".dx9.package" })
            {
                var f = j.files.FirstOrDefault(x => x.rel.Equals("Database/" + j.level + ext, StringComparison.OrdinalIgnoreCase));
                if (f.rel == null) continue;
                if (!first) sb.Append(',');
                first = false;
                sb.Append("{\"name\":").Append(Json(f.rel.Substring(9))).Append(",\"bytes\":").Append(f.bytes).Append(",\"sha256\":\"").Append(sha[f.rel]).Append("\"}");
            }
            sb.Append(']');
            if (alive != null) sb.Append(",\"alive\":").Append(alive);
            return sb.Append('}').ToString();
        }

        // ------------------------------------------------------------------------------------------------ packing

        /// Pack on the calling thread (the import's worker).
        internal static Result PackNow(string root, Action<Progress> progress, CancellationToken cancel, int level = 9, bool removeLoose = true, int threads = 0)
            => Run(Path.GetFullPath(root).TrimEnd('/', '\\'), progress, cancel, level, removeLoose, threads);

        public static Task<Result> Pack(string root, Action<Progress> progress, CancellationToken cancel, int level = 9, bool removeLoose = true, int threads = 0)
        {
            root = Path.GetFullPath(root).TrimEnd('/', '\\');
            return Task.Run(() => Run(root, progress, cancel, level, removeLoose, threads), cancel);
        }

        /// A pack is running (the menus' warm test-ride session waits: OpenArenaWarm.Enabled; the library page shows the progress).
        public static volatile bool Running;

        static Result Run(string root, Action<Progress> progress, CancellationToken cancel, int level, bool removeLoose, int threads)
        {
            Running = true;
            try { return RunPack(root, progress, cancel, level, removeLoose, threads); }
            finally { Running = false; }
        }

        static string Sha(byte[] b) { using (var h = SHA256.Create()) return string.Concat(h.ComputeHash(b).Select(x => x.ToString("x2"))); }

        static Result RunPack(string root, Action<Progress> progress, CancellationToken cancel, int level, bool removeLoose, int threads)
        {
            var t0 = DateTime.UtcNow;
            if (!File.Exists(Path.Combine(root, ReflexAssetImport.ExtractedMarker))) throw new InvalidDataException("PACK GAME FILES packs an imported game library (" + ReflexAssetImport.ExtractedMarker + "): " + root);
            RxFiles.EnsureCore?.Invoke();
            if (!RxArchiveNative.Available) throw new InvalidOperationException("the core library has no archive support (rebuild librxcore)");
            RxFiles.Reset();
            var c = Collect(root, cancel);
            var jobs = new List<Job>();
            if (c.shared != null) jobs.Add(c.shared);
            jobs.AddRange(c.levels);
            var res = new Result();
            if (jobs.Count == 0) return res;
            long total = c.Bytes;
            int workers = threads > 0 ? threads : Math.Max(1, Math.Min(6, Environment.ProcessorCount - 2));
            var p = new Progress { count = jobs.Sum(j => j.files.Count), total = total * 3 };
            object gate = new object();
            void Report(string current) { lock (gate) { p.current = current; p.seconds = (DateTime.UtcNow - t0).TotalSeconds; } progress?.Invoke(p); }
            byte[] Read(string rel) => RxFiles.ReadAllBytes(Full(root, rel));
            void Each(IEnumerable<(string rel, long bytes, bool loose)> files, Action<(string rel, long bytes, bool loose)> act)
            {
                Exception failure = null;
                Parallel.ForEach(files, new ParallelOptions { MaxDegreeOfParallelism = workers, CancellationToken = cancel }, (f, state) =>
                {
                    if (failure != null) { state.Stop(); return; }
                    try { act(f); } catch (Exception e) { failure = e; state.Stop(); }
                });
                if (failure != null) throw failure;
            }

            // 0: every file's SHA-256 (the pool and the track metadata)
            p.phase = 0; Report("reading your game files");
            var sha = new Dictionary<string, string>(StringComparer.Ordinal);
            var source = new Dictionary<string, string>(StringComparer.Ordinal);   // hash -> a path with those bytes
            var owners = new Dictionary<string, HashSet<Job>>(StringComparer.Ordinal);
            int read = 0;
            foreach (var j in jobs)
                Each(j.files, f =>
                {
                    string h = Sha(Read(f.rel));
                    lock (gate)
                    {
                        sha[f.rel] = h; if (!source.ContainsKey(h)) source[h] = f.rel;
                        if (!owners.TryGetValue(h, out var o)) owners[h] = o = new HashSet<Job>();
                        o.Add(j); p.done += f.bytes; p.files = ++read;
                    }
                    if ((read & 1023) == 0) Report("reading your game files");
                });
            // the pool: FULL - every file more than one archive has (one of them a level's), and what the old pool holds (an old level archive
            // that is not packed again yet keeps its references); LEVELS - what the library's pool already has
            var pool = new HashSet<string>(StringComparer.Ordinal);
            foreach (var a in RxFiles.ArchivesOf(root).Where(a => a.kind == RxArchiveNative.KindData))
                foreach (string q in RxFiles.PathsOf(a.file))
                    if (q.StartsWith(PoolPrefix, StringComparison.Ordinal) && (!c.full || source.ContainsKey(q.Substring(PoolPrefix.Length)))) pool.Add(q.Substring(PoolPrefix.Length));
            if (c.full)
                foreach (var kv in owners) if (kv.Value.Count > 1 && kv.Value.Any(j => j.kind == RxArchiveNative.KindTrack)) pool.Add(kv.Key);
            long largest = c.full ? (c.shared?.files.Sum(f => f.bytes) ?? 0) + pool.Where(h => c.shared == null || !owners[h].Contains(c.shared)).Sum(h => RxFiles.Length(Full(root, source[h])))
                                  : jobs.Max(j => j.files.Sum(f => f.bytes));
            long? free = ReflexAssetImport.FreeBytes(root);
            if (free.HasValue && free.Value < largest * 6 / 10 + (256L << 20))
                throw new IOException($"Not enough free space to pack: about {(largest * 6 / 10 >> 20) + 256} MB needed, {free.Value >> 20} MB free on that drive.");
            string registry = null;
            try { string r = Path.Combine(root, "Database", AliveLevels.Registry); if (File.Exists(r)) registry = File.ReadAllText(r); } catch (IOException) { }
            foreach (var j in c.levels) j.meta = TrackMeta(j, registry == null ? null : AliveEntry(registry, j.level), sha);
            if (c.shared != null)
                c.shared.meta = "{\"format\":\"reflex-data-archive\",\"version\":2,\"created\":\"" + DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ") + "\",\"dirs\":[\"Assets\"],\"files\":" + c.shared.files.Count + ",\"pool\":" + pool.Count + "}";
            Directory.CreateDirectory(Path.Combine(root, TracksDir));
            string temp = null;
            try
            {
                foreach (var j in jobs)
                {
                    cancel.ThrowIfCancellationRequested();
                    bool data = j.kind == RxArchiveNative.KindData;
                    string what = j.level ?? "shared assets";
                    // the pool's own entries go into the shared data (FULL); a level references them
                    var poolEntries = new List<(string rel, long bytes, bool loose)>();
                    if (data) foreach (string h in pool) poolEntries.Add((PoolPrefix + h, RxFiles.Length(Full(root, source[h])), false));
                    p.phase = data ? 2 : 1;
                    temp = j.archive + ".new";
                    IntPtr w = RxArchiveNative.Create(temp, j.kind, level);
                    if (w == IntPtr.Zero) throw new IOException("cannot create " + temp + ": " + RxArchiveNative.LastError());
                    try
                    {
                        RxArchiveNative.SetMeta(w, j.meta);
                        Each(j.files, f =>
                        {
                            byte[] b = Read(f.rel);
                            bool asRef = !data && pool.Contains(sha[f.rel]);
                            if (!(asRef ? RxArchiveNative.AddRef(w, f.rel, b, b.Length) : RxArchiveNative.Add(w, f.rel, b, b.Length))) throw new IOException(f.rel + ": " + RxArchiveNative.LastError());
                            lock (gate) { p.raw += b.Length; p.done += b.Length; }
                            Report(what);
                        });
                        Each(poolEntries, f =>
                        {
                            byte[] b = Read(source[f.rel.Substring(PoolPrefix.Length)]);
                            if (!RxArchiveNative.Add(w, f.rel, b, b.Length)) throw new IOException(f.rel + ": " + RxArchiveNative.LastError());
                        });
                        cancel.ThrowIfCancellationRequested();
                        long deduped = RxArchiveNative.Deduped(w, out long pooled);
                        res.dedupedBytes += deduped; res.pooledBytes += pooled;
                        bool ok = RxArchiveNative.Finish(w); w = IntPtr.Zero;
                        if (!ok) throw new IOException("cannot finish " + temp + ": " + RxArchiveNative.LastError());
                    }
                    finally { if (w != IntPtr.Zero) RxArchiveNative.Abort(w); }
                    // check: every file read back and compared with its source; a pool reference with its Pool/ copy (already in place)
                    p.phase = 3; Report("checking " + what);
                    IntPtr a = RxArchiveNative.Open(temp);
                    if (a == IntPtr.Zero) throw new IOException(temp + ": " + RxArchiveNative.LastError());
                    try
                    {
                        if (RxArchiveNative.Count(a) != j.files.Count + poolEntries.Count) throw new IOException(temp + ": " + RxArchiveNative.Count(a) + " files, " + (j.files.Count + poolEntries.Count) + " packed");
                        Each(j.files.Concat(poolEntries), f =>
                        {
                            bool poolEntry = data && f.rel.StartsWith(PoolPrefix, StringComparison.Ordinal);
                            byte[] want = Read(poolEntry ? source[f.rel.Substring(PoolPrefix.Length)] : f.rel);
                            int i = RxArchiveNative.Find(a, f.rel);
                            if (i < 0 || RxArchiveNative.Size(a, i) != want.Length) throw new IOException(f.rel + " is missing from " + Path.GetFileName(temp));
                            byte[] got;
                            if (RxArchiveNative.EntrySha(a, i, out string hex) == 2)
                            {   // a pool reference: the library's Pool/ copy of the same SHA-256
                                if (hex != sha[f.rel]) throw new IOException(f.rel + ": its pool reference names other bytes");
                                got = RxFiles.ReadAllBytes(Full(root, PoolPrefix + hex));
                            }
                            else
                            {
                                got = new byte[want.Length];
                                if (!RxArchiveNative.Read(a, i, 0, got, 0, got.Length)) throw new IOException(f.rel + ": " + RxArchiveNative.LastError());
                            }
                            if (!got.AsSpan().SequenceEqual(want)) throw new IOException(f.rel + " reads back different from " + Path.GetFileName(temp));
                            lock (gate) p.done += want.Length;
                        });
                    }
                    finally { RxArchiveNative.Close(a); }
                    // in place (the overlay sees it from now on), then its loose copies go
                    RxFiles.Release(j.archive);   // (an archive replaced: on Windows an open one can't be deleted and moved over)
                    if (File.Exists(j.archive)) File.Delete(j.archive);
                    File.Move(temp, j.archive); temp = null;
                    res.archives.Add(j.archive);
                    res.packedBytes += new FileInfo(j.archive).Length;
                    RxFiles.Reset();
                    if (removeLoose)
                    {
                        p.phase = 4; Report("tidying " + what);
                        foreach (var f in j.files.Where(f => f.loose))
                        {
                            string full = Full(root, f.rel);
                            try { File.Delete(full); } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { ReflexAssetImport.DefaultLog?.Invoke("[pack] could not remove " + full + ": " + e.Message); }
                        }
                        res.removed = true;
                    }
                }
                res.levels = c.levels.Count; res.files = p.count; res.looseBytes = total;
                if (removeLoose)
                {
                    RemoveEmpty(Path.Combine(root, "Assets"));
                    if (!Directory.Exists(Path.Combine(root, "Assets")))
                    {   // the import's browsable index points at the loose files: gone with them
                        try { File.Delete(Path.Combine(root, "asset-index.html")); } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
                        ReflexAssetImport.DeleteTree(Path.Combine(root, "Browse"));
                    }
                }
                File.AppendAllText(Path.Combine(root, Marker), DateTime.UtcNow.ToString("u") + "  packed " + (c.full ? "everything" : "levels") + ": " + res.levels + " levels" + (c.shared != null ? " and the shared assets" : "") +
                                   ", " + total / (1 << 20) + " MB in " + p.count + " files -> " + res.packedBytes / (1 << 20) + " MB (pool " + pool.Count + " files; stored once: " + (res.dedupedBytes + res.pooledBytes) / (1 << 20) + " MB)\n");
                res.seconds = (DateTime.UtcNow - t0).TotalSeconds;
                RxFiles.Reset();
                return res;
            }
            catch
            {
                if (temp != null) try { File.Delete(temp); } catch (Exception) { }
                RxFiles.Reset();
                throw;
            }
        }

        static void RemoveEmpty(string dir)
        {
            if (!Directory.Exists(dir) || ReflexFs.IsLink(dir)) return;
            foreach (string d in Directory.GetDirectories(dir)) RemoveEmpty(d);
            try { if (!Directory.EnumerateFileSystemEntries(dir).Any()) Directory.Delete(dir); } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
        }
    }
}
