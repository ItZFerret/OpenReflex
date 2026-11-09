using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // WP-4 / WP-F front-end pages and sound in play mode (FrontEndMotocard.cs, FrontEndPopups.cs, FrontEndSounds.cs), PNGs and a report
    // to $FE2_SHOTS:
    //   Setup   (deletes this project's save and persona first) title -> Load Game -> the whole motocard setup (Autosave Notice, the
    //           name entry, Screen Adjustment with a gamma change, Welcome, Vehicle, Gear with a gear-set change, Skintone with a
    //           change, Jersey Name with the popup, Jersey Number, Completion) -> main menu -> MOTOCARD (the card, HIGHLIGHTS,
    //           LEADERBOARDS with the local records, STUNTS) -> OPTIONS (brightness, CHEAT ENTRY with a valid and an invalid code);
    //           the UI sound / narration / music requests and the event player's voices are logged and checked.
    //   Popups  (needs a save from Setup) the corrupt-save question (the save made unreadable, restored after), the DLC bind error
    //           and storage notification popups, the Save Popup while the game saves.
    // The user's PlayerPrefs keys are restored at the end; the bench's own productName keeps its save apart.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.FrontEndPagesTests.Setup (no -quit)
    public static class FrontEndPagesTests
    {
        public static void Compile() { Debug.Log("[FePages] compiled"); if (Application.isBatchMode) EditorApplication.Exit(0); }
        public static void Setup() => Begin("setup");
        /// Edit mode: the UI music themes, the gamma ramp, the save check and the game's answers through "lua_ui" (stunts, records).
        public static void Quick()
        {
            report.Clear(); bad = 0; mode = "quick";
            try
            {
                var th = FeUiMusic.Describe();
                foreach (var t in th) Log("theme " + t);
                Check(th.Count == 3, "three UI music themes in MusicAudioMap: " + th.Count);
                foreach (int g in new[] { 0, 1 }) foreach (int b in new[] { 45, 50, 55 })
                {
                    PlayerPrefs.SetInt("ReflexUI.Gamma", g); PlayerPrefs.SetInt("ReflexUI.Brightness", b);
                    var r = ReflexScreenAdjustment.Ramp();
                    Log($"gamma {g} brightness {b}: e {ReflexScreenAdjustment.Exponent:F4} offset {ReflexScreenAdjustment.Offset:F2} ramp[0] {r[0]:F4} [64] {r[64]:F4} [128] {r[128]:F4} [192] {r[192]:F4} [255] {r[255]:F4}");
                }
                PlayerPrefs.DeleteKey("ReflexUI.Gamma"); PlayerPrefs.DeleteKey("ReflexUI.Brightness");
                if (ReflexProfile.HasSave) Check(!TitleLoadGameScript.SaveLooksCorrupt(), "the bench's save reads (" + new FileInfo(ReflexProfile.SaveFile).Length + " bytes)");
                ReflexProfile.Disabled = false;
                var open = ReflexProfile.Open(); open.Wait(120000);
                Check(open.IsCompleted && open.Result, "profile session: " + ReflexProfile.Error);
                var t0 = DateTime.Now;
                string stunts = FePopupScripts.AskGame("local o = \"\"; for stuntTier = 1, 4 do local c = App.GetStuntDescriptorTierContainer( stuntTier ); for i = 0, c:GetNumStuntDescriptors( ) - 1 do o = o .. stuntTier .. \"|\" .. c:GetStuntDescriptorName( i ) .. \"|\" .. c:GetStuntDescriptorDisplayNameLocalization( i ) .. \"^\" end end; ReflexAnswer = o");
                Log($"stunts ({(DateTime.Now - t0).TotalMilliseconds:F0} ms): " + (stunts == null ? "null" : stunts.Length + " chars, " + stunts.Split('^').Length + " rows: " + stunts.Substring(0, Math.Min(300, stunts.Length))));
                Check(stunts != null && stunts.Split('^').Length > 20, "the trick library from the game");
                string ev = FePopupScripts.AskGame("App.SetUIVariableString( \"EventType\", \"National\" ); ReflexAnswer = App.GetUIVariableInt( \"EventHasLaps\" ) .. \",\" .. App.GetUIVariableInt( \"EventHasTimer\" ) .. \",\" .. App.GetMXProfile( 0 ):GetNetworkName( ) .. \",\" .. App.GetExclusiveDeviceIndex( )");
                Log("EventType National -> EventHasLaps, EventHasTimer, network name, exclusive device: " + ev);
                string err = FePopupScripts.AskGame("ReflexAnswer = nil .. 1");
                Check(err != null && err.StartsWith("ERR "), "a Lua error comes back: " + err);
                // the local records of every location with a PlayerRecords row (after a race: CareerTests.Create)
                string recs = FePopupScripts.AskGame("local t = App.LookupPlayerTable( \"PlayerRecords\", 0 ); ReflexAnswer = tostring( t:numRows( ) )");
                Log("PlayerRecords rows: " + recs);
                var pr = ReflexProfile.Current?.T("PlayerRecords");
                if (pr != null) for (int i = 0; i < pr.rows; i++) Log("  PlayerRecords[" + i + "]: " + string.Join(", ", pr.columns.Select(c => c.Key + "=" + (c.Value.Count > i ? c.Value[i] : null))));
                var w = FeTables.Shared.Rows("EventTypeLocations").Where(r => FeTables.I(r, "EventTypeId") == 14).Select(r => $"etl {FeTables.I(r, "EventTypeLocationId")} loc {FeTables.I(r, "EventLocationId")} track {FeTables.I(r, "TrackId", 0)}");
                Log("Waypoint EventTypeLocations: " + string.Join("; ", w));
                Log("records WaypointRace FR_Alpine? " + FePopupScripts.AskGame(MotocardRecordsScript.RecordsLua("WaypointRace", 4, 1, -1)) + " / " + FePopupScripts.AskGame("local h = App.GetMXTableHelper( ); local e = h:MapEventTypeId( App.GetMXActivityType( \"WaypointRace\" ) ); ReflexAnswer = tostring( e ) .. \",\" .. tostring( h:GetEventTypeLocationId( e, 4, 1 ) )"));
                foreach (var r in FeTables.Shared.Rows("EventTypeLocations").Where(r => new[] { 9, 14, 10, 17, 7, 8, 16 }.Contains(FeTables.I(r, "EventTypeId"))))
                {
                    string act = FeTables.S(FeTables.Shared.Rows("EventType").FirstOrDefault(e => FeTables.I(e, "EventTypeId") == FeTables.I(r, "EventTypeId")), "ActivityName");
                    string a = FePopupScripts.AskGame(MotocardRecordsScript.RecordsLua(act, FeTables.I(r, "EventLocationId"), FeTables.I(r, "TrackId", 0), -1));
                    var rows = MotocardRecordsScript.ParseRecords(a);
                    if (rows.Count > 0 || (a != null && a.StartsWith("ERR"))) Log($"records {act} location {FeTables.I(r, "EventLocationId")} track {FeTables.I(r, "TrackId", 0)}: {a}");
                }
                t0 = DateTime.Now; string cheat = FePopupScripts.AskGame("if( App.ProcessCheatEntryString( \"zzz\" ) == TRUE ) then ReflexAnswer = \"1\" else ReflexAnswer = \"0\" end");
                Log($"cheat zzz: {cheat} ({(DateTime.Now - t0).TotalMilliseconds:F0} ms)");
            }
            catch (Exception e) { Check(false, "exception " + e); }
            ReflexProfile.Close();
            Log("quick: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
        public static void Popups() => Begin("popups");

        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage; static double stageAt, started; static string shots, mode;
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode", "ReflexUI.Gamma", "ReflexUI.Brightness" };
        static Dictionary<string, string> prefs;
        static int profileVersion, gearBefore = -1; static string saveBackup; static float gammaE0;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[FePages] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }

        static void Begin(string m)
        {
            mode = m; report.Clear(); bad = 0; stage = 0; ReflexProfile.Disabled = false;
            prefs = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            shots = Environment.GetEnvironmentVariable("FE2_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "fe2-shots");
            Directory.CreateDirectory(shots);
            lock (FeSounds.Log) FeSounds.Log.Clear();
            if (m == "setup")
            {
                if (Directory.Exists(ReflexProfile.RemoteDir)) Directory.Delete(ReflexProfile.RemoteDir, true);
                string persona = Path.Combine(ReflexProfile.SaveDir, "steam_persona.txt"); if (File.Exists(persona)) File.Delete(persona);
                Log("fresh profile: save and persona deleted");
            }
            if (m == "popups")
            {
                if (!ReflexProfile.HasSave) { Check(false, "no save (run Setup first)"); End(); return; }
                saveBackup = ReflexProfile.SaveFile + ".fe2bak"; File.Copy(ReflexProfile.SaveFile, saveBackup, true);
                // keep the Steam Cloud trailer (PCStorageDevice::vf5 hands the file over) but break the document: FileCorrupt
                var b = File.ReadAllBytes(ReflexProfile.SaveFile);
                for (int i = b.Length / 2; i < Math.Min(b.Length - 4, b.Length / 2 + 64); i++) b[i] ^= 0x5a;   // inside the zlib stream
                File.WriteAllBytes(ReflexProfile.SaveFile, b);
                Log("save made unreadable (trailer kept), backup " + saveBackup);
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
        static void Go(int s) { stage = s; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static bool Idle(FrontEndManager fm) => fm && fm.assets != null && !fm.Busy;
        static bool At(FrontEndManager fm, string page, double after = 1.0, double timeout = 60)
        {
            if (Idle(fm) && fm.CurrentPage == page && fm.TopPopup == null && Age > after && !ReflexProfile.Busy) return true;
            if (Age > timeout) Fail("page " + page + " not reached (at " + fm?.CurrentPage + " / " + fm?.TopPopup + ")");
            return false;
        }
        static bool Popup(FrontEndManager fm, string popup, double after = 0.8, double timeout = 30)
        {
            if (fm && fm.TopPopup == popup) { if (popupSeen != popup) { popupSeen = popup; popupSince = EditorApplication.timeSinceStartup; } if (EditorApplication.timeSinceStartup - popupSince > after) { popupSeen = null; return true; } }
            if (Age > timeout) Fail("popup " + popup + " not shown (at " + fm?.CurrentPage + " / " + fm?.TopPopup + ")");
            return false;
        }
        static string popupSeen; static double popupSince;
        static T Script<T>(FrontEndManager fm, string layer) where T : FeScript => fm.Layer(layer).script as T;
        static int SkinTone() { var g = ReflexProfile.Current?.T("PlayerRiderGear"); if (g == null) return -1; for (int i = 0; i < g.rows; i++) { int id = g.Int("RiderGearId", i); if (FeMotocardScripts.RowType(id) == 10) return id; } return -1; }
        static string Gear() { var g = ReflexProfile.Current?.T("PlayerRiderGear"); return g == null ? "" : string.Join(",", Enumerable.Range(0, g.rows).Select(i => g.Int("RiderGearId", i))); }
        static bool Heard(string fragment) { lock (FeSounds.Log) return FeSounds.Log.Any(l => l.IndexOf(fragment, StringComparison.OrdinalIgnoreCase) >= 0); }

        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > 1200) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 1000) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                if (fm && fm.SavePopupShown && !savePopupShot) { savePopupShot = true; Shot("save_popup"); Check(fm.TopPopup == "Save Popup", "the Save Popup is up while the game saves: " + fm.TopPopup); }
                if (mode == "setup") StepSetup(fm); else StepPopups(fm);
            }
            catch (Exception e) { Fail("exception: " + e); }
        }
        static bool savePopupShot;

        static void StepSetup(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 3)) { Shot("00_title"); fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage == "Autosave Notice Page" && Age > 1) { Shot("01_autosave"); fm.DispatchKey("Accept"); Next(); }
                    else if (Age > 120) Fail("a fresh profile did not reach the autosave notice: " + fm?.CurrentPage + " / " + ReflexProfile.Error);
                    break;
                case 2:
                    if (Idle(fm) && fm.CurrentPage == "Motocard Setup Name Page" && Age > 1 && !named)
                    { named = true; Script<MotocardNameEntryScript>(fm, "MotocardNameEntry.lyr")?.Input?.Type("Tester"); fm.DispatchKey("Accept"); }
                    if (At(fm, "Screen Adjustment Page", 1.0, 120))
                    {
                        Shot("02_screen_adjustment");
                        gammaE0 = ReflexScreenAdjustment.Exponent;
                        int g0 = ReflexScreenAdjustment.Gamma;
                        fm.Navigate(g0 == 0 ? 1 : -1, 0);
                        Check(ReflexScreenAdjustment.Gamma != g0 && Mathf.Abs(Shader.GetGlobalVector("_ReflexGammaParams").x - ReflexScreenAdjustment.Exponent) < 1e-5f && ReflexScreenAdjustment.Exponent != gammaE0,
                              $"gamma {g0} -> {ReflexScreenAdjustment.Gamma}: exponent {gammaE0:F4} -> {ReflexScreenAdjustment.Exponent:F4} (_ReflexGammaParams {Shader.GetGlobalVector("_ReflexGammaParams")}), ramp[128] {ReflexScreenAdjustment.Ramp()[128]:F4}");
                        Shot("02b_screen_adjustment_lcd");
                        fm.Navigate(g0 == 0 ? -1 : 1, 0);
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 3:
                    if (At(fm, "Motocard Setup Welcome Page", 2.5))
                    {
                        Shot("03_welcome");
                        Check(Heard("UI_GAME_INTRO/VO/INTRO -> ui_game_intro_eng/vo/intro"), "the welcome page's narration: UI_Game_Intro -> UI_GAME_INTRO_ENG/VO/INTRO");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 4:
                    if (At(fm, "Motocard Setup Vehicle page", 3))
                    {
                        var v = Script<MotocardSetupVehicleScript>(fm, "MotocardSetupVehicle.lyr");
                        Shot("04_vehicle");
                        Check(v != null && v.Model >= 0, "the setup vehicle: the favourite MX Lite, model " + v?.Model + " (garage " + v?.Garage?.Shown + ")");
                        Check(Heard("ui_moto_vehicle_eng/vo/vehicle"), "the vehicle page's narration");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 5:
                    if (At(fm, "Motocard Setup Gear Page", 3))
                    {
                        Shot("05_gear");
                        gearBefore = Gear().GetHashCode(); Log("gear set 1: " + Gear());
                        profileVersion = ReflexProfile.Version;
                        fm.Navigate(1, 0); Next();
                    }
                    break;
                case 6:
                    if (ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                    {
                        Log("gear set 2: " + Gear());
                        Check(Gear().GetHashCode() != gearBefore && SkinTone() >= 0, "the second gear set equipped by the game's Lua (PlayerRiderGear " + Gear() + ", skin tone " + SkinTone() + ")");
                        Shot("06_gear_set2");
                        Check(Heard("ui_moto_gear_eng/vo/gear"), "the gear page's narration");
                        fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 30) Fail("the gear set did not change");
                    break;
                case 7:
                    if (At(fm, "Motocard Setup Skintone Page", 2.5))
                    {
                        Shot("07_skintone");
                        fm.Navigate(1, 0);
                        var r = Script<MotocardSetupRiderScript>(fm, "MotocardSetupRider.lyr");
                        Log("skin tone focused: " + r?.Skintone);
                        Check(Heard("ui_moto_skintone_eng/vo/skintone"), "the skin tone page's narration");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 8:
                    if (At(fm, "Motocard Setup Jersey Name Page", 2.5))
                    {
                        Check(SkinTone() == 512, "the skin tone written by End: " + SkinTone() + " (Black = 512)");
                        Shot("08_jersey_name");
                        fm.DispatchKey("Toggle"); Next();
                    }
                    break;
                case 9:
                    if (Popup(fm, "JerseyNameEntryPopup"))
                    {
                        Script<JerseyEntryPopupScript>(fm, "JerseyNameEntry.lyr")?.Input?.Type("fe2");
                        Shot("09_jersey_name_popup"); profileVersion = ReflexProfile.Version; fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 10:
                    if (fm.TopPopup == null && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 1)
                    {
                        Check(ReflexProfile.Current.T("PlayerRider").Str("Name", 0) == "FE2", "the jersey name set by the game: " + ReflexProfile.Current.T("PlayerRider").Str("Name", 0));
                        Check(Heard("ui_moto_name_eng/vo/name"), "the jersey name page's narration");
                        fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 30) Fail("the jersey name was not applied");
                    break;
                case 11:
                    if (At(fm, "Motocard Setup Jersey Number Page", 2.5)) { Shot("10_jersey_number"); Check(Heard("ui_moto_number_eng/vo/number"), "the jersey number page's narration"); fm.DispatchKey("Accept"); Next(); }
                    break;
                case 12:
                    if (At(fm, "Motocard Setup Completion", 2.5))
                    {
                        Shot("11_completion");
                        Check(Heard("COMPLETE_[PLATFORM] -> ui_moto_complete_eng/vo/complete_pc"), "the completion page asks for COMPLETE_PC (not in the data: silent, as the PC game)");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 13:
                    if (At(fm, "Main Menu Page", 2))
                    {
                        Check(ReflexProfile.Current.MotocardSetup, "PlayerRider.MotocardSetup set by the game");
                        Shot("12_main_menu");
                        var mus = fm.sounds?.music;
                        Check(mus != null && mus.ThemeName != null && mus.IsPlaying && mus.Volume > 0, $"UI music: theme {mus?.ThemeName} ({mus?.ThemeIndex}), playing {mus?.IsPlaying}, volume {mus?.Volume:F3}, tier {mus?.Tier:F2}");
                        fm.Navigate(1, 0); fm.Navigate(1, 0); fm.Navigate(1, 0); Next();
                    }
                    break;
                case 14: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;   // MOTOCARD
                case 15:
                    if (At(fm, "Motocard Page", 2))
                    {
                        Shot("13_motocard");
                        for (int i = 0; i < 4; i++) fm.Navigate(0, 1);
                        Next();
                    }
                    break;
                case 16:
                    if (Age > 1)
                    {
                        var ml = Script<MotocardLayerScript>(fm, "Motocard.lyr");
                        Shot("14_motocard_highlights");
                        Check(ml?.Highlights != null && ml.Highlights.Count > 0, "the highlights list: " + ml?.Highlights?.Count + " highlights, " + ml?.Highlights?.Unlocked + " unlocked");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 17:
                    if (Age > 0.5)
                    {
                        var ml = Script<MotocardLayerScript>(fm, "Motocard.lyr");
                        Check(ml != null && ml.HighlightsFocused, "HIGHLIGHTS focuses the highlights list");
                        fm.Navigate(0, 1); fm.Navigate(0, 1);
                        Shot("15_motocard_highlights_list");
                        fm.DispatchKey("Back"); Next();
                    }
                    break;
                case 18:
                    if (Age > 0.5)
                    {
                        var ml = Script<MotocardLayerScript>(fm, "Motocard.lyr");
                        Check(ml != null && !ml.HighlightsFocused && fm.CurrentPage == "Motocard Page", "Back leaves the highlights list (still on the motocard)");
                        for (int i = 0; i < 4; i++) fm.Navigate(0, -1);
                        fm.DispatchKey("Accept"); Next();   // LEADERBOARDS
                    }
                    break;
                case 19:
                    if (At(fm, "Motocard Leaderboard Page", 2))
                    {
                        var rs = Script<MotocardRecordsScript>(fm, "MotocardRecords.lyr");
                        Shot("16_records");
                        Check(rs != null && rs.TypeCount > 0 && rs.LocationCount > 0, "the records page: " + rs?.TypeCount + " activity types, " + rs?.LocationCount + " locations");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 20: if (Age > 1) { Shot("17_records_locations"); fm.DispatchKey("Accept"); Next(); } break;
                case 21:
                    {
                        var rs = Script<MotocardRecordsScript>(fm, "MotocardRecords.lyr");
                        if (rs != null && rs.LastAnswer != null && Age > 1.5)
                        {
                            Shot("18_records_local");
                            Check(rs.LastAnswer.Contains("#"), "the local records came back from the game's Lua: " + rs.RecordCount + " rows (" + rs.LastAnswer.Replace("\n", " | ") + ")");
                            fm.DispatchKey("Back"); fm.DispatchKey("Back"); Next();
                        }
                        else if (Age > 30) Fail("no local records answer");
                    }
                    break;
                case 22:
                    if (At(fm, "Motocard Page", 1.5)) { fm.Navigate(0, 1); fm.DispatchKey("Accept"); Next(); }   // STUNTS
                    break;
                case 23:
                    if (At(fm, "Motocard Stunt Page", 2))
                    {
                        var st = Script<MotocardStuntsScript>(fm, "MotocardStunts.lyr");
                        if (st != null && st.Preview.Loaded && Age > 3)
                        {
                            Shot("19_stunts");
                            Check(st.Preview.data.Count > 20, "the trick library from App.GetStuntDescriptorTierContainer: " + st.Preview.data.Count + " stunts (" + string.Join(", ", st.Preview.data.Take(6).Select(d => d.tier + ":" + d.name + "=" + d.key)) + " ...)");
                            for (int i = 0; i < 6; i++) fm.Navigate(0, 1);
                            Shot("20_stunts_scrolled");
                            fm.DispatchKey("Back"); Next();
                        }
                    }
                    break;
                case 24: if (At(fm, "Motocard Page", 1.5)) { fm.DispatchKey("Back"); Next(); } break;
                case 25:
                    if (At(fm, "Main Menu Page", 1.5)) { fm.DispatchKey("Toggle"); Next(); }   // OPTIONS
                    break;
                case 26:
                    if (At(fm, "Options Page", 1.5))
                    {
                        Shot("21_options");
                        float off0 = ReflexScreenAdjustment.Offset;
                        fm.Navigate(0, 1); fm.Navigate(1, 0);   // BRIGHTNESS +10 %
                        Check(Mathf.Abs(ReflexScreenAdjustment.Offset - off0 - 0.02f) < 1e-4f && Mathf.Abs(Shader.GetGlobalVector("_ReflexGammaParams").y - ReflexScreenAdjustment.Offset) < 1e-5f,
                              $"BRIGHTNESS {ReflexScreenAdjustment.Brightness}: ramp offset {off0:F2} -> {ReflexScreenAdjustment.Offset:F2}");
                        fm.Navigate(-1, 0); fm.Navigate(0, -1);
                        for (int i = 0; i < 14; i++) fm.Navigate(0, 1);   // CHEAT ENTRY
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 27:
                    if (Popup(fm, "CheatEntryPopup"))
                    {
                        Script<CheatEntryPopupScript>(fm, "CheatEntry.lyr")?.Input?.Type("gearedup");
                        Shot("22_cheat_entry"); fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 28:
                    if (Popup(fm, "CheatCodePopup", 1.0))
                    {
                        Shot("23_cheat_gearedup");
                        Check(fm.Var<string>("CheatCodeEntry") == "gearedup", "the game accepted 'gearedup' (CheatCodeEntry " + fm.Var<string>("CheatCodeEntry") + ")");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 29:
                    if (fm.TopPopup == null && Age > 1) { fm.DispatchKey("Accept"); Next(); }   // CHEAT ENTRY again
                    break;
                case 30:
                    if (Popup(fm, "CheatEntryPopup"))
                    {
                        Script<CheatEntryPopupScript>(fm, "CheatEntry.lyr")?.Input?.Type("notacheat");
                        fm.DispatchKey("Accept"); Next();
                    }
                    break;
                case 31:
                    if (Popup(fm, "CheatCodePopup", 1.0))
                    {
                        Shot("24_cheat_invalid");
                        Check(fm.Var<string>("CheatCodeEntry") == "Invalid", "the game refused 'notacheat' (CheatCodeEntry " + fm.Var<string>("CheatCodeEntry") + ")");
                        fm.DispatchKey("Back"); Next();
                    }
                    break;
                case 32:
                    if (fm.TopPopup == null && Age > 1) { fm.DispatchKey("Back"); Next(); }
                    break;
                case 33:
                    if (At(fm, "Main Menu Page", 1.5)) { Sounds(fm); Go(1000); EditorApplication.ExitPlaymode(); }
                    break;
            }
        }
        static bool named;

        static void Sounds(FrontEndManager fm)
        {
            List<string> log; lock (FeSounds.Log) log = FeSounds.Log.ToList();
            Log("front-end sound requests (" + log.Count + "):");
            foreach (var l in log.Distinct()) Log("  " + l + " x" + log.Count(x => x == l));
            var a = fm.sounds?.Player;
            if (a != null)
            {
                Log("event player: " + a.Projects.Count + " projects (" + string.Join(", ", a.Projects) + "), clips " + a.ClipsLoaded + "/" + a.ClipsTotal);
                foreach (var kv in a.Stats.OrderBy(k => k.Key)) Log($"  {kv.Key}: starts {kv.Value.starts}, voices {kv.Value.voices}, unknown {kv.Value.unknown}, channel {kv.Value.channel}");
                Check(a.Stats.TryGetValue("ui_02/ui/accept", out var acc) && acc.voices > 0, "UI_02/UI/ACCEPT played through the event player");
                if (mode != "setup") return;
                Check(a.Stats.TryGetValue("ui_game_intro_eng/vo/intro", out var vo) && vo.voices > 0 && vo.channel == ReflexChannel.VoiceOver, "the game intro narration played (VoiceOver channel)");
                Check(a.Stats.TryGetValue("ui_moto_complete_eng/vo/complete_pc", out var c) && c.voices == 0, "COMPLETE_PC: no such event, silent");
            }
        }

        static void StepPopups(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 3)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Popup(fm, "StorageDeviceQuestionPopup", 1.0, 120))
                    {
                        Shot("00_storage_corrupted");
                        Check(TitleLoadGameScript.LastRoute == "corrupt", "the unreadable save routes to the corrupt-save question: " + TitleLoadGameScript.LastRoute);
                        fm.Navigate(1, 0); fm.DispatchKey("Accept"); Next();   // BUTTON TWO: continue without saving -> main menu
                    }
                    break;
                case 2:
                    if (At(fm, "Main Menu Page", 2))
                    {
                        Shot("01_after_corrupt_buttontwo");
                        File.Copy(saveBackup, ReflexProfile.SaveFile, true); File.Delete(saveBackup); saveBackup = null;
                        Log("save restored");
                        fm.vars["StorageNotification"] = "DeviceOutOfSpace"; fm.SendUIEvent("ShowStorageNotificationPopup"); Next();
                    }
                    break;
                case 3: if (Popup(fm, "StorageNotificationPopup")) { Shot("02_storage_out_of_space"); fm.DispatchKey("Accept"); Next(); } break;
                case 4: if (fm.TopPopup == null && Age > 1) { fm.SendUIEvent("ShowDLCBindErrorPopup"); Next(); } break;
                case 5: if (Popup(fm, "DLCBindErrorPopup")) { Shot("03_dlc_bind_error"); fm.DispatchKey("Accept"); Next(); } break;   // ButtonTwo focused: cancel -> title
                case 6:
                    if (At(fm, "Title Page", 2)) { Shot("04_after_dlc_cancel"); fm.DispatchKey("Accept"); Next(); }
                    break;
                case 7:
                    // the restored save loads again; the motocard flag reaches the file only with the save after the setup's completion
                    // (MotocardWelcome sets it in End, after its save), so a save made by the setup alone routes to Create Motocard
                    if (Idle(fm) && (fm.CurrentPage == "Main Menu Page" || fm.CurrentPage == "Autosave Notice Page") && Age > 2 && !ReflexProfile.Busy)
                    {
                        Check(TitleLoadGameScript.LastRoute == (ReflexProfile.Current.MotocardSetup ? "main" : "create"), "the restored save loads again (DLC valid): " + TitleLoadGameScript.LastRoute + " -> " + fm.CurrentPage);
                        ReflexProfile.Run("set_int PlayerRider 0 NameStatus 0", "save");   // a changed table and App.RequestSaveGame: the Save Popup
                        Go(9);
                    }
                    else if (Age > 120) Fail("the restored save did not load: " + fm?.CurrentPage + " / " + fm?.TopPopup);
                    break;
                case 9:
                    if (Age > 8 || savePopupShot) { Check(savePopupShot, "the Save Popup was shown while the game saved"); Sounds(fm); Go(1000); EditorApplication.ExitPlaymode(); }
                    break;
            }
        }

        // ---------------------------------------------------------------- the game's MusicPlayer in a race (MUSIC_TRACE_EVENT, MUSIC_TRACE_SECONDS)
        // Reads G + 0x15c6c (MusicPlayer: +8 mode, +0xc jukebox state, +0x1d0 requested / +0x1d4 current track, +0x1d8 delay, +0x1dc
        // activity, +0x55c/+0x560/+0x570 playlist / count / position) and the music channel ramp 00d67ab4..ac0 every 0.5 s between the
        // world's frames, with OriginalAudio's MUSIC_IN_GAME stats and OriginalMusic's track.
        static double traceAt, traceStart, traceDrop; static string lastTrace;
        public static void MusicTrace()
        {
            string key = Environment.GetEnvironmentVariable("MUSIC_TRACE_EVENT") ?? "National/NAT_Concord";
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = 0;
            EditorSettings.enterPlayModeOptionsEnabled = true; EditorSettings.enterPlayModeOptions = EnterPlayModeOptions.DisableDomainReload;
            UnityEditor.SceneManagement.EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            mode = "music"; report.Clear(); bad = 0; stage = 0; traceStart = EditorApplication.timeSinceStartup; traceDrop = -1;
            shots = Environment.GetEnvironmentVariable("FE2_SHOTS") ?? Application.temporaryCachePath;
            EditorApplication.update += TraceStep;
            EditorApplication.EnterPlaymode();
        }
        static void TraceStep()
        {
            try
            {
                double now = EditorApplication.timeSinceStartup;
                float total = float.Parse(Environment.GetEnvironmentVariable("MUSIC_TRACE_SECONDS") ?? "120");
                if (!EditorApplication.isPlaying) { if (stage >= 1000) { EditorApplication.update -= TraceStep; File.WriteAllText(Path.Combine(shots, "music-trace.txt"), report.ToString()); EditorApplication.Exit(0); } else if (now - traceStart > 300) { Log("play mode never started"); EditorApplication.Exit(1); } return; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var audio = UnityEngine.Object.FindAnyObjectByType<OriginalAudio>();
                var music = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>();
                if (rt == null || !rt.Running || rt.Simulation == null) { if (now - traceStart > 900) { Log("world not running"); stage = 1000; EditorApplication.ExitPlaymode(); } return; }
                if (traceDrop < 0) { traceDrop = now; Log("world running"); }
                if (now - traceAt < 0.5) return;
                traceAt = now; double rel = now - traceDrop;
                rt.Simulation.BetweenFrames(w =>
                {
                    var b = new byte[4];
                    uint U(uint a) => ReflexWorldNative.reflex_world_read(w, a, b, 4) == 0 ? BitConverter.ToUInt32(b, 0) : 0xdeadbeef;
                    float F(uint a) => ReflexWorldNative.reflex_world_read(w, a, b, 4) == 0 ? BitConverter.ToSingle(b, 0) : float.NaN;
                    uint G = U(0xa946b8), P = G + 0x15c6c, count = U(0xc01c10), cur = U(P + 0x1d4), list = U(P + 0x55c), n = U(P + 0x560);
                    string order = ""; for (uint i = 0; i < n && i < 40; i++) order += (i > 0 ? "," : "") + (int)U(list + 4 * i);
                    uint rec = (int)cur >= 0 && cur < count ? U(U(0xc01c0c) + 4 * cur) : 0;
                    string t = $"mode {U(P + 8)} juke {U(P + 0xc)} req {(int)U(P + 0x1d0)} cur {(int)cur} recState {(rec != 0 ? U(rec + 0x34) : 0)} delay {F(P + 0x1d8):F2} act {(int)U(P + 0x1dc)} flags {U(P + 4):x} +130 {U(P + 0x130)} pos {(int)U(P + 0x570)} n {n} ramp {F(0xd67ab4):F3}->{F(0xd67ab8):F3} from {F(0xd67abc):F3} t {F(0xd67ac0):F3} tracks {count} order [{order}]";
                    lock (report) { if (t != lastTrace) { lastTrace = t; report.AppendLine($"{rel:F1}s {t}"); } }
                });
                {
                    string m = $"OriginalMusic following {music?.FollowingGame} '{music?.NowPlaying}' playing {music?.CurrentClip != null && music.GetComponent<AudioSource>().isPlaying} ramp {(music ? music.Ramp : -1):F1} tracks [{(music ? string.Join(",", music.GameTracksPlayed) : "")}]";
                    if (m != lastMusic) { lastMusic = m; lock (report) report.AppendLine($"{now - traceDrop:F1}s {m}"); }
                }
                if (rt) rt.testHeldKeys = new[] { 0x11 };
                // MUSIC_TRACE_SKIP: the first track ends early (its source stopped) at 60 s: the next playlist entry must follow
                if (Environment.GetEnvironmentVariable("MUSIC_TRACE_SKIP") == "1" && !skipped && now - traceDrop > 60 && music && music.GameTracksPlayed.Count == 1)
                { skipped = true; music.GetComponent<AudioSource>().Stop(); lock (report) report.AppendLine($"{now - traceDrop:F1}s (test) the track's source stopped"); }
                if (now - traceDrop > total) { Debug.Log("[FePages] music trace:\n" + report); stage = 1000; EditorApplication.ExitPlaymode(); }
            }
            catch (Exception e) { Debug.LogError("[FePages] trace " + e); stage = 1000; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); }
        }
        static string lastMusic; static bool skipped;

        static void Fail(string why) { Check(false, why); stage = 1000; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            ReflexProfile.Close();
            if (saveBackup != null && File.Exists(saveBackup)) { File.Copy(saveBackup, ReflexProfile.SaveFile, true); File.Delete(saveBackup); Log("save restored"); }
            foreach (var k in new[] { "ReflexUI.Gamma", "ReflexUI.Brightness" })
            {   // the motocard / options walks change them: back to what they were
                var v = prefs.TryGetValue(k, out var s) ? s : null;
                if (v == null) PlayerPrefs.DeleteKey(k); else PlayerPrefs.SetInt(k, int.Parse(v.Substring(v.IndexOf('|') + 1)));
            }
            PlayerPrefs.Save();
            var after = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            Check(after.All(kv => kv.Value == prefs[kv.Key]), "user PlayerPrefs keys untouched");
            Log("fe pages " + mode + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "fepages-" + mode + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
