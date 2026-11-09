using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // Additive copied snapshot. Route distance is measured along the course in feet,
    // not time or straight-line distance to the next gate.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxWaypointState
    {
        public uint size, version, valid, distance_valid;
        public int activity_type, activity_mode, gate_index, gate_count, laps_completed, num_laps;
        public float remaining_distance;
        public static RxWaypointState Create() => new RxWaypointState { size = 44, version = 1 };
    }

    internal static class RxWaypointNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate int ReadFn(IntPtr session, ref RxWaypointState state);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_waypoint");
        internal static int Read(IntPtr session, ref RxWaypointState state) => read.Get()(session, ref state);
    }
}
