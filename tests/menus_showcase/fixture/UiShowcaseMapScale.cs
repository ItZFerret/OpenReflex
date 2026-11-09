// TEST ONLY (w-mapscale): the race setup page's MAP SCALE row (core/plan/MAP_SCALE.md), from the product's own code. A National's setup
// page: the row on the EVENT tab with its value and hint; pad right / left step 5 %, the limits 50 % and 200 % (no wrap), the keyboard's
// arrows and the mouse on the row's arrows step it too, A puts it back to 100 % (the setting's key removed); the value is kept per track
// (another track keeps its own), shown in the summary, and handed to the launch; a Free Ride's setup page has the row too; the career hub
// has none. The scratch save of the runner holds the settings (the user's are never touched).
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseMapScale
    {
        const string Tag = "[ui-showcase] [mapscale] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("mapscale-row", Journey);

        static OpenShell Shell => OpenShell.Instance;
        static string Focused => (Shell.Nav.Focused as VisualElement)?.name ?? "(none)";
        static VisualElement Q(string name) => Shell.Root.Q(name);
        static void Check(bool ok, string what) => UiShowcaseNav.Check(ok, "mapscale", what);
        static IEnumerator Tap(GamepadButton b) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(8); }
        static IEnumerator Key(Key k) { yield return UiShowcaseNav.Key(k); yield return UiShowcase.Frames(8); }
        static IEnumerator Nav(string method, params object[] args) => (IEnumerator)typeof(UiShowcaseNav).GetMethod(method, All).Invoke(null, args);
        static Vector2 ScreenPoint(VisualElement e) => (Vector2)typeof(UiShowcaseNav).GetMethod("ScreenPoint", All).Invoke(null, new object[] { e });
        static void Set(object o, string field, object value) => o.GetType().GetField(field, All).SetValue(o, value);

        static string Value() { var row = Q("ou-setup-mapscale"); return row?.Q<Label>(className: "mxa-value")?.text ?? "(no row)"; }
        static string Hint() => (Q("ou-setup-mapscale-hint") as Label)?.text ?? "(no hint)";

        static IEnumerator ToSetup(OpenEvent ev)
        {
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(ev.freeRide);
            Set(Shell, "_browseEvent", ev); Set(Shell, "_setup", OpenHistory.SetupOf(ev.key));
            Shell.ShowSetupPage();
            yield return UiShowcase.Frames(40);
        }
        static IEnumerator FocusRow()
        {
            var row = Q("ou-setup-mapscale");
            Check(row != null, "the setup page has no MAP SCALE row");
            if (row == null) yield break;
            typeof(OpenShell).GetMethod("SetSetupTab", All).Invoke(Shell, new object[] { 0, false });
            yield return UiShowcase.Frames(4);
            Shell.Nav.Focus(row); yield return UiShowcase.Frames(6);
            Check(Focused == "ou-setup-mapscale", "the row did not take the focus (" + Focused + ")");
        }

        static IEnumerator Journey()
        {
            var events = OpenContent.Events.ToList();
            var nat = events.FirstOrDefault(e => e.ev.spec.type == "National" && !e.freeRide);
            var other = events.FirstOrDefault(e => !e.freeRide && nat != null && e.ev.location != nat.ev.location && e.ev.spec.type != "FreeRide");
            var fr = events.FirstOrDefault(e => e.freeRide);
            Check(nat != null && other != null && fr != null, "the install has no National / other track / Free Ride event");
            if (nat == null || other == null || fr == null) yield break;
            string track = nat.ev.location;
            ReflexSettings.SetMapScalePercent(track, 100); ReflexSettings.SetMapScalePercent(other.ev.location, 100);
            Debug.Log(Tag + "tracks " + track + " / " + other.ev.location + " / free ride " + fr.ev.location);

            yield return ToSetup(nat);
            Check(Shell.Current == OpenShell.Screen.Setup, "the setup page did not open");
            yield return FocusRow();
            Debug.Log(Tag + "row value '" + Value() + "' hint '" + Hint() + "'");
            Check(Value().StartsWith("100 %"), "the row does not start at 100 % (" + Value() + ")");
            Check(!ReflexSave.Has("Race.MapScale." + track), "100 % keeps a key");
            yield return UiShowcase.Shot("mapscale-1-row-100");

            // pad right x7: 135 %, saved for the track
            for (int i = 0; i < 7; i++) yield return Tap(GamepadButton.DpadRight);
            Debug.Log(Tag + "after 7 x right: '" + Value() + "' setting " + ReflexSettings.MapScalePercent(track) + " hint '" + Hint() + "'");
            Check(Value() == "135 %" && ReflexSettings.MapScalePercent(track) == 135, "7 steps right are not 135 % (" + Value() + ", " + ReflexSettings.MapScalePercent(track) + ")");
            Check(Focused == "ou-setup-mapscale", "stepping lost the row's focus (" + Focused + ")");
            var summary = Shell.Root.Query<Label>().ToList().Select(l => l.text).ToList();
            Check(summary.Any(t => t.Contains("135 %") && t.Contains("NO RECORDS")), "the summary has no MAP SCALE chip");
            yield return UiShowcase.Shot("mapscale-2-row-135");

            // the upper limit: 200 %, no wrap
            for (int i = 0; i < 16; i++) yield return Tap(GamepadButton.DpadRight);
            Check(Value() == "200 %" && ReflexSettings.MapScalePercent(track) == 200, "the upper limit is not 200 % (" + Value() + ")");
            yield return UiShowcase.Shot("mapscale-3-row-200");
            // the keyboard: left arrow 195, right 200
            yield return Key(UnityEngine.InputSystem.Key.LeftArrow);
            Check(ReflexSettings.MapScalePercent(track) == 195, "the left arrow key did not step down (" + Value() + ")");
            yield return Key(UnityEngine.InputSystem.Key.RightArrow);
            Check(ReflexSettings.MapScalePercent(track) == 200, "the right arrow key did not step up (" + Value() + ")");
            // the lower limit: 50 %, no wrap
            for (int i = 0; i < 34; i++) yield return Tap(GamepadButton.DpadLeft);
            Check(Value() == "50 %" && ReflexSettings.MapScalePercent(track) == 50, "the lower limit is not 50 % (" + Value() + ")");
            Debug.Log(Tag + "at the lower limit: '" + Value() + "' hint '" + Hint() + "'");
            yield return UiShowcase.Shot("mapscale-4-row-50");

            // the mouse on the row's arrows: › 55, ‹ 50
            var arrows = Q("ou-setup-mapscale").Query<Label>(className: "mxa-arrow").ToList();
            Check(arrows.Count == 2, "the row has no two arrows");
            if (arrows.Count == 2)
            {
                yield return Nav("MouseClick", ScreenPoint(arrows[1])); yield return UiShowcase.Frames(6);
                Check(ReflexSettings.MapScalePercent(track) == 55, "a click on › did not step up (" + Value() + ")");
                yield return Nav("MouseClick", ScreenPoint(arrows[0])); yield return UiShowcase.Frames(6);
                Check(ReflexSettings.MapScalePercent(track) == 50, "a click on ‹ did not step down (" + Value() + ")");
                yield return FocusRow();
            }

            // A: back to 100 %, the key removed
            for (int i = 0; i < 6; i++) yield return Tap(GamepadButton.DpadRight);   // 80 %
            yield return Tap(GamepadButton.South);
            Check(Value().StartsWith("100 %") && !ReflexSave.Has("Race.MapScale." + track), "A did not reset to 100 % (" + Value() + ")");
            // Enter does the same
            for (int i = 0; i < 4; i++) yield return Tap(GamepadButton.DpadRight);   // 120 %
            yield return Key(UnityEngine.InputSystem.Key.Enter);
            Check(Value().StartsWith("100 %") && !ReflexSave.Has("Race.MapScale." + track), "Enter did not reset to 100 % (" + Value() + ")");
            yield return UiShowcase.Shot("mapscale-5-reset");

            // per track: 150 % here, the other track stays at 100 %; the launch gets 150 %
            for (int i = 0; i < 10; i++) yield return Tap(GamepadButton.DpadRight);
            Check(ReflexSettings.MapScalePercent(track) == 150, "150 % not set (" + Value() + ")");
            float launched = -1; OpenSetup.LaunchOverride = s => launched = RxMapScale.FromPercent(OpenSetup.MapScalePercent(s.Event));
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(4);
            yield return Tap(GamepadButton.South);
            yield return UiShowcase.Frames(10);
            // (a rules popup may stand before the launch on some event types: a National has none)
            Check(Math.Abs(launched - 1.5f) < 1e-4f, "START EVENT did not launch at 150 % (" + launched + ")");
            OpenSetup.LaunchOverride = null;
            yield return ToSetup(other);
            Check(Value().StartsWith("100 %"), "another track does not keep its own scale (" + Value() + " on " + other.ev.location + ")");
            yield return ToSetup(nat);
            Check(Value() == "150 %", "the first track lost its scale (" + Value() + ")");
            yield return FocusRow();
            yield return UiShowcase.Shot("mapscale-6-kept-150");

            // a Free Ride location: the row is there (EVENT tab)
            yield return ToSetup(fr);
            Check(Q("ou-setup-mapscale") != null, "a Free Ride setup page has no MAP SCALE row");
            yield return FocusRow();
            yield return UiShowcase.Shot("mapscale-7-freeride");

            // the career hub: no MAP SCALE anywhere (its events are always at 100 %)
            Shell.Clear(OpenShell.Screen.Working, "career");
            UiShowcasePages.Call(Shell, "CareerChrome");
            var presentation = new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, () => { }, () => { });
            OcRaceAndRide hub = null;
            hub = new OcRaceAndRide(Shell.Layer, Shell.Nav, Shell.ShowMain, c => { });
            hub.Fill(UiShowcasePages.SynthChoicesPublic(), "CAREER PROFILE");
            yield return UiShowcase.Frames(30);
            Check(Q("ou-setup-mapscale") == null, "the career hub shows a MAP SCALE row");
            Check(Math.Abs(RxMapScale.Current - 1f) < 1e-6f, "the front end's map scale is not 100 % (" + RxMapScale.Current + ")");

            ReflexSettings.SetMapScalePercent(track, 100);
            Shell.ShowMain(); yield return UiShowcase.Frames(5);
            Debug.Log(Tag + "DONE failures " + UiShowcaseNav.Failures);
        }
    }
}
