using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Tests
{
    // In-level video screens and ad slots (_re/notes/screens-ads.md):
    //   Verify  every level with VideoEntity (REFLEX_SCREENS_LEVELS to restrict): the screens' videos open in the install's binkw32.dll, their
    //           size / frames / rate equal the .bik header, no sound track, a target header (A8R8G8B8 w x h) and the materials that sample it
    //           with a program the port draws; decode cost per frame; every MassiveAdEntity's "Ad Texture" against its material.
    //   Render  RenderCompareTests.Render with the screens decoded to REFLEX_SCREENS_FRAMES ("<entity id>=<frame>,...", default frame 1 of
    //           every screen; "<frame>" alone for all) and their targets written to <REFLEX_RC_OUT>_target_<entity>.png.
    //   Play    a race (REFLEX_SCREENS_EVENT, default Supercross/SX_Kingston) in play mode for REFLEX_SCREENS_SECONDS (default 40): frames
    //           decoded / shown, decode ms on the decode thread, main-thread ms, the port's pause from 15 to 20 s (the shown frames must hold).
    public static class ScreensTests
    {
        static readonly StringBuilder log = new StringBuilder();
        static int failures;
        static void Check(bool ok, string what) { if (!ok) failures++; log.AppendLine((ok ? "  ok   " : "  FAIL ") + what); }

        public static readonly string[] VideoLevels = {
            "FMX_BuenaVista", "FMX_BuenaVistaB", "FMX_DLC03_FunFactory", "FMX_DLC04_FunFactoryB", "FMX_Dover", "FMX_DoverB", "FMX_Springfield", "FMX_SpringfieldB",
            "NAT_Renne", "NAT_RenneB", "PX_Laguna", "PX_LagunaB", "PX_LagunaB_BW", "PX_Laguna_BW", "SX_DLC01_FreightYard", "SX_DLC02_FreightYardB",
            "SX_FortDodge", "SX_FortDodgeB", "SX_Kingston", "SX_KingstonB", "SX_Manchester", "SX_ManchesterB", "SX_Springfield", "SX_SpringfieldB" };
        static readonly string[] DrawnPrograms = { "Template_Prelit_Map_Ctl", "Template_Prelit_Stadiums", "FY_Projector", "SX_DLC_FY_Bink_ProjectionBeam" };

        public static void Verify()
        {
            log.Clear(); failures = 0;
            var levels = (Environment.GetEnvironmentVariable("REFLEX_SCREENS_LEVELS") is string l && l.Length > 0) ? l.Split(',') : VideoLevels;
            string cache = ReflexInstall.CacheDir;
            int screens = 0, videos = 0; var seenVideos = new HashSet<string>();
            foreach (var level in levels)
            {
                try
                {
                    var sw = Stopwatch.StartNew();
                    var c = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { scenery = true, terrain = false, tracks = false });
                    var list = ReflexVideoScreens.Read(c);
                    log.AppendLine($"{level}: {list.Count} screen(s) ({sw.ElapsedMilliseconds} ms load)");
                    foreach (var s in list)
                    {
                        screens++;
                        log.AppendLine("  " + s);
                        Check(s.d3dFormat == 21 && s.width > 0 && s.height > 0, $"{s.name}: target {s.target} A8R8G8B8 {s.width}x{s.height}");
                        if (s.colorReference) log.AppendLine($"  ok   {s.name}: the level's colour reference map (s13 of every lit program)");
                        else if (s.uses.Count == 0) { log.AppendLine($"  note {s.name}: no material of the level samples {s.target} (not drawn in the original either: not decoded)"); }
                        else Check(true, $"{s.name}: {s.uses.Count} material(s) sample the target");
                        foreach (var u in s.uses) Check(Array.IndexOf(DrawnPrograms, u.pixelShader) >= 0, $"{s.name}: material {u.material} slot {u.slot} program {u.pixelShader} is drawn by the port");
                        var bik = ReflexVideoScreens.Video(c, s);
                        uint frames = BitConverter.ToUInt32(bik, 8), w = BitConverter.ToUInt32(bik, 20), h = BitConverter.ToUInt32(bik, 24), num = BitConverter.ToUInt32(bik, 28), den = BitConverter.ToUInt32(bik, 32), tracks = BitConverter.ToUInt32(bik, 40);
                        string file = ReflexVideoScreens.TempFile(cache, c, s);
                        var p = ReflexBink.reflex_bink_open(ReflexBink.Dll, file, 1, out var info);
                        File.Delete(file);
                        if (p == IntPtr.Zero) { Check(false, $"{s.name}: BinkOpen {ReflexBink.Error(IntPtr.Zero)}"); continue; }
                        try
                        {
                            Check(info.width == w && info.height == h && info.frames == frames && info.fpsNum == num && info.fpsDen == den, $"{s.name}: {s.bink} {info.width}x{info.height} {info.frames} frames at {info.fpsNum}/{info.fpsDen} = header ({w}x{h} {frames} {num}/{den})");
                            Check(info.audioRate == 0 && tracks == 0, $"{s.name}: no sound track (header {tracks}, decoder {info.audioRate} Hz)");
                            ReflexBink.reflex_bink_control(p, ReflexBink.Play, 0);
                            var planes = new[] { new byte[info.ywidth * info.yheight], new byte[info.cwidth * info.cheight], new byte[info.cwidth * info.cheight], info.hasAlpha != 0 ? new byte[info.ywidth * info.yheight] : null };
                            double step = 1000.0 * info.fpsDen / info.fpsNum, t = 0, total = 0, worst = 0; int got = 0, last = 0; long sum = 0;
                            int want = (int)Math.Min(60, info.frames);
                            var ft = new Stopwatch();
                            while (got < want && t < step * (want + 10))
                            {
                                ft.Restart();
                                int r = ReflexBink.reflex_bink_update(p, t, out int frame, out int done);
                                if (r < 0) { Check(false, $"{s.name}: {ReflexBink.Error(p)}"); break; }
                                if (r == 1) { ReflexBink.reflex_bink_planes(p, planes[0], planes[1], planes[2], planes[3]); double ms = ft.Elapsed.TotalMilliseconds; if (got > 0) { total += ms; worst = Math.Max(worst, ms); } got++; last = frame; sum += planes[0].Sum(x => (long)x); }
                                t += step;
                            }
                            Check(got == want && last == want, $"{s.name}: {got} frames decoded on the video clock (last {last}), Y mean {(got > 0 ? sum / (double)got / planes[0].Length : 0):F1}");
                            if (seenVideos.Add(level.TrimEnd('B') + "/" + s.bink)) { videos++; log.AppendLine($"    decode {(got > 1 ? total / (got - 1) : 0):F2} ms per frame average, {worst:F2} worst (after the first; {info.ywidth}x{info.yheight} planes, JIT)"); }
                        }
                        finally { ReflexBink.reflex_bink_close(p); }
                    }
                    // ads: the slot's Ad Texture is a texture of its own MaterialList (the PC game draws the material, no Massive)
                    var ad = ReflexVideoScreens.CheckAds(c);
                    if (ad.slots > 0) Check(ad.inMaterial == ad.slots, $"{level}: {ad.slots} ad slots, Ad Texture in the slot's material {ad.inMaterial}, in its diffuse slot {ad.diffuseSlot}, material sampler states other than the override's defaults {ad.samplerDiffers}");
                    c.Dispose();
                }
                catch (Exception e) { Check(false, $"{level}: {e.GetType().Name} {e.Message}\n{e.StackTrace}"); }
            }
            log.AppendLine($"{screens} screens, {videos} distinct videos");
            log.AppendLine(failures == 0 ? "verify: PASS" : $"verify: FAIL ({failures})");
            Debug.Log("[Screens] " + log);
            string o = Environment.GetEnvironmentVariable("REFLEX_RC_OUT"); if (!string.IsNullOrEmpty(o)) File.WriteAllText(o + "_verify.txt", log.ToString());
        }

        /// The boot spec (REFLEX_EVENT_SPEC) of every event (captures of the original renderer).
        public static void Specs()
        {
            foreach (var e in ReflexEvents.All) { var x = e.spec; Debug.Log($"[Screens] spec {e.key} {x.type},{x.typeId},{x.location},{x.locationId},{x.trackId},{x.vehicleClass},{x.durationType},{x.duration},{x.activity}"); }
        }

        /// Where the screens are (REFLEX_RC_LEVEL): every renderer whose material samples a video target, with the world centre and mean
        /// normal of that submesh (to aim a capture camera at it).
        public static void Where()
        {
            string level = Environment.GetEnvironmentVariable("REFLEX_RC_LEVEL") ?? "SX_DLC01_FreightYard";
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var ol = new GameObject("level").AddComponent<OriginalLevel>(); ol.level = level; ol.originalPipeline = false; ol.alignSun = false;
            OriginalVideoScreens.TestManual = true;
            try { ol.LoadNow(); } finally { OriginalVideoScreens.TestManual = false; }
            var screens = ReflexVideoScreens.Read(ol.Content);
            var names = new HashSet<string>(screens.SelectMany(s => s.uses.Select(u => u.material)));
            foreach (var mr in ol.SceneryRoot.GetComponentsInChildren<MeshRenderer>())
            {
                var mats = mr.sharedMaterials; var mesh = mr.GetComponent<MeshFilter>().sharedMesh;
                for (int i = 0; i < mats.Length; i++)
                {
                    if (!mats[i] || !names.Contains(mats[i].name)) continue;
                    var tris = mesh.GetTriangles(i); var v = mesh.vertices; var n = mesh.normals; Vector3 c = Vector3.zero, nn = Vector3.zero;
                    foreach (var t in tris) { c += mr.transform.TransformPoint(v[t]); nn += mr.transform.TransformDirection(n[t]); }
                    c /= Math.Max(1, tris.Length);
                    Debug.Log($"[Screens] where {mats[i].name} on {mr.name}: centre {c.x:F1},{c.y:F1},{c.z:F1} normal {nn.normalized.x:F2},{nn.normalized.y:F2},{nn.normalized.z:F2} ({tris.Length / 3} triangles)");
                }
            }
        }

        public static void Render()
        {
            string frames = Environment.GetEnvironmentVariable("REFLEX_SCREENS_FRAMES") ?? "1";
            var map = new Dictionary<int, int>(); int all = 1;
            foreach (var part in frames.Split(',').Where(x => x.Length > 0))
            {
                var kv = part.Split('=');
                if (kv.Length == 2) map[int.Parse(kv[0], CultureInfo.InvariantCulture)] = int.Parse(kv[1], CultureInfo.InvariantCulture);
                else all = int.Parse(kv[0], CultureInfo.InvariantCulture);
            }
            bool off = Environment.GetEnvironmentVariable("REFLEX_SCREENS_OFF") == "1";   // "before": the screens as the port drew them before (black targets)
            string outBase = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_rc");
            OriginalVideoScreens.TestManual = true; OriginalVideoScreens.TestDisable = off;   // off: the level as the port drew it before (no OriginalVideoScreens)
            OriginalVideoScreens.TestAttached = v =>
            {
                if (off) { v.enabled = false; return; }
                v.DecodeTo(s => map.TryGetValue(s.entityId, out var f) ? f : all);
                foreach (var n in v.ScreenNames) Debug.Log("[Screens] screen " + n);
            };
            try { RenderCompareTests.Render(); }
            finally { OriginalVideoScreens.TestManual = false; OriginalVideoScreens.TestAttached = null; OriginalVideoScreens.TestDisable = false; }
            var vs = UnityEngine.Object.FindAnyObjectByType<OriginalVideoScreens>();
            if (vs && !off)
            {
                var sb = new StringBuilder(vs.report).AppendLine().AppendLine($"materials bound {vs.materialsBound}");
                foreach (var s in ReflexVideoScreens.Read(vs.level.Content))
                {
                    var rt = vs.TargetOf(s.entityId); if (!rt) continue;
                    sb.AppendLine($"{s.name} #{s.entityId}: frame {vs.ShownFrame(s.entityId)}");
                    var prev = RenderTexture.active; RenderTexture.active = rt;
                    var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false, true); t.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); t.Apply();
                    RenderTexture.active = prev;
                    File.WriteAllBytes($"{outBase}_target_{s.entityId}.png", t.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(t);
                }
                File.WriteAllText(outBase + "_screens.txt", sb.ToString());
                Debug.Log("[Screens] " + sb);
            }
        }

        // ------------------------------------------------------------------------------------------------ play (the running game)
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static (string key, bool had, string s, int i, float f)[] savedKeys;
        static OriginalWorldRuntime playRuntime; static double playStarted, runStarted = -1, lastLog; static float playSeconds; static int framesAtPause = -1; static bool pauseHeld = true, sawGamePause;
        static string playOut;

        public static void Play()
        {
            playOut = Environment.GetEnvironmentVariable("REFLEX_RC_OUT") ?? Path.Combine(Path.GetTempPath(), "reflex_screens_play");
            Directory.CreateDirectory(Path.GetDirectoryName(playOut + "_x"));
            playSeconds = float.Parse(Environment.GetEnvironmentVariable("REFLEX_SCREENS_SECONDS") ?? "40", CultureInfo.InvariantCulture);
            savedKeys = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
            ReflexEventSelect.Pending = Environment.GetEnvironmentVariable("REFLEX_SCREENS_EVENT") ?? "Supercross/SX_Kingston";
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
            var vs = UnityEngine.Object.FindAnyObjectByType<OriginalVideoScreens>();
            double t = now - runStarted;
            playRuntime.paused = t >= 15 && t < 20;   // the port's pause (F6): the screens must hold their frames
            if (vs && t >= 16 && t < 20) { if (framesAtPause < 0) framesAtPause = vs.framesShown; else if (vs.framesShown != framesAtPause) pauseHeld = false; }
            // the game's own pause (Esc goes to the game where its pause page is live, OriginalPauseMenu.GameOwnsPause): Esc at 25 s and 31 s
            playRuntime.testHeldKeys = (t >= 25 && t < 25.3) || (t >= 31 && t < 31.3) ? new[] { 0x01 } : Array.Empty<int>();
            if (vs && vs.gamePaused) sawGamePause = true;
            if (now - lastLog > 2.0)
            {
                lastLog = now;
                Debug.Log(vs ? $"[ScreensPlay] t {t:F1} screens {vs.screens} materials {vs.materialsBound} decoded {vs.framesDecoded} shown {vs.framesShown} paused {vs.paused} (game {vs.gamePaused}, entities found {vs.gameEntitiesFound}) decode {vs.decodeMsAverage:F2} ms avg {vs.decodeMsWorst:F1} worst, main {vs.mainMsAverage:F3} ms/frame"
                             : $"[ScreensPlay] t {t:F1} no OriginalVideoScreens");
            }
            if (t >= playSeconds) { Debug.Log($"[ScreensPlay] the game's own pause seen by the screens: {sawGamePause} (game owns the pause here: {OriginalPauseMenu.GameOwnsPause(playRuntime)})"); }
            if (t >= playSeconds) PlayFinish(vs ? (pauseHeld && vs.framesShown > 0 ? "done" : "FAIL pause held " + pauseHeld + " shown " + vs.framesShown) : "FAIL no screens");
        }

        static void PlayFinish(string why)
        {
            EditorApplication.update -= PlayTick;
            if (playRuntime) playRuntime.paused = false;
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
            Debug.Log("[ScreensPlay] finished: " + why);
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(why == "done" ? 0 : 1);
        }
    }
}
