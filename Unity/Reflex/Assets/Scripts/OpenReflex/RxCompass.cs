using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // The HUD compass' markers (core/game/rx_compass_api.h): the positions the running Free Ride / challenge activity holds, which the original's
    // MXUICompassComponent files on its strip (the level's challenge areas in the Race & Ride challenges Free Ride, the TargetPractice targets not
    // yet hit, the Trailblazer finish) and the Waypoint race's gates. 992 bytes; a mark is {type, index, aux, x, y, z} (type 1 area / 2 target /
    // 3 finish / 4 waypoint gate).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCompassState
    {
        public const int Marks = 40, Stride = 6;
        public const int TypeArea = 1, TypeTarget = 2, TypeFinish = 3, TypeGate = 4;
        public uint size, version, valid;
        /// The challenge's kind (0 BigAir, 1 HillClimb, 3 Pathfinder, 4 StepUp, 6 Trailblazer, 8 TargetPractice); -1 in the Free Ride.
        public int challengeKind;
        /// MXActivityFreeRide::vf54: the Free Ride is the Race and Ride challenges' (the area icons show only then).
        public uint discoveryMode;
        public uint count;
        /// A Waypoint race: the local driver's last gate passed (+0x10d4, -1 before the first), else -1 (0 from a core before the gates).
        public int waypointGate;
        /// A Waypoint race: 1 once the local driver finished (its "Player Wins": the compass hides the gates, 00602ac0).
        public uint waypointFinished;
        fixed int marks[Marks * Stride];

        public static RxCompassState Create() => new RxCompassState { size = 992, version = 1, challengeKind = -1, waypointGate = -1 };
        public int Type(int i) { fixed (int* p = marks) return p[i * Stride]; }
        /// AREA: the challenge type (0 BigAir .. 9 Wheelie, the layer's icon order); TARGET: the target's slot; GATE: the compass slot (WaypointGateN - 1).
        public int Index(int i) { fixed (int* p = marks) return p[i * Stride + 1]; }
        /// AREA: the trigger's TrackId; TARGET: 1 when the target was hit; GATE: the gate's own index (the race's order).
        public int Aux(int i) { fixed (int* p = marks) return p[i * Stride + 2]; }
        public (float x, float y, float z) Position(int i)
        {
            fixed (int* p = marks)
                return (BitConverter.Int32BitsToSingle(p[i * Stride + 3]), BitConverter.Int32BitsToSingle(p[i * Stride + 4]), BitConverter.Int32BitsToSingle(p[i * Stride + 5]));
        }
        /// Tests: a synthetic mark.
        public void Set(int i, int type, int index, int aux, float x, float y, float z)
        {
            fixed (int* p = marks)
            {
                p[i * Stride] = type; p[i * Stride + 1] = index; p[i * Stride + 2] = aux;
                p[i * Stride + 3] = BitConverter.SingleToInt32Bits(x); p[i * Stride + 4] = BitConverter.SingleToInt32Bits(y); p[i * Stride + 5] = BitConverter.SingleToInt32Bits(z);
            }
            if (i >= count) count = (uint)(i + 1);
        }
    }

    internal static class RxCompassNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxCompassState state);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_compass");
        internal static int Read(IntPtr session, ref RxCompassState state) => read.Get()(session, ref state);
    }
}
