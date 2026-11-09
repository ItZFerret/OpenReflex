using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // MOTOCAREER end to end in play mode (FrontEndCareer.cs, ReflexProfile, ReflexCareer; native career.h), PNGs to $CAREER_SHOTS:
    //   Create  (CAREER_FRESH=1 deletes this project's save first) title -> Load Game -> the motocard setup (Autosave Notice, Screen
    //           Adjustment, Welcome, Completion) -> main menu -> MOTOCAREER -> Career Welcome -> Race and Ride -> the first series'
    //           events -> Start Race -> the career world -> the race driven to its end (a waypoint / race-line autopilot on W + A / D)
    //           -> post-race CONTINUE (the game completes the series event and saves) -> back on the events page with the result.
    //   Verify  (a new editor process) title -> Load Game -> main menu (the motocard is set up) -> MOTOCAREER -> Race and Ride (the
    //           welcome is done) -> the events page: the saved PlayerSeriesStatus of the raced series is there.
    // The user's PlayerPrefs keys are restored at the end; the bench's own productName keeps its save apart.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.CareerTests.Create (or .Verify)
    public static class CareerTests
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read__fn(IntPtr world, uint address, byte[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn> reflex_world_read__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn>("reflex_world_read");
        static int reflex_world_read(IntPtr world, uint address, byte[] output, uint size) => reflex_world_read__p.Get()(world, address, output, size);
        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage; static double stageAt, started; static string shots, mode;
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, string> prefs;
        static int raceSeries = -1, profileVersion; static bool named;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Career] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }

        public static void Create() => Begin("create");
        public static void Verify() => Begin("verify");
        /// A Moto series (CAREER_MOTO_SERIES, default 15: NAT_Renne, two motos) from the events page: the practice popup (skip),
        /// moto 1 (rules popup, race, results, moto results), the world of moto 2 from the replayed series, moto 2, the series'
        /// completion and save, the rewards page, back to the front end.
        public static void Moto() => Begin("moto");
        /// The garage from an existing career save: Race and Ride -> GARAGE (the Garage Page with the player's rider and bike) -> RIDER
        /// (Garage Customize Rider Page) -> a gear type's store list -> equip an owned, unequipped piece (else buy the cheapest affordable
        /// one through the purchase popup) -> back (the game saves) -> the profile read again from the save has it.
        public static void Garage() => Begin("garage");
        /// The front end's career pages that follow a series: the Vehicle Reward Page (RewardVehiclePresentation, here with the
        /// profile's own vehicle model as the reward) -> accept -> the events page; the Career Completion Page -> CareerModeMenuPage.
        public static void Pages() => Begin("pages");

        static void Begin(string m)
        {
            mode = m; report.Clear(); bad = 0; stage = 0; ReflexProfile.Disabled = false;
            prefs = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            shots = Environment.GetEnvironmentVariable("CAREER_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "career-shots");
            Directory.CreateDirectory(shots);
            Log("save " + ReflexProfile.SaveFile + (ReflexProfile.HasSave ? " (" + new FileInfo(ReflexProfile.SaveFile).Length + " bytes)" : " (none)"));
            if ((m == "create" || m == "moto") && Environment.GetEnvironmentVariable("CAREER_FRESH") == "1")
            {
                if (Directory.Exists(ReflexProfile.RemoteDir)) Directory.Delete(ReflexProfile.RemoteDir, true);
                string persona = Path.Combine(ReflexProfile.SaveDir, "steam_persona.txt"); if (File.Exists(persona)) File.Delete(persona);
                Log("fresh career: save and persona deleted");
            }
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (fm) fm.rememberChoice = false;
            stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Shot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var main = Camera.main;
            if (main) { c.CopyFrom(main); c.targetTexture = target; c.enabled = false; c.cullingMask = main.cullingMask | 1; }
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, mode + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + mode + "_" + name);
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }

        // WP-5: a Heat / Moto race starts on the Gate Selection page (OriginalGateSelection): after 3 s it is accepted with the default gate,
        // or with CAREER_GATE=next the next available one for the first page (the race's world is booted again with it).
        static double gateSeen = -1; static int gatePages;
        static bool GatePage()
        {
            var cr = UnityEngine.Object.FindAnyObjectByType<OriginalCareerRace>();
            var g = cr ? cr.GatePage : null;
            if (g == null || !g.IsOpen) { gateSeen = -1; return false; }
            double now = EditorApplication.timeSinceStartup;
            if (gateSeen < 0) { gateSeen = now; Log($"Gate Selection page: selection {g.index}, gates {string.Join(",", g.available)}, default {g.defaultGate} ({OriginalGateSelection.LastLog})"); }
            if (now - gateSeen < 3) return true;
            Shot("gate_selection_" + gatePages);
            if (gatePages++ == 0 && Environment.GetEnvironmentVariable("CAREER_GATE") == "next") g.Move(+1);
            Log("Gate Selection: accept gate " + g.Focus);
            bool reload = g.Focus != g.defaultGate;
            g.Accept(); gateSeen = -1;
            if (reload && mode == "moto") { stage = 33; stageAt = now; Log("the race's world again with that gate"); }   // the world of this race, booted with the choice
            if (Environment.GetEnvironmentVariable("CAREER_GATE_ONLY") == "1" && gatePages >= (reload ? 99 : 1)) { Log("CAREER_GATE_ONLY: done after the gate page"); stage = 100; EditorApplication.ExitPlaymode(); }
            return true;
        }
        static bool videosEnded;
        static readonly string[] SetupPages = { "Motocard Setup Vehicle page", "Motocard Setup Gear Page", "Motocard Setup Skintone Page", "Motocard Setup Jersey Name Page", "Motocard Setup Jersey Number Page" };
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static bool Idle(FrontEndManager fm) => fm && fm.assets != null && !fm.Busy;
        /// Waits for a page: true once it is current and the transition is over (fails after `timeout` s).
        static bool At(FrontEndManager fm, string page, double after = 1.0, double timeout = 60)
        {
            if (Idle(fm) && fm.CurrentPage == page && Age > after) return true;
            if (Age > timeout) Fail("page " + page + " not reached (at " + fm?.CurrentPage + " / " + fm?.TopPopup + ")");
            return false;
        }

        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > (mode == "moto" ? 4000 : 1500)) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                if (GatePage()) return;   // WP-5: the Gate Selection page of a Heat / Moto race
                if (mode == "create" || mode == "moto" && stage < 30) StepCreate(fm); else if (mode == "moto") StepMoto(fm); else if (mode == "garage") StepGarage(fm); else if (mode == "pages") StepPages(fm); else StepVerify(fm);
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void StepCreate(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { Shot("00_title"); fm.DispatchKey("Accept"); Next(); } break;
                case 1: if (At(fm, "Load Game Page", 0.5)) { Shot("01_load_game"); Next(); } break;
                case 2:   // the game signs the profile in and loads the save (none: create the motocard)
                    if (Idle(fm) && fm.CurrentPage != "Load Game Page" && Age > 1)
                    {
                        Log("profile: " + Describe(ReflexProfile.Current));
                        if (fm.CurrentPage == "Main Menu Page") { stage = 10; stageAt = EditorApplication.timeSinceStartup; break; }
                        Check(fm.CurrentPage == "Autosave Notice Page", "a new profile goes to the motocard setup: " + fm.CurrentPage);
                        Shot("02_autosave_notice"); fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 90) Fail("the profile did not load: " + ReflexProfile.Error);
                    break;
                case 3:   // no persona yet: the Motocard Setup Name Page names the motocard (the Steam persona stand-in)
                    if (Idle(fm) && fm.CurrentPage == "Motocard Setup Name Page" && Age > 1 && !named)
                    {
                        named = true; string n = Environment.GetEnvironmentVariable("CAREER_NAME") ?? "Tester";
                        var script = fm.Layer("MotocardNameEntry.lyr").script as MotocardNameEntryScript;
                        script?.Input?.Type(n); Shot("02b_name_entry"); fm.DispatchKey("Accept"); Log("motocard name typed: " + n);
                    }
                    if (At(fm, "Screen Adjustment Page", 1.0, 90)) { if (named) Check(ReflexProfile.Persona == (Environment.GetEnvironmentVariable("CAREER_NAME") ?? "Tester"), "the persona is the typed name: " + ReflexProfile.Persona); fm.DispatchKey("Accept"); Next(); }
                    break;
                case 4: if (At(fm, "Motocard Setup Welcome Page")) { Shot("03_motocard_welcome"); fm.DispatchKey("Accept"); Next(); } break;
                case 5:   // the setup's vehicle, gear, skin tone, jersey name and number pages (FrontEndMotocard.cs): their defaults
                    if (Idle(fm) && Age > 2.5 && !ReflexProfile.Busy && SetupPages.Contains(fm.CurrentPage) && fm.TopPopup == null) { Shot("04_" + fm.CurrentPage.Replace(' ', '_')); fm.DispatchKey("Accept"); stageAt = EditorApplication.timeSinceStartup; }
                    else if (At(fm, "Motocard Setup Completion", 2, 120)) { Shot("04_motocard_completion"); fm.DispatchKey("Accept"); Next(); }
                    break;
                case 6: if (At(fm, "Main Menu Page", 2) && !ReflexProfile.Busy) { Check(ReflexProfile.Current.MotocardSetup, "PlayerRider.MotocardSetup set by the game"); stage = 10; stageAt = EditorApplication.timeSinceStartup; } break;
                case 10: if (At(fm, "Main Menu Page", 1.5)) { Shot("05_main_menu"); fm.DispatchKey("Accept"); Next(); } break;   // MOTOCAREER is focused first
                case 11:
                    if (Idle(fm) && Age > 1.5 && (fm.CurrentPage == "Career Welcome Page" || fm.CurrentPage == "Race and Ride Page"))
                    {
                        if (fm.CurrentPage == "Career Welcome Page") { Shot("06_career_welcome"); fm.DispatchKey("Accept"); Next(); }
                        else { stage = 13; stageAt = EditorApplication.timeSinceStartup; }
                    }
                    else if (Age > 30) Fail("MOTOCAREER went to " + fm?.CurrentPage + " / " + fm?.TopPopup);
                    break;
                case 12: if (At(fm, "Race and Ride Page", 2)) Next(); break;
                case 13: if (Age > 1 && !ReflexProfile.Busy) { Shot("07_race_and_ride"); fm.DispatchKey("Accept"); Next(); } break;
                case 14:
                    if (At(fm, "Race and Ride Events", 2))
                    {
                        Shot("08_events");
                        if (mode == "moto")   // the series straight from the Series table (a fresh profile has it locked): as OnAcceptSeries for Heat / Moto
                        {
                            int series = int.TryParse(Environment.GetEnvironmentVariable("CAREER_MOTO_SERIES") ?? "15", out var ms) ? ms : 15;
                            var ev = ReflexCareer.EventOf(series, FeCareerScripts.Lookup);
                            if (ev == null) { Fail("series " + series + " has no event"); break; }
                            int type = ReflexProfile.Current.T("PlayerVehicle").Int("VehicleTypeId", 0, 2);
                            fm.vars["CareerSeries"] = new ReflexCareer.Race { seriesId = series, vehicleTypeId = 2, parentStoreProductId = ReflexCareer.SeriesStoreProductId, eventKey = ev.key };
                            raceSeries = series; Log("moto series " + series + " (" + ev.key + "), vehicle type 2 (profile vehicle type " + type + ")");
                            fm.SendUIEvent("ShowPracticePopup"); stage = 30; stageAt = EditorApplication.timeSinceStartup; break;
                        }
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 15:
                    if (fm.TopPopup == "Start Race Popup" && Age > 1) { Shot("09_start_race"); fm.DispatchKey("Accept"); Next(); }
                    else if (Age > 10) Fail("no start race popup: " + fm.TopPopup);
                    break;
                case 16:
                    if (ReflexCareer.Current != null && raceSeries < 0) { raceSeries = ReflexCareer.Current.seriesId; Log("career race: " + ReflexCareer.Current); }
                    if (FrontEndLoadingOverlay.Current && Age > 1) { Shot("10_loading"); Next(); } else if (Age > 30) Fail("no loading page");
                    break;
                case 17:
                {
                    var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                    if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 3) { Log("career world running: " + rt.statusLabel?.text?.Split('\n')[0]); Next(); }
                    else if (Age > 400) Fail("career world not running: " + rt?.statusLabel?.text);
                    break;
                }
                case 18: Race(); break;
                case 19:   // post-race pages: CONTINUE
                {
                    if (Age < 3) break;
                    // the career's own post pages (OriginalCareerRace): the game's results table, then CONTINUE (SeriesContinue)
                    var cr = Career();
                    if (cr == null || cr.Post == null || !cr.Post.IsOpen) { if (Age > 30) Fail("career post-race pages not open"); break; }
                    Shot("12_post_race");
                    cr.Continue();
                    Check(ReflexCareer.LastResult != null, "the game completed the career event: " + ReflexCareer.LastResult?.Substring(0, Math.Min(160, ReflexCareer.LastResult?.Length ?? 0)));
                    if (cr.Post.PageName == "Rewards") { Shot("12b_rewards"); cr.Post.AcceptPage(); }   // PostEventRewards: accept
                    profileVersion = ReflexProfile.Version;
                    Next();
                    break;
                }
                case 20:   // back on the events page with the new profile (read again from the save the race wrote)
                    if (Idle(fm) && fm.CurrentPage == "Race and Ride Events" && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                    {
                        Shot("13_events_after_race");
                        CheckProgress();
                        stage = 100; EditorApplication.ExitPlaymode();
                    }
                    else if (Age > 90) Fail("not back on the events page with the profile: " + fm?.CurrentPage + " v" + ReflexProfile.Version);
                    break;
            }
        }

        static int motoRace;
        static OriginalCareerRace Career() => UnityEngine.Object.FindAnyObjectByType<OriginalCareerRace>();
        static void StepMoto(FrontEndManager fm)
        {
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            var cr = Career();
            switch (stage)
            {
                case 30:
                    if (fm && fm.TopPopup == "Practice Popup" && Age > 1.5) { Shot("09_practice_popup"); fm.Navigate(1, 0); Next(); }
                    else if (Age > 10) Fail("no practice popup: " + fm?.TopPopup);
                    break;
                case 31: if (Age > 0.5) { fm.DispatchKey("Accept"); Next(); } break;   // SKIP PRACTICE
                case 32:
                    if (ReflexCareer.Current != null && FrontEndLoadingOverlay.Current) { Check(ReflexCareer.Current.skipPractice == 1, "the practice popup's answer: skip " + ReflexCareer.Current.skipPractice); Log("career race: " + ReflexCareer.Current); motoRace = 1; Next(); }
                    else if (Age > 20) Fail("the practice popup did not start the series (" + fm?.TopPopup + ", current " + ReflexCareer.Current + ")");
                    break;
                case 33:   // the world of this race (built: boot, series start, gate selection, the races before it replayed)
                    if (rt && rt.Running && Age > 3) { Log("moto " + motoRace + " world running"); Next(); }
                    else if (Age > 600) Fail("moto " + motoRace + " world not running: " + rt?.statusLabel?.text);
                    break;
                case 34:   // the rules popup (the activity's hold), when the game has one for this race
                    if (cr && cr.RulesOpen && Age > 2) { Shot("1" + motoRace + "_rules_" + OriginalRulesPopup.LastShown); Log("rules popup " + OriginalRulesPopup.LastShown); cr.Rules.Finish(false); Next(); }
                    else if (Age > 12) { Log("no rules popup for moto " + motoRace); Next(); }
                    break;
                case 35: { var race = RaceState(rt); if (race.state >= 1) { Log("moto " + motoRace + " left Pre"); Next(); } else if (Age > 60) Fail("the race stays in Pre (rules hold?)"); break; }
                case 36: RaceTo(rt, () => Next()); break;
                case 37:
                    if (cr && cr.Post != null && cr.Post.IsOpen && Age > 2) { Shot("1" + motoRace + "_results"); Log("results, SeriesEventStatus " + cr.SeriesStatus); cr.Continue(); Next(); }
                    else if (Age > 60) Fail("career results page not open");
                    break;
                case 38:
                    if (cr.Post.PageName == "Moto" && Age > 2) { Shot("1" + motoRace + "_moto_results"); Log("moto results"); cr.Continue(); Next(); }
                    else if (Age > 30) Fail("moto results page not open (" + cr.Post.PageName + ")");
                    break;
                case 39:
                    if (ReflexCareer.Current != null && ReflexCareer.Current.outcomes.Count == motoRace) { Log("next race: " + ReflexCareer.Current); motoRace++; stage = 33; stageAt = EditorApplication.timeSinceStartup; }
                    else if (ReflexCareer.LastResult != null && ReflexCareer.Current == null)
                    {
                        Check(ReflexCareer.LastResult.Contains("\"status\":\"MainEvent\""), "the series completed: " + ReflexCareer.LastResult.Substring(0, Math.Min(400, ReflexCareer.LastResult.Length)));
                        profileVersion = ReflexProfile.Version; Next();
                    }
                    else if (Age > 60) Fail("neither the next race nor the completion");
                    break;
                case 40:
                    if (cr && cr.Post != null && cr.Post.PageName == "Rewards" && Age > 2) { Shot("20_rewards"); cr.Post.AcceptPage(); Next(); }
                    else if (fm && Age > 5) { Log("no rewards page"); Next(); }
                    else if (Age > 30) Fail("no rewards page / front end");
                    break;
                case 41:
                    if (Idle(fm) && fm.CurrentPage == "Race and Ride Events" && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                    { Shot("21_events_after_series"); CheckProgress(); stage = 100; EditorApplication.ExitPlaymode(); }
                    else if (Age > 120) Fail("not back on the events page: " + fm?.CurrentPage);
                    break;
            }
        }
        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => rt ? (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt) : default;
        static void RaceTo(OriginalWorldRuntime rt, Action over)
        {
            if (!rt) return;
            var race = RaceState(rt);
            rt.testHeldKeys = race.state == 2 ? Steer(rt) : Array.Empty<int>();
            if (EditorApplication.timeSinceStartup - lastLog > 10) { lastLog = EditorApplication.timeSinceStartup; Log($"moto {motoRace}: race {race.state} place {race.place}/{race.riders} lap {race.laps_completed}/{race.num_laps} time {race.race_time:F1}"); }
            if (race.state == 2 && Age > 30 && !racingShot) { racingShot = true; Shot("1" + motoRace + "_racing"); }
            if (race.state >= 4) { rt.testHeldKeys = Array.Empty<int>(); racingShot = false; Log("moto " + motoRace + " over: place " + race.place + " of " + race.riders); over(); }
            else if (Age > 1200) Fail("the race did not end");
        }

        static int garageGear = -1, garageTries, balanceBefore; static string saveBefore; static bool bought;
        static void StepGarage(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage == "Main Menu Page" && Age > 1.5 && !ReflexProfile.Busy && ReflexProfile.Current != null)
                    {
                        // test setup: credits to buy with (the game's own table write; the bench's save only)
                        if (ReflexProfile.Current.T("PlayerRider").Int("AccountBalance", 0, 0) < 20000) { ReflexProfile.Run("set_int PlayerRider 0 AccountBalance 20000"); Log("test setup: AccountBalance 20000"); }
                        fm.DispatchKey("Accept"); Next();   // MOTOCAREER
                    }
                    else if (Age > 90) Fail("no main menu");
                    break;
                case 2: if (At(fm, "Race and Ride Page", 2)) { fm.DispatchKey("Toggle"); Next(); } break;   // GARAGE
                case 3: if (At(fm, "Garage Page", 4)) { Shot("30_garage"); fm.Navigate(0, 1); fm.Navigate(1, 0); Next(); } break;
                case 4: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;   // RIDER (the focused item after navigating)
                case 5:
                    if (Idle(fm) && fm.CurrentPage == "Garage My Rides Page") { Log("VEHICLE was focused: back, then the other item"); fm.SendUIEvent("TravelBack"); stage = 3; stageAt = EditorApplication.timeSinceStartup - 3; break; }
                    if (At(fm, "Garage Customize Rider Page", 3)) { Shot("31_customize_rider"); fm.DispatchKey("Accept"); saveBefore = ReflexProfile.SaveStamp(); Next(); }
                    break;
                case 6:
                {
                    var rc = fm.Layer("GarageRiderCustomize.lyr").script as GarageRiderCustomizeScript;
                    if (rc == null || Age < 2 || ReflexProfile.Busy) break;
                    if (garageGear < 0) { balanceBefore = ReflexProfile.Current.T("PlayerRider").Int("AccountBalance", 0, 0); garageGear = rc.TestAcceptFirst(true); bought = garageGear >= 0; }
                    if (garageGear < 0) garageGear = rc.TestAcceptFirst(false);
                    if (garageGear < 0) { if (++garageTries > 6) { Fail("no gear to equip or buy"); break; } rc.TestNextType(); stageAt = EditorApplication.timeSinceStartup; break; }
                    Log("garage: gear " + garageGear + " (" + rc.FocusedTypeFactory + ")" + (fm.TopPopup != null ? ", popup " + fm.TopPopup : ""));
                    profileVersion = ReflexProfile.Version; Shot("32_store_list"); Next();
                    break;
                }
                case 7:
                    if (fm.TopPopup == "ShowPurchasePopup") { if (Idle(fm) && Age > 1.5) { Shot("33_purchase_popup"); fm.DispatchKey("Accept"); stageAt = EditorApplication.timeSinceStartup; } break; }
                    if (fm.TopPopup == null && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                    {
                        var g = ReflexProfile.Current.T("PlayerRiderGear");
                        Check(Enumerable.Range(0, g.rows).Any(i => g.Int("RiderGearId", i) == garageGear), "the game's PlayerRiderGear has gear " + garageGear + ": " + string.Join(",", Enumerable.Range(0, g.rows).Select(i => g.Int("RiderGearId", i))));
                        if (bought)
                        {
                            int bal = ReflexProfile.Current.T("PlayerRider").Int("AccountBalance", 0, 0);
                            int asset = FeTables.I(FeTables.Shared.Rows("RiderGear").FirstOrDefault(r => FeTables.I(r, "RiderGearId") == garageGear), "StoreAssetId");
                            Check(bal < balanceBefore, "the purchase was paid: " + balanceBefore + " -> " + bal);
                            Check(ReflexProfile.Current.T("PlayerAssetsPurchased").Find("StoreAssetId", asset) >= 0, "the game's PlayerAssetsPurchased has store asset " + asset);
                        }
                        Shot("34_equipped"); fm.DispatchKey("Back"); Next();
                    }
                    else if (Age > 40) Fail("the gear was not applied (popup " + fm.TopPopup + ")");
                    break;
                case 8:   // back through the lists to the gear type list, then TravelBack (End: RequestSaveGame)
                    if (Idle(fm) && fm.CurrentPage == "Garage Customize Rider Page" && Age > 1.5) { fm.DispatchKey("Back"); stageAt = EditorApplication.timeSinceStartup; }
                    else if (Idle(fm) && fm.CurrentPage == "Garage Page") Next();
                    break;
                case 9:
                    if (Idle(fm) && fm.CurrentPage == "Garage Page" && !ReflexProfile.Busy && Age > 3)
                    {
                        Check(ReflexProfile.SaveStamp() != saveBefore, "the game saved the change (save " + saveBefore + " -> " + ReflexProfile.SaveStamp() + ")");
                        ReflexProfile.Close(); ReflexProfile.Open(); profileVersion = ReflexProfile.Version; Next();
                    }
                    else if (Age > 30) Fail("not back on the garage page: " + fm?.CurrentPage);
                    break;
                case 10:
                    if (ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && ReflexProfile.Current != null)
                    {
                        var g = ReflexProfile.Current.T("PlayerRiderGear");
                        Check(Enumerable.Range(0, g.rows).Any(i => g.Int("RiderGearId", i) == garageGear), "the reloaded save has gear " + garageGear);
                        Log("credits " + ReflexProfile.Current.T("PlayerRider").Int("AccountBalance", 0, 0));
                        Shot("35_garage_after"); fm.Navigate(-1, 0); Next();
                    }
                    else if (Age > 60) Fail("the profile did not reopen");
                    break;
                case 11: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;   // MY RIDES
                case 12:
                    if (At(fm, "Garage My Rides Page", 4))
                    {
                        var mr = fm.Layer("GarageMyRides.lyr").script as GarageMyRidesScript;
                        Shot("36_my_rides"); Log("my rides: equipped model " + mr?.EquippedModel + ", vehicle list " + (mr?.VehicleListActive ?? false) + ", garage " + mr?.Garage?.Shown);
                        Check(mr != null && mr.VehicleListActive && mr.EquippedModel >= 0 && (mr.Garage?.Shown ?? "none") != "none / none", "My Rides shows the equipped vehicle");
                        fm.DispatchKey("Accept"); Next();   // the equipped vehicle -> GotoCustomizeVehiclePage
                    }
                    break;
                case 13:
                    if (At(fm, "Garage Customize Vehicle Page", 4))
                    {
                        var vc = fm.Layer("GarageVehicleCustomize.lyr").script as GarageVehicleCustomizeScript;
                        Shot("37_customize_vehicle"); Log("vehicle part types " + vc?.TypeCount + ", garage " + vc?.Garage?.Shown);
                        profileVersion = ReflexProfile.Version;
                        if (vc != null) { fm.DispatchKey("Accept"); garageGear = vc.TestEquipFirst(); Log("equipped part " + garageGear); }
                        Next();
                    }
                    break;
                case 14:
                    if (garageGear < 0) { Log("no owned part to equip"); stage = 100; EditorApplication.ExitPlaymode(); break; }
                    if (ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                    {
                        var vp = ReflexProfile.Current.T("PlayerVehicleParts");
                        Check(Enumerable.Range(0, vp.rows).Any(i => vp.Int("VehiclePartId", i) == garageGear), "the game's PlayerVehicleParts has part " + garageGear);
                        Shot("38_part_equipped"); stage = 100; EditorApplication.ExitPlaymode();
                    }
                    else if (Age > 40) Fail("the part was not applied");
                    break;
            }
        }

        static void StepPages(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage == "Main Menu Page" && Age > 1.5 && !ReflexProfile.Busy && ReflexProfile.Current != null)
                    {
                        var p = ReflexProfile.Current; int model = p.T("PlayerVehicle").Int("VehicleModelId", 0);
                        ReflexCareer.RewardVehicleModel = model;
                        ReflexCareer.RewardVehicleAsset = FeTables.I(FeTables.Shared.Rows("VehicleModel").FirstOrDefault(r => FeTables.I(r, "VehicleModelId") == model), "StoreAssetId");
                        Log("reward vehicle (test): model " + model + ", asset " + ReflexCareer.RewardVehicleAsset);
                        fm.JumpTo("Vehicle Reward Page"); Next();
                    }
                    else if (Age > 90) Fail("no main menu");
                    break;
                case 2:
                    if (At(fm, "Vehicle Reward Page", 4))
                    {
                        var rv = fm.Layer("RewardVehiclePresentation.lyr").script as RewardVehiclePresentationScript;
                        Shot("40_vehicle_reward"); Check(ReflexCareer.RewardVehicleAsset == -1, "the reward page took the reward (RemoveUIVariable)");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 3: if (At(fm, "Race and Ride Events", 2)) { Shot("41_events_after_reward"); saveBefore = ReflexProfile.SaveStamp(); fm.JumpTo("Career Completion Page"); Next(); } break;
                case 4:
                    if (Idle(fm) && fm.CurrentPage == "Race and Ride Page" && !ReflexProfile.Busy && Age > 2)
                    { Log("career completion -> " + fm.CurrentPage + ", save " + saveBefore + " -> " + ReflexProfile.SaveStamp()); Check(true, "the Career Completion Page went on to the career menu (videos not decoded)"); stage = 100; EditorApplication.ExitPlaymode(); }
                    else if (Idle(fm) && fm.CurrentPage == "Career Completion Page" && Age > 5 && !videosEnded)
                    {   // CareerCompletion.bik and Credits.bik play for minutes now (FrontEndVideo.cs) and cannot be skipped: their DoneEvents
                        videosEnded = true; Log("career completion: the videos' DoneEvents sent (CompletionVideoDone, CreditsVideoDone)");
                        fm.SendUIEvent("CompletionVideoDone"); fm.SendUIEvent("CreditsVideoDone");
                    }
                    else if (Age > 40) Fail("career completion went to " + fm?.CurrentPage);
                    break;
            }
        }

        static void StepVerify(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage != "Title Page" && fm.CurrentPage != "Load Game Page" && Age > 1)
                    {
                        Log("profile: " + Describe(ReflexProfile.Current));
                        Check(fm.CurrentPage == "Main Menu Page", "a set-up motocard goes to the main menu: " + fm.CurrentPage);
                        Next();
                    }
                    else if (Age > 90) Fail("the profile did not load");
                    break;
                case 2: if (At(fm, "Main Menu Page", 1.5)) { Shot("01_main_menu"); fm.Navigate(1, 0); fm.Navigate(1, 0); fm.DispatchKey("Accept"); Next(); } break;   // MOTOCARD
                case 3: if (At(fm, "Motocard Page", 1.5)) { Shot("02_motocard"); fm.Navigate(0, 1); fm.Navigate(0, 1); fm.DispatchKey("Accept"); stage = 20; stageAt = EditorApplication.timeSinceStartup; } break;   // CHANGE RIDER NAME
                case 20:
                    if (fm.TopPopup == "JerseyNameEntryPopup" && Age > 1)
                    {
                        var js = fm.Layer("JerseyNameEntry.lyr").script as JerseyEntryPopupScript; js?.Input?.Type("blue7");
                        Shot("02b_jersey_name"); profileVersion = ReflexProfile.Version; fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 10) Fail("no jersey name popup: " + fm.TopPopup);
                    break;
                case 21:
                    if (fm.TopPopup == null && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 1)
                    {
                        Check(ReflexProfile.Current.T("PlayerRider").Str("Name", 0) == "BLUE7", "the jersey name set by the game: " + ReflexProfile.Current.T("PlayerRider").Str("Name", 0));
                        fm.Navigate(0, 1); fm.DispatchKey("Accept"); Next();   // CHANGE RIDER NUMBER
                    }
                    else if (Age > 30) Fail("the jersey name was not applied");
                    break;
                case 22:
                    if (fm.TopPopup == "JerseyNumberEntryPopup" && Age > 1)
                    {
                        var js = fm.Layer("JerseyNumberEntry.lyr").script as JerseyEntryPopupScript; js?.Input?.Type("42");
                        Shot("02c_jersey_number"); profileVersion = ReflexProfile.Version; fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 10) Fail("no jersey number popup: " + fm.TopPopup);
                    break;
                case 23:
                    if (fm.TopPopup == null && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 1)
                    {
                        Check(ReflexProfile.Current.T("PlayerRider").Str("Number", 0) == "42", "the jersey number set by the game: " + ReflexProfile.Current.T("PlayerRider").Str("Number", 0));
                        Shot("02d_motocard_after"); fm.DispatchKey("Back"); stage = 4; stageAt = EditorApplication.timeSinceStartup;
                    }
                    else if (Age > 30) Fail("the jersey number was not applied");
                    break;
                case 4: if (At(fm, "Main Menu Page", 1.5)) { fm.Navigate(-1, 0); fm.Navigate(-1, 0); fm.DispatchKey("Accept"); Next(); } break;                    // MOTOCAREER
                case 5: if (At(fm, "Race and Ride Page", 2)) { Shot("03_race_and_ride"); fm.DispatchKey("Accept"); Next(); } break;
                case 6: if (At(fm, "Race and Ride Events", 2)) { Shot("04_events"); CheckProgress(); stage = 100; EditorApplication.ExitPlaymode(); } break;
            }
        }

        static void CheckProgress()
        {
            var p = ReflexProfile.Current; var st = p?.T("PlayerSeriesStatus");
            Log("PlayerSeriesStatus: " + (st == null ? "none" : string.Join("; ", Enumerable.Range(0, st.rows).Select(i => $"series {st.Int("SeriesId", i)} status {st.Int("SeriesStatus", i)} best {st.Int("BestPlaceFinish", i)} gold {st.Int("GoldMedals", i)}"))));
            Log("career completion " + p?.careerCompletion + ", credits " + p?.T("PlayerRider").Int("AccountBalance", 0, 0) + ", save " + new FileInfo(ReflexProfile.SaveFile).Length + " bytes");
            int series = raceSeries >= 0 ? raceSeries : int.TryParse(Environment.GetEnvironmentVariable("CAREER_SERIES") ?? "3", out var s) ? s : 3;
            Check(st != null && st.Find("SeriesId", series) >= 0, "the raced series' result is in the profile (series " + series + ")");
        }

        static string Describe(ProfileData p) => p == null ? "none (" + ReflexProfile.Error + ")" :
            $"signed in {p.signedIn}, storage {p.storage}, loaded {p.gameLoaded}, motocard {p.MotocardSetup}, career welcome {p.EventStatus("CareerWelcome")}, completion {p.careerCompletion}, tables {p.tables.Count}, products {p.products.Count}";

        // ---------------------------------------------------------------------------------------------------- the race
        static double lastLog, slowSince, reverseUntil; static bool racingShot; static int lastGate; static Vector3 lastTarget, lastPos;
        static void Race()
        {
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!rt) { Fail("no race runtime"); return; }
            var race = (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            if (race.state == 2) rt.testHeldKeys = Steer(rt); else rt.testHeldKeys = Array.Empty<int>();
            if (EditorApplication.timeSinceStartup - lastLog > 5) { lastLog = EditorApplication.timeSinceStartup; Log($"race {race.state} place {race.place}/{race.riders} lap {race.laps_completed} time {race.race_time:F1} gate {lastGate} pos {lastPos} target {lastTarget}"); }
            if (race.state == 2 && Age > 20 && !racingShot) { racingShot = true; Shot("11_racing"); }
            if (race.state >= 4) { rt.testHeldKeys = Array.Empty<int>(); Log("race over: place " + race.place + " of " + race.riders); Next(); }
            else if (Age > 900) Fail("the race did not end");
        }

        static uint U32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToUInt32(b, 0); }
        static float F32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToSingle(b, 0); }
        // ShowcaseShots' autopilot: throttle and steer (A 0x1e / D 0x20) toward the race line ~45 units ahead, or the next gate of a
        // waypoint race (activity A+0xff074 gates at A+0xff078, position +0x60; driver D+0x10d4 = the last gate passed). Test only:
        // reads the running world between its frames (reflex_world_read).
        static int[] Steer(OriginalWorldRuntime rt)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            var world = (IntPtr)typeof(OriginalWorldRuntime).GetField("world", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            if (st.position == null || world == IntPtr.Zero) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            uint G = U32(world, 0xa946b8), P = G + 0x7460, A = U32(world, P + 0x94), drivers = U32(world, A + 0x268), n = U32(world, A + 0x26c), D = 0;
            for (uint i = 0; i < n && i < 64; i++) { uint d = U32(world, drivers + 4 * i); if (d != 0 && U32(world, d + 0x74) == P) { D = d; break; } }
            if (D == 0) return new[] { 0x11 };
            Vector3 target = pos;
            uint node = U32(world, D + 0x1c), first = node;
            if (node != 0)
                for (int k = 0; node != 0 && k < 64; k++) { var p = new Vector3(F32(world, node), 0, F32(world, node + 8)); target = p; if ((p - pos).magnitude > 45) break; node = U32(world, node + 0x64); if (node == first) break; }
            else
            {
                uint gc = U32(world, A + 0xff074); if (gc == 0) return new[] { 0x11 };
                uint gi = (uint)(((int)U32(world, D + 0x10d4) + 1) % (int)gc), g = U32(world, U32(world, A + 0xff078) + 4 * gi);
                target = new Vector3(F32(world, g + 0x60), 0, F32(world, g + 0x68));
            }
            var dd = target - pos;
            lastGate = (int)U32(world, D + 0x10d4); lastTarget = target; lastPos = pos;
            // stuck (slow for 2.5 s): back off with reverse (X) and the opposite lock for 1.5 s
            double now = EditorApplication.timeSinceStartup;
            if (vel.magnitude >= 3f) slowSince = now;
            if (now - slowSince > 2.5) { reverseUntil = now + 1.5; slowSince = now + 1.5; }
            if (now < reverseUntil) return new[] { 0x2d, (vel.x * dd.z - vel.z * dd.x) > 0 ? 0x20 : 0x1e };
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * dd.z - vel.z * dd.x, ang = Vector3.Angle(vel, dd);
            int key = ang < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            ReflexProfile.Close();
            var after = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            Check(after.All(kv => kv.Value == prefs[kv.Key]), "user PlayerPrefs keys untouched");
            Log("career " + mode + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "career-" + mode + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
