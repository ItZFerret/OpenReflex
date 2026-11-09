using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The race format's HUD values (core/include/rx_race_format.h, 48 bytes): what the career formats' HUD components read from the
    // local player's activity. Copied by the core; nothing is recomputed here.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxRaceFormatState
    {
        public uint size, version, valid;
        /// A+0x238: 1 race, 5 eliminator, 6 overtake, 7 speed and style, 8 outrun.
        public int activityMode, activityState;
        /// A+0x26c and the eliminator's remaining riders A+0xff088 (MXUIEliminatorComponent "%d/%d").
        public int drivers, ridersLeft;
        /// The local player's Outrun clock: the section (-1 before the first) and the time left (P+0x11ac / P+0x11b4).
        public int section;
        public float sectionTime;
        public int reserved0, reserved1, reserved2;
        public static RxRaceFormatState Create() => new RxRaceFormatState { size = 48, version = 1 };
    }
    internal static class RxRaceFormatNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxRaceFormatState value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_race_format");
        internal static int Read(IntPtr session, ref RxRaceFormatState value) => read.Get()(session, ref value);
    }
}
