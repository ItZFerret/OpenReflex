// TEST ONLY: the .rxmod bikes (core/plan/RXMOD.md) in the garage and the race setup. Run with a client root whose Mods/Bikes holds .rxmod files
// (run.py --client-root), a scratch save (MASTER VOLUME 0 seeded by the runner):
//   rxmod-garage   for each .rxmod bike: GARAGE > VEHICLE, its MOD card focused (the card's lines and author logged, the stage's bike shot from three
//                  angles: rx-<model>-<mod>-stage / -side / -front), A on the card (equips the base bike with the mod as its model), then an
//                  event's SETUP page on its vehicle tab (the 3D hero: the player's equipped bike of the setup's type) -> rx-<model>-<mod>-setup.
// Result lines "[ui-showcase] rxmods ..." ; "FAIL rxmods: ..." on a failure. Unlock-everything is on (every bike owned) in the scratch profile only.
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseRxMods
    {
        const string Tag = "[ui-showcase] ";
        const BindingFlags All = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("rxmod-garage", Garage);

        static void Log(string s) => Debug.Log(Tag + "rxmods " + s);
        static void Fail(string s) => Debug.LogError(Tag + "FAIL rxmods: " + s);
        static OpenShell Shell => OpenShell.Instance;

        static IEnumerator Open(int tab, bool unlock)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (CoreGarageProfile.UnlockEverythingEnabled != unlock && !CoreGarageProfile.SetUnlockEverything(unlock)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            var st = typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
            st.GetField("Verbose")?.SetValue(null, true); st.GetField("NoAutoTurn")?.SetValue(null, true);
            Shell.ShowGarage(tab);
            for (int i = 0; i < 120; i++) yield return null;
        }
        static void Angles(float yaw, float pitch)
        {
            var stage = typeof(OpenShell).GetField("garageStage", All).GetValue(Shell);
            stage?.GetType().GetMethod("SetAngles").Invoke(stage, new object[] { yaw, pitch });
        }
        static Gamepad pad;
        static IEnumerator PadTap(GamepadButton b, int frames = 3)
        {
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("rxmods pad");
            InputSystem.QueueStateEvent(pad, new GamepadState(b));
            for (int i = 0; i < frames; i++) yield return null;
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 8; i++) yield return null;
        }
        /// GARAGE > VEHICLE, then MY RIDES > MODDED BIKES (every mod card listed): B up to the classes, A on the MODDED BIKES entry.
        static IEnumerator ModdedBikes()
        {
            yield return Open(OpenShell.GarageVehicle, true);
            if (Shell.Nav.Items().Any(e => e.name != null && e.name.StartsWith("garage-mod-")) && Shell.GarageTitle?.IndexOf("MOD", StringComparison.OrdinalIgnoreCase) >= 0) yield break;
            yield return PadTap(GamepadButton.East); for (int i = 0; i < 20; i++) yield return null;
            var modsEntry = Shell.Nav.Items().FirstOrDefault(e => e.name == "garage-class-mods");
            for (int tries = 0; modsEntry == null && tries < 3; tries++)
            {   // (the first garage open of a run can still be building its classes)
                for (int i = 0; i < 120; i++) yield return null;
                yield return Open(OpenShell.GarageVehicle, true);
                yield return PadTap(GamepadButton.East); for (int i = 0; i < 20; i++) yield return null;
                modsEntry = Shell.Nav.Items().FirstOrDefault(e => e.name == "garage-class-mods");
            }
            if (modsEntry == null) { Fail("MY RIDES has no MODDED BIKES entry (on " + Shell.GarageTitle + ")"); yield break; }
            Shell.Nav.Focus(modsEntry); yield return null; yield return PadTap(GamepadButton.South);
            for (int i = 0; i < 60; i++) yield return null;
            Log("level " + Shell.GarageCrumb + " > " + Shell.GarageTitle + ": " + string.Join(",", Shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-mod-")).Select(e => e.name)));
        }

        static string Short(BikeMod m) => new string(m.name.Where(char.IsLetterOrDigit).ToArray()).ToLowerInvariant();

        static IEnumerator Garage()
        {
            var mods = BikeMods.All.Where(m => m.rx != null).ToList();
            Log("installed .rxmod mods: " + mods.Count + " (" + string.Join(", ", mods.Select(m => m.name + " by " + m.author + ": " + string.Join("+", m.rx.bikes.Select(b => b.vehicleModelId + "=" + b.name?.Full)))) + ")");
            if (mods.Count == 0) { Fail("no .rxmod installed"); yield break; }
            // every bike, and every skin of an installed bike (REFLEX_RXMOD_ONLY=<text>: only the mods whose id has it)
            string only = Environment.GetEnvironmentVariable("REFLEX_RXMOD_ONLY");
            var work = mods.Where(x => string.IsNullOrEmpty(only) || x.id.IndexOf(only, StringComparison.OrdinalIgnoreCase) >= 0)
                           .SelectMany(x => x.rx.bikes.Select(y => (x.id, y.index, false)).Concat(x.rx.skins.Where(y => y.target != null || y.original >= 0).Select(y => (x.id, y.index, true)))).ToList();
            foreach (var (modId, bikeIndex, isSkin) in work)
                {
                    yield return ModdedBikes();
                    // (the garage rescans the mods: this scan's objects)
                    var m = BikeMods.Find(modId);
                    var skin = isSkin ? m?.rx?.skins.FirstOrDefault(y => y.index == bikeIndex) : null;
                    var b = isSkin ? skin?.target : m?.rx?.bikes.FirstOrDefault(y => y.index == bikeIndex);
                    if (b == null && skin?.original < 0) { Fail(modId + " #" + bikeIndex + " is gone after the rescan"); continue; }
                    int model = b?.vehicleModelId ?? skin.original; string tag = "rx-" + model + "-" + Short(m) + (isSkin ? "-skin" : "");   // (an original bike's skin: no .rxmod bike)
                    var list = OpenGarageData.ModsOfModel(model);
                    int k = list.FindIndex(x => x.id == m.id);
                    var card = Shell.Nav.Items().FirstOrDefault(e => e.name == "garage-mod-" + model + "-" + k);
                    if (k < 0 || card == null) { Fail($"{m.name} {b?.name?.Full ?? skin?.Title}: no MOD card for model {model} (mods of the model: {string.Join(", ", list.Select(x => x.name))})"); continue; }
                    string text = string.Join(" | ", card.Query<Label>().ToList().Select(l => l.text).Where(t => !string.IsNullOrWhiteSpace(t)));
                    Log($"{tag}: card {card.name}: {text}");
                    if (text.IndexOf("NFG COLE", StringComparison.OrdinalIgnoreCase) < 0 && text.IndexOf(m.author, StringComparison.OrdinalIgnoreCase) < 0) Fail($"{tag}: the card does not credit {m.author}");
                    if (b?.name != null && text.IndexOf(b.name.model, StringComparison.OrdinalIgnoreCase) < 0) Fail($"{tag}: the card does not say {b.name.model}");
                    Shell.Nav.Focus(card);
                    for (int i = 0; i < 360; i++) yield return null;
                    yield return UiShowcase.Shot(tag + "-stage");
                    Angles(90f, 6f); for (int i = 0; i < 60; i++) yield return null;
                    yield return UiShowcase.Shot(tag + "-side");
                    Angles(-35f, 12f); for (int i = 0; i < 60; i++) yield return null;
                    yield return UiShowcase.Shot(tag + "-front");
                    // A: the base bike equipped with the mod as its model
                    (card.userData as OuNavItem)?.submit?.Invoke();
                    for (int i = 0; i < 200; i++) yield return null;
                    Log($"{tag}: A on the card: choice \"{BikeMods.ChoiceFor(model)}\", equipped model {OpenGarageData.EquippedModel}");
                    if (BikeMods.ChoiceFor(model) != m.id) Fail($"{tag}: A on the card did not store the mod (\"{BikeMods.ChoiceFor(model)}\")");
                    var spec = OpenGarageData.BikeOfModel(model);
                    Log($"{tag}: the player's bike of model {model}: {spec?.name} rx {(spec?.rx != null ? spec.rx.name?.Full + " (" + spec.rx.mod.name + ")" : "none")} skin {spec?.rxSkin?.Title ?? "-"} tokens {spec?.rx?.Token} {spec?.rxSkin?.Token}");
                    if (spec?.rx?.Key != b?.Key) Fail($"{tag}: the player's bike is not the .rxmod bike");
                    if (isSkin && spec?.rxSkin?.Key != skin.Key) Fail($"{tag}: the player's bike is not painted by the skin");
                    if (isSkin && skin.name.Length > 0 && text.IndexOf(skin.name, StringComparison.OrdinalIgnoreCase) < 0) Fail($"{tag}: the card does not name the skin {skin.name}");
                    yield return Setup(tag, model);
                }
            Shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// A National's SETUP page on its vehicle tab: the 3D hero is the player's equipped bike of the setup's type (the mod's when chosen).
        static IEnumerator Setup(string tag, int model)
        {
            var ev = OpenContent.Events.FirstOrDefault(e => e.ev.spec.type == "National" && !e.freeRide);
            if (ev == null) { Log("no National: setup skipped"); yield break; }
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(ev.freeRide);
            void Set(string f, object v) => typeof(OpenShell).GetField(f, All).SetValue(Shell, v);
            Set("_browseEvent", ev); Set("_setup", OpenHistory.SetupOf(ev.key));
            Shell.ShowSetupPage();
            for (int i = 0; i < 30; i++) yield return null;
            // the setup's vehicle type: the equipped bike's (MX Lite for a 250F, MX for a 450F)
            try
            {
                var setup = typeof(OpenShell).GetField("_setup", All).GetValue(Shell) as OpenSetup;
                int type = OpenGarageData.Vehicles().FirstOrDefault(v => v.id == model)?.type ?? -1;
                if (setup != null && type >= 0) setup.vehicleType = type;
                Log($"{tag}: setup {ev.key} vehicle type {type} -> {setup?.Type(ev.ev)}");
            }
            catch (Exception e) { Log($"{tag}: setup type: {e.Message}"); }
            typeof(OpenShell).GetMethod("SetSetupTab", All)?.Invoke(Shell, new object[] { 1, false });
            for (int i = 0; i < 360; i++) yield return null;
            yield return UiShowcase.Shot(tag + "-setup");
        }
    }
}
