// The game's display gamma ramp on the presented image (ReflexGammaOutput): each 8-bit channel value i of the back buffer becomes
// ramp[i] / 65535 (IDirect3DDevice9::SetGammaRamp semantics; the three channels share one table, 007aff40). The back buffer is
// sRGB-encoded in the linear project, so values are converted to the 8-bit display code and back.
Shader "Hidden/Reflex/GammaRamp"
{
    Properties { _MainTex("Source", 2D) = "white" {} }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #include "UnityCG.cginc"
            sampler2D _MainTex;
            float _FlipY;   // the copy of the back buffer comes back upside down where UVs start at the top (Metal, D3D)
            Texture2D<float4> _ReflexGammaRamp;

            struct V2F { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
            V2F Vert(appdata_img v) { V2F o; o.pos = UnityObjectToClipPos(v.vertex); o.uv = v.texcoord; return o; }

            float Code(float linear01) { return linear01 <= 0.0031308 ? linear01 * 12.92 : 1.055 * pow(linear01, 1.0 / 2.4) - 0.055; }
            float Linear(float code) { return code <= 0.04045 ? code / 12.92 : pow((code + 0.055) / 1.055, 2.4); }
            float Ramp(float linear01)
            {
                int i = (int)round(saturate(Code(saturate(linear01))) * 255.0);
                return Linear(_ReflexGammaRamp.Load(int3(i, 0, 0)).r);
            }
            float4 Frag(V2F i) : SV_Target
            {
                float2 uv = i.uv; if (_FlipY > 0.5) uv.y = 1.0 - uv.y;
                float4 c = tex2D(_MainTex, uv);
                return float4(Ramp(c.r), Ramp(c.g), Ramp(c.b), 1);
            }
            ENDCG
        }
    }
}
