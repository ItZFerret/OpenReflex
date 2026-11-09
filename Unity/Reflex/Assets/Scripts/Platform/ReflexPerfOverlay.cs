// PERFORMANCE OVERLAY (OPTIONS > VIDEO, ReflexSettings.PerfOverlay; default off): a small panel in the top right corner for testers, in the
// career pages' look (OcUi.Panel: the orange title bar over dark glass). Not part of the original game.
//   FPS / FRAME   presented frames per second, the mean frame time and the worst frame of the last second
//   CPU           main thread and render thread (FrameTimingManager; the players are built with Frame Timing Stats)
//   GPU           the GPU frame time (FrameTimingManager; "-" where the driver reports none)
//   GAME          game frames per second, the simulation thread's job (the core frame + its exports) and the core frame alone,
//                 the pacing (same-frame / one frame later) and its budget
//   DRAWS         SetPass calls, draw calls (development players only report them), triangles
//   LIGHTING      ORIGINAL / ENHANCED and the SHADOW QUALITY tier, the resolution and render scale
// The text is refreshed four times a second (a UI Toolkit label change re-lays out the panel; once a frame would cost more than it shows).
using System;
using System.Text;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using Reflex.Port.Rendering;
using Unity.Profiling;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Platform
{
    [DefaultExecutionOrder(30002)]
    public sealed class ReflexPerfOverlay : MonoBehaviour
    {
        static ReflexPerfOverlay s_inst;
        /// Shows or hides the overlay (the option; tests).
        public static void Apply(bool on)
        {
            if (on && !s_inst)
            {
                var go = new GameObject("Reflex Performance Overlay") { hideFlags = HideFlags.HideAndDontSave };
                DontDestroyOnLoad(go);
                s_inst = go.AddComponent<ReflexPerfOverlay>();
            }
            if (s_inst) s_inst.SetVisible(on);
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            bool on = false;
            try { on = ReflexSettings.PerfOverlay; } catch (Exception) { }
            if (Environment.GetEnvironmentVariable("REFLEX_PERF_OVERLAY") == "1") on = true;
            if (on) Apply(true);
        }

        UIDocument doc; VisualElement panel; Label text; bool visible;
        readonly FrameTiming[] ft = new FrameTiming[1];
        ProfilerRecorder setPass, draws, tris;
        readonly float[] frames = new float[256]; int frameCount, frameHead;
        double cpuMain, cpuRender, gpu; int timed, gpuTimed;
        float nextText;
        double lastGameFrame = double.NaN, gameFrames; float gameWindow;
        double simJob, simCore; int simN; double lastSimTime = double.NaN;
        readonly StringBuilder sb = new StringBuilder(512);

        void Awake()
        {
            var settings = Resources.Load<PanelSettings>("OpenFrontEnd/OuPanelSettings");
            if (!settings) { Debug.LogWarning("[perf overlay] OuPanelSettings missing"); enabled = false; return; }
            doc = gameObject.AddComponent<UIDocument>(); doc.panelSettings = settings; doc.sortingOrder = 995;
            var root = doc.rootVisualElement; root.pickingMode = PickingMode.Ignore;
            root.style.position = Position.Absolute; root.style.left = 0; root.style.right = 0; root.style.top = 0; root.style.bottom = 0;
            OuTheme.Apply(root);
            panel = OcUi.Panel(root, "PERFORMANCE", out var body, out var title);
            panel.pickingMode = PickingMode.Ignore;
            panel.style.position = Position.Absolute; panel.style.right = 18; panel.style.top = 18; panel.style.width = 470;
            var bar = title.parent; bar.style.height = 34; bar.style.paddingLeft = 14; bar.style.paddingRight = 14;
            title.style.fontSize = 26; title.style.marginTop = -2;
            body.style.paddingLeft = 14; body.style.paddingRight = 14; body.style.paddingTop = 8; body.style.paddingBottom = 10;
            text = OuUi.Text("", "oc-text", body); text.enableRichText = false;
            text.style.fontSize = 21; text.style.color = new Color(0.88f, 0.88f, 0.8f, 1f); text.style.whiteSpace = WhiteSpace.Normal;
            setPass = ProfilerRecorder.StartNew(ProfilerCategory.Render, "SetPass Calls Count");
            draws = ProfilerRecorder.StartNew(ProfilerCategory.Render, "Draw Calls Count");
            tris = ProfilerRecorder.StartNew(ProfilerCategory.Render, "Triangles Count");
        }

        void OnDestroy() { setPass.Dispose(); draws.Dispose(); tris.Dispose(); if (s_inst == this) s_inst = null; }

        void SetVisible(bool on)
        {
            visible = on;
            if (panel != null) panel.style.display = on ? DisplayStyle.Flex : DisplayStyle.None;
            if (on) FrameTimingManager.CaptureFrameTimings();
        }

        void LateUpdate()
        {
            if (!visible || text == null) return;
            float dt = Time.unscaledDeltaTime * 1000f;
            frames[frameHead] = dt; frameHead = (frameHead + 1) % frames.Length; frameCount = Math.Min(frameCount + 1, frames.Length);
            FrameTimingManager.CaptureFrameTimings();
            if (FrameTimingManager.GetLatestTimings(1, ft) > 0 && ft[0].cpuFrameTime > 0)
            {
                cpuMain += ft[0].cpuMainThreadFrameTime; cpuRender += ft[0].cpuRenderThreadFrameTime; timed++;
                if (ft[0].gpuFrameTime > 0) { gpu += ft[0].gpuFrameTime; gpuTimed++; }
            }
            var rt = ReflexClientLive.Runtime;
            gameWindow += Time.unscaledDeltaTime;
            if (rt && rt.Running && !rt.paused)
            {
                double g = rt.PresentedGameFrame;
                if (!double.IsNaN(g)) { if (!double.IsNaN(lastGameFrame) && g > lastGameFrame) gameFrames += g - lastGameFrame; lastGameFrame = g; }
                var n = rt.Presentation?.Newest;
                if (n != null && n.simTime != lastSimTime) { lastSimTime = n.simTime; simJob += n.milliseconds; simCore += n.frameMilliseconds; simN++; }
            }
            else lastGameFrame = double.NaN;
            if (Time.unscaledTime < nextText) return;
            nextText = Time.unscaledTime + 0.25f;
            Compose(rt);
        }

        void Compose(OriginalWorldRuntime rt)
        {
            // the last second of frames: mean and worst
            float sum = 0, worst = 0, span = 0; int n = 0;
            for (int i = 0; i < frameCount && span < 1000f; i++)
            {
                float f = frames[(frameHead - 1 - i + frames.Length) % frames.Length];
                sum += f; span += f; n++; if (f > worst) worst = f;
            }
            float mean = n > 0 ? sum / n : 0;
            sb.Clear();
            sb.Append("FPS ").Append(mean > 0 ? (1000f / mean).ToString("0") : "-").Append("   FRAME ").Append(mean.ToString("0.0")).Append(" ms   WORST ").Append(worst.ToString("0.0")).Append('\n');
            sb.Append("CPU MAIN ").Append(timed > 0 ? (cpuMain / timed).ToString("0.0") : "-").Append("   RENDER ").Append(timed > 0 ? (cpuRender / timed).ToString("0.0") : "-").Append(" ms\n");
            sb.Append("GPU ").Append(gpuTimed > 0 ? (gpu / gpuTimed).ToString("0.0") + " ms" : "-").Append('\n');
            if (rt && rt.Running)
            {
                sb.Append("GAME ").Append(gameWindow > 0 ? (gameFrames / gameWindow).ToString("0") : "-").Append(" FPS   SIM ").Append(simN > 0 ? (simJob / simN).ToString("0.0") : "-")
                  .Append(" ms (CORE ").Append(simN > 0 ? (simCore / simN).ToString("0.0") : "-").Append(")\n");
                if (rt.Pacing == OriginalWorldRuntime.PacingMode.Original)
                    sb.Append("PACING ").Append(rt.SyncFrames ? "SAME FRAME" : "ONE FRAME LATER").Append("   BUDGET ").Append(rt.PacingBudgetMs.ToString("0.0")).Append(" ms   QUEUE ").Append(QualitySettings.maxQueuedFrames).Append(Reflex.Port.Options.ReflexClientLive.LatencySteppedBack ? " (LOW LATENCY STEPPED BACK)" : "").Append('\n');
                else sb.Append("PACING ").Append(rt.Pacing.ToString().ToUpperInvariant()).Append("   QUEUE ").Append(QualitySettings.maxQueuedFrames).Append(Reflex.Port.Options.ReflexClientLive.LatencySteppedBack ? " (LOW LATENCY STEPPED BACK)" : "").Append('\n');
            }
            long dc = draws.Valid ? draws.LastValue : 0;
            sb.Append("SETPASS ").Append(setPass.Valid ? setPass.LastValue.ToString() : "-");
            if (dc > 0) sb.Append("   DRAWS ").Append(dc);
            sb.Append("   TRIS ").Append(tris.Valid ? (tris.LastValue / 1e6).ToString("0.00") + " M" : "-").Append('\n');
            bool enh = ReflexEnhancedLighting.Requested;
            sb.Append("LIGHTING ").Append(enh ? "ENHANCED " + (ReflexEnhancedLighting.Tier == 0 ? "LOW" : ReflexEnhancedLighting.Tier == 1 ? "MEDIUM" : "HIGH") : "ORIGINAL")
              .Append("   ").Append(Screen.width).Append('x').Append(Screen.height);
            if (ReflexSettings.RenderScale != 100) sb.Append(" @ ").Append(ReflexSettings.RenderScale).Append('%');
            text.text = sb.ToString();
            cpuMain = cpuRender = gpu = 0; timed = gpuTimed = 0;
            gameFrames = 0; gameWindow = 0; simJob = simCore = 0; simN = 0;
        }
    }
}
