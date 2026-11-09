// What the owned in-race pages (OwnRacePause, OwnRaceResults) show for each offline mode, as data the pages draw (no UI here, so the
// showcase and the tests feed synthetic values). One mode vocabulary for both: the HUD's (HudMode: Race, Eliminator, Overtake, Outrun,
// Waypoint, SectionRace, Freestyle, FreeRide, Challenge), decided by ReflexHudModel from the core's exports (the activity type A+0x23c,
// the race format A+0x238, the waypoint / challenge copies).
//
// PAUSE (the original's PauseMenu.lua, run in the game's world and recorded in OriginalEventIntro.PauseRowsLua):
//   every mode        RESUME ... TRICK LIBRARY, OPTIONS, QUIT
//   RESTART           every mode but the Free Ride (needsRestart = FALSE for an eventType of FreeRide); an activated challenge (isAAA) has it again
//   VEHICLE TUNING    the Free Ride and its challenges (needsVehicleTuning stays TRUE from the FreeRide branch) when the equipped vehicle is the
//                     player's own (GetEquippedVehicle owner == Player1)
//   VEHICLE LIST      the Free Ride only (needsVehicleSelect; the challenge's isAAA clears it)
//   the Free Ride map RESUME's container: the location's HUDMapIconId with the rider's arrow ((x + 2048) / 4096 * 388, 388 - (z + 2048) / 4096 * 388);
//                     an activated challenge shows its title and description instead (ChallengeEventContainer: STR_<ACTIVITY>_TITLE / _DESCRIPTION)
// The stat tiles are the port's own (the original's pause page has none): what the HUD of that mode counts, from the same core copies.
//
// RESULTS (the original's PostEventResults table, 00613dd0: the column list by activity type): races TOTAL TIME / PENALTY TIME / BEST LAP TIME (the
// Eliminator: ELIMINATED, TOTAL TIME, BEST LAP TIME), Freestyle TOTAL SCORE / BEST TRICK SCORE / BEST TRICK, Waypoint TOTAL TIME, Section Race
// SECTIONS / BEST SECTION / BEST SECTION TIME. The core runs the race tables (rx_session_results_page); the others come from rx_session_mode_results.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    /// One stat tile: a label over a value; tone "good" (green), "gold" or null.
    public sealed class OwnTile
    {
        public readonly string label, value, tone;
        public OwnTile(string label, string value, string tone = null) { this.label = label; this.value = value; this.tone = tone; }
    }

    /// What is known about the running event beyond the HUD model (the runtime fills it; the showcase writes it).
    public struct OwnModeContext
    {
        /// "NATIONAL  ·  CONCORD" (OpenEvent.Label), the location alone ("ALPINE"), the player's vehicle ("BLAZE 250R").
        public string eventLabel, location, vehicle;
        /// A series event of the open career (its RESTART restarts the event's activity in place; the career keeps its state).
        public bool career;
        /// An activated Free Ride challenge (EventTypeActivation set), and its activation name ("HillClimb").
        public bool challengeWorld; public string activation;
        /// The equipped vehicle is the player's own: VEHICLE TUNING is offered.
        public bool ownVehicle;
        /// The Free Ride's map icon (EventTypeLocations HUDMapIconId, -1 none) and the rider's position (game x / z) and heading (degrees from +Z, clockwise).
        public int mapIcon; public bool hasBike; public float bikeX, bikeZ, heading;
        /// The Freestyle / Section Race standings row of the local rider (rx_session_mode_results), when the core exports it.
        public bool hasLocal; public RxModeResultRow local;
        /// The vehicle's speed text ("56 MPH") for the Free Ride.
        public string speed;
        /// The path the local bike has ridden in this world, in feet (OriginalWorldRuntime.RideDistance); 0: none yet.
        public float distance;
        /// PRACTICE (w-lb-practice): the world is a practice session (OriginalWorldRuntime.IsPractice): END PRACTICE, the lap list's tiles.
        public bool practice;
        /// RESET TRACK (w-fb-hud) is offered: an offline single-rider ride (ReflexTrackReset.Unavailable: Free Ride, PRACTICE, a race without AI).
        public bool resetTrack;
    }

    public sealed class OwnPauseSpec
    {
        public HudMode mode;
        public string title = "PAUSED", caption = "", modeName = "";
        public readonly List<OwnTile> tiles = new List<OwnTile>();
        public bool restart, vehicleTuning, vehicleList, map;
        /// RESET TRACK (w-fb-hud): the ruts and tyre tracks back to the untouched track, the rider where he is (ReflexTrackReset).
        public bool resetTrack;
        public int mapIcon = -1; public bool hasBike; public float bikeX, bikeZ, heading;
        public string challengeTitle, challengeText;
        /// The confirmation popups' wording (RESTART EVENT? / QUIT EVENT?; a challenge's quit goes back to the Free Ride).
        public string quitLabel = "QUIT TO MENU", quitTitle = "LEAVE EVENT?", quitLead = "YOUR CURRENT RACE WILL END", restartLead = "YOUR CURRENT RACE WILL END";
        /// A championship weekend's stage (RESTORED CONTENT): RETIRE's row and its confirmation (null: no such row).
        public string retireLabel, retireTitle, retireLead;
    }

    public sealed class OwnBoardColumn
    {
        /// css = the board class that gives the width ("ou-board-time" 168, "ou-board-best" 168, "ou-board-status" 190, "ou-board-points" 150).
        public readonly string header, css;
        public OwnBoardColumn(string header, string css) { this.header = header; this.css = css; }
    }

    public sealed class OwnResultsRow
    {
        public int slot, rank; public string name; public bool you;
        public string[] cells;
    }

    /// One results page: the title, the side column (the place and the tiles), the board.
    public sealed class OwnResultsSpec
    {
        public HudMode mode;
        public string title = "RACE RESULTS", caption = "";
        /// 0: no place number (Freestyle, a board of one).
        public int place;
        public readonly List<OwnTile> tiles = new List<OwnTile>();
        public readonly List<OwnBoardColumn> columns = new List<OwnBoardColumn>();
        public readonly List<OwnResultsRow> rows = new List<OwnResultsRow>();
        /// The Freestyle judges' final scores (Overall, Difficulty, Execution, Flow, Variety), empty elsewhere.
        public readonly List<OwnTile> judges = new List<OwnTile>();
        public string againLabel = "RACE AGAIN", againTitle = "RACE AGAIN?", againLead = "Start a new race with the same event settings?";
    }

    public static class OwnModes
    {
        // the engine's activity enum (ReflexEvents.Activities): the modes the shell offers offline
        public const int National = 0, Supercross = 1, Freestyle = 5, Waypoint = 6, FreeRide = 8, SectionRace = 12, Terracross = 17, Powercross = 19, HillClimb = 22, TargetPractice = 29;

        /// Activity types the owned pause covers: the races, the Waypoint and Section Race, Freestyle, the Free Ride and its challenges.
        public static bool PauseType(int type)
        {
            type = RxChampWeekend.BaseType(type);   // RESTORED CONTENT: a championship weekend's stage as its discipline's race
            switch (type)
            {
                case National: case Supercross: case Freestyle: case Waypoint: case FreeRide: case SectionRace: case Terracross: case Powercross: case 18: return true;
            }
            return type >= HillClimb && type <= 30;
        }

        /// Activity types that have a results page of their own (the challenges have OwnPostChallenge, the Free Ride none).
        public static bool ResultsType(int type)
        {
            type = RxChampWeekend.BaseType(type);   // RESTORED CONTENT: a championship weekend's stage as its discipline's race
            switch (type) { case National: case Supercross: case Freestyle: case Waypoint: case SectionRace: case Terracross: case Powercross: return true; }
            return false;
        }

        /// The races whose results the core's table covers and whose end needs laps (the checks of a finished local rider).
        public static bool LapType(int type) => (type = RxChampWeekend.BaseType(type)) == National || type == Supercross || type == Terracross || type == Powercross;

        // ------------------------------------------------------------------------------------------------ strings

        static string S(string key, string fallback)
        {
            try { string s = OpenContent.Str(key, null); if (!string.IsNullOrEmpty(s) && !s.StartsWith("STR_", StringComparison.Ordinal)) return s; } catch (Exception) { }
            return fallback;
        }
        static string Caps(string key, string fallback) => S(key, fallback).ToUpperInvariant();
        static string Num(float v) => v.ToString("0.00", CultureInfo.InvariantCulture);
        /// The game table's cell text, else the presenter's own text of the same value (before the page started).
        static string Cell(OwnRaceResultsSnapshot.Row r, string column, string fallback) => r.TryPageCell(column, out var text, out _) ? text : fallback;
        /// The Eliminator's ELIMINATED cell (00614950 column 9: the localized ordinal 005dc7a0): the place is the cell's sort value, the
        /// suffix the install's string of the original's key (place 1 / 2 / 3 / any other: STR_FIRST / STR_SECOND / STR_THIRD /
        /// STR_FOURTH, so 21 is "21TH" as in the game, not "21ST"); the core's text (the English text the oracle recorded) when the install has none; the
        /// standings leader's cell is empty.
        internal static string EliminatedPlace(float sort, string cell)
        {
            int place = (int)sort;
            if (place <= 0 || string.IsNullOrEmpty(cell)) return cell ?? "";
            string key = place == 1 ? "STR_FIRST" : place == 2 ? "STR_SECOND" : place == 3 ? "STR_THIRD" : "STR_FOURTH";
            string suffix = HudStrings.Get(key, null);
            return string.IsNullOrEmpty(suffix) ? cell : place.ToString(CultureInfo.InvariantCulture) + suffix;
        }
        static string Time(float t) => OuWidgets.Clock(t);
        /// A path length: feet / metres under a mile / a kilometre, then miles / kilometres with one decimal.
        internal static string LongDist(float feet, bool metric)
        {
            if (metric) { float m = feet * 0.3048f; return m < 1000f ? m.ToString("0", CultureInfo.InvariantCulture) + " M" : (m / 1000f).ToString("0.0", CultureInfo.InvariantCulture) + " KM"; }
            return feet < 5280f ? feet.ToString("0", CultureInfo.InvariantCulture) + " FT" : (feet / 5280f).ToString("0.0", CultureInfo.InvariantCulture) + " MI";
        }
        static string Dist(float feet, bool metric) => (feet * (metric ? 0.3048f : 1f)).ToString("0", CultureInfo.InvariantCulture) + (metric ? " M" : " FT");
        /// A stunt's display name ("STR_TWISTER" -> "Twister": MXRaven_Stunts), or the key without its prefix.
        public static string TrickName(string key)
        {
            if (string.IsNullOrEmpty(key)) return "";
            try
            {
                var names = OriginalUiAssets.Data.Strings("InGame", "MXRaven_Stunts");
                if (names.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v)) return v.ToUpperInvariant();
            }
            catch (Exception) { }
            return (key.StartsWith("STR_", StringComparison.Ordinal) ? key.Substring(4) : key).Replace('_', ' ');
        }

        // ------------------------------------------------------------------------------------------------ from the running world

        /// "NATIONAL  ·  CONCORD" for the event of the world (the shell's own label), a challenge or other event the shell's list lacks by its type and level.
        public static string EventLabel(OriginalWorldRuntime rt)
        {
            if (!rt) return "";
            try
            {
                var oe = OpenContent.Find(rt.level);
                if (oe != null) return oe.Label;
                var ev = ReflexEvents.Find(rt.level);
                if (ev != null)
                {
                    string place = PlaceName(ev);
                    string type = !string.IsNullOrEmpty(ev.typeName) ? ev.typeName : !string.IsNullOrEmpty(ev.spec.activation) ? ev.spec.activation : ev.spec.type;
                    return (type + "  ·  " + place).ToUpperInvariant();
                }
            }
            catch (Exception e) { Debug.LogWarning("[OwnModes] event label: " + e.Message); }
            return "";
        }

        /// The location's name as the shell lists it ("HILLSGROVE, NY"): any event of the shell's list at that level, else the level's name.
        public static string PlaceName(ReflexEvents.Event ev)
        {
            if (ev == null) return "";
            try { var any = OpenContent.Events.FirstOrDefault(x => string.Equals(x.Location, ev.location, StringComparison.OrdinalIgnoreCase)); if (any != null && !string.IsNullOrEmpty(any.locationName)) return any.locationName; }
            catch (Exception) { }
            return ev.location;
        }

        /// The results of a settled world: the HUD model's mode, the label, the core's mode values (the Waypoint / Freestyle / Section Race standings, the judges).
        public static OwnRaceResultsSnapshot.Context ResultsContext(OriginalWorldRuntime rt)
        {
            var model = new ReflexHudModel();
            model.Fill(rt, 0f);
            return new OwnRaceResultsSnapshot.Context
            {
                Mode = model.mode == HudMode.None || model.mode == HudMode.FreeRide || model.mode == HudMode.Challenge ? HudMode.Race : model.mode,
                EventLabel = EventLabel(rt), HasModeResults = rt.HasModeResults, ModeResults = rt.ModeResults, HasRaceHud = rt.HasRaceHud, RaceHud = rt.RaceHud,
            };
        }

        /// The HUD model filled from the paused world and the context the pause page needs (the Free Ride's map and vehicle, the challenge, the career).
        public static OwnModeContext PauseContext(OriginalWorldRuntime rt, ReflexHudModel model)
        {
            var c = new OwnModeContext { eventLabel = EventLabel(rt) };
            if (!rt) return c;
            c.practice = rt.IsPractice;
            var ev = ReflexEvents.Find(rt.level);
            c.career = rt.OwnedCareerActive;
            c.challengeWorld = ev != null && !string.IsNullOrEmpty(ev.spec.activation);
            c.activation = ev?.spec.activation;
            c.ownVehicle = OriginalVehicleTuningPopup.Offered(rt);
            if (ev != null && ev.spec.type == "FreeRide")
            {
                c.location = PlaceName(ev);
                c.mapIcon = MapIcon(ev);
            }
            try
            {
                var g = rt.garage;
                if (g != null && g.vehicleOwner == ReflexWorldFiles.Garage.AI)
                {   // a pro rider's ride (the VEHICLE LIST's PRO RIDERS, or a pro chosen at Vehicle Select): his name and the vehicle type
                    var ride = OpenGarageData.ProRides().FirstOrDefault(r => r.vehicle == g.vehicleId && (g.riderOwner != ReflexWorldFiles.Garage.AI || r.rider == g.riderId));
                    if (ride != null) c.vehicle = ride.name + " · " + OpenGarageData.TypeName(ride.vehicleType).ToUpperInvariant();
                }
                if (c.vehicle == null) c.vehicle = OpenGarageData.Vehicles().FirstOrDefault(v => v.equipped)?.title?.ToUpperInvariant();
            }
            catch (Exception) { }
            if (rt.TryGetBike(out var pos, out var vel)) { c.hasBike = true; c.bikeX = pos.x; c.bikeZ = pos.z; c.heading = model.heading; }
            c.distance = rt.RideDistance;
            c.resetTrack = ReflexTrackReset.Unavailable(rt).Length == 0;
            if (rt.TryGetVehicle(out var info)) c.speed = Mathf.RoundToInt(ReflexSettings.Speed(Mathf.Abs(info.speed) * 0.681818f)) + (ReflexSettings.Metric ? " KM/H" : " MPH");
            if (rt.HasModeResults && rt.ModeResults.TryLocal(out var row)) { c.hasLocal = true; c.local = row; }
            return c;
        }

        /// RefreshFreeRideMap: the location's EventTypeLocations row of the Free Ride (EventTypeId 9): HUDMapIconId, -1 none.
        public static int MapIcon(ReflexEvents.Event ev)
        {
            try
            {
                foreach (var r in FeTables.Shared.Rows("EventTypeLocations"))
                    if (FeTables.I(r, "EventTypeId") == 9 && FeTables.I(r, "EventLocationId") == ev.spec.locationId) return FeTables.I(r, "HUDMapIconId", -1);
            }
            catch (Exception e) { Debug.LogWarning("[OwnModes] map icon: " + e.Message); }
            return -1;
        }

        // ------------------------------------------------------------------------------------------------ the pause page

        /// The pause page of the running mode (m: the HUD model filled from the paused world).
        public static OwnPauseSpec Pause(ReflexHudModel m, ReflexRaceState race, OwnModeContext c)
        {
            bool metric = ReflexSettings.Metric;
            var spec = new OwnPauseSpec { mode = m.mode, modeName = ReflexHudModel.ModeName(m.mode, m.challengeKind) };
            var t = spec.tiles;
            string where = !string.IsNullOrEmpty(c.eventLabel) ? c.eventLabel : spec.modeName;
            spec.caption = where;
            // the rows (PauseMenu.lua)
            bool freeRide = m.mode == HudMode.FreeRide, challenge = m.mode == HudMode.Challenge || c.challengeWorld;
            spec.restart = !freeRide;
            spec.vehicleTuning = (freeRide || challenge) && c.ownVehicle;
            spec.vehicleList = freeRide && !challenge;
            spec.resetTrack = c.resetTrack;
            if (challenge) { spec.quitLabel = "QUIT TO FREE RIDE"; spec.quitTitle = "QUIT CHALLENGE?"; spec.quitLead = "YOU RETURN TO THE FREE RIDE"; spec.restartLead = "THE CHALLENGE STARTS AGAIN"; }
            else if (freeRide) { spec.quitTitle = "LEAVE FREE RIDE?"; spec.quitLead = "YOU RETURN TO THE MENU"; }
            if (m.champStage != null && !c.practice)
            {   // RESTORED CONTENT (championship weekend): RESTART runs the same stage again, RETIRE ends it (you last) and the weekend goes on,
                // QUIT WEEKEND leaves the weekend for the menu
                spec.caption = "CHAMPIONSHIP WEEKEND  ·  " + m.champStage + "  ·  " + where;
                spec.restartLead = "THE " + m.champStage + " STARTS AGAIN";
                spec.retireLabel = "RETIRE FROM " + m.champStage; spec.retireTitle = "RETIRE FROM THE " + m.champStage + "?"; spec.retireLead = "YOU ARE CLASSED LAST · THE WEEKEND GOES ON";
                spec.quitLabel = "QUIT WEEKEND"; spec.quitTitle = "QUIT THE WEEKEND?"; spec.quitLead = "THE WEEKEND ENDS · YOU RETURN TO THE MENU";
            }
            if (c.practice)
            {   // PRACTICE (w-lb-practice): RESUME, RESTART (a new session from the gate, its lap list empty), END PRACTICE (the practice setup)
                spec.modeName = "PRACTICE"; spec.caption = "PRACTICE  ·  " + where;
                spec.quitLabel = "END PRACTICE"; spec.quitTitle = "END PRACTICE?"; spec.quitLead = "YOU RETURN TO THE PRACTICE SETUP";
                spec.restartLead = "THE PRACTICE STARTS AGAIN FROM THE GATE";
                var p = Reflex.Port.Practice.RxPractice.Summary();
                t.Add(new OwnTile("LAP", p.lap.ToString(CultureInfo.InvariantCulture)));
                if (p.last != null) t.Add(new OwnTile("LAST LAP", Reflex.Port.Practice.RxPractice.LapText(p.last.ms) + (p.last.invalid != null ? "  ·  " + p.last.invalid : ""), p.last.invalid == null ? null : "gold"));
                if (p.best != null) t.Add(new OwnTile("SESSION BEST", Reflex.Port.Practice.RxPractice.LapText(p.best.ms), "good"));
                t.Add(new OwnTile("CLEAN LAPS", p.clean + " / " + p.timed));
                if (race.race_time > 0) t.Add(new OwnTile("SESSION TIME", Time(race.race_time)));
                if (ReflexSettings.SpawnMarkers)
                {   // SPAWN MARKERS in practice (w-lb-markers): the controls and the MX Bikes rule, in the page's aside panel
                    spec.challengeTitle = "SPAWN MARKER";
                    spec.challengeText = "HOLD " + OriginalSpawnMarker.SetControls() + " TO KEEP YOUR PLACE AS THE MARKER, PRESS " + OriginalSpawnMarker.RideControls() + " TO RIDE BACK THERE. " +
                                         "RIDING BACK ENDS THE LAP (NOT COUNTED: MARKER); THE TIMING STARTS AGAIN AT THE FINISH LINE. KEEPING A MARKER CHANGES NOTHING.";
                }
                return spec;
            }
            switch (m.mode)
            {
                case HudMode.Race: case HudMode.Eliminator: case HudMode.Overtake: case HudMode.Outrun:
                {
                    int laps = Mathf.Max(1, race.num_laps), lap = Mathf.Clamp(race.laps_completed + 1, 1, laps);
                    // (a timed championship stage: the clock while it runs, then the laps left; never the placeholder lap total)
                    if (m.timed) t.Add(new OwnTile(m.timedExpired ? "LAP" : "CLOCK", m.timedExpired ? lap + "  ·  " + m.TimedText(false) : Time(m.timedLeft) + " LEFT", m.timedExpired && (m.timedFlag || m.timedToGo <= 1) ? "gold" : null));
                    else t.Add(new OwnTile("LAP", lap + " / " + laps));
                    t.Add(new OwnTile("POSITION", race.place + " / " + race.riders, race.place == 1 ? "gold" : null));
                    if (m.mode == HudMode.Eliminator && m.ridersTotal > 0) t.Add(new OwnTile("RIDERS LEFT", m.ridersLeft + " / " + m.ridersTotal));
                    if (m.mode == HudMode.Outrun && m.gateShown) t.Add(new OwnTile(HudStrings.Gate.TrimEnd(':'), m.gateNumber + " / " + m.gateTotal));
                    if (race.race_time > 0) t.Add(new OwnTile("RACE TIME", Time(race.race_time)));
                    if (m.mode == HudMode.Outrun && m.outrunTimerShown) t.Add(new OwnTile("SECTION TIME", Time(m.outrunSectionTime)));
                    if (race.best_lap > 0 && race.best_lap < 3.0e38f) t.Add(new OwnTile("BEST LAP", Time(race.best_lap), "good"));
                    break;
                }
                case HudMode.Waypoint:
                {
                    if (m.hasWaypoint && m.gateCount > 0) t.Add(new OwnTile("GATES", m.gatesPassed + " / " + m.gateCount));
                    if (race.riders > 1) t.Add(new OwnTile("POSITION", race.place + " / " + race.riders, race.place == 1 ? "gold" : null));
                    if (race.num_laps > 1) t.Add(new OwnTile("LAP", Mathf.Clamp(race.laps_completed + 1, 1, race.num_laps) + " / " + race.num_laps));
                    if (race.race_time > 0) t.Add(new OwnTile("TIME", Time(race.race_time)));
                    if (m.waypointDistanceValid) t.Add(new OwnTile("TO NEXT GATE", Dist(m.routeDistance, metric)));
                    if (race.best_lap > 0 && race.best_lap < 3.0e38f) t.Add(new OwnTile("BEST LAP", Time(race.best_lap), "good"));
                    break;
                }
                case HudMode.SectionRace:
                {
                    if (m.sectionCount > 0) t.Add(new OwnTile("SECTION", (m.sectionCurrent >= 0 ? (m.sectionCurrent + 1).ToString(CultureInfo.InvariantCulture) : "—") + " / " + m.sectionCount));
                    if (m.timerShown) t.Add(new OwnTile(m.timerCountsUp ? "TIME" : "TIME LEFT", Time(m.timerValue)));
                    if (c.hasLocal) t.Add(new OwnTile("SECTIONS WON", c.local.sectionsWon.ToString(CultureInfo.InvariantCulture), c.local.sectionsWon > 0 ? "gold" : null));
                    if (m.sectionBest > 0 && m.sectionBest < 3.0e38f) t.Add(new OwnTile("SECTION BEST", Time(m.sectionBest), "good"));
                    else if (m.sectionCurrent >= 0) t.Add(new OwnTile("SPLIT", Time(m.sectionTime)));
                    break;
                }
                case HudMode.Freestyle:
                {
                    if (m.timerShown) t.Add(new OwnTile(m.timerCountsUp ? "TIME" : "TIME LEFT", Time(m.timerValue)));
                    if (c.hasLocal) t.Add(new OwnTile("SCORE", Num(c.local.score), "gold"));
                    if (c.hasLocal && c.local.hasBestTrick != 0) t.Add(new OwnTile("BEST TRICK", TrickName(c.local.BestTrick) + "  " + Num(c.local.bestTrickScore), "good"));
                    int lit = 0; for (int j = 0; j < 4; j++) lit += m.judgeBars[j];
                    if (lit > 0) t.Add(new OwnTile("JUDGES", m.judgeBars[0] + "  " + m.judgeBars[1] + "  " + m.judgeBars[2] + "  " + m.judgeBars[3]));
                    break;
                }
                case HudMode.FreeRide:
                {
                    // (the location is in the page's caption: "FREE RIDE  ·  ALPINE")
                    if (!string.IsNullOrEmpty(c.vehicle)) t.Add(new OwnTile("VEHICLE", c.vehicle));
                    if (!string.IsNullOrEmpty(c.speed)) t.Add(new OwnTile("SPEED", c.speed));
                    if (c.distance >= 1f) t.Add(new OwnTile("DISTANCE", LongDist(c.distance, metric)));   // (the port's own tile: the path ridden since this Free Ride began)
                    if (m.discoveryTotal > 0) t.Add(new OwnTile("DISCOVERIES", m.discoveryFound + " / " + m.discoveryTotal, m.discoveryFound >= m.discoveryTotal ? "gold" : null));
                    spec.map = c.mapIcon >= 0; spec.mapIcon = c.mapIcon; spec.hasBike = c.hasBike; spec.bikeX = c.bikeX; spec.bikeZ = c.bikeZ; spec.heading = c.heading;
                    break;
                }
                case HudMode.Challenge:
                {
                    int kind = m.challengeKind;
                    bool timeKind = kind == 6 || kind == 3, heightKind = kind == 1;
                    string label = kind == 1 ? "HEIGHT" : kind == 6 ? "TIME" : kind == 8 ? "HITS" : "SCORE";
                    if (m.challengeValueValid)
                    {
                        float v = m.challengeValue;
                        string text = timeKind ? Time(v) : heightKind ? Dist(v, metric) : Mathf.RoundToInt(v).ToString(CultureInfo.InvariantCulture);
                        t.Add(new OwnTile(label, text, m.challengeMedal >= 3 ? "gold" : null));
                    }
                    if (m.timerShown) t.Add(new OwnTile(m.timerCountsUp ? "TIME" : "TIME LEFT", Time(m.timerValue)));
                    string[] medal = { "BRONZE", "SILVER", "GOLD" };
                    for (int i = 0; i < 3; i++)
                    {
                        float crit = m.challengeCriteria[i];
                        if (crit <= 0f) continue;
                        bool hit = m.challengeValueValid && (timeKind ? m.challengeValue > 0f && m.challengeValue <= crit : m.challengeValue >= crit);
                        t.Add(new OwnTile(medal[i], timeKind ? Time(crit) : heightKind ? Dist(crit, metric) : Mathf.RoundToInt(crit).ToString(CultureInfo.InvariantCulture), hit ? "good" : null));
                    }
                    string a = (c.activation ?? "").ToUpperInvariant();
                    spec.challengeTitle = a.Length > 0 ? S("STR_" + a + "_TITLE", spec.modeName) : spec.modeName;
                    spec.challengeText = a.Length > 0 ? S("STR_" + a + "_DESCRIPTION", "") : "";
                    break;
                }
                default:
                    if (race.race_time > 0) t.Add(new OwnTile("TIME", Time(race.race_time)));
                    break;
            }
            if (t.Count > MaxTiles) t.RemoveRange(MaxTiles, t.Count - MaxTiles);   // two rows: the rows below the tiles stay on screen
            return spec;
        }

        /// The most tiles the pause page shows (two rows of tiles over the rows of the menu).
        public const int MaxTiles = 4;

        // ------------------------------------------------------------------------------------------------ the results page

        /// The results page of a settled snapshot.
        public static OwnResultsSpec Results(OwnRaceResultsSnapshot s)
        {
            var spec = new OwnResultsSpec { mode = s.Mode };
            string where = !string.IsNullOrEmpty(s.EventLabel) ? s.EventLabel : ReflexHudModel.ModeName(s.Mode, -1);
            spec.caption = where;
            OwnRaceResultsSnapshot.Row mine = null;
            foreach (var r in s.Rows) if (r.IsLocal) { mine = r; break; }
            switch (s.Mode)
            {
                case HudMode.Freestyle: FreestyleResults(spec, s, mine); break;
                case HudMode.Waypoint: WaypointResults(spec, s, mine); break;
                case HudMode.SectionRace: SectionResults(spec, s, mine); break;
                default: RaceResults(spec, s, mine); break;
            }
            if (RxChampWeekend.StageInSession && RxChampWeekend.SessionStage >= 0)
            {   // RESTORED CONTENT (championship weekend): the stage's name over its results ("MOTO 1 RESULTS")
                string stage = RxChampWeekend.StageLabel(RxChampWeekend.SessionStage);
                spec.title = stage + " RESULTS"; spec.caption = "CHAMPIONSHIP WEEKEND  ·  " + where;
            }
            return spec;
        }

        // POS: the rider's number on the original's page (Index + 1, the standings order), unique; the header's place is the local row's
        static void Own(OwnResultsSpec spec, OwnRaceResultsSnapshot.Row r, string[] cells)
            => spec.rows.Add(new OwnResultsRow { slot = r.Slot, rank = r.HasValidRank ? r.Position : 0, name = r.Name, you = r.IsLocal, cells = cells });

        static void RaceResults(OwnResultsSpec spec, OwnRaceResultsSnapshot s, OwnRaceResultsSnapshot.Row mine)
        {
            bool elim = s.Mode == HudMode.Eliminator;
            spec.title = "RACE RESULTS";
            if (s.Mode == HudMode.Eliminator) spec.title = "ELIMINATOR RESULTS"; else if (s.Mode == HudMode.Overtake) spec.title = "OVERTAKE RESULTS"; else if (s.Mode == HudMode.Outrun) spec.title = "OUTRUN RESULTS";
            if (mine != null)
            {
                if (mine.HasValidRank) spec.place = mine.Position;
                spec.tiles.Add(new OwnTile("RACE TIME", mine.Time, "good"));
                spec.tiles.Add(new OwnTile("BEST LAP", mine.BestLap, "gold"));
                spec.tiles.Add(new OwnTile("RIDERS", s.Rows.Count.ToString(CultureInfo.InvariantCulture)));
            }
            // the original's columns: TOTAL TIME, PENALTY TIME, BEST LAP TIME (the Eliminator: ELIMINATED first)
            if (elim) spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_ELIMINATED", "ELIMINATED"), "ou-board-status"));
            else spec.columns.Add(new OwnBoardColumn("STATUS", "ou-board-status"));
            spec.columns.Add(new OwnBoardColumn("TIME", "ou-board-time"));
            spec.columns.Add(new OwnBoardColumn("PENALTY", "ou-board-points"));   // (always: the original's table has the column, empty without a penalty)
            spec.columns.Add(new OwnBoardColumn("BEST LAP", "ou-board-best"));
            foreach (var r in s.Rows)
            {
                string status = elim ? (r.TryPageCell("ELIMINATED", out var placeText, out var placeSort) ? EliminatedPlace(placeSort, placeText)
                    : r.Place > 0 ? r.Place + OuWidgets.Ordinal(r.Place) : "—") : r.Status;
                Own(spec, r, new[] { status, r.Time, r.Penalty, r.BestLap });
            }
        }

        static void WaypointResults(OwnResultsSpec spec, OwnRaceResultsSnapshot s, OwnRaceResultsSnapshot.Row mine)
        {
            spec.title = "WAYPOINT RESULTS";
            if (mine != null)
            {
                if (mine.HasValidRank && s.Rows.Count > 1) spec.place = mine.Position;
                spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_TOTALTIME", "TOTAL TIME"), mine.Time, "good"));
                if (mine.HasModeRow && mine.ModeRow.bestLap > 0 && mine.ModeRow.bestLap < 3.0e38f) spec.tiles.Add(new OwnTile("BEST LAP", OwnRaceResultsSnapshot.FormatTime(mine.ModeRow.bestLap), "gold"));
                if (s.Rows.Count > 1) spec.tiles.Add(new OwnTile("RIDERS", s.Rows.Count.ToString(CultureInfo.InvariantCulture)));
            }
            spec.columns.Add(new OwnBoardColumn("STATUS", "ou-board-status"));
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_TOTALTIME", "TOTAL TIME"), "ou-board-time"));
            foreach (var r in s.Rows) Own(spec, r, new[] { r.Finished ? "FINISHED" : "NOT FINISHED", r.Time });
        }

        static void FreestyleResults(OwnResultsSpec spec, OwnRaceResultsSnapshot s, OwnRaceResultsSnapshot.Row mine)
        {
            spec.title = "FREESTYLE RESULTS";
            spec.againLabel = "RIDE AGAIN"; spec.againTitle = "RIDE AGAIN?"; spec.againLead = "Start a new run with the same event settings?";
            if (mine != null)
            {
                if (mine.HasValidRank && s.Rows.Count > 1) spec.place = mine.Position;
                if (mine.HasModeRow)
                {
                    spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_TOTALSCORE", "TOTAL SCORE"), Cell(mine, "TOTALSCORE", Num(mine.ModeRow.score)), "gold"));
                    if (mine.ModeRow.hasBestTrick != 0)
                        spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTTRICKSCORE", "BEST TRICK SCORE"), Cell(mine, "BESTTRICKSCORE", Num(mine.ModeRow.bestTrickScore)), "good"));
                }
            }
            if (s.HasRaceHud)   // MXUIFMXFinalScoreCard: the five ratings the judges gave at "WindDown Entered"
            {
                string[] names = { "OVERALL", "DIFFICULTY", "EXECUTION", "FLOW", "VARIETY" };
                for (int i = 0; i < 5; i++) spec.judges.Add(new OwnTile(names[i], Num(s.RaceHud.FinalScore(i)), i == 0 ? "gold" : null));
            }
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_TOTALSCORE", "TOTAL SCORE"), "ou-board-time"));
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTTRICKSCORE", "BEST TRICK SCORE"), "ou-board-best"));
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTTRICK", "BEST TRICK"), "ou-board-status"));
            foreach (var r in s.Rows)
            {
                string[] cells = r.HasModeRow
                    ? new[] { Cell(r, "TOTALSCORE", Num(r.ModeRow.score)), Cell(r, "BESTTRICKSCORE", r.ModeRow.hasBestTrick != 0 ? Num(r.ModeRow.bestTrickScore) : ""),
                              r.ModeRow.hasBestTrick != 0 ? TrickName(r.ModeRow.BestTrick) : "" }   // (the table's BEST TRICK is the same key, record + 0x40; its export is cut at 15 characters)
                    : new[] { "—", "", "" };
                Own(spec, r, cells);
            }
        }

        // the best section's time: the game table's sort value (005133f0's time) once the page started, in the board's time format
        static string SectionTime(OwnRaceResultsSnapshot.Row r)
            => OwnRaceResultsSnapshot.FormatTime(r.TryPageCell("BESTSECTIONTIME", out _, out var t) ? t : r.ModeRow.bestSectionTime);

        static void SectionResults(OwnResultsSpec spec, OwnRaceResultsSnapshot s, OwnRaceResultsSnapshot.Row mine)
        {
            spec.title = "SECTION RACE RESULTS";
            if (mine != null)
            {
                if (mine.HasValidRank) spec.place = mine.Position;
                if (mine.HasModeRow)
                {
                    spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_SECTIONS", "SECTIONS"), Cell(mine, "SECTIONS", mine.ModeRow.sectionsWon.ToString(CultureInfo.InvariantCulture)) + (s.SectionCount > 0 ? " / " + s.SectionCount : ""), mine.ModeRow.sectionsWon > 0 ? "gold" : null));
                    if (mine.ModeRow.bestSection >= 0)
                    {
                        spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTSECTIONTIME", "BEST SECTION TIME"), SectionTime(mine), "good"));
                        spec.tiles.Add(new OwnTile(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTSECTION", "BEST SECTION"), Cell(mine, "BESTSECTION", (mine.ModeRow.bestSection + 1).ToString(CultureInfo.InvariantCulture))));
                    }
                }
                spec.tiles.Add(new OwnTile("RIDERS", s.Rows.Count.ToString(CultureInfo.InvariantCulture)));
            }
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_SECTIONS", "SECTIONS"), "ou-board-points"));
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTSECTION", "BEST SECTION"), "ou-board-points"));
            spec.columns.Add(new OwnBoardColumn(Caps("STR_ACTIVITYRESULTSCOLUMN_BESTSECTIONTIME", "BEST SECTION TIME"), "ou-board-best"));
            foreach (var r in s.Rows)
            {
                string[] cells = r.HasModeRow
                    ? new[] { Cell(r, "SECTIONS", r.ModeRow.sectionsWon.ToString(CultureInfo.InvariantCulture)), Cell(r, "BESTSECTION", r.ModeRow.bestSection >= 0 ? (r.ModeRow.bestSection + 1).ToString(CultureInfo.InvariantCulture) : ""),
                              r.ModeRow.bestSection >= 0 ? SectionTime(r) : "—" }
                    : new[] { "", "", "—" };
                Own(spec, r, cells);
            }
        }
    }
}
