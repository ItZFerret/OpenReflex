// The menus' navigation (one for every menu, see OuInput.cs for the input): drives the focus of one screen (the root holding its items)
// from an OuMenuInput. Every selectable element is a focusable VisualElement with an OuNavItem in userData and the class mxa-item.
//   - up / down / left / right move to the nearest item in that direction (up / down stay in the column unless the item is alone
//     in it, left / right take the nearest on that side); an item's explicit up / down / left / right link wins;
//     on a value row (adjust set) left / right change the value instead
//   - a screen never loses its focus: when the focused item goes away (a page rebuilt, a list refilled, a dialog closed) the focus
//     comes back to DefaultItem or the first item within a few frames, with or without input
//   - Modal limits the focus to a dialog's subtree (nothing behind it can be reached), Remember / Restore keep the place across a rebuild
//   - the triggers page through a long list (OnPage replaces that), the bumpers are the screen's tabs (OnTab)
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public class OuNavItem
    {
        public Action submit;
        /// Left / right on a value row (-1 / +1); null = the item moves focus spatially instead.
        public Action<int> adjust;
        /// Explicit neighbours where the nearest item in that direction is the wrong one; null = spatial.
        public VisualElement up, down, left, right;
    }

    /// Drives the navigation of one screen (the root holding its items) from OuMenuInput.
    public class OuNav
    {
        public VisualElement Screen;
        /// Called for Cancel; return true when handled.
        public Func<bool> OnCancel;
        public Action<int> OnTab;
        /// The extra action (Triangle / Y, F) and the search shortcut (Square / X, / or Tab); null = none.
        public Action OnAlt, OnFind;
        /// The triggers (-1 / +1); null = page the focus through the items.
        public Action<int> OnPage;
        /// The item to focus when nothing on the screen has the focus.
        public Func<VisualElement> DefaultItem;
        /// Input goes elsewhere (a binding is listening): the nav does nothing.
        public Func<bool> Suspended;
        /// A dialog: only the items inside it can be focused or reached; the focus is pulled in when it is set.
        public VisualElement Modal
        {
            get => _modal;
            set { _modal = value; _lost = 0; if (value != null) FocusFirstIn(value); }
        }
        /// Items one trigger press moves by.
        public const int PageStep = 6;

        VisualElement _modal;
        int _lost;

        protected virtual void Play(string sound) => OuSound.Play(sound);

        VisualElement Scope => _modal ?? Screen;

        public VisualElement Focused
        {
            get
            {
                var f = Screen?.focusController?.focusedElement as VisualElement;
                return f != null && f.userData is OuNavItem && IsOn(f) ? f : null;
            }
        }

        /// Whether e can take the focus now: enabled, shown all the way up to the screen, and inside the modal when there is one.
        public bool IsOn(VisualElement e)
        {
            if (Screen == null || e == null || !e.enabledInHierarchy) return false;
            bool inModal = _modal == null;
            for (var p = e; p != null; p = p.parent)
            {
                if (p.resolvedStyle.display == DisplayStyle.None || p.resolvedStyle.visibility == Visibility.Hidden) return false;
                if (p == _modal) inModal = true;
                if (p == Screen) return inModal;
            }
            return false;
        }

        public void Focus(VisualElement e)
        {
            if (e == null) return;
            OuUi.NoteNavigation();   // (a hover may not take the focus back from this move until the pointer really travels)
            e.Focus();
            Follow(e);
        }

        // ------------------------------------------------------------------------------------------------ the focus stays in sight

        VisualElement _follow, _lastFocused; float _followUntil;
        readonly List<(ScrollView sv, int edge)> _edges = new List<(ScrollView, int)>();
        /// How long (seconds) a focused item is held in view after it got the focus: a page that was just built, shown or filled has its layout
        /// (and so its scroll range) a frame or two later, and a first ScrollTo against the old layout does nothing.
        const float FollowTime = 0.35f;

        void Follow(VisualElement e)
        {
            _follow = e; _followUntil = Time.unscaledTime + FollowTime;
            // the first / last item of a scrolling list shows the list's start / end (a heading above the first row, a note under the last)
            _edges.Clear();
            for (var p = e.parent; p != null; p = p.parent)
            {
                if (!(p is ScrollView sv) || !sv.contentContainer.Contains(e)) continue;
                int edge = 0;
                if (Mathf.Max(sv.verticalScroller.highValue, sv.horizontalScroller.highValue) > 1f)
                {
                    VisualElement first = null, last = null;
                    sv.contentContainer.Query<VisualElement>(className: "mxa-item").ForEach(x => { if (x.userData is OuNavItem && IsOn(x)) { first ??= x; last = x; } });
                    edge = first == e ? -1 : last == e ? 1 : 0;
                }
                _edges.Add((sv, edge));
            }
            Reveal(e, _edges);
        }

        /// Scrolls every ScrollView above e (nested ones too, vertical and horizontal) just far enough to show e whole, with a margin so it is
        /// not flush at the edge (the next / previous row shows a little). An item taller than the viewport gets its top edge.
        public static void Reveal(VisualElement e) => Reveal(e, null);

        static void Reveal(VisualElement e, List<(ScrollView sv, int edge)> edges)
        {
            if (e == null || e.panel == null) return;
            for (var p = e.parent; p != null; p = p.parent)
            {
                if (!(p is ScrollView sv) || !sv.contentContainer.Contains(e)) continue;
                int edge = 0;
                if (edges != null) foreach (var x in edges) if (x.sv == sv) { edge = x.edge; break; }
                RevealIn(sv, e, edge);
            }
        }

        static void RevealIn(ScrollView sv, VisualElement e, int edge)
        {
            var vp = sv.contentViewport;
            if (vp == null || vp.resolvedStyle.display == DisplayStyle.None) return;
            Rect v = vp.worldBound, b = e.worldBound;
            if (float.IsNaN(v.width) || float.IsNaN(b.width) || v.width < 1f || v.height < 1f || b.width < 0.5f || b.height < 0.5f) return;   // (not laid out yet: Follow tries again)
            var was = sv.scrollOffset; var off = was;
            if (sv.mode != ScrollViewMode.Horizontal) off.y += RevealDelta(b.yMin, b.yMax, v.yMin, v.yMax, edge, was.y, sv.verticalScroller.highValue);
            if (sv.mode != ScrollViewMode.Vertical) off.x += RevealDelta(b.xMin, b.xMax, v.xMin, v.xMax, edge, was.x, sv.horizontalScroller.highValue);
            if ((off - was).sqrMagnitude > 0.01f) sv.scrollOffset = off;
        }

        /// How far the content must move (in scroll offset units) to show [lo, hi] inside [vlo, vhi] with a margin. edge -1 / +1: the item is the
        /// first / last of the list, so the list's start / end (offset 0 / high) is shown instead whenever the item stays whole in view with it.
        static float RevealDelta(float lo, float hi, float vlo, float vhi, int edge, float offset, float high)
        {
            float size = hi - lo, span = vhi - vlo;
            float m = Mathf.Min(Mathf.Clamp(size * 0.3f, 6f, 40f), span * 0.15f);
            if (edge < 0 && offset > 0.5f) { if (hi + offset <= vhi - m) return -offset; }   // (the content moves down by `offset`: the item goes with it)
            else if (edge > 0 && offset < high - 0.5f) { float d = high - offset; if (lo - d >= vlo + m) return d; }
            if (size + 2f * m > span)
                return lo <= vlo && hi >= vhi ? 0f : lo - vlo;   // taller than the viewport: its top edge (unless it already fills it)
            if (lo < vlo + m) return lo - (vlo + m);
            if (hi > vhi - m) return hi - (vhi - m);
            return 0f;
        }

        void FocusFirstIn(VisualElement scope)
        {
            foreach (var e in Items()) if (scope.Contains(e)) { Focus(e); return; }
        }

        public List<VisualElement> Items()
        {
            var list = new List<VisualElement>();
            Scope?.Query<VisualElement>(className: "mxa-item").ForEach(e => { if (e.userData is OuNavItem && IsOn(e)) list.Add(e); });
            return list;
        }

        // ------------------------------------------------------------------------------------------------ focus memory

        /// Where the focus is, to put it back after the screen is rebuilt: the item's name, else its place in the list.
        public struct Place { public string name; public int index; public bool valid; }
        public Place Remember()
        {
            var f = Focused;
            if (f == null) return default;
            return new Place { name = f.name, index = Items().IndexOf(f), valid = true };
        }
        /// Focuses what Remember saw (by name, else the same place, clamped), else fallback, else the default item. Returns it.
        public VisualElement Restore(Place place, VisualElement fallback = null)
        {
            VisualElement target = null;
            var items = Items();
            if (place.valid)
            {
                if (!string.IsNullOrEmpty(place.name)) target = items.Find(e => e.name == place.name);
                if (target == null && place.index >= 0 && items.Count > 0) target = items[Mathf.Min(place.index, items.Count - 1)];
            }
            target ??= fallback != null && IsOn(fallback) ? fallback : null;
            target ??= DefaultOrFirst();
            Focus(target);
            return target;
        }

        VisualElement DefaultOrFirst()
        {
            var d = DefaultItem?.Invoke();
            if (d != null && IsOn(d)) return d;
            var items = Items();
            return items.Count > 0 ? items[0] : null;
        }

        // ------------------------------------------------------------------------------------------------ one frame

        /// One frame of input.
        public virtual void Update(OuMenuInput input)
        {
            // a screen that is not on show (its root hidden, another screen up) neither moves nor takes the focus: every menu document of a
            // panel shares one focus, and a hidden screen's "nothing is focused: take the default item" stole the focus of the screen on top
            if (Screen == null || Screen.panel == null || !IsShown) { input.Step(); return; }
            if (Suspended != null && Suspended()) { input.Step(); return; }
            KeepInSight();
            ScrollText(input);
            // typing into a text field: leave the keyboard to it (the gamepad still navigates; Q / E type, not switch tabs)
            var focusedNow = Screen.focusController?.focusedElement;
            bool typing = IsTextFocus(focusedNow);
            if (typing && !OuMenuInput.GamepadLast && !input.injectSubmit && !input.injectCancel && input.injectStep == Vector2Int.zero) { input.Step(); return; }
            int tab = input.Tab, page = input.Page;
            bool find = input.Find, alt = input.Alt;
            if (typing) { tab = page = 0; find = alt = false; }   // (a Q, E, F, / or Tab typed into a box on the keyboard is a letter, not the tabs, the extra action or the search)
            if (tab != 0 && OnTab != null) { Play("LeftRight"); OnTab(tab); }
            var cur = Focused;
            if (find && OnFind != null) { Play("Accept"); OnFind(); return; }
            if (alt && OnAlt != null) { Play("LeftRight"); OnAlt(); return; }
            Vector2Int d = input.Step();
            bool submit = input.Submit, cancel = input.Cancel;
            if (cur == null && ForeignFocus()) return;   // another screen's item has the focus: its own navigation answers
            // the pad's A with a text box in the hands is the box's row answering (the garage's opens the on-screen keyboard), not "nothing is
            // focused": that refocus below took the focus out of the box, so A ended the typing like B did and the box could not be edited
            if (cur == null && submit && typing)
            {
                for (var p = focusedNow as VisualElement; p != null; p = p.parent)
                    if (p.userData is OuNavItem owner) { if (owner.submit != null) { Play("Accept"); owner.submit(); } break; }
                return;
            }
            if (cur == null && (d != Vector2Int.zero || submit || page != 0))
            {
                Focus(DefaultOrFirst());
                return;
            }
            if (cancel) { if (OnCancel != null && OnCancel()) Play("Back"); return; }
            if (cur == null) { KeepFocus(); return; }
            _lost = 0;
            var item = (OuNavItem)cur.userData;
            if (submit) { if (item.submit != null) { Play("Accept"); item.submit(); } return; }
            if (page != 0)
            {
                if (OnPage != null) OnPage(page);
                else PageFocus(cur, page);
                return;
            }
            if (d.x != 0 && item.adjust != null) { Play("LeftRight"); item.adjust(d.x); return; }
            if (d != Vector2Int.zero)
            {
                OuUi.NoteNavigation();
                var next = Neighbour(cur, d);
                if (next != null && next != cur) { Play("UpDown"); Focus(next); }   // (a link to itself = the end of a list: stay, quietly)
            }
        }

        // ------------------------------------------------------------------------------------------------ text that scrolls

        /// A ScrollView with nothing in it to focus (instructions, a log, a long description) that the right stick scrolls; null = the first
        /// one of the screen that can move.
        public ScrollView TextScroll;
        ScrollView _textFound; float _textProbeAt = -10f;
        /// Scroll speed of the right stick on a text, in layout pixels a second at full tilt.
        const float TextScrollSpeed = 900f;

        ScrollView FindTextScroll()
        {
            if (Time.unscaledTime - _textProbeAt < 0.5f && (_textFound == null || _textFound.panel != null)) return _textFound;
            _textProbeAt = Time.unscaledTime; _textFound = null;
            foreach (var sv in Scope.Query<ScrollView>().ToList())
            {
                if (sv.resolvedStyle.display == DisplayStyle.None || sv.verticalScroller.highValue <= 4f || sv.contentContainer.Q(className: "mxa-item") != null) continue;
                bool shown = true; for (VisualElement p = sv; p != null && shown; p = p.parent) shown = p.resolvedStyle.display != DisplayStyle.None && p.resolvedStyle.visibility != Visibility.Hidden;
                if (shown) { _textFound = sv; break; }
            }
            return _textFound;
        }

        void ScrollText(OuMenuInput input)
        {
            float y = input.LookPeek.y;
            if (Mathf.Abs(y) < 0.3f) return;
            var sv = TextScroll ?? FindTextScroll();
            if (sv == null || sv.panel == null) return;
            var o = sv.scrollOffset; o.y = Mathf.Clamp(o.y - y * TextScrollSpeed * Time.unscaledDeltaTime, 0f, sv.verticalScroller.highValue);
            sv.scrollOffset = o;
        }

        /// Whoever put the focus on an item (a screen's own callback, a poster, a refocus after a rebuild; not the mouse passing over it), the
        /// focused item is scrolled into view, and kept there while the layout settles.
        void KeepInSight()
        {
            var f = Focused;
            if (f != _lastFocused)
            {
                _lastFocused = f;
                Reflex.Port.Platform.ReflexMenuPerf.Note("focus");
                if (f != null && !OuUi.FocusFromHover(f)) Follow(f);
            }
            if (_follow != null && Time.unscaledTime < _followUntil && _follow == f) Reveal(f, _edges);
        }

        /// Where a direction from cur leads: the item's own link, else the nearest item that way (null: the edge).
        public VisualElement Neighbour(VisualElement cur, Vector2Int d)
        {
            if (cur?.userData is not OuNavItem item) return null;
            var link = d.y > 0 ? item.up : d.y < 0 ? item.down : d.x > 0 ? item.right : item.left;
            return link != null && link.userData is OuNavItem && IsOn(link) && (_modal == null || _modal.Contains(link)) ? link : Nearest(cur, d);
        }

        // ------------------------------------------------------------------------------------------------ tests

        static readonly List<WeakReference<OuNav>> s_live = new List<WeakReference<OuNav>>();
        public OuNav() { s_live.Add(new WeakReference<OuNav>(this)); }
        /// The navigations alive, newest last (the showcase's walker).
        public static List<OuNav> Live()
        {
            var list = new List<OuNav>();
            for (int i = s_live.Count - 1; i >= 0; i--) { if (s_live[i].TryGetTarget(out var n)) list.Insert(0, n); else s_live.RemoveAt(i); }
            return list;
        }

        /// The screen is attached and shown all the way up (not behind a hidden root).
        public bool IsShown
        {
            get
            {
                if (Screen == null || Screen.panel == null) return false;
                for (var p = Screen; p != null; p = p.parent)
                    if (p.resolvedStyle.display == DisplayStyle.None || p.resolvedStyle.visibility == Visibility.Hidden) return false;
                return true;
            }
        }

        /// Nothing is focused and nothing was pressed: after a few frames (a rebuilt page needs its layout) the default item takes it.
        /// An item that is not this screen's has the focus (another menu document of the same panel).
        bool ForeignFocus()
        {
            var f = Screen?.focusController?.focusedElement as VisualElement;
            return f != null && f.userData is OuNavItem && !Screen.Contains(f);
        }

        /// A text box has the focus: the TextField itself (what a Focus() on it gives) or its input element (a click in it).
        static bool IsTextFocus(Focusable f) => f is TextField || f is TextElement { parent: TextField };

        void KeepFocus()
        {
            if (++_lost < 3 || _lost % 3 != 0 || ForeignFocus()) return;
            if (Screen?.focusController?.focusedElement is VisualElement typed && IsTextFocus(typed) && Screen.Contains(typed)) return;   // (a text box being typed in has the focus: it is not lost)
            var target = DefaultOrFirst();
            if (target != null) Focus(target);
        }

        void PageFocus(VisualElement cur, int page)
        {
            var items = Items();
            // a list scrolls under the triggers too: the column is the list's own (the footer button under it is not a row of it)
            for (var p = cur.parent; p != null; p = p.parent)
                if (p is ScrollView sv && sv.contentContainer.Contains(cur)) { var inside = items.FindAll(e => sv.contentContainer.Contains(e)); if (inside.Count >= 2) items = inside; break; }
            if (items.Count < 2) return;
            // the column the focus is in, top to bottom
            var col = items.FindAll(e => Mathf.Abs(e.worldBound.center.x - cur.worldBound.center.x) < Mathf.Max(24f, cur.worldBound.width * 0.5f));
            col.Sort((a, b) => a.worldBound.center.y.CompareTo(b.worldBound.center.y));
            int at = col.IndexOf(cur);
            if (at < 0) return;
            int to = Mathf.Clamp(at + page * PageStep, 0, col.Count - 1);
            if (to == at) return;
            Play("UpDown"); Focus(col[to]);
        }

        /// The nearest item from cur in direction d (screen space: +y = up). Up / down stay in the column (items overlapping in x)
        /// unless the item is alone in it; left / right take the nearest item on that side.
        public VisualElement Nearest(VisualElement cur, Vector2Int d)
        {
            var items = Items();
            // Inside the scroll views cur sits in first (innermost out): a list row's next row is the next row of the list, however far below
            // the fold it is. Judged on the screen alone, the footer button under the list (DONE, USE, TEST RIDE) is nearer than a row that is
            // cut off by the list's edge, with a section heading between rows it is nearer even when the row would be on screen, and the pad
            // would jump out of the list instead of scrolling it.
            for (var p = cur.parent; p != null; p = p.parent)
            {
                if (!(p is ScrollView sv) || !sv.contentContainer.Contains(cur)) continue;
                var inside = NearestAmong(cur, d, items, e => sv.contentContainer.Contains(e), true);
                if (inside != null) return inside;
            }
            return NearestAmong(cur, d, items, null, false);
        }

        VisualElement NearestAmong(VisualElement cur, Vector2Int d, List<VisualElement> items, Func<VisualElement, bool> only, bool inner)
        {
            Rect a = cur.worldBound;
            Vector2 dir = new Vector2(d.x, -d.y); // UI Toolkit y grows downwards
            VisualElement best = null, relaxed = null, stacked = null, sibling = null;
            bool columnPeer = false;
            float bestScore = float.MaxValue, relaxedScore = float.MaxValue, stackedScore = float.MaxValue, siblingScore = float.MaxValue;
            foreach (var e in items)
            {
                if (e == cur || only != null && !only(e)) continue;
                Rect b = e.worldBound;
                if (b.xMin < a.xMax && a.xMin < b.xMax) columnPeer = true;
                float along, across, centerOff;
                if (dir.x != 0)
                {
                    along = dir.x > 0 ? b.xMin - a.xMax : a.xMin - b.xMax;
                    across = Mathf.Max(0, Mathf.Max(b.yMin - a.yMax, a.yMin - b.yMax));
                    if (dir.x > 0 ? b.center.x <= a.center.x + 1 : b.center.x >= a.center.x - 1) continue;
                    centerOff = Mathf.Abs(b.center.y - a.center.y);
                    // an item stacked under / over this one (most of its width shared: the hub's small chips under the big buttons) is up / down
                    // territory, not "beside": it only takes the right / left when nothing is really beside (so a chip does not take the right of
                    // a button whose neighbour on that side is further away, but the garage's DONE stays reachable from a list row)
                    if (Mathf.Min(a.xMax, b.xMax) - Mathf.Max(a.xMin, b.xMin) > 0.5f * Mathf.Min(a.width, b.width))
                    {
                        float st = Mathf.Max(0, along) + across * 3f + centerOff * 0.1f;
                        if (st < stackedScore) { stackedScore = st; stacked = e; }
                        continue;
                    }
                }
                else
                {
                    along = dir.y > 0 ? b.yMin - a.yMax : a.yMin - b.yMax;
                    across = Mathf.Max(0, Mathf.Max(b.xMin - a.xMax, a.xMin - b.xMax));
                    if (dir.y > 0 ? b.center.y <= a.center.y + 1 : b.center.y >= a.center.y - 1) continue;
                    centerOff = Mathf.Abs(b.center.x - a.center.x);
                }
                float score = Mathf.Max(0, along) + across * 3f + centerOff * 0.1f;
                if (dir.x == 0 && across > 0)
                {   // not in the column: a fallback so up / down are never dead, but anything in the column wins
                    float far = Mathf.Max(0, along) + centerOff * 1.5f;
                    if (far < relaxedScore) { relaxedScore = far; relaxed = e; }
                    // a grid's short last row: down from a card with nothing under it goes to the nearest card of the next row (a sibling)
                    if (e.parent == cur.parent && along >= -4f && far < siblingScore) { siblingScore = far; sibling = e; }   // (a whole row further: not the card beside it, which a focus lift puts a pixel lower)
                    continue;
                }
                if (score < bestScore) { bestScore = score; best = e; }
            }
            // an item alone in its column (a lone button beside a list) would have no up / down at all: it may reach what is beyond
            if (inner) return best ?? (dir.y != 0 && columnPeer ? sibling : null);   // (inside a list only a real neighbour counts: the stacked / relaxed fallbacks are for leaving it)
            return best ?? (dir.x != 0 ? stacked : columnPeer ? sibling : relaxed);
        }
    }
}
