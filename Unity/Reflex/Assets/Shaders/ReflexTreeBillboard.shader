// The forest's far trees: hardware-instanced 360-degree billboards from the forest's atlas (TreeShader BBVertexShader 0a1291b1 /
// BBPixelShader 53a24063, drawn by XenonForestAsset::vf4 00661000 with stream 0 = the 4 corners (DAT_00a941a4, built in
// XenonForestAsset::vf3) and stream 1 = the tree records (48 bytes: position + type + 0.5, scale, rotation, sin, cos)). Hand port,
// instruction for instruction (_re/notes/forest.md 5):
//   frame: the camera's azimuth atan2(fwd.x, fwd.z) + the tree's rotation, wrapped to [0, 2 pi), * count / 2 pi: frames i and i + 1
//   (u + i uStep) of the type's row of the atlas, blended by 1 - ((max(f, tp) - tp) / (1 - tp))^1.7 (f the fraction, tp the
//   transitionPercent);
//   fade: depth * fadeScale + fadeBias (per type); below 84 / 255 the quad collapses (o0 = (0, 0, 0, 1)); the weights are times the
//   fade, so the alpha test (chosen alpha * weight - 84 / 255 > 0) dissolves the billboard in with distance;
//   depth: the z of the corner pushed 3 units towards the camera (g_BBWorldZ), at the w of the corner itself;
//   lighting: the atlas normal map with the five lights in the billboard's frame (g_Tangent*Light = (L.X, L.Y, L.Z)).
// One draw for every tree of the forest (the original draws the clusters the camera sees past the nearest 3D range; the fade
// collapses every billboard nearer than that, so drawing all of them gives the same image).
Shader "Reflex/Tree Billboard"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Atlas diffuse (BillboardAtlas diffuse, point sampled)", 2D) = "white" {}
        [NoScaleOffset] _NormalMap("Atlas normal (BillboardAtlas normal)", 2D) = "bump" {}
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="TransparentCutout" "Queue"="AlphaTest" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        TEXTURE2D(_BaseMap);   SAMPLER(sampler_BaseMap);
        TEXTURE2D(_NormalMap); SAMPLER(sampler_NormalMap);
        #include "ReflexCommon.hlsl"
        #include "ReflexDrawDistance.hlsl"

        StructuredBuffer<float4> _RfxBBInstances;   // 2 float4 per tree: (x, y, z, type + 0.5), (scale, rotation, sin, cos)
        float4 _RfxBBTreeTypes[60];                 // g_BBTreeTypes c80: per type (width, height, transitionPercent, uStep),
                                                    //   (uStart, vStart, uScale, vScale), (fadeScale, fadeBias, count, 1 / count)
        float4 _RfxBBWorldX, _RfxBBWorldY, _RfxBBWorldZ;                          // c69..c71
        float4 _RfxTreeCamPos, _RfxTreeCamRight, _RfxTreeCamUp, _RfxTreeCamFwd;   // gCameraPos c8, gViewInverse c0..c2 (columns)
        float4 _RfxBBTangentLight0, _RfxBBTangentLight1, _RfxBBTangentLight2, _RfxBBTangentLight3, _RfxBBTangentSun;   // c64..c68 (back, side A, side B, front, sun)

        struct BBAttributes { float4 corner : TEXCOORD0; uint instanceID : SV_InstanceID; };   // stream 0: (x, y, u, v)

        struct BBOut
        {
            float4 positionCS;
            float3 positionWS;    // o2
            float4 uv;            // o1: frame i (xy), frame i + 1 (zw)
            float4 fog;           // o3
            float2 weights;       // o5.zw
        };

        // the frame alpha x weight against 84 / 255 (Reflex's alpha test), or Alive's screen-noise dissolve (as ForwardLit)
        void BBClip(float alpha, float2 pixel)
        {
            if (RfxFoliageDither()) RfxFoliageClip(RfxFoliageFade(1.329411775 - alpha), pixel);
            else clip(round(saturate(alpha - 0.329411775) * 255.0) - 0.5);
        }

        BBOut BBVertex(BBAttributes v)
        {
            BBOut o;
            uint ii = v.instanceID * 2u;
            float4 v0 = _RfxBBInstances[ii];        // position, type + 0.5
            float4 v1 = _RfxBBInstances[ii + 1u];   // scale, rotation, sin, cos
            float4 v2 = v.corner;
            // camera azimuth (0e54..1078): r0.xy = normalize(fwd.z, fwd.x); HLSL atan2 expansion of the compiler
            float fx = _RfxTreeCamFwd.x, fz = _RfxTreeCamFwd.z;
            float rl = rsqrt(fz * fz + fx * fx);
            float2 r0 = float2(fz, fx) * rl;        // r0.x = z, r0.y = x
            float m = max(abs(r0.y), abs(r0.x));
            float n = min(abs(r0.y), abs(r0.x));
            float q = rcp(m) * n;
            float q2 = q * q;
            float p = q2 * 0.0208350997 + -0.0851330012;
            p = q2 * p + 0.180141002;
            p = q2 * p + -0.330299497;
            p = q2 * p + 0.999866009;
            float a = p * q;
            float a2 = a * -2.0 + 1.57079637;
            a = a2 * (abs(r0.y) < abs(r0.x) ? 1.0 : 0.0) + a;
            a = (r0.y < -r0.y ? 1.0 : 0.0) * -3.14159274 + a;
            float twoA = a + a;
            float mn = min(r0.y, r0.x), mx = max(r0.y, r0.x);
            float neg = (mx >= -mx ? 1.0 : 0.0) * (mn < -mn ? 1.0 : 0.0);
            float ang = neg * -twoA + a;
            ang = ang + v1.y;
            ang = (ang < 0.0 ? 1.0 : 0.0) * 6.28318596 + ang;
            float rev = frac(ang * 0.159154922);
            // type rows (mova a0.x = round(floor(v0.w) * 3))
            int ti = (int)round((v0.w - frac(v0.w)) * 3.0);
            float4 row0 = _RfxBBTreeTypes[ti], row1 = _RfxBBTreeTypes[ti + 1], row2 = _RfxBBTreeTypes[ti + 2];
            float fr = rev * row2.z;
            float f = frac(fr);
            float frame = rev * row2.z - f;
            float u0 = frame * row0.w;
            float next = frac((frame + 1.0) * row2.w) * row2.z;
            float u1 = next * row0.w;
            float2 uv = v2.zw * row1.zw + row1.xy;
            o.uv = float4(uv.x + u0, uv.y, uv.x + u1, uv.y);
            o.uv.yw = 1.0 - o.uv.yw;                // Unity's row order (textures are uploaded bottom row first)
            // corner (11b4..1208)
            float2 wh = v2.xy * row0.xy * v1.x;
            float3 P = wh.y * _RfxBBWorldY.xyz + wh.x * _RfxBBWorldX.xyz + v0.xyz;
            o.positionWS = P;
            // fog (120c..131c): k = sat(dot(normalize(sun.x, (dirToCam.y + sun.y) / 2, sun.z), dirToCam) + 1)
            o.fog = RfxFog(P, _RfxTreeCamPos.xyz);
            // fade (1378..13c0)
            float depth = RfxForestDepth(v0.xyz - _RfxTreeCamPos.xyz, _RfxTreeCamFwd.xyz);   // HIGH / ULTRA: the distance, stretched (ReflexDrawDistance.hlsl)
            float fadeV = depth * row2.x + row2.y;
            float hidden = fadeV < 0.329411775 ? 1.0 : 0.0;
            // position: z of the corner pushed 3 towards the camera, at the w of the corner (13dc..1488)
            float4 c = TransformWorldToHClip(P);
            float4 cs = TransformWorldToHClip(P + 3.0 * _RfxBBWorldZ.xyz);
            c.z = cs.z / cs.w * c.w;
            o.positionCS = lerp(c, float4(0, 0, 0, 1), hidden);
            // frame weights (149c..1560)
            float2 g = (max(float2(f, 1.0 - f), row0.z) - row0.z) * rcp(1.0 - row0.z);
            g = exp2(log2(g) * 1.70000005);
            o.weights = (1.0 - g) * fadeV;
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
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"
            #include "ReflexAliveShadow.hlsl"

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float4 uv : TEXCOORD0;
                float4 fog : TEXCOORD1;
                float3 positionWS : TEXCOORD2;
                float2 weights : TEXCOORD3;
            };

            Varyings Vert(BBAttributes v)
            {
                BBOut b = BBVertex(v);
                Varyings o; o.positionCS = b.positionCS; o.uv = b.uv; o.fog = b.fog; o.positionWS = b.positionWS; o.weights = b.weights;
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                // 0988..0a50: the frame whose weighted alpha is larger
                float4 nB = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.zw);
                float4 nA = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.xy);
                float4 cA = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy);
                float4 cB = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.zw);
                float aA = cA.a * i.weights.x, aB = cB.a * i.weights.y;
                bool useB = i.weights.y * cB.a - aA >= 0.0;
                float alpha = useB ? aB : aA;
                float3 nrm = (useB ? nB.rgb : nA.rgb) * 2.0 - 1.0;
                float3 colourTex = useB ? cB.rgb : cA.rgb;
                float outAlpha = alpha - 0.329411775;
                // MX vs ATV Alive (BBPixelShader: max(aA wA, aB wB) - g_ScreenNoise): the screen-noise dissolve, the weights' 84 / 255 start
                // as the 3D trees' fade (RfxFoliageFade); Reflex: ALPHATEST, ALPHAREF 0, GREATER
                BBClip(alpha, i.positionCS.xy);
                if (RfxFoliageDither()) outAlpha = 1.0;
                float3 n = normalize(nrm);
                float3 amb = saturate(dot(_RfxBBTangentLight1.xyz, n)) * _RfxLightColor1.rgb;
                amb += saturate(dot(_RfxBBTangentLight0.xyz, n)) * _RfxLightColor0.rgb;
                amb += saturate(dot(_RfxBBTangentLight2.xyz, n)) * _RfxLightColor2.rgb;
                amb += saturate(dot(_RfxBBTangentLight3.xyz, n)) * _RfxLightColor3.rgb;
                float bounceK = saturate(-n.y);
                float nl = saturate(dot(_RfxBBTangentSun.xyz, n));
                float3 bounce = _RfxColorRef.w * _RfxBounceColor.rgb - (_RfxColorRef.w - 1.0) * _RfxColorRef.rgb;
                amb = bounce * bounceK + amb;
                float3 rad = RfxRadiosity();
                float vol = RfxVolumeShadow(i.positionWS) * RfxMapShadow(i.positionWS);               // samplerStaticShadowMap: sat(y 0.25 + 1.025 - 512 H)
                if (RfxIsAlive())
                {
                    // MX vs ATV Alive (TreeShader BBPixelShader): the atlas texel x the light buffer (not squared): the sun term plus the
                    // light buffer's ambient (its bounce volume; the fill lights only when the level enables them), at the world normal
                    // of the camera-facing card (tangent x = view right, y = view up, z = toward the camera)
                    float3 Nw = normalize(n.x * UNITY_MATRIX_V[0].xyz + n.y * UNITY_MATRIX_V[1].xyz + n.z * UNITY_MATRIX_V[2].xyz);
                    amb = RfxAliveAmbient(Nw, i.positionWS);
                }
                if (RfxIsAlive())
                {
                    // MX vs ATV Alive: the billboard pixels are lit by DL_AmdDirLight like every deferred pixel, so the sun carries the
                    // shadow mask (volume x the dynamic shadow map, DirectionShadowMaskPS; Unity's main-light shadow stands in for the VSM,
                    // 1 beyond its range). The atlas is linearised like every other Alive colour texture: the 3D trees' programs square
                    // their atlas texel in the ALU, BBPixelShader multiplies the raw fetch, so the forest's billboard sampler is a gamma
                    // (sRGB) fetch set by the forest code (the encrypted executable). Evidence: the billboard atlas (mean 0.22, 0.23, 0.15 on
                    // NAT_QR_A_Eco_D) is in the same space as the leaf atlases it was rendered from (0.28 .. 0.35), and the reference stills
                    // show the far forests as dark as the near 3D trees (_re/formats/ALIVE_CONVERSION.md, forests).
                    vol *= RfxAliveDynShadow(i.positionWS);
                }
                colourTex = RfxTreeAlbedo(colourTex);   // Alive's atlas squared in both looks, as its 3D trees' texels (ReflexCommon.hlsl)
                float3 colour = (vol * rad * _RfxSunColor.rgb) * nl + amb;
                colour *= colourTex;
                colour = colour * i.fog.w + i.fog.rgb;
                return half4(RfxOutput(colour), outAlpha);
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
            #pragma target 4.5
            #pragma vertex DepthVert
            #pragma fragment DepthFrag
            struct Varyings { float4 positionCS : SV_POSITION; float4 uv : TEXCOORD0; float2 weights : TEXCOORD1; };
            Varyings DepthVert(BBAttributes v) { BBOut b = BBVertex(v); Varyings o; o.positionCS = b.positionCS; o.uv = b.uv; o.weights = b.weights; return o; }
            half DepthFrag(Varyings i) : SV_Target
            {
                float aA = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy).a * i.weights.x;
                float aB = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.zw).a * i.weights.y;
                BBClip(max(aA, aB), i.positionCS.xy);
                return i.positionCS.z;
            }
            ENDHLSL
        }

        Pass
        {
            Name "DepthNormals"
            Tags { "LightMode"="DepthNormals" }
            Cull Off
            ZWrite On

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex DepthNormalsVert
            #pragma fragment DepthNormalsFrag
            #pragma multi_compile_fragment _ _GBUFFER_NORMALS_OCT
            struct Varyings { float4 positionCS : SV_POSITION; float4 uv : TEXCOORD0; float2 weights : TEXCOORD1; };
            Varyings DepthNormalsVert(BBAttributes v) { BBOut b = BBVertex(v); Varyings o; o.positionCS = b.positionCS; o.uv = b.uv; o.weights = b.weights; return o; }
            half4 DepthNormalsFrag(Varyings i) : SV_Target
            {
                float aA = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy).a * i.weights.x;
                float aB = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.zw).a * i.weights.y;
                BBClip(max(aA, aB), i.positionCS.xy);
                float3 normalWS = -_RfxBBWorldZ.xyz;    // facing the camera
                normalWS = -normalWS;
            #if defined(_GBUFFER_NORMALS_OCT)
                float2 octNormalWS = PackNormalOctQuadEncode(normalWS);
                float2 remapped = saturate(octNormalWS * 0.5 + 0.5);
                return half4(PackFloat2To888(remapped), 0.0);
            #else
                return half4(NormalizeNormalPerPixel(normalWS), 0.0);
            #endif
            }
            ENDHLSL
        }
    }
    Fallback Off
}
