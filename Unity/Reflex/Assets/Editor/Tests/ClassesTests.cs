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
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // Rival and Multi vehicle classes (WP-V2, _re/notes/vehicles.md "Rival and Multi classes"): 10 / 28 MX vs ATV, 11 MX vs MX Lite,
    // 12 ATV vs MX Lite, 23 Super Buggy vs Pro 2, 26 MX vs ATV Freestyle (Rival), 15 Moto, 16 Machines, 17 Unlimited (Multi).
    //   Verify: edit mode. ReflexEvents with the Rival / Multi formats raceable: every event keeps the default class (and world name) it had
    //           with Single classes only; every class of EventTypeVehicleClasses whose types are drawable is offered; LocationSelect's lists
    //           (ClassesFor, FeTables.VehicleClasses: the game's Lua table.sort order); the world names of the class choices (_c<class>).
    //   Race:   play mode. The race scene on $CLS_EVENT (National/NAT_Concord) in $CLS_CLASS (10) on $CLS_TYPE (4 ATV; -1 the profile's MX
    //           Lite) with $CLS_AI (2: Amateur, 11 AI): every rider's vehicle type is one of the class's (the running game's slot vehicle
    //           class = the factory of its AIVehicle's type), the riders' machines differ, every one is built and posed in Unity (bike /
    //           ATV / machine model, rider, plates), the engine projects of every type load; shots at the gate (game camera, the grid from
    //           behind and from the front) and in the race to $CLS_SHOTS. PlayerPrefs untouched.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.ClassesTests.Verify (-quit) / .Race (no -quit)
    public static class ClassesTests
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read__fn(IntPtr world, uint address, byte[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn> reflex_world_read__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn>("reflex_world_read");
        static int reflex_world_read(IntPtr world, uint address, byte[] output, uint size) => reflex_world_read__p.Get()(world, address, output, size);
        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage, vtype, vclass, ai; static string shots, tag; static bool modeEvent; static double stageAt, runningAt, raceAt; static float maxSpeed;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[CLS] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string Shots() { var s = Env("CLS_SHOTS", Path.Combine(Application.temporaryCachePath, "classes-shots")); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        // the vehicle classes of VehicleFactory_CreateVehicle 00542b90 by factory index (_re/notes/vehicles.md)
        static readonly Dictionary<uint, int> FactoryOfVtable = new Dictionary<uint, int> { [0x93cd84] = 0, [0x93d0ec] = 1, [0x93cee4] = 3, [0x93d2e4] = 5, [0x93d4d4] = 7 };
        static readonly string[] FactoryNames = { "MX", "ATV", "?", "TRUCK", "?", "BUGGY", "?", "UTV" };

        // ---------------------------------------------------------------------------------------------------- verify (edit mode)
        public static void Verify()
        {
            report.Clear(); bad = 0; shots = Shots(); saved = SavePrefs();
            try
            {
                var fmt = ReflexEvents.RaceableClassFormats.ToList();
                Check(fmt.Contains("Single") && fmt.Contains("Rival") && fmt.Contains("Multi"), "raceable class formats: " + string.Join(", ", fmt));
                // the events and their defaults as they were with Single classes only (the default worlds must not change)
                ReflexEvents.RaceableClassFormats.Clear(); ReflexEvents.RaceableClassFormats.Add("Single"); ReflexEvents.Refresh();
                var single = ReflexEvents.All.ToDictionary(e => e.key, e => (cls: e.spec.vehicleClass, w0: ReflexWorldFiles.For(e.key).name, w11: ReflexWorldFiles.For(e.key, 11, 95).name, classes: e.vehicleClasses));
                ReflexEvents.RaceableClassFormats.Clear(); ReflexEvents.RaceableClassFormats.UnionWith(fmt); ReflexEvents.Refresh();
                var all = ReflexEvents.All;
                Check(all.Count == single.Count && all.All(e => single.ContainsKey(e.key)), $"the same {single.Count} events (now {all.Count})");
                int same = all.Count(e => single.TryGetValue(e.key, out var s) && s.cls == e.spec.vehicleClass && s.w0 == ReflexWorldFiles.For(e.key).name && s.w11 == ReflexWorldFiles.For(e.key, 11, 95).name);
                Check(same == all.Count, $"every event keeps its default class and world names ({same}/{all.Count})");
                Check(all.All(e => single.TryGetValue(e.key, out var s) && e.vehicleClasses.Take(s.classes.Length).SequenceEqual(s.classes)), "the Single classes stay first, in their order");

                var t = ReflexEvents.Tables;
                var classTypes = t["VehicleClassType"].GroupBy(r => FeTables.I(r, "VehicleClassId")).ToDictionary(g => g.Key, g => g.Select(r => FeTables.I(r, "VehicleTypeId")).ToArray());
                var typeClasses = t["EventTypeVehicleClasses"].GroupBy(r => FeTables.I(r, "EventTypeId")).ToDictionary(g => g.Key, g => g.Select(r => FeTables.I(r, "VehicleClassId")).Distinct().ToArray());
                var rivals = new SortedSet<int>();
                foreach (var e in all)
                {
                    if (e.spec.vehicleClass < 0) continue;
                    var want = typeClasses.TryGetValue(e.spec.typeId, out var cs) ? cs.Where(c => classTypes.TryGetValue(c, out var ts) && ts.All(ReflexEvents.DrawableVehicleTypes.Contains)).OrderBy(c => c).ToArray() : new int[0];
                    var have = e.vehicleClasses.OrderBy(c => c).ToArray();
                    if (!have.SequenceEqual(want)) Check(false, $"{e.key}: classes [{string.Join(",", have)}], the tables' [{string.Join(",", want)}]");
                    foreach (int c in e.vehicleClasses) if (ReflexEvents.ClassFormat(c) != "Single") rivals.Add(c);
                }
                Log("events: " + string.Join("; ", all.Where(e => e.spec.vehicleClass >= 0).GroupBy(e => e.spec.type).Select(g => g.Key + " [" + string.Join(",", g.First().vehicleClasses) + "] default " + g.First().spec.vehicleClass)));
                Check(new[] { 10, 11, 12, 15, 16, 17, 23, 26 }.All(rivals.Contains), "every Rival / Multi class of the event types is offered: " + string.Join(",", rivals.Select(c => c + " " + ReflexEvents.ClassName(c)?.Replace("STR_VEHICLECLASS_", ""))));

                var nat = ReflexEvents.Find("National/NAT_Concord");
                var atvList = nat.ClassesFor(ReflexAtv.VehicleTypeAtv);
                var feList = FeTables.Shared.VehicleClasses(10, ReflexAtv.VehicleTypeAtv).Select(c => c.id).ToArray();
                Check(atvList.SequenceEqual(new[] { 1, 12, 10, 15 }) && feList.SequenceEqual(atvList), $"LocationSelect for an ATV at a National (Lua table.sort): [{string.Join(",", atvList)}], front end [{string.Join(",", feList)}]");
                Check(nat.ClassesFor(2).SequenceEqual(new[] { 3, 12, 11, 15 }) && nat.ClassesFor(3).First() == 2, $"MX Lite [{string.Join(",", nat.ClassesFor(2))}], MX [{string.Join(",", nat.ClassesFor(3))}]");
                var trx = ReflexEvents.Find("Terracross/TRX_Halifax"); var px = ReflexEvents.Find("Powercross/PX_Laguna"); var fmx = ReflexEvents.Find("Freestyle/FMX_BuenaVista");
                Log($"Terracross UTV [{string.Join(",", trx.ClassesFor(11))}], Powercross Super Buggy [{string.Join(",", px.ClassesFor(12))}], Freestyle MX [{string.Join(",", fmx.ClassesFor(3))}]");
                Check(trx.ClassesFor(11).SequenceEqual(new[] { 22, 16, 17 }) && px.ClassesFor(12).SequenceEqual(new[] { 19, 23, 16 }) && fmx.ClassesFor(3).SequenceEqual(new[] { 25, 26 }), "machine and Freestyle class lists");

                foreach (var (key, type, cls) in new[] { ("National/NAT_Concord", 4, 10), ("National/NAT_Concord", 3, 11), ("Supercross/SX_Kingston", 4, 12), ("National/NAT_Concord", 4, 15),
                                                         ("Powercross/PX_Laguna", 11, 16), ("Terracross/TRX_Halifax", 11, 17), ("Powercross/PX_Laguna", 12, 23), ("Freestyle/FMX_BuenaVista", 3, 26) })
                {
                    var e = ReflexEvents.Find(key); var g = e?.GarageFor(type, cls);
                    var w = g != null ? ReflexWorldFiles.For(key, 11, 95, 0, g) : null;
                    Check(g != null && g.vehicleClass == cls && w.name.EndsWith("_c" + cls), $"{key} class {cls} {ReflexEvents.ClassFormat(cls)} on type {type}: {g} -> world {w?.name}");
                }
                var own = new ReflexWorldFiles.Garage { vehicleClass = 15 };
                Check(ReflexWorldFiles.For("National/NAT_Concord", 11, 95, 0, own).name == "National_NAT_Concord_ai11_c15", "the profile's MX Lite in Moto: " + ReflexWorldFiles.For("National/NAT_Concord", 11, 95, 0, own).name);
                Check(nat.GarageFor(2, 10) == null && nat.GarageFor(4, 11) == null, "no garage for a class without the type (MX Lite in MX vs ATV, ATV in MX vs MX Lite)");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            ReflexEvents.Refresh();
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log("classes verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "classes-verify-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------------- race (play mode)
        public static void Race()
        {
            report.Clear(); bad = 0; shots = Shots(); saved = SavePrefs();
            string key = Env("CLS_EVENT", "National/NAT_Concord");
            vtype = int.Parse(Env("CLS_TYPE", "4")); vclass = int.Parse(Env("CLS_CLASS", "10")); ai = int.Parse(Env("CLS_AI", "2"));
            tag = Env("CLS_TAG", "c" + vclass + "_t" + vtype + "_" + key.Replace('/', '_'));
            var ev = ReflexEvents.Find(key);
            var g = ev == null ? null : vtype < 0 ? new ReflexWorldFiles.Garage { vehicleClass = vclass } : ev.GarageFor(vtype, vclass);
            if (g == null || (vtype < 0 && !ev.ClassesFor(ReflexEvents.DefaultVehicleType).Contains(vclass))) { Check(false, $"no event {key} with class {vclass} for type {vtype}"); End(); return; }
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = g;
            modeEvent = ev.spec.type == "Freestyle" || ev.spec.type == "SectionRace";   // the game's live UI: its RulesPopup takes Enter
            Log($"race {key} class {vclass} {ReflexEvents.ClassName(vclass)} ({ReflexEvents.ClassFormat(vclass)}: types {string.Join(",", ReflexAtv.TypesOfClass(vclass))}) AI {ai} on type {vtype}: {g} -> world {ReflexWorldFiles.For(key, ai > 0 ? 11 : 0, ai > 0 ? ReflexWorldFiles.AiSkills[ai - 1].skill : 0, 0, g).name}");
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            stage = 0; stageAt = EditorApplication.timeSinceStartup; runningAt = raceAt = -1; maxSpeed = 0;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static IntPtr World(OriginalWorldRuntime rt) => (IntPtr)typeof(OriginalWorldRuntime).GetField("world", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static ReflexBikeModel LocalModel(OriginalWorldRuntime rt) => (ReflexBikeModel)typeof(OriginalWorldRuntime).GetField("bikeModel", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static uint U32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToUInt32(b, 0); }

        static string Kind(BikeSpec b) => b == null ? "none" : b.machine ? "machine" : b.atv ? "ATV" : "bike";
        static string KindOfType(int type) { int f = ReflexVehicles.FactoryOfType(type); return f == 0 ? "bike" : f == 1 ? "ATV" : ReflexVehicles.IsMachineFactory(f) ? "machine" : "?"; }

        /// Every rider: the running game's slot vehicle (vtable -> factory), the tables' type of its vehicle model, the Unity spec.
        static void CheckRoster(OriginalWorldRuntime rt)
        {
            var roster = rt.Roster; var w = World(rt);
            var types = ReflexAtv.TypesOfClass(vclass);
            int localType = vtype < 0 ? ReflexEvents.DefaultVehicleType : vtype;
            var seen = new List<int>();
            int good = 0;
            foreach (var e in roster.entries)
            {
                int type = ReflexAtv.TypeOfModel(e.vehicleModelId);
                uint vt = e.slot.vehicle != 0 ? U32(w, e.slot.vehicle) : 0;
                int gameFactory = FactoryOfVtable.TryGetValue(vt, out int f) ? f : -1;
                int tableFactory = ReflexVehicles.FactoryOfType(type);
                bool ok = (e.Local ? type == localType : types.Contains(type)) && gameFactory == tableFactory && Kind(e.bike) == KindOfType(type) && e.plate != null;
                if (ok) good++; else Log($"  mismatch: {e}");
                seen.Add(type);
                Log($"  slot {e.slot.slot,2} {(e.Local ? "local" : "AI   ")} {e.name,-20} type {type,2} ({ReflexVehicles.FactoryOfType(type)}) game vehicle {vt:x8} = {(gameFactory >= 0 ? FactoryNames[gameFactory] : "?")} -> {Kind(e.bike)} {e.bike?.chassisPkg} {e.bike?.kitPkg} on {e.bike?.hierarchyModel}");
            }
            Check(good == roster.entries.Count, $"every rider ({good}/{roster.entries.Count}) rides a type of class {vclass} (the local one type {localType}); the running game's vehicle class matches the tables' factory of its model; drawn as that kind; plates read");
            var distinct = seen.Distinct().OrderBy(x => x).ToList();
            Log("vehicle types in the race: " + string.Join(", ", distinct.Select(x => x + " x" + seen.Count(y => y == x))));
            if (ai > 0 && types.Length > 1) Check(distinct.Count >= 2, $"mixed machines: {distinct.Count} types of the class's {types.Length}");
            var audio = UnityEngine.Object.FindAnyObjectByType<OriginalAudio>();
            var want = roster.entries.Select(e => ReflexAudioSet.EngineProjectOfModel(e.vehicleModelId)).Where(p => p != null).Distinct().ToList();
            Log("engine projects: " + string.Join(", ", want) + "; the race's: " + string.Join(", ", audio ? audio.RaceEngines : new string[0]));
            Check(audio && want.All(audio.RaceEngines.Contains), "the race loads every rider's engine project (OriginalAudio roster hook)");
        }

        static Bounds? BoundsOf(GameObject g)
        {
            if (!g) return null;
            Bounds? b = null;
            foreach (var r in g.GetComponentsInChildren<Renderer>()) { if (!r.enabled || !r.gameObject.activeInHierarchy) continue; if (b == null) b = r.bounds; else { var x = b.Value; x.Encapsulate(r.bounds); b = x; } }
            return b;
        }
        /// The riders' machines as built: (position, bounds) of the local vehicle and every opponent.
        static List<Vector3> RiderPositions(OriginalWorldRuntime rt)
        {
            var l = new List<Vector3>();
            var m = LocalModel(rt); var lb = BoundsOf(m?.chassis); if (lb != null) l.Add(lb.Value.center);
            if (rt.opponentRiders) foreach (var o in rt.opponentRiders.Riders) { var b = BoundsOf(o.bike?.chassis); if (b != null && o.posed) l.Add(b.Value.center); }
            return l;
        }

        static void PlayShot(string name, Func<Camera, bool> place = null)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false;
            var target = new RenderTexture(1920, 1080, 24) { antiAliasing = 4 };
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            var main = rt?.chaseCamera ?? Camera.main;
            if (main) c.CopyFrom(main);
            if (place != null && !place(c)) { UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release(); return; }
            c.targetTexture = target; c.enabled = false;
            var overlays = place != null ? new List<Canvas>() : UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1920, 1080, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1920, 1080), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, tag + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + tag + "_" + name);
        }
        /// A camera over the riders: behind (side -1) or in front of (+1) the grid along the race direction (the game camera's heading).
        static Func<Camera, bool> GridCamera(OriginalWorldRuntime rt, float side, float height = 0.45f)
        {
            return c =>
            {
                var ps = RiderPositions(rt); if (ps.Count == 0) return false;
                // an arena event (Freestyle: the riders spread over the whole arena): the local rider and the three nearest to him
                if (ps.Count > 4 && ps.Max(p => (p - ps[0]).magnitude) > 150f) ps = ps.OrderBy(p => (p - ps[0]).sqrMagnitude).Take(4).ToList();
                var centre = ps.Aggregate(Vector3.zero, (a, p) => a + p) / ps.Count;
                float spread = Mathf.Max(8f, ps.Max(p => (p - centre).magnitude));
                var fwd = Vector3.ProjectOnPlane((rt.chaseCamera ? rt.chaseCamera.transform : c.transform).forward, Vector3.up); if (fwd.sqrMagnitude < 1e-4f) fwd = Vector3.forward; fwd.Normalize();
                c.transform.position = centre + fwd * side * (spread * 1.05f + 10f) + Vector3.up * (spread * height + 5f);
                c.transform.LookAt(centre, Vector3.up);
                c.fieldOfView = 55f; c.nearClipPlane = 0.5f;
                return true;
            };
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
                bool enter = modeEvent && raceAt < 0 && runningAt >= 0 && now - runningAt > 6 && (now - runningAt) % 2.0 < 0.15;   // RulesPopup / EventIntro accept
                if (rt) rt.testHeldKeys = raceAt >= 0 ? new[] { 0x11 } : enter ? new[] { 0x1c } : Array.Empty<int>();
                if (rt && raceAt >= 0 && rt.TryGetVehicle(out var cur)) maxSpeed = Mathf.Max(maxSpeed, cur.speed);
                switch (stage)
                {
                    case 0:
                        if (rt && rt.Running && now - runningAt > 4 && rt.Roster != null)
                        {
                            Check(rt.garage != null && rt.garage.vehicleClass == vclass, "race scene class: " + rt.garage);
                            CheckRoster(rt);
                            Next();
                        }
                        else if (Age > 480) Fail("world not running after 480 s");
                        break;
                    case 1:   // every opponent built (one per frame) and posed by the game
                    {
                        var opp = rt.opponentRiders;
                        int want = ai > 0 ? ReflexWorldFiles.FullGateOpponents : 0;
                        if (opp && opp.Count == want && opp.Built == want && opp.Posed == want && Age > 2)
                        {
                            Check(opp.Riders.All(o => o.bike != null && o.rider != null && o.rider.rider.HasOriginalPose), $"opponents built {opp.Built}, posed {opp.Posed} of {opp.Count}; their riders posed by the game (69 bones)");
                            Check(rt.bikePose && rt.bikePose.HasPose && rt.rider && rt.rider.HasOriginalPose, "the local vehicle and rider posed by the game");
                            PlayShot("01_gate");
                            PlayShot("02_grid_back", GridCamera(rt, -1f));
                            PlayShot("03_grid_front", GridCamera(rt, 1f, 0.3f));
                            Next();
                        }
                        else if (Age > 120) Fail($"opponents {opp?.Count} built {opp?.Built} posed {opp?.Posed} (want {want})");
                        break;
                    }
                    case 2:
                        if (raceAt >= 0 && now - raceAt > 2.5) { PlayShot("04_race_2s"); PlayShot("05_pack_2s", GridCamera(rt, -1f, 0.6f)); Next(); }
                        else if (Age > 300) Fail("the race did not start");
                        break;
                    case 3: if (now - raceAt > 7.0) { PlayShot("06_race_7s"); PlayShot("07_pack_7s", GridCamera(rt, -1f, 0.8f)); Next(); } break;
                    case 4:
                        if (now - raceAt > 10.0)
                        {
                            rt.TryGetVehicle(out var v);
                            Log($"race {race.state} place {race.place}/{race.riders} speed {v.speed:0.0} (max {maxSpeed:0.0}) sim {rt.SimMilliseconds:0.00} ms");
                            Check(race.state >= 2 && maxSpeed > 15, $"the race runs: state {race.state}, top speed {maxSpeed:0.0} ft/s");
                            var opp = rt.opponentRiders;
                            Check(opp == null || opp.Posed == opp.Count, $"all opponents still posed ({opp?.Posed}/{opp?.Count})");
                            stage = 100; EditorApplication.ExitPlaymode();
                        }
                        break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log($"classes race {tag}: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, tag + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
// MOTOCAREER series of the types the audit marked unverified, end to end in play mode from an existing career save (CareerTests.Create
    // made it): Freestyle, Powercross, Section Race and the car-class series. $CS_SERIES (68), $CS_ASSET (a vehicle's StoreAssetId the
    // profile buys first when it owns none of the series' types, through the game's PurchaseStoreAsset; -1 none), then the equipped vehicle
    // (MXTableHelper:SetPlayerFavoriteVehicle) and the save; MOTOCAREER -> Race and Ride -> the events page -> the series (as
    // OnAcceptSeries: the valid-type check, StartRacePopup) -> the career world -> rules popup -> the race with the autopilot -> the post
    // pages' CONTINUE (the game completes the event and saves) -> rewards -> the events page: PlayerSeriesStatus / credits / unlocks of the
    // re-read save. PNGs to $CLS_SHOTS (career_s<series>_*). The bench's own productName keeps its save apart; PlayerPrefs untouched.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.ClassesCareerTests.Series (no -quit)
    public static class ClassesCareerTests
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read__fn(IntPtr world, uint address, byte[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn> reflex_world_read__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn>("reflex_world_read");
        static int reflex_world_read(IntPtr world, uint address, byte[] output, uint size) => reflex_world_read__p.Get()(world, address, output, size);
        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage, series, asset, balanceBefore, statusRowsBefore, unlockedBefore, profileVersion; static double stageAt, started; static string shots, tag;
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static void Log(string s) { report.AppendLine(s); Debug.Log("[CLSC] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));

        public static void Series()
        {
            report.Clear(); bad = 0; stage = 0; saved = SavePrefs(); ReflexProfile.Disabled = false;
            series = int.Parse(Env("CS_SERIES", "68")); asset = int.Parse(Env("CS_ASSET", "-1"));
            shots = Env("CLS_SHOTS", Path.Combine(Application.temporaryCachePath, "classes-shots")); Directory.CreateDirectory(shots); tag = "career_s" + series;
            Log("save " + ReflexProfile.SaveFile + (ReflexProfile.HasSave ? " (" + new FileInfo(ReflexProfile.SaveFile).Length + " bytes)" : " (none: run CareerTests.Create first)"));
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (fm) fm.rememberChoice = false;
            stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static bool Idle(FrontEndManager fm) => fm && fm.assets != null && !fm.Busy;
        static Dictionary<string, object> Row(string table, string col, int v) => FeTables.Shared.Rows(table).FirstOrDefault(r => FeTables.I(r, col) == v);
        static int[] ValidTypes => FeTables.Shared.Rows("VehicleClassType").Where(r => FeTables.I(r, "VehicleClassId") == FeTables.I(Row("Series", "SeriesId", series), "VehicleClassId")).Select(r => FeTables.I(r, "VehicleTypeId")).ToArray();
        static OriginalCareerRace Career() => UnityEngine.Object.FindAnyObjectByType<OriginalCareerRace>();
        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => rt ? (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt) : default;

        static void Shot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>(); var main = rt && rt.chaseCamera ? rt.chaseCamera : Camera.main;
            if (main) { c.CopyFrom(main); c.targetTexture = target; c.enabled = false; c.cullingMask = main.cullingMask | 1; }
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, tag + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + tag + "_" + name);
        }

        static string Describe()
        {
            var p = ReflexProfile.Current; if (p == null) return "no profile";
            var st = p.T("PlayerSeriesStatus");
            return $"credits {p.T("PlayerRider").Int("AccountBalance", 0, 0)}, equipped vehicle type {FeCareerScripts.PlayerVehicleTypeId()}, series status rows {st.rows}, unlocked products {p.T("PlayerProductsUnlocked").rows}, completion {p.careerCompletion}";
        }

        static bool liveEnter, rulesShot; static double lastLog, raceShotAt;
        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > 2400) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var cr = Career();
                switch (stage)
                {
                    case 0: if (Idle(fm) && fm.CurrentPage == "Title Page" && Age > 2) { fm.DispatchKey("Accept"); Next(); } else if (Age > 60) Fail("no title page"); break;
                    case 1:
                        if (Idle(fm) && fm.CurrentPage == "Main Menu Page" && Age > 1.5 && !ReflexProfile.Busy && ReflexProfile.Current != null)
                        {
                            Log("profile: " + Describe());
                            // the vehicle: one the profile owns of the series' types, else the bought one (the garage's purchase)
                            var p = ReflexProfile.Current; var pv = p.T("PlayerVehicle"); var valid = ValidTypes;
                            int owned = Enumerable.Range(0, pv.rows).Where(i => valid.Contains(FeTables.I(Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", i)), "VehicleTypeId"))).Select(i => (int?)i).FirstOrDefault() ?? -1;
                            var cmds = new List<string>();
                            if (owned < 0 && asset >= 0) cmds.Add("purchase_asset " + asset);
                            Log($"series {series}: class {FeTables.I(Row("Series", "SeriesId", series), "VehicleClassId")} types [{string.Join(",", valid)}], owned row {owned}" + (cmds.Count > 0 ? ", buying asset " + asset : ""));
                            profileVersion = ReflexProfile.Version;
                            if (cmds.Count > 0) ReflexProfile.Run(cmds.ToArray());
                            Next();
                        }
                        else if (Age > 120) Fail("no main menu: " + fm?.CurrentPage + " " + ReflexProfile.Error);
                        break;
                    case 2:
                        if (!ReflexProfile.Busy && Age > 1)
                        {
                            var p = ReflexProfile.Current; var pv = p.T("PlayerVehicle"); var valid = ValidTypes;
                            int row = Enumerable.Range(0, pv.rows).Where(i => valid.Contains(FeTables.I(Row("VehicleModel", "VehicleModelId", pv.Int("VehicleModelId", i)), "VehicleTypeId"))).Select(i => (int?)i).LastOrDefault() ?? -1;
                            if (row < 0) { Fail("the profile owns no vehicle of the series' types"); break; }
                            ReflexProfile.Run("favorite_vehicle " + pv.Int("PlayerVehicleId", row), "save");
                            Log("equipped PlayerVehicle " + pv.Int("PlayerVehicleId", row) + " (model " + pv.Int("VehicleModelId", row) + ")");
                            Next();
                        }
                        break;
                    case 3:
                        if (!ReflexProfile.Busy && Age > 1 && Idle(fm))
                        {
                            Check(ValidTypes.Contains(FeCareerScripts.PlayerVehicleTypeId()), "the equipped vehicle's type " + FeCareerScripts.PlayerVehicleTypeId() + " is one of the series'");
                            balanceBefore = ReflexProfile.Current.T("PlayerRider").Int("AccountBalance", 0, 0); statusRowsBefore = ReflexProfile.Current.T("PlayerSeriesStatus").rows;
                            unlockedBefore = ReflexProfile.Current.T("PlayerProductsUnlocked").rows;
                            Log("before: " + Describe());
                            fm.DispatchKey("Accept"); Next();   // MOTOCAREER
                        }
                        break;
                    case 4: if (Idle(fm) && fm.CurrentPage == "Race and Ride Page" && Age > 2 && !ReflexProfile.Busy) { fm.DispatchKey("Accept"); Next(); } else if (Age > 40) Fail("no Race and Ride page: " + fm?.CurrentPage); break;
                    case 5:
                        if (Idle(fm) && fm.CurrentPage == "Race and Ride Events" && Age > 2)
                        {
                            Shot("01_events");
                            var ev = ReflexCareer.EventOf(series, FeCareerScripts.Lookup);
                            if (ev == null) { Fail("series " + series + " has no event"); break; }
                            int type = FeCareerScripts.PlayerVehicleTypeId();
                            // RaceAndRideEvents:OnAcceptSeries for this series (the page lists only its group's; the group's parent product)
                            int parent = FeTables.I(Row("StoreProducts", "StoreProductId", FeTables.I(Row("Series", "SeriesId", series), "StoreProductId")), "ParentStoreProductId");
                            fm.vars["CareerSeries"] = new ReflexCareer.Race { seriesId = series, vehicleTypeId = type, parentStoreProductId = parent, eventKey = ev.key };
                            string format = FeTables.S(Row("Series", "SeriesId", series), "SeriesFormat");
                            Log($"series {series} ({format}, {ev.key}) on vehicle type {type}");
                            liveEnter = ReflexModes.IsMode(ev);
                            fm.SendUIEvent(format == "Heat" || format == "Moto" ? "ShowPracticePopup" : "ShowStartRacePopup"); Next();
                        }
                        else if (Age > 40) Fail("no events page: " + fm?.CurrentPage);
                        break;
                    case 6:
                        if ((fm.TopPopup == "Start Race Popup" || fm.TopPopup == "Practice Popup") && Age > 1.5) { Shot("02_start_popup"); fm.DispatchKey("Accept"); Next(); }   // START (practice: PRACTICE)
                        else if (Age > 10) Fail("no start race popup: " + fm.TopPopup);
                        break;
                    case 7:
                        if (rt && rt.Running && Age > 3) { Log("career world running: " + ReflexCareer.Current); Next(); }
                        else if (Age > 600) Fail("career world not running: " + rt?.statusLabel?.text);
                        break;
                    case 8:   // before the race: the rules popup (the port's, OriginalCareerRace) or the game's own (a mode's live UI) -> accept
                    {
                        var race = RaceState(rt);
                        var live = OriginalLiveUi.Active;
                        if (cr && cr.RulesOpen && Age > 2) { Shot("03_rules"); Check(live == null, "one rules popup (the port's; no live UI)"); cr.Rules.Finish(false); Log("rules popup accepted (port)"); }
                        if (live != null && !rulesShot && live.Pages.Contains("RaceRulesPopup") && Age > 2) { rulesShot = true; Shot("03_rules_live"); Check(!(cr && cr.RulesOpen), "one rules popup (the game's own, live)"); }
                        bool enter = liveEnter && race.state < 1 && Age > 4 && Age % 2.0 < 0.15;
                        rt.testHeldKeys = enter ? new[] { 0x1c } : Array.Empty<int>();

                        if (race.state >= 1) { Log("the race left Pre (live UI " + (live != null ? "route " + live.Route + ", pages " + string.Join(", ", live.Pages) : "none") + ")"); Next(); }
                        else if (Age > 90) Fail("the race stays in Pre (rules hold): live " + (live != null ? string.Join(", ", live.Pages) : "none"));
                        break;
                    }
                    case 9:
                    {
                        var race = RaceState(rt);
                        rt.testHeldKeys = race.state == 2 ? Steer(rt) : Array.Empty<int>();
                        if (EditorApplication.timeSinceStartup - lastLog > 15) { lastLog = EditorApplication.timeSinceStartup; Log($"race {race.state} place {race.place}/{race.riders} lap {race.laps_completed}/{race.num_laps} time {race.race_time:F1}"); }
                        if (race.state == 2 && raceShotAt == 0 && Age > 20) { raceShotAt = Age; Shot("04_racing"); }
                        if (race.state >= 4) { rt.testHeldKeys = Array.Empty<int>(); Log("race over: place " + race.place + " of " + race.riders); Next(); }
                        else if (Age > 1200) Fail("the race did not end");
                        break;
                    }
                    case 10:
                        if (Age < 3) break;
                        if (cr == null || cr.Post == null || !cr.Post.IsOpen) { if (Age > 40) Fail("career post-race pages not open (live " + (OriginalLiveUi.Active != null ? string.Join(", ", OriginalLiveUi.Active.Pages) : "none") + ")"); break; }
                        Shot("05_post_race"); profileVersion = ReflexProfile.Version;
                        cr.Continue();
                        Check(ReflexCareer.LastResult != null && ReflexCareer.LastResult.Contains("\"saved\":0"), "the game completed the career event and saved: " + ReflexCareer.LastResult?.Substring(0, Math.Min(400, ReflexCareer.LastResult?.Length ?? 0)));
                        Next();
                        break;
                    case 11:
                        if (cr && cr.Post != null && cr.Post.IsOpen && cr.Post.PageName == "Rewards" && Age > 2) { Shot("06_rewards"); cr.Post.AcceptPage(); Next(); }
                        else if (Age > 6) { Log("no rewards page"); Next(); }
                        break;
                    case 12:
                        if (Idle(fm) && fm.CurrentPage == "Race and Ride Events" && ReflexProfile.Version != profileVersion && !ReflexProfile.Busy && Age > 2)
                        {
                            Shot("07_events_after");
                            var p = ReflexProfile.Current; var st = p.T("PlayerSeriesStatus"); int r = st.Find("SeriesId", series);
                            Log("after: " + Describe());
                            Log("PlayerSeriesStatus: " + string.Join("; ", Enumerable.Range(0, st.rows).Select(i => $"series {st.Int("SeriesId", i)} status {st.Int("SeriesStatus", i)} best {st.Int("BestPlaceFinish", i)} gold {st.Int("GoldMedals", i)} silver {st.Int("SilverMedals", i)} bronze {st.Int("BronzeMedals", i)}")));
                            Check(r >= 0, $"the series' result is in the re-read save (series {series} best place {(r >= 0 ? st.Int("BestPlaceFinish", r) : -1)})");
                            int bal = p.T("PlayerRider").Int("AccountBalance", 0, 0);
                            Log($"credits {balanceBefore} -> {bal}, unlocked products {unlockedBefore} -> {p.T("PlayerProductsUnlocked").rows}");
                            Check(bal >= balanceBefore, "credits not lost");
                            stage = 100; EditorApplication.ExitPlaymode();
                        }
                        else if (Age > 120) Fail("not back on the events page: " + fm?.CurrentPage + " v" + ReflexProfile.Version);
                        break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static uint U32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToUInt32(b, 0); }
        static float F32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToSingle(b, 0); }
        static double slowSince, reverseUntil;
        // CareerTests' autopilot (the race line ~45 units ahead, or a waypoint race's next gate; reverse when stuck)
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
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log($"classes career {tag}: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, tag + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
