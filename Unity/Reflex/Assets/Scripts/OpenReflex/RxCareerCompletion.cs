using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCareerCompletionStatus
    {
        public const uint ByteSize = 160;
        public uint size, version;
        public ulong generation, ticket, frontend_request_id, checkpoint_one_id, checkpoint_two_id;
        public uint post_state_observed, complete_event_state, frontend_ready, records_state;
        public uint complete_series_state, tour_enabled, in_memory_settled, durable;
        public uint complete_event_allowed, frontend_ready_allowed, complete_series_allowed, checkpoint_error;
        public fixed byte frontend_route[64];
        public static RxCareerCompletionStatus Create() => new RxCareerCompletionStatus { size = ByteSize, version = 1 };
        public string FrontendRoute { get { fixed (byte* p = frontend_route) return RxFixed.GetUtf8(p, 64); } }
    }

    internal interface ICareerCompletionWorkerNative
    {
        RxCareerResult Completion(IntPtr session, ulong generation, ref RxCareerCompletionStatus status);
        RxCareerResult CompleteEvent(IntPtr session, ulong generation, ulong ticket);
        RxCareerResult FrontendReady(IntPtr session, ulong generation, ulong ticket, ulong routeId);
        RxCareerResult CompleteSeries(IntPtr session, ulong generation, ulong ticket);
    }

    public static class RxCareerCompletionNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StatusFn(IntPtr s, ulong generation, ref RxCareerCompletionStatus status);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ActionFn(IntPtr s, ulong generation, ulong ticket);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FrontendFn(IntPtr s, ulong generation, ulong ticket, ulong routeId);
        static readonly RxNativeLoader.Fn<StatusFn> status = new RxNativeLoader.Fn<StatusFn>("rx_session_career_completion");
        static readonly RxNativeLoader.Fn<ActionFn> completeEvent = new RxNativeLoader.Fn<ActionFn>("rx_session_career_complete_event");
        static readonly RxNativeLoader.Fn<FrontendFn> frontend = new RxNativeLoader.Fn<FrontendFn>("rx_session_career_frontend_ready");
        static readonly RxNativeLoader.Fn<ActionFn> completeSeries = new RxNativeLoader.Fn<ActionFn>("rx_session_career_complete_series");
        public static RxCareerResult Status(IntPtr s, ulong g, ref RxCareerCompletionStatus value) => (RxCareerResult)status.Get()(s, g, ref value);
        public static RxCareerResult CompleteEvent(IntPtr s, ulong g, ulong ticket) => (RxCareerResult)completeEvent.Get()(s, g, ticket);
        public static RxCareerResult FrontendReady(IntPtr s, ulong g, ulong ticket, ulong routeId) => (RxCareerResult)frontend.Get()(s, g, ticket, routeId);
        public static RxCareerResult CompleteSeries(IntPtr s, ulong g, ulong ticket) => (RxCareerResult)completeSeries.Get()(s, g, ticket);
    }
}
