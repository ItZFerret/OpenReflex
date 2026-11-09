// TEST ONLY (w-padscroll2): the scroll walk. Every scrollable container of a screen (a ScrollView that can move, either axis) is walked with
// the synthetic pad: the focus starts on the container's first item, then DOWN (RIGHT in a horizontal strip) until nothing moves, a
// side step into the next column of a grid and back up, and the whole trip again in the other direction. At every step the focused item
// must be at least 85% inside the viewports of all the scroll views it sits in (both axes) and the panel; every item must be visited;
// a held stick must keep moving without the focus jumping back; items that are clipped and sit in nothing that scrolls are reported.
//   REFLEX_UI_SCROLLWALK=1 : runs after every screenshot of every page (--pages ...), like REFLEX_UI_WALK
//   page nav-scrollall      : the shell screens that need an opening call (browser grid / list, setup, options tabs, physics studio, ...)
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static partial class UiShowcaseNav
    {
        public static int ScrollLists, ScrollSteps, ScrollWarnings;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void RegisterScroll()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_UI_SCROLLWALK") == "1") UiShowcase.AfterShot = ScrollWalk;
            UiShowcase.Register("nav-scrollall", ScrollAll);
        }

        static float Range(ScrollView sv, bool vertical)
        {
            var s = vertical ? sv.verticalScroller : sv.horizontalScroller;
            if (sv.mode == ScrollViewMode.Vertical && !vertical) return 0f;
            if (sv.mode == ScrollViewMode.Horizontal && vertical) return 0f;
            return Mathf.Max(0f, s.highValue - s.lowValue);
        }

        static void Axis(Rect b, Rect v, ref float fx, ref float fy)
        {
            float ox = Mathf.Min(b.xMax, v.xMax) - Mathf.Max(b.xMin, v.xMin), oy = Mathf.Min(b.yMax, v.yMax) - Mathf.Max(b.yMin, v.yMin);
            fx = Mathf.Min(fx, Mathf.Clamp01(ox / Mathf.Max(1f, Mathf.Min(b.width, v.width))));
            fy = Mathf.Min(fy, Mathf.Clamp01(oy / Mathf.Max(1f, Mathf.Min(b.height, v.height))));
        }

        /// How much of e is seen: the smaller of the shares on both axes inside the panel and every scroll view viewport above it
        /// (a part taller than the viewport only needs the viewport filled).
        static float Seen(VisualElement e)
        {
            var b = e.worldBound; if (b.width <= 0.5f || b.height <= 0.5f || e.panel == null) return 1f;
            float fx = 1f, fy = 1f;
            Axis(b, e.panel.visualTree.worldBound, ref fx, ref fy);
            for (var p = e.parent; p != null; p = p.parent) if (p is ScrollView sv) Axis(b, sv.contentViewport.worldBound, ref fx, ref fy);
            return Mathf.Min(fx, fy);
        }

        static OuNav NavOf(VisualElement item)
        {
            foreach (var n in OuNav.Live()) if (n.IsShown && n.Items().Contains(item)) return n;
            return null;
        }

        static GamepadButton Opp(GamepadButton b) => b == GamepadButton.DpadDown ? GamepadButton.DpadUp : b == GamepadButton.DpadUp ? GamepadButton.DpadDown : b == GamepadButton.DpadLeft ? GamepadButton.DpadRight : GamepadButton.DpadLeft;

        static string Where(ScrollView sv) => (string.IsNullOrEmpty(sv.name) ? "(" + string.Join(".", sv.GetClasses().Take(2)) + ")" : sv.name) + " y" + sv.scrollOffset.y.ToString("F0") + "/" + Range(sv, true).ToString("F0") + " x" + sv.scrollOffset.x.ToString("F0") + "/" + Range(sv, false).ToString("F0");

        static void Warn(string screen, string what) { ScrollWarnings++; Debug.LogWarning(Tag + "SCROLL WARN [" + screen + "] " + what); }

        /// All the scroll views of the screen on top that can move, and the items clipped in nothing that scrolls (a carousel).
        public static IEnumerator ScrollWalk(string screen)
        {
            Devices();
            for (int i = 0; i < 40 && !Look().anyItems; i++) yield return null;
            for (int i = 0; i < 40 && Look().focused == null; i++) yield return null;
            for (int i = 0; i < 20; i++) yield return null;
            var p = Look();
            if (!p.anyItems) { Debug.Log(Tag + "scroll [" + screen + "] no items (a display page)"); yield break; }
            var root = p.doc.rootVisualElement;
            // the focus a screen opens with is in view once the page has settled
            if (p.focused != null) { float s0 = Seen(p.focused); Debug.Log(Tag + "scroll [" + screen + "] opening focus " + Label(p.focused) + " seen " + (s0 * 100).ToString("F0") + "%"); Check(s0 >= 0.85f, screen, "the item the screen opened with (" + Label(p.focused) + ") is only " + (s0 * 100).ToString("F0") + "% in view"); }
            var all = root.Query<ScrollView>().ToList().Where(sv => Shown(sv)).ToList();
            var moving = all.Where(sv => Range(sv, true) > 4f || Range(sv, false) > 4f).ToList();
            Debug.Log(Tag + "scroll [" + screen + "] " + p.items + " items, " + all.Count + " scroll views, " + moving.Count + " that move" + (moving.Count > 0 ? ": " + string.Join(" | ", moving.Select(Where)) : ""));
            // items that are clipped but in nothing that scrolls (a carousel, a list cut off by its panel): the pad must bring each into view
            var clipped = new List<VisualElement>();
            foreach (var e in root.Query<VisualElement>(className: "mxa-item").ToList())
            {
                if (!(e.userData is OuNavItem) || !Shown(e) || !e.enabledInHierarchy) continue;
                bool inMoving = false; for (var q = e.parent; q != null; q = q.parent) if (q is ScrollView sv && moving.Contains(sv)) { inMoving = true; break; }
                if (!inMoving && Seen(e) < 0.6f) clipped.Add(e);
            }
            if (clipped.Count > 0) yield return WalkClipped(screen, clipped);
            foreach (var sv in moving) yield return WalkList(screen, sv);
            yield return null;
        }

        static readonly Vector2Int[] Dirs = { new Vector2Int(0, -1), new Vector2Int(1, 0), new Vector2Int(0, 1), new Vector2Int(-1, 0) };   // down, right, up, left
        static GamepadButton Pad(Vector2Int d) => d.y < 0 ? GamepadButton.DpadDown : d.y > 0 ? GamepadButton.DpadUp : d.x > 0 ? GamepadButton.DpadRight : GamepadButton.DpadLeft;

        /// What the directions lead to from every item of the navigation (the graph OuNav.Neighbour makes; value rows keep left / right).
        static Dictionary<VisualElement, VisualElement[]> Graph(OuNav nav)
        {
            var g = new Dictionary<VisualElement, VisualElement[]>();
            foreach (var it in nav.Items())
            {
                var n = new VisualElement[4];
                for (int k = 0; k < 4; k++)
                {
                    if (Dirs[k].x != 0 && ((OuNavItem)it.userData).adjust != null) continue;
                    var nb = nav.Neighbour(it, Dirs[k]); n[k] = nb == it ? null : nb;
                }
                g[it] = n;
            }
            return g;
        }

        sealed class WalkResult { public HashSet<VisualElement> visited = new HashSet<VisualElement>(); public int offView, moved, mismatch, replans, presses, leaps; public float worst = 1f; public string firstBad, firstMismatch; public VisualElement cur; }

        /// The pad walks to every target (the nearest not visited yet by the graph from where the focus is, a real press for each step); at every
        /// item reached that is meant to be in view the share seen is judged. sv: the list the targets sit in (null: judged against the screen).
        static IEnumerator WalkTo(string id, ScrollView sv, Dictionary<VisualElement, VisualElement[]> g, List<VisualElement> targets, WalkResult r, int settle, int cap)
        {
            var fails = new Dictionary<VisualElement, int>();
            for (int guard = 0; guard < cap && r.presses < cap; guard++)
            {
                if (r.cur == null || !g.ContainsKey(r.cur)) break;
                var from = new Dictionary<VisualElement, (VisualElement prev, int k)> { [r.cur] = (null, -1) }; var qq = new Queue<VisualElement>(); qq.Enqueue(r.cur);
                VisualElement target = null;
                while (qq.Count > 0 && target == null)
                {
                    var c = qq.Dequeue(); var nb = g[c];
                    for (int k = 0; k < 4; k++)
                    {
                        var x = nb[k]; if (x == null || from.ContainsKey(x) || !g.ContainsKey(x)) continue;
                        from[x] = (c, k); qq.Enqueue(x);
                        if (target == null && !r.visited.Contains(x) && targets.Contains(x) && (!fails.TryGetValue(x, out var nf) || nf < 2)) target = x;
                    }
                }
                if (target == null) break;
                var path = new List<(VisualElement to, int k)>();
                for (var t = target; from[t].prev != null; t = from[t].prev) path.Add((t, from[t].k));
                path.Reverse();
                foreach (var (to, k) in path)
                {
                    yield return PadTap(Pad(Dirs[k])); yield return UiShowcase.Frames(settle); r.presses++;
                    var f = Look().focused;
                    if (f == null) { Check(false, id, "the focus was lost after " + Pad(Dirs[k]) + " from " + Label(r.cur)); r.cur = null; yield break; }
                    if (f != r.cur) { r.moved++; ScrollSteps++; }
                    if (targets.Contains(f)) { r.visited.Add(f); Judge(id, sv, f, r); }
                    if (f != to && to != null && !targets.Contains(f) && targets.Contains(to)) r.leaps++;   // the press left the list for something outside it while the list goes on that way
                    if (f != to)
                    {
                        r.mismatch++; r.replans++; r.firstMismatch ??= Pad(Dirs[k]) + " from " + Label(r.cur) + " " + B(r.cur) + " landed on " + Label(f) + " " + B(f) + " (the graph says " + Label(to) + " " + B(to) + ")" + (sv != null ? " at " + Where(sv) : "");
                        fails[target] = (fails.TryGetValue(target, out var n0) ? n0 : 0) + 1; r.cur = f; break;
                    }
                    r.cur = f;
                }
            }
        }

        static string B(VisualElement e) { var b = e.worldBound; return "[" + b.xMin.ToString("F0") + "," + b.yMin.ToString("F0") + " " + b.width.ToString("F0") + "x" + b.height.ToString("F0") + "]"; }

        static void Judge(string id, ScrollView sv, VisualElement f, WalkResult r)
        {
            float s = Seen(f);
            if (s < r.worst) r.worst = s;
            if (s < 0.85f) { r.offView++; r.firstBad ??= Label(f) + " " + (s * 100).ToString("F0") + "% " + (sv != null ? Where(sv) : "(not in a scroll view)"); }
        }

        /// Items cut off by something that does not scroll (a carousel): the pad walks to each; once the move has settled it must be in view.
        static IEnumerator WalkClipped(string screen, List<VisualElement> clipped)
        {
            string id = screen + " > (clipped items, no scroll view)";
            var nav = NavOf(clipped[0]); if (nav == null) { Warn(id, "no navigation owns the items"); yield break; }
            var g = Graph(nav);
            var r = new WalkResult { cur = Look().focused };
            if (r.cur == null) yield break;
            ScrollLists++;
            var reach = new HashSet<VisualElement> { r.cur }; var q0 = new Queue<VisualElement>(); q0.Enqueue(r.cur);
            while (q0.Count > 0) { var c = q0.Dequeue(); if (!g.TryGetValue(c, out var nb)) continue; foreach (var x in nb) if (x != null && reach.Add(x)) q0.Enqueue(x); }
            var lost = clipped.Where(e => !reach.Contains(e)).ToList();
            yield return WalkTo(id, null, g, clipped, r, 24, Mathf.Min(clipped.Count * 6 + 20, 400));
            // once it has stopped moving the focused item is in view (judged at each landing after the settle wait)
            Debug.Log(Tag + "scroll [" + id + "] " + r.visited.Count + " of " + clipped.Count + " clipped items walked to in " + r.moved + " presses, " + r.offView + " not 85% in view (worst " + r.worst.ToString("F2") + ")" + (r.firstBad != null ? ", first bad: " + r.firstBad : ""));
            Check(r.offView == 0, id, r.offView + " clipped items were not brought into view by the pad (worst " + r.worst.ToString("F2") + ", first: " + r.firstBad + ")");
            Check(lost.Count == 0, id, lost.Count + " of " + clipped.Count + " cut-off items cannot be reached by the directions: " + string.Join(", ", lost.Take(5).Select(Label)));
            Check(r.visited.Count == clipped.Count || lost.Count > 0, id, (clipped.Count - r.visited.Count) + " cut-off items were not reached by the walking pad" + (r.firstMismatch != null ? " (" + r.firstMismatch + ")" : ""));
        }

        static IEnumerator WalkList(string screen, ScrollView sv)
        {
            bool v = Range(sv, true) > 4f, h = Range(sv, false) > 4f;
            string id = screen + " > " + (string.IsNullOrEmpty(sv.name) ? "(" + string.Join(".", sv.GetClasses().Take(2)) + ")" : sv.name);
            var items = ListItems(sv);
            ScrollLists++;
            bool nested = false; for (var q = sv.parent; q != null; q = q.parent) if (q is ScrollView) { nested = true; break; }
            if (items.Count == 0)
            {   // nothing focusable in it (text, a log): the right stick scrolls it
                float max = Range(sv, true), before = sv.scrollOffset.y;
                Devices();
                InputSystem.QueueStateEvent(pad, new GamepadState { rightStick = new Vector2(0, -1) });
                for (int i = 0; i < 40; i++) yield return null;
                InputSystem.QueueStateEvent(pad, new GamepadState());
                for (int i = 0; i < 4; i++) yield return null;
                float down = sv.scrollOffset.y;
                InputSystem.QueueStateEvent(pad, new GamepadState { rightStick = new Vector2(0, 1) });
                for (int i = 0; i < 40; i++) yield return null;
                InputSystem.QueueStateEvent(pad, new GamepadState());
                for (int i = 0; i < 4; i++) yield return null;
                float up = sv.scrollOffset.y;
                Debug.Log(Tag + "scroll [" + id + "] text only, " + Where(sv) + ": right stick down " + before.ToString("F0") + " -> " + down.ToString("F0") + ", up -> " + up.ToString("F0"));
                Check(down >= Mathf.Min(max, before + 40f) - 1f && up <= down - Mathf.Min(down, 20f) + 1f, id, "text that scrolls (" + Where(sv) + ") cannot be scrolled with the right stick (down " + before.ToString("F0") + " -> " + down.ToString("F0") + ", up -> " + up.ToString("F0") + ")");
                yield break;
            }
            var nav = NavOf(items[0]);
            if (nav == null) { Warn(id, "no navigation owns its items"); yield break; }
            bool horizontalOnly = h && !v;
            var g = Graph(nav);
            // 1. every item of the list is reachable from the first (the graph), then the pad walks it
            var reach = new HashSet<VisualElement> { items[0] }; var q0 = new Queue<VisualElement>(); q0.Enqueue(items[0]);
            while (q0.Count > 0) { var c = q0.Dequeue(); if (!g.TryGetValue(c, out var nb)) continue; foreach (var x in nb) if (x != null && reach.Add(x)) q0.Enqueue(x); }
            var lostItems = items.Where(e => !reach.Contains(e)).ToList();
            sv.scrollOffset = Vector2.zero; yield return UiShowcase.Frames(3);
            nav.Focus(items[0]); yield return UiShowcase.Frames(8);
            var start = Look().focused;
            Check(start == items[0], id, "the first item could not take the focus (" + (start == null ? "none" : Label(start)) + ")");
            var r = new WalkResult { cur = start ?? items[0] }; r.visited.Add(r.cur);
            int cap = Mathf.Min(items.Count * 4 + 40, 1500);
            yield return WalkTo(id, sv, g, items, r, 3, cap);
            // 2. back: from the last item the other way (up; left in a strip) until nothing moves
            int backMoved = 0; var last = items[items.Count - 1];
            nav.Focus(last); yield return UiShowcase.Frames(8);
            var cur = Look().focused ?? last;
            var backDir = horizontalOnly ? Dirs[3] : Dirs[2];
            for (int i = 0; i < cap; i++)
            {
                yield return PadTap(Pad(backDir)); yield return UiShowcase.Frames(3);
                var f = Look().focused;
                if (f == null || f == cur || !items.Contains(f)) break;
                backMoved++; ScrollSteps++; cur = f; Judge(id, sv, f, r);
            }
            int never = items.Count(e => reach.Contains(e) && !r.visited.Contains(e));
            Debug.Log(Tag + "scroll [" + id + "] " + (horizontalOnly ? "horizontal" : "vertical") + (nested ? " NESTED" : "") + ": " + r.visited.Count + " of " + items.Count + " items walked in " + r.moved + " presses (" + r.replans + " re-plans, " + r.leaps + " jumped out of the list), back " + backMoved + " presses, " + r.offView + " not 85% in view (worst " + r.worst.ToString("F2") + "), " + Where(sv) + (r.firstBad != null ? ", first bad: " + r.firstBad : "") + (r.mismatch > 0 ? ", " + r.mismatch + " presses did not land where the graph said: " + r.firstMismatch : ""));
            Check(r.offView == 0, id, r.offView + " focused items were less than 85% in view (worst " + r.worst.ToString("F2") + ", first: " + r.firstBad + ")");
            Check(r.leaps == 0, id, r.leaps + " presses jumped out of the list to something outside it although the list goes on that way (" + r.firstMismatch + ")");
            Check(lostItems.Count == 0, id, lostItems.Count + " of " + items.Count + " items cannot be reached from the first by the directions: " + string.Join(", ", lostItems.Take(5).Select(Label)));
            Check(never == 0, id, never + " of " + items.Count + " reachable items were not reached by the walking pad: " + string.Join(", ", items.Where(e => reach.Contains(e) && !r.visited.Contains(e)).Take(5).Select(Label)) + (r.firstMismatch != null ? " (" + r.firstMismatch + ")" : ""));
            // the hold: the stick held toward the end keeps moving and never jumps back
            if (items.Count >= 8) yield return HoldList(id, sv, items, nav, horizontalOnly);
            // a value changed on a row far down: the page that is rebuilt for it gives the focus back in view (not in the options: they would switch the window)
            if (!screen.StartsWith("options") && sv.panel != null)
            {
                var valueRows = items.Where(e => e.panel != null && ((OuNavItem)e.userData).adjust != null).ToList();
                var picks = new List<string>();
                if (valueRows.Count > 0) { picks.Add(valueRows[valueRows.Count - 1].name); if (valueRows.Count > 2) picks.Add(valueRows[valueRows.Count / 2].name); }
                foreach (var name in picks.Where(n => !string.IsNullOrEmpty(n)).Distinct().ToList())
                {
                    var row = sv.contentContainer.Q<VisualElement>(name); if (row == null || row.panel == null) continue;
                    nav.Focus(row); yield return UiShowcase.Frames(12);
                    foreach (var key in new[] { GamepadButton.DpadRight, GamepadButton.DpadLeft })
                    {
                        yield return PadTap(key); yield return UiShowcase.Frames(30);
                        var f = Look().focused;
                        Check(f != null, id, "no focus after " + key + " changed the value of " + name);
                        if (f == null) continue;
                        float s = Seen(f);
                        Debug.Log(Tag + "scroll [" + id + "] value change on " + name + " (" + key + "): focus " + Label(f) + " seen " + (s * 100).ToString("F0") + "%");
                        Check(s >= 0.85f, id, "after " + key + " changed the value of " + name + " the focus (" + Label(f) + ") is only " + (s * 100).ToString("F0") + "% in view");
                    }
                }
            }
        }

        static IEnumerator HoldList(string id, ScrollView sv, List<VisualElement> items, OuNav nav, bool horizontal)
        {
            sv.scrollOffset = Vector2.zero; yield return UiShowcase.Frames(3);
            nav.Focus(items[0]); yield return UiShowcase.Frames(8);
            Devices();
            float secs = 6f; var dir = horizontal ? new Vector2(1, 0) : new Vector2(0, -1);
            var prev = Look().focused; int changes = 0, back = 0, off = 0, left = 0, leftAt = -1; int prevAt = items.IndexOf(prev); float worst = 1f; string bad = null; var trail = new System.Text.StringBuilder();
            InputSystem.QueueStateEvent(pad, new GamepadState { leftStick = dir });
            for (float t = 0; t < secs; t += Time.unscaledDeltaTime)
            {
                yield return null;
                var f = Look().focused;
                if (f == null || f == prev) continue;
                changes++; prev = f;
                int at = items.IndexOf(f); if (trail.Length < 400) trail.Append(at).Append(' ');
                if (at < 0) { left++; leftAt = prevAt; break; }
                if (at < prevAt) { back++; if (back <= 3) Debug.Log(Tag + "scroll [" + id + "] held stick: JUMPED BACK from item " + prevAt + " to " + at + " at " + t.ToString("F2") + " s, " + Where(sv) + ", focus " + B(f)); }
                prevAt = at;
                float s = Seen(f); if (s < worst) worst = s; if (s < 0.85f) { off++; bad ??= Label(f) + " " + (s * 100).ToString("F0") + "% " + Where(sv); }
            }
            InputSystem.QueueStateEvent(pad, new GamepadState());
            for (int i = 0; i < 4; i++) yield return null;
            bool atEnd = items.IndexOf(Look().focused) >= items.Count - 1 - 8;
            Debug.Log(Tag + "scroll [" + id + "] held stick " + secs + " s: " + changes + " steps, to item " + prevAt + " of " + items.Count + ", jumped back " + back + "x, not in view " + off + "x (worst " + worst.ToString("F2") + "), left " + left + "x" + (back > 0 ? ", trail " + trail : ""));
            Check(left == 0 || leftAt >= items.Count - 2, id, "the held stick left the list at item " + leftAt + " of " + items.Count + " (jumped out to something beside it)");
            Check(back == 0, id, "the held stick jumped back " + back + " times");
            Check(off == 0, id, "the held stick left " + off + " focused items out of view (worst " + worst.ToString("F2") + ", first: " + bad + ")");
            Check(changes >= 12 || atEnd || left > 0, id, "the held stick moved only " + changes + " steps in " + secs + " s");
        }

        // ------------------------------------------------------------------------------------------------ the shell screens

        static IEnumerator Visit(string name, Action open, int frames = 60)
        {
            var only = Environment.GetEnvironmentVariable("REFLEX_UI_SCROLLONLY");
            if (!string.IsNullOrEmpty(only) && !name.Contains(only)) yield break;
            Debug.Log(Tag + "scrollall visit " + name);
            open(); yield return UiShowcase.Frames(frames);
            yield return ScrollWalk(name);
        }

        static IEnumerator ScrollAll()
        {
            var shell = OpenShell.Instance; Devices();
            PadEvents(5);
            {   // a short last row in the grids: one race event and two free ride events fewer
                var evs = OpenContent.Events as List<OpenEvent>;
                if (evs != null) { int a = evs.FindLastIndex(e => !e.freeRide); if (a >= 0) evs.RemoveAt(a); for (int k = 0; k < 2; k++) { int b = evs.FindLastIndex(e => e.freeRide); if (b >= 0) evs.RemoveAt(b); } }
            }
            foreach (var (free, view) in new[] { (false, 0), (false, 1), (true, 0), (true, 1) })
            {
                OpenHistory.View = view;
                shell.ShowMain(); yield return UiShowcase.Frames(20);
                yield return Visit("browser " + (free ? "free ride" : "race") + (view == 0 ? " grid" : " list"), () => shell.ShowBrowser(free), 60);
            }
            OpenHistory.View = 0;
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            yield return Visit("setup", () => { shell.ShowBrowser(false); shell.ShowSetupPage(); }, 80);
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            yield return Visit("hub", () => shell.ShowMain(), 30);
            yield return Visit("play", () => shell.ShowPlay(), 30);
            yield return Visit("credits", () => shell.ShowCredits(), 40);
            yield return Visit("library", () => shell.ShowLibrary(true), 40);
            yield return Visit("install", () => shell.ShowInstall(false), 40);
            yield return Visit("motoskills", () => shell.ShowMotoSkills(), 40);
            for (int tab = 0; tab < 3; tab++) { int t = tab; yield return Visit("physics studio " + t, () => shell.ShowPhysicsStudio(t), 40); }
            for (int tab = 0; tab < ReflexOptions.Tabs.Length; tab++)
            {
                int t = tab;
                yield return Visit("options " + ReflexOptions.Tabs[t], () => shell.ShowOptions(t), 40);
                yield return PadTap(GamepadButton.East); yield return UiShowcase.Frames(30);
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
            Debug.Log(Tag + "scrollall: " + ScrollLists + " scroll lists walked, " + ScrollSteps + " pad moves, " + ScrollWarnings + " warnings, " + Failures + " failures so far");
        }
    }
}
