// Port of the original sky pixel/vertex programs (Template_Luma_Sky_Daytime, materials FR_AL_Sky / NAT_Concord_A_New_SKY):
//   vs: world = M3x3 * position + cameraPosition (the dome follows the camera), clip = VP * world with clip.z = clip.w (far plane)
//   ps: colour = texture(uv) * (usr_self_bracketing_value.w - 1)           (HDR self-bracketing exposure of the sky texture)
// Render states of the material: ZWRITE on, ZTEST less-equal, cull CCW, no blend; the depth at the far plane means only pixels
// nothing else covered are drawn. The original draws the sky after the opaque scenery (recorded frames: draw ~1011 of ~1180)
// with ZWRITE on and stencil REPLACE 0; the result is a raw value for the HDR target (RfxOutput, ReflexCommon.hlsl).
// _SKY_ENTITY_COMP: SkyEntityComp (the DLC levels NAT_DLC05_Dalton(B), SX_DLC01/02_FreightYard(B); same vertex program), samplers
//   s0 usr_sampler_compositergb_image_hdrluminance (_BaseMap), s1 usr_sampler_compositergb_image_hdrsaturation (_SatMap):
//   colour = (lum.x * lumScale + lumBias) * (sat.xyz * satScale + satBias) (0304..034c).
//   Literal constants differ in each DLC package despite the shared shader asset name; the resource parser supplies them.
Shader "Reflex/Original Sky"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Sky texture", 2D) = "white" {}
        _Bracket("usr_self_bracketing_value", Float) = 1.7
        [NoScaleOffset] _SatMap("SkyEntityComp saturation texture (s1)", 2D) = "black" {}
        _SkySaturationScale("SkyEntityComp literal saturation scale", Vector) = (0, 0, 0, 0)
        _SkySaturationBias("SkyEntityComp literal saturation bias", Vector) = (0, 0, 0, 0)
        _SkyLuminance("SkyEntityComp literal luminance scale, bias", Vector) = (0, 0, 0, 0)
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
            #pragma multi_compile_local_fragment _ _SKY_ENTITY_COMP
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "ReflexCommon.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float _Bracket;
                float4 _SkySaturationScale, _SkySaturationBias, _SkyLuminance;
            CBUFFER_END
            TEXTURE2D(_BaseMap); SAMPLER(sampler_BaseMap);
            TEXTURE2D(_SatMap); SAMPLER(sampler_SatMap);

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
            #if defined(_SKY_ENTITY_COMP)
                float3 sat = SAMPLE_TEXTURE2D(_SatMap, sampler_SatMap, i.uv).rgb;
                float lum = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv).r;
                float3 c = RfxSkyGamma((lum * _SkyLuminance.x + _SkyLuminance.y) * (sat * _SkySaturationScale.xyz + _SkySaturationBias.xyz));   // (1 but on an Alive level in the REFLEX look)
            #else
                // mad_pp oC0.xyz, r0, c85.w, -r0 (= tex * w - tex), alpha 1
                float3 t = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv).rgb;
                float3 c = t * _Bracket - t;
            #endif
                return half4(RfxOutput(c), 1.0);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
