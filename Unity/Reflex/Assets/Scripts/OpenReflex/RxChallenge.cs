using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // The Free Ride challenges' end-of-challenge page (core/game/rx_challenge_api.h): the challenge activity's medal state the original's
    // PostChallengePopup shows. Everything here is copied from the core; nothing is recomputed.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxChallengeState
    {
        public uint size, version, valid;
        /// A+0xff070: 0 BigAir, 1 HillClimb, 3 Pathfinder, 4 StepUp, 6 Trailblazer, 8 TargetPractice.
        public int kind;
        /// A+0x21c: 0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive.
        public int state;
        public int area;
        /// The activity's medal flags (set at the Post enter) and the popup's ChallengeMedalStatus: 0 Failed, 1 Bronze, 2 Silver, 3 Gold.
        public int gold, silver, bronze, medal;
        public int valueValid;
        /// The class's vf62: HillClimb the best height in feet, Trailblazer the time in seconds, TargetPractice the hits.
        public float value;
        /// The Bronze / Silver / Gold criteria of the level's MXChallengesSpecEntity (what PreChallengePopup lists).
        public float criteria0, criteria1, criteria2;
        /// From the Post state on: the profile's PlayerChallenges row of the location after the award.
        public int recordValid;
        public float record;
        public int count0, count1, count2;
        public int reserved0, reserved1;

        public static RxChallengeState Create() => new RxChallengeState { size = 88, version = 1 };
        public float Criterion(int medal) => medal == 0 ? criteria0 : medal == 1 ? criteria1 : criteria2;
        public int Count(int medal) => medal == 0 ? count0 : medal == 1 ? count1 : count2;
    }

    // The page's prize (core/game/rx_challenge_api.h rx_challenge_payout, 68 bytes): PostChallengePopup:Start's SeriesPoints prize and bonus,
    // MXTableHelper:DepositIntoAccount and CheckForFreeRideSeriesChallengeCompletion, done by the core once per Post.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxChallengePayout
    {
        public uint size, version, paid, repeat;
        /// 0 Failed, 1 Bronze, 2 Silver, 3 Gold.
        public int medal, rewardLevel, eventTypeLocationId;
        /// Credits: PRIZE, BONUS, TOTAL; the account balance after the deposit.
        public int prize, bonus, total, accountBalance;
        public int seriesId, seriesBestPlace;
        public uint seriesRowCreated, seriesCompleted;
        public int reserved0, reserved1;
        public static RxChallengePayout Create() => new RxChallengePayout { size = 68, version = 1 };
    }

    internal static class RxChallengeNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PayoutFn(IntPtr session, int seriesId, ref RxChallengePayout payout);
        static readonly RxNativeLoader.Fn<PayoutFn> payout = new RxNativeLoader.Fn<PayoutFn>("rx_session_challenge_payout");
        /// PostChallengePopup:Start's payout (once per Post; a repeat returns the same figures without paying).
        internal static int Payout(IntPtr session, int seriesId, ref RxChallengePayout value) => payout.Get()(session, seriesId, ref value);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxChallengeState state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RestartFn(IntPtr session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int WaitFn(IntPtr session, int hold);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_challenge");
        static readonly RxNativeLoader.Fn<RestartFn> restart = new RxNativeLoader.Fn<RestartFn>("rx_session_challenge_restart");
        static readonly RxNativeLoader.Fn<WaitFn> waitForUi = new RxNativeLoader.Fn<WaitFn>("rx_session_wait_for_ui");
        static readonly RxNativeLoader.Fn<RestartFn> restartActivity = new RxNativeLoader.Fn<RestartFn>("rx_session_restart_activity");
        /// PreChallengePopup's UI variable WaitForUI (rx_session_wait_for_ui): 1 keeps the challenge in its Pre state, 0 lets it start.
        internal static int WaitForUi(IntPtr session, bool hold) => waitForUi.Get()(session, hold ? 1 : 0);
        internal static int Read(IntPtr session, ref RxChallengeState state) => read.Get()(session, ref state);
        /// App.RestartActivity (MXGame_RestartActivity): the world keeps running, the challenge starts again.
        internal static int Restart(IntPtr session) => restart.Get()(session);
        /// The pause page's RESTART of any event (rx_session_restart_activity: MXGame_RestartActivity for the local player's activity; 3: the
        /// career event's results were taken). Challenges use Restart, which also arms their next payout.
        internal static int RestartActivity(IntPtr session) => restartActivity.Get()(session);
    }
}
