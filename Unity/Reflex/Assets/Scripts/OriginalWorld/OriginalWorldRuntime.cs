using System;
using System.IO;
using System.Collections.Generic;
using Reflex.Port.OpenFrontEnd;
using System.Text;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // Runs the original game's input + physics update via libreflexworld and draws the result.
    public sealed class OriginalWorldRuntime : MonoBehaviour
    {
        // The bike and rider are built at run time from the player's own Reflex install (Content/ReflexBikeModel, ReflexRiderModel;
        // nothing of the game is in the scene): EnsureVehicle makes them before Create() needs them, unless a scene assigns its own.
        public Transform chassis, frontWheel, rearWheel, riderProxy;
        public OriginalRider rider;
        [Tooltip("Reflex/Vehicle (the bike and rider shader); a serialized reference keeps it in player builds.")]
        public Shader vehicleShader;
        public OriginalTerrain terrain;
        [Tooltip("Drives forks, swingarm, bars and tyres from the original model nodes (falls back to rigid bodies).")]
        public OriginalBikePose bikePose;
        [Tooltip("Scenery the original moves (start gate).")]
        public OriginalAnimatedObjects animatedObjects;
        [Tooltip("All original game sound: the running game issues its FMOD event commands, OriginalAudio plays the original events.")]
        public OriginalAudio gameAudio;
        /// The open path's vehicle sound layer (w-audio): the original engine emitters and the garage's engine packs, from rx_session_vehicle_audio.
        public OriginalVehicleSound VehicleSound { get; private set; }
        [Tooltip("Camera: 0 the original game's camera (intro, start gate, chase), 1 Dynamic FPV (first person), 2 Unity follow camera. F7 cycles.")]
        public int cameraMode = 0;
        public OriginalOnboardCamera onboard;
        ReflexCamera gameCamera; bool haveGameCamera;
        readonly OriginalSceneryModels sceneryModels = new OriginalSceneryModels();   // the static models a scenario script hides / shows
        /// The open core's scrolling textures run on the game's own offsets (OriginalSceneryModels); OriginalProps keeps its formula otherwise.
        public bool GameScrolling => sceneryModels.GameScrolling;
        Reflex.Port.RxCore.RxOnboardState onboardState;
        int openDrivingCamera = -1;
        bool soundFirstPersonKnown, soundFirstPerson;   // the core's first-person camera flag of the newest frame (RxSoundState.firstPersonView)
        public int DrivingCameraPreset => simulation?.Open != null ? openDrivingCamera : GetComponent<OriginalInput>()?.DrivingCamera ?? -1;
        /// The player's view is the first-person camera while a race runs (w-reverb): the game's MXFirstPersonCamera (DrivingCamera preset 3) or the
        /// Dynamic FPV camera. The camera sound effect (CameraSoundEffectsManager: a FirstPerson reverb on the camera) and the rider sounds' positions
        /// (the foley emitter puts them on the camera, 0068bfb0) follow it. Not in the intro / the start gate (Pre) and after the race (Post).
        public bool FirstPersonView
        {
            get
            {
                // the game's own camera (mode 0): the core's flag (rx_session_first_person_view: the player's selected camera is the MXFirstPersonCamera with a
                // valid pose = the camera sound effect's active flag, 0068bfb0) when the loaded rxcore has it; the host's cameras (Dynamic FPV, Unity follow)
                // are not the game's, they keep the approximation below
                if (Running && cameraMode == 0 && soundFirstPersonKnown) return soundFirstPerson;
                if (!Running || race.valid == 0 || race.state < 1 || race.state > 3) return false;
                if (cameraMode == 1) return onboard && onboard.Active;
                return cameraMode == 0 && DrivingCameraPreset == 3;
            }
        }
        public Camera chaseCamera;
        public UnityEngine.UI.Text statusLabel, telemetryLabel, helpLabel, raceLabel;
        public bool paused;
        public Reflex.Port.OpenFrontEnd.OwnRacePause racePause;
        /// The Event Intro page's title card (OwnEventIntro); the HUD page starts only with the countdown, so no HUD shows under it.
        public Reflex.Port.OpenFrontEnd.OwnEventIntro eventIntro;
        public bool IntroCardShown => eventIntro && eventIntro.Shown;
        public bool OwnPauseActive => racePause && racePause.Active;
        public Reflex.Port.OpenFrontEnd.OwnTestRide testRide;              // the garage's TEST RIDE: its HUD and pause page (OpenFrontEnd/OpenTestRide.cs)
        public Reflex.Port.OpenFrontEnd.OwnLoadArena loadArena;            // the test-ride arena while an event loads: its card and PRESS START (OpenFrontEnd/OpenLoadArena.cs)
        public Reflex.Port.OpenFrontEnd.OwnTutorial tutorial;              // the MotoSkills tutorials: the lessons' pages and HUD (OpenFrontEnd/OpenTutorial.cs)
        public OwnRaceResults raceResults;
        public Reflex.Port.OpenFrontEnd.OwnPreChallenge preChallenge;     // the rules popup before a Free Ride challenge starts (OwnActivationArea attaches it)
        public Reflex.Port.OpenFrontEnd.OwnPostChallenge postChallenge;   // the end-of-challenge page of a Free Ride challenge (OwnActivationArea attaches it)
        public OwnRaceResultsSnapshot OwnRaceResultsSnapshot { get; private set; }
        public long WorldGeneration { get; private set; }
        public string EventIdentity { get; private set; }
        bool openExhibitionLaunch, resultsWaiting, resultsPausedBefore;
        float resultsWaitStarted;
        readonly Dictionary<int, bool> resultRosterSlots = new Dictionary<int, bool>();
        readonly Dictionary<int, string> resultRosterNames = new Dictionary<int, string>();
        public bool OfflineExhibitionOpen => openExhibitionLaunch && Running && simulation.Mode == ReflexSimMode.Open
            && resultRosterSlots.Count > 0 && !Reflex.Port.Content.ReflexGarage.TestDrive.Active
            && Reflex.Port.Content.ReflexCareer.Current == null
            && !(TryGetComponent<OriginalCareerRace>(out var career) && career.HasRace);
        // Select presentation before OriginalHud.Tick can construct an imported post page.
        // Post 0/1 continues advancing normally; only a settled Post 2 is frozen for results.
        public bool OwnResultsPresentation => OwnRaceResultsPolicy.OwnsPost(OfflineExhibitionOpen, race, EventUi);
        /// The REPLAY / PHOTO MODE viewer is open (Reflex.Port.Replay.ReflexReplay): the world holds, its camera and HUD make way.
        public bool ViewerActive { get; set; }
        /// The replay viewer plays its recorded sound: the game's sound player ticks on real time while the world stays paused.
        public bool ViewerAudio { get; set; }
        /// A career page (rules, hub, results) is up over the world and takes the input.
        public bool CareerPageUp => careerPresenter?.BlocksInput ?? false;
        public bool OwnMenuActive => ViewerActive || (careerPresenter?.BlocksInput ?? false) || OwnPauseActive || resultsWaiting || OwnResultsPresentation || raceResults && raceResults.Active || postChallenge && postChallenge.Active || preChallenge && preChallenge.Active || testRide && testRide.MenuOpen || tutorial && tutorial.BlocksInput || Reflex.Port.OpenFrontEnd.ChampWeekendFlow.CardUp(this);
        public Reflex.Port.RxCore.RxEventUiState EventUi { get; private set; }

        [Tooltip("The level (Database name, e.g. NAT_Concord, FR_Alpine). Its running world is made on first use from the player's install (Content/ReflexWorldFiles) and cached per user.")]
        public string level = "";
        [Tooltip("Legacy: the old captured-world file name; only used to infer the level when 'level' is empty.")]
        public string worldFile = "alpine-world.bin";
        [Tooltip("Legacy (unused): the deformation grid now comes with the level's built world.")]
        public string deformFile = "alpine-deform.bin";
        [Tooltip("AI riders in the race (0..31 on the open core, RIDER CAP; the legacy world 0..11; ReflexEventSelect sets it from the event list).")]
        public int opponents = 0;
        [Tooltip("The AI riders' AIDifficulty (Easy 80, Normal 95, Hard 105, Pro 120).")]
        public int aiSkill = Reflex.Port.Content.ReflexWorldFiles.AiSkillNormal;
        [Tooltip("Event Duration in the event type's unit (laps, Freestyle's minutes; 0: the table's); the front end's choice.")]
        public int laps = 0;
        [Tooltip("The front end's Vehicle Select choice (rider, bike, vehicle class) built into the world; null: the profile's rider and bike.")]
        public Reflex.Port.Content.ReflexWorldFiles.Garage garage;
        [Tooltip("Draws the race's other riders (their bikes and gear from the running game's tables, posed by the game). Added when the race has any.")]
        public OriginalOpponents opponentRiders;
        [Tooltip("The post-race presentation's podium riders (the game's finish scenario; OriginalPodium).")]
        public OriginalPodium podium;
        [Tooltip("The original particles (dust, roost, splashes, pyro, fireworks), simulated by the running game. Added when missing.")]
        public OriginalParticles particles;
        [Tooltip("The local player's bike and gear from the running game's tables (PlayerVehicle / PlayerVehicleParts / PlayerRiderGear) instead of the fixed MarsF Lite + Answer outfit.")]
        public bool vehicleFromTables = true;
        Reflex.Port.Content.ReflexWorldFiles pendingFiles;   // world being built from the install
        ReflexEventSelect eventSelect;
        [Tooltip("Simulate all decorative ambient animation (crowds, crew chiefs, looping/spline animations). Never read by gameplay; costs most of a national's frame. Off: only the classes OriginalAmbient draws run (drawAmbient, OriginalAmbient.mask).")]
        public bool simulateAmbientAnimation = false;
        [Tooltip("Draws the original ambient animated entities (crew chiefs, crowds, flaggers, spline / scenario animations; OriginalAmbient): their classes keep their original update (OriginalAmbient.mask) while the rest of the ambient animation stays skipped.")]
        public bool drawAmbient = true;
        public OriginalAmbient ambient;
        /// The drivable hovercraft of the Free Ride map (port extra, OriginalHovercraft); null elsewhere.
        public OriginalHovercraft hovercraft;
        /// The static scenery models the game's scenario scripts hide / show (the port's extras may suppress one).
        public OriginalSceneryModels SceneryModels => sceneryModels;
        /// The path length the local bike has ridden in this world, in feet (the game's unit): the sum of the presented position's steps, a step of a respawn / reset (over
        /// 150 ft in one frame) not counted. Nothing in the core keeps an odometer (the original's pages have no distance), so the pause page's Free Ride DISTANCE tile
        /// (OwnModes) is this: Unity-side, read-only, from the position already exported.
        public float RideDistance { get; private set; }
        Vector3 odoLast; bool odoHave;
        void Odometer()
        {
            if (state.renderPosition == null) return;
            var p = P(state.renderPosition, 0);
            if (!float.IsFinite(p.x + p.y + p.z)) return;
            if (odoHave) { float d = Vector3.Distance(p, odoLast); if (d < 150f) RideDistance += d; }
            odoLast = p; odoHave = true;
        }
        /// The local bike's presented position and velocity (the last applied frame); false before the first.
        public bool TryGetBike(out Vector3 position, out Vector3 velocity)
        {
            position = velocity = default;
            if (!haveState || state.renderPosition == null || state.velocity == null) return false;
            position = P(state.renderPosition, 0); velocity = new Vector3(state.velocity[0], state.velocity[1], state.velocity[2]);
            return true;
        }
#if UNITY_EDITOR
        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]
        public int[] testHeldKeys = Array.Empty<int>();
#endif

        IntPtr world;
        IReflexSimDriver simulation;   // legacy: OriginalSimulation (libreflexworld); open: OpenReflexSimulation (librxcore)
        float pendingDt;
        double simMilliseconds;
        ReflexBikeState state;
        Reflex.Port.Content.ReflexVehicleInfo vehicleInfo; bool hasVehicleInfo;   // reflex_world_vehicle: the class; an ATV's six bodies (ReflexAtv.cs)
        /// The local vehicle (class, bodies) of the last completed frame; false before the first or with an older plugin.
        public bool TryGetVehicle(out Reflex.Port.Content.ReflexVehicleInfo info) { info = vehicleInfo; return hasVehicleInfo; }
        // w-rpmgauge: the local rider's engine of the presented frame, from the vehicle sound layer's snapshot (rx_session_vehicle_audio,
        // the same values the engine sound follows): the real engine speed (the simulation's S+0x8abc x 9.55: clutch slip, wheel spin, the
        // limiter, revs in the air) and the vehicle's idle / limiter rpm (MX S+0x505c / S+0x5014, ATV S+0x4c44 / S+0x4bfc ...); engineDead: the
        // rider is wrecked (MX) or the engine drowned: the simulation stops stepping the engine (its rpm holds the last value) and the engine
        // sound leaves the rpm for its Wreck / Off state
        float engineRpm, engineIdleRpm, engineMaxRpm; bool hasEngine, engineDead;
        /// The local rider's engine speed and its idle / rev-limit rpm (the presented frame); false on the legacy path or before the first.
        public bool TryGetEngine(out float rpm, out float idleRpm, out float maxRpm) { rpm = engineRpm; idleRpm = engineIdleRpm; maxRpm = engineMaxRpm; return hasEngine; }
        /// The local rider is wrecked or the engine drowned (TryGetEngine's rpm is then stale).
        public bool EngineDead => hasEngine && engineDead;
        void ReadEngine(Reflex.Port.RxCore.RxVehicleAudioState va)
        {
            hasEngine = false;
            if (va == null || !va.valid) return;
            Reflex.Port.RxCore.RxVehicleAudioRider local = null;
            for (int i = 0; i < va.riderCount && local == null; i++)
                if (hasVehicleInfo ? va.riders[i].vehicle == vehicleInfo.vehicle : (va.riders[i].flags & Reflex.Port.RxCore.RxVehicleAudioRider.LocalHuman) != 0) local = va.riders[i];
            if (local == null || !(local.maxRpm > 0f)) return;
            engineRpm = local.rpm; engineIdleRpm = local.idleRpm; engineMaxRpm = local.maxRpm; engineDead = local.wrecked || local.drowned; hasEngine = true;
        }
        ReflexRaceState race;
        /// The race state of the last completed frame.
        public ReflexRaceState Race => race;
        /// PRACTICE (w-lb-practice): this world's session is a practice session (an endless lap race: rx_session_practice).
        public bool IsPractice => simulation?.Open?.Practice == true;
        readonly byte[] keys = new byte[256];
        readonly ReflexPad[] pads = new ReflexPad[4];
        Quaternion frontBind, rearBind, riderBind;
        Vector3 cameraVelocity;
        bool calibrated, haveState;
        string failure, missingFiles;
        string statusText = "Starting";

        public bool Running => simulation != null && !pendingOpen && failure == null;
        bool pendingOpen;
        string openLevel;
        double openStarted;

        CareerLaunch ownedCareerLaunch;
        IPlayerSessionPersistence ownedCareerPersistence;
        IOwnCareerPresentationInstallation careerInstallation;
        IOwnCareerRuntimePresenter careerPresenter;
        OwnReplayRenderer replayRenderer;
        /// The end-of-race replay (the highlight reel) is installed and plays in this world: PostEventMenu's EventHasReplay (the results page offers HIGHLIGHT REEL).
        public bool ReplayAvailable => replayRenderer != null;
        /// The original's end-of-race replay is on screen now (its poses and camera): the OPEN REFLEX replay does not record it.
        public bool OriginalReplayShowing => replayRenderer?.RequiresExactPose ?? false;
        OwnGateView gateView;                // the career Gate Selection page's 3D view (OwnGateView)
        OriginalGateCards GateCards() => FindAnyObjectByType<OriginalLevel>()?.GateCards;
        /// The career's first Gate Selection page comes before its race activity exists (the world is not running yet): the page's 3D view needs the
        /// level, so it is built now, where PollOpenStartup would build it with the first frames (it keeps what is loaded).
        void PrepareGateLevel()
        {
            if (!OwnedCareerActive || careerEntry == null) return;
            string loc = simulation?.Open?.Career.Snapshot?.Status.Location;
            if (string.IsNullOrEmpty(loc)) return;
            var content = FindAnyObjectByType<OriginalLevel>();
            if (!content) return;
            string activity = CareerSceneActivity;
            if (content.Loaded && content.level == loc && content.activity == activity) return;
            content.level = loc; content.activity = activity; content.runtime = this; content.terrain = terrain; content.LoadNow();
        }
        /// The Gate Selection page's terrain (OwnGateView): the level's terrain the core has loaded, built before the race's startup snapshot
        /// (which builds it again with the race's world); the page then shows the start gate on its ground, not over the camera's clear colour.
        /// The Gate Selection page shows the world: its camera is on a gate and the level's terrain is built (the career's loading page leaves).
        public bool GateViewShown => gateView != null && gateView.Active && gateView.Ready && gateView.Focus >= 0 && terrain && terrain.Built;
        void BuildGateTerrain(RxTerrainInfo info, float[] heights, float[] deformation)
        {
            if (!terrain || terrain.Built || !pendingOpen || !OwnedCareerActive) return;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            terrain.Build(info, heights, deformation);
            if (terrain.TryGetComponent<OriginalTracks>(out var tracks)) tracks.Init(terrain.UnityTerrain.materialTemplate, terrain.Info, null);
            Debug.Log($"[Gate] the level's terrain for the gate selection: {info.samples} x {info.samples}, built in {sw.Elapsed.TotalMilliseconds:F0} ms");
        }
        object careerWorldIdentity;
        object exhibitionReplayIdentity;   // the exhibition replay renderer's world identity (null: none installed)
        ulong careerGeneration;
        bool runtimeStarted, careerConsumed, careerPoseApplied;
        string careerFatal;
        public bool OwnedCareerActive => ownedCareerLaunch != null && simulation?.Open?.Career != null;
        public RxCareerPresenter OwnedCareerCapabilities => ownedCareerLaunch?.Presenters ?? 0;

        /// Prepare an inactive/new scene before Start. No menu entry is enabled by this API.
        /// The installation must contain actual renderers/routes and the persistence must own a complete profile lease.
        public void PrepareOwnedCareer(CareerEventChoice choice, IPlayerSessionPersistence persistence, IOwnCareerPresentationInstallation presenters, bool? motoSkipPractice = null)
        {
            if (runtimeStarted || simulation != null || careerConsumed || ownedCareerLaunch != null)
                throw new InvalidOperationException("Prepare career on a new runtime before Start.");
            if (choice == null || persistence == null || presenters == null) throw new ArgumentNullException("Career needs its series, profile persistence and an installed presenter.");
            if (!choice.CanSelect) throw new NotSupportedException("This career series is not supported by this runtime.");
            uint caps = (uint)presenters.Capabilities;
            uint required = (uint)choice.RequiredPresenters;
            if ((caps & ~(uint)RxCareerPresenter.AllV3) != 0 || required == 0 || (caps & required) != required)
                throw new InvalidOperationException("The installed presenter does not cover this career event.");
            if (!chaseCamera) throw new InvalidOperationException("Career replay requires an installed world camera.");
            careerEntry = choice.Entry;
            RequireCareerHud(choice.HudRoute);
            ownedCareerLaunch = new CareerLaunch((uint)choice.Entry.SeriesId, presenters.Capabilities, replayPresentation: true, motoSkipPractice: motoSkipPractice,
                format: choice.Entry.Format, hudRoute: choice.HudRoute, requiredPresenters: choice.RequiredPresenters, riders: choice.Entry.Riders);
            ownedCareerPersistence = persistence; careerInstallation = presenters;
        }
        CareerEventEntry careerEntry;
        // The career race's HUD: the package of the route the original requests for the series' race (OriginalHud).
        void RequireCareerHud(string route)
        {
            if (route == "HudNationalSinglePlayer" || route == "HudWaypointRaceSinglePlayer") return;   // the default National package
            if (!originalHud) throw new InvalidOperationException("Career requires its authored HUD renderer (" + route + ").");
            if (route == "HudPowercrossSinglePlayer") originalHud.RequirePowercrossHud();
            else if (route == "HudSupercrossSinglePlayer") originalHud.RequireSupercrossHud();
            else originalHud.RequirePackageHud(route);
            if (!originalHud.Built) throw new InvalidOperationException("The authored HUD " + route + " did not build.");
        }
        // The scene section of the career race ("<level><Activity>[<track>]", ReflexEvents.SceneSection) and the music's activity.
        string CareerSceneActivity => careerEntry.Activity + (careerEntry.TrackId > 0 ? careerEntry.TrackId.ToString() : "");
        string CareerMusicActivity => careerEntry.Activity == "WaypointRace" ? "FreeRide" : careerEntry.Activity;

        void TickOwnedCareer()
        {
            var open = simulation?.Open;
            if (!OwnedCareerActive) return;
            if (open.CurrentPhase == OpenReflexSimulation.Phase.Failed)
            { Fail(open.Failure ?? "Career worker failed."); return; }
            var snapshot = open.Career.Snapshot;
            if (snapshot == null || snapshot.Status.generation == 0) return;
            if (careerPresenter == null)
            {
                careerGeneration = snapshot.Status.generation;
                var identity = careerWorldIdentity; var generation = careerGeneration;
                Func<bool> current = () => ReferenceEquals(careerWorldIdentity, identity) && careerGeneration == generation && simulation?.Open == open;
                gateView = new OwnGateView(chaseCamera, open.BetweenFrames, current, GateCards, PrepareGateLevel, BuildGateTerrain);
                var installed = careerInstallation.Create(new OwnCareerRuntimeContext(transform, identity, generation,
                    open.Career, current, () => { if (current()) open.WakeCareerCommands(); },
                    message => { if (current()) careerFatal = message ?? "Career presentation failed."; }, gateView, () => replayRenderer != null));
                if (!current()) { installed?.Dispose(); return; }
                careerPresenter = installed;
                if (open.Replay != null) replayRenderer = new OwnReplayRenderer(transform, chaseCamera, identity, generation,
                    open.Replay, current, () => { if (current()) open.WakeCareerCommands(); });
                if (careerPresenter == null) throw new InvalidOperationException("Career presenter installation returned no renderer.");
            }
            if (snapshot.Status.generation != careerGeneration) throw new InvalidOperationException("Career generation changed without a new world.");
            replayRenderer?.Tick();
            careerPresenter.Tick(careerWorldIdentity, careerGeneration, Time.realtimeSinceStartupAsDouble);
            if (careerFatal != null) { var message = careerFatal; careerFatal = null; Fail(message); }
        }
        // Exhibition events (no career): the original plays the end-of-race replay for every race that reaches Post, so the open
        // session gets the same replay presenter (OpenReflexSimulation replayPresentation) and this world installs the same renderer
        // as a career world once the presenter has its generation.
        void TickExhibitionReplay()
        {
            var open = simulation?.Open;
            if (OwnedCareerActive || open?.Replay == null) return;
            if (exhibitionReplayIdentity == null)
            {
                ulong generation = open.Replay.Generation;
                if (generation == 0 || open.CurrentPhase != OpenReflexSimulation.Phase.Ready) return;
                if (!chaseCamera) throw new InvalidOperationException("The event replay requires an installed world camera.");
                var identity = new object(); exhibitionReplayIdentity = identity;
                Func<bool> current = () => ReferenceEquals(exhibitionReplayIdentity, identity) && simulation?.Open == open;
                replayRenderer = new OwnReplayRenderer(transform, chaseCamera, identity, generation, open.Replay, current, () => { if (current()) open.WakeCareerCommands(); });
            }
            replayRenderer?.Tick();
        }
        bool CareerFinalized => simulation?.Open?.Career?.Snapshot?.Status.records_finalized == 1;
        bool CareerNeedsPostFrames => OwnedCareerActive && !CareerFinalized && simulation.Open.Career.Snapshot?.Status.activity_state == 4;
        // The original plays the end-of-race replay behind its results page: while the exhibition results hold the world paused (no
        // input reaches the game), the Post frames keep running for the replay exactly as a career's Post does.
        bool ExhibitionNeedsPostFrames => exhibitionReplayIdentity != null && raceResults && raceResults.Active && race.valid != 0 && race.state == 4;

        // The display rate: with interpolated presentation the game keeps its own 60 Hz and Unity draws at the display's refresh rate
        // (vsync when QualitySettings has it: then no cap); the old pacing needed one Unity frame per game frame (60).
        void Awake() => ApplyClientInfoVisibility();

        void ApplyClientInfoVisibility()
        {
            var canvas = statusLabel ? statusLabel.GetComponentInParent<Canvas>(true) : null;
            if (canvas) canvas.enabled = HudVisible;
        }

        public LocalGamepadRouting PadRouting { get; } = new LocalGamepadRouting();
        readonly Gamepad[] heldPadDevices = new Gamepad[4];

        void Start()
        {
            runtimeStarted = true;
            PadRouting.BeginSession();
            ApplyClientInfoVisibility();
            Application.runInBackground = true;
            ApplyFrameRate();
            Reflex.Port.Options.ReflexClientLive.OnWorldStart(this);   // the client's parts (MODERN HUD, pause extras, rumble strength; Options)
            if (!GetComponent<OriginalInput>()) gameObject.AddComponent<OriginalInput>();
            if (!GetComponent<OriginalOnboardAdjust>()) gameObject.AddComponent<OriginalOnboardAdjust>();
            eventSelect = GetComponent<ReflexEventSelect>(); Create();
        }
        /// The race pacing after the shared launch/user cap and VSync policy.
        public void ApplyFrameRate()
        {
            Reflex.Port.Options.ReflexClientLive.ApplyLatency();   // LOW LATENCY: the driver's queued frames
            if (Reflex.Port.Options.ReflexClientLive.ApplyFrameRateLimit()) return;
            // Original pacing: the game's own loop caps its frames at 1/60 s (StartRealFrame, as Engine::UpdateTiming 0084ea80), so
            // Unity adds no limiter of its own. targetFrameRate 60 waited for the next fixed 1/60 slot, turning every frame that ran
            // slightly over 16.7 ms into a 33 ms one (profiled 2026-10-25: 13% of race frames, WaitForTargetFPS 14-17 ms in each).
            Application.targetFrameRate = Pacing == PacingMode.Interpolated ? (QualitySettings.vSyncCount > 0 ? -1 : DisplayRate())
                                        : Pacing == PacingMode.Original ? -1 : QualitySettings.vSyncCount > 0 ? -1 : 60;
        }
        static int DisplayRate() { double hz = Platform.ReflexDisplayInfo.RefreshHz; return hz >= 30 && hz <= 500 ? (int)Math.Round(hz) : 60; }
        void OnDisable() { UnityEngine.Rendering.RenderPipelineManager.endContextRendering -= OnEndContextRendering; Release(); }
        void OnDestroy()
        {
            if (preInputOwner == this) preInputOwner = null; Release(); DestroyVehicle(); PadRouting.Dispose();
            // the static references to this world let go of it (they kept the whole destroyed world, its replay rings included, alive in
            // the menus until the next race: w-gcspikes)
            ControllerInputDiagnostics.Forget(this);
            Reflex.Port.Practice.RxPractice.WorldGone(this);   // (its lap list and the simulation driver went with the world only in PRACTICE)
        }
        void OnApplicationQuit() => Release();

        void Release()
        {
            // Invalidate callback identity before detach, then join/destroy the native owner.
            careerWorldIdentity = null; careerGeneration = 0; careerPoseApplied = false; careerFatal = null; exhibitionReplayIdentity = null;
            gateView?.End(); gateView = null;
            var oldReplay = replayRenderer; replayRenderer = null;
            try { oldReplay?.Dispose(); } catch (Exception e) { Debug.LogWarning("Replay renderer detach: " + e.Message); }
            var oldPresenter = careerPresenter; careerPresenter = null;
            try { oldPresenter?.Dispose(); } catch (Exception e) { Debug.LogWarning("Career presenter detach: " + e.Message); }
            if (raceResults) raceResults.CloseForWorldChange();
            if (resultsWaiting) paused = resultsPausedBefore;
            resultsWaiting = false; OwnRaceResultsSnapshot = null; ResultsPage = null;
            openExhibitionLaunch = false; EventIdentity = null; Waypoint = default; HasWaypoint = false; Powercross = default; HasPowercross = false; Activation = default; HasActivation = false; Challenge = default; HasChallenge = false; Compass = default; HasCompass = false;
            resultRosterSlots.Clear(); resultRosterNames.Clear();
            if (racePause) racePause.CloseForWorldChange();
            EventUi = default; ModeResults = default; HasModeResults = false;
            GetComponent<OriginalOnboardAdjust>()?.End();
            if (onboard) onboard.Deactivate();
            pendingOpen = false; pendingFiles = null;
            LogNativeGame();
            nativePortCount = 0;
            presentation?.CloseLog(); presentation = null;
            hovercraft?.End(); hovercraft = null;
            VehicleSound?.Dispose(); VehicleSound = null;
            if (simulation != null) { simulation.Dispose(); simulation = null; }
            if (world == IntPtr.Zero) { restarting = false; return; }
            var w = world; world = IntPtr.Zero;
            Reflex.Port.Content.ReflexPersistence.Leave(w, restarting); restarting = false;   // the game's saves of the race left / restarted (WP-7)
            try { ReflexWorldNative.reflex_world_destroy(w); } catch (Exception e) { Debug.LogWarning(e.Message); }
        }


        Reflex.Port.Content.ReflexBikeModel bikeModel;
        /// The local vehicle's model (diagnostics / test probes); null before it is built.
        public Reflex.Port.Content.ReflexBikeModel BikeModel => bikeModel;
        Reflex.Port.Content.ReflexRiderModel riderModel, proxyModel;
        string bikeKey, riderKey;
        /// What the last completed original frame cost on the simulation thread (ms).
        public double SimMilliseconds => simMilliseconds;

        // Builds the bike (chassis + wheels + parts, OriginalBikePose bound to the model nodes), the rider (MXRider_Hierarchy
        // skeleton + gear, OriginalRider) and the capsule stand-in from the install, for whatever the scene did not assign.
        // local: the local player's bike and gear as the running game's tables give them (null: MarsF Lite + Answer red); a
        // bike / rider built for other tables is rebuilt.
        void EnsureVehicle(ReflexRoster.Entry local = null)
        {
            var bikeSpec = Reflex.Port.Content.BikeMods.ForPlayer(local?.bike); var riderSpec = local?.rider;   // (the bike model the player chose: Original or a bike mod)
            string bk = (bikeSpec ?? Reflex.Port.Content.BikeSpec.MarsFLite).Key, rk = (riderSpec ?? Reflex.Port.Content.RiderSpec.AnswerRed()).Key;
            if (bikeModel != null && bk != bikeKey) { bikeModel.Destroy(); bikeModel = null; chassis = frontWheel = rearWheel = null; bikePose = null; }
            if (riderModel != null && rk != riderKey) { riderModel.Destroy(); riderModel = null; rider = null; }
            if (chassis && frontWheel && rearWheel && rider && riderProxy) return;
            if (vehicleShader) Reflex.Port.Content.ReflexVehicleShader.Override = vehicleShader;
            try
            {
                if (!chassis || !frontWheel || !rearWheel)
                {
                    bikeModel = Reflex.Port.Content.ReflexBikeModel.Build(bikeSpec, highResParts: true); bikeKey = bk;   // (the player's own: the garage's hand guards, bars and pipe, ReflexBikeModel.RideHighResParts)
                    chassis = bikeModel.chassis.transform; frontWheel = bikeModel.frontWheel; rearWheel = bikeModel.rearWheel; bikePose = bikeModel.pose;
                }
                if (!riderProxy) { proxyModel = Reflex.Port.Content.ReflexRiderModel.BuildProxy(); riderProxy = proxyModel.proxyRoot; }
                if (!rider)
                {
                    riderModel = Reflex.Port.Content.ReflexRiderModel.Build(riderSpec, proxyModel?.proxyArt); riderKey = rk;
                    rider = riderModel.rider;
                }
            }
            catch (Exception e) { Debug.LogWarning("Original bike / rider not built: " + e.Message, this); }
            SetVehicleActive(false);   // shown with the first state of the game (they are modelled at the origin)
        }

        void SetVehicleActive(bool on)
        {
            if (bikeModel != null) { bikeModel.chassis.SetActive(on); bikeModel.frontWheel.gameObject.SetActive(on); bikeModel.rearWheel.gameObject.SetActive(on); }
            if (proxyModel != null) proxyModel.proxyRoot.gameObject.SetActive(on);
            if (riderModel != null) riderModel.root.SetActive(on);
        }

        void DestroyVehicle()
        {
            bikeModel?.Destroy(); riderModel?.Destroy(); proxyModel?.Destroy(); bikeModel = null; riderModel = proxyModel = null;
            Reflex.Port.Content.VehicleCatalog.ReleaseTextures();
        }

        void Create()
        {
            if (ownedCareerLaunch != null && careerConsumed)
            { Debug.LogWarning("Select a career event explicitly in a new world; this career launch was already consumed."); return; }
            Release(); WorldGeneration++; failure = null; missingFiles = null; calibrated = false; haveState = false; RideDistance = 0; odoHave = false;
            state = ReflexBikeState.Create(); race = default; onboardState = default; openDrivingCamera = -1; soundFirstPersonKnown = soundFirstPerson = false; DriverCount = 0; Roster = null;
            haveHud = hasVehicleInfo = hasEngine = haveGameCamera = false; cameraRig.Reset();
            SetVehicleActive(false); Array.Clear(keys, 0, keys.Length);
            if (ownedCareerLaunch != null || OpenReflexSelector.UseOpenReflex) { CreateOpen(); return; }
            // Everything comes from the player's install: the executable, the VC90 runtime (from its vcredist) and the
            // level's running world, built by booting the original game once (cached per user).
            string lvl = string.IsNullOrEmpty(level) ? Reflex.Port.Content.ReflexWorldFiles.LevelOf(worldFile) : level;
            // (the legacy plugin's game has the original's 12 seats: a bigger field is the open core's, RIDER CAP)
            var files = Reflex.Port.Content.ReflexWorldFiles.For(lvl, Math.Min(opponents, Reflex.Port.RxCore.RxRiderCap.OriginalOpponents), aiSkill, laps, garage);
            string exe, crt, worldBin = files.World, deform = files.Deform, deformModel = files.DeformModel;
            try { exe = Reflex.Port.Content.ReflexInstall.Exe; crt = Path.Combine(Reflex.Port.Content.ReflexCrt.Dir, "msvcr90.dll"); }
            catch (Exception e) { Fail(e.Message); return; }
            if (!files.Ready)
            {
                try { if (!files.Building) files.Build(); }
                catch (Exception e) { Fail("world build: " + e.Message); return; }
                pendingFiles = files; statusText = "BUILDING " + lvl + " FROM YOUR REFLEX INSTALL";
                return;
            }
            files.Touch();
            try
            {
                ReflexWorldNative.VerifyLayout();
                var missing = new StringBuilder();
                foreach (var f in new[] { exe, crt, worldBin, deform }) if (!File.Exists(f)) missing.Append("\n  missing: ").Append(f);
                // the editor loads Assets/Plugins/macOS; a macOS player bundles native plugins in <app>/Contents/PlugIns (dataPath = Contents);
                // Windows (editor and player): Plugins/x86_64/reflexworld.dll under Assets / <name>_Data
                bool windows = Application.platform == RuntimePlatform.WindowsPlayer || Application.platform == RuntimePlatform.WindowsEditor;
                string plugin = Path.Combine(Application.dataPath, windows ? "Plugins/x86_64/reflexworld.dll" : Application.isEditor ? "Plugins/macOS/libreflexworld.dylib" : "PlugIns/libreflexworld.dylib");
                if (!File.Exists(plugin)) missing.Append("\n  missing plugin: ").Append(plugin);
                if (missing.Length > 0) { missingFiles = missing.ToString(); throw new FileNotFoundException("Original physics inputs are not installed yet." + missingFiles); }
                world = ReflexWorldNative.reflex_world_create(exe, crt, worldBin, deform);
                if (world == IntPtr.Zero) throw new InvalidOperationException("reflex_world_create failed: " + ReflexWorldNative.Error(IntPtr.Zero, "unknown error"));
                statusText = "LIVE ORIGINAL PHYSICS";
                Reflex.Port.Content.ReflexPersistence.Attach(world, Reflex.Port.Content.ReflexCareer.Current != null || Reflex.Port.Content.ReflexGarage.TestDrive.Active);   // the profile into the race (WP-7); a test drive is booted from the save (WP-G)
                Reflex.Port.Content.ReflexGarage.ApplyAttachedTuning(world, garage);   // an arcade race: the save's PlayerVehicleTuning on the running vehicle (WP-G)
                // The race's riders, bikes and gear from the running game's tables (read before the simulation thread owns the plugin).
                ReflexRoster roster = null; Roster = null;
                try { roster = Roster = ReflexRoster.Read(world); }
                catch (Exception e) { Debug.LogWarning("Original riders not read: " + e.Message, this); }
                EnsureVehicle(vehicleFromTables ? roster?.Local : null);
                Reflex.Port.Content.ReflexPlates.Apply(bikeModel, riderModel, roster?.Local?.plate);   // the local rider's number plate / jersey name
                // The plugin reads "<name>-deform-model.bin" (brush + softness mask) next to the deform grid; without it ruts are off.
                string model = deformModel;
                if (!File.Exists(model)) { statusText += "  (terrain deformation off: missing " + Path.GetFileName(model) + ")"; Debug.LogWarning("Terrain deformation disabled, missing " + model); }
                ReflexWorldNative.reflex_world_set_ambient_animation(world, simulateAmbientAnimation ? 1 : 0);
                ApplyNativeGame();
                bool ambientExport = drawAmbient;
                if (drawAmbient && !simulateAmbientAnimation)
                    try { ReflexAmbientNative.reflex_world_ambient_mask(world, ambient ? ambient.mask : OriginalAmbient.DefaultMask); }
                    catch (EntryPointNotFoundException) { ambientExport = false; }
                if (terrain) terrain.Build(world);
                if (terrain && terrain.Built && terrain.TryGetComponent<OriginalTracks>(out var tracks))
                    tracks.Init(terrain.UnityTerrain.materialTemplate, terrain.Info, deformModel);
                simulation = OpenReflexSelector.CreateLegacy(world, terrain != null);
                CompleteSceneStartup(roster, lvl, ambientExport);
            }
            catch (Exception e) { Fail(e.Message); }
        }

        // The Heat format's next round (OpenReflexSimulation.TakeRosterData): the riders the core re-created behind the AAA loading page
        // replace the previous round's opponents, results roster and podium characters.
        void PollOpenRoster()
        {
            if (pendingOpen || simulation == null || simulation.Open == null || Roster == null) return;
            var data = simulation.Open.TakeRosterData();
            if (data == null) return;
            Roster = ReflexRoster.ReadOpen(data.slots, data.appearances);
            resultRosterSlots.Clear(); resultRosterNames.Clear();
            foreach (var entry in Roster.entries) { resultRosterSlots.Add(entry.slot.slot, entry.Local); resultRosterNames.Add(entry.slot.slot, entry.name); }
            if (!opponentRiders && Roster.entries.Count > 1) opponentRiders = gameObject.AddComponent<OriginalOpponents>();
            if (opponentRiders) { if (!opponentRiders.viewer) opponentRiders.viewer = chaseCamera; opponentRiders.Begin(Roster, simulation); }
            if (podium) { if (!podium.viewer) podium.viewer = chaseCamera; podium.Begin(Roster); podiumHidLocal = false; }
            Debug.Log($"OPEN REFLEX roster rebuilt for the next round: {Roster.entries.Count} riders");
        }

        void CompleteSceneStartup(ReflexRoster roster, string lvl, bool ambientExport)
        {
                if (!TryGetComponent<OriginalInput>(out _)) gameObject.AddComponent<OriginalInput>();   // mouse, rumble, the game's UI settings (OriginalInput)
                StartPresentation();
                if (Reflex.Port.Content.ReflexCareer.Current != null)   // MOTOCAREER: the series event's in-race pages (OriginalCareerRace)
                    (GetComponent<OriginalCareerRace>() ?? gameObject.AddComponent<OriginalCareerRace>()).OnWorldCreated();
                if (!opponentRiders && roster != null && roster.entries.Count > 1) opponentRiders = gameObject.AddComponent<OriginalOpponents>();
                if (opponentRiders && roster != null) { if (!opponentRiders.viewer) opponentRiders.viewer = chaseCamera; opponentRiders.Begin(roster, simulation); }
                if (!podium && !TryGetComponent(out podium)) podium = gameObject.AddComponent<OriginalPodium>();
                if (!podium.viewer) podium.viewer = chaseCamera;
                podium.Begin(roster); podiumHidLocal = false;
                cameraMode = Mathf.Clamp(Reflex.Port.Options.ReflexSave.GetInt("ReflexCameraMode", cameraMode), 0, 2);
                if (onboard) { onboard.Bind(chaseCamera, rider); onboard.NewSession(); }   // (a new world: its seat is learnt again)
                HudVisible = HudVisible;
                if (originalHud) originalHud.visible = OriginalHudVisible;
                if (animatedObjects) simulation.TrackObjects(animatedObjects.entries);
                if (gameAudio) simulation.AttachAudio(gameAudio.Finished);
                if (!particles && !TryGetComponent(out particles)) particles = gameObject.AddComponent<OriginalParticles>();
                particles.level = lvl; if (!simulation.AttachParticles()) particles.visible = false;
                if (ambientExport)
                {
                    if (!ambient && !TryGetComponent(out ambient)) ambient = gameObject.AddComponent<OriginalAmbient>();
                    if (!ambient.viewer) ambient.viewer = chaseCamera;
                    ambient.simulatedMask = simulateAmbientAnimation ? OriginalAmbient.AllClasses : ambient.mask;
                    ambient.Begin(FindAnyObjectByType<OriginalLevel>()); simulation.AttachAmbient();
                }
                sceneryModels.Begin(FindAnyObjectByType<OriginalLevel>());
                OriginalHovercraft.Attach(this, lvl);   // the drivable hovercraft on the Free Ride map FR_Alpine (port extra)
                pendingDt = 0;
        }

        // Choose the managed install event before touching ReflexWorldFiles: even its cache Version probes libreflexworld.
        void CreateOpen()
        {
            try
            {
                bool ownedCareer = ownedCareerLaunch != null;
                string key = string.IsNullOrEmpty(level) ? Reflex.Port.Content.ReflexWorldFiles.LevelOf(worldFile) : level;
                var ev = ownedCareer ? null : key.Contains("/") ? Reflex.Port.Content.ReflexEvents.Find(key) : Reflex.Port.Content.ReflexEvents.ForLevel(key);
                if (!ownedCareer && ev == null) throw new InvalidOperationException("No install event for " + key);
                string pluginDir = RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor);
                string path = RxNativeLoader.ResolvePath(pluginDir);
                if (path == null) throw new FileNotFoundException("OPEN REFLEX core library missing; set REFLEX_RXCORE_PATH or bundle " + RxNativeLoader.FileName + ".");
                RxNativeLoader.Load(path, Application.isEditor ? OpenReflexTempRoot() : null);
                if (ownedCareer)
                {
                    if (careerInstallation.Capabilities != ownedCareerLaunch.Presenters)
                        throw new InvalidOperationException("Installed career capabilities changed before startup.");
                    if (Reflex.Port.Content.ReflexCareer.Current != null || Reflex.Port.Content.ReflexGarage.TestDrive.Active ||
                        (TryGetComponent<OriginalCareerRace>(out var oldCareer) && oldCareer.HasRace))
                        throw new InvalidOperationException("Another career or test drive owns this world.");
                    careerConsumed = true; careerWorldIdentity = new object();
                    var placeholder = new RxEventSpec(); RxApiNative.rx_session_event_default(ref placeholder);
                    // CareerWorker loads the actual authored event; this default is never an exhibition load.
                    simulation = OpenReflexSelector.CreateOpen(Reflex.Port.Content.ReflexInstall.Require(), placeholder,
                        terrain != null, true, persistence: ownedCareerPersistence, career: ownedCareerLaunch);
                    EventIdentity = "Career/" + ownedCareerLaunch.SeriesId + "#" + WorldGeneration;
                    try
                    {   // the Event Intro page's title card: the series' event (its type / location / track rows)
                        var entry = System.Linq.Enumerable.FirstOrDefault(Reflex.Port.Content.CareerEventCatalog.Read(), x => x.SeriesId == ownedCareerLaunch.SeriesId);
                        if (entry != null) Reflex.Port.OpenFrontEnd.OwnEventIntro.Attach(this, entry.EventTypeId, entry.LocationId, entry.TrackId, false);
                    }
                    catch (Exception e) { Debug.LogWarning("[EventIntro] career event rows: " + e.Message); }
                    openLevel = null; pendingOpen = true; openStarted = Time.realtimeSinceStartupAsDouble;
                    statusText = "LOADING CAREER";
                    return;
                }
                openLevel = ev.location;
                // The core's production default is the complete game frame. Do not
                // depend on managed environment writes reaching a native CRT cache.
                var resolvedSpec = OpenEventSpec(ev, opponents, aiSkill, laps, garage);
                openExhibitionLaunch = OwnRaceResultsPolicy.IsExhibitionSpec(resolvedSpec)
                    && !Reflex.Port.Content.ReflexGarage.TestDrive.Active && Reflex.Port.Content.ReflexCareer.Current == null
                    && !(TryGetComponent<OriginalCareerRace>(out var career) && career.HasRace);
                EventIdentity = resolvedSpec.EventType + "/" + resolvedSpec.Location + "#" + WorldGeneration;
                bool challenges = Reflex.Port.Content.ReflexModes.InChallengesFreeRide(ev);
                // the garage's TEST RIDE: the core's own test drive in the session's UI world (no level load), the garage's selection for this
                // session only and nothing saved (OpenFrontEnd/OpenTestRide.cs)
                bool testDrive = Reflex.Port.Content.ReflexGarage.TestDrive.Active && Reflex.Port.Content.ReflexGarage.TestDrive.Current.ev == ev;
                // the MotoSkills tutorials: the core's tutorial mode in the session's UI world; the profile is saved as for a ride (TutorialsCompleted)
                bool tutorialRun = !testDrive && Reflex.Port.Content.ReflexTutorials.Open.Active && Reflex.Port.Content.ReflexTutorials.Open.Current.ev == ev;
                // the test-ride arena while the player's event loads (OpenFrontEnd/OpenLoadArena.cs): the test drive in the core's second image
                bool arenaRide = testDrive && Reflex.Port.OpenFrontEnd.OpenLoadArena.Riding;
                // the shell's event load in the test-ride arena (OpenFrontEnd/OpenLoadArena.cs) started this event's session in the front end: the
                // same spec and settings are taken over here (its loading is over or under way); anything else starts the session now
                simulation = !testDrive && !tutorialRun && terrain != null && chaseCamera != null ? Reflex.Port.OpenFrontEnd.OpenEventPrestart.Take(resolvedSpec, challenges) : null;
                // the arena's ride: its session made ahead (kept warm in the menus or started at START EVENT: OpenFrontEnd/OpenArenaWarm.cs)
                if (arenaRide)
                {
                    Reflex.Port.OpenFrontEnd.OpenLoadArena.Mark("the race scene is up (its runtime starts the ride)");
                    if (terrain != null) simulation = Reflex.Port.OpenFrontEnd.OpenArenaWarm.Take(resolvedSpec, garage);
                }
                if (simulation == null)
                {
                    var persistence = Reflex.Port.Content.CoreGarageProfile.CapturePersistence(garage);
                    simulation = OpenReflexSelector.CreateOpen(Reflex.Port.Content.ReflexInstall.Require(), resolvedSpec, terrain != null, true,
                        persistence: arenaRide ? new Reflex.Port.RxCore.ArenaRidePersistence(persistence) : testDrive ? new Reflex.Port.RxCore.TestDrivePersistence(persistence) : persistence,
                        replayPresentation: chaseCamera != null && !testDrive && !tutorialRun, inCareerMode: challenges && !testDrive, testDrive: testDrive, tutorial: tutorialRun, arenaImage: arenaRide);
                }
                if (testDrive && FindAnyObjectByType<OriginalMusic>() is OriginalMusic uiMusic) uiMusic.uiWorld = true;   // the test ride / load arena in the UI world: no jukebox (OriginalMusic.uiWorld)
                if (arenaRide) loadArena = Reflex.Port.OpenFrontEnd.OwnLoadArena.Attach(this);
                else if (testDrive) testRide = Reflex.Port.OpenFrontEnd.OwnTestRide.Attach(this);
                if (tutorialRun) tutorial = Reflex.Port.OpenFrontEnd.OwnTutorial.Attach(this);
                Reflex.Port.OpenFrontEnd.OwnEventIntro.Attach(this, ev.spec.typeId, ev.spec.locationId, ev.spec.trackId, ev.spec.type == "FreeRide", ev);   // the Event Intro page's title card
                if (challenges) Reflex.Port.OpenFrontEnd.OwnActivationArea.Attach(this, ev);   // the activation areas: notification, pause start, EXIT
                pendingOpen = true; openStarted = Time.realtimeSinceStartupAsDouble;
                statusText = "LOADING " + openLevel;
                Debug.Log("OPEN REFLEX startup: core session only; no legacy world or cached snapshot. Library: " + path
                          + $". Event {resolvedSpec.EventType}/{resolvedSpec.Location}, duration {resolvedSpec.duration} {resolvedSpec.DurationType}, riders {resolvedSpec.riders}, AI {resolvedSpec.ai_difficulty}");
            }
            catch (Exception e) { Fail(e.Message); }
        }

        /// The core's spec of an install event as this world starts it: the event's rows, the front end's AI (opponents, aiSkill), Event Duration
        /// (laps) and Vehicle Select choice (garage), the profile's persona. The test-ride arena's pre-start (OpenEventPrestart) makes the same.
        internal static RxEventSpec OpenEventSpec(Reflex.Port.Content.ReflexEvents.Event ev, int opponents, int aiSkill, int laps, Reflex.Port.Content.ReflexWorldFiles.Garage garage)
        {
            var spec = new RxEventSpec();
            RxApiNative.rx_session_event_default(ref spec);
            var e = ev.spec;
            spec.EventType = e.type; spec.event_type_id = e.typeId;
            spec.EventActivation = e.activation ?? "";   // a Free Ride challenge (ReflexModes.Challenges): EventTypeActivation
            spec.Location = e.location; spec.location_id = e.locationId; spec.track_id = e.trackId;
            spec.DurationType = e.durationType; spec.duration = e.duration;
            spec.riders = e.riders; spec.ai_difficulty = e.ai; spec.vehicle_class = e.vehicleClass;
            if (e.type != "FreeRide" && opponents >= 0) { spec.riders = Math.Min(Reflex.Port.RxCore.RxRiderCap.Max, opponents + 1); spec.ai_difficulty = aiSkill; }   // RIDER CAP: up to 31 AI
            // EventDuration (LocationSelect.lua sets it for every type with a duration list: laps, Freestyle's minutes);
            if ((e.durationType == "Laps" || e.durationType == "Minutes") && laps > 0) spec.duration = laps;
            if (garage != null && garage.vehicleClass >= 0) spec.vehicle_class = garage.vehicleClass;
            // PRACTICE (w-lb-practice): you alone on the track (the core makes the lap race endless: rx_session_set_practice)
            if (RxPracticeMode.Current && RxPracticeMode.LapRace(e.type) && string.IsNullOrEmpty(spec.EventActivation)) { spec.riders = 1; spec.ai_difficulty = 0; }
            spec.PlayerName = Reflex.Port.Content.ReflexProfile.Persona;
            return spec;
        }

        // Startup progresses at most one frame per Unity update. The worker owns all native reads; only copied data
        // reaches terrain/model creation here. No guessed legacy handle or worker/main-thread session sharing.
        void PollOpenStartup()
        {
            try
            {
                if (simulation.Phase == OpenReflexSimulation.Phase.Failed)
                    throw new InvalidOperationException(simulation.Failure ?? "Core startup failed.");
                // Rules and other interactive pages may legitimately wait for the player.
                if (!OwnedCareerActive && Time.realtimeSinceStartupAsDouble - openStarted > 180)
                    throw new TimeoutException("Core startup did not produce a complete scene within 180 seconds.");
                var data = simulation.Idle ? simulation.Open.TakeStartupData() : null;
                if (data != null)
                {
                    if (OwnedCareerActive)
                    {
                        openLevel = simulation.Open.Career.Snapshot?.Status.Location;
                        if (string.IsNullOrEmpty(openLevel)) throw new InvalidOperationException("Career startup has no authored location.");
                        // OriginalLevel may have been prepared by an exhibition scene's Awake.
                        // Match the authored finite career section before building terrain/render readiness.
                        string activity = CareerSceneActivity;
                        var content = FindAnyObjectByType<OriginalLevel>();
                        if (!content) throw new InvalidOperationException("Career needs an installed level renderer.");
                        if (!content.Loaded || content.level != openLevel || content.activity != activity)
                        { content.level = openLevel; content.activity = activity; content.runtime = this; content.terrain = terrain; content.LoadNow(); }
                        if (!content.Loaded) throw new InvalidOperationException("The authored career scene did not load.");
                        level = openLevel;
                        RequireCareerHud(ownedCareerLaunch.HudRoute);
                        if (gameAudio) gameAudio.level = openLevel;
                        var music = FindAnyObjectByType<OriginalMusic>();
                        if (music) { music.level = openLevel; music.activity = CareerMusicActivity; }
                    }
                    Roster = ReflexRoster.ReadOpen(data.slots, data.appearances);
                    int humanCount = 0, localCount = 0;
                    foreach (var entry in Roster.entries)
                    {
                        resultRosterSlots.Add(entry.slot.slot, entry.Local);
                        resultRosterNames.Add(entry.slot.slot, entry.name);
                        if (!entry.Computer) humanCount++;
                        if (entry.Local) localCount++;
                    }
                    openExhibitionLaunch &= humanCount == 1 && localCount == 1;
                    bool arenaMarks = loadArena != null;
                    if (arenaMarks)
                    {
                        var o = simulation.Open;
                        Reflex.Port.OpenFrontEnd.OpenLoadArena.Mark($"the arena's core startup is taken (its worker: session create {o.StartupCreateMs:F0} ms, test drive start {o.StartupLoadMs:F0} ms, first frames {o.StartupFramesMs:F0} ms)");
                    }
                    var buildClock = System.Diagnostics.Stopwatch.StartNew();
                    EnsureVehicle(Roster.Local);
                    if (!chassis || !frontWheel || !rearWheel || !rider)
                        throw new InvalidOperationException("Could not build the core session's bike and rider from the install.");
                    Reflex.Port.Content.ReflexPlates.Apply(bikeModel, riderModel, Roster.Local.plate);
                    double vehicleMs = buildClock.Elapsed.TotalMilliseconds;
                    if (terrain)
                    {
                        terrain.Build(data.terrainInfo, data.heights, data.deformation);
                        if (terrain.TryGetComponent<OriginalTracks>(out var tracks))
                            tracks.Init(terrain.UnityTerrain.materialTemplate, terrain.Info, null);
                    }
                    double terrainMs = buildClock.Elapsed.TotalMilliseconds - vehicleMs;
                    if (data.cameraBindings.valid != 0)
                        OriginalInput.ConfigureCameraBindings(data.cameraBindings.keyboard_scan, data.cameraBindings.pad_input, data.cameraBindings.right_stick_rides != 0, data.cameraBindings.look_back_pad_input);
                    else OriginalInput.ConfigureCameraBindings(0, 0, true);
                    pendingOpen = false;
                    SetDrivingCamera(Reflex.Port.Options.ReflexSettings.DrivingCamera);
                    CompleteSceneStartup(Roster, openLevel, drawAmbient);
                    simulation.ResetClock();
                    statusText = "OPEN REFLEX";
                    double sceneMs = buildClock.Elapsed.TotalMilliseconds - vehicleMs - terrainMs;
                    Debug.Log($"OPEN REFLEX startup ready: {Roster.entries.Count} riders, terrain {data.terrainInfo.samples}, legacy loaded={Reflex.Port.Platform.ReflexNativeLoader.IsLoaded}; built in {buildClock.Elapsed.TotalMilliseconds:F0} ms (bike and rider {vehicleMs:F0} ms, terrain {terrainMs:F0} ms, the scene's parts {sceneMs:F0} ms)");
                    if (arenaMarks) Reflex.Port.OpenFrontEnd.OpenLoadArena.Mark($"the arena's Unity world is built (bike and rider {vehicleMs:F0} ms, terrain {terrainMs:F0} ms, the scene's parts {sceneMs:F0} ms)");
                    return;
                }
                var done = simulation.Collect();
                if (done != null)
                {
                    string error = done.error; simulation.Recycle(done);
                    if (error != null) throw new InvalidOperationException(error);
                }
                if (simulation.Phase == OpenReflexSimulation.Phase.Ready && simulation.Idle)
                    simulation.Submit(OriginalPresentation.Step, keys, pads, 0);
            }
            catch (Exception e) { Release(); Fail(e.Message); }
        }

        // The editor copies a rebuilt librxcore to a stamped temp folder (never overwrite the image the editor has mapped);
        // the runtime uses the same rule as the standalone checks: <project>/Temp/OpenReflex.
        static string OpenReflexTempRoot()
        {
            string project = Path.GetDirectoryName(Application.dataPath);
            return Path.Combine(string.IsNullOrEmpty(project) ? Application.dataPath : project, "Temp", "OpenReflex");
        }

        // ---------------------------------------------------------------------------------------- the native game (native/game)
        // The conversion to native code (as Open Unleashed): the game's functions rewritten in C++ replace the original ones in the
        // running world. Native: the ports run instead of the original. Verify: both run on every call, the original's result is
        // kept and differences are counted (the report goes to the log when the world is released).
        public enum NativeGameMode { Off, Native, Verify }
        [Tooltip("The native game's ports (C++ rewrites of the original functions): Off = the original code everywhere, Native = the ports replace it, Verify = both run and are compared (the original is kept). PlayerPrefs ReflexNativeGame (0/1/2) and REFLEX_NATIVE_GAME override it.")]
        public NativeGameMode nativeGame = NativeGameMode.Native;
        [Tooltip("Which ports: \"all\", or groups / names / addresses, comma separated (\"-x\" excludes).")]
        public string nativePorts = "all";
        public NativeGameMode NativeGame
        {
            get
            {
                string e = Environment.GetEnvironmentVariable("REFLEX_NATIVE_GAME");
                if (int.TryParse(e, out int v) || (Reflex.Port.Options.ReflexSave.Has("ReflexNativeGame") && (v = Reflex.Port.Options.ReflexSave.GetInt("ReflexNativeGame")) >= 0)) return (NativeGameMode)Mathf.Clamp(v, 0, 2);
                return nativeGame;
            }
        }
        int nativePortCount;
        void ApplyNativeGame()
        {
            nativePortCount = 0;
            var mode = NativeGame;
            if (mode == NativeGameMode.Off) return;
            try { nativePortCount = ReflexWorldNative.reflex_world_port(world, string.IsNullOrEmpty(nativePorts) ? "all" : nativePorts, (int)mode); }
            catch (EntryPointNotFoundException) { Debug.LogWarning("Native game: the plugin has no ports (restart Unity after a plugin update)."); return; }
            Debug.Log($"Native game: {nativePortCount} ports installed ({mode}, \"{nativePorts}\")");
        }
        void LogNativeGame()
        {
            if (nativePortCount <= 0) return;
            string r = ReflexWorldNative.PortReport();
            if (!string.IsNullOrEmpty(r)) Debug.Log("Native game report:\n" + r);
        }

        // ---------------------------------------------------------------------------------------- frame pacing (OriginalPresentation)
        // How real time and input reach the game, and which of its frames is shown.
        //  Original (default): as the PC game's own loop. Engine::UpdateTiming 0084ea80 measures the frame with QueryPerformanceCounter
        //    and busy-waits until at least engine+0x14 (1/60 s) has passed (its frame cap), then averages the last 5 frame times
        //    (engine+0x1c on, ring +0x24..+0x34) and clamps at engine+0x48 (0.1 s); Engine::Update then reads the input, runs the
        //    world and the renderer draws that same frame. Here: a game frame starts once 1/60 s has passed since the last one (the
        //    cap; frames of a faster display wait), its dt is the real time since that frame started (the game's own smoothing and
        //    clamp run on it unchanged), the input is the one read this Unity frame, and the frame is waited for and shown in this
        //    same Unity frame: no frame of lag, no blending, no merged frames. A frame the simulation could not finish in time is
        //    shown when it completes, and the next one covers the real time since (as the original on a slower machine).
        //  Interpolated: whole 1/60 s frames on the simulation thread, the display interpolates between the last two completed
        //    frames (smooth at any display rate, but at least one game frame later plus the pacing margin).
        //  Legacy: one frame of the real elapsed time per Unity frame, pipelined (one frame late), the newest result shown.
        public enum PacingMode { Original, Interpolated, Legacy }
        [Tooltip("Original: the PC game's own frame loop (60 Hz cap, the real frame time, input to display in the same frame). Interpolated: smooth display at any refresh rate, at least one game frame later. Legacy: the old pipelined pacing. REFLEX_PACING=original|interpolated|legacy overrides it.")]
        public PacingMode pacing = PacingMode.Original;
        /// The pacing in effect (REFLEX_PACING overrides the inspector).
        public PacingMode Pacing
        {
            get
            {
                string e = Environment.GetEnvironmentVariable("REFLEX_PACING");
                if (e == "legacy") return PacingMode.Legacy;
                if (e == "interpolated" || e == "interp") return PacingMode.Interpolated;
                if (e == "original") return PacingMode.Original;
                return pacing == PacingMode.Original && Reflex.Port.Options.ReflexSettings.FramePacing == 1 ? PacingMode.Interpolated : pacing;   // OPTIONS > VIDEO > FRAME PACING
            }
        }
        [Tooltip("Original pacing: the longest the main thread waits for a game frame (ms) before showing the previous one and going on.")]
        public float frameWaitMs = 100f;
        OriginalPresentation presentation;
        double simAccumulator;                 // real time not yet given to the game (< one game frame)
        bool legacyPacing;
        PacingMode activePacing;
        /// Original pacing: the last frame submitted this Unity frame and was waited for (false: not due yet, or still running).
        public bool FrameThisUpdate { get; private set; }
        double lastParticleFrame = double.NaN; float lastAlpha = -1; double lastA = double.NaN, lastB = double.NaN;
        /// The presentation (time shown, delay, blend) of the running world; null when none.
        public OriginalPresentation Presentation => presentation;

        void StartPresentation()
        {
            activePacing = Pacing;
            if (activePacing == PacingMode.Original) { InstallPreInputWait(); preInputOwner = this; }
            legacyPacing = activePacing == PacingMode.Legacy;
            presentation = new OriginalPresentation(simulation) { Interpolate = activePacing == PacingMode.Interpolated };
            simAccumulator = 0; lastParticleFrame = lastA = lastB = double.NaN; lastAlpha = -1;
            string log = Environment.GetEnvironmentVariable("REFLEX_PACING_LOG");
            if (!string.IsNullOrEmpty(log)) presentation.OpenLog(log);
        }

        // Gives the game the real time passed: whole 1/60 s frames (the accumulator keeps the rest), or the old way one frame of the
        // real elapsed time (legacy). Returns the simulation clock "now" for the presentation.
        double SubmitGameTime(uint mask)
        {
            if (activePacing == PacingMode.Original) return SubmitOriginalFrame(mask);
            float real = Mathf.Min(Time.unscaledDeltaTime, 0.25f);
            if (legacyPacing)
            {
                pendingDt += real;
                if (simulation.Submit(Mathf.Min(pendingDt, 0.25f), keys, pads, mask)) pendingDt = 0;
                return simulation.Requested;
            }
            simAccumulator += real;
            int due = (int)(simAccumulator / OriginalPresentation.Step);
            if (due > 0) { simAccumulator -= due * (double)OriginalPresentation.Step; simulation.SubmitFrames(due, OriginalPresentation.Step, keys, pads, mask); }
            return simulation.Requested + simAccumulator;
        }

        // Original pacing without VSync (w-latency): the original's loop is wait for the cap -> read the input -> run the world -> draw ->
        // present (Engine::UpdateTiming 0084ea80 busy-waits BEFORE Engine::Update polls the devices). Here a due game frame used to take
        // this Unity frame's input first and then wait for its cap on the simulation thread (up to 0.4 of a game frame), so the input was
        // that much older than the frame it fed. With VSync off the Unity frame itself can wait instead, before the Input System's update
        // (a PlayerLoop step right before PreUpdate.NewInputUpdate): the input is read when the cap ends, as the exe reads it. Uncapped
        // (no FRAME RATE CAP, VSync off: the original's own default, graphics.cfg VSync=0) every Unity frame waits for the next game frame:
        // one frame drawn per game frame, presented at once, the original's loop step for step. With a cap above 60 only the frames that
        // start a game frame wait. With VSync the display paces the frames (the cap ends within the vblank wait), so nothing changes.
        // The game's dt is still the real time between frame starts, measured by the game frame itself: the simulation is untouched.
        // REFLEX_PREINPUT_WAIT=0 turns it off (A / B checks).
        static readonly bool preInputWaitOff = Environment.GetEnvironmentVariable("REFLEX_PREINPUT_WAIT") == "0";
        static OriginalWorldRuntime preInputOwner;
        static bool preInputInstalled;
        /// Original pacing: the last pre-input wait (ms) and how many Unity frames waited (tests / the probe).
        public static double PreInputWaitMs { get; private set; }
        public static int PreInputWaits { get; private set; }
        struct PreInputWaitStep { }
        static void InstallPreInputWait()
        {
            if (preInputInstalled) return;
            preInputInstalled = true;
            try
            {
                var loop = UnityEngine.LowLevel.PlayerLoop.GetCurrentPlayerLoop();
                var systems = loop.subSystemList;
                for (int i = 0; systems != null && i < systems.Length; i++)
                {
                    if (systems[i].type != typeof(UnityEngine.PlayerLoop.PreUpdate)) continue;
                    var subs = new System.Collections.Generic.List<UnityEngine.LowLevel.PlayerLoopSystem>(systems[i].subSystemList ?? new UnityEngine.LowLevel.PlayerLoopSystem[0]);
                    int at = subs.FindIndex(x => x.type != null && x.type.Name == "NewInputUpdate");
                    subs.Insert(at < 0 ? 0 : at, new UnityEngine.LowLevel.PlayerLoopSystem { type = typeof(PreInputWaitStep), updateDelegate = PreInputWait });
                    systems[i].subSystemList = subs.ToArray();
                    loop.subSystemList = systems;
                    UnityEngine.LowLevel.PlayerLoop.SetPlayerLoop(loop);
                    return;
                }
            }
            catch (Exception e) { Debug.LogWarning("Original pacing: no pre-input wait (" + e.Message + ")"); }
        }
        static void PreInputWait()
        {
            var rt = preInputOwner;
            if (preInputWaitOff || rt == null || !rt || rt.activePacing != PacingMode.Original || !rt.Running || rt.paused || rt.simulation == null) return;
            if (QualitySettings.vSyncCount != 0 || !rt.simulation.RealClock || !rt.simulation.Idle) return;
            double last = rt.simulation.LastFrameStart;
            if (double.IsNaN(last)) return;
            double cap = OriginalSimulation.FrameCap, now = OriginalSimulation.Clock, remaining = last + cap - now;
            // uncapped: wait for the next game frame; a cap above 60: only a frame that will start one (SubmitOriginalFrame's due rule)
            bool uncapped = Application.targetFrameRate <= 0;
            if (!(remaining > 0) || !uncapped && remaining > cap * 0.4) return;
            // it ends `preInputLead` before the cap: the time this frame still needs from here to the game frame's start (the input update,
            // the Updates before the runtime's, the worker's wake-up), learnt from the frames themselves (NotePreInputStart), so the game
            // frame starts at the cap (the worker spins the last part) and the game keeps its 60 Hz instead of 60 Hz plus that delay
            Reflex.Port.RxCore.PreciseWait.Until(last + cap - preInputLead, cap);
            preInputEnd = OriginalSimulation.Clock;
            PreInputWaitMs = (preInputEnd - now) * 1000.0; PreInputWaits++;
        }
        static double preInputLead = 0.0015, preInputEnd = double.NaN;
        /// Original pacing: what a pre-input wait leaves to the cap (s; the learnt delay from its end to the game frame's start + 0.3 ms).
        public static double PreInputLeadMs => preInputLead * 1000.0;
        // the first collected game frame that started after the last pre-input wait: its start - the wait's end - its own cap wait
        static void NotePreInputStart(OriginalSimulation.Results done)
        {
            if (double.IsNaN(preInputEnd) || !(done.frameStartClock > preInputEnd)) return;
            double delay = done.frameStartClock - preInputEnd - Math.Max(0, done.capWaitMilliseconds) / 1000.0;
            preInputEnd = double.NaN;
            if (delay < 0 || delay > 0.01) return;
            // up fast, down slowly: a late start costs the game a frame's worth of cadence, an early one only a short spin
            double target = Math.Min(Math.Max(delay + 0.0003, 0.0003), 0.004);
            preInputLead += (target - preInputLead) * (target > preInputLead ? 0.3 : 0.02);
        }

        // Original pacing: one game frame when the game's frame cap allows it, with this Unity frame's input. When this Unity frame can
        // afford it (the main thread's own work plus the game frame fit in the cap) the main thread waits for the frame, so Collect below
        // gets the frame this Unity frame shows (the original: input, world, render in one frame); otherwise it goes on and shows the
        // frame finished meanwhile (one frame later, the least a machine that cannot do both in 1/60 s can do).
        double SubmitOriginalFrame(uint mask)
        {
            const double Wait = OriginalSimulation.FrameCap * 0.6;  // due when the cap ends within 0.4 of a frame (the game spins the rest);
                                                                    // not a faster display's in-between frame (120 / 144 Hz: 8.3 / 9.7 ms early)
            FrameThisUpdate = false;
            MeasureBusy();
            waitedMs = 0; submitted = false;
            updateStart = Time.realtimeSinceStartupAsDouble;
            simulation.RealClock = true;
            double last = simulation.LastFrameStart;
            if (!double.IsNaN(last) && OriginalSimulation.Clock - last < Wait) return simulation.Requested;   // the cap holds the next frame
            bool idle = simulation.Idle;
            simulation.Submit(0f, keys, pads, mask);   // starts now, or right after the running frame with this input
            if (!idle) { skippedBusy++; return simulation.Requested; }
            submitted = true;
            if (SyncFrames) { double w0 = Time.realtimeSinceStartupAsDouble; FrameThisUpdate = simulation.WaitIdle(frameWaitMs); waitedMs += (Time.realtimeSinceStartupAsDouble - w0) * 1000.0; }
            return simulation.Requested;
        }

        // Original pacing: waiting for the frame or not. Per Unity frame that started a game frame: the main thread's busy time (this
        // script's Update to the end of rendering, without the wait) plus that game frame's own cost (the job without its frame-cap wait).
        // Waiting is worth it only while both (and the game frame's wait for its cap) fit in one PRESENTED frame (VSync: the refresh interval, a cap: 1 / cap, else the game's
        // 1/60): a frame that waits and does not fit misses its vblank, and at 60 Hz VSync every miss shows as 33 ms (the 30-40 fps of
        // playtesters on fast GPUs). So: one frame later as soon as 3 of the last 60 such frames did not fit in 0.9 of it (or their
        // smoothed sum is over 0.85); back to same-frame only after 2 s with the smoothed sum under 0.65 and no frame over 0.8 of it
        // in the last 120. REFLEX_PACING_SYNC=1 / 0 forces it.
        double updateStart = double.NaN, waitedMs, busyMs = 8, simMs = 4, capMs; int skippedBusy; bool submitted;
        bool syncFrames = true;
        ulong overrunBits, nearBits0, nearBits1; double lastPacingSwitch = double.NegativeInfinity;
        /// Original pacing: the budget of one presented frame in ms (tests / the performance overlay).
        public double PacingBudgetMs { get; private set; } = 1000.0 / 60.0;
        /// Original pacing: the smoothed main-thread busy time and game-frame cost of the frames that started one (ms).
        public double PacingMainMs => busyMs;
        public double PacingSimMs => simMs;
        /// Original pacing: the smoothed wait of those game frames for the 1/60 s cap (ms; part of the same-frame budget).
        public double PacingCapMs => capMs;
        static double PresentedFrameMs()
        {
            double cap = OriginalPresentation.Step * 1000.0, hz = Platform.ReflexDisplayInfo.RefreshHz;   // (cached: asked every game frame)
            if (QualitySettings.vSyncCount > 0 && hz >= 30 && hz <= 1000) return Math.Min(cap, 1000.0 * QualitySettings.vSyncCount / hz);
            if (Application.targetFrameRate > 0) return Math.Min(cap, 1000.0 / Application.targetFrameRate);
            return cap;
        }
        /// Original pacing: whether game frames are waited for (input to display in the same Unity frame).
        public bool SyncFrames
        {
            get
            {
                string e = Environment.GetEnvironmentVariable("REFLEX_PACING_SYNC");
                return e == "1" || e != "0" && syncFrames;
            }
        }
        /// Original pacing: Unity frames that found the last game frame still running.
        public int SkippedBusy => skippedBusy;
        // The end of the Unity frame's main-thread work: rendering when it renders (URP culls and records on the main thread), else LateUpdate.
        double busyEnd = double.NaN;
        void OnEndContextRendering(UnityEngine.Rendering.ScriptableRenderContext ctx, System.Collections.Generic.List<Camera> cameras)
        {
            if (!double.IsNaN(updateStart)) busyEnd = Time.realtimeSinceStartupAsDouble;
            try { if (Running && careerPoseApplied) replayRenderer?.WorldRendered(cameras); } catch (Exception e) { Fail(e.Message); }
            if (careerPresenter != null && Running && careerPoseApplied && !CareerFinalized && chaseCamera && cameras.Contains(chaseCamera))
            {
                try { careerPresenter.WorldRendered(careerWorldIdentity, careerGeneration); }
                catch (Exception e) { Fail(e.Message); }
            }
        }
        void MeasureBusy()
        {
            double start = updateStart, end = busyEnd; updateStart = busyEnd = double.NaN;
            if (activePacing != PacingMode.Original || !submitted || double.IsNaN(start) || double.IsNaN(end) || simulation == null) return;   // Unity frames that started a game frame
            double busy = (end - start) * 1000.0 - waitedMs;
            if (busy < 0 || busy > 250) return;
            var n = presentation?.Newest;
            double sim = n != null && n.milliseconds > 0 ? n.milliseconds : simMs;
            // the game frame's wait for its 1/60 s cap (RealClock) holds a same-frame Unity frame too: on a 120 / 144 Hz display (or with
            // VSync off) a due frame is submitted up to 0.4 of a game frame before the cap ends (SubmitOriginalFrame), and waiting for it
            // made the frame miss its vblank while the budget looked fine (w-latency); at 60 Hz it is about 0 and nothing changes
            double cap = n != null && n.capWaitMilliseconds > 0 ? n.capWaitMilliseconds : 0;
            busyMs += (busy - busyMs) * 0.05;
            simMs += (sim - simMs) * 0.05;
            capMs += (cap - capMs) * 0.05;
            double budget = PacingBudgetMs = PresentedFrameMs(), need = busy + sim + cap, smooth = busyMs + simMs + capMs;
            overrunBits = (overrunBits << 1) | (need > 0.9 * budget ? 1ul : 0ul);
            nearBits1 = (nearBits1 << 1) | (nearBits0 >> 63); nearBits0 = (nearBits0 << 1) | (need > 0.8 * budget ? 1ul : 0ul);
            int overruns = PopCount(overrunBits & 0x0FFFFFFFFFFFFFFFul);   // the last 60
            double now = Time.realtimeSinceStartupAsDouble;
            if (syncFrames && (overruns >= 3 || smooth > 0.85 * budget))
            {
                syncFrames = false; lastPacingSwitch = now;
                Debug.Log(FormattableString.Invariant($"Original pacing: one frame later (main {busyMs:0.0} ms + game frame {simMs:0.0} ms + cap wait {capMs:0.0} ms, {overruns} of the last 60 over {0.9 * budget:0.0} ms)"));
            }
            else if (!syncFrames && now - lastPacingSwitch > 2.0 && smooth < 0.65 * budget && nearBits0 == 0 && (nearBits1 & 0x00FFFFFFFFFFFFFFul) == 0)
            {
                syncFrames = true; lastPacingSwitch = now;
                Debug.Log(FormattableString.Invariant($"Original pacing: same-frame (main {busyMs:0.0} ms + game frame {simMs:0.0} ms + cap wait {capMs:0.0} ms, budget {budget:0.0} ms)"));
            }
        }
        static int PopCount(ulong v) { int c = 0; while (v != 0) { v &= v - 1; c++; } return c; }
        void OnEnable() => UnityEngine.Rendering.RenderPipelineManager.endContextRendering += OnEndContextRendering;

        // The presentation's blended frame to the bike, rider, other riders, scenery, particles, ambient entities and cameras.
        /// The presented frame, after the world applied it (test probes read the drawn pose from here). Main thread.
        public static event Action<OriginalWorldRuntime, OriginalSimulation.Results> ViewApplied;
        void ApplyView(OriginalSimulation.Results v)
        {
            var p = presentation;
            if (p.Alpha == lastAlpha && p.A.simTime == lastA && p.B.simTime == lastB) return;   // the same moment as last frame (held)
            bool newB = p.B.simTime != lastB;
            lastAlpha = p.Alpha; lastA = p.A.simTime; lastB = p.B.simTime;
            if (hovercraft) hovercraft.BeforePoses();   // port extra: the rider carried onto the craft is put back to where the game poses it
            state = v.bike; onboardState = v.onboardState; vehicleInfo = v.vehicle; hasVehicleInfo = v.hasVehicle;
            ReadEngine(v.vehicleAudio);
            if (hasVehicleInfo) Reflex.Port.Content.ReflexObjectShading.ApplyVehicleConstants(vehicleInfo.vehicle, bikeModel);   // run-time shader constants (belt scroll, flywheel blur)
            if (v.hasCamera) { gameCamera = v.camera; haveGameCamera = true; }
            if (onboard) onboard.OnSimFrame(v.dt, v.raceLine, v.raceLinePoints, v.raceLineClosed);
            haveState = true; Odometer(); Apply();
            if (bikePose) { bikePose.Apply(v.bikePose, v.bikeNodes); bikePose.ApplyFlags(v.bikeNodeFlags, v.bikeNodeFlagCount); }   // a machine's damage panels (the game's node flags)
            if (animatedObjects) animatedObjects.Apply(v.objects, v.objectsValid);
            ApplyRider(v);
            var am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
            if (opponentRiders) { int mud = v.mudCount; if (!newB) v.mudCount = 0; opponentRiders.Apply(v); v.mudCount = mud; }   // mud once per game frame
            Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Opponents, am); am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
            ViewApplied?.Invoke(this, v);
            Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.View, am);   // the ViewApplied observers
            var near = p.Alpha < 0.5f ? p.A : p.B;
            if (particles && near.simTime != lastParticleFrame) { lastParticleFrame = near.simTime; particles.Apply(v.particles, v.particleCount, v.particleInfo); }
            if (ambient) ambient.Apply(v.ambient);
            ApplyPodium(v.podium);
            if (hovercraft) hovercraft.AfterPoses();
            sceneryModels.Apply(v.sceneryModels);   // the static models a scenario script hides / shows (the parked hovercraft, the dropped box)
            // The replay renderer presents only after the world's own poses were applied (career or exhibition replay).
            if ((OwnedCareerActive || exhibitionReplayIdentity != null) && v.hasVehicle && v.bikeNodes > 0) careerPoseApplied = true;
        }

        // The scenario cast (OriginalPodium): the intro's start-line riders and the podium riders, and the live rider / vehicle the
        // scenario hides (the rider's +0xa2 / the vehicle model's +0x14e: the original does not draw them while clear). Outside the
        // intro and Post (no valid frame) the live models are shown again.
        bool podiumHidLocal;
        /// The scenario the cast is playing (rx_session_podium: 1 the finish, 2 the results, 3 the intro; 0 none): the Event Intro page's window.
        public int ScenarioKind { get; private set; }
        void ApplyPodium(OriginalPodium.Frame f)
        {
            ScenarioKind = f != null && f.valid ? f.scenario : 0;
            if (podium) podium.Apply(f);
            bool gateHide = gateView != null && gateView.Active;   // the Gate Selection page: no riders yet (the original's activity is not created until the gate is chosen)
            if (opponentRiders) opponentRiders.ApplyDrawn(f, gateHide);
            bool riderOn = true, vehicleOn = true;
            if (gateHide) { riderOn = vehicleOn = false; }
            else
            if (f != null && f.valid && Roster?.Local != null) f.TryDrawn(Roster.Local.slot.slot, out riderOn, out vehicleOn);
            if (!calibrated) return;   // not shown yet (SetVehicleActive(true) at the first state)
            if (riderOn && vehicleOn && !podiumHidLocal) return;
            if (bikeModel != null) { bikeModel.chassis.SetActive(vehicleOn); bikeModel.frontWheel.gameObject.SetActive(vehicleOn); bikeModel.rearWheel.gameObject.SetActive(vehicleOn); }
            if (riderModel != null) riderModel.root.SetActive(riderOn);
            if (proxyModel != null) proxyModel.proxyRoot.gameObject.SetActive(riderOn && (rider == null || !rider.HasOriginalPose));
            podiumHidLocal = !(riderOn && vehicleOn);
        }

        public string FailureMessage => failure;
        /// The level's own load failed (OriginalLevel: a custom track's level that does not read): the event stops with the reason.
        public void FailLevel(string message) { if (failure == null) Fail(message); }

        void Fail(string message)
        {
            if (simulation?.Mode == ReflexSimMode.Open) Release();
            failure = message; statusText = "STOPPED: " + message + (ownedCareerLaunch == null ? "\nPress F5 to recreate the world." : "\nReturn to career selection.");
            Debug.LogWarning("Original world: " + message, this);
        }

        void Update()
        {
            try { TickOwnedCareer(); } catch (Exception e) { Fail(e.Message); }
            try { TickExhibitionReplay(); } catch (Exception e) { Fail(e.Message); }
            if (pendingOpen || pendingFiles != null) PresentedGameFrame = double.NaN;   // starting / building: no game frame on screen
            if (pendingOpen)
            {
                PollOpenStartup();
                RefreshHud();
                return;
            }
            if (pendingFiles != null)   // the level's world is being built from the install
            {
                if (pendingFiles.Ready) { pendingFiles = null; Create(); }
                else if (!pendingFiles.Building) { string f = pendingFiles.Failure ?? "world build stopped", log = pendingFiles.Log; pendingFiles = null; Fail(f + " (log: " + log + ")"); }
                else
                {
                    float p = Reflex.Port.Content.ReflexWorldFiles.Progress(out string st);
                    statusText = $"BUILDING {pendingFiles.level} FROM YOUR REFLEX INSTALL  {p * 100:0}%  ({st})";
                    if (statusLabel) statusLabel.text = statusText;
                    return;
                }
            }
            PollOwnResultsTransition();
            try { PollOpenRoster(); } catch (Exception e) { Fail(e.Message); }
            var kb = Keyboard.current;
            bool menu = OwnMenuActive || optionsMenu && optionsMenu.IsOpen || pauseMenu && pauseMenu.IsOpen || eventSelect && eventSelect.IsOpen;
            if (kb != null && menu) Array.Clear(keys, 0, keys.Length);   // the options panel has the keyboard
            if (kb != null && heldAfterMenu.Count > 0) heldAfterMenu.RemoveWhere(k => !kb[k].isPressed);
            if (kb != null && !menu)
            {
                if (kb.f5Key.wasPressedThisFrame) { Create(); }
                if (kb.f6Key.wasPressedThisFrame) paused = !paused;
                if (kb.f7Key.wasPressedThisFrame && !OriginalInput.IsCameraKey(Key.F7)) Reflex.Port.Options.ReflexClientLive.CycleCamera();
                if (onboard) onboard.HandleKeys();
                Array.Clear(keys, 0, keys.Length);
                foreach (var kv in DirectInputMap.Table)
                {
                    if (kv.Key == Key.F5 || kv.Key == Key.F6 || kv.Key == Key.F7 || kv.Key == Key.F8 || kv.Key == Key.F9 || kv.Key == Key.F10 ||
                        kv.Key == Key.F11 || kv.Key == Key.F12 || kv.Key == Key.F4 || kv.Key == Key.Insert || kv.Key == Key.Delete || kv.Key == Key.M || kv.Key == Key.LeftAlt || kv.Key == Key.Escape && !OriginalPauseMenu.GameOwnsPause(this)) continue;   // client keys (Esc: the port's pause menu, or the game's own pause page when its UI runs live)
                    if (kv.Key == Key.H && !OriginalInput.GameBinds(Key.H)) continue;   // H hides the HUD (Options: ReflexModernHud), unless the game binds it
                    if (heldAfterMenu.Contains(kv.Key)) continue;                         // still held from a menu that just closed
                    if (!OriginalOnboardAdjust.KeyboardCaptured && !OriginalInput.IsCameraKey(kv.Key) && kb[kv.Key].isPressed) keys[kv.Value] = 0x80;
                }
            }
#if UNITY_EDITOR
            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;
#endif
            if (Reflex.Port.Content.ExperimentalMtxLaunch.HoldThrottle) keys[0x11] = 0x80;
            uint mask = ReadPads(!menu && !paused);
            if (menu || paused) { Array.Clear(keys, 0, keys.Length); Array.Clear(pads, 0, pads.Length); mask = 0; }
            if (!(menu || paused)) OriginalSpawnMarker.FilterGameInput(this, keys, ref pads[0]);   // SPAWN MARKERS (w-markerbtns): its inputs (default Y / B, F / V) are not the game's where it works
            if (hovercraft) hovercraft.Feed(keys, pads, !(menu || paused));   // port extra: the craft takes the riding input while it is driven
            ControllerInputDiagnostics.Record(this, pads, mask, menu);
            bool advanceCareerPost = (CareerNeedsPostFrames || ExhibitionNeedsPostFrames) && !ViewerActive;   // (the replay viewer over the results: the post frames wait)
            if (Running && (!paused || advanceCareerPost) && !CareerFinalized && simulation != null)
            {
                try
                {
                    // Results of the frame that finished on the simulation thread.
                    // Results of the frame that finished on the simulation thread: what happens once per game frame is applied now (sound,
                    // terrain, tracks, mud, race / HUD state); the poses go to the presentation, which shows them blended (ApplyView).
                    double now = activePacing == PacingMode.Original && failure == null ? SubmitGameTime(mask) : double.NaN;
                    CollectCompletedFrame();
                    BeginOwnResultsTransition();
                    // Real time drives the game's own clock; then the frame to show for this moment.
                    if (failure == null)
                    {
                        if (activePacing != PacingMode.Original) now = paused && !advanceCareerPost ? simulation.Requested : SubmitGameTime(mask);
                        presentation.Interpolate = activePacing == PacingMode.Interpolated && !(replayRenderer?.RequiresExactPose ?? false);
                        if (presentation.Present(now, Time.unscaledDeltaTime))
                        {
                            PresentedGameFrame = GameFrameOf(presentation); PresentedGameTime = presentation.view.gameTime;
                            ApplyView(presentation.view);
                            if (careerPoseApplied) replayRenderer?.Present(presentation.view.replay, presentation.view.nativeFrame, presentation.Alpha >= 1f);
                        }
                        presentation.LogFrame(Time.realtimeSinceStartupAsDouble, Time.unscaledDeltaTime);
                    }
                }
                catch (Exception e) when (e is DllNotFoundException || e is EntryPointNotFoundException || e is BadImageFormatException)
                { Fail("Native plugin unavailable: " + e.Message); }
                catch (Exception e) { Fail(e.Message); }
            }
            else if (activePacing == PacingMode.Original && simulation != null && simulation.Idle) simulation.ResetClock();   // paused: the game's clock does not see the pause
            FramesHeld = !Running || paused && !advanceCareerPost || CareerFinalized || simulation == null || failure != null;
            if (FramesHeld) PresentedGameFrame = double.NaN;
            if (originalHud) originalHud.visible = OriginalHudVisible && !OwnMenuActive && !IntroCardShown && !(pauseMenu && pauseMenu.IsOpen) && !(hovercraft && hovercraft.Driving);   // the pause page replaces the HUD
            var am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
            if (originalHud && failure == null) originalHud.Tick(paused ? 0f : Time.unscaledDeltaTime, race, hudState, haveHud, Reflex.Port.Options.ReflexCameraPresets.ViewKey(cameraMode), CameraName(cameraMode));
            Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Hud, am); am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
            if (gameAudio && failure == null) gameAudio.Tick(paused && !advanceCareerPost && !ViewerAudio ? 0f : ViewerAudio ? Time.unscaledDeltaTime : Time.deltaTime, chaseCamera ? chaseCamera.transform.position : Vector3.zero);
            if (failure == null) VehicleSound?.Tick(Time.deltaTime);   // w-audio: the engine packs' voices
            Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Sound, am);
            RefreshHud();
        }

        /// The original frame on screen: the game's frame number (rx_session_frames of the presented results), fractional while the
        /// presentation blends two frames; NaN while no game frame advances (no world, paused, a path without frame numbers). The HDR
        /// eye adaptation steps once per original frame by it (ReflexRenderPipeline.originalFrame), as the original renders once per frame.
        public double PresentedGameFrame { get; private set; } = double.NaN;
        /// No game frame is submitted this frame (paused, the replay viewer / photo mode, the records finalized): the simulation worker
        /// stays idle, so what it does "between frames" waits (the RT_Bump fit then asks it directly: Rendering/ReflexRTBump.cs).
        public bool FramesHeld { get; private set; } = true;
        /// The game's clock (Engine +0x3c, rx_session_game_time) of the frame on screen; valid while PresentedGameFrame is.
        public float PresentedGameTime { get; private set; }
        static double GameFrameOf(OriginalPresentation p)
        {
            if (p.A == null || p.B == null || p.B.nativeFrame == 0) return double.NaN;
            double a = p.A.nativeFrame, b = p.B.nativeFrame;
            return b > a ? a + p.Alpha * (b - a) : b;
        }

        void CollectCompletedFrame()
        {
            var done = simulation?.Collect();
            if (done == null) return;
            EventUi = done.eventUi; Waypoint = done.waypoint; HasWaypoint = done.hasWaypoint; Powercross = done.powercross; HasPowercross = done.hasPowercross; ResultsPage = done.resultsPage;
            Activation = done.activation; HasActivation = done.hasActivation; Challenge = done.challenge; HasChallenge = done.hasChallenge; Compass = done.compass; HasCompass = done.hasCompass;
            RaceFormat = done.raceFormat; HasRaceFormat = done.hasRaceFormat;
            var raceHud = done.raceHud; raceHud.activityType = Reflex.Port.RxCore.RxChampWeekend.BaseType(raceHud.activityType);   // (RESTORED CONTENT: a championship stage's HUD is its discipline's)
            RaceHud = raceHud; HasRaceHud = done.hasRaceHud;
            ModeResults = done.modeResults; HasModeResults = done.hasModeResults;
            StuntHud = done.stuntHud; HasStuntHud = done.hasStuntHud;
            ReflexHud = done.reflexHud; HasReflexHud = done.hasReflexHud;
            done.collectionClock = OriginalSimulation.Clock;
            NotePreInputStart(done);
            simMilliseconds = done.milliseconds;
            if (done.error != null) Fail(done.error);
            else
            {
                soundFirstPersonKnown = done.sound.firstPersonKnown; soundFirstPerson = done.sound.firstPersonView;
                race = done.race; DriverCount = Math.Min(done.driverCount, Drivers.Length); Array.Copy(done.drivers, Drivers, DriverCount); hudState = done.hud; haveHud = done.hasHud;
                // PRACTICE (w-lb-practice): the completed frame's laps to the lap dispatcher (the lap list), once per completed result, before
                // the view is applied; it never fails the world
                var am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
                try { Reflex.Port.Practice.RxPractice.OnFrame(this, done); } catch (Exception e) { Debug.LogWarning("[Practice] " + e); }
                Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Practice, am); am = Reflex.Port.Platform.ReflexAllocMeter.Begin();
                if (CheckFinite(done.bike)) { if (gameAudio) foreach (var a in done.audio) gameAudio.Handle(a); if (gameAudio && done.vehicleAudio.valid) { var vs = VehicleSound ??= new OriginalVehicleSound(gameAudio, this); vs.Poses(done); vs.Frame(done.vehicleAudio, done.sound, done.dt > 0f ? done.dt : done.gameDt); } Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Sound, am); if (done.mudCount > 0) Reflex.Port.Content.ReflexMud.Apply(bikeModel, riderModel, done.mud[0]); if (terrain) { foreach (var p in done.terrain) terrain.ApplyPatch(p); if (done.tracksChanged && terrain.TryGetComponent<OriginalTracks>(out var tr)) tr.Apply(done.tracks, done.trackTriangles); } presentation.Push(done); }
                else Fail("Native state contained a nonfinite value.");
            }
        }

        void BeginOwnResultsTransition()
        {
            if (resultsWaiting || OwnRaceResultsSnapshot != null || !Reflex.Port.OpenFrontEnd.OwnRaceResultsSnapshot.Supports(OfflineExhibitionOpen, race, EventUi)) return;
            if (racePause) racePause.CloseForWorldChange();
            GetComponent<OriginalOnboardAdjust>()?.End();
            resultsPausedBefore = paused;
            resultsWaiting = true; resultsWaitStarted = Time.unscaledTime;
            paused = true; ClearMenuInput();
        }

        void PollOwnResultsTransition()
        {
            if (!resultsWaiting) return;
            if (!Running || !OwnResultsPresentation)
            {
                resultsWaiting = false; paused = resultsPausedBefore; ClearMenuInput(); return;
            }
            if (!TrySettleMenuPause())
            {
                if (Time.unscaledTime - resultsWaitStarted > 2f) ReportOwnResultsFailure("The race did not settle for final standings.");
                return;
            }
            // Collection may have changed the route/lifecycle. Unwind our temporary pause,
            // leaving an excluded route to its existing owner or an outro to keep advancing.
            if (!OwnResultsPresentation || !Reflex.Port.OpenFrontEnd.OwnRaceResultsSnapshot.Supports(OfflineExhibitionOpen, race, EventUi))
            {
                resultsWaiting = false; paused = resultsPausedBefore; ClearMenuInput(); return;
            }
            // The last queued frame is authoritative. Validate identity against the startup roster,
            // independently of the optional display names, before copying any result rows.
            bool lapRace = OwnModes.LapType(EventUi.activity_type);
            if (!(lapRace ? OwnRaceResultsPolicy.MatchesRoster(resultRosterSlots, Drivers, DriverCount, race.riders)
                          : OwnRaceResultsPolicy.MatchesLocalRoster(resultRosterSlots, Drivers, DriverCount, race.riders)))
            { ReportOwnResultsFailure("Final standings do not match this race's riders."); return; }
            if (!Reflex.Port.OpenFrontEnd.OwnRaceResultsSnapshot.TryCreateMode(WorldGeneration, EventIdentity, OfflineExhibitionOpen,
                race, EventUi, Drivers, DriverCount, resultRosterNames, OwnModes.ResultsContext(this), out var snapshot, out var reason))
            { ReportOwnResultsFailure(reason); return; }
            OwnRaceResultsSnapshot = snapshot;
            paused = resultsPausedBefore; resultsWaiting = false;
            try
            {
                raceResults = raceResults ? raceResults : GetComponent<OwnRaceResults>() ?? gameObject.AddComponent<OwnRaceResults>();
                if (!raceResults.Begin(snapshot) && Running)
                    ReportOwnResultsFailure("The results screen declined this race snapshot.");
            }
            catch (Exception e) { ReportOwnResultsFailure(e.Message); }
        }

        public void ReportOwnResultsFailure(string reason) => Fail("Could not display final standings: " + reason);

        // No worker wait on the render thread: the menu polls until all submitted work drains.
        public bool TrySettleMenuPause()
        {
            if (!paused || !Running || simulation == null || !simulation.Idle) return false;
            try
            {
                CollectCompletedFrame();
                if (!Running) return false;
                simulation.ResetClock();
                if (presentation != null && presentation.Present(simulation.Requested, 0f))
                {
                    ApplyView(presentation.view);
                    if (careerPoseApplied) replayRenderer?.Present(presentation.view.replay, presentation.view.nativeFrame, presentation.Alpha >= 1f);
                }
                return true;
            }
            catch (Exception e) { Fail("Could not pause the event: " + e.Message); return false; }
        }

        public void ClearMenuInput()
        {
            Array.Clear(keys, 0, keys.Length);
            Array.Clear(pads, 0, pads.Length);
            HoldKeysUntilReleased();
            pendingDt = 0; simAccumulator = 0;
            if (simulation != null && simulation.Idle) simulation.ResetClock();
        }

        uint ReadPads(bool captureCamera = false)
        {
            uint mask = 0; PadRouting.Refresh();
            for (int i = 0; i < 4; i++)
            {
                pads[i] = default;
                var g = PadRouting.Pad(i);
                if (heldPadDevices[i] != g) { heldPadDevices[i] = g; padHeld[i] = 0xffff; }
                if (g == null) continue;
                mask |= 1u << i;
                ushort b = OriginalPadConvert.Buttons(g);
                padHeld[i] &= b; b = (ushort)(b & ~padHeld[i]);   // buttons still held from a menu that just closed
                // unprocessed: XInput's raw ranges (the Input System's stick deadzone would rescale them; the game applies its own, 7864)
                pads[i] = OriginalPadConvert.Convert(g, b);
                if (captureCamera && i == 0) OriginalInput.FilterCameraPad(ref pads[i]);
            }
            return mask;
        }

        bool CheckFinite(ReflexBikeState state)
        {
            // Positions and the velocity are SSE float4s whose w lane is unused padding: the original's
            // vector math leaves NaN/Inf there after some contacts (same bytes as the original), so only
            // x/y/z are checked; orientations are quaternions (all four lanes used).
            static bool Finite(float[] a, bool skipW) { for (int i = 0; i < a.Length; i++) if ((!skipW || i % 4 != 3) && (float.IsNaN(a[i]) || float.IsInfinity(a[i]))) return false; return true; }
            return Finite(state.renderPosition, true) && Finite(state.position, true) && Finite(state.velocity, true)
                && Finite(state.renderOrientation, false) && Finite(state.orientation, false) && !float.IsNaN(state.speed) && !float.IsInfinity(state.speed);
        }

        static Vector3 P(float[] a, int i) => new Vector3(a[i * 4], a[i * 4 + 1], a[i * 4 + 2]);
        static Quaternion Q(float[] a, int i) => new Quaternion(a[i * 4], a[i * 4 + 1], a[i * 4 + 2], a[i * 4 + 3]);

        void Apply()
        {
            var rp = state.renderPosition; var ro = state.renderOrientation;
            if (!calibrated)
            {
                // The proven chassis-frame mapping: the model is modelled in the chassis frame, so each
                // part is bound by its first-frame rotation relative to body 0.
                frontBind = Quaternion.Inverse(Q(ro, 2)) * Q(ro, 0);
                rearBind = Quaternion.Inverse(Q(ro, 3)) * Q(ro, 0);
                riderBind = Quaternion.Inverse(Q(ro, 1)) * Q(ro, 0);
                calibrated = true;
                SetVehicleActive(true);
                MoveCamera(true);
            }
            if (chassis) chassis.SetPositionAndRotation(P(rp, 0), Q(ro, 0));
            if (frontWheel) frontWheel.SetPositionAndRotation(P(rp, 2), Q(ro, 2) * frontBind);
            if (rearWheel) rearWheel.SetPositionAndRotation(P(rp, 3), Q(ro, 3) * rearBind);
            if (riderProxy) riderProxy.SetPositionAndRotation(P(rp, 1), Q(ro, 1) * riderBind);
        }

        // The game's own rider skeleton for the completed frame (69 world matrices, completed as the renderer does).
        void ApplyRider(OriginalSimulation.Results done)
        {
            if (!rider || !rider.CanUseOriginalPose) return;
            if (done.riderBones < 0) { Fail("reflex_world_rider_pose failed"); return; }
            if (done.riderBones != OriginalRider.OriginalBoneCount || !rider.ApplyBoneMatrices(done.riderPose, done.riderBones)) return;
            if (bikeModel?.limbOffsets != null) RiderLimbRetarget.Apply(rider, bikeModel.limbOffsets, done.bikePose, done.bikeNodes);   // a bike mod's grips and pegs (w-bikemods)
            if (bikeModel != null && bikePose && bikePose.HasPose) RiderGripIK.Apply(rider, bikeModel);   // OPEN REFLEX: the hands closed on the grips (w-grips)
        }

        void LateUpdate()
        {
            if (gateView != null && gateView.ApplyCamera()) { }   // the Gate Selection page's camera (before the race's world is running too)
            else if (calibrated) MoveCamera(false);
            if (!double.IsNaN(updateStart)) busyEnd = Time.realtimeSinceStartupAsDouble;   // Original pacing: rendering, if any, moves it on
        }

        Vector3 cameraTarget, cameraForward = Vector3.forward;

        void MoveCamera(bool snap)
        {
            if (!chaseCamera || !haveState || ViewerActive) return;   // (the replay viewer places the camera; it let the onboard camera go when it opened)
            if (replayRenderer?.ApplyCamera() == true) { LeaveOnboard(); return; }   // the game's replays (the highlight reels)
            if (hovercraft && hovercraft.ApplyCamera(chaseCamera)) { LeaveOnboard(); return; }   // port extra: the chase camera of the driven hovercraft
            if (cameraMode == 1 && onboard)
            {
                var kb = Keyboard.current;
                Vector3 front = P(state.renderPosition, 2), rear = P(state.renderPosition, 3);
                if (hasVehicleInfo && vehicleInfo.FourWheels) { front = vehicleInfo.Front; rear = vehicleInfo.Rear; }   // an ATV / a machine: its axles (ATV bodies 2..5, machine bodies 1..4 are its wheels)
                var velocity = new Vector3(state.velocity[0], state.velocity[1], state.velocity[2]);
                bool validOnboardState = onboardState.valid != 0;
                onboard.SetContext(hasVehicleInfo && vehicleInfo.Atv, hasVehicleInfo && vehicleInfo.Machine,
                    rider && rider.gameObject.activeInHierarchy && (!validOnboardState || onboardState.rider_active != 0),
                    validOnboardState && onboardState.wrecked != 0, validOnboardState && onboardState.trick_active != 0,
                    OriginalInput.RightStickRides, validOnboardState && onboardState.front_grounded != 0,
                    validOnboardState && onboardState.rear_grounded != 0, validOnboardState && onboardState.contacts_valid != 0,
                    presentation != null ? presentation.Time : double.NaN);
                // the game's rider-lean inputs: [7] fore / aft (+ forward), [6] side (+ right)
                if (onboard.Apply(P(state.renderPosition, 0), Q(state.renderOrientation, 0), front, rear, velocity, state.speed,
                                  state.inputs[7], state.inputs[6], terrain ? terrain.UnityTerrain : null, kb != null && kb.leftAltKey.isPressed || OriginalInput.LookBackHeld)) return;
                // off the bike (a wreck): the game's camera, as MX Bikes goes external when crashed
            }
            if ((cameraMode == 0 || cameraMode == 1) && haveGameCamera && ApplyGameCamera()) return;
            Vector3 pos = P(state.renderPosition, 0);
            Vector3 forward = Q(state.renderOrientation, 0) * Vector3.forward; forward.y = 0;
            if (forward.sqrMagnitude < 0.001f) forward = cameraForward; else forward.Normalize();
            Vector3 target = pos + Vector3.up * 1.1f;
            // The game teleports the bike back to the track after a wreck: cut instead of swooping.
            if ((target - cameraTarget).sqrMagnitude > 30f * 30f) snap = true;
            cameraTarget = target;
            // Heading follows the bike smoothly (a tumbling bike would otherwise spin the camera).
            float dt = Time.unscaledDeltaTime;
            cameraForward = snap ? forward : Vector3.Slerp(cameraForward, forward, 1f - Mathf.Exp(-4f * dt)).normalized;
            Vector3 desired = target - cameraForward * 16f + Vector3.up * 7f + Vector3.Cross(Vector3.up, cameraForward) * 3f;
            if (snap) cameraVelocity = Vector3.zero;
            chaseCamera.transform.position = snap ? desired : Vector3.SmoothDamp(chaseCamera.transform.position, desired, ref cameraVelocity, 0.13f, 400, dt);
            chaseCamera.transform.LookAt(target + cameraForward);
        }

        /// Another camera than the rider's own view has the screen (a replay, the hovercraft): the Dynamic FPV camera lets go, so the whole rider is
        /// drawn again instead of its cockpit rider (arms and legs only). Driving in first person again puts the cockpit rider back (Publish).
        void LeaveOnboard() { if (onboard && onboard.Active) onboard.Deactivate(); }

        // The original render camera: rows right/up/forward in native axes (= Unity axes), vertical FOV in radians. A custom camera preset
        // (OPTIONS > CAMERA & HUD; ReflexCameraPresets) moves it over the driving camera (ReflexCameraRig).
        bool ApplyGameCamera()
        {
            var c = gameCamera;
            var forward = new Vector3(c.fx, c.fy, c.fz); var up = new Vector3(c.ux, c.uy, c.uz); var pos = new Vector3(c.px, c.py, c.pz);
            if (forward.sqrMagnitude < 0.5f || up.sqrMagnitude < 0.5f || !(c.fov > 0.05f && c.fov < 3f) || float.IsNaN(pos.x)) return false;
            var preset = Reflex.Port.Options.ReflexCameraPresets.ActiveFor(cameraMode, DrivingCameraPreset >= 0 ? DrivingCameraPreset : Reflex.Port.Options.ReflexSettings.DrivingCamera);
            if (preset != null && haveState && cameraRig.Apply(preset, RigFrame(pos, forward, up, c.fov), out var rigPos, out var rigRot, out float rigFov))
            {
                chaseCamera.transform.SetPositionAndRotation(rigPos, rigRot);
                chaseCamera.fieldOfView = rigFov;
            }
            else
            {
                if (preset == null) cameraRig.Reset();
                chaseCamera.transform.SetPositionAndRotation(pos, Quaternion.LookRotation(forward, up));
                chaseCamera.fieldOfView = Reflex.Port.Options.ReflexClientLive.ChaseFov(c.fov) * Mathf.Rad2Deg;   // CHASE CAMERA FOV (Options; 100 %: the game's)
            }
            if (c.nearPlane > 0 && c.farPlane > c.nearPlane) { chaseCamera.nearClipPlane = DrivingNear(c.nearPlane); chaseCamera.farClipPlane = c.farPlane; }
            return true;
        }

        /// The game's third-person cameras clip at 2 ft (the gate and chase cameras' near plane, read headless: core/tools/camera_distance_probe.c;
        /// the intro's cameras 0.5): anything within 2 ft of the lens - a fence, a banner, a rider alongside, the ground under a low custom camera,
        /// the rider under a close one - was cut open and showed what is behind it. The original needed that for its 24-bit depth buffer; this
        /// renderer's depth is reversed float (Metal, D3D11 / 12, Vulkan), so the near plane is 0.25 ft (3 in) and nothing opens up until it
        /// touches the lens (w-camfix). The game's first-person camera keeps its own 0.5 (the rider's head is not drawn into the view).
        public const float ThirdPersonNear = 0.25f;
        float DrivingNear(float gameNear) => cameraMode == 0 && DrivingCameraPreset == 3 ? gameNear : Mathf.Min(gameNear, ThirdPersonNear);

        readonly ReflexCameraRig cameraRig = new ReflexCameraRig();
        /// The custom camera preset's rig (its shake, blend: tests and the showcase).
        public ReflexCameraRig CameraRig => cameraRig;
        /// The driving camera can be the game's view: the activity runs (Countdown .. WindDown) or has no race state (yet: the rig also wants the game's
        /// camera near the bike), the intro card is down; not the post-race pages or the podium.
        public bool DrivingView => Running && !IntroCardShown && (race.valid == 0 || race.state >= 1 && race.state <= 3);
        /// A custom camera preset shows (ReflexCameraRig): the driving view once the gate has dropped (InProgress .. WindDown, or no race state: Free
        /// Ride). Until then the game's own gate camera (w-camfix: a preset over the gate camera, 6.8 ft from the bike, put the view inside the rider);
        /// the camera editor's preview also shows it at the gate.
        public bool CustomCameraView => DrivingView && (race.valid == 0 || race.state >= 2 || Reflex.Port.Options.ReflexCameraPresets.Preview != null);

        ReflexCameraRig.Frame RigFrame(Vector3 pos, Vector3 forward, Vector3 up, float fov)
        {
            bool known = onboardState.valid != 0 && onboardState.contacts_valid != 0;
            float real = Time.unscaledDeltaTime;
            return new ReflexCameraRig.Frame
            {
                gamePos = pos, gameForward = forward, gameUp = up, gameFov = fov, aspect = chaseCamera.aspect,
                bike = P(state.renderPosition, 0), bikeRot = Q(state.renderOrientation, 0),
                velocity = state.velocity != null ? new Vector3(state.velocity[0], state.velocity[1], state.velocity[2]) : Vector3.zero, speed = state.speed,
                riding = CustomCameraView, contactsKnown = known,
                holdKey = ReflexCameraRig.HoldKey(hasVehicleInfo ? vehicleInfo.type : 0, DrivingCameraPreset >= 0 ? DrivingCameraPreset : Reflex.Port.Options.ReflexSettings.DrivingCamera), grounded = known && (onboardState.front_grounded != 0 || onboardState.rear_grounded != 0),
                wrecked = onboardState.valid != 0 && onboardState.wrecked != 0, terrain = terrain,
                dt = paused ? 0f : real, preview = Reflex.Port.Options.ReflexCameraPresets.Preview != null ? real : 0f,
            };
        }

        public OriginalOptionsMenu optionsMenu;
        public OriginalPauseMenu pauseMenu;
        /// Back to the gate: the world recreated from its image (F5, the pause menu's RESTART).
        // Paused edits are persisted immediately; native application runs on the worker when riding resumes.
        public void ApplyOpenTuning(Reflex.Port.Content.ReflexGarage.Tuning tuning)
        {
            if (simulation?.Open == null) return;
            int[] values = { tuning.engine1, tuning.engine2, tuning.suspension1, tuning.suspension2, tuning.tire1, tuning.tire2, tuning.braking, tuning.steering };
            simulation.Open.BetweenFrames(s => {
                if (RxApiNative.rx_session_tuning_set(s,values) != 0)
                    throw new InvalidOperationException("Vehicle tuning failed: " + RxApiNative.rx_session_last_error(s));
            });
        }

        public void Restart() { if (ownedCareerLaunch != null) return; restarting = true; Create(); }
        bool restarting;
        /// The running world with its simulation thread stopped (the event is over): MOTOCAREER's post-race commit runs the game's own
        /// completion in it (ReflexCareer.Finish) before the scene goes.
        public IntPtr StopSimulation() { presentation?.CloseLog(); presentation = null; if (simulation != null) { simulation.Dispose(); simulation = null; } return world; }
        /// The running world's simulation thread (null when stopped): MOTOCAREER's in-race pages call the game between its frames.
        /// The OLD plugin's simulation (null while REFLEX_SIM=open: the core has its own session and no old-plugin extras).
        public OriginalSimulation Simulation => simulation?.Legacy;
        /// The active backend's worker, for read-only presentation snapshots between simulation frames.
        public IReflexSimDriver SimDriver => simulation;
        [Tooltip("The original game's in-race HUD (OriginalHud).")]
        public OriginalHud originalHud;
        /// The race's riders (slots, names, bikes and gear from the running game's tables) and every rider's standing.
        public ReflexRoster Roster { get; private set; }
        public readonly ReflexDriverResult[] Drivers = new ReflexDriverResult[Reflex.Port.RxCore.RxRiderCap.Max];
        public int DriverCount { get; private set; }
        ReflexHudState hudState; bool haveHud;

        /// The original in-race HUD on or off (saved).
        public bool OriginalHudVisible
        {
            get => Reflex.Port.Options.ReflexSettings.HudVisible;
            set { Reflex.Port.Options.ReflexSettings.HudVisible = value; Reflex.Port.Options.ReflexSave.Save(); if (originalHud) originalHud.visible = value; }
        }

        /// The HUD export of the last completed frame (reflex_world_hud) and whether the plugin has it; the bike state (Options' HUD model).
        public Reflex.Port.RxCore.RxWaypointState Waypoint { get; private set; }
        /// The Free Ride activation area of the last collected frame (open path: rx_session_activation; OwnActivationArea).
        public Reflex.Port.RxCore.RxActivationState Activation { get; private set; }
        public bool HasActivation { get; private set; }
        /// The challenge activity's medal state of the last collected frame (open path: rx_session_challenge; OwnPostChallenge).
        public Reflex.Port.RxCore.RxChallengeState Challenge { get; private set; }
        public bool HasChallenge { get; private set; }
        /// The HUD compass' markers of the last collected frame (open path: rx_session_compass; the ORIGINAL HUD's compass).
        public Reflex.Port.RxCore.RxCompassState Compass { get; private set; }
        public bool HasCompass { get; private set; }
        /// RESTART of the challenge page: the core restarts the activity in place (App.RestartActivity); `done` gets the call's result on
        /// the worker (0 ok). False when this world has no open core session.
        public bool RestartChallenge(Action<int> done = null)
        {
            var open = simulation?.Open;
            if (open == null || !Running) return false;
            open.RestartChallenge(done);
            return true;
        }
        /// The challenge page's prize (PostChallengePopup:Start): the core pays it once per Post; `done` runs on the worker.
        public bool PayoutChallenge(int seriesId, Action<int, Reflex.Port.RxCore.RxChallengePayout> done)
        {
            var open = simulation?.Open;
            if (open == null || !Running) return false;
            open.PayoutChallenge(seriesId, done);
            return true;
        }
        /// The race format's HUD values of the last collected frame (rx_session_race_format: eliminator riders, the Outrun clock).
        public Reflex.Port.RxCore.RxRaceFormatState RaceFormat { get; private set; }
        public bool HasRaceFormat { get; private set; }
        /// The Eliminator / Outrun / Section Race / Freestyle components' values of the last collected frame (rx_session_race_hud).
        public Reflex.Port.RxCore.RxRaceHudState RaceHud { get; private set; }
        public bool HasRaceHud { get; private set; }
        /// The Waypoint / Freestyle / Section Race standings' result values of the last collected frame (rx_session_mode_results; the pause page's score tiles, the results page's table).
        public Reflex.Port.RxCore.RxModeResultsState ModeResults { get; private set; }
        public bool HasModeResults { get; private set; }
        /// The stunt feedback overlay's trick names of the last collected frame (rx_session_stunt_hud).
        public Reflex.Port.RxCore.RxStuntHudState StuntHud { get; private set; }
        public bool HasStuntHud { get; private set; }
        /// The wreck-recovery arrows of the last collected frame (rx_session_reflex_hud): MXUIReflexComponent's state.
        public Reflex.Port.RxCore.RxReflexHudState ReflexHud { get; private set; }
        public bool HasReflexHud { get; private set; }
        /// This world is a Free Ride challenge (the end page's world): its RESTART is the in-place App.RestartActivity.
        public bool IsChallengeWorld => postChallenge != null;
        /// RESTART of a career event's pause page: the core restarts the event's activity in place (App.RestartActivity); `done` gets the call's
        /// result on the worker (0 ok). False when this world has no running open core session.
        bool RestartCareerActivity(Action<int> done = null)
        {
            var open = simulation?.Open;
            if (open == null || !Running) return false;
            open.RestartActivity(done);
            return true;
        }
        /// The pause page's RESTART (PauseMenu:OnRestartEventPopupResult: App.RestartActivity, then SwitchToHud): in a challenge and in a career
        /// event the core restarts the activity in place (the world and the session stay; a career keeps its event, heat, moto and gate and takes
        /// no result of the aborted attempt: the series is written by the attempt that finishes), else the event starts again as a new session.
        public void RestartEvent()
        {
            if (IsChallengeWorld && RestartChallenge(r =>
            {
                if (r != 0) UnityEngine.Debug.LogWarning("[Challenge] the core could not restart the activity in place (" + r + ")");
            })) { UnityEngine.Debug.Log("[Challenge] pause RESTART: the activity restarts in place (rx_session_challenge_restart)"); return; }
            if (ownedCareerLaunch != null)
            {
                // (Restart() does nothing in a career: a new session would start the series over)
                if (OwnedCareerActive && RestartCareerActivity(r =>
                {
                    if (r != 0) UnityEngine.Debug.LogWarning("[Career] the core could not restart the activity in place (" + r + ")");
                }))
                {
                    var replay = Reflex.Port.Replay.ReflexReplay.Of(this);
                    if (replay) replay.SessionReset();   // the aborted attempt is not in the replay
                    UnityEngine.Debug.Log("[Career] pause RESTART: the activity restarts in place (rx_session_restart_activity)");
                }
                return;
            }
            Reflex.Port.RxCore.RxChampWeekend.PrepareRestart();   // (RESTORED CONTENT: a championship stage restarts as the same stage, no result)
            Restart();
        }
        /// PreChallengePopup:End: WaitForUI = 0. False when this world holds no challenge start.
        public bool ReleasePreChallenge(Action<int> done = null)
        {
            var open = simulation?.Open;
            if (open == null || !open.PreChallengeHeld) return false;
            open.ReleasePreChallenge(done);
            return true;
        }
        /// A challenge that waits for PreChallengePopup (the core's WaitForUI hold).
        public bool PreChallengeHeld => simulation?.Open?.PreChallengeHeld == true;
        public RxPowercrossHudState Powercross { get; private set; }
        /// The game's results page table (rx_session_results_page, w-resultsrng): null until the core ran the original's results page start
        /// (14 frames after "Show Activity Results"); its TOTAL / BEST LAP hold the projected times of the riders still racing.
        public RxResultsPage ResultsPage { get; private set; }
        public bool HasPowercross { get; private set; }
        public bool HasWaypoint { get; private set; }
        public bool IsWaypoint => HasWaypoint || EventIdentity?.StartsWith("WaypointRace/", StringComparison.Ordinal) == true;
        public ReflexHudState HudState => hudState;
        public bool HasHud => haveHud;
        public ReflexBikeState Bike => state;

        // keys and pad buttons held when a menu closed: kept from the game until released (an Esc that closes the options pages does
        // not also reach the game's own pause page)
        readonly System.Collections.Generic.HashSet<Key> heldAfterMenu = new System.Collections.Generic.HashSet<Key>();
        readonly ushort[] padHeld = new ushort[4];
        public void HoldKeysUntilReleased()
        {
            var kb = Keyboard.current;
            if (kb != null) foreach (var kv in DirectInputMap.Table) if (kb[kv.Key].isPressed) heldAfterMenu.Add(kv.Key);
            for (int i = 0; i < 4; i++) { heldPadDevices[i] = PadRouting.Pad(i); padHeld[i] = 0xffff; }   // every button until it reads released
        }

        static string CameraName(int mode) => Reflex.Port.Options.ReflexCameraPresets.DisplayName(mode);

        /// Camera: 0 the game's camera, 1 Dynamic FPV, 2 the follow camera (saved).
        public void SetDrivingCamera(int preset)
        {
            preset = Mathf.Clamp(preset, 0, 3);
            if (simulation?.Open != null) { simulation.Open.SetDrivingCamera(preset); openDrivingCamera = preset; }
            else GetComponent<OriginalInput>()?.SetSetting("DrivingCamera", preset);
        }
        public void SetCameraMode(int mode) => SetCameraMode(mode, true);
        public void SetCameraMode(int mode, bool persist)
        {
            GetComponent<OriginalOnboardAdjust>()?.End();
            cameraMode = Mathf.Clamp(mode, 0, 2);
            if (cameraMode == 1 && !onboard) cameraMode = 2;
            if (cameraMode != 1 && onboard) onboard.Deactivate();
            if (cameraMode == 1 && onboard) onboard.Resnap();
            if (cameraMode == 2 && chaseCamera) { chaseCamera.nearClipPlane = 0.15f; chaseCamera.farClipPlane = 14000; chaseCamera.fieldOfView = 47; MoveCamera(true); }
            if (persist) { Reflex.Port.Options.ReflexSettings.CameraMode = cameraMode; Reflex.Port.Options.ReflexSave.Save(); }
        }

        /// The client's own info overlay (status, telemetry, race, help) on or off (saved; off by default:
        /// the original HUD shows the race).
        public bool HudVisible
        {
            get => Reflex.Port.Options.ReflexSettings.ClientInfo;
            set
            {
                Reflex.Port.Options.ReflexSettings.ClientInfo = value; Reflex.Port.Options.ReflexSave.Save();
                ApplyClientInfoVisibility();
            }
        }

        static string FormatTime(float seconds)
        {
            if (seconds < 0 || float.IsNaN(seconds) || seconds > 35999) return "--:--.--";
            int m = (int)(seconds / 60f); float s = seconds - m * 60f;
            return $"{m}:{s:00.00}";
        }

        void RefreshHud()
        {
            // the overlay's texts only while it is shown (they were built every frame behind the disabled canvas: about 3 KB of strings
            // a frame, w-gcspikes); ApplyClientInfoVisibility shows it and the next frame fills it
            if (!HudVisible) return;
            if (statusLabel)
            {
                string riderNote = rider && rider.HasOriginalPose ? "Rider: original model, original animation" : "Rider: PROXY capsule (original rider model/pose unavailable)";
                statusLabel.text = (failure != null ? statusText : paused ? "PAUSED (F6 to resume)" : statusText) + "\n" + riderNote;
                statusLabel.color = failure != null ? new Color(1, 0.45f, 0.3f) : new Color(0.64f, 0.9f, 0.69f);
            }
            if (telemetryLabel)
            {
                var i = state.inputs ?? new float[8];
                telemetryLabel.text = $"{state.speed:0.0} <size=18>native units/s</size>   <color=#96A0AD>GEAR</color> {state.gear + 1}   <color=#96A0AD>FRAME</color> {state.frames}   <size=16><color=#96A0AD>SIM</color> {simMilliseconds:0.0} ms  <color=#96A0AD>FPS</color> {1f / Mathf.Max(1e-4f, Time.smoothDeltaTime):0}</size>\n" +
                    $"<size=16><color=#96A0AD>THR</color> {i[0]:0.00}  <color=#96A0AD>REV</color> {i[1]:0.00}  <color=#96A0AD>FBRK</color> {i[2]:0.00}  <color=#96A0AD>RBRK</color> {i[3]:0.00}  <color=#96A0AD>STEER</color> {i[4]:0.00},{i[5]:0.00}  <color=#96A0AD>LEAN</color> {i[6]:0.00},{i[7]:0.00}</size>";
            }
            if (raceLabel)
            {
                if (race.valid != 0 && race.num_laps > 0)
                {
                    bool unknown = Reflex.Port.RxCore.RxChampWeekend.LapTotalUnknown(race.num_laps);   // (a timed championship stage's clock still runs)
                    string total = unknown ? "TIMED" : race.num_laps.ToString();
                    int lap = Mathf.Clamp(race.laps_completed + 1, 1, race.num_laps);
                    bool finished = race.laps_completed >= race.num_laps;
                    string best = race.best_lap > 0 && race.best_lap < 1e30f ? FormatTime(race.best_lap) : "--:--.--";
                    string last = race.last_lap > 0 ? FormatTime(race.last_lap) : "--:--.--";
                    if (race.state == 0)
                        raceLabel.text = $"<color=#F2C14E>ON THE GATE</color>\n<size=26>GET READY</size>\n<size=16>{total} LAPS · {race.riders} RIDER{(race.riders == 1 ? "" : "S")}</size>";
                    else if (race.state == 1)
                    {
                        // Countdown: the start board (A+0x2cc, +0x4f4) holds the gate; its time isn't shown (the start is the rider's reaction).
                        raceLabel.text = $"<color=#F2C14E>ON THE GATE</color>\n<size=26>WATCH THE BOARD</size>\n<size=16>{total} LAPS · {race.riders} RIDER{(race.riders == 1 ? "" : "S")}</size>";
                    }
                    else
                    raceLabel.text = (finished ? "<color=#F2C14E>FINISHED</color>" : race.num_laps >= 0x61 ? $"LAP <b>{Mathf.Max(lap, Reflex.Port.Practice.RxPractice.LapNumber)}</b>" : unknown ? $"LAP <b>{lap}</b>" : $"LAP <b>{lap}</b>/{race.num_laps}") +
                        $"   <color=#96A0AD>POS</color> {race.place}/{race.riders}\n" +
                        $"<size=26>{FormatTime(finished ? race.race_time : race.lap_time)}</size>\n" +
                        $"<size=16><color=#96A0AD>LAST</color> {last}   <color=#96A0AD>BEST</color> {best}   <color=#96A0AD>RACE</color> {FormatTime(race.race_time)}</size>" +
                        (race.reset_timer > 0 ? "\n<color=#F28C4E>RESETTING TO TRACK</color>" : "");
                }
                else raceLabel.text = "";
            }
            if (helpLabel) helpLabel.text = "W throttle  S brake  A/D steer  Arrows rider lean (whips, preload)  X reverse  —  original DirectInput, gamepads 1-4   F5 reset world  F6 pause  F7 / Camera action: Visual / Game Play / User / 1st Person / Dynamic FPV  M music  F4 options" + (cameraMode == 1 && onboard ? "\n" + onboard.SettingsText + "   F8 head mode  F9/F10 FOV  F11 corner anticipation  F12 free look  hold L-Alt look back" : "");
        }
    }
}
