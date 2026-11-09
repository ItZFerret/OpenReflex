// Reflex/Vehicle: the original object shading of MX vs ATV Reflex - every vehicle (MX, ATV, UTV, Super Buggy, Pro Lite / Pro 2 trucks),
// the rider's gear and body, the ambient characters and crowds (Template_Crowd_*) - 1:1 with the original renderer
// (_re/notes/object-shading.md). The game gives each material a shader asset with a static vertex program, a skinned one and a pixel
// program (SM3); Shaders/ReflexObjectPrograms.hlsl holds a port of each static vertex program and each pixel program, instruction
// for instruction (generated from their disassembly by the object-shading tools, verified against the software-rasterized original:
// the replay test renders the recorded draws of the original's frames with these programs). _Program selects the material's pair
// (Content/ReflexObjectShading.cs: the program table, the material factory). Unity skins the skinned meshes, so the static vertex
// program with gWorld = the renderer's matrix gives what the skinned one gives.
// Inputs (Shaders/ReflexObjectShading.hlsl): the level globals of the original renderer (ReflexCommon.hlsl: the four-light rig, sun,
// bounce, fog, colour reference, volume shadow map), the level's reflection cube, the dynamic shadow (the original's VSM maps when
// _RfxVsmOn, else Unity's main-light shadow through the programs' own Chebyshev term), the material's textures in sampler order
// (_Tex0.._Tex6, _Cube3 / _Cube4) and its constants by their CTAB names (usr_*, gTextureOffset: the material parameters over the
// program's defaults; the game's run-time values - mud, belt scroll, flywheel fade, damage - are set on the materials as the game sets
// the constants). Render states are the material's (cull, z write, alpha test with ALPHAREF / ALPHAFUNC, blend).
// Output: raw values for ReflexRenderPipeline's FP16 target (ReflexCommon.hlsl RfxOutput).
Shader "Reflex/Vehicle"
{
    Properties
    {
        _Program("Program (ReflexObjectShading.Programs)", Float) = 0
        [NoScaleOffset] _Tex0("s0", 2D) = "black" {}
        [NoScaleOffset] _Tex1("s1", 2D) = "black" {}
        [NoScaleOffset] _Tex2("s2", 2D) = "black" {}
        [NoScaleOffset] _Tex3("s3", 2D) = "black" {}
        [NoScaleOffset] _Tex4("s4", 2D) = "black" {}
        [NoScaleOffset] _Tex5("s5", 2D) = "black" {}
        [NoScaleOffset] _Tex6("s6", 2D) = "black" {}
        [NoScaleOffset] _Cube3("s3 (cube)", Cube) = "black" {}
        [NoScaleOffset] _Cube4("s4 (cube)", Cube) = "black" {}
        [HideInInspector] usr_mud_amount("usr_mud_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_mud_color("usr_mud_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_mud_wetness("usr_mud_wetness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_reflection_amount("usr_reflection_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_plastic_color("usr_plastic_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specularcolor_blend8_source_constant("usr_specularcolor_blend8_source_constant", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_diffuse_color("usr_diffuse_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specularcolor_blend4_source_constant("usr_specularcolor_blend4_source_constant", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_lens_color("usr_lens_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_opacity("usr_opacity", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_far_color("usr_far_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_near_color("usr_near_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_fade_value("usr_fade_value", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_falloff_amount("usr_falloff_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_falloff_tightness("usr_falloff_tightness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_frame_color("usr_frame_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specular_amount("usr_specular_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specular_tightness("usr_specular_tightness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_sss_color("usr_sss_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_anisotropic_direction("usr_anisotropic_direction", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_color("usr_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_metal_color("usr_metal_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_boots_specular_amount("usr_boots_specular_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_earring_color("usr_earring_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_earring_specular("usr_earring_specular", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_skin_specular("usr_skin_specular", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specular_color("usr_specular_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_pants_specular_amount("usr_pants_specular_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_fallof_color("usr_fallof_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_specularcolor_blend2_amount_constant("usr_specularcolor_blend2_amount_constant", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_shine_amount("usr_shine_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_shine_tightness("usr_shine_tightness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_falloffbrightness("usr_falloffbrightness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_fallofftightness("usr_fallofftightness", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_mx_hub_color("usr_mx_hub_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_backbrightness_white_only_("usr_backbrightness_white_only_", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_damage("usr_damage", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_scratch_color("usr_scratch_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_damage_amount("usr_damage_amount", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_damage_color("usr_damage_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_damage_scratches_("usr_damage_scratches_", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_frame_tint("usr_frame_tint", Vector) = (0, 0, 0, 0)
        [HideInInspector] usr_wheel_color("usr_wheel_color", Vector) = (0, 0, 0, 0)
        [HideInInspector] gTextureOffset("gTextureOffset", Vector) = (0, 0, 0, 0)
        // MATERIALS: ENHANCED / MUD BUILDUP (ReflexVehiclePBR.hlsl; Content/ReflexMaterialClasses.cs, ReflexDirt.cs): the material's class and the vehicle's dirt
        [HideInInspector] _RfxMat("class, roughness bias", Vector) = (0, 0, 0, 0)
        [HideInInspector] _RfxDirt("amount, wetness, model units per metre, seed", Vector) = (0, 0, 1, 0)
        [HideInInspector] _RfxDirtColor("mud colour", Vector) = (0.25, 0.2, 0.15, 1)
        [HideInInspector] _RfxDirtF("the vehicle's forward (world)", Vector) = (0, 0, 1, 0)
        _AlphaTest("Alpha test (D3DRS_ALPHATESTENABLE)", Float) = 0
        _AlphaRef("ALPHAREF (0..255)", Float) = 0
        _AlphaFunc("ALPHAFUNC (D3DCMPFUNC)", Float) = 8
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 1
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 0
        [Enum(Off, 0, On, 1)] _ZWrite("ZWrite", Float) = 1
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Geometry" }
        HLSLINCLUDE
        // shared by the forward pass and the depth pass (the program library: the depth pass runs the material's program for its alpha test)
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"
        #include "ReflexShadows.hlsl"

        CBUFFER_START(UnityPerMaterial)
            float _Program;
            float4 usr_mud_amount;
            float4 usr_mud_color;
            float4 usr_mud_wetness;
            float4 usr_reflection_amount;
            float4 usr_plastic_color;
            float4 usr_specularcolor_blend8_source_constant;
            float4 usr_diffuse_color;
            float4 usr_specularcolor_blend4_source_constant;
            float4 usr_lens_color;
            float4 usr_opacity;
            float4 usr_far_color;
            float4 usr_near_color;
            float4 usr_fade_value;
            float4 usr_falloff_amount;
            float4 usr_falloff_tightness;
            float4 usr_frame_color;
            float4 usr_specular_amount;
            float4 usr_specular_tightness;
            float4 usr_sss_color;
            float4 usr_anisotropic_direction;
            float4 usr_color;
            float4 usr_metal_color;
            float4 usr_boots_specular_amount;
            float4 usr_earring_color;
            float4 usr_earring_specular;
            float4 usr_skin_specular;
            float4 usr_specular_color;
            float4 usr_pants_specular_amount;
            float4 usr_fallof_color;
            float4 usr_specularcolor_blend2_amount_constant;
            float4 usr_shine_amount;
            float4 usr_shine_tightness;
            float4 usr_falloffbrightness;
            float4 usr_fallofftightness;
            float4 usr_mx_hub_color;
            float4 usr_backbrightness_white_only_;
            float4 usr_damage;
            float4 usr_scratch_color;
            float4 usr_damage_amount;
            float4 usr_damage_color;
            float4 usr_damage_scratches_;
            float4 usr_frame_tint;
            float4 usr_wheel_color;
            float4 gTextureOffset;
            float4 _RfxMat, _RfxDirt, _RfxDirtColor, _RfxDirtF;
            float _AlphaTest, _AlphaRef, _AlphaFunc, _SrcBlend, _DstBlend;
        CBUFFER_END
        TEXTURE2D(_Tex0); SAMPLER(sampler_Tex0); TEXTURE2D(_Tex1); SAMPLER(sampler_Tex1); TEXTURE2D(_Tex2); SAMPLER(sampler_Tex2);
        TEXTURE2D(_Tex3); SAMPLER(sampler_Tex3); TEXTURE2D(_Tex4); SAMPLER(sampler_Tex4); TEXTURE2D(_Tex5); SAMPLER(sampler_Tex5);
        TEXTURE2D(_Tex6); SAMPLER(sampler_Tex6);
        TEXTURECUBE(_Cube3); SAMPLER(sampler_Cube3); TEXTURECUBE(_Cube4); SAMPLER(sampler_Cube4);

        #include "ReflexObjectShading.hlsl"
        #include "ReflexObjectPrograms.hlsl"

        struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float4 tangentOS : TANGENT; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float3 restPos : TEXCOORD2; };

        RfxV2F Vert(Attributes a)
        {
            RfxVsIn v;
            v.position = float4(a.positionOS.xyz, 1);
            v.normal = float4(a.normalOS, 1);
            v.tangent = float4(a.tangentOS.xyz, 1);
            v.uv0 = float4(a.uv0.x, 1.0 - a.uv0.y, 0, 1);   // D3D texture coordinates (the meshes hold (u, 1 - v))
            v.uv1 = float4(a.uv1.x, 1.0 - a.uv1.y, 0, 1);
            v.color = 1;
            RfxV2F o = (RfxV2F)0;
            RfxObjectVS((int)_Program, v, o);
            // MATERIALS: ENHANCED: the world normal and the rest-pose position (uv3: the mesh as built; skinning leaves it alone)
            o.t9 = float4(normalize(mul(a.normalOS, (float3x3)unity_WorldToObject)), 0);
            o.t10 = float4(a.restPos, 0);
            return o;
        }

        // D3DRS_ALPHAFUNC on the 8-bit alpha (D3DCMP_NEVER 1 .. ALWAYS 8), as the rasterizer compares round(a * 255) with ALPHAREF
        bool AlphaPasses(float a)
        {
            float a8 = floor(saturate(a) * 255.0 + 0.5), r = _AlphaRef;
            int f = (int)_AlphaFunc;
            return f == 1 ? false : f == 2 ? a8 < r : f == 3 ? a8 == r : f == 4 ? a8 <= r : f == 5 ? a8 > r : f == 6 ? a8 != r : f == 7 ? a8 >= r : true;
        }

        ENDHLSL

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull [_Cull]
            ZWrite [_ZWrite]
            ZTest LEqual
            Blend [_SrcBlend] [_DstBlend]
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile_fragment _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile_fragment _ _SHADOWS_SOFT
            #pragma multi_compile_fragment _ _RFX_ENHANCED
            // one group of 16 object programs per variant (ReflexObjectShading.SetProgram: _RFXOBJ_G<_Program / 16>): the first use of a
            // variant compiles one group, not all 134 programs (the compile was the front end's 4-5 s first frame; the register budget of
            // the largest program in the library set every draw's occupancy). No keyword: the whole library (materials made elsewhere).
            #pragma multi_compile_local _ _RFXOBJ_G0 _RFXOBJ_G1 _RFXOBJ_G2 _RFXOBJ_G3 _RFXOBJ_G4 _RFXOBJ_G5 _RFXOBJ_G6 _RFXOBJ_G7 _RFXOBJ_G8
            #include "ReflexEnhanced.hlsl"
            #include "ReflexVehiclePBR.hlsl"
            #define RFX_VOL_NO_SAMPLER   // (the program library and ENHANCED use all 16 samplers)
            #include "ReflexVolumetricApply.hlsl"

            // The front end's 3D menu (OpenFrontEnd/OpenMenuWorld; not part of the original renderer): a key spotlight on the rider and the vehicle
            // and a cool rim on their edges, over the original program's result (its colours scaled, not replaced). Globals; gain 0 = off (races,
            // the garage picture, everything but the menu world).
            float4 _RfxMenuSpotPos;    // xyz: the light (world), w: the key's gain (0: off)
            float4 _RfxMenuSpotDir;    // xyz: its direction (unit), w: cos of the cone's outer half angle
            float4 _RfxMenuSpotCone;   // x: cos of the inner half angle, y: range (ft), z: wrap, w: the rim's gain
            float4 _RfxMenuRimColor;   // rgb: the rim (raw scene values)
            // n: the interpolated vertex normal (t9, world): the models' own smoothing groups (w-fb-models: the face normal of the screen-space
            // derivatives it used before lit every triangle flat - the exhausts, tanks and helmets showed their polygons under the key and the rim)
            float3 RfxMenuLight(float3 col, float3 p, float3 n, bool front)
            {
                float3 toP = p - _RfxMenuSpotPos.xyz; float d = length(toP); float3 l = toP / max(d, 1e-3);
                float cone = smoothstep(_RfxMenuSpotDir.w, _RfxMenuSpotCone.x, dot(l, _RfxMenuSpotDir.xyz));
                float fall = saturate(1.0 - d / max(_RfxMenuSpotCone.y, 1.0));
                n = n / max(length(n), 1e-6);
                if (!front) n = -n;   // (a two-sided material's back face)
                float3 v = normalize(_WorldSpaceCameraPos - p);
                float wrap = _RfxMenuSpotCone.z;
                float diffuse = saturate((dot(n, -l) + wrap) / (1.0 + wrap));
                float key = _RfxMenuSpotPos.w * cone * fall * diffuse;
                float rim = pow(1.0 - saturate(dot(n, v)), 3.0) * cone;
                return col * (1.0 + key) + _RfxMenuRimColor.rgb * (rim * _RfxMenuSpotCone.w);
            }

            float4 Frag(RfxV2F i, bool face : SV_IsFrontFace) : SV_Target
            {
                rfxObjPosWS = i.t0.xyz;   // (the Alive MapShadow term of RfxVolumeShadowTexel)
                // MATERIALS: ENHANCED (_RfxMatOn: the option, with LIGHTING: ENHANCED): the material's class; the program's own environment reflection
                // is replaced by the GGX terms of ReflexVehiclePBR.hlsl
                int rfxMatClass = _RfxMatOn > 0.5 ? (int)(_RfxMat.x + 0.5) : 0;
                rfxReflScale = rfxMatClass > 0 ? RfxMatOrigReflection(rfxMatClass) : 1.0;
                if (_RfxVsmOn < 0.5)
                {
                #if defined(_MAIN_LIGHT_SHADOWS) || defined(_MAIN_LIGHT_SHADOWS_CASCADE) || defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                    #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                    float4 shadowCoord = ComputeScreenPos(TransformWorldToHClip(i.t0.xyz));
                    #else
                    float4 shadowCoord = TransformWorldToShadowCoord(i.t0.xyz);   // t0: every program's world position (o0)
                    #endif
                    rfxShadowAtten = RfxMainLightShadow(shadowCoord);
                #endif
                }
                float2 rfxOcc2 = float2(1, 1); float rfxSunShadow = rfxShadowAtten;   // (the sun's shadow alone: the new layers take the contact shadow with their own light)
            #if defined(_RFX_ENHANCED)
                // LIGHTING: ENHANCED (around the original program, which stays as it is): contact shadows into its shadow input, the AO after it
                float2 rfxOcc = RfxEnhOcclusion(i.pos); rfxOcc2 = rfxOcc;
                if (_RfxVsmOn < 0.5) rfxShadowAtten *= rfxOcc.g;
            #endif
                float4 c = RfxObjectPS((int)_Program, i, face);
            #if defined(_RFX_ENHANCED)
                c.rgb *= lerp(1.0, rfxOcc.r, _RfxEnhParams.z);
            #endif
                if (rfxMatClass > 0) c = RfxVehicleMaterial(c, rfxMatClass, i.t0.xyz, i.t9.xyz, i.t10.xyz, rfxOcc2, _RfxVsmOn < 0.5 ? rfxSunShadow : 1.0, face);
                if (_RfxMenuSpotPos.w > 0) c.rgb = RfxMenuLight(c.rgb, i.t0.xyz, i.t9.xyz, face);
                if (_AlphaTest > 0.5 && !AlphaPasses(c.a)) discard;
                // VOLUMETRIC LIGHT: a blended material (the Transparent queue: lenses, glass, the crowds' blended cards) draws after the medium's
                // composite and takes the medium in front of it here (ReflexVolumetricApply.hlsl)
                if (_SrcBlend != 1.0 || _DstBlend != 0.0) c.rgb = RfxVolApply(c.rgb, c.a, i.t0.xyz, i.pos, _SrcBlend, _DstBlend);
                return float4(RfxOutput(c.rgb), c.a);
            }
            ENDHLSL
        }
        UsePass "Universal Render Pipeline/Lit/ShadowCaster"
        // The depth pre-pass (ENHANCED's occlusion, the volumetric light's march and composite, TAA read it): an alpha-tested material
        // (ALPHATESTENABLE: the crowds' and characters' cut-out cards, hair, plates) runs its program for the alpha and the same test as the
        // forward pass, so the cut-out parts leave the depth of what is behind them (URP Lit's DepthOnly wrote the whole card: the haze,
        // the AO and the contact shadows took the card's depth around every cut-out figure). Opaque materials: the position only.
        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            Cull [_Cull]
            ZWrite On
            ColorMask R
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex DepthVert
            #pragma fragment DepthFrag
            #pragma multi_compile_local _ _RFXOBJ_G0 _RFXOBJ_G1 _RFXOBJ_G2 _RFXOBJ_G3 _RFXOBJ_G4 _RFXOBJ_G5 _RFXOBJ_G6 _RFXOBJ_G7 _RFXOBJ_G8
            RfxV2F DepthVert(Attributes a)
            {
                if (_AlphaTest > 0.5) return Vert(a);
                RfxV2F o = (RfxV2F)0;
                o.pos = TransformObjectToHClip(a.positionOS.xyz);
                return o;
            }
            half DepthFrag(RfxV2F i, bool face : SV_IsFrontFace) : SV_Target
            {
                if (_AlphaTest > 0.5)
                {
                    rfxObjPosWS = i.t0.xyz;
                    if (!AlphaPasses(RfxObjectPS((int)_Program, i, face).a)) discard;
                }
                return i.pos.z;
            }
            ENDHLSL
        }
        UsePass "Universal Render Pipeline/Lit/MotionVectors"   // per-object motion vectors (the bikes, riders, wheels): TAA (ReflexTemporalAA) and REPLAY / PHOTO MODE motion blur (ReflexCinematic)
    }
}
