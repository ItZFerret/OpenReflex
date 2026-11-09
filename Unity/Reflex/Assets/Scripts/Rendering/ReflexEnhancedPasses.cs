// LIGHTING: ENHANCED (OPTIONS > VIDEO) render passes. Not part of the original renderer: the ORIGINAL mode never records any of this
// (ReflexRenderPipeline.enhanced is null there and the original post chain runs exactly as before).
//   ReflexOcclusionPass  after the depth pre-pass: screen-space ambient occlusion + contact shadows towards the sun into
//                        _RfxScreenOcclusion, which the world shaders' _RFX_ENHANCED variants read before they shade
//   ReflexPostPass (enhanced part)  the modern post chain: the original's luminance measurement and per-game-frame adaptation, but
//                        metering the whole frame; a mip-chain bloom; a filmic tonemap and the level's grade (Shaders/ReflexEnhancedPost.shader)
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.RenderGraphModule.Util;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    /// Everything the enhanced passes need for one camera (ReflexEnhancedLighting fills it from the quality tier and the level's profile).
    public sealed class ReflexEnhancedSettings
    {
        // occlusion (0 = off: no depth pre-pass at all)
        public bool occlusion = true;
        public int occlusionDownscale = 1;          // 1 full resolution, 2 half
        public float aoRadius = 1.5f, aoIntensity = 1.5f, aoDistance = 160f; public int aoSamples = 10;
        public bool contact = true;
        public float contactLength = 0.6f, contactThickness = 0.2f, contactDistance = 60f;
        // the world shaders' weights (_RfxEnhParams / _RfxEnhParams2)
        public float skyAmbient = 0.5f, aoOnAmbient = 1f, aoOnColour = 0.55f;
        public float reflection = 0.6f, fresnelF0 = 0.04f; public float skyCubeMips; public bool skyCube;
        // post
        public float bloomIntensity = 0.06f, bloomThreshold = 0.85f, bloomKnee = 0.35f, bloomClamp = 64f;
        public float exposureBias = 1f, shoulder = 0.65f, slope = 1.07f, highlightDesat = 0.45f;
        public float saturation = 1.06f, contrast = 1.04f, vignette = 0.08f, dither = 1f;
        public Vector3 gain = Vector3.one, lift = Vector3.zero;
    }

    /// Screen-space ambient occlusion and contact shadows from the depth pre-pass (requesting the depth before the opaques makes URP
    /// draw one), published as _RfxScreenOcclusion before the opaque objects shade.
    sealed class ReflexOcclusionPass : ScriptableRenderPass, IDisposable
    {
        readonly ReflexRenderPipeline owner;
        Material material;
        readonly List<MaterialPropertyBlock> blocks = new List<MaterialPropertyBlock>();
        int blockIndex;
        static readonly int OcclusionId = Shader.PropertyToID("_RfxScreenOcclusion"), EnhParamsId = Shader.PropertyToID("_RfxEnhParams"),
            EnhParams2Id = Shader.PropertyToID("_RfxEnhParams2");
        static readonly int Src0 = Shader.PropertyToID("_RfxSrc0"), SrcSize = Shader.PropertyToID("_RfxSrcSize"), AO = Shader.PropertyToID("_RfxAO"),
            Contact = Shader.PropertyToID("_RfxContact"), ZParams = Shader.PropertyToID("_RfxZParams"), TopLeft = Shader.PropertyToID("_RfxViewTopLeft"),
            XExt = Shader.PropertyToID("_RfxViewXExtent"), YExt = Shader.PropertyToID("_RfxViewYExtent"), CamPos = Shader.PropertyToID("_RfxCamPos"),
            CamFwd = Shader.PropertyToID("_RfxCamFwd"), SunDir = Shader.PropertyToID("_RfxSunDirAO"), ViewProj = Shader.PropertyToID("_RfxViewProj"),
            BlurDir = Shader.PropertyToID("_RfxBlurDir"), RfxSunDir = Shader.PropertyToID("_RfxSunDir");
        const int PassOcclusion = 6, PassBlur = 7;
        static Vector4 enhParams, enhParams2;

        public ReflexOcclusionPass(ReflexRenderPipeline owner)
        {
            this.owner = owner;
            renderPassEvent = RenderPassEvent.AfterRenderingPrePasses;
            profilingSampler = new ProfilingSampler("Reflex enhanced occlusion");
        }

        public void Dispose() { if (material) CoreUtils.Destroy(material); material = null; }

        /// The game camera starts rendering in ENHANCED: the shaders' weights, and the pass (which asks URP for the depth pre-pass).
        internal static void BeginCamera(ReflexEnhancedSettings s, ReflexOcclusionPass pass, ScriptableRenderer renderer)
        {
            enhParams = new Vector4(s.skyAmbient, s.aoOnAmbient, s.aoOnColour, s.occlusion ? 1f : 0f);
            enhParams2 = new Vector4(s.reflection, s.fresnelF0, s.skyCubeMips, s.skyCube ? 1f : 0f);
            Shader.SetGlobalVector(EnhParamsId, enhParams);
            Shader.SetGlobalVector(EnhParams2Id, enhParams2);
            if (!s.occlusion || pass == null) { Shader.SetGlobalTexture(OcclusionId, Texture2D.whiteTexture); return; }
            pass.ConfigureInput(ScriptableRenderPassInput.Depth);
            renderer.EnqueuePass(pass);
        }

        /// Other cameras (previews, the letterbox) see no occlusion and no sky weights.
        internal static void EndCamera()
        {
            Shader.SetGlobalVector(EnhParamsId, Vector4.zero);
            Shader.SetGlobalVector(EnhParams2Id, new Vector4(enhParams2.x, enhParams2.y, enhParams2.z, 0));
            Shader.SetGlobalTexture(OcclusionId, Texture2D.whiteTexture);
        }

        class PassData
        {
            internal Material material; internal int pass; internal MaterialPropertyBlock mpb; internal TextureHandle src;
            internal Vector4 size, ao, contact, z, tl, xe, ye, cam, fwd, sun, blur; internal Matrix4x4 vp;
        }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var s = owner.enhanced;
            if (s == null || !s.occlusion) return;
            if (!material)
            {
                var shader = Shader.Find("Hidden/Reflex/EnhancedPost");
                if (!shader) { Debug.LogError("ReflexOcclusionPass: Hidden/Reflex/EnhancedPost not found (Always Included Shaders)"); s.occlusion = false; return; }
                material = CoreUtils.CreateEngineMaterial(shader);
            }
            var res = frameData.Get<UniversalResourceData>();
            var camData = frameData.Get<UniversalCameraData>();
            var depth = res.cameraDepthTexture;
            if (!depth.IsValid()) return;
            blockIndex = 0;
            var cd = rg.GetTextureDesc(res.activeColorTexture);
            int ds = Mathf.Max(1, s.occlusionDownscale);
            int w = Mathf.Max(1, cd.width / ds), h = Mathf.Max(1, cd.height / ds);
            var dd = rg.GetTextureDesc(depth);

            // the camera (as URP's SSAO: rays through the near-plane corners in world orientation; GL projection, uv.y 0 = bottom)
            var cam = camData.camera;
            Matrix4x4 view = camData.GetViewMatrix(), proj = camData.GetProjectionMatrix();
            var camView = view; camView.SetColumn(3, new Vector4(0, 0, 0, 1));
            var inv = (proj * camView).inverse;
            Vector3 tl = inv.MultiplyPoint(new Vector3(-1, 1, -1)), tr = inv.MultiplyPoint(new Vector3(1, 1, -1)), bl = inv.MultiplyPoint(new Vector3(-1, -1, -1));
            float n = cam.nearClipPlane, f = cam.farClipPlane;
            Vector4 zp = SystemInfo.usesReversedZBuffer
                ? new Vector4(-1f + f / n, 1f, (-1f + f / n) / f, 1f / f)
                : new Vector4(1f - f / n, f / n, (1f - f / n) / f, f / n / f);
            Vector3 sun = Shader.GetGlobalVector(RfxSunDir);
            var common = new PassData
            {
                ao = new Vector4(s.aoRadius, s.aoIntensity, s.aoDistance, s.aoSamples),
                contact = new Vector4(s.contactLength, s.contactThickness, s.contactDistance, s.contact ? 1f : 0f),
                z = zp, tl = tl, xe = tr - tl, ye = bl - tl,
                cam = new Vector4(cam.transform.position.x, cam.transform.position.y, cam.transform.position.z, n),
                fwd = cam.transform.forward, sun = sun.sqrMagnitude > 0.5f ? sun.normalized : Vector3.up, vp = proj * view,
            };

            var occ = rg.CreateTexture(new TextureDesc(w, h) { colorFormat = GraphicsFormat.R16G16B16A16_SFloat, name = "Reflex occlusion", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp });
            var tmp = rg.CreateTexture(new TextureDesc(w, h) { colorFormat = GraphicsFormat.R16G16B16A16_SFloat, name = "Reflex occlusion blur", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp });
            var outp = rg.CreateTexture(new TextureDesc(w, h) { colorFormat = GraphicsFormat.R16G16B16A16_SFloat, name = "Reflex occlusion final", filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp });
            Add(rg, "Reflex occlusion", PassOcclusion, depth, new Vector2Int(dd.width, dd.height), occ, common, Vector4.zero, false);
            Add(rg, "Reflex occlusion blur H", PassBlur, occ, new Vector2Int(w, h), tmp, common, new Vector4(1f / w, 0, 8f, 0), false);
            Add(rg, "Reflex occlusion blur V", PassBlur, tmp, new Vector2Int(w, h), outp, common, new Vector4(0, 1f / h, 8f, 0), true);
        }

        void Add(RenderGraph rg, string name, int passIndex, TextureHandle src, Vector2Int srcSize, TextureHandle dst, PassData c, Vector4 blur, bool publish)
        {
            using (var builder = rg.AddRasterRenderPass<PassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = material; d.pass = passIndex; d.src = src;
                if (blockIndex >= blocks.Count) blocks.Add(new MaterialPropertyBlock());
                d.mpb = blocks[blockIndex++];
                d.size = new Vector4(srcSize.x, srcSize.y, 1f / srcSize.x, 1f / srcSize.y);
                d.ao = c.ao; d.contact = c.contact; d.z = c.z; d.tl = c.tl; d.xe = c.xe; d.ye = c.ye; d.cam = c.cam; d.fwd = c.fwd; d.sun = c.sun; d.vp = c.vp; d.blur = blur;
                builder.UseTexture(src);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                if (publish) builder.SetGlobalTextureAfterPass(dst, OcclusionId);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var p = data.mpb; p.Clear();
                    p.SetTexture(Src0, data.src);
                    p.SetVector(SrcSize, data.size); p.SetVector(AO, data.ao); p.SetVector(Contact, data.contact); p.SetVector(ZParams, data.z);
                    p.SetVector(TopLeft, data.tl); p.SetVector(XExt, data.xe); p.SetVector(YExt, data.ye); p.SetVector(CamPos, data.cam);
                    p.SetVector(CamFwd, data.fwd); p.SetVector(SunDir, data.sun); p.SetMatrix(ViewProj, data.vp); p.SetVector(BlurDir, data.blur);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, p);
                });
            }
        }
    }

    // The enhanced post chain (the rest of the class: ReflexRenderPipeline.cs).
    sealed partial class ReflexPostPass
    {
        Material enhancedMaterial;
        static readonly int ESrc0 = Shader.PropertyToID("_RfxSrc0"), ESrc1 = Shader.PropertyToID("_RfxSrc1"), ESrc2 = Shader.PropertyToID("_RfxSrc2"),
            ESrcSize = Shader.PropertyToID("_RfxSrcSize"), ETone = Shader.PropertyToID("_RfxTone"), EGrade = Shader.PropertyToID("_RfxGrade"),
            EGain = Shader.PropertyToID("_RfxGradeGain"), ELift = Shader.PropertyToID("_RfxGradeLift"), EBloom = Shader.PropertyToID("_RfxBloom");
        const int EPassAvgDown4 = 0, EPassLumAverage = 1, EPassBloomPre = 2, EPassBloomDown = 3, EPassBloomUp = 4, EPassTone = 5;

        void DisposeEnhanced() { if (enhancedMaterial) CoreUtils.Destroy(enhancedMaterial); enhancedMaterial = null; }

        class EPassData
        {
            internal Material material; internal int pass; internal MaterialPropertyBlock mpb;
            internal TextureHandle s0, s1, s2; internal Vector4 size, tone, grade, gain, lift, bloom;
        }

        void DrawE(RenderGraph rg, string name, int passIndex, TextureHandle dst, TextureHandle s0, Vector2Int s0Size, TextureHandle s1 = default, TextureHandle s2 = default)
        {
            var s = owner.enhanced; var h = owner.hdr;
            using (var builder = rg.AddRasterRenderPass<EPassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = enhancedMaterial; d.pass = passIndex;
                if (blockIndex >= blocks.Count) blocks.Add(new MaterialPropertyBlock());
                d.mpb = blocks[blockIndex++];
                d.s0 = s0; d.s1 = s1; d.s2 = s2;
                if (s0.IsValid()) builder.UseTexture(s0);
                if (s1.IsValid()) builder.UseTexture(s1);
                if (s2.IsValid()) builder.UseTexture(s2);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                if (ReflexEnhancedLighting.DebugView > 0 && passIndex == EPassTone) builder.UseAllGlobalTextures(true);   // diagnostics read the occlusion
                d.size = new Vector4(s0Size.x, s0Size.y, 1f / Mathf.Max(1, s0Size.x), 1f / Mathf.Max(1, s0Size.y));
                d.tone = new Vector4(h.keyValue * h.KeyMultiplier, s.bloomIntensity, s.exposureBias, s.shoulder);
                d.grade = new Vector4(s.saturation, s.contrast, s.vignette, s.dither);
                d.gain = new Vector4(s.gain.x, s.gain.y, s.gain.z, s.highlightDesat);
                d.lift = new Vector4(s.lift.x, s.lift.y, s.lift.z, s.slope);
                d.bloom = new Vector4(s.bloomThreshold, s.bloomKnee, s.bloomClamp, h.keyValue * h.KeyMultiplier);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (EPassData data, RasterGraphContext ctx) =>
                {
                    var p = data.mpb; p.Clear();
                    if (data.s0.IsValid()) p.SetTexture(ESrc0, data.s0);
                    if (data.s1.IsValid()) p.SetTexture(ESrc1, data.s1);
                    if (data.s2.IsValid()) p.SetTexture(ESrc2, data.s2);
                    p.SetVector(ESrcSize, data.size); p.SetVector(ETone, data.tone); p.SetVector(EGrade, data.grade);
                    p.SetVector(EGain, data.gain); p.SetVector(ELift, data.lift); p.SetVector(EBloom, data.bloom);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, p);
                });
            }
        }

        // The original's measurement (copies, ln luminance 64 x 64) and adaptation step (per original frame, same rate and clamp), but the
        // pyramid averages the whole frame; then the mip-chain bloom and the filmic tonemap.
        void RecordEnhanced(RenderGraph rg, UniversalResourceData res, TextureHandle src, int W, int H, GraphicsFormat ldrFormat)
        {
            if (!enhancedMaterial)
            {
                var shader = Shader.Find("Hidden/Reflex/EnhancedPost");
                if (!shader) { Debug.LogError("ReflexRenderPipeline: Hidden/Reflex/EnhancedPost not found (Always Included Shaders)"); owner.enhanced = null; return; }
                enhancedMaterial = CoreUtils.CreateEngineMaterial(shader);
            }
            var h = owner.hdr;
            var screen = new Vector2Int(W, H);
            var hdrFmt = GraphicsFormat.R16G16B16A16_SFloat; var lumFmt = GraphicsFormat.R32_SFloat;
            Vector2Int s2 = new Vector2Int(W / 2, H / 2), s4 = new Vector2Int(W / 4, H / 4), s8 = new Vector2Int(W / 8, H / 8);
            var d2 = Make(rg, s2.x, s2.y, hdrFmt, "Reflex 1/2");
            var d4 = Make(rg, s4.x, s4.y, hdrFmt, "Reflex 1/4");
            var d8 = Make(rg, s8.x, s8.y, hdrFmt, "Reflex 1/8");
            var lum64 = Make(rg, 64, 64, lumFmt, "Reflex log luminance 64");
            var lum16 = Make(rg, 16, 16, lumFmt, "Reflex luminance 16 (average)");
            var lum4 = Make(rg, 4, 4, lumFmt, "Reflex luminance 4 (average)");
            var lum1 = Make(rg, 1, 1, lumFmt, "Reflex luminance 1 (whole frame)");
            Draw(rg, "Reflex 1/2", PassCopy, d2, s2, screen, src, screen);
            Draw(rg, "Reflex 1/4", PassCopy, d4, s4, screen, d2, s2);
            Draw(rg, "Reflex 1/8", PassCopy, d8, s8, screen, d4, s4);
            Draw(rg, "Reflex log luminance", PassLog, lum64, new Vector2Int(64, 64), screen, d8, s8);
            DrawE(rg, "Reflex luminance 16 (enhanced)", EPassAvgDown4, lum16, lum64, new Vector2Int(64, 64));
            DrawE(rg, "Reflex luminance 4 (enhanced)", EPassAvgDown4, lum4, lum16, new Vector2Int(16, 16));
            DrawE(rg, "Reflex luminance 1 (enhanced)", EPassLumAverage, lum1, lum4, new Vector2Int(4, 4));

            float frames = owner.AdaptationFrames(); owner.LastAdaptationFrames = frames;
            float rate = ReflexRenderPipeline.AdaptationRate(h.adaptationRate, frames);
            var one = new Vector2Int(1, 1);
            int iterations = owner.convergeAdaptation ? 400 : 1;
            TextureHandle adapted = default;
            var hA = rg.ImportTexture(adaptA); var hB = rg.ImportTexture(adaptB);
            for (int it = 0; it < iterations; it++)
            {
                flip = !flip;
                var prev = flip ? hB : hA;
                var next = flip ? hA : hB;
                Draw(rg, "Reflex adaptation", PassAdapt, next, one, screen, lum1, one, prev, default, h, owner.convergeAdaptation ? h.adaptationRate : rate, first ? 1f : 0f);
                first = false;
                adapted = next;
            }

            // bloom: W/2 (prefilter) .. W/64, then back up with a tent filter
            const int levels = 6;
            var sizes = new Vector2Int[levels]; var down = new TextureHandle[levels]; var up = new TextureHandle[levels];
            for (int i = 0; i < levels; i++)
            {
                sizes[i] = new Vector2Int(Mathf.Max(1, W >> (i + 1)), Mathf.Max(1, H >> (i + 1)));
                down[i] = Make(rg, sizes[i].x, sizes[i].y, hdrFmt, "Reflex bloom down " + i);
                if (i < levels - 1) up[i] = Make(rg, sizes[i].x, sizes[i].y, hdrFmt, "Reflex bloom up " + i);
            }
            DrawE(rg, "Reflex bloom prefilter", EPassBloomPre, down[0], src, screen, adapted);
            for (int i = 1; i < levels; i++) DrawE(rg, "Reflex bloom down", EPassBloomDown, down[i], down[i - 1], sizes[i - 1]);
            var coarse = down[levels - 1];
            for (int i = levels - 2; i >= 0; i--)
            {
                DrawE(rg, "Reflex bloom up", EPassBloomUp, up[i], coarse, sizes[i + 1], down[i]);
                coarse = up[i];
            }
            var ldr = Make(rg, W, H, ldrFormat, "Reflex tonemapped (enhanced)");
            DrawE(rg, "Reflex tonemap (enhanced)", EPassTone, ldr, src, screen, coarse, adapted);
            res.cameraColor = ldr;   // the tonemapped image is the camera colour from here on (no full-screen copy back into src)
        }
    }
}
