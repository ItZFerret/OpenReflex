using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Networking;

namespace Reflex.Port.OriginalWorld
{
    // The game's licensed in-race soundtrack (MUSIC_IN_GAME: the 24 MUS_INGAME_nn stream banks of the Music database, listed by
    // MusicAudioMap.bxml; _re/notes/game-audio.md). M toggles.
    //
    // The game's MusicPlayer (G + 0x15c6c; +8 its mode: 1 stopped, 2 jukebox, 3 UI, 4 scenario score) in a race (WP-F, traced in the
    // running world, _re/notes/audio-port.md section 7):
    // * the activity's audio logic gives the jukebox a list of track names (ActivityAudioLogic::vf35 -> 00677440): the playlist
    //   (+0x55c, count +0x560) is those tracks shuffled with the game's generator (00677220: 2 x count random swaps after swapping
    //   entry 0 with a random one, position +0x570 = random % count - 1); e.g. National on NAT_Concord: 13 of the 24 tracks;
    // * "InProgress Entered" (00582270; Free Ride 005822e0) starts the jukebox (00677700): mode 2, the activity (+0x1dc), the delay
    //   +0x1d8 = ActivityStartDelay (National 20 s, Supercross 30 s, Powercross 20 s, ...), and the next playlist entry requested
    //   (006771b0: position + 1, wrapping; +0x1d0); the delay counts down in the game's time (00676ae0), then the track is loaded
    //   (+0x1d4, its package) and MUSIC_IN_GAME/Music/Play started on it (00677920);
    // * the music channel ramp (00d67ab4 now, ab8 target, abc from, ac0 seconds): the jukebox's first track fades in from silence
    //   over ActivityTransitionTimes of the activity (National 25 s, Supercross 30 s, Terracross 15 s, Powercross / Freestyle /
    //   FreeRide 10 s; 00677920 when the channel is silent, which it is after the scenario score's stop); later tracks start at full
    //   volume; when a track's event ends the next entry of the playlist plays (006771b0);
    // * the race's end (ActivityAudioLogic vf16 / vf24 -> 00677600): the jukebox fades out over 1 s (mode 2; a scenario score: the
    //   second ActivityTransitionTimes value) and stops (flag 8 while fading).
    // The world runtime has no package loader, so the game's jukebox stops at the track's package load (the track record's state 1)
    // and never starts the event. This component therefore reads the game's MusicPlayer between the world's frames every 0.25 s and
    // plays what the game asked for: the requested track when its load starts (the game's delay, playlist and choice), the following
    // playlist entries when each track ends (as 006771b0 would), the activity's transition fade-in on the first track, the 1 s
    // fade-out when the game leaves the jukebox. Without a running original world (the test scenes, no plugin) it plays the tracks
    // itself as a shuffle after the activity's start delay.
    //
    // YOUR MUSIC (Content/ReflexCustomMusic.cs, OPTIONS > AUDIO): a playlist entry is where a song of Mods/Music may stand in for the game's track (MIX IN: in
    // proportion to the folder; ONLY: always). Everything above stays the game's: when the entry comes up, the fade-in over the activity's transition time on the
    // first one, the pause duck, the 1 s fade-out at the race's end, the next entry when the song has played out. The scenario scores stay the game's. N skips
    // to the next entry (Skip); the NOW PLAYING pop-up (Hud/ReflexNowPlaying) names each song once the fade-in has made it audible.
    //
    // Everything is read from the player's Reflex install at run time: the track list and titles from the BXML, each track's MPEG
    // stream is cut out of its FSB bank into a .mp3 in ReflexInstall.CacheDir the first time it is played and streamed by Unity's
    // MP3 decoder (the next track is prepared while the current one plays).
    //
    // The Music volume channel (MXRegistry.xml VolumeChannels, ReflexVolumeChannels): MasterVolume x MusicVolume (`volume`); it is
    // not PausedWithGame, so the pause menu (AudioListener.pause) does not stop it: it ducks to DuckedPercentageWhenPaused (50 %)
    // over DuckTime (0.5 s) and back, and the playlist goes on.
    [DisallowMultipleComponent]
    public sealed class OriginalMusic : MonoBehaviour
    {
        [Tooltip("Level the music plays in (NAT_Concord / FR_Alpine): selects the activity's start delay in MusicAudioMap.bxml.")]
        public string level = "";
        [Tooltip("MusicAudioMap activity name (National, FreeRide, ...). Empty: derived from the level.")]
        public string activity = "";
        public bool useActivityStartDelay = true;
        public AudioClip[] tracks = new AudioClip[0];     // optional offline override
        public string[] titles = new string[0];
        [Range(0, 1)] public float volume = 0.6f;
        public bool playing = true;
        [Tooltip("Follow the game's own MusicPlayer when an original world runs (its playlist, start delay, transitions).")]
        public bool followGame = true;
        /// The ride is in the front end's own world (the garage's TEST RIDE, the load arena: OriginalWorldRuntime.CreateOpen): the
        /// game's jukebox does not run there (FreeRideActivityAudioLogic::vf19 005822e0 skips it in the UI world, +0x234 +0x20018), so this
        /// plays no shuffle of its own; what the original plays there is the UI music or nothing (OwnTestRide, OpenShell).
        [NonSerialized] public bool uiWorld;

        AudioSource source;
        float duck = 1f; ReflexVolumeChannels.Channel channel;
        /// The pause duck factor now (1 = not ducked).
        public float Duck => duck;
        int[] order = new int[0]; int position = -1;
        Task<ReflexMusic.Info> infoTask; ReflexMusic.Info info;
        Task<string> nextFile; int nextTrack = -1;
        bool loading; float startAt;
        AudioClip current; string cacheDir;
        public string NowPlaying { get; private set; } = "";
        public float NowPlayingAge => source && source.isPlaying ? source.time : float.MaxValue;
        public string LoadError { get; private set; }
        public int TrackCount => tracks.Length > 0 ? tracks.Length : info?.tracks.Count ?? 0;
        public AudioClip CurrentClip => source ? source.clip : null;

        // ---- the game's MusicPlayer ----
        sealed class GameState { public int mode, flags, requested, current, position, activity = -1, score = -1; public float delay; public int[] playlist = new int[0]; public string[] banks = new string[0]; }
        OriginalWorldRuntime runtime; int runtimeSearches; float pollAt; volatile bool polling;
        readonly ConcurrentQueue<GameState> polled = new ConcurrentQueue<GameState>();
        GameState game; int[] gamePlaylist = new int[0]; string[] gameBanks = new string[0]; int gamePosition = -1;
        bool jukebox; int playingGameTrack = -1; float ramp = 1f, rampFrom, rampTo = 1f, rampTime, rampT = 1f; bool fadingOut;
        /// True once the game's MusicPlayer is read (a race with a running original world).
        public bool FollowingGame { get; private set; }
        /// The game's music channel ramp as this component models it (1 when not following the game).
        public float Ramp => FollowingGame ? ramp : 1f;
        /// The game's MusicPlayer mode last read (-1 none), the tracks it started here, the playlist (track indices) and its position.
        public int GameMode => game?.mode ?? -1;
        public readonly List<int> GameTracksPlayed = new List<int>();
        public int[] GamePlaylist => gamePlaylist;

        string LevelName => !string.IsNullOrEmpty(level) ? level : "";
        static string ActivityOf(string level) => ActivityOfStyled(TrackMods.Style(level));   // (a custom track: its kind's)
        static string ActivityOfStyled(string level) =>
            level.StartsWith("NAT_", StringComparison.Ordinal) ? "National" : level.StartsWith("FR_", StringComparison.Ordinal) ? "FreeRide" : "";
        string ActivityName
        {
            get
            {
                if (!string.IsNullOrEmpty(activity)) return activity;
                var sel = FindAnyObjectByType<ReflexEventSelect>(); string key = sel ? sel.Current : null;
                if (!string.IsNullOrEmpty(key) && key.IndexOf('/') > 0) return key.Substring(0, key.IndexOf('/'));
                return ActivityOf(LevelName);
            }
        }

        void Start()
        {
            source = gameObject.AddComponent<AudioSource>();
            source.spatialBlend = 0f; source.playOnAwake = false; source.loop = false; source.ignoreListenerPause = true; OriginalReverb.Route(source, false);
            source.priority = 0;   // (the music is never the voice a 12-rider race's engines take: 64 real voices, OriginalAudio.RealVoices)
            deck = new ReflexCustomMusic.SongDeck(source);
            try { channel = ReflexVolumeChannels.Shared[ReflexChannel.Music]; } catch (Exception) { }
            ReflexCustomMusic.AddSkipper(Skip);
            if (tracks != null && System.Linq.Enumerable.Any(tracks, t => t)) Shuffle(tracks.Length);     // offline override
            else
            {
                tracks = new AudioClip[0];       // serialized clips that no longer exist
                _ = ReflexInstall.Packages;      // resolves the install on the main thread
                cacheDir = System.IO.Path.Combine(ReflexInstall.CacheDir, "audio");
                infoTask = Task.Run(ReflexMusic.Load);
            }
        }

        void Shuffle(int n)
        {
            order = new int[n];
            for (int i = 0; i < n; i++) order[i] = i;
            var r = new System.Random();
            for (int i = n - 1; i > 0; i--) { int j = r.Next(i + 1); (order[i], order[j]) = (order[j], order[i]); }
        }

        void Update()
        {
            if (Keyboard.current != null && Keyboard.current.mKey.wasPressedThisFrame && !OriginalOnboardAdjust.KeyboardCaptured) { playing = !playing; if (!playing && source) source.Stop(); }
            if (!source) return;
            // pause ducking: DuckedPercentageWhenPaused over DuckTime (the original's channel ramp 0x8048f0)
            float duckTo = AudioListener.pause && channel != null && channel.duckWhenPaused > 0 ? 1f - channel.duckWhenPaused : 1f;
            float rate = channel != null && channel.duckTime > 0 ? Mathf.Max(channel.duckWhenPaused, 0.01f) / channel.duckTime : 1000f;
            duck = Mathf.MoveTowards(duck, duckTo, rate * Time.unscaledDeltaTime);
            if (rampT < 1f) { rampT = Mathf.Min(1f, rampT + Time.unscaledDeltaTime / Mathf.Max(0.001f, rampTime)); ramp = Mathf.Lerp(rampFrom, rampTo, rampT); }
            source.volume = volume * (channel != null ? channel.master : 1f) * duck * Ramp;
            if (infoTask != null && infoTask.IsCompleted)
            {
                var t = infoTask; infoTask = null;
                if (t.IsFaulted) { LoadError = t.Exception.GetBaseException().Message; Debug.LogWarning("Original music not loaded from the Reflex install: " + LoadError); }
                else
                {
                    info = t.Result; Shuffle(info.tracks.Count);
                    string act = string.IsNullOrEmpty(activity) ? ActivityOf(LevelName) : activity;
                    startAt = Time.time + (useActivityStartDelay && info.startDelay.TryGetValue(act, out float d) ? d : 0f);
                    if (order.Length > 0 && !FollowingGame) Prepare(order[0]);
                }
            }
            FindGame();
            if (FollowingGame) { FollowGame(); return; }
            if (uiWorld) { if (source.isPlaying) source.Stop(); return; }   // the game's jukebox never runs in the front end's world (no shuffle of our own)
            if (order.Length == 0 || loading || !playing || source.isPlaying || Time.time < startAt) return;
            // (w-fb-hud) the launch's shuffle bag of the whole list (ReflexShuffleBag: no repeat before every track played, across races)
            int tr = ReflexMusicBags.Get(ReflexMusicBags.RaceOffline).Next(order.Length);
            if (tr < 0) return;
            position = tr;
            if (tracks.Length > 0)
            {
                source.clip = tracks[tr]; source.Play();
                NowPlaying = tr < titles.Length ? titles[tr] : (tracks[tr] ? tracks[tr].name : "");
            }
            else StartCoroutine(PlayRuntime(tr));
        }

        // ------------------------------------------------------------------------------------ following the game's MusicPlayer
        void FindGame()
        {
            if (!followGame || FollowingGame || tracks.Length > 0 || runtimeSearches > 2400) return;
            runtimeSearches++;
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime || runtime.Simulation == null) return;
            FollowingGame = true; ramp = 0f; rampT = 1f; rampTo = 0f;
            if (source) source.Stop();
            Debug.Log("[Music] following the game's MusicPlayer");
        }

        void Poll()
        {
            if (polling || Time.unscaledTime - pollAt < 0.25f || !runtime || runtime.Simulation == null) return;
            pollAt = Time.unscaledTime; polling = true;
            runtime.Simulation.BetweenFrames(w =>
            {
                var b = new byte[32];
                uint U(uint a) => ReflexWorldNative.reflex_world_read(w, a, b, 4) == 0 ? BitConverter.ToUInt32(b, 0) : 0;
                var s = new GameState();
                try
                {
                    uint G = U(0xa946b8), P = G + 0x15c6c, count = U(0xc01c10), records = U(0xc01c0c);
                    s.mode = (int)U(P + 8); s.flags = (int)U(P + 4); s.requested = (int)U(P + 0x1d0); s.current = (int)U(P + 0x1d4); s.position = (int)U(P + 0x570);
                    s.delay = BitConverter.ToSingle(BitConverter.GetBytes(U(P + 0x1d8)), 0); s.activity = (int)U(P + 0x1dc);
                    uint list = U(P + 0x55c), n = Math.Min(U(P + 0x560), 64u);
                    s.playlist = new int[n]; for (uint i = 0; i < n; i++) s.playlist[i] = (int)U(list + 4 * i);
                    s.banks = new string[Math.Min(count, 64u)];
                    for (uint i = 0; i < s.banks.Length; i++)
                    {   // the track record: +0x00 package, +0x19 BankName (0x19 bytes)
                        uint rec = U(records + 4 * i);
                        if (rec != 0 && ReflexWorldNative.reflex_world_read(w, rec + 0x19, b, 25) == 0) { int z = Array.IndexOf(b, (byte)0); s.banks[i] = System.Text.Encoding.ASCII.GetString(b, 0, z < 0 || z > 25 ? 25 : z); }
                    }
                }
                catch (Exception) { s = null; }
                if (s != null) polled.Enqueue(s);
                polling = false;
            });
        }

        // ------------------------------------------------------------------------------------ the open path (librxcore, w-sfx)
        // The core runs the MusicPlayer's game state (core/game/music_player.c: the playlist shuffled with the game's generator, the
        // jukebox's start delay, the scenario score, the fades) and reports it (rx_session_sound music); this component plays it the
        // way it follows the legacy world's memory, plus the scenario score (MUSIC_SCORES/Music/Play on the MUS_SCORE_nn bank of the
        // requested score: 00676d20 / 00677d50; from silence over the ScenarioMusic transition's first value, out over its second).
        bool openPath; uint openSerial = uint.MaxValue; int scorePlaying = -1; bool scoreFading;
        public int ScorePlaying => scorePlaying;
        public void ObserveOpen(Reflex.Port.RxCore.RxSoundState s)
        {
            if (s == null || !s.musicValid || tracks.Length > 0) return;
            if (!FollowingGame)
            {
                FollowingGame = true; openPath = true; ramp = 0f; rampT = 1f; rampTo = 0f;
                if (source) source.Stop();
                Debug.Log("[Music] following the core's MusicPlayer (open path)");
            }
            if (s.musicSerial == openSerial) return;
            openSerial = s.musicSerial;
            var g = new GameState { mode = s.musicMode, flags = (int)s.musicFlags, requested = s.musicRequested, current = s.musicCurrent, position = s.musicPosition,
                                    activity = s.musicKind, delay = s.musicDelay, score = s.scoreCurrent };
            int n = Math.Max(0, Math.Min(s.musicCount, s.playlist.Length));
            g.playlist = new int[n]; Array.Copy(s.playlist, g.playlist, n);
            g.banks = info != null ? info.trackBanks.ToArray() : new string[0];
            polled.Enqueue(g);
        }
        void ObserveScore(GameState s)
        {
            if (info == null) return;
            string act = s.activity >= 0 && s.activity < ActivityNames.Length ? ActivityNames[s.activity] : ActivityName;
            if (s.mode == 4 && s.score >= 0 && s.score != scorePlaying && (s.flags & 8) == 0 && s.score < info.scores.Count && info.scores[s.score] != null)
            {
                scorePlaying = s.score; scoreFading = false; jukebox = false; fadingOut = false;
                RampTo(1f, ramp < 0.001f && info.scoreFadeIn.TryGetValue(act, out float fin) && fin > 0 ? fin : 0.001f);
                Debug.Log($"[Music] the game's score {s.score} ({info.scores[s.score].bank}), fade-in {rampTime:F1} s ({act})");
                StartCoroutine(PlayScore(info.scores[s.score]));
            }
            else if (scorePlaying >= 0 && !scoreFading && (s.mode != 4 || (s.flags & 8) != 0))
            {
                scoreFading = true;
                float fout = s.mode == 4 && info.scoreFadeOut.TryGetValue(act, out float fo) && fo > 0 ? fo : 1f;
                if ((s.flags & 8) != 0) RampTo(0f, fout); else { RampTo(0f, 0.001f); if (source) source.Stop(); }
                Debug.Log($"[Music] the game ends the score (mode {s.mode}, flags {s.flags:x}): fade-out {fout:F1} s");
            }
            if (scoreFading && s.mode != 4) { scorePlaying = -1; scoreFading = false; }
        }
        IEnumerator PlayScore(ReflexMusicTrack t)
        {
            loading = true;
            var file = Task.Run(() => ReflexMusic.Extract(t, cacheDir));
            while (!file.IsCompleted) yield return null;
            if (file.IsFaulted) { LoadError = file.Exception.GetBaseException().Message; Debug.LogWarning("Original music score failed: " + LoadError); loading = false; yield break; }
            using (var req = UnityWebRequestMultimedia.GetAudioClip(new Uri(file.Result).AbsoluteUri, AudioType.MPEG))
            {
                ((DownloadHandlerAudioClip)req.downloadHandler).streamAudio = true;
                yield return req.SendWebRequest();
                if (req.result == UnityWebRequest.Result.Success && scorePlaying >= 0 && !scoreFading)
                {
                    var clip = DownloadHandlerAudioClip.GetContent(req); clip.name = t.title;
                    var old = current; current = clip;
                    if (source) { deck.Stop(); source.clip = clip; source.timeSamples = 0; source.Play(); NowPlaying = t.title; playedOnce = false; customNow = null; }
                    if (old) Destroy(old);
                }
            }
            loading = false;
        }

        void FollowGame()
        {
            if (!runtime && !openPath) { FollowingGame = false; return; }
            Poll();
            while (polled.TryDequeue(out var s)) Observe(s);
            if (info == null) return;
            if (!playing) { if (source.isPlaying) source.mute = true; return; }
            source.mute = false;
            if (!jukebox || fadingOut || loading) return;
            // a track played out: the next entry of the game's playlist (006771b0: position + 1, wrapping)
            bool ended = deck.Over();   // (heard to its end by the deck's own clock: a pause, a focus blip or a stolen voice is not the end)
            if (playingGameTrack >= 0 && source.clip && ended && playedOnce)
            {
                playedOnce = false; skipping = false;
                if (gamePlaylist.Length > 0)
                {
                    gamePosition = (gamePosition + 1) % gamePlaylist.Length;
                    PlayGame(gamePlaylist[gamePosition], false);
                }
            }
        }
        bool playedOnce;

        // ---- YOUR MUSIC
        ReflexSong customNow; ReflexCustomMusic.SongDeck deck; bool skipping;
        /// The song of Mods/Music playing now (null: the game's own track or nothing).
        public ReflexSong CustomNow => customNow;

        static ReflexSong GameSong(ReflexMusicTrack t) => new ReflexSong { path = t.bank, title = string.IsNullOrWhiteSpace(t.song) ? t.title : t.song, artist = t.artist, source = ReflexSong.SourceGame };

        /// SKIP SONG (N): the next playlist entry now (the game's playlist while its jukebox runs, else the shuffle). False when nothing plays.
        public bool Skip()
        {
            if (!source || !playing || loading || !source.clip) return false;
            if (FollowingGame)
            {
                if (!jukebox || fadingOut || info == null || gamePlaylist.Length == 0) return false;
                skipping = true;
                gamePosition = (gamePosition + 1) % gamePlaylist.Length;
                PlayGame(gamePlaylist[gamePosition], false);
                return true;
            }
            if (order.Length == 0 || !source.isPlaying) return false;
            skipping = true; startAt = 0f; deck.Stop();   // Update starts the next entry
            return true;
        }

        void Observe(GameState s)
        {
            var prev = game; game = s;
            if (openPath) ObserveScore(s);
            if (s.playlist.Length > 0) gamePlaylist = s.playlist;
            if (s.banks.Length > 0) gameBanks = s.banks;
            if (!jukebox)
            {
                // the jukebox's first track: requested, its delay over, its package load begun (+0x1d4 set). Again after an ended jukebox (the
                // next moto of a series event, a restart): only once the new delay ran out (+0x1d8; a +0x1d4 the stop left is not the new track)
                if (s.mode == 2 && s.current >= 0 && (s.flags & 8) == 0 && (!fadingOut || s.delay <= 0f))
                {
                    jukebox = true; fadingOut = false; gamePosition = s.position; scorePlaying = -1; scoreFading = false;
                    PlayGame(s.current, true);
                }
                return;
            }
            // the game left the jukebox (the race's end: 00677600 fades it out over 1 s, flag 8; or another mode)
            if (!fadingOut && (s.mode != 2 || (s.flags & 8) != 0))
            {
                fadingOut = true;
                RampTo(0f, 1f);
                Debug.Log($"[Music] the game ends the jukebox (mode {s.mode}, flags {s.flags:x}): fade-out 1 s");
            }
            // ... and stopped it (the moto's Post Entered 005848a0 -> 00677510: mode 1): the next InProgress Entered (00582270 -> 00677700) starts a
            // new jukebox in the same world. A Moto event's later motos have no race intro, so no scenario score (mode 4, ObserveScore) comes in
            // between: this is the only way back for moto 2 on (the fade-out keeps ramping meanwhile)
            if (fadingOut && s.mode != 2) jukebox = false;
        }

        static readonly string[] ActivityNames = { "National", "Supercross", "Opencross", "Supermoto", "Minimoto", "Freestyle", "WaypointRace", "Endurocross",
            "FreeRide", "Tag", "Snake", "Graffiti", "SectionRace", "PokerRace", "Infection", "EggHunt", "Capture", "Terracross", "UTVMoto", "Powercross",
            "FMXJam", "BigAir", "HillClimb", "Jumper", "Pathfinder", "StepUp", "Stoppie", "Trailblazer", "HitDiscovery", "TargetPractice", "Wheelie" };

        void RampTo(float to, float seconds) { rampFrom = ramp; rampTo = to; rampTime = seconds; rampT = seconds <= 0.001f ? 1f : 0f; if (rampT >= 1f) ramp = to; }

        // the install's track (info.tracks) of a game track index (its bank), -1 none
        int TrackOf(int gameIndex)
        {
            string bank = gameIndex >= 0 && gameIndex < gameBanks.Length ? gameBanks[gameIndex] : null;
            int t = bank != null ? info.tracks.FindIndex(x => string.Equals(x.bank, bank, StringComparison.OrdinalIgnoreCase)) : -1;
            if (t < 0 && gameIndex >= 0 && gameIndex < info.tracks.Count) t = gameIndex;
            return t;
        }
        /// The tracks this launch played in races, in order (info.tracks indices; w-fb-hud's shuffle bag), for tests.
        public readonly List<int> TracksPlayed = new List<int>();

        void PlayGame(int gameIndex, bool first)
        {
            int requested = TrackOf(gameIndex);
            if (requested < 0) return;
            // (w-fb-hud) WHICH track: the launch's race bag over the whole soundtrack, from the tracks of the game's playlist (the level's); the
            // game's own pick (its generator restarts with every race session: the same songs in the same order) only says WHEN one starts
            var allowed = new HashSet<int>();
            foreach (int g in gamePlaylist) { int a = TrackOf(g); if (a >= 0) allowed.Add(a); }
            allowed.Add(requested);
            int t = ReflexMusicBags.Get(ReflexMusicBags.Race).Next(info.tracks.Count, allowed);
            if (t < 0) t = requested;
            TracksPlayed.Add(t);
            string bank = info.tracks[t].bank;
            playingGameTrack = gameIndex; GameTracksPlayed.Add(gameIndex);
            // 00677920: the channel silent (the first jukebox track) -> fade in over ActivityTransitionTimes[+0x1dc], the activity's
            // engine enum (its names: the table at 009d8428, MusicAudioMap's tag order)
            int ai = game?.activity ?? -1;
            string act = ai >= 0 && ai < ActivityNames.Length ? ActivityNames[ai] : ActivityName;
            if (first) RampTo(1f, info.transitionTime.TryGetValue(act, out float tt) && tt > 0 ? tt : 0.001f);
            Debug.Log($"[Music] the game's entry {gameIndex} (track {requested}) -> the bag's track {t} ({bank}) {info.tracks[t].title}{(first ? $", fade-in {rampTime:F0} s ({act})" : "")}, playlist [{string.Join(",", gamePlaylist)}] position {gamePosition}, {allowed.Count} allowed");
            StartCoroutine(PlayRuntime(t));
        }

        void Prepare(int track)
        {
            if (info == null || track < 0 || track >= info.tracks.Count || nextTrack == track) return;
            var t = info.tracks[track];
            nextTrack = track; nextFile = Task.Run(() => ReflexMusic.Extract(t, cacheDir));
        }

        IEnumerator PlayRuntime(int track)
        {
            loading = true;
            // YOUR MUSIC: a song of Mods/Music stands in for this entry (the cues around it stay the game's); one that cannot be played gives way to the game's track
            var mine = ReflexCustomMusic.PickForSlot(FollowingGame && gamePlaylist.Length > 0 ? gamePlaylist.Length : info != null ? info.tracks.Count : 0);
            if (mine != null)
            {
                AudioClip mineClip = null;
                yield return StartCoroutine(ReflexCustomMusic.Load(mine, c => mineClip = c));
                if (mineClip)
                {
                    var prev = current; current = mineClip;
                    if ((playing || FollowingGame) && source && !(FollowingGame && fadingOut))
                    {
                        deck.Play(mineClip, mine.Label); NowPlaying = mine.Label; playedOnce = true;
                        customNow = mine; ReflexCustomMusic.Played(mine);
                        Debug.Log($"[Music] your music: {mine.Label} ({mineClip.length:F0} s) in place of the game's track {track}");
                        ReflexNowPlaying.Notify(mine, skipping, () => Ramp); skipping = false;
                    }
                    if (prev) Destroy(prev);
                    loading = false;
                    yield break;
                }
                ReflexCustomMusic.Failed(mine);
            }
            if (nextTrack != track) Prepare(track);
            var file = nextFile;
            while (!file.IsCompleted) yield return null;
            if (file.IsFaulted) { LoadError = file.Exception.GetBaseException().Message; Debug.LogWarning("Original music track failed: " + LoadError); nextTrack = -1; loading = false; startAt = Time.time + 5f; yield break; }
            using (var req = UnityWebRequestMultimedia.GetAudioClip(new Uri(file.Result).AbsoluteUri, AudioType.MPEG))
            {
                ((DownloadHandlerAudioClip)req.downloadHandler).streamAudio = true;
                yield return req.SendWebRequest();
                if (req.result != UnityWebRequest.Result.Success) { LoadError = req.error; Debug.LogWarning("Original music track failed: " + req.error); nextTrack = -1; loading = false; startAt = Time.time + 5f; yield break; }
                var clip = DownloadHandlerAudioClip.GetContent(req);
                var t = info.tracks[track];
                clip.name = t.title;
                var old = current; current = clip;
                if ((playing || FollowingGame) && source && !(FollowingGame && fadingOut))
                {
                    deck.Play(clip, t.title); NowPlaying = t.title; playedOnce = true; customNow = null;
                    ReflexNowPlaying.Notify(GameSong(t), skipping, () => Ramp); skipping = false;
                }
                if (old) Destroy(old);
            }
            nextTrack = -1;
            if (FollowingGame) { if (gamePlaylist.Length > 1) { int nx = gamePlaylist[(gamePosition + 1 + gamePlaylist.Length) % gamePlaylist.Length]; string bk = nx >= 0 && nx < gameBanks.Length ? gameBanks[nx] : null; int ti = bk != null ? info.tracks.FindIndex(x => string.Equals(x.bank, bk, StringComparison.OrdinalIgnoreCase)) : nx; Prepare(ti); } }
            else if (order.Length > 1) Prepare(order[(position + 1) % order.Length]);     // cut the next track out of its bank while this one plays
            loading = false;
        }

        void OnDestroy() { ReflexCustomMusic.RemoveSkipper(Skip); if (current) Destroy(current); Reflex.Port.Hud.ReflexNowPlaying.ForgetGate(this); }
    }
}
