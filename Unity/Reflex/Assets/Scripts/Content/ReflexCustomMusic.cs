// YOUR MUSIC, the run-time part (ported from Open Unleashed's MxaSongLibrary / MxaMusic / MxaOptions; the pure part: ReflexSongLibrary): the player's own
// .mp3 / .ogg / .wav files in <client root>/Mods/Music (ReflexMods: next to OpenReflex.app / OpenReflex.exe; subfolders too), played through Unity's own
// decoders (UnityWebRequestMultimedia; no third-party decoder) in the game's place or among its tracks.
//   OPTIONS > AUDIO > YOUR MUSIC   OFF | MIX IN (default) | ONLY (ReflexSettings.CustomMusic)
//   races      OriginalMusic keeps the game's MusicPlayer cues (start delay, the activity's fade-in, the end-of-race fade-out, the pause duck, a track
//              that ends brings the next playlist entry). A playlist entry is where a song of the folder may stand in for the game's track:
//              MIX IN in proportion (PickForSlot: n songs against the playlist's tracks), ONLY always. The scenario scores stay the game's.
//   menus      MIX IN keeps the game's UI themes (as Open Unleashed keeps its menu theme); ONLY plays the songs one after another in their place
//              (FeUiMusic, with its 2.5 s fade-in, the music switch and the mute of the videos)
//   queue      ReflexSongLibrary.ShuffleQueue with a memory across runs (save key Music.Recent): what was heard lately comes last, no artist twice in a row
//   N          SKIP SONG (this class: the host below, Skip); the NOW PLAYING pop-up is Hud/ReflexNowPlaying
// The folder is scanned in the background (at start, when the window gets the focus back, and when the options page is built); until the first scan is
// done there are no songs and the game's soundtrack plays. A song that fails to load is skipped and logged (never silence for it).
// -reflexMusicDir <dir> (or REFLEX_MUSIC_DIR) reads another folder: tests.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Networking;

namespace Reflex.Port.Content
{
    public static class ReflexCustomMusic
    {
        public static readonly string[] ModeNames = { "OFF", "MIX IN", "ONLY" };
        public const string RecentKey = "Music.Recent";

        /// 0 OFF, 1 MIX IN, 2 ONLY (ReflexSettings.CustomMusic).
        public static int Mode => ReflexSettings.CustomMusic;
        /// The songs replace the game's soundtrack now (ONLY, with songs in the folder).
        public static bool Only => Mode == 2 && Count > 0;

        static string Arg(string name)
        {
            try { var a = Environment.GetCommandLineArgs(); int i = Array.IndexOf(a, name); if (i >= 0 && i + 1 < a.Length) return a[i + 1]; }
            catch (Exception) { }
            return null;
        }

        /// The music folder: Mods/Music beside the game (null without a client root); -reflexMusicDir / REFLEX_MUSIC_DIR for tests.
        public static string Folder
        {
            get
            {
                string o = Arg("-reflexMusicDir") ?? Environment.GetEnvironmentVariable("REFLEX_MUSIC_DIR");
                return !string.IsNullOrEmpty(o) ? o : ReflexMods.Folder(ReflexMods.MusicName);
            }
        }

        // ------------------------------------------------------------------------------------------------ the songs

        static Task<List<ReflexSong>> s_scan;
        static List<ReflexSong> s_songs = new List<ReflexSong>();
        static Dictionary<string, ReflexSong> s_byPath = new Dictionary<string, ReflexSong>(StringComparer.OrdinalIgnoreCase);
        static readonly Dictionary<string, ReflexSong> s_tags = new Dictionary<string, ReflexSong>();   // (the scan thread only)
        static float s_scanAt = -1000f;
        static bool s_scanned;
        /// Changes whenever a scan finds another set of songs (the queue is built again).
        public static int Version { get; private set; }

        /// The songs known now (empty until the first scan is done).
        public static IReadOnlyList<ReflexSong> Songs { get { Poll(); return s_songs; } }
        public static int Count => Songs.Count;
        /// The first scan has finished.
        public static bool Scanned { get { Poll(); return s_scanned; } }

        /// Scans the folder in the background (a scan under way is left to finish).
        public static void Rescan()
        {
            if (s_scan != null) return;
            string folder;
            try { folder = Folder; } catch (Exception) { return; }   // (Unity's paths: main thread)
            s_scanAt = Time.realtimeSinceStartup;
            s_scan = Task.Run(() => ReflexSongLibrary.Scan(folder, ReflexMods.IsJunk, s_tags));
        }

        /// Scans again unless a scan was made in the last minSeconds.
        public static void RescanIfOld(float minSeconds) { if (Time.realtimeSinceStartup - s_scanAt >= minSeconds) Rescan(); }

        static void Poll()
        {
            if (s_scan == null) { if (!s_scanned && ReflexSettings.CustomMusic != 0) Rescan(); return; }
            if (!s_scan.IsCompleted) return;
            var t = s_scan; s_scan = null;
            if (t.IsFaulted) { Debug.LogWarning("[Music] your music: the scan failed: " + t.Exception.GetBaseException().Message); s_scanned = true; return; }
            var found = t.Result;
            bool same = found.Count == s_songs.Count;
            for (int i = 0; same && i < found.Count; i++) same = string.Equals(found[i].path, s_songs[i].path, StringComparison.OrdinalIgnoreCase);
            if (!same || !s_scanned)
            {
                s_songs = found;
                s_byPath = new Dictionary<string, ReflexSong>(StringComparer.OrdinalIgnoreCase);
                foreach (var s in found) s_byPath[s.path] = s;
                Version++;
                Debug.Log("[Music] your music: " + found.Count + " song" + (found.Count == 1 ? "" : "s") + " in " + (Folder ?? "(no folder)"));
            }
            else s_songs = found;   // (tags may have been read again)
            s_scanned = true;
        }

        /// Opens Mods/Music in the desktop's file browser (made first, with its README, when missing).
        public static void OpenFolder()
        {
            string f = Folder; if (f == null) return;
            try { ReflexMods.EnsureLayout(); System.IO.Directory.CreateDirectory(f); } catch (Exception e) { Debug.LogWarning("[Music] " + f + ": " + e.Message); }
            Application.OpenURL(new Uri(f.EndsWith(System.IO.Path.DirectorySeparatorChar.ToString()) ? f : f + System.IO.Path.DirectorySeparatorChar).AbsoluteUri);
        }

        // ------------------------------------------------------------------------------------------------ the queue

        static List<string> s_queue;
        static int s_qi, s_queueVersion = -1;
        static string s_last;
        static readonly System.Random s_rnd = new System.Random();

        static List<string> Recent()
        {
            string v = ReflexSave.GetString(RecentKey, "");
            return v.Length == 0 ? new List<string>() : v.Split('\n').ToList();
        }

        /// A song of the folder began: the shuffle remembers it.
        public static void Heard(ReflexSong s)
        {
            if (s == null) return;
            var r = Recent();
            ReflexSongLibrary.Heard(r, s.Key);
            ReflexSave.SetString(RecentKey, string.Join("\n", r));
        }

        /// The next song of the shuffle (null: the folder has none).
        public static ReflexSong Next()
        {
            var songs = Songs;
            if (songs.Count == 0) return null;
            for (int round = 0; round < 2; round++)
            {
                if (s_queue == null || s_queueVersion != Version || s_qi >= s_queue.Count)
                {
                    s_queue = ReflexSongLibrary.ShuffleQueue(songs, Recent(), s_last, s_rnd);
                    s_qi = 0; s_queueVersion = Version;
                }
                while (s_qi < s_queue.Count)
                {
                    string p = s_queue[s_qi++];
                    if (s_byPath.TryGetValue(p, out var s)) { s_last = p; return s; }
                }
            }
            return null;
        }

        /// A playlist entry of the game's jukebox: the song that stands in for it, or null for the game's own track. MIX IN: a song with the chance
        /// songs / (songs + the playlist's tracks); ONLY: always one.
        public static ReflexSong PickForSlot(int gameTracks)
        {
            int mode = Mode, n = Count;
            if (mode == 0 || n == 0) return null;
            if (mode == 1 && s_rnd.Next(n + Math.Max(1, gameTracks)) >= n) return null;
            return Next();
        }

        static int s_failed;
        /// A song could not be played: counted (a run of failures hands the menu back to the game's themes).
        public static void Failed(ReflexSong s) { s_failed++; }
        public static void Played(ReflexSong s) { s_failed = 0; Heard(s); }
        public static bool Failing => s_failed >= 8;
        public static void ResetFailures() { s_failed = 0; }

        // ------------------------------------------------------------------------------------------------ playing

        static AudioType TypeOf(string path)
        {
            string ext = System.IO.Path.GetExtension(path).ToLowerInvariant();
            return ext == ".mp3" ? AudioType.MPEG : ext == ".ogg" ? AudioType.OGGVORBIS : AudioType.WAV;
        }

        /// Loads a song with Unity's decoder, streamed from its file as it plays (as the game's own tracks): no wait on the main thread (a song
        /// decoded in full when it loads froze the game ~0.5 s on every SKIP SONG, 2026-11-07), an exact length and place; done gets the clip,
        /// or null when it could not be played.
        public static IEnumerator Load(ReflexSong song, Action<AudioClip> done)
        {
            UnityWebRequest req = null;
            try
            {
                req = UnityWebRequestMultimedia.GetAudioClip(new Uri(song.path).AbsoluteUri, TypeOf(song.path));
                ((DownloadHandlerAudioClip)req.downloadHandler).streamAudio = true;
                req.SendWebRequest();
            }
            catch (Exception e)
            {
                Debug.LogWarning("[Music] your music: cannot open " + song.path + ": " + e.Message);
                req?.Dispose();
                done(null);
                yield break;
            }
            while (!req.isDone) yield return null;
            AudioClip clip = null;
            string err = req.error;
            if (req.result == UnityWebRequest.Result.Success)
            {
                try { clip = DownloadHandlerAudioClip.GetContent(req); } catch (Exception e) { err = e.Message; }
            }
            req.Dispose();
            if (clip && clip.loadState == AudioDataLoadState.Unloaded) clip.LoadAudioData();
            while (clip && clip.loadState == AudioDataLoadState.Loading) yield return null;
            if (clip && clip.loadState == AudioDataLoadState.Failed) { UnityEngine.Object.Destroy(clip); clip = null; err = "the decoder failed"; }
            if (clip && clip.length < 0.5f) { UnityEngine.Object.Destroy(clip); clip = null; err = "too short to be a song"; }
            if (!clip) Debug.LogWarning("[Music] your music: could not play " + song.path + " (" + (err ?? "not a song") + ")");
            else clip.name = song.Label;
            done(clip);
        }

        /// One song (or one of the game's tracks) on an AudioSource, with its own clock of how much of it was heard. Players' reports
        /// (2026-11-03 .. 11-07, Windows) showed songs "ending" in the middle (mostly as the pause menu opened or closed) and then a run of
        /// 5 .. 20 songs a fraction of a second each, or a song heard from near its end: the old watch trusted the source (its time, its
        /// isPlaying), which a paused, stolen or restarted voice does not keep. Here:
        ///   * a song starts at 0:00 (Stop, the clip, timeSamples 0, Play) and its place only moves forward with the music: a jump of the
        ///     source's place (more than the wall clock allows) is put back to where the song was;
        ///   * it is over only once it was heard to (nearly) its end by this clock; a source that stops before that plays on from where it
        ///     was (the focus coming back, a voice given back), and only one that will not play on after 4 tries counts as over.
        public sealed class SongDeck
        {
            public readonly AudioSource Source;
            public AudioClip Clip { get; private set; }
            public string Label { get; private set; }
            /// Seconds of the song heard (this clock).
            public float Heard => (float)heard;
            public bool Active => active && Clip;
            double heard; int last; float lastWall, quietSince = -1f; int resumes, jumps; bool active;
            public SongDeck(AudioSource source) { Source = source; }

            /// Plays clip from its start.
            public void Play(AudioClip clip, string label)
            {
                Clip = clip; Label = label ?? (clip ? clip.name : "");
                heard = 0; last = 0; resumes = 0; jumps = 0; quietSince = -1f; lastWall = Time.realtimeSinceStartup; active = clip;
                if (!Source) return;
                Source.Stop(); Source.loop = false; Source.clip = clip; Source.timeSamples = 0; Source.Play();
            }

            /// Leaves the song (the source is stopped; the clip is the caller's).
            public void Stop() { active = false; if (Source) Source.Stop(); }

            /// Call every frame the song should be playing: true once, when it was heard to its end (or would not play on).
            public bool Over()
            {
                if (!active || !Source || !Clip) return false;
                var s = Source;
                int rate = Mathf.Max(1, Clip.frequency), total = Clip.samples;
                double length = total / (double)rate;
                float wall = Time.realtimeSinceStartup, dt = Mathf.Max(0f, wall - lastWall); lastWall = wall;
                if (s.isPlaying)
                {
                    quietSince = -1f;
                    int now = s.timeSamples, d = now - last;
                    if (d >= -rate / 20 && d <= (int)((dt * 1.25f + 0.25f) * rate)) { if (d > 0) { heard += d / (double)rate; last = now; } return false; }
                    // the place jumped (a restarted or stolen voice, a clip that came back elsewhere): the song goes on where it was
                    int at = (int)Math.Min(total - 1, heard * rate);
                    if (jumps++ < 3) Debug.LogWarning($"[Music] {Label}: the player jumped from {last / (float)rate:F1} s to {now / (float)rate:F1} s: back to {at / (float)rate:F1} s");
                    s.timeSamples = at; last = at;
                    return false;
                }
                if (heard >= length - 1.0) { active = false; Debug.Log($"[Music] {Label}: played to its end ({length:F0} s)"); return true; }
                if (!s.ignoreListenerPause && AudioListener.pause) return false;   // (paused with the game: waits)
                if (quietSince < 0f) { quietSince = wall; return false; }
                if (wall - quietSince < 0.5f) return false;
                quietSince = -1f;
                if (resumes >= 4) { active = false; Debug.LogWarning($"[Music] {Label}: stopped at {heard:F1} of {length:F0} s and would not play on: the next song"); return true; }
                resumes++;
                int from = (int)Math.Min(total - 1, heard * rate);
                s.timeSamples = from; s.Play(); s.timeSamples = from; last = from;
                Debug.Log($"[Music] {Label}: stopped at {heard:F1} of {length:F0} s (not its end): plays on from there");
                return false;
            }
        }

        // ------------------------------------------------------------------------------------------------ SKIP SONG and the options

        static float s_skipAt = -10f;
        static readonly List<Func<bool>> s_skippers = new List<Func<bool>>();
        /// A music player that can skip (OriginalMusic in a race, FeUiMusic in the menus while it plays the songs); it returns whether it did.
        public static void AddSkipper(Func<bool> f) { if (f != null && !s_skippers.Contains(f)) s_skippers.Add(f); }
        public static void RemoveSkipper(Func<bool> f) { s_skippers.Remove(f); }
        /// SKIP SONG: the next song now, in the player that is playing.
        public static bool Skip()
        {
            float now = Time.realtimeSinceStartup;
            if (now - s_skipAt < 0.3f) return false;   // (a held or bouncing key: one skip)
            s_skipAt = now;
            Debug.Log("[Music] SKIP SONG");
            foreach (var f in s_skippers.ToArray())
                try { if (f()) return true; } catch (Exception e) { Debug.LogException(e); }
            return false;
        }

        /// YOUR MUSIC was changed (the options page): the menu music picks again.
        public static event Action Changed;
        public static void SettingsChanged()
        {
            ResetFailures();
            if (s_scan == null && !s_scanned) Rescan();
            try { Changed?.Invoke(); } catch (Exception e) { Debug.LogException(e); }
        }

        // ------------------------------------------------------------------------------------------------ the host

        sealed class Host : MonoBehaviour
        {
            void OnEnable() { Application.focusChanged += FocusChanged; }
            void OnDisable() { Application.focusChanged -= FocusChanged; }
            static void FocusChanged(bool focused) { if (focused && ReflexSettings.CustomMusic != 0) RescanIfOld(5f); }   // (back from the file browser)

            void Update()
            {
                Poll();
                var kb = Keyboard.current;
                if (kb == null || !kb.nKey.wasPressedThisFrame) return;
                if (OriginalOnboardAdjust.KeyboardCaptured || OriginalInput.GameBinds(Key.N) || OriginalInput.IsCameraKey(Key.N)) return;
                var shell = OpenShell.Instance;
                if (shell && shell.TextFocused) return;
                Skip();
            }
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            var go = new GameObject("Your music") { hideFlags = HideFlags.HideAndDontSave };
            UnityEngine.Object.DontDestroyOnLoad(go);
            go.AddComponent<Host>();
            if (ReflexSettings.CustomMusic != 0) Rescan();
        }
    }
}
