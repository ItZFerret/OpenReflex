#!/usr/bin/env python3
"""terrain_variants.py <out dir> [--src SHADER] -- TEST ONLY: cost probes of the terrain shader for the bench's swap= ablations
(bench.py build --extra-shaders <out dir>; run --ablations "o:swap=Reflex/Original HFTerrain=PerfBench/T1,..."). Each variant is the
shipped Shaders/ReflexTerrain.shader with one part of the pixel program removed or simplified (the timing of that part, not a look):
  T0 control (unchanged)            T1 height taps: 1 Load each instead of a bilinear 4     T2 no Unity main-light shadow
  T3 no terrain normal-map sample    T4 one detail layer (no red / green / blue fetches)       T5 RT_Bump centre tap only
  T6 no deformation samples          T7 no spherical / shadow-vis / colour-ref samples"""
import argparse, re
from pathlib import Path

R = Path(__file__).resolve().parents[3]
ap = argparse.ArgumentParser(); ap.add_argument("out", type=Path); ap.add_argument("--src", type=Path, default=R / "Unity/Reflex/Assets/Shaders/ReflexTerrain.shader")
a = ap.parse_args()
src = a.src.read_text()
src = src.replace('#include "Reflex', '#include "Assets/Shaders/Reflex')
NEAREST = """
        float RfxHeightAtNearest(float2 uvD)
        {
            int n = int(_GridParams.x);
            int2 c = clamp(int2(uvD * _GridParams.x), 0, n - 1);
            return LOAD_TEXTURE2D(_HeightMap, c).r;
        }
"""
V = {
    "T0": [],
    "T1": [('#include "Assets/Shaders/ReflexTerrainInput.hlsl"\n', '#include "Assets/Shaders/ReflexTerrainInput.hlsl"\n' + NEAREST), ("RfxHeightAt(", "RfxHeightAtNearest(")],
    "T2": [("rtW *= MainLightRealtimeShadow(shadowCoord);", "rtW *= 1.0;")],
    "T3": [("float3 N = ReflexTerrainNormalWS(i.terrainUV, i.normalWS);", "float3 N = normalize(i.normalWS);")],
    "T4": [("float4 dRed   = SAMPLE_TEXTURE2D_BIAS(_DetailRed,   sampler_DetailRed,   FlipV(uv * _DetailTile.y), bias);", "float4 dRed = dBase;"),
           ("float4 dGreen = SAMPLE_TEXTURE2D_BIAS(_DetailGreen, sampler_DetailGreen, FlipV(uv * _DetailTile.z), bias);", "float4 dGreen = dBase;"),
           ("float4 dBlue  = SAMPLE_TEXTURE2D_BIAS(_DetailBlue,  sampler_DetailBlue,  FlipV(uv * _DetailTile.w), bias);", "float4 dBlue = dBase;")],
    "T5": [("float4 tx0 = SAMPLE_TEXTURE2D(_RfxRTBump, sampler_RfxRTBump, ruv - _RfxRTBumpStep.xy);", "float4 tx0 = rt;"),
           ("float4 tz0 = SAMPLE_TEXTURE2D(_RfxRTBump, sampler_RfxRTBump, ruv - _RfxRTBumpStep.zw);", "float4 tz0 = rt;"),
           ("float4 tz1 = SAMPLE_TEXTURE2D(_RfxRTBump, sampler_RfxRTBump, ruv + _RfxRTBumpStep.zw);", "float4 tz1 = rt;"),
           ("float4 tx1 = SAMPLE_TEXTURE2D(_RfxRTBump, sampler_RfxRTBump, ruv + _RfxRTBumpStep.xy);", "float4 tx1 = rt;")],
    "T6": [("if (_UseDeformMap > 0.5)\n                        d4 =", "if (false)\n                        d4 ="),
           ("float deform = _UseDeformMap > 0.5 ?", "float deform = false ?")],
    "T7": [("float4 srm = SAMPLE_TEXTURE2D_LOD(_SphericalReflectionMap, sampler_SphericalReflectionMap, float2(0.5 + 0.5 * R.x, 0.5 + 0.5 * R.z), 0);", "float4 srm = float4(R * 0.1, 0.5);"),
           ("float4 slm = SAMPLE_TEXTURE2D_LOD(_SphericalLightMap, sampler_SphericalLightMap, float2(0.5 + 0.5 * N.x, 0.5 + 0.5 * N.z), 0);", "float4 slm = float4(N * 0.5 + 0.5, 0.7);"),
           ("float vis = _UseShadowVis > 0.5 ? SAMPLE_TEXTURE2D(_ShadowVisMap, sampler_ShadowVisMap, uvU).r : 1.0;", "float vis = 1.0;")],
}
a.out.mkdir(parents=True, exist_ok=True)
for name, reps in V.items():
    s = src.replace('Shader "Reflex/Original HFTerrain"', f'Shader "PerfBench/{name}"')
    for old, new in reps:
        assert s.count(old) >= 1, (name, old)
        s = s.replace(old, new)
    (a.out / f"PerfBench_{name}.shader").write_text("// TEST ONLY (tests/perf_profile/fixture/terrain_variants.py): a cost probe, not a look\n" + s)
print("wrote", len(V), "variants to", a.out)
