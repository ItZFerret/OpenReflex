// TEST ONLY (w-trackmods): the custom tracks of Mods/Tracks (Content/TrackMods.cs) in the browser, from the product's own code. Run with a
// client root whose Mods/Tracks holds tracks (run.py --client-root):
//   trackmods-browser   FREE RIDE > CUSTOM TRACKS (every track's Free Ride, a broken track and a loose archive with their reasons), the pad
//                       moving over the cards, A on a track opening its setup page, A on a broken one doing nothing; RACE > CUSTOM TRACKS
//                       (only the National / Supercross tracks); the empty state of a client root without tracks (how to install one).
// Shots: trackmods-freeride, trackmods-broken, trackmods-setup, trackmods-race, trackmods-empty. Nothing is ridden.
using System;
using System.Collections;
using System.IO;
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
    static class UiShowcaseTrackMods
    {
        const string Tag = "[ui-showcase] [trackmods] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("trackmods-browser", Journey);

        static OpenShell Shell => OpenShell.Instance;
        static string Focused => (Shell.Nav.Focused as VisualElement)?.Q<Label>()?.text ?? "(none)";
        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "trackmods", what); if (ok) Log("ok: " + what); }
        static void Log(string s) => Debug.Log(Tag + s);
        static IEnumerator Tap(GamepadButton b) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(8); }

        static IEnumerator Journey()
        {
            foreach (var m in TrackMods.All) Log($"track {m.id}: \"{m.name}\" {m.kindLabel} {m.level} {(m.Ok ? "ok" : "BROKEN " + m.problem)}");
            // FREE RIDE > CUSTOM TRACKS
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(true); yield return UiShowcase.Frames(30);
            Shell.SelectBrowse(OpenContent.CustomGroup); yield return UiShowcase.Frames(30);
            Shell.LoadAllThumbnails(); yield return UiShowcase.Frames(10);
            var cards = Shell.CardEvents.ToList();
            Log("free ride custom cards: " + string.Join(" | ", cards.Select(c => c.locationName + " [" + c.Where + "] " + c.key)));
            int tracks = TrackMods.All.Count;
            Check(cards.Count == tracks && cards.All(c => c.Custom && c.freeRide), $"FREE RIDE > CUSTOM TRACKS lists every track once ({cards.Count} cards, {tracks} tracks)");
            Check(cards.Any(c => c.Broken && !c.mod.archive) && cards.Any(c => c.Broken && c.mod.archive), "a broken track and a loose archive are listed with their reasons");
            Check(cards.Count(c => c.ev.location.Equals("Beta_Track_Slot_1", StringComparison.OrdinalIgnoreCase) && !c.Broken) >= 2, "two tracks of the same slot (Beta_Track_Slot_1) are both listed");
            yield return UiShowcase.Shot("trackmods-freeride");

            // the pad over the cards: right steps to the next card, the selection follows
            var first = Shell.SelectedEvent;
            yield return Tap(GamepadButton.DpadRight);
            Check(Shell.SelectedEvent != null && Shell.SelectedEvent != first, "the pad moves the selection to the next card (" + first?.locationName + " -> " + Shell.SelectedEvent?.locationName + ")");

            // a broken track: the reason, no SET UP, A does nothing
            var broken = cards.First(c => c.Broken && !c.mod.archive);
            Check(Shell.SelectEvent(broken.key), "the broken track's card can be selected");
            yield return UiShowcase.Frames(10);
            var next = Shell.Root.Q("ou-setup-next");
            Check(next != null && !next.enabledSelf, "SET UP RIDE is off on a broken track");
            yield return UiShowcase.Shot("trackmods-broken");
            yield return Tap(GamepadButton.South);
            Check(Shell.Current == OpenShell.Screen.Browser, "A on a broken track stays in the browser (" + Shell.Current + ")");

            // a good track: A opens its setup page, B comes back
            var good = cards.First(c => !c.Broken && c.mod.kind == "National");
            Check(Shell.SelectEvent(good.key), "the National track's card can be selected");
            yield return UiShowcase.Frames(10);
            yield return Tap(GamepadButton.South);
            yield return UiShowcase.Frames(30);
            Check(Shell.Current == OpenShell.Screen.Setup, "A on a custom track opens its setup page (" + Shell.Current + ")");
            yield return UiShowcase.Shot("trackmods-setup");
            yield return Tap(GamepadButton.East);
            yield return UiShowcase.Frames(20);

            // RACE > CUSTOM TRACKS: the National / Supercross tracks' races (and the broken race-kind track)
            Shell.ShowBrowser(false); yield return UiShowcase.Frames(30);
            Shell.SelectBrowse(OpenContent.CustomGroup); yield return UiShowcase.Frames(30);
            Shell.LoadAllThumbnails(); yield return UiShowcase.Frames(10);
            var races = Shell.CardEvents.ToList();
            Log("race custom cards: " + string.Join(" | ", races.Select(c => c.typeName + " " + c.locationName + " [" + c.Where + "] " + c.key)));
            int racing = TrackMods.All.Count(m => m.Races);
            Check(races.Count(c => !c.Broken) == racing && races.All(c => c.Broken || c.activity == "National" || c.activity == "Supercross"), $"RACE > CUSTOM TRACKS lists the {racing} race tracks");
            yield return UiShowcase.Shot("trackmods-race");

            // a client root without tracks: the empty state says how to install one (OPEN MODS FOLDER)
            string empty = Path.Combine(Path.GetTempPath(), "trackmods-empty-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Path.Combine(empty, "Mods", "Tracks"));
            string root = ReflexMods.ClientRoot;
            ReflexMods.SetRootForTest(empty);
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(true); yield return UiShowcase.Frames(20);
            Shell.SelectBrowse(OpenContent.CustomGroup); yield return UiShowcase.Frames(30);
            Check(!Shell.CardEvents.Any(), "an empty Mods/Tracks lists no custom track");
            var text = string.Join(" ", Shell.Root.Query<Label>(className: "ou-empty-text").ToList().Select(l => l.text));
            Check(text.Contains("Mods/Tracks/<Track Name>/"), "the empty state says how to install a track");
            yield return UiShowcase.Shot("trackmods-empty");
            ReflexMods.SetRootForTest(root);
            TrackMods.Refresh();
            try { Directory.Delete(empty, true); } catch (Exception) { }
            Log("done");
        }
    }
}
