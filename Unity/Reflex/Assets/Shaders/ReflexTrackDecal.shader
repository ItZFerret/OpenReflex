// Port of the original tyre-track decal pass (HFT_Track_Default: vs_hft_tracks / ps_hft_tracks,
// _re/notes/rt-bump.md section 2.2) drawing the game's own ribbon triangles (reflex_world_tracks)
// into a world-space track multiplier texture instead of the RT_Bump cascades. Multiply blend
// (SRCBLEND=DESTCOLOR, DESTBLEND=ZERO), colour write RGB, no depth, no culling, as 007cc2a0 sets.
//   tex = lerp(s0(uv), s1(uv), useVehicleTexture)      uv = (side 0/1, 32*|w|), U clamp, V wrap
//   a   = pow(softness.r, 0.25) * I
//   z   = 1 + softness.g * I * (tex.b - 1)
//   out = lerp(float4(1, 1, z, 1), tex, a)
Shader "Reflex/Original HFT Tracks"
{
    Properties
    {
        [NoScaleOffset] _TrackGeneric("s0 GenericTrack (slip / generic)", 2D) = "white" {}
        [NoScaleOffset] _TrackMX("MXTrack (slot type 1)", 2D) = "white" {}
        [NoScaleOffset] _TrackATV("ATVTrack (slot type 2)", 2D) = "white" {}
        [NoScaleOffset] _TrackDefault("UTV / ProLite / Buggy (types 3-5)", 2D) = "white" {}
        [NoScaleOffset] _SoftnessMask("s2 SoftnessMask (terrain, v up = +z)", 2D) = "white" {}
        _TrackWorld("Terrain centre x, z / half size x, z (the ribbon vertices are world positions)", Vector) = (0, 0, 1, 1)
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Name "TrackDecal"
            Blend DstColor Zero
            ColorMask RGB
            ZTest Always ZWrite Off Cull Off
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            TEXTURE2D(_TrackGeneric); SAMPLER(sampler_TrackGeneric);
            TEXTURE2D(_TrackMX); SAMPLER(sampler_TrackMX);
            TEXTURE2D(_TrackATV); SAMPLER(sampler_TrackATV);
            TEXTURE2D(_TrackDefault); SAMPLER(sampler_TrackDefault);
            TEXTURE2D(_SoftnessMask); SAMPLER(sampler_SoftnessMask);

            struct Attributes { float3 position : POSITION; float2 uv : TEXCOORD0; float2 attr : TEXCOORD1; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float2 attr : TEXCOORD1; float2 maskUV : TEXCOORD2; };

            float4 _TrackWorld;   // xy: terrain centre (x, z), zw: half size (x, z)

            // position.xy: the world position (x, z) of the core's vertex; normalized here to the terrain's [-1, 1] (x, z), the original
            // SHORT4N value ((x - centre) / half size, formerly computed per vertex on the CPU).
            Varyings Vert(Attributes v)
            {
                Varyings o;
                float2 n = (v.position.xy - _TrackWorld.xy) / _TrackWorld.zw;
                o.positionCS = mul(UNITY_MATRIX_VP, float4(n, 0.5, 1));
                o.uv = v.uv; o.attr = v.attr;
                o.maskUV = n * 0.5 + 0.5;
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float intensity = i.attr.x, type = i.attr.y;
                float2 uv = float2(saturate(i.uv.x), i.uv.y);   // U clamp, V wrap (sampler repeat)
                float4 tex = SAMPLE_TEXTURE2D(_TrackGeneric, sampler_TrackGeneric, uv);
                if (type > 0.5)
                {
                    if (type < 1.5) tex = SAMPLE_TEXTURE2D(_TrackMX, sampler_TrackMX, uv);
                    else if (type < 2.5) tex = SAMPLE_TEXTURE2D(_TrackATV, sampler_TrackATV, uv);
                    else tex = SAMPLE_TEXTURE2D(_TrackDefault, sampler_TrackDefault, uv);
                }
                float4 soft = SAMPLE_TEXTURE2D(_SoftnessMask, sampler_SoftnessMask, i.maskUV);
                float a = pow(max(soft.r, 0), 0.25) * intensity;
                float z = 1 + soft.g * intensity * (tex.b - 1);
                return lerp(float4(1, 1, z, 1), tex, a);
            }
            ENDHLSL
        }
    }
}
