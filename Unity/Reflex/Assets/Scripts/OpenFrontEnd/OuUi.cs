// The OPEN REFLEX shell's UI Toolkit plumbing, ported from Open Unleashed's MxaUi (Scripts/UI/MxaUi.cs): one runtime PanelSettings
// (Resources/OpenFrontEnd/OuPanelSettings, theme OuTheme.tss + OuMenu.uss; 1920x1080 reference, scaled by height so 16:9 and 16:10
// get the same vertical layout), the EventSystem for the mouse, the controller / keyboard navigation (OuMenuInput + OuNav) and the
// widgets the screens are made of.
//
// Navigation: every selectable element is a focusable VisualElement with an OuNavItem in userData. The focused one is UI Toolkit's
// focused element (so :focus styles it). Up / down / left / right move spatially to the nearest item in that direction on the
// active screen; on a value row (spinner, toggle) left / right change the value instead. Submit = Cross / A / Enter / Space,
// Cancel = Circle / B / Esc, L1 / R1 (Q / E, Page Up / Down) = the screen's tabs, Triangle / Y (F) = the screen's extra action (the
// track browser's favourite), Square / X (/ or Tab) = its search field. The mouse hovers (focuses) and clicks. The EventSystem's own
// move / submit / cancel actions are switched off so nothing is handled twice.
// The menu sounds are Reflex's own UI sounds from the install (OuSound -> FeSounds, UIRegistry.xml), never Open Unleashed's files.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.UI;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    /// The shell's menu sounds: Reflex's UI sounds (UIRegistry.xml UINames) through the shell's FeSounds (OpenShell sets Player).
    public static class OuSound
    {
        public static Reflex.Port.FrontEnd.FeSounds Player;
        /// Every sound asked for, newest last (tests).
        public static readonly List<string> Log = new List<string>();
        public static void Play(string what)
        {
            string ui = what switch { "UpDown" => "UpDownDirectional", "LeftRight" => "LeftRightDirectional", _ => what };
            lock (Log) { Log.Add(ui); if (Log.Count > 500) Log.RemoveRange(0, 250); }
            try { Player?.Play(ui); } catch (Exception e) { Debug.LogWarning("[OpenReflex] sound " + ui + ": " + e.Message); }
        }
    }

    public static class OuUi
    {
        public const string ResourceRoot = "OpenFrontEnd/";
        static PanelSettings s_panel;
        static Texture2D s_scrim, s_scrimBottom;

        /// The shared runtime PanelSettings (created on first use).
        public static PanelSettings Panel
        {
            get
            {
                if (s_panel) return s_panel;
                // the asset (Resources/OpenFrontEnd/OuPanelSettings) puts a PanelSettings into the player, so Unity includes the ICU data
                // UI Toolkit's text needs ("ICU Data not available" otherwise); an instance copy keeps the asset untouched
                var asset = Resources.Load<PanelSettings>(ResourceRoot + "OuPanelSettings");
                if (asset) { s_panel = UnityEngine.Object.Instantiate(asset); s_panel.name = "OuPanelSettings"; OuScale.Register(s_panel); return s_panel; }
                s_panel = ScriptableObject.CreateInstance<PanelSettings>();
                s_panel.name = "OuPanelSettings";
                s_panel.themeStyleSheet = Resources.Load<ThemeStyleSheet>(ResourceRoot + "OuTheme");
                s_panel.scaleMode = PanelScaleMode.ScaleWithScreenSize;
                s_panel.referenceResolution = new Vector2Int(1920, 1080);
                s_panel.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
                s_panel.match = 1f;
                s_panel.sortingOrder = 50;
                OuScale.Register(s_panel);
                return s_panel;
            }
        }

        /// A UIDocument on host with the menu stylesheet; returns its root (full screen).
        public static VisualElement CreateDocument(GameObject host, float sortingOrder = 0)
        {
            var doc = host.GetComponent<UIDocument>() ?? host.AddComponent<UIDocument>();
            doc.panelSettings = Panel;
            doc.sortingOrder = sortingOrder;
            var root = doc.rootVisualElement;
            foreach (var name in new[] { "OuMenu" })
            {
                var uss = Resources.Load<StyleSheet>(ResourceRoot + name);
                if (uss && !root.styleSheets.Contains(uss)) root.styleSheets.Add(uss);
            }
            root.AddToClassList("mxa-root");
            OuMotion.Sync(root);
            root.pickingMode = PickingMode.Ignore;
            return root;
        }

        /// An EventSystem for the mouse (the Input System UI module with its navigation actions off: OuNav does that). One that is already
        /// there gets its navigation turned off too (w-uibatch): the race's original pause menu fallback (OriginalPauseMenu.Build) makes a kept
        /// (DontDestroyOnLoad) EventSystem with the module's default actions, and the front end after the race found it and kept it as it was:
        /// the module then sent its own navigation events from the d-pad / stick / Enter to the focused element as well, and UI Toolkit moved
        /// the focus a second time, by its own rules, on every press OuNav answered ("left / right in the 3-wide track grid does not work after
        /// a race; fixed by relaunching").
        public static void EnsureEventSystem()
        {
            bool any = false;
            foreach (var es in UnityEngine.Object.FindObjectsByType<EventSystem>(FindObjectsSortMode.None)) { any = true; NavigationOff(es); }
            if (any) return;
            var go = new GameObject("EventSystem", typeof(EventSystem));
            var m = go.AddComponent<InputSystemUIInputModule>();
            m.AssignDefaultActions();
            NavigationOff(go.GetComponent<EventSystem>());
        }

        /// The UI module's own navigation off (the menus' input is OuNav's / OptNav's; the original pause menu and front end read their keys
        /// themselves): the mouse stays.
        public static void NavigationOff(EventSystem es)
        {
            if (!es) return;
            var m = es.GetComponent<InputSystemUIInputModule>();
            if (m == null || m.move == null && m.submit == null && m.cancel == null) return;
            m.move = null; m.submit = null; m.cancel = null;
            Debug.Log("[OpenReflex] the EventSystem '" + es.name + "': its UI module's navigation turned off (the menus navigate themselves)");
        }

        // --------------------------------------------------------------------------------------------- widgets

        public static VisualElement Div(string cls, VisualElement parent = null)
        {
            var e = new VisualElement();
            if (!string.IsNullOrEmpty(cls)) { foreach (var c in cls.Split(' ')) e.AddToClassList(c); OuLayout.TrackByClass(e, cls); }
            parent?.Add(e);
            return e;
        }

        public static Label Text(string text, string cls, VisualElement parent = null)
        {
            var l = new Label(text);
            if (!string.IsNullOrEmpty(cls)) foreach (var c in cls.Split(' ')) l.AddToClassList(c);
            l.pickingMode = PickingMode.Ignore;
            parent?.Add(l);
            return l;
        }

        static VisualElement s_hoverFocused; static int s_hoverFrame = -10;
        /// The focus e just got came from the mouse passing over it, not from a click or the keys / pad.
        public static bool FocusFromHover(VisualElement e) => e != null && s_hoverFocused == e && Time.frameCount - s_hoverFrame <= 1;

        /// e is about to take the focus because the pointer passed over it (the navigation does not scroll it into view for that).
        public static void NoteHover(VisualElement e) { s_hoverFocused = e; s_hoverFrame = Time.frameCount; }

        static float s_mouseMovedAt = -10f;
        static Vector2 s_navPointer; static float s_navPointerAt = -10f;

        /// The pad / keys just moved the focus (or a screen put it somewhere): remembers where the pointer is. A hover may take the focus
        /// only once the pointer has travelled away from here (MouseMoving). Without this a list that scrolls under a pointer resting over it
        /// (a mouse, a touchpad or a remote desktop session reports a pixel or two of noise a frame) hands the focus to the row that slid under
        /// the pointer: the pad scrolls the list one row, the pointer takes the focus back to the row near the top, and the pad never gets
        /// past the bottom of what is visible.
        public static void NoteNavigation()
        {
            var m = UnityEngine.InputSystem.Mouse.current;
            if (m != null) s_navPointer = m.position.ReadValue();
            s_navPointerAt = Time.unscaledTime;
        }

        /// Pixels the pointer must move after the last pad / key navigation before a hover may focus (a few millimetres on a monitor).
        static float HoverTravel => Mathf.Max(8f, Screen.height * 0.008f);

        /// The pointer is really being moved by the user: it moved this moment, and it is away from where it was when the pad / keys last
        /// navigated. A list scrolling under a resting (or noisy) pointer raises PointerEnter for the rows sliding past it, which took the focus
        /// from the pad's own choice; only a pointer that actually travels may focus by hover.
        public static bool MouseMoving()
        {
            var m = UnityEngine.InputSystem.Mouse.current;
            if (m == null) return true;   // touch / pen / no mouse device: the events are real
            float now = Time.unscaledTime;
            if (m.delta.ReadValue().sqrMagnitude > 0.01f) s_mouseMovedAt = now;
            if (now - s_mouseMovedAt >= 0.15f) return false;
            if (s_navPointerAt < 0f) return true;   // the pad / keys have not navigated yet
            return (m.position.ReadValue() - s_navPointer).sqrMagnitude > HoverTravel * HoverTravel;
        }

        /// Makes e a navigable item (focus on hover, submit on click).
        public static T Item<T>(T e, Action submit, Action<int> adjust = null) where T : VisualElement
        {
            e.focusable = true;
            e.tabIndex = -1;   // UI Toolkit's own Tab ring would move the focus too (Tab is the search key here)
            e.pickingMode = PickingMode.Position;
            e.userData = new OuNavItem { submit = submit, adjust = adjust };
            e.AddToClassList("mxa-item");
            e.RegisterCallback<PointerEnterEvent>(_ => { if (!(e.focusController?.focusedElement is VisualElement held && (held == e || e.Contains(held))) && MouseMoving()) { NoteHover(e); e.Focus(); } });   // (the focus already inside the item, a text box typed in, is not taken to the item itself)
            e.RegisterCallback<ClickEvent>(ev =>
            {
                if (ev.button != 0) return;
                e.Focus();
                var it = e.userData as OuNavItem;
                if (it?.submit != null && e.enabledInHierarchy) { OuSound.Play("Accept"); it.submit(); }
            });
            return e;
        }

        /// A menu entry (a large label that submits).
        public static VisualElement Button(string text, string cls, Action submit, VisualElement parent = null)
        {
            var b = Div("mxa-button " + cls, parent);
            Text(text, "mxa-button-label", b);
            return Item(b, submit);
        }

        /// A value row: label on the left, "<  value  >" on the right; left / right (or clicking the arrows) change it.
        public static VisualElement Spinner(string label, Func<string> value, Action<int> change, VisualElement parent, Action submit = null)
        {
            var row = Div("mxa-row", parent);
            Text(label, "mxa-row-label", row);
            var right = Div("mxa-row-value", row);
            var prev = Text("‹", "mxa-arrow", right);
            var val = Text(value(), "mxa-value", right);
            var next = Text("›", "mxa-arrow", right);
            void Change(int d) { change(d); val.text = value(); }
            prev.pickingMode = next.pickingMode = PickingMode.Position;
            prev.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Change(-1); e.StopPropagation(); });
            next.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Change(+1); e.StopPropagation(); });
            row.userData = null;
            Item(row, submit != null ? () => { submit(); val.text = value(); } : () => Change(+1), Change);
            row.RegisterCallback<AttachToPanelEvent>(_ => val.text = value());
            return row;
        }

        /// An ON / OFF row.
        public static VisualElement Toggle(string label, Func<bool> get, Action<bool> set, VisualElement parent, string on = "ON", string off = "OFF")
            => Spinner(label, () => get() ? on : off, _ => set(!get()), parent);

        // the hint bars on screen and what they say, so a key cap follows the device in use (OuDevice.Changed)
        static readonly List<(WeakReference<VisualElement> bar, (string key, string label)[] hints)> s_bars = new List<(WeakReference<VisualElement>, (string, string)[])>();
        static bool s_barsHooked;

        /// The key caps + labels of the bottom hint bar ("[Enter] SELECT  [Esc] BACK"). The caps change with the device in use.
        public static void Hints(VisualElement bar, params (string key, string label)[] hints)
        {
            if (bar == null) return;
            Render(bar, hints);
            s_bars.RemoveAll(b => !b.bar.TryGetTarget(out var t) || t == bar);
            s_bars.Add((new WeakReference<VisualElement>(bar), hints));
            if (s_barsHooked) return;
            s_barsHooked = true;
            OuDevice.Changed += RefreshHints;
            Reflex.Port.Options.ReflexSettings.Changed += key => { if (key == null || key == "Options.GlyphStyle") RefreshHints(); };
        }

        static void Render(VisualElement bar, (string key, string label)[] hints)
        {
            bar.Clear();
            foreach (var (key, label) in hints)
            {
                string k = OuGlyphs.Retarget(key);
                var h = Div("mxa-hint", bar);
                Text(k, "mxa-key " + (OuGlyphs.CapClass(k) ?? ""), h);
                Text(label, "mxa-hint-label", h);
            }
        }

        /// Every hint bar again with the caps of the device in use (the device changed, or CONTROLLER GLYPHS did).
        public static void RefreshHints()
        {
            for (int i = s_bars.Count - 1; i >= 0; i--)
            {
                if (!s_bars[i].bar.TryGetTarget(out var bar) || bar.panel == null) { if (bar == null) s_bars.RemoveAt(i); continue; }
                Render(bar, s_bars[i].hints);
            }
        }

        public static string KeySubmit => OuGlyphs.Submit;
        public static string KeyCancel => OuGlyphs.Cancel;
        public static string KeyTabs => OuGlyphs.Tabs;
        public static string KeyAlt => OuGlyphs.Alt;
        public static string KeyFind => OuGlyphs.Find;

        /// A horizontal alpha ramp (dark on the left) for the text side of a photo background.
        public static Texture2D Scrim => s_scrim ? s_scrim : s_scrim = Ramp(256, 1, x => Mathf.Lerp(0.92f, 0f, Mathf.SmoothStep(0, 1, x)));
        /// A vertical ramp (dark at the bottom).
        public static Texture2D ScrimBottom => s_scrimBottom ? s_scrimBottom : s_scrimBottom = Ramp(1, 256, y => Mathf.Lerp(0.85f, 0f, Mathf.SmoothStep(0, 1, y)));

        static Texture2D Ramp(int w, int h, Func<float, float> alpha)
        {
            var t = new Texture2D(w, h, TextureFormat.RGBA32, false) { wrapMode = TextureWrapMode.Clamp, name = "ou_scrim" };
            var px = new Color32[w * h];
            int n = Math.Max(w, h);
            for (int i = 0; i < n; i++)
            {
                var c = new Color32(8, 9, 11, (byte)(alpha(i / (float)(n - 1)) * 255));
                if (w > 1) for (int y = 0; y < h; y++) px[y * w + i] = c;
                else px[i] = c; // row 0 = bottom
            }
            t.SetPixels32(px);
            t.Apply(false, true);
            t.hideFlags = HideFlags.DontSave;
            return t;
        }

        /// The first navigable item of a container (null: none shown).
        public static VisualElement FirstItem(VisualElement page) =>
            page?.Query<VisualElement>(className: "mxa-item").ToList().Find(e => e.style.display != DisplayStyle.None);
    }

}
