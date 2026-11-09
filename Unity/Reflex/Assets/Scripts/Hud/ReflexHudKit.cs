// The MODERN HUD's visual language, adapted from Open Unleashed's MxaHudKit (Scripts/UI/MxaHudKit.cs): one type family (Barlow /
// Barlow Condensed, SIL OFL 1.1: Assets/OpenFrontEnd/Resources/OpenFrontEnd/Fonts, OFL-Barlow.txt), white text, translucent
// dark panels with soft gradients and fades, one green for good values / the player, one red for warnings. uGUI, built in code:
//   ReflexHudKit   the fonts, the colours, the canvas (1920 x 1080 reference, scaled by height) with a safe root (at most 2.1 : 1)
//   HudRect        a panel: rounded corners, a vertical tint and a horizontal fade on either side (MxaHudRect)
//   HudVector      antialiased discs, arcs and ticks in one mesh (MxaHudVector's): the speed gauge, the compass, the medal marks
//   HudAnim        the eased entrances / exits, the value flashes and the clock text (HudAnim.cs)
// The layout is built on an 8 unit grid (G) in a fixed type scale (Type*); HUD SCALE, HUD OPACITY and the title-safe inset are
// applied live (Scale, PanelOpacity, HudSafeRoot).
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    public static class ReflexHudKit
    {
        public const float RefW = 1920f, RefH = 1080f, MaxAspect = 2.1f, Margin = 40f, MarginTop = 32f;
        /// The layout grid (reference units) and the title-safe inset (a fraction of the safe area's height, on every side).
        public const float G = 8f, TitleSafe = 0.02f;
        /// The type scale (reference units at 1080p; the smallest text is TypeLabel: legible from a couch at 720p and up).
        public const int TypeDisplay = 200, TypeHero = 104, TypeNumXl = 88, TypeNumLg = 64, TypeNumMd = 56, TypeTitle = 48, TypeSub = 32, TypeBody = 26, TypeLabel = 22;

        /// HUD SCALE as a factor (1 = the design size): the canvas reference resolution is divided by it. Set by the view's options.
        public static float Scale = 1f;
        /// HUD OPACITY as a factor: multiplies the alpha of every panel (HudRect.panel) and the gauge's back disc; text stays opaque.
        public static float PanelOpacity = 1f;

        public static readonly Color White = new Color(0.97f, 0.97f, 0.96f, 1f);
        public static readonly Color Dim = new Color(0.97f, 0.97f, 0.96f, 0.62f);
        public static readonly Color Faint = new Color(0.97f, 0.97f, 0.96f, 0.32f);
        public static readonly Color Green = new Color(0.50f, 0.87f, 0.36f, 1f);
        public static readonly Color Red = new Color(0.86f, 0.10f, 0.12f, 1f);
        public static readonly Color Amber = new Color(1f, 0.74f, 0.18f, 1f);
        public static readonly Color Ink = new Color(0.08f, 0.085f, 0.095f, 1f);
        public static readonly Color PanelBg = new Color(0.075f, 0.08f, 0.09f, 0.80f);
        public static readonly Color PanelTop = new Color(0.17f, 0.18f, 0.20f, 0.80f);
        /// Red for text on a panel (brighter than the warning red of the accent bars).
        public static readonly Color RedText = new Color(1f, 0.35f, 0.32f, 1f);
        public static readonly Color Gold = new Color(1f, 0.80f, 0.28f, 1f);
        public static readonly Color Silver = new Color(0.82f, 0.85f, 0.88f, 1f);
        public static readonly Color Bronze = new Color(0.86f, 0.55f, 0.30f, 1f);
        public static readonly Color Shade = new Color(0f, 0f, 0f, 0.45f);

        static Font s_medium, s_semi, s_bold, s_cond, s_condBold, s_num, s_numHeavy;
        public static Font Medium => s_medium ??= Load("Barlow-Medium");
        public static Font SemiBold => s_semi ??= Load("Barlow-SemiBold");
        public static Font Bold => s_bold ??= Load("Barlow-Bold");
        public static Font Cond => s_cond ??= Load("BarlowCondensed-SemiBold");
        public static Font CondBold => s_condBold ??= Load("BarlowCondensed-Bold");
        public static Font Num => s_num ??= Load("BarlowCondensed-BoldItalic");
        public static Font NumHeavy => s_numHeavy ??= Load("BarlowCondensed-ExtraBoldItalic");

        static Font Load(string name)
        {
            var f = Resources.Load<Font>("OpenFrontEnd/Fonts/" + name);
            return f ? f : Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        }

        /// The canvas on go: overlay, 1920 x 1080 reference scaled by height; returns the safe root.
        public static RectTransform CreateCanvas(GameObject go, int sortingOrder, out Canvas canvas)
        {
            canvas = go.GetComponent<Canvas>(); if (!canvas) canvas = go.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = sortingOrder;
            var scaler = go.GetComponent<CanvasScaler>(); if (!scaler) scaler = go.AddComponent<CanvasScaler>();
            scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            ApplyScale(scaler);
            scaler.screenMatchMode = CanvasScaler.ScreenMatchMode.MatchWidthOrHeight;
            scaler.matchWidthOrHeight = 1f;
            var root = new GameObject("Safe", typeof(RectTransform)).GetComponent<RectTransform>();
            root.SetParent(go.transform, false);
            Stretch(root);
            go.AddComponent<HudSafeRoot>().Init(root);
            return root;
        }

        /// HUD SCALE onto a canvas scaler (reference resolution / scale: a larger scale makes every unit bigger on screen).
        public static void ApplyScale(CanvasScaler scaler)
        {
            float s = Mathf.Clamp(Scale, 0.5f, 2f);
            var r = new Vector2(RefW / s, RefH / s);
            if (scaler.referenceResolution != r) scaler.referenceResolution = r;
        }

        public static void Stretch(RectTransform rt) { rt.anchorMin = Vector2.zero; rt.anchorMax = Vector2.one; rt.pivot = new Vector2(0.5f, 0.5f); rt.offsetMin = rt.offsetMax = Vector2.zero; }

        public static RectTransform Rect(Transform parent, string name, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 size)
        {
            var rt = new GameObject(name, typeof(RectTransform)).GetComponent<RectTransform>();
            rt.SetParent(parent, false);
            rt.anchorMin = rt.anchorMax = anchor; rt.pivot = pivot; rt.anchoredPosition = pos; rt.sizeDelta = size;
            return rt;
        }

        public static Text Label(Transform parent, string name, Font font, int size, Color color, TextAnchor align, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 box)
        {
            var t = Rect(parent, name, anchor, pivot, pos, box).gameObject.AddComponent<Text>();
            t.font = font; t.fontSize = size; t.color = color; t.alignment = align;
            t.horizontalOverflow = HorizontalWrapMode.Overflow; t.verticalOverflow = VerticalWrapMode.Overflow;
            t.supportRichText = true; t.raycastTarget = false;
            return t;
        }

        /// A soft drop shadow under text that sits on the scene (numerals, the countdown): cheap, one extra pass.
        public static T Shadowed<T>(T t) where T : Graphic
        {
            var sh = t.gameObject.AddComponent<Shadow>();
            sh.effectColor = Shade; sh.effectDistance = new Vector2(0f, -2f); sh.useGraphicAlpha = true;
            return t;
        }

        public static Text Fill(Transform parent, string name, Font font, int size, Color color, TextAnchor align)
        {
            var t = Label(parent, name, font, size, color, align, Vector2.zero, new Vector2(0.5f, 0.5f), Vector2.zero, Vector2.zero);
            Stretch(t.rectTransform);
            return t;
        }

        public static HudRect Panel(Transform parent, string name, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 size, float fadeLeft = 0f, float fadeRight = 0f, float radius = 4f)
        {
            var p = Graphic<HudRect>(Rect(parent, name, anchor, pivot, pos, size));
            p.raycastTarget = false; p.top = PanelTop; p.bottom = PanelBg; p.fadeLeft = fadeLeft; p.fadeRight = fadeRight; p.radius = radius;
            return p;
        }

        public static HudRect Cell(Transform parent, string name, float x, float w, Color c)
        {
            var rt = Rect(parent, name, new Vector2(0, 0.5f), new Vector2(0, 0.5f), new Vector2(x, 0), new Vector2(w, 0));
            rt.anchorMin = new Vector2(0, 0); rt.anchorMax = new Vector2(0, 1); rt.sizeDelta = new Vector2(w, 0);
            var p = Graphic<HudRect>(rt);
            p.raycastTarget = false; p.top = p.bottom = c; p.radius = 0;
            return p;
        }

        public static HudVector Vector(Transform parent, string name, Vector2 anchor, Vector2 pos, Vector2 size)
        {
            var v = Graphic<HudVector>(Rect(parent, name, anchor, new Vector2(0.5f, 0.5f), pos, size));
            v.raycastTarget = false;
            return v;
        }

        /// A graphic of the kit (its CanvasRenderer added first: a script graphic's RequireComponent is not applied to a component
        /// added at run time here).
        public static T Graphic<T>(RectTransform rt) where T : Graphic
        {
            if (!rt.GetComponent<CanvasRenderer>()) rt.gameObject.AddComponent<CanvasRenderer>();
            return rt.gameObject.AddComponent<T>();
        }

        public static string Ordinal(int n)
        {
            int t = n % 100;
            if (t >= 11 && t <= 13) return "TH";
            switch (n % 10) { case 1: return "ST"; case 2: return "ND"; case 3: return "RD"; default: return "TH"; }
        }

        /// m:ss.ss (the game's times are seconds; none = dashes).
        public static string Clock(float t) => t <= 0f || t >= 3.0e38f || float.IsNaN(t) ? "-:--.--" : $"{(int)(t / 60f)}:{t - (int)(t / 60f) * 60f:00.00}";

        public static void Show(Component c, bool on) { if (c && c.gameObject.activeSelf != on) c.gameObject.SetActive(on); }
        public static void SetText(Text t, string s) { if (t && t.text != s) t.text = s; }
    }

}
