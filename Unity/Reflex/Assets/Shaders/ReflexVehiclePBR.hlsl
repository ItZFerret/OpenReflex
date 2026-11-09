// MATERIALS: ENHANCED (OPTIONS > VIDEO, with LIGHTING: ENHANCED; not part of the original renderer): physically based bike / rider surfaces and
// mud / dirt build-up over Reflex/Vehicle's ported object programs (Shaders/ReflexVehicle.shader). The original program of the material still runs
// first and gives the base look (the game's albedo, decals, masks, fill lights, shadows); this file adds to its result, per material CLASS
// (Content/ReflexMaterialClasses.cs sets _RfxMat.x from the material's shader / material / texture names):
//   the original's environment reflection   scaled down (rfxReflScale, RfxReflection in ReflexObjectShading.hlsl): replaced by the GGX terms below
//   specular                                a GGX sun highlight and the environment's reflection (the level's reflection cube: in ENHANCED the HDRI
//                                           sky) through the split-sum approximation, a clear coat lobe over plastics / paint / helmets, F0 from
//                                           the metal class (chrome, anodised, brushed, painted frame)
//   fabric sheen, micro grain               a grazing-angle sheen for cloth, weave / pebble / rubber grain in the albedo (no new textures)
//   mud / dirt                              _RfxDirt (Content/ReflexDirt.cs): a mask from the vertex's rest-pose height (uv3: ground = 0), the
//                                           part's affinity (wheels > boots > frame > seat > jersey), fractal noise in rest space and the side
//                                           the rear wheel throws it to; caked mud / dry dust by the amount, wetter and glossier when wet; capped
//                                           so the graphics always show through
// Everything is in the original's raw (display-referred) units like the programs' own output, lit by the same rig as the programs (RfxObjRig:
// the fill lights, the bounce and the sun's radiosity through the level's colour reference map, the volume shadow / MapShadow term, the
// shadow term the fragment gives the program) so the new layers sit in the level's light.
#ifndef REFLEX_VEHICLE_PBR_INCLUDED
#define REFLEX_VEHICLE_PBR_INCLUDED

float _RfxMatOn;       // global: MATERIALS: ENHANCED in effect (ReflexEnhancedMaterials; needs LIGHTING: ENHANCED)
float _RfxDirtOn;      // global: MUD BUILDUP in effect

// ---------------------------------------------------------------------------------------------------------------------------- classes
struct RfxSurf
{
    float metal, rough, cc, ccRough, f0, sheen, origRefl, gain, dirtAff, dirtH, grain, grainScale, chrome;
};

// k: ReflexMaterialClasses.Kind
RfxSurf RfxSurfOf(int k)
{
    RfxSurf s;
    s.metal = 0; s.rough = 0.5; s.cc = 0; s.ccRough = 0.1; s.f0 = 0.04; s.sheen = 0; s.origRefl = 0; s.gain = 1; s.dirtAff = 0.8; s.dirtH = 1.0;
    s.grain = 0; s.grainScale = 60; s.chrome = 0;
    switch (k)
    {
        case 1:  s.rough = 0.42; s.cc = 1.0;  s.ccRough = 0.07; s.dirtAff = 0.85; s.dirtH = 1.0; s.grain = 0.01; break;                       // PAINT PLASTIC (graphic kit plastics, fenders)
        case 2:  s.rough = 0.62; s.cc = 0.2;  s.ccRough = 0.22; s.dirtAff = 0.95; s.dirtH = 1.1; s.grain = 0.03; s.grainScale = 110; break;    // MATTE PLASTIC (hand guards, nerf bars, neck brace)
        case 3:  s.metal = 0.62; s.rough = 0.46; s.cc = 0.5;  s.ccRough = 0.1; s.dirtAff = 1.05; s.dirtH = 1.0; s.grain = 0.015; break;       // FRAME (painted / powder-coated metal)
        case 4:  s.metal = 1.0;  s.rough = 0.07; s.dirtAff = 0.7; s.dirtH = 0.9; s.chrome = 1; break;                                        // CHROME
        case 5:  s.metal = 1.0;  s.rough = 0.2;  s.dirtAff = 0.9; s.dirtH = 0.9; break;                                                      // ANODISED
        case 6:  s.metal = 1.0;  s.rough = 0.42; s.dirtAff = 1.0; s.dirtH = 0.9; s.grain = 0.04; s.grainScale = 220; break;                   // BRUSHED METAL (engine, muffler)
        case 7:  s.rough = 0.88; s.f0 = 0.045; s.gain = 0.55; s.dirtAff = 1.45; s.dirtH = 2.0; s.grain = 0.05; s.grainScale = 160; break;      // RUBBER TYRE
        case 8:  s.rough = 0.64; s.cc = 0.08; s.ccRough = 0.3; s.dirtAff = 0.6; s.dirtH = 0.85; s.grain = 0.04; s.grainScale = 90; break;     // SEAT / GRIP
        case 9:  s.rough = 0.96; s.gain = 0.35; s.sheen = 1.0; s.dirtAff = 0.8; s.dirtH = 0.8; s.grain = 0.07; s.grainScale = 380; break;      // FABRIC
        case 10: s.rough = 0.5;  s.cc = 0.35; s.ccRough = 0.3; s.dirtAff = 1.25; s.dirtH = 0.5; s.grain = 0.04; s.grainScale = 150; break;    // LEATHER / BOOT
        case 11: s.rough = 0.4;  s.cc = 1.0;  s.ccRough = 0.035; s.dirtAff = 0.1; s.dirtH = 0.5; s.grain = 0.008; break;                      // HELMET SHELL
        case 12: s.rough = 0.04; s.f0 = 0.06;  s.gain = 1.6; s.dirtAff = 0.12; s.dirtH = 0.5; break;                                          // GOGGLE LENS
        case 13: s.rough = 0.55; s.cc = 0; s.f0 = 0.028; s.gain = 0.5; s.sheen = 0.2; s.dirtAff = 0.45; s.dirtH = 0.6; break;                // SKIN
        case 14: s.metal = 0.28; s.rough = 0.4;  s.cc = 1.0;  s.ccRough = 0.05; s.dirtAff = 0.9; s.dirtH = 1.4; s.grain = 0.01; break;       // BODY PAINT (trucks, buggies, UTVs)
        case 15: s.metal = 0.9;  s.rough = 0.3;  s.dirtAff = 1.2; s.dirtH = 1.2; break;                                                      // WHEEL RIM / HUB
        case 16: s.metal = 1.0;  s.rough = 0.26; s.dirtAff = 0.8; s.dirtH = 1.0; break;                                                      // SPRING / SHOCK
        default: break;
    }
    return s;
}

// the original program's own environment reflection kept for the class (0: all of it replaced by the terms below)
float RfxMatOrigReflection(int k) { return RfxSurfOf(k).origRefl; }

// ---------------------------------------------------------------------------------------------------------------------------- noise
float RfxHash(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}
float RfxValueNoise(float3 x)
{
    float3 i = floor(x), f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    float a = lerp(lerp(RfxHash(i), RfxHash(i + float3(1, 0, 0)), f.x), lerp(RfxHash(i + float3(0, 1, 0)), RfxHash(i + float3(1, 1, 0)), f.x), f.y);
    float b = lerp(lerp(RfxHash(i + float3(0, 0, 1)), RfxHash(i + float3(1, 0, 1)), f.x), lerp(RfxHash(i + float3(0, 1, 1)), RfxHash(i + float3(1, 1, 1)), f.x), f.y);
    return lerp(a, b, f.z);
}
float RfxFbm(float3 p)
{
    float a = 0.5, s = 0, w = 0;
    [unroll] for (int o = 0; o < 3; o++)
    {
        s += a * RfxValueNoise(p); w += a; p = p * 2.03 + 17.1; a *= 0.5;
    }
    return s / w;
}

// a screen-space bump (Mikkelsen's surface gradient): the normal tilted by the height field's screen derivatives (taken by the caller
// outside any divergent branch)
float3 RfxBumpFromHeight(float3 dpdx, float3 dpdy, float dhx, float dhy, float3 N, float k)
{
    float3 r1 = cross(dpdy, N), r2 = cross(N, dpdx);
    float det = dot(dpdx, r1);
    float3 grad = sign(det) * (dhx * r1 + dhy * r2);
    return normalize(abs(det) * N - k * grad);
}

// ---------------------------------------------------------------------------------------------------------------------------- BRDF
float3 RfxEnvBRDF(float3 F0, float rough, float NoV)
{
    // Karis' analytic split-sum (mobile): scale and bias of F0
    const float4 c0 = float4(-1, -0.0275, -0.572, 0.022), c1 = float4(1, 0.0425, 1.04, -0.04);
    float4 r = rough * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}
float RfxGGX(float NoH, float a2)
{
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * d * d + 1e-6);
}
float RfxVisSmith(float NoV, float NoL, float a2)
{
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2), gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-5);
}
float3 RfxFresnelSchlick(float3 F0, float VoH) { float f = pow(1.0 - saturate(VoH), 5.0); return F0 + (1.0 - F0) * f; }

// the environment in direction R at a perceptual roughness: the level's reflection cube (ENHANCED: the HDRI sky), raw HDR units
float3 RfxMatEnv(float3 R, float rough)
{
    float maxMip = _RfxEnhParams2.w > 0.5 ? _RfxEnhParams2.z : 6.0;
    return SAMPLE_TEXTURECUBE_LOD(_RfxReflectionCube, sampler_RfxReflectionCube, R, rough * (1.7 - 0.7 * rough) * maxMip).rgb;
}

// geometric specular anti-aliasing: the roughness widened by the normal's variance over the pixel (Kaplanyan / Tokuyoshi)
float RfxSpecAA(float3 N, float a2)
{
    float3 dx = ddx(N), dy = ddy(N);
    float variance = 0.25 * (dot(dx, dx) + dot(dy, dy));
    return saturate(a2 + min(2.0 * variance, 0.18));
}

float RfxLuma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

// ---------------------------------------------------------------------------------------------------------------------------- the light
// The light the original object programs give a surface at P with normal N (e.g. RfxPs0 071c..09e4): the four fill lights; the bounce from
// below through the level's colour reference (s13 at gColorRefProjection: sat(-N.y) lerp(ref.rgb, gBounceLightColor, ref.a)); the sun's
// visibility of the volume shadow map (s12 through RfxVolumeShadowTexel: the Alive MapShadow included) and its radiosity floor(ref.a) ? ref.rgb : 1.
// The radiosity is what darkens a vehicle in the unlit parts of a stadium (the colour reference maps of the supercross / night levels: the
// light pools under the rigs) - the new layers must take it, or they glow there (w-mudlight).
// amb: the ambient (before ENHANCED's sky tint), sunK: what multiplies _RfxSunColor before the dynamic shadow and N.L.
void RfxObjRig(float3 P, float3 N, out float3 amb, out float3 sunK)
{
    float3 n = normalize(N);
    float4 ref = RfxColorRefAt(P);
    amb = saturate(dot(_RfxLightDir1.xyz, n)) * _RfxLightColor1.rgb + saturate(dot(_RfxLightDir0.xyz, n)) * _RfxLightColor0.rgb
        + saturate(dot(_RfxLightDir2.xyz, n)) * _RfxLightColor2.rgb + saturate(dot(_RfxLightDir3.xyz, n)) * _RfxLightColor3.rgb
        + saturate(-n.y) * lerp(ref.rgb, _RfxBounceColor.rgb, ref.a);
    float2 vuv = float2(dot(float4(P, 1), RfxWorldProj0()), dot(float4(P, 1), RfxWorldProj1()));
    float vol = saturate(1.0 - 0.25 * (RfxVolumeShadowTexel(vuv).x * 2048.0 - P.y - 0.1));
    sunK = vol * lerp(float3(1, 1, 1), ref.rgb, floor(ref.a));
}

// ---------------------------------------------------------------------------------------------------------------------------- the layers
// c: the original program's result (fog included). P / N / rp: world position, world normal, rest-pose position in metres. occ: (ao, contact
// shadow) of ReflexOcclusionPass, shadow: Unity's main light shadow at the pixel (1 without), fog: RfxFog(P) (rgb, 1 - amount).
float3 RfxMatLayers(float3 c, int k, float3 P, float3 N, float3 rp, float2 occ, float shadow, float4 fog, bool front, inout float alpha)
{
    RfxSurf s = RfxSurfOf(k);
    float3 V = normalize(_WorldSpaceCameraPos - P);
    float NoVraw = dot(N, V);
    if (NoVraw < 0) { N = -N; NoVraw = -NoVraw; }
    float NoV = max(NoVraw, 1e-3);
    float3 L = normalize(_RfxSunDir.xyz);
    float NoL = saturate(dot(N, L));
    float3 amb, sunK;
    RfxObjRig(P, N, amb, sunK);
    float aoAmb = lerp(1.0, occ.x, _RfxEnhParams.y);
    #if defined(_RFX_ENHANCED)
    amb = RfxEnhAmbient(amb, N, occ.x);
    #endif
    float3 sunC = _RfxSunColor.rgb * sunK * (shadow * occ.y);
    float rough = saturate(s.rough + _RfxMat.y);

    // ---- micro grain in the albedo (weave / pebble / rubber), faded where it would shimmer
    float grainFade = 0;
    if (s.grain > 0)
    {
        float3 q = rp * s.grainScale;
        grainFade = saturate(1.0 - length(fwidth(q)) * 0.6);
        float g;
        if (k == 9) g = (sin(q.x) * sin(q.y) + sin(q.y) * sin(q.z) + sin(q.z) * sin(q.x)) * 0.33;   // weave
        else g = RfxValueNoise(q) * 2.0 - 1.0;
        c *= 1.0 + s.grain * g * grainFade;
    }

    // ---- albedo estimate: the colour the program gave, normalised (its hue, with a share of its luminance for F0)
    float lum = RfxLuma(c);
    float3 tint = saturate((c + 0.03) / (lum + 0.03));
    float3 albedoF = saturate(tint * (0.55 + 0.45 * saturate(lum * 1.4)));

    // ---- specular: base layer
    float3 F0d = s.f0.xxx;
    float3 F0 = lerp(F0d, s.chrome > 0.5 ? lerp(float3(0.93, 0.93, 0.93), tint, 0.2) : albedoF * 0.95 + 0.03, s.metal);
    float a = rough * rough;
    float a2 = RfxSpecAA(N, a * a);
    float3 H = normalize(L + V);
    float NoH = saturate(dot(N, H)), VoH = saturate(dot(V, H));
    float3 R = reflect(-V, N);
    float specOcc = saturate(pow(NoV + occ.x, exp2(-16.0 * rough - 1.0)) - 1.0 + occ.x);
    float3 envBase = RfxMatEnv(R, rough) * specOcc;
    float3 specBase = envBase * RfxEnvBRDF(F0, rough, NoV);
    float3 sunSpecBase = RfxFresnelSchlick(F0, VoH) * RfxGGX(NoH, a2) * RfxVisSmith(NoV, NoL, a2) * NoL * 3.14159265 * sunC;

    // ---- clear coat over it
    float3 specCC = 0, sunSpecCC = 0; float ccAtten = 1;
    if (s.cc > 0)
    {
        float cr = s.ccRough, ca = cr * cr, ca2 = RfxSpecAA(N, ca * ca);
        float Fcc = 0.04 + 0.96 * pow(1.0 - NoV, 5.0);
        float3 envCC = RfxMatEnv(R, cr) * specOcc;
        specCC = envCC * RfxEnvBRDF(float3(0.04, 0.04, 0.04), cr, NoV) * s.cc * 2.0;   // (art direction: the sky in a clear coat reads stronger than physical)
        sunSpecCC = (0.04 + 0.96 * pow(1.0 - VoH, 5.0)) * RfxGGX(NoH, ca2) * RfxVisSmith(NoV, NoL, ca2) * NoL * 3.14159265 * sunC * s.cc * 1.5;
        ccAtten = 1.0 - Fcc * s.cc;
    }

    // ---- the sum: the original's diffuse (metals keep part of it: their textures paint the metal), the new specular over it
    float diffuseKeep = 1.0 - 0.62 * s.metal;
    float3 spec = (specBase * ccAtten + specCC + (sunSpecBase * ccAtten + sunSpecCC)) * s.gain;
    // the sun's glints are already in the original's own highlight for plastics: keep the new one from doubling it
    c = (c - fog.rgb) * diffuseKeep + fog.rgb + spec * fog.w;

    // ---- sheen
    if (s.sheen > 0)
    {
        float f = pow(1.0 - NoV, 2.5);
        float3 litv = amb * aoAmb + sunC * NoL;
        c += (c * 0.9 + 0.05 * litv) * f * s.sheen * fog.w;
    }
    // ---- lens: the reflection shows through the (blended) tint
    if (k == 12) alpha = saturate(alpha + RfxLuma(spec) * 0.8);
    return c;
}

// ----------------------------------------------------------------------------------------------------------------------------- mud / dirt
// returns the coverage 0..1 (already capped); noiseOut: a height for the relief
float RfxDirtCoverage(float3 rp, float3 N, float amount, float aff, float H, float seed, float fade, out float height)
{
    float h = rp.y;
    float cover = amount * aff * saturate(1.15 - h / H);
    float back = saturate(dot(N, -_RfxDirtF.xyz) * 0.5 + 0.5);               // the rear wheel's roost lands behind and below
    cover *= lerp(0.82, 1.18, back);
    height = 0.5;
    if (cover < 0.035) return 0.0;                                           // (above the dirt's reach: no noise at all)
    float3 q = rp * 5.0 + seed;
    float n = RfxFbm(q);
    float n2 = RfxValueNoise(rp * 26.0 + seed * 1.7);
    float n3 = 0.5;
    [branch] if (fade > 0.05) n3 = lerp(0.5, RfxValueNoise(rp * 85.0 + seed * 3.1), fade);   // (the finest grain fades out where it would shimmer)
    height = n2 * 0.7 + n3 * 0.3;
    float d = cover * 1.6 + (n - 0.5) * 0.9 + (n2 - 0.5) * 0.4 + (n3 - 0.5) * 0.18 - 0.4;
    return smoothstep(0.0, 0.28, d) * 0.86;
}

float3 RfxDirtLayer(float3 c, int k, float3 P, float3 N, float3 rp, float2 occ, float shadow, float4 fog)
{
    RfxSurf s = RfxSurfOf(k);
    float amount = _RfxDirt.x;
    if (amount < 0.004 || s.dirtAff <= 0.001) return c;
    float fade = saturate(1.0 - length(fwidth(rp * 85.0)) * 0.5);
    float height;
    float m = RfxDirtCoverage(rp, N, amount, s.dirtAff, s.dirtH, _RfxDirt.w, fade, height);
    float3 dpdx = ddx(P), dpdy = ddy(P); float dhx = ddx(height), dhy = ddy(height);   // (before the early out: derivatives need uniform flow)
    if (m <= 0.002) return c;
    float wet = _RfxDirt.y;
    float3 V = normalize(_WorldSpaceCameraPos - P);
    if (dot(N, V) < 0) N = -N;
    float unit = _RfxDirt.z > 0 ? _RfxDirt.z : 1.0;
    float3 Nd = RfxBumpFromHeight(dpdx * 1.0, dpdy * 1.0, dhx * 0.012 * unit, dhy * 0.012 * unit, N, 1.0);   // (a few millimetres of relief, in world units)
    float NoV = max(dot(Nd, V), 1e-3);
    float3 L = normalize(_RfxSunDir.xyz);
    float NoL = saturate(dot(Nd, L));
    float3 amb, sunK;
    RfxObjRig(P, Nd, amb, sunK);
    #if defined(_RFX_ENHANCED)
    amb = RfxEnhAmbient(amb, Nd, occ.x);
    #endif
    float aoAmb = lerp(1.0, occ.x, _RfxEnhParams.y);
    float3 sunC = _RfxSunColor.rgb * sunK * (shadow * occ.y);
    // the dirt itself: the level's mud colour; dust where thin, darker caked mud where thick; wet mud darker
    float3 mud = saturate(_RfxDirtColor.rgb * 1.9 + 0.07);
    float3 dust = saturate(_RfxDirtColor.rgb * 2.3 + 0.2);
    float thick = saturate(m * 1.1 + amount * 0.5);
    float3 alb = lerp(dust, mud, thick);
    alb *= lerp(0.8, 1.2, height);
    alb *= 1.0 - 0.3 * wet;
    float cav = lerp(0.62, 1.0, height);
    float3 lit = (amb * aoAmb + sunC * NoL) * cav;
    float3 dirtC = alb * lit;
    // a share of the original's colour shows through (the graphics never disappear)
    dirtC = lerp(dirtC, c * alb * 1.2, 0.12);
    // gloss: dry dirt is rough, wet mud shines
    float rough = lerp(0.9, 0.3, wet);
    float a = rough * rough, a2 = saturate(a * a + 0.02);
    float3 H = normalize(L + V);
    float NoH = saturate(dot(Nd, H)), VoH = saturate(dot(V, H));
    float3 F0 = float3(0.03, 0.03, 0.03);
    float3 sp = RfxFresnelSchlick(F0, VoH) * RfxGGX(NoH, a2) * RfxVisSmith(NoV, NoL, a2) * NoL * 3.14159265 * sunC;
    if (wet > 0.01) sp += RfxMatEnv(reflect(-V, Nd), rough) * RfxEnvBRDF(F0, rough, NoV) * wet * saturate(occ.x + 0.2) * 0.8;
    dirtC += sp;
    float3 dirtFinal = dirtC * fog.w + fog.rgb;
    return lerp(c, dirtFinal, m);
}

// every ENHANCED layer over the program's result; call only when _RfxMatOn > 0.5 and the material has a class
float4 RfxVehicleMaterial(float4 c, int k, float3 P, float3 N, float3 restPos, float2 occ, float shadow, bool front)
{
    float unit = _RfxDirt.z > 0 ? _RfxDirt.z : 1.0;                         // model units per metre
    float3 rp = restPos / unit;
    float4 fog = RfxFog(P, _WorldSpaceCameraPos);
    float alpha = c.a;
    N = normalize(N);
    c.rgb = RfxMatLayers(c.rgb, k, P, N, rp, occ, shadow, fog, front, alpha);
    if (_RfxDirtOn > 0.5) c.rgb = RfxDirtLayer(c.rgb, k, P, N, rp, occ, shadow, fog);
    c.a = alpha;
    return c;
}

#endif
