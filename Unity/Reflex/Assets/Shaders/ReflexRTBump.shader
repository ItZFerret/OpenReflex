// The terrain's RT_Bump layer-1 content (rt-bump.md, render-pipeline.md): the level's HFBumpModelRSData meshes drawn top-down
// into ReflexRenderPipeline's camera-following RT_Bump slices with the original pixel programs (transcribed):
//   HFB_4tex            r = lerp(lerp(B0(uv t.x), B1(uv t.y), m.r), B2(uv t.z), m.g); out = lerp(r, B3(uv t.w), m.b)   (m = mask(uv))
//   HFB_4tex_vertalpha  HFB_4tex with out.a *= vertex alpha (FMX_BuenaVista(B))
//   HFB_1tex_vertalpha  out = (tex.rgb, tex.a * vertex alpha)
//   HFB_1tex_deformed   d = deform - 0.5 ... (0174..0264), a reveal of the bump texture where the surface was deformed
//   HFB_ch_road         mask from the terrain-size UV of the world position; z scaled by lerp(tuner, 1, mask.z), a = (1 - mask.y) tex.a v.a
// Render states of every HFB material: SRCALPHA / INVSRCALPHA, COLORWRITEENABLE = RGB, no Z, cull CCW (vertalpha: none).
// The vertex programs use only the mesh's x / z (clip = M_slice * (x, z, 1)): here the slice camera is a top-down ortho view.
Shader "Hidden/Reflex/RTBump"
{
    Properties
    {
        [NoScaleOffset] _Bump0("usr_sampler_bump_map_base", 2D) = "gray" {}
        [NoScaleOffset] _Bump1("usr_sampler_bump_map_r", 2D) = "gray" {}
        [NoScaleOffset] _Bump2("usr_sampler_bump_map_g", 2D) = "gray" {}
        [NoScaleOffset] _Bump3("usr_sampler_bump_map_b", 2D) = "gray" {}
        [NoScaleOffset] _Mask("usr_sampler_mask", 2D) = "black" {}
        _TileRate("usr_bump_map_tile_rate", Vector) = (1, 1, 1, 1)
        _Tuner("usr_dirt_grass_puffy_tuner", Float) = 1
        _TerrainSize("usr_terrain_size", Float) = 4096
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 0
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        TEXTURE2D(_Bump0); SAMPLER(sampler_Bump0);
        TEXTURE2D(_Bump1); SAMPLER(sampler_Bump1);
        TEXTURE2D(_Bump2); SAMPLER(sampler_Bump2);
        TEXTURE2D(_Bump3); SAMPLER(sampler_Bump3);
        TEXTURE2D(_Mask);  SAMPLER(sampler_Mask);
        TEXTURE2D(_RfxDeformRT); SAMPLER(sampler_RfxDeformRT);   // the live deformation surface (DeformSampler), Unity rows flipped
        float4 _RfxDeformParams;                                 // x: 1 when _RfxDeformRT is valid
        CBUFFER_START(UnityPerMaterial)
            float4 _TileRate; float _Tuner, _TerrainSize, _Cull;
        CBUFFER_END
        struct A { float4 positionOS : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR; };
        struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float4 color : TEXCOORD1; float2 xz : TEXCOORD2; };
        V Vert(A a)
        {
            V o;
            float3 w = TransformObjectToWorld(a.positionOS.xyz);
            o.positionCS = mul(UNITY_MATRIX_VP, float4(w.x, 0.0, w.z, 1.0));
            o.uv = float2(a.uv.x, 1.0 - a.uv.y);     // D3D uv (the scenery meshes carry Unity-flipped v)
            o.color = a.color; o.xz = w.xz;
            return o;
        }
        float4 S(TEXTURE2D_PARAM(t, s), float2 d3d) { return SAMPLE_TEXTURE2D(t, s, float2(d3d.x, 1.0 - d3d.y)); }
        ENDHLSL

        Pass
        {
            Name "HFB_4tex"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull [_Cull]
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 r0 = _TileRate.xxyy * i.uv.xyxy;
                float4 r1 = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), r0.xy);
                float4 b1 = S(TEXTURE2D_ARGS(_Bump1, sampler_Bump1), r0.zw);
                float4 m = S(TEXTURE2D_ARGS(_Mask, sampler_Mask), i.uv);
                float4 r3 = lerp(r1, b1, m.x);
                r0 = _TileRate.zzww * i.uv.xyxy;
                float4 g = S(TEXTURE2D_ARGS(_Bump2, sampler_Bump2), r0.xy);
                float4 b = S(TEXTURE2D_ARGS(_Bump3, sampler_Bump3), r0.zw);
                float4 r4 = lerp(r3, g, m.y);
                return m.z * (b - r4) + r4;
            }
            ENDHLSL
        }
        Pass
        {
            // HFB_4tex_vertalpha (FMX_BuenaVista(B) FMX_BV_Mainbump, ps afd9c3774ec7865d 0220..02dc): HFB_4tex with alpha * vertex alpha
            Name "HFB_4tex_vertalpha"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull [_Cull]
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 r0 = _TileRate.xxyy * i.uv.xyxy;
                float4 r1 = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), r0.xy);
                float4 b1 = S(TEXTURE2D_ARGS(_Bump1, sampler_Bump1), r0.zw);
                float4 m = S(TEXTURE2D_ARGS(_Mask, sampler_Mask), i.uv);
                float4 r3 = lerp(r1, b1, m.x);
                r0 = _TileRate.zzww * i.uv.xyxy;
                float4 g = S(TEXTURE2D_ARGS(_Bump2, sampler_Bump2), r0.xy);
                float4 b = S(TEXTURE2D_ARGS(_Bump3, sampler_Bump3), r0.zw);
                float4 r4 = lerp(r3, g, m.y);
                float4 o = m.z * (b - r4) + r4;
                return float4(o.rgb, o.a * i.color.a);
            }
            ENDHLSL
        }
        Pass
        {
            Name "HFB_1tex_vertalpha"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull [_Cull]
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 t = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), i.uv);
                return float4(t.rgb, t.a * i.color.a);
            }
            ENDHLSL
        }
        Pass
        {
            Name "MTX_Ruts"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull Off
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                float4 t = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), i.uv);
                float2 terrainUV = float2(i.xz.x, -i.xz.y) / _TerrainSize + .5;
                float4 mask = S(TEXTURE2D_ARGS(_Mask, sampler_Mask), terrainUV);
                float soil = (1.0 - mask.g) * lerp(1.0, 1.0 - mask.b, _Tuner);
                // Native rut height/tint, with shallow visual displacement. Live collision
                // deformation remains owned by the simulation, exactly as for stock bump decals.
                return float4(t.r, t.g, t.b * .25, t.a * i.color.a * soil);
            }
            ENDHLSL
        }
        Pass
        {
            Name "HFB_1tex_deformed"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull [_Cull]
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                // texld s10 (DeformSampler) at the mesh's own uv (the bump meshes carry the terrain uv)
                float d = _RfxDeformParams.x > 0.5 ? S(TEXTURE2D_ARGS(_RfxDeformRT, sampler_RfxDeformRT), i.uv).r : 0.50196081;
                float r0x = d - 0.5;
                float r0y = r0x + r0x;
                float r0z = saturate(saturate(r0y) * 4.0);
                r0y = saturate(-r0y);
                float r1w = saturate(r0y + r0z);
                r0y = saturate(r0x * -10.0);
                float r2w = min(r0y, 0.25);
                r0y = 1.0 - r2w;
                float4 r3 = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), _TileRate.xy * i.uv);
                float r2x = r0y * r3.x;
                float r2z = saturate(r3.z - r2w);
                // cmp oC0.xzw, r0.x, (r3.x, r3.z, r1.w), (r2.x, r2.z, r2.w); oC0.y = r3.y
                return r0x >= 0 ? float4(r3.x, r3.y, r3.z, r1w) : float4(r2x, r3.y, r2z, r2w);
            }
            ENDHLSL
        }
        Pass
        {
            Name "HFB_ch_road"
            Blend SrcAlpha OneMinusSrcAlpha
            ColorMask RGB
            ZTest Always ZWrite Off Cull [_Cull]
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment F
            float4 F(V i) : SV_Target
            {
                // v1 = world xz (gWorld rows): mask at (x / size + 0.5, z / -size + 0.5)
                float2 muv = float2(i.xz.x * rcp(_TerrainSize), i.xz.y * rcp(-_TerrainSize)) + 0.5;
                float4 m = S(TEXTURE2D_ARGS(_Mask, sampler_Mask), muv);
                float r2x = lerp(_Tuner, 1.0, m.z);                 // lrp r2.x, r0.z, 1, c150.x
                float4 t = S(TEXTURE2D_ARGS(_Bump0, sampler_Bump0), i.uv);
                return float4(t.x, t.y, r2x * t.z, (1.0 - m.y) * (t.w * i.color.a));
            }
            ENDHLSL
        }
    }
    Fallback Off
}
