// ANTI-ALIASING: TAA (OPTIONS > VIDEO; LIGHTING ENHANCED only, not part of the original renderer). ORIGINAL never records any of this:
// the component does nothing while ReflexRenderPipeline.enhanced is null, so the original's frame stays exactly as it was.
//   jitter     the game camera's projection is offset by a Halton (2, 3) sub-pixel sequence while it renders (8 phases at native
//              resolution, 16 / 24 when upscaling); everything else (culling, the RT_Bump fit, the forest, the HUD's projections) sees
//              the plain projection: it is set in beginCameraRendering and reset in endCameraRendering. The UI is never jittered.
//   motion     URP's motion vectors (requested by the pass: its camera motion from the depth buffer for the static world, the terrain
//              with its RT_Bump displacement included, and the per-object vectors of the bikes, riders and ambient characters, whose
//              Reflex/Vehicle shader has URP's MotionVectors pass; skinned meshes through Unity's previous-frame skinning), minus the
//              jitter's own frame-to-frame offset. Particles and other transparent objects have none (as in Unity's own TAA).
//   resolve    (Shaders/ReflexTemporalAA.shader) on the HDR scene before the ENHANCED post chain, so the eye adaptation and the bloom see
//              the stable image: the current frame reconstructed at the output pixel from its 3 x 3 jittered samples (a Gaussian of the
//              distance to each sample's true position; also the upscaler), the history re-projected with the closest-depth (dilated)
//              motion vector and sampled with a 5-tap Catmull-Rom, variance clipping in YCoCg of a Reinhard-compressed colour (tighter
//              when moving), history rejection on disocclusion (no surface in last frame's motion vectors at the re-projected point moved
//              like this one: the ground revealed behind a bike), off-screen and camera cuts reset.
//   sharpen    a contrast-adaptive sharpening (AMD FidelityFX CAS) on the tonemapped image (SHARPENING 0 .. 100 %).
//   upscale    UPSCALING QUALITY (67 %) / BALANCED (58 %): the world renders at that share of the output (player builds: the URP asset's render
//              scale while the game camera renders, put back after), the resolve writes the full-size image.
// F9 cycles ANTI-ALIASING (OFF, MSAA 2X, MSAA 4X, TAA) with a label on the screen (this test build's A/B key).
using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.RenderGraphModule.Util;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    /// OPTIONS > VIDEO ANTI-ALIASING / SHARPENING / UPSCALING (saved in save.json; MSAA itself stays graphics.cfg's Antialiasing).
    public static class ReflexAntiAliasing
    {
        public enum Mode { Off = 0, Msaa2 = 1, Msaa4 = 2, Taa = 3 }
        public static readonly string[] ModeNames = { "OFF", "MSAA 2X", "MSAA 4X", "TAA" };
        public static readonly string[] UpscalingNames = { "OFF", "QUALITY", "BALANCED" };
        public static readonly float[] UpscalingScales = { 1f, 0.67f, 0.58f };
        const string KeyTaa = "Options.TemporalAA", KeySharpen = "Options.Sharpening", KeyUpscale = "Options.Upscaling";

        /// Tests: forces the mode / sharpening (0..100) / upscaling (0..2); -1 = the option. REFLEX_AA = off | msaa2 | msaa4 | taa,
        /// REFLEX_SHARPEN, REFLEX_UPSCALE at start.
        public static int ModeOverride = -1, SharpenOverride = -1, UpscaleOverride = -1;

        /// TAA chosen (the default of this test build: nothing saved = TAA). Applies with LIGHTING ENHANCED only.
        public static bool TaaSelected => ModeOverride >= 0 ? ModeOverride == (int)Mode.Taa : Options.ReflexSave.GetInt(KeyTaa, 1) == 1;
        public static Mode Current
        {
            get
            {
                if (TaaSelected) return Mode.Taa;
                if (ModeOverride >= 0) return (Mode)Mathf.Clamp(ModeOverride, 0, 2);
                int a = 0; try { a = Content.ReflexLaunchSettings.Current.antialiasing; } catch { }
                return (Mode)Mathf.Clamp(a, 0, 2);
            }
        }
        public static int Sharpening
        {
            get => SharpenOverride >= 0 ? Mathf.Clamp(SharpenOverride, 0, 100) : Mathf.Clamp(Options.ReflexSave.GetInt(KeySharpen, 50), 0, 100);
            set { Options.ReflexSave.SetInt(KeySharpen, Mathf.Clamp(value, 0, 100)); Options.ReflexSave.Save(); }
        }
        public static int Upscaling
        {
            get => UpscaleOverride >= 0 ? Mathf.Clamp(UpscaleOverride, 0, 2) : Mathf.Clamp(Options.ReflexSave.GetInt(KeyUpscale, 0), 0, 2);
            set { Options.ReflexSave.SetInt(KeyUpscale, Mathf.Clamp(value, 0, 2)); Options.ReflexSave.Save(); }
        }

        /// Chooses the mode (the options page and F9): TAA is ours (graphics.cfg Antialiasing 0, so ORIGINAL draws without MSAA as the game's
        /// OFF); OFF / MSAA 2X / 4X are graphics.cfg's Antialiasing 0 / 1 / 2 as before. Saved, applied at once.
        public static void Set(Mode m)
        {
            ModeOverride = -1;
            Options.ReflexSave.SetInt(KeyTaa, m == Mode.Taa ? 1 : 0); Options.ReflexSave.Save();
            try
            {
                var ls = Content.ReflexLaunchSettings.Current;
                ls.antialiasing = m == Mode.Taa ? 0 : (int)m;
                ls.Save();
            }
            catch (Exception e) { Debug.LogWarning("[Reflex AA] graphics.cfg not written: " + e.Message); }
            Options.ReflexClientLive.ApplyGraphicsLive();
        }

        public static void Cycle(int d) { int n = ModeNames.Length; Set((Mode)((((int)Current + d) % n + n) % n)); }

        public static string Label => Current == Mode.Taa ? (ReflexEnhancedLighting.Requested ? "TAA" : "TAA (ENHANCED)") : ModeNames[(int)Current];

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_AA");
            if (!string.IsNullOrEmpty(v)) ModeOverride = Array.FindIndex(new[] { "off", "msaa2", "msaa4", "taa" }, s => string.Equals(s, v, StringComparison.OrdinalIgnoreCase));
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_SHARPEN"), out int s0)) SharpenOverride = Mathf.Clamp(s0, 0, 100);
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_UPSCALE"), out int u0)) UpscaleOverride = Mathf.Clamp(u0, 0, 2);
        }
    }

    /// TAA for the camera of a ReflexRenderPipeline (ReflexRenderPipeline.OnEnable adds it): the jitter, the resolve and the sharpening
    /// while LIGHTING is ENHANCED and ANTI-ALIASING is TAA.
    [DisallowMultipleComponent]
    [DefaultExecutionOrder(32500)]
    public sealed class ReflexTemporalAA : MonoBehaviour
    {
        [Header("Result (read only)")]
        public bool active;
        public Vector2 jitterPixels;
        public Vector2Int inputSize, outputSize;
        public int resets;
        [TextArea(1, 3)] public string report;

        Camera cam;
        ReflexRenderPipeline pipeline;
        ReflexTemporalPass resolvePass;
        ReflexSharpenPass sharpenPass;
        Material material;
        bool applied, savedMsaa; float savedScale = -1f; UniversalRenderPipelineAsset scaledAsset;
        int frameIndex, lastFrame = -10;
        internal Vector2 jitterUv, jitterDeltaUv, prevJitterDeltaUv;   // the jitter in input uv; its change since the last frame (current, previous)
        Vector2 lastJitterUv; Vector3 lastPos; Quaternion lastRot; float lastFov; Vector2Int lastOut; int lastMode = -1;
        internal bool resetHistory = true;
        internal float sharpness;
        static int f9Frame = -1;
        static float labelUntil; static string labelText;

        /// Tests (the cost hunt): 0 everything; 1 the jitter and URP's depth + motion vectors, no resolve; 2 the jitter and the depth only;
        /// 3 the resolve drawn 5 times; 4 no sharpening pass; 5 no motion vector copy.
        public static int ProfileMode;
        /// Tests (-batchmode players render the camera into a target): the TAA as on screen (w-replayterrain's photo mode renders).
        public static bool OffscreenTest;
        /// Tests / the perf overlay: the TAA of the game camera in use (null when none ran this frame).
        public static ReflexTemporalAA Active { get; private set; }

        void OnEnable()
        {
            cam = GetComponent<Camera>();
            pipeline = GetComponent<ReflexRenderPipeline>();
            RenderPipelineManager.beginCameraRendering += OnBegin;
            RenderPipelineManager.endCameraRendering += OnEnd;
        }

        void OnDisable()
        {
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            RenderPipelineManager.endCameraRendering -= OnEnd;
            Restore();
            resolvePass?.Dispose(); resolvePass = null;
            sharpenPass = null;
            if (material) CoreUtils.Destroy(material);
            material = null;
            if (Active == this) Active = null;
        }

        void Update()
        {
            // F9: the A/B key of this test build (once per frame, whichever pipeline sees it first)
            var kb = UnityEngine.InputSystem.Keyboard.current;
            if (kb != null && kb.ctrlKey.isPressed && kb.f9Key.wasPressedThisFrame && f9Frame != Time.frameCount)
            {
                f9Frame = Time.frameCount;
                ReflexAntiAliasing.Cycle(+1);
                var m = ReflexAntiAliasing.Current;
                labelText = "ANTI-ALIASING: " + ReflexAntiAliasing.ModeNames[(int)m]
                            + (m == ReflexAntiAliasing.Mode.Taa ? $"  (SHARPENING {ReflexAntiAliasing.Sharpening}%, UPSCALING {ReflexAntiAliasing.UpscalingNames[ReflexAntiAliasing.Upscaling]})" : "")
                            + (ReflexEnhancedLighting.Requested ? "" : "  - LIGHTING ORIGINAL: TAA needs ENHANCED");
                labelUntil = Time.unscaledTime + 2.5f;
                Debug.Log("[Reflex AA] CTRL+F9: " + labelText);
            }
        }

        GUIStyle labelStyle;
        static ReflexTemporalAA guiOwner; static int guiFrame = -1;
        void OnGUI()
        {
            if (Time.unscaledTime > labelUntil || string.IsNullOrEmpty(labelText)) return;
            if (guiFrame != Time.frameCount || !guiOwner) { guiFrame = Time.frameCount; guiOwner = this; }   // one label, whichever pipeline draws first
            if (guiOwner != this) return;
            if (labelStyle == null) labelStyle = new GUIStyle(GUI.skin.label) { fontSize = Mathf.Max(14, Screen.height / 40), fontStyle = FontStyle.Bold, alignment = TextAnchor.UpperCenter };
            var r = new Rect(0, Screen.height * 0.06f, Screen.width, Screen.height * 0.1f);
            var c = GUI.color;
            GUI.color = new Color(0, 0, 0, 0.8f); GUI.Label(new Rect(r.x + 2, r.y + 2, r.width, r.height), labelText, labelStyle);
            GUI.color = Color.white; GUI.Label(r, labelText, labelStyle);
            GUI.color = c;
        }

        bool Wanted(Camera c)
        {
            if (!pipeline || !pipeline.isActiveAndEnabled || pipeline.enhanced == null || !pipeline.postProcess) return false;
            if (c.cameraType != CameraType.Game || c.targetTexture != null && !OffscreenTest) return false;   // photo mode's off-screen renders: one plain frame
            return ReflexAntiAliasing.Current == ReflexAntiAliasing.Mode.Taa;
        }

        void OnBegin(ScriptableRenderContext context, Camera c)
        {
            if (c != cam) return;
            if (!Wanted(c))
            {
                if (active) { active = false; report = "off"; if (Active == this) Active = null; }
                return;
            }
            if (!material)
            {
                var shader = Shader.Find("Hidden/Reflex/TemporalAA");
                if (!shader) { Debug.LogError("ReflexTemporalAA: Hidden/Reflex/TemporalAA not found (Always Included Shaders)"); enabled = false; return; }
                material = CoreUtils.CreateEngineMaterial(shader);
                resolvePass = new ReflexTemporalPass(this, material);
                sharpenPass = new ReflexSharpenPass(this, material);
            }
            active = true; Active = this;
            int upscale = Application.isEditor ? 0 : ReflexAntiAliasing.Upscaling;   // the URP asset is shared: changed in player builds only
            int mode = upscale;
            // the output (the camera's pixels at the RENDER SCALE option) and the input (the upscaling share of it)
            var urp = GraphicsSettings.currentRenderPipeline as UniversalRenderPipelineAsset;
            float baseScale = urp ? urp.renderScale : 1f;
            outputSize = new Vector2Int(Mathf.Max(1, (int)(c.pixelWidth * baseScale)), Mathf.Max(1, (int)(c.pixelHeight * baseScale)));
            float factor = ReflexAntiAliasing.UpscalingScales[upscale];
            if (urp && upscale > 0) { savedScale = baseScale; scaledAsset = urp; urp.renderScale = baseScale * factor; }
            float inScale = baseScale * (upscale > 0 ? factor : 1f);
            inputSize = new Vector2Int(Mathf.Max(1, (int)(c.pixelWidth * inScale)), Mathf.Max(1, (int)(c.pixelHeight * inScale)));
            if (upscale == 0) outputSize = inputSize;

            // history reset: the first frame, a gap, a new size or mode, a camera cut (a replay's new shot, a teleport, a FOV jump)
            var t = c.transform;
            bool cut = (t.position - lastPos).sqrMagnitude > 8f * 8f || Quaternion.Angle(t.rotation, lastRot) > 35f || Mathf.Abs(c.fieldOfView - lastFov) > 0.2f * Mathf.Max(lastFov, 1f);
            // (PHOTO MODE's shutter averaging sub-frames of different moments: each sub-frame its own picture, ReflexCinematic.Accumulating)
            resetHistory = Time.frameCount - lastFrame > 2 || cut || outputSize != lastOut || mode != lastMode || ReflexCinematic.Accumulating;
            if (resetHistory) resets++;
            lastPos = t.position; lastRot = t.rotation; lastFov = c.fieldOfView; lastOut = outputSize; lastMode = mode;
            bool sameFrame = Time.frameCount == lastFrame;
            lastFrame = Time.frameCount;

            // the jitter: Halton (2, 3), centred, in input pixels
            int phases = upscale == 0 ? 8 : upscale == 1 ? 16 : 24;
            if (!sameFrame) frameIndex++;
            int k = frameIndex % phases + 1;
            jitterPixels = new Vector2(Halton(k, 2) - 0.5f, Halton(k, 3) - 0.5f);
            var j = new Vector2(jitterPixels.x / inputSize.x, jitterPixels.y / inputSize.y);
            prevJitterDeltaUv = jitterDeltaUv;
            jitterDeltaUv = resetHistory ? Vector2.zero : j - lastJitterUv;
            if (resetHistory) prevJitterDeltaUv = Vector2.zero;
            lastJitterUv = jitterUv = j;

            savedMsaa = c.allowMSAA; c.allowMSAA = false;   // TAA replaces MSAA (graphics.cfg's MSAA stays the ORIGINAL mode's)
            c.ResetProjectionMatrix();
            var p = c.projectionMatrix;
            var pj = p;
            float dx = 2f * j.x, dy = 2f * j.y;   // NDC offset (GL convention, y up): row0 += dx * row3, row1 += dy * row3
            pj.m00 += dx * p.m30; pj.m01 += dx * p.m31; pj.m02 += dx * p.m32; pj.m03 += dx * p.m33;
            pj.m10 += dy * p.m30; pj.m11 += dy * p.m31; pj.m12 += dy * p.m32; pj.m13 += dy * p.m33;
            c.nonJitteredProjectionMatrix = p;
            c.projectionMatrix = pj;
            c.useJitteredProjectionMatrixForTransparentRendering = true;
            applied = true;

            sharpness = ReflexAntiAliasing.Sharpening / 100f;
            var renderer = c.GetUniversalAdditionalCameraData().scriptableRenderer;
            resolvePass.ConfigureInput(ProfileMode == 2 ? ScriptableRenderPassInput.Depth : ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(resolvePass);
            if (sharpness > 0.001f && ProfileMode != 4) renderer.EnqueuePass(sharpenPass);
            report = $"TAA {inputSize.x}x{inputSize.y} -> {outputSize.x}x{outputSize.y}, jitter {jitterPixels.x:+0.00;-0.00} {jitterPixels.y:+0.00;-0.00} ({k}/{phases}), sharpening {ReflexAntiAliasing.Sharpening}%{(resetHistory ? ", history reset" : "")}";
        }

        void OnEnd(ScriptableRenderContext context, Camera c)
        {
            if (c != cam) return;
            Restore();
        }

        void Restore()
        {
            if (!applied) return;
            applied = false;
            if (cam) { cam.ResetProjectionMatrix(); cam.allowMSAA = savedMsaa; }
            if (scaledAsset && savedScale > 0f) scaledAsset.renderScale = savedScale;
            scaledAsset = null; savedScale = -1f;
        }

        static float Halton(int index, int b)
        {
            float f = 1f, r = 0f;
            for (int i = index; i > 0; i /= b) { f /= b; r += f * (i % b); }
            return r;
        }
    }

    /// The resolve: the jittered HDR scene + depth + motion vectors + last frame's history -> the new history and the camera colour the post
    /// chain continues with (full size when upscaling). Runs after the transparent objects, before ReflexPostPass's chain.
    sealed class ReflexTemporalPass : ScriptableRenderPass, IDisposable
    {
        readonly ReflexTemporalAA owner;
        readonly Material material;
        readonly RTHandle[] history = new RTHandle[2];
        RTHandle prevMotion;
        int index;
        bool historyValid, motionValid;
        static readonly int ColorId = Shader.PropertyToID("_TaaColor"), DepthId = Shader.PropertyToID("_TaaDepth"), MotionId = Shader.PropertyToID("_TaaMotion"),
            HistoryId = Shader.PropertyToID("_TaaHistory"), PrevMotionId = Shader.PropertyToID("_TaaPrevMotion"), InSizeId = Shader.PropertyToID("_TaaInSize"),
            OutSizeId = Shader.PropertyToID("_TaaOutSize"), JitterId = Shader.PropertyToID("_TaaJitter"), ParamsId = Shader.PropertyToID("_TaaParams"),
            Params2Id = Shader.PropertyToID("_TaaParams2");
        const int PassResolve = 0;
        static bool warnedMotion;

        public ReflexTemporalPass(ReflexTemporalAA owner, Material material)
        {
            this.owner = owner; this.material = material;
            renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing - 1;   // after the transparents, before ReflexPostPass (550)
            profilingSampler = new ProfilingSampler("Reflex TAA");
            requiresIntermediateTexture = true;
            ConfigureInput(ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Motion);
        }

        public void Dispose()
        {
            history[0]?.Release(); history[1]?.Release(); prevMotion?.Release();
            history[0] = history[1] = prevMotion = null;
        }

        readonly MaterialPropertyBlock mpb = new MaterialPropertyBlock();
        readonly System.Collections.Generic.List<MaterialPropertyBlock> extraBlocks = new System.Collections.Generic.List<MaterialPropertyBlock>();
        class PassData
        {
            internal Material material; internal MaterialPropertyBlock mpb; internal TextureHandle color, depth, motion, prevHistory, prevMotion;
            internal Vector4 inSize, outSize, jitter, prms, prms2;
        }

        static bool Ensure(ref RTHandle h, int w, int hgt, GraphicsFormat f, string name)
        {
            if (h != null && h.rt != null && h.rt.width == w && h.rt.height == hgt && h.rt.graphicsFormat == f) return false;
            h?.Release();
            h = RTHandles.Alloc(w, hgt, f, name: name, filterMode: FilterMode.Bilinear, wrapMode: TextureWrapMode.Clamp);
            return true;
        }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var res = frameData.Get<UniversalResourceData>();
            if (res.isActiveTargetBackBuffer) return;
            var src = res.activeColorTexture; var depth = res.cameraDepthTexture; var motion = res.motionVectorColor;
            if (ReflexTemporalAA.ProfileMode == 1 || ReflexTemporalAA.ProfileMode == 2) { historyValid = false; return; }
            if (!src.IsValid() || !depth.IsValid() || !motion.IsValid())
            {
                if (!warnedMotion) { warnedMotion = true; Debug.LogWarning($"ReflexTemporalAA: no {(depth.IsValid() ? "" : "depth ")}{(motion.IsValid() ? "" : "motion vectors")} this frame: TAA skipped"); }
                historyValid = false;
                return;
            }
            var sd = rg.GetTextureDesc(src);
            int iw = sd.width, ih = sd.height;
            var o = owner.outputSize;
            if (o.x <= iw || o.y <= ih) o = new Vector2Int(iw, ih);   // not upscaling (or URP drew at the full size anyway)
            bool realloc = Ensure(ref history[0], o.x, o.y, GraphicsFormat.R16G16B16A16_SFloat, "Reflex TAA history A");
            realloc |= Ensure(ref history[1], o.x, o.y, GraphicsFormat.R16G16B16A16_SFloat, "Reflex TAA history B");
            bool mvRealloc = Ensure(ref prevMotion, iw, ih, GraphicsFormat.R16G16_SFloat, "Reflex TAA previous motion");
            if (realloc || mvRealloc) historyValid = false;
            bool valid = historyValid && !owner.resetHistory;
            bool pmValid = valid && motionValid && !mvRealloc;
            index ^= 1;
            var prevHist = rg.ImportTexture(history[index ^ 1]);
            var nextHist = rg.ImportTexture(history[index]);
            var pm = rg.ImportTexture(prevMotion);
            var outDesc = new TextureDesc(o.x, o.y) { colorFormat = sd.colorFormat, name = "Reflex TAA output", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, clearBuffer = false };
            var output = rg.CreateTexture(outDesc);

            int draws = ReflexTemporalAA.ProfileMode == 3 ? 5 : 1;   // the cost hunt: 4 more resolves into scratch targets
            for (int rep = 0; rep < draws; rep++)
            using (var builder = rg.AddRasterRenderPass<PassData>("Reflex TAA resolve", out var d, ReflexPassSamplers.Get("Reflex TAA resolve")))
            {
                if (rep >= extraBlocks.Count) extraBlocks.Add(new MaterialPropertyBlock());
                d.material = material; d.mpb = rep == 0 ? mpb : extraBlocks[rep]; d.color = src; d.depth = depth; d.motion = motion; d.prevHistory = prevHist; d.prevMotion = pm;
                d.inSize = new Vector4(iw, ih, 1f / iw, 1f / ih);
                d.outSize = new Vector4(o.x, o.y, 1f / o.x, 1f / o.y);
                d.jitter = new Vector4(owner.jitterUv.x, owner.jitterUv.y, owner.jitterDeltaUv.x, owner.jitterDeltaUv.y);
                bool up = o.x != iw;
                // x base blend of the current frame, y history valid, z the reconstruction kernel's sharpness (exp(-z d^2), d in input pixels),
                // w last frame's motion vectors usable for the disocclusion test
                d.prms = new Vector4(up ? 0.085f : 0.1f, valid ? 1f : 0f, up ? 2.0f : 3.0f, pmValid ? 1f : 0f);
                // xy last frame's jitter delta (in its motion vectors), z variance clip gamma when still, w when moving fast
                d.prms2 = new Vector4(owner.prevJitterDeltaUv.x, owner.prevJitterDeltaUv.y, 1.25f, 0.9f);
                builder.UseTexture(src); builder.UseTexture(depth); builder.UseTexture(motion); builder.UseTexture(prevHist); builder.UseTexture(pm);
                builder.SetRenderAttachment(rep == 0 ? nextHist : rg.CreateTexture(new TextureDesc(o.x, o.y) { colorFormat = GraphicsFormat.R16G16B16A16_SFloat, name = "scratch" }), 0, AccessFlags.Write);
                builder.SetRenderAttachment(rep == 0 ? output : rg.CreateTexture(outDesc), 1, AccessFlags.Write);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var m = data.mpb; m.Clear();
                    m.SetTexture(ColorId, data.color); m.SetTexture(DepthId, data.depth); m.SetTexture(MotionId, data.motion);
                    m.SetTexture(HistoryId, data.prevHistory); m.SetTexture(PrevMotionId, data.prevMotion);
                    m.SetVector(InSizeId, data.inSize); m.SetVector(OutSizeId, data.outSize); m.SetVector(JitterId, data.jitter);
                    m.SetVector(ParamsId, data.prms); m.SetVector(Params2Id, data.prms2);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, PassResolve, MeshTopology.Triangles, 3, 1, m);
                });
            }
            // this frame's motion vectors for the next frame's disocclusion test
            if (ReflexTemporalAA.ProfileMode != 5) rg.AddBlitPass(motion, pm, Vector2.one, Vector2.zero, passName: "Reflex TAA motion copy");
            historyValid = true; motionValid = true;
            res.cameraColor = output;
        }
    }

    /// Contrast-adaptive sharpening (AMD FidelityFX CAS) of the tonemapped image, after ReflexPostPass's chain.
    sealed class ReflexSharpenPass : ScriptableRenderPass
    {
        readonly ReflexTemporalAA owner;
        readonly Material material;
        static readonly int ColorId = Shader.PropertyToID("_TaaColor"), InSizeId = Shader.PropertyToID("_TaaInSize"), SharpId = Shader.PropertyToID("_TaaSharpen");
        const int PassSharpen = 1;

        public ReflexSharpenPass(ReflexTemporalAA owner, Material material)
        {
            this.owner = owner; this.material = material;
            renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 1;   // after ReflexPostPass's tonemap (550)
            profilingSampler = new ProfilingSampler("Reflex TAA sharpen");
            requiresIntermediateTexture = true;
        }

        readonly MaterialPropertyBlock mpb = new MaterialPropertyBlock();
        class PassData { internal Material material; internal MaterialPropertyBlock mpb; internal TextureHandle src; internal Vector4 size, sharpen; }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var res = frameData.Get<UniversalResourceData>();
            if (res.isActiveTargetBackBuffer) return;
            var src = res.activeColorTexture;
            if (!src.IsValid()) return;
            var sd = rg.GetTextureDesc(src);
            var desc = new TextureDesc(sd.width, sd.height) { colorFormat = sd.colorFormat, name = "Reflex sharpened", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, clearBuffer = false };
            var dst = rg.CreateTexture(desc);
            using (var builder = rg.AddRasterRenderPass<PassData>("Reflex TAA sharpen", out var d, ReflexPassSamplers.Get("Reflex TAA sharpen")))
            {
                d.material = material; d.mpb = mpb; d.src = src;
                d.size = new Vector4(sd.width, sd.height, 1f / sd.width, 1f / sd.height);
                d.sharpen = new Vector4(owner.sharpness, 0, 0, 0);
                builder.UseTexture(src);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var m = data.mpb; m.Clear();
                    m.SetTexture(ColorId, data.src); m.SetVector(InSizeId, data.size); m.SetVector(SharpId, data.sharpen);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, PassSharpen, MeshTopology.Triangles, 3, 1, m);
                });
            }
            res.cameraColor = dst;
        }
    }
}
