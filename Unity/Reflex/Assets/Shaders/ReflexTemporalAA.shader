// ANTI-ALIASING: TAA (OPTIONS > VIDEO, LIGHTING ENHANCED only; Scripts/Rendering/ReflexTemporalAA.cs). Not part of the original renderer.
//   0 Resolve   the jittered HDR scene -> history (Reinhard-compressed, SV_Target0) + the linear HDR image the post chain continues with
//               (SV_Target1), both at the output size (larger than the input when upscaling)
//   1 Sharpen   contrast-adaptive sharpening (AMD FidelityFX CAS) of the tonemapped image
// uv conventions: the fullscreen triangle's uv is the textures' uv; URP's motion vectors are uv deltas (current - previous) in that space; the
// jitter moved every scene sample by +_TaaJitter.xy (uv), so input texel t shows the unjittered image at its centre - jitter.
Shader "Hidden/Reflex/TemporalAA"
{
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"

    TEXTURE2D(_TaaColor); TEXTURE2D(_TaaDepth); TEXTURE2D(_TaaMotion); TEXTURE2D(_TaaHistory); TEXTURE2D(_TaaPrevMotion);
    SamplerState sampler_LinearClamp, sampler_PointClamp;
    float4 _TaaInSize, _TaaOutSize;   // w, h, 1/w, 1/h
    float4 _TaaJitter;                // xy this frame's jitter (input uv), zw its change since the last frame (in this frame's motion vectors)
    float4 _TaaParams;                // x base blend, y history valid, z kernel sharpness, w last frame's motion vectors valid
    float4 _TaaParams2;               // xy last frame's jitter change (in its motion vectors), z clip gamma still, w clip gamma moving
    float4 _TaaSharpen;               // x sharpness 0..1

    struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
    V Vert(uint id : SV_VertexID)
    {
        V o;
        o.positionCS = GetFullScreenTriangleVertexPosition(id);
        o.uv = GetFullScreenTriangleTexCoord(id);
        return o;
    }

    float Max3(float3 c) { return max(c.r, max(c.g, c.b)); }
    // NaN / Inf by the bits (the compilers drop isnan() under their float rules)
    bool AnyBad(float3 c) { uint3 b = asuint(c) & 0x7fffffffu; return any(b >= 0x7f800000u); }
    // Reinhard on the largest channel (hue kept, invertible): the HDR scene's fireflies cannot dominate the filters
    float3 ToT(float3 c) { c = max(c, 0.0); return c * rcp(1.0 + Max3(c)); }
    float3 FromT(float3 t) { return t * rcp(max(1.0 - Max3(t), 1e-4)); }
    float3 ToYCoCg(float3 c) { return float3(dot(c, float3(0.25, 0.5, 0.25)), dot(c, float3(0.5, 0.0, -0.5)), dot(c, float3(-0.25, 0.5, -0.25))); }
    float3 FromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

    float3 HistoryTap(float2 uv) { return SAMPLE_TEXTURE2D_LOD(_TaaHistory, sampler_LinearClamp, uv, 0).rgb; }
    // 5-tap Catmull-Rom (bilinear taps, the corners dropped): a sharp re-projection that does not blur the history frame after frame
    float3 HistoryCatmullRom(float2 uv)
    {
        float2 pos = uv * _TaaOutSize.xy;
        float2 t1 = floor(pos - 0.5) + 0.5;
        float2 f = pos - t1;
        float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
        float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
        float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
        float2 w3 = f * f * (-0.5 + 0.5 * f);
        float2 w12 = w1 + w2;
        float2 t0 = (t1 - 1.0) * _TaaOutSize.zw, t3 = (t1 + 2.0) * _TaaOutSize.zw, t12 = (t1 + w2 / w12) * _TaaOutSize.zw;
        float a = w12.x * w0.y, b = w0.x * w12.y, c = w12.x * w12.y, d = w3.x * w12.y, e = w12.x * w3.y;
        float3 s = HistoryTap(float2(t12.x, t0.y)) * a + HistoryTap(float2(t0.x, t12.y)) * b + HistoryTap(t12) * c
                 + HistoryTap(float2(t3.x, t12.y)) * d + HistoryTap(float2(t12.x, t3.y)) * e;
        return max(s * rcp(a + b + c + d + e), 0.0);
    }

    // clip q towards the centre of the box (Playdead's INSIDE TAA)
    float3 ClipToBox(float3 bmin, float3 bmax, float3 q)
    {
        float3 c = 0.5 * (bmax + bmin), e = 0.5 * (bmax - bmin) + 1e-5;
        float3 v = q - c, u = abs(v / e);
        float m = max(u.x, max(u.y, u.z));
        return m > 1.0 ? c + v / m : q;
    }

    struct ResolveOut { float4 history : SV_Target0; float4 color : SV_Target1; };

    ResolveOut FragResolve(V i)
    {
        float2 uv = i.uv;
        int2 maxT = int2(_TaaInSize.xy) - 1;
        float2 jpx = _TaaJitter.xy * _TaaInSize.xy;
        float2 q = uv * _TaaInSize.xy;            // the output pixel's centre in input pixels
        int2 t0 = int2(floor(q + jpx));            // the input sample whose true position is nearest

        float3 sum = 0.0, m1 = 0.0, m2 = 0.0, mn = 1e5, mx = -1e5;
        float wsum = 0.0, wmax = 0.0;
    #if UNITY_REVERSED_Z
        float best = -1.0;
    #else
        float best = 2.0;
    #endif
        int2 bestT = clamp(t0, 0, maxT);
        [unroll] for (int y = -1; y <= 1; y++)
        [unroll] for (int x = -1; x <= 1; x++)
        {
            int2 tu = t0 + int2(x, y);
            int2 t = clamp(tu, 0, maxT);
            float3 raw = LOAD_TEXTURE2D(_TaaColor, t).rgb;
            raw = AnyBad(raw) ? 0.0 : min(raw, 65000.0);
            float3 c = ToT(raw);
            float2 dd = (float2(tu) + 0.5 - jpx) - q;
            float w = exp(-_TaaParams.z * dot(dd, dd));
            sum += c * w; wsum += w; wmax = max(wmax, w);
            float3 yc = ToYCoCg(c);
            m1 += yc; m2 += yc * yc; mn = min(mn, yc); mx = max(mx, yc);
            float z = LOAD_TEXTURE2D(_TaaDepth, t).r;
        #if UNITY_REVERSED_Z
            if (z > best) { best = z; bestT = t; }
        #else
            if (z < best) { best = z; bestT = t; }
        #endif
        }
        float3 cur = sum / max(wsum, 1e-5);

        // the closest surface's motion (dilated: a moving edge keeps its own motion), less the jitter's change
        float2 mv = LOAD_TEXTURE2D(_TaaMotion, bestT).xy - _TaaJitter.zw;
        float2 prevUv = uv - mv;
        float speed = length(mv * _TaaOutSize.xy);   // output pixels per frame

        float alpha = lerp(_TaaParams.x, _TaaParams.x * 2.5, saturate(speed / 40.0));   // fast motion: less re-sampled history (no smear of the streaming ground)
        alpha *= lerp(0.45, 1.3, wmax);              // a sample right at the pixel counts more (the upscaler's confidence)

        // disocclusion: last frame, no surface around the re-projected point moved like this one (the ground revealed behind a bike)
        float disocc = 0.0;
        if (_TaaParams.w > 0.5)
        {
            float2 pq = prevUv * _TaaInSize.xy - 0.5;
            int2 p0 = int2(floor(pq));
            float bestDv = 1e5;
            [unroll] for (int py = 0; py <= 1; py++)
            [unroll] for (int px = 0; px <= 1; px++)
            {
                float2 pmv = LOAD_TEXTURE2D(_TaaPrevMotion, clamp(p0 + int2(px, py), 0, maxT)).xy - _TaaParams2.xy;
                bestDv = min(bestDv, length((mv - pmv) * _TaaOutSize.xy));
            }
            disocc = saturate((bestDv - (1.0 + 0.15 * speed)) * 0.35);
        }

        // variance clipping (YCoCg of the compressed colour), tighter while moving
        float3 mean = m1 / 9.0, sigma = sqrt(max(m2 / 9.0 - mean * mean, 0.0));
        float g = lerp(_TaaParams2.z, _TaaParams2.w, saturate(speed / 16.0)) * (1.0 - 0.5 * disocc);
        float3 bmin = max(mn, mean - g * sigma), bmax = min(mx, mean + g * sigma);
        float3 hist = HistoryCatmullRom(prevUv);
        hist = FromYCoCg(ClipToBox(bmin, bmax, ToYCoCg(hist)));

        alpha = max(alpha, disocc * 0.6);
        bool outside = any(prevUv < 0.0) || any(prevUv > 1.0);
        if (outside || _TaaParams.y < 0.5) alpha = 1.0;
        alpha = saturate(alpha);
        float3 r = lerp(hist, cur, alpha);
        r = AnyBad(r) ? cur : r;
        r = min(r, 0.9999);   // the compressed range is [0, 1)

        ResolveOut o;
        o.history = float4(r, 1.0);
        o.color = float4(FromT(r), 1.0);
        return o;
    }

    // AMD FidelityFX CAS (sharpen only, no scaling)
    float4 FragSharpen(V i) : SV_Target
    {
        int2 p = int2(i.uv * _TaaInSize.xy);
        int2 mxp = int2(_TaaInSize.xy) - 1;
        #define CAS_TAP(dx, dy) LOAD_TEXTURE2D(_TaaColor, clamp(p + int2(dx, dy), 0, mxp)).rgb
        float4 ec = LOAD_TEXTURE2D(_TaaColor, p);
        float3 a = CAS_TAP(-1, -1), b = CAS_TAP(0, -1), c = CAS_TAP(1, -1);
        float3 d = CAS_TAP(-1, 0), e = ec.rgb, f = CAS_TAP(1, 0);
        float3 g = CAS_TAP(-1, 1), h = CAS_TAP(0, 1), k = CAS_TAP(1, 1);
        #undef CAS_TAP
        float3 mnC = min(min(min(d, e), min(f, b)), h);
        float3 mnA = min(mnC, min(min(a, c), min(g, k)));
        mnC += mnA;
        float3 mxC = max(max(max(d, e), max(f, b)), h);
        float3 mxA = max(mxC, max(max(a, c), max(g, k)));
        mxC += mxA;
        float3 amp = saturate(min(mnC, 2.0 - mxC) * rcp(max(mxC, 1e-5)));
        amp = sqrt(amp);
        float peak = -rcp(lerp(8.0, 5.0, saturate(_TaaSharpen.x)));
        float3 w = amp * peak;
        float3 rw = rcp(1.0 + 4.0 * w);
        float3 o = saturate((b * w + d * w + f * w + h * w + e) * rw);
        return float4(o, ec.a);
    }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off Blend Off

        Pass
        {
            Name "Resolve"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment FragResolve
            ENDHLSL
        }

        Pass
        {
            Name "Sharpen"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment FragSharpen
            ENDHLSL
        }
    }
}
