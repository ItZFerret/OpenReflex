// The game library's files as the client reads them (core/plan/RXARCHIVE.md): the loose files first, then the Reflex archives below them -
// the same rule as the core's file system (core/src/fmt_vfs.c), so the client and the simulation always see the same bytes.
//   a root's archives   every *.rxdata directly in a folder and every Tracks/<level>.rxtrack (version 5: an RXAR file) of it: their files
//                       appear below that folder ("<root>/Assets/texture/...", "<root>/Database/NAT_Concord.dx9.level"), under the loose
//                       files of the same path; the track archives before the .rxdata files (name order).
//   archive as folder   "<...>/Foo.rxtrack/Database/X.dx9.level": a file inside an archive named by the archive's own path (Mods/Tracks
//                       tracks packed as .rxtrack: TrackMods keeps such paths in its file lists).
// Paths are full paths, as before (string paths stay the currency: a caller asks RxFiles where it asked File / Directory). A loose file is
// one stat; a path that is not on disk looks for the folder above it that has archives (cached per folder). Reads are thread safe.
// The archives are read by the core (RxArchiveNative); EnsureCore (set by the Unity side, ReflexInstall) loads the library on first use.
// REFLEX_NO_ARCHIVES=1 leaves every archive out, like the core. Nothing here uses Unity (tests build it on .NET).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    public static class RxFiles
    {
        /// Loads the core library when it is not loaded yet (ReflexInstall sets it; tests call RxNativeLoader.Load themselves).
        public static Action EnsureCore;
        public static Action<string> Log = s => { };
        public static bool Disabled
        {
            get { string v = Environment.GetEnvironmentVariable("REFLEX_NO_ARCHIVES"); return !string.IsNullOrEmpty(v) && v != "0"; }
        }

        internal sealed class Archive
        {
            public string file; public IntPtr handle; public uint kind, version; public string meta;
            public string[] paths; public long[] sizes;
            /// Entries that are pool references (container version 2: their bytes are the root's Pool/<sha256>, RXARCHIVE.md "Dedupe").
            public bool[] refs;
            // Release (below) closes the handle once no call is using it: on Windows a folder with an open file inside cannot be renamed,
            // and a deleted file that is still open keeps its name (the import's publish, a re-pack, an Alive re-import).
            int users; bool retired;
            /// f(handle) while the handle stays open; an IOException once the archive was released.
            public T Use<T>(Func<IntPtr, T> f)
            {
                lock (this) { if (handle == IntPtr.Zero) throw new IOException(file + ": the archive was closed (packed again or moved)"); users++; }
                try { return f(handle); }
                finally { lock (this) { if (--users == 0 && retired) CloseNow(); } }
            }
            public void Read(int i, long offset, byte[] buffer, int at, int n, string what)
            {
                if (!Use(h => RxArchiveNative.Read(h, i, offset, buffer, at, n))) throw new IOException(what + ": " + RxArchiveNative.LastError());
            }
            /// Closes the handle now, or when the last call using it ends.
            public void Retire() { lock (this) { retired = true; if (users == 0) CloseNow(); } }
            void CloseNow() { IntPtr h = handle; handle = IntPtr.Zero; RxArchiveNative.Close(h); }
        }
        /// A folder's archives: its files by lower-case relative path ('/'), and the names directly in each relative folder.
        sealed class Overlay
        {
            public string root;
            public readonly List<Archive> archives = new List<Archive>();
            public readonly Dictionary<string, (Archive a, int i)> files = new Dictionary<string, (Archive, int)>(StringComparer.Ordinal);
            public readonly Dictionary<string, List<string>> dirs = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        }

        static readonly object gate = new object();
        static readonly Dictionary<string, Overlay> overlays = new Dictionary<string, Overlay>(StringComparer.Ordinal);   // folder (normalized) -> its archives, null: none
        static readonly Dictionary<string, Archive> opened = new Dictionary<string, Archive>(StringComparer.Ordinal);      // archive file (normalized) -> archive, null: not one
        static readonly List<Archive> live = new List<Archive>();   // every archive opened and not released (Reset forgets them, they stay open)
        static bool coreFailed;
        /// Bumped by Reset (a pack added or removed archives): callers that cache what they found re-ask.
        public static int Generation { get; private set; }

        static string Norm(string path)
        {
            string full = Path.GetFullPath(path).Replace('\\', '/');
            while (full.Length > 1 && full.EndsWith("/", StringComparison.Ordinal) && !(full.Length == 3 && full[1] == ':')) full = full.Substring(0, full.Length - 1);
            return full;
        }
        static string Key(string norm) => norm.ToLowerInvariant();

        /// Forgets every folder's archives (they are found again on the next read). Archive handles stay open (a reader may hold one).
        public static void Reset()
        {
            lock (gate) { overlays.Clear(); opened.Clear(); coreFailed = false; Generation++; }
        }

        /// Closes every archive in or under path (a folder, or one archive file) and forgets them: call it before that folder is renamed
        /// or that archive is deleted or replaced. A read still running finishes first (the handle closes after it); a stream of a released
        /// archive fails from then on. Every other folder's archives are untouched.
        public static void Release(string path)
        {
            string key;
            try { key = Key(Norm(path)); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return; }
            bool Under(string k) => k == key || k.StartsWith(key + "/", StringComparison.Ordinal);
            var close = new List<Archive>();
            lock (gate)
            {
                foreach (var a in live) if (Under(Key(Norm(a.file)))) close.Add(a);
                foreach (var a in close) live.Remove(a);
                foreach (var k in opened.Keys.Where(Under).ToList()) opened.Remove(k);
                foreach (var k in overlays.Keys.Where(k => Under(k) || key.StartsWith(k + "/", StringComparison.Ordinal)).ToList()) overlays.Remove(k);   // (a folder above holds it too)
                Generation++;
            }
            foreach (var a in close) a.Retire();
            if (close.Count > 0) Log("[RxFiles] released " + close.Count + " archive handle(s) under " + path);
        }

        static bool Core()
        {
            if (coreFailed) return false;
            try
            {
                if (string.IsNullOrEmpty(RxNativeLoader.LoadedPath)) EnsureCore?.Invoke();
                if (!RxArchiveNative.Available) { coreFailed = true; Log("[RxFiles] the core library has no archive API: archives are not read"); return false; }
                return true;
            }
            catch (Exception e) { coreFailed = true; Log("[RxFiles] the core library is not available, archives are not read: " + e.Message); return false; }
        }

        internal static bool IsArchive(string file)
        {
            try
            {
                using (var f = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    var b = new byte[4];
                    return f.Read(b, 0, 4) == 4 && b[0] == 'R' && b[1] == 'X' && b[2] == 'A' && b[3] == 'R';
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return false; }
        }

        /// The archive in a file (opened once), or null (not an archive, unreadable, no core).
        internal static Archive OpenArchive(string file)
        {
            string key = Key(Norm(file));
            lock (gate)
            {
                if (opened.TryGetValue(key, out var have)) return have;
                Archive a = null;
                if (File.Exists(file) && IsArchive(file) && Core())
                {
                    IntPtr h = RxArchiveNative.Open(file);
                    if (h == IntPtr.Zero) Log("[RxFiles] " + file + ": " + RxArchiveNative.LastError());
                    else
                    {
                        RxArchiveNative.List(h, out var paths, out var sizes, out var refs);
                        a = new Archive { file = file, handle = h, kind = RxArchiveNative.Kind(h), version = (uint)RxArchiveNative.Version(h), meta = RxArchiveNative.Meta(h), paths = paths, sizes = sizes, refs = refs };
                        live.Add(a);
                    }
                }
                opened[key] = a;
                return a;
            }
        }

        /// The archives of a folder (cached; null when it has none).
        static Overlay OverlayOf(string folderNorm)
        {
            string key = Key(folderNorm);
            lock (gate)
            {
                if (overlays.TryGetValue(key, out var have)) return have;
                Overlay o = null;
                // a game root only (like the core: a folder with Database/, or an imported library): Mods/ with its Tracks/ is not one
                if (!Disabled && Directory.Exists(folderNorm) && (Directory.Exists(Path.Combine(folderNorm, "Database")) || File.Exists(Path.Combine(folderNorm, ReflexAssetImport.ExtractedMarker))))
                {
                    var candidates = new List<string>();
                    string tracks = Path.Combine(folderNorm, "Tracks");
                    try
                    {
                        if (Directory.Exists(tracks)) candidates.AddRange(Directory.GetFiles(tracks).Where(f => f.EndsWith(".rxtrack", StringComparison.OrdinalIgnoreCase)).OrderBy(f => f, StringComparer.Ordinal));
                        candidates.AddRange(Directory.GetFiles(folderNorm).Where(f => f.EndsWith(".rxdata", StringComparison.OrdinalIgnoreCase)).OrderBy(f => f, StringComparer.Ordinal));
                    }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
                    foreach (string c in candidates)
                    {
                        var a = OpenArchive(c);
                        if (a == null) continue;
                        o ??= new Overlay { root = folderNorm };
                        o.archives.Add(a);
                    }
                    // the pool (Pool/<sha256> entries of the root's archives), for the pool references
                    var pool = new Dictionary<string, (Archive a, int i)>(StringComparer.Ordinal);
                    if (o != null)
                        foreach (var a in o.archives)
                            for (int i = 0; i < a.paths.Length; i++)
                                if (!a.refs[i] && a.paths[i].StartsWith("Pool/", StringComparison.Ordinal) && !pool.ContainsKey(a.paths[i])) pool[a.paths[i]] = (a, i);
                    int unresolved = 0;
                    foreach (var a in o?.archives ?? new List<Archive>())
                    {
                        for (int i = 0; i < a.paths.Length; i++)
                        {
                            string rel = a.paths[i], k = rel.ToLowerInvariant();
                            if (o.files.ContainsKey(k)) continue;                    // an earlier archive has it
                            var at = (a, i);
                            if (a.refs[i])
                            {
                                string hex = null;
                                try { a.Use(h => RxArchiveNative.EntrySha(h, i, out hex)); } catch (IOException) { }
                                if (hex == null || !pool.TryGetValue("Pool/" + hex, out at) || at.a.sizes[at.i] != a.sizes[i]) { unresolved++; continue; }
                            }
                            o.files[k] = at;
                            int slash = rel.LastIndexOf('/');
                            string dir = slash < 0 ? "" : k.Substring(0, slash);
                            if (!o.dirs.TryGetValue(dir, out var names)) o.dirs[dir] = names = new List<string>();
                            names.Add(slash < 0 ? rel : rel.Substring(slash + 1));
                        }
                    }
                    if (o != null) Log("[RxFiles] " + folderNorm + ": " + o.archives.Count + " archives, " + o.files.Count + " files" + (unresolved > 0 ? ", " + unresolved + " pool references without their Pool/ file" : ""));
                }
                overlays[key] = o;
                return o;
            }
        }

        /// Where a path that is not on disk is packed: (archive, index), or (null, -1).
        internal static (Archive a, int i) Resolve(string path)
        {
            if (string.IsNullOrEmpty(path)) return (null, -1);
            string norm;
            try { norm = Norm(path); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return (null, -1); }
            // an archive named as a folder: ".../X.rxtrack/<path inside>"
            int at = 0;
            while (true)
            {
                int k = IndexOfArchiveSegment(norm, at);
                if (k < 0) break;
                string file = norm.Substring(0, k);
                if (File.Exists(file))
                {
                    var a = OpenArchive(file);
                    if (a == null) return (null, -1);
                    int i;
                    try { i = a.Use(h => RxArchiveNative.Find(h, norm.Substring(k + 1))); } catch (IOException) { return (null, -1); }
                    return i < 0 || a.refs[i] ? (null, -1) : (a, i);   // (a pool reference has no bytes in a lone archive)
                }
                at = k + 1;
            }
            if (Disabled) return (null, -1);
            // a folder above it with archives (at most 8 levels up: "<root>/Assets/<type>/<db>/<package>/<asset>")
            int slash = norm.LastIndexOf('/');
            for (int up = 0; up < 8 && slash > 0; up++)
            {
                string folder = norm.Substring(0, slash);
                var o = OverlayOf(folder);
                if (o != null && o.files.TryGetValue(norm.Substring(slash + 1).ToLowerInvariant(), out var hit)) return hit;
                slash = folder.LastIndexOf('/');
            }
            return (null, -1);
        }
        static int IndexOfArchiveSegment(string norm, int from)
        {
            for (int i = from; i < norm.Length; i++)
            {
                if (norm[i] != '/') continue;
                if (i >= 8 && string.Compare(norm, i - 8, ".rxtrack", 0, 8, StringComparison.OrdinalIgnoreCase) == 0) return i;
                if (i >= 7 && string.Compare(norm, i - 7, ".rxdata", 0, 7, StringComparison.OrdinalIgnoreCase) == 0) return i;
            }
            return -1;
        }

        // ------------------------------------------------------------------------------------------------ the File / Directory calls

        public static bool Exists(string path) => File.Exists(path) || Resolve(path).a != null;
        /// The file's length, -1 when there is no such file.
        public static long Length(string path)
        {
            if (File.Exists(path)) try { return new FileInfo(path).Length; } catch (FileNotFoundException) { }   // (removed by a pack just now: packed)
            var (a, i) = Resolve(path);
            return a == null ? -1 : a.sizes[i];
        }
        /// True when the file is read from an archive (not a loose file).
        public static bool Packed(string path) => !File.Exists(path) && Resolve(path).a != null;

        public static byte[] ReadAllBytes(string path)
        {
            if (File.Exists(path)) try { return File.ReadAllBytes(path); } catch (Exception e) when (Gone(e)) { }   // (removed by a pack just now)
            var (a, i) = Resolve(path);
            if (a == null) throw new FileNotFoundException("Could not find file '" + path + "'.", path);
            long n = a.sizes[i];
            if (n > int.MaxValue) throw new IOException(path + ": file too large");
            var b = new byte[n];
            a.Read(i, 0, b, 0, (int)n, path);
            return b;
        }
        /// count bytes at offset into buffer[at..]; the file must hold them.
        public static void ReadAt(string path, long offset, byte[] buffer, int at, int count)
        {
            if (File.Exists(path))
                try
                {
                    using (var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete))
                    {
                        f.Position = offset; int got = 0;
                        while (got < count) { int r = f.Read(buffer, at + got, count - got); if (r <= 0) throw new EndOfStreamException(path); got += r; }
                    }
                    return;
                }
                catch (Exception e) when (Gone(e)) { }
            var (a, i) = Resolve(path);
            if (a == null) throw new FileNotFoundException("Could not find file '" + path + "'.", path);
            a.Read(i, offset, buffer, at, count, path);
        }
        /// A read-only, seekable stream of the file (a FileStream for a loose one).
        public static Stream OpenRead(string path)
        {
            if (File.Exists(path)) try { return new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete, 1 << 16); } catch (Exception e) when (Gone(e)) { }
            var (a, i) = Resolve(path);
            if (a == null) throw new FileNotFoundException("Could not find file '" + path + "'.", path);
            return new PackedStream(a, i, path);
        }
        /// A stamp that changes when the file may have: a loose file's write time, a packed file's archive's.
        public static long Stamp(string path)
        {
            if (File.Exists(path)) return File.GetLastWriteTimeUtc(path).Ticks;
            var (a, _) = Resolve(path);
            return a == null ? 0 : File.GetLastWriteTimeUtc(a.file).Ticks;
        }
        sealed class PackedStream : Stream
        {
            readonly Archive a; readonly int index; readonly string path; long position;
            public PackedStream(Archive a, int index, string path) { this.a = a; this.index = index; this.path = path; }
            public override bool CanRead => true;
            public override bool CanSeek => true;
            public override bool CanWrite => false;
            public override long Length => a.sizes[index];
            public override long Position { get => position; set { if (value < 0) throw new ArgumentOutOfRangeException(nameof(value)); position = value; } }
            // reads go through a 1 MiB window (a compressed file decodes whole 1 MiB chunks: small reads must not decode one each)
            byte[] window; long windowAt = -1; int windowLength;
            public override int Read(byte[] buffer, int offset, int count)
            {
                long left = Length - position;
                if (left <= 0 || count <= 0) return 0;
                int n = (int)Math.Min(count, left);
                if (n >= 1 << 20)
                {
                    a.Read(index, position, buffer, offset, n, path);
                }
                else for (int done = 0; done < n;)
                {   // windows on 1 MiB boundaries: the archive's chunks
                    long p = position + done;
                    if (windowAt < 0 || p < windowAt || p >= windowAt + windowLength)
                    {
                        window ??= new byte[1 << 20];
                        windowAt = p & ~((1L << 20) - 1); windowLength = (int)Math.Min(window.Length, Length - windowAt);
                        try { a.Read(index, windowAt, window, 0, windowLength, path); } catch { windowAt = -1; throw; }
                    }
                    int take = (int)Math.Min(n - done, windowAt + windowLength - p);
                    Buffer.BlockCopy(window, (int)(p - windowAt), buffer, offset + done, take);
                    done += take;
                }
                position += n;
                return n;
            }
            public override long Seek(long offset, SeekOrigin origin)
            {
                long p = origin == SeekOrigin.Begin ? offset : origin == SeekOrigin.Current ? position + offset : Length + offset;
                if (p < 0) throw new IOException("seek before the start");
                return position = p;
            }
            public override void Flush() { }
            public override void SetLength(long value) => throw new NotSupportedException();
            public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        }

        /// A loose file removed between the look and the read (PACK GAME FILES removes a packed level's loose copies while the game runs).
        static bool Gone(Exception e) => e is FileNotFoundException || e is DirectoryNotFoundException;

        public static string ReadAllText(string path) => System.Text.Encoding.UTF8.GetString(StripBom(ReadAllBytes(path)));
        static byte[] StripBom(byte[] b) => b.Length >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF ? b.Skip(3).ToArray() : b;

        /// The files directly in dir (full paths): the loose ones, then the packed ones no loose file covers. pattern: "*", "*.ext",
        /// "prefix*" or "prefix*.ext" (case-insensitive, like Windows).
        public static string[] GetFiles(string dir, string pattern = "*")
        {
            var result = new List<string>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (Directory.Exists(dir))
                foreach (string f in Directory.GetFiles(dir)) if (Match(Path.GetFileName(f), pattern)) { result.Add(f); seen.Add(Path.GetFileName(f)); }
            if (Disabled) return result.ToArray();
            string norm;
            try { norm = Norm(dir); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return result.ToArray(); }
            int slash = norm.Length;
            string folder = norm;
            for (int up = 0; up < 8; up++)
            {
                var o = Directory.Exists(folder) ? OverlayOf(folder) : null;
                if (o != null)
                {
                    string rel = folder.Length >= norm.Length ? "" : norm.Substring(folder.Length + 1).ToLowerInvariant();
                    if (o.dirs.TryGetValue(rel, out var names))
                        foreach (string n in names) if (Match(n, pattern) && seen.Add(n)) result.Add(Path.Combine(dir, n));
                }
                int s = folder.LastIndexOf('/');
                if (s <= 0) break;
                folder = folder.Substring(0, s);
            }
            return result.ToArray();
        }
        public static bool DirectoryExists(string dir)
        {
            if (Directory.Exists(dir)) return true;
            if (Disabled) return false;
            string norm = Norm(dir), folder = norm;
            for (int up = 0; up < 8; up++)
            {
                int s = folder.LastIndexOf('/');
                if (s <= 0) return false;
                folder = folder.Substring(0, s);
                var o = Directory.Exists(folder) ? OverlayOf(folder) : null;
                if (o != null && o.dirs.ContainsKey(norm.Substring(folder.Length + 1).ToLowerInvariant())) return true;
            }
            return false;
        }

        internal static bool Match(string name, string pattern)
        {
            if (string.IsNullOrEmpty(pattern) || pattern == "*" || pattern == "*.*") return true;
            int star = pattern.IndexOf('*');
            if (star < 0) return string.Equals(name, pattern, StringComparison.OrdinalIgnoreCase);
            string head = pattern.Substring(0, star), tail = pattern.Substring(star + 1);
            return name.Length >= head.Length + tail.Length && name.StartsWith(head, StringComparison.OrdinalIgnoreCase) && name.EndsWith(tail, StringComparison.OrdinalIgnoreCase);
        }

        /// Opens a root's archives ahead (a worker thread at startup: the first read on the main thread does not wait for the ~0.2 s it takes).
        public static void Warm(string root)
        {
            if (string.IsNullOrEmpty(root) || Disabled) return;
            try { OverlayOf(Norm(root)); } catch (Exception e) { Log("[RxFiles] warm " + root + ": " + e.Message); }
        }

        // ------------------------------------------------------------------------------------------------ track archives

        /// The track archives (Tracks/<level>.rxtrack) of a root folder, with their metadata: (archive file, metadata JSON).
        public static List<(string file, string meta)> TrackArchives(string root)
        {
            var list = new List<(string, string)>();
            if (string.IsNullOrEmpty(root) || Disabled) return list;
            Overlay o;
            try { o = OverlayOf(Norm(root)); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return list; }
            if (o == null) return list;
            foreach (var a in o.archives) if (a.kind == RxArchiveNative.KindTrack) list.Add((a.file, a.meta));
            return list;
        }
        /// The archives of a root folder: (file, kind, container version) - GAME LIBRARY and PACK GAME FILES (version 1 archives are packed again, deduped).
        public static List<(string file, uint kind, uint version)> ArchivesOf(string root)
        {
            var list = new List<(string, uint, uint)>();
            if (string.IsNullOrEmpty(root) || Disabled) return list;
            Overlay o;
            try { o = OverlayOf(Norm(root)); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return list; }
            if (o != null) foreach (var a in o.archives) list.Add((a.file, a.kind, a.version));
            return list;
        }
        /// Every path a root's archives give it (the effective packed files, the pool's own Pool/ entries left out).
        public static List<string> PackedPaths(string root)
        {
            var list = new List<string>();
            if (string.IsNullOrEmpty(root) || Disabled) return list;
            Overlay o;
            try { o = OverlayOf(Norm(root)); } catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { return list; }
            if (o == null) return list;
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var a in o.archives)
                for (int i = 0; i < a.paths.Length; i++)
                    if (!a.paths[i].StartsWith("Pool/", StringComparison.Ordinal) && o.files.ContainsKey(a.paths[i].ToLowerInvariant()) && seen.Add(a.paths[i])) list.Add(a.paths[i]);
            return list;
        }

        /// The metadata JSON of an archive file ("" when it is not one).
        public static string MetaOf(string archiveFile) => OpenArchive(archiveFile)?.meta ?? "";
        /// The paths inside an archive file (empty when it is not one).
        public static string[] PathsOf(string archiveFile) => OpenArchive(archiveFile)?.paths ?? Array.Empty<string>();
    }
}
