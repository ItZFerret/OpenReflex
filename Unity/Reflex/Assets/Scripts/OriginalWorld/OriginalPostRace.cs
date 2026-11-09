using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The original post-race pages, shown from race state Post (4) in place of the HUD (InGamePageGraph: HUB -GotoPostEvent->
    // "PostEventResults" = PostEventResults.lyr + ButtonLegend.lyr, then -GotoPostEventMenuPage-> "PostEventPage" =
    // PostEventMenu.lyr + ButtonLegend.lyr; scripts PostEventResults.lua / PostEventMenu.lua):
    //   results   the table (PostEventResults.uicomponent at (152,177)): column titles in the orange bar, one row per rider
    //             (index, name, three columns; for a race TOTAL / BEST LAP / PENALTY = STR_ACTIVITYRESULTSCOLUMN_*), the legend's
    //             START button "CONTINUE" (STR_CONTINUE); Enter / F3 / the legend continue to the menu.
    //   menu      PostEventList: CONTINUE (STR_POSTEVENT_CONTINUE), RESTART (the Restart Event confirmation popup). HIGHLIGHT REEL
    //             (replay) and QUIT (online only) are not offered. CONTINUE is "back to the front end" in the game; the client has
    //             none, so it leaves like the pause menu's QUIT (OriginalPostRace.onContinue).
    // The rows come from the caller (SetRows): OriginalHud fills them from every rider's standing (reflex_world_race_drivers:
    // place, finishing time, best lap), as the game's PopulateResultsTable does from its standings.
    // MOTOCAREER (OriginalCareerRace): the menu's CONTINUE is PostEventMenu:SeriesContinue and RESTART is offered only when the series
    // allows it (SeriesAllowRestart); the table's column titles and rows are the game's own "Activity Results" (PopulateResultsTable);
    // two more pages of the InGame graph: "Moto Results Page" (PostEventMotoResults.lyr: the same table, PopulateSeriesMotoResultsTable,
    // the legend's accept "CONTINUE") and "Post Event Reward Page" (PostEventRewards.lyr: credits, series rewards, trophy).
    // Arcade (WP-7): the table is the game's own too (AskGameTable: App.PopulateResultsTable on the running world, as PostEventResults:Start
    // runs it), so the columns are the activity's (a race: TOTAL TIME, PENALTY TIME, BEST LAP TIME) as the game formats them; the local
    // player's name is the Steam persona the PC build signs in with (ReflexProfile.Persona) when one was entered. The original shows no
    // record / new-best mark on these pages: the table's ColumnN_Record cells (made by 004df240) are not read by PostEventResults.lua,
    // and the records the race set (PlayerRecords, PlayerHighlight) are saved when the race is left (ReflexPersistence) and shown by the
    // motocard's records / highlights pages.
    // Not ported: PostEventTimer / PostEventVote (online vote), the highlight reel.
    public sealed class OriginalPostRace
    {
        public struct Row { public int index; public string name, c0, c1, c2; }

        /// A post-race page is up. The open page's canvas, not a plain flag: the scene can unload with a page open (QUIT from the pause menu
        /// over the results), and a flag left set hid every later race's MODERN HUD, its replay recording and its pause page until the game restarted.
        public static bool Active => open != null && open.canvas && open.page != Page.None;
        static OriginalPostRace open;
        public bool IsOpen => page != Page.None;
        public string PageName => page.ToString();
        public Action onContinue, onRestart;

        enum Page { None, Results, Menu, Moto, Rewards }
        Page page;
        readonly Canvas canvas; readonly RectTransform resultsRoot, menuRoot, root;
        readonly OriginalUiTree results, menu;
        OriginalUiTree moto, rewards; RectTransform motoRoot, rewardsRoot; Action pageAccept;
        string[] headers;   // the game's column titles (row 0 of "Activity Results"); null: a race's TOTAL / BEST LAP / PENALTY
        readonly OriginalButtonLegend legend;
        readonly OriginalConfirmPopup popup;
        readonly Dictionary<string, string> loc;
        readonly List<OriginalUiTree.Node> items = new List<OriginalUiTree.Node>();
        readonly List<(string key, Action action)> entries = new List<(string, Action)>();
        List<Row> rows = new List<Row>();
        int focus;
        // RIDER CAP (w-rc-client): the layout has the original's 12 rows (ListItem0..11). A bigger field pages through them, 12 riders at a
        // time: up / down (d-pad, arrows, Page Up / Down, the wheel) or the legend's "toggle" entry; a field of 12 or fewer is the original's page.
        const int PageRows = Reflex.Port.RxCore.RxRiderCap.Original;
        int rowPage;
        int RowPages => Math.Max(1, (rows.Count + PageRows - 1) / PageRows);
        const string Base = "PostEventResults.uicomponent/LayoutContainer/";

        readonly Transform owner;
        public OriginalPostRace(Transform parent, Action onContinue, Action onRestart, bool restart = true)
        {
            this.onContinue = onContinue; this.onRestart = onRestart; owner = parent;
            var data = OriginalUiAssets.Data; loc = data.Strings("InGame", "MXRaven_Default_Strings");
            var cgo = new GameObject("Original post-race pages", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(parent, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 55;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            resultsRoot = NewPage(root, "PostEventResults"); menuRoot = NewPage(root, "PostEventPage");

            results = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            var rl = data.BuildLayer("InGame", "PostEventResults.lyr");
            foreach (var e in OriginalUi.Arr(rl, "elements")) results.Build((Dictionary<string, object>)e, resultsRoot, null);
            results.AddAnimations(OriginalUi.Arr(rl, "layerAnimations"));

            menu = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            var ml = data.BuildLayer("InGame", "PostEventMenu.lyr");
            foreach (var e in OriginalUi.Arr(ml, "elements")) menu.Build((Dictionary<string, object>)e, menuRoot, null);
            foreach (string hidden in new[] { "Veto Container", "OnlineSkillLevelPopup" }) OriginalUiTree.SetVisible(menu.Find(hidden), false);   // online only

            legend = new OriginalButtonLegend(root);
            legend.SetAction("start", () => { if (page == Page.Results) Show(Page.Menu); else if (page == Page.Rewards) { var a = pageAccept; pageAccept = null; a?.Invoke(); } });
            legend.SetAction("accept", () => { if (page == Page.Moto) { var a = pageAccept; pageAccept = null; a?.Invoke(); } });
            legend.SetAction("toggle", () => { if (page == Page.Results) TurnRows(1); });
            popup = new OriginalConfirmPopup(menu, root, loc, data.DescribeLayer("Shared", "RestartEventConfirmationPopup.lyr"), null);

            // PostEventMenu:Start list data (a single-player race: CONTINUE, RESTART)
            entries.Add(("STR_POSTEVENT_CONTINUE", () => this.onContinue?.Invoke()));
            if (restart) entries.Add(("STR_POSTEVENT_RESTART", () => popup.ShowRestart(() => this.onRestart?.Invoke())));
            for (int i = 0; i < 5; i++)
            {
                var item = menu.Find("PostEventList/NavListContainer/Item" + i);
                if (item == null) continue;
                if (i >= entries.Count) { OriginalUiTree.SetVisible(item, false); continue; }
                OriginalUiTree.SetText(menu.Child(item, "Text"), loc.TryGetValue(entries[i].key, out var s) ? s : entries[i].key);
                int row = i;
                var bg = menu.Child(item, "CursorBackground");
                if (bg != null && bg.graphic)
                {
                    bg.graphic.raycastTarget = true;
                    var b = bg.rect.gameObject.AddComponent<Button>(); b.transition = Selectable.Transition.None;
                    b.onClick.AddListener(() => { if (page == Page.Menu && !popup.IsOpen) { FocusItem(row); entries[row].action(); } });
                }
                items.Add(item);
            }
            canvas.enabled = false;
        }

        static RectTransform NewPage(RectTransform parent, string name)
        {
            var go = new GameObject(name, typeof(RectTransform)); go.transform.SetParent(parent, false);
            var rt = (RectTransform)go.transform; rt.anchorMin = Vector2.zero; rt.anchorMax = Vector2.one; rt.offsetMin = rt.offsetMax = Vector2.zero;
            return rt;
        }

        /// The table's rows (the local player's result); applied when the results page opens.
        public void SetRows(List<Row> r) { rows = r ?? new List<Row>(); rowPage = 0; }
        /// The column titles (Name, Column0..2) as the game's table gives them (null: the race's own).
        public void SetHeaders(string[] h) { headers = Localized(h); }
        // App.SetDynamicText with the table's text: a string-table key ("STR_...") shows its text
        string[] Localized(string[] h) => h != null && h.Length >= 4 ? h.Select(x => x != null && loc.TryGetValue(x, out var v) ? v : x ?? "").ToArray() : null;

        /// PostEventMotoResults (the Moto Results Page): the series' standings in the same table layout; accept continues.
        public void ShowMotoResults(List<Row> table, string[] titles, Action onAccept)
        {
            if (moto == null)
            {
                motoRoot = NewPage(root, "Moto Results Page");
                moto = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
                var l = OriginalUiAssets.Data.BuildLayer("InGame", "PostEventMotoResults.lyr");
                foreach (var e in OriginalUi.Arr(l, "elements")) moto.Build((Dictionary<string, object>)e, motoRoot, null);
                moto.AddAnimations(OriginalUi.Arr(l, "layerAnimations"));
            }
            FillTable(moto, Localized(titles), table, Str);
            OpenPage(Page.Moto, onAccept);
            legend.Set("accept", true, "STR_CONTINUE");   // PostEventMotoResults:Start
        }

        public sealed class Rewards
        {
            public string prize, bonus, total; public bool credits; public readonly List<string> items = new List<string>(); public int place = 1000;
        }
        /// PostEventRewards (the Post Event Reward Page): Populate{Series,Credit,Trophy}Reward and LayoutVisibleComponents; start continues.
        /// The page's accept (the legend / Enter), for tests.
        public void AcceptPage() { var a = pageAccept; pageAccept = null; a?.Invoke(); }
        public void ShowRewards(Rewards r, Action onAccept)
        {
            if (rewards == null)
            {
                rewardsRoot = NewPage(root, "Post Event Reward Page");
                rewards = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
                var l = OriginalUiAssets.Data.BuildLayer("InGame", "PostEventRewards.lyr");
                foreach (var e in OriginalUi.Arr(l, "elements")) rewards.Build((Dictionary<string, object>)e, rewardsRoot, null);
                rewards.AddAnimations(OriginalUi.Arr(l, "layerAnimations"));
            }
            var t = rewards;
            for (int i = 0; i < 8; i++) { var item = t.Find("Item" + i); OriginalUiTree.SetVisible(item, false); }
            for (int i = 0; i < r.items.Count && i < 8 - 1; i++) { var item = t.Find("Item" + i); OriginalUiTree.SetVisible(item, true); OriginalUiTree.SetText(item, r.items[i]); }
            var series = t.Find("SeriesReward"); OriginalUiTree.SetVisible(series, r.items.Count > 0);
            OriginalUiTree.SetText(t.Find("PrizeCredits"), r.prize ?? ""); OriginalUiTree.SetText(t.Find("BonusCredits"), r.bonus ?? ""); OriginalUiTree.SetText(t.Find("TotalCredits"), r.total ?? "");
            var credit = t.Find("CreditReward"); OriginalUiTree.SetVisible(credit, r.credits);
            var trophy = t.Find("TrophyReward");
            OriginalUiTree.SetVisible(t.Find("GoldTrophy"), r.place == 0); OriginalUiTree.SetVisible(t.Find("SilverTrophy"), r.place == 1); OriginalUiTree.SetVisible(t.Find("BronzeTrophy"), r.place == 2);
            OriginalUiTree.SetVisible(trophy, r.place < 3);
            // LayoutVisibleComponents: the visible credit / series containers spread over the layer's height
            var shown = new List<OriginalUiTree.Node>(); foreach (var c in new[] { credit, series }) if (c != null && c.rect.gameObject.activeSelf) shown.Add(c);
            float spacing = 720f / (shown.Count + 1);
            for (int i = 0; i < shown.Count; i++) { var c = shown[i]; c.rect.anchoredPosition = new Vector2(c.rect.anchoredPosition.x, -(i + 1) * spacing); c.basePos = c.rect.anchoredPosition; }
            OpenPage(Page.Rewards, onAccept);
            legend.Set("start", true, "STR_CONTINUE");   // PostEventRewards:Start
        }

        void OpenPage(Page p, Action onAccept)
        {
            open = this; canvas.enabled = true; Reflex.Port.Platform.ReflexCursor.Want(canvas, true);   // (keyed on the canvas: a world torn down with the page up drops it)
            popup.Close(); pageAccept = onAccept; Show(p);
        }

        /// The results layout's header and rows (PostEventResults:Start / OnFormatResultsList) in a table tree.
        static void FillTable(OriginalUiTree t, string[] titles, List<Row> table, Func<string, string> Str)
        {
            var art = t.Find("BackgroundArt"); var cols = t.Child(art, "ColumnTextComponents");
            if (titles != null) { OriginalUiTree.SetText(t.Child(cols, "PlayerNameColumn"), titles[0]); for (int c = 0; c < 3; c++) OriginalUiTree.SetText(t.Child(cols, "Column" + c), titles[c + 1]); }
            for (int i = 0; i < 12; i++)
            {
                var item = t.Find("ListItem" + i); if (item == null) continue;
                bool used = i < table.Count; OriginalUiTree.SetVisible(item, used); if (!used) continue;
                var cc = t.Child(item, "ColumnTextComponents"); var r = table[i];
                OriginalUiTree.SetText(t.Child(cc, "IndexColumn"), r.index.ToString(CultureInfo.InvariantCulture));
                OriginalUiTree.SetText(t.Child(cc, "PlayerNameColumn"), r.name ?? "");
                OriginalUiTree.SetText(t.Child(cc, "Column0"), Str(r.c0 ?? "")); OriginalUiTree.SetText(t.Child(cc, "Column1"), Str(r.c1 ?? "")); OriginalUiTree.SetText(t.Child(cc, "Column2"), Str(r.c2 ?? ""));
            }
        }

        string Str(string key) => loc.TryGetValue(key, out var v) ? v : key;

        // ---- the game's own "Activity Results" (arcade; MOTOCAREER's pages ask it themselves, OriginalCareerRace)
        readonly object tableGate = new object(); string gameTable; bool tableAsked; float tableWait;
        /// SetHeaders / SetRows came from the game's table (AskGameTable).
        public bool HasGameTable { get; private set; }
        /// PostEventResults:Start's App.PopulateResultsTable on the running world, between its frames (once per race). False while the
        /// answer is pending (at most a second); then, when the game's table had rows, its titles and rows are applied (HasGameTable) with
        /// the local player's name (localName: the world's Player List name) shown as the Steam persona the PC build would sign in with.
        public bool AskGameTable(OriginalSimulation sim, float dt, string localName = null)
        {
            if (OutroHold(owner, dt)) return false;   // the Event Outro page first (the post scenarios)
            string gt;
            lock (tableGate)
            {
                if (!tableAsked)
                {
                    tableAsked = true; tableWait = 0; gameTable = null;
                    if (sim == null) gameTable = "";
                    else sim.BetweenFrames(w => { string r = ReflexCareer.WorldCommand(w, "results"); lock (tableGate) gameTable = r ?? ""; });
                }
                gt = gameTable;
            }
            if (gt == null) { tableWait += dt; if (tableWait < 1f) return false; lock (tableGate) gameTable = gt = ""; }
            if (HasGameTable || gt.Length == 0) return true;
            var (titles, table) = FromGameTable(gt);
            if (titles == null || table.Count == 0) return true;
            string persona = ReflexProfile.HasPersona ? ReflexProfile.Persona : null;
            if (persona != null && !string.IsNullOrEmpty(localName)) for (int i = 0; i < table.Count; i++) if (table[i].name == localName) { var r = table[i]; r.name = persona; table[i] = r; }
            SetHeaders(titles); SetRows(table); HasGameTable = true;
            return true;
        }

        // The Event Outro page (WP-5): the post scenarios play after the finish before the game goes to PostEventResults (MXActivityRace's
        // post sub-state A+0x13e640 = 1, ~5.3 s on NAT_Concord; accept skips them: OriginalEventIntro). The pages wait for their end; at
        // most 30 s, and not at all with an older plugin or an activity without that state.
        static float outroWaited;
        public static bool OutroHold(Transform owner, float dt)
        {
            var pages = owner ? OriginalEventIntro.For(owner) : null;
            if (!pages || OriginalEventIntro.NativeMissing || !pages.HasState) { outroWaited = 0; return false; }
            var st = pages.Current;
            bool hold = st.race != 4 && st.race >= 0 || pages.OutroRunning;   // a state from before Post, or the post scenarios
            if (st.race == 4 && st.post < 0) hold = false;                    // an activity without the post sub-state
            if (!hold) { outroWaited = 0; return false; }
            outroWaited += dt; if (outroWaited > 30f) return false;
            return true;
        }

        /// The game's "Activity Results" table JSON (career::world_command "results": {"table": {"columns": Index / Name / Column0..2}}):
        /// row 0's Name / Column0..2 are the column titles, the rows with Index >= 0 sorted by it (PostEventResults:Start).
        public static (string[] headers, List<Row> rows) FromGameTable(string resultsJson)
        {
            var rows = new List<Row>(); string[] headers = null;
            var o = OriginalUi.ParseJson(resultsJson) as Dictionary<string, object>;
            var cols = OriginalUi.Obj(OriginalUi.Obj(o, "table"), "columns"); if (cols == null) return (null, rows);
            List<object> C(string n) => cols.TryGetValue(n, out var v) ? v as List<object> : null;
            var index = C("Index"); var name = C("Name"); var c0 = C("Column0"); var c1 = C("Column1"); var c2 = C("Column2");
            string S(List<object> l, int i) => l != null && i < l.Count ? l[i] as string ?? "" : "";
            int n = index?.Count ?? 0;
            var order = new List<(int idx, int row)>();
            for (int i = 0; i < n; i++) if (index[i] is double d) { if (d >= 0) order.Add(((int)d, i)); else if (headers == null) headers = new[] { S(name, i), S(c0, i), S(c1, i), S(c2, i) }; }
            foreach (var (idx, row) in order.OrderBy(x => x.idx))
                rows.Add(new Row { index = idx + 1, name = S(name, row), c0 = S(c0, row), c1 = S(c1, row), c2 = S(c2, row) });
            return (headers, rows);
        }

        public void Open()
        {
            if (page != Page.None) return;
            open = this; canvas.enabled = true;
            Reflex.Port.Platform.ReflexCursor.Want(canvas, true);
            FillResults();
            Show(Page.Results);
        }

        public void Close()
        {
            lock (tableGate) { gameTable = null; tableAsked = false; tableWait = 0; HasGameTable = false; }
            if (page == Page.None) return;
            popup.Close(); legend.Reset();
            page = Page.None; if (open == this) open = null; canvas.enabled = false; Reflex.Port.Platform.ReflexCursor.Want(canvas, false);
        }

        void Show(Page p)
        {
            page = p;
            resultsRoot.gameObject.SetActive(p == Page.Results); menuRoot.gameObject.SetActive(p == Page.Menu);
            if (motoRoot) motoRoot.gameObject.SetActive(p == Page.Moto);
            if (rewardsRoot) rewardsRoot.gameObject.SetActive(p == Page.Rewards);
            legend.Reset();
            if (p == Page.Results) { legend.Set("start", true, "STR_CONTINUE"); RowsLegend(); }   // PostEventResultsLayer:Start (+ a big field's pages)
            if (p == Page.Menu) FocusItem(0);
        }

        void FillResults()
        {
            // column titles (PostEventResults:Start: the table's header row)
            if (headers != null) { Header("PlayerNameColumn", headers[0]); Header("Column0", headers[1]); Header("Column1", headers[2]); Header("Column2", headers[3]); }
            else { Header("PlayerNameColumn", ""); Header("Column0", Str("STR_ACTIVITYRESULTSCOLUMN_TOTALTIME")); Header("Column1", Str("STR_ACTIVITYRESULTSCOLUMN_BESTLAPTIME")); Header("Column2", Str("STR_ACTIVITYRESULTSCOLUMN_PENALTYTIME")); }
            rowPage = Mathf.Clamp(rowPage, 0, RowPages - 1);
            int first = rowPage * PageRows;
            for (int i = 0; i < PageRows; i++)
            {
                var item = results.Find(Base + "ResultsList/ListItem" + i); if (item == null) continue;
                bool used = first + i < rows.Count;
                OriginalUiTree.SetVisible(item, used);
                if (!used) continue;
                var cols = results.Child(item, "ColumnTextComponents"); var r = rows[first + i];
                OriginalUiTree.SetText(results.Child(cols, "IndexColumn"), r.index.ToString(CultureInfo.InvariantCulture));
                OriginalUiTree.SetText(results.Child(cols, "PlayerNameColumn"), r.name ?? "");
                // App.SetDynamicText: a cell holding a string-table key (a Freestyle best trick "STR_TWISTER") shows its text
                OriginalUiTree.SetText(results.Child(cols, "Column0"), Str(r.c0 ?? "")); OriginalUiTree.SetText(results.Child(cols, "Column1"), Str(r.c1 ?? "")); OriginalUiTree.SetText(results.Child(cols, "Column2"), Str(r.c2 ?? ""));
            }
        }

        // the next / previous 12 riders of a big field (wraps); the legend says which riders the page shows
        void TurnRows(int step)
        {
            if (RowPages <= 1) return;
            rowPage = (rowPage + step + RowPages) % RowPages;
            FillResults(); RowsLegend();
        }
        void RowsLegend()
        {
            if (RowPages <= 1) { legend.Set("toggle", false); return; }
            int next = (rowPage + 1) % RowPages, from = next * PageRows + 1, to = Math.Min(rows.Count, from + PageRows - 1);
            legend.Set("toggle", true, "RIDERS " + from + "-" + to);
        }

        void Header(string column, string text) => OriginalUiTree.SetText(results.Find(Base + "BackgroundArt/ColumnTextComponents/" + column), text);

        void FocusItem(int i)
        {
            if (items.Count == 0) return;
            focus = (i + items.Count) % items.Count;
            for (int k = 0; k < items.Count; k++)
            {
                bool f = k == focus;
                var bg = menu.Child(items[k], "CursorBackground"); var text = menu.Child(items[k], "Text");
                OriginalUiTree.SetColor(bg, OriginalUiTree.StateColor(bg, f, true)); OriginalUiTree.SetColor(text, OriginalUiTree.StateColor(text, f, true));
            }
        }

        /// Per frame while open: animations, the legend and the keyboard.
        public void Tick(float dt)
        {
            if (page == Page.None) return;
            results.Tick(dt); menu.Tick(dt); legend.Tick(dt); moto?.Tick(dt); rewards?.Tick(dt);
            var kb = Keyboard.current; var pad = OriginalPadInput.Read();   // the keyboard and every gamepad (A accepts, Start is the game's Start, the d-pad / stick move)
            if (popup.IsOpen) { popup.HandleKeys(kb, pad); return; }
            bool accept = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || pad.accept;
            bool start = kb != null && kb.f3Key.wasPressedThisFrame || pad.start;
            if (page == Page.Moto || page == Page.Rewards)
            {
                if (accept || start) { var a = pageAccept; pageAccept = null; if (a != null) a(); }
                return;
            }
            if (page == Page.Results)
            {
                if (accept || start) Show(Page.Menu);
                else if (RowPages > 1)
                {   // a big field's other riders (RIDER CAP)
                    float wheel = Mouse.current != null ? Mouse.current.scroll.ReadValue().y : 0f;
                    if (kb != null && (kb.downArrowKey.wasPressedThisFrame || kb.pageDownKey.wasPressedThisFrame) || pad.down || wheel < 0f) TurnRows(1);
                    else if (kb != null && (kb.upArrowKey.wasPressedThisFrame || kb.pageUpKey.wasPressedThisFrame) || pad.up || wheel > 0f) TurnRows(-1);
                }
            }
            else
            {
                if (kb != null && kb.upArrowKey.wasPressedThisFrame || pad.up) FocusItem(focus - 1);
                if (kb != null && kb.downArrowKey.wasPressedThisFrame || pad.down) FocusItem(focus + 1);
                if (accept) entries[focus].action();
            }
        }
    }
}
