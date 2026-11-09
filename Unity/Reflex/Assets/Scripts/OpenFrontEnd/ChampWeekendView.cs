// RESTORED CONTENT (championship weekend): what the weekend card (OwnChampWeekendCard) and the weekend setup page show, as plain data: the
// stages (their format, raced / decided / next), the riders in the standings' order (points or judged score, each stage's place and gate),
// the Supercross qualification status, the local rider's standing and gate for the next stage. FromWeekend reads it from the weekend
// (RxChampWeekend, the core's struct); the showcase builds its own (OwnChampWeekendCard.Preview).
using System;
using System.Collections.Generic;
using Reflex.Port.RxCore;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class ChampWeekendView
    {
        public sealed class Stage
        {
            public string label, format;   // "MOTO 1", "15:00 + 1 LAP" / "3 LAPS" / "" (the event's own rounds / time)
            public bool raced, decided, scored;
            public int field, grid, gridFrom, minutes, laps;
        }
        public sealed class Rider
        {
            public string name; public bool local;
            public int points; public float total;          // SeriesPoints / Freestyle's judged scores summed
            public int[] place = new int[RxChampWeekend.MaxStages], gate = new int[RxChampWeekend.MaxStages];
            public float[] bestLap = new float[RxChampWeekend.MaxStages], score = new float[RxChampWeekend.MaxStages];
            /// Supercross: where the qualification puts the rider ("MAIN", "LCQ", "OUT"); null before the heat / elsewhere.
            public string status;
        }

        public string eventLabel = "", locationName = "", disciplineName = "";
        public int discipline = -1;
        public bool judged, finished, eliminated, timeable;
        /// The stage the local rider races next (-1: the weekend is over), the stage recorded last (-1: none yet).
        public int next = -1, last = -1;
        public readonly List<Stage> stages = new List<Stage>();
        /// The standings, best first.
        public readonly List<Rider> riders = new List<Rider>();
        /// The local rider's place in the standings (1-based, 0 none) and gate in the next stage (1-based; 0: the event's own order / unknown).
        public int localRank, nextGate, nextField;
        /// Supercross, once the heat is placed: its field and split (RxChampWeekend.Split: heat 1 .. heatToMain to the main, .. heatLcqLast to
        /// the LCQ, LCQ 1 .. lcqToMain to the main); 0 before the heat / elsewhere.
        public int heatField, heatToMain, heatLcqLast, lcqToMain;
        public Rider Local => riders.Find(r => r.local);
        public Stage NextStage => next >= 0 && next < stages.Count ? stages[next] : null;
        public Stage LastStage => last >= 0 && last < stages.Count ? stages[last] : null;

        public static string DisciplineName(int d) => d == RxChampWeekend.National ? "NATIONAL" : d == RxChampWeekend.Supercross ? "SUPERCROSS" : d == RxChampWeekend.Waypoint ? "WAYPOINT RACE" : d == RxChampWeekend.Freestyle ? "FREESTYLE" : "";
        /// A stage's column heading on the board (short: four stages and the points share the row).
        public static string Short(string label) => label == "PRACTICE" ? "PRAC" : label ?? "";

        /// The offline weekend now (null when none runs).
        public static ChampWeekendView FromWeekend()
        {
            if (!RxChampWeekend.Active) return null;
            var v = new ChampWeekendView();
            var ev = OpenContent.Find(RxChampWeekend.EventKey);
            v.eventLabel = ev != null ? ev.Label : ""; v.locationName = ev != null ? ev.locationName : "";
            v.discipline = RxChampWeekend.Discipline; v.disciplineName = DisciplineName(v.discipline);
            v.judged = RxChampWeekend.Judged; v.finished = RxChampWeekend.Finished; v.eliminated = RxChampWeekend.Eliminated; v.timeable = RxChampWeekend.Timeable;
            int count = RxChampWeekend.StageCount;
            for (int s = 0; s < count; s++)
            {
                var st = new Stage
                {
                    label = RxChampWeekend.StageLabel(s), raced = RxChampWeekend.StageRaced(s), decided = RxChampWeekend.StageDecided(s), scored = RxChampWeekend.StageScored(s),
                    field = RxChampWeekend.StageField(s), grid = RxChampWeekend.StageGrid(s), gridFrom = RxChampWeekend.StageGridFrom(s),
                    minutes = RxChampWeekend.StageMinutes(s), laps = RxChampWeekend.StageLaps(s),
                };
                st.format = v.timeable ? RxChampWeekend.StageFormat(s) : "";
                v.stages.Add(st);
                if (st.raced || st.decided) v.last = s;
            }
            v.next = v.finished ? -1 : Math.Min(RxChampWeekend.Stage, count - 1);
            int n = RxChampWeekend.RiderCount;
            for (int i = 0; i < n; i++)
            {
                int r = RxChampWeekend.Overall(i);
                if (r < 0 || r >= n) continue;
                var x = new Rider { name = RxChampWeekend.RiderName(r), local = RxChampWeekend.RiderLocal(r), points = RxChampWeekend.RiderPoints(r), total = RxChampWeekend.RiderTotalScore(r) };
                // the local rider by the GARAGE's name (the HUD's), not the race's plate ("RIDER" for a profile without one)
                if (x.local) try { if (Reflex.Port.Content.ReflexProfile.HasPersona) x.name = Reflex.Port.Content.ReflexProfile.Persona; } catch (Exception) { }
                for (int s = 0; s < count; s++)
                {
                    x.place[s] = RxChampWeekend.RiderPlace(r, s); x.gate[s] = RxChampWeekend.RiderGate(r, s);
                    x.bestLap[s] = RxChampWeekend.RiderBestLap(r, s); x.score[s] = RxChampWeekend.RiderScore(r, s);
                }
                v.riders.Add(x);
            }
            v.Derive();
            return v;
        }

        /// The derived parts (the qualification status, the local rider's rank and next gate) from the stages and the riders' places.
        public void Derive()
        {
            int heat = stages.FindIndex(s => s.label == "HEAT"), lcq = stages.FindIndex(s => s.label == "LCQ");
            bool supercross = discipline == RxChampWeekend.Supercross && heat >= 0;
            // FIELD SIZE: the split follows the heat's field (the riders it placed), as the core's champ_heat_split
            heatField = heat >= 0 ? riders.FindAll(r => r.place[heat] >= 1).Count : 0;
            RxChampWeekend.Split(heatField, out heatToMain, out heatLcqLast, out lcqToMain);
            foreach (var r in riders)
            {
                r.status = null;
                if (!supercross || !(stages[heat].raced || stages[heat].decided)) continue;
                int hp = r.place[heat];
                if (hp >= 1 && hp <= heatToMain) r.status = "MAIN";
                else if (hp > heatToMain && hp <= heatLcqLast)
                {
                    if (lcq >= 0 && (stages[lcq].raced || stages[lcq].decided)) r.status = r.place[lcq] >= 1 && r.place[lcq] <= lcqToMain ? "MAIN" : "OUT";
                    else r.status = "LCQ";
                }
                else if (hp > 0) r.status = "OUT";
            }
            localRank = riders.FindIndex(r => r.local) + 1;
            nextGate = 0; nextField = 0;
            var st = NextStage;
            if (st == null) return;
            // the next stage's grid as the core sets it (champ_weekend_session.inc champ_grid_key: the field by the key, a stable order)
            var field = new List<(Rider r, long key, int i)>();
            for (int i = 0; i < riders.Count; i++)
            {
                var r = riders[i];
                if (!InField(st, r, heat, lcq)) continue;
                field.Add((r, GridKey(st, r, heat, lcq), i));
            }
            nextField = field.Count;
            if (st.grid == RxChampWeekend.GridEvent) return;
            field.Sort((a, b) => a.key != b.key ? a.key.CompareTo(b.key) : a.i.CompareTo(b.i));
            nextGate = field.FindIndex(f => f.r.local) + 1;
        }

        bool InField(Stage st, Rider r, int heat, int lcq)
        {
            if (st.field == RxChampWeekend.FieldLcq) return heat >= 0 && r.place[heat] > heatToMain && r.place[heat] <= heatLcqLast;
            if (st.field == RxChampWeekend.FieldMain)
                return heat >= 0 && r.place[heat] >= 1 && r.place[heat] <= heatToMain || lcq >= 0 && r.place[lcq] >= 1 && r.place[lcq] <= lcqToMain;
            return true;
        }

        long GridKey(Stage st, Rider r, int heat, int lcq)
        {
            int from = st.gridFrom; const long unplaced = 1000;
            switch (st.grid)
            {
                case RxChampWeekend.GridBestLap:
                {
                    float lap = from >= 0 ? r.bestLap[from] : 0f;
                    if (lap > 0f) return (long)(lap * 1000f);
                    return 100000000 + (from >= 0 && r.place[from] != 0 ? r.place[from] : unplaced);
                }
                case RxChampWeekend.GridPlace: return from >= 0 && r.place[from] != 0 ? r.place[from] : unplaced;
                case RxChampWeekend.GridMain:
                    if (heat >= 0 && r.place[heat] >= 1 && r.place[heat] <= heatToMain) return r.place[heat];
                    if (lcq >= 0 && r.place[lcq] >= 1) return 100 + r.place[lcq];
                    return unplaced;
            }
            return 0;
        }

        /// What the local rider's weekend comes to now, in a few words ("YOU'RE IN THE MAIN EVENT", "TO THE LCQ: THE TOP 2 MAKE THE MAIN": the
        /// split's lcqToMain).
        public string LocalStatusLine(out string tone)
        {
            tone = null;
            var me = Local;
            if (eliminated) { tone = "bad"; return "YOU DID NOT MAKE THE MAIN EVENT"; }
            if (me == null) return null;
            if (finished) { tone = localRank == 1 ? "gold" : null; return localRank == 1 ? "WEEKEND WINNER" : "WEEKEND COMPLETE"; }
            if (me.status == "MAIN") { tone = "good"; return "YOU'RE IN THE MAIN EVENT"; }
            if (me.status == "LCQ") { tone = "warn"; return lcqToMain == 1 ? "TO THE LCQ  ·  ITS WINNER MAKES THE MAIN" : "TO THE LCQ  ·  THE TOP " + lcqToMain + " MAKE THE MAIN"; }
            return null;
        }
    }
}
