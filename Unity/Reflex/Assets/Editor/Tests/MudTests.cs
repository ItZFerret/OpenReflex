using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // Mud build-up (Content/ReflexMud.cs, _re/notes/mud.md).
    //   Verify: edit mode. Every vehicle / rider pixel shader of the install that declares usr_mud_amount analysed for its mud term
    //           (constants, noise sampler and tiling, mask sampler / channel / scale) and compared with the reference table of the
    //           d3d9_shader.py decoder ($MUD_PY_TSV, scratchpad mud/mud_analyze.py); the default bike, rider and an ATV built with the
    //           term on every material whose shader has it; garage renders at forced amounts 0 / 0.35 / 0.7 / 1 to $MUD_OUT/shots.
    //   Race:   play mode. $MUD_EVENT (National/NAT_Concord) with $MUD_AI (2: Amateur, 11 AI; 0 none): the game's records drive every
    //           rider's materials; throttle + race line from the gate drop for $MUD_SECONDS; screenshots at the gate (clean) and
    //           during the race. PlayerPrefs: nothing written (ReflexEventSelect.Pending*); the user's keys are checked.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.MudTests.Verify (-quit) / .Race (no -quit)
    public static class MudTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[MUD] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string shots;
        static string Shots() { var s = Path.Combine(Env("MUD_OUT", Path.Combine(Application.temporaryCachePath, "mud")), "shots"); Directory.CreateDirectory(s); return s; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        static readonly string[] Prefixes = { "MX", "ATV", "Pro", "SB", "UTV", "Rider", "HR_" };

        // ---------------------------------------------------------------------------------------------------- verify (edit mode)
        public static void Verify()
        {
            report.Clear(); bad = 0; shots = Shots();
            var before = SavePrefs();
            bool async = ShaderUtil.allowAsyncCompilation; ShaderUtil.allowAsyncCompilation = false;
            try
            {
                // 1. the shaders' mud terms against the Python decoder's
                var mine = ReflexMud.AnalyzeInstall(Prefixes);
                var withMud = mine.Values.Where(s => s != null).OrderBy(s => s.shader, StringComparer.Ordinal).ToList();
                Log($"shaders: {mine.Count} distinct, {withMud.Count} declare usr_mud_amount, {withMud.Count(s => s.Usable)} with the blend found");
                foreach (var s in withMud) Log("  " + s);
                string py = Env("MUD_PY_TSV", "");
                if (File.Exists(py))
                {
                    var rows = File.ReadAllLines(py).Where(l => l.Contains('\t')).Select(l => l.Split('\t')).ToList();
                    int same = 0; var diffs = new List<string>();
                    foreach (var r in rows)
                    {
                        var s = withMud.FirstOrDefault(x => x.shader == r[0]);
                        bool ok = s != null && Same(r, s);
                        if (ok) same++; else diffs.Add(r[0] + (s == null ? " (missing)" : $": py [{string.Join(" ", r.Skip(1))}] cs [{string.Join(" ", s.ToString().Split('\t').Skip(1))}]"));
                    }
                    Check(rows.Count == withMud.Count && same == rows.Count, $"mud terms equal to the d3d9_shader.py reference: {same}/{rows.Count} shaders (C# {withMud.Count})" + (diffs.Count > 0 ? ": " + string.Join(" | ", diffs.Take(8)) : ""));
                }
                else Log("(no $MUD_PY_TSV: the Python comparison skipped)");
                // the FNL shaders the game's vehicles and gear use all resolve
                var fnl = withMud.Where(s => s.shader.EndsWith("FNL", StringComparison.OrdinalIgnoreCase) || s.shader.EndsWith("_Fnl", StringComparison.Ordinal)).ToList();
                Check(fnl.All(s => s.Usable), $"every *_FNL shader with usr_mud_amount has the blend ({fnl.Count(s => s.Usable)}/{fnl.Count}); unresolved (HR / legacy): {string.Join(", ", withMud.Where(s => !s.Usable).Select(s => s.shader))}");

                // 2. models: the term on every material whose shader has it
                var t = new ReflexGameTables(new Reflex.Port.FrontEnd.FeTableSource(Reflex.Port.FrontEnd.FeTables.Shared));
                var bike = ReflexBikeModel.Build(BikeSpec.MarsFLite);
                var rider = ReflexRiderModel.Build(RiderSpec.AnswerRed());
                var fe = Reflex.Port.FrontEnd.FeTables.Shared;
                var atvRow = fe.Rows("AIRiderVehicle").First(r => Reflex.Port.FrontEnd.FeTables.I(r, "VehicleTypeId") == ReflexAtv.VehicleTypeAtv);
                int vid = Reflex.Port.FrontEnd.FeTables.I(atvRow, "AIVehicleId");
                int model = Reflex.Port.FrontEnd.FeTables.I(fe.Rows("AIVehicle").First(x => Reflex.Port.FrontEnd.FeTables.I(x, "AIVehicleId") == vid), "VehicleModelId");
                var atvSpec = ReflexVehicleSpecs.Bike(t, model, t.Where("AIVehicleParts", "AIVehicleId", vid).Select(i => t.Int("AIVehicleParts", "VehiclePartId", i)).ToList());
                var atv = ReflexBikeModel.Build(atvSpec);
                foreach (var (name, roots) in new[] { ("MarsF Lite", BikeRoots(bike)), ("Answer rider", new[] { rider.root }), ("ATV " + atvSpec.chassisPkg + " " + atvSpec.kitPkg, BikeRoots(atv)) })
                {
                    var mats = roots.SelectMany(g => g.GetComponentsInChildren<Renderer>(true)).SelectMany(r => r.sharedMaterials).Where(m => m).Distinct().ToList();
                    var on = mats.Where(m => ReflexObjectShading.Declares(m, "usr_mud_amount")).ToList();   // Reflex/Vehicle: the program's own mud term
                    Log($"{name}: {mats.Count} materials, mud on {on.Count}: " + string.Join(", ", on.Select(m => $"{m.name.Split(new[] { " -> " }, StringSplitOptions.None).Last()} [{ReflexObjectShading.ProgramName(m)}]")));
                    Check(on.Count > 0, $"{name}: the mud term on {on.Count} materials");
                }
                // 3. renders at forced amounts (the shader's blend; the race run shows the game's own amounts)
                var colour = new Vector4(62 / 255f, 54 / 255f, 47 / 255f, 1);   // NAT_Concord's MudManager MudR/G/B/A
                foreach (float amount in new[] { 0f, 0.35f, 0.7f, 1f })
                {
                    var s = new ReflexMudState { record = 0, amount = Vector4.one * amount, color = colour };
                    ReflexMud.Apply(bike, rider, s); ReflexMud.Apply(atv, null, s);
                    Render(new[] { bike.chassis }, $"garage_mx_mud{amount * 100:000}");   // (the rider unposed has no shape: see the race shots)
                    Render(new[] { atv.chassis }, $"garage_atv_mud{amount * 100:000}");
                }
                bike.Destroy(); rider.Destroy(); atv.Destroy();
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally { ShaderUtil.allowAsyncCompilation = async; }
            Check(SavePrefs().All(kv => kv.Value.Equals(before[kv.Key])), "user PlayerPrefs untouched");
            Log("mud verify: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "..", "verify-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        static char Channel(Vector4 v) { for (int i = 0; i < 4; i++) if (v[i] > 0.5f) return "xyzw"[i]; return '-'; }
        static GameObject[] BikeRoots(ReflexBikeModel m) => new[] { m.chassis, m.frontWheel ? m.frontWheel.gameObject : null, m.rearWheel ? m.rearWheel.gameObject : null }.Where(g => g).ToArray();

        static bool Same(string[] py, MudShaderInfo s)
        {
            bool F(string a, float b) => a == "-" ? false : Math.Abs(float.Parse(a, CultureInfo.InvariantCulture) - b) < 1e-5f;
            int R(string a) => a == "-" ? -1 : int.Parse(a.Substring(1), CultureInfo.InvariantCulture);
            if (R(py[1]) != s.amountReg || R(py[2]) != s.colorReg || R(py[3]) != s.wetnessReg || R(py[4]) != s.noiseSampler || R(py[6]) != s.maskSampler) return false;
            if (s.noiseSampler >= 0 && !F(py[5], s.tile)) return false;
            if (s.maskSampler >= 0 && (py[7] != "xyzw"[s.maskChannel].ToString() || !F(py[8], s.maskScale))) return false;
            return true;
        }

        // a 3/4 view of models built at the origin (bind pose), as the garage shows one
        static void Render(GameObject[] roots, string name)
        {
            const int layer = 30;
            var layers = new Dictionary<Transform, int>();
            foreach (var g in roots) foreach (var tr in g.GetComponentsInChildren<Transform>(true)) { layers[tr] = tr.gameObject.layer; tr.gameObject.layer = layer; }
            var b = new Bounds(); bool any = false;
            foreach (var g in roots) foreach (var r in g.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            var cgo = new GameObject("mud shot camera"); var cam = cgo.AddComponent<Camera>();
            var lgo = new GameObject("mud shot light"); var l = lgo.AddComponent<Light>(); l.type = LightType.Directional; l.intensity = 1.25f; l.cullingMask = 1 << layer; lgo.transform.rotation = Quaternion.Euler(38, 150, 0);
            var rt = new RenderTexture(1280, 720, 24) { antiAliasing = 4 };
            cam.targetTexture = rt; cam.cullingMask = 1 << layer; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = new Color(0.16f, 0.17f, 0.19f); cam.fieldOfView = 34;
            cam.transform.position = b.center + new Vector3(-1.05f, 0.42f, 1.0f).normalized * b.extents.magnitude * 2.1f; cam.transform.LookAt(b.center);
            cam.Render(); cam.Render();   // (the first render of a batch-mode session comes out empty)
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(cgo); UnityEngine.Object.DestroyImmediate(lgo); rt.Release(); UnityEngine.Object.DestroyImmediate(rt);
            foreach (var kv in layers) if (kv.Key) kv.Key.gameObject.layer = kv.Value;
            Log("shot " + name);
        }

        // ---------------------------------------------------------------------------------------------------- race (play mode)
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static int stage; static double stageAt, runningAt, raceAt; static string tag; static int ai; static float seconds, maxSpeed; static int nextShot;

        // the bench showcase's autopilot: throttle, and A (0x1e) / D (0x20) toward the race line ~45 ft ahead of the vehicle
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
            ai = int.Parse(Env("MUD_AI", "2")); string key = Env("MUD_EVENT", "National/NAT_Concord");
            seconds = float.Parse(Env("MUD_SECONDS", "60"), CultureInfo.InvariantCulture);
            tag = Env("MUD_TAG", "race_" + key.Replace('/', '_'));
            if (ReflexEvents.Find(key) == null) { Check(false, "no event " + key); End(); return; }
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = null;
            Log($"race {key} AI {ai}, {seconds} s");
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            stage = 0; stageAt = EditorApplication.timeSinceStartup; runningAt = raceAt = -1; maxSpeed = 0; nextShot = 0;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void PlayShot(string name, Camera from = null)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false;
            var target = new RenderTexture(1920, 1080, 24) { antiAliasing = 4 };
            var main = from ? from : UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>()?.chaseCamera ?? Camera.main;
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

        // a close 3/4 shot of a model root (the local bike from beside, an AI rider) in the race's own lighting
        static void CloseShot(GameObject root, string name)
        {
            if (!root) return;
            var b = new Bounds(); bool any = false;
            foreach (var r in root.GetComponentsInChildren<Renderer>()) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
            if (!any) return;
            var main = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>()?.chaseCamera ?? Camera.main;
            var go = new GameObject("close cam"); var c = go.AddComponent<Camera>(); if (main) c.CopyFrom(main); c.enabled = false; c.fieldOfView = 30;
            var fwd = root.transform.forward; var side = Vector3.Cross(Vector3.up, fwd).normalized;
            go.transform.position = b.center + (side * 1.0f + fwd * 0.55f + Vector3.up * 0.35f).normalized * Mathf.Max(4f, b.extents.magnitude * 3.2f);
            go.transform.LookAt(b.center);
            PlayShot(name, c);
            UnityEngine.Object.DestroyImmediate(go);
        }

        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static ReflexBikeModel LocalBike(OriginalWorldRuntime rt) => (ReflexBikeModel)typeof(OriginalWorldRuntime).GetField("bikeModel", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);

        static string MudSummary(OriginalWorldRuntime rt, out float local, out float aiMax, out int aiWith)
        {
            local = -1; aiMax = 0; aiWith = 0;
            var lb = LocalBike(rt);
            if (lb != null && lb.chassis && lb.chassis.TryGetComponent<ReflexMudMaterials>(out var lm)) local = lm.Amount.w;
            var sb = new StringBuilder($"local {local:0.000} ({(lb != null && lb.chassis && lb.chassis.TryGetComponent<ReflexMudMaterials>(out var lm2) ? lm2.Count : 0)} bike materials)");
            if (rt.opponentRiders)
                foreach (var o in rt.opponentRiders.Riders)
                {
                    float a = o.bike != null && o.bike.chassis && o.bike.chassis.TryGetComponent<ReflexMudMaterials>(out var m) ? m.Amount.w : -1;
                    float ra = o.rider != null && o.rider.root && o.rider.root.TryGetComponent<ReflexMudMaterials>(out var rm) ? rm.Amount.w : -1;
                    if (a > 0) aiWith++; aiMax = Mathf.Max(aiMax, a);
                    sb.Append($"; {o.name} {a:0.000}/{ra:0.000}");
                }
            return sb.ToString();
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
                            Log("gate: " + MudSummary(rt, out float l0, out _, out _));
                            Check(l0 == 0, "at the gate the local bike is clean (record amount 0, as MudRecord_Bind leaves it)");
                            PlayShot("01_gate");
                            var lb = LocalBike(rt); if (lb != null) CloseShot(lb.chassis, "01_gate_close");
                            Next();
                        }
                        else if (Age > 420) Fail("world not running after 420 s");
                        break;
                    case 1:
                        if (raceAt >= 0)
                        {
                            double t = now - raceAt;
                            if (t > nextShot * seconds / 3.0 + 3)
                            {
                                Log($"race {t:0} s: " + MudSummary(rt, out _, out _, out _));
                                if (nextShot > 0) PlayShot($"02_race_{t:000}s");
                                nextShot++;
                            }
                            if (t > seconds + 3) Next();
                        }
                        else if (Age > 240) Fail("the gate did not drop");
                        break;
                    case 2:
                    {
                        string summary = MudSummary(rt, out float local, out float aiMax, out int aiWith);
                        Log("end: " + summary);
                        PlayShot("03_end");
                        var lb = LocalBike(rt); if (lb != null) CloseShot(lb.chassis, "03_end_close_bike");
                        var riders = rt.opponentRiders ? rt.opponentRiders.Riders.ToList() : new List<(string, int, ReflexBikeModel, ReflexRiderModel, bool)>();
                        var eye = rt.chaseCamera ? rt.chaseCamera.transform.position : Vector3.zero;
                        var nearest = riders.Where(o => o.Item3 != null && o.Item3.chassis && o.Item5).OrderBy(o => (o.Item3.chassis.transform.position - eye).sqrMagnitude).FirstOrDefault();
                        if (nearest.Item3 != null) CloseShot(nearest.Item3.chassis, "03_end_close_ai");
                        bool muddyLevel = Env("MUD_EXPECT", "1") == "1";
                        if (muddyLevel) Check(local > 0, $"the local rider's materials carry the game's growing mud amount ({local:0.000})");
                        else Check(local == 0, $"no mud on this level (no MudManager: rates 0): local {local:0.000}");
                        if (ai > 0) Check(muddyLevel ? aiWith == riders.Count && aiWith > 0 : aiMax == 0, $"AI riders with mud: {aiWith}/{riders.Count} (max {aiMax:0.000})");
                        Check(maxSpeed > 25, $"riding: top speed {maxSpeed:0.0} ft/s, place {race.place}/{race.riders}, lap progress {race.lap_progress:0.000}");
                        stage = 100; EditorApplication.ExitPlaymode();
                        break;
                    }
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            Check(SavePrefs().All(kv => kv.Value.Equals(saved[kv.Key])), "user PlayerPrefs untouched");
            Log($"mud race {tag}: " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "..", tag + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
