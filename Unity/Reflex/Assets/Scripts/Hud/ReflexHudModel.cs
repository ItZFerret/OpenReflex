// The data the race HUDs show, gathered in one place (Open Unleashed's MxaHudModel) from what OriginalHud already reads of the
// running game each frame (OriginalWorldRuntime: the race state reflex_world_race, every rider's standing reflex_world_race_drivers,
// the HUD export reflex_world_hud, the bike state), so the MODERN HUD and the game's own never disagree:
//   rank            race.place (1 before the start: Pre / Countdown), riders race.riders
//   laps            clamp(laps_completed + 1, 1, num_laps) / num_laps (MXUILapCountComponent)
//   lap time        the last lap while the game holds it (hud.hold_clock <= hud.hold_until: MXUILapTimeComponent's 3 s), else
//                   the running lap; LAST race.last_lap, BEST race.best_lap (none: FLT_MAX)
//   gaps            -(time_behind) to the leader (TimeBehindLeaderComponent), time_ahead (TimeAhead Component)
//   speed           hud.speed * 0.68182 = mph (the Speedometer Component's), km/h with the game's MeasurementSystem
//   gear            the bike state's gear (0-based: shown + 1)
//   rpm             the local rider's real engine speed (OriginalWorldRuntime.TryGetEngine: rx_session_vehicle_audio, what the engine sound
//                   follows), rpmGauge = its place in the useful band, (rpm - floor) / (limiter - floor) with floor = max(idle, 0.3 x
//                   limiter) (RpmFloor, w-revgauge): 0 at idle and below the floor, 1 on the limiter, 0 while the rider is wrecked / the
//                   engine drowned (the simulation holds a stale rpm then) (w-rpmgauge; the original HUD has no tachometer, only the mph number)
//   wrong way       hud.wrong_way while racing (RuleBackwards); off track hud.on_track == 0 while racing (RuleOfftrackTimePenalty),
//                   the penalty shown 3 s once back on track
//   final lap       the last lap (OneLapLeft) until the finish
//   countdown       a start board (National, Supercross) has none on the HUD (the board and the gate are the start, as the original); every other activity counts its own
//                   steps (race.countdown_step: 3, 2, 1 as MXUIBeginningCountdownComponentStub shows them, then GO at InProgress)
//   modes           HudMode from what the core says (the race format A+0x238: Eliminator 5 / Overtake 6 / Outrun 8; the activity type A+0x23c:
//                   Freestyle 5 / Section Race 12; the waypoint and challenge copies; a Free Ride has none of them), and per mode the
//                   core copies OriginalHud.RaceHud.cs draws: riders remaining, the 3 s "Player Eliminated" text, the Overtake start
//                   countdown (drivers x 1 s - 0.01, then STR_GO), the Outrun gate counter and section timer, the Section Race split /
//                   best / leaders, the event timer, the Freestyle judge bars and final score card, the Powercross start board, the
//                   Free Ride challenge's value, the Free Ride's Hit Discovery count (the discovery tracker's found / total, "HitDiscoveryFound" shows
//                   it for 3 s: rx_session_race_hud, w-pausemodes); the bike's heading for the compass
//   standings       every rider in place order (the drivers' places, laps, finish flag, times; names from the race's Player List,
//                   the local profile's name; numbers from the plates the game made)
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Hud
{
    public struct HudStanding
    {
        public int position, slot, laps;
        public bool you, ai, finished;
        public string name, number, gapText;
        public float raceTime, bestLap;
    }

    /// What the race is, for the HUD: which widgets it needs.
    public enum HudMode { None, Race, Eliminator, Overtake, Outrun, SectionRace, Freestyle, Waypoint, FreeRide, Challenge, Tutorial }

    public sealed class ReflexHudModel
    {
        // what the race is
        public HudMode mode;
        /// The Powercross start board is up in this race (the countdown is the board's, not the plain start timer).
        public bool powercross;
        /// Free Ride challenge kind (RxChallengeState.kind: 1 Hill Climb, 6 Trailblazer, 8 Target Practice ...).
        public int challengeKind = -1;
        /// The title of the mode ("RACE", "ELIMINATOR" ...).
        public string ModeTitle => ModeName(mode, challengeKind);
        /// The MODERN HUD has a layout for this race (a mode it does not cover keeps the game's own HUD).
        public bool Covered => mode != HudMode.None && mode != HudMode.Tutorial;
        public static string ModeName(HudMode m, int challengeKind)
        {
            switch (m)
            {
                case HudMode.Race: return "RACE"; case HudMode.Eliminator: return "ELIMINATOR"; case HudMode.Overtake: return "OVERTAKE";
                case HudMode.Outrun: return "OUTRUN"; case HudMode.SectionRace: return "SECTION RACE"; case HudMode.Freestyle: return "FREESTYLE";
                case HudMode.Waypoint: return "WAYPOINT RACE"; case HudMode.FreeRide: return "FREE RIDE";
                case HudMode.Challenge: return challengeKind == 1 ? "HILL CLIMB" : challengeKind == 6 ? "TRAILBLAZER" : challengeKind == 8 ? "TARGET PRACTICE" : "CHALLENGE";
                case HudMode.Tutorial: return "TUTORIAL";
                default: return "";
            }
        }

        // race
        public bool valid, inRace, inPost, racing, finished;
        public int state, place, riders, lap, laps, lapsCompleted;
        /// An endless lap race (PRACTICE: NumLaps 0x7fffffff, w-lb-practice): the lap shows as "LAP n" (n from the lap log: laps_completed stops at 97).
        public bool endless;
        public float lapTime, shownLapTime, lastLap, bestLap, raceTime, startTimer, resetTimer;
        /// The activity's own countdown step (3, 2, 1) in an activity without a start board; 0 / -1 none (the board's startTimer counts instead).
        public int countdownStep = -1;
        public bool holdingLastLap, hasBest, finalLap;
        /// RESTORED CONTENT (championship weekend TIMED RACES): the stage runs on a clock (timedSeconds) + timedExtra laps; until it runs out
        /// the lap total is the core's placeholder (RxChampWeekend.TimedLaps: never shown) and timedLeft counts down; then the lap total is
        /// final (timedToGo: the laps the local rider still has to cross the line, by the leader's count: a lapped rider is flagged with him);
        /// timedFlag: the leader took the flag, the local rider finishes at its next crossing. TimedText is what every lap counter shows.
        public bool timed, timedExpired, timedFlag; public float timedSeconds, timedLeft; public int timedExtra, timedToGo;
        /// The championship stage this session races ("MOTO 1"), null outside a weekend.
        public string champStage;
        /// A timed stage's lap counter: "4:59" while the clock runs, then "+1 LAP" / "+2 LAPS" (the laps after the one being ridden), then
        /// "FINAL LAP" (the white flag: the leader's last lap, or the flag is out). compact: the classic HUD's narrow counter ("+1", "FINAL").
        public string TimedText(bool compact)
        {
            if (!timed) return "";
            // (cached: the counters ask every frame; a new string only when what it says changes)
            int key = !timedExpired ? Mathf.CeilToInt(Mathf.Max(0f, timedLeft)) : finished ? -1 : timedFlag || timedToGo <= 1 ? -2 : -2 - timedToGo;
            int slot = compact ? 1 : 0;
            if (timedTextKey[slot] == key && timedTextCache[slot] != null) return timedTextCache[slot];
            string t;
            if (!timedExpired) t = (key / 60) + ":" + (key % 60).ToString("00");
            else if (finished) t = compact ? "FIN" : "FINISHED";
            else if (timedFlag || timedToGo <= 1) t = compact ? "FINAL" : "FINAL LAP";
            else { int more = timedToGo - 1; t = compact ? "+" + more : "+" + more + (more == 1 ? " LAP" : " LAPS"); }
            timedTextKey[slot] = key; timedTextCache[slot] = t;
            return t;
        }
        readonly int[] timedTextKey = { int.MinValue, int.MinValue }; readonly string[] timedTextCache = new string[2];
        /// "15:00 + 1 LAP": a timed stage's format (the HUD's sub line, the pause page).
        public string TimedFormat
        {
            get
            {
                int min = Mathf.RoundToInt(timedSeconds / 60f);
                if (timedFormatMin != min || timedFormatExtra != timedExtra || timedFormat == null)
                { timedFormatMin = min; timedFormatExtra = timedExtra; timedFormat = timedExtra <= 0 ? min + ":00 CLOCK" : min + ":00 + " + timedExtra + (timedExtra == 1 ? " LAP" : " LAPS"); }
                return timedFormat;
            }
        }
        int timedFormatMin = -1, timedFormatExtra = -1; string timedFormat;
        // the HUD export
        public bool waypoint, hasWaypoint, waypointDistanceValid;
        public int gatesPassed, gateCount; public float routeDistance;
        /// The Waypoint race's compass gates (the original's MXUICompassComponent, UI kind 6, 005ff150): the next gate (the slot after the local
        /// driver's last gate) and the one after it (drawn at 0.35), each as its world bearing from the bike (degrees, 0 = +Z, clockwise) and its
        /// XZ distance (feet); hidden once the local driver finished ("Player Wins", 00602ac0).
        public bool nextGateShown, afterGateShown; public float nextGateAngle, afterGateAngle, nextGateDistance;
        public bool haveHud, onTrack = true, wrongWay, penaltyShown;
        public float speedMph, timeBehind, timeAhead, penalty;
        public int activityState;
        // the bike
        public int gear;
        /// The engine (w-rpmgauge): hasRpm when the core reports it (the open path), rpm / idleRpm / maxRpm in rpm, rpmGauge 0 .. 1 (RpmGauge).
        public bool hasRpm; public float rpm, idleRpm, maxRpm, rpmGauge;
        /// Where the rev ring starts (w-revgauge): RpmFloorFraction of the limiter, or idle when that is higher (at most 0.9 of the limiter).
        /// Riding sits in the top of the rev range (core/tools/rpm_gauge_trace: the 250F upshifts at 11,200 .. 12,400 and lands on 9,750 ..
        /// 10,250 of 14,000; the ATV 9,400 .. 10,400 -> 7,500 .. 8,300 of 11,500), so a ring from idle lived in its top third and read like a
        /// speedometer. 0.3 x the limiter puts the 250F's floor at 4,200 (the ~4,000 asked for) and the ATV's at 3,450: riding fills the
        /// ring's middle and top (0.5 .. 0.86 between shifts), each upshift drops it by 0.1 .. 0.27 of the ring, idle reads empty. A fraction rather than a fixed rpm, so ATVs, UTVs, buggies and trucks with other
        /// limiters scale the same way. A missing / invalid idle counts as 0.
        public const float RpmFloorFraction = 0.3f;
        public static float RpmFloor(float idleRpm, float maxRpm)
        {
            if (!(idleRpm >= 0f) || idleRpm >= maxRpm) idleRpm = 0f;
            return Mathf.Min(Mathf.Max(idleRpm, RpmFloorFraction * maxRpm), 0.9f * maxRpm);
        }
        /// The rev gauge's fill: the engine speed's place between RpmFloor and the rev limiter, clamped to 0 .. 1 (empty at idle, full on the
        /// limiter). core/tools/rpm_gauge_trace.c computes the same to check it headless: keep the two in step.
        public static float RpmGauge(float rpm, float idleRpm, float maxRpm)
        {
            if (!(maxRpm > 0f)) return 0f;
            float lo = RpmFloor(idleRpm, maxRpm);
            return Mathf.Clamp01((rpm - lo) / (maxRpm - lo));
        }
        /// The bike's heading in degrees (0 = the world's +Z, clockwise), for the compass.
        public float heading;

        // banners and toasts (real-time deadlines, Time.unscaledTime): GO! after the gate drops, the white flag, the camera's name
        public float goUntil = -1f, finalLapUntil = -1f, cameraUntil = -1f; public string cameraToast;

        // Eliminator (MXUIEliminatorComponent "left/drivers"; PlayerEliminatedComponent 3 s)
        public int ridersLeft, ridersTotal;
        public string eliminatedText = ""; public float eliminatedUntil = -1f;
        // Overtake (MXUIOverTakeCountdownComponentStub): the start countdown, a whole number or STR_GO
        public bool overtakeShown; public string overtakeText = "";
        // Outrun (Outrunner Gate Indicator, OutrunnerSectionTimer)
        public bool gateShown, outrunTimerShown; public int gateNumber, gateTotal; public float outrunSectionTime;
        // the event timer (Freestyle, Section Race): the time left, or the clock of an activity that counts up
        // timerLow = the WarningTime flash is on: Freestyle's last 5 s (MXUITimerComponent armed by "FreestyleEnd", 00562bd0), until the time is 0
        public bool timerShown, timerCountsUp, timerLow; public float timerValue;
        // the stunt feedback overlay (MXUIStuntFeedbackOverlay): three slots, newest first; shown until 3 s after the last begin (stuntSerial counts the begins)
        public readonly string[] stuntText = { "", "", "" }; public bool stuntShown; public int stuntSerial;
        // Section Race (Timer Dashboard, Best Section, SectionLeadersContainer)
        public bool sectionShown; public int sectionCount, sectionCurrent; public float sectionTime, sectionBest;
        public uint leaderSet; public readonly string[] leaderNames = { "", "", "", "", "" };
        // Freestyle (FMXJudgesFeedback, Judges Panel)
        public readonly int[] judgeBars = new int[4]; public bool finalShown; public int finalSerial; public readonly float[] finalScores = new float[5];
        // Powercross start board (CountDownComponent): visible and the displayed step (3, 2, 1; 0 = GO)
        public bool pcVisible; public int pcStep;
        // Free Ride's Hit Discovery counter (MXUIHitDiscoveredComponent "%d/%d"; the tracker's counts in the challenges Free Ride's discovery mode):
        // total 0 = none; discoverySerial counts the finds, discoveryUntil is the real-time deadline of the 3 s the original shows the text for
        public int discoveryFound, discoveryTotal, discoverySerial; public float discoveryUntil = -1f;
        // a Free Ride challenge in progress
        public bool challengeValid, challengeValueValid; public int challengeState, challengeMedal; public float challengeValue;
        public readonly float[] challengeCriteria = new float[3];
        /// The wreck-recovery prompt (the original's "Arrows" component: rx_session_reflex_hud), see ReflexWreckHud.
        public readonly ReflexWreckHud wreck = new ReflexWreckHud();
        // the field (refreshed four times a second)
        public readonly List<HudStanding> standings = new List<HudStanding>(Reflex.Port.RxCore.RxRiderCap.Max);
        public int youRow = -1;
        public int Version { get; private set; }
        bool timerArmed; uint timerRestarts; bool timerRestartSynced;
        float stuntClock; bool stuntSynced; uint stuntSeen, stuntRestarts; static System.Collections.Generic.Dictionary<string, string> s_stuntNames;
        /// Changes whenever the field (standings) was refreshed.
        public int StandingsVersion { get; private set; }
        /// A producer of synthetic standings (the gallery) announces a changed field.
        public void MarkStandings() { StandingsVersion++; }

        bool wasOnTrack = true; float penaltyTimer, standingsAt;
        static readonly string[] LapTexts = new string[64];
        static string LapText(int lap) { lap = Mathf.Clamp(lap, 0, 63); return LapTexts[lap] ??= "L" + lap; }
        readonly HudStanding[] scratch = new HudStanding[Reflex.Port.RxCore.RxRiderCap.Max];
        string profileName; bool profileRead;

        /// From the runtime's last completed frame (dt: real time since the last fill, for the penalty's 3 s).
        public void Fill(OriginalWorldRuntime rt, float dt)
        {
            Version++;
            if (!rt) { valid = inRace = false; mode = HudMode.None; return; }
            var race = rt.Race; var hud = rt.HudState; haveHud = rt.HasHud;
            waypoint = rt.IsWaypoint; hasWaypoint = rt.HasWaypoint;
            var wp = rt.Waypoint;
            gateCount = wp.gate_count; gatesPassed = Mathf.Clamp(wp.gate_index + 1, 0, Mathf.Max(0, gateCount));
            waypointDistanceValid = hasWaypoint && wp.distance_valid != 0;
            routeDistance = wp.remaining_distance;
            valid = race.valid != 0;
            state = race.state;
            laps = Mathf.Max(0, race.num_laps);
            inRace = valid && laps > 0;
            inPost = valid && state == 4;
            racing = valid && state == 2;
            lapsCompleted = race.laps_completed;
            finished = inRace && lapsCompleted >= laps;
            place = state <= 1 ? 1 : Mathf.Max(1, race.place);
            riders = Mathf.Max(1, race.riders);
            lap = Mathf.Clamp(lapsCompleted + 1, 1, Mathf.Max(1, laps));
            endless = laps >= 0x61;
            if (endless) lap = Mathf.Max(lap, Reflex.Port.Practice.RxPractice.LapNumber);
            lapTime = race.lap_time; lastLap = race.last_lap; bestLap = race.best_lap; raceTime = race.race_time;
            startTimer = race.start_timer; resetTimer = race.reset_timer; countdownStep = race.countdown_step;
            hasBest = bestLap > 0 && bestLap < 3.0e38f;
            holdingLastLap = !waypoint && haveHud && hud.hold_clock <= hud.hold_until && lastLap > 0;
            shownLapTime = holdingLastLap ? lastLap : lapTime;
            finalLap = racing && laps > 0 && lapsCompleted == laps - 1;
            FillTimed(rt);
            gear = rt.Bike.gear;
            hasRpm = rt.TryGetEngine(out rpm, out idleRpm, out maxRpm);
            rpmGauge = hasRpm && !rt.EngineDead ? RpmGauge(rpm, idleRpm, maxRpm) : 0f;
            if (haveHud)
            {
                activityState = hud.activity_state;
                speedMph = Mathf.Max(0f, hud.speed * 0.68182f);
                timeBehind = -hud.time_behind; timeAhead = hud.time_ahead; penalty = hud.penalty;
                onTrack = hud.on_track != 0 || !racing;
                if (onTrack && !wasOnTrack && hud.penalty > 0) penaltyTimer = 3f;
                if (!onTrack) penaltyTimer = 0;
                wasOnTrack = onTrack;
                if (penaltyTimer > 0) penaltyTimer -= dt;
                penaltyShown = penaltyTimer > 0 && hud.penalty > 0;
                wrongWay = racing && hud.wrong_way != 0;
            }
            else
            {
                speedMph = Mathf.Max(0f, rt.Bike.speed * 0.68182f); onTrack = true; wrongWay = false; penaltyShown = false; activityState = 0;
            }
            if (Time.unscaledTime >= standingsAt) { standingsAt = Time.unscaledTime + 0.25f; Standings(rt); }
            FillModes(rt, dt, Time.unscaledTime);
            wreck.Fill(rt, Time.unscaledTime);
        }

        // RESTORED CONTENT (championship weekend TIMED RACES): the clock from the core (rx_session_champ_clock, read on the worker after each
        // frame: the activity's own InProgress clock, the expiry and the flag), else the stage's length against the local race time. The lap total
        // is the placeholder while the clock runs (RxChampWeekend.LapTotalUnknown): no widget may show it.
        void FillTimed(OriginalWorldRuntime rt)
        {
            champStage = Reflex.Port.RxCore.RxChampWeekend.StageInSession ? StageLabelOf(Reflex.Port.RxCore.RxChampWeekend.SessionStage) : null;
            timed = inRace && (Reflex.Port.RxCore.RxChampWeekend.TimedStageInSession(out timedSeconds, out timedExtra) || Reflex.Port.RxCore.RxChampWeekend.LapTotalUnknown(laps));
            timedFlag = false; timedToGo = 0;
            if (!timed) { timedExpired = false; timedLeft = 0f; return; }
            if (Reflex.Port.RxCore.RxChampWeekend.SessionClock(out var c))
            {
                timedSeconds = c.seconds; timedExtra = c.extra_laps;
                timedExpired = c.expired != 0 && !Reflex.Port.RxCore.RxChampWeekend.LapTotalUnknown(laps);
                timedFlag = c.flag != 0;
                timedLeft = Mathf.Max(0f, c.seconds - c.elapsed);
            }
            else
            {
                timedExpired = !Reflex.Port.RxCore.RxChampWeekend.LapTotalUnknown(laps);
                timedLeft = timedExpired ? 0f : Mathf.Max(0f, timedSeconds - (state >= 2 ? raceTime : 0f));
            }
            if (!timedExpired) { finalLap = false; return; }
            // the laps to go: the local rider's own count, but never more than the leader's (a lapped rider is flagged at its crossing after his)
            int leader = lapsCompleted;
            for (int i = 0; i < rt.DriverCount && i < rt.Drivers.Length; i++)
            {
                leader = Mathf.Max(leader, rt.Drivers[i].laps_completed);
                if (rt.Drivers[i].local != 0 && rt.Drivers[i].finished != 0) finished = true;   // (flagged with fewer laps than the total)
            }
            timedToGo = Mathf.Max(0, Mathf.Min(laps - lapsCompleted, laps - leader + (leader > lapsCompleted ? 1 : 0)));
            if (timedFlag && !finished) timedToGo = 1;
            finalLap = racing && !finished && timedToGo == 1;
        }
        /// Only what a timed stage's lap counter needs (the classic HUD, OriginalHud: its lap counter shows TimedText(true)).
        public void FillTimedOnly(OriginalWorldRuntime rt)
        {
            if (!rt) { timed = false; return; }
            var race = rt.Race;
            valid = race.valid != 0; state = race.state; laps = Mathf.Max(0, race.num_laps); inRace = valid && laps > 0; racing = valid && state == 2;
            lapsCompleted = race.laps_completed; finished = inRace && lapsCompleted >= laps; raceTime = race.race_time;
            FillTimed(rt);
        }
        static string s_stageLabel; static int s_stageLabelOf = -2;
        static string StageLabelOf(int stage)
        {
            int key = stage < 0 ? -1 : Reflex.Port.RxCore.RxChampWeekend.Discipline * 16 + stage;   // (a stage index names another stage in another discipline)
            if (key != s_stageLabelOf) { s_stageLabelOf = key; s_stageLabel = stage >= 0 ? Reflex.Port.RxCore.RxChampWeekend.StageLabel(stage) : null; }
            return s_stageLabel;
        }

        // ------------------------------------------------------------------------------------------------ the modes
        bool elimSynced, finalSynced, discoverySynced; uint elimSeen, finalSeen, leaderEventsSeen, leaderSetSeen, discoverySeen;
        bool overtakeArmed; float overtakeClock; int lastRaceState = -1, lastCam = -1; bool lastFinal;
        // MXUIStuntFeedbackOverlay (00603130 / 006032c0): the begins of the local player's tricks (rx_session_stunt_hud) move the slots up and
        // show the newest ("<trick>", "<stunt>" or "<trick> + <stunt>", MXRaven_Stunts); 3 s after the last, or at "Restart", it hides and empties
        void FillStunts(OriginalWorldRuntime rt, float dt)
        {
            if (rt.HasStuntHud)
            {
                var s = rt.StuntHud;
                if (!stuntSynced || s.events < stuntSeen) { stuntSynced = true; stuntSeen = s.events; stuntRestarts = s.restartEvents; ClearStunts(); }
                else
                {
                    if (s.restartEvents != stuntRestarts) { stuntRestarts = s.restartEvents; ClearStunts(); }
                    int fresh = (int)Mathf.Min(s.events - stuntSeen, s.count);
                    stuntSeen = s.events;   // (taken before the lines are made: a line that fails is dropped once, never retried each frame)
                    for (int k = fresh - 1; k >= 0; k--) StuntBegin(StuntLine(s.Trick(k), s.Stunt(k)));
                }
            }
            if (stuntShown)
            {
                if (stuntClock > 3f) ClearStunts();
                stuntClock += dt;
            }
        }
        void StuntBegin(string text)
        {
            if (text.Length == 0) return;
            stuntText[2] = stuntText[1]; stuntText[1] = stuntText[0]; stuntText[0] = text;
            stuntShown = true; stuntClock = 0f; stuntSerial++;
        }
        void ClearStunts() { stuntShown = false; stuntClock = 0f; stuntText[0] = stuntText[1] = stuntText[2] = ""; }
        static string StuntLine(string trick, string stunt)
        {
            string Name(string key)
            {
                if (key.Length == 0) return null;
                s_stuntNames ??= StuntNames();
                return s_stuntNames.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : key;
            }
            string a = Name(trick), b = Name(stunt);
            return a != null && b != null ? a + " + " + b : a ?? b ?? "";
        }
        // The trick names; an install whose table cannot be read shows the keys (once warned) instead of throwing out of every HUD frame.
        static System.Collections.Generic.Dictionary<string, string> StuntNames()
        {
            try { return OriginalUiAssets.Data.Strings("InGame", "MXRaven_Stunts"); }
            catch (Exception e) { Debug.LogWarning("HUD trick names not read (the keys are shown): " + e.Message); return new System.Collections.Generic.Dictionary<string, string>(); }
        }
        /// Synthetic input (the gallery): the begin of a trick with this line.
        public void DebugStunt(string line) { StuntBegin(line); }

        static readonly string[] IntTexts = new string[256];
        public static string IntText(int n) => (uint)n < 256u ? IntTexts[n] ??= n.ToString(System.Globalization.CultureInfo.InvariantCulture) : n.ToString(System.Globalization.CultureInfo.InvariantCulture);

        // true once when a counter of the core's events moved up (a drop means a new activity: the new count is the baseline)
        static bool Rose(ref bool synced, ref uint seen, uint now)
        {
            if (!synced) { synced = true; seen = now; return false; }
            bool rose = now > seen; seen = now; return rose;
        }

        void FillModes(OriginalWorldRuntime rt, float dt, float now)
        {
            bool haveFmt = rt.HasRaceFormat, haveRh = rt.HasRaceHud, haveCh = rt.HasChallenge;
            var fmt = haveFmt ? rt.RaceFormat : default;
            var rh = haveRh ? rt.RaceHud : default;
            var ch = haveCh ? rt.Challenge : default;
            powercross = rt.HasPowercross;
            string id = rt.EventIdentity;
            HudMode m;
            if (id != null && id.StartsWith("Tutorial/", StringComparison.Ordinal)) m = HudMode.Tutorial;
            else if (haveCh && ch.valid != 0) m = HudMode.Challenge;
            else if (waypoint) m = HudMode.Waypoint;
            else if (haveRh && rh.activityType == 5) m = HudMode.Freestyle;
            else if (haveRh && rh.activityType == 12) m = HudMode.SectionRace;
            else if (haveFmt && fmt.activityMode == 5) m = HudMode.Eliminator;
            else if (haveFmt && fmt.activityMode == 6) m = HudMode.Overtake;
            else if (haveFmt && fmt.activityMode == 8) m = HudMode.Outrun;
            else if (inRace) m = HudMode.Race;
            else if (valid || haveHud) m = HudMode.FreeRide;
            else m = HudMode.None;
            mode = m;

            // the challenge (Free Ride): the value the class keeps, the medal criteria
            challengeValid = m == HudMode.Challenge;
            challengeKind = challengeValid ? ch.kind : -1;
            if (challengeValid)
            {
                challengeState = ch.state; challengeMedal = ch.medal; challengeValueValid = ch.valueValid != 0; challengeValue = ch.value;
                challengeCriteria[0] = ch.criteria0; challengeCriteria[1] = ch.criteria1; challengeCriteria[2] = ch.criteria2;
            }

            // the event timer (MXUITimerComponent): Freestyle and Section Race; a challenge that has a time limit
            timerShown = false;
            if (haveRh && (rh.activityType == 5 || rh.activityType == 12 || (challengeValid && rh.clockLimit > 0.01f)))
            {
                timerShown = true; timerCountsUp = rh.clockCountsUp != 0;
                timerValue = timerCountsUp ? rh.clock : Mathf.Max(0f, rh.clockLimit - rh.clock);
                // 005f7140 / 00562bd0: the Freestyle activity raises "FreestyleEnd" each frame from Duration - 5 s (modes other than 0 / 2 / 3), which arms
                // the timer; a time shown of 0 disarms it (vf47); the event restarting puts it back. Nothing else warns (Section Race, the challenges).
                if (!timerRestartSynced || rh.restartEvents != timerRestarts) { timerRestartSynced = true; timerRestarts = rh.restartEvents; timerArmed = false; }
                if (rh.activityType == 5 && rh.activityMode != 0 && rh.activityMode != 2 && rh.activityMode != 3 && rh.clock >= rh.clockLimit - 5f) timerArmed = true;
                if (timerValue <= 0f) timerArmed = false;
            }
            else timerArmed = false;
            timerLow = timerShown && timerArmed;
            FillStunts(rt, dt);

            // Eliminator: riders remaining, and the message for 3 s of real time from the event (00605ec0)
            ridersTotal = haveFmt ? fmt.drivers : 0;
            ridersLeft = m == HudMode.Eliminator ? (state <= 1 || fmt.ridersLeft <= 0 || fmt.ridersLeft > fmt.drivers ? fmt.drivers : fmt.ridersLeft) : 0;
            if (haveRh && Rose(ref elimSynced, ref elimSeen, rh.eliminatedEvents))
            {
                eliminatedText = rh.EliminatorName + "\n" + HudStrings.Eliminated + "\n" + rh.EliminatedName + "!";
                eliminatedUntil = now + 3f;
            }

            // Overtake: at InProgress the clock = drivers x 1 s - 0.01, its whole seconds while >= 1, then STR_GO until -2 (005fbbf0)
            if (haveFmt && fmt.activityMode == 6 && state == 2 && lastRaceState != 2) { overtakeArmed = true; overtakeClock = fmt.drivers * 1f - 0.01f; }
            lastRaceState = state;
            if (overtakeArmed)
            {
                overtakeShown = true;
                overtakeText = overtakeClock >= 1f ? IntText((int)overtakeClock) : HudStrings.Go;
                if (overtakeClock <= -2f) { overtakeArmed = false; overtakeShown = false; }
                overtakeClock -= dt;
            }
            else overtakeShown = false;

            // Outrun: the gate counter from the first gate that gives time; the section timer from the first section on
            gateShown = haveRh && rh.activityMode == 8 && rh.gateEvents > 0;
            gateNumber = rh.gateNumber; gateTotal = rh.gateTotal;
            outrunTimerShown = haveFmt && fmt.activityMode == 8 && fmt.section >= 0;
            outrunSectionTime = fmt.sectionTime;

            // Section Race: the split time, the section's best and the leaders ("#n name" once a section has one)
            sectionShown = haveRh && rh.activityType == 12;
            if (sectionShown)
            {
                sectionCount = rh.sectionCount; sectionCurrent = rh.sectionCurrent; sectionTime = rh.sectionTime; sectionBest = rh.sectionBest;
                if (rh.leaderEvents != leaderEventsSeen || rh.leaderSet != leaderSetSeen)
                {
                    leaderEventsSeen = rh.leaderEvents; leaderSetSeen = rh.leaderSet; leaderSet = rh.leaderSet;
                    for (int i = 0; i < 5; i++) leaderNames[i] = (rh.leaderSet & (1u << i)) != 0 ? rh.LeaderName(i) : "";
                }
            }
            else { leaderSet = 0; leaderEventsSeen = 0; leaderSetSeen = 0; }

            // Freestyle: the judges' lit bars; the final score card (the five ratings, "%2.2f")
            if (haveRh && rh.activityType == 5)
            {
                for (int j = 0; j < 4; j++) judgeBars[j] = Mathf.Clamp(rh.JudgeBars(j), 0, 10);
                finalShown = rh.finalShown != 0;
                if (Rose(ref finalSynced, ref finalSeen, rh.finalEvents)) { for (int i = 0; i < 5; i++) finalScores[i] = rh.FinalScore(i); finalSerial++; }
            }
            else { for (int j = 0; j < 4; j++) judgeBars[j] = 0; finalShown = false; }

            // Hit Discovery (Free Ride): the counts the discovery tracker keeps, a find shows its text for 3 s (00604670)
            discoveryFound = haveRh && m == HudMode.FreeRide ? rh.discoveryFound : 0;
            discoveryTotal = haveRh && m == HudMode.FreeRide ? rh.discoveryTotal : 0;
            if (haveRh && Rose(ref discoverySynced, ref discoverySeen, rh.discoveryEvents)) { discoveryUntil = now + 3f; discoverySerial++; }
            else if (!haveRh) discoverySynced = false;

            // Powercross: the start board (CountDownComponent): visible, and its step (0 = GO)
            pcVisible = false;
            if (powercross)
            {
                var pc = rt.Powercross;
                if (pc.component_bound == 1 && pc.displayed_step >= 0 && pc.displayed_step <= 3) { pcVisible = pc.visible != 0; pcStep = pc.displayed_step; }
            }

            // the compass: the bike's heading
            var o = rt.Bike.renderOrientation;
            if (o != null && o.Length >= 4)
            {
                var f = new Quaternion(o[0], o[1], o[2], o[3]) * Vector3.forward;
                if (f.x * f.x + f.z * f.z > 1e-4f) { float h = Mathf.Atan2(f.x, f.z) * Mathf.Rad2Deg; heading = h < 0f ? h + 360f : h; }
            }
            // the Waypoint race's gates on the compass (rx_session_compass: the gates in the compass' slots, the local driver's last gate)
            nextGateShown = afterGateShown = false;
            var bp = rt.Bike.renderPosition;
            if (waypoint && rt.HasCompass && bp != null && bp.Length >= 3)
            {
                var cs = rt.Compass;
                if (cs.valid != 0 && cs.waypointFinished == 0)
                {
                    int next = cs.waypointGate + 1, n = Mathf.Min((int)cs.count, Reflex.Port.RxCore.RxCompassState.Marks);
                    for (int i = 0; i < n; i++)
                    {
                        if (cs.Type(i) != Reflex.Port.RxCore.RxCompassState.TypeGate) continue;
                        int k = cs.Index(i); if (k != next && k != next + 1) continue;
                        var gp = cs.Position(i);
                        float dx = gp.x - bp[0], dz = gp.z - bp[2], a = Mathf.Atan2(dx, dz) * Mathf.Rad2Deg; if (a < 0f) a += 360f;
                        if (k == next) { nextGateShown = true; nextGateAngle = a; nextGateDistance = Mathf.Sqrt(dx * dx + dz * dz); }
                        else { afterGateShown = true; afterGateAngle = a; }
                    }
                }
            }

            // banners and toasts
            if (lastStateForGo == 1 && state == 2) goUntil = now + 1.2f;
            lastStateForGo = state;
            if (finalLap && !lastFinal) finalLapUntil = now + 2.5f;
            lastFinal = finalLap;
            // the camera's name (a custom preset's own) on every switch (ReflexCameraPresets; the CLASSIC HUD's is ReflexCameraToast)
            int cam = Reflex.Port.Options.ReflexCameraPresets.ViewKey(rt.cameraMode);
            if (lastCam >= 0 && cam != lastCam)
            {
                cameraToast = Reflex.Port.Options.ReflexCameraPresets.DisplayName(rt.cameraMode);
                cameraUntil = Reflex.Port.Options.ReflexSettings.CameraToast ? now + 1.8f : -1f;
            }
            lastCam = cam;
        }
        int lastStateForGo = -1;

        void Standings(OriginalWorldRuntime rt)
        {
            standings.Clear(); youRow = -1; StandingsVersion++;
            int n = Mathf.Min(rt.DriverCount, scratch.Length);
            // the local rider's name: the garage's NAME (the profile's persona); the world's Player List names the local player "Player 1"
            // whatever the profile says
            if (!profileRead) { profileRead = true; profileName = Reflex.Port.Content.ReflexProfile.HasPersona ? Reflex.Port.Content.ReflexProfile.Persona : null; }
            for (int i = 0; i < n; i++)
            {
                var d = rt.Drivers[i];
                ReflexRoster.Entry entry = null;
                var roster = rt.Roster;
                if (roster != null) for (int k = 0; k < roster.entries.Count; k++) if (roster.entries[k].slot.slot == d.slot) { entry = roster.entries[k]; break; }
                bool you = d.local != 0;
                scratch[i] = new HudStanding
                {
                    position = Mathf.Max(1, d.place), slot = d.slot, laps = d.laps_completed, you = you, ai = entry != null && entry.Computer, finished = d.finished != 0,
                    name = ShortName(you ? (!string.IsNullOrEmpty(profileName) ? profileName : !string.IsNullOrEmpty(entry?.name) ? entry.name : "Player") : !string.IsNullOrEmpty(entry?.name) ? entry.name : "Rider " + (d.slot + 1)),
                    number = entry?.plate?.number ?? "", raceTime = d.race_time, bestLap = d.best_lap,
                    gapText = you ? "---" : d.finished != 0 ? "FIN" : LapText(Mathf.Clamp(d.laps_completed + 1, 1, Mathf.Max(1, laps))),
                };
            }
            // place order (insertion sort: at most RxRiderCap.Max, no garbage)
            for (int i = 1; i < n; i++) { var x = scratch[i]; int j = i - 1; while (j >= 0 && scratch[j].position > x.position) { scratch[j + 1] = scratch[j]; j--; } scratch[j + 1] = x; }
            for (int i = 0; i < n; i++) { standings.Add(scratch[i]); if (scratch[i].you) youRow = i; }
        }

        static readonly Dictionary<string, string> s_short = new Dictionary<string, string>(StringComparer.Ordinal);
        /// "J. Cooper" from "JOSH COOPER" (Open Unleashed's MxaHudKit.ShortName), cached.
        public static string ShortName(string full)
        {
            if (string.IsNullOrEmpty(full)) return "";
            if (s_short.TryGetValue(full, out var s)) return s;
            var ti = System.Globalization.CultureInfo.InvariantCulture.TextInfo;
            string f = full.Trim(); int sp = f.LastIndexOf(' ');
            bool numbered = sp > 0 && sp + 1 < f.Length && char.IsDigit(f[sp + 1]);   // "Player 1": kept whole
            s = sp <= 0 || numbered ? ti.ToTitleCase(f.ToLowerInvariant()) : char.ToUpperInvariant(f[0]) + ". " + ti.ToTitleCase(f.Substring(sp + 1).ToLowerInvariant());
            if (s_short.Count < 256) s_short[full] = s;
            return s;
        }

        /// The place text "1ST" ... (the game's rank component style).
        public static string Ordinal(int n) => n + ReflexHudKit.Ordinal(n);
    }
}
