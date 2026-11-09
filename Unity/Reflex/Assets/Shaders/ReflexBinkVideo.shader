// The game's Bink video pixel programs (MXReflex.exe: HLSL text at 0x934c88 (YUV) and 0x935138 (YUVA), compiled ps_2_0 by
// 00594220 and drawn by 00594a40 over the video's render target), hand-ported. The planes are the A8 textures the game uploads
// (Y, cR, cB at half size, A; here R8, .r = .a): sampled with the quad's one UV (D3DSAMP linear filter, clamp), then
//   p = y * 1.164123535 + crc * cr + crb * cb + adj,   crc (1.595794678, -0.813476563, 0), crb (0, -0.391448975, 2.017822266),
//   adj (-0.87065506, 0.529705048, -1.081668854),  p.w = 1 (YUV) or a (YUVA),  p *= c0 (1, 1, 1, 1).
// The target (A8R8G8B8, cleared to 0) holds p saturated; the YUVA draw is alpha blended SRCALPHA / INVSRCALPHA onto the cleared
// target (ALPHABLENDENABLE 1, SRCBLEND 5, DESTBLEND 6), which leaves (rgb * a, a * a). _SrgbTarget: the target is an sRGB
// render texture of a linear-space project, so the stored bytes are the game's (the hardware encodes what we decode here).
// Rows: the planes are top-down, Unity textures bottom-up (v flipped). The quad maps UV 0..1 over the whole planes
// (BINKFRAMEBUFFERS YABufferWidth / Height, wider than the picture when it is not a multiple of 16) onto the whole target.
Shader "Reflex/Bink Video"
{
    Properties
    {
        _YTex("Y", 2D) = "black" {}
        _CrTex("cR", 2D) = "black" {}
        _CbTex("cB", 2D) = "black" {}
        _ATex("A", 2D) = "white" {}
        _Alpha("YUVA program", Float) = 0
        _SrgbTarget("sRGB target", Float) = 1
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" "PreviewType" = "Plane" }
        Cull Off ZWrite Off ZTest Always Blend Off
        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            sampler2D _YTex, _CrTex, _CbTex, _ATex;
            float _Alpha, _SrgbTarget;
            struct v2f { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
            v2f vert(appdata_img v) { v2f o; o.pos = UnityObjectToClipPos(v.vertex); o.uv = v.texcoord; return o; }
            float3 toLinear(float3 c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
            float4 frag(v2f i) : SV_Target
            {
                float2 t = float2(i.uv.x, 1.0 - i.uv.y);   // UV 0..1 over the whole plane (YABufferWidth / Height), as the quad
                const float4 crc = float4(1.595794678, -0.813476563, 0, 0.0);
                const float4 crb = float4(0, -0.391448975, 2.017822266, 0.0);
                const float4 adj = float4(-0.87065506, 0.529705048, -1.081668854, 0);
                float y = tex2D(_YTex, t).r, cr = tex2D(_CrTex, t).r, cb = tex2D(_CbTex, t).r;
                float4 p = y * 1.164123535;
                p += crc * cr + crb * cb + adj;
                p.w = _Alpha > 0.5 ? tex2D(_ATex, t).r : 1.0;
                p = saturate(p);                                   // the 8-bit target
                if (_Alpha > 0.5) p = float4(p.rgb * p.w, p.w * p.w);   // SRCALPHA / INVSRCALPHA onto (0, 0, 0, 0)
                if (_SrgbTarget > 0.5) p.rgb = toLinear(p.rgb);
                return p;
            }
            ENDHLSL
        }
    }
}
