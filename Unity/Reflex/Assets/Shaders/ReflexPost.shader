// The original HDR post-processing chain (MXReflex.exe 007b4460..007b5380, HDRSettingsRSData), pass by pass as the recorded
// frame of the original renderer draws it (_re/notes/render-pipeline.md); the pixel programs are transcribed instruction for
// instruction. All targets are FP16 (A16B16G16R16F) or R32F like the original's.
//
// Sampling positions: the original's blit vertex program adds g_HalfPixelOffset = (-1/W, 1/H) of the BACK BUFFER (W x H) to
// the clip position and D3D9 samples pixel (i, j) at integer screen coordinates, so a target of w x h reads at
//   u = i / w + 1 / (2 W),   v = j / h + 1 / (2 H)        (D3D v, top row 0)
// The vertex program here passes Unity's pixel-centre UV; D3D9UV() converts it to those coordinates.
Shader "Hidden/Reflex/Post"
{
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Color.hlsl"

    TEXTURE2D(_RfxSrc0); SAMPLER(sampler_RfxSrc0);
    TEXTURE2D(_RfxSrc1); SAMPLER(sampler_RfxSrc1);
    TEXTURE2D(_RfxSrc2); SAMPLER(sampler_RfxSrc2);
    SamplerState sampler_LinearClamp, sampler_PointClamp;
    float4 _RfxTargetSize;      // w, h, 1/w, 1/h of the target being drawn
    float4 _RfxScreenSize;      // W, H of the back buffer (g_HalfPixelOffset)
    float4 _RfxSrcSize;         // g_SamplerSize of the pixel program (source w, h)
    float4 _RfxBright;          // minBloomLuminance c0, maxBloomLuminance c1, wholeSceneBloomAmount c2
    float4 _RfxAdapt;           // minLuminance c0, maxLuminance c1, g_fElapsedTime c2, g_firstTime c3
    float4 _RfxTone;            // g_KeyValue c0, g_bloomAttn c1, g_OneOverWhitePointSquared c2, g_luminanceShift c3
    float4 _RfxBlurWeights;     // g_avSampleWeights[0..2].x
    float4 _RfxFlip;            // x: 1 when the source textures are stored top row first (D3D/Metal render targets)
    float4 _RfxAlive;           // x: an MX vs ATV Alive level (Content/AliveLevels.cs AliveBounce.ApplyGlobals)
    float4 _RfxAliveLut;        // x: the level's colour grade is bound, yz: g_LUTScaleOffset (31/32, 0.5/32) (AliveBounce.ApplyLut)
    TEXTURE3D(_RfxAliveLutTex);

    struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
    V Vert(uint id : SV_VertexID)
    {
        V o;
        o.positionCS = GetFullScreenTriangleVertexPosition(id);
        o.uv = GetFullScreenTriangleTexCoord(id);            // Unity image UV: (0,0) bottom-left ... (1,1) top-right
        return o;
    }
    // Unity image UV of this pixel's centre -> the D3D9 coordinates the original samples at, as a Unity UV (v up).
    float2 D3D9UV(float2 uv)
    {
        float2 ij = floor(float2(uv.x * _RfxTargetSize.x, (1.0 - uv.y) * _RfxTargetSize.y));   // D3D pixel index (top row 0)
        float2 d3d = ij * _RfxTargetSize.zw + 0.5 / _RfxScreenSize.xy;
        return float2(d3d.x, 1.0 - d3d.y);
    }
    // D3D texture coordinates (v down) -> Unity UV
    float2 U(float2 d3d) { return float2(d3d.x, 1.0 - d3d.y); }
    float2 D(float2 uv) { return float2(uv.x, 1.0 - uv.y); }
    float4 Lin0(float2 d3d) { return SAMPLE_TEXTURE2D_LOD(_RfxSrc0, sampler_LinearClamp, U(d3d), 0); }
    float4 Pt0(float2 d3d) { return SAMPLE_TEXTURE2D_LOD(_RfxSrc0, sampler_PointClamp, U(d3d), 0); }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off Blend Off

        // 0: copy (c2c4b2: texld oC0, v0, s0; bilinear, clamp) - the 1/2, 1/4, 1/8 downsamples and 64 -> 16 -> 4 luminance steps
        Pass
        {
            Name "Copy"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target { return Lin0(D(D3D9UV(i.uv))); }
            ENDHLSL
        }

        // 1: bright pass (e3e8cc12): t = sat((lum - min) / (max - min)); s = t^2 (3 - 2t); out = max(max(s * max * c, c * whole), 0)
        Pass
        {
            Name "BrightPass"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 c = Lin0(D(D3D9UV(i.uv)));
                if (_RfxBright.w > 0.5)
                {
                    // MX vs ATV Alive BrightPassPS: x = colour x exposure (g_KeyValue / adapted luminance); t = max(x - g_BloomThreshold, 0);
                    // out = t / (t + g_BloomDistribution) (g_BloomAttn is applied by the tonemap pass)
                    float la = SAMPLE_TEXTURE2D_LOD(_RfxSrc2, sampler_PointClamp, float2(0.5, 0.5), 0).x;
                    float3 t = max(c.rgb * (rcp(la + 9.99999975e-05) * _RfxTone.x) - _RfxBright.x, 0.0);
                    return float4(t * rcp(t + _RfxBright.y), 1.0);
                }
                float r0x = rcp(_RfxBright.y - _RfxBright.x);
                float lum = dot(c.rgb, float3(0.300000012, 0.589999974, 0.109999999)) - _RfxBright.x;
                float t = saturate(r0x * lum);
                float s = t * t * (t * -2.0 + 3.0) * _RfxBright.y;
                float4 a = s * c, b = c * _RfxBright.z;
                return max(max(a, b), 0.0);
            }
            ENDHLSL
        }

        // 2: log luminance 64 x 64 (cca7954a): mean of ln(dot(clamp(c, 0, 32), lum) + 1e-4) over 9 taps at -1, 0, +1 source texels
        Pass
        {
            Name "LogLuminance"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float L(float2 d3d)
            {
                float3 c = min(max(Lin0(d3d).rgb, 0.0), 32.0);
                return log2(dot(float3(0.300000012, 0.589999974, 0.109999999), c) + 9.99999975e-05);
            }
            float4 F(V i) : SV_Target
            {
                float2 uv = D(D3D9UV(i.uv));
                float2 o = rcp(_RfxSrcSize.xy);
                // instruction order of 06ac..0b0c (sum of log2 * ln 2 in this order, then / 9)
                float a = L(uv);
                float b = L(uv - o);
                float c = L(uv + float2(-o.x, o.y));    // r2.zw = o.xy * (-1, 1) + uv  (mad r2, r1.xyxy, c1.xyxz)
                float d = L(uv + float2(-o.x, 0));       // r2.xy = (-o.x, 0)
                float acc = d * 0.693147182;
                acc = b * 0.693147182 + acc;
                acc = c * 0.693147182 + acc;
                float e = L(uv + float2(0, o.y));       // r2.zw (0, +1)
                float f = L(uv + float2(0, -o.y));      // r2.xy (0, -1)
                acc = f * 0.693147182 + acc;
                acc = a * 0.693147182 + acc;
                acc = e * 0.693147182 + acc;
                float g = L(uv + o);                     // r0.yz = o + uv
                float h = L(uv + float2(o.x, o.y * 0)) ; // r2.zw = (+1, 0)
                float k = L(uv + float2(o.x, -o.y));     // r2.xy = (+1, -1)
                acc = k * 0.693147182 + acc;
                acc = h * 0.693147182 + acc;
                acc = g * 0.693147182 + acc;
                return acc * 0.111111112;
            }
            ENDHLSL
        }

        // 3: 4 x 4 -> 1 x 1 (81449f92): exp2(sum of 16 taps at (-1.5..1.5) source texels * 0.0901684389 = 1 / (16 ln 2))
        Pass
        {
            Name "LuminanceFinal"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = D(D3D9UV(i.uv));
                float2 r = rcp(_RfxSrcSize.xy);
                const float4 c1 = float4(-1.5, -0.5, 0.5, 1.5);
                float s = 0;
                [unroll] for (int x = 0; x < 4; x++) [unroll] for (int y = 0; y < 4; y++)   // the original's summation order
                    s += Lin0(uv + r * float2(c1[x], c1[y])).x;
                return exp2(s * 0.0901684389);
            }
            ENDHLSL
        }

        // 4: adaptation (bb1ceb2b): s0 current, s1 previous adapted; a = first ? cur : prev; a += (cur - a) * rate; clamp [min, max]
        Pass
        {
            Name "Adapt"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float cur = SAMPLE_TEXTURE2D_LOD(_RfxSrc0, sampler_PointClamp, float2(0.5, 0.5), 0).x;
                float prev = SAMPLE_TEXTURE2D_LOD(_RfxSrc1, sampler_LinearClamp, float2(0.5, 0.5), 0).x;
                float r0y = rcp(cur);
                r0y = -abs(r0y) >= 0 ? prev : cur;                 // cmp r0.y, -|1/cur|, prev, cur
                float r0x = -abs(_RfxAdapt.w) >= 0 ? prev : r0y;    // cmp r0.x, -|first|, prev, r0.y
                float a = _RfxAdapt.z * (r0y - r0x) + r0x;          // lrp r1.x, rate, r0.y, r0.x
                float o = (a - _RfxAdapt.x) >= 0 ? min(a, _RfxAdapt.y) : _RfxAdapt.x;
                return float4(o, o, o, 1);
            }
            ENDHLSL
        }

        // 5 / 6: 3-tap gaussian, horizontal (a3f2b7e7) / vertical (65bb4251), point sampling, g_avSampleWeights
        Pass
        {
            Name "BlurH"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = D(D3D9UV(i.uv));
                float o = rcp(_RfxSrcSize.x);
                float4 l = Pt0(uv + float2(-o, 0)), r = Pt0(uv + float2(o, 0)), m = Pt0(uv);
                float4 acc = m * _RfxBlurWeights.y;
                acc = _RfxBlurWeights.x * l + acc;
                return _RfxBlurWeights.z * r + acc;
            }
            ENDHLSL
        }
        Pass
        {
            Name "BlurV"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = D(D3D9UV(i.uv));
                float o = rcp(_RfxSrcSize.y);
                float4 l = Pt0(uv + float2(0, -o)), r = Pt0(uv + float2(0, o)), m = Pt0(uv);
                float4 acc = m * _RfxBlurWeights.y;
                acc = _RfxBlurWeights.x * l + acc;
                return _RfxBlurWeights.z * r + acc;
            }
            ENDHLSL
        }

        // 7: tonemap (0f3bb02c): s0 HDR scene (point), s1 bloom (bilinear), s2 adapted luminance
        //   Ls = L * key / (La + 1e-4); scale = Ls (1 + Ls / white^2) / (1 + Ls) / (L + 1e-4); out = c * scale + bloom * attn * scale
        //   alpha = c.a <= 0 ? c.a : 1. The 8-bit target quantises; the result is written for the sRGB back buffer of the linear project.
        Pass
        {
            Name "Tonemap"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = D(D3D9UV(i.uv));
                float la = SAMPLE_TEXTURE2D_LOD(_RfxSrc2, sampler_PointClamp, float2(0.5, 0.5), 0).x;
                float r0x = rcp(la + 9.99999975e-05) * _RfxTone.x;
                float4 c = SAMPLE_TEXTURE2D_LOD(_RfxSrc0, sampler_PointClamp, U(uv), 0);
                float a = -c.a >= 0 ? c.a : 1.0;
                float3 o;
                if (_RfxBright.w > 0.5)     // ReflexHdrSettings.alive
                {
                    // MX vs ATV Alive CopyBackBufferPS: x = colour x exposure per channel; (x + x^2 / white^2) / (1 + x) + bloom; ^(1 / 2.2)
                    // into the 8-bit target (Alive's programs square their albedo and light linearly; this pass is its display encode)
                    float3 x = c.rgb * r0x;
                    o = (x * x * _RfxTone.z + x) * rcp(x + 1.0) + SAMPLE_TEXTURE2D_LOD(_RfxSrc1, sampler_LinearClamp, U(uv), 0).rgb * _RfxTone.y;
                    o = saturate(pow(max(o, 0.0), 0.454545));
                }
                else
                {
                float L = dot(c.rgb, float3(0.300000012, 0.589999974, 0.109999999));
                float ls = r0x * L;
                float k = L * r0x + 1.0;
                float rl = rcp(L + 9.99999975e-05);
                k = rcp(k);
                float w = ls * _RfxTone.z + _RfxTone.w;
                ls = ls * w;
                k = k * ls;
                k = k * rl;
                float3 bloom = SAMPLE_TEXTURE2D_LOD(_RfxSrc1, sampler_LinearClamp, U(uv), 0).rgb * _RfxTone.y;
                bloom = k * bloom;
                o = saturate(c.rgb * k + bloom);
                }
                o = round(o * 255.0) / 255.0;                    // A8R8G8B8 target of the original
                // Alive's final pass (PostProcessMBColorLUTPS): the 8-bit display colour through the level's 32^3 colour grade
                // (coordinate = colour * g_LUTScaleOffset.xy + .zw, bilinear), into the 8-bit back buffer.
                if (_RfxBright.w > 0.5 && _RfxAliveLut.x > 0.5)
                    o = round(SAMPLE_TEXTURE3D_LOD(_RfxAliveLutTex, sampler_LinearClamp, o * _RfxAliveLut.y + _RfxAliveLut.z, 0).rgb * 255.0) / 255.0;
            #if !defined(UNITY_COLORSPACE_GAMMA)
                o = SRGBToLinear(o);
            #endif
                return float4(o, saturate(a));
            }
            ENDHLSL
        }
    }
    Fallback Off
}
