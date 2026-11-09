// RESTORED CONTENT (w-champweekend): the championship weekend (core/include/rx_champ_weekend.h). Not part of the original PC game.
// The weekend is a native struct (rx_champ_weekend, 7880 bytes, version 4: TIMED RACES, 32 riders) kept here between the stages: each stage is its
// own session; the simulation's worker calls Apply right after rx_session_event_load, AfterFrame after every frame (the stage is recorded as
// soon as its activity is in Post; the running stage's clock is read) and AtSessionEnd before the session is destroyed. Everything a session
// does here is keyed on that session (appliedSession): a stage's next session can start before the last one's teardown without losing either.
// The in-game flow between the stages is OpenFrontEnd/ChampWeekendFlow.cs (the results, the weekend card, the next stage's session in the same
// scene); the setup page is OpenShell.ChampWeekend.cs.
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    public static class RxChampWeekend
    {
        const int Size = 7880, StagesAt = 36, StageSize = 80, RiderCountAt = 676, RidersAt = 680, RiderSize = 188, OverallAt = 6696, MinutesAt = 6824, RiderLapsAt = 6856;
        public const int MaxStages = 8, MaxRiders = 32;
        /// TIMED RACES (rx_champ_weekend.h): the race's lap total while a stage's clock runs (RX_CHAMP_TIMED_LAPS), the setters' limits.
        public const int TimedLaps = 0x60, MaxMinutes = 60, MaxLaps = 30;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int BeginFn(IntPtr weekend, int discipline);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StageFn(IntPtr session, IntPtr weekend);
        static readonly RxNativeLoader.Fn<BeginFn> begin = new RxNativeLoader.Fn<BeginFn>("rx_champ_weekend_begin");
        static readonly RxNativeLoader.Fn<StageFn> apply = new RxNativeLoader.Fn<StageFn>("rx_session_champ_stage_apply");
        static readonly RxNativeLoader.Fn<StageFn> record = new RxNativeLoader.Fn<StageFn>("rx_session_champ_stage_record");
        static readonly RxNativeLoader.Fn<StageFn> retire = new RxNativeLoader.Fn<StageFn>("rx_session_champ_stage_retire");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TimingFn(IntPtr weekend, int stage, int minutes, int laps);
        static readonly RxNativeLoader.Fn<TimingFn> timing = new RxNativeLoader.Fn<TimingFn>("rx_champ_weekend_set_timing");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ClockFn(IntPtr session, ref Clock clock);
        static readonly RxNativeLoader.Fn<ClockFn> clockFn = new RxNativeLoader.Fn<ClockFn>("rx_session_champ_clock");

        /// The running stage's clock (rx_champ_clock): read on the worker after every frame of a stage's session.
        [StructLayout(LayoutKind.Sequential)]
        public struct Clock
        {
            public uint size;
            public int timed;          // 1: the session runs a timed stage
            public float seconds;      // the clock's length
            public float elapsed;      // the activity's InProgress clock
            public int expired;        // the clock ran out: num_laps is final
            public int flag;           // the leader took the flag: every other rider finishes at its next crossing
            public int extra_laps, num_laps;
            public float expired_at, flag_at;
        }

        static readonly object sync = new object();
        static byte[] weekend;        // the native struct (host memory)
        static bool armed;            // the next exhibition load of `armedLocation` is the current stage
        static string armedLocation;
        static bool applied;          // the current session runs a stage (sessionStage)
        static bool recorded;         // ... and its result is in the weekend already
        static bool restartRequested; // the pause page's RESTART: the session ends without a result, the same stage runs again
        static int sessionStage = -1;
        static IntPtr appliedSession; // the session that runs sessionStage (Zero: none)
        static bool started;          // ... and its race has been InProgress (a Post before that is a load state, not the stage's end)
        static Clock clock; static bool haveClock, noClock;

        /// A weekend is running (begun and not abandoned).
        public static bool Active { get { lock (sync) return weekend != null; } }
        /// The event the weekend races (OpenEvent key), set by the shell.
        public static string EventKey;
        /// The level the weekend races at (the location Arm was given last).
        public static string Location { get { lock (sync) return armedLocation; } }
        /// The last session-side failure (shown by the shell), null when none.
        public static string LastError;

        /// The disciplines (rx_champ_weekend.h): their events' EventType.ActivityName.
        public const int National = 0, Supercross = 1, Waypoint = 2, Freestyle = 3;
        public static int DisciplineOf(string activity) => activity == "National" ? National : activity == "Supercross" ? Supercross : activity == "WaypointRace" ? Waypoint : activity == "Freestyle" ? Freestyle : -1;

        /// The championship classes' activity types (the original's ActivityName enum, A+0x23c: NationalChampPractice 0x22 ..) as their
        /// discipline's base type (National 0, Supercross 1, Freestyle 5, WaypointRace 6), so the front end's per-mode pages (the owned pause,
        /// results page and HUD mode, keyed on the base types) treat a stage as its discipline's race. Any other type is returned as is.
        public static int BaseType(int type)
        {
            switch (type)
            {
                case 0x1f: case 0x22: case 0x23: case 0x24: case 0x25: return 0;   // RaceChampIntro, NationalChamp Practice / Heat / MainEvent / MainEvent2
                case 0x26: case 0x27: case 0x28: case 0x29: return 1;              // SupercrossChamp Practice / Heat / MainEvent / LCQ
                case 0x20: case 0x3a: case 0x3b: case 0x3c: return 5;              // FreestyleChampIntro, FreestyleChamp Practice / MainEvent / MainEvent2
                case 0x21: case 0x30: case 0x31: return 6;                         // WaypointRaceChampIntro, WaypointRaceChamp Practice / MainEvent
            }
            return type;
        }

        /// The running session races a weekend's stage (applied, not yet torn down): its results page offers no RACE AGAIN (the weekend's
        /// page starts the next stage).
        public static bool StageInSession { get { lock (sync) return applied && weekend != null; } }
        /// The running session's stage was recorded (its activity reached Post): the weekend's next stage / standings are current.
        public static bool StageRecorded { get { lock (sync) return applied && recorded; } }
        /// The stage the running session races (-1 none).
        public static int SessionStage { get { lock (sync) return applied ? sessionStage : -1; } }

        /// A new weekend of `discipline`. False when this librxcore has no championship weekend (or not that discipline).
        public static bool Begin(string eventKey, int discipline)
        {
            var w = new byte[Size];
            try
            {
                var h = GCHandle.Alloc(w, GCHandleType.Pinned);
                try { if (begin.Get()(h.AddrOfPinnedObject(), discipline) != 0) { LastError = "This librxcore has no championship weekend of that discipline."; return false; } }
                finally { h.Free(); }
            }
            catch (EntryPointNotFoundException) { LastError = "This librxcore has no championship weekend: rebuild the core."; return false; }
            catch (DllNotFoundException e) { LastError = "The core library is not loaded: " + e.Message; return false; }
            if (BitConverter.ToUInt32(w, 0) != Size) { LastError = "The core's championship weekend has another layout: rebuild the core and the player."; return false; }
            lock (sync) { weekend = w; armed = false; applied = false; recorded = false; appliedSession = IntPtr.Zero; haveClock = false; }
            EventKey = eventKey; LastError = null;
            return true;
        }

        /// TIMED RACES: a stage's clock (minutes, 0 = laps only) and laps (the extra laps after the clock, or the race's laps without one).
        /// Only before the stage is raced, National / Supercross only. False when refused.
        public static bool SetTiming(int stage, int minutes, int laps)
        {
            lock (sync)
            {
                if (weekend == null) return false;
                var h = GCHandle.Alloc(weekend, GCHandleType.Pinned);
                try { return timing.Get()(h.AddrOfPinnedObject(), stage, minutes, laps) == 0; }
                catch (EntryPointNotFoundException) { return false; }
                finally { h.Free(); }
            }
        }
        /// National and Supercross stages run on a clock + laps; Waypoint (gate rounds) and Freestyle (judged runs) do not.
        public static bool Timeable => Discipline == National || Discipline == Supercross;
        public static int StageMinutes(int s) => I(MinutesAt + 4 * s);
        public static int RiderLaps(int r, int s) => I(RiderLapsAt + (r * MaxStages + s) * 4);
        /// The running session races a timed stage: its clock and extra laps (the HUD counts the clock down, then "+1 LAP" / "FINAL LAP").
        public static bool TimedStageInSession(out float seconds, out int extraLaps)
        {
            seconds = 0f; extraLaps = 0;
            lock (sync)
            {
                if (!applied || weekend == null) return false;
                int s = sessionStage, min = s >= 0 && s < MaxStages ? BitConverter.ToInt32(weekend, MinutesAt + 4 * s) : 0;
                if (min <= 0) return false;
                seconds = min * 60f; extraLaps = BitConverter.ToInt32(weekend, StagesAt + s * StageSize + 52);
                return true;
            }
        }
        /// The running timed stage's clock as the core has it after the last frame (false: none read yet, or no timed stage runs).
        public static bool SessionClock(out Clock c)
        {
            lock (sync) { c = clock; return applied && haveClock && clock.timed != 0; }
        }
        /// A race's lap total is the timed stage's placeholder (the clock still runs: the total is not known yet). No real race has 96 laps
        /// (the lap counts go to 30; PRACTICE's endless race is 0x7fffffff), so the number alone says it whatever the weekend's state.
        public static bool LapTotalUnknown(int numLaps) => numLaps == TimedLaps;
        /// "15:00 + 1" / "3 LAPS": a stage's format.
        public static string StageFormat(int s)
        {
            int min = StageMinutes(s), laps = StageLaps(s);
            if (min > 0) return laps <= 0 ? min + ":00" : min + ":00 + " + laps + (laps == 1 ? " LAP" : " LAPS");   // (a practice: the clock alone)
            return laps > 0 ? laps + (laps == 1 ? " LAP" : " LAPS") : "";
        }

        public static void Abandon() { lock (sync) { weekend = null; armed = false; applied = false; recorded = false; restartRequested = false; appliedSession = IntPtr.Zero; sessionStage = -1; haveClock = false; } EventKey = null; }

        /// The current stage's race is launched at `location` (the level, "NAT_Concord"): the next exhibition load there runs it. False when
        /// there is no weekend or it is over.
        public static bool Arm(string location) { lock (sync) { armed = weekend != null && !FinishedLocked; armedLocation = location; return armed; } }
        /// The next exhibition load runs a stage (armed, not applied yet).
        public static bool Armed { get { lock (sync) return armed && weekend != null; } }
        /// The label of the stage the next armed load runs ("MOTO 1"), null when nothing is armed for `location`.
        public static string ArmedStageLabel(string location)
        {
            lock (sync)
            {
                if (!armed || weekend == null || !string.Equals(location, armedLocation, StringComparison.OrdinalIgnoreCase)) return null;
                int s = BitConverter.ToInt32(weekend, 12);
                return s >= 0 && s < MaxStages ? SL(StagesAt + s * StageSize, 16) : null;
            }
        }

        /// Worker thread, right after rx_session_event_load of `location`: the armed stage applied. Null on success (or nothing armed for
        /// that level), else the error.
        public static string Apply(IntPtr session, string location)
        {
            lock (sync)
            {
                if (!armed || weekend == null || !string.Equals(location, armedLocation, StringComparison.OrdinalIgnoreCase)) return null;
                armed = false;
                if (FinishedLocked) return null;   // (nothing left to race: the event runs as itself)
                int rc = Call(apply, session);
                if (rc != 0) { LastError = Clean(RxApiNative.rx_session_last_error(session)); return "championship stage: " + LastError; }
                applied = true; recorded = false; restartRequested = false; appliedSession = session; haveClock = false; started = false;
                sessionStage = BitConverter.ToInt32(weekend, 12);
                return null;
            }
        }

        /// Worker thread, after every frame's snapshot: the stage recorded once its activity is in Post (4), so the results page, the
        /// in-game weekend card and the menus all see the result (the stage index moves on here, not at the session's end).
        public static void AfterFrame(IntPtr session, int activityState)
        {
            lock (sync)
            {
                if (!applied || weekend == null || session != appliedSession) return;
                if (!noClock)
                {
                    var c = new Clock { size = (uint)Marshal.SizeOf<Clock>() };
                    try { if (clockFn.Get()(session, ref c) == 0) { clock = c; haveClock = true; } }
                    catch (EntryPointNotFoundException) { noClock = true; }
                }
                // (only a race that ran: the event's load states run as frames too, the prestart's in the menus among them, and the activity reads
                // a state past Post there; recording then took the stage before it was raced)
                if (activityState == 2 || activityState == 3) started = true;
                if (recorded || !started || activityState < 4) return;
                recorded = true;
                if (Call(record, session) != 0) LastError = Clean(RxApiNative.rx_session_last_error(session));
            }
        }

        /// Worker thread (between two frames): the local rider leaves the running stage (the pause page's RETIRE): the stage is recorded with the
        /// local rider last (or with its places when it had ended already), so the weekend moves on. True when the stage is recorded.
        public static bool RetireNow(IntPtr session)
        {
            lock (sync)
            {
                if (!applied || weekend == null || session != appliedSession) return false;
                if (recorded) return true;
                recorded = true; restartRequested = false;
                int rc = RecordOrRetire(session);
                if (rc != 0) LastError = Clean(RxApiNative.rx_session_last_error(session));
                return rc == 0;
            }
        }

        /// The pause page's RESTART of a stage: the session ends without a result and the next session at the level races the same stage.
        public static void PrepareRestart() { lock (sync) if (applied && !recorded) restartRequested = true; }

        /// Worker thread, before rx_session_destroy. A stage that did not reach Post: a RESTART arms it again; any other end (a quit, a
        /// retire, ESC) records it with the local rider last (rx_session_champ_stage_retire), so the weekend always moves on.
        public static void AtSessionEnd(IntPtr session)
        {
            lock (sync)
            {
                if (!applied || weekend == null || session != appliedSession) return;
                applied = false; sessionStage = -1; appliedSession = IntPtr.Zero; haveClock = false;
                if (recorded) return;
                if (restartRequested) { restartRequested = false; armed = !FinishedLocked; return; }
                int rc = RecordOrRetire(session);
                if (rc != 0) LastError = Clean(RxApiNative.rx_session_last_error(session));
            }
        }

        /// The stage's places as they stand: recorded when the race ended (the session FINISHED: WindDown / Post, the local rider finished),
        /// else retired (the local rider last). Never one after the other: a refused call puts the session in RX_SESSION_ERROR (session_error),
        /// which then refuses the other too. That was the old order (record, then retire on failure): a quit / ESC before the end recorded
        /// nothing, the weekend stayed on the same stage and the menu offered PRACTICE again.
        static int RecordOrRetire(IntPtr session)
        {
            bool ended = started && RxApiNative.rx_session_get_state(session) == RxSessionState.Finished;
            return Call(ended ? record : retire, session);
        }

        static int Call(RxNativeLoader.Fn<StageFn> fn, IntPtr session)
        {
            var h = GCHandle.Alloc(weekend, GCHandleType.Pinned);
            try { return fn.Get()(session, h.AddrOfPinnedObject()); }
            catch (EntryPointNotFoundException) { return 1; }
            finally { h.Free(); }
        }

        static string Clean(string s) => string.IsNullOrEmpty(s) ? "(no error text)" : s;

        // ---- reading the weekend (main thread) ----
        static int I(int at) { lock (sync) return weekend == null ? 0 : BitConverter.ToInt32(weekend, at); }
        static float F(int at) { lock (sync) return weekend == null ? 0f : BitConverter.ToSingle(weekend, at); }
        static string S(int at, int n) { lock (sync) return SL(at, n); }
        static string SL(int at, int n)
        {
            if (weekend == null) return "";
            int len = 0; while (len < n && weekend[at + len] != 0) len++;
            return Encoding.UTF8.GetString(weekend, at, len);
        }
        public static int Stage => I(12);
        public static int StageCount => Math.Min(I(16), MaxStages);
        public static bool Finished => I(20) != 0;
        public static int Discipline => I(8);
        /// The local rider did not make the main event (the original's "Failed").
        public static bool Eliminated => I(28) != 0;
        /// The standings are judged scores (Freestyle), not SeriesPoints.
        public static bool Judged => I(32) != 0;
        static bool FinishedLocked => weekend != null && BitConverter.ToInt32(weekend, 20) != 0;
        public static string StageLabel(int s) => S(StagesAt + s * StageSize, 16);
        public static string StageActivity(int s) => S(StagesAt + s * StageSize + 16, 32);
        public static int StageLaps(int s) => I(StagesAt + s * StageSize + 52);
        public static bool StageScored(int s) => I(StagesAt + s * StageSize + 56) != 0;
        public static bool StageRaced(int s) => I(StagesAt + s * StageSize + 60) != 0;
        /// Not raced: its places decided by the previous stage (an LCQ without the local rider).
        public static bool StageDecided(int s) => I(StagesAt + s * StageSize + 64) != 0;
        /// Who rides the stage (FieldAll / FieldLcq / FieldMain) and how its gates are set (GridEvent / GridBestLap / GridPlace / GridMain) from
        /// which earlier stage (-1 none).
        public static int StageField(int s) => I(StagesAt + s * StageSize + 68);
        public static int StageGrid(int s) => I(StagesAt + s * StageSize + 72);
        public static int StageGridFrom(int s) => I(StagesAt + s * StageSize + 76);
        public const int FieldAll = 0, FieldLcq = 1, FieldMain = 2, GridEvent = 0, GridBestLap = 1, GridPlace = 2, GridMain = 3;
        /// The Supercross qualification of a heat of 12 (the original's MXSeriesEventHeat::vf13): heat 1-5 to the main, 6-11 to the LCQ, the
        /// LCQ's first two to the main. Any field: Split.
        public const int HeatToMain = 5, HeatLcqLast = 11, LcqToMain = 2;
        /// FIELD SIZE: the Supercross split of a heat of `field` riders, as the core's champ_split / rx_champ_weekend_split
        /// (champ_weekend_session.inc; the table in rx_champ_weekend.h): heat 1 .. toMain to the main, toMain + 1 .. lcqLast to the LCQ (none
        /// when equal), LCQ 1 .. lcqToMain to the main. 12 riders: 5 / 11 / 2 (the constants above); 24: 10 / 22 / 4; 32: 13 / 29 / 5.
        public static void Split(int field, out int toMain, out int lcqLast, out int lcqToMain)
        {
            toMain = lcqLast = lcqToMain = 0;
            if (field < 1) return;
            int h = Math.Min(Math.Max((5 * field + 6) / 12, 1), field);
            int last = field - (field + 6) / 12;
            int l = Math.Max(last - h, 0), t = (last - h + 1) / 3;
            if (l > 0 && t < 1) t = 1;
            if (t >= l) { h += l; l = 0; t = 0; }   // (everyone would go through: no LCQ)
            toMain = h; lcqLast = h + l; lcqToMain = t;
        }
        public static int RiderCount => Math.Min(I(RiderCountAt), MaxRiders);
        /// The i-th rider of the overall order (best first).
        public static int Overall(int i) => I(OverallAt + 4 * i);
        public static string RiderName(int r) => S(RidersAt + r * RiderSize, 16);
        public static bool RiderLocal(int r) => I(RidersAt + r * RiderSize + 48) != 0;
        public static int RiderPoints(int r) => I(RidersAt + r * RiderSize + 52);
        public static int RiderPlace(int r, int stage) => I(RidersAt + r * RiderSize + 60 + 4 * stage);
        public static float RiderBestLap(int r, int stage) => F(RidersAt + r * RiderSize + 92 + 4 * stage);
        /// The 1-based start slot the rider took in the stage (0 = not raced).
        public static int RiderGate(int r, int stage) => I(RidersAt + r * RiderSize + 124 + 4 * stage);
        /// Freestyle: the judged TotalScore of the stage, and of the scored stages summed.
        public static float RiderScore(int r, int stage) => F(RidersAt + r * RiderSize + 156 + 4 * stage);
        public static float RiderTotalScore(int r) => F(RidersAt + r * RiderSize + 56);
    }
}
