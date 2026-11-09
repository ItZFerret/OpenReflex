// The options pages' UI Toolkit plumbing, adapted from Open Unleashed's MxaUi (Scripts/UI/MxaUi.cs): one runtime PanelSettings
// (an instance of the OPEN REFLEX shell's Resources/OpenFrontEnd/OuPanelSettings, 1920 x 1080 reference scaled by height), the menu
// stylesheets (the shell's theme, the one source: OpenFrontEnd/OuMenu.uss = Open Unleashed's MxaMenu.uss, OuFrontEnd.uss = its
// MxaFrontEnd.uss "Open Unleashed" look, OuControls.uss = its MxaControls.uss + the pause extras: the hot red, Barlow), the EventSystem for the mouse, the controller / keyboard navigation (OptMenuInput + OptNav) and the widgets.
//
// Navigation: the shell's (OpenFrontEnd/OuInput.cs, OuNav.cs): OptNav / OptMenuInput / OptNavItem only add the pages' own bits to
// the shared ones, so a fix of the focus or the direction repeat reaches the options too. Every selectable element is a focusable
// VisualElement with an OuNavItem in userData; on a value row (spinner, slider, toggle) left / right change the value.
// The mouse hovers (focuses) and clicks.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.UI;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public class OptNavItem : Reflex.Port.OpenFrontEnd.OuNavItem { }

    /// Menu actions (keyboard + every gamepad): the shell's, with the options host switching them off while the pages are closed.
    public class OptMenuInput : Reflex.Port.OpenFrontEnd.OuMenuInput
    {
    }

    public static class OptUi
    {
        static PanelSettings s_panel;

        /// The UI sounds (Accept, Back, UpDown, LeftRight); null: silent. The options host plays the front end's / the race's.
        public static Action<string> Sound;
        public static void Play(string name) { try { Sound?.Invoke(name); } catch (Exception) { } }

        /// The shared runtime PanelSettings (the asset in Resources keeps UI Toolkit's ICU data in a player build).
        public static PanelSettings Panel
        {
            get
            {
                if (s_panel) return s_panel;
                var asset = Resources.Load<PanelSettings>("OpenFrontEnd/OuPanelSettings");   // the shell's theme is the one source (Assets/OpenFrontEnd)
                if (asset) { s_panel = UnityEngine.Object.Instantiate(asset); s_panel.name = "OptPanelSettings"; }
                else
                {
                    s_panel = ScriptableObject.CreateInstance<PanelSettings>();
                    s_panel.name = "OptPanelSettings";
                    s_panel.themeStyleSheet = Resources.Load<ThemeStyleSheet>("OpenFrontEnd/OuTheme");
                    s_panel.scaleMode = PanelScaleMode.ScaleWithScreenSize;
                    s_panel.referenceResolution = new Vector2Int(1920, 1080);
                    s_panel.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
                    s_panel.match = 1f;
                }
                s_panel.sortingOrder = 200;   // over the race's canvases and the original pages
                Reflex.Port.OpenFrontEnd.OuScale.Register(s_panel);
                return s_panel;
            }
        }

        /// A UIDocument on host with the stylesheets; returns its root (full screen, the Open Unleashed look).
        public static VisualElement CreateDocument(GameObject host, float sortingOrder = 0)
        {
            var doc = host.GetComponent<UIDocument>(); if (!doc) doc = host.AddComponent<UIDocument>();
            doc.panelSettings = Panel;
            doc.sortingOrder = sortingOrder;
            var root = doc.rootVisualElement;
            foreach (var name in new[] { "OpenFrontEnd/OuMenu", "OpenFrontEnd/OuFrontEnd", "OpenFrontEnd/OuControls", "OpenFrontEnd/OuModern", "OpenFrontEnd/OuRestyle", "OpenFrontEnd/OuP2Front" })   // the shell's theme (Assets/OpenFrontEnd/Resources/OpenFrontEnd)
            {
                var uss = Resources.Load<StyleSheet>(name);
                if (uss && !root.styleSheets.Contains(uss)) root.styleSheets.Add(uss);
            }
            root.AddToClassList("mxa-root");
            root.AddToClassList("ou-frontend");
            Reflex.Port.OpenFrontEnd.OuMotion.Sync(root);
            root.pickingMode = PickingMode.Ignore;
            return root;
        }

        /// An EventSystem for the mouse (navigation actions off: OptNav does that).
        public static void EnsureEventSystem()
        {
            bool any = false;
            foreach (var es in UnityEngine.Object.FindObjectsByType<EventSystem>(FindObjectsSortMode.None)) { any = true; Reflex.Port.OpenFrontEnd.OuUi.NavigationOff(es); }   // (one with its navigation on: OuUi.EnsureEventSystem)
            if (any) return;
            var go = new GameObject("EventSystem", typeof(EventSystem));
            var m = go.AddComponent<InputSystemUIInputModule>();
            m.AssignDefaultActions(); m.move = null; m.submit = null; m.cancel = null;
            UnityEngine.Object.DontDestroyOnLoad(go);
        }

        // --------------------------------------------------------------------------------------------- widgets

        public static VisualElement Div(string cls, VisualElement parent = null)
        {
            var e = new VisualElement();
            if (!string.IsNullOrEmpty(cls)) { foreach (var c in cls.Split(' ')) e.AddToClassList(c); Reflex.Port.OpenFrontEnd.OuLayout.TrackByClass(e, cls); }
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

        /// Makes e a navigable item (focus on hover, submit on click).
        public static T Item<T>(T e, Action submit, Action<int> adjust = null) where T : VisualElement
        {
            e.focusable = true;
            e.tabIndex = -1;
            e.pickingMode = PickingMode.Position;
            e.userData = new OptNavItem { submit = submit, adjust = adjust };
            e.AddToClassList("mxa-item");
            e.RegisterCallback<PointerEnterEvent>(_ => { if (e.focusController?.focusedElement != e && Reflex.Port.OpenFrontEnd.OuUi.MouseMoving()) { Reflex.Port.OpenFrontEnd.OuUi.NoteHover(e); e.Focus(); } });
            e.RegisterCallback<ClickEvent>(ev =>
            {
                if (ev.button != 0) return;
                e.Focus();
                var it = e.userData as OptNavItem;
                if (it?.submit != null) { Play("Accept"); it.submit(); }
            });
            return e;
        }

        public static VisualElement Button(string text, string cls, Action submit, VisualElement parent = null)
        {
            var b = Div("mxa-button " + cls, parent);
            Text(text, "mxa-button-label", b);
            return Item(b, submit);
        }

        /// A value row: label on the left, "<  value  >" on the right; left / right (or the arrows) change it.
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
            prev.RegisterCallback<ClickEvent>(e => { Play("LeftRight"); Change(-1); e.StopPropagation(); });
            next.RegisterCallback<ClickEvent>(e => { Play("LeftRight"); Change(+1); e.StopPropagation(); });
            Item(row, submit != null ? () => { submit(); val.text = value(); } : () => Change(+1), Change);
            row.RegisterCallback<AttachToPanelEvent>(_ => val.text = value());
            // rows that another row changes (PRESET sets LIGHTING, MATERIALS: ENHANCED switches LIGHTING on, LIGHTING decides what MATERIALS
            // shows) follow it: the value is read again a few times a second while the page is shown (the scheduler stops off the panel)
            row.schedule.Execute(() => { try { var t = value(); if (t != val.text) val.text = t; } catch (Exception) { } }).Every(250);
            row.userData ??= new OptNavItem();
            return row;
        }

        /// A min..max slider row in steps of step; fmt labels the value (null: a percentage of 0..1).
        public static VisualElement Slider(string label, Func<float> get, Action<float> set, VisualElement parent,
                                           float min = 0f, float max = 1f, float step = 0.05f, Func<float, string> fmt = null, Action submit = null)
        {
            var row = Div("mxa-row", parent);
            Text(label, "mxa-row-label", row);
            var right = Div("mxa-row-value", row);
            var track = Div("mxa-slider", right);
            var fill = Div("mxa-slider-fill", track);
            var val = Text("", "mxa-value mxa-value-narrow", right);
            float Snap(float v) => Mathf.Clamp(min + Mathf.Round((v - min) / step) * step, min, max);
            void Show()
            {
                float v = get();
                fill.style.width = Length.Percent(Mathf.Clamp01((v - min) / (max - min)) * 100f);
                val.text = fmt != null ? fmt(v) : Mathf.RoundToInt(v * 100f) + "%";
            }
            track.pickingMode = PickingMode.Position;
            track.RegisterCallback<PointerDownEvent>(e =>
            {
                float w = track.resolvedStyle.width;
                if (w > 1) { set(Snap(min + Mathf.Clamp01(e.localPosition.x / w) * (max - min))); Show(); }
                e.StopPropagation();
            });
            Item(row, submit != null ? () => { submit(); Show(); } : null, d => { set(Snap(get() + d * step)); Show(); });
            row.RegisterCallback<AttachToPanelEvent>(_ => Show());
            Show();
            return row;
        }

        public static VisualElement Toggle(string label, Func<bool> get, Action<bool> set, VisualElement parent, string on = "ON", string off = "OFF")
            => Spinner(label, () => get() ? on : off, _ => set(!get()), parent);

        /// The hint bar (the shell's: the key caps follow the device in use).
        public static void Hints(VisualElement bar, params (string key, string label)[] hints) => Reflex.Port.OpenFrontEnd.OuUi.Hints(bar, hints);

        public static string KeySubmit => Reflex.Port.OpenFrontEnd.OuGlyphs.Submit;
        public static string KeyCancel => Reflex.Port.OpenFrontEnd.OuGlyphs.Cancel;
        public static string KeyTabs => Reflex.Port.OpenFrontEnd.OuGlyphs.Tabs;
    }

    /// Drives the navigation of the options pages (the shell's OuNav with the options' sounds).
    public class OptNav : Reflex.Port.OpenFrontEnd.OuNav
    {
        protected override void Play(string sound) => OptUi.Play(sound);
        public void Update(OptMenuInput input) => base.Update(input);
    }
}
