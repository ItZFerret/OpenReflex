using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // The Free Ride activation areas (core/game/rx_activation_api.h): the local MXPlayer's area fields the game's trigger handlers keep
    // (MXUIManager_OnTriggerEnter 005a3b60 / OnTriggerExit 005a3d00): active (+0xa0), the area's activity (+0xa4: the EventTypeActivation
    // the pause key starts) and its TrackId (+0x1a8); required_class is the ChallengeRequiredVehicleClass they set (-1 none).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxActivationState
    {
        public uint size, version, valid, active;
        public int track_id, required_class;
        public fixed byte activity[24];
        public string Activity { get { fixed (byte* p = activity) return RxFixed.GetUtf8(p, 24); } }
        public static RxActivationState Create() => new RxActivationState { size = 48, version = 1 };
    }

    internal static class RxActivationNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxActivationState state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CareerModeFn(IntPtr session, int value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_activation");
        static readonly RxNativeLoader.Fn<CareerModeFn> careerMode = new RxNativeLoader.Fn<CareerModeFn>("rx_session_event_in_career_mode");
        internal static int Read(IntPtr session, ref RxActivationState state) => read.Get()(session, ref state);
        /// Before rx_session_event_load: the event runs with the UI property InCareerMode (the Race & Ride challenges Free Ride).
        internal static int SetInCareerMode(IntPtr session, bool value) => careerMode.Get()(session, value ? 1 : 0);
    }
}
