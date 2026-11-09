// TEST ONLY: GARAGE > RIDER PRESETS / BIKE PRESETS (OpenFrontEnd/GaragePresets.cs, OpenShell.GaragePresets.cs). Page (a scratch save,
// REFLEX_SAVE_DIR; muted; UNLOCK EVERYTHING):
//   presets   RIDER: the empty list (pr-rider-empty), SAVE CURRENT AS PRESET opens the keyboard with "RIDER 1" (pr-rider-keyboard), a typed name
//             "MY LOOK" saves it; another helmet and jersey put on, a second preset "RACE DAY"; the list (pr-rider-list: RACE DAY IN USE), the
//             first focused (its look on the stage: pr-rider-preview), its page (pr-rider-page), LOAD puts its helmet and jersey back
//             (pr-rider-loaded), RENAME through the keyboard, DELETE asks first (pr-rider-delete-armed) then removes it;
//             BIKE: a preset of the bike as it is, then other tuning and another exhaust saved as a second one, another bike ridden, the list
//             (pr-bike-list), the first preset's page (pr-bike-page), LOAD rides that bike with its parts and tuning again (pr-bike-loaded);
//             the store read back from the client save keeps them all.
// Log lines "[presets-test] ..." (PASS / FAIL).
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePresets
    {
        const string Tag = "[presets-test] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("presets", Page); }

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        static VisualElement Item(string name) => OpenShell.Instance.Nav.Items().FirstOrDefault(e => e.name == name);
        static IEnumerator Focus(string name, int frames = 60)
        {
            var e = Item(name);
            if (e == null) { Check(false, "no menu entry " + name); yield break; }
            OpenShell.Instance.Nav.Focus(e); yield return UiShowcase.Frames(frames);
        }
        static IEnumerator Submit(string name, int frames = 90)
        {
            var e = Item(name);
            if (e == null) { Check(false, "no menu entry " + name); yield break; }
            OpenShell.Instance.Nav.Focus(e); yield return UiShowcase.Frames(20);
            (e.userData as OuNavItem)?.submit?.Invoke();
            yield return UiShowcase.Frames(frames);
        }
        static object Keyboard() => typeof(OpenShell).GetField("osk", All).GetValue(OpenShell.Instance);
        static string KeyboardText() { var k = Keyboard(); return k == null ? null : (string)k.GetType().GetProperty("Value").GetValue(k); }
        static IEnumerator Type(string text)
        {
            var k = Keyboard(); if (k == null) { Check(false, "the keyboard is not open"); yield break; }
            k.GetType().GetField("value", All).SetValue(k, text);
            k.GetType().GetMethod("Close").Invoke(k, new object[] { true });
            yield return UiShowcase.Frames(90);
        }
        static string Title(string name) => Item(name)?.Q<Label>(className: "ou-gcard-title")?.text ?? "?";
        static string Pills(string name) => string.Join(",", Item(name)?.Query<Label>(className: "ou-pill").ToList().Select(l => l.text) ?? new string[0]);

        /// Another owned piece of a gear type (not the worn one).
        static bool Other(int type)
        {
            var it = OpenGarageData.GearItems(type).Where(i => i.Available && !i.equipped && i.kit == null).Skip(3).FirstOrDefault();
            return it != null && OpenGarageData.EquipGear(it);
        }

        static IEnumerator Page()
        {
            UiShowcaseNav.IsolateDevices();
            AudioListener.volume = 0f; new GameObject("presets-mute").AddComponent<Mute>().hideFlags = HideFlags.DontSave;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            Options.ReflexSave.SetString(GaragePresets.SaveKey, ""); GaragePresets.Reload();
            StageType.GetField("NoAutoTurn").SetValue(null, true);
            Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            var types = OpenGarageData.GearTypes();
            int helmet = types.First(t => t.factory == "Helmet").id, jersey = types.First(t => t.factory == "Jersey").id;

            // ---- RIDER
            shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(150);
            Check(Item("garage-rider-presets") != null, "RIDER has a RIDER PRESETS card: '" + Title("garage-rider-presets") + "'");
            yield return Submit("garage-rider-presets", 150);
            Check(Item("garage-preset-new") != null && !Enumerable.Range(0, 3).Any(i => Item("garage-preset-" + i) != null), "the empty list: only SAVE CURRENT AS PRESET");
            yield return Focus("garage-preset-new");
            yield return UiShowcase.Shot("pr-rider-empty");
            yield return Submit("garage-preset-new", 90);
            Check(KeyboardText() == "RIDER 1", "SAVE opens the keyboard with a name to keep or change: '" + KeyboardText() + "'");
            yield return UiShowcase.Shot("pr-rider-keyboard");
            int helmet1 = OpenGarageData.EquippedGear(helmet), jersey1 = OpenGarageData.EquippedGear(jersey);
            yield return Type("my look");
            Check(GaragePresets.Riders.Count == 1 && GaragePresets.Riders[0].name == "MY LOOK" && GaragePresets.InUse(GaragePresets.Riders[0]), "a typed name saves the worn look as 'MY LOOK' (IN USE)");

            Check(Other(helmet) && Other(jersey), "another helmet and jersey put on");
            ReflexProfile.Open();
            int helmet2 = OpenGarageData.EquippedGear(helmet);
            GaragePresets.SaveRider("RACE DAY");
            shell.ShowGarage(OpenShell.GarageRiderPresets); yield return UiShowcase.Frames(150);
            Check(Title("garage-preset-0") == "MY LOOK" && Title("garage-preset-1") == "RACE DAY" && Pills("garage-preset-1").Contains("IN USE") && !Pills("garage-preset-0").Contains("IN USE"),
                  "the list: MY LOOK, RACE DAY (IN USE: " + Pills("garage-preset-1") + ")");
            yield return Focus("garage-preset-1", 120);
            yield return UiShowcase.Shot("pr-rider-list");
            yield return Focus("garage-preset-0", 160);
            yield return UiShowcase.Shot("pr-rider-preview");
            yield return Submit("garage-preset-0", 150);
            yield return Focus("garage-preset-load", 120);
            yield return UiShowcase.Shot("pr-rider-page");
            yield return Submit("garage-preset-load", 150);
            Check(OpenGarageData.EquippedGear(helmet) == helmet1 && OpenGarageData.EquippedGear(jersey) == jersey1 && helmet1 != helmet2, $"LOAD puts MY LOOK back (helmet {helmet2} -> {OpenGarageData.EquippedGear(helmet)}, jersey -> {OpenGarageData.EquippedGear(jersey)})");
            Check(Title("garage-preset-load") == "LOADED", "the page says LOADED: '" + Title("garage-preset-load") + "'");
            yield return UiShowcase.Shot("pr-rider-loaded");
            yield return Submit("garage-preset-rename", 90);
            yield return Type("Sunday   Best!!");
            Check(GaragePresets.Riders[0].name == "SUNDAY BEST", "RENAME through the keyboard: '" + GaragePresets.Riders[0].name + "'");
            yield return Submit("garage-preset-delete", 40);
            Check(GaragePresets.Riders.Count == 2 && Title("garage-preset-delete") == "PRESS AGAIN TO DELETE", "DELETE asks first ('" + Title("garage-preset-delete") + "')");
            yield return UiShowcase.Shot("pr-rider-delete-armed");
            yield return Submit("garage-preset-delete", 120);
            Check(GaragePresets.Riders.Count == 1 && GaragePresets.Riders[0].name == "RACE DAY", "the second press deletes it (left: " + string.Join(", ", GaragePresets.Riders.Select(p => p.name)) + ")");

            // ---- BIKE
            int pv = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var tuning0 = GaragePresets.CurrentTuning(pv);
            GaragePresets.SaveBike("STOCK " + GaragePresets.BikeName(model));
            var t = tuning0.ToArray(); t[6] = t[6] == 100 ? 0 : 100; t[7] = t[7] == 0 ? 100 : 0;
            ReflexProfile.SetTuning(pv, new ReflexGarage.Tuning { engine1 = t[0], engine2 = t[1], suspension1 = t[2], suspension2 = t[3], tire1 = t[4], tire2 = t[5], braking = t[6], steering = t[7] });
            var exhaust = OpenGarageData.PartTypes(pv).FirstOrDefault(x => x.factory.IndexOf("Exh", StringComparison.OrdinalIgnoreCase) >= 0);
            var other = exhaust.factory == null ? null : OpenGarageData.PartItems(model, pv, exhaust.id).Where(i => i.Available && !i.equipped && i.kit == null).Skip(2).FirstOrDefault();
            bool changed = other != null && OpenGarageData.EquipPart(pv, other, exhaust.tuning);
            ReflexProfile.Open();
            GaragePresets.SaveBike("LOUD PIPE");
            Check(changed && GaragePresets.Bikes.Count == 2 && GaragePresets.InUse(GaragePresets.Bikes[1]) && !GaragePresets.InUse(GaragePresets.Bikes[0]), "two bike presets (the second IN USE, another exhaust: " + other?.title + ")");
            var otherBike = OpenGarageData.Vehicles().FirstOrDefault(v => v.owned && v.id != model && v.playerVehicle >= 0);
            Check(otherBike != null && OpenGarageData.SelectVehicle(otherBike.playerVehicle), "another bike ridden: " + otherBike?.title);
            ReflexProfile.Open();
            shell.ShowGarage(OpenShell.GarageBikePresets); yield return UiShowcase.Frames(150);
            yield return Focus("garage-preset-0", 160);
            yield return UiShowcase.Shot("pr-bike-list");
            yield return Submit("garage-preset-0", 150);
            yield return Focus("garage-preset-load", 120);
            yield return UiShowcase.Shot("pr-bike-page");
            yield return Focus("garage-preset-delete", 80);
            Check(Item("garage-preset-delete")?.parent?.Q(className: "ou-preset-contents")?.childCount > 2, "WHAT'S IN IT lists the bike, its parts and its tuning (" + Item("garage-preset-delete")?.parent?.Q(className: "ou-preset-contents")?.childCount + " lines)");
            yield return UiShowcase.Shot("pr-bike-contents");
            int last = Enumerable.Range(0, 20).LastOrDefault(i => Item("garage-preset-line-" + i) != null);
            yield return Focus("garage-preset-line-" + last, 80);
            Check(OpenShell.Instance.Nav.Focused?.name == "garage-preset-line-" + last, "the pad reaches the last line of WHAT'S IN IT (" + OpenShell.Instance.Nav.Focused?.name + ")");
            yield return UiShowcase.Shot("pr-bike-contents-end");
            yield return Submit("garage-preset-load", 200);
            int pvNow = OpenGarageData.Equipped;
            Check(OpenGarageData.EquippedModel == model && GaragePresets.CurrentTuning(pvNow).SequenceEqual(tuning0) && GaragePresets.InUse(GaragePresets.Bikes[0]),
                  $"LOAD rides {GaragePresets.BikeName(model)} again with its parts and tuning (model {OpenGarageData.EquippedModel}, tuning {string.Join(",", GaragePresets.CurrentTuning(pvNow))})");
            yield return UiShowcase.Shot("pr-bike-loaded");
            yield return Focus("garage-preset-overwrite", 60);

            // the client save keeps them
            GaragePresets.Reload();
            Check(GaragePresets.Riders.Count == 1 && GaragePresets.Bikes.Count == 2 && GaragePresets.Bikes[1].name == "LOUD PIPE", "the client save reads back: " + GaragePresets.Riders.Count + " rider, " + GaragePresets.Bikes.Count + " bike presets");
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            yield return UiShowcase.Frames(10);
        }

        static Type StageType => typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
    }
}
