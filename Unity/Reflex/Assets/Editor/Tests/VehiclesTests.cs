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
    // The machines: UTV, Super Buggy, Pro Lite and Pro 2 trucks (Content/ReflexVehicles.cs, _re/notes/vehicles.md).
    //   Verify: edit mode. Which vehicle types are machines (VehicleType FactoryName -> the factory index), every AI machine of the tables as a
    //           BikeSpec and a built model (its hierarchy's nodes, every part on a node, every chassis binding a kit material by the game's override
    //           rule, the DMG_ panels hidden), the templates of their materials, the events that race them (Powercross: machine classes only)
    //           and their garages; garage-style renders of the pros' machines to $VEH_SHOTS.
    //   Race:   play mode. The race scene on $VEH_EVENT (Powercross/PX_Laguna) with $VEH_AI (2: Amateur, 11 AI; 0 none) on $VEH_TYPE (11 UTV,
    //           12 Super Buggy, 14 Pro Lite, 15 Pro 2): the local and AI vehicles are that machine in the running game (reflex_world_vehicle:
    //           factory class, 5 bodies, the hierarchy's nodes) and in Unity, posed by the game (nodes, node flags, 69 rider bones); autopilot
    //           from the gate drop; screenshots at the gate and in the race (game camera, FPV, follow). PlayerPrefs untouched.
    //   FrontEndRace: play mode. ARCADE -> POWERCROSS -> Vehicle Select (first pro, UTV, his UTV) -> Locations (VehicleClass 22 STR_VEHICLECLASS_UTV)
    //           -> PX_Laguna -> the race on that UTV.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.VehiclesTests.Verify (-quit) / .Race / .FrontEndRace (no -quit)
    public static class VehiclesTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[VEH] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string shots;
        static string Shots() { var s = Env("VEH_SHOTS", Path.Combine(Application.temporaryCachePath, "vehicle-shots")); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        // the running game's model node counts (reflex_world_vehicle model_nodes of a booted world of each)
        static readonly Dictionary<string, int> HierarchyNodes = new Dictionary<string, int> { ["UTV01_Hierarchy"] = 61, ["SB02_Hierarchy"] = 56, ["ProLite_Hierarchy"] = 57, ["Pro2_Hierarchy"] = 61 };
        static readonly Dictionary<int, int> FactoryOfType = new Dictionary<int, int> { [11] = 7, [12] = 5, [14] = 3, [15] = 3 };

        // ---------------------------------------------------------------------------------------------------- verify (edit mode)
        public static void Verify()
        {
            report.Clear(); bad = 0; shots = Shots();
            var before = SavePrefs();
            var warnings = new List<string>();
            void OnLog(string msg, string st, LogType type) { if (type == LogType.Warning && (msg.StartsWith("Reflex bike") || msg.StartsWith("Reflex texture"))) warnings.Add(msg); }
            Application.logMessageReceived += OnLog;
            bool async = ShaderUtil.allowAsyncCompilation; ShaderUtil.allowAsyncCompilation = false;
            try
            {
                var fe = FeTables.Shared;
                var t = new ReflexGameTables(new FeTableSource(fe));
                foreach (var vt in fe.Rows("VehicleType"))
                {
                    int id = FeTables.I(vt, "VehicleTypeId"); int models = fe.Rows("VehicleModel").Count(r => FeTables.I(r, "VehicleTypeId") == id);
                    Log($"VehicleType {id} {FeTables.S(vt, "ShortName")}: FactoryName {FeTables.S(vt, "FactoryName")} (index {ReflexVehicles.FactoryOfType(id, t)}), {models} models, drawable {ReflexVehicles.Drawable(id, t)}");
                }
                Check(ReflexVehicles.MachineTypes.All(x => ReflexVehicles.FactoryOfType(x, t) == FactoryOfType[x] && ReflexVehicles.Drawable(x, t)), "machine types 11 / 12 / 14 / 15: factories 7 Rhino / 5 Buggy / 3 TrophyTruck, drawable");
                Check(ReflexVehicles.MaterialKey("Pro201_Body_Black") == "Pro_Body" && ReflexVehicles.MaterialKey("UTV01_FrameCC") == "UTV_FrameCC" && ReflexVehicles.MaterialKey("ProTruck_Wheel01") == "ProTruck_Wheel"
                      && ReflexVehicles.MaterialKey("HR_UTV01_Body_Flare01") == "HR_UTV_Body" && ReflexVehicles.MaterialKey("MX_Exh_FMF_PowerCore_250_Pipe") == "MX_Exh", "FUN_00543fc0 material keys");
                var pros = fe.Rows("AIRider").Where(r => FeTables.I(r, "PlayerSelectable", 0) != 0).Select(r => FeTables.I(r, "AIRiderId")).ToHashSet();
                var templates = new Dictionary<string, int>();
                foreach (int type in ReflexVehicles.MachineTypes)
                {
                    var rows = fe.Rows("AIRiderVehicle").Where(r => FeTables.I(r, "VehicleTypeId") == type).ToList();
                    int built = 0, specOk = 0, kitBound = 0, bindings = 0, hidden = 0; long verts = 0; var watch = System.Diagnostics.Stopwatch.StartNew();
                    var chassis = new HashSet<string>(); var unbound = new HashSet<string>();
                    foreach (var r in rows)
                    {
                        int vid = FeTables.I(r, "AIVehicleId"), model = FeTables.I(fe.Rows("AIVehicle").FirstOrDefault(x => FeTables.I(x, "AIVehicleId") == vid), "VehicleModelId");
                        var parts = t.Where("AIVehicleParts", "AIVehicleId", vid).Select(i => t.Int("AIVehicleParts", "VehiclePartId", i)).ToList();
                        var spec = ReflexVehicleSpecs.Bike(t, model, parts);
                        if (spec == null || !spec.machine || spec.atv || ReflexAtv.HierarchyOf(model, t) != spec.hierarchyModel) { Check(false, $"type {type} AIVehicle {vid} (model {model}): spec {(spec == null ? "null" : spec.hierarchyModel)}"); continue; }
                        specOk++; chassis.Add(spec.chassisPkg + " " + spec.hierarchyModel);
                        ReflexBikeModel m = null;
                        try
                        {
                            m = ReflexBikeModel.Build(spec);
                            int want = HierarchyNodes.TryGetValue(spec.hierarchyModel, out int w) ? w : -1;
                            if (want > 0 && m.hierarchyNodes.Length != want) Check(false, $"AIVehicle {vid}: {spec.hierarchyModel} has {m.hierarchyNodes.Length} nodes, the game's model {want}");
                            foreach (var f in m.chassis.GetComponentsInChildren<MeshFilter>(true)) verts += f.sharedMesh.vertexCount;
                            foreach (var p in m.parts)
                            {
                                var mat = p.part.GetComponent<MeshRenderer>().sharedMaterial; bindings++;
                                if (mat.name.Contains(" -> ") || spec.partModels.Count > 0 && p.name.Contains("_Wheel")) kitBound++;
                                int tp = (int)mat.GetFloat("_Program"); string ps = mat.name;   // Reflex/Vehicle: the ported program (ReflexObjectShading.Programs)
                                string prog = tp >= 0 && tp < ReflexObjectShading.Programs.Length ? ReflexObjectShading.Programs[tp].name : "?";
                                templates[ps.Substring(ps.IndexOf("-> ", StringComparison.Ordinal) < 0 ? 0 : ps.IndexOf("-> ", StringComparison.Ordinal) + 3).Split(' ')[0] + " " + prog] = prog.Contains("Machine") ? 6 : 0;
                                if (mat.GetTexture("_Tex0") == null && mat.GetTexture("_Tex1") == null && mat.GetTexture("_Tex2") == null) unbound.Add(p.material);
                                if (ReflexVehicles.HiddenAtStart(m.hierarchyNodes[p.node])) { if (!p.part.gameObject.activeSelf) hidden++; else Check(false, $"AIVehicle {vid}: damage panel {p.name} shown"); }
                            }
                            built++;
                            if (pros.Contains(FeTables.I(r, "AIRiderId"))) { Log($"type {type} AIVehicle {vid} rider {FeTables.I(r, "AIRiderId")} model {model}: {spec.chassisPkg} on {spec.hierarchyModel} kit {spec.kitPkg} -> {m.parts.Count} part meshes"); Render(m, $"garage_type{type}_rider{FeTables.I(r, "AIRiderId")}_vehicle{vid}"); }
                        }
                        catch (Exception e) { Check(false, $"AIVehicle {vid}: build {e.Message}"); }
                        finally { if (m != null) Kill(m); }
                    }
                    Check(rows.Count > 0 && specOk == rows.Count && built == rows.Count, $"type {type}: every AI machine ({rows.Count} AIRiderVehicle rows): {specOk} specs, {built} built ({verts} vertices) in {watch.ElapsedMilliseconds} ms; chassis {string.Join(", ", chassis)}; {kitBound}/{bindings} meshes on kit materials, {hidden} damage panels hidden");
                    Log($"type {type}: bindings drawn without a diffuse texture: " + (unbound.Count == 0 ? "none" : string.Join(", ", unbound.OrderBy(x => x))));
                }
                Log("materials -> templates: " + string.Join("; ", templates.Keys.OrderBy(k => k)));
                Check(templates.Values.Count(v => v >= 6) > 0, "machine materials carry their ported programs (Template_Machine*)");
                Check(warnings.Count == 0, $"no part off the hierarchy, no missing material or texture ({warnings.Count} warnings{(warnings.Count > 0 ? ": " + string.Join(" | ", warnings.Distinct().Take(8)) : "")})");
                // the events
                var all = ReflexEvents.All;
                var px = all.Where(e => e.spec.type == "Powercross").ToList();
                Log($"ReflexEvents: {all.Count} events; Powercross: " + string.Join(", ", px.Select(e => $"{e.key} class {e.spec.vehicleClass} [{string.Join(",", e.vehicleClasses)}]")));
                Check(px.Count == 4 && px.All(e => e.MachineOnly && e.spec.vehicleClass == 22), "the 4 Powercross events (PX_Laguna, _BW, B, B_BW): machine classes only, the event's class UTV 22");
                foreach (int type in ReflexVehicles.MachineTypes)
                {
                    var g = px.FirstOrDefault()?.GarageFor(type);
                    Check(g != null && g.riderOwner == 4 && g.vehicleTypeId == type && ReflexVehicles.IsMachineModel(FeTables.I(fe.Rows("AIVehicle").FirstOrDefault(x => FeTables.I(x, "AIVehicleId") == g.vehicleId), "VehicleModelId"), t),
                          $"Powercross on type {type}: {g} -> world {(g != null ? ReflexWorldFiles.For(px[0].key, 11, 95, 0, g).name : "-")}");
                }
                foreach (var key in new[] { "Terracross/TRX_Verde", "WaypointRace/FR_Alpine/1", "FreeRide/FR_Alpine" })
                {
                    var e = ReflexEvents.Find(key); var g = e?.GarageFor(ReflexVehicles.TypeUtv);
                    Check(e != null && !e.MachineOnly && g != null, $"{key}: classes [{string.Join(",", e?.vehicleClasses ?? new int[0])}], on a UTV: {g}");
                }
                Check(ReflexEvents.Find("National/NAT_Concord")?.GarageFor(ReflexVehicles.TypeUtv) == null, "National has no UTV class");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally { Application.logMessageReceived -= OnLog; ShaderUtil.allowAsyncCompilation = async; }
            Check(SavePrefs().All(kv => kv.Value.Equals(before[kv.Key])), "user PlayerPrefs untouched");
            Log("vehicles verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
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

        static void Render(ReflexBikeModel m, string name)
        {
            const int layer = 30;
            foreach (var tr in m.chassis.GetComponentsInChildren<Transform>(true)) tr.gameObject.layer = layer;
            var b = new Bounds(); bool any = false;
            foreach (var r in m.chassis.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            var cgo = new GameObject("vehicle shot camera"); var cam = cgo.AddComponent<Camera>();
            var lgo = new GameObject("vehicle shot light"); var l = lgo.AddComponent<Light>(); l.type = LightType.Directional; l.intensity = 1.25f; l.cullingMask = 1 << layer; lgo.transform.rotation = Quaternion.Euler(38, 150, 0);
            var rt = new RenderTexture(1280, 720, 24) { antiAliasing = 4 };
            cam.targetTexture = rt; cam.cullingMask = 1 << layer; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = new Color(0.16f, 0.17f, 0.19f); cam.fieldOfView = 34;
            cam.transform.position = b.center + new Vector3(-1.05f, 0.42f, 1.0f).normalized * b.extents.magnitude * 3.0f; cam.transform.LookAt(b.center);
            cam.Render(); cam.Render();
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(cgo); UnityEngine.Object.DestroyImmediate(lgo); rt.Release(); UnityEngine.Object.DestroyImmediate(rt);
            Log("shot " + name);
        }

        // ---------------------------------------------------------------------------------------------------- race (play mode)
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static double lastNav; static int stage, vtype; static double stageAt, runningAt, raceAt; static string tag; static int ai; static float maxSpeed;

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
            vtype = int.Parse(Env("VEH_TYPE", "11")); ai = int.Parse(Env("VEH_AI", "2")); string key = Env("VEH_EVENT", "Powercross/PX_Laguna");
            tag = Env("VEH_TAG", "type" + vtype + "_" + key.Replace('/', '_'));
            var ev = ReflexEvents.Find(key);
            var g = ev?.GarageFor(vtype);
            if (g == null) { Check(false, $"no event {key} with a class for type {vtype}"); End(); return; }
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = g;
            Log($"race {key} AI {ai} on type {vtype}: {g}");
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

        /// The Vehicle Select page's garage render target (MXGarage preview), as the page shows it behind the lists.
        static void GarageShot(FeGarage g, string name)
        {
            if (g == null) return;
            g.Render();
            var prev = RenderTexture.active; RenderTexture.active = g.Texture;
            var tex = new Texture2D(g.Texture.width, g.Texture.height, TextureFormat.RGBA32, false); tex.ReadPixels(new Rect(0, 0, g.Texture.width, g.Texture.height), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, tag + "_" + name + ".png"), tex.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(tex);
            Log("shot " + tag + "_" + name);
        }

        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static ReflexBikeModel LocalModel(OriginalWorldRuntime rt) => (ReflexBikeModel)typeof(OriginalWorldRuntime).GetField("bikeModel", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);

        static void CheckLocal(OriginalWorldRuntime rt, int type, string what)
        {
            var local = rt.Roster?.Local;
            Log("roster local: " + local);
            bool hasV = rt.TryGetVehicle(out var v);
            Log($"local vehicle: export {hasV} type {v.type} bodies {v.bodies} model nodes {v.modelNodes} gear {v.gear} speed {v.speed:0.0}");
            int nodes = local?.bike != null && HierarchyNodes.TryGetValue(local.bike.hierarchyModel, out int w) ? w : -1;
            if (local?.bike != null && nodes < 0) nodes = v.modelNodes;   // (UTV02 / SB01 have their own counts)
            Check(local?.bike != null && local.bike.machine && hasV && v.type == FactoryOfType[type] && v.bodies == 5 && v.modelNodes == nodes && v.Machine,
                  $"{what}: the local vehicle is factory class {FactoryOfType[type]} (type {v.type}, 5 bodies, {v.modelNodes} model nodes) drawn as {local?.bike?.chassisPkg} {local?.bike?.kitPkg} on {local?.bike?.hierarchyModel}");
            var m = LocalModel(rt);
            if (m != null) { int dmg = m.parts.Count(p => ReflexVehicles.HiddenAtStart(m.hierarchyNodes[p.node])), shown = m.parts.Count(p => ReflexVehicles.HiddenAtStart(m.hierarchyNodes[p.node]) && p.part.gameObject.activeSelf); Check(dmg > 0 && shown == 0, $"its {dmg} damage panel meshes hidden by the game's node flags ({shown} shown)"); }
        }

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
                if (rt) rt.testHeldKeys = raceAt >= 0 ? Steer(rt) : Array.Empty<int>();
                if (rt && raceAt >= 0 && rt.TryGetVehicle(out var cur)) maxSpeed = Mathf.Max(maxSpeed, cur.speed);
                switch (stage)
                {
                    case 0:
                        if (rt && rt.Running && now - runningAt > 4 && rt.TryGetVehicle(out _))
                        {
                            CheckLocal(rt, vtype, "race");
                            Check(rt.bikePose && rt.bikePose.HasPose, "the local vehicle posed from the game's model nodes (OriginalBikePose)");
                            Check(rt.rider && rt.rider.HasOriginalPose, "the local driver posed by the game's rider animation (69 bones)");
                            var opp = rt.opponentRiders;
                            int want = ai > 0 ? ReflexWorldFiles.FullGateOpponents : 0;
                            Check((opp ? opp.Count : 0) == want, $"opponents {(opp ? opp.Count : 0)} == {want}");
                            if (opp && want > 0)
                            {
                                Log("opponents: " + string.Join("; ", rt.Roster.entries.Where(e => !e.Local).Select(e => $"{e.name}: model {e.vehicleModelId} {e.bike?.chassisPkg} {e.bike?.kitPkg}")));
                                Check(rt.Roster.entries.Where(e => !e.Local).All(e => e.bike != null && e.bike.machine && ReflexAtv.TypeOfModel(e.vehicleModelId) == vtype), $"every AI on a type {vtype} machine (the running game's AIVehicle -> VehicleType)");
                            }
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
                            if (opp && opp.Count > 0) Check(opp.Built == opp.Count && opp.Posed == opp.Count && opp.Riders.All(o => o.rider != null && o.rider.rider.HasOriginalPose), $"opponents built {opp.Built}, posed {opp.Posed} of {opp.Count}; their drivers posed by the game");
                            if (opp && opp.Count > 0) Check(opp.Riders.All(o => o.bike == null || o.bike.parts.Where(p => ReflexVehicles.HiddenAtStart(o.bike.hierarchyNodes[p.node])).All(p => !p.part.gameObject.activeSelf)), "the AI machines' damage panels hidden by their node flags");
                            Next();
                        }
                        else if (Age > 240) Fail("the gate did not drop");
                        break;
                    case 2: if (now - raceAt > 6.0) { PlayShot("03_race_6s"); Next(); } break;
                    case 3: if (now - raceAt > 12.0) { PlayShot("03_race_12s"); rt.cameraMode = 1; stage = 5; stageAt = now; } break;
                    case 5: if (Age > 1.5) { PlayShot("04_fpv"); rt.cameraMode = 2; Next(); } break;
                    case 6:
                        if (Age > 1.5)
                        {
                            PlayShot("05_follow"); rt.cameraMode = 0;
                            rt.TryGetVehicle(out var v);
                            Log($"race {race.state} place {race.place}/{race.riders} lap progress {race.lap_progress:0.000} speed {v.speed:0.0} (max {maxSpeed:0.0}) gear {v.gear} rpm {v.rpm:0} sim {rt.SimMilliseconds:0.00} ms");
                            Check(maxSpeed > 20 && race.place > 0, $"driving: top speed {maxSpeed:0.0} ft/s, place {race.place}/{race.riders}, lap progress {race.lap_progress:0.000}");
                            stage = 100; EditorApplication.ExitPlaymode();
                        }
                        break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        // ---------------------------------------------------------------------------------------------------- the front end's Powercross UTV (play mode)
        public static void FrontEndRace()
        {
            report.Clear(); bad = 0; shots = Shots(); saved = SavePrefs(); tag = "frontend_powercross_utv";
            ReflexProfile.Disabled = true;
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
                    case 3:                                                                                                                      // POWERCROSS (the 6th poster)
                    {
                        var es = fm.Layer("EventSelect.lyr")?.script as EventSelectScript;
                        if (Age > 2 && !fm.Busy && es != null && Age - lastNav > 0.8) { if (es.Focused < 5) { fm.Navigate(1, 0); lastNav = Age; } else Next(); }
                        else if (Age > 30) FeFail("event select: page " + fm.CurrentPage + ", focused " + (es != null ? es.Focused : -1));
                        break;
                    }
                    case 4: if (Age > 1.5) { fm.DispatchKey("Accept"); Next(); } break;
                    case 5:
                        if (Age > 20) FeFail("vehicle select not reached: page " + fm.CurrentPage);
                        else if (Age > 2 && !fm.Busy && fm.CurrentPage == "Vehicle Select Page")
                        {
                            Check(fm.CurrentPage == "Vehicle Select Page" && fm.Var("EventTypeId", -1) == 16, "vehicle select for Powercross (EventTypeId 16)");
                            Check(vs.riders.Count > 0 && vs.riders[0].id != -1, "no MY RIDES (the profile owns no machine); first pro " + (vs.riders.Count > 0 ? vs.riders[0].display : "-"));
                            Check(vs.types.Count == 4 && vs.types.Select(x => x.id).SequenceEqual(new[] { 11, 14, 12, 15 }) && vs.types.All(x => !x.locked), "types UTV, Pro Lite, Super Buggy, Pro 2 offered (VehicleSelect's sort 11, 14, 12, 15): " + string.Join(",", vs.types.Select(x => x.id + (x.locked ? "L" : ""))));
                            fm.DispatchKey("Accept"); Next();
                        }
                        break;
                    case 6: if (Age > 1.5) { Check(vs.types[vs.TypeIndex].id == 11, "vehicle type UTV"); PlayShot("10_vehicle_select_types"); fm.DispatchKey("Accept"); Next(); } break;
                    case 7: if (Age > 2) { Check(vs.vehicles.Count > 0 && ReflexVehicles.IsMachineModel(vs.vehicles[vs.VehicleIndex].modelId) && vs.garage != null && vs.garage.Shown.StartsWith("UTV", StringComparison.Ordinal), $"his UTV: AIVehicle {vs.vehicles[vs.VehicleIndex].id}, VehicleModel {vs.vehicles[vs.VehicleIndex].modelId}, garage shows {vs.garage?.Shown}"); GarageShot(vs.garage, "11_vehicle_select_utv_garage"); fm.DispatchKey("Accept"); Next(); } break;
                    case 8: if (Age > 2 && !fm.Busy) { Check(fm.CurrentPage == "Locations Page", "locations"); Next(); } break;
                    case 9: if (Age > 1) { fm.DispatchKey("Toggle"); fm.Navigate(0, 1); fm.Navigate(0, 1); Next(); } break;
                    case 10: if (Age > 1) { Check(fm.Var("VehicleClass", -1) == 22 && fm.Var("Player1VehicleTypeId", -1) == 11, "LocationSelect's vehicle class for the UTV: 22 (STR_VEHICLECLASS_UTV): " + fm.Var("VehicleClass", -1)); fm.DispatchKey("Toggle"); Next(); } break;
                    case 11: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;
                    case 12:
                    {
                        var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                        if (rt && rt.Running && !FrontEndLoadingOverlay.Current && Age > 3 && rt.TryGetVehicle(out _))
                        {
                            var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                            Check(sel && sel.Current.StartsWith("Powercross/PX_Laguna", StringComparison.Ordinal), "race scene event " + (sel ? sel.Current : "?"));
                            Check(rt.garage != null && rt.garage.vehicleTypeId == 11 && rt.garage.vehicleClass == 22, "race scene garage (VehicleSelect:End + LocationSelect's class): " + rt.garage);
                            CheckLocal(rt, 11, "front end");
                            Next();
                        }
                        else if (Age > 400) FeFail("race world not running after 400 s");
                        break;
                    }
                    case 13: if (Age > 3) { PlayShot("13_race_gate"); stage = 100; EditorApplication.ExitPlaymode(); } break;   // (QUIT to the front end: AtvTests.FrontEndRace)
                }
            }
            catch (Exception e) { FeFail("exception: " + e); }
        }
        static void FeFail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else { EditorApplication.update -= FeStep; End(); } }

        static void End()
        {
            EditorApplication.update -= Step;
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log($"vehicles race {tag}: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, tag + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
