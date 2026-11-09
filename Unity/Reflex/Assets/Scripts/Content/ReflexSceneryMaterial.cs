// Scenery material factory: turns a level material (pixel shader + textures in sampler order + parameters + render states, all read
// from the install) into a material of the hand-ported shader "Reflex/Original Scenery" (Assets/Shaders/ReflexOriginalScenery.shader).
//
// The original materials use ~40 pixel shaders (Template_Diffuse, Template_Diffuse_AO, Template_DF_AO_SP_NM, Template_Trackside_*,
// Template_Prelit_*, the Alpine edge/rock shaders ...). They share building blocks (fill lights + sun x shadow, specular, reflection,
// distance fog); which blocks a program uses and its literal constants are listed in Templates, transcribed from the disassembly of
// every program recorded in frames of the original renderer (NAT_Concord, SX_Kingston, FR_Alpine: _re/notes/render-pipeline.md
// section 6). Programs that are not listed fall back to what their constant table declares (gBackLight -> per-pixel ambient,
// samplerVolumeShadowMap, samplerDynamicShadowMap, usr_specular_hardness ...).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    internal static class ReflexSceneryMaterial
    {
        public const int ModeLit = 0, ModePrelit = 1, ModeRock = 2, ModeEdge = 3, ModeEdgeAlpha = 4, ModePrelitVertex = 5, ModeVehicle = 6,
                         ModePrelitUV1 = 7, ModePrelitStadiums = 8, ModePrelitColor = 9, ModePrelitBackground = 10, ModePrelitVertexColour = 11, ModeLightBeam = 12, ModeGibsonCliff = 13, ModeRenneEdge = 14, ModeRenneMountains = 15, ModeVertexBlendCliff = 16, ModePrelitVertexColourOpaque = 17, ModeAnimChevron = 18, ModeAnimChevronNoFade = 19,
                         ModeX2Overlay = 20, ModeFmxRamp = 21, ModeFmxRampTopDover = 22,
                         ModePrelitProduct = 23, ModePrelitBanners = 25, ModePrelitDim = 26;

        // One template program: what its pixel / vertex code does (see ReflexOriginalScenery.shader for the blocks).
        sealed class Tpl
        {
            public int mode = ModeLit, diffuseSrc, ao, normal, alpha;   // diffuse 0 tex 1 tex(1-vcol) 2 constant 3 0.4 vcol.xzy 4 bayle; ao 0..3; normal 0 vertex 1 amount 2 direct
            public bool diffuseUV1, normalUV1, twoSided, pixelAmbient, dyn = true, vol = true, shadowLit = true;
            public float hardness, scale; public bool specSun, spec; public string specScaleParam, specTintParam, hardnessParam;
            public float normalAmount = 1f; public string normalAmountParam = "usr_normal_amount";
            public float env; public string envParam;                  // reflection amount (literal) or its parameter
            public Color colorDefault = Color.white;
            public string aoParam, colorParam, fadeParam = "usr_diffuse_fade", powerParam, sssParam, emissiveParam;
            public Vector2 uvScale = Vector2.one;
            // w-fsshaders (the Freestyle arenas' programs):
            //   slots      the role of each sampler register s0.. (diffuse, ao, normal, detail, combo, cube; null: unused), instead of the names
            //   vcol       D x vertex colour: 1 rgb, 2 xzy        aoUV0  the AO map at UV0 (else UV1)
            //   specMap    the specular term x 1 the detail map's rgb (UV0), 2 the AO sample, 3 the diffuse rgb
            //   alphaParam the constant output alpha (alpha 6)    power  the literal prelit scale (mode 23)
            //   twoTone    mode 23: lerp(detail, diffuse, vertex alpha), the detail at 1 UV0, 2 the volume-shadow projection of the model
            //              position x 5 (Template_Sx_sp_hills)
            //   uvMatrix   the diffuse UV in D3D space times (x y; z w) (FF_Chainlink_Fence)
            //   envGlobal  the level's reflection cube (s14) instead of the material's; envSharpParam: texldb bias 30 / max(s, 1) - 1/3 (s < 90)
            //   envMask    1 the combo map's rgb (UV0) x envTintParam, 2 diffuse alpha x the AO sample, 3 diffuse alpha; envNoWeight: no (min(shadow, N.L) + 1) / 2
            //   selfLit    + D x this (FF_Largebuilding_windows02: D ambient + D); noRad: the sun term without the colour reference radiosity
            public string[] slots;
            public int vcol, specMap, twoTone, envMask;
            public bool aoUV0, envGlobal, envNoWeight, noRad;
            public string alphaParam, envSharpParam, envTintParam, colorParam2;
            public float power = 1f, envSharpDefault = 5f, alphaDefault = 1f, selfLit;
            public Vector4 uvMatrix = new Vector4(1, 0, 0, 1);
            public Color color2Default = Color.white;
        }

        static readonly Dictionary<string, Tpl> Templates = new Dictionary<string, Tpl>(StringComparer.Ordinal)
        {
            // Renne A/B have byte-identical original four-texture outer-terrain programs.
            ["NAT_RE_A_EOW"] = new Tpl { mode = ModeRenneEdge, pixelAmbient = true, dyn = false, vol = false },
            ["NAT_RE_B_Mountains"] = new Tpl { mode = ModeRenneMountains, dyn = false, vol = false },
            ["NAT_RE_B_EOW"] = new Tpl { mode = ModeRenneEdge, pixelAmbient = true, dyn = false, vol = false },
            // Byte-identical original programs: four samplers, two diffuse overlays and averaged detail/decal normals.
            ["FR_GI_CliffRockGeneric_NT"] = new Tpl { mode = ModeGibsonCliff, pixelAmbient = true },
            ["FR_GI_CliffRockBlueTinge_NT"] = new Tpl { mode = ModeGibsonCliff, pixelAmbient = true },
            ["FR_GI_CliffRockPeachTinge_NT"] = new Tpl { mode = ModeGibsonCliff, pixelAmbient = true },
            // lit, vertex ambient
            ["Template_Diffuse"] = new Tpl { twoSided = true },
            ["Template_Diffuse_Scenario"] = new Tpl { twoSided = true },
            ["Template_Diffuse_Alpha"] = new Tpl { alpha = 3 },
            ["Template_Diffuse_AO"] = new Tpl { ao = 1, aoParam = "usr_ao_intensity" },
            ["Template_DF_X2_VXB_AO_NM"] = new Tpl { mode = ModeVertexBlendCliff, ao = 1, aoParam = "usr_ao_amount", normal = 1, pixelAmbient = true },
            // The Freestyle arenas' roll-in and quarter-pipe tops (FMX_DO_B_Rollin_Top2, FMX_SP_Qpipe_Top ...; w-fsfix). Pixel program (FMX_DoverB's
            // Template_DF_X2_AO_SP, 04a0..0894; DF_X2_AO the same without the specular): s3 usr_sampler_diffuse_texture at UV0, lrp toward s1
            // usr_sampler_diffuse_map_2 by s2 usr_sampler_diffuse_map_2_alpha's rgb (both UV1), times 1 - (1 - ao(s0, UV1)) usr_ao_amount; vertex
            // ambient (v7); the sun with the shadows; SP: + usr_specular_color pow(sat(L.R), usr_specular_hardness) colour reference c56 min(shadow, N.L).
            ["Template_DF_X2_AO"] = new Tpl { mode = ModeX2Overlay, ao = 1, aoParam = "usr_ao_amount" },
            ["Template_DF_X2_AO_SP"] = new Tpl { mode = ModeX2Overlay, ao = 1, aoParam = "usr_ao_amount", spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1 },
            // The FMX ramps' tops (w-fsfix). FMX_DO_Ramp360 (0480..07dc; FMX_DO_RampS_LL byte-identical): D = s1(UV0) + s0(UV1) (the ramp texture plus
            // the chevrons), the per-pixel ambient (fills + the colour reference bounce), the sun with the shadows, alpha sat(s2(UV0).a).
            // FMX_DO_RampFMX_LL: the same with two samplers, alpha sat(s1(UV0).a).
            ["FMX_DO_Ramp360"] = new Tpl { mode = ModeFmxRamp, pixelAmbient = true, alpha = 3 },
            ["FMX_DO_RampS_LL"] = new Tpl { mode = ModeFmxRamp, pixelAmbient = true, alpha = 3 },
            ["FMX_DO_RampFMX_LL"] = new Tpl { mode = ModeFmxRamp, pixelAmbient = true, alpha = 3 },
            // FMX_RampTop_Dover_SH. (FMX_DoverB): D = s1(UV0) s0(UV0) 1.3, vertex ambient, the sun with the shadows, alpha 1.
            ["FMX_RampTop_Dover_SH."] = new Tpl { mode = ModeFmxRampTopDover },
            // ---- w-fsshaders: the rest of the Freestyle arenas' programs (and their Free Ride levels), from the levels' shader assets
            // (scratch: _re/formats/d3d9_shader.py; every name has one program in the install). Offsets are the pixel programs'.
            // Unlit (_Mode 23: diffuse x AO map (UV1) x vertex colour x power, alpha by template):
            //   Template_Prelit_VertexColor 0144..0198 (FunFactory's prelit walls / clutter): s1(UV0) s0(UV1) vertex rgb, alpha 1
            ["Template_Prelit_VertexColor"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse" }, ao = 2, vcol = 1 },
            //   FF_FurnaceBaseGrate / FF_WallRideGrate (one program, 0114..0170): s0(UV1) s1(UV0) vertex rgb 1.5, alpha sat(s1.a)
            ["FF_FurnaceBaseGrate"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse" }, ao = 2, vcol = 1, power = 1.5f, alpha = 1 },
            ["FF_WallRideGrate"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse" }, ao = 2, vcol = 1, power = 1.5f, alpha = 1 },
            //   walkwayWESTgrateTEST 00fc..0148: s0(UV1) s1(UV0) vertex rgb, alpha sat(s1.a); walkwayEASTgrateTEST 00f0..012c: the same without
            //   the vertex colour (its vertex program has none)
            ["walkwayWESTgrateTEST"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse" }, ao = 2, vcol = 1, alpha = 1 },
            ["walkwayEASTgrateTEST"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse" }, ao = 2, alpha = 1 },
            //   grateTEST 00c0..00ec and Template_Prelit_VXC_Alpha_TR 00c8..00f4: s0(UV0) vertex rgb, alpha sat(s0.a)
            ["grateTEST"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "diffuse" }, vcol = 1, alpha = 1 },
            ["Template_Prelit_VXC_Alpha_TR"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "diffuse" }, vcol = 1, alpha = 1 },
            //   FF_Razor_Wire 00cc..00f8: s0(UV0) 0.225, alpha sat(s0.a)
            ["FF_Razor_Wire"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "diffuse" }, power = 0.224999994f, alpha = 1 },
            //   FF_Lights_02 0090..00c0: vertex rgb 10; FF_PowerLines 0090..00b0: vertex rgb (no textures)
            ["FF_Lights_02"] = new Tpl { mode = ModePrelitProduct, slots = new string[0], vcol = 1, power = 10f },
            ["FF_PowerLines"] = new Tpl { mode = ModePrelitProduct, slots = new string[0], vcol = 1 },
            //   FF_Chainlink_Fence 00cc..0160: s0 at (dp2add(uv, -k, -k), dp2add(uv, k, -k)) (k = 0.106066: the mesh UV turned 45 degrees and
            //   scaled), x 0.4, alpha sat(s0.a)
            ["FF_Chainlink_Fence"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "diffuse" }, power = 0.400000006f, alpha = 1, uvMatrix = new Vector4(-0.106066003f, -0.106066003f, 0.106066003f, -0.106066003f) },
            //   Template_Prelit_TR 0114..0148: s0(UV0), alpha sat(s1(UV0).a)
            ["Template_Prelit_TR"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "diffuse", "detail" }, alpha = 3 },
            //   FF_Twotone_brick_02 (FF_B_ the same program) 0150..01b8: s0(UV1) lerp(s2(UV0), s1(UV0), vertex alpha)
            ["FF_Twotone_brick_02"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse", "detail" }, ao = 2, twoTone = 1 },
            ["FF_B_Twotone_brick_02"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse", "detail" }, ao = 2, twoTone = 1 },
            //   Template_Sx_sp_hills 018c..0204: s0(UV1) lerp(s2(5 x the model position through the volume-shadow projection c39 / c40),
            //   s1(UV0), vertex alpha)
            ["Template_Sx_sp_hills"] = new Tpl { mode = ModePrelitProduct, slots = new[] { "ao", "diffuse", "detail" }, ao = 2, twoTone = 2 },
            // Template_Prelit_Banners 01a0..0238 (_Mode 25): ((1 - usr_overal_lightness)(w - 1) + 1) lerp(s1(UV1), usr_light_color, w),
            // w = s0(UV0) (the wrinkle map), alpha 1
            ["Template_Prelit_Banners"] = new Tpl { mode = ModePrelitBanners, slots = new[] { "detail", "diffuse" }, diffuseUV1 = true, colorParam = "usr_light_color", colorDefault = new Color(0.411765f, 0.388235f, 0.305882f, 1f), powerParam = "usr_overal_lightness", power = 0.5f },
            // Template_Prelit_Color_Dim_Ctl 01b8..0294 (_Mode 26; light bulbs, porta lights, lasers): g = s0(1 - |V.N|);
            // g (usr_dim_amount - C P) + C P with C usr_constant_color, P usr_prelit_power; alpha sat(vertex colour r)
            ["Template_Prelit_Color_Dim_Ctl"] = new Tpl { mode = ModePrelitDim, slots = new[] { "diffuse" }, colorParam = "usr_constant_color", colorDefault = new Color(1f, 0.952941f, 0.109804f, 1f),
                                                         colorParam2 = "usr_dim_amount", color2Default = new Color(0.164706f, 0.164706f, 0.164706f, 1f), powerParam = "usr_prelit_power", power = 10f, alpha = 8 },
            // Lit (the lit path; ambient v = the vertex program's fills, p = per pixel; shadows d = s15 dynamic, v = s12 volume):
            //   Template_SC_AO 057c..05dc: usr_solid_color_1 (1 - (1 - s0(UV1)) usr_ao_amount), v, d v
            ["Template_SC_AO"] = new Tpl { diffuseSrc = 2, colorParam = "usr_solid_color_1", slots = new[] { "ao" }, ao = 1, aoParam = "usr_ao_amount" },
            //   Template_SC_VXAO 0278..0298: usr_base_color x vertex rgb, v, no shadows
            ["Template_SC_VXAO"] = new Tpl { diffuseSrc = 2, colorParam = "usr_base_color", vcol = 1, dyn = false, vol = false },
            //   FMX_Dover_RoughGrey 025c..027c (FMX_Dover_RoughLayout's material): (0.1745, 0.2020, 0.2255) x vertex rgb.xzy, v, no shadows
            ["FMX_Dover_RoughGrey"] = new Tpl { diffuseSrc = 2, colorDefault = new Color(0.174510002f, 0.201960996f, 0.225490004f, 1f), vcol = 2, dyn = false, vol = false },
            //   SM_Shelby_Buildings 041c..043c: 0.498 x vertex rgb, v, d
            ["SM_Shelby_Buildings"] = new Tpl { diffuseSrc = 2, colorDefault = new Color(0.498039007f, 0.498039007f, 0.498039007f, 1f), vcol = 1, vol = false },
            //   FMX_DO_CargoShipSS 0298..02c8: s0(UV1) x (0.8392, 0.8471, 0.8549), v, no shadows
            ["FMX_DO_CargoShipSS"] = new Tpl { diffuseSrc = 5, colorDefault = new Color(0.839215994f, 0.847059011f, 0.854902029f, 1f), slots = new[] { "diffuse" }, diffuseUV1 = true, dyn = false, vol = false },
            //   Template_DF_AO_RF_A 0840..08e8: s1(UV0) usr_diffuse_fade (1 - (1 - s0(UV1)) usr_ao_amount), v, d v, + the level cube (s14) at R with
            //   bias 30 / max(usr_reflection_sharpness, 1) - 1/3 (0 from 90) x (min(shadow, N.L) + 1) / 2 x s2(UV0) x usr_reflection_color
            ["Template_DF_AO_RF_A"] = new Tpl { slots = new[] { "ao", "diffuse", "combo" }, ao = 1, aoParam = "usr_ao_amount", env = 1f, envGlobal = true, envMask = 1, envTintParam = "usr_reflection_color", envSharpParam = "usr_reflection_sharpness", envSharpDefault = 6f },
            //   Template_DF_AO_RF_A_NM 0b30..0d14: the same with the normal map (s2, by usr_normal_amount), p, the mask s3
            ["Template_DF_AO_RF_A_NM"] = new Tpl { slots = new[] { "ao", "diffuse", "normal", "combo" }, ao = 1, aoParam = "usr_ao_amount", normal = 1, pixelAmbient = true, env = 1f, envGlobal = true, envMask = 1, envTintParam = "usr_reflection_color", envSharpParam = "usr_reflection_sharpness", envSharpDefault = 6f },
            //   Template_DF_AO_SP_TR 0770..082c: Template_DF_AO_SP (fade, specular colour), alpha sat(usr_opacity)
            ["Template_DF_AO_SP_TR"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 1, aoParam = "usr_ao_amount", spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1, alpha = 6, alphaParam = "usr_opacity", alphaDefault = 0.9f },
            //   Template_DF_AO_TRBlend 055c..05bc: s1(UV0) s0(UV1), v, d v, alpha sat(s1.a)
            ["Template_DF_AO_TRBlend"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 2, alpha = 1 },
            //   Template_DF_VXC_AO 05a8..0628: s1(UV0) (1 - (1 - s0(UV1)) usr_ao_amount) vertex rgb, v, d v
            ["Template_DF_VXC_AO"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 1, aoParam = "usr_ao_amount", vcol = 1 },
            //   Template_DF_VXC_AO_NM 09d0..0a30: s0(UV1) usr_ao_amount s1(UV0) vertex rgb, the normal map s2 by usr_normal_amount, p, d v
            ["Template_DF_VXC_AO_NM"] = new Tpl { slots = new[] { "ao", "diffuse", "normal" }, ao = 4, aoParam = "usr_ao_amount", vcol = 1, normal = 1, pixelAmbient = true },
            // Level programs whose samplers have no names (s_sampler0 ..): without their roles the fallback took the first texture, here the
            // light map, as the albedo at UV0 (w-fb-models: the Halifax train bridge's stone arches white with black blotches in rows, the
            // light map tiled up the pillars).
            //   TRX_HA_TrainBridge (TRX_Halifax / HalifaxBW) 039c..06dc: s2 normal map (UV0) direct, s1 stone (UV0) x vertex rgb x s0 light map
            //   (UV1), the four side lights and the colour reference per pixel of the bump normal, the sun N.L without a shadow map, fog
            ["TRX_HA_TrainBridge"] = new Tpl { slots = new[] { "ao", "diffuse", "normal" }, ao = 2, vcol = 1, normal = 2, pixelAmbient = true, dyn = false, vol = false },
            //   TRX_HA_Rock_LL / TRX_HA_RockOutcrop (the same program; the river rocks and outcrops beside it) 03c0..0724: s1 normal map (UV0)
            //   direct, s0 (UV0) x vertex rgb (the fallback lost the vertex colour: white rocks), the side lights per pixel, the sun with the
            //   volume shadow only
            ["TRX_HA_Rock_LL"] = new Tpl { slots = new[] { "diffuse", "normal" }, vcol = 1, normal = 2, pixelAmbient = true, dyn = false },
            ["TRX_HA_RockOutcrop"] = new Tpl { slots = new[] { "diffuse", "normal" }, vcol = 1, normal = 2, pixelAmbient = true, dyn = false },
            //   GL_ChallengeMarkerBase_NoTemp (the Race & Ride challenge markers' base, FR_Alpine Shared) 052c..05a8: s1 (UV0) x s0 AO (UV1),
            //   the vertex ambient, the sun with both shadow maps
            ["GL_ChallengeMarkerBase_NoTemp"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 2 },
            //   Template_DF_VX_NM 0978..09bc: s0 usr_diffuse_fade (1 - vertex rgb), the normal map s1 by usr_normal_amount, p, d v
            ["Template_DF_VX_NM"] = new Tpl { slots = new[] { "diffuse", "normal" }, diffuseSrc = 1, normal = 1, pixelAmbient = true },
            //   Template_DF_NM 0944..0964: s0, the normal map s1 by usr_normal_amount, p, d v
            ["Template_DF_NM"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 1, pixelAmbient = true },
            //   Template_DF_TRDifRefAlphaTest_NM 062c..0660: s0, the normal map s1 direct, p, no shadows, alpha sat(s0.a)
            ["Template_DF_TRDifRefAlphaTest_NM"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 2, pixelAmbient = true, dyn = false, vol = false, alpha = 1 },
            //   Template_Diffuse_AO_PP 06c0..07f4: s1(UV0) (1 - (1 - s0(UV1)) usr_ao_intensity), the per-pixel ambient of the vertex normal, d v
            ["Template_Diffuse_AO_PP"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 1, aoParam = "usr_ao_intensity", pixelAmbient = true },
            //   Template_Diffuse_Spec 05c0..06e4: s0, v, d v, + s1(UV0) pow(sat(L.R), usr_specular_hardness) sun colour min(shadow, N.L)
            ["Template_Diffuse_Spec"] = new Tpl { slots = new[] { "diffuse", "detail" }, spec = true, specSun = true, hardnessParam = "usr_specular_hardness", hardness = 25f, scale = 1, specMap = 1 },
            //   Template_Glass_Opaque 0324..0584: s0 usr_diffuse_fade, two-sided, v, no shadows, + usr_reflection_amount x the level cube (bias as above)
            ["Template_Glass_Opaque"] = new Tpl { slots = new[] { "diffuse" }, twoSided = true, dyn = false, vol = false, envParam = "usr_reflection_amount", env = 0.25f, envGlobal = true, envNoWeight = true, envSharpParam = "usr_reflection_sharpness" },
            //   Template_Glass_Transperant 033c..0540: the same one-sided, alpha sat(usr_transperancy)
            ["Template_Glass_Transperant"] = new Tpl { slots = new[] { "diffuse" }, dyn = false, vol = false, envParam = "usr_reflection_amount", env = 1f, envGlobal = true, envNoWeight = true, envSharpParam = "usr_reflection_sharpness", alpha = 6, alphaParam = "usr_transperancy", alphaDefault = 0.5f },
            //   Template_Glass_Transperant_NM 0518..09e4: + the normal map s1 by usr_normal_amount (two-sided), p
            ["Template_Glass_Transperant_NM"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 1, twoSided = true, pixelAmbient = true, dyn = false, vol = false, envParam = "usr_reflection_amount", env = 1f, envGlobal = true, envNoWeight = true, envSharpParam = "usr_reflection_sharpness", alpha = 6, alphaParam = "usr_transperancy", alphaDefault = 0.5f },
            //   Template_LoclRefl_Alpha_NM 04f4..0900: s0 and the normal map s1 (direct) at 10 UV0, p, v only, + usr_reflection_amount x the
            //   material's cube s2 x (min(volume shadow, N.L) + 1) / 2, alpha sat(s0.a)
            ["Template_LoclRefl_Alpha_NM"] = new Tpl { slots = new[] { "diffuse", "normal", "cube" }, normal = 2, pixelAmbient = true, dyn = false, alpha = 1, envParam = "usr_reflection_amount", env = 0.89f, uvScale = new Vector2(10, 10) },
            //   Template_Trackside_ShinyVinyl_VXClr 05bc..0b68: s0(UV1) vertex rgb, the normal map s1 direct (two-sided), p, d v, + 0.8 pow(sat(L.R), 150)
            //   min(shadow, N.L) (no sun colour), + usr_reflection_amount x the material's cube s2 x (min(shadow, N.L) + 1) / 2
            ["Template_Trackside_ShinyVinyl_VXClr"] = new Tpl { slots = new[] { "diffuse", "normal", "cube" }, normal = 2, twoSided = true, pixelAmbient = true, diffuseUV1 = true, vcol = 1, spec = true, hardness = 150f, scale = 0.800000012f, envParam = "usr_reflection_amount", env = 0.1f },
            //   Template_Trackside_Tent 05f4..0c0c: (1 - ao)(1 - usr_ao_amount) + ao s1(UV1) with ao = s0(UV0), the normal map s2 by usr_normal_amount
            //   (two-sided), p, d only, SSS usr_sss_amount, + usr_specular_amount ao pow(sat(L.R), 12) sun colour min(shadow, N.L)
            ["Template_Trackside_Tent"] = new Tpl { slots = new[] { "ao", "diffuse", "normal" }, ao = 5, aoParam = "usr_ao_amount", aoUV0 = true, diffuseUV1 = true, normal = 1, twoSided = true, pixelAmbient = true, vol = false,
                                                   spec = true, specSun = true, hardness = 12f, specScaleParam = "usr_specular_amount", specMap = 2, sssParam = "usr_sss_amount" },
            //   Template_Trackside_Tent_Metal 06d4..0760: s1(3 UV1) s0(UV0), v, d v, + usr_specular_color s0 pow(sat(L.R), usr_specular_hardness) sun colour
            //   min(shadow, N.L)
            ["Template_Trackside_Tent_Metal"] = new Tpl { slots = new[] { "ao", "diffuse" }, ao = 2, aoUV0 = true, diffuseUV1 = true, uvScale = new Vector2(3, 3), spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1, specMap = 2 },
            //   Lambert_Haybale_Test 0510..09f0: s0(UV1), the normal map s1 (UV0) direct, p, d v, + 0.75 pow(sat(L.R), 25) min(shadow, N.L) (no sun colour)
            ["Lambert_Haybale_Test"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 2, pixelAmbient = true, diffuseUV1 = true, spec = true, hardness = 25f, scale = 0.75f },
            //   Lambert_FMX_DO_AC1 03f0..0698: s1(UV0) s0(UV1), the normal map s2 direct, p, no shadows
            ["Lambert_FMX_DO_AC1"] = new Tpl { slots = new[] { "ao", "diffuse", "normal" }, ao = 2, normal = 2, pixelAmbient = true, dyn = false, vol = false },
            //   FF_Largebuilding_windows02 0470..07ec: the same, + D (D ambient + D), + s1.a s0 x the level cube (bias 0)
            ["FF_Largebuilding_windows02"] = new Tpl { slots = new[] { "ao", "diffuse", "normal" }, ao = 2, normal = 2, pixelAmbient = true, dyn = false, vol = false, selfLit = 1f, env = 1f, envGlobal = true, envMask = 2, envNoWeight = true, envSharpDefault = 1000f },
            //   Caustics 027c..03dc: usr_ray_color lit (two-sided, no shadows, v) + usr_power, alpha sat(s0(UV0).r x vertex colour r)
            ["Caustics"] = new Tpl { slots = new[] { "diffuse" }, diffuseSrc = 2, colorParam = "usr_ray_color", colorDefault = new Color(0.984314f, 0.862745f, 0.635294f, 1f), twoSided = true, dyn = false, vol = false, emissiveParam = "usr_power", alpha = 7 },
            //   Editor_Default 04f8..0518 / GL_Default 0440..0460 (anchors, collision stand-ins): s0, v, d v (GL_Default: d only)
            // The podium girls' dress and trophy (animated entities without an object program: the scenery factory draws them):
            //   Template_Podium_Dress 04f0..0ab8: s0, the normal map s1 direct (two-sided), p, d v, + s0 usr_specular_color sat(L.R)^4 sun colour
            //   min(shadow, N.L)
            ["Template_Podium_Dress"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 2, twoSided = true, pixelAmbient = true, spec = true, specSun = true, hardness = 4f, scale = 1, specTintParam = "usr_specular_color", specMap = 3 },
            //   Template_Tropy_Cup 056c..0bbc: s0 usr_color (0.2 + 1.1 |V.N|^3), the normal map s1 direct, p, d v, + usr_reflect_amount s0.a x the
            //   level cube x (min(shadow, N.L) + 1) / 2, + 1.5 usr_color sat(L.R)^5 min(shadow, N.L) (no sun colour)
            ["Template_Tropy_Cup"] = new Tpl { slots = new[] { "diffuse", "normal" }, normal = 2, pixelAmbient = true, diffuseSrc = 6, colorParam = "usr_color", colorDefault = new Color(0.792157f, 0.643137f, 0.184314f, 1f),
                                              spec = true, hardness = 5f, scale = 1.5f, specTintParam = "usr_color", envParam = "usr_reflect_amount", env = 0.1f, envGlobal = true, envMask = 3 },
            //   Template_Animated_UV_nonlit_alpha 0270..0478 (Dover's scrolling smoke; lit despite the name): s0 at UV0 + gTextureOffset, v, d v,
            //   the sun without the colour reference's radiosity, alpha sat(s0.a) x vertex alpha
            ["Template_Animated_UV_nonlit_alpha"] = new Tpl { slots = new[] { "diffuse" }, noRad = true, alpha = 9 },
            //   SkyCraneBlurBlades 01e4..02d0 (the FunFactory / Cheyenne sky crane's rotor blur): s0, v, no shadows, alpha sat(s0.a)
            ["SkyCraneBlurBlades"] = new Tpl { slots = new[] { "diffuse" }, dyn = false, vol = false, alpha = 1 },
            ["Editor_Default"] = new Tpl { slots = new[] { "diffuse" } },
            ["GL_Default"] = new Tpl { slots = new[] { "diffuse" }, vol = false },
            // Stillwater tree cards: s0 diffuse alpha (PS06ac), s1 tangent normal;
            // projected normal strength is literal3 (04e8), with unshadowed sunlight.
            ["NAT_ST_BB_Trees_01"] = new Tpl { alpha = 1, normal = 1, normalAmount = 3f, normalAmountParam = null, pixelAmbient = true, dyn = false, vol = false },
            ["NAT_ST_BB_Trees_02"] = new Tpl { alpha = 1, normal = 1, normalAmount = 3f, normalAmountParam = null, pixelAmbient = true, dyn = false, vol = false },
            ["Template_diffuse_ao_shadow"] = new Tpl { ao = 1, aoParam = "usr_ao_intensity" },
            ["Template_DF_AO_SP"] = new Tpl { ao = 1, aoParam = "usr_ao_amount", spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1 },
            ["Template_DF_SP"] = new Tpl { spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1 },
            // Bedford SC_SP PS064c..06a4: constant albedo, vertex ambient, sun-coloured Phong specular; no user textures.
            ["Template_SC_SP"] = new Tpl { diffuseSrc = 2, colorParam = "usr_solid_color", colorDefault = new Color(0.349020004f, 0.847059011f, 0.294117987f, 1f), spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1 },
            ["Template_DF_VX"] = new Tpl { diffuseSrc = 1 },
            ["Template_SX_Metal_Spec"] = new Tpl { ao = 2, dyn = false, spec = true, hardness = 12.5f, scale = 0.5f },
            ["Editor_DiffuseColor"] = new Tpl { diffuseSrc = 2, colorParam = "usr_diffuse_color", dyn = false, vol = false },
            ["SX_Kingston_Rough_White"] = new Tpl { diffuseSrc = 3, dyn = false, vol = false },
            ["Template_Eco_Frond"] = new Tpl { dyn = false, alpha = 1 },
            // lit, per-pixel ambient (colour reference texel bounce)
            ["Template_DF_AO_SP_NM"] = new Tpl { ao = 3, aoParam = "usr_ao_amount", normal = 1, pixelAmbient = true, spec = true, specSun = true, hardnessParam = "usr_specular_hardness", specTintParam = "usr_specular_color", scale = 1 },
            ["Template_Diffuse_Nrm"] = new Tpl { normal = 1, pixelAmbient = true },
            ["Template_DF_TRDifRefAlphaBlend"] = new Tpl { twoSided = true, pixelAmbient = true, dyn = false, alpha = 1 },
            ["NAT_CO_facadeTEST"] = new Tpl { normal = 2, pixelAmbient = true, dyn = false, vol = false, alpha = 1, uvScale = new Vector2(2, 1) },
            ["NAT_CO_facadeTEST2"] = new Tpl { normal = 2, pixelAmbient = true, dyn = false, vol = false, alpha = 1, uvScale = new Vector2(2, 1) },
            ["Template_Trackside_ShinyVinyl"] = new Tpl { normal = 2, twoSided = true, pixelAmbient = true, diffuseUV1 = true, spec = true, hardness = 150f, scale = 0.9f, env = 0.1f },
            ["Template_Trackside_Haybale_FNL"] = new Tpl { normal = 2, pixelAmbient = true, diffuseUV1 = true, spec = true, hardness = 7.5f, scale = 0.5f },
            ["Template_Trackside_Tent_FNL"] = new Tpl { normal = 2, pixelAmbient = true, dyn = false, diffuseUV1 = true, spec = true, hardness = 12.5f, scale = 0.25f },
            ["Template_Trackside_Banners"] = new Tpl { normal = 1, twoSided = true, pixelAmbient = true, vol = false, diffuseUV1 = true, spec = true, specSun = true, hardness = 12f, specScaleParam = "usr_specular_amount", sssParam = "usr_sss_amount" },
            ["Template_Trackside_bayle"] = new Tpl { normal = 2, pixelAmbient = true, diffuseUV1 = true, diffuseSrc = 4, spec = true, specSun = true, hardness = 12f, specScaleParam = "usr_specular_amount" },
            ["Template_WrldRefl_Alpha_NM"] = new Tpl { normal = 2, pixelAmbient = true, dyn = false, alpha = 1, envParam = "usr_reflection_amount", uvScale = new Vector2(10, 10) },
            ["Template_Color_Refl_Spec"] = new Tpl { diffuseSrc = 2, colorParam = "usr_color", pixelAmbient = true, spec = true, hardnessParam = "usr_specular_tightness", specScaleParam = "usr_specular_amount", envParam = "usr_reflection_amount" },
            ["Template_Eco_Branch_Nm"] = new Tpl { normal = 2, pixelAmbient = true },
            // unlit
            ["Template_Prelit_Map_Ctl"] = new Tpl { mode = ModePrelit, powerParam = "usr_prelit_power" },
            ["Template_Prelit"] = new Tpl { mode = ModePrelitUV1 },
            ["Template_Prelit_Stadiums"] = new Tpl { mode = ModePrelitStadiums, powerParam = "usr_brightness" },
            ["Template_Prelit_Color_Ctl"] = new Tpl { mode = ModePrelitColor, colorParam = "usr_constant_color", powerParam = "usr_prelit_power" },
            ["Template_Prelit_Background"] = new Tpl { mode = ModePrelitBackground },
            ["Template_Prelit_VXC_TR"] = new Tpl { mode = ModePrelitVertexColour },
            // UI_Compound's light poles, light boxes and board trims. Pixel program (disassembled, 00a8..0124): tex(uv0) * vertex rgb (texcoord2),
            // out = that * fog.w + fog, alpha 1. No lighting at all: the vertex colours are the baked light (these drew as lit models, black at night).
            ["Template_Prelit_VXC"] = new Tpl { mode = ModePrelitVertexColourOpaque },
            // UI_Compound's board scaffolds and light blooms (01a4..0368): tex * (sun colour * colour reference * sat(N.sun) + vertex ambient) + usr_power
            // (the power is ADDED, not a factor: 0.0465 on the scaffold, 1 on the blooms), two-sided, no shadow terms, alpha = sat(tex.a).
            ["Template_Prelit_Map_A"] = new Tpl { twoSided = true, dyn = false, vol = false, alpha = 1, emissiveParam = "usr_power" },
            // The MotoSkills tutorials' chevron walls and arrows (UI_Compound; ScrollingTextureRSData). Pixel program (disassembled from the level's shader asset, 0194..0280):
            // s0 = the HDR gradient (usr_sampler_hdr_gradient_keep_small_, A16B16G16R16F) at uv0 + gTextureOffset (c85), s1 = the chevron (usr_sampler_diffuse_texture) at uv0;
            // rgb = chevron * (1 + a (gradient - 1)) * fog.w + fog, alpha = sat((1 - smoothstep(sat((120 - d) / 100))) * a) with d the camera distance and a the chevron's
            // alpha (blended SrcAlpha / InvSrcAlpha, alpha test GREATER 0, depth write on, from the material's own states). No lighting.
            ["Template_AnimChevron_Ctrl"] = new Tpl { mode = ModeAnimChevron, alpha = 1 },
            // The challenges' chevrons (Target Practice's GL_TargetPractice; FR_Alpine / FR_Cheyenne / FR_Gibson): Template_AnimChevron_NoFadeCtrl, the same vertex program and
            // samplers, pixel program 0128..017c: the same rgb, alpha = sat(chevron.a) (no distance fade). States (GL_TargetPractice): cull none, SrcAlpha / InvSrcAlpha, alpha test
            // GREATER 0, depth write on.
            ["Template_AnimChevron_NoFadeCtrl"] = new Tpl { mode = ModeAnimChevronNoFade, alpha = 1 },
            ["SX_KI_LightBeam"] = new Tpl { mode = ModeLightBeam },
            // the same programs (vs / ps bytecode and constant tables identical to SX_KI_LightBeam; WP-8, _re/notes/props.md)
            ["SX_MAN_B_LightBeam"] = new Tpl { mode = ModeLightBeam }, ["SX_FD_LightBeam"] = new Tpl { mode = ModeLightBeam },
            ["SX_SP_LightBeam"] = new Tpl { mode = ModeLightBeam }, ["SX_DLC_FY_LightBeam"] = new Tpl { mode = ModeLightBeam },
            // Same complete shader resource as SX_DLC_FY_LightBeam; scrolling dust planes across warehouse openings.
            ["SX_DLC_FY_Light_Grad"] = new Tpl { mode = ModeLightBeam },
        };

        static float W(ReflexLevelContent.LevelMaterial m, string name, float def)
        {
            return name != null && m.parameters.TryGetValue(name, out var v) && v != null && v.Length > 0 ? v[v.Length > 3 ? 3 : v.Length - 1] : def;
        }
        static Color C(ReflexLevelContent.LevelMaterial m, string name, Color def)
        {
            return name != null && m.parameters.TryGetValue(name, out var v) && v != null && v.Length >= 3 ? new Color(v[0], v[1], v[2], v.Length > 3 ? v[3] : 1) : def;
        }

        static bool Token(string s, params string[] tokens) => s.Split('_').Any(t => tokens.Contains(t));


        public static Material Create(ReflexLevelContent content, ReflexLevelContent.LevelMaterial lm, ReflexLevelContent.SceneryShaders shaders,
                                      Func<string, Dictionary<int, float>, Texture2D> textureFor, Func<string, Cubemap> cubeFor = null)
        {
            if (!shaders.scenery) return null;
            var m = new Material(shaders.scenery) { name = lm.name, hideFlags = HideFlags.DontSave };
            string ps = lm.pixelShader ?? "";
            var info = lm.shader;
            Templates.TryGetValue(ps, out var tpl);
            int mode = tpl?.mode ?? ModeLit;
            if (ps == "FR_AL_Rock2") mode = ModeRock;
            else if (ps == "FR_AL_EoW_Base_PreLit") mode = ModeEdge;
            else if (ps == "FR_AL_EoW_Base_PreLit_Alpha") mode = ModeEdgeAlpha;
            else if (ps == "FR_AL_EoW_RivBirchLrg") mode = ModePrelitVertex;
            else if (ps == "Template_ATV_MX_Engine_FNL") mode = ModeVehicle;
            m.SetFloat("_Mode", mode);
            // MX vs ATV Alive: the T_*VColor programs multiply the texel by the vertex colour before squaring (ReflexCommon RfxAliveAlbedo)
            m.SetFloat("_AliveVColor", AliveLevels.Is(content.level) && ps.IndexOf("VColor", StringComparison.Ordinal) >= 0 ? 1f : 0f);

            // texture slots <-> the pixel shader's user samplers, in register order. Named samplers (usr_sampler_ao_map,
            // usr_sampler_normal_map, usr_sampler_diffuse_texture, usr_sampler_combo_map ...) bind by role; the unnamed s_samplerN of the
            // few shaders that have them bind as that shader uses them; cube textures are the reflection map.
            Texture2D diffuse = null, ao = null, normal = null, detail = null, combo = null, decalNormal = null; Cubemap cube = null;
            var unnamed = new List<(int slot, Texture2D tex)>();
            for (int i = 0; i < lm.textures.Count; i++)
            {
                string sampler = (i < lm.userSamplers.Length ? lm.userSamplers[i] : "").ToLowerInvariant();
                if (cubeFor != null && !cube) { var c = cubeFor(lm.textures[i]); if (c) { cube = c; continue; } }
                var t = textureFor(lm.textures[i], lm.samplerStates[i]);
                if (!t) continue;
                if (tpl?.slots != null)           // the program's own register roles (w-fsshaders templates)
                {
                    switch (i < tpl.slots.Length ? tpl.slots[i] : null)
                    {
                        case "diffuse": diffuse = t; break;
                        case "ao": ao = t; break;
                        case "normal": normal = t; break;
                        case "detail": detail = t; break;
                        case "combo": combo = t; break;
                    }
                    continue;
                }
                if (mode == ModeVertexBlendCliff) // original s0 AO, s1 blend target, s2 base, s3 normal
                {
                    if (i == 0) ao = t; else if (i == 1) detail = t; else if (i == 2) diffuse = t; else if (i == 3) normal = t;
                    continue;
                }
                if (mode == ModeX2Overlay)        // s0 AO (UV1), s1 the second diffuse (UV1), s2 its mask (UV1), s3 the base (UV0)
                {
                    if (i == 0) ao = t; else if (i == 1) detail = t; else if (i == 2) combo = t; else if (i == 3) diffuse = t;
                    continue;
                }
                if (mode == ModeFmxRamp)          // s0 the chevrons (UV1), s1 the ramp texture (UV0), s2 (or s1 when there are two) the alpha (UV0)
                {
                    if (i == 0) combo = t; else if (i == 1) { diffuse = t; if (lm.textures.Count < 3) detail = t; } else if (i == 2) detail = t;
                    continue;
                }
                if (mode == ModeFmxRampTopDover)  // s0 x s1, both at UV0
                {
                    if (i == 0) detail = t; else if (i == 1) diffuse = t;
                    continue;
                }
                if (sampler.Contains("combo")) combo = t;
                else if (Token(sampler, "ao", "occlusion")) ao = t;
                else if (sampler.Contains("prelit_color_map") || sampler.Contains("prelit_light_ao")) ao = t;            // the prelit map on UV1
                else if (sampler.Contains("normal")) normal = t;
                else if (sampler.Contains("mud_alpha") || sampler.Contains("alpha_texture") || sampler.Contains("transperancy")) detail = t;
                else if (sampler.Contains("diffuse") || sampler.Contains("banner") || sampler.Contains("base") || sampler.Contains("haybale") || sampler.Contains("prelit_texture")) diffuse = diffuse ? diffuse : t;
                else unnamed.Add((i, t));
            }
            if (mode == ModeLightBeam)   // s_sampler0 dust (scrolling), s_sampler1 view gradient, s_sampler2 beam mask
            {
                foreach (var (slot, t) in unnamed) { if (slot == 0) diffuse = t; else if (slot == 1) ao = t; else if (slot == 2) detail = t; }
                unnamed.Clear();
            }
            if (mode == ModeAnimChevron || mode == ModeAnimChevronNoFade)   // s0 the HDR gradient (_AOMap), s1 the chevron (the diffuse)
            {
                foreach (var (slot, t) in unnamed) { if (slot == 0) ao = t; else if (!diffuse) diffuse = t; }
                unnamed.Clear();
            }
            if (mode == ModeRenneEdge) // s0 overlay control, s1 light dirt, s2 track dirt, s3 normal
            {
                foreach (var (slot, t) in unnamed)
                {
                    if (slot == 0) combo = t; else if (slot == 1) diffuse = t;
                    else if (slot == 2) detail = t; else if (slot == 3) normal = t;
                }
                unnamed.Clear();
            }
            if (mode == ModeGibsonCliff) // s0 detail diffuse, s1 decal diffuse, s2 detail normal, s3 decal normal
            {
                foreach (var (slot, t) in unnamed)
                {
                    if (slot == 0) diffuse = t; else if (slot == 1) detail = t;
                    else if (slot == 2) normal = t; else if (slot == 3) decalNormal = t;
                }
                unnamed.Clear();
            }
            foreach (var (slot, t) in unnamed)
            {
                if (mode == ModeEdge || mode == ModeEdgeAlpha || mode == ModeVehicle) { if (slot == 0 && !detail) detail = t; else if (!diffuse) diffuse = t; }
                else if (mode == ModeRock) { if (slot == 0) diffuse = t; else normal = t; }
                else if (!diffuse) diffuse = t;
                else if (!normal) normal = t;
            }
            if (!diffuse && lm.textures.Count > 0 && info == null)       // shader information missing: the texture that is not a map by name
            {
                string pick = lm.textures.FirstOrDefault(n => !System.Text.RegularExpressions.Regex.IsMatch(n, "(_NML?\\d*$|_NRM|Reflection|Noise|_AO$|_SFX|Editor_Default|LightMap|LM\\d*$)", System.Text.RegularExpressions.RegexOptions.IgnoreCase));
                if (pick != null) diffuse = textureFor(pick, lm.samplerStates[lm.textures.IndexOf(pick)]);
            }
            float aliveTile = 0f;
            if (AliveLevels.Is(content.level) && lm.textures.Contains("FR_HI_Rock_Detail"))
            {
                // Alive rock programs (FR_HI_*Rock*_NoTemp, Xbox microcode, not translated): s0 baked occlusion on the unique UV0
                // unwrap (the converter gives the PC programs' UV1 those coordinates), s1 the tiling rock detail, s2 the tag ramp
                // (not drawn), s3 the detail normal map. The detail and its normal map tile over UV0 by the program's own literal
                // (AliveLevels.RockTile: the level's rock-detail sidecar, 10 or 8); FR_HI_Rock_NoTemp (box-projected UV0 of ~100+ feet per unit,
                // no literal, its s0 the detail's own greyscale) tiles 3x, about the same texel density on screen.
                int baseSlot = lm.textures.IndexOf("FR_HI_Rock_Detail");
                diffuse = textureFor(lm.textures[baseSlot], lm.samplerStates[baseSlot]);
                ao = lm.textures[0] == "FR_HI_Rock_Detail_AO" ? null : textureFor(lm.textures[0], lm.samplerStates[0]);
                int nrm = lm.textures.IndexOf("FR_HI_Rock_Detail_NRM");
                normal = nrm >= 0 ? textureFor(lm.textures[nrm], lm.samplerStates[nrm]) : null;
                aliveTile = AliveLevels.RockTile(content.level, lm.pixelShader) ?? 3f;
            }
            // MX vs ATV Alive: a level's own pixel program (cliffs, outcrops, end-of-world backdrops: no Reflex template) drawn by the
            // scenery shader's generic albedo program (_ALIVE_VM; AliveLevels.Program, the converter's alive_recipe.py)
            var aliveProgram = AliveLevels.Is(content.level) && tpl == null && aliveTile == 0f && !ps.StartsWith("T_", StringComparison.Ordinal)
                               && !ps.StartsWith("Template_", StringComparison.Ordinal) ? AliveLevels.Program(content.level, ps) : null;
            if (aliveProgram != null && aliveProgram.slots.All(s => s >= 0 && s < lm.textures.Count))
            {
                m.EnableKeyword(aliveProgram.general != 0 ? "_ALIVE_VM_GENERAL" : "_ALIVE_VM");   // separate variants (register budget)
                m.SetTexture("_AliveVMTex", AliveProgramTexture(aliveProgram));
                m.SetVector("_AliveVMInfo", new Vector4(aliveProgram.ops.Length / 8, aliveProgram.result, aliveProgram.lit ? 0 : 1, aliveProgram.alpha + 1));
                // a whole program (alive_general): general ops, its albedo already squared, an unlit emissive part (cube reflections)
                m.SetVector("_AliveVMInfo2", new Vector4(aliveProgram.general, aliveProgram.emissive, aliveProgram.normal, 0));
                if (aliveProgram.cubes != null)
                    for (int k = 0; k < aliveProgram.cubes.Length; k++)
                    {
                        int slot = aliveProgram.cubes[k];
                        var c = slot >= 0 && slot < lm.textures.Count && cubeFor != null ? cubeFor(lm.textures[slot]) : null;
                        if (c) m.SetTexture("_AliveC" + k, c);
                    }
                for (int k = 0; k < 4; k++)
                {
                    var v = aliveProgram.@params != null && k < aliveProgram.@params.Length && lm.parameters.TryGetValue(aliveProgram.@params[k], out var pv) && pv != null ? pv : null;
                    m.SetVector("_AliveP" + k, v == null ? Vector4.zero : new Vector4(v.Length > 0 ? v[0] : 0, v.Length > 1 ? v[1] : 0, v.Length > 2 ? v[2] : 0, v.Length > 3 ? v[3] : 0));
                }
                // the program's textures share two sampler states (the shader's sampler registers are limited): clamp where the
                // material's sampler clamps (bit k of _AliveWrap), else wrap
                int clampMask = 0;
                for (int k = 0; k < aliveProgram.slots.Length; k++)
                {
                    int slot = aliveProgram.slots[k];
                    var t = textureFor(lm.textures[slot], lm.samplerStates[slot]);
                    if (t) { m.SetTexture("_AliveT" + k, t); if (t.wrapModeU == TextureWrapMode.Clamp) clampMask |= 1 << k; }
                }
                m.SetFloat("_AliveWrap", clampMask);
                m.SetFloat("_AliveVColor", 0f);
                normal = null; ao = null; combo = null;
            }
            if (combo) m.SetTexture("_ComboMap", combo);
            if (lm.parameters.TryGetValue("usr_mud_color", out var mc) && mc != null) m.SetVector("_MudColor", new Vector4(mc[0], mc[1], mc[2], 1));
            m.SetFloat("_MudAmount", W(lm, "usr_mud_amount", 0f));
            if (diffuse) m.SetTexture("_BaseMap", diffuse);
            if (detail) m.SetTexture("_DetailMap", detail);
            if (ao) m.SetTexture("_AOMap", ao);
            if (normal) m.SetTexture("_NormalMap", normal);
            if (decalNormal) m.SetTexture("_DecalNormalMap", decalNormal);
            if (cube) m.SetTexture("_EnvCube", cube);
            if (!diffuse && mode != ModeEdge && mode != ModeEdgeAlpha) m.SetVector("_BaseColor", new Vector4(0.5f, 0.5f, 0.5f, 1));

            bool has(string c) => info != null && info.Has(c);
            bool sampler_(string c) => info != null && info.samplers.Values.Contains(c);
            float hardness = 0, scale = 0, sunColour = 1, sss = 0, env = 0; Vector4 specColor = Vector4.one; Vector2 uvScale = Vector2.one;
            int envMask = 0, envCube = 0;   // _EnvParams.zw: the mask (w-fsshaders), the cube (0 the material's at LOD 0, 1 the material's at the program's bias, 2 the level's s14)
            if (tpl != null)
            {
                m.SetFloat("_DiffuseSrc", tpl.diffuseSrc);
                m.SetFloat("_DiffuseUV", tpl.diffuseUV1 ? 1 : 0);
                m.SetFloat("_AOMode", ao ? tpl.ao : 0);
                m.SetFloat("_AOAmount", W(lm, tpl.aoParam, 1f));
                m.SetFloat("_NormalMode", normal ? tpl.normal : 0);
                m.SetFloat("_NormalUV", tpl.normalUV1 ? 1 : 0);
                m.SetFloat("_NormalAmount", W(lm, tpl.normalAmountParam, tpl.normalAmount));
                m.SetFloat("_TwoSided", tpl.twoSided ? 1 : 0);
                m.SetFloat("_AmbientSrc", tpl.pixelAmbient ? 1 : 0);
                m.SetFloat("_UseDynShadow", tpl.dyn ? 1 : 0);
                m.SetFloat("_UseVolShadow", tpl.vol ? 1 : 0);
                m.SetFloat("_ShadowLit", tpl.dyn || tpl.vol ? 1 : 0);
                m.SetFloat("_DiffuseFade", W(lm, tpl.fadeParam, 1f));
                m.SetFloat("_AlphaSource", tpl.alpha);
                // raw (SetVector / Vector property: no gamma conversion); a literal constant colour without a parameter (FMX_Dover_RoughGrey ...)
                if (tpl.colorParam != null || tpl.diffuseSrc == 2 || tpl.diffuseSrc == 5 || tpl.diffuseSrc == 6) m.SetVector("_BaseColor", C(lm, tpl.colorParam, tpl.colorDefault));
                m.SetFloat("_PrelitPower", W(lm, tpl.powerParam, tpl.power));
                m.SetFloat("_EmissiveAdd", tpl.emissiveParam != null ? W(lm, tpl.emissiveParam, 0f) : 0f);
                m.SetVector("_FsParams", new Vector4(tpl.vcol, tpl.aoUV0 ? 1 : 0, tpl.specMap, tpl.alphaParam != null ? W(lm, tpl.alphaParam, tpl.alphaDefault) : tpl.alphaDefault));
                float envBias = 0f;
                if (tpl.envSharpParam != null)
                {
                    // texldb's bias: cmp (s - 90) >= 0 ? 5.55e-17 : 30 rcp(max(s, 1)) - 1/3
                    float s = Mathf.Max(W(lm, tpl.envSharpParam, tpl.envSharpDefault), 1f);
                    envBias = s - 90f >= 0f ? 0f : 30f / s - 0.333333343f;
                }
                m.SetVector("_FsParams2", new Vector4(tpl.twoTone, tpl.uvMatrix != new Vector4(1, 0, 0, 1) ? 1 : 0, envBias, tpl.envNoWeight ? 1 : 0));
                m.SetVector("_UVMatrix", tpl.uvMatrix);
                m.SetVector("_FsParams3", new Vector4(tpl.selfLit, tpl.noRad ? 1 : 0, 0, 0));
                if (tpl.envTintParam != null || tpl.colorParam2 != null)
                {
                    var c2 = C(lm, tpl.envTintParam ?? tpl.colorParam2, tpl.color2Default);
                    m.SetVector("_FsColor", new Vector4(c2.r, c2.g, c2.b, c2.a));
                }
                if (tpl.spec)
                {
                    hardness = tpl.hardnessParam != null ? W(lm, tpl.hardnessParam, tpl.hardness > 0f ? tpl.hardness : 5f) : tpl.hardness;
                    scale = tpl.specScaleParam != null ? W(lm, tpl.specScaleParam, 0f) : tpl.scale;
                    sunColour = tpl.specSun ? 1 : 0;
                    if (tpl.specTintParam != null) { var c = C(lm, tpl.specTintParam, Color.white); specColor = new Vector4(c.r, c.g, c.b, 1); }
                }
                if (tpl.sssParam != null) sss = W(lm, tpl.sssParam, 0f);
                env = tpl.envParam != null ? W(lm, tpl.envParam, tpl.env) : tpl.env;
                envMask = tpl.envMask; envCube = tpl.envGlobal ? 2 : tpl.slots != null ? 1 : 0;
                uvScale = tpl.uvScale;
            }
            else
            {
                // not transcribed: what the program declares
                m.SetFloat("_AmbientSrc", has("gBackLight") ? 1 : 0);
                m.SetFloat("_UseVolShadow", sampler_("samplerVolumeShadowMap") ? 1 : 0);
                m.SetFloat("_UseDynShadow", sampler_("samplerDynamicShadowMap") ? 1 : 0);
                m.SetFloat("_DiffuseSrc", ps == "Template_DF_VXC_AO_NM" ? 0 : 0);
                m.SetFloat("_AOMode", !ao ? 0 : ps == "Template_DF_VXC_AO_NM" ? 2 : 1);
                m.SetFloat("_AOAmount", ao ? W(lm, "usr_ao_amount", W(lm, "usr_ao_intensity", 1f)) : 1f);
                m.SetFloat("_NormalMode", normal ? 1 : 0);
                m.SetFloat("_NormalAmount", W(lm, "usr_normal_amount", 1f));
                m.SetFloat("_TwoSided", 1);
                m.SetFloat("_DiffuseFade", W(lm, "usr_diffuse_fade", 1f));
                m.SetFloat("_PrelitPower", W(lm, "usr_prelit_power", 1f));
                // SkyCraneBlurBlades (Cheyenne's sky crane rotor blur, FR_CH_SkycranePropBlur: alpha blend on): its pixel program ends
                // `mov_sat_pp oC0.w, r1.w` with r1 = texld s0 (usr_sampler_diffuse_texture): the diffuse texture's alpha
                // a program that writes the diffuse texture's alpha (oC0.w <- texld s<k>.w, found by ReflexLevelContent.ScanAlphaSampler) while
                // the material tests / blends on alpha: the per-level copies of the tree programs (Bedford's NAT_BE_LeaningTree_Needles drew
                // every card whole: alpha 1 passed the alpha test)
                bool alphaFromDiffuse = false, aliveAlphaSlot = false;
                if (info != null && info.alphaSampler >= 0 && (lm.alphaTest || lm.alphaBlend) && diffuse)
                {
                    var regs = info.samplers.Where(kv => !(kv.Value.StartsWith("sampler", StringComparison.Ordinal) && kv.Value.Length > 7 && char.IsUpper(kv.Value[7])))
                                            .OrderBy(kv => kv.Key).Select(kv => kv.Key).ToList();
                    int slot = regs.IndexOf(info.alphaSampler);
                    alphaFromDiffuse = slot >= 0 && slot < lm.textures.Count && textureFor(lm.textures[slot], lm.samplerStates[slot]) == diffuse;
                    // MX vs ATV Alive programs with a separate alpha texture (FR_JSC_Fence_*_NoTemp, Template_Diffuse_Alpha: s1): its
                    // alpha through the detail slot (_AlphaSource 3)
                    if (!alphaFromDiffuse && slot >= 0 && slot < lm.textures.Count && AliveLevels.Is(content.level) && !detail)
                    {
                        detail = textureFor(lm.textures[slot], lm.samplerStates[slot]);
                        if (detail) aliveAlphaSlot = true;
                    }
                }
                m.SetFloat("_AlphaSource", mode == ModeEdgeAlpha ? 2 : aliveAlphaSlot ? 3 : (mode == ModePrelitVertex || ps.StartsWith("Template_DF_TRDif", StringComparison.Ordinal) || ps == "SkyCraneBlurBlades" || alphaFromDiffuse) ? 1 : 0);
                if (aliveAlphaSlot) m.SetTexture("_DetailMap", detail);
                if (lm.parameters.ContainsKey("usr_specular_hardness"))
                {
                    hardness = W(lm, "usr_specular_hardness", 5f); scale = 1;
                    if (lm.parameters.TryGetValue("usr_specular_color", out var c) && c != null) specColor = new Vector4(c[0], c[1], c[2], 1);
                }
            }
            if (mode == ModeEdgeAlpha) m.SetFloat("_AlphaSource", 2);
            if (mode == ModePrelitVertex) m.SetFloat("_AlphaSource", 1);
            m.SetVector("_SpecParams", new Vector4(hardness, scale, sunColour, scale != 0 ? 1 : 0));
            m.SetVector("_SpecTint", specColor);
            m.SetVector("_EnvParams", new Vector4(env, env != 0 && (cube || envCube == 2) ? 1 : 0, envMask, envCube));
            m.SetFloat("_SSS", sss);
            m.SetVector("_UVScale", aliveTile > 0f ? new Vector4(aliveTile, aliveTile, 0, 0) : new Vector4(uvScale.x, uvScale.y, 0, 0));

            // render states
            m.SetFloat("_Cull", lm.cull == 1 ? (float)CullMode.Off : lm.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);   // D3DCULL: 1 none, 2 CW, 3 CCW (Unity front faces are clockwise)
            m.SetFloat("_AlphaTest", lm.alphaTest ? 1 : 0);
            if (lm.alphaTest) m.EnableKeyword("_ALPHATEST_ON"); else m.DisableKeyword("_ALPHATEST_ON");   // the shadow / depth passes clip only then
            m.SetFloat("_Cutoff", Mathf.Clamp01(lm.alphaRef / 255f));
            m.SetFloat("_AlphaFuncGE", lm.alphaFunc == 7 ? 1 : 0);
            bool blend = lm.alphaBlend || mode == ModeEdgeAlpha;
            m.SetFloat("_SrcBlend", blend ? ReflexLevelContentBlend(lm.srcBlend) : (float)BlendMode.One);
            m.SetFloat("_DstBlend", blend ? ReflexLevelContentBlend(lm.dstBlend) : (float)BlendMode.Zero);
            m.SetFloat("_ZWrite", lm.zWrite ? 1 : 0);
            if (blend) { m.renderQueue = (int)RenderQueue.Transparent; m.SetOverrideTag("RenderType", "Transparent"); }
            else if (lm.alphaTest) { m.renderQueue = (int)RenderQueue.AlphaTest; m.SetOverrideTag("RenderType", "TransparentCutout"); }
            return m;
        }

        static readonly Dictionary<AliveLevels.AlbedoProgram, Texture2D> s_aliveProgramTextures = new Dictionary<AliveLevels.AlbedoProgram, Texture2D>();

        /// The ops of an Alive albedo program as the texture the shader reads them from (RGBAFloat, two texels per op).
        static Texture2D AliveProgramTexture(AliveLevels.AlbedoProgram p)
        {
            lock (s_aliveProgramTextures)
            {
                if (s_aliveProgramTextures.TryGetValue(p, out var t) && t) return t;
                int n = p.ops.Length / 8;
                t = new Texture2D(2 * n, 1, TextureFormat.RGBAFloat, false, true) { name = "Alive program " + p.shader, filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
                var px = new Color[2 * n];
                for (int k = 0; k < n; k++)
                {
                    px[2 * k] = new Color(p.ops[8 * k], p.ops[8 * k + 1], p.ops[8 * k + 2], p.ops[8 * k + 3]);
                    px[2 * k + 1] = new Color(p.ops[8 * k + 4], p.ops[8 * k + 5], p.ops[8 * k + 6], p.ops[8 * k + 7]);
                }
                t.SetPixels(px); t.Apply(false, true);
                s_aliveProgramTextures[p] = t;
                return t;
            }
        }

        static float ReflexLevelContentBlend(int d3d)
        {
            switch (d3d)
            {
                case 1: return (float)BlendMode.Zero; case 2: return (float)BlendMode.One; case 3: return (float)BlendMode.SrcColor; case 4: return (float)BlendMode.OneMinusSrcColor;
                case 5: return (float)BlendMode.SrcAlpha; case 6: return (float)BlendMode.OneMinusSrcAlpha; case 7: return (float)BlendMode.DstAlpha; case 8: return (float)BlendMode.OneMinusDstAlpha;
                case 9: return (float)BlendMode.DstColor; case 10: return (float)BlendMode.OneMinusDstColor; default: return (float)BlendMode.One;
            }
        }
    }
}
