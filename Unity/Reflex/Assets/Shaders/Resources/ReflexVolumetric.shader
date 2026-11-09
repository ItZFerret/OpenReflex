// LIGHTING: ENHANCED > VOLUMETRIC LIGHT (OPTIONS > VIDEO; Scripts/Rendering/ReflexVolumetricLight.cs). Not part of the original renderer:
// the original fakes its light shafts with additive cards and cones (the stadiums' SX_*_LightBeam cones, the god-ray / light-ray cards,
// the spot-light trailers' beams); in ENHANCED they are hidden and the light is scattered by a participating medium instead:
//   0 March      per low-resolution pixel a ray march from the camera to the scene depth (at most the sun's shadow distance) through a
//                height fog + ground dust medium; in-scattering of the level's sun (Henyey-Greenstein, shadowed by Unity's cascaded sun
//                shadow map and the level's own volume shadow map: shafts through trees, grandstands and roof openings) and of the
//                stadium spot lights placed where the fake beams were (analytic per-light segments, no shadows). Start offsets from a
//                64 x 64 blue-noise tile rotated by the golden ratio every frame. rgb = in-scattered light (raw HDR), a = transmittance.
//   1 Temporal   reprojects last frame's result to this frame by the pixel's scene position, clamps it to this frame's 3 x 3
//                neighbourhood and blends (the march's noise and the jitter average out over ~10 frames; no ghosting on motion)
//   2 Composite  depth-aware upsample to full resolution, blended onto the HDR scene before the transparent objects:
//                colour = scene x transmittance + in-scattered (Blend One SrcAlpha). The temporal result is then published as
//                _RfxVolResult: the transparent objects' shaders take the medium in front of them from it (ReflexVolumetricApply.hlsl)
// Works in the raw HDR values of ReflexRenderPipeline's target (the ENHANCED post chain exposes and tone maps the result).
Shader "Hidden/Reflex/Volumetric"
{
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
    #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"
    #include "../ReflexCommon.hlsl"

    TEXTURE2D(_RfxVolDepth);        // the camera depth (full resolution)
    TEXTURE2D(_RfxVolSrc);          // march / history input
    TEXTURE2D(_RfxVolHistory);      // last frame's result
    TEXTURE2D(_RfxVolNoise);        // 64 x 64 blue noise (R8)
    TEXTURE2D(_RfxVolGround);       // the terrain's height (world y) over its rectangle (R32F)
    // (sampler_PointClamp / LinearClamp / PointRepeat: URP's GlobalSamplers.hlsl)

    float4 _RfxVolFull;             // full-resolution w, h, 1/w, 1/h
    float4 _RfxVolLow;              // low-resolution w, h, 1/w, 1/h
    float4 _RfxVolParams;           // x scale (2 / 4), y march steps, z spot samples, w frame index
    float4 _RfxVolZParams;          // LinearEyeDepth: 1 / (z d + w)
    float4 _RfxVolTopLeft, _RfxVolXExtent, _RfxVolYExtent;   // camera rays at the near plane (world orientation)
    float4 _RfxVolCam;              // xyz camera position, w near plane
    float4x4 _RfxVolPrevVP;         // last frame's GL view-projection (uv.y 0 = bottom, as the rays)
    float4 _RfxVolMedium;           // x scattering at the base height (1/m), y 1 / height falloff (1/m), z base height (world y), w max distance (m)
    float4 _RfxVolMedium2;          // x extinction / scattering, y far fade start (fraction of max), z anisotropy g (sun), w forward lobe weight
    float4 _RfxVolDust;             // x dust scattering (1/m), y 1 / dust height (1/m), z wind phase, w ground texture bound
    float4 _RfxVolGroundRect;       // xz min, 1 / size (x, z)
    float4 _RfxVolSun;              // rgb sun in-scattering colour (sun colour x level scale), w sun on
    float4 _RfxVolAmbient;          // rgb ambient in-scattering colour, w unused
    float4 _RfxVolShadow;           // x Unity sun shadow map bound, y cascades, z the level's volume shadow, w shadow distance (m)
    float4 _RfxVolTemporal;         // x history weight (0: reset), y clamp expansion
    // spot lights (the stadium beams): virtual apex + range, direction + cos outer, colour x intensity + cos inner, x r0, y anisotropy
    #define RFX_VOL_MAX_SPOTS 24
    float4 _RfxVolSpotPos[RFX_VOL_MAX_SPOTS];
    float4 _RfxVolSpotDir[RFX_VOL_MAX_SPOTS];
    float4 _RfxVolSpotCol[RFX_VOL_MAX_SPOTS];
    float4 _RfxVolSpotCount;        // x count, y medium boost for the spots, z anisotropy g, w the ground dust's share for the spots
    float _RfxVolDebug;             // tests: 1 the in-scattered light alone, 2 the transmittance alone

    struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
    V Vert(uint id : SV_VertexID)
    {
        V o;
        o.positionCS = GetFullScreenTriangleVertexPosition(id);
        o.uv = GetFullScreenTriangleTexCoord(id);
        return o;
    }

    float EyeDepth(float raw) { return 1.0 / (_RfxVolZParams.z * raw + _RfxVolZParams.w); }
    bool IsSky(float raw)
    {
    #if UNITY_REVERSED_Z
        return raw <= 1e-7;
    #else
        return raw >= 1.0 - 1e-7;
    #endif
    }
    // the view ray through uv (its forward component is 1: position = camera + ray x eye depth)
    float3 Ray(float2 uv)
    {
        float2 q = float2(uv.x, 1.0 - uv.y);
        return (_RfxVolTopLeft.xyz + _RfxVolXExtent.xyz * q.x + _RfxVolYExtent.xyz * q.y) / _RfxVolCam.w;
    }
    // the full-resolution texel that stands for low-resolution pixel k (the top-left of its block): the march, the history and the
    // upsample all read the depth there
    float2 FullUV(float2 k) { return (k * _RfxVolParams.x + 0.5) * _RfxVolFull.zw; }
    float RawAt(float2 uvF) { return SAMPLE_TEXTURE2D_LOD(_RfxVolDepth, sampler_PointClamp, uvF, 0).r; }

    float HG(float g, float c) { float g2 = g * g; return (1.0 - g2) / (12.5663706 * pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5)); }

    // cheap value noise (the dust's slow billows)
    float Hash3(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
    float Noise3(float3 x)
    {
        float3 i = floor(x), f = frac(x); f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(Hash3(i), Hash3(i + float3(1, 0, 0)), f.x), lerp(Hash3(i + float3(0, 1, 0)), Hash3(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(Hash3(i + float3(0, 0, 1)), Hash3(i + float3(1, 0, 1)), f.x), lerp(Hash3(i + float3(0, 1, 1)), Hash3(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }

    float GroundY(float3 p)
    {
        if (_RfxVolDust.w < 0.5) return _RfxVolMedium.z;
        float2 uv = (p.xz - _RfxVolGroundRect.xy) * _RfxVolGroundRect.zw;
        return SAMPLE_TEXTURE2D_LOD(_RfxVolGround, sampler_LinearClamp, uv, 0).r;
    }

    // scattering coefficient (1/m) at p: height fog over the level's base height + ground dust; fades out towards the march's far end
    // (billows: the slow noise on the dust; the spots' samples skip it)
    float Density(float3 p, float t, float dustW, bool billows)
    {
        float h = p.y - _RfxVolMedium.z;
        float d = _RfxVolMedium.x * exp(-clamp(h, -20.0, 400.0) * _RfxVolMedium.y);
        if (_RfxVolDust.x > 0.0 && dustW > 0.0 && h < 40.0)   // (the ground under the camera +- the terrain's relief near the track)
        {
            float hg = p.y - GroundY(p);
            float dust = exp(-max(hg, 0.0) * _RfxVolDust.y);
            if (dust > 0.02)
            {
                float n = billows ? Noise3(p * 0.09 + float3(_RfxVolDust.z, 0.0, _RfxVolDust.z * 0.6)) : 0.5;
                d += _RfxVolDust.x * dust * (0.4 + n) * dustW;
            }
        }
        float fade = saturate((1.0 - t / _RfxVolMedium.w) / max(1.0 - _RfxVolMedium2.y, 1e-3));
        return d * fade;
    }

    // the sun's visibility at p: Unity's cascaded shadow map (the scenery, the trees, the bikes) and the level's volume shadow map (hills)
    float SunShadow(float3 p)
    {
        float s = 1.0;
        if (_RfxVolShadow.x > 0.5)
        {
            int c = _RfxVolShadow.y > 1.5 ? (int)ComputeCascadeIndex(p) : 0;
            if (c < (int)_RfxVolShadow.y && distance(p, _RfxVolCam.xyz) < _RfxVolShadow.w)
            {
                float3 sc = mul(_MainLightWorldToShadow[c], float4(p, 1.0)).xyz;
                if (all(sc.xy > 0.0) && all(sc.xy < 1.0))
                {
                    float v = SAMPLE_TEXTURE2D_SHADOW(_MainLightShadowmapTexture, sampler_LinearClampCompare, sc);
                    s = lerp(1.0, v, _MainLightShadowParams.x);
                }
            }
        }
        if (_RfxVolShadow.z > 0.5) s = min(s, RfxVolumeShadow(p));
        return s;
    }

    float Noise(float2 pix, float salt)
    {
        float n = SAMPLE_TEXTURE2D_LOD(_RfxVolNoise, sampler_PointRepeat, (pix + 0.5) / 64.0 + salt * float2(0.4142, 0.7320), 0).r;
        return frac(n + _RfxVolParams.w * 0.61803398875 + salt * 0.5);
    }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        ZTest Always ZWrite Off Cull Off

        // 0: march (low resolution)
        Pass
        {
            Name "March"
            Blend Off
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 k = floor(i.uv * _RfxVolLow.xy);
                float2 uvF = FullUV(k);
                float raw = RawAt(uvF);
                float3 ray = Ray(uvF);
                float rayLen = length(ray);
                float3 dir = ray / rayLen;
                float sceneDist = IsSky(raw) ? 1e6 : EyeDepth(raw) * rayLen;
                float tMax = min(sceneDist, _RfxVolMedium.w);
                float3 cam = _RfxVolCam.xyz;

                int n = (int)_RfxVolParams.y;
                float j = Noise(k, 0.0);
                float cosSun = dot(dir, _RfxSunDir.xyz);
                float g = _RfxVolMedium2.z;
                float phaseSun = lerp(HG(0.0, cosSun), HG(g, cosSun), _RfxVolMedium2.w);
                float ext = _RfxVolMedium2.x;
                float3 L = 0.0; float T = 1.0; float depthOptical = 0.0;
                float invN = 1.0 / n;
                [loop] for (int s = 0; s < n; s++)
                {
                    // quadratic spacing (more samples near the camera, where shafts are sharp); jittered inside each cell
                    float x = (s + j) * invN;
                    float t = tMax * x * x;
                    float dt = tMax * (2.0 * s + 1.0) * invN * invN;
                    float3 p = cam + dir * t;
                    float sigma = Density(p, t, 1.0, true);
                    if (sigma <= 0.0) continue;
                    float3 S = _RfxVolAmbient.rgb;
                    if (_RfxVolSun.w > 0.5) S += _RfxVolSun.rgb * (phaseSun * SunShadow(p));
                    S *= sigma;
                    float st = sigma * ext;
                    float Ts = exp(-st * dt);
                    L += T * S * (1.0 - Ts) / st;   // energy-conserving step (Hillaire 2015)
                    T *= Ts; depthOptical += st * dt;
                }

                // the stadium spot lights: per light the ray's part inside its cone's bounding cylinder, sampled on its own
                int ns = (int)_RfxVolSpotCount.x;
                if (ns > 0)
                {
                    float avgExt = tMax > 0.0 ? depthOptical / tMax : 0.0;
                    int m = (int)_RfxVolParams.z;
                    float j2 = Noise(k, 1.0);
                    float gs = _RfxVolSpotCount.z;
                    [loop] for (int li = 0; li < ns; li++)
                    {
                        float4 A = _RfxVolSpotPos[li], D = _RfxVolSpotDir[li], C = _RfxVolSpotCol[li];
                        float range = A.w, cosOut = D.w, cosIn = C.w;
                        float tanOut = sqrt(max(1.0 - cosOut * cosOut, 0.0)) / max(cosOut, 1e-3);
                        float R = range * tanOut;
                        float3 co = cam - A.xyz;
                        float dd = dot(dir, D.xyz), cd = dot(co, D.xyz);
                        float3 w = dir - D.xyz * dd, u = co - D.xyz * cd;
                        float a = dot(w, w), b = 2.0 * dot(u, w), c = dot(u, u) - R * R;
                        float t0, t1;
                        if (a < 1e-6) { if (c > 0.0) continue; t0 = 0.0; t1 = sceneDist; }
                        else
                        {
                            float disc = b * b - 4.0 * a * c;
                            if (disc <= 0.0) continue;
                            float sq = sqrt(disc);
                            t0 = (-b - sq) / (2.0 * a); t1 = (-b + sq) / (2.0 * a);
                        }
                        // the slab 0 <= axis distance <= range
                        if (abs(dd) > 1e-5)
                        {
                            float s0 = (0.0 - cd) / dd, s1 = (range - cd) / dd;
                            t0 = max(t0, min(s0, s1)); t1 = min(t1, max(s0, s1));
                        }
                        else if (cd < 0.0 || cd > range) continue;
                        t0 = max(t0, 0.0); t1 = min(t1, sceneDist);
                        if (t1 <= t0) continue;
                        float seg = (t1 - t0) / m;
                        float3 acc = 0.0;
                        [loop] for (int q = 0; q < m; q++)
                        {
                            float t = t0 + (q + j2) * seg;
                            float3 p = cam + dir * t;
                            float3 v = p - A.xyz;
                            float d2 = dot(v, v);
                            float dist = sqrt(d2);
                            float cosA = dot(v, D.xyz) / max(dist, 1e-4);
                            float cone = smoothstep(cosOut, cosIn, cosA);
                            if (cone <= 0.0) continue;
                            float along = dot(v, D.xyz) / range;
                            float fade = saturate(1.0 - along * along * along * along); fade *= fade;
                            float sigma = Density(p, min(t, _RfxVolMedium.w * 0.5), _RfxVolSpotCount.w, false) * _RfxVolSpotCount.y;
                            acc += cone * cone * fade / max(d2, 1e-2) * HG(gs, -dot(dir, v / max(dist, 1e-4))) * sigma * exp(-avgExt * t);
                        }
                        L += C.rgb * acc * seg;
                    }
                }
                L = max(L, 0.0);
                // a soft limit on the added light (raw HDR): the glow around the sun and the beams' cores roll off instead of burning the picture white
                float lum = dot(L, float3(0.2126, 0.7152, 0.0722));
                L *= 1.0 / (1.0 + lum / 1.2);
                if (any(isnan(L)) || isnan(T)) { L = 0.0; T = 1.0; }
                return float4(L, saturate(T));
            }
            ENDHLSL
        }

        // 1: temporal accumulation (low resolution)
        Pass
        {
            Name "Temporal"
            Blend Off
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 k = floor(i.uv * _RfxVolLow.xy);
                float2 uvL = (k + 0.5) * _RfxVolLow.zw;
                float4 cur = SAMPLE_TEXTURE2D_LOD(_RfxVolSrc, sampler_PointClamp, uvL, 0);
                if (_RfxVolTemporal.x <= 0.0) return cur;
                float2 uvF = FullUV(k);
                float raw = RawAt(uvF);
                float eye = IsSky(raw) ? 1e5 : EyeDepth(raw);
                float3 P = _RfxVolCam.xyz + Ray(uvF) * eye;
                float4 clip = mul(_RfxVolPrevVP, float4(P, 1.0));
                if (clip.w <= 1e-4) return cur;
                float2 prev = clip.xy / clip.w * 0.5 + 0.5;
                if (any(prev < 0.0) || any(prev > 1.0)) return cur;
                // a low-resolution pixel stands for its block's top-left full-resolution texel (FullUV): the history pixel of that texel
                float2 kp = (prev * _RfxVolFull.xy - 0.5) / _RfxVolParams.x;
                float4 hist = SAMPLE_TEXTURE2D_LOD(_RfxVolHistory, sampler_LinearClamp, (kp + 0.5) * _RfxVolLow.zw, 0);
                // 3 x 3 neighbourhood box of this frame (the history may not leave it: no trails when the shafts move)
                float4 mn = cur, mx = cur, m1 = 0, m2 = 0;
                [unroll] for (int y = -1; y <= 1; y++)
                    [unroll] for (int x = -1; x <= 1; x++)
                    {
                        float4 c = SAMPLE_TEXTURE2D_LOD(_RfxVolSrc, sampler_PointClamp, uvL + float2(x, y) * _RfxVolLow.zw, 0);
                        mn = min(mn, c); mx = max(mx, c); m1 += c; m2 += c * c;
                    }
                m1 /= 9.0; m2 /= 9.0;
                float4 sd = sqrt(max(m2 - m1 * m1, 0.0));
                float4 lo = max(mn, m1 - sd * _RfxVolTemporal.y), hi = min(mx, m1 + sd * _RfxVolTemporal.y);
                hist = clamp(hist, lo, hi);
                return lerp(cur, hist, _RfxVolTemporal.x);
            }
            ENDHLSL
        }

        // 2: depth-aware upsample, composited onto the HDR scene
        Pass
        {
            Name "Composite"
            Blend One SrcAlpha
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float2 uv = i.uv;
                float raw = RawAt(uv);
                float z = IsSky(raw) ? 1e5 : EyeDepth(raw);
                float2 hc = (uv * _RfxVolFull.xy - 0.5) / _RfxVolParams.x;   // low-resolution pixel k stands for full texel k x scale
                float2 b = floor(hc), f = hc - b;
                float4 acc = 0.0; float wsum = 0.0;
                float4 best = 0.0; float bestErr = 1e30;
                [unroll] for (int y = 0; y <= 1; y++)
                    [unroll] for (int x = 0; x <= 1; x++)
                    {
                        float2 kk = clamp(b + float2(x, y), 0.0, _RfxVolLow.xy - 1.0);
                        float rk = RawAt(FullUV(kk));
                        float zk = IsSky(rk) ? 1e5 : EyeDepth(rk);
                        float err = abs(zk - z) / max(z, 1e-3);
                        float wb = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
                        float w = wb / (1e-3 + err * 20.0);
                        float4 c = SAMPLE_TEXTURE2D_LOD(_RfxVolSrc, sampler_PointClamp, (kk + 0.5) * _RfxVolLow.zw, 0);
                        acc += c * w; wsum += w;
                        if (err < bestErr) { bestErr = err; best = c; }
                    }
                float4 r = wsum > 1e-4 ? acc / wsum : best;
                if (_RfxVolDebug > 1.5) return float4(saturate(r.aaa), 0.0);
                if (_RfxVolDebug > 0.5) return float4(max(r.rgb, 0.0), 0.0);
                return float4(max(r.rgb, 0.0), saturate(r.a));
            }
            ENDHLSL
        }
    }
}
