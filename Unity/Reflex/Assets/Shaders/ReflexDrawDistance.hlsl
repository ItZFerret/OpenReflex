// DRAW DISTANCE (OPTIONS > VIDEO; Scripts/Rendering/ReflexDrawDistance.cs). ORIGINAL: _RfxDrawDist = (1, 0, 0, 0) and every function here
// returns what the original program computes. HIGH / ULTRA:
//   x   1 / TreeScale: the forest's depths (LOD dissolve, billboard fade) are divided by the tree scale (the trees reach that much further)
//   y   1: those depths are the distance from the camera, not the depth along its view direction (the depth changes when the camera only
//       turns, so a tree near a threshold flipped when the rider steered)
//   z   1: the scenery's draw-distance fade is a dither on opaque materials (they keep casting their shadow and need no sorting)
#ifndef REFLEX_DRAW_DISTANCE_INCLUDED
#define REFLEX_DRAW_DISTANCE_INCLUDED

float4 _RfxDrawDist;

// the forest depth of the offset (point - camera) for a camera looking along fwd
float RfxForestDepth(float3 offset, float3 fwd)
{
    float viewDepth = dot(offset, fwd);
    return lerp(viewDepth, length(offset), _RfxDrawDist.y) * _RfxDrawDist.x;
}

// interleaved gradient noise (Jimenez) at a pixel: the dither threshold in [0, 1)
float RfxDitherNoise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

// MX vs ATV Alive's foliage alpha (w-alivetrees, OriginalForest sets it per camera): T_EcoLeaves / T_EcoBranches (normal and colour pass) and
// TreeShader BBPixelShader keep a pixel where coverage - g_ScreenNoise > 32 / 255, the noise a per-pixel texture tiled over the screen (the
// vertex programs' (x / w * 10 + 10, y / w * -5.625 + 5.625): one texel per pixel at 1280 x 720), so the leaf atlases' soft alpha and every
// fade (leaf LODs, 3D to billboard) are a screen-door dissolve instead of a hard alpha test.
//   x: 1 on the levels with Alive's content, 0 elsewhere (Reflex's own trees keep their alpha test); y: the threshold (32 / 255);
//   z: the noise span (1 - y: a texel of alpha 1 at full fade always stays); w: the frame's noise offset (advances with the TAA only)
float4 _RfxFoliageDither;
bool RfxFoliageDither() { return _RfxFoliageDither.x > 0.5; }
// the screen noise: interleaved gradient noise on the transposed pixel (decorrelated from RfxDitherNoise, which the LOD cross-fade of the
// same pixels uses), shifted per frame while the TAA runs (it then resolves to the coverage)
float RfxFoliageNoise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel.yx + float2(17.0, 31.0) + 5.588238 * _RfxFoliageDither.w, float2(0.06711056, 0.00583715))));
}
// keep the pixel where coverage > threshold + span x noise
void RfxFoliageClip(float coverage, float2 pixel)
{
    clip(coverage - _RfxFoliageDither.y - _RfxFoliageDither.z * RfxFoliageNoise(pixel));
}
// the forest's alpha-test value (84 / 255 fully drawn .. 1 gone: the SpeedTree 4 dissolve the converter's LOD records produce) as Alive's
// fade (g_TreeFade: 1 fully drawn .. 0 gone), which multiplies the texel's alpha
float RfxFoliageFade(float alphaTest) { return saturate((1.0 - alphaTest) * 1.48571432); }   // 1 / (1 - 84 / 255)

// a per-instance LOD cross-fade: dith = (threshold, mode): mode +1 keeps the pixels with noise < threshold, -1 those with noise >= it, 0 all
void RfxDitherClip(float2 dith, float2 pixel)
{
    if (dith.y != 0.0)
    {
        float n = RfxDitherNoise(pixel);
        clip(dith.y > 0.0 ? dith.x - n : n - dith.x);
    }
}

#endif
