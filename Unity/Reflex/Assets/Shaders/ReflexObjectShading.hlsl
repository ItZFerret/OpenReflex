// Shared inputs of the original object programs (vehicles, rider gear, characters, crowds: Shaders/ReflexVehicle.shader and the
// generated program library Shaders/ReflexObjectPrograms.hlsl). The programs are the original vertex / pixel programs of the
// materials' shader assets, register for register (_re/notes/object-shading.md); this file gives them their registers:
//   v-registers   the vertex program's inputs (D3D: missing components 0, w 1; UV rows top-down: v_d3d = 1 - v_unity)
//   o / t         the interpolators by D3D usage index (TEXCOORD0..9)
//   c-registers   the global constants of the original renderer (ReflexCommon.hlsl, render-pipeline.md section 3), the
//                 material's usr_* parameters (by CTAB name, material properties) and the program's own literals
//   s0..s6        the material's textures in sampler order (_Tex0.._Tex6; cubes _Cube3 / _Cube4)
//   s12           the static volume shadow map (ShadowMapRSData), s13 the colour reference map (AmbientDirectionalLight
//                 _ColorReferenceMap: Editor_Default_White on most levels, SX_KI_A_ColorRef on Kingston),
//   s14           the level's reflection map (samplerReflectionMap), s15 the dynamic shadow moments (VSM, see RfxVsmTexel)
#ifndef REFLEX_OBJECT_SHADING_INCLUDED
#define REFLEX_OBJECT_SHADING_INCLUDED

#include "ReflexCommon.hlsl"

struct RfxVsIn { float4 position, normal, tangent, uv0, uv1, color; };
struct RfxV2F
{
    float4 pos : SV_POSITION;
    float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 t2 : TEXCOORD2; float4 t3 : TEXCOORD3; float4 t4 : TEXCOORD4;
    float4 t5 : TEXCOORD5; float4 t6 : TEXCOORD6; float4 t7 : TEXCOORD7; float4 t8 : TEXCOORD8;
    // MATERIALS: ENHANCED (ReflexVehiclePBR.hlsl; not the original's): t9.xyz the world normal, t10.xyz the vertex's rest-pose position
    float4 t9 : TEXCOORD9; float4 t10 : TEXCOORD10;
};

// --- the original's dynamic shadows (render-pipeline.md section 5): two light projections, the plane choosing between them,
// the filtered moments (mean, variance) of the casters' depth. _RfxVsmOn = 1: the original's maps (ReflexObjectShading / tests);
// 0: Unity's main-light shadow stands in, fed through the programs' own Chebyshev term (RfxVsmTexel).
float4 _RfxShadowProj1[4];      // gShadowProjectionMatrix1 c17..c20 (near, perspective)
float4 _RfxShadowProj2[4];      // gShadowProjectionMatrix2 c21..c24 (far, orthographic)
float4 _RfxShadowPlane;         // gShadowSeparatingPlane c25
float _RfxVsmOn;
TEXTURE2D(_RfxVsmMap); SAMPLER(sampler_RfxVsmMap);   // G16R16 moments, 768 x 1024 (near rows 0..383, far 384..1023), point / clamp
// --- the level's reflection map (s14, samplerReflectionMap: e.g. NAT_CO_ReflectionMap, an FP16 cube)
TEXTURECUBE(_RfxReflectionCube); SAMPLER(sampler_RfxReflectionCube);
float _RfxReflectionOn;

static float rfxShadowAtten = 1;   // Unity's main-light shadow at this pixel (the stand-in), set by the fragment before the program

// Without the original's maps: the projections give every pixel the depth 0.5 at (0.5, 0.5) of the near map and keep the far-edge
// fade off; RfxVsmTexel then returns moments whose Chebyshev bound is Unity's shadow attenuation: mean = -0.5, variance = a / (1 - a).
float4 RfxShadowProj1(int k) { return _RfxVsmOn > 0.5 ? _RfxShadowProj1[k] : (k == 3 ? float4(0, 0, 0, 1) : float4(0, 0, 0, 0.5)); }
float4 RfxShadowProj2(int k) { return _RfxVsmOn > 0.5 ? _RfxShadowProj2[k] : (k == 1 || k == 3 ? float4(0, 0, 0, 1) : float4(0, 0, 0, 0.5)); }
float4 RfxShadowPlane() { return _RfxVsmOn > 0.5 ? _RfxShadowPlane : float4(0, 0, 0, 1); }
float4 RfxVsmTexel(float2 uv)
{
    if (_RfxVsmOn > 0.5) return float4(SAMPLE_TEXTURE2D_LOD(_RfxVsmMap, sampler_RfxVsmMap, float2(uv.x, 1.0 - uv.y), 0).rg, 1, 1);
    float a = saturate(rfxShadowAtten);
    return a > 0.9999 ? float4(2, 0, 1, 1) : float4(-0.5, a / (1.0 - a), 1, 1);
}

// gWorldProjectionXZY c39 / c40 = gColorRefProjection c57 / c58 = (k, 0, 0, 0.5), (0, 0, -k, 0.5), k = 1 / (grid size x spacing)
float4 RfxWorldProj0() { return float4(_RfxVolumeShadowParams.x, 0, 0, 0.5); }
float4 RfxWorldProj1() { return float4(0, 0, -_RfxVolumeShadowParams.x, 0.5); }
// s12: the volume shadow height (R32F); no map: a height far below everything (the programs' sat(1 - 0.25 (2048 h - y - 0.1)) = 1)
static float3 rfxObjPosWS = 0;   // the fragment's world position (every program's o0 = t0), set by the fragment before the program
float4 RfxVolumeShadowTexel(float2 uv)
{
    float h = _RfxVolumeShadowParams.y < 0.5 ? -1e4 : SAMPLE_TEXTURE2D_LOD(_RfxVolumeShadowMap, sampler_RfxVolumeShadowMap, float2(uv.x, 1.0 - uv.y), 0).r * RfxMapScale();   // MAP SCALE: a world height
    // MX vs ATV Alive: DL_AmdDirLight multiplies the sun by volume x MapShadow x dynamic for every pixel it lights, objects included
    // (DirectionShadowMaskMapPS). Every one of the library's programs turns this height into its volume term sat(1 - 0.25 (2048 h - y - 0.1))
    // (107 of 107 texture reads, checked when this was written) and multiplies the sun by it, so the map's value goes in as the height
    // that gives vol x map: 2048 h' = y + 0.1 + 4 (1 - vol x map).
    if (_RfxAlive.x > 0.5 && _RfxMapShadowParams.x > 0.5)
    {
        float y = rfxObjPosWS.y;
        float vol = saturate(1.0 - 0.25 * (h * 2048.0 - y - 0.1));
        h = (y + 0.1 + 4.0 * (1.0 - vol * RfxMapShadow(rfxObjPosWS))) / 2048.0;
    }
    return float4(h, 0, 0, 1);
}
// s13: the level's colour reference map at gColorRefProjection (the 1x1 constant _RfxColorRef without a map)
// _RfxColorRefMap / _RfxColorRefOn: ReflexCommon.hlsl
float4 RfxColorRefTexel(float2 uv)
{
    if (_RfxColorRefOn < 0.5) return _RfxColorRef;
    return SAMPLE_TEXTURE2D(_RfxColorRefMap, sampler_RfxColorRefMap, float2(uv.x, 1.0 - uv.y));
}
// s14: the level's reflection cube (black without one, as an unbound D3D sampler)
static float rfxReflScale = 1;   // MATERIALS: ENHANCED scales the programs' own environment reflection (1 = the original's) where it adds its own
float4 RfxReflection(float3 dir, float bias)
{
    if (_RfxReflectionOn < 0.5) return 0;
    return SAMPLE_TEXTURECUBE_BIAS(_RfxReflectionCube, sampler_RfxReflectionCube, dir, bias) * rfxReflScale;
}

// D3D9 instruction semantics (as swr.cpp executes them)
float2 RfxUV(float2 d3d) { return float2(d3d.x, 1.0 - d3d.y); }   // D3D texture coordinates -> Unity's (rows bottom-up)
float RfxRcp(float x) { return x == 0 ? 1e38 * 1e38 : 1.0 / x; }
float RfxRsq(float x) { x = abs(x); return x == 0 ? 1e38 * 1e38 : rsqrt(x); }
float RfxLog(float x) { return log2(abs(x)); }
float RfxPow(float x, float y) { return exp2(log2(abs(x)) * y); }
float3 RfxNrm(float3 v) { float d = dot(v, v); return d == 0 ? float3(0, 0, 0) : v * rsqrt(d); }
float RfxCmp(float a, float b, float c) { return a >= 0 ? b : c; }
float2 RfxCmp(float2 a, float2 b, float2 c) { return float2(a.x >= 0 ? b.x : c.x, a.y >= 0 ? b.y : c.y); }
float3 RfxCmp(float3 a, float3 b, float3 c) { return float3(a.x >= 0 ? b.x : c.x, a.y >= 0 ? b.y : c.y, a.z >= 0 ? b.z : c.z); }
float4 RfxCmp(float4 a, float4 b, float4 c) { return float4(a.x >= 0 ? b.x : c.x, a.y >= 0 ? b.y : c.y, a.z >= 0 ? b.z : c.z, a.w >= 0 ? b.w : c.w); }

#endif
