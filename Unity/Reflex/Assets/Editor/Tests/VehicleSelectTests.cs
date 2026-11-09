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
    // The arcade Vehicle Select page (FrontEndVehicleSelect.cs) and its hand-over to the race (ReflexWorldFiles.Garage ->
    // Boot::start_event's EquipVehicle / EquipRider, _re/notes/vehicle-select.md).
    //   Bench: edit mode. Title -> main menu -> ARCADE -> NATIONALS -> Vehicle Select: the lists against the tables and the Lua rules
    //          (riders, types, vehicles, performance bars, brand), the 3D garage, then Ricky Carmichael's MX -> Locations ->
    //          accept (hand-over intercepted): the Garage handed over. PNGs to $VEHICLESELECT_SHOTS.
    //   Race:  play mode. The same pick at NAT_Concord with AI None, the real hand-over: the race scene's world is built with the
    //          choice, and the local slot's rider / vehicle (the running game's Rider List / Vehicle List rows) and the bike and
    //          gear Unity draws are checked; screenshots of the race. rememberChoice off; the user's PlayerPrefs keys are restored.
    // Run: Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Editor.VehicleSelectTests.Bench (Race: no -quit)
    public static class VehicleSelectTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[VehicleSelect] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string shots;
        static string Shots() { var s = Environment.GetEnvironmentVariable("VEHICLESELECT_SHOTS"); if (string.IsNullOrEmpty(s)) s = Path.Combine(Application.temporaryCachePath, "vehicleselect-shots"); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));

        // ---------------------------------------------------------------------------------------------------- bench
        static FrontEndManager fe; static Camera cam; static RenderTexture rt;
        static void Run(float seconds) { for (float t = 0; t < seconds; t += 1f / 30f) fe.Step(1f / 30f); }
        static VehicleSelectScript Vs() => fe.Layer("VehicleSelect.lyr").script as VehicleSelectScript;
        static void Shot(string name)
        {
            Vs()?.garage?.Render();
            Canvas.ForceUpdateCanvases(); cam.Render();
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); tex.Apply();
            RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(tex);
            Log($"shot {name}: page '{fe.CurrentPage}'");
        }
        static string Texts(FeNode n) { var o = new List<string>(); void W(FeNode x) { if (!x.visible) return; if (x.text && !string.IsNullOrEmpty(x.text.Text)) o.Add(x.name + "=" + x.text.Text); foreach (var k in x.kids) W(k); } W(n); return string.Join(" | ", o); }

        public static void Bench()
        {
            report.Clear(); bad = 0; shots = Shots();
            ReflexProfile.Disabled = true;   // the guest path (no profile session): title -> profile reminder
            var before = SavePrefs();
            ReflexEvents.Event launched = null; int launchedAi = -1;
            FrontEndManager.LaunchOverride = (ev, ai) => { launched = ev; launchedAi = ai; };
            try
            {
                rt = new RenderTexture(1280, 720, 24);
                cam = new GameObject("bench camera").AddComponent<Camera>();
                cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black; cam.targetTexture = rt; cam.orthographic = true; cam.enabled = false; cam.cullingMask = ~(1 << FeGarage.Layer);
                fe = new GameObject("Reflex front end").AddComponent<FrontEndManager>();
                fe.uiShader = Shader.Find("Reflex/FrontEnd UI");
                fe.Boot();
                var c = fe.canvas; c.renderMode = RenderMode.ScreenSpaceCamera; c.worldCamera = cam; c.planeDistance = 1;
                Run(1.0f);
                fe.DispatchKey("Accept"); Run(0.5f); fe.DispatchKey("Accept"); Run(1.2f);           // title -> profile reminder -> main menu
                Check(fe.CurrentPage == "Main Menu Page", "main menu");
                fe.Navigate(1, 0); Run(0.3f); fe.Navigate(-1, 0); fe.Navigate(-1, 0); Run(0.4f);       // ARCADE
                fe.DispatchKey("Accept"); Run(1.2f);
                Check(fe.CurrentPage == "Event Select Page", "ARCADE -> event select");
                fe.Navigate(1, 0); Run(0.5f);                                                       // NATIONALS
                fe.DispatchKey("Accept"); Run(1.2f);
                Check(fe.CurrentPage == "Vehicle Select Page", "NATIONALS -> vehicle select: " + fe.CurrentPage);
                var vs = Vs();
                Check(vs != null, "VehicleSelect script on VehicleSelect.lyr");
                Log("riders: " + string.Join(", ", vs.riders.Select(r => r.id == -1 ? "MY RIDES" : r.display + " #" + r.number + " (" + r.id + ", gear set " + r.gearSet + ")")));
                Check(string.Join(",", vs.riders.Select(r => r.id)) == "-1,4,3,2,1,5", "rider list: MY RIDES, then PlayerSelectable AIRider by DisplayName (James Stewart, Jeremy McGrath, Josh Grant, Ricky Carmichael, Ryan Villopoto)");
                Check(vs.RiderIndex == 0 && vs.types.Count == 1 && vs.types[0].id == 2 && vs.vehicles.Count == 1 && vs.vehicles[0].id == 0, $"default: MY RIDES, MX Lite, PlayerVehicle 0 (types {string.Join(",", vs.types.Select(x => x.id))}, vehicles {string.Join(",", vs.vehicles.Select(x => x.id))})");
                Log("page: " + Texts(fe.Layer("VehicleSelect.lyr").root));
                Log("garage: " + vs.garage?.Shown);
                Shot("01_vehicle_select_my_rides");
                // Ricky Carmichael (4th): right x4
                for (int i = 0; i < 4; i++) { fe.Navigate(1, 0); Run(0.25f); }
                Run(0.3f);
                Check(vs.riders[vs.RiderIndex].id == 1, "rider list -> Ricky Carmichael (AIRiderId 1)");
                Check(string.Join(",", vs.types.Select(x => x.id)) == "2,3,4" && !vs.types[2].locked && !vs.types[1].locked, "his types: MX Lite, MX, ATV (EventTypeVehicleClasses 10, Single classes 1/2/3, sorted 2,3,4); ATV offered");
                Log("garage: " + vs.garage?.Shown);
                Shot("02_vehicle_select_carmichael");
                fe.DispatchKey("Accept"); Run(0.4f);
                Check(vs.TypeListActive, "Accept -> vehicle type list");
                Shot("03_vehicle_types");
                fe.Navigate(0, 1); Run(0.3f); fe.Navigate(0, 1); Run(0.3f);                         // ATV (ReflexAtv)
                fe.DispatchKey("Accept"); Run(0.4f);
                Check(vs.VehicleListActive && vs.types[vs.TypeIndex].id == 4 && vs.vehicles.Count == 1 && vs.vehicles[0].id == 44 && vs.vehicles[0].modelId == 21 && vs.garage?.Shown?.StartsWith("Diz") == true,
                      $"ATV accepted -> his AIVehicle 44 (VehicleModel 21, ATVs/Diz) in the garage: {string.Join(",", vs.vehicles.Select(v => v.id + "/" + v.modelId))}, {vs.garage?.Shown}");
                Shot("04_vehicle_list_atv");
                fe.DispatchKey("Back"); Run(0.4f);
                Check(vs.TypeListActive && vs.types[vs.TypeIndex].id == 4, "Back -> the vehicle type list, ATV still focused");
                fe.Navigate(0, -1); Run(0.3f);                                                      // MX
                Check(vs.types[vs.TypeIndex].id == 3 && vs.vehicles.Count == 1 && vs.vehicles[0].id == 43 && vs.vehicles[0].modelId == 32, $"MX -> his AIVehicle 43 (VehicleModel 32): {string.Join(",", vs.vehicles.Select(v => v.id + "/" + v.modelId))}");
                fe.DispatchKey("Accept"); Run(0.4f);
                Check(vs.VehicleListActive, "Accept -> vehicle list");
                var perf = fe.Layer("VehicleSelect.lyr").Find("VehiclePerformanceDisplay.component");
                string Bars(string a) { var n = perf.kids.First(k => k.name.Equals(a + "_Attribute", StringComparison.OrdinalIgnoreCase)); return string.Join(" ", new[] { "Attribute1", "Attribute2" }.Select(x => n.Kid(x)).Select(x => $"base {x.Kid("Base").size.x}@{x.Kid("Base").pos.x} bonus {x.Kid("Bonus").size.x}@{x.Kid("Bonus").pos.x}")); }
                Log("performance: engine " + Bars("engine") + "; suspension " + Bars("suspension") + "; tires " + Bars("tires"));
                // VehicleModel 32: DefaultEngine 15/25, Suspension 35/15, Tire 25/25 -> floor(v/100*160/8)*8 = 24/40, 56/24, 40/40
                var e1 = perf.kids.First(k => k.name.Equals("Engine_Attribute", StringComparison.OrdinalIgnoreCase)).Kid("Attribute1").Kid("Base");
                var s1 = perf.kids.First(k => k.name.Equals("Suspension_Attribute", StringComparison.OrdinalIgnoreCase)).Kid("Attribute1").Kid("Base");
                Check(Mathf.Approximately(e1.size.x, 24) && Mathf.Approximately(s1.size.x, 56), $"performance bars of VehicleModel 32 (engine1 {e1.size.x} = 24, suspension1 {s1.size.x} = 56)");
                var brand = fe.Layer("VehicleSelect.lyr").Find("BrandImageIcon");
                Check(brand.visible && brand.image && brand.image.texture, "brand image (StoreProducts.BrandId -> Brand.BrandIconId): " + (brand.image && brand.image.texture ? brand.image.texture.name : "none"));
                Log("garage: " + vs.garage?.Shown);
                Shot("05_vehicle_list_mx");
                fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f);
                Check(fe.CurrentPage == "Locations Page", "Accept -> locations");
                var g = fe.Var<ReflexWorldFiles.Garage>("Garage");
                Check(g != null && g.riderOwner == 4 && g.riderId == 1 && g.riderGearSet == 3 && g.vehicleOwner == 4 && g.vehicleId == 43 && g.vehicleTypeId == 3, "VehicleSelect:End: EquipVehicle(43, AI) / EquipRider(1, gear set 3, AI): " + g);
                Check(fe.Var("Player1VehicleTypeId", -1) == 3, "Player1VehicleTypeId 3");
                Shot("06_locations_after_vehicle_select");
                fe.DispatchKey("Toggle"); Run(0.3f); fe.Navigate(0, 1); Run(0.2f); fe.Navigate(0, 1); Run(0.3f); Shot("07_locations_rules_vehicle_class");
                Check(fe.Var("VehicleClass", -1) == 2, "LocationSelect's vehicle class for an MX: 2 (STR_VEHICLECLASS_MX): " + fe.Var("VehicleClass", -1));
                fe.DispatchKey("Toggle"); Run(0.3f);
                fe.DispatchKey("Accept"); Run(0.6f);
                var lg = FrontEndManager.LaunchedGarage;
                string first = FeTables.Shared.Locations(10).FirstOrDefault()?.ev?.key;   // LocationSelect's first location (the DLC ones when installed)
                Check(launched != null && launched.key == first && lg != null && lg.vehicleClass == 2 && lg.vehicleId == 43, $"accept launches {launched?.key} AI {launchedAi} with {lg}");
                var wf = ReflexWorldFiles.For(launched.key, ReflexWorldFiles.FullGateOpponents, 95, 0, lg);
                Log("world name: " + wf.name);
                Check(wf.name.EndsWith("_vsr1g3a43_c2"), "world name suffix for the choice");
                Check(ReflexWorldFiles.For(launched.key, 0, 95, 0, new ReflexWorldFiles.Garage { vehicleClass = 3 }).name == ReflexWorldFiles.For(launched.key).name, "the default choice keeps the default world name");
                // back: Locations -> Vehicle Select (the rider kept, Lua), -> event select
                fe.SendUIEvent("TravelBack"); Run(1.2f);
                Check(fe.CurrentPage == "Vehicle Select Page" && vs.riders[vs.RiderIndex].id == 1, "Back from locations -> vehicle select, Carmichael still chosen");
                Shot("08_vehicle_select_again");
                fe.DispatchKey("Back"); Run(1.2f);
                Check(fe.CurrentPage == "Event Select Page", "Back -> event select");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally
            {
                FrontEndManager.LaunchOverride = null; FrontEndManager.LaunchedGarage = null;
                if (fe) UnityEngine.Object.DestroyImmediate(fe.gameObject);
                if (cam) UnityEngine.Object.DestroyImmediate(cam.gameObject);
            }
            Check(SavePrefs().All(kv => kv.Value.Equals(before[kv.Key])), "user PlayerPrefs untouched");
            Log("vehicle select bench: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "bench-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------------- race (play mode)
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static int stage; static double stageAt;
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;

        public static void Race()
        {
            report.Clear(); bad = 0; shots = Shots();
            ReflexProfile.Disabled = true;   // the guest path (no profile session): title -> profile reminder
            saved = SavePrefs();
            Log("prefs before: " + string.Join(", ", saved.Select(kv => kv.Key + "=" + (kv.Value.has ? kv.Value.s + "/" + kv.Value.i : "<none>"))));
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); fm.rememberChoice = false;
            stage = 0; stageAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void PlayShot(string name)
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

        static void Step()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                VehicleSelectScript vs = fm && fm.assets != null ? fm.Layer("VehicleSelect.lyr").script as VehicleSelectScript : null;
                switch (stage)
                {
                    case 0: if (fm && fm.assets != null && Age > 2) { fm.DispatchKey("Accept"); Next(); } else if (Age > 60) Fail("front end did not boot"); break;
                    case 1: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 2: if (Age > 2 && !fm.Busy) { fm.Navigate(-1, 0); fm.DispatchKey("Accept"); Next(); } break;                         // ARCADE
                    case 3: if (Age > 2 && !fm.Busy) { fm.Navigate(1, 0); Next(); } break;                                                      // NATIONALS
                    case 4: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 5: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Vehicle Select Page", "vehicle select (play mode)"); for (int i = 0; i < 4; i++) fm.Navigate(1, 0); Next(); } break;   // Ricky Carmichael
                    case 6: if (Age > 2) { PlayShot("30_play_vehicle_select_carmichael"); fm.DispatchKey("Accept"); fm.Navigate(0, 1); Next(); } break;          // types: MX
                    case 7: if (Age > 1.5) { PlayShot("31_play_vehicle_select_mx"); fm.DispatchKey("Accept"); Next(); } break;
                    case 8: if (Age > 1.5) { PlayShot("32_play_vehicle_list"); Check(vs != null && vs.vehicles.Count > 0 && vs.vehicles[vs.VehicleIndex].id == 43, "AIVehicle 43 (his MX)"); fm.DispatchKey("Accept"); Next(); } break;
                    case 9: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Locations Page", "locations (play mode)"); for (int k = FeTables.Shared.Locations(10).FindIndex(l => l.filename == "NAT_Concord"); k > 0; k--) fm.Navigate(1, 0); Next(); } break;     // NAT_Concord (after the DLC locations)
                    case 10: if (Age > 1) { fm.DispatchKey("Toggle"); fm.Navigate(0, 1); fm.Navigate(-1, 0); fm.Navigate(-1, 0); fm.DispatchKey("Toggle"); Next(); } break;   // AI None
                    case 11: if (Age > 1) { PlayShot("33_play_locations"); fm.DispatchKey("Accept"); Next(); } break;
                    case 12:
                    {
                        var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 3)
                        {
                            var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                            Check(sel && sel.Current == "National/NAT_Concord", "race scene event " + (sel ? sel.Current : "?"));
                            Check(rt.garage != null && rt.garage.riderId == 1 && rt.garage.vehicleId == 43 && rt.garage.vehicleClass == 2, "race scene garage: " + rt.garage);
                            var local = rt.Roster?.Local;
                            Log("roster local: " + local);
                            Check(local != null && local.riderOwner == 4 && local.ownerRiderId == 1 && local.gearSetId == 3 && local.vehicleOwner == 4 && local.ownerVehicleId == 43 && local.vehicleModelId == 32,
                                  "the running game's local slot: Rider List (owner 4, AIRiderId 1, gear set 3), Vehicle List (owner 4, AIVehicleId 43 -> VehicleModel 32)");
                            Check(local?.bike != null && local.rider != null, "Unity draws that bike and gear: " + local?.bike?.chassisPkg + " / " + (local?.rider != null ? string.Join(",", local.rider.parts.Select(p => p.modelPkg)) : "none"));
                            Next();
                        }
                        else if (Age > 400) Fail("race world not running after 400 s");
                        break;
                    }
                    case 13: if (Age > 3) { PlayShot("34_play_race_gate"); Next(); } break;
                    case 14: if (Age > 25) { PlayShot("35_play_race_later"); var r = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>(); Log("status: " + (r && r.statusLabel ? r.statusLabel.text.Replace("\n", " | ") : "-")); OriginalPauseMenu.QuitToFrontEnd(); Next(); } break;
                    case 15: if (fm && fm.assets != null && Age > 3 && !fm.Busy) { Check(fm.CurrentPage == "Event Select Page", "QUIT returns to the front end at " + fm.CurrentPage); Next(); } else if (Age > 60) Fail("front end not back"); break;
                    case 16: stage = 100; EditorApplication.ExitPlaymode(); break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            var after = SavePrefs();
            if (!after.All(kv => kv.Value.Equals(saved[kv.Key])))
            {
                Log("PlayerPrefs changed during the run, restoring");
                foreach (var kv in saved)
                {
                    if (!kv.Value.has) { PlayerPrefs.DeleteKey(kv.Key); continue; }
                    if (!string.IsNullOrEmpty(kv.Value.s)) PlayerPrefs.SetString(kv.Key, kv.Value.s);
                    else if (kv.Value.i != int.MinValue) PlayerPrefs.SetInt(kv.Key, kv.Value.i);
                    else if (!float.IsNaN(kv.Value.f)) PlayerPrefs.SetFloat(kv.Key, kv.Value.f);
                }
                PlayerPrefs.Save(); after = SavePrefs();
            }
            Check(after.All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs restored");
            Log("vehicle select race: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "race-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
