// MX vs ATV Alive (Xbox 360): the soundtrack. The 23 songs of the game's jukebox on the player's own disc go into the client's music pool as
// the Alive soundtrack: <Mods/Music>/MX vs ATV Alive/<Artist> - <Title>.ogg (ReflexGameMusic: staging, publish, the rules for a folder made
// before), played by OPTIONS > AUDIO > SOUNDTRACK (ALL SONGS, ALIVE SONGS ONLY) with the NOW PLAYING pop-up and SKIP SONG.
//   the disc   Database/Music.xenon.{database,package}: MusicConfig/MusicAudioMap.bxml (Music/Jukebox/Track: Package, BankName, SongName,
//              ArtistName: the names below are the game's own, "when I Feel" capitalised), and one FMOD sound bank per song (package TrackNN,
//              asset MUS_INGAME_NN.soundbnk, LZX): a 0x40-byte bank header, then an FSB4 bank of one sample in XMA (sample mode 0x01000000),
//              44.1 kHz stereo, its data in 2048-byte XMA packets. The scenario scores and the menu themes are not songs: left out.
//   a song     Unity can't decode XMA, so each song is decoded and encoded once, here: its XMA data (checked against the table: size and
//              SHA-256, the supported disc's) in a RIFF / XMA2 header, which ffmpeg's xma2 decoder reads, encoded to Ogg Vorbis (libvorbis,
//              quality 6, about 190 kbps; an ffmpeg without libvorbis: MP3 with libmp3lame) with the title, artist, album and year as tags.
//              Every file made is decoded again with no error and (Ogg) holds the song's length to the sample block
//   ffmpeg     the client's own (<client root>/ffmpeg: the Windows package ships an LGPL build there for the videos; REFLEX_FFMPEG, the
//              usual install folders, PATH: ReflexVideoSource's FfmpegBinkSource.Locate, which the caller passes in). Without one the
//              soundtrack can't be imported and the screen says so.
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using Reflex.Port.Content.UntamedImport;

namespace Reflex.Port.Content.AliveImport
{
    /// A song of the supported disc's jukebox.
    public sealed class AliveSong
    {
        /// The package and bank on the disc; the artist and title shown; the XMA data's size, SHA-256 and length in samples (44.1 kHz).
        public readonly string package, bank, artist, title, xmaSha256;
        public readonly long xmaBytes, samples;
        public AliveSong(string package, string bank, string artist, string title, long xmaBytes, long samples, string xmaSha256)
        { this.package = package; this.bank = bank; this.artist = artist; this.title = title; this.xmaBytes = xmaBytes; this.samples = samples; this.xmaSha256 = xmaSha256; }
        public string Label => artist + " - " + title;
        public double Seconds => samples / 44100.0;
    }

    /// The soundtrack found in the player's sources.
    public sealed class AliveMusicSource
    {
        public Volume volume;
        /// The volume's folder of the Music files ("Database", "<folder>/Database").
        public string directory;
        /// Why it can't be imported (null: it is the supported disc's).
        public string problem;
        /// The soundtrack's folder in Mods/Music as it is now (RefreshStatus; never null).
        public ReflexGameMusicStatus status = new ReflexGameMusicStatus();
        public bool Ok => problem == null;
        public bool Complete => status.imported != null && status.present == AliveMusic.Songs.Length;
    }

    public static class AliveMusic
    {
        public const string ImporterVersion = "alive-music/1";
        const string Year = "2011";
        const string MusicLevel = "Music", BankType = "soundbnk";
        /// The songs' encoding: Ogg Vorbis at this quality (about 190 kbps for this music), or MP3 at this VBR quality.
        const string VorbisQuality = "6", Mp3Quality = "2";

        // Database/Music.xenon.database and .package of the supported disc: sizes, and the database's SHA-256.
        const long DatabaseSize = 3498, PackageSize = 80655966;
        const string DatabaseSha256 = "658b77759f95894dad0ae7c8d40f7d2a9f80d9e4ab1ce28ed240d511d9933303";

        /// The jukebox (Music/Jukebox in MusicAudioMap.bxml), in its order.
        public static readonly AliveSong[] Songs =
        {
            new AliveSong("Track01", "MUS_INGAME_01", "Arkaea", "Black Ocean", 3600384, 11010304, "f367ef1c961e8fdcb045874fefccaafe8812ca0cccc0d12bc90efb461dc352f4"),
            new AliveSong("Track02", "MUS_INGAME_02", "BeetRoots", "New Noise", 2256896, 5843200, "b3abf8370be2251e5323d5cffbd71e92cc196e73cb8fb03467ef683c57f5e13e"),
            new AliveSong("Track03", "MUS_INGAME_03", "Vegas Child", "Steve McQueen", 3041280, 8589312, "87d15edce93eeb935f3861307d4da7310d30bec24af4e256a86806dd946a9ea8"),
            new AliveSong("Track04", "MUS_INGAME_04", "Jay Reatard", "Turning Blue", 2908160, 7083648, "d414a2210febcd6a97a8c32be2761501d74ce396e90506fe0dca2afb07c58b74"),
            new AliveSong("Track05", "MUS_INGAME_05", "Thee Outlaws", "What's Your Problem", 3067904, 10244736, "f20938e3811bdb15d8617839da4d01d637e2d1f852b32670ffe775df3d32936a"),
            new AliveSong("Track06", "MUS_INGAME_06", "The Sword", "Tres Brujas", 3913728, 10670080, "88a1aca745309bb49e0bb7cbf6efa0da41acb69bb0e49644089cc95492732ba1"),
            new AliveSong("Track07", "MUS_INGAME_07", "Anberlin", "The Resistance", 3020800, 8635008, "331d9dd4b877585bafa7e229dc13fa5f8ba5d95c123a1abef6c01e18f466b0b8"),
            new AliveSong("Track08", "MUS_INGAME_08", "Robert Cote", "Now With More Meat", 2826240, 7542656, "7e299a10c1cfb910b17cea6e5438138b9bb986c1256f6b23d51501870edbd07d"),
            new AliveSong("Track09", "MUS_INGAME_09", "3volution", "Fight to the Finish", 3266560, 8991744, "2af50695a5e70d916bc204867d0fbaddbdba5ba3bc0b3da12ecb7c53e3580eeb"),
            new AliveSong("Track10", "MUS_INGAME_10", "3volution", "Go Hard", 3495936, 8934912, "22cd6287792319157ded991bc7aa4f78cbaa5007109f8ae3356e05da86417dcd"),
            new AliveSong("Track11", "MUS_INGAME_11", "3volution", "Race King", 2996224, 8155008, "305f0a2a543971baf1cca3fd39507628656a54d88b5ee9576eefc3ac90fafd66"),
            new AliveSong("Track12", "MUS_INGAME_12", "3volution", "Red Line", 2551808, 6093312, "712d2bc50f2babcb91678b3d6e9392987760f8284204358e99ab96387c71b9e9"),
            new AliveSong("Track13", "MUS_INGAME_13", "3volution", "Slow Down", 3330048, 8934784, "ab89ff8b09f8a8e4958defa92900806d111abd0d736c51ddcea638c567575252"),
            new AliveSong("Track14", "MUS_INGAME_14", "3volution", "Dirt Pit", 4395008, 9780864, "1be9d3dc8103136bb7475009cf2d1de254c84d2f4c6ef12c079e41e5a37ff175"),
            new AliveSong("Track15", "MUS_INGAME_15", "3volution", "Electroad", 4044800, 9498624, "24657e8d4ed2af2a5aee1f514110ef9165a7f703fc5c8d9b7cc44ffea78f25ee"),
            new AliveSong("Track16", "MUS_INGAME_16", "Street Dogs", "Rattle and Roll", 1880064, 4866944, "dfb991506df4b8ee9556dedc62e940f8fd7fa8a2157e86d2fea0521c08243fd8"),
            new AliveSong("Track17", "MUS_INGAME_17", "Black Pacific", "The System", 2689024, 7076864, "7666c5aab854fe123aa8c0144bd5b9021cf2ce99c65d788ca9ca3907d9938e98"),
            new AliveSong("Track18", "MUS_INGAME_18", "Rise Against", "Architects", 3381248, 9796352, "349279c1afc82b37e273391d9e45edc2db0911485100db42d6e59dab76cd01c6"),
            new AliveSong("Track19", "MUS_INGAME_19", "Bounty Hunter", "One Nation's Warpath", 2037760, 5870080, "921a0198ad232be01c96ee6acde1efc0953714b42ac0facd58f1ced75729949e"),
            new AliveSong("Track20", "MUS_INGAME_20", "Pop Evil", "Last Man Standing", 3078144, 8809728, "4e1a7f922efd100eecd191b18cb8992c026e38bb5de559037b5eb859f5a2afbb"),
            new AliveSong("Track21", "MUS_INGAME_21", "CSS", "I Fly", 3280896, 8617344, "309b49f0dd70010e2d045abaf48604cf00e01f910e5748eeb4566bc51e870287"),
            new AliveSong("Track22", "MUS_INGAME_22", "Mayfield", "When I Feel", 2455552, 6496256, "9ff7085bcfef6c882768d1abee992ac2bbec81d1360a865b9bc732d488b0654f"),
            new AliveSong("Track23", "MUS_INGAME_23", "Heartsongs", "The Song Inside Me", 3532800, 9480960, "67ed12238eafe30785cc1cb9988f032d8e230c4f200b2d58d174e5956e3ea559"),
        };

        /// About what the songs take as Ogg Vorbis (quality 6: about 190 kbps).
        public static long MusicBytes => (long)(Songs.Sum(s => s.Seconds) * 190000 / 8) + 4096;
        public static double TotalSeconds => Songs.Sum(s => s.Seconds);

        // ------------------------------------------------------------------------------------------------ the scan

        /// The soundtrack in the scan's sources (AliveSources.volumes): Database/Music.xenon.{database,package} of an Alive disc (the disc image or an extracted folder;
        /// the first that matches the supported disc, else the first found with its difference). null: none. Hashes the songs imported
        /// before (musicFolder: Mods/Music, null skips that): on a worker thread.
        public static AliveMusicSource Find(IEnumerable<Volume> volumes, string musicFolder, CancellationToken cancel = default)
        {
            if (volumes == null) return null;
            AliveMusicSource best = null;
            foreach (var v in volumes.ToList())
            {
                foreach (var f in v.files.Where(f => string.Equals(f.Name, MusicLevel + ".xenon.database", StringComparison.OrdinalIgnoreCase)).ToList())
                {
                    cancel.ThrowIfCancellationRequested();
                    var src = new AliveMusicSource { volume = v, directory = f.Directory ?? "" };
                    src.problem = Check(src, cancel);
                    if (best == null || (best.problem != null && src.problem == null)) best = src;
                    if (src.problem == null) break;
                }
                if (best != null && best.problem == null) break;
            }
            if (best != null) RefreshStatus(best, musicFolder);
            return best;
        }

        public static void RefreshStatus(AliveMusicSource src, string musicFolder)
        {
            if (src != null) src.status = ReflexGameMusic.Status(musicFolder, ReflexSongLibrary.Alive);
        }

        static string PathOf(AliveMusicSource s, string suffix) => (s.directory.Length == 0 ? "" : s.directory + "/") + MusicLevel + ".xenon." + suffix;

        static string Check(AliveMusicSource s, CancellationToken cancel)
        {
            var db = s.volume.Find(PathOf(s, "database")); var pkg = s.volume.Find(PathOf(s, "package"));
            if (db == null) return "missing Database/" + MusicLevel + ".xenon.database";
            if (pkg == null) return "missing Database/" + MusicLevel + ".xenon.package";
            if (db.size != DatabaseSize || pkg.size != PackageSize) return "the soundtrack differs from the supported disc (size)";
            try
            {
                using (var h = SHA256.Create())
                    if (UxBin.Hex(h.ComputeHash(s.volume.Read(db, cancel))) != DatabaseSha256) return MusicLevel + ".xenon.database differs from the supported disc";
            }
            catch (AliveImportException ex) { return ex.Message; }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException) { return "can't be read: " + ex.Message; }
            return null;
        }

        // ------------------------------------------------------------------------------------------------ a song

        /// A song's bank -> where its XMA data is (offset, length) and its length in samples. The bank checked: FSB4, one sample, XMA, 44.1 kHz
        /// stereo, whole 2048-byte packets that end with the bank.
        public static (int offset, int length, long samples) Fsb4Xma(byte[] b, string name)
        {
            int i = -1;
            for (int p = 0; p + 4 <= Math.Min(b.Length, 0x400); p++) if (b[p] == 'F' && b[p + 1] == 'S' && b[p + 2] == 'B' && b[p + 3] == '4') { i = p; break; }
            UxBin.Require(i >= 0 && i + 48 + 80 <= b.Length, name + ": not an FMOD sound bank (FSB4)");
            int I32(int p) => b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24;
            int count = I32(i + 4), headers = I32(i + 8), data = I32(i + 12);
            UxBin.Require(count == 1, name + ": a bank of " + count + " sounds, not one song");
            int sh = i + 48;
            long samples = (uint)I32(sh + 32); int length = I32(sh + 36); uint mode = (uint)I32(sh + 48); int freq = I32(sh + 52);
            int channels = b[sh + 62] | b[sh + 63] << 8;
            UxBin.Require((mode & 0x01000000) != 0, name + ": not XMA (sample mode " + mode.ToString("X8", CultureInfo.InvariantCulture) + ")");
            UxBin.Require(freq == 44100 && channels == 2, name + ": not 44.1 kHz stereo");
            int at = i + 48 + headers;
            UxBin.Require(length > 0 && length == data && length % 2048 == 0 && at + length == b.Length, name + ": the XMA data does not fill the bank");
            return (at, length, samples);
        }

        /// The XMA data in a RIFF WAVE file with an XMA2WAVEFORMATEX (one stream, stereo, 44.1 kHz, 64 KB blocks): what ffmpeg's xma2 decoder reads.
        public static byte[] XmaWave(byte[] b, int offset, int length, long samples)
        {
            var ms = new MemoryStream(length + 128);
            var w = new BinaryWriter(ms);
            int blocks = (length + 0xFFFF) / 0x10000;
            w.Write(Encoding.ASCII.GetBytes("RIFF")); w.Write(4 + 8 + 52 + 8 + length); w.Write(Encoding.ASCII.GetBytes("WAVE"));
            w.Write(Encoding.ASCII.GetBytes("fmt ")); w.Write(52);
            w.Write((ushort)0x166); w.Write((ushort)2); w.Write(44100); w.Write(44100 * 4); w.Write((ushort)2048); w.Write((ushort)16); w.Write((ushort)34);
            w.Write((ushort)1); w.Write(3);                                     // streams, channel mask (front left, right)
            w.Write((uint)samples); w.Write(0x10000);                          // samples encoded, bytes per block
            w.Write(0); w.Write((uint)samples); w.Write(0); w.Write(0);        // play begin, length; loop begin, length
            w.Write((byte)0); w.Write((byte)4); w.Write((ushort)blocks);        // loop count, encoder version, blocks
            w.Write(Encoding.ASCII.GetBytes("data")); w.Write(length);
            w.Write(b, offset, length);
            w.Flush();
            return ms.ToArray();
        }

        // ------------------------------------------------------------------------------------------------ ffmpeg

        /// The encoder this ffmpeg has for the songs ("libvorbis", else "libmp3lame"); null: neither (or it does not run).
        public static string Encoder(string ffmpeg)
        {
            if (string.IsNullOrEmpty(ffmpeg) || !File.Exists(ffmpeg)) return null;
            try
            {
                var (code, output, _) = Run(ffmpeg, new List<string> { "-hide_banner", "-encoders" }, CancellationToken.None, 20000);
                if (code != 0) return null;
                if (output.Contains(" libvorbis ")) return "libvorbis";
                if (output.Contains(" libmp3lame ")) return "libmp3lame";
            }
            catch (Exception e) when (e is IOException || e is System.ComponentModel.Win32Exception || e is InvalidOperationException || e is TimeoutException) { }
            return null;
        }

        /// Runs ffmpeg: (exit code, its output, its error output). Killed when cancelled (OperationCanceledException) or after timeoutMs.
        static (int code, string output, string errors) Run(string ffmpeg, List<string> args, CancellationToken cancel, int timeoutMs)
        {
            var psi = new ProcessStartInfo(ffmpeg) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = false, CreateNoWindow = true };
            foreach (string a in args) psi.ArgumentList.Add(a);
            var output = new StringBuilder(); var errors = new StringBuilder();
            using (var p = new Process { StartInfo = psi })
            {
                p.OutputDataReceived += (s, e) => { if (e.Data != null) lock (output) output.AppendLine(e.Data); };
                p.ErrorDataReceived += (s, e) => { if (e.Data != null) lock (errors) errors.AppendLine(e.Data); };
                p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine();
                var clock = Stopwatch.StartNew();
                while (!p.WaitForExit(100))
                {
                    if (cancel.IsCancellationRequested || clock.ElapsedMilliseconds > timeoutMs)
                    {
                        try { p.Kill(); } catch (Exception) { }
                        try { p.WaitForExit(2000); } catch (Exception) { }
                        cancel.ThrowIfCancellationRequested();
                        throw new TimeoutException("ffmpeg did not finish in " + timeoutMs / 1000 + " s");
                    }
                }
                p.WaitForExit();   // (the output read to its end)
                lock (output) lock (errors) return (p.ExitCode, output.ToString(), errors.ToString());
            }
        }

        static string FirstLine(string s) { s = (s ?? "").Trim(); int n = s.IndexOf('\n'); return (n < 0 ? s : s.Substring(0, n)).Trim(); }

        /// The length in samples an Ogg file ends at: the granule position of its last page (-1: not an Ogg stream).
        public static long OggSamples(string path)
        {
            using (var f = File.OpenRead(path))
            {
                int n = (int)Math.Min(f.Length, 1 << 16);
                var b = new byte[n]; f.Seek(-n, SeekOrigin.End);
                int got = 0; while (got < n) { int r = f.Read(b, got, n - got); if (r <= 0) break; got += r; }
                for (int p = got - 27; p >= 0; p--)
                    if (b[p] == 'O' && b[p + 1] == 'g' && b[p + 2] == 'g' && b[p + 3] == 'S' && b[p + 4] == 0) return BitConverter.ToInt64(b, p + 6);
            }
            return -1;
        }

        /// One song: the bank's XMA data checked against the table, decoded and encoded into path (with its tags), the result checked.
        static void Transcode(string ffmpeg, string encoder, AliveSong s, byte[] bank, string path, CancellationToken cancel)
        {
            var (offset, length, samples) = Fsb4Xma(bank, s.bank);
            string sha; using (var h = SHA256.Create()) sha = UxBin.Hex(h.ComputeHash(bank, offset, length));
            if (length != s.xmaBytes || samples != s.samples || sha != s.xmaSha256) throw new UxException(s.Label + " differs from the supported disc's song");
            string wav = Path.Combine(Path.GetDirectoryName(path), "." + Guid.NewGuid().ToString("N").Substring(0, 8) + ".xma.wav");
            try
            {
                File.WriteAllBytes(wav, XmaWave(bank, offset, length, samples));
                bool ogg = encoder == "libvorbis";
                var args = new List<string> { "-nostdin", "-hide_banner", "-v", "error", "-y", "-i", wav, "-map", "0:a:0", "-map_metadata", "-1", "-c:a", encoder, "-q:a", ogg ? VorbisQuality : Mp3Quality };
                if (!ogg) { args.Add("-id3v2_version"); args.Add("3"); }
                foreach (var (k, v) in new[] { ("title", s.title), ("artist", s.artist), ("album", ReflexSongLibrary.Alive.album), ("date", Year) }) { args.Add("-metadata"); args.Add(k + "=" + v); }
                args.Add("-f"); args.Add(ogg ? "ogg" : "mp3"); args.Add(path);
                var (code, _, errors) = Run(ffmpeg, args, cancel, 300000);
                if (code != 0 || !File.Exists(path)) throw new UxException(s.Label + ": ffmpeg could not convert it (" + (FirstLine(errors).Length > 0 ? FirstLine(errors) : "exit code " + code) + ")");
                if (FirstLine(errors).Length > 0) throw new UxException(s.Label + ": ffmpeg reported an error while converting it (" + FirstLine(errors) + ")");
            }
            finally { try { File.Delete(wav); } catch (Exception) { } }
            // the file plays to its end: decoded again with no error, and (Ogg) as long as the song
            var (c2, _, e2) = Run(ffmpeg, new List<string> { "-nostdin", "-hide_banner", "-v", "error", "-i", path, "-f", "null", "-" }, cancel, 300000);
            if (c2 != 0 || FirstLine(e2).Length > 0) throw new UxException(s.Label + ": the converted file does not decode cleanly (" + (FirstLine(e2).Length > 0 ? FirstLine(e2) : "exit code " + c2) + ")");
            if (encoder == "libvorbis")
            {
                long got = OggSamples(path);
                if (Math.Abs(got - samples) > 4096) throw new UxException(s.Label + ": the converted file is " + (got / 44100.0).ToString("0.0", CultureInfo.InvariantCulture) + " s long, the song " + (samples / 44100.0).ToString("0.0", CultureInfo.InvariantCulture) + " s");
            }
        }

        // ------------------------------------------------------------------------------------------------ the import

        /// Converts the soundtrack into musicFolder/MX vs ATV Alive (ReflexGameMusic.Install's rules). ffmpeg: the executable (null: not
        /// found). Never throws: the result says what happened. progress: (step, 0..1).
        public static ReflexGameMusicResult Import(AliveMusicSource src, string musicFolder, string ffmpeg, Func<string, long?> freeBytes, Action<string> log,
                                                   Action<string, float> progress, CancellationToken cancel)
        {
            ReflexGameMusicResult Fail(string why) => new ReflexGameMusicResult { state = ReflexGameMusicState.Failed, error = why };
            if (src == null) return Fail("No MX vs ATV Alive soundtrack was found in these sources (it is on the game disc: Database/Music.xenon.package).");
            if (!src.Ok) return Fail("The soundtrack can't be imported: " + src.problem);
            if (string.IsNullOrEmpty(ffmpeg)) return Fail("The Alive songs are converted with ffmpeg, which was not found: put an ffmpeg build in the ffmpeg folder beside the game (the Windows download has one) or set REFLEX_FFMPEG.");
            progress?.Invoke("Checking ffmpeg", 0f);
            string encoder = Encoder(ffmpeg);
            if (encoder == null) return Fail("This ffmpeg (" + ffmpeg + ") can't encode Ogg Vorbis (libvorbis) or MP3 (libmp3lame): the Alive songs need one of them.");
            string why;
            try { why = Check(src, cancel); } catch (OperationCanceledException) { return new ReflexGameMusicResult { state = ReflexGameMusicState.Cancelled }; }
            if (why != null) return Fail("The soundtrack can't be imported: " + why);
            UxDiscLevel disc = null; var gate = new object();
            string ext = encoder == "libvorbis" ? ".ogg" : ".mp3";
            var songs = Songs.Select(s => new ReflexGameSong
            {
                fileName = ReflexGameMusic.SongFileName(s.artist, s.title, ext), artist = s.artist, title = s.title,
                extra = new JObj().Set("package", s.package).Set("bank", s.bank).Set("xma_sha256", s.xmaSha256).Set("encoder", encoder),
                write = (path, c) =>
                {
                    lock (gate) disc = disc ?? new UxDiscLevel(src.volume, src.directory, null, new UxLevelRow(MusicLevel, "music", "", "", ReflexSongLibrary.Alive.album, "", "", ""), c) { CacheBytes = 0 };
                    var asset = disc.assets.FirstOrDefault(a => a.type == BankType && a.package == s.package && string.Equals(a.name, s.bank, StringComparison.Ordinal));
                    if (asset == null) throw new UxException(s.Label + " (" + s.package + "/" + s.bank + ") is not in the disc's soundtrack");
                    Transcode(ffmpeg, encoder, s, disc.Read(asset, c), path, c);
                },
            }).ToList();
            var source = new JObj().Set("game", "MX vs ATV Alive (Xbox 360)").Set("from", LastName(src.volume.origin))
                .Set("form", src.volume is DiscVolume ? "disc image" : "extracted disc folder").Set("level", MusicLevel).Set("encoder", encoder);
            return ReflexGameMusic.Install(musicFolder, ReflexSongLibrary.Alive, ImporterVersion, source, songs, MusicBytes, freeBytes, log, progress, cancel);
        }

        static string LastName(string path)
        {
            try { return Path.GetFileName((path ?? "").TrimEnd('/', '\\')) ?? ""; }
            catch (ArgumentException) { return ""; }
        }
    }
}
