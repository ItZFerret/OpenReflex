using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // Water (WP-R2, _re/notes/water.md).
    //  Verify: every water level's surfaces load (asset, material, family), the clipmap buffers / tables and the per-frame level
    //    placement / draw constants against a recorded frame of the original (REFLEX_WT_CAPTURE=<capture dir>, its water/ memory dumps
    //    and obj/ draw dumps, optional), the ocean FFT against the recorded height map.
    //  Render: like RenderCompareTests.Render (the static level from a recorded frame's camera, original pipeline) with and without the
    //    water; REFLEX_WT_OCEAN=<ocean object dump> REFLEX_WT_TIME=<seconds> give the recorded frame's ocean state.
    public static class WaterTests
    {
        [Serializable] class CameraJson { public float[] position, right, up, forward; public float fov, aspect, near, far; }

        public static void Verify()
        {
            var log = new StringBuilder(); bool ok = true;
            try
            {
                foreach (var level in new[] { "FR_Alpine", "FMX_Dover", "FMX_DoverB", "FR_Cheyenne", "FR_Gibson", "NAT_DLC05_Dalton", "NAT_DLC06_DaltonB", "NAT_Stillwater",
                                              "NAT_StillwaterB", "PX_Laguna", "PX_LagunaB", "PX_LagunaB_BW", "PX_Laguna_BW", "SX_DLC01_FreightYard", "SX_DLC02_FreightYardB",
                                              "TRX_Halifax", "TRX_HalifaxBW", "TRX_Lynchburg", "TRX_LynchburgBW", "NAT_Concord" })
                {
                    if (!File.Exists(Path.Combine(ReflexInstall.DatabaseDir, level + ".dx9.database"))) { log.AppendLine($"{level}: not installed"); continue; }
                    var c = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { scenery = false, terrain = false, tracks = false });
                    var w = ReflexWater.Load(c);
                    log.AppendLine($"{level}: {w.Describe()} reflection {w.reflectionMap}" + (w.warnings.Count > 0 ? " WARN " + string.Join("; ", w.warnings) : ""));
                    if (w.warnings.Count > 0) ok = false;
                    if (level != "NAT_Concord" && w.surfaces.Count == 0) { ok = false; log.AppendLine("  FAIL no water"); }
                }
                string cap = Environment.GetEnvironmentVariable("REFLEX_WT_CAPTURE");
                if (!string.IsNullOrEmpty(cap) && Directory.Exists(cap)) ok &= VerifyCapture(cap, log);
            }
            catch (Exception e) { ok = false; log.AppendLine("FAILED " + e); }
            log.AppendLine(ok ? "WaterTests.Verify PASS" : "WaterTests.Verify FAIL");
            Debug.Log("[WaterTests] " + log);
        }

        // The recorded frame: the device buffers of the clipmap, the level records, every water draw's constants, the ocean height map.
        static bool VerifyCapture(string cap, StringBuilder log)
        {
            bool ok = true;
            var index = File.ReadAllLines(Path.Combine(cap, "water", "index.txt"));
            string frameDraw = null; string Mem(string what)
            {
                string tag = null;
                foreach (var l in index) { if (l.StartsWith("frame ")) { var p = l.Split(' '); tag = $"f{p[1]}_d{int.Parse(p[3]):D4}"; continue; } var q = l.Split(' '); if (q.Length >= 3 && q[2] == what && tag != null && tag.EndsWith(frameDraw ?? tag.Substring(tag.Length - 4))) return Path.Combine(cap, "water", $"{tag}_mem_{q[0]}.bin"); }
                return null;
            }
            string last = null; foreach (var l in index) if (l.StartsWith("frame ")) last = l;
            frameDraw = $"{int.Parse(last.Split(' ')[3]):D4}";
            var b = ReflexWater.Buffers;
            bool Same(string what, byte[] mine) { var f = Mem(what); if (f == null) { log.AppendLine($"  {what}: no dump"); return true; } bool s = File.ReadAllBytes(f).SequenceEqual(mine); log.AppendLine($"  {what}: {(s ? "byte-identical" : "DIFFERS")} ({mine.Length} bytes)"); return s; }
            byte[] V(Vector2[] v) { var o = new byte[v.Length * 8]; for (int i = 0; i < v.Length; i++) { BitConverter.GetBytes(v[i].x).CopyTo(o, 8 * i); BitConverter.GetBytes(v[i].y).CopyTo(o, 8 * i + 4); } return o; }
            byte[] I(ushort[] v) { var o = new byte[v.Length * 2]; Buffer.BlockCopy(v, 0, o, 0, o.Length); return o; }
            string[] names = index.Where(l => l.Contains("buffer_mesh+")).Select(l => l.Split(' ')[2]).Distinct().ToArray();
            string N(string off) => names.FirstOrDefault(n => n.StartsWith("buffer_mesh+" + off));
            ok &= Same(N("2c"), I(b.inner)); ok &= Same(N("38"), I(b.rows)); ok &= Same(N("44"), I(b.columns)); ok &= Same(N("48"), I(b.columnsOdd));
            for (int k = 0; k < 5; k++) ok &= Same(N(new[] { "54", "58", "5c", "60", "64" }[k]), V(b.vertices[k]));
            var mesh = File.ReadAllBytes(Mem("mesh")); var lv = File.ReadAllBytes(Mem("mesh_levels"));
            var t3 = File.ReadAllBytes(Mem("mesh_3c")); var t4 = File.ReadAllBytes(Mem("mesh_4c"));
            bool tables = true; for (int n = 0; n <= ReflexWater.GridH; n++) tables &= BitConverter.ToInt32(t3, 4 * n) == b.rowPrims[n] && BitConverter.ToInt32(t4, 4 * n) == b.columnPrims[n];
            log.AppendLine($"  primitive tables: {(tables ? "identical" : "DIFFER")}"); ok &= tables;
            var asset = File.ReadAllBytes(Mem("XenonWaterAsset"));
            float sx = BitConverter.ToSingle(mesh, 0x14), sz = BitConverter.ToSingle(mesh, 0x18);
            var m = new float[16]; for (int i = 0; i < 16; i++) m[i] = BitConverter.ToSingle(mesh, 0x70 + 4 * i);
            var cm = new ReflexWater.Clipmap(sx, sz);
            ok &= cm.count == BitConverter.ToInt32(mesh, 8);
            // the camera of the water draws (gCameraPos c8 of the first surface draw)
            Vector3 camPos = Vector3.zero;
            foreach (var f in Directory.GetFiles(Path.Combine(cap, "obj"), "*.bin").OrderBy(x => x))
            {
                var d = File.ReadAllBytes(f); int p0 = ConstantsOffset(d);
                if (BitConverter.ToSingle(d, p0 + 16 * 204) == 0) continue;
                camPos = new Vector3(BitConverter.ToSingle(d, p0 + 16 * 8), BitConverter.ToSingle(d, p0 + 16 * 8 + 4), BitConverter.ToSingle(d, p0 + 16 * 8 + 8)); break;
            }
            cm.Update(camPos, m);
            bool levels = cm.first == BitConverter.ToInt32(mesh, 0x68);
            for (int l = 0; l < cm.count; l++)
            {
                var L = cm.levels[l]; int o = l * 0x50;
                levels &= L.scale == BitConverter.ToSingle(lv, o) && L.x == BitConverter.ToSingle(lv, o + 8) && L.z == BitConverter.ToSingle(lv, o + 12) && L.sideOffset == BitConverter.ToSingle(lv, o + 16)
                       && L.bottom == BitConverter.ToInt32(lv, o + 20) && L.top == BitConverter.ToInt32(lv, o + 24) && L.left == BitConverter.ToInt32(lv, o + 28) && L.right == BitConverter.ToInt32(lv, o + 32);
            }
            log.AppendLine($"  clipmap: {cm.count} levels, first {cm.first}: level records {(levels ? "identical" : "DIFFER")}"); ok &= levels;
            // per-draw constants of the recorded surface draws (obj/f<frame>_d<draw>.bin: vertex constants at 256 x 16 bytes)
            int matched = 0, total = 0;
            var draws = cm.Draws().ToList();
            foreach (var f in Directory.GetFiles(Path.Combine(cap, "obj"), "*.bin").OrderBy(x => x))
            {
                var d = File.ReadAllBytes(f); int p = ConstantsOffset(d);
                Vector4 C(int r) => new Vector4(BitConverter.ToSingle(d, p + 16 * r), BitConverter.ToSingle(d, p + 16 * r + 4), BitConverter.ToSingle(d, p + 16 * r + 8), BitConverter.ToSingle(d, p + 16 * r + 12));
                int prims = BitConverter.ToInt32(d, 20);
                if (C(204).x == 0 || C(190).x == 0 || prims <= 2) continue;   // not a surface draw (the wake step: 2 primitives)
                total++;
                foreach (var dr in draws)
                {
                    cm.DrawConstants(dr.level, dr.piece, new Vector2(BitConverter.ToSingle(mesh, 0xe0), BitConverter.ToSingle(mesh, 0xe4)), BitConverter.ToInt32(mesh, 0xd8), out var c201, out var c202, out var c204, out var c191, out var c190, out var c205);
                    if (dr.prims == prims && c201 == C(201) && c202 == C(202) && c204 == C(204) && c191.x == C(191).x && c191.y == C(191).y && c191.z == C(191).z && c190 == C(190) && c205.x == C(205).x && c205.y == C(205).y && c205.z == C(205).z)
                    { matched++; break; }
                }
            }
            log.AppendLine($"  draws: {matched} / {total} recorded surface draws equal to a port draw (primitives, c190 c191 c201 c202 c204 c205); port draws {draws.Count} (the original also frustum-culls pieces)");
            ok &= matched == total && total > 0;
            // ocean: the recorded height map = the update at time (object time - frame dt)
            var ob = File.ReadAllBytes(Mem("ocean"));
            float dt = BitConverter.ToSingle(asset, 0x490);
            var oc = ReflexWater.Ocean.FromGame(ob, 0, BitConverter.ToSingle(ob, 0x14018) - dt);
            oc.Update(dt);
            string texFile = Directory.GetFiles(Path.Combine(cap, "rt"), $"tex_{BitConverter.ToUInt32(ob, 0x14004 + 4 * BitConverter.ToInt32(ob, 0x1400c)):x8}_d*.raw").OrderBy(x => x).FirstOrDefault();
            if (texFile != null)
            {
                var t = File.ReadAllBytes(texFile); double maxd = 0; int exact = 0;
                for (int i = 0; i < 4096; i++) { float g = BitConverter.ToSingle(t, 16 + 4 * i); maxd = Math.Max(maxd, Math.Abs(g - oc.heights[i])); if (g == oc.heights[i]) exact++; }
                log.AppendLine($"  ocean height map vs recorded: max |diff| {maxd:E2} (values +-{oc.heights.Max(Math.Abs):F3}), {exact}/4096 bit-exact; time {oc.time:R} vs {BitConverter.ToSingle(ob, 0x14018):R}");
                ok &= maxd < 1e-5;
            }
            return ok;
        }

        // obj/*.bin (render_capture.cpp dump_obj): header, indices, declaration, streams, then the 256 vertex constants
        static int ConstantsOffset(byte[] d)
        {
            int nidx = BitConverter.ToInt32(d, 28), p = 32 + 4 * nidx; int nd = BitConverter.ToInt32(d, p); p += 4 + 8 * nd;
            for (int s = 0; s < 16; s++) { int want = BitConverter.ToInt32(d, p + 12); p += 16 + want; }
            return p;
        }

        public static void Render()
        {
            string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "FR_Alpine";
            string camPath = Environment.GetEnvironmentVariable("REFLEX_RC_CAMERA");
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_wt");
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
                ol.LoadNow();
                log.AppendLine(ol.report);
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
                if (ol.RTBump && ol.RTBump.enabled) { ol.RTBump.targetCamera = cam; RenderCompareTests.ApplyD3D9PixelCenters(cam); }
                var pipe = camGo.AddComponent<ReflexRenderPipeline>();
                pipe.hdr = ol.Content.lighting.hdr; pipe.convergeAdaptation = true;

                OriginalWater water = null;
                foreach (bool withWater in new[] { false, true })
                {
                    if (withWater)
                    {
                        water = OriginalWater.Attach(ol);
                        if (!water) { log.AppendLine("no water"); break; }
                        water.targetCamera = cam; water.useGameState = false;
                        string oceanPath = Environment.GetEnvironmentVariable("REFLEX_WT_OCEAN");
                        if (!string.IsNullOrEmpty(oceanPath) && File.Exists(oceanPath))
                        {
                            var ob = File.ReadAllBytes(oceanPath);
                            float t = float.Parse(Environment.GetEnvironmentVariable("REFLEX_WT_TIME") ?? BitConverter.ToSingle(ob, 0x14018).ToString("R", CultureInfo.InvariantCulture), CultureInfo.InvariantCulture);
                            water.SetOcean(ReflexWater.Ocean.FromGame(ob, 0, t));
                        }
                        if (Environment.GetEnvironmentVariable("REFLEX_WT_FLOW") is string fl && fl.Length > 0)   // "<entity id>:<offset>,..."
                            foreach (var kv in fl.Split(',')) { var q2 = kv.Split(':'); water.SetFlowOffset(int.Parse(q2[0]), float.Parse(q2[1], CultureInfo.InvariantCulture)); }
                        if (Environment.GetEnvironmentVariable("REFLEX_WT_WAKETEX") is string wp && File.Exists(wp))   // a recorded wake target (tex dump, D3D rows)
                        {
                            var b = File.ReadAllBytes(wp); int w = BitConverter.ToInt32(b, 4), pitch = BitConverter.ToInt32(b, 8), h = BitConverter.ToInt32(b, 12);
                            var t = new Texture2D(w, h, TextureFormat.RFloat, false, true) { wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
                            var px = new float[w * h]; for (int y = 0; y < h; y++) Buffer.BlockCopy(b, 16 + y * pitch, px, y * w * 4, w * 4); t.SetPixelData(px, 0); t.Apply();
                            water.wakeOverride = t;
                        }
                        water.Tick();
                        log.AppendLine("water: " + water.report + $" ocean time {water.oceanTime:R}");
                    }
                    string tag = withWater ? "" : "_nowater";
                    var ldr = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                    cam.targetTexture = ldr; pipe.postProcess = true;
                    RenderFrames(cam, 3);
                    SavePng(ldr, outBase + tag + "_unity.png");
                    var hdr = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
                    cam.targetTexture = hdr; pipe.postProcess = false;
                    RenderFrames(cam, 1);
                    SaveRawHalf(hdr, outBase + tag + "_unity_hdr.raw");
                    cam.targetTexture = null;
                    if (withWater) log.AppendLine($"water draws {water.drawsLastFrame}");
                    if (withWater && Environment.GetEnvironmentVariable("REFLEX_WT_TRACE") is string tr && tr.Length > 0)
                    {
                        // per-instruction register values of the pixel program at one pixel (a shader generated with --trace)
                        var q = tr.Split(','); int px = int.Parse(q[0]), py = int.Parse(q[1]), n = int.Parse(q[2]);
                        var dt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
                        var one = new Texture2D(1, 1, TextureFormat.RGBAFloat, false, true);
                        for (int k = 1; k <= n; k++)
                        {
                            Shader.SetGlobalFloat("_RfxWaterTraceStep", k);
                            cam.targetTexture = dt; pipe.postProcess = false; RenderFrames(cam, 1);
                            var prev = RenderTexture.active; RenderTexture.active = dt; one.ReadPixels(new Rect(px, H - 1 - py, 1, 1), 0, 0); one.Apply(); RenderTexture.active = prev;
                            var c = one.GetPixel(0, 0); log.AppendLine($"trace {k}: {c.r:R} {c.g:R} {c.b:R} {c.a:R}");
                        }
                        Shader.SetGlobalFloat("_RfxWaterTraceStep", 0f); cam.targetTexture = null;
                    }
                    if (withWater && Environment.GetEnvironmentVariable("REFLEX_WT_DEBUG") is string dbg && dbg.Length > 0)
                        foreach (var k in dbg.Split(','))
                        {
                            Shader.SetGlobalFloat("_RfxWaterDebug", float.Parse(k, CultureInfo.InvariantCulture));
                            var dt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
                            cam.targetTexture = dt; pipe.postProcess = false; RenderFrames(cam, 1);
                            SaveRawFloat(dt, outBase + "_dbg" + k + ".raw"); cam.targetTexture = null;
                            Shader.SetGlobalFloat("_RfxWaterDebug", 0f);
                        }
                }
                log.AppendLine("ok");
            }
            catch (Exception e) { log.AppendLine("FAILED " + e); }
            File.WriteAllText(outBase + "_unity.txt", log.ToString());
            Debug.Log("[WaterRender] " + log);
        }

        static void RenderFrames(Camera cam, int n) { for (int i = 0; i < n; i++) { RenderCompareTests.ApplyD3D9PixelCenters(cam); cam.Render(); } }

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

        static void SaveRawFloat(RenderTexture rt, string path)
        {
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBAFloat, false, true);
            t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            var px = t.GetPixelData<float>(0);
            int w = rt.width, h = rt.height;
            using (var f = new BinaryWriter(File.Create(path)))
            {
                f.Write(116); f.Write(w); f.Write(h); f.Write(w * 16);
                for (int y = h - 1; y >= 0; y--) for (int x = 0; x < w * 4; x++) f.Write(px[y * w * 4 + x]);
            }
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
                for (int y = h - 1; y >= 0; y--) for (int x = 0; x < w * 4; x++) f.Write(px[y * w * 4 + x]);
            }
            UnityEngine.Object.DestroyImmediate(t);
        }

        // ------------------------------------------------------------------------------------------------ play (the running game)
        // Opens the race scene on an event (REFLEX_WT_EVENT, default FreeRide/FR_Alpine), holds throttle and steers at REFLEX_WT_TARGET
        // ("x,z", the river by default) and logs the water every 2 s (ocean time vs the game clock, wake steps, splats, draws, cost);
        // shots of the game camera at REFLEX_WT_SHOTS seconds after the world runs into REFLEX_RC_OUT. The user's PlayerPrefs keys are
        // left untouched (ReflexEventSelect.Pending; checked and restored on exit).
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static (string key, bool had, string s, int i, float f)[] savedKeys;
        static OriginalWorldRuntime playRuntime; static double playStarted, runStarted = -1, lastLog; static float[] shotTimes; static int shotIndex; static string playOut;
        static float startGameTime = float.NaN, startOceanTime; static int waypoint;

        public static void Play()
        {
            playOut = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_wt_play");
            Directory.CreateDirectory(Path.GetDirectoryName(playOut + "_x"));
            shotTimes = (Environment.GetEnvironmentVariable("REFLEX_WT_SHOTS") ?? "20,30,40").Split(',').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray();
            savedKeys = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
            ReflexEventSelect.Pending = Environment.GetEnvironmentVariable("REFLEX_WT_EVENT") ?? "FreeRide/FR_Alpine";
            ReflexEventSelect.PendingAi = 0;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += PlayTick;
            playStarted = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }

        static void PlayTick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - playStarted > 900) { PlayFinish("timeout"); return; }
            if (!EditorApplication.isPlaying) return;
            if (!playRuntime) playRuntime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!playRuntime || !playRuntime.Running) return;
            if (runStarted < 0) runStarted = now;
            var water = UnityEngine.Object.FindAnyObjectByType<OriginalWater>();
            playRuntime.testHeldKeys = now - runStarted > 3 ? PlaySteer() : Array.Empty<int>();
            if (now - lastLog > 2.0 && water)
            {
                lastLog = now;
                var res = playRuntime.Simulation?.Collect();
                float gameTime = res != null ? res.gameTime : float.NaN;
                if (float.IsNaN(startGameTime) && !float.IsNaN(gameTime) && water.oceanFromGame) { startGameTime = gameTime; startOceanTime = water.oceanTime; }
                var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(playRuntime);
                Debug.Log($"[WaterPlay] t {now - runStarted:F1} bike {(st.position != null ? $"{st.position[0]:F1} {st.position[1]:F1} {st.position[2]:F1}" : "-")} clock {water.gameClock:F3} ocean {water.oceanTime:F3} (from game {water.oceanFromGame}; advanced {water.oceanTime - startOceanTime:F3} s) draws {water.drawsLastFrame} wake steps {water.WakeSteps} splats {water.SplatsReceived} update {water.Milliseconds:F3} ms | {water.report.Replace("\n", " | ")}");
            }
            if (shotIndex < shotTimes.Length && now - runStarted >= shotTimes[shotIndex]) { PlayShot($"play_{shotIndex}_{shotTimes[shotIndex]:0}s"); shotIndex++; }
            if (shotIndex >= shotTimes.Length) PlayFinish("done");
        }

        static int[] PlaySteer()
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(playRuntime);
            if (st.position == null) return new[] { 0x11 };
            // waypoints "x,z;x,z;...": the next one once within 12 m (default: off the Alpine hovercraft and into the river)
            var wps = (Environment.GetEnvironmentVariable("REFLEX_WT_TARGET") ?? "1690,-1600;1700,-1575;1775,-1552;1850,-1520").Split(';')
                .Select(w => { var t = w.Split(','); return new Vector3(float.Parse(t[0], CultureInfo.InvariantCulture), 0, float.Parse(t[1], CultureInfo.InvariantCulture)); }).ToArray();
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            while (waypoint < wps.Length - 1 && (wps[waypoint] - pos).magnitude < 12f) waypoint++;
            var target = wps[Math.Min(waypoint, wps.Length - 1)];
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x, ang = Vector3.Angle(vel, d);
            int key = ang < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        static void PlayShot(string name)
        {
            var c = playRuntime.chaseCamera ? playRuntime.chaseCamera : Camera.main;
            if (!c) return;
            const int SW = 1280, SH = 720;
            var target = new RenderTexture(SW, SH, 24);
            var prevTarget = c.targetTexture; c.targetTexture = target;
            c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(SW, SH, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, SW, SH), 0, 0); tex.Apply(); RenderTexture.active = prev;
            c.targetTexture = prevTarget;
            File.WriteAllBytes(playOut + "_" + name + ".png", tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex); target.Release(); UnityEngine.Object.DestroyImmediate(target);
            Debug.Log("[WaterPlay] shot " + name);
        }

        static void PlayFinish(string why)
        {
            EditorApplication.update -= PlayTick;
            if (playRuntime) playRuntime.testHeldKeys = Array.Empty<int>();
            foreach (var (key, had, s, i, f) in savedKeys)
            {
                bool now = PlayerPrefs.HasKey(key);
                bool changed = now != had || (had && (PlayerPrefs.GetString(key, null) != s || PlayerPrefs.GetInt(key, int.MinValue) != i || !PlayerPrefs.GetFloat(key, float.NaN).Equals(f)));
                if (!changed) continue;
                if (!had) PlayerPrefs.DeleteKey(key);
                else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(key, s);
                else if (i != int.MinValue) PlayerPrefs.SetInt(key, i);
                else PlayerPrefs.SetFloat(key, f);
            }
            PlayerPrefs.Save();
            Debug.Log("[WaterPlay] finished: " + why);
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(why == "done" ? 0 : 1);
        }

        // ------------------------------------------------------------------------------------------------ wake step against a recorded frame
        // REFLEX_WT_CAPTURE = a capture with a wake step and a pressure splat (draws REFLEX_WT_WAKE = "sim,pressure,surface"): the step from the
        // recorded TexLast / TexLastLast with the recorded g_SimShift vs the target after it, then the recorded splat vertices vs the wake
        // texture the surface draws sample.
        public static void WakeVerify()
        {
            var log = new StringBuilder(); bool ok = true;
            try
            {
                string cap = Environment.GetEnvironmentVariable("REFLEX_WT_CAPTURE");
                var dr = (Environment.GetEnvironmentVariable("REFLEX_WT_WAKE") ?? "42,43,252").Split(',').Select(int.Parse).ToArray();
                string frame = Environment.GetEnvironmentVariable("REFLEX_WT_FRAME") ?? Directory.GetFiles(Path.Combine(cap, "obj"), "*.bin").Select(Path.GetFileName).First().Split('_')[0];   // e.g. f3185 (tex dumps: the last frame)
                byte[] Obj(int d) => File.ReadAllBytes(Path.Combine(cap, "obj", $"{frame}_d{d:D4}.bin"));
                var sim = Obj(dr[0]); int p0 = ConstantsOffset(sim); int ps = p0 + 256 * 16;
                var shift = new Vector4(BitConverter.ToSingle(sim, ps + 200 * 16), BitConverter.ToSingle(sim, ps + 200 * 16 + 4), BitConverter.ToSingle(sim, ps + 200 * 16 + 8), BitConverter.ToSingle(sim, ps + 200 * 16 + 12));
                // the textures the step sampled (s0 TexLast, s1 TexLastLast: object ids after the constants)
                int texAt = ps + 224 * 16 + 64 * 4 * 2 + 16 * 4 * 2 + 256 * 4;
                uint t0 = BitConverter.ToUInt32(sim, texAt), t1 = BitConverter.ToUInt32(sim, texAt + 4);
                Texture2D Load(string f) { var b = File.ReadAllBytes(f); int w = BitConverter.ToInt32(b, 4), pitch = BitConverter.ToInt32(b, 8), h = BitConverter.ToInt32(b, 12);
                    var t = new Texture2D(w, h, TextureFormat.RFloat, false, true) { wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
                    var px = new float[w * h]; for (int y = 0; y < h; y++) Buffer.BlockCopy(b, 16 + y * pitch, px, y * w * 4, w * 4); t.SetPixelData(px, 0); t.Apply(); return t; }
                float[] LoadImage(string f) { var b = File.ReadAllBytes(f); int w = BitConverter.ToInt32(b, 4), h = BitConverter.ToInt32(b, 8), pitch = BitConverter.ToInt32(b, 12);
                    var px = new float[w * h]; for (int y = 0; y < h; y++) Buffer.BlockCopy(b, 16 + y * pitch, px, y * w * 4, w * 4); return px; }
                var last = Load(Path.Combine(cap, "rt", $"tex_{t0:x8}_d{dr[0]:D4}.raw")); var lastLast = Load(Path.Combine(cap, "rt", $"tex_{t1:x8}_d{dr[0]:D4}.raw"));
                var shader = Shader.Find("Reflex/Original Water");
                var m = new Material(shader);
                var content = ReflexLevelContent.Load(Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "FR_Alpine", new ReflexLevelContent.Options { scenery = false, terrain = false, tracks = false });
                var pressure = typeof(OriginalWater).GetMethod("LoadTextureStatic", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { content, "WaterPressure" }) as Texture2D;
                m.SetTexture("TexLast", last); m.SetTexture("TexLastLast", lastLast); m.SetTexture("TexMask", pressure); m.SetTexture("pressure_texture", pressure);
                m.SetVector("g_SimShift", shift);
                var rt = new RenderTexture(512, 512, 0, RenderTextureFormat.RFloat, RenderTextureReadWrite.Linear) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp };
                var cb = new CommandBuffer(); cb.SetRenderTarget(rt); cb.ClearRenderTarget(false, true, Color.clear);
                cb.DrawProcedural(Matrix4x4.identity, m, 1, MeshTopology.Triangles, 3);
                Graphics.ExecuteCommandBuffer(cb);
                float[] Read(RenderTexture r) { var prev = RenderTexture.active; RenderTexture.active = r; var t = new Texture2D(512, 512, TextureFormat.RFloat, false, true); t.ReadPixels(new Rect(0, 0, 512, 512), 0, 0); t.Apply(); RenderTexture.active = prev; var px = t.GetPixelData<float>(0).ToArray(); UnityEngine.Object.DestroyImmediate(t); return px; }
                string Cmp(float[] a, float[] b) { double sum = 0, mx = 0; int n = 0, exact = 0; for (int i = 0; i < a.Length; i++) { double d = Math.Abs(a[i] - b[i]); sum += d; mx = Math.Max(mx, d); if (b[i] != 0) n++; if (a[i] == b[i]) exact++; } return $"mean |diff| {sum / a.Length:E2}, max {mx:E2}, {exact}/{a.Length} equal, {n} non-zero texels in the recorded target"; }
                var mine = Read(rt); var refSim = LoadImage(Path.Combine(cap, "rt", $"{frame}_wpre{dr[1]:D4}.raw"));
                log.AppendLine($"wake step (shift {shift}): {Cmp(mine, refSim)}");
                // pressure splats: the recorded vertices (already placed by 007ed300), drawn into the stepped target
                var pr = Obj(dr[1]); int nidx = BitConverter.ToInt32(pr, 28), q = 32 + 4 * nidx; int nd = BitConverter.ToInt32(pr, q); q += 4 + 8 * nd;
                int has = BitConverter.ToInt32(pr, q), stride = BitConverter.ToInt32(pr, q + 4), want = BitConverter.ToInt32(pr, q + 12); q += 16;
                int nv = want / stride; var pos = new Vector3[nv]; var uv = new Vector2[nv];
                for (int i = 0; i < nv; i++) { int o = q + i * stride; pos[i] = new Vector3(BitConverter.ToSingle(pr, o), BitConverter.ToSingle(pr, o + 4), BitConverter.ToSingle(pr, o + 8)); uv[i] = new Vector2(BitConverter.ToSingle(pr, o + 12), BitConverter.ToSingle(pr, o + 16)); }
                var mesh = new Mesh { vertices = pos, uv = uv }; mesh.SetTriangles(Enumerable.Range(0, nv - nv % 3).ToArray(), 0); mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 10);
                cb.Clear(); cb.SetRenderTarget(rt); cb.DrawMesh(mesh, Matrix4x4.identity, m, 0, 2); Graphics.ExecuteCommandBuffer(cb);
                var mine2 = Read(rt);
                var surf = Obj(dr[2]); int sps = ConstantsOffset(surf) + 256 * 16; int sTex = sps + 224 * 16 + 64 * 4 * 2 + 16 * 4 * 2 + 256 * 4;
                uint wakeObj = BitConverter.ToUInt32(surf, sTex + 6 * 4);
                var refWake = Load(Path.Combine(cap, "rt", $"tex_{wakeObj:x8}_d{dr[2]:D4}.raw")).GetPixelData<float>(0).ToArray();
                for (int i = 0, k = 0; i < mine2.Length && k < 8; i++) if (mine2[i] != refWake[i] && k++ >= 0) log.AppendLine($"  texel {i % 512},{i / 512}: mine {mine2[i]:R} recorded {refWake[i]:R}");
                log.AppendLine($"pressure ({nv} vertices, first {pos[0]} uv {uv[0]}): {Cmp(mine2, refWake)}; changed texels {mine2.Zip(mine, (a, b) => a != b).Count(x => x)}");
            }
            catch (Exception e) { ok = false; log.AppendLine("FAILED " + e); }
            Debug.Log("[WaterTests] wake\n" + log + (ok ? "done" : "FAIL"));
        }
    }
}
