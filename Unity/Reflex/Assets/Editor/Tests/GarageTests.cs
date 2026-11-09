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
    // WP-G garage (_re/notes/garage.md): FrontEndGarage.cs, FrontEndGarageTuning.cs, ReflexGarage.cs, OriginalVehicleTuningPopup.cs,
    // ReflexBikeModel.HighRes.
    //   Verify: edit mode. The HR_ twins of every VehicleModel's default vehicle and of the pros' gear sets built (bindings resolved, the
    //           twin packages taken), the performance display's bars against VehiclePerformanceDisplayComponent's formula, the tuning
    //           popup's step / bounds rules.
    //   Play:   play mode, the career save. Title -> MOTOCAREER -> GARAGE -> VEHICLE (My Rides: the equipped vehicle, its stats, the HR model)
    //           -> START (the tuning popup: engine +10 to Engine1, braking HIGH) -> the game's PlayerVehicleTuning and the save -> TOGGLE (the
    //           quick view) -> MISC (free look) -> back -> TEST DRIVE (the UI_Compound world booted from the save: the vehicle spawned with the
    //           saved tuning, driving) -> pause (back at the Garage Page) -> an arcade Free Ride (FR_Alpine: the save's tuning applied after
    //           the attach; the pause page's VEHICLE TUNING on the running vehicle). PNGs to $GARAGE_SHOTS. The user's PlayerPrefs keys
    //           are checked unchanged.
    // Run: Unity -batchmode -nographics -quit -projectPath <bench> -executeMethod Reflex.Port.Editor.GarageTests.Verify (Play: no -quit, graphics)
    public static class GarageTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[GarageTests] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string shots;
        static string Shots() { var s = Environment.GetEnvironmentVariable("GARAGE_SHOTS"); if (string.IsNullOrEmpty(s)) s = Path.Combine(Application.temporaryCachePath, "garage-shots"); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, string> Prefs() => UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) + "|" + PlayerPrefs.GetFloat(k, 0) : "<none>");

        // ---------------------------------------------------------------------------------------------------- verify (edit mode)
        public static void Verify()
        {
            report.Clear(); bad = 0; shots = Shots();
            int warnings = 0; var warned = new List<string>();
            void OnLog(string m, string st, LogType t) { if (t == LogType.Warning && (m.Contains("no material") || m.Contains("Reflex bike") || m.Contains("HR rider"))) { warnings++; if (warned.Count < 20) warned.Add(m); } }
            Application.logMessageReceived += OnLog;
            try
            {
                var S = FeTables.Shared; var t = new ReflexGameTables(new FeTableSource(S));
                // 1. HR twins of every VehicleModel with its default parts (VehicleModelParts Default) as the garage's showroom shows it
                int models = 0, twins = 0, built = 0, partsNormal = 0, partsHr = 0;
                foreach (var m in S.Rows("VehicleModel"))
                {
                    int id = FeTables.I(m, "VehicleModelId"); if (FeTables.I(m, "StoreAssetId") < 0) continue;
                    var parts = S.Rows("VehicleModelParts").Where(r => FeTables.I(r, "VehicleModelId") == id && FeTables.I(r, "Default", 0) != 0).Select(r => FeTables.I(r, "VehiclePartId")).ToList();
                    BikeSpec bs = null; try { bs = ReflexVehicleSpecs.Bike(t, id, parts); } catch (Exception e) { Log("model " + id + ": " + e.Message); }
                    if (bs == null) continue;
                    models++;
                    var hr = ReflexBikeModel.HighRes(bs);
                    if (hr != bs && hr.chassisDb.StartsWith("HR_")) twins++;
                    try
                    {
                        int w0 = warnings;
                        var a = ReflexBikeModel.Build(bs); var b = ReflexBikeModel.Build(hr);
                        partsNormal += a.parts.Count; partsHr += b.parts.Count; built++;
                        if (models <= 3 || warnings != w0) Log($"model {id} {bs.name}: {bs.chassisDb}/{bs.chassisPkg}/{bs.chassisModel} -> {hr.chassisDb}/{hr.chassisPkg}/{hr.chassisModel}, parts {a.parts.Count} -> {b.parts.Count}, kit {hr.kitDb}/{hr.kitPkg}" + (warnings != w0 ? $" ({warnings - w0} warnings)" : ""));
                        foreach (var x in new[] { a, b }) { UnityEngine.Object.DestroyImmediate(x.chassis); if (x.frontWheel) UnityEngine.Object.DestroyImmediate(x.frontWheel.gameObject); if (x.rearWheel) UnityEngine.Object.DestroyImmediate(x.rearWheel.gameObject); }
                    }
                    catch (Exception e) { Check(false, "HR build of model " + id + ": " + e.Message); }
                    if (built % 10 == 0) ReflexBikeModel.ReleaseTextures();
                }
                ReflexBikeModel.ReleaseTextures();
                Check(models > 60 && twins == models, $"every VehicleModel's default vehicle has its HR_ twin chassis: {twins} / {models}");
                Check(built == models, $"HR twins built: {built} / {models}, parts {partsNormal} normal / {partsHr} HR");
                // 2. HR riders: the pros' gear sets
                int riders = 0, hrRiders = 0;
                foreach (var set in S.Rows("RiderGearSet").Select(r => FeTables.I(r, "RiderGearSetId")).Distinct().Take(8))
                {
                    var gear = S.Rows("RiderGearSetGear").Where(r => FeTables.I(r, "RiderGearSetId") == set).Select(r => FeTables.I(r, "RiderGearId")).ToList();
                    RiderSpec rs = null; try { rs = ReflexVehicleSpecs.Rider(t, gear); } catch (Exception) { }
                    if (rs == null) continue;
                    riders++;
                    var hr = FeGarage.HighResRider(rs);
                    if (hr == null) { Log("gear set " + set + ": no HR twin"); continue; }
                    try { var r = ReflexRiderModel.Build(hr); hrRiders++; if (riders <= 2) Log($"gear set {set}: {string.Join(",", hr.parts.Select(p => p.modelDb + "/" + p.modelPkg + "/" + p.asset))} skin {hr.skinDb}/{hr.skinPkg}"); UnityEngine.Object.DestroyImmediate(r.root); if (r.proxyRoot) UnityEngine.Object.DestroyImmediate(r.proxyRoot.gameObject); }
                    catch (Exception e) { Log("gear set " + set + " HR rider: " + e.Message); }
                }
                ReflexBikeModel.ReleaseTextures();
                Check(riders > 0 && hrRiders == riders, $"HR riders built: {hrRiders} / {riders}");
                Check(warnings == 0, "no unresolved bindings in the HR builds: " + warnings + (warned.Count > 0 ? " (" + string.Join(" | ", warned.Take(6)) + ")" : ""));
                // 3. the tuning EndLua is the Lua's own writes
                var lua = ReflexGarage.EndLua(new ReflexGarage.Tuning { engine1 = 20, engine2 = 0, suspension1 = 5, suspension2 = 15, tire1 = 0, tire2 = 0, braking = 100, steering = 0 });
                Check(lua.Contains("r:setCell_int( \"Engine1\", 20 )") && lua.Contains("r:setCell_int( \"Braking\", 100 )") && lua.EndsWith("App.ApplyVehicleTuning( d )"), "EndLua: " + lua.Substring(0, 120) + "...");
            }
            finally { Application.logMessageReceived -= OnLog; }
            Log("garage verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "verify-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        /// The garage frame as the port draws it (FeGarage: the HR models, ApplyFrontEndGlobals, the post chain with UI_Compound's HDRSettings)
        /// with the GarageHUB camera of an MX Lite, against the original's own frame of the same view (garage.md 4: over the models' pixels
        /// the tonemapped luminance 39.2 / 255, the raw HDR 41.3): the mean colour of the drawn pixels with the post chain on and off. PNGs.
        public static void Look()
        {
            report.Clear(); bad = 0; shots = Shots();
            var S = FeTables.Shared; var t = new ReflexGameTables(new FeTableSource(S));
            int pv = FeTables.I(S.Rows("PlayerRider").FirstOrDefault(), "LastEquippedVehicle", 0);
            int model = FeTables.I(S.Rows("PlayerVehicle").FirstOrDefault(r => FeTables.I(r, "PlayerVehicleId") == pv), "VehicleModelId");
            var parts = S.Rows("PlayerVehicleParts").Where(r => FeTables.I(r, "PlayerVehicleId") == pv).Select(r => FeTables.I(r, "VehiclePartId")).ToList();
            var gear = S.Rows("PlayerRiderGear").Select(r => FeTables.I(r, "RiderGearId")).ToList();
            var bike = ReflexVehicleSpecs.Bike(t, model, parts); var rider = ReflexVehicleSpecs.Rider(t, gear);
            foreach (bool post in new[] { true, false })
            {
                FeGarage.OriginalPost = post;
                var g = new FeGarage(null);
                g.ShowSpecs(bike, rider, true);
                g.SetCamera(new Vector3(-3.59f, 2.84f, 6.53f), new Vector3(0.96f, 2.40f, -0.22f), new Vector3(-0.59f, 0.36f, -9.47f));   // GarageHUB_Cameras, MX Lite
                for (int i = 0; i < 40; i++) { g.Update(0.05f); g.Render(); }   // the camera's 0.5 s move, the eye adaptation
                var prev = RenderTexture.active; RenderTexture.active = g.Texture;
                var tex = new Texture2D(1280, 720, TextureFormat.RGBA32, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
                var px = tex.GetPixels32(); double r = 0, gg = 0, b = 0, lum = 0; int n = 0;
                foreach (var p in px) { if (p.a == 0) continue; n++; r += p.r; gg += p.g; b += p.b; lum += 0.299 * p.r + 0.587 * p.g + 0.114 * p.b; }
                File.WriteAllBytes(Path.Combine(shots, "40_garage_frame_" + (post ? "post" : "raw") + ".png"), tex.EncodeToPNG());
                Log($"garage frame, post chain {(post ? "on (key " + FeGarage.KeyValue + ")" : "off")}: {n} drawn pixels, mean RGB {r / Math.Max(1, n):F1} {gg / Math.Max(1, n):F1} {b / Math.Max(1, n):F1}, luminance {lum / Math.Max(1, n):F1} (the original's frame: 99306 px, RGB 49.9 34.8 34.0, luminance 39.2)");
                UnityEngine.Object.DestroyImmediate(tex); g.Destroy();
            }
            FeGarage.OriginalPost = true;
            File.WriteAllText(Path.Combine(shots, "look-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(0);
        }

        /// The test drive alone (the save's equipped vehicle in UI_Compound), straight into the race scene: its state over time (diagnostics).
        public static void TestDriveOnly()
        {
            report.Clear(); bad = 0; shots = Shots(); ReflexProfile.Disabled = false; prefs = Prefs();
            UnityEditor.SceneManagement.EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            var run = ReflexGarage.TestDrive.Begin();
            ReflexEventSelect.Pending = run.ev.key; ReflexEventSelect.PendingAi = 0;
            stage = 0; stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += StepTestDrive;
            EditorApplication.EnterPlaymode();
        }
        static void StepTestDrive()
        {
            if (!EditorApplication.isPlaying) { if (stage >= 100) End(); return; }
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (Age < 3 || !rt) return;
            stageAt = EditorApplication.timeSinceStartup; stage++;
            var cc = rt.chaseCamera;
            Log($"t {EditorApplication.timeSinceStartup - started:F0}s running {rt.Running} paused {rt.paused} sim {rt.Simulation != null} race {rt.Race.state} chassis {(rt.chassis ? rt.chassis.position.ToString() : "-")} cam {(cc ? cc.transform.position.ToString() : "-")} status {(rt.statusLabel ? rt.statusLabel.text.Replace("\n", " | ") : "-")}");
            if (stage == 4) { rt.testHeldKeys = new[] { 0x11 }; Shot("50_td_only"); }
            if (stage == 8) { Shot("51_td_only_later"); stage = 100; EditorApplication.ExitPlaymode(); }
        }

        /// The motocard setup's riders (gear sets 1, 2, 15 with each skin tone) built in the garage's HR, one by one (a probe for slow builds).
        /// Diagnostic: VehicleTuningPopup.lyr as the in-race tree describes it (material, texture, colours per node).
        public static void PopupProbe()
        {
            var data = OriginalUiAssets.Data; var doc = data.DescribeLayer("Shared", "VehicleTuningPopup.lyr");
            void W(Dictionary<string, object> e, string ind)
            {
                var st = OriginalUi.Obj(e, "style");
                string S(string k) => st != null && st.TryGetValue(k, out var v) && v != null ? (v is List<object> l ? "[" + string.Join(",", l) + "]" : v.ToString()) : "-";
                Debug.Log("[PopupProbe] " + ind + OriginalUi.S(e, "name") + " (" + OriginalUi.S(e, "class") + ") mat " + S("material") + " tex " + S("texture") + " color " + S("color") + " focusable " + S("FocusableColor") + " focused " + S("FocusedColor") + " enabled " + S("EnabledColor") + " layout " + string.Join(",", OriginalUi.Arr(OriginalUi.Obj(e, "layout"), "size") ?? new List<object>()));
                var kids = OriginalUi.Arr(e, "children"); if (kids != null) foreach (var k in kids) if (k is Dictionary<string, object> kd) W(kd, ind + "  ");
            }
            W(doc, "");
            if (Application.isBatchMode) EditorApplication.Exit(0);
        }

        public static void RiderProbe()
        {
            var S = FeTables.Shared; var t = new ReflexGameTables(new FeTableSource(S));
            int RowType(int g) => FeTables.I(S.Rows("RiderGear").FirstOrDefault(r => FeTables.I(r, "RiderGearId") == g), "RiderGearTypeId");
            foreach (int set in new[] { 1, 2, 15 })
                foreach (int skin in new[] { 321, 512, 513, 514 })
                {
                    var gear = S.Rows("RiderGearSetGear").Where(r => FeTables.I(r, "RiderGearSetId") == set).Select(r => FeTables.I(r, "RiderGearId")).Where(g => RowType(g) != 10).Append(skin).ToList();
                    var rs = ReflexVehicleSpecs.Rider(t, gear); var hr = FeGarage.HighResRider(rs);
                    Debug.Log($"[GarageTests] probe set {set} skin {skin}: gear {string.Join(",", gear)} -> HR {(hr == null ? "none" : hr.skinDb + "/" + hr.skinPkg + " " + string.Join(",", hr.parts.Select(p => p.modelDb + "/" + p.modelPkg)))}");
                    var w = System.Diagnostics.Stopwatch.StartNew();
                    var r = ReflexRiderModel.Build(hr ?? rs);
                    Debug.Log($"[GarageTests] probe built in {w.ElapsedMilliseconds} ms");
                    UnityEngine.Object.DestroyImmediate(r.root); if (r.proxyRoot) UnityEngine.Object.DestroyImmediate(r.proxyRoot.gameObject);
                }
            if (Application.isBatchMode) EditorApplication.Exit(0);
        }

        // ---------------------------------------------------------------------------------------------------- play
        static int stage; static double stageAt, started; static Dictionary<string, string> prefs;
        static ReflexGarage.Tuning written; static string saveBefore; static int equipped = -1;
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static bool Idle(FrontEndManager fm) => fm && fm.assets != null && !fm.Busy && !ReflexProfile.Busy;

        public static void Play()
        {
            report.Clear(); bad = 0; shots = Shots(); ReflexProfile.Disabled = false;
            prefs = Prefs();
            Log("save " + ReflexProfile.SaveFile + " (" + (ReflexProfile.HasSave ? new FileInfo(ReflexProfile.SaveFile).Length + " bytes" : "none") + ")");
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (fm) fm.rememberChoice = false;
            stage = 0; stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Shot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var main = Camera.main;
            if (main) { c.CopyFrom(main); c.targetTexture = target; c.enabled = false; c.cullingMask = main.cullingMask | 1; }
            c.cullingMask &= ~(1 << FeGarage.Layer);
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

        static bool At(FrontEndManager fm, string page, double after = 1.5, double timeout = 90)
        {
            if (Idle(fm) && fm.CurrentPage == page && Age > after) return true;
            if (Age > timeout) Fail("page " + page + " not reached (at " + fm?.CurrentPage + " / " + fm?.TopPopup + ")");
            return false;
        }
        static string Bars(FeVehiclePerformance p) => p == null ? "-" : string.Join(" ", p.Shown.Select(kv => kv.Key + " " + string.Join("/", kv.Value)));

        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > 2400) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                var mr = fm && fm.assets != null && fm.CurrentPage == "Garage My Rides Page" ? fm.Layer("GarageMyRides.lyr").script as GarageMyRidesScript : null;
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                switch (stage)
                {
                    case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                    case 1:
                        if (Idle(fm) && fm.CurrentPage == "Main Menu Page" && Age > 1.5 && ReflexProfile.Current != null) { Log("profile: MotocardSetup " + ReflexProfile.Current.MotocardSetup); fm.DispatchKey("Accept"); Next(); }
                        else if (Idle(fm) && fm.CurrentPage != "Main Menu Page" && fm.CurrentPage != "Title Page" && fm.CurrentPage != "Load Game Page" && Age > 5) { fm.DispatchKey("Accept"); stageAt = EditorApplication.timeSinceStartup; }
                        else if (Age > 120) Fail("no main menu (at " + fm?.CurrentPage + ")");
                        break;
                    case 2:
                        if (Idle(fm) && fm.CurrentPage == "Career Welcome Page" && Age > 1.5) { fm.DispatchKey("Accept"); stageAt = EditorApplication.timeSinceStartup; }
                        else if (At(fm, "Race and Ride Page", 2)) { fm.DispatchKey("Toggle"); Next(); }   // GARAGE
                        break;
                    case 3: if (At(fm, "Garage Page", 4)) { Shot("10_garage_hub"); fm.Navigate(-1, 0); Next(); } break;
                    case 4: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;   // VEHICLE
                    case 5:
                        if (Idle(fm) && fm.CurrentPage == "Garage Customize Rider Page") { fm.SendUIEvent("TravelBack"); stage = 3; stageAt = EditorApplication.timeSinceStartup - 3; break; }
                        if (At(fm, "Garage My Rides Page", 4) && mr != null)
                        {
                            Shot("11_my_rides");
                            equipped = mr.EquippedModel;
                            Log($"my rides: equipped model {mr.EquippedModel}, focused {mr.FocusedModel}, tuning {mr.AllowTuning}, free look {mr.AllowFreeLook}, garage {mr.Garage?.Shown}, chassis {mr.Garage?.BuiltChassis}, bars {Bars(mr.Performance)}");
                            Check(mr.OnEquipped && mr.AllowTuning, "My Rides starts on the equipped vehicle with tuning allowed (m_AllowTuning)");
                            Check(mr.Garage?.BuiltChassis?.StartsWith("HR_") == true, "the garage draws the HR_ twin: " + mr.Garage?.BuiltChassis);
                            var p = ReflexProfile.Current; int pvid = p.T("PlayerRider").Int("LastEquippedVehicle", 0, 0);
                            var tt = p.T("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", pvid);
                            var m = FeTables.Shared.Rows("VehicleModel").First(r => FeTables.I(r, "VehicleModelId") == mr.EquippedModel);
                            var e = mr.Performance?.Shown.GetValueOrDefault("engine");
                            Check(e != null && e[0] == FeTables.I(m, "DefaultEngine1") && e[1] == FeTables.I(m, "DefaultEngine2") && e[2] == tt.Int("Engine1", row, 0) && e[3] == tt.Int("Engine2", row, 0),
                                  "the performance display: VehicleModel DefaultEngine1/2 + the PlayerVehicleTuning row (" + (e == null ? "-" : string.Join("/", e)) + ")");
                            // test setup (the bench's save only): bonus points to move (a fresh profile's row is all 0: nothing moves, as in the game)
                            if (row >= 0 && tt.Int("Engine2", row, 0) < 10) { ReflexProfile.Run($"set_int PlayerVehicleTuning {row} Engine2 20", $"set_int PlayerVehicleTuning {row} Suspension1 10"); Log("test setup: PlayerVehicleTuning row " + row + " Engine2 20, Suspension1 10"); }
                            Next();
                        }
                        break;
                    case 50: break;
                    case 6: if (Idle(fm) && Age > 1) { fm.DispatchKey("Start"); Next(); } break;   // OnEnableVehicleTuning
                    case 7:
                        if (Idle(fm) && fm.TopPopup == "VehicleTuningPopup" && Age > 1.5)
                        {
                            var tp = fm.Layer("VehicleTuningPopup.lyr").script as GarageVehicleTuningScript;
                            Log("tuning popup values: " + string.Join(" ", tp.values.Select(kv => kv.Key + " " + string.Join("/", kv.Value))) + " -> " + tp.Current);
                            Shot("12_tuning_popup");
                            var before = tp.Current;
                            fm.Navigate(1, 0); fm.Navigate(1, 0);                  // engine: bonus 5 + 5 from Engine2 to Engine1
                            fm.Navigate(0, 1); fm.Navigate(0, 1); fm.Navigate(0, 1); // braking
                            fm.Navigate(1, 0);                                     // braking HIGH (the list wraps: MID -> HIGH)
                            written = tp.Current;
                            Log("after the keys: " + written + ", modified " + tp.Modified);
                            bool engineMoved = written.engine1 + written.engine2 == before.engine1 + before.engine2 && written.engine1 == before.engine1 + 10;
                            Check(tp.Modified && engineMoved && written.braking == (before.braking + 50) % 150, "Left / Right move 5 bonus points between Engine1 and Engine2, the braking list (Lua: OnKeyDownRight_Attribute, BrakingList)");
                            Shot("13_tuning_changed");
                            saveBefore = ReflexProfile.SaveStamp();
                            fm.DispatchKey("Accept"); Next();   // TravelBack -> End: the table writes, ApplyVehicleTuning, RequestSaveGame
                        }
                        else if (Age > 30) Fail("no tuning popup (top " + fm?.TopPopup + ")");
                        break;
                    case 8:
                        if (Idle(fm) && fm.TopPopup == null && Age > 3)
                        {
                            var p = ReflexProfile.Current; int pvid = p.T("PlayerRider").Int("LastEquippedVehicle", 0, 0);
                            var tt = p.T("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", pvid);
                            Check(tt.Int("Engine1", row, -1) == written.engine1 && tt.Int("Engine2", row, -1) == written.engine2 && tt.Int("Braking", row, -1) == written.braking,
                                  $"the game's PlayerVehicleTuning row of PlayerVehicleId {pvid}: Engine1 {tt.Int("Engine1", row, -1)} Engine2 {tt.Int("Engine2", row, -1)} Braking {tt.Int("Braking", row, -1)} Steering {tt.Int("Steering", row, -1)}");
                            Check(ReflexProfile.SaveStamp() != saveBefore, "the game saved it (save " + saveBefore + " -> " + ReflexProfile.SaveStamp() + ")");
                            Log("bars after tuning: " + Bars(mr?.Performance));
                            Shot("14_my_rides_tuned");
                            fm.DispatchKey("Toggle"); Next();   // the quick view
                        }
                        else if (Age > 40) Fail("the tuning was not written");
                        break;
                    case 9:
                        if (Idle(fm) && fm.TopPopup == "VehiclesQuickViewPopup" && Age > 1.5)
                        {
                            var qv = fm.Layer("VehiclesQuickViewPopup.lyr").script as VehiclesQuickViewPopupScript;
                            var pv = ReflexProfile.Current.T("PlayerVehicle");
                            Log("quick view: " + string.Join(", ", qv.items.Select(i => i.vehicleId + ":" + i.typeLoc + ":" + i.modelId)) + "; bars " + Bars(qv.Performance));
                            Check(qv.ShowPlayerVehicles == ReflexProfile.Current.MotocardSetup && qv.items.Count == (qv.ShowPlayerVehicles ? pv.rows : qv.items.Count), $"the player's vehicles ({qv.items.Count} of {pv.rows} PlayerVehicle rows)");
                            Shot("15_quick_view");
                            if (qv.items.Count > 1) fm.Navigate(0, 1);
                            fm.DispatchKey("Accept"); Next();
                        }
                        else if (Age > 30) Fail("no quick view (top " + fm?.TopPopup + ")");
                        break;
                    case 10:
                        if (Idle(fm) && fm.TopPopup == null && Age > 3 && mr != null)
                        {
                            Log($"after the quick view: equipped model {mr.EquippedModel} (was {equipped}), focused {mr.FocusedModel}");
                            Check(mr.FocusedModel == mr.EquippedModel, "OnSelectPlayerVehicleId: the list on the chosen vehicle");
                            Shot("16_my_rides_after_quick_view");
                            fm.DispatchKey("Misc"); Next();   // free look
                        }
                        break;
                    case 11:
                        if (Age > 2 && mr != null)
                        {
                            Check(mr.FreeLook, "MISC: free look (FadeOut, the camera around the vehicle)");
                            Shot("17_free_look");
                            fm.DispatchKey("Back"); Next();
                        }
                        break;
                    case 12:
                        if (Age > 2 && mr != null)
                        {
                            Check(!mr.FreeLook, "BACK leaves free look");
                            if (equipped >= 0 && mr.EquippedModel != equipped) { mr.EquipVehicle(equipped); Log("re-equipped model " + equipped); }
                            Next();
                        }
                        break;
                    case 13: if (Idle(fm) && Age > 2) { fm.SendUIEvent("TravelBack"); Next(); } break;
                    case 14: if (At(fm, "Garage Page", 3)) { Log("TEST DRIVE"); saveBefore = ReflexProfile.SaveStamp(); fm.DispatchKey("Start"); Next(); } break;
                    case 15:
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 4)
                        {
                            var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                            Log("test drive: event " + sel?.Current + ", level " + rt.level + ", status " + (rt.statusLabel ? rt.statusLabel.text.Replace("\n", " | ") : "-"));
                            Check(ReflexGarage.TestDrive.Active && sel != null && sel.Current == ReflexGarage.TestDrive.Current.ev.key, "the race scene runs the test drive");
                            string st = ReflexGarage.State(ReflexGarage.World);
                            Log("test drive vehicle tuning: " + st);
                            Check(st != null && st.Contains("\"values\":[") && Values(st, 6) == written.braking, "the vehicle spawned with the saved tuning (Braking " + written.braking + ")");
                            var cc = rt.chaseCamera; var mc = Camera.main;
                            Log($"cameras: chase {(cc ? cc.transform.position + " fwd " + cc.transform.forward + " fov " + cc.fieldOfView + " mode " + rt.cameraMode : "-")}, main {(mc ? mc.name + " " + mc.transform.position : "-")}, chassis {(rt.chassis ? rt.chassis.position.ToString() : "-")}");
                            Shot("20_test_drive"); rt.testHeldKeys = new[] { 0x11 }; Next();   // W: drive
                        }
                        else if (Age > 420) Fail("test drive not running after 420 s");
                        break;
                    case 16:
                        if (Age > 8)
                        {
                            Shot("21_test_drive_later");
                            Log($"later: running {rt.Running} paused {rt.paused} sim {rt.Simulation != null} race {rt.Race.state} chassis {(rt.chassis ? rt.chassis.position.ToString() : "-")} cam {(rt.chaseCamera ? rt.chaseCamera.transform.position.ToString() : "-")} timeScale {Time.timeScale} status {(rt.statusLabel ? rt.statusLabel.text.Replace("\n", " | ") : "-")}");
                            bool live = OriginalPauseMenu.GameOwnsPause(rt);
                            Log("the pause request: " + (live ? "the game's own (Esc to the world, its UI live)" : "the port's pause page") + ", test drive flag " + ReflexGarage.TestDrive.LastFlag);
                            if (live) rt.testHeldKeys = new[] { 0x01 }; else { rt.testHeldKeys = new int[0]; rt.pauseMenu?.Open(); }
                            Next();
                        }
                        break;
                    case 17:
                        if (Idle(fm) && fm.CurrentPage == "Garage Page" && Age > 3) { Check(!ReflexGarage.TestDrive.Active, "the pause request returned to the Garage Page"); Shot("22_back_in_garage"); Next(); }
                        else if (Age > 60) Fail("not back in the garage (at " + fm?.CurrentPage + ")");
                        break;
                    case 18:   // an arcade Free Ride with the profile's vehicle: the save's tuning after the attach, the pause page's VEHICLE TUNING
                        ReflexEventSelect.Launch("FreeRide/FR_Alpine", 0, false); Next(); break;
                    case 19:
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 4)
                        {
                            string st = ReflexGarage.State(ReflexGarage.World);
                            Log("Free Ride vehicle tuning after the attach: " + st);
                            Check(st != null && Values(st, 6) == written.braking, "the attached save's PlayerVehicleTuning on the running vehicle (App.ApplyVehicleTuning)");
                            Check(OriginalVehicleTuningPopup.Offered(rt), "VEHICLE TUNING offered in Free Ride with the player's vehicle");
                            rt.pauseMenu?.Open(); Next();
                        }
                        else if (Age > 420) Fail("Free Ride not running after 420 s");
                        break;
                    case 20:
                        if (Age > 2)
                        {
                            Shot("30_free_ride_pause");
                            var pop = OriginalVehicleTuningPopup.Show(rt);
                            Check(pop != null && OriginalVehicleTuningPopup.IsOpen, "the tuning popup over the pause page");
                            if (pop == null) { Fail("no popup"); break; }
                            Log("in-race popup values: " + string.Join(" ", pop.values.Select(kv => kv.Key + " " + string.Join("/", kv.Value))) + ", braking " + pop.braking + " steering " + pop.steering);
                            pop.Navigate(0, 1); pop.Navigate(-1, 0); pop.Navigate(-1, 0);   // suspension: 10 points to Suspension2
                            pop.Navigate(0, 1); pop.Navigate(0, 1); pop.Navigate(-1, 0);    // braking one lower
                            Next();
                        }
                        break;
                    case 21:
                        if (Age > 1.5)
                        {
                            Shot("31_free_ride_tuning_popup");
                            var pop = OriginalVehicleTuningPopup.Current; var t = pop.Tuning; Log("in-race tuning: " + t);
                            pop.Close(); rt.pauseMenu?.Close(); Next();
                        }
                        break;
                    case 22:
                        if (Age > 3)
                        {
                            Log("world after the popup: " + OriginalVehicleTuningPopup.LastState);
                            var lw = OriginalVehicleTuningPopup.LastWritten;
                            Check(lw != null && OriginalVehicleTuningPopup.LastState != null && Values(OriginalVehicleTuningPopup.LastState, 6) == lw.Value.braking && Values(OriginalVehicleTuningPopup.LastState, 3) - Values(OriginalVehicleTuningPopup.LastState, 3, "defaults") == lw.Value.suspension2,
                                  "App.ApplyVehicleTuning in the running world: the spec's values are the popup's");
                            Shot("32_free_ride_after_tuning"); OriginalPauseMenu.QuitToFrontEnd(); Next();
                        }
                        break;
                    case 23: if (fm && fm.assets != null && Age > 4 && !fm.Busy) { Next(); } else if (Age > 60) Fail("front end not back"); break;
                    case 24: stage = 100; EditorApplication.ExitPlaymode(); break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        /// The i-th number of a JSON array field of the "tuning" answer.
        static int Values(string json, int i, string field = "values")
        {
            int a = json.IndexOf("\"" + field + "\":[", StringComparison.Ordinal); if (a < 0) return int.MinValue;
            a += field.Length + 4; int b = json.IndexOf(']', a);
            var parts = json.Substring(a, b - a).Split(',');
            return i < parts.Length && int.TryParse(parts[i], out int v) ? v : int.MinValue;
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            var after = Prefs();
            Check(after.All(kv => kv.Value == prefs[kv.Key]), "the user's PlayerPrefs keys unchanged: " + string.Join(", ", after.Select(kv => kv.Key + "=" + kv.Value)));
            Log("garage play: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "play-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
