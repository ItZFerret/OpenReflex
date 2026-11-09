using System;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    // Main-thread view. The session coordinator owns native commands and persistence.
    public sealed class OwnCareerPanels : IDisposable
    {
        readonly GameObject host;
        readonly VisualElement root, body, hints;
        readonly VisualElement layer;
        VisualElement legend;
        readonly Func<bool> current;
        readonly Func<bool> replayAvailable;   // the world's end-of-race replay is installed (PostEventMenu: EventHasReplay)
        readonly OuNav nav;
        OuMenuInput input;
        bool disposed, armed, committed, visible, cursorLease, reel;
        VisualElement reelButton;
        int shownFrame;
        ulong showEpoch, renderedEpoch;
        public ulong ShowEpoch => showEpoch;
        public bool RenderReady => LayoutReady && renderedEpoch == showEpoch && showEpoch != 0 && VisibleGeometry();
        bool VisibleGeometry()
        {
            for (var e = body; e != null; e = e.parent)
                if (!e.visible || e.resolvedStyle.display == DisplayStyle.None || e.resolvedStyle.visibility != Visibility.Visible || e.resolvedStyle.opacity <= 0) return false;
            return body.panel != null && body.worldBound.width > 0 && body.worldBound.height > 0;
        }
        public bool Rendered(ulong epoch) => epoch == showEpoch && RenderReady;
        public string Page { get; private set; }
        public bool Visible => !disposed && visible && current();
        public bool LayoutReady => Visible && root.panel != null && body.resolvedStyle.width > 0
            && body.resolvedStyle.height > 0 && Time.frameCount > shownFrame;

        public OwnCareerPanels(Transform parent, Func<bool> isCurrent, Func<bool> replayAvailable = null)
        {
            current = isCurrent ?? throw new ArgumentNullException(nameof(isCurrent));
            this.replayAvailable = replayAvailable;
            host = new GameObject("Open Reflex career panels");
            host.transform.SetParent(parent, false);
            try
            {
                OuUi.EnsureEventSystem();
                root = OuUi.CreateDocument(host, 75); OuTheme.Apply(root);
                var style = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuRaceResults");
                if (!style) throw new InvalidOperationException("Missing career results style.");
                root.styleSheets.Add(style);
                var layer = OuUi.Div("mxa-layer mxa-dim ou-race-results oc-over", root);
                OcUi.Stage(layer, false);
                body = OuUi.Div("ou-results-body", layer);
                this.layer = layer;
                hints = OuUi.Div("mxa-hints", root);
                nav = new OuNav { Screen = body, OnCancel = () => { if (reel) ExitReel(); return true; } };
                body.generateVisualContent += context =>
                {
                    if (LayoutReady && root.visible && body.visible && root.resolvedStyle.display != DisplayStyle.None
                        && body.resolvedStyle.visibility == Visibility.Visible && body.resolvedStyle.opacity > 0)
                        renderedEpoch = showEpoch;
                };
                body.RegisterCallback<DetachFromPanelEvent>(e => renderedEpoch = 0);
                root.AddToClassList("ou-results-hidden");
            }
            catch { UnityEngine.Object.Destroy(host); throw; }
        }

        static bool Held()
        {
            var k = Keyboard.current;
            if (k != null && (k.enterKey.isPressed || k.numpadEnterKey.isPressed || k.spaceKey.isPressed
                || k.escapeKey.isPressed)) return true;
            if (Mouse.current?.leftButton.isPressed == true) return true;
            foreach (var pad in Gamepad.all)
                if (pad.buttonSouth.isPressed || pad.buttonEast.isPressed || pad.startButton.isPressed) return true;
            return false;
        }
        public void Tick(bool allowInput = true)
        {
            if (disposed) return;
            if (!current()) { Dispose(); return; }
            if (Visible && !RenderReady && Time.frameCount > shownFrame) body.MarkDirtyRepaint();
            if (!allowInput || !Visible || committed || Time.frameCount == shownFrame) return;
            if (!armed) { if (!Held()) { armed = true; shownFrame = Time.frameCount; } return; }
            nav.Update(input);
        }
        void Show(string page, string title, string subtitle)
        {
            if (disposed || !current()) throw new InvalidOperationException("The career view is no longer current.");
            if (!cursorLease)
            {
                cursorLease = true; Reflex.Port.Platform.ReflexCursor.Want(this, true);
            }
            if (showEpoch == ulong.MaxValue) throw new InvalidOperationException("Career panel presentation epoch exhausted.");
            showEpoch++; renderedEpoch = 0;
            Page = page; visible = true; armed = committed = false; shownFrame = Time.frameCount;
            reel = false; reelButton = null; layer.RemoveFromClassList("oc-reel");
            input?.Dispose(); input = new OuMenuInput(); body.Clear(); hints.Clear();
            nav.DefaultItem = null; root.RemoveFromClassList("ou-results-hidden");
            legend?.RemoveFromHierarchy();
            OcUi.Header(body, title, subtitle);
            legend = OcUi.Legend(layer, (OuUi.KeySubmit, "SELECT", null));
        }
        static Label Text(string value, string classes, VisualElement parent)
        { var l = OuUi.Text(value ?? "", classes, parent); l.enableRichText = false; return l; }
        VisualElement Button(string label, string name, Action action, VisualElement parent)
        {
            return OcUi.Button(label, name, () =>
            {
                if (!RenderReady || !armed || committed || Time.frameCount == shownFrame) return;
                committed = true; action();
            }, parent, name == "career-cancel" ? "oc-btn-quiet" : null);
        }
        void Focus(VisualElement item)
        {
            nav.DefaultItem = () => item;
            body.schedule.Execute(() => { if (Visible && item.panel != null && item.parent != null) nav.Focus(item); });
        }
        /// The page's frame: a side column (hero, stats, actions) and a content column.
        void Frame(out VisualElement side, out VisualElement main)
        {
            var page = OuUi.Div("oc-page", body);
            side = OuUi.Div("oc-page-side", page); main = OuUi.Div("oc-page-main", page);
        }
        public void ShowResults(RxCareerStatus snapshot, Action continueRace)
        {
            if (snapshot.results_count == 0 || snapshot.results_count > 16 || continueRace == null)
                throw new ArgumentException("Career standings are unavailable.");
            Show("results", "EVENT RESULTS", "MOTOCAREER");
            Frame(out var side, out var main);
            // the local rider's place and points lead the side column
            int mine = -1;
            for (int i = 0; i < snapshot.results_count; i++) if (snapshot.Result(i).player_type == 0) { mine = i; break; }
            if (mine >= 0)
            {
                var me = snapshot.Result(mine);
                int place = me.rank + 1;
                OcUi.Place(side, place, OuWidgets.Ordinal(place));
                var stats = OuUi.Div("oc-stats", side);
                OcUi.Stat(stats, "POINTS", me.points.ToString(), "gold");
                OcUi.Stat(stats, "STATUS", me.dnf != 0 ? "DNF" : "FINISHED", me.dnf != 0 ? null : "good");
            }
            OcUi.BoardHead(main, "POS", "RIDER", "STATUS", null, "POINTS");
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("oc-page-scroll"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; main.Add(scroll);
            var rows = new System.Collections.Generic.List<VisualElement>();
            for (int i = 0; i < snapshot.results_count; i++)
            {
                var value = snapshot.Result(i);
                var row = OcUi.BoardRow(scroll, value.rank + 1, value.Name, value.player_type == 0, value.dnf != 0 ? "DNF" : "FINISHED", null, value.points.ToString());
                row.name = "career-standing-" + i;
                row.RegisterCallback<FocusInEvent>(_ => scroll.ScrollTo(row));
                rows.Add(row);
            }
            OuWidgets.LinkColumn(rows);
            OuMotion.Enter(side, 0.05f, 0.02f, true); OuMotion.Enter(scroll.contentContainer, 0.035f, 0.1f);
            if (snapshot.continue_allowed != 0 && snapshot.continue_submitted == 0)
            {
                var actions = OuUi.Div("oc-actions", side);
                var action = Button("CONTINUE", "career-continue", continueRace, actions);
                // PostEventMenu:Start offers HIGHLIGHT REEL (EventHasReplay) between CONTINUE (the tour's SERIES CONTINUE) and RESTART, in tour mode too
                if (ReelOffered)
                {
                    reelButton = OcUi.Button("HIGHLIGHT REEL", "career-reel", () =>
                    {
                        if (!RenderReady || !armed || committed || Time.frameCount == shownFrame) return;
                        EnterReel();
                    }, actions);
                    OuWidgets.LinkColumn(new System.Collections.Generic.List<VisualElement> { action, reelButton });
                }
                foreach (var r in rows) ((OuNavItem)r.userData).right = action;
                if (rows.Count > 0)
                {
                    ((OuNavItem)action.userData).left = rows[Math.Max(0, mine)];
                    if (reelButton != null) ((OuNavItem)reelButton.userData).left = rows[Math.Max(0, mine)];
                }
                Focus(action);
            }
        }
        // HIGHLIGHT REEL: the world's end-of-race replay (it plays behind the table all along: the Post frames keep running for it) with nothing over
        // it but the legend's BACK (Post Event Highlight Page: PostEventHighlight.lyr has only the legend; the PC build binds no replay controls).
        // The page's elements are hidden, not rebuilt: the results page keeps its epoch, so its CONTINUE is still the painted one.
        bool ReelOffered => replayAvailable != null && replayAvailable();
        public bool ReelShown => reel;
        internal void EnterReel()
        {
            if (!Visible || reel) return;
            reel = true; layer.AddToClassList("oc-reel");
            legend?.RemoveFromHierarchy();
            legend = OcUi.Legend(layer, (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant(), ExitReel));
            OuSound.Play("Accept");
            Debug.Log("[Results] HIGHLIGHT REEL (career)");
            nav.DefaultItem = null;
        }
        internal void ExitReel()
        {
            if (!reel) return;
            reel = false; layer.RemoveFromClassList("oc-reel");
            legend?.RemoveFromHierarchy();
            legend = OcUi.Legend(layer, (OuUi.KeySubmit, "SELECT", null));
            // PostEventHighlight:OnBackKeypress: TravelBack to the post event menu; the focus returns to the entry just left (as the exhibition results do)
            var back = reelButton;
            if (back != null) Focus(back);
        }
        public void ShowRewards(RxCareerStatus snapshot, bool saved, Action returnToCareer)
        {
            if (snapshot.reward_valid != 1) throw new ArgumentException("Career rewards are unavailable.");
            Show("rewards", "EVENT REWARDS", "MOTOCAREER");
            Frame(out var side, out var main);
            var area = OuUi.Div("oc-rewards", main);
            if (snapshot.reward_credits_flag != 0)
            {
                OcUi.Reward(area, "CREDITS", snapshot.reward_base.ToString("N0"), "gold");
                if (snapshot.reward_bonus != 0) OcUi.Reward(area, "BONUS", snapshot.reward_bonus.ToString("N0"), "good");
            }
            Text(saved ? "PROGRESS SAVED" : "SAVING PROGRESS…", "oc-heading", area);
            OuMotion.Enter(area, 0.06f, 0.04f);
            if (saved && returnToCareer != null) Focus(Button("RETURN TO CAREER", "career-return", returnToCareer, OuUi.Div("oc-actions", side)));
        }
        public void ShowStatus(string title, string message, Action retry = null, Action cancel = null, string actionLabel = "RETRY")
        {
            Show("status", title, "MOTOCAREER");
            Frame(out var side, out var main);
            var area = OuUi.Div("oc-rewards", main);
            Text(message, "oc-text oc-text-light", area);
            var actions = OuUi.Div("oc-actions", side);
            if (retry != null) Focus(Button(actionLabel, "career-retry", retry, actions));
            if (cancel != null) { var exit = Button("LEAVE EVENT", "career-cancel", cancel, actions); if (retry == null) Focus(exit); }
        }
        public void Hide()
        {
            if (disposed) return;
            visible = false; renderedEpoch = 0; Page = null; root.AddToClassList("ou-results-hidden");
            input?.Dispose(); input = null;
            RestoreCursor();
        }
        void RestoreCursor()
        {
            if (!cursorLease) return;
            cursorLease = false; Reflex.Port.Platform.ReflexCursor.Want(this, false);
        }
        public void Dispose()
        {
            if (disposed) return;
            Hide(); disposed = true; UnityEngine.Object.Destroy(host);
        }
    }
}
