#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // Diagnostics only, off unless REFLEX_PACING_PROBE=<seconds> is set (editor play mode or a player): rides the event (W held after
    // the gate drop, or after 2 s in free ride), measures frame pacing and cost, logs a summary every 5 s and at the end, then quits
    // (Done = true; a player quits itself). Options: REFLEX_PROBE_FPS (Application.targetFrameRate after the runtime set its own;
    // -1 = display rate), REFLEX_PROBE_VSYNC (QualitySettings.vSyncCount), REFLEX_PROBE_WARMUP (s of riding before measuring, 5).
    // Per window: Unity frame time (mean / p99 / max), simulation cost per job (mean / max: OriginalSimulation.Results.milliseconds,
    // and the original frames alone), the game's own clock against the wall clock, and presentation jitter = displayed game time
    // advance minus real time per Unity frame (RMS, p99), repeats (no advance) and skips (> 1.5 game frames in one Unity frame).
    public sealed class OriginalPacingProbe : MonoBehaviour
    {
        public static bool Done { get; private set; }
        public static bool Measuring { get; private set; }   // inside the measured window (for profiler passes)
        public static string Summary { get; private set; } = "";
        float seconds, warmup; double start = -1, measureStart = -1, windowStart, lastStamp = double.NaN, lastGameTime = double.NaN;
        OriginalWorldRuntime runtime; int gcStart; Unity.Profiling.ProfilerRecorder allocRec; long allocSum; double allocT0; bool shotTaken;
        readonly List<double> frame = new List<double>(), err = new List<double>(), sim = new List<double>(), orig = new List<double>(), apply = new List<double>(), lag = new List<double>(), gdt = new List<double>();
        int repeats, skips, held, unityFrames, jobs, merged; double gameAdvance, stampAdvance, presentedAdvance, realAdvance;
        // totals over the whole measurement
        readonly List<double> allFrame = new List<double>(), allErr = new List<double>(), allSim = new List<double>(), allOrig = new List<double>(), allLag = new List<double>(), allGdt = new List<double>();
        int allRepeats, allSkips, allHeld, allFrames, allJobs, allMerged; double allGame, allStamp, allPresented, allReal;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            string s = Environment.GetEnvironmentVariable("REFLEX_PACING_PROBE");
            if (string.IsNullOrEmpty(s) || FindAnyObjectByType<OriginalPacingProbe>()) return;
            var go = new GameObject("Reflex pacing probe"); DontDestroyOnLoad(go);
            var p = go.AddComponent<OriginalPacingProbe>();
            p.seconds = float.Parse(s, CultureInfo.InvariantCulture);
            p.warmup = float.Parse(Environment.GetEnvironmentVariable("REFLEX_PROBE_WARMUP") ?? "5", CultureInfo.InvariantCulture);
            Done = false;
        }

        static int? EnvInt(string k) => int.TryParse(Environment.GetEnvironmentVariable(k), NumberStyles.Integer, CultureInfo.InvariantCulture, out int v) ? v : (int?)null;

        void Update()
        {
            if (Done) return;
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            if (start < 0 && Time.realtimeSinceStartup > 300) { Finish("no running world after 300 s"); return; }
            if (start >= 0 && runtime && !runtime.Running) { Finish("world stopped during measurement"); return; }
            if (!runtime || !runtime.Running || runtime.Presentation?.Newest == null) return;
            if (EnvInt("REFLEX_PROBE_FPS") is int fps) Application.targetFrameRate = fps;
            if (EnvInt("REFLEX_PROBE_VSYNC") is int vs) QualitySettings.vSyncCount = vs;
            if (EnvInt("REFLEX_PROBE_NOGC") == 1 && UnityEngine.Scripting.GarbageCollector.GCMode != UnityEngine.Scripting.GarbageCollector.Mode.Disabled) UnityEngine.Scripting.GarbageCollector.GCMode = UnityEngine.Scripting.GarbageCollector.Mode.Disabled;
            double now = Time.realtimeSinceStartupAsDouble;
            var race = runtime.Race;
            // The open worker returns loading results before the race exists. An absent
            // race is not evidence of FreeRide, and must not start the measurement timer.
            bool freeRide = runtime.level.StartsWith("FR_", StringComparison.OrdinalIgnoreCase);
            if ((race.valid == 0 && !freeRide) || runtime.Bike.frames == 0) return;
            if (start < 0) start = now;
            // Event construction briefly exposes an InProgress activity before the
            // intro. The race clock must advance before this counts as a riding run.
            bool riding = freeRide ? now - start > 2.0 : (race.state == 2 || race.state == 5) && race.race_time > 0f;
            runtime.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();   // W
            if (now - start > 240) { Finish("timeout before the race"); return; }
            if (!riding) return;
            if (measureStart < 0) { measureStart = now + warmup; windowStart = measureStart; gcStart = GC.CollectionCount(0); allocRec = Unity.Profiling.ProfilerRecorder.StartNew(Unity.Profiling.ProfilerCategory.Memory, "GC Allocated In Frame"); allocSum = 0; allocT0 = now; }
            else if (allocRec.Valid) allocSum += allocRec.LastValue;
            if (now < measureStart) return;
            Measuring = true;
            var p = runtime.Presentation; var n = p.Newest;
            float realDt = Time.unscaledDeltaTime;
            frame.Add(realDt * 1000.0); unityFrames++;
            double e = p.PresentedDt - realDt; err.Add(e * 1000.0);
            if (p.PresentedDt <= 1e-7) repeats++;
            if (p.PresentedDt > 1.5 * OriginalPresentation.Step) skips++;
            if (p.Held) held++;
            presentedAdvance += p.PresentedDt; realAdvance += realDt;
            if (n != null && n.simTime != lastStamp)
            {
                if (!double.IsNaN(lastStamp)) { stampAdvance += n.simTime - lastStamp; gameAdvance += n.gameTime - lastGameTime; }
                lastStamp = n.simTime; lastGameTime = n.gameTime; jobs++; if (n.merged) merged++;
                sim.Add(n.milliseconds); orig.Add(n.frameMilliseconds); gdt.Add(n.gameDt * 1000.0);
            }
            if (runtime.opponentRiders) apply.Add(runtime.opponentRiders.LastApplyMilliseconds);
            if (now - windowStart >= 5) { Report("window", false); windowStart = now; }
            string shot = Environment.GetEnvironmentVariable("REFLEX_PROBE_SHOT");   // a screenshot half way (players: the window)
            if (!shotTaken && !string.IsNullOrEmpty(shot) && now - measureStart >= seconds * 0.5) { shotTaken = true; ScreenCapture.CaptureScreenshot(shot); }
            if (now - measureStart >= seconds) { Report("window", false); Report("total", true); Finish("done"); }
        }

        // Input-to-display lag, after every Update of the frame: the time since the input of the game frame(s) shown was read (blended
        // frames weighted by the blend). The display's own latency (GPU, compositor) comes on top, the same in every pacing.
        void LateUpdate()
        {
            if (Done || !Measuring || !runtime || runtime.Presentation == null) return;
            var p = runtime.Presentation; if (p.A == null || p.B == null) return;
            double shown = p.A.inputClock + (p.B.inputClock - p.A.inputClock) * p.Alpha;
            if (shown > 0) lag.Add((OriginalSimulation.Clock - shown) * 1000.0);
        }

        static double Mean(List<double> v) { double s = 0; foreach (var x in v) s += x; return v.Count > 0 ? s / v.Count : 0; }
        static double Max(List<double> v) { double m = 0; foreach (var x in v) m = Math.Max(m, x); return m; }
        static double Min(List<double> v) { double m = v.Count > 0 ? double.MaxValue : 0; foreach (var x in v) m = Math.Min(m, x); return m; }
        static double Rms(List<double> v) { double s = 0; foreach (var x in v) s += x * x; return v.Count > 0 ? Math.Sqrt(s / v.Count) : 0; }
        static double P99Abs(List<double> v) { if (v.Count == 0) return 0; var a = v.ConvertAll(Math.Abs); a.Sort(); return a[Math.Min(a.Count - 1, (int)(a.Count * 0.99))]; }

        void Report(string what, bool total)
        {
            if (!total)
            {
                allFrame.AddRange(frame); allErr.AddRange(err); allSim.AddRange(sim); allOrig.AddRange(orig); allLag.AddRange(lag); allGdt.AddRange(gdt);
                allRepeats += repeats; allSkips += skips; allHeld += held; allFrames += unityFrames; allJobs += jobs; allMerged += merged;
                allGame += gameAdvance; allStamp += stampAdvance; allPresented += presentedAdvance; allReal += realAdvance;
            }
            var F = total ? allFrame : frame; var E = total ? allErr : err; var S = total ? allSim : sim; var O = total ? allOrig : orig; var L = total ? allLag : lag; var G = total ? allGdt : gdt;
            int rep = total ? allRepeats : repeats, sk = total ? allSkips : skips, h = total ? allHeld : held, uf = total ? allFrames : unityFrames, jb = total ? allJobs : jobs, mg = total ? allMerged : merged;
            double ga = total ? allGame : gameAdvance, sa = total ? allStamp : stampAdvance, pa = total ? allPresented : presentedAdvance, ra = total ? allReal : realAdvance;
            var p = runtime.Presentation;
            string mode = runtime.Pacing.ToString().ToLowerInvariant() + (runtime.Pacing == OriginalWorldRuntime.PacingMode.Original ? (runtime.SyncFrames ? " (same-frame)" : " (one frame later)") + Inv($" busy {runtime.SkippedBusy}") : "");
            string line = Inv($"[pacing] {what} {mode} frames {uf} fps {uf / Math.Max(ra, 1e-6):0.0}  frame ms mean {Mean(F):0.00} p99 {P99(F):0.00} max {Max(F):0.00}  ") +
                Inv($"sim jobs {jb} ({mg} merged) ms mean {Mean(S):0.00} max {Max(S):0.00} (original frames {Mean(O):0.00})  ") +
                Inv($"jitter rms {Rms(E):0.00} ms p99 {P99Abs(E):0.00} ms  repeats {rep} skips {sk} held {h}  ") +
                Inv($"clock: real {ra:0.000} s presented {pa:0.000} s game-frames {sa:0.000} s game's own {ga:0.000} s ({(sa > 0 ? 100 * (ga / sa - 1) : 0):+0.00;-0.00}%)  delay {p.Delay * 1000:0.0} ms") +
                Inv($"  gc {GC.CollectionCount(0) - gcStart} heap {GC.GetTotalMemory(false) / 1e6:0} MB alloc {allocSum / 1e6 / Math.Max(1e-3, Time.realtimeSinceStartupAsDouble - allocT0):0.0} MB/s") + (runtime.opponentRiders ? Inv($"  opponents apply {Mean(apply):0.00} ms") : "") + (runtime.ambient ? Inv($"  ambient main {runtime.ambient.mainThreadMs:0.00} ms") : "") +
                Inv($"  input lag ms mean {Mean(L):0.0} p99 {P99(L):0.0} max {Max(L):0.0}  game dt ms mean {Mean(G):0.00} min {Min(G):0.00} max {Max(G):0.00}");
            Debug.Log(line);
            if (total) Summary = line;
            frame.Clear(); err.Clear(); sim.Clear(); orig.Clear(); apply.Clear(); lag.Clear(); gdt.Clear(); repeats = skips = held = unityFrames = jobs = merged = 0; gameAdvance = stampAdvance = presentedAdvance = realAdvance = 0;
        }
        static string Inv(FormattableString f) => FormattableString.Invariant(f);
        static double P99(List<double> v) { if (v.Count == 0) return 0; var a = new List<double>(v); a.Sort(); return a[Math.Min(a.Count - 1, (int)(a.Count * 0.99))]; }

        void Finish(string why)
        {
            if (runtime) Debug.Log(Inv($"[pacing] final race state {runtime.Race.state}, bike frame {runtime.Bike.frames}, speed {runtime.Bike.speed:0.000}, drivers {runtime.DriverCount}"));
            Debug.Log("[pacing] finished: " + why);
            Done = true; Measuring = false; if (allocRec.Valid) allocRec.Dispose();
            if (runtime) runtime.testHeldKeys = Array.Empty<int>();
            if (!Application.isEditor) Application.Quit(why == "done" ? 0 : 1);
        }
    }
}

#endif
