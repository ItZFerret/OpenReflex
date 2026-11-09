using System;
using System.Diagnostics;
using System.Linq;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using Reflex.Port.Content;
using Debug = UnityEngine.Debug;

using UnityEditor.SceneManagement;
using UnityEngine.Rendering;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Tests
{
    // Batch-mode checks of the run-time level loaders against the Python exports, and a render of the loaded level:
    //   Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Tests.WorldContentTests.Verify -logFile <log>
    public static class WorldContentTests
    {
        static string OutFile => Environment.GetEnvironmentVariable("REFLEX_TEST_OUT") is string s && s.Length > 0 ? s : Path.Combine(Path.GetTempPath(), "world-content-test.txt");


        // Builds a scene in edit mode with OriginalLevel (scenery + terrain material), a Unity Terrain from the level's own height map
        // (stand-in for the live-world terrain) and renders views to PNG:  REFLEX_RENDER_LEVEL=NAT_Concord REFLEX_RENDER_DIR=<dir>
        public static void Render()
        {
            string level = Environment.GetEnvironmentVariable("REFLEX_RENDER_LEVEL") ?? "NAT_Concord";
            string dir = Environment.GetEnvironmentVariable("REFLEX_RENDER_DIR") ?? Path.GetTempPath();
            var log = new StringBuilder();
            try
            {
                Directory.CreateDirectory(dir);
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var sun = new GameObject("Sun").AddComponent<Light>(); sun.type = LightType.Directional; sun.shadows = LightShadows.Soft; sun.intensity = 1f;
                RenderSettings.sun = sun;
                var go = new GameObject("Original level");
                var terrainGo = new GameObject("Original terrain"); var otr = terrainGo.AddComponent<OriginalTerrain>();
                var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.terrain = otr;
                ol.sceneryShader = Shader.Find("Reflex/Original Scenery"); ol.terrainShader = Shader.Find("Reflex/Original HFTerrain"); ol.trackShader = Shader.Find("Reflex/Original HFT Tracks");
                ol.LoadNow();
                log.AppendLine(ol.report);
                // Unity terrain from the level's height map
                int n; float[] h; int spacing = ol.Content.TerrainSpacing;
                h = ReflexLevelVerify.ReadHeights(ol.Content, out n);
                int res = 1; while (res < n) res *= 2; res += 1;
                float lo = float.MaxValue, hi = float.MinValue; for (int i = 0; i < h.Length; i++) { float v = h[i] * 2048f; if (v < lo) lo = v; if (v > hi) hi = v; }
                float yMin = lo - 2f, sizeY = hi - lo + 4f;
                var data = new TerrainData { heightmapResolution = res };
                data.size = new Vector3((res - 1) * spacing, sizeY, (res - 1) * spacing);
                var grid = new float[res, res];
                for (int j = 0; j < res; j++) { int r = Mathf.Clamp(n - 1 - j, 0, n - 1); for (int i = 0; i < res; i++) grid[j, i] = (h[r * n + Mathf.Min(i, n - 1)] * 2048f - yMin) / sizeY; }
                data.SetHeights(0, 0, grid);
                var tgo = Terrain.CreateTerrainGameObject(data); var terrain = tgo.GetComponent<Terrain>();
                float half = (n - 1) * spacing * 0.5f;
                tgo.transform.position = new Vector3(-half, yMin, -half);
                terrain.drawInstanced = true;
                var mat = new Material(otr.materialTemplate);
                mat.SetVector("_GridOrigin", new Vector4(-half, half, 2 * half, 0));
                terrain.materialTemplate = mat;
                // cameras
                var anchors = ol.Anchors; var a0 = anchors[0];
                var views = new (string name, Vector3 pos, Vector3 look)[]
                {
                    ("start", a0.position + new Vector3(0, 6, 0) - a0.forward * 18, a0.position + a0.forward * 40 + Vector3.up * 2),
                    ("ahead", a0.position + new Vector3(0, 25, 0) - a0.forward * 40, a0.position + a0.forward * 120),
                    ("high", a0.position + new Vector3(60, 160, 60), a0.position + a0.forward * 30),
                };
                string extra = Environment.GetEnvironmentVariable("REFLEX_RENDER_VIEWS");   // "name:px,py,pz:lx,ly,lz;..."
                if (!string.IsNullOrEmpty(extra))
                {
                    var list = new System.Collections.Generic.List<(string, Vector3, Vector3)>();
                    foreach (var v in extra.Split(';'))
                    {
                        var p = v.Split(':'); var a = p[1].Split(','); var b = p[2].Split(',');
                        list.Add((p[0], new Vector3(float.Parse(a[0]), float.Parse(a[1]), float.Parse(a[2])), new Vector3(float.Parse(b[0]), float.Parse(b[1]), float.Parse(b[2]))));
                    }
                    views = list.ToArray();
                }
                // REFLEX_RENDER_FOCUS="name1;name2": frame the first placed instance whose name/model contains the text
                string focus = Environment.GetEnvironmentVariable("REFLEX_RENDER_FOCUS");
                if (!string.IsNullOrEmpty(focus))
                {
                    var list = new System.Collections.Generic.List<(string, Vector3, Vector3)>();
                    foreach (var f in focus.Split(';'))
                    {
                        var inst = ol.Content.instances.FirstOrDefault(i => i.transform && (i.name.IndexOf(f, StringComparison.OrdinalIgnoreCase) >= 0 || i.model.IndexOf(f, StringComparison.OrdinalIgnoreCase) >= 0));
                        if (inst == null) { log.AppendLine("focus: no instance " + f); continue; }
                        var rends = inst.transform.GetComponentsInChildren<Renderer>(); var b = rends[0].bounds; foreach (var r in rends) b.Encapsulate(r.bounds);
                        float d = Mathf.Max(b.extents.magnitude * 2.2f, 3f);
                        var toSun = ol.Content.lighting.sunDirection; toSun.y = 0; toSun = toSun.sqrMagnitude > 1e-4f ? toSun.normalized : Vector3.right;
                        list.Add((f.Replace(' ', '_'), b.center + (Vector3.Cross(toSun, Vector3.up) * 0.8f + toSun * 0.5f).normalized * d + Vector3.up * d * 0.35f, b.center));
                        log.AppendLine($"focus {f}: {inst.name} [{inst.model}] at {b.center}");
                    }
                    views = list.ToArray();
                }
                var cam = new GameObject("cam").AddComponent<Camera>();
                cam.fieldOfView = 47; cam.nearClipPlane = 0.15f; cam.farClipPlane = 14000; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Environment.GetEnvironmentVariable("REFLEX_RENDER_BG") == "magenta" ? Color.magenta : new Color(0.47f, 0.61f, 0.73f);
                int W = 1600, H = 900;
                var rt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                cam.targetTexture = rt;
                for (int w = 0; w < 4; w++) { cam.transform.position = views[0].pos; cam.transform.LookAt(views[0].look); cam.Render(); System.Threading.Thread.Sleep(300); }   // warm-up: the editor compiles shader variants asynchronously
                foreach (var g in ol.Content.instances) if (g.entityType == "DropGateRSData") log.AppendLine($"gate {g.name} at {g.transform.position}");
                foreach (var v in views)
                {
                    cam.transform.position = v.pos; cam.transform.LookAt(v.look);
                    cam.Render();
                    RenderTexture.active = rt;
                    var tex = new Texture2D(W, H, TextureFormat.RGB24, false);
                    tex.ReadPixels(new Rect(0, 0, W, H), 0, 0); tex.Apply();
                    File.WriteAllBytes(Path.Combine(dir, $"{level}-{v.name}.png"), tex.EncodeToPNG());
                    UnityEngine.Object.DestroyImmediate(tex);
                    log.AppendLine("rendered " + v.name);
                }
                RenderTexture.active = null;
            }
            catch (Exception e) { log.AppendLine("EXCEPTION: " + e); }
            File.WriteAllText(OutFile, log.ToString());
            Debug.Log(log.ToString());
            EditorApplication.Exit(0);
        }

        // Opens a built scene and plays it (REFLEX_PLAYTEST=<out dir>; ReflexLevelPlayTest renders and exits):
        //   REFLEX_PLAYTEST_SCENE=Assets/Scenes/Reflex.unity
        public static void PlayTest()
        {
            string scene = Environment.GetEnvironmentVariable("REFLEX_PLAYTEST_SCENE") ?? "Assets/Scenes/Reflex.unity";
            EditorSceneManager.OpenScene(scene, OpenSceneMode.Single);
            // the sound/music loaders decode thousands of clips (minutes in batch mode); not needed for the visual check
            if (Environment.GetEnvironmentVariable("REFLEX_PLAYTEST_AUDIO") != "1")
            {
                foreach (var mb in UnityEngine.Object.FindObjectsByType<MonoBehaviour>(FindObjectsSortMode.None))
                    if (mb && (mb.GetType().Name == "OriginalAudio" || mb.GetType().Name == "OriginalMusic")) UnityEngine.Object.DestroyImmediate(mb.gameObject);
            }
            // no realtime/baked GI work in the headless editor (the Enlighten update dominates the frame otherwise)
            try { Lightmapping.lightingSettings = new LightingSettings { realtimeGI = false, bakedGI = false, autoGenerate = false }; } catch (Exception e) { Debug.LogWarning(e.Message); }
            Lightmapping.Cancel();
            EditorApplication.isPlaying = true;
        }

        // Edit-mode check of the scene wiring: opens a built scene and runs OriginalLevel.LoadNow() (what Awake does in play mode).
        public static void Wiring()
        {
            var log = new StringBuilder();
            try
            {
                string scene = Environment.GetEnvironmentVariable("REFLEX_PLAYTEST_SCENE") ?? "Assets/Scenes/Reflex.unity";
                EditorSceneManager.OpenScene(scene, OpenSceneMode.Single);
                var ol = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
                var rt = ol.runtime ? ol.runtime : UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                log.AppendLine($"scene {scene}: OriginalLevel {ol.level}, runtime {(rt ? "present" : "MISSING")}, terrain {(ol.terrain ? "present" : "MISSING")}; shaders: scenery {ol.sceneryShader}, sky {ol.skyShader}, terrain {ol.terrainShader}, tracks {ol.trackShader}");
                log.AppendLine("before: runtime.animatedObjects = " + (rt && rt.animatedObjects ? "set" : "null") + ", terrain.materialTemplate = " + (ol.terrain && ol.terrain.materialTemplate ? "set" : "null"));
                ol.LoadNow();
                log.AppendLine(ol.report);
                log.AppendLine("after: runtime.animatedObjects entries = " + (rt && rt.animatedObjects ? rt.animatedObjects.entries.Length.ToString() : "null"));
                if (rt && rt.animatedObjects) foreach (var e in rt.animatedObjects.entries) log.AppendLine($"  gate target {e.target.name} vtable 0x{e.vtable:x} id {e.id} idOffset 0x{e.idOffset:x} matrixOffset 0x{e.matrixOffset:x}");
                var t = ol.terrain;
                log.AppendLine("after: terrain.materialTemplate = " + (t.materialTemplate ? t.materialTemplate.name + " shader " + t.materialTemplate.shader.name : "null") + ", layers " + t.layers.Length);
                var tr = t.GetComponent<OriginalTracks>();
                log.AppendLine("after: OriginalTracks " + (tr ? $"generic {tr.trackGeneric?.name} mx {tr.trackMX?.name} atv {tr.trackATV?.name} default {tr.trackDefault?.name} softness {(tr.softnessMask ? tr.softnessMask.width + "x" + tr.softnessMask.height : "null")} shader {tr.trackShader}" : "MISSING"));
                log.AppendLine("scenery root children: " + (ol.SceneryRoot ? ol.SceneryRoot.transform.childCount + " groups" : "none"));
                var gates = UnityEngine.Object.FindObjectsByType<Transform>(FindObjectsSortMode.None).Where(x => x.name.StartsWith("DropGate [", StringComparison.Ordinal)).ToArray();
                log.AppendLine("DropGate objects found by name: " + gates.Length + (gates.Length > 0 ? " (" + gates[0].name + ")" : ""));
            }
            catch (Exception e) { log.AppendLine("EXCEPTION: " + e); }
            File.WriteAllText(OutFile, log.ToString());
            Debug.Log(log.ToString());
            EditorApplication.Exit(0);
        }

        public static void Verify()
        {
            var sb = new StringBuilder();
            try
            {
                ReflexLevelVerify.UsePythonDxt5Alpha(Environment.GetEnvironmentVariable("REFLEX_PY_DXT5") == "1");
                foreach (var level in new[] { "NAT_Concord", "FR_Alpine" })
                {
                    var sw = Stopwatch.StartNew();
                    using var c = ReflexLevelContent.Load(level);
                    sw.Stop();
                    sb.AppendLine($"=== {level}: Load() {sw.Elapsed.TotalSeconds:F2} s (parallel, blocking)");
                    sb.AppendLine(c.report);
                    sb.Append(ReflexLevelVerify.ShaderUsage(c));
                    sb.Append(ReflexLevelVerify.VerifyLightRig(c));
                    if (level == "NAT_Concord")
                    {
                        sb.Append(ReflexLevelVerify.VerifyConcordScenery(c));
                        sb.Append(ReflexLevelVerify.VerifyTerrain(c, "unity_concord", "NAT_CO"));
                        sb.Append(ReflexLevelVerify.VerifyAnchors(c, "unity_concord"));
                    }
                    else
                    {
                        string o = Environment.GetEnvironmentVariable("REFLEX_FORMATS_OUT") is string e && e.Length > 0 ? e : Path.GetFullPath(Path.Combine(Application.dataPath, "../../../_re/formats/out"));
                        sb.Append(ReflexLevelVerify.VerifyWorldMeshes(c, Path.Combine(o, "unity_assets/alpine_scenery/AlpineScenery.mesh.json"), "Alpine scenery (5 meshes)"));
                        sb.Append(ReflexLevelVerify.VerifyWorldMeshes(c, Path.Combine(o, "unity_assets/start_support/HC_Static.mesh.json"), "Alpine hovercraft"));
                        sb.Append(ReflexLevelVerify.VerifyTerrain(c, "unity_alpine", "FR_AL"));
                        sb.Append(ReflexLevelVerify.VerifyAnchors(c, "unity_alpine"));
                    }
                    // sequential load for comparison
                    sw.Restart();
                    using (var c2 = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { parallel = false })) { }
                    sb.AppendLine($"    sequential Load(): {sw.Elapsed.TotalSeconds:F2} s");
                }
            }
            catch (Exception e) { sb.AppendLine("EXCEPTION: " + e); }
            File.WriteAllText(OutFile, sb.ToString());
            Debug.Log(sb.ToString());
            EditorApplication.Exit(0);
        }
    }
}
