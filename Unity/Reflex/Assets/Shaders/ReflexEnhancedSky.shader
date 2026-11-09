// LIGHTING: ENHANCED (OPTIONS > VIDEO), not part of the original renderer: the level's sky dome drawn with a CC0 HDRI sky
// (Assets/ThirdParty/Skies, chosen per level by Scripts/Rendering/ReflexEnhancedSky.cs) in place of the original sky texture.
//
// The HDRI keeps the level's look: it is turned so that its sun stands at the level's sun azimuth, its sun is clamped away (the level's
// own sun billboard / glow stays the sun), and its low frequencies are pulled towards the ORIGINAL sky's (a 128 x 64 lat-long image of
// the original dome, "Unwrap" pass): colour = hdri * lerp(exposure * tint, original_low / hdri_low, keep). Near the horizon the original
// sky image itself is kept (painted backdrops, the haze the fog was authored against).
// Passes: 0 EnhancedSky (drawn on the dome instead of Reflex/Original Sky, same states), 1 Unwrap (dome -> lat-long of the original
// sky, alpha 2 + dir.y marks coverage and orientation), 2 Prepare (equirect HDRI -> clamped RT; or a mip of it), 3 Probe (the final sky
// by direction as a lat-long, below the horizon the level's own reflection map: for the ambient SH and the reflection cube).
Shader "Reflex/Enhanced Sky"
{
    Properties
    {
        [NoScaleOffset] _BaseMap("Original sky texture", 2D) = "white" {}
        _Bracket("usr_self_bracketing_value", Float) = 1.7
        [NoScaleOffset] _SatMap("SkyEntityComp saturation texture (s1)", 2D) = "black" {}
        _SkySaturationScale("SkyEntityComp literal saturation scale", Vector) = (0, 0, 0, 0)
        _SkySaturationBias("SkyEntityComp literal saturation bias", Vector) = (0, 0, 0, 0)
        _SkyLuminance("SkyEntityComp literal luminance scale, bias", Vector) = (0, 0, 0, 0)
        [Enum(UnityEngine.Rendering.CompareFunction)] _SkyZTest("ZTest", Float) = 4
        [Enum(UnityEngine.Rendering.CullMode)] _Cull("Cull", Float) = 2
        [NoScaleOffset] _SkyHdri("Prepared HDRI (clamped, mips)", 2D) = "black" {}
        [NoScaleOffset] _SkyOrigLow("Original sky, lat-long 128 x 64", 2D) = "black" {}
        _SkyRot("x: u offset (HDRI sun u - level sun azimuth / 2pi)", Vector) = (0, 0, 0, 0)
        _SkyGrade("rgb: exposure * tint, w: keep (0 HDRI gradient .. 1 original gradient)", Vector) = (1, 1, 1, 0.6)
        _SkyHorizon("x, y: elevation sin where the HDRI starts / is full, z: HDRI mix, w: low mip", Vector) = (0.02, 0.2, 1, 5)
        _SkyGlow("rgb: sun glow colour, w: exponent", Vector) = (0, 0, 0, 64)
        _SkyPrep("Prepare: x clamp luminance, y source lod", Vector) = (1000000, 0, 0, 0)
        _SkyDetail("x: cloud contrast around the low frequencies, y: saturation", Vector) = (0.8, 0.9, 0, 0)
        _SkySunDir("The sky's sun (the original sky's painted sun, else the level's light)", Vector) = (0, 1, 0, 0)
    }
    HLSLINCLUDE
    #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
    #include "ReflexCommon.hlsl"
    #include "ReflexWeather.hlsl"   // WEATHER: the overcast deck over the dome (Scripts/Rendering/ReflexWeather.cs)
    CBUFFER_START(UnityPerMaterial)
        float _Bracket;
        float4 _SkySaturationScale, _SkySaturationBias, _SkyLuminance;
        float4 _SkyRot, _SkyGrade, _SkyHorizon, _SkyGlow, _SkyPrep, _SkyDetail, _SkySunDir;
    CBUFFER_END
    TEXTURE2D(_BaseMap); SAMPLER(sampler_BaseMap);
    TEXTURE2D(_SatMap); SAMPLER(sampler_SatMap);
    TEXTURE2D(_SkyHdri); SAMPLER(sampler_SkyHdri);
    TEXTURE2D(_SkyOrigLow); SAMPLER(sampler_SkyOrigLow);
    float _RfxEnhForceRaw;     // 1 while the sky is probed outside the game camera (raw values regardless of _RfxRawPipeline)

    // the original sky programs (Template_Luma_Sky_Daytime / SkyEntityComp, Shaders/ReflexOriginalSky.shader) at a dome uv
    float3 OriginalSky(float2 uv)
    {
    #if defined(_SKY_ENTITY_COMP)
        float3 sat = SAMPLE_TEXTURE2D_LOD(_SatMap, sampler_SatMap, uv, 0).rgb;
        float lum = SAMPLE_TEXTURE2D_LOD(_BaseMap, sampler_BaseMap, uv, 0).r;
        return RfxSkyGamma((lum * _SkyLuminance.x + _SkyLuminance.y) * (sat * _SkySaturationScale.xyz + _SkySaturationBias.xyz));   // (ALIVE LOOK REFLEX)
    #else
        float3 t = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, uv).rgb;
        return t * _Bracket - t;
    #endif
    }
    // lat-long of a direction: u = 0.5 + azimuth / 2pi (azimuth = atan2(z, x)), v = 0.5 + elevation / pi (v up)
    float2 LatLong(float3 d) { return float2(0.5 + atan2(d.z, d.x) * (0.5 * INV_PI), 0.5 + asin(clamp(d.y, -1.0, 1.0)) * INV_PI); }
    float3 LatLongDir(float2 uv)
    {
        float az = (uv.x - 0.5) * TWO_PI, el = (uv.y - 0.5) * PI;
        return float3(cos(el) * cos(az), sin(el), cos(el) * sin(az));
    }
    // the enhanced sky in direction d; orig = the original sky there (full detail on the dome, the low-res image when probing)
    float3 EnhancedSky(float3 d, float3 orig)
    {
        float2 ll = LatLong(d);
        float2 uv = float2(frac(ll.x + _SkyRot.x), ll.y);
        float3 hdri = SAMPLE_TEXTURE2D_LOD(_SkyHdri, sampler_SkyHdri, uv, 0).rgb;
        float3 hdriLow = SAMPLE_TEXTURE2D_LOD(_SkyHdri, sampler_SkyHdri, uv, _SkyHorizon.w).rgb;
        float3 origLow = SAMPLE_TEXTURE2D_LOD(_SkyOrigLow, sampler_SkyOrigLow, ll, 0).rgb;
        float3 ratio = clamp(origLow / max(hdriLow, 1e-4), _SkyGrade.rgb * 0.25, _SkyGrade.rgb * 4.0);
        hdri = hdriLow * pow(max(hdri, 1e-5) / max(hdriLow, 1e-5), _SkyDetail.x);      // softer clouds than the photograph
        float3 c = hdri * lerp(_SkyGrade.rgb, ratio, _SkyGrade.w);
        float cl = dot(c, float3(0.2126, 0.7152, 0.0722));
        c = max(lerp(cl.xxx, c, _SkyDetail.y), 0);
        // the level's sun (the HDRI's own sun is clamped away): a disk, a halo and a wide glow around the level's sun direction
        float mu = saturate(dot(d, normalize(_SkySunDir.xyz)));
        float disk = smoothstep(0.99990, 0.99995, mu) * 30.0;
        c += _SkyGlow.rgb * (disk + pow(mu, _SkyGlow.w) + 0.25 * pow(mu, _SkyGlow.w * 0.125));
        float h = smoothstep(_SkyHorizon.x, _SkyHorizon.y, d.y) * _SkyHorizon.z;
        return lerp(orig, c, h);
    }
    struct FsV { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
    FsV FsVert(uint id : SV_VertexID)
    {
        FsV o;
        o.positionCS = GetFullScreenTriangleVertexPosition(id);
        o.uv = GetFullScreenTriangleTexCoord(id);
        return o;
    }
    ENDHLSL

    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" "Queue"="Background" "IgnoreProjector"="True" }

        // 0: the dome (states of Reflex/Original Sky: far plane, z write off, the material's cull)
        Pass
        {
            Name "EnhancedSky"
            Tags { "LightMode"="UniversalForward" }
            Cull [_Cull]
            ZWrite Off
            ZTest [_SkyZTest]
            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile_local_fragment _ _SKY_ENTITY_COMP
            struct Attributes { float4 positionOS : POSITION; float2 uv : TEXCOORD0; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float3 dir : TEXCOORD1; };
            Varyings Vert(Attributes v)
            {
                Varyings o;
                float3 rel = mul((float3x3)GetObjectToWorldMatrix(), v.positionOS.xyz);
                float4 clip = TransformWorldToHClip(rel + _WorldSpaceCameraPos);
            #if UNITY_REVERSED_Z
                clip.z = 0.0;
            #else
                clip.z = clip.w;
            #endif
                o.positionCS = clip; o.uv = v.uv; o.dir = rel;
                return o;
            }
            half4 Frag(Varyings i) : SV_Target
            {
                float3 d = normalize(i.dir);
                float3 c = EnhancedSky(d, OriginalSky(i.uv));
                [branch] if (_RfxWeather.w > 0.5 && _RfxEnhForceRaw < 0.5) c = lerp(c, RfxWeatherSkyColour(d, 1.0), _RfxWeather.z);   // WEATHER: the clouds close in
                return half4(_RfxEnhForceRaw > 0.5 ? c : RfxOutput(c), 1.0);
            }
            ENDHLSL
        }

        // 1: the original sky unwrapped into a lat-long target (vertices placed at their direction's lat-long position); triangles that
        // straddle the azimuth seam are discarded (the interpolated azimuth differs from the pixel's), the CPU fills those texels
        Pass
        {
            Name "Unwrap"
            Tags { "LightMode"="ReflexSkyUnwrap" }
            Cull Off ZWrite Off ZTest Always Blend Off
            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma multi_compile_local_fragment _ _SKY_ENTITY_COMP
            float4 _RfxUnwrapSize;   // w, h of the target
            struct Attributes { float4 positionOS : POSITION; float2 uv : TEXCOORD0; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; float3 dir : TEXCOORD1; };
            Varyings Vert(Attributes v)
            {
                Varyings o;
                float3 d = normalize(mul((float3x3)GetObjectToWorldMatrix(), v.positionOS.xyz));
                o.positionCS = float4(LatLong(d) * 2.0 - 1.0, 0.5, 1.0);
                o.uv = v.uv; o.dir = d;
                return o;
            }
            half4 Frag(Varyings i) : SV_Target
            {
                // a pixel whose own direction lies elsewhere in azimuth belongs to a triangle stretched across the seam
                float3 d = normalize(i.dir);
                float du = abs(i.positionCS.x / _RfxUnwrapSize.x - LatLong(d).x);
                if (min(du, 1.0 - du) > 0.08) discard;
                return half4(OriginalSky(i.uv), 2.0 + d.y);
            }
            ENDHLSL
        }

        // 2: the HDRI prepared for sampling: luminance clamped (its sun removed), or a mip of the prepared image (_SkyPrep.y)
        Pass
        {
            Name "Prepare"
            Tags { "LightMode"="ReflexSkyPrepare" }
            Cull Off ZWrite Off ZTest Always Blend Off
            HLSLPROGRAM
            #pragma vertex FsVert
            #pragma fragment Frag
            half4 Frag(FsV i) : SV_Target
            {
                float3 c = SAMPLE_TEXTURE2D_LOD(_SkyHdri, sampler_SkyHdri, i.uv, _SkyPrep.y).rgb;
                c = max(c, 0);
                float l = dot(c, float3(0.2126, 0.7152, 0.0722));
                if (l > _SkyPrep.x) c *= _SkyPrep.x / l;
                return half4(c, 1.0);
            }
            ENDHLSL
        }

        // 3: the enhanced sky by direction as a lat-long (alpha 2 + dir.y: orientation marker for the read-back)
        Pass
        {
            Name "Probe"
            Tags { "LightMode"="ReflexSkyProbe" }
            Cull Off ZWrite Off ZTest Always Blend Off
            HLSLPROGRAM
            #pragma vertex FsVert
            #pragma fragment Frag
            TEXTURECUBE(_SkyOrigCube); SAMPLER(sampler_SkyOrigCube);
            float _SkyOrigCubeOn;
            half4 Frag(FsV i) : SV_Target
            {
                float3 d = LatLongDir(i.uv);
                float3 orig = SAMPLE_TEXTURE2D_LOD(_SkyOrigLow, sampler_SkyOrigLow, i.uv, 0).rgb;
                float3 c = EnhancedSky(d, orig);
                // below and near the horizon the level's own reflection map (terrain, trees, stands as the original reflects them)
                if (_SkyOrigCubeOn > 0.5)
                    c = lerp(SAMPLE_TEXTURECUBE_LOD(_SkyOrigCube, sampler_SkyOrigCube, d, 0).rgb, c, smoothstep(0.05, 0.45, d.y));
                return half4(c, 2.0 + d.y);
            }
            ENDHLSL
        }
    }
    Fallback Off
}
