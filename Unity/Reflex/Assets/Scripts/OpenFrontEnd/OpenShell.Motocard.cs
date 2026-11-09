// The OPEN REFLEX MOTOCARD (the hub's MOTOCARD card, the garage RIDER tab's motocard header): the original's motocard pages in one screen with
// tabs, in the career menu's look (OcUi parts, OuMotocard.uss), the rider's live portrait at the top (OpenRiderPortrait), LB / RB (Q / E) for the tabs.
//   STATS         the card (Motocard.lua): the name, the jersey number, MOTOCAREER PROGRESS (App.CalculateCareerCompletion x 100, floored; with the
//                 medals it is made of), TOTAL CREDITS, the recent achievements; JERSEY NAME / NUMBER open the garage's RIDER tab. RECORD TOTALS
//                 are a port extra (sums of the player records the core keeps), marked as such on the page.
//   ACHIEVEMENTS  the 48 highlights (MotocardHighlights.lua: STR_HIGHLIGHTS) with their icons, titles, descriptions and progress, the original's order
//   RECORDS       the offline leaderboards (MotocardRecords.lua: STR_LEADERBOARDS_OFFLINE): activity type, location, the PlayerRecords of the
//                 focused location by the original's column (best lap / highest score / best total time)
//   TRICK LIBRARY the four tiers' stunts with their gesture pictures (StuntPreview.lua: STR_STUNTS); A on a trick plays it on the player's rider and
//                 vehicle in a live picture (MXGarage:PreviewStunt: OpenStuntPreview)
// The data and what each part is from: OpenMotocardData.cs. Read only: nothing here writes the profile.
// Test hooks: motocard-tab-<n> (class ou-gtab-on), motocard-hl-<HighlightId>, motocard-loc-<n>, motocard-trick-<n>, motocard-jersey-name /
// -number, motocard-back, motocard-recent-<n>, motocard-type (the RECORDS activity row).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        public const int MotocardStats = 0, MotocardAchievements = 1, MotocardRecords = 2, MotocardTricks = 3;
        static readonly string[] MotocardTabs = { "STATS", "ACHIEVEMENTS", "RECORDS", "TRICK LIBRARY" };

        int motocardTab;
        Action motocardBack;
        ProfileData motocardProfile;
        VisualElement motocardBody;
        readonly Dictionary<int, string> motocardFocus = new Dictionary<int, string>();
        int motocardRecType, motocardRecLocation;

        public int MotocardCurrentTab => motocardTab;

        // ------------------------------------------------------------------------------------------------ the screen

        /// The MOTOCARD page. back = where Back goes (null: the hub, the MOTOCARD card focused).
        public void ShowMotocard(int tab = MotocardStats, Action back = null)
        {
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) { OpenOriginalFor(OriginalEmbed.Purpose.Garage); return; }
            motocardBack = back ?? (() => { ShowMain(); _nav.Focus(_layer.Q<VisualElement>("ou-garage-card")); });
            var open = ReflexProfile.Open();
            Clear(Screen.Motocard, "career"); SetBackground(null); MusicTier(2); CareerChrome();
            var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuMotocard");
            if (uss && !Root.styleSheets.Contains(uss)) Root.styleSheets.Add(uss);
            OcUi.Stage(_layer, true);
            Action exit = () => motocardBack();
            _nav.OnCancel = () => { exit(); return true; };
            OcUi.Legend(_layer, (OuUi.KeySubmit, "SELECT", null), (OuUi.KeyCancel, "BACK", exit), (OuUi.KeyTabs, "TABS", null));
            if (!open.IsCompletedSuccessfully || !open.Result || ReflexProfile.Current == null)
            {
                var box = OuUi.Div("mc-page", _layer);
                OuUi.Text(ReflexProfile.Error ?? "Your motocard could not be opened.", "mc-empty", box).enableRichText = false;
                var back2 = OcUi.Button("BACK", "motocard-back", exit, box);
                _nav.DefaultItem = () => back2; _nav.Focus(back2);
                return;
            }
            motocardProfile = ReflexProfile.Current;
            motocardTab = Mathf.Clamp(tab, 0, MotocardTabs.Length - 1);
            motocardRecType = 0; motocardRecLocation = 0; motocardFocus.Clear();   // (a new visit starts at the defaults; the tabs keep their places within it)
            var page = OuUi.Div("mc-page", _layer);
            MotocardTop(page, motocardProfile);
            var tabs = OuUi.Div("ou-gtabs mc-tabs", page);
            string[] keys = OuUi.KeyTabs.Split('/');
            OuUi.Text(keys[0].Trim(), "mxa-key ou-gtabs-key", tabs);
            for (int i = 0; i < MotocardTabs.Length; i++)
            {
                int t = i;
                var b = OuUi.Button(MotocardTabs[i], "ou-gtab", () => SetMotocardTab(t, true), tabs);
                b.name = "motocard-tab-" + i; OuUi.Div("ou-gtab-bar", b);
            }
            OuUi.Text(keys.Length > 1 ? keys[1].Trim() : "", "mxa-key ou-gtabs-key", tabs);
            motocardBody = OuUi.Div("mc-body", page);
            _nav.OnTab = d => SetMotocardTab((motocardTab + d + MotocardTabs.Length) % MotocardTabs.Length, true);
            _nav.DefaultItem = MotocardDefaultItem;
            SetMotocardTab(motocardTab, true);
        }

        VisualElement MotocardDefaultItem()
        {
            if (motocardBody != null) { var first = OuUi.FirstItem(motocardBody); if (first != null) return first; }
            return _layer.Q<VisualElement>("motocard-tab-" + motocardTab);
        }

        void SetMotocardTab(int tab, bool focus, string focusName = null)
        {
            if (motocardBody == null) return;
            motocardTab = Mathf.Clamp(tab, 0, MotocardTabs.Length - 1);
            ScreenUpdate = null;   // (the TRICK LIBRARY's preview ticks while it is on show)
            motocardBody.Clear();
            for (int i = 0; i < MotocardTabs.Length; i++) _layer.Q<VisualElement>("motocard-tab-" + i)?.EnableInClassList("ou-gtab-on", i == motocardTab);
            var p = motocardProfile;
            switch (motocardTab)
            {
                case MotocardStats: MotocardStatsTab(motocardBody, p); break;
                case MotocardAchievements: MotocardAchievementsTab(motocardBody, p); break;
                case MotocardRecords: MotocardRecordsTab(motocardBody, p); break;
                default: MotocardTricksTab(motocardBody); break;
            }
            OuMotion.Enter(motocardBody, 0.05f, 0.0f);
            string remembered = focusName ?? (motocardFocus.TryGetValue(motocardTab, out var n) ? n : null);
            VisualElement target = remembered != null ? motocardBody.Q<VisualElement>(remembered) : null;
            if (focus) _nav.Focus(target ?? MotocardDefaultItem());
        }

        void MotocardRemember(VisualElement e) { if (e != null && !string.IsNullOrEmpty(e.name)) motocardFocus[motocardTab] = e.name; }

        // ------------------------------------------------------------------------------------------------ the top band: portrait, name, figures

        void MotocardTop(VisualElement page, ProfileData p)
        {
            var top = OuUi.Div("mc-top", page);
            PortraitRefresh();
            PortraitFrame(top, "motocard-portrait");
            var id = OuUi.Div("mc-id", top);
            OuUi.Text(OpenMotocardData.Text("STR_MOTOCARD", "MOTOCARD").ToUpperInvariant(), "mc-eyebrow", id).enableRichText = false;
            var row = OuUi.Div("mc-name-row", id);
            OuUi.Text("#" + OpenMotocardData.RiderNumber(p), "mc-number", row).enableRichText = false;
            OuUi.Text(OpenMotocardData.RiderName.ToUpperInvariant(), "mc-name", row).enableRichText = false;
            bool all = CoreGarageProfile.UnlockEverythingEnabled;
            OuUi.Text(all ? "UNLOCKED PROFILE  ·  YOUR EARNED CAREER STAYS SEPARATE" : "YOUR RIDER  ·  YOUR CAREER", "mc-note", id).enableRichText = false;
            OpenMotocardData.Highlights(p, out int unlocked);
            var figures = OuUi.Div("mc-figures", top);
            OcUi.Stat(figures, Label("EMB_CAREER_PROGRESS", "MOTOCAREER PROGRESS"), OpenMotocardData.CareerPercent(p) + "%");
            OcUi.Stat(figures, Label("EMB_TOTAL_CREDITS", "TOTAL CREDITS"), all ? "ALL UNLOCKED" : FeGarageData.Number(OpenMotocardData.Credits(p)), "gold");
            OcUi.Stat(figures, OpenMotocardData.Text("STR_HIGHLIGHTS", "ACHIEVEMENTS").ToUpperInvariant(), unlocked + "/" + T().Rows("Highlight").Count, "good");
        }
        static FeTables T() => FeTables.Shared;
        /// The label the original's page puts before a value ("MotoCareer Progress: " -> MOTOCAREER PROGRESS).
        static string Label(string embedKey, string def) => OpenMotocardData.Embedded(embedKey, def).Trim().TrimEnd(':').Trim().ToUpperInvariant();

        static VisualElement Column(VisualElement parent, string cls, bool last = false)
        {
            var c = OuUi.Div("mc-col " + cls + (last ? " mc-col-last" : ""), parent);
            return c;
        }

        static ScrollView ListScroll(VisualElement parent, string name)
        {
            var scroll = new ScrollView(ScrollViewMode.Vertical) { name = name };
            scroll.AddToClassList("mc-scroll");
            scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            scroll.verticalScrollerVisibility = ScrollerVisibility.Auto;
            parent.Add(scroll);
            return scroll;
        }

        // ------------------------------------------------------------------------------------------------ STATS

        void MotocardStatsTab(VisualElement body, ProfileData p)
        {
            // MOTOCAREER PROGRESS and the medals it is made of
            var a = Column(body, "mc-stats-a");
            OcUi.Panel(a, Label("EMB_CAREER_PROGRESS", "MOTOCAREER PROGRESS"), out var pb, out _, "mc-panel");
            int pct = OpenMotocardData.CareerPercent(p);
            var big = OuUi.Div("mc-pct", pb);
            OuUi.Text(pct.ToString(), "mc-pct-num", big).enableRichText = false; OuUi.Text("%", "mc-pct-suffix", big);
            var bar = OuUi.Div("mc-bar", pb); OuUi.Div("mc-bar-fill", bar).style.width = Length.Percent(Mathf.Clamp(pct, 0, 100));
            int[] medals = new int[3]; int counted = 0, series = 0;
            var status = p.T("PlayerSeriesStatus");
            foreach (var s in T().Rows("Series"))
            {
                if (FeTables.F(s, "CareerCompletionFactor") <= 0f) continue;
                series++;
                int row = status.Find("SeriesId", FeTables.I(s, "SeriesId")); if (row < 0) continue;
                int place = status.Int("BestPlaceFinish", row, -1);
                if (place >= 0 && place <= 2) { medals[place]++; counted++; }
            }
            OuUi.Text(counted + " OF " + series + " SERIES MEDALLED  ·  GOLD COUNTS IN FULL, SILVER TWO THIRDS, BRONZE ONE THIRD", "mc-caption", pb).enableRichText = false;
            var row3 = OuUi.Div("mc-medals", pb);
            string[] names = { "GOLD", "SILVER", "BRONZE" };
            for (int m = 0; m < 3; m++)
            {
                var tile = OuUi.Div("mc-medal mc-medal-" + names[m].ToLowerInvariant(), row3); tile.pickingMode = PickingMode.Ignore;
                var cup = OuUi.Div("mc-medal-cup", tile); var tex = OcUi.Trophy(m); if (tex) cup.style.backgroundImage = tex;
                var col = OuUi.Div(null, tile);
                OuUi.Text(medals[m].ToString(), "mc-medal-num", col);
                OuUi.Text(names[m], "oc-stat-label", col);
            }
            var edit = OuUi.Div("mc-edit", a);
            var jn = OcUi.Button(OpenMotocardData.Text("STR_CHANGE_RIDER_NAME", "JERSEY NAME").ToUpperInvariant(), "motocard-jersey-name", () => ShowGarage(GarageIdentity), edit, "oc-btn-quiet");
            var jr = OcUi.Button(OpenMotocardData.Text("STR_CHANGE_RIDER_NUMBER", "JERSEY NUMBER").ToUpperInvariant(), "motocard-jersey-number", () => ShowGarage(GarageIdentity), edit, "oc-btn-quiet");
            jn.RegisterCallback<FocusInEvent>(_ => MotocardRemember(jn)); jr.RegisterCallback<FocusInEvent>(_ => MotocardRemember(jr));
            OuWidgets.LinkColumn(new[] { jn, jr });

            // RECENT ACHIEVEMENTS (the card's seven icons, here with their names; A opens the achievement)
            var b = Column(body, "mc-stats-b");
            OcUi.Panel(b, OpenMotocardData.Text("STR_RECENT_HIGHLIGHTS", "RECENT ACHIEVEMENTS").ToUpperInvariant(), out var rb, out _, "mc-panel");
            var recent = OpenMotocardData.RecentHighlights(p);
            var all = OpenMotocardData.Highlights(p, out _);
            var rows = new List<VisualElement>();
            if (recent.Count == 0) OuUi.Text("NO ACHIEVEMENTS UNLOCKED YET. RIDE EVENTS AND PLAY THE CAREER TO EARN THEM.", "mc-empty", rb).enableRichText = false;
            for (int i = 0; i < recent.Count; i++)
            {
                var h = all.FirstOrDefault(x => x.id == recent[i]); if (h == null) continue;
                int hid = h.id;
                var row = OuUi.Item(OuUi.Div("mc-recent", rb), () => SetMotocardTab(MotocardAchievements, true, "motocard-hl-" + hid));
                row.name = "motocard-recent-" + i;
                var icon = OuUi.Div("mc-recent-icon", row); var tex = OpenMotocardData.HighlightIcon(h.id, false); if (tex) icon.style.backgroundImage = tex;
                OuUi.Text(h.title, "mc-recent-title", row).enableRichText = false;
                OcUi.Tag(row, h.category).AddToClassList("mc-recent-tag");
                row.RegisterCallback<FocusInEvent>(_ => MotocardRemember(row));
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);

            // RECORD TOTALS (a port extra: sums of the player records the core keeps)
            var c = Column(body, "mc-stats-c", true);
            OcUi.Panel(c, "RECORD TOTALS", out var cb, out _, "mc-panel");
            var t = OpenMotocardData.Totals(p);
            Line(cb, "TOTAL EARNINGS", FeGarageData.Number(t.earnings), true);
            Line(cb, "EVENTS RIDDEN", FeGarageData.Number(t.events));
            Line(cb, "LAPS RIDDEN", FeGarageData.Number(t.laps));
            Line(cb, "RIDE TIME", OpenMotocardData.Duration(t.seconds));
            Line(cb, "JUMPS ATTEMPTED", FeGarageData.Number(t.jumps));
            Line(cb, "JUMPS MISSED", FeGarageData.Number(t.failed));
            Line(cb, "COMBOS COMPLETED", FeGarageData.Number(t.combos));
            Line(cb, "HOLESHOTS WON", FeGarageData.Number(t.holeshots));
            OuUi.Text("ADDED BY THIS PORT FROM THE RECORDS THE GAME KEEPS FOR EVERY EVENT; THE ORIGINAL'S MOTOCARD DOES NOT SHOW THEM", "mc-extra", cb).enableRichText = false;
        }

        static void Line(VisualElement parent, string label, string value, bool gold = false)
        {
            var line = OuUi.Div("mc-line" + (gold ? " mc-line-gold" : ""), parent); line.pickingMode = PickingMode.Ignore;
            OuUi.Text(label, "mc-line-label", line).enableRichText = false;
            OuUi.Text(value, "mc-line-value", line).enableRichText = false;
        }

        // ------------------------------------------------------------------------------------------------ ACHIEVEMENTS

        void MotocardAchievementsTab(VisualElement body, ProfileData p)
        {
            var list = OpenMotocardData.Highlights(p, out int unlocked);
            var a = Column(body, "mc-hl-a");
            var head = OuUi.Div("mc-hl-head", a); head.pickingMode = PickingMode.Ignore;
            var line = OuUi.Div("mc-hl-headline", head);
            OuUi.Text(unlocked + "/" + list.Count, "mc-hl-count", line);
            OuUi.Text(OpenMotocardData.Text("STR_HIGHLIGHTS", "ACHIEVEMENTS").ToUpperInvariant() + " UNLOCKED", "mc-hl-count-label", line);
            var bar = OuUi.Div("mc-bar mc-bar-gold mc-hl-headbar", head);
            OuUi.Div("mc-bar-fill", bar).style.width = Length.Percent(list.Count == 0 ? 0 : 100f * unlocked / list.Count);
            var scroll = ListScroll(a, "motocard-list");

            var b = Column(body, "mc-hl-b", true);
            OcUi.Panel(b, "ACHIEVEMENT", out var db, out var dtitle, "mc-panel");
            var rows = new List<VisualElement>();
            foreach (var h in list)
            {
                var hh = h;
                var row = OuUi.Item(OuUi.Div("mc-hl " + (h.unlocked ? "mc-hl-unlocked" : "mc-hl-locked"), scroll.contentContainer), null);
                row.name = "motocard-hl-" + h.id;
                var icon = OuUi.Div("mc-hl-icon", row); var tex = OpenMotocardData.HighlightIcon(h.id, !h.unlocked); if (tex) icon.style.backgroundImage = tex;
                var copy = OuUi.Div("mc-hl-copy", row); copy.pickingMode = PickingMode.Ignore;
                OuUi.Text(h.title, "mc-hl-title", copy).enableRichText = false;
                OuUi.Text(h.description, "mc-hl-desc", copy).enableRichText = false;
                var state = OuUi.Div("mc-hl-state", row); state.pickingMode = PickingMode.Ignore;
                if (!string.IsNullOrEmpty(h.ProgressText)) OuUi.Text(h.ProgressText, "mc-hl-progress", state).enableRichText = false;
                OcUi.Tag(state, h.unlocked ? "UNLOCKED" : "LOCKED", h.unlocked ? "orange" : null);
                row.RegisterCallback<FocusInEvent>(_ => { scroll.ScrollTo(row); MotocardRemember(row); MotocardHighlightDetail(db, dtitle, hh); });
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);
            if (list.Count > 0) MotocardHighlightDetail(db, dtitle, list[0]);
        }

        void MotocardHighlightDetail(VisualElement db, Label title, MotocardHighlight h)
        {
            db.Clear();
            title.text = h.title.ToUpperInvariant();
            var big = OuUi.Div("mc-hl-big", db); var tex = OpenMotocardData.HighlightIcon(h.id, !h.unlocked); if (tex) big.style.backgroundImage = tex;
            OuUi.Text(h.title.ToUpperInvariant(), "mc-hl-bigtitle", db).enableRichText = false;
            var tags = OuUi.Div("mc-hl-tags", db);
            OcUi.Tag(tags, h.category);
            OcUi.Tag(tags, h.unlocked ? "UNLOCKED" : "LOCKED", h.unlocked ? "orange" : null);
            if (h.unlocked && h.order > 0) OcUi.Tag(tags, "NUMBER " + h.order + " TO UNLOCK");
            OuUi.Text(h.description, "mc-hl-bigdesc", db).enableRichText = false;
            if (h.goal > 1)
            {
                var meter = OuUi.Div("mc-hl-meter", db);
                Line(meter, "PROGRESS", h.progress + " / " + h.goal);
                var bar = OuUi.Div("mc-bar mc-bar-gold", meter); OuUi.Div("mc-bar-fill", bar).style.width = Length.Percent(100f * h.Fraction);
            }
        }

        // ------------------------------------------------------------------------------------------------ RECORDS

        void MotocardRecordsTab(VisualElement body, ProfileData p)
        {
            var types = OpenMotocardData.RecordTypes();
            motocardRecType = Mathf.Clamp(motocardRecType, 0, Math.Max(0, types.Count - 1));
            var a = Column(body, "mc-rec-a");
            OuUi.Text(OpenMotocardData.Text("STR_LEADERBOARDS_OFFLINE", "OFFLINE LEADERBOARDS").ToUpperInvariant(), "oc-heading", a).enableRichText = false;
            var b = Column(body, "mc-rec-b", true);
            OcUi.Panel(b, "", out var rb, out var rtitle, "mc-panel");
            if (types.Count == 0) { OuUi.Text("NO LEADERBOARDS IN THIS INSTALL", "mc-empty", rb); return; }
            ScrollView scroll = null; VisualElement spinner = null;
            var rows = new List<VisualElement>();
            void Fill()
            {
                var type = types[motocardRecType];
                var locs = OpenMotocardData.RecordLocations(type);
                scroll.contentContainer.Clear(); rows.Clear();
                motocardRecLocation = Mathf.Clamp(motocardRecLocation, 0, Math.Max(0, locs.Count - 1));
                for (int i = 0; i < locs.Count; i++)
                {
                    int li = i; var loc = locs[i];
                    var recs = OpenMotocardData.Records(p, type, loc);
                    var row = OuUi.Item(OuUi.Div("mc-loc" + (recs.Count == 0 ? " mc-loc-none" : ""), scroll.contentContainer), null);
                    row.name = "motocard-loc-" + i;
                    OuUi.Text(loc.name, "mc-loc-name", row).enableRichText = false;
                    OuUi.Text(recs.Count == 0 ? "NO RECORD" : recs[0].value, "mc-loc-best", row).enableRichText = false;
                    row.RegisterCallback<FocusInEvent>(_ => { scroll.ScrollTo(row); MotocardRemember(row); motocardRecLocation = li; MotocardRecordBoard(rb, rtitle, p, type, loc); });
                    rows.Add(row);
                }
                var chain = new List<VisualElement> { spinner }; chain.AddRange(rows);
                OuWidgets.LinkColumn(chain);
                if (locs.Count > 0) MotocardRecordBoard(rb, rtitle, p, type, locs[motocardRecLocation]);
                else { rb.Clear(); rtitle.text = type.name; OuUi.Text(OpenMotocardData.Text("STR_NO_LEADERBOARD_RESULTS", "No Leaderboard Scores").ToUpperInvariant(), "mc-empty", rb); }
            }
            spinner = OuUi.Spinner("ACTIVITY", () => types[motocardRecType].name, d =>
            {
                motocardRecType = Mathf.Clamp(motocardRecType + d, 0, types.Count - 1); motocardRecLocation = 0; Fill();
            }, a);
            spinner.name = "motocard-type";
            spinner.RegisterCallback<FocusInEvent>(_ => MotocardRemember(spinner));
            scroll = ListScroll(a, "motocard-locations");
            Fill();
        }

        void MotocardRecordBoard(VisualElement rb, Label title, ProfileData p, MotocardRecordType type, MotocardRecordLocation loc)
        {
            rb.Clear(); title.text = type.name;
            var top = OuUi.Div("mc-rec-top", rb); top.pickingMode = PickingMode.Ignore;
            var poster = OuUi.Div("mc-poster", top); var tex = OpenContent.Icon(loc.posterIcon); if (tex) poster.style.backgroundImage = tex;
            var meta = OuUi.Div("mc-rec-meta", top);
            OuUi.Text(loc.name, "mc-rec-loc", meta).enableRichText = false;
            OuUi.Text(type.header, "mc-rec-sub", meta).enableRichText = false;
            var recs = OpenMotocardData.Records(p, type, loc);
            if (recs.Count == 0)
            {
                OuUi.Text(OpenMotocardData.Text("STR_NO_LEADERBOARD_RESULTS", "No Leaderboard Scores").ToUpperInvariant(), "mc-empty", rb).enableRichText = false;
                return;
            }
            var board = OuUi.Div("mc-rec-board mc-scroll", rb);
            OcUi.BoardHead(board, "#", "RIDER", "VEHICLE", type.header, null);
            var sv = ListScroll(board, "motocard-records");
            foreach (var r in recs) OcUi.BoardRow(sv.contentContainer, r.index, r.name, false, r.vehicle, r.value, null);
        }

        // ------------------------------------------------------------------------------------------------ TRICK LIBRARY

        void MotocardTricksTab(VisualElement body)
        {
            var stunts = OpenMotocardData.Stunts();
            var a = Column(body, "mc-tr-a");
            var scroll = ListScroll(a, "motocard-tricks");
            var b = Column(body, "mc-tr-b", true);
            OcUi.Panel(b, OpenMotocardData.Text("STR_STUNTS", "TRICK LIBRARY").ToUpperInvariant(), out var db, out var dtitle, "mc-panel");
            if (stunts.Count == 0) { OuUi.Text("THE TRICK LIBRARY COULD NOT BE READ FROM THIS INSTALL", "mc-empty", scroll.contentContainer); return; }
            // the detail: the live preview (the player's rider and vehicle performing the focused trick), beside the gesture and the name
            var detail = new TrickDetail { panel = db, title = dtitle };
            var rows = new List<VisualElement>(); int lastTier = -1, n = 0;
            foreach (var s in stunts)
            {
                if (s.tier != lastTier)
                {
                    lastTier = s.tier;
                    OuUi.Text(OpenMotocardData.Text("STR_STUNT_TIER" + s.tier, "Tier " + s.tier).ToUpperInvariant(), "oc-heading mc-tr-head", scroll.contentContainer).enableRichText = false;
                }
                var st = s;
                // StuntPreview:OnAcceptStuntList: A previews the stunt on the garage rider
                var row = OuUi.Item(OuUi.Div("mc-trick", scroll.contentContainer), () => PlayTrick(detail, st));
                row.name = "motocard-trick-" + n++;
                var img = Gesture(s.material, "mc-trick-img"); if (img != null) row.Add(img);
                OuUi.Text(s.display, "mc-trick-name", row).enableRichText = false;
                row.RegisterCallback<FocusInEvent>(_ => { scroll.ScrollTo(row); MotocardRemember(row); MotocardTrickDetail(detail, st); });
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);
            BuildTrickDetail(detail, stunts[0]);
        }

        /// The TRICK LIBRARY's detail panel: its elements and the stunt it shows.
        sealed class TrickDetail
        {
            public VisualElement panel, picture, hint, hints; public Label title, name, note; public MotocardStunt stunt; public bool previewTried;
        }

        void BuildTrickDetail(TrickDetail d, MotocardStunt first)
        {
            d.panel.Clear();
            var row = OuUi.Div("mc-trick-detail", d.panel);
            d.picture = OuUi.Div("mc-trick-preview", row); d.picture.name = "motocard-trick-preview"; d.picture.pickingMode = PickingMode.Ignore;
            var info = OuUi.Div("mc-trick-info", row);
            d.hint = OuUi.Div("mc-trick-gesture", info);
            d.name = OuUi.Text("", "mc-trick-bigname", info); d.name.enableRichText = false;
            d.note = OuUi.Text("", "mc-trick-note", info); d.note.enableRichText = false;
            d.hints = OuUi.Div("mc-trick-hints", info); d.hints.pickingMode = PickingMode.Ignore;
            ScreenUpdate = () => { if (_stunt != null && _stunt.Ready) _stunt.Tick(Time.unscaledDeltaTime); };
            MotocardTrickDetail(d, first);
            // the preview's garage is built a moment after the page (it reads the rider's and the vehicle's models)
            d.panel.schedule.Execute(() => TrickPreviewStart(d)).ExecuteLater(120);
        }

        void TrickPreviewStart(TrickDetail d)
        {
            if (d.picture == null || d.picture.panel == null || Current != Screen.Motocard || motocardTab != MotocardTricks) return;
            d.previewTried = true;
            try
            {
                _stunt ??= new OpenStuntPreview(transform);
                if (_stunt.Refresh(MenuWorld)) { _stunt.Bind(d.picture); _stunt.SetStunt(d.stunt?.anim); }
            }
            catch (Exception e) { Debug.LogWarning("[StuntPreview] " + e.Message); }
            TrickNote(d);
        }

        void MotocardTrickDetail(TrickDetail d, MotocardStunt s)
        {
            d.stunt = s;
            d.title.text = OpenMotocardData.Text("STR_STUNT_TIER" + s.tier, "Tier " + s.tier).ToUpperInvariant();
            d.hint.Clear();
            var big = Gesture(s.material, "mc-trick-big"); if (big != null) d.hint.Add(big);
            d.name.text = s.display;
            if (_stunt != null && _stunt.Ready) _stunt.SetStunt(s.anim);
            TrickNote(d);
        }

        /// What the picture under the name says: A plays it, or why it cannot.
        void TrickNote(TrickDetail d)
        {
            if (d.note == null) return;
            bool ready = _stunt != null && _stunt.Ready;
            bool performance = ready && _stunt.Anim != null;
            d.note.text = !d.previewTried ? "" : !ready ? (_stunt?.Problem ?? "THE PREVIEW IS NOT AVAILABLE").ToUpperInvariant()
                        : performance ? "" : "A PHYSICS TRICK: THE BIKE'S OWN MOTION, NO ANIMATION";
            d.hints.Clear();
            if (performance) OuUi.Hints(d.hints, (OuUi.KeySubmit, "PREVIEW"));
            d.picture.EnableInClassList("mc-trick-preview-off", !ready);
        }

        void PlayTrick(TrickDetail d, MotocardStunt s)
        {
            if (d.stunt != s) MotocardTrickDetail(d, s);
            if (_stunt != null && _stunt.Ready && _stunt.Play()) return;
            OuSound.Play("Back");
        }

        // ---- the preview's picture (OpenStuntPreview), kept while the MOTOCARD is the screen
        OpenStuntPreview _stunt;
        internal OpenStuntPreview StuntPreview => _stunt;
        void ReleaseStuntPreview() { _stunt?.Destroy(); _stunt = null; }

        static Image Gesture(string material, string cls)
        {
            try
            {
                var (tex, uv) = OpenContent.Assets.Material(material);
                if (!tex) return null;
                var img = new Image { image = tex, uv = uv, scaleMode = ScaleMode.ScaleToFit, pickingMode = PickingMode.Ignore };
                img.AddToClassList(cls);
                return img;
            }
            catch (Exception e) { Debug.LogWarning("[Motocard] gesture " + material + ": " + e.Message); return null; }
        }

    }
}
