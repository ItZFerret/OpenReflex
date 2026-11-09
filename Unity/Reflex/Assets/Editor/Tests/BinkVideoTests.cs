using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEditor;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Editor
{
    // The Bink videos (Content/ReflexVideo.cs over native bink.cpp: the install's binkw32.dll in the interpreter / JIT):
    //   Verify:    every <install>/Videos/*.bik opened with the install's decoder (size, frame rate, frames, sound), frames decoded at the
    //              file's rate (ms per frame), the Reflex/Bink Video shader's picture read back and compared byte for byte with the
    //              game's ps_2_0 YUV(A) program evaluated on the CPU over the same planes; a PNG per video to $BINK_SHOTS.
    //   PlayIntro: play mode, FrontEnd.unity with the startup sequence (legal screen, Nordic / Rainbow / Double Helix logos, the
    //              intro video skipped with Accept, loading, the title video then the title image), the attract page, VIEW CREDITS,
    //              the career completion page; a PNG per step and the decode cost of every video played.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.BinkVideoTests.Verify (or .PlayIntro)
    public static class BinkVideoTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static string shots;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[BinkVerify] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static void Shots() { shots = Environment.GetEnvironmentVariable("BINK_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "bink-shots"); Directory.CreateDirectory(shots); }
        static void Finish(string name)
        {
            Log(name + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, name + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------------- Verify
        public static void Verify()
        {
            report.Clear(); bad = 0; Shots();
            try
            {
                var dir = ReflexInstall.PathOf("Videos");
                var files = Directory.GetFiles(dir, "*.bik").OrderBy(f => f).ToList();
                Check(files.Count == 19, files.Count + " videos in " + dir);
                var shader = Shader.Find("Reflex/Bink Video");
                Check(shader && shader.isSupported, "Reflex/Bink Video shader");
                foreach (var f in files) VerifyOne(f, shader);
                LoopCheck();
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            Finish("verify");
        }

        static void VerifyOne(string file, Shader shader)
        {
            string name = Path.GetFileNameWithoutExtension(file);
            bool preload = name.Contains("Logo") || name == "Title";
            var p = ReflexBink.reflex_bink_open(ReflexBink.Dll, file, preload ? 1 : 0, out var info);
            if (p == IntPtr.Zero) { Check(false, name + ": BinkOpen " + ReflexBink.Error(IntPtr.Zero)); return; }
            try
            {
                Log($"{name}: {info.width}x{info.height}, {info.frames} frames at {info.Fps:0.###} fps, planes Y {info.ywidth}x{info.yheight} c {info.cwidth}x{info.cheight}, alpha {info.hasAlpha}, sound " +
                    (info.audioRate > 0 ? $"{info.audioRate} Hz x{info.audioChannels} {info.audioBits}-bit" : "none"));
                ReflexBink.reflex_bink_control(p, ReflexBink.Play, 0);
                int want = (int)Math.Min(info.frames, 90), at = Math.Min(want, 45), frame = 0, done = 0, decoded = 0;
                double ms = 0, cost = 0, worst = 0; long pcm = 0; var sw = new Stopwatch(); var buf = new short[1 << 16];
                byte[][] pl = null;
                for (int u = 0; u < 100000 && frame < want && done == 0; u++, ms += 1000.0 / 60)
                {
                    sw.Restart();
                    int r = ReflexBink.reflex_bink_update(p, ms, out frame, out done);
                    double t = sw.Elapsed.TotalMilliseconds;
                    if (r < 0) { Check(false, name + ": " + ReflexBink.Error(p)); return; }
                    if (r == 1)
                    {
                        decoded++; cost += t; worst = Math.Max(worst, t);
                        if (frame == at)
                        {
                            pl = new[] { new byte[info.ywidth * info.yheight], new byte[info.cwidth * info.cheight], new byte[info.cwidth * info.cheight], info.hasAlpha != 0 ? new byte[info.ywidth * info.yheight] : null };
                            ReflexBink.reflex_bink_planes(p, pl[0], pl[1], pl[2], pl[3]);
                        }
                    }
                    for (int n; (n = ReflexBink.reflex_bink_audio(p, buf, buf.Length)) > 0;) pcm += n;
                }
                double fps = info.Fps;
                Check(decoded >= want - 1, $"{name}: {decoded} frames decoded to frame {frame}; {cost / Math.Max(1, decoded):0.00} ms per frame average, {worst:0.0} worst (budget {1000 / fps:0.0} ms)");
                if (info.audioRate > 0) Check(pcm > 0, $"{name}: {pcm / Math.Max(1u, info.audioChannels)} sound samples ({pcm / (double)info.audioChannels / info.audioRate:0.00} s for {frame / fps:0.00} s of video)");
                if (pl != null) ComparePicture(name, info, pl, shader);
            }
            finally { ReflexBink.reflex_bink_close(p); }
        }

        /// OriginalVideo (the component the front end uses) driven in edit mode: a Loop video (as PreTutorialPopup's) restarts at its
        /// last frame (BinkGoto(1), Looped) and keeps the file's rate; a plain one ends with Done.
        static void LoopCheck()
        {
            var go = new GameObject("bink loop check");
            try
            {
                var v = OriginalVideo.Create(go, "Videos\\HUDFlagRed", false, true);
                int looped = 0, done = 0; v.Looped += () => looped++; v.Done += () => done++;
                v.Play();
                var sw = Stopwatch.StartNew();
                while (sw.Elapsed.TotalSeconds < 3.5) { v.Pump(); System.Threading.Thread.Sleep(5); }
                double fps = v.Info.Fps, secs = sw.Elapsed.TotalSeconds;
                Check(looped >= 2 && done == 0 && Math.Abs(v.FramesShown - secs * fps) < 0.1 * secs * fps + 3,
                    $"Loop: HUDFlagRed ({v.Info.frames} frames) looped {looped} times in {secs:0.0} s, {v.FramesShown} frames shown ({secs * fps:0} due), Done {done}");
                v.loop = false; looped = 0;
                v.Play(); sw.Restart();
                while (sw.Elapsed.TotalSeconds < 2.5 && done == 0) { v.Pump(); System.Threading.Thread.Sleep(5); }
                Check(done == 1 && looped == 0 && v.Frame == v.Info.frames, $"no Loop: Done at frame {v.Frame} after {sw.Elapsed.TotalSeconds:0.00} s ({v.Info.frames / v.Info.Fps:0.00} s long)");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        /// The shader's target vs the game's program on the CPU (bilinear planes, saturate, YUVA blend onto 0, sRGB bytes).
        static void ComparePicture(string name, ReflexBink.Info info, byte[][] pl, Shader shader)
        {
            int W = (int)info.width, H = (int)info.height;
            Texture2D Plane(byte[] d, uint w, uint h) { var t = new Texture2D((int)w, (int)h, TextureFormat.R8, false, true) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp }; t.LoadRawTextureData(d); t.Apply(false, false); return t; }
            var tx = new[] { Plane(pl[0], info.ywidth, info.yheight), Plane(pl[1], info.cwidth, info.cheight), Plane(pl[2], info.cwidth, info.cheight), pl[3] != null ? Plane(pl[3], info.ywidth, info.yheight) : null };
            bool srgb = QualitySettings.activeColorSpace == ColorSpace.Linear;
            var rt = new RenderTexture(W, H, 0, RenderTextureFormat.ARGB32, srgb ? RenderTextureReadWrite.sRGB : RenderTextureReadWrite.Linear);
            var m = new Material(shader);
            m.SetTexture("_YTex", tx[0]); m.SetTexture("_CrTex", tx[1]); m.SetTexture("_CbTex", tx[2]); if (tx[3]) m.SetTexture("_ATex", tx[3]);
            m.SetFloat("_Alpha", info.hasAlpha != 0 ? 1 : 0); m.SetFloat("_SrgbTarget", srgb ? 1 : 0);
            Graphics.Blit(null, rt, m);
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var read = new Texture2D(W, H, TextureFormat.RGBA32, false, !srgb); read.ReadPixels(new Rect(0, 0, W, H), 0, 0); read.Apply();
            RenderTexture.active = prev;
            var got = read.GetRawTextureData<byte>().ToArray();   // bottom-up rows
            int maxd = 0; long off = 0, n = (long)W * H * 4;
            float S(byte[] t, uint tw, uint th, float u, float v)
            {
                float x = u * tw - 0.5f, y = v * th - 0.5f; int x0 = Mathf.FloorToInt(x), y0 = Mathf.FloorToInt(y); float fx = x - x0, fy = y - y0;
                float A(int xx, int yy) { xx = Mathf.Clamp(xx, 0, (int)tw - 1); yy = Mathf.Clamp(yy, 0, (int)th - 1); return t[yy * tw + xx] / 255f; }
                return (A(x0, y0) * (1 - fx) + A(x0 + 1, y0) * fx) * (1 - fy) + (A(x0, y0 + 1) * (1 - fx) + A(x0 + 1, y0 + 1) * fx) * fy;
            }
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    float u = (x + 0.5f) / W, v = (y + 0.5f) / H;
                    float Y = S(pl[0], info.ywidth, info.yheight, u, v), cr = S(pl[1], info.cwidth, info.cheight, u, v), cb = S(pl[2], info.cwidth, info.cheight, u, v);
                    float r = Mathf.Clamp01(Y * 1.164123535f + 1.595794678f * cr - 0.87065506f);
                    float g = Mathf.Clamp01(Y * 1.164123535f - 0.813476563f * cr - 0.391448975f * cb + 0.529705048f);
                    float b = Mathf.Clamp01(Y * 1.164123535f + 2.017822266f * cb - 1.081668854f);
                    float a = pl[3] != null ? S(pl[3], info.ywidth, info.yheight, u, v) : 1f;
                    if (pl[3] != null) { r *= a; g *= a; b *= a; a *= a; }
                    int o = ((H - 1 - y) * W + x) * 4;
                    int[] want = { Mathf.RoundToInt(r * 255), Mathf.RoundToInt(g * 255), Mathf.RoundToInt(b * 255), Mathf.RoundToInt(a * 255) };
                    for (int c = 0; c < 4; c++) { int d = Math.Abs(got[o + c] - want[c]); if (d > 1) off++; maxd = Math.Max(maxd, d); }
                }
            Check(maxd <= 3 && off < n / 1000, $"{name}: shader picture vs the game's YUV{(pl[3] != null ? "A" : "")} program on the CPU: max |diff| {maxd}, {off} of {n} channels off by more than 1");
            File.WriteAllBytes(Path.Combine(shots, "bink-" + name + ".png"), read.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(read); rt.Release(); UnityEngine.Object.DestroyImmediate(rt); UnityEngine.Object.DestroyImmediate(m);
            foreach (var t in tx) if (t) UnityEngine.Object.DestroyImmediate(t);
        }

        // ---------------------------------------------------------------------------------------------------- PlayIntro
        static int stage; static double stageAt, startAt; static FrontEndManager fm;
        static readonly Dictionary<string, (double avg, double worst, int decoded, int shown, double secs, double fps, long sound, uint rate, double upload)> stats = new Dictionary<string, (double, double, int, int, double, double, long, uint, double)>();
        static readonly Dictionary<OriginalVideo, double> seenAt = new Dictionary<OriginalVideo, double>();

        public static void PlayIntro()
        {
            report.Clear(); bad = 0; stats.Clear(); seenAt.Clear(); Shots();
            ReflexProfile.Disabled = true;   // the guest path
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            FrontEndManager.IntroInBatchMode = true;
            stage = 0; stageAt = startAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }
        static FeVideo Video(string layer, string node) { var l = fm.Layer(layer); return FeVideo.Of(l.Find(node)); }
        static void Track()
        {
            foreach (var v in UnityEngine.Object.FindObjectsByType<OriginalVideo>(FindObjectsSortMode.None))
            {
                if (!seenAt.ContainsKey(v)) seenAt[v] = EditorApplication.timeSinceStartup;
                if (v.FramesDecoded > 0) stats[v.video] = (v.DecodeMsAverage, v.DecodeMsWorst, v.FramesDecoded, v.FramesShown, EditorApplication.timeSinceStartup - seenAt[v], v.Info.Fps, v.AudioSamplesPlayed, v.Info.audioRate, v.UploadMsAverage);
            }
        }
        static string page; static double pageAt;
        static bool On(string p, double after) => fm && fm.CurrentPage == p && !fm.Busy && EditorApplication.timeSinceStartup - pageAt > after;   // on the page for `after` s
        static void PlayingCheck(string layer, string node, string what)
        {
            var v = Video(layer, node);
            Check(v != null && v.player && v.player.HasFrame && v.image.enabled && v.image.texture == v.player.Texture, $"{what}: {v?.path} playing, frame {(v?.player ? v.player.Frame : -1)} on screen");
        }

        static void Step()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (EditorApplication.timeSinceStartup - startAt > 120) Fail("play mode never started"); return; }
                fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                if (fm && fm.CurrentPage != page) { page = fm.CurrentPage; pageAt = EditorApplication.timeSinceStartup; }
                Track();
                switch (stage)
                {
                    case 0: if (fm && fm.assets != null && Age > 1.5) { Check(fm.CurrentPage == FeVideoFlow.IntroFirstPage, "startup: " + fm.CurrentPage); Shot("01_legal_screen"); Next(); } else if (Age > 60) Fail("front end did not boot"); break;
                    case 1: if (On("Publisher Video Page", 2)) { PlayingCheck("PublisherVideo.lyr", "Video", "publisher logo"); Shot("02_publisher_nordic"); Next(); } else if (Age > 15) Fail("no publisher video page: " + fm?.CurrentPage); break;
                    case 2: if (On("Studio Video Page", 2.5)) { PlayingCheck("StudioVideo.lyr", "Video", "studio logo"); Shot("03_studio_rainbow"); fm.DispatchKey("Accept"); Next(); } else if (Age > 20) Fail("no studio video page (after the publisher video's DoneEvent): " + fm?.CurrentPage); break;
                    case 3: if (On("Studio Video Page2", 2)) { Check(true, "Accept skipped the Rainbow logo (StudioVideoDone)"); PlayingCheck("StudioVideo2.lyr", "Video", "Double Helix logo"); Shot("04_studio2_dh"); Next(); } else if (Age > 10) Fail("Accept did not skip: " + fm?.CurrentPage); break;
                    case 4: if (On("Intro Video Page", 3)) { Check(true, "DHLogo done -> intro video"); PlayingCheck("IntroVideo.lyr", "Video", "intro video"); Shot("05_intro_attract"); fm.DispatchKey("Accept"); Next(); } else if (Age > 20) Fail("no intro video page: " + fm?.CurrentPage); break;
                    case 5: if (On("Title Page", 1.2)) { PlayingCheck("Title.lyr", "Title Video", "title video"); Shot("06_title_video"); Next(); } else if (Age > 20) Fail("no title page after the loading screen: " + fm?.CurrentPage); break;
                    case 6:
                    {
                        var img = fm.Layer("Title.lyr").Find("Title Image");
                        if (img != null && img.Visible && Age > 0.5) { Check(!fm.Layer("Title.lyr").Find("Title Video").Visible, "TitleVideoDone -> OnTitleVideoDone: the title image"); Shot("07_title_image"); fm.SendUIEvent("Attract Page"); Next(); }
                        else if (Age > 20) Fail("the title video never finished");
                        break;
                    }
                    case 7: if (On("Attract Page", 4)) { PlayingCheck("Attract.lyr", "Video", "attract video"); Shot("08_attract"); fm.DispatchKey("Accept"); Next(); } else if (Age > 15) Fail("no attract page: " + fm?.CurrentPage); break;
                    case 8: if (On("Title Page", 1)) { Check(true, "Accept on the attract page -> title"); fm.DispatchKey("Accept"); Next(); } else if (Age > 10) Fail("attract did not return: " + fm?.CurrentPage); break;
                    case 9: if (Age > 1) { fm.DispatchKey("Accept"); Next(); } break;   // profile reminder: CONTINUE
                    case 10: if (On("Main Menu Page", 1.5)) { fm.DispatchKey("Toggle"); Next(); } else if (Age > 10) Fail("no main menu: " + fm?.CurrentPage); break;
                    case 11: if (On("Options Page", 1)) { fm.Layer("OptionsFrontEnd.lyr").script.Call("OnCreditsKeypress"); Next(); } else if (Age > 10) Fail("no options page: " + fm?.CurrentPage); break;
                    case 12: if (On("Credits Page", 4)) { PlayingCheck("Credits.lyr", "Video", "credits"); Shot("09_credits"); fm.DispatchKey("Back"); Next(); } else if (Age > 15) Fail("VIEW CREDITS -> no credits page: " + fm?.CurrentPage); break;
                    case 13: if (On("Options Page", 1)) { Check(true, "Back on the credits -> options (TravelBack)"); fm.JumpTo("Career Completion Page"); Next(); } else if (Age > 10) Fail("credits did not travel back: " + fm?.CurrentPage); break;
                    case 14: if (On("Career Completion Page", 4)) { PlayingCheck("CareerCompletion.lyr", "CareerCompletionVideo", "career completion"); Shot("10_career_completion"); Next(); } else if (Age > 15) Fail("no career completion page: " + fm?.CurrentPage); break;
                    case 15: stage = 100; EditorApplication.ExitPlaymode(); break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static void End()
        {
            EditorApplication.update -= Step;
            FrontEndManager.IntroInBatchMode = false;
            foreach (var kv in stats.OrderBy(k => k.Key))
            {
                var s = kv.Value; double expect = Math.Max(1, s.secs * s.fps);
                Log($"{kv.Key}: {s.decoded} frames decoded, {s.shown} shown in {s.secs:0.0} s ({s.fps:0.##} fps: {expect:0} due), decode {s.avg:0.00} ms average, {s.worst:0.0} ms worst (decode thread), upload + draw {s.upload:0.00} ms (main thread)" +
                    (s.rate > 0 ? $", sound {s.sound / (double)s.rate:0.00} s played" : ""));
                Check(s.avg < 1000 / s.fps, $"{kv.Key}: decoding keeps up ({s.avg:0.00} ms per frame < {1000 / s.fps:0.0} ms)");
            }
            Finish("playintro");
        }

        static void Shot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + name + " (" + fm?.CurrentPage + ")");
        }
    }
}
