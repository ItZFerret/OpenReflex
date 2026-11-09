// ReflexLatencyGuard: LOW LATENCY steps back to Unity's own queued frames while the frames do not fit one refresh.
// With VSync and one queued frame a frame's CPU work waits until the previous frame is on screen, so the CPU and the GPU no longer
// overlap: when CPU main + GPU passes the refresh the frame waits a second refresh and the rate falls to half the display's
// (2026-11-02 report: a 75 Hz monitor, a National with 11 AI riders, CPU main 13.5 + GPU 2-4 ms: 37.5 fps with LOW LATENCY, 74.9 without).
// The guard counts the frames that missed a refresh; when they are 20 % of a 90-frame window it gives the driver Unity's two frames
// again (ReflexClientLive.LatencySteppedBack) and writes a "[Latency]" line to the session log. It tries one frame again when the
// measured work (CPU main without its wait for Present, plus GPU) fits 80 % of a refresh for a whole window, after a back-off
// (15 s, doubled whenever a retry fails at once, at most 240 s) or at once on a new scene. Without frame timings (no GPU time) it
// only retries on a new scene. Only with VSync: without it one queued frame does not halve anything. The saved choice never changes.
using System;
using Reflex.Port.Platform;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.Options
{
    [DefaultExecutionOrder(30002)]
    public sealed class ReflexLatencyGuard : MonoBehaviour
    {
        const int Window = 90;
        const float MissedShare = 0.2f, FitShare = 0.8f, FirstBackoff = 15f, MaxBackoff = 240f, FailedRetry = 10f, Hitch = 0.25f;

        static ReflexLatencyGuard s_inst;
        readonly FrameTiming[] _ft = new FrameTiming[1];
        int _frames, _missed, _timed;
        double _work;
        float _backoff = FirstBackoff, _steppedAt = -1000f, _retriedAt = -1000f;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (s_inst) return;
            var go = new GameObject("Reflex Latency Guard") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            s_inst = go.AddComponent<ReflexLatencyGuard>();
        }

        void OnEnable() => SceneManager.activeSceneChanged += SceneChanged;
        void OnDisable() => SceneManager.activeSceneChanged -= SceneChanged;

        void SceneChanged(Scene from, Scene to)
        {
            ResetWindow();
            _backoff = FirstBackoff;
            if (ReflexClientLive.LatencySteppedBack) Retry("a new scene");
        }

        void ResetWindow() { _frames = 0; _missed = 0; _timed = 0; _work = 0; }

        void Update()
        {
            // only while LOW LATENCY is what decides the queue (not a REFLEX_MAX_QUEUED_FRAMES override) and VSync paces the frames
            if (!ReflexClientLive.LowLatencyRequested || QualitySettings.vSyncCount <= 0)
            {
                if (ReflexClientLive.LatencySteppedBack) { ReflexClientLive.LatencySteppedBack = false; ReflexClientLive.ApplyLatency(); }
                ResetWindow();
                return;
            }
            double hz = Screen.currentResolution.refreshRateRatio.value;
            float dt = Time.unscaledDeltaTime;
            if (!(hz > 1) || dt > Hitch) { ResetWindow(); return; }   // a load or a pause: not a pacing sample
            double period = QualitySettings.vSyncCount / hz;
            _frames++;
            if (dt > 1.5 * period) _missed++;
            FrameTimingManager.CaptureFrameTimings();
            if (FrameTimingManager.GetLatestTimings(1, _ft) > 0 && _ft[0].cpuMainThreadFrameTime > 0 && _ft[0].gpuFrameTime > 0)
            {
                _work += Math.Max(0.0, _ft[0].cpuMainThreadFrameTime - _ft[0].cpuMainThreadPresentWaitTime) + _ft[0].gpuFrameTime;
                _timed++;
            }
            if (_frames < Window) return;
            float now = Time.realtimeSinceStartup;
            double work = _timed > Window / 2 ? _work / _timed : -1;   // milliseconds, -1 unknown
            if (!ReflexClientLive.LatencySteppedBack)
            {
                if (_missed >= MissedShare * _frames)
                {
                    if (now - _retriedAt < FailedRetry) _backoff = Mathf.Min(_backoff * 2f, MaxBackoff);
                    _steppedAt = now;
                    ReflexClientLive.LatencySteppedBack = true;
                    ReflexClientLive.ApplyLatency();
                    Log($"LOW LATENCY steps back to {QualitySettings.maxQueuedFrames} queued frames: {_missed} of {_frames} frames missed the {hz:0.#} Hz refresh"
                        + (work >= 0 ? $" (CPU main + GPU {work:0.0} ms of {period * 1000:0.0} ms)" : "") + $"; it tries one again in {_backoff:0} s at the earliest");
                }
            }
            else if (work >= 0 && work < FitShare * period * 1000 && now - _steppedAt >= _backoff)
                Retry($"CPU main + GPU {work:0.0} ms fit the {period * 1000:0.0} ms refresh");
            ResetWindow();
        }

        void Retry(string why)
        {
            _retriedAt = Time.realtimeSinceStartup;
            ReflexClientLive.LatencySteppedBack = false;
            ReflexClientLive.ApplyLatency();
            Log($"LOW LATENCY tries {QualitySettings.maxQueuedFrames} queued frame again: {why}");
        }

        static void Log(string line)
        {
            ReflexCrashLog.WriteLine("[Latency] " + line);
            Debug.Log("[Latency] " + line);
        }
    }
}
