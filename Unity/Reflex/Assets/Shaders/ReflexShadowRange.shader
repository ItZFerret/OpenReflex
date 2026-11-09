// The depth range of the main-light shadow atlas per 8 x 8 texel block (Scripts/Rendering/ReflexShadowRange.cs), for the receivers'
// early out (ReflexShadows.hlsl RfxMainLightShadow). Not part of the original renderer: it changes no pixel, only how often the full
// soft filter runs.
//   pass 0  block      min / max of the raw depth of the block's 8 x 8 atlas texels (clamped at the atlas edge)
//   pass 1  dilate     min / max over the 3 x 3 blocks around: every texel a receiver's filter can touch (the 7 x 7 tent's 16
//                      bilinear taps reach at most 5 texels from the shadow coordinate) lies in the 3 x 3 blocks of its own block
Shader "Hidden/Reflex/ShadowRange"
{
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        TEXTURE2D_FLOAT(_RfxShadowRangeSrc);
        float4 _RfxShadowRangeSrcSize;   // xy: the source's size in texels

        struct V2F { float4 positionCS : SV_POSITION; };
        V2F Vert(uint id : SV_VertexID)
        {
            V2F o;
            float2 uv = float2((id << 1) & 2, id & 2);
            o.positionCS = float4(uv * 2.0 - 1.0, 0.0, 1.0);
            return o;
        }
        ENDHLSL

        Pass
        {
            Name "Block"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            float2 Frag(V2F i) : SV_Target
            {
                int2 base = int2(i.positionCS.xy) * 8;
                int2 last = int2(_RfxShadowRangeSrcSize.xy) - 1;
                float lo = 1e30, hi = -1e30;
                [unroll] for (int y = 0; y < 8; y++)
                    [unroll] for (int x = 0; x < 8; x++)
                    {
                        float d = LOAD_TEXTURE2D(_RfxShadowRangeSrc, min(base + int2(x, y), last)).r;
                        lo = min(lo, d); hi = max(hi, d);
                    }
                return float2(lo, hi);
            }
            ENDHLSL
        }

        Pass
        {
            Name "Dilate"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            float2 Frag(V2F i) : SV_Target
            {
                int2 c = int2(i.positionCS.xy);
                int2 last = int2(_RfxShadowRangeSrcSize.xy) - 1;
                float lo = 1e30, hi = -1e30;
                [unroll] for (int y = -1; y <= 1; y++)
                    [unroll] for (int x = -1; x <= 1; x++)
                    {
                        float2 r = LOAD_TEXTURE2D(_RfxShadowRangeSrc, clamp(c + int2(x, y), int2(0, 0), last)).rg;
                        lo = min(lo, r.x); hi = max(hi, r.y);
                    }
                return float2(lo, hi);
            }
            ENDHLSL
        }
    }
}
