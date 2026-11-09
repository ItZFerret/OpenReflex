// The level's grass (GrassSystem, NAT_Renne / NAT_RenneB): hand port of GrassShader (GrassVertexShader, PixelShaderFar,
// PixelShaderNear; _re/notes/forest.md 6). Every cell of the grass grid draws the same blade template (six vertices per blade);
// the vertex program reads the cell's place in the world texture (density R, tuft G, 16-bit height A B, bilinear by hand from four
// point taps), drops the blade when its random byte (COLOR1.x) is not below the density, lifts it to the height, sizes it by the
// tuft (G: width = frac(15.9375 G) TuftScale.x + TuftOffset.x, height = floor(15.9375 G) TuftScale.y / 15 + TuftOffset.y) and
// spreads it along the camera's right (g_View) and an up vector bent by the wind (four g_UpVectors blended by frac(world x, z)).
// OriginalForest draws the visible cells of a frame as two instanced draws (far cells back to front, blended; near cells alpha
// tested), one instance per cell in the original's order, with the blade order of the camera's heading (submesh k).
Shader "Reflex/Grass"
{
    Properties
    {
        [NoScaleOffset] _GrassTex("g_GrassTexture (tuft atlas, point sampled)", 2D) = "white" {}
        [NoScaleOffset] _ColorTex("g_ColorTexture (colour map over the grid)", 2D) = "white" {}
        [NoScaleOffset] _WorldTex("g_WorldTexture (density, tuft, height)", 2D) = "black" {}
        _Near("PixelShaderNear (else PixelShaderFar)", Float) = 0
        _AlphaRef("ALPHAREF (GREATER)", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 5
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 10
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent" }

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull Off
            ZWrite On
            ZTest LEqual
            Blend [_SrcBlend] [_DstBlend]

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "ReflexCommon.hlsl"
            #include "ReflexShadows.hlsl"
            #include "ReflexVolumetricApply.hlsl"

            CBUFFER_START(UnityPerMaterial)
                float _Near, _AlphaRef, _SrcBlend, _DstBlend;
            CBUFFER_END
            TEXTURE2D(_GrassTex); SAMPLER(sampler_GrassTex);
            TEXTURE2D(_ColorTex); SAMPLER(sampler_ColorTex);
            TEXTURE2D(_WorldTex);

            // per draw (OriginalForest)
            StructuredBuffer<float4> _RfxGrassCells;   // 2 per instance: (W.x, W.z, X scale, Z scale), (fade, seed bytes c60.yzw)
            float _RfxGrassBase;
            float4 _RfxGrassTuftScale, _RfxGrassTuftOffset;   // g_TuftScale c65, g_TuftOffset c66
            float4 _RfxGrassGrid;                       // W.y (grid bottom), Y scale (2 half.y), alpha test value * 0.94, MIP bias
            float4 _RfxGrassWorldToUV;                  // u = x * a + b, v = z * c + d (g_WorldToUV rows c56 / c57)
            float4 _RfxGrassTexDim;                     // g_WorldTextureDimensions c102 (w, h, 1 / w, 1 / h)
            float4 _RfxGrassUp0, _RfxGrassUp1, _RfxGrassUp2, _RfxGrassUp3;   // g_UpVectors c61..c64 (the wind)
            float4 _RfxGrassViewRight;                  // g_View c20 xyz: the camera's right, horizontal
            float4 _RfxHeightFog;                       // gHeightFog c38 (0 on the grass levels)

            struct Attributes
            {
                float4 uv0 : TEXCOORD0;      // POSITION1 (blade x, z in the cell), TEXCOORD0 (u, v)
                float4 uv1 : TEXCOORD1;      // POSITION0: the blade's corner (x along the camera's right, y up)
                float4 color : COLOR;        // COLOR1: x density threshold, yzw tuft column choice
                uint instanceID : SV_InstanceID;
            };
            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float4 uv : TEXCOORD0;       // o1: atlas uv, fade, mip bias
                float3 cuv : TEXCOORD1;      // o2: colour-map uv, fade
                float4 fog : TEXCOORD2;      // o3: fog colour, amount
                float3 positionWS : TEXCOORD3;
            };

            float3 WorldTap(float2 uv, float2 offset)
            {
                // texldl point tap (texel floor(uv * dims)); height = (A 0.99609375 + B 0.00389099121) 1.00001526 (0d4e0..0500)
                float2 t = (uv + offset) * _RfxGrassTexDim.xy;
                int2 p = clamp(int2(floor(t)), int2(0, 0), int2(_RfxGrassTexDim.xy) - 1);
                float4 c = LOAD_TEXTURE2D(_WorldTex, p);
                return float3(c.r, c.g, (c.a * 0.99609375 + c.b * 0.00389099121) * 1.00001526);
            }

            Varyings Vert(Attributes v)
            {
                Varyings o = (Varyings)0;
                uint ii = ((uint)_RfxGrassBase + v.instanceID) * 2u;
                float4 cell = _RfxGrassCells[ii], seed = _RfxGrassCells[ii + 1u];
                // g_ClusterToWorld: X = (2 hx, 0, 0), Y = (0, 2 half.y, 0), Z = (0, 0, 2 hz), W = (cx - hx, cy - half.y, cz - hz)
                float3 P = float3(cell.x + v.uv0.x * cell.z, _RfxGrassGrid.x, cell.y + v.uv0.y * cell.w);
                float2 uv = float2(P.x * _RfxGrassWorldToUV.x + _RfxGrassWorldToUV.y, P.z * _RfxGrassWorldToUV.z + _RfxGrassWorldToUV.w);
                float2 f = frac(uv * _RfxGrassTexDim.xy);
                float3 t0 = WorldTap(uv, float2(0, 0)), t1 = WorldTap(uv, float2(_RfxGrassTexDim.z, 0));
                float3 t2 = WorldTap(uv, float2(0, _RfxGrassTexDim.w)), t3 = WorldTap(uv, _RfxGrassTexDim.zw);
                float3 w = lerp(lerp(t0, t1, f.x), lerp(t2, t3, f.x), f.y);   // density, tuft, height
                if (!(v.color.x < w.x)) return o;                            // if_lt v3.x, r1.x ... else all outputs 0
                P.y += w.z * _RfxGrassGrid.y;
                float t = w.y * 15.9375;
                float fl = floor(t), fr = t - fl;
                // height: Reflex floor(15.9375 G) Sy / 15 + Oy; MX vs ATV Alive's GrassVertexShader (VS 58 / 59) G Sy + Oy
                float2 wh = float2(fr * _RfxGrassTuftScale.x, (RfxIsAliveData() ? w.y : fl * 0.0666666701) * _RfxGrassTuftScale.y) + _RfxGrassTuftOffset.xy;
                wh *= v.uv1.xy;
                float2 g = frac(P.xz);
                float3 up = normalize(lerp(lerp(_RfxGrassUp0.xyz, _RfxGrassUp1.xyz, g.x), lerp(_RfxGrassUp2.xyz, _RfxGrassUp3.xyz, g.x), g.y));
                P += wh.x * _RfxGrassViewRight.xyz + wh.y * up;
                o.positionWS = P;
                o.positionCS = TransformWorldToHClip(P);
                // fog (07e4..0aa8): smoothstep distance fog, height fog; clamped between the start and end colours / amounts
                float dist = length(_WorldSpaceCameraPos - P);
                float z0 = (_RfxFogDistance.y - _RfxFogDistance.x) != 0 ? 0 : 1;
                float ts = saturate((dist + z0 - (_RfxFogDistance.x - z0)) / (_RfxFogDistance.y - (_RfxFogDistance.x - z0)));
                float ssd = ts * ts * (ts * -2 + 3);
                float hy = clamp(P.y, _RfxHeightFog.z, _RfxHeightFog.w);
                float2 hz = float2((_RfxHeightFog.y - _RfxHeightFog.x) != 0 ? 0 : 1, (_RfxHeightFog.w - _RfxHeightFog.z) != 0 ? 0 : 1);
                float hx = saturate((dist + hz.x - (_RfxHeightFog.x - hz.x)) / (_RfxHeightFog.y - (_RfxHeightFog.x - hz.x)));
                float hh = saturate((hy + hz.y - (_RfxHeightFog.z - hz.y)) / (_RfxHeightFog.w - (_RfxHeightFog.z - hz.y)));
                float hf = max(1 - hh * hh * (hh * -2 + 3), 0);
                float hdist = hx * hx * (hx * -2 + 3);
                float4 d = _RfxFogColorEnd - _RfxFogColorStart;
                float hamount = hdist * d.w + _RfxFogColorStart.w;
                float4 fogc = ssd * d + _RfxFogColorStart;
                fogc.w = hamount * hf + fogc.w;
                o.fog = min(max(min(_RfxFogColorStart, _RfxFogColorEnd), fogc), max(_RfxFogColorStart, _RfxFogColorEnd));
                // the atlas column (0b24..0b8c): the cell's seed bytes against the blade's random bytes
                float3 r = seed.yzw * v.color.yzw;
                float3 s = float3(r.x < r.y ? 1 : 0, r.y < r.z ? 1 : 0, r.x < r.z ? 1 : 0);
                float2 u2 = _RfxGrassTuftScale.wz + v.uv0.z;
                float u3 = lerp(u2.y, u2.x, s.y);
                float u1 = s.z * _RfxGrassTuftScale.w + v.uv0.z;
                o.uv = float4(lerp(u1, u3, s.x), v.uv0.w, seed.x, _RfxGrassGrid.w);
                float2 cuv = float2(P.x * _RfxGrassWorldToUV.x + _RfxGrassWorldToUV.y, P.z * _RfxGrassWorldToUV.z + _RfxGrassWorldToUV.w);
                o.cuv = float3(cuv.x, 1.0 - cuv.y, seed.x);   // colour map: Unity's row order
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float4 tex = SAMPLE_TEXTURE2D_BIAS(_GrassTex, sampler_GrassTex, i.uv.xy, i.uv.w);
                // nrm of a black texel (Alive's grass atlases have them under alpha 0): 0 instead of NaN, which the blended far cells
                // would write into the HDR target (NaN blocks through the bloom, the adaptation falling to its floor)
                float w = pow(abs(dot(tex.rgb * rsqrt(max(dot(tex.rgb, tex.rgb), 1e-12)), 0.57735002)), 16.0);
                float lum = dot(tex.rgb, float3(0.298999995, 0.587000012, 0.114));
                float4 cm = SAMPLE_TEXTURE2D(_ColorTex, sampler_ColorTex, i.cuv.xy);
                float3 c = w * (cm.rgb * lum - tex.rgb) + tex.rgb;
                float k = cm.a * 0.300000012;
                float alpha;
                if (_Near > 0.5)
                {
                    // PixelShaderNear: the dynamic shadow (s4) removes the colour map's 0.3 a term; alpha = min(tex.a + c51, 1) fade
                #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                    float4 shadowCoord = ComputeScreenPos(TransformWorldToHClip(i.positionWS));
                #else
                    float4 shadowCoord = TransformWorldToShadowCoord(i.positionWS);
                #endif
                    if (RfxMainLightShadow(shadowCoord) < 0.5) k = 0;
                    alpha = min(tex.a + _RfxGrassGrid.z * (RfxIsAliveData() ? 1.0 : i.cuv.z), 1.0) * i.cuv.z;   // Alive PixelShaderNear 10 / 21 / 24: c51 unscaled
                }
                else alpha = tex.a * i.cuv.z;
                clip(round(saturate(alpha) * 255.0) - _AlphaRef - 0.5);
                float3 f = lerp(c, i.fog.rgb, i.fog.w);
                float3 colour = f * 0.699999988 + f * k;
                // MX vs ATV Alive (GrassShader PixelShaderFar / Near): the same colour, squared (mul oC0.xyz, r2.xyz, r0.xyz with r2 = r0 x
                // gHDRScale): Alive's display encode of its unlit grass
                if (RfxIsAlive()) colour *= colour;
                // VOLUMETRIC LIGHT: the grass draws in the Transparent queue, after the medium's composite: the medium in front of it
                // (ReflexVolumetricApply.hlsl)
                colour = RfxVolApply(colour, alpha, i.positionWS, i.positionCS, _SrcBlend, _DstBlend);
                return half4(RfxOutput(colour), alpha);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
