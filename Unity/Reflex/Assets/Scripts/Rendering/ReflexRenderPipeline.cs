// The original renderer's frame structure for a Unity camera (_re/notes/render-pipeline.md, recovered from recorded frames
// of MXReflex.exe's own renderer replayed by a software rasterizer):
//   world -> FP16 HDR target (raw texture values, no sRGB conversion anywhere) -> HDR post chain -> 8-bit image -> UI.
// Attach to the game camera (OriginalLevel adds it). While the component is enabled the world shaders write raw values
// (_RfxRawPipeline = 1 during this camera's rendering, Shaders/ReflexCommon.hlsl), and ReflexPostPass runs the original chain
// (Shaders/ReflexPost.shader) before URP's own post-processing, writing the tonemapped image back as linear values for the
// sRGB back buffer of the linear project. Requirements (checked, warned once): camera HDR on, URP HDR precision 64 bit
// (A16B16G16R16F like the original target), MSAA off, render scale 1, the camera's URP post-processing off.
using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.RenderGraphModule.Util;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    /// HDRSettingsRSData of a level, as the executable feeds the post chain (HDRSettingsEntity::vf5 0040c7b0 copies the entity
    /// values; FUN_007b3950 holds the defaults used when a level has none; FUN_007b5380 passes g_KeyValue = Key Value * 2).
    [Serializable]
    public sealed class ReflexHdrSettings
    {
        public float bloomAttenuation = 0.3f;        // g_bloomAttn (0x00a8139c)
        public float adaptationRate = 0.06f;         // g_fElapsedTime: fraction per rendered game frame (0x00a7fb8c, not scaled by time)
        public float maxAdaptation = 0.8f;           // maxLuminance (0x00a813a4)
        public float minAdaptation = 0.5f;           // minLuminance (0x00a81390)
        public float maxBloomLuminance = 0.3f;       // maxBloomLuminance (0x00a7f780)
        public float minBloomLuminance = 0f;         // minBloomLuminance (0x00a81398)
        public float keyValue = 0.22f;               // g_KeyValue = keyValue * 2 (0x00a7fb88)
        public float wholeSceneBloomAmount = 0.1f;   // wholeSceneBloomAmount (0x00a813a0)
        /// g_OneOverWhitePointSquared of the tonemap: the executable's constant (white point 0.9) unless the level's HDRSettings author a
        /// "White Point" (MX vs ATV Alive levels: 2.2), which the Alive post chain uses as the Reinhard white point.
        public float oneOverWhitePointSquared = OneOverWhitePointSquared;
        public const float OneOverWhitePointSquared = 1.234568f;   // 0x3f9e0653 (white point 0.9)
        public const float LuminanceShift = 1f;
        /// MX vs ATV Alive's post chain (EngineDependencies "bloom" programs) instead of Reflex's: bright pass BrightPassPS
        /// (x = colour x exposure; max(x - Bloom Threshold, 0) / (that + Bloom Distribution)), tonemap CopyBackBufferPS (per channel
        /// (x + x^2 / white^2) / (1 + x) + bloom, then ^(1 / 2.2)); exposure = g_KeyValue / adapted luminance (SampleLumFinalPS; g_KeyValue = Key Value x 2 as in Reflex).
        public bool alive;
        /// g_KeyValue = Key Value x KeyMultiplier: 2 in Reflex's executable (FUN_007b5380); for MX vs ATV Alive levels the executable is encrypted,
        /// so the factor is calibrated against the reference captures (docs: _re/formats/ALIVE_CONVERSION.md "Round 3"): 2 left the open levels
        /// (Southwest, Dunes, Stewart Compound, Northwest, the SX stadiums) 1.3 to 1.9 times brighter than the captures' medians. REFLEX_ALIVE_KEYSCALE overrides it.
        public const float AliveKeyScale = 1.5f;
        static readonly float AliveKeyOverride = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_ALIVE_KEYSCALE"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var k) && k > 0f ? k : AliveKeyScale;
        public float KeyMultiplier => alive ? AliveKeyOverride : 2f;
        public float bloomThreshold = 1f, bloomDistribution = 1f;
        public static readonly float[] BlurWeights = { 0.274068624f, 0.451862752f, 0.274068624f };   // g_avSampleWeights (3-tap, 160 x 90)
    }

    [ExecuteAlways]
    [DisallowMultipleComponent]
    [RequireComponent(typeof(Camera))]
    public sealed class ReflexRenderPipeline : MonoBehaviour
    {
        public ReflexHdrSettings hdr = new ReflexHdrSettings();
        [Tooltip("Run the original HDR post chain (tonemap, bloom, eye adaptation). Off: the raw HDR image is shown clamped.")]
        public bool postProcess = true;
        /// The original frame on screen (OriginalWorldRuntime.PresentedGameFrame: the game's frame number, fractional while the
        /// presentation blends two frames; NaN while no game frame advances). OriginalLevel sets it for the game camera.
        public Func<double> originalFrame;
        [Tooltip("Tests: iterate the adaptation to its steady state every frame (the luminance the original converges to for a static view).")]
        public bool convergeAdaptation;
        /// LIGHTING: ENHANCED (OPTIONS > VIDEO; ReflexEnhancedLighting sets it, null = ORIGINAL): the modern post chain and the
        /// screen-space occlusion pass (Scripts/Rendering/ReflexEnhancedPasses.cs) instead of the original chain. Not part of the original.
        [NonSerialized] public ReflexEnhancedSettings enhanced;
        ReflexOcclusionPass occlusionPass;
        ReflexShadowRangePass shadowRangePass;   // the shadow receivers' early out (both modes; changes no pixel)

        [Header("graphics.cfg (Content/ReflexLaunchSettings, as the game's WinMain reads it)")]
        [Tooltip("MotionBlur of graphics.cfg. The original only runs PostProcessMotionBlurPS (007af510 -> 007a9f70) when this flag, the viewport's " +
                 "motion blur field (+0x64) and the scene's (+0x2cb8) are all set; recorded race / gate frames have it off, so no pass is drawn here.")]
        public bool motionBlur = true;
        [Tooltip("ShadowQuality row of the game's shadow table (near w, h, far w, h): the dynamic shadow map sizes. The Unity main-light shadow " +
                 "map that stands in for the VSM gets the next power of two of the largest (player builds; the editor keeps the URP asset).")]
        public int[] shadowSizes = { 768, 384, 768, 640 };
        [Tooltip("The game's centred 16:9 viewport inside the back buffer (fullscreen modes that are not 16:9). Applied in player builds after " +
                 "ReflexLaunchSettings set the display mode; the rest of the screen is cleared black.")]
        public Rect viewport = new Rect(0, 0, 1, 1);
        Camera letterbox;

        static readonly int RawPipelineId = Shader.PropertyToID("_RfxRawPipeline");
        Camera cam;
        Material material;
        ReflexPostPass pass;
        bool warned;

        /// The adapted luminance target of the last frame (1 x 1 R32F; tests read it back).
        public RTHandle AdaptedLuminance => pass?.Adapted;
        /// Tests: copies of the 1/8 scene, the bright pass, the 4x4 and 1x1 luminance and the bloom of the last frame (null until enabled).
        public bool keepIntermediates;
        public RTHandle[] Intermediates => pass?.debug;

        // HDRPost_Adapt 007b51d0 uploads the HDR Adaptation Rate (00a7fb8c, HDRSettingsEntity +0x94) as g_fElapsedTime unscaled: one
        // step a += (cur - a) * rate per frame the original renders. The original renders once per game frame (Engine::UpdateTiming
        // 0084ea80 floors every frame at 1/60 s), while Unity draws at the display rate or uncapped (Original pacing shows the newest
        // game frame for several Unity frames): a step per Unity frame adapted 2x (120 Hz) to 5x+ (uncapped) faster than the original,
        // so the exposure followed every change of the frame's top-left luminance almost at once. AdaptationRate gives the rate of
        // `frames` original steps (fractional while the presentation blends two frames): 1 - (1 - rate)^frames.
        public static float AdaptationRate(float authoredRate, float frames)
        {
            if (frames <= 0f) return 0f;
            float r = Mathf.Clamp01(authoredRate);
            return Mathf.Approximately(frames, 1f) ? r : 1f - Mathf.Pow(1f - r, frames);
        }

        /// Original frames since the last adaptation step (once per Unity frame): the game frames presented since then; without a
        /// game frame source (front end, garage, pause: the original keeps rendering at its 60 Hz cap) the real time in 1/60 s frames,
        /// at most one per Unity frame (a slower machine renders slower in the original too).
        internal float AdaptationFrames()
        {
            if (Time.frameCount == lastStepUnityFrame) return 0f;   // the camera rendered twice in one Unity frame
            lastStepUnityFrame = Time.frameCount;
            double f = originalFrame != null ? originalFrame() : double.NaN;
            float n;
            if (!double.IsNaN(f) && !double.IsNaN(lastOriginalFrame) && f >= lastOriginalFrame) n = (float)Math.Min(f - lastOriginalFrame, MaxAdaptationFrames);
            else n = Mathf.Clamp(Time.unscaledDeltaTime * 60f, 0f, 1f);
            lastOriginalFrame = f;
            return n;
        }
        double lastOriginalFrame = double.NaN; int lastStepUnityFrame = -1;
        const double MaxAdaptationFrames = 8;
        /// The original frames the last adaptation step covered (tests / probes).
        public float LastAdaptationFrames { get; internal set; }

        void OnEnable()
        {
            cam = GetComponent<Camera>();
            var shader = Shader.Find("Hidden/Reflex/Post");
            if (!shader) { Debug.LogError("ReflexRenderPipeline: shader Hidden/Reflex/Post not found (Always Included Shaders)", this); enabled = false; return; }
            material = CoreUtils.CreateEngineMaterial(shader);
            pass = new ReflexPostPass(this, material) { renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing };
            occlusionPass = new ReflexOcclusionPass(this);
            shadowRangePass = new ReflexShadowRangePass();
            RenderPipelineManager.beginCameraRendering += OnBegin;
            RenderPipelineManager.endCameraRendering += OnEnd;
            ApplyLaunchSettings();
            if (Application.isPlaying && !GetComponent<ReflexTemporalAA>()) gameObject.AddComponent<ReflexTemporalAA>();   // ANTI-ALIASING TAA (ENHANCED only)
            if (Application.isPlaying && !GetComponent<ReflexSpeedBlur>()) gameObject.AddComponent<ReflexSpeedBlur>();     // SPEED BLUR (ENHANCED only)
        }

        /// Re-applies the viewport after OPTIONS > VIDEO > ASPECT RATIO changed (every live pipeline).
        public static void ReapplyViewports()
        {
            foreach (var p in FindObjectsByType<ReflexRenderPipeline>(FindObjectsSortMode.None)) if (p.isActiveAndEnabled) p.ApplyLaunchSettings();
        }

        // graphics.cfg: motion blur flag, shadow map sizes, the 16:9 viewport (ReflexLaunchSettings.Current; the whole screen with FILL SCREEN).
        void ApplyLaunchSettings()
        {
            Content.ReflexLaunchSettings ls;
            try { ls = Content.ReflexLaunchSettings.Current; } catch (Exception e) { Debug.LogWarning("ReflexRenderPipeline: graphics.cfg not read: " + e.Message, this); return; }
            motionBlur = ls.motionBlur;
            shadowSizes = ls.ShadowSizes;
            viewport = ls.ViewportRect;
            if (Application.isEditor || !Application.isPlaying) return;      // project settings / the editor's game view stay as they are
            if (GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp)
                urp.mainLightShadowmapResolution = Mathf.Clamp(Mathf.NextPowerOfTwo(Mathf.Max(Mathf.Max(shadowSizes[0], shadowSizes[1]), Mathf.Max(shadowSizes[2], shadowSizes[3]))), 256, 8192);
            if (Content.ReflexLaunchSettings.Applied && (viewport.width < 0.999f || viewport.height < 0.999f))
            {
                cam.rect = viewport;
                if (!letterbox)
                {
                    // the back buffer outside the viewport: black (the original clears it and draws nothing there)
                    var go = new GameObject("Reflex letterbox") { hideFlags = HideFlags.DontSave };
                    go.transform.SetParent(transform, false);
                    letterbox = go.AddComponent<Camera>();
                    letterbox.depth = cam.depth - 100; letterbox.cullingMask = 0; letterbox.clearFlags = CameraClearFlags.SolidColor;
                    letterbox.backgroundColor = Color.black; letterbox.rect = new Rect(0, 0, 1, 1);
                    var d = letterbox.GetUniversalAdditionalCameraData(); d.renderPostProcessing = false; d.renderShadows = false;
                }
            }
            else if (Content.ReflexLaunchSettings.Applied)
            {
                cam.rect = new Rect(0, 0, 1, 1);   // FILL SCREEN (or a 16:9 screen): the whole back buffer
                if (letterbox) { Destroy(letterbox.gameObject); letterbox = null; }
            }
        }

        void OnDisable()
        {
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            RenderPipelineManager.endCameraRendering -= OnEnd;
            Shader.SetGlobalFloat(RawPipelineId, 0f);
            pass?.Dispose(); pass = null;
            occlusionPass?.Dispose(); occlusionPass = null;
            shadowRangePass?.Dispose(); shadowRangePass = null;
            if (letterbox) { Destroy(letterbox.gameObject); letterbox = null; }
            if (material) CoreUtils.Destroy(material);
        }

        void OnBegin(ScriptableRenderContext context, Camera c)
        {
            // Only the camera's owner may change this state. Another enabled pipeline (for example the garage preview)
            // receives the same event and must not undo the owning pipeline's raw-HDR setup because it subscribed later.
            // OnEnd restores the preview path after the owning camera finishes rendering.
            if (c != cam) return;
            Check();
            // the scene target is cleared to 0x808080 before the world pass (Clear(..., 7, 0xff808080, 1, 0) from 007af749): levels
            // without a sky (supercross stadiums) show it. Unity converts the clear colour to linear: give the sRGB of the raw value.
            c.clearFlags = CameraClearFlags.SolidColor;
            c.backgroundColor = new Color(128f / 255f, 128f / 255f, 128f / 255f, 1f).gamma;
            Shader.SetGlobalFloat(RawPipelineId, 1f);
            var data = c.GetUniversalAdditionalCameraData();
            if (enhanced != null) ReflexOcclusionPass.BeginCamera(enhanced, occlusionPass, data.scriptableRenderer);
            if (enhanced != null) ReflexVolumetricLight.BeginCamera(this, c, data.scriptableRenderer);   // VOLUMETRIC LIGHT (ReflexVolumetricLight.cs)
            ReflexShadowRangePass.Off();   // (on again from the pass, when this frame has a shadow atlas)
            if (ReflexShadowRangePass.Enabled) data.scriptableRenderer.EnqueuePass(shadowRangePass);
            data.scriptableRenderer.EnqueuePass(pass);
            ReflexCinematic.BeginCamera(data.scriptableRenderer, c);   // REPLAY / PHOTO MODE's lens and camera effects, and SPEED BLUR while riding (Rendering/ReflexCinematic.cs, ReflexSpeedBlur.cs)
        }

        void OnEnd(ScriptableRenderContext context, Camera c)
        {
            if (c == cam) { Shader.SetGlobalFloat(RawPipelineId, 0f); ReflexOcclusionPass.EndCamera(); ReflexVolumetricLight.EndCamera(); ReflexShadowRangePass.Off(); }
        }

        void Check()
        {
            if (warned) return;
            warned = true;
            var asset = GraphicsSettings.currentRenderPipeline as UniversalRenderPipelineAsset;
            if (!cam.allowHDR) Debug.LogWarning("ReflexRenderPipeline: camera HDR is off (the original renders into an FP16 target).", this);
            if (asset)
            {
                if (!asset.supportsHDR) Debug.LogWarning("ReflexRenderPipeline: the URP asset has HDR disabled.", this);
                if (asset.hdrColorBufferPrecision != HDRColorBufferPrecision._64Bits) Debug.LogWarning("ReflexRenderPipeline: URP HDR precision is 32 bit (R11G11B10); the original target is A16B16G16R16F (set 64 bit).", this);
                if (asset.msaaSampleCount > 1) Debug.LogWarning("ReflexRenderPipeline: MSAA is on; the original renders without anti-aliasing.", this);
                if (Mathf.Abs(asset.renderScale - 1f) > 1e-3f) Debug.LogWarning("ReflexRenderPipeline: render scale is not 1.", this);
            }
            var data = cam.GetUniversalAdditionalCameraData();
            if (data && data.renderPostProcessing) Debug.LogWarning("ReflexRenderPipeline: URP post-processing is on for this camera (it would grade the tonemapped image again).", this);
        }
    }

    /// One ProfilingSampler per pass name (the profiler's CPU / GPU view and tests/perf_profile's per-pass GPU timings show every pass of
    /// the post chain and the enhanced passes separately instead of one sampler for the whole chain). Made once per name, no per-frame cost.
    static class ReflexPassSamplers
    {
        static readonly System.Collections.Generic.Dictionary<string, ProfilingSampler> samplers = new System.Collections.Generic.Dictionary<string, ProfilingSampler>();
        internal static ProfilingSampler Get(string name)
        {
            if (!samplers.TryGetValue(name, out var s)) samplers[name] = s = new ProfilingSampler(name);
            return s;
        }
    }

    /// The post chain as one render-graph recording (draw for draw the original's FUN_007b4460 sequence):
    /// 1/2, 1/4, 1/8 copies; bright pass; log luminance 64x64; copies 16x16, 4x4; 1x1 average; adaptation (persistent, ping-pong);
    /// 3 x (horizontal, vertical) blur of the bright pass; tonemap into the camera colour.
    sealed partial class ReflexPostPass : ScriptableRenderPass, IDisposable
    {
        readonly ReflexRenderPipeline owner;
        readonly Material material;
        RTHandle adaptA, adaptB;
        bool first = true, flip;
        internal RTHandle Adapted => flip ? adaptA : adaptB;
        internal RTHandle[] debug;
        readonly System.Collections.Generic.List<MaterialPropertyBlock> blocks = new System.Collections.Generic.List<MaterialPropertyBlock>();
        int blockIndex;

        static readonly int Src0 = Shader.PropertyToID("_RfxSrc0"), Src1 = Shader.PropertyToID("_RfxSrc1"), Src2 = Shader.PropertyToID("_RfxSrc2");
        static readonly int TargetSize = Shader.PropertyToID("_RfxTargetSize"), ScreenSize = Shader.PropertyToID("_RfxScreenSize"), SrcSize = Shader.PropertyToID("_RfxSrcSize");
        static readonly int Bright = Shader.PropertyToID("_RfxBright"), Adapt = Shader.PropertyToID("_RfxAdapt"), Tone = Shader.PropertyToID("_RfxTone"), Blur = Shader.PropertyToID("_RfxBlurWeights");
        const int PassCopy = 0, PassBright = 1, PassLog = 2, PassLumFinal = 3, PassAdapt = 4, PassBlurH = 5, PassBlurV = 6, PassTone = 7;

        public ReflexPostPass(ReflexRenderPipeline owner, Material material)
        {
            this.owner = owner; this.material = material;
            profilingSampler = new ProfilingSampler("Reflex HDR post");
            requiresIntermediateTexture = true;
        }

        public void Dispose() { adaptA?.Release(); adaptB?.Release(); adaptA = adaptB = null; DisposeEnhanced(); }

        class PassData
        {
            internal Material material; internal int pass; internal MaterialPropertyBlock mpb;
            internal TextureHandle s0, s1, s2;
            internal Vector4 target, screen, src, bright, adapt, tone, blur;
        }

        TextureHandle Make(RenderGraph rg, int w, int h, GraphicsFormat f, string name)
        {
            var d = new TextureDesc(Mathf.Max(1, w), Mathf.Max(1, h)) { colorFormat = f, name = name, filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, clearBuffer = false };
            return rg.CreateTexture(d);
        }

        void Draw(RenderGraph rg, string name, int passIndex, TextureHandle dst, Vector2Int dstSize, Vector2Int screen, TextureHandle s0, Vector2Int srcSize,
                  TextureHandle s1 = default, TextureHandle s2 = default, ReflexHdrSettings h = null, float rate = 0, float firstTime = 0)
        {
            using (var builder = rg.AddRasterRenderPass<PassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = material; d.pass = passIndex;
                if (blockIndex >= blocks.Count) blocks.Add(new MaterialPropertyBlock());
                d.mpb = blocks[blockIndex++];
                d.s0 = s0; d.s1 = s1; d.s2 = s2;
                if (s0.IsValid()) builder.UseTexture(s0);
                if (s1.IsValid()) builder.UseTexture(s1);
                if (s2.IsValid()) builder.UseTexture(s2);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                d.target = new Vector4(dstSize.x, dstSize.y, 1f / dstSize.x, 1f / dstSize.y);
                d.screen = new Vector4(screen.x, screen.y, 0, 0);
                d.src = new Vector4(srcSize.x, srcSize.y, 0, 0);
                h = h ?? owner.hdr;
                d.bright = h.alive ? new Vector4(h.bloomThreshold, h.bloomDistribution, 0, 1)
                                   : new Vector4(h.minBloomLuminance, h.maxBloomLuminance, h.wholeSceneBloomAmount, 0);
                d.adapt = new Vector4(h.minAdaptation, h.maxAdaptation, rate, firstTime);
                d.tone = new Vector4(h.keyValue * h.KeyMultiplier, h.bloomAttenuation, h.oneOverWhitePointSquared, ReflexHdrSettings.LuminanceShift);
                d.blur = new Vector4(ReflexHdrSettings.BlurWeights[0], ReflexHdrSettings.BlurWeights[1], ReflexHdrSettings.BlurWeights[2], 0);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var p = data.mpb; p.Clear();
                    if (data.s0.IsValid()) p.SetTexture(Src0, data.s0);
                    if (data.s1.IsValid()) p.SetTexture(Src1, data.s1);
                    if (data.s2.IsValid()) p.SetTexture(Src2, data.s2);
                    p.SetVector(TargetSize, data.target); p.SetVector(ScreenSize, data.screen); p.SetVector(SrcSize, data.src);
                    p.SetVector(Bright, data.bright); p.SetVector(Adapt, data.adapt); p.SetVector(Tone, data.tone); p.SetVector(Blur, data.blur);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, p);
                });
            }
        }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var res = frameData.Get<UniversalResourceData>();
            if (res.isActiveTargetBackBuffer) return;
            blockIndex = 0;
            var src = res.activeColorTexture;
            var desc = rg.GetTextureDesc(src);
            int W = desc.width, H = desc.height;
            var screen = new Vector2Int(W, H);
            var hdrFmt = GraphicsFormat.R16G16B16A16_SFloat;
            var lumFmt = GraphicsFormat.R32_SFloat;

            if (adaptA == null)
            {
                adaptA = RTHandles.Alloc(1, 1, lumFmt, name: "Reflex adapted luminance A", filterMode: FilterMode.Point);
                adaptB = RTHandles.Alloc(1, 1, lumFmt, name: "Reflex adapted luminance B", filterMode: FilterMode.Point);
                first = true;
            }

            var h = owner.hdr;
            if (!owner.postProcess) return;
            src = ReflexCinematic.RecordHdr(rg, frameData, res, src, W, H);   // PHOTO MODE accumulation, motion blur, depth of field (HDR; the src itself while off)
            if (owner.enhanced != null) { RecordEnhanced(rg, res, src, W, H, desc.colorFormat); return; }   // LIGHTING: ENHANCED

            Vector2Int s2 = new Vector2Int(W / 2, H / 2), s4 = new Vector2Int(W / 4, H / 4), s8 = new Vector2Int(W / 8, H / 8);
            var d2 = Make(rg, s2.x, s2.y, hdrFmt, "Reflex 1/2");
            var d4 = Make(rg, s4.x, s4.y, hdrFmt, "Reflex 1/4");
            var d8 = Make(rg, s8.x, s8.y, hdrFmt, "Reflex 1/8");
            var bright = Make(rg, s8.x, s8.y, hdrFmt, "Reflex bright pass");
            var lum64 = Make(rg, 64, 64, lumFmt, "Reflex log luminance 64");
            var lum16 = Make(rg, 16, 16, lumFmt, "Reflex luminance 16");
            var lum4 = Make(rg, 4, 4, lumFmt, "Reflex luminance 4");
            var lum1 = Make(rg, 1, 1, lumFmt, "Reflex luminance 1");
            var blurA = Make(rg, s8.x, s8.y, hdrFmt, "Reflex bloom A");
            var blurB = Make(rg, s8.x, s8.y, hdrFmt, "Reflex bloom B");
            var ldr = Make(rg, W, H, desc.colorFormat, "Reflex tonemapped");

            Draw(rg, "Reflex 1/2", PassCopy, d2, s2, screen, src, screen);
            Draw(rg, "Reflex 1/4", PassCopy, d4, s4, screen, d2, s2);
            Draw(rg, "Reflex 1/8", PassCopy, d8, s8, screen, d4, s4);
            if (!h.alive) Draw(rg, "Reflex bright pass", PassBright, bright, s8, screen, d8, s8);
            Draw(rg, "Reflex log luminance", PassLog, lum64, new Vector2Int(64, 64), screen, d8, s8);
            Draw(rg, "Reflex luminance 16", PassCopy, lum16, new Vector2Int(16, 16), screen, lum64, new Vector2Int(64, 64));
            Draw(rg, "Reflex luminance 4", PassCopy, lum4, new Vector2Int(4, 4), screen, lum16, new Vector2Int(16, 16));
            Draw(rg, "Reflex luminance 1", PassLumFinal, lum1, new Vector2Int(1, 1), screen, lum4, new Vector2Int(4, 4));

            // adaptation: previous = the target of the last frame (ping-pong, FUN_007b51d0 swaps +0xc / +0x10 each frame); the step covers
            // the original frames since the last one (0: the same game frame is drawn again, the exposure stays)
            float frames = owner.AdaptationFrames(); owner.LastAdaptationFrames = frames;
            float rate = ReflexRenderPipeline.AdaptationRate(h.adaptationRate, frames);
            var one = new Vector2Int(1, 1);
            int iterations = owner.convergeAdaptation ? 400 : 1;
            TextureHandle adapted = default;
            var hA = rg.ImportTexture(adaptA); var hB = rg.ImportTexture(adaptB);   // imported once: the graph orders the ping-pong
            for (int it = 0; it < iterations; it++)
            {
                flip = !flip;
                var prev = flip ? hB : hA;
                var next = flip ? hA : hB;
                Draw(rg, "Reflex adaptation", PassAdapt, next, one, screen, lum1, one, prev, default, h, owner.convergeAdaptation ? h.adaptationRate : rate, first ? 1f : 0f);
                first = false;
                adapted = next;
            }

            // Alive's bright pass scales by the exposure, so it follows the adaptation
            if (h.alive) Draw(rg, "Alive bright pass", PassBright, bright, s8, screen, d8, s8, default, adapted);
            Draw(rg, "Reflex bloom H", PassBlurH, blurA, s8, screen, bright, s8);
            Draw(rg, "Reflex bloom V", PassBlurV, blurB, s8, screen, blurA, s8);
            Draw(rg, "Reflex bloom H", PassBlurH, blurA, s8, screen, blurB, s8);
            Draw(rg, "Reflex bloom V", PassBlurV, blurB, s8, screen, blurA, s8);
            Draw(rg, "Reflex bloom H", PassBlurH, blurA, s8, screen, blurB, s8);
            Draw(rg, "Reflex bloom V", PassBlurV, blurB, s8, screen, blurA, s8);

            if (owner.keepIntermediates)
            {
                var srcs = new[] { (d8, s8), (bright, s8), (lum4, new Vector2Int(4, 4)), (lum1, one), (blurB, s8) };
                if (debug == null)
                {
                    debug = new RTHandle[srcs.Length];
                    for (int i = 0; i < srcs.Length; i++) debug[i] = RTHandles.Alloc(srcs[i].Item2.x, srcs[i].Item2.y, GraphicsFormat.R32G32B32A32_SFloat, name: "Reflex debug " + i);
                }
                for (int i = 0; i < srcs.Length; i++) rg.AddBlitPass(srcs[i].Item1, rg.ImportTexture(debug[i]), Vector2.one, Vector2.zero, passName: "Reflex debug copy");
            }
            Draw(rg, "Reflex tonemap", PassTone, ldr, screen, screen, src, screen, blurB, adapted);
            res.cameraColor = ldr;   // the tonemapped image is the camera colour from here on (no full-screen copy back into src)
        }
    }
}
