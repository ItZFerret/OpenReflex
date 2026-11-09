using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OwnRaceResultsPolicy
    {
        // This is the resolved core launch spec, never an event display label or a directory guess. Every offline mode with a results page of
        // its own: the lap races (the event's duration is its laps), Waypoint (laps), Freestyle and Section Race (their duration is minutes).
        public static bool IsExhibitionSpec(RxEventSpec spec)
        {
            if (spec.PlayerMode != "SinglePlayer" || !string.IsNullOrEmpty(spec.EventActivation) || spec.duration <= 0 || spec.riders <= 0 || spec.riders > RxRiderCap.Max) return false;
            switch (spec.EventType)
            {
                case "National": case "Supercross": case "Terracross": case "Powercross": case "WaypointRace": return spec.DurationType == "Laps";
                case "Freestyle": case "SectionRace": return spec.DurationType == "Minutes";
            }
            return false;
        }

        public static bool OwnsPost(bool offlineExhibitionOpen, ReflexRaceState race, RxEventUiState ui)
        {
            const RxEventUiKnown required = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState
                | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState;
            return offlineExhibitionOpen && race.valid != 0 && race.state == 4 && (race.num_laps > 0 || !OwnModes.LapType(ui.activity_type))
                && race.riders > 0 && race.riders <= RxRiderCap.Max && (ui.known & required) == required
                && OwnModes.ResultsType(ui.activity_type) && ui.race_state == 4
                && ui.rules_hold == 0 && ui.post_state >= 0 && ui.post_state <= 2;
        }

        /// The timed modes (Freestyle, Section Race) and the Waypoint race list riders that have no vehicle in the world (the AI of an FMX arena is
        /// scored, not ridden): the standings need exactly one local rider, who is the roster's, and valid unique slots.
        public static bool MatchesLocalRoster(IReadOnlyDictionary<int, bool> roster, ReflexDriverResult[] drivers, int count, int expected)
        {
            if (roster == null || drivers == null || count != expected || count < 1 || count > RxRiderCap.Max || count > drivers.Length) return false;
            var seen = new HashSet<int>();
            int localCount = 0;
            for (int i = 0; i < count; i++)
            {
                var row = drivers[i];
                if (row.slot < 0 || row.slot >= RxRiderCap.Max || !seen.Add(row.slot)) return false;
                if (row.local != 0) { localCount++; if (!roster.TryGetValue(row.slot, out bool local) || !local) return false; }
            }
            return localCount == 1;
        }

        public static bool MatchesRoster(IReadOnlyDictionary<int, bool> roster, ReflexDriverResult[] drivers, int count, int expected)
        {
            if (roster == null || drivers == null || count != expected || count != roster.Count || count < 1 || count > RxRiderCap.Max || count > drivers.Length) return false;
            var seen = new HashSet<int>();
            int localCount = 0;
            for (int i = 0; i < count; i++)
            {
                var row = drivers[i];
                if (row.slot < 0 || row.slot >= RxRiderCap.Max || !seen.Add(row.slot) || !roster.TryGetValue(row.slot, out bool local) || local != (row.local != 0)) return false;
                if (local) localCount++;
            }
            return localCount == 1;
        }
    }
}
