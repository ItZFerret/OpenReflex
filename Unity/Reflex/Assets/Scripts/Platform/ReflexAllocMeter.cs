// ReflexAllocMeter: which system allocates managed memory during a race, in any player (w-gcspikes). Unity's "GC Allocated In Frame"
// counter only exists in development players, and its Mono does not implement GC.GetAllocatedBytesForCurrentThread, so a scope reads the
// managed heap in use (GC.GetTotalMemory(false)) at its start and end and keeps the growth when no collection ran in between. The heap
// grows a block at a time as the allocator's free lists refill, so one scope's number is coarse frame by frame, but over a perf window
// (hundreds of frames) the shares follow the allocations. Main thread only; a scope costs two reads (well under a microsecond).
// ReflexPerfLog writes the window's per-frame growth and the largest systems in its "alloc" part:
//   | alloc 3.1 KB/frame (heap growth), sound 1.2, replay 0.6, hud 0.3, practice 0.2, opponents 0.1, other 0.7
using System;

namespace Reflex.Port.Platform
{
    public static class ReflexAllocMeter
    {
        public const int Sound = 0, Practice = 1, Opponents = 2, Hud = 3, Replay = 4, View = 5, Count = 6;
        public static readonly string[] Names = { "sound", "practice", "opponents", "hud", "replay", "view" };

        /// The scopes measure (ReflexPerfLog turns it on while it samples a race; off, Begin / End cost a branch).
        public static bool On;

        public readonly struct Mark
        {
            public readonly long used; public readonly int gc;
            public Mark(long used, int gc) { this.used = used; this.gc = gc; }
        }

        static readonly long[] s_bytes = new long[Count];

        public static Mark Begin() => On ? new Mark(GC.GetTotalMemory(false), GC.CollectionCount(0)) : default;

        public static void End(int slot, in Mark m)
        {
            if (!On || m.used == 0) return;
            long d = GC.GetTotalMemory(false) - m.used;
            if (d > 0 && GC.CollectionCount(0) == m.gc) s_bytes[slot] += d;
        }

        /// The bytes of each system since the last Take (cleared).
        public static void Take(long[] into)
        {
            for (int i = 0; i < Count; i++) { into[i] = s_bytes[i]; s_bytes[i] = 0; }
        }
    }
}
