using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // Renders a level's static world (OriginalLevel scenery + sky, the original terrain material on a Unity Terrain built from the
    // level's height map) through ReflexRenderPipeline from the camera of a recorded frame of the original renderer
    // (native/drafts/world_runtime/render_capture.cpp, camera.json = gViewInverse / gViewProjection of that frame), for the
    // side-by-side comparison with the software-rasterized original (_re/notes/render-pipeline.md):
    //   REFLEX_RC_LEVEL=NAT_Concord REFLEX_RC_CAMERA=<capture>/camera.json REFLEX_RC_OUT=<dir>/<name>
    //   Unity -batchmode -quit -projectPath <bench> -executeMethod Reflex.Port.Tests.RenderCompareTests.Render -logFile <log>
    // Writes <name>_unity.png (the tonemapped image as displayed), <name>_unity_hdr.raw (the raw FP16 scene before the post
    // chain: header format 113, w, h, pitch, then RGBA half rows top first, like the capture's .raw dumps) and <name>_unity.txt.
    public static class RenderCompareTests
    {
        [Serializable] class CameraJson { public float[] position, right, up, forward; public float fov, aspect, near, far; }
        /// Tests that need per-object state of the recorded frame (e.g. ResidTests: each scrolling entity's gTextureOffset) set it here;
        /// called after the level and the camera are set up, before the frames are rendered.
        public static Action<OriginalLevel, Camera> BeforeRender;

        public static void Render()
        {
            string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "NAT_Concord";
            string camPath = Environment.GetEnvironmentVariable("REFLEX_RC_CAMERA");
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_rc");
            int W = 1280, H = 720;
            var log = new StringBuilder();
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(outBase));
                var cj = JsonUtility.FromJson<CameraJson>(File.ReadAllText(camPath));
                EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                var go = new GameObject("Original level");
                var terrainGo = new GameObject("Original terrain"); var otr = terrainGo.AddComponent<OriginalTerrain>();
                var ol = go.AddComponent<OriginalLevel>(); ol.level = level; ol.terrain = otr; ol.originalPipeline = false; ol.alignSun = false;
                ol.sceneryShader = Shader.Find("Reflex/Original Scenery"); ol.terrainShader = Shader.Find("Reflex/Original HFTerrain"); ol.trackShader = Shader.Find("Reflex/Original HFT Tracks");
                ol.skyShader = Shader.Find("Reflex/Original Sky");
                ol.LoadNow();
                log.AppendLine(ol.report);
                File.WriteAllText(outBase + "_materials.txt", ol.Content.DescribeMaterials());
                BuildTerrain(ol, otr, log);

                var camGo = new GameObject("Camera");
                var cam = camGo.AddComponent<Camera>();
                var urp = camGo.AddComponent<UniversalAdditionalCameraData>();
                urp.renderPostProcessing = false; urp.antialiasing = AntialiasingMode.None; urp.renderShadows = false;
                cam.allowHDR = true; cam.allowMSAA = false;
                cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black;
                Vector3 V(float[] a) => new Vector3(a[0], a[1], a[2]);
                cam.transform.SetPositionAndRotation(V(cj.position), Quaternion.LookRotation(V(cj.forward), V(cj.up)));
                cam.fieldOfView = cj.fov; cam.nearClipPlane = cj.near; cam.farClipPlane = Mathf.Min(cj.far, 15000f); cam.aspect = (float)W / H;
                // the recorded frame's scrolling offset (gTextureOffset c85 of the Kingston light beams) when given
                if (Environment.GetEnvironmentVariable("REFLEX_RC_SCROLL") is string sc && sc.Length > 0) { var p2 = sc.Split(','); Shader.SetGlobalVector("_RfxScrollOverride", new Vector4(float.Parse(p2[0], CultureInfo.InvariantCulture), float.Parse(p2[1], CultureInfo.InvariantCulture), 1, 0)); }
                if (ol.RTBump && Environment.GetEnvironmentVariable("REFLEX_RC_NORTBUMP") == "1") ol.RTBump.enabled = false;   // A/B: terrain without RT_Bump
                if (ol.RTBump && ol.RTBump.enabled)
                {
                    ol.RTBump.targetCamera = cam;
                    ApplyD3D9PixelCenters(cam);
                    var fit = ReflexRTBump.FitCascades(cam, ReflexRTBump.UnityTerrainHeight);
                    log.AppendLine($"rt_bump: {ol.RTBump.draws.Count} draws, near {fit.near:F2} (hit {fit.hit}) half {fit.nearHalf:F2} far half {fit.farHalf:F2}");
                    var ct = cam.transform; float tv = Mathf.Tan(cam.fieldOfView * 0.5f * Mathf.Deg2Rad), th = tv * cam.aspect;
                    log.AppendLine($"rt_bump: camera {ct.position.y - ReflexRTBump.UnityTerrainHeight(ct.position.x, ct.position.z):F2} above ground");
                    foreach (float sy in new[] { -1f, -0.9f, -0.75f, -0.5f })
                        foreach (float sx in new[] { -1f, 0f, 1f })
                            if (ReflexRTBump.RayHit(ct.position, ct.forward + ct.up * (tv * sy) + ct.right * (th * sx), ReflexRTBump.UnityTerrainHeight, out var hp))
                            {
                                var rel = new Vector2(hp.x - ct.position.x, hp.z - ct.position.z);
                                log.AppendLine($"  ray ({sx},{sy}): along {Vector2.Dot(rel, fit.forward):F2} side {Vector2.Dot(rel, fit.right):F2} dist {(hp - ct.position).magnitude:F2}");
                            }
                }
                BeforeRender?.Invoke(ol, cam);
                var pipe = camGo.AddComponent<ReflexRenderPipeline>();
                pipe.hdr = ol.Content.lighting.hdr; pipe.convergeAdaptation = true;
                log.AppendLine($"hdr: key {pipe.hdr.keyValue} attn {pipe.hdr.bloomAttenuation} adapt {pipe.hdr.minAdaptation}..{pipe.hdr.maxAdaptation} rate {pipe.hdr.adaptationRate} bloom {pipe.hdr.minBloomLuminance}..{pipe.hdr.maxBloomLuminance} whole {pipe.hdr.wholeSceneBloomAmount} (entity {ol.Content.lighting.hasHdrSettings})");

                // tonemapped image as displayed (sRGB target = the back buffer)
                // rendered into a half-float target: with an 8-bit camera target URP takes an 8-bit intermediate and clamps the HDR scene at
                // 1 before the post chain (bloom around anything brighter, found by WP-R3); then encoded into the sRGB image
                var ldr = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                var ldrHalf = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
                cam.targetTexture = ldrHalf;
                pipe.postProcess = true; pipe.keepIntermediates = true;
                RenderFrames(cam, 3);
                Graphics.Blit(ldrHalf, ldr); ldrHalf.Release();
                if (pipe.Intermediates != null)
                    foreach (var rt in pipe.Intermediates) { var px = ReadAll(rt.rt); Vector4 sum = Vector4.zero; foreach (var c in px) sum += (Vector4)c; log.AppendLine($"intermediate {rt.rt.name} {rt.rt.width}x{rt.rt.height} mean {sum / px.Length} first {px[0]}" + (px.Length <= 16 ? " all(row 0 = bottom) " + string.Join(" ", System.Linq.Enumerable.Select(px, c => c.r.ToString("F4", System.Globalization.CultureInfo.InvariantCulture))) : "")); }
                SavePng(ldr, outBase + "_unity.png");
                if (ol.RTBump && ol.RTBump.enabled && ol.RTBump.Target) SavePng(ol.RTBump.Target, outBase + "_unity_rtbump.png");
                var adapted = pipe.AdaptedLuminance;
                if (adapted != null) log.AppendLine($"adapted luminance {ReadFloat(adapted.rt):F6}");

                // raw HDR scene (post chain off, linear float target: no conversion on the final blit)
                var hdr = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
                cam.targetTexture = hdr;
                pipe.postProcess = false;
                RenderFrames(cam, 1);
                SaveRawHalf(hdr, outBase + "_unity_hdr.raw");
                cam.targetTexture = null;
                log.AppendLine("ok");
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            File.WriteAllText(outBase + "_unity.txt", log.ToString());
            Debug.Log("[RenderCompare] " + log);
        }

        static void RenderFrames(Camera cam, int n)
        {
            for (int i = 0; i < n; i++) { ApplyD3D9PixelCenters(cam); cam.Render(); }
        }

        // D3D9 samples pixel (i, j) at integer screen coordinates; Unity at i + 0.5: shift the projection by half a pixel so each
        // pixel sees the scene point the original's pixel sees.
        public static void ApplyD3D9PixelCenters(Camera cam)
        {
            cam.ResetProjectionMatrix();
            var p = cam.projectionMatrix;
            float w = cam.targetTexture ? cam.targetTexture.width : cam.pixelWidth, h = cam.targetTexture ? cam.targetTexture.height : cam.pixelHeight;
            p.m02 -= 1f / w;     // clip.x += clip.w / W  (clip.w = -z_view)
            p.m12 += 1f / h;     // clip.y -= clip.w / H
            cam.projectionMatrix = p;
        }

        static void BuildTerrain(OriginalLevel ol, OriginalTerrain otr, StringBuilder log)
        {
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
            // grid column c at x = (c - (n - 1) / 2) * spacing, row r at z = ((n - 1) / 2 - r) * spacing (gVertexOffset / gWorldHalfSize of the recorded frames)
            float half = (n - 1) * spacing * 0.5f;
            tgo.transform.position = new Vector3(-half, yMin, -half);
            terrain.drawInstanced = true; terrain.heightmapPixelError = 1f; terrain.basemapDistance = 20000f;
            terrain.shadowCastingMode = ShadowCastingMode.Off;
            var mat = new Material(otr.materialTemplate);
            mat.SetVector("_GridOrigin", new Vector4(-half, half, 2 * half, 0));
            mat.SetFloat("_UseUnityShadows", 0f);
            terrain.materialTemplate = mat;
            log.AppendLine($"terrain {n}x{n} spacing {spacing} y {yMin:F1}..{yMin + sizeY:F1}");
        }

        static void SavePng(RenderTexture rt, string path)
        {
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false, false);
            t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            File.WriteAllBytes(path, t.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(t);
        }

        static void SaveRawHalf(RenderTexture rt, string path)
        {
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBAHalf, false, true);
            t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            var px = t.GetPixelData<ushort>(0);
            int w = rt.width, h = rt.height, pitch = w * 8;
            using (var f = new BinaryWriter(File.Create(path)))
            {
                f.Write(113); f.Write(w); f.Write(h); f.Write(pitch);
                for (int y = h - 1; y >= 0; y--) for (int x = 0; x < w * 4; x++) f.Write(px[y * w * 4 + x]);   // top row first
            }
            UnityEngine.Object.DestroyImmediate(t);
        }

        static Color[] ReadAll(RenderTexture rt)
        {
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBAFloat, false, true);
            t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            var px = t.GetPixels(); UnityEngine.Object.DestroyImmediate(t); return px;
        }

        static float ReadFloat(RenderTexture rt)
        {
            if (!rt) return float.NaN;
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(1, 1, TextureFormat.RGBAFloat, false, true);
            t.ReadPixels(new Rect(0, 0, 1, 1), 0, 0); t.Apply();
            RenderTexture.active = prev;
            float v = t.GetPixel(0, 0).r; UnityEngine.Object.DestroyImmediate(t);
            return v;
        }
    }
}
