using System;
using System.Collections.Generic;
using System.IO;
using Reflex.Port.Content;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    // The garage's per-bike ENGINE SOUND choice (w-audio): "ORIGINAL" (the game's own engine events) or any MX Bikes engine pack found
    // (Content/EngineSoundPacks.cs: the shipped ones and the Mods folders). Persisted per vehicle model in the client save
    // (save.json "EngineSound.Model.<VehicleModelId>" = the pack id, "" = original); a model without its own choice follows the
    // OPTIONS > AUDIO > ENGINE SOUNDS default. The race plays the choice for the local rider's bike (OriginalVehicleSound).
    //
    // A self-contained component so any garage can host it:
    //   EngineSoundPicker.Row(parent, vehicleModelId, label, name)   one controller-navigable spinner row (OuUi.Spinner) for one bike
    //   EngineSoundPicker.Section(parent, bikes)                     a titled section with one row per (model id, title)
    //   EngineSoundPicker.OptionsRows(panel)                          the OPTIONS > AUDIO > ENGINE SOUNDS rows (ReflexOptionsHooks.AudioExtras)
    //   EngineSoundPicker.Changed                                     raised after a choice is stored (model id, pack id)
    public static class EngineSoundPicker
    {
        public static event Action<int, string> Changed;

        static string Label(string id) => EngineSoundPacks.Title(id);

        static void Step(List<string> choices, string current, int d, Action<string> set)
        {
            int i = choices.IndexOf(current); if (i < 0) i = 0;
            set(choices[((i + d) % choices.Count + choices.Count) % choices.Count]);
        }

        /// One spinner row for a bike (vehicle model id).
        public static VisualElement Row(VisualElement parent, int vehicleModelId, string label = "ENGINE SOUND", string name = null)
        {
            var row = OuUi.Spinner(label, () =>
            {
                string id = EngineSoundPacks.ChoiceFor(vehicleModelId);
                return Label(id) + (EngineSoundPacks.HasOwnChoice(vehicleModelId) ? "" : " · DEFAULT");
            }, d =>
            {
                var choices = EngineSoundPacks.Choices();
                Step(choices, EngineSoundPacks.ChoiceFor(vehicleModelId), d, id => { EngineSoundPacks.SetChoice(vehicleModelId, id); Changed?.Invoke(vehicleModelId, id); });
            }, parent);
            row.name = name ?? "engine-sound-" + vehicleModelId;
            return row;
        }

        /// "ENGINE SOUND" and one row per bike.
        public static void Section(VisualElement parent, IEnumerable<(int model, string title)> bikes)
        {
            OuTheme.Section(parent, "ENGINE SOUND");
            var seen = new HashSet<int>();
            foreach (var (model, title) in bikes) if (model >= 0 && seen.Add(model)) Row(parent, model, title);
            int packs = EngineSoundPacks.All.Count;
            OuUi.Text(packs == 0 ? "No engine sound packs found. Drop MX Bikes engine packs into Mods/EngineSounds."
                : "ORIGINAL is the game's own engine. " + packs + " MX Bikes engine pack" + (packs == 1 ? "" : "s") + " installed (more go in Mods/EngineSounds).", "mxa-note", parent);
        }

        // ---- OPTIONS > AUDIO > ENGINE SOUNDS (the options page's hook; its owner leaves it null) ----
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void InstallOptionsHook() { if (ReflexOptionsHooks.AudioExtras == null) ReflexOptionsHooks.AudioExtras = OptionsRows; }

        public static void OptionsRows(VisualElement panel)
        {
            OptUi.Spinner("DEFAULT ENGINE SOUND", () => Label(EngineSoundPacks.DefaultChoice), d =>
                Step(EngineSoundPacks.Choices(), EngineSoundPacks.DefaultChoice, d, id => EngineSoundPacks.DefaultChoice = id), panel).name = "engine-sound-default";
            OptUi.Text("The engine every bike plays unless the garage gives it its own (GARAGE > VEHICLE > ENGINE SOUND). Ours.", "mxa-note", panel);
            OptUi.Button("RESCAN ENGINE PACKS", "mxa-main-button", () => { EngineSoundPacks.Refresh(); _ = EngineSoundPacks.All; }, panel).name = "engine-sound-rescan";
            string user = EngineSoundPacks.UserFolder;
            OptUi.Text("Drop MX Bikes engine packs (a folder with engine.scl, sfx.cfg and the WAVs) into " + user + " or a Mods/EngineSounds folder next to the game. Ours.", "mxa-note", panel);
            OptUi.Button("OPEN ENGINE PACKS FOLDER", "mxa-main-button", () =>
            {
                try { Directory.CreateDirectory(user); Application.OpenURL(new Uri(user).AbsoluteUri); } catch (Exception e) { Debug.LogWarning("[EngineSounds] " + e.Message); }
            }, panel).name = "engine-sound-folder";
        }
    }
}
