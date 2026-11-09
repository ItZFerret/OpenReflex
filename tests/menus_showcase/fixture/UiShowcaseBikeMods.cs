// TEST ONLY: the bike mods in the garage (w-bikemods). Run with a client root whose Mods/Bikes holds a bike mod (run.py --client-root):
//   bikemod-garage   same as bikemod-cards (the MODEL row is gone: mod bikes are vehicles of their own in the VEHICLE list)
//   bikemod-cards    GARAGE > VEHICLE with the mod bikes' cards under the originals: shots, A on a mod card / the original card, CLASS > MODDED ONLY
//   bikemod-kits     GARAGE > PARTS with the mod chosen: the graphic kit row's strip (the mod's kits tagged MOD), a kit focused
// A scratch save (REFLEX_SAVE_DIR), nothing of the user's.
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
    static class UiShowcaseBikeMods
    {
        const string Tag = "[ui-showcase] ";
        static Gamepad pad;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("bikemod-garage", Cards);
            UiShowcase.Register("bikemod-kits", Kits);
            UiShowcase.Register("bikemod-cards", Cards);
        }

        static IEnumerator PadTap(GamepadButton b, int frames = 3)
        {
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("bikemods pad");
            InputSystem.QueueStateEvent(pad, new GamepadState(b));
            for (int i = 0; i < frames; i++) yield return null;
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 8; i++) yield return null;
        }

        static IEnumerator Open(int tab)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(false)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            var st = typeof(OpenShell).Assembly.GetType("Reflex.Port.OpenFrontEnd.OpenGarageStage");
            st.GetField("Verbose").SetValue(null, true); st.GetField("NoAutoTurn").SetValue(null, true);
            OpenShell.Instance.ShowGarage(tab);
            for (int i = 0; i < 200; i++) yield return null;
        }
        static void Angles(float yaw, float pitch)
        {
            var stage = typeof(OpenShell).GetField("garageStage", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(OpenShell.Instance);
            stage.GetType().GetMethod("SetAngles").Invoke(stage, new object[] { yaw, pitch });
        }
        static string Focused() => OpenShell.Instance.Nav.Focused?.name ?? "(none)";
        static void Fail(string what) => Debug.LogError(Tag + "FAIL bikemods: " + what);

        /// GARAGE > VEHICLE with the mod's cards: a shot of the list (the mod card of the equipped bike focused), A on the mod card (equips the base bike and
        /// stores the mod as its model: the EQUIPPED label moves to that card), A on the ORIGINAL card (back), the CLASS filter on MODDED ONLY.
        static IEnumerator Cards()
        {
            if (BikeMods.All.Count == 0) { Fail("no bike mod"); yield break; }
            yield return Open(OpenShell.GarageVehicle);
            var shell = OpenShell.Instance;
            int model = OpenGarageData.EquippedModel;
            BikeMods.SetChoice(model, BikeMods.Original);
            yield return Open(OpenShell.GarageVehicle);
            var names = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-mod-")).Select(e => e.name).ToList();
            Log("mod cards: " + names.Count + " " + string.Join(",", names.Take(12)));
            var card = shell.Nav.Items().FirstOrDefault(e => e.name == "garage-mod-" + model + "-0");
            if (card == null) { Fail("no mod card for the equipped model " + model); yield break; }
            shell.Nav.Focus(card);
            for (int i = 0; i < 400; i++) yield return null;
            yield return UiShowcase.Shot("garage-mod-cards");
            (card.userData as OuNavItem)?.submit?.Invoke();
            for (int i = 0; i < 300; i++) yield return null;
            Log("A on the mod card: choice \"" + BikeMods.ChoiceFor(model) + "\" equipped " + OpenGarageData.Equipped + " model " + OpenGarageData.EquippedModel);
            if (BikeMods.ChoiceFor(model).Length == 0) Fail("A on the mod card did not store the mod");
            yield return UiShowcase.Shot("garage-mod-cards-equipped");
            var orig = shell.Nav.Items().FirstOrDefault(e => e.name != null && (e.name == "garage-vehicle-" + OpenGarageData.Equipped));
            if (orig != null) { shell.Nav.Focus(orig); yield return null; (orig.userData as OuNavItem)?.submit?.Invoke(); }
            for (int i = 0; i < 200; i++) yield return null;
            Log("A on the original card: choice \"" + BikeMods.ChoiceFor(model) + "\"");
            if (BikeMods.ChoiceFor(model).Length != 0) Fail("A on the original card did not restore the original");
            // MY RIDES > MODDED BIKES: B up to the classes, then the MODDED BIKES entry
            yield return PadTap(GamepadButton.East); for (int i = 0; i < 20; i++) yield return null;
            var modsEntry = shell.Nav.Items().FirstOrDefault(e => e.name == "garage-class-mods");
            if (modsEntry == null) Fail("MY RIDES has no MODDED BIKES entry (on " + shell.GarageTitle + ")");
            else { shell.Nav.Focus(modsEntry); yield return null; yield return PadTap(GamepadButton.South); }
            for (int i = 0; i < 100; i++) yield return null;
            Log("level = " + shell.GarageCrumb + " > " + shell.GarageTitle + ", mod cards " + shell.Nav.Items().Count(e => e.name != null && e.name.StartsWith("garage-mod-")) + ", original cards " + shell.Nav.Items().Count(e => e.name != null && (e.name.StartsWith("garage-vehicle-") || e.name.StartsWith("garage-model-"))));
            yield return UiShowcase.Shot("garage-mod-cards-modded-only");
            var kx = shell.Nav.Items().FirstOrDefault(e => e.name == "garage-mod-13-0") ?? shell.Nav.Items().FirstOrDefault(e => e.name != null && e.name.StartsWith("garage-mod-"));
            if (kx != null) { shell.Nav.Focus(kx); for (int i = 0; i < 400; i++) yield return null; yield return UiShowcase.Shot("garage-mod-bikes"); }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Kits()
        {
            if (BikeMods.All.Count == 0) { Fail("no bike mod"); yield break; }
            yield return Open(OpenShell.GarageParts);
            int model = OpenGarageData.EquippedModel;
            if (BikeMods.ChoiceFor(model).Length == 0) { BikeMods.SetChoice(model, BikeMods.ModsFor(OpenGarageData.BikeOfModel(model, -1, false)).First().id); yield return Open(OpenShell.GarageParts); }
            var shell = OpenShell.Instance;
            var rows = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-part-")).ToList();
            Log("part rows: " + string.Join(",", rows.Select(r => r.name)));
            var row = rows.FirstOrDefault(r => r.name == "garage-part-11") ?? rows.FirstOrDefault();
            if (row == null) { Fail("no part rows"); yield break; }
            shell.Nav.Focus(row); for (int i = 0; i < 40; i++) yield return null;
            yield return PadTap(GamepadButton.South);   // A: into the part type (its brands / models)
            for (int i = 0; i < 20; i++) yield return null;
            bool Mod(VisualElement c) => c.Query<Label>(className: "ou-pill").ToList().Any(l => l.text == "MOD");
            var lines = shell.Nav.Items().Where(e => e.name != null && (e.name.StartsWith("garage-line-") || e.name.StartsWith("garage-item-"))).ToList();
            var modLine = lines.FirstOrDefault(c => c.name.StartsWith("garage-line-") && Mod(c));
            Log("section " + shell.GarageTitle + ": " + lines.Count + " entries, " + lines.Count(Mod) + " tagged MOD; focus " + Focused());
            if (modLine != null) { shell.Nav.Focus(modLine); yield return null; yield return PadTap(GamepadButton.South); for (int i = 0; i < 20; i++) yield return null; }   // A: the line's styles
            var chips = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-item-")).ToList();
            var mods = chips.Where(Mod).ToList();
            Log("items: " + chips.Count + ", " + mods.Count + " tagged MOD; focus " + Focused());
            if (mods.Count == 0) Fail("no item tagged MOD under " + row.name);
            // a kit other than the worn one, with the pad (the worn one is focused first)
            string before = Focused();
            yield return PadTap(GamepadButton.DpadDown);
            if (Focused() == before) yield return PadTap(GamepadButton.DpadUp);
            Log("pad down / up -> " + Focused());
            for (int i = 0; i < 300; i++) yield return null;
            yield return UiShowcase.Shot("bikemod-kit-strip");
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        static void Log(string s) => Debug.Log(Tag + "bikemods " + s);
    }
}
