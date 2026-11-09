// TEST ONLY (tests/menus_showcase fixtures, run with tests/menu3d_motion/run_player.py --env REFLEX_RACEFLOW_SCRIPT=waypoint,terracross,national):
// the start of an event in the real flow (w-raceflow). For each mode the probe starts the event through the shell's own launch with two AI riders,
//   1. waits for the Event Intro card, presses A on a VIRTUAL GAMEPAD (the card's own SKIP INTRO) and logs how long the intro and the start take,
//   2. logs every change of the start countdown the MODERN HUD reads (race state, the board's start_timer, the activity's countdown_step) and takes
//      screenshots of the countdown,
//   3. at InProgress opens the pause page (Start) and logs its rows; REFLEX_RACEFLOW_RESTART=1 also restarts the event from the pause page and
//      logs the rows of the restarted event's pause page.
//   REFLEX_RACEFLOW_SHOTS=<dir>   screenshots (<mode>-<what>.png)
// Result lines: "[RaceFlow] PASS|FAIL ..." and "RESULT PASS|FAIL"; the player quits at the end.
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    public sealed class RaceFlowProbe : MonoBehaviour
    {
        static string Script => Environment.GetEnvironmentVariable("REFLEX_RACEFLOW_SCRIPT");
        string shots; Gamepad pad; int fails; OriginalWorldRuntime rt;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (string.IsNullOrEmpty(Script)) return;
            OpenShell.SkipIntro = true;
            var g = new GameObject("race flow probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<RaceFlowProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_RACEFLOW_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            StartCoroutine(Run());
        }

        void Log(string s) => Debug.Log("[RaceFlow] " + s);
        bool Check(bool ok, string what) { Log((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; return ok; }
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        IEnumerator Shot(string name)
        {
            yield return new WaitForEndOfFrame();
            if (shots != null) ScreenCapture.CaptureScreenshot(Path.Combine(shots, name + ".png"));
            yield return Frames(2);
            Log("shot " + name);
        }
        IEnumerator Tap(GamepadButton b)
        {
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("race flow probe pad");
            InputSystem.QueueStateEvent(pad, new GamepadState(b));
            yield return Frames(3);
            InputSystem.QueueStateEvent(pad, new GamepadState());
            yield return Frames(4);
        }

        static string KeyOf(string mode)
        {
            switch (mode)
            {
                case "waypoint": return ReflexEvents.All.First(e => e.spec.type == "WaypointRace").key;
                case "terracross": return ReflexEvents.All.First(e => e.spec.type == "Terracross").key;
                case "national": return ReflexEvents.All.First(e => e.spec.type == "National").key;
                case "supercross": return ReflexEvents.All.First(e => e.spec.type == "Supercross").key;
                case "freestyle": return ReflexEvents.All.First(e => e.spec.type == "Freestyle").key;
                case "section": return ReflexEvents.All.First(e => e.spec.type == "SectionRace").key;
            }
            return mode;
        }

        IEnumerator Run()
        {
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!Check(shell, "the shell started")) { Quit(); yield break; }
            yield return Seconds(2f);
            foreach (var mode in Script.Split(',').Select(m => m.Trim()).Where(m => m.Length > 0))
            {
                string key;
                try { key = KeyOf(mode); } catch (Exception e) { Check(false, mode + ": no such event (" + e.Message + ")"); continue; }
                Log("== " + mode + " (" + key + ")");
                ReflexEventSelect.Launch(key, 2, false, "Reflex", 0, null);
                yield return Event(mode, true);
                if (Environment.GetEnvironmentVariable("REFLEX_RACEFLOW_RESTART") == "1" && rt && rt.Running)
                {
                    yield return Tap(GamepadButton.Start); yield return Seconds(1f);
                    var p = rt.racePause;
                    if (Check(p && p.IsOpen && p.RowNames.Contains("pause-restart"), mode + ": the pause page offers RESTART"))
                    {
                        var old = rt;
                        for (int i = 0; i < 10 && p.RowNames.ToList().IndexOf("pause-restart") >= 0 && FocusName(p) != "pause-restart"; i++) yield return Tap(GamepadButton.DpadDown);
                        yield return Tap(GamepadButton.South); yield return Frames(10);
                        yield return Tap(GamepadButton.DpadRight); yield return Frames(5);
                        yield return Tap(GamepadButton.South);
                        Log(mode + ": RESTART confirmed");
                        yield return Seconds(3f);
                        yield return Event(mode + "-restarted", false);
                    }
                }
                OriginalPauseMenu.QuitToFrontEnd();
                for (float t = 0; t < 60f && FindAnyObjectByType<OriginalWorldRuntime>(); t += Time.unscaledDeltaTime) yield return null;
                yield return Seconds(2.5f);
            }
            Log(fails == 0 ? "RESULT PASS" : "RESULT FAIL (" + fails + ")");
            yield return Frames(30);
            Quit();
        }

        static string FocusName(OwnRacePause p) => (p?.Root?.panel?.focusController?.focusedElement as UnityEngine.UIElements.VisualElement)?.name ?? "(none)";

        // one event from its intro to InProgress, then its pause page
        IEnumerator Event(string mode, bool skip)
        {
            float t0 = Time.realtimeSinceStartup;
            rt = null;
            for (float t = 0; t < 240f; t += Time.unscaledDeltaTime)
            {
                rt = FindAnyObjectByType<OriginalWorldRuntime>();
                if (rt && rt.Running && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                yield return null;
            }
            if (!Check(rt && rt.Running, mode + ": the world runs")) yield break;
            Log(mode + ": intro card " + rt.IntroCardShown + " at " + (Time.realtimeSinceStartup - t0).ToString("F1") + " s (scenario " + rt.ScenarioKind + ", state " + rt.EventUi.race_state + ")");
            float skipAt = -1f;
            if (skip && rt.IntroCardShown)
            {
                yield return Seconds(2f);
                yield return Shot(mode + "-0-intro");
                skipAt = Time.realtimeSinceStartup;
                yield return Tap(GamepadButton.South);
                Log(mode + ": A pressed on the intro card (skips sent " + OwnEventIntro.Skips + ")");
                yield return Seconds(1f);
                if (rt && rt.IntroCardShown) yield return Shot(mode + "-0-skipped");
            }
            var m = new ReflexHudModel();
            string last = null; float countdownAt = -1f, cardGoneAt = -1f; bool shot1 = false, shot2 = false;
            for (float t = 0; t < 90f; t += Time.unscaledDeltaTime)
            {
                if (!rt || !rt.Running) break;
                m.Fill(rt, Time.unscaledDeltaTime);
                if (cardGoneAt < 0 && skipAt > 0 && !rt.IntroCardShown) cardGoneAt = Time.realtimeSinceStartup;
                string now = "state " + m.state + " scenario " + rt.ScenarioKind + " step " + m.countdownStep + " board " + (m.state == 1 ? Mathf.CeilToInt(m.startTimer).ToString() : "-") + " card " + rt.IntroCardShown;
                if (now != last) { Log(mode + ": " + (Time.realtimeSinceStartup - t0).ToString("F2") + " s " + now); last = now; }
                if (m.state == 1 && countdownAt < 0) countdownAt = Time.realtimeSinceStartup;
                if (m.state == 1 && !shot1 && Time.realtimeSinceStartup - countdownAt > 0.3f) { shot1 = true; yield return Shot(mode + "-1-countdown"); }
                if (m.state == 1 && !shot2 && Time.realtimeSinceStartup - countdownAt > 2.4f) { shot2 = true; yield return Shot(mode + "-2-countdown"); }
                if (m.state == 2) break;
                yield return null;
            }
            if (skipAt > 0) Log(mode + ": after the skip: the card went in " + (cardGoneAt > 0 ? (cardGoneAt - skipAt).ToString("F1") : "-") + " s, the countdown came in " + (countdownAt > 0 ? (countdownAt - skipAt).ToString("F1") : "-") + " s");
            if (!Check(rt && rt.Running && rt.EventUi.race_state == 2, mode + ": InProgress")) yield break;
            yield return Seconds(1.5f);
            yield return Tap(GamepadButton.Start); yield return Seconds(1f);
            var p = rt.racePause;
            if (Check(p && p.IsOpen, mode + ": the pause page opened (" + (p ? p.WhyNot() : "no page") + ")"))
            {
                Log(mode + ": pause rows " + string.Join(", ", p.RowNames) + " (mode " + p.Spec?.mode + ", identity " + rt.EventIdentity + ", waypoint " + rt.IsWaypoint + "/" + rt.HasWaypoint + ")");
                Check(p.RowNames.Contains("pause-restart"), mode + ": RESTART is on the pause page");
                yield return Shot(mode + "-3-pause");
                yield return Tap(GamepadButton.Start); yield return Frames(20);
            }
        }

        static void Quit()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}
