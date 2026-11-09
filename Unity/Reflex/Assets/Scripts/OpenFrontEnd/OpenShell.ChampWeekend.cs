// RESTORED CONTENT (w-champweekend): CHAMPIONSHIP WEEKEND, set up in RACE's normal track select. Not part of the original PC game: the
// original's championship sub events (MXActivityRaceChamp* / MXActivityWaypointRaceChamp* / MXActivityFreestyleChamp*) run as a weekend
// with the game's SeriesPoints for the main events (core/include/rx_champ_weekend.h; the native state is RxChampWeekend):
//   National   PRACTICE > HEAT > MOTO 1 > MOTO 2 (each grid from the stage before; the motos score points)
//   Supercross PRACTICE > HEAT > LCQ > MAIN (12 riders: heat 1-5 to the main, 6-11 to the LCQ, its first two to the main; the main scores
//              points; another field: the split scales with it, RxChampWeekend.Split / the core's champ_split)
//   Waypoint   INTRO > PRACTICE > MAIN; Freestyle PRACTICE > MAIN 1 > MAIN 2 (the two mains' judged scores summed)
//   reached  RACE > a National / Supercross / Waypoint Race / Freestyle event > its setup page's EVENT tab: FORMAT SINGLE RACE / CHAMPIONSHIP
//            WEEKEND (remembered per event: OpenReflex.ChampWeekend.Format.<event key>); with CHAMPIONSHIP WEEKEND the setup page's START
//            reads SET UP WEEKEND and opens the weekend setup page below (the event's own setup, vehicle / class / rider / AI, is kept)
//   page     the setup page's skeleton (Setup.uxml): the location on the left with the stages in order and the setup's summary; on the right
//            STAGES (per stage CLOCK: OFF / 1:00 .. 60:00 and LAPS: the laps after the clock, or the race's laps without one; National /
//            Supercross only: Waypoint's rounds and Freestyle's time are the event's own) and FIELD (AI OPPONENTS; FIELD SIZE, the shared
//            OpenFieldSize row on the event setup's own `riders`: 2 .. 32 for National / Supercross, 12 for Waypoint / Freestyle; Supercross:
//            the split of that field); START
//            WEEKEND launches the first stage as any race starts (the load arena, the loading screen). From there the weekend stays in game
//            (ChampWeekendFlow / OwnChampWeekendCard) until its end, then back to the event's setup page.
//   names    ou-setup-format (the setup page's row), champweekend-page, champweekend-start, champweekend-clock-<stage>,
//            champweekend-laps-<stage>, champweekend-reset, champweekend-ai, champweekend-riders-slot (the FIELD SIZE row's place),
//            champweekend-riders (the row; its note champweekend-riders-note), champweekend-split (Supercross), champweekend-rules,
//            champweekend-tab-<tab>
// FIELD SIZE: the weekend's field is the event setup's own FIELD SIZE (OpenSetup.riders, saved with the event's setup): the event's setup
// page and the weekend page show and change the same value, and every stage launches with it (OpenSetup.LaunchRiders).
// TIMED RACES: the player's CLOCK / LAPS per discipline and stage are remembered (PlayerPrefs OpenReflex.ChampWeekend.<discipline>.<stage>.min
// / .laps) and applied when a weekend begins.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        const string ChampWeekendTitle = "CHAMPIONSHIP WEEKEND";
        static readonly string[] ChampTabs = { "STAGES", "FIELD" };

        // ------------------------------------------------------------------------------------------------ FORMAT on the setup page

        /// The event can be raced as a championship weekend (RACE, a National / Supercross / Waypoint Race / Freestyle event of the game).
        bool ChampWeekendOffered(OpenEvent ev) => ev != null && ev.ev != null && !_practice && !_freeRide && ev.mod == null
                                                   && RxChampWeekend.DisciplineOf(ev.activity) >= 0 && _setup != null && _setup.Rules(ev.ev);
        static string ChampFormatKey(string eventKey) => "OpenReflex.ChampWeekend.Format." + eventKey;
        /// The setup page's FORMAT is CHAMPIONSHIP WEEKEND for this event.
        bool ChampFormatOn(OpenEvent ev) => ChampWeekendOffered(ev) && PlayerPrefs.GetInt(ChampFormatKey(ev.key), 0) == 1;
        static void SetChampFormat(OpenEvent ev, bool on) { PlayerPrefs.SetInt(ChampFormatKey(ev.key), on ? 1 : 0); PlayerPrefs.Save(); }

        /// The EVENT tab's FORMAT row (and, with CHAMPIONSHIP WEEKEND, the stages in a line under it). True: the weekend's format is on.
        bool SetupFormatRow(OpenEvent ev, VisualElement page)
        {
            if (!ChampWeekendOffered(ev)) return false;
            bool on = ChampFormatOn(ev);
            OuTheme.Section(page, "FORMAT");
            OuUi.Spinner("FORMAT", () => ChampFormatOn(ev) ? "CHAMPIONSHIP WEEKEND" : "SINGLE RACE", k =>
            {
                SetChampFormat(ev, !ChampFormatOn(ev));
                SetupChanged(true);
                var label = _startButton?.Q<Label>();
                if (label != null) label.text = SetupStartLabel(ev);
            }, page).name = "ou-setup-format";
            if (on) OuUi.Text(string.Join("  ›  ", ChampStageLabels(RxChampWeekend.DisciplineOf(ev.activity))) + ".  Clocks, laps and the field on the next page.", "mxa-note", page);
            return on;
        }

        /// The setup page's START: START EVENT, or SET UP WEEKEND when the format is the weekend.
        string SetupStartLabel(OpenEvent ev) => _practice ? "START PRACTICE" : _freeRide ? "START RIDE" : ChampFormatOn(ev) ? "SET UP WEEKEND  ›" : "START EVENT";

        static IEnumerable<string> ChampStageLabels(int discipline)
        {
            switch (discipline)
            {
                case RxChampWeekend.National: return new[] { "PRACTICE", "HEAT", "MOTO 1", "MOTO 2" };
                case RxChampWeekend.Supercross: return new[] { "PRACTICE", "HEAT", "LCQ", "MAIN" };
                case RxChampWeekend.Waypoint: return new[] { "INTRO", "PRACTICE", "MAIN" };
                case RxChampWeekend.Freestyle: return new[] { "PRACTICE", "MAIN 1", "MAIN 2" };
            }
            return new string[0];
        }

        static string Ordinal(int n) => n + (n % 100 >= 11 && n % 100 <= 13 ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th");

        /// Supercross: the heat's split of a field of `riders` in words ("Heat 1st to 5th go straight to the main, 6th to 11th to the LCQ,
        /// whose top 2 join them." at 12: the split RxChampWeekend.Split gives the core's).
        static string ChampSplitText(int riders)
        {
            RxChampWeekend.Split(riders, out int toMain, out int lcqLast, out int lcqToMain);
            string main = toMain == 1 ? "The heat winner goes" : toMain >= riders ? "Every rider of the heat goes" : "Heat 1st to " + Ordinal(toMain) + " go";
            if (lcqLast <= toMain) return main + " straight to the main.";
            string lcq = lcqLast == toMain + 1 ? Ordinal(lcqLast) + " to the LCQ" : Ordinal(toMain + 1) + " to " + Ordinal(lcqLast) + " to the LCQ";
            return main + " straight to the main, " + lcq + ", whose " + (lcqToMain == 1 ? "winner joins" : "top " + lcqToMain + " join") + " them.";
        }

        /// The Supercross field's line on the FIELD tab: "A HEAT OF 24  ›  MAIN 14: HEAT 1-10 + LCQ TOP 4 (12 IN THE LCQ, 2 OUT)".
        static string ChampSplitLine(int riders)
        {
            RxChampWeekend.Split(riders, out int toMain, out int lcqLast, out int lcqToMain);
            string s = "A HEAT OF " + riders + "  ›  A MAIN OF " + (toMain + lcqToMain) + ": HEAT 1-" + toMain;
            if (lcqLast > toMain) s += " + LCQ TOP " + lcqToMain + "  (" + (lcqLast - toMain) + " IN THE LCQ";
            int home = riders - lcqLast;
            if (lcqLast > toMain) s += home > 0 ? ", " + home + " OUT)" : ")";
            else if (home > 0) s += "  (" + home + " OUT)";
            return s;
        }

        static string ChampRulesText(int discipline, int riders)
        {
            switch (discipline)
            {
                case RxChampWeekend.National: return "Practice sets the heat's gates by best lap, the heat sets moto 1's and moto 1 sets moto 2's. Both motos score points: the most points wins.";
                case RxChampWeekend.Supercross: return "Practice sets the heat's gates by best lap. " + ChampSplitText(riders) + " The main scores the points.";
                case RxChampWeekend.Waypoint: return "An intro round, a practice, then the main event, which scores the points. Each grid follows the stage before.";
                case RxChampWeekend.Freestyle: return "A practice, then two judged mains: the two scores together decide the weekend.";
            }
            return "";
        }

        // ------------------------------------------------------------------------------------------------ the weekend setup page

        sealed class ChampSetupPage { public VisualElement root, flow, summary, actions; public Label rules, split; public ScrollView rows; public readonly List<VisualElement> pages = new List<VisualElement>(), tabs = new List<VisualElement>(); public int tab; }
        ChampSetupPage _cw;

        /// The weekend setup page of the browser's event (the setup page's SET UP WEEKEND). A new weekend each time it opens (the remembered
        /// clocks and laps on it); BACK goes back to the event's setup page.
        public void ShowChampWeekendSetup()
        {
            var ev = _browseEvent;
            int discipline = ev == null ? -1 : RxChampWeekend.DisciplineOf(ev.activity);
            if (ev == null || discipline < 0) return;
            if (!(RxChampWeekend.Active && RxChampWeekend.EventKey == ev.key && RxChampWeekend.Stage == 0 && !RxChampWeekend.StageRaced(0)))
            {
                ChampCoreLoaded();
                if (!RxChampWeekend.Begin(ev.key, discipline)) { Debug.LogWarning("[ChampWeekend] " + RxChampWeekend.LastError); ShowSetupPage(); return; }
                LoadChampTiming();
            }
            if (_setup.Ai(ev.ev) <= 0) { _setup.ai = 2; OpenHistory.SaveSetup(_setup); }   // (a weekend is raced with a full gate: AI on)
            Clear(Screen.Setup, "setup");
            _layer.AddToClassList("mxa-dim");
            MusicTier(5);
            var right = OuTheme.Header(_layer, ChampWeekendTitle, ev.Label);
            OuUi.Button("SINGLE RACE", "ou-chip", () => { SetChampFormat(ev, false); LeaveChampSetup(); }, right).name = "champweekend-single";

            var p = _cw = new ChampSetupPage();
            p.root = OuTheme.Clone("Setup", _layer) ?? OuUi.Div("ou-setup", _layer);
            p.root.name = "champweekend-page";
            var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuChampWeekend");
            if (style && !p.root.styleSheets.Contains(style)) p.root.styleSheets.Add(style);
            var info = OuTheme.Part(p.root, "setup-info", "ou-setup-info");
            var hero = OuTheme.Part(info, "setup-hero", "ou-setup-hero");
            OuTheme.Part(hero, "setup-hero-shade", "ou-setup-hero-shade").style.backgroundImage = OuUi.ScrimBottom;
            var map = OuTheme.Part(hero, "setup-map", "ou-setup-map");
            var copy = OuTheme.Part(hero, "setup-hero-copy", "ou-setup-hero-copy");
            var badges = OuUi.Div("ou-setup-badges", copy);
            OuUi.Text(ChampWeekendView.DisciplineName(discipline) + "  ·  WEEKEND", "ou-badge", badges);
            OuUi.Text(ev.locationName, "ou-setup-title", copy);
            OuUi.Text(ev.Label + "  ·  " + ev.Where, "ou-setup-sub", copy);
            ShowPreview(ev, hero, map);
            var cardsScroll = p.root.Q<ScrollView>("setup-cards") ?? AddScroll(info, "ou-setup-cards");
            cardsScroll.verticalScrollerVisibility = ScrollerVisibility.Auto; cardsScroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            var cards = cardsScroll.contentContainer;
            OuTheme.Section(cards, "THE WEEKEND");
            p.flow = OuUi.Div("ou-cw-flow", cards);
            p.rules = OuUi.Text(ChampRulesText(discipline, _setup.Riders(ev.ev)), "mxa-note ou-setup-text", cards);
            p.rules.name = "champweekend-rules";
            if (RxChampWeekend.Timeable) OuUi.Text("CLOCK + LAPS: when the clock runs out, the leader's next lap starts the extra laps and everyone is flagged with the leader.", "mxa-note ou-setup-text ou-faint", cards);
            OuTheme.Section(cards, "YOUR SETUP");
            p.summary = OuUi.Div("ou-setup-summary", cards);

            var panel = OuTheme.Part(p.root, "setup-panel", "ou-panel ou-setup-panel");
            var tabs = OuTheme.Part(panel, "setup-tabs", "ou-gtabs ou-setup-tabs");
            tabs.Clear();
            string[] keys = OuUi.KeyTabs.Split('/');
            OuUi.Text(keys[0].Trim(), "mxa-key ou-gtabs-key", tabs);
            for (int i = 0; i < ChampTabs.Length; i++)
            {
                int t = i;
                var b = OuUi.Button(ChampTabs[i], "ou-gtab", () => ChampTab(t, true), tabs);
                b.name = "champweekend-tab-" + ChampTabs[i].ToLowerInvariant();
                OuUi.Div("ou-gtab-bar", b);
                p.tabs.Add(b);
            }
            OuUi.Text(keys.Length > 1 ? keys[1].Trim() : "", "mxa-key ou-gtabs-key", tabs);
            p.rows = p.root.Q<ScrollView>("setup-rows") ?? AddScroll(panel, "mxa-option-rows ou-setup-rows");
            p.rows.verticalScrollerVisibility = ScrollerVisibility.Auto; p.rows.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            for (int i = 0; i < ChampTabs.Length; i++)
            {
                int t = i;
                var pg = OuUi.Div("ou-spage", p.rows.contentContainer);
                pg.name = "champweekend-spage-" + ChampTabs[i].ToLowerInvariant();
                pg.RegisterCallback<FocusInEvent>(_ => { if (_cw == p && p.tab != t) ChampTab(t, false); });
                p.pages.Add(pg);
            }
            p.actions = OuTheme.Part(panel, "setup-actions", "ou-setup-actions");
            var start = OuUi.Button("START WEEKEND", "mxa-start ou-cta", StartChampWeekend, p.actions);
            start.name = "champweekend-start";
            OuTheme.Backdrop(start);
            _startButton = start;
            BuildChampRows();
            ChampTab(0, false);
            _nav.OnTab = d => ChampTab(((p.tab + d) % ChampTabs.Length + ChampTabs.Length) % ChampTabs.Length, true);
            _nav.OnCancel = () => { LeaveChampSetup(); return true; };
            _nav.OnAlt = null; _nav.OnFind = null;
            _nav.DefaultItem = () => OuUi.FirstItem(p.pages[p.tab]) ?? start;
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "EVENT SETUP"), (OuUi.KeyTabs, string.Join(" · ", ChampTabs)));
            _nav.Focus(start);
        }

        /// The weekend lives in the core's struct: the core library is loaded for it in the menus as the event's prestart loads it (the race
        /// scene loads the same library again: RxNativeLoader.Load keeps a loaded one). The weekend page used to fail here on a first start
        /// (DllNotFoundException) when nothing in the menus had loaded the library yet.
        static void ChampCoreLoaded()
        {
            if (RxNativeLoader.IsLoaded) return;
            try
            {
                string path = RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor));
                if (path != null) RxNativeLoader.Load(path, Application.isEditor ? System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Application.dataPath) ?? Application.dataPath, "Temp", "OpenReflex") : null);
            }
            catch (Exception e) { Debug.LogWarning("[ChampWeekend] the core library: " + e.Message); }
        }

        /// The weekend setup page is up (tests).
        public bool ChampSetupOpen => _cw != null && Current == Screen.Setup && _layer.Q("champweekend-page") != null;

        /// BACK / SINGLE RACE: the event's setup page again; the weekend that was being set up goes.
        void LeaveChampSetup()
        {
            _cw = null;
            if (RxChampWeekend.Active && RxChampWeekend.Stage == 0 && !RxChampWeekend.StageRaced(0)) RxChampWeekend.Abandon();
            ShowSetupPage();
        }

        void ChampTab(int tab, bool focus)
        {
            var p = _cw; if (p == null) return;
            p.tab = Mathf.Clamp(tab, 0, p.pages.Count - 1);
            for (int i = 0; i < p.pages.Count; i++) { p.pages[i].style.display = i == p.tab ? DisplayStyle.Flex : DisplayStyle.None; p.tabs[i].EnableInClassList("ou-gtab-on", i == p.tab); }
            p.rows.scrollOffset = Vector2.zero;
            if (!focus) return;
            var page = p.pages[p.tab];
            page.schedule.Execute(() => { if (page.panel != null && _cw == p) _nav.Focus(OuUi.FirstItem(page) ?? _startButton); }).ExecuteLater(1);
        }

        /// The rows on both tabs, the stages' flow and the summary.
        void BuildChampRows()
        {
            var p = _cw; var ev = _browseEvent; if (p == null || ev == null) return;
            string focused = _nav.Focused != null && p.rows.Contains(_nav.Focused) ? _nav.Focused.name : null;
            foreach (var pg in p.pages) pg.Clear();
            var e = ev.ev; var s = _setup;
            int count = RxChampWeekend.StageCount;

            // ---- STAGES: each stage's clock and laps (National / Supercross), else the event's own duration
            var page = p.pages[0];
            for (int st = 0; st < count; st++)
            {
                int stage = st; string label = RxChampWeekend.StageLabel(st);
                OuTheme.Section(page, label + (RxChampWeekend.StageScored(st) ? "  ·  POINTS" : ""));
                if (!RxChampWeekend.Timeable) continue;
                OuUi.Spinner("CLOCK", () => RxChampWeekend.StageMinutes(stage) > 0 ? RxChampWeekend.StageMinutes(stage) + ":00" : "OFF  ·  LAPS ONLY",
                    d => { ChampTiming(stage, Mathf.Clamp(RxChampWeekend.StageMinutes(stage) + d, 0, RxChampWeekend.MaxMinutes), RxChampWeekend.StageLaps(stage)); ChampFormatChanged(); }, page).name = "champweekend-clock-" + st;
                OuUi.Spinner(RxChampWeekend.StageMinutes(stage) > 0 ? "LAPS AFTER THE CLOCK" : "LAPS", () => RxChampWeekend.StageMinutes(stage) > 0 ? "+ " + RxChampWeekend.StageLaps(stage) : RxChampWeekend.StageLaps(stage).ToString(),
                    d => { ChampTiming(stage, RxChampWeekend.StageMinutes(stage), RxChampWeekend.StageLaps(stage) + d); ChampFormatChanged(); }, page).name = "champweekend-laps-" + st;
            }
            if (RxChampWeekend.Timeable)
            {
                var reset = OuUi.Button("DEFAULT CLOCKS AND LAPS", "ou-chip", ResetChampTiming, page);
                reset.name = "champweekend-reset";
            }
            else
            {   // Waypoint's rounds / Freestyle's time: the event's own duration, every stage the same
                OuTheme.Section(page, ev.activity == "Freestyle" ? "SESSION TIME" : "ROUNDS");
                var d = s.Durations(e, out bool laps, out string singular, out string plural);
                if (d.Count > 0)
                    OuUi.Spinner("EVERY STAGE", () => { int i = s.DurationIndex(d); return d[i] + " " + OpenContent.Str(d[i] > 1 ? plural : singular, laps ? (d[i] > 1 ? "LAPS" : "LAP") : "").ToUpperInvariant(); },
                        k => { s.duration = (s.DurationIndex(d) + k + d.Count) % d.Count; OpenHistory.SaveSetup(s); ChampFormatChanged(); }, page).name = "champweekend-duration";
                else OuUi.Text("Each stage runs the event's own " + (ev.activity == "Freestyle" ? "session time." : "rounds."), "mxa-note", page);
            }

            // ---- FIELD: the AI and the field
            page = p.pages[1];
            OuTheme.Section(page, "THE FIELD");
            OuUi.Spinner("AI OPPONENTS", () => OpenContent.AiName(s.Ai(e)), k => { s.ai = ((s.Ai(e) - 1 + k) % 4 + 4) % 4 + 1; OpenHistory.SaveSetup(s); ChampFormatChanged(); }, page).name = "champweekend-ai";
            // FIELD SIZE (w-rc-client's OpenFieldSize row): the event setup's own `riders` (the setup page's row shows the same value); every
            // stage launches with it (OpenSetup.LaunchRiders). National / Supercross up to 32, Waypoint / Freestyle the original's 12 (greyed).
            var slot = OuUi.Div("ou-cw-field-slot", page); slot.name = "champweekend-riders-slot";
            OpenFieldSize.Row(slot, e, () => s.Ai(e) > 0, () => s.riders, n => { s.riders = n; OpenHistory.SaveSetup(s); ChampFormatChanged(); }, "champweekend-riders");
            if (RxChampWeekend.Discipline == RxChampWeekend.Supercross)
            {
                p.split = OuUi.Text(ChampSplitLine(s.Riders(e)), "mxa-note ou-cw-split", slot);
                p.split.name = "champweekend-split";
            }
            OuUi.Text("Your vehicle, class and rider are the event's setup (EVENT SETUP, its RIDER tab).", "mxa-note", page);

            FillChampFlow();
            FillChampSummary();
            if (focused != null) { var again = p.rows.Q<VisualElement>(focused); if (again != null) _nav.Focus(again); }
        }

        /// A clock / laps / AI change: the flow and the summary follow (the spinners read their values themselves).
        void ChampFormatChanged()
        {
            if (_cw == null) return;
            for (int st = 0; st < RxChampWeekend.StageCount; st++)
            {   // the LAPS row's label follows the clock (LAPS AFTER THE CLOCK / LAPS)
                var row = _cw.rows.Q<VisualElement>("champweekend-laps-" + st)?.Q<Label>(className: "mxa-row-label");
                if (row != null) row.text = RxChampWeekend.StageMinutes(st) > 0 ? "LAPS AFTER THE CLOCK" : "LAPS";
            }
            var ev = _browseEvent;
            if (ev != null)
            {   // FIELD SIZE: the Supercross split follows the field
                int riders = _setup.Riders(ev.ev);
                if (_cw.rules != null) _cw.rules.text = ChampRulesText(RxChampWeekend.Discipline, riders);
                if (_cw.split != null) _cw.split.text = ChampSplitLine(riders);
            }
            FillChampFlow(); FillChampSummary();
        }

        void FillChampFlow()
        {
            var p = _cw; if (p == null) return;
            p.flow.Clear();
            for (int st = 0; st < RxChampWeekend.StageCount; st++)
            {
                var chip = OuUi.Div("ou-cw-stage" + (st == 0 ? " ou-cw-stage-next" : ""), p.flow);
                OuUi.Text(RxChampWeekend.StageLabel(st), "ou-cw-stage-name", chip).enableRichText = false;
                string f = RxChampWeekend.Timeable ? RxChampWeekend.StageFormat(st) : "";
                OuUi.Text(f.Length > 0 ? f : RxChampWeekend.StageScored(st) ? "POINTS" : " ", "ou-cw-stage-sub", chip).enableRichText = false;
            }
        }

        void FillChampSummary()
        {
            var p = _cw; var ev = _browseEvent; if (p == null || ev == null) return;
            var sum = p.summary; sum.Clear();
            var e = ev.ev; var s = _setup;
            OuTheme.Chip(sum, "FORMAT", "CHAMPIONSHIP WEEKEND", true);
            OuTheme.Chip(sum, "STAGES", RxChampWeekend.StageCount.ToString());
            OuTheme.Chip(sum, "FIELD", s.Riders(e) + " RIDERS");
            OuTheme.Chip(sum, "AI", OpenContent.AiName(s.Ai(e)));
            OuTheme.Chip(sum, "VEHICLE", OpenContent.VehicleTypeName(s.Type(e)));
            int cls = s.Class(e); if (cls >= 0) OuTheme.Chip(sum, "CLASS", OpenContent.ClassName(cls));
            int rider = s.Rider(e);
            OuTheme.Chip(sum, "RIDER", OpenSetup.Riders(e, s.Type(e)).FirstOrDefault(r => r.id == rider).name ?? "YOUR RIDER");
            int ms = OpenSetup.MapScalePercent(e);
            if (ms != RxMapScale.DefaultPercent) OuTheme.Chip(sum, "MAP SCALE", RxMapScale.Label(ms), true);
        }

        /// START WEEKEND: the first stage, launched as any race (the event's setup, its AI; the weekend's stage armed at the location).
        void StartChampWeekend()
        {
            var ev = _browseEvent; if (ev == null) return;
            if (!RxChampWeekend.Active || RxChampWeekend.EventKey != ev.key) { ShowChampWeekendSetup(); return; }
            if (!RxChampWeekend.Arm(ev.Location)) { Debug.LogWarning("[ChampWeekend] the first stage could not be armed"); return; }
            Debug.Log("[ChampWeekend] START WEEKEND: " + ev.key + ", " + string.Join(" > ", Enumerable.Range(0, RxChampWeekend.StageCount).Select(st => RxChampWeekend.StageLabel(st) + (RxChampWeekend.Timeable ? " " + RxChampWeekend.StageFormat(st) : ""))));
            _cw = null;
            var s = _setup.Clone();
            if (s.Ai(ev.ev) <= 0) s.ai = 2;
            StartEvent(ev, s);   // (back to this event's setup page when the weekend is over)
        }

        // ---- TIMED RACES: the stages' clock + laps, remembered per discipline and stage
        static string ChampTimingKey(int stage, string what) => "OpenReflex.ChampWeekend." + RxChampWeekend.Discipline + "." + stage + "." + what;

        /// A stage's clock / laps (the laps kept in range: 0 .. 30 after a clock, 1 .. 30 without one), saved for the next weekend.
        static void ChampTiming(int stage, int minutes, int laps)
        {
            laps = Mathf.Clamp(laps, minutes > 0 ? 0 : 1, RxChampWeekend.MaxLaps);
            if (!RxChampWeekend.SetTiming(stage, minutes, laps)) return;
            PlayerPrefs.SetInt(ChampTimingKey(stage, "min"), minutes);
            PlayerPrefs.SetInt(ChampTimingKey(stage, "laps"), laps);
            PlayerPrefs.Save();
        }

        /// The remembered format of each stage onto a new weekend (the core's defaults where nothing was saved).
        static void LoadChampTiming()
        {
            if (!RxChampWeekend.Timeable) return;
            for (int s = 0; s < RxChampWeekend.StageCount; s++)
            {
                if (!PlayerPrefs.HasKey(ChampTimingKey(s, "min"))) continue;
                int minutes = Mathf.Clamp(PlayerPrefs.GetInt(ChampTimingKey(s, "min"), 0), 0, RxChampWeekend.MaxMinutes);
                int laps = Mathf.Clamp(PlayerPrefs.GetInt(ChampTimingKey(s, "laps"), 3), minutes > 0 ? 0 : 1, RxChampWeekend.MaxLaps);
                RxChampWeekend.SetTiming(s, minutes, laps);
            }
        }

        /// DEFAULT CLOCKS AND LAPS: the saved choices of this discipline forgotten, a new weekend with the core's defaults.
        void ResetChampTiming()
        {
            var ev = _browseEvent; if (ev == null) return;
            for (int s = 0; s < RxChampWeekend.MaxStages; s++) { PlayerPrefs.DeleteKey(ChampTimingKey(s, "min")); PlayerPrefs.DeleteKey(ChampTimingKey(s, "laps")); }
            PlayerPrefs.Save();
            ChampCoreLoaded();
            if (!RxChampWeekend.Begin(ev.key, RxChampWeekend.DisciplineOf(ev.activity))) return;
            BuildChampRows();
            _nav.Focus(_cw?.rows.Q<VisualElement>("champweekend-reset") ?? _startButton);
        }
    }
}
