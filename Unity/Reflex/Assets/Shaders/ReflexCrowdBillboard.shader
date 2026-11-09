// Hand port of the original billboard crowd programs (shader resource "crowd" of each stadium / national level package:
// StaticVertexShader + CrowdPixelShader), drawn by XenonBillboardCrowdRenderEvent (006c6cd0) for BillboardCrowdEntity sections.
// A crowd section model holds one camera-facing quad per seat: its 4 corners share the seat position; TEXCOORD0 is the corner.
//   vs: corner c = (uv * (2, -2) + (-1, 1)) * (1.75, 3.75); world = gWorld * pos + c.x * right + c.y * up (View c67 / c68)
//       view column = floor(16 * lerp(t, max(0.875 - t, 0), side)), t = sat(0.5 * dot(-n, forward)), side = dot(-n, right) < 0
//       seed = |(pos, 1)|; frame = floor(8 * (seed + time)); u = (uv.x + column) / 16;
//       v = animated and seated ? (uv.y + frame) / 8 : uv.y   (time c65, isAnimating c71)
//       empty seat = emptyChairRatio (c66) < frac(3.222 seed)  ->  the EmptyChair texture
//       tint = colorTable[floor(24 frac(seed))] (c72[24], the program's own constant table defaults)
//       fog: the scenery's distance fog (c14 / c15 / c16 / c26)
//   MX vs ATV Alive levels (the level has no crowd program: the Xbox one is in the encrypted executable): the section is drawn as every Alive
//       colour program draws an unlit surface, (tex x vertex colour)^2 (GrassShader, the T_Prelit / banner programs: the texel is squared, then fog).
//       Measured against the SX captures (Lexington / Williamsburg): the squared, unlit crowd is the dark maroon crowd of the stands (pink skin and
//       chairs squared; the distant sections pale with the fog); lit by the light buffer (sun x MapShadow + ambient + bounce volume) it comes out black
//       (the stands get almost none), unsquared it is the pale grey-pink crowd the Reflex programs give.
//   ps: tex = empty ? EmptyChair : Base; rgb *= tint where 0.27451 < a < 0.588235 (the clothing band); colour = rgb * vertex colour;
//       out = lerp(colour, fog colour, fog); alpha test a > 100/255 (ALPHAREF 100, GREATER); cull none.
// sRGB sampling is disabled in the original device: raw values for ReflexRenderPipeline's FP16 target (ReflexCommon.hlsl RfxOutput).
// Verified against the software-rasterized original's draws (Editor/Tests/ObjShadeTests: mean |d| 0.0004..0.0006 HDR).
Shader "Reflex/Original Crowd Billboard"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Crowd (16 views x 8 frames)", 2D) = "white" {}
        [NoScaleOffset] _EmptyChair("Empty chair", 2D) = "white" {}
        _CrowdTime("time (c65)", Float) = 0
        _EmptyChairRatio("emptyChairRatio (c66)", Float) = 0.9
        _IsAnimating("isAnimating (c71)", Float) = 0
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="TransparentCutout" "Queue"="AlphaTest" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        #include "ReflexCommon.hlsl"

        CBUFFER_START(UnityPerMaterial)
            float _CrowdTime, _EmptyChairRatio, _IsAnimating;
        CBUFFER_END
        TEXTURE2D(_BaseMap);    SAMPLER(sampler_BaseMap);
        TEXTURE2D(_EmptyChair); SAMPLER(sampler_EmptyChair);
        float4 _RfxCrowdColorTable[24];   // colorTable c72[24] (Shader.SetGlobalVectorArray by OriginalAmbient)
        // (_RfxSunDir, _RfxFogColorStart / End, _RfxFogParams: ReflexCommon.hlsl, the level's globals)

        struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv0 : TEXCOORD0; float4 color : COLOR; };
        struct Varyings
        {
            float4 positionCS : SV_POSITION;
            float2 uv : TEXCOORD0;      // D3D convention (v down)
            float4 color : TEXCOORD1;
            float4 tint : TEXCOORD2;
            float empty : TEXCOORD3;
            float4 fog : TEXCOORD4;     // rgb: amount * fog colour, w: 1 - amount
        };

        Varyings Vert(Attributes v)
        {
            Varyings o;
            float2 uvD3D = float2(v.uv0.x, 1.0 - v.uv0.y);                     // the mesh stores (u, 1 - v)
            float3 right = UNITY_MATRIX_V[0].xyz, up = UNITY_MATRIX_V[1].xyz, forward = -UNITY_MATRIX_V[2].xyz;
            float2 corner = (uvD3D * float2(2, -2) + float2(-1, 1)) * float2(1.75, 3.75) * RfxMapScale();   // MAP SCALE: the crowd grows with its stands
            float3 world = TransformObjectToWorld(v.positionOS.xyz);
            float3 positionWS = world + corner.x * right + corner.y * up;
            o.positionCS = TransformWorldToHClip(positionWS);

            // view column (16 views around the seat) and animation frame
            float side = dot(-v.normalOS, right) < 0.0 ? 1.0 : 0.0;
            float t = saturate(dot(-v.normalOS, forward) * 0.5);
            float a = side * max(0.875 - t, 0.0) + (1.0 - side) * t;
            float column = floor(a * 16.0);
            float seed = sqrt(dot(v.positionOS.xyz, v.positionOS.xyz) + 1.0);
            float frame = floor((seed + _CrowdTime) * 8.0);
            float2 uv = float2(uvD3D.x * 0.0625 + column * 0.0625, uvD3D.y * 0.125 + frame * 0.125);
            float empty = _EmptyChairRatio < frac(seed * 3.222) ? 1.0 : 0.0;
            if (_IsAnimating == 0.0 || empty > 0.0) uv.y = uvD3D.y;
            o.uv = uv;
            o.empty = empty;
            o.color = float4(v.color.rgb, 1.0);
            o.tint = _RfxCrowdColorTable[(int)floor(frac(seed) * 24.0)];

            // distance fog (as Reflex/Original Scenery: DistanceFogRSData c14 / c15 / c16 / c26)
            float3 toCam = _WorldSpaceCameraPos - world;
            float dist = length(toCam);
            float3 dirToCam = toCam / max(dist, 1e-5);
            float range = max(_RfxFogParams.y - _RfxFogParams.x, 1e-3);
            float f = saturate((dist - _RfxFogParams.x) / range);
            float amount = clamp(f * (_RfxFogParams.w - _RfxFogParams.z) + _RfxFogParams.z, _RfxFogParams.z, _RfxFogParams.w);
            float3 s = normalize(float3(_RfxSunDir.x, (dirToCam.y + _RfxSunDir.y) * 0.5, _RfxSunDir.z));
            float k = saturate(dot(s, dirToCam) + 1.0);
            if (_RfxAlive.x > 0.5) k = f;     // MX vs ATV Alive's vertex programs ramp the fog colour with the distance factor (ReflexCommon.hlsl RfxFog)
            float3 fogColour = _RfxFogColorStart.rgb + (_RfxFogColorEnd.rgb - _RfxFogColorStart.rgb) * k;
            o.fog = float4(amount * fogColour, 1.0 - amount);
            return o;
        }
        ENDHLSL

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull Off
            ZWrite On
            ZTest LEqual

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag

            half4 Frag(Varyings i) : SV_Target
            {
                float2 uv = float2(i.uv.x, 1.0 - i.uv.y);
                float4 tex = i.empty > 0.5 ? SAMPLE_TEXTURE2D(_EmptyChair, sampler_EmptyChair, uv) : SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, uv);
                clip(tex.a * 255.0 - 100.5);                                   // ALPHATESTENABLE, ALPHAFUNC GREATER, ALPHAREF 100
                bool band = tex.a > 0.27451 && tex.a < 0.588235;
                float3 rgb = band ? tex.rgb * i.tint.rgb : tex.rgb;
                float3 colour = rgb * i.color.rgb;
                colour = RfxAliveAlbedo(colour);                               // Alive: the albedo is squared (a Reflex level: unchanged)
                colour = colour * i.fog.w + i.fog.rgb;
                return half4(RfxOutput(colour), 1.0);   // raw values for ReflexRenderPipeline's HDR target
            }
            ENDHLSL
        }

        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            Cull Off
            ZWrite On
            ColorMask R

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment DepthFrag
            half DepthFrag(Varyings i) : SV_Target
            {
                float2 uv = float2(i.uv.x, 1.0 - i.uv.y);
                float4 tex = i.empty > 0.5 ? SAMPLE_TEXTURE2D(_EmptyChair, sampler_EmptyChair, uv) : SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, uv);
                clip(tex.a * 255.0 - 100.5);
                return 0;
            }
            ENDHLSL
        }
    }
}
