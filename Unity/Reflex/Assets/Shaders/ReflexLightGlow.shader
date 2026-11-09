// Hand port of the original MX vs ATV Reflex "glow" programs: the unlit / fogged additive-looking scenery materials that make the
// light glows, light rays / god rays, fog planes and HDR-safe transparent cards of the levels (WP-R3, _re/notes/fx.md). All of them
// share the Template vertex program (world position o0, world normal o1, vertex colour o2, uv0 [+ gTextureOffset c85] o3,
// per-vertex distance fog o4 = (amount * colour, 1 - amount), per-vertex fill-light ambient o5); the pixel programs, transcribed
// instruction by instruction from the shader assets of the install (every same-named copy has the same instructions):
//   0 Template_LightGlow             rgb = tex0(uv).rgb * fog.w + fog.rgb;          a = sat(tex0.a * vcol.r)
//   1 godraytest, FF_Lightrays       rgb = vcol.rgb * fog.w + fog.rgb;              a = sat((1 - grad(1 - |V.N|).r) * tex1(uv + c85).r * vcol.a)
//   2 NAT_DA_GodRay_NoTemp,          rad = floor(ref.a) ? ref.rgb : 1 (ref = s13 at gColorRefProjection c57/c58);
//     NAT_BE_FogPlane                rgb = (rad c56 sat(c55.normalize(N)) vcol.rgb + vcol.rgb amb) * fog.w + fog.rgb;   a = sat(vcol.a)
//   3 FR_CH_VistaFogPlane1, FF_DoorFog  rgb = vcol.rgb * fog.w + fog.rgb;           a = sat(vcol.a)
//   4 HDRSafeTransFog                rgb = tex0(uv).rgb * fog.w + fog.rgb;          a = sat(tex0.a)
//   5 HDRSafeTransparency            rgb = tex0(uv).rgb;                            a = sat(tex0.a)            (no fog)
//   6 FY_Projector                   v = tex0(uv) usr_overbright.w; rgb = (rad c56 sat(c55.N) v + v amb) * fog.w + fog.rgb;  a = sat(tex1(uv).a)
//   7 SX_DLC_FY_Bink_ProjectionBeam  rgb = (4 tex0(uv + c85) + tex1(uv + c85)) * fog.w + fog.rgb;
//                                    a = sat((1 - grad2(1 - |V.N|).r) * tex3(uv).r)
//   8 SUN_Material (the sun billboard, CameraFacingModelEntity; OriginalSun): rgb = tex0(uv).rgb; a = sat(tex1(uv).a); no fog. The
//     billboard matrix of CameraFacingModelEntity's render event (004110a0, mode "CameraFacing"): rows = camera right, up, forward
//     scaled by the entity matrix's row lengths, translation = the entity's (checked against gWorld of the sun draw of a recorded
//     NAT_Stillwater frame); built here from the rendering camera (_RfxBillboard = 1).
// Textures are raw (no sRGB), the output a raw value for ReflexRenderPipeline's HDR target (RfxOutput). Unity uv = (u, 1 - v) of the
// original: a 1-D gradient lookup at D3D (g, g) is (g, 1 - g) here, gTextureOffset (u, v) is (u, -v).
Shader "Reflex/Original Light Glow"
{
    Properties
    {
        [NoScaleOffset] _Tex0("s0", 2D) = "white" {}
        [NoScaleOffset] _Tex1("s1", 2D) = "white" {}
        [NoScaleOffset] _Tex2("s2", 2D) = "white" {}
        [NoScaleOffset] _Tex3("s3", 2D) = "white" {}
        _Program("Program (see header)", Float) = 0
        _Overbright("usr_overbright.w (FY_Projector)", Float) = 5.12
        _RfxBillboard("Camera-facing billboard (SUN_Material)", Float) = 0
        _BillboardScale("Entity matrix row lengths", Vector) = (1, 1, 1, 0)
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 0
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 5
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 10
        [Enum(Off, 0, On, 1)] _ZWrite("ZWrite", Float) = 0
        _AlphaTest("Alpha test", Float) = 0
        _AlphaFuncGE("Alpha func GREATEREQUAL (else GREATER)", Float) = 0
        _Cutoff("Alpha reference", Range(0, 1)) = 0
        // the scenery factory's per-renderer hooks (OriginalProps / OriginalSceneryLod set them through property blocks)
        _RfxModelAlpha("Model fade alpha", Float) = 1
        _RfxFadeOpaque("Fading clone of an opaque material", Float) = 0
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent" }

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull [_Cull]
            ZWrite [_ZWrite]
            ZTest LEqual
            Blend [_SrcBlend] [_DstBlend]

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float4 _BillboardScale;
                float _Program, _Overbright, _RfxBillboard, _AlphaTest, _AlphaFuncGE, _Cutoff, _RfxModelAlpha, _RfxFadeOpaque, _SrcBlend, _DstBlend;
            CBUFFER_END
            TEXTURE2D(_Tex0); SAMPLER(sampler_Tex0);
            TEXTURE2D(_Tex1); SAMPLER(sampler_Tex1);
            TEXTURE2D(_Tex2); SAMPLER(sampler_Tex2);
            TEXTURE2D(_Tex3); SAMPLER(sampler_Tex3);
            // gTextureOffset c85 of a ScrollingTextureEntity (per renderer): the running game's (OriginalProps: _RfxTextureOffset /
            // _RfxScrollOverride, z = 1 when set), else the entity's wrap periods on the clock (_ScrollParams, ReflexScenery)
            float4 _RfxTextureOffset, _RfxScrollOverride, _ScrollParams;

            #include "ReflexCommon.hlsl"
            #include "ReflexVolumetricApply.hlsl"

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv0 : TEXCOORD0; float4 color : COLOR; };
            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 positionWS : TEXCOORD0;
                float3 normalWS : TEXCOORD1;      // un-normalised world normal (o1)
                float4 color : TEXCOORD2;         // o2
                float4 uv : TEXCOORD3;            // xy uv0, zw uv0 + gTextureOffset
                float4 fog : TEXCOORD4;           // o4
                float3 ambient : TEXCOORD5;       // o5
            };

            float2 TextureOffset()
            {
                if (_RfxTextureOffset.z > 0.5) return _RfxTextureOffset.xy;
                if (_RfxScrollOverride.z > 0.5) return _RfxScrollOverride.xy;
                return float2(_ScrollParams.x > 0 ? frac(_Time.y / _ScrollParams.x) : 0, _ScrollParams.y > 0 ? frac(_Time.y / _ScrollParams.y) : 0) * (1.0 - 2.0 * _ScrollParams.zw);
            }

            Varyings Vert(Attributes v)
            {
                Varyings o;
                float3x3 m = (float3x3)GetObjectToWorldMatrix();
                if (_RfxBillboard > 0.5)
                {
                    // CameraFacingModelEntity: world = entity position + right sx x + up sy y + forward sz z of the rendering camera
                    // (Unity's view space looks down -z: forward = -third column of the inverse view matrix)
                    float3 right = UNITY_MATRIX_I_V._m00_m10_m20, up = UNITY_MATRIX_I_V._m01_m11_m21, fwd = -UNITY_MATRIX_I_V._m02_m12_m22;
                    float3 origin = GetObjectToWorldMatrix()._m03_m13_m23;
                    o.positionWS = origin + right * (_BillboardScale.x * v.positionOS.x) + up * (_BillboardScale.y * v.positionOS.y) + fwd * (_BillboardScale.z * v.positionOS.z);
                    o.normalWS = right * v.normalOS.x + up * v.normalOS.y + fwd * v.normalOS.z;
                }
                else
                {
                    o.positionWS = TransformObjectToWorld(v.positionOS.xyz);
                    o.normalWS = mul(m, v.normalOS);
                }
                o.positionCS = TransformWorldToHClip(o.positionWS);
                o.color = v.color;
                float2 off = TextureOffset();
                o.uv = float4(v.uv0, v.uv0 + float2(off.x, -off.y));
                o.fog = RfxFog(o.positionWS, _WorldSpaceCameraPos);
                o.ambient = RfxAmbient(o.normalWS);
                return o;
            }

            // 1 - gradient(1 - |V.N|).x of the beam programs (godraytest 017c..01f4)
            float ViewFade(TEXTURE2D_PARAM(grad, s), float3 P, float3 N)
            {
                float3 V = normalize(_WorldSpaceCameraPos - P);
                float g = 1.0 - abs(dot(V, normalize(N)));
                return 1.0 - SAMPLE_TEXTURE2D(grad, s, float2(g, 1.0 - g)).x;
            }
            // the sun term of the lit programs: rad c56 sat(c55.normalize(N)) (NAT_BE_FogPlane 0198..0234)
            float3 SunTerm(float3 P, float3 N)
            {
                return RfxRadiosityAt(P) * _RfxSunColor.rgb * saturate(dot(_RfxSunDir.xyz, normalize(N)));
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float3 rgb; float a;
                int p = (int)(_Program + 0.5);
                if (p == 0)
                {
                    float4 t = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.xy);
                    rgb = t.rgb * i.fog.w + i.fog.rgb; a = saturate(t.a * i.color.r);
                }
                else if (p == 1)
                {
                    rgb = i.color.rgb * i.fog.w + i.fog.rgb;
                    a = saturate(ViewFade(TEXTURE2D_ARGS(_Tex0, sampler_Tex0), i.positionWS, i.normalWS) * SAMPLE_TEXTURE2D(_Tex1, sampler_Tex1, i.uv.zw).x * i.color.a);
                }
                else if (p == 2)
                {
                    float3 c = SunTerm(i.positionWS, i.normalWS) * i.color.rgb + i.color.rgb * i.ambient;
                    rgb = c * i.fog.w + i.fog.rgb; a = saturate(i.color.a);
                }
                else if (p == 3)
                {
                    rgb = i.color.rgb * i.fog.w + i.fog.rgb; a = saturate(i.color.a);
                }
                else if (p == 4)
                {
                    float4 t = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.xy);
                    rgb = t.rgb * i.fog.w + i.fog.rgb; a = saturate(t.a);
                }
                else if (p == 5)
                {
                    float4 t = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.xy);
                    rgb = t.rgb; a = saturate(t.a);
                }
                else if (p == 6)
                {
                    float3 vid = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.xy).rgb * _Overbright;
                    float3 c = SunTerm(i.positionWS, i.normalWS) * vid + vid * i.ambient;
                    rgb = c * i.fog.w + i.fog.rgb; a = saturate(SAMPLE_TEXTURE2D(_Tex1, sampler_Tex1, i.uv.xy).a);
                }
                else if (p == 7)
                {
                    float3 c = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.zw).rgb * 4.0 + SAMPLE_TEXTURE2D(_Tex1, sampler_Tex1, i.uv.zw).rgb;
                    rgb = c * i.fog.w + i.fog.rgb;
                    a = saturate(ViewFade(TEXTURE2D_ARGS(_Tex2, sampler_Tex2), i.positionWS, i.normalWS) * SAMPLE_TEXTURE2D(_Tex3, sampler_Tex3, i.uv.xy).x);
                }
                else
                {
                    rgb = SAMPLE_TEXTURE2D(_Tex0, sampler_Tex0, i.uv.xy).rgb;
                    a = saturate(SAMPLE_TEXTURE2D(_Tex1, sampler_Tex1, i.uv.xy).a);
                }
                if (_AlphaTest > 0.5) clip(a * 255.0 - _Cutoff * 255.0 + (_AlphaFuncGE > 0.5 ? 0.5 : -0.5));
                float outAlpha = (_RfxFadeOpaque > 0.5 ? 1.0 : a) * _RfxModelAlpha;
                // VOLUMETRIC LIGHT: the glows draw after the medium's composite: the medium in front of them (ReflexVolumetricApply.hlsl; additive
                // glows only dim by its transmittance)
                if (_SrcBlend != 1.0 || _DstBlend != 0.0) rgb = RfxVolApply(rgb, outAlpha, i.positionWS, i.positionCS, _SrcBlend, _DstBlend);
                return half4(RfxOutput(rgb), outAlpha);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
