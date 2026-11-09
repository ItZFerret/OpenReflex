// WEATHER (LIGHTING: ENHANCED only; not part of the original renderer): the mist of rain and snow (Scripts/Rendering/ReflexWeather.cs,
// ReflexWeatherMistPass). One full-screen triangle blended over the opaque world before the transparent objects: the amount grows with the
// distance (1 - exp(-density d), thicker low over the ground), the colour is the light under the clouds (_RfxWeatherFog); the sky gets the
// same haze along its horizon so the far hills sink into it.
Shader "Hidden/Reflex/WeatherMist"
{
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" }
        Pass
        {
            Name "Mist"
            Cull Off ZWrite Off ZTest Always
            Blend SrcAlpha OneMinusSrcAlpha
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "../ReflexCommon.hlsl"
            #include "../ReflexWeather.hlsl"
            TEXTURE2D_FLOAT(_RfxWDepth);
            float4 _RfxWZParams;                       // LinearEyeDepth: 1 / (z raw + w)
            float4 _RfxWTopLeft, _RfxWXExtent, _RfxWYExtent, _RfxWCam;   // camera rays at the near plane (world orientation); camera, near

            struct V { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; };
            V Vert(uint id : SV_VertexID)
            {
                V o;
                o.positionCS = GetFullScreenTriangleVertexPosition(id);
                o.uv = GetFullScreenTriangleTexCoord(id);
                return o;
            }

            half4 Frag(V i) : SV_Target
            {
                float raw = SAMPLE_TEXTURE2D_LOD(_RfxWDepth, sampler_PointClamp, i.uv, 0).r;
                float2 q = float2(i.uv.x, 1.0 - i.uv.y);
                float3 ray = (_RfxWTopLeft.xyz + _RfxWXExtent.xyz * q.x + _RfxWYExtent.xyz * q.y) / _RfxWCam.w;   // forward component 1
                float3 dir = normalize(ray);
            #if UNITY_REVERSED_Z
                bool sky = raw <= 1e-7;
            #else
                bool sky = raw >= 1.0 - 1e-7;
            #endif
                float density = _RfxWeatherFog.w;
                float a;
                if (sky) a = (1.0 - smoothstep(-0.02, 0.32, dir.y)) * saturate(density * 300.0) * 0.85;
                else
                {
                    float eye = 1.0 / (_RfxWZParams.z * raw + _RfxWZParams.w);
                    float3 toP = ray * eye;
                    float d = length(toP);
                    // thicker low: towards half the density over the first few hundred metres above the camera (the hills stay hazed)
                    float k = density * lerp(0.45, 1.0, exp2(-max(toP.y, 0.0) / 120.0));
                    a = 1.0 - exp(-k * max(d - 4.0, 0.0));
                    a = min(a, 0.92);
                }
                float3 c = _RfxWeatherFog.rgb;
                return half4(RfxOutput(c), saturate(a));
            }
            ENDHLSL
        }
    }
    Fallback Off
}
