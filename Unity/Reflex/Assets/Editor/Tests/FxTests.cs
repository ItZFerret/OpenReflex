using System;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor.SceneManagement;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // WP-R3 fx (_re/notes/fx.md).
    //  Verify: every level with a scene: the glow-family materials of its static scenery (program, texture slots), its camera-facing
    //    models (the sun), and the counts of the fx entities the PC build does not draw (VolumeFog, LensFlare, OverlayLight); then one
    //    level is attached for real (OriginalLevel + OriginalLightGlow, REFLEX_FX_LEVEL, default NAT_Stillwater): materials converted to
    //    "Reflex/Original Light Glow" with their textures, the sun billboard built, the cost of the attach.
    //  The frame comparisons use RenderCompareTests.Render (OriginalLevel registers OriginalLightGlow) with and without REFLEX_R3_OFF=1.
    public static class FxTests
    {
        public static void Verify()
        {
            var log = new StringBuilder(); bool ok = true;
            try
            {
                var glow = new int[ReflexLightGlow.ProgramSun + 1]; int sunLevels = 0, fogLevels = 0, flareLevels = 0, overlayLevels = 0;
                foreach (var scene in Directory.GetFiles(ReflexInstall.DatabaseDir, "*.dx9.scene").OrderBy(s => s))
                {
                    string level = Path.GetFileName(scene).Replace(".dx9.scene", "");
                    if (level.StartsWith("Ctrl_", StringComparison.Ordinal) || !File.Exists(Path.Combine(ReflexInstall.DatabaseDir, level + ".dx9.database"))) continue;
                    ReflexLevelContent c;
                    try { c = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { terrain = false, tracks = false }); }
                    catch (Exception e) { log.AppendLine($"{level}: not loaded ({e.Message})"); continue; }
                    var mats = c.GlowMaterialSurvey(); var faces = c.CameraFacingEntities();
                    int vf = c.CountEntities("VolumeFogRSData"), lf = c.CountEntities("LensFlareRSData"), ol = c.CountEntities("OverlayLightRSData");
                    if (faces.Count > 0) sunLevels++; if (vf > 0) fogLevels++; if (lf > 0) flareLevels++; if (ol > 0) overlayLevels++;
                    foreach (var m in mats) glow[m.program]++;
                    if (mats.Count == 0 && faces.Count == 0 && vf + lf + ol == 0) { c.Dispose(); continue; }
                    log.Append($"{level}: glow {mats.Count}");
                    foreach (var m in mats) log.Append($" [{m.material} {m.pixelShader} p{m.program} {string.Join("/", m.textures)}]");
                    foreach (var f in faces) log.Append($" | camera-facing #{f.entityId} {f.model} {f.material} axis {f.axisToAlign} enabled {f.startEnabled} at ({f.matrix[12]:F0}, {f.matrix[13]:F0}, {f.matrix[14]:F0})");
                    log.AppendLine($" | not drawn on PC: VolumeFog {vf}, LensFlare {lf}, OverlayLight {ol}");
                    foreach (var m in mats)
                    {
                        int need = m.program == 1 || m.program == 6 ? 2 : m.program == 7 ? 4 : m.program == 2 || m.program == 3 ? 0 : 1;
                        if (m.textures.Length < need) { ok = false; log.AppendLine($"  FAIL {m.material}: {m.textures.Length} texture slots, program {m.program} samples {need}"); }
                    }
                    c.Dispose();
                }
                log.AppendLine($"glow materials by program {string.Join(" ", glow.Select((n, i) => $"p{i}:{n}"))}; levels with a sun billboard {sunLevels}, VolumeFog {fogLevels}, LensFlare {flareLevels}, OverlayLight {overlayLevels}");

                // one level attached for real (edit mode: the OriginalLevel component loads in LoadNow)
                string level1 = Environment.GetEnvironmentVariable("REFLEX_FX_LEVEL") ?? "NAT_Stillwater";
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var go = new GameObject("Original level");
                var lvl = go.AddComponent<OriginalLevel>(); lvl.level = level1; lvl.originalPipeline = false; lvl.alignSun = false;
                lvl.LoadNow();
                var g = go.GetComponent<OriginalLightGlow>();
                if (!g) { ok = false; log.AppendLine($"FAIL {level1}: OriginalLightGlow not attached"); }
                else
                {
                    log.AppendLine("attach: " + g.report);
                    var converted = lvl.Content.SceneryMaterials.Values.Where(m => m && m.shader && m.shader.name == ReflexLightGlow.ShaderName).ToList();
                    foreach (var m in converted)
                        log.AppendLine($"  {m.name}: program {m.GetFloat("_Program")} queue {m.renderQueue} blend {m.GetFloat("_SrcBlend")}/{m.GetFloat("_DstBlend")} zwrite {m.GetFloat("_ZWrite")} cull {m.GetFloat("_Cull")} tex0 {(m.GetTexture("_Tex0") ? m.GetTexture("_Tex0").name : "-")}");
                    foreach (var r in go.GetComponentsInChildren<MeshRenderer>(true).Where(r => r.sharedMaterial && r.sharedMaterial.GetFloat("_RfxBillboard") > 0.5f))
                    {
                        var m = r.sharedMaterial;
                        log.AppendLine($"  billboard {r.name}: at {r.transform.position} scale {m.GetVector("_BillboardScale")} tex0 {(m.GetTexture("_Tex0") ? m.GetTexture("_Tex0").name + " " + m.GetTexture("_Tex0").graphicsFormat : "-")} tex1 {(m.GetTexture("_Tex1") ? m.GetTexture("_Tex1").name : "-")} queue {m.renderQueue}");
                        if (!m.GetTexture("_Tex0") || m.GetTexture("_Tex0") == Texture2D.whiteTexture) { ok = false; log.AppendLine("  FAIL billboard texture missing"); }
                    }
                    if (g.billboards == 0 && lvl.Content.CameraFacingEntities().Count > 0) { ok = false; log.AppendLine("  FAIL camera-facing models not built"); }
                }
                lvl.Unload(); UnityEngine.Object.DestroyImmediate(go);
            }
            catch (Exception e) { ok = false; log.AppendLine("FAILED " + e); }
            log.AppendLine(ok ? "FxTests.Verify PASS" : "FxTests.Verify FAIL");
            Debug.Log("[FxTests] " + log);
        }
        // Post-chain intermediates of a frame (sky-only check near a bright source such as the sun): the static level without terrain from
        // a recorded camera, ReflexRenderPipeline with keepIntermediates; writes <out>_i<k>.f32 (header w, h, then RGBA float rows, top first).
        //   REFLEX_RC_LEVEL, REFLEX_RC_CAMERA (camera.json), REFLEX_RC_OUT
        [Serializable] class CameraJson { public float[] position, right, up, forward; public float fov, aspect, near, far; }
        public static void PostDump()
        {
            var log = new StringBuilder();
            try
            {
                string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL"), outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT");
                var cj = JsonUtility.FromJson<CameraJson>(File.ReadAllText(Environment.GetEnvironmentVariable("REFLEX_RC_CAMERA")));
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var go = new GameObject("Original level");
                var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.originalPipeline = false; ol.alignSun = false;
                ol.LoadNow();
                var camGo = new GameObject("Camera"); var cam = camGo.AddComponent<Camera>();
                var urp = camGo.AddComponent<UnityEngine.Rendering.Universal.UniversalAdditionalCameraData>();
                urp.renderPostProcessing = false; urp.antialiasing = UnityEngine.Rendering.Universal.AntialiasingMode.None; urp.renderShadows = false;
                cam.allowHDR = true; cam.allowMSAA = false; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black;
                Vector3 V(float[] a) => new Vector3(a[0], a[1], a[2]);
                cam.transform.SetPositionAndRotation(V(cj.position), Quaternion.LookRotation(V(cj.forward), V(cj.up)));
                cam.fieldOfView = cj.fov; cam.nearClipPlane = cj.near; cam.farClipPlane = Mathf.Min(cj.far, 15000f); cam.aspect = 1280f / 720f;
                var pipe = camGo.AddComponent<Reflex.Port.Rendering.ReflexRenderPipeline>();
                pipe.hdr = ol.Content.lighting.hdr; pipe.convergeAdaptation = true;
                bool halfTarget = Environment.GetEnvironmentVariable("REFLEX_FX_HALF_TARGET") == "1";   // A/B: an FP16 camera target instead of the 8-bit one
                var ldr = halfTarget ? new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear) : new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB); cam.targetTexture = ldr;
                pipe.postProcess = true; pipe.keepIntermediates = true;
                for (int i = 0; i < 3; i++) { RenderCompareTests.ApplyD3D9PixelCenters(cam); cam.Render(); }
                for (int k = 0; pipe.Intermediates != null && k < pipe.Intermediates.Length; k++)
                {
                    var rt = pipe.Intermediates[k].rt; var prev = RenderTexture.active; RenderTexture.active = rt;
                    var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBAFloat, false, true); t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
                    RenderTexture.active = prev;
                    var px = t.GetPixelData<float>(0);
                    using (var f = new BinaryWriter(File.Create($"{outBase}_i{k}.f32")))
                    {
                        f.Write(rt.width); f.Write(rt.height);
                        for (int y = rt.height - 1; y >= 0; y--) for (int x = 0; x < rt.width * 4; x++) f.Write(px[y * rt.width * 4 + x]);
                    }
                    log.AppendLine($"intermediate {k} {rt.name} {rt.width}x{rt.height}");
                    UnityEngine.Object.DestroyImmediate(t);
                }
                cam.targetTexture = null; ldr.Release();
                ol.Unload(); log.AppendLine("ok");
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            Debug.Log("[FxTests.PostDump] " + log);
        }
        // Cost: the level from a recorded camera (REFLEX_RC_LEVEL / REFLEX_RC_CAMERA) through ReflexRenderPipeline into an FP16 target,
        // 200 frames with the glow / sun renderers on, 200 with them off (each frame waits for the GPU: a 1-pixel read back), and the
        // OriginalLightGlow per-frame update (the material table check) timed over 10000 calls.
        public static void Perf()
        {
            var log = new StringBuilder();
            try
            {
                string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL");
                var cj = JsonUtility.FromJson<CameraJson>(File.ReadAllText(Environment.GetEnvironmentVariable("REFLEX_RC_CAMERA")));
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var go = new GameObject("Original level");
                var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.originalPipeline = false; ol.alignSun = false;
                ol.LoadNow();
                var g = go.GetComponent<OriginalLightGlow>();
                var camGo = new GameObject("Camera"); var cam = camGo.AddComponent<Camera>();
                var urp = camGo.AddComponent<UnityEngine.Rendering.Universal.UniversalAdditionalCameraData>();
                urp.renderPostProcessing = false; urp.renderShadows = false; cam.allowHDR = true; cam.allowMSAA = false;
                Vector3 V(float[] a) => new Vector3(a[0], a[1], a[2]);
                cam.transform.SetPositionAndRotation(V(cj.position), Quaternion.LookRotation(V(cj.forward), V(cj.up)));
                cam.fieldOfView = cj.fov; cam.nearClipPlane = cj.near; cam.farClipPlane = Mathf.Min(cj.far, 15000f); cam.aspect = 1280f / 720f;
                var pipe = camGo.AddComponent<Reflex.Port.Rendering.ReflexRenderPipeline>(); pipe.hdr = ol.Content.lighting.hdr; pipe.postProcess = true;
                var rt = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear); cam.targetTexture = rt;
                var probe = new Texture2D(1, 1, TextureFormat.RGBAHalf, false, true);
                var glowRenderers = go.GetComponentsInChildren<Renderer>(true).Where(r => r.sharedMaterials.Any(m => m && m.shader && m.shader.name == ReflexLightGlow.ShaderName)).ToList();
                double Run(int n)
                {
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    for (int i = 0; i < n; i++)
                    {
                        cam.Render();
                        var prev = RenderTexture.active; RenderTexture.active = rt; probe.ReadPixels(new Rect(0, 0, 1, 1), 0, 0); RenderTexture.active = prev;
                    }
                    return sw.Elapsed.TotalMilliseconds / n;
                }
                Run(10);
                double on1 = Run(200);
                foreach (var r in glowRenderers) r.enabled = false;
                double off = Run(200);
                foreach (var r in glowRenderers) r.enabled = true;
                double on2 = Run(200);
                var upd = typeof(OriginalLightGlow).GetMethod("LateUpdate", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                var sw2 = System.Diagnostics.Stopwatch.StartNew(); for (int i = 0; g && i < 10000; i++) upd.Invoke(g, null);
                log.AppendLine($"{level}: {glowRenderers.Count} glow / sun renderers; frame {on1:F3} / {on2:F3} ms with, {off:F3} ms without (GPU + render submit, waited): delta {((on1 + on2) / 2 - off):F3} ms; per-frame update {sw2.Elapsed.TotalMilliseconds / 10000 * 1000:F2} us (incl. reflection call); attach {g?.report}");
                cam.targetTexture = null; rt.Release(); ol.Unload();
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            Debug.Log("[FxTests.Perf] " + log);
        }
    }
}
