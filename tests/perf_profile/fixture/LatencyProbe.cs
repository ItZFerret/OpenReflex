// TEST ONLY (tests/perf_profile, bench.py --latency): input-to-presentation latency of the running world, in Unity frames and ms.
// Off unless REFLEX_LATENCY_PROBE=1. While the world runs (the bench's live window: rt.Running, not paused) it adds a virtual keyboard
// and toggles W (throttle) every REFLEX_LATENCY_PERIOD seconds (0.5). The key event is queued in this script's LateUpdate of Unity frame
// N, so the Input System hands it to the runtime's Update of frame N + 1 (= the frame a real event arriving during frame N reaches; a
// real device's own age comes on top: XInput is polled at InputSystem.pollingFrequency, keyboards are event driven).
// Per edge it records the first Unity frame whose PRESENTED game frame (rt.Presentation.view: what ApplyView put on the bike this frame)
// ran with the new key state (the game's own input block S+1c, view.bike.inputs[0] throttle), how many core frames and ms that took,
// and the pacing state. Logged as "[latency] ..." lines; the player's own exit is the bench's.
// It also logs the PlayerLoop order once (where the Input System's update sits against TimeUpdate.WaitForLastPresentation).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

[DefaultExecutionOrder(32001)]
public sealed class LatencyProbe : MonoBehaviour
{
    const string Tag = "[latency] ";
    static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);
    Keyboard kb; OriginalWorldRuntime rt; bool forced;
    float period = 0.5f; double nextEdge = double.NaN;
    bool down; int edgeFrame = -1; double edgeTime; ulong edgeCoreFrame; int edges, skipped;
    readonly List<double> frames = new List<double>(), ms = new List<double>(), coreFrames = new List<double>();
    readonly Dictionary<string, List<double>> byMode = new Dictionary<string, List<double>>();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_LATENCY_PROBE") != "1" || FindAnyObjectByType<LatencyProbe>()) return;
        var g = new GameObject("latency probe (test)"); DontDestroyOnLoad(g); g.AddComponent<LatencyProbe>();
    }

    void Start()
    {
        if (float.TryParse(Environment.GetEnvironmentVariable("REFLEX_LATENCY_PERIOD"), NumberStyles.Float, CultureInfo.InvariantCulture, out float p) && p > 0.05f) period = p;
        var sb = new StringBuilder(Tag + "player loop:");
        var loop = UnityEngine.LowLevel.PlayerLoop.GetCurrentPlayerLoop();
        foreach (var top in loop.subSystemList ?? Array.Empty<UnityEngine.LowLevel.PlayerLoopSystem>())
        {
            sb.Append(' ').Append(top.type?.Name).Append('[');
            foreach (var sub in top.subSystemList ?? Array.Empty<UnityEngine.LowLevel.PlayerLoopSystem>())
            {
                string n = sub.type?.Name ?? "?";
                if (n.Contains("Input") || n.Contains("Present") || n.Contains("ScriptRun") || n.Contains("WaitFor") || n.Contains("Finish") || n.Contains("Gfx"))
                    sb.Append(n).Append(' ');
            }
            sb.Append(']');
        }
        Debug.Log(sb.ToString());
        Debug.Log(Tag + Inv($"input: pollingFrequency {InputSystem.pollingFrequency} Hz, updateMode {InputSystem.settings.updateMode}, maxQueuedFrames {QualitySettings.maxQueuedFrames}, vSync {QualitySettings.vSyncCount}, target {Application.targetFrameRate}, batch {Application.isBatchMode}"));
    }

    void LateUpdate()
    {
        if (!rt) rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var p = rt ? rt.Presentation : null;
        if (!rt || !rt.Running || rt.paused || p == null || p.Newest == null) { nextEdge = double.NaN; return; }
        double now = Time.realtimeSinceStartupAsDouble;
        // REFLEX_LATENCY_VSYNC / REFLEX_LATENCY_TARGET: the display policy forced once the world runs (e.g. 0 / -1: VSync off, uncapped,
        // the original's own default; the options' FRAME RATE CAP OFF + VSYNC OFF)
        if (!forced)
        {
            forced = true;
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_LATENCY_VSYNC"), out int vs)) QualitySettings.vSyncCount = vs;
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_LATENCY_TARGET"), out int tf)) Application.targetFrameRate = tf;
            Debug.Log(Tag + Inv($"live: vSync {QualitySettings.vSyncCount}, target {Application.targetFrameRate}, maxQueuedFrames {QualitySettings.maxQueuedFrames}"));
        }
        // the presented frame's throttle (the game's input block) against the key state in force since edgeFrame
        float shown = p.view.bike.inputs != null && p.view.bike.inputs.Length > 0 ? p.view.bike.inputs[0] : 0f;
        if (edgeFrame >= 0 && Time.frameCount >= edgeFrame && (shown > 0.5f) == down)
        {
            double f = Time.frameCount - edgeFrame, t = (now - edgeTime) * 1000.0, c = (double)(p.view.nativeFrame - edgeCoreFrame);
            string mode = rt.Pacing == OriginalWorldRuntime.PacingMode.Original ? (rt.SyncFrames ? "original-same" : "original-later") : rt.Pacing.ToString().ToLowerInvariant();
            frames.Add(f); ms.Add(t); coreFrames.Add(c);
            if (!byMode.TryGetValue(mode, out var l)) byMode[mode] = l = new List<double>(); l.Add(f);
            var n = p.Newest;
            Debug.Log(Tag + Inv($"edge {edges} {(down ? "down" : "up")}: shown in Unity frame +{f:0} ({t:0.0} ms after the event was queued), core frames +{c:0}, {mode}, unity dt {Time.unscaledDeltaTime * 1000:0.0} ms, job {n.milliseconds:0.0} ms, cap wait {CapWait(n):0.0} ms, delay {p.Delay * 1000:0.0} ms, alpha {p.Alpha:0.00}, pre-input wait {PreWait():0.0} ms, input to frame start {(n.frameStartClock - n.inputClock) * 1000:0.0} ms, lead {Lead():0.00} ms, game dt {n.gameDt * 1000:0.00} ms"));
            edgeFrame = -1;
        }
        if (double.IsNaN(nextEdge)) { nextEdge = now + 1.0; return; }   // the world settles first
        if (now < nextEdge) return;
        if (edgeFrame >= 0) { skipped++; Debug.Log(Tag + Inv($"edge {edges} not shown within {period:0.00} s")); }
        // queue the next edge now: the Input System processes it at the start of the next frame, before the runtime's Update reads keys
        kb ??= InputSystem.AddDevice<Keyboard>("latency probe keyboard");
        kb.MakeCurrent();
        down = !down; edges++;
        InputSystem.QueueStateEvent(kb, down ? new KeyboardState(Key.W) : new KeyboardState());
        edgeFrame = Time.frameCount + 1; edgeTime = now; edgeCoreFrame = p.view.nativeFrame;
        nextEdge = now + period;
        if (edges % 20 == 0) Summary();
    }

    // (the field is newer than some builds this probe is compiled into: read by name)
    static readonly System.Reflection.FieldInfo capField = typeof(OriginalSimulation.Results).GetField("capWaitMilliseconds");
    static readonly System.Reflection.PropertyInfo preProp = typeof(OriginalWorldRuntime).GetProperty("PreInputWaitMs");
    static readonly System.Reflection.PropertyInfo leadProp = typeof(OriginalWorldRuntime).GetProperty("PreInputLeadMs");
    static double Lead() => leadProp != null ? (double)leadProp.GetValue(null) : double.NaN;
    static double PreWait() => preProp != null ? (double)preProp.GetValue(null) : double.NaN;
    static double CapWait(OriginalSimulation.Results n) => capField != null ? (double)capField.GetValue(n) : double.NaN;

    void OnDestroy() => Summary();
    void OnApplicationQuit() => Summary();

    void Summary()
    {
        if (frames.Count == 0) return;
        string Stat(List<double> v) { var a = v.OrderBy(x => x).ToList(); return Inv($"mean {a.Average():0.00} median {a[a.Count / 2]:0.00} min {a[0]:0.00} max {a[a.Count - 1]:0.00}"); }
        Debug.Log(Tag + Inv($"SUMMARY {frames.Count} edges ({skipped} not shown): Unity frames {Stat(frames)}; ms {Stat(ms)}; core frames {Stat(coreFrames)}; by pacing ") +
                  string.Join(", ", byMode.Select(kv => kv.Key + " " + kv.Value.Count + "x mean " + kv.Value.Average().ToString("0.00", CultureInfo.InvariantCulture))));
    }
}
