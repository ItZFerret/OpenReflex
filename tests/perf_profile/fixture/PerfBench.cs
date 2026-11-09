// TEST ONLY (tests/perf_profile, bench.py): the scene benchmark. Loads one event straight from the open front end (or stays in the
// front end's 3D menu hub), holds the game camera on one frozen view and measures every combination of resolution x LIGHTING mode x
// quality tier on that same frame: frame time, CPU main / render thread and GPU time (FrameTimingManager), managed allocations of the
// main thread, the render counters (draw calls, SetPass calls, batches, triangles, shadow casters), the hitch after a switch (shader
// variants compiled on first use), then a breakdown window with GPU / CPU recorders on the passes (ProfilingSampler / Unity markers),
// optional ablations (ENHANCED with one part off) and a live window (the world running: the core simulation step).
// It presses nothing and writes no game state (the save is the run's scratch directory). Plan (REFLEX_PERF_PLAN, JSON):
//   { "scene": "National/NAT_Stillwater" | "menu", "prefix": "national", "ai": 11, "t": 15,
//     "res": [1920, 1080, 2560, 1440], "tiers": [0, 1, 2], "modes": ["original", "enhanced"], "warm": 1.2, "window": 3,
//     "breakdown": 1.5, "live": 5, "ablations": ["noshadow", "noocc", "nocontact", "noscenery", "notrees"],
//     "view": { "cam": "pose", "frame": "sun", "offset": [0, 40, 70], "look": "yawpitch", "yaw": 180, "pitch": -14, "fov": 70 } }
// Output (REFLEX_PREVIEW_SHOTS): <prefix>_perf.json (every window), <prefix>_<res>_<mode>_t<tier>.png (the first resolution only).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Rendering;
using Unity.Profiling;
using Unity.Profiling.LowLevel;
using Unity.Profiling.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.Profiling;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

[DefaultExecutionOrder(32000)]
public sealed class PerfBench : MonoBehaviour
{
    const string Tag = "[perf-bench] ";

    [Serializable] public sealed class View
    {
        public string cam = "game", frame = "bike", look = "anchor";
        public float[] offset = { 0, 3, -8 }; public float lookY = 1f, yaw, pitch, fov;
    }
    [Serializable] public sealed class Plan
    {
        public string scene = "National/NAT_Stillwater", prefix = "bench";
        public int ai; public float t = 15, settle = 3, warm = 1.2f, window = 3, breakdown = 1.5f, live = 5;
        public int[] res = { 1920, 1080 };
        public int[] tiers = { 0, 1, 2 };
        public string[] modes = { "original", "enhanced" };
        public string[] ablations = Array.Empty<string>();
        public bool shots = true;
        public int offscreen = 24;
        public int interleave;   // ablations: this many interleaved with / without offscreen measures each (0: one measure with)
        public bool identity;   // the same frame with every scenery material's _ALPHATEST_ON on (the old discard path) against as built
        public View view = new View();
    }
    [Serializable] public sealed class Window
    {
        public string label, mode; public int width, height, tier; public string variant = "";
        public int frames; public float frameMs, frameP95, frameMax, cpuMs, mainMs, renderMs, gpuMs, gpuP95, presentWaitMs;
        public float gcBytesPerFrame, gcRecorderBytes; public int gcCollections;
        public float drawCalls, setPass, batches, triangles, vertices, shadowCasters;
        public float switchHitchMs;
        // offscreen: the game camera rendered `offscreenFrames` times back to back into a target of the window's size, then one GPU
        // readback: ms per frame = max(CPU, GPU) of the frame without the window server (a background window's drawables / display link
        // hold presented frames; this does not depend on them), the best of three runs
        public float offscreenMs, offscreenCpuMs; public int offscreenFrames;
        public int simJobs; public float simJobMs, simFrameMs, simJobMax;
        // live: the game frames shown per second (distinct PresentedGameFrame values), frames over 25 / 33 ms, the pacing state at the end
        public float gameFps; public int over25, over33; public string pacing = "";
        public List<string> passes = new List<string>();
        public string lighting;
    }
    [Serializable] sealed class Report { public string scene, device, api, gpu; public int vram; public bool gpuRecorder; public List<Window> windows = new List<Window>(); public float loadSeconds; public float loadMaxFrameMs; public float frontEndMaxFrameMs; }

    Plan plan; string dir; readonly Report report = new Report();
    bool posed;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string p = Environment.GetEnvironmentVariable("REFLEX_PERF_PLAN"), d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(p) || string.IsNullOrEmpty(d) || FindAnyObjectByType<PerfBench>()) return;
        var go = new GameObject("Perf bench (test)"); DontDestroyOnLoad(go);
        var s = go.AddComponent<PerfBench>();
        s.plan = JsonUtility.FromJson<Plan>(File.ReadAllText(p)); s.dir = d;
        Application.runInBackground = true;
        AudioListener.volume = 0f;   // the test player is silent (the game's own volume settings are left alone)
        FrameTimingManager.CaptureFrameTimings();
    }

    static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

    IEnumerator Start()
    {
        Directory.CreateDirectory(dir);
        report.scene = plan.scene; report.device = SystemInfo.graphicsDeviceName; report.api = SystemInfo.graphicsDeviceType.ToString();
        report.gpu = SystemInfo.graphicsDeviceVendor + " " + SystemInfo.graphicsDeviceVersion; report.vram = SystemInfo.graphicsMemorySize;
        report.gpuRecorder = SystemInfo.supportsGpuRecorder;
        Debug.Log(Tag + "BEGIN " + plan.scene + " gpu recorder " + SystemInfo.supportsGpuRecorder + " " + report.device + " " + report.api);
        float t0 = Time.realtimeSinceStartup;
        // the front end settles (fonts, the shell, its first renders): at least 4 s and 90 frames in a row under 100 ms (at most 60 s), so the
        // launch's long frames are the level load's own (the front end's first frames compile their shader variants: frontEndMaxFrameMs)
        {
            int calm = 0; float feMax = 0;
            while (Time.realtimeSinceStartup - t0 < 60f && (Time.realtimeSinceStartup - t0 < 4f || calm < 90))
            {
                yield return null;
                float dt = Time.unscaledDeltaTime * 1000f; feMax = Mathf.Max(feMax, dt); calm = dt < 100f ? calm + 1 : 0;
                if (dt > 100f) Debug.Log(Tag + Inv($"FRONT END FRAME {Time.frameCount}: {dt:0} ms, {Time.realtimeSinceStartup - t0:0.00} s after the start"));
            }
            report.frontEndMaxFrameMs = feMax;
            Debug.Log(Tag + Inv($"FRONT END settled after {Time.realtimeSinceStartup - t0:0.0} s (longest frame {feMax:0} ms)"));
        }
        ReflexEnhancedLighting.Override = 0;
        OriginalWorldRuntime rt = null; OriginalLevel level = null; Camera cam = null;
        float wait = Time.realtimeSinceStartup, loadMax = 0;
        bool menu = plan.scene == "menu";
        if (!menu)
        {
            try
            {
                var all = ReflexEvents.All;
                var ev = ReflexEvents.Find(plan.scene) ?? all.FirstOrDefault(e => e.key.StartsWith(plan.scene, StringComparison.OrdinalIgnoreCase));
                if (ev == null) throw new InvalidOperationException("no event " + plan.scene);
                Debug.Log(Tag + "LAUNCH " + ev.key + " ai " + plan.ai);
                // MAP SCALE (REFLEX_MAP_SCALE=<0.5 .. 2>, bench.py --env): the event's track scaled as the setup row does it
                float ms = 1f;
                var mse = Environment.GetEnvironmentVariable("REFLEX_MAP_SCALE");
                if (!string.IsNullOrEmpty(mse)) float.TryParse(mse, NumberStyles.Float, CultureInfo.InvariantCulture, out ms);
                if (ms != 1f) Debug.Log(Tag + Inv($"MAP SCALE {ms * 100f:0} %"));
                ReflexEventSelect.Launch(ev.key, plan.ai, false, "Reflex", 0, null, ms);
            }
            catch (Exception e) { Fail(e.ToString()); yield break; }
            while (true)
            {
                loadMax = Mathf.Max(loadMax, Time.unscaledDeltaTime * 1000f);
                if (Time.unscaledDeltaTime > 0.1f) Debug.Log(Tag + Inv($"LOAD FRAME {Time.frameCount}: {Time.unscaledDeltaTime * 1000f:0} ms, {Time.realtimeSinceStartup - wait:0.00} s after the launch"));
                rt = FindAnyObjectByType<OriginalWorldRuntime>(); level = FindAnyObjectByType<OriginalLevel>();
                if (rt && rt.FailureMessage != null) { Fail("world: " + rt.FailureMessage); yield break; }
                if (rt && level && level.Loaded && rt.Running && !double.IsNaN(rt.PresentedGameFrame)) break;
                if (Time.realtimeSinceStartup - wait > 600) { Fail("the event did not start"); yield break; }
                yield return null;
            }
            report.loadSeconds = Time.realtimeSinceStartup - wait;
            double f0 = rt.PresentedGameFrame;
            while (rt.PresentedGameFrame - f0 < plan.t * 60.0) { HideHud(rt); yield return null; }
            rt.paused = true;
            cam = rt.chaseCamera ? rt.chaseCamera : Camera.main;
        }
        else
        {
            OpenMenuWorld.Still = true;
            while (true)
            {
                loadMax = Mathf.Max(loadMax, Time.unscaledDeltaTime * 1000f);
                var w = OpenMenuWorld.Instance;
                if (w && w.Status == OpenMenuWorld.State.Failed) { Fail("menu world: " + w.Error); yield break; }
                if (w && w.Status == OpenMenuWorld.State.Ready && w.LevelObject && w.LevelObject.Loaded) { level = w.LevelObject; cam = w.Camera; break; }
                if (Time.realtimeSinceStartup - wait > 300) { Fail("the menu world did not start"); yield break; }
                yield return null;
            }
            report.loadSeconds = Time.realtimeSinceStartup - wait;
            float s0 = Time.realtimeSinceStartup;
            while (Time.realtimeSinceStartup - s0 < plan.t) yield return null;   // the walk-in ends, the hub settles
        }
        report.loadMaxFrameMs = loadMax;
        Debug.Log(Tag + Inv($"RUNNING {level.level} load {report.loadSeconds:0.0} s (longest frame {loadMax:0} ms), camera {cam?.transform.position}"));
        if (rt) MapScaleChecks(rt, level);
        posed = plan.view != null && plan.view.cam == "pose";
        if (posed) { poseRt = rt; poseLevel = level; poseCam = cam; }
        { float s0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - s0 < plan.settle) { HideHud(rt); yield return null; } }

        int urpDefaultRes = GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset u0 ? u0.mainLightShadowmapResolution : 0;
        for (int r = 0; r + 1 < plan.res.Length; r += 2)
        {
            int W = plan.res[r], H = plan.res[r + 1];
            Screen.SetResolution(W, H, FullScreenMode.Windowed);
            { float s0 = Time.realtimeSinceStartup; while ((Screen.width != W || Screen.height != H) && Time.realtimeSinceStartup - s0 < 4f) yield return null; }
            { float s0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - s0 < 0.8f) yield return null; }
            Debug.Log(Tag + Inv($"RES asked {W}x{H}, got {Screen.width}x{Screen.height}"));
            foreach (var mode in plan.modes)
                foreach (int tier in plan.tiers)
                {
                    yield return SetMode(mode, tier, urpDefaultRes);
                    var w = new Window { label = $"{W}x{H} {mode} {Tier(tier)}", mode = mode, width = Screen.width, height = Screen.height, tier = tier };
                    yield return Measure(w, rt, false, plan.window, true);
                    if (rt && r == 0 && Environment.GetEnvironmentVariable("REFLEX_MAP_SCALE_SHOTS") == "1") MapScaleShot(rt, cam, mode, W, H);
                    if (plan.offscreen > 0) Offscreen(w, cam, W, H, plan.offscreen, plan.shots && r == 0 ? $"{plan.prefix}_{W}x{H}_{mode}_t{tier}_offscreen.png" : null);
                    if (plan.shots && r == 0 && !Application.isBatchMode)
                    {
                        yield return new WaitForEndOfFrame();
                        ScreenCapture.CaptureScreenshot(Path.Combine(dir, $"{plan.prefix}_{W}x{H}_{mode}_t{tier}.png"));
                        yield return null; yield return null;
                    }
                    if (r == 0 && plan.breakdown > 0)
                    {
                        var b = new Window { label = w.label + " breakdown", mode = mode, width = Screen.width, height = Screen.height, tier = tier, variant = "breakdown" };
                        yield return Measure(b, rt, true, plan.breakdown, false);
                    }
                }
            if (r == 0 && plan.identity)
                foreach (var md in plan.modes)
                {
                    yield return SetMode(md, 2, urpDefaultRes);
                    Identity(md, cam, W, H);
                }
            // ablations: ENHANCED HIGH with one part switched off (the first resolution)
            if (r == 0)
                foreach (var ab0 in plan.ablations)
                {
                    // "o:<what>": the ablation in ORIGINAL HIGH (else ENHANCED HIGH)
                    bool orig = ab0.StartsWith("o:"); string ab = orig ? ab0.Substring(2) : ab0; string md = orig ? "original" : "enhanced";
                    // "t0:" / "t1:" after it: at LOW / MEDIUM instead of HIGH
                    int abTier = 2; if (ab.Length > 3 && ab[0] == 't' && char.IsDigit(ab[1]) && ab[2] == ':') { abTier = ab[1] - '0'; ab = ab.Substring(3); }
                    yield return SetMode(md, abTier, urpDefaultRes);
                    var undo = Ablate(ab);
                    { float s1 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - s1 < 0.4f) yield return null; }
                    var w = new Window { label = $"{W}x{H} {md} {Tier(abTier)} -{ab}", mode = md, width = Screen.width, height = Screen.height, tier = abTier, variant = "-" + ab };
                    yield return Measure(w, rt, false, plan.window, true);
                    if (plan.offscreen > 0) Offscreen(w, cam, W, H, plan.offscreen);
                    if (plan.offscreen > 0 && plan.interleave > 0)
                    {
                        // interleaved in-process A / B (the same frozen frame, the ablation on and off in turns): medians of `interleave`
                        // offscreen measures each; the "control" window is the frame as built measured between them
                        var on = new List<float>(); var off = new List<float>();
                        var c = new Window { label = $"{W}x{H} {md} {Tier(abTier)} control", mode = md, width = Screen.width, height = Screen.height, tier = abTier, variant = "-" + ab + " control" };
                        for (int k = 0; k < plan.interleave; k++)
                        {
                            undo(); yield return null; Offscreen(c, cam, W, H, plan.offscreen); off.Add(c.offscreenMs);
                            undo = Ablate(ab); yield return null; Offscreen(w, cam, W, H, plan.offscreen); on.Add(w.offscreenMs);
                        }
                        on.Sort(); off.Sort(); w.offscreenMs = on[on.Count / 2]; c.offscreenMs = off[off.Count / 2];
                        report.windows.Add(c);
                        Debug.Log(Tag + Inv($"INTERLEAVED {md} {ab}: {w.offscreenMs:0.000} ms with, {c.offscreenMs:0.000} ms as built (medians of {plan.interleave}; with {string.Join(" ", on.Select(x => x.ToString("0.00", CultureInfo.InvariantCulture)))}; built {string.Join(" ", off.Select(x => x.ToString("0.00", CultureInfo.InvariantCulture)))})"));
                    }
                    // a shader swap (a test variant of a shipped shader): the same frame with the variant and as built, compared
                    // (and a static switch of the build: "static=<type>.<field>=<value>", e.g. the shadow receivers' early out off)
                    byte[] withSwap = ab.StartsWith("swap=") || ab.StartsWith("static=") || ab.StartsWith("kwoff=") || ab.StartsWith("softq=") ? Shot(cam, W, H) : null;
                    undo();
                    if (withSwap != null) CompareShots(withSwap, Shot(cam, W, H), W, H, $"{md} {Tier(abTier)} {ab}", $"cmp_{plan.prefix}_{md}_t{abTier}_{new string(ab.Select(ch => char.IsLetterOrDigit(ch) ? ch : '_').ToArray())}");
                }
        }
        // live: the world runs (the core's frames, the opponents, the camera) at the first resolution
        if (rt && plan.live > 0)
        {
            Screen.SetResolution(plan.res[0], plan.res[1], FullScreenMode.Windowed);
            { float s0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - s0 < 1f) yield return null; }
            posed = false;
            foreach (var mode in plan.modes)
            {
                yield return SetMode(mode, 2, urpDefaultRes);
                rt.paused = false;
                Debug.Log(Tag + "LIVE BEGIN " + mode);
                var w = new Window { label = $"{Screen.width}x{Screen.height} {mode} HIGH live", mode = mode, width = Screen.width, height = Screen.height, tier = 2, variant = "live" };
                yield return Measure(w, rt, plan.breakdown > 0, plan.live, true);   // a development build: the live frame's markers too
                rt.paused = true;
            }
        }
        File.WriteAllText(Path.Combine(dir, plan.prefix + "_perf.json"), JsonUtility.ToJson(report, true));
        Debug.Log(Tag + "DONE");
        yield return new WaitForSeconds(0.5f);
        Application.Quit(0);
    }

    // MAP SCALE checks (core/plan/MAP_SCALE.md): the scale the world was made at on both sides; the Unity terrain (built from the core's
    // heights) against the core's own height export at points around the bike; the scaled scenery against the core's model matrices
    // (OriginalSceneryModels: instances whose game matrix differs from the level's moved by the scale); the bike on the ground.
    static void MapScaleChecks(OriginalWorldRuntime rt, OriginalLevel level)
    {
        try
        {
            float coreScale = rt.SimDriver?.Open?.MapScale ?? 1f;
            var terrain = rt.terrain ? rt.terrain.UnityTerrain : null;
            var bike = rt.Bike; var bp = new Vector3(bike.position[0], bike.position[1], bike.position[2]);
            int pts = 0, ok = 0; float worst = 0;
            if (terrain)
                for (int dz = -10; dz <= 10; dz += 5)
                    for (int dx = -10; dx <= 10; dx += 5)
                    {
                        var p = bp + new Vector3(dx, 0, dz) * 6f;
                        float unity = terrain.SampleHeight(p) + terrain.transform.position.y;
                        float core = rt.terrain.CoreHeightAt(p.x, p.z, out bool have);
                        if (!have) continue;
                        pts++; float e = Mathf.Abs(unity - core); worst = Mathf.Max(worst, e); if (e < 0.5f) ok++;
                    }
            float ground = terrain ? terrain.SampleHeight(bp) + terrain.transform.position.y : float.NaN;
            var sm = rt.SceneryModels;
            Debug.Log(Tag + Inv($"MAPSCALE level {level.MapScale:0.00} core {coreScale:0.00} root {(level.SceneryRoot ? level.SceneryRoot.transform.localScale.x : 0f):0.00} terrain {ok}/{pts} within 0.5 (worst {worst:0.00}) bike {bp} above ground {bp.y - ground:0.00} scenery differing {sm?.Differing}/{sm?.Compared} spacing {rt.terrain?.Info.spacing:0.00}"));
        }
        catch (Exception e) { Debug.Log(Tag + "MAPSCALE check failed: " + e.Message); }
    }

    // MAP SCALE shots: the game camera put behind and above the player's bike (real-size offsets, along the bike's heading), so the same
    // view at every scale shows the track around a bike of the same size: mapscale_<prefix>_<percent>_<mode>.png (rows flipped upright)
    void MapScaleShot(OriginalWorldRuntime rt, Camera cam, string mode, int W, int H)
    {
        try
        {
            var b = rt.Bike; var bp = new Vector3(b.position[0], b.position[1], b.position[2]);
            var q = new Quaternion(b.orientation[0], b.orientation[1], b.orientation[2], b.orientation[3]);
            var fwd = q * Vector3.forward; fwd.y = 0; fwd = fwd.sqrMagnitude > 1e-4f ? fwd.normalized : Vector3.forward;
            string axis = Environment.GetEnvironmentVariable("REFLEX_MAP_SCALE_SHOT_AXIS");
            if (axis == "x") { fwd = q * Vector3.right; fwd.y = 0; fwd.Normalize(); } else if (axis == "-x") { fwd = q * Vector3.left; fwd.y = 0; fwd.Normalize(); } else if (axis == "-z") fwd = -fwd;
            var t = cam.transform; var p0 = t.position; var r0 = t.rotation; float f0 = cam.fieldOfView;
            float back = 38f, up = 30f, ahead = 70f;   // REFLEX_MAP_SCALE_SHOT_VIEW=back,up,ahead (feet, real size)
            var v = (Environment.GetEnvironmentVariable("REFLEX_MAP_SCALE_SHOT_VIEW") ?? "").Split(',');
            if (v.Length == 3) { float.TryParse(v[0], NumberStyles.Float, CultureInfo.InvariantCulture, out back); float.TryParse(v[1], NumberStyles.Float, CultureInfo.InvariantCulture, out up); float.TryParse(v[2], NumberStyles.Float, CultureInfo.InvariantCulture, out ahead); }
            t.position = bp - fwd * back + Vector3.up * up;
            t.LookAt(bp + fwd * ahead);
            cam.fieldOfView = 62f;
            var px = Shot(cam, W, H);
            t.SetPositionAndRotation(p0, r0); cam.fieldOfView = f0;
            if (px == null) { Debug.Log(Tag + "MAPSCALE shot: no readback"); return; }
            var flipped = new byte[px.Length]; int row = W * 4;
            for (int y = 0; y < H; y++) Buffer.BlockCopy(px, y * row, flipped, (H - 1 - y) * row, row);
            string pct = (rt.SimDriver?.Open?.MapScale ?? 1f).ToString("0.00", CultureInfo.InvariantCulture).Replace(".", "");
            var tex = new Texture2D(W, H, TextureFormat.RGBA32, false); tex.SetPixelData(flipped, 0); tex.Apply(false);
            File.WriteAllBytes(Path.Combine(dir, $"mapscale_{plan.prefix}_{pct}_{mode}.png"), tex.EncodeToPNG()); Destroy(tex);
            Debug.Log(Tag + Inv($"MAPSCALE shot {plan.prefix} {pct} {mode} from {t.position} heading {fwd}"));
        }
        catch (Exception e) { Debug.Log(Tag + "MAPSCALE shot failed: " + e.Message); }
    }

    void Offscreen(Window w, Camera cam, int W, int H, int frames, string shotName = null)
    {
        if (!cam) return;
        var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
        var prior = cam.targetTexture;
        try
        {
            cam.targetTexture = rtx;
            for (int i = 0; i < 3; i++) cam.Render();
            AsyncGPUReadback.Request(rtx, 0, 0, 1, 0, 1, 0, 1).WaitForCompletion();
            double best = double.MaxValue, bestCpu = 0;
            for (int run = 0; run < 3; run++)
            {
                var sw = System.Diagnostics.Stopwatch.StartNew();
                for (int i = 0; i < frames; i++) cam.Render();
                double cpu = sw.Elapsed.TotalMilliseconds / frames;   // the main thread's part (culling, recording; the render thread runs behind)
                AsyncGPUReadback.Request(rtx, 0, 0, 1, 0, 1, 0, 1).WaitForCompletion();
                double t = sw.Elapsed.TotalMilliseconds / frames;
                if (t < best) { best = t; bestCpu = cpu; }
            }
            w.offscreenMs = (float)best; w.offscreenCpuMs = (float)bestCpu; w.offscreenFrames = frames;
            Debug.Log(Tag + Inv($"OFFSCREEN {w.label} {w.variant}: {best:0.00} ms per frame, main thread {bestCpu:0.00} ms ({frames} frames x 3, {W}x{H})"));
            if (shotName != null)
            {
                var req = AsyncGPUReadback.Request(rtx, 0, TextureFormat.RGBA32); req.WaitForCompletion();
                if (!req.hasError)
                {
                    var tex = new Texture2D(W, H, TextureFormat.RGBA32, false);
                    tex.SetPixelData(req.GetData<byte>(), 0); tex.Apply(false);
                    File.WriteAllBytes(Path.Combine(dir, shotName), tex.EncodeToPNG()); Destroy(tex);
                }
            }
        }
        catch (Exception e) { Debug.LogWarning(Tag + "offscreen: " + e.Message); }
        finally { cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx); }
    }

    static byte[] Shot(Camera cam, int W, int H)
    {
        var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
        var prior = cam.targetTexture; cam.targetTexture = rtx;
        for (int i = 0; i < 2; i++) cam.Render();
        var req = AsyncGPUReadback.Request(rtx, 0, TextureFormat.RGBA32); req.WaitForCompletion();
        cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx);
        return req.hasError ? null : req.GetData<byte>().ToArray();
    }

    // per channel value differences of two renders of the same frozen frame; both images (and a x16 difference) saved when they differ
    void CompareShots(byte[] a, byte[] b, int W, int H, string label, string file)
    {
        if (a == null || b == null || a.Length != b.Length) { Debug.Log(Tag + "COMPARE " + label + ": no readback"); return; }
        int diff = 0, max = 0, px = 0; long sum = 0;
        var d = new byte[a.Length];
        for (int i = 0; i < a.Length; i += 4)
        {
            int m = 0;
            for (int c = 0; c < 3; c++) { int x = Math.Abs(a[i + c] - b[i + c]); if (x > 0) diff++; sum += x; if (x > m) m = x; d[i + c] = (byte)Math.Min(255, x * 16); }
            d[i + 3] = 255; if (m > 0) px++; if (m > max) max = m;
        }
        Debug.Log(Tag + Inv($"COMPARE {label}: {px} of {W * H} pixels differ ({100.0 * px / (W * H):0.000} %), {diff} channel values, max {max}, mean {sum / (3.0 * W * H):0.0000}"));
        if (px == 0) return;
        void Save(byte[] p, string n) { var t = new Texture2D(W, H, TextureFormat.RGBA32, false); t.SetPixelData(p, 0); t.Apply(false); File.WriteAllBytes(Path.Combine(dir, n), t.EncodeToPNG()); Destroy(t); }
        Save(a, file + "_variant.png"); Save(b, file + "_built.png"); Save(d, file + "_diff_x16.png");
    }

    // Renders the frozen frame twice: as built, then with _ALPHATEST_ON on every Reflex/Original Scenery material (the pre-w-perf discard
    // path in every program, a no-op where _AlphaTest is 0); logs the per-pixel difference (it must be 0).
    void Identity(string mode, Camera cam, int W, int H)
    {
        byte[] Shot()
        {
            var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
            var prior = cam.targetTexture; cam.targetTexture = rtx;
            for (int i = 0; i < 2; i++) cam.Render();
            var req = AsyncGPUReadback.Request(rtx, 0, TextureFormat.RGBA32); req.WaitForCompletion();
            cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx);
            return req.hasError ? null : req.GetData<byte>().ToArray();
        }
        var scenery = Shader.Find("Reflex/Original Scenery");
        var changed = new List<Material>(); var seen = new HashSet<Material>();
        var a = Shot(); var a2 = Shot();   // control: the same frame twice, nothing changed
        int noise = 0; if (a != null && a2 != null) for (int i = 0; i < a.Length; i++) if (a[i] != a2[i]) noise++;
        Debug.Log(Tag + Inv($"IDENTITY {mode} control: {noise} channel values differ between two renders of the same frame"));
        foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
            foreach (var m in r.sharedMaterials)
                if (m && m.shader == scenery && seen.Add(m) && !m.IsKeywordEnabled("_ALPHATEST_ON")) { m.EnableKeyword("_ALPHATEST_ON"); changed.Add(m); }
        var b = Shot();
        int tested = 0; var names = new List<string>();
        foreach (var m in changed) if (m && m.GetFloat("_AlphaTest") > 0.5f) { tested++; if (names.Count < 12) names.Add(m.name); }
        Debug.Log(Tag + Inv($"IDENTITY {mode}: {tested} of the switched materials have _AlphaTest on without the keyword: {string.Join(", ", names)}"));
        foreach (var m in changed) if (m) m.DisableKeyword("_ALPHATEST_ON");
        if (a == null || b == null || a.Length != b.Length) { Debug.Log(Tag + "IDENTITY " + mode + ": no readback"); return; }
        int diff = 0, max = 0;
        for (int i = 0; i < a.Length; i++) { int d = Math.Abs(a[i] - b[i]); if (d > 0) diff++; if (d > max) max = d; }
        void Save(byte[] px, string n) { var t = new Texture2D(W, H, TextureFormat.RGBA32, false); t.SetPixelData(px, 0); t.Apply(false); File.WriteAllBytes(Path.Combine(dir, n), t.EncodeToPNG()); Destroy(t); }
        Save(a, $"identity_{plan.prefix}_{mode}_built.png"); Save(b, $"identity_{plan.prefix}_{mode}_discard.png");
        Debug.Log(Tag + Inv($"IDENTITY {mode} HIGH: {changed.Count} materials switched to the discard path, {diff} of {a.Length} channel values differ (max {max})"));
    }

    static string Tier(int t) => t == 0 ? "LOW" : t == 1 ? "MEDIUM" : "HIGH";

    float lastSwitchHitch;
    IEnumerator SetMode(string mode, int tier, int urpDefaultRes)
    {
        // SHADOW QUALITY: the graphics.cfg value (both modes: the ORIGINAL shadow map size ReflexRenderPipeline makes from the game's table,
        // the ENHANCED tier) changed live as a start with that value would have it
        var ls = ReflexLaunchSettings.Current; ls.shadowQuality = tier;
        ReflexEnhancedLighting.TierOverride = tier;
        bool enh = mode == "enhanced";
        ReflexEnhancedLighting.Override = enh ? 1 : 0;
        yield return null; yield return null;   // applied in LateUpdate
        if (!enh && GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp)
        {
            var sz = ls.ShadowSizes;
            urp.mainLightShadowmapResolution = Mathf.Clamp(Mathf.NextPowerOfTwo(Mathf.Max(Mathf.Max(sz[0], sz[1]), Mathf.Max(sz[2], sz[3]))), 256, 8192);
        }
        float worst = 0, s0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - s0 < plan.warm) { yield return null; worst = Mathf.Max(worst, Time.unscaledDeltaTime * 1000f); }
        lastSwitchHitch = worst;
    }

    static readonly string[] RenderCounters = { "Draw Calls Count", "SetPass Calls Count", "Batches Count", "Triangles Count", "Vertices Count", "Shadow Casters Count" };

    List<(string name, Recorder r)> recorders;
    void EnableRecorders(bool on)
    {
        if (recorders == null)
        {
            recorders = new List<(string, Recorder)>();
            var handles = new List<ProfilerRecorderHandle>(); ProfilerRecorderHandle.GetAvailable(handles);
            var names = new HashSet<string>();
            foreach (var h in handles)
            {
                var d = ProfilerRecorderHandle.GetDescription(h);
                if ((d.Flags & MarkerFlags.Counter) != 0 || d.UnitType != ProfilerMarkerDataUnit.TimeNanoseconds) continue;
                names.Add(d.Name);
            }
            foreach (var n in names) { var r = Recorder.Get(n); if (r != null && r.isValid) recorders.Add((n, r)); }
            Debug.Log(Tag + "recorders " + recorders.Count + " of " + names.Count);
        }
        foreach (var (_, r) in recorders) r.enabled = on;
    }

    IEnumerator Measure(Window w, OriginalWorldRuntime rt, bool breakdown, float seconds, bool counters)
    {
        w.switchHitchMs = lastSwitchHitch;
        var enh = FindAnyObjectByType<ReflexEnhancedLighting>();
        w.lighting = enh ? enh.report : "";
        var rec = new List<ProfilerRecorder>();
        if (counters) foreach (var n in RenderCounters) rec.Add(ProfilerRecorder.StartNew(ProfilerCategory.Render, n));
        var gcRec = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame");
        var sums = new double[rec.Count];
        var frame = new List<float>(); var gpu = new List<float>();
        double cpu = 0, main = 0, render = 0, wait = 0; int timed = 0; double gcRecSum = 0; int gcRecN = 0;
        var cpuSum = new Dictionary<string, double>(); var gpuSum = new Dictionary<string, double>();
        if (breakdown) EnableRecorders(true);
        yield return null; yield return null;
        var timings = new FrameTiming[1];
        long gc0 = GC.GetAllocatedBytesForCurrentThread(); int coll0 = GC.CollectionCount(0);
        double lastSim = double.NaN; double jobMs = 0, simMs = 0, jobMax = 0; int jobs = 0;
        double lastShown = double.NaN; int shown = 0;
        float t0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - t0 < seconds)
        {
            if (rt) HideHud(rt);
            yield return null;
            frame.Add(Time.unscaledDeltaTime * 1000f);
            if (rt && !rt.paused) { double g = rt.PresentedGameFrame; if (!double.IsNaN(g)) { if (!double.IsNaN(lastShown) && g > lastShown) shown += (int)Math.Round(g - lastShown); lastShown = g; } }
            FrameTimingManager.CaptureFrameTimings();
            if (FrameTimingManager.GetLatestTimings(1, timings) > 0 && timings[0].cpuFrameTime > 0)
            {
                cpu += timings[0].cpuFrameTime; main += timings[0].cpuMainThreadFrameTime; render += timings[0].cpuRenderThreadFrameTime;
                wait += timings[0].cpuMainThreadPresentWaitTime; timed++;
                if (timings[0].gpuFrameTime > 0) gpu.Add((float)timings[0].gpuFrameTime);
            }
            for (int i = 0; i < rec.Count; i++) sums[i] += rec[i].LastValue;
            if (gcRec.Valid) { gcRecSum += gcRec.LastValue; gcRecN++; }
            if (breakdown)
                foreach (var (n, r) in recorders)
                {
                    double c = r.elapsedNanoseconds / 1e6, g = SystemInfo.supportsGpuRecorder ? r.gpuElapsedNanoseconds / 1e6 : 0;
                    if (c > 0) cpuSum[n] = (cpuSum.TryGetValue(n, out var a) ? a : 0) + c;
                    if (g > 0) gpuSum[n] = (gpuSum.TryGetValue(n, out var b) ? b : 0) + g;
                }
            var nw = rt ? rt.Presentation?.Newest : null;
            if (nw != null && nw.simTime != lastSim && !rt.paused)
            {
                if (!double.IsNaN(lastSim)) { jobs++; jobMs += nw.milliseconds; simMs += nw.frameMilliseconds; jobMax = Math.Max(jobMax, nw.milliseconds); }
                lastSim = nw.simTime;
            }
        }
        long gc1 = GC.GetAllocatedBytesForCurrentThread();
        if (breakdown) EnableRecorders(false);
        int nF = Mathf.Max(1, frame.Count);
        w.over25 = frame.Count(x => x > 25f); w.over33 = frame.Count(x => x > 33.4f);
        w.gameFps = shown / Mathf.Max(0.001f, frame.Sum() / 1000f);
        if (rt) w.pacing = rt.Pacing + (rt.Pacing == OriginalWorldRuntime.PacingMode.Original ? (rt.SyncFrames ? " same-frame" : " one-frame-later") : "") + Inv($" vsync {QualitySettings.vSyncCount} target {Application.targetFrameRate}");
        frame.Sort(); gpu.Sort();
        w.frames = frame.Count;
        w.frameMs = frame.Count > 0 ? frame.Average() : 0; w.frameP95 = frame.Count > 0 ? frame[(int)(frame.Count * 0.95f)] : 0; w.frameMax = frame.Count > 0 ? frame[frame.Count - 1] : 0;
        if (timed > 0) { w.cpuMs = (float)(cpu / timed); w.mainMs = (float)(main / timed); w.renderMs = (float)(render / timed); w.presentWaitMs = (float)(wait / timed); }
        if (gpu.Count > 0) { w.gpuMs = gpu.Average(); w.gpuP95 = gpu[(int)(gpu.Count * 0.95f)]; }
        w.gcBytesPerFrame = (gc1 - gc0) / (float)nF; w.gcCollections = GC.CollectionCount(0) - coll0;
        w.gcRecorderBytes = gcRecN > 0 ? (float)(gcRecSum / gcRecN) : -1;
        if (counters) { w.drawCalls = (float)(sums[0] / nF); w.setPass = (float)(sums[1] / nF); w.batches = (float)(sums[2] / nF); w.triangles = (float)(sums[3] / nF); w.vertices = (float)(sums[4] / nF); w.shadowCasters = (float)(sums[5] / nF); }
        foreach (var r in rec) r.Dispose(); gcRec.Dispose();
        if (jobs > 0) { w.simJobs = jobs; w.simJobMs = (float)(jobMs / jobs); w.simFrameMs = (float)(simMs / jobs); w.simJobMax = (float)jobMax; }
        if (breakdown)
        {
            foreach (var kv in gpuSum.OrderByDescending(k => k.Value).Take(40)) if (kv.Value / nF >= 0.01) w.passes.Add(Inv($"GPU {kv.Value / nF,7:0.000} ms  {kv.Key}"));
            foreach (var kv in cpuSum.OrderByDescending(k => k.Value).Take(70)) if (kv.Value / nF >= 0.03) w.passes.Add(Inv($"CPU {kv.Value / nF,7:0.000} ms  {kv.Key}"));
        }
        report.windows.Add(w);
        Debug.Log(Tag + Inv($"WINDOW {w.label}{(w.variant.Length > 0 && w.variant != "breakdown" ? " " + w.variant : "")}: {w.frames} frames, frame {w.frameMs:0.00} (p95 {w.frameP95:0.00}, max {w.frameMax:0.0}), cpu {w.cpuMs:0.00} main {w.mainMs:0.00} render {w.renderMs:0.00} gpu {w.gpuMs:0.00} (p95 {w.gpuP95:0.00}) ms, gc {w.gcBytesPerFrame:0} B/frame ({w.gcCollections} gc), draws {w.drawCalls:0} setpass {w.setPass:0} batches {w.batches:0} tris {w.triangles / 1e6:0.00} M casters {w.shadowCasters:0}, hitch {w.switchHitchMs:0} ms, game {w.gameFps:0.0} fps, >25 ms {w.over25}, >33 ms {w.over33}, {w.pacing}, sim {w.simJobs} jobs {w.simJobMs:0.00} ms (core {w.simFrameMs:0.00}, max {w.simJobMax:0.0})"));
        foreach (var p in w.passes) Debug.Log(Tag + "  " + p);
    }

    // ENHANCED with one part off (the costs of the parts)
    static Action Ablate(string what)
    {
        var undo = new List<Action>();
        var e = FindAnyObjectByType<ReflexRenderPipeline>()?.enhanced;
        foreach (var p in FindObjectsByType<ReflexRenderPipeline>(FindObjectsSortMode.None)) if (p.enhanced != null) e = p.enhanced;
        switch (what)
        {
            case "noshadow":
                if (RenderSettings.sun) { var sun = RenderSettings.sun; var was = sun.shadows; sun.shadows = LightShadows.None; undo.Add(() => sun.shadows = was); }
                break;
            case "nocontact": if (e != null) { bool was = e.contact; e.contact = false; undo.Add(() => e.contact = was); } break;
            case "noocc": if (e != null) { bool was = e.occlusion; e.occlusion = false; undo.Add(() => e.occlusion = was); } break;
            case "notrees":
                foreach (var f in FindObjectsByType<OriginalForest>(FindObjectsSortMode.None)) { var ff = f; bool was = ff.castShadows; ff.castShadows = false; undo.Add(() => ff.castShadows = was); }
                break;
            case "noscenery":
            {
                var scenery = Shader.Find("Reflex/Original Scenery");
                foreach (var r in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None))
                {
                    if (r.shadowCastingMode == ShadowCastingMode.Off || !r.sharedMaterial || r.sharedMaterial.shader != scenery) continue;
                    var rr = r; var was = rr.shadowCastingMode; rr.shadowCastingMode = ShadowCastingMode.Off; undo.Add(() => { if (rr) rr.shadowCastingMode = was; });
                }
                break;
            }
            case "noopaque":   // URP's opaque colour copy (nothing of the game reads _CameraOpaqueTexture)
                if (GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset uo) { bool was = uo.supportsCameraOpaqueTexture; uo.supportsCameraOpaqueTexture = false; undo.Add(() => uo.supportsCameraOpaqueTexture = was); }
                break;
            case "nodepthtex":   // URP's depth copy (the soft particles read it: a cost measurement only)
                if (GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset ud) { bool was = ud.supportsCameraDepthTexture; ud.supportsCameraDepthTexture = false; undo.Add(() => ud.supportsCameraDepthTexture = was); }
                break;
            case "noalivevm":      // the scenery's whole Alive programs (general ops) drawn without their program (cost of the interpreter)
            case "noalivevmall":   // every Alive program (albedo ones too)
            {
                var seen = new HashSet<Material>();
                foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
                    foreach (var m in r.sharedMaterials)
                    {
                        if (!m || !seen.Add(m)) continue;
                        // before w-perf one keyword (_ALIVE_VM) for both kinds; after it _ALIVE_VM_GENERAL for the whole programs
                        string kw = m.IsKeywordEnabled("_ALIVE_VM_GENERAL") ? "_ALIVE_VM_GENERAL" : m.IsKeywordEnabled("_ALIVE_VM") ? "_ALIVE_VM" : null;
                        if (kw == null) continue;
                        if (what == "noalivevm" && !(m.HasProperty("_AliveVMInfo2") && m.GetVector("_AliveVMInfo2").x > 0.5f)) continue;
                        var mm = m; mm.DisableKeyword(kw); undo.Add(() => { if (mm) mm.EnableKeyword(kw); });
                    }
                break;
            }
            case "nopost":   // the HDR post chain off (the raw HDR image clamped): its cost
                foreach (var pp in FindObjectsByType<ReflexRenderPipeline>(FindObjectsSortMode.None)) { var q = pp; bool was = q.postProcess; q.postProcess = false; undo.Add(() => q.postProcess = was); }
                break;
            case "hide-terrain":
                foreach (var t in FindObjectsByType<Terrain>(FindObjectsSortMode.None)) { var tt = t; bool was = tt.drawHeightmap; tt.drawHeightmap = false; undo.Add(() => tt.drawHeightmap = was); }
                break;
            case "hide-forest":   // the trees, billboards and grass (OriginalForest draws them)
                foreach (var f in FindObjectsByType<OriginalForest>(FindObjectsSortMode.None)) { var ff = f; bool was = ff.enabled; ff.enabled = false; undo.Add(() => ff.enabled = was); }
                break;
            default:
                if (what.StartsWith("hide-"))   // renderers whose material's shader name contains the rest ("hide-Water", "hide-Scenery", "hide-Crowd")
                {
                    string key = what.Substring(5);
                    foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
                    {
                        if (r.forceRenderingOff || !r.sharedMaterial || !r.sharedMaterial.shader || r.sharedMaterial.shader.name.IndexOf(key, StringComparison.OrdinalIgnoreCase) < 0) continue;
                        var rr = r; rr.forceRenderingOff = true; undo.Add(() => { if (rr) rr.forceRenderingOff = false; });
                    }
                }
                break;
            case string sw when sw.StartsWith("swap="):
            {
                // "swap=<shipped shader>=<test variant>": every material of the shipped shader drawn with the variant (Resources/PerfBenchShaders)
                var parts = what.Split('='); var from = Shader.Find(parts[1]);
                var to = Resources.LoadAll<Shader>("PerfBenchShaders").FirstOrDefault(x => x.name == parts[2]);
                if (!from || !to) { Debug.LogWarning(Tag + "swap: shader missing " + what + " " + (bool)from + " " + (bool)to); break; }
                var seen = new HashSet<Material>();
                void SwapMat(Material m) { if (m && m.shader == from && seen.Add(m)) { var mm = m; mm.shader = to; undo.Add(() => { if (mm) mm.shader = from; }); } }
                foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None)) foreach (var m in r.sharedMaterials) SwapMat(m);
                foreach (var t in FindObjectsByType<Terrain>(FindObjectsSortMode.None)) SwapMat(t.materialTemplate);
                break;
            }
            case string kw when kw.StartsWith("kwoff="):
            {
                // "kwoff=<shader>=<keyword prefix>": the materials of the shader drawn with those (local) keywords off
                var parts = what.Split('='); var sh = Shader.Find(parts[1]);
                if (!sh) { Debug.LogWarning(Tag + "kwoff: no shader " + parts[1]); break; }
                var seen = new HashSet<Material>();
                foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
                    foreach (var m in r.sharedMaterials)
                    {
                        if (!m || m.shader != sh || !seen.Add(m)) continue;
                        foreach (var k in m.shaderKeywords)
                            if (k.StartsWith(parts[2])) { var mm = m; var kk = k; mm.DisableKeyword(kk); undo.Add(() => { if (mm) mm.EnableKeyword(kk); }); }
                    }
                break;
            }
            case string sq when sq.StartsWith("softq="):
            {
                // "softq=<1 low | 2 medium | 3 high>": the sun's soft shadow filter (ENHANCED sets it per tier)
                var sun = RenderSettings.sun; var ld = sun ? sun.GetComponent<UniversalAdditionalLightData>() : null;
                if (!ld) { Debug.LogWarning(Tag + "softq: no sun light data"); break; }
                var was = ld.softShadowQuality; ld.softShadowQuality = (SoftShadowQuality)int.Parse(what.Substring(6), CultureInfo.InvariantCulture);
                undo.Add(() => { if (ld) ld.softShadowQuality = was; });
                break;
            }
            case string st when st.StartsWith("static="):
            {
                // "static=<namespace.type>.<field>=<true|false|number>": a static field of the build set for the window (reflection: the
                // fixture compiles against builds that lack it; a missing field is logged and skipped)
                var parts = what.Split('='); int dot = parts[1].LastIndexOf('.');
                var type = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(parts[1].Substring(0, dot))).FirstOrDefault(t => t != null);
                var f = type?.GetField(parts[1].Substring(dot + 1), System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic);
                if (f == null) { Debug.LogWarning(Tag + "static: no field " + parts[1]); break; }
                var was = f.GetValue(null);
                f.SetValue(null, Convert.ChangeType(parts[2], f.FieldType, CultureInfo.InvariantCulture));
                undo.Add(() => f.SetValue(null, was));
                break;
            }
            case "cascades2":
                if (GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp) { int was = urp.shadowCascadeCount; urp.shadowCascadeCount = 2; undo.Add(() => urp.shadowCascadeCount = was); }
                break;
        }
        Debug.Log(Tag + "ABLATE " + what + ": " + undo.Count + " switches");
        return () => { for (int i = undo.Count - 1; i >= 0; i--) undo[i](); };
    }

    static void HideHud(OriginalWorldRuntime rt)
    {
        Reflex.Port.Options.ReflexSettings.HudHidden = true;
        if (rt) { if (rt.OriginalHudVisible) rt.OriginalHudVisible = false; if (rt.HudVisible) rt.HudVisible = false; }
    }

    OriginalWorldRuntime poseRt; OriginalLevel poseLevel; Camera poseCam;
    void LateUpdate()
    {
        AudioListener.volume = 0f;   // (kept silent: a scene load or the options may set it again)
        if (!posed || !poseCam || !poseLevel) return;
        var v = plan.view;
        Vector3 anchor = poseRt && poseRt.chassis ? poseRt.chassis.position : Vector3.zero, fwd;
        if (v.frame == "sun") { var s = poseLevel.Content.lighting.sunDirection; fwd = new Vector3(s.x, 0, s.z).normalized; }
        else if (v.frame == "world") fwd = Vector3.forward;
        else { fwd = poseRt && poseRt.chassis ? poseRt.chassis.forward : Vector3.forward; fwd.y = 0; fwd = fwd.sqrMagnitude > 1e-4f ? fwd.normalized : Vector3.forward; }
        var rot = Quaternion.LookRotation(fwd, Vector3.up);
        Vector3 pos = anchor + rot * new Vector3(v.offset[0], v.offset[1], v.offset[2]);
        var terrain = poseLevel.terrain ? poseLevel.terrain.UnityTerrain : null;
        if (terrain) { float g = terrain.SampleHeight(pos) + terrain.transform.position.y + 1.2f; if (pos.y < g) pos.y = g; }
        Quaternion look = v.look == "anchor" ? Quaternion.LookRotation((anchor + Vector3.up * v.lookY) - pos, Vector3.up) : rot * Quaternion.Euler(-v.pitch, v.yaw, 0);
        poseCam.transform.SetPositionAndRotation(pos, look);
        if (v.fov > 1) poseCam.fieldOfView = v.fov;
    }

    void Fail(string why)
    {
        Debug.LogError(Tag + "FAIL " + why);
        File.WriteAllText(Path.Combine(dir, plan.prefix + "_perf.json"), JsonUtility.ToJson(report, true));
        Application.Quit(3);
    }
}
