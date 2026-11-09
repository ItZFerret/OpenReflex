// Imported MTX Mototrax venues: Neversoft's multi-pass materials in one pass. The base texture times the prelit vertex colour
// (0x80 = 1.0), then up to three layers, each on its own UV set (pass k uses set k) or sphere-mapped (environment passes),
// with the engine's blend modes: 1/2 add, 3/4 subtract, 5/6 blend, 7/8 modulate, 9/10 brighten, 12/13 blend by the previous
// pass's alpha (inverse). Each pass is tinted by its pass colour (x2; 0.5 = none); an environment base pass is sphere-mapped. Source alpha = texture alpha x vertex alpha (unless the pass ignores vertex alpha); the even modes
// use the pass's fixed alpha. The alpha cutoff clips the base texture's alpha. Billboards turn about the vertical axis
// through their pivot (TEXCOORD4) by their offsets (TEXCOORD5: across, up).
// _MtxRaw (Content/ExperimentalMtxLighting.cs): the textures are raw (not sRGB) like every Reflex world texture, the result goes to Reflex's
// HDR target through RfxOutput with Reflex's distance fog, and the venue's MTX `lighting` tint (world: colours toward vc_target by
// vc_percent, x lev / 128; sky: x sky / 128) multiplies the prelit colours. 0 (REFLEX_MTX_LIGHT=legacy): sRGB textures, Unity fog, no tint.
Shader "Reflex/Experimental MTX Scenery"
{
    Properties
    {
        _MainTex ("Base",2D)="white" {}
        _L1Tex ("Layer 1",2D)="black" {}
        _L2Tex ("Layer 2",2D)="black" {}
        _L3Tex ("Layer 3",2D)="black" {}
        _L1Mode ("Layer 1 blend",Float)=0
        _L2Mode ("Layer 2 blend",Float)=0
        _L3Mode ("Layer 3 blend",Float)=0
        _L1Params ("Layer 1 fixed alpha, env, ignore VA, UV set",Vector)=(1,0,0,1)
        _L2Params ("Layer 2 fixed alpha, env, ignore VA, UV set",Vector)=(1,0,0,2)
        _L3Params ("Layer 3 fixed alpha, env, ignore VA, UV set",Vector)=(1,0,0,3)
        _BaseColor ("Base pass colour",Color)=(1,1,1,1)
        _BaseEnv ("Base env map: on, tiling",Vector)=(0,1,1,0)
        _L1Color ("Layer 1 colour",Color)=(1,1,1,1)
        _L2Color ("Layer 2 colour",Color)=(1,1,1,1)
        _L3Color ("Layer 3 colour",Color)=(1,1,1,1)
        _L1Tile ("Layer 1 env tiling",Vector)=(1,1,0,0)
        _L2Tile ("Layer 2 env tiling",Vector)=(1,1,0,0)
        _L3Tile ("Layer 3 env tiling",Vector)=(1,1,0,0)
        _BaseBlend ("Base alpha blend",Float)=0
        _Cutoff ("Alpha cutoff",Float)=0
        _Sky ("Sky",Float)=0
        _FixedAlpha ("Fixed alpha",Float)=0
        _IgnoreVertexAlpha ("Ignore vertex alpha",Float)=0
        _ColorScale ("Vertex colour scale",Float)=2
        _Billboard ("Billboard",Float)=0
        _SrcBlend ("Source blend",Float)=1
        _DstBlend ("Destination blend",Float)=0
        _ZWrite ("Depth write",Float)=1
        _MtxRaw ("Raw Reflex pipeline",Float)=0
        _MtxTint ("MTX lighting tint (rgb), vc target (w)",Vector)=(1,1,1,0)
        _MtxVcPercent ("MTX lighting vc percent",Float)=0
        _MtxFog ("Reflex fog: 0 none, 1 blend, 2 dim only",Float)=0
        _MtxSkyGamma ("Sky dome raw value exponent",Float)=1
    }
    SubShader
    {
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" }
        HLSLINCLUDE
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "../Shaders/ReflexCommon.hlsl"
            TEXTURE2D(_MainTex); SAMPLER(sampler_MainTex);
            TEXTURE2D(_L1Tex); SAMPLER(sampler_L1Tex);
            TEXTURE2D(_L2Tex); SAMPLER(sampler_L2Tex);
            TEXTURE2D(_L3Tex); SAMPLER(sampler_L3Tex);
            CBUFFER_START(UnityPerMaterial)
            float4 _L1Params,_L2Params,_L3Params,_L1Tile,_L2Tile,_L3Tile,_BaseColor,_BaseEnv,_L1Color,_L2Color,_L3Color,_MtxTint;
            float _L1Mode,_L2Mode,_L3Mode,_BaseBlend,_Cutoff,_Sky,_FixedAlpha,_IgnoreVertexAlpha,_ColorScale,_Billboard,_ZWrite,_MtxRaw,_MtxVcPercent,_MtxFog,_MtxSkyGamma;
            CBUFFER_END
            struct A {float4 p:POSITION;float3 n:NORMAL;float2 uv0:TEXCOORD0;float2 uv1:TEXCOORD1;float2 uv2:TEXCOORD2;float2 uv3:TEXCOORD3;float3 origin:TEXCOORD4;float2 offset:TEXCOORD5;half4 color:COLOR;};
            struct V {float4 p:SV_POSITION;float4 uv01:TEXCOORD0;float4 uv23:TEXCOORD1;float2 env:TEXCOORD2;half4 color:COLOR;half fog:TEXCOORD3;float3 nw:TEXCOORD4;half4 rfog:TEXCOORD5;};
            V vert(A i)
            {
                V o;float3 w=TransformObjectToWorld(i.p.xyz);
                if(_Billboard>.5)
                {
                    float3 pivot=TransformObjectToWorld(i.origin);float3 view=_WorldSpaceCameraPos-pivot;view.y=0;
                    float3 across=normalize(float3(-view.z,0,view.x)+float3(1e-5,0,0));
                    float k=length(float3(UNITY_MATRIX_M._m00,UNITY_MATRIX_M._m10,UNITY_MATRIX_M._m20));   // MAP SCALE: the venue's root scale
                    w=pivot+(across*i.offset.x+float3(0,i.offset.y,0))*k;
                }
                o.p=TransformWorldToHClip(w);o.uv01=float4(i.uv0,i.uv1);o.uv23=float4(i.uv2,i.uv3);o.color=i.color;
                o.rfog=half4(0,0,0,1);
                if(_MtxRaw>.5)
                {
                    o.color.rgb=lerp(i.color.rgb,_MtxTint.w,_MtxVcPercent)*_MtxTint.rgb;
                    if(_MtxFog>.5)o.rfog=RfxFog(w,_WorldSpaceCameraPos);
                }
                o.nw=TransformObjectToWorldNormal(i.n);float3 vn=normalize(TransformWorldToViewDir(o.nw));o.env=vn.xy*.5+.5;
                o.fog=ComputeFogFactor(o.p.z);return o;
            }
            // the depth passes (URP's depth prepass and SSAO): what the forward pass writes to depth, cut-outs clipped
            void DepthClip(V i)
            {
                if(_Sky>.5||_ZWrite<.5)clip(-1);
                if(_Cutoff>0)clip(SAMPLE_TEXTURE2D(_MainTex,sampler_MainTex,i.uv01.xy).a-_Cutoff);
            }
        ENDHLSL
        Pass
        {
            Tags { "LightMode"="UniversalForward" }
            Cull Off ZWrite [_ZWrite] Blend [_SrcBlend] [_DstBlend]
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile_fog
            float2 LayerUV(V i,float4 params,float4 tile)
            {
                if(params.y>.5)return i.env*tile.xy;
                return params.w<1.5?i.uv01.zw:params.w<2.5?i.uv23.xy:i.uv23.zw;
            }
            void Apply(inout half3 c,inout half prevA,half4 l,float mode,float4 params,half3 vc,half va)
            {
                if(mode<.5)return;
                half3 s=l.rgb*vc;bool even=fmod(mode,2)<.5&&mode<11.5;half a=even?params.x:l.a*(params.z>.5?1:va);
                if(mode<2.5)c+=s*a;
                else if(mode<4.5)c=max(c-s*a,0);
                else if(mode<6.5)c=lerp(c,s,saturate(a));
                else if(mode<8.5)c*=lerp(1,s,saturate(even?params.x:1));
                else if(mode<10.5)c+=c*s*a;
                else if(mode<12.5)c=lerp(c,s,saturate(prevA));
                else c=lerp(c,s,saturate(1-prevA));
                prevA=l.a*(params.z>.5?1:va);
            }
            half4 frag(V i):SV_Target
            {
                half4 t=SAMPLE_TEXTURE2D(_MainTex,sampler_MainTex,_BaseEnv.x>.5?i.env*_BaseEnv.yz:i.uv01.xy);
                half3 vc=i.color.rgb*_ColorScale*_BaseColor.rgb;half va=saturate(i.color.a*2),va0=_IgnoreVertexAlpha>.5?1:va;
                half3 c=t.rgb*vc;half alpha=_BaseBlend>.5?t.a*(_FixedAlpha>0?_FixedAlpha:va0):t.a;half prevA=t.a*va0;
                half3 lvc=i.color.rgb*2;
                if(_L1Mode>.5)Apply(c,prevA,SAMPLE_TEXTURE2D(_L1Tex,sampler_L1Tex,LayerUV(i,_L1Params,_L1Tile)),_L1Mode,_L1Params,lvc*_L1Color.rgb,va);
                if(_L2Mode>.5)Apply(c,prevA,SAMPLE_TEXTURE2D(_L2Tex,sampler_L2Tex,LayerUV(i,_L2Params,_L2Tile)),_L2Mode,_L2Params,lvc*_L2Color.rgb,va);
                if(_L3Mode>.5)Apply(c,prevA,SAMPLE_TEXTURE2D(_L3Tex,sampler_L3Tex,LayerUV(i,_L3Params,_L3Tile)),_L3Mode,_L3Params,lvc*_L3Color.rgb,va);
                if(_Cutoff>0)clip(alpha-_Cutoff);
                if(_MtxRaw>.5)
                {
                    if(_MtxSkyGamma!=1)c=pow(max(c,0),_MtxSkyGamma);   // sky domes (ExperimentalMtxLighting.SkyGamma)
                    c=_MtxFog>1.5?c*i.rfog.w:_MtxFog>.5?c*i.rfog.w+i.rfog.rgb:c;
                    return half4(RfxOutput(max(c,0)),alpha);
                }
                if(_Sky<.5)c=MixFog(c,i.fog);
                return half4(c,alpha);
            }
            ENDHLSL
        }
        // MTX's ground and scenery in the camera's depth (and normals) textures: the MODERN look's haze, flare smoke and
        // soft particles stop at MTX's terrain like at the game's own
        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            Cull Off ZWrite On ColorMask R
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment depthFrag
            half depthFrag(V i):SV_Target { DepthClip(i); return i.p.z; }
            ENDHLSL
        }
        Pass
        {
            Name "DepthNormals"
            Tags { "LightMode"="DepthNormals" }
            Cull Off ZWrite On
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment normalsFrag
            half4 normalsFrag(V i, bool front:SV_IsFrontFace):SV_Target { DepthClip(i); float3 n=normalize(i.nw)*(front?1:-1); return half4(n,0); }
            ENDHLSL
        }
    }
}
