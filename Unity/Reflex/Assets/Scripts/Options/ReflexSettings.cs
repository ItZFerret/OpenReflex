// The client's settings as one typed API (every part of the client reads and writes them here; ReflexSave keeps them).
//
// Two kinds:
//  * the game's own UI settings and the client's old keys, under their old PlayerPrefs names (the original front end's OPTIONS
//    page, the pause page's PauseOptions, the race runtime and the tests use those names; ReflexSave mirrors them):
//      DrivingCamera ReflexUI.DrivingCamera 0..3 (Visual, Game Play, User, 1st Person), CameraMode ReflexCameraMode 0..2 (the game's
//      camera, Dynamic FPV, the follow camera), HudVisible ReflexOriginalHud, ClientInfo ReflexClientInfo, EffectsVolume
//      ReflexAudio.master 0..1, MusicVolume ReflexAudio.music 0..1, MusicOn ReflexAudio.musicOn, EngineVolume ReflexUI.EngineVolume
//      0..10, Vibration ReflexUI.Vibration (the game's own, 0 = off on the PC), MouseSensitivity ReflexUI.MouseSensitivity 0..100,
//      Metric ReflexUI.MeasurementSystem, CareerDifficulty ReflexUI.CareerDifficulty 80 / 95 / 105 / 120, NativeGame ReflexNativeGame
//      0..2 (OriginalWorldRuntime.NativeGame; unset = the scene's choice), the Dynamic FPV camera's ReflexFpv.* (the rewrite's Fpv.Config taken once);
//  * the client's own (Open Unleashed's options), in save.json only: HudStyle, RumbleStrength, ChaseFov, FpsCap, Quality,
//    RenderScale, Lighting.
// The display settings the game itself reads at start (resolution, fullscreen, vsync, anti-aliasing, texture / shadow quality,
// motion blur) stay graphics.cfg's (ReflexLaunchSettings: the install's file is the default, the per-user copy the player's
// choice; the backups hold it).
using System;
using UnityEngine;

namespace Reflex.Port.Options
{
    public enum HudStyle { Original = 0, Modern = 1 }

    public static class ReflexSettings
    {
        public static event Action<string> Changed { add => ReflexSave.Changed += value; remove => ReflexSave.Changed -= value; }

        // ------------------------------------------------------------------------------------------- the game's own (old keys)
        public static int DrivingCamera { get => Mathf.Clamp(ReflexSave.GetInt("ReflexUI.DrivingCamera", 0), 0, 3); set => ReflexSave.SetInt("ReflexUI.DrivingCamera", Mathf.Clamp(value, 0, 3)); }
        public static int CameraMode { get => Mathf.Clamp(ReflexSave.GetInt("ReflexCameraMode", 0), 0, 2); set => ReflexSave.SetInt("ReflexCameraMode", Mathf.Clamp(value, 0, 2)); }
        public static bool HudVisible { get => ReflexSave.GetInt("ReflexOriginalHud", 1) != 0; set => ReflexSave.SetInt("ReflexOriginalHud", value ? 1 : 0); }
        public static bool ClientInfo { get => ReflexSave.GetInt("ReflexClientInfo", 0) != 0; set => ReflexSave.SetInt("ReflexClientInfo", value ? 1 : 0); }
        public static float EffectsVolume { get => Mathf.Clamp(ReflexSave.GetFloat("ReflexAudio.master", 1f), 0f, 2f); set => ReflexSave.SetFloat("ReflexAudio.master", Mathf.Clamp(value, 0f, 2f)); }
        public static float MusicVolume { get => Mathf.Clamp01(ReflexSave.GetFloat("ReflexAudio.music", 0.6f)); set => ReflexSave.SetFloat("ReflexAudio.music", Mathf.Clamp01(value)); }
        public static bool MusicOn { get => ReflexSave.GetInt("ReflexAudio.musicOn", 1) != 0; set => ReflexSave.SetInt("ReflexAudio.musicOn", value ? 1 : 0); }
        public static int EngineVolume { get => Mathf.Clamp(ReflexSave.GetInt("ReflexUI.EngineVolume", 10), 0, 10); set => ReflexSave.SetInt("ReflexUI.EngineVolume", Mathf.Clamp(value, 0, 10)); }
        /// The game's Vibration UI setting (the game rumbles only when it is on; the PC game's default is off).
        public static bool Vibration { get => ReflexSave.GetInt("ReflexUI.Vibration", 0) != 0; set => ReflexSave.SetInt("ReflexUI.Vibration", value ? 1 : 0); }
        public static int MouseSensitivity { get => Mathf.Clamp(ReflexSave.GetInt("ReflexUI.MouseSensitivity", 50), 0, 100); set => ReflexSave.SetInt("ReflexUI.MouseSensitivity", Mathf.Clamp(value, 0, 100)); }
        /// MeasurementSystem 1 = metric (km/h), 0 = US (mph).
        public static bool Metric { get => ReflexSave.GetInt("ReflexUI.MeasurementSystem", 0) == 1; set => ReflexSave.SetInt("ReflexUI.MeasurementSystem", value ? 1 : 0); }
        public static readonly int[] CareerDifficulties = { 80, 95, 105, 120 };
        /// CareerDifficulty 80 ROOKIE (the original's default: UISettings_Defaults) / 95 AMATEUR / 105 PRO / 120 ALL TIME; the core's AI reads it.
        public static int CareerDifficulty { get => ReflexSave.GetInt("ReflexUI.CareerDifficulty", 80); set => ReflexSave.SetInt("ReflexUI.CareerDifficulty", value); }
        /// ControllerHandedness 1 = right (the original's default: the left stick steers, the right stick / arrows move the rider), 0 = left (swapped).
        public static bool RightHanded { get => ReflexSave.GetInt("ReflexUI.ControllerHandedness", 1) != 0; set => ReflexSave.SetInt("ReflexUI.ControllerHandedness", value ? 1 : 0); }
        public static bool OnlinePlayerName { get => ReflexSave.GetInt("ReflexUI.OnlinePlayerName", 0) == 0; set => ReflexSave.SetInt("ReflexUI.OnlinePlayerName", value ? 0 : 1); }
        /// The native game (OriginalWorldRuntime.NativeGameMode 0 Off, 1 Native, 2 Verify); -1: not chosen (the scene's default).
        public static int NativeGame { get => ReflexSave.Has("ReflexNativeGame") ? Mathf.Clamp(ReflexSave.GetInt("ReflexNativeGame", 1), 0, 2) : -1; set { if (value < 0) ReflexSave.Delete("ReflexNativeGame"); else ReflexSave.SetInt("ReflexNativeGame", Mathf.Clamp(value, 0, 2)); } }

        // ------------------------------------------------------------------------------------------- the client's own (save.json)
        public static readonly string[] HudStyleNames = { "CLASSIC", "MODERN" };
        public static bool ClassicSpeedometer { get => ReflexSave.GetInt("Hud.ClassicSpeedometer", 0) != 0; set => ReflexSave.SetInt("Hud.ClassicSpeedometer", value ? 1 : 0); }
        /// HUD STYLE: ORIGINAL the game's own HUD (live, or the hand-built fallback), exactly as before; MODERN the client's
        /// (ReflexModernHud, Open Unleashed's race HUD). Switches at once. Never chosen: it follows the FRONT END in use
        /// (Reflex.Port.OpenFrontEnd.FrontEndSwitch.Effective: OPEN REFLEX -> MODERN, as Open Unleashed; ORIGINAL -> ORIGINAL; headless
        /// runs use the original front end, so the tests that do not choose keep the ORIGINAL HUD).
        public static HudStyle HudStyle { get => ReflexSave.GetInt("Hud.Style", (int)DefaultHudStyle) == 1 ? HudStyle.Modern : HudStyle.Original; set => ReflexSave.SetInt("Hud.Style", (int)value); }
        public static HudStyle DefaultHudStyle => Reflex.Port.OpenFrontEnd.FrontEndSwitch.Effective == Reflex.Port.OpenFrontEnd.FrontEndStyle.Open ? HudStyle.Modern : HudStyle.Original;
        /// The gamepads' rumble: the game's XInputSetState motors times this (0 .. 2, 1 = the game's own).
        public static float RumbleStrength { get => Mathf.Clamp(ReflexSave.GetFloat("Options.RumbleStrength", 1f), 0f, 2f); set => ReflexSave.SetFloat("Options.RumbleStrength", Mathf.Clamp(value, 0f, 2f)); }
        /// CHASE CAMERA FOV: the game camera's field of view scaled (percent of the game's own, in tangent space; 100 = the game's).
        public static int ChaseFov { get => Mathf.Clamp(ReflexSave.GetInt("Options.ChaseFov", 100), ChaseFovMin, ChaseFovMax); set => ReflexSave.SetInt("Options.ChaseFov", Mathf.Clamp(value, ChaseFovMin, ChaseFovMax)); }
        public const int ChaseFovMin = 70, ChaseFovMax = 150;
        /// FRAME RATE CAP choices: DisplayRate (-1), OFF (0), then fps caps.
        public static readonly int[] FpsCaps = { DisplayRate, 0, 30, 60, 75, 90, 120, 144, 165, 240 };
        /// FRAME RATE CAP DISPLAY RATE: one frame per display refresh (VSync on, no extra cap).
        public const int DisplayRate = -1;
        /// FRAME RATE CAP (-1 = DISPLAY RATE, 0 = off: the VSYNC preference decides, > 0 = that many fps). Nothing saved: DISPLAY RATE (the lead's
        /// decision, w-menu3d: the old default of 30 made the menus and races step visibly on 60-120 Hz displays); a saved choice wins.
        public static int FpsCap { get => Mathf.Max(DisplayRate, ReflexSave.GetInt("Options.FpsCap", DisplayRate)); set => ReflexSave.SetInt("Options.FpsCap", Mathf.Max(DisplayRate, value)); }
        /// QUALITY: a QualitySettings level (-1 = the project's own). Player builds.
        public static int Quality { get => ReflexSave.GetInt("Options.Quality", -1); set => ReflexSave.SetInt("Options.Quality", value); }
        /// LIGHTING: 0 ORIGINAL (the original renderer, default), 1 ENHANCED (Rendering/ReflexEnhancedLighting: modern shadows, AO, HDRI skies, tone mapping).
        public static int Lighting { get => Mathf.Clamp(ReflexSave.GetInt("Options.Lighting", 0), 0, 1); set => ReflexSave.SetInt("Options.Lighting", Mathf.Clamp(value, 0, 1)); }
        /// ENHANCED QUALITY (OPTIONS > VIDEO): the ENHANCED lighting's tier, -1 AUTO (SHADOW QUALITY, at most what the GPU suits:
        /// ReflexEnhancedLighting.AutoTier), 0 LOW, 1 MEDIUM, 2 HIGH.
        public static int EnhancedQuality { get => Mathf.Clamp(ReflexSave.GetInt("Options.EnhancedQuality", -1), -1, 2); set => ReflexSave.SetInt("Options.EnhancedQuality", Mathf.Clamp(value, -1, 2)); }
        /// DRAW DISTANCE (OPTIONS > VIDEO, w-rmlod): 0 ORIGINAL (the original renderer's tree / scenery / grass distances and shadow
        /// distance), 1 HIGH, 2 ULTRA (Rendering/ReflexDrawDistance: longer 3D tree, scenery LOD, grass and shadow distances, with
        /// cross-fades and hysteresis instead of hard swaps). Default HIGH in this test build.
        public static int DrawDistance { get => Mathf.Clamp(ReflexSave.GetInt("Options.DrawDistance", 1), 0, 2); set => ReflexSave.SetInt("Options.DrawDistance", Mathf.Clamp(value, 0, 2)); }
        /// FRAME PACING (OPTIONS > VIDEO): 0 ORIGINAL (the game's own 60 Hz loop, the lowest input latency: OriginalWorldRuntime PacingMode.Original),
        /// 1 SMOOTH (the game frames blended to the display's rate: PacingMode.Interpolated, one game frame later; for 120 Hz+ displays).
        public static int FramePacing { get => Mathf.Clamp(ReflexSave.GetInt("Options.FramePacing", 0), 0, 1); set => ReflexSave.SetInt("Options.FramePacing", Mathf.Clamp(value, 0, 1)); }
        /// LOW LATENCY (OPTIONS > VIDEO, w-latency): 1 ON lets the graphics driver queue one finished frame ahead of the display
        /// (QualitySettings.maxQueuedFrames = 1: with VSync up to one refresh less input delay), 0 OFF (default) Unity's own two. Off by
        /// default: with one queued frame Unity starts a frame only once the last one is on screen, so the CPU and GPU no longer overlap
        /// and a machine near its budget (the 30-40 fps playtesters) can drop to half the refresh. Presentation only: the game's frames
        /// and their results are the same either way (ReflexClientLive.ApplyLatency).
        public static int LowLatency { get => Mathf.Clamp(ReflexSave.GetInt("Options.LowLatency", 0), 0, 1); set => ReflexSave.SetInt("Options.LowLatency", Mathf.Clamp(value, 0, 1)); }
        /// ASPECT RATIO (OPTIONS > VIDEO): 1 FILL SCREEN (default; ultrawide and other non-16:9 screens get the whole picture, the view
        /// widened sideways at the game's vertical field of view), 0 ORIGINAL 16:9 (the game's centred 16:9 picture with black bars,
        /// FUN_007ad3c0; windowed: the largest 16:9 window). Presentation only (ReflexLaunchSettings.BackBuffer / ViewportRect).
        /// TERRAIN DETAIL (OPTIONS > VIDEO): 0 STANDARD (Unity terrain pixel error 3), 1 HIGH (pixel error 1: full-resolution terrain much
        /// further out). The original draws its terrain on one grid near the camera; Unity's LOD steps to coarser patches a few metres out,
        /// where the original's per-vertex RT_Bump height detail changes look (a ring ahead of the rider). HIGH pushes that step far away.
        public static int TerrainDetail { get => Mathf.Clamp(ReflexSave.GetInt("Options.TerrainDetail", 1), 0, 1); set => ReflexSave.SetInt("Options.TerrainDetail", Mathf.Clamp(value, 0, 1)); }
        public static float TerrainPixelError => TerrainDetail == 1 ? 1f : 3f;
        public static int AspectFill { get => Mathf.Clamp(ReflexSave.GetInt("Options.AspectFill", 1), 0, 1); set => ReflexSave.SetInt("Options.AspectFill", Mathf.Clamp(value, 0, 1)); }
        /// MAP SCALE (the race setup page; core/plan/MAP_SCALE.md): a track's scale in percent (RxMapScale.MinPercent .. MaxPercent in
        /// steps of 5), kept per track (the event's location) in the client's settings; 100 % removes the key.
        public static int MapScalePercent(string track)
        {
            if (string.IsNullOrEmpty(track)) return Reflex.Port.RxCore.RxMapScale.DefaultPercent;
            return Reflex.Port.RxCore.RxMapScale.ClampPercent(ReflexSave.GetInt("Race.MapScale." + track, Reflex.Port.RxCore.RxMapScale.DefaultPercent));
        }
        public static void SetMapScalePercent(string track, int percent)
        {
            if (string.IsNullOrEmpty(track)) return;
            percent = Reflex.Port.RxCore.RxMapScale.ClampPercent(percent);
            if (percent == Reflex.Port.RxCore.RxMapScale.DefaultPercent) ReflexSave.Delete("Race.MapScale." + track);
            else ReflexSave.SetInt("Race.MapScale." + track, percent);
        }
        /// AUTO LEVEL and COUNTER-STEER SNAP (OPTIONS > OPEN REFLEX > RIDING, w-airsliders; core rx_session_set_air_helpers): 0 .. 1, how much of the
        /// original's two in-air helpers the player keeps (1 = the original's, the default; 0 = removed, after Open Unleashed's FREER AIR ROTATION).
        /// AUTO LEVEL turns the bike back toward level after the sticks are let go, COUNTER-STEER SNAP is the trick hold (a hard counter-steer after a whip
        /// snaps the bike back); in between the helper works that much slower. A save from before the sliders (FREER AIR + KEEP COUNTER-STEER SNAP /
        /// KEEP AUTO-LEVEL, keys OpenReflex.FreerAir / .FreerAirKeepSnap / .FreerAirKeepLevel) reads as: FREER AIR on -> each helper 0 % unless kept (100 %),
        /// off -> 100 % each; the first change of a slider stores its own key.
        public static float AirAutoLevel
        {
            get => ReflexSave.TryGetFloat("OpenReflex.AirAutoLevel", out float v) ? Mathf.Clamp01(v) : (FreerAir && !FreerAirKeepLevel ? 0f : 1f);
            set => ReflexSave.SetFloat("OpenReflex.AirAutoLevel", Mathf.Clamp01(value));
        }
        public static float AirSnap
        {
            get => ReflexSave.TryGetFloat("OpenReflex.AirSnap", out float v) ? Mathf.Clamp01(v) : (FreerAir && !FreerAirKeepSnap ? 0f : 1f);
            set => ReflexSave.SetFloat("OpenReflex.AirSnap", Mathf.Clamp01(value));
        }
        /// The retired FREER AIR toggle and its two KEEP toggles: read only, for the migration above.
        static bool FreerAir => ReflexSave.GetInt("OpenReflex.FreerAir", 0) != 0;
        static bool FreerAirKeepSnap => ReflexSave.GetInt("OpenReflex.FreerAirKeepSnap", 0) != 0;
        static bool FreerAirKeepLevel => ReflexSave.GetInt("OpenReflex.FreerAirKeepLevel", 0) != 0;
        /// AIR LOOSENESS (OPTIONS > OPEN REFLEX, w-freeair2; core rx_session_set_free_air_looseness): 0 .. 1, how much looser the air is (less coast damping, a
        /// quicker stick lag, a little more stick authority). 0 (the default) = off; it no longer waits for FREER AIR (gone): it works with any AUTO LEVEL /
        /// COUNTER-STEER SNAP. Key OpenReflex.AirLooseness2; a save from before it takes FREER AIR on -> its AirLooseness (default 35 %), off -> 0.
        public const float AirLoosenessLegacyDefault = 0.35f;
        public static float AirLooseness
        {
            get => ReflexSave.TryGetFloat("OpenReflex.AirLooseness2", out float v) ? Mathf.Clamp01(v)
                 : FreerAir ? Mathf.Clamp01(ReflexSave.GetFloat("OpenReflex.AirLooseness", AirLoosenessLegacyDefault)) : 0f;
            set => ReflexSave.SetFloat("OpenReflex.AirLooseness2", Mathf.Clamp01(value));
        }
        /// FREE RIDE WITHOUT BORDERS (OPTIONS > OPEN REFLEX, w-frnowalls; core rx_session_set_free_ride_borders): no invisible borders in Free Ride
        /// (no reset zones off the track, no reset triggers but deep water, no white-out band, the terrain's edge only as a fall net). ON by default (the owner's
        /// ask); OFF brings the original's borders back.
        public static bool FreeRideNoBorders { get => ReflexSave.GetBool("OpenReflex.FreeRideNoBorders", true); set => ReflexSave.SetBool("OpenReflex.FreeRideNoBorders", value); }
        /// ALIVE LOOK (OPTIONS > OPEN REFLEX > FREE RIDE, w-alivereflex experiment; Content/AliveLook.cs): the MX vs ATV Alive levels drawn with Reflex's own
        /// rendering (true, REFLEX) or with the Alive translation (false, ALIVE). Read when a level loads. Default: AliveLook.DefaultReflex.
        public static bool AliveLookReflex { get => ReflexSave.GetBool("OpenReflex.AliveLookReflex", Reflex.Port.Content.AliveLook.DefaultReflex); set => ReflexSave.SetBool("OpenReflex.AliveLookReflex", value); }
        public static int FreeRideBordersMode => FreeRideNoBorders ? Reflex.Port.RxCore.RxApiNative.FreeRideBordersOpen : Reflex.Port.RxCore.RxApiNative.FreeRideBordersOriginal;
        /// SPAWN MARKERS (OPTIONS > OPEN REFLEX, w-spawnmarker; OriginalWorld/OriginalSpawnMarker.cs, core rx_session_spawn_at_instant): hold SET SPAWN MARKER to
        /// keep the rider's place as a marker, press RIDE TO SPAWN MARKER to ride back there at once (Free Ride and PRACTICE). ON by default.
        public static bool SpawnMarkers { get => ReflexSave.GetBool("OpenReflex.SpawnMarkers", true); set => ReflexSave.SetBool("OpenReflex.SpawnMarkers", value); }
        /// (w-markerbtns) The marker's two controls, ours, not columns of the game's controller table, changed under OPTIONS > CONTROLS (0 = unbound): SET (held
        /// OriginalSpawnMarker.HoldSeconds) and RIDE BACK (pressed). Keys are DirectInput scan codes, pad inputs the game's ids 9 .. 24. The defaults are the inputs
        /// of the game's own SetResetPoint / UseResetPoint (its Free Ride "save spot" flag and "back to spot"), which the marker replaces where it works:
        /// pad Y (12) / B (10), keyboard F (0x21) / V (0x2F). The same input on both rows is one control: held = set, tapped = ride back (the old behaviour).
        public const int SpawnMarkerSetKeyDefault = 0x21, SpawnMarkerSetPadDefault = 12, SpawnMarkerRideKeyDefault = 0x2F, SpawnMarkerRidePadDefault = 10;
        /// Before w-markerbtns the marker was one control ("OpenReflex.SpawnMarkerKey" / ".SpawnMarkerPad", hold = set, tap = ride back), default M (0x32) and the
        /// D-pad down (23, the game's own look back). Until a row is changed on the CONTROLS page its value is read from there: the old default moves to the new
        /// defaults; a custom input the marker still accepts stays the one control on both rows; one it no longer accepts (an input the game or the client
        /// uses while riding: SpawnMarkerInput.PadRefusal / KeyRefusal) moves to the new defaults too. Nothing is written until the page changes a row.
        public const int SpawnMarkerKeyOld = 0x32, SpawnMarkerPadOld = 23;
        public static int SpawnMarkerSetKey { get => ReflexSave.TryGetInt("OpenReflex.SpawnMarker.SetKey", out int v) ? v : OldKey(SpawnMarkerSetKeyDefault); set => ReflexSave.SetInt("OpenReflex.SpawnMarker.SetKey", value); }
        public static int SpawnMarkerRideKey { get => ReflexSave.TryGetInt("OpenReflex.SpawnMarker.RideKey", out int v) ? v : OldKey(SpawnMarkerRideKeyDefault); set => ReflexSave.SetInt("OpenReflex.SpawnMarker.RideKey", value); }
        public static int SpawnMarkerSetPad { get => ReflexSave.TryGetInt("OpenReflex.SpawnMarker.SetPad", out int v) ? v : OldPad(SpawnMarkerSetPadDefault); set => ReflexSave.SetInt("OpenReflex.SpawnMarker.SetPad", value); }
        public static int SpawnMarkerRidePad { get => ReflexSave.TryGetInt("OpenReflex.SpawnMarker.RidePad", out int v) ? v : OldPad(SpawnMarkerRidePadDefault); set => ReflexSave.SetInt("OpenReflex.SpawnMarker.RidePad", value); }
        /// RESET TRACK's own control (w-fb-hud; OriginalWorld/ReflexTrackReset.cs, CONTROLS > RESET TRACK): a DirectInput key / a pad input id
        /// (SpawnMarkerInput's), 0 = unbound (the default for both).
        public static int ResetTrackKey { get => ReflexSave.GetInt("OpenReflex.ResetTrack.Key", 0); set => ReflexSave.SetInt("OpenReflex.ResetTrack.Key", value); }
        public static int ResetTrackPad { get => ReflexSave.GetInt("OpenReflex.ResetTrack.Pad", 0); set => ReflexSave.SetInt("OpenReflex.ResetTrack.Pad", value); }
        static int OldKey(int def) =>
            ReflexSave.TryGetInt("OpenReflex.SpawnMarkerKey", out int old) && old != SpawnMarkerKeyOld && (old == 0 || Reflex.Port.OriginalWorld.SpawnMarkerInput.KeyRefusal(old) == null) ? old : def;
        static int OldPad(int def) =>
            ReflexSave.TryGetInt("OpenReflex.SpawnMarkerPad", out int old) && old != SpawnMarkerPadOld && (old == 0 || Reflex.Port.OriginalWorld.SpawnMarkerInput.PadRefusal(old) == null) ? old : def;
        /// PHYSICS STUDIO EVERYWHERE (OPTIONS > OPEN REFLEX): the active physics config also in the career, MotoSkills and the challenges
        /// (practice there: nothing saved). Off by default.
        public static bool PhysicsEverywhere { get => ReflexSave.GetBool("OpenReflex.PhysicsEverywhere", false); set => ReflexSave.SetBool("OpenReflex.PhysicsEverywhere", value); }
        /// PERFORMANCE OVERLAY (OPTIONS > VIDEO; Platform/ReflexPerfOverlay): frame, CPU, GPU and game-frame times in a corner panel for testers.
        public static bool PerfOverlay { get => ReflexSave.GetInt("Options.PerfOverlay", 0) != 0; set => ReflexSave.SetInt("Options.PerfOverlay", value ? 1 : 0); }
        public static readonly int[] RenderScales = { 50, 67, 75, 85, 100, 125, 150, 200 };
        /// RENDER SCALE (percent; 100 = the game's own: the URP asset's render scale). Player builds.
        public static int RenderScale { get => Mathf.Clamp(ReflexSave.GetInt("Options.RenderScale", 100), 25, 200); set => ReflexSave.SetInt("Options.RenderScale", Mathf.Clamp(value, 25, 200)); }

        /// HUD SCALE (percent, 70 .. 150; 100 = the design size): the MODERN HUD's canvas scale (ReflexHudKit.Scale).
        public const int HudScaleMin = 70, HudScaleMax = 150;
        public static int HudScale { get => Mathf.Clamp(ReflexSave.GetInt("Options.HudScale", 100), HudScaleMin, HudScaleMax); set => ReflexSave.SetInt("Options.HudScale", Mathf.Clamp(value, HudScaleMin, HudScaleMax)); }
        /// HUD OPACITY (percent, 40 .. 100): the MODERN HUD's panels (text stays opaque).
        public static int HudOpacity { get => Mathf.Clamp(ReflexSave.GetInt("Options.HudOpacity", 100), 40, 100); set => ReflexSave.SetInt("Options.HudOpacity", Mathf.Clamp(value, 40, 100)); }
        /// STANDINGS BOARD: the MODERN HUD's position + field board (top left) on or off.
        public static bool HudStandings { get => ReflexSave.GetInt("Options.HudStandings", 1) != 0; set => ReflexSave.SetInt("Options.HudStandings", value ? 1 : 0); }
        /// SPEEDOMETER: the MODERN HUD's speed and gear gauge on or off.
        public static bool HudGauge { get => ReflexSave.GetInt("Options.HudGauge", 1) != 0; set => ReflexSave.SetInt("Options.HudGauge", value ? 1 : 0); }
        /// REV GAUGE (w-revgauge): what the MODERN gauge's ring shows (and the gauge-only supplement over the CLASSIC HUD): 0 RPM the engine's
        /// rev band with the shift light (the default; saves without the key read RPM), 1 SPEED the speed ring (0 .. 90 mph, the ring before
        /// w-rpmgauge), 2 OFF no ring (the gear and the speed number stay).
        public const int RevGaugeRpm = 0, RevGaugeSpeed = 1, RevGaugeOff = 2;
        public static readonly string[] RevGaugeNames = { "RPM", "SPEED", "OFF" };
        public static int RevGauge { get => Mathf.Clamp(ReflexSave.GetInt("Options.HudRevGauge", RevGaugeRpm), 0, 2); set => ReflexSave.SetInt("Options.HudRevGauge", Mathf.Clamp(value, 0, 2)); }
        /// CAMERA NAME (OPTIONS > CAMERA & HUD): the camera's name shows for a moment on every switch (the MODERN HUD's toast, ReflexCameraToast with the
        /// CLASSIC HUD or a hidden one). The cameras themselves: ReflexCameraPresets.
        public static bool CameraToast { get => ReflexSave.GetInt("Hud.CameraToast", 1) != 0; set => ReflexSave.SetInt("Hud.CameraToast", value ? 1 : 0); }
        /// UI SCALE (percent, 80 .. 130; 100 = the design size): every menu page (OuUi.Panel / OptUi.Panel reference resolution).
        public const int UiScaleMin = 80, UiScaleMax = 130;
        public static int UiScale { get => Mathf.Clamp(ReflexSave.GetInt("Options.UiScale", 100), UiScaleMin, UiScaleMax); set => ReflexSave.SetInt("Options.UiScale", Mathf.Clamp(value, UiScaleMin, UiScaleMax)); }
        /// CONTROLLER GLYPHS: the button hints' style (0 AUTO from the last device used, 1 KEYBOARD, 2 XBOX, 3 PLAYSTATION).
        public static readonly string[] GlyphStyleNames = { "AUTO", "KEYBOARD", "XBOX", "PLAYSTATION" };
        public static int GlyphStyle { get => Mathf.Clamp(ReflexSave.GetInt("Options.GlyphStyle", 0), 0, 3); set => ReflexSave.SetInt("Options.GlyphStyle", Mathf.Clamp(value, 0, 3)); }
        /// REDUCE MOTION: menu transitions and the HUD's flourishes are skipped.
        public static bool ReduceMotion { get => ReflexSave.GetInt("Options.ReduceMotion", 0) != 0; set => ReflexSave.SetInt("Options.ReduceMotion", value ? 1 : 0); }

        /// YOUR MUSIC (OPTIONS > AUDIO, Content/ReflexCustomMusic.cs; the player's .mp3 / .ogg / .wav in Mods/Music, Open Unleashed's YOUR MUSIC): 0 OFF (the game's
        /// soundtrack only), 1 MIX IN (default: the songs are played among the game's tracks in the races; nothing changes while the folder is empty), 2 ONLY (they
        /// replace the game's soundtrack in the menus and the races; the game's soundtrack plays while the folder has no songs).
        public static int CustomMusic { get => Mathf.Clamp(ReflexSave.GetInt("Options.CustomMusic", 1), 0, 2); set => ReflexSave.SetInt("Options.CustomMusic", Mathf.Clamp(value, 0, 2)); }
        /// NOW PLAYING POP-UP (OPTIONS > AUDIO, Hud/ReflexNowPlaying.cs): the title and artist of a song slide in when it starts (the game's tracks too). ON by default.
        public static bool NowPlayingPopup { get => ReflexSave.GetInt("Options.NowPlaying", 1) != 0; set => ReflexSave.SetInt("Options.NowPlaying", value ? 1 : 0); }
        /// MASTER VOLUME (0 .. 1): every sound (AudioListener.volume), over the game's own EFFECTS / MUSIC / ENGINE volumes.
        public static float MasterVolume { get => Mathf.Clamp01(ReflexSave.GetFloat("Options.MasterVolume", 1f)); set => ReflexSave.SetFloat("Options.MasterVolume", Mathf.Clamp01(value)); }
        /// CROWD VOLUME (0 .. 1): the crowd channel over EFFECTS (the game has no crowd row of its own: its crowd follows EffectsVolume).
        /// REVERB (OPTIONS > AUDIO): the original's system reverb on the 3D sounds (the level's ambient and zones); ON = the original's. Read by OriginalReverb.
        public static bool AudioReverb { get => ReflexSave.GetBool("OpenReflex.AudioReverb", true); set => ReflexSave.SetBool("OpenReflex.AudioReverb", value); }
        /// FIRST-PERSON REVERB: the short, dark reverb the helmet camera adds while the first-person view is on; ON = the original's.
        public static bool AudioReverbFirstPerson { get => ReflexSave.GetBool("OpenReflex.AudioReverbFirstPerson", true); set => ReflexSave.SetBool("OpenReflex.AudioReverbFirstPerson", value); }
        public static float CrowdVolume { get => Mathf.Clamp01(ReflexSave.GetFloat("Options.CrowdVolume", 1f)); set => ReflexSave.SetFloat("Options.CrowdVolume", Mathf.Clamp01(value)); }
        /// MOUSE LEAN: the mouse leans the rider (the game's Raw Input path, scaled by MOUSE SENSITIVITY); off = the mouse is not sent to the game.
        public static bool MouseLean { get => ReflexSave.GetInt("Options.MouseLean", 1) != 0; set => ReflexSave.SetInt("Options.MouseLean", value ? 1 : 0); }
        /// LANGUAGE of the game's texts (the install's string tables): "" = the install's own (UIRegistry ForceLanguage, else ENG); else a
        /// language code the install has (ENG, GER, FRE, SPA, ITA). Applies at the next start.
        public static string Language { get => ReflexSave.GetString("Options.Language", ""); set => ReflexSave.SetString("Options.Language", value ?? ""); }

        /// H: the HUD hidden for this session (both styles; Open Unleashed's HudHidden).
        public static bool HudHidden;

        // ------------------------------------------------------------------------------------------- the Dynamic FPV camera
        // w-fpvrevert: the Open Unleashed Dynamic FPV camera again (OriginalWorld/OriginalOnboardCamera.cs), its settings the established ReflexFpv.*
        // keys (old keys: PlayerPrefs + save.json, ReflexSave), read by OriginalOnboardCamera.Load (again on every change of one: ReflexSave.Changed).
        // Defaults and ranges: OriginalOnboardCamera's constants (the 7a9c99ff camera's). headBob / headSmooth / viewSmooth are w-fpvrevert's.
        // The rewrite (w-fpvrewrite .. w-fpvfix, 2026-11-03) saved one JSON object "Fpv.Config" (FpvConfig v1 / v2) instead: MigrateFpvConfig takes
        // what translates from it once.
        const string Fpv = "ReflexFpv.";
        static float FpvFloat(string key, float fallback, float min, float max) { float v = ReflexSave.GetFloat(Fpv + key, fallback); return float.IsNaN(v) || float.IsInfinity(v) ? fallback : Mathf.Clamp(v, min, max); }
        static void SetFpvFloat(string key, float value, float min, float max) => ReflexSave.SetFloat(Fpv + key, Mathf.Clamp(value, min, max));
        public static int OnboardHeadMode { get => Mathf.Clamp(ReflexSave.GetInt(Fpv + "mode", 1), 0, 2); set => ReflexSave.SetInt(Fpv + "mode", Mathf.Clamp(value, 0, 2)); }
        public static float OnboardFov { get => FpvFloat("fov", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultFov, 40, 140); set => SetFpvFloat("fov", value, 40, 140); }
        public static float OnboardTilt { get => FpvFloat("tilt", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultTilt, 0, 1); set => SetFpvFloat("tilt", value, 0, 1); }
        public static float OnboardPitch { get => FpvFloat("pitch", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultPitch, -1, 1); set => SetFpvFloat("pitch", value, -1, 1); }
        public static int OnboardCornerAnticipation { get => Mathf.Clamp(ReflexSave.GetInt(Fpv + "ca", 0), 0, 2); set => ReflexSave.SetInt(Fpv + "ca", Mathf.Clamp(value, 0, 2)); }
        public static float OnboardCornerScale { get => FpvFloat("caScale", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultCaScale, 0, 1); set => SetFpvFloat("caScale", value, 0, 1); }
        public static float OnboardLeanHeading { get => FpvFloat("leanHead", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultLeanHeadingScale, 0, 60); set => SetFpvFloat("leanHead", value, 0, 60); }
        public static bool OnboardFreeLook { get => ReflexSave.GetInt(Fpv + "free", 0) != 0; set => ReflexSave.SetInt(Fpv + "free", value ? 1 : 0); }
        public static bool OnboardHeadShift { get => ReflexSave.GetInt(Fpv + "shift", 0) != 0; set => ReflexSave.SetInt(Fpv + "shift", value ? 1 : 0); }
        public static bool OnboardLeanPitch { get => ReflexSave.GetInt(Fpv + "leanPitch", 1) != 0; set => ReflexSave.SetInt(Fpv + "leanPitch", value ? 1 : 0); }
        public static float OnboardTrueSmoothing { get => FpvFloat("trueSmooth", .35f, 0, 1); set => SetFpvFloat("trueSmooth", value, 0, 1); }
        public static float OnboardTrueRoll { get => FpvFloat("trueRoll", 1, 0, 1); set => SetFpvFloat("trueRoll", value, 0, 1); }
        public static float OnboardTrueYawSmoothing { get => FpvFloat("trueYaw", 0, 0, 1); set => SetFpvFloat("trueYaw", value, 0, 1); }
        public static float OnboardTruePitchSmoothing { get => FpvFloat("truePitch", 0, 0, 1); set => SetFpvFloat("truePitch", value, 0, 1); }
        /// HEAD BOB (w-fpvrevert): how much of the rider's animated head movement about the riding posture the eye takes (1 = the old camera, 0 = none).
        public static float OnboardHeadBob { get => FpvFloat("headBob", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultHeadBob, 0, 1); set => SetFpvFloat("headBob", value, 0, 1); }
        /// HEAD SMOOTHING (w-fpvrevert): the eye spring's time constant, s (the old camera's headSmoothing).
        public static float OnboardHeadSmoothing { get => FpvFloat("headSmooth", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultHeadSmoothing, 0, 0.25f); set => SetFpvFloat("headSmooth", value, 0, 0.25f); }
        /// VIEW SMOOTHING (w-fpvrevert): a critically damped spring on the view's pitch and roll, s (0 = the old camera's: none).
        public static float OnboardViewSmoothing { get => FpvFloat("viewSmooth", Reflex.Port.OriginalWorld.OriginalOnboardCamera.DefaultViewSmoothing, 0, 0.25f); set => SetFpvFloat("viewSmooth", value, 0, 0.25f); }
        public const float OnboardOffsetMax = 2f;
        public static Vector3 OnboardOffset { get => new Vector3(FpvFloat("offX", 0, -2, 2), FpvFloat("offY", 0, -2, 2), FpvFloat("offZ", 0, -2, 2)); set => SetOnboardOffset(value, true); }
        public static void SetOnboardOffset(Vector3 value, bool save)
        {
            SetFpvFloat("offX", value.x, -2, 2); SetFpvFloat("offY", value.y, -2, 2); SetFpvFloat("offZ", value.z, -2, 2);
            if (save) Save();
        }
        public static float OnboardShiftLong { get => FpvFloat("shiftLong", FpvFloat("shiftStrength", 1, 0, 3), 0, 3); set => SetFpvFloat("shiftLong", value, 0, 3); }
        public static float OnboardShiftLat { get => FpvFloat("shiftLat", FpvFloat("shiftStrength", 1, 0, 3), 0, 3); set => SetFpvFloat("shiftLat", value, 0, 3); }
        public static float OnboardShiftVert { get => FpvFloat("shiftVert", FpvFloat("shiftStrength", 1, 0, 3), 0, 3); set => SetFpvFloat("shiftVert", value, 0, 3); }
        public static readonly string[] OnboardKeys = { "mode", "fov", "tilt", "pitch", "ca", "caScale", "leanHead", "free", "shift", "shiftStrength", "leanPitch", "offX", "offY", "offZ", "trueSmooth", "trueRoll", "trueYaw", "truePitch", "shiftLong", "shiftLat", "shiftVert", "headBob", "headSmooth", "viewSmooth" };
        /// Every first-person setting back to its default (the keys deleted: a running camera reads the defaults on the next frame).
        public static void ResetOnboard() { foreach (string key in OnboardKeys) ReflexSave.Delete(Fpv + key); Save(); }

        // ---- the rewrite's settings (Fpv.Config), taken once
        // A FpvConfig (v1 / v2) JSON object: only the fields that translate to this camera (the rest of the head-on-a-neck model has no counterpart).
        // Their defaults are the rewrite's (a value still at its default was not the player's choice and is not taken).
        [Serializable]
        sealed class RewriteFpv
        {
            public int headMode = 1; public float fov = 116f, lookDown = 20f, horizonLock = 0.7f, trueSmoothing = 0.35f; public bool freeLook; public Vector3 eyeOffset;
        }
        public const string RewriteFpvKey = "Fpv.Config", RewriteFpvTakenKey = "Fpv.ConfigTaken";
        static bool s_rewriteChecked;
        /// The rewrite's look down (deg below the head's level) as this camera's PITCH (look down = pitch x 20 + 15 deg).
        public static float PitchFromLookDown(float lookDownDeg) => Mathf.Clamp((lookDownDeg - 15f) / 20f, -1f, 1f);
        /// The rewrite's HORIZON LOCK (1 = level) as this camera's TILT (the horizon's roll with the bike's lean, 0 = level).
        public static float TiltFromHorizonLock(float horizonLock) => Mathf.Clamp01(1f - horizonLock);
        /// Once per save (marked "Fpv.ConfigTaken"): the rewrite's Fpv.Config values the player changed from its defaults become the ReflexFpv.* keys
        /// they translate to (HEAD MODE, FIELD OF VIEW, LOOK DOWN -> PITCH, HORIZON LOCK -> TILT, FREE LOOK, the eye offset, TRUE SMOOTHING). A
        /// rewrite value is newer than an old key (the rewrite had copied FOV, HEAD MODE, FREE LOOK and the offset from them), so it wins; a value left
        /// at the rewrite's default leaves the old key (or this camera's default) alone. Fpv.Config itself is left in the save.
        public static void MigrateFpvConfig()
        {
            if (s_rewriteChecked) return;
            s_rewriteChecked = true;
            if (ReflexSave.GetInt(RewriteFpvTakenKey, 0) != 0) return;
            string json = ReflexSave.GetString(RewriteFpvKey, "");
            if (string.IsNullOrEmpty(json)) return;
            var c = new RewriteFpv(); var d = new RewriteFpv();
            try { JsonUtility.FromJsonOverwrite(json, c); } catch (Exception) { return; }
            var taken = new System.Collections.Generic.List<string>();
            bool Diff(float a, float b) => !float.IsNaN(a) && !float.IsInfinity(a) && Mathf.Abs(a - b) > 1e-4f;
            if (c.headMode != d.headMode) { OnboardHeadMode = c.headMode; taken.Add("mode " + OnboardHeadMode); }
            if (Diff(c.fov, d.fov)) { OnboardFov = c.fov; taken.Add("fov " + OnboardFov); }
            if (Diff(c.lookDown, d.lookDown)) { OnboardPitch = PitchFromLookDown(c.lookDown); taken.Add("pitch " + OnboardPitch.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture)); }
            if (Diff(c.horizonLock, d.horizonLock)) { OnboardTilt = TiltFromHorizonLock(c.horizonLock); taken.Add("tilt " + OnboardTilt.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture)); }
            if (c.freeLook != d.freeLook) { OnboardFreeLook = c.freeLook; taken.Add("free look"); }
            if (Diff(c.eyeOffset.x, 0) || Diff(c.eyeOffset.y, 0) || Diff(c.eyeOffset.z, 0)) { SetOnboardOffset(c.eyeOffset, false); taken.Add("eye offset"); }
            if (Diff(c.trueSmoothing, d.trueSmoothing)) { OnboardTrueSmoothing = c.trueSmoothing; taken.Add("true smoothing"); }
            ReflexSave.SetInt(RewriteFpvTakenKey, 1);
            Debug.Log("[FPV] the rewrite's first-person settings (Fpv.Config) taken: " + (taken.Count > 0 ? string.Join(", ", taken) : "none changed from its defaults"));
            Save();
        }

        // ------------------------------------------------------------------------------------------- units
        public static float Speed(float mph) => Metric ? mph * 1.609344f : mph;
        public static string SpeedUnit => Metric ? "KM/H" : "MPH";

        public static void Save() => ReflexSave.Save();
    }

    /// What other parts of the client plug into the options pages (they own these; a null hook shows the row disabled).
    public static class ReflexOptionsHooks
    {
        /// SYSTEM > GAME FOLDER: opens the shell's Game Library (the folder setting is the shell's: ReflexInstall).
        public static Action OpenGameLibrary;
        /// SYSTEM > ABOUT > CREDITS: the shell's credits screen (null: the row is disabled, in a race).
        public static Action OpenCredits;
        /// OPTIONS > AUDIO > ENGINE SOUNDS: rows another part adds to the audio tab (the garage's per-bike engine sound packs); null = no section.
        public static Action<UnityEngine.UIElements.VisualElement> AudioExtras;
        /// SYSTEM > SUPPORT > CREATE REPORT: the platform's support report; returns the file made (null: failed / nothing).
        public static Func<string> CreateSupportReport;
        /// SYSTEM > SUPPORT > OPEN LOG FOLDER (the platform's logs); null: the player's log folder.
        public static Action OpenLogFolder;
        /// The front end's music volume changed (the shell / the original front end apply it to their music player).
        public static Action MusicVolumeChanged;
    }
}
