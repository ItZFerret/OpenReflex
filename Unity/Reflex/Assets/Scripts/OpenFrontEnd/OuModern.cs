// The OPEN REFLEX modern layer's code (Resources/OpenFrontEnd/OuModern.uss is its look):
//   OuLayout   the safe area and the content frame: every full-screen layer (.mxa-layer), the hint bar and the status bar get their
//              insets from the screen's safe area, a title-safe margin and a content width limited to ~2:1, so a 21:9 / 32:9 screen
//              keeps the pages in the middle and a TV's overscan never cuts a title. Recomputed when the screen or UI SCALE changes.
//   OuScale    UI SCALE (Options): the panel settings' reference resolution divided by the scale (every menu document shares it).
//   OuGlyphs   the button names the hints and buttons show: keyboard keys, Xbox or PlayStation buttons, by CONTROLLER GLYPHS.
//   OuMotion   eased entrances (staggered .ou-enter children) and REDUCE MOTION.
//   OuWidgets  the page parts built the same way everywhere: pills, stat tiles, cards, board rows, dialogs.
using System;
using System.Collections.Generic;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OuLayout
    {
        public enum Kind { Layer, Hints, Status }
        /// The widest the content is allowed to be, as width / height (21:9 = 2.33 is centred inside a 2.0 frame).
        public const float MaxAspect = 2.0f;
        /// Reference height of the panels at UI SCALE 100.
        public const float RefHeight = 1080f;
        const float BaseSide = 96f, BaseTop = 44f, BaseBottom = 112f, TitleSafe = 0.025f;

        static readonly List<WeakReference<VisualElement>> tracked = new List<WeakReference<VisualElement>>();

        /// The layout height of the panels now (reference height divided by UI SCALE).
        public static float LayoutHeight => RefHeight * 100f / ReflexSettings.UiScale;

        public static void Track(VisualElement e, Kind kind)
        {
            if (e == null) return;
            tracked.Add(new WeakReference<VisualElement>(e));
            e.userData ??= kind;
            e.RegisterCallback<GeometryChangedEvent>(_ => Apply(e, kind));
            e.RegisterCallback<AttachToPanelEvent>(_ => Apply(e, kind));
        }

        /// Tests only (offscreen renders of a -batchmode player, whose window is not the shot's size): the screen size the frames and the
        /// HUD's safe root are laid out for. Zero = the real screen.
        public static Vector2Int ScreenOverride;
        public static int ScreenWidth => ScreenOverride.x > 0 ? ScreenOverride.x : Screen.width;
        public static int ScreenHeight => ScreenOverride.y > 0 ? ScreenOverride.y : Screen.height;
        public static Rect SafeArea => ScreenOverride.x > 0 ? new Rect(0, 0, ScreenOverride.x, ScreenOverride.y) : Screen.safeArea;

        /// The insets (layout px) of the content frame at the screen's shape: left, top, right, bottom.
        public static Vector4 Insets()
        {
            float sh = Mathf.Max(1, ScreenHeight), sw = Mathf.Max(1, ScreenWidth);
            float layoutH = LayoutHeight, k = layoutH / sh;           // screen px -> layout px
            float width = sw * k;
            Rect safe = SafeArea; if (safe.width <= 0 || safe.height <= 0) safe = new Rect(0, 0, sw, sh);
            float left = safe.xMin * k, right = (sw - safe.xMax) * k, top = (sh - safe.yMax) * k, bottom = safe.yMin * k;
            float frameW = layoutH * MaxAspect, extra = Mathf.Max(0, (width - frameW) * 0.5f);
            float title = TitleSafe * Mathf.Min(width, layoutH * 1.78f);   // 2.5 % of the 16:9 width
            return new Vector4(
                Mathf.Max(BaseSide, left + title + 48f) + extra,
                Mathf.Max(BaseTop, top + title + 8f),
                Mathf.Max(BaseSide, right + title + 48f) + extra,
                Mathf.Max(BaseBottom, bottom + title + 72f));
        }

        public static void Apply(VisualElement e, Kind kind)
        {
            if (e == null || e.panel == null) return;
            var i = Insets();
            switch (kind)
            {
                case Kind.Layer:
                    if (e.ClassListContains("mxa-screen-loading")) { e.style.paddingLeft = e.style.paddingTop = e.style.paddingRight = e.style.paddingBottom = 0; break; }   // the loading picture is full screen
                    e.style.paddingLeft = i.x; e.style.paddingTop = i.y; e.style.paddingRight = i.z; e.style.paddingBottom = i.w;
                    break;
                case Kind.Hints:
                    e.style.right = i.z; e.style.bottom = Mathf.Max(24f, i.w - 88f);
                    EnsureBand(e);
                    break;
                case Kind.Status:
                    e.style.left = i.x; e.style.bottom = Mathf.Max(22f, i.w - 90f);
                    break;
            }
        }

        /// The dark legend band along the bottom (the career pages' look, OuCareer.uss .oc-legend) behind the hint bar and the status bar: a sibling
        /// placed before both, shown while the hint bar is (the career pages hide the shell's bars for their own band).
        static void EnsureBand(VisualElement hints)
        {
            var parent = hints.parent; if (parent == null) return;
            VisualElement band = null; int at = parent.IndexOf(hints);
            foreach (var c in parent.Children())
            {
                if (c.ClassListContains("ou-legend-band")) band = c;
                else if (c.ClassListContains("ou-statusbar")) at = Mathf.Min(at, parent.IndexOf(c));
            }
            if (band == null)
            {
                band = new VisualElement { name = "ou-legend-band", pickingMode = PickingMode.Ignore };
                band.AddToClassList("ou-legend-band");
                parent.Insert(Mathf.Max(0, at), band);
                band.schedule.Execute(() =>
                {   // shown while the hint bar is, and as faded as it is (the hub's entrance fades the bar in by its inline opacity)
                    bool off = hints.panel == null || hints.resolvedStyle.display == DisplayStyle.None;
                    band.style.display = off ? DisplayStyle.None : DisplayStyle.Flex;
                    if (!off) band.style.opacity = hints.resolvedStyle.opacity;
                }).Every(16);
            }
            else if (parent.IndexOf(band) > at) { band.RemoveFromHierarchy(); parent.Insert(Mathf.Max(0, at), band); }
            float bottom = hints.style.bottom.value.value;
            band.style.height = Mathf.Max(84f, bottom + 60f);
        }

        /// Everything tracked again (UI SCALE changed).
        public static void Refresh()
        {
            for (int n = tracked.Count - 1; n >= 0; n--)
            {
                if (!tracked[n].TryGetTarget(out var e)) { tracked.RemoveAt(n); continue; }
                if (e.userData is Kind k) Apply(e, k);
            }
        }

        /// Tracks the element by its classes (the widgets' own Div helpers call this).
        public static void TrackByClass(VisualElement e, string classes)
        {
            if (string.IsNullOrEmpty(classes)) return;
            if (classes.Contains("mxa-layer")) Track(e, Kind.Layer);
            else if (classes.Contains("mxa-hints")) Track(e, Kind.Hints);
            else if (classes.Contains("ou-statusbar")) Track(e, Kind.Status);
        }
    }

    public static class OuScale
    {
        static readonly List<PanelSettings> panels = new List<PanelSettings>();
        static bool hooked;

        /// A menu document's PanelSettings: its reference resolution follows UI SCALE from now on.
        public static void Register(PanelSettings p)
        {
            if (!p) return;
            panels.RemoveAll(x => !x);
            if (!panels.Contains(p)) panels.Add(p);
            Apply(p);
            if (hooked) return;
            hooked = true;
            ReflexSettings.Changed += key => { if (key == null || key == "Options.UiScale") ApplyAll(); };
        }

        public static void Apply(PanelSettings p)
        {
            if (!p) return;
            float s = ReflexSettings.UiScale / 100f;
            p.scaleMode = PanelScaleMode.ScaleWithScreenSize;
            p.referenceResolution = new Vector2Int(Mathf.RoundToInt(1920f / s), Mathf.RoundToInt(1080f / s));
            p.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
            p.match = 1f;
        }

        public static void ApplyAll()
        {
            panels.RemoveAll(x => !x);
            foreach (var p in panels) Apply(p);
            OuLayout.Refresh();
        }
    }

    public static class OuGlyphs
    {
        public enum Device { Keyboard, Xbox, PlayStation }

        /// The device whose buttons are shown: CONTROLLER GLYPHS (Options), AUTO follows the device used last (OuDevice).
        public static Device Current
        {
            get
            {
                switch (ReflexSettings.GlyphStyle)
                {
                    case 1: return Device.Keyboard;
                    case 2: return Device.Xbox;
                    case 3: return Device.PlayStation;
                }
                if (!OuDevice.PadLast) return Device.Keyboard;
                return PadIsPlayStation() ? Device.PlayStation : Device.Xbox;
            }
        }

        /// The pad used last is a DualShock / DualSense (its type, else its name).
        static bool PadIsPlayStation()
        {
            var pad = OuDevice.LastPad;
            if (pad == null) return false;
            if (pad is UnityEngine.InputSystem.DualShock.DualShockGamepad) return true;
            string n = pad.layout + " " + pad.displayName;
            foreach (var k in new[] { "DualSense", "DualShock", "PlayStation", "PS4", "PS5" })
                if (n.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0) return true;
            return false;
        }

        public static string Submit => Current == Device.Keyboard ? "Enter" : Current == Device.Xbox ? "A" : "Cross";
        public static string Cancel => Current == Device.Keyboard ? "Esc" : Current == Device.Xbox ? "B" : "Circle";
        public static string Alt => Current == Device.Keyboard ? "F" : Current == Device.Xbox ? "Y" : "Triangle";
        public static string Find => Current == Device.Keyboard ? "/" : Current == Device.Xbox ? "X" : "Square";
        public static string Tabs => Current == Device.Keyboard ? "Q / E" : Current == Device.Xbox ? "LB / RB" : "L1 / R1";
        public static string Menu => Current == Device.Keyboard ? "Esc" : Current == Device.Xbox ? "Menu" : "Options";
        /// The garage's TEST RIDE: T, the pad's X / Square. A hint stores "T" (Retarget follows the device from it).
        public static string TestRide => Current == Device.Keyboard ? "T" : Current == Device.Xbox ? "X" : "Square";
        public static string Move => Current == Device.Keyboard ? "Arrows" : "D-pad";
        /// The garage's camera: the right stick turns it, the triggers zoom.
        public static string Look => Current == Device.Keyboard ? ", ." : Current == Device.Xbox ? "R Stick" : "R Stick";
        public static string Zoom => Current == Device.Keyboard ? "- +" : Current == Device.Xbox ? "LT / RT" : "L2 / R2";

        // the names of one action on the three devices, to follow the device with a hint that was built for another
        static readonly string[][] Families =
        {
            new[] { "Enter", "A", "Cross" }, new[] { "Esc", "B", "Circle" }, new[] { "F", "Y", "Triangle" }, new[] { "/", "X", "Square" },
            new[] { "Q / E", "LB / RB", "L1 / R1" }, new[] { ", .", "R Stick", "R Stick" }, new[] { "- +", "LT / RT", "L2 / R2" },
            new[] { "T", "X", "Square" },   // the garage's TEST RIDE (keyboard T; after the Find family, so "X" keeps meaning Find)
            new[] { "Esc", "Menu", "Options" },   // the pause (after the Cancel family: "Esc" keeps meaning Cancel; a hint stores "Menu")
        };

        /// The name on the device in use of the action whose name on some device is key (unknown names stay).
        public static string Retarget(string key)
        {
            if (key == null) return key;
            int d = (int)Current;
            foreach (var f in Families) if (Array.IndexOf(f, key) >= 0) return f[d];
            return key;
        }

        /// The style class of a pad button's cap ("mxa-key-a" ...), null for a keyboard key.
        public static string CapClass(string key)
        {
            switch (key)
            {
                case "A": case "Cross": return "mxa-key-pad mxa-key-a";
                case "B": case "Circle": return "mxa-key-pad mxa-key-b";
                case "X": case "Square": return "mxa-key-pad mxa-key-x";
                case "Y": case "Triangle": return "mxa-key-pad mxa-key-y";
                case "LB / RB": case "L1 / R1": case "LT / RT": case "L2 / R2": return "mxa-key-pad mxa-key-shoulder";
                case "Menu": case "Options": case "D-pad": case "R Stick": return "mxa-key-pad mxa-key-menu";
            }
            return null;
        }
    }

    public static class OuMotion
    {
        public static bool Reduced => ReflexSettings.ReduceMotion;

        /// Marks the children of parent for an eased entrance (a lift and a fade, staggered); one frame later they settle.
        public static void Enter(VisualElement parent, float stagger = 0.045f, float first = 0.02f, bool fromLeft = false)
        {
            if (parent == null) return;
            if (Reduced) return;
            int n = 0;
            foreach (var child in parent.Children()) Enter(child, first + stagger * n++, fromLeft);
        }

        public static void Enter(VisualElement e, float delay, bool fromLeft = false)
        {
            if (e == null || Reduced) return;
            e.AddToClassList("ou-enter");
            if (fromLeft) e.AddToClassList("ou-enter-left");
            e.style.transitionDelay = new List<TimeValue> { new TimeValue(delay, TimeUnit.Second), new TimeValue(delay, TimeUnit.Second) };
            e.schedule.Execute(() => e.AddToClassList("ou-in")).ExecuteLater(30);
        }

        public static void Fade(VisualElement e, float delay = 0f)
        {
            if (e == null || Reduced) return;
            e.AddToClassList("ou-fade");
            e.style.transitionDelay = new List<TimeValue> { new TimeValue(delay, TimeUnit.Second) };
            e.schedule.Execute(() => e.AddToClassList("ou-in")).ExecuteLater(30);
        }

        /// The root's REDUCE MOTION class (transitions off).
        public static void Sync(VisualElement root) { if (root != null) root.EnableInClassList("ou-reduce-motion", Reduced); }
    }

    /// The page parts every modern screen is built from.
    public static class OuWidgets
    {
        public static Label Pill(VisualElement parent, string text, string kind = null)
        {
            var l = OuUi.Text(text, "ou-pill" + (string.IsNullOrEmpty(kind) ? "" : " ou-pill-" + kind), parent);
            l.enableRichText = false;
            return l;
        }

        /// A label over a big value; kind "good" (green), "gold".
        public static VisualElement Stat(VisualElement parent, string label, string value, string kind = null)
        {
            var s = OuUi.Div("ou-stat" + (string.IsNullOrEmpty(kind) ? "" : " ou-stat-" + kind), parent);
            OuUi.Text(label, "ou-stat-label", s).enableRichText = false;
            var v = OuUi.Text(value, "ou-stat-value ou-display", s); v.enableRichText = false;
            return s;
        }

        /// Stat tiles in explicit rows of perRow equal tiles (no flex-wrap): a page column that stacks tiles over a list must know the tiles' height, and a
        /// wrapped row container measured one line where its tiles wrapped onto two, so the list under it was drawn over the wrapped tile. The grid's height is
        /// its rows' own; the look is OuRacePause.uss (.ou-tile-grid).
        public static VisualElement StatGrid(VisualElement parent, IEnumerable<(string label, string value, string kind)> tiles, int perRow = 2)
        {
            var grid = OuUi.Div("ou-tile-grid", parent);
            VisualElement row = null; int n = 0;
            foreach (var t in tiles)
            {
                if (row == null || n % perRow == 0) row = OuUi.Div("ou-tile-row", grid);
                Stat(row, t.label, t.value, t.kind); n++;
            }
            return grid;
        }

        /// The big place number with its suffix: "1" "ST".
        public static VisualElement Place(VisualElement parent, int place, string suffix)
        {
            var p = OuUi.Div("ou-place" + (place >= 1 && place <= 3 ? " ou-place-" + place : ""), parent);
            OuUi.Text(place.ToString(), "ou-place-num ou-display", p);
            OuUi.Text(suffix, "ou-place-suffix ou-display", p);
            return p;
        }

        public static string Ordinal(int n)
        {
            int t = n % 100;
            if (t >= 11 && t <= 13) return "TH";
            switch (n % 10) { case 1: return "ST"; case 2: return "ND"; case 3: return "RD"; default: return "TH"; }
        }

        /// A navigable card: title, a line under it and optional pills on the right; submit on Enter / A / click.
        public static VisualElement Card(VisualElement parent, string title, string sub, Action submit, Texture2D thumb = null)
        {
            var card = OuUi.Item(OuUi.Div("ou-card2", parent), submit);
            if (thumb) { var t = OuUi.Div("ou-card2-thumb", card); t.style.backgroundImage = thumb; }
            var copy = OuUi.Div("ou-card2-copy", card);
            OuUi.Text(title, "ou-card2-title ou-display", copy).enableRichText = false;
            if (!string.IsNullOrEmpty(sub)) OuUi.Text(sub, "ou-card2-sub ou-label", copy).enableRichText = false;
            OuUi.Div("ou-card2-tags", card);
            return card;
        }

        public static VisualElement Tags(VisualElement card) => card.Q<VisualElement>(className: "ou-card2-tags");

        public sealed class BoardCols
        {
            public string rank = "POS", name = "RIDER", status, time, best, points;
        }

        /// A board's heading line for the columns given.
        public static VisualElement BoardHead(VisualElement parent, BoardCols c)
        {
            var head = OuUi.Div("ou-board-head", parent);
            OuUi.Text(c.rank, "ou-board-rank", head); OuUi.Text(c.name, "ou-board-name ou-board-cell", head);
            if (c.status != null) OuUi.Text(c.status, "ou-board-status ou-board-cell", head);
            if (c.time != null) OuUi.Text(c.time, "ou-board-time ou-board-cell", head);
            if (c.best != null) OuUi.Text(c.best, "ou-board-best ou-board-cell", head);
            if (c.points != null) OuUi.Text(c.points, "ou-board-points ou-board-cell", head);
            return head;
        }

        /// One board row (rank slab, name, optional status / time / best / points); navigable so a controller scrolls the field.
        public static VisualElement BoardRow(VisualElement parent, int rank, string name, bool you, string status = null, string time = null, string best = null, string points = null)
        {
            var row = OuUi.Item(OuUi.Div("ou-board-row" + (you ? " ou-board-you" : ""), parent), null);
            var r = OuUi.Text(rank > 0 ? rank.ToString() : "-", "ou-board-rank ou-display" + (rank >= 1 && rank <= 3 ? " ou-board-rank-" + rank : ""), row);
            r.enableRichText = false;
            OuUi.Text(name, "ou-board-name ou-board-cell", row).enableRichText = false;
            if (status != null) OuUi.Text(status, "ou-board-status ou-board-cell", row).enableRichText = false;
            if (time != null) OuUi.Text(time, "ou-board-time ou-board-cell", row).enableRichText = false;
            if (best != null) OuUi.Text(best, "ou-board-best ou-board-cell", row).enableRichText = false;
            if (points != null) OuUi.Text(points, "ou-board-points ou-board-cell", row).enableRichText = false;
            return row;
        }

        /// Links a list of rows vertically so up / down walk them and stop at the ends.
        public static void LinkColumn(IList<VisualElement> items)
        {
            for (int i = 0; i < items.Count; i++)
            {
                var it = (OuNavItem)items[i].userData;
                if (i > 0) it.up = items[i - 1];
                if (i + 1 < items.Count) it.down = items[i + 1];
            }
        }

        /// Links a row of buttons horizontally (left / right stop at the ends).
        public static void LinkRow(IList<VisualElement> items)
        {
            for (int i = 0; i < items.Count; i++)
            {
                var it = (OuNavItem)items[i].userData;
                it.left = items[Mathf.Max(0, i - 1)]; it.right = items[Mathf.Min(items.Count - 1, i + 1)];
            }
        }

        /// A dialog: a scrim and a centred card over the layer; returns the card (children go into it).
        public static VisualElement Dialog(VisualElement parent, string kind = null)
        {
            OuUi.Div("ou-scrim", parent);
            var wrap = OuUi.Div("ou-dialog-wrap", parent);
            var card = OuUi.Div("ou-dialog" + (string.IsNullOrEmpty(kind) ? "" : " ou-dialog-" + kind), wrap);
            return card;
        }

        /// m:ss.cc (a time that was never set = dashes).
        public static string Clock(float t) => t <= 0f || t >= 3.0e38f || float.IsNaN(t) ? "--:--.--" : $"{(int)(t / 60f):00}:{t - (int)(t / 60f) * 60f:00.00}";
    }
}
