using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The stunt feedback overlay's trick names (core/include/rx_stunt_hud.h, 872 bytes): the begin events of the local player's tricks,
    // newest first, each with the LocString keys (MXRaven_Stunts) of the player's current trick and of the physics stunt record at that
    // moment. The overlay (HUD layers' "StuntFeedbackComponent") keeps three of them, 3 s after the last (its own clock) and clears on "Restart".
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxStuntHudState
    {
        public const int Ring = 8, Key = 48, EntryBytes = 104, Size = 872;
        public uint size, version, valid;
        /// Begin events since "Activity Init" (a drop means a new activity), "Restart" events, entries kept (the restart empties them).
        public uint events, restartEvents, count;
        fixed byte entries[Ring * EntryBytes];
        public fixed uint reserved[4];

        /// Entry i (0 = the newest): its running number, whether it came from "PhysicsStuntBegin", the trick's and the stunt's LocString keys ("" none).
        public uint Number(int i) { fixed (byte* p = entries) return *(uint*)(p + i * EntryBytes); }
        public bool Physics(int i) { fixed (byte* p = entries) return *(uint*)(p + i * EntryBytes + 4) != 0; }
        public string Trick(int i) { fixed (byte* p = entries) return RxFixed.GetUtf8(p + i * EntryBytes + 8, Key); }
        public string Stunt(int i) { fixed (byte* p = entries) return RxFixed.GetUtf8(p + i * EntryBytes + 8 + Key, Key); }
        public static RxStuntHudState Create() => new RxStuntHudState { size = Size, version = 1 };
    }

    internal static class RxStuntHudNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxStuntHudState value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_stunt_hud");
        internal static int Read(IntPtr session, ref RxStuntHudState value) => read.Get()(session, ref value);
    }
}
