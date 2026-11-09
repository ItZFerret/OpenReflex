// The main-light shadow of a receiver with an early out (both LIGHTING modes; not part of the original renderer). Same result as URP's
// MainLightRealtimeShadow(shadowCoord), but where the shadow atlas around the shadow coordinate is all in front of or all behind the
// receiver it gives that answer without the soft filter's texture reads (up to 16 compares per pixel):
//   _RfxShadowRange (Scripts/Rendering/ReflexShadowRange.cs, Shaders/ReflexShadowRange.shader) holds per 8 x 8 atlas block the min / max
//   raw depth over the 3 x 3 blocks around it, i.e. over every texel the filter can read for a coordinate in the block. A receiver
//   nearer the light than all of them passes every compare, one farther than all of them fails every compare; a margin of 4 steps of
//   the 16-bit atlas keeps the test on the safe side of the hardware's compare precision.
//   All passing, the filter's value is the sum of its weights (RfxShadowAllLit: the same weights, added in the same order: 1 is not
//   what the filter returns, the weights of a tent sum to 1 only within rounding); all failing, it is 0. Then URP's own strength and
//   far-plane steps.
// Off (_RfxShadowRangeParams.x = 0: another camera, no shadow map): URP's function as it is. Needs Shadows.hlsl.
#ifndef REFLEX_SHADOWS_INCLUDED
#define REFLEX_SHADOWS_INCLUDED

TEXTURE2D_FLOAT(_RfxShadowRange);
float4 _RfxShadowRangeParams;   // x: on, y: the margin, zw: atlas size / 8 (blocks per row / column)

#if defined(MAIN_LIGHT_CALCULATE_SHADOWS) && !defined(_MAIN_LIGHT_SHADOWS_SCREEN)
// the value of URP's soft filters when every compare passes (SampleShadowmapFiltered*Quality with every SAMPLE_TEXTURE2D_SHADOW = 1)
real RfxShadowTent5x5AllLit(float4 shadowCoord, ShadowSamplingData samplingData)
{
    float w[9]; float2 uv[9];
    SampleShadow_ComputeSamples_Tent_Filter_5x5(float, samplingData.shadowmapSize, shadowCoord, w, uv);
    return w[0] + w[1] + w[2] + w[3] + w[4] + w[5] + w[6] + w[7] + w[8];
}
real RfxShadowTent7x7AllLit(float4 shadowCoord, ShadowSamplingData samplingData)
{
    float w[16]; float2 uv[16];
    SampleShadow_ComputeSamples_Tent_Filter_7x7(float, samplingData.shadowmapSize, shadowCoord, w, uv);
    return w[0] + w[1] + w[2] + w[3] + w[4] + w[5] + w[6] + w[7] + w[8] + w[9] + w[10] + w[11] + w[12] + w[13] + w[14] + w[15];
}
real RfxShadowAllLit(float4 shadowCoord, ShadowSamplingData samplingData, half4 shadowParams)
{
    real lit = real(1.0);                                    // hard: one compare; LOW: dot(1, 0.25) of four
#if defined(_SHADOWS_SOFT_MEDIUM)
    lit = RfxShadowTent5x5AllLit(shadowCoord, samplingData);
#elif defined(_SHADOWS_SOFT_HIGH)
    lit = RfxShadowTent7x7AllLit(shadowCoord, samplingData);
#elif defined(_SHADOWS_SOFT) && !defined(_SHADOWS_SOFT_LOW)
    if (shadowParams.y > SOFT_SHADOW_QUALITY_OFF && samplingData.softShadowQuality != SOFT_SHADOW_QUALITY_LOW)
        lit = samplingData.softShadowQuality == SOFT_SHADOW_QUALITY_MEDIUM ? RfxShadowTent5x5AllLit(shadowCoord, samplingData)
                                                                           : RfxShadowTent7x7AllLit(shadowCoord, samplingData);
#endif
    return lit;
}
#endif

half RfxMainLightShadow(float4 shadowCoord)
{
#if defined(MAIN_LIGHT_CALCULATE_SHADOWS) && !defined(_MAIN_LIGHT_SHADOWS_SCREEN)
    int known = 0; half value = half(1.0);   // 1: the answer without the filter
    [branch] if (_RfxShadowRangeParams.x > 0.5)
    {
        if (BEYOND_SHADOW_FAR(shadowCoord)) known = 1;
        else
        {
            int2 last = int2(ceil(_RfxShadowRangeParams.zw)) - 1;
            int2 b = clamp(int2(floor(shadowCoord.xy * _RfxShadowRangeParams.zw)), int2(0, 0), last);
            float2 range = LOAD_TEXTURE2D(_RfxShadowRange, b).rg;   // min, max of the raw depth
            float z = shadowCoord.z, m = _RfxShadowRangeParams.y;
        #if UNITY_REVERSED_Z
            bool allPass = z >= range.y + m, allFail = z < range.x - m;   // a compare passes where z >= the texel's depth
        #else
            bool allPass = z <= range.x - m, allFail = z > range.y + m;   // a compare passes where z <= the texel's depth
        #endif
            half4 shadowParams = GetMainLightShadowParams();
            if (allPass) { known = 1; value = half(LerpWhiteTo(RfxShadowAllLit(shadowCoord, GetMainLightShadowSamplingData(), shadowParams), shadowParams.x)); }
            else if (allFail) { known = 1; value = half(LerpWhiteTo(real(0.0), shadowParams.x)); }
        }
    }
    [branch] if (known == 0) value = MainLightRealtimeShadow(shadowCoord);
    return value;
#else
    return MainLightRealtimeShadow(shadowCoord);
#endif
}

#endif
