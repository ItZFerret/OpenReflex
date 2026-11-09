// The forest's 3D trees (ForestSystem, SpeedTree geometry of the level's "tree" assets): hand port of the original programs the
// forest draws them with (_re/notes/forest.md 4), chosen by the tree material's shader asset (Template_Eco_*):
//   _Template 0: Template_Eco_Frond / Template_Eco_Leaf_CamFace (BranchFrondVertexShader 32f9df6a, LeafVertexShader cd63a54c,
//                SharedPixelShader 7338d8a4: per-vertex fill lights, per-pixel sun, volume shadow, alpha = tex.a - fade)
//   _Template 1: Template_Eco_Branch_Nm (BranchFrondVertexShader 506936be, SharedPixelShader 96195ae2: tangent-space normal map,
//                per-pixel fill lights + bounce, the VSM dynamic shadow (Unity main-light shadow here), alpha = 1 - fade)
//   _Template 2: Template_Eco_Branch_Detail_Nm (f0457331 / bd37bddd: as 1, diffuse = lerp(main(uv.xy), detail(uv.zw), detail.a))
// Geometry (per draw, _RfxTreeLeaf): branches and fronds use BranchFrondVertexShader; leaves LeafVertexShader (camera-facing
// cards: corner offset (TEXCOORD4 of the asset) along the camera's right / up / forward, times the world matrix's y scale).
//
// Instancing: the original draws every tree with its own constants (gWorld c64, g_TreeAlphaTests c68, gColorRefSample c84); here
// OriginalForest draws all trees of a (type, LOD record) at once with Graphics.RenderMeshPrimitives and this shader reads each
// tree from _RfxTreeInstances (3 float4: position + scale, sin / cos of the quantised rotation, the four alpha tests). No
// instancing keywords are involved (SV_InstanceID + a structured buffer).
//
// The LOD of the original is an alpha-test dissolve: gTreeLODParams1/2 (c80/c81, per type and geometry) turn the view depth and
// the vertex's LOD value (POSITION.w) into the divisor of the tree's alpha test value; gTreeLODParams3 (c82) picks the
// component of g_TreeAlphaTests for the geometry (branch x, frond y, leaf z, the next leaf LOD w).
Shader "Reflex/Tree"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Diffuse (usr_sampler_diffuse_texture / usr_sampler_main_branch_texture)", 2D) = "white" {}
        [NoScaleOffset] _DetailMap("Detail (usr_sampler_detail_branch_texture)", 2D) = "white" {}
        [NoScaleOffset] _NormalMap("Normal map (usr_sampler_normal_texture)", 2D) = "bump" {}
        _Template("Template: 0 Frond / Leaf_CamFace, 1 Branch_Nm, 2 Branch_Detail_Nm", Float) = 0
        _AliveNormal("MX vs ATV Alive normal pass of template 0: 0 none, 1 T_EcoLeaves (rgb), 2 T_EcoBranches (rg, z derived)", Float) = 0
        _UseDynShadow("Dynamic shadow (the VSM of Branch_Nm / Detail_Nm: Unity main-light shadow)", Float) = 0
        _AlphaRef("ALPHAREF (D3DRS 24, func GREATER)", Float) = 0
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
        [Enum(Off, 0, On, 1)] _ZWrite("ZWrite", Float) = 1
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="TransparentCutout" "Queue"="AlphaTest" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

        CBUFFER_START(UnityPerMaterial)
            float _Template, _UseDynShadow, _AlphaRef, _AliveNormal;
        CBUFFER_END
        TEXTURE2D(_BaseMap);   SAMPLER(sampler_BaseMap);
        TEXTURE2D(_DetailMap); SAMPLER(sampler_DetailMap);
        TEXTURE2D(_NormalMap); SAMPLER(sampler_NormalMap);

        #include "ReflexCommon.hlsl"
        #include "ReflexDrawDistance.hlsl"

        // per draw (MaterialPropertyBlock): the instance range and the original's per-draw constants
        StructuredBuffer<float4> _RfxTreeInstances;
        float _RfxTreeBase;            // first instance of this draw
        float4 _RfxTreeLod1;           // gTreeLODParams1 c80 (type + 0x1d050 branch / + 0x1d070 frond)
        float4 _RfxTreeLod2;           // gTreeLODParams2 c81
        float4 _RfxTreeSel;            // gTreeLODParams3 c82: (1,0,0,0) branch, (0,1,0,0) frond, (0,0,1,0) leaf, (0,0,0,1) next leaf LOD
        float _RfxTreeLeaf;            // 1: LeafVertexShader (camera-facing leaf cards)
        // per camera (OriginalForest, beginCameraRendering): gViewInverse c0..c2 columns and gCameraPos of the view the forest is
        // drawn for (also used by the shadow-caster pass, so the cards keep the main view's orientation)
        float4 _RfxTreeCamPos, _RfxTreeCamRight, _RfxTreeCamUp, _RfxTreeCamFwd;

        struct TreeInst { float3 pos; float scale; float sn, cs; float2 dith; float4 alpha; };   // dith: the LOD cross-fade (RfxDitherClip), (0, 0) = none
        TreeInst LoadTree(uint instanceID)
        {
            uint i = ((uint)_RfxTreeBase + instanceID) * 3u;
            float4 a = _RfxTreeInstances[i], b = _RfxTreeInstances[i + 1u];
            TreeInst t; t.pos = a.xyz; t.scale = a.w; t.sn = b.x; t.cs = b.y; t.dith = b.zw; t.alpha = _RfxTreeInstances[i + 2u];
            return t;
        }
        // gWorld (00660720): rows (cos s, 0, sin s, x), (0, s, 0, y), (-sin s, 0, cos s, z) with the record's +0x18 sin / +0x1c cos
        float3 TreeDir(TreeInst t, float3 v) { return float3(t.cs * t.scale * v.x + t.sn * t.scale * v.z, t.scale * v.y, -t.sn * t.scale * v.x + t.cs * t.scale * v.z); }
        float3 TreePoint(TreeInst t, float3 v) { return TreeDir(t, v) + t.pos; }

        // the vertex programs' position and fade: P (world), fade = o4.w / o2.w of the original
        //   branch / frond: alphaTest / min(sat(lod * c81.x + depth * c80.x + c80.y) + 1/3, 1)   (BranchFrondVertexShader 0418..04a4)
        //   leaf: alphaTest                                                                     (LeafVertexShader 0510..051c)
        // depth = dot(P - camera, forward) (= the clip w the original uses, dp4 r0.y, r1, c7)
        float3 TreeVertex(TreeInst t, float3 posOS, float lod, float3 corner, out float fade)
        {
            float3 P = TreePoint(t, posOS);
            float alphaTest = dot(_RfxTreeSel, t.alpha);
            if (_RfxTreeLeaf > 0.5)
            {
                float3 r0 = corner.x * _RfxTreeCamRight.xyz + corner.y * _RfxTreeCamUp.xyz + corner.z * _RfxTreeCamFwd.xyz;
                P += t.scale * r0;                                   // mad r1.xyz, c65.y, r0, r2 (c65.y = gWorld._22 = scale)
                fade = alphaTest;
            }
            else
            {
                float depth = RfxForestDepth(P - _RfxTreeCamPos.xyz, _RfxTreeCamFwd.xyz);   // HIGH / ULTRA: the distance, stretched (ReflexDrawDistance.hlsl)
                float z = depth * _RfxTreeLod1.x + _RfxTreeLod1.y;
                float d = min(saturate(lod * _RfxTreeLod2.x + z) + 0.333333343, 1.0);
                fade = alphaTest * rcp(d);
            }
            return P;
        }

        float TreeAlpha(float fade, float texAlpha)
        {
            return _Template > 0.5 ? saturate(1.0 - fade) : saturate(texAlpha - fade);   // add_sat oC0.w, 1, -v4.w / tex.a - v2.w
        }
        // ALPHATEST: round(sat(alpha) * 255) > ALPHAREF (D3DCMP_GREATER, as the reference rasterizer compares)
        void TreeClip(float alpha) { clip(round(saturate(alpha) * 255.0) - _AlphaRef - 0.5); }
        // the pixel's test: Reflex's alpha test, or on Alive's content the leaves' and fronds' screen-noise dissolve (texel alpha x fade,
        // RfxFoliageClip); the bark (template 1) keeps its dissolve test
        void TreeKeep(float fade, float texAlpha, float2 pixel)
        {
            if (_Template < 0.5 && RfxFoliageDither()) RfxFoliageClip(texAlpha * RfxFoliageFade(fade), pixel);
            else TreeClip(TreeAlpha(fade, texAlpha));
        }

        float TreeTexAlpha(float4 uv)
        {
            if (_Template > 0.5) return 1.0;
            return SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, uv.xy).a;
        }

        struct TreeAttributes
        {
            float3 positionOS : POSITION;          // SpeedTree position (asset POSITION xyz)
            float3 normalOS : NORMAL;
            float4 tangentOS : TANGENT;
            float4 uv0 : TEXCOORD0;                // asset TEXCOORD0 (xy diffuse, zw detail; v flipped for Unity's row order)
            float4 uv1 : TEXCOORD1;                // x: POSITION.w (the vertex's LOD value) / leaves: xyz corner (TEXCOORD4), w LOD value
            float2 uv2 : TEXCOORD2;                // x: NORMAL.w (MX vs ATV Alive: the vertex's ambient occlusion; 1 for Reflex trees)
            uint instanceID : SV_InstanceID;
        };
        float TreeLodValue(TreeAttributes v) { return _RfxTreeLeaf > 0.5 ? v.uv1.w : v.uv1.x; }
        float3 TreeCorner(TreeAttributes v) { return _RfxTreeLeaf > 0.5 ? v.uv1.xyz : float3(0, 0, 0); }
        ENDHLSL

        Pass
        {
            Name "ForwardLit"
            Tags { "LightMode"="UniversalForward" }
            Cull [_Cull]
            ZWrite [_ZWrite]
            ZTest LEqual

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile_fragment _ _SHADOWS_SOFT _SHADOWS_SOFT_LOW _SHADOWS_SOFT_MEDIUM _SHADOWS_SOFT_HIGH
            #pragma multi_compile_fragment _ _RFX_ENHANCED
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "ReflexEnhanced.hlsl"
            #include "ReflexAliveShadow.hlsl"
            #include "ReflexShadows.hlsl"

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 positionWS : TEXCOORD0;     // o0
                float3 normalWS : TEXCOORD1;       // N (unnormalised, as the original interpolates it)
                float3 tangentWS : TEXCOORD2;      // T
                float3 binormalWS : TEXCOORD3;     // B = T x N (BranchFrondVertexShader 06a0..0790)
                float4 uv : TEXCOORD4;
                float4 fog : TEXCOORD5;            // rgb: amount * colour, w: 1 - amount
                float4 ambient : TEXCOORD6;        // rgb: the vertex fill lights (template 0), w: fade
                float shade : TEXCOORD7;           // NORMAL.w (Alive)
                float2 dith : TEXCOORD8;           // the LOD cross-fade
            };

            Varyings Vert(TreeAttributes v)
            {
                Varyings o;
                TreeInst t = LoadTree(v.instanceID);
                float fade;
                float3 P = TreeVertex(t, v.positionOS, TreeLodValue(v), TreeCorner(v), fade);
                o.positionWS = P;
                o.positionCS = TransformWorldToHClip(P);
                float3 N = TreeDir(t, v.normalOS), T = TreeDir(t, v.tangentOS.xyz);
                o.normalWS = N; o.tangentWS = T; o.binormalWS = cross(T, N);
                o.uv = v.uv0;
                o.fog = RfxFog(P, _WorldSpaceCameraPos);
                o.ambient = float4(_Template < 0.5 ? RfxAmbient(N) : float3(0, 0, 0), fade);
                o.shade = v.uv2.x; o.dith = t.dith;
                return o;
            }

            // ambient of the Branch_Nm / Detail pixel programs (05ec..06f8): the four fill lights + sat(-N.y) lerp(ref.rgb, bounce, ref.w)
            float3 AmbientPixel(float3 N)
            {
                float3 a = saturate(dot(_RfxLightDir1.xyz, N)) * _RfxLightColor1.rgb;
                a += saturate(dot(_RfxLightDir0.xyz, N)) * _RfxLightColor0.rgb;
                a += saturate(dot(_RfxLightDir2.xyz, N)) * _RfxLightColor2.rgb;
                a += saturate(dot(_RfxLightDir3.xyz, N)) * _RfxLightColor3.rgb;
                float3 bounce = _RfxColorRef.w * _RfxBounceColor.rgb - (_RfxColorRef.w - 1.0) * _RfxColorRef.rgb;
                return bounce * saturate(-N.y) + a;
            }

            half4 Frag(Varyings i) : SV_Target
            {
                RfxDitherClip(i.dith, i.positionCS.xy);
                float3 P = i.positionWS;
                float fade = i.ambient.w;
                float3 colour; float alpha;
                float3 rad = RfxRadiosity();
            #if defined(_RFX_ENHANCED)
                float2 rfxOcc = RfxEnhOcclusion(i.positionCS);   // LIGHTING: ENHANCED: AO / contact shadow, sky-coloured ambient
            #endif
                if (_Template < 0.5)
                {
                    // SharedPixelShader 7338d8a4: (rad vol c56 sat(sun.N)) tex + tex ambient; alpha sat(tex.a - fade)
                    float4 tex = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy);
                    float3 N = normalize(i.normalWS);
                    if (_AliveNormal > 0.5)
                    {
                        // MX vs ATV Alive's normal pass (T_EcoLeaves / T_EcoBranches NormalPixelShader with NormalLeafVertexShader /
                        // NormalBranchFrondVertexShader): the light buffer is lit with the normal-mapped world normal
                        //   N' = normalize(n.x T + n.y (T x N) + n.z N), the three frame vectors normalised first;
                        //   leaves: n = 2 tex.rgb - 1 (tfetch .xzy into (T, N, T x N)); fronds: n.xy = 2 tex.rg - 1, n.z = sqrt(1 - x^2 - y^2)
                        float3 T = normalize(i.tangentWS), Bn = normalize(i.binormalWS);
                        float4 nm = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.xy);
                        float3 n = nm.rgb * 2.0 - 1.0;
                        if (_AliveNormal > 1.5) n.z = sqrt(abs(1.0 - n.x * n.x - n.y * n.y));
                        N = n.x * T + n.y * Bn + n.z * N;
                        N *= rsqrt(max(dot(N, N), 1e-12));
                    }
                    float3 sun = rad * RfxVolumeShadow(P) * RfxMapShadow(P) * _RfxSunColor.rgb * saturate(dot(_RfxSunDir.xyz, N));
                    if (RfxIsAlive())
                    {
                        // Alive lights every deferred pixel with DL_AmdDirLight's shadow mask (volume x the dynamic shadow map), leaves and
                        // fronds included: Unity's main-light shadow stands in for the VSM (in ENHANCED it is applied below)
                    #if !defined(_RFX_ENHANCED)
                        sun *= RfxAliveDynShadow(P);
                    #endif
                    }
                    // MX vs ATV Alive (T_EcoLeaves / Bark / Branches): albedo = (tex x NORMAL.w)^2 in both looks (RfxTreeAlbedo) (the tree vertex programs pass the
                    // normal's w, SpeedTree's ambient occlusion, as the colour interpolator o4), the light buffer's ambient (RfxAliveAmbient)
                    float3 albedo = RfxTreeAlbedo(RfxIsAliveData() ? tex.rgb * i.shade : tex.rgb);   // (the occlusion and the squared texel are Alive's data: kept in the REFLEX look)
                    float3 amb0 = RfxAmbientAt(i.ambient.rgb, N, P);
                #if defined(_RFX_ENHANCED)
                    {
                        float4 sc = TransformWorldToShadowCoord(P);
                        sun *= RfxMainLightShadow(sc) * rfxOcc.g;
                        colour = sun * albedo + albedo * RfxEnhAmbient(amb0, N, rfxOcc.r);
                    }
                #else
                    colour = sun * albedo + albedo * amb0;
                #endif
                    alpha = saturate(tex.a - fade);
                    if (RfxFoliageDither()) { RfxFoliageClip(tex.a * RfxFoliageFade(fade), i.positionCS.xy); alpha = 1.0; }   // Alive: the screen-noise dissolve
                }
                else
                {
                    // Branch_Nm 96195ae2 / Branch_Detail_Nm bd37bddd: N' = normalize(n.x T + n.y B + n.z N), n from the RG normal map
                    float3 T = normalize(i.tangentWS), N = normalize(i.normalWS), B = normalize(i.binormalWS);
                    float2 rg = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.xy).rg;
                    float2 nxy = rg * 2.0 - 1.0;
                    float nz = sqrt(abs(1.0 - nxy.x * nxy.x - nxy.y * nxy.y));       // rcp(rsq(x)): rsq takes |x|
                    float3 N2 = normalize(nxy.x * T + nxy.y * B + nz * N);
                    float3 ambient = RfxAmbientAt(AmbientPixel(N2), N2, P);
                #if defined(_RFX_ENHANCED)
                    ambient = RfxEnhAmbient(ambient, N2, rfxOcc.r);
                #endif
                    float nl = saturate(dot(_RfxSunDir.xyz, N2));
                    float shadow = 1.0;
                #if defined(_RFX_ENHANCED)
                    if (true)
                #else
                    if (_UseDynShadow > 0.5)
                #endif
                    {
                    #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                        float4 shadowCoord = ComputeScreenPos(TransformWorldToHClip(P));
                    #else
                        float4 shadowCoord = TransformWorldToShadowCoord(P);
                    #endif
                        shadow = RfxMainLightShadow(shadowCoord);                      // stand-in for the VSM (07a0..0884)
                    }
                    shadow *= RfxVolumeShadow(P) * RfxMapShadow(P);
                #if defined(_RFX_ENHANCED)
                    shadow *= rfxOcc.g;
                #endif
                    float3 sun = nl * (shadow * rad * _RfxSunColor.rgb);
                    float4 D;
                    if (_Template > 1.5)
                    {
                        float4 main = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy);
                        float4 detail = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.zw);
                        D = float4(lerp(main.rgb, detail.rgb, detail.a), 1);             // lrp r4.xyz, r3.w, r3, r2
                    }
                    else D = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy);
                    D.rgb = RfxTreeAlbedo(RfxIsAliveData() ? D.rgb * i.shade : D.rgb);
                    colour = D.rgb * ambient + sun * D.rgb;
                    alpha = saturate(1.0 - fade);
                }
                TreeClip(alpha);
                colour = colour * i.fog.w + i.fog.rgb;
                return half4(RfxOutput(colour), alpha);
            }
            ENDHLSL
        }

        Pass
        {
            Name "ShadowCaster"
            Tags { "LightMode"="ShadowCaster" }
            Cull [_Cull]
            ZWrite On
            ZTest LEqual
            ColorMask 0

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex ShadowVert
            #pragma fragment ShadowFrag
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            float3 _LightDirection;
            float3 _LightPosition;
            struct Varyings { float4 positionCS : SV_POSITION; float4 uv : TEXCOORD0; float fade : TEXCOORD1; };

            Varyings ShadowVert(TreeAttributes v)
            {
                Varyings o;
                TreeInst t = LoadTree(v.instanceID);
                float3 P = TreeVertex(t, v.positionOS, TreeLodValue(v), TreeCorner(v), o.fade);
                float3 N = normalize(TreeDir(t, v.normalOS));
            #if _CASTING_PUNCTUAL_LIGHT_SHADOW
                float3 lightDirectionWS = normalize(_LightPosition - P);
            #else
                float3 lightDirectionWS = _LightDirection;
            #endif
                float4 positionCS = TransformWorldToHClip(ApplyShadowBias(P, N, lightDirectionWS));
            #if UNITY_REVERSED_Z
                positionCS.z = min(positionCS.z, UNITY_NEAR_CLIP_VALUE);
            #else
                positionCS.z = max(positionCS.z, UNITY_NEAR_CLIP_VALUE);
            #endif
                o.positionCS = positionCS; o.uv = v.uv0;
                return o;
            }
            half4 ShadowFrag(Varyings i) : SV_Target
            {
                TreeClip(TreeAlpha(i.fade, TreeTexAlpha(i.uv)));
                return 0;
            }
            ENDHLSL
        }

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
            struct Varyings { float4 positionCS : SV_POSITION; float4 uv : TEXCOORD0; float fade : TEXCOORD1; float2 dith : TEXCOORD2; };
            Varyings DepthVert(TreeAttributes v)
            {
                Varyings o;
                TreeInst t = LoadTree(v.instanceID);
                float3 P = TreeVertex(t, v.positionOS, TreeLodValue(v), TreeCorner(v), o.fade);
                o.positionCS = TransformWorldToHClip(P); o.uv = v.uv0; o.dith = t.dith;
                return o;
            }
            half DepthFrag(Varyings i) : SV_Target
            {
                RfxDitherClip(i.dith, i.positionCS.xy);
                TreeKeep(i.fade, TreeTexAlpha(i.uv), i.positionCS.xy);
                return i.positionCS.z;
            }
            ENDHLSL
        }

        Pass
        {
            Name "DepthNormals"
            Tags { "LightMode"="DepthNormals" }
            Cull [_Cull]
            ZWrite On

            HLSLPROGRAM
            #pragma target 4.5
            #pragma vertex DepthNormalsVert
            #pragma fragment DepthNormalsFrag
            #pragma multi_compile_fragment _ _GBUFFER_NORMALS_OCT
            struct Varyings { float4 positionCS : SV_POSITION; float4 uv : TEXCOORD0; float fade : TEXCOORD1; float3 normalWS : TEXCOORD2; float2 dith : TEXCOORD3; };
            Varyings DepthNormalsVert(TreeAttributes v)
            {
                Varyings o;
                TreeInst t = LoadTree(v.instanceID);
                float3 P = TreeVertex(t, v.positionOS, TreeLodValue(v), TreeCorner(v), o.fade);
                o.positionCS = TransformWorldToHClip(P); o.uv = v.uv0; o.normalWS = TreeDir(t, v.normalOS); o.dith = t.dith;
                return o;
            }
            half4 DepthNormalsFrag(Varyings i) : SV_Target
            {
                RfxDitherClip(i.dith, i.positionCS.xy);
                TreeKeep(i.fade, TreeTexAlpha(i.uv), i.positionCS.xy);
                float3 normalWS = normalize(i.normalWS);
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
    Fallback Off
}
