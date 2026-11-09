// The depth range of the main-light shadow atlas per 8 x 8 texel block, for the receivers' early out (Shaders/ReflexShadows.hlsl
// RfxMainLightShadow; both LIGHTING modes). Not part of the original renderer and it changes no pixel: a receiver whose shadow coordinate
// is nearer the light than every atlas texel its soft filter could read is lit, one farther than all of them is shadowed, both without
// the filter (URP's 7 x 7 tent: 16 compares and the weights per pixel). In ORIGINAL only the bikes and riders cast, so nearly every pixel
// takes the early out; in ENHANCED the open ground and the inside of large shadows do.
// Cost: two small passes after the shadow atlas (2048 x 2048: 256 x 256 blocks of 64 loads, then a 3 x 3 dilation).
// REFLEX_SHADOW_RANGE=0 or Enabled = false: off (the receivers run URP's filter everywhere; tests compare the two).
using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    sealed class ReflexShadowRangePass : ScriptableRenderPass, IDisposable
    {
        /// Tests / diagnostics: false sends every receiver through URP's filter (REFLEX_SHADOW_RANGE=0 at start).
        public static bool Enabled = Environment.GetEnvironmentVariable("REFLEX_SHADOW_RANGE") != "0";
        // 4 steps of the 16-bit atlas (URP's main-light shadow map is D16): on the safe side of the compare's precision
        const float Margin = 1f / 16384f;
        const int PassBlock = 0, PassDilate = 1;
        static readonly int SrcId = Shader.PropertyToID("_RfxShadowRangeSrc"), SrcSizeId = Shader.PropertyToID("_RfxShadowRangeSrcSize"),
            RangeId = Shader.PropertyToID("_RfxShadowRange"), ParamsId = Shader.PropertyToID("_RfxShadowRangeParams");
        Material material; bool missing;
        readonly MaterialPropertyBlock blockMpb = new MaterialPropertyBlock(), dilateMpb = new MaterialPropertyBlock();

        public ReflexShadowRangePass()
        {
            renderPassEvent = RenderPassEvent.AfterRenderingShadows;
            profilingSampler = new ProfilingSampler("Reflex shadow range");
        }

        public void Dispose() { if (material) CoreUtils.Destroy(material); material = null; }

        /// The receivers use URP's filter everywhere (other cameras, no atlas this frame).
        internal static void Off() => Shader.SetGlobalVector(ParamsId, Vector4.zero);

        class PassData
        {
            internal Material material; internal MaterialPropertyBlock mpb; internal int pass;
            internal TextureHandle src; internal Vector4 srcSize, parameters;
        }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            if (!Enabled || missing) return;
            var res = frameData.Get<UniversalResourceData>();
            var shadow = res.mainShadowsTexture;
            if (!shadow.IsValid()) return;
            var desc = rg.GetTextureDesc(shadow);
            if (desc.width < 64 || desc.height < 64) return;   // URP's empty 1 x 1 stand-in
            if (!material)
            {
                var shader = Shader.Find("Hidden/Reflex/ShadowRange");
                if (!shader) { missing = true; Debug.LogWarning("ReflexShadowRangePass: Hidden/Reflex/ShadowRange not found (Always Included Shaders)"); return; }
                material = CoreUtils.CreateEngineMaterial(shader);
            }
            int bw = (desc.width + 7) / 8, bh = (desc.height + 7) / 8;
            var block = rg.CreateTexture(new TextureDesc(bw, bh) { colorFormat = GraphicsFormat.R32G32_SFloat, name = "Reflex shadow range blocks", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp });
            var range = rg.CreateTexture(new TextureDesc(bw, bh) { colorFormat = GraphicsFormat.R32G32_SFloat, name = "Reflex shadow range", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp });
            Add(rg, "Reflex shadow range blocks", PassBlock, blockMpb, shadow, new Vector4(desc.width, desc.height), block, default);
            Add(rg, "Reflex shadow range", PassDilate, dilateMpb, block, new Vector4(bw, bh), range, new Vector4(1f, Margin, desc.width / 8f, desc.height / 8f));
        }

        void Add(RenderGraph rg, string name, int pass, MaterialPropertyBlock mpb, TextureHandle src, Vector4 srcSize, TextureHandle dst, Vector4 parameters)
        {
            using (var builder = rg.AddRasterRenderPass<PassData>(name, out var d, ReflexPassSamplers.Get(name)))
            {
                d.material = material; d.mpb = mpb; d.pass = pass; d.src = src; d.srcSize = srcSize; d.parameters = parameters;
                builder.UseTexture(src);
                builder.SetRenderAttachment(dst, 0, AccessFlags.Write);
                bool publish = parameters.x > 0.5f;
                if (publish) { builder.SetGlobalTextureAfterPass(dst, RangeId); builder.AllowGlobalStateModification(true); }
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    data.mpb.Clear();
                    data.mpb.SetTexture(SrcId, data.src);
                    data.mpb.SetVector(SrcSizeId, data.srcSize);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, data.pass, MeshTopology.Triangles, 3, 1, data.mpb);
                    if (data.parameters.x > 0.5f) ctx.cmd.SetGlobalVector(ParamsId, data.parameters);
                });
            }
        }
    }
}
