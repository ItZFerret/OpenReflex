// The settings applied to what is running: the race (OriginalWorldRuntime and its parts) when there is one, the display, the
// frame rate. Every options row writes ReflexSettings and then calls here, so a change shows at once where it can (the rest
// at the next race / start, as each row's help says).
using System;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Options
{
    public static class ReflexClientLive
    {
        static OriginalWorldRuntime s_runtime; static float s_lookedUp = -10;
        /// The race of this scene (null in the front end).
        public static OriginalWorldRuntime Runtime
        {
            get
            {
                if (s_runtime) return s_runtime;
                if (Time.unscaledTime - s_lookedUp < 0.5f) return null;
                s_lookedUp = Time.unscaledTime;
                return s_runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            }
        }
        public static OriginalInput Input { get { var rt = Runtime; return rt ? rt.GetComponent<OriginalInput>() : null; } }

        // ------------------------------------------------------------------------------------------- the game's UI settings
        /// App.SetUISetting in the running world (the game applies it: DrivingCamera at once, the others where the game reads them).
        public static void GameSetting(string name, int value)
        {
            var rt = Runtime;
            if (name == "DrivingCamera" && rt)
            {   // the camera now, and the game's own DrivingCamera setting: a player the race builds later (after the intro, at the gate) takes
                // the setting (Player_ApplyDrivingCamera 0054aef0), not the camera of the moment
                rt.SetDrivingCamera(value);
                rt.SimDriver?.Open?.SetUiSetting(name, value);
            }
            else
            {
                var i = Input; if (i) i.SetSetting(name, value);                       // the old plugin's world
                var open = rt ? rt.SimDriver?.Open : null; open?.SetUiSetting(name, value);   // the open core's session (rx_session_ui_setting)
            }
            SyncGameSettings();
        }

        /// The game's own UI settings as the options pages keep them (the open core reads them from its settings object: the mouse lean's
        /// sensitivity, the pads' vibration, the AI's CAREER DIFFICULTY, the stick layout, the measurement system, the HUD visibility, the camera).
        /// OpenReflexSimulation applies this list to every new session before its event loads; called on the main thread whenever they change.
        public static void SyncGameSettings()
        {
            Reflex.Port.RxCore.OpenReflexSimulation.GameUiSettings = new[]
            {
                new System.Collections.Generic.KeyValuePair<string, int>("MouseSensitivity", ReflexSettings.MouseSensitivity),
                new System.Collections.Generic.KeyValuePair<string, int>("Vibration", ReflexSettings.Vibration ? 1 : 0),
                new System.Collections.Generic.KeyValuePair<string, int>("CareerDifficulty", ReflexSettings.CareerDifficulty),
                new System.Collections.Generic.KeyValuePair<string, int>("ControllerHandedness", ReflexSettings.RightHanded ? 1 : 0),
                new System.Collections.Generic.KeyValuePair<string, int>("MeasurementSystem", ReflexSettings.Metric ? 1 : 0),
                new System.Collections.Generic.KeyValuePair<string, int>("OnlinePlayerName", ReflexSettings.OnlinePlayerName ? 0 : 1),
                new System.Collections.Generic.KeyValuePair<string, int>("HUDVisibility", ReflexSettings.HudVisible ? 0 : 1),
                // the saved camera in every session (races, free ride, career, the arena, test rides): the game's players apply it
                // when they are made; without it every race started on the guest default (VISUAL)
                new System.Collections.Generic.KeyValuePair<string, int>("DrivingCamera", ReflexSettings.DrivingCamera),
            };
        }

        /// AUTO LEVEL / COUNTER-STEER SNAP / AIR LOOSENESS (OPTIONS > OPEN REFLEX): every new session takes them before its event loads; the race running
        /// now gets them at its next frame (the open core only; the old plugin's world has no such options).
        public static void ApplyFreeAir()
        {
            float level = ReflexSettings.AirAutoLevel, snap = ReflexSettings.AirSnap, loose = ReflexSettings.AirLooseness;
            Reflex.Port.RxCore.OpenReflexSimulation.GameAirAutoLevel = level;
            Reflex.Port.RxCore.OpenReflexSimulation.GameAirSnap = snap;
            Reflex.Port.RxCore.OpenReflexSimulation.GameFreeAirLooseness = loose;
            var rt = Runtime;
            var open = rt ? rt.SimDriver?.Open : null; open?.SetFreeAir(level, snap, loose);
        }

        /// FREE RIDE WITHOUT BORDERS (OPTIONS > OPEN REFLEX): every new session takes it before its event loads; a Free Ride running now gets the
        /// change at its next frame (it never touches collision: w-dfcollide).
        public static void ApplyFreeRideBorders()
        {
            int mode = ReflexSettings.FreeRideBordersMode;
            Reflex.Port.RxCore.OpenReflexSimulation.GameFreeRideBorders = mode;
            var rt = Runtime;
            var open = rt ? rt.SimDriver?.Open : null; open?.SetFreeRideBorders(mode);
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void SyncAtStart() { try { SyncGameSettings(); Reflex.Port.RxCore.OpenReflexSimulation.GameFreeRideBorders = ReflexSettings.FreeRideBordersMode; Reflex.Port.RxCore.OpenReflexSimulation.GameAirAutoLevel = ReflexSettings.AirAutoLevel; Reflex.Port.RxCore.OpenReflexSimulation.GameAirSnap = ReflexSettings.AirSnap; Reflex.Port.RxCore.OpenReflexSimulation.GameFreeAirLooseness = ReflexSettings.AirLooseness; Reflex.Port.RxCore.RxPhysicsStudio.Everywhere = ReflexSettings.PhysicsEverywhere; ReflexSettings.Changed += key => { if (key != null && (key.StartsWith("ReflexUI.") || key == "ReflexOriginalHud")) SyncGameSettings(); }; } catch (Exception e) { Debug.LogException(e); } }

        public static void DrivingCamera(int v)
        {
            ReflexSettings.DrivingCamera = v;
            GameSetting("DrivingCamera", v);
            var rt = Runtime;
            if (rt && rt.cameraMode != OriginalPauseMenu.CameraModeOf[Mathf.Clamp(v, 0, 3)]) rt.SetCameraMode(OriginalPauseMenu.CameraModeOf[Mathf.Clamp(v, 0, 3)]);
            else if (!rt) ReflexSettings.CameraMode = OriginalPauseMenu.CameraModeOf[Mathf.Clamp(v, 0, 3)];
            ReflexSettings.Save();
        }

        public static void CameraMode(int mode) => CameraMode(mode, true);
        public static void CameraMode(int mode, bool persist)
        {
            var rt = Runtime;
            if (rt) rt.SetCameraMode(mode, persist); else if (persist) { ReflexSettings.CameraMode = mode; ReflexSettings.Save(); }
        }

        /// The built-in cameras: the game's four presets (its own camera) and the Dynamic FPV view (ReflexCameraPresets adds the player's own presets).
        public static readonly string[] CameraNames = { "VISUAL", "GAME PLAY", "USER", "1ST PERSON", "DYNAMIC FPV" };
        /// The built-in view in use (0..3 the game's DrivingCamera preset, 4 DYNAMIC FPV); a custom preset's is its BASE.
        public static int CameraIndex
        {
            get
            {
                var rt = Runtime;
                int mode = rt ? rt.cameraMode : ReflexSettings.CameraMode;
                if (mode == 1) return 4;
                int dc = rt && rt.Running && rt.DrivingCameraPreset >= 0 ? rt.DrivingCameraPreset : ReflexSettings.DrivingCamera;
                return Mathf.Clamp(dc, 0, 3);
            }
        }
        /// The name of the camera in use (a custom preset's own name).
        public static string CameraName => ReflexCameraPresets.Current.name;
        /// The Camera action, F7 and the pause pages' CAMERA rows: the next camera that is ON in OPTIONS > CAMERA & HUD > CAMERA CYCLE.
        public static void CycleCamera(int direction = 1) => ReflexCameraPresets.Cycle(direction);
        /// A built-in view by its CameraNames index (the original pause page's DrivingCamera list, the front end's).
        public static void SelectCamera(int index) => SelectCamera(index, true);
        public static void SelectCamera(int index, bool persist)
        {
            index = (index % 5 + 5) % 5;
            ApplyView(index, persist);
            ReflexCameraPresets.NoteBuiltin(index, persist);
        }
        /// The view itself: 4 = Dynamic FPV, else the game's camera on DrivingCamera preset index (a custom preset passes its BASE).
        internal static void ApplyView(int index, bool persist)
        {
            index = (index % 5 + 5) % 5;
            if (index == 4) { CameraMode(1, persist); return; }
            var rt = Runtime;
            if (rt && rt.cameraMode != 0) rt.SetCameraMode(0, persist); else if (!rt && persist) ReflexSettings.CameraMode = 0;
            GameSetting("DrivingCamera", index);
            if (persist) { ReflexSettings.DrivingCamera = index; ReflexSettings.Save(); }
        }

        public static void HudVisible(bool on)
        {
            var rt = Runtime;
            if (rt) rt.OriginalHudVisible = on; else { ReflexSettings.HudVisible = on; ReflexSettings.Save(); }
        }

        public static void ClientInfo(bool on)
        {
            var rt = Runtime;
            if (rt) rt.HudVisible = on; else { ReflexSettings.ClientInfo = on; ReflexSettings.Save(); }
        }

        // ------------------------------------------------------------------------------------------- audio
        public static void EffectsVolume(float v)
        {
            ReflexSettings.EffectsVolume = v;
            var rt = Runtime; if (rt && rt.gameAudio) rt.gameAudio.masterVolume = v;
            var fe = UnityEngine.Object.FindAnyObjectByType<Reflex.Port.FrontEnd.FrontEndManager>();
            if (fe && fe.sounds != null) fe.sounds.volume = v;
            var shell = Reflex.Port.OpenFrontEnd.OpenShell.Instance; if (shell != null && shell.Sounds != null) shell.Sounds.volume = v;   // the open shell's UI sounds and narration
            ReflexSettings.Save();
        }
        public static void MusicVolume(float v)
        {
            ReflexSettings.MusicVolume = v;
            var m = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>(); if (m) m.volume = v;
            ReflexOptionsHooks.MusicVolumeChanged?.Invoke();
            ReflexSettings.Save();
        }
        /// MASTER VOLUME: every sound.
        public static void MasterVolume(float v)
        {
            ReflexSettings.MasterVolume = v; AudioListener.volume = ReflexSettings.MasterVolume; ReflexSettings.Save();
        }
        public static void MusicOn(bool on)
        {
            ReflexSettings.MusicOn = on;
            var m = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>(); if (m) m.playing = on;
            ReflexOptionsHooks.MusicVolumeChanged?.Invoke();
            ReflexSettings.Save();
        }

        // ------------------------------------------------------------------------------------------- the Dynamic FPV camera
        // (OriginalOnboardCamera: its settings are the ReflexFpv.* keys; a running camera reads them again now, and on its next frame after any change of
        // one anyway: ReflexSave.Changed)
        public static void FpvChanged() { var rt = Runtime; if (rt && rt.onboard) rt.onboard.Load(); ReflexSettings.Save(); }

        // ------------------------------------------------------------------------------------------- display
        // Explicit process overrides never modify the saved preference.
        public static int LaunchFrameRateCap => ParseLaunchFrameRateCap(Environment.GetEnvironmentVariable("REFLEX_FPS_CAP"));
        static int ParseLaunchFrameRateCap(string value) => int.TryParse(value, out int cap) && cap > 0 ? Mathf.Clamp(cap, 15, 240) : 0;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void InitialFrameRateOverride()
        {
            // (only the launch override here: the saved LOW LATENCY applies with the first ApplyFrameRate, after the save has loaded)
            if (Environment.GetEnvironmentVariable("REFLEX_MAX_QUEUED_FRAMES") != null) ApplyLatency();
            ApplyFrameRateOverride();
        }

        /// LOW LATENCY: the frames the driver may queue ahead of the display (QualitySettings.maxQueuedFrames; Unity's own value on PC is 2).
        /// Unity starts a frame after frame n - maxQueuedFrames reached the screen, so with VSync (and whenever the GPU or the display is
        /// the limit) each queued frame is one refresh between the input read and the picture. 1 saves one, but then a frame's CPU work
        /// waits for the previous frame to be shown, so the CPU and GPU overlap less (hence OFF by default). D3D11 / D3D12 / Vulkan honour
        /// it (Unity's docs). REFLEX_MAX_QUEUED_FRAMES=1..3 overrides it (tests / a tester's comparison) without touching the saved choice.
        public static int QueuedFrames => int.TryParse(Environment.GetEnvironmentVariable("REFLEX_MAX_QUEUED_FRAMES"), out int q) && q >= 1 && q <= 3 ? q
                                        : ReflexSettings.LowLatency == 1 && !LatencySteppedBack ? 1 : UnityQueuedFrames;
        /// LOW LATENCY decides the queue: ON and no REFLEX_MAX_QUEUED_FRAMES override.
        public static bool LowLatencyRequested => ReflexSettings.LowLatency == 1 && Environment.GetEnvironmentVariable("REFLEX_MAX_QUEUED_FRAMES") == null;
        /// ReflexLatencyGuard: LOW LATENCY is ON but the frames missed the refresh with one queued frame, so Unity's own queue is used
        /// for now (the saved choice is unchanged).
        public static bool LatencySteppedBack { get; internal set; }
        static int s_unityQueued = -1;   // Unity's own value, read before the first change (OFF puts it back)
        static int UnityQueuedFrames => s_unityQueued > 0 ? s_unityQueued : 2;
        public static void ApplyLatency()
        {
            try
            {
                if (s_unityQueued < 0) s_unityQueued = QualitySettings.maxQueuedFrames;
                if (QualitySettings.maxQueuedFrames != QueuedFrames) QualitySettings.maxQueuedFrames = QueuedFrames;
            }
            catch (Exception e) { Debug.LogWarning("[Options] queued frames: " + e.Message); }
        }
        public static bool ApplyFrameRateOverride()
        {
            int cap = LaunchFrameRateCap;
            return cap > 0 && ApplyFrameRateLimit(cap, false);
        }

        // Desktop VSync ignores targetFrameRate, so finite caps must disable
        // effective VSync. Keep the stored preference for when the cap is OFF.
        static bool ApplyFrameRateLimit(int cap, bool preferVsync)
        {
            // DISPLAY RATE (-1): one frame per refresh (VSync on, no cap); OFF (0): the VSYNC preference; a cap: VSync off, that rate
            QualitySettings.vSyncCount = cap > 0 ? 0 : cap == ReflexSettings.DisplayRate || preferVsync ? 1 : 0;
            if (cap <= 0) return false;
            Application.targetFrameRate = cap;
            return true;
        }
        static int ResolveFrameRateCap(string launchValue, int savedCap)
        {
            int launch = ParseLaunchFrameRateCap(launchValue);
            return launch > 0 ? launch : Mathf.Max(ReflexSettings.DisplayRate, savedCap);
        }
        public static bool ApplyFrameRateLimit()
        {
            int cap = ResolveFrameRateCap(Environment.GetEnvironmentVariable("REFLEX_FPS_CAP"), ReflexSettings.FpsCap);
            return ApplyFrameRateLimit(cap, ReflexLaunchSettings.Current.vsync);
        }

        /// Shared policy for startup, menus and races: launch cap, saved cap,
        /// then saved VSync and the runtime's normal presentation pacing.
        public static void ApplyFrameRate()
        {
            ApplyLatency();
            if (ApplyFrameRateLimit()) return;
            var rt = Runtime;
            if (rt) rt.ApplyFrameRate();
            else Application.targetFrameRate = -1;
        }

        /// Player builds: the display settings at once (the editor's project settings stay as they are; the values are stored).
        public static bool CanApplyDisplay => !Application.isEditor && !Application.isBatchMode;

        public static void ApplyDisplay()
        {
            if (!CanApplyDisplay) return;
            try
            {
                var ls = ReflexLaunchSettings.Current;
                var mode = ls.fullscreen ? FullScreenMode.FullScreenWindow : FullScreenMode.Windowed;
                var bb = ls.BackBuffer;
                if (!ReflexWindowOverride.TryApply()) Screen.SetResolution(bb.x, bb.y, mode);
                QualitySettings.vSyncCount = ls.vsync ? 1 : 0;
            }
            catch (Exception e) { Debug.LogWarning("[Options] display: " + e.Message); }
            ApplyFrameRate();
        }

        /// The game's own video options that can change under a running game (graphics.cfg: MOTION BLUR, ANTI-ALIASING); the texture and shadow
        /// qualities are read when the assets and the shadow maps are made, so those apply at the next start.
        public static void ApplyGraphicsLive()
        {
            try
            {
                var ls = ReflexLaunchSettings.Current;
                var pipeline = UnityEngine.Object.FindAnyObjectByType<Reflex.Port.Rendering.ReflexRenderPipeline>();
                if (pipeline) pipeline.motionBlur = ls.motionBlur;
                if (CanApplyDisplay && UnityEngine.Rendering.GraphicsSettings.currentRenderPipeline is UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset urp)
                { urp.msaaSampleCount = ls.MsaaSamples; QualitySettings.antiAliasing = ls.MsaaSamples > 1 ? ls.MsaaSamples : 0; }
            }
            catch (Exception e) { Debug.LogWarning("[Options] graphics: " + e.Message); }
        }

        public static void ApplyQuality()
        {
            if (!CanApplyDisplay) return;
            int q = ReflexSettings.Quality;
            if (q >= 0 && q < QualitySettings.names.Length && QualitySettings.GetQualityLevel() != q) QualitySettings.SetQualityLevel(q, true);
            if (UnityEngine.Rendering.GraphicsSettings.currentRenderPipeline is UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset urp)
                urp.renderScale = Mathf.Clamp(ReflexSettings.RenderScale / 100f, 0.1f, 2f);
            ApplyFrameRate();
        }

        /// At start (player builds): the client's own display settings over graphics.cfg's (ReflexLaunchSettings applies those).
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void ApplyAtStart()
        {
            try { ReflexSave.Load(); SyncGameSettings(); AudioListener.volume = ReflexSettings.MasterVolume; } catch (Exception e) { Debug.LogException(e); }
            if (!CanApplyDisplay) return;
            ApplyQuality();
        }

        // ------------------------------------------------------------------------------------------- the race's hooks
        /// OriginalWorldRuntime.Start: the client's parts of the race (the MODERN HUD, the pause extras, the rumble strength).
        public static void OnWorldStart(OriginalWorldRuntime rt)
        {
            if (!rt) return;
            s_runtime = rt;
            try
            {
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !rt.GetComponent<Reflex.Port.OpenFrontEnd.OpenSessionError>()) rt.gameObject.AddComponent<Reflex.Port.OpenFrontEnd.OpenSessionError>();
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
                    rt.racePause = rt.GetComponent<Reflex.Port.OpenFrontEnd.OwnRacePause>() ?? rt.gameObject.AddComponent<Reflex.Port.OpenFrontEnd.OwnRacePause>();
                if (!rt.GetComponent<Reflex.Port.Hud.ReflexModernHud>()) rt.gameObject.AddComponent<Reflex.Port.Hud.ReflexModernHud>();
                if (!rt.GetComponent<Reflex.Port.Hud.ReflexCameraToast>()) rt.gameObject.AddComponent<Reflex.Port.Hud.ReflexCameraToast>();   // the camera's name with the CLASSIC / a hidden HUD
                if (!rt.GetComponent<ReflexPauseExtras>()) rt.gameObject.AddComponent<ReflexPauseExtras>();
                if (!rt.GetComponent<ReflexRumbleScale>()) rt.gameObject.AddComponent<ReflexRumbleScale>();
                if (!rt.GetComponent<Reflex.Port.Replay.ReflexReplay>()) rt.gameObject.AddComponent<Reflex.Port.Replay.ReflexReplay>();   // REPLAY / PHOTO MODE (w-replayphoto)
                if (!rt.optionsMenu) { rt.optionsMenu = rt.GetComponent<OriginalOptionsMenu>(); if (!rt.optionsMenu) rt.optionsMenu = rt.gameObject.AddComponent<OriginalOptionsMenu>(); rt.optionsMenu.runtime = rt; }
            }
            catch (Exception e) { Debug.LogException(e); }
            ApplyFrameRate();
        }

        /// The game camera's field of view (radians, vertical) as CHASE CAMERA FOV scales it (tangent space; 100 % = the game's).
        public static float ChaseFov(float fovRadians)
        {
            int pct = ReflexSettings.ChaseFov;
            if (pct == 100) return fovRadians;
            float t = Mathf.Tan(fovRadians * 0.5f) * pct / 100f;
            return Mathf.Clamp(2f * Mathf.Atan(t), 0.1f, 2.9f);
        }
    }

}
