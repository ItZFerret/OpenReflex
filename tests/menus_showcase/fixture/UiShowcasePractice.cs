// TEST ONLY (w-lb-practice): PRACTICE in the front end and its in-race pages (core/plan/LEADERBOARDS.md 2.1, 2.3), from the product's own code.
// Page "practice": PLAY lists PRACTICE after RACE; PRACTICE's browser lists only the lap races (National, Supercross, Terracross, Powercross,
// UTV Moto) under its own header without the RACE / FREE RIDE switch; a National's practice setup: EVENT has the GHOST choice (saved, its note
// follows it) and MAP SCALE but no EVENT DURATION and no AI OPPONENTS, START PRACTICE hands a practice setup with no AI to the launch (the
// launch is captured: no race is ridden); CHANGE TRACK goes back to PRACTICE's browser. Then the in-race parts with synthetic laps: the lap
// list (RxPracticeHud) and the pause page (OwnModes.Pause: END PRACTICE, the lap tiles). The scratch save holds the settings.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.Practice;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePractice
    {
        const string Tag = "[ui-showcase] [practice] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("practice", Journey);

        static OpenShell Shell => OpenShell.Instance;
        static VisualElement Q(string name) => Shell.Root.Q(name);
        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "practice", what); Debug.Log(Tag + (ok ? "ok   " : "FAIL ") + what); }
        static string Value(string row) => Q(row)?.Q<Label>(className: "mxa-value")?.text ?? "(no row)";
        static string Text(string name) => (Q(name) as Label)?.text ?? Q(name)?.Q<Label>()?.text ?? "(none)";

        static IEnumerator Journey()
        {
            // ---- PLAY: the PRACTICE row after RACE
            Shell.ShowMain(); yield return UiShowcase.Frames(20);
            Shell.ShowPlay(); yield return UiShowcase.Frames(40);
            var rows = Shell.Layer.Query<VisualElement>().ToList().Where(e => e.name != null && e.name.StartsWith("ou-hub-") && e.name != "ou-hub-back").Select(e => e.name).Distinct().ToList();
            Debug.Log(Tag + "PLAY rows: " + string.Join(", ", rows));
            int race = rows.IndexOf("ou-hub-race"), practice = rows.IndexOf("ou-hub-practice");
            Check(practice >= 0 && practice == race + 1, "PLAY lists PRACTICE right after RACE");
            Shell.Nav.Focus(Q("ou-hub-practice")); yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("practice-1-play");

            // ---- PRACTICE's browser: the lap races only
            Shell.ShowBrowser(OpenBrowse.Practice); yield return UiShowcase.Frames(60);
            Shell.LoadAllThumbnails(); yield return UiShowcase.Frames(10);
            var cards = Shell.CardEvents.ToList();
            var types = cards.Select(e => e.activity).Distinct().ToList();
            Debug.Log(Tag + "browser: " + cards.Count + " cards, types " + string.Join(", ", types));
            Check(Shell.Practice && Shell.Current == OpenShell.Screen.Browser, "PRACTICE's browser is up in practice mode");
            Check(cards.Count > 0 && cards.All(OpenShell.PracticeEvent), "every card is a lap race");
            Check(Shell.Layer.Query<VisualElement>(className: "ou-seg-item").ToList().Count == 0, "no RACE / FREE RIDE switch");
            Check(Text("ou-setup-next").Contains("PRACTICE"), "the card's action is SET UP PRACTICE (" + Text("ou-setup-next") + ")");
            yield return UiShowcase.Shot("practice-2-browser");

            // ---- a National's practice setup
            var nat = cards.FirstOrDefault(e => e.activity == "National" && !e.Custom) ?? cards.First();
            Shell.SelectBrowse("National"); yield return UiShowcase.Frames(10);
            if (!Shell.SelectEvent(nat.key)) typeof(OpenShell).GetField("_browseEvent", All).SetValue(Shell, nat);
            yield return UiShowcase.Frames(10);
            Shell.ShowSetupPage(); yield return UiShowcase.Frames(50);
            Check(Shell.Setup != null && Shell.Setup.practice, "the setup is a practice setup");
            Check(Q("ou-setup-duration") == null && Q("ou-setup-ai") == null, "no EVENT DURATION and no AI OPPONENTS");
            Check(Q("ou-setup-mapscale") != null, "MAP SCALE stays");
            Check(Text("ou-setup-start").Contains("START PRACTICE"), "START PRACTICE (" + Text("ou-setup-start") + ")");
            yield return UiShowcase.Shot("practice-3-setup");
            typeof(OpenShell).GetMethod("SetSetupTab", All).Invoke(Shell, new object[] { 1, false }); yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("practice-5-setup-rider");

            // ---- START PRACTICE: the launch gets a practice setup with no AI (captured)
            OpenSetup launched = null;
            OpenSetup.LaunchOverride = s => launched = s;
            typeof(OpenShell).GetMethod("StartFromSetup", All).Invoke(Shell, null);
            yield return UiShowcase.Frames(10);
            OpenSetup.LaunchOverride = null;
            Check(launched != null && launched.practice && launched.Ai(launched.Event) == 0, "START PRACTICE launches a practice setup with no AI");
            Check(!OpenHistory.Recent().Any(r => r.eventKey == nat.key && r.practice), "a practice launch is not a RECENT race");

            // ---- back: CHANGE TRACK is PRACTICE's browser again
            Shell.ShowBrowser(OpenBrowse.Practice); yield return UiShowcase.Frames(20);
            Shell.ShowSetupPage(); yield return UiShowcase.Frames(30);
            typeof(OpenShell).GetMethod("BackToBrowser", All).Invoke(Shell, null); yield return UiShowcase.Frames(30);
            Check(Shell.Practice && Shell.Current == OpenShell.Screen.Browser, "CHANGE TRACK goes back to PRACTICE's browser");

            // ---- the lap list over a world (synthetic laps of a session)
            RxPractice.ShowcaseSession(Laps());
            UiShowcase.ShellVisible(false);
            var hudGo = new GameObject("showcase practice HUD");
            hudGo.AddComponent<RxPracticeHud>();
            yield return UiShowcase.Frames(30);
            RxPracticeHud.Banner("LAP NOT COUNTED  ·  OFF TRACK", new Color(0.886f, 0.675f, 0.251f));
            yield return UiShowcase.Frames(20);
            yield return UiShowcase.Shot("practice-6-laps");
            UnityEngine.Object.Destroy(hudGo);

            // ---- the pause page of a practice session
            var m = new ReflexHudModel { mode = HudMode.Race };
            var raceState = new ReflexRaceState { valid = 1, state = 2, num_laps = 0x7fffffff, laps_completed = 97, place = 1, riders = 1, race_time = 1310.4f };
            var spec = OwnModes.Pause(m, raceState, new OwnModeContext { eventLabel = "NATIONAL  ·  CONCORD", practice = true });
            Debug.Log(Tag + "pause: quit '" + spec.quitLabel + "', tiles " + string.Join(" | ", spec.tiles.Select(t => t.label + " " + t.value)));
            Check(spec.quitLabel == "END PRACTICE" && spec.restart, "the pause page has RESTART and END PRACTICE");
            Check(spec.tiles.Any(t => t.label == "LAP" && t.value == "12"), "the LAP tile is the lap log's (12), not laps_completed's cap");
            var page = PausePage(out var go, spec);
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("practice-7-pause");
            typeof(OwnRacePause).GetMethod("Confirm", All).Invoke(page, new object[] { false });
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("practice-8-end-confirm");
            typeof(OwnRacePause).GetField("open", All).SetValue(page, false); UnityEngine.Object.Destroy(go);
            RxPractice.ResetSession();
            UiShowcase.ShellVisible(true);
            yield return UiShowcase.Frames(3);
        }

        /// Eleven laps of a session: the gate lap, clean laps, a reset, an off-track lap, a cut and the best.
        static IEnumerable<RxLap> Laps()
        {
            var ms = new[] { 98304, 86150, 85575, 91212, 84038, 85602, 84547, 85768, 83990, 86205, 84401 };
            var why = new[] { RxPractice.FromTheGate, null, null, RxPractice.Reset, null, RxPractice.OffTrack, null, null, null, RxPractice.Cut, null };
            for (int i = 0; i < ms.Length; i++)
                yield return new RxLap { serial = (uint)(i + 1), flying = i > 0, ms = ms[i], raw = ms[i] / 1000f, invalid = why[i], practice = true };
        }

        static OwnRacePause PausePage(out GameObject go, OwnPauseSpec spec)
        {
            go = new GameObject("showcase practice pause");
            var page = go.AddComponent<OwnRacePause>(); page.enabled = false;   // (no world: its Update would close it)
            typeof(OwnRacePause).GetField("open", All).SetValue(page, true);
            UiShowcasePages.Call(page, "Build");
            UiShowcasePages.Call(page, "BuildMain", spec, "pause-resume");
            UiShowcasePages.DriveOf(go, page);
            return page;
        }
    }
}
