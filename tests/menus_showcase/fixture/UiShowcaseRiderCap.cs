// TEST ONLY (w-rc-client, core/plan/RIDER_CAP.md "Done (w-rc-client)"): the race setup's FIELD SIZE row and a 32-rider results page, from
// the product's own code. A National's setup: the row on the EVENT tab starts at 12 (the original's), the pad's right steps it one rider
// (12 -> 24), the summary's FIELD chip follows; a Supercross at 32 shows the crowding warning; a Freestyle's row is greyed and stays at 12;
// AI off greys it ("YOU ALONE"). Then the owned results page with 32 riders: 32 rows in its scroll view, the local rider's in view.
// The scratch save of the runner holds the settings (the user's are never touched). Page: "ridercap".
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseRiderCap
    {
        const string Tag = "[ui-showcase] [ridercap] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("ridercap", Journey);

        static OpenShell Shell => OpenShell.Instance;
        static string Focused => (Shell.Nav.Focused as VisualElement)?.name ?? "(none)";
        static VisualElement Q(string name) => Shell.Root.Q(name);
        static void Check(bool ok, string what) => UiShowcaseNav.Check(ok, "ridercap", what);
        // a step of the focused value row through its nav item (OuNavItem.adjust: what the pad's / keyboard's left and right call). The
        // menus' own pad handling is covered by UiShowcaseNav; in a batch player the setup page's late focus (the leaderboard panel) moves
        // the focus under a synthetic pad, so the row is driven directly here.
        static IEnumerator Step(int d)
        {
            var row = Q("ou-setup-riders");
            (row?.userData as OuNavItem)?.adjust?.Invoke(d);
            yield return UiShowcase.Frames(2);
        }
        static IEnumerator Settle(float s) { for (float t0 = Time.realtimeSinceStartup; Time.realtimeSinceStartup - t0 < s;) yield return null; }
        static void Set(object o, string field, object value) => o.GetType().GetField(field, All).SetValue(o, value);
        static object Get(object o, string field) => o.GetType().GetField(field, All).GetValue(o);
        static void Call(object o, string method, params object[] args) => o.GetType().GetMethod(method, All).Invoke(o, args);

        static string Value() => Q("ou-setup-riders")?.Q<Label>(className: "mxa-value")?.text ?? "(no row)";
        static string Note() => (Q("ou-setup-riders-note") as Label)?.text ?? "(no note)";
        static float Opacity() { var r = Q("ou-setup-riders"); return r == null ? -1f : r.resolvedStyle.opacity; }
        static OpenSetup Setup => (OpenSetup)Get(Shell, "_setup");

        static IEnumerator ToSetup(OpenEvent ev, int ai, int riders)
        {
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(ev.freeRide);
            var s = OpenHistory.SetupOf(ev.key); s.ai = ai; s.riders = riders;
            Set(Shell, "_browseEvent", ev); Set(Shell, "_setup", s);
            Shell.ShowSetupPage();
            yield return UiShowcase.Frames(40);
            typeof(OpenShell).GetMethod("SetSetupTab", All).Invoke(Shell, new object[] { 0, false });
            // (a batch player's frames are short: wait for the page by time, at most 8 s)
            for (float t0 = Time.realtimeSinceStartup; Q("ou-setup-riders") == null && Time.realtimeSinceStartup - t0 < 8f;) yield return null;
            yield return UiShowcase.Frames(6);
        }

        static IEnumerator Journey()
        {
            UiShowcaseNav.PadState(new GamepadState());   // (the virtual devices only: the person's own pad / keyboard / mouse are disabled in the test player)
            var events = OpenContent.Events.ToList();
            var nat = events.FirstOrDefault(e => e.ev.spec.type == "National" && !e.freeRide && e.ev.location == "NAT_Stillwater")
                      ?? events.FirstOrDefault(e => e.ev.spec.type == "National" && !e.freeRide);
            var sx = events.FirstOrDefault(e => e.ev.spec.type == "Supercross" && !e.freeRide);
            var fs = events.FirstOrDefault(e => e.ev.spec.type == "Freestyle" && !e.freeRide);
            Check(nat != null && sx != null && fs != null, "the install has no National / Supercross / Freestyle event");
            if (nat == null || sx == null || fs == null) yield break;

            // ---- a National: 12 by default, the pad steps it to 24
            yield return ToSetup(nat, 2, 0);
            var row = Q("ou-setup-riders");
            Check(row != null, "the setup page has no FIELD SIZE row");
            if (row == null) yield break;
            Debug.Log(Tag + "national default '" + Value() + "' note '" + Note() + "'");
            Check(Value() == "12 RIDERS  ·  ORIGINAL", "FIELD SIZE starts at the original's 12 (" + Value() + ")");
            Check(Setup.Riders(nat.ev) == RxRiderCap.Original, "the setup's field is 12 by default (" + Setup.Riders(nat.ev) + ")");
            Shell.Nav.Focus(row); yield return UiShowcase.Frames(6);
            yield return UiShowcase.Shot("ridercap-1-national-12");
            for (int i = 0; i < 12; i++) yield return Step(+1);
            yield return UiShowcase.Frames(20);
            Debug.Log(Tag + "national after 12 x right '" + Value() + "' riders " + Setup.Riders(nat.ev));
            Check(Value() == "24 RIDERS" && Setup.Riders(nat.ev) == 24, "12 presses right give 24 riders (" + Value() + ")");
            Check(Note().StartsWith("You and 23 computer riders"), "the note counts the computer riders (" + Note() + ")");
            Check(Shell.Root.Query<Label>().ToList().Any(l => l.text == "24 RIDERS"), "the summary's FIELD chip says 24 RIDERS");
            Check(Mathf.Approximately(Opacity(), 1f), "the row is not greyed on a National (" + Opacity() + ")");
            yield return UiShowcase.Shot("ridercap-2-national-24");
            for (int i = 0; i < 9; i++) yield return Step(+1);   // 24 -> 32 -> wraps to 2
            yield return UiShowcase.Frames(10);
            Check(Setup.Riders(nat.ev) == 2, "the row wraps from 32 to 2 (" + Setup.Riders(nat.ev) + ")");
            yield return Step(-1);
            Check(Setup.Riders(nat.ev) == RxRiderCap.Max, "left from 2 wraps to 32 (" + Setup.Riders(nat.ev) + ")");

            // ---- a Supercross at 32: the crowding warning
            yield return ToSetup(sx, 2, 32);
            Debug.Log(Tag + "supercross 32 '" + Value() + "' note '" + Note() + "'");
            Check(Value() == "32 RIDERS", "a Supercross takes 32 (" + Value() + ")");
            Check(Note().Contains("SUPERCROSS"), "the Supercross warning above 24 (" + Note() + ")");
            yield return UiShowcase.Shot("ridercap-3-supercross-32");
            Setup.riders = 24;
            Check(!Setup.CrowdedSupercross(sx.ev), "no warning at 24");

            // ---- a Freestyle: greyed, stays at 12 (the mode's tables were not opened past 12)
            yield return ToSetup(fs, 2, 24);
            row = Q("ou-setup-riders");
            Debug.Log(Tag + "freestyle '" + Value() + "' note '" + Note() + "' opacity " + Opacity());
            Check(Value() == "12 RIDERS  ·  THIS MODE'S MOST" && Setup.Riders(fs.ev) == 12, "a Freestyle stays at 12 even with 24 saved (" + Value() + ")");
            Check(Opacity() < 0.6f, "the Freestyle row is greyed (" + Opacity() + ")");
            if (row != null) { Shell.Nav.Focus(row); yield return UiShowcase.Frames(4); yield return Step(+1); }
            Check(Setup.Riders(fs.ev) == 12, "the Freestyle row does not move (" + Setup.Riders(fs.ev) + ")");
            yield return UiShowcase.Shot("ridercap-4-freestyle-12");

            // ---- AI off: YOU ALONE, greyed
            yield return ToSetup(nat, 0, 24);
            Check(Value() == "YOU ALONE" && Opacity() < 0.6f, "AI off greys the row (" + Value() + ", " + Opacity() + ")");
            yield return UiShowcase.Shot("ridercap-5-ai-off");

            // ---- the owned results page with 32 riders
            yield return Results32();
        }

        static OwnRaceResultsSnapshot Snapshot32()
        {
            const int n = 32;
            var race = new ReflexRaceState { valid = 1, state = 4, num_laps = 3, laps_completed = 3, place = 19, riders = n };
            var ui = new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState, activity_type = 0, race_state = 4, rules_hold = 0, post_state = 2 };
            var drivers = new ReflexDriverResult[n]; var names = new Dictionary<int, string>();
            string[] first = { "Josh", "Ali", "Melissa", "Cece", "Pat", "Ray", "Lena", "Tomas", "Kai", "Ivy", "Dante", "Rosa", "Milo", "Nina", "Theo", "Uma" };
            string[] last = { "Cooper", "Bouda", "Saito", "Harris", "Moreno", "Okafor", "Voss", "Reyes", "Anders", "Lang", "Cruz", "Ito" };
            for (int i = 0; i < n; i++)
            {
                drivers[i] = new ReflexDriverResult { slot = i, local = i == 18 ? 1 : 0, place = i + 1, laps_completed = 3, finished = 1, race_time = 301.2f + i * 2.4f, best_lap = 97.4f + i * 0.31f, last_lap = 98f };
                names[i] = i == 18 ? "Ferret" : first[i % first.Length] + " " + last[(i * 7) % last.Length];
            }
            if (!OwnRaceResultsSnapshot.TryCreate(1, "National/NAT_Showcase", true, race, ui, drivers, n, names, out var snapshot, out var reason)) throw new InvalidOperationException(reason);
            return snapshot;
        }

        static IEnumerator Results32()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase race results 32");
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;   // (no world: its Update would close it)
            OwnRaceResultsSnapshot snap = null;
            try { snap = Snapshot32(); } catch (Exception e) { Check(false, "a 32-rider results snapshot: " + e.Message); }
            if (snap == null) { UnityEngine.Object.Destroy(go); yield break; }
            Set(page, "snapshot", snap); Set(page, "active", true); Set(page, "armed", true);
            Call(page, "Build"); Call(page, "ShowResults", new object[] { null });
            UiShowcasePages.DriveOf(go, page);
            { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < 1.5f) yield return null; }   // (the board's entry motion, then the centring)
            var root = (VisualElement)Get(page, "root");
            int rows = root?.Query(className: "ou-board-row").ToList().Count ?? -1;
            bool big = root?.Q(className: "ou-results-big") != null;
            var scroll = root?.Q<ScrollView>(className: "ou-page-scroll");
            var you = root?.Q(className: "ou-board-you");
            bool youInView = scroll != null && you != null && scroll.contentViewport.worldBound.yMin <= you.worldBound.yMin && you.worldBound.yMax <= scroll.contentViewport.worldBound.yMax;
            Debug.Log(Tag + $"results: {rows} rows, big {big}, scroll {(scroll != null)}, offset {scroll?.scrollOffset}, local row in view {youInView}");
            Check(rows == 32, "32 rows on the results page (" + rows + ")");
            Check(big, "the big field's tighter rows (ou-results-big)");
            Check(youInView, "the local rider's row (19th) is wholly in view");
            yield return UiShowcase.Shot("ridercap-6-results-32");
            if (scroll != null) { scroll.scrollOffset = new Vector2(0, 100000); yield return UiShowcase.Frames(20); }
            yield return UiShowcase.Shot("ridercap-7-results-32-bottom");
            Set(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
            UiShowcase.ShellVisible(true);
        }
    }
}
