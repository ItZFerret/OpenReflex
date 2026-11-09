// Diagnostics of the front end's 3D menu world (OpenMenuWorld), off unless asked for in the environment; nothing of it runs in a normal start.
//   REFLEX_MENU3D_MOTION=<file.csv>   one row per rendered frame: the clocks, the walk's time, the rider's root and head, the bike's chassis and
//                                     front wheel, the camera (the motion's continuity is checked from it: tools/menu3d_motion.py)
//   REFLEX_MENU3D_CAPTURE=<dir>       every rendered frame from REFLEX_MENU3D_CAPTURE_AT=<from>:<seconds> (after the first walk starts) as a
//                                     small PNG (480 px wide, read back asynchronously, written on a worker thread)
//   REFLEX_MENU3D_SCRIPT=<steps>      page changes at times after the first walk starts: "garage@1.5,hub@5,pick@6.5,garage@7.2,hub@11"
//                                     (pick / pick-atv / pick-utv / pick-truck / pick-buggy: the world brings another vehicle in, as a
//                                     garage pick does; skip: the intro's skip, as a press; garage-demo: the garage's VEHICLE list browsed (three
//                                     previews) and the third bike picked, everything unlocked in the scratch save)
//   REFLEX_MENU3D_RECORD=<dir>        a deterministic recording: Time.captureFramerate 60 from the start, every frame as a JPEG from the intro's
//                                     start (or the garage-demo's) until 2 s after it ended (REFLEX_MENU3D_RECORD_TAIL), then the player quits
//   REFLEX_MENU3D_HERO_CLASS=<class>  the hero rides the first vehicle of that class (its TypeName: MX LITE, ATV, UTV ...)
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DefaultExecutionOrder(32000)]
    public sealed class OpenMenuWorldProbe : MonoBehaviour
    {
        public static string HeroClass => Environment.GetEnvironmentVariable("REFLEX_MENU3D_HERO_CLASS") is string c && c.Length > 0 ? c : null;
        public static string RecordDir => Environment.GetEnvironmentVariable("REFLEX_MENU3D_RECORD") is string c && c.Length > 0 ? c : null;
        public static bool Wanted => !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_MENU3D_MOTION")) || RecordDir != null ||
                                     !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_MENU3D_CAPTURE")) ||
                                     !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_MENU3D_SCRIPT"));
        OpenMenuWorld world;
        StreamWriter csv; readonly StringBuilder row = new StringBuilder(512);
        string captureDir; float captureFrom = 0f, captureFor = 2f; int captured; RenderTexture full, small;
        readonly List<(float at, string what)> steps = new List<(float, string)>();
        float walkStart = -1f; int stepAt;

        public static void Attach(OpenMenuWorld w) { if (Wanted && !w.GetComponent<OpenMenuWorldProbe>()) w.gameObject.AddComponent<OpenMenuWorldProbe>().world = w; }

        void Start()
        {
            var path = Environment.GetEnvironmentVariable("REFLEX_MENU3D_MOTION");
            if (!string.IsNullOrEmpty(path))
            {
                csv = new StreamWriter(path, false, Encoding.ASCII) { AutoFlush = false };
                csv.WriteLine("time,frame,dt,mode,walking,walk_t,hold_idle,serial,root_x,root_y,root_z,root_yaw,head_x,head_y,head_z,bike_x,bike_y,bike_z,bike_pitch,bike_yaw,bike_roll,wheel_angle,cam_x,cam_y,cam_z,cam_yaw,cam_pitch,lfoot_x,lfoot_y,lfoot_z,rfoot_x,rfoot_y,rfoot_z,intro,real");
            }
            captureDir = Environment.GetEnvironmentVariable("REFLEX_MENU3D_CAPTURE");
            if (!string.IsNullOrEmpty(captureDir))
            {
                Directory.CreateDirectory(captureDir);
                var at = (Environment.GetEnvironmentVariable("REFLEX_MENU3D_CAPTURE_AT") ?? "0:2").Split(':');
                float.TryParse(at[0], NumberStyles.Float, CultureInfo.InvariantCulture, out captureFrom);
                if (at.Length > 1) float.TryParse(at[1], NumberStyles.Float, CultureInfo.InvariantCulture, out captureFor);
            }
            foreach (var s in (Environment.GetEnvironmentVariable("REFLEX_MENU3D_SCRIPT") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = s.Split('@');
                if (kv.Length == 2 && float.TryParse(kv[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float t)) steps.Add((t, kv[0].Trim()));
            }
            steps.Sort((a, b) => a.at.CompareTo(b.at));
            Debug.Log($"[MenuWorldProbe] motion log {path ?? "-"}, capture {captureDir ?? "-"} {captureFrom}+{captureFor} s, script {steps.Count} steps");
        }

        void OnDestroy() { csv?.Flush(); csv?.Dispose(); csv = null; if (full) full.Release(); if (small) small.Release(); }

        static string F(float v) => v.ToString("0.#####", CultureInfo.InvariantCulture);

        /// From the first scene on: every frame longer than 0.25 s is logged (where the time went before the world is ready).
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Watch()
        {
            if (!Wanted) return;
            var g = new GameObject("menu world frame watch (diagnostics)"); DontDestroyOnLoad(g); g.AddComponent<FrameWatch>();
            if (RecordDir != null) { Time.captureFramerate = 60; QualitySettings.vSyncCount = 0; g.AddComponent<Recorder>(); }
        }
        [DefaultExecutionOrder(-32000)]
        sealed class FrameWatch : MonoBehaviour
        {
            float last;
            void Update()
            {
                float now = Time.realtimeSinceStartup;
                if (last > 0 && now - last > 0.25f) Debug.Log($"[MenuWorldProbe] long frame {Time.frameCount}: {(now - last) * 1000f:F0} ms ending at {now:F2} s");
                last = now;
            }
        }

        void LateUpdate()
        {
            if (!world || world.Status != OpenMenuWorld.State.Ready) return;
            if (walkStart < 0 && world.WalkStartedAt >= 0) walkStart = world.WalkStartedAt;
            float since = walkStart < 0 ? -1f : MenuTime.Now - walkStart;
            // the script
            while (walkStart >= 0 && stepAt < steps.Count && since >= steps[stepAt].at) Run(steps[stepAt++].what);
            // the row
            if (csv != null)
            {
                var h = world.Hero; var rm = h?.RiderModel?.rider; var bones = rm ? rm.bones : null;
                Transform root = bones != null && bones.Length > 0 ? bones[0] : null, head = bones != null && bones.Length > 31 ? bones[31] : null;
                var ch = h?.BikeChassis; var fw = h?.BikeFrontWheel; var cam = world.Camera ? world.Camera.transform : null;
                row.Clear();
                row.Append(F(Time.unscaledTime)).Append(',').Append(Time.frameCount).Append(',').Append(F(Time.unscaledDeltaTime)).Append(',').Append(world.Current).Append(',')
                   .Append(world.Walking ? 1 : 0).Append(',').Append(F(world.WalkTime)).Append(',').Append(h != null && h.HoldIdle ? 1 : 0).Append(',').Append(h?.BuildSerial ?? 0);
                void P(Transform t) { var p = t ? t.position : Vector3.zero; row.Append(',').Append(F(p.x)).Append(',').Append(F(p.y)).Append(',').Append(F(p.z)); }
                P(root); row.Append(',').Append(F(root ? root.eulerAngles.y : 0)); P(head); P(ch);
                var e = ch ? ch.eulerAngles : Vector3.zero; row.Append(',').Append(F(e.x)).Append(',').Append(F(e.y)).Append(',').Append(F(e.z));
                row.Append(',').Append(F(fw ? fw.localEulerAngles.x : 0)); P(cam);
                var ce = cam ? cam.eulerAngles : Vector3.zero; row.Append(',').Append(F(ce.y)).Append(',').Append(F(ce.x));
                Transform lf = null, rf = null;
                if (bones != null) foreach (var b in bones) { if (!b) continue; if (b.name == "MXRider_LFoot") lf = b; else if (b.name == "MXRider_RFoot") rf = b; }
                P(lf); P(rf);
                row.Append(',').Append(OpenShell.Instance ? OpenShell.Instance.IntroState : "-").Append(',').Append(F(Time.realtimeSinceStartup));
                csv.WriteLine(row.ToString());
            }
            // consecutive frames
            if (captureDir != null && walkStart >= 0 && since >= captureFrom && since < captureFrom + captureFor) Capture();
        }

        void Run(string what)
        {
            var shell = OpenShell.Instance;
            Debug.Log($"[MenuWorldProbe] step {what} at walk +{MenuTime.Now - walkStart:F2} s (walking {world.Walking}, t {world.WalkTime:F2}, intro {OpenShell.Instance?.IntroState})");
            if (!shell) return;
            switch (what)
            {
                case "garage": shell.ShowGarage(OpenShell.GarageRider); break;
                case "garage-vehicle": shell.ShowGarage(OpenShell.GarageVehicle); break;
                case "hub": shell.ShowMain(); break;
                case "options": shell.ShowOptions(); break;
                case "pick": shell.ProbePick(); break;
                case "pick-atv": shell.ProbePick("atv"); break;
                case "pick-utv": shell._probeMachine = "UTV"; shell.ProbePick("machine"); break;
                case "pick-truck": shell._probeMachine = "Pro"; shell.ProbePick("machine"); break;
                case "pick-buggy": shell._probeMachine = "SB"; shell.ProbePick("machine"); break;
                case "skip": shell.ProbeSkip(); break;
                case "garage-demo": StartCoroutine(GarageDemo(shell)); break;
                case "shot":   // a full-size screenshot into the capture folder (REFLEX_MENU3D_CAPTURE)
                    if (captureDir != null) ScreenCapture.CaptureScreenshot(Path.Combine(captureDir, $"shot_{Screen.width}x{Screen.height}_{Time.frameCount}.png"));
                    break;
            }
        }

        /// The garage's VEHICLE list: three previews, then the third bike picked (the walk with it).
        System.Collections.IEnumerator GarageDemo(OpenShell shell)
        {
            Recorder.Begin("garage");
            Content.CoreGarageProfile.SetUnlockEverything(true); Content.ReflexProfile.Open();
            shell.ShowGarage(OpenShell.GarageVehicle);
            for (int i = 0; i < 90; i++) yield return null;
            var cards = shell.Nav.Items().Where(e => e.name != null && e.name.StartsWith("garage-vehicle-") && e.Q<Label>(className: "ou-pill") == null).ToList();
            var picks = cards.Where(e => !e.ClassListContains("ou-gcard-off")).Take(4).ToList();
            Debug.Log($"[MenuWorldProbe] garage demo: {cards.Count} owned cards, browsing {picks.Count}");
            foreach (var c in picks.Take(3)) { shell.Nav.Focus(c); for (int i = 0; i < 100; i++) yield return null; }
            if (picks.Count > 2) (picks[2].userData as OuNavItem)?.submit?.Invoke();
            for (int i = 0; i < 30; i++) yield return null;
            while (world && world.Walking) yield return null;
            Recorder.End();
        }

        /// The deterministic recording (REFLEX_MENU3D_RECORD): every frame from Begin to End + the tail, then the player quits.
        sealed class Recorder : MonoBehaviour
        {
            static Recorder s_i; static bool s_on, s_ending; static int s_tail, s_n; static readonly List<System.Threading.Tasks.Task> s_writes = new List<System.Threading.Tasks.Task>();
            void Awake() { s_i = this; }
            public static void Begin(string why) { if (s_i == null || s_on) return; s_on = true; Debug.Log("[MenuWorldProbe] recording from " + why + " at frame " + Time.frameCount); s_i.StartCoroutine(s_i.Run()); }
            public static void End() { if (!s_on || s_ending) return; s_ending = true; s_tail = Mathf.RoundToInt(60f * (float.TryParse(Environment.GetEnvironmentVariable("REFLEX_MENU3D_RECORD_TAIL"), NumberStyles.Float, CultureInfo.InvariantCulture, out float t) ? t : 2f)); }
            void Update()
            {
                var shell = OpenShell.Instance;
                if (!s_on && shell && shell.IntroState != "None" && Environment.GetEnvironmentVariable("REFLEX_MENU3D_SCRIPT")?.Contains("garage-demo") != true) Begin("the intro");
                if (s_on && !s_ending && shell && shell.IntroState == "None" && Environment.GetEnvironmentVariable("REFLEX_MENU3D_SCRIPT")?.Contains("garage-demo") != true) End();
            }
            System.Collections.IEnumerator Run()
            {
                var dir = RecordDir; Directory.CreateDirectory(dir);
                var eof = new WaitForEndOfFrame();
                while (true)
                {
                    yield return eof;
                    var tex = ScreenCapture.CaptureScreenshotAsTexture();
                    var data = tex.GetRawTextureData<byte>().ToArray(); var fmt = tex.graphicsFormat; uint w = (uint)tex.width, h = (uint)tex.height;
                    Destroy(tex);
                    int n = s_n++;
                    s_writes.Add(System.Threading.Tasks.Task.Run(() => File.WriteAllBytes(Path.Combine(dir, $"f{n:00000}.jpg"), ImageConversion.EncodeArrayToJPG(data, fmt, w, h, 0, 92))));
                    if (s_ending && --s_tail <= 0) break;
                }
                System.Threading.Tasks.Task.WaitAll(s_writes.ToArray());
                Debug.Log($"[MenuWorldProbe] recording done: {s_n} frames");
                Application.Quit();
            }
        }

        void Capture()
        {
            int w = Screen.width, hgt = Screen.height;
            if (!full || full.width != w || full.height != hgt) { if (full) full.Release(); full = new RenderTexture(w, hgt, 0, RenderTextureFormat.ARGB32); }
            int sw = 480, sh = Mathf.RoundToInt(480f * hgt / w);
            if (!small) small = new RenderTexture(sw, sh, 0, RenderTextureFormat.ARGB32);
            ScreenCapture.CaptureScreenshotIntoRenderTexture(full);
            Graphics.Blit(full, small, new Vector2(1, -1), new Vector2(0, 1));   // (the capture is bottom-up on Metal)
            int n = captured++; float t = MenuTime.Now - walkStart; int frame = Time.frameCount;
            string dir = captureDir;
            AsyncGPUReadback.Request(small, 0, TextureFormat.RGBA32, r =>
            {
                if (r.hasError) return;
                var data = r.GetData<byte>().ToArray();
                System.Threading.Tasks.Task.Run(() =>
                {
                    var png = ImageConversion.EncodeArrayToPNG(data, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm, (uint)sw, (uint)sh);
                    File.WriteAllBytes(Path.Combine(dir, $"f{n:0000}_frame{frame}_t{t:0.000}.png"), png);
                });
            });
        }
    }
}
