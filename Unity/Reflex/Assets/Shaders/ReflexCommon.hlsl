// Shared Reflex world-rendering globals and helpers (the original renderer's global shader constants, recovered from a
// recorded frame of the original renderer: _re/notes/render-pipeline.md). Set by ReflexLevelContent.ApplyShaderGlobals and
// ReflexRenderPipeline (Scripts/Rendering). Every world shader (terrain, scenery, sky, tracks, vehicles, riders, crowds,
// particles) should include this file and finish with RfxOutput().
//
// Colour: the original samples every texture without sRGB conversion, lights, fogs and blends the raw values in an FP16
// HDR target (A16B16G16R16F), tonemaps into an 8-bit target and presents it unchanged. ReflexRenderPipeline reproduces
// that in the linear project: world shaders write raw values into the camera's FP16 target (_RfxRawPipeline = 1), the
// Reflex post chain (Hidden/Reflex/Post) tonemaps and writes SRGBToLinear(ldr), so the sRGB back buffer shows the raw value.
// Without the pipeline (_RfxRawPipeline = 0: previews, scenes without ReflexRenderPipeline) RfxOutput converts each object.
#ifndef REFLEX_COMMON_INCLUDED
#define REFLEX_COMMON_INCLUDED

#include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Color.hlsl"

// --- level lighting (AmbientDirectionalLightRSData), vertex/pixel constants of the original ---
float4 _RfxSunDir;            // gSunLight c55: unit vector towards the sun = -(light matrix row 2)
float4 _RfxSunColor;          // gSunLightColor c56 = SunLightColor * SunLightColorIntensity (scenery, characters, vehicles)
float4 _RfxTerrainSunColor;   // c56 while drawing HFTerrain = gSunLightColor * 1.25 (constant 0x9c4068, FUN_007cd000)
float4 _RfxLightDir0, _RfxLightDir1, _RfxLightDir2, _RfxLightDir3;       // gBackLight c45, gSideLightA c47, gSideLightB c49, gFrontLight c51
float4 _RfxLightColor0, _RfxLightColor1, _RfxLightColor2, _RfxLightColor3; // c46, c48, c50, c52 (colour * intensity)
float4 _RfxBounceColor;       // gBounceLightColor c54
float4 _RfxColorRefSample;    // gColorRefSample c84 (static scenery: 0, 0, 0, 0)
float4 _RfxColorRef;          // texel of the ambient colour reference map (s13 via gColorRefProjection c57/c58; Editor_Default_White)
float4 _RfxSpecularColor;     // gSpecularLightColor (SpecularLightColor * intensity)
// --- distance fog (DistanceFogRSData): c14 gDistanceFog, c15 start (rgb, amount), c16 end (rgb, amount), c26 end - start ---
float4 _RfxFogColorStart;     // c15: ColorStart, AmountStart
float4 _RfxFogColorEnd;       // c16: ColorEnd, AmountEnd
float4 _RfxFogParams;         // DistanceStart, DistanceEnd, AmountStart, AmountEnd
float4 _RfxFogDistance;       // c14: DistanceStart, DistanceEnd, 1 / (End - Start), -Start / (End - Start)
// --- static volume shadow (ShadowMapRSData.VolumeShadowMapAsset, s12) ---
float4 _RfxVolumeShadowParams; // x: 1 / (grid extent), y: enabled, z: MAP SCALE (the world's scale; 0 = 1)
// MAP SCALE (core/plan/MAP_SCALE.md): the level's baked heights (units of 2048 file feet) times this are world heights
float RfxMapScale() { return _RfxVolumeShadowParams.z > 0.0 ? _RfxVolumeShadowParams.z : 1.0; }
// s13 as a map (Kingston's SX_KI_A_ColorRef; ReflexObjectShading.ApplyLevelGlobals): sampled at gColorRefProjection c57 / c58 =
// (k, 0, 0, 0.5), (0, 0, -k, 0.5) of the world position, i.e. (0.5 + k x, 0.5 + k z) in Unity space (v flipped as the uploads are);
// without a map the 1x1 texel _RfxColorRef.
TEXTURE2D(_RfxColorRefMap); SAMPLER(sampler_RfxColorRefMap);
float _RfxColorRefOn;
float4 RfxColorRefAt(float3 positionWS)
{
    if (_RfxColorRefOn < 0.5) return _RfxColorRef;
    float k = _RfxVolumeShadowParams.x;
    return SAMPLE_TEXTURE2D_LOD(_RfxColorRefMap, sampler_RfxColorRefMap, float2(0.5 + positionWS.x * k, 0.5 + positionWS.z * k), 0);
}
TEXTURE2D(_RfxVolumeShadowMap); SAMPLER(sampler_RfxVolumeShadowMap);
// --- pipeline ---
float _RfxRawPipeline;        // 1: ReflexRenderPipeline tonemaps the raw HDR target (write raw values)

// Final colour of a world shader: raw (display-referred) value -> the camera target.
float3 RfxOutput(float3 raw)
{
    if (_RfxRawPipeline > 0.5) return raw;
#if !defined(UNITY_COLORSPACE_GAMMA)
    return SRGBToLinear(max(raw, 0.0));
#else
    return raw;
#endif
}

// Per-vertex distance fog of every original vertex program (Template_* 05a4..06a0, vs_hft_lod0 0944..0a78):
//   f = sat(dist * c14.z + c14.w); amount = clamp(f * c26.w + c15.w, c15.w, c16.w)
//   k = sat(dot(normalize(sun.x, (dirToCam.y + sun.y) / 2, sun.z), dirToCam) + 1); colour = c15.rgb + c26.rgb * k
// returns (amount * colour, 1 - amount): out = c * fog.w + fog.rgb
float4 _RfxAlive;               // x: Alive level, y: SideLightsEnabled, z: BounceLightEnabled, w: bounce light volume bound
float4 RfxFog(float3 positionWS, float3 cameraWS)
{
    float3 toCam = cameraWS - positionWS;
    float dist = length(toCam);
    float3 dirToCam = toCam * rsqrt(max(dot(toCam, toCam), 1e-20));
    float f = saturate(dist * _RfxFogDistance.z + _RfxFogDistance.w);
    float amount = clamp(f * (_RfxFogColorEnd.w - _RfxFogColorStart.w) + _RfxFogColorStart.w, _RfxFogColorStart.w, _RfxFogColorEnd.w);
    float3 s = normalize(float3(_RfxSunDir.x, (dirToCam.y + _RfxSunDir.y) * 0.5, _RfxSunDir.z));
    float k = saturate(dot(s, dirToCam) * 0.5 + 0.5);
    k = saturate(k + k);
    // MX vs ATV Alive's static vertex programs (T_Diffuse StaticVertexShader: mad r0 = f * c26 + c15): the colour and the amount both ramp with the
    // distance factor f; there is no view-to-sun term
    if (_RfxAlive.x > 0.5) k = f;
    float3 colour = _RfxFogColorStart.rgb + (_RfxFogColorEnd.rgb - _RfxFogColorStart.rgb) * k;
    return float4(amount * colour, 1.0 - amount);
}

// The fill-light rig (vertex programs, Template_Diffuse_AO 06b0..07b4): four saturated N.L terms plus the bounce term
// sat(-N.y) * lerp(gColorRefSample.rgb, gBounceLightColor, gColorRefSample.w). N: world normal, not normalised.
float3 RfxAmbient(float3 N)
{
    float3 a = saturate(dot(_RfxLightDir1.xyz, N)) * _RfxLightColor1.rgb;
    a += saturate(dot(_RfxLightDir0.xyz, N)) * _RfxLightColor0.rgb;
    a += saturate(dot(_RfxLightDir2.xyz, N)) * _RfxLightColor2.rgb;
    a += saturate(dot(_RfxLightDir3.xyz, N)) * _RfxLightColor3.rgb;
    float3 bounce = _RfxColorRefSample.rgb + _RfxColorRefSample.w * (_RfxBounceColor.rgb - _RfxColorRefSample.rgb);
    return a + saturate(-N.y) * bounce;
}

// Radiosity factor of the colour reference map (pixel programs 04d8..0534): floor(ref.a) ? ref.rgb : 1.
float3 RfxRadiosity()
{
    return lerp(float3(1, 1, 1), _RfxColorRef.rgb, floor(_RfxColorRef.a));
}
float3 RfxRadiosityAt(float3 positionWS)
{
    float4 r = RfxColorRefAt(positionWS);
    return lerp(float3(1, 1, 1), r.rgb, floor(r.a));
}

// Static volume shadow (04a4..04b4): sat(1 - 0.25 * (2048 * H(xz) - y - 0.1)); H in the terrain UV space (gWorldProjectionXZY).
float RfxVolumeShadow(float3 positionWS)
{
    if (_RfxVolumeShadowParams.y < 0.5) return 1.0;
    float2 suv = float2(0.5 + positionWS.x * _RfxVolumeShadowParams.x, 0.5 + positionWS.z * _RfxVolumeShadowParams.x);
    float h = SAMPLE_TEXTURE2D_LOD(_RfxVolumeShadowMap, sampler_RfxVolumeShadowMap, suv, 0).r * (2048.0 * RfxMapScale());
    return saturate(1.0 - 0.25 * (h - positionWS.y - 0.1));
}

// --- MX vs ATV Alive (Content/AliveLevels.cs AliveBounce) ---
// Alive's programs square their albedo ((tex * vertex colour)^2: T_Diffuse*, HFB / HFTerrain_vtex*) and multiply it by a deferred light
// buffer: DL_AmdDirLight's sun (N.L x shadow), its fill terms only when the level enables them, and DL_BounceLight's volume.
float4 _RfxAliveBounceCentre, _RfxAliveBounceHalf, _RfxAliveBounceDims;   // box centre / half extents; dims.xyz grid, w LightScale
TEXTURE3D(_RfxAliveBounceO); TEXTURE3D(_RfxAliveBounceS);
TEXTURE3D(_RfxAliveBounceX); TEXTURE3D(_RfxAliveBounceY); TEXTURE3D(_RfxAliveBounceZ);
#ifndef RFX_ALIVE_SAMPLER
SAMPLER(sampler_linear_clamp);
#define RFX_ALIVE_SAMPLER sampler_linear_clamp
#endif

// The level's baked sun mask (MX vs ATV Alive AmbientDirectionalLight.MapShadow, DirectionShadowMaskMapPS): the green channel of a map in the
// volume shadow map's world projection multiplies the sun. 1 where the level has none.
float4 _RfxMapShadowParams;     // x: bound, y: 1 / (grid extent)
TEXTURE2D(_RfxMapShadowMap);
float RfxMapShadow(float3 positionWS)
{
    if (_RfxMapShadowParams.x < 0.5) return 1.0;
    float2 suv = float2(0.5 + positionWS.x * _RfxMapShadowParams.y, 0.5 + positionWS.z * _RfxMapShadowParams.y);
    return SAMPLE_TEXTURE2D_LOD(_RfxMapShadowMap, RFX_ALIVE_SAMPLER, suv, 0).r;
}

bool RfxIsAlive() { return _RfxAlive.x > 0.5; }
float3 RfxAliveAlbedo(float3 c) { return _RfxAlive.x > 0.5 ? c * c : c; }
// ALIVE LOOK (Content/AliveLook.cs): _RfxAlive.x is Alive's rendering (the ALIVE look); _RfxAliveData.x marks an Alive level's content in either
// look (its vertex / program encodings: grass heights, tree occlusion), y the REFLEX look. _RfxSkyGamma: the sky's exponent (1; 0 = unset = 1).
float4 _RfxAliveData;
float _RfxSkyGamma;
bool RfxIsAliveData() { return _RfxAliveData.x > 0.5 || _RfxAlive.x > 0.5; }
// The forest's textures on an MX vs ATV Alive level (leaf / frond atlases, bark, the billboard atlas), in either look (w-fb-vegetation):
// Alive's tree programs square their texel (T_EcoLeaves / T_EcoBranches / T_EcoBark: (tex x NORMAL.w)^2; the billboard atlas the same
// way, see ReflexTreeBillboard), and its forest art is made for that: unsquared the atlases average 0.24 .. 0.53 (leaves), 0.41 .. 0.46
// (bark), 0.24 .. 0.28 (billboards), against Reflex's own tree textures 0.10 .. 0.18, 0.17 .. 0.36 and 0.12 .. 0.15 (its other scenery
// is in the same range in both games: 0.33 .. 0.39). Reflex's tree programs, which the REFLEX look draws them with, were made for
// Reflex's dark atlases, so the Alive forest was drawn 2.5 - 3 x too bright there. Squared, the atlases come to Reflex's range.
float3 RfxTreeAlbedo(float3 c) { return RfxIsAliveData() ? c * c : c; }
float3 RfxSkyGamma(float3 c) { return _RfxSkyGamma > 0.0 && abs(_RfxSkyGamma - 1.0) > 1e-4 ? pow(max(c, 0.0), _RfxSkyGamma) : c; }

// DL_BounceLight BounceLightGridPS: the voxel's offset / scale (point: Load) place the position inside its brick of the X / Y / Z
// atlases; each axis samples its negative (u) or positive (u + 0.5) half by the normal's sign; weights |N| / (|N.x| + |N.y| + |N.z|).
float3 RfxAliveBounce(float3 P, float3 N)
{
    float3 uvw = float3((P.x - _RfxAliveBounceCentre.x) / (2.0 * _RfxAliveBounceHalf.x) + 0.5,
                        (P.z - _RfxAliveBounceCentre.z) / (2.0 * _RfxAliveBounceHalf.z) + 0.5,
                        (P.y - _RfxAliveBounceCentre.y) / (2.0 * _RfxAliveBounceHalf.y) + 0.5);
    uvw = min(saturate(uvw), 0.9999);
    float3 g = uvw * _RfxAliveBounceDims.xyz;
    int3 cell = int3(floor(g));
    float3 o = LOAD_TEXTURE3D(_RfxAliveBounceO, cell).xyz;
    float3 sc = LOAD_TEXTURE3D(_RfxAliveBounceS, cell).xyz;
    float3 a = frac(g) * sc + o;
    float3 n = N * rsqrt(max(dot(N, N), 1e-12));
    float3 pos = step(0.0, n) * 0.5;
    float3 x = SAMPLE_TEXTURE3D_LOD(_RfxAliveBounceX, RFX_ALIVE_SAMPLER, float3(a.x + pos.x, a.y, a.z), 0).rgb;
    float3 y = SAMPLE_TEXTURE3D_LOD(_RfxAliveBounceY, RFX_ALIVE_SAMPLER, float3(a.x + pos.y, a.y, a.z), 0).rgb;
    float3 z = SAMPLE_TEXTURE3D_LOD(_RfxAliveBounceZ, RFX_ALIVE_SAMPLER, float3(a.x + pos.z, a.y, a.z), 0).rgb;
    float3 w = abs(n) / max(abs(n.x) + abs(n.y) + abs(n.z), 1e-5);
    return (w.x * x + w.y * y + w.z * z) * _RfxAliveBounceDims.w;
}

// The ambient part of Alive's light buffer (everything but the sun): fill lights when enabled (DirectionLightPS: sat(N.L_i) C_i), the
// bounce colour from below when enabled (sat(-N.y) C_bounce), the bounce light volume. N: world normal.
float3 RfxAliveAmbient(float3 N, float3 P)
{
    float3 n = N * rsqrt(max(dot(N, N), 1e-12));   // a degenerate normal must not make the light buffer NaN
    float3 a = 0;
    if (_RfxAlive.y > 0.5)
        a += saturate(dot(_RfxLightDir0.xyz, n)) * _RfxLightColor0.rgb + saturate(dot(_RfxLightDir1.xyz, n)) * _RfxLightColor1.rgb
           + saturate(dot(_RfxLightDir2.xyz, n)) * _RfxLightColor2.rgb + saturate(dot(_RfxLightDir3.xyz, n)) * _RfxLightColor3.rgb;
    if (_RfxAlive.z > 0.5) a += saturate(-n.y) * _RfxBounceColor.rgb;
    if (_RfxAlive.w > 0.5) a += RfxAliveBounce(P, n);
    return a;
}
// The ambient a Reflex shader computed (fill rig), or Alive's light buffer ambient on an Alive level.
float3 RfxAmbientAt(float3 reflexAmbient, float3 N, float3 P) { return _RfxAlive.x > 0.5 ? RfxAliveAmbient(N, P) : reflexAmbient; }

#endif
