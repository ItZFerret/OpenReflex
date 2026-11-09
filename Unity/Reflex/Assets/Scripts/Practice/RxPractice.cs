// RxPractice: PRACTICE's lap dispatcher. Once per completed game frame (OriginalWorldRuntime.CollectCompletedFrame, on the main thread,
// before the view is applied) it turns each new record of the core's lap log (Results.laps: rx_session_lap_log) into an RxLap: its time
// (ms = round(exact * 1000), the sub-frame time) and its validity (the first failing reason), then raises LapCompleted(lap) in serial
// order (RxPracticeHud lists them). Every exhibition lap race is dispatched; the career is not.
// Session holds this world's laps (a RESTART is a new session: the list starts over).
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.Practice
{
    /// One lap the race counted.
    public sealed class RxLap
    {
        public uint serial; public bool flying; public int ms; public float raw;          // ms = round(exact*1000)
        public string invalid;                                                              // null: clean
        public RxLapRecord record;                                                          // rx_lap_record as the core logged it
        public bool practice;                                                               // a PRACTICE session
        public int physClass = -1;                                                          // rx_session_local_phys_class (RX_PS_*)
        public string level;                                                                // the level ridden
        public float exact => record.exact;
    }

    public static class RxPractice
    {
        /// The flying-lap rules: seconds off the track a clean lap may have, the shortest lap counted (ms).
        public const float OffTrackGrace = 2f;
        public const int MinLapMs = 5000;
        /// The reasons a lap is not counted (the lap list shows the first one that fails).
        public const string FromTheGate = "FROM THE GATE", Reset = "RESET", OffTrack = "OFF TRACK", Cut = "CUT", WrongWay = "WRONG WAY",
                            MapScale = "MAP SCALE", TooShort = "TOO SHORT",
                            Marker = "MARKER",   // (w-lb-markers) the rider rode back to the SPAWN MARKER in the lap
                            TrackReset = "TRACK RESET";   // (w-fb-hud) RESET TRACK in the lap: the track changed under it

        /// A lap the race counted: main thread, in serial order.
        public static event Action<RxLap> LapCompleted;

        static readonly List<RxLap> s_laps = new List<RxLap>();
        static RxLap[] s_array = new RxLap[0];
        static object s_world;                  // the session the laps belong to (the world's simulation driver: a RESTART makes a new one)
        static int s_physClass = -1;
        static bool s_practice;

        /// The laps of this session (the lap list), oldest first.
        public static RxLap[] Session => s_array;
        /// The running world is a practice session (the HUD's lap list and the pause page's tiles).
        public static bool Active => s_practice && s_world != null;
        /// The lap being ridden: the last serial + 1 (laps_completed stops at 97; the serial does not). 0 before any world.
        public static int LapNumber => s_world == null ? 0 : (s_laps.Count > 0 ? (int)s_laps[s_laps.Count - 1].serial : 0) + 1;
        /// The fastest clean lap of the session (null: none yet).
        public static RxLap SessionBest { get; private set; }
        /// (w-lb-markers) The lap being ridden is an out-lap: the rider rode back to the SPAWN MARKER in it, so it is not counted and the timing
        /// restarts from the finish line (the next crossing ends it and starts a fresh flying lap).
        public static bool OutLap { get; private set; }

        /// (w-lb-markers) The rider rode back to the SPAWN MARKER (OriginalSpawnMarker, the core answered 0): the core logs the lap with its
        /// markers (Invalid: MARKER); the lap is an out-lap until the next crossing of the line.
        public static void MarkerUsed()
        {
            if (s_world == null) return;
            OutLap = true;
            Debug.Log("[Practice] SPAWN MARKER: the lap is not counted (MARKER); the timing starts again at the finish line");
        }

        /// (w-fb-hud) RESET TRACK in this session (ReflexTrackReset, the core answered 0): the lap being ridden is not counted (TRACK RESET);
        /// the timing goes on, the next lap counts.
        public static void TrackResetUsed()
        {
            if (s_world == null) return;
            s_trackResetLap = (uint)LapNumber;
            Debug.Log("[Practice] RESET TRACK: lap #" + s_trackResetLap + " is not counted (TRACK RESET)");
        }
        static uint s_trackResetLap;

        /// Called by OriginalWorldRuntime once per collected result (r: the frames completed since the last call).
        public static void OnFrame(OriginalWorldRuntime rt, OriginalSimulation.Results r)
        {
            if (!rt || r == null) return;
            object world = rt.SimDriver;
            if (world != s_world) Begin(rt, world);
            if (s_world == null) return;
            if (r.physClass >= 0) s_physClass = r.physClass;
            if (r.laps.Count == 0) return;
            if (rt.OwnedCareerActive) return;   // (the career: not dispatched)
            foreach (var rec in r.laps)
            {
                if (s_laps.Count > 0 && rec.serial <= s_laps[s_laps.Count - 1].serial) continue;   // (never twice)
                OutLap = false;   // (the crossing ends an out-lap: the next lap is timed from the line)
                var lap = Make(rt, rec);
                s_laps.Add(lap);
                s_array = s_laps.ToArray();
                if (lap.invalid == null && (SessionBest == null || lap.ms < SessionBest.ms)) SessionBest = lap;
                Debug.Log($"[Practice] lap #{lap.serial} {LapText(lap.ms)} (raw {lap.raw:0.000} s) {(lap.invalid ?? "clean")}  {lap.level ?? "-"}");
                var handlers = LapCompleted;
                if (handlers == null) continue;
                foreach (Action<RxLap> h in handlers.GetInvocationList())
                    try { h(lap); } catch (Exception e) { Debug.LogWarning("[Practice] LapCompleted: " + e); }
            }
        }

        /// A new world: the lap list starts over (also the HUD for a practice session).
        static void Begin(OriginalWorldRuntime rt, object world)
        {
            s_world = world; s_laps.Clear(); s_array = new RxLap[0]; SessionBest = null; s_physClass = -1; OutLap = false; s_trackResetLap = 0;
            s_practice = world != null && rt.IsPractice;
            if (world == null) return;
            if (s_practice && !rt.GetComponent<RxPracticeHud>()) rt.gameObject.AddComponent<RxPracticeHud>();
        }

        static RxLap Make(OriginalWorldRuntime rt, RxLapRecord rec)
        {
            var lap = new RxLap
            {
                serial = rec.serial, flying = rec.flying != 0, raw = rec.time, record = rec,
                ms = Mathf.RoundToInt(rec.exact * 1000f),
                practice = s_practice, physClass = s_physClass, level = rt.level,
            };
            lap.invalid = Invalid(lap, RxMapScale.OffDefault);   // (BASE SCALE: an Alive race level's own default counts as unscaled)
            if (lap.invalid == null && s_trackResetLap != 0 && rec.serial == s_trackResetLap) lap.invalid = TrackReset;   // (w-fb-hud)
            return lap;
        }

        /// The first failing rule (null: a clean flying lap).
        public static string Invalid(RxLap lap, bool mapScaled)
        {
            var r = lap.record;
            if (r.markers > 0) return Marker;   // (w-lb-markers; before FROM THE GATE: the core marks the lap not flying too)
            if (!lap.flying) return FromTheGate;
            if (r.resets > 0) return Reset;
            if (r.penalty > 0f) return Cut;
            if (r.off_track > OffTrackGrace) return OffTrack;
            if (r.wrong_way != 0) return WrongWay;
            if (mapScaled || (r.mods & RxLapRecord.ModMapScale) != 0) return MapScale;
            if (lap.ms < MinLapMs) return TooShort;
            return null;
        }

        /// The pause page's and the HUD's numbers: the lap being ridden, the last lap, the session best, clean / timed flying laps.
        public struct Totals { public int lap, clean, timed; public RxLap last, best; }
        public static Totals Summary()
        {
            var t = new Totals { lap = Math.Max(1, LapNumber), best = SessionBest };
            if (s_laps.Count > 0) t.last = s_laps[s_laps.Count - 1];
            foreach (var l in s_laps) { if (!l.flying) continue; t.timed++; if (l.invalid == null) t.clean++; }
            return t;
        }

        /// "1:23.456" (a lap time in ms).
        public static string LapText(int ms)
        {
            if (ms <= 0) return "-:--.---";
            int m = ms / 60000, s = ms / 1000 % 60, f = ms % 1000;
            return m.ToString(CultureInfo.InvariantCulture) + ":" + s.ToString("00", CultureInfo.InvariantCulture) + "." + f.ToString("000", CultureInfo.InvariantCulture);
        }
        /// "+0.312" / "-0.410" (a gap in ms).
        public static string GapText(int ms) => (ms >= 0 ? "+" : "-") + (Math.Abs(ms) / 1000f).ToString("0.000", CultureInfo.InvariantCulture);

        /// The world is gone (its scene unloads): its laps are forgotten unless a newer world took over already.
        public static void WorldGone(OriginalWorldRuntime rt) { if (rt && (s_world == null || rt.SimDriver == null || s_world == rt.SimDriver)) ResetSession(); }
        /// Tests and the showcase: forget the session (as a new world does).
        public static void ResetSession() { s_world = null; s_laps.Clear(); s_array = new RxLap[0]; SessionBest = null; s_practice = false; s_physClass = -1; OutLap = false; }
        /// The showcase: a practice session's laps without a world (the lap list and the pause page draw them).
        public static void ShowcaseSession(IEnumerable<RxLap> laps)
        {
            ResetSession(); s_world = new object(); s_practice = true;
            foreach (var l in laps) { s_laps.Add(l); if (l.invalid == null && (SessionBest == null || l.ms < SessionBest.ms)) SessionBest = l; }
            s_array = s_laps.ToArray();
        }
    }
}
