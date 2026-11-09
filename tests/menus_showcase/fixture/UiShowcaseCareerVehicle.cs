// TEST ONLY: the career hub's top-left plate with the scratch garage profile open (the vehicle the player rides in the focused class).
using System;
using System.Collections;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseCareerVehicle
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("career-equipped", () => Equipped("career-equipped", false, 0, -1));
            UiShowcase.Register("career-equipped-event", () => Equipped("career-equipped-event", false, 0, 1));
            UiShowcase.Register("career-equipped-far", () => Equipped("career-equipped-far", false, 4, 2));
            UiShowcase.Register("career-equipped-unlocked", () => Equipped("career-equipped-unlocked", true, 4, -1));
            UiShowcase.Register("career-equipped-sweep", EquippedSweep);
            UiShowcase.Register("career-equipped-unlocked-event", () => Equipped("career-equipped-unlocked-event", true, 9, 1));
        }

        /// The career hub with the scratch garage profile open: the plate top left names the vehicle the player rides in the focused class.
        static IEnumerator Equipped(string shot, bool unlocked, int tier, int eventIndex)
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (CoreGarageProfile.UnlockEverythingEnabled != unlocked && !CoreGarageProfile.SetUnlockEverything(unlocked)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            yield return UiShowcasePages.CareerSynth(shot, tier, eventIndex);
        }


        /// Every tier of the unlocked profile (all vehicles owned): the plate's text each time; a screenshot of the first tier of each different plate.
        static IEnumerator EquippedSweep()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            var seen = new System.Collections.Generic.HashSet<string>();
            for (int tier = 0; tier < 24; tier++)
            {
                var shell = OpenShell.Instance;
                shell.Clear(OpenShell.Screen.Working, "career");
                UiShowcasePages.Call(shell, "CareerChrome");
                var hub = new OcRaceAndRide(shell.Layer, shell.Nav, shell.ShowMain, c => { });
                hub.Fill(UiShowcasePages.SynthChoicesPublic(), "sweep");
                yield return UiShowcase.Frames(8);
                var focus = hub.DefaultFocus;
                for (int i = 0; i < tier && focus != null; i++) focus = ((OuNavItem)focus.userData).right;
                if (focus == null) break;
                shell.Nav.Focus(focus);
                yield return UiShowcase.Frames(12);
                var cap = shell.Layer.Q<Label>(className: "oc-ride-cap")?.text; var name = shell.Layer.Q<Label>(className: "oc-ride-name")?.text;
                Debug.Log("[ui-showcase] tier " + tier + ": " + cap + " | " + name);
                if (seen.Add(cap + "|" + name)) { yield return UiShowcase.Frames(20); yield return UiShowcase.Shot("career-sweep-" + tier); }
                shell.ShowMain();
                yield return UiShowcase.Frames(3);
            }
        }
    }
}
