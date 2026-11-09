using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // ATVs (Content/ReflexAtv.cs, _re/notes/atv.md).
    //   Verify: edit mode. Which vehicles are ATVs (VehicleType FactoryName "Atv") and their hierarchy (VehicleHierarchies), every AI
    //           ATV of the tables as a BikeSpec and a built model (50 ATV_Hierarchy nodes, every part on a node, every binding a
    //           material), the events an ATV races (ReflexEvents) and the F2 list's ATV choice (Vehicle Select's first pro with an
    //           ATV, LocationSelect's ATV class) and its world name; garage-style renders of the pros' ATVs to $ATV_SHOTS.
    //   Race:   play mode. The race scene on $ATV_EVENT (National/NAT_Concord) with $ATV_AI (2: Amateur, 11 AI; 0 none), on an ATV
    //           ($ATV_MX=1: the profile's MX Lite, the regression run): the world is built, the local and AI vehicles are ATVs in the
    //           running game (reflex_world_vehicle) and in Unity (the roster's specs, the built models), every vehicle and rider is
    //           posed by the game (50 / 69 nodes); throttle from the gate drop; screenshots at the gate and in the race (game
    //           camera, FPV, follow). PlayerPrefs: nothing written (ReflexEventSelect.Pending*); the user's keys are checked.
    //   FrontEndRace: play mode. The original front end: ARCADE -> NATIONALS -> Vehicle Select James Stewart -> ATV -> his ATV ->
    //           Locations (VehicleClass: STR_VEHICLECLASS_ATV) -> NAT_Concord with the default AI -> the race on that ATV; back to the front end.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.AtvTests.Verify (-quit) / .Race / .FrontEndRace (no -quit)
    public static class AtvTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[ATV] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string shots;
        static string Shots() { var s = Env("ATV_SHOTS", Path.Combine(Application.temporaryCachePath, "atv-shots")); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));

        // The running game's ATV model node order (reflex_world_bike_pose of an ATV world, checked against the bind positions).
        static readonly string[] HierarchyHead = { "OBJ_Frame", "OBJ_Swingarm", "OBJ_BCK_CTR_ShockSpring", "OBJ_BCK_CTR_LWR_Shock", "OBJ_Axle", "OBJ_BCK_RHT_Wheel", "SWP_BCK_RHT_Tire", "OBJ_BCK_LFT_Wheel", "SWP_BCK_LFT_Tire", "OBJ_SteeringStem", "OBJ_ClutchLVR" };

        // ---------------------------------------------------------------------------------------------------- verify (edit mode)
        public static void Verify()
        {
            report.Clear(); bad = 0; shots = Shots();
            var before = SavePrefs();
            var warnings = new List<string>();
            void OnLog(string msg, string st, LogType type) { if (type == LogType.Warning && (msg.StartsWith("Reflex bike") || msg.StartsWith("Reflex texture"))) warnings.Add(msg); }
            Application.logMessageReceived += OnLog;
            bool async = ShaderUtil.allowAsyncCompilation; ShaderUtil.allowAsyncCompilation = false;   // the renders need the shaders compiled
            try
            {
                var fe = FeTables.Shared;
                var t = new ReflexGameTables(new FeTableSource(fe));
                // which vehicles are ATVs: VehicleModel -> VehicleType FactoryName, VehicleHierarchies
                var atvModels = fe.Rows("VehicleModel").Where(r => ReflexAtv.IsAtvModel(FeTables.I(r, "VehicleModelId"), t)).Select(r => FeTables.I(r, "VehicleModelId")).ToList();
                Log("ATV vehicle models: " + string.Join(",", atvModels));
                Check(atvModels.Count > 0 && atvModels.All(m => ReflexAtv.TypeOfModel(m, t) == ReflexAtv.VehicleTypeAtv && ReflexAtv.HierarchyOf(m, t) == "ATV_Hierarchy"), "every ATV model: VehicleType 4 (FactoryName Atv), VehicleHierarchies ATV_Hierarchy");
                Check(!ReflexAtv.IsAtvModel(11, t) && ReflexAtv.HierarchyOf(11, t) == "MX_Hierarchy", "the profile's MX Lite (VehicleModel 11) is not an ATV, MX_Hierarchy");
                Check(ReflexAtv.ClassFor(10, 4) == 1 && ReflexAtv.ClassFor(8, 4) == 1 && ReflexAtv.ClassFor(10, 2) == 3, "LocationSelect's first class: ATV -> 1 (National, Supercross), MX Lite -> 3");
                // every AI ATV: spec and model
                var rows = fe.Rows("AIRiderVehicle").Where(r => FeTables.I(r, "VehicleTypeId") == ReflexAtv.VehicleTypeAtv).ToList();
                var pros = fe.Rows("AIRider").Where(r => FeTables.I(r, "PlayerSelectable", 0) != 0).Select(r => FeTables.I(r, "AIRiderId")).ToHashSet();
                int built = 0, specOk = 0; long verts = 0, tris = 0; var watch = System.Diagnostics.Stopwatch.StartNew();
                var unbound = new HashSet<string>();
                foreach (var r in rows)
                {
                    int vid = FeTables.I(r, "AIVehicleId"), model = FeTables.I(fe.Rows("AIVehicle").FirstOrDefault(x => FeTables.I(x, "AIVehicleId") == vid), "VehicleModelId");
                    var parts = t.Where("AIVehicleParts", "AIVehicleId", vid).Select(i => t.Int("AIVehicleParts", "VehiclePartId", i)).ToList();
                    var spec = ReflexVehicleSpecs.Bike(t, model, parts);
                    if (spec == null || !spec.atv || spec.hierarchyModel != "ATV_Hierarchy") { Check(false, $"AIVehicle {vid} (model {model}): spec {(spec == null ? "null" : spec.hierarchyModel)}"); continue; }
                    specOk++;
                    ReflexBikeModel m = null;
                    try
                    {
                        m = ReflexBikeModel.Build(spec);
                        bool nodes = m.hierarchyNodes.Length == 50 && HierarchyHead.Select((n, i) => m.hierarchyNodes[i] == n).All(x => x);
                        int v = 0, tr = 0;
                        foreach (var f in m.chassis.GetComponentsInChildren<MeshFilter>()) { v += f.sharedMesh.vertexCount; tr += f.sharedMesh.triangles.Length / 3; }
                        foreach (var p in m.parts) if (p.material != null && p.part.GetComponent<MeshRenderer>().sharedMaterial.GetTexture("_BaseMap") == null && !p.material.StartsWith("ATV_Alpha", StringComparison.Ordinal)) unbound.Add(p.material);
                        verts += v; tris += tr; built++;
                        if (pros.Contains(FeTables.I(r, "AIRiderId")) || built <= 3)
                            Log($"AIVehicle {vid} rider {FeTables.I(r, "AIRiderId")} model {model}: {spec.chassisPkg} kit {spec.kitPkg}, {spec.partModels.Count} part models ({string.Join(",", spec.partModels.Select(p => p.pkg))}), {spec.materials.Count} material packages -> {m.parts.Count} part meshes, {v} vertices, {tr} triangles, nodes {(nodes ? "ATV_Hierarchy x50" : "WRONG")}");
                        if (!nodes) Check(false, $"AIVehicle {vid}: hierarchy nodes {string.Join(",", m.hierarchyNodes.Take(12))}");
                        if (pros.Contains(FeTables.I(r, "AIRiderId"))) Render(m, $"garage_atv_rider{FeTables.I(r, "AIRiderId")}_vehicle{vid}");
                    }
                    catch (Exception e) { Check(false, $"AIVehicle {vid}: build {e.Message}"); }
                    finally { if (m != null) Kill(m); }
                }
                Check(specOk == rows.Count && built == rows.Count, $"every AI ATV ({rows.Count} AIRiderVehicle rows of type 4): {specOk} specs, {built} models built ({verts} vertices, {tris} triangles) in {watch.ElapsedMilliseconds} ms");
                Log("bindings drawn without a diffuse texture (their game material has none, or no match): " + (unbound.Count == 0 ? "none" : string.Join(", ", unbound.OrderBy(x => x))));
                Check(warnings.Count == 0, $"no part off the hierarchy, no missing material or texture ({warnings.Count} warnings{(warnings.Count > 0 ? ": " + string.Join(" | ", warnings.Distinct().Take(8)) : "")})");
                // the events
                var all = ReflexEvents.All;
                Log($"ReflexEvents: {all.Count} events, {all.Count(e => e.Atv)} with the ATV class: " + string.Join(", ", all.Where(e => e.Atv).Select(e => e.key)));
                foreach (var key in new[] { "National/NAT_Concord", "Supercross/SX_Kingston", "Terracross/TRX_Verde", "WaypointRace/FR_Alpine/1", "FreeRide/FR_Alpine" })
                {
                    var e = ReflexEvents.Find(key); var g = e?.AtvGarage();
                    Check(e != null && e.Atv && g != null && g.riderOwner == 4 && g.riderId == 4 && g.vehicleOwner == 4 && g.vehicleTypeId == 4 && g.vehicleClass == 1
                          && ReflexAtv.IsAtvModel(FeTables.I(fe.Rows("AIVehicle").FirstOrDefault(x => FeTables.I(x, "AIVehicleId") == g.vehicleId), "VehicleModelId"), t),
                          $"{key}: classes [{string.Join(",", e?.vehicleClasses ?? new int[0])}], on an ATV: {g} -> world {ReflexWorldFiles.For(key, 11, 95, 0, g).name}");
                }
                Check(all.Where(e => e.spec.type == "National" || e.spec.type == "Supercross").All(e => e.Atv), "every National and Supercross offers the ATV class");
                Check(ReflexEvents.Find("Endurocross/ENX_Dallas")?.Atv != true, "Endurocross has no ATV class (EventTypeVehicleClasses 2, 3, 11)");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally { Application.logMessageReceived -= OnLog; ShaderUtil.allowAsyncCompilation = async; }
            Check(SavePrefs().All(kv => kv.Value.Equals(before[kv.Key])), "user PlayerPrefs untouched");
            Log("ATV verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "verify-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        static void Kill(ReflexBikeModel m)
        {
            foreach (var g in new[] { m.chassis, m.frontWheel ? m.frontWheel.gameObject : null, m.rearWheel ? m.rearWheel.gameObject : null })
            {
                if (!g) continue;
                foreach (var f in g.GetComponentsInChildren<MeshFilter>(true)) UnityEngine.Object.DestroyImmediate(f.sharedMesh);
                foreach (var r in g.GetComponentsInChildren<MeshRenderer>(true)) foreach (var x in r.sharedMaterials) if (x) UnityEngine.Object.DestroyImmediate(x);
                UnityEngine.Object.DestroyImmediate(g);
            }
        }

        // a 3/4 view of a vehicle built at the origin (bind pose), as the garage shows one
        static void Render(ReflexBikeModel m, string name)
        {
            const int layer = 30;
            foreach (var tr in m.chassis.GetComponentsInChildren<Transform>(true)) tr.gameObject.layer = layer;
            var b = new Bounds(); bool any = false;
            foreach (var r in m.chassis.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            var cgo = new GameObject("atv shot camera"); var cam = cgo.AddComponent<Camera>();
            var lgo = new GameObject("atv shot light"); var l = lgo.AddComponent<Light>(); l.type = LightType.Directional; l.intensity = 1.25f; l.cullingMask = 1 << layer; lgo.transform.rotation = Quaternion.Euler(38, 150, 0);
            var rt = new RenderTexture(1280, 720, 24) { antiAliasing = 4 };
            cam.targetTexture = rt; cam.cullingMask = 1 << layer; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = new Color(0.16f, 0.17f, 0.19f); cam.fieldOfView = 34;
            cam.transform.position = b.center + new Vector3(-1.05f, 0.42f, 1.0f).normalized * b.extents.magnitude * 3.3f; cam.transform.LookAt(b.center);
            cam.Render(); cam.Render();   // (the first render of a batch-mode session comes out empty)
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(cgo); UnityEngine.Object.DestroyImmediate(lgo); rt.Release(); UnityEngine.Object.DestroyImmediate(rt);
            Log("shot " + name);
        }

        // ---------------------------------------------------------------------------------------------------- race (play mode)
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static int stage; static double stageAt, runningAt, raceAt; static bool mx; static string tag; static int ai; static float maxSpeed;

        // a simple autopilot (the bench showcase's): throttle, and A (0x1e) / D (0x20) toward the race line ~45 ft ahead of the vehicle
        static int[] Steer(OriginalWorldRuntime rt)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            var onboard = rt.onboard;
            if (onboard == null || st.position == null) return new[] { 0x11 };
            var line = (float[])onboard.GetType().GetField("raceLine", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard);
            int n = (int)(onboard.GetType().GetField("raceLinePoints", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard) ?? 0);
            if (line == null || n < 2) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            Vector3 target = new Vector3(line[3 * (n - 1)], 0, line[3 * (n - 1) + 2]);
            for (int i = 0; i < n; i++) { var p = new Vector3(line[3 * i], 0, line[3 * i + 2]); if ((p - pos).magnitude > 45f) { target = p; break; } }
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x;
            int key = Vector3.Angle(vel, d) < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;

        public static void Race()
        {
            report.Clear(); bad = 0; shots = Shots(); saved = SavePrefs();
            mx = Env("ATV_MX", "0") == "1"; ai = int.Parse(Env("ATV_AI", "2")); string key = Env("ATV_EVENT", "National/NAT_Concord");
            tag = Env("ATV_TAG", (mx ? "mx_" : "atv_") + key.Replace('/', '_'));
            var ev = ReflexEvents.Find(key);
            if (ev == null) { Check(false, "no event " + key); End(); return; }
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = mx ? null : ev.AtvGarage();
            Log($"race {key} AI {ai} {(mx ? "on the profile's MX Lite" : "on an ATV: " + ReflexEventSelect.PendingGarage)}");
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            stage = 0; stageAt = EditorApplication.timeSinceStartup; runningAt = raceAt = -1; maxSpeed = 0;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void PlayShot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false;
            var target = new RenderTexture(1920, 1080, 24) { antiAliasing = 4 };
            var main = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>()?.chaseCamera ?? Camera.main;
            if (main) c.CopyFrom(main);
            c.targetTexture = target; c.enabled = false;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1920, 1080, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1920, 1080), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, tag + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + tag + "_" + name);
        }

        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);

        static void Step()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                double now = EditorApplication.timeSinceStartup;
                if (rt && rt.Running && runningAt < 0) runningAt = now;
                var race = rt && rt.Running ? RaceState(rt) : default;
                if (rt && raceAt < 0 && race.valid != 0 && race.state >= 2) raceAt = now;
                if (rt) rt.testHeldKeys = raceAt >= 0 ? Steer(rt) : Array.Empty<int>();   // throttle (W) from the gate drop, A / D toward the race line
                if (rt && raceAt >= 0 && rt.TryGetVehicle(out var cur)) maxSpeed = Mathf.Max(maxSpeed, cur.speed);
                switch (stage)
                {
                    case 0:
                        if (rt && rt.Running && now - runningAt > 4 && rt.TryGetVehicle(out _))
                        {
                            var local = rt.Roster?.Local;
                            Log("roster local: " + local);
                            bool hasV = rt.TryGetVehicle(out var v);
                            Log($"local vehicle: export {hasV} type {v.type} bodies {v.bodies} model nodes {v.modelNodes} gear {v.gear} speed {v.speed:0.0}");
                            if (mx) Check(local?.bike != null && !local.bike.atv && hasV && v.type == 0 && v.bodies == 4 && v.modelNodes == 18, "MX: the local vehicle is a motocross bike (RavenVehicleMotocross, 4 bodies, MX_Hierarchy 18) drawn as " + local?.bike?.chassisPkg);
                            else Check(local?.bike != null && local.bike.atv && hasV && v.type == 1 && v.bodies == 6 && v.modelNodes == 50, "ATV: the local vehicle is an ATV (RavenVehicleATV, 6 bodies, ATV_Hierarchy 50) drawn as " + local?.bike?.chassisPkg + " kit " + local?.bike?.kitPkg);
                            Check(rt.bikePose && rt.bikePose.HasPose, "the local vehicle posed from the game's model nodes (OriginalBikePose)");
                            Check(rt.rider && rt.rider.HasOriginalPose, "the local rider posed by the game's rider animation (69 bones)");
                            var opp = rt.opponentRiders;
                            int want = ai > 0 ? ReflexWorldFiles.FullGateOpponents : 0;
                            Check((opp ? opp.Count : 0) == want, $"opponents {(opp ? opp.Count : 0)} == {want}");
                            if (opp && want > 0) Log("opponents: " + string.Join("; ", rt.Roster.entries.Where(e => !e.Local).Select(e => $"{e.name}: {(e.bike?.atv == true ? "ATV " : "")}{e.bike?.chassisPkg} {e.bike?.kitPkg}")));
                            if (opp && want > 0) Check(rt.Roster.entries.Where(e => !e.Local).All(e => e.bike != null && e.bike.atv == !mx), (mx ? "every AI on a motocross bike" : "every AI on an ATV") + " (the running game's AIVehicle -> VehicleType)");
                            PlayShot("01_gate");
                            Next();
                        }
                        else if (Age > 420) Fail("world not running after 420 s");
                        break;
                    case 1:
                        if (raceAt >= 0 && now - raceAt > 2.0)
                        {
                            PlayShot("02_race_2s");
                            var opp = rt.opponentRiders;
                            if (opp && opp.Count > 0) Check(opp.Built == opp.Count && opp.Posed == opp.Count && opp.Riders.All(o => o.rider != null && o.rider.rider.HasOriginalPose), $"opponents built {opp.Built}, posed {opp.Posed} of {opp.Count}; their riders posed by the game");
                            Next();
                        }
                        else if (Age > 240) Fail("the gate did not drop");
                        break;
                    case 2: if (now - raceAt > 6.0) { PlayShot("03_race_6s"); Next(); } break;
                    case 3: if (now - raceAt > 12.0) { PlayShot("03_race_12s"); rt.cameraMode = 1; stage = 5; stageAt = now; } break;   // cameraMode: the field only (SetCameraMode saves the user's preference)
                    case 5: if (Age > 1.5) { PlayShot("04_fpv"); rt.cameraMode = 2; Next(); } break;
                    case 6:
                        if (Age > 1.5)
                        {
                            PlayShot("05_follow"); rt.cameraMode = 0;
                            rt.TryGetVehicle(out var v);
                            Log($"race {race.state} place {race.place}/{race.riders} lap progress {race.lap_progress:0.000} speed {v.speed:0.0} (max {maxSpeed:0.0}) gear {v.gear} rpm {v.rpm:0} sim {rt.SimMilliseconds:0.00} ms");
                            Check(maxSpeed > 25 && race.lap_progress > 0.05f, $"riding: top speed {maxSpeed:0.0} ft/s, lap progress {race.lap_progress:0.000}");
                            stage = 100; EditorApplication.ExitPlaymode();
                        }
                        break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        // ---------------------------------------------------------------------------------------------------- the front end's ATV (play mode)
        public static void FrontEndRace()
        {
            report.Clear(); bad = 0; shots = Shots(); saved = SavePrefs(); tag = "frontend_atv";
            ReflexProfile.Disabled = true;   // the guest path (no profile session): title -> profile reminder
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); fm.rememberChoice = false;
            stage = 0; stageAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += FeStep;
            EditorApplication.EnterPlaymode();
        }

        static void FeStep()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) { EditorApplication.update -= FeStep; End(); } else if (Age > 120) { Check(false, "play mode never started"); EditorApplication.update -= FeStep; End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                VehicleSelectScript vs = fm && fm.assets != null ? fm.Layer("VehicleSelect.lyr").script as VehicleSelectScript : null;
                switch (stage)
                {
                    case 0: if (fm && fm.assets != null && Age > 2) { fm.DispatchKey("Accept"); Next(); } else if (Age > 60) FeFail("front end did not boot"); break;
                    case 1: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 2: if (Age > 2 && !fm.Busy) { fm.Navigate(-1, 0); fm.DispatchKey("Accept"); Next(); } break;                         // ARCADE
                    case 3: if (Age > 2 && !fm.Busy) { fm.Navigate(1, 0); Next(); } break;                                                      // NATIONALS
                    case 4: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 5: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Vehicle Select Page", "vehicle select"); fm.Navigate(1, 0); Next(); } break;   // James Stewart
                    case 6: if (Age > 2) { Check(vs.riders[vs.RiderIndex].id == 4 && vs.types.Any(t => t.id == 4 && !t.locked), "James Stewart (AIRiderId 4), his ATV type offered"); fm.DispatchKey("Accept"); fm.Navigate(0, 1); fm.Navigate(0, 1); Next(); } break;
                    case 7: if (Age > 1.5) { Check(vs.types[vs.TypeIndex].id == 4, "vehicle type ATV"); fm.DispatchKey("Accept"); Next(); } break;
                    case 8: if (Age > 1.5) { Check(vs.vehicles.Count > 0 && ReflexAtv.IsAtvModel(vs.vehicles[vs.VehicleIndex].modelId), $"his ATV: AIVehicle {vs.vehicles[vs.VehicleIndex].id}, VehicleModel {vs.vehicles[vs.VehicleIndex].modelId}"); fm.DispatchKey("Accept"); Next(); } break;
                    case 9: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Locations Page", "locations"); for (int k = FeTables.Shared.Locations(10).FindIndex(l => l.filename == "NAT_Concord"); k > 0; k--) fm.Navigate(1, 0); Next(); } break;     // NAT_Concord (after the DLC locations)
                    case 10: if (Age > 1) { fm.DispatchKey("Toggle"); fm.Navigate(0, 1); fm.Navigate(0, 1); Next(); } break;                    // the rules' vehicle class row
                    case 11: if (Age > 1) { Check(fm.Var("VehicleClass", -1) == ReflexAtv.VehicleClassAtv && fm.Var("Player1VehicleTypeId", -1) == 4, "LocationSelect's vehicle class for the ATV: 1 (STR_VEHICLECLASS_ATV): " + fm.Var("VehicleClass", -1)); fm.DispatchKey("Toggle"); Next(); } break;
                    case 12: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 13:
                    {
                        var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 3 && rt.TryGetVehicle(out _))   // (a frame completed)
                        {
                            var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                            Check(sel && sel.Current == "National/NAT_Concord", "race scene event " + (sel ? sel.Current : "?"));
                            Check(rt.garage != null && rt.garage.riderId == 4 && rt.garage.vehicleTypeId == 4 && rt.garage.vehicleClass == 1, "race scene garage (VehicleSelect:End + LocationSelect's class): " + rt.garage);
                            var local = rt.Roster?.Local; rt.TryGetVehicle(out var v);
                            Log("roster local: " + local);
                            Check(local?.bike != null && local.bike.atv && v.type == 1 && v.bodies == 6, $"the running game's local vehicle is an ATV (type {v.type}, {v.bodies} bodies), drawn as {local?.bike?.chassisPkg} {local?.bike?.kitPkg}");
                            Check(rt.Roster.entries.Where(e => !e.Local).All(e => e.bike != null && e.bike.atv), $"the {rt.Roster.entries.Count - 1} AI ride ATVs");
                            Next();
                        }
                        else if (Age > 400) FeFail("race world not running after 400 s");
                        break;
                    }
                    case 14: if (Age > 3) { PlayShot("13_race_gate"); OriginalPauseMenu.QuitToFrontEnd(); Next(); } break;
                    case 15: if (fm && fm.assets != null && Age > 3 && !fm.Busy) { Check(fm.CurrentPage == "Event Select Page", "QUIT returns to the front end at " + fm.CurrentPage); Next(); } else if (Age > 60) FeFail("front end not back"); break;
                    case 16: stage = 100; EditorApplication.ExitPlaymode(); break;
                }
            }
            catch (Exception e) { FeFail("exception: " + e); }
        }
        static void FeFail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else { EditorApplication.update -= FeStep; End(); } }


        static void End()
        {
            EditorApplication.update -= Step;
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log($"ATV race {tag}: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, tag + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
