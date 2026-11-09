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
    // The original front end (Reflex.Port.FrontEnd) read from the install, checked and driven page by page in edit mode:
    //   Verify: page graph, layers of every page of the flow, strings, posters, location lists / rules data against the tables
    //           and the Lua rules (durations, AI levels, vehicle classes), the event keys handed to the race scene.
    //   Bench:  boots the front end, walks title -> profile reminder -> main menu -> arcade event select -> locations -> rules ->
    //           accept (the race hand-over is intercepted) and the popups, writing a 1280x720 PNG per page to $FRONTEND_SHOTS.
    // Neither writes PlayerPrefs (the hand-over is intercepted); both check that ReflexEvent / ReflexAI are unchanged.
    // Run: Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Editor.FrontEndContentTests.Verify (or .Bench)
    public static class FrontEndContentTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[FrontEndVerify] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }

        static (bool hasE, string e, bool hasA, int a) Prefs() => (PlayerPrefs.HasKey(ReflexEventSelect.PrefsKey), PlayerPrefs.GetString(ReflexEventSelect.PrefsKey, ""), PlayerPrefs.HasKey(ReflexEventSelect.AiKey), PlayerPrefs.GetInt(ReflexEventSelect.AiKey, 0));

        static void Finish(string name, (bool, string, bool, int) prefsBefore)
        {
            Check(Prefs().Equals(prefsBefore), "PlayerPrefs ReflexEvent / ReflexAI untouched");
            Log(name + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            string outDir = Environment.GetEnvironmentVariable("FRONTEND_SHOTS");
            if (!string.IsNullOrEmpty(outDir)) { Directory.CreateDirectory(outDir); File.WriteAllText(Path.Combine(outDir, name + "-report.txt"), report.ToString()); }
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        public static void Verify()
        {
            report.Clear(); bad = 0; var before = Prefs();
            try
            {
                var tables = FeTables.Shared;
                // event types of EventSelect.lua and their posters
                foreach (var a in new[] { "Freeride", "National", "Supercross", "Terracross", "Freestyle", "Powercross", "WaypointRace" })
                {
                    var r = tables.EventTypeOf(a);
                    Check(r != null, $"EventType row for {a}: id {FeTables.I(r, "EventTypeId")} icon {FeTables.I(r, "IconId")}");
                    Check(tables.IconAsset(FeTables.I(r, "IconId"), out var asset, out var pkg, out var db) && db == "MXUI" && pkg == "ActivityTypePosters", $"  poster {asset} in {db}/{pkg}");
                }
                // durations (BuildDurationList) for National: 2..10 by 1, then 15..30 by 5; default index 1 = 3 laps
                var d = tables.Durations(10, out var sing, out var plur, out var type);
                Check(d != null && string.Join(",", d) == "2,3,4,5,6,7,8,9,10,15,20,25,30" && sing == "STR_LAP" && plur == "STR_LAPS" && type == "Laps", "National durations " + (d == null ? "null" : string.Join(",", d)));
                var w = tables.Durations(14, out _, out _, out _);
                Check(w != null && w.Count == 1 && w[0] == 1, "Waypoint durations " + (w == null ? "null" : string.Join(",", w)));
                // vehicle classes containing the default vehicle type (MX Lite, VehicleTypeId 2)
                int vt = tables.DefaultVehicleTypeId();
                var vc = tables.VehicleClasses(10, vt);
                Check(vt == 2 && string.Join(",", vc.Select(c => c.id)) == "3,12,11,15", "National vehicle classes (the game's Lua sort, ties as table.sort leaves them) for type " + vt + ": " + string.Join(", ", vc.Select(c => c.id + " " + c.key + " (" + c.types + ")")));
                // locations: every unlocked one maps to a ReflexEvents event with the same type / location / track
                int total = 0, unlocked = 0;
                var arcadeTypes = new[] { 9, 10, 8, 17, 7, 16, 14 };   // EventSelect.lua's types (Section Race and the challenges are career-only)
                foreach (int t in arcadeTypes)
                {
                    var locs = tables.Locations(t);
                    total += locs.Count; unlocked += locs.Count(l => !l.locked);
                    Log($"type {t}: " + string.Join(", ", locs.Select(l => (l.locked ? "[locked] " : "") + l.filename + (l.trackId != 0 ? "/" + l.trackId : "") + " -> " + (l.ev?.key ?? "-"))));
                    foreach (var l in locs.Where(l => !l.locked)) Check(l.ev.spec.typeId == t && l.ev.spec.locationId == l.eventLocationId && l.ev.spec.trackId == l.trackId && ReflexEvents.Find(l.ev.key) == l.ev, "  " + l.ev.key);
                }
                var all = ReflexEvents.All;
                int arcade = all.Count(e => arcadeTypes.Contains(e.spec.typeId));
                Check(unlocked == arcade, $"unlocked arcade locations {unlocked} == ReflexEvents.All of the arcade types {arcade} (of {all.Count}; listed {total})");
                // strings
                var assets = new FeAssets(Shader.Find("Reflex/FrontEnd UI")) { tables = tables };
                foreach (var k in new[] { "STR_PRESS_START_TO_BEGIN", "STR_MAINMENU", "STR_ARCADE", "STR_CAREER", "STR_EVENT_SELECT", "STR_LOCATION_SELECT", "STR_AI_LEVEL2", "STR_MXACTIVITYTYPE_NATIONAL", "STR_1555" })
                    Check(assets.Str(k) != k, $"{k} = \"{assets.Str(k)}\"");
                // icons: posters, previews and track maps of the National locations decode
                foreach (var l in tables.Locations(10).Take(3))
                {
                    var p = assets.Icon(l.posterIconId); var v = assets.Icon(l.iconId); var m = assets.Icon(l.mapIconId);
                    Check(p.tex && v.tex && m.tex, $"icons of {l.filename}: poster {p.tex?.name} {p.tex?.width}x{p.tex?.height}, preview {v.tex?.name}, map {m.tex?.name}");
                }
                // layers of the flow build
                var root = new GameObject("verify", typeof(RectTransform));
                foreach (var n in new[] { "Background01.lyr", "Title.lyr", "MainMenu.lyr", "DemoMainMenu.lyr", "Header.lyr", "ArcadeBackground", "EventSelect.lyr", "LocationSelect.lyr", "LoadingScreen.lyr", "QuitConfirmation.lyr", "ProfileReminderPopup.lyr", "ProfileRequiredPopup.lyr", "NetworkNotificationPopup.lyr", "OptionsFrontEnd.lyr" })
                {
                    try { var l = new FeLayer(assets, n, (RectTransform)root.transform); Check(l.root != null, $"layer {n}: root '{l.Name}', script {OriginalUi.S(l.root.props, "Script")}"); }
                    catch (Exception e) { Check(false, $"layer {n}: {e.Message}"); }
                }
                UnityEngine.Object.DestroyImmediate(root);
                var sounds = new GameObject("snd").AddComponent<AudioSource>();
                var fs = new FeSounds(sounds); var t0 = DateTime.Now;
                while (fs.Count == 0 && fs.Error == null && (DateTime.Now - t0).TotalSeconds < 30) System.Threading.Thread.Sleep(50);
                Check(fs.Has("Accept") && fs.Has("Back") && fs.Has("LeftRightDirectional") && fs.Has("UpDownDirectional") && fs.Has("DirectionalFail") && fs.Has("PopupShow"), $"UI sounds from UIAudio/UI_02: {fs.Count} {fs.Error}");
                UnityEngine.Object.DestroyImmediate(sounds.gameObject);
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            Finish("verify", before);
        }

        // ---------------------------------------------------------------------------------------------------- bench
        static FrontEndManager fe; static Camera cam; static RenderTexture rt; static string shots;

        static void Run(float seconds) { for (float t = 0; t < seconds; t += 1f / 30f) fe.Step(1f / 30f); }

        static void Shot(string name)
        {
            Canvas.ForceUpdateCanvases();
            cam.Render();
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGB24, false);
            tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); tex.Apply();
            RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex);
            Log($"shot {name}: page '{fe.CurrentPage}' popup '{fe.TopPopup}'");
        }

        static void UseCamera(Canvas c)
        {
            c.renderMode = RenderMode.ScreenSpaceCamera; c.worldCamera = cam; c.planeDistance = 1;
        }

        // the OPTIONS page (main menu F1 / X): values come from and go to the race's own settings storage
        static readonly string[] IntSettingKeys = { "ReflexUI.DrivingCamera", "ReflexCameraMode", "ReflexOriginalHud", "ReflexUI.OnlinePlayerName", "ReflexUI.EngineVolume", "ReflexAudio.musicOn",
            "ReflexUI.Brightness", "ReflexUI.Gamma", "ReflexUI.MeasurementSystem", "ReflexUI.CareerDifficulty", "ReflexUI.Vibration", "ReflexUI.MouseSensitivity" };
        static readonly string[] FloatSettingKeys = { "ReflexAudio.master", "ReflexAudio.music" };
        static void OptionsWalk()
        {
            var ints = IntSettingKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? (int?)PlayerPrefs.GetInt(k) : null);
            var floats = FloatSettingKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? (float?)PlayerPrefs.GetFloat(k) : null);
            try
            {
                fe.DispatchKey("Toggle"); Run(0.8f); Shot("07a_options");
                Check(fe.CurrentPage == "Options Page", "F1 / OPTIONS -> options page");
                int music0 = OptionsFrontEndScript.GetSetting("MusicVolume");
                for (int i = 0; i < 7; i++) { fe.Navigate(0, 1); Run(0.05f); }
                int dir = music0 > 0 ? -1 : 1;
                fe.Navigate(dir, 0); Run(0.3f); Shot("07b_options_music_changed");
                Check(Mathf.Abs(PlayerPrefs.GetFloat("ReflexAudio.music", -1) - (music0 + dir * 10) / 100f) < 1e-4f, $"MUSIC VOLUME {music0}% -> {(music0 + dir * 10)}% written to ReflexAudio.music ({PlayerPrefs.GetFloat("ReflexAudio.music", -1)})");
                for (int i = 0; i < 7; i++) { fe.Navigate(0, -1); Run(0.05f); }
                int cam0 = OptionsFrontEndScript.GetSetting("DrivingCamera"), cd = cam0 < 3 ? 1 : -1;
                fe.Navigate(cd, 0); Run(0.2f);
                Check(PlayerPrefs.GetInt("ReflexUI.DrivingCamera", -1) == cam0 + cd && PlayerPrefs.GetInt("ReflexCameraMode", -1) == OriginalPauseMenu.CameraModeOf[cam0 + cd],
                      $"DRIVING CAMERA {cam0} -> {cam0 + cd}: ReflexUI.DrivingCamera {PlayerPrefs.GetInt("ReflexUI.DrivingCamera", -1)}, ReflexCameraMode {PlayerPrefs.GetInt("ReflexCameraMode", -1)} (the race's camera mode)");
                fe.Navigate(0, 1); fe.Navigate(0, 1); fe.Navigate(0, 1); fe.Navigate(1, 0); Run(0.2f);   // HUD VISIBILITY -> OFF
                Check(PlayerPrefs.GetInt("ReflexOriginalHud", 1) == 0, "HUD VISIBILITY OFF -> ReflexOriginalHud 0 (OriginalWorldRuntime.OriginalHudVisible)");
                Shot("07c_options_hud_off");
                fe.DispatchKey("Back"); Run(0.8f);
                Check(fe.CurrentPage == "Main Menu Page", "Back -> main menu");
            }
            finally
            {
                foreach (var kv in ints) if (kv.Value.HasValue) PlayerPrefs.SetInt(kv.Key, kv.Value.Value); else PlayerPrefs.DeleteKey(kv.Key);
                foreach (var kv in floats) if (kv.Value.HasValue) PlayerPrefs.SetFloat(kv.Key, kv.Value.Value); else PlayerPrefs.DeleteKey(kv.Key);
                PlayerPrefs.Save();
                bool same = ints.All(kv => (PlayerPrefs.HasKey(kv.Key) ? (int?)PlayerPrefs.GetInt(kv.Key) : null) == kv.Value) && floats.All(kv => (PlayerPrefs.HasKey(kv.Key) ? (float?)PlayerPrefs.GetFloat(kv.Key) : null) == kv.Value);
                Check(same, "settings PlayerPrefs restored (" + string.Join(", ", ints.Where(kv => kv.Value.HasValue).Select(kv => kv.Key)) + string.Join(", ", floats.Where(kv => kv.Value.HasValue).Select(kv => kv.Key)) + " existed)");
            }
        }

        public static void Bench()
        {
            report.Clear(); bad = 0; var before = Prefs();
            ReflexProfile.Disabled = true;   // the guest path (no profile session): title -> profile reminder
            shots = Environment.GetEnvironmentVariable("FRONTEND_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "frontend-shots");
            Directory.CreateDirectory(shots);
            ReflexEvents.Event launched = null; int launchedAi = -1;
            FrontEndManager.LaunchOverride = (ev, ai) => { launched = ev; launchedAi = ai; };
            try
            {
                rt = new RenderTexture(1280, 720, 24);
                cam = new GameObject("bench camera").AddComponent<Camera>();
                cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black; cam.targetTexture = rt; cam.orthographic = true; cam.enabled = false;
                fe = new GameObject("Reflex front end").AddComponent<FrontEndManager>();
                fe.uiShader = Shader.Find("Reflex/FrontEnd UI");
                fe.Boot();
                Check(fe.assets != null, "front end booted, status: " + fe.Status);
                UseCamera(fe.canvas);
                Run(1.0f); Shot("01_title");
                Check(fe.CurrentPage == "Title Page", "title page");
                fe.DispatchKey("Back"); Run(0.5f); Shot("02_title_quit_popup");
                Check(fe.TopPopup == "QuitConfirmationPopup", "Back on the title -> quit confirmation");
                fe.DispatchKey("Back"); Run(0.5f);
                Check(fe.TopPopup == null, "Back closes it");
                fe.DispatchKey("Accept"); Run(0.5f); Shot("03_profile_reminder");
                Check(fe.TopPopup == "ProfileReminderPopup", "Start / Accept -> profile reminder (no profile signed in)");
                fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f); Shot("04_main_menu");
                Check(fe.CurrentPage == "Main Menu Page", "CONTINUE -> main menu");
                fe.DispatchKey("Accept"); Run(0.6f); Shot("05_main_menu_motocard_required");
                Check(fe.TopPopup == "ProfileRequiredPopup", "MOTOCAREER (focused first) -> Motocard required popup");
                fe.DispatchKey("Accept"); Run(0.5f);
                fe.Navigate(1, 0); Run(0.4f); fe.DispatchKey("Accept"); Run(0.5f); Shot("06_main_menu_network");
                Check(fe.TopPopup == "NetworkNotificationPopup", "MULTIPLAYER -> network notification");
                fe.DispatchKey("Back"); Run(0.5f);
                fe.Navigate(1, 0); Run(0.3f);   // MOTOCARD is not focusable: stays on MULTIPLAYER
                fe.Navigate(-1, 0); fe.Navigate(-1, 0); Run(0.4f); Shot("07_main_menu_arcade");
                OptionsWalk();
                fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f); Shot("08_event_select");
                Check(fe.CurrentPage == "Event Select Page", "ARCADE -> event select");
                fe.Navigate(1, 0); Run(0.5f); Shot("09_event_select_national");
                fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f); Shot("09b_vehicle_select");
                Check(fe.CurrentPage == "Vehicle Select Page", "accept -> vehicle select");
                fe.DispatchKey("Accept"); Run(0.3f); fe.DispatchKey("Accept"); Run(0.3f); fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f); Shot("10_location_select");
                Check(fe.CurrentPage == "Locations Page", "vehicle select (MY RIDES, its MX Lite: the default) -> locations");
                fe.Navigate(1, 0); Run(0.5f); Shot("11_location_select_next");
                fe.DispatchKey("Misc"); Run(0.4f); Shot("12_location_track_map");
                fe.DispatchKey("Misc"); fe.DispatchKey("Toggle"); Run(0.3f);
                fe.Navigate(0, 1); Run(0.2f); fe.Navigate(1, 0); Run(0.3f); Shot("13_location_rules_ai");
                fe.DispatchKey("Toggle"); Run(0.3f);
                fe.Navigate(-1, 0); Run(0.4f);
                fe.DispatchKey("Accept"); Run(0.6f);
                // the first location of LocationSelect's order (LocationSort with Lua's table.sort: the DLC rows' SortValue 0 first when installed)
                string first = FeTables.Shared.Locations(10).FirstOrDefault()?.ev?.key;
                Check(launched != null && launched.key == first && launchedAi == 3, $"accept launches {launched?.key} AI {launchedAi} (expected {first}, 3 = HARD / STR_AI_LEVEL3)");
                // back path: TravelBack to event select, then main menu
                var loc = FeTables.Shared.Locations(10);
                Log("National locations: " + string.Join(", ", loc.Select(l => l.filename + (l.locked ? "*" : ""))));
                // the loading page
                var ov = FrontEndLoadingOverlay.Show(fe.assets, new List<string> { "Background01.lyr", "LoadingScreen.lyr" });
                UseCamera(ov.GetComponentInChildren<Canvas>());
                fe.canvas.enabled = false;
                for (int i = 0; i < 15; i++) ov.Step(1f / 30f);
                Shot("14_loading");
                UnityEngine.Object.DestroyImmediate(ov.gameObject);
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally
            {
                FrontEndManager.LaunchOverride = null;
                if (fe) UnityEngine.Object.DestroyImmediate(fe.gameObject);
                if (cam) UnityEngine.Object.DestroyImmediate(cam.gameObject);
            }
            Finish("bench", before);
        }
    
        // ---------------------------------------------------------------------------------------------------- play mode
        // PlaySmoke: builds FrontEnd.unity (+ build order), plays it, walks title -> main menu -> ARCADE -> NATIONALS -> the second
        // location (NAT_Concord) with AI None, accepts (the real hand-over: loading page, ReflexEventSelect.Launch, Reflex.unity),
        // waits for the original world to run, then quits the event the way the pause menu does and checks the front end comes
        // back at the arcade event select. rememberChoice is off and the user's PlayerPrefs keys are restored at the end.
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> savedPrefs;
        static int stage; static double stageAt; static readonly List<string> playLog = new List<string>();

        static Dictionary<string, (bool, string, int, float)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        static void RestorePrefs(Dictionary<string, (bool has, string s, int i, float f)> p)
        {
            foreach (var kv in p)
            {
                if (!kv.Value.has) { PlayerPrefs.DeleteKey(kv.Key); continue; }
                if (kv.Value.s != null && kv.Value.s != "") PlayerPrefs.SetString(kv.Key, kv.Value.s);
                else if (kv.Value.i != int.MinValue && kv.Value.i != 0) PlayerPrefs.SetInt(kv.Key, kv.Value.i);
                else if (!float.IsNaN(kv.Value.f) && kv.Value.f != 0) PlayerPrefs.SetFloat(kv.Key, kv.Value.f);
                else if (kv.Value.i == 0) PlayerPrefs.SetInt(kv.Key, 0);
            }
            PlayerPrefs.Save();
        }

        public static void PlaySmoke()
        {
            report.Clear(); bad = 0; playLog.Clear();
            ReflexProfile.Disabled = true;   // the guest path (no profile session): title -> profile reminder
            savedPrefs = SavePrefs();
            Log("prefs before: " + string.Join(", ", savedPrefs.Select(kv => kv.Key + "=" + (kv.Value.has ? kv.Value.s + "/" + kv.Value.i : "<none>"))));
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            Log("build scenes: " + string.Join(", ", EditorBuildSettings.scenes.Select(x => x.path)));
            Check(EditorBuildSettings.scenes.Length >= 2 && EditorBuildSettings.scenes[0].path == "Assets/Scenes/FrontEnd.unity" && EditorBuildSettings.scenes[1].path == "Assets/Scenes/Reflex.unity", "build order FrontEnd 0, Reflex 1");
            shots = Environment.GetEnvironmentVariable("FRONTEND_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "frontend-shots");
            Directory.CreateDirectory(shots);
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); fm.rememberChoice = false;
            stage = 0; stageAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += PlayStep;
            EditorApplication.EnterPlaymode();
        }

        static void PlayShot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var main = Camera.main;
            if (main) { c.CopyFrom(main); c.targetTexture = target; c.enabled = false; c.cullingMask = main.cullingMask | 1; }   // the canvases (layer Default)
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + name);
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;

        static void PlayStep()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) EndSmoke(); else if (Age > 120) { Check(false, "play mode never started"); EndSmoke(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                switch (stage)
                {
                    case 0: if (fm && fm.assets != null && Age > 2) { Log("front end up: " + fm.CurrentPage); fm.DispatchKey("Accept"); Next(); } else if (Age > 60) Fail("front end did not boot"); break;
                    case 1: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;                               // profile reminder: CONTINUE
                    case 2: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Main Menu Page", "main menu (play mode)"); fm.Navigate(-1, 0); fm.DispatchKey("Accept"); Next(); } break;
                    case 3: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Event Select Page", "event select (play mode): " + fm.CurrentPage + " / " + fm.TopPopup); fm.Navigate(1, 0); Next(); } break;
                    case 4: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 5: if (Age > 2 && !fm.Busy && fm.CurrentPage == "Vehicle Select Page") { fm.DispatchKey("Accept"); fm.DispatchKey("Accept"); fm.DispatchKey("Accept"); stageAt = EditorApplication.timeSinceStartup; break; }   // rider, type, vehicle: the default
                            if (Age > 2 && !fm.Busy)
                            {
                                Check(fm.CurrentPage == "Locations Page", "locations (play mode): " + fm.CurrentPage + " / " + fm.TopPopup);
                                int concord = FeTables.Shared.Locations(10).FindIndex(l => l.filename == "NAT_Concord");   // after the DLC locations when installed
                                for (int k = 0; k < concord; k++) fm.Navigate(1, 0);
                                Next();
                            }
                            break;
                    case 6: if (Age > 1) { fm.DispatchKey("Toggle"); fm.Navigate(0, 1); fm.Navigate(-1, 0); fm.Navigate(-1, 0); fm.DispatchKey("Toggle"); Next(); } break;   // AI: Amateur -> None
                    case 7: if (Age > 1) { PlayShot("20_play_locations_ai_none"); fm.DispatchKey("Accept"); Next(); } break;
                    case 8: if (FrontEndLoadingOverlay.Current && Age > 1.5) { PlayShot("21_play_loading"); Next(); } else if (Age > 30) Fail("no loading page"); break;
                    case 9:
                    {
                        var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                        var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                        if (rt && rt.statusLabel && Age > 5 && ((int)Age) % 10 == 0 && playLog.Count < (int)Age / 10) { playLog.Add(rt.statusLabel.text); Log("race: " + rt.statusLabel.text.Split('\n')[0]); }
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 3)
                        {
                            Check(sel && sel.Current == "National/NAT_Concord", "race scene event " + (sel ? sel.Current : "?"));
                            Check(rt.opponents == 0, "race scene opponents " + rt.opponents + " (AI None)");
                            Next();
                        }
                        else if (Age > 300) Fail("race world not running after 300 s");
                        break;
                    }
                    case 10: if (Age > 4) { PlayShot("22_play_race"); OriginalPauseMenu.QuitToFrontEnd(); Next(); } break;
                    case 11: if (fm && fm.assets != null && Age > 3 && !fm.Busy) { Check(fm.CurrentPage == "Event Select Page", "QUIT returns to the front end at " + fm.CurrentPage); PlayShot("23_play_back_in_front_end"); Next(); } else if (Age > 60) Fail("front end not back"); break;
                    case 12: stage = 100; EditorApplication.ExitPlaymode(); break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else EndSmoke(); }

        static void EndSmoke()
        {
            EditorApplication.update -= PlayStep;
            var after = SavePrefs();
            if (!after.All(kv => kv.Value.Equals(savedPrefs[kv.Key]))) { Log("PlayerPrefs changed during the run, restoring"); RestorePrefs(savedPrefs); after = SavePrefs(); }
            Check(after.All(kv => kv.Value.Equals(savedPrefs[kv.Key])), "user PlayerPrefs restored: " + string.Join(", ", after.Select(kv => kv.Key + "=" + (kv.Value.Item1 ? kv.Value.Item2 + "/" + kv.Value.Item3 : "<none>"))));
            Log("play smoke: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            if (!string.IsNullOrEmpty(shots)) File.WriteAllText(Path.Combine(shots, "playsmoke-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
