// MX vs ATV Untamed (Xbox 360): the soundtrack. The 22 licensed songs of the player's own disc go into the client's music pool as the Untamed
// soundtrack: <Mods/Music>/MX vs ATV Untamed/<Artist> - <Title>.mp3 (ReflexGameMusic: staging, publish, the rules for a folder made before),
// played by OPTIONS > AUDIO > SOUNDTRACK (ALL SONGS, UNTAMED SONGS ONLY) with the NOW PLAYING pop-up and SKIP SONG.
//   the disc   Database/Music.xenon.{database,package} (beside the stadium levels): the asset MX_ATV_music (the playlist) and one 'sounddat'
//              per song, in a package of its own. A sounddat is a 72-byte big-endian header (version 1, 1, sample rate 44100, channels 2,
//              bits 16, the stream's bytes, ...; then the length in seconds as a float) and a plain MPEG-1 Layer III stream (96 kbps, 44.1 kHz,
//              joint stereo): Unity decodes it as it is, so nothing is transcoded.
//   a song     the stream, behind an ID3v2.3 tag (title, artist, album "MX vs ATV Untamed", year 2007) that ReflexSongLibrary reads for the
//              NOW PLAYING pop-up; every file is checked against the table below (its size and SHA-256: the supported disc's songs byte for
//              byte), and the stream frame by frame
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// A song of the supported disc's soundtrack.
    public sealed class UntamedSong
    {
        /// The sounddat asset (and package) name on the disc; the artist and title shown; the file's size and SHA-256 as the import writes it.
        public readonly string asset, artist, title, sha256;
        public readonly long bytes;
        public UntamedSong(string asset, string artist, string title, long bytes, string sha256) { this.asset = asset; this.artist = artist; this.title = title; this.bytes = bytes; this.sha256 = sha256; }
        /// "Artist - Title.mp3", with the characters a file name can't hold left out ("12:34" -> "12-34").
        public string FileName => ReflexGameMusic.SongFileName(artist, title, ".mp3");
        public string Label => artist + " - " + title;
    }

    /// The soundtrack found in the player's sources.
    public sealed class UntamedMusicSource
    {
        public Volume volume;
        /// The volume's folder of the Music files ("Database", "<folder>/Database").
        public string directory;
        /// Why it can't be imported (null: it is the supported disc's).
        public string problem;
        /// The soundtrack's folder in Mods/Music as it is now (RefreshMusic; never null).
        public ReflexGameMusicStatus status = new ReflexGameMusicStatus();
        /// What the songs take on the disk.
        public long bytes;
        public bool Ok => problem == null;
        /// Why the import can't write <Mods/Music>/MX vs ATV Untamed (null: it can): something of the player's own is there.
        public string inTheWay => status.inTheWay;
        /// The folder this importer made before (relative to Mods/Music; null: none) and how many of the songs are there as it wrote them.
        public string imported => status.imported;
        public int present => status.present;
        /// Imported before, every song there unchanged.
        public bool Complete => imported != null && present == UntamedImporter.Songs.Length;
    }

    public static partial class UntamedImporter
    {
        /// The folder in Mods/Music the songs go into, and the manifest that tells it from one of the player's own.
        public static string MusicSubfolder => ReflexSongLibrary.Untamed.folder;
        public static string MusicManifestName => ReflexSongLibrary.Untamed.manifest;
        public const string MusicImporterVersion = "untamed-music/1";
        const string MusicLevel = "Music";
        const string SongType = "sounddat";
        const string Year = "2007";
        const int SongHeader = 72;

        // Database/Music.xenon.database and .package of the supported disc (title 545107DE): sizes, and the database's SHA-256 (the songs are
        // checked one by one as they are written).
        const long MusicDatabaseSize = 2260, MusicPackageSize = 47474639;
        const string MusicDatabaseSha256 = "bf2ba43cb7d1c91bfbb24d445cce4d9a30cb592159f0dd902384271f67fa7691";

        /// The soundtrack, in the disc's package order. Titles and artists as the songs are known (the asset names are lowercase with the
        /// artist last: "high_of_75_relient_k").
        public static readonly UntamedSong[] Songs =
        {
            new UntamedSong("collapse_saosin", "Saosin", "Collapse", 2378068, "53a8182124810d782d0b95fc7b8bc52fd6b4333f566abc36026f46cfe7e31a9e"),
            new UntamedSong("dinosaurs_will_die_nofx", "NOFX", "Dinosaurs Will Die", 2063979, "1fd4622c5ef669c390a01ed7f2f20300d1c2f780f9ead35486cfc7b1673c8e88"),
            new UntamedSong("hearts_and_stars_cage_9", "Cage9", "Hearts and Stars", 2472742, "8f026849ca34fd74e44495e0b3b27e5c161ec083b8d71c90c2bb00ade9c9f809"),
            new UntamedSong("disconnect_pennywise", "Pennywise", "Disconnect", 2163033, "2ee8f085a2d9cf61f01ee9ea2006bc493a72e73d08f02d407ac1c5ec411bfe46"),
            new UntamedSong("loose_primer_55", "Primer 55", "Loose", 2184030, "44f23ff9750c5acd174fa83be6eb3a3ed707c44937956976d89743b277987089"),
            new UntamedSong("facedown_red_jumpsuit_apparatus", "The Red Jumpsuit Apparatus", "Face Down", 2339845, "f0821eaed07217b2919a579d0586bba197e5d299db2cc4aa43a0f2b77d06da5e"),
            new UntamedSong("general_admission_mower", "Mower", "General Admission", 1775901, "37efba1cef4b609bb7c6e4b082dbe6036de68e11403a111579087a4fc9590d15"),
            new UntamedSong("on_edge_authority_zero", "Authority Zero", "On Edge", 2041722, "5a6a015bfbca154a758599080aa48ccce88fe7959a62f18ef7c348a13526b5a4"),
            new UntamedSong("guarded_disturbed", "Disturbed", "Guarded", 2388414, "ffc95c4b3024b508cec948b7c1d211f05898b23cc2a9ba081faafcf23e1607c7"),
            new UntamedSong("happy_mudvayne", "Mudvayne", "Happy?", 2568030, "73a4e70f58dade0c77ed7acd40c97240764f4407cc798b7f852a5f1eb8b46b38"),
            new UntamedSong("12_34_authority_zero", "Authority Zero", "12:34", 2208172, "d973ac4f47ba3818b422039924b28751d408bb4986edb869350e2e3e03ba2869"),
            new UntamedSong("high_of_75_relient_k", "Relient K", "High of 75", 1809753, "5d64557607b628f2467c62f0fc09c7762357a6eb3cb65b7e4bc46ff69d6c8b6b"),
            new UntamedSong("house_of_wolves_my_chemical_romance", "My Chemical Romance", "House of Wolves", 2204426, "2b8e0eba307dcb899f8ad59800cd5039db7f19191e0f4e8266507bc608b0f0dd"),
            new UntamedSong("kwad_up_kasper", "Kasper", "Kwad Up", 2884947, "bf86fcb8e88ffaabf6161c7f68b3591d02f41e64cf79e12e4ad5e0b3d823af8c"),
            new UntamedSong("lobotomy_for_dummies_zebrahead", "Zebrahead", "Lobotomy for Dummies", 1878099, "55010ef67e7c8d402b7498bad38eb30438ad44c1380e775ab0e629de58ca0650"),
            new UntamedSong("secret_weapon_mxpx", "MxPx", "Secret Weapon", 1528569, "11e2bb3254de991c50cb7445e8f7e7979858eb2f5c94e59b1360dc106e5c12df"),
            new UntamedSong("shapeshifter_celldweller", "Celldweller", "Shapeshifter", 2579638, "f909ccde0477b07b02f9d375a2253ef50b310b51035fda5f29adbc1dd0c220d8"),
            new UntamedSong("side_of_a_bullet_nickelback", "Nickelback", "Side of a Bullet", 2111944, "916ddd052d172a6d6d4a8fed38e56ca983144ffe01449198ee6d7a71ed33429d"),
            new UntamedSong("social_suicide_bad_religion", "Bad Religion", "Social Suicide", 1152101, "1bc7969c42b7317067b34c44889a0f895b4b9375e308505d566e235305fd27e8"),
            new UntamedSong("soul_crusher_operator", "Operator", "Soul Crusher", 2560513, "a6ae29448e512e65c4c408d260ace7c3350eff8d5c701b30a526cf6a3c730e2c"),
            new UntamedSong("i_dont_like_you_electric_six", "Electric Six", "I Don't Like You", 2084674, "6ebaa8a053aca407f3e09e60c7499a6e690d533e9013381e5bee50b2435147c0"),
            new UntamedSong("the_ripper_the_used", "The Used", "The Ripper", 2095949, "d732241ca13311a91df3fe095d0f124991d54c1539492842aa815b17a708003b"),
        };

        /// What the soundtrack takes on the disk.
        public static long MusicBytes => Songs.Sum(s => s.bytes) + 4096;

        // ------------------------------------------------------------------------------------------------ the scan

        /// The soundtrack in the scan's sources: Database/Music.xenon.{database,package} beside the stadium levels of an Untamed disc (the first
        /// that matches the supported disc, else the first found with its difference); null when no source has one. musicFolder: Mods/Music
        /// (read on the main thread: ReflexCustomMusic.Folder), for the songs imported before; null skips that. Hashes the songs imported
        /// before: on a worker thread.
        public static UntamedMusicSource FindMusic(UntamedSources found, string musicFolder, CancellationToken cancel = default)
        {
            if (found == null) return null;
            UntamedMusicSource best = null;
            // the folders of the level files that hold an Untamed stadium (another game's Database folder is never taken)
            foreach (var (v, dir) in found.tracks.Select(t => (t.volume, t.directory)).Distinct().ToList())
            {
                cancel.ThrowIfCancellationRequested();
                var src = new UntamedMusicSource { volume = v, directory = dir };
                src.problem = CheckMusic(src, cancel);
                if (best == null || (best.problem != null && src.problem == null)) best = src;
                if (src.problem == null) break;
            }
            if (best == null) return null;
            best.bytes = Songs.Sum(s => s.bytes);
            RefreshMusic(best, musicFolder);
            return best;
        }

        /// What is in Mods/Music now: the folder imported before, its songs, what is in the way. Hashes the songs: on a worker thread.
        public static void RefreshMusic(UntamedMusicSource src, string musicFolder)
        {
            if (src == null) return;
            src.status = ReflexGameMusic.Status(musicFolder, ReflexSongLibrary.Untamed);
        }

        static string MusicPath(UntamedMusicSource s, string suffix) => (s.directory.Length == 0 ? "" : s.directory + "/") + MusicLevel + ".xenon." + suffix;

        /// null: the supported disc's Music files (sizes, the database's SHA-256); else what differs.
        static string CheckMusic(UntamedMusicSource s, CancellationToken cancel)
        {
            var db = s.volume.Find(MusicPath(s, "database")); var pkg = s.volume.Find(MusicPath(s, "package"));
            if (db == null) return "missing Database/" + MusicLevel + ".xenon.database";
            if (pkg == null) return "missing Database/" + MusicLevel + ".xenon.package";
            if (db.size != MusicDatabaseSize || pkg.size != MusicPackageSize) return "the soundtrack differs from the supported disc (size)";
            try
            {
                if (UxBin.Sha256(s.volume.Read(db, cancel)) != MusicDatabaseSha256) return MusicLevel + ".xenon.database differs from the supported disc";
            }
            catch (AliveImportException ex) { return ex.Message; }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException) { return "can't be read: " + ex.Message; }
            return null;
        }

        /// The manifest of the songs' folder (null: none, or it does not read).
        public static JObj ReadMusicManifest(string folder) => ReflexGameMusic.ReadManifest(folder, ReflexSongLibrary.Untamed);

        // ------------------------------------------------------------------------------------------------ a song

        /// A sounddat as the disc has it -> the MP3 stream in it (the header checked: 44.1 kHz stereo, the stream's length, an MPEG-1 Layer III
        /// stream of whole frames to its last byte).
        public static ArraySegment<byte> SongStream(byte[] d, string name)
        {
            UxBin.Require(d != null && d.Length > SongHeader + 4, name + ": too short to be a song");
            uint U(int p) => (uint)(d[p] << 24 | d[p + 1] << 16 | d[p + 2] << 8 | d[p + 3]);
            UxBin.Require(U(0) == 1 && U(8) == 44100 && U(12) == 2 && U(16) == 16, name + ": not a song of the supported disc (its header)");
            UxBin.Require(U(20) == d.Length - SongHeader, name + ": the stream's length does not match");
            int frames = 0;
            int[] kbps = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
            for (int p = SongHeader; p < d.Length;)
            {
                UxBin.Require(p + 4 <= d.Length && d[p] == 0xFF && (d[p + 1] & 0xFE) == 0xFA && (d[p + 2] & 0x0C) == 0, name + ": not an MP3 stream at byte " + p);
                int br = d[p + 2] >> 4; UxBin.Require(br > 0 && br < 15, name + ": an MP3 frame without a bit rate at byte " + p);
                p += 144000 * kbps[br] / 44100 + (d[p + 2] >> 1 & 1);
                UxBin.Require(p <= d.Length, name + ": the MP3 stream ends inside a frame");
                frames++;
            }
            UxBin.Require(frames > 0, name + ": an empty stream");
            return new ArraySegment<byte>(d, SongHeader, d.Length - SongHeader);
        }

        /// An ID3v2.3 tag: title, artist, album, year (ISO-8859-1 text frames; every name in the table is ASCII).
        public static byte[] SongTag(UntamedSong s)
        {
            var frames = new MemoryStream();
            void Frame(string id, string text)
            {
                byte[] b = Encoding.ASCII.GetBytes(text);
                int n = b.Length + 1;
                frames.Write(Encoding.ASCII.GetBytes(id), 0, 4);
                frames.Write(new[] { (byte)(n >> 24), (byte)(n >> 16), (byte)(n >> 8), (byte)n, (byte)0, (byte)0, (byte)0 }, 0, 7);   // size, flags, encoding 0
                frames.Write(b, 0, b.Length);
            }
            Frame("TIT2", s.title); Frame("TPE1", s.artist); Frame("TALB", ReflexSongLibrary.Untamed.album); Frame("TYER", Year);
            int size = (int)frames.Length;
            var tag = new MemoryStream();
            tag.Write(new byte[] { (byte)'I', (byte)'D', (byte)'3', 3, 0, 0, (byte)(size >> 21 & 0x7F), (byte)(size >> 14 & 0x7F), (byte)(size >> 7 & 0x7F), (byte)(size & 0x7F) }, 0, 10);
            frames.Position = 0; frames.CopyTo(tag);
            return tag.ToArray();
        }

        /// The file the import writes for a song: the tag, then the disc's stream.
        public static byte[] SongFile(UntamedSong s, byte[] sounddat)
        {
            var stream = SongStream(sounddat, s.asset);
            byte[] tag = SongTag(s), file = new byte[tag.Length + stream.Count];
            Buffer.BlockCopy(tag, 0, file, 0, tag.Length);
            Buffer.BlockCopy(stream.Array, stream.Offset, file, tag.Length, stream.Count);
            return file;
        }

        // ------------------------------------------------------------------------------------------------ the import

        /// Writes the soundtrack into musicFolder/MX vs ATV Untamed (ReflexGameMusic.Install's rules). Never throws: the result says what
        /// happened. progress: (step, 0..1).
        public static ReflexGameMusicResult ImportMusic(UntamedMusicSource src, string musicFolder, UntamedImportOptions options, Action<string, float> progress, CancellationToken cancel)
        {
            options = options ?? new UntamedImportOptions();
            if (src == null || !src.Ok)
                return new ReflexGameMusicResult { state = ReflexGameMusicState.Failed, error = src == null ? "No MX vs ATV Untamed soundtrack was found in these sources." : "The soundtrack can't be imported: " + src.problem };
            UxDiscLevel disc = null;
            var gate = new object();
            string why = null;
            try { why = CheckMusic(src, cancel); } catch (OperationCanceledException) { return new ReflexGameMusicResult { state = ReflexGameMusicState.Cancelled }; }
            if (why != null) return new ReflexGameMusicResult { state = ReflexGameMusicState.Failed, error = "The soundtrack can't be imported: " + why };
            var songs = Songs.Select(s => new ReflexGameSong
            {
                fileName = s.FileName, artist = s.artist, title = s.title, bytes = s.bytes, sha256 = s.sha256, extra = new JObj().Set("asset", s.asset),
                write = (path, c) =>
                {
                    lock (gate) disc = disc ?? new UxDiscLevel(src.volume, src.directory, null, new UxLevelRow(MusicLevel, "music", "", "", ReflexSongLibrary.Untamed.album, "", "", ""), c) { CacheBytes = 0 };
                    var asset = disc.assets.FirstOrDefault(a => a.type == SongType && string.Equals(a.name, s.asset, StringComparison.Ordinal));
                    if (asset == null) throw new UxException(s.Label + " (" + s.asset + ") is not in the disc's soundtrack");
                    File.WriteAllBytes(path, SongFile(s, disc.Read(asset, c)));
                },
            }).ToList();
            var source = new JObj().Set("game", "MX vs ATV Untamed (Xbox 360)").Set("title_id", TitleId.ToString("X8", CultureInfo.InvariantCulture))
                .Set("from", LastName(src.volume.origin)).Set("form", src.volume is DiscVolume ? "disc image" : "extracted disc folder").Set("level", MusicLevel);
            return ReflexGameMusic.Install(musicFolder, ReflexSongLibrary.Untamed, MusicImporterVersion, source, songs, MusicBytes, options.freeBytes,
                                           options.log, progress, cancel);
        }
    }
}
