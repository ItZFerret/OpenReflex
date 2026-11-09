// The career hub ("RACE & RIDE"), inspired by the original's Race & Ride + Events pages on ONE page: a carousel of tall tilted posters, one per
// SERIES TIER (the parent store product: Waypoint Series 1, Pro Nationals ...), the focused one large and saturated, the others washed out; an
// information panel under an orange title bar (the tier's text and sponsor, or, with an event focused, its location photo, format and vehicle);
// the tier's EVENTS under it (gold row = focus, medal squares as the original's progress slots). Select a poster (A / Enter / click) to step
// into its events, Back to step out; LB / RB (Q / E) change the tier from anywhere. Every series of the catalog is a row named
// "career-series-<SeriesId>" (the test fixtures click them), enabled exactly when the core admits the series; a locked one, or one for a vehicle the
// player does not ride, can still be chosen and shows the original's popup (LockedPopup / InvalidRaceRequirements; the owner's select callback), VEHICLE LIST
// (Y / F: VehiclesQuickViewPopup) swaps the vehicle from the event, and the player's credits sit top right.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OcRaceAndRide
    {
        sealed class Tier
        {
            public int Parent, Index, Sort;
            public string Title, Description;
            public Texture2D Art, Brand;
            public readonly List<Ev> Events = new List<Ev>();
            public VisualElement Poster, Badge;
            public ScrollView List;
            public bool AllLocked, AllGold;
            public int Medals => Events.Count(e => e.Choice.BestPlace >= 0);
            /// The first event that can be focused (a locked event can: choosing it shows the LockedPopup, as the original's).
            public Ev FirstSelectable => Events.FirstOrDefault(e => Navigable(e.Choice));
        }
        /// An event row the player can reach: every series the core admits (a locked one or one for another vehicle shows the original's popup when chosen).
        static bool Navigable(CareerEventChoice c) => c.Authored && c.Supported && c.UnownedRewardVehicles == 0;
        sealed class Ev { public CareerEventChoice Choice; public Tier Tier; public int Number; public string Title, Location; public VisualElement Row; }

        readonly OuNav nav;
        readonly Action back;
        readonly Action<CareerEventChoice> select;
        /// LockedPopup: a poster whose series are all locked was chosen (RaceAndRide:OnAcceptSeries).
        public Action Locked;
        /// VehiclesQuickViewPopup on the focused event (the legend's VEHICLE LIST, Y / F) and whether the event allows it (RaceAndRideEvents:
        /// the series is unlocked and the player owns a vehicle of its class).
        public Action<CareerEventChoice> QuickView;
        public Func<CareerEventChoice, bool> CanQuickView;
        VisualElement legendRow;
        bool quickShown;
        Label creditsValue;
        readonly VisualElement carousel, infoTierPage, infoEventPage, slots, eventsBody, rideArt, rideArt2;
        readonly Label rideName, rideCap;
        readonly Label infoTitle, eventsTitle;
        readonly OcArrow leftArrow, rightArrow;
        public readonly Label Note;
        Label loading;
        readonly List<Tier> tiers = new List<Tier>();
        int cur;
        Ev shown;

        public VisualElement DefaultFocus => tiers.Count > 0 ? tiers[Mathf.Clamp(cur, 0, tiers.Count - 1)].Poster : null;

        /// The hub's frame, drawn at once (the catalog is still being read): stage, ride plate, note, panels, plaque, legend.
        public OcRaceAndRide(VisualElement layer, OuNav nav, Action back, Action<CareerEventChoice> select)
        {
            this.nav = nav; this.back = back; this.select = select;
            OcUi.Stage(layer, true);
            var top = OuUi.Div("oc-top", layer);
            var ride = OuUi.Div("oc-ride", top); ride.pickingMode = PickingMode.Ignore;
            rideArt = OuUi.Div("oc-ride-art", ride); rideArt2 = OuUi.Div("oc-ride-art oc-ride-art-2", ride);
            var text = OuUi.Div("oc-ride-text", ride);
            rideCap = OuUi.Text("", "oc-ride-cap", text); rideName = OuUi.Text("", "oc-ride-name", text);
            rideArt.style.display = rideArt2.style.display = DisplayStyle.None;
            Note = OuUi.Text("READING YOUR CAREER…", "oc-note", top); Note.enableRichText = false;
            // the player's credits (the Garage's balance; the original's PlayerAccountBalance layer shows DYN_ACCOUNT_BALANCE)
            var credits = OuUi.Div("p2-credits", top); credits.pickingMode = PickingMode.Ignore; credits.name = "career-credits";
            OuUi.Text("CREDITS", "p2-credits-label", credits).enableRichText = false;
            creditsValue = OuUi.Text("", "p2-credits-value", credits); creditsValue.enableRichText = false;
            RefreshCredits();

            var mid = OuUi.Div("oc-mid", layer);
            carousel = OuUi.Div("oc-carousel", mid);
            loading = OuUi.Text("READING YOUR CAREER…", "oc-big", carousel); loading.enableRichText = false;
            loading.style.alignSelf = Align.Center; loading.style.marginTop = 260;
            leftArrow = new OcArrow(true); rightArrow = new OcArrow(false);
            leftArrow.AddToClassList("oc-arrow-left"); rightArrow.AddToClassList("oc-arrow-right");
            foreach (var (arrow, step) in new[] { (leftArrow, -1), (rightArrow, 1) })
            {
                var a = arrow; int d = step; a.pickingMode = PickingMode.Position; a.AddToClassList("oc-arrow-off");
                a.RegisterCallback<ClickEvent>(e => { if (e.button == 0) Step(d, true); });
                carousel.Add(a);
            }

            float wheelAt = -1f;   // the mouse wheel over the posters changes the tier (one step per notch)
            carousel.RegisterCallback<WheelEvent>(e =>
            {
                if (Time.unscaledTime - wheelAt < 0.12f || Mathf.Abs(e.delta.y) < 0.01f) return;
                wheelAt = Time.unscaledTime; Step(e.delta.y > 0 ? 1 : -1, true); e.StopPropagation();
            });
            var side = OuUi.Div("oc-side", mid);
            var info = OcUi.Panel(side, "", out var infoBody, out infoTitle, "oc-info");
            infoTierPage = OuUi.Div("oc-info-page", infoBody); infoEventPage = OuUi.Div("oc-info-page oc-info-off", infoBody);
            infoTierPage.pickingMode = infoEventPage.pickingMode = PickingMode.Ignore;
            var events = OcUi.Panel(side, OpenContent.Str("STR_EVENT_SELECT", "EVENTS").ToUpperInvariant(), out eventsBody, out eventsTitle, "oc-events");
            slots = OuUi.Div("oc-slots", events.Q<VisualElement>(className: "oc-panel-bar")); slots.pickingMode = PickingMode.Ignore;

            var bottom = OuUi.Div("oc-bottom", layer);
            OcUi.Plaque(bottom, OpenContent.Str("STR_RACEANDRIDE", "RACE & RIDE").ToUpperInvariant());
            var legend = OcUi.Legend(layer, (OuUi.KeySubmit, "SELECT", null), (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant(), back), (OuUi.KeyTabs, "SERIES", null));
            legendRow = legend.Q<VisualElement>(className: "oc-legend-items");
            quickShown = true; SyncQuickLegend();   // (the VEHICLE LIST item only while the quick view is allowed: RaceAndRideEvents' toggle legend)
            OuMotion.Enter(side, 0.06f, 0.05f);
            nav.OnCancel = () => { back(); return true; };
            nav.OnAlt = DoQuickView;
        }

        /// The catalog's answer: tiers, posters and events.
        public void Fill(IReadOnlyList<CareerEventChoice> choices, string note)
        {
            Note.text = note; RefreshCredits();
            loading?.RemoveFromHierarchy(); loading = null;
            foreach (var t in Group(choices)) tiers.Add(t);
            if (tiers.Count == 0) { ShowMessage("No supported career events were found in these game files."); return; }
            for (int i = 0; i < tiers.Count; i++) { tiers[i].Index = i; BuildPoster(tiers[i]); carousel.Add(tiers[i].Poster); }
            foreach (var t in tiers) BuildList(t);
            Link();
            cur = -1; SetTier(0, false); ShowEvent(null);
            // the posters slide in from the right, one after the other
            foreach (var t in tiers) { t.Poster.style.opacity = 0f; t.Poster.style.translate = new Translate(260f + t.Index * 40f, 0f); }
            carousel.schedule.Execute(ApplyCarousel).ExecuteLater(60);
        }

        /// A line instead of the career (nothing to show, or the catalog failed).
        public void ShowMessage(string text)
        {
            Note.text = text;
            if (loading == null) { loading = OuUi.Text(text, "oc-big", carousel); loading.style.alignSelf = Align.Center; loading.style.marginTop = 260; loading.enableRichText = false; }
            loading.text = text; loading.style.fontSize = 44; loading.style.whiteSpace = WhiteSpace.Normal;
        }

        // ------------------------------------------------------------------------------------------------ data

        static List<Tier> Group(IReadOnlyList<CareerEventChoice> choices)
        {
            var tables = FeTables.Shared;
            var products = new Dictionary<int, Dictionary<string, object>>();
            foreach (var r in tables.Rows("StoreProducts")) products[FeTables.I(r, "StoreProductId")] = r;
            var brandIcon = new Dictionary<int, int>();
            foreach (var r in tables.Rows("Brand")) brandIcon[FeTables.I(r, "BrandId")] = FeTables.I(r, "BrandIconId");
            var byParent = new Dictionary<int, Tier>();
            foreach (var choice in choices)
            {
                int pid = choice.Entry.ProductId, parent = pid;
                if (products.TryGetValue(pid, out var row)) { int p = FeTables.I(row, "ParentStoreProductId"); if (p >= 0) parent = p; }
                if (!byParent.TryGetValue(parent, out var tier))
                {
                    tier = new Tier { Parent = parent, Sort = int.MaxValue };
                    string key = tables.TitleKey(parent);
                    tier.Title = OpenContent.Str(key, choice.Entry.Activity).ToUpperInvariant();
                    tier.Description = OpenContent.Str("STR_SERIES_DESCRIPTION_" + parent, "");
                    if (products.TryGetValue(parent, out var prow))
                    {
                        tier.Art = OpenContent.Icon(FeTables.I(prow, "IconId"));
                        int brand = FeTables.I(prow, "BrandId");
                        if (brand > 0 && brandIcon.TryGetValue(brand, out var icon)) tier.Brand = OpenContent.Icon(icon);
                    }
                    byParent[parent] = tier;
                }
                tier.Sort = Math.Min(tier.Sort, choice.Entry.SortValue);
                tier.Events.Add(new Ev { Choice = choice, Tier = tier });
            }
            var list = byParent.Values.ToList();
            foreach (var t in list)
            {
                t.Events.Sort((a, b) => a.Choice.Entry.SeriesId.CompareTo(b.Choice.Entry.SeriesId));
                int n = 0;
                foreach (var e in t.Events)
                {
                    e.Number = ++n;
                    var entry = e.Choice.Entry;
                    e.Title = EventTitle(entry);
                    e.Location = EventLocation(entry);
                }
                t.AllLocked = t.Events.All(e => e.Choice.Locked || !Navigable(e.Choice));   // (RaceAndRide: the parent product's IsLocked)
                t.AllGold = t.Events.Count > 0 && t.Events.All(e => e.Choice.BestPlace == 0);
            }
            list.Sort((a, b) => a.AllLocked != b.AllLocked ? (a.AllLocked ? 1 : -1) : a.Parent.CompareTo(b.Parent));   // the original: unlocked series first, then the series' order
            return list;
        }

        // ------------------------------------------------------------------------------------------------ building

        void BuildPoster(Tier tier)
        {
            var p = OuUi.Div("oc-poster oc-poster-dim", null);
            p.name = "career-tier-" + tier.Parent;
            if (tier.Art)
            {
                var art = OuUi.Div("oc-poster-art", p); art.style.backgroundImage = tier.Art; art.pickingMode = PickingMode.Ignore;
            }
            else
            {
                var card = OuUi.Div("oc-poster-card", p); card.pickingMode = PickingMode.Ignore;
                OuUi.Div("oc-poster-card-bar", card);
                OuUi.Text(tier.Title, "oc-poster-card-name", card).enableRichText = false;
            }
            tier.Badge = OuUi.Div("oc-poster-badge", p); tier.Badge.pickingMode = PickingMode.Ignore;
            var badge = tier.AllGold ? OcUi.Trophy(0) : tier.AllLocked ? OcUi.Lock : null;
            if (badge) { tier.Badge.style.backgroundImage = badge; if (!tier.AllGold) tier.Badge.style.unityBackgroundImageTintColor = new Color(0.88f, 0.88f, 0.78f, 0.9f); }
            OuUi.Div("oc-poster-focus", p).pickingMode = PickingMode.Ignore;
            p.focusable = true; p.pickingMode = PickingMode.Position; p.AddToClassList("mxa-item");
            p.userData = new OuNavItem { submit = () => Enter(tier) };
            p.RegisterCallback<FocusInEvent>(_ => { SetTier(tier.Index, true); ShowEvent(null); });
            p.RegisterCallback<ClickEvent>(e =>
            {
                if (e.button != 0) return;
                if (tier.Index == cur && nav.Focused == p) { OuSound.Play("Accept"); Enter(tier); return; }
                p.Focus();
            });
            tier.Poster = p;
        }

        void BuildList(Tier tier)
        {
            var list = new ScrollView(ScrollViewMode.Vertical) { name = "career-events-" + tier.Parent };
            list.AddToClassList("oc-list"); list.AddToClassList("oc-list-off");
            list.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            tier.List = list; eventsBody.Add(list);
            foreach (var ev in tier.Events)
            {
                var choice = ev.Choice; var entry = choice.Entry;
                var row = OuUi.Div("oc-event", list.contentContainer);
                OuUi.Item(row, () => select(choice));
                row.name = "career-series-" + entry.SeriesId;
                ev.Row = row;
                OcUi.Slot(row, ev.Number, choice.BestPlace, choice.Locked || !Navigable(choice), false, "oc-event-slot");
                var copy = OuUi.Div("oc-event-copy", row); copy.pickingMode = PickingMode.Ignore;
                OuUi.Text(ev.Title, "oc-event-title", copy).enableRichText = false;
                string laps = entry.Laps == 1 ? "LAP" : OpenContent.Str("STR_LAPS", "laps").ToUpperInvariant();
                OuUi.Text(entry.Laps > 0 ? ev.Location + "  ·  " + entry.Laps + " " + laps : ev.Location, "oc-event-sub", copy).enableRichText = false;
                var tags = OuUi.Div("oc-event-tags", row); tags.pickingMode = PickingMode.Ignore;
                if (!Navigable(choice)) OcUi.Tag(tags, "NOT AVAILABLE YET", "warn");
                else if (choice.Locked) OcUi.Tag(tags, OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED").ToUpperInvariant(), "warn");
                else if (!choice.Eligible) OcUi.Tag(tags, OpenContent.Str("STR_INVALID_VEHICLEREQ_TITLE", "INVALID VEHICLE").ToUpperInvariant(), "warn");
                else if (entry.Format != "SingleEvent") OcUi.Tag(tags, FormatName(choice).ToUpperInvariant());
                var trophy = choice.BestPlace >= 0 ? OcUi.Trophy(choice.BestPlace) : null;
                if (trophy) { var t = OuUi.Div("oc-event-trophy", tags); t.style.backgroundImage = trophy; t.pickingMode = PickingMode.Ignore; }
                row.SetEnabled(Navigable(choice));
                var scroll = list; var e = ev;
                row.RegisterCallback<FocusInEvent>(_ => { if (cur != e.Tier.Index) SetTier(e.Tier.Index, true); scroll.ScrollTo(row); ShowEvent(e); });
                foreach (var l in row.Query<Label>().ToList()) l.enableRichText = false;
            }
            if (tier.Events.Count == 0) OuUi.Text("", "oc-empty", list.contentContainer);
        }

        void Link()
        {
            for (int i = 0; i < tiers.Count; i++)
            {
                var t = tiers[i]; var item = (OuNavItem)t.Poster.userData;
                item.left = tiers[Math.Max(0, i - 1)].Poster; item.right = tiers[Math.Min(tiers.Count - 1, i + 1)].Poster;
                item.up = t.Poster; item.down = t.FirstSelectable?.Row ?? t.Poster;
                var rows = t.Events.Where(e => Navigable(e.Choice)).Select(e => e.Row).ToList();
                for (int r = 0; r < rows.Count; r++)
                {
                    var n = (OuNavItem)rows[r].userData;
                    n.up = r > 0 ? rows[r - 1] : rows[r]; n.down = r + 1 < rows.Count ? rows[r + 1] : rows[r];
                    n.left = t.Poster; n.right = rows[r];
                }
            }
            nav.OnCancel = () => { if (InEvents()) { tiers[cur].Poster.Focus(); return true; } back(); return true; };
            nav.OnTab = d => Step(d, !InEvents());
        }

        bool InEvents() { var f = nav.Focused; return f != null && f.ClassListContains("oc-event"); }

        // ------------------------------------------------------------------------------------------------ behaviour

        void Enter(Tier tier)
        {
            if (tier.AllLocked) { OuSound.Play("Locked"); if (Locked != null) Locked(); else Note.text = OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED") + "  ·  " + OpenContent.Str("STR_LOCKEDPOPUP_MESSAGE", ""); return; }   // RaceAndRide:OnAcceptSeries -> ShowLockedPopup
            var first = tier.FirstSelectable;
            if (first != null) { nav.Focus(first.Row); return; }
            OuSound.Play("Locked");
            Note.text = "NOT AVAILABLE YET";
        }

        // ------------------------------------------------------------------------------------------------ credits, vehicle list

        /// The player's credits in the top row (the Garage's balance; "ALL UNLOCKED" on the unlocked profile).
        public void RefreshCredits()
        {
            if (creditsValue == null) return;
            try { creditsValue.text = OpenGarageData.Unlocked ? "ALL UNLOCKED" : OpenGarageData.Balance; }
            catch (Exception e) { Debug.LogWarning("[Career] credits: " + e.Message); creditsValue.text = "-"; }
        }

        /// Focuses the event row of a series (after a vehicle swap the hub is read again and the focus goes back to where it was).
        public bool FocusSeries(int seriesId)
        {
            foreach (var t in tiers)
                foreach (var e in t.Events)
                    if (e.Choice.Entry.SeriesId == seriesId && Navigable(e.Choice)) { SetTier(t.Index, false); nav.Focus(e.Row); return true; }
            return false;
        }

        /// The focused event, null when a poster has the focus.
        public CareerEventChoice FocusedChoice => shown?.Choice;

        void DoQuickView()
        {
            var choice = shown?.Choice;
            if (choice == null || QuickView == null || CanQuickView == null || !CanQuickView(choice)) return;
            OuSound.Play("Accept");
            QuickView(choice);
        }

        /// The legend again with or without VEHICLE LIST (its own button cap, clickable like the others).
        void SyncQuickLegend()
        {
            if (legendRow == null) return;
            bool on = shown != null && CanQuickView != null && QuickView != null && CanQuickView(shown.Choice);
            if (on == quickShown && legendRow.childCount > 0) return;
            quickShown = on;
            var keys = new List<(string key, string label)> { (OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant()), (OuUi.KeyTabs, "SERIES") };
            if (on) keys.Add((OuUi.KeyAlt, OpenContent.Str("STR_QUICKVIEW", "VEHICLE LIST").ToUpperInvariant()));
            OuUi.Hints(legendRow, keys.ToArray());
            int n = 0;
            foreach (var e in legendRow.Children())
            {
                int i = n++;
                Action click = i == 1 ? back : i == 3 ? (Action)DoQuickView : null;
                if (click == null) { e.pickingMode = PickingMode.Ignore; continue; }
                e.pickingMode = PickingMode.Position; e.AddToClassList("oc-legend-click"); if (i == 3) e.name = "career-legend-vehicles";
                e.RegisterCallback<ClickEvent>(ev => { if (ev.button != 0) return; OuSound.Play("Back"); click(); });
            }
        }

        /// LB / RB, the arrows: the neighbouring tier; the focus follows (a poster, or the new tier's first event when events had it).
        void Step(int d, bool posterFocus)
        {
            if (tiers.Count == 0) return;
            int i = Mathf.Clamp(cur + d, 0, tiers.Count - 1);
            if (i == cur) return;
            var t = tiers[i];
            if (posterFocus || t.FirstSelectable == null) t.Poster.Focus(); else nav.Focus(t.FirstSelectable.Row);
        }

        void SetTier(int i, bool animate)
        {
            if (tiers.Count == 0) return;
            i = Mathf.Clamp(i, 0, tiers.Count - 1);
            bool changed = i != cur || tiers[i].List.ClassListContains("oc-list-off");
            cur = i;
            ApplyCarousel();
            if (!changed) return;
            foreach (var t in tiers)
            {
                bool off = t.Index != cur;
                if (off) { t.List.AddToClassList("oc-list-off"); continue; }
                t.List.style.opacity = 0.02f; t.List.RemoveFromClassList("oc-list-off");
                var list = t.List; list.schedule.Execute(() => list.style.opacity = 1f).ExecuteLater(20);
            }
            BuildInfoTier(tiers[cur]);
            BuildSlots(tiers[cur], null);
            BuildRide(tiers[cur].Events.Select(e => e.Choice.Entry.VehicleClass));
            if (shown == null || shown.Tier != tiers[cur]) ShowEvent(null);
        }

        static readonly float[] Tilt = { -3.4f, 2.6f, -1.8f, 3.9f, -2.7f, 1.4f, -3.9f, 2.2f, -1.1f };

        void ApplyCarousel()
        {
            foreach (var t in tiers)
            {
                int o = t.Index - cur, a = Mathf.Abs(o);
                var p = t.Poster;
                float scale = o == 0 ? 1f : a == 1 ? 0.76f : a == 2 ? 0.6f : 0.5f;
                float x = o == 0 ? 0 : Mathf.Sign(o) * (282f + (a - 1) * 128f);
                float y = o == 0 ? 0 : ((t.Index * 37) % 5 - 2) * 11f;
                float rot = o == 0 ? -1.2f : Tilt[t.Index % Tilt.Length] + Mathf.Sign(o) * 1.5f;
                float alpha = o == 0 ? 1f : a == 1 ? 0.9f : a == 2 ? 0.6f : a == 3 ? 0.28f : 0f;
                p.style.translate = new Translate(x, y);
                p.style.scale = new Scale(new Vector3(scale, scale, 1f));
                p.style.rotate = new Rotate(new Angle(rot, AngleUnit.Degree));
                p.style.opacity = alpha;
                p.EnableInClassList("oc-poster-dim", o != 0);
                p.EnableInClassList("oc-poster-on", o == 0);
                p.pickingMode = a <= 3 ? PickingMode.Position : PickingMode.Ignore;
                if (o == 0) p.BringToFront();
            }
            leftArrow.EnableInClassList("oc-arrow-off", cur <= 0);
            rightArrow.EnableInClassList("oc-arrow-off", cur >= tiers.Count - 1);
            leftArrow.BringToFront(); rightArrow.BringToFront();
        }

        // ------------------------------------------------------------------------------------------------ the information panel

        void BuildInfoTier(Tier tier)
        {
            infoTierPage.Clear();
            var desc = OuUi.Text(tier.Description, "oc-desc", infoTierPage); desc.enableRichText = false;
            if (tier.Brand) { var b = OuUi.Div("oc-brand", infoTierPage); b.style.backgroundImage = tier.Brand; b.pickingMode = PickingMode.Ignore; }
            var foot = OuUi.Div("oc-info-foot", infoTierPage); foot.pickingMode = PickingMode.Ignore;
            OcUi.Tag(foot, tier.Events.Count + (tier.Events.Count == 1 ? " EVENT" : " EVENTS"));
            if (tier.AllGold) OcUi.Tag(foot, "COMPLETE", "good");
            else if (tier.Medals > 0) OcUi.Tag(foot, tier.Medals + " / " + tier.Events.Count + " PODIUM", "orange");
            if (tier.AllLocked) OcUi.Tag(foot, OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED").ToUpperInvariant(), "warn");
        }

        /// The event panel (the focused row), or the tier's own text (null).
        void ShowEvent(Ev ev)
        {
            if (tiers.Count == 0) return;
            if (ev != null && ev == shown && !infoEventPage.ClassListContains("oc-info-off")) return;
            shown = ev;
            SyncQuickLegend();
            if (ev == null)
            {
                infoTitle.text = tiers[cur].Title;
                infoTierPage.RemoveFromClassList("oc-info-off"); infoEventPage.AddToClassList("oc-info-off");
                BuildSlots(tiers[cur], null);
                BuildRide(tiers[cur].Events.Select(e => e.Choice.Entry.VehicleClass));
                return;
            }
            infoTitle.text = ev.Title.ToUpperInvariant();
            BuildInfoEvent(ev);
            infoTierPage.AddToClassList("oc-info-off"); infoEventPage.RemoveFromClassList("oc-info-off");
            BuildSlots(ev.Tier, ev);
            BuildRide(new[] { ev.Choice.Entry.VehicleClass });
        }

        /// The event's name and its place, as the rows and the information panel (and the Start Race popup) show them.
        internal static string EventTitle(CareerEventEntry entry) => OpenContent.Str(FeTables.Shared.LocalizationKey(entry.ProductLocalizationId), entry.Activity.ToUpperInvariant());
        internal static string EventLocation(CareerEventEntry entry) => OpenContent.Str(FeTables.Shared.LocalizationKey(entry.TrackLocalizationId), entry.TrackAsset);

        internal static string FormatName(CareerEventChoice choice)
        {
            var entry = choice.Entry;
            string key = entry.AiFormat == "Rival" ? "STR_MXSERIESAIFORMAT_RIVAL" : "STR_MXSERIESFORMAT_" + (entry.Format ?? "").ToUpperInvariant();
            return OpenContent.Str(key, entry.Format ?? "");
        }

        void BuildInfoEvent(Ev ev)
        {
            infoEventPage.Clear();
            var entry = ev.Choice.Entry;
            var row = OuUi.Div("oc-info-row", infoEventPage); row.pickingMode = PickingMode.Ignore;
            var photo = OuUi.Div("oc-photo", row); photo.pickingMode = PickingMode.Ignore;
            var tex = OpenContent.Icon(PreviewIcon(entry)); if (tex) photo.style.backgroundImage = tex;
            var facts = OuUi.Div("oc-facts", row); facts.pickingMode = PickingMode.Ignore;
            OcUi.Fact(facts, OpenContent.Str("STR_EVENT_FORMAT", "Event Format").ToUpperInvariant(), FormatName(ev.Choice));
            string laps = entry.Laps == 1 ? "LAP" : OpenContent.Str("STR_LAPS", "laps").ToUpperInvariant();
            if (entry.Laps > 0) OcUi.Fact(facts, OpenContent.Str("STR_EVENT_DURATION", "Event Duration").ToUpperInvariant(), entry.Laps + " " + laps);
            OcUi.Fact(facts, "VEHICLE CLASS", OpenContent.ClassName(entry.VehicleClass));
            var place = OuUi.Text(ev.Location, "oc-photo-cap", photo); place.enableRichText = false;
            var foot = OuUi.Div("oc-info-foot", infoEventPage); foot.pickingMode = PickingMode.Ignore;
            OcUi.Tag(foot, entry.Riders + " RIDERS");
            if (ev.Choice.BestPlace >= 0) OcUi.Tag(foot, ev.Choice.BestPlace == 0 ? "GOLD" : ev.Choice.BestPlace == 1 ? "SILVER" : "BRONZE", "orange");
            if (!Navigable(ev.Choice)) OcUi.Tag(foot, "NOT AVAILABLE YET", "warn");
            else if (ev.Choice.Locked) OcUi.Tag(foot, OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED").ToUpperInvariant(), "warn");
            else if (!ev.Choice.Eligible) OcUi.Tag(foot, OpenContent.Str("STR_INVALID_VEHICLEREQ_TITLE", "INVALID VEHICLE").ToUpperInvariant(), "warn");
        }

        /// EventTypeLocations.PreviewIconId of the event's activity, location and track (the original's location photo); -1 when none.
        static int PreviewIcon(CareerEventEntry entry)
        {
            foreach (var r in FeTables.Shared.Rows("EventTypeLocations"))
                if (FeTables.I(r, "EventTypeId") == entry.EventTypeId && FeTables.I(r, "EventLocationId") == entry.LocationId && FeTables.I(r, "TrackId") == entry.TrackId)
                    return FeTables.I(r, "PreviewIconId");
            return -1;
        }

        /// The progress squares of the events header (medal colour, lock, the number of an event still to win), the focused event ringed.
        void BuildSlots(Tier tier, Ev here)
        {
            slots.Clear();
            int total = Math.Max(8, tier.Events.Count);
            for (int i = 0; i < total; i++)
            {
                if (i >= tier.Events.Count) { OcUi.Slot(slots, 0, -1, false, true); continue; }
                var e = tier.Events[i];
                var s = OcUi.Slot(slots, e.Number, e.Choice.BestPlace, e.Choice.Locked || !Navigable(e.Choice));
                if (e == here) s.AddToClassList("oc-slot-here");
            }
        }

        /// The top-left plate, as the original's Race & Ride page (CareerPlayerInformation: the equipped vehicle's name and its type icon): the
        /// vehicle the player rides in the class of what is selected - the equipped one when it fits, else the favourite one owned (the garage's
        /// data) - with the silhouette of its type. With none owned (or no profile) the class name and the silhouettes of the class's vehicle types.
        void BuildRide(IEnumerable<int> classes)
        {
            var ids = classes.Distinct().ToList();
            (string title, int type, bool equipped)? own = null;
            try { own = OpenGarageData.VehicleOfClasses(ids); } catch (Exception e) { Debug.LogWarning("[Career] equipped vehicle: " + e.Message); }
            var kinds = new List<string>();
            if (own.HasValue)
            {
                rideCap.text = own.Value.equipped ? "YOUR VEHICLE" : "OWNED  ·  EQUIP IN GARAGE";
                rideName.text = own.Value.title;
                string k = OcUi.SilhouetteOf(own.Value.type); if (k != null) kinds.Add(k);
            }
            else
            {
                var names = ids.Select(OpenContent.ClassName).Distinct().Take(2).ToList();
                rideCap.text = "VEHICLE CLASS";
                rideName.text = names.Count == 0 ? "" : string.Join("  /  ", names);
                foreach (int id in ids)
                    foreach (var r in FeTables.Shared.Rows("VehicleClassType").Where(r => FeTables.I(r, "VehicleClassId") == id))
                    {
                        string k = OcUi.SilhouetteOf(FeTables.I(r, "VehicleTypeId"));
                        if (k != null && !kinds.Contains(k)) kinds.Add(k);
                    }
            }
            var arts = new[] { rideArt, rideArt2 };
            for (int i = 0; i < arts.Length; i++)
            {
                var bg = i < kinds.Count ? OcUi.Silhouette(kinds[i]) : default;
                bool on = i < kinds.Count && (bg.sprite != null || bg.texture != null);
                arts[i].style.display = on ? DisplayStyle.Flex : DisplayStyle.None;
                if (on) arts[i].style.backgroundImage = bg;
            }
        }
    }
}
