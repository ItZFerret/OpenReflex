// TEST ONLY: the owned pause and results pages of every offline mode (w-pausemodes), built with synthetic data: a HUD model and a context per
// mode go through OwnModes.Pause / OwnModes.Results, and the pages draw the specs as in a race. Pages: pause (a lap race), pause-eliminator,
// pause-outrun, pause-waypoint, pause-section, pause-freestyle, pause-freeride (the map), pause-freeride-vehicles, pause-freeride-vehicle-confirm,
// pause-freeride-tuning, pause-challenge, pause-career, pause-restart-confirm, results-waypoint, results-freestyle, results-section,
// results-race-penalty, results-eliminator.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePauseModes
    {
        const BindingFlags All = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("pause-eliminator", () => ModePause("pause-eliminator", Eliminator()));
            UiShowcase.Register("pause-outrun", () => ModePause("pause-outrun", Outrun()));
            UiShowcase.Register("pause-waypoint", () => ModePause("pause-waypoint", Waypoint()));
            UiShowcase.Register("pause-section", () => ModePause("pause-section", Section()));
            UiShowcase.Register("pause-freestyle", () => ModePause("pause-freestyle", Freestyle()));
            UiShowcase.Register("pause-freeride", () => ModePause("pause-freeride", FreeRide()));
            UiShowcase.Register("pause-freeride-vehicles", FreeRideVehicles);
            UiShowcase.Register("pause-freeride-tuning", FreeRideTuning);
            UiShowcase.Register("pause-challenge", () => ModePause("pause-challenge", Challenge()));
            UiShowcase.Register("pause-career", () => ModePause("pause-career", Career()));
            UiShowcase.Register("pause-restart-confirm", RestartConfirm);
            UiShowcase.Register("nav-pause", PauseJourney);
            UiShowcase.Register("nav-results", ResultsJourney);
            UiShowcase.Register("nav-original-pause", OriginalPauseJourney);
            UiShowcase.Register("nav-original-post", OriginalPostJourney);
            UiShowcase.Register("results-waypoint", () => Results("results-waypoint", HudMode.Waypoint));
            UiShowcase.Register("results-freestyle", () => Results("results-freestyle", HudMode.Freestyle));
            UiShowcase.Register("results-section", () => Results("results-section", HudMode.SectionRace));
            UiShowcase.Register("results-race-penalty", () => Results("results-race-penalty", HudMode.Race));
            UiShowcase.Register("results-eliminator", () => Results("results-eliminator", HudMode.Eliminator));
        }

        // ------------------------------------------------------------------------------------------------ the pause specs

        /// The lap race of the existing "pause" page: lap 2 of 3, third of 12.
        internal static OwnPauseSpec Race()
        {
            var m = new ReflexHudModel { mode = HudMode.Race };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 3, laps_completed = 1, place = 3, riders = 12, race_time = 131.4f, best_lap = 97.8f };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "NATIONAL  ·  CONCORD" });
        }
        static OwnPauseSpec Eliminator()
        {
            var m = new ReflexHudModel { mode = HudMode.Eliminator, ridersLeft = 5, ridersTotal = 12 };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 8, laps_completed = 3, place = 2, riders = 12, race_time = 211.9f, best_lap = 55.2f };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "ELIMINATOR  ·  CAREER" });
        }
        static OwnPauseSpec Outrun()
        {
            var m = new ReflexHudModel { mode = HudMode.Outrun, gateShown = true, gateNumber = 3, gateTotal = 6, outrunTimerShown = true, outrunSectionTime = 14.3f };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 1, laps_completed = 0, place = 4, riders = 8, race_time = 74.1f, best_lap = float.MaxValue };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "OUTRUN  ·  CAREER" });
        }
        static OwnPauseSpec Waypoint()
        {
            var m = new ReflexHudModel { mode = HudMode.Waypoint, hasWaypoint = true, gateCount = 12, gatesPassed = 5, waypointDistanceValid = true, routeDistance = 842f };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 1, laps_completed = 0, place = 1, riders = 1, race_time = 95.7f, best_lap = float.MaxValue };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "WAYPOINT RACE  ·  ALPINE" });
        }
        static OwnPauseSpec Section()
        {
            var m = new ReflexHudModel { mode = HudMode.SectionRace, sectionShown = true, sectionCount = 10, sectionCurrent = 3, sectionTime = 8.42f, sectionBest = 7.91f, timerShown = true, timerValue = 38.2f };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 1, laps_completed = -1, place = 2, riders = 12 };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "SECTION RACE  ·  CONCORD B", hasLocal = true, local = RxModeResultRow.Make(0, true, 2, false, 21.8f, sectionsWon: 2, bestSection: 1, bestSectionTime: 7.91f) });
        }
        static OwnPauseSpec Freestyle()
        {
            var m = new ReflexHudModel { mode = HudMode.Freestyle, timerShown = true, timerValue = 33.6f };
            m.judgeBars[0] = 7; m.judgeBars[1] = 5; m.judgeBars[2] = 6; m.judgeBars[3] = 4;
            var race = new ReflexRaceState { valid = 1, state = 2, riders = 12 };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "FREESTYLE  ·  BUENA VISTA", hasLocal = true, local = RxModeResultRow.Make(0, true, 1, false, score: 61.37f, bestTrickScore: 8.74f, trick: "STR_RODEO") });
        }
        static OwnPauseSpec FreeRide()
        {
            var m = new ReflexHudModel { mode = HudMode.FreeRide, discoveryFound = 3, discoveryTotal = 9, heading = 47f };
            var race = new ReflexRaceState { valid = 1, state = 2, riders = 1 };
            int icon = -1; try { icon = OwnModes.MapIcon(ReflexEvents.Find("FreeRide/FR_Alpine")); } catch (Exception e) { Debug.Log("[ui-showcase] map icon: " + e.Message); }
            return OwnModes.Pause(m, race, new OwnModeContext
            {
                eventLabel = "FREE RIDE  ·  ALPINE", location = "ALPINE", vehicle = "BLAZE 250R", speed = "38 MPH", ownVehicle = true, mapIcon = icon,
                hasBike = true, bikeX = 420f, bikeZ = -640f, heading = 47f,
            });
        }
        static OwnPauseSpec Challenge()
        {
            var m = new ReflexHudModel { mode = HudMode.Challenge, challengeKind = 1, challengeValueValid = true, challengeValue = 148f, challengeMedal = 2, timerShown = false };
            m.challengeCriteria[0] = 65f; m.challengeCriteria[1] = 130f; m.challengeCriteria[2] = 200f;
            var race = new ReflexRaceState { valid = 1, state = 2, riders = 1 };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "HILL CLIMB  ·  ALPINE", challengeWorld = true, activation = "HillClimb", ownVehicle = true, location = "ALPINE" });
        }
        static OwnPauseSpec Career()
        {
            var m = new ReflexHudModel { mode = HudMode.Race };
            var race = new ReflexRaceState { valid = 1, state = 2, num_laps = 2, laps_completed = 0, place = 5, riders = 8, race_time = 44.0f };
            return OwnModes.Pause(m, race, new OwnModeContext { eventLabel = "MOTOCAREER  ·  SOUTHWICK", career = true });
        }

        // ------------------------------------------------------------------------------------------------ the pause pages

        static OwnRacePause Page(out GameObject go, OwnPauseSpec spec, string focus = "pause-resume")
        {
            UiShowcase.ShellVisible(false);
            go = new GameObject("showcase pause");
            var page = go.AddComponent<OwnRacePause>(); page.enabled = false;   // (no world: its Update would close it)
            var open = typeof(OwnRacePause).GetField("open", All); open.SetValue(page, true);
            UiShowcasePages.Call(page, "Build");
            UiShowcasePages.Call(page, "BuildMain", spec, focus);
            UiShowcasePages.DriveOf(go, page);
            return page;
        }
        static void End(GameObject go, OwnRacePause page) { typeof(OwnRacePause).GetField("open", All).SetValue(page, false); UnityEngine.Object.Destroy(go); }

        static IEnumerator ModePause(string shot, OwnPauseSpec spec)
        {
            var page = Page(out var go, spec);
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot(shot);
            End(go, page);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator RestartConfirm()
        {
            var page = Page(out var go, Race(), "pause-restart");
            UiShowcasePages.Call(page, "Confirm", true);
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("pause-restart-confirm");
            End(go, page);
            yield return UiShowcase.Frames(3);
        }

        // a scratch profile with the garage's own tables (the garage pages of the showcase do the same)
        static void Profile()
        {
            ReflexProfile.Disabled = false;
            if (ReflexProfile.Current == null)
            {
                if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
                if (!ReflexProfile.Open().Result) throw new InvalidOperationException("no profile");
            }
        }

        static List<GarageItem> SomeVehicles()
        {
            var list = OpenGarageData.Vehicles();
            var shown = new List<GarageItem>();
            int n = 0;
            foreach (var v in list)
            {
                if (shown.Count >= 7) break;
                var c = v.Clone(); c.owned = true; c.playerVehicle = 200 + n; c.equipped = n == 1; n++;
                shown.Add(c);
            }
            return shown;
        }

        static IEnumerator FreeRideVehicles()
        {
            Profile();
            var page = Page(out var go, FreeRide());
            typeof(OwnRacePause).GetField("VehicleSource", All).SetValue(page, (Func<List<GarageItem>>)SomeVehicles);
            UiShowcasePages.Call(page, "ShowVehicles", new object[] { null });
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("pause-freeride-vehicles");
            // the confirmation of a change
            var items = SomeVehicles();
            UiShowcasePages.Call(page, "ChooseVehicle", items[3]);
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("pause-freeride-vehicle-confirm");
            End(go, page);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator FreeRideTuning()
        {
            Profile();
            var page = Page(out var go, FreeRide());
            UiShowcasePages.Call(page, "ShowTuning");
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("pause-freeride-tuning");
            End(go, page);
            yield return UiShowcase.Frames(3);
        }

        // ------------------------------------------------------------------------------------------------ the journeys (the pad on the pages)

        static string Focused() => UiShowcaseNav.Look().name;

        /// The Free Ride's pause with the pad: A opens a row's page, B comes back to that row, the d-pad changes the tuning, QUIT asks first.
        static IEnumerator PauseJourney()
        {
            Profile();
            var page = Page(out var go, FreeRide());
            yield return UiShowcase.Frames(40);
            UiShowcaseNav.Check(Focused() == "pause-resume", "pause journey", "the pause page did not open on RESUME (" + Focused() + ")");
            var rows = page.RowNames.ToList();
            UiShowcaseNav.Check(rows.SequenceEqual(new[] { "pause-resume", "pause-tuning", "pause-vehicles", "pause-tricks", "pause-options", "pause-quit" }), "pause journey", "the Free Ride rows are " + string.Join(", ", rows));
            // VEHICLE LIST
            typeof(OwnRacePause).GetField("VehicleSource", All).SetValue(page, (Func<List<GarageItem>>)SomeVehicles);
            yield return Down(rows, "pause-vehicles");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "vehicles", "pause journey", "A on VEHICLE LIST did not open it (" + page.Page + ")");
            string at = Focused();
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(8);
            UiShowcaseNav.Check(Focused() != at, "pause journey", "the d-pad did not move down the vehicle list (" + at + " -> " + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "vehicle-confirm", "pause journey", "A on a vehicle did not ask first (" + page.Page + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "vehicles", "pause journey", "B on the vehicle question did not come back to the list (" + page.Page + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "main" && Focused() == "pause-vehicles", "pause journey", "B on the list did not return to VEHICLE LIST (" + page.Page + ", " + Focused() + ")");
            // VEHICLE TUNING
            yield return Down(rows, "pause-tuning");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "tuning", "pause journey", "A on VEHICLE TUNING did not open it (" + page.Page + ")");
            // (engine / suspension / tires move bonus points a part gives: a stock vehicle has none; braking and steering step 0 / 50 / 100)
            for (int i = 0; i < 3 && Focused() != "pause-tuning-3"; i++) { yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(4); }
            UiShowcaseNav.Check(Focused() == "pause-tuning-3", "pause journey", "the d-pad did not reach the BRAKING row (" + Focused() + ")");
            string row = Focused();
            string TuneText() { var r = page.Root.Query<VisualElement>(row).First(); return r == null ? "?" : r.Query<Label>().ToList().Last(l => l.ClassListContains("ou-tune2-verdict")).text; }
            string was = TuneText();
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadRight); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(TuneText() != was, "pause journey", "the d-pad did not change the tuning (" + was + " -> " + TuneText() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "main" && Focused() == "pause-tuning", "pause journey", "B on the tuning did not return to its row (" + page.Page + ", " + Focused() + ")");
            // TRICK LIBRARY
            yield return Down(rows, "pause-tricks");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(40);
            UiShowcaseNav.Check(page.Page == "tricks", "pause journey", "A on TRICK LIBRARY did not open it (" + page.Page + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "main" && Focused() == "pause-tricks", "pause journey", "B did not leave the trick library (" + page.Page + ", " + Focused() + ")");
            // QUIT asks first, the safe answer is focused, right reaches the other, B cancels
            yield return Down(rows, "pause-quit");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "quit" && Focused() == "pause-cancel", "pause journey", "A on QUIT did not ask first on the safe answer (" + page.Page + ", " + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadRight); yield return UiShowcase.Frames(8);
            UiShowcaseNav.Check(Focused() == "pause-confirm", "pause journey", "the d-pad did not reach the QUIT button (" + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(page.Page == "main" && Focused() == "pause-quit", "pause journey", "B did not cancel the quit (" + page.Page + ", " + Focused() + ")");
            End(go, page);
            yield return UiShowcase.Frames(3);
        }

        /// The fallback page (the install's art, OriginalPauseMenu) with the pad: Start-less here (it opens from the world's key), then the d-pad walks the rows,
        /// A on RESTART asks first, B closes the question, Start resumes.
        static IEnumerator OriginalPauseJourney()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase original pause");
            var pm = go.AddComponent<OriginalPauseMenu>();
            pm.Open();
            yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(pm.IsOpen, "original pause", "the fallback pause page did not open (it needs the install)");
            if (!pm.IsOpen) { UnityEngine.Object.Destroy(go); yield break; }
            UiShowcaseNav.Check(pm.FocusedRow == "STR_RESUME_GAME", "original pause", "it did not open on RESUME (" + pm.FocusedRow + ")");
            yield return UiShowcase.Shot("original-pause");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(6);
            UiShowcaseNav.Check(pm.FocusedRow == "STR_RESTART_GAME", "original pause", "the d-pad did not move down to RESTART (" + pm.FocusedRow + ")");
            yield return UiShowcaseNav.Stick(new Vector2(0, -1), 0.2f);
            UiShowcaseNav.Check(pm.FocusedRow == "STR_STUNTS", "original pause", "the left stick did not move down to TRICK LIBRARY (" + pm.FocusedRow + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadUp); yield return UiShowcase.Frames(6);
            // A on RESTART: the confirmation popup (NO is focused first); B closes it
            var popups = (OriginalConfirmPopup)typeof(OriginalPauseMenu).GetField("popups", All).GetValue(pm);
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(popups != null && popups.IsOpen, "original pause", "A on RESTART did not ask first");
            yield return UiShowcase.Shot("original-pause-confirm");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadLeft); yield return UiShowcase.Frames(6);
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(popups != null && !popups.IsOpen && pm.IsOpen, "original pause", "B did not close the question and leave the page open");
            // A on TRICK LIBRARY focuses the stunt list, B comes back, Start resumes
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(6);
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(20);
            UiShowcaseNav.Check(pm.FocusedRow == "STR_STUNTS", "original pause", "the focus left TRICK LIBRARY unexpectedly (" + pm.FocusedRow + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(pm.IsOpen, "original pause", "B on the trick list closed the whole page");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.Start); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(!pm.IsOpen, "original pause", "Start did not resume");
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        sealed class PostDriver : MonoBehaviour { public OriginalPostRace post; void Update() { post?.Tick(Time.unscaledDeltaTime); } }

        /// The fallback results pages (the install's art, OriginalPostRace) with the pad: A on the table goes to the menu, the d-pad moves, A on RESTART asks, B closes
        /// the question, A on CONTINUE continues.
        static IEnumerator OriginalPostJourney()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase original post");
            bool continued = false, restarted = false;
            var post = new OriginalPostRace(go.transform, () => continued = true, () => restarted = true);
            go.AddComponent<PostDriver>().post = post;
            var rows = new List<OriginalPostRace.Row>();
            string[] names = { "Josh Cooper", "Ali Bouda", "Melissa Saito", "Ferret" };
            for (int i = 0; i < names.Length; i++) rows.Add(new OriginalPostRace.Row { index = i + 1, name = names[i], c0 = "05:0" + i + ".20", c1 = "01:37.4" + i, c2 = "" });
            post.SetRows(rows); post.Open();
            yield return UiShowcase.Frames(30);
            UiShowcaseNav.Check(post.IsOpen && post.PageName == "Results", "original post", "the results page did not open (" + post.PageName + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(20);
            UiShowcaseNav.Check(post.PageName == "Menu", "original post", "A on the table did not go to the menu (" + post.PageName + ")");
            yield return UiShowcase.Shot("original-post-menu");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(6);
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(10);
            var popup = (OriginalConfirmPopup)typeof(OriginalPostRace).GetField("popup", All).GetValue(post);
            UiShowcaseNav.Check(popup.IsOpen && !restarted, "original post", "A on RESTART did not ask first");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.East); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(!popup.IsOpen && !restarted && post.IsOpen, "original post", "B did not cancel the restart");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadUp); yield return UiShowcase.Frames(6);
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.South); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(continued, "original post", "A on CONTINUE did not continue");
            post.Close();
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        // the d-pad walks the main page's rows to the named one
        static IEnumerator Down(List<string> rows, string row)
        {
            for (int i = 0; i < 10 && Focused() != row; i++)
            {
                bool down = rows.IndexOf(row) > rows.IndexOf(Focused());
                yield return UiShowcaseNav.PadTap(down ? UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown : UnityEngine.InputSystem.LowLevel.GamepadButton.DpadUp); yield return UiShowcase.Frames(3);
            }
            UiShowcaseNav.Check(Focused() == row, "pause journey", "the d-pad did not reach " + row + " (" + Focused() + ")");
        }

        /// The results page with the pad: down to RETURN TO MENU and back; the board's rows scroll with the d-pad (A needs a world: the real-player probe does it).
        static IEnumerator ResultsJourney()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase results");
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;
            Set2(page, "snapshot", Snapshot(HudMode.Freestyle)); Set2(page, "active", true); Set2(page, "armed", true);
            UiShowcasePages.Call(page, "Build"); UiShowcasePages.Call(page, "ShowResults", new object[] { null });
            UiShowcasePages.DriveOf(go, page);
            yield return UiShowcase.Frames(50);
            UiShowcaseNav.Check(Focused() == "results-again", "results journey", "the page did not open on the first button (" + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(8);
            UiShowcaseNav.Check(Focused() == "results-menu", "results journey", "d-pad down did not reach RETURN TO MENU (" + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadUp); yield return UiShowcase.Frames(8);
            // left reaches the board (the local rider's row), up / down scroll it, right comes back to the buttons
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadLeft); yield return UiShowcase.Frames(10);
            string row = Focused();
            UiShowcaseNav.Check(row.StartsWith("result-slot-"), "results journey", "d-pad left did not reach the board (" + row + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadDown); yield return UiShowcase.Frames(8);
            UiShowcaseNav.Check(Focused().StartsWith("result-slot-") && Focused() != row, "results journey", "d-pad down did not scroll the board (" + row + " -> " + Focused() + ")");
            yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadRight); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(Focused() == "results-again", "results journey", "d-pad right did not come back to the buttons (" + Focused() + ")");
            Set2(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        // ------------------------------------------------------------------------------------------------ the results pages

        static OwnRaceResultsSnapshot Snapshot(HudMode mode)
        {
            string[] n = { "Josh Cooper", "Ali Bouda", "Melissa Saito", "Ferret", "Cece Harris", "Pat Moreno", "Ray Okafor", "Lena Voss", "Tomas Reyes", "Kai Anders", "Ivy Lang", "Dante Cruz" };
            int count = mode == HudMode.Waypoint ? 1 : 12, activity = mode == HudMode.Waypoint ? OwnModes.Waypoint : mode == HudMode.Freestyle ? OwnModes.Freestyle : mode == HudMode.SectionRace ? OwnModes.SectionRace : OwnModes.National;
            var race = new ReflexRaceState { valid = 1, state = 4, num_laps = mode == HudMode.Waypoint ? 1 : mode == HudMode.Race || mode == HudMode.Eliminator ? 3 : 0, laps_completed = 3, place = 4, riders = count };
            var ui = new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState, activity_type = activity, race_state = 4, rules_hold = 0, post_state = 2 };
            var drivers = new ReflexDriverResult[count]; var names = new Dictionary<int, string>();
            int me = mode == HudMode.Waypoint ? 0 : 3;
            for (int i = 0; i < count; i++)
            {
                drivers[i] = new ReflexDriverResult { slot = i, local = i == me ? 1 : 0, place = i + 1, laps_completed = 3, finished = 1, race_time = 301.2f + i * 2.4f, best_lap = 97.4f + i * 0.31f, last_lap = 98f };
                names[i] = mode == HudMode.Waypoint ? "Ferret" : n[i];
            }
            var ctx = new OwnRaceResultsSnapshot.Context { Mode = mode };
            switch (mode)
            {
                case HudMode.Waypoint: ctx.EventLabel = "WAYPOINT RACE  ·  ALPINE"; drivers[0].race_time = 214.6f; ctx.HasModeResults = true; ctx.ModeResults = RxModeResultsState.Make(6, -1, RxModeResultRow.Make(0, true, 1, true, 214.6f, 61.8f)); break;
                case HudMode.Freestyle:
                {
                    ctx.EventLabel = "FREESTYLE  ·  BUENA VISTA"; ctx.HasModeResults = true;
                    var rows = new RxModeResultRow[count];
                    string[] tricks = { "STR_RODEO", "STR_FLATLINER", "STR_RODEO", "STR_NINJANAC", "STR_TWISTER", "STR_SUPERMAN", "STR_HEARTATTACK", "STR_CANCAN", "STR_NAC", "STR_INDY", "STR_SLOBBER", "STR_BACKFLIP" };
                    for (int i = 0; i < count; i++) rows[i] = RxModeResultRow.Make(i, i == me, i + 1, true, 60f, float.MaxValue, 91.2f - i * 4.1f, 9.4f - i * 0.45f, tricks[i]);
                    ctx.ModeResults = RxModeResultsState.Make(5, -1, rows);
                    ctx.HasRaceHud = true; ctx.RaceHud = RxRaceHudState.Create(); ctx.RaceHud.valid = 1;
                    ctx.RaceHud.SetFinalScores(5.72f, 9.0f, 4.76f, 5.27f, 4.5f);
                    break;
                }
                case HudMode.SectionRace:
                {
                    ctx.EventLabel = "SECTION RACE  ·  CONCORD B"; ctx.HasModeResults = true;
                    var rows = new RxModeResultRow[count];
                    for (int i = 0; i < count; i++) rows[i] = RxModeResultRow.Make(i, i == me, i + 1, false, 60f, float.MaxValue, 0, 0, null, Mathf.Max(0, 3 - i / 2), i % 5, 7.4f + i * 0.35f);
                    ctx.ModeResults = RxModeResultsState.Make(12, 10, rows);
                    break;
                }
                case HudMode.Eliminator: ctx.EventLabel = "ELIMINATOR  ·  CAREER"; break;
                default: ctx.EventLabel = "NATIONAL  ·  CONCORD"; break;
            }
            if (!OwnRaceResultsSnapshot.TryCreateMode(1, "Showcase/" + mode, true, race, ui, drivers, count, names, ctx, out var snapshot, out var reason)) throw new InvalidOperationException(reason);
            return snapshot;
        }

        static IEnumerator Results(string shot, HudMode mode)
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase results");
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;   // (no world: its Update would close it)
            var snapshot = Snapshot(mode);
            Set2(page, "snapshot", snapshot); Set2(page, "active", true); Set2(page, "armed", true);
            UiShowcasePages.Call(page, "Build");
            UiShowcasePages.Call(page, "ShowResults", new object[] { null });
            UiShowcasePages.DriveOf(go, page);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot(shot);
            Set2(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }
        static void Set2(object o, string field, object value) { var f = o.GetType().GetField(field, All); if (f == null) throw new MissingFieldException(o.GetType().Name, field); f.SetValue(o, value); }
    }
}
