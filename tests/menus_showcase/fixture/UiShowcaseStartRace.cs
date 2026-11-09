// TEST ONLY: the career's StartRacePopup (FEATURE_AUDIT C03), from the product's own code (OpenShell.ChooseCareerEvent / ShowStartRacePopup) over the career hub
// with the catalog's real events. Pad journey: A on an event row opens the popup (YES focused), B closes it with the focus back on the row, A again, A on YES
// starts the event (the launch is replaced by a log line: the showcase must not leave the front end). Also: right to NO + A is the same as B; a Moto row
// shows the PRACTICE page instead; a locked row still shows the LockedPopup.
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseStartRace
    {
        const string Tag = "[ui-showcase] [startrace] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("startrace-popup", Journey);

        static string FocusedName(OpenShell shell) => (shell.Nav.Focused as VisualElement)?.name ?? "(none)";
        static IEnumerator Tap(GamepadButton b) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(14); }

        static IEnumerator Journey()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            var shell = OpenShell.Instance;
            shell.Clear(OpenShell.Screen.Working, "career");
            UiShowcasePages.Call(shell, "CareerChrome");
            var presentation = new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, () => { }, () => { });
            OcRaceAndRide hub = null;
            hub = new OcRaceAndRide(shell.Layer, shell.Nav, shell.ShowMain, c => shell.ChooseCareerEvent(c, presentation, hub));
            var choices = UiShowcasePages.SynthChoicesPublic();
            hub.Fill(choices, "CAREER PROFILE · Earn progress by completing events.");
            shell.Nav.OnCancel = () => { shell.ShowMain(); return true; };
            yield return UiShowcase.Frames(40);

            var launched = new System.Collections.Generic.List<int>();
            OpenShell.CareerLaunchOverride = c => { launched.Add(c.Entry.SeriesId); Debug.Log(Tag + "LAUNCH series " + c.Entry.SeriesId + " (" + c.Entry.Format + ")"); };

            // an ordinary race event: not Moto / Heat, not locked
            var pick = choices.First(c => !c.Entry.IsMoto && !c.Entry.IsHeat && !c.Locked && c.Entry.SeriesId >= 3);
            string rowName = "career-series-" + pick.Entry.SeriesId;
            UiShowcaseNav.Check(hub.FocusSeries(pick.Entry.SeriesId), "startrace", "the row of series " + pick.Entry.SeriesId + " could not be focused");
            yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(FocusedName(shell) == rowName, "startrace", "the row is not focused (" + FocusedName(shell) + ")");
            Debug.Log(Tag + "row " + rowName + " " + pick.Entry.Activity + " / " + pick.Entry.Format + " laps " + pick.Entry.Laps);
            yield return UiShowcase.Shot("startrace-1-row");

            // A: the popup opens, YES focused, nothing started
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(shell.PopupOpen && shell.Popup.name == "popup-start-race", "startrace", "A on the row did not open the StartRacePopup");
            UiShowcaseNav.Check(launched.Count == 0, "startrace", "the event started before the popup was answered");
            UiShowcaseNav.Check(FocusedName(shell) == "popup-start-yes", "startrace", "YES is not focused (" + FocusedName(shell) + ")");
            var texts = shell.Popup.Query<Label>().ToList().Select(l => l.text).ToList();
            Debug.Log(Tag + "popup texts: " + string.Join(" | ", texts));
            UiShowcaseNav.Check(texts.Contains("START EVENT") && texts.Any(t => t.StartsWith("Are you sure")), "startrace", "the popup lacks the original title / message");
            UiShowcaseNav.Check(shell.Popup.Q("popup-start-facts") != null, "startrace", "the popup lacks the event card");
            yield return UiShowcase.Frames(20);
            { var r = shell.Popup; string chain = ""; for (var e = r; e != null; e = e.parent) chain += e.name + "[" + string.Join(".", e.GetClasses()) + "] < ";
              Debug.Log(Tag + "popup chain " + chain + " pos " + r.resolvedStyle.position + " bound " + r.worldBound + " card " + r.Q(className: "ou-dialog").worldBound); }
            yield return UiShowcase.Shot("startrace-2-popup-yes");

            // d-pad right: NO; left: YES again
            yield return Tap(GamepadButton.DpadRight);
            UiShowcaseNav.Check(FocusedName(shell) == "popup-start-no", "startrace", "right did not reach NO (" + FocusedName(shell) + ")");
            yield return Tap(GamepadButton.DpadLeft);
            UiShowcaseNav.Check(FocusedName(shell) == "popup-start-yes", "startrace", "left did not return to YES (" + FocusedName(shell) + ")");
            yield return Tap(GamepadButton.DpadRight);
            yield return UiShowcase.Shot("startrace-3-popup-no");

            // B: back to the list, focus kept on the row
            yield return Tap(GamepadButton.East);
            UiShowcaseNav.Check(!shell.PopupOpen, "startrace", "B did not close the popup");
            UiShowcaseNav.Check(FocusedName(shell) == rowName, "startrace", "B did not return the focus to the row (" + FocusedName(shell) + ")");
            UiShowcaseNav.Check(launched.Count == 0, "startrace", "B started the event");
            yield return UiShowcase.Shot("startrace-4-after-b");

            // A, then NO + A: closed again, focus on the row
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(shell.PopupOpen, "startrace", "the popup did not open the second time");
            yield return Tap(GamepadButton.DpadRight);
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(!shell.PopupOpen && FocusedName(shell) == rowName && launched.Count == 0, "startrace", "NO did not return to the row (" + FocusedName(shell) + ", launched " + launched.Count + ")");

            // A, A on YES: the event starts, the popup is gone
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(shell.PopupOpen && FocusedName(shell) == "popup-start-yes", "startrace", "the popup did not open the third time");
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(!shell.PopupOpen, "startrace", "YES left the popup open");
            UiShowcaseNav.Check(launched.Count == 1 && launched[0] == pick.Entry.SeriesId, "startrace", "YES did not start series " + pick.Entry.SeriesId + " (" + string.Join(",", launched) + ")");
            // (the start is latched: the row does not start it twice)
            yield return Tap(GamepadButton.South);
            UiShowcaseNav.Check(!shell.PopupOpen && launched.Count == 1, "startrace", "a second A after the start opened a popup or launched again");
            yield return UiShowcase.Shot("startrace-5-started");

            // the other branches of OnAcceptSeries keep their pages: a Moto row -> the practice page; a locked row -> the LockedPopup
            var moto = choices.FirstOrDefault(c => c.Entry.IsMoto && !c.Locked && c.Eligible);
            var locked = choices.FirstOrDefault(c => c.Locked);
            OpenShell.CareerLaunchOverride = null;
            typeof(OpenShell).GetField("_careerStarting", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic).SetValue(shell, false);   // (ShowOwnedCareer resets it when the hub is built)
            shell.Clear(OpenShell.Screen.Working, "career");
            UiShowcasePages.Call(shell, "CareerChrome");
            hub = new OcRaceAndRide(shell.Layer, shell.Nav, shell.ShowMain, c => shell.ChooseCareerEvent(c, presentation, hub));
            hub.Fill(choices, "branches");
            yield return UiShowcase.Frames(40);
            if (locked != null && hub.FocusSeries(locked.Entry.SeriesId))
            {
                yield return UiShowcase.Frames(15);
                yield return Tap(GamepadButton.South);
                Debug.Log(Tag + "locked row -> popup " + (shell.PopupOpen ? shell.Popup.name : "(none)"));
                UiShowcaseNav.Check(shell.PopupOpen && shell.Popup.name == "popup-locked", "startrace", "a locked row did not show the LockedPopup");
                yield return UiShowcase.Shot("startrace-6-locked-popup-reference");
                yield return Tap(GamepadButton.East);
            }
            if (moto != null && hub.FocusSeries(moto.Entry.SeriesId))
            {
                yield return UiShowcase.Frames(15);
                yield return Tap(GamepadButton.South);
                bool practice = shell.Layer.Q("career-moto-practice-choice") != null;
                Debug.Log(Tag + "moto row -> practice page " + practice + ", popup " + shell.PopupOpen);
                UiShowcaseNav.Check(practice && !shell.PopupOpen, "startrace", "a Moto row did not show the practice page");
            }
            shell.ShowMain();
            yield return UiShowcase.Frames(3);
        }
    }
}
