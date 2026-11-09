// The on-screen keyboard (a text box needs a keyboard, a pad has none): a dialog over the screen that the pad drives alone.
//   - d-pad / stick move over the keys, A (Cross) types the key, B (Circle) closes it dropping what was typed, DONE keeps it;
//     X (Square) is backspace and Y (Triangle) a space, so a name is typed without leaving the letters
//   - the digits-only keyboard (a number) is a pad of its own; max is the text box's own length
//   - nothing behind it can be reached (OuNav.Modal); the screen's tab / extra / page buttons are the keyboard's while it is open and
//     are given back, with the focus on what opened it, when it closes
// One keyboard for every screen that has a layer, a navigation and a hint bar: the shell's (OpenShell.Keyboard.cs: the garage's text rows open it
// when the pad's A uses them) and the options pages' (ReflexOptionsMenu.Cameras.cs: a camera preset's NAME).
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OuKeyboard
    {
        sealed class OsKey
        {
            public VisualElement el;
            public float x, w;   // the key's place in its row, in key widths (the neighbours up and down are the nearest by centre)
        }

        readonly OuNav _nav;
        readonly VisualElement _hints;
        VisualElement root;
        Label text, count;
        string value = "";
        bool selected;   // the start text is selected: the first key replaces it, DELETE clears it
        int max;
        bool spaces;
        Action<string> done; Action cancelled, closed;
        VisualElement returnTo;
        Action<int> tab, page; Action alt, find; Func<bool> cancel; Func<VisualElement> defaultItem;
        IVisualElementScheduledItem caret;

        /// The menu sounds ("Locked" when a key cannot type); the shell's by default, the options pass theirs.
        public Action<string> Sound = OuSound.Play;
        /// The keyboard is up.
        public bool IsOpen { get; private set; }
        /// The text typed so far (tests).
        public string Value => value;

        OuKeyboard(OuNav nav, VisualElement hints) { _nav = nav; _hints = hints; }

        /// The row-by-row keys of the letters keyboard. A name takes space, "-" and "_"; the rest letters and digits.
        static readonly string[][] OsLetterRows =
        {
            new[] { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
            new[] { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" },
            new[] { "A", "S", "D", "F", "G", "H", "J", "K", "L" },
            new[] { "Z", "X", "C", "V", "B", "N", "M" },
        };

        /// Opens the keyboard on start over layer: the title (the orange bar of a dialog) over the text, note under it (what is allowed). digits = a
        /// number pad; spaces = also space, "-" and "_". done gets the text as typed (the caller cleans / validates it), cancelled is called after
        /// B / CANCEL; closed (the owner forgets the keyboard) before either. The focus comes back to returnTo either way. hints: the screen's hint bar.
        public static OuKeyboard Open(VisualElement layer, OuNav nav, VisualElement hints, string title, string note, string start, int max, bool digits, bool spaces,
                                      VisualElement returnTo, Action<string> done, Action cancelled = null, Action closed = null, Action<string> sound = null)
        {
            var k = new OuKeyboard(nav, hints) { max = max, spaces = spaces, done = done, cancelled = cancelled, closed = closed, returnTo = returnTo };
            if (sound != null) k.Sound = sound;
            k.Build(layer, title, note, start, digits);
            return k;
        }

        /// The dialog's look (the shell's garage sheet: the dialog root over the screen) on the document of layer.
        public static void EnsureStyles(VisualElement layer)
        {
            VisualElement top = layer;
            for (var v = layer; v != null; v = v.parent) if (v.ClassListContains("mxa-root")) top = v;
            var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuGarage");
            if (uss && !top.styleSheets.Contains(uss)) top.styleSheets.Add(uss);
        }

        void Build(VisualElement layer, string title, string note, string start, bool digits)
        {
            IsOpen = true;
            value = start ?? "";
            if (value.Length > max) value = value.Substring(0, max);
            selected = value.Length > 0;
            // what the screen had on its buttons, to hand back
            tab = _nav.OnTab; page = _nav.OnPage; alt = _nav.OnAlt; find = _nav.OnFind; cancel = _nav.OnCancel; defaultItem = _nav.DefaultItem;

            root = OuUi.Div("ou-gdialog-root ou-osk-root", layer);
            var card = OuWidgets.Dialog(root, "osk");
            card.AddToClassList(digits ? "ou-osk-digits" : "ou-osk-letters");
            OuUi.Text(title.ToUpperInvariant(), "ou-dialog-title ou-display", card).enableRichText = false;
            var display = OuUi.Div("ou-osk-display", card);
            text = OuUi.Text("", "ou-osk-text ou-display", display); text.enableRichText = false;
            count = OuUi.Text("", "ou-osk-count ou-label", display);
            if (!string.IsNullOrEmpty(note)) OuUi.Text(note, "ou-osk-note", card).enableRichText = false;

            var rows = new List<List<OsKey>>();
            var grid = OuUi.Div("ou-osk-grid", card);
            VisualElement Row() { var r = OuUi.Div("ou-osk-row", grid); rows.Add(new List<OsKey>()); return r; }
            OsKey Key(VisualElement row, string label, float width, Action press, string cls = null)
            {
                var el = OuUi.Button(label, "ou-osk-key" + (cls != null ? " " + cls : ""), press, row);
                el.name = "osk-" + label.ToLowerInvariant();
                el.style.flexGrow = width;
                var list = rows[rows.Count - 1];
                float x = list.Count == 0 ? 0f : list[list.Count - 1].x + list[list.Count - 1].w;
                var key = new OsKey { el = el, x = x, w = width };
                list.Add(key);
                return key;
            }
            void Type(string s)
            {
                if (selected) { value = ""; selected = false; }
                if (value.Length + s.Length > max) { Sound?.Invoke("Locked"); return; }
                value += s; Show();
            }
            void Delete()
            {
                if (selected) { value = ""; selected = false; Show(); return; }
                if (value.Length == 0) { Sound?.Invoke("Locked"); return; }
                value = value.Substring(0, value.Length - 1); Show();
            }
            void Space() { if (spaces) Type(" "); else Sound?.Invoke("Locked"); }

            if (digits)
            {
                string[][] pad = { new[] { "1", "2", "3" }, new[] { "4", "5", "6" }, new[] { "7", "8", "9" } };
                foreach (var line in pad)
                {
                    var r = Row();
                    foreach (var d in line) { string c = d; Key(r, c, 1f, () => Type(c)); }
                }
                var last = Row();
                Key(last, "DELETE", 1f, Delete, "ou-osk-wide");
                Key(last, "0", 1f, () => Type("0"));
                Key(last, "DONE", 1f, () => Close(true), "ou-osk-wide ou-osk-done");
                var bottom = Row();
                Key(bottom, "CANCEL", 3f, () => Close(false), "ou-osk-wide");
            }
            else
            {
                foreach (var line in OsLetterRows)
                {
                    var r = Row();
                    foreach (var l in line) { string c = l; Key(r, c, 1f, () => Type(c)); }
                    if (line == OsLetterRows[2]) { if (spaces) Key(r, "-", 1f, () => Type("-")); else Key(r, "DELETE", 1f, Delete, "ou-osk-wide"); }
                    if (line == OsLetterRows[3]) { if (spaces) Key(r, "_", 1f, () => Type("_")); Key(r, "DELETE", spaces ? 2f : 3f, Delete, "ou-osk-wide"); }
                }
                var bottom = Row();
                if (spaces) Key(bottom, "SPACE", 4f, Space, "ou-osk-wide");
                Key(bottom, "CANCEL", 3f, () => Close(false), "ou-osk-wide");
                Key(bottom, "DONE", 3f, () => Close(true), "ou-osk-wide ou-osk-done");
            }
            Link(rows);
            Show();

            // the keys: up / down / left / right over them, B closes, X deletes, Y spaces; the screen's tabs and pages stay out of it
            var first = rows[digits ? 0 : 1][0].el;
            _nav.OnTab = null; _nav.OnPage = _ => { };
            _nav.OnAlt = spaces ? Space : (Action)null; _nav.OnFind = Delete;
            _nav.OnCancel = () => { Close(false); return true; };
            _nav.DefaultItem = () => first;
            _nav.Modal = card;
            _nav.Focus(first);
            if (_hints != null)
            {
                if (spaces) OuUi.Hints(_hints, (OuUi.KeySubmit, "TYPE"), (OuUi.KeyFind, "DELETE"), (OuUi.KeyAlt, "SPACE"), (OuUi.KeyCancel, "CANCEL"));
                else OuUi.Hints(_hints, (OuUi.KeySubmit, "TYPE"), (OuUi.KeyFind, "DELETE"), (OuUi.KeyCancel, "CANCEL"));
            }
            caret = card.schedule.Execute(() => { if (IsOpen) Show(); }).Every(450);
        }

        /// The typed text with its caret (blinking), and the count under the max.
        void Show()
        {
            bool on = (int)(Time.unscaledTime * 2.2f) % 2 == 0;
            text.text = value + (on ? "|" : " ");
            text.EnableInClassList("ou-osk-selected", selected);
            count.text = value.Length + " / " + max;
            count.EnableInClassList("ou-osk-full", value.Length >= max);
        }

        /// Left / right stop at the ends of a row; up / down go to the key of the next row whose middle is nearest (the wide keys of the bottom
        /// row sit under several).
        static void Link(List<List<OsKey>> rows)
        {
            for (int r = 0; r < rows.Count; r++)
            {
                var row = rows[r];
                for (int i = 0; i < row.Count; i++)
                {
                    var it = (OuNavItem)row[i].el.userData;
                    it.left = row[Mathf.Max(0, i - 1)].el; it.right = row[Mathf.Min(row.Count - 1, i + 1)].el;
                    float mid = row[i].x + row[i].w * 0.5f;
                    it.up = r > 0 ? NearestKey(rows[r - 1], mid) : row[i].el;
                    it.down = r + 1 < rows.Count ? NearestKey(rows[r + 1], mid) : row[i].el;
                }
            }
        }

        static VisualElement NearestKey(List<OsKey> row, float mid)
        {
            OsKey best = null; float bestD = float.MaxValue;
            foreach (var key in row) { float d = Mathf.Abs(key.x + key.w * 0.5f - mid); if (d < bestD) { bestD = d; best = key; } }
            return best.el;
        }

        /// The screen is being left or rebuilt: the keyboard goes with it (no callback; the screen's own Clear gives the navigation back).
        public void Release()
        {
            if (!IsOpen) return;
            IsOpen = false;
            caret?.Pause();
            closed?.Invoke();
        }

        /// Closes the keyboard: keep = DONE (done gets the text), else B / CANCEL. The screen gets its buttons and the focus back.
        public void Close(bool keep)
        {
            if (!IsOpen) return;
            IsOpen = false;
            caret?.Pause();
            closed?.Invoke();
            _nav.Modal = null;
            _nav.OnTab = tab; _nav.OnPage = page; _nav.OnAlt = alt; _nav.OnFind = find; _nav.OnCancel = cancel; _nav.DefaultItem = defaultItem;
            root.RemoveFromHierarchy();
            if (returnTo != null && returnTo.panel != null) _nav.Focus(returnTo);
            if (keep) done?.Invoke(value); else cancelled?.Invoke();
        }
    }
}
