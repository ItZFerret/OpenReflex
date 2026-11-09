using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.Networking;

namespace Reflex.Port.OriginalWorld
{
    // All of the game's sound, played the way its FMOD event system would (_re/notes/game-audio.md,
    // engine-audio.md). The running original game decides what plays: every FMOD event command it
    // issues (play / stop / parameter / 3D position / volume / mute / pause, reflex_world_audio) is
    // handled here against the level's event library (_re/formats/unity_audio_library.py, built from
    // the game's own FEV/FSB data): layers and sound instances over their control parameter,
    // volume / pitch / DSP / spawn envelopes, sound-definition play modes and spawning, 2D or
    // linear-rolloff 3D. When an event ends by itself, the game is told (EVENT_FINISHED), as FMOD does.
    //
    // Nothing of the game is stored in the project: given only `level` ("NAT_Concord" / "FR_Alpine") the library is built at run
    // time on a worker thread from the player's own install (ReflexAudioSet: the level's FEV projects + the global ones, parsed by
    // ReflexFmod) and the waves are decoded from the FSB banks as they are needed (MPEG streams are cached as .mp3 files in
    // ReflexInstall.CacheDir and decoded by Unity's MP3 decoder; PCM/IMA samples are decoded in C#). Commands that arrive before
    // the library is ready are queued and replayed in order; a wave whose clip is not decoded yet plays as a silent null entry.
    // The serialized `library` / `clipFiles` / `clips` fields are an optional offline override (old scenes) and win when set.
    //
    // Which projects a race loads, and the mix, as the original (WP-1, _re/notes/audio-port.md):
    //   * the library is ReflexAudioSet.Build(Race): the level's projects, the foley, the level's crowd type, the engine of every
    //     size the fielded vehicle types have, the event type's ACTIVITY_* + announcer <VO>_<LANG> projects (activity from
    //     ReflexEventSelect's event, language from ReflexUiData). Projects added later (UseRoster: the engine of every vehicle in
    //     the race, e.g. a car's RHINO_ENGINE_02; AddProjects) are built on a worker thread and merged; a play command of a project
    //     that is not loaded but exists in the global audio databases loads it on demand (logged: that event itself stays silent).
    //   * volume channels (MXRegistry.xml VolumeChannels, ReflexVolumeChannels): each event's channel gain = channel MasterVolume x
    //     the user's setting (EffectsVolume = masterVolume for Effects / UI / Crowd / DriveOn / Skid / Scenario / VoiceOver;
    //     EngineVolume = engineVolume for Engine, the stored ReflexUI.EngineVolume / 10). Pause (AudioListener.pause): the
    //     PausedWithGame channels pause, UI and VoiceOver keep playing (ignoreListenerPause), Music ducks (OriginalMusic).
    //   * max playbacks per event (FEV header): when an event's limit is reached the behaviour steals the oldest / newest /
    //     quietest instance (its EVENT_FINISHED is sent) or fails the new one (finished at once).
    //   * 3D occlusion (sub-command 10: direct occlusion, the engine under water): FMOD_INIT_OCCLUSION_LOWPASS (EventSystem::init(500, 8) at
    //     0x804490) low-passes the event at 22050 Hz x (1 - direct)^2 and does not attenuate it (ChannelSoftware 10076766); the reverb occlusion
    //     (the second value) is not applied to the wet signal.
    //   * reverb (w-reverb, OriginalReverb): the level's ambient reverb, its 3D reverb zones and the first-person camera's reverb, morphed as
    //     FMOD Ex does, carried by one AudioReverbZone at the listener; 3D events are sent to it (2D events stay dry).
    //   * voices: 64 real / 512 virtual (the game's System::setSoftwareChannels(64) at 0x80440f and init(500)), set once at start.
    //   * doppler stays off: the game never passes an event velocity (cmd+0x3c is 0 on every play / 3D command) and its listener
    //     velocity block (0x804b62) is not taken, so FMOD's doppler (DopplerFactor 1.0) has nothing to shift.
    [DisallowMultipleComponent]
    public sealed class OriginalAudio : MonoBehaviour
    {
        // ---- library (JsonUtility) ----
        [Serializable] public sealed class Library { public string level; public EventDef[] events; public string[] files; }
        [Serializable] public sealed class EventDef
        {
            public string path; public bool is3d; public float minDistance, maxDistance, volume, pitchOctaves, pitchRandOctaves, volumeRand;
            public int fadeInMs, fadeOutMs; public ParamDef[] parameters; public LayerDef[] layers; public SoundDef[] defs;
            // not in the python reference library (JsonUtility skips them): ReflexChannel, FEV max playbacks / behaviour / priority
            [NonSerialized] public int channel, maxPlaybacks, maxPlaybacksBehavior, priority = 128;
        }
        [Serializable] public sealed class ParamDef { public string name; public float min, max; }
        [Serializable] public sealed class LayerDef { public int param; public InstanceDef[] instances; public EnvelopeDef[] envelopes; }
        [Serializable] public sealed class InstanceDef { public int def; public float start, length, gain, fadeIn, fadeOut; public bool loop; }
        [Serializable] public sealed class EnvelopeDef { public string kind; public int param; public bool log; public float dspMin, dspMax; public float[] x, y; public int[] shape; }
        [Serializable] public sealed class SoundDef
        {
            public string name, mode; public float volume, volumeRandMin, pitchOctaves, pitchRandOctaves;
            public int maxSpawned, spawnMinMs, spawnMaxMs; public string[] files; public float[] weights;
        }

        [Tooltip("Level whose FMOD projects to load at run time from the Reflex install: NAT_Concord or FR_Alpine.")]
        public string level = "";
        [Tooltip("Engine ActivityName of the event (National, Supercross, WaypointRace, FreeRide, Terracross ...): its ACTIVITY_* and announcer projects. Empty: ReflexEventSelect's event, else from the level's prefix.")]
        public string activity = "";
        [Tooltip("UI language (ENG, FRE, GER, ITA, SPA). Empty: ReflexUiData's. The voice is ENG or FRE.")]
        public string language = "";
        public TextAsset library;
        public string[] clipFiles = new string[0];
        public AudioClip[] clips = new AudioClip[0];
        [Tooltip("EffectsVolume (ReflexAudio.master): the Effects / UI / Crowd / DriveOn / Skid / Scenario / VoiceOver channels.")]
        [Range(0, 2)] public float masterVolume = 1f;
        [Tooltip("EngineVolume (ReflexUI.EngineVolume / 10): the Engine channel. Followed from the settings storage while followSettings.")]
        [Range(0, 1)] public float engineVolume = 1f;
        public bool followSettings = true;

        public readonly ConcurrentQueue<(uint wrapper, uint serial)> Finished = new ConcurrentQueue<(uint, uint)>();
        public int ActiveEvents { get; private set; }
        public int ActiveVoices { get; private set; }
        /// The event library is built (commands are processed; clips may still be decoding).
        public bool LibraryReady => lib != null;
        public int ClipsLoaded => clipByFile.Count;
        public int ClipsTotal { get; private set; }
        /// Every wave of the library has been decoded (or failed).
        public bool ClipsReady { get; private set; }
        public string LoadError { get; private set; }
        public IReadOnlyDictionary<string, AudioClip> Clips => clipByFile;
        public Library CurrentLibrary => lib;
        public float LibrarySeconds { get; private set; }
        /// The race spec of the run-time library (null for an offline library).
        public ReflexAudioSet.Race Spec { get; private set; }
        /// The FEV projects in the library (main build + merged extensions).
        public IReadOnlyCollection<string> Projects => loadedProjects;
        /// Per event path (lower case): what the game asked for and what played (diagnostics, AudioContentTests.Race).
        public sealed class EventStat { public int starts, unknown, voices, nullVoices, stolen, failed, stoppedBeforePlay; public ReflexChannel channel; }
        public readonly Dictionary<string, EventStat> Stats = new Dictionary<string, EventStat>();
        /// Projects loaded on demand because the game played one of their events before they were in the library.
        public readonly List<string> OnDemand = new List<string>();
        public int PeakVoices { get; private set; }
        /// Sub-command 10 (set3DOcclusion) commands received: the plugin forwards them (runtime.cpp AUDIO_EXECUTE).
        public int OcclusionCommands { get; private set; }

        Library lib;
        /// The level's reverb (w-reverb): ambient, zones, the first-person camera's.
        public OriginalReverb Reverb { get; private set; }
        readonly Dictionary<string, EventDef> events = new Dictionary<string, EventDef>();
        readonly Dictionary<string, AudioClip> clipByFile = new Dictionary<string, AudioClip>();
        readonly Dictionary<uint, Instance> instances = new Dictionary<uint, Instance>();
        readonly Dictionary<string, int> sequential = new Dictionary<string, int>();   // play mode "sequential" (persists across starts)
        readonly System.Random random = new System.Random(7);
        readonly Stack<Voice> pool = new Stack<Voice>();

        sealed class Voice
        {
            public AudioSource source; public AudioLowPassFilter lowpass; public AudioHighPassFilter highpass; public AudioDistortionFilter distortion;
            public float gain = 1f, pitch = 1f; public bool started, isNull, paused; public float nullUntil;
        }
        sealed class SlotState   // one sound instance of a layer
        {
            public bool active, fired; public int seq; public readonly List<int> shuffle = new List<int>(); public int lastPick = -1;
            public readonly List<Voice> voices = new List<Voice>(); public float spawnTimer;
        }
        sealed class Instance
        {
            public EventDef def; public uint wrapper, serial; public bool playing, stopping, muted, paused;
            public float[] param; public Vector3 position; public GameObject node; public float volumeCmd = 1f, gain = 1f, pitch = 1f, fade = 1f, fadeRate;
            public SlotState[][] slots;
            public float occlusion, startedAt; public EventStat stat; public bool virtualized, ticked;
            public bool keyedOff;                                   // Event::stop(false) with no fade-out (w-landclank): loops ended, the one-shot samples play out
            public float virtGain = 1f;                             // the max-playbacks ranking's gain, ramped (w-landclank: no click when a voice drops in or out of the loudest set)
            public float minDistance = -1f, maxDistance = -1f;   // Event::set3DMinMaxDistance (host sub 6), -1 = the event's own
            public int channelOverride = -1;                       // AudioEvent_SetChannel (host sub 13, 0080bce0: the wrapper's +0x1a0), -1 = by project
            public int selKind = -1;                               // the owner's SOUNDDEF_SELECTINDEX callback (host sub 14; wrapper +0x19c): the crowd emitters' 006743b0 kind 0 .. 3, -1 none
            public int selIndex = -1;                              // the last programmer-selected index (wrapper +0x124), -1 after a start / stop (0080a810, 0080a890, 0080a8e0)
        }

        Task<ReflexAudioSet> build; float buildStart; string cacheDir;
        readonly Queue<ReflexAudioCommand> pending = new Queue<ReflexAudioCommand>();
        bool loadStarted;

        void Awake()
        {
            if (PlayerPrefs.HasKey("ReflexAudio.master")) masterVolume = PlayerPrefs.GetFloat("ReflexAudio.master", masterVolume);   // EffectsVolume
            Reverb = new OriginalReverb(transform);
            ReadSettings();
            Load();
        }

        // ---- the game's volume channels and the user's settings (ReflexVolumeChannels) ----
        ReflexVolumeChannels channels; float settingsAt = -1f;
        readonly float[] channelGain = new float[10];
        readonly float[] levelMix = { 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f };
        /// The level volume mix the game wrote into its channel records (rx_session_sound channel_mix, w-sfx): AudioVolumeMixEntity
        /// 0067d810 sets record + 0 of each channel to the level's VolumeMix group of the player count ((players - 1) / 3), and every event's
        /// volume is the user setting x this x the channel ramp x its own (FUN_0080b490, the event's own channel only). NAT_Concord: Effects
        /// 0.6, Engine 0.7, Crowd 0.4, DriveOn 0.65, Skid 0.75, Scenario 0.7. 1 where the core reported none (front end, legacy path).
        public void SetLevelMix(float[] mix, int count)
        {
            for (int c = 0; c < levelMix.Length; c++)
            {
                float v = mix != null && c < count && c < mix.Length ? mix[c] : 1f;
                levelMix[c] = float.IsNaN(v) || float.IsInfinity(v) ? 1f : Mathf.Clamp01(v);
            }
        }
        public float LevelMix(ReflexChannel c) => levelMix[(int)c];
        void ReadSettings()
        {
            if (followSettings) engineVolume = Mathf.Clamp01(OriginalPauseMenu.EngineVolume);    // ReflexUI.EngineVolume 0..10
            settingsAt = Time.unscaledTime;
        }
        void UpdateChannelGains()
        {
            if (channels == null) { try { channels = ReflexVolumeChannels.Shared; } catch (Exception) { } }
            if (Time.unscaledTime - settingsAt > 0.25f) ReadSettings();
            for (int c = 0; c < channelGain.Length; c++)
            {
                var ch = channels?.channels[c];
                float user = ReflexVolumeChannels.SettingOf((ReflexChannel)c) == ReflexVolumeChannels.Setting.Engine ? engineVolume : masterVolume;
                channelGain[c] = (ch != null ? ch.master : 1f) * user * levelMix[c] * (c == (int)ReflexChannel.Crowd ? Reflex.Port.Options.ReflexSettings.CrowdVolume : 1f);   // CROWD VOLUME (Options): ours, over EFFECTS
            }
        }
        bool KeepsPlayingWhenPaused(int c)
        {
            if (c == (int)ReflexChannel.Global) c = (int)ReflexChannel.Effects;   // offline libraries carry no channel
            var ch = channels?.channels[c];
            return ch != null ? !ch.pausedWithGame : c == (int)ReflexChannel.UI || c == (int)ReflexChannel.VoiceOver || c == (int)ReflexChannel.Music;
        }

        // FMOD as the game sets it up: System::setSoftwareChannels(64) (0x80440f), EventSystem::init(500 channels, ...) (0x804490).
        // Unity's default is 32 real voices; 12-rider races (engines, drive-on, gravel spawns) need the original's 64.
        public const int RealVoices = 64, VirtualVoices = 512;
        /// Seconds a voice takes to fade in or out when the max-playbacks ranking mutes or restores it (ours; FMOD steals with its own short fade).
        public const float VirtualRamp = 0.03f;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ConfigureVoices()
        {
            var cfg = AudioSettings.GetConfiguration();
            if (cfg.numRealVoices >= RealVoices && cfg.numVirtualVoices >= VirtualVoices) return;
            cfg.numRealVoices = Mathf.Max(cfg.numRealVoices, RealVoices); cfg.numVirtualVoices = Mathf.Max(cfg.numVirtualVoices, VirtualVoices);
            if (!AudioSettings.Reset(cfg)) Debug.LogWarning("Audio: the original's 64 real voices could not be configured");
        }

        void Load()
        {
            if (loadStarted) return;
            if (library)                      // offline override
            {
                loadStarted = true;
                lib = JsonUtility.FromJson<Library>(library.text);
                foreach (var e in lib.events) events[e.path] = e;
                for (int i = 0; i < clipFiles.Length && i < clips.Length; i++) if (clips[i]) clipByFile[clipFiles[i]] = clips[i];
                ClipsReady = true; ClipsTotal = clipByFile.Count;
            }
            else
            {
                string lv = !string.IsNullOrEmpty(level) ? level : LevelOfActiveScene();
                if (!string.IsNullOrEmpty(lv)) Begin(lv);
            }
        }

        /// Scenes built before `level` existed: the level follows from the scene's name (AlpineOriginalPhysics, ConcordOriginalPhysics).
        public static string LevelOfActiveScene()
        {
            string n = UnityEngine.SceneManagement.SceneManager.GetActiveScene().name;
            return n.IndexOf("Concord", StringComparison.OrdinalIgnoreCase) >= 0 ? "NAT_Concord" : n.IndexOf("Alpine", StringComparison.OrdinalIgnoreCase) >= 0 ? "FR_Alpine" : "";
        }

        /// Starts building the library of a level from the Reflex install (call before the first command when `level` was not set).
        public void Begin(string levelName)
        {
            level = levelName; loadStarted = true; lib = null; ClipsReady = false;
            buildStart = Time.realtimeSinceStartup;
            SetLevelMix(null, 0);
            _ = ReflexInstall.Packages;      // resolves the install (PlayerPrefs / Application paths) on the main thread
            cacheDir = Path.Combine(ReflexInstall.CacheDir, "audio"); Directory.CreateDirectory(cacheDir);
            var race = Spec = new ReflexAudioSet.Race { level = levelName, activity = ResolveActivity(levelName), language = ResolveLanguage() };
            foreach (var p in race.Projects()) requested.Add(p);
            build = Task.Run(() => ReflexAudioSet.Build(race));
        }

        /// A library without a level (the front end's FeSounds, WP-F): no events until AddProjects brings their projects; the commands
        /// sent meanwhile wait for them (gate), as a race's wait for its level.
        public void BeginEmpty(string label)
        {
            level = label; loadStarted = true; ClipsReady = true; libReadyAt = Time.realtimeSinceStartup;
            lib = new Library { level = label, events = new EventDef[0], files = new string[0] };
            _ = ReflexInstall.Packages;
            cacheDir = Path.Combine(ReflexInstall.CacheDir, "audio"); Directory.CreateDirectory(cacheDir);
        }

        /// The event's engine ActivityName: `activity`, else the key of ReflexEventSelect's event ("Supercross/SX_Kingston"), else
        /// the level's prefix.
        string ResolveActivity(string levelName)
        {
            if (!string.IsNullOrEmpty(activity)) return activity;
            var sel = FindAnyObjectByType<ReflexEventSelect>();
            string key = sel ? sel.Current : null;
            if (!string.IsNullOrEmpty(key) && key.IndexOf('/') > 0)
            {
                var parts = key.Split('/');
                if (parts.Length > 1 && string.Equals(parts[1], levelName, StringComparison.OrdinalIgnoreCase)) return activity = parts[0];
            }
            return activity = ReflexAudioSet.ActivityOfLevel(levelName);
        }
        string ResolveLanguage()
        {
            if (string.IsNullOrEmpty(language)) { try { language = ReflexUiData.Shared.language; } catch (Exception) { language = "ENG"; } }
            return language;
        }

        void Update() { Load(); CheckRoster(); Poll(); Flush(); }

        // ---- projects added after the main build (roster engines, on-demand loads) ----
        sealed class Extension { public Task<ReflexAudioSet> task; public bool gate, clipsDone; public float started; public string[] projects; }
        readonly List<Extension> extensions = new List<Extension>();
        readonly HashSet<string> requested = new HashSet<string>(StringComparer.OrdinalIgnoreCase);       // in the library or being built
        readonly HashSet<string> loadedProjects = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        /// Adds FEV projects of the global audio databases to the library (built on a worker thread, merged when ready). `gate`:
        /// the game's commands wait for them (up to MaxWait), as for the main library.
        public void AddProjects(IEnumerable<string> names, bool gate = true)
        {
            var want = names.Where(n => !string.IsNullOrEmpty(n) && requested.Add(n)).ToArray();
            if (want.Length == 0 || library) return;
            if (!loadStarted) { Load(); if (!loadStarted) return; }
            _ = ReflexInstall.Packages;
            if (cacheDir == null) { cacheDir = Path.Combine(ReflexInstall.CacheDir, "audio"); Directory.CreateDirectory(cacheDir); }
            string label = level + "+" + string.Join("+", want);
            extensions.Add(new Extension { task = Task.Run(() => ReflexAudioSet.BuildProjects(label, want)), gate = gate, started = Time.realtimeSinceStartup, projects = want });
        }

        /// The roster hook: the engine project of every vehicle in the race (VehicleModel -> VehicleEngineSize -> AudioEngineStates),
        /// so a car-type vehicle (UTV RHINO_ENGINE_02, Pro 2 PRO_02_02, Pro Lite PRO_LITE_ENGINE_01, Super Buggy BUGGY_ENGINE_01)
        /// brings its engine. Called by itself once OriginalWorldRuntime has read the roster.
        public void UseRoster(ReflexRoster roster)
        {
            if (roster == null) return;
            var engines = new List<string>();
            foreach (var e in roster.entries)
            {
                if (e.vehicleModelId < 0) continue;
                string p = null; try { p = ReflexAudioSet.EngineProjectOfModel(e.vehicleModelId); } catch (Exception x) { Debug.LogWarning("Audio: engine of vehicle model " + e.vehicleModelId + ": " + x.Message); }
                if (p != null && !engines.Contains(p)) engines.Add(p);
            }
            RaceEngines = engines.ToArray();
            AddProjects(engines, true);
        }
        public string[] RaceEngines { get; private set; } = new string[0];
        OriginalWorldRuntime runtime; bool rosterSeen; int runtimeSearches;
        /// The player's view is the first-person camera (the camera sound effect's reverb applies): OriginalWorldRuntime.FirstPersonView.
        bool FirstPersonView() => firstPersonOverride ?? (runtime && runtime.FirstPersonView);
        /// Tests: forces the first-person view on or off for the reverb (null: the runtime's camera).
        public bool? firstPersonOverride;
        void CheckRoster()
        {
            if (rosterSeen || library) return;
            if (!runtime && runtimeSearches < 600) { runtimeSearches++; runtime = FindAnyObjectByType<OriginalWorldRuntime>(); }
            if (runtime && runtime.Roster != null) { rosterSeen = true; UseRoster(runtime.Roster); }
        }

        void PollExtensions()
        {
            for (int i = 0; i < extensions.Count; i++)
            {
                var x = extensions[i];
                if (x.task == null || !x.task.IsCompleted || lib == null) continue;
                var t = x.task; x.task = null;
                if (t.IsFaulted || t.IsCanceled) { Debug.LogWarning("Audio projects " + string.Join(", ", x.projects) + " not loaded: " + t.Exception?.GetBaseException().Message); x.clipsDone = true; continue; }
                var set = t.Result;
                foreach (var m in set.missing) Debug.LogWarning("Audio project " + m + " is not in the install's audio databases");
                var added = new List<EventDef>();
                foreach (var e in set.library.events) if (!events.ContainsKey(e.path)) { events[e.path] = e; added.Add(e); }
                foreach (var pn in set.projects) loadedProjects.Add(pn);
                lib.events = lib.events.Concat(added).ToArray();
                lib.files = lib.files.Union(set.library.files).ToArray();
                var newWaves = set.waves.Where(kv => !clipByFile.ContainsKey(kv.Key)).ToList();
                ClipsTotal += newWaves.Count;
                var only = new ReflexAudioSet { level = set.level }; foreach (var kv in newWaves) only.waves[kv.Key] = kv.Value;
                StartCoroutine(LoadClips(only, () => x.clipsDone = true));
            }
        }

        void Poll()
        {
            PollExtensions();
            if (build == null || !build.IsCompleted) return;
            var t = build; build = null;
            LibrarySeconds = Time.realtimeSinceStartup - buildStart;
            if (t.IsFaulted || t.IsCanceled)
            {
                LoadError = (t.Exception?.GetBaseException().Message) ?? "canceled";
                Debug.LogError("Original game sound (" + level + ") could not be loaded from the Reflex install: " + LoadError);
                lib = new Library { level = level, events = new EventDef[0], files = new string[0] }; ClipsReady = true;
            }
            else
            {
                var set = t.Result; lib = set.library;
                if (set.reverbDefs.TryGetValue("FirstPerson", out var fp)) Reverb.SetCameraDef(fp);   // CameraSoundEffectsManagerConfig's Reverb "FirstPerson" (the level's Default project)
                foreach (var e in lib.events) events[e.path] = e;
                foreach (var pn in set.projects) { loadedProjects.Add(pn); requested.Add(pn); }
                foreach (var m in set.missing) Debug.LogWarning("Audio project " + m + " is not in the install's audio databases");
                ClipsTotal += set.waves.Count;
                StartCoroutine(LoadClips(set, () => { ClipsReady = true; LibraryClipsLoaded?.Invoke(this); }));
            }
            libReadyAt = Time.realtimeSinceStartup;
        }

        // Commands wait (in order) until the level's waves are decoded (a fraction of a second after the library, cold cache a few
        // seconds) so that nothing the game starts in the first moments is lost; after MaxWait they play with whatever is decoded.
        const float MaxWait = 8f; float libReadyAt;
        bool Gated => lib == null || (!ClipsReady && Time.realtimeSinceStartup - libReadyAt < MaxWait) || ExtensionGate;
        bool ExtensionGate
        {
            get { foreach (var x in extensions) if (x.gate && !x.clipsDone && Time.realtimeSinceStartup - x.started < MaxWait + 30f) return true; return false; }
        }
        bool flushing;
        void Flush()
        {
            if (flushing || Gated) return;
            flushing = true;
            while (pending.Count > 0) Dispatch(pending.Dequeue());
            flushing = false;
        }

        // ---- waves: MPEG through a cached .mp3 + Unity's decoder, PCM/IMA in C# ----
        sealed class Prepared { public string key; public string path; public short[] pcm; public int rate, channels, length; }
        // Unity's MP3 decoder (and every Layer III decoder) outputs the 528+1 sample decoder delay before the stream's first sample and
        // flushes it after the last frame; FMOD (and the FSB's length) start at the first sample and stop after frames*1152 samples.
        // Measured against afconvert's WAVs: the best alignment is exactly 529 samples for every wave.
        const int Mp3DecoderDelay = 529;
        float[] scratch = new float[0];

        AudioClip Trim(AudioClip clip, string key, int length)
        {
            int ch = clip.channels, total = clip.samples, len = Mathf.Min(length, total - Mp3DecoderDelay);
            if (len <= 0) return clip;
            if (scratch.Length < total * ch) scratch = new float[total * ch];
            clip.GetData(scratch, 0);
            var slice = new float[len * ch]; Array.Copy(scratch, Mp3DecoderDelay * ch, slice, 0, slice.Length);
            var trimmed = AudioClip.Create(key, len, ch, clip.frequency, false);
            trimmed.SetData(slice, 0);
            Destroy(clip);
            return trimmed;
        }

        IEnumerator LoadClips(ReflexAudioSet set, Action done)
        {
            // the engine banks first (they are needed the moment the race starts), then the rest in library order
            var keys = set.waves.Keys.OrderBy(k => k.StartsWith("engine/", StringComparison.Ordinal) ? 0 : 1).ToList();
            var queue = new ConcurrentQueue<Prepared>(); bool workerDone = false;
            var worker = Task.Run(() =>
            {
                try
                {
                    string dir = cacheDir;
                    foreach (var k in keys)
                    {
                        var w = set.waves[k];
                        try
                        {
                            if (w.codec == "mpeg")
                            {
                                string path = Path.Combine(dir, w.CacheName);
                                if (!File.Exists(path) || new FileInfo(path).Length == 0)
                                {
                                    string tmp = path + "." + Environment.CurrentManagedThreadId + ".tmp";
                                    File.WriteAllBytes(tmp, w.Mp3());
                                    try { File.Move(tmp, path); } catch (IOException) { File.Delete(tmp); }
                                }
                                queue.Enqueue(new Prepared { key = k, path = path, length = w.lengthSamples });
                            }
                            else if (w.codec == "pcm" || w.codec == "ima_adpcm")
                                queue.Enqueue(new Prepared { key = k, pcm = w.Pcm(), rate = w.rate, channels = w.channels });
                        }
                        catch (Exception e) { Debug.LogWarning("Original sound " + k + ": " + e.Message); }
                    }
                }
                catch (Exception e) { Debug.LogError("Original sound decoding stopped: " + e); }
                finally { workerDone = true; }
            });
            var active = new List<(Prepared p, UnityWebRequest r)>();
            const int Concurrent = 8;
            var frameWork = new System.Diagnostics.Stopwatch();
            while (!workerDone || !queue.IsEmpty || active.Count > 0)
            {
                frameWork.Restart(); int made = clipByFile.Count;
                while (active.Count < Concurrent && queue.TryDequeue(out var p))
                {
                    if (p.pcm != null)
                    {
                        var data = new float[p.pcm.Length];
                        for (int i = 0; i < data.Length; i++) data[i] = p.pcm[i] / 32768f;
                        var clip = AudioClip.Create(p.key, p.pcm.Length / p.channels, p.channels, p.rate, false);
                        clip.SetData(data, 0); clipByFile[p.key] = clip;
                        continue;
                    }
                    var req = UnityWebRequestMultimedia.GetAudioClip(new Uri(p.path).AbsoluteUri, AudioType.MPEG);
                    ((DownloadHandlerAudioClip)req.downloadHandler).streamAudio = false;
                    req.SendWebRequest(); active.Add((p, req));
                }
                float frameStart = Time.realtimeSinceStartup;
                for (int i = active.Count - 1; i >= 0; i--)
                {
                    var (p, r) = active[i];
                    if (!r.isDone) continue;
                    if (Time.realtimeSinceStartup - frameStart > 0.004f) break;      // keep frames short: finish the rest next frame
                    if (r.result == UnityWebRequest.Result.Success)
                    {
                        var clip = DownloadHandlerAudioClip.GetContent(r);
                        if (clip) { clip.name = p.key; clipByFile[p.key] = Trim(clip, p.key, p.length); }
                    }
                    else Debug.LogWarning("Original sound " + p.key + ": " + r.error);
                    r.Dispose(); active.RemoveAt(i);
                }
                if (frameWork.Elapsed.TotalMilliseconds > 20) Debug.Log($"[Audio] {level}: a long frame of the sounds' load: {clipByFile.Count - made} clip(s) made in {frameWork.Elapsed.TotalMilliseconds:F0} ms");
                yield return null;
            }
            foreach (var w in set.waves.Values) w.DropBank();
            done?.Invoke();
        }

        public event Action<OriginalAudio> LibraryClipsLoaded;

        // ---- host-issued commands (the host's own sound layer: OriginalEngineAudio, the engine emitters) ----
        /// Wrappers at or above this belong to the host's sound layer: their EVENT_FINISHED goes to HostFinished, not to the game's queue.
        public const uint HostWrapperBase = 0xF0000000u;
        /// EVENT_FINISHED of a host wrapper (wrapper, serial), on the main thread inside Tick / Handle.
        public event Action<uint, uint> HostFinished;
        // ---- the OPEN REFLEX replay (Reflex.Port.Replay.ReflexReplay, w-replayphoto2) ----
        /// Wrappers at or above this are the replay's own instances (the recorded commands played again): nobody is told they finished.
        public const uint ReplayWrapperBase = 0xFE000000u;
        /// Every command, the game's (Handle) and the host's (Host), as it arrives (the replay records them); not the replay's own.
        public static event Action<OriginalAudio, int, uint, uint, string, string, float, Vector3, bool> CommandIssued;
        /// While isolated only the replay's instances play: every other instance's voices are paused where they are and skipped by Tick
        /// (no fades, no EVENT_FINISHED), then resumed when the isolation ends, so the paused world's sound is exactly where it was.
        public bool ReplayIsolated { get; private set; }
        readonly List<Voice> isolationPaused = new List<Voice>();
        public void SetReplayIsolation(bool on)
        {
            if (on == ReplayIsolated) return;
            ReplayIsolated = on;
            if (on)
            {
                foreach (var inst in instances.Values)
                {
                    if (inst.wrapper >= ReplayWrapperBase || inst.slots == null) continue;
                    foreach (var layer in inst.slots) foreach (var st in layer) foreach (var v in st.voices)
                        if (v.source && !v.paused) { v.source.Pause(); v.paused = true; isolationPaused.Add(v); }
                }
            }
            else
            {
                foreach (var v in isolationPaused) if (v.source && v.paused && v.started) { v.source.UnPause(); v.paused = false; }
                isolationPaused.Clear();
            }
        }
        /// Stops every replay instance at once (a seek, the viewer muted or closed).
        public void StopReplayInstances() { foreach (var inst in instances.Values) if (inst.wrapper >= ReplayWrapperBase && inst.playing) { StopVoices(inst); inst.playing = false; } }
        /// Replay instances playing now (tests).
        public int ReplayInstancesPlaying { get { int n = 0; foreach (var inst in instances.Values) if (inst.wrapper >= ReplayWrapperBase && inst.playing) n++; return n; } }
        /// Replay voices audible now (an AudioSource playing with a volume above 0; tests).
        public int ReplayVoicesAudible
        {
            get
            {
                int n = 0;
                foreach (var inst in instances.Values)
                    if (inst.wrapper >= ReplayWrapperBase && inst.playing && inst.slots != null)
                        foreach (var layer in inst.slots) foreach (var st in layer) foreach (var v in st.voices) if (v.source && v.source.isPlaying && v.source.volume > 0f) n++;
                return n;
            }
        }

        void Finish(uint wrapper, uint serial)
        {
            if (wrapper >= ReplayWrapperBase) return;
            if (wrapper >= HostWrapperBase) HostFinished?.Invoke(wrapper, serial);
            else Finished.Enqueue((wrapper, serial));
        }
        /// A command of the host's sound layer (same sub-commands as the game's: 0 play, 1 stop, 12 stop now, 2 volume, 4 parameter, 5 3D position,
        /// 10 occlusion). Dispatched at once without allocating when the library is ready, else queued in order with the game's.
        public void Host(int sub, uint wrapper, uint serial, string path, string param, float value, Vector3 position, bool hasPosition)
        {
            if (wrapper < ReplayWrapperBase) CommandIssued?.Invoke(this, sub, wrapper, serial, path, param, value, position, hasPosition);
            Load(); Poll();
            if (!loadStarted) { lib = new Library { events = new EventDef[0], files = new string[0] }; loadStarted = true; ClipsReady = true; }
            if (Gated || pending.Count > 0)
            {
                pending.Enqueue(new ReflexAudioCommand { sub = sub, wrapper = wrapper, serial = serial, path = path ?? "", param = param ?? "", value = value,
                    position = new[] { position.x, position.y, position.z }, has_position = hasPosition ? 1 : 0 });
                Flush(); return;
            }
            instances.TryGetValue(wrapper, out var inst);
            switch (sub)
            {
                case 0: StartEvent(wrapper, Lower(path), serial, hasPosition, position); break;
                case 1: if (inst != null && inst.playing) BeginStop(inst, false); break;
                case 12: if (inst != null && inst.playing) BeginStop(inst, true); break;
                case 2: if (inst != null) inst.volumeCmd = value; break;   // Event::setVolume (the engine field trim, w-aiengine)
                case 4: if (inst != null) SetParam(inst, param, value); break;
                case 5: if (inst != null) inst.position = position; break;
                case 10: if (inst != null) inst.occlusion = Mathf.Clamp01(value); break;
                case 6: SetMinMax(wrapper, value, position.x); break;   // set3DMinMaxDistance(min = value, max = position.x) (0080b310)
                case 13: SetChannel(wrapper, (int)value); break;       // the wrapper's volume channel (0080bce0)
                case 14: SetSelector(wrapper, (int)value); break;      // the wrapper's SOUNDDEF_SELECTINDEX owner callback (crowd kind 0 .. 3)
            }
        }
        /// The wrapper's owner callback for SOUNDDEF_SELECTINDEX (wrapper + 0x19c): the crowd emitters' 006743b0, `kind` 0 amb / 1 neg / 2 pos / 3 pos start-end.
        void SetSelector(uint wrapper, int kind)
        {
            if (!instances.TryGetValue(wrapper, out var inst)) { inst = new Instance { wrapper = wrapper }; instances[wrapper] = inst; }
            inst.selKind = kind;
        }
        /// The crowd's rotation counters ([0xa7f088 + 4 kind], zeroed by the crowd unit's init 00674cd0 at the world's start).
        readonly int[] crowdCounter = new int[4];
        public void ResetCrowdPicks() { for (int i = 0; i < crowdCounter.Length; i++) crowdCounter[i] = 0; }
        /// A programmer-selected sound definition's entry (play mode 5; AudioEvent_OnSelectIndex 0080bbd0 with `count` entries). With an owner callback it is
        /// the crowd's 006743b0: the index the wrapper kept from its last pick if it is still in range (every layer of a CROWD_AMB / _RCT_* event, and its
        /// loops, take the same index), else the next of a rotation shared by all emitters of that kind ((counter + 1), 0 at the count). Without one the game
        /// draws from its own LCG (g_GameRandSeed): the core's, which the host cannot reach, so the host's random stands in (QUEUE: w-reverb).
        int PickProgrammer(Instance inst, int count)
        {
            int result;
            if (inst.selKind >= 0 && inst.selKind < crowdCounter.Length)
            {
                if (inst.selIndex >= 0 && inst.selIndex < count) return inst.selIndex;
                int c = crowdCounter[inst.selKind] + 1; if (c >= count) c = 0;
                crowdCounter[inst.selKind] = c; result = c;
            }
            else result = random.Next(count);
            result = Mathf.Clamp(result, 0, count - 1);
            inst.selIndex = result;
            return result;
        }

        void SetChannel(uint wrapper, int channel)
        {
            if (!instances.TryGetValue(wrapper, out var inst)) { inst = new Instance { wrapper = wrapper }; instances[wrapper] = inst; }
            inst.channelOverride = channel >= 0 && channel < channelGain.Length ? channel : -1;
        }
        /// Whether an event of the library is 3D (its FMOD mode), false when unknown.
        public bool IsEvent3D(string path) => !string.IsNullOrEmpty(path) && events.TryGetValue(Lower(path), out var def) && def.is3d;
        /// Event::set3DMinMaxDistance on a wrapper's instance (kept by the instance, as the game's FMOD event handle keeps it).
        void SetMinMax(uint wrapper, float min, float max)
        {
            if (!instances.TryGetValue(wrapper, out var inst)) { inst = new Instance { wrapper = wrapper }; instances[wrapper] = inst; }
            inst.minDistance = min; inst.maxDistance = max;
            if (inst.slots != null) foreach (var layer in inst.slots) foreach (var st in layer) foreach (var v in st.voices) if (v.source) { v.source.minDistance = min; v.source.maxDistance = Mathf.Max(max, min + 0.01f); }
        }
        /// The range of an event's parameter (FMOD EventParameter::getRange, 0080b0b0), false when the event or parameter is not loaded.
        public bool TryEventParameterRange(string path, string name, out float min, out float max)
        {
            min = max = 0f;
            if (string.IsNullOrEmpty(path) || !events.TryGetValue(Lower(path), out var def)) return false;
            foreach (var p in def.parameters) if (string.Equals(p.name, name, StringComparison.OrdinalIgnoreCase)) { min = p.min; max = p.max; return true; }
            return false;
        }
        /// The name of an event's first parameter (FMOD Event::getParameterByIndex(0), the command 0x15 the drive-on emitter sends), null when unknown.
        public string FirstEventParameter(string path)
        {
            if (string.IsNullOrEmpty(path) || !events.TryGetValue(Lower(path), out var def) || def.parameters.Length == 0) return null;
            return def.parameters[0].name;
        }
        /// A volume channel's gain now (MasterVolume x the user's setting), for host voices outside the event library (engine packs).
        public float ChannelGain(ReflexChannel c) { UpdateChannelGainsIfNeeded(); return channelGain[(int)c]; }
        void UpdateChannelGainsIfNeeded() { if (channels == null || Time.unscaledTime - settingsAt > 0.25f) UpdateChannelGains(); }

        // ---- commands from the game ----
        public void Handle(ReflexAudioCommand c)
        {
            Load(); if (!rosterSeen && runtime) CheckRoster(); Poll();
            if (c.sub == 0 && c.path != null && c.path.IndexOf('[') >= 0) c.path = ReflexAudioSet.ResolveTokens(c.path, Spec);   // a scenario's [FORMAT]_[LOC] voice
            CommandIssued?.Invoke(this, c.sub, c.wrapper, c.serial, c.path, c.param, c.value, Vec(c), c.has_position != 0);
            if (!loadStarted) { lib = new Library { events = new EventDef[0], files = new string[0] }; loadStarted = true; ClipsReady = true; }   // no level and no library: silent
            if (Gated || pending.Count > 0) { pending.Enqueue(c); Flush(); return; }
            Dispatch(c);
        }

        // event paths repeat every frame (interned by ReflexAudioCommand.Decode): lowered once each
        readonly Dictionary<string, string> lowered = new Dictionary<string, string>();
        string Lower(string p)
        {
            if (string.IsNullOrEmpty(p)) return "";
            if (!lowered.TryGetValue(p, out var l)) { if (lowered.Count > 4096) lowered.Clear(); lowered[p] = l = p.ToLowerInvariant(); }
            return l;
        }

        void Dispatch(ReflexAudioCommand c)
        {
            string path = Lower(c.path);
            instances.TryGetValue(c.wrapper, out var inst);
            switch (c.sub)
            {
                case 0: StartEvent(c.wrapper, path, c.serial, c.has_position != 0, Vec(c)); break;
                case 1: if (inst != null && inst.playing) BeginStop(inst, false); break;
                case 12: if (inst != null && inst.playing) BeginStop(inst, true); break;
                case 2: if (inst != null) inst.volumeCmd = c.value; break;
                case 7: if (inst != null) inst.muted = c.value != 0; break;
                case 8: if (inst != null) inst.paused = c.value != 0; break;
                case 10: OcclusionCommands++; if (inst != null) inst.occlusion = Mathf.Clamp01(c.value); break;   // set3DOcclusion(direct = cmd+0x0c, reverb = cmd+0x10)
                case 6: SetMinMax(c.wrapper, c.value, Vec(c).x); break;   // a queued host set3DMinMaxDistance
                case 13: SetChannel(c.wrapper, (int)c.value); break;
                case 14: SetSelector(c.wrapper, (int)c.value); break;
                case 4:
                case 5:
                    // An event already playing when the world was captured never gets a play command: resume it.
                    if ((inst == null || !inst.playing) && c.state == 6 && events.ContainsKey(path)) inst = StartEvent(c.wrapper, path, c.serial, false, Vector3.zero);
                    if (inst == null) break;
                    if (c.sub == 5) inst.position = Vec(c);
                    else SetParam(inst, c.param, c.value);
                    break;
            }
        }

        static Vector3 Vec(ReflexAudioCommand c) => c.position != null && c.position.Length >= 3 ? new Vector3(c.position[0], c.position[1], c.position[2]) : Vector3.zero;

        EventStat Stat(string path, EventDef def)
        {
            if (!Stats.TryGetValue(path, out var st)) { st = new EventStat(); Stats[path] = st; }
            if (def != null) st.channel = (ReflexChannel)def.channel;
            return st;
        }

        Instance StartEvent(uint wrapper, string path, uint serial, bool hasPosition, Vector3 position)
        {
            if (!events.TryGetValue(path, out var def))
            {
                Stat(path, null).unknown++;
                LoadOnDemand(path);
                Finish(wrapper, serial);   // not in this level's library: end at once so the game's logic moves on
                return null;
            }
            instances.TryGetValue(wrapper, out var inst);
            var stat = Stat(path, def); stat.starts++;
            // FMOD max playbacks: the event's instances (other wrappers) beyond its limit, by the event's behaviour
            if (def.maxPlaybacks > 0)
            {
                Instance victim = null; int live = 0; float quietest = float.MaxValue;
                Vector3 at = hasPosition ? position : inst != null ? inst.position : Vector3.zero;
                foreach (var o in instances.Values)
                {
                    if (o == inst || !o.playing || o.def != def) continue;
                    if (ReplayIsolated && (o.wrapper >= ReplayWrapperBase) != (wrapper >= ReplayWrapperBase)) continue;   // (the replay's and the paused world's instances never steal from each other)
                    live++;
                    float a = Audibility(o, o.position);
                    switch (def.maxPlaybacksBehavior)
                    {
                        case 2: if (victim == null || o.startedAt > victim.startedAt) victim = o; break;                    // steal newest
                        case 3: case 5: if (a < quietest) { quietest = a; victim = o; } break;                               // steal quietest
                        default: if (victim == null || o.startedAt < victim.startedAt) victim = o; break;                   // 1 steal oldest
                    }
                }
                if (live >= def.maxPlaybacks)
                {
                    float mine = def.volume * (inst != null ? Mathf.Max(inst.volumeCmd, 0f) : 1f) * Rolloff(def, at);
                    bool fail = def.maxPlaybacksBehavior == 4 || def.maxPlaybacksBehavior == 5 && mine <= quietest;
                    if (fail || victim == null)
                    {
                        stat.failed++;
                        if (inst != null) { StopVoices(inst); inst.playing = false; }
                        Finish(wrapper, serial);
                        return null;
                    }
                    if (def.maxPlaybacks == 1)
                    {
                        // a single-instance event (every announcer / activity line): the new start takes over, the old one ends.
                        // FMOD reports it STOLEN (callback type 3: wrapper state 8 and its owner's stolen handler); the plugin only
                        // carries EVENT_FINISHED, so the game hears that.
                        StopVoices(victim); victim.playing = false; victim.stopping = false;
                        if (victim.stat != null) victim.stat.stolen++;
                        Finish(victim.wrapper, victim.serial);
                    }
                    // pools (engines, drive-on, collisions, crowd: max 2..12): every instance keeps running for the game and only the
                    // max_playbacks the behaviour keeps are audible (Tick: Virtualize), re-evaluated every frame
                }
            }
            if (inst == null) { inst = new Instance { wrapper = wrapper }; instances[wrapper] = inst; }
            StopVoices(inst);
            if (inst.def != def)
            {
                inst.def = def;
                inst.param = new float[def.parameters.Length];
                for (int i = 0; i < inst.param.Length; i++) inst.param[i] = def.parameters[i].min;   // FMOD: initial value = min
                inst.slots = new SlotState[def.layers.Length][];
                for (int l = 0; l < def.layers.Length; l++) { inst.slots[l] = new SlotState[def.layers[l].instances.Length]; for (int s = 0; s < inst.slots[l].Length; s++) inst.slots[l][s] = new SlotState(); }
            }
            else foreach (var layer in inst.slots) foreach (var s in layer) { s.active = false; s.fired = false; s.seq = 0; s.spawnTimer = 0; }
            if (!inst.node) { inst.node = new GameObject("FMOD event " + wrapper.ToString("x")); inst.node.transform.SetParent(transform, false); }
            inst.node.name = def.path;
            inst.serial = serial; inst.playing = true; inst.stopping = false; inst.paused = false; inst.muted = false; inst.keyedOff = false; inst.virtGain = inst.virtualized ? 0f : 1f;
            inst.selIndex = -1;
            inst.startedAt = Time.realtimeSinceStartup; inst.stat = stat; inst.ticked = false;
            inst.gain = def.volume * (1f - Rand(0, def.volumeRand));
            inst.pitch = Mathf.Pow(2f, def.pitchOctaves + Rand(-def.pitchRandOctaves, def.pitchRandOctaves));
            inst.fade = def.fadeInMs > 0 ? 0f : 1f; inst.fadeRate = def.fadeInMs > 0 ? 1000f / def.fadeInMs : 0f;
            if (hasPosition) inst.position = position;
            return inst;
        }

        // max_playbacks of the pooled events (max 2..12: engines, drive-on, collisions, crowd, scenario): of the instances of one
        // event beyond its limit only the loudest max_playbacks are audible, the rest are silent until they rank again. FMOD
        // steals for good (STOLEN callback, wrapper state 8), which the plugin cannot deliver; ranking every frame keeps the same
        // audible set as steal quietest (behaviour 3, every pool but MX_250R_TEST_05's 22 engine events and one SCENARIOS_GLOBAL
        // event, which steal the oldest: ranked by loudness too, so the rider's own engine is never the one dropped).
        readonly Dictionary<EventDef, List<Instance>> byDef = new Dictionary<EventDef, List<Instance>>();
        readonly List<List<Instance>> listPool = new List<List<Instance>>();
        public int Virtualized { get; private set; }
        void Virtualize()
        {
            foreach (var l in byDef.Values) { l.Clear(); listPool.Add(l); }
            byDef.Clear();
            int virt = 0;
            foreach (var inst in instances.Values)
            {
                inst.virtualized = false;
                if (!inst.playing || inst.def.maxPlaybacks <= 0) continue;
                if (ReplayIsolated && inst.wrapper < ReplayWrapperBase) continue;   // (paused under the replay: not ranked against it)
                if (!byDef.TryGetValue(inst.def, out var l))
                {
                    if (listPool.Count > 0) { l = listPool[listPool.Count - 1]; listPool.RemoveAt(listPool.Count - 1); } else l = new List<Instance>();
                    byDef[inst.def] = l;
                }
                l.Add(inst);
            }
            foreach (var kv in byDef)
            {
                var l = kv.Value; int max = kv.Key.maxPlaybacks;
                if (l.Count <= max) continue;
                l.Sort((a, b) => Audibility(b, b.position).CompareTo(Audibility(a, a.position)));                             // keep the loudest
                for (int i = max; i < l.Count; i++) { l[i].virtualized = true; virt++; }
            }
            Virtualized = virt;
        }

        // A play command of a project that is not in the library: load the project if a global audio database has it (the event
        // itself stays silent; later ones play). The music projects are OriginalMusic's (programmer sounds), not loaded here.
        readonly HashSet<string> demandTried = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        void LoadOnDemand(string path)
        {
            int slash = path.IndexOf('/'); if (slash <= 0 || library) return;
            string project = path.Substring(0, slash);
            if (!demandTried.Add(project) || requested.Contains(project) || loadedProjects.Contains(project)) return;
            if (project.StartsWith("music_", StringComparison.OrdinalIgnoreCase) || project.StartsWith("ui_mus", StringComparison.OrdinalIgnoreCase) || project.StartsWith("scenarios_music", StringComparison.OrdinalIgnoreCase)) return;
            bool global = false; try { global = ReflexAudioSet.IsGlobalProject(project); } catch (Exception) { }
            if (!global) return;
            Debug.LogWarning("Audio: the game played " + path + " but its project " + project + " was not in the library; loading it now");
            OnDemand.Add(project);
            AddProjects(new[] { project }, false);
        }

        Vector3 listenerAt;
        /// The listener position of the last Tick (game coordinates), for the host's sound layer (the managers' nearest-listener tests).
        public Vector3 Listener => listenerAt;
        static float Rolloff(EventDef d, Vector3 p, Vector3 listener)
        {
            if (!d.is3d) return 1f;
            float dist = Vector3.Distance(p, listener), mn = d.minDistance, mx = Mathf.Max(d.maxDistance, mn + 0.01f);
            return dist <= mn ? 1f : dist >= mx ? 0f : 1f - (dist - mn) / (mx - mn);
        }
        float Rolloff(EventDef d, Vector3 p) => Rolloff(d, p, listenerAt);
        /// FMOD_INIT_OCCLUSION_LOWPASS: the cutoff of a fully open event (22050 Hz; the occlusion only low-passes).
        public const float OcclusionCutoff = 22050f;
        float Audibility(Instance o, Vector3 p) => o.gain * o.volumeCmd * o.fade * (o.muted ? 0f : 1f) * RolloffOf(o, p);
        float RolloffOf(Instance o, Vector3 p)
        {
            if (o.minDistance < 0f || !o.def.is3d) return Rolloff(o.def, p);
            float dist = Vector3.Distance(p, listenerAt), mn = o.minDistance, mx = Mathf.Max(o.maxDistance, mn + 0.01f);
            return dist <= mn ? 1f : dist >= mx ? 0f : 1f - (dist - mn) / (mx - mn);
        }

        void BeginStop(Instance inst, bool immediate)
        {
            if (!inst.ticked && inst.stat != null) inst.stat.stoppedBeforePlay++;   // started and stopped by the game between two mixes
            if (!immediate && inst.def.fadeOutMs > 0) { inst.stopping = true; inst.fadeRate = -1000f / inst.def.fadeOutMs; return; }
            if (!immediate && inst.ticked && KeyOff(inst)) return;
            StopVoices(inst); inst.playing = false;
        }

        /// Event::stop(false) of an event with no fade-out time (w-landclank). FMOD ends the loops and lets the one-shot samples that are
        /// playing finish; the port cut everything at once. The game depends on it: the drive-on emitter starts a landing event for the
        /// one frame of the landing and calls stop(false) on the next (006828b0, the skid / landing / foley-tire events), so a cut leaves
        /// a one-frame click instead of the landing sound. False when no one-shot voice is playing (then there is nothing to let finish).
        bool KeyOff(Instance inst)
        {
            if (inst.slots == null) return false;
            bool oneShots = false;
            for (int l = 0; l < inst.def.layers.Length; l++)
                for (int s = 0; s < inst.slots[l].Length; s++)
                    if (!inst.def.layers[l].instances[s].loop && inst.slots[l][s].voices.Count > 0) oneShots = true;
            if (!oneShots) return false;
            inst.keyedOff = true;
            for (int l = 0; l < inst.def.layers.Length; l++)
                for (int s = 0; s < inst.slots[l].Length; s++)
                    if (inst.def.layers[l].instances[s].loop) { StopSlot(inst.slots[l][s]); inst.slots[l][s].active = false; }
            return true;
        }

        void SetParam(Instance inst, string name, float value)
        {
            if (string.IsNullOrEmpty(name) || inst.def == null) return;
            var ps = inst.def.parameters;
            for (int i = 0; i < ps.Length; i++) if (string.Equals(ps[i].name, name, StringComparison.OrdinalIgnoreCase)) inst.param[i] = value;
        }

        // ---- per frame ----
        public void Tick(float dt, Vector3 listener)
        {
            int liveEvents = 0, liveVoices = 0;
            listenerAt = listener;
            UpdateChannelGains();
            Reverb.Update(listener, FirstPersonView());
            Virtualize();
            foreach (var inst in instances.Values)
            {
                if (!inst.playing) continue;
                if (ReplayIsolated && inst.wrapper < ReplayWrapperBase) continue;   // (the paused world's instances wait, the replay plays)
                liveEvents++;
                // the max-playbacks ranking mutes a voice (Virtualize); the gain follows over VirtualRamp seconds so that a voice joining or leaving the loudest set does not click
                // (an event on its first tick takes its rank at once: it was never audible before)
                inst.virtGain = !inst.ticked ? (inst.virtualized ? 0f : 1f) : Mathf.MoveTowards(inst.virtGain, inst.virtualized ? 0f : 1f, dt / VirtualRamp);
                inst.ticked = true;
                int chIndex = inst.channelOverride >= 0 ? inst.channelOverride : inst.def.channel == 0 ? (int)ReflexChannel.Effects : inst.def.channel;
                float chGain = channelGain[Mathf.Clamp(chIndex, 0, channelGain.Length - 1)];
                // FMOD_INIT_OCCLUSION_LOWPASS (ChannelSoftware 10076766): no attenuation, a low-pass at 22050 Hz x ((1 - geometry) (1 - direct))^2 (geometry is
                // never set by the game); the lower of it and the event's own low-pass envelope
                const float occGain = 1f;
                float occCutoff = inst.occlusion > 0.001f ? Mathf.Max(10f, OcclusionCutoff * (1f - inst.occlusion) * (1f - inst.occlusion)) : -1f;
                inst.node.transform.position = inst.def.is3d ? inst.position : listener;
                if (inst.fadeRate != 0)
                {
                    inst.fade = Mathf.Clamp01(inst.fade + inst.fadeRate * dt);
                    if (inst.fadeRate > 0 && inst.fade >= 1f) inst.fadeRate = 0;
                    if (inst.stopping && inst.fade <= 0f) { StopVoices(inst); inst.playing = false; continue; }
                }
                bool anyAlive = false;
                for (int l = 0; l < inst.def.layers.Length; l++)
                {
                    var layer = inst.def.layers[l];
                    float v = Norm(inst, layer.param);
                    float vol = 1f, pitch = 1f, spawn = 1f, lowpass = -1f, highpass = -1f, distortion = -1f;
                    foreach (var env in layer.envelopes)
                    {
                        float y = Evaluate(env, Norm(inst, env.param >= 0 ? env.param : layer.param));
                        switch (env.kind)
                        {
                            case "volume": vol *= y; break;
                            case "pitch": pitch *= Mathf.Pow(2f, 4f * (2f * y - 1f)); break;
                            case "spawn": spawn *= (Mathf.Exp(5.78652f * y) - 1f) * 0.3078f; break;
                            case "lowpass": lowpass = Dsp(env, y); break;
                            case "highpass": highpass = Dsp(env, y); break;
                            case "distortion": distortion = Dsp(env, y); break;
                        }
                    }
                    for (int s = 0; s < layer.instances.Length; s++)
                    {
                        var si = layer.instances[s]; var st = inst.slots[l][s]; var sd = inst.def.defs[si.def];
                        bool inRange = layer.param < 0 || (v >= si.start - 1e-6f && v <= si.start + si.length + 1e-6f);
                        float rangeGain = 1f;
                        if (layer.param >= 0 && inRange)
                        {
                            if (si.fadeIn > 0) rangeGain *= Mathf.Clamp01((v - si.start) / si.fadeIn);
                            if (si.fadeOut > 0) rangeGain *= Mathf.Clamp01((si.start + si.length - v) / si.fadeOut);
                        }
                        if (inRange && !st.active) { st.active = true; st.fired = false; st.spawnTimer = 0; }
                        if (!inRange && st.active) { st.active = false; if (si.loop) StopSlot(st); }
                        bool spawns = !si.loop && (sd.spawnMaxMs > 0 || sd.maxSpawned > 1) && sd.spawnMaxMs > 0;
                        if (st.active && !inst.stopping && !inst.keyedOff)
                        {
                            if (si.loop)
                            {
                                if (st.voices.Count == 0 || Done(st.voices[st.voices.Count - 1])) { StopSlot(st); Play(inst, st, sd, loopSingle: Entries(sd) == 1); }
                            }
                            else if (!st.fired) { st.fired = true; Play(inst, st, sd, false); st.spawnTimer = spawns ? NextSpawn(sd, spawn) : 0; }
                            else if (spawns)
                            {
                                st.spawnTimer -= dt;
                                if (st.spawnTimer <= 0) { if (Alive(st) < Mathf.Max(1, sd.maxSpawned)) Play(inst, st, sd, false); st.spawnTimer = NextSpawn(sd, spawn); }
                            }
                        }
                        // voices: mix, filters, reap
                        for (int i = st.voices.Count - 1; i >= 0; i--)
                        {
                            var voice = st.voices[i];
                            if (Done(voice)) { if (!si.loop) { Release(voice); st.voices.RemoveAt(i); } continue; }
                            float g = chGain * occGain * inst.gain * inst.volumeCmd * inst.fade * si.gain * rangeGain * vol * voice.gain;
                            voice.source.volume = inst.muted ? 0f : Mathf.Clamp01(g * inst.virtGain);
                            voice.source.pitch = Mathf.Clamp(inst.pitch * pitch * voice.pitch, 0.05f, 8f);
                            ApplyFilters(voice, occCutoff > 0 ? (lowpass > 0 ? Mathf.Min(lowpass, occCutoff) : occCutoff) : lowpass, highpass, distortion);
                            if (inst.paused && !voice.paused && voice.source.isPlaying) { voice.source.Pause(); voice.paused = true; }
                            else if (!inst.paused && voice.paused) { voice.source.UnPause(); voice.paused = false; }
                        }
                        liveVoices += st.voices.Count;
                        if (si.loop && st.active && !inst.keyedOff) anyAlive = true;
                        if (st.voices.Count > 0 || (spawns && st.active && !inst.keyedOff)) anyAlive = true;
                    }
                }
                // A one-shot event ends when nothing is left to play: tell the game (FMOD EVENT_FINISHED).
                if (!anyAlive && !inst.stopping) { inst.playing = false; Finish(inst.wrapper, inst.serial); }
            }
            ActiveEvents = liveEvents; ActiveVoices = liveVoices; if (liveVoices > PeakVoices) PeakVoices = liveVoices;
        }

        float Norm(Instance inst, int p)
        {
            if (p < 0 || p >= inst.param.Length) return 0f;
            var d = inst.def.parameters[p];
            return d.max > d.min ? Mathf.Clamp01((inst.param[p] - d.min) / (d.max - d.min)) : 0f;
        }

        static float Evaluate(EnvelopeDef e, float v)
        {
            int n = e.x.Length;
            if (n == 0) return 1f;
            if (v <= e.x[0]) return e.y[0];
            for (int i = 1; i < n; i++)
            {
                if (v > e.x[i]) continue;
                float t = e.x[i] > e.x[i - 1] ? (v - e.x[i - 1]) / (e.x[i] - e.x[i - 1]) : 1f;
                if (e.shape[i] == 1) t = t * t * (3f - 2f * t);   // smooth (cubic); 2 = linear; the later point's shape selects the segment
                return e.y[i - 1] + (e.y[i] - e.y[i - 1]) * t;
            }
            return e.y[n - 1];
        }

        static float Dsp(EnvelopeDef e, float y) =>
            e.log && e.dspMin > 0 && e.dspMax > 0 ? e.dspMin * Mathf.Pow(e.dspMax / e.dspMin, y) : e.dspMin + y * (e.dspMax - e.dspMin);

        float NextSpawn(SoundDef sd, float intensity) => Rand(sd.spawnMinMs, sd.spawnMaxMs) / 1000f / Mathf.Max(intensity, 0.01f);

        static int Entries(SoundDef sd) => sd.files.Length;
        static int Alive(SlotState st) { int n = 0; foreach (var v in st.voices) if (!Done(v)) n++; return n; }
        // a voice held by the pause menu (AudioListener.pause, a PausedWithGame channel) reports !isPlaying but has not ended
        static bool Done(Voice v) => v.isNull ? Time.time >= v.nullUntil : v.started && !v.paused && !(AudioListener.pause && !v.source.ignoreListenerPause) && !v.source.isPlaying;

        int Pick(Instance inst, SlotState st, SoundDef sd)
        {
            int n = sd.files.Length;
            if (n <= 1) return 0;
            switch (sd.mode)
            {
                case "sequential_event_restart": return st.seq++ % n;
                case "sequential":
                {
                    string key = inst.def.path + sd.name; sequential.TryGetValue(key, out int i); sequential[key] = i + 1; return i % n;
                }
                case "programmer": return PickProgrammer(inst, n);
                case "shuffle":
                    if (st.shuffle.Count == 0) { for (int i = 0; i < n; i++) st.shuffle.Add(i); for (int i = n - 1; i > 0; i--) { int j = random.Next(i + 1); (st.shuffle[i], st.shuffle[j]) = (st.shuffle[j], st.shuffle[i]); } }
                    { int p = st.shuffle[0]; st.shuffle.RemoveAt(0); return p; }
                default:   // random, random_no_repeat, programmer
                {
                    float total = 0; for (int i = 0; i < n; i++) if (!(sd.mode == "random_no_repeat" && i == st.lastPick)) total += sd.weights[i];
                    float r = (float)random.NextDouble() * total;
                    for (int i = 0; i < n; i++) { if (sd.mode == "random_no_repeat" && i == st.lastPick) continue; r -= sd.weights[i]; if (r <= 0) { st.lastPick = i; return i; } }
                    return st.lastPick = (st.lastPick + 1) % n;
                }
            }
        }

        void Play(Instance inst, SlotState st, SoundDef sd, bool loopSingle)
        {
            if (sd.files.Length == 0) return;
            string file = sd.files[Pick(inst, st, sd)];
            var voice = Get(inst);
            voice.gain = sd.volume * Rand(Mathf.Min(sd.volumeRandMin, 1f), 1f);
            voice.pitch = Mathf.Pow(2f, sd.pitchOctaves + Rand(-sd.pitchRandOctaves, sd.pitchRandOctaves));
            st.voices.Add(voice);
            if (string.IsNullOrEmpty(file) || !clipByFile.TryGetValue(file, out var clip) || !clip)
            {
                voice.isNull = true; voice.nullUntil = Time.time;   // null entry: silent, ends at once
                if (inst.stat != null) inst.stat.nullVoices++;
                return;
            }
            if (inst.stat != null) inst.stat.voices++;
            var src = voice.source;
            src.clip = clip; src.loop = loopSingle;
            src.priority = Mathf.Clamp(inst.def.priority, 0, 256);                          // FMOD event priority (0 = most important, as Unity)
            src.ignoreListenerPause = KeepsPlayingWhenPaused(inst.def.channel);          // UI / VoiceOver play on in the pause menu
            src.spatialBlend = inst.def.is3d ? 1f : 0f;
            OriginalReverb.Route(src, inst.def.is3d);                                       // w-reverb: the 3D events go to the system reverb
            float mnD = inst.minDistance >= 0f ? inst.minDistance : inst.def.minDistance, mxD = inst.minDistance >= 0f ? inst.maxDistance : inst.def.maxDistance;
            src.rolloffMode = AudioRolloffMode.Linear; src.minDistance = mnD; src.maxDistance = Mathf.Max(mxD, mnD + 0.01f);
            src.volume = 0f; src.Play(); voice.started = true;
        }

        Voice Get(Instance inst)
        {
            var v = pool.Count > 0 ? pool.Pop() : null;
            if (v == null)
            {
                var go = new GameObject("voice");
                var s = go.AddComponent<AudioSource>(); s.playOnAwake = false; s.dopplerLevel = 0f;
                v = new Voice { source = s };
            }
            v.source.transform.SetParent(inst.node.transform, false);
            v.source.gameObject.SetActive(true);
            v.started = false; v.isNull = false; v.paused = false;
            return v;
        }

        void Release(Voice v)
        {
            v.source.Stop(); v.source.clip = null; v.started = false; v.isNull = false;
            if (v.lowpass) v.lowpass.enabled = false; if (v.highpass) v.highpass.enabled = false; if (v.distortion) v.distortion.enabled = false;
            v.source.transform.SetParent(transform, false);
            pool.Push(v);
        }

        void StopSlot(SlotState st) { foreach (var v in st.voices) Release(v); st.voices.Clear(); }
        void StopVoices(Instance inst) { if (inst.slots != null) foreach (var layer in inst.slots) foreach (var st in layer) StopSlot(st); }

        static void ApplyFilters(Voice v, float lowpass, float highpass, float distortion)
        {
            if (lowpass > 0) { if (!v.lowpass) v.lowpass = v.source.gameObject.AddComponent<AudioLowPassFilter>(); v.lowpass.enabled = true; v.lowpass.cutoffFrequency = Mathf.Clamp(lowpass, 10f, 22000f); }
            else if (v.lowpass) v.lowpass.enabled = false;
            if (highpass > 0) { if (!v.highpass) v.highpass = v.source.gameObject.AddComponent<AudioHighPassFilter>(); v.highpass.enabled = true; v.highpass.cutoffFrequency = Mathf.Clamp(highpass, 10f, 22000f); }
            else if (v.highpass) v.highpass.enabled = false;
            if (distortion > 0.001f) { if (!v.distortion) v.distortion = v.source.gameObject.AddComponent<AudioDistortionFilter>(); v.distortion.enabled = true; v.distortion.distortionLevel = Mathf.Clamp01(distortion); }
            else if (v.distortion) v.distortion.enabled = false;
        }

        float Rand(float a, float b) => a + (float)random.NextDouble() * (b - a);

        public void StopAll() { foreach (var inst in instances.Values) { StopVoices(inst); inst.playing = false; } }
    }
}
