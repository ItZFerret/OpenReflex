// YOUR MUSIC, the pure part (System only, so tests/paths builds it outside Unity): the songs of the player's own music folder and what is
// known of each, after Open Unleashed's MxaSongLibrary (and the queue of its MxaMusic).
//   <client root>/Mods/Music (and its subfolders)  .mp3, .ogg, .wav files; Unity decodes them (ReflexCustomMusic)
//   title / artist / album / year   the file's ID3v2 (2.2, 2.3, 2.4) or ID3v1 tags (mp3), its Ogg Vorbis comments (ogg), else the file name
//                                   ("Artist - Title.mp3", a leading track number "01 - " dropped, "_" read as a blank)
// The game soundtracks (GameSoundtracks): the songs the Alive and Untamed importers write into Mods/Music/<the game's folder> with their manifest
// count as that game's (ReflexSong.source), not as the player's own; Pool picks what may play for OPTIONS > AUDIO > SOUNDTRACK and YOUR MUSIC.
// The queue (ShuffleQueue): one round of the songs, the ones not heard lately first and shuffled, the ones heard lately after them (the
// longest ago first), no artist twice in a row where another can follow, the song playing never opening the next round. The memory of what
// was heard (Heard) is kept across runs by the caller. Nothing of anyone's music is in the project: it is read where the player has it.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Reflex.Port.Content
{
    public sealed class ReflexSong
    {
        public const string SourceGame = "THE GAME", SourceCustom = "YOUR MUSIC", SourceAlive = "MX vs ATV ALIVE", SourceUntamed = "MX vs ATV UNTAMED";
        public string path, title, artist, album, source = SourceCustom;
        public int year;
        /// The file's size and write time when its tags were read (a changed file is read again).
        public long size, stamp;
        /// Stable across runs (the shuffle's memory): the source and the file name.
        public string Key => source + "|" + ReflexSongLibrary.FileName(path).ToLowerInvariant();
        /// "Artist - Title" (or the title alone).
        public string Label => string.IsNullOrWhiteSpace(artist) ? (title ?? "") : artist + " - " + title;
    }

    /// A game's soundtrack in the music folder: the folder its importer writes and the manifest that tells it from a folder of the player's own.
    public sealed class ReflexGameSoundtrack
    {
        public readonly string source, folder, manifest, album;
        public ReflexGameSoundtrack(string source, string folder, string manifest, string album) { this.source = source; this.folder = folder; this.manifest = manifest; this.album = album; }
    }

    public static class ReflexSongLibrary
    {
        public static readonly ReflexGameSoundtrack Alive = new ReflexGameSoundtrack(ReflexSong.SourceAlive, "MX vs ATV Alive", "alive-music.json", "MX vs ATV Alive");
        public static readonly ReflexGameSoundtrack Untamed = new ReflexGameSoundtrack(ReflexSong.SourceUntamed, "MX vs ATV Untamed", "untamed-music.json", "MX vs ATV Untamed");
        public static readonly ReflexGameSoundtrack[] GameSoundtracks = { Alive, Untamed };

        /// OPTIONS > AUDIO > SOUNDTRACK (ReflexSettings.Soundtrack).
        public const int SoundtrackAll = 0, SoundtrackReflex = 1, SoundtrackAlive = 2, SoundtrackUntamed = 3;
        public static readonly string[] SoundtrackNames = { "ALL SONGS", "REFLEX SONGS ONLY", "ALIVE SONGS ONLY", "UNTAMED SONGS ONLY" };
        /// YOUR MUSIC (ReflexSettings.CustomMusic).
        public const int YourMusicOff = 0, YourMusicMixIn = 1, YourMusicOnly = 2;

        /// What may play: the songs of the folder that may stand in for the game's tracks, and whether Reflex's own tracks may play (false:
        /// every playlist entry is one of the songs, and the menus play them instead of the game's themes).
        ///   SOUNDTRACK  ALL SONGS: Reflex's tracks, the Alive and the Untamed songs together; REFLEX / ALIVE / UNTAMED SONGS ONLY: that game's
        ///               (one whose songs are not imported plays Reflex's: never silence)
        ///   YOUR MUSIC  the player's own songs: OFF none, MIX IN among the soundtrack, ONLY instead of any soundtrack (while there are some)
        public static List<ReflexSong> Pool(IEnumerable<ReflexSong> songs, int yourMusic, int soundtrack, out bool gameTracks)
        {
            var yours = new List<ReflexSong>(); var alive = new List<ReflexSong>(); var untamed = new List<ReflexSong>();
            foreach (var s in songs ?? new ReflexSong[0])
            {
                if (s == null) continue;
                if (s.source == ReflexSong.SourceAlive) alive.Add(s);
                else if (s.source == ReflexSong.SourceUntamed) untamed.Add(s);
                else yours.Add(s);
            }
            if (yourMusic == YourMusicOnly && yours.Count > 0) { gameTracks = false; return yours; }
            var pool = new List<ReflexSong>();
            if (yourMusic != YourMusicOff) pool.AddRange(yours);
            bool all = soundtrack != SoundtrackReflex && soundtrack != SoundtrackAlive && soundtrack != SoundtrackUntamed;
            if (all || soundtrack == SoundtrackAlive) pool.AddRange(alive);
            if (all || soundtrack == SoundtrackUntamed) pool.AddRange(untamed);
            gameTracks = all || soundtrack == SoundtrackReflex || (soundtrack == SoundtrackAlive && alive.Count == 0) || (soundtrack == SoundtrackUntamed && untamed.Count == 0);
            if (pool.Count == 0) gameTracks = true;
            return pool;
        }

        /// Marks the songs of each game soundtrack folder of the music folder (the folder named so, in any case, holding its importer's
        /// manifest) with that game; every other song is the player's own.
        public static void Classify(string folder, List<ReflexSong> songs)
        {
            var dirs = new List<(string dir, string source)>();
            try
            {
                if (!string.IsNullOrEmpty(folder) && Directory.Exists(folder))
                    foreach (string d in Directory.GetDirectories(folder))
                        foreach (var g in GameSoundtracks)
                            if (string.Equals(Path.GetFileName(d), g.folder, StringComparison.OrdinalIgnoreCase) && File.Exists(Path.Combine(d, g.manifest)))
                                dirs.Add((d.TrimEnd('/', '\\') + Path.DirectorySeparatorChar, g.source));
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException) { }
            foreach (var s in songs)
            {   // (one write of the final value: the song objects are shared with the main thread, which reads them meanwhile)
                string source = ReflexSong.SourceCustom;
                foreach (var (dir, src) in dirs)
                    if (s.path != null && s.path.StartsWith(dir, StringComparison.OrdinalIgnoreCase)) { source = src; break; }
                if (s.source != source) s.source = source;
            }
        }

        public static readonly string[] Extensions = { ".mp3", ".ogg", ".wav" };
        /// Folders deeper than this, and lists longer than this, are not followed (a link loop, a whole drive dropped in by mistake).
        public const int MaxDepth = 8, MaxSongs = 4000;
        /// The head of a file the tag readers look at (ID3v2 frames are read from their place, so a large cover picture does not matter).
        const int HeadBytes = 256 * 1024;

        public static bool IsSong(string path)
        {
            string ext = Path.GetExtension(path ?? "").ToLowerInvariant();
            return Array.IndexOf(Extensions, ext) >= 0;
        }

        /// The last part of a path (either separator, whichever machine reads it).
        public static string FileName(string path)
        {
            if (string.IsNullOrEmpty(path)) return "";
            int i = path.LastIndexOfAny(new[] { '/', '\\' });
            return i >= 0 ? path.Substring(i + 1) : path;
        }

        /// What the desktops leave in folders (never a song): dot names (.DS_Store, AppleDouble "._x"), Thumbs.db, desktop.ini, __MACOSX, the recycle bin.
        public static bool DefaultJunk(string name)
        {
            if (string.IsNullOrEmpty(name) || name.StartsWith(".", StringComparison.Ordinal)) return true;
            return string.Equals(name, "Thumbs.db", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "desktop.ini", StringComparison.OrdinalIgnoreCase)
                || string.Equals(name, "__MACOSX", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "$RECYCLE.BIN", StringComparison.OrdinalIgnoreCase)
                || string.Equals(name, "System Volume Information", StringComparison.OrdinalIgnoreCase);
        }

        /// The songs in folder and its subfolders (files first, then each subfolder; case-insensitive order). isJunk names the files and folders
        /// to pass over (default: DefaultJunk, the same rule as ReflexMods.IsJunk). cache (path -> song, filled and used by the caller across scans) saves reading the tags of an
        /// unchanged file again. A missing folder or an unreadable one is an empty list.
        public static List<ReflexSong> Scan(string folder, Func<string, bool> isJunk = null, IDictionary<string, ReflexSong> cache = null)
        {
            var list = new List<ReflexSong>();
            if (string.IsNullOrEmpty(folder)) return list;
            isJunk = isJunk ?? DefaultJunk;
            try { if (Directory.Exists(folder)) Walk(folder, 0, list, isJunk, cache); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException) { }
            Classify(folder, list);
            return list;
        }

        static void Walk(string dir, int depth, List<ReflexSong> list, Func<string, bool> isJunk, IDictionary<string, ReflexSong> cache)
        {
            string[] files, dirs;
            try { files = Directory.GetFiles(dir); dirs = Directory.GetDirectories(dir); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return; }
            Array.Sort(files, StringComparer.OrdinalIgnoreCase); Array.Sort(dirs, StringComparer.OrdinalIgnoreCase);
            foreach (string f in files)
            {
                string name = Path.GetFileName(f);
                if (isJunk(name) || !IsSong(name)) continue;
                if (list.Count >= MaxSongs) return;
                ReflexSong s = null;
                long size = 0, stamp = 0;
                try { var fi = new FileInfo(f); size = fi.Length; stamp = fi.LastWriteTimeUtc.Ticks; } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { continue; }
                if (cache == null || !cache.TryGetValue(f, out s) || s.size != size || s.stamp != stamp)
                {
                    s = ReadTags(f);
                    s.size = size; s.stamp = stamp;
                    if (cache != null) cache[f] = s;
                }
                list.Add(s);
            }
            if (depth >= MaxDepth) return;
            foreach (string d in dirs) if (!isJunk(Path.GetFileName(d))) Walk(d, depth + 1, list, isJunk, cache);
        }

        // ------------------------------------------------------------------------------------------------ names and tags

        static readonly Regex TrackNumber = new Regex(@"^\d{1,3}\s*[.\-]\s+(?=\S)", RegexOptions.CultureInvariant);

        /// "01 - Artist - Title" / "Artist_-_Title" -> artist, title (no " - ": the whole name is the title).
        public static ReflexSong FromFileName(string path)
        {
            string name = FileName(path);
            int dot = name.LastIndexOf('.');
            if (dot > 0) name = name.Substring(0, dot);
            name = name.Replace('_', ' ').Trim();
            name = TrackNumber.Replace(name, "");   // "01 - ", "7. "
            var s = new ReflexSong { path = path, title = name };
            int dash = name.IndexOf(" - ", StringComparison.Ordinal);
            if (dash > 0 && dash + 3 < name.Length) { s.artist = name.Substring(0, dash).Trim(); s.title = name.Substring(dash + 3).Trim(); }
            return s;
        }

        /// A file's tags, else its name. Never throws for an unreadable or damaged file.
        public static ReflexSong ReadTags(string path)
        {
            var s = FromFileName(path);
            try
            {
                using (var f = File.OpenRead(path))
                {
                    string ext = Path.GetExtension(path).ToLowerInvariant();
                    if (ext == ".mp3")
                    {
                        if (!Id3v2(f, s) && f.Length >= 128)
                        {
                            f.Seek(-128, SeekOrigin.End);
                            var t = new byte[128];
                            if (ReadFull(f, t, 128) == 128) Id3v1(t, s);
                        }
                    }
                    else if (ext == ".ogg")
                    {
                        var head = new byte[Math.Min(f.Length, HeadBytes)];
                        int n = ReadFull(f, head, head.Length);
                        Vorbis(head, n, s);
                    }
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException) { }
            if (string.IsNullOrWhiteSpace(s.title)) s.title = FromFileName(path).title;
            return s;
        }

        static int ReadFull(Stream f, byte[] b, int count)
        {
            int n = 0;
            while (n < count) { int r = f.Read(b, n, count - n); if (r <= 0) break; n += r; }
            return n;
        }

        /// One tag field into the song (ID3 frame ids of 2.2 / 2.3 / 2.4 and Vorbis comment names); blank values are ignored.
        public static bool Set(ReflexSong s, string id, string v)
        {
            v = (v ?? "").Split('\0').Select(x => x.Trim()).FirstOrDefault(x => x.Length > 0) ?? "";
            if (v.Length == 0) return false;
            switch (id)
            {
                case "TIT2": case "TT2": case "TITLE": s.title = v; return true;
                case "TPE1": case "TP1": case "ARTIST": s.artist = v; return true;
                case "TALB": case "TAL": case "ALBUM": s.album = v; return true;
                case "TYER": case "TYE": case "TDRC": case "DATE": if (v.Length >= 4 && int.TryParse(v.Substring(0, 4), out int y) && y > 0) { s.year = y; return true; } return false;
            }
            return false;
        }

        /// ID3v2.2 / 2.3 / 2.4 text frames (title, artist, album, year) read from their place in the stream. False without a tag or text frame.
        public static bool Id3v2(Stream f, ReflexSong s)
        {
            f.Position = 0;
            var h = new byte[10];
            if (ReadFull(f, h, 10) != 10 || h[0] != 'I' || h[1] != 'D' || h[2] != '3') return false;
            int ver = h[3];
            if (ver < 2 || ver > 4) return false;
            long end = Math.Min(f.Length, 10L + Syncsafe(h, 6)), pos = 10;
            if (ver >= 3 && (h[5] & 0x40) != 0)
            {   // the extended header
                var e = new byte[4];
                f.Position = pos;
                if (ReadFull(f, e, 4) != 4) return false;
                pos += ver >= 4 ? Syncsafe(e, 0) : (long)(e[0] << 24 | e[1] << 16 | e[2] << 8 | e[3]) + 4;
            }
            int idLen = ver == 2 ? 3 : 4, hdr = ver == 2 ? 6 : 10;
            var fh = new byte[10];
            bool any = false;
            while (pos + hdr <= end)
            {
                f.Position = pos;
                if (ReadFull(f, fh, hdr) != hdr || fh[0] == 0) break;
                string id = Encoding.ASCII.GetString(fh, 0, idLen);
                long len = ver == 2 ? fh[3] << 16 | fh[4] << 8 | fh[5] : ver >= 4 ? Syncsafe(fh, 4) : (long)(fh[4] << 24 | fh[5] << 16 | fh[6] << 8 | fh[7]);
                long data = pos + hdr;
                if (len <= 0 || data + len > end) break;
                if (id[0] == 'T' && id != "TXXX" && id != "TXX" && len > 1 && len <= 8192)
                {
                    var b = new byte[len];
                    if (ReadFull(f, b, (int)len) == (int)len) any |= Set(s, id, Text(b, 1, (int)len - 1, b[0]));
                }
                pos = data + len;
            }
            return any;
        }

        static int Syncsafe(byte[] b, int p) => (b[p] & 0x7f) << 21 | (b[p + 1] & 0x7f) << 14 | (b[p + 2] & 0x7f) << 7 | (b[p + 3] & 0x7f);

        /// An ID3 text field: encoding byte 0 ISO-8859-1, 1 UTF-16 with a byte order mark, 2 UTF-16BE, 3 UTF-8.
        public static string Text(byte[] b, int p, int len, byte enc)
        {
            switch (enc)
            {
                case 1:
                    if (len >= 2 && b[p] == 0xFE && b[p + 1] == 0xFF) return Encoding.BigEndianUnicode.GetString(b, p + 2, len - 2);
                    if (len >= 2 && b[p] == 0xFF && b[p + 1] == 0xFE) return Encoding.Unicode.GetString(b, p + 2, len - 2);
                    return Encoding.Unicode.GetString(b, p, len);
                case 2: return Encoding.BigEndianUnicode.GetString(b, p, len);
                case 3: return Encoding.UTF8.GetString(b, p, len);
                default: return Latin1(b, p, len);
            }
        }

        /// ISO-8859-1 (every runtime: the code page tables are not in all of Unity's).
        public static string Latin1(byte[] b, int p, int len) { var c = new char[len]; for (int i = 0; i < len; i++) c[i] = (char)b[p + i]; return new string(c); }

        /// ID3v1 (the last 128 bytes: "TAG", title 30, artist 30, album 30, year 4).
        public static void Id3v1(byte[] t, ReflexSong s)
        {
            if (t == null || t.Length < 128 || t[0] != 'T' || t[1] != 'A' || t[2] != 'G') return;
            Set(s, "TIT2", Latin1(t, 3, 30)); Set(s, "TPE1", Latin1(t, 33, 30)); Set(s, "TALB", Latin1(t, 63, 30)); Set(s, "TYER", Latin1(t, 93, 4));
        }

        /// Ogg Vorbis comments: the comment header ("\x03vorbis", vendor, then TITLE= / ARTIST= / ALBUM= / DATE=).
        public static void Vorbis(byte[] b, int n, ReflexSong s)
        {
            for (int i = 0; i + 7 < n; i++)
            {
                if (b[i] != 3 || b[i + 1] != 'v' || b[i + 2] != 'o' || b[i + 3] != 'r' || b[i + 4] != 'b' || b[i + 5] != 'i' || b[i + 6] != 's') continue;
                int p = i + 7;
                if (p + 4 > n) return;
                int vendor = BitConverter.ToInt32(b, p);
                if (vendor < 0 || vendor > n) return;
                p += 4 + vendor;
                if (p + 4 > n) return;
                int count = BitConverter.ToInt32(b, p); p += 4;
                for (int c = 0; c < count && c < 256 && p + 4 <= n; c++)
                {
                    int len = BitConverter.ToInt32(b, p); p += 4;
                    if (len < 0 || p + len > n) return;
                    string kv = Encoding.UTF8.GetString(b, p, len); p += len;
                    int eq = kv.IndexOf('=');
                    if (eq > 0) Set(s, kv.Substring(0, eq).ToUpperInvariant(), kv.Substring(eq + 1));
                }
                return;
            }
        }

        // ------------------------------------------------------------------------------------------------ the queue

        /// One round of the songs (paths): the ones not among the most recent ~60 % heard first (shuffled), then the recent ones, the longest
        /// ago first; no artist twice in a row where another can follow; last (the song playing) never first.
        public static List<string> ShuffleQueue(IEnumerable<ReflexSong> songs, IList<string> recent, string last, Random rnd)
        {
            var uniq = new List<ReflexSong>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var s in songs) if (s != null && s.path != null && seen.Add(s.path)) uniq.Add(s);
            recent = recent ?? new List<string>();
            int window = uniq.Count <= 2 ? Math.Max(0, uniq.Count - 1) : Math.Max(1, (int)(uniq.Count * 0.6f));
            var rank = new Dictionary<string, int>();
            for (int i = Math.Max(0, recent.Count - window); i < recent.Count; i++) rank[recent[i]] = i;
            var fresh = uniq.Where(s => !rank.ContainsKey(s.Key)).ToList();
            for (int i = fresh.Count - 1; i > 0; i--) { int j = rnd.Next(i + 1); var t = fresh[i]; fresh[i] = fresh[j]; fresh[j] = t; }
            var stale = uniq.Where(s => rank.ContainsKey(s.Key)).OrderBy(s => rank[s.Key]).ToList();
            var q = fresh.Concat(stale).ToList();
            if (q.Count > 1 && last != null && string.Equals(q[0].path, last, StringComparison.OrdinalIgnoreCase)) { var t = q[0]; q[0] = q[q.Count - 1]; q[q.Count - 1] = t; }
            bool Same(ReflexSong a, ReflexSong b) => !string.IsNullOrEmpty(a.artist) && string.Equals(a.artist, b.artist, StringComparison.OrdinalIgnoreCase);
            // a song with the artist of the one before it trades places with the song anywhere that lowers the count of same-artist neighbours most (passes
            // repeat while a trade helps; where one artist is most of the songs some neighbours stay)
            int Local(int i, int j)
            {
                int c = 0;
                foreach (int p in new SortedSet<int> { i - 1, i, j - 1, j }) if (p >= 0 && p + 1 < q.Count && Same(q[p], q[p + 1])) c++;
                return c;
            }
            int budget = 120000;   // (trades tried: a folder that is mostly one artist cannot be separated, and must not stall the game)
            for (int pass = 0; pass < 6 && budget > 0; pass++)
            {
                bool traded = false;
                for (int i = 1; i < q.Count; i++)
                {
                    if (!Same(q[i], q[i - 1])) continue;
                    int bestJ = -1, bestGain = 0;
                    for (int j = 0; j < q.Count && budget > 0; j++)
                    {
                        if (j == i || j == i - 1) continue;
                        budget--;
                        int before = Local(i, j);
                        var t = q[i]; q[i] = q[j]; q[j] = t;
                        int gain = before - Local(i, j);
                        t = q[i]; q[i] = q[j]; q[j] = t;
                        if (gain > bestGain) { bestGain = gain; bestJ = j; }
                    }
                    if (bestJ >= 0) { var t = q[i]; q[i] = q[bestJ]; q[bestJ] = t; traded = true; }
                }
                if (!traded) break;
            }
            return q.Select(s => s.path).ToList();
        }

        /// A song heard: its key goes last in the memory (oldest first), which holds at most cap keys.
        public static void Heard(IList<string> recent, string key, int cap = 300)
        {
            if (recent == null || string.IsNullOrEmpty(key)) return;
            recent.Remove(key);
            recent.Add(key);
            while (recent.Count > cap) recent.RemoveAt(0);
        }
    }
}
