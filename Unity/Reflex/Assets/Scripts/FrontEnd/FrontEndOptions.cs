// OptionsFrontEnd.lua (page "Options Page" of FrontEndPageGraph.pag: Background01.lyr, OptionsFrontEnd.lyr, ButtonLegend.lyr,
// Header.lyr): the NavList of settings rows (UIFocusContainer: Background, Title, a one-slot UIDynamicList "List", Left/Right
// arrows) under the DISPLAY / AUDIO / GAME / MISC headers; Up/Down move (UpDownDirectional), Left/Right change the focused
// row's list (LeftRightDirectional / DirectionalFail at the ends), each change is applied at once (App.SetUISetting), Back
// travels back to the main menu. The list data (values and labels) is the Lua's.
//
// Storage: the same as the in-race pause page's PauseOptions (OriginalPauseMenu) and the race runtime, so a value set here is
// what the race starts with and what the pause page shows:
//   DrivingCamera     ReflexUI.DrivingCamera (0..3) + ReflexCameraMode (OriginalPauseMenu.CameraModeOf, read by OriginalWorldRuntime)
//   HUDVisibility     ReflexOriginalHud (OriginalWorldRuntime.OriginalHudVisible: 1 = ON)
//   OnlinePlayerName  ReflexUI.OnlinePlayerName (0 ON / 1 OFF)
//   EffectsVolume     ReflexAudio.master (0..1; game sound + the front end's UI sounds)
//   MusicVolume       ReflexAudio.music (0..1; the front-end music plays at it at once)
//   EngineVolume      ReflexUI.EngineVolume (0..10, OriginalPauseMenu.EngineVolume)
// The settings the pause page does not have use its ReflexUI.<SettingName> scheme with the Lua's raw value (Brightness 45..55,
// Gamma 0/1, MeasurementSystem 0/1, CareerDifficulty 80..120, Vibration 1/0, MouseSensitivity 0..100). Brightness and Gamma drive
// the game's gamma ramp (ReflexScreenAdjustment below: shader globals for the final pass); the others are stored only, the port has
// no consumer for them yet. VIEW CREDITS opens the Credits Page (Credits.bik, FrontEndVideo.cs); CHEAT ENTRY the cheat entry popup
// (FrontEndPopups.cs: the game checks the code). CONTROLLER OPTIONS opens the Controller Remap Page (GotoControllerRemapPage:
// ControllerRemapping.lua, FrontEndRemap.cs). The graph's other controller page, Options Controller Page (OptionsControllerPreset.lua:
// the pad presets / handedness), is unreachable on PC: no script and nothing in MXReflex.exe sends GotoOptionsControllerPage.
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public sealed class OptionsFrontEndScript : FeScript
    {
        sealed class Setting
        {
            public string container, setting; public (int value, string label)[] data; public Func<int> read; public Action<int> write;
            public FeNode list, left, right;
        }
        readonly List<Setting> settings = new List<Setting>();

        static (int, string)[] Percent(int step = 10) { var l = new List<(int, string)>(); for (int v = 0; v <= 100; v += step) l.Add((v, v + "%")); return l.ToArray(); }

        // --------------------------------------------------------------------------------------- the shared settings storage
        public static int GetSetting(string name)
        {
            switch (name)
            {
                case "DrivingCamera":
                {   // OriginalPauseMenu.DrivingCameraIndex without a runtime: the saved index if it still matches the camera mode
                    int mode = PlayerPrefs.GetInt("ReflexCameraMode", 0), saved = PlayerPrefs.GetInt("ReflexUI.DrivingCamera", -1);
                    var map = OriginalPauseMenu.CameraModeOf;
                    if (saved >= 0 && saved < map.Length && map[saved] == mode) return saved;
                    for (int i = 0; i < map.Length; i++) if (map[i] == mode) return i;
                    return 0;
                }
                case "HUDVisibility": return PlayerPrefs.GetInt("ReflexOriginalHud", 1) != 0 ? 0 : 1;
                case "OnlinePlayerName": return PlayerPrefs.GetInt("ReflexUI.OnlinePlayerName", 0);
                case "EffectsVolume": return Mathf.Clamp(Mathf.RoundToInt(PlayerPrefs.GetFloat("ReflexAudio.master", 1f) * 10f), 0, 10) * 10;
                case "MusicVolume": return Mathf.Clamp(Mathf.RoundToInt(PlayerPrefs.GetFloat("ReflexAudio.music", 0.6f) * 10f), 0, 10) * 10;
                case "EngineVolume": return Mathf.Clamp(PlayerPrefs.GetInt("ReflexUI.EngineVolume", 10), 0, 10) * 10;
                case "Brightness": return PlayerPrefs.GetInt("ReflexUI.Brightness", 50);
                case "Gamma": return PlayerPrefs.GetInt("ReflexUI.Gamma", 0);
                case "MeasurementSystem": return PlayerPrefs.GetInt("ReflexUI.MeasurementSystem", 0);
                case "CareerDifficulty": return PlayerPrefs.GetInt("ReflexUI.CareerDifficulty", 95);
                case "Vibration": return PlayerPrefs.GetInt("ReflexUI.Vibration", 0);   // the PC game's default: off
                case "MouseSensitivity": return PlayerPrefs.GetInt("ReflexUI.MouseSensitivity", 50);
            }
            return 0;
        }

        /// App.SetUISetting(name, value): written to the shared storage at once (the original applies it at once and saves the
        /// profile when the page ends).
        public static void SetSetting(string name, int v, FrontEndManager fe = null)
        {
            switch (name)
            {
                case "DrivingCamera": PlayerPrefs.SetInt("ReflexUI.DrivingCamera", v); PlayerPrefs.SetInt("ReflexCameraMode", OriginalPauseMenu.CameraModeOf[Mathf.Clamp(v, 0, 3)]); break;
                case "HUDVisibility": PlayerPrefs.SetInt("ReflexOriginalHud", v == 0 ? 1 : 0); break;
                case "OnlinePlayerName": PlayerPrefs.SetInt("ReflexUI.OnlinePlayerName", v); break;
                case "EffectsVolume": PlayerPrefs.SetFloat("ReflexAudio.master", v / 100f); if (fe != null && fe.sounds != null) fe.sounds.volume = v / 100f; break;
                case "MusicVolume": PlayerPrefs.SetFloat("ReflexAudio.music", v / 100f); fe?.ApplyMusicVolume(); break;
                case "EngineVolume": PlayerPrefs.SetInt("ReflexUI.EngineVolume", v / 10); break;
                case "Brightness": case "Gamma": PlayerPrefs.SetInt("ReflexUI." + name, v); PlayerPrefs.Save(); ReflexScreenAdjustment.Apply(); return;
                default: PlayerPrefs.SetInt("ReflexUI." + name, v); break;
            }
            PlayerPrefs.Save();
        }

        // --------------------------------------------------------------------------------------- page
        public override void Initialize()
        {
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); };
            functions["OnControllerSettingsKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoControllerRemapPage"); };   // the Controller Remap Page (FrontEndRemap.cs)
            functions["OnCreditsKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent("GotoCredits"); };   // the Credits Page (Credits.bik, FrontEndVideo.cs)
            functions["OnCheatEntryKeypress"] = () => fe.SendUIEvent("ShowCheatEntryPopup");     // CheatEntryPopup / CheatPopup (FrontEndPopups.cs)
            functions["OnAcceptStorageQuestionButtonOne"] = () => { };
            void Add(string container, string setting, params (int, string)[] data)
            {
                var c = C(container); if (c == null) return;
                var s = new Setting { container = container, setting = setting, data = data, list = c.Kid("List"), left = c.Kid("LeftArrow"), right = c.Kid("RightArrow") };
                if (s.list == null) return;
                s.list.onListChanged = (l, old) => OnSettingsListFocusedChanged(s);
                settings.Add(s);
            }
            Add("DrivingCameraContainer", "DrivingCamera", (0, "STR_OPT_DC_1"), (1, "STR_OPT_DC_2"), (2, "STR_OPT_DC_3"), (3, "STR_OPT_DC_4"));
            Add("BrightnessContainer", "Brightness", (45, "0%"), (46, "10%"), (47, "20%"), (48, "30%"), (49, "40%"), (50, "50%"), (51, "60%"), (52, "70%"), (53, "80%"), (54, "90%"), (55, "100%"));
            Add("GammaContainer", "Gamma", (0, "STR_OPT_GAMMA_CRT"), (1, "STR_OPT_GAMMA_LCD"));
            Add("HUDVisibilityContainer", "HUDVisibility", (0, "STR_OPT_ON"), (1, "STR_OPT_OFF"));
            Add("OnlinePlayerNameContainer", "OnlinePlayerName", (0, "STR_OPT_ON"), (1, "STR_OPT_OFF"));
            Add("MeasurementSystemContainer", "MeasurementSystem", (0, "STR_OPT_MS_US"), (1, "STR_OPT_MS_METRIC"));
            Add("EffectsVolumeContainer", "EffectsVolume", Percent());
            Add("MusicVolumeContainer", "MusicVolume", Percent());
            Add("EngineVolumeContainer", "EngineVolume", Percent());
            Add("CareerDifficultyContainer", "CareerDifficulty", (80, "STR_AI_LEVEL1"), (95, "STR_AI_LEVEL2"), (105, "STR_AI_LEVEL3"), (120, "STR_AI_LEVEL4"));
            Add("VibrationContainer", "Vibration", (1, "STR_OPT_ON"), (0, "STR_OPT_OFF"));
            Add("MouseSensitivityContainer", "MouseSensitivity", Percent());
            // the Xenon-only storage device row stays hidden on the PC (it is not in this layer's NavList)
        }

        public override void Start()
        {
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("toggle", false, "STR_BACK");
            fe.SetHeader("STR_OPTIONS");
            PopulateSettings();
        }

        /// PopulateSettings: each list gets its data and the index of the current setting (index 0 when the value is not in the data).
        void PopulateSettings()
        {
            foreach (var s in settings)
            {
                int v = GetSetting(s.setting), index = 0;
                for (int i = 0; i < s.data.Length; i++) if (s.data[i].value == v) { index = i; break; }
                s.list.numItems = s.data.Length; s.list.absIndex = index;
                Format(s); UpdateArrows(s);
            }
        }

        void Format(Setting s)
        {   // OnFormatSettingsList: STR_ labels by key, the others as dynamic strings
            string label = s.data[Mathf.Clamp(s.list.absIndex, 0, s.data.Length - 1)].label;
            if (s.list.kids.Count > 0) s.list.kids[0].SetText(label.StartsWith("STR_", StringComparison.Ordinal) ? Str(label) : label);
        }

        void OnSettingsListFocusedChanged(Setting s)
        {
            SetSetting(s.setting, s.data[s.list.absIndex].value, fe);
            Format(s); UpdateArrows(s);
        }

        static void UpdateArrows(Setting s)
        {   // OptionsFrontEnd:UpdateArrows
            int i = s.list.absIndex, n = s.list.numItems;
            if (s.left == null || s.right == null) return;
            if (i == 0) { s.left.Visible = false; s.right.Visible = true; }
            else if (i == n - 1) { s.left.Visible = true; s.right.Visible = false; }
            else { s.left.Visible = true; s.right.Visible = true; }
        }

        // bench access
        public int Value(string setting) { foreach (var s in settings) if (s.setting == setting) return s.data[s.list.absIndex].value; return -1; }
        public int Focused => C("NavList")?.focusIndex ?? -1;
    }
}

namespace Reflex.Port.FrontEnd
{
    // The game's screen adjustment (Brightness / Gamma UI settings) as its renderer applies it: the settings code (005b9af0) sets the
    // renderer's gamma midpoint +0x680 from Gamma (0 CRT -> 0x8f = 143, 1 LCD -> 0x96 = 150) and its brightness +0x684 = Brightness
    // * 0.01 (45..55 -> 0.45..0.55; the contrast +0x688 stays 1.0, 007ad1f7), then builds the ramp (007aff40):
    //   e = ln(mid / 255) / ln(0.5); for i in 0..255: x = clamp01(i / 255 * contrast + (2 * brightness - 1)), k = trunc(x * 255),
    //   ramp[i] = trunc(pow(k / 255, e) * 65535), the same for red, green and blue (D3DGAMMARAMP at +0x7c);
    // and IDirect3DDevice9::SetGammaRamp( 0, 0, ramp ) at the next Present (007af53b, dirty flag +0x67c). (D3D9 applies a gamma ramp
    // in full-screen mode.) The port publishes it for the final render pass (the render pipeline's): the shader globals
    // _ReflexGammaRamp (256 x 1, R = ramp[i] / 65535, linear) and _ReflexGammaParams (e, 2 * brightness - 1, contrast, 1); Changed
    // fires on every change. Applied at start and whenever the OPTIONS page or the motocard's Screen Adjustment page changes it.
    public static class ReflexScreenAdjustment
    {
        public static int Brightness => Mathf.Clamp(PlayerPrefs.GetInt("ReflexUI.Brightness", 50), 45, 55);
        public static int Gamma => PlayerPrefs.GetInt("ReflexUI.Gamma", 0) != 0 ? 1 : 0;
        public const float Contrast = 1.0f;
        public static int Midpoint => Gamma == 0 ? 0x8f : 0x96;
        public static float Exponent => (float)(Math.Log(Midpoint / 255.0) / Math.Log(0.5));
        public static float Offset => 2f * (Brightness * 0.01f) - 1f;
        public static event Action Changed;
        static Texture2D ramp;

        /// ramp[i] / 65535 of the game's table (i 0..255).
        public static float[] Ramp()
        {
            var o = new float[256]; double e = Math.Log(Midpoint / 255.0) / Math.Log(0.5); float off = (float)(2.0 * (Brightness * 0.01f) - 1.0);
            for (int i = 0; i < 256; i++)
            {
                float x = i * (1f / 255f) * Contrast + off; x = x >= 1f ? 1f : x <= 0f ? 0f : x;
                int k = (int)(x * 255f);
                o[i] = (int)(Math.Pow(k * (1.0 / 255.0), e) * 65535.0) / 65535f;
            }
            return o;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        public static void Apply()
        {
            var r = Ramp();
            if (!ramp)
            {
                var fmt = SystemInfo.SupportsTextureFormat(TextureFormat.RFloat) ? TextureFormat.RFloat : TextureFormat.RGBA32;
                ramp = new Texture2D(256, 1, fmt, false, true) { name = "_ReflexGammaRamp", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            }
            var px = new Color[256]; for (int i = 0; i < 256; i++) px[i] = new Color(r[i], r[i], r[i], 1f);
            ramp.SetPixels(px); ramp.Apply(false, false);
            Shader.SetGlobalTexture("_ReflexGammaRamp", ramp);
            Shader.SetGlobalVector("_ReflexGammaParams", new Vector4(Exponent, Offset, Contrast, 1f));
            Changed?.Invoke();
        }
    }
}
