// LIGHTING: ENHANCED > VOLUMETRIC LIGHT on the transparent objects (Scripts/Rendering/ReflexVolumetricLight.cs; not part of the original renderer).
// The volumetric pass (Shaders/Resources/ReflexVolumetric.shader) marches each pixel's ray to the opaque depth and composites the result onto
// the scene before the transparent queue, so anything blended (particles, glows, grass, blended scenery / vehicle materials, video screens)
// would draw over the haze untouched. Those shaders call RfxVolApply on their final colour instead: the march's result at the pixel (in-
// scattered light L and transmittance T up to the opaque surface behind, distance D) is cut back to the fragment's own distance d by the
// share of the ray's optical depth in front of it - the height fog's analytic integral along the ray (exp(-k (y - y0))), so a fragment
// half way to the opaque surface gets the haze of that half - T_d = T^f, L_d = L (1 - T_d) / (1 - T). Then out = c T_d + L_d, which
// alpha-blended over the (already fogged) scene is the medium in front of the fragment, the fragment, the medium behind it.
// Additive blends only take the transmittance (the haze in front does not add once per layer). Cost: one depth fetch, one bilinear fetch
// of the half / quarter resolution result, a few exp per transparent fragment; nothing when the pass is off (_RfxVolApply.x = 0).
// Include after URP's Core.hlsl.
#ifndef REFLEX_VOLUMETRIC_APPLY_INCLUDED
#define REFLEX_VOLUMETRIC_APPLY_INCLUDED

#include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/DeclareDepthTexture.hlsl"

TEXTURE2D(_RfxVolResult);    // the march after the temporal pass (low resolution): rgb in-scattered (raw HDR), a transmittance
// (URP's global sampler; the scene depth is loaded without one. A shader at its sampler limit defines RFX_VOL_NO_SAMPLER: the nearest texel
// of the result is loaded instead - the medium is smooth, the low resolution's blocks only show along the shafts' edges)
#if !defined(RFX_VOL_SAMPLER) && !defined(RFX_VOL_NO_SAMPLER)
#define RFX_VOL_SAMPLER sampler_LinearClamp
#endif
float4 _RfxVolApply;         // x on (this camera ran the pass), y full pixels per low pixel, zw 1 / low size
float4 _RfxVolApply2;        // x height falloff (1/m), y the march's max distance (m), zw full size

// (in-scattered rgb, transmittance) between the camera and P; (0, 0, 0, 1) when the pass is off. positionCS: the fragment's SV_POSITION.
float4 RfxVolAt(float3 P, float4 positionCS)
{
    if (_RfxVolApply.x < 0.5) return float4(0, 0, 0, 1);
    float2 uv = GetNormalizedScreenSpaceUV(positionCS);
    // the march's pixel k stands for the full-resolution texel k x scale (ReflexVolumetric.shader FullUV)
    float2 hc = (uv * _RfxVolApply2.zw - 0.5) / _RfxVolApply.y;
    #if defined(RFX_VOL_NO_SAMPLER)
    float4 vol = LOAD_TEXTURE2D(_RfxVolResult, int2(clamp(floor(hc + 0.5), 0.0, 1.0 / _RfxVolApply.zw - 1.0)));
    #else
    float4 vol = SAMPLE_TEXTURE2D_LOD(_RfxVolResult, RFX_VOL_SAMPLER, (hc + 0.5) * _RfxVolApply.zw, 0);
    #endif
    float T = saturate(vol.a);
    if (T > 0.9995) return float4(0, 0, 0, 1);

    float3 toP = P - _WorldSpaceCameraPos;
    float d = length(toP);
    float raw = LoadSceneDepth(uint2(positionCS.xy));
    #if UNITY_REVERSED_Z
    bool sky = raw <= 1e-7;
    #else
    bool sky = raw >= 1.0 - 1e-7;
    #endif
    float dEye = max(LinearEyeDepth(P, UNITY_MATRIX_V), 1e-4);
    float D = sky ? 1e6 : d * LinearEyeDepth(raw, _ZBufferParams) / dEye;   // (the same ray: distances scale as eye depths)
    float maxD = _RfxVolApply2.y;
    float de = min(d, maxD), De = min(D, maxD);
    float f = 1.0;
    if (de < De)
    {
        // the height fog's optical depth to t along the ray: (1 - exp(-a t)) / a, a = k dir.y (stable for either sign)
        float b = abs(_RfxVolApply2.x * toP.y / max(d, 1e-4));
        if (b * De < 1e-3) f = de / De;
        else
        {
            float r = (1.0 - exp(-b * de)) / (1.0 - exp(-b * De));
            f = toP.y >= 0.0 ? r : exp(-b * (De - de)) * r;
        }
    }
    float Td = pow(max(T, 1e-4), f);
    float3 L = max(vol.rgb, 0.0) * ((1.0 - T) > 1e-3 ? (1.0 - Td) / (1.0 - T) : f);
    return float4(L, Td);
}

// The fragment's colour (raw, before RfxOutput) with the medium in front of it, by the blend it is drawn with (UnityEngine.Rendering.BlendMode:
// 0 Zero, 1 One, 2 DstColor, 3 SrcColor, 10 OneMinusSrcAlpha). a: the fragment's alpha.
float3 RfxVolApply(float3 c, float a, float3 P, float4 positionCS, float srcBlend, float dstBlend)
{
    if (_RfxVolApply.x < 0.5) return c;
    float4 v = RfxVolAt(P, positionCS);
    if (dstBlend > 0.5 && dstBlend < 1.5) return c * v.a;                                                  // additive: dst + c (x a)
    if ((srcBlend > 1.5 && srcBlend < 2.5 && dstBlend < 0.5) || (srcBlend < 0.5 && dstBlend > 2.5 && dstBlend < 3.5))
        return lerp(float3(1, 1, 1), c, v.a);                                                              // multiplicative: towards 1
    if (srcBlend > 0.5 && srcBlend < 1.5 && dstBlend > 9.5 && dstBlend < 10.5) return c * v.a + v.rgb * a;  // premultiplied
    return c * v.a + v.rgb;                                                                                 // alpha blended / opaque
}

#endif
