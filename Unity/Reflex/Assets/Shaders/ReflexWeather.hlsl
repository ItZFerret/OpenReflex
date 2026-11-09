// WEATHER (OPTIONS > VIDEO > WEATHER, LIGHTING: ENHANCED only; not part of the original renderer): the shared inputs and the ground model of
// Scripts/Rendering/ReflexWeather.cs. Every function here is only reached behind a uniform branch on _RfxWeather.w (on), so with WEATHER: CLEAR
// (or LIGHTING: ORIGINAL) no pixel changes: the original programs' results are returned untouched.
//   _RfxWeather       x surface wetness 0..1 (darker, glossier), y snow cover 0..1, z overcast 0..1 (grey sky, flat light), w on (1)
//   _RfxWeather2      x puddle level 0..1 (how full the ruts are), y rain falling 0..1 (ripples), z snow falling 0..1, w time (s, wrapped)
//   _RfxWeatherSky    rgb: the overcast sky's zenith colour (raw values, from the level's own fog colour), w: cloud darkness
//   _RfxWeatherFog    rgb: the mist colour (raw), w: mist density (1/m)
//   _RfxWeatherWind   xz: the wind (m/s, world), y: unused, w: the cloud scroll (m)
//   _RfxWeatherDebug  x: 1 puddle mask, 2 snow cover / trail, 3 the raw deformation, 4 the tyre-track darkness (tests)
#ifndef REFLEX_WEATHER_INCLUDED
#define REFLEX_WEATHER_INCLUDED

float4 _RfxWeather, _RfxWeather2, _RfxWeatherSky, _RfxWeatherFog, _RfxWeatherWind, _RfxWeatherDebug;

bool RfxWeatherOn() { return _RfxWeather.w > 0.5; }

// ---------------------------------------------------------------------------------------------- noise (no textures: no sampler registers)
float RfxWHash12(float2 p) { float3 p3 = frac(float3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.x + p3.y) * p3.z); }
float2 RfxWHash22(float2 p) { float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.xx + p3.yz) * p3.zy); }
float RfxWNoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = RfxWHash12(i), b = RfxWHash12(i + float2(1, 0)), c = RfxWHash12(i + float2(0, 1)), d = RfxWHash12(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float RfxWFbm(float2 p, int octaves)
{
    float s = 0, a = 0.5, n = 0;
    [loop] for (int k = 0; k < octaves; k++) { s += a * RfxWNoise(p); n += a; p = float2(p.x * 1.6 - p.y * 1.2, p.x * 1.2 + p.y * 1.6) + 17.3; a *= 0.5; }
    return s / n;
}

// ---------------------------------------------------------------------------------------------- the overcast sky
// The cloud deck seen in direction d (raw values): the level's fog colour as the light under the clouds (it was authored to sit against the
// horizon), darker towards the zenith, with slowly moving fbm cloud masses projected on a plane 1.5 km up. a: how much it covers the sky.
float3 RfxWeatherSkyColour(float3 d, float detail)
{
    float3 zenith = _RfxWeatherSky.rgb, horizon = _RfxWeatherFog.rgb;
    float up = saturate(d.y);
    float3 c = lerp(horizon, zenith, smoothstep(0.0, 0.55, up));
    [branch] if (detail > 0.5)   // (blurry reflections take the deck's gradient only)
    {
        float2 pl = d.xz / max(d.y + 0.08, 0.02) * 1.6 + _RfxWeatherWind.w * float2(0.0007, 0.0004);
        float n = RfxWFbm(pl, 4);
        float m = RfxWNoise(pl * 2.7 + 9.1);
        float dens = saturate((n - 0.32) * 1.8);
        // the undersides: darker masses (_RfxWeatherSky.w), lighter thinner gaps; less contrast towards the horizon (aerial perspective)
        float shade = lerp(1.18, 1.0 - _RfxWeatherSky.w, dens) * lerp(0.92, 1.08, m);
        c *= lerp(1.0, shade, detail * smoothstep(-0.02, 0.25, d.y));
    }
    return max(c, 0.0);
}


// ---------------------------------------------------------------------------------------------- rain ripples on standing water
// Expanding rings from drops: one drop per cell (random centre and phase), the 3 x 3 neighbourhood so rings cross cell borders. Returns the
// slope (d/dx, d/dz) of the water surface.
float2 RfxWRippleLayer(float2 p, float t, float density)
{
    float2 g = 0;
    float2 i0 = floor(p);
    [unroll] for (int y = -1; y <= 1; y++)
    [unroll] for (int x = -1; x <= 1; x++)
    {
        float2 c = i0 + float2(x, y);
        float2 h = RfxWHash22(c);
        if (h.y > density) continue;
        float ph = frac(t + h.x * 7.31);
        float2 centre = c + 0.5 + (RfxWHash22(c + 19.7) - 0.5) * 0.7;
        float2 dv = p - centre;
        float r = length(dv);
        float w = r - ph * 1.3;                         // the ring's front grows to 1.3 cells
        float env = (1.0 - ph) * (1.0 - ph) * exp(-w * w * 40.0);
        float wave = cos(w * 28.0) * env;
        g += dv / max(r, 1e-3) * wave;
    }
    return g;
}
float2 RfxWeatherRipples(float2 xz, float t, float rain)
{
    float d = saturate(rain);
    float2 g = RfxWRippleLayer(xz * 3.1, t * 1.15, 0.35 + 0.6 * d) * 0.55;
    g += RfxWRippleLayer(xz * 4.7 + 13.0, t * 1.4 + 0.37, 0.25 + 0.6 * d) * 0.4;
    return g * (0.25 + 0.75 * d);
}

// ---------------------------------------------------------------------------------------------- the ground (terrain)
struct RfxWeatherGround
{
    float3 N;          // the shading normal after the weather (snow smooths the bumps, water is flat with ripples)
    float wet;         // surface wetness (darkening, gloss)
    float puddle;      // standing water 0..1
    float snow;        // snow over the ground 0..1
    float trail;       // ridden-through 0..1 (ruts and tyre tracks through the snow)
    float mudTrail;    // the trail's share that shows churned mud (deep ruts) instead of packed snow
    float edge;        // puddle edge (darkest, wettest soil)
    float sparkle;     // snow glints
    float tone;        // the snow's brightness over the ground (drifts, wind-blown patches)
};

// P: world position, Ngeo: the heightfield normal (no bump), Nb: the bump-mapped normal, dm: deformation - neutral (ruts < 0, berms > 0),
// conc: the deformation's concavity (neighbours' mean - centre: > 0 at a rut's bottom), basin: the height field's concavity (m),
// track: the tyre-track darkness at the pixel (0 none .. 1), trackWide: the same over a few decimetres, trackGrad: its gradient (per m),
// deformGrad: the deformation's gradient (per m), dist: to the camera
RfxWeatherGround RfxWeatherGroundPrepare(float3 P, float3 Ngeo, float3 Nb, float dm, float conc, float basin, float track, float trackWide,
                                         float2 trackGrad, float2 deformGrad, float3 V, float dist)
{
    RfxWeatherGround g = (RfxWeatherGround)0;
    g.N = Nb;
    float near = 1.0 - saturate((dist - 120.0) / 120.0);   // the small-scale terms fade out with distance
    // (the terrain's normal sees the heights three times steeper than the geometry, as the original's: the true slope for the water)
    float trueY = normalize(float3(Ngeo.x / 3.0, Ngeo.y, Ngeo.z / 3.0)).y;
    float flat = smoothstep(0.90, 0.985, trueY);
    float n1 = RfxWNoise(P.xz * 0.35), n2 = RfxWNoise(P.xz * 1.7 + 5.3);

    // ---- rain: wet soil everywhere, water collecting where the ground is lowest: the ruts' bottoms, the tyre grooves, hollows
    float wet = _RfxWeather.x;
    g.wet = wet;
    float level = _RfxWeather2.x;
    [branch] if (level > 0.001)
    {
        float rut = saturate(-dm * 14.0);                       // depth below the neutral surface
        float bottom = saturate(conc * 60.0);                   // the trough itself
        float groove = track * 0.55 + trackWide * 0.35;
        float hollow = saturate(basin * 3.0);
        float s = max(max(rut * 0.55 + bottom * 0.75, groove), hollow) + (n1 - 0.5) * 0.45 + (n2 - 0.5) * 0.12;
        float th = 1.02 - level * 0.72;                          // a higher level reaches shallower ground
        float p = smoothstep(th, th + 0.12, s) * flat;
        g.puddle = p * near;
        g.edge = saturate(smoothstep(th - 0.22, th, s) - p) * flat;
    }

    // ---- snow: on the flatter ground, broken up at its edge by noise; ridden through where the ground was rutted or tracked
    float cover = _RfxWeather.y;
    [branch] if (cover > 0.001)
    {
        float slope = smoothstep(0.42, 0.78, Ngeo.y + (n1 - 0.5) * 0.25);
        float amount = saturate(cover * 1.35 - (1.0 - slope) - (n2 - 0.5) * 0.25 * (1.0 - cover));
        float rutTrail = saturate(abs(dm) * 18.0) * near;
        float tyre = saturate(track * 1.6 + trackWide * 1.2) * near;
        // a clear edge: the snow is either ridden through or not (soft over a few centimetres, broken a little by the noise)
        g.trail = smoothstep(0.22, 0.62, max(rutTrail, tyre) + (n2 - 0.5) * 0.12);
        g.mudTrail = saturate(rutTrail * 1.5 - 0.25) * saturate(1.15 - cover * 0.25);
        g.snow = amount * (1.0 - g.trail);
        // the surface: snow fills the small bumps (towards the heightfield's normal) and its trail walls slope down into the tracks
        float depth = 0.12 * amount;                             // m of snow (only the trail walls' normal uses it)
        float2 hg = -(trackGrad * 1.6 + deformGrad * 18.0) * depth * near;
        float3 Ns = normalize(Ngeo + float3(-hg.x, 0, -hg.y) * 6.0);
        g.N = normalize(lerp(Nb, Ns, saturate(amount * 1.5)));
        // glints: tiny facets that catch the light from one view direction only
        float2 cell = floor(P.xz * 38.0);
        float hsh = RfxWHash12(cell + floor(V.xz * 23.0 + V.y * 31.0));
        g.sparkle = step(0.985, hsh) * saturate(1.0 - dist / 25.0) * g.snow;
        g.tone = lerp(0.90, 1.04, RfxWNoise(P.xz * 0.06)) * lerp(0.96, 1.02, n2);
    }
    return g;
}

// The ground's colour after the weather. albedo: the original's albedo (raw display values), light: what it is multiplied by (sun + ambient,
// raw), spec: the original's specular term, sunLight: the sun's share (shadowed), R: reflect(view, N), V: camera -> point, sky: the sky
// reflection function's result for the wet / water reflections (raw).
float3 RfxWeatherGroundShade(RfxWeatherGround g, float3 albedo, float3 light, float3 spec, float3 sunLight, float3 V, float3 skyRough, float3 skySharp, float3 waterN)
{
    float3 colour = albedo * light + spec;
    // wet soil: darker (water fills the pores) and a little more saturated, a soft sheen of the sky
    float wet = saturate(g.wet * (1.0 - g.snow));
    [branch] if (wet > 0.001)
    {
        float lum = dot(albedo, float3(0.299, 0.587, 0.114));
        float3 wa = max(lerp(lum.xxx, albedo, 1.5), 0.0) * lerp(1.0, 0.58, wet);
        wa *= lerp(1.0, 0.78, g.edge * wet);
        float fr = 0.02 + 0.98 * pow(1.0 - saturate(dot(-V, g.N)), 5.0);
        colour = wa * light + spec * lerp(1.0, 0.6, wet) + skyRough * fr * wet * 0.4;
    }
    // standing water: the soil far darker under it, a mirror of the sky (Fresnel) with the drops' ripples, the sun's glint
    [branch] if (g.puddle > 0.001)
    {
        float cosV = saturate(dot(-V, waterN));
        float fr = 0.02 + 0.98 * pow(1.0 - cosV, 5.0);
        float3 under = albedo * float3(0.30, 0.27, 0.22) * light;
        float3 water = lerp(under, skySharp, fr);
        float3 H = normalize(_RfxSunDir.xyz - V);
        float glint = pow(saturate(dot(waterN, H)), 900.0) * 40.0;
        water += sunLight * glint * fr;
        colour = lerp(colour, water, g.puddle);
    }
    // snow: bright, slightly blue in the shade (the sky light), packed and greyer in the trail, mud where the ruts went deep
    [branch] if (g.snow > 0.001 || g.trail > 0.001)
    {
        float3 snowA = float3(0.80, 0.82, 0.86) * g.tone;
        // the trail: snow packed into ice-grey ruts, the dirt showing through it, churned brown mud where the ruts went deep
        float3 packedA = lerp(float3(0.62, 0.64, 0.69), albedo, 0.15);
        float3 mudA = albedo * float3(0.50, 0.44, 0.38);
        float3 trailA = lerp(packedA, mudA, smoothstep(0.35, 0.9, g.mudTrail));
        float3 sunPart = max(sunLight, 0.0);
        float3 amb = max(light - sunPart, 0.0);
        float3 snowC = snowA * (sunPart + amb * float3(0.95, 1.0, 1.12)) + skyRough * 0.04;
        snowC += sunPart * g.sparkle * 6.0;
        float3 trailC = trailA * light;
        float cov = saturate(_RfxWeather.y * 1.2);
        float3 ground = lerp(colour, trailC, saturate(g.trail * cov));
        colour = lerp(ground, snowC, g.snow);
    }
    return colour;
}

#endif
