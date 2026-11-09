// Port of the original MX vs ATV Reflex terrain pixel shader HFTerrain_4d_cl_s_xtra / ps_hft_lod0
// (material e.g. NAT_CO_HFTRN, FR_AL_HFTerrainMaterial) for a Unity Terrain (Terrain.materialTemplate).
// Instruction offsets in comments refer to _re/formats/out/unity_<level>/terrain_textures/shader/*.asm;
// the derivation and what is proven vs inferred is in _re/notes/terrain-shader.md.
//
// Original per-pixel math (normalized texture samples, sRGB sampling disabled in the original device):
//   uv      = (column/2047, row/2047) of the height grid (D3D v; Unity v = 1 - v)
//   N       = heightmap central-difference normal                       (0788..0924)
//   mask    = DetailMask(uv)                     rgb: red/green/blue detail weights, a: specular mask
//   detail  = lerp(lerp(lerp(Base(uv*t.x), Red(uv*t.y), mask.r), Green(uv*t.z), mask.g), Blue(uv*t.w), mask.b)
//             (each texldb with mip bias -0.67)                           (0cf4..0dcc)
//   cl      = ChromLum(uv);  adj = (detail.rgb + 0.25*cl.rgb) * sat(2*cl.a) + sat(cl.a - 0.5)   (0e78..0ebc)
//   albedo  = lerp(TopDown(uv), adj, detailFade),  detailFade = 1 - sat((|cam-P| - 384)/128)      (0ed0..0ef4)
//   sunVis  = sat(500*(height + 0.002 - shadowHeight)) * RT_Bump.w * radiosity(=1)              (0b34..0c14)
//   slm     = SphericalLightMap(0.5+0.5*N.x, 0.5-0.5*N.z)  rgb ambient, a = N.sun               (0f14)
//   srm     = SphericalReflectionMap(reflect(V,N).xz)      rgb reflection, a = sat(N.sun)^8     (0b24)
//   spec    = (srm.rgb + srm.a*sunVis*sunColor) * mask.a * detail.a * bumpFade * (1 - RT_Bump.z)
//   colour  = albedo * (slm.rgb + slm.a*sunVis*sunColor) + spec                                (0f24..0f38)
//   out     = colour * (1 - fog) + fog * fogColour (per-vertex distance fog)                   (0f4c)
//
// Variants (the material's Shaders asset; ReflexTerrainMaterial.cs enables the keyword; disassembled with _re/formats/d3d9_shader.py):
//   HFTerrain_4d_cl_s_xtra (no keyword; 46 levels): the math above.
//   _HFT_4D_CL_S  HFTerrain_4d_cl_s (TRX_Lynchburg, TRX_LynchburgBW, UI_Compound; ps 172 / vs 141 lines):
//                 detail weights = mask.rgb (no deformation mask scale), the rut bump scale is the constant 0.25 (no
//                 usr_bump_scalar) and not multiplied by N.y, the chroma/lum adjustment comes before the dirt tint
//                 (tinted = lerp(tint, adj(detail), t.x)), and the top-down blend uses the bump fade v2.w (vs: 1 - 10 max(V - 0.9, 0)
//                 instead of 1 - 1.25 max(V^2 - 0.2, 0)) instead of the 384..512 distance fade (v5.w is not written).
//   _HFT_SX_FY    HFTerrain_sx_fy (SX_DLC01_FreightYard, SX_DLC02_FreightYardB; vs = xtra's): RT_Bump.w is faded with the bump fade
//                 (lerp(1, RT.w, v2.w), 1b30..1b64), and the detail is lerp(red layer, mask chain(mask.rgb), deformation mask scale)
//                 (1cd4..1d30: deformed ground shows the red layer instead of the base layer).
//   _HFT_FMX_FF   HFTerrain_fmx_ff (FMX_DLC03_FunFactory, FMX_DLC04_FunFactoryB; vs = xtra's): sunVis = max(0.5, shadow * RT.w) (no
//                 radiosity in it), the radiosity sample replaces the sun colour: spec = srm.rgb + srm.a sunVis rad,
//                 light = slm.rgb + N.y * sunVis * rad * (1 + sunColor) (0b20..0c08, 0ec8..0f0c; slm.a is not used).
//   Radiosity (s13 via gColorRefProjection) is the level's colour reference map; the port models its 1x1 texel (_ColorRef):
//   xtra / cl_s / sx_fy use floor(a) ? rgb : 1 (Editor_Default_White -> 1), fmx_ff uses rgb.
//   HFTerrain_Basic (SX_Fresno, SX_FresnoB: RESTORED levels): the xtra math with the detail fade held at 0 and no detail mask (ReflexTerrainMaterial: albedo = top-down map, no specular).
Shader "Reflex/Original HFTerrain"
{
    Properties
    {
        [Header(Material textures (slot order of the original material))]
        [NoScaleOffset] _TopDownMap("s0 Top-down world map", 2D) = "gray" {}
        [NoScaleOffset] _DetailBase("s1 Detail base", 2D) = "gray" {}
        [NoScaleOffset] _DetailRed("s2 Detail red", 2D) = "gray" {}
        [NoScaleOffset] _DetailGreen("s3 Detail green", 2D) = "gray" {}
        [NoScaleOffset] _DetailBlue("s4 Detail blue", 2D) = "gray" {}
        [NoScaleOffset] _DetailMask("s5 Detail mask (rgb weights, a specular)", 2D) = "black" {}
        [NoScaleOffset] _ChromLumMap("s6 Chrominance/luminance adjustment", 2D) = "gray" {}
        [NoScaleOffset] _SphericalLightMap("s7 Spherical light map", 2D) = "gray" {}
        [NoScaleOffset] _SphericalReflectionMap("s8 Spherical reflection map", 2D) = "black" {}
        [NoScaleOffset] _ShadowVisMap("Engine s12 shadow visibility (derived)", 2D) = "white" {}

        [Header(Material parameters)]
        _AliveTerrain("Alive terrain (virtual-texture HFTerrain_vtex programs)", Float) = 0
        [NoScaleOffset] _AliveVT0("Alive decal ring 0 (AliveTerrainVT)", 2D) = "gray" {}
        [NoScaleOffset] _AliveVT1("Alive decal ring 1", 2D) = "gray" {}
        [NoScaleOffset] _AliveVT2("Alive decal ring 2", 2D) = "gray" {}
        [NoScaleOffset] _AliveVT3("Alive decal ring 3", 2D) = "gray" {}
        _AliveVTRect0("Ring 0 (x0, z0, size, valid)", Vector) = (0, 0, 1, 0)
        _AliveVTRect1("Ring 1", Vector) = (0, 0, 1, 0)
        _AliveVTRect2("Ring 2", Vector) = (0, 0, 1, 0)
        _AliveVTRect3("Ring 3", Vector) = (0, 0, 1, 0)
        _AliveLook("ALIVE LOOK REFLEX: layer from top-down, top-down chroma adj, colour match, pick sharpness", Vector) = (0, 0, 0, 40)
        _AliveLook2("ALIVE LOOK REFLEX: x top-down recoloured in the layers' hue, y specular scale, z layers' mean luminance, w debug", Vector) = (0, 0, 0, 0)
        _AliveTopDownMean("ALIVE LOOK REFLEX: the baked composite's mean colour (a: valid)", Vector) = (0, 0, 0, 0)
        _AliveLook3("ALIVE LOOK REFLEX: red clay x strength, y..z excess redness ramp", Vector) = (0, 0.14, 0.22, 0)
        _AliveLayerClass("ALIVE LOOK REFLEX: layer kinds (0 dirt, 1 grass, 2 snow, 3 rock)", Vector) = (0, 0, 0, 0)
        _AliveLayerClassPresent("ALIVE LOOK REFLEX: kinds present (dirt, grass, snow, rock)", Vector) = (1, 0, 0, 0)
        _AliveLayerMean0("ALIVE LOOK: mean colour of detail base (a: mean alpha)", Vector) = (0.5, 0.5, 0.5, 1)
        _AliveLayerMean1("ALIVE LOOK: mean colour of detail red", Vector) = (0.5, 0.5, 0.5, 1)
        _AliveLayerMean2("ALIVE LOOK: mean colour of detail green", Vector) = (0.5, 0.5, 0.5, 1)
        _AliveLayerMean3("ALIVE LOOK: mean colour of detail blue", Vector) = (0.5, 0.5, 0.5, 1)
        _DetailTile("usr_detail_tile (base, red, green, blue)", Vector) = (32, 32, 32, 32)
        // raw constants: Vector, not Color (a Color property is converted gamma -> linear in the linear project: fog 0.612 became 0.332)
        _DirtTint("usr_dirt_tint_color", Vector) = (0.4, 0.3, 0.05, 1)
        _BumpScalar("usr_bump_scalar", Vector) = (0.25, 0.25, 0.25, 0.25)

        [Header(Grid and scene lighting)]
        _GridOrigin("Grid origin (x of column 0, z of row 0, 2047*spacing)", Vector) = (-2047, 2047, 4094, 0)
        _SunDirection("gSunLight (towards sun)", Vector) = (0, 1, 0, 0)
        _SunColor("gSunLightColor", Vector) = (1, 1, 1, 0)
        _DetailFade("Detail fade (start, range)", Vector) = (384, 128, 0, 0)
        _DetailParams("Detail mip bias, chroma scale", Vector) = (-0.67, 0.25, 0, 0)
        _MtxSourceColour("MTX authored colour", Float) = 0
        _MtxDetail("MTX coarse breakup, fine grain, fade distance, grain normalization", Vector) = (0, 0, 240, 0)
        _RTBumpNeutral("RT_Bump neutral sample", Vector) = (0.5, 1, 0, 1)
        _DeformNeutral("Deformation neutral sample", Vector) = (0.50196081, 0, 0, 0)
        [NoScaleOffset] _DeformMap("Engine s10 live deformation surface (set at runtime)", 2D) = "gray" {}
        [ToggleUI] _UseDeformMap("Use live deformation surface", Float) = 0
        [NoScaleOffset] _TrackMap("Live tyre-track multiplier (RT_Bump track decal pass, set at runtime)", 2D) = "white" {}
        [ToggleUI] _UseTrackMap("Use live tyre tracks", Float) = 0
        _TrackMapRect("Track map centre x/z, half size x/z", Vector) = (0, 0, 1024, 1024)
        _TrackMapTexel("Track map 1/width, 1/height", Vector) = (0.000244140625, 0.000244140625, 0, 0)
        _BumpFade("Bump/specular fade approximation (start, end, dy start, dy range)", Vector) = (256, 512, 128, 128)
        _FogColorStart("Fog colour start", Vector) = (0.6, 0.6, 0.6, 0)
        _FogColorEnd("Fog colour end", Vector) = (0.6, 0.6, 0.6, 0)
        _FogParams("Fog (distance start, end, amount start, end)", Vector) = (0, 1000, 0, 0)
        _ColorRef("Colour reference (radiosity) texel, s13", Vector) = (1, 1, 1, 1)
        [ToggleUI] _FogEnabled("Original distance fog", Float) = 1
        [ToggleUI] _UseUnityShadows("Multiply Unity main-light shadows (dynamic objects)", Float) = 1
        [ToggleUI] _UseShadowVis("Original static terrain shadow map", Float) = 1
        [ToggleUI] _GammaOutput("Original values are display values (convert in linear projects)", Float) = 1
        [NoScaleOffset] _HeightMap("Engine s9 height map (R32F, D3D rows)", 2D) = "black" {}
        _GridParams("Grid samples N, rebuild normal from _HeightMap, g_NormalStrength.x", Vector) = (0, 0, 2, 0)
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Geometry-100" "TerrainCompatible"="True" "IgnoreProjector"="True" }

        HLSLINCLUDE
        #include "ReflexTerrainInput.hlsl"
        ENDHLSL

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            ZWrite On
            ZTest LEqual
            Cull Back

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile_fragment _ _SHADOWS_SOFT _SHADOWS_SOFT_LOW _SHADOWS_SOFT_MEDIUM _SHADOWS_SOFT_HIGH
            #pragma multi_compile_instancing
            #pragma instancing_options assumeuniformscaling nomatrices nolightprobe nolightmap
            #pragma multi_compile_local_fragment _ _HFT_4D_CL_S _HFT_SX_FY _HFT_FMX_FF
            #pragma multi_compile_fragment _ _RFX_ENHANCED
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            TEXTURE2D(_ChromLumMap);            SAMPLER(sampler_ChromLumMap);
            #define RFX_ALIVE_SAMPLER sampler_ChromLumMap   // (the bounce light volume: no sampler register left)
            #include "ReflexCommon.hlsl"

            TEXTURE2D(_TopDownMap);             SAMPLER(sampler_TopDownMap);
            TEXTURE2D(_DetailBase);             SAMPLER(sampler_DetailBase);
            TEXTURE2D(_DetailRed);              SAMPLER(sampler_DetailRed);
            TEXTURE2D(_DetailGreen);            SAMPLER(sampler_DetailGreen);
            TEXTURE2D(_DetailBlue);             SAMPLER(sampler_DetailBlue);
            TEXTURE2D(_DetailMask);             SAMPLER(sampler_DetailMask);
            TEXTURE2D(_SphericalLightMap);      SAMPLER(sampler_SphericalLightMap);
            TEXTURE2D(_SphericalReflectionMap); SAMPLER(sampler_SphericalReflectionMap);
            TEXTURE2D(_ShadowVisMap);           SAMPLER(sampler_ShadowVisMap);
            TEXTURE2D(_AliveVT0); TEXTURE2D(_AliveVT1); TEXTURE2D(_AliveVT2); TEXTURE2D(_AliveVT3);   // AliveTerrainVT rings (sampler: ChromLum's)
            #define RFX_ENH_OCC_SAMPLER sampler_TopDownMap   // ENHANCED: the terrain already uses all 16 sampler registers
            #define RFX_ENH_SKY_SAMPLER sampler_TopDownMap   // (WEATHER: the sky cube in the wet ground and puddles, the same)
            #include "ReflexEnhanced.hlsl"
            #include "ReflexShadows.hlsl"
            #include "ReflexWeather.hlsl"   // WEATHER (ENHANCED only, behind a uniform branch: Scripts/Rendering/ReflexWeather.cs)

            struct Attributes
            {
                float4 positionOS : POSITION;
                float3 normalOS   : NORMAL;
                float2 texcoord   : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 positionWS : TEXCOORD0;
                float3 normalWS   : TEXCOORD1;
                float2 terrainUV  : TEXCOORD2;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            Varyings Vert(Attributes v)
            {
                Varyings o = (Varyings)0;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
                ReflexTerrainInstancing(v.positionOS, v.normalOS, v.texcoord);
                o.positionWS = RfxRTBumpDisplace(TransformObjectToWorld(v.positionOS.xyz));
                o.positionCS = TransformWorldToHClip(o.positionWS);
                o.normalWS = TransformObjectToWorldNormal(v.normalOS);
                o.terrainUV = v.texcoord;
                return o;
            }

            // D3D texture coordinate (v down) -> Unity (v up; PNG row 0 is the D3D top row).
            float2 FlipV(float2 d3d) { return float2(d3d.x, 1.0 - d3d.y); }

        #if defined(_RFX_ENHANCED)
            // WEATHER: the tyre tracks around P (the live track map: 1 = untouched): the darkness at P, over a few decimetres, and its gradient (per m)
            void RfxWTrackTaps(float3 P, out float trk, out float wide, out float2 grad)
            {
                trk = 0.0; wide = 0.0; grad = 0;
                if (_UseTrackMap < 0.5) return;
                float2 tuv = (P.xz - _TrackMapRect.xy) / _TrackMapRect.zw * 0.5 + 0.5;
                const float3 L3 = float3(0.3333, 0.3333, 0.3334);
                trk = 1.0 - dot(SAMPLE_TEXTURE2D_LOD(_TrackMap, sampler_TrackMap, tuv, 0).rgb, L3);
                float2 du = float2(_TrackMapTexel.x, 0) * 2.5, dv = float2(0, _TrackMapTexel.y) * 2.5;
                float a = 1.0 - dot(SAMPLE_TEXTURE2D_LOD(_TrackMap, sampler_TrackMap, tuv + du, 0).rgb, L3);
                float b = 1.0 - dot(SAMPLE_TEXTURE2D_LOD(_TrackMap, sampler_TrackMap, tuv - du, 0).rgb, L3);
                float c = 1.0 - dot(SAMPLE_TEXTURE2D_LOD(_TrackMap, sampler_TrackMap, tuv + dv, 0).rgb, L3);
                float e = 1.0 - dot(SAMPLE_TEXTURE2D_LOD(_TrackMap, sampler_TrackMap, tuv - dv, 0).rgb, L3);
                wide = saturate(0.25 * (a + b + c + e) * 1.3);
                float tw = 2.0 * 2.5 * 2.0 * _TrackMapRect.z * _TrackMapTexel.x;     // m between the wide taps
                grad = float2(a - b, c - e) / max(tw, 1e-3);
            }
            // WEATHER: the height field's concavity around the D3D uv (m; > 0 in a hollow)
            float RfxWBasin(float2 uv)
            {
                if (_GridParams.y < 0.5) return 0.0;
                // (bilinear: nearest texels would show the grid as hard-edged squares of water)
                float t3 = 3.0 / _GridParams.x;
                return (0.25 * (RfxHeightAt(uv + float2(t3, 0)) + RfxHeightAt(uv - float2(t3, 0)) + RfxHeightAt(uv + float2(0, t3)) + RfxHeightAt(uv - float2(0, t3))) - RfxHeightAt(uv)) * 2048.0;
            }
        #endif

            half4 Frag(Varyings i) : SV_Target
            {
                float3 P = i.positionWS;
                // the terrain normal map only when the height map does not rebuild the normal (_GridParams.y: both paths below replace N)
                float3 N = float3(0, 1, 0);
                [branch] if (_GridParams.y < 0.5) N = ReflexTerrainNormalWS(i.terrainUV, i.normalWS);
                float3 toCam = _WorldSpaceCameraPos - P;
                float dist = length(toCam);
                float3 dirToCam = toCam / max(dist, 1e-5);
                float3 V = -dirToCam;                                   // o3.xyz: camera -> point

                // Height-grid UV (vs 0a88..0b00): u = 0.5 + 0.5 x / c201.z = (column + 0.5) / N, v = (row + 0.5) / N (D3D v down),
                // i.e. texel centres at the grid samples (recorded: FMX_BuenaVista f1382 v0 = (0.637347, 0.322368) at x 140.643, z 181.895,
                // N 1024). Without N (old materials): column / (N - 1).
                float2 uv = float2(P.x - _GridOrigin.x, _GridOrigin.y - P.z) / _GridOrigin.z;
                if (_GridParams.x > 1.5) uv = (uv * (_GridParams.x - 1.0) + 0.5) / _GridParams.x;
                float2 uvU = FlipV(uv);
                // MX vs ATV Alive (HFTerrain_vtex / _xtraRelief ps_hft_lod0, disassembled with _re/formats/xenos_disasm.py):
                //   vt     = the terrain virtual texture: the HFDecalModel layers composited in SortBias order (AliveTerrainVT rings; before the
                //            rings exist, the base layer's own four-texture splat)
                //   adj    = (vt + 0.25 cl.rgb) sat(2 cl.a) + sat(cl.a - 0.5)   (cl: the chrominance / luminance map; 0b24..0b2c)
                //   albedo = adj^2, colour = albedo x light buffer (sun N.L x shadow + RfxAliveAmbient: fill lights when enabled, bounce volume)
                //   fog    = lerp(colour, fog colour, amount) (c14 / c15 / c26 as the Reflex terrain); x gHDRScale by the post chain
                if (_AliveTerrain > 0.5)
                {
                    float3 vt = 0; float vtW = 0;
                    float2 dx = ddx(P.xz), dy = ddy(P.xz);
                    // finest ring holding the point, blended into the next one over its outer tenth
                    [unroll] for (int k = 3; k >= 0; k--)
                    {
                        float4 rect = k == 0 ? _AliveVTRect0 : k == 1 ? _AliveVTRect1 : k == 2 ? _AliveVTRect2 : _AliveVTRect3;
                        if (rect.w < 0.5) continue;
                        float2 ruv = (P.xz - rect.xy) / rect.z;
                        float edge = max(abs(ruv.x - 0.5), abs(ruv.y - 0.5)) * 2.0;
                        if (edge >= 1.0) continue;
                        float3 c = k == 0 ? SAMPLE_TEXTURE2D_GRAD(_AliveVT0, sampler_ChromLumMap, ruv, dx / rect.z, dy / rect.z).rgb
                                 : k == 1 ? SAMPLE_TEXTURE2D_GRAD(_AliveVT1, sampler_ChromLumMap, ruv, dx / rect.z, dy / rect.z).rgb
                                 : k == 2 ? SAMPLE_TEXTURE2D_GRAD(_AliveVT2, sampler_ChromLumMap, ruv, dx / rect.z, dy / rect.z).rgb
                                 :          SAMPLE_TEXTURE2D_GRAD(_AliveVT3, sampler_ChromLumMap, ruv, dx / rect.z, dy / rect.z).rgb;
                        float w = k == 3 || vtW == 0 ? 1.0 : 1.0 - saturate((edge - 0.9) / 0.1);
                        vt = lerp(vt, c, w); vtW = max(vtW, w);
                    }
                    if (vtW == 0)
                    {
                        float4 mask = SAMPLE_TEXTURE2D(_DetailMask, sampler_DetailMask, uvU);
                        vt = SAMPLE_TEXTURE2D(_DetailBase, sampler_DetailBase, uvU * _DetailTile.x).rgb;
                        vt = lerp(vt, SAMPLE_TEXTURE2D(_DetailRed, sampler_DetailRed, uvU * _DetailTile.y).rgb, mask.r);
                        vt = lerp(vt, SAMPLE_TEXTURE2D(_DetailGreen, sampler_DetailGreen, uvU * _DetailTile.z).rgb, mask.g);
                        vt = lerp(vt, SAMPLE_TEXTURE2D(_DetailBlue, sampler_DetailBlue, uvU * _DetailTile.w).rgb, mask.b);
                    }
                    // tyre tracks: Alive draws the ribbons into the virtual texture with HFT_Track_vtex (the track material's diffuse tread
                    // / torn textures, alpha = softness^(1/4) x tread alpha); here the live track pass (OriginalTracks, the same ribbons
                    // and softness rule with the tread texture) darkens the composite: vt x lerp(1, tread, a)
                    if (_UseTrackMap > 0.5)
                    {
                        float2 tuv = (P.xz - _TrackMapRect.xy) / _TrackMapRect.zw * 0.5 + 0.5;
                        vt *= SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv).rgb;
                    }
                    float4 cl = SAMPLE_TEXTURE2D(_ChromLumMap, sampler_ChromLumMap, uvU);
                    float3 adj = (vt + 0.25 * cl.rgb) * saturate(2.0 * cl.a) + saturate(cl.a - 0.5);
                    float3 albedo = adj * adj;
                    // the normal pass (normal_ps_hft_lod0) rebuilds N from the height map as the Reflex terrain does
                    float3 Na = N;
                    if (_GridParams.y > 0.5)
                    {
                        float t = 1.0 / _GridParams.x;
                        float4 H = float4(RfxHeightAt(uv + float2(t, 0)), RfxHeightAt(uv - float2(t, 0)), RfxHeightAt(uv + float2(0, t)), RfxHeightAt(uv - float2(0, t))) * 6144.0;
                        Na = normalize(float3(-(H.x - H.y), _GridParams.z, H.z - H.w));
                    }
                    float visibility = (_UseShadowVis > 0.5 ? SAMPLE_TEXTURE2D(_ShadowVisMap, sampler_ShadowVisMap, uvU).r : 1.0) * RfxMapShadow(P);
                    if (_UseUnityShadows > 0.5)
                    {
                    #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                        float4 sc = ComputeScreenPos(TransformWorldToHClip(P));
                    #else
                        float4 sc = TransformWorldToShadowCoord(P);
                    #endif
                        visibility *= RfxMainLightShadow(sc);
                    }
                    float3 light = RfxAliveAmbient(Na, P) + saturate(dot(Na, _RfxSunDir.xyz)) * visibility * _RfxSunColor.rgb;
                    float3 colour = albedo * light;
                #if defined(_RFX_ENHANCED)
                    [branch] if (RfxWeatherOn())
                    {
                        // WEATHER on the Alive terrain (no deformation surface there: the trails are the tyre tracks)
                        float trk = 0.0, trkWide = 0.0; float2 trkGrad = 0;
                        if (dist < 260.0) RfxWTrackTaps(P, trk, trkWide, trkGrad);
                        float basin = dist < 220.0 ? RfxWBasin(uv) : 0.0;
                        float3 Vw = -dirToCam;
                        RfxWeatherGround aw = RfxWeatherGroundPrepare(P, Na, Na, 0.0, 0.0, basin, trk, trkWide, trkGrad, 0, Vw, dist);
                        float3 wn = float3(0, 1, 0);
                        if (aw.puddle > 0.001 && dist < 70.0)
                        {
                            float2 rg = RfxWeatherRipples(P.xz, _RfxWeather2.w, _RfxWeather2.y) * saturate(_RfxWeather2.y * 4.0);
                            wn = normalize(float3(-rg.x * 0.35, 1.0, -rg.y * 0.35));
                        }
                        float3 sunW = saturate(dot(aw.N, _RfxSunDir.xyz)) * visibility * _RfxSunColor.rgb;
                        float3 lightW = RfxAliveAmbient(aw.N, P) + sunW;
                        colour = RfxWeatherGroundShade(aw, albedo, lightW, 0, sunW, Vw, RfxWeatherReflection(reflect(Vw, aw.N), 0.55), RfxWeatherReflection(reflect(Vw, wn), 0.0), wn);
                    }
                #endif
                    float fog = saturate((dist - _FogParams.x) / max(1, _FogParams.y - _FogParams.x));
                    colour = lerp(colour, lerp(_FogColorStart.rgb, _FogColorEnd.rgb, fog), lerp(_FogParams.z, _FogParams.w, fog));
                    return half4(_GammaOutput < 0.5 ? max(colour, 0.0) : RfxOutput(colour), 1);
                }
                // rut slope of the deformation surface (086c, 08a4..08d0): 1 - max(sat|20 (d(u+) - d(u-))|, sat|20 (d(v+) - d(v-))|) / 2
                float rutFlat = 1.0;
                float4 rfxD4 = _DeformNeutral.xxxx;   // (WEATHER: the four deformation taps, the ruts' shape)
                if (_GridParams.y > 0.5)
                {
                    // Normal (0788..0924): central differences of H = height * g_TerrainStrength.y (6144) + deform * g_TerrainStrength.w (20)
                    // at +-1 texel (g_HFMapSize.z), N = normalize(-(H(u+) - H(u-)), g_NormalStrength.x, H(v+) - H(v-)). g_TerrainStrength =
                    // (2048, 6144, 4, 20) on every recorded level: the normal sees the heights three times steeper than the geometry (2048).
                    float t = 1.0 / _GridParams.x;
                    float2 up = uv + float2(t, 0), um = uv - float2(t, 0), vp = uv + float2(0, t), vm = uv - float2(0, t);
                    float4 d4 = _DeformNeutral.xxxx;
                    if (_UseDeformMap > 0.5)
                        d4 = float4(SAMPLE_TEXTURE2D(_DeformMap, sampler_DeformMap, FlipV(up)).r, SAMPLE_TEXTURE2D(_DeformMap, sampler_DeformMap, FlipV(um)).r,
                                    SAMPLE_TEXTURE2D(_DeformMap, sampler_DeformMap, FlipV(vp)).r, SAMPLE_TEXTURE2D(_DeformMap, sampler_DeformMap, FlipV(vm)).r);
                    float4 H = float4(RfxHeightAt(up), RfxHeightAt(um), RfxHeightAt(vp), RfxHeightAt(vm)) * 6144.0 + d4 * 20.0;
                    float2 dd = abs(float2(d4.x * 20.0 - d4.y * 20.0, d4.z * 20.0 - d4.w * 20.0));
                    rutFlat = 1.0 - max(saturate(dd.x) * 0.5, saturate(dd.y) * 0.5);
                    rfxD4 = d4;
                    N = normalize(float3(-(H.x - H.y), _GridParams.z, H.z - H.w));   // +u = +x, +v (D3D) = -z
                }

                float3 rfxNgeo = N;   // (WEATHER: the heightfield's normal before the bump)
                // Fades interpolated from the original vertex shader.
                float detailFade = 1.0 - saturate((dist - _DetailFade.x) / _DetailFade.y);     // o6.w
                float dy = abs(toCam.y);
                float horiz = length(toCam.xz);
                bool rtValid = _RfxRTBumpParams.x > 0.5;
                float2 ruv = rtValid ? RfxRTBumpUV(P) : float2(0, 0);
                // o3.w (rut/specular fade) from the RT_Bump cascade coordinate V; o5.w = (1 - V)^3
                float Vrt = ruv.y;
                if (!rtValid)
                {
                    // without RT_Bump (no ReflexRTBump on this camera): V from a distance stand-in
                    float fadeS = saturate((horiz - _BumpFade.x) / max(_BumpFade.y - _BumpFade.x, 1e-3));
                    Vrt = sqrt(0.2 + 0.8 * fadeS);                       // 1 - 1.25 max(V^2 - 0.2, 0) = 1 - fadeS
                }
            #if defined(_HFT_4D_CL_S)
                float vFade = 1.0 - 10.0 * max(Vrt - 0.9, 0.0);         // vs_hft_lod0 of _4d_cl_s
            #else
                float vFade = 1.0 - 1.25 * max(Vrt * Vrt - 0.2, 0.0);
            #endif
                float bumpFade = rtValid ? vFade * (1.0 - saturate((toCam.y - 128.0) * 0.0078125))
                                         : (1.0 - saturate((dy - _BumpFade.z) / _BumpFade.w)) * vFade;
                float v4w = rtValid ? (1.0 - Vrt) * (1.0 - Vrt) * (1.0 - Vrt) : pow(1.0 - saturate(horiz / max(_BumpFade.y, 1e-3)), 3);

                // Engine inputs: RT_Bump (bump models, tyre tracks; .w dynamic shadow = 1) and the deformation surface (s10).
                float4 rt = _RTBumpNeutral;
                if (rtValid)
                {
                    // 06c8..0778: four taps at -+ g_UVOneStep, H = x + 2 z; r0.x = H(-s_x) - H(+s_x), r0.y = H(-s_z) - H(+s_z)
                    rt = SAMPLE_TEXTURE2D_LOD(_RfxRTBump, sampler_RfxRTBump, ruv, 0);
                    float4 tx0 = SAMPLE_TEXTURE2D_LOD(_RfxRTBump, sampler_RfxRTBump, ruv - _RfxRTBumpStep.xy, 0);
                    float4 tz0 = SAMPLE_TEXTURE2D_LOD(_RfxRTBump, sampler_RfxRTBump, ruv - _RfxRTBumpStep.zw, 0);
                    float4 tz1 = SAMPLE_TEXTURE2D_LOD(_RfxRTBump, sampler_RfxRTBump, ruv + _RfxRTBumpStep.zw, 0);
                    float4 tx1 = SAMPLE_TEXTURE2D_LOD(_RfxRTBump, sampler_RfxRTBump, ruv + _RfxRTBumpStep.xy, 0);
                    if (_UseTrackMap > 0.5 && _RfxRTBumpParams.y < 0.5)
                    {
                        // tracks not in RT_Bump: the world-space track multiplier at the centre only
                        float2 tuv = (P.xz - _TrackMapRect.xy) / _TrackMapRect.zw * 0.5 + 0.5;
                        rt.xyz *= SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv).rgb;
                    }
                    float gx = (tx0.x + 2.0 * tx0.z) - (tx1.x + 2.0 * tx1.z);
                    float gz = (tz0.x + 2.0 * tz0.z) - (tz1.x + 2.0 * tz1.z);
                    if (_RfxRTBumpNearOn > 0.5)
                    {
                        // OPEN REFLEX: the same taps from the world-locked near target (ReflexRTBump.cs: the slices' resampling made the
                        // bump shading crawl as the camera turned), faded to RT_Bump over the square's edge band
                        float2 nuv = (P.xz - _RfxRTBumpNearRect.xy) * _RfxRTBumpNearRect.z;
                        float edge = min(min(nuv.x, nuv.y), min(1.0 - nuv.x, 1.0 - nuv.y)) / (_RfxRTBumpNearRect.z * _RfxRTBumpNearRect.w);
                        float wn = saturate(edge);
                        if (wn > 0.0)
                        {
                            float tl = 2.0 * dot(_RfxRTBumpW.xyz, float3(P.x, P.z, 1.0)) / 768.0;
                            float d = tl * _RfxRTBumpNearRect.z;
                            float4 n0 = SAMPLE_TEXTURE2D_LOD(_RfxRTBumpNear, sampler_RfxRTBump, nuv + float2(d, 0), 0);
                            float4 n1 = SAMPLE_TEXTURE2D_LOD(_RfxRTBumpNear, sampler_RfxRTBump, nuv - float2(d, 0), 0);
                            float4 m0 = SAMPLE_TEXTURE2D_LOD(_RfxRTBumpNear, sampler_RfxRTBump, nuv - float2(0, d), 0);
                            float4 m1 = SAMPLE_TEXTURE2D_LOD(_RfxRTBumpNear, sampler_RfxRTBump, nuv + float2(0, d), 0);
                            float ngx = (n0.x + 2.0 * n0.z) - (n1.x + 2.0 * n1.z);
                            float ngz = (m0.x + 2.0 * m0.z) - (m1.x + 2.0 * m1.z);
                            gx = lerp(gx, ngx, wn); gz = lerp(gz, ngz, wn);
                        }
                    }
                    // 0934..0ab0: k = N.y max((1 - V)^3, dot(toCam, N)) fade; b = normalize(-k r0.x, usr_bump_scalar, k r0.y);
                    // N' = normalize(b.y N + b.x T1 + b.z T2), T1 = norm(N.y, -N.x, 0), T2 = norm(0, -N.z, N.y)
                #if defined(_HFT_4D_CL_S)
                    float k = max(v4w, dot(dirToCam, N)) * bumpFade;
                    float3 b = normalize(float3(-k * gx, 0.25, k * gz));
                #else
                    float k = N.y * max(v4w, dot(dirToCam, N)) * bumpFade;
                    float3 b = normalize(float3(-k * gx, _BumpScalar.x, k * gz));
                #endif
                    float3 T1 = float3(N.y, -N.x, 0) * rsqrt(max(N.x * N.x + N.y * N.y, 1e-8));
                    float3 T2 = float3(0, -N.z, N.y) * rsqrt(max(N.z * N.z + N.y * N.y, 1e-8));
                    N = normalize(N * b.y + T1 * b.x + T2 * b.z);
                }
                else if (_UseTrackMap > 0.5)
                {
                    // RT_Bump after the track decal pass: the clear value times the multiplied ribbons.
                    float2 tuv = (P.xz - _TrackMapRect.xy) / _TrackMapRect.zw * 0.5 + 0.5;
                    float3 tm = SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv).rgb;
                    rt.xyz *= tm;
                    // Rut bump normal (0934..0ab0): H = x + 2z at +-1 texel, b = normalize(-k gx, usr_bump_scalar, k gz),
                    // k = N.y * max((1 - V)^3, dot(toCam, N)) * fade, folded in with T1 = norm(N.y, -N.x, 0), T2 = norm(0, -N.z, N.y).
                    // Gradient signs chosen so raised H shades as raised (cascade axis orientation is not recovered).
                    float2 du = float2(_TrackMapTexel.x, 0), dv = float2(0, _TrackMapTexel.y);
                    float3 mxp = SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv + du).rgb, mxn = SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv - du).rgb;
                    float3 mzp = SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv + dv).rgb, mzn = SAMPLE_TEXTURE2D(_TrackMap, sampler_TrackMap, tuv - dv).rgb;
                    float4 n0 = _RTBumpNeutral;
                    float gx = (n0.x * mxp.x + 2 * n0.z * mxp.z) - (n0.x * mxn.x + 2 * n0.z * mxn.z);
                    float gz = (n0.x * mzn.x + 2 * n0.z * mzn.z) - (n0.x * mzp.x + 2 * n0.z * mzp.z);
                #if defined(_HFT_4D_CL_S)
                    float k = max(v4w, dot(dirToCam, N)) * bumpFade;          // no N.y factor; bump scale the constant 0.25 (c1.z)
                    float3 b = normalize(float3(-k * gx, 0.25, k * gz));
                #else
                    float k = N.y * max(v4w, dot(dirToCam, N)) * bumpFade;
                    float3 b = normalize(float3(-k * gx, _BumpScalar.x, k * gz));
                #endif
                    float3 T1 = float3(N.y, -N.x, 0) * rsqrt(max(N.x * N.x + N.y * N.y, 1e-8));
                    float3 T2 = float3(0, -N.z, N.y) * rsqrt(max(N.z * N.z + N.y * N.y, 1e-8));
                    N = normalize(N * b.y + T1 * b.x + T2 * b.z);
                }
                float deform = _UseDeformMap > 0.5 ? SAMPLE_TEXTURE2D(_DeformMap, sampler_DeformMap, uvU).r : _DeformNeutral.x;
            #if defined(_RFX_ENHANCED)
                // WEATHER (not the original program; Scripts/Rendering/ReflexWeather.cs): wet ground and puddles in the ruts, snow cover with
                // trails where the riders went (the live deformation and tyre tracks of every rider). The normal changes before the light maps.
                RfxWeatherGround rfxW = (RfxWeatherGround)0;
                float3 rfxWaterN = float3(0, 1, 0);
                [branch] if (RfxWeatherOn())
                {
                    float dmW = deform - _DeformNeutral.x;
                    float texW = 2.0 * _GridOrigin.z / max(_GridParams.x, 1.0);           // m between the +- 1 texel taps
                    float conc = dot(rfxD4, 0.25) - deform;
                    float2 dGrad = float2(rfxD4.x - rfxD4.y, rfxD4.w - rfxD4.z) / texW;   // +u = +x, +v (D3D) = -z
                    float basin = dist < 220.0 ? RfxWBasin(uv) : 0.0;
                    float trk = 0.0, trkWide = 0.0; float2 trkGrad = 0;
                    if (dist < 260.0) RfxWTrackTaps(P, trk, trkWide, trkGrad);
                    rfxW = RfxWeatherGroundPrepare(P, rfxNgeo, N, dmW, conc, basin, trk, trkWide, trkGrad, dGrad, V, dist);
                    N = rfxW.N;
                    if (rfxW.puddle > 0.001 && dist < 70.0)
                    {
                        float2 rg = RfxWeatherRipples(P.xz, _RfxWeather2.w, _RfxWeather2.y) * saturate(_RfxWeather2.y * 4.0);
                        rfxWaterN = normalize(float3(-rg.x * 0.35, 1.0, -rg.y * 0.35));
                    }
                    if (_RfxWeatherDebug.x > 0.5)
                    {
                        float k = _RfxWeatherDebug.x;
                        float3 dbg = k < 1.5 ? float3(rfxW.puddle, rfxW.edge, rfxW.wet * 0.3) : k < 2.5 ? float3(rfxW.snow, rfxW.trail, rfxW.mudTrail)
                                   : k < 3.5 ? float3(saturate(-dmW * 14.0), saturate(dmW * 14.0), saturate(conc * 60.0)) : float3(trk, trkWide, saturate(basin * 3.0));
                        return half4(dbg, 1);
                    }
                }
            #endif

                // Reflection / spherical maps (0abc..0b24, 0af0, 0f14): mip 0 (sampler LOD bias -15).
                float3 R = reflect(V, N);
                float4 srm = SAMPLE_TEXTURE2D_LOD(_SphericalReflectionMap, sampler_SphericalReflectionMap, float2(0.5 + 0.5 * R.x, 0.5 + 0.5 * R.z), 0);
                float4 slm = SAMPLE_TEXTURE2D_LOD(_SphericalLightMap, sampler_SphericalLightMap, float2(0.5 + 0.5 * N.x, 0.5 + 0.5 * N.z), 0);

                // Sun visibility (0b34..0c14): radiosity map is Editor_Default_White in both levels -> 1.
                float vis = _UseShadowVis > 0.5 ? SAMPLE_TEXTURE2D(_ShadowVisMap, sampler_ShadowVisMap, uvU).r : 1.0;
                float rtW = rt.w;
                if (_UseUnityShadows > 0.5)
                {
                #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                    float4 shadowCoord = ComputeScreenPos(TransformWorldToHClip(P));
                #else
                    float4 shadowCoord = TransformWorldToShadowCoord(P);
                #endif
                    rtW *= RfxMainLightShadow(shadowCoord);             // stands in for RT_Bump.w (ps_hfb_shadows)
                }
            #if defined(_RFX_ENHANCED)
                // LIGHTING: ENHANCED (not the original program): contact shadows on the sun, AO and the sky's colour on the light map
                float2 rfxOcc = RfxEnhOcclusion(i.positionCS);
                rtW *= rfxOcc.g;
                slm.rgb = RfxEnhAmbient(slm.rgb, N, rfxOcc.r);
            #endif
            #if defined(_HFT_FMX_FF)
                float4 cref = _RfxColorRefOn > 0.5 ? RfxColorRefAt(P) : _ColorRef;   // s13 at gColorRefProjection (a map on Kingston)
                float3 rad = cref.rgb;                                    // raw radiosity sample (0bd0)
                float3 sunVis = max(0.5, vis * rtW);                      // 0b90
                float3 spec = srm.rgb + srm.a * sunVis * rad;             // 0be0
                float3 sunLight = sunVis * (rad * _SunColor.rgb + rad);   // 0bf4..0c08
            #else
            #if defined(_HFT_SX_FY)
                rtW = lerp(1.0, rtW, bumpFade);                           // (1 - v2.w) + RT.w v2.w (1b30..1b50)
            #endif
                float4 cref = _RfxColorRefOn > 0.5 ? RfxColorRefAt(P) : _ColorRef;   // s13 at gColorRefProjection (a map on Kingston)
                float3 rad = lerp(float3(1, 1, 1), cref.rgb, floor(cref.a));   // floor(rad.a) ? rad.rgb : 1 (0b64..0b90)
                float3 sunVis = rad * vis * rtW;
                float3 spec = srm.rgb + srm.a * sunVis * _SunColor.rgb;   // 0c24..0c34
                float3 sunLight = sunVis * _SunColor.rgb;                 // 0c48
            #endif

                // Deformation terms (0c58..0ca4) and detail mask (0cb4..0ce4).
                float dm = deform - 0.5;
                float maskScale = 1.0 - saturate(abs(dm) * 8.0);
                float deformLift = 1.0 - saturate(dm);
                float4 mask = SAMPLE_TEXTURE2D(_DetailMask, sampler_DetailMask, uvU);
                float3 topDown = SAMPLE_TEXTURE2D(_TopDownMap, sampler_TopDownMap, uvU).rgb;
                // ALIVE LOOK REFLEX (Content/AliveLook.cs; not the original program): an Alive level has no Reflex detail mask. Its decal composite
                // (baked into the top-down map, before the chroma / lum adjustment) picks the detail layers: what the composite shows (grass, snow,
                // rock, dirt) selects the donor layer of that kind, as weights of the mask chain (w3 = b, w2 = g (1 - b), w1 = r (1 - g)(1 - b));
                // lookMean is the picked layers' mean colour. (Nearest mean colour alone failed: the two games' palettes differ, e.g. Alive's
                // farm track matched Concord's dark grass layer.)
                float3 lookMean = 1.0, lookTD = topDown, lookClay = 1.0;
                [branch] if (_AliveLook.x > 0.5)
                {
                    // the composite balanced per channel onto the layers' palette (its whole-map mean _AliveTopDownMean, read back by AliveTerrainVT,
                    // to the four layers' mean colour: the two games' brightness and colour balance differ, Alive's grade was its LUT), then the
                    // nearest layer mean
                    float3 layersAvg = 0.25 * (_AliveLayerMean0.rgb + _AliveLayerMean1.rgb + _AliveLayerMean2.rgb + _AliveLayerMean3.rgb);
                    float3 tdAvg = _AliveTopDownMean.a > 0.5 ? _AliveTopDownMean.rgb : layersAvg;
                    lookTD = topDown * (layersAvg / max(tdAvg, 0.02));
                    // what the composite shows, as soft scores: grass (green over red and blue), snow (well above the map's mean brightness,
                    // unsaturated), rock (grey, at or above the mean), dirt (the rest); a class the donor has no layer for counts as dirt
                    const float3 LW = float3(0.299, 0.587, 0.114);
                    // (measured on the composites, raw: the farm / tropic grass 0.02 .. 0.25 green excess, the snow 1.45 .. 1.55 of the map's mean
                    // brightness at saturation ~0, the mud 0.5 .. 0.6)
                    float lt = max(dot(lookTD, LW), 1e-3), lr = max(dot(topDown, LW), 1e-3);
                    float mx = max(topDown.r, max(topDown.g, topDown.b)), mn = min(topDown.r, min(topDown.g, topDown.b));
                    float sat = (mx - mn) / max(mx, 1e-3);
                    float relB = lr / max(dot(tdAvg, LW), 0.02);
                    float sGrass = smoothstep(0.0, 0.05, (topDown.g - max(topDown.r, topDown.b)) / lr);
                    float sSnow = smoothstep(1.2, 1.4, relB) * (1.0 - smoothstep(0.12, 0.3, sat)) * (1.0 - sGrass);
                    float sRock = (1.0 - smoothstep(0.08, 0.18, sat)) * smoothstep(0.85, 1.1, relB) * (1.0 - sGrass) * (1.0 - sSnow);
                    float4 present = _AliveLayerClassPresent;   // dirt, grass, snow, rock
                    float4 cls = float4(0, sGrass, sSnow, sRock) * present.xyzw;
                    cls.x = saturate(1.0 - cls.y - cls.z - cls.w);
                    // each layer: its class's score, the layers of one class shared by how near their brightness is to the composite's
                    float4 ml = float4(dot(_AliveLayerMean0.rgb, LW), dot(_AliveLayerMean1.rgb, LW), dot(_AliveLayerMean2.rgb, LW), dot(_AliveLayerMean3.rgb, LW));
                    float4 near = exp(-_AliveLook.w * (lt - ml) * (lt - ml));
                    float4 lc = _AliveLayerClass;
                    float4 score = float4(cls[(int)lc.x], cls[(int)lc.y], cls[(int)lc.z], cls[(int)lc.w]);
                    float4 classSum = float4(dot(near, (float4)(lc == lc.x)), dot(near, (float4)(lc == lc.y)), dot(near, (float4)(lc == lc.z)), dot(near, (float4)(lc == lc.w)));
                    float4 wk = score * near / max(classSum, 1e-4);
                    wk /= max(dot(wk, 1.0), 1e-6);
                    float mb = wk.w, mg = wk.z / max(1.0 - mb, 1e-4), mr = wk.y / max((1.0 - mb) * (1.0 - mg), 1e-4);
                    // a: the picked layers' specular masks on the donor (x _AliveLook2.y)
                    mask = float4(saturate(float3(mr, mg, mb)), _AliveLook2.y * dot(wk, float4(_AliveLayerMean0.a, _AliveLayerMean1.a, _AliveLayerMean2.a, _AliveLayerMean3.a)));
                    lookMean = wk.x * _AliveLayerMean0.rgb + wk.y * _AliveLayerMean1.rgb + wk.z * _AliveLayerMean2.rgb + wk.w * _AliveLayerMean3.rgb;
                    // red clay (w-alivefix): dirt the composite shows far redder than the picked donor layers (JS7's red supercross clay under
                    // Renne's tan sand) takes the composite's colour: the detail and the far terrain carried by the per-channel ratio of the
                    // composite's luminance-normalised colour to the layers' (luminance kept), by _AliveLook3.x, by how far its redness,
                    // (r - b) / (r + g + b), exceeds the layers' (_AliveLook3.y .. z) and by the dirt layers' share of the pick; a composite no redder than the donor's
                    // dirt (every donor's own palette: sand, grey soil, Stillwater's red clay under Tropic's) is left as it was
                    [branch] if (_AliveLook3.x > 0.0)
                    {
                        float lm = max(dot(lookMean, LW), 1e-3);
                        float redT = (lookTD.r - lookTD.b) / max(dot(lookTD, 1.0), 1e-3), redM = (lookMean.r - lookMean.b) / max(dot(lookMean, 1.0), 1e-3);
                        float dirtShare = dot(wk, (float4)(lc == 0.0));
                        float clay = _AliveLook3.x * smoothstep(_AliveLook3.y, _AliveLook3.z, redT - redM) * dirtShare;
                        lookClay = lerp(1.0, clamp((lookTD / lt) / max(lookMean / lm, 1e-3), 0.25, 4.0), clay);
                        lookMean *= lookClay;
                        if (_AliveLook2.w > 1.5) return half4(clay, redT - redM, dirtShare, 1);   // (REFLEX_ALIVE_LOOK_DEBUG 2: r the clay weight, g the excess redness, b the dirt share)
                    }
                    if (_AliveLook2.w > 0.5) return half4(wk.yzw, 1);   // (REFLEX_ALIVE_LOOK_DEBUG: the pick, base black, red / green / blue layers)
                }
            #if defined(_HFT_4D_CL_S) || defined(_HFT_SX_FY)
                float3 w = mask.rgb;                                      // the raw mask (sx_fy scales the chain below)
            #else
                float3 w = maskScale * mask.rgb * rutFlat;                // 0cc4..0ce4
            #endif
                float specMask = mask.a * bumpFade;

                // Detail layers (0cf4..0dcc), D3D uv * tile with Unity v = 1 - v_d3d * tile.
                float bias = _DetailParams.x;
                float4 dBase  = SAMPLE_TEXTURE2D_BIAS(_DetailBase,  sampler_DetailBase,  FlipV(uv * _DetailTile.x), bias);
                float4 dRed   = SAMPLE_TEXTURE2D_BIAS(_DetailRed,   sampler_DetailRed,   FlipV(uv * _DetailTile.y), bias);
                float4 dGreen = SAMPLE_TEXTURE2D_BIAS(_DetailGreen, sampler_DetailGreen, FlipV(uv * _DetailTile.z), bias);
                float4 dBlue  = SAMPLE_TEXTURE2D_BIAS(_DetailBlue,  sampler_DetailBlue,  FlipV(uv * _DetailTile.w), bias);
                float4 detail = lerp(dBase, dRed, w.r);
                detail = lerp(detail, dGreen, w.g);
                detail = lerp(detail, dBlue, w.b);
            #if defined(_HFT_SX_FY)
                detail = lerp(dRed, detail, maskScale * rutFlat);         // 180c -> 1c14 -> 1d30: deformation amount AND adjacent rut slope
            #endif
                // Imported tracks only. Reuse native soil textures at two physical scales;
                // no additional sampler, and no change to the deformation/track normal path.
                if (_MtxDetail.x > 0.0)
                {
                    float2 soilUV = uv * _DetailTile.x;
                    float2 rotated = float2(soilUV.y, -soilUV.x) * .371 + float2(.37, .61);
                    float3 coarse = SAMPLE_TEXTURE2D_BIAS(_DetailBase, sampler_DetailBase, FlipV(rotated), bias).rgb;
                    float fine = dot(SAMPLE_TEXTURE2D_BIAS(_DetailRed, sampler_DetailRed,
                        FlipV(soilUV * 5.13), bias).rgb, float3(.299,.587,.114));
                    float soil = (1.0 - w.b) * (1.0 - w.g);
                    detail.rgb = lerp(detail.rgb, coarse, _MtxDetail.x * soil * (1.0 - .7 * w.r));
                    float nearGrain = 1.0 - saturate(dist / max(_MtxDetail.z, 1.0));
                    detail.rgb *= lerp(1.0, clamp(fine * max(_MtxDetail.w, 2.0), .65, 1.35), _MtxDetail.y * nearGrain * soil);
                }
                if (_MtxSourceColour > .5)
                {
                    // Authored MTX colour controls the venue palette; native detail is
                    // normalized by its measured layer means, retaining fine grain/relief.
                    float3 mean = lerp(float3(.305,.242,.200),float3(.320,.265,.217),w.r);
                    mean=lerp(mean,float3(.25,.24,.22),w.g);
                    mean=lerp(mean,float3(.180,.181,.094),w.b);
                    detail.rgb=topDown*clamp(detail.rgb/max(mean,.02),.4,2.0);
                }
                // ALIVE LOOK REFLEX: the detail's brightness moved toward the composite's (z: 0 the Reflex layers' own, 1 the Alive layout's); the hue
                // stays the Reflex layers' (Alive's own hue came from its colour LUT, which the REFLEX look does not use)
                // (red clay: the detail in the composite's colour, lookClay above; 1 elsewhere)
                detail.rgb *= lookClay;
                float lookLum = dot(lookMean, float3(0.299, 0.587, 0.114)), tdLum = dot(lookTD, float3(0.299, 0.587, 0.114));
                if (_AliveLook.z > 0.0) detail.rgb *= lerp(1.0, clamp(tdLum / max(lookLum, 0.02), 0.25, 4.0), _AliveLook.z);
                // and the far terrain (the top-down map) in the picked layers' hue at the composite's brightness (_AliveLook2.x: how much)
                if (_AliveLook2.x > 0.0) topDown = lerp(topDown, lookMean * (tdLum / max(lookLum, 0.02)), _AliveLook2.x);

                float specAmount = specMask * detail.a * (1.0 - rt.z);     // 0de0..0e00
                spec *= specAmount;                                       // 0e10

                // Rut dirt tint (0e20..0e64): neutral without ruts.
                float2 tintT = float2(rt.y, saturate(rt.x + 0.5)) * deformLift - 1.0;
                tintT = bumpFade * tintT + 1.0;
                float4 cl = SAMPLE_TEXTURE2D(_ChromLumMap, sampler_ChromLumMap, uvU);
                // ALIVE LOOK REFLEX: the baked composite is Alive's vt (before its chroma / lum adjustment): adjusted as the near detail is
                if (_AliveLook.y > 0.5) topDown = (topDown + _DetailParams.y * cl.rgb) * saturate(cl.a + cl.a) + saturate(cl.a - 0.5);
            #if defined(_HFT_4D_CL_S)
                // _4d_cl_s: chroma / lum on the detail first, then the dirt tint; the top-down blend uses the bump fade (v2.w)
                float3 adj = (cl.rgb * _DetailParams.y + detail.rgb) * saturate(cl.a + cl.a) + saturate(cl.a - 0.5);
                float3 tinted = lerp(_DirtTint.rgb, adj, tintT.x);
                float3 albedo = lerp(topDown, tintT.y * tinted, bumpFade);
            #else
                float3 tinted = lerp(_DirtTint.rgb, detail.rgb, tintT.x);

                // Chrominance / luminance adjustment (0e78..0ebc).
                float3 adj = (cl.rgb * _DetailParams.y + tinted) * saturate(cl.a + cl.a) + saturate(cl.a - 0.5);

                // Near detail over the far top-down render (0ed0..0ef4).
                float3 albedo = lerp(topDown, tintT.y * adj, detailFade);
            #endif

            #if defined(_HFT_FMX_FF)
                float3 colour = albedo * (sunLight * N.y + slm.rgb) + spec;     // 0ef8..0f0c: N.y instead of slm.a
            #else
                float3 colour = albedo * (sunLight * slm.a + slm.rgb) + spec;   // 0f24..0f38
            #endif
            #if defined(_RFX_ENHANCED)
                [branch] if (RfxWeatherOn())
                {
                #if defined(_HFT_FMX_FF)
                    float3 sunW = sunLight * N.y;
                #else
                    float3 sunW = sunLight * slm.a;
                #endif
                    float3 Rw = reflect(V, N), Rp = reflect(V, rfxWaterN);
                    colour = RfxWeatherGroundShade(rfxW, albedo, sunW + slm.rgb, spec, sunW, V, RfxWeatherReflection(Rw, 0.55), RfxWeatherReflection(Rp, 0.0), rfxWaterN);
                }
            #endif

                // Distance fog from vs_hft_lod0 (0944..0a78): constants mapped from DistanceFogRSData.
                if (_FogEnabled > 0.5)
                {
                    // same op order as the vertex program (c14 = (start, end, 1 / (end - start), -start / (end - start)))
                    float range = _FogParams.y - _FogParams.x;
                    float f = range != 0 ? saturate(dist * rcp(range) + (-_FogParams.x * rcp(range))) : 0.0;
                    float amount = clamp(f * (_FogParams.w - _FogParams.z) + _FogParams.z, _FogParams.z, _FogParams.w);
                    float3 s = normalize(float3(_SunDirection.x, (dirToCam.y + _SunDirection.y) * 0.5, _SunDirection.z));
                    float k = saturate(dot(s, dirToCam) * 0.5 + 0.5); k = saturate(k + k);
                    float3 fogColour = _FogColorStart.rgb + (_FogColorEnd.rgb - _FogColorStart.rgb) * k;
                    colour = colour * (1.0 - amount) + amount * fogColour;
                }

                // raw value for the HDR target of ReflexRenderPipeline (converted per object without it), ReflexCommon.hlsl
                if (_GammaOutput < 0.5) return half4(max(colour, 0.0), 1.0);
                return half4(RfxOutput(colour), 1.0);
            }
            ENDHLSL
        }

        Pass
        {
            Name "ShadowCaster"
            Tags { "LightMode"="ShadowCaster" }
            ZWrite On
            ZTest LEqual
            ColorMask 0

            HLSLPROGRAM
            #pragma target 2.0
            #pragma vertex ShadowVert
            #pragma fragment ShadowFrag
            #pragma multi_compile_instancing
            #pragma instancing_options assumeuniformscaling nomatrices nolightprobe nolightmap
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            float3 _LightDirection;
            float3 _LightPosition;

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 texcoord : TEXCOORD0; UNITY_VERTEX_INPUT_INSTANCE_ID };
            struct Varyings { float4 positionCS : SV_POSITION; };

            Varyings ShadowVert(Attributes v)
            {
                Varyings o = (Varyings)0;
                UNITY_SETUP_INSTANCE_ID(v);
                ReflexTerrainInstancing(v.positionOS, v.normalOS, v.texcoord);
                float3 positionWS = RfxRTBumpDisplace(TransformObjectToWorld(v.positionOS.xyz));
                float3 normalWS = TransformObjectToWorldNormal(v.normalOS);
            #if _CASTING_PUNCTUAL_LIGHT_SHADOW
                float3 lightDirectionWS = normalize(_LightPosition - positionWS);
            #else
                float3 lightDirectionWS = _LightDirection;
            #endif
                float4 positionCS = TransformWorldToHClip(ApplyShadowBias(positionWS, normalWS, lightDirectionWS));
            #if UNITY_REVERSED_Z
                positionCS.z = min(positionCS.z, UNITY_NEAR_CLIP_VALUE);
            #else
                positionCS.z = max(positionCS.z, UNITY_NEAR_CLIP_VALUE);
            #endif
                o.positionCS = positionCS;
                return o;
            }

            half4 ShadowFrag(Varyings i) : SV_Target { return 0; }
            ENDHLSL
        }

        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            ZWrite On
            ColorMask R

            HLSLPROGRAM
            #pragma target 2.0
            #pragma vertex DepthVert
            #pragma fragment DepthFrag
            #pragma multi_compile_instancing
            #pragma instancing_options assumeuniformscaling nomatrices nolightprobe nolightmap

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 texcoord : TEXCOORD0; UNITY_VERTEX_INPUT_INSTANCE_ID };
            struct Varyings { float4 positionCS : SV_POSITION; UNITY_VERTEX_OUTPUT_STEREO };

            Varyings DepthVert(Attributes v)
            {
                Varyings o = (Varyings)0;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
                ReflexTerrainInstancing(v.positionOS, v.normalOS, v.texcoord);
                o.positionCS = TransformWorldToHClip(RfxRTBumpDisplace(TransformObjectToWorld(v.positionOS.xyz)));
                return o;
            }

            half DepthFrag(Varyings i) : SV_Target { return i.positionCS.z; }
            ENDHLSL
        }

        Pass
        {
            Name "DepthNormals"
            Tags { "LightMode"="DepthNormals" }
            ZWrite On

            HLSLPROGRAM
            #pragma target 2.0
            #pragma vertex DepthNormalsVert
            #pragma fragment DepthNormalsFrag
            #pragma multi_compile_fragment _ _GBUFFER_NORMALS_OCT
            #pragma multi_compile_instancing
            #pragma instancing_options assumeuniformscaling nomatrices nolightprobe nolightmap

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 texcoord : TEXCOORD0; UNITY_VERTEX_INPUT_INSTANCE_ID };
            struct Varyings { float4 positionCS : SV_POSITION; float3 normalWS : TEXCOORD0; float2 terrainUV : TEXCOORD1; UNITY_VERTEX_OUTPUT_STEREO };

            Varyings DepthNormalsVert(Attributes v)
            {
                Varyings o = (Varyings)0;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);
                ReflexTerrainInstancing(v.positionOS, v.normalOS, v.texcoord);
                o.positionCS = TransformWorldToHClip(RfxRTBumpDisplace(TransformObjectToWorld(v.positionOS.xyz)));
                o.normalWS = TransformObjectToWorldNormal(v.normalOS);
                o.terrainUV = v.texcoord;
                return o;
            }

            half4 DepthNormalsFrag(Varyings i) : SV_Target
            {
                float3 normalWS = ReflexTerrainNormalWS(i.terrainUV, i.normalWS);
            #if defined(_GBUFFER_NORMALS_OCT)
                float2 octNormalWS = PackNormalOctQuadEncode(normalWS);
                float2 remapped = saturate(octNormalWS * 0.5 + 0.5);
                return half4(PackFloat2To888(remapped), 0.0);
            #else
                return half4(NormalizeNormalPerPixel(normalWS), 0.0);
            #endif
            }
            ENDHLSL
        }
    }
    Fallback "Hidden/Universal Render Pipeline/FallbackError"
}
