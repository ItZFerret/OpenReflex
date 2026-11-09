using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    // Call only on the owning simulation worker, between advances. Receipt of
    // a request never means its animation, navigation, or save has completed.
    public static class RxCareerNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ConfigureFn(IntPtr s, ref RxCareerConfig config, out ulong generation);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SeriesFn(IntPtr s, ulong generation, uint series, ref RxCareerSeriesInfo info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ValueFn(IntPtr s, ulong generation, uint value);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VehicleFn(IntPtr s, ulong generation, ref RxCareerVehicleIdentity value);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ActionFn(IntPtr s, ulong generation);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StatusFn(IntPtr s, ulong generation, ref RxCareerStatus status);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int NextFn(IntPtr s, ulong generation, ref RxCareerRequest request);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AckFn(IntPtr s, ulong generation, ulong id, uint kind, uint accepted);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int WipeFn(IntPtr s, ulong generation, ulong id, uint kind, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, uint milestone);
        static readonly RxNativeLoader.Fn<ConfigureFn> configure = new RxNativeLoader.Fn<ConfigureFn>("rx_session_career_configure");
        static readonly RxNativeLoader.Fn<SeriesFn> series = new RxNativeLoader.Fn<SeriesFn>("rx_session_career_series");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PresentationFn(IntPtr s, ulong generation, uint series, ref RxCareerSeriesPresentation info);
        static readonly RxNativeLoader.Fn<PresentationFn> presentation = new RxNativeLoader.Fn<PresentationFn>("rx_session_career_series_presentation");
        public static RxCareerResult SeriesPresentation(IntPtr s, ulong g, uint id, ref RxCareerSeriesPresentation info) => (RxCareerResult)presentation.Get()(s, g, id, ref info);
        static readonly RxNativeLoader.Fn<ValueFn> load = new RxNativeLoader.Fn<ValueFn>("rx_session_career_load");
        static readonly RxNativeLoader.Fn<ValueFn> select = new RxNativeLoader.Fn<ValueFn>("rx_session_career_select_vehicle");
        static readonly RxNativeLoader.Fn<ActionFn> initialize = new RxNativeLoader.Fn<ActionFn>("rx_session_career_initialize");
        static readonly RxNativeLoader.Fn<ActionFn> start = new RxNativeLoader.Fn<ActionFn>("rx_session_career_start");
        static readonly RxNativeLoader.Fn<StatusFn> status = new RxNativeLoader.Fn<StatusFn>("rx_session_career_status");
        static readonly RxNativeLoader.Fn<NextFn> next = new RxNativeLoader.Fn<NextFn>("rx_session_career_next");
        static readonly RxNativeLoader.Fn<AckFn> ack = new RxNativeLoader.Fn<AckFn>("rx_session_career_ack");
        static readonly RxNativeLoader.Fn<WipeFn> wipe = new RxNativeLoader.Fn<WipeFn>("rx_session_career_wipe");
        static readonly RxNativeLoader.Fn<ActionFn> proceed = new RxNativeLoader.Fn<ActionFn>("rx_session_career_continue");
        static readonly RxNativeLoader.Fn<ActionFn> abort = new RxNativeLoader.Fn<ActionFn>("rx_session_career_abort");
        public static RxCareerResult Configure(IntPtr s, ref RxCareerConfig c, out ulong g) => (RxCareerResult)configure.Get()(s, ref c, out g);
        public static RxCareerResult Series(IntPtr s, ulong g, uint id, ref RxCareerSeriesInfo info) => (RxCareerResult)series.Get()(s, g, id, ref info);
        public static RxCareerResult Load(IntPtr s, ulong g, uint id) => (RxCareerResult)load.Get()(s, g, id);
        static readonly RxNativeLoader.Fn<VehicleFn> vehicle = new RxNativeLoader.Fn<VehicleFn>("rx_session_career_vehicle");
        public static RxCareerResult Vehicle(IntPtr s, ulong g, ref RxCareerVehicleIdentity value) => (RxCareerResult)vehicle.Get()(s, g, ref value);
        public static RxCareerResult SelectVehicle(IntPtr s, ulong g, uint type) => (RxCareerResult)select.Get()(s, g, type);
        public static RxCareerResult Initialize(IntPtr s, ulong g) => (RxCareerResult)initialize.Get()(s, g);
        public static RxCareerResult Start(IntPtr s, ulong g) => (RxCareerResult)start.Get()(s, g);
        public static RxCareerResult Status(IntPtr s, ulong g, ref RxCareerStatus value) => (RxCareerResult)status.Get()(s, g, ref value);
        public static RxCareerResult Next(IntPtr s, ulong g, ref RxCareerRequest value) => (RxCareerResult)next.Get()(s, g, ref value);
        public static RxCareerResult Ack(IntPtr s, ulong g, ulong id, RxCareerRequestKind kind, bool accepted) => (RxCareerResult)ack.Get()(s, g, id, (uint)kind, accepted ? 1u : 0u);
        public static RxCareerResult Wipe(IntPtr s, ulong g, ulong id, string name, uint milestone)
        {
            if (string.IsNullOrEmpty(name) || name.IndexOf('\0') >= 0 || Encoding.UTF8.GetByteCount(name) >= 256)
                throw new ArgumentException("Invalid page transition name.", nameof(name));
            if (milestone != 1 && milestone != 2) throw new ArgumentOutOfRangeException(nameof(milestone));
            return (RxCareerResult)wipe.Get()(s, g, id, (uint)RxCareerRequestKind.PageWipe, name, milestone);
        }
        public static RxCareerResult Continue(IntPtr s, ulong g) => (RxCareerResult)proceed.Get()(s, g);
        public static RxCareerResult Abort(IntPtr s, ulong g) => (RxCareerResult)abort.Get()(s, g);
    }
}
