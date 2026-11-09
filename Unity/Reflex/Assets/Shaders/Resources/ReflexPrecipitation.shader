// WEATHER (OPTIONS > VIDEO > WEATHER, LIGHTING: ENHANCED only; not part of the original renderer): the falling rain and snow and the rain's
// splashes on the ground, for Scripts/Rendering/ReflexWeather.cs. No particle buffer and no CPU work per particle: one procedural draw per
// layer (6 vertices per particle, SV_VertexID), every particle's place computed here from its index and the time.
//   rain / snow (_PMode 0 / 1): each particle has a fixed random place in a box that wraps around the camera (so it is still in the world
//     while the camera moves: the drops stream past at the right parallax), falls at its own speed with the wind (snow sways), and is drawn
//     as a segment from where it is to where it was _PFall.z seconds ago relative to the camera: the streak a camera's shutter sees. Thin
//     particles are widened to 1.25 pixels with their alpha scaled down by as much (no shimmering sub-pixel lines). Faded near the box's
//     edges. Lit by the sky (_PLight) and the sun (forward scattering towards it for the rain, diffuse for the snow).
//   splashes (_PMode 2): drops landing on the terrain around the camera (heights from _PGround, the ground under the camera sampled by the
//     CPU), a short crown of droplets each, one after another on new random spots.
// Blended (straight alpha) after the opaque world, the volumetric haze in front of them (ReflexVolumetricApply.hlsl).
Shader "Hidden/Reflex/Precipitation"
{
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent+10" "IgnoreProjector"="True" }
        Pass
        {
            Name "Precipitation"
            Tags { "LightMode"="UniversalForward" }
            Blend SrcAlpha OneMinusSrcAlpha
            ZWrite Off ZTest LEqual Cull Off
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "../ReflexCommon.hlsl"
            #include "../ReflexWeather.hlsl"
            #include "../ReflexVolumetricApply.hlsl"

            TEXTURE2D_FLOAT(_PGround); SAMPLER(sampler_PGround);
            float4 _PBox;        // xyz: the box (m), w: how far above the camera its centre is (m)
            float4 _PFall;       // x fall speed (m/s), y size (m: the drop's / flake's width), z shutter (s), w alpha
            float4 _PCamVel;     // xyz the camera's velocity (m/s), w pixel size at 1 m (2 tan(fov / 2) / screen height)
            float4 _PLight;      // rgb the sky light on a particle (raw), w the mode's brightness
            float4 _PSun;        // rgb the sun (raw, after the clouds), w sway (snow, m)
            float4 _PGroundRect; // x0, z0 of the ground texture, 1 / its size (m), w valid
            float4 _PMisc;       // x mode (0 rain, 1 snow, 2 splash), y splash life (s), z splash radius (m), w the first particle's index (layers differ)

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 seg : TEXCOORD0;      // x across (-1..1), y along (half lengths of the width), z half length / half width
                float4 colour : TEXCOORD1;
                float3 positionWS : TEXCOORD2;
                float2 misc : TEXCOORD3;     // x the splash's age 0..1, y a random number
            };

            float3 Hash31(uint n)
            {
                n = (n << 13u) ^ n; n = n * (n * n * 15731u + 789221u) + 1376312589u;
                uint3 k = n * uint3(n, n * 16807u, n * 48271u);
                return float3((k >> 8) & 0xffffffu) / float(0xffffff);
            }
            float3 WrapBox(float3 p, float3 lo, float3 size) { float3 r = p - lo; return lo + r - size * floor(r / size); }

            Varyings Vert(uint vid : SV_VertexID)
            {
                static const uint kIndex[6] = { 0, 1, 2, 0, 2, 3 };
                static const float2 kCorner[4] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, 1) };
                uint id = vid / 6u + (uint)_PMisc.w;
                float2 k = kCorner[kIndex[vid % 6u]];
                float3 h = Hash31(id * 3u + 1u), h2 = Hash31(id * 7u + 5u);
                float t = _RfxWeather2.w;
                float3 cam = _WorldSpaceCameraPos;
                float mode = _PMisc.x;
                Varyings o = (Varyings)0;
                float3 head, tail; float width, alpha = _PFall.w;
                float3 col;
                if (mode < 1.5)
                {
                    float speed = _PFall.x * (0.8 + 0.4 * h2.x);
                    float3 vel = mode < 0.5 ? float3(_RfxWeatherWind.x, -speed, _RfxWeatherWind.z) : float3(_RfxWeatherWind.x * (0.25 + 0.25 * h2.y), -speed, _RfxWeatherWind.z * (0.25 + 0.25 * h2.y));
                    float3 p = h * _PBox.xyz + vel * t;
                    if (mode > 0.5)
                    {   // snow sways: two slow circles of its own
                        float w1 = t * (0.7 + h2.z * 0.9) + h.x * 6.283, w2 = t * (1.9 + h2.y) + h.z * 6.283;
                        p.xz += (float2(sin(w1), cos(w1 * 0.8)) + 0.35 * float2(cos(w2), sin(w2))) * _PSun.w;
                    }
                    float3 lo = cam - 0.5 * _PBox.xyz + float3(0, _PBox.w, 0);
                    p = WrapBox(p, lo, _PBox.xyz);
                    float3 rel = (p - lo) / _PBox.xyz * 2.0 - 1.0;
                    alpha *= smoothstep(0.6, 2.2, length(p - cam));   // (no thick streaks across the lens)
                    float edge = max(abs(rel.x), max(abs(rel.z), abs(rel.y)));
                    alpha *= 1.0 - smoothstep(0.72, 1.0, edge);
                    float3 vr = vel - _PCamVel.xyz;
                    head = p; tail = p - vr * _PFall.z;
                    width = _PFall.y * (mode < 0.5 ? (0.7 + 0.6 * h2.z) : (0.45 + 1.1 * h2.z * h2.z));
                    float3 toSun = _RfxSunDir.xyz;
                    float3 vd = normalize(p - cam);
                    if (mode < 0.5)
                    {   // a drop: the sky it refracts, brighter looking towards the sun (forward scattering), a little of its glint
                        float fwd = pow(saturate(dot(vd, toSun)), 6.0);
                        col = _PLight.rgb * (0.85 + 0.3 * h.y) + _PSun.rgb * (0.08 + 0.7 * fwd);
                    }
                    else
                    {   // a flake: white, the sky light from all round, the sun on it
                        col = (_PLight.rgb * 1.05 + _PSun.rgb * 0.55) * (0.85 + 0.15 * h.y);
                    }
                    col *= _PLight.w;
                }
                else
                {
                    // a splash: life _PMisc.y s, a new random spot in the disc each life
                    float life = _PMisc.y * (0.75 + 0.5 * h2.x);
                    float cyc = t / life + h.x * 17.0;
                    float n = floor(cyc), age = frac(cyc);
                    float3 r = Hash31(id * 131u + (uint)(n + 70000.0) * 977u);
                    float rad = sqrt(r.x) * _PMisc.z, ang = r.y * 6.2831853;
                    float2 xz = cam.xz + float2(cos(ang), sin(ang)) * rad;
                    float2 guv = (xz - _PGroundRect.xy) * _PGroundRect.z;
                    float y = SAMPLE_TEXTURE2D_LOD(_PGround, sampler_PGround, guv, 0).r;
                    bool inside = _PGroundRect.w > 0.5 && all(guv > 0.02) && all(guv < 0.98) && y > -1e5;
                    float size = _PFall.y * (0.6 + 0.8 * r.z) * (0.45 + 0.75 * age);
                    // onto the surface the camera sees there (the drawn ground sits above the collision grid by the ruts' and bump models'
                    // displacement): the scene depth along the ray to the ground point; a bike or a wall in front hides it instead
                    float3 g0 = float3(xz.x, y, xz.y);
                    float4 gc = TransformWorldToHClip(g0);
                    float2 suv = ComputeScreenPos(gc).xy / max(gc.w, 1e-4);
                    if (gc.w > 0.05 && all(suv > 0.0) && all(suv < 1.0))
                    {
                        float raw = LoadSceneDepth(uint2(suv * _ScreenSize.xy));
                        float3 surf = ComputeWorldSpacePosition(suv, raw, UNITY_MATRIX_I_VP);
                        if (abs(surf.y - g0.y) < 1.5 && length(surf.xz - g0.xz) < 2.5) g0 = surf; else inside = false;
                    }
                    else inside = false;
                    head = g0 + normalize(cam - g0) * 0.03; tail = head + float3(0, size, 0);
                    width = size;
                    alpha *= inside ? 1.0 : 0.0;
                    alpha *= 1.0 - smoothstep(0.6, 1.0, rad / _PMisc.z);
                    col = (_PLight.rgb * 1.2 + _PSun.rgb * 0.25) * _PLight.w;
                    o.misc = float2(age, r.z);
                }

                // the segment as a camera-facing ribbon, at least 1.25 pixels wide (alpha scaled down by the widening)
                float3 mid = 0.5 * (head + tail);
                float d = max(length(mid - cam), 0.05);
                float pix = _PCamVel.w * d;
                float wPix = max(width, pix * 1.25);
                alpha *= saturate(width / wPix);
                float3 axis = tail - head;
                float len = length(axis);
                float3 view = normalize(mid - cam);
                float3 dir = len > 1e-5 ? axis / len : float3(0, 1, 0);
                if (mode > 1.5) dir = float3(0, 1, 0);
                float3 side = cross(dir, view);
                float sl = length(side);
                side = sl > 1e-4 ? side / sl : UNITY_MATRIX_I_V._m00_m10_m20;
                if (mode > 1.5) { side = normalize(cross(float3(0, 1, 0), view)); }
                float hw = 0.5 * wPix, hl = 0.5 * len;
                float ratio = hl / hw;
                float3 world;
                if (mode > 1.5) world = head + side * (k.x * hw) + float3(0, (k.y * 0.5 + 0.5) * width, 0);
                else world = mid + side * (k.x * hw) + dir * (k.y * (hl + hw));
                o.seg = float3(k.x, k.y * (ratio + 1.0), ratio);
                if (mode > 1.5) o.seg = float3(k.x, k.y * 0.5 + 0.5, 0);
                o.colour = float4(col, alpha);
                o.positionWS = world;
                o.positionCS = alpha < 0.002 ? float4(2, 2, 2, 1) : TransformWorldToHClip(world);
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                float mode = _PMisc.x;
                float a;
                if (mode < 1.5)
                {
                    float2 q = float2(i.seg.x, max(abs(i.seg.y) - i.seg.z, 0.0));
                    float dd = length(q);
                    if (mode < 0.5)
                    {
                        float along = saturate(abs(i.seg.y) / max(i.seg.z + 1.0, 1e-3));
                        a = saturate(1.0 - dd) * (1.0 - along * along * 0.6);   // a streak, thinning at its ends
                    }
                    else a = smoothstep(1.0, 0.2, dd);                          // a soft flake
                }
                else
                {
                    // the crown: droplets thrown up and out from the impact point (bottom centre), a ring on the ground at first
                    float age = i.misc.x;
                    float2 uv = float2(i.seg.x, i.seg.y);
                    float r = length(uv), th = atan2(uv.x, max(uv.y, 1e-3));
                    float rays = pow(saturate(cos(th * 4.5 + i.misc.y * 6.0)), 18.0);
                    float front = exp(-pow(r - (0.25 + 0.75 * age), 2.0) * 60.0);
                    float base = exp(-uv.y * uv.y * 400.0) * (1.0 - abs(uv.x)) * (1.0 - age);
                    a = saturate(rays * front * 1.6 + base * 0.8) * (1.0 - age * age);
                }
                a *= i.colour.a;
                if (_RfxWeatherDebug.x > 4.5 && mode > 1.5) return half4(1, 0, 1, 1);   // (tests: the splashes' quads in magenta)
                clip(a - 0.002);
                float3 c = RfxVolApply(i.colour.rgb, a, i.positionWS, i.positionCS, 5, 10);
                return half4(RfxOutput(c), a);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
