// Hand port of the stands crowd's camera-flash programs (the level's "crowd" shader asset: CameraFlashVS + CameraFlashPS, vs_3_0 / ps_3_0,
// disassembled with _re/formats/d3d9_shader.py; every stadium / national package carries the same asset), drawn by
// XenonBillboardCrowdRenderEvent 006c7880 (the end of vf0 006c6cd0) for each ACTIVE flash record (rx_crowd_api.h):
//
//   CameraFlashVS (gWorld c0, gViewProjection c4, gMaxScale c8 = 15, gCurrentScale c9 = 1, emptyChairRatio c66, View c67 / c68 / c69):
//     one quad of the section's seat mesh (4 vertices sharing the seat position, TEXCOORD0 the corner, NORMAL the seat's facing)
//       seed  = |position|                                  (dp3 with itself: NOT the |(position, 1)| of the crowd program)
//       empty = frac(3.222 seed) >= emptyChairRatio         (the crowd program's empty chair: no flash on it)
//       fwdH  = normalize(c69.x, 0, c69.z)                  (the camera's forward, flattened)
//       hide  = (dot(-normal, fwdH) < 0) || empty           (a seat facing away from the camera, or an empty one: the quad collapses to
//                                                            (0, 0, 0, 1))
//       corner = (2u - 1, 1 - 2v);  q = corner * (gMaxScale * gCurrentScale + 1) = corner * 16
//       P = gWorld position + q.x * c67 + q.y * c68 + (0, 1, 0)      (the camera's right / up: a 32 x 32 square, one unit up)
//       Q = P - 15 * dot(-normal, fwdH) * c69                        (pulled towards the camera, by up to 15 units)
//       position = (clip(P).xy / clip(P).w * clip(Q).w, clip(Q).z, clip(Q).w)
//           = the screen position of P with the depth of Q: the glow keeps its place and wins the depth test against the seats / stand
//     CameraFlashPS: colour = tex(uv) * (3, 3, 3, 1)
//   Render states of the draw (006c7c0b .. 006c7ccb): alpha test off, ALPHABLEND on SRCALPHA / INVSRCALPHA (NOT additive), cull none,
//   z write off (z test as the crowd's), sampler 0 wrap / linear / mip linear (the "CameraFlash" texture, 128 x 128 DXT5). No fog.
// The quad's four vertices are written by OriginalAmbient from the section's mesh: world position (gWorld applied on the CPU), the raw
// object-space normal (the program does not transform it), the corner uv (D3D convention, v down) and uv1.x = emptyChairRatio (c66).
// Output: a raw value for ReflexRenderPipeline's FP16 target (RfxOutput): rgb up to 3 x the texture, as the original's HDR scene.
Shader "Reflex/Original Crowd Flash"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("CameraFlash", 2D) = "white" {}
        _MaxScale("gMaxScale (c8)", Float) = 15
        _CurrentScale("gCurrentScale (c9)", Float) = 1
        _Brightness("Pixel program multiplier (c0.xxx)", Float) = 3
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent" }

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull Off
            ZWrite Off
            ZTest LEqual
            Blend SrcAlpha OneMinusSrcAlpha

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "ReflexCommon.hlsl"
            #include "ReflexVolumetricApply.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float _MaxScale, _CurrentScale, _Brightness;
            CBUFFER_END
            TEXTURE2D(_BaseMap); SAMPLER(sampler_BaseMap);

            struct Attributes { float3 positionWS : POSITION; float3 normal : NORMAL; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float3 positionWS : TEXCOORD1; };

            Varyings Vert(Attributes v)
            {
                Varyings o;
                float2 uvD3D = float2(v.uv0.x, 1.0 - v.uv0.y);                       // the mesh stores (u, 1 - v)
                float3 right = UNITY_MATRIX_V[0].xyz, up = UNITY_MATRIX_V[1].xyz, forward = -UNITY_MATRIX_V[2].xyz;   // c67, c68, c69

                // sge r0.x, frac(3.222 |position|), c66.x: the position is the seat's object-space position (dp3 v0, v0): the world
                // position's own length would differ, so OriginalAmbient hands the object-space length over in uv1.y
                float seed = v.uv1.y;
                float keep = frac(seed * 3.222) >= v.uv1.x ? 1.0 : 0.0;

                float2 fh = normalize(float2(forward.x, forward.z) + 1e-20);
                float3 fwdH = float3(fh.x, 0.0, fh.y);
                float facing = dot(-v.normal, fwdH);
                float hide = (facing < 0.0 || keep > 0.5) ? 1.0 : 0.0;

                float2 corner = float2(2.0 * uvD3D.x - 1.0, 1.0 - 2.0 * uvD3D.y);
                corner = corner * _MaxScale * _CurrentScale + corner;
                float3 P = v.positionWS + corner.x * right + corner.y * up + float3(0.0, 1.0, 0.0);
                float3 Q = P - 15.0 * facing * forward;
                float4 cp = TransformWorldToHClip(P), cq = TransformWorldToHClip(Q);
                float4 pos = float4(cp.xy / cp.w * cq.w, cq.z, cq.w);
                o.positionCS = hide > 0.5 ? float4(0, 0, 0, 1) : pos;
                o.uv = uvD3D;
                o.positionWS = P;
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float4 tex = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, float2(i.uv.x, 1.0 - i.uv.y));
                // VOLUMETRIC LIGHT: the medium in front of the flash (ReflexVolumetricApply.hlsl; Blend SrcAlpha OneMinusSrcAlpha)
                float3 c = RfxVolApply(tex.rgb * _Brightness, tex.a, i.positionWS, i.positionCS, 5, 10);
                return half4(RfxOutput(c), tex.a);   // mul oC0, r0, c0.xxxy: alpha x 1
            }
            ENDHLSL
        }
    }
}
