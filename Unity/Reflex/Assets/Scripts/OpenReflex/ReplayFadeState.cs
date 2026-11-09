using System;
using System.Runtime.CompilerServices;
namespace Reflex.Port.RxCore
{
    // Finite replay modes 2/3 of original FadeTransitionEffect_Update 007b75d0.
    // Evaluate on the simulation owner once per actual native frame, including request-frame dt.
    // Rendering consumes the copied result and never advances this state on Unity's clock.
    public sealed class ReplayFadeState
    {
        public bool Enabled { get; private set; }
        public float Red { get; private set; }
        public float Green { get; private set; }
        public float Blue { get; private set; }
        public float Alpha { get; private set; }
        public float Elapsed { get; private set; }
        public float Progress { get; private set; }
        public float Duration { get; private set; }
        public uint Kind { get; private set; }
        bool haveColor;
        public static bool Finite(float x) => !float.IsNaN(x) && !float.IsInfinity(x);
        // Explicit operation boundaries prevent multiply-add contraction across the original SSE instructions.
        [MethodImpl(MethodImplOptions.NoInlining)] static float Add(float a,float b)=>a+b;
        [MethodImpl(MethodImplOptions.NoInlining)] static float Sub(float a,float b)=>a-b;
        [MethodImpl(MethodImplOptions.NoInlining)] static float Mul(float a,float b)=>a*b;
        [MethodImpl(MethodImplOptions.NoInlining)] static float Div(float a,float b)=>a/b;
        // 00834c70: (3 - x*2)*x*x, saturating before arithmetic.
        public static float Smooth(float x)
        { if(x<0f)return 0f;if(x>=1f)return 1f;float y=Mul(x,2f);y=Sub(3f,y);y=Mul(y,x);return Mul(y,x); }
        public void ObserveColor(float r,float g,float b)
        {
            if(!Finite(r)||!Finite(g)||!Finite(b))throw new ArgumentException("Replay color is not finite.");
            Red=r;Green=g;Blue=b;haveColor=true;
        }
        public void Begin(uint kind,float duration,bool observedColor,float r,float g,float b)
        {
            if((kind!=2&&kind!=3)||!Finite(duration)||duration<=0f)
                throw new ArgumentException("Unsupported replay fade request.");
            if(observedColor)ObserveColor(r,g,b);
            if(!haveColor)throw new InvalidOperationException("Replay fade has no observed native color.");
            Kind=kind;Duration=duration;Elapsed=Progress=0f;Enabled=true;
            // The original sets kind3 alpha=1, while kind2 preserves alpha until its first update.
            if(kind==3)Alpha=1f;
        }
        // Returns whether this update produced a draw. Original disables AFTER the elapsed>duration draw.
        // Alpha remains available for that last draw even when Enabled becomes false.
        public bool Step(float engineDt)
        {
            if(!Finite(engineDt)||engineDt<0f)throw new ArgumentException("Invalid original renderer frame duration.");
            if(!Enabled)return false;
            Elapsed=Add(Elapsed,engineDt);Progress=Add(Progress,engineDt);
            Alpha=Smooth(Div(Progress,Duration));if(Kind==2)Alpha=Sub(1f,Alpha);
            if(Elapsed>Duration){Enabled=false;Elapsed=Progress=0f;}
            return true;
        }
    }
}
