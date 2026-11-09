using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.FrontEnd;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // Render residuals (_re/notes/render-pipeline.md, section "Render residuals"): comparisons against recorded frames of the original
    // renderer with per-object state of that frame, and a scan for materials Unity draws with its error shader.
    //   Render  RenderCompareTests.Render (or ScreensTests.Render with REFLEX_SCREENS_FRAMES) plus REFLEX_RC_SCROLLS=<file>: lines
    //           "x y z u v" = a scrolling entity's world position (gWorld row 3 of its draw) and the gTextureOffset of the recorded frame;
    //           every scenery renderer within 0.5 m of that position gets it as _RfxScrollOverride / _RfxTextureOffset (OriginalProps does
    //           the same from the running game).
    //   Probe   REFLEX_RC_LEVEL=<level[,level...]>: every renderer of the loaded level (scenery, props, glows, water, forest, screens) whose
    //           material is missing or whose shader is unsupported / has errors / is the error shader, with the material's original
    //           program; writes <REFLEX_RC_OUT>_probe.txt.
    //   Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Tests.ResidTests.Render -logFile <log>
    public static class ResidTests
    {
        static int ApplyScrolls(OriginalLevel ol, string file, StringBuilder log)
        {
            var list = new List<(Vector3 p, Vector2 uv)>();
            foreach (var line in File.ReadAllLines(file))
            {
                var t = line.Split(new[] { ' ', '\t', ',' }, StringSplitOptions.RemoveEmptyEntries);
                if (t.Length < 5) continue;
                float F(int i) => float.Parse(t[i], CultureInfo.InvariantCulture);
                list.Add((new Vector3(F(0), F(1), F(2)), new Vector2(F(3), F(4))));
            }
            int set = 0; var mpb = new MaterialPropertyBlock();
            var renderers = ol.SceneryRoot ? ol.SceneryRoot.GetComponentsInChildren<Renderer>(true) : new Renderer[0];
            foreach (var r in renderers)
                foreach (var (p, uv) in list)
                    if ((r.transform.position - p).sqrMagnitude < 0.25f)
                    {
                        var v = new Vector4(uv.x, uv.y, 1, 0);
                        r.GetPropertyBlock(mpb); mpb.SetVector("_RfxScrollOverride", v); mpb.SetVector("_RfxTextureOffset", v); r.SetPropertyBlock(mpb);
                        set++; break;
                    }
            log.AppendLine($"scrolls: {list.Count} recorded offsets, {set} renderers set");
            return set;
        }

        static Camera lastCam;
        public static void Render()
        {
            string scrolls = Environment.GetEnvironmentVariable("REFLEX_RC_SCROLLS");
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_rc");
            var log = new StringBuilder();
            RenderCompareTests.BeforeRender = (ol, cam) =>
            {
                if (!string.IsNullOrEmpty(scrolls)) ApplyScrolls(ol, scrolls, log);
                lastCam = cam;
                foreach (var t in UnityEngine.Object.FindObjectsByType<Terrain>(FindObjectsSortMode.None))
                {
                    var m = t.materialTemplate; if (!m) continue;
                    // "before" runs: the terrain as drawn before the residuals work (Unity's geometric normal, grid uv column / (N - 1))
                    if (Environment.GetEnvironmentVariable("REFLEX_RC_OLDTERRAIN") == "1") m.SetVector("_GridParams", Vector4.zero);
                    log.Append($"terrain material {m.name} keywords [{string.Join(",", m.shaderKeywords)}]:");
                    foreach (var n in m.GetTexturePropertyNames()) { var tx = m.GetTexture(n); if (tx) log.Append($" {n}={tx.name} {tx.width}x{tx.height} {tx.graphicsFormat} {tx.filterMode} {tx.wrapMode} mips {tx.mipmapCount};"); }
                    log.AppendLine();
                }
            };
            try
            {
                if (Environment.GetEnvironmentVariable("REFLEX_SCREENS_FRAMES") != null) ScreensTests.Render(); else RenderCompareTests.Render();
            }
            finally { RenderCompareTests.BeforeRender = null; }
            // REFLEX_RC_PICK="x,y;x,y": the renderer that draws each pixel (D3D pixel coordinates, row 0 = top), by bisection over the
            // enabled renderers with the HDR scene
            if (Environment.GetEnvironmentVariable("REFLEX_RC_PICK") is string pick && pick.Length > 0 && lastCam)
            {
                var all = UnityEngine.Object.FindObjectsByType<Renderer>(FindObjectsInactive.Exclude, FindObjectsSortMode.None).Where(r => r.enabled).ToList();
                foreach (var pp in pick.Split(';'))
                {
                    var xy = pp.Split(','); int px = int.Parse(xy[0]), py = int.Parse(xy[1]);
                    Color At(List<Renderer> hide)
                    {
                        foreach (var r in hide) r.enabled = false;
                        var hdr = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
                        lastCam.targetTexture = hdr; RenderCompareTests.ApplyD3D9PixelCenters(lastCam); lastCam.Render();
                        var prev = RenderTexture.active; RenderTexture.active = hdr;
                        var t = new Texture2D(1, 1, TextureFormat.RGBAFloat, false, true); t.ReadPixels(new Rect(px, 719 - py, 1, 1), 0, 0); t.Apply();
                        RenderTexture.active = prev; lastCam.targetTexture = null; hdr.Release();
                        var c = t.GetPixel(0, 0); UnityEngine.Object.DestroyImmediate(t);
                        foreach (var r in hide) r.enabled = true;
                        return c;
                    }
                    var baseC = At(new List<Renderer>());
                    var set = all;
                    while (set.Count > 1)
                    {
                        var half = set.Take(set.Count / 2).ToList();
                        var c = At(half);
                        set = Mathf.Abs(c.r - baseC.r) + Mathf.Abs(c.g - baseC.g) + Mathf.Abs(c.b - baseC.b) > 1e-4f ? half : set.Skip(set.Count / 2).ToList();
                    }
                    var rr = set.FirstOrDefault();
                    log.AppendLine($"pick {px},{py}: {baseC} drawn by {(rr ? PathOf(rr.transform) : "-")} materials {(rr ? string.Join(" | ", rr.sharedMaterials.Select(m => m ? $"{m.name} [{m.shader?.name} mode {(m.HasProperty("_Mode") ? m.GetFloat("_Mode") : -1)}]" : "null")) : "")}");
                }
            }
            File.WriteAllText(outBase + "_resid.txt", log.ToString());
            Debug.Log("[Resid] " + log);
        }

        public static void Probe()
        {
            string levels = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "FMX_BuenaVista";
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_probe");
            var log = new StringBuilder();
            foreach (var level in levels.Split(','))
            {
                try
                {
                    EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                    var go = new GameObject("Original level");
                    var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.originalPipeline = false; ol.alignSun = false;
                    ol.sceneryShader = Shader.Find("Reflex/Original Scenery"); ol.terrainShader = Shader.Find("Reflex/Original HFTerrain");
                    ol.skyShader = Shader.Find("Reflex/Original Sky");
                    ol.LoadNow();
                    log.AppendLine($"== {level}: {ol.report}");
                    File.WriteAllText(outBase + "_" + level + "_materials.txt", ol.Content.DescribeMaterials());
                    int bad = 0, total = 0;
                    var shaders = new Dictionary<string, int>();
                    foreach (var r in UnityEngine.Object.FindObjectsByType<Renderer>(FindObjectsInactive.Include, FindObjectsSortMode.None))
                    {
                        var mats = r.sharedMaterials; int subs = r is MeshRenderer mr && mr.TryGetComponent<MeshFilter>(out var mf) && mf.sharedMesh ? mf.sharedMesh.subMeshCount : r is SkinnedMeshRenderer smr && smr.sharedMesh ? smr.sharedMesh.subMeshCount : mats.Length;
                        for (int i = 0; i < Math.Max(subs, mats.Length); i++)
                        {
                            total++;
                            var m = i < mats.Length ? mats[i] : null;
                            string why = null;
                            if (!m) why = "no material";
                            else if (!m.shader) why = "no shader";
                            else if (!m.shader.isSupported) why = "shader unsupported " + m.shader.name;
                            else if (m.shader.name.Contains("InternalErrorShader") || m.shader.name.Contains("FallbackError")) why = "error shader " + m.shader.name;
                            else if (ShaderUtil.ShaderHasError(m.shader)) why = "shader has errors " + m.shader.name;
                            if (m && m.shader) { shaders.TryGetValue(m.shader.name, out int c); shaders[m.shader.name] = c + 1; }
                            if (why == null) continue;
                            bad++;
                            log.AppendLine($"  BAD {PathOf(r.transform)} slot {i}: {why} material '{(m ? m.name : "-")}' visible {r.enabled && r.gameObject.activeInHierarchy}");
                        }
                    }
                    log.AppendLine($"  {total} renderer slots, {bad} bad; shaders: {string.Join(", ", shaders.Select(kv => kv.Key + " " + kv.Value))}");
                    foreach (var w in ol.Content.warnings.Take(80)) log.AppendLine("  warn " + w);
                }
                catch (Exception e) { log.AppendLine($"== {level}: FAILED {e}"); }
            }
            File.WriteAllText(outBase + "_probe.txt", log.ToString());
            Debug.Log("[Resid] " + log);
        }

        // ------------------------------------------------------------------------------------------------ play: magenta scan
        //   Play  REFLEX_RESID_EVENT (default Freestyle/FMX_BuenaVista) in play mode; after REFLEX_RESID_WAIT s (default 12) of running world:
        //         the material probe over every renderer, shots from the chase camera and from the arena centre looking up at the roof
        //         (8 headings), each shot's magenta pixels, and for a shot with magenta the renderers that draw it (bisection over the
        //         renderers). Writes <REFLEX_RC_OUT>_play.txt and the shots. PlayerPrefs of the user are checked and restored.
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static (string key, bool had, string s, int i, float f)[] savedKeys;
        static StringBuilder playLog; static double playStart, runAt; static int playStage; static string playOut;

        public static void Play()
        {
            savedKeys = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
            playOut = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? System.IO.Path.Combine(System.IO.Path.GetTempPath(), "reflex_resid_play");
            playLog = new StringBuilder();
            string key = Environment.GetEnvironmentVariable("REFLEX_RESID_EVENT") ?? "Freestyle/FMX_BuenaVista";
            playLog.AppendLine("event " + key);
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = 0; ReflexEventSelect.PendingGarage = null;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            playStart = EditorApplication.timeSinceStartup; runAt = -1; playStage = 0;
            EditorApplication.update += PlayStep;
            EditorApplication.EnterPlaymode();
        }

        static void PlayFinish()
        {
            EditorApplication.update -= PlayStep;
            bool changed = false;
            foreach (var (k, had, s, i, f) in savedKeys)
            {
                bool now = PlayerPrefs.HasKey(k);
                if (now == had && (!had || PlayerPrefs.GetString(k, null) == s && PlayerPrefs.GetInt(k, int.MinValue) == i && PlayerPrefs.GetFloat(k, float.NaN).Equals(f))) continue;
                changed = true;
                if (!had) PlayerPrefs.DeleteKey(k); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(k, s); else if (i != int.MinValue) PlayerPrefs.SetInt(k, i); else PlayerPrefs.SetFloat(k, f);
            }
            if (changed) PlayerPrefs.Save();
            playLog.AppendLine(changed ? "PlayerPrefs of the user were changed (restored)" : "PlayerPrefs of the user untouched");
            File.WriteAllText(playOut + "_play.txt", playLog.ToString());
            Debug.Log("[Resid] " + playLog);
            if (Application.isBatchMode) EditorApplication.Exit(0);
        }

        static bool IsMagenta(Color32 c) => c.r > 2 * c.g + 40 && c.b > 2 * c.g + 40 && Math.Abs(c.r - c.b) < 70;

        static Color32[] Shoot(Camera src, Vector3? pos, Quaternion? rot, string name, out int magenta, List<Renderer> hide = null)
        {
            // the game's own camera (its ReflexRenderPipeline: raw HDR scene, the original post chain with bloom), moved for the shot
            var c = src; var tp = c.transform.position; var tr = c.transform.rotation; var tt = c.targetTexture;
            if (pos.HasValue) c.transform.SetPositionAndRotation(pos.Value, rot.Value);
            var rt = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
            c.targetTexture = rt;
            var was = new List<(Renderer, bool)>();
            if (hide != null) foreach (var r in hide) if (r) { was.Add((r, r.enabled)); r.enabled = false; }
            c.Render();
            foreach (var (r, e) in was) r.enabled = e;
            c.targetTexture = tt; c.transform.SetPositionAndRotation(tp, tr);
            var ldr = new RenderTexture(1280, 720, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            Graphics.Blit(rt, ldr);
            var prev = RenderTexture.active; RenderTexture.active = ldr;
            var t = new Texture2D(1280, 720, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); t.Apply(); RenderTexture.active = prev;
            var px = t.GetPixels32(); magenta = px.Count(IsMagenta);
            if (name != null) File.WriteAllBytes(playOut + "_" + name + ".png", t.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(t); rt.Release(); ldr.Release();
            return px;
        }

        static void PlayStep()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (playStage >= 100 || EditorApplication.timeSinceStartup - playStart > 200) { if (playStage < 100) playLog.AppendLine("play mode never started"); PlayFinish(); } return; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                double now = EditorApplication.timeSinceStartup;
                if (rt && rt.Running && runAt < 0) runAt = now;
                double wait = double.Parse(Environment.GetEnvironmentVariable("REFLEX_RESID_WAIT") ?? "12", CultureInfo.InvariantCulture);
                if (playStage == 0 && now - playStart > 600) { playLog.AppendLine("world not running after 600 s"); playStage = 100; EditorApplication.ExitPlaymode(); return; }
                if (playStage != 0 || runAt < 0 || now - runAt < wait) return;
                playStage = 1;
                var renderers = UnityEngine.Object.FindObjectsByType<Renderer>(FindObjectsInactive.Exclude, FindObjectsSortMode.None).Where(r => r.enabled).ToList();
                int bad = 0;
                foreach (var r in renderers)
                    foreach (var m in r.sharedMaterials)
                    {
                        string why = !m ? "no material" : !m.shader ? "no shader" : !m.shader.isSupported ? "unsupported " + m.shader.name : m.shader.name.Contains("Error") ? "error shader " + m.shader.name : ShaderUtil.ShaderHasError(m.shader) ? "shader errors " + m.shader.name : null;
                        if (why != null) { bad++; if (bad < 60) playLog.AppendLine($"  BAD {PathOf(r.transform)}: {why} material '{(m ? m.name : "-")}'"); }
                    }
                playLog.AppendLine($"{renderers.Count} enabled renderers, {bad} bad material slots");
                var cam = rt.chaseCamera ? rt.chaseCamera : Camera.main;
                var shots = new List<(string name, Vector3? p, Quaternion? q)> { ("chase", null, null) };
                var centre = Environment.GetEnvironmentVariable("REFLEX_RESID_CENTRE") is string cs && cs.Length > 0 ? cs.Split(',').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray() : new float[] { cam.transform.position.x, cam.transform.position.y + 8f, cam.transform.position.z };
                float pitch = float.Parse(Environment.GetEnvironmentVariable("REFLEX_RESID_PITCH") ?? "-25", CultureInfo.InvariantCulture);
                for (int k = 0; k < 8; k++) shots.Add(($"up{k}", new Vector3(centre[0], centre[1], centre[2]), Quaternion.Euler(pitch, k * 45f, 0)));
                foreach (var (name, p, q) in shots)
                {
                    Shoot(cam, p, q, name, out int mag);
                    playLog.AppendLine($"shot {name}: {mag} magenta pixels");
                    if (mag < 8) continue;
                    // bisection: the smallest set of renderers whose hiding removes the magenta
                    var cand = renderers.Where(r => r && r.isVisible).ToList();
                    var found = new List<Renderer>();
                    void Search(List<Renderer> set, int depth)
                    {
                        if (set.Count == 0 || found.Count > 12) return;
                        Shoot(cam, p, q, null, out int m2, set);
                        if (m2 >= mag) return;                 // hiding these does not remove any magenta
                        if (set.Count == 1) { found.Add(set[0]); return; }
                        int h = set.Count / 2; Search(set.Take(h).ToList(), depth + 1); Search(set.Skip(h).ToList(), depth + 1);
                    }
                    Search(cand, 0);
                    foreach (var r in found)
                        playLog.AppendLine($"  magenta from {PathOf(r.transform)} ({r.GetType().Name}): materials {string.Join(" | ", r.sharedMaterials.Select(m => m ? $"{m.name} [{m.shader?.name} prog {(m.HasProperty("_Program") ? m.GetFloat("_Program") : -1)} mode {(m.HasProperty("_Mode") ? m.GetFloat("_Mode") : -1)}]" : "null"))}");
                }
                playStage = 100; EditorApplication.ExitPlaymode();
            }
            catch (Exception e) { playLog.AppendLine("exception " + e); playStage = 100; EditorApplication.ExitPlaymode(); }
        }

        static string PathOf(Transform t) { var s = t.name; for (var p = t.parent; p; p = p.parent) s = p.name + "/" + s; return s; }
    }
}
