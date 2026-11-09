// Hand port of the original MX vs ATV Reflex scenery programs (the Template_* shaders of the level packages, FR_AL_Rock2,
// FR_AL_EoW_*). The templates share their building blocks; which blocks a template uses, in which variant, and its literal
// constants are material values set by Reflex.Port.Content.ReflexSceneryMaterial from a per-template table transcribed from the
// disassembly of each program (recorded frames of the original renderer: _re/notes/render-pipeline.md section 6).
//
// Lit templates (_Mode 0), in the programs' own order of operations:
//   D        = diffuse(uv0 | uv1) * usr_diffuse_fade  [* AO(uv1): 1 - (1 - ao) k | ao | 1 - ao k]  [* (1 - vertex rgb)] (or a constant)
//   N        = normalize(vertex normal), or the tangent-space map n = (2r-1, 2g-1, sqrt|1 - x^2 - y^2|), N' = n.x T + n.z N + n.y (T x N):
//              direct normalize(N'), or blended normalize(N'n - (N'n.N)N) * usr_normal_amount + N); two-sided: flipped by vFace
//   ambient  = vertex programs: sum sat(L_i.N) C_i + sat(-N.y) lerp(gColorRefSample.rgb, bounce, w)   (gColorRefSample = 0)
//              pixel programs:  the same with the colour reference texel (s13 Editor_Default_White) instead of gColorRefSample
//   shadow   = dynamic (VSM, stand-in: Unity main-light shadows) * volume sat(1 - 0.25 (2048 H - y - 0.1)); rad = floor(ref.a) ? ref.rgb : 1
//   colour   = D ambient + D shadow rad c56 sat(c55.N)
//   spec     = pow(sat(normalize(c55).R), h) rad [c56] min(shadow, sat(normalize(c55).N)) * scale * tint
//   env      = cube(R) (0.5 min(shadow, N.L) + 0.5) * amount; SSS (banners) -min(c55.R, 0)^3 (D sss)^2
//   out      = colour * (1 - fog) + fog colour (per vertex);  alpha by template
// Lit FMX arena programs: _Mode 20 Template_DF_X2_AO(_SP) (a second diffuse over the base by a mask), 21 the FMX ramp tops
// (texture + chevrons), 22 FMX_RampTop_Dover_SH. (product x 1.3); the rest of their lighting is the lit path's.
// The other arena programs (w-fsshaders; ReflexSceneryMaterial's table): unlit _Mode 23 (diffuse x AO map x vertex colour x power, or a
// two-tone lerp by the vertex alpha), 25 Template_Prelit_Banners, 26 Template_Prelit_Color_Dim_Ctl; lit ones use _FsParams (vertex colour
// factor, AO at UV0, specular map, constant alpha), _FsParams2 (two-tone, UV matrix, reflection bias / weight), _FsParams3 (D added)
// and _EnvParams.zw (reflection mask, the level's cube s14).
// Prelit / special templates: _Mode 1..11 (see Frag). sRGB sampling is disabled in the original device: textures are raw values,
// the result is a raw value for ReflexRenderPipeline's HDR target (RfxOutput).
Shader "Reflex/Original Scenery"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Diffuse", 2D) = "white" {}
        // raw material constants: Vector, not Color (a Color property is converted gamma -> linear in the linear project)
        _BaseColor("Constant colour (Editor_DiffuseColor, Color_Refl_Spec) / fallback", Vector) = (1, 1, 1, 1)
        [NoScaleOffset] _AOMap("Ambient occlusion (UV1) / prelit map", 2D) = "white" {}
        [NoScaleOffset] _NormalMap("Normal map (RG, 0..1)", 2D) = "bump" {}
        [NoScaleOffset] _DecalNormalMap("Gibson cliff decal normal (mode 13)", 2D) = "bump" {}
        [NoScaleOffset] _DetailMap("Edge dirt (mode 3/4), mud noise (mode 6), mud alpha (bayle), alpha texture", 2D) = "gray" {}
        [NoScaleOffset] _ComboMap("Combo map (mode 6: x fresnel, y mud)", 2D) = "black" {}
        [NoScaleOffset] _EnvCube("Reflection cube", Cube) = "black" {}
        _MudColor("Mud colour", Vector) = (0.25, 0.21, 0.17, 1)
        _MudAmount("Mud amount (mode 6)", Float) = 0
        _Mode("Mode", Float) = 0
        _DiffuseSrc("Diffuse: 0 texture, 1 x (1 - vertex rgb), 2 constant, 3 0.4 vertex rgb.xzy, 4 bayle mud", Float) = 0
        _AliveVColor("MX vs ATV Alive: albedo x vertex colour (T_*VColor programs)", Float) = 0
        [NoScaleOffset] _AliveVMTex("MX vs ATV Alive albedo program (RGBAFloat, 2 texels per op)", 2D) = "black" {}
        _AliveVMInfo("Alive albedo program: op count, result register, unlit, alpha register + 1", Vector) = (0, 0, 0, 0)
        _AliveP0("Alive program parameter 0", Vector) = (0, 0, 0, 0)
        _AliveP1("Alive program parameter 1", Vector) = (0, 0, 0, 0)
        _AliveP2("Alive program parameter 2", Vector) = (0, 0, 0, 0)
        _AliveP3("Alive program parameter 3", Vector) = (0, 0, 0, 0)
        [NoScaleOffset] _AliveT0("Alive program texture 0", 2D) = "gray" {}
        [NoScaleOffset] _AliveT1("Alive program texture 1", 2D) = "gray" {}
        [NoScaleOffset] _AliveT2("Alive program texture 2", 2D) = "gray" {}
        [NoScaleOffset] _AliveT3("Alive program texture 3", 2D) = "gray" {}
        [NoScaleOffset] _AliveT4("Alive program texture 4", 2D) = "gray" {}
        [NoScaleOffset] _AliveT5("Alive program texture 5", 2D) = "gray" {}
        [NoScaleOffset] _AliveT6("Alive program texture 6", 2D) = "gray" {}
        [NoScaleOffset] _AliveT7("Alive program texture 7", 2D) = "gray" {}
        [NoScaleOffset] _AliveT8("Alive program texture 8", 2D) = "gray" {}
        [NoScaleOffset] _AliveT9("Alive program texture 9", 2D) = "gray" {}
        [NoScaleOffset] _AliveT10("Alive program texture 10", 2D) = "gray" {}
        [NoScaleOffset] _AliveT11("Alive program texture 11", 2D) = "gray" {}
        [NoScaleOffset] _AliveC0("Alive program cube map 0", Cube) = "black" {}
        [NoScaleOffset] _AliveC1("Alive program cube map 1", Cube) = "black" {}
        _AliveWrap("Alive program textures sampled clamped (bit k: texture k)", Float) = 0
        _AliveVMInfo2("Alive program: general ops, emissive register + 1, normal register + 1", Vector) = (0, 0, 0, 0)
        _DiffuseUV("Diffuse UV set", Float) = 0
        _AOMode("AO: 0 none, 1 1-(1-ao)k, 2 ao, 3 1-ao k", Float) = 0
        _AOAmount("AO amount", Float) = 1
        _DiffuseFade("Diffuse fade", Float) = 1
        _NormalMode("Normal: 0 vertex, 1 map blended by amount, 2 map direct", Float) = 0
        _NormalUV("Normal map UV set", Float) = 0
        _NormalAmount("Normal amount", Float) = 1
        _TwoSided("Two-sided normal (vFace)", Float) = 0
        _AmbientSrc("Ambient: 0 vertex, 1 pixel", Float) = 0
        _UseVolShadow("Volume shadow map", Float) = 1
        _UseDynShadow("Dynamic shadow", Float) = 1
        _ShadowLit("Lighting uses the shadow term", Float) = 1
        _SpecParams("Specular: hardness, scale, uses sun colour, enabled", Vector) = (0, 0, 1, 0)
        _SpecTint("Specular colour", Vector) = (1, 1, 1, 1)
        _EnvParams("Reflection: amount, enabled", Vector) = (0, 0, 0, 0)
        _SSS("Sub-surface amount", Float) = 0
        _UVScale("UV scale", Vector) = (1, 1, 0, 0)
        _PrelitPower("Prelit power / brightness", Float) = 1
        _EncodeBase("Mode 1: the texture is a linear render target of a UI (its bytes are the colours drawn): re-encode it to what was drawn", Float) = 0
        _EmissiveAdd("Added to the lit colour before the fog (Template_Prelit_Map_A: usr_power)", Float) = 0
        _AlphaSource("Output alpha: 0 one, 1 diffuse alpha, 2 vertex alpha, 3 second texture alpha, 4 diffuse alpha - vertex alpha, 5 1 - vertex alpha, 6 constant, 7 diffuse r x vertex r, 8 vertex r, 9 diffuse alpha x vertex alpha", Float) = 0
        _AlphaTest("Alpha test", Float) = 0
        _AlphaFuncGE("Alpha func GREATEREQUAL (else GREATER)", Float) = 0
        _Cutoff("Alpha reference", Range(0, 1)) = 0
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend("Src blend", Float) = 1
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend("Dst blend", Float) = 0
        [Enum(Off, 0, On, 1)] _ZWrite("ZWrite", Float) = 1
        // Model_Submit's distance fade (OriginalSceneryLod): alpha byte / 255 per renderer; an opaque material is drawn through a blended
        // clone while fading (PlatformModelAllSemiTransEvent) whose output alpha is the fade alone
        _FsParams("w-fsshaders: D x vertex colour (1 rgb, 2 xzy), AO at UV0, specular x (1 detail rgb, 2 AO), constant alpha", Vector) = (0, 0, 0, 1)
        _FsParams2("w-fsshaders: two-tone (1 UV0, 2 projected), UV matrix on, reflection bias, reflection unweighted", Vector) = (0, 0, 0, 0)
        _FsParams3("w-fsshaders: + D x this (self-lit), sun without radiosity", Vector) = (0, 0, 0, 0)
        _UVMatrix("w-fsshaders: diffuse UV matrix in D3D space (u' = x u + y v, v' = z u + w v)", Vector) = (1, 0, 0, 1)
        _FsColor("w-fsshaders: reflection tint / usr_dim_amount", Vector) = (1, 1, 1, 1)
        _RfxModelAlpha("Model fade alpha", Float) = 1
        _RfxFadeOpaque("Fading clone of an opaque material", Float) = 0
    }

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Geometry" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

        CBUFFER_START(UnityPerMaterial)
            float4 _BaseColor;
            float4 _SpecParams;
            float4 _SpecTint;
            float4 _EnvParams;
            float4 _UVScale;
            float4 _MudColor;
            float _MudAmount;
            float _AliveVColor;
            float4 _AliveVMInfo, _AliveVMInfo2;
            float _AliveWrap;
            float4 _AliveP0, _AliveP1, _AliveP2, _AliveP3;
            float _Mode, _DiffuseSrc, _DiffuseUV, _AOMode, _AOAmount, _DiffuseFade, _NormalMode, _NormalUV, _NormalAmount, _TwoSided, _AmbientSrc;
            float _UseVolShadow, _UseDynShadow, _ShadowLit, _SSS, _PrelitPower;
            float _AlphaSource, _AlphaTest, _AlphaFuncGE, _Cutoff;
            float _RfxModelAlpha, _RfxFadeOpaque, _EmissiveAdd, _EncodeBase;
            float _SrcBlend, _DstBlend;
            float4 _FsParams, _FsParams2, _FsParams3, _UVMatrix, _FsColor;
        CBUFFER_END

        TEXTURE2D(_BaseMap);   SAMPLER(sampler_BaseMap);
        TEXTURE2D(_AOMap);     SAMPLER(sampler_AOMap);
        TEXTURE2D(_NormalMap); SAMPLER(sampler_NormalMap);
        TEXTURE2D(_DecalNormalMap); SAMPLER(sampler_DecalNormalMap);
        TEXTURE2D(_DetailMap); SAMPLER(sampler_DetailMap);
        TEXTURE2D(_ComboMap);  SAMPLER(sampler_ComboMap);
        TEXTURECUBE(_EnvCube); SAMPLER(sampler_EnvCube);
        float4 _ScrollParams;          // ScrollingTextureRSData (per renderer): U / V seconds for wrap, reverse U / V
        float4 _RfxScrollOverride;     // tests: xy = gTextureOffset of a recorded frame when z > 0

        // Level-wide values (the original's global shader constants): Shaders/ReflexCommon.hlsl
        #include "ReflexCommon.hlsl"
        #include "ReflexDrawDistance.hlsl"
        // DRAW DISTANCE HIGH / ULTRA: the fade of a model past 70% of its draw distance is a dither on the opaque material (OriginalSceneryLod keeps it
        // opaque: it keeps casting its shadow and needs no sorting); ORIGINAL (_RfxDrawDist.z = 0) fades through blended clones as the original
        void RfxModelDither(float2 pixel) { if (_RfxDrawDist.z > 0.5 && _RfxModelAlpha < 0.999) clip(_RfxModelAlpha - RfxDitherNoise(pixel)); }

        // MX vs ATV Alive: a level's own Xbox pixel program, reduced by the converter (_re/formats/alive_recipe.py) to its albedo before
        // the square: ops over float4 registers, read from _AliveVMTex (op k: texel 2k = code, destination, a, b; texel 2k + 1 = p0..p3).
        //   0 fetch     r[d] = T_a(uv); uv = UV0 (p0 0) / UV1 (1) x (p1, p2), or the camera distance ramp (2 smoothstep(sat(p1 d)),
        //               3 sat(p1 d)), or the view-angle ramp (4: 1 - |N.V|); b = 4: .rgb (a colour fetch), 0..3: that channel broadcast
        //   1 vertex colour (b as above); _AliveVMInfo: op count, result register, z 1 = an unlit program (the result is the colour)
        //     2 constant p0   3 add   4 mul   5 neg   6 abs   7 sat   8 lerp(r[a], r[b], r[p0])   9 max  10 min
        //    11 material parameter _AliveP[a] (b as above); a fetch's p3 = k + 1 offsets its UV by _AliveP[k].xy (D3D space)
        // The fetches use explicit gradients (the loop is uniform per material but not provably so).
        TEXTURE2D(_AliveVMTex);
        TEXTURE2D(_AliveT0); TEXTURE2D(_AliveT1); TEXTURE2D(_AliveT2); TEXTURE2D(_AliveT3); TEXTURE2D(_AliveT4); TEXTURE2D(_AliveT5);
        TEXTURE2D(_AliveT6); TEXTURE2D(_AliveT7); TEXTURE2D(_AliveT8); TEXTURE2D(_AliveT9); TEXTURE2D(_AliveT10); TEXTURE2D(_AliveT11);
        TEXTURECUBE(_AliveC0); TEXTURECUBE(_AliveC1); TEXTURECUBE(_RfxReflectionCube); float _RfxReflectionOn;
        // the program textures share two inline sampler states (a material's twelve textures would exceed the sampler registers):
        // trilinear, clamped where the material's sampler clamps (bit k of _AliveWrap), else wrapped
        SAMPLER(sampler_trilinear_repeat); SAMPLER(sampler_trilinear_clamp);
        float4 AliveTex2D(TEXTURE2D_PARAM(t, unusedSampler), bool clampUV, float2 uv, float2 dx, float2 dy)
        {
            [branch] if (clampUV) return SAMPLE_TEXTURE2D_GRAD(t, sampler_trilinear_clamp, uv, dx, dy);
            return SAMPLE_TEXTURE2D_GRAD(t, sampler_trilinear_repeat, uv, dx, dy);
        }
        #define ALIVE_TEX(T) AliveTex2D(TEXTURE2D_ARGS(T, sampler_trilinear_repeat), clampUV, uv, dx, dy)
        float4 AliveFetch(int k, float2 uv, float2 dx, float2 dy)
        {
            bool clampUV = (((uint)round(_AliveWrap)) >> k & 1u) != 0u;
            [branch] switch (k)
            {
                case 0: return ALIVE_TEX(_AliveT0);
                case 1: return ALIVE_TEX(_AliveT1);
                case 2: return ALIVE_TEX(_AliveT2);
                case 3: return ALIVE_TEX(_AliveT3);
                case 4: return ALIVE_TEX(_AliveT4);
                case 5: return ALIVE_TEX(_AliveT5);
                case 6: return ALIVE_TEX(_AliveT6);
                case 7: return ALIVE_TEX(_AliveT7);
                case 8: return ALIVE_TEX(_AliveT8);
                case 9: return ALIVE_TEX(_AliveT9);
                case 10: return ALIVE_TEX(_AliveT10);
                default: return ALIVE_TEX(_AliveT11);
            }
        }
        float4 AliveCube(int k, float3 dir)
        {
            [branch] switch (k)
            {
                case 0: return SAMPLE_TEXTURECUBE_LOD(_AliveC0, sampler_trilinear_clamp, dir, 0);
                case 1: return SAMPLE_TEXTURECUBE_LOD(_AliveC1, sampler_trilinear_clamp, dir, 0);
                default: return _RfxReflectionOn > 0.5 ? SAMPLE_TEXTURECUBE_LOD(_RfxReflectionCube, sampler_trilinear_clamp, dir, 0) : 0;
            }
        }
        float4 AliveSwizzle(float4 v, int b) { return b >= 4 ? float4(v.rgb, 1.0) : v[min(b, 3)].xxxx; }
        float4 AliveParam(int k) { return k == 0 ? _AliveP0 : k == 1 ? _AliveP1 : k == 2 ? _AliveP2 : _AliveP3; }
        // uv: (uv0, uv1) in Unity's v (1 - v_d3d); the program scales D3D coordinates, so v_unity = 1 - (1 - v) s
        float4 AliveAlbedoProgram(float4 uv, float4 vcol, float3 P, float3 N)
        {
            float4 r[12];
            [unroll] for (int z = 0; z < 12; z++) r[z] = 0;
            float4 duv = float4(ddx(uv.xy), ddx(uv.zw)), vuv = float4(ddy(uv.xy), ddy(uv.zw));
            float dist = distance(P, _WorldSpaceCameraPos);
            int n = min((int)_AliveVMInfo.x, 48);
            [loop] for (int k = 0; k < n; k++)
            {
                float4 a = LOAD_TEXTURE2D(_AliveVMTex, int2(2 * k, 0));
                int code = (int)round(a.x), s0 = (int)round(a.z), s1 = (int)round(a.w);
                // the op's parameters only where it has any (fetch, constant, lerp): one texel read less for the others (uniform per draw)
                float4 p = 0;
                [branch] if (code == 0 || code == 2 || code == 8) p = LOAD_TEXTURE2D(_AliveVMTex, int2(2 * k + 1, 0));
                float4 v = 0;
                [branch] if (code == 0)
                {
                    int mode = (int)round(p.x);
                    float2 t, gx = 0, gy = 0;
                    if (mode <= 1)
                    {
                        float2 base = mode == 0 ? uv.xy : uv.zw;
                        float2 off = p.w > 0.5 ? AliveParam((int)round(p.w) - 1).xy : 0;
                        t = float2(base.x * p.y + off.x, 1.0 - ((1.0 - base.y) * p.z + off.y));
                        gx = (mode == 0 ? duv.xy : duv.zw) * p.yz; gy = (mode == 0 ? vuv.xy : vuv.zw) * p.yz;
                    }
                    else if (mode <= 3)
                    {
                        float s = saturate(p.y * dist);
                        float w = mode == 2 ? s * s * (3.0 - 2.0 * s) : s;
                        t = float2(w, 1.0 - w);
                    }
                    else
                    {
                        // 4: the view-angle ramp 1 - |N.V| (SX light beams, glows)
                        float w = 1.0 - abs(dot(normalize(_WorldSpaceCameraPos - P), N * rsqrt(max(dot(N, N), 1e-12))));
                        t = float2(w, 1.0 - w);
                    }
                    v = AliveSwizzle(AliveFetch(s0, t, gx, gy), s1);
                }
                else if (code == 1) v = AliveSwizzle(vcol, s1);
                else if (code == 2) v = p.xxxx;
                else if (code == 3) v = r[s0] + r[s1];
                else if (code == 4) v = r[s0] * r[s1];
                else if (code == 5) v = -r[s0];
                else if (code == 6) v = abs(r[s0]);
                else if (code == 7) v = saturate(r[s0]);
                else if (code == 8) v = lerp(r[s0], r[s1], r[(int)round(p.x)]);
                else if (code == 9) v = max(r[s0], r[s1]);
                else if (code == 10) v = min(r[s0], r[s1]);
                else v = AliveSwizzle(AliveParam(s0), s1);
                r[(int)round(a.y)] = v;
            }
            // .a: the program's own alpha (oC0.w) when it has one, else -1 (the material's alpha source applies)
            return float4(r[(int)round(_AliveVMInfo.y)].rgb, _AliveVMInfo.w > 0.5 ? r[(int)round(_AliveVMInfo.w) - 1].x : -1.0);
        }

        // MX vs ATV Alive: a whole level program (_re/formats/alive_general.py: programs the albedo reduction above does not cover: the
        // Dunes cliffs' normal-mapped grass blend, water / glass cube reflections, ...): scalar and float4 ops over 40 registers, the
        // interpolators as the client's own (P, N, T, B = T x N, UV in D3D's v, vertex colour, camera); returns the albedo (already
        // squared by the program; for an unlit program its colour) and the alpha (-1: none); emissive: the colour added after the
        // light (cube reflections), 0 without one; normal: the world normal of the material's normal pass (the light buffer's), else N.
        //   20 input a   21 r[a][b]   22 p0   23 add   24 mul   25 neg   26 abs   27 sat   28 max   29 min   30 rcp   31 rsq |x|
        //   32 sqrt |x|   33 frc   34 floor   35 exp2   36 log2 |x|   37 trunc   38 cnd (p1: == / >= / > 0) r[a] ? r[b] : r[p0]
        //   39 set (p0: == / >= / > / !=) r[a], r[b]   40 texture a (b 0 / 1: UV set x (p0, p1) + parameter p2 - 1; 2: (r[p0].x, r[p1].x))
        //   41 cube a at r[b].xyz   42 pack (r[a].x, r[b].x, r[p0].x, r[p1].x)   43 parameter a   44 (p0..p3)   45 r[a] swizzled p0..p3
        //   46 dot(r[a], r[b]) over p0 components
        float4 AliveGeneralProgram(float4 uv, float4 vcol, float3 P, float3 N, float3 T, float3 B, out float3 emissive, out float3 normal)
        {
            float4 r[40];
            [unroll] for (int z = 0; z < 40; z++) r[z] = 0;
            float4 duv = float4(ddx(uv.xy), ddx(uv.zw)), vuv = float4(ddy(uv.xy), ddy(uv.zw));
            float4 uvD = float4(uv.x, 1.0 - uv.y, uv.z, 1.0 - uv.w);
            int n = min((int)_AliveVMInfo.x, 512);
            [loop] for (int k = 0; k < n; k++)
            {
                float4 a = LOAD_TEXTURE2D(_AliveVMTex, int2(2 * k, 0));
                int code = (int)round(a.x), s0 = (int)round(a.z), s1 = (int)round(a.w);
                // the op's parameters only where it has any (22, 38..40, 42, 44..46): one texel read less for the others (uniform per draw)
                float4 p = 0;
                [branch] if (code == 22 || (code >= 38 && code <= 40) || code == 42 || code >= 44) p = LOAD_TEXTURE2D(_AliveVMTex, int2(2 * k + 1, 0));
                float4 x = r[s0], y = r[s1];
                float4 v = 0;
                [branch] switch (code)
                {
                    case 20: v = s0 == 0 ? float4(P, 1) : s0 == 1 ? float4(N, 0) : s0 == 2 ? float4(T, 0) : s0 == 3 ? float4(B, 0)
                               : s0 == 4 ? uvD : s0 == 5 ? vcol : float4(_WorldSpaceCameraPos, 1); break;
                    case 21: v = x[min(s1, 3)].xxxx; break;
                    case 22: v = p.xxxx; break;
                    case 23: v = x + y; break;
                    case 24: v = x * y; break;
                    case 25: v = -x; break;
                    case 26: v = abs(x); break;
                    case 27: v = saturate(x); break;
                    case 28: v = max(x, y); break;
                    case 29: v = min(x, y); break;
                    case 30: v = rcp(x); break;
                    case 31: v = rsqrt(abs(x)); break;
                    case 32: v = sqrt(abs(x)); break;
                    case 33: v = frac(x); break;
                    case 34: v = floor(x); break;
                    case 35: v = exp2(x); break;
                    case 36: v = log2(abs(x)); break;
                    case 37: v = trunc(x); break;
                    case 38:
                    {
                        int kind = (int)round(p.y);
                        float4 c = kind == 0 ? float4(x == 0) : kind == 1 ? float4(x >= 0) : float4(x > 0);
                        v = lerp(r[(int)round(p.x)], y, c);
                        break;
                    }
                    case 39:
                    {
                        int kind = (int)round(p.x);
                        v = kind == 0 ? float4(x == y) : kind == 1 ? float4(x >= y) : kind == 2 ? float4(x > y) : float4(x != y);
                        break;
                    }
                    case 40:
                    {
                        float2 t, gx = 0, gy = 0;
                        if (s1 <= 1)
                        {
                            float2 base = s1 == 0 ? uv.xy : uv.zw;
                            float2 off = p.z > 0.5 ? AliveParam((int)round(p.z) - 1).xy : 0;
                            t = float2(base.x * p.x + off.x, 1.0 - ((1.0 - base.y) * p.y + off.y));
                            gx = (s1 == 0 ? duv.xy : duv.zw) * p.xy; gy = (s1 == 0 ? vuv.xy : vuv.zw) * p.xy;
                        }
                        else t = float2(r[(int)round(p.x)].x, 1.0 - r[(int)round(p.y)].x);   // a computed coordinate: mip 0
                        v = AliveFetch(s0, t, gx, gy);
                        break;
                    }
                    case 41: v = AliveCube(s0, y.xyz); break;
                    case 42: v = float4(x.x, y.x, r[(int)round(p.x)].x, r[(int)round(p.y)].x); break;
                    case 43: v = AliveParam(s0); break;
                    case 44: v = p; break;
                    case 45: v = float4(x[(int)round(p.x)], x[(int)round(p.y)], x[(int)round(p.z)], x[(int)round(p.w)]); break;
                    default:
                    {
                        int m = (int)round(p.x);
                        v = (m >= 4 ? dot(x, y) : m == 3 ? dot(x.xyz, y.xyz) : dot(x.xy, y.xy)).xxxx;
                        break;
                    }
                }
                r[(int)round(a.y)] = v;
            }
            emissive = _AliveVMInfo2.y > 0.5 ? r[(int)round(_AliveVMInfo2.y) - 1].rgb : 0;
            normal = _AliveVMInfo2.z > 0.5 ? r[(int)round(_AliveVMInfo2.z) - 1].xyz : N;
            return float4(r[(int)round(_AliveVMInfo.y)].rgb, _AliveVMInfo.w > 0.5 ? r[(int)round(_AliveVMInfo.w) - 1].x : -1.0);
        }

        // gTextureOffset (c85) of a ScrollingTextureEntity (per renderer, OriginalProps: xy offset, z 1 when set), added to uv0 by the
        // scrolling templates' vertex programs (Template_Animated_UV_nonlit_alpha 050c: add o3.xy, c85, v3); Unity v = 1 - v_d3d
        float4 _RfxTextureOffset;
        float2 DiffuseUV(float4 uv)
        {
            float2 t = (_DiffuseUV > 0.5 ? uv.zw : uv.xy + (_RfxTextureOffset.z > 0.5 ? float2(_RfxTextureOffset.x, -_RfxTextureOffset.y) : 0)) * _UVScale.xy;
            if (_FsParams2.y > 0.5)   // FF_Chainlink_Fence: dp2add of the D3D coordinates
            {
                float2 d = float2(t.x, 1.0 - t.y);
                t = float2(dot(_UVMatrix.xy, d), 1.0 - dot(_UVMatrix.zw, d));
            }
            return t;
        }
        float AlphaOf(float4 tex, float4 vcol, float2 uv)
        {
            if (_AlphaSource > 8.5) return saturate(tex.a) * vcol.a;          // Template_Animated_UV_nonlit_alpha: sat(s0.a) v8.w
            if (_AlphaSource > 7.5) return saturate(vcol.r);                  // Template_Prelit_Color_Dim_Ctl: sat(v2.x)
            if (_AlphaSource > 6.5) return saturate(tex.r * vcol.r);          // Caustics: sat(s0.x v2.x)
            if (_AlphaSource > 5.5) return saturate(_FsParams.w);             // a constant (usr_opacity, usr_transperancy)
            if (_AlphaSource > 4.5) return saturate(1.0 - vcol.a);
            if (_AlphaSource > 3.5) return saturate(tex.a - vcol.a);
            if (_AlphaSource > 2.5) return saturate(SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, uv).a);
            return _AlphaSource > 1.5 ? saturate(vcol.a) : _AlphaSource > 0.5 ? saturate(tex.a) : 1.0;
        }
        void AlphaTest(float a)
        {
            if (_AlphaTest > 0.5) clip(a * 255.0 - _Cutoff * 255.0 + (_AlphaFuncGE > 0.5 ? 0.5 : -0.5));
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
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile_fragment _ _SHADOWS_SOFT _SHADOWS_SOFT_LOW _SHADOWS_SOFT_MEDIUM _SHADOWS_SOFT_HIGH
            #pragma multi_compile_fragment _ _RFX_ENHANCED
            // MX vs ATV Alive programs: _ALIVE_VM the albedo programs (<= 48 ops, 12 registers), _ALIVE_VM_GENERAL the whole programs (<= 512 ops,
            // 40 registers). Separate variants: the general interpreter's register file (40 float4) set the register budget, and so the
            // occupancy, of every albedo-program draw too (SX_DLC_SX01: the albedo programs cost 2.3 ms of 8 ms at 1080p on an M5)
            #pragma multi_compile_local_fragment _ _ALIVE_VM _ALIVE_VM_GENERAL
            // _ALPHATEST_ON only on the alpha-tested templates (ReflexSceneryMaterial): a fragment program that can discard loses early
            // depth / hidden-surface removal for every pixel of the draw, opaque or not
            #pragma multi_compile_local_fragment _ _ALPHATEST_ON
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "ReflexEnhanced.hlsl"
            #include "ReflexShadows.hlsl"
            #include "ReflexVolumetricApply.hlsl"

            struct Attributes
            {
                float4 positionOS : POSITION; float3 normalOS : NORMAL; float4 tangentOS : TANGENT;
                float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float4 color : COLOR;
            };
            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float3 positionWS : TEXCOORD0;
                float3 normalWS : TEXCOORD1;        // un-normalised world normal (the original interpolates it as is)
                float3 tangentWS : TEXCOORD2;
                float3 binormalWS : TEXCOORD7;      // T x N of the vertex program (Template_Trackside_* 056c..0650)
                float4 uv : TEXCOORD3;              // uv0, uv1
                float4 color : TEXCOORD4;
                float3 ambient : TEXCOORD5;         // per-vertex ambient (vertex programs)
                float4 fog : TEXCOORD6;             // rgb: amount * fog colour, w: 1 - amount
            };

            Varyings Vert(Attributes v)
            {
                Varyings o;
                o.positionWS = TransformObjectToWorld(v.positionOS.xyz);
                o.positionCS = TransformWorldToHClip(o.positionWS);
                float3x3 m = (float3x3)GetObjectToWorldMatrix();
                o.normalWS = mul(m, v.normalOS);
                o.tangentWS = mul(m, v.tangentOS.xyz);
                o.binormalWS = cross(o.tangentWS, o.normalWS);
                o.uv = float4(v.uv0, v.uv1);
                o.color = v.color;
                o.ambient = RfxAmbient(o.normalWS);
                o.fog = RfxFog(o.positionWS, _WorldSpaceCameraPos);
                return o;
            }

            float3 DecodeNormalRG(float2 rg)
            {
                float2 xy = rg * 2.0 - 1.0;
                return float3(xy, sqrt(abs(1.0 - xy.x * xy.x - xy.y * xy.y)));   // rcp(rsq(x)): rsq takes |x|
            }
            float Overlay1(float t1, float t2) { float p = t1 * t2; return 2.0 * (p + saturate(t1) * (t1 + t2 - 2.0 * p - 0.5)); }
            float3 Overlay(float3 t1, float3 t2) { return float3(Overlay1(t1.x, t2.x), Overlay1(t1.y, t2.y), Overlay1(t1.z, t2.z)); }
            // Texture upload flips v. Scale the original D3D UV first, then flip, including non-integer decal scales.
            float2 CliffUV(float2 uv, float2 scale) { return float2(uv.x * scale.x, 1.0 - (1.0 - uv.y) * scale.y); }

            // ambient of the pixel programs: the four fill lights + sat(-N.y) lerp(ref.rgb, bounce, ref.w) with the s13 texel
            float3 AmbientPixel(float3 N, float3 P)
            {
                float4 cref = RfxColorRefAt(P);
                float3 a = saturate(dot(_RfxLightDir1.xyz, N)) * _RfxLightColor1.rgb;
                a += saturate(dot(_RfxLightDir0.xyz, N)) * _RfxLightColor0.rgb;
                a += saturate(dot(_RfxLightDir2.xyz, N)) * _RfxLightColor2.rgb;
                a += saturate(dot(_RfxLightDir3.xyz, N)) * _RfxLightColor3.rgb;
                float3 bounce = cref.w * _RfxBounceColor.rgb - (cref.w - 1.0) * cref.rgb;
                return bounce * saturate(-N.y) + a;
            }

            half4 Frag(Varyings i, FRONT_FACE_TYPE isFront : FRONT_FACE_SEMANTIC) : SV_Target
            {
                RfxModelDither(i.positionCS.xy);
                float3 P = i.positionWS;
                float facing = IS_FRONT_VFACE(isFront, 1.0, -1.0);
                float2 uvD = DiffuseUV(i.uv), uv1 = i.uv.zw;
                float4 tex = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, uvD);
                float3 colour;
                float alpha = 1.0;
            #if defined(_RFX_ENHANCED)
                // LIGHTING: ENHANCED (not the original program): screen-space AO on the ambient term, contact shadows on the sun term,
                // every lit object receives the real-time sun shadow, the sky's colour on the ambient, a Fresnel sky reflection
                float2 rfxOcc = RfxEnhOcclusion(i.positionCS);
                bool rfxLit = false;
            #endif

                // ---------------------------------------------------------------- unlit / prelit templates
            #if defined(_ALIVE_VM) || defined(_ALIVE_VM_GENERAL)
                if (_AliveVMInfo.z > 0.5)   // an unlit MX vs ATV Alive program (prelit billboards, fences, glows): its colour before fog
                {
                    float3 aliveEm, aliveN;
                #if defined(_ALIVE_VM_GENERAL)
                    float4 prog = AliveGeneralProgram(i.uv, i.color, P, i.normalWS, i.tangentWS, i.binormalWS, aliveEm, aliveN);
                #else
                    float4 prog = AliveAlbedoProgram(i.uv, i.color, P, i.normalWS);
                #endif
                    colour = RfxIsAlive() ? prog.rgb : sqrt(max(prog.rgb, 0.0));   // ALIVE LOOK REFLEX: Alive's linear colour as a display value
                    alpha = prog.a >= 0.0 ? saturate(prog.a) : AlphaOf(tex, i.color, uvD);
                }
                else
            #endif
                if (_Mode > 22.5 && _Mode < 23.5)
                {
                    // w-fsshaders unlit arena programs (Template_Prelit_VertexColor, the FunFactory grates / fences / wires / lights,
                    // Template_Prelit_TR, the two-tone bricks and Template_Sx_sp_hills): diffuse (or lerp(detail, diffuse, vertex alpha))
                    // x AO map (UV1) x vertex colour x power
                    float3 a = tex.rgb;
                    if (_FsParams2.x > 0.5)
                    {
                        // the detail at UV0, or (hills) at 5 x the model-space position through the volume-shadow projection (VS 0x: dp4 o5, v0, c39 / c40)
                        float2 tuv = i.uv.xy;
                        if (_FsParams2.x > 1.5) tuv = 5.0 * (0.5 + _RfxVolumeShadowParams.x * mul(GetWorldToObjectMatrix(), float4(P, 1.0)).xz);
                        a = lerp(SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, tuv).rgb, tex.rgb, i.color.a);
                    }
                    if (_AOMode > 0.5) a *= SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, uv1).rgb;
                    if (_FsParams.x > 1.5) a *= i.color.xzy; else if (_FsParams.x > 0.5) a *= i.color.rgb;
                    colour = a * _PrelitPower;
                    alpha = AlphaOf(tex, i.color, uvD);
                }
                else if (_Mode > 24.5 && _Mode < 25.5)
                {
                    // Template_Prelit_Banners 01a0..0238: w = s0 (UV0) the wrinkle map; ((1 - lightness)(w - 1) + 1) lrp(w, usr_light_color, s1 (UV1))
                    float3 w = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).rgb;
                    colour = ((1.0 - _PrelitPower) * (w - 1.0) + 1.0) * lerp(tex.rgb, _BaseColor.rgb, w);
                }
                else if (_Mode > 25.5 && _Mode < 26.5)
                {
                    // Template_Prelit_Color_Dim_Ctl 01b8..0294: g = s0 at 1 - |V.N|; g (usr_dim_amount - C P) + C P (C usr_constant_color,
                    // P usr_prelit_power); alpha sat(vertex colour r)
                    float3 V = normalize(_WorldSpaceCameraPos - P);
                    float gx = 1.0 - abs(dot(V, normalize(i.normalWS)));
                    float3 g = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, float2(gx, 1.0 - gx)).rgb;
                    float3 cp = _BaseColor.rgb * _PrelitPower;
                    colour = g * (_FsColor.rgb - cp) + cp;
                    alpha = AlphaOf(tex, i.color, uvD);
                }
                else if (_Mode > 0.5 && _Mode < 1.5)  // Template_Prelit_Map_Ctl: tex * usr_prelit_power
                {
                    colour = (_EncodeBase > 0.5 ? LinearToSRGB(tex.rgb) : tex.rgb) * _PrelitPower;
                }
                else if (_Mode > 6.5 && _Mode < 7.5)  // Template_Prelit: tex(uv0) * prelit(uv1)
                {
                    colour = tex.rgb * SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, uv1).rgb;
                }
                else if (_Mode > 7.5 && _Mode < 8.5)  // Template_Prelit_Stadiums: tex * pre + pre.x * usr_brightness
                {
                    float3 pre = SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, uv1).rgb;
                    colour = tex.rgb * pre + pre.x * _PrelitPower;
                }
                else if (_Mode > 8.5 && _Mode < 9.5)  // Template_Prelit_Color_Ctl: usr_constant_color * usr_prelit_power
                {
                    colour = _BaseColor.rgb * _PrelitPower;
                }
                else if (_Mode > 9.5 && _Mode < 10.5)  // Template_Prelit_Background: tex, alpha sat(tex.a)
                {
                    colour = tex.rgb; alpha = saturate(tex.a);
                }
                else if (_Mode > 10.5 && _Mode < 11.5)  // Template_Prelit_VXC_TR: tex * vertex rgb, alpha sat(transparency.a)
                {
                    colour = tex.rgb * i.color.rgb;
                    alpha = saturate(SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, uvD).a);
                }
                else if (_Mode > 16.5 && _Mode < 17.5)  // Template_Prelit_VXC: tex(uv0) * vertex rgb, alpha 1 (no lighting: the vertex colours are the prelight)
                {
                    colour = tex.rgb * i.color.rgb;
                }
                else if (_Mode > 17.5 && _Mode < 19.5)
                {
                    // Template_AnimChevron_Ctrl (the MotoSkills chevron walls / arrows; 0194..0280): s0 = HDR gradient at uv0 + gTextureOffset (the scroll),
                    // s1 = chevron at uv0 (not scrolled). rgb = chevron (1 + a (gradient - 1)); alpha = sat((1 - smoothstep(sat((120 - d) / 100))) a), d = the
                    // camera distance: the wall is gone within 20 units of the camera and whole from 120 on. No lighting.
                    // Template_AnimChevron_NoFadeCtrl (_Mode 19; the challenges' chevrons: Target Practice's GL_TargetPractice; same vertex program, pixel program
                    // 0128..017c): the same rgb, alpha = sat(chevron.a) (no gCameraPos, no distance term).
                    float2 off = _RfxScrollOverride.z > 0.5 ? _RfxScrollOverride.xy :
                        float2(_ScrollParams.x > 0 ? frac(_Time.y / _ScrollParams.x) : 0, _ScrollParams.y > 0 ? frac(_Time.y / _ScrollParams.y) : 0) * (1.0 - 2.0 * _ScrollParams.zw);
                    float4 chevron = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy);
                    float3 grad = SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, i.uv.xy + float2(off.x, -off.y)).rgb;
                    colour = chevron.rgb + chevron.a * (grad * chevron.rgb - chevron.rgb);
                    if (_Mode > 18.5)
                        alpha = saturate(chevron.a);
                    else
                    {
                        float x = saturate((distance(_WorldSpaceCameraPos, P) - 120.0) * -0.00999999978);
                        alpha = saturate((1.0 - x * x * (3.0 - 2.0 * x)) * chevron.a);
                    }
                }
                else if (_Mode > 11.5 && _Mode < 12.5)
                {
                    // SX_KI_LightBeam (ScrollingTextureRSData): rgb = dust(uv0 + gTextureOffset) * (1 - fog) * 5 + fog;
                    // alpha = sat((1 - gradient(1 - |V.N|).x) * beam(uv0).x)  (0198..0264)
                    float2 off = _RfxScrollOverride.z > 0.5 ? _RfxScrollOverride.xy :
                        float2(_ScrollParams.x > 0 ? frac(_Time.y / _ScrollParams.x) : 0, _ScrollParams.y > 0 ? frac(_Time.y / _ScrollParams.y) : 0) * (1.0 - 2.0 * _ScrollParams.zw);
                    float3 dust = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy + float2(off.x, -off.y)).rgb;
                    float3 V = normalize(_WorldSpaceCameraPos - P);
                    float g = 1.0 - abs(dot(V, normalize(i.normalWS)));
                    float grad = SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, float2(g, 1.0 - g)).x;
                    float beam = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).x;
                    alpha = saturate((1.0 - grad) * beam);
                    return half4(RfxOutput(dust * i.fog.w * 5.0 + i.fog.rgb), alpha);
                }
                else if (_Mode > 14.5 && _Mode < 15.5)
                {
                    // NAT_RE_B_Mountains 00cc..0120: doubled original UV, squared diffuse, *2, vertex fog.
                    float3 mountain = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, CliffUV(i.uv.xy, float2(2, 2))).rgb;
                    colour = (mountain * mountain) * 2.0;
                }
                else if (_Mode > 2.5 && _Mode < 4.5)
                {
                    // Alpine edge of the world: t = .25 + .5 d (d = the dirt texture's red at 256 x uv); c = base; rgb = 2 (p + sat(c) q)
                    float d = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy * 256.0).r;
                    float3 c = tex.rgb; float t = 0.25 + 0.5 * d;
                    float3 p = c * t; float3 q = c + t - 2.0 * p - 0.5;
                    colour = 2.0 * (p + saturate(c) * q);
                    alpha = _Mode > 3.5 ? saturate(i.color.a) : 1.0;
                }
                else if (_Mode > 4.5 && _Mode < 5.5) { colour = tex.rgb * i.color.rgb; alpha = saturate(tex.a); }   // FR_AL_EoW_RivBirchLrg
                else
                {
                    // ---------------------------------------------------------------- lit templates
                    float3 Ng = normalize(i.normalWS);
                    float3 N = Ng;
                    bool rock = _Mode > 1.5 && _Mode < 2.5;
                    bool gibsonCliff = _Mode > 12.5 && _Mode < 13.5;
                    bool renneEdge = _Mode > 13.5 && _Mode < 14.5;
                    if (renneEdge)
                    {
                        // NAT_RE_[AB]_EOW 0474..0540: normal at original UV0 * 32, direct T/N/B transform.
                        float3 nt = DecodeNormalRG(SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, CliffUV(i.uv.xy, float2(32, 32))).xy);
                        N = normalize(nt.x * normalize(i.tangentWS) + nt.z * Ng + nt.y * normalize(i.binormalWS));
                    }
                    else if (gibsonCliff)
                    {
                        // FR_GI_CliffRock*_NT 0724..0920: detail at (3u,3v), decal at (.1u,.2v).
                        // The encoded-normal lerps reduce to the mean of the two decoded tangent normals.
                        float3 nd = DecodeNormalRG(SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, CliffUV(i.uv.xy, float2(3, 3))).xy);
                        float3 nc = DecodeNormalRG(SAMPLE_TEXTURE2D(_DecalNormalMap, sampler_DecalNormalMap, CliffUV(i.uv.xy, float2(0.1, 0.2))).xy);
                        float3 nt = (nd + nc) * 0.5;
                        N = normalize(nt.x * normalize(i.tangentWS) + nt.y * normalize(i.binormalWS) + nt.z * Ng);
                    }
                    else if (rock)
                    {
                        float3 T = normalize(i.tangentWS), B = normalize(i.binormalWS);
                        // two-scale normal map blended as colours (overlay) in n * .5 + .5 space, then decoded
                        float2 r1 = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.xy).xy;
                        float2 r2 = SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, i.uv.xy * 0.3).xy;
                        float3 n1 = DecodeNormalRG(r1), n2 = DecodeNormalRG(r2);
                        float3 t1 = float3(2.0 * r1, n1.z + 1.0) * 0.5, t2 = float3(2.0 * r2, n2.z + 1.0) * 0.5;
                        float3 nt = Overlay(t1, t2) * 2.0 - 1.0;
                        N = normalize(nt.x * T + nt.y * B + nt.z * Ng);
                    }
                    else if (_NormalMode > 0.5)
                    {
                        float3 T = normalize(i.tangentWS), B = normalize(i.binormalWS);
                        float2 nuv = (_NormalUV > 0.5 ? i.uv.zw : i.uv.xy) * _UVScale.xy;
                        float3 n = DecodeNormalRG(SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, nuv).xy);
                        float3 Np = n.x * T + n.z * Ng + n.y * B;
                        if (_NormalMode < 1.5)
                        {
                            float3 Nn = normalize(Np);
                            N = normalize((Nn - dot(Nn, Ng) * Ng) * _NormalAmount + Ng);
                        }
                        else N = normalize(Np);
                    }
                    if (_TwoSided > 0.5) N *= facing;

                    // --- albedo ---
                    float3 D = tex.rgb * _DiffuseFade;
                    if (_Mode > 15.5 && _Mode < 16.5)
                    {
                        // Template_DF_X2_VXB_AO_NM 0a1c..0a3c: v4 is the unchanged
                        // vertex RGB (VS0674); each channel blends s2 toward s1 at UV0.
                        D = lerp(tex.rgb, SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).rgb, i.color.rgb);
                    }
                    if (_Mode > 19.5 && _Mode < 20.5)
                    {
                        // Template_DF_X2_AO / _SP (the FMX roll-in and quarter-pipe tops; DF_X2_AO_SP 07b4..07e4): s3 the base at UV0, s1 the
                        // second diffuse at UV1 over it by s2's rgb (UV1), per channel: lrp r5.xyz, s2, s1, s3
                        float3 over = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, uv1).rgb;
                        float3 mask = SAMPLE_TEXTURE2D(_ComboMap, sampler_ComboMap, uv1).rgb;
                        D = lerp(tex.rgb, over, mask);
                    }
                    else if (_Mode > 20.5 && _Mode < 21.5)
                    {
                        // FMX_DO_Ramp360 / FMX_DO_RampS_LL / FMX_DO_RampFMX_LL (the FMX ramps' tops, 0694..06b4): the ramp texture at UV0 plus
                        // the chevrons at UV1 (s0); the output alpha is a texture's alpha at UV0 (_AlphaSource)
                        D = tex.rgb + SAMPLE_TEXTURE2D(_ComboMap, sampler_ComboMap, uv1).rgb;
                    }
                    else if (_Mode > 21.5 && _Mode < 22.5)
                    {
                        // FMX_RampTop_Dover_SH. (Dover B's ramp tops): s1 * s0 * 1.3, both at UV0
                        D = tex.rgb * SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).rgb * 1.29999995;
                    }
                    if (_DiffuseSrc > 0.5 && _DiffuseSrc < 1.5) D = i.color.rgb * -D + D;                       // DF_VX: D (1 - vertex rgb)
                    else if (_DiffuseSrc > 1.5 && _DiffuseSrc < 2.5) D = _BaseColor.rgb;                         // usr_diffuse_color / usr_color
                    else if (_DiffuseSrc > 2.5 && _DiffuseSrc < 3.5) D = 0.400000006 * i.color.xzy;             // SX_Kingston_Rough_White
                    else if (_DiffuseSrc > 4.5 && _DiffuseSrc < 5.5) D = tex.rgb * _BaseColor.rgb;               // FMX_DO_CargoShipSS: tex x a literal
                    else if (_DiffuseSrc > 5.5 && _DiffuseSrc < 6.5)                                             // Template_Tropy_Cup: tex usr_color (0.2 + 1.1 |V.N|^3)
                    {
                        float vn = abs(dot(normalize(_WorldSpaceCameraPos - P), N));
                        D = tex.rgb * (_BaseColor.rgb * (vn * vn * vn) * 1.10000002 + _BaseColor.rgb * 0.200000003);
                    }
                    float mudSpec = 1.0;
                    if (_DiffuseSrc > 3.5 && _DiffuseSrc < 4.5)                                                   // Template_Trackside_bayle
                    {
                        float m = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).x * 0.400000006;
                        D = lerp(tex.rgb, _MudColor.rgb, m);
                        mudSpec = 1.0 - m;
                    }
                    if (rock)
                    {
                        float3 b = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv.xy * 4.0).rgb;
                        D = Overlay(lerp(tex.rgb, b, 0.45), i.color.rgb);
                    }
                    if (gibsonCliff)
                    {
                        // 094c..0a68: overlay(decal, detail3), then overlay that result with detail6.
                        float3 detail3 = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, CliffUV(i.uv.xy, float2(3, 3))).rgb;
                        float3 decal = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, CliffUV(i.uv.xy, float2(0.1, 0.2))).rgb;
                        float3 detail6 = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, CliffUV(i.uv.xy, float2(6, 6))).rgb;
                        D = Overlay(Overlay(decal, detail3), detail6);
                    }
                    if (renneEdge)
                    {
                        // 06bc..0720: s0 is a control image, not albedo. Green selects light/track dirt;
                        // green * .75 and blue modulate the result. Both dirt samplers use original UV0 * 64.
                        float2 dirtUV = CliffUV(i.uv.xy, float2(64, 64));
                        float3 lightDirt = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, dirtUV).rgb;
                        float3 trackDirt = SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, dirtUV).rgb;
                        float3 overlay = SAMPLE_TEXTURE2D(_ComboMap, sampler_ComboMap, i.uv.xy).rgb;
                        float scale = overlay.g * 0.75;
                        D = scale * lerp(trackDirt, lightDirt, overlay.g);
                        D = overlay.b * D;
                    }
                    float3 aoS = 1.0;
                    if (_AOMode > 0.5)
                    {
                        float3 ao = SAMPLE_TEXTURE2D(_AOMap, sampler_AOMap, _FsParams.y > 0.5 ? i.uv.xy : uv1).rgb;   // UV1 (the Trackside_Tent programs: UV0)
                        aoS = ao;
                        if (_AOMode < 1.5) D = (1.0 - ao) * _AOAmount * -D + D;          // 1 - (1 - ao) k
                        else if (_AOMode < 2.5) D = D * ao;                              // ao
                        else if (_AOMode < 3.5) D = (ao * -_AOAmount + 1.0) * D;         // 1 - ao k
                        else if (_AOMode < 4.5) D = D * ao * _AOAmount;                  // ao k (Template_DF_VXC_AO_NM)
                        else D = ao * D + (1.0 - ao) * (1.0 - _AOAmount);                // Template_Trackside_Tent: lrp(1 - ao, 1 - k, D)
                    }
                    if (_FsParams.x > 1.5) D *= i.color.xzy; else if (_FsParams.x > 0.5) D *= i.color.rgb;   // vertex colour (VXC programs)
                    if (_Mode > 5.5 && _Mode < 6.5)
                    {
                        // Template_ATV_MX_Engine_FNL: D1 = lerp(D, D * (0.3 + 1.45 |V.N|^3), combo.x); D = lerp(D1, mudColour * noise(15 uv), sat(combo.y + mud - 1))
                        float3 V0 = normalize(_WorldSpaceCameraPos - P);
                        float f = abs(dot(V0, N)); f = f * f * f;
                        float3 D0 = tex.rgb;
                        float4 combo = SAMPLE_TEXTURE2D(_ComboMap, sampler_ComboMap, i.uv.xy);
                        float3 D1 = lerp(D0, D0 * f * 1.45 + D0 * 0.3, combo.x);
                        float mudAmt = saturate(combo.y + _MudAmount - 1.0);
                        float3 mud = _MudColor.rgb * SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy * 15.0).rgb;
                        D = lerp(D1, mud, mudAmt);
                    }
                    alpha = AlphaOf(tex, i.color, uvD);

                    // --- shadows ---
                    float shadow = 1.0;
                #if defined(_RFX_ENHANCED)
                    rfxLit = true;
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
                        shadow = RfxMainLightShadow(shadowCoord);          // stand-in for the original's VSM (casters: dynamic objects only)
                    }
                    if (_UseVolShadow > 0.5) shadow *= RfxVolumeShadow(P);
                #if defined(_RFX_ENHANCED)
                    shadow *= rfxOcc.g;
                #endif

                    // --- lighting ---
                    // MX vs ATV Alive (T_Diffuse*, T_*VColor, FR_HI_*: SharedPixelShader): albedo = (tex x vertex colour)^2, lit by the
                    // light buffer (RfxAliveAmbient + the sun term below)
                    if (_AliveVColor > 0.5) D *= i.color.rgb;
                    float3 aliveEmissive = 0;
                #if defined(_ALIVE_VM) || defined(_ALIVE_VM_GENERAL)
                    float3 aliveN = N;
                #if defined(_ALIVE_VM_GENERAL)
                    const bool aliveGeneral = true;
                    float4 prog = AliveGeneralProgram(i.uv, i.color, P, i.normalWS, i.tangentWS, i.binormalWS, aliveEmissive, aliveN);
                #else
                    const bool aliveGeneral = false;
                    float4 prog = AliveAlbedoProgram(i.uv, i.color, P, N);
                #endif
                    D = prog.rgb;
                    if (prog.a >= 0.0) alpha = saturate(prog.a);
                    if (aliveGeneral && _AliveVMInfo2.z > 0.5) N = aliveN * rsqrt(max(dot(aliveN, aliveN), 1e-12));   // its normal pass
                    if (aliveGeneral)                   // a whole program's albedo is already squared (ALIVE LOOK REFLEX: undone)
                    {
                        if (!RfxIsAlive()) { D = sqrt(max(D, 0.0)); aliveEmissive = sqrt(max(aliveEmissive, 0.0)); }
                    }
                    else
                #endif
                    D = RfxAliveAlbedo(D);
                    float3 rad = _FsParams3.y > 0.5 ? 1.0 : RfxRadiosityAt(P);   // (Template_Animated_UV_nonlit_alpha: no colour reference term)
                    float3 ambient = RfxAmbientAt(_AmbientSrc > 0.5 ? AmbientPixel(N, P) : i.ambient, N, P);
                #if defined(_RFX_ENHANCED)
                    ambient = RfxEnhAmbient(ambient, N, rfxOcc.r);
                #endif
                    float3 sun = (_ShadowLit > 0.5 ? shadow : 1.0) * RfxMapShadow(P) * rad * _RfxSunColor.rgb * saturate(dot(_RfxSunDir.xyz, N));
                    colour = D * ambient + sun * D + _EmissiveAdd + aliveEmissive + D * _FsParams3.x;

                    float3 V = normalize(_WorldSpaceCameraPos - P);
                    float3 Rm = V - 2.0 * dot(V, N) * N;                 // -R
                    float3 L = normalize(_RfxSunDir.xyz);
                    float minShadow = min(_ShadowLit > 0.5 ? shadow : 1.0, saturate(dot(L, N)));
                    if (_SSS > 0.0)
                    {
                        float t = min(dot(_RfxSunDir.xyz, -Rm), 0.0);
                        float3 dd = D * _SSS;
                        colour += (t * t * -t) * (dd * dd);
                    }
                    if (_SpecParams.w > 0.5)
                    {
                        float sp = pow(saturate(dot(L, -Rm)), _SpecParams.x);
                        float3 s = sp * rad;
                        if (_SpecParams.z > 0.5) s *= _RfxSunColor.rgb;
                        float3 specMask = _FsParams.z > 2.5 ? tex.rgb : _FsParams.z > 1.5 ? aoS : _FsParams.z > 0.5 ? SAMPLE_TEXTURE2D(_DetailMap, sampler_DetailMap, i.uv.xy).rgb : 1.0;
                        colour += _SpecTint.rgb * (s * minShadow) * (_SpecParams.y * mudSpec) * specMask;
                    }
                    if (_EnvParams.y > 0.5)
                    {
                        // the material's cube at LOD 0 (the earlier templates), or as texldb at the program's bias (w-fsshaders: the material's cube,
                        // or the level's s14 samplerReflectionMap: black without one)
                        float3 env;
                        if (_EnvParams.w > 1.5) env = _RfxReflectionOn > 0.5 ? SAMPLE_TEXTURECUBE_BIAS(_RfxReflectionCube, sampler_trilinear_clamp, -Rm, _FsParams2.z).rgb : 0;
                        else if (_EnvParams.w > 0.5) env = SAMPLE_TEXTURECUBE_BIAS(_EnvCube, sampler_EnvCube, -Rm, _FsParams2.z).rgb;
                        else env = SAMPLE_TEXTURECUBE_LOD(_EnvCube, sampler_EnvCube, -Rm, 0).rgb;
                        float3 envMask = _EnvParams.z > 2.5 ? tex.a : _EnvParams.z > 1.5 ? tex.a * aoS                                                                   // windows02: s1.a s0
                                       : _EnvParams.z > 0.5 ? SAMPLE_TEXTURE2D(_ComboMap, sampler_ComboMap, i.uv.xy).rgb * _FsColor.rgb : 1.0;   // RF_A: mask x tint
                        colour += env * (_FsParams2.w > 0.5 ? 1.0 : minShadow * 0.5 + 0.5) * envMask * _EnvParams.x;
                    }
                #if defined(_RFX_ENHANCED)
                    // glossy materials (the template has a specular or reflection term) reflect the enhanced sky with Fresnel; the
                    // roughness follows the specular hardness (pow 1..256)
                    if (_SpecParams.w > 0.5 || _EnvParams.y > 0.5)
                    {
                        float rough = _SpecParams.w > 0.5 ? saturate(1.0 - log2(max(_SpecParams.x, 1.0)) / 8.0) : 0.35;
                        float k = RfxEnhFresnel(N, V) * _RfxEnhParams2.x * (_EnvParams.y > 0.5 ? 1.0 : 0.5) * mudSpec;
                        colour += RfxEnhSkyReflection(-Rm, rough) * k * (minShadow * 0.5 + 0.5) * lerp(1.0, rfxOcc.r, 0.75);
                    }
                #endif
                }
            #if defined(_RFX_ENHANCED)
                // prelit / baked templates keep their light; the AO only grounds them (not the light sources: modes 1, 9, 12, 18, 19, 26)
                if (!rfxLit && _Mode > 1.5 && !(_Mode > 8.5 && _Mode < 9.5) && !(_Mode > 17.5 && _Mode < 19.5) && !(_Mode > 25.5 && _Mode < 26.5)) colour *= lerp(1.0, rfxOcc.r, _RfxEnhParams.z);
            #endif

            #if defined(_ALPHATEST_ON)
                AlphaTest(alpha);
            #endif
                colour = colour * i.fog.w + i.fog.rgb;
                float outAlpha = (_RfxFadeOpaque > 0.5 ? 1.0 : alpha) * _RfxModelAlpha;
                // VOLUMETRIC LIGHT: a blended material (the Transparent queue: ReflexSceneryMaterial, the fading clones) draws after the medium's
                // composite and takes the medium in front of it here (ReflexVolumetricApply.hlsl)
                if (_SrcBlend != 1.0 || _DstBlend != 0.0) colour = RfxVolApply(colour, outAlpha, i.positionWS, i.positionCS, _SrcBlend, _DstBlend);
                return half4(RfxOutput(colour), outAlpha);
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
            #pragma target 3.0
            #pragma vertex ShadowVert
            #pragma fragment ShadowFrag
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW
            // _ALPHATEST_ON (ReflexSceneryMaterial: the template's alpha test): only those materials sample and clip here; a fragment
            // program that can discard turns off early depth on PC GPUs for the whole draw (shadow cascades, depth pre-pass)
            #pragma multi_compile_local_fragment _ _ALPHATEST_ON
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            float3 _LightDirection;
            float3 _LightPosition;
            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float4 color : TEXCOORD1; };

            Varyings ShadowVert(Attributes v)
            {
                Varyings o;
                float3 positionWS = TransformObjectToWorld(v.positionOS.xyz);
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
                o.positionCS = positionCS; o.uv = DiffuseUV(float4(v.uv0, v.uv1)); o.color = v.color;
                return o;
            }
            half4 ShadowFrag(Varyings i) : SV_Target
            {
            #if defined(_ALPHATEST_ON)
                AlphaTest(AlphaOf(SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv), i.color, i.uv));
            #endif
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
            #pragma target 3.0
            #pragma vertex DepthVert
            #pragma fragment DepthFrag
            #pragma multi_compile_local_fragment _ _ALPHATEST_ON

            struct Attributes { float4 positionOS : POSITION; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float4 color : TEXCOORD1; };

            Varyings DepthVert(Attributes v)
            {
                Varyings o;
                o.positionCS = TransformObjectToHClip(v.positionOS.xyz);
                o.uv = DiffuseUV(float4(v.uv0, v.uv1)); o.color = v.color;
                return o;
            }
            half DepthFrag(Varyings i) : SV_Target
            {
                RfxModelDither(i.positionCS.xy);
            #if defined(_ALPHATEST_ON)
                AlphaTest(AlphaOf(SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv), i.color, i.uv));
            #endif
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
            #pragma target 3.0
            #pragma vertex DepthNormalsVert
            #pragma fragment DepthNormalsFrag
            #pragma multi_compile_fragment _ _GBUFFER_NORMALS_OCT
            #pragma multi_compile_local_fragment _ _ALPHATEST_ON

            struct Attributes { float4 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float3 normalWS : TEXCOORD0; float2 uv : TEXCOORD1; float4 color : TEXCOORD2; };

            Varyings DepthNormalsVert(Attributes v)
            {
                Varyings o;
                o.positionCS = TransformObjectToHClip(v.positionOS.xyz);
                o.normalWS = TransformObjectToWorldNormal(v.normalOS);
                o.uv = DiffuseUV(float4(v.uv0, v.uv1)); o.color = v.color;
                return o;
            }
            half4 DepthNormalsFrag(Varyings i) : SV_Target
            {
                RfxModelDither(i.positionCS.xy);
            #if defined(_ALPHATEST_ON)
                AlphaTest(AlphaOf(SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv), i.color, i.uv));
            #endif
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
    Fallback "Hidden/Universal Render Pipeline/FallbackError"
}
