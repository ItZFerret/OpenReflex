using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The wreck-recovery ("reflex") arrows of the HUD (core/include/rx_reflex_hud.h, 124 bytes): what MXUIReflexComponent - the HUD layers'
    // "Arrows" - shows while a bad landing / a crash can still be saved by flicking the stick: the component's visibility and lit arrows
    // (vf11 005f7b80), the wreck's kind and the direction it asks, the time left of its window (P + 0xb38 of P + 0xb3c) and the flick so far
    // (P + 0xb40 of 0.25), which controls feed the wreck input, and the counts of the four wreck events (Begin / Success / Failed / Reset).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxReflexHudState
    {
        public const int Size = 124;
        public const int DirNone = 0, DirUp = 1, DirRight = 2, DirDown = 3, DirLeft = 4;
        public uint size, version, valid;
        /// The component is visible (vf11); lit_mask: arrows made visible and not yet hidden (bit 0 ArrowTop, 1 ArrowRight, 2 ArrowBottom, 3 ArrowLeft).
        public uint shown, litMask;
        /// The direction the current wreck asks (DirUp ...; DirNone for no wreck or an auto-recovering one).
        public int direction;
        public uint mode, wreckClass, wreckType;
        /// V + 0x194: 0 bike, 1 ATV (the only vehicles the component shows for).
        public uint vehicleKind;
        /// 0 keys + mouse + right stick, 1 right stick, 2 left stick, 3 motion sensor; the pad block's handedness (1 right) and motion (0 off, 1 left, 2 right).
        public uint inputSource, handedness, motion;
        public float timeLeft, timeMax, progress, threshold, stickX, stickY;
        public uint beginEvents, successEvents, failEvents, failAnyEvents, resetEvents, lastResult, recovered, falls;
        public fixed uint reserved[4];

        /// A wreck recovery is pending (the clock runs).
        public bool Pending => valid != 0 && timeLeft > 0f;
        public static RxReflexHudState Create() => new RxReflexHudState { size = Size, version = 1, threshold = 0.25f };
    }

    internal static class RxReflexHudNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxReflexHudState value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_reflex_hud");
        internal static int Read(IntPtr session, ref RxReflexHudState value) => read.Get()(session, ref value);
    }
}
