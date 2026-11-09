// LIGHTING: ENHANCED (OPTIONS > VIDEO), not part of the original renderer: the modern passes ReflexRenderPipeline runs instead of / beside
// the original post chain (Shaders/ReflexPost.shader stays the ORIGINAL mode's, unchanged):
//   0 AvgDown4     4 x 4 box average (the luminance pyramid 64 -> 16 -> 4 meters the whole frame, not the original's top-left corner)
//   1 LumAverage   4 x 4 -> 1 x 1: exp(mean ln L) of all 16 texels (the original adaptation pass then steps towards it per game frame)
//   2 BloomPrefilter  13-tap downsample of the HDR scene with a soft threshold in exposed units
//   3 BloomDown    13-tap downsample (Jimenez 2014)
//   4 BloomUp      9-tap tent upsample added to the finer level
//   5 Tonemap      exposure (the original's key / adapted luminance), bloom, a filmic shoulder (no hard clip, highlights roll off to
//                  white), colour grading per level, vignette, dither; written for the sRGB back buffer like the original pass
//   6 Occlusion    screen-space ambient occlusion (r) + contact shadows towards the sun (g) from the depth pre-pass
//   7 OcclusionBlur  depth-aware 9-tap blur (drawn horizontally, then vertically)
Shader "Hidden/Reflex/EnhancedPost"
{
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Color.hlsl"
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Random.hlsl"

    TEXTURE2D(_RfxSrc0); TEXTURE2D(_RfxSrc1); TEXTURE2D(_RfxSrc2);
    SamplerState sampler_LinearClamp, sampler_PointClamp;
    float4 _RfxSrcSize;       // source w, h, 1/w, 1/h
    float4 _RfxTone;          // x key * 2, y bloom intensity, z exposure bias, w filmic shoulder start
    float4 _RfxGrade;         // x saturation, y contrast, z vignette, w dither
    float4 _RfxGradeGain;     // rgb white balance gain, w highlight desaturation (0 hue preserving .. 1 per channel)
    float4 _RfxGradeLift;     // rgb lift (added in display space), w slope of the curve's linear part (1.07: the original curve's mid-tones)
    float4 _RfxBloom;         // x threshold (exposed), y knee, z clamp, w key * 2 (exposure for the threshold)
    // occlusion
    float4 _RfxAO;            // x radius (m), y intensity, z max distance (m), w samples
    float4 _RfxContact;       // x length (m), y thickness (m), z max distance (m), w on
    float4 _RfxZParams;       // LinearEyeDepth: 1 / (z d + w)
    float4 _RfxViewTopLeft, _RfxViewXExtent, _RfxViewYExtent;   // camera rays at the near plane (world orientation), as URP's SSAO
    float4 _RfxCamPos;        // xyz camera position, w near plane
    float4 _RfxCamFwd;        // camera forward (world)
    float4 _RfxSunDirAO;      // towards the sun (world)
    float4x4 _RfxViewProj;    // GL projection * view (non-flipped): uv.y 0 = bottom, as the reconstruction
    float4 _RfxBlurDir;       // xy texel step, z depth sharpness
    float _RfxEnhDebugView;   // diagnostics: 1 shows the ambient occlusion, 2 the contact shadows (the final occlusion texture), 3 the reconstructed position (2 m checker), 4 the reconstructed normal's y
    TEXTURE2D(_RfxScreenOcclusion);
    static const float kBlur[4] = { 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };

    struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
    V Vert(uint id : SV_VertexID)
    {
        V o;
        o.positionCS = GetFullScreenTriangleVertexPosition(id);
        o.uv = GetFullScreenTriangleTexCoord(id);
        return o;
    }
    float4 Lin(TEXTURE2D(t), float2 uv) { return SAMPLE_TEXTURE2D_LOD(t, sampler_LinearClamp, uv, 0); }
    float4 Pt(TEXTURE2D(t), float2 uv) { return SAMPLE_TEXTURE2D_LOD(t, sampler_PointClamp, uv, 0); }
    float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

    // 13-tap downsample (Call of Duty: Advanced Warfare, Jimenez 2014) with the Karis average on the first level
    float3 Down13(float2 uv, float2 t, bool karis)
    {
        float3 a = Lin(_RfxSrc0, uv + t * float2(-2, -2)).rgb, b = Lin(_RfxSrc0, uv + t * float2(0, -2)).rgb, c = Lin(_RfxSrc0, uv + t * float2(2, -2)).rgb;
        float3 d = Lin(_RfxSrc0, uv + t * float2(-2, 0)).rgb, e = Lin(_RfxSrc0, uv).rgb, f = Lin(_RfxSrc0, uv + t * float2(2, 0)).rgb;
        float3 g = Lin(_RfxSrc0, uv + t * float2(-2, 2)).rgb, h = Lin(_RfxSrc0, uv + t * float2(0, 2)).rgb, i = Lin(_RfxSrc0, uv + t * float2(2, 2)).rgb;
        float3 j = Lin(_RfxSrc0, uv + t * float2(-1, -1)).rgb, k = Lin(_RfxSrc0, uv + t * float2(1, -1)).rgb;
        float3 l = Lin(_RfxSrc0, uv + t * float2(-1, 1)).rgb, m = Lin(_RfxSrc0, uv + t * float2(1, 1)).rgb;
        if (!karis)
            return e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
        float3 g0 = (j + k + l + m) * 0.25, g1 = (a + b + d + e) * 0.25, g2 = (b + c + e + f) * 0.25, g3 = (d + e + g + h) * 0.25, g4 = (e + f + h + i) * 0.25;
        float w0 = 1.0 / (1.0 + Luma(g0)), w1 = 1.0 / (1.0 + Luma(g1)), w2 = 1.0 / (1.0 + Luma(g2)), w3 = 1.0 / (1.0 + Luma(g3)), w4 = 1.0 / (1.0 + Luma(g4));
        return (g0 * w0 * 0.5 + (g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) * 0.125) / (w0 * 0.5 + (w1 + w2 + w3 + w4) * 0.125);
    }

    // filmic shoulder in the original's display-referred units: linear (slope m) below s, then an exponential roll-off to 1
    float Shoulder(float x, float s, float m)
    {
        float ys = m * s;
        return x < s ? x * m : 1.0 - (1.0 - ys) * exp(-m * (x - s) / (1.0 - ys));
    }
    float3 Shoulder3(float3 x, float s, float m) { return float3(Shoulder(x.r, s, m), Shoulder(x.g, s, m), Shoulder(x.b, s, m)); }

    float LinearDepth(float raw) { return 1.0 / (_RfxZParams.z * raw + _RfxZParams.w); }
    float3 WorldRay(float2 uv)
    {
        float2 q = float2(uv.x, 1.0 - uv.y);   // URP's SSAO: the corner rays start top-left
        return (_RfxViewTopLeft.xyz + _RfxViewXExtent.xyz * q.x + _RfxViewYExtent.xyz * q.y) / _RfxCamPos.w;
    }
    // world position of the pixel (eye depth along the forward axis)
    float3 WorldPos(float2 uv, float eyeDepth) { return _RfxCamPos.xyz + WorldRay(uv) * eyeDepth; }
    float EyeDepthAt(float2 uv) { return LinearDepth(Pt(_RfxSrc0, uv).r); }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off Blend Off

        // 0: 4 x 4 box average (ln luminance pyramid)
        Pass
        {
            Name "AvgDown4"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 t = _RfxSrcSize.zw;
                float s = 0;
                [unroll] for (int y = 0; y < 4; y++) [unroll] for (int x = 0; x < 4; x++)
                    s += Pt(_RfxSrc0, i.uv + t * float2(x - 1.5, y - 1.5)).r;
                return s / 16.0;
            }
            ENDHLSL
        }

        // 1: the frame's geometric mean luminance from the 4 x 4 ln image
        Pass
        {
            Name "LumAverage"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float s = 0;
                [unroll] for (int y = 0; y < 4; y++) [unroll] for (int x = 0; x < 4; x++)
                    s += Pt(_RfxSrc0, float2((x + 0.5) * 0.25, (y + 0.5) * 0.25)).r;
                return exp(s / 16.0);
            }
            ENDHLSL
        }

        // 2: bloom prefilter: 13-tap (Karis) downsample, soft threshold on the exposed luminance (s1: adapted luminance)
        Pass
        {
            Name "BloomPrefilter"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float3 c = min(Down13(i.uv, _RfxSrcSize.zw, true), _RfxBloom.z);
                float la = Pt(_RfxSrc1, float2(0.5, 0.5)).r;
                float e = _RfxBloom.w / (la + 1e-4);
                float l = Luma(c) * e;
                float k = _RfxBloom.y;
                float soft = clamp(l - _RfxBloom.x + k, 0.0, 2.0 * k);
                soft = soft * soft / (4.0 * k + 1e-5);
                float w = max(soft, l - _RfxBloom.x) / max(l, 1e-5);
                return float4(c * w, 1);
            }
            ENDHLSL
        }

        // 3: bloom downsample
        Pass
        {
            Name "BloomDown"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target { return float4(Down13(i.uv, _RfxSrcSize.zw, false), 1); }
            ENDHLSL
        }

        // 4: bloom upsample (9-tap tent of the coarser level s0) + the finer level s1
        Pass
        {
            Name "BloomUp"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 t = _RfxSrcSize.zw;
                float3 s = Lin(_RfxSrc0, i.uv).rgb * 4.0;
                s += (Lin(_RfxSrc0, i.uv + t * float2(-1, 0)).rgb + Lin(_RfxSrc0, i.uv + t * float2(1, 0)).rgb + Lin(_RfxSrc0, i.uv + t * float2(0, -1)).rgb + Lin(_RfxSrc0, i.uv + t * float2(0, 1)).rgb) * 2.0;
                s += Lin(_RfxSrc0, i.uv + t * float2(-1, -1)).rgb + Lin(_RfxSrc0, i.uv + t * float2(1, -1)).rgb + Lin(_RfxSrc0, i.uv + t * float2(-1, 1)).rgb + Lin(_RfxSrc0, i.uv + t * float2(1, 1)).rgb;
                return float4(s / 16.0 + Lin(_RfxSrc1, i.uv).rgb, 1);
            }
            ENDHLSL
        }

        // 5: tonemap and grade (s0 scene, s1 bloom, s2 adapted luminance)
        Pass
        {
            Name "Tonemap"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float la = Pt(_RfxSrc2, float2(0.5, 0.5)).r;
                float e = _RfxTone.x / (la + 1e-4) * _RfxTone.z;           // the original's exposure: key / adapted luminance
                float4 c = Pt(_RfxSrc0, i.uv);
                float3 x = max(c.rgb + Lin(_RfxSrc1, i.uv).rgb * _RfxTone.y, 0.0) * e;
                // white balance before the curve (the grade's gain), then the shoulder: hue preserving on luminance, mixed with the
                // per-channel curve that rolls saturated highlights off to white
                x *= _RfxGradeGain.rgb;
                float L = Luma(x);
                float3 yl = x * (Shoulder(L, _RfxTone.w, _RfxGradeLift.w) / max(L, 1e-5));
                float3 yc = Shoulder3(x, _RfxTone.w, _RfxGradeLift.w);
                float3 y = lerp(yl, yc, _RfxGradeGain.w);
                y = max(y, 0.0);
                // grade: saturation, contrast around mid grey (display units), lift
                float g = Luma(y);
                y = lerp(g.xxx, y, _RfxGrade.x);
                y = (y - 0.45) * _RfxGrade.y + 0.45;
                y = y + _RfxGradeLift.rgb * (1.0 - saturate(y));
                // vignette
                float2 v = i.uv * 2.0 - 1.0;
                y *= 1.0 - _RfxGrade.z * smoothstep(0.4, 1.6, dot(v, v));
                y = saturate(y);
                // dither (instead of the original's 8-bit rounding)
                float n = InterleavedGradientNoise(i.positionCS.xy, 0) - 0.5;
                y = saturate(y + n * _RfxGrade.w / 255.0);
            #if !defined(UNITY_COLORSPACE_GAMMA)
                y = SRGBToLinear(y);
            #endif
                float a = -c.a >= 0 ? c.a : 1.0;
                if (_RfxEnhDebugView > 0.5)
                {
                    float2 o = Lin(_RfxScreenOcclusion, i.uv).rg;
                    y = (_RfxEnhDebugView < 1.5 || _RfxEnhDebugView > 2.5 ? o.r : o.g).xxx;
                #if !defined(UNITY_COLORSPACE_GAMMA)
                    y = SRGBToLinear(y);
                #endif
                }
                return float4(y, saturate(a));
            }
            ENDHLSL
        }

        // 6: occlusion: r = ambient occlusion (hemisphere samples around the reconstructed position), g = contact shadow (a short
        // march towards the sun through the depth buffer). s0: the camera depth.
        Pass
        {
            Name "Occlusion"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            // Every depth read is at a depth texel's centre and its view ray goes through that same centre. The pass runs at half
            // resolution: its pixel centres fall exactly on the corner of four depth texels, where a point sample picks one of them by
            // rounding (row / column dependent) while the ray went through the corner. On terrain seen at a grazing angle (the
            // first-person camera) half a texel is metres of depth, so the reconstructed positions / normals tilted in whole rows and
            // columns and the AO drew them as long dark straight lines across the ground (w-darklines).
            float2 Snap(float2 uv) { return (floor(uv * _RfxSrcSize.xy) + 0.5) * _RfxSrcSize.zw; }
            float3 Pos(float2 uv) { uv = Snap(uv); return WorldPos(uv, EyeDepthAt(uv)); }
            // AO angle bias (HBAO style): occluders less than ~14 degrees above the tangent plane do not count, so the slight creases
            // between the terrain mesh's flat triangles (its per-pixel lit normal hides them) are not drawn as dark lines
            static const float kAoAngleBias = 0.25;
            float4 F(V i) : SV_Target
            {
                float2 uv0 = Snap(i.uv);
                float raw = Pt(_RfxSrc0, uv0).r;
                float z = LinearDepth(raw);
            #if UNITY_REVERSED_Z
                bool sky = raw <= 1e-7;
            #else
                bool sky = raw >= 1.0 - 1e-7;
            #endif
                if (sky || z > max(_RfxAO.z, _RfxContact.z)) return float4(1, 1, 0, 1);
                float3 P = WorldPos(uv0, z);
                // normal from the nearer of the two neighbours on each axis (no smearing across silhouettes); at the screen edge the
                // inner one (the clamped outer read would be the pixel itself: a zero edge, a NaN normal)
                float2 t = _RfxSrcSize.zw;
                float3 pr = Pos(uv0 + float2(t.x, 0)), pl = Pos(uv0 - float2(t.x, 0));
                float3 pu = Pos(uv0 + float2(0, t.y)), pd = Pos(uv0 - float2(0, t.y));
                float er = abs(dot(pr - P, _RfxCamFwd.xyz)), el = abs(dot(P - pl, _RfxCamFwd.xyz));
                float eu = abs(dot(pu - P, _RfxCamFwd.xyz)), ed = abs(dot(P - pd, _RfxCamFwd.xyz));
                if (uv0.x + t.x > 1.0) er = 1e30; if (uv0.x - t.x < 0.0) el = 1e30;
                if (uv0.y + t.y > 1.0) eu = 1e30; if (uv0.y - t.y < 0.0) ed = 1e30;
                float3 dx = er < el ? pr - P : P - pl;
                float3 dy = eu < ed ? pu - P : P - pd;
                float3 N = normalize(cross(dy, dx));
                if (dot(N, _RfxCamPos.xyz - P) < 0) N = -N;
                float noise = InterleavedGradientNoise(i.positionCS.xy, 0);
                float ao = 1.0;
                if (z < _RfxAO.z)
                {
                    // projected disc of samples (Alchemy AO): sum max(0, v.N - bias) / (v.v + eps), radius in metres
                    float r = _RfxAO.x;
                    int n = (int)_RfxAO.w;
                    float3 T = normalize(cross(N, abs(N.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0)));
                    float3 B = cross(N, T);
                    float acc = 0;
                    [loop] for (int k = 0; k < n; k++)
                    {
                        float a = (k + noise) * 2.39996323;                    // golden angle spiral
                        float rr = r * sqrt((k + 0.5) / n);
                        float h = r * (0.15 + 0.85 * frac(noise + k * 0.618034));
                        float3 S = P + (T * cos(a) + B * sin(a)) * rr + N * h * 0.5;
                        float4 clip = mul(_RfxViewProj, float4(S, 1));
                        if (clip.w <= 0) continue;
                        float2 suv = clip.xy / clip.w * 0.5 + 0.5;
                        if (any(suv < 0) || any(suv > 1)) continue;
                        float3 Q = Pos(suv);
                        float3 v = Q - P;
                        float vv = dot(v, v);
                        float occ = max(dot(v, N) - kAoAngleBias * sqrt(vv) - 0.02 * z * 0.05, 0) / (vv + 0.01);
                        occ *= saturate(1.0 - vv / (4.0 * r * r));              // range check: far occluders do not count
                        acc += occ;
                    }
                    ao = saturate(1.0 - _RfxAO.y * 2.0 * acc / n * r);
                    ao = lerp(ao, 1.0, saturate((z - _RfxAO.z * 0.7) / (_RfxAO.z * 0.3)));
                }
                float contact = 1.0;
                if (_RfxContact.w > 0.5 && z < _RfxContact.z && z > 1.5 && dot(N, _RfxSunDirAO.xyz) > 0)
                {
                    // contact shadow: march towards the sun; occluded when the depth buffer is in front of the ray by less than the thickness
                    float3 L = normalize(_RfxSunDirAO.xyz);
                    float len = _RfxContact.x * (1.0 + z * 0.02);
                    float3 S0 = P + N * (0.02 + z * 0.0015);
                    const int steps = 12;
                    float occ = 0;
                    [loop] for (int k = 1; k <= steps; k++)
                    {
                        float3 S = S0 + L * (len * (k - 1.0 + noise) / steps);
                        float4 clip = mul(_RfxViewProj, float4(S, 1));
                        if (clip.w <= 0) break;
                        float2 suv = clip.xy / clip.w * 0.5 + 0.5;
                        if (any(suv < 0) || any(suv > 1)) break;
                        float sz = dot(S - _RfxCamPos.xyz, _RfxCamFwd.xyz);
                        float dz = sz - EyeDepthAt(Snap(suv));
                        float thick = _RfxContact.y * (1.0 + sz * 0.01);
                        if (dz > 0.02 && dz < thick) { occ = 1.0 - (k - 1.0) / steps * 0.5; break; }
                    }
                    contact = 1.0 - occ * (1.0 - saturate((z - _RfxContact.z * 0.6) / (_RfxContact.z * 0.4)));
                }
                // diagnostics: 3 = a 2 m world checker of the reconstructed position, 4 = the reconstructed normal's y
                if (_RfxEnhDebugView > 3.5) return float4(N.y, N.y, z, 1);
                if (_RfxEnhDebugView > 2.5) { float2 c = step(0.5, frac(P.xz / 2.0)); float k = abs(c.x - c.y) * 0.6 + 0.2 + 0.2 * step(0.5, frac(P.y)); return float4(k, k, z, 1); }
                return float4(ao, contact, z, 1);
            }
            ENDHLSL
        }

        // 7: depth-aware blur (s0: occlusion rgba, b holds the eye depth)
        Pass
        {
            Name "OcclusionBlur"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 c = Pt(_RfxSrc0, i.uv);
                float z = c.b;
                float2 sum = c.rg * 0.2270270270; float w = 0.2270270270;
                [unroll] for (int j = 1; j <= 4; j++)
                {
                    float4 a = Pt(_RfxSrc0, i.uv + _RfxBlurDir.xy * j);
                    float4 b = Pt(_RfxSrc0, i.uv - _RfxBlurDir.xy * j);
                    float wa = kBlur[j - 1] * saturate(1.0 - abs(a.b - z) * _RfxBlurDir.z / max(z, 1e-3));
                    float wb = kBlur[j - 1] * saturate(1.0 - abs(b.b - z) * _RfxBlurDir.z / max(z, 1e-3));
                    sum += a.rg * wa + b.rg * wb; w += wa + wb;
                }
                return float4(sum / w, z, 1);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
