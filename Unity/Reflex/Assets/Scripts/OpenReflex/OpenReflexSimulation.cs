using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

namespace Reflex.Port.RxCore
{
    // Runs OPEN REFLEX core frames (librxcore) on a dedicated thread so rendering never waits for the simulation. Same threading and
    // result contract as OriginalWorld/OriginalSimulation (read that file first): the main thread submits input every frame; when the
    // simulation is busy the input (and the time passed) is queued and the next frame starts as soon as the current one ends;
    // completed results wait in `ready` until collected, and one not collected before the next completes is merged into it (terrain
    // patches accumulate, the game time adds up). Only the worker thread calls into the core while it runs.
    //
    // Differences from OriginalSimulation:
    //  - The worker owns the whole session: create, event load, the frames, destroy. The constructor returns at once; Phase says where
    //    the startup is (Starting -> Ready, or Failed), and every Results carries SessionState (Loading while the core runs the
    //    event's load states, then Countdown / Racing / Finished). State is read only once the world is up.
    //  - Errors are results, not exceptions: a failed create / load / frame (including a core abort at an unported game function,
    //    which the API turns into the error state instead of killing the process) sets Results.error and Phase.Failed; the worker
    //    then stops calling the core (it only reports) until Dispose.
    //  - Only what rx_api.h exports is collected. Audio has no equivalent yet (notes/unity-api-surface.md); its Results field is
    //    absent, not empty. Ambient entities, the scene's animated objects, node flags and the tyre-track ribbons ARE exported
    //    (unityscene / tyretracks) and collected when the scene asks (AttachAmbient / TrackObjects / trackTerrain).
    //  - Native types are the Rx* structs of this assembly (no dependency on the Assembly-CSharp ones); an adapter to the old
    //    Reflex* result types belongs in the project assembly.
    public sealed partial class OpenReflexSimulation : IDisposable
    {
        // elapsed milliseconds since a Stopwatch.GetTimestamp() (no Stopwatch object a frame, w-gcspikes)
        static double Ms(long since) => (System.Diagnostics.Stopwatch.GetTimestamp() - since) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        public enum Phase { Starting = 0, Ready = 1, Failed = 2, Stopped = 3 }

        // Immutable managed startup data, captured only by the session owner. Unity consumes it on its main thread.
        public sealed class StartupData
        {
            public RxSlot[] slots;
            public RxCameraBindings cameraBindings;
            public RxAppearance[] appearances;
            public RxTerrainInfo terrainInfo;
            public float[] heights, deformation;
        }
        readonly bool prepareStartup;
        StartupData startupData;
        bool startupCaptured;
        string pacingMetadata;
        readonly int[] pacingTuning = new int[8];
        readonly RxSlot[] pacingSlots = new RxSlot[RxRiderCap.Max];
        public StartupData TakeStartupData() { lock (gate) { var data = startupData; startupData = null; return data; } }
        /// The startup snapshot is captured and not taken yet (the event's load states are over: the world exists).
        public bool StartupReady { get { lock (gate) return startupData != null; } }
        /// Diagnostics (the load arena's timing lines, w-arenafast): the worker's startup in ms, -1 until done: the session's create (with the
        /// profile's import), the event's load (or the test drive's start), and the load states' frames until the startup snapshot.
        public double StartupCreateMs { get; private set; } = -1;
        public double StartupLoadMs { get; private set; } = -1;
        public double StartupFramesMs { get; private set; } = -1;
        readonly Stopwatch startupClock = Stopwatch.StartNew();
        double startupLoadedAt = -1;
        // The Heat format re-creates its riders between the rounds (the AAA loading page: the session is Loading while the core rebuilds
        // the activity area, MXGame states 9 / 10): the roster of the next round, captured once the session runs again.
        public sealed class RosterData { public RxSlot[] slots; public RxAppearance[] appearances; }
        RosterData rosterData;
        bool rosterDirty;
        public RosterData TakeRosterData() { lock (gate) { var data = rosterData; rosterData = null; return data; } }

        public struct TerrainPatch { public int x0, z0, w, h; public float[] heights, deform; }

        public const int RiderBoneCount = 69;
        public const int MaxSlotNodes = 64;
        public const int MaxStandings = RxRiderCap.Max;   // RIDER CAP: every rider's standing (was 16)
        public const int MaxMud = RxRiderCap.Max + 1;   // [0] the local player, [1 + i] the tracked slots
        public const int MaxParticles = 6000;   // 3000 drawn per manager (the original's cap)
        public const int MaxRaceLine = 64;
        public const int MaxAmbientEntities = 4096;   // the old plugin's OriginalAmbient.MaxEntities
        public const int MaxAmbientNodeFlags = 16384, MaxScenarioModels = 4096, MaxCrowdFlashes = 2048;
        public const int MaxPodiumRiders = RxRiderCap.Max;   // the podium RiderCharacters: one per player (G+0x1541c; RIDER CAP, was 16)
        public const int MaxAmbientMatrices = 16384;  // the old plugin's OriginalAmbient.MaxMatrices
        public const int MaxTrackTriangles = 65536;   // the old plugin's MaxTrackTriangles (18 floats each)
        public const int MaxTrackRibbons = 72;        // 3 rings x 24 strips

        public sealed class Results
        {
            public ReplaySnapshot replay;                              // immutable camera/fade paired with this collected pose frame
            // The post-race presentation's cast (rx_podium_api.h, w-podium): collected while the activity is in Post. podiumRiderCount
            // podium RiderCharacters (their bones in podiumMatrices), podiumSlotCount slots' live rider / vehicle drawn flags.
            public bool hasPodium; public RxPodiumInfo podium;
            public RxPodiumRider[] podiumRiders = new RxPodiumRider[MaxPodiumRiders]; public int podiumRiderCount;
            public float[] podiumMatrices = new float[MaxPodiumRiders * 69 * 16];
            public RxPodiumSlot[] podiumSlots = new RxPodiumSlot[RxRiderCap.Max]; public int podiumSlotCount;
            public RxSessionState sessionState;                      // the core's state after this result's frames
            public RxBikeState bike; public bool hasBike;
            public RxOnboardState onboardState;
            public bool raceLineClosed;
            public float[] riderPose = new float[RiderBoneCount * 16]; public int riderBones;
            public RxRaceState race; public bool hasRace;
            public RxHudState hud; public bool hasHud;
            public RxWaypointState waypoint; public bool hasWaypoint;
            public RxPowercrossHudState powercross; public bool hasPowercross;
            public RxActivationState activation; public bool hasActivation;   // the Free Ride activation area (rx_session_activation)
            public RxChallengeState challenge; public bool hasChallenge;      // the challenge activity's medal state (rx_session_challenge)
            public RxCompassState compass; public bool hasCompass;            // the HUD compass' markers (rx_session_compass)
            public RxRaceFormatState raceFormat; public bool hasRaceFormat;   // the race format's HUD values (rx_session_race_format)
            public RxRaceHudState raceHud; public bool hasRaceHud;            // the Eliminator / Outrun / Section Race / Freestyle components' values (rx_session_race_hud)
            public RxModeResultsState modeResults; public bool hasModeResults;   // the Waypoint / Freestyle / Section Race standings' result values (rx_session_mode_results)
            public RxStuntHudState stuntHud; public bool hasStuntHud;         // the stunt feedback overlay's trick names (rx_session_stunt_hud)
            public RxReflexHudState reflexHud; public bool hasReflexHud;      // the wreck-recovery arrows (rx_session_reflex_hud)
            public readonly RxVehicleAudioState vehicleAudio = new RxVehicleAudioState();   // the vehicle sound layer's telemetry and cues (rx_session_vehicle_audio, w-audio)
            public readonly RxSoundState sound = new RxSoundState();                        // the other sound managers' input (rx_session_sound, w-sfx)
            public RxEventUiState eventUi;
            public float[] raceLine = new float[3 * MaxRaceLine]; public int raceLinePoints;
            public RxVehicleInfo vehicle; public bool hasVehicle;
            public RxDriverResult[] drivers = new RxDriverResult[MaxStandings]; public int driverCount;
            public RxResultsPage resultsPage;                                  // the results page's table once the page started (rx_session_results_page), else null
            public float[] bikePose = new float[MaxSlotNodes * 16]; public int bikeNodes;
            public RxCamera camera; public bool hasCamera;
            public RxTerrainInfo terrainInfo; public bool hasTerrainInfo;
            public readonly List<TerrainPatch> terrain = new List<TerrainPatch>();   // the first one is the whole grid (rx_terrain_info)
            public RxParticleInfo particleInfo; public bool hasParticles;
            public RxParticle[] particles = new RxParticle[MaxParticles]; public int particleCount;
            public RxAmbientInfo ambient; public bool hasAmbient;
            // The ambient animated entities of this frame (when AttachAmbient was called): the entity records and the node
            // matrices they index (first_matrix / bones). Same data the old plugin's reflex_world_ambient handed over.
            public RxAmbientEntity[] ambientEntities = new RxAmbientEntity[MaxAmbientEntities]; public int ambientEntityCount;
            public float[] ambientMatrices = new float[MaxAmbientMatrices * 16]; public int ambientMatrixCount;
            // The scenario actors' node flags (rx_session_ambient_node_flags, bit 0x10 hidden: SetNodeVisibility, e.g. the sky crane box's
            // petals): ambientNodeFlagCount[i] bytes from ambientNodeFlagStart[i] for ambient entity i (0: none read).
            public int[] ambientNodeFlagStart = new int[MaxAmbientEntities], ambientNodeFlagCount = new int[MaxAmbientEntities];
            public byte[] ambientNodeFlags = new byte[MaxAmbientNodeFlags]; public int ambientNodeFlagBytes;
            // The stands crowd's active camera flashes (rx_session_crowd_flashes; OriginalAmbient draws them with the CameraFlash programs).
            public RxCrowdFlash[] crowdFlashes = new RxCrowdFlash[MaxCrowdFlashes]; public int crowdFlashCount;
            // The static scenery models as the game draws them (rx_session_scenario_models: drawn flag, instance matrix).
            public RxScenarioModel[] scenarioModels = new RxScenarioModel[MaxScenarioModels]; public int scenarioModelCount; public bool hasScenarioModels;
            // The scenario scripts' sound effects of this frame (rx_session_scenario_sounds: the arrival's vehicle, the pilot, the crate).
            public RxScenarioSound[] scenarioSounds = new RxScenarioSound[16]; public int scenarioSoundCount;
            // Animated scene objects (TrackObjects): one world matrix per tracked entry; valid[i] false when not found.
            public float[] objects = new float[0]; public bool[] objectsValid = new bool[0];
            // A vehicle model's node flags: the game's record +0xc bytes (bit 0x10 hidden: the damage panels).
            public byte[] bikeNodeFlags = new byte[MaxSlotNodes]; public int bikeNodeFlagCount;
            public byte[][] slotNodeFlags = new byte[0][]; public int[] slotNodeFlagCounts = new int[0];
            // The live tyre-track ribbons (TerrainTracks): 18 floats per triangle (3 vertices * x, z, side, V, intensity, type),
            // the per-strip descriptors and the material/format block. Same data the old reflex_world_tracks handed over.
            public float[] tracks; public int trackTriangles; public bool tracksChanged;
            public RxTrackInfo trackInfo;
            public RxTrackRibbon[] trackRibbons = new RxTrackRibbon[MaxTrackRibbons]; public int trackRibbonCount;
            public RxMudState[] mud = new RxMudState[MaxMud]; public bool[] mudValid = new bool[MaxMud]; public int mudCount;
            // The other riders of the race (tracked slots): bike model nodes and the riding rider's 69 bones; riderBones 0 = not read.
            public int slotCount; public int[] slots = new int[0];
            public float[][] slotBikePose = new float[0][], slotRiderPose = new float[0][];
            public int[] slotBikeNodes = new int[0], slotRiderBones = new int[0];
            public double slotMilliseconds;
            public float dt;                                         // time this result's frames covered (the caller's clock)
            public string error;                                     // null = fine; see Phase
            public double milliseconds;                              // the whole job on the worker, exports included (not the frame-cap wait)
            public double capWaitMilliseconds;                       // RealClock: the wait for the game's 1/60 s frame cap before the frame
            public double frameMilliseconds;                         // the core frame(s) alone
            // Presentation timeline (same meaning as OriginalSimulation.Results): the virtual clock this result belongs to = the total
            // time submitted up to and including it; frames run for it (> 1: back to back; merged: one frame of several steps).
            public double simTime; public int framesRun; public bool merged;
            public double inputClock;                                // Clock when the input this result ran with was read
            // Opt-in pacing diagnostics. Stopwatch clocks are scheduling observations, not photon latency.
            public double frameStartClock, collectionClock;
            public string appliedPadCsv, corneringMetadata;

            public float gameDt, gameTime;                           // the game's clock after it: rx_session_game_time, and its change per call
            public ulong sessionFrames;                              // rx_session_frames
            // LAP LOG (rx_session_lap_log, w-lb-practice): the local human's laps the core logged in this result's frames (oldest first; a merged
            // result keeps the older ones too), and the local vehicle's physics class (rx_session_local_phys_class, -1 unknown)
            public readonly List<RxLapRecord> laps = new List<RxLapRecord>();
            public int physClass = -1;
        }

        readonly string installRoot;
        readonly RxEventSpec spec;
        int loadedRiders = -1;   // RIDER CAP: the riders the event was loaded with (spec.riders, or 12 when the core had no big field)
        readonly string physicsText, physicsName;   // PHYSICS STUDIO: the player's config for this event (RxPhysicsStudio at construction; "" = the original)
        /// ALIVE CLASSES: the models the player rides as a bike with physics of its own (BikeMods.OwnPhysics at construction; empty: none)
        readonly List<(int model, int cls, byte[] file, string name, string nodes)> ownPhysics;
        readonly bool physicsLive;                  // PHYSICS STUDIO: the in-race editor may change this event (the core keeps the file values)
        /// PHYSICS STUDIO: where this session rides (RxPhysicsStudio.AllowedHere).
        public PhysicsContext PhysicsContext { get; }
        readonly float mapScale;     // MAP SCALE: the scale this session's track is made at (RxMapScale.Current at construction; 1 off)
        readonly float mapScaleBase; // BASE SCALE: the track's default scale (RxMapScale.Base at construction; 1 for the game's tracks)
        /// The scale the core made the track at (rx_session_map_scale), 1 before the event loads or without one.
        public float MapScale { get; private set; } = 1f;
        readonly bool practice;      // PRACTICE: this session's lap race is endless (RxPracticeMode.Current at construction, a lap race only)
        /// The core made this session a practice session (rx_session_practice after the set; false before the event loads or without one).
        public bool Practice { get; private set; }
        readonly bool inCareerMode;   // the Race & Ride challenges Free Ride (and its challenges): UI InCareerMode, before the load
        readonly bool preChallengeHold;   // a challenge (EventTypeActivation): the PreChallengePopup holds WaitForUI from before the load
        volatile bool preChallengeHeld;
        readonly CareerLaunch careerLaunch;
        public CareerWorker Career { get; }
        public ReplayWorker Replay { get; }
        readonly RxProfileCell[] profile;
        readonly PlayerSessionCheckpoint playerCheckpoint;
        readonly bool persistPlayer;
        /// The garage's TEST DRIVE (RxTestDriveNative): the session starts it instead of loading the spec's event, ends it before it is
        /// destroyed (the pause request's end, MXGarage_TestDriveEnd) and saves nothing (the profile was only read).
        readonly bool testDrive;
        public bool TestDrive => testDrive;
        /// The MotoSkills tutorials (RxTutorialNative, w-tutorials): the session starts the tutorial mode in its UI world instead of loading the
        /// spec's event (rx_session_tutorial_start), ends it before it is destroyed (EnableTutorialMode( FALSE )) and saves the profile as a ride
        /// does (PlayerRider.TutorialsCompleted is the game's own write: 004e8460).
        readonly bool tutorial;
        public bool Tutorial => tutorial;
        /// The tutorial's state after the last job (the worker's copy; null before the first) and the lessons' objects (refreshed every job).
        public volatile RxTutorialState TutorialSnapshot;
        public volatile RxTutorialObject[] TutorialObjects;
        readonly RxTutorialState tutorialRead = new RxTutorialState();
        readonly RxTutorialObject[] tutorialObjectsRead = new RxTutorialObject[64];
        /// The pages' calls into the tutorial (on the worker before its next job): App.EnableTutorial / DisableTutorial / PauseActiveWorld,
        /// RetryTutorial, the popup stack, SuspendPlayerInput, EnableTutorialMode( FALSE ). `done` (on the worker) gets the call's result.
        public void TutorialCommand(string what, int value = 0, Action<int> done = null)
        {
            BetweenFrames(s =>
            {
                int r = -1;
                try
                {
                    switch (what)
                    {
                        case "lesson": r = RxTutorialNative.Lesson(s, value); break;
                        case "disable": r = RxTutorialNative.Disable(s); break;
                        case "pause": r = RxTutorialNative.Pause(s, value != 0); break;
                        case "retry": r = RxTutorialNative.Retry(s); break;
                        case "popup": r = RxTutorialNative.Popup(s, value != 0); break;
                        case "suspend": r = RxTutorialNative.SuspendInput(s, value != 0); break;
                        case "end": r = RxTutorialNative.End(s); break;
                    }
                    if (r != 0) Log("tutorial " + what + " " + value + ": " + Clean(RxApiNative.rx_session_last_error(s)));
                    if (RxTutorialNative.Read(s, tutorialRead) == 0) TutorialSnapshot = tutorialRead.Clone();
                }
                catch (EntryPointNotFoundException) { Log("this librxcore has no MotoSkills tutorials (rx_session_tutorial_*): rebuild the core"); }
                done?.Invoke(r);
            });
        }
        readonly int[] remap;
        readonly bool trackTerrain;
        readonly Thread thread;
        readonly AutoResetEvent go = new AutoResetEvent(false);
        readonly bool pacingDiagnostics = !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_PACING_LOG"));
        readonly object gate = new object();
        volatile bool stopping, busy;
        volatile int phase;                    // Phase
        volatile int stateValue = (int)RxSessionState.Loading;
        string failure;                        // guarded by gate
        IntPtr session;                        // the worker's only

        // one session per core image: the core has one global arena per loaded image (the main image's count is activeSessions, the
        // arena image's activeArenaSessions: RxNativeLoader.LoadArena, the test-ride arena while an event loads)
        static int activeSessions, activeArenaSessions;
        static readonly object sessionOwnership = new object();
        static string arenaFailure, arenaImageFailure;
        /// This session runs in the arena image (RxNativeLoader.LoadArena) on its worker: the test-ride arena while an event loads. It
        /// touches none of the client-wide session state the event's own session owns (PHYSICS STUDIO's event values).
        public bool ArenaImage => arenaImage;
        readonly bool arenaImage;

        // job (the frames running now) and queued (what arrived meanwhile), as in OriginalSimulation
        float jobDt; readonly byte[] jobKeys = new byte[256]; readonly RxPad[] jobPads = new RxPad[4]; uint jobMask;
        readonly byte[] queuedKeys = new byte[256]; readonly RxPad[] queuedPads = new RxPad[4]; uint queuedMask;
        Results front = new Results(), back = new Results(), ready = new Results();
        bool readyValid, queued, commandOnly, commandWake; float queuedDt;
        int jobFrames = 1, queuedFrames; double requested, jobStamp, queuedStamp, jobInput, queuedInput; bool owned;
        readonly Stack<Results> pool = new Stack<Results>();
        double frameMsAverage = 2;
        double lastStart = double.NaN;
        float previousGameTime = float.NaN;

        /// The game's own clock (RealClock): each frame measures its dt when it starts: the real time since the last frame started,
        /// after waiting until at least the frame cap 1/60 s has passed; the dt Submit passes is unused.
        public bool RealClock;
        public const double FrameCap = 1.0 / 60.0;
        public const int MaxSeparateFrames = 4, MaxMergedSteps = 3, MaxQueuedFrames = 15;
        public double LastFrameStart { get { lock (gate) return lastStart; } }
        public void ResetClock() { lock (gate) lastStart = double.NaN; }
        public static double Clock => Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency;

        /// Messages that are not results (worker thread). Set to UnityEngine.Debug.LogWarning from the project; default: System.Diagnostics.Debug.
        public static Action<string> Log = m => Debug.WriteLine("[OpenReflex] " + m);

        public Phase CurrentPhase => (Phase)phase;
        /// Why Phase is Failed (null otherwise).
        public string Failure { get { lock (gate) return failure; } }
        /// The core's state as of the last finished job.
        public RxSessionState State => (RxSessionState)stateValue;
        public bool Idle => !busy;

        // Riders tracked for the opponents (set from the main thread; read once per job).
        volatile int[] trackedSlots = new int[0];
        volatile bool[] slotRiderNear = new bool[0];
        int slotFrame;
        public int farRiderInterval = 3;
        public void TrackSlots(int[] slots) { trackedSlots = slots ?? new int[0]; slotRiderNear = new bool[trackedSlots.Length]; for (int i = 0; i < slotRiderNear.Length; i++) slotRiderNear[i] = true; }
        public void SetRiderDetail(bool[] near) { if (near != null && near.Length == trackedSlots.Length) slotRiderNear = near; }

        readonly ConcurrentQueue<Action<IntPtr>> betweenFrames = new ConcurrentQueue<Action<IntPtr>>();
        /// Runs `call` (given the session) on the worker before its next job. Not while the session failed.
        public void BetweenFrames(Action<IntPtr> call) { if (call != null) betweenFrames.Enqueue(call); }

        /// The challenge page's RESTART (App.RestartActivity -> rx_session_challenge_restart) on the worker before its next job; `done` (on the
        /// worker) gets the call's result. The world and the session keep running: the challenge starts again from its Pre state.
        public void RestartChallenge(Action<int> done = null)
        {
            BetweenFrames(s =>
            {
                int result = -1;
                try { result = RxChallengeNative.Restart(s); }
                catch (EntryPointNotFoundException) { Log("rx_session_challenge_restart is not exported by this librxcore; rebuild the core"); }
                done?.Invoke(result);
            });
        }

        /// The pause page's RESTART of a career event (PauseMenu:OnRestartEventPopupResult: App.RestartActivity -> rx_session_restart_activity) on the
        /// worker before its next job; `done` (on the worker) gets the call's result. The world, the session and the career keep running: the
        /// event's activity starts again from its Pre state, and nothing of the aborted attempt reaches the series.
        public void RestartActivity(Action<int> done = null)
        {
            BetweenFrames(s =>
            {
                int result = -1;
                try { result = RxChallengeNative.RestartActivity(s); }
                catch (EntryPointNotFoundException) { Log("rx_session_restart_activity is not exported by this librxcore; rebuild the core"); }
                done?.Invoke(result);
            });
        }

        /// CONTROLS in a race (w-fb-hud): the saved bindings (CoreRemapping's 38 values) into this session at once (rx_session_remap_apply: the
        /// table and the local human's KeyMaps), on the worker before its next job; `done` (on the worker) gets the result (0 applied).
        public void ApplyRemap(int[] values, Action<int> done = null)
        {
            if (values == null || values.Length != 38) { done?.Invoke(-1); return; }
            var copy = (int[])values.Clone();
            BetweenFrames(s =>
            {
                int result = -1;
                try { result = RxApiNative.rx_session_remap_apply(s, copy); }
                catch (EntryPointNotFoundException) { Log("rx_session_remap_apply is not exported by this librxcore; rebuild the core"); }
                done?.Invoke(result);
            });
        }

        /// RESET TRACK (w-fb-hud): the game's own level reset of the terrain (the ruts and the tyre tracks back to the untouched track) at the
        /// next frame, on the worker before its next job; `done` (on the worker) gets the result (0 armed).
        public void ResetTrack(Action<int> done = null)
        {
            BetweenFrames(s =>
            {
                int result = -1;
                try { result = RxApiNative.rx_session_terrain_reset(s); }
                catch (EntryPointNotFoundException) { Log("rx_session_terrain_reset is not exported by this librxcore; rebuild the core"); }
                done?.Invoke(result);
            });
        }

        /// The challenge page's prize (PostChallengePopup:Start: rx_session_challenge_payout) on the worker before its next job; `done` (on the
        /// worker) gets the call's result and the payout copy.
        public void PayoutChallenge(int seriesId, Action<int, RxChallengePayout> done)
        {
            BetweenFrames(s =>
            {
                int result = -1;
                var value = RxChallengePayout.Create();
                try { result = RxChallengeNative.Payout(s, seriesId, ref value); }
                catch (EntryPointNotFoundException) { Log("rx_session_challenge_payout is not exported by this librxcore; rebuild the core"); }
                done?.Invoke(result, value);
            });
        }

        /// PreChallengePopup is up: the core holds the challenge in Pre (WaitForUI = 1) until ReleasePreChallenge.
        public bool PreChallengeHeld => preChallengeHeld;
        /// PreChallengePopup:End: WaitForUI = 0, on the worker before its next job; `done` (on the worker) gets the call's result.
        public void ReleasePreChallenge(Action<int> done = null)
        {
            BetweenFrames(s =>
            {
                int result = -1;
                try { result = RxChallengeNative.WaitForUi(s, false); }
                catch (EntryPointNotFoundException) { Log("rx_session_wait_for_ui is not exported by this librxcore; rebuild the core"); }
                if (result == 0) preChallengeHeld = false;
                done?.Invoke(result);
            });
        }

        // 1 = ambient entity export on (AttachAmbient). Off by default: the old path only collected it when a scene asked.
        volatile bool ambientOn;
        public void AttachAmbient() => ambientOn = true;

        // ---- the game's UI settings (include/rx_ui_settings.h), the mouse and the pads' motors
        /// The client's choices of the game's own UI settings (name, value), as the options pages keep them: every new session applies them
        /// after it is created and BEFORE its event loads (the players' KeyMaps read handedness / motion / preset when they are created).
        /// Set by the client on the main thread (Reflex.Port.Options.ReflexClientLive.SyncGameSettings); replaced as a whole, never edited.
        public static System.Collections.Generic.KeyValuePair<string, int>[] GameUiSettings;
        System.Collections.Generic.KeyValuePair<string, int>[] uiSettings;
        /// The setting written on the worker before its next job (the options pages' live change in a race).
        public void SetUiSetting(string name, int value)
        {
            BetweenFrames(s =>
            {
                try { if (RxApiNative.rx_session_ui_setting(s, name, value, 1, out _) == 2) Log("UI setting " + name + " is not known to this core"); }
                catch (EntryPointNotFoundException) { Log("rx_session_ui_setting is not exported by this librxcore; rebuild the core"); }
            });
        }
        /// AUTO LEVEL and COUNTER-STEER SNAP (OPTIONS > OPEN REFLEX > RIDING; core rx_session_set_air_helpers): how much of the original's two in-air
        /// helpers the local player keeps, 0 .. 1 each (1 = the original's, the default; 0 = removed; in between the helper turns the bike back that much
        /// slower). The options set them (ReflexClientLive.ApplyFreeAir); every new session takes them before its event loads, a running one at its next
        /// frame (SetFreeAir). An older core without the call gets the same as rx_session_set_free_air's bits (a strength of 0 = the bit).
        public static volatile float GameAirAutoLevel = 1f, GameAirSnap = 1f;
        bool hasAirHelpersExport = true, hasFreeAirExport = true;
        void ApplyAirHelpers(IntPtr s, float level, float snap)
        {
            if (hasAirHelpersExport)
            {
                try { if (RxApiNative.rx_session_set_air_helpers(s, level, snap) != 0) Log("AIR HELPERS " + level + " / " + snap + " refused: " + Clean(RxApiNative.rx_session_last_error(s))); return; }
                catch (EntryPointNotFoundException) { hasAirHelpersExport = false; }
            }
            if (!hasFreeAirExport) return;
            int mode = (snap <= 0f ? RxApiNative.FreeAirNoHold : 0) | (level <= 0f ? RxApiNative.FreeAirNoSettle : 0);
            try { if (RxApiNative.rx_session_set_free_air(s, mode) != 0) Log("FREER AIR " + mode + " refused: " + Clean(RxApiNative.rx_session_last_error(s))); }
            catch (EntryPointNotFoundException) { hasFreeAirExport = false; if (mode != 0) Log("rx_session_set_free_air is not exported by this librxcore; the air helpers need the current core"); }
        }
        /// AIR LOOSENESS (OPTIONS > OPEN REFLEX; core rx_session_set_free_air_looseness): 0 .. 1, 0 = off (the default).
        public static volatile float GameFreeAirLooseness;
        bool hasLoosenessExport = true;
        void ApplyFreeAirLooseness(IntPtr s, float looseness)
        {
            if (!hasLoosenessExport) return;
            try { if (RxApiNative.rx_session_set_free_air_looseness(s, looseness) != 0) Log("AIR LOOSENESS " + looseness + " refused: " + Clean(RxApiNative.rx_session_last_error(s))); }
            catch (EntryPointNotFoundException) { hasLoosenessExport = false; if (looseness > 0f) Log("rx_session_set_free_air_looseness is not exported by this librxcore; AIR LOOSENESS needs the current core"); }
        }
        /// The air options changed in a race: the worker applies them before its next job (the next airborne step uses them).
        public void SetFreeAir(float autoLevel, float snap, float looseness) => BetweenFrames(s => { ApplyAirHelpers(s, autoLevel, snap); ApplyFreeAirLooseness(s, looseness); });
        /// FREE RIDE WITHOUT BORDERS (OPTIONS > OPEN REFLEX; core rx_session_set_free_ride_borders): 1 (the option's default) = in Free Ride no
        /// reset zones, no reset triggers but the deep-water ones, no cannonball (white-out) band, the terrain's edge only as a fall net (collision
        /// is the original's: w-dfcollide); 0 = the original's borders. Every new session takes it before its event loads; a running one gets it
        /// at its next frame (SetFreeRideBorders). Races, the career events, the challenges,
        /// MotoSkills and the test ride never change (the core's gate).
        public static volatile int GameFreeRideBorders = RxApiNative.FreeRideBordersOpen;
        bool hasBordersExport = true;
        void ApplyFreeRideBorders(IntPtr s, int mode)
        {
            if (!hasBordersExport) return;
            try { if (RxApiNative.rx_session_set_free_ride_borders(s, mode) != 0) Log("FREE RIDE BORDERS " + mode + " refused: " + Clean(RxApiNative.rx_session_last_error(s))); }
            catch (EntryPointNotFoundException) { hasBordersExport = false; if (mode != 0) Log("rx_session_set_free_ride_borders is not exported by this librxcore; FREE RIDE WITHOUT BORDERS needs the current core"); }
        }
        public void SetFreeRideBorders(int mode) => BetweenFrames(s => ApplyFreeRideBorders(s, mode));

        /// WEATHER GRIP (Rendering/ReflexWeather.GripFactor; core rx_session_set_weather_grip): rain and snow take grip off every tyre of the
        /// event (every rider, every class). The weather writes it every frame; the worker hands a changed value to the core before its next
        /// frame. 1 (CLEAR, no weather drawn) is the core untouched.
        public static volatile float GameWeatherGrip = 1f;
        float appliedWeatherGrip = 1f;   // (a new session's core starts at 1)
        bool hasWeatherGripExport = true;
        void ApplyWeatherGrip(IntPtr s)
        {
            float g = GameWeatherGrip;
            if (!hasWeatherGripExport || g == appliedWeatherGrip || float.IsNaN(g)) return;
            try { if (RxApiNative.rx_session_set_weather_grip(s, g) != 0) Log("WEATHER GRIP " + g + " refused: " + Clean(RxApiNative.rx_session_last_error(s))); appliedWeatherGrip = g; }
            catch (EntryPointNotFoundException) { hasWeatherGripExport = false; if (g != 1f) Log("rx_session_set_weather_grip is not exported by this librxcore; rain / snow grip needs the current core"); }
        }

        /// SPAWN MARKER (OPTIONS > OPEN REFLEX > SPAWN MARKERS; core rx_session_spawn_at): the local rider through the game's own reset to a spot,
        /// on the worker before its next job. `done` (on the worker) gets the core's answer (0 ok, 2 not a Free Ride, 3 not now; -1 an old core).
        /// instant (w-markerbtns; rx_session_spawn_at_instant): the same reset at once, without the reset key's 0.5 s white fade (an older core: the fade).
        public void SpawnAt(float px, float py, float pz, float dx, float dz, Action<int> done = null, bool instant = false)
        {
            BetweenFrames(s =>
            {
                int rc = -1;
                bool ran = false;
                if (instant && hasSpawnInstant)
                {
                    try { rc = RxApiNative.rx_session_spawn_at_instant(s, px, py, pz, dx, dz); ran = true; }
                    catch (EntryPointNotFoundException) { hasSpawnInstant = false; Log("rx_session_spawn_at_instant is not exported by this librxcore; the marker respawn keeps the reset's fade"); }
                }
                if (!ran)
                {
                    try { rc = RxApiNative.rx_session_spawn_at(s, px, py, pz, dx, dz); }
                    catch (EntryPointNotFoundException) { rc = -1; Log("rx_session_spawn_at is not exported by this librxcore; SPAWN MARKERS need the current core"); }
                }
                done?.Invoke(rc);
            });
        }
        volatile bool hasSpawnInstant = true;

        /// RT_Bump cascade layout (w-rtbumpfit; core rx_session_rtbump_fit, the game's own RTBump_FitCascadeMatrices 007ba8d0): the renderer posts its
        /// camera, the worker fits it between frames, the renderer takes the latest result (Rendering/ReflexRTBump.cs). One request in flight at a time.
        public static volatile bool RTBumpFitAvailable = true;
        readonly object rtbumpGate = new object();
        bool rtbumpPending;
        float[] rtbumpResult; int rtbumpResultSerial;
        public void RequestRTBumpFit(float[] pos, float[] fwd, float[] up, float fovRad, float aspect, float near)
        {
            if (!RTBumpFitAvailable) return;
            lock (rtbumpGate) { if (rtbumpPending) return; rtbumpPending = true; }
            BetweenFrames(s =>
            {
                var o = new float[12]; int rc;
                try { rc = RxApiNative.rx_session_rtbump_fit(s, pos, fwd, up, fovRad, aspect, near, o); }
                catch (EntryPointNotFoundException) { rc = -1; RTBumpFitAvailable = false; Log("rx_session_rtbump_fit is not exported by this librxcore; RT_Bump uses its stand-in fit"); }
                lock (rtbumpGate) { rtbumpPending = false; if (rc == 0) { rtbumpResult = o; rtbumpResultSerial++; } }
            });
        }
        /// The latest RT_Bump fit (null before the first); serial counts the results.
        public float[] LatestRTBumpFit(out int serial) { lock (rtbumpGate) { serial = rtbumpResultSerial; return rtbumpResult; } }

        /// The RT_Bump fit while no game frames run (w-replayterrain: the world paused, the replay viewer, photo mode). RequestRTBumpFit waits for
        /// the next game frame, so the layout stayed the one of the last riding camera while the replay / photo camera flew elsewhere: the terrain
        /// near that camera lost its detail, ruts and tracks (only the riding camera's footprint kept them, with a hard edge). Here the worker is
        /// woken for this fit alone (no game frame, no other between-frame call) and the caller waits up to waitMs for it: the layout of the
        /// camera now, in the same render frame (a camera cut does not show the previous camera's layout). Its own slot: the riding requests and
        /// their result are untouched (the first frame after the viewer closes has the riding camera's result again). Null: not in time / the
        /// worker is busy (the caller uses its stand-in). The result (as LatestRTBumpFit's) is copied into out12.
        public bool RTBumpFitNow(float[] pos, float[] fwd, float[] up, float fovRad, float aspect, float near, double waitMs, float[] out12)
        {
            if (!RTBumpFitAvailable || CurrentPhase != Phase.Ready) return false;
            int want;
            lock (rtbumpGate)
            {
                for (int i = 0; i < 3; i++) { nowArgs[i] = pos[i]; nowArgs[3 + i] = fwd[i]; nowArgs[6 + i] = up[i]; }
                nowArgs[9] = fovRad; nowArgs[10] = aspect; nowArgs[11] = near;
                want = ++nowAsked;
            }
            lock (gate)
            {
                if (stopping || busy) return false;   // (a job runs: frames are coming after all, or a command; the next render frame asks again)
                fitOnly = true; commandOnly = true; busy = true;
            }
            go.Set();
            long until = Stopwatch.GetTimestamp() + (long)(waitMs * Stopwatch.Frequency / 1000.0);
            lock (rtbumpGate)
            {
                while (nowDone < want)
                {
                    long left = until - Stopwatch.GetTimestamp();
                    if (left <= 0 || !Monitor.Wait(rtbumpGate, TimeSpan.FromTicks(Math.Max(1L, left * TimeSpan.TicksPerSecond / Stopwatch.Frequency)))) break;
                }
                if (nowDone < want || !nowOk) return false;
                Array.Copy(nowResult, out12, 12);
                return true;
            }
        }
        readonly float[] nowArgs = new float[12], nowResult = new float[12], nowOut = new float[12];
        readonly float[] nowPos = new float[3], nowFwd = new float[3], nowUp = new float[3];
        int nowAsked, nowDone; bool nowOk, fitOnly;

        // (worker) the fit RTBumpFitNow asked for; returns its serial (published by the loop once the job is complete)
        int RunRTBumpFitNow(IntPtr s)
        {
            int serial; float fov, aspect, near;
            lock (rtbumpGate)
            {
                serial = nowAsked;
                for (int i = 0; i < 3; i++) { nowPos[i] = nowArgs[i]; nowFwd[i] = nowArgs[3 + i]; nowUp[i] = nowArgs[6 + i]; }
                fov = nowArgs[9]; aspect = nowArgs[10]; near = nowArgs[11];
            }
            int rc;
            try { rc = RxApiNative.rx_session_rtbump_fit(s, nowPos, nowFwd, nowUp, fov, aspect, near, nowOut); }
            catch (EntryPointNotFoundException) { rc = -1; RTBumpFitAvailable = false; }
            lock (rtbumpGate)
            {
                nowOk = rc == 0;
                if (nowOk) Array.Copy(nowOut, nowResult, 12);
            }
            return serial;
        }

        /// What a new session takes from the client's settings at its start (the game's UI settings, the air helpers, AIR LOOSENESS, FREE RIDE
        /// WITHOUT BORDERS, the controller bindings): a session started earlier with an equal stamp started as a new one would (the load
        /// arena kept warm in the menus, OpenFrontEnd/OpenArenaWarm.cs).
        public static string StartupSettingsStamp(int[] remap)
        {
            var b = new System.Text.StringBuilder(256);
            var list = GameUiSettings;
            if (list != null) foreach (var kv in list) b.Append(kv.Key).Append('=').Append(kv.Value).Append(';');
            b.Append(GameAirAutoLevel.ToString("R", System.Globalization.CultureInfo.InvariantCulture)).Append('/').Append(GameAirSnap.ToString("R", System.Globalization.CultureInfo.InvariantCulture))
             .Append('/').Append(GameFreeAirLooseness.ToString("R", System.Globalization.CultureInfo.InvariantCulture)).Append('/').Append(GameFreeRideBorders).Append(';');
            if (remap != null) foreach (int v in remap) b.Append(v).Append(',');
            return b.ToString();
        }

        void ApplyUiSettings()
        {
            var list = uiSettings; if (list == null) return;
            try
            {
                foreach (var kv in list)
                    if (RxApiNative.rx_session_ui_setting(session, kv.Key, kv.Value, 1, out _) == 2) Log("UI setting " + kv.Key + " is not known to this core");
            }
            catch (EntryPointNotFoundException) { Log("rx_session_ui_setting is not exported by this librxcore; the game's own settings stay at their defaults"); }
        }

        // the mouse (Raw Input counts, +y down): summed by the main thread, handed to the game once before the next job's frames
        int mouseDx, mouseDy, mouseWheel, mouseFlags;
        bool hasMouseExport = true;
        public void AddMouse(int dx, int dy, int wheel, uint buttonFlags)
        {
            System.Threading.Interlocked.Add(ref mouseDx, dx); System.Threading.Interlocked.Add(ref mouseDy, dy); System.Threading.Interlocked.Add(ref mouseWheel, wheel);
            int old, upd; do { old = mouseFlags; upd = old | (int)buttonFlags; } while (System.Threading.Interlocked.CompareExchange(ref mouseFlags, upd, old) != old);
        }
        /// Mouse messages handed to the game in this session (tests).
        public int MouseMessages { get; private set; }

        // the motors the game last sent each pad (Pad_UpdateVibration -> XInputSetState): (left << 16) | right, 0..65535 each
        readonly int[] rumble = new int[4];
        bool hasRumbleExport = true;
        /// The last motor speeds the game sent pad `pad` (0..3): left (low frequency) and right (high frequency), 0..65535.
        public void Motors(int pad, out ushort left, out ushort right) { int v = System.Threading.Volatile.Read(ref rumble[pad & 3]); left = (ushort)(v >> 16); right = (ushort)v; }

        /// Selects one of the four original DrivingCamera presets on the worker before its next job.
        /// Presets0..3 map to native entities0,1,4,2; the older SetCamera API below retains its0/2 semantics.
        /// The game's validation applies (a human local player with a vehicle in an activity in state1..3).
        public void SetDrivingCamera(int preset)
        {
            BetweenFrames(s =>
            {
                try { RxApiNative.rx_session_set_driving_camera(s, preset); }
                catch (EntryPointNotFoundException) { Log("Full camera selection requires the current core build"); }
            });
        }

        /// Selects the local player's view on the worker before its next job, through the game's own camera setter (the one the
        /// shipped Camera key calls): 0 = the default third person, 2 = first person. The game's validation applies (a human local
        /// player with a vehicle in an activity in state 1..3); otherwise nothing changes. Requires a core with
        /// rx_session_set_camera (an older dylib logs once and leaves the camera alone).
        public void SetCamera(int mode)
        {
            BetweenFrames(s =>
            {
                try { RxApiNative.rx_session_set_camera(s, mode); }
                catch (EntryPointNotFoundException) { Log("rx_session_set_camera is not exported by this librxcore; rebuild the core"); }
            });
        }

        // Animated scene objects (the old TrackObjects): vtable + entity id + the matrix offset, found per frame through
        // rx_session_scene_object_matrix (no game address is handed out).
        uint[] objectVtables = new uint[0]; int[] objectIds = new int[0]; uint[] objectOffsets = new uint[0];
        public void TrackObjects(uint[] vtables, int[] ids, uint[] matrixOffsets)
        {
            int n = vtables == null || ids == null || matrixOffsets == null ? 0 : Math.Min(vtables.Length, Math.Min(ids.Length, matrixOffsets.Length));
            objectVtables = new uint[n]; objectIds = new int[n]; objectOffsets = new uint[n];
            for (int i = 0; i < n; i++) { objectVtables[i] = vtables[i]; objectIds[i] = ids[i]; objectOffsets[i] = matrixOffsets[i]; }
        }

        // optional exports, switched off on the first EntryPointNotFoundException (a core built before the export existed)
        bool hasInput = true, hasSlotPoses = true, hasHudExport = true, hasRaceLineExport = true, hasParticleArray = true, hasVehicleExport = true;
        bool hasAmbientExport = true, hasNodeFlags = true, hasSceneObjects = true, hasTracksExport = true;
        uint tracksVersion; int framesSinceTracks;

        [DllImport("/usr/lib/libSystem.dylib")] static extern int pthread_set_qos_class_self_np(int qos, int relativePriority);
        const int QosUserInteractive = 0x21;
        public static bool HighQos = Environment.GetEnvironmentVariable("REFLEX_SIM_QOS") != "default";

        /// RxNativeLoader.Load must have run. The worker starts creating the session at once.
        /// replayPresentation: an exhibition event (no career) gets the same replay presenter as a career event; the caller must then
        /// install its renderer (OwnReplayRenderer), which paints and acknowledges the replay's requests.
        public OpenReflexSimulation(string installRoot, RxEventSpec spec, bool trackTerrain, bool prepareStartup = false, RxProfileCell[] profile = null, int[] remap = null, IPlayerSessionPersistence persistence = null, CareerLaunch career = null, bool replayPresentation = false, bool inCareerMode = false, bool testDrive = false, bool tutorial = false, bool arenaImage = false)
        {
            this.arenaImage = arenaImage && testDrive && career == null && RxNativeLoader.ArenaLoaded;
            this.testDrive = testDrive && career == null;
            this.tutorial = tutorial && career == null && !this.testDrive;
            this.inCareerMode = inCareerMode && career == null;
            // MAP SCALE: an exhibition event only (the career, the test ride, the tutorials and the challenges themselves stay at 100 %; an
            // arcade Free Ride in the challenges' discovery mode is still the arcade's Free Ride: it scales, its challenges start at 100 %)
            mapScale = career == null && !this.testDrive && !this.tutorial && string.IsNullOrEmpty(spec.EventActivation) ? RxMapScale.Current : 1f;
            mapScaleBase = career == null && !this.testDrive && !this.tutorial && string.IsNullOrEmpty(spec.EventActivation) ? RxMapScale.Base : 1f;
            // PRACTICE: an exhibition lap race only (never the career, the test ride, the tutorials or a challenge)
            practice = career == null && !this.testDrive && !this.tutorial && string.IsNullOrEmpty(spec.EventActivation) && RxPracticeMode.Current && RxPracticeMode.LapRace(spec.EventType);
            preChallengeHold = this.inCareerMode && !string.IsNullOrEmpty(spec.EventActivation);
            // PHYSICS STUDIO (RxPhysicsStudio.AllowedHere, the one gate): races, Free Ride and the test ride; the career, MotoSkills and the
            // challenges with PHYSICS STUDIO EVERYWHERE. "" leaves the core untouched (bit for
            // bit the original). A config in the career / MotoSkills / a challenge makes the session practice: its progress is not saved.
            PhysicsContext = career != null ? PhysicsContext.Career : this.tutorial ? PhysicsContext.MotoSkills : !string.IsNullOrEmpty(spec.EventActivation) ? PhysicsContext.Challenge
                           : this.testDrive ? PhysicsContext.TestRide : string.Equals(spec.EventType, "FreeRide", StringComparison.OrdinalIgnoreCase) ? PhysicsContext.FreeRide : PhysicsContext.Race;
            // ALIVE CLASSES (core/plan/ALIVE_BIKES.md): an imported Alive 50cc / 650 rides on its own physics wherever it is ridden (the local rider)
            try { ownPhysics = this.arenaImage ? new List<(int, int, byte[], string, string)>() : RxOwnPhysics.Current(); }
            catch (Exception e) { ownPhysics = new List<(int, int, byte[], string, string)>(); Log("own physics: " + e.Message); }
            if (this.arenaImage) { physicsText = ""; physicsName = ""; physicsLive = false; }   // the arena's ride: the original physics; PHYSICS STUDIO's event values stay the loading event's
            else
            {
                physicsText = RxPhysicsStudio.TextFor(PhysicsContext);
                physicsName = RxPhysicsStudio.PendingName;
                physicsLive = RxPhysicsStudio.LiveEditAllowed(PhysicsContext);
                RxPhysicsStudio.EventActive = false; RxPhysicsStudio.EventRestored = false; RxPhysicsStudio.EventValues = 0; RxPhysicsStudio.EventName = ""; RxPhysicsStudio.EventReport = "";
                RxPhysicsStudio.EventContext = (int)PhysicsContext; RxPhysicsStudio.EventLiveArmed = false;
                RxPhysicsStudio.EventPractice = !string.IsNullOrEmpty(physicsText) && RxPhysicsStudio.PracticeOnly(PhysicsContext);
                if (RxPhysicsStudio.EventPractice && persistence != null) persistence = PhysicsPracticePersistence.Wrap(persistence, Log);
            }
            if (career != null && persistence == null) throw new ArgumentException("Career requires a complete player profile authority.");
            careerLaunch = career;
            if (career?.ReplayPresentation == true || career == null && replayPresentation) Replay = new ReplayWorker();
            if (profile != null && persistence != null) throw new ArgumentException("Choose one player profile authority.");
            playerCheckpoint = new PlayerSessionCheckpoint(persistence);
            if (career != null) Career = new CareerWorker(
                () => playerCheckpoint.Checkpoint(new NativePlayerSnapshotBackend(session), CurrentPhase == Phase.Ready && State != RxSessionState.Error),
                message => playerCheckpoint.ReportCheckpointFailure(message));
            persistPlayer = persistence != null;
            this.remap = remap == null ? null : (int[])remap.Clone();
            uiSettings = GameUiSettings;
            this.profile = profile == null ? null : (RxProfileCell[])profile.Clone();
            this.installRoot = installRoot; this.spec = spec; this.trackTerrain = trackTerrain; this.prepareStartup = prepareStartup;
            thread = new Thread(Loop) { IsBackground = true, Name = "OpenReflex simulation", Priority = HighQos ? ThreadPriority.AboveNormal : ThreadPriority.Normal };
            thread.Start();
        }

        /// The load arena's session made ahead in the menus (OpenFrontEnd/OpenArenaWarm.cs; w-menufps): its worker at the normal priority while
        /// nothing waits for it (HighQos put it above the menus' main and render threads), back to its own when START EVENT claims it. Not below
        /// normal: on a busy machine that made it 2-3x slower to be ready (a START EVENT soon after the menus came up would find it starting).
        /// The thread's priority only (macOS keeps the QoS class the worker set itself).
        public void SetBackground(bool background)
        {
            try { thread.Priority = background || !HighQos ? ThreadPriority.Normal : ThreadPriority.AboveNormal; }
            catch (Exception) { }   // (a worker that has ended)
        }

        /// Waits until no job is running or queued (at most timeoutMs); true when idle.
        public bool WaitIdle(float timeoutMs)
        {
            long watch = Stopwatch.GetTimestamp();
            lock (gate)
                while (busy)
                {
                    int left = (int)Math.Ceiling(timeoutMs - Ms(watch));
                    if (left <= 0 || stopping) return false;
                    Monitor.Wait(gate, left);
                }
            return true;
        }

        /// Waits for the session to be created and the event loaded (not for the load states: those are frames); true when Ready.
        public bool WaitReady(float timeoutMs)
        {
            long watch = Stopwatch.GetTimestamp();
            lock (gate)
                while (CurrentPhase == Phase.Starting)
                {
                    int left = (int)Math.Ceiling(timeoutMs - Ms(watch));
                    if (left <= 0 || stopping) return false;
                    Monitor.Wait(gate, left);
                }
            return CurrentPhase == Phase.Ready;
        }

        /// Queues one frame (the input of the next frame and the real time it covers); always accepted.
        public bool Submit(float dt, byte[] keys, RxPad[] pads, uint mask)
        {
            lock (gate)
            {
                if (stopping || CurrentPhase == Phase.Failed || CurrentPhase == Phase.Stopped) return false;
                requested += dt;
                if (busy)
                {
                    queuedDt = Math.Min(queuedDt + dt, 0.25f); queued = true; queuedFrames = 1; queuedStamp = requested; queuedInput = Clock;
                    CopyInput(keys, pads, queuedKeys, queuedPads); queuedMask = mask;
                    return true;
                }
                jobDt = dt; jobFrames = 1; jobStamp = requested; jobInput = Clock; CopyInput(keys, pads, jobKeys, jobPads); jobMask = mask;
                commandOnly = false; busy = true;
            }
            go.Set();
            return true;
        }

        /// Queues `frames` whole frames of `step` seconds each with the latest input; frames requested while one runs wait and start
        /// right after it (back to back while the simulation keeps up, else merged). Results are stamped with the requested clock.
        public void SubmitFrames(int frames, float step, byte[] keys, RxPad[] pads, uint mask)
        {
            if (frames <= 0) return;
            lock (gate)
            {
                if (stopping || CurrentPhase == Phase.Failed || CurrentPhase == Phase.Stopped) return;
                requested += frames * (double)step;
                if (busy)
                {
                    queuedFrames = Math.Min(queuedFrames + frames, MaxQueuedFrames); queuedDt = step; queuedStamp = requested; queuedInput = Clock; queued = true;
                    CopyInput(keys, pads, queuedKeys, queuedPads); queuedMask = mask;
                    return;
                }
                jobFrames = frames; jobDt = step; jobStamp = requested; jobInput = Clock;
                CopyInput(keys, pads, jobKeys, jobPads); jobMask = mask;
                commandOnly = false; busy = true;
            }
            go.Set();
        }

        /// Wake only the native owner command queue (career commands and replay receipts). Does not advance the game or publish a render snapshot.
        public void WakeCareerCommands()
        {
            lock (gate)
            {
                if (Career == null && Replay == null || stopping || CurrentPhase == Phase.Failed || CurrentPhase == Phase.Stopped) return;
                if (busy) { commandWake = true; return; }
                commandOnly = true; busy = true;
            }
            go.Set();
        }

        static void CopyInput(byte[] keys, RxPad[] pads, byte[] keysTo, RxPad[] padsTo)
        {
            if (keys != null) Buffer.BlockCopy(keys, 0, keysTo, 0, Math.Min(keys.Length, 256)); else Array.Clear(keysTo, 0, 256);
            if (pads != null) Array.Copy(pads, padsTo, Math.Min(pads.Length, 4)); else Array.Clear(padsTo, 0, 4);
        }

        /// The clock SubmitFrames has requested so far (s).
        public double Requested { get { lock (gate) return requested; } }

        /// The results of the frames completed since the last call (null if none).
        public Results Collect()
        {
            lock (gate)
            {
                if (!readyValid) return null;
                readyValid = false;
                var r = ready;
                if (owned) ready = pool.Count > 0 ? pool.Pop() : new Results();   // r stays the caller's until Recycle
                else { ready = front; front = r; }
                return r;
            }
        }
        /// Keeps collected results until Recycle (several held at once: a presentation interpolates between two).
        public void KeepResults() { lock (gate) owned = true; }
        public void Recycle(Results r) { if (r != null) lock (gate) if (owned && pool.Count < 8) pool.Push(r); }

        // a completed job: older uncollected results merge into it; then the next queued job starts at once
        bool Complete(Results r)
        {
            lock (gate)
            {
                if (r != null && readyValid)
                {
                    var o = ready;
                    r.terrain.InsertRange(0, o.terrain); DropCoveredPatches(r.terrain); r.dt += o.dt;
                    r.laps.InsertRange(0, o.laps);   // (the lap log is read once: an uncollected result's laps are not lost)
                    if (r.error == null) r.error = o.error;
                }
                if (r != null) { back = ready; ready = r; readyValid = true; }
                if (stopping || CurrentPhase == Phase.Failed) { queued = commandWake = false; busy = false; Monitor.PulseAll(gate); return false; }
                if (!queued)
                {
                    if (commandWake) { commandWake = false; commandOnly = true; return true; }
                    busy = false; Monitor.PulseAll(gate); return false;
                }
                commandOnly = false;
                jobDt = queuedDt; Buffer.BlockCopy(queuedKeys, 0, jobKeys, 0, 256); Array.Copy(queuedPads, jobPads, 4); jobMask = queuedMask;
                jobFrames = Math.Max(1, queuedFrames); jobStamp = queuedStamp; jobInput = queuedInput;
                queued = false; queuedDt = 0; queuedFrames = 0;
                return true;
            }
        }

        void Fail(string message)
        {
            Replay?.CloseWorker();
            Career?.CloseWorker();
            lock (gate)
            {
                if (failure == null) failure = message;
                phase = (int)Phase.Failed;
                Monitor.PulseAll(gate);
            }
            stateValue = (int)RxSessionState.Error;
            Log(message);
        }

        // ---------------------------------------------------------------------------------------------------------------- worker

        // Create + event load. Both are long (the install, the startup, the level's request) but run once; a failure is a Failed phase.
        void Startup()
        {
            try
            {
                var check = RxApiNative.Check();
                if (!check.Compatible) { Fail((check.Loaded ? "librxcore does not match this build: " : "librxcore is not usable: ") + check.Summary); return; }
                if (persistPlayer) RxApiNative.RequirePlayerPersistence();
                // An earlier Dispose may have timed out while its final frame/save finishes.
                // Keep ownership until that worker has saved and destroyed its arena.
                lock (sessionOwnership)
                {
                    if ((arenaImage ? arenaImageFailure : arenaFailure) != null) { Fail(arenaImage ? arenaImageFailure : arenaFailure); return; }
                    var waiting = Stopwatch.StartNew();
                    while ((arenaImage ? activeArenaSessions : activeSessions) != 0)
                    {
                        if (stopping) return;
                        if (waiting.ElapsedMilliseconds > 60000) { Fail("The previous session is still closing. Please wait before starting another ride."); return; }
                        Monitor.Wait(sessionOwnership, 250);
                        if ((arenaImage ? arenaImageFailure : arenaFailure) != null) { Fail(arenaImage ? arenaImageFailure : arenaFailure); return; }
                    }
                    if (stopping) return;
                    if (arenaImage) activeArenaSessions++; else activeSessions++;
                    counted = true;
                }
                double createAt = startupClock.Elapsed.TotalMilliseconds;
                session = RxApiNative.rx_session_create(installRoot);
                if (session == IntPtr.Zero) { Fail("rx_session_create failed: " + Clean(RxApiNative.rx_session_last_error(IntPtr.Zero))); return; }
                playerCheckpoint.Start(new NativePlayerSnapshotBackend(session));
                double loadAt = startupClock.Elapsed.TotalMilliseconds; StartupCreateMs = loadAt - createAt;
                if (stopping) return;
                if (profile != null && RxApiNative.rx_session_profile_import(session, profile) != 0)
                { Fail("Profile selection failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                if (remap != null && RxApiNative.rx_session_remap_set(session, remap) != 0)
                { Fail("Controller bindings failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                ApplyUiSettings();   // before the event loads: the players' KeyMaps and the AI's pace read them then
                { float level = GameAirAutoLevel, snap = GameAirSnap; if (level < 1f || snap < 1f) ApplyAirHelpers(session, level, snap); }   // AUTO LEVEL / COUNTER-STEER SNAP (1, 1 is the core's default)
                { float loose = GameFreeAirLooseness; if (loose > 0f) ApplyFreeAirLooseness(session, loose); }   // AIR LOOSENESS (likewise)
                { int borders = GameFreeRideBorders; if (borders != 0) ApplyFreeRideBorders(session, borders); }   // FREE RIDE WITHOUT BORDERS (before the load)
                ApplyPhysics(session);   // PHYSICS STUDIO: before any event loads (the career's too), so before its vehicles are made
                foreach (var (model, cls, file, name, nodes) in ownPhysics)   // ALIVE CLASSES: likewise
                    try
                    {
                        int n = RxPhysicsNative.VehiclePhysics(session, model, cls, file, out string rep);
                        int k = n > 0 && nodes != null ? RxPhysicsNative.VehiclePhysicsNodes(session, model, nodes) : 0;
                        Log($"{name}: its own physics (class {cls}) for model {model}: {n} record(s) {rep}" + (nodes != null ? $", {k} hierarchy node(s) of its own" : ""));
                    }
                    catch (EntryPointNotFoundException) { Log("rx_session_vehicle_physics is not exported by this librxcore: " + name + " rides the original's physics"); }
                if (Career != null)
                {
                    // the career's configure re-defaults the primary player's UI settings row (rx_ui_set_primary_player_device_index(0) =
                    // UISettings_Reset: the original restores the profile's saved PlayerSettings there; the port's profile has none, its
                    // settings are the client's): written again right after it, before the series loads and makes its players, so a career
                    // race rides the saved camera (and CAREER DIFFICULTY, handedness ...) as an arcade race does, not the guest defaults
                    var replay = Replay;
                    Career.StartWorker(session, careerLaunch, (s, g) => { ApplyUiSettings(); replay?.BeginWorker(s, g); });
                }
                else
                {
                    // An exhibition event's replay presenter is installed before the load (before the first frame).
                    Replay?.BeginEventWorker(session);
                    // the Race & Ride challenges Free Ride (MainMenu:CareerMode's InCareerMode): the Free Ride's discovery mode marks the
                    // activation areas (core/game/race_activation.c); also for a challenge started from one of them
                    if (inCareerMode && RxActivationNative.SetInCareerMode(session, true) != 0)
                    { Fail("rx_session_event_in_career_mode failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                    // a challenge starts behind PreChallengePopup:Start's WaitForUI = 1 (released by the page's accept: ReleasePreChallenge)
                    if (preChallengeHold)
                    {
                        try { preChallengeHeld = RxChallengeNative.WaitForUi(session, true) == 0; }
                        catch (EntryPointNotFoundException) { Log("rx_session_wait_for_ui is not exported by this librxcore; the challenge starts without its popup"); }
                    }
                    var s = spec;
                    if (testDrive)
                    {   // the garage's TEST DRIVE in the session's own UI world (no level is loaded)
                        try
                        {
                            if (RxTestDriveNative.Start(session, ref s) != 0)
                            { Fail("rx_session_test_drive_start failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                        }
                        catch (EntryPointNotFoundException) { Fail("This librxcore has no test drive (rx_session_test_drive_start): rebuild the core."); return; }
                    }
                    else if (tutorial)
                    {   // the MotoSkills tutorials in the session's own UI world (no level is loaded)
                        try
                        {
                            if (RxTutorialNative.Start(session, ref s) != 0)
                            { Fail("rx_session_tutorial_start failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                        }
                        catch (EntryPointNotFoundException) { Fail("This librxcore has no MotoSkills tutorials (rx_session_tutorial_start): rebuild the core."); return; }
                    }
                    else
                    {
                        if (mapScaleBase != 1f)
                        {   // BASE SCALE: the track's default scale (an Alive race level's): at it the event keeps its records (the core's test)
                            try
                            {
                                if (RxApiNative.rx_session_set_map_scale_base(session, mapScaleBase) != 0)
                                { Fail("rx_session_set_map_scale_base failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                            }
                            catch (EntryPointNotFoundException) { Fail("This librxcore has no BASE SCALE (rx_session_set_map_scale_base): rebuild the core."); return; }
                        }
                        if (mapScale != 1f)
                        {   // MAP SCALE: the core makes the track at the scale the world's file-built parts were made at (RxMapScale)
                            try
                            {
                                if (RxApiNative.rx_session_set_map_scale(session, mapScale) != 0)
                                { Fail("rx_session_set_map_scale failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                            }
                            catch (EntryPointNotFoundException) { Fail("This librxcore has no MAP SCALE (rx_session_set_map_scale): rebuild the core, or set MAP SCALE to 100 %."); return; }
                            MapScale = RxApiNative.rx_session_map_scale(session);
                            if (Math.Abs(mapScale - mapScaleBase) > 1e-4f * mapScaleBase)
                                Log($"MAP SCALE {MapScale * 100f:0} % (the track's default {mapScaleBase * 100f:0} %): the track is scaled; offline records, best laps and highlights are not kept for this event");
                            else Log($"MAP SCALE {MapScale * 100f:0} %: the track's default scale (its records are kept)");
                        }
                        if (practice)
                        {   // PRACTICE: the exhibition lap race is endless (the original's Practice rule; core/game/practice.h)
                            try
                            {
                                if (RxApiNative.rx_session_set_practice(session, true) != 0)
                                { Fail("rx_session_set_practice failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                                Practice = RxApiNative.rx_session_practice(session);
                            }
                            catch (EntryPointNotFoundException) { Fail("This librxcore has no PRACTICE (rx_session_set_practice): rebuild the core."); return; }
                            Log("PRACTICE: an endless lap race, one rider; the laps are logged (rx_session_lap_log)");
                        }
                        // MODS/TRACKS (RxCustomTrack, Content/TrackMods.cs): a custom track's files read from its folder for this session (the install untouched)
                        if (RxCustomTrack.For(s.Location))
                        {
                            try
                            {
                                if (RxApiNative.rx_session_mount_level(session, RxCustomTrack.Level, RxCustomTrack.Folder) != 0)
                                { Fail("The custom track could not be loaded: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                            }
                            catch (EntryPointNotFoundException) { Fail("This librxcore has no custom tracks (rx_session_mount_level): rebuild the core."); return; }
                            Log("MODS/TRACKS: " + RxCustomTrack.Level + " read from " + RxCustomTrack.Folder + " (no offline records for this event)");
                        }
                        // RIDER CAP (w-rc-client): an exhibition takes spec.riders up to the core's cap as is (rx_api.h); a core from before the
                        // big field loads the original's 12, and the startup waits for the riders actually loaded (loadedRiders)
                        if (RxRiderCap.Big(s.riders))
                        {
                            int cap = RxRiderCap.Clamp(s.riders);
                            if (cap != s.riders) Log($"RIDER CAP: this librxcore races at most {cap} riders: the event loads with {cap} instead of {s.riders}");
                            else Log($"RIDER CAP: a field of {s.riders} riders (the original's {RxRiderCap.Original} + {s.riders - RxRiderCap.Original} behind the gate)");
                            s.riders = cap;
                        }
                        loadedRiders = s.riders;
                        if (RxApiNative.rx_session_event_load(session, ref s) != 0)
                        { Fail("rx_session_event_load failed: " + Clean(RxApiNative.rx_session_last_error(session))); return; }
                        // RESTORED CONTENT: a championship weekend's stage (RxChampWeekend; only when the shell armed one)
                        var champ = RxChampWeekend.Apply(session, s.Location);
                        if (champ != null) { Fail(champ); return; }
                    }
                }
                startupLoadedAt = startupClock.Elapsed.TotalMilliseconds; StartupLoadMs = startupLoadedAt - loadAt;
                lock (gate) { phase = (int)Phase.Ready; Monitor.PulseAll(gate); }
            }
            catch (Exception e) when (e is DllNotFoundException || e is EntryPointNotFoundException || e is BadImageFormatException)
            { Fail("librxcore is not usable: " + e.Message); }
            catch (Exception e) { Fail("startup failed: " + e); }
        }
        bool counted;

        /// PHYSICS STUDIO (core rx_session_phys_set): the player's config for this event's local vehicles. Nothing is sent without one.
        void ApplyPhysics(IntPtr s)
        {
            if (physicsLive)
                try { RxPhysicsStudio.EventLiveArmed = RxPhysicsNative.ArmLive(s, true) == 0; }   // (the whole-arena hashes equal main's armed)
                catch (EntryPointNotFoundException) { }
            if (string.IsNullOrEmpty(physicsText)) return;
            try
            {
                int n = RxPhysicsNative.Set(s, physicsText, out string report);
                RxPhysicsStudio.EventValues = Math.Max(0, n);
                RxPhysicsStudio.EventName = physicsName;
                RxPhysicsStudio.EventReport = report ?? "";
                RxPhysicsStudio.EventActive = n > 0;
                Log($"PHYSICS STUDIO \"{physicsName}\": {n} value(s) for the local vehicle; {(report ?? "").Replace('\n', ' ').Trim()}");
            }
            catch (EntryPointNotFoundException) { Log("rx_session_phys_set is not exported by this librxcore; the event rides the original physics"); }
        }

        static string Clean(string s) => string.IsNullOrEmpty(s) ? "(no error text)" : s;

        void Loop()
        {
            RxNativeLoader.ThreadImage = arenaImage ? 1 : 0;   // every core call of this worker reaches its session's image
            if (HighQos && RuntimeInformation.IsOSPlatform(OSPlatform.OSX)) try { pthread_set_qos_class_self_np(QosUserInteractive, 0); } catch (Exception) { }
            try
            {
                Startup();
                bool next = false;
                while (true)
                {
                    if (!next) go.WaitOne();
                    if (stopping) return;
                    bool commands, fitNow;
                    lock (gate) { if (!busy) { next = false; continue; } commands = commandOnly; fitNow = fitOnly; fitOnly = false; }
                    if (fitNow)
                    {   // RTBumpFitNow: that fit alone (no game frame, no command, no other between-frame call)
                        int serial = 0;
                        if (CurrentPhase == Phase.Ready) try { serial = RunRTBumpFitNow(session); } catch (Exception e) { Log("RT_Bump fit: " + e.Message); }
                        next = Complete(null);
                        // the answer after the job is complete (the caller finds the worker idle again: Idle is true when it goes on)
                        lock (rtbumpGate) { if (serial > nowDone) nowDone = serial; Monitor.PulseAll(rtbumpGate); }
                        continue;
                    }
                    Results r = null;
                    long watch = Stopwatch.GetTimestamp();
                    try
                    {
                        if (CurrentPhase == Phase.Ready && Replay != null && Career?.Snapshot?.Status.records_finalized != 1)
                        { Replay.CommandsWorker(session); Replay.RefreshWorker(session); }
                        if (CurrentPhase == Phase.Ready && Career != null)
                        {
                            // A pending render batch must paint before Continue can stop its playback.
                            // Preserve queued career commands; no busy rejection or fabricated receipt.
                            if (!(Replay?.Pending ?? false)) Career.CommandsWorker(session);
                            Career.RefreshWorker(session);
                        }
                        bool finalized = Career?.Snapshot?.Status.records_finalized == 1;
                        if (finalized) Replay?.CloseWorker();
                        else if (CurrentPhase == Phase.Ready && Replay != null) Replay.RefreshWorker(session);
                        if (!commands && !finalized && !(Replay?.Pending ?? false))
                        { r = back; ResetResults(r); RunJob(r); r.milliseconds = Ms(watch) - r.capWaitMilliseconds; }
                    }
                    catch (Exception e)
                    { if (r != null) r.error = e.Message; Fail("exception in the simulation thread: " + e); }
                    next = Complete(r);
                }
            }
            finally { Teardown(); }
        }

        static void ResetResults(Results r)
        {
            r.replay = null;
            r.error = null; r.terrain.Clear();
            r.hasBike = r.hasRace = r.hasCamera = r.hasTerrainInfo = r.hasParticles = r.hasAmbient = false;
            r.hasHud = r.hasVehicle = r.hasWaypoint = r.hasPowercross = r.hasActivation = r.hasChallenge = r.hasCompass = r.hasRaceFormat = r.hasRaceHud = r.hasStuntHud = r.hasModeResults = r.hasReflexHud = false; r.modeResults = default; r.raceFormat = default; r.raceHud = default; r.stuntHud = default; r.reflexHud = default; r.waypoint = default; r.powercross = default; r.activation = default; r.challenge = default; r.compass = default;
            r.eventUi = default; r.vehicleAudio.Clear(); r.sound.Clear(); r.resultsPage = null;
            r.riderBones = r.bikeNodes = r.driverCount = r.mudCount = r.slotCount = r.raceLinePoints = r.particleCount = 0;
            r.ambientEntityCount = r.ambientMatrixCount = 0; r.ambientNodeFlagBytes = 0; r.crowdFlashCount = 0;
            r.scenarioModelCount = 0; r.hasScenarioModels = false; r.scenarioSoundCount = 0;
            r.hasPodium = false; r.podium = default; r.podiumRiderCount = r.podiumSlotCount = 0;
            r.bikeNodeFlagCount = 0;
            r.dt = 0; r.slotMilliseconds = 0;
            r.laps.Clear();
        }

        void RunJob(Results r)
        {
            while (betweenFrames.TryDequeue(out var call) && CurrentPhase == Phase.Ready)
            { try { call(session); } catch (Exception e) { Log("between frames: " + e.Message); } }
            if (CurrentPhase == Phase.Ready) ApplyWeatherGrip(session);   // WEATHER GRIP: the weather's factor of this frame

            // jobFrames whole frames of jobDt: back to back while they fit in about one frame period at the measured cost, else one
            // frame covering MaxMergedSteps of them (a slower machine); the rest dropped.
            int frames = jobFrames, calls = frames; float dt = jobDt; double stamp = jobStamp;
            r.capWaitMilliseconds = 0;
            if (RealClock)
            {
                frames = calls = 1; long w0 = Stopwatch.GetTimestamp(); dt = StartRealFrame(out stamp); r.capWaitMilliseconds = (Stopwatch.GetTimestamp() - w0) * 1000.0 / Stopwatch.Frequency;
                // Input that arrived while this frame waited for its cap is newer than the input it was submitted with, and it is for this
                // same frame (the original reads its devices when the cap ends: Engine::UpdateTiming 0084ea80 then the poll in
                // Engine::Update). Take it, so a frame requested meanwhile does not run one cap later with input already a frame old
                // (fast or uncapped Unity frames queue one while a frame waits: measured 16.8 ms old at its start before, w-latency).
                // Only the input changes; the frame, its dt and its results are the game's.
                lock (gate)
                    if (queued && r.capWaitMilliseconds > 0)
                    {
                        Buffer.BlockCopy(queuedKeys, 0, jobKeys, 0, 256); Array.Copy(queuedPads, jobPads, 4); jobMask = queuedMask; jobInput = queuedInput;
                        queued = false; queuedDt = 0; queuedFrames = 0;
                    }
            }
            else if (frames > 1 && (frames > MaxSeparateFrames || frameMsAverage * frames > 0.8 * 1000.0 * jobDt)) { calls = 1; dt = jobDt * Math.Min(frames, MaxMergedSteps); }
            r.framesRun = calls; r.merged = calls == 1 && frames > 1; r.simTime = stamp; r.inputClock = jobInput; r.dt = 0;

            if (CurrentPhase != Phase.Ready)
            {
                r.sessionState = RxSessionState.Error;
                r.error = Failure ?? "the session is not running";
                return;
            }

            if (hasInput)
                try { RxApiNative.rx_session_set_input(session, jobKeys, jobPads, jobMask); }
                catch (EntryPointNotFoundException) { hasInput = false; }
            if (hasMouseExport)
            {
                int mdx = System.Threading.Interlocked.Exchange(ref mouseDx, 0), mdy = System.Threading.Interlocked.Exchange(ref mouseDy, 0);
                int mw = System.Threading.Interlocked.Exchange(ref mouseWheel, 0), mf = System.Threading.Interlocked.Exchange(ref mouseFlags, 0);
                if (mdx != 0 || mdy != 0 || mw != 0 || mf != 0)
                    try { if (RxApiNative.rx_session_set_mouse(session, mdx, mdy, mw, (uint)mf) == 0) MouseMessages++; }
                    catch (EntryPointNotFoundException) { hasMouseExport = false; }
            }

            r.frameStartClock = Clock; r.collectionClock = double.NaN;
            if (pacingDiagnostics)
            {
                var pad = jobPads[0];
                r.appliedPadCsv = FormattableString.Invariant($"{jobMask},{pad.lx},{pad.ly},{pad.rx},{pad.ry},{pad.left_trigger},{pad.right_trigger},{pad.buttons}");
            }
            int code = 0; long frameWatch = Stopwatch.GetTimestamp();
            int completedCalls = 0;
            for (int c = 0; c < calls && code == 0; c++)
            {
                code = RxApiNative.rx_session_advance_dt(session, dt);
                r.dt += dt; completedCalls++;
                if (code == 0 && Replay != null)
                { Replay.RefreshWorker(session); if (Replay.Pending) break; }
            }
            r.framesRun = completedCalls;
            // PHYSICS STUDIO: the core's guard put a vehicle the config made diverge back on the file's values (the badge says so)
            if (!arenaImage && RxPhysicsStudio.EventActive && !RxPhysicsStudio.EventRestored)   // (one call per frame, only while a config rides; not the arena's ride)
                try { RxPhysicsStudio.EventRestored = (RxPhysicsNative.ActiveBits(session) & 2) != 0; } catch (EntryPointNotFoundException) { }
            if (hasRumbleExport)
                try { for (uint pad = 0; pad < 4; pad++) { if (RxApiNative.rx_session_rumble(session, pad, out ushort l, out ushort rr, out _) == 0) System.Threading.Volatile.Write(ref rumble[pad], (l << 16) | rr); } }
                catch (EntryPointNotFoundException) { hasRumbleExport = false; }
            r.frameMilliseconds = Ms(frameWatch);
            frameMsAverage += (r.frameMilliseconds / Math.Max(1, completedCalls) - frameMsAverage) * 0.1;

            var state = RxApiNative.rx_session_get_state(session);
            r.sessionState = state; stateValue = (int)state;
            r.sessionFrames = RxApiNative.rx_session_frames(session);
            r.replay = Replay?.Snapshot;
            if (r.replay != null && r.replay.View.frame != r.sessionFrames)
                throw new InvalidOperationException("Replay and pose frame identities differ during collection.");
            if (code != 0 || state == RxSessionState.Error)
            {
                // an unported function aborted the core, or the load failed: the API reports it; keep it as the failure and stop
                r.error = Clean(RxApiNative.rx_session_last_error(session));   // the core names the call
                Fail(r.error);
                return;
            }

            float t = RxApiNative.rx_session_game_time(session);
            r.gameTime = t; r.gameDt = float.IsNaN(previousGameTime) ? 0f : t - previousGameTime; previousGameTime = t;

            if (pacingDiagnostics && pacingMetadata == null && state != RxSessionState.Loading)
            {
                try
                {
                    var appearance = new RxAppearance();
                    int localSlot = -1, count = RxApiNative.rx_session_slots(session, pacingSlots, (uint)pacingSlots.Length);
                    for (int i = 0; i < count; i++)
                        if (pacingSlots[i].local != 0) { localSlot = pacingSlots[i].slot; break; }
                    if (localSlot >= 0 && RxApiNative.rx_session_appearance(session, localSlot, ref appearance) == 0 &&
                        RxApiNative.rx_session_tuning_get(session, pacingTuning) == 0)
                        pacingMetadata = "initialModel=" + appearance.vehicle_model_id + ";initialTuning=" + string.Join(";", pacingTuning);
                }
                catch (EntryPointNotFoundException) { pacingMetadata = "initialMetadata=unavailable"; }
            }
            Career?.RefreshWorker(session);
            r.corneringMetadata = pacingMetadata;
            if (state != RxSessionState.Loading) { CaptureStartup(); Collect(r); }   // the world exists from the countdown on
            if (startupCaptured && careerLaunch != null && careerLaunch.IsHeat)
            {
                if (state == RxSessionState.Loading) rosterDirty = true;
                else if (rosterDirty && CaptureRoster()) rosterDirty = false;
            }
        }

        bool CaptureRoster()
        {
            var slots = new RxSlot[RxRiderCap.Max];
            int count = RxApiNative.rx_session_slots(session, slots, (uint)slots.Length);
            if (count < careerLaunch.Riders) return false;
            Array.Resize(ref slots, count);
            var data = new RosterData { slots = slots, appearances = new RxAppearance[count] };
            for (int i = 0; i < count; i++)
                if (RxApiNative.rx_session_appearance(session, slots[i].slot, ref data.appearances[i]) != 0) return false;
            lock (gate) rosterData = data;
            return true;
        }

        void CaptureStartup()
        {
            if (!prepareStartup || startupCaptured) return;
            var slots = new RxSlot[RxRiderCap.Max];
            int count = RxApiNative.rx_session_slots(session, slots, (uint)slots.Length);
            if (count == 0) return;
            // A partial roster during construction must not become the permanent visual roster.
            if (count < (careerLaunch != null ? careerLaunch.Riders : Math.Max(1, loadedRiders > 0 ? loadedRiders : spec.riders))) return;
            if (RxApiNative.rx_session_sizeof("rx_appearance") != Marshal.SizeOf<RxAppearance>())
                throw new InvalidOperationException("librxcore appearance ABI does not match this client; rebuild the core.");
            Array.Resize(ref slots, count);
            var data = new StartupData { slots = slots, appearances = new RxAppearance[count] };
            try { RxApiNative.rx_session_camera_bindings(session, ref data.cameraBindings); }
            catch (EntryPointNotFoundException) { Log("Current core required for mapped five-view camera switching"); }
            for (int i = 0; i < count; i++)
                if (RxApiNative.rx_session_appearance(session, slots[i].slot, ref data.appearances[i]) != 0) return;
            if (trackTerrain)
            {
                if (RxApiNative.rx_session_terrain(session, ref data.terrainInfo) != 0) return;
                int n = checked((int)data.terrainInfo.samples);
                if (n < 2 || n > 8192) throw new InvalidOperationException("Invalid core terrain dimensions.");
                data.heights = new float[checked(n*n)]; data.deformation = new float[n*n];
                if (RxApiNative.rx_session_terrain_heights(session, data.heights, 0, 0, n, n) != 0 ||
                    RxApiNative.rx_session_terrain_deform(session, data.deformation, 0, 0, n, n) != 0)
                    throw new InvalidOperationException("Could not read the core startup terrain.");
                terrainInfo = data.terrainInfo; // the snapshot supplies the first complete grid
            }
            if (startupLoadedAt >= 0) StartupFramesMs = startupClock.Elapsed.TotalMilliseconds - startupLoadedAt;
            lock (gate) { startupData = data; startupCaptured = true; }
        }

        // RealClock: the frame cap and the dt of the frame starting now; the results are stamped with the game time so far.
        float StartRealFrame(out double stamp)
        {
            double last; lock (gate) last = lastStart;
            double t = Clock, dt = FrameCap;
            if (!double.IsNaN(last))
            {
                while (t - last < FrameCap)
                {
                    // sleep to within a margin of the cap, then yield: Windows' Sleep runs on the system timer (15.6 ms unless raised), so
                    // PreciseWait uses a high-resolution waitable timer there (else timeBeginPeriod(1)); a late wake-up starts the game
                    // frame late, and the main thread waits for it (same-frame pacing)
                    double remaining = FrameCap - (t - last);
                    if (remaining > PreciseWait.Threshold) PreciseWait.Sleep(remaining - PreciseWait.Margin);
                    else Thread.Yield();
                    t = Clock;
                }
                dt = Math.Min(t - last, 0.25);
            }
            lock (gate) { lastStart = t; requested += dt; stamp = requested; }
            return (float)dt;
        }

        // The getters return nonzero / 0 when there is nothing to read yet (no local vehicle, no terrain): that is "not available", not an error.
        bool hasWaypointExport = true, hasActivationExport = true, hasChallengeExport = true, hasCompassExport = true, hasRaceFormatExport = true, hasRaceHudExport = true, hasStuntHudExport = true, hasModeResultsExport = true, hasReflexHudExport = true;
        bool hasVehicleAudioExport = true; readonly byte[] vehicleAudioScratch = new byte[RxVehicleAudioState.Size];
        bool hasSoundExport = true; readonly byte[] soundScratch = new byte[RxSoundState.Size];
        bool hasResultsExport = true; readonly byte[] resultsScratch = new byte[RxResultsPage.Size]; RxResultsPage resultsPageCache;   // the results page's table (w-resultsrng)
        bool hasOnboardExport = true, hasRaceLineExExport = true, hasEventUiExport = true, checkedEventUiLayout;
        // LAP LOG (w-lb-practice): the last serial read; the records since then go into the result (RxPractice turns them into laps)
        bool hasLapLogExport = true, checkedLapLayout; uint lapSerial; readonly RxLapRecord[] lapScratch = new RxLapRecord[16];
        void ReadLapLog(Results r)
        {
            r.physClass = -1;
            if (!hasLapLogExport) return;
            try
            {
                if (!checkedLapLayout) { checkedLapLayout = true; hasLapLogExport = RxApiNative.rx_session_sizeof("rx_lap_record") == Marshal.SizeOf<RxLapRecord>(); if (!hasLapLogExport) { Log("rx_lap_record's size differs from this core's: no lap log"); return; } }
                for (int guard = 0; guard < 8; guard++)
                {
                    int n = RxApiNative.rx_session_lap_log(session, lapSerial, lapScratch);
                    if (n <= 0) break;
                    for (int i = 0; i < n; i++) { r.laps.Add(lapScratch[i]); lapSerial = lapScratch[i].serial; }
                    if (n < lapScratch.Length) break;
                }
                r.physClass = RxApiNative.rx_session_local_phys_class(session);
            }
            catch (EntryPointNotFoundException) { hasLapLogExport = false; }
        }
        void Collect(Results r)
        {
            if (tutorial)
                try
                {
                    if (RxTutorialNative.Read(session, tutorialRead) == 0) TutorialSnapshot = tutorialRead.Clone();
                    int n = RxTutorialNative.Objects(session, tutorialObjectsRead);
                    if (n >= 0) { var o = new RxTutorialObject[n]; Array.Copy(tutorialObjectsRead, o, n); TutorialObjects = o; }
                }
                catch (EntryPointNotFoundException) { }
            r.hasBike = RxApiNative.rx_session_bike(session, ref r.bike) == 0;
            r.onboardState = default;
            if (hasOnboardExport)
                try { RxApiNative.rx_session_onboard_state(session, ref r.onboardState); }
                catch (EntryPointNotFoundException) { hasOnboardExport = false; }
            r.riderBones = RxApiNative.rx_session_rider_pose(session, r.riderPose, RiderBoneCount);
            r.bikeNodes = RxApiNative.rx_session_bike_pose(session, r.bikePose, MaxSlotNodes);
            r.hasRace = RxApiNative.rx_session_race(session, ref r.race) == 0 && r.race.valid != 0;
            // RESTORED CONTENT: a championship stage is recorded on this worker as soon as its activity is in Post (before any results page)
            if (r.hasRace && Career == null) try { RxChampWeekend.AfterFrame(session, r.race.state); } catch (Exception e) { Log("Championship weekend: " + e.Message); }
            ReadLapLog(r);
            r.eventUi = default;
            if (hasEventUiExport)
                try
                {
                    if (!checkedEventUiLayout) { checkedEventUiLayout = true; hasEventUiExport = RxApiNative.rx_session_sizeof("rx_event_ui_state") == Marshal.SizeOf<RxEventUiState>(); }
                    if (hasEventUiExport && RxApiNative.rx_session_event_ui_state(session, ref r.eventUi) != 0) r.eventUi = default;
                }
                catch (EntryPointNotFoundException) { hasEventUiExport = false; }

            try { r.hasHud = RxApiNative.rx_session_hud(session, ref r.hud) == 0 && r.hud.valid != 0; }
            catch (EntryPointNotFoundException) { hasHudExport = false; r.hasHud = false; }
            if (!hasHudExport) r.hasHud = false;
            r.waypoint = RxWaypointState.Create(); r.hasWaypoint = false;
            if (hasWaypointExport)
                try { r.hasWaypoint = RxWaypointNative.Read(session, ref r.waypoint) == 0 && r.waypoint.valid != 0; }
                catch (EntryPointNotFoundException) { hasWaypointExport = false; }
            if (!r.hasWaypoint) r.waypoint = default;
            r.activation = RxActivationState.Create(); r.hasActivation = false;
            if (hasActivationExport)
                try { r.hasActivation = RxActivationNative.Read(session, ref r.activation) == 0 && r.activation.valid != 0; }
                catch (EntryPointNotFoundException) { hasActivationExport = false; }
            if (!r.hasActivation) r.activation = default;
            r.challenge = RxChallengeState.Create(); r.hasChallenge = false;
            if (hasChallengeExport)
                try { r.hasChallenge = RxChallengeNative.Read(session, ref r.challenge) == 0 && r.challenge.valid != 0; }
                catch (EntryPointNotFoundException) { hasChallengeExport = false; }
            if (!r.hasChallenge) r.challenge = default;
            r.compass = RxCompassState.Create(); r.hasCompass = false;
            if (hasCompassExport)
                try { r.hasCompass = RxCompassNative.Read(session, ref r.compass) == 0 && r.compass.valid != 0; }
                catch (EntryPointNotFoundException) { hasCompassExport = false; }
            if (!r.hasCompass) r.compass = default;
            r.raceFormat = RxRaceFormatState.Create(); r.hasRaceFormat = false;
            if (hasRaceFormatExport)
                try { r.hasRaceFormat = RxRaceFormatNative.Read(session, ref r.raceFormat) == 0 && r.raceFormat.valid != 0; }
                catch (EntryPointNotFoundException) { hasRaceFormatExport = false; }
            if (!r.hasRaceFormat) r.raceFormat = default;
            r.raceHud = RxRaceHudState.Create(); r.hasRaceHud = false;
            if (hasRaceHudExport)
                try { r.hasRaceHud = RxRaceHudNative.Read(session, ref r.raceHud) == 0 && r.raceHud.valid != 0; }
                catch (EntryPointNotFoundException) { hasRaceHudExport = false; }
            if (!r.hasRaceHud) r.raceHud = default;
            r.modeResults = RxModeResultsState.Create(); r.hasModeResults = false;
            if (hasModeResultsExport)
                try { r.hasModeResults = RxModeResultsNative.Read(session, ref r.modeResults) == 0 && r.modeResults.valid != 0; }
                catch (EntryPointNotFoundException) { hasModeResultsExport = false; }
            if (!r.hasModeResults) r.modeResults = default;
            r.stuntHud = RxStuntHudState.Create(); r.hasStuntHud = false;
            if (hasStuntHudExport)
                try { r.hasStuntHud = RxStuntHudNative.Read(session, ref r.stuntHud) == 0 && r.stuntHud.valid != 0; }
                catch (EntryPointNotFoundException) { hasStuntHudExport = false; }
            if (!r.hasStuntHud) r.stuntHud = default;
            r.reflexHud = RxReflexHudState.Create(); r.hasReflexHud = false;
            if (hasReflexHudExport)
                try { r.hasReflexHud = RxReflexHudNative.Read(session, ref r.reflexHud) == 0 && r.reflexHud.valid != 0; }
                catch (EntryPointNotFoundException) { hasReflexHudExport = false; }
            if (!r.hasReflexHud) r.reflexHud = default;
            if (hasVehicleAudioExport)
                try { RxVehicleAudioNative.Read(session, vehicleAudioScratch, r.vehicleAudio); }
                catch (EntryPointNotFoundException) { hasVehicleAudioExport = false; r.vehicleAudio.Clear(); }
            if (hasSoundExport)
                try { RxSoundNative.Read(session, soundScratch, r.sound); }
                catch (EntryPointNotFoundException) { hasSoundExport = false; r.sound.Clear(); }
            r.powercross = RxPowercrossHudState.Create(); r.hasPowercross = false;
            try { r.hasPowercross = RxPowercrossHudNative.Read(session, ref r.powercross) == 0 && r.powercross.valid != 0; }
            catch (EntryPointNotFoundException) { }
            if (!r.hasPowercross) r.powercross = default;
            r.raceLineClosed = false;
            try
            {
                if (hasRaceLineExExport)
                    try { r.raceLinePoints = Math.Max(0, RxApiNative.rx_session_race_line_ex(session, r.raceLine, (uint)MaxRaceLine, out int closed)); r.raceLineClosed = closed != 0; }
                    catch (EntryPointNotFoundException) { hasRaceLineExExport = false; r.raceLinePoints = Math.Max(0, RxApiNative.rx_session_race_line(session, r.raceLine, (uint)MaxRaceLine)); }
                else r.raceLinePoints = Math.Max(0, RxApiNative.rx_session_race_line(session, r.raceLine, (uint)MaxRaceLine));
            }
            catch (EntryPointNotFoundException) { hasRaceLineExport = false; r.raceLinePoints = 0; }
            if (!hasRaceLineExport) r.raceLinePoints = 0;
            try { r.hasVehicle = RxApiNative.rx_session_vehicle_info(session, -1, ref r.vehicle) == 0; }
            catch (EntryPointNotFoundException) { hasVehicleExport = false; r.hasVehicle = false; }
            if (!hasVehicleExport) r.hasVehicle = false;
            r.driverCount = Math.Max(0, RxApiNative.rx_session_standings(session, r.drivers, (uint)r.drivers.Length));
            // the results page's table (rx_session_results_page): the core fills it where the original's page starts, in Post
            r.resultsPage = null;
            if (hasResultsExport && r.hasRace && r.race.state == 4)
                try { r.resultsPage = resultsPageCache = RxResultsNative.Read(session, resultsScratch, resultsPageCache); }
                catch (EntryPointNotFoundException) { hasResultsExport = false; }
            r.hasCamera = RxApiNative.rx_session_camera(session, ref r.camera) == 0;
            // the info block and the live particles are one export now; a core without it has the info block only
            if (hasParticleArray)
            {
                try
                {
                    int n = RxApiNative.rx_session_particles_array(session, r.particles, (uint)r.particles.Length, ref r.particleInfo);
                    if (n >= 0) { r.particleCount = n; r.hasParticles = true; }
                    else { hasParticleArray = false; r.particleCount = 0; r.hasParticles = false; }
                }
                catch (EntryPointNotFoundException) { hasParticleArray = false; r.hasParticles = false; r.particleCount = 0; }
            }
            if (!hasParticleArray) r.hasParticles = RxApiNative.rx_session_particles(session, ref r.particleInfo) == 0;
            r.hasAmbient = RxApiNative.rx_session_ambient(session, ref r.ambient) == 0;
            if (ambientOn && hasAmbientExport)
            {
                try
                {
                    int ne = RxApiNative.rx_session_ambient_entities(session, r.ambientEntities, (uint)r.ambientEntities.Length,
                                                                    r.ambientMatrices, (uint)(r.ambientMatrices.Length / 16), ref r.ambient);
                    if (ne >= 0)
                    {
                        r.ambientEntityCount = ne; r.hasAmbient = true;
                        r.ambientMatrixCount = Math.Min(r.ambient.matrices, r.ambientMatrices.Length / 16);
                        CollectAmbientNodeFlags(r);
                        CollectCrowdFlashes(r);
                    }
                }
                catch (EntryPointNotFoundException) { hasAmbientExport = false; r.ambientEntityCount = 0; r.ambientMatrixCount = 0; }
            }
            if (hasNodeFlags && r.hasVehicle)
            {
                try { r.bikeNodeFlagCount = RxApiNative.rx_session_node_flags(session, -1, r.bikeNodeFlags, (uint)r.bikeNodeFlags.Length); }
                catch (EntryPointNotFoundException) { hasNodeFlags = false; r.bikeNodeFlagCount = 0; }
            }
            CollectObjects(r);
            if (trackTerrain) { CollectTerrain(r); CollectTracks(r); }
            CollectSlots(r);
            CollectMud(r);
            CollectPodium(r);
            CollectScenarioModels(r);
        }

        // The scenario cast (rx_podium_api.h): while the activity is in Post (race state 4) the finish / results scenarios hide / show the
        // live riders and vehicles and the podium RiderCharacters stand; while it is in Pre / Countdown (0, 1) and its intro scenario plays
        // (info.scenario 3) the same RiderCharacters play the start-line shots and the live riders are hidden. Outside those scenarios there
        // is nothing to report (no frame: the live models show). An older core without the export: never again.
        // The scenario actors' (ScenarioAnimEntity, ambient kind 1, and its ThirtySecondBoard, kind 16) node flags: a scenario script hides
        // parts of an actor (SetNodeVisibility 0055dff0, which handles 0x103a too: the sky crane's box opens in Cheyenne's hero shot).
        // Only the drawn skinned scenario actors are read (a handful).
        bool hasAmbientNodeFlags = true;
        void CollectAmbientNodeFlags(Results r)
        {
            int used = 0;
            for (int i = 0; i < r.ambientEntityCount; i++)
            {
                r.ambientNodeFlagCount[i] = 0;
                var e = r.ambientEntities[i];
                if (!hasAmbientNodeFlags || (e.kind != 1 && e.kind != 16) || (e.flags & 3u) != 3u || e.bones <= 0 || used + e.bones > r.ambientNodeFlags.Length) continue;
                try
                {
                    var buf = nodeFlagScratch.Length >= e.bones ? nodeFlagScratch : (nodeFlagScratch = new byte[e.bones]);
                    int n = RxApiNative.rx_session_ambient_node_flags(session, e.object_id, buf, (uint)buf.Length);
                    if (n != e.bones) continue;
                    Array.Copy(buf, 0, r.ambientNodeFlags, used, n);
                    r.ambientNodeFlagStart[i] = used; r.ambientNodeFlagCount[i] = n; used += n;
                }
                catch (EntryPointNotFoundException) { hasAmbientNodeFlags = false; }
            }
            r.ambientNodeFlagBytes = used;
        }
        byte[] nodeFlagScratch = new byte[256];

        // The stands crowd's camera flashes (rx_crowd_api.h): the active flash records of the BillboardCrowd sections this frame. An older
        // core without the export: never again (no flashes).
        bool hasCrowdFlashes = true;
        void CollectCrowdFlashes(Results r)
        {
            r.crowdFlashCount = 0;
            if (!hasCrowdFlashes) return;
            bool any = false;
            for (int i = 0; i < r.ambientEntityCount && !any; i++) any = r.ambientEntities[i].kind == 8;   // BillboardCrowdEntity
            if (!any) return;
            try { r.crowdFlashCount = Math.Max(0, Math.Min(RxApiNative.rx_session_crowd_flashes(session, r.crowdFlashes, (uint)r.crowdFlashes.Length), r.crowdFlashes.Length)); }
            catch (EntryPointNotFoundException) { hasCrowdFlashes = false; }
        }

        // The static scenery models a scenario script hides / shows (rx_session_scenario_models): every frame, as the game draws them.
        bool hasScenarioModelExport = true;
        void CollectScenarioModels(Results r)
        {
            if (!hasScenarioModelExport) return;
            try
            {
                int n = RxApiNative.rx_session_scenario_models(session, r.scenarioModels, (uint)r.scenarioModels.Length);
                if (n < 0) return;
                r.scenarioModelCount = Math.Min(n, r.scenarioModels.Length); r.hasScenarioModels = true;
            }
            catch (EntryPointNotFoundException) { hasScenarioModelExport = false; }
            if (!hasScenarioSoundExport) return;
            try { r.scenarioSoundCount = Math.Max(0, RxApiNative.rx_session_scenario_sounds(session, r.scenarioSounds, (uint)r.scenarioSounds.Length)); }
            catch (EntryPointNotFoundException) { hasScenarioSoundExport = false; }
        }
        bool hasScenarioSoundExport = true;

        bool hasPodiumExport = true;
        void CollectPodium(Results r)
        {
            if (!hasPodiumExport) return;
            // the Free Ride has no race driver (rx_session_race is not valid there), but its activity plays FreerideIntroScenario in Pre
            // (the arrival: the hovercraft / sky crane / box car, then the hero shot): the core's own test (state Pre / Countdown) decides
            int state = r.hasRace ? r.race.state : -1;
            bool post = state == 4, intro = !r.hasRace || state == 0 || state == 1;
            if (!post && !intro) return;
            try
            {
                var info = default(RxPodiumInfo);
                if (intro)   // is the intro scenario playing? (info only: the characters' bones are evaluated only when it does)
                {
                    if (RxApiNative.rx_session_podium(session, ref info, null, 0, null, 0, null, 0) != 0 || info.scenario != 3) return;
                }
                if (RxApiNative.rx_session_podium(session, ref info, r.podiumRiders, (uint)r.podiumRiders.Length, r.podiumMatrices,
                                                  (uint)(r.podiumMatrices.Length / 16), r.podiumSlots, (uint)r.podiumSlots.Length) != 0) return;
                r.podium = info; r.hasPodium = true;
                r.podiumRiderCount = Math.Max(0, Math.Min(info.riders, r.podiumRiders.Length));
                r.podiumSlotCount = Math.Max(0, Math.Min(info.slots, r.podiumSlots.Length));
            }
            catch (EntryPointNotFoundException) { hasPodiumExport = false; }
        }

        void CollectObjects(Results r)
        {
            int n = objectVtables.Length;
            if (n == 0) return;
            if (r.objects.Length != n * 16) { r.objects = new float[n * 16]; r.objectsValid = new bool[n]; }
            if (!hasSceneObjects) { r.objectsValid = new bool[n]; return; }
            for (int i = 0; i < n; i++)
            {
                try { r.objectsValid[i] = RxApiNative.rx_session_scene_object_matrix(session, objectVtables[i], objectIds[i], objectOffsets[i], objectScratch) == 0; }
                catch (EntryPointNotFoundException) { hasSceneObjects = false; r.objectsValid[i] = false; break; }
                if (r.objectsValid[i]) Array.Copy(objectScratch, 0, r.objects, i * 16, 16);
            }
        }
        readonly float[] objectScratch = new float[16];

        void CollectMud(Results r)
        {
            var slots = trackedSlots;
            int n = Math.Min(1 + slots.Length, MaxMud);
            for (int i = 0; i < n; i++)
            {
                r.mudValid[i] = RxApiNative.rx_session_mud(session, i == 0 ? -1 : slots[i - 1], ref r.mud[i]) == 0;
                if (r.mudValid[i]) r.mudCount++;
            }
            for (int i = n; i < MaxMud; i++) r.mudValid[i] = false;
        }

        void CollectSlots(Results r)
        {
            var slots = trackedSlots; var near = slotRiderNear;
            if (!hasSlotPoses || slots.Length == 0) return;
            long watch = Stopwatch.GetTimestamp();
            int n = slots.Length; slotFrame++;
            if (r.slots.Length != n)
            {
                r.slots = new int[n]; r.slotBikeNodes = new int[n]; r.slotRiderBones = new int[n];
                r.slotBikePose = new float[n][]; r.slotRiderPose = new float[n][];
                for (int i = 0; i < n; i++) { r.slotBikePose[i] = new float[MaxSlotNodes * 16]; r.slotRiderPose[i] = new float[RiderBoneCount * 16]; }
            }
            try
            {
                for (int i = 0; i < n; i++)
                {
                    r.slots[i] = slots[i];
                    r.slotBikeNodes[i] = RxApiNative.rx_session_bike_pose_slot(session, slots[i], r.slotBikePose[i], MaxSlotNodes);
                    bool due = i >= near.Length || near[i] || farRiderInterval <= 1 || (slotFrame + i) % farRiderInterval == 0;
                    r.slotRiderBones[i] = due ? RxApiNative.rx_session_rider_pose_slot(session, slots[i], r.slotRiderPose[i], RiderBoneCount) : 0;
                }
                r.slotCount = n;
                if (hasNodeFlags)
                {
                    if (r.slotNodeFlags.Length != n)
                    {
                        r.slotNodeFlags = new byte[n][]; r.slotNodeFlagCounts = new int[n];
                        for (int i = 0; i < n; i++) r.slotNodeFlags[i] = new byte[MaxSlotNodes];
                    }
                    try { for (int i = 0; i < n; i++) r.slotNodeFlagCounts[i] = RxApiNative.rx_session_node_flags(session, slots[i], r.slotNodeFlags[i], MaxSlotNodes); }
                    catch (EntryPointNotFoundException) { hasNodeFlags = false; for (int i = 0; i < n; i++) r.slotNodeFlagCounts[i] = 0; }
                }
            }
            catch (EntryPointNotFoundException) { hasSlotPoses = false; r.slotCount = 0; }
            r.slotMilliseconds = Ms(watch);
        }

        RxTerrainInfo terrainInfo;
        readonly RxTerrainRect[] terrainRects = new RxTerrainRect[16];   // rx_session_terrain_tiles' answer (at most RX_TERRAIN_MAX_RECTS)
        bool hasTerrainTiles = true;

        // The API's first terrain call reports the whole grid dirty; later calls only when deform_version changed. The changed texels
        // come as up to 16 rectangles of whole 32 x 32 tiles (rx_session_terrain_tiles: a frame changes 2-7 tiles, but the riders'
        // ruts are far apart, so their one bounding rectangle was 45-100 k texels: w-perfgrow); an older core (no such call) gives
        // the one bounding rectangle. One TerrainPatch per rectangle; ApplyPatch handles any rectangle.
        void CollectTerrain(Results r)
        {
            uint previous = terrainInfo.deform_version;
            if (RxApiNative.rx_session_terrain(session, ref terrainInfo) != 0) return;
            r.terrainInfo = terrainInfo; r.hasTerrainInfo = true;
            if (terrainInfo.deform_version == previous) return;
            int n = (int)terrainInfo.samples;
            int count = 0;
            if (hasTerrainTiles)
            {
                try { count = RxApiNative.rx_session_terrain_tiles(session, terrainRects, terrainRects.Length); }
                catch (EntryPointNotFoundException) { hasTerrainTiles = false; count = 0; }
            }
            if (count > 0)
            {
                for (int i = 0; i < count; i++) AddTerrainPatch(r, n, terrainRects[i].x0, terrainRects[i].z0, terrainRects[i].x1, terrainRects[i].z1);
                return;
            }
            AddTerrainPatch(r, n, terrainInfo.dirty_x0, terrainInfo.dirty_z0, terrainInfo.dirty_x1, terrainInfo.dirty_z1);
        }

        void AddTerrainPatch(Results r, int n, int rx0, int rz0, int rx1, int rz1)
        {
            int x0 = Math.Max(0, rx0), x1 = Math.Min(n, rx1);
            int z0 = Math.Max(0, rz0), z1 = Math.Min(n, rz1);
            if (x1 <= x0 || z1 <= z0) return;
            // pooled: rented arrays may be longer than w * h; the consumer returns them (TerrainPatchArrays)
            var patch = new TerrainPatch { x0 = x0, z0 = z0, w = x1 - x0, h = z1 - z0, heights = TerrainPatchArrays.Rent((x1 - x0) * (z1 - z0)) };
            if (RxApiNative.rx_session_terrain_heights(session, patch.heights, x0, z0, patch.w, patch.h) != 0) { TerrainPatchArrays.Return(patch.heights); return; }
            patch.deform = TerrainPatchArrays.Rent(patch.w * patch.h);
            if (RxApiNative.rx_session_terrain_deform(session, patch.deform, x0, z0, patch.w, patch.h) != 0) { TerrainPatchArrays.Return(patch.deform); patch.deform = null; }
            r.terrain.Add(patch);
        }

        // Older uncollected results merge into a newer one (Complete): a patch that lies inside a LATER patch is overwritten by it (the later
        // one was read from the core a frame later), so it is dropped and its arrays go back to the pool. Keeps the list short when the
        // render frame is slower than the core's.
        static void DropCoveredPatches(List<TerrainPatch> list)
        {
            if (list.Count < 2) return;
            for (int i = list.Count - 2; i >= 0; i--)
            {
                var a = list[i];
                for (int j = i + 1; j < list.Count; j++)
                {
                    var b = list[j];
                    if (a.x0 >= b.x0 && a.z0 >= b.z0 && a.x0 + a.w <= b.x0 + b.w && a.z0 + a.h <= b.z0 + b.h)
                    {
                        if (a.heights != null) TerrainPatchArrays.Return(a.heights);
                        if (a.deform != null) TerrainPatchArrays.Return(a.deform);
                        list.RemoveAt(i);
                        break;
                    }
                }
            }
        }

        // Tyre-track ribbons: the core's own TerrainTracks buffers, re-read when the game flushed new vertices (at most every
        // 4th frame, as the old plugin's CollectTracks did). The export is asked only on those frames: the info-only call decodes
        // every live chunk of the three rings just to count triangles (0.4 -> 0.23 ms a frame at 1000 chunks, w-perfgrow), so it is
        // not made on the frames in between. On a fetch frame the core decodes once and the three calls share it (w-terrtiles).
        void CollectTracks(Results r)
        {
            r.tracksChanged = false; r.trackRibbonCount = 0;
            if (!hasTracksExport) return;
            framesSinceTracks++;
            if (framesSinceTracks < 4) return;
            try
            {
                var info = new RxTrackInfo();
                if (r.tracks == null) r.tracks = new float[MaxTrackTriangles * 18];
                int n = RxApiNative.rx_session_tracks(session, r.tracks, (uint)MaxTrackTriangles, ref info);
                if (n < 0) { hasTracksExport = false; return; }
                r.trackInfo = info;
                if (info.version == tracksVersion) return;   // nothing flushed since the last fetch (the game is not advancing)
                r.tracksChanged = true; r.trackTriangles = n;
                if (r.trackRibbons == null || r.trackRibbons.Length < MaxTrackRibbons) r.trackRibbons = new RxTrackRibbon[MaxTrackRibbons];
                int nr = RxApiNative.rx_session_track_ribbons(session, r.trackRibbons, (uint)r.trackRibbons.Length, ref info);
                r.trackRibbonCount = nr < 0 ? 0 : nr;
                tracksVersion = info.version; framesSinceTracks = 0;
            }
            catch (EntryPointNotFoundException) { hasTracksExport = false; r.trackTriangles = 0; r.tracksChanged = false; }
        }

        void Teardown()
        {
            bool careerSaved = false;
            if (session != IntPtr.Zero && Career != null && CurrentPhase == Phase.Ready && State != RxSessionState.Error)
            {
                try { Career.RefreshWorker(session); careerSaved = Career.DurablySettled; }
                catch (Exception e) { Log("Career final status: " + e); }
            }
            Replay?.CloseWorker();
            Career?.CloseWorker();
            if (session != IntPtr.Zero)
            {
                // RESTORED CONTENT: a championship weekend's stage records its places when its race ended (RxChampWeekend)
                if (Career == null) try { RxChampWeekend.AtSessionEnd(session); } catch (Exception e) { Log("Championship weekend: " + e.Message); }
                // Exhibition replay presenter released before the final records (its unpainted requests go with the renderer).
                if (Career == null) try { Replay?.EndEventWorker(session); } catch (Exception e) { Log("Replay release: " + e.Message); }
                // (the tutorial's end takes the local player out of the UI world: the session reads LOADING after it, so whether the ride
                // settles is decided before it)
                bool tutorialSettled = tutorial && CurrentPhase == Phase.Ready && State != RxSessionState.Error && State != RxSessionState.Loading;
                bool tutorialEnd = tutorial && CurrentPhase == Phase.Ready && State != RxSessionState.Error;
                // TutorialPauseMenu:OnQuit / TutorialConclusionPopup:End: App.EnableTutorialMode( FALSE ), SuspendPlayerInput FALSE and the vehicle
                // equipped before the MX Lite again (MXTableHelper:EquipVehicle) before the session goes
                void EndTutorial()
                {
                    try { if (RxTutorialNative.End(session) != 0) Log("tutorial end: " + Clean(RxApiNative.rx_session_last_error(session))); }
                    catch (Exception e) { Log("tutorial end: " + e.Message); }
                }
                if (tutorial)
                {   // the records are settled while the local player is still in the UI world's Free Ride (the settlement reads its activity); the
                    // profile is exported after the end, as the original's save (RequestSaveGame's worker) is written after the re-equip
                    try { playerCheckpoint.Finish(new NativePlayerSnapshotBackend(session), tutorialSettled, tutorialEnd ? EndTutorial : (Action)null); }
                    catch (Exception e) { Log("Player checkpoint: " + e); }
                }
                if (testDrive && CurrentPhase == Phase.Ready && State != RxSessionState.Error)
                {   // the pause request's end of the test drive (EnableTestDrive( FALSE ), MXGarage_TestDriveEnd), then nothing is saved
                    try { if (RxTestDriveNative.End(session) != 0) Log("test drive end: " + Clean(RxApiNative.rx_session_last_error(session))); }
                    catch (Exception e) { Log("test drive end: " + e.Message); }
                }
                try
                {
                    if (testDrive) playerCheckpoint.Finish(new NativePlayerSnapshotBackend(session), false);   // (a ride of the garage: read only)
                    else if (tutorial) { }   // (settled above, before the tutorial's end)
                    else if (careerSaved) playerCheckpoint.FinishAlreadyCheckpointed();
                    else playerCheckpoint.Finish(new NativePlayerSnapshotBackend(session), Career == null && CurrentPhase == Phase.Ready && State != RxSessionState.Error && State != RxSessionState.Loading);
                }
                catch (Exception e) { Log("Player checkpoint: " + e); }
                try { RxApiNative.rx_session_destroy(session); }
                catch (Exception e)
                {
                    // A failed destroy gives no guarantee the global native arena was released.
                    // Keep all later sessions out of it until the process restarts.
                    string why = "The previous core session could not close safely. Please restart OpenReflex. " + e.Message;
                    lock (sessionOwnership) { if (arenaImage) arenaImageFailure = why; else arenaFailure = why; }
                    Fail(why);
                }
                session = IntPtr.Zero;
            }
            lock (sessionOwnership)
                if (counted) { counted = false; if (arenaImage) activeArenaSessions--; else activeSessions--; Monitor.PulseAll(sessionOwnership); }
            lock (gate)
            {
                if (CurrentPhase != Phase.Failed) phase = (int)Phase.Stopped;
                busy = false;
                Monitor.PulseAll(gate);
            }
        }

        /// Stops the worker (after the job it is running) and destroys the session on it. A load frame can take a while: if the worker
        /// does not stop in time it still destroys the session when that job ends.
        public void Dispose()
        {
            lock (gate) { stopping = true; queued = false; queuedFrames = 0; queuedDt = 0; }
            go.Set();
            if (!thread.Join(5000)) Log("the simulation thread did not stop in time; it destroys the session when its current job ends.");
        }
    }

    /// The terrain patches' arrays (TerrainPatch.heights / deform: rented by the simulation thread, returned by the consumer). The shared
    /// pool keeps arrays up to 1 M floats only: the end-of-race replay puts the whole 2048 x 2048 grid back at each clip, and its two 16 MB
    /// arrays were new every time (32 MB of garbage every few seconds behind the results, w-gcspikes); the large ones have a pool of their own.
    public static class TerrainPatchArrays
    {
        const int SharedMax = 1 << 20;
        static readonly System.Buffers.ArrayPool<float> s_large = System.Buffers.ArrayPool<float>.Create(1 << 24, 4);
        public static float[] Rent(int length) => length > SharedMax ? s_large.Rent(length) : System.Buffers.ArrayPool<float>.Shared.Rent(length);
        public static void Return(float[] array)
        {
            if (array == null) return;
            try { if (array.Length > SharedMax) s_large.Return(array); else System.Buffers.ArrayPool<float>.Shared.Return(array); }
            catch (ArgumentException) { }   // (not one of the pool's sizes: left to the collector)
        }
    }

    /// Sleeping for a fraction of a frame without oversleeping a whole system timer tick. Windows: a high-resolution waitable timer
    /// (Windows 10 1803+, ~0.5 ms), else the system timer raised to 1 ms (timeBeginPeriod, once per process) and Sleep; elsewhere
    /// Thread.Sleep (macOS / Linux sleep to ~0.1 ms). Callers spin / yield the last Margin seconds.
    public static class PreciseWait
    {
        static readonly bool Windows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
        public static double Margin => Windows ? 0.0012 : 0.0005;
        /// Sleep only when more than this is left (else yield): macOS as before w-perf (1.5 ms), Windows 2 ms.
        public static double Threshold => Windows ? 0.002 : 0.0015;
        [ThreadStatic] static IntPtr timer;
        [ThreadStatic] static bool tried;
        static bool periodRaised;
        const uint CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x2, TIMER_ALL_ACCESS = 0x1F0003, INFINITE = 0xFFFFFFFF;
        [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateWaitableTimerExW(IntPtr attributes, IntPtr name, uint flags, uint access);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetWaitableTimer(IntPtr timer, ref long dueTime, int period, IntPtr completion, IntPtr arg, bool resume);
        [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
        [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint period);

        /// Waits until Stopwatch time `until` (seconds, the Clock of OpenReflexSimulation / OriginalSimulation): sleeps to within Margin,
        /// then yields; at most `limit` seconds in all.
        public static void Until(double until, double limit)
        {
            double t0 = Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency, t = t0;
            while (t < until && t - t0 < limit)
            {
                double remaining = until - t;
                if (remaining > Threshold) Sleep(remaining - Margin); else Thread.Yield();
                t = Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency;
            }
        }

        public static void Sleep(double seconds)
        {
            if (seconds <= 0) return;
            if (!Windows) { Thread.Sleep(Math.Max(1, (int)(seconds * 1000))); return; }
            try
            {
                if (!tried) { tried = true; timer = CreateWaitableTimerExW(IntPtr.Zero, IntPtr.Zero, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS); }
                if (timer != IntPtr.Zero)
                {
                    long due = -(long)(seconds * 1e7);   // relative, 100 ns units
                    if (SetWaitableTimer(timer, ref due, 0, IntPtr.Zero, IntPtr.Zero, false)) { WaitForSingleObject(timer, INFINITE); return; }
                }
                if (!periodRaised) { periodRaised = true; timeBeginPeriod(1); }
            }
            catch (Exception) { periodRaised = true; }   // no kernel32 / winmm entry: plain Sleep
            int ms = (int)(seconds * 1000);
            if (ms >= 1) Thread.Sleep(ms); else Thread.Yield();
        }
    }
}
