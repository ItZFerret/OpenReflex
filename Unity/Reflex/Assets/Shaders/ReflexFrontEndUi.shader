// The original front end's two UI pixel programs, hand-ported for the uGUI canvas of Reflex.Port.FrontEnd:
//   DefaultIcon / TextIcon  out = tex * vertex colour, then desaturated by the component's DesaturationLevel (0..255, carried in
//                           uv1.x as 0..1; the original's "texcoord2.x saturation") - MXUIStreamableImageIcon posters of the
//                           arcade lists are drawn at level 255 (grey) except the focused one (15).
//   UIMaskContainer         MaskType TextureMask: the children's alpha is multiplied by the mask texture's red channel (DXT1,
//                           no alpha: HorizontalListMask_Large fades the poster rows at both ends) over the container's box;
//                           MaskType Stencil: the same with a 1x1White texture = a plain clip to the box (_MaskMatrix maps
//                           canvas space to the 0..1 box of the mask container, rotation included).
// Alpha blend SRCALPHA / INVSRCALPHA, no depth, as the original's UI materials (D3DRS states of UIDefault).
Shader "Reflex/FrontEnd UI"
{
    Properties
    {
        [PerRendererData] _MainTex("Texture", 2D) = "white" {}
        _MaskTex("Mask (red channel)", 2D) = "white" {}
        _MaskOn("Mask on", Float) = 0
        _StencilComp("Stencil Comparison", Float) = 8
        _Stencil("Stencil ID", Float) = 0
        _StencilOp("Stencil Operation", Float) = 0
        _StencilWriteMask("Stencil Write Mask", Float) = 255
        _StencilReadMask("Stencil Read Mask", Float) = 255
        _ColorMask("Color Mask", Float) = 15
    }
    SubShader
    {
        Tags { "Queue" = "Transparent" "IgnoreProjector" = "True" "RenderType" = "Transparent" "PreviewType" = "Plane" "CanUseSpriteAtlas" = "True" }
        Stencil
        {
            Ref [_Stencil]
            Comp [_StencilComp]
            Pass [_StencilOp]
            ReadMask [_StencilReadMask]
            WriteMask [_StencilWriteMask]
        }
        Cull Off
        Lighting Off
        ZWrite Off
        ZTest [unity_GUIZTestMode]
        Blend SrcAlpha OneMinusSrcAlpha
        ColorMask [_ColorMask]

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            struct appdata { float4 vertex : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv1 : TEXCOORD1; };
            struct v2f { float4 pos : SV_POSITION; fixed4 color : COLOR; float2 uv : TEXCOORD0; float desat : TEXCOORD1; float2 maskUv : TEXCOORD2; };

            sampler2D _MainTex; float4 _MainTex_ST;
            sampler2D _MaskTex; float _MaskOn; float4x4 _MaskMatrix;

            v2f vert(appdata v)
            {
                v2f o;
                o.pos = UnityObjectToClipPos(v.vertex);
                o.color = v.color;
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                o.desat = v.uv1.x;
                o.maskUv = mul(_MaskMatrix, float4(v.vertex.xyz, 1)).xy;   // canvas space -> the mask container's 0..1 box
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                fixed4 c = tex2D(_MainTex, i.uv) * i.color;
                float l = dot(c.rgb, float3(0.299, 0.587, 0.114));
                c.rgb = lerp(c.rgb, l.xxx, saturate(i.desat));
                if (_MaskOn > 0.5)
                {
                    float2 m = i.maskUv;
                    float inside = step(0, m.x) * step(m.x, 1) * step(0, m.y) * step(m.y, 1);
                    c.a *= tex2D(_MaskTex, m).r * inside;
                }
                return c;
            }
            ENDCG
        }
    }
}
