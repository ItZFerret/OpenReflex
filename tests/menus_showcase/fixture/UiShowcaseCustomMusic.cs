// TEST ONLY (w-custommusic): YOUR MUSIC (Content/ReflexCustomMusic.cs, Hud/ReflexNowPlaying.cs), from the product's own code. The player's client root is the
// run folder, so Mods/Music is <run>/Mods/Music; this page puts synthetic songs there (a generated WAV, and the encoder-made files of REFLEX_TEST_MUSIC_DIR
// when set: an ID3v2.3 and v2.4 mp3, an ID3v1 mp3, an ogg and a WAV with a '#' and '%' in its name, all tagged; made with ffmpeg / lame in a scratch folder,
// never committed). The master volume is 0 for the whole run: sounds are judged by their state.
//   custommusic   the folder and its README; the scan (tags of every format, no hidden files); Unity decodes each song (UnityWebRequestMultimedia);
//                 OPTIONS > AUDIO rows (YOUR MUSIC OFF / MIX IN / ONLY, OPEN MUSIC FOLDER, NOW PLAYING POP-UP); the NOW PLAYING pop-up as the menus' strip
//                 and the race's card (a custom song, the game's track, SKIPPED TO), waiting for a fade-in, hidden with the HUD or the option;
//                 ONLY in the menus: FeUiMusic plays the songs one after another, N skips, MIX IN hands the menus back to the themes.
// Shots: custommusic-options, custommusic-strip, custommusic-card, custommusic-card-game, custommusic-card-skipped, custommusic-menu-only.
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseCustomMusic
    {
        const string Tag = "[ui-showcase] [custommusic] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("custommusic", Journey);

        static void Log(string s) => Debug.Log(Tag + s);
        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "custommusic", what); if (ok) Log("ok: " + what); }
        static ReflexSong Find(string file) => ReflexCustomMusic.Songs.FirstOrDefault(s => string.Equals(ReflexSongLibrary.FileName(s.path), file, StringComparison.OrdinalIgnoreCase));

        static byte[] Wav(float seconds, float hz)
        {
            const int rate = 22050;
            int n = (int)(seconds * rate);
            using (var ms = new MemoryStream())
            using (var w = new BinaryWriter(ms))
            {
                w.Write(new[] { 'R', 'I', 'F', 'F' }); w.Write(36 + n * 2); w.Write(new[] { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' }); w.Write(16); w.Write((short)1); w.Write((short)1);
                w.Write(rate); w.Write(rate * 2); w.Write((short)2); w.Write((short)16); w.Write(new[] { 'd', 'a', 't', 'a' }); w.Write(n * 2);
                for (int i = 0; i < n; i++) w.Write((short)(Mathf.Sin(2f * Mathf.PI * hz * i / rate) * 9000f));
                return ms.ToArray();
            }
        }

        static void CopyTree(string from, string to)
        {
            Directory.CreateDirectory(to);
            foreach (var f in Directory.GetFiles(from)) File.Copy(f, Path.Combine(to, Path.GetFileName(f)), true);
            foreach (var d in Directory.GetDirectories(from)) CopyTree(d, Path.Combine(to, Path.GetFileName(d)));
        }

        static IEnumerator Journey()
        {
            AudioListener.volume = 0f;
            ReflexSettings.CustomMusic = 1; ReflexSettings.NowPlayingPopup = true; ReflexSettings.HudHidden = false;
            string dir = ReflexCustomMusic.Folder;
            Log("music folder " + dir);
            Check(dir != null && dir.Replace('\\', '/').EndsWith("/Mods/Music"), "the folder is Mods/Music of the client root");
            Check(Directory.Exists(dir) && File.Exists(Path.Combine(dir, "README.txt")), "Mods/Music exists with its README (ReflexMods.EnsureLayout)");

            // the songs: three generated WAVs (named "Artist - Title") and the encoder-made files
            Directory.CreateDirectory(Path.Combine(dir, "Generated"));
            File.WriteAllBytes(Path.Combine(dir, "Generated", "Test Artist - Sine Low.wav"), Wav(2.5f, 220f));
            File.WriteAllBytes(Path.Combine(dir, "Generated", "Test Artist - Sine Mid.wav"), Wav(2.5f, 440f));
            File.WriteAllBytes(Path.Combine(dir, "Generated", "Other Artist - Sine High.wav"), Wav(2.5f, 880f));
            File.WriteAllBytes(Path.Combine(dir, "Generated", "notes.txt"), new byte[10]);
            File.WriteAllBytes(Path.Combine(dir, "Generated", "._Test Artist - Sine Low.wav"), new byte[10]);
            string real = Environment.GetEnvironmentVariable("REFLEX_TEST_MUSIC_DIR");
            int expected = 3;
            if (!string.IsNullOrEmpty(real) && Directory.Exists(real))
            {
                CopyTree(real, Path.Combine(dir, "Copied"));
                expected += Directory.GetFiles(real, "*.*", SearchOption.AllDirectories).Count(f => !f.Contains(Path.DirectorySeparatorChar + ".") && ReflexSongLibrary.IsSong(f));
            }
            for (float t = 0; t < 20f && ReflexCustomMusic.Count != expected; t += 0.3f) { ReflexCustomMusic.Rescan(); yield return new WaitForSecondsRealtime(0.3f); }
            Log("songs: " + string.Join(" | ", ReflexCustomMusic.Songs.Select(s => $"{ReflexSongLibrary.FileName(s.path)} => [{s.artist}] [{s.title}] [{s.album}] {s.year}")));
            Check(ReflexCustomMusic.Count == expected, $"the scan finds {expected} songs, no hidden or AppleDouble file, no .txt (found {ReflexCustomMusic.Count})");
            var s1 = Find("Test Artist - Sine Low.wav");
            Check(s1 != null && s1.artist == "Test Artist" && s1.title == "Sine Low", "a WAV is named from its file name");
            if (!string.IsNullOrEmpty(real))
            {
                var a = Find("01 - mp3 v23.mp3"); Check(a != null && a.title == "Real Mp3 Title" && a.artist == "Real Mp3 Artist" && a.album == "Real Mp3 Album" && a.year == 2011, "ffmpeg's ID3v2.3 tags are read");
                a = Find("mp3 v24 utf8.mp3"); Check(a != null && a.title == "Tést Ärtist 日本語" && a.artist == "Zoë & The Bänd" && a.album == "Álbum" && a.year == 2019, "ffmpeg's ID3v2.4 UTF-8 tags are read");
                a = Find("ogg vorbis.ogg"); Check(a != null && a.title == "Real Ogg Title" && a.artist == "Real Ogg Artist" && a.album == "Real Ogg Album" && a.year == 2020, "ffmpeg's Vorbis comments are read");
                a = Find("id3v1 only.mp3"); Check(a != null && a.title == "V1 Only Title" && a.artist == "V1 Only Artist" && a.year == 1998, "lame's ID3v1 tag is read");
                a = Find("Hash # and % Name - Wäv.wav"); Check(a != null && a.artist == "Hash # and % Name" && a.title == "Wäv", "a name with '#', '%' and an umlaut is read");
            }

            // Unity decodes each one
            foreach (var s in ReflexCustomMusic.Songs.ToList())
            {
                AudioClip clip = null; bool done = false;
                var host = new GameObject("music load").AddComponent<UiShowcaseCoroutineHost>();
                host.StartCoroutine(ReflexCustomMusic.Load(s, c => { clip = c; done = true; }));
                for (float t = 0; t < 15f && !done; t += Time.unscaledDeltaTime) yield return null;
                Check(done && clip != null && clip.length > 2f && clip.length < 3.5f, $"Unity decodes {ReflexSongLibrary.FileName(s.path)} ({(clip ? clip.length.ToString("F2") + " s, " + clip.frequency + " Hz, " + clip.channels + " ch" : "FAILED")})");
                if (clip) UnityEngine.Object.Destroy(clip);
                UnityEngine.Object.Destroy(host.gameObject);
            }
            var queueSeen = Enumerable.Range(0, ReflexCustomMusic.Count).Select(_ => ReflexCustomMusic.Next().path).ToList();
            Check(queueSeen.Distinct().Count() == ReflexCustomMusic.Count, "the queue plays each song once per round (" + queueSeen.Distinct().Count() + " of " + ReflexCustomMusic.Count + ")");

            // OPTIONS > AUDIO
            var shell = OpenShell.Instance;
            if (shell != null) shell.ShowOptions(ReflexOptions.Audio); else ReflexOptions.Open(ReflexOptions.Audio);
            yield return UiShowcase.Frames(40);
            var root = ReflexOptions.Root ?? shell?.Root;
            var row = root?.Q("custom-music"); var folderBtn = root?.Q("music-folder"); var pop = root?.Q("now-playing");
            Check(row != null && folderBtn != null && pop != null, "OPTIONS > AUDIO has YOUR MUSIC, OPEN MUSIC FOLDER and NOW PLAYING POP-UP");
            if (row != null)
            {
                string Value() => row.Q<Label>(className: "mxa-value")?.text;
                Log("YOUR MUSIC row: " + Value());
                Check(Value() != null && Value().StartsWith("MIX IN") && Value().Contains(expected + " SONGS"), "the row says MIX IN and the song count");
                var sv = root.Q<ScrollView>(); if (sv != null) sv.ScrollTo(pop ?? row);
                row.Focus();
                yield return UiShowcase.Frames(10);
                yield return UiShowcase.Shot("custommusic-options");
                var it = row.userData as OptNavItem;
                it.adjust(1); Check(ReflexSettings.CustomMusic == 2 && Value().StartsWith("ONLY"), "right on YOUR MUSIC: ONLY");
                it.adjust(1); Check(ReflexSettings.CustomMusic == 0 && Value().StartsWith("OFF"), "right again: OFF");
                it.adjust(1); Check(ReflexSettings.CustomMusic == 1 && Value().StartsWith("MIX IN"), "and round to MIX IN");
                var pit = pop.userData as OptNavItem;
                pit.adjust(1); Check(!ReflexSettings.NowPlayingPopup, "NOW PLAYING POP-UP toggles off");
                pit.adjust(1); Check(ReflexSettings.NowPlayingPopup, "and on");
            }
            ReflexOptions.Close();
            yield return UiShowcase.Frames(20);

            // the pop-up
            var mine = Find("01 - mp3 v23.mp3") ?? Find("Test Artist - Sine Mid.wav");
            var game = new ReflexSong { title = "A Very Long Song Name That Does Not Fit On The Card At All, Part Two", artist = "Some Band With A Long Name", source = ReflexSong.SourceGame };
            ReflexNowPlaying.ShowNow(mine, false, false);
            yield return new WaitForSecondsRealtime(0.9f);
            Check(ReflexNowPlaying.Visible && ReflexNowPlaying.Text.Contains(mine.title), "the menus' strip shows the song (" + ReflexNowPlaying.Text + ")");
            yield return UiShowcase.Shot("custommusic-strip");
            UiShowcase.ShellVisible(false);   // (a race has no shell: the card is drawn over the world)
            ReflexNowPlaying.ShowNow(mine, false, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("custommusic-card");
            ReflexNowPlaying.ShowNow(game, false, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("custommusic-card-game");
            ReflexNowPlaying.ShowNow(mine, true, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("custommusic-card-skipped");
            UiShowcase.ShellVisible(true);
            // a race: HUD hidden hides it (H, photo mode, replays)
            ReflexSettings.HudHidden = true;
            ReflexNowPlaying.ShowNow(mine, false, true);
            yield return UiShowcase.Frames(20);
            Check(!ReflexNowPlaying.Visible, "in a race the pop-up stays away while the HUD is hidden");
            ReflexSettings.HudHidden = false;
            // the option off
            ReflexSettings.NowPlayingPopup = false;
            ReflexNowPlaying.Notify(mine, false);
            yield return UiShowcase.Frames(5);
            Check(!ReflexNowPlaying.Waiting, "NOW PLAYING POP-UP off: no pop-up is queued");
            ReflexSettings.NowPlayingPopup = true;
            yield return UiShowcase.Frames(3);
            // a fade-in: the pop-up waits until the song is audible
            for (float t = 0; t < 9f && ReflexNowPlaying.Visible; t += Time.unscaledDeltaTime) yield return null;
            float level = 0f;
            ReflexNowPlaying.Notify(game, false, () => level);
            yield return UiShowcase.Frames(10);
            Check(ReflexNowPlaying.Waiting && !ReflexNowPlaying.Visible, "a song in its fade-in is waited for (level 0)");
            level = 0.5f;
            yield return UiShowcase.Frames(15);
            Check(!ReflexNowPlaying.Waiting && ReflexNowPlaying.Visible, "and shown once it is audible (level 0.5)");
            for (float t = 0; t < 9f && ReflexNowPlaying.Visible; t += Time.unscaledDeltaTime) yield return null;
            Check(!ReflexNowPlaying.Visible, "the pop-up leaves again after its hold");

            // ONLY in the menus: FeUiMusic plays the songs
            var music = shell?.Sounds?.music;
            if (music == null) { Log("no UI music object in this run (no install?): the menu part is skipped"); yield break; }
            ReflexSettings.CustomMusic = 2; ReflexCustomMusic.SettingsChanged();
            music.SetEnable(false); music.SetEnable(true);   // (the way back from a race)
            for (float t = 0; t < 20f && music.CustomSong == null; t += Time.unscaledDeltaTime) yield return null;
            Check(music.CustomSong != null && music.IsPlaying, "ONLY: the menus play a song of Mods/Music (" + music.CustomSong?.Label + ")");
            for (float t = 0; t < 3f && !ReflexNowPlaying.Visible; t += Time.unscaledDeltaTime) yield return null;
            Check(ReflexNowPlaying.Visible && music.CustomSong != null && ReflexNowPlaying.Text.Contains(music.CustomSong.title), "and the strip names it (" + ReflexNowPlaying.Text + ")");
            yield return new WaitForSecondsRealtime(1.0f);
            yield return UiShowcase.Shot("custommusic-menu-only");
            var before = music.CustomSong;
            Check(ReflexCustomMusic.Skip(), "N: SKIP SONG is taken by the menus' music");
            for (float t = 0; t < 15f && (music.CustomSong == null || music.CustomSong == before); t += Time.unscaledDeltaTime) yield return null;
            Check(music.CustomSong != null && music.CustomSong != before, "and the next song plays (" + before?.Label + " -> " + music.CustomSong?.Label + ")");
            before = music.CustomSong;
            for (float t = 0; t < 12f && (music.CustomSong == null || music.CustomSong == before); t += Time.unscaledDeltaTime) yield return null;
            Check(music.CustomSong != null && music.CustomSong != before, "a song that plays out is followed by the next (" + before?.Label + " -> " + music.CustomSong?.Label + ")");
            ReflexSettings.CustomMusic = 1; ReflexCustomMusic.SettingsChanged();
            for (float t = 0; t < 3f && music.CustomSong != null; t += Time.unscaledDeltaTime) yield return null;
            Check(music.CustomSong == null, "MIX IN hands the menus back to the game's themes");
            ReflexSettings.CustomMusic = 1;
        }

        sealed class UiShowcaseCoroutineHost : MonoBehaviour { }
    }
}
