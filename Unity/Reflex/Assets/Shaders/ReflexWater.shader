// Water of the original renderer (WaterSystemRSData: rivers, lakes, the sea, pools; WP-R2, _re/notes/water.md), ported 1:1 from the
// game's own vertex / pixel programs (shader assets Water, Water_Ocean, Water_Pool, Water_Wake: every instruction transliterated into
// the functions below, the original's register names kept as the constant-table names; nothing of the game is in the project).
//
// What the original draws (XenonWater.cpp, FUN_007eec20 -> FUN_007e98a0, after the sky, before the alpha-blended scenery):
//  - a geometry clipmap around the camera (9 levels of a 65 x 65 grid, 1 m cells doubling per level), one draw per visible piece:
//    level 0's 33 x 33 inner patch, then per level the four ring pieces (Content/ReflexWater.cs builds the same buffers and per-draw
//    constants gVertexOffset c201, gMeshResolution c204, gTextureWidth c191, gTilingRate c190, gUVMultiplier c205 ...);
//    levels 0 and 1 use the *_lod0 programs, levels 2+ the *_lod1 programs (*_lod2 are never drawn);
//  - "lake" programs unless the water asset has a flow (UV) map ("river": NAT_Stillwater, TRX_Lynchburg); the Alpine river is a lake;
//  - the vertex program clamps the grid to the asset's extents, reads the asset's description map (alpha mask / colour blend / river
//    height), the 64 x 64 ocean height map (a CPU FFT of a Phillips spectrum, OriginalWater) and the 512 x 512 wake simulation, and
//    computes the original's own distance fog; the pixel program builds a normal from the ocean map at two scales plus the wake
//    gradient, Fresnel-mixes the reflection cube with the water colour and applies the fog;
//  - render states: opaque (no blend), alpha test GREATER 0 (the description map's mask), z write / LEQUAL, cull CCW;
//  - refraction is off on PC: RefractionSampler / RefractionDepthSampler (s7 / s8) are null (the game creates their targets only when
//    the scene colour target exists at water creation, which it does not), so both read (0, 0, 0, 1) as on the D3D9 device;
//  - material parameters: the game binds them through the constant tables of the lod0 / lod1 programs (the lod2 tables get the lod1
//    handles, which D3DX rejects), so every drawn program sees the material's values.
// Pass 1 / 2: the wake simulation (vs/ps_wake_sim) and the pressure splats (vs/ps_pressure) into the 512 x 512 R32F targets.
// The WaterShader asset (vs_lake_cull / vs_river_cull) is not referenced by the PC executable and is not drawn by the original.
Shader "Reflex/Original Water"
{
    Properties
    {
        // material parameters with the defaults of the programs' constant tables (the material overrides them)
        [HideInInspector] usr_amount_of_refl_color_a("usr_amount_of_refl_color_a", Vector) = (0.1, 0.1, 0.1, 0.1)
        [HideInInspector] usr_amount_of_refl_color_b("usr_amount_of_refl_color_b", Vector) = (0.1, 0.1, 0.1, 0.1)
        [HideInInspector] usr_amount_of_water_color_a("usr_amount_of_water_color_a", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_amount_of_water_color_b("usr_amount_of_water_color_b", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_color_filter_a("usr_color_filter_a", Vector) = (0.75, 0.85, 0.95, 0.0)
        [HideInInspector] usr_color_filter_b("usr_color_filter_b", Vector) = (0.75, 0.85, 0.95, 0.0)
        [HideInInspector] usr_deep_water_color_a("usr_deep_water_color_a", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_deep_water_color_b("usr_deep_water_color_b", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_reflection_color_a("usr_reflection_color_a", Vector) = (0.1, 0.3, 1.0, 0.1)
        [HideInInspector] usr_reflection_color_b("usr_reflection_color_b", Vector) = (0.1, 0.3, 1.0, 0.1)
        [HideInInspector] usr_ripple_map_size("usr_ripple_map_size", Vector) = (512.0, 512.0, 512.0, 512.0)
        [HideInInspector] usr_ripple_map_strength("usr_ripple_map_strength", Vector) = (0.2, 0.2, 0.2, 0.2)
        [HideInInspector] usr_river_tile_multiplier("usr_river_tile_multiplier", Vector) = (1.0, 1.0, 1.0, 1.0)
        [HideInInspector] usr_water_clarity_a("usr_water_clarity_a", Vector) = (20.0, 20.0, 20.0, 20.0)
        [HideInInspector] usr_water_clarity_b("usr_water_clarity_b", Vector) = (20.0, 20.0, 20.0, 20.0)
        [HideInInspector] usr_water_color_a("usr_water_color_a", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_water_color_b("usr_water_color_b", Vector) = (0.0, 0.0, 0.0, 0.0)
        [HideInInspector] usr_wave_fade_distance("usr_wave_fade_distance", Vector) = (256.0, 256.0, 256.0, 256.0)
        [HideInInspector] usr_wave_min("usr_wave_min", Vector) = (0.2, 0.2, 0.2, 0.2)
        [HideInInspector] usr_wave_scale("usr_wave_scale", Vector) = (0.2, 0.2, 0.2, 0.2)
        [HideInInspector] usr_wave_tile("usr_wave_tile", Vector) = (1.0, 1.0, 1.0, 1.0)
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="TransparentCutout" "Queue"="AlphaTest+20" "IgnoreProjector"="True" }

    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
    #include "ReflexCommon.hlsl"
    #include "ReflexWeather.hlsl"   // WEATHER (Scripts/Rendering/ReflexWeather.cs): the overcast sky in the reflection (nothing without weather)
    // ---- the original constant registers, by their constant-table names (set by OriginalWater / ReflexWater.cs) ----
    float4 gFlowRate;
    float4 gMeshResolution;
    float4 gRiverHeightValue;
    float4 gRotationMatrix[4];
    float4 gTextureWidth;
    float4 gTileWidth;
    float4 gTilingRate;
    float4 gUVMultiplier;
    float4 gUVOffset;
    float4 gVertexOffset;
    float4 gWorld[4];
    float4 g_HalfPixelOffset;
    float4 g_SimShift;
    float4 usr_amount_of_refl_color_a;
    float4 usr_amount_of_refl_color_b;
    float4 usr_amount_of_water_color_a;
    float4 usr_amount_of_water_color_b;
    float4 usr_color_filter_a;
    float4 usr_color_filter_b;
    float4 usr_deep_water_color_a;
    float4 usr_deep_water_color_b;
    float4 usr_reflection_color_a;
    float4 usr_reflection_color_b;
    float4 usr_ripple_map_size;
    float4 usr_ripple_map_strength;
    float4 usr_river_tile_multiplier;
    float4 usr_water_clarity_a;
    float4 usr_water_clarity_b;
    float4 usr_water_color_a;
    float4 usr_water_color_b;
    float4 usr_wave_fade_distance;
    float4 usr_wave_min;
    float4 usr_wave_scale;
    float4 usr_wave_tile;
    // scene globals of the original (render-pipeline.md section 3) -> the port's _Rfx globals
    #define gCameraPos float4(_WorldSpaceCameraPos, 1.0)
    #define gDistanceFog _RfxFogDistance
    #define gDistanceFogColorStart _RfxFogColorStart
    #define gDistanceFogColorEnd _RfxFogColorEnd
    #define gSunLight _RfxSunDir
    // per camera, filled by the wrappers: gWorldView (D3D view: x right, y up, z forward), gWorldViewProjection (Unity clip of the
    // camera being rendered, so o0 is the Unity position), g_ProjectionInverse rows 2 / 3 (the D3D projection inverse: near / far)
    static float4 gWorldView[4];
    static float4 gWorldViewProjection[4];
    static float4 g_ProjectionInverse[4];
    TEXTURE2D(DescVertSampler); SAMPLER(samplerDescVertSampler);
    TEXTURE2D(OceanMapVertSampler); SAMPLER(samplerOceanMapVertSampler);
    TEXTURE2D(OceanWaveSampler); SAMPLER(samplerOceanWaveSampler);
    TEXTURECUBE(ReflectionSampler); SAMPLER(samplerReflectionSampler);
    TEXTURE2D(TexLast); SAMPLER(samplerTexLast);
    TEXTURE2D(TexLastLast); SAMPLER(samplerTexLastLast);
    TEXTURE2D(TexMask); SAMPLER(samplerTexMask);
    TEXTURE2D(UVVertSampler); SAMPLER(samplerUVVertSampler);
    TEXTURE2D(WakeSampler); SAMPLER(samplerWakeSampler);
    TEXTURE2D(WakeVertSampler); SAMPLER(samplerWakeVertSampler);
    TEXTURE2D(pressure_texture); SAMPLER(samplerpressure_texture);
    TEXTURECUBE(usr_sampler_cube_reflection); SAMPLER(samplerusr_sampler_cube_reflection);
    // WEATHER: the reflection cube's sample in direction d under the clouds (the programs below are unchanged otherwise)
    float4 RfxWaterWeather(float4 c, float3 d)
    {
        [branch] if (_RfxWeather.w > 0.5)
        {
            float3 n = normalize(d); n.y = abs(n.y);
            c.rgb = lerp(c.rgb, RfxWeatherSkyColour(n, 1.0), _RfxWeather.z);
        }
        return c;
    }
    TEXTURE2D(usr_sampler_foam); SAMPLER(samplerusr_sampler_foam);
    TEXTURE2D(usr_sampler_ripple_map); SAMPLER(samplerusr_sampler_ripple_map);
    TEXTURE2D(usr_sampler_static_reflection); SAMPLER(samplerusr_sampler_static_reflection);
    #define RFX_TRACE(n, v)
    // D3D9 cmp: per component src0 >= 0 ? src1 : src2
    
    float4 RfxCmp(float4 a, float4 b, float4 c) { return float4(a.x >= 0.0 ? b.x : c.x, a.y >= 0.0 ? b.y : c.y, a.z >= 0.0 ? b.z : c.z, a.w >= 0.0 ? b.w : c.w); }
    // ---- Water_Wake (wake simulation and pressure splats) ----
    // vs_wake_sim
    void RfxWaterWake_vs_wake_sim(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, -0.5, 0.0, 1.0);
        o[0].xy = (g_HalfPixelOffset + v0).xy;   // 00d4 add
        o[1].xy = (v0 * k0 + k0.xxxx).xy;   // 00e4 mad
        o[0].zw = (k0).zw;   // 00f8 mov
    }
    // ps_wake_sim
    float4 RfxWaterWake_ps_wake_sim(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 oC0 = 0;
        const float4 k0 = float4(-0.000122070312, 0.000122070312, 0.0, 0.25);
        const float4 k1 = float4(0.984375, 0.0, 0.0, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k0.xxxx + v0.xxxx).x;   // 0178 add
        r0.x = (r0.xxxx + (-g_SimShift.xxxx)).x;   // 0188 add
        r1 = (-g_SimShift.yxzw) + v0.yxxy;   // 0198 add
        r0.y = (r1.xxxx).y;   // 01a8 mov
        r2 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r0).xy);   // 01b4 texld
        r3 = r1.yxyx + k0.yzzy;   // 01c4 add
        r4 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r3).xy);   // 01d4 texld
        r3 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r3.zwzw).xy);   // 01e4 texld
        r0.y = (r2.xxxx + r4.xxxx).y;   // 01f4 add
        r2.xyz = (r1.yxxw + k0.yxyw).xyz;   // 0204 add
        r0.zw = (r2.xyyz).zw;   // 0214 mov
        r4 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r0.xzzw).xy);   // 0220 texld
        r5 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r0.xwzw).xy);   // 0230 texld
        r0.x = (r0.yyyy + r4.xxxx).x;   // 0240 add
        r4 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r2).xy);   // 0250 texld
        r2 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r2.xzzw).xy);   // 0260 texld
        r0.x = (r0.xxxx + r4.xxxx).x;   // 0270 add
        r0.x = (r5.xxxx + r0.xxxx).x;   // 0280 add
        r0.x = (r2.xxxx + r0.xxxx).x;   // 0290 add
        r0.x = (r3.xxxx + r0.xxxx).x;   // 02a0 add
        r0.yz = (r1.xyxw + k0.xzxw).yz;   // 02b0 add
        r1 = SAMPLE_TEXTURE2D(TexLastLast, samplerTexLastLast, (r1.zwzw).xy);   // 02c0 texld
        r2 = SAMPLE_TEXTURE2D(TexLast, samplerTexLast, (r0.yzzw).xy);   // 02d0 texld
        r0.x = (r0.xxxx + r2.xxxx).x;   // 02e0 add
        r0.x = (r0.xxxx * k0.wwww + (-r1.xxxx)).x;   // 02f0 mad
        r0.x = (r0.xxxx * k1.xxxx).x;   // 0304 mul
        r1 = SAMPLE_TEXTURE2D_LOD(TexMask, samplerTexMask, (float4((v0).x, 1.0 - (v0).y, (v0).zw)).xy, 0.0);   // 0314 texld
        oC0.x = (r0.xxxx * r1.zzzz).x;   // 0324 mul
        oC0.yzw = (k0.zzzz).yzw;   // 0334 mov
        return oC0;
    }
    // vs_pressure
    void RfxWaterWake_vs_pressure(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        float4 v1 = v[1];   // texcoord0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, -0.5, 0.0, 1.0);
        o[2].xy = (v0 * k0 + k0.xxxx).xy;   // 00f8 mad
        o[0].xy = (g_HalfPixelOffset + v0).xy;   // 010c add
        o[0].zw = (k0).zw;   // 011c mov
        o[1].xy = (v1).xy;   // 0128 mov
        o[3].xy = (v0.zzzz).xy;   // 0134 mov
    }
    // ps_pressure
    float4 RfxWaterWake_ps_pressure(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 oC0 = 0;
        float4 r0 = 0, r1 = 0;
        r0 = SAMPLE_TEXTURE2D_LOD(pressure_texture, samplerpressure_texture, (float4((v0).x, 1.0 - (v0).y, (v0).zw)).xy, 0.0);   // 00c8 texld
        r0.x = ((-r0.yyyy) + r0.xxxx).x;   // 00d8 add
        r0.x = (r0.xxxx * v2.xxxx).x;   // 00e8 mul
        r1 = SAMPLE_TEXTURE2D_LOD(pressure_texture, samplerpressure_texture, (float4((v1).x, 1.0 - (v1).y, (v1).zw)).xy, 0.0);   // 00f8 texld
        oC0 = r0.xxxx * r1.zzzz;   // 0108 mul
        return oC0;
    }
    ENDHLSL
        Pass
        {
            Name "Water"
            Tags { "LightMode"="UniversalForward" }
            Cull Back
            ZWrite On
            ZTest LEqual

            HLSLPROGRAM
            #pragma target 3.5
            #pragma vertex WaterVert
            #pragma fragment WaterFrag
            #pragma multi_compile_local _WATER_WATER _WATER_OCEAN _WATER_POOL
            #pragma multi_compile_local _ _WATER_RIVER
            #pragma multi_compile_local _ _WATER_LOD1

    #if defined(_WATER_WATER) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_lake_lod0
    void RfxWater_vs_lake_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, 0.100000001);
        const float4 k1 = float4(0.00390625, 0.0078125, 0.200000003, 5.0);
        const float4 k2 = float4(6.0, 9.99999975e-06, 0.0, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 06f0 add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 0700 rcp
        r1.xy = (gVertexOffset).xy;   // 070c mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0718 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 072c max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 073c min
        r1.w = (k0.yyyy).w;   // 074c mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 0758 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 0768 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 0778 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 0788 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0798 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 07ac mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 07bc rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 07c8 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 07d8 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 07e4 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 07f0 mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 0800 mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + (-k0.yyyy))).y;   // 0814 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0828 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 083c add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 084c mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 0860 mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 0870 add
        r0.xy = (r3.wwww * k1).xy;   // 0880 mul
        r0.xy = (min(r0, k0.yyyy)).xy;   // 0890 min
        r0.xy = ((-r0) + k0.yyyy).xy;   // 08a0 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 08b0 mul
        o[5].z = (max(r0.yyyy, k1.zzzz)).z;   // 08c0 max
        r0.x = (max(r0.xxxx, k0.wwww)).x;   // 08d0 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 08e0 mul
        r4.zw = (k0.zzzz).zw;   // 08f0 mov
        r5.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 08fc rcp
        r5.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0908 rcp
        r0.yz = (r1.xxzw * r5.xxyw).yz;   // 0914 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0924 mad
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4).xy, (r4).w);   // 0938 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 0948 mul
        o[5].xy = (r0.xxxx * r2).xy;   // 0958 mul
        r2.zw = (k0.zzzz).zw;   // 0968 mov
        r2.xy = (r4 * gTilingRate.xxxx).xy;   // 0974 mul
        o[4].xy = (r4).xy;   // 0984 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 0990 texldl
        r0.x = (r0.xxxx * r2.xxxx).x;   // 09a0 mul
        r1.y = (r0.xxxx * k1.wwww).y;   // 09b0 mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 09c0 dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 09d0 dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 09e0 dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 09f0 dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 0a00 rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0a0c dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0a1c dp3
        r0.xy = (r0.wwww * r2).xy;   // 0a2c mul
        r0.zw = (k0.zzzz).zw;   // 0a3c mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 0a48 texldl
        o[4].zw = (r0.xyxy).zw;   // 0a58 mov
        r1.y = (r2.xxxx * k2.xxxx + r1.yyyy).y;   // 0a64 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a78 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a88 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a98 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0aa8 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0ab8 rcp
        o[1] = r0 * r2.xxxx;   // 0ac4 mul
        o[0] = r0;   // 0ad4 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0ae0 dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0af0 dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0b00 dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0b10 dp4
        r0.xyz = (usr_water_color_b).xyz;   // 0b20 mov
        r1.xyz = ((-r0) + usr_water_color_a).xyz;   // 0b2c add
        r0.w = (usr_amount_of_water_color_b.xxxx).w;   // 0b3c mov
        r1.w = ((-r0.wwww) + usr_amount_of_water_color_a.xxxx).w;   // 0b48 add
        o[8] = r5.zzzz * r1 + r0;   // 0b58 mad
        r0.xyz = (usr_color_filter_b).xyz;   // 0b6c mov
        r0.xyz = ((-r0) + usr_color_filter_a).xyz;   // 0b78 add
        o[7].xyz = (r5.zzzz * r0 + usr_color_filter_b).xyz;   // 0b88 mad
        r0.x = (usr_water_clarity_b.xxxx).x;   // 0b9c mov
        r0.x = ((-r0.xxxx) + usr_water_clarity_a.xxxx).x;   // 0ba8 add
        o[7].w = (r5.zzzz * r0.xxxx + usr_water_clarity_b.xxxx).w;   // 0bb8 mad
        r0 = usr_deep_water_color_b;   // 0bcc mov
        r0 = (-r0) + usr_deep_water_color_a;   // 0bd8 add
        o[9] = r5.zzzz * r0 + usr_deep_water_color_b;   // 0be8 mad
        r0.xyz = (usr_reflection_color_b).xyz;   // 0bfc mov
        r1.xyz = ((-r0) + usr_reflection_color_a).xyz;   // 0c08 add
        r0.w = (usr_amount_of_refl_color_b.xxxx).w;   // 0c18 mov
        r1.w = ((-r0.wwww) + usr_amount_of_refl_color_a.xxxx).w;   // 0c24 add
        o[10] = r5.zzzz * r1 + r0;   // 0c34 mad
        o[5].w = (r5.yyyy).w;   // 0c48 mov
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0c54 add
        o[3] = r3;   // 0c64 mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0c70 dp3
        r0.y = (max(r0.yyyy, k2.yyyy)).y;   // 0c80 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0c90 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0ca0 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0cb0 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0cc0 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0ccc mul
        r1 = gDistanceFogColorStart;   // 0cdc mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0ce8 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0cf8 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0d0c mad
    }
    #endif
    #if defined(_WATER_WATER) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_lake_lod0
    float4 RfxWater_ps_lake_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, -1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 10.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 0348 rcp
        r0.x = (r0.xxxx * gTilingRate.xxxx).x;   // 0354 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0364 mul
        r2.zw = (r1.xywz).zw;   // 0374 mov
        r0.y = ((1.0 / gTextureWidth.x).xxxx).y;   // 0380 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.yyyy)).xy;   // 038c mad
        r1.xy = (r0.xxxx * v3 + r0.yyyy).xy;   // 03a0 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 03b4 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 03c4 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 03d4 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 03e4 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 03f4 add
        r0.z = ((-r3.xxxx) + r4.xxxx).z;   // 0404 add
        r0.w = ((1.0 / gTilingRate.y).xxxx).w;   // 0414 rcp
        r0.w = (r0.wwww * gTilingRate.xxxx).w;   // 0420 mul
        r1.xy = (r0.wwww * v3 + r0.yyyy).xy;   // 0430 mad
        r2.xy = (r0.wwww * v3 + (-r0.yyyy)).xy;   // 0444 mad
        r1.zw = (r0.wwww * v3.xyxy).zw;   // 0458 mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 0468 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0478 texld
        r2.zw = (r1.xywz).zw;   // 0488 mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0494 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 04a4 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 04b4 add
        r0.w = ((-r1.xxxx) + r3.xxxx).w;   // 04c4 add
        r1.xy = (k0.xxxx * v4).xy;   // 04d4 mul
        r0.yw = (r0 * r1.yyyy).yw;   // 04e4 mul
        r0.z = (r1.xxxx * r0.zzzz + r0.wwww).z;   // 04f4 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 0508 add
        r2.zw = (v3.xywz).zw;   // 0518 mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 0524 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0534 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0544 add
        r4.zw = (v3.xywz).zw;   // 0554 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0560 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0570 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 0580 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0590 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 05a0 mad
        r2.x = ((-r0.zzzz)).x;   // 05b4 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 05c0 mad
        r2.z = (r0.wwww * k0.yyyy + r0.xxxx).z;   // 05d4 mad
        r2.y = (k0.zzzz).y;   // 05e8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 05f4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 0600 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 0610 mad
        r1.x = (r1.xxxx * (-k3.xxxx) + k3.yyyy).x;   // 0624 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0638 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0644 rcp
        r0.w = (r0.wwww * k3.zzzz + r1.xxxx).w;   // 0650 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0664 mul
        r1.xy = (v2.xzzw * k3.wwww + (-r1)).xy;   // 0674 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0688 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0698 rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 06a4 mad
        r2.x = (v0.xxxx * k2.xxxx + r0.wwww).x;   // 06b8 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 06cc rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 06d8 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.wwww).y;   // 06ec mad
        r1.xy = (r1 * k2.zzzz + r2).xy;   // 0700 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0714 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 0724 texld
        r3.yzw = (r3.xxyz).yzw;   // 0734 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0740 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 074c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0758 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 076c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0778 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0788 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 079c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 07ac texld
        r2.yzw = (r2.xxyz).yzw;   // 07bc mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 07c8 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 07dc rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 07e8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 07f8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 080c add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 081c rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 0828 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0838 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0844 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0850 log
        r2.xyz = (r0.wwww * r2).xyz;   // 085c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 086c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0878 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0884 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0890 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 08a0 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 08b4 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 08c8 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 08dc dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 08ec add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 08fc add
        r0.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 090c mad
        r0.xyz = ((-r0)).xyz;   // 0920 mov
        r1.x = (r0.yyyy + k0.zzzz).x;   // 092c add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(ReflectionSampler, samplerReflectionSampler, (r0).xyz), (r0).xyz);   // 093c texld
        r0.x = (min(r1.xxxx, k0.zzzz)).x;   // 094c min
        r0.y = (r0.wwww * r0.wwww).y;   // 095c mul
        r0.y = (r0.yyyy * r0.yyyy).y;   // 096c mul
        r0.y = (r0.yyyy * r0.wwww).y;   // 097c mul
        r0.y = (r0.yyyy * k1.xxxx + k1.yyyy).y;   // 098c mad
        r0.x = (r0.xxxx * r0.yyyy).x;   // 09a0 mul
        r0.yzw = (r3.xxyz + k0.wwww).yzw;   // 09b0 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 09c0 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 09d0 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 09e0 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 09f4 mul
        r4.xyz = (max(k0.zzzz, r0.yzww)).xyz;   // 0a04 max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 0a14 mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0a28 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0a3c lrp
        r0.w = (k2.wwww * v4.wwww).w;   // 0a50 mul
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0a60 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_WATER) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_lake_lod1
    void RfxWater_vs_lake_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, 0.00390625);
        const float4 k1 = float4(0.100000001, 5.0, 6.0, 9.99999975e-06);
        const float4 k2 = float4(0.0, 1.0, 0.200000003, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 06f0 add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 0700 rcp
        r1.xy = (gVertexOffset).xy;   // 070c mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0718 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 072c max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 073c min
        r1.w = (k0.yyyy).w;   // 074c mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 0758 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 0768 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 0778 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 0788 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0798 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 07ac mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 07bc rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 07c8 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 07d8 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 07e4 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 07f0 mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 0800 mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + (-k0.yyyy))).y;   // 0814 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0828 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 083c add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 084c mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 0860 mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 0870 add
        r0.x = (r3.wwww * k0.wwww).x;   // 0880 mul
        r0.x = (min(r0.xxxx, k0.yyyy)).x;   // 0890 min
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 08a0 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 08b0 mul
        r0.x = (max(r0.xxxx, k1.xxxx)).x;   // 08c0 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 08d0 mul
        r4.zw = (k0.zzzz).zw;   // 08e0 mov
        r5.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 08ec rcp
        r5.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 08f8 rcp
        r0.yz = (r1.xxzw * r5.xxyw).yz;   // 0904 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0914 mad
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4).xy, (r4).w);   // 0928 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 0938 mul
        o[5].xy = (r0.xxxx * r2).xy;   // 0948 mul
        r0.x = (r0.xxxx * k1.yyyy).x;   // 0958 mul
        r2.zw = (k0.zzzz).zw;   // 0968 mov
        r2.xy = (r4 * gTilingRate.xxxx).xy;   // 0974 mul
        o[4].xy = (r4).xy;   // 0984 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 0990 texldl
        r1.y = (r0.xxxx * r2.xxxx).y;   // 09a0 mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 09b0 dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 09c0 dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 09d0 dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 09e0 dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 09f0 rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 09fc dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0a0c dp3
        r0.xy = (r0.wwww * r2).xy;   // 0a1c mul
        r0.zw = (k0.zzzz).zw;   // 0a2c mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 0a38 texldl
        o[4].zw = (r0.xyxy).zw;   // 0a48 mov
        r1.y = (r2.xxxx * k1.zzzz + r1.yyyy).y;   // 0a54 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a68 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a78 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a88 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a98 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0aa8 rcp
        o[1] = r0 * r2.xxxx;   // 0ab4 mul
        o[0] = r0;   // 0ac4 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0ad0 dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0ae0 dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0af0 dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0b00 dp4
        r0.xyz = (usr_water_color_b).xyz;   // 0b10 mov
        r1.xyz = ((-r0) + usr_water_color_a).xyz;   // 0b1c add
        r0.w = (usr_amount_of_water_color_b.xxxx).w;   // 0b2c mov
        r1.w = ((-r0.wwww) + usr_amount_of_water_color_a.xxxx).w;   // 0b38 add
        o[8] = r5.zzzz * r1 + r0;   // 0b48 mad
        r0.xyz = (usr_color_filter_b).xyz;   // 0b5c mov
        r0.xyz = ((-r0) + usr_color_filter_a).xyz;   // 0b68 add
        o[7].xyz = (r5.zzzz * r0 + usr_color_filter_b).xyz;   // 0b78 mad
        r0.x = (usr_water_clarity_b.xxxx).x;   // 0b8c mov
        r0.x = ((-r0.xxxx) + usr_water_clarity_a.xxxx).x;   // 0b98 add
        o[7].w = (r5.zzzz * r0.xxxx + usr_water_clarity_b.xxxx).w;   // 0ba8 mad
        r0 = usr_deep_water_color_b;   // 0bbc mov
        r0 = (-r0) + usr_deep_water_color_a;   // 0bc8 add
        o[9] = r5.zzzz * r0 + usr_deep_water_color_b;   // 0bd8 mad
        r0.xyz = (usr_reflection_color_b).xyz;   // 0bec mov
        r1.xyz = ((-r0) + usr_reflection_color_a).xyz;   // 0bf8 add
        r0.w = (usr_amount_of_refl_color_b.xxxx).w;   // 0c08 mov
        r1.w = ((-r0.wwww) + usr_amount_of_refl_color_a.xxxx).w;   // 0c14 add
        o[10] = r5.zzzz * r1 + r0;   // 0c24 mad
        o[5].zw = (r5.yyyy * k2.xyxy + k2.xyzx).zw;   // 0c38 mad
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0c4c add
        o[3] = r3;   // 0c5c mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0c68 dp3
        r0.y = (max(r0.yyyy, k1.wwww)).y;   // 0c78 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0c88 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0c98 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0ca8 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0cb8 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0cc4 mul
        r1 = gDistanceFogColorStart;   // 0cd4 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0ce0 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0cf0 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0d04 mad
    }
    #endif
    #if defined(_WATER_WATER) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_lake_lod1
    float4 RfxWater_ps_lake_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, -1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 0.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 0348 rcp
        r0.x = (r0.xxxx * gTilingRate.xxxx).x;   // 0354 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0364 mul
        r2.zw = (r1.xywz).zw;   // 0374 mov
        r0.y = ((1.0 / gTextureWidth.x).xxxx).y;   // 0380 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.yyyy)).xy;   // 038c mad
        r1.xy = (r0.xxxx * v3 + r0.yyyy).xy;   // 03a0 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 03b4 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 03c4 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 03d4 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 03e4 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 03f4 add
        r0.z = ((-r3.xxxx) + r4.xxxx).z;   // 0404 add
        r0.w = ((1.0 / gTilingRate.y).xxxx).w;   // 0414 rcp
        r0.w = (r0.wwww * gTilingRate.xxxx).w;   // 0420 mul
        r1.xy = (r0.wwww * v3 + r0.yyyy).xy;   // 0430 mad
        r2.xy = (r0.wwww * v3 + (-r0.yyyy)).xy;   // 0444 mad
        r1.zw = (r0.wwww * v3.xyxy).zw;   // 0458 mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 0468 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0478 texld
        r2.zw = (r1.xywz).zw;   // 0488 mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0494 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 04a4 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 04b4 add
        r0.w = ((-r1.xxxx) + r3.xxxx).w;   // 04c4 add
        r1.xy = (k0.xxxx * v4).xy;   // 04d4 mul
        r0.yw = (r0 * r1.yyyy).yw;   // 04e4 mul
        r0.z = (r1.xxxx * r0.zzzz + r0.wwww).z;   // 04f4 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 0508 add
        r2.zw = (v3.xywz).zw;   // 0518 mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 0524 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0534 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0544 add
        r4.zw = (v3.xywz).zw;   // 0554 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0560 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0570 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 0580 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0590 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 05a0 mad
        r2.x = ((-r0.zzzz)).x;   // 05b4 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 05c0 mad
        r2.z = (r0.wwww * k0.yyyy + r0.xxxx).z;   // 05d4 mad
        r2.y = (k0.zzzz).y;   // 05e8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 05f4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 0600 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 0610 mad
        r1.x = (r1.xxxx * (-k3.xxxx) + k3.yyyy).x;   // 0624 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0638 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0644 rcp
        r0.w = (r0.wwww * k3.zzzz + r1.xxxx).w;   // 0650 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0664 mul
        r1.xy = (v2.xzzw * k3.wwww + (-r1)).xy;   // 0674 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0688 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0698 rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 06a4 mad
        r2.x = (v0.xxxx * k2.xxxx + r0.wwww).x;   // 06b8 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 06cc rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 06d8 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.wwww).y;   // 06ec mad
        r1.xy = (r1 * k2.zzzz + r2).xy;   // 0700 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0714 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 0724 texld
        r3.yzw = (r3.xxyz).yzw;   // 0734 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0740 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 074c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0758 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 076c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0778 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0788 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 079c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 07ac texld
        r2.yzw = (r2.xxyz).yzw;   // 07bc mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 07c8 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 07dc rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 07e8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 07f8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 080c add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 081c rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 0828 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0838 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0844 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0850 log
        r2.xyz = (r0.wwww * r2).xyz;   // 085c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 086c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0878 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0884 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0890 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 08a0 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 08b4 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 08c8 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 08dc dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 08ec add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 08fc add
        r0.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 090c mad
        r0.xyz = ((-r0)).xyz;   // 0920 mov
        r1.x = (r0.yyyy + k0.zzzz).x;   // 092c add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(ReflectionSampler, samplerReflectionSampler, (r0).xyz), (r0).xyz);   // 093c texld
        r0.x = (min(r1.xxxx, k0.zzzz)).x;   // 094c min
        r0.y = (r0.wwww * r0.wwww).y;   // 095c mul
        r0.y = (r0.yyyy * r0.yyyy).y;   // 096c mul
        r0.y = (r0.yyyy * r0.wwww).y;   // 097c mul
        r0.y = (r0.yyyy * k1.xxxx + k1.yyyy).y;   // 098c mad
        r0.x = (r0.xxxx * r0.yyyy).x;   // 09a0 mul
        r0.yzw = (r3.xxyz + k0.wwww).yzw;   // 09b0 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 09c0 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 09d0 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 09e0 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 09f4 mul
        r4.xyz = (max(k0.zzzz, r0.yzww)).xyz;   // 0a04 max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 0a14 mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0a28 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0a3c lrp
        r0.w = (v4.wwww).w;   // 0a50 mov
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0a5c cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_WATER) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_river_lod0
    void RfxWater_vs_river_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.52590219e-05, 6.0);
        const float4 k1 = float4(0.5, 0.0, 1.0, 5.0);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 0778 mov
        r1.w = (k1.yyyy).w;   // 0784 mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0790 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 079c rcp
        r3.xy = (gVertexOffset).xy;   // 07a8 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 07b4 mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 07c8 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 07d8 min
        r2.xy = (r2 * r3.xzzw).xy;   // 07e8 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 07f8 mad
        r2.zw = (k1.yyyy).zw;   // 080c mov
        r4 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 0818 texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 0828 texldl
        r1.z = (r4.yyyy * usr_river_tile_multiplier.xxxx).z;   // 0838 mul
        r5.x = (usr_river_tile_multiplier.xxxx).x;   // 0848 mov
        r1.x = (r4.xxxx * r5.xxxx + (-gFlowRate.xxxx)).x;   // 0854 mad
        r4 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r1.xzww).xy, (r1.xzww).w);   // 0868 texldl
        o[4].xy = (r1.xzzw).xy;   // 0878 mov
        r1.xy = (r2.xwzw * k0).xy;   // 0884 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0894 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 08a4 mul
        r1.z = (k0.zzzz).z;   // 08b4 mov
        r1.x = (r1.xxxx * r1.zzzz + gRiverHeightValue.yyyy).x;   // 08c0 mad
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 08d4 mul
        r1.y = ((-r2.zzzz) + k1.zzzz).y;   // 08e4 add
        r1.z = (r0.wwww * k1.wwww).z;   // 08f4 mul
        r3.y = (r1.zzzz * r4.xxxx + r1.xxxx).y;   // 0904 mad
        r3.w = (k1.zzzz).w;   // 0918 mov
        r2.x = (dot(r3, gWorld[0]).xxxx).x;   // 0924 dp4
        r2.y = (dot(r3, gWorld[2]).xxxx).y;   // 0934 dp4
        r2.z = (dot(r3, gWorld[3]).xxxx).z;   // 0944 dp4
        r1.x = (dot((r2).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).x;   // 0954 dp3
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0964 rcp
        r4.x = (dot((r2).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0970 dp3
        r4.y = (dot((r2).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0980 dp3
        r2.xy = (r1.xxxx * r4).xy;   // 0990 mul
        r2.zw = (k1.yyyy).zw;   // 09a0 mov
        r4 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r2).xy, (r2).w);   // 09ac texldl
        o[4].zw = (r2.xyxy).zw;   // 09bc mov
        r3.y = (r4.xxxx * k0.wwww + r3.yyyy).y;   // 09c8 mad
        r2.x = (dot(r3, gWorld[0]).xxxx).x;   // 09dc dp4
        r2.y = (dot(r3, gWorld[1]).xxxx).y;   // 09ec dp4
        r2.z = (dot(r3, gWorld[2]).xxxx).z;   // 09fc dp4
        r1.xzw = ((-r2.xyyz) + gCameraPos.xyyz).xzw;   // 0a0c add
        r2.x = (dot((r1.xzww).xyz, (r1.xzww).xyz).xxxx).x;   // 0a1c dp3
        r2.x = (rsqrt(abs(r2.x)).xxxx).x;   // 0a2c rsq
        r4.w = ((1.0 / r2.x).xxxx).w;   // 0a38 rcp
        r4.xyz = (r1.xzww * r2.xxxx).xyz;   // 0a44 mul
        r2 = r4.wwww * k2;   // 0a54 mul
        r2 = min(r2, k1.zzzz);   // 0a64 min
        r2 = (-r2) + k1.zzzz;   // 0a74 add
        r5.x = (r0.wwww * r2.xxxx).x;   // 0a84 mul
        r5.yzw = (max(r2, k3.xxyy)).yzw;   // 0a94 max
        r0 = r0 * r5;   // 0aa4 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 0ab4 max
        o[5].xyw = (r0).xyw;   // 0ac4 mov
        r0.x = (dot(r3, gWorldViewProjection[0]).xxxx).x;   // 0ad0 dp4
        r0.y = (dot(r3, gWorldViewProjection[1]).xxxx).y;   // 0ae0 dp4
        r0.z = (dot(r3, gWorldViewProjection[2]).xxxx).z;   // 0af0 dp4
        r0.w = (dot(r3, gWorldViewProjection[3]).xxxx).w;   // 0b00 dp4
        r1.x = ((1.0 / r0.w).xxxx).x;   // 0b10 rcp
        o[1] = r0 * r1.xxxx;   // 0b1c mul
        o[0] = r0;   // 0b2c mov
        o[2].x = (dot(r3, gWorldView[0]).xxxx).x;   // 0b38 dp4
        o[2].y = (dot(r3, gWorldView[1]).xxxx).y;   // 0b48 dp4
        o[2].z = (dot(r3, gWorldView[2]).xxxx).z;   // 0b58 dp4
        o[2].w = (dot(r3, gWorldView[3]).xxxx).w;   // 0b68 dp4
        r0.xyz = (usr_water_color_b).xyz;   // 0b78 mov
        r2.xyz = ((-r0) + usr_water_color_a).xyz;   // 0b84 add
        r0.w = (usr_amount_of_water_color_b.xxxx).w;   // 0b94 mov
        r2.w = ((-r0.wwww) + usr_amount_of_water_color_a.xxxx).w;   // 0ba0 add
        o[8] = r1.yyyy * r2 + r0;   // 0bb0 mad
        r0.xyz = (usr_color_filter_b).xyz;   // 0bc4 mov
        r0.xyz = ((-r0) + usr_color_filter_a).xyz;   // 0bd0 add
        o[7].xyz = (r1.yyyy * r0 + usr_color_filter_b).xyz;   // 0be0 mad
        r0.x = (usr_water_clarity_b.xxxx).x;   // 0bf4 mov
        r0.x = ((-r0.xxxx) + usr_water_clarity_a.xxxx).x;   // 0c00 add
        o[7].w = (r1.yyyy * r0.xxxx + usr_water_clarity_b.xxxx).w;   // 0c10 mad
        r0 = usr_deep_water_color_b;   // 0c24 mov
        r0 = (-r0) + usr_deep_water_color_a;   // 0c30 add
        o[9] = r1.yyyy * r0 + usr_deep_water_color_b;   // 0c40 mad
        r0.xyz = (usr_reflection_color_b).xyz;   // 0c54 mov
        r2.xyz = ((-r0) + usr_reflection_color_a).xyz;   // 0c60 add
        r0.w = (usr_amount_of_refl_color_b.xxxx).w;   // 0c70 mov
        r2.w = ((-r0.wwww) + usr_amount_of_refl_color_a.xxxx).w;   // 0c7c add
        o[10] = r1.yyyy * r2 + r0;   // 0c8c mad
        r0.x = (r4.wwww + (-gDistanceFog.xxxx)).x;   // 0ca0 add
        o[3] = r4;   // 0cb0 mov
        r0.y = (dot((gSunLight).xyz, (r4).xyz).xxxx).y;   // 0cbc dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0ccc max
        r0.y = (min(r0.yyyy, k1.zzzz)).y;   // 0cdc min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0cec mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0cfc add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0d0c rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0d18 mul
        r1 = gDistanceFogColorStart;   // 0d28 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0d34 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0d44 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0d58 mad
    }
    #endif
    #if defined(_WATER_WATER) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_river_lod0
    float4 RfxWater_ps_river_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(1.0, 0.979600012, 0.0204000007, -1.0);
        const float4 k1 = float4(0.0, 0.0500000007, 9.99999994e-09, 1.0);
        const float4 k2 = float4(0.100000001, -0.200000003, -0.5, 0.25);
        const float4 k3 = float4(9.99999975e-05, -9.99999975e-05, 0.5, -0.5);
        const float4 k4 = float4(5.0, 4.0, 8.0, 12.0);
        const float4 k5 = float4(20.0, 1.0, 10.0, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k0.xxxx).y;   // 038c mov
        r1.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 0398 add
        r1.zw = (v3.xywz).zw;   // 03a8 mov
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r1.xzzw).xy);   // 03b4 texld
        r1 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r1.wyzw).xy);   // 03c4 texld
        r3.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 03d4 add
        r3.zw = (v3.xywz).zw;   // 03e4 mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 03f0 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 0400 texld
        r0.w = (r1.xxxx + (-r3.xxxx)).w;   // 0410 add
        r1.x = (r2.xxxx + (-r4.xxxx)).x;   // 0420 add
        r2.zw = (v3.xyyx).zw;   // 0430 mov
        r1.y = ((1.0 / gTextureWidth.x).xxxx).y;   // 043c rcp
        r2.xy = (r1.yyyy + v3).xy;   // 0448 add
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0458 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0468 texld
        r4.xy = ((-r1.yyyy) + v3).xy;   // 0478 add
        r4.zw = (v3.xyyx).zw;   // 0488 mov
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r4.xzzw).xy);   // 0494 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r4.wyzw).xy);   // 04a4 texld
        r1.z = (r2.xxxx + (-r4.xxxx)).z;   // 04b4 add
        r1.w = (r3.xxxx + (-r5.xxxx)).w;   // 04c4 add
        r2.xy = (v3 * k4.yyyy + r1.yyyy).xy;   // 04d4 mad
        r3.xy = (v3 * k4.yyyy + (-r1.yyyy)).xy;   // 04e8 mad
        r2.zw = (k4.yyyy * v3.xyyx).zw;   // 04fc mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 050c texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 051c texld
        r3.zw = (k4.yyyy * v3.xyyx).zw;   // 052c mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 053c texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 054c texld
        r1.y = (r2.xxxx + (-r3.xxxx)).y;   // 055c add
        r2.x = (r4.xxxx + (-r5.xxxx)).x;   // 056c add
        r2.yz = (k4.xxxx * v4.xwxw).yz;   // 057c mul
        r2.x = (r2.xxxx * r2.zzzz).x;   // 058c mul
        r1.w = (r2.yyyy * r1.wwww + r2.xxxx).w;   // 059c mad
        r1.x = (r1.xxxx * k4.wwww + r1.wwww).x;   // 05b0 mad
        r0.x = ((-r1.xxxx)).x;   // 05c4 mov
        r1.x = (r1.yyyy * r2.zzzz).x;   // 05d0 mul
        r1.x = (r2.yyyy * r1.zzzz + r1.xxxx).x;   // 05e0 mad
        r0.z = (r0.wwww * k4.wwww + r1.xxxx).z;   // 05f4 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 0608 nrm
        r0.x = (dot(((-v2)).xyz, (r1).xyz).xxxx).x;   // 0614 dp3
        r0.y = (r0.xxxx * (-r0.xxxx) + k0.xxxx).y;   // 0624 mad
        r0.y = (r0.yyyy * (-k1.zzzz) + k1.wwww).y;   // 0638 mad
        r0.y = (rsqrt(abs(r0.y)).xxxx).y;   // 064c rsq
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0658 rcp
        r0.x = (r0.xxxx * k3.xxxx + r0.yyyy).x;   // 0664 mad
        r0.xy = (r1.xzzw * r0.xxxx).xy;   // 0678 mul
        r0.xy = (v2.xzzw * k3.yyyy + (-r0)).xy;   // 0688 mad
        r0.xy = (r0 * v4.zzzz).xy;   // 069c mul
        r0.z = ((1.0 / gUVMultiplier.y).xxxx).z;   // 06ac rcp
        r0.z = (r0.zzzz * k3.zzzz + k3.zzzz).z;   // 06b8 mad
        r3.x = (v0.xxxx * k3.zzzz + r0.zzzz).x;   // 06cc mad
        r0.z = ((1.0 / gUVMultiplier.z).xxxx).z;   // 06e0 rcp
        r0.z = (r0.zzzz * k3.zzzz + k3.zzzz).z;   // 06ec mad
        r3.y = (v0.yyyy * k3.wwww + r0.zzzz).y;   // 0700 mad
        r0.xy = (r0 * k2.xxxx + r3).xy;   // 0714 mad
        r4 = float4(0.0, 0.0, 0.0, 1.0);   // 0728 texld
        r0 = float4(0.0, 0.0, 0.0, 1.0);   // 0738 texld
        r4.yzw = (r4.xxyz).yzw;   // 0748 mov
        r5.xz = (g_ProjectionInverse[2].zyww).xz;   // 0754 mov
        r5.yw = (g_ProjectionInverse[3].xzzw).yw;   // 0760 mov
        r0.xy = (r5 * r0.xxxx + r5.zwzw).xy;   // 076c mad
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0780 rcp
        r4.x = (r0.yyyy * r0.xxxx).x;   // 078c mul
        r0.x = (r0.xxxx * r0.yyyy + (-v1.zzzz)).x;   // 079c mad
        r6 = float4(0.0, 0.0, 0.0, 1.0);   // 07b0 texld
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 07c0 texld
        r3.yzw = (r3.xxyz).yzw;   // 07d0 mov
        r0.yz = (r5.xxyw * r6.xxxx + r5.xzww).yz;   // 07dc mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 07f0 rcp
        r3.x = (r0.zzzz * r0.yyyy).x;   // 07fc mul
        r0 = RfxCmp(r0.xxxx, r4, r3);   // 080c cmp
        r0.x = (r0.xxxx + (-v1.zzzz)).x;   // 0820 add
        r1.w = ((1.0 / v6.w).xxxx).w;   // 0830 rcp
        r1.w = (saturate(r0.xxxx * r1.wwww)).w;   // 083c mul
        r3.x = (log2(abs(v6.x)).xxxx).x;   // 084c log
        r3.y = (log2(abs(v6.y)).xxxx).y;   // 0858 log
        r3.z = (log2(abs(v6.z)).xxxx).z;   // 0864 log
        r3.xyz = (r0.xxxx * r3).xyz;   // 0870 mul
        r0.x = (saturate(r0.xxxx)).x;   // 0880 mov
        r0.x = ((-r0.xxxx) + k0.xxxx).x;   // 088c add
        r0.x = ((dot((r0.xxxx).xy, (r1.xxxx).xy) + k1.x).xxxx).x;   // 089c dp2add
        r4.x = (saturate(exp2(r3.x).xxxx)).x;   // 08b0 exp
        r4.y = (saturate(exp2(r3.y).xxxx)).y;   // 08bc exp
        r4.z = (saturate(exp2(r3.z).xxxx)).z;   // 08c8 exp
        r3.xyz = (r0.yzww * r4).xyz;   // 08d4 mul
        r0.yzw = (r0 * (-r4.xxyz) + v8.xxyz).yzw;   // 08e4 mad
        r0.yzw = (r1.wwww * r0 + r3.xxyz).yzw;   // 08f8 mad
        r3.xyz = (v7.wwww * (v7 - r0.yzww) + r0.yzww).xyz;   // 090c lrp
        r4 = k4.yyzz * v3.xyxy;   // 0920 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r4).xy);   // 0930 texld
        r4 = SAMPLE_TEXTURE2D(usr_sampler_foam, samplerusr_sampler_foam, (float4((r4.zwzw).x, 1.0 - (r4.zwzw).y, (r4.zwzw).zw)).xy);   // 0940 texld
        r0.y = (r2.zzzz * r5.xxxx).y;   // 0950 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (v3).xy);   // 0960 texld
        r0.y = (r2.yyyy * r5.xxxx + r0.yyyy).y;   // 0970 mad
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (v3.zwzw).xy);   // 0984 texld
        r0.y = (r2.xxxx * k4.wwww + r0.yyyy).y;   // 0994 mad
        r0.yz = (r0.yyyy + k2).yz;   // 09a8 add
        r0.yz = (r0 * r1.yyyy).yz;   // 09b8 mul
        r0.y = (r0.yyyy * k2.wwww).y;   // 09c8 mul
        r1.w = (saturate(max(r0.yyyy, r0.zzzz))).w;   // 09d8 max
        r0.y = (r1.wwww * (-k5.xxxx) + k5.yyyy).y;   // 09e8 mad
        r2.x = (max(r0.xxxx, r1.wwww)).x;   // 09fc max
        r1.w = (max(r0.yyyy, k1.xxxx)).w;   // 0a0c max
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 0a1c dp3
        r0.y = ((-r0.xxxx) + k0.xxxx).y;   // 0a2c add
        r0.x = (r0.xxxx + r0.xxxx).x;   // 0a3c add
        r0.xzw = (r1.xyyz * (-r0.xxxx) + v2.xyyz).xzw;   // 0a4c mad
        r0.xzw = ((-r0)).xzw;   // 0a60 mov
        r1.x = (r0.yyyy * r0.yyyy).x;   // 0a6c mul
        r1.x = (r1.xxxx * r1.xxxx).x;   // 0a7c mul
        r0.y = (r0.yyyy * r1.xxxx).y;   // 0a8c mul
        r0.y = (r0.yyyy * k0.yyyy + k0.zzzz).y;   // 0a9c mad
        r2.y = (min(r1.wwww, r0.yyyy)).y;   // 0ab0 min
        r0.y = (r0.zzzz + k0.xxxx).y;   // 0ac0 add
        r1 = RfxWaterWeather(SAMPLE_TEXTURECUBE(ReflectionSampler, samplerReflectionSampler, (r0.xzww).xyz), (r0.xzww).xyz);   // 0ad0 texld
        r1.w = (min(r0.yyyy, k0.xxxx)).w;   // 0ae0 min
        r0.x = (r1.wwww * r2.yyyy).x;   // 0af0 mul
        r0.yzw = (r1.xxyz + k0.wwww).yzw;   // 0b00 add
        r2.yzw = (max(r0, k1.xxxx)).yzw;   // 0b10 max
        r0.yzw = (r1.xxyz + (-r2)).yzw;   // 0b20 add
        r1.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0b30 lrp
        r0.yzw = (r2 * k1.yyyy).yzw;   // 0b44 mul
        r5.xyz = (max(k0.xxxx, r0.yzww)).xyz;   // 0b54 max
        r0.yzw = (r2 * r5.xxyz + r1.xxyz).yzw;   // 0b64 mad
        r1.xyz = (r0.xxxx * (r0.yzww - r3) + r3).xyz;   // 0b78 lrp
        r0.xyz = (r4 * r2.xxxx + r1).xyz;   // 0b8c mad
        r1.xyz = (v5.wwww * (v5 - r0) + r0).xyz;   // 0ba0 lrp
        r1.w = (k5.zzzz * v4.wwww).w;   // 0bb4 mul
        oC0 = RfxCmp(-abs(v4.wwww), k1.xxxx, r1);   // 0bc4 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_WATER) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_river_lod1
    void RfxWater_vs_river_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.52590219e-05, 6.0);
        const float4 k1 = float4(0.5, 0.0, 1.0, 5.0);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 0778 mov
        r1.w = (k1.yyyy).w;   // 0784 mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0790 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 079c rcp
        r3.xy = (gVertexOffset).xy;   // 07a8 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 07b4 mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 07c8 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 07d8 min
        r2.xy = (r2 * r3.xzzw).xy;   // 07e8 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 07f8 mad
        r2.zw = (k1.yyyy).zw;   // 080c mov
        r4 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 0818 texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 0828 texldl
        r1.z = (r4.yyyy * usr_river_tile_multiplier.xxxx).z;   // 0838 mul
        r5.x = (usr_river_tile_multiplier.xxxx).x;   // 0848 mov
        r1.x = (r4.xxxx * r5.xxxx + (-gFlowRate.xxxx)).x;   // 0854 mad
        r4 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r1.xzww).xy, (r1.xzww).w);   // 0868 texldl
        o[4].xy = (r1.xzzw).xy;   // 0878 mov
        r1.xy = (r2.xwzw * k0).xy;   // 0884 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0894 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 08a4 mul
        r1.z = (k0.zzzz).z;   // 08b4 mov
        r1.x = (r1.xxxx * r1.zzzz + gRiverHeightValue.yyyy).x;   // 08c0 mad
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 08d4 mul
        r1.y = ((-r2.zzzz) + k1.zzzz).y;   // 08e4 add
        r1.z = (r0.wwww * k1.wwww).z;   // 08f4 mul
        r3.y = (r1.zzzz * r4.xxxx + r1.xxxx).y;   // 0904 mad
        r3.w = (k1.zzzz).w;   // 0918 mov
        r2.x = (dot(r3, gWorld[0]).xxxx).x;   // 0924 dp4
        r2.y = (dot(r3, gWorld[2]).xxxx).y;   // 0934 dp4
        r2.z = (dot(r3, gWorld[3]).xxxx).z;   // 0944 dp4
        r1.x = (dot((r2).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).x;   // 0954 dp3
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0964 rcp
        r4.x = (dot((r2).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0970 dp3
        r4.y = (dot((r2).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0980 dp3
        r2.xy = (r1.xxxx * r4).xy;   // 0990 mul
        r2.zw = (k1.yyyy).zw;   // 09a0 mov
        r4 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r2).xy, (r2).w);   // 09ac texldl
        o[4].zw = (r2.xyxy).zw;   // 09bc mov
        r3.y = (r4.xxxx * k0.wwww + r3.yyyy).y;   // 09c8 mad
        r2.x = (dot(r3, gWorld[0]).xxxx).x;   // 09dc dp4
        r2.y = (dot(r3, gWorld[1]).xxxx).y;   // 09ec dp4
        r2.z = (dot(r3, gWorld[2]).xxxx).z;   // 09fc dp4
        r1.xzw = ((-r2.xyyz) + gCameraPos.xyyz).xzw;   // 0a0c add
        r2.x = (dot((r1.xzww).xyz, (r1.xzww).xyz).xxxx).x;   // 0a1c dp3
        r2.x = (rsqrt(abs(r2.x)).xxxx).x;   // 0a2c rsq
        r4.w = ((1.0 / r2.x).xxxx).w;   // 0a38 rcp
        r4.xyz = (r1.xzww * r2.xxxx).xyz;   // 0a44 mul
        r2 = r4.wwww * k2;   // 0a54 mul
        r2 = min(r2, k1.zzzz);   // 0a64 min
        r2 = (-r2) + k1.zzzz;   // 0a74 add
        r5.x = (r0.wwww * r2.xxxx).x;   // 0a84 mul
        r5.yzw = (max(r2, k3.xxyy)).yzw;   // 0a94 max
        r0 = r0 * r5;   // 0aa4 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 0ab4 max
        o[5].xyw = (r0).xyw;   // 0ac4 mov
        r0.x = (dot(r3, gWorldViewProjection[0]).xxxx).x;   // 0ad0 dp4
        r0.y = (dot(r3, gWorldViewProjection[1]).xxxx).y;   // 0ae0 dp4
        r0.z = (dot(r3, gWorldViewProjection[2]).xxxx).z;   // 0af0 dp4
        r0.w = (dot(r3, gWorldViewProjection[3]).xxxx).w;   // 0b00 dp4
        r1.x = ((1.0 / r0.w).xxxx).x;   // 0b10 rcp
        o[1] = r0 * r1.xxxx;   // 0b1c mul
        o[0] = r0;   // 0b2c mov
        o[2].x = (dot(r3, gWorldView[0]).xxxx).x;   // 0b38 dp4
        o[2].y = (dot(r3, gWorldView[1]).xxxx).y;   // 0b48 dp4
        o[2].z = (dot(r3, gWorldView[2]).xxxx).z;   // 0b58 dp4
        o[2].w = (dot(r3, gWorldView[3]).xxxx).w;   // 0b68 dp4
        r0.xyz = (usr_water_color_b).xyz;   // 0b78 mov
        r2.xyz = ((-r0) + usr_water_color_a).xyz;   // 0b84 add
        r0.w = (usr_amount_of_water_color_b.xxxx).w;   // 0b94 mov
        r2.w = ((-r0.wwww) + usr_amount_of_water_color_a.xxxx).w;   // 0ba0 add
        o[8] = r1.yyyy * r2 + r0;   // 0bb0 mad
        r0.xyz = (usr_color_filter_b).xyz;   // 0bc4 mov
        r0.xyz = ((-r0) + usr_color_filter_a).xyz;   // 0bd0 add
        o[7].xyz = (r1.yyyy * r0 + usr_color_filter_b).xyz;   // 0be0 mad
        r0.x = (usr_water_clarity_b.xxxx).x;   // 0bf4 mov
        r0.x = ((-r0.xxxx) + usr_water_clarity_a.xxxx).x;   // 0c00 add
        o[7].w = (r1.yyyy * r0.xxxx + usr_water_clarity_b.xxxx).w;   // 0c10 mad
        r0 = usr_deep_water_color_b;   // 0c24 mov
        r0 = (-r0) + usr_deep_water_color_a;   // 0c30 add
        o[9] = r1.yyyy * r0 + usr_deep_water_color_b;   // 0c40 mad
        r0.xyz = (usr_reflection_color_b).xyz;   // 0c54 mov
        r2.xyz = ((-r0) + usr_reflection_color_a).xyz;   // 0c60 add
        r0.w = (usr_amount_of_refl_color_b.xxxx).w;   // 0c70 mov
        r2.w = ((-r0.wwww) + usr_amount_of_refl_color_a.xxxx).w;   // 0c7c add
        o[10] = r1.yyyy * r2 + r0;   // 0c8c mad
        r0.x = (r4.wwww + (-gDistanceFog.xxxx)).x;   // 0ca0 add
        o[3] = r4;   // 0cb0 mov
        r0.y = (dot((gSunLight).xyz, (r4).xyz).xxxx).y;   // 0cbc dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0ccc max
        r0.y = (min(r0.yyyy, k1.zzzz)).y;   // 0cdc min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0cec mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0cfc add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0d0c rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0d18 mul
        r1 = gDistanceFogColorStart;   // 0d28 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0d34 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0d44 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0d58 mad
    }
    #endif
    #if defined(_WATER_WATER) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_river_lod1
    float4 RfxWater_ps_river_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k1 = float4(0.5, -0.5, 0.100000001, -0.200000003);
        const float4 k2 = float4(0.25, 20.0, 1.0, 10.0);
        const float4 k3 = float4(5.0, 4.0, 12.0, 1.0);
        const float4 k4 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k3.wwww).y;   // 0330 mov
        r1.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 033c add
        r1.zw = (v3.xywz).zw;   // 034c mov
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r1.xzzw).xy);   // 0358 texld
        r1 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r1.wyzw).xy);   // 0368 texld
        r3.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0378 add
        r3.zw = (v3.xywz).zw;   // 0388 mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 0394 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 03a4 texld
        r0.w = (r1.xxxx + (-r3.xxxx)).w;   // 03b4 add
        r1.x = (r2.xxxx + (-r4.xxxx)).x;   // 03c4 add
        r2.zw = (v3.xyyx).zw;   // 03d4 mov
        r1.y = ((1.0 / gTextureWidth.x).xxxx).y;   // 03e0 rcp
        r2.xy = (r1.yyyy + v3).xy;   // 03ec add
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 03fc texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 040c texld
        r4.xy = ((-r1.yyyy) + v3).xy;   // 041c add
        r4.zw = (v3.xyyx).zw;   // 042c mov
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r4.xzzw).xy);   // 0438 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r4.wyzw).xy);   // 0448 texld
        r1.z = (r2.xxxx + (-r4.xxxx)).z;   // 0458 add
        r1.w = (r3.xxxx + (-r5.xxxx)).w;   // 0468 add
        r2.xy = (v3 * k3.yyyy + r1.yyyy).xy;   // 0478 mad
        r3.xy = (v3 * k3.yyyy + (-r1.yyyy)).xy;   // 048c mad
        r2.zw = (k3.yyyy * v3.xyyx).zw;   // 04a0 mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 04b0 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 04c0 texld
        r3.zw = (k3.yyyy * v3.xyyx).zw;   // 04d0 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 04e0 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 04f0 texld
        r1.y = (r2.xxxx + (-r3.xxxx)).y;   // 0500 add
        r2.x = (r4.xxxx + (-r5.xxxx)).x;   // 0510 add
        r2.yz = (k3.xxxx * v4.xwxw).yz;   // 0520 mul
        r2.x = (r2.xxxx * r2.zzzz).x;   // 0530 mul
        r1.w = (r2.yyyy * r1.wwww + r2.xxxx).w;   // 0540 mad
        r1.x = (r1.xxxx * k3.zzzz + r1.wwww).x;   // 0554 mad
        r0.x = ((-r1.xxxx)).x;   // 0568 mov
        r1.x = (r1.yyyy * r2.zzzz).x;   // 0574 mul
        r1.x = (r2.yyyy * r1.zzzz + r1.xxxx).x;   // 0584 mad
        r0.z = (r0.wwww * k3.zzzz + r1.xxxx).z;   // 0598 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 05ac nrm
        r0.xy = (k3.yyyy * v3).xy;   // 05b8 mul
        r0 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r0).xy);   // 05c8 texld
        r0.x = (r0.xxxx * r2.zzzz).x;   // 05d8 mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (v3).xy);   // 05e8 texld
        r0.x = (r2.yyyy * r3.xxxx + r0.xxxx).x;   // 05f8 mad
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (v3.zwzw).xy);   // 060c texld
        r0.x = (r2.xxxx * k3.zzzz + r0.xxxx).x;   // 061c mad
        r0.xy = (r0.xxxx + k1.wyzw).xy;   // 0630 add
        r0.xy = (r0 * r1.yyyy).xy;   // 0640 mul
        r0.x = (r0.xxxx * k2.xxxx).x;   // 0650 mul
        r1.w = (saturate(max(r0.xxxx, r0.yyyy))).w;   // 0660 max
        r0.x = (r1.wwww * (-k2.yyyy) + k2.zzzz).x;   // 0670 mad
        r2.x = (max(r0.xxxx, k0.zzzz)).x;   // 0684 max
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 0694 dp3
        r0.y = ((-r0.xxxx) + k3.wwww).y;   // 06a4 add
        r0.x = (r0.xxxx + r0.xxxx).x;   // 06b4 add
        r0.xzw = (r1.xyyz * (-r0.xxxx) + v2.xyyz).xzw;   // 06c4 mad
        r0.xzw = ((-r0)).xzw;   // 06d8 mov
        r2.y = (r0.yyyy * r0.yyyy).y;   // 06e4 mul
        r2.y = (r2.yyyy * r2.yyyy).y;   // 06f4 mul
        r0.y = (r0.yyyy * r2.yyyy).y;   // 0704 mul
        r0.y = (r0.yyyy * k0.xxxx + k0.yyyy).y;   // 0714 mad
        r3.x = (min(r2.xxxx, r0.yyyy)).x;   // 0728 min
        r0.y = (r0.zzzz + k3.wwww).y;   // 0738 add
        r2 = RfxWaterWeather(SAMPLE_TEXTURECUBE(ReflectionSampler, samplerReflectionSampler, (r0.xzww).xyz), (r0.xzww).xyz);   // 0748 texld
        r2.w = (min(r0.yyyy, k3.wwww)).w;   // 0758 min
        r0.x = (r2.wwww * r3.xxxx).x;   // 0768 mul
        r0.y = (dot(((-v2)).xyz, (r1).xyz).xxxx).y;   // 0778 dp3
        r0.z = (r0.yyyy * (-r0.yyyy) + k3.wwww).z;   // 0788 mad
        r0.z = (r0.zzzz * (-k4.xxxx) + k4.yyyy).z;   // 079c mad
        r0.z = (rsqrt(abs(r0.z)).xxxx).z;   // 07b0 rsq
        r0.z = ((1.0 / r0.z).xxxx).z;   // 07bc rcp
        r0.y = (r0.yyyy * k4.zzzz + r0.zzzz).y;   // 07c8 mad
        r0.yz = (r1.xxzw * r0.yyyy).yz;   // 07dc mul
        r0.yz = (v2.xxzw * k4.wwww + (-r0)).yz;   // 07ec mad
        r0.yz = (r0 * v4.zzzz).yz;   // 0800 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0810 rcp
        r0.w = (r0.wwww * k1.xxxx + k1.xxxx).w;   // 081c mad
        r3.x = (v0.xxxx * k1.xxxx + r0.wwww).x;   // 0830 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 0844 rcp
        r0.w = (r0.wwww * k1.xxxx + k1.xxxx).w;   // 0850 mad
        r3.y = (v0.yyyy * k1.yyyy + r0.wwww).y;   // 0864 mad
        r0.yz = (r0 * k1.zzzz + r3.xxyw).yz;   // 0878 mad
        r4 = float4(0.0, 0.0, 0.0, 1.0);   // 088c texld
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 089c texld
        r4.yzw = (r4.xxyz).yzw;   // 08ac mov
        r6.xz = (g_ProjectionInverse[2].zyww).xz;   // 08b8 mov
        r6.yw = (g_ProjectionInverse[3].xzzw).yw;   // 08c4 mov
        r0.yz = (r6.xxyw * r5.xxxx + r6.xzww).yz;   // 08d0 mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 08e4 rcp
        r4.x = (r0.zzzz * r0.yyyy).x;   // 08f0 mul
        r0.y = (r0.yyyy * r0.zzzz + (-v1.zzzz)).y;   // 0900 mad
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 0914 texld
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0924 texld
        r3.yzw = (r3.xxyz).yzw;   // 0934 mov
        r0.zw = (r6.xyxy * r5.xxxx + r6).zw;   // 0940 mad
        r0.w = ((1.0 / r0.w).xxxx).w;   // 0954 rcp
        r3.x = (r0.wwww * r0.zzzz).x;   // 0960 mul
        r3 = RfxCmp(r0.yyyy, r4, r3);   // 0970 cmp
        r0.y = (r3.xxxx + (-v1.zzzz)).y;   // 0984 add
        r0.z = ((1.0 / v6.w).xxxx).z;   // 0994 rcp
        r0.z = (saturate(r0.zzzz * r0.yyyy)).z;   // 09a0 mul
        r4.x = (log2(abs(v6.x)).xxxx).x;   // 09b0 log
        r4.y = (log2(abs(v6.y)).xxxx).y;   // 09bc log
        r4.z = (log2(abs(v6.z)).xxxx).z;   // 09c8 log
        r4.xyz = (r0.yyyy * r4).xyz;   // 09d4 mul
        r0.y = (saturate(r0.yyyy)).y;   // 09e4 mov
        r0.y = ((-r0.yyyy) + k3.wwww).y;   // 09f0 add
        r0.y = ((dot((r0.yyyy).xy, (r1.xxxx).xy) + k0.z).xxxx).y;   // 0a00 dp2add
        r2.w = (max(r0.yyyy, r1.wwww)).w;   // 0a14 max
        r1.x = (saturate(exp2(r4.x).xxxx)).x;   // 0a24 exp
        r1.y = (saturate(exp2(r4.y).xxxx)).y;   // 0a30 exp
        r1.z = (saturate(exp2(r4.z).xxxx)).z;   // 0a3c exp
        r4.xyz = (r1 * r3.yzww).xyz;   // 0a48 mul
        r1.xyz = (r3.yzww * (-r1) + v8).xyz;   // 0a58 mad
        r0.yzw = (r0.zzzz * r1.xxyz + r4.xxyz).yzw;   // 0a6c mad
        r1.xyz = (v7.wwww * (v7 - r0.yzww) + r0.yzww).xyz;   // 0a80 lrp
        r0.yzw = (r2.xxyz + (-k3.wwww)).yzw;   // 0a94 add
        r3.xyz = (max(r0.yzww, k0.zzzz)).xyz;   // 0aa4 max
        r0.yzw = (r2.xxyz + (-r3.xxyz)).yzw;   // 0ab4 add
        r2.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0ac4 lrp
        r0.yzw = (r3.xxyz * k0.wwww).yzw;   // 0ad8 mul
        r4.xyz = (max(k3.wwww, r0.yzww)).xyz;   // 0ae8 max
        r0.yzw = (r3.xxyz * r4.xxyz + r2.xxyz).yzw;   // 0af8 mad
        r2.xyz = (r0.xxxx * (r0.yzww - r1) + r1).xyz;   // 0b0c lrp
        r0.xyz = (r2.wwww + r2).xyz;   // 0b20 add
        r1.xyz = (v5.wwww * (v5 - r0) + r0).xyz;   // 0b30 lrp
        r1.w = (k2.wwww * v4.wwww).w;   // 0b44 mul
        oC0 = RfxCmp(-abs(v4.wwww), k0.zzzz, r1);   // 0b54 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_OCEAN) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_lake_lod0
    void RfxWaterOcean_vs_lake_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, -1.0);
        const float4 k1 = float4(0.0078125, 0.200000003, 5.0, 6.0);
        const float4 k2 = float4(9.99999975e-06, 0.0, 0.0, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 065c add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 066c rcp
        r1.xy = (gVertexOffset).xy;   // 0678 mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0684 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 0698 max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 06a8 min
        r1.w = (k0.yyyy).w;   // 06b8 mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 06c4 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 06d4 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 06e4 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 06f4 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0704 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 0718 mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 0728 rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 0734 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 0744 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 0750 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 075c mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 076c mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + k0.wwww)).y;   // 0780 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0794 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 07a8 add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 07b8 mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 07cc mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 07dc add
        r0.x = ((1.0 / usr_wave_fade_distance.x).xxxx).x;   // 07ec rcp
        r0.x = (saturate(r0.xxxx * r3.wwww)).x;   // 07f8 mul
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 0808 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 0818 mul
        r0.x = (max(r0.xxxx, usr_wave_min.xxxx)).x;   // 0828 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 0838 mul
        r4.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0848 rcp
        r4.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0854 rcp
        r0.yz = (r1.xxzw * r4.xxyw).yz;   // 0860 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0870 mad
        r4.zw = (k0.zzzz).zw;   // 0884 mov
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4.xyww).xy, (r4.xyww).w);   // 0890 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 08a0 mul
        o[5].w = (r5.yyyy).w;   // 08b0 mov
        o[5].xy = (r0.xxxx * r2).xy;   // 08bc mul
        r0.y = (r3.wwww * k1.xxxx).y;   // 08cc mul
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 08dc min
        r0.y = ((-r0.yyyy) + k0.yyyy).y;   // 08ec add
        o[5].z = (max(r0.yyyy, k1.yyyy)).z;   // 08fc max
        r2.x = (gTilingRate.xxxx).x;   // 090c mov
        r0.y = (r2.xxxx * usr_wave_tile.xxxx).y;   // 0918 mul
        r2 = r4 * r0.yyyy;   // 0928 mul
        o[4].xy = (r4).xy;   // 0938 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 0944 texldl
        r0.x = (r0.xxxx * r2.xxxx).x;   // 0954 mul
        r1.y = (r0.xxxx * k1.zzzz).y;   // 0964 mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 0974 dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 0984 dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 0994 dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 09a4 dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 09b4 rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 09c0 dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 09d0 dp3
        r0.xy = (r0.wwww * r2).xy;   // 09e0 mul
        r0.zw = (k0.zzzz).zw;   // 09f0 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 09fc texldl
        o[4].zw = (r0.xyxy).zw;   // 0a0c mov
        r1.y = (r2.xxxx * k1.wwww + r1.yyyy).y;   // 0a18 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a2c dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a3c dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a4c dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a5c dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a6c rcp
        o[1] = r0 * r2.xxxx;   // 0a78 mul
        o[0] = r0;   // 0a88 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a94 dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0aa4 dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0ab4 dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0ac4 dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0ad4 add
        o[3] = r3;   // 0ae4 mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0af0 dp3
        r0.y = (max(r0.yyyy, k2.xxxx)).y;   // 0b00 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0b10 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b20 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0b30 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b40 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b4c mul
        r1 = gDistanceFogColorStart;   // 0b5c mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b68 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b78 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b8c mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0ba0 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0bac mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0bb8 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0bc4 mov
        o[9] = usr_deep_water_color_a;   // 0bd0 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0bdc mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0be8 mov
    }
    #endif
    #if defined(_WATER_OCEAN) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_lake_lod0
    float4 RfxWaterOcean_ps_lake_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, 0.25);
        const float4 k1 = float4(0.979600012, 0.0204000007, 1.0, -1.0);
        const float4 k2 = float4(0.0, 0.0500000007, 9.99999994e-09, 1.0);
        const float4 k3 = float4(0.100000001, 10.0, 0.0, 0.0);
        const float4 k4 = float4(9.99999975e-05, -9.99999975e-05, 0.5, -0.5);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 03fc rcp
        r1.x = (gTilingRate.xxxx).x;   // 0408 mov
        r0.y = (r1.xxxx * usr_wave_tile.xxxx).y;   // 0414 mul
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0424 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0434 mul
        r2.zw = (r1.xywz).zw;   // 0444 mov
        r0.z = ((1.0 / gTextureWidth.x).xxxx).z;   // 0450 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.zzzz)).xy;   // 045c mad
        r1.xy = (r0.xxxx * v3 + r0.zzzz).xy;   // 0470 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0484 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0494 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 04a4 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 04b4 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 04c4 add
        r0.w = ((-r3.xxxx) + r4.xxxx).w;   // 04d4 add
        r1.x = ((1.0 / gTilingRate.y).xxxx).x;   // 04e4 rcp
        r0.y = (r0.yyyy * r1.xxxx).y;   // 04f0 mul
        r1.xy = (r0.yyyy * v3 + r0.zzzz).xy;   // 0500 mad
        r2.xy = (r0.yyyy * v3 + (-r0.zzzz)).xy;   // 0514 mad
        r1.zw = (r0.yyyy * v3.xyxy).zw;   // 0528 mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 0538 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0548 texld
        r2.zw = (r1.xywz).zw;   // 0558 mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0564 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0574 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 0584 add
        r0.z = ((-r1.xxxx) + r3.xxxx).z;   // 0594 add
        r1.xy = (k0.xxxx * v4).xy;   // 05a4 mul
        r0.yz = (r0 * r1.yyyy).yz;   // 05b4 mul
        r0.z = (r1.xxxx * r0.wwww + r0.zzzz).z;   // 05c4 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 05d8 add
        r2.zw = (v3.xywz).zw;   // 05e8 mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 05f4 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0604 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0614 add
        r4.zw = (v3.xywz).zw;   // 0624 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0630 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0640 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 0650 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0660 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 0670 mad
        r2.x = ((-r0.zzzz)).x;   // 0684 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 0690 mad
        r2.z = (r0.wwww * k0.yyyy + r0.xxxx).z;   // 06a4 mad
        r2.y = (k0.zzzz).y;   // 06b8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 06c4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 06d0 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 06e0 mad
        r1.x = (r1.xxxx * (-k2.zzzz) + k2.wwww).x;   // 06f4 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0708 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0714 rcp
        r0.w = (r0.wwww * k4.xxxx + r1.xxxx).w;   // 0720 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0734 mul
        r1.xy = (v2.xzzw * k4.yyyy + (-r1)).xy;   // 0744 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0758 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0768 rcp
        r0.w = (r0.wwww * k4.zzzz + k4.zzzz).w;   // 0774 mad
        r2.x = (v0.xxxx * k4.zzzz + r0.wwww).x;   // 0788 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 079c rcp
        r0.w = (r0.wwww * k4.zzzz + k4.zzzz).w;   // 07a8 mad
        r2.y = (v0.yyyy * k4.wwww + r0.wwww).y;   // 07bc mad
        r1.xy = (r1 * k3.xxxx + r2).xy;   // 07d0 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 07e4 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 07f4 texld
        r3.yzw = (r3.xxyz).yzw;   // 0804 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0810 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 081c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0828 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 083c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0848 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0858 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 086c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 087c texld
        r2.yzw = (r2.xxyz).yzw;   // 088c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0898 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 08ac rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 08b8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 08c8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 08dc add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 08ec rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 08f8 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0908 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0914 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0920 log
        r2.xyz = (r0.wwww * r2).xyz;   // 092c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 093c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0948 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0954 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0960 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 0970 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 0984 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 0998 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 09ac dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 09bc add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 09cc add
        r1.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 09dc mad
        r0.xy = (r0.xzzw * k0.wwww).xy;   // 09f0 mul
        r0.xy = (v3 * k1.zwzw + (-r0)).xy;   // 0a00 mad
        r3 = SAMPLE_TEXTURE2D(usr_sampler_static_reflection, samplerusr_sampler_static_reflection, (float4((r0).x, 1.0 - (r0).y, (r0).zw)).xy);   // 0a14 texld
        r0.xyz = ((-r1)).xyz;   // 0a24 mov
        r1 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0).xyz), (r0).xyz);   // 0a30 texld
        r0.x = (r0.yyyy + k0.zzzz).x;   // 0a40 add
        r1.w = (min(r0.xxxx, k0.zzzz)).w;   // 0a50 min
        r0.xyz = (r3.wwww * (r3 - r1) + r1).xyz;   // 0a60 lrp
        r1.xyz = (r0 + (-k0.zzzz)).xyz;   // 0a74 add
        r3.xyz = (max(r1, k2.xxxx)).xyz;   // 0a84 max
        r0.xyz = (r0 + (-r3)).xyz;   // 0a94 add
        r1.xyz = (v9.wwww * (v9 - r0) + r0).xyz;   // 0aa4 lrp
        r0.xyz = (r3 * k2.yyyy).xyz;   // 0ab8 mul
        r4.xyz = (max(k0.zzzz, r0)).xyz;   // 0ac8 max
        r0.xyz = (r3 * r4 + r1).xyz;   // 0ad8 mad
        r1.x = (r0.wwww * r0.wwww).x;   // 0aec mul
        r1.x = (r1.xxxx * r1.xxxx).x;   // 0afc mul
        r0.w = (r0.wwww * r1.xxxx).w;   // 0b0c mul
        r0.w = (r0.wwww * k1.xxxx + k1.yyyy).w;   // 0b1c mad
        r0.w = (r1.wwww * r0.wwww).w;   // 0b30 mul
        r1.xyz = (r0.wwww * (r0 - r2) + r2).xyz;   // 0b40 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0b54 lrp
        r0.w = (k3.yyyy * v4.wwww).w;   // 0b68 mul
        oC0 = RfxCmp(-abs(v4.wwww), k2.xxxx, r0);   // 0b78 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_OCEAN) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_lake_lod1
    void RfxWaterOcean_vs_lake_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, -1.0);
        const float4 k1 = float4(5.0, 6.0, 9.99999975e-06, 0.0);
        const float4 k2 = float4(0.0, 1.0, 0.200000003, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 065c add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 066c rcp
        r1.xy = (gVertexOffset).xy;   // 0678 mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0684 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 0698 max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 06a8 min
        r1.w = (k0.yyyy).w;   // 06b8 mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 06c4 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 06d4 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 06e4 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 06f4 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0704 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 0718 mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 0728 rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 0734 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 0744 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 0750 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 075c mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 076c mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + k0.wwww)).y;   // 0780 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0794 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 07a8 add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 07b8 mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 07cc mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 07dc add
        r0.x = ((1.0 / usr_wave_fade_distance.x).xxxx).x;   // 07ec rcp
        r0.x = (saturate(r0.xxxx * r3.wwww)).x;   // 07f8 mul
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 0808 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 0818 mul
        r0.x = (max(r0.xxxx, usr_wave_min.xxxx)).x;   // 0828 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 0838 mul
        r4.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0848 rcp
        r4.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0854 rcp
        r0.yz = (r1.xxzw * r4.xxyw).yz;   // 0860 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0870 mad
        r4.zw = (k0.zzzz).zw;   // 0884 mov
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4.xyww).xy, (r4.xyww).w);   // 0890 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 08a0 mul
        o[5].zw = (r5.yyyy * k2.xyxy + k2.xyzx).zw;   // 08b0 mad
        o[5].xy = (r0.xxxx * r2).xy;   // 08c4 mul
        r0.x = (r0.xxxx * k1.xxxx).x;   // 08d4 mul
        r2.x = (gTilingRate.xxxx).x;   // 08e4 mov
        r0.y = (r2.xxxx * usr_wave_tile.xxxx).y;   // 08f0 mul
        r2 = r4 * r0.yyyy;   // 0900 mul
        o[4].xy = (r4).xy;   // 0910 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 091c texldl
        r1.y = (r0.xxxx * r2.xxxx).y;   // 092c mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 093c dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 094c dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 095c dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 096c dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 097c rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0988 dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0998 dp3
        r0.xy = (r0.wwww * r2).xy;   // 09a8 mul
        r0.zw = (k0.zzzz).zw;   // 09b8 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 09c4 texldl
        o[4].zw = (r0.xyxy).zw;   // 09d4 mov
        r1.y = (r2.xxxx * k1.yyyy + r1.yyyy).y;   // 09e0 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 09f4 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a04 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a14 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a24 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a34 rcp
        o[1] = r0 * r2.xxxx;   // 0a40 mul
        o[0] = r0;   // 0a50 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a5c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0a6c dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0a7c dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0a8c dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0a9c add
        o[3] = r3;   // 0aac mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0ab8 dp3
        r0.y = (max(r0.yyyy, k1.zzzz)).y;   // 0ac8 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0ad8 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0ae8 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0af8 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b08 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b14 mul
        r1 = gDistanceFogColorStart;   // 0b24 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b30 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b40 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b54 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0b68 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0b74 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0b80 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0b8c mov
        o[9] = usr_deep_water_color_a;   // 0b98 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0ba4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bb0 mov
    }
    #endif
    #if defined(_WATER_OCEAN) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_lake_lod1
    float4 RfxWaterOcean_ps_lake_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, 0.25);
        const float4 k1 = float4(0.979600012, 0.0204000007, 1.0, -1.0);
        const float4 k2 = float4(0.0, 0.0500000007, 9.99999994e-09, 1.0);
        const float4 k3 = float4(0.100000001, 0.0, 0.0, 0.0);
        const float4 k4 = float4(9.99999975e-05, -9.99999975e-05, 0.5, -0.5);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 03fc rcp
        r1.x = (gTilingRate.xxxx).x;   // 0408 mov
        r0.y = (r1.xxxx * usr_wave_tile.xxxx).y;   // 0414 mul
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0424 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0434 mul
        r2.zw = (r1.xywz).zw;   // 0444 mov
        r0.z = ((1.0 / gTextureWidth.x).xxxx).z;   // 0450 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.zzzz)).xy;   // 045c mad
        r1.xy = (r0.xxxx * v3 + r0.zzzz).xy;   // 0470 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0484 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0494 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 04a4 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 04b4 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 04c4 add
        r0.w = ((-r3.xxxx) + r4.xxxx).w;   // 04d4 add
        r1.x = ((1.0 / gTilingRate.y).xxxx).x;   // 04e4 rcp
        r0.y = (r0.yyyy * r1.xxxx).y;   // 04f0 mul
        r1.xy = (r0.yyyy * v3 + r0.zzzz).xy;   // 0500 mad
        r2.xy = (r0.yyyy * v3 + (-r0.zzzz)).xy;   // 0514 mad
        r1.zw = (r0.yyyy * v3.xyxy).zw;   // 0528 mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 0538 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0548 texld
        r2.zw = (r1.xywz).zw;   // 0558 mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0564 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0574 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 0584 add
        r0.z = ((-r1.xxxx) + r3.xxxx).z;   // 0594 add
        r1.xy = (k0.xxxx * v4).xy;   // 05a4 mul
        r0.yz = (r0 * r1.yyyy).yz;   // 05b4 mul
        r0.z = (r1.xxxx * r0.wwww + r0.zzzz).z;   // 05c4 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 05d8 add
        r2.zw = (v3.xywz).zw;   // 05e8 mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 05f4 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0604 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0614 add
        r4.zw = (v3.xywz).zw;   // 0624 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0630 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0640 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 0650 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0660 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 0670 mad
        r2.x = ((-r0.zzzz)).x;   // 0684 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 0690 mad
        r2.z = (r0.wwww * k0.yyyy + r0.xxxx).z;   // 06a4 mad
        r2.y = (k0.zzzz).y;   // 06b8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 06c4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 06d0 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 06e0 mad
        r1.x = (r1.xxxx * (-k2.zzzz) + k2.wwww).x;   // 06f4 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0708 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0714 rcp
        r0.w = (r0.wwww * k4.xxxx + r1.xxxx).w;   // 0720 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0734 mul
        r1.xy = (v2.xzzw * k4.yyyy + (-r1)).xy;   // 0744 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0758 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0768 rcp
        r0.w = (r0.wwww * k4.zzzz + k4.zzzz).w;   // 0774 mad
        r2.x = (v0.xxxx * k4.zzzz + r0.wwww).x;   // 0788 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 079c rcp
        r0.w = (r0.wwww * k4.zzzz + k4.zzzz).w;   // 07a8 mad
        r2.y = (v0.yyyy * k4.wwww + r0.wwww).y;   // 07bc mad
        r1.xy = (r1 * k3.xxxx + r2).xy;   // 07d0 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 07e4 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 07f4 texld
        r3.yzw = (r3.xxyz).yzw;   // 0804 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0810 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 081c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0828 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 083c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0848 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0858 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 086c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 087c texld
        r2.yzw = (r2.xxyz).yzw;   // 088c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0898 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 08ac rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 08b8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 08c8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 08dc add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 08ec rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 08f8 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0908 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0914 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0920 log
        r2.xyz = (r0.wwww * r2).xyz;   // 092c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 093c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0948 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0954 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0960 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 0970 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 0984 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 0998 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 09ac dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 09bc add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 09cc add
        r1.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 09dc mad
        r0.xy = (r0.xzzw * k0.wwww).xy;   // 09f0 mul
        r0.xy = (v3 * k1.zwzw + (-r0)).xy;   // 0a00 mad
        r3 = SAMPLE_TEXTURE2D(usr_sampler_static_reflection, samplerusr_sampler_static_reflection, (float4((r0).x, 1.0 - (r0).y, (r0).zw)).xy);   // 0a14 texld
        r0.xyz = ((-r1)).xyz;   // 0a24 mov
        r1 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0).xyz), (r0).xyz);   // 0a30 texld
        r0.x = (r0.yyyy + k0.zzzz).x;   // 0a40 add
        r1.w = (min(r0.xxxx, k0.zzzz)).w;   // 0a50 min
        r0.xyz = (r3.wwww * (r3 - r1) + r1).xyz;   // 0a60 lrp
        r1.xyz = (r0 + (-k0.zzzz)).xyz;   // 0a74 add
        r3.xyz = (max(r1, k2.xxxx)).xyz;   // 0a84 max
        r0.xyz = (r0 + (-r3)).xyz;   // 0a94 add
        r1.xyz = (v9.wwww * (v9 - r0) + r0).xyz;   // 0aa4 lrp
        r0.xyz = (r3 * k2.yyyy).xyz;   // 0ab8 mul
        r4.xyz = (max(k0.zzzz, r0)).xyz;   // 0ac8 max
        r0.xyz = (r3 * r4 + r1).xyz;   // 0ad8 mad
        r1.x = (r0.wwww * r0.wwww).x;   // 0aec mul
        r1.x = (r1.xxxx * r1.xxxx).x;   // 0afc mul
        r0.w = (r0.wwww * r1.xxxx).w;   // 0b0c mul
        r0.w = (r0.wwww * k1.xxxx + k1.yyyy).w;   // 0b1c mad
        r0.w = (r1.wwww * r0.wwww).w;   // 0b30 mul
        r1.xyz = (r0.wwww * (r0 - r2) + r2).xyz;   // 0b40 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0b54 lrp
        r0.w = (v4.wwww).w;   // 0b68 mov
        oC0 = RfxCmp(-abs(v4.wwww), k2.xxxx, r0);   // 0b74 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_OCEAN) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_river_lod0
    void RfxWaterOcean_vs_river_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.0, 6.0);
        const float4 k1 = float4(0.5, 0.0, 5.0, 1.52590219e-05);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 0660 mov
        r1.zw = (k1.yyyy).zw;   // 066c mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0678 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0684 rcp
        r3.xy = (gVertexOffset).xy;   // 0690 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 069c mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 06b0 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 06c0 min
        r2.xy = (r2 * r3.xzzw).xy;   // 06d0 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 06e0 mad
        r4.xy = (r2 + (-gUVOffset.zwzw)).xy;   // 06f4 add
        r1.xy = (r4 * gTileWidth.yzzw).xy;   // 0704 mul
        r1 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r1).xy, (r1).w);   // 0714 texldl
        r1.xy = (r1.xwzw * k0).xy;   // 0724 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0734 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 0744 mul
        r1.w = (k1.wwww).w;   // 0754 mov
        r1.x = (r1.xxxx * r1.wwww + gRiverHeightValue.yyyy).x;   // 0760 mad
        r4.w = (k1.yyyy).w;   // 0774 mov
        r2.zw = (k1.yyyy).zw;   // 0780 mov
        r5 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 078c texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 079c texldl
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 07ac mul
        r4.z = (r5.yyyy * usr_wave_scale.xxxx).z;   // 07bc mul
        r2.x = (usr_wave_scale.xxxx).x;   // 07cc mov
        r4.x = (r5.xxxx * r2.xxxx + (-gFlowRate.xxxx)).x;   // 07d8 mad
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r4.xzww).xy, (r4.xzww).w);   // 07ec texldl
        o[4].xy = (r4.xzzw).xy;   // 07fc mov
        r1.y = (r0.wwww * k1.zzzz).y;   // 0808 mul
        r3.y = (r1.yyyy * r2.xxxx + r1.xxxx).y;   // 0818 mad
        r3.w = (k0.zzzz).w;   // 082c mov
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0838 dp4
        r1.y = (dot(r3, gWorld[2]).xxxx).y;   // 0848 dp4
        r1.z = (dot(r3, gWorld[3]).xxxx).z;   // 0858 dp4
        r1.w = (dot((r1).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 0868 dp3
        r1.w = ((1.0 / r1.w).xxxx).w;   // 0878 rcp
        r2.x = (dot((r1).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0884 dp3
        r2.y = (dot((r1).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0894 dp3
        r1.xy = (r1.wwww * r2).xy;   // 08a4 mul
        r1.zw = (k1.yyyy).zw;   // 08b4 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r1).xy, (r1).w);   // 08c0 texldl
        o[4].zw = (r1.xyxy).zw;   // 08d0 mov
        r3.y = (r2.xxxx * k0.wwww + r3.yyyy).y;   // 08dc mad
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 08f0 dp4
        r1.y = (dot(r3, gWorld[1]).xxxx).y;   // 0900 dp4
        r1.z = (dot(r3, gWorld[2]).xxxx).z;   // 0910 dp4
        r1.w = (dot(r3, gWorld[3]).xxxx).w;   // 0920 dp4
        r2.xyz = ((-r1) + gCameraPos).xyz;   // 0930 add
        r2.w = (dot((r2).xyz, (r2).xyz).xxxx).w;   // 0940 dp3
        r2.w = (rsqrt(abs(r2.w)).xxxx).w;   // 0950 rsq
        r3.w = ((1.0 / r2.w).xxxx).w;   // 095c rcp
        r3.xyz = (r2.wwww * r2).xyz;   // 0968 mul
        r2 = r3.wwww * k2;   // 0978 mul
        r2 = min(r2, k0.zzzz);   // 0988 min
        r2 = (-r2) + k0.zzzz;   // 0998 add
        r4.x = (r0.wwww * r2.xxxx).x;   // 09a8 mul
        r4.yzw = (max(r2, k3.xxyy)).yzw;   // 09b8 max
        r0 = r0 * r4;   // 09c8 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 09d8 max
        o[5].xyw = (r0).xyw;   // 09e8 mov
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 09f4 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a04 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a14 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a24 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a34 rcp
        o[1] = r0 * r2.xxxx;   // 0a40 mul
        o[0] = r0;   // 0a50 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a5c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0a6c dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0a7c dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0a8c dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0a9c add
        o[3] = r3;   // 0aac mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0ab8 dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0ac8 max
        r0.y = (min(r0.yyyy, k0.zzzz)).y;   // 0ad8 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0ae8 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0af8 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b08 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b14 mul
        r1 = gDistanceFogColorStart;   // 0b24 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b30 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b40 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b54 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0b68 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0b74 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0b80 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0b8c mov
        o[9] = usr_deep_water_color_a;   // 0b98 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0ba4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bb0 mov
    }
    #endif
    #if defined(_WATER_OCEAN) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_river_lod0
    float4 RfxWaterOcean_ps_river_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 1.0, -1.0, 4.0);
        const float4 k1 = float4(12.0, 0.979600012, 0.0204000007, 0.25);
        const float4 k2 = float4(0.0, 0.0500000007, 9.99999994e-09, 1.0);
        const float4 k3 = float4(0.100000001, 10.0, 0.0, 0.0);
        const float4 k4 = float4(9.99999975e-05, -9.99999975e-05, 0.5, -0.5);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k0.yyyy).y;   // 0388 mov
        r1.zw = (v3.xyyx).zw;   // 0394 mov
        r0.w = ((1.0 / gTextureWidth.x).xxxx).w;   // 03a0 rcp
        r1.xy = (r0.wwww + v3).xy;   // 03ac add
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xzzw).xy);   // 03bc texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.wyzw).xy);   // 03cc texld
        r3.xy = ((-r0.wwww) + v3).xy;   // 03dc add
        r3.zw = (v3.xyyx).zw;   // 03ec mov
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 03f8 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 0408 texld
        r1.x = (r1.xxxx + (-r3.xxxx)).x;   // 0418 add
        r1.y = (r2.xxxx + (-r4.xxxx)).y;   // 0428 add
        r2.xy = (v3 * k0.wwww + r0.wwww).xy;   // 0438 mad
        r3.xy = (v3 * k0.wwww + (-r0.wwww)).xy;   // 044c mad
        r2.zw = (k0.wwww * v3.xyyx).zw;   // 0460 mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0470 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0480 texld
        r3.zw = (k0.wwww * v3.xyyx).zw;   // 0490 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 04a0 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 04b0 texld
        r0.w = (r2.xxxx + (-r3.xxxx)).w;   // 04c0 add
        r1.z = (r4.xxxx + (-r5.xxxx)).z;   // 04d0 add
        r2.xy = (k0.xxxx * v4.wxzw).xy;   // 04e0 mul
        r1.z = (r1.zzzz * r2.yyyy).z;   // 04f0 mul
        r1.y = (r2.xxxx * r1.yyyy + r1.zzzz).y;   // 0500 mad
        r3.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 0514 add
        r3.zw = (v3.xywz).zw;   // 0524 mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 0530 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 0540 texld
        r5.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0550 add
        r5.zw = (v3.xywz).zw;   // 0560 mov
        r6 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.xzzw).xy);   // 056c texld
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.wyzw).xy);   // 057c texld
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 058c add
        r1.w = (r4.xxxx + (-r6.xxxx)).w;   // 059c add
        r1.y = (r1.wwww * k1.xxxx + r1.yyyy).y;   // 05ac mad
        r0.x = ((-r1.yyyy)).x;   // 05c0 mov
        r0.w = (r0.wwww * r2.yyyy).w;   // 05cc mul
        r0.w = (r2.xxxx * r1.xxxx + r0.wwww).w;   // 05dc mad
        r0.z = (r1.zzzz * k1.xxxx + r0.wwww).z;   // 05f0 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 0604 nrm
        r0.x = (dot(((-v2)).xyz, (r1).xyz).xxxx).x;   // 0610 dp3
        r0.y = (r0.xxxx * (-r0.xxxx) + k0.yyyy).y;   // 0620 mad
        r0.y = (r0.yyyy * (-k2.zzzz) + k2.wwww).y;   // 0634 mad
        r0.y = (rsqrt(abs(r0.y)).xxxx).y;   // 0648 rsq
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0654 rcp
        r0.x = (r0.xxxx * k4.xxxx + r0.yyyy).x;   // 0660 mad
        r0.xy = (r1.xzzw * r0.xxxx).xy;   // 0674 mul
        r0.xy = (v2.xzzw * k4.yyyy + (-r0)).xy;   // 0684 mad
        r0.xy = (r0 * v4.zzzz).xy;   // 0698 mul
        r0.z = ((1.0 / gUVMultiplier.y).xxxx).z;   // 06a8 rcp
        r0.z = (r0.zzzz * k4.zzzz + k4.zzzz).z;   // 06b4 mad
        r2.x = (v0.xxxx * k4.zzzz + r0.zzzz).x;   // 06c8 mad
        r0.z = ((1.0 / gUVMultiplier.z).xxxx).z;   // 06dc rcp
        r0.z = (r0.zzzz * k4.zzzz + k4.zzzz).z;   // 06e8 mad
        r2.y = (v0.yyyy * k4.wwww + r0.zzzz).y;   // 06fc mad
        r0.xy = (r0 * k3.xxxx + r2).xy;   // 0710 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0724 texld
        r0 = float4(0.0, 0.0, 0.0, 1.0);   // 0734 texld
        r3.yzw = (r3.xxyz).yzw;   // 0744 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0750 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 075c mov
        r0.xy = (r4 * r0.xxxx + r4.zwzw).xy;   // 0768 mad
        r0.y = ((1.0 / r0.y).xxxx).y;   // 077c rcp
        r3.x = (r0.yyyy * r0.xxxx).x;   // 0788 mul
        r0.x = (r0.xxxx * r0.yyyy + (-v1.zzzz)).x;   // 0798 mad
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 07ac texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 07bc texld
        r2.yzw = (r2.xxyz).yzw;   // 07cc mov
        r0.yz = (r4.xxyw * r5.xxxx + r4.xzww).yz;   // 07d8 mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 07ec rcp
        r2.x = (r0.zzzz * r0.yyyy).x;   // 07f8 mul
        r0 = RfxCmp(r0.xxxx, r3, r2);   // 0808 cmp
        r0.x = (r0.xxxx + (-v1.zzzz)).x;   // 081c add
        r1.w = ((1.0 / v6.w).xxxx).w;   // 082c rcp
        r1.w = (saturate(r0.xxxx * r1.wwww)).w;   // 0838 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0848 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0854 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0860 log
        r2.xyz = (r0.xxxx * r2).xyz;   // 086c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 087c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0888 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0894 exp
        r2.xyz = (r0.yzww * r3).xyz;   // 08a0 mul
        r0.xyz = (r0.yzww * (-r3) + v8).xyz;   // 08b0 mad
        r0.xyz = (r1.wwww * r0 + r2).xyz;   // 08c4 mad
        r2.xyz = (v7.wwww * (v7 - r0) + r0).xyz;   // 08d8 lrp
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 08ec dp3
        r0.y = (r0.xxxx + r0.xxxx).y;   // 08fc add
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 090c add
        r0.yzw = (r1.xxyz * (-r0.yyyy) + v2.xxyz).yzw;   // 091c mad
        r0.yzw = ((-r0)).yzw;   // 0930 mov
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0.yzww).xyz), (r0.yzww).xyz);   // 093c texld
        r0.y = (r0.zzzz + k0.yyyy).y;   // 094c add
        r1.y = (min(r0.yyyy, k0.yyyy)).y;   // 095c min
        r0.yz = (k0 * v3.xxyw).yz;   // 096c mul
        r0.yz = (r1.xxzw * (-k1.wwww) + r0).yz;   // 097c mad
        r4 = SAMPLE_TEXTURE2D(usr_sampler_static_reflection, samplerusr_sampler_static_reflection, (float4((r0.yzzw).x, 1.0 - (r0.yzzw).y, (r0.yzzw).zw)).xy);   // 0990 texld
        r0.yzw = (r4.wwww * (r4.xxyz - r3.xxyz) + r3.xxyz).yzw;   // 09a0 lrp
        r1.xzw = (r0.yyzw + k0.zzzz).xzw;   // 09b4 add
        r3.xyz = (max(r1.xzww, k2.xxxx)).xyz;   // 09c4 max
        r0.yzw = (r0 + (-r3.xxyz)).yzw;   // 09d4 add
        r1.xzw = (v9.wwww * (v9.xyyz - r0.yyzw) + r0.yyzw).xzw;   // 09e4 lrp
        r0.yzw = (r3.xxyz * k2.yyyy).yzw;   // 09f8 mul
        r4.xyz = (max(k0.yyyy, r0.yzww)).xyz;   // 0a08 max
        r0.yzw = (r3.xxyz * r4.xxyz + r1.xxzw).yzw;   // 0a18 mad
        r1.x = (r0.xxxx * r0.xxxx).x;   // 0a2c mul
        r1.x = (r1.xxxx * r1.xxxx).x;   // 0a3c mul
        r0.x = (r0.xxxx * r1.xxxx).x;   // 0a4c mul
        r0.x = (r0.xxxx * k1.yyyy + k1.zzzz).x;   // 0a5c mad
        r0.x = (r1.yyyy * r0.xxxx).x;   // 0a70 mul
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0a80 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0a94 lrp
        r0.w = (k3.yyyy * v4.wwww).w;   // 0aa8 mul
        oC0 = RfxCmp(-abs(v4.wwww), k2.xxxx, r0);   // 0ab8 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_OCEAN) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_river_lod1
    void RfxWaterOcean_vs_river_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.0, 6.0);
        const float4 k1 = float4(0.5, 0.0, 5.0, 1.52590219e-05);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 06a0 mov
        r1.zw = (k1.yyyy).zw;   // 06ac mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 06b8 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 06c4 rcp
        r3.xy = (gVertexOffset).xy;   // 06d0 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 06dc mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 06f0 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 0700 min
        r2.xy = (r2 * r3.xzzw).xy;   // 0710 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 0720 mad
        r4.xy = (r2 + (-gUVOffset.zwzw)).xy;   // 0734 add
        r1.xy = (r4 * gTileWidth.yzzw).xy;   // 0744 mul
        r1 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r1).xy, (r1).w);   // 0754 texldl
        r1.xy = (r1.xwzw * k0).xy;   // 0764 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0774 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 0784 mul
        r1.w = (k1.wwww).w;   // 0794 mov
        r1.x = (r1.xxxx * r1.wwww + gRiverHeightValue.yyyy).x;   // 07a0 mad
        r4.w = (k1.yyyy).w;   // 07b4 mov
        r2.zw = (k1.yyyy).zw;   // 07c0 mov
        r5 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 07cc texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 07dc texldl
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 07ec mul
        r4.z = (r5.yyyy * usr_river_tile_multiplier.xxxx).z;   // 07fc mul
        r2.x = (usr_river_tile_multiplier.xxxx).x;   // 080c mov
        r4.x = (r5.xxxx * r2.xxxx + (-gFlowRate.xxxx)).x;   // 0818 mad
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r4.xzww).xy, (r4.xzww).w);   // 082c texldl
        o[4].xy = (r4.xzzw).xy;   // 083c mov
        r1.y = (r0.wwww * k1.zzzz).y;   // 0848 mul
        r3.y = (r1.yyyy * r2.xxxx + r1.xxxx).y;   // 0858 mad
        r3.w = (k0.zzzz).w;   // 086c mov
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0878 dp4
        r1.y = (dot(r3, gWorld[2]).xxxx).y;   // 0888 dp4
        r1.z = (dot(r3, gWorld[3]).xxxx).z;   // 0898 dp4
        r1.w = (dot((r1).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 08a8 dp3
        r1.w = ((1.0 / r1.w).xxxx).w;   // 08b8 rcp
        r2.x = (dot((r1).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 08c4 dp3
        r2.y = (dot((r1).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 08d4 dp3
        r1.xy = (r1.wwww * r2).xy;   // 08e4 mul
        r1.zw = (k1.yyyy).zw;   // 08f4 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r1).xy, (r1).w);   // 0900 texldl
        o[4].zw = (r1.xyxy).zw;   // 0910 mov
        r3.y = (r2.xxxx * k0.wwww + r3.yyyy).y;   // 091c mad
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0930 dp4
        r1.y = (dot(r3, gWorld[1]).xxxx).y;   // 0940 dp4
        r1.z = (dot(r3, gWorld[2]).xxxx).z;   // 0950 dp4
        r1.w = (dot(r3, gWorld[3]).xxxx).w;   // 0960 dp4
        r2.xyz = ((-r1) + gCameraPos).xyz;   // 0970 add
        r2.w = (dot((r2).xyz, (r2).xyz).xxxx).w;   // 0980 dp3
        r2.w = (rsqrt(abs(r2.w)).xxxx).w;   // 0990 rsq
        r3.w = ((1.0 / r2.w).xxxx).w;   // 099c rcp
        r3.xyz = (r2.wwww * r2).xyz;   // 09a8 mul
        r2 = r3.wwww * k2;   // 09b8 mul
        r2 = min(r2, k0.zzzz);   // 09c8 min
        r2 = (-r2) + k0.zzzz;   // 09d8 add
        r4.x = (r0.wwww * r2.xxxx).x;   // 09e8 mul
        r4.yzw = (max(r2, k3.xxyy)).yzw;   // 09f8 max
        r0 = r0 * r4;   // 0a08 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 0a18 max
        o[5].xyw = (r0).xyw;   // 0a28 mov
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a34 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a44 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a54 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a64 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a74 rcp
        o[1] = r0 * r2.xxxx;   // 0a80 mul
        o[0] = r0;   // 0a90 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a9c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0aac dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0abc dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0acc dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0adc add
        o[3] = r3;   // 0aec mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0af8 dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0b08 max
        r0.y = (min(r0.yyyy, k0.zzzz)).y;   // 0b18 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b28 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0b38 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b48 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b54 mul
        r1 = gDistanceFogColorStart;   // 0b64 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b70 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b80 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b94 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0ba8 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0bb4 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0bc0 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0bcc mov
        o[9] = usr_deep_water_color_a;   // 0bd8 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0be4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bf0 mov
    }
    #endif
    #if defined(_WATER_OCEAN) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_river_lod1
    float4 RfxWaterOcean_ps_river_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 1.0, -1.0, 4.0);
        const float4 k1 = float4(12.0, 0.979600012, 0.0204000007, 0.25);
        const float4 k2 = float4(0.0, 0.0500000007, 9.99999994e-09, 1.0);
        const float4 k3 = float4(0.100000001, 10.0, 0.0, 0.0);
        const float4 k4 = float4(9.99999975e-05, -9.99999975e-05, 0.5, -0.5);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k0.yyyy).y;   // 0388 mov
        r1.zw = (v3.xyyx).zw;   // 0394 mov
        r0.w = ((1.0 / gTextureWidth.x).xxxx).w;   // 03a0 rcp
        r1.xy = (r0.wwww + v3).xy;   // 03ac add
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xzzw).xy);   // 03bc texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.wyzw).xy);   // 03cc texld
        r3.xy = ((-r0.wwww) + v3).xy;   // 03dc add
        r3.zw = (v3.xyyx).zw;   // 03ec mov
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 03f8 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 0408 texld
        r1.x = (r1.xxxx + (-r3.xxxx)).x;   // 0418 add
        r1.y = (r2.xxxx + (-r4.xxxx)).y;   // 0428 add
        r2.xy = (v3 * k0.wwww + r0.wwww).xy;   // 0438 mad
        r3.xy = (v3 * k0.wwww + (-r0.wwww)).xy;   // 044c mad
        r2.zw = (k0.wwww * v3.xyyx).zw;   // 0460 mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0470 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0480 texld
        r3.zw = (k0.wwww * v3.xyyx).zw;   // 0490 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 04a0 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 04b0 texld
        r0.w = (r2.xxxx + (-r3.xxxx)).w;   // 04c0 add
        r1.z = (r4.xxxx + (-r5.xxxx)).z;   // 04d0 add
        r2.xy = (k0.xxxx * v4.wxzw).xy;   // 04e0 mul
        r1.z = (r1.zzzz * r2.yyyy).z;   // 04f0 mul
        r1.y = (r2.xxxx * r1.yyyy + r1.zzzz).y;   // 0500 mad
        r3.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 0514 add
        r3.zw = (v3.xywz).zw;   // 0524 mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 0530 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 0540 texld
        r5.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0550 add
        r5.zw = (v3.xywz).zw;   // 0560 mov
        r6 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.xzzw).xy);   // 056c texld
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.wyzw).xy);   // 057c texld
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 058c add
        r1.w = (r4.xxxx + (-r6.xxxx)).w;   // 059c add
        r1.y = (r1.wwww * k1.xxxx + r1.yyyy).y;   // 05ac mad
        r0.x = ((-r1.yyyy)).x;   // 05c0 mov
        r0.w = (r0.wwww * r2.yyyy).w;   // 05cc mul
        r0.w = (r2.xxxx * r1.xxxx + r0.wwww).w;   // 05dc mad
        r0.z = (r1.zzzz * k1.xxxx + r0.wwww).z;   // 05f0 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 0604 nrm
        r0.x = (dot(((-v2)).xyz, (r1).xyz).xxxx).x;   // 0610 dp3
        r0.y = (r0.xxxx * (-r0.xxxx) + k0.yyyy).y;   // 0620 mad
        r0.y = (r0.yyyy * (-k2.zzzz) + k2.wwww).y;   // 0634 mad
        r0.y = (rsqrt(abs(r0.y)).xxxx).y;   // 0648 rsq
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0654 rcp
        r0.x = (r0.xxxx * k4.xxxx + r0.yyyy).x;   // 0660 mad
        r0.xy = (r1.xzzw * r0.xxxx).xy;   // 0674 mul
        r0.xy = (v2.xzzw * k4.yyyy + (-r0)).xy;   // 0684 mad
        r0.xy = (r0 * v4.zzzz).xy;   // 0698 mul
        r0.z = ((1.0 / gUVMultiplier.y).xxxx).z;   // 06a8 rcp
        r0.z = (r0.zzzz * k4.zzzz + k4.zzzz).z;   // 06b4 mad
        r2.x = (v0.xxxx * k4.zzzz + r0.zzzz).x;   // 06c8 mad
        r0.z = ((1.0 / gUVMultiplier.z).xxxx).z;   // 06dc rcp
        r0.z = (r0.zzzz * k4.zzzz + k4.zzzz).z;   // 06e8 mad
        r2.y = (v0.yyyy * k4.wwww + r0.zzzz).y;   // 06fc mad
        r0.xy = (r0 * k3.xxxx + r2).xy;   // 0710 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0724 texld
        r0 = float4(0.0, 0.0, 0.0, 1.0);   // 0734 texld
        r3.yzw = (r3.xxyz).yzw;   // 0744 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0750 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 075c mov
        r0.xy = (r4 * r0.xxxx + r4.zwzw).xy;   // 0768 mad
        r0.y = ((1.0 / r0.y).xxxx).y;   // 077c rcp
        r3.x = (r0.yyyy * r0.xxxx).x;   // 0788 mul
        r0.x = (r0.xxxx * r0.yyyy + (-v1.zzzz)).x;   // 0798 mad
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 07ac texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 07bc texld
        r2.yzw = (r2.xxyz).yzw;   // 07cc mov
        r0.yz = (r4.xxyw * r5.xxxx + r4.xzww).yz;   // 07d8 mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 07ec rcp
        r2.x = (r0.zzzz * r0.yyyy).x;   // 07f8 mul
        r0 = RfxCmp(r0.xxxx, r3, r2);   // 0808 cmp
        r0.x = (r0.xxxx + (-v1.zzzz)).x;   // 081c add
        r1.w = ((1.0 / v6.w).xxxx).w;   // 082c rcp
        r1.w = (saturate(r0.xxxx * r1.wwww)).w;   // 0838 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0848 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0854 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0860 log
        r2.xyz = (r0.xxxx * r2).xyz;   // 086c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 087c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0888 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0894 exp
        r2.xyz = (r0.yzww * r3).xyz;   // 08a0 mul
        r0.xyz = (r0.yzww * (-r3) + v8).xyz;   // 08b0 mad
        r0.xyz = (r1.wwww * r0 + r2).xyz;   // 08c4 mad
        r2.xyz = (v7.wwww * (v7 - r0) + r0).xyz;   // 08d8 lrp
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 08ec dp3
        r0.y = (r0.xxxx + r0.xxxx).y;   // 08fc add
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 090c add
        r0.yzw = (r1.xxyz * (-r0.yyyy) + v2.xxyz).yzw;   // 091c mad
        r0.yzw = ((-r0)).yzw;   // 0930 mov
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0.yzww).xyz), (r0.yzww).xyz);   // 093c texld
        r0.y = (r0.zzzz + k0.yyyy).y;   // 094c add
        r1.y = (min(r0.yyyy, k0.yyyy)).y;   // 095c min
        r0.yz = (k0 * v3.xxyw).yz;   // 096c mul
        r0.yz = (r1.xxzw * (-k1.wwww) + r0).yz;   // 097c mad
        r4 = SAMPLE_TEXTURE2D(usr_sampler_static_reflection, samplerusr_sampler_static_reflection, (float4((r0.yzzw).x, 1.0 - (r0.yzzw).y, (r0.yzzw).zw)).xy);   // 0990 texld
        r0.yzw = (r4.wwww * (r4.xxyz - r3.xxyz) + r3.xxyz).yzw;   // 09a0 lrp
        r1.xzw = (r0.yyzw + k0.zzzz).xzw;   // 09b4 add
        r3.xyz = (max(r1.xzww, k2.xxxx)).xyz;   // 09c4 max
        r0.yzw = (r0 + (-r3.xxyz)).yzw;   // 09d4 add
        r1.xzw = (v9.wwww * (v9.xyyz - r0.yyzw) + r0.yyzw).xzw;   // 09e4 lrp
        r0.yzw = (r3.xxyz * k2.yyyy).yzw;   // 09f8 mul
        r4.xyz = (max(k0.yyyy, r0.yzww)).xyz;   // 0a08 max
        r0.yzw = (r3.xxyz * r4.xxyz + r1.xxzw).yzw;   // 0a18 mad
        r1.x = (r0.xxxx * r0.xxxx).x;   // 0a2c mul
        r1.x = (r1.xxxx * r1.xxxx).x;   // 0a3c mul
        r0.x = (r0.xxxx * r1.xxxx).x;   // 0a4c mul
        r0.x = (r0.xxxx * k1.yyyy + k1.zzzz).x;   // 0a5c mad
        r0.x = (r1.yyyy * r0.xxxx).x;   // 0a70 mul
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0a80 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0a94 lrp
        r0.w = (k3.yyyy * v4.wwww).w;   // 0aa8 mul
        oC0 = RfxCmp(-abs(v4.wwww), k2.xxxx, r0);   // 0ab8 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_POOL) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_lake_lod0
    void RfxWaterPool_vs_lake_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, -1.0);
        const float4 k1 = float4(0.0078125, 0.200000003, 5.0, 6.0);
        const float4 k2 = float4(9.99999975e-06, 0.0, 0.0, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 065c add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 066c rcp
        r1.xy = (gVertexOffset).xy;   // 0678 mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0684 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 0698 max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 06a8 min
        r1.w = (k0.yyyy).w;   // 06b8 mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 06c4 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 06d4 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 06e4 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 06f4 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0704 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 0718 mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 0728 rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 0734 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 0744 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 0750 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 075c mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 076c mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + k0.wwww)).y;   // 0780 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0794 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 07a8 add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 07b8 mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 07cc mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 07dc add
        r0.x = ((1.0 / usr_wave_fade_distance.x).xxxx).x;   // 07ec rcp
        r0.x = (saturate(r0.xxxx * r3.wwww)).x;   // 07f8 mul
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 0808 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 0818 mul
        r0.x = (max(r0.xxxx, usr_wave_min.xxxx)).x;   // 0828 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 0838 mul
        r4.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0848 rcp
        r4.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0854 rcp
        r0.yz = (r1.xxzw * r4.xxyw).yz;   // 0860 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0870 mad
        r4.zw = (k0.zzzz).zw;   // 0884 mov
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4.xyww).xy, (r4.xyww).w);   // 0890 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 08a0 mul
        o[5].w = (r5.yyyy).w;   // 08b0 mov
        o[5].xy = (r0.xxxx * r2).xy;   // 08bc mul
        r0.y = (r3.wwww * k1.xxxx).y;   // 08cc mul
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 08dc min
        r0.y = ((-r0.yyyy) + k0.yyyy).y;   // 08ec add
        o[5].z = (max(r0.yyyy, k1.yyyy)).z;   // 08fc max
        r2.x = (gTilingRate.xxxx).x;   // 090c mov
        r0.y = (r2.xxxx * usr_wave_tile.xxxx).y;   // 0918 mul
        r2 = r4 * r0.yyyy;   // 0928 mul
        o[4].xy = (r4).xy;   // 0938 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 0944 texldl
        r0.x = (r0.xxxx * r2.xxxx).x;   // 0954 mul
        r1.y = (r0.xxxx * k1.zzzz).y;   // 0964 mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 0974 dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 0984 dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 0994 dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 09a4 dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 09b4 rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 09c0 dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 09d0 dp3
        r0.xy = (r0.wwww * r2).xy;   // 09e0 mul
        r0.zw = (k0.zzzz).zw;   // 09f0 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 09fc texldl
        o[4].zw = (r0.xyxy).zw;   // 0a0c mov
        r1.y = (r2.xxxx * k1.wwww + r1.yyyy).y;   // 0a18 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a2c dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a3c dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a4c dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a5c dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a6c rcp
        o[1] = r0 * r2.xxxx;   // 0a78 mul
        o[0] = r0;   // 0a88 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a94 dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0aa4 dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0ab4 dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0ac4 dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0ad4 add
        o[3] = r3;   // 0ae4 mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0af0 dp3
        r0.y = (max(r0.yyyy, k2.xxxx)).y;   // 0b00 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0b10 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b20 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0b30 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b40 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b4c mul
        r1 = gDistanceFogColorStart;   // 0b5c mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b68 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b78 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b8c mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0ba0 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0bac mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0bb8 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0bc4 mov
        o[9] = usr_deep_water_color_a;   // 0bd0 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0bdc mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0be8 mov
    }
    #endif
    #if defined(_WATER_POOL) && !defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_lake_lod0
    float4 RfxWaterPool_ps_lake_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, -1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 10.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 0450 rcp
        r1.x = (gTilingRate.xxxx).x;   // 045c mov
        r0.y = (r1.xxxx * usr_wave_tile.xxxx).y;   // 0468 mul
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0478 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0488 mul
        r2.zw = (r1.xywz).zw;   // 0498 mov
        r0.z = ((1.0 / gTextureWidth.x).xxxx).z;   // 04a4 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.zzzz)).xy;   // 04b0 mad
        r1.xy = (r0.xxxx * v3 + r0.zzzz).xy;   // 04c4 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 04d8 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 04e8 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 04f8 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0508 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 0518 add
        r0.w = ((-r3.xxxx) + r4.xxxx).w;   // 0528 add
        r1.x = ((1.0 / gTilingRate.y).xxxx).x;   // 0538 rcp
        r0.y = (r0.yyyy * r1.xxxx).y;   // 0544 mul
        r1.xy = (r0.yyyy * v3 + r0.zzzz).xy;   // 0554 mad
        r2.xy = (r0.yyyy * v3 + (-r0.zzzz)).xy;   // 0568 mad
        r1.zw = (r0.yyyy * v3.xyxy).zw;   // 057c mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 058c texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 059c texld
        r2.zw = (r1.xywz).zw;   // 05ac mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 05b8 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 05c8 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 05d8 add
        r0.z = ((-r1.xxxx) + r3.xxxx).z;   // 05e8 add
        r1.xy = (k0.xxxx * v4).xy;   // 05f8 mul
        r0.yz = (r0 * r1.yyyy).yz;   // 0608 mul
        r0.z = (r1.xxxx * r0.wwww + r0.zzzz).z;   // 0618 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 062c add
        r2.zw = (v3.xywz).zw;   // 063c mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 0648 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0658 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0668 add
        r4.zw = (v3.xywz).zw;   // 0678 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0684 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0694 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 06a4 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 06b4 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 06c4 mad
        r1.z = ((1.0 / usr_ripple_map_size.x).xxxx).z;   // 06d8 rcp
        r2.xy = (r1.zzzz + v3).xy;   // 06e4 add
        r3.xy = ((-r1.zzzz) + v3).xy;   // 06f4 add
        r2.zw = (v3.xyyx).zw;   // 0704 mov
        r4 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r2.xzzw).x, 1.0 - (r2.xzzw).y, (r2.xzzw).zw)).xy);   // 0710 texld
        r2 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r2.wyzw).x, 1.0 - (r2.wyzw).y, (r2.wyzw).zw)).xy);   // 0720 texld
        r3.zw = (v3.xyyx).zw;   // 0730 mov
        r5 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r3.xzzw).x, 1.0 - (r3.xzzw).y, (r3.xzzw).zw)).xy);   // 073c texld
        r3 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r3.wyzw).x, 1.0 - (r3.wyzw).y, (r3.wyzw).zw)).xy);   // 074c texld
        r1.z = (r2.xxxx + (-r3.xxxx)).z;   // 075c add
        r1.w = (r4.xxxx + (-r5.xxxx)).w;   // 076c add
        r0.z = (usr_ripple_map_strength.xxxx * r1.wwww + r0.zzzz).z;   // 077c mad
        r2.x = ((-r0.zzzz)).x;   // 0790 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 079c mad
        r0.x = (r0.wwww * k0.yyyy + r0.xxxx).x;   // 07b0 mad
        r2.z = (usr_ripple_map_strength.xxxx * r1.zzzz + r0.xxxx).z;   // 07c4 mad
        r2.y = (k0.zzzz).y;   // 07d8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 07e4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 07f0 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 0800 mad
        r1.x = (r1.xxxx * (-k3.xxxx) + k3.yyyy).x;   // 0814 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0828 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0834 rcp
        r0.w = (r0.wwww * k3.zzzz + r1.xxxx).w;   // 0840 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0854 mul
        r1.xy = (v2.xzzw * k3.wwww + (-r1)).xy;   // 0864 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0878 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0888 rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 0894 mad
        r2.x = (v0.xxxx * k2.xxxx + r0.wwww).x;   // 08a8 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 08bc rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 08c8 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.wwww).y;   // 08dc mad
        r1.xy = (r1 * k2.zzzz + r2).xy;   // 08f0 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0904 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 0914 texld
        r3.yzw = (r3.xxyz).yzw;   // 0924 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0930 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 093c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0948 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 095c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0968 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0978 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 098c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 099c texld
        r2.yzw = (r2.xxyz).yzw;   // 09ac mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 09b8 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 09cc rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 09d8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 09e8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 09fc add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 0a0c rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 0a18 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0a28 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0a34 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0a40 log
        r2.xyz = (r0.wwww * r2).xyz;   // 0a4c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 0a5c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0a68 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0a74 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0a80 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 0a90 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 0aa4 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 0ab8 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 0acc dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 0adc add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 0aec add
        r0.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 0afc mad
        r0.xyz = ((-r0)).xyz;   // 0b10 mov
        r1.x = (r0.yyyy + k0.zzzz).x;   // 0b1c add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0).xyz), (r0).xyz);   // 0b2c texld
        r0.x = (min(r1.xxxx, k0.zzzz)).x;   // 0b3c min
        r0.y = (r0.wwww * r0.wwww).y;   // 0b4c mul
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b5c mul
        r0.y = (r0.yyyy * r0.wwww).y;   // 0b6c mul
        r0.y = (r0.yyyy * k1.xxxx + k1.yyyy).y;   // 0b7c mad
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0b90 mul
        r0.yzw = (r3.xxyz + k0.wwww).yzw;   // 0ba0 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 0bb0 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 0bc0 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0bd0 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 0be4 mul
        r4.xyz = (max(k0.zzzz, r0.yzww)).xyz;   // 0bf4 max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 0c04 mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0c18 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0c2c lrp
        r0.w = (k2.wwww * v4.wwww).w;   // 0c40 mul
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0c50 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_POOL) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_lake_lod1
    void RfxWaterPool_vs_lake_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(0.5, 1.0, 0.0, -1.0);
        const float4 k1 = float4(5.0, 6.0, 9.99999975e-06, 0.0);
        const float4 k2 = float4(0.0, 1.0, 0.200000003, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (gTilingRate.wwww + gTilingRate.wwww).x;   // 065c add
        r0.x = ((1.0 / r0.x).xxxx).x;   // 066c rcp
        r1.xy = (gVertexOffset).xy;   // 0678 mov
        r0.yz = (v0.xxyw * gMeshResolution.xxxx + r1.xxyw).yz;   // 0684 mad
        r0.yz = (max(r0, (-gVertexOffset.xzww))).yz;   // 0698 max
        r1.xz = (min(r0.yyzw, gVertexOffset.zyww)).xz;   // 06a8 min
        r1.w = (k0.yyyy).w;   // 06b8 mov
        r2.x = (dot((r1.xzww).xyz, (gWorld[0].xzww).xyz).xxxx).x;   // 06c4 dp3
        r2.y = (dot((r1.xzww).xyz, (gWorld[1].xzww).xyz).xxxx).y;   // 06d4 dp3
        r2.z = (dot((r1.xzww).xyz, (gWorld[2].xzww).xyz).xxxx).z;   // 06e4 dp3
        r0.yzw = ((-r2.xxyz) + gCameraPos.xxyz).yzw;   // 06f4 add
        r0.x = (saturate(r0.zzzz * r0.xxxx + k0.xxxx)).x;   // 0704 mad
        r2.x = (r0.xxxx * gTilingRate.wwww).x;   // 0718 mul
        r2.x = ((1.0 / r2.x).xxxx).x;   // 0728 rcp
        r2.y = (dot((r0.yzww).xyz, (r0.yzww).xyz).xxxx).y;   // 0734 dp3
        r2.y = (rsqrt(abs(r2.y)).xxxx).y;   // 0744 rsq
        r3.w = ((1.0 / r2.y).xxxx).w;   // 0750 rcp
        r3.xyz = (r0.yzww * r2.yyyy).xyz;   // 075c mul
        r0.x = (gTilingRate.wwww * (-r0.xxxx) + r3.wwww).x;   // 076c mad
        r0.y = (saturate(r0.xxxx * r2.xxxx + k0.wwww)).y;   // 0780 mad
        r0.x = (r0.xxxx * r2.xxxx + (-r0.yyyy)).x;   // 0794 mad
        r0.y = ((-r0.xxxx) + k0.yyyy).y;   // 07a8 add
        r0.z = (r0.yyyy * (-gTextureWidth.yyyy) + r0.yyyy).z;   // 07b8 mad
        r2.y = (r0.yyyy * gTextureWidth.yyyy).y;   // 07cc mul
        r2.x = (r0.zzzz + r0.xxxx).x;   // 07dc add
        r0.x = ((1.0 / usr_wave_fade_distance.x).xxxx).x;   // 07ec rcp
        r0.x = (saturate(r0.xxxx * r3.wwww)).x;   // 07f8 mul
        r0.x = ((-r0.xxxx) + k0.yyyy).x;   // 0808 add
        r0.x = (r0.xxxx * r0.xxxx).x;   // 0818 mul
        r0.x = (max(r0.xxxx, usr_wave_min.xxxx)).x;   // 0828 max
        r0.x = (r0.xxxx * usr_wave_scale.xxxx).x;   // 0838 mul
        r4.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0848 rcp
        r4.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0854 rcp
        r0.yz = (r1.xxzw * r4.xxyw).yz;   // 0860 mul
        r4.xy = (r0.yzzw * k0.xxxx + k0.xxxx).xy;   // 0870 mad
        r4.zw = (k0.zzzz).zw;   // 0884 mov
        r5 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r4.xyww).xy, (r4.xyww).w);   // 0890 texldl
        r0.x = (r0.xxxx * r5.yyyy).x;   // 08a0 mul
        o[5].zw = (r5.yyyy * k2.xyxy + k2.xyzx).zw;   // 08b0 mad
        o[5].xy = (r0.xxxx * r2).xy;   // 08c4 mul
        r0.x = (r0.xxxx * k1.xxxx).x;   // 08d4 mul
        r2.x = (gTilingRate.xxxx).x;   // 08e4 mov
        r0.y = (r2.xxxx * usr_wave_tile.xxxx).y;   // 08f0 mul
        r2 = r4 * r0.yyyy;   // 0900 mul
        o[4].xy = (r4).xy;   // 0910 mov
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r2).xy, (r2).w);   // 091c texldl
        r1.y = (r0.xxxx * r2.xxxx).y;   // 092c mul
        r0.x = (dot(r1, gWorld[0]).xxxx).x;   // 093c dp4
        r0.y = (dot(r1, gWorld[2]).xxxx).y;   // 094c dp4
        r0.z = (dot(r1, gWorld[3]).xxxx).z;   // 095c dp4
        r0.w = (dot((r0).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 096c dp3
        r0.w = ((1.0 / r0.w).xxxx).w;   // 097c rcp
        r2.x = (dot((r0).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0988 dp3
        r2.y = (dot((r0).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0998 dp3
        r0.xy = (r0.wwww * r2).xy;   // 09a8 mul
        r0.zw = (k0.zzzz).zw;   // 09b8 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r0).xy, (r0).w);   // 09c4 texldl
        o[4].zw = (r0.xyxy).zw;   // 09d4 mov
        r1.y = (r2.xxxx * k1.yyyy + r1.yyyy).y;   // 09e0 mad
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 09f4 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a04 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a14 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a24 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a34 rcp
        o[1] = r0 * r2.xxxx;   // 0a40 mul
        o[0] = r0;   // 0a50 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a5c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0a6c dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0a7c dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0a8c dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0a9c add
        o[3] = r3;   // 0aac mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0ab8 dp3
        r0.y = (max(r0.yyyy, k1.zzzz)).y;   // 0ac8 max
        r0.y = (min(r0.yyyy, k0.yyyy)).y;   // 0ad8 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0ae8 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0af8 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b08 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b14 mul
        r1 = gDistanceFogColorStart;   // 0b24 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b30 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b40 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b54 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0b68 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0b74 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0b80 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0b8c mov
        o[9] = usr_deep_water_color_a;   // 0b98 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0ba4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bb0 mov
    }
    #endif
    #if defined(_WATER_POOL) && !defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_lake_lod1
    float4 RfxWaterPool_ps_lake_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 12.0, 1.0, -1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 0.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = ((1.0 / gTilingRate.z).xxxx).x;   // 0450 rcp
        r1.x = (gTilingRate.xxxx).x;   // 045c mov
        r0.y = (r1.xxxx * usr_wave_tile.xxxx).y;   // 0468 mul
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0478 mul
        r1.zw = (r0.xxxx * v3.xyxy).zw;   // 0488 mul
        r2.zw = (r1.xywz).zw;   // 0498 mov
        r0.z = ((1.0 / gTextureWidth.x).xxxx).z;   // 04a4 rcp
        r2.xy = (r0.xxxx * v3 + (-r0.zzzz)).xy;   // 04b0 mad
        r1.xy = (r0.xxxx * v3 + r0.zzzz).xy;   // 04c4 mad
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 04d8 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 04e8 texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 04f8 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 0508 texld
        r0.x = ((-r2.xxxx) + r1.xxxx).x;   // 0518 add
        r0.w = ((-r3.xxxx) + r4.xxxx).w;   // 0528 add
        r1.x = ((1.0 / gTilingRate.y).xxxx).x;   // 0538 rcp
        r0.y = (r0.yyyy * r1.xxxx).y;   // 0544 mul
        r1.xy = (r0.yyyy * v3 + r0.zzzz).xy;   // 0554 mad
        r2.xy = (r0.yyyy * v3 + (-r0.zzzz)).xy;   // 0568 mad
        r1.zw = (r0.yyyy * v3.xyxy).zw;   // 057c mul
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xwzw).xy);   // 058c texld
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.zyzw).xy);   // 059c texld
        r2.zw = (r1.xywz).zw;   // 05ac mov
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 05b8 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 05c8 texld
        r0.y = ((-r2.xxxx) + r4.xxxx).y;   // 05d8 add
        r0.z = ((-r1.xxxx) + r3.xxxx).z;   // 05e8 add
        r1.xy = (k0.xxxx * v4).xy;   // 05f8 mul
        r0.yz = (r0 * r1.yyyy).yz;   // 0608 mul
        r0.z = (r1.xxxx * r0.wwww + r0.zzzz).z;   // 0618 mad
        r2.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 062c add
        r2.zw = (v3.xywz).zw;   // 063c mov
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.xzzw).xy);   // 0648 texld
        r2 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r2.wyzw).xy);   // 0658 texld
        r4.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 0668 add
        r4.zw = (v3.xywz).zw;   // 0678 mov
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.xzzw).xy);   // 0684 texld
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r4.wyzw).xy);   // 0694 texld
        r0.w = (r2.xxxx + (-r4.xxxx)).w;   // 06a4 add
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 06b4 add
        r0.z = (r1.zzzz * k0.yyyy + r0.zzzz).z;   // 06c4 mad
        r1.z = ((1.0 / usr_ripple_map_size.x).xxxx).z;   // 06d8 rcp
        r2.xy = (r1.zzzz + v3).xy;   // 06e4 add
        r3.xy = ((-r1.zzzz) + v3).xy;   // 06f4 add
        r2.zw = (v3.xyyx).zw;   // 0704 mov
        r4 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r2.xzzw).x, 1.0 - (r2.xzzw).y, (r2.xzzw).zw)).xy);   // 0710 texld
        r2 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r2.wyzw).x, 1.0 - (r2.wyzw).y, (r2.wyzw).zw)).xy);   // 0720 texld
        r3.zw = (v3.xyyx).zw;   // 0730 mov
        r5 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r3.xzzw).x, 1.0 - (r3.xzzw).y, (r3.xzzw).zw)).xy);   // 073c texld
        r3 = SAMPLE_TEXTURE2D(usr_sampler_ripple_map, samplerusr_sampler_ripple_map, (float4((r3.wyzw).x, 1.0 - (r3.wyzw).y, (r3.wyzw).zw)).xy);   // 074c texld
        r1.z = (r2.xxxx + (-r3.xxxx)).z;   // 075c add
        r1.w = (r4.xxxx + (-r5.xxxx)).w;   // 076c add
        r0.z = (usr_ripple_map_strength.xxxx * r1.wwww + r0.zzzz).z;   // 077c mad
        r2.x = ((-r0.zzzz)).x;   // 0790 mov
        r0.x = (r1.xxxx * r0.xxxx + r0.yyyy).x;   // 079c mad
        r0.x = (r0.wwww * k0.yyyy + r0.xxxx).x;   // 07b0 mad
        r2.z = (usr_ripple_map_strength.xxxx * r1.zzzz + r0.xxxx).z;   // 07c4 mad
        r2.y = (k0.zzzz).y;   // 07d8 mov
        r0.xyz = ((r2) * rsqrt(dot((r2).xyz, (r2).xyz))).xyz;   // 07e4 nrm
        r0.w = (dot(((-v2)).xyz, (r0).xyz).xxxx).w;   // 07f0 dp3
        r1.x = (r0.wwww * (-r0.wwww) + k0.zzzz).x;   // 0800 mad
        r1.x = (r1.xxxx * (-k3.xxxx) + k3.yyyy).x;   // 0814 mad
        r1.x = (rsqrt(abs(r1.x)).xxxx).x;   // 0828 rsq
        r1.x = ((1.0 / r1.x).xxxx).x;   // 0834 rcp
        r0.w = (r0.wwww * k3.zzzz + r1.xxxx).w;   // 0840 mad
        r1.xy = (r0.xzzw * r0.wwww).xy;   // 0854 mul
        r1.xy = (v2.xzzw * k3.wwww + (-r1)).xy;   // 0864 mad
        r1.xy = (r1 * v4.zzzz).xy;   // 0878 mul
        r0.w = ((1.0 / gUVMultiplier.y).xxxx).w;   // 0888 rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 0894 mad
        r2.x = (v0.xxxx * k2.xxxx + r0.wwww).x;   // 08a8 mad
        r0.w = ((1.0 / gUVMultiplier.z).xxxx).w;   // 08bc rcp
        r0.w = (r0.wwww * k2.xxxx + k2.xxxx).w;   // 08c8 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.wwww).y;   // 08dc mad
        r1.xy = (r1 * k2.zzzz + r2).xy;   // 08f0 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 0904 texld
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 0914 texld
        r3.yzw = (r3.xxyz).yzw;   // 0924 mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 0930 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 093c mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 0948 mad
        r0.w = ((1.0 / r1.y).xxxx).w;   // 095c rcp
        r3.x = (r0.wwww * r1.xxxx).x;   // 0968 mul
        r0.w = (r1.xxxx * r0.wwww + (-v1.zzzz)).w;   // 0978 mad
        r1 = float4(0.0, 0.0, 0.0, 1.0);   // 098c texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 099c texld
        r2.yzw = (r2.xxyz).yzw;   // 09ac mov
        r1.xy = (r4 * r1.xxxx + r4.zwzw).xy;   // 09b8 mad
        r1.y = ((1.0 / r1.y).xxxx).y;   // 09cc rcp
        r2.x = (r1.yyyy * r1.xxxx).x;   // 09d8 mul
        r1 = RfxCmp(r0.wwww, r3, r2);   // 09e8 cmp
        r0.w = (r1.xxxx + (-v1.zzzz)).w;   // 09fc add
        r1.x = ((1.0 / v6.w).xxxx).x;   // 0a0c rcp
        r1.x = (saturate(r0.wwww * r1.xxxx)).x;   // 0a18 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 0a28 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 0a34 log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 0a40 log
        r2.xyz = (r0.wwww * r2).xyz;   // 0a4c mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 0a5c exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0a68 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 0a74 exp
        r2.xyz = (r1.yzww * r3).xyz;   // 0a80 mul
        r1.yzw = (r1 * (-r3.xxyz) + v8.xxyz).yzw;   // 0a90 mad
        r1.xyz = (r1.xxxx * r1.yzww + r2).xyz;   // 0aa4 mad
        r2.xyz = (v7.wwww * (v7 - r1) + r1).xyz;   // 0ab8 lrp
        r0.w = (dot((v2).xyz, (r0).xyz).xxxx).w;   // 0acc dp3
        r1.x = (r0.wwww + r0.wwww).x;   // 0adc add
        r0.w = ((-r0.wwww) + k0.zzzz).w;   // 0aec add
        r0.xyz = (r0 * (-r1.xxxx) + v2).xyz;   // 0afc mad
        r0.xyz = ((-r0)).xyz;   // 0b10 mov
        r1.x = (r0.yyyy + k0.zzzz).x;   // 0b1c add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0).xyz), (r0).xyz);   // 0b2c texld
        r0.x = (min(r1.xxxx, k0.zzzz)).x;   // 0b3c min
        r0.y = (r0.wwww * r0.wwww).y;   // 0b4c mul
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b5c mul
        r0.y = (r0.yyyy * r0.wwww).y;   // 0b6c mul
        r0.y = (r0.yyyy * k1.xxxx + k1.yyyy).y;   // 0b7c mad
        r0.x = (r0.xxxx * r0.yyyy).x;   // 0b90 mul
        r0.yzw = (r3.xxyz + k0.wwww).yzw;   // 0ba0 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 0bb0 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 0bc0 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0bd0 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 0be4 mul
        r4.xyz = (max(k0.zzzz, r0.yzww)).xyz;   // 0bf4 max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 0c04 mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 0c18 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 0c2c lrp
        r0.w = (v4.wwww).w;   // 0c40 mov
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0c4c cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_POOL) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // vs_river_lod0
    void RfxWaterPool_vs_river_lod0(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.0, 6.0);
        const float4 k1 = float4(0.5, 0.0, 5.0, 1.52590219e-05);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 0660 mov
        r1.zw = (k1.yyyy).zw;   // 066c mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 0678 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 0684 rcp
        r3.xy = (gVertexOffset).xy;   // 0690 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 069c mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 06b0 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 06c0 min
        r2.xy = (r2 * r3.xzzw).xy;   // 06d0 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 06e0 mad
        r4.xy = (r2 + (-gUVOffset.zwzw)).xy;   // 06f4 add
        r1.xy = (r4 * gTileWidth.yzzw).xy;   // 0704 mul
        r1 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r1).xy, (r1).w);   // 0714 texldl
        r1.xy = (r1.xwzw * k0).xy;   // 0724 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0734 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 0744 mul
        r1.w = (k1.wwww).w;   // 0754 mov
        r1.x = (r1.xxxx * r1.wwww + gRiverHeightValue.yyyy).x;   // 0760 mad
        r4.w = (k1.yyyy).w;   // 0774 mov
        r2.zw = (k1.yyyy).zw;   // 0780 mov
        r5 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 078c texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 079c texldl
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 07ac mul
        r4.z = (r5.yyyy * usr_wave_scale.xxxx).z;   // 07bc mul
        r2.x = (usr_wave_scale.xxxx).x;   // 07cc mov
        r4.x = (r5.xxxx * r2.xxxx + (-gFlowRate.xxxx)).x;   // 07d8 mad
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r4.xzww).xy, (r4.xzww).w);   // 07ec texldl
        o[4].xy = (r4.xzzw).xy;   // 07fc mov
        r1.y = (r0.wwww * k1.zzzz).y;   // 0808 mul
        r3.y = (r1.yyyy * r2.xxxx + r1.xxxx).y;   // 0818 mad
        r3.w = (k0.zzzz).w;   // 082c mov
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0838 dp4
        r1.y = (dot(r3, gWorld[2]).xxxx).y;   // 0848 dp4
        r1.z = (dot(r3, gWorld[3]).xxxx).z;   // 0858 dp4
        r1.w = (dot((r1).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 0868 dp3
        r1.w = ((1.0 / r1.w).xxxx).w;   // 0878 rcp
        r2.x = (dot((r1).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 0884 dp3
        r2.y = (dot((r1).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 0894 dp3
        r1.xy = (r1.wwww * r2).xy;   // 08a4 mul
        r1.zw = (k1.yyyy).zw;   // 08b4 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r1).xy, (r1).w);   // 08c0 texldl
        o[4].zw = (r1.xyxy).zw;   // 08d0 mov
        r3.y = (r2.xxxx * k0.wwww + r3.yyyy).y;   // 08dc mad
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 08f0 dp4
        r1.y = (dot(r3, gWorld[1]).xxxx).y;   // 0900 dp4
        r1.z = (dot(r3, gWorld[2]).xxxx).z;   // 0910 dp4
        r1.w = (dot(r3, gWorld[3]).xxxx).w;   // 0920 dp4
        r2.xyz = ((-r1) + gCameraPos).xyz;   // 0930 add
        r2.w = (dot((r2).xyz, (r2).xyz).xxxx).w;   // 0940 dp3
        r2.w = (rsqrt(abs(r2.w)).xxxx).w;   // 0950 rsq
        r3.w = ((1.0 / r2.w).xxxx).w;   // 095c rcp
        r3.xyz = (r2.wwww * r2).xyz;   // 0968 mul
        r2 = r3.wwww * k2;   // 0978 mul
        r2 = min(r2, k0.zzzz);   // 0988 min
        r2 = (-r2) + k0.zzzz;   // 0998 add
        r4.x = (r0.wwww * r2.xxxx).x;   // 09a8 mul
        r4.yzw = (max(r2, k3.xxyy)).yzw;   // 09b8 max
        r0 = r0 * r4;   // 09c8 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 09d8 max
        o[5].xyw = (r0).xyw;   // 09e8 mov
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 09f4 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a04 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a14 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a24 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a34 rcp
        o[1] = r0 * r2.xxxx;   // 0a40 mul
        o[0] = r0;   // 0a50 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a5c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0a6c dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0a7c dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0a8c dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0a9c add
        o[3] = r3;   // 0aac mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0ab8 dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0ac8 max
        r0.y = (min(r0.yyyy, k0.zzzz)).y;   // 0ad8 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0ae8 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0af8 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b08 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b14 mul
        r1 = gDistanceFogColorStart;   // 0b24 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b30 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b40 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b54 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0b68 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0b74 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0b80 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0b8c mov
        o[9] = usr_deep_water_color_a;   // 0b98 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0ba4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bb0 mov
    }
    #endif
    #if defined(_WATER_POOL) && defined(_WATER_RIVER) && !defined(_WATER_LOD1)
    // ps_river_lod0
    float4 RfxWaterPool_ps_river_lod0(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 4.0, 12.0, 1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 10.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k0.wwww).y;   // 0320 mov
        r1.zw = (v3.xyyx).zw;   // 032c mov
        r0.w = ((1.0 / gTextureWidth.x).xxxx).w;   // 0338 rcp
        r1.xy = (r0.wwww + v3).xy;   // 0344 add
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xzzw).xy);   // 0354 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.wyzw).xy);   // 0364 texld
        r3.xy = ((-r0.wwww) + v3).xy;   // 0374 add
        r3.zw = (v3.xyyx).zw;   // 0384 mov
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 0390 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 03a0 texld
        r1.x = (r1.xxxx + (-r3.xxxx)).x;   // 03b0 add
        r1.y = (r2.xxxx + (-r4.xxxx)).y;   // 03c0 add
        r2.xy = (v3 * k0.yyyy + r0.wwww).xy;   // 03d0 mad
        r3.xy = (v3 * k0.yyyy + (-r0.wwww)).xy;   // 03e4 mad
        r2.zw = (k0.yyyy * v3.xyyx).zw;   // 03f8 mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0408 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0418 texld
        r3.zw = (k0.yyyy * v3.xyyx).zw;   // 0428 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 0438 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 0448 texld
        r0.w = (r2.xxxx + (-r3.xxxx)).w;   // 0458 add
        r1.z = (r4.xxxx + (-r5.xxxx)).z;   // 0468 add
        r2.xy = (k0.xxxx * v4.wxzw).xy;   // 0478 mul
        r1.z = (r1.zzzz * r2.yyyy).z;   // 0488 mul
        r1.y = (r2.xxxx * r1.yyyy + r1.zzzz).y;   // 0498 mad
        r3.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 04ac add
        r3.zw = (v3.xywz).zw;   // 04bc mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 04c8 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 04d8 texld
        r5.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 04e8 add
        r5.zw = (v3.xywz).zw;   // 04f8 mov
        r6 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.xzzw).xy);   // 0504 texld
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.wyzw).xy);   // 0514 texld
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0524 add
        r1.w = (r4.xxxx + (-r6.xxxx)).w;   // 0534 add
        r1.y = (r1.wwww * k0.zzzz + r1.yyyy).y;   // 0544 mad
        r0.x = ((-r1.yyyy)).x;   // 0558 mov
        r0.w = (r0.wwww * r2.yyyy).w;   // 0564 mul
        r0.w = (r2.xxxx * r1.xxxx + r0.wwww).w;   // 0574 mad
        r0.z = (r1.zzzz * k0.zzzz + r0.wwww).z;   // 0588 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 059c nrm
        r0.x = (dot(((-v2)).xyz, (r1).xyz).xxxx).x;   // 05a8 dp3
        r0.y = (r0.xxxx * (-r0.xxxx) + k0.wwww).y;   // 05b8 mad
        r0.y = (r0.yyyy * (-k3.xxxx) + k3.yyyy).y;   // 05cc mad
        r0.y = (rsqrt(abs(r0.y)).xxxx).y;   // 05e0 rsq
        r0.y = ((1.0 / r0.y).xxxx).y;   // 05ec rcp
        r0.x = (r0.xxxx * k3.zzzz + r0.yyyy).x;   // 05f8 mad
        r0.xy = (r1.xzzw * r0.xxxx).xy;   // 060c mul
        r0.xy = (v2.xzzw * k3.wwww + (-r0)).xy;   // 061c mad
        r0.xy = (r0 * v4.zzzz).xy;   // 0630 mul
        r0.z = ((1.0 / gUVMultiplier.y).xxxx).z;   // 0640 rcp
        r0.z = (r0.zzzz * k2.xxxx + k2.xxxx).z;   // 064c mad
        r2.x = (v0.xxxx * k2.xxxx + r0.zzzz).x;   // 0660 mad
        r0.z = ((1.0 / gUVMultiplier.z).xxxx).z;   // 0674 rcp
        r0.z = (r0.zzzz * k2.xxxx + k2.xxxx).z;   // 0680 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.zzzz).y;   // 0694 mad
        r0.xy = (r0 * k2.zzzz + r2).xy;   // 06a8 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 06bc texld
        r0 = float4(0.0, 0.0, 0.0, 1.0);   // 06cc texld
        r3.yzw = (r3.xxyz).yzw;   // 06dc mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 06e8 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 06f4 mov
        r0.xy = (r4 * r0.xxxx + r4.zwzw).xy;   // 0700 mad
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0714 rcp
        r3.x = (r0.yyyy * r0.xxxx).x;   // 0720 mul
        r0.x = (r0.xxxx * r0.yyyy + (-v1.zzzz)).x;   // 0730 mad
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 0744 texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 0754 texld
        r2.yzw = (r2.xxyz).yzw;   // 0764 mov
        r0.yz = (r4.xxyw * r5.xxxx + r4.xzww).yz;   // 0770 mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0784 rcp
        r2.x = (r0.zzzz * r0.yyyy).x;   // 0790 mul
        r0 = RfxCmp(r0.xxxx, r3, r2);   // 07a0 cmp
        r0.x = (r0.xxxx + (-v1.zzzz)).x;   // 07b4 add
        r1.w = ((1.0 / v6.w).xxxx).w;   // 07c4 rcp
        r1.w = (saturate(r0.xxxx * r1.wwww)).w;   // 07d0 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 07e0 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 07ec log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 07f8 log
        r2.xyz = (r0.xxxx * r2).xyz;   // 0804 mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 0814 exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0820 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 082c exp
        r2.xyz = (r0.yzww * r3).xyz;   // 0838 mul
        r0.xyz = (r0.yzww * (-r3) + v8).xyz;   // 0848 mad
        r0.xyz = (r1.wwww * r0 + r2).xyz;   // 085c mad
        r2.xyz = (v7.wwww * (v7 - r0) + r0).xyz;   // 0870 lrp
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 0884 dp3
        r0.y = (r0.xxxx + r0.xxxx).y;   // 0894 add
        r0.x = ((-r0.xxxx) + k0.wwww).x;   // 08a4 add
        r0.yzw = (r1.xxyz * (-r0.yyyy) + v2.xxyz).yzw;   // 08b4 mad
        r0.yzw = ((-r0)).yzw;   // 08c8 mov
        r1.x = (r0.zzzz + k0.wwww).x;   // 08d4 add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0.yzww).xyz), (r0.yzww).xyz);   // 08e4 texld
        r0.y = (min(r1.xxxx, k0.wwww)).y;   // 08f4 min
        r0.z = (r0.xxxx * r0.xxxx).z;   // 0904 mul
        r0.z = (r0.zzzz * r0.zzzz).z;   // 0914 mul
        r0.x = (r0.zzzz * r0.xxxx).x;   // 0924 mul
        r0.x = (r0.xxxx * k1.xxxx + k1.yyyy).x;   // 0934 mad
        r0.x = (r0.yyyy * r0.xxxx).x;   // 0948 mul
        r0.yzw = (r3.xxyz + (-k0.wwww)).yzw;   // 0958 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 0968 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 0978 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0988 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 099c mul
        r4.xyz = (max(k0.wwww, r0.yzww)).xyz;   // 09ac max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 09bc mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 09d0 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 09e4 lrp
        r0.w = (k2.wwww * v4.wwww).w;   // 09f8 mul
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0a08 cmp
        return oC0;
    }
    #endif
    #if defined(_WATER_POOL) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // vs_river_lod1
    void RfxWaterPool_vs_river_lod1(float4 v[4], out float4 o[11])
    {
        float4 v0 = v[0];   // position0
        o[0] = 0; o[1] = 0; o[2] = 0; o[3] = 0; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = 0; o[8] = 0; o[9] = 0; o[10] = 0;
        const float4 k0 = float4(65280.0, 255.0, 1.0, 6.0);
        const float4 k1 = float4(0.5, 0.0, 5.0, 1.52590219e-05);
        const float4 k2 = float4(0.00666666683, 0.00333333341, 0.0078125, 0.00400000019);
        const float4 k3 = float4(0.5, 0.200000003, 9.99999975e-06, 0.0);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
        r0.x = (k1.xxxx).x;   // 06a0 mov
        r1.zw = (k1.yyyy).zw;   // 06ac mov
        r2.x = ((1.0 / gVertexOffset.z).xxxx).x;   // 06b8 rcp
        r2.y = ((1.0 / gVertexOffset.w).xxxx).y;   // 06c4 rcp
        r3.xy = (gVertexOffset).xy;   // 06d0 mov
        r2.zw = (v0.xyxy * gMeshResolution.xxxx + r3.xyxy).zw;   // 06dc mad
        r2.zw = (max(r2, (-gVertexOffset))).zw;   // 06f0 max
        r3.xz = (min(r2.zyww, gVertexOffset.zyww)).xz;   // 0700 min
        r2.xy = (r2 * r3.xzzw).xy;   // 0710 mul
        r2.xy = (r2 * k1.xxxx + k1.xxxx).xy;   // 0720 mad
        r4.xy = (r2 + (-gUVOffset.zwzw)).xy;   // 0734 add
        r1.xy = (r4 * gTileWidth.yzzw).xy;   // 0744 mul
        r1 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r1).xy, (r1).w);   // 0754 texldl
        r1.xy = (r1.xwzw * k0).xy;   // 0764 mul
        r1.x = (r1.yyyy + r1.xxxx).x;   // 0774 add
        r1.x = (r1.xxxx * gRiverHeightValue.xxxx).x;   // 0784 mul
        r1.w = (k1.wwww).w;   // 0794 mov
        r1.x = (r1.xxxx * r1.wwww + gRiverHeightValue.yyyy).x;   // 07a0 mad
        r4.w = (k1.yyyy).w;   // 07b4 mov
        r2.zw = (k1.yyyy).zw;   // 07c0 mov
        r5 = SAMPLE_TEXTURE2D_LOD(UVVertSampler, samplerUVVertSampler, (r2.xyww).xy, (r2.xyww).w);   // 07cc texldl
        r2 = SAMPLE_TEXTURE2D_LOD(DescVertSampler, samplerDescVertSampler, (r2).xy, (r2).w);   // 07dc texldl
        r0.yzw = (r2.yyyy * usr_wave_scale.xxxx).yzw;   // 07ec mul
        r4.z = (r5.yyyy * usr_river_tile_multiplier.xxxx).z;   // 07fc mul
        r2.x = (usr_river_tile_multiplier.xxxx).x;   // 080c mov
        r4.x = (r5.xxxx * r2.xxxx + (-gFlowRate.xxxx)).x;   // 0818 mad
        r2 = SAMPLE_TEXTURE2D_LOD(OceanMapVertSampler, samplerOceanMapVertSampler, (r4.xzww).xy, (r4.xzww).w);   // 082c texldl
        o[4].xy = (r4.xzzw).xy;   // 083c mov
        r1.y = (r0.wwww * k1.zzzz).y;   // 0848 mul
        r3.y = (r1.yyyy * r2.xxxx + r1.xxxx).y;   // 0858 mad
        r3.w = (k0.zzzz).w;   // 086c mov
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0878 dp4
        r1.y = (dot(r3, gWorld[2]).xxxx).y;   // 0888 dp4
        r1.z = (dot(r3, gWorld[3]).xxxx).z;   // 0898 dp4
        r1.w = (dot((r1).xyz, (gRotationMatrix[3].xzww).xyz).xxxx).w;   // 08a8 dp3
        r1.w = ((1.0 / r1.w).xxxx).w;   // 08b8 rcp
        r2.x = (dot((r1).xyz, (gRotationMatrix[0].xzww).xyz).xxxx).x;   // 08c4 dp3
        r2.y = (dot((r1).xyz, (gRotationMatrix[1].xzww).xyz).xxxx).y;   // 08d4 dp3
        r1.xy = (r1.wwww * r2).xy;   // 08e4 mul
        r1.zw = (k1.yyyy).zw;   // 08f4 mov
        r2 = SAMPLE_TEXTURE2D_LOD(WakeVertSampler, samplerWakeVertSampler, (r1).xy, (r1).w);   // 0900 texldl
        o[4].zw = (r1.xyxy).zw;   // 0910 mov
        r3.y = (r2.xxxx * k0.wwww + r3.yyyy).y;   // 091c mad
        r1.x = (dot(r3, gWorld[0]).xxxx).x;   // 0930 dp4
        r1.y = (dot(r3, gWorld[1]).xxxx).y;   // 0940 dp4
        r1.z = (dot(r3, gWorld[2]).xxxx).z;   // 0950 dp4
        r1.w = (dot(r3, gWorld[3]).xxxx).w;   // 0960 dp4
        r2.xyz = ((-r1) + gCameraPos).xyz;   // 0970 add
        r2.w = (dot((r2).xyz, (r2).xyz).xxxx).w;   // 0980 dp3
        r2.w = (rsqrt(abs(r2.w)).xxxx).w;   // 0990 rsq
        r3.w = ((1.0 / r2.w).xxxx).w;   // 099c rcp
        r3.xyz = (r2.wwww * r2).xyz;   // 09a8 mul
        r2 = r3.wwww * k2;   // 09b8 mul
        r2 = min(r2, k0.zzzz);   // 09c8 min
        r2 = (-r2) + k0.zzzz;   // 09d8 add
        r4.x = (r0.wwww * r2.xxxx).x;   // 09e8 mul
        r4.yzw = (max(r2, k3.xxyy)).yzw;   // 09f8 max
        r0 = r0 * r4;   // 0a08 mul
        o[5].z = (max(r0.zzzz, k3.yyyy)).z;   // 0a18 max
        o[5].xyw = (r0).xyw;   // 0a28 mov
        r0.x = (dot(r1, gWorldViewProjection[0]).xxxx).x;   // 0a34 dp4
        r0.y = (dot(r1, gWorldViewProjection[1]).xxxx).y;   // 0a44 dp4
        r0.z = (dot(r1, gWorldViewProjection[2]).xxxx).z;   // 0a54 dp4
        r0.w = (dot(r1, gWorldViewProjection[3]).xxxx).w;   // 0a64 dp4
        r2.x = ((1.0 / r0.w).xxxx).x;   // 0a74 rcp
        o[1] = r0 * r2.xxxx;   // 0a80 mul
        o[0] = r0;   // 0a90 mov
        o[2].x = (dot(r1, gWorldView[0]).xxxx).x;   // 0a9c dp4
        o[2].y = (dot(r1, gWorldView[1]).xxxx).y;   // 0aac dp4
        o[2].z = (dot(r1, gWorldView[2]).xxxx).z;   // 0abc dp4
        o[2].w = (dot(r1, gWorldView[3]).xxxx).w;   // 0acc dp4
        r0.x = (r3.wwww + (-gDistanceFog.xxxx)).x;   // 0adc add
        o[3] = r3;   // 0aec mov
        r0.y = (dot((gSunLight).xyz, (r3).xyz).xxxx).y;   // 0af8 dp3
        r0.y = (max(r0.yyyy, k3.zzzz)).y;   // 0b08 max
        r0.y = (min(r0.yyyy, k0.zzzz)).y;   // 0b18 min
        r0.y = (r0.yyyy * r0.yyyy).y;   // 0b28 mul
        r0.z = ((-gDistanceFog.xxxx) + gDistanceFog.yyyy).z;   // 0b38 add
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0b48 rcp
        r0.x = (saturate(r0.zzzz * r0.xxxx)).x;   // 0b54 mul
        r1 = gDistanceFogColorStart;   // 0b64 mov
        r1 = (-r1.wxyz) + gDistanceFogColorEnd.wxyz;   // 0b70 add
        o[6].w = (r0.xxxx * r1.xxxx + gDistanceFogColorStart.wwww).w;   // 0b80 mad
        o[6].xyz = (r0.yyyy * r1.yzww + gDistanceFogColorStart).xyz;   // 0b94 mad
        o[7].xyz = (usr_color_filter_a).xyz;   // 0ba8 mov
        o[7].w = (usr_water_clarity_a.xxxx).w;   // 0bb4 mov
        o[8].xyz = (usr_water_color_a).xyz;   // 0bc0 mov
        o[8].w = (usr_amount_of_water_color_a.xxxx).w;   // 0bcc mov
        o[9] = usr_deep_water_color_a;   // 0bd8 mov
        o[10].xyz = (usr_reflection_color_a).xyz;   // 0be4 mov
        o[10].w = (usr_amount_of_refl_color_a.xxxx).w;   // 0bf0 mov
    }
    #endif
    #if defined(_WATER_POOL) && defined(_WATER_RIVER) && defined(_WATER_LOD1)
    // ps_river_lod1
    float4 RfxWaterPool_ps_river_lod1(float4 v[10])
    {
        float4 v0 = v[0];   // texcoord0
        float4 v1 = v[1];   // texcoord1
        float4 v2 = v[2];   // texcoord2
        float4 v3 = v[3];   // texcoord3
        float4 v4 = v[4];   // texcoord4
        float4 v5 = v[5];   // texcoord5
        float4 v6 = v[6];   // texcoord6
        float4 v7 = v[7];   // texcoord7
        float4 v8 = v[8];   // texcoord8
        float4 v9 = v[9];   // texcoord9
        float4 oC0 = 0;
        const float4 k0 = float4(5.0, 4.0, 12.0, 1.0);
        const float4 k1 = float4(0.979600012, 0.0204000007, 0.0, 0.0500000007);
        const float4 k2 = float4(0.5, -0.5, 0.100000001, 10.0);
        const float4 k3 = float4(9.99999994e-09, 1.0, 9.99999975e-05, -9.99999975e-05);
        float4 r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0;
        r0.y = (k0.wwww).y;   // 0320 mov
        r1.zw = (v3.xyyx).zw;   // 032c mov
        r0.w = ((1.0 / gTextureWidth.x).xxxx).w;   // 0338 rcp
        r1.xy = (r0.wwww + v3).xy;   // 0344 add
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.xzzw).xy);   // 0354 texld
        r1 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r1.wyzw).xy);   // 0364 texld
        r3.xy = ((-r0.wwww) + v3).xy;   // 0374 add
        r3.zw = (v3.xyyx).zw;   // 0384 mov
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 0390 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 03a0 texld
        r1.x = (r1.xxxx + (-r3.xxxx)).x;   // 03b0 add
        r1.y = (r2.xxxx + (-r4.xxxx)).y;   // 03c0 add
        r2.xy = (v3 * k0.yyyy + r0.wwww).xy;   // 03d0 mad
        r3.xy = (v3 * k0.yyyy + (-r0.wwww)).xy;   // 03e4 mad
        r2.zw = (k0.yyyy * v3.xyyx).zw;   // 03f8 mul
        r4 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.xzzw).xy);   // 0408 texld
        r2 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r2.wyzw).xy);   // 0418 texld
        r3.zw = (k0.yyyy * v3.xyyx).zw;   // 0428 mul
        r5 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.xzzw).xy);   // 0438 texld
        r3 = SAMPLE_TEXTURE2D(OceanWaveSampler, samplerOceanWaveSampler, (r3.wyzw).xy);   // 0448 texld
        r0.w = (r2.xxxx + (-r3.xxxx)).w;   // 0458 add
        r1.z = (r4.xxxx + (-r5.xxxx)).z;   // 0468 add
        r2.xy = (k0.xxxx * v4.wxzw).xy;   // 0478 mul
        r1.z = (r1.zzzz * r2.yyyy).z;   // 0488 mul
        r1.y = (r2.xxxx * r1.yyyy + r1.zzzz).y;   // 0498 mad
        r3.xy = (gTextureWidth.zzzz + v3.zwzw).xy;   // 04ac add
        r3.zw = (v3.xywz).zw;   // 04bc mov
        r4 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.xzzw).xy);   // 04c8 texld
        r3 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r3.wyzw).xy);   // 04d8 texld
        r5.xy = ((-gTextureWidth.zzzz) + v3.zwzw).xy;   // 04e8 add
        r5.zw = (v3.xywz).zw;   // 04f8 mov
        r6 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.xzzw).xy);   // 0504 texld
        r5 = SAMPLE_TEXTURE2D(WakeSampler, samplerWakeSampler, (r5.wyzw).xy);   // 0514 texld
        r1.z = (r3.xxxx + (-r5.xxxx)).z;   // 0524 add
        r1.w = (r4.xxxx + (-r6.xxxx)).w;   // 0534 add
        r1.y = (r1.wwww * k0.zzzz + r1.yyyy).y;   // 0544 mad
        r0.x = ((-r1.yyyy)).x;   // 0558 mov
        r0.w = (r0.wwww * r2.yyyy).w;   // 0564 mul
        r0.w = (r2.xxxx * r1.xxxx + r0.wwww).w;   // 0574 mad
        r0.z = (r1.zzzz * k0.zzzz + r0.wwww).z;   // 0588 mad
        r1.xyz = ((r0) * rsqrt(dot((r0).xyz, (r0).xyz))).xyz;   // 059c nrm
        r0.x = (dot(((-v2)).xyz, (r1).xyz).xxxx).x;   // 05a8 dp3
        r0.y = (r0.xxxx * (-r0.xxxx) + k0.wwww).y;   // 05b8 mad
        r0.y = (r0.yyyy * (-k3.xxxx) + k3.yyyy).y;   // 05cc mad
        r0.y = (rsqrt(abs(r0.y)).xxxx).y;   // 05e0 rsq
        r0.y = ((1.0 / r0.y).xxxx).y;   // 05ec rcp
        r0.x = (r0.xxxx * k3.zzzz + r0.yyyy).x;   // 05f8 mad
        r0.xy = (r1.xzzw * r0.xxxx).xy;   // 060c mul
        r0.xy = (v2.xzzw * k3.wwww + (-r0)).xy;   // 061c mad
        r0.xy = (r0 * v4.zzzz).xy;   // 0630 mul
        r0.z = ((1.0 / gUVMultiplier.y).xxxx).z;   // 0640 rcp
        r0.z = (r0.zzzz * k2.xxxx + k2.xxxx).z;   // 064c mad
        r2.x = (v0.xxxx * k2.xxxx + r0.zzzz).x;   // 0660 mad
        r0.z = ((1.0 / gUVMultiplier.z).xxxx).z;   // 0674 rcp
        r0.z = (r0.zzzz * k2.xxxx + k2.xxxx).z;   // 0680 mad
        r2.y = (v0.yyyy * k2.yyyy + r0.zzzz).y;   // 0694 mad
        r0.xy = (r0 * k2.zzzz + r2).xy;   // 06a8 mad
        r3 = float4(0.0, 0.0, 0.0, 1.0);   // 06bc texld
        r0 = float4(0.0, 0.0, 0.0, 1.0);   // 06cc texld
        r3.yzw = (r3.xxyz).yzw;   // 06dc mov
        r4.xz = (g_ProjectionInverse[2].zyww).xz;   // 06e8 mov
        r4.yw = (g_ProjectionInverse[3].xzzw).yw;   // 06f4 mov
        r0.xy = (r4 * r0.xxxx + r4.zwzw).xy;   // 0700 mad
        r0.y = ((1.0 / r0.y).xxxx).y;   // 0714 rcp
        r3.x = (r0.yyyy * r0.xxxx).x;   // 0720 mul
        r0.x = (r0.xxxx * r0.yyyy + (-v1.zzzz)).x;   // 0730 mad
        r5 = float4(0.0, 0.0, 0.0, 1.0);   // 0744 texld
        r2 = float4(0.0, 0.0, 0.0, 1.0);   // 0754 texld
        r2.yzw = (r2.xxyz).yzw;   // 0764 mov
        r0.yz = (r4.xxyw * r5.xxxx + r4.xzww).yz;   // 0770 mad
        r0.z = ((1.0 / r0.z).xxxx).z;   // 0784 rcp
        r2.x = (r0.zzzz * r0.yyyy).x;   // 0790 mul
        r0 = RfxCmp(r0.xxxx, r3, r2);   // 07a0 cmp
        r0.x = (r0.xxxx + (-v1.zzzz)).x;   // 07b4 add
        r1.w = ((1.0 / v6.w).xxxx).w;   // 07c4 rcp
        r1.w = (saturate(r0.xxxx * r1.wwww)).w;   // 07d0 mul
        r2.x = (log2(abs(v6.x)).xxxx).x;   // 07e0 log
        r2.y = (log2(abs(v6.y)).xxxx).y;   // 07ec log
        r2.z = (log2(abs(v6.z)).xxxx).z;   // 07f8 log
        r2.xyz = (r0.xxxx * r2).xyz;   // 0804 mul
        r3.x = (saturate(exp2(r2.x).xxxx)).x;   // 0814 exp
        r3.y = (saturate(exp2(r2.y).xxxx)).y;   // 0820 exp
        r3.z = (saturate(exp2(r2.z).xxxx)).z;   // 082c exp
        r2.xyz = (r0.yzww * r3).xyz;   // 0838 mul
        r0.xyz = (r0.yzww * (-r3) + v8).xyz;   // 0848 mad
        r0.xyz = (r1.wwww * r0 + r2).xyz;   // 085c mad
        r2.xyz = (v7.wwww * (v7 - r0) + r0).xyz;   // 0870 lrp
        r0.x = (dot((v2).xyz, (r1).xyz).xxxx).x;   // 0884 dp3
        r0.y = (r0.xxxx + r0.xxxx).y;   // 0894 add
        r0.x = ((-r0.xxxx) + k0.wwww).x;   // 08a4 add
        r0.yzw = (r1.xxyz * (-r0.yyyy) + v2.xxyz).yzw;   // 08b4 mad
        r0.yzw = ((-r0)).yzw;   // 08c8 mov
        r1.x = (r0.zzzz + k0.wwww).x;   // 08d4 add
        r3 = RfxWaterWeather(SAMPLE_TEXTURECUBE(usr_sampler_cube_reflection, samplerusr_sampler_cube_reflection, (r0.yzww).xyz), (r0.yzww).xyz);   // 08e4 texld
        r0.y = (min(r1.xxxx, k0.wwww)).y;   // 08f4 min
        r0.z = (r0.xxxx * r0.xxxx).z;   // 0904 mul
        r0.z = (r0.zzzz * r0.zzzz).z;   // 0914 mul
        r0.x = (r0.zzzz * r0.xxxx).x;   // 0924 mul
        r0.x = (r0.xxxx * k1.xxxx + k1.yyyy).x;   // 0934 mad
        r0.x = (r0.yyyy * r0.xxxx).x;   // 0948 mul
        r0.yzw = (r3.xxyz + (-k0.wwww)).yzw;   // 0958 add
        r1.xyz = (max(r0.yzww, k1.zzzz)).xyz;   // 0968 max
        r0.yzw = ((-r1.xxyz) + r3.xxyz).yzw;   // 0978 add
        r3.xyz = (v9.wwww * (v9 - r0.yzww) + r0.yzww).xyz;   // 0988 lrp
        r0.yzw = (r1.xxyz * k1.wwww).yzw;   // 099c mul
        r4.xyz = (max(k0.wwww, r0.yzww)).xyz;   // 09ac max
        r0.yzw = (r1.xxyz * r4.xxyz + r3.xxyz).yzw;   // 09bc mad
        r1.xyz = (r0.xxxx * (r0.yzww - r2) + r2).xyz;   // 09d0 lrp
        r0.xyz = (v5.wwww * (v5 - r1) + r1).xyz;   // 09e4 lrp
        r0.w = (k2.wwww * v4.wwww).w;   // 09f8 mul
        oC0 = RfxCmp(-abs(v4.wwww), k1.zzzz, r0);   // 0a08 cmp
        return oC0;
    }
    #endif

            struct Attributes { float3 positionOS : POSITION; };
            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 t2 : TEXCOORD2; float4 t3 : TEXCOORD3; float4 t4 : TEXCOORD4;
                float4 t5 : TEXCOORD5; float4 t6 : TEXCOORD6; float4 t7 : TEXCOORD7; float4 t8 : TEXCOORD8; float4 t9 : TEXCOORD9;
            };

            // gWorld c64..c67: the water asset's matrix (rows = Unity's object-to-world rows). The camera matrices of the original are
            // rebuilt for the camera being rendered: gWorldView with the D3D view (z forward), gWorldViewProjection with Unity's view
            // projection (so the program's o0 is this camera's clip position and depth convention).
            void RfxWaterCamera()
            {
                float4x4 world = float4x4(gWorld[0], gWorld[1], gWorld[2], gWorld[3]);
                float4x4 v = UNITY_MATRIX_V; v[2] = -v[2];
                float4x4 wv = mul(v, world), wvp = mul(UNITY_MATRIX_VP, world);
                [unroll] for (int k = 0; k < 4; k++) { gWorldView[k] = wv[k]; gWorldViewProjection[k] = wvp[k]; }
            }
            // g_ProjectionInverse c196..c199 (the D3D projection's inverse, constant-table layout): rows 2 / 3 linearise a depth sample
            float4 RfxProjection()
            {
                float n = _ProjectionParams.y, f = _ProjectionParams.z;
                g_ProjectionInverse[0] = float4(1.0 / unity_CameraProjection._m00, 0, 0, 0);
                g_ProjectionInverse[1] = float4(0, 1.0 / abs(unity_CameraProjection._m11), 0, 0);
                g_ProjectionInverse[2] = float4(0, 0, 0, 1);
                g_ProjectionInverse[3] = float4(0, 0, -(1.0 / n - 1.0 / f), 1.0 / n);
                return 0;
            }

            Varyings WaterVert(Attributes a)
            {
                RfxWaterCamera();
                float4 o[11];
                float4 v0[4] = { float4(a.positionOS.xy, 0.0, 1.0), (float4)0, (float4)0, (float4)0 };   // the grid vertex: FLOAT2 -> (x, y, 0, 1)
            #if defined(_WATER_WATER)
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                RfxWater_vs_river_lod1(v0, o);
                    #else
                RfxWater_vs_river_lod0(v0, o);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                RfxWater_vs_lake_lod1(v0, o);
                    #else
                RfxWater_vs_lake_lod0(v0, o);
                    #endif
                #endif
            #elif defined(_WATER_OCEAN)
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                RfxWaterOcean_vs_river_lod1(v0, o);
                    #else
                RfxWaterOcean_vs_river_lod0(v0, o);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                RfxWaterOcean_vs_lake_lod1(v0, o);
                    #else
                RfxWaterOcean_vs_lake_lod0(v0, o);
                    #endif
                #endif
            #else
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                RfxWaterPool_vs_river_lod1(v0, o);
                    #else
                RfxWaterPool_vs_river_lod0(v0, o);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                RfxWaterPool_vs_lake_lod1(v0, o);
                    #else
                RfxWaterPool_vs_lake_lod0(v0, o);
                    #endif
                #endif
            #endif
                Varyings r;
                r.positionCS = o[0];
                r.t0 = o[1]; r.t1 = o[2]; r.t2 = o[3]; r.t3 = o[4]; r.t4 = o[5]; r.t5 = o[6]; r.t6 = o[7]; r.t7 = o[8]; r.t8 = o[9]; r.t9 = o[10];
                return r;
            }

            float _RfxWaterDebug;
            float4 WaterFrag(Varyings i) : SV_Target
            {
                RfxProjection();
                float4 v[10] = { i.t0, i.t1, i.t2, i.t3, i.t4, i.t5, i.t6, i.t7, i.t8, i.t9 };
                float4 c;
            #if defined(_WATER_WATER)
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                c = RfxWater_ps_river_lod1(v);
                    #else
                c = RfxWater_ps_river_lod0(v);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                c = RfxWater_ps_lake_lod1(v);
                    #else
                c = RfxWater_ps_lake_lod0(v);
                    #endif
                #endif
            #elif defined(_WATER_OCEAN)
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                c = RfxWaterOcean_ps_river_lod1(v);
                    #else
                c = RfxWaterOcean_ps_river_lod0(v);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                c = RfxWaterOcean_ps_lake_lod1(v);
                    #else
                c = RfxWaterOcean_ps_lake_lod0(v);
                    #endif
                #endif
            #else
                #if defined(_WATER_RIVER)
                    #if defined(_WATER_LOD1)
                c = RfxWaterPool_ps_river_lod1(v);
                    #else
                c = RfxWaterPool_ps_river_lod0(v);
                    #endif
                #else
                    #if defined(_WATER_LOD1)
                c = RfxWaterPool_ps_lake_lod1(v);
                    #else
                c = RfxWaterPool_ps_lake_lod0(v);
                    #endif
                #endif
            #endif
                if (_RfxWaterDebug > 0.5) { int k = (int)_RfxWaterDebug - 1; float4 dv = c; [unroll] for (int j = 0; j < 10; j++) if (j == k) dv = v[j]; return dv; }   // tests: an interpolant
            #if defined(RFX_WATER_TRACE)
                if (_RfxWaterTraceStep > 0.5) return gRfxTrace;
            #endif
                // alpha test GREATER ALPHAREF (0) on the 8-bit alpha, as the D3D9 device compares it
                if (floor(saturate(c.a) * 255.0 + 0.5) <= 0.0) discard;
                return float4(RfxOutput(c.rgb), c.a);
            }
            ENDHLSL
        }

        // Wake simulation (WakeSimRenderEvent 007e3e90 -> FUN_007ed300): one step per game frame into the oldest of the three
        // 512 x 512 R32F targets, a full-target quad (D3D9 half-pixel offset g_HalfPixelOffset: texel centres, as Unity rasterises).
        Pass
        {
            Name "WakeSim"
            Tags { "LightMode"="ReflexWaterWake" }   // drawn by OriginalWater's command buffer only (never by the camera's renderers)
            Cull Off ZWrite Off ZTest Always
            HLSLPROGRAM
            #pragma target 3.5
            #pragma vertex WakeVert
            #pragma fragment WakeFrag
            struct WakeV2F { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
            WakeV2F WakeVert(uint id : SV_VertexID)
            {
                WakeV2F o;
                float2 p = float2((id << 1) & 2, id & 2);          // full-screen triangle
                o.positionCS = float4(p * 2.0 - 1.0, 0.0, 1.0);
                o.uv = p;
            #if UNITY_UV_STARTS_AT_TOP
                o.uv.y = 1.0 - o.uv.y;
            #endif
                return o;
            }
            float4 WakeFrag(WakeV2F i) : SV_Target
            {
                float4 v[10] = { float4(i.uv, 0, 0), (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0 };
                return RfxWaterWake_ps_wake_sim(v);
            }
            ENDHLSL
        }

        // Pressure splats (the vehicles' contacts with the water, FUN_007ec280 / 007ecd80), drawn by FUN_007ed300 after the step into the
        // same target: a triangle list of (x, y, strength, u, v) vertices around each splat's centre
        Pass
        {
            Name "Pressure"
            Tags { "LightMode"="ReflexWaterPressure" }
            Cull Off ZWrite Off ZTest Always
            HLSLPROGRAM
            #pragma target 3.5
            #pragma vertex PressureVert
            #pragma fragment PressureFrag
            struct PressureIn { float4 position : POSITION; float2 uv : TEXCOORD0; };
            struct PressureV2F { float4 positionCS : SV_POSITION; float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 t2 : TEXCOORD2; };
            PressureV2F PressureVert(PressureIn a)
            {
                float4 o[11];
                float4 vin[4] = { a.position, float4(a.uv, 0, 1), (float4)0, (float4)0 };
                RfxWaterWake_vs_pressure(vin, o);
                PressureV2F r;
                // the original's clip position in the wake target; Unity's fullscreen convention (WakeVert) puts uv v = (1 - y) / 2 here
            #if UNITY_UV_STARTS_AT_TOP
                r.positionCS = float4(a.position.x, a.position.y, 0.0, 1.0);
            #else
                r.positionCS = float4(a.position.x, -a.position.y, 0.0, 1.0);
            #endif
                r.t0 = o[1]; r.t1 = o[2]; r.t2 = o[3];
                return r;
            }
            float4 PressureFrag(PressureV2F i) : SV_Target
            {
                float4 v[10] = { i.t0, i.t1, i.t2, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0, (float4)0 };
                float4 c = RfxWaterWake_ps_pressure(v);
                if (floor(saturate(c.a) * 255.0 + 0.5) == 0.0) discard;   // alpha test NOTEQUAL 0, no blending (FUN_007ed300)
                return c;
            }
            ENDHLSL
        }
    }
    Fallback Off
}

