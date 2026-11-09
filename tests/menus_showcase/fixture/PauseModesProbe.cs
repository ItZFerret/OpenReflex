// TEST ONLY (tests/menus_showcase fixtures, run with tests/menu3d_motion/run_player.py --env REFLEX_PAUSE_SCRIPT=freeride,freestyle,waypoint): the pause pages
// of the offline modes in the real flow, driven by a VIRTUAL GAMEPAD (Input System state events, so the real bindings are in the loop). For each mode
// the probe starts the event through the shell's own launch (ReflexEventSelect.Launch), rides until the activity is InProgress, then
//   Start opens the pause page (the rows are the mode's, OwnModes), the d-pad / stick move the focus, A opens a row's page, B goes back, B on the main
//   page / Start resumes (the world runs again), and QUIT TO MENU (A, then right + A) goes back to the shell.
// Free Ride also: VEHICLE LIST, VEHICLE TUNING (the d-pad changes a value, the profile and the physics take it), the map's rider arrow;
// "discovery" (the Race & Ride challenges Free Ride): the Hit Discovery counter reaches the HUD model.
//   REFLEX_PAUSE_SCRIPT=freeride,freestyle,waypoint,section,terracross,powercross,national,challenge,discovery   the modes (in order)
//   REFLEX_PAUSE_SHOTS=<dir>                                                                                  screenshots (<mode>-<step>.png)
// Result lines: "[PauseProbe] PASS|FAIL ..." and "RESULT PASS|FAIL"; the player quits at the end.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    public sealed class PauseModesProbe : MonoBehaviour
    {
        static string Script => Environment.GetEnvironmentVariable("REFLEX_PAUSE_SCRIPT");
        string shots; Gamepad pad; Keyboard kb; int fails; OriginalWorldRuntime rt;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (string.IsNullOrEmpty(Script)) return;
            OpenShell.SkipIntro = true;   // (the hub's intro would hold the input)
            var g = new GameObject("pause modes probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<PauseModesProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_PAUSE_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;   // (the sandboxed window need not be the one in front for the virtual pad)
            StartCoroutine(Run());
        }

        void Log(string s) => Debug.Log("[PauseProbe] " + s);
        bool Check(bool ok, string what) { Log((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; return ok; }
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        IEnumerator Shot(string name)
        {
            yield return new WaitForEndOfFrame();
            if (shots != null) ScreenCapture.CaptureScreenshot(Path.Combine(shots, name + ".png"));
            yield return Frames(3);
            Log("shot " + name);
        }

        void Devices()
        {
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("pause probe pad");
            if (kb == null || !kb.added) kb = InputSystem.AddDevice<Keyboard>("pause probe keyboard");
        }
        IEnumerator Tap(GamepadButton b, int frames = 3)
        {
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState(b));
            yield return Frames(frames);
            InputSystem.QueueStateEvent(pad, new GamepadState());
            yield return Frames(4);
        }
        IEnumerator Stick(Vector2 v, float seconds)
        {
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState { leftStick = v });
            yield return Seconds(seconds);
            InputSystem.QueueStateEvent(pad, new GamepadState());
            yield return Frames(4);
        }
        void Throttle(bool on) { Devices(); InputSystem.QueueStateEvent(kb, on ? new KeyboardState(Key.W) : new KeyboardState()); }

        OwnRacePause Pause => rt ? rt.racePause : null;
        string FocusName()
        {
            var p = Pause; var root = p ? p.Root : null;
            var f = root?.panel?.focusController?.focusedElement as VisualElement;
            return f?.name ?? "(none)";
        }
        VisualElement Item(string name) => Pause?.Root?.Query<VisualElement>(name).First();
        string Label(string name) { var e = Item(name); return e?.Q<Label>()?.text; }

        static string KeyOf(string mode)
        {
            switch (mode)
            {
                case "freeride": return "FreeRide/FR_Alpine";
                case "discovery": return "HitDiscovery/FR_Alpine";
                case "challenge": return "HillClimb/FR_Alpine";
                case "freestyle": return ReflexEvents.All.First(e => e.spec.type == "Freestyle").key;
                case "waypoint": return ReflexEvents.All.First(e => e.spec.type == "WaypointRace").key;
                case "section": return ReflexEvents.All.First(e => e.spec.type == "SectionRace").key;
                case "terracross": return ReflexEvents.All.First(e => e.spec.type == "Terracross").key;
                case "powercross": return ReflexEvents.All.First(e => e.spec.type == "Powercross").key;
                case "national": return ReflexEvents.All.First(e => e.spec.type == "National").key;
                case "supercross": return ReflexEvents.All.First(e => e.spec.type == "Supercross").key;
            }
            return mode;
        }

        IEnumerator Run()
        {
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!Check(shell, "the shell started")) { Quit(); yield break; }
            Devices();
            yield return Seconds(2f);
            var modes = Script.Split(',').Select(m => m.Trim()).Where(m => m.Length > 0).ToList();
            foreach (var mode in modes)
            {
                string key = null; bool results = mode.EndsWith("-results", StringComparison.Ordinal);
                try { key = KeyOf(results ? mode.Substring(0, mode.Length - "-results".Length) : mode); } catch (Exception e) { Check(false, mode + ": no such event (" + e.Message + ")"); continue; }
                Log("== " + mode + " (" + key + ")");
                float t0 = Time.realtimeSinceStartup;
                ReflexEventSelect.Launch(key, results ? 2 : 0, false, "Reflex", 0, null);   // (the results runs have the AI: the standings of a full field)
                // the world: running and InProgress
                rt = null;
                bool popupTapped = false;
                for (float t = 0; t < 300f; t += Time.unscaledDeltaTime)
                {
                    rt ??= FindAnyObjectByType<OriginalWorldRuntime>();
                    if (rt && rt.Running && rt.EventUi.race_state == 2 && rt.EventUi.known != 0) break;
                    if (mode == "challenge" && rt && rt.preChallenge && rt.preChallenge.Active && !popupTapped)
                    {   // the challenge's rules popup: A accepts it (OnAcceptKeypress), the countdown follows
                        yield return Seconds(1.5f);
                        Log(mode + ": the rules popup is up (" + rt.preChallenge.Title + "); A accepts");
                        yield return Tap(GamepadButton.South); popupTapped = true;
                    }
                    yield return null;
                }
                if (!Check(rt && rt.Running && rt.EventUi.race_state == 2, mode + ": InProgress after " + (Time.realtimeSinceStartup - t0).ToString("F1") + " s (activity type " + (rt ? rt.EventUi.activity_type : -1) + ")")) { yield return Leave(); continue; }
                if (results) { yield return RunResults(mode); yield return Leave(); continue; }
                Throttle(true);
                yield return Seconds(mode == "challenge" ? 1f : 3.5f);
                Throttle(false);
                yield return Frames(10);
                if (mode == "discovery")
                {
                    Check(rt.HasRaceHud && rt.RaceHud.discoveryTotal > 0, mode + ": the Hit Discovery counter reaches the HUD (" + rt.RaceHud.discoveryFound + " / " + rt.RaceHud.discoveryTotal + ")");
                    yield return Shot("discovery-hud");
                }
                yield return RunPause(mode);
                yield return Leave();
            }
            Log(fails == 0 ? "RESULT PASS" : "RESULT FAIL (" + fails + ")");
            yield return Frames(30);
            Quit();
        }

        IEnumerator RunPause(string mode)
        {
            bool freeRide = mode == "freeride" || mode == "discovery";
            var p0 = rt.Bike.position != null && rt.Bike.position.Length >= 3 ? new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]) : Vector3.zero;
            // 1. Start (the pad) opens it
            yield return Tap(GamepadButton.Start);
            yield return Seconds(1f);
            if (!Check(Pause != null && Pause.IsOpen, mode + ": Start on the pad opened the pause page (" + (Pause == null ? "no pause component" : Pause.WhyNot() ?? "it could open") + "; pads " + Gamepad.all.Count + ", focused " + Application.isFocused + ")")) yield break;
            Check(Pause.Page == "main", mode + ": the main page (" + Pause.Page + ")");
            Check(rt.paused, mode + ": the world is paused");
            var rows = Pause.RowNames.ToList();
            Log(mode + ": rows " + string.Join(", ", rows) + "; tiles " + string.Join(" | ", Pause.Spec.tiles.Select(t => t.label + " " + t.value)));
            Check(rows.Contains("pause-resume") && rows.Contains("pause-tricks") && rows.Contains("pause-options") && rows.Contains("pause-quit"), mode + ": RESUME / TRICK LIBRARY / OPTIONS / QUIT are there");
            Check(rows.Contains("pause-restart") == !freeRide, mode + ": RESTART " + (freeRide ? "is not" : "is") + " there (the original: no RESTART in the Free Ride)");
            Check(rows.Contains("pause-vehicles") == freeRide, mode + ": VEHICLE LIST " + (freeRide ? "is" : "is not") + " there");
            Check(Pause.Spec.tiles.Count > 0, mode + ": the mode's stat tiles (" + Pause.Spec.tiles.Count + ")");
            Check(FocusName() == "pause-resume", mode + ": the focus starts on RESUME (" + FocusName() + ")");
            yield return Shot(mode + "-1-main");
            // 2. the d-pad and the stick move the focus
            yield return Tap(GamepadButton.DpadDown);
            string second = rows.Count > 1 ? rows[1] : "pause-resume";
            Check(FocusName() == second, mode + ": d-pad down -> " + second + " (" + FocusName() + ")");
            yield return Tap(GamepadButton.DpadUp);
            Check(FocusName() == "pause-resume", mode + ": d-pad up -> RESUME (" + FocusName() + ")");
            yield return Stick(new Vector2(0, -1), 0.2f);
            Check(FocusName() == second, mode + ": the left stick down -> " + second + " (" + FocusName() + ")");
            yield return Stick(new Vector2(0, 1), 0.2f);
            Check(FocusName() == "pause-resume", mode + ": the left stick up -> RESUME (" + FocusName() + ")");
            // 3. Free Ride: the map, the vehicle list, the tuning
            if (freeRide)
            {
                Check(Item("pause-map") != null && Pause.Spec.map, mode + ": the Free Ride map is on the page");
                Check(Item("pause-map-rider") != null, mode + ": the rider's arrow is on the map (position " + Pause.Spec.bikeX.ToString("F0") + ", " + Pause.Spec.bikeZ.ToString("F0") + ", heading " + Pause.Spec.heading.ToString("F0") + ")");
                {
                    yield return GoTo("pause-vehicles");
                    yield return Tap(GamepadButton.South); yield return Frames(20);
                    Check(Pause.Page == "vehicles", mode + ": A on VEHICLE LIST opens the list (" + Pause.Page + ")");
                    yield return Seconds(0.6f);
                    yield return Shot(mode + "-2-vehicles");
                    yield return Tap(GamepadButton.DpadDown); yield return Frames(10);
                    yield return Shot(mode + "-2b-vehicles");
                    yield return Tap(GamepadButton.East); yield return Frames(20);
                    Check(Pause.Page == "main" && FocusName() == "pause-vehicles", mode + ": B comes back to the main page on VEHICLE LIST (" + Pause.Page + ", " + FocusName() + ")");
                }
                yield return GoTo("pause-tuning");
                yield return Tap(GamepadButton.South); yield return Frames(20);
                Check(Pause.Page == "tuning", mode + ": A on VEHICLE TUNING opens it (" + Pause.Page + ")");
                if (Pause.Page == "tuning")
                {
                    // (engine / suspension / tires move the bonus points a part gives, a stock vehicle has none: braking steps 0 / 50 / 100)
                    for (int i = 0; i < 3 && FocusName() != "pause-tuning-3"; i++) { yield return Tap(GamepadButton.DpadDown); yield return Frames(6); }
                    string name = FocusName();
                    string before = Label(name) != null ? Item(name).Query<Label>().ToList().Last(l => l.ClassListContains("ou-tune2-verdict")).text : "?";
                    yield return Tap(GamepadButton.DpadRight); yield return Frames(10);
                    string after = Item(name).Query<Label>().ToList().Last(l => l.ClassListContains("ou-tune2-verdict")).text;
                    Log(mode + ": tuning row " + name + " " + before + " -> " + after);
                    Check(before != after, mode + ": the d-pad changed the tuning value (" + before + " -> " + after + ")");
                    yield return Tap(GamepadButton.DpadLeft); yield return Frames(10);   // (back to what it was)
                    yield return Shot(mode + "-3-tuning");
                    yield return Tap(GamepadButton.East); yield return Frames(20);
                    Check(Pause.Page == "main" && FocusName() == "pause-tuning", mode + ": B comes back to the main page on VEHICLE TUNING (" + Pause.Page + ", " + FocusName() + ")");
                }
            }
            // 4. TRICK LIBRARY
            yield return GoTo("pause-tricks");
            yield return Tap(GamepadButton.South); yield return Frames(30);
            Check(Pause.Page == "tricks", mode + ": A on TRICK LIBRARY opens it (" + Pause.Page + ")");
            Check(Pause.TrickCount > 0, mode + ": the trick library lists " + Pause.TrickCount + " tricks");
            yield return Tap(GamepadButton.DpadDown); yield return Tap(GamepadButton.DpadDown);
            yield return Tap(GamepadButton.East); yield return Frames(20);
            Check(Pause.Page == "main" && FocusName() == "pause-tricks", mode + ": B leaves the trick library (" + Pause.Page + ", " + FocusName() + ")");
            // 5. RESTART / QUIT dialogs (cancelled with B)
            if (rows.Contains("pause-restart"))
            {
                yield return GoTo("pause-restart");
                yield return Tap(GamepadButton.South); yield return Frames(20);
                Check(Pause.Page == "restart" && FocusName() == "pause-cancel", mode + ": A on RESTART asks first, the focus on the safe answer (" + Pause.Page + ", " + FocusName() + ")");
                yield return Tap(GamepadButton.DpadRight); yield return Frames(5);
                Check(FocusName() == "pause-confirm", mode + ": d-pad right reaches RESTART (" + FocusName() + ")");
                yield return Tap(GamepadButton.East); yield return Frames(20);
                Check(Pause.Page == "main" && FocusName() == "pause-restart", mode + ": B cancels the restart (" + Pause.Page + ", " + FocusName() + ")");
            }
            yield return GoTo("pause-quit");
            yield return Tap(GamepadButton.South); yield return Frames(20);
            Check(Pause.Page == "quit" && FocusName() == "pause-cancel", mode + ": A on QUIT asks first (" + Pause.Page + ", " + FocusName() + ")");
            yield return Shot(mode + "-4-quit");
            yield return Tap(GamepadButton.East); yield return Frames(20);
            Check(Pause.Page == "main", mode + ": B cancels the quit (" + Pause.Page + ")");
            // 6. B on the main page resumes; the world runs again and the rider moves; Start opens it again and closes it
            yield return GoTo("pause-resume");
            yield return Tap(GamepadButton.East); yield return Frames(20);
            Check(!Pause.IsOpen && !rt.paused, mode + ": B on the main page resumes (open " + Pause.IsOpen + ", paused " + rt.paused + ")");
            Throttle(true);
            yield return Seconds(2.5f);
            Throttle(false);
            var p1 = rt.Bike.position != null && rt.Bike.position.Length >= 3 ? new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]) : Vector3.zero;
            if (mode != "challenge") Check((p1 - p0).magnitude > 5f, mode + ": the world runs again after RESUME (moved " + (p1 - p0).magnitude.ToString("F0") + " ft)");
            yield return Tap(GamepadButton.Start); yield return Seconds(0.8f);
            Check(Pause.IsOpen, mode + ": Start opens it again");
            yield return Tap(GamepadButton.Start); yield return Frames(20);
            Check(!Pause.IsOpen && !rt.paused, mode + ": Start closes it (open " + Pause.IsOpen + ", paused " + rt.paused + ")");
            // 7. Start, then QUIT -> A, right, A: back to the shell (a challenge: to its Free Ride)
            yield return Tap(GamepadButton.Start); yield return Seconds(0.8f);
            yield return GoTo("pause-quit");
            yield return Tap(GamepadButton.South); yield return Frames(20);
            yield return Tap(GamepadButton.DpadRight); yield return Frames(5);
            Check(FocusName() == "pause-confirm", mode + ": the quit's confirm button is reachable with the d-pad (" + FocusName() + ")");
            yield return Tap(GamepadButton.South);
            if (mode == "challenge")
            {   // QUIT of a challenge goes back to the Free Ride it was started in (App.QuitToFreeRide), not to the menu
                for (float t = 0; t < 90f; t += Time.unscaledDeltaTime)
                {
                    var now = FindAnyObjectByType<OriginalWorldRuntime>();
                    if (now && now != rt && now.Running && (now.EventIdentity?.StartsWith("FreeRide/") ?? false)) { rt = now; break; }
                    yield return null;
                }
                Check(rt && (rt.EventIdentity?.StartsWith("FreeRide/") ?? false), mode + ": QUIT TO FREE RIDE came back to the Free Ride (" + rt?.EventIdentity + ")");
            }
        }

        // a timed mode's own end (a Freestyle run / a Section Race lasts a minute): the results page opens by itself, then the pad drives it
        IEnumerator RunResults(string mode)
        {
            var spec0 = Pause;
            float t0 = Time.realtimeSinceStartup;
            // (the rider idles; a pad press now and then keeps nothing alive: the activity ends by its clock)
            for (float t = 0; t < 420f && !(rt.raceResults && rt.raceResults.Active); t += Time.unscaledDeltaTime) yield return null;
            var page = rt.raceResults;
            if (!Check(page != null && page.Active, mode + ": the results page opened by itself after " + (Time.realtimeSinceStartup - t0).ToString("F0") + " s (race state " + rt.EventUi.race_state + ", post " + rt.EventUi.post_state + ")")) yield break;
            yield return Seconds(3f);   // (the page's own settle: it takes input once the buttons that opened the world are released)
            Check(page.Page == "results", mode + ": the results page (" + page.Page + ")");
            Check(page.Snapshot != null && page.Snapshot.Rows.Count > 0, mode + ": " + (page.Snapshot?.Rows.Count ?? 0) + " rows in the table, mode " + page.Snapshot?.Mode);
            if (page.Snapshot != null) foreach (var row in page.Snapshot.Rows) Log(mode + ": row " + row.Place + " " + row.Name + (row.IsLocal ? " (you)" : "") + " " + (row.HasModeRow ? "score " + row.ModeRow.score.ToString("F2") + " sections " + row.ModeRow.sectionsWon + " best " + row.ModeRow.BestTrick : row.Time));
            Check(rt.paused || rt.OwnMenuActive, mode + ": the world waits behind the page");
            Check(FocusOf(page) == "results-again", mode + ": the focus starts on the first button (" + FocusOf(page) + ")");
            yield return Shot(mode + "-1");
            yield return Tap(GamepadButton.DpadDown); yield return Frames(6);
            Check(FocusOf(page) == "results-menu", mode + ": d-pad down -> RETURN TO MENU (" + FocusOf(page) + ")");
            yield return Tap(GamepadButton.DpadUp); yield return Frames(6);
            yield return Tap(GamepadButton.South); yield return Frames(20);
            Check(page.Page == "restart" && FocusOf(page) == "results-cancel", mode + ": A on the first button asks first (" + page.Page + ", " + FocusOf(page) + ")");
            yield return Tap(GamepadButton.East); yield return Frames(20);
            Check(page.Page == "results", mode + ": B cancels (" + page.Page + ")");
            yield return Tap(GamepadButton.DpadLeft); yield return Frames(10);
            string row0 = FocusOf(page);
            Check(row0.StartsWith("result-slot-"), mode + ": d-pad left reaches the board (" + row0 + ")");
            if (page.Snapshot.Rows.Count > 1)
            {
                yield return Tap(GamepadButton.DpadDown); yield return Frames(8);
                if (FocusOf(page) == row0) { yield return Tap(GamepadButton.DpadUp); yield return Frames(8); }   // (the local rider's row can be the last one)
                Check(FocusOf(page) != row0, mode + ": the d-pad scrolls the board (" + FocusOf(page) + ")");
            }
            else Log(mode + ": a board of one row: nothing to scroll");
            yield return Tap(GamepadButton.DpadRight); yield return Frames(10);
            Check(FocusOf(page) == "results-again", mode + ": d-pad right comes back to the buttons (" + FocusOf(page) + ")");
            yield return Tap(GamepadButton.DpadDown); yield return Frames(6);
            yield return Tap(GamepadButton.South); yield return Frames(20);
            Check(page.Page == "quit", mode + ": A on RETURN TO MENU asks first (" + page.Page + ")");
            yield return Tap(GamepadButton.DpadRight); yield return Frames(6);
            Check(FocusOf(page) == "results-confirm", mode + ": the confirm button is reachable (" + FocusOf(page) + ")");
            yield return Tap(GamepadButton.South);
        }
        static string FocusOf(OwnRaceResults page)
        {
            var f = page?.Root?.panel?.focusController?.focusedElement as VisualElement;
            return f?.name ?? "(none)";
        }

        // the d-pad walks the main page's rows down to the named one
        IEnumerator GoTo(string row)
        {
            for (int i = 0; i < 10 && FocusName() != row; i++)
            {
                bool down = Pause.RowNames.ToList().IndexOf(row) > Pause.RowNames.ToList().IndexOf(FocusName());
                yield return Tap(down ? GamepadButton.DpadDown : GamepadButton.DpadUp); yield return Frames(3);
            }
            Check(FocusName() == row, "the d-pad reaches " + row + " (on " + FocusName() + ")");
        }

        // after the QUIT: the shell is back
        IEnumerator Leave()
        {
            for (float t = 0; t < 60f; t += Time.unscaledDeltaTime)
            {
                var s = OpenShell.Instance;
                if (s && !FindAnyObjectByType<OriginalWorldRuntime>()) break;
                yield return null;
            }
            if (FindAnyObjectByType<OriginalWorldRuntime>() && rt) { OriginalPauseMenu.QuitToFrontEnd(); yield return Seconds(4f); }   // (a challenge's quit went to its Free Ride: on to the menu)
            Check(OpenShell.Instance != null, "back in the shell");
            yield return Seconds(2.5f);
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
