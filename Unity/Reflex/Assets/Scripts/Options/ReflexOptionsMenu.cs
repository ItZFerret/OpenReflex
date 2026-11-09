// The options pages, modelled on Open Unleashed's MxaOptionsMenu (Scripts/UI/MxaOptionsMenu*.cs, _documentation/options-menu.md):
// eight tabs, value rows (spinners, sliders, toggles) in a scrolling panel, the focused row's help in one line under the page
// (CollapseNotes), Q / E the tabs, Esc back.
//   CONTROLS      RUMBLE (the game's Vibration) + RUMBLE STRENGTH (the game's motors scaled: ReflexRumbleScale), MOUSE SENSITIVITY
//                 (the game's), CONTROLLER REMAPPING (the game's own ControllerRemapping table through the profile session:
//                 ReflexRemap, as the original front end's Controller Remap Page)        ReflexOptionsMenu.Controls.cs
//   AUDIO         EFFECTS, MUSIC (on / volume), ENGINE: the game's own volumes; YOUR MUSIC, NOW PLAYING POP-UP (ReflexOptionsMenu.Music.cs)
//   VIDEO         DISPLAY: resolution, display mode, vsync, frame rate cap; QUALITY: quality, render scale, anti-aliasing,
//                 textures, shadows, motion blur; SCREEN: brightness, gamma (graphics.cfg = ReflexLaunchSettings for the game's own)
//   GAMEPLAY      UNITS, CAREER DIFFICULTY, NATIVE GAME (OFF / NATIVE / VERIFY)
//   CAMERA & HUD  CHASE CAMERA (the camera in use, the camera mode, CHASE CAMERA FOV), CAMERA CYCLE and MY CAMERAS (the player's own presets
//                 and their editor: ReflexOptionsMenu.Cameras.cs), FIRST-PERSON VIEW (the Dynamic FPV camera: OriginalOnboardCamera), HUD (HUD STYLE
//                 MODERN / ORIGINAL, HUD on / off, CAMERA NAME, CLIENT INFO)
//   INTERFACE     MENUS (UI SCALE, CONTROLLER GLYPHS, REDUCE MOTION), LANGUAGE of the game's texts      ReflexOptionsMenu.Interface.cs
//   SYSTEM        GAME FOLDER (the shell's Game Library), SAVE DATA (backup / restore / open folder), SUPPORT (the platform's
//                 report)                                                                ReflexOptionsMenu.System.cs
//   OPEN REFLEX   the settings of OPEN REFLEX itself that change the game: RIDING (FREER AIR and its two finer choices)
//                                                                                        ReflexOptionsMenu.OpenReflex.cs
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        public static readonly string[] OptionTabs = { "CONTROLS", "AUDIO", "VIDEO", "GAMEPLAY", "CAMERA & HUD", "INTERFACE", "SYSTEM", "OPEN REFLEX" };
        static readonly CultureInfo Ci = CultureInfo.InvariantCulture;

        readonly MonoBehaviour _host;
        readonly VisualElement _layer, _hints;
        readonly OptNav _nav;
        readonly Action _back;
        int _tab;
        public int Tab => _tab;
        Dictionary<string, string> _strings;

        public ReflexOptionsMenu(MonoBehaviour host, VisualElement layer, VisualElement hints, OptNav nav, Action back)
        { _host = host; _layer = layer; _hints = hints; _nav = nav; _back = back; }

        /// The game's own text for a key (the front end's string table; fallback when the install has none).
        string Str(string key, string fallback)
        {
            if (_strings == null)
            {
                _strings = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var (pkg, table) in new[] { ("FrontEnd", "MXRaven_Default_Strings"), ("InGame", "MXRaven_Default_Strings") })
                    try { foreach (var kv in OriginalUiAssets.Data.Strings(pkg, table)) if (!_strings.ContainsKey(kv.Key)) _strings[kv.Key] = kv.Value; }
                    catch (Exception) { }
            }
            return _strings.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v.ToUpperInvariant() : fallback;
        }

        /// Called every frame while open (the CONTROLS page's listening and loading).
        public void Tick() { TickControls(); }

        public void Closing() { CloseControls(); EndCameraEditor(); }

        public void ShowOptions(int tab)
        {
            if (_tab == 0 && tab != 0) CloseControls();
            EndCameraEditor();
            _tab = (tab % OptionTabs.Length + OptionTabs.Length) % OptionTabs.Length;
            _layer.Clear();
            _layer.AddToClassList("mxa-dim");
            _layer.AddToClassList("mxa-options-open");
            var head = OptUi.Div("mxa-head", _layer);
            OptUi.Text(Str("STR_OPTIONS", "OPTIONS"), "mxa-screen-title", head);
            var tabs = OptUi.Div("mxa-tabs", head);
            for (int i = 0; i < OptionTabs.Length; i++)
            {
                int ii = i;
                var t = OptUi.Button(OptionTabs[i], "mxa-tab", () => ShowOptions(ii), tabs);
                t.EnableInClassList("mxa-tab-on", i == _tab);
                t.focusable = false;
                t.name = "tab-" + i;
            }
            var body = OptUi.Div("ou-opt-body", _layer);
            var page = OptUi.Div("mxa-panel mxa-options-page", body);
            OptUi.Text(OptionTabs[_tab], "ou-bar-title ou-opt-page-bar ou-bar", page).enableRichText = false;   // the career pages' orange title bar: the page's name
            var side = OptUi.Div("mxa-panel ou-opt-side", body);
            var sideTag = OptUi.Text("", "ou-pill ou-label", side);
            var sideTitle = OptUi.Text("", "ou-opt-side-title ou-display", side);
            var sideHelp = OptUi.Text("", "ou-opt-side-help", side);
            var sv = new ScrollView(ScrollViewMode.Vertical);
            sv.AddToClassList("mxa-options-scroll");
            sv.verticalScrollerVisibility = ScrollerVisibility.Auto;
            sv.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            page.Add(sv);
            VisualElement panel = sv.contentContainer;
            VisualElement first = null;
            switch (_tab)
            {
                case 0: first = BuildControls(panel); break;
                case 1: first = BuildAudio(panel); break;
                case 2: first = BuildVideo(panel); break;
                case 3: first = BuildGameplay(panel); break;
                case 4: first = BuildCameraHud(panel); break;
                case 5: first = BuildInterface(panel); break;
                case 6: first = BuildSystem(panel); break;
                case 7: first = BuildOpenReflex(panel); break;
            }
            CollapseNotes(panel, sideHelp, sideTitle, sideTag);
            _nav.OnTab = d => ShowOptions(_tab + d);
            _nav.OnCancel = () => { if (ControlsListening) return false; _back(); return true; };
            _nav.Suspended = () => ControlsListening;
            var f0 = first;
            _nav.DefaultItem = () => f0;
            OptUi.Hints(_hints, (OptUi.KeySubmit, Str("STR_SELECT", "SELECT")), (OptUi.KeyCancel, Str("STR_BACK", "BACK")), (OptUi.KeyTabs, "PAGE"));
            if (first != null) _nav.Focus(first);
        }

        static void Section(VisualElement parent, string title) => OptUi.Text(title, "mxa-panel-title mxa-options-section", parent);
        static void Note(VisualElement parent, string text) => OptUi.Text(text, "mxa-note", parent);
        static Label Keep(VisualElement parent, string text) { var l = OptUi.Text(text, "mxa-note mxa-keep", parent); l.enableRichText = false; return l; }
        static string Pct(float v) => Mathf.RoundToInt(v * 100f) + "%";
        static T Named<T>(T e, string name) where T : VisualElement { e.name = name; return e; }

        /// The notes under the rows become the help of those rows (shown for the focused row in the side card: its name, whether it is the
        /// game's own setting or ours, and the note).
        static void CollapseNotes(VisualElement page, Label help, Label title, Label tag)
        {
            var helpOf = new Dictionary<VisualElement, string>();
            void Walk(VisualElement parent)
            {
                var pending = new List<VisualElement>();
                foreach (var c in parent.Children().ToList())
                {
                    if (c.ClassListContains("mxa-keep")) continue;
                    if (c.ClassListContains("mxa-note"))
                    {
                        if (pending.Count == 0 || !(c is Label l)) continue;
                        foreach (var item in pending) helpOf[item] = helpOf.TryGetValue(item, out var t) ? t + " " + l.text : l.text;
                        c.style.display = DisplayStyle.None;
                        pending.Clear();
                        continue;
                    }
                    if (c.ClassListContains("mxa-panel-title")) { pending.Clear(); continue; }
                    if (c.ClassListContains("mxa-item")) { pending.Add(c); continue; }
                    var inner = c.Query<VisualElement>(className: "mxa-item").ToList();
                    if (inner.Count > 0 && !(c is ScrollView)) pending.AddRange(inner);
                    else if (c is ScrollView sv) Walk(sv.contentContainer);
                }
            }
            Walk(page);
            help.enableRichText = false; title.enableRichText = false; tag.enableRichText = false;
            page.RegisterCallback<FocusInEvent>(e =>
            {
                for (var v = e.target as VisualElement; v != null; v = v.parent)
                    if (helpOf.TryGetValue(v, out var t))
                    {
                        bool ours = t.EndsWith(" Ours.", StringComparison.Ordinal);
                        if (ours) t = t.Substring(0, t.Length - 6);
                        help.text = t;
                        var label = v.Q<Label>(className: "mxa-row-label"); title.text = label != null ? label.text : "";
                        tag.text = ours ? "CLIENT OPTION" : t.StartsWith("The game's", StringComparison.Ordinal) || t.Contains("the game's") ? "GAME SETTING" : "OPTION";
                        tag.EnableInClassList("ou-pill-gold", ours);
                        return;
                    }
                help.text = ""; title.text = ""; tag.text = "";
            }, TrickleDown.TrickleDown);
        }

        static string Cycle<T>(T[] values, T current, int d, out T next)
        {
            int i = Array.IndexOf(values, current); if (i < 0) i = 0;
            next = values[((i + d) % values.Length + values.Length) % values.Length];
            return null;
        }

        // ------------------------------------------------------------------------------------------------ AUDIO
        VisualElement BuildAudio(VisualElement panel)
        {
            Section(panel, "VOLUME");
            var first = Named(OptUi.Slider("MASTER VOLUME", () => ReflexSettings.MasterVolume, ReflexClientLive.MasterVolume, panel, 0f, 1f, 0.1f), "master-volume");
            Note(panel, "Every sound at once, over the game's own volumes below. Ours.");
            Named(OptUi.Slider(Str("STR_OPT_EFFECTS_VOLUME", "EFFECTS VOLUME"), () => Mathf.Clamp01(ReflexSettings.EffectsVolume), ReflexClientLive.EffectsVolume, panel, 0f, 1f, 0.1f), "effects-volume");
            Note(panel, "The game's EffectsVolume: crashes, riders, the crowd, the race and menu sounds.");
            Named(OptUi.Slider(Str("STR_OPT_ENGINE_VOLUME", "ENGINE VOLUME"), () => ReflexSettings.EngineVolume / 10f, v => { ReflexSettings.EngineVolume = Mathf.RoundToInt(v * 10f); ReflexSettings.Save(); }, panel, 0f, 1f, 0.1f), "engine-volume");
            Note(panel, "The game's EngineVolume: every bike's and ATV's engine. The same settings as the game's own OPTIONS page and the pause page's.");
            Named(OptUi.Slider("CROWD VOLUME", () => ReflexSettings.CrowdVolume, v => { ReflexSettings.CrowdVolume = v; ReflexSettings.Save(); }, panel, 0f, 1f, 0.1f), "crowd-volume");
            Note(panel, "The crowd's share of the effects volume (the game has no crowd row of its own: its crowd follows EFFECTS VOLUME). Ours.");
            Section(panel, "MUSIC");
            Named(OptUi.Toggle(Str("STR_OPT_MUSIC", "MUSIC"), () => ReflexSettings.MusicOn, ReflexClientLive.MusicOn, panel), "music-on");
            Named(OptUi.Slider(Str("STR_OPT_MUSIC_VOLUME", "MUSIC VOLUME"), () => ReflexSettings.MusicVolume, ReflexClientLive.MusicVolume, panel, 0f, 1f, 0.1f), "music-volume");
            Note(panel, "The game's soundtrack in the menus and the races (M toggles it in a race).");
            MusicRows(panel);
            Section(panel, "ACOUSTICS");
            Named(OptUi.Toggle("REVERB", () => ReflexSettings.AudioReverb, v => { ReflexSettings.AudioReverb = v; ReflexSettings.Save(); }, panel), "audio-reverb");
            Note(panel, "The original's echo on the 3D sounds: the stadium and arena halls, the caves, pipes and warehouses of some tracks. OFF plays them dry. Applies at once.");
            Named(OptUi.Toggle("FIRST-PERSON REVERB", () => ReflexSettings.AudioReverbFirstPerson, v => { ReflexSettings.AudioReverbFirstPerson = v; ReflexSettings.Save(); }, panel), "audio-reverb-first-person");
            Note(panel, "The original's short, muffled helmet reverb while the first-person camera is your view. OFF keeps only the track's own reverb. Applies at once.");
            if (ReflexOptionsHooks.AudioExtras != null)
            {
                Section(panel, "ENGINE SOUNDS");
                try { ReflexOptionsHooks.AudioExtras(panel); } catch (Exception e) { Debug.LogWarning("[Options] audio extras: " + e.Message); }
            }
            return first;
        }

        // ------------------------------------------------------------------------------------------------ VIDEO
        static readonly string[] PresetNames = { "CUSTOM", "AUTO", "PERFORMANCE", "BALANCED", "QUALITY" };
        /// PRESET (OPTIONS > VIDEO): 0 AUTO (by ReflexEnhancedLighting.AutoTier), 1 PERFORMANCE, 2 BALANCED, 3 QUALITY; applied at once and saved.
        static void ApplyPreset(int preset, Action<Action<ReflexLaunchSettings>, bool, bool> saveCfg)
        {
            int kind = preset == 0 ? 3 - Rendering.ReflexEnhancedLighting.AutoTier : preset;   // AUTO: HIGH tier -> QUALITY ... LOW -> PERFORMANCE
            ReflexSave.SetInt("Options.Preset", preset);
            ReflexSettings.DrawDistance = kind == 1 ? 0 : 1;   // PERFORMANCE: the original distances; BALANCED / QUALITY: HIGH (w-rmlod)
            if (kind == 1) { ReflexSettings.Lighting = 0; saveCfg(s => s.antialiasing = 0, false, true); }
            else if (kind == 2) { ReflexSettings.Lighting = 1; ReflexSettings.EnhancedQuality = 1; saveCfg(s => s.antialiasing = 0, false, true); }
            else { ReflexSettings.Lighting = 1; ReflexSettings.EnhancedQuality = preset == 0 ? -1 : 2; }
            ReflexSettings.Save();
        }

        VisualElement BuildVideo(VisualElement panel)
        {
            var ls = ReflexLaunchSettings.Current;
            void SaveCfg(Action<ReflexLaunchSettings> set, bool applyNow, bool applyGraphics = false)
            {
                var s = ReflexLaunchSettings.Current; set(s);
                try { s.Save(); } catch (Exception e) { Debug.LogWarning("[Options] graphics.cfg not written: " + e.Message); }
                if (applyNow) ReflexClientLive.ApplyDisplay();
                if (applyGraphics) ReflexClientLive.ApplyGraphicsLive();
            }
            string later = ReflexClientLive.CanApplyDisplay ? "" : " (Stored here; the built game applies it.)";
            Section(panel, "DISPLAY");
            var res = Screen.resolutions.Select(r => new Vector2Int(r.width, r.height)).Distinct().OrderBy(r => r.x * 10000 + r.y).ToList();
            foreach (var std in new[] { new Vector2Int(1280, 720), new Vector2Int(1600, 900), new Vector2Int(1920, 1080) }) if (!res.Contains(std)) res.Add(std);
            res = res.OrderBy(r => r.x * 10000 + r.y).ToList();
            var first = Named(OptUi.Spinner("RESOLUTION", () => { var c = ReflexLaunchSettings.Current; return $"{c.width} × {c.height}"; }, d =>
            {
                var c = ReflexLaunchSettings.Current; var cur = new Vector2Int(c.width, c.height);
                int i = res.IndexOf(cur); if (i < 0) i = res.FindLastIndex(r => r.x * 10000 + r.y <= cur.x * 10000 + cur.y); if (i < 0) i = 0;
                var n = res[((i + d) % res.Count + res.Count) % res.Count];
                SaveCfg(s => { s.width = n.x; s.height = n.y; }, true);
            }, panel), "resolution");
            Note(panel, "The size the game draws at (graphics.cfg ScreenWidth / ScreenHeight; with ASPECT RATIO ORIGINAL 16:9 and WINDOWED, the largest 16:9 picture inside it, as the game does)." + later);
            Named(OptUi.Toggle("DISPLAY MODE", () => ReflexLaunchSettings.Current.fullscreen, v => SaveCfg(s => s.fullscreen = v, true), panel, "FULLSCREEN", "WINDOWED"), "display-mode");
            Note(panel, "FULLSCREEN uses the whole screen; WINDOWED runs it in a window." + later);
            Named(OptUi.Toggle("ASPECT RATIO", () => ReflexSettings.AspectFill == 1, v =>
            {
                ReflexSettings.AspectFill = v ? 1 : 0; ReflexSettings.Save();
                ReflexClientLive.ApplyDisplay(); Reflex.Port.Rendering.ReflexRenderPipeline.ReapplyViewports();
            }, panel, "FILL SCREEN", "ORIGINAL 16:9"), "aspect-ratio");
            Note(panel, "FILL SCREEN uses the whole screen on ultrawide and other non-16:9 displays: the same height of view, wider at the sides. ORIGINAL 16:9 draws the game's 16:9 picture with black bars, as the original did." + later);
            Named(OptUi.Toggle("TERRAIN DETAIL", () => ReflexSettings.TerrainDetail == 1, v =>
            {
                ReflexSettings.TerrainDetail = v ? 1 : 0; ReflexSettings.Save();
                foreach (var t in Terrain.activeTerrains) if (t) t.heightmapPixelError = ReflexSettings.TerrainPixelError;
            }, panel, "HIGH", "STANDARD"), "terrain-detail");
            Note(panel, "HIGH keeps the terrain at full resolution much further from the camera, like the original's single near grid: the dirt no longer changes look a few metres ahead of the rider. STANDARD draws far less terrain geometry (faster on slow GPUs).");
            double hz = Screen.currentResolution.refreshRateRatio.value;
            string monitor = hz > 1 ? $" ({hz:0} Hz)" : "";
            Named(OptUi.Toggle("VSYNC", () => ReflexLaunchSettings.Current.vsync, v => SaveCfg(s => s.vsync = v, true), panel), "vsync");
            Note(panel, $"Waits for your monitor's refresh{monitor} when FRAME RATE CAP is OFF. A frame cap temporarily disables VSync; this preference is kept." + later);
            int launchCap = ReflexClientLive.LaunchFrameRateCap;
            var capRow = Named(OptUi.Spinner("FRAME RATE CAP", () => launchCap > 0 ? launchCap + " FPS (LAUNCH)" : ReflexSettings.FpsCap > 0 ? ReflexSettings.FpsCap + " FPS" : ReflexSettings.FpsCap == ReflexSettings.DisplayRate ? "DISPLAY RATE" : "OFF", d =>
            {
                Cycle(ReflexSettings.FpsCaps, ReflexSettings.FpsCap, d, out int n);
                ReflexSettings.FpsCap = n; ReflexSettings.Save(); ReflexClientLive.ApplyFrameRate();
            }, panel), "frame-rate-cap");
            capRow.SetEnabled(launchCap == 0);
            Note(panel, launchCap > 0
                ? $"Launch override: REFLEX_FPS_CAP={launchCap}. Close the game and remove that environment setting to use the saved cap. Your saved choice is kept."
                : "DISPLAY RATE draws one frame per refresh of your monitor (VSync on). A number limits drawn frames immediately, including when VSync is selected. OFF follows your VSync preference.");
            Named(OptUi.Toggle("FRAME PACING", () => ReflexSettings.FramePacing == 1, v => { ReflexSettings.FramePacing = v ? 1 : 0; ReflexSettings.Save(); }, panel, "SMOOTH", "ORIGINAL"), "frame-pacing");
            Note(panel, "ORIGINAL runs the game's own 60 Hz loop with the lowest input delay (on a 120 Hz+ display each game frame is shown for two or three refreshes). SMOOTH blends the game frames to your display's refresh (one game frame later). Applies from the next event.");
            Named(OptUi.Toggle("LOW LATENCY", () => ReflexSettings.LowLatency == 1, v => { ReflexSettings.LowLatency = v ? 1 : 0; ReflexSettings.Save(); ReflexClientLive.ApplyLatency(); }, panel), "low-latency");
            Note(panel, "ON lets the graphics driver hold one finished frame instead of two: up to one refresh less input delay with VSync. The game itself runs the same either way. With VSync, a frame that no longer fits one refresh would halve the frame rate, so the game steps back to two frames by itself while that happens and tries one again when there is room (DirectX 11 / 12 and Vulkan).");
            Section(panel, "QUALITY");
            var qnames = QualitySettings.names;
            Named(OptUi.Spinner("QUALITY", () => ReflexSettings.Quality < 0 || ReflexSettings.Quality >= qnames.Length ? "DEFAULT" : qnames[ReflexSettings.Quality].ToUpperInvariant(), d =>
            {
                int n = qnames.Length + 1, i = Mathf.Clamp(ReflexSettings.Quality, -1, qnames.Length - 1) + 1;
                ReflexSettings.Quality = ((i + d) % n + n) % n - 1; ReflexSettings.Save(); ReflexClientLive.ApplyQuality();
            }, panel), "quality");
            Note(panel, "The client's quality level (DEFAULT: the build's own)." + later);
            Named(OptUi.Spinner("RENDER SCALE", () => ReflexSettings.RenderScale == 100 ? "100%  (GAME)" : ReflexSettings.RenderScale + "%", d =>
            {
                Cycle(ReflexSettings.RenderScales, ReflexSettings.RenderScale, d, out int n);
                ReflexSettings.RenderScale = n; ReflexSettings.Save(); ReflexClientLive.ApplyQuality();
            }, panel), "render-scale");
            Note(panel, "Draws the 3D picture at this share of the screen's resolution (lower = faster; 100% = the game's)." + later);
            string[] aa = { "OFF", "2X", "4X" }, lmh = { "LOW", "MEDIUM", "HIGH" };
            BuildAntiAliasing(panel);   // ANTI-ALIASING (OFF / MSAA 2X / MSAA 4X / TAA), SHARPENING, UPSCALING: ReflexOptionsMenu.AntiAliasing.cs
            Named(OptUi.Spinner("TEXTURE QUALITY", () => lmh[Mathf.Clamp(ReflexLaunchSettings.Current.textureQuality, 0, 2)], d => SaveCfg(s => s.textureQuality = ((s.textureQuality + d) % 3 + 3) % 3, false), panel), "texture-quality");
            Named(OptUi.Spinner("SHADOW QUALITY", () => lmh[Mathf.Clamp(ReflexLaunchSettings.Current.shadowQuality, 0, 2)], d => SaveCfg(s => s.shadowQuality = ((s.shadowQuality + d) % 3 + 3) % 3, false), panel), "shadow-quality");
            Named(OptUi.Toggle("MOTION BLUR", () => ReflexLaunchSettings.Current.motionBlur, v => SaveCfg(s => s.motionBlur = v, false, true), panel), "motion-blur");
            Named(OptUi.Toggle("LIGHTING", () => ReflexSettings.Lighting == 1, v => { ReflexSettings.Lighting = v ? 1 : 0; ReflexSettings.Save(); }, panel, "ENHANCED", "ORIGINAL"), "lighting");
            Note(panel, "The game's own Video Options (MXSettings.exe: graphics.cfg Antialiasing, TextureQuality, ShadowQuality, MotionBlur). ANTI-ALIASING and MOTION BLUR apply at once; the textures and the shadow maps are made when the game starts, so those two apply from the next start. Your choices go into your own copy of graphics.cfg; the install's file is never changed.");
            string[] eq = { "LOW", "MEDIUM", "HIGH" };
            Named(OptUi.Spinner("ENHANCED QUALITY", () => ReflexSettings.EnhancedQuality < 0 ? "AUTO (" + eq[Rendering.ReflexEnhancedLighting.Tier] + ")" : eq[ReflexSettings.EnhancedQuality], d =>
            {
                int n = ((ReflexSettings.EnhancedQuality + 1 + d) % 4 + 4) % 4 - 1;   // AUTO, LOW, MEDIUM, HIGH
                ReflexSettings.EnhancedQuality = n; ReflexSettings.Save();   // applies at once (ReflexEnhancedLighting follows the tier)
            }, panel), "enhanced-quality");
            Note(panel, "LIGHTING: ORIGINAL draws exactly as the original game. ENHANCED (applies at once) adds real-time sun shadows from every object, ambient occlusion and contact shadows, new HDRI skies matched to each track's time of day, sky reflections and modern tone mapping. ENHANCED QUALITY sets its cost: LOW (2 shadow cascades over 70 m, no ambient occlusion), MEDIUM, HIGH; AUTO follows SHADOW QUALITY, at most what your graphics card suits.");
            // (MATERIALS shows what is drawn: ENHANCED needs LIGHTING: ENHANCED, so picking it turns LIGHTING to ENHANCED too; MUD BUILDUP works in either look)
            Named(OptUi.Toggle("MATERIALS", () => Rendering.ReflexEnhancedMaterials.MaterialsShown, v => Rendering.ReflexEnhancedMaterials.PickMaterials(v), panel, "ENHANCED", "ORIGINAL"), "materials");
            Named(OptUi.Toggle("MUD BUILDUP", () => Rendering.ReflexEnhancedMaterials.MudSetting == 1, v => Rendering.ReflexEnhancedMaterials.PickMud(v), panel, "ON", "OFF"), "mud-buildup");
            Note(panel, "Both apply at once (CTRL+F11 flips both in the test build). MATERIALS: ENHANCED shades the bikes, ATVs, UTVs and riders as physically based surfaces: clear-coated plastics, real metal and chrome reflections of the sky, rough rubber, fabric with sheen, a glossy helmet. It needs LIGHTING: ENHANCED, so choosing it switches LIGHTING to ENHANCED as well; with LIGHTING: ORIGINAL the bikes and riders keep the original look. MUD BUILDUP ON: mud collects on the wheels, frame, boots and lower legs as you ride on dirt, more at speed and where the terrain is muddy, and washes off in water; a restart or respawn cleans it (with MATERIALS: ORIGINAL it is the original game's own mud). OFF: the bikes and riders stay clean.");
            string[] dd = { "ORIGINAL", "HIGH", "ULTRA" };
            Named(OptUi.Spinner("DRAW DISTANCE", () => dd[ReflexSettings.DrawDistance], d => { ReflexSettings.DrawDistance = ((ReflexSettings.DrawDistance + d) % 3 + 3) % 3; ReflexSettings.Save(); }, panel), "draw-distance");
            Note(panel, "How far trees, scenery and grass are drawn in detail, and how far shadows reach (applies at once). ORIGINAL is the original game's distances, with its hard level-of-detail switches. HIGH and ULTRA reach 1.7x / 2.5x as far for trees (2x / 3x for shadows), measure distance from the camera so trees do not flip when you only turn, cross-fade the tree and scenery detail levels instead of swapping them, and keep the shadows of distant trees and objects. ULTRA costs the most.");
            // VOLUMETRIC LIGHT (Rendering/ReflexVolumetricLight.cs)
            Named(OptUi.Spinner("SPEED BLUR", () => Rendering.ReflexSpeedBlur.Names[Rendering.ReflexSpeedBlur.Setting], d => Rendering.ReflexSpeedBlur.Setting = ((Rendering.ReflexSpeedBlur.Setting + d) % 3 + 3) % 3, panel), "speed-blur");
            Note(panel, "SPEED BLUR (ENHANCED only): motion blur while riding that grows with your speed (from about 15 mph, full from about 65 mph): the ground, the scenery and the spinning wheels streak while the bike stays sharp. Replays and photo mode keep their own settings. Ours, not the original's.");
            Named(OptUi.Toggle("VOLUMETRIC LIGHT", () => Rendering.ReflexVolumetricLight.Option == 1, v => { Rendering.ReflexVolumetricLight.Option = v ? 1 : 0; ReflexSave.Save(); }, panel), "volumetric-light");
            Named(OptUi.Spinner("VOLUMETRIC QUALITY", () => Rendering.ReflexVolumetricLight.QualityOption < 0 ? "AUTO (" + eq[Rendering.ReflexVolumetricLight.Tier] + ")" : eq[Rendering.ReflexVolumetricLight.QualityOption], d =>
            {
                Rendering.ReflexVolumetricLight.QualityOption = ((Rendering.ReflexVolumetricLight.QualityOption + 1 + d) % 4 + 4) % 4 - 1; ReflexSave.Save();
            }, panel), "volumetric-quality");
            Named(OptUi.Spinner("FOG DENSITY", () => eq[Rendering.ReflexVolumetricLight.DensityOption], d =>
            {
                Rendering.ReflexVolumetricLight.DensityOption = ((Rendering.ReflexVolumetricLight.DensityOption + d) % 3 + 3) % 3; ReflexSave.Save();
            }, panel), "fog-density");
            Note(panel, "VOLUMETRIC LIGHT (ENHANCED only): real light shafts instead of the original's painted beams: sunlight scattered in the air with the real shadows of trees, grandstands and riders, the stadium spotlights as real light cones, a light haze from each track's own fog and a little dust over the ground. OFF brings the original beams back. VOLUMETRIC QUALITY sets its cost (AUTO follows ENHANCED QUALITY); FOG DENSITY how thick the air is. Test build: CTRL+F10 switches between the two during a race.");
            // (WEATHER moved to the RACE / FREE RIDE setup page: it is chosen per ride, OpenShell.Setup.cs / Rendering/ReflexWeather.cs)
            Named(OptUi.Spinner("PRESET", () => PresetNames[Mathf.Clamp(ReflexSave.GetInt("Options.Preset", -1) + 1, 0, PresetNames.Length - 1)], d =>
            {
                int cur = Mathf.Clamp(ReflexSave.GetInt("Options.Preset", -1), -1, PresetNames.Length - 2);
                int next = ((cur + d) % (PresetNames.Length - 1) + (PresetNames.Length - 1)) % (PresetNames.Length - 1);   // AUTO .. QUALITY (not CUSTOM)
                ApplyPreset(next, SaveCfg);
            }, panel), "preset");
            Note(panel, "Sets several options at once. PERFORMANCE: ORIGINAL lighting, no anti-aliasing (the fastest picture; the original game's). BALANCED: ENHANCED lighting at MEDIUM, no anti-aliasing. QUALITY: ENHANCED at HIGH. AUTO: one of them for your graphics card. Any option can be changed afterwards.");
            Section(panel, "PERFORMANCE");
            Named(OptUi.Toggle("PERFORMANCE OVERLAY", () => ReflexSettings.PerfOverlay, v => { ReflexSettings.PerfOverlay = v; ReflexSettings.Save(); Platform.ReflexPerfOverlay.Apply(v); }, panel), "perf-overlay");
            Note(panel, "For testers: a corner panel with the frame rate and frame time, the CPU (main / render thread) and GPU times, the game's own frame rate and simulation cost, the pacing, the draw calls and the lighting mode.");
            Section(panel, "SCREEN");
            Named(OptUi.Slider(Str("STR_OPT_BRIGHTNESS", "BRIGHTNESS"), () => (ReflexSave.GetInt("ReflexUI.Brightness", 50) - 45) / 10f,
                v => { ReflexSave.SetInt("ReflexUI.Brightness", 45 + Mathf.RoundToInt(v * 10f)); ReflexSave.Save(); Reflex.Port.FrontEnd.ReflexScreenAdjustment.Apply(); }, panel, 0f, 1f, 0.1f), "brightness");
            Named(OptUi.Toggle(Str("STR_OPT_GAMMA", "GAMMA"), () => ReflexSave.GetInt("ReflexUI.Gamma", 0) != 0,
                v => { ReflexSave.SetInt("ReflexUI.Gamma", v ? 1 : 0); ReflexSave.Save(); Reflex.Port.FrontEnd.ReflexScreenAdjustment.Apply(); }, panel, Str("STR_OPT_GAMMA_LCD", "LCD"), Str("STR_OPT_GAMMA_CRT", "CRT")), "gamma");
            Note(panel, "The game's screen adjustment (its gamma ramp), as on its own OPTIONS page.");
            Named(OptUi.Button(Str("STR_SCREENADJUSTMENT_TITLE", "SCREEN ADJUSTMENT"), "mxa-link", () => ShowScreenAdjustment(false), panel), "screen-adjustment");
            Note(panel, "The game's Screen Adjustment Page: grey steps to set the gamma and brightness against (the page of its first-run MotoCard setup).");
            Keep(panel, "graphics.cfg: " + (ls.loaded ? ReflexSave.Redact(ls.source) : "none (the game's defaults)"));
            return first;
        }

        // ------------------------------------------------------------------------------------------------ GAMEPLAY
        VisualElement BuildGameplay(VisualElement panel)
        {
            Section(panel, "GAME");
            var first = Named(OptUi.Toggle(Str("STR_OPT_MEASUREMENT_SYSTEM", "UNITS"), () => ReflexSettings.Metric, v => { ReflexSettings.Metric = v; ReflexSettings.Save(); }, panel,
                Str("STR_OPT_MS_METRIC", "METRIC") + "  (KM/H)", Str("STR_OPT_MS_US", "US") + "  (MPH)"), "units");
            Note(panel, "The game's MeasurementSystem: speeds and distances in its pages and the MODERN HUD.");
            string[] diffKeys = { "STR_AI_LEVEL1", "STR_AI_LEVEL2", "STR_AI_LEVEL3", "STR_AI_LEVEL4" }, diffNames = { "ROOKIE", "AMATEUR", "PRO", "ALL TIME" };
            Named(OptUi.Spinner(Str("STR_OPT_CAREER_DIFFICULTY", "CAREER DIFFICULTY"), () =>
            {
                int i = Array.IndexOf(ReflexSettings.CareerDifficulties, ReflexSettings.CareerDifficulty); if (i < 0) i = 0;
                return Str(diffKeys[i], diffNames[i]);
            }, d =>
            {
                Cycle(ReflexSettings.CareerDifficulties, ReflexSettings.CareerDifficulty, d, out int n);
                ReflexSettings.CareerDifficulty = n; ReflexSettings.Save();
            }, panel), "career-difficulty");
            Note(panel, "The game's CareerDifficulty: how fast the AI riders of the MOTOCAREER are. Applies to the next event.");
            Named(OptUi.Toggle(Str("STR_OPT_ONLINE_PLAYER_NAME", "ONLINE PLAYER NAMES"), () => ReflexSettings.OnlinePlayerName, v =>
            {
                ReflexSettings.OnlinePlayerName = v; ReflexClientLive.GameSetting("OnlinePlayerName", v ? 0 : 1); ReflexSettings.Save();
            }, panel), "online-player-name");
            Note(panel, "The game's OnlinePlayerName: whether the names of the other riders show over their bikes in an online race. It has no effect offline.");
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return first;   // the game's functions run natively: nothing to choose
            Section(panel, "NATIVE GAME");
            string[] modes = { "OFF", "NATIVE", "VERIFY" };
            Named(OptUi.Spinner("NATIVE GAME", () =>
            {
                int v = ReflexSettings.NativeGame;
                var rt = ReflexClientLive.Runtime;
                int eff = v >= 0 ? v : rt ? (int)rt.nativeGame : (int)OriginalWorldRuntime.NativeGameMode.Native;
                return modes[Mathf.Clamp(eff, 0, 2)] + (v < 0 ? "  (DEFAULT)" : "");
            }, d =>
            {
                var rt = ReflexClientLive.Runtime;
                int v = ReflexSettings.NativeGame; if (v < 0) v = rt ? (int)rt.nativeGame : 1;
                ReflexSettings.NativeGame = ((v + d) % 3 + 3) % 3; ReflexSettings.Save();
            }, panel), "native-game");
            Note(panel, "The game's functions rewritten in C++: OFF runs the original code everywhere, NATIVE the rewrites instead, VERIFY both (the original's result kept, differences counted in the log). Applies to the next race (F5 restarts this one). REFLEX_NATIVE_GAME overrides it.");
            Named(OptUi.Button("USE THE DEFAULT", "mxa-link", () => { ReflexSettings.NativeGame = -1; ReflexSettings.Save(); ShowOptions(3); }, panel), "native-default");
            Note(panel, "Forgets the choice: the race's own setting (NATIVE).");
            return first;
        }

        // ------------------------------------------------------------------------------------------------ CAMERA & HUD
        VisualElement BuildCameraHud(VisualElement panel)
        {
            Section(panel, "CHASE CAMERA");
            var first = Named(OptUi.Spinner(Str("STR_OPT_DRIVING_CAMERA", "DRIVING CAMERA"), () => CameraEntryName(ReflexCameraPresets.Current), d => ReflexClientLive.CycleCamera(d), panel), "driving-camera");
            Note(panel, "The camera in use, and the one every event starts on. The regular Camera action and F7 go through the cameras that are ON in CAMERA CYCLE below: the game's VISUAL, GAME PLAY, USER and 1ST PERSON, DYNAMIC FPV and your own. Keyboard and controller remaps apply.");
            string[] cams = { "GAME CAMERA", "DYNAMIC FPV", "FOLLOW CAMERA" };
            Named(OptUi.Spinner("CAMERA", () => { var rt = ReflexClientLive.Runtime; return cams[Mathf.Clamp(rt ? rt.cameraMode : ReflexSettings.CameraMode, 0, 2)]; }, d =>
            {
                var rt = ReflexClientLive.Runtime; int m = rt ? rt.cameraMode : ReflexSettings.CameraMode;
                ReflexClientLive.CameraMode(((m + d) % 3 + 3) % 3);
            }, panel), "camera-mode");
            Note(panel, "GAME CAMERA: the game's own (its preset above, or your camera over it). DYNAMIC FPV: the client's first-person view (below). FOLLOW CAMERA: a plain chase view. F7 and the mapped Camera action cycle the regular driving views in a race.");
            Named(OptUi.Spinner("CHASE CAMERA FOV", () =>
            {
                int p = ReflexSettings.ChaseFov;
                return p == 100 ? "100%  (GAME)" : p + "%  (" + (p > 100 ? "+" : "") + (p - 100) + ")";
            }, d => { ReflexSettings.ChaseFov = ReflexSettings.ChaseFov + d; ReflexSettings.Save(); }, panel), "chase-fov");
            Note(panel, "How wide the game's own views see, against the game's own field of view (100%). Your cameras have their own FIELD OF VIEW, and the first-person view has its own below.");

            CameraPresetRows(panel);

            FpvRows(panel);   // FIRST-PERSON VIEW (ReflexOptionsMenu.Fpv.cs; it ends with FpvShareRows: CAMERA SHARING's FIRST-PERSON SETUPS)

            Section(panel, "HUD");
            Named(OptUi.Spinner("HUD STYLE", () => ReflexSettings.HudStyleNames[(int)ReflexSettings.HudStyle], d => { ReflexSettings.HudStyle = ReflexSettings.HudStyle == HudStyle.Modern ? HudStyle.Original : HudStyle.Modern; ReflexSettings.Save(); }, panel), "hud-style");
            Note(panel, "MODERN is the client's HUD in the Open Unleashed style: the position board, the lap and time panels, the speed and gear, and the panels of every event type (Eliminator, Outrun, Section Race, Freestyle judges, Waypoint, Powercross, the challenges). CLASSIC is the game's own HUD. Switches at once. H hides either in a race.");
            Named(OptUi.Toggle(Str("STR_OPT_HUD_VISIBILITY", "HUD VISIBILITY"), () => { var rt = ReflexClientLive.Runtime; return rt ? rt.OriginalHudVisible : ReflexSettings.HudVisible; }, ReflexClientLive.HudVisible, panel), "hud-visible");
            Note(panel, "The game's HUD Visibility setting (both styles follow it).");
            Named(OptUi.Spinner("HUD SCALE", () => ReflexSettings.HudScale + "%", d => { ReflexSettings.HudScale = ReflexSettings.HudScale + d * 5; ReflexSettings.Save(); }, panel), "hud-scale");
            Note(panel, "The size of the MODERN HUD (70% .. 150%). Ours.");
            Named(OptUi.Spinner("HUD OPACITY", () => ReflexSettings.HudOpacity + "%", d => { ReflexSettings.HudOpacity = ReflexSettings.HudOpacity + d * 10; ReflexSettings.Save(); }, panel), "hud-opacity");
            Note(panel, "How solid the MODERN HUD's panels are (40% .. 100%); the numbers stay bright. Ours.");
            Named(OptUi.Toggle("STANDINGS BOARD", () => ReflexSettings.HudStandings, v => { ReflexSettings.HudStandings = v; ReflexSettings.Save(); }, panel), "hud-standings");
            Note(panel, "The position and the field beside it, top left of the MODERN HUD. Ours.");
            Named(OptUi.Toggle("SPEED AND GEAR", () => ReflexSettings.HudGauge, v => { ReflexSettings.HudGauge = v; ReflexSettings.Save(); }, panel), "hud-gauge");
            Note(panel, "The speed and gear gauge, bottom right of the MODERN HUD (the units follow GAMEPLAY > UNITS). Ours.");
            Named(OptUi.Spinner("REV GAUGE", () => ReflexSettings.RevGaugeNames[ReflexSettings.RevGauge], d => { ReflexSettings.RevGauge = ((ReflexSettings.RevGauge + d) % 3 + 3) % 3; ReflexSettings.Save(); }, panel), "hud-rev-gauge");
            Note(panel, "The gauge's ring. RPM: the engine's revs, from 30% of the rev limiter (empty at idle) up to the limiter; the ring turns amber near the top and flashes red on the limiter. SPEED: the speed (up to 90 mph). OFF: no ring, only the gear and the speed. Also applies to the gauge over the CLASSIC HUD. Ours.");
            Named(OptUi.Toggle("MODERN GAUGE IN CLASSIC", () => ReflexSettings.ClassicSpeedometer, value => { ReflexSettings.ClassicSpeedometer = value; ReflexSettings.Save(); }, panel), "classic-speedometer");
            Note(panel, "Adds the modern speed and gear gauge while keeping the classic race display. HUD visibility hides both.");
            Named(OptUi.Toggle("CAMERA NAME", () => ReflexSettings.CameraToast, v => { ReflexSettings.CameraToast = v; ReflexSettings.Save(); }, panel), "camera-toast");
            Note(panel, "The camera's name for a moment when you switch cameras in a race (with either HUD style, and with the HUD hidden). Ours.");
            Named(OptUi.Toggle("CLIENT INFO", () => { var rt = ReflexClientLive.Runtime; return rt ? rt.HudVisible : ReflexSettings.ClientInfo; }, ReflexClientLive.ClientInfo, panel), "client-info");
            Note(panel, "The client's own overlay in a race: status, telemetry, the race line and the keys.");
            return first;
        }
    }
}
