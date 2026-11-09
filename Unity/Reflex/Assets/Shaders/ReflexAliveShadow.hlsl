#ifndef REFLEX_ALIVE_SHADOW_INCLUDED
#define REFLEX_ALIVE_SHADOW_INCLUDED
// MX vs ATV Alive's dynamic shadow on the forest (DL_AmdDirLight's shadow mask: one fetch of a variance shadow map per pixel). Unity's
// main-light shadow map stands in for it with one hardware compare (no soft-shadow taps: a forest's leaf cards and billboards are
// drawn in many layers), and nothing is sampled where the shadow has faded out (the far forest). Needs Shadows.hlsl.
float RfxAliveDynShadow(float3 P)
{
#if defined(_MAIN_LIGHT_SHADOWS) || defined(_MAIN_LIGHT_SHADOWS_CASCADE) || defined(_MAIN_LIGHT_SHADOWS_SCREEN)
    half fade = GetMainLightShadowFade(P);
    [branch] if (fade >= 0.999h) return 1.0;
    float4 sc = TransformWorldToShadowCoord(P);
    float a = SAMPLE_TEXTURE2D_SHADOW(_MainLightShadowmapTexture, sampler_LinearClampCompare, sc.xyz);
    a = LerpWhiteTo(a, GetMainLightShadowParams().x);
    a = BEYOND_SHADOW_FAR(sc) ? 1.0 : a;
    return lerp(a, 1.0, fade);
#else
    return 1.0;
#endif
}
#endif
