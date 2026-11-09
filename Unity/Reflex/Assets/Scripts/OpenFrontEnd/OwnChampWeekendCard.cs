// RESTORED CONTENT (championship weekend): the weekend card between two stages, over the world after the stage's results (ChampWeekendFlow):
// the race results page's look (OuRaceResults: the side column, the board), with
//   side   your place in the standings (the big number), the qualification line (Supercross: IN THE MAIN EVENT / TO THE LCQ / DID NOT MAKE
//          THE MAIN), the tiles (POINTS or SCORE, the next stage, its CLOCK + LAPS, YOUR GATE in it, the field), the actions: START <stage>
//          (ChampWeekendFlow.NextStage); QUIT WEEKEND (a confirmation); after the last stage FINISH WEEKEND
//   main   the stages in order (done / decided / next, each with its format) and the standings: every stage's place, Supercross's STATUS
//          (MAIN / LCQ / OUT), the points (Freestyle: the judged scores)
// It reads a ChampWeekendView (the weekend's, or the showcase's through Preview).
//   names  cw-card, cw-next, cw-finish, cw-quit, cw-confirm, cw-cancel, cw-row-<i>
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class OwnChampWeekendCard : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        GameObject host;
        VisualElement root, body, hints, overlay;
        OuMenuInput input; OuNav nav;
        bool active, pending, armed, lease, pausedBefore, preview;
        float pendingSince; int pageFrame = -1;
        string page;
        ChampWeekendView view;

        public bool Active => active;
        /// "card", "quit" (the confirmation); null when down.
        public string Page => page;
        public VisualElement Root => root;
        public ChampWeekendView View => view;

        /// The stage is over: the card comes up as soon as the stage is recorded (at once after the results; a retire takes a frame or two).
        public void Begin(OriginalWorldRuntime rt)
        {
            runtime = rt; preview = false;
            if (active) return;
            active = true; pending = true; pendingSince = Time.unscaledTime;
            if (!lease && runtime)
            {
                lease = true;
                pausedBefore = runtime.paused;
                runtime.ClearMenuInput();
                Reflex.Port.Platform.ReflexCursor.Want(this, true);   // shown for a mouse player only (this used to free and recentre it for a pad too)
            }
            Update();
        }

        /// The showcase / tests: the card of a given view, without a world (its buttons log only).
        public void Preview(ChampWeekendView v)
        {
            preview = true; active = true; pending = false; view = v;
            Build(); Show();
        }

        void Update()
        {
            if (!active) return;
            if (!preview && (!runtime || !runtime.isActiveAndEnabled)) { Close(); return; }
            if (pending)
            {
                // the stage's result first (recorded on the worker; a retire: the world runs a frame or two for it, without input)
                bool ready = !RxChampWeekend.StageInSession || RxChampWeekend.StageRecorded || Time.unscaledTime - pendingSince > 3f;
                if (!ready) return;
                pending = false;
                if (runtime) { runtime.paused = true; runtime.ClearMenuInput(); }
                try { view = ChampWeekendFlow.View(); Build(); Show(); }
                catch (Exception e) { Debug.LogException(e); if (runtime) runtime.ReportOwnResultsFailure("Could not show the weekend standings: " + e.Message); Close(); return; }
                return;
            }
            if (pageFrame == Time.frameCount || nav == null) return;
            if (!armed) { if (!MenuHeld()) { armed = true; pageFrame = Time.frameCount; } return; }
            nav.Update(input);
        }

        static bool MenuHeld()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.enterKey.isPressed || kb.numpadEnterKey.isPressed || kb.spaceKey.isPressed || kb.escapeKey.isPressed)) return true;
            if (Mouse.current?.leftButton.isPressed == true) return true;
            foreach (var pad in Gamepad.all) if (pad.buttonSouth.isPressed || pad.buttonEast.isPressed || pad.startButton.isPressed) return true;
            return false;
        }
        bool Interactive => active && !pending && armed && pageFrame != Time.frameCount;

        void Build()
        {
            if (root != null) { root.RemoveFromClassList("ou-results-hidden"); return; }
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex weekend card"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 76); OuTheme.Apply(root);
            foreach (var name in new[] { "OuRaceResults", "OuChampWeekend" })
            {
                var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + name);
                if (style) root.styleSheets.Add(style); else Debug.LogWarning("[ChampWeekend] missing stylesheet " + name);
            }
            var layer = OuUi.Div("mxa-layer mxa-dim ou-race-results ou-cw", root);
            layer.name = "cw-card";
            body = OuUi.Div("ou-results-body", layer);
            overlay = OuUi.Div("ou-overlay", layer); overlay.pickingMode = PickingMode.Ignore;
            hints = OuUi.Div("mxa-hints", root);
            nav = new OuNav { Screen = layer, OnCancel = Back };
        }

        void ResetPage(string name)
        {
            page = name; armed = false; pageFrame = Time.frameCount;
            body.Clear(); overlay.Clear(); overlay.pickingMode = PickingMode.Ignore; overlay.RemoveFromClassList("ou-in");
            input?.Dispose(); input = new OuMenuInput();
            OuUi.Hints(hints, name == "card" ? new[] { (OuUi.KeySubmit, "SELECT") } : new[] { (OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "CANCEL") });
        }

        static Label Text(string value, string cls, VisualElement parent) { var l = OuUi.Text(value, cls, parent); l.enableRichText = false; return l; }
        VisualElement Button(string title, string name, Action action, VisualElement parent)
        {
            var b = OuUi.Button(title, "mxa-main-button", () => { if (Interactive || preview) action(); }, parent);
            b.name = name; return b;
        }
        void Focus(VisualElement target)
        {
            nav.DefaultItem = () => target;
            body.schedule.Execute(() => { if (active && target.panel != null) nav.Focus(target); });
        }

        // ---------------------------------------------------------------------------------------------- the card
        void Show()
        {
            var v = view ?? new ChampWeekendView();
            ResetPage("card");
            var next = v.NextStage; var last = v.LastStage;
            string title = v.finished ? "FINAL STANDINGS" : last != null ? last.label + "  ·  WEEKEND STANDINGS" : "WEEKEND STANDINGS";
            OuTheme.Header(body, title, "CHAMPIONSHIP WEEKEND  ·  " + v.eventLabel);
            var pageEl = OuUi.Div("ou-page", body);
            if (v.riders.Count > RxRiderCap.Original) pageEl.AddToClassList("ou-results-big");   // FIELD SIZE: a big field's tighter rows (as OwnRaceResults)
            var side = OuUi.Div("ou-page-side", pageEl); var main = OuUi.Div("ou-page-main", pageEl);

            // ---- the side: you
            var info = OuUi.Div("ou-results-info", side);
            if (v.localRank > 0) OuWidgets.Place(info, v.localRank, OuWidgets.Ordinal(v.localRank));
            string line = v.LocalStatusLine(out string tone);
            if (line != null) { var pill = OuWidgets.Pill(info, line, tone); pill.AddToClassList("ou-cw-status"); pill.name = "cw-status"; }
            var me = v.Local;
            var tiles = new List<(string, string, string)>();
            if (me != null) tiles.Add(v.judged ? ("SCORE", me.total.ToString("0.00", CultureInfo.InvariantCulture), "gold") : ("POINTS", me.points.ToString(CultureInfo.InvariantCulture), "gold"));
            int mine = -1;   // the last stage the local rider raced (a decided LCQ is not one of his)
            if (me != null) for (int s = v.stages.Count - 1; s >= 0; s--) if (me.place[s] > 0 && (v.stages[s].raced || v.stages[s].decided)) { mine = s; break; }
            if (mine >= 0)
            {
                int lp = me.place[mine];
                tiles.Add((v.stages[mine].label, lp + OuWidgets.Ordinal(lp), lp == 1 ? "good" : null));
            }
            if (next != null)
            {
                tiles.Add(("NEXT", next.label, null));
                tiles.Add((next.minutes > 0 ? "CLOCK + LAPS" : v.timeable ? "LAPS" : "FORMAT", string.IsNullOrEmpty(next.format) ? EventFormat(v) : next.format, null));
                tiles.Add(("YOUR GATE", v.nextGate > 0 ? v.nextGate + OuWidgets.Ordinal(v.nextGate) + " OF " + v.nextField : "EVENT ORDER", v.nextGate > 0 && v.nextGate <= 3 ? "good" : null));
                if (v.nextField > 0) tiles.Add(("FIELD", v.nextField + " RIDERS", null));
            }
            else if (v.eliminated)
                tiles.Add(("OUT AFTER THE", mine >= 0 ? v.stages[mine].label : "-", null));
            else if (v.riders.Count > 0)
            {
                tiles.Add(("WINNER", v.riders[0].name.ToUpperInvariant(), "gold"));
                tiles.Add(("RIDERS", v.riders.Count.ToString(CultureInfo.InvariantCulture), null));
            }
            if (tiles.Count > 0) OuWidgets.StatGrid(info, tiles, 2);

            var actions = OuUi.Div("ou-actions", side);
            var column = new List<VisualElement>();
            if (next != null)
            {
                column.Add(Button("START " + next.label + "  ›", "cw-next", StartNext, actions));
                column.Add(Button("QUIT WEEKEND", "cw-quit", ConfirmQuit, actions));
            }
            else column.Add(Button("FINISH WEEKEND", "cw-finish", Finish, actions));
            OuWidgets.LinkColumn(column);
            OwnRaceResults.FitSide(side, info);

            // ---- the main: the stages, the standings
            var strip = OuUi.Div("ou-cw-strip", main);
            for (int s = 0; s < v.stages.Count; s++)
            {
                var st = v.stages[s];
                bool isNext = s == v.next, done = st.raced || st.decided;
                var chip = OuUi.Div("ou-cw-stage" + (done ? " ou-cw-stage-done" : "") + (isNext ? " ou-cw-stage-next" : ""), strip);
                Text(st.label, "ou-cw-stage-name", chip);
                Text(st.decided && !st.raced ? "DECIDED" : st.raced ? "DONE" : isNext ? (string.IsNullOrEmpty(st.format) ? "NEXT" : st.format) : st.format, "ou-cw-stage-sub", chip);
            }
            var spec = BoardSpec(v);
            OwnRaceResults.BoardHead(main, spec);
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("ou-page-scroll"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; main.Add(scroll);
            var rows = new List<VisualElement>(); VisualElement local = null;
            for (int i = 0; i < spec.rows.Count; i++)
            {
                var row = OwnRaceResults.BoardRow(scroll, spec, spec.rows[i]);
                row.name = "cw-row-" + i;
                var r = row; row.RegisterCallback<FocusInEvent>(_ => scroll.ScrollTo(r));
                if (spec.rows[i].you) local = row;
                if (v.riders[i].status == "OUT") row.AddToClassList("ou-cw-out");
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);
            if (column.Count > 0)
            {
                foreach (var r in rows) ((OuNavItem)r.userData).right = column[0];
                foreach (var b in column) ((OuNavItem)b.userData).left = local ?? (rows.Count > 0 ? rows[0] : null);
            }
            OuMotion.Enter(side, 0.05f, 0.02f, true); OuMotion.Enter(scroll.contentContainer, 0.03f, 0.12f);
            if (column.Count > 0) Focus(column[0]); else if (local != null) Focus(local);
            if (local != null) scroll.schedule.Execute(() => scroll.ScrollTo(local)).ExecuteLater(60);
        }

        static string EventFormat(ChampWeekendView v) => v.discipline == RxChampWeekend.Freestyle ? "THE EVENT'S TIME" : v.discipline == RxChampWeekend.Waypoint ? "THE EVENT'S ROUNDS" : "THE EVENT'S LAPS";

        /// The standings as a board: every stage's place, Supercross's STATUS, the points (or the judged score).
        internal static OwnResultsSpec BoardSpec(ChampWeekendView v)
        {
            var spec = new OwnResultsSpec { title = "WEEKEND STANDINGS" };
            foreach (var st in v.stages) spec.columns.Add(new OwnBoardColumn(ChampWeekendView.Short(st.label), "ou-cw-col"));
            bool status = v.riders.Exists(r => r.status != null);
            if (status) spec.columns.Add(new OwnBoardColumn("STATUS", "ou-cw-statuscol"));
            spec.columns.Add(new OwnBoardColumn(v.judged ? "SCORE" : "PTS", "ou-board-points"));
            for (int i = 0; i < v.riders.Count; i++)
            {
                var r = v.riders[i];
                var cells = new List<string>();
                for (int s = 0; s < v.stages.Count; s++)
                {
                    int p = r.place[s];
                    cells.Add(p <= 0 ? "-" : v.judged && v.stages[s].scored ? r.score[s].ToString("0.0", CultureInfo.InvariantCulture) : p.ToString(CultureInfo.InvariantCulture));
                }
                if (status) cells.Add(r.status ?? "");
                cells.Add(v.judged ? r.total.ToString("0.00", CultureInfo.InvariantCulture) : r.points.ToString(CultureInfo.InvariantCulture));
                spec.rows.Add(new OwnResultsRow { slot = i, rank = i + 1, name = (r.name ?? "").ToUpperInvariant(), you = r.local, cells = cells.ToArray() });
            }
            return spec;
        }

        void StartNext()
        {
            if (preview) { Debug.Log("[ChampWeekend] (preview) START"); return; }
            var rt = runtime;
            Close();
            ChampWeekendFlow.NextStage(rt);
        }
        void Finish()
        {
            if (preview) { Debug.Log("[ChampWeekend] (preview) FINISH"); return; }
            Close();
            ChampWeekendFlow.FinishWeekend();
        }
        void ConfirmQuit()
        {
            ResetPage("quit");
            var area = OuWidgets.Dialog(overlay);
            Text("CHAMPIONSHIP WEEKEND", "ou-eyebrow2 ou-label", area);
            Text("QUIT THE WEEKEND?", "ou-dialog-title ou-display", area);
            Text("The weekend ends here and you return to the menu.", "ou-dialog-lead", area);
            var row = OuUi.Div("ou-button-row", area);
            var cancel = Button("CANCEL", "cw-cancel", Show, row);
            var confirm = Button("QUIT WEEKEND", "cw-confirm", () => { if (preview) { Debug.Log("[ChampWeekend] (preview) QUIT"); return; } Close(); ChampWeekendFlow.QuitWeekend(); }, row);
            OuWidgets.LinkRow(new[] { cancel, confirm });
            overlay.pickingMode = PickingMode.Position;
            OuMotion.Fade(overlay);
            Focus(cancel);
        }
        bool Back()
        {
            if (!Interactive) return true;
            if (page == "quit") Show();
            return true;   // (the card itself has no way back: the stage is over)
        }

        /// The card goes (the next stage starts, the weekend ends, the world goes).
        public void Close()
        {
            if (!active && !lease) return;
            active = false; pending = false; page = null; armed = false;
            input?.Dispose(); input = null;
            root?.AddToClassList("ou-results-hidden");
            if (lease)
            {
                lease = false;
                if (runtime) { runtime.paused = pausedBefore; runtime.ClearMenuInput(); }
                Reflex.Port.Platform.ReflexCursor.Want(this, false);
            }
        }
        void OnDisable() => Close();
        void OnDestroy() { Close(); if (host) Destroy(host); }
    }
}
