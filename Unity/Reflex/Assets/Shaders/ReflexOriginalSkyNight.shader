// Port of the original night sky programs (Template_Luma_Sky_Nighttime: SX_Manchester_A_New_Sky, the FunFactory, Dover, Springfield B,
// Fort Dodge, Kingston B and UI_Compound skies; WP-8, _re/notes/props.md):
//   vs (StaticVertexShader 0110..01ec): world = M3x3 * position + cameraPosition (gWorld rows with gCameraPos as the translation column),
//      clip = VP * world with clip.z = clip.w (o1 = r1.xyzz: the far plane); o0 = uv0.
//   ps (SharedPixelShader 02a8..0358), uv = the D3D texture coordinate:
//      starUV = uv.x * usr_star_mask_transform_r1.xy + uv.y * usr_star_mask_transform_r2.xy
//      m      = star_mask(starUV).rgb * blend(uv).rgb
//      sky    = sky(uv).rgb * usr_self_bracketing_value.w - sky(uv).rgb
//      base   = lerp(sky, usr_blend_color.rgb, blend(uv).rgb)            (lrp_pp r3, r1, c88, r2)
//      colour = lerp(base, usr_star_color.rgb, m), alpha 1
// Samplers s0 usr_sampler_star_mask, s1 usr_sampler_blend_texture, s2 usr_sampler_sky_texture (the material's three texture slots in
// order). Render states as the daytime sky (Shaders/ReflexOriginalSky.shader). Unity UVs are (u, 1 - v) and textures are uploaded
// bottom row first, so a D3D coordinate (s, t) is sampled at (s, 1 - t).
Shader "Reflex/Original Sky Night"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Sky texture (s2)", 2D) = "black" {}
        [NoScaleOffset] _BlendMap("Blend texture (s1)", 2D) = "black" {}
        [NoScaleOffset] _StarMask("Star mask (s0)", 2D) = "black" {}
        _Bracket("usr_self_bracketing_value.w", Float) = 2.1
        _BlendColor("usr_blend_color", Vector) = (0.043137, 0.05098, 0.082353, 1)
        _StarColor("usr_star_color", Vector) = (1, 1, 1, 1)
        _StarR1("usr_star_mask_transform_r1", Vector) = (4, 0, 0, 0)
        _StarR2("usr_star_mask_transform_r2", Vector) = (0, 3, 0, 0)
        [Enum(UnityEngine.Rendering.CompareFunction)] _SkyZTest("ZTest (GreaterEqual with a reversed depth buffer)", Float) = 4
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Background" "IgnoreProjector"="True" }
        Pass
        {
            Name "Sky"
            Tags { "LightMode"="UniversalForward" }
            Cull [_Cull]
            ZWrite Off
            ZTest [_SkyZTest]

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "ReflexCommon.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float _Bracket;
                float4 _BlendColor, _StarColor, _StarR1, _StarR2;
            CBUFFER_END
            TEXTURE2D(_BaseMap); SAMPLER(sampler_BaseMap);
            TEXTURE2D(_BlendMap); SAMPLER(sampler_BlendMap);
            TEXTURE2D(_StarMask); SAMPLER(sampler_StarMask);

            struct Attributes { float4 positionOS : POSITION; float2 uv : TEXCOORD0; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };

            Varyings Vert(Attributes v)
            {
                Varyings o;
                float3 world = mul((float3x3)GetObjectToWorldMatrix(), v.positionOS.xyz) + _WorldSpaceCameraPos;
                float4 clip = TransformWorldToHClip(world);
            #if UNITY_REVERSED_Z
                clip.z = 0.0;           // far plane
            #else
                clip.z = clip.w;
            #endif
                o.positionCS = clip; o.uv = v.uv;
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float2 d3d = float2(i.uv.x, 1.0 - i.uv.y);                        // the original's texture coordinate
                float2 star = d3d.y * _StarR2.xy + d3d.x * _StarR1.xy;            // mul r0.xy, c87, v0.y; mad r0.xy, v0.x, c86, r0
                float3 s0 = SAMPLE_TEXTURE2D(_StarMask, sampler_StarMask, float2(star.x, 1.0 - star.y)).rgb;
                float3 s1 = SAMPLE_TEXTURE2D(_BlendMap, sampler_BlendMap, i.uv).rgb;
                float3 s2 = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv).rgb;
                float3 m = s0 * s1;
                float3 sky = s2 * _Bracket - s2;
                float3 base = lerp(sky, _BlendColor.rgb, s1);
                float3 c = m * (_StarColor.rgb - base) + base;
                return half4(RfxOutput(c), 1.0);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
