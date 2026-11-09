// Hand port of the two scenery programs of the original that draw an in-level Bink video's render target with their own math
// (SX_DLC01_FreightYard / SX_DLC02_FreightYardB; the other video screens use Template_Prelit_Map_Ctl / Template_Prelit_Stadiums,
// already modes of Reflex/Original Scenery). OriginalVideoScreens (Scripts/OriginalWorld) switches those materials to this shader
// and binds the video's render target; _re/notes/screens-ads.md. Raw values as everywhere in the world (ReflexCommon RfxOutput).
//
// _Program 0: FY_Projector (material SX_DLC_FY_Building_Bink: the factory wall screen). Pixel program 0788..089c:
//   rad   = floor(ref.a) ? ref.rgb : 1   (s13 colour reference at gColorRefProjection c57 / c58 of the world position)
//   sun   = rad * gSunLightColor c56 * sat(gSunLight c55 . normalize(N))
//   video = tex(s0 usr_sampler_video_file, uv0) * usr_overbright.w (c85)
//   rgb   = (sun * video + video * vertexAmbient) * (1 - fog) + fog;  alpha = sat(tex(s1 s_sampler1, uv0).a)
//   (vertex ambient: the four fill lights + bounce of every Template vertex program, o4; fog o3)
// _Program 1: SX_DLC_FY_Bink_ProjectionBeam (ScrollingTextureRSData SX_DLC_ProjectorBox_Beam). Vertex 0c00: o2.xy = uv0 + gTextureOffset
//   c85; pixel 0728..0808: rgb = (tex(s0 dust, uv0 + off) * 4 + tex(s1 video target, uv0 + off)) * (1 - fog) + fog;
//   alpha = sat((1 - tex(s2 gradient, (x, x)).x) * tex(s3 beam mask, uv0).x),  x = 1 - |normalize(c8 - P) . normalize(N)|
// Render states are the material's (SRCALPHA / INVSRCALPHA blend on both; ZWRITE 1 on the screen, 0 on the beam; CULL CCW).
Shader "Reflex/Original Video Screen"
{
    Properties
    {
        [NoScaleOffset] _VideoTex("Video render target (VideoEntity Texture To Override)", 2D) = "black" {}
        [NoScaleOffset] _MaskTex("Program 0: s1 screen alpha / program 1: s3 beam mask", 2D) = "white" {}
        [NoScaleOffset] _DustTex("Program 1: s0 dust (scrolling)", 2D) = "black" {}
        [NoScaleOffset] _GradTex("Program 1: s2 view gradient", 2D) = "black" {}
        _Program("Program: 0 FY_Projector, 1 SX_DLC_FY_Bink_ProjectionBeam", Float) = 0
        _Overbright("usr_overbright.w", Float) = 5.12
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 5
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 10
        [Enum(Off, 0, On, 1)] _ZWrite("ZWrite", Float) = 1
        _RfxModelAlpha("Model fade alpha", Float) = 1
        _RfxFadeOpaque("Fading clone of an opaque material", Float) = 0
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        CBUFFER_START(UnityPerMaterial)
            float _Program, _Overbright, _RfxModelAlpha, _RfxFadeOpaque, _SrcBlend, _DstBlend;
        CBUFFER_END
        TEXTURE2D(_VideoTex); SAMPLER(sampler_VideoTex);
        TEXTURE2D(_MaskTex);  SAMPLER(sampler_MaskTex);
        TEXTURE2D(_DustTex);  SAMPLER(sampler_DustTex);
        TEXTURE2D(_GradTex);  SAMPLER(sampler_GradTex);
        float4 _ScrollParams;          // ScrollingTextureRSData (per renderer): U / V seconds for wrap, reverse U / V (no running game)
        float4 _RfxScrollOverride;     // the running game's gTextureOffset (OriginalProps) or a recorded frame's (tests): xy when z > 0
        float4 _RfxTextureOffset;      // the same offset as OriginalProps sets it for the scenery shader
        #include "ReflexCommon.hlsl"

        // gTextureOffset c85 in d3d UV (Unity v = 1 - v_d3d: the offset's v changes sign)
        float2 ScrollOffset()
        {
            if (_RfxScrollOverride.z > 0.5) return _RfxScrollOverride.xy;
            if (_RfxTextureOffset.z > 0.5) return _RfxTextureOffset.xy;
            return float2(_ScrollParams.x > 0 ? frac(_Time.y / _ScrollParams.x) : 0, _ScrollParams.y > 0 ? frac(_Time.y / _ScrollParams.y) : 0) * (1.0 - 2.0 * _ScrollParams.zw);
        }
        ENDHLSL

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
            #include "ReflexVolumetricApply.hlsl"

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv0 : TEXCOORD0; };
            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 positionWS : TEXCOORD0;
                float3 normalWS : TEXCOORD1;     // un-normalised world normal (the programs normalise per pixel)
                float2 uv : TEXCOORD2;
                float4 fog : TEXCOORD3;          // rgb amount * fog colour, w 1 - amount (o3 / o4)
                float3 ambient : TEXCOORD4;      // the fill-light rig of the vertex program (o4 / o5)
            };

            Varyings Vert(Attributes v)
            {
                Varyings o;
                o.positionWS = TransformObjectToWorld(v.positionOS.xyz);
                o.positionCS = TransformWorldToHClip(o.positionWS);
                o.normalWS = mul((float3x3)GetObjectToWorldMatrix(), v.normalOS);
                o.uv = v.uv0;
                o.fog = RfxFog(o.positionWS, _WorldSpaceCameraPos);
                o.ambient = RfxAmbient(o.normalWS);
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float3 P = i.positionWS;
                float3 N = normalize(i.normalWS);
                float3 rgb; float alpha;
                if (_Program < 0.5)
                {
                    float3 rad = RfxRadiosityAt(P);
                    float3 sun = rad * _RfxSunColor.rgb * saturate(dot(_RfxSunDir.xyz, N));
                    float3 video = SAMPLE_TEXTURE2D(_VideoTex, sampler_VideoTex, i.uv).rgb * _Overbright;
                    rgb = sun * video + video * i.ambient;
                    alpha = saturate(SAMPLE_TEXTURE2D(_MaskTex, sampler_MaskTex, i.uv).a);
                }
                else
                {
                    float2 off = ScrollOffset();
                    float2 uvs = i.uv + float2(off.x, -off.y);
                    float3 video = SAMPLE_TEXTURE2D(_VideoTex, sampler_VideoTex, uvs).rgb;
                    float3 dust = SAMPLE_TEXTURE2D(_DustTex, sampler_DustTex, uvs).rgb;
                    rgb = dust * 4.0 + video;
                    float3 V = normalize(_WorldSpaceCameraPos - P);
                    float x = 1.0 - abs(dot(V, N));
                    float g = SAMPLE_TEXTURE2D(_GradTex, sampler_GradTex, float2(x, 1.0 - x)).x;   // d3d (x, x)
                    alpha = saturate((1.0 - g) * SAMPLE_TEXTURE2D(_MaskTex, sampler_MaskTex, i.uv).x);
                }
                rgb = rgb * i.fog.w + i.fog.rgb;
                float outAlpha = (_RfxFadeOpaque > 0.5 ? 1.0 : alpha) * _RfxModelAlpha;
                // VOLUMETRIC LIGHT: blended screens draw after the medium's composite: the medium in front of them (ReflexVolumetricApply.hlsl)
                if (_SrcBlend != 1.0 || _DstBlend != 0.0) rgb = RfxVolApply(rgb, outAlpha, P, i.positionCS, _SrcBlend, _DstBlend);
                return half4(RfxOutput(rgb), outAlpha);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
