// ReflexPerfLog: performance and memory samples in the session log (ReflexCrashLog) during races, so a player's log shows when
// and why the frame rate fell (after Open Unleashed's MxaPerfLog; notes: _re/notes/client-platform.md).
//   [hh:mm:ss.fff] perf t=40s: fps 59.9 avg / 52.1 min (worst 19.2 ms), frame 16.69 ms, cpu main 9.10 render 3.20, gpu 6.90 ms
//                  | sim 61 jobs (1.02 frames/job, 0 merged), job 7.42 ms (max 11.3), original 6.80 ms, opponents apply 0.21 ms
//                  | mono 92.1 MB, gc 14 (+2), gc alloc 17.0 KB/frame, unity alloc 812 / reserved 1024 MB, ... draw calls 2100
//                  | plugin heap=310.2MB rss=1820.4MB footprint=1650.0MB threads=61
// One line every 10 s of a running, unpaused world (-reflexPerfInterval <s>), plus a "perf drop" line at once when the frame rate
// over one second falls below half of the race's running average (at most every 10 s). The native cost is the world thread's
// own measurement (OriginalSimulation.Results: milliseconds = the job with its exports, frameMilliseconds = the original
// frame(s) alone, framesRun / merged), the same numbers the pacing probe (OriginalPacingProbe) summarises.
// CPU / GPU times need Player Settings > Frame Timing Stats (the build entry turns it on for its players); without it they are
// left out. Every 3rd line also counts objects (GameObjects, Meshes, Textures, Materials, RenderTextures, AudioSources).
// A "display:" line with the real window size is written at the first frames and whenever it changes.
// -reflexPerfLog (or REFLEX_PERF_LOG=1) also echoes the lines to Unity's log; the Editor samples only with one of them.
// The "alloc" part (w-gcspikes) is in every player: the managed heap's growth per frame between collections, the systems' shares of it
// (ReflexAllocMeter: sound, practice, opponents, hud, replay, view; other = the rest) and, where the player has them, the longest
// GC.Collect / incremental GC step in one frame:
//                  | alloc 3.1 KB/frame (heap growth), sound 1.2, replay 0.6, hud 0.3, other 0.7, gc step max 4.2 ms
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using Reflex.Port.OriginalWorld;
using Unity.Profiling;
using UnityEngine;
using UnityEngine.Profiling;

namespace Reflex.Port.Platform
{
    [DefaultExecutionOrder(30001)]
    public sealed class ReflexPerfLog : MonoBehaviour
    {
        static ReflexPerfLog s_inst;
        /// The last sample line written (tests, a SUPPORT page).
        public static string LastLine { get; private set; } = "";
        /// Lines written this session.
        public static int Lines { get; private set; }

        float _interval = 10f;
        bool _echo;
        double _acc, _worst, _main, _render, _gpu;
        int _frames, _timings;
        double _secAcc; int _secFrames; double _avgFps; int _avgN; float _lastDrop = -100f;
        float _raceStart = -1f, _next;
        int _sample, _gc0;
        string _display = "";
        // the world thread's jobs in the window
        double _lastSimTime = double.NaN, _jobMs, _jobMax, _origMs, _applyMs; int _jobs, _jobFrames, _merged, _applyN;
        ProfilerRecorder _gcAlloc; double _gcBytes; int _gcFrames;
        // the heap's growth between collections (any player) and the systems' shares (ReflexAllocMeter)
        long _heapPrev; int _heapGc; double _heapGrowth; int _heapFrames;
        readonly long[] _sysBytes = new long[ReflexAllocMeter.Count], _sysWin = new long[ReflexAllocMeter.Count];
        readonly double[] _sysAcc = new double[ReflexAllocMeter.Count];
        readonly int[] _sysOrder = new int[ReflexAllocMeter.Count];
        // the GC's own markers where the player records them (GC.Collect, the incremental steps): the longest in one frame
        readonly List<ProfilerRecorder> _gcMarkers = new List<ProfilerRecorder>(); double _gcStepMax;
        readonly FrameTiming[] _ft = new FrameTiming[1];
        readonly Dictionary<string, ProfilerRecorder> _rec = new Dictionary<string, ProfilerRecorder>();
        static readonly string[] s_mem = { "Total Used Memory", "Total Reserved Memory", "GC Used Memory", "Gfx Used Memory", "System Used Memory" };
        static readonly string[] s_render = { "Draw Calls Count", "Batches Count", "Triangles Count" };
        readonly byte[] _stats = new byte[512];

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (s_inst) return;
            if (Application.isEditor && !EchoRequested()) return;
            var go = new GameObject("Reflex Perf Log") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            s_inst = go.AddComponent<ReflexPerfLog>();
        }

        static bool EchoRequested() => Array.IndexOf(Environment.GetCommandLineArgs(), "-reflexPerfLog") >= 0 || Environment.GetEnvironmentVariable("REFLEX_PERF_LOG") == "1";

        static string Arg(string name)
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, name);
            return i >= 0 && i + 1 < a.Length ? a[i + 1] : null;
        }

        void Awake()
        {
            if (float.TryParse(Arg("-reflexPerfInterval") ?? Environment.GetEnvironmentVariable("REFLEX_PERF_INTERVAL") ?? "", NumberStyles.Float, CultureInfo.InvariantCulture, out float iv) && iv >= 1f) _interval = iv;
            _echo = EchoRequested();
            // -reflexGcSliceMs <ms> (REFLEX_GC_SLICE_MS): the incremental GC's work per frame (Unity's default 3 ms), for a tester's A/B; the
            // "display:" line says what the player runs with (w-gcspikes)
            if (float.TryParse(Arg("-reflexGcSliceMs") ?? Environment.GetEnvironmentVariable("REFLEX_GC_SLICE_MS") ?? "", NumberStyles.Float, CultureInfo.InvariantCulture, out float slice) && slice > 0f && slice <= 50f)
                try { UnityEngine.Scripting.GarbageCollector.incrementalTimeSliceNanoseconds = (ulong)(slice * 1e6); } catch (Exception) { }
            foreach (var n in s_mem) _rec[n] = ProfilerRecorder.StartNew(ProfilerCategory.Memory, n);
            foreach (var n in s_render) _rec[n] = ProfilerRecorder.StartNew(ProfilerCategory.Render, n);
            _gcAlloc = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame");
            try
            {
                var handles = new List<Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle>();
                Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle.GetAvailable(handles);
                foreach (var h in handles)
                {
                    var d = Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle.GetDescription(h);
                    if (d.Name == "GC.Collect" || d.Name == "GC.MarkDependencies" || d.Name == "GC.Incremental")
                    {
                        var r = new ProfilerRecorder(h, 1, ProfilerRecorderOptions.Default); r.Start();
                        if (r.Valid) _gcMarkers.Add(r); else r.Dispose();
                    }
                }
            }
            catch (Exception) { }
        }

        void OnDestroy() { foreach (var r in _rec.Values) r.Dispose(); _gcAlloc.Dispose(); foreach (var r in _gcMarkers) r.Dispose(); ReflexAllocMeter.On = false; }

        void Write(string line)
        {
            LastLine = line; Lines++;
            ReflexCrashLog.WriteLine(line);
            if (_echo) Debug.Log("[Reflex] " + line);
        }

        void Update()
        {
            CheckDisplay();
            var rt = ReflexCrashLog.Runtime;
            if (!rt || !rt.Running || rt.paused)
            {
                if (!rt || !rt.Running) _raceStart = -1f;
                ReflexAllocMeter.On = false; _heapPrev = 0;
                ResetWindow();
                return;
            }
            ReflexAllocMeter.On = true;
            {   // the heap's growth since the last frame, when no collection ran in between
                long used = GC.GetTotalMemory(false); int gcn = GC.CollectionCount(0);
                if (_heapPrev > 0 && gcn == _heapGc && used > _heapPrev) _heapGrowth += used - _heapPrev;
                _heapPrev = used; _heapGc = gcn; _heapFrames++;
                for (int i = 0; i < _gcMarkers.Count; i++) { double ms = _gcMarkers[i].LastValue * 1e-6; if (ms > _gcStepMax) _gcStepMax = ms; }
            }
            float now = Time.realtimeSinceStartup;
            if (_raceStart < 0f)
            {
                _raceStart = now; _next = now + _interval; _sample = 0; _avgFps = 0; _avgN = 0; _gc0 = GC.CollectionCount(0);
                ResetWindow();
                return;
            }
            double dt = Time.unscaledDeltaTime;
            _acc += dt; _frames++;
            if (dt > _worst) _worst = dt;
            if (_gcAlloc.Valid) { _gcBytes += _gcAlloc.LastValue; _gcFrames++; }
            FrameTimingManager.CaptureFrameTimings();
            if (FrameTimingManager.GetLatestTimings(1, _ft) > 0 && _ft[0].cpuMainThreadFrameTime > 0)
            {
                _main += _ft[0].cpuMainThreadFrameTime; _render += _ft[0].cpuRenderThreadFrameTime; _gpu += _ft[0].gpuFrameTime;
                _timings++;
            }
            var n = rt.Presentation?.Newest;
            if (n != null && n.simTime != _lastSimTime)
            {
                _lastSimTime = n.simTime;
                _jobs++; _jobMs += n.milliseconds; if (n.milliseconds > _jobMax) _jobMax = n.milliseconds;
                _origMs += n.frameMilliseconds; _jobFrames += n.framesRun; if (n.merged) _merged++;
            }
            if (rt.opponentRiders) { _applyMs += rt.opponentRiders.LastApplyMilliseconds; _applyN++; }
            _secAcc += dt; _secFrames++;
            if (_secAcc >= 1.0)
            {
                double fps = _secFrames / _secAcc;
                if (_avgN >= 5 && _avgFps >= 20 && fps < 0.5 * _avgFps && now - _lastDrop > 10f)
                {
                    _lastDrop = now;
                    Write(Sample(Inv($"perf drop t={now - _raceStart:0}s: {fps:0.0} fps over 1 s (race average {_avgFps:0.0})"), false));
                }
                _avgFps = (_avgFps * _avgN + fps) / (_avgN + 1); _avgN = Math.Min(_avgN + 1, 30);
                _secAcc = 0; _secFrames = 0;
            }
            if (now >= _next)
            {
                _next = now + _interval;
                _sample++;
                Write(Sample(Inv($"perf t={now - _raceStart:0}s"), _sample % 3 == 1));
                ResetWindow();
            }
        }

        static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

        void ResetWindow()
        {
            _acc = _worst = _main = _render = _gpu = 0; _frames = _timings = 0; _gcBytes = 0; _gcFrames = 0;
            _heapGrowth = 0; _heapFrames = 0; _gcStepMax = 0; ReflexAllocMeter.Take(_sysBytes); Array.Clear(_sysWin, 0, _sysWin.Length);
            _jobs = _jobFrames = _merged = _applyN = 0; _jobMs = _jobMax = _origMs = _applyMs = 0;
        }

        void CheckDisplay()
        {
            if (Time.frameCount < 3) return;
            var r = ReflexDisplayInfo.Current;   // (cached: Screen.currentResolution / Screen.dpi ask the window system each time)
            float dpi = ReflexDisplayInfo.Dpi;
            // the line is made only when something in it changed (no string per frame)
            long key = Screen.width * 73856093L ^ Screen.height * 19349663L ^ (long)Screen.fullScreenMode * 83492791L ^ r.width * 2654435761L ^ r.height * 40503L
                       ^ (long)(r.refreshRateRatio.value * 1000) * 97L ^ QualitySettings.vSyncCount * 7919L ^ Application.targetFrameRate * 104729L ^ (long)dpi * 15485863L ^ QualitySettings.maxQueuedFrames * 31L;
            if (key == _displayKey && _display.Length > 0) return;
            _displayKey = key;
            string d = string.Format(CultureInfo.InvariantCulture, "display: {0} x {1} {2}, monitor {3} x {4} @ {5:0.#} Hz, vsync {6}, target fps {7}, dpi {8:0}, queued frames {9}, gc {10}",
                Screen.width, Screen.height, Screen.fullScreenMode, r.width, r.height, r.refreshRateRatio.value, QualitySettings.vSyncCount,
                Application.targetFrameRate, dpi, QualitySettings.maxQueuedFrames, GcMode());
            if (d == _display) return;
            _display = d;
            Write(d);
        }
        long _displayKey;

        static string GcMode()
        {
            try
            {
                return UnityEngine.Scripting.GarbageCollector.isIncremental
                    ? "incremental, slice " + (UnityEngine.Scripting.GarbageCollector.incrementalTimeSliceNanoseconds / 1e6).ToString("0.0#", CultureInfo.InvariantCulture) + " ms"
                    : "not incremental";
            }
            catch (Exception) { return "?"; }
        }

        // " | alloc 3.1 KB/frame (heap growth), sound 1.2, replay 0.6, other 0.7, gc step max 4.2 ms": the systems with at least 0.05 KB a frame,
        // largest first (w-gcspikes)
        void AppendAlloc(StringBuilder sb, CultureInfo ci)
        {
            if (_heapFrames <= 0) return;
            ReflexAllocMeter.Take(_sysBytes);
            for (int i = 0; i < _sysWin.Length; i++) _sysWin[i] += _sysBytes[i];   // (a "perf drop" line mid-window does not lose them)
            double total = _heapGrowth / _heapFrames / 1024.0, named = 0;
            sb.Append(" | alloc ").Append(total.ToString("0.0", ci)).Append(" KB/frame (heap growth)");
            for (int i = 0; i < _sysBytes.Length; i++) { _sysAcc[i] = _sysWin[i] / (double)_heapFrames / 1024.0; named += _sysAcc[i]; _sysOrder[i] = i; }
            Array.Sort(_sysOrder, (a, b) => _sysAcc[b].CompareTo(_sysAcc[a]));
            foreach (int i in _sysOrder)
                if (_sysAcc[i] >= 0.05) sb.Append(", ").Append(ReflexAllocMeter.Names[i]).Append(' ').Append(_sysAcc[i].ToString("0.0", ci));
            if (total - named >= 0.05) sb.Append(", other ").Append((total - named).ToString("0.0", ci));
            if (_gcMarkers.Count > 0) sb.Append(", gc step max ").Append(_gcStepMax.ToString("0.0", ci)).Append(" ms");
        }

        string Sample(string head, bool objects)
        {
            var ci = CultureInfo.InvariantCulture;
            var sb = new StringBuilder(768);
            double avg = _frames > 0 ? _acc / _frames : 0;
            sb.Append(head).Append(": ");
            sb.Append(string.Format(ci, "fps {0:0.0} avg / {1:0.0} min (worst {2:0.0} ms), frame {3:0.00} ms", avg > 0 ? 1 / avg : 0,
                                    _worst > 0 ? 1 / _worst : 0, _worst * 1000, avg * 1000));
            if (_timings > 0) sb.Append(string.Format(ci, ", cpu main {0:0.00} render {1:0.00}, gpu {2:0.00} ms", _main / _timings, _render / _timings, _gpu / _timings));
            if (_jobs > 0)
                sb.Append(string.Format(ci, " | sim {0} jobs ({1:0.00} frames/job, {2} merged), job {3:0.00} ms (max {4:0.0}), original {5:0.00} ms",
                                        _jobs, (double)_jobFrames / _jobs, _merged, _jobMs / _jobs, _jobMax, _origMs / _jobs));
            else sb.Append(" | sim no jobs");
            if (_applyN > 0) sb.Append(string.Format(ci, ", opponents apply {0:0.00} ms", _applyMs / _applyN));
            int gc = GC.CollectionCount(0);
            sb.Append(string.Format(ci, " | mono {0:0.0} MB, gc {1} (+{2})", GC.GetTotalMemory(false) / 1048576.0, gc, gc - _gc0));
            _gc0 = gc;
            if (_gcFrames > 0) sb.Append(string.Format(ci, ", gc alloc {0:0.0} KB/frame", _gcBytes / _gcFrames / 1024.0));
            sb.Append(string.Format(ci, ", unity alloc {0:0} / reserved {1:0} MB", Profiler.GetTotalAllocatedMemoryLong() / 1048576.0,
                                    Profiler.GetTotalReservedMemoryLong() / 1048576.0));
            foreach (var name in s_mem)
                if (_rec.TryGetValue(name, out var r) && r.Valid && r.CurrentValue > 0)
                    sb.Append(", ").Append(name.Replace(" Memory", "").ToLowerInvariant()).Append(' ').Append((r.CurrentValue / 1048576.0).ToString("0", ci)).Append(" MB");
            foreach (var name in s_render)
                if (_rec.TryGetValue(name, out var r) && r.Valid)
                    sb.Append(", ").Append(name.Replace(" Count", "").ToLowerInvariant()).Append(' ').Append(r.LastValue.ToString(ci));
            string nat = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex ? null : ReflexWorldNative.PerfStats(_stats);
            if (!string.IsNullOrEmpty(nat)) sb.Append(" | plugin ").Append(nat);
            AppendAlloc(sb, ci);
            if (objects)
            {
                int go = Resources.FindObjectsOfTypeAll<GameObject>().Length, me = Resources.FindObjectsOfTypeAll<Mesh>().Length,
                    tx = Resources.FindObjectsOfTypeAll<Texture>().Length, ma = Resources.FindObjectsOfTypeAll<Material>().Length,
                    rtx = Resources.FindObjectsOfTypeAll<RenderTexture>().Length;
                var srcs = FindObjectsByType<AudioSource>(FindObjectsInactive.Include, FindObjectsSortMode.None);
                int playing = 0;
                foreach (var s in srcs) if (s && s.isPlaying) playing++;
                sb.Append(string.Format(ci, " | objects: gameobjects {0}, meshes {1}, textures {2}, materials {3}, rendertextures {4}, audio sources {5} ({6} playing)",
                                        go, me, tx, ma, rtx, srcs.Length, playing));
            }
            return sb.ToString();
        }
    }
}
