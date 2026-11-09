using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RxReplayRequest
    {
        public uint size,version;public ulong generation,id,frame;
        public uint operation,viewport_count,fade_kind,duration_bits,color_mask;
        public uint red_bits,green_bits,blue_bits,alpha_bits,flags;
        public static RxReplayRequest Create()=>new RxReplayRequest{size=72,version=1};
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct RxReplayView
    {
        public uint size,version;public ulong generation,frame;
        public uint active,camera_valid,engine_dt_bits,pending_requests;
        public RxCamera camera;public uint reserved;
        public static RxReplayView Create()=>new RxReplayView{size=104,version=1};
    }
    public static class RxReplayNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int BeginFn(IntPtr s,ulong g);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int BeginEventFn(IntPtr s,out ulong g);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int EndEventFn(IntPtr s,ulong g);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PeekFn(IntPtr s,ulong g,uint index,ref RxReplayRequest r);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ViewFn(IntPtr s,ulong g,ref RxReplayView v);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AckFn(IntPtr s,ulong g,ref RxReplayRequest r,uint milestone);
        static readonly RxNativeLoader.Fn<BeginFn> begin=new RxNativeLoader.Fn<BeginFn>("rx_session_replay_begin");
        // Exhibition events (no career): the same presenter, with its own generation (rx_replay_api.h).
        static readonly RxNativeLoader.Fn<BeginEventFn> beginEvent=new RxNativeLoader.Fn<BeginEventFn>("rx_session_replay_begin_event");
        static readonly RxNativeLoader.Fn<EndEventFn> endEvent=new RxNativeLoader.Fn<EndEventFn>("rx_session_replay_end_event");
        static readonly RxNativeLoader.Fn<PeekFn> peek=new RxNativeLoader.Fn<PeekFn>("rx_session_replay_peek");
        static readonly RxNativeLoader.Fn<ViewFn> view=new RxNativeLoader.Fn<ViewFn>("rx_session_replay_view");
        static readonly RxNativeLoader.Fn<AckFn> ack=new RxNativeLoader.Fn<AckFn>("rx_session_replay_ack");
        internal static RxCareerResult Begin(IntPtr s,ulong g)=>(RxCareerResult)begin.Get()(s,g);
        internal static RxCareerResult BeginEvent(IntPtr s,out ulong g)=>(RxCareerResult)beginEvent.Get()(s,out g);
        internal static RxCareerResult EndEvent(IntPtr s,ulong g)=>(RxCareerResult)endEvent.Get()(s,g);
        internal static RxCareerResult Peek(IntPtr s,ulong g,uint index,ref RxReplayRequest r)=>(RxCareerResult)peek.Get()(s,g,index,ref r);
        internal static RxCareerResult View(IntPtr s,ulong g,ref RxReplayView v)=>(RxCareerResult)view.Get()(s,g,ref v);
        internal static RxCareerResult Ack(IntPtr s,ulong g,ref RxReplayRequest r,uint milestone)=>(RxCareerResult)ack.Get()(s,g,ref r,milestone);
        public static unsafe float Float(uint bits)=>*(float*)&bits;
    }
}
