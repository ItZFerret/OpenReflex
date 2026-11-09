using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The standings' source values of the Waypoint / Freestyle / Section Race results table (core/include/rx_mode_results.h, 1200 bytes): what the
    // original's table fillers 00615450 / 00614bf0 / 006152e0 read from the drivers. Copied by the core, live while the event runs and final in Post.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxModeResultRow
    {
        public const int KeyCapacity = 48, Size = 96;
        public int slot, local, place, finished;
        /// The driver's clock (D+0x134, stops at its finish) and the Waypoint driver's best lap (D+0x10b0).
        public float totalTime, bestLap;
        /// Freestyle: P+0xcec (TotalScore), the largest of the player's five trick scores (0050de10) when he has a best trick.
        public float score, bestTrickScore;
        /// Section Race: D+0x1134 (the "Sections" column) and 005133f0's lowest section time with its index (-1 and FLT_MAX: none).
        public int sectionsWon, bestSection;
        public float bestSectionTime;
        public int hasBestTrick;
        fixed byte bestTrick[KeyCapacity];
        /// The best trick's LocString key ("STR_TWISTER"), empty without one.
        public string BestTrick { get { fixed (byte* p = bestTrick) return RxFixed.GetUtf8(p, KeyCapacity); } }
        /// A row built by hand (the showcase and the tests; the core's rows come from rx_session_mode_results).
        public static RxModeResultRow Make(int slot, bool local, int place, bool finished, float totalTime = 0f, float bestLap = float.MaxValue, float score = 0f, float bestTrickScore = 0f,
            string trick = null, int sectionsWon = 0, int bestSection = -1, float bestSectionTime = float.MaxValue)
        {
            var r = new RxModeResultRow { slot = slot, local = local ? 1 : 0, place = place, finished = finished ? 1 : 0, totalTime = totalTime, bestLap = bestLap, score = score,
                bestTrickScore = bestTrickScore, sectionsWon = sectionsWon, bestSection = bestSection, bestSectionTime = bestSectionTime, hasBestTrick = string.IsNullOrEmpty(trick) ? 0 : 1 };
            if (!string.IsNullOrEmpty(trick))
            {
                var bytes = System.Text.Encoding.UTF8.GetBytes(trick);
                for (int i = 0; i < bytes.Length && i < KeyCapacity - 1; i++) r.bestTrick[i] = bytes[i];
            }
            return r;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxModeResultsState
    {
        public const int Rows = 12, Size = 1200;
        public uint size, version, valid;
        /// A+0x23c, A+0x238, A+0x21c; the Section Race's section count (A+0x13e650, -1 elsewhere).
        public int activityType, activityMode, raceState, sectionCount;
        public uint count;
        public fixed int reserved[4];
        fixed byte rows[Rows * RxModeResultRow.Size];
        public static RxModeResultsState Create() => new RxModeResultsState { size = Size, version = 1 };
        /// A state built by hand (the showcase and the tests).
        public static RxModeResultsState Make(int activityType, int sectionCount, params RxModeResultRow[] list)
        {
            var s = Create(); s.valid = 1; s.activityType = activityType; s.sectionCount = sectionCount; s.raceState = 4;
            s.count = (uint)Math.Min(list.Length, Rows);
            byte* p = s.rows;
            for (int i = 0; i < s.count; i++) *(RxModeResultRow*)(p + i * RxModeResultRow.Size) = list[i];
            return s;
        }
        public int Count => (int)Math.Min(count, (uint)Rows);
        public RxModeResultRow Row(int i)
        {
            if ((uint)i >= (uint)Count) return default;
            fixed (byte* p = rows) return *(RxModeResultRow*)(p + i * RxModeResultRow.Size);
        }
        /// The standings' row of a player slot (the local player's: slot of the local rider), or null.
        public bool TryBySlot(int slot, out RxModeResultRow row)
        {
            for (int i = 0; i < Count; i++) { var r = Row(i); if (r.slot == slot) { row = r; return true; } }
            row = default; return false;
        }
        public bool TryLocal(out RxModeResultRow row)
        {
            for (int i = 0; i < Count; i++) { var r = Row(i); if (r.local != 0) { row = r; return true; } }
            row = default; return false;
        }
    }

    internal static class RxModeResultsNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxModeResultsState value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_mode_results");
        internal static int Read(IntPtr session, ref RxModeResultsState value) => read.Get()(session, ref value);
    }
}
