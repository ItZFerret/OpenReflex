// The track browser: RACE / FREE RIDE's first screen (Resources/OpenFrontEnd/FrontEnd/Browser.uxml), after Open Unleashed's
// MxaMainMenu.Browser.cs, over Reflex's events (OpenContent: ReflexEvents with the original's names and pictures):
//   header    the screen title, the RACE / FREE RIDE switch and the search field (/ or Tab, Square / X): every word typed must appear in
//             the location's name or file name, the event type, the event key or "dlc"
//   filters   (left) ALL EVENTS, FAVORITES (the starred locations' events), RECENT (the events started last), CUSTOM TRACKS (Mods/Tracks:
//             Content/TrackMods.cs; always listed, empty it says how to install one, OPEN MODS FOLDER), DLC (when the install
//             has DLC events), then the event types in the original's order (EventSelect.lua) with their counts. L1 / R1 (Q / E)
//             step through the event types.
//   cards     (middle) the filter's events with their location pictures (PreviewIconId, loaded as they scroll into view, a few a
//             frame), GRID or LIST, sorted in GAME ORDER, A-Z or FAVORITES FIRST. Focus selects a card (the keys / pad; the mouse
//             selects on a click); Cross / Enter goes to the setup page; Triangle / Y (F) stars the location.
//   preview   (right) the location's picture with its track map (MapIconId) in the corner, the event type and the location, the
//             location's description, what the setup comes to and whether its world is built, SET UP EVENT (OpenShell.Setup.cs).
// PRACTICE (OpenBrowse.Practice, w-lb-practice): the same page over the lap races only (National, Supercross,
// Terracross, Powercross, UTV Moto; installed or custom), headed PRACTICE, without the RACE / FREE RIDE switch; a card's setup is the practice
// setup (OpenShell.Setup.cs: no duration, no AI), START launches the event as an endless lap race for one rider.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    /// The track browser's modes: RACE, FREE RIDE and PRACTICE (the lap races, w-lb-practice).
    public enum OpenBrowse { Race, FreeRide, Practice }

    public sealed partial class OpenShell
    {
        const string BrowseAll = "*all", BrowseFavourites = "*fav", BrowseRecent = "*recent", BrowseDlc = "*dlc", BrowseAlive = "*alive", BrowseRestored = "*restored", BrowseCustom = OpenContent.CustomGroup;
        static readonly string[] SortNames = { "GAME ORDER", "A – Z", "FAVORITES FIRST" };

        sealed class BrowserCard
        {
            public OpenEvent ev;
            public VisualElement row, thumb, star;
            public bool thumbDone, visible = true;
        }

        bool _freeRide;
        bool _practice;   // PRACTICE: the lap races, launched as practice sessions (OpenBrowse.Practice)
        string _browse;   // the filter: an activity, or BrowseAll / Favourites / Recent / Dlc
        string _query = "";
        OpenEvent _browseEvent;   // the selected card's event
        OpenSetup _setup;         // its setup
        readonly List<BrowserCard> _cards = new List<BrowserCard>();
        readonly Dictionary<string, VisualElement> _filterItems = new Dictionary<string, VisualElement>();
        readonly Dictionary<string, Label> _filterCounts = new Dictionary<string, Label>();
        VisualElement _modsChip;   // OPEN MODS FOLDER (the CUSTOM TRACKS filter)
        VisualElement _filterList, _tools, _empty, _searchRow, _favToggle, _sortChip, _viewChip, _trackList, _preview, _previewMap, _previewBody, _setupNext, _lastTrackItem;
        ScrollView _gridScroll, _filterScroll;
        TextField _search;
        Label _count, _eventBadge, _trackTitle, _trackSub;
        float _gridWidth;

        /// The browser's state (tests).
        public OpenEvent SelectedEvent => _browseEvent;
        public OpenSetup Setup => _setup;
        public bool FreeRide => _freeRide;
        public bool Practice => _practice;
        /// The browser's mode now (RACE, FREE RIDE or PRACTICE).
        public OpenBrowse BrowseMode => _practice ? OpenBrowse.Practice : _freeRide ? OpenBrowse.FreeRide : OpenBrowse.Race;
        /// PRACTICE lists an event: a lap race (RxPracticeMode.LapRace: National, Supercross, Terracross, Powercross, UTV Moto), not a challenge.
        public static bool PracticeEvent(OpenEvent e) => e?.ev != null && !e.freeRide && Reflex.Port.RxCore.RxPracticeMode.LapRace(e.activity) && string.IsNullOrEmpty(e.ev.spec.activation);
        public int VisibleCards => _cards.Count(c => c.visible);
        public IEnumerable<OpenEvent> CardEvents => _cards.Where(c => c.visible).Select(c => c.ev);

        IEnumerable<OpenEvent> ModeEvents => OpenContent.Events.Where(e => e.freeRide == _freeRide && (!_practice || PracticeEvent(e)));
        bool InMode(OpenEvent e) => e != null && e.freeRide == _freeRide && (!_practice || PracticeEvent(e));
        /// The filter codes of the event types in the order the page lists them.
        List<string> TypeCodes() => ModeTypes().Select(t => t.activity).ToList();
        /// The event types the page lists (PRACTICE: the lap races only).
        List<(string activity, string name, int count)> ModeTypes() => OpenContent.Types(_freeRide).Where(t => !_practice || Reflex.Port.RxCore.RxPracticeMode.LapRace(t.activity)).ToList();

        public void ShowBrowser(bool freeRide) => ShowBrowser(freeRide ? OpenBrowse.FreeRide : OpenBrowse.Race);

        /// RACE, FREE RIDE or PRACTICE's browser.
        public void ShowBrowser(OpenBrowse mode)
        {
            bool practice = mode == OpenBrowse.Practice;
            if (_practice != practice) { _browse = null; _browseEvent = null; _setup = null; }
            _practice = practice;
            ShowBrowserPage(mode == OpenBrowse.FreeRide);
        }

        void ShowBrowserPage(bool freeRide)
        {
            bool fromOutside = Current != Screen.Browser && Current != Screen.Setup;
            if (_freeRide != freeRide || _browse == null) { _browse = null; _browseEvent = null; }
            _freeRide = freeRide;
            if (fromOutside) _query = "";
            if (fromOutside) TrackMods.Refresh();   // MODS/TRACKS: tracks dropped into Mods/Tracks since the last visit
            if (_browseEvent != null && !InMode(_browseEvent)) _browseEvent = null;
            if (_browseEvent == null)
            {   // the last started event of the mode, else its first
                var last = OpenHistory.Recent().Select(r => OpenContent.Find(r.eventKey)).FirstOrDefault(e => e != null && InMode(e));
                _browseEvent = last ?? ModeEvents.FirstOrDefault();
            }
            if (_browse == null || (_browse[0] != '*' && !ModeEvents.Any(e => e.Group == _browse))) _browse = _browseEvent?.Group ?? BrowseAll;
            Clear(Screen.Browser);
            _layer.AddToClassList("mxa-dim");
            MusicTier(_freeRide ? 5 : 3);   // EventSelect:Start (tier 3); free ride goes straight to its locations (LocationSelect: 5)

            var right = OuTheme.Header(_layer, _practice ? "PRACTICE" : freeRide ? "FREE RIDE" : "RACE",
                _practice ? "PICK A TRACK  ·  UNLIMITED LAPS, YOU ALONE, YOUR LAP TIMES" : freeRide ? "PICK A LOCATION  ·  THE WHOLE MAP, NO LAPS, NO RIVALS" : "PICK AN EVENT AND A LOCATION");
            if (!_practice)
            {
                var seg = OuUi.Div("ou-seg", right);
                SegItem(seg, "RACE", !freeRide, () => SwitchMode(false));
                SegItem(seg, "FREE RIDE", freeRide, () => SwitchMode(true));
            }
            BuildSearch(right);

            var browser = OuTheme.Clone("Browser", _layer) ?? OuUi.Div("ou-browser", _layer);
            _filterScroll = browser.Q<ScrollView>("browser-filters") ?? AddScroll(OuTheme.Part(browser, "browser-sidebar", "ou-panel ou-sidebar"), "ou-filter-scroll");
            _filterScroll.verticalScrollerVisibility = ScrollerVisibility.Auto;
            _filterScroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _filterList = _filterScroll.contentContainer;
            _count = browser.Q<Label>("browser-count") ?? OuUi.Text("", "ou-count", browser);
            _tools = OuTheme.Part(browser, "browser-tools", "ou-tools");
            _gridScroll = browser.Q<ScrollView>("browser-scroll") ?? AddScroll(OuTheme.Part(browser, "browser-results", "ou-results"), "ou-grid-scroll");
            _gridScroll.verticalScrollerVisibility = ScrollerVisibility.AlwaysVisible;
            _gridScroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _trackList = _gridScroll.contentContainer;
            _trackList.AddToClassList("ou-grid");
            _trackList.RegisterCallback<GeometryChangedEvent>(_ => { if (Mathf.Abs(_trackList.contentRect.width - 1f - _gridWidth) > 0.5f) LayoutCards(); });
            _empty = OuTheme.Part(browser, "browser-empty", "ou-empty");

            _sortChip = OuUi.Button("SORT  " + SortNames[OpenHistory.Sort], "ou-chip", () => { OpenHistory.Sort = (OpenHistory.Sort + 1) % SortNames.Length; FillCards(); _nav.Focus(_sortChip); }, _tools);
            _sortChip.name = "ou-sort";
            _viewChip = OuUi.Button(OpenHistory.View == 0 ? "GRID" : "LIST", "ou-chip", () => { OpenHistory.View = 1 - OpenHistory.View; ApplyView(); _nav.Focus(_viewChip); }, _tools);
            _viewChip.name = "ou-view";
            _modsChip = OuUi.Button("OPEN MODS FOLDER", "ou-chip", OpenTracksFolder, _tools);   // (shown on CUSTOM TRACKS)
            _modsChip.name = "ou-open-mods";
            if (freeRide) BrowserChallengesChip(_tools);   // the Race & Ride challenges page (OpenShell.Challenges.cs)

            _preview = OuTheme.Part(browser, "detail-shot", "ou-detail-shot");
            _previewMap = _preview.Q<VisualElement>("detail-map") ?? OuUi.Div("ou-detail-map", _preview);
            var caption = OuTheme.Part(browser, "detail-caption", "ou-detail-caption");
            var badges = OuUi.Div("ou-detail-badges", caption);
            _eventBadge = OuUi.Text("", "ou-badge", badges);
            OuUi.Div("ou-spacer", badges);
            _favToggle = OuUi.Button("FAVORITE", "ou-chip ou-fav-toggle", ToggleFavourite, badges);
            _favToggle.name = "ou-fav-toggle";
            var favStar = new OuStar(); favStar.AddToClassList("ou-fav-star"); _favToggle.Insert(0, favStar);
            _trackTitle = OuUi.Text("", "mxa-track-title ou-detail-title", caption);
            _trackSub = OuUi.Text("", "mxa-track-sub ou-detail-sub", caption);
            var body = browser.Q<ScrollView>("detail-body") ?? AddScroll(OuTheme.Part(browser, "browser-detail", "ou-panel ou-detail"), "ou-detail-body");
            body.verticalScrollerVisibility = ScrollerVisibility.Auto;
            body.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            _previewBody = body.contentContainer;
            _setupNext = OuUi.Button(_practice ? "SET UP PRACTICE  ›" : freeRide ? "SET UP RIDE  ›" : "SET UP EVENT  ›", "mxa-start ou-cta", ShowSetupPage, OuTheme.Part(browser, "detail-actions", "ou-detail-actions"));
            _setupNext.name = "ou-setup-next";
            OuTheme.Backdrop(_setupNext);

            BuildFilters();
            FillCards();
            _nav.OnTab = d =>
            {
                var types = TypeCodes();
                if (types.Count == 0) return;
                int i = types.IndexOf(_browse);
                SelectBrowse(types[((i < 0 ? (d > 0 ? -1 : 0) : i) + d + types.Count) % types.Count]);
            };
            _nav.OnCancel = () =>
            {
                var f = _nav.Focused;
                if (f != null && !_trackList.Contains(f) && _lastTrackItem != null && _lastTrackItem.panel != null) { _nav.Focus(_lastTrackItem); return true; }
                ShowHome();
                return true;
            };
            _nav.OnAlt = ToggleFavourite;
            _nav.OnFind = FocusSearch;
            _nav.DefaultItem = () => _lastTrackItem;
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"), (OuUi.KeyTabs, "EVENT TYPE"), (OuUi.KeyAlt, "FAVORITE"), (OuUi.KeyFind, "SEARCH"));
        }

        static ScrollView AddScroll(VisualElement parent, string cls)
        {
            var sv = new ScrollView(ScrollViewMode.Vertical);
            foreach (var c in cls.Split(' ')) sv.AddToClassList(c);
            parent.Add(sv);
            return sv;
        }

        void ReleaseBrowser()
        {
            _previewWant = null;
            _previewBody = null; _setupNext = null; _gridScroll = null; _filterScroll = null; _search = null; _gridWidth = 0;
            _cards.Clear(); _filterItems.Clear(); _filterCounts.Clear();
            ReleaseSetupPage();
        }

        void SwitchMode(bool freeRide)
        {
            if (_freeRide == freeRide) return;
            ShowBrowser(freeRide);
            var seg = _layer.Query<VisualElement>(className: "ou-seg-item").ToList();
            if (seg.Count == 2) _nav.Focus(seg[freeRide ? 1 : 0]);
        }

        void SegItem(VisualElement seg, string label, bool on, Action submit)
        {
            var b = OuUi.Button(label, "ou-seg-item", submit, seg);
            b.name = "ou-seg-" + (label == "RACE" ? "race" : "freeride");
            b.EnableInClassList("ou-seg-on", on);
        }

        // ------------------------------------------------------------------------------------------------ search

        void BuildSearch(VisualElement parent)
        {
            _searchRow = OuUi.Div("ou-search", parent);
            _searchRow.name = "ou-search";
            var icon = OuUi.Div("ou-search-icon", _searchRow);
            OuUi.Div("ou-search-lens", icon);
            OuUi.Div("ou-search-handle", icon);
            _search = new TextField { maxLength = 48, value = _query };
            _search.AddToClassList("mxa-path");
            _search.AddToClassList("ou-search-field");
            _search.textEdition.placeholder = "SEARCH EVENTS";
            _search.textEdition.hidePlaceholderOnFocus = true;
            _searchRow.Add(_search);
            OuUi.Text(OuUi.KeyFind, "mxa-key ou-search-key", _searchRow);
            _search.RegisterValueChangedCallback(e => { _query = e.newValue ?? ""; ApplyFilter(); });
            _search.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter || e.keyCode == KeyCode.DownArrow)
                {
                    var first = _cards.Find(c => c.visible);
                    if (first != null) _nav.Focus(first.row); else _searchRow.Focus();
                    e.StopPropagation();
                }
                else if (e.keyCode == KeyCode.Escape)
                {
                    if (!string.IsNullOrEmpty(_search.value)) _search.value = "";
                    else _searchRow.Focus();
                    e.StopPropagation();
                }
            }, TrickleDown.TrickleDown);
            OuUi.Item(_searchRow, FocusSearch);
        }

        void FocusSearch()
        {
            if (_search == null) return;
            _searchRow.Focus();
            _search.Q("unity-text-input")?.Focus();
        }

        /// The search text (tests: as typed).
        public void SetQuery(string q) { if (_search != null) _search.value = q ?? ""; else { _query = q ?? ""; ApplyFilter(); } }

        // ------------------------------------------------------------------------------------------------ filters

        void BuildFilters()
        {
            _filterList.Clear(); _filterItems.Clear(); _filterCounts.Clear();
            FilterItem(BrowseAll, "ALL EVENTS");
            FilterItem(BrowseFavourites, "FAVORITES");
            FilterItem(BrowseRecent, "RECENT");
            if (ModeEvents.Any(e => e.dlc)) FilterItem(BrowseDlc, "DOWNLOADED");
            if (ModeEvents.Any(e => e.Alive)) FilterItem(BrowseAlive, "MX vs ATV Alive");   // Free Ride: the Alive maps; race: their AI races (w-aliveai)
            if (ModeEvents.Any(e => e.Restored)) FilterItem(BrowseRestored, "RESTORED");   // the levels cut from the original game (w-fresnob)
            FilterItem(BrowseCustom, "CUSTOM TRACKS");   // MODS/TRACKS: always there (empty: how to install one)
            var types = ModeTypes().Select(t => (code: t.activity, t.name)).ToList();
            if (types.Count > 0)
            {
                OuUi.Text(_freeRide ? "LOCATIONS" : "EVENT TYPES", "ou-filter-group", _filterList);
                foreach (var t in types) FilterItem(t.code, t.name);
            }
            RefreshCounts();
            HighlightFilter();
        }

        void FilterItem(string code, string name)
        {
            var it = OuUi.Button(name, "ou-filter", () => SelectBrowse(code), _filterList);
            it.name = "ou-filter-" + code.TrimStart('*').ToLowerInvariant();
            _filterCounts[code] = OuUi.Text("", "ou-filter-count", it);
            _filterItems[code] = it;
        }

        void RefreshCounts()
        {
            foreach (var kv in _filterCounts) kv.Value.text = Entries(kv.Key).Count.ToString();
        }

        void HighlightFilter() { foreach (var kv in _filterItems) kv.Value.EnableInClassList("ou-filter-on", kv.Key == _browse); }

        /// A filter (tests too).
        public void SelectBrowse(string code)
        {
            if (code == BrowseCustom && TrackMods.Refresh()) { _browse = code; BuildFilters(); }   // a rescan of Mods/Tracks (cheap: only a change re-reads)
            _browse = code;
            HighlightFilter();
            FillCards();
            if (_filterItems.TryGetValue(code, out var item)) _filterScroll?.ScrollTo(item);
        }
        public string Browse => _browse;

        // ------------------------------------------------------------------------------------------------ cards

        List<OpenEvent> Entries(string code)
        {
            var mode = ModeEvents.ToList();
            switch (code)
            {
                case BrowseAll: return mode;
                case BrowseFavourites: return mode.Where(e => OpenHistory.IsFavourite(e.Location)).ToList();
                case BrowseDlc: return mode.Where(e => e.dlc).ToList();
                case BrowseAlive: return mode.Where(e => e.Alive).ToList();
                case BrowseRestored: return mode.Where(e => e.Restored).ToList();
                case BrowseRecent:
                    return OpenHistory.Recent().Select(r => OpenContent.Find(r.eventKey)).Where(e => e != null && InMode(e)).ToList();
                default: return mode.Where(e => e.Group == code).ToList();   // an event type (Free Ride: the race type of the level, OpenEvent.Group)
            }
        }

        List<OpenEvent> BrowseEntries()
        {
            var list = Entries(_browse);
            if (_browse == BrowseRecent) return list;   // newest first, whatever the sort
            var order = list.Select((x, i) => (x, i)).ToList();
            if (OpenHistory.Sort == 1) order = order.OrderBy(o => o.x.locationName, StringComparer.OrdinalIgnoreCase).ThenBy(o => o.i).ToList();
            else if (OpenHistory.Sort == 2) order = order.OrderBy(o => OpenHistory.IsFavourite(o.x.Location) ? 0 : 1).ThenBy(o => o.i).ToList();
            return order.Select(o => o.x).ToList();
        }

        void FillCards()
        {
            _trackList.Clear(); _cards.Clear();
            _lastTrackItem = null; _gridWidth = 0;
            _sortChip.Q<Label>().text = "SORT  " + SortNames[OpenHistory.Sort];
            _sortChip.SetEnabled(_browse != BrowseRecent);
            if (_modsChip != null) _modsChip.style.display = _browse == BrowseCustom ? DisplayStyle.Flex : DisplayStyle.None;
            var list = BrowseEntries();
            bool mixed = _browse[0] == '*';
            BrowserCard cur = null;
            foreach (var ev in list)
            {
                var c = new BrowserCard { ev = ev };
                BuildCard(c, mixed);
                _cards.Add(c);
                if (cur == null && ev == _browseEvent) cur = c;
            }
            cur ??= _cards.Count > 0 ? _cards[0] : null;
            ApplyView();
            if (cur != null) { SelectCard(cur); _lastTrackItem = cur.row; }
            else ShowNothingSelected();
            ApplyFilter();
            _nav.Focus(_lastTrackItem);
        }

        void BuildCard(BrowserCard c, bool mixed)
        {
            var e = c.ev;
            var row = OuUi.Div("ou-card", _trackList);
            row.tooltip = e.key;
            // the caption first (FocusedText = the location's name); column-reverse draws the picture above it
            var cap = OuUi.Div("ou-card-caption", row);
            OuUi.Text(e.locationName, "mxa-track-name ou-card-name", cap);
            string sub = (mixed ? e.typeName + "  ·  " : "") + (e.ev.spec.trackId > 0 ? "TRACK " + (e.ev.spec.trackId + 1) + "  ·  " : "") + e.Where;
            OuUi.Text(sub, "ou-card-sub", cap);
            var thumb = OuUi.Div("ou-card-thumb", row);
            if (mixed) OuUi.Text(e.typeName, "ou-badge ou-card-badge", thumb);
            if (e.dlc) OuUi.Text("DLC", "ou-badge ou-card-badge ou-card-dlc", thumb);
            else if (e.Restored) OuUi.Text("RESTORED", "ou-badge ou-card-badge ou-card-dlc", thumb);
            if (e.Custom) OuUi.Text(e.Broken ? (e.mod.archive ? OpenEvent.ArchiveBadge(e.mod) : "BROKEN") : "CUSTOM", "ou-badge ou-card-badge ou-card-dlc", thumb);
            var star = new OuStar(); star.AddToClassList("ou-card-star"); thumb.Add(star);
            c.row = row; c.thumb = thumb; c.star = star;
            star.style.display = OpenHistory.IsFavourite(e.Location) ? DisplayStyle.Flex : DisplayStyle.None;
            OuUi.Item(row, () => { SelectCard(c); ShowSetupPage(); });
            // the keys / pad select as they move; the mouse only highlights on its way past: a click selects
            row.RegisterCallback<FocusInEvent>(_ => { if (!OuUi.FocusFromHover(row)) SelectCard(c); });
        }

        void SelectCard(BrowserCard c)
        {
            if (c?.ev == null) return;
            using var perf = Platform.ReflexMenuPerf.Measure("browser card selected");
            bool changed = _browseEvent != c.ev || _setup == null || _setup.eventKey != c.ev.key || _setup.practice != _practice;
            _browseEvent = c.ev;
            if (changed) _setup = _practice ? OpenHistory.PracticeSetupOf(c.ev.key) : OpenHistory.SetupOf(c.ev.key);
            foreach (var x in _cards) { bool on = x == c; x.row.EnableInClassList("ou-card-on", on); if (on) _lastTrackItem = x.row; }
            _empty.style.display = DisplayStyle.None;
            _previewWant = c.ev; _previewCleared = false; ShowPreviewLater();   // (the pictures read on a worker the first time: UpdateBrowser shows them when they are there)
            BoardsShow(c.ev, _setup);   // (the compound's status boards show the event under the cursor, as the original's do)
            _eventBadge.text = c.ev.typeName;
            _trackTitle.text = c.ev.locationName;
            _trackSub.text = c.ev.Label + "  ·  " + c.ev.Where + (c.ev.dlc ? "  ·  DLC" : "") + (c.ev.Restored ? "  ·  RESTORED" : "");
            UpdateFavouriteToggle();
            _setupNext?.SetEnabled(!c.ev.Broken);   // a broken custom track says why and is never started
            if (changed || _previewBody.childCount == 0) RefreshBrowserPreview();
        }

        /// A card by event key (tests: as the pad would focus it).
        public bool SelectEvent(string key)
        {
            var c = _cards.Find(x => string.Equals(x.ev.key, key, StringComparison.OrdinalIgnoreCase));
            if (c == null) return false;
            _nav.Focus(c.row); SelectCard(c);
            return true;
        }

        void RefreshBrowserPreview()
        {
            if (_previewBody == null || _browseEvent == null) return;
            _previewBody.Clear();
            if (!string.IsNullOrEmpty(_browseEvent.description)) OuUi.Text(_browseEvent.description, "mxa-note ou-detail-text", _previewBody);
            else if (!string.IsNullOrEmpty(_browseEvent.typeDescription)) OuUi.Text(_browseEvent.typeDescription, "mxa-note ou-detail-text", _previewBody);
            if (_browseEvent.Custom)
            {   // MODS/TRACKS: where the track lives, and the way to its folder
                var m = _browseEvent.mod;
                if (m.Ok) OuUi.Text("CUSTOM TRACK  ·  " + m.kindLabel + (string.IsNullOrEmpty(m.author) ? "" : "  ·  BY " + m.author.ToUpperInvariant()), "mxa-note ou-detail-text", _previewBody);   // (a broken one: its description says why)
                OuUi.Text("Mods/Tracks/" + m.id + (m.Ok ? "  ·  " + m.level + "  ·  " + OpenWorldCache.SizeText(m.bytes) : "") + "  ·  offline only, no records", "mxa-note ou-detail-text ou-faint", _previewBody);
                var open = OuUi.Button("OPEN MODS FOLDER", "ou-chip", OpenTracksFolder, _previewBody); open.name = "ou-open-mods-detail";
                if (_browseEvent.Broken) return;
            }
            var sum = OuUi.Div("ou-setup-summary ou-setup-summary-small", _previewBody);
            FillSummary(sum, _browseEvent, _setup);
        }

        /// MODS/TRACKS: Mods/Tracks in the desktop's file browser (made with its README when missing).
        static void OpenTracksFolder() => TrackMods.OpenFolder();

        void ShowNothingSelected()
        {
            _preview.style.backgroundImage = new StyleBackground(StyleKeyword.None);
            _previewMap.style.display = DisplayStyle.None;
            _eventBadge.text = "";
            _trackTitle.text = "NO EVENT SELECTED";
            _trackSub.text = "";
            UpdateFavouriteToggle();
            _setupNext?.SetEnabled(false);
            _previewBody?.Clear();
        }

        void ShowEmptyState()
        {
            _empty.Clear();
            string title, text, action = null; Action act = null;
            if (_cards.Count > 0) { title = "NO MATCHES"; text = $"Nothing here matches “{_query.Trim()}”. Try another name, or clear the search."; action = "CLEAR SEARCH"; act = () => { _search.value = ""; _nav.Focus(_lastTrackItem); }; }
            else if (_browse == BrowseFavourites) { title = "NO FAVORITES YET"; text = $"Press {OuUi.KeyAlt} on an event, or FAVORITE beside its name, to keep its location here."; }
            else if (_browse == BrowseRecent) { title = "NOTHING RIDDEN YET"; text = (_freeRide ? "Free rides" : "Events") + " you start show up here, newest first."; }
            else if (_browse == BrowseCustom)
            {   // MODS/TRACKS: how to install one
                title = "NO CUSTOM TRACKS YET";
                text = "Put each track in a folder of its own in " + (TrackMods.Folder ?? "Mods/Tracks") + ": Mods/Tracks/<Track Name>/ with its .dx9 files (database, level, package, scene). "
                     + (_freeRide ? "Every custom track can be free-ridden." : "National and Supercross tracks race here; every track is free-ridden in FREE RIDE.")
                     + " A downloaded .zip / .rar / .7z can go in as it is: the game extracts it into a folder of its own. No beta slots: any number of tracks, the game's files untouched.";
                action = "OPEN MODS FOLDER"; act = OpenTracksFolder;
            }
            else { title = "NO EVENTS"; text = "Your install has no event of this kind that the port runs."; }
            OuTheme.Backdrop(OuUi.Div("ou-empty-mark", _empty));
            OuUi.Text(title, "ou-empty-title", _empty);
            OuUi.Text(text, "ou-empty-text", _empty);
            if (action != null)
            {
                var b = OuUi.Button(action, "mxa-start ou-cta ou-empty-action", act, _empty);
                OuTheme.Backdrop(b);
                if (_cards.Count == 0) _lastTrackItem = b;
            }
        }

        /// The search: hides the cards that do not match every word; the count and the empty state follow.
        void ApplyFilter()
        {
            if (_count == null) return;
            var words = (_query ?? "").ToLowerInvariant().Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            int shown = 0;
            foreach (var c in _cards)
            {
                c.visible = words.All(w => c.ev.haystack.Contains(w));
                c.row.style.display = c.visible ? DisplayStyle.Flex : DisplayStyle.None;
                if (c.visible) shown++;
            }
            string noun = _cards.Count == 1 ? "EVENT" : "EVENTS";
            _count.text = words.Length > 0 ? $"{shown} OF {_cards.Count} {noun}" : $"{_cards.Count} {noun}";
            bool empty = shown == 0;
            _empty.style.display = empty ? DisplayStyle.Flex : DisplayStyle.None;
            if (empty) ShowEmptyState();
            LayoutCards();
        }

        void ApplyView()
        {
            if (_gridScroll == null) return;
            bool list = OpenHistory.View == 1;
            _trackList.EnableInClassList("ou-view-list", list);
            _trackList.EnableInClassList("ou-view-grid", !list);
            _viewChip.Q<Label>().text = list ? "LIST" : "GRID";
            _gridWidth = 0;
            LayoutCards();
        }

        /// Grid: as many columns as fit (cards at least 220 px wide, 16 px apart), the pictures 16:9; list: full rows.
        void LayoutCards()
        {
            if (_gridScroll == null) return;
            float w = _trackList.contentRect.width - 1f;
            if (float.IsNaN(w) || w < 50f) return;
            _gridWidth = w;
            bool list = OpenHistory.View == 1;
            const float gap = 16f, minCard = 220f;
            int cols = Mathf.Max(1, Mathf.FloorToInt((w + gap) / (minCard + gap)));
            float cw = Mathf.Floor((w + gap) / cols - gap);
            int i = 0;
            foreach (var c in _cards)
            {
                if (!c.visible) continue;
                if (list) { c.row.style.width = StyleKeyword.Null; c.row.style.marginRight = StyleKeyword.Null; c.thumb.style.height = StyleKeyword.Null; }
                else
                {
                    c.row.style.width = cw;
                    c.row.style.marginRight = (i % cols) == cols - 1 ? 0f : gap;
                    c.thumb.style.height = Mathf.Round(cw * 9f / 16f);
                }
                i++;
            }
        }

        void UpdateBrowser()
        {
            if (_previewWant != null) ShowPreviewLater();
            LoadThumbnails();
            // (w-fb-customtrack) an archive's install ended (Content/TrackArchives.cs): the track it made, or why it could not, without leaving the page
            if (_archiveGen != TrackArchives.Generation)
            {
                _archiveGen = TrackArchives.Generation;
                if (TrackMods.Refresh())
                {
                    string key = _browseEvent?.key;
                    _browseEvent = key != null ? OpenContent.Find(key) : null;
                    if (_browseEvent == null && _browse == BrowseCustom) _browseEvent = ModeEvents.FirstOrDefault(e => e.Custom && !e.Broken);
                    BuildFilters(); FillCards();
                }
            }
        }
        int _archiveGen = -1;

        /// The preview's picture and track map of the selected card when they are read (w-menufps: never a wait on the main thread for them).
        OpenEvent _previewWant; bool _previewCleared;
        void ShowPreviewLater()
        {
            var ev = _previewWant;
            if (ev == null || _preview == null) { _previewWant = null; return; }
            var tex = OpenContent.PictureLater(ev, out bool p0);
            var map = OpenContent.MapPictureLater(ev, out bool p1);
            if (!p0)
            {
                _preview.style.backgroundImage = tex ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
                _preview.EnableInClassList("mxa-preview-empty", !tex);
            }
            else if (!_previewCleared) _preview.style.backgroundImage = new StyleBackground(StyleKeyword.None);   // (not the last card's picture under this card's name)
            if (_previewMap != null && (!p1 || !_previewCleared))
            {
                _previewMap.style.backgroundImage = !p1 && map ? new StyleBackground(map) : new StyleBackground(StyleKeyword.None);
                _previewMap.style.display = !p1 && map ? DisplayStyle.Flex : DisplayStyle.None;
            }
            _previewCleared = true;
            if (!p0 && !p1) _previewWant = null;
        }

        /// The pictures of the cards in (or near) view, a few a frame.
        void LoadThumbnails()
        {
            if (_gridScroll == null || _cards.Count == 0) return;
            Rect view = _gridScroll.contentViewport.worldBound;
            if (float.IsNaN(view.height) || view.height < 1f) return;
            view.yMin -= 240f; view.yMax += 480f;
            var watch = Stopwatch.StartNew();
            int loaded = 0;
            foreach (var c in _cards)
            {
                if (c.thumbDone || !c.visible) continue;
                Rect b = c.row.worldBound;
                if (float.IsNaN(b.height) || b.height < 1f || !b.Overlaps(view)) continue;
                var tex = OpenContent.PictureLater(c.ev, out bool pending);   // (read on a worker: the card gets it a few frames later, w-menufps)
                if (pending) continue;
                c.thumbDone = true;
                if (tex) c.thumb.style.backgroundImage = tex;
                c.thumb.EnableInClassList("ou-card-thumb-empty", !tex);
                if (++loaded >= 4 || watch.ElapsedMilliseconds > 12) break;
            }
        }

        /// Every card's picture loaded (tests / captures).
        public void LoadAllThumbnails() { foreach (var c in _cards) c.thumbDone = false; for (int k = 0; k < 64 && _cards.Any(c => c.visible && !c.thumbDone); k++) { foreach (var c in _cards) if (c.visible && !c.thumbDone) { c.thumbDone = true; var t = OpenContent.Picture(c.ev); if (t) c.thumb.style.backgroundImage = t; } } }

        // ------------------------------------------------------------------------------------------------ favourites

        void ToggleFavourite()
        {
            var f = _nav.Focused;
            var c = f != null ? _cards.Find(x => x.row == f) : null;
            var ev = c?.ev ?? _browseEvent;
            if (ev == null) return;
            bool on = OpenHistory.ToggleFavourite(ev.Location);
            foreach (var x in _cards) if (x.ev.Location == ev.Location) x.star.style.display = on ? DisplayStyle.Flex : DisplayStyle.None;
            UpdateFavouriteToggle();
            if (_filterCounts.TryGetValue(BrowseFavourites, out var l)) l.text = Entries(BrowseFavourites).Count.ToString();
        }

        void UpdateFavouriteToggle()
        {
            if (_favToggle == null) return;
            bool has = _browseEvent != null, on = has && OpenHistory.IsFavourite(_browseEvent.Location);
            _favToggle.style.visibility = has ? Visibility.Visible : Visibility.Hidden;
            _favToggle.EnableInClassList("ou-fav-on", on);
            _favToggle.Q<Label>().text = on ? "FAVORITE" : "ADD TO FAVORITES";
        }

        // ------------------------------------------------------------------------------------------------ preview

        static void ShowPreview(OpenEvent ev, VisualElement shot, VisualElement mapEl)
        {
            var tex = OpenContent.Picture(ev);
            shot.style.backgroundImage = tex ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
            shot.EnableInClassList("mxa-preview-empty", !tex);
            var map = OpenContent.MapPicture(ev);
            if (mapEl == null) return;
            mapEl.style.backgroundImage = map ? new StyleBackground(map) : new StyleBackground(StyleKeyword.None);
            mapEl.style.display = map ? DisplayStyle.Flex : DisplayStyle.None;
        }

        /// What the setup comes to: chips (event, field, duration, vehicle, class, rider, the world).
        void FillSummary(VisualElement sum, OpenEvent ev, OpenSetup s)
        {
            using var perf = Platform.ReflexMenuPerf.Measure("setup summary");
            sum.Clear();
            if (ev == null || s == null) return;
            var e = ev.ev;
            OuTheme.Chip(sum, "EVENT", ev.typeName, true);
            bool weekend = ChampFormatOn(ev);   // RESTORED CONTENT: the FORMAT row's CHAMPIONSHIP WEEKEND (OpenShell.ChampWeekend.cs)
            if (weekend) OuTheme.Chip(sum, "FORMAT", "CHAMPIONSHIP WEEKEND", true);
            if (s.practice)
            {   // PRACTICE: you alone, laps until you stop
                OuTheme.Chip(sum, "PRACTICE", "UNLIMITED LAPS  ·  SOLO", true);
            }
            else if (s.Rules(e))
            {
                int ai = s.Ai(e);
                OuTheme.Chip(sum, "FIELD", ai > 0 ? s.Riders(e) + " RIDERS" : "SOLO");   // RIDER CAP: the setup's RIDERS (12 = the full gate)
                OuTheme.Chip(sum, "AI", OpenContent.AiName(ai));
                var d = s.Durations(e, out bool laps, out string singular, out string plural); int di = s.DurationIndex(d);
                if (di >= 0 && !(weekend && Reflex.Port.RxCore.RxChampWeekend.DisciplineOf(ev.activity) <= Reflex.Port.RxCore.RxChampWeekend.Supercross)) OuTheme.Chip(sum, "DURATION", d[di] + " " + OpenContent.Str(d[di] > 1 ? plural : singular, laps ? (d[di] > 1 ? "LAPS" : "LAP") : "").ToUpperInvariant());
            }
            else OuTheme.Chip(sum, "RIDE", "NO LAPS  ·  NO RIVALS");
            OuTheme.Chip(sum, "VEHICLE", OpenContent.VehicleTypeName(s.Type(e)));
            int cls = s.Class(e); if (cls >= 0 && s.Rules(e)) OuTheme.Chip(sum, "CLASS", OpenContent.ClassName(cls));
            int rider = s.Rider(e);
            OuTheme.Chip(sum, "RIDER", OpenSetup.Riders(e, s.Type(e)).FirstOrDefault(r => r.id == rider).name ?? "YOUR RIDER");
            int ms = OpenSetup.MapScalePercent(e);
            if (ms != Reflex.Port.RxCore.RxMapScale.DefaultPercent) OuTheme.Chip(sum, "MAP SCALE", Reflex.Port.RxCore.RxMapScale.Label(ms) + "  ·  NO RECORDS", true);
            int wx = s.Weather; if (wx != 0) OuTheme.Chip(sum, "WEATHER", Reflex.Port.Rendering.ReflexWeather.Names[wx] + "  ·  " + Reflex.Port.Rendering.ReflexWeather.IntensityNames[s.WeatherIntensity], true);
            string world = WorldText(e, s);
            _summaryWorld = world;
            OuTheme.Chip(sum, "WORLD", world);
        }

        /// The WORLD chip's text: the event's built world and its size, BUILDING…, or FIRST RIDE BUILDS IT.
        static string WorldText(ReflexEvents.Event e, OpenSetup s)
        {
            using var perf = Platform.ReflexMenuPerf.Measure("world files check");
            try
            {
                var w = s.World(e);
                return w.Ready ? "BUILT  ·  " + OpenWorldCache.SizeText(OpenWorldCache.SizeOf(w)) : w.Building ? "BUILDING…" : "FIRST RIDE BUILDS IT";
            }
            catch (Exception) { return "-"; }
        }
        /// The WORLD chip the setup page's summary shows (its refresh every second rebuilds the chips only when this changed).
        string _summaryWorld;

        const int ReflexWorldFilesFull = Content.ReflexWorldFiles.FullGateOpponents;
    }
}
