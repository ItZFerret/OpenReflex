// LIGHTING: ENHANCED (OPTIONS > VIDEO): the shared inputs of the modern lighting mode (Scripts/Rendering/ReflexEnhancedLighting.cs).
// Nothing here is part of the original renderer: every world shader only reads these under the global keyword _RFX_ENHANCED, so the
// ORIGINAL mode compiles and draws exactly the original programs.
//   _RfxScreenOcclusion   r: screen-space ambient occlusion, g: contact shadow (ReflexOcclusionPass, from the depth pre-pass)
//   _RfxEnhSH*            the enhanced sky's irradiance as order-2 spherical harmonics (ReflexEnhancedSky), normalised by its mean
//                         luminance: RfxEnhSkyTint(N) is a colour ratio around 1 (sky colour and direction, not brightness)
//   _RfxEnhSkyCube        the enhanced sky as a mip-mapped cube (reflections), raw HDR units like the visible sky
// Include after URP's Core.hlsl and ReflexCommon.hlsl.
#ifndef REFLEX_ENHANCED_INCLUDED
#define REFLEX_ENHANCED_INCLUDED
#include "ReflexWeather.hlsl"   // WEATHER: the overcast sky in the reflections and the ambient (nothing without weather: _RfxWeather.w = 0)

TEXTURE2D(_RfxScreenOcclusion);
#ifndef RFX_ENH_OCC_SAMPLER
SAMPLER(sampler_RfxScreenOcclusion);
#define RFX_ENH_OCC_SAMPLER sampler_RfxScreenOcclusion   // a shader at its sampler limit (the terrain: 16) names one of its own instead
#endif
TEXTURECUBE(_RfxEnhSkyCube);
#ifndef RFX_ENH_SKY_SAMPLER
SAMPLER(sampler_RfxEnhSkyCube);
#define RFX_ENH_SKY_SAMPLER sampler_RfxEnhSkyCube   // (the terrain, at its sampler limit, names one of its own)
#endif
float4 _RfxEnhSHAr, _RfxEnhSHAg, _RfxEnhSHAb, _RfxEnhSHBr, _RfxEnhSHBg, _RfxEnhSHBb, _RfxEnhSHC;
float4 _RfxEnhParams;     // x: sky ambient weight, y: AO strength on ambient, z: AO strength on the whole colour (programs we cannot split), w: occlusion on (1)
float4 _RfxEnhParams2;    // x: reflection strength, y: Fresnel F0, z: sky cube max mip, w: sky cube on (1)

// AO (r) and contact shadow (g) of this pixel; (1, 1) when the occlusion pass did not run for this camera
float2 RfxEnhOcclusion(float4 positionCS)
{
    if (_RfxEnhParams.w < 0.5) return float2(1, 1);
    float2 uv = GetNormalizedScreenSpaceUV(positionCS);
    return SAMPLE_TEXTURE2D_LOD(_RfxScreenOcclusion, RFX_ENH_OCC_SAMPLER, uv, 0).rg;
}

// order-2 SH irradiance (Unity's packed layout, as SampleSH9)
float3 RfxEnhSH(float3 N)
{
    float4 n = float4(N, 1);
    float3 x1 = float3(dot(_RfxEnhSHAr, n), dot(_RfxEnhSHAg, n), dot(_RfxEnhSHAb, n));
    float4 vB = n.xyzz * n.yzzx;
    float3 x2 = float3(dot(_RfxEnhSHBr, vB), dot(_RfxEnhSHBg, vB), dot(_RfxEnhSHBb, vB));
    float vC = n.x * n.x - n.y * n.y;
    return max(x1 + x2 + _RfxEnhSHC.rgb * vC, 0);
}

// the original ambient term, coloured and shaped by the sky that surrounds the level (brightness kept: the ratio averages to 1)
float3 RfxEnhAmbient(float3 ambient, float3 N, float ao)
{
    float3 tint = lerp(float3(1, 1, 1), RfxEnhSH(normalize(N)), _RfxEnhParams.x);
    if (_RfxWeather.w > 0.5) tint = lerp(tint, float3(0.97, 0.99, 1.03), _RfxWeather.z * 0.8);   // WEATHER: the light of a grey sky
    return ambient * tint * lerp(1.0, ao, _RfxEnhParams.y);
}

// the sky cube in direction R at a roughness 0..1
float3 RfxEnhSkyReflection(float3 R, float roughness)
{
    if (_RfxEnhParams2.w < 0.5) return 0;
    float3 c = SAMPLE_TEXTURECUBE_LOD(_RfxEnhSkyCube, RFX_ENH_SKY_SAMPLER, R, roughness * _RfxEnhParams2.z).rgb;
    [branch] if (_RfxWeather.w > 0.5) c = lerp(c, RfxWeatherSkyColour(float3(R.x, abs(R.y), R.z), roughness < 0.15 ? 1.0 : 0.0), _RfxWeather.z);   // WEATHER
    return c;
}

// WEATHER: what a wet surface / water reflects in direction R (raw): the sky (with the overcast deck as the clouds close in); the level's
// fog colour under the deck where there is no enhanced sky (stadiums, night). roughness 0 (water) .. 1.
float3 RfxWeatherReflection(float3 R, float roughness)
{
    R.y = abs(R.y);   // (the ground mirrors the sky above it)
    if (_RfxEnhParams2.w > 0.5) return RfxEnhSkyReflection(R, roughness);
    return lerp(_RfxWeatherFog.rgb, RfxWeatherSkyColour(R, roughness < 0.15 ? 1.0 : 0.0), _RfxWeather.z);
}

// Schlick's Fresnel term with the mode's F0
float RfxEnhFresnel(float3 N, float3 V)
{
    float c = 1.0 - saturate(dot(N, V));
    float c2 = c * c;
    return _RfxEnhParams2.y + (1.0 - _RfxEnhParams2.y) * c2 * c2 * c;
}

#endif
