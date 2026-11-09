// MENU PERF (w-menufps; a tester: "FPS drops when navigating the menus"): the front end's frame-time spikes in the session log, with what was
// happening, so a player's report shows when and why a menu frame was long (ReflexPerfLog samples races only).
//   [hh:mm:ss.fff] menu hitch 212 ms on Browser (2 frames >50 ms in the last second): work picture 3x 61 ms, page 9 ms; notes focus, arena:make;
//                  gc +1 (mono 182 MB); arena warm starting; cpu main 198.1 render 3.0 gpu 4.2 ms
//   [hh:mm:ss.fff] menu perf 30 s on Main, Browser: 4210 frames, fps 140 avg, 1% low 61, worst 212 ms, >33 ms 6, >50 ms 3, >100 ms 1;
//                  cpu main 5.2 render 2.1 gpu 3.9 ms; gc 4 (+2), mono 182 MB, unity alloc 1210 MB; work picture 14x 180 ms, ...
//   [hh:mm:ss.fff] menu focus: the window lost the focus (the game pauses in the background) / back after 686.2 s
// A hitch is a frame of at least 50 ms and 3x the running mean; it is charged to the work measured in the frame before it (Measure scopes:
// the parts of the front end that may run long on the main thread) and the notes of that frame (a screen change, a focus move, the load
// arena's session made). At most 30 hitch lines a minute (the rest still count in the summary). A frame after the window got the focus back
// is not a hitch (the game does not run in the background: that wait is not a frame). Summaries every 30 s while the menus are up (and when
// they close). REFLEX_MENU_PERF=0: off. -reflexPerfLog / REFLEX_PERF_LOG=1 also echoes the lines to Unity's log (tests).
using System;
using System.Diagnostics;
using System.Globalization;
using System.Text;
using Reflex.Port.OpenFrontEnd;
using Unity.Profiling;
using UnityEngine;
using UnityEngine.Profiling;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Platform
{
    [DefaultExecutionOrder(-32000)]
    public sealed class ReflexMenuPerf : MonoBehaviour
    {
        static ReflexMenuPerf s_inst;
        static bool s_echo;
        public static bool Enabled => s_inst;

        // ---- the work of a frame (main thread): named scopes, summed per name; the notes (events) of the frame
        const int MaxNames = 24;
        static readonly string[] s_names = new string[MaxNames];
        static readonly double[] s_frameMs = new double[MaxNames], s_prevMs = new double[MaxNames], s_windowMs = new double[MaxNames], s_windowMax = new double[MaxNames];
        static readonly int[] s_frameN = new int[MaxNames], s_prevN = new int[MaxNames], s_windowN = new int[MaxNames];
        static int s_used;
        static readonly StringBuilder s_notes = new StringBuilder(128), s_prevNotes = new StringBuilder(128);
        static readonly double s_tick = 1000.0 / Stopwatch.Frequency;

        /// A scope of main-thread work, charged to its name (a frame's hitch line lists them): using (ReflexMenuPerf.Measure("picture")) { ... }.
        /// Free when the logger is off. name: a constant (it is the key).
        public struct Scope : IDisposable
        {
            readonly int slot; readonly long t0;
            internal Scope(int slot) { this.slot = slot; t0 = slot >= 0 ? Stopwatch.GetTimestamp() : 0; }
            public void Dispose() { if (slot >= 0) { s_frameMs[slot] += (Stopwatch.GetTimestamp() - t0) * s_tick; s_frameN[slot]++; } }
        }

        public static Scope Measure(string name)
        {
            if (!s_inst || !s_inst.active || !UnityThread) return new Scope(-1);
            return new Scope(Slot(name));
        }

        /// Work measured elsewhere (ms), charged to name.
        public static void Add(string name, double ms)
        {
            if (!s_inst || !s_inst.active || !UnityThread) return;
            int i = Slot(name); if (i < 0) return;
            s_frameMs[i] += ms; s_frameN[i]++;
        }

        /// Something that happened in this frame (a screen, a focus move, the arena's session): listed with a hitch of the frame.
        public static void Note(string what)
        {
            if (!s_inst || !s_inst.active || !UnityThread || string.IsNullOrEmpty(what)) return;
            if (s_notes.Length > 200) return;
            if (s_notes.Length > 0) s_notes.Append(", ");
            s_notes.Append(what);
        }

        static int s_mainThread = -1;
        static bool UnityThread => System.Threading.Thread.CurrentThread.ManagedThreadId == s_mainThread;

        static int Slot(string name)
        {
            for (int i = 0; i < s_used; i++) if (ReferenceEquals(s_names[i], name) || s_names[i] == name) return i;
            if (s_used >= MaxNames) return -1;
            s_names[s_used] = name; return s_used++;
        }

        /// Seconds the window spent without the focus since the start (the game does not run in the background: the load arena's timing lines
        /// subtract it).
        public static double BackgroundSeconds => s_background + (s_lostAt >= 0 ? Time.realtimeSinceStartupAsDouble - s_lostAt : 0);
        static double s_background, s_lostAt = -1;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            s_mainThread = System.Threading.Thread.CurrentThread.ManagedThreadId;
            Application.focusChanged -= FocusChanged; Application.focusChanged += FocusChanged;   // (kept with the logger off too: BackgroundSeconds)
            if (s_inst || Environment.GetEnvironmentVariable("REFLEX_MENU_PERF") == "0") return;
            if (Application.isEditor && Array.IndexOf(Environment.GetCommandLineArgs(), "-reflexPerfLog") < 0 && Environment.GetEnvironmentVariable("REFLEX_PERF_LOG") != "1") return;
            s_echo = Array.IndexOf(Environment.GetCommandLineArgs(), "-reflexPerfLog") >= 0 || Environment.GetEnvironmentVariable("REFLEX_PERF_LOG") == "1";
            var go = new GameObject("Reflex Menu Perf") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            s_inst = go.AddComponent<ReflexMenuPerf>();
        }

        bool active;   // the front end is up (OpenShell) and no race runs
        double ema = 1.0 / 60.0; int emaFrames;
        bool skipNext = true;
        // the summary's window
        float windowStart = -1f; int frames, over33, over50, over100, hitchLines, hitchesSuppressed; double sum, worst;
        readonly float[] window = new float[8192]; int windowN;
        int gc0 = -1, gcFrame = -1; string screens = "";
        double cpuMain, cpuRender, gpu; int timed;
        readonly FrameTiming[] ft = new FrameTiming[1];
        int minuteLines; float minuteStart;
        // the last second's long frames (a hitch line says how many there were)
        readonly float[] recent = new float[64]; int recentHead;
        ProfilerRecorder gcAlloc;

        void Awake() { gcAlloc = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame"); }
        void OnDestroy() { gcAlloc.Dispose(); if (s_inst == this) s_inst = null; }

        static void FocusChanged(bool focus)
        {
            double now = Time.realtimeSinceStartupAsDouble;
            var me = s_inst && s_inst.active ? s_inst : null;
            if (!focus) { if (s_lostAt < 0) s_lostAt = now; me?.Write("menu focus: the window lost the focus (the game pauses in the background)"); }
            else if (s_lostAt >= 0)
            {
                double away = now - s_lostAt; s_background += away; s_lostAt = -1;
                if (away > 0.5) me?.Write(Inv($"menu focus: back after {away:0.0} s in the background"));
            }
            if (s_inst) s_inst.skipNext = true;
        }

        static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

        void Write(string line)
        {
            ReflexCrashLog.WriteLine(line);
            if (s_echo) Debug.Log("[Reflex] " + line);
        }

        void Update()
        {
            bool menus = OpenShell.Instance && !(ReflexCrashLog.Runtime && ReflexCrashLog.Runtime.Running);
            // the previous frame's work is what this frame's delta time measured
            for (int i = 0; i < s_used; i++) { s_prevMs[i] = s_frameMs[i]; s_prevN[i] = s_frameN[i]; s_frameMs[i] = 0; s_frameN[i] = 0; }
            s_prevNotes.Clear(); s_prevNotes.Append(s_notes); s_notes.Clear();
            if (menus != active)
            {
                if (active) Summary(true);
                active = menus; skipNext = true; ResetWindow(); emaFrames = 0;
                if (!active) return;
            }
            if (!active) return;
            float dt = Time.unscaledDeltaTime;
            FrameTimingManager.CaptureFrameTimings();
            bool haveTiming = FrameTimingManager.GetLatestTimings(1, ft) > 0 && ft[0].cpuFrameTime > 0;
            if (haveTiming) { cpuMain += ft[0].cpuMainThreadFrameTime; cpuRender += ft[0].cpuRenderThreadFrameTime; gpu += ft[0].gpuFrameTime; timed++; }
            int gc = GC.CollectionCount(0);
            int gcDelta = gcFrame < 0 ? 0 : gc - gcFrame; gcFrame = gc;
            if (gc0 < 0) gc0 = gc;
            if (skipNext) { skipNext = false; return; }   // (a frame across a focus change or a screen of the front end starting: not a menu frame)
            if (windowStart < 0) windowStart = Time.realtimeSinceStartup;
            string screen = OpenShell.Instance.Current.ToString();
            if (screens.IndexOf(screen, StringComparison.Ordinal) < 0 && screens.Length < 120) screens = screens.Length == 0 ? screen : screens + ", " + screen;
            frames++; sum += dt; if (dt > worst) worst = dt;
            if (windowN < window.Length) window[windowN++] = dt;
            if (dt > 0.0333f) over33++;
            if (dt > 0.050f) over50++;
            if (dt > 0.100f) over100++;
            for (int i = 0; i < s_used; i++) if (s_prevN[i] > 0) { s_windowMs[i] += s_prevMs[i]; s_windowN[i] += s_prevN[i]; if (s_prevMs[i] > s_windowMax[i]) s_windowMax[i] = s_prevMs[i]; }
            bool hitch = dt >= 0.050f && emaFrames >= 30 && dt >= 3.0 * ema;
            if (hitch)
            {
                recent[recentHead] = Time.realtimeSinceStartup; recentHead = (recentHead + 1) % recent.Length;
                if (Time.realtimeSinceStartup - minuteStart > 60f) { minuteStart = Time.realtimeSinceStartup; minuteLines = 0; }
                if (minuteLines < 30) { minuteLines++; hitchLines++; Hitch(dt, screen, gcDelta, haveTiming); }
                else hitchesSuppressed++;
            }
            // the running mean leaves the hitches out (a slow page does not raise its own bar)
            // (each frame counts at most 50 ms: the long first frames of a page or of the front end's start do not raise the bar for a while)
            if (!hitch) { ema += (Math.Min(dt, 0.05) - ema) * (emaFrames < 30 ? 1.0 / (emaFrames + 1) : 0.05); emaFrames++; }
            if (Time.realtimeSinceStartup - windowStart >= 30f) Summary(false);
        }

        readonly StringBuilder sb = new StringBuilder(512);
        void Hitch(float dt, string screen, int gcDelta, bool haveTiming)
        {
            int inSecond = 0; float now = Time.realtimeSinceStartup;
            foreach (var t in recent) if (t > 0 && now - t <= 1f) inSecond++;
            sb.Clear();
            sb.Append(Inv($"menu hitch {dt * 1000:0} ms on {screen}"));
            if (inSecond > 1) sb.Append(Inv($" ({inSecond} in the last second)"));
            sb.Append(Inv($" (running mean {ema * 1000:0.0} ms)"));
            sb.Append(": work ");
            int listed = 0; double measured = 0;
            for (int i = 0; i < s_used; i++)
                if (s_prevN[i] > 0 && s_prevMs[i] >= 0.5)
                {
                    if (listed++ > 0) sb.Append(", ");
                    sb.Append(s_names[i]); if (s_prevN[i] > 1) sb.Append(' ').Append(s_prevN[i]).Append('x');
                    sb.Append(' ').Append(s_prevMs[i].ToString("0.0", CultureInfo.InvariantCulture)).Append(" ms");
                    measured += s_prevMs[i];
                }
            if (listed == 0) sb.Append("none measured");
            if (s_prevNotes.Length > 0) sb.Append("; notes ").Append(s_prevNotes);
            if (gcDelta > 0) sb.Append(Inv($"; gc +{gcDelta}"));
            sb.Append(Inv($" (mono {GC.GetTotalMemory(false) / 1048576.0:0} MB"));
            if (gcAlloc.Valid) sb.Append(Inv($", {gcAlloc.LastValue / 1024.0:0} KB allocated in the frame"));
            sb.Append(')');
            var warm = OpenArenaWarm.Current;
            sb.Append("; arena ").Append(warm ? (warm.Ready ? "warm ready" : warm.Failure != null ? "failed" : "warm starting (its worker)") : RxCore.RxNativeLoader.ArenaLoaded ? "none (image loaded)" : "none");
            if (haveTiming) sb.Append(Inv($"; cpu main {ft[0].cpuMainThreadFrameTime:0.0} render {ft[0].cpuRenderThreadFrameTime:0.0} gpu {ft[0].gpuFrameTime:0.0} ms (a frame or two behind)"));
            Write(sb.ToString());
        }

        void Summary(bool closing)
        {
            if (frames < 10) { ResetWindow(); return; }
            float span = windowStart < 0 ? 0 : Time.realtimeSinceStartup - windowStart;
            Array.Sort(window, 0, windowN);
            float low1 = windowN > 0 ? window[Math.Min(windowN - 1, (int)(windowN * 0.99f))] : 0;   // the 99th percentile frame time = the "1% low"
            float p50 = windowN > 0 ? window[windowN / 2] : 0;
            sb.Clear();
            sb.Append(Inv($"menu perf {span:0} s{(closing ? " (the menus closed)" : "")} on {screens}: {frames} frames, fps {frames / Math.Max(1e-3, sum):0} avg / {1.0 / Math.Max(1e-4, p50):0} median / {1.0 / Math.Max(1e-4, low1):0} 1% low, worst {worst * 1000:0} ms, >33 ms {over33}, >50 ms {over50}, >100 ms {over100}"));
            if (hitchesSuppressed > 0) sb.Append(Inv($" ({hitchesSuppressed} hitches not listed)"));
            if (timed > 0) sb.Append(Inv($"; cpu main {cpuMain / timed:0.0} render {cpuRender / timed:0.0} gpu {gpu / timed:0.0} ms"));
            int gc = GC.CollectionCount(0);
            sb.Append(Inv($"; gc {gc} (+{gc - gc0}), mono {GC.GetTotalMemory(false) / 1048576.0:0} MB, unity alloc {Profiler.GetTotalAllocatedMemoryLong() / 1048576.0:0} MB"));
            sb.Append("; work ");
            int listed = 0;
            for (int i = 0; i < s_used; i++)
                if (s_windowN[i] > 0 && s_windowMs[i] >= 1)
                {
                    if (listed++ > 0) sb.Append(", ");
                    sb.Append(Inv($"{s_names[i]} {s_windowN[i]}x {s_windowMs[i]:0} ms (max {s_windowMax[i]:0.0})"));
                }
            if (listed == 0) sb.Append("none over 1 ms");
            Write(sb.ToString());
            ResetWindow();
        }

        void ResetWindow()
        {
            windowStart = -1f; frames = over33 = over50 = over100 = hitchLines = hitchesSuppressed = 0; sum = worst = 0; windowN = 0;
            gc0 = GC.CollectionCount(0); screens = ""; cpuMain = cpuRender = gpu = 0; timed = 0;
            for (int i = 0; i < s_used; i++) { s_windowMs[i] = 0; s_windowN[i] = 0; s_windowMax[i] = 0; }
        }
    }
}
