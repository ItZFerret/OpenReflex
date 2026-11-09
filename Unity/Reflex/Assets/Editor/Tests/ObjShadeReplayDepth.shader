// Editor/Tests/ObjShadeTests (the object shading replay): fills the depth buffer with a recorded frame's own depth (D3D z / w, R32F),
// so the replayed transparent draws (particles) are hidden where the original's were.
Shader "Hidden/Reflex/ObjShadeReplayDepth"
{
    Properties { _MainTex("Depth (D3D z / w)", 2D) = "black" {} }
    SubShader
    {
        Pass
        {
            ZWrite On ZTest Always Cull Off ColorMask 0
            HLSLPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Common.hlsl"
            Texture2D _MainTex; SamplerState sampler_point_clamp;
            struct V { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
            V Vert(uint id : SV_VertexID)
            {
                V o; float2 uv = float2((id << 1) & 2, id & 2);
                o.pos = float4(uv * 2 - 1, 0.5, 1);
            #if UNITY_UV_STARTS_AT_TOP
                o.pos.y = -o.pos.y;
            #endif
                o.uv = uv; return o;
            }
            float Frag(V i, out float depth : SV_Depth) : SV_Target
            {
                float z = _MainTex.Sample(sampler_point_clamp, i.uv).r;
            #if UNITY_REVERSED_Z
                depth = 1.0 - z;
            #else
                depth = z;
            #endif
                return 0;
            }
            ENDHLSL
        }
    }
}
