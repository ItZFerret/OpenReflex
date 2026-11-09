using System;
using System.Collections.Generic;
using Reflex.Port.Hud;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DefaultExecutionOrder(-190), DisallowMultipleComponent]
    public sealed class OwnRaceResults : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        OwnRaceResultsSnapshot snapshot;
        GameObject host;
        VisualElement root, body, hints, overlay;
        OuMenuInput input;
        OuNav nav;
        bool active, closing, committed, lease, pausedBefore;
        bool armed, holdHover;
        Vector2 hoverOrigin;
        int pageFrame = -1, closedFrame = -1;
        string page;
        readonly Dictionary<int, VisualElement> rowBySlot = new Dictionary<int, VisualElement>();

        public bool Active => active;
        public bool OwnsInput => active || closedFrame == Time.frameCount;
        public string Page => page;
        public VisualElement Root => root;
        public OwnRaceResultsSnapshot Snapshot => snapshot;

        void Awake() => runtime = GetComponent<OriginalWorldRuntime>();
        bool Current(OwnRaceResultsSnapshot value) => runtime && runtime.Running && value != null
            && runtime.OwnResultsPresentation && runtime.OfflineExhibitionOpen
            && runtime.WorldGeneration == value.Generation
            && string.Equals(runtime.EventIdentity, value.EventIdentity, StringComparison.Ordinal)
            && ReferenceEquals(runtime.OwnRaceResultsSnapshot, value);

        // Runtime calls only after Post2 has settled, restoring its temporary pre-drain pause before this lease.
        public bool Begin(OwnRaceResultsSnapshot value)
        {
            if (active) return ReferenceEquals(snapshot, value) && Current(value);
            if (!runtime) runtime = GetComponent<OriginalWorldRuntime>();
            if (!Current(value) || !OwnRaceResultsSnapshot.Supports(true, value.Race, value.EventUi)
                || !OwnRaceResultsSnapshot.Supports(true, runtime.Race, runtime.EventUi)) return false;
            snapshot = value; committed = false;
            pausedBefore = runtime.paused;
            lease = active = true;
            try
            {
                runtime.GetComponent<OriginalOnboardAdjust>()?.End();
                // The world stops for the page (no input), the sound does not: the original's post-event pages are not a game pause (no
                // Sound_PauseAll: only the pause menu's MXUIManager_OnPauseRequest pauses the sound) and the end-of-race replay's clips run with
                // the sound resumed (ClipRecorder_BeginClip: Sound_PauseAll( 0, 0 )), so the replay behind the results is heard (the Post
                // frames keep running for it, and the audio with them: OriginalWorldRuntime.Update).
                runtime.paused = true; runtime.ClearMenuInput();
                Reflex.Port.Platform.ReflexCursor.Want(this, true);
                snapshot.ApplyResultsPage(runtime.ResultsPage);
                Build(); ShowResults();
                return true;
            }
            catch (Exception e)
            {
                CloseForWorldChange(); // Restore before reporting; native calls belong to runtime only.
                if (runtime) runtime.ReportOwnResultsFailure("Could not open race results: " + e.Message);
                return false;
            }
        }

        static bool MenuHeld()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.enterKey.isPressed || kb.numpadEnterKey.isPressed || kb.spaceKey.isPressed
                || kb.escapeKey.isPressed || kb.f3Key.isPressed)) return true;
            if (Mouse.current?.leftButton.isPressed == true) return true;
            foreach (var pad in Gamepad.all)
                if (pad.buttonSouth.isPressed || pad.buttonEast.isPressed || pad.startButton.isPressed) return true;
            return false;
        }
        bool Interactive => active && !committed && armed && pageFrame != Time.frameCount && Current(snapshot);
        void Update()
        {
            if (!active) return;
            if (!Current(snapshot)) { CloseForWorldChange(); return; }
            if (Reflex.Port.Replay.ReflexReplay.ViewerOpen || Reflex.Port.Replay.ReflexReplay.ClosedFrame == Time.frameCount) return;   // REPLAY over the page
            ApplyResultsPage();
            if (holdHover && Mouse.current != null && (Mouse.current.position.ReadValue() - hoverOrigin).sqrMagnitude > 1f) holdHover = false;
            if (pageFrame == Time.frameCount || committed) return;
            if (!armed)
            {
                if (!MenuHeld()) { armed = true; pageFrame = Time.frameCount; }
                return;
            }
            // Start/F3 never resume a finished race. Back only cancels a confirmation.
            nav.Update(input);
        }

        // The game's results page started (the core's App.PopulateResultsTable, 14 frames after the race's "Show Activity Results"): the
        // riders still racing get the original's projected TOTAL (and BEST LAP), and every mode's cells the game table's values (w-wpfill:
        // Waypoint, Freestyle, Section Race, the Eliminator's place); the shown rows take the new values in place (the mode's spec again,
        // its cells into the row's cell labels in column order: rank, name, then the columns).
        // When the game's table numbers the riders otherwise than the standings the board was built from (its Index, the original's POS),
        // the shown board is built again in the table's order (the header's place with it), keeping the focused button.
        void ApplyResultsPage()
        {
            if (!snapshot.ApplyResultsPage(runtime.ResultsPage)) return;
            if (snapshot.OrderRevision != shownOrder)
            {
                if (page == "results") ShowResults(nav?.Focused?.name);
                return;
            }
            var spec = OwnModes.Results(snapshot);
            foreach (var value in spec.rows)
            {
                if (!rowBySlot.TryGetValue(value.slot, out var row) || row.panel == null) continue;
                for (int i = 0; i < spec.columns.Count && i < value.cells.Length && 2 + i < row.childCount; i++)
                    if (row[2 + i] is Label cell && cell.text != value.cells[i]) cell.text = value.cells[i];
            }
        }
        void Build()
        {
            if (root != null) return;
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex race results"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 75); OuTheme.Apply(root);
            var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuRaceResults");
            if (!style) throw new InvalidOperationException("Missing results stylesheet.");
            root.styleSheets.Add(style);
            var layer = OuUi.Div("mxa-layer mxa-dim ou-race-results", root);
            body = OuUi.Div("ou-results-body", layer);
            body.RegisterCallback<PointerEnterEvent>(e => { if (holdHover) e.StopImmediatePropagation(); }, TrickleDown.TrickleDown);
            overlay = OuUi.Div("ou-overlay", layer); overlay.pickingMode = PickingMode.Ignore;
            overlay.RegisterCallback<PointerEnterEvent>(e => { if (holdHover) e.StopImmediatePropagation(); }, TrickleDown.TrickleDown);
            hints = OuUi.Div("mxa-hints", root);
            nav = new OuNav { Screen = layer, OnCancel = Back };
        }
        void ResetPage(string name, string title, string caption)
        {
            page = name; armed = false; pageFrame = Time.frameCount;
            holdHover = true; hoverOrigin = Mouse.current?.position.ReadValue() ?? Vector2.zero;
            body.Clear(); overlay.Clear(); overlay.pickingMode = PickingMode.Ignore; overlay.RemoveFromClassList("ou-in"); root.RemoveFromClassList("ou-results-hidden");
            input?.Dispose(); input = new OuMenuInput();
            if (name == "results") OuTheme.Header(body, title, caption);
            root.Q<VisualElement>(className: "ou-race-results")?.EnableInClassList("mxa-dim", true);
            if (name == "results") OuUi.Hints(hints, (OuUi.KeySubmit, "SELECT"));
            else if (name == "reel") OuUi.Hints(hints, (OuUi.KeyCancel, "BACK"));
            else OuUi.Hints(hints, (OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "CANCEL"));
        }
        Label Text(string value, string cls, VisualElement parent)
        {
            var label = OuUi.Text(value, cls, parent); label.enableRichText = false; return label;
        }
        VisualElement Button(string title, string name, Action action, VisualElement parent)
        {
            var button = OuUi.Button(title, "mxa-main-button", () => { if (Interactive) action(); }, parent);
            button.name = name; return button;
        }
        void Focus(VisualElement target)
        {
            nav.DefaultItem = () => target;
            body.schedule.Execute(() => { if (active && target.panel != null && target.parent != null) nav.Focus(target); });
        }
        void ShowResults(string focusName = null) => ShowSpec(snapshot != null ? OwnModes.Results(snapshot) : resultsSpec, focusName);

        // (the page of a results spec: the showcase builds it without a world, from synthetic data)
        internal void ShowSpec(OwnResultsSpec spec, string focusName)
        {
            resultsSpec = spec; shownOrder = snapshot?.OrderRevision ?? 0;
            ResetPage("results", spec.title, spec.caption);
            var page = OuUi.Div("ou-page", body);
            if (spec.rows.Count > Reflex.Port.RxCore.RxRiderCap.Original) page.AddToClassList("ou-results-big");   // RIDER CAP: a big field's tighter rows
            var side = OuUi.Div("ou-page-side", page); var main = OuUi.Div("ou-page-main", page);
            // the local rider's result leads the side column: the place, the mode's numbers, the field
            var info = SideInfo(side, spec);
            if (spec.judges.Count > 0)
            {   // Freestyle: the judges' final card
                OuTheme.Section(info, "JUDGES");
                OuWidgets.StatGrid(info, spec.judges.ConvertAll(t => (t.label, t.value, t.tone)), 5).AddToClassList("ou-stats-judges");
            }
            BoardHead(main, spec);
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("ou-page-scroll"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; main.Add(scroll);
            var rows = new List<VisualElement>(); VisualElement local = null;
            rowBySlot.Clear();
            foreach (var value in spec.rows)
            {
                var row = BoardRow(scroll, spec, value);
                row.name = "result-slot-" + value.slot;   // (rows perform no action: keyboard / gamepad focus scrolls the table)
                rowBySlot[value.slot] = row;
                row.RegisterCallback<FocusInEvent>(_ => scroll.ScrollTo(row));
                if (value.you) local = row;
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);
            var actions = OuUi.Div("ou-actions", side);
            FitSide(side, info);
            // RESTORED CONTENT (championship weekend): a stage has no RACE AGAIN; CONTINUE WEEKEND (and BACK) shows the weekend card over the world
            // (ChampWeekendFlow: the standings, then the next stage loads in this scene)
            bool champStage = ChampWeekendFlow.InStage;
            var again = champStage ? null : Button(spec.againLabel, "results-again", () => Confirm(true), actions);
            // PostEventMenu:Start: HIGHLIGHT REEL (EventHasReplay: the races' end-of-race replay, which plays behind this page), between CONTINUE and RESTART
            var reel = ReelOffered ? Button("HIGHLIGHT REEL", "results-reel", ShowReel, actions) : null;
            // REPLAY (OPEN REFLEX's REPLAY / PHOTO MODE, Reflex.Port.Replay): the race as recorded, its own cameras; not the original's reel
            var replay = replayForShowcase || Reflex.Port.Replay.ReflexReplay.Offered(runtime) ? Button("REPLAY", "results-replay", OpenReplay, actions) : null;
            var menu = champStage ? Button("CONTINUE WEEKEND", "results-menu", ContinueWeekend, actions) : Button("RETURN TO MENU", "results-menu", () => Confirm(false), actions);
            var column = new List<VisualElement>(); if (again != null) column.Add(again); if (reel != null) column.Add(reel); if (replay != null) column.Add(replay); column.Add(menu);
            OuWidgets.LinkColumn(column);
            foreach (var r in rows) ((OuNavItem)r.userData).right = column[0];
            foreach (var button in column) ((OuNavItem)button.userData).left = local ?? (rows.Count > 0 ? rows[0] : null);
            OuMotion.Enter(side, 0.05f, 0.02f, true); OuMotion.Enter(scroll.contentContainer, 0.03f, 0.12f);
            Focus(focusName == "results-again" && again != null ? again : focusName == "results-menu" ? menu : focusName == "results-reel" && reel != null ? reel : focusName == "results-replay" && replay != null ? replay : column[0]);
            if (local != null)
            {
                scroll.schedule.Execute(() => scroll.ScrollTo(local)).ExecuteLater(60);
                // RIDER CAP (w-rc-client): a field past the viewport (a big field's 13+ rows) centres the local rider's row once the rows are
                // laid out and the board's entry motion is over (a ScrollTo during the motion left a 19th place just below the fold)
                if (spec.rows.Count > Reflex.Port.RxCore.RxRiderCap.Original)
                    scroll.schedule.Execute(() => CentreRow(scroll, local)).ExecuteLater(450);
            }
        }

        static void CentreRow(ScrollView scroll, VisualElement row)
        {
            if (scroll.panel == null || row.panel == null) return;
            float view = scroll.contentViewport.layout.height, content = scroll.contentContainer.layout.height;
            if (float.IsNaN(view) || view <= 0 || content <= view) return;
            float y = row.layout.y - (view - row.layout.height) * 0.5f;
            scroll.scrollOffset = new Vector2(0, Mathf.Clamp(y, 0, content - view));
        }
        OwnResultsSpec resultsSpec;
        int shownOrder;

        // The side column's two parts (w-uibatch): the result (the place, the mode's tiles in explicit rows, Freestyle's judges) and under it the
        // actions. The column ends above the legend band at any screen shape and UI SCALE: the actions never shrink; the result gives way, first
        // compact (.ou-results-tight: a smaller place number, shorter action plates), then clipped at its bottom. RETURN TO MENU used to run
        // under the band (every tile and plate kept its size and the column simply overflowed the page by ~80 px at 1080 with four actions).
        internal static VisualElement SideInfo(VisualElement side, OwnResultsSpec spec)
        {
            var info = OuUi.Div("ou-results-info", side);
            if (spec.place > 0) OuWidgets.Place(info, spec.place, OuWidgets.Ordinal(spec.place));
            if (spec.tiles.Count > 0)
            {   // explicit rows (a wrapping tile container is measured one line tall); three to a row, four as two pairs
                int per = spec.tiles.Count == 4 ? 2 : 3;
                var grid = OuWidgets.StatGrid(info, spec.tiles.ConvertAll(t => (t.label, t.value, t.tone)), per);
                if (spec.place <= 0) grid.AddToClassList("ou-stats-lead");
            }
            return info;
        }
        /// .ou-results-tight once the result does not fit above the actions (measured after layout; kept until the page is built again).
        internal static void FitSide(VisualElement side, VisualElement info)
        {
            void Check()
            {
                if (side.panel == null || side.ClassListContains("ou-results-tight") || info.childCount == 0) return;
                var last = info[info.childCount - 1];
                float need = last.layout.yMax + last.resolvedStyle.marginBottom;
                if (float.IsNaN(need) || float.IsNaN(info.layout.height)) return;
                if (need > info.layout.height + 1f) side.AddToClassList("ou-results-tight");
            }
            info.RegisterCallback<GeometryChangedEvent>(_ => Check());
            // a narrow page (16:10, 4:3: the content frame under 1600 px wide) gives the board the room its columns need (.ou-results-narrow:
            // a narrower side column, narrower cells); at 4:3 the TIME / PENALTY / BEST LAP columns were cut off at the right
            var page = side.parent;
            if (page != null) page.RegisterCallback<GeometryChangedEvent>(_ => { float w = page.layout.width; if (!float.IsNaN(w) && w > 0) page.EnableInClassList("ou-results-narrow", w < 1600f); });
        }   // the snapshot's OrderRevision the shown board was built in

        // the board of a spec: the rank, the rider, then the mode's columns (OwnModes), each at the width its board class gives
        internal static void BoardHead(VisualElement parent, OwnResultsSpec spec)
        {
            var head = OuUi.Div("ou-board-head", parent);
            OuUi.Text("POS", "ou-board-rank", head); OuUi.Text("RIDER", "ou-board-name ou-board-cell", head);
            foreach (var c in spec.columns) OuUi.Text(c.header, c.css + " ou-board-cell", head);
        }
        internal static VisualElement BoardRow(VisualElement parent, OwnResultsSpec spec, OwnResultsRow value)
        {
            var row = OuUi.Item(OuUi.Div("ou-board-row" + (value.you ? " ou-board-you" : ""), parent), null);
            var rank = OuUi.Text(value.rank > 0 ? value.rank.ToString() : "-", "ou-board-rank ou-display" + (value.rank >= 1 && value.rank <= 3 ? " ou-board-rank-" + value.rank : ""), row);
            rank.enableRichText = false;
            OuUi.Text(value.name, "ou-board-name ou-board-cell", row).enableRichText = false;
            for (int i = 0; i < spec.columns.Count; i++) OuUi.Text(i < value.cells.Length ? value.cells[i] : "", spec.columns[i].css + " ou-board-cell", row).enableRichText = false;
            return row;
        }
        bool Back()
        {
            if (!Interactive) return true;
            if (page == "results" && ChampWeekendFlow.InStage) ContinueWeekend();   // (a championship stage: BACK goes on to the weekend card too)
            else if (page == "restart") ShowResults("results-again");
            else if (page == "quit") ShowResults("results-menu");
            else if (page == "reel") ShowResults("results-reel");   // PostEventHighlight:OnBackKeypress: TravelBack to the post event menu
            return true;
        }

        // HIGHLIGHT REEL: the world's end-of-race replay (playing behind the table all along) with nothing over it but the legend's BACK
        // (Post Event Highlight Page: PostEventHighlight.lyr has only the legend; the PC build binds no replay controls).
        bool reelForShowcase, replayForShowcase;   // (the showcase has no world: it offers the entries itself)
        internal bool ReelOffered => reelForShowcase || (runtime && runtime.ReplayAvailable);
        internal bool ReelShown => page == "reel";
        void ShowReel() { if (Interactive) EnterReel(); }
        internal void EnterReel()
        {
            ResetPage("reel", "", "");
            root.Q<VisualElement>(className: "ou-race-results")?.RemoveFromClassList("mxa-dim");
            OuSound.Play("Accept");
            Debug.Log("[Results] HIGHLIGHT REEL");
            nav.DefaultItem = null;
        }
        void OpenReplay()
        {
            var r = Reflex.Port.Replay.ReflexReplay.Of(runtime);
            if (!Interactive || !r) return;
            root.AddToClassList("ou-results-hidden"); input?.Dispose(); input = null;
            if (!r.Open(Reflex.Port.Replay.ReflexReplay.View.Replay, Reflex.Port.Replay.ReflexReplay.Origin.Results, () => { if (active && !closing) ShowResults("results-replay"); }))
                ShowResults("results-replay");
        }

        void Confirm(bool restart)
        {
            var spec = resultsSpec ?? new OwnResultsSpec();
            ResetPage(restart ? "restart" : "quit", restart ? spec.againTitle : "RETURN TO MENU?", "OFFLINE EXHIBITION");
            var area = OuWidgets.Dialog(overlay);
            OuUi.Text("OFFLINE EXHIBITION", "ou-eyebrow2 ou-label", area);
            Text(restart ? spec.againTitle : "RETURN TO MENU?", "ou-dialog-title ou-display", area);
            Text(restart ? spec.againLead : "Leave this event and return to the menu?", "ou-dialog-lead", area);
            var row = OuUi.Div("ou-button-row", area);
            var cancel = Button("CANCEL", "results-cancel", () => ShowResults(restart ? "results-again" : "results-menu"), row);
            var confirm = Button(restart ? spec.againLabel : "RETURN TO MENU", "results-confirm", () => Commit(restart), row);
            OuWidgets.LinkRow(new[] { cancel, confirm });
            overlay.pickingMode = PickingMode.Position;
            OuMotion.Fade(overlay);
            Focus(cancel);
        }
        void Commit(bool restart)
        {
            if (!Interactive) return;
            committed = true;
            var owner = runtime;
            CloseForWorldChange();
            try { if (restart) owner.Restart(); else OriginalPauseMenu.QuitToFrontEnd(); }
            catch (Exception e) { if (owner) owner.ReportOwnResultsFailure("Could not leave race results: " + e.Message); }
        }

        // RESTORED CONTENT (championship weekend): the stage's results are done: the weekend card (ChampWeekendFlow.StageOver)
        void ContinueWeekend()
        {
            if (!Interactive) return;
            committed = true;
            var owner = runtime;
            CloseForWorldChange();
            try { ChampWeekendFlow.StageOver(owner); }
            catch (Exception e) { if (owner) owner.ReportOwnResultsFailure("Could not show the weekend standings: " + e.Message); }
        }

        void RestoreLease()
        {
            if (!lease) return;
            lease = false;
            try
            {
                if (runtime) { runtime.paused = pausedBefore; runtime.ClearMenuInput(); }
            }
            finally
            {
                Reflex.Port.Platform.ReflexCursor.Want(this, false);
            }
        }
        public void CloseForWorldChange()
        {
            if (closing) return;
            closing = true;
            try
            {
                active = false; closedFrame = Time.frameCount;
                try { input?.Dispose(); }
                catch (Exception e) { Debug.LogException(e); }
                finally
                {
                    input = null;
                    root?.AddToClassList("ou-results-hidden");
                    page = null; snapshot = null; armed = false;
                    try { RestoreLease(); } catch (Exception e) { Debug.LogException(e); }
                }
            }
            finally { closing = false; }
        }
        void OnDisable() => CloseForWorldChange();
        void OnDestroy() { CloseForWorldChange(); if (host) Destroy(host); }
    }
}
