using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using Reflex.Port.Hud;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;

namespace Reflex.Port.OpenFrontEnd
{
    // Read-only presentation of one settled native result. The caller establishes launch origin and generation.
    public sealed class OwnRaceResultsSnapshot
    {
        public sealed class Row
        {
            readonly ReflexDriverResult native;
            readonly int riderCount;
            internal Row(ReflexDriverResult value, string name, int riderCount, int position) { native = value; Name = name; this.riderCount = riderCount; Position = position; }
            public ReflexDriverResult Native => native; // value copy, never the worker's mutable array
            public int Slot => native.slot;
            public bool IsLocal => native.local != 0;
            /// The driver's place +0x78 (the HUD's position). Not the results page's number: after InProgress the race's comparator
            /// (MXActivityRace::vf43 00565570) returns -1 or 0, never 1, so 00559eb0 keeps the order and repeats places.
            public int Place => native.place;
            /// The row's number on the results page (1-based): the rider's index in the activity's standings (+0x2d0, the order
            /// rx_session_standings writes), once the page started the game table's own Index + 1 (00613dd0; PostEventResults.lua
            /// sorts the rows by Index and shows Index + 1). Unique, 1 to the rider count.
            public int Position { get; private set; }
            internal void SetPosition(int value) => Position = value;
            public int LapsCompleted => native.laps_completed;
            public bool Finished => native.finished != 0;
            public string Name { get; }
            public bool HasValidRank => Position > 0 && Position <= riderCount;
            public string Rank => HasValidRank ? Position.ToString(CultureInfo.InvariantCulture) : "—";
            public string Status => Finished ? "FINISHED" : "NOT FINISHED";
            // Once the game's results page started (RxResultsPage, w-resultsrng) its TOTAL / BEST LAP: the original's page shows a
            // PROJECTED total for a rider still racing (00613880) and a projected best lap for one without a lap; before that the
            // standings' clocks.
            public string Time => HasPage && !float.IsNaN(pageTotal) ? FormatTime(pageTotal) : Finished ? FormatTime(native.race_time) : "—";
            public string BestLap => HasPage && !float.IsNaN(pageBest) ? FormatTime(pageBest) : FormatTime(native.best_lap);
            public bool HasPage { get; private set; }
            float pageTotal = float.NaN, pageBest = float.NaN;
            internal void SetPage(float total, float best) { pageTotal = total; pageBest = best; HasPage = true; }
            public string LastLap => FormatTime(native.last_lap);
            /// The game table's PENALTY cell ("+MM:SS.cc", empty without one; the race types, once the page started).
            public string Penalty { get; private set; } = "";
            internal void SetPenalty(string text) { Penalty = text ?? ""; }
            /// Every cell of this rider's row in the game's results table once the page started (w-wpfill: the Waypoint, Freestyle and
            /// Section Race fillers and the Eliminator's place column run in the core too), by the column's title name (the key's last
            /// part: TOTALTIME, ELIMINATED, TOTALSCORE, SECTIONS ...): the game's own text (at most 15 characters) and the sort value.
            readonly Dictionary<string, (string text, float sort)> pageCells = new Dictionary<string, (string, float)>(StringComparer.Ordinal);
            internal void SetPageCell(string column, string text, float sort) => pageCells[column] = (text ?? "", sort);
            public bool TryPageCell(string column, out string text, out float sort)
            {
                if (pageCells.TryGetValue(column, out var c)) { text = c.text; sort = c.sort; return true; }
                text = ""; sort = 0f; return false;
            }
            /// The Waypoint / Freestyle / Section Race values of this rider (rx_session_mode_results), when the core exports them.
            public bool HasModeRow { get; private set; }
            public RxModeResultRow ModeRow { get; private set; }
            internal void SetModeRow(RxModeResultRow row) { ModeRow = row; HasModeRow = true; }
        }

        /// What the snapshot knows beyond the standings: the mode (the HUD model's), the event's name, the mode's own values.
        public sealed class Context
        {
            public HudMode Mode = HudMode.Race;
            public string EventLabel = "";
            public bool HasModeResults; public RxModeResultsState ModeResults;
            public bool HasRaceHud; public RxRaceHudState RaceHud;
        }

        public long Generation { get; }
        public string EventIdentity { get; }
        public ReflexRaceState Race { get; }
        public RxEventUiState EventUi { get; }
        /// The riders in the results page's order (Position 1 first).
        public IReadOnlyList<Row> Rows { get; }
        readonly Row[] rows;
        /// Counts the changes of the rows' order (a page that numbers the riders otherwise than the standings did): a shown board rebuilds.
        public int OrderRevision { get; private set; }
        /// The mode shown (Race for the lap races, Waypoint, SectionRace, Freestyle), the event's label and the mode's own values.
        public HudMode Mode { get; }
        public string EventLabel { get; }
        public bool HasRaceHud { get; }
        public RxRaceHudState RaceHud { get; }
        public int SectionCount { get; }

        OwnRaceResultsSnapshot(long generation, string identity, ReflexRaceState race, RxEventUiState ui, Row[] rows, Context context)
        {
            Generation = generation; EventIdentity = identity; Race = race; EventUi = ui;
            this.rows = rows; Rows = new ReadOnlyCollection<Row>(rows);
            Mode = context.Mode; EventLabel = context.EventLabel ?? ""; HasRaceHud = context.HasRaceHud; RaceHud = context.RaceHud;
            SectionCount = context.HasModeResults ? context.ModeResults.sectionCount : -1;
        }

        // offlineExhibitionOpen is explicit caller policy, not inferred from activity enum or rider count. The modes with a results page of
        // their own: the lap races (National, Supercross, Terracross, Powercross), Waypoint, Section Race and Freestyle (OwnModes.ResultsType).
        public static bool Supports(bool offlineExhibitionOpen, ReflexRaceState race, RxEventUiState ui)
        {
            const RxEventUiKnown required = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState
                | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState;
            return offlineExhibitionOpen && race.valid != 0 && race.state == 4
                && (race.num_laps > 0 || !OwnModes.LapType(ui.activity_type)) && race.riders > 0 && race.riders <= RxRiderCap.Max
                && (ui.known & required) == required && OwnModes.ResultsType(ui.activity_type)
                && ui.race_state == 4 && ui.post_state == 2 && ui.rules_hold == 0;
        }

        public static bool TryCreate(long generation, string eventIdentity, bool offlineExhibitionOpen,
            ReflexRaceState race, RxEventUiState eventUi, ReflexDriverResult[] drivers, int driverCount,
            IReadOnlyDictionary<int, string> names, out OwnRaceResultsSnapshot snapshot, out string reason)
            => TryCreateMode(generation, eventIdentity, offlineExhibitionOpen, race, eventUi, drivers, driverCount, names, new Context(), out snapshot, out reason);

        public static bool TryCreateMode(long generation, string eventIdentity, bool offlineExhibitionOpen,
            ReflexRaceState race, RxEventUiState eventUi, ReflexDriverResult[] drivers, int driverCount,
            IReadOnlyDictionary<int, string> names, Context context, out OwnRaceResultsSnapshot snapshot, out string reason)
        {
            snapshot = null; reason = null;
            context ??= new Context();
            if (generation <= 0 || string.IsNullOrWhiteSpace(eventIdentity)) { reason = "Missing world identity."; return false; }
            if (!Supports(offlineExhibitionOpen, race, eventUi)) { reason = "Results route is not supported."; return false; }
            if (drivers == null || driverCount != race.riders || driverCount > drivers.Length)
            { reason = "Standings are incomplete."; return false; }
            var copied = new Row[driverCount];
            var slots = new HashSet<int>();
            int localCount = 0;
            for (int i = 0; i < driverCount; i++)
            {
                var d = drivers[i];
                if (d.slot < 0 || d.slot >= RxRiderCap.Max || !slots.Add(d.slot)) { reason = "Standings have invalid rider slots."; return false; }
                if (d.local != 0)
                {
                    localCount++;
                    // a lap race ends with the local rider's last lap; the timed modes (Freestyle, Section Race) end by their clock
                    // (RESTORED CONTENT, a timed championship stage: a lapped rider is flagged at its next crossing, with fewer laps)
                    if (OwnModes.LapType(eventUi.activity_type) && (d.finished == 0 || (d.laps_completed < race.num_laps && !Reflex.Port.RxCore.RxChampWeekend.TimedStageInSession(out _, out _))))
                    { reason = "The local rider has not finished."; return false; }
                }
                string name = null;
                names?.TryGetValue(d.slot, out name);
                if (string.IsNullOrWhiteSpace(name)) name = d.local != 0 ? "You" : "Rider " + (d.slot + 1).ToString(CultureInfo.InvariantCulture);
                // the core writes the standings in the activity's order (+0x2d0): the original's page numbers its rows by it (00613dd0's
                // Index), not by the places, which repeat once the race left InProgress
                copied[i] = new Row(d, name, race.riders, i + 1);
            }
            if (localCount != 1) { reason = "Standings require exactly one local rider."; return false; }
            if (context.HasModeResults)
                foreach (var row in copied) if (context.ModeResults.TryBySlot(row.Slot, out var mr)) row.SetModeRow(mr);
            snapshot = new OwnRaceResultsSnapshot(generation, eventIdentity, race, eventUi, copied, context);
            return true;
        }

        // The game's results page table (the core runs the original's page start 14 frames after "Show Activity Results"): its TOTAL and
        // BEST LAP per rider (the sort values are -t) and every cell by its column (the mode pages read them: OwnModes). True when it
        // changed what the rows show.
        uint appliedPage;
        public bool ApplyResultsPage(Reflex.Port.RxCore.RxResultsPage page)
        {
            if (page == null || page.populated == appliedPage) return false;
            int total = page.Column("TOTALTIME"), best = page.Column("BESTLAPTIME"), penalty = page.Column("PENALTYTIME");
            // the page's numbers (Index + 1) when it holds every rider once: the rows take its order (PostEventResults:Start sorts by Index)
            var index = new int[rows.Length]; var seen = new bool[rows.Length]; bool numbered = true;
            for (int i = 0; i < rows.Length && numbered; i++)
            {
                var pr = page.BySlot(rows[i].Slot);
                numbered = pr != null && pr.index >= 0 && pr.index < rows.Length && !seen[pr.index];
                if (numbered) { seen[pr.index] = true; index[i] = pr.index + 1; }
            }
            if (numbered)
            {
                bool moved = false;
                for (int i = 0; i < rows.Length; i++) { if (rows[i].Position != index[i]) moved = true; rows[i].SetPosition(index[i]); }
                if (moved) { Array.Sort(rows, (a, b) => a.Position.CompareTo(b.Position)); OrderRevision++; }
            }
            foreach (var row in Rows)
            {
                var pr = page.BySlot(row.Slot);
                if (pr == null) continue;
                row.SetPage(total >= 0 ? -pr.sort[total] : float.NaN, best >= 0 ? -pr.sort[best] : float.NaN);
                string text = penalty >= 0 ? pr.cells[penalty] : "";
                row.SetPenalty(text);
                for (int k = 0; k < page.columns && k < 3; k++)
                {
                    string title = page.titles[k] ?? "";
                    int cut = title.LastIndexOf('_');
                    if (cut >= 0 && cut + 1 < title.Length) row.SetPageCell(title.Substring(cut + 1).ToUpperInvariant(), pr.cells[k], pr.sort[k]);
                }
            }
            appliedPage = page.populated;
            return true;
        }

        // Positive native clocks only. FLT_MAX is the original one-lap best-time sentinel.
        public static string FormatTime(float seconds)
        {
            if (float.IsNaN(seconds) || float.IsInfinity(seconds) || seconds <= 0 || seconds == float.MaxValue) return "—";
            double rounded = Math.Round((double)seconds * 1000d, MidpointRounding.AwayFromZero);
            if (rounded >= long.MaxValue) return "—";
            long ms = (long)rounded;
            return (ms / 60000).ToString("00", CultureInfo.InvariantCulture) + ":"
                + ((ms / 1000) % 60).ToString("00", CultureInfo.InvariantCulture) + "."
                + (ms % 1000).ToString("000", CultureInfo.InvariantCulture);
        }
    }
}
