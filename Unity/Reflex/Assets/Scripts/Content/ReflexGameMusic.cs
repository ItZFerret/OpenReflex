// A game soundtrack imported from the player's own disc into the music pool (ReflexCustomMusic: <client root>/Mods/Music): the MX vs ATV Untamed
// songs (UntamedImport/UntamedImporter.Music.cs) and the MX vs ATV Alive songs (AliveImport/AliveMusic.cs) go into Mods/Music/<the game's folder>
// (ReflexSongLibrary.GameSoundtracks), which the music scan counts as that game's soundtrack (OPTIONS > AUDIO > SOUNDTRACK).
//   staging    <Mods/Music>/.game-music-<8 hex>/ (a dot name: the music scan never reads it): 'o' the songs being made, 'lock' held open while
//              the import runs. Removed however the import ends; one a crashed import left is removed by the next import
//   new        the songs made into the staging folder, checked, the manifest written, then the folder moved in whole: the songs are there
//              complete or not at all
//   again      into the folder this importer made before (its manifest): a song that is missing is made and put in; one that is as the
//              importer wrote it (the manifest's SHA-256, or the expected one) stays (or is replaced when the expected file changed); one the
//              player changed (retagged, replaced) is the player's and stays as it is. Any other file of the player's stays. A file the
//              importer wrote under a name it no longer uses (the Alive songs as .mp3, now .ogg) goes, when it is unchanged. Order: those
//              old files out, the manifest replaced in one step (never missing; it lists the new songs before they are moved in, so a
//              crash or a held file in between leaves songs the next import makes again, never songs taken for the player's), the
//              songs in, the manifest once more with the replaced songs' new hashes
//   never      a folder of that name without the manifest (the player's), a file or a link of that name: nothing is written. Nothing is
//              written through a link, and only the importer's own folders are removed (a link in one as a link)
// The manifest names the game and the disc's file name, never a path (the folder is shared as it is). Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;

namespace Reflex.Port.Content
{
    /// One song of a game soundtrack for the installer.
    public sealed class ReflexGameSong
    {
        /// The file's name in the soundtrack's folder ("Artist - Title.mp3"), the song's "Artist - Title".
        public string fileName, artist, title;
        /// The file's size and SHA-256 when they are known before it is made (null: whatever it is, as the encoder writes it).
        public long bytes = -1; public string sha256;
        /// Makes the file at the given path (throws what goes wrong; OperationCanceledException when cancelled).
        public Action<string, CancellationToken> write;
        /// More fields for the manifest's entry (the asset on the disc ...).
        public JObj extra;
        public string Label => artist + " - " + title;
    }

    public enum ReflexGameMusicState { Pending, Imported, Failed, Cancelled }

    public sealed class ReflexGameMusicResult
    {
        public ReflexGameMusicState state = ReflexGameMusicState.Pending;
        /// Why it failed (Failed).
        public string error;
        /// Relative to Mods/Music ("MX vs ATV Untamed"); the folder's full path.
        public string folder, path;
        /// The songs written now; the ones that were there as they should be; the ones the player changed (left as they are); the
        /// files the importer wrote under names it no longer uses, removed.
        public int written, kept, changed, removed;
        public long bytes;
        public double seconds;
        /// It went into the folder made before.
        public bool updated;
        public bool Ok => state == ReflexGameMusicState.Imported;
        public override string ToString() => "soundtrack: " + state + (error != null ? " (" + error + ")" : "") + (folder != null ? " -> " + folder + " (" + written + " written, " + kept + " kept" + (changed > 0 ? ", " + changed + " changed by the player, left" : "") + ")" : "");
    }

    /// A game soundtrack in the music folder as it is now.
    public sealed class ReflexGameMusicStatus
    {
        /// Relative to Mods/Music when the importer made it ("MX vs ATV Untamed"; null: none).
        public string imported;
        /// Why the import can't write there (null: it can).
        public string inTheWay;
        /// The songs the manifest lists, and how many of them are there as the importer wrote them.
        public int listed, present;
    }

    public static class ReflexGameMusic
    {
        public const string StagingPrefix = ".game-music-";
        const string LockName = "lock", OutName = "o";

        // ------------------------------------------------------------------------------------------------ names

        /// "Artist - Title" + extension, without the characters Windows, macOS or Linux can't have in a file name ("12:34" -> "12-34").
        public static string SongFileName(string artist, string title, string extension)
        {
            string name = artist + " - " + title.Replace(':', '-');
            var sb = new StringBuilder(name.Length);
            foreach (char c in name) if (c >= ' ' && "\\/:*?\"<>|".IndexOf(c) < 0) sb.Append(c);
            return sb.ToString().Trim().TrimEnd('.') + extension;
        }

        static string Child(string dir, string name)
        {
            if (!Directory.Exists(dir)) return null;
            return Directory.GetDirectories(dir).FirstOrDefault(d => string.Equals(Path.GetFileName(d), name, StringComparison.OrdinalIgnoreCase));
        }

        /// The soundtrack's folder as it is named on the disk (any case), else as the import makes it.
        public static string Destination(string musicFolder, ReflexGameSoundtrack g) => Child(musicFolder, g.folder) ?? Path.Combine(musicFolder, g.folder);

        static bool IsLink(string path)
        {
            try { return (File.Exists(path) || Directory.Exists(path)) && (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0; }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return false; }
        }

        /// Why the import can't write the soundtrack's folder (null: it can). Throws what the file system throws.
        public static string DestinationProblem(string musicFolder, ReflexGameSoundtrack g)
        {
            string dest = Destination(musicFolder, g), shown = "Mods/Music/" + Path.GetFileName(dest);
            if (File.Exists(musicFolder)) return "Mods/Music is a file, not a folder; move or delete it to import the soundtrack";
            if (IsLink(musicFolder)) return "Mods/Music is a symbolic link to another folder; the importer writes nothing through a link";
            if (IsLink(dest)) return shown + " is a symbolic link to another folder; the importer writes nothing through a link";
            if (File.Exists(dest)) return shown + " is a file; move or delete it to import the soundtrack";
            if (Directory.Exists(dest) && !File.Exists(Path.Combine(dest, g.manifest)))
                return shown + " is already there and was not made by this importer; move or delete that folder to import the soundtrack";
            if (Directory.Exists(dest) && ReadManifest(dest, g) == null)
                return shown + "/" + g.manifest + " can't be read now (is another program using it?); try again";
            return null;
        }

        /// The soundtrack folder's manifest (null: none, or it does not read).
        public static JObj ReadManifest(string folder, ReflexGameSoundtrack g)
        {
            try
            {
                string p = Path.Combine(folder, g.manifest);
                if (!File.Exists(p)) return null;
                var m = UxJson.ParseUtf8(File.ReadAllBytes(p)) as JObj;
                return m != null && m.Get("importer") is string && m.Get("files") is List<object> ? m : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is UxException || e is FormatException || e is ArgumentException) { return null; }
        }

        /// The manifest's songs: file name -> the SHA-256 the importer wrote.
        static Dictionary<string, string> Listed(JObj m)
        {
            var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (m?.Get("files") is List<object> files)
                foreach (object o in files)
                    if (o is JObj f && f.Get("name") is string n && f.Get("sha256") is string sha && n.IndexOfAny(new[] { '/', '\\', ':' }) < 0 && n != "." && n != "..") d[n] = sha;
            return d;
        }

        /// What is in Mods/Music now. Hashes the songs (about 50 - 100 MB): not on the main thread in a frame that must be quick.
        public static ReflexGameMusicStatus Status(string musicFolder, ReflexGameSoundtrack g)
        {
            var st = new ReflexGameMusicStatus();
            if (string.IsNullOrEmpty(musicFolder)) return st;
            try
            {
                st.inTheWay = DestinationProblem(musicFolder, g);
                string dest = Destination(musicFolder, g);
                var m = st.inTheWay == null && Directory.Exists(dest) ? ReadManifest(dest, g) : null;
                if (m == null) return st;
                st.imported = Path.GetFileName(dest);
                foreach (var kv in Listed(m))
                {
                    st.listed++;
                    string f = Path.Combine(dest, kv.Key);
                    if (File.Exists(f) && !IsLink(f) && Sha256File(f) == kv.Value) st.present++;
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException) { }
            return st;
        }

        public static string Sha256File(string path) { using (var h = SHA256.Create()) using (var f = File.OpenRead(path)) return UxBin.Hex(h.ComputeHash(f)); }

        // ------------------------------------------------------------------------------------------------ the import

        /// Makes the songs and installs them into musicFolder/<g.folder> (see the file header). Never throws: the result says what happened.
        ///   source    the manifest's "source" (the game, the disc's file name, never a path)
        ///   needBytes about what the songs take on the disk
        ///   progress  (step, 0..1)
        public static ReflexGameMusicResult Install(string musicFolder, ReflexGameSoundtrack g, string importerVersion, JObj source, IList<ReflexGameSong> songs, long needBytes,
                                                    Func<string, long?> freeBytes, Action<string> log, Action<string, float> progress, CancellationToken cancel)
        {
            log = log ?? (s => { });
            var res = new ReflexGameMusicResult();
            var clock = System.Diagnostics.Stopwatch.StartNew();
            void Report(string what, float f) => progress?.Invoke(what, Math.Min(1f, Math.Max(0f, f)));
            string stage = null; FileStream lockFile = null;
            try
            {
                if (string.IsNullOrEmpty(musicFolder)) throw new IOException("The music folder (Mods/Music) was not found.");
                musicFolder = Path.GetFullPath(musicFolder);
                cancel.ThrowIfCancellationRequested();
                string problem = DestinationProblem(musicFolder, g);
                if (problem != null) throw new UxException(problem);
                string dest = Destination(musicFolder, g);
                res.folder = Path.GetFileName(dest); res.path = dest;
                Directory.CreateDirectory(musicFolder);
                CleanStaging(musicFolder, log);
                long? free = (freeBytes ?? DefaultFreeBytes)(musicFolder);
                if (free != null && free.Value < needBytes + (16L << 20))
                    throw new UxException("Not enough free disk space for the soundtrack in " + musicFolder + ": about " + Mb(needBytes) + " are needed, " + Mb(free.Value) + " are free.");

                // ---- what is there: the folder made before, each song's file
                bool update = Directory.Exists(dest);
                var recorded = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (update)
                {   // (it read a moment ago: one that does not read now is held by another program; nothing is taken for the player's on that)
                    var old = ReadManifest(dest, g);
                    if (old == null) throw new UxException("Mods/Music/" + res.folder + "/" + g.manifest + " can't be read now (is another program using it?); try again");
                    recorded = Listed(old);
                }
                var replaced = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);   // (an older file of the importer's, made again: its hash now)
                var make = new List<ReflexGameSong>();
                var entries = new Dictionary<string, (long bytes, string sha)>(StringComparer.OrdinalIgnoreCase);
                for (int i = 0; i < songs.Count; i++)
                {
                    cancel.ThrowIfCancellationRequested();
                    var s = songs[i];
                    string at = Path.Combine(dest, s.fileName);
                    if (!update || !File.Exists(at) && !Directory.Exists(at)) { make.Add(s); continue; }
                    if (IsLink(at) || Directory.Exists(at)) { res.changed++; continue; }   // (the player's: a link or a folder of that name)
                    Report("Checking " + s.Label, 0.05f * i / songs.Count);
                    string sha = Sha256File(at);
                    recorded.TryGetValue(s.fileName, out string was);
                    bool ours = sha == was || s.sha256 != null && sha == s.sha256;
                    if (!ours) { res.changed++; if (was != null) entries[s.fileName] = (-1, was); continue; }   // (the player changed it: stays theirs)
                    if (s.sha256 != null && sha != s.sha256) { make.Add(s); replaced[s.fileName] = sha; continue; }   // (as the importer wrote it, but an older file: made again)
                    entries[s.fileName] = (new FileInfo(at).Length, sha); res.kept++;
                }

                // the files the importer wrote under names it no longer uses (another encoder's extension), unchanged since: they go
                var names = new HashSet<string>(songs.Select(x => x.fileName), StringComparer.OrdinalIgnoreCase);
                var obsolete = new List<string>();
                foreach (var kv in recorded)
                {
                    if (names.Contains(kv.Key) || string.Equals(kv.Key, g.manifest, StringComparison.OrdinalIgnoreCase)) continue;
                    string at = Path.Combine(dest, kv.Key);
                    if (File.Exists(at) && !IsLink(at) && Sha256File(at) == kv.Value) obsolete.Add(at);
                }

                // ---- the songs to make, in the staging folder, checked
                stage = Path.Combine(musicFolder, StagingPrefix + Guid.NewGuid().ToString("N").Substring(0, 8));
                Directory.CreateDirectory(stage);
                lockFile = new FileStream(Path.Combine(stage, LockName), FileMode.Create, FileAccess.ReadWrite, FileShare.None);
                string staged = Path.Combine(stage, OutName);
                Directory.CreateDirectory(staged);
                for (int i = 0; i < make.Count; i++)
                {
                    cancel.ThrowIfCancellationRequested();
                    var s = make[i];
                    Report(s.Label, 0.05f + 0.85f * i / Math.Max(1, make.Count));
                    string to = Path.Combine(staged, s.fileName);
                    s.write(to, cancel);
                    if (!File.Exists(to)) throw new UxException(s.Label + " was not written");
                    long len = new FileInfo(to).Length; string sha = Sha256File(to);
                    if (s.sha256 != null && (len != s.bytes || sha != s.sha256)) throw new UxException(s.Label + " differs from the supported disc's song");
                    if (len <= 0) throw new UxException(s.Label + " was written empty");
                    entries[s.fileName] = (len, sha);
                }
                cancel.ThrowIfCancellationRequested();
                // the manifest: every song in the table's order (one the player changed keeps what the importer wrote; with
                // stillOld, a song being replaced keeps the hash of the file that is there until it is)
                string stagedManifest = Path.Combine(staged, g.manifest);
                void WriteManifest(string path, bool stillOld)
                {
                    var list = new List<object>();
                    foreach (var s in songs)
                    {
                        if (!entries.TryGetValue(s.fileName, out var e)) continue;
                        var o = new JObj().Set("name", s.fileName).Set("artist", s.artist).Set("title", s.title);
                        if (s.extra != null) foreach (var kv in s.extra.items) o.Set(kv.Key, kv.Value);
                        bool old = stillOld && replaced.ContainsKey(s.fileName);
                        if (e.bytes >= 0 && !old) o.Set("bytes", e.bytes);
                        list.Add(o.Set("sha256", old ? replaced[s.fileName] : e.sha));
                    }
                    var manifest = new JObj().Set("version", 1L).Set("importer", importerVersion).Set("album", g.album).Set("source", source ?? new JObj()).Set("files", list);
                    File.WriteAllText(path, UxJson.Dumps(manifest, 1, false, false) + "\n", new UTF8Encoding(false));
                }
                WriteManifest(stagedManifest, update);

                // ---- publish
                Report("Installing", 0.92f);
                if (!update)
                {
                    Retry(() => Directory.Move(staged, dest));
                    res.written = make.Count;
                }
                else
                {
                    res.updated = true;
                    string target = Path.Combine(dest, g.manifest);
                    void PutManifest()
                    {   // in one step: replaced, never deleted first
                        if (File.Exists(target))
                        {
                            var a = File.GetAttributes(target);
                            if ((a & FileAttributes.ReadOnly) != 0) File.SetAttributes(target, a & ~FileAttributes.ReadOnly);
                            Retry(() => File.Replace(stagedManifest, target, null));
                        }
                        else Retry(() => File.Move(stagedManifest, target));
                    }
                    foreach (string at in obsolete)
                    {
                        var a = File.GetAttributes(at);
                        if ((a & FileAttributes.ReadOnly) != 0) File.SetAttributes(at, a & ~FileAttributes.ReadOnly);
                        Retry(() => File.Delete(at));
                        res.removed++;
                    }
                    PutManifest();
                    foreach (var s in make)
                    {
                        string from = Path.Combine(staged, s.fileName), to = Path.Combine(dest, s.fileName);
                        if (File.Exists(to))
                        {   // (an older file the importer wrote, unchanged since: checked above)
                            var a = File.GetAttributes(to);
                            if ((a & FileAttributes.ReadOnly) != 0) File.SetAttributes(to, a & ~FileAttributes.ReadOnly);
                            Retry(() => File.Replace(from, to, null));
                        }
                        else Retry(() => File.Move(from, to));
                        res.written++;
                    }
                    if (replaced.Count > 0) { WriteManifest(stagedManifest, false); PutManifest(); }   // (the replaced songs' new hashes)
                }
                res.bytes = entries.Values.Where(e => e.bytes > 0).Sum(e => e.bytes);
                res.state = ReflexGameMusicState.Imported;
                Report("Done", 1f);
            }
            catch (OperationCanceledException) when (cancel.IsCancellationRequested) { res.state = ReflexGameMusicState.Cancelled; }
            catch (Exception e) when (!(e is OperationCanceledException))
            {
                res.state = ReflexGameMusicState.Failed; res.error = Message(e);
                log("[music] the " + g.album + " soundtrack failed: " + e);
            }
            finally
            {
                lockFile?.Dispose();
                if (stage != null && !DeleteFolder(stage)) log("[music] the staging folder " + stage + " could not be removed; the next import removes it");
            }
            if (res.state == ReflexGameMusicState.Cancelled && !res.updated) { res.written = 0; res.kept = 0; }
            res.seconds = clock.Elapsed.TotalSeconds;
            return res;
        }

        static string Message(Exception e)
        {
            if (e is AggregateException a) e = a.Flatten().InnerExceptions.FirstOrDefault() ?? e;
            string m = (e.Message ?? "").Trim();
            if (m.Length == 0) m = e.GetType().Name;
            return e is UxException || e is AliveImportException || e is IOException || e is UnauthorizedAccessException ? m : e.GetType().Name + ": " + m;
        }

        static string Mb(long bytes) => ((bytes + (1 << 20) - 1) >> 20).ToString(CultureInfo.InvariantCulture) + " MB";

        static long? DefaultFreeBytes(string folder)
        {
            try
            {
                string root = Path.GetPathRoot(Path.GetFullPath(folder));
                if (string.IsNullOrEmpty(root)) return null;
                var d = new DriveInfo(root);
                return d.IsReady ? d.AvailableFreeSpace : (long?)null;
            }
            catch (Exception) { return null; }
        }

        /// A move or replace; a file another program holds for a moment (a virus scanner on a file just written) is waited for.
        static void Retry(Action a)
        {
            for (int attempt = 0; ; attempt++)
            {
                try { a(); return; }
                catch (Exception e) when ((e is IOException || e is UnauthorizedAccessException) && !(e is FileNotFoundException) && !(e is DirectoryNotFoundException) && attempt < 5)
                {
                    Thread.Sleep(120 * (attempt + 1));
                }
            }
        }

        // ------------------------------------------------------------------------------------------------ the staging folders

        static readonly Regex StagingRx = new Regex("^" + Regex.Escape(StagingPrefix) + "[0-9a-f]{8}$", RegexOptions.CultureInvariant);

        /// The staging folders of imports that did not end: removed (one whose lock is held belongs to an import that is running).
        public static int CleanStaging(string musicFolder, Action<string> log = null)
        {
            log = log ?? (s => { });
            int removed = 0;
            if (string.IsNullOrEmpty(musicFolder) || !Directory.Exists(musicFolder)) return 0;
            foreach (string dir in Directory.GetDirectories(musicFolder))
            {
                if (!StagingRx.IsMatch(Path.GetFileName(dir)) || IsLink(dir)) continue;
                try { using (new FileStream(Path.Combine(dir, LockName), FileMode.Open, FileAccess.ReadWrite, FileShare.None)) { } }
                catch (FileNotFoundException) { }
                catch (DirectoryNotFoundException) { continue; }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { log("[music] " + dir + " belongs to an import that is running: left"); continue; }
                if (DeleteFolder(dir)) { removed++; log("[music] removed the staging folder an earlier import left: " + dir); }
            }
            return removed;
        }

        /// Removes a folder and what it holds; false when something of it stays. Never through a link (a link in it is removed as a link).
        static bool DeleteFolder(string dir)
        {
            for (int attempt = 0; attempt < 4; attempt++)
            {
                try
                {
                    if (!Directory.Exists(dir)) return true;
                    DeleteTree(dir);
                    if (!Directory.Exists(dir)) return true;
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
                Thread.Sleep(100 * (attempt + 1));
            }
            return !Directory.Exists(dir);
        }

        static void DeleteTree(string dir)
        {
            if ((File.GetAttributes(dir) & FileAttributes.ReparsePoint) != 0)
            {
                try { Directory.Delete(dir); }
                catch (IOException) when (Path.DirectorySeparatorChar != '\\') { File.Delete(dir); }
                return;
            }
            foreach (string d in Directory.GetDirectories(dir)) DeleteTree(d);
            foreach (string f in Directory.GetFiles(dir))
            {
                var a = File.GetAttributes(f);
                if ((a & FileAttributes.ReadOnly) != 0 && (a & FileAttributes.ReparsePoint) == 0) File.SetAttributes(f, a & ~FileAttributes.ReadOnly);
                File.Delete(f);
            }
            Directory.Delete(dir);
        }
    }
}
