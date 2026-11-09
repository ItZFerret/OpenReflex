// MATERIALS: ENHANCED and MUD BUILDUP (OPTIONS > VIDEO; not part of the original renderer): the options, the test build's F11 A/B key and the shader globals
// of Shaders/ReflexVehiclePBR.hlsl.
//   MATERIALS   ORIGINAL / ENHANCED (default ENHANCED in this test build): bikes, ATVs, UTVs and riders shaded as physically based surfaces by class
//               (Content/ReflexMaterialClasses.cs); only with LIGHTING: ENHANCED (ORIGINAL lighting always draws the original programs, exactly).
//   MUD BUILDUP OFF / ON (default ON): whether the vehicles and riders get muddy at all, in either look. ON with the enhanced materials: the
//               new dirt (Content/ReflexDirt.cs), which replaces the original's own mud term; ON without them (LIGHTING or MATERIALS: ORIGINAL):
//               the original's own mud (ReflexMud: the game's mud record through the programs' usr_mud_* constants), exactly as the game.
//               OFF: neither - the original's term gets a zero amount and the new dirt none (w-mudoff: OFF used to fall back to the original's
//               mud, and did nothing at all with LIGHTING: ORIGINAL, the default).
//   F11         test build: toggles both at once (the A/B of the remastered look); a label shows the state. Not saved (OPTIONS > VIDEO is).
using System;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.Rendering
{
    public sealed class ReflexEnhancedMaterials : MonoBehaviour
    {
        /// -1: the options decide; 0 / 1: F11 forces both OFF / ON (tests: REFLEX_MATERIALS=original|enhanced).
        public static int AbOverride = -1;
        static readonly int MatOnId = Shader.PropertyToID("_RfxMatOn"), DirtOnId = Shader.PropertyToID("_RfxDirtOn");

        /// OPTIONS > VIDEO > MATERIALS: 0 ORIGINAL, 1 ENHANCED (default).
        public static int MaterialsSetting { get => Mathf.Clamp(ReflexSave.GetInt("Options.Materials", 1), 0, 1); set { ReflexSave.SetInt("Options.Materials", Mathf.Clamp(value, 0, 1)); AbOverride = -1; } }
        /// OPTIONS > VIDEO > MUD BUILDUP: 0 OFF, 1 ON (default).
        public static int MudSetting { get => Mathf.Clamp(ReflexSave.GetInt("Options.MudBuildup", 1), 0, 1); set { ReflexSave.SetInt("Options.MudBuildup", Mathf.Clamp(value, 0, 1)); AbOverride = -1; } }

        /// The enhanced materials are in effect: the option (or F11) and LIGHTING: ENHANCED.
        public static bool MaterialsOn => (AbOverride >= 0 ? AbOverride == 1 : MaterialsSetting == 1) && LightingOn;
        /// MUD BUILDUP: the vehicles and riders get muddy at all (the option; the F11 A/B shows either look with its mud).
        public static bool MudWanted => AbOverride >= 0 || MudSetting == 1;
        /// The new dirt is in effect: MUD BUILDUP and the enhanced materials.
        public static bool MudOn => MaterialsOn && MudWanted;
        /// The original's own mud term is shown: MUD BUILDUP without the enhanced materials (the original look keeps the game's mud).
        public static bool OriginalMudOn => MudWanted && !MudOn;
        static bool LightingOn { get { try { return ReflexEnhancedLighting.Requested; } catch (Exception) { return false; } } }

        /// OPTIONS > VIDEO > MATERIALS as the menu shows it: what is in effect (ENHANCED needs LIGHTING: ENHANCED; with LIGHTING: ORIGINAL the
        /// row reads ORIGINAL whatever is stored, since that is what is drawn).
        public static bool MaterialsShown => MaterialsSetting == 1 && LightingOn;
        /// The menu's pick of MATERIALS: ENHANCED needs LIGHTING: ENHANCED, so picking it switches LIGHTING to ENHANCED too (w-mudoff: before, the
        /// pick was stored and nothing changed on screen with LIGHTING: ORIGINAL, the default). Saved at once.
        public static void PickMaterials(bool enhanced)
        {
            MaterialsSetting = enhanced ? 1 : 0;
            if (enhanced && ReflexSettings.Lighting != 1) ReflexSettings.Lighting = 1;
            ReflexSave.Save();
        }
        /// The menu's MUD BUILDUP pick (applies at once in either look). Saved at once.
        public static void PickMud(bool on) { MudSetting = on ? 1 : 0; ReflexSave.Save(); }

        static ReflexEnhancedMaterials instance;
        string label; float labelUntil;
        GUIStyle style;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_MATERIALS");
            if (string.Equals(v, "enhanced", StringComparison.OrdinalIgnoreCase)) AbOverride = 1;
            else if (string.Equals(v, "original", StringComparison.OrdinalIgnoreCase)) AbOverride = 0;
            Shader.SetGlobalFloat(MatOnId, 0); Shader.SetGlobalFloat(DirtOnId, 0);
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (instance) return;
            var go = new GameObject("Reflex enhanced materials") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            instance = go.AddComponent<ReflexEnhancedMaterials>();
        }

        /// Shows the state label for a few seconds (the F11 key; tests).
        public static void ShowLabel() { if (instance) instance.Show(); }

        void Show()
        {
            bool m = MaterialsOn, d = MudOn;
            string state = m ? (d ? "ENHANCED  +  MUD BUILDUP" : "ENHANCED  (MUD OFF)") : MudWanted ? "ORIGINAL" : "ORIGINAL  (MUD OFF)";
            label = "MATERIALS: " + state + (!LightingOn && (AbOverride >= 0 ? AbOverride == 1 : MaterialsSetting == 1) ? "   (needs LIGHTING: ENHANCED)" : "") + "    [CTRL+F11]";
            labelUntil = Time.unscaledTime + 3f;
        }

        void Update()
        {
            var kb = Keyboard.current;
            if (kb != null && kb.ctrlKey.isPressed && kb.f11Key.wasPressedThisFrame)
            {
                bool on = AbOverride >= 0 ? AbOverride == 1 : MaterialsSetting == 1 && MudSetting == 1;
                AbOverride = on ? 0 : 1;
                Show();
            }
        }

        void LateUpdate()
        {
            Shader.SetGlobalFloat(MatOnId, MaterialsOn ? 1f : 0f);
            Shader.SetGlobalFloat(DirtOnId, MudOn ? 1f : 0f);
            Content.ReflexDirt.CheckReplace();
        }

        void OnGUI()
        {
            if (label == null || Time.unscaledTime > labelUntil) return;
            style ??= new GUIStyle(GUI.skin.label) { fontSize = 26, fontStyle = FontStyle.Bold, alignment = TextAnchor.UpperCenter };
            var r = new Rect(0, Screen.height * 0.06f, Screen.width, 50);
            style.normal.textColor = new Color(0, 0, 0, 0.85f);
            GUI.Label(new Rect(r.x + 2, r.y + 2, r.width, r.height), label, style);
            style.normal.textColor = MaterialsOn ? new Color(1f, 0.86f, 0.4f) : new Color(0.85f, 0.85f, 0.85f);
            GUI.Label(r, label, style);
        }
    }
}
