using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The values of four race HUD components the original draws from typed events and activity fields (core/include/rx_race_hud.h,
    // 620 bytes): the Eliminator's "Player Eliminated" message, the Outrun gate counter, the Section Race split times and section
    // leaders, the event timer and the Freestyle (FMX) judges. Copied by the core; the strings and the layout are the presenter's.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxRaceHudState
    {
        public const int NameCapacity = 68, Sections = 5;
        public uint size, version, valid;
        /// A+0x23c (5 Freestyle, 12 Section Race), A+0x238 (8 Outrun, 5 Eliminator), A+0x21c.
        public int activityType, activityMode, activityState;
        /// The event timer (MXUITimerComponent 005f7140): A+0x294 and A+0x2bc, and whether the activity counts up (A+0x2c8).
        public float clock, clockLimit;
        public int clockCountsUp;
        /// "Restart" events of the activity.
        public uint restartEvents;
        /// "Player Eliminated" (00605de0): events, the eliminated / next drivers' slots and names (the message is next \n STR_ELIMINATED \n driver!).
        public uint eliminatedEvents;
        public int eliminatedSlot, eliminatorSlot;
        /// MXUIOutrunnerGateCounter (00605190): shows of the text, the section number shown and the total (A+0xff08c).
        public uint gateEvents;
        public int gateNumber, gateTotal;
        /// Section Race: the section count, the local driver's section (-1 none), its running time there and the section's best time.
        public int sectionCount, sectionCurrent;
        public float sectionTime, sectionBest;
        /// MXUISectionLeadersContainer: bit i set once "SectionWinner" came for section i (the name may be empty: the leader left).
        public uint leaderSet, leaderEvents;
        /// MXUIFMXFeedbackComponent: lit bars (0..10) of the four judge columns.
        public fixed int judgeBars[4];
        /// MXUIFMXFinalScoreCard: shows (WindDown Entered) and whether it is up now; Overall, Difficulty, Execution, Flow, Variety.
        public uint finalEvents, finalShown;
        public fixed float finalScores[5];
        /// MXUIHitDiscoveredComponent (006045b0): the local player's "HitDiscoveryFound" events, and the Free Ride's discovery tracker counts (discovery mode only: 0 / 0 elsewhere).
        public uint discoveryEvents;
        public int discoveryFound, discoveryTotal;
        fixed byte eliminatedName[NameCapacity], eliminatorName[NameCapacity], leaderNames[Sections * NameCapacity];

        public string EliminatedName { get { fixed (byte* p = eliminatedName) return RxFixed.GetUtf8(p, NameCapacity); } }
        public string EliminatorName { get { fixed (byte* p = eliminatorName) return RxFixed.GetUtf8(p, NameCapacity); } }
        public string LeaderName(int section) { if ((uint)section >= Sections) return ""; fixed (byte* p = leaderNames) return RxFixed.GetUtf8(p + section * NameCapacity, NameCapacity); }
        public int JudgeBars(int i) { fixed (int* p = judgeBars) return p[i]; }
        public float FinalScore(int i) { fixed (float* p = finalScores) return p[i]; }
        public static RxRaceHudState Create() => new RxRaceHudState { size = 620, version = 1 };
        /// The five final-score ratings set by hand (the showcase and the tests).
        public void SetFinalScores(params float[] v) { fixed (float* p = finalScores) for (int i = 0; i < 5 && i < v.Length; i++) p[i] = v[i]; }
    }

    internal static class RxRaceHudNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxRaceHudState value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_race_hud");
        internal static int Read(IntPtr session, ref RxRaceHudState value) => read.Get()(session, ref value);
    }
}
