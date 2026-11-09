// MX vs ATV Alive terrain decal layers, drawn top-down into the terrain-space rings of OriginalWorld/AliveTerrainVT.cs (Alive draws the same
// HFDecalModel meshes into its terrain virtual texture; their HFB_* StaticVertexShader projects (x, z) only, gWorldViewProjection c76..79).
// The pixel programs (disassembled from the EngineDependencies / level shader resources with _re/formats/xenos_disasm.py; colour output oC0):
//   0 HFB_4tex: four layers tiled by usr_bump_map_tile_rate (base x, red y, green z, blue w), blended by the mask (s4) in turn:
//     c = lerp(lerp(lerp(base, red, mask.r), green, mask.g), blue, mask.b); the full-terrain base layer: opaque.
//   1 HFB_4tex_texvertalpha: the same layers composited "over" each other by mask x layer alpha (the base by mask.a x base alpha),
//     colour normalized by the coverage, alpha = the coverage.
//   2 HFB_1tex_vertalpha: tex.rgb, alpha = tex.a x vertex alpha (oC0.w = r2.w * r0.z).
//   3 HFB_1tex (and the 1tex_deformed / 2tex families): tex.rgb, alpha = tex.a.
//   4 HFB_4tex_vertalpha: program 0's colour, alpha = vertex alpha.
// Colour values are written raw (the terrain program adjusts and squares them); blended SrcAlpha / InvSrcAlpha in SortBias order.
Shader "Reflex/Alive Terrain Decal"
{
    Properties
    {
        [NoScaleOffset] _Layer0("Base / single texture", 2D) = "gray" {}
        [NoScaleOffset] _Layer1("Red layer", 2D) = "gray" {}
        [NoScaleOffset] _Layer2("Green layer", 2D) = "gray" {}
        [NoScaleOffset] _Layer3("Blue layer", 2D) = "gray" {}
        [NoScaleOffset] _Mask("Mask", 2D) = "black" {}
        _TileRate("usr_bump_map_tile_rate (base, red, green, blue)", Vector) = (1, 1, 1, 1)
        _Program("HFB program (see header)", Float) = 0
        _Opaque("Opaque (the first layer)", Float) = 0
    }
    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" "IgnoreProjector"="True" }
        Pass
        {
            Name "AliveDecal"
            Blend SrcAlpha OneMinusSrcAlpha, One OneMinusSrcAlpha
            ZWrite Off ZTest Always Cull Off

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            TEXTURE2D(_Layer0); SAMPLER(sampler_Layer0);
            TEXTURE2D(_Layer1); SAMPLER(sampler_Layer1);
            TEXTURE2D(_Layer2); SAMPLER(sampler_Layer2);
            TEXTURE2D(_Layer3); SAMPLER(sampler_Layer3);
            TEXTURE2D(_Mask);   SAMPLER(sampler_Mask);
            CBUFFER_START(UnityPerMaterial)
                float4 _TileRate;
                float _Program, _Opaque;
            CBUFFER_END

            struct Attributes { float3 positionOS : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float3 uva : TEXCOORD0; };

            Varyings Vert(Attributes v)
            {
                Varyings o;
                // position.xy = world (x, z); the ring's view-projection (AliveTerrainVT) maps it to the target
                o.positionCS = mul(UNITY_MATRIX_VP, float4(v.positionOS.xy, 0.0, 1.0));
                o.uva = float3(v.uv, v.color.a);
                return o;
            }

            float4 L0(float2 uv) { return SAMPLE_TEXTURE2D(_Layer0, sampler_Layer0, uv); }

            half4 Frag(Varyings i) : SV_Target
            {
                float2 uv = i.uva.xy; float va = i.uva.z;
                if (_Program > 1.5 && _Program < 3.5)
                {
                    float4 t = L0(uv);
                    return half4(t.rgb, saturate(t.a * (_Program < 2.5 ? va : 1.0)));
                }
                float4 m = SAMPLE_TEXTURE2D(_Mask, sampler_Mask, uv);
                float4 b = L0(uv * _TileRate.x);
                float4 r = SAMPLE_TEXTURE2D(_Layer1, sampler_Layer1, uv * _TileRate.y);
                float4 g = SAMPLE_TEXTURE2D(_Layer2, sampler_Layer2, uv * _TileRate.z);
                float4 bl = SAMPLE_TEXTURE2D(_Layer3, sampler_Layer3, uv * _TileRate.w);
                if (_Program > 0.5 && _Program < 1.5)
                {
                    // texvertalpha: each layer over the previous with weight mask x layer alpha (the base: mask.a x base alpha)
                    float wb = saturate(m.a * b.a), wr = saturate(m.r * r.a), wg = saturate(m.g * g.a), wl = saturate(m.b * bl.a);
                    float3 c = b.rgb * wb; float a = wb;
                    c = c * (1.0 - wr) + r.rgb * wr; a = a * (1.0 - wr) + wr;
                    c = c * (1.0 - wg) + g.rgb * wg; a = a * (1.0 - wg) + wg;
                    c = c * (1.0 - wl) + bl.rgb * wl; a = a * (1.0 - wl) + wl;
                    return half4(c / max(a, 1e-4), saturate(a * va));
                }
                float3 c4 = lerp(lerp(lerp(b.rgb, r.rgb, m.r), g.rgb, m.g), bl.rgb, m.b);
                float alpha = _Program > 3.5 ? va : 1.0;
                return half4(c4, _Opaque > 0.5 ? 1.0 : alpha);
            }
            ENDHLSL
        }
    }
}
