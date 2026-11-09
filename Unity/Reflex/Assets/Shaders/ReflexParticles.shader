// Hand port of the original particle programs (EngineDependencies shader "PyroParticles": UncompressedNoLightVS + NoLightPS /
// NoLightNoSoftParticlesPS, disassembled with _re/formats/d3d9_shader.py), drawing the running game's own particles
// (reflex_world_particles, native/drafts/world_runtime/particles.cpp) from a structured buffer: one quad per particle, the
// original instanced draw (stream 0 = corner id 0..3, index buffer 0 1 2 0 2 3) done with SV_VertexID.
//   corner c = (-1,1) (1,1) (1,-1) (-1,-1), uv = c * (0.5, -0.5) + 0.5 -> uv * frame.xy + frame.zw (D3D v down)
//   X = size.x * scale * (c.x - pivot.x), Y = size.y * scale * (c.y - pivot.y), r = wrap(rotation)
//   view offset = (X cos r + Y sin r, Y cos r - X sin r) added to the particle's view-space position (camera-facing quad)
//   colour = color0 * color1;  lit = colour.rgb * (g_NoLightLuminance + shadow * g_LightLuminanceDelta)
//   shadow = sat(1.025 + 0.25 * (P.y + offset.y) - 512 * volumeShadow(P.xz))   (s12 g_VolumeShadow, the level's volume map)
//   area = (size.x scale / w) (size.y scale / w);  alpha = colour.a * sat(fade.x * area + fade.y), killed if < fade.z
//   NoLightPS (soft particles): alpha *= sat(|z_scene - z| * 75 + dot(o2, o2) * 0.3), z = D3D z / w (depth buffer value)
//   out = (lit, alpha) * texture(uv)
// Blend: normal SRCALPHA / INVSRCALPHA, additive SRCALPHA / ONE; Z test LEQUAL, no Z write (00698890 render states);
// sampler linear / linear / point mip, clamp. No fog in these programs. Raw values throughout, blended in ReflexRenderPipeline's FP16
// target as the original blends in its A16B16G16R16F scene (ReflexCommon.hlsl RfxOutput). Verified against the software-rasterized
// original's draws (Editor/Tests/ObjShadeTests: the recorded instance streams drawn with this shader, mean |d| 0.0001..0.0008 HDR).
// Soft particles (NoLightPS, when the game's flag 0xc11d04 is set: off in the recorded frames) fade by the depth of a 2 x 2 minimum
// downsample of the scene depth (DownsampleDepthPS, sampled half a texel in); here by the full-resolution scene depth.
Shader "Reflex/Original Particles"
{
    Properties
    {
        [NoScaleOffset] _MainTex("Particle atlas (the level's ParticleEmitterAsset texture)", 2D) = "white" {}
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 5
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 10
        _Soft("Soft particles (NoLightPS)", Float) = 1
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Transparent" "Queue"="Transparent" "IgnoreProjector"="True" }
        Pass
        {
            Name "Particles"
            Tags { "LightMode"="UniversalForward" }
            Blend [_SrcBlend] [_DstBlend]
            ZWrite Off ZTest LEqual Cull Off
            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/DeclareDepthTexture.hlsl"
            #include "ReflexCommon.hlsl"
            #include "ReflexVolumetricApply.hlsl"

            TEXTURE2D(_MainTex); SAMPLER(sampler_MainTex);
            StructuredBuffer<float4> _Particles;    // 4 float4 per ReflexParticle (64 bytes)
            int _Base;                              // first particle of this draw
            float4 _Luminance, _LuminanceDelta;     // c101, c102
            float4 _Fade;                           // c100
            float _Soft;
            float _SrcBlend, _DstBlend;
            float _RfxParticleDebug;                // test: every particle quad drawn opaque magenta
            // (_RfxVolumeShadowParams / _RfxVolumeShadowMap: ReflexCommon.hlsl, the level's volume shadow map, s12 g_VolumeShadow)

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float2 uv : TEXCOORD0;
                float4 colour : TEXCOORD1;   // o2: lit-without-shadow rgb (c101), faded alpha
                float4 delta : TEXCOORD2;    // o3: rgb * c102
                float3 shadow : TEXCOORD3;   // volume shadow uv, height term
                float depth : TEXCOORD4;     // D3D z / w of the quad
                float3 positionWS : TEXCOORD5;   // (VOLUMETRIC LIGHT: ReflexVolumetricApply.hlsl)
            };

            float4 Unpack(uint c) { return float4((c >> 16) & 255u, (c >> 8) & 255u, c & 255u, c >> 24) / 255.0; }
            float D3DDepth(float eye) { float n = _ProjectionParams.y, f = _ProjectionParams.z; return f / (f - n) * (1.0 - n / max(eye, 1e-6)); }

            Varyings Vert(uint vid : SV_VertexID)
            {
                static const uint kIndex[6] = { 0, 1, 2, 0, 2, 3 };
                static const float2 kCorner[4] = { float2(-1, 1), float2(1, 1), float2(1, -1), float2(-1, -1) };
                uint p = (uint)_Base + vid / 6u, corner = kIndex[vid % 6u];
                float4 a = _Particles[p * 4u], b = _Particles[p * 4u + 1u], c = _Particles[p * 4u + 2u], d = _Particles[p * 4u + 3u];
                float3 P = a.xyz; float2 size = float2(a.w, b.x), pivot = b.yz; float scale = b.w, rot = c.x;
                float4 frame = float4(c.w, d.x, d.y, d.z);
                float2 k = kCorner[corner];
                Varyings o;
                o.uv = (k * float2(0.5, -0.5) + 0.5) * frame.xy + frame.zw;
                o.uv.y = 1.0 - o.uv.y;                        // Unity row order (textures are stored bottom row first)
                float2 s = size * scale;
                float X = s.x * (k.x - pivot.x), Y = s.y * (k.y - pivot.y);
                float r = frac(rot * 0.159154937 + 0.5) * 6.28318548 - 3.14159274, sn, cs;
                sincos(r, sn, cs);
                float2 off = float2(X * cs + Y * sn, Y * cs - X * sn);
                float3 right = UNITY_MATRIX_I_V._m00_m10_m20, up = UNITY_MATRIX_I_V._m01_m11_m21;
                float3 world = P + off.x * right + off.y * up;
                float4 clip = TransformWorldToHClip(world);
                float w = clip.w;
                float4 col = Unpack(asuint(c.y)) * Unpack(asuint(c.z));
                o.delta = col * _LuminanceDelta;
                float area = (s.x / w) * (s.y / w);
                float alpha = saturate(_Fade.x * area + _Fade.y) * col.a;
                o.colour = float4(col.rgb * _Luminance.rgb, alpha);
                o.shadow = float3(0.5 + P.x * _RfxVolumeShadowParams.x, 0.5 + P.z * _RfxVolumeShadowParams.x, (off.y + P.y) * 0.25 + 1.025);
                o.depth = D3DDepth(w);
                o.positionWS = world;
                o.positionCS = alpha < _Fade.z && (_RfxParticleDebug < 0.5 || _RfxParticleDebug > 1.5) ? float4(2, 2, 2, 1) : clip;   // killed: outside the clip volume (D3D: 0; w = 0 is not safe here)
                return o;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                if (_RfxParticleDebug > 0.5 && _RfxParticleDebug < 1.5) return half4(1, 0, 1, 1);
                if (_RfxParticleDebug > 1.5 && _RfxParticleDebug < 2.5) return half4(saturate(i.colour.rgb + i.delta.rgb), saturate(i.colour.a));
                if (_RfxParticleDebug > 2.5 && _RfxParticleDebug < 3.5) return SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                float shadow = 1.0;
                if (_RfxVolumeShadowParams.y > 0.5)
                    shadow = saturate(SAMPLE_TEXTURE2D(_RfxVolumeShadowMap, sampler_RfxVolumeShadowMap, i.shadow.xy).r * (-512.0 * RfxMapScale()) + i.shadow.z);
                float4 c = i.colour + shadow * float4(i.delta.rgb, 0);
                if (_Soft > 0.5 && _RfxParticleDebug < 3.5)
                {
                    float2 suv = i.positionCS.xy / _ScaledScreenParams.xy;
                    float scene = D3DDepth(LinearEyeDepth(SampleSceneDepth(suv), _ZBufferParams));
                    c.a *= saturate(abs(scene - i.depth) * 75.0 + dot(i.colour, i.colour) * 0.3);
                }
                float4 o = c * SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                // VOLUMETRIC LIGHT: particles draw after the medium's composite: the medium in front of them (ReflexVolumetricApply.hlsl;
                // the additive manager only dims by its transmittance)
                o.rgb = RfxVolApply(o.rgb, o.a, i.positionWS, i.positionCS, _SrcBlend, _DstBlend);
                o.rgb = RfxOutput(o.rgb);   // raw values for ReflexRenderPipeline's HDR target
                return o;
            }
            ENDHLSL
        }
    }
}
