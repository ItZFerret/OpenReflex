// CINEMATIC EFFECTS (OPEN REFLEX, not part of the original renderer): the lens and camera effects of REPLAY and PHOTO MODE. Never drawn
// while riding (Scripts/Rendering/ReflexCinematic.cs only records these passes while the replay viewer owns the shot).
// HDR stage (before Reflex's tonemap, so bright lights keep their energy in the blur):
//   0 MotionBlur    gather along the per-pixel velocity (URP's motion vector texture: camera reprojection from depth + the vehicles' own
//                   vectors), depth-aware (a fast foreground smears over the background, not the other way round)
//   1 DofPre        full resolution -> half: colour (Karis weighted) and the signed circle of confusion (full resolution pixels)
//   2 DofGather     bokeh: every tap scatters its disc over the pixel (area weighted, so highlights keep their energy), a background tap
//                   is limited by the pixel's own blur (a sharp rider is not smeared by what is behind him); a near-field coverage alpha
//   3 DofComposite  the sharp frame where the circle of confusion is small, the gather where it is not (and wherever a near blur covers)
//   4 Accumulate    PHOTO MODE's progressive shutter: running mean of sub-frames
// Display stage (after the tonemap, linear display values):
//   5 FlareBright   the lights that are over the white point, 1/4 resolution (max pooled: a small lamp is not averaged away)
//   6 FlareGhosts   ghosts + halo (pseudo lens flare) with a chromatic split, a bright veil, all of it modulated by the lens dirt
//   7 LensFinal     chromatic aberration, flare + dirt veil, vignette, film grain
Shader "Hidden/Reflex/Cinematic"
{
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Color.hlsl"
    #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Random.hlsl"

    TEXTURE2D(_CineSrc0); TEXTURE2D(_CineSrc1); TEXTURE2D(_CineSrc2); TEXTURE2D(_CineSrc3); TEXTURE2D(_CineDirt);
    SamplerState sampler_LinearClamp, sampler_PointClamp, sampler_LinearRepeat;
    float4 _CineSize;    // the primary source: w, h, 1/w, 1/h
    float4 _CineFull;    // the camera target: w, h, 1/w, 1/h
    float4 _CineZ;       // LinearEyeDepth = 1 / (z * raw + w)
    float _CineDofZone;  // ft behind the focus plane kept fully sharp (ReflexCinematic.focusZone)
    float _CineDofZoneNear;  // ft in front of it (ReflexCinematic.focusZoneNear: the ground between the camera and the rider)
    float4 _CineDof;     // x focus distance (ft), y k (circle of confusion in full res pixels per (d - focus) / d), z max circle (full res pixels), w highlight weight
    float4 _CineMb;      // x velocity scale (shutter), y max length (uv), z soft depth (ft), w unused
    float4 _CineLens;    // x chromatic aberration, y vignette, z grain, w frame seed
    float4 _CineFlare;   // x flare amount, y dirt amount, z threshold, w ghost count
    float4 _CineAcc;     // x weight of the new frame
    float4 _CineView;    // x aspect, y tan(fov / 2), z far plane, w 1 when Src2 is the depth
    float4 _CineSun;     // xy the sun's uv, z the radius it counts as a light source in (uv, vertical), w 1 when the sun is in front of the camera

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
    float EyeDepth(float2 uv) { return 1.0 / (_CineZ.z * Pt(_CineSrc2, uv).r + _CineZ.w); }
    float Ign(float2 px, float seed) { return frac(52.9829189 * frac(dot(px + seed * 5.588238, float2(0.06711056, 0.00583715)))); }
    // the signed circle of confusion (full resolution pixels; negative in front of the focus plane) at an eye depth
    float Coc(float d)
    {
        float dz = d - _CineDof.x;
        dz = dz > 0.0 ? max(dz - _CineDofZone, 0.0) : min(dz + _CineDofZoneNear, 0.0);   // the sharp zone around the focus plane (the whole rider stays sharp)
        float c = _CineDof.y * dz / max(d, 0.01);
        return clamp(c, -_CineDof.z, _CineDof.z);
    }
    float2 ClampV(float2 v)
    {
        float l = length(v);
        return l > _CineMb.y ? v * (_CineMb.y / l) : v;
    }
    float SoftCover(float dist, float r) { return saturate((r - dist) / max(r * 0.35, 1e-5) + 0.5); }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off Blend Off

        // 0: motion blur. Src0 colour, Src1 motion vectors (uv, forward), Src2 depth
        Pass
        {
            Name "MotionBlur"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = i.uv;
                float3 c0 = Pt(_CineSrc0, uv).rgb;
                float2 v = ClampV(Pt(_CineSrc1, uv).xy * _CineMb.x);
                float len = length(v);
                if (length(v * _CineFull.xy) < 0.75) return float4(c0, 1);
                float z0 = EyeDepth(uv);
                float soft = max(_CineMb.z, z0 * 0.03);
                float jitter = Ign(uv * _CineFull.xy, _CineLens.w);
                const int N = 20;
                float3 acc = 0; float wsum = 0;
                [loop] for (int k = 0; k < N; k++)
                {
                    float t = (k + jitter) / N - 0.5;
                    float2 us = uv + v * t;
                    float dist = abs(t) * len;
                    float3 cs = Pt(_CineSrc0, us).rgb;
                    float zs = EyeDepth(us);
                    float2 vs = ClampV(Pt(_CineSrc1, us).xy * _CineMb.x);
                    float lenS = length(vs);
                    float fore = saturate(0.5 + (z0 - zs) / soft);     // the sample is in front of this pixel
                    float back = saturate(0.5 + (zs - z0) / soft);     // behind it
                    float w = fore * SoftCover(dist, 0.5 * lenS) + back * SoftCover(dist, 0.5 * len);
                    acc += cs * w; wsum += w;
                }
                return float4(wsum > 1e-4 ? acc / wsum : c0, 1);
            }
            ENDHLSL
        }

        // 1: half resolution colour + signed circle of confusion. Src0 colour, Src2 depth
        Pass
        {
            Name "DofPre"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 t = _CineFull.zw * 0.5;
                float3 col = 0; float ws = 0; float cMin = 1e6, cMax = -1e6;
                [unroll] for (int k = 0; k < 4; k++)
                {
                    float2 o = float2(k & 1 ? 1 : -1, k & 2 ? 1 : -1) * t;
                    float3 c = Lin(_CineSrc0, i.uv + o).rgb;
                    float w = 1.0 / (1.0 + Luma(c));
                    col += c * w; ws += w;
                    float cc = Coc(EyeDepth(i.uv + o));
                    cMin = min(cMin, cc); cMax = max(cMax, cc);
                }
                float coc = cMin < -0.75 ? cMin : cMax;     // the near field wins (a foreground edge over a far background)
                return float4(col / ws, coc);
            }
            ENDHLSL
        }

        // 2: the bokeh gather. Src0 = the half resolution colour + circle. Two layers: the far / focus layer (the pixel itself and every tap whose
        // disc covers it, area weighted) and the near-field layer (taps in front of the focus plane) which lies over it by its coverage.
        Pass
        {
            Name "DofGather"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 c0 = Pt(_CineSrc0, i.uv);
                float cp = c0.a;                                   // full resolution pixels
                float rp = abs(cp) * 0.5;                          // half resolution pixels
                float R = _CineDof.z * 0.5;                        // the widest disc, half resolution pixels
                float2 texel = _CineSize.zw;
                const int N = 96;
                float rot = Ign(i.uv * _CineSize.xy, _CineLens.w) * 6.2831853;
                float wc = (1.0 + _CineDof.w * Luma(c0.rgb)) / max(1.0, rp * rp);
                float3 accF = 0, accN = 0; float wF = 0, wN = 0, cover = 0;
                if (cp < -1.0) { accN = c0.rgb * wc; wN = wc; } else { accF = c0.rgb * wc; wF = wc; }
                [loop] for (int k = 0; k < N; k++)
                {
                    float r = sqrt((k + 0.5) / N) * R;
                    float a = k * 2.39996323 + rot;
                    float2 o = float2(cos(a), sin(a)) * r;
                    float4 s = Pt(_CineSrc0, i.uv + o * texel);
                    float rs = abs(s.a) * 0.5;
                    if (s.a > cp) rs = min(rs, rp * 2.0 + 0.5);    // behind this pixel: not wider than the pixel's own blur allows
                    float m = saturate(rs - r + 0.5);
                    float area = max(1.0, rs * rs);
                    float w = m / area * (1.0 + _CineDof.w * Luma(s.rgb));   // the highlights show as bokeh
                    if (s.a < -1.0) { accN += s.rgb * w; wN += w; cover += m / area; }
                    else { accF += s.rgb * w; wF += w; }
                }
                float3 far = wF > 1e-6 ? accF / wF : c0.rgb;
                float alpha = saturate(cover * R * R / N);
                float3 nearC = wN > 1e-6 ? accN / wN : far;
                return float4(lerp(far, nearC, alpha), alpha);
            }
            ENDHLSL
        }

        // 3: composite. Src0 sharp full resolution, Src1 the gather (half), Src2 depth
        Pass
        {
            Name "DofComposite"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float3 sharp = Pt(_CineSrc0, i.uv).rgb;
                float2 h = _CineSize.zw * 0.75;   // (the gather's noise: a 4 tap smoothing of the half resolution result, it is blurred anyway)
                float4 g = 0.25 * (Lin(_CineSrc1, i.uv + float2(h.x, h.y)) + Lin(_CineSrc1, i.uv + float2(-h.x, h.y)) + Lin(_CineSrc1, i.uv + float2(h.x, -h.y)) + Lin(_CineSrc1, i.uv + float2(-h.x, -h.y)));
                float coc = Coc(EyeDepth(i.uv));
                float blend = max(smoothstep(0.35, 1.6, abs(coc)), saturate(g.a * 8.0));   // (a pixel under a near blur takes the gather: it holds the near layer over the far one)
                return float4(lerp(sharp, g.rgb, blend), 1);
            }
            ENDHLSL
        }

        // 4: running mean of the sub-frames. Src0 the new frame, Src1 the mean so far
        Pass
        {
            Name "Accumulate"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float3 n = Pt(_CineSrc0, i.uv).rgb;
                float3 p = Pt(_CineSrc1, i.uv).rgb;
                return float4(lerp(p, n, _CineAcc.x), 1);
            }
            ENDHLSL
        }

        // 5: the light sources, 1/4 resolution (max pooled). Src0 display image, Src2 the depth. The original's tonemap clips (and its sky is an 8 bit
        // texture): a light is never "brighter than white", it is white and stands out of what is around it.
        //  - in the sky: the sun's disc, white in all three channels where the sky around it is blue (a cloud is as white as what is around it)
        //  - elsewhere: a lamp, a glint on chrome, a sign: at the white point and well over the brightest channel around it (not the sunlit
        //    ground, not a bright gap in the trees: not the sky, which has its own rule)
        Pass
        {
            Name "FlareBright"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float Peak(float3 c) { return max(c.r, max(c.g, c.b)); }
            float Floor(float3 c) { return min(c.r, min(c.g, c.b)); }
            float4 F(V i) : SV_Target
            {
                float2 t = _CineSize.zw;
                // the surroundings: a ring of 8 + 8 taps at 32 and 64 pixels (outside the sun's disc): the mean of the brightest and of the dimmest channel
                float aroundMax = 0, aroundMin = 0;
                [unroll] for (int r = 0; r < 8; r++)
                {
                    float a = r * 0.7853982;
                    float2 d = float2(cos(a), sin(a));
                    float3 c0 = Lin(_CineSrc0, i.uv + d * t * 32.0).rgb, c1 = Lin(_CineSrc0, i.uv + d * t * 64.0).rgb;
                    aroundMax += Peak(c0) + Peak(c1);
                    aroundMin += Floor(c0) + Floor(c1);
                }
                aroundMax /= 16.0; aroundMin /= 16.0;
                float3 best = 0; float bk = 0;
                [unroll] for (int k = 0; k < 4; k++)
                {
                    float2 uvk = i.uv + float2(k & 1 ? 1.5 : -1.5, k & 2 ? 1.5 : -1.5) * t;
                    float3 c = Lin(_CineSrc0, uvk).rgb;
                    bool sky = _CineView.w > 0.5 && EyeDepth(uvk) > _CineView.z * 0.97;
                    float kk = sky
                        ? saturate((Floor(c) - 0.95) / 0.05) * saturate((Floor(c) - aroundMin - 0.45) / 0.15) * 2.5
                        : saturate((Peak(c) - _CineFlare.z) / 0.25) * saturate((Peak(c) - aroundMax - 0.3) / 0.4);
                    if (kk > bk) { bk = kk; best = c / max(Peak(c), 1.0); }   // (the colour, not the energy: the flare's strength is the kk)
                }
                return float4(best * bk, 1);
            }
            ENDHLSL
        }

        // 6: ghosts, halo, an anamorphic streak and the dirty veil from the lights. Src0 the bright image (1/4)
        Pass
        {
            Name "FlareGhosts"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            // the ghosts' coatings: blue, green, amber, magenta, cyan
            float3 Tint(int k)
            {
                float3 t[5] = { float3(0.35, 0.55, 1.3), float3(0.5, 1.1, 0.55), float3(1.3, 0.75, 0.3), float3(1.0, 0.4, 0.8), float3(0.4, 0.8, 1.2) };
                return t[(k - 1) % 5];
            }
            // the bright image softened by a 12 tap disc (a light becomes a soft round disc about 28 pixels wide)
            float3 Soft(float2 uv)
            {
                float3 c = Lin(_CineSrc0, uv).rgb * 0.2;
                [unroll] for (int j = 0; j < 12; j++)
                {
                    float a = j * 2.39996323;
                    float r = sqrt((j + 0.5) / 12.0) * 3.5;
                    c += Lin(_CineSrc0, uv + float2(cos(a), sin(a)) * r * _CineSize.zw).rgb * (0.8 / 12.0);
                }
                return c;
            }
            float4 F(V i) : SV_Target
            {
                float2 uv = i.uv;
                float2 ghostVec = (0.5 - uv) * 0.55;
                float3 sum = 0;
                const int G = 5;
                [unroll] for (int k = 1; k <= G; k++)
                {
                    float2 o = frac(uv + ghostVec * k);
                    float fall = pow(saturate(1.0 - length(0.5 - o) / 0.7071), 3.0);
                    sum += Soft(o) * fall * Tint(k) * (1.6 / k);
                }
                // the halo ring
                float2 hv = normalize(ghostVec + 1e-5) * 0.42;
                float2 ho = frac(uv + hv);
                float hw = pow(saturate(1.0 - length(0.5 - ho) / 0.7071), 4.0);
                sum += Soft(ho) * hw * 0.8;
                // the anamorphic streak: the lights pulled sideways, blue
                float3 streak = 0;
                [unroll] for (int s = -8; s <= 8; s++)
                    streak += Lin(_CineSrc0, uv + float2(s * 2.0, 0) * _CineSize.zw).rgb * exp(-abs(s) * 0.32);
                sum += streak * float3(0.35, 0.55, 1.0) * 0.3;
                // the dirt catches the flare; the veil is the light itself, spread by a blur, through the dirt
                float dirt = Lin(_CineDirt, uv).r;
                float2 t = _CineSize.zw;
                float3 veil = 0;
                [unroll] for (int y = -2; y <= 2; y++) [unroll] for (int x = -2; x <= 2; x++)
                    veil += Lin(_CineSrc0, uv + float2(x, y) * t * 2.2).rgb * exp(-0.18 * (x * x + y * y));
                veil /= 9.0;
                float3 flare = sum * (0.5 + 1.2 * dirt * _CineFlare.y);
                float3 spread = veil * (0.12 + 1.4 * dirt) * _CineFlare.y;
                return float4(flare + spread, 1);
            }
            ENDHLSL
        }

        // 7: the display stage. Src0 the display image, Src1 the flare (1/4)
        Pass
        {
            Name "LensFinal"
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = i.uv;
                float2 p = uv - 0.5;
                float2 pa = float2(p.x * _CineView.x, p.y);
                float r2 = dot(pa, pa);
                // chromatic aberration: red and blue pulled apart along the radius, growing towards the corners
                float ca = _CineLens.x * 0.0042;
                float2 off = p * (ca * (0.35 + 1.4 * r2 * 2.0));
                float3 c;
                c.r = Lin(_CineSrc0, uv + off).r;
                c.g = Lin(_CineSrc0, uv).g;
                c.b = Lin(_CineSrc0, uv - off).b;
                float flareK = _CineFlare.x;
                if (flareK > 0.0) c += Lin(_CineSrc1, uv).rgb * flareK;
                // vignette (rounder than the frame: the corners lose about the setting's share)
                float vg = smoothstep(0.08, 0.62, r2);
                c *= 1.0 - _CineLens.y * 0.75 * vg;
                // film grain: luminance dependent (strongest in the mid-tones), a new pattern every frame
                if (_CineLens.z > 0.0)
                {
                    float2 px = uv * _CineFull.xy;
                    float n = (Ign(px, _CineLens.w) + Ign(px + 17.0, _CineLens.w * 1.37) - 1.0);   // triangular, -1..1
                    float l = saturate(Luma(c));
                    float amp = _CineLens.z * 0.085 * (0.35 + 0.65 * (1.0 - abs(2.0 * l - 1.0)));
                    float3 g = LinearToSRGB(max(c, 0.0));
                    g += n * amp;
                    c = SRGBToLinear(max(g, 0.0));
                }
                return float4(c, 1);
            }
            ENDHLSL
        }
    }
}
