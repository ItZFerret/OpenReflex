// The OPEN REFLEX shell's look, ported from Open Unleashed's MxaFrontEnd (Scripts/UI/MxaFrontEnd.cs): the theme
// (Resources/OpenFrontEnd/OuFrontEnd.uss on top of OuMenu.uss, scoped to .ou-frontend, plus OuReflex.uss for what the Reflex shell
// adds), the screen skeletons (Resources/OpenFrontEnd/FrontEnd/*.uxml: open them in UI Builder to adjust the layout; the code fills
// the named elements), the logo (Logo: the project's logo picture, Resources/OpenFrontEnd/OpenReflexLogo.png; without it the shell's name, OpenReflex.Name, set in Barlow Condensed with a slanted bar;
// renaming the shell is one constant) and the vector elements the stylesheet colours:
//   OuShape   a slanted panel (parallelogram) drawn with Painter2D; USS: --shape-fill, --shape-stroke, --shape-stroke-width,
//             --shape-skew (px, the top edge's shift against the bottom)
//   OuStar    a five-point star (the track browser's favourites); USS: --shape-fill
// The fonts are Barlow / Barlow Condensed (Resources/OpenFrontEnd/Fonts, SIL Open Font License: OFL-Barlow.txt beside them).
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OuTheme
    {
        public const string RootClass = "ou-frontend";

        /// The front end's stylesheets and root class on a menu document's root.
        public static void Apply(VisualElement root)
        {
            foreach (var name in new[] { "OuFrontEnd", "OuReflex", "OuModern", "OuCareer", "OuRestyle", "OuP2Front" })
            {
                var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + name);
                if (uss && !root.styleSheets.Contains(uss)) root.styleSheets.Add(uss);
            }
            root.AddToClassList(RootClass);
            OuMotion.Sync(root);
        }

        /// A screen skeleton (Resources/OpenFrontEnd/FrontEnd/<name>.uxml) cloned into parent; null when the asset is missing.
        public static VisualElement Clone(string name, VisualElement parent)
        {
            var tree = Resources.Load<VisualTreeAsset>(OuUi.ResourceRoot + "FrontEnd/" + name);
            if (!tree) { Debug.LogWarning("[OpenReflex] missing " + OuUi.ResourceRoot + "FrontEnd/" + name + ".uxml"); return null; }
            int before = parent.childCount;
            tree.CloneTree(parent);
            return parent.childCount > before ? parent[before] : null;
        }

        /// A named element of a cloned skeleton, or a new one (the skeleton missing) so the screen still works.
        public static VisualElement Part(VisualElement tree, string name, string cls) => tree.Q<VisualElement>(name) ?? OuUi.Div(cls, tree);

        /// The screen header: title, a small caption under it, and a slot on the right (returned) for the screen's controls.
        public static VisualElement Header(VisualElement parent, string title, string crumb)
        {
            var head = OuUi.Div("ou-header", parent);
            var mark = new OuShape { name = "ou-header-mark" };
            mark.AddToClassList("ou-header-mark");
            head.Add(mark);
            var titles = OuUi.Div("ou-header-titles", head);
            OuUi.Text(title, "ou-header-title", titles);
            if (!string.IsNullOrEmpty(crumb)) OuUi.Text(crumb, "ou-header-crumb", titles);
            OuUi.Div("ou-spacer", head);
            return OuUi.Div("ou-header-right", head);
        }

        /// A slanted background shape as e's first child (the USS colours it per state: .x:focus > .ou-shape ...).
        public static OuShape Backdrop(VisualElement e, string cls = null)
        {
            var s = new OuShape();
            if (!string.IsNullOrEmpty(cls)) foreach (var c in cls.Split(' ')) s.AddToClassList(c);
            s.AddToClassList("ou-backdrop");
            e.Insert(0, s);
            return s;
        }

        /// A section title inside a page (the garage's / setup's).
        public static void Section(VisualElement parent, string title)
        {
            var head = OuUi.Div(parent.childCount == 0 ? "ou-section ou-section-first" : "ou-section", parent);
            Backdrop(OuUi.Div("ou-section-mark", head));
            OuUi.Text(title.ToUpperInvariant(), "ou-section-title", head);
        }

        /// A summary chip (the setup's cards): a label, a value under it.
        public static VisualElement Chip(VisualElement parent, string label, string value, bool accent = false)
        {
            var c = OuUi.Div(accent ? "ou-schip ou-schip-accent" : "ou-schip", parent);
            OuUi.Text(label, "ou-schip-label", c);
            OuUi.Text(value, "ou-schip-value", c).enableRichText = false;
            return c;
        }

        /// The project's logo (Resources/OpenFrontEnd/OpenReflexLogo.png, cropped to its artwork), sized from `size`; without the
        /// picture, the text wordmark: the shell's name (OpenReflex.Name) in two lines, the last word large, on a slanted bar. size =
        /// the large word's font size (the hub: 150; the status bar / loading: small).
        public static VisualElement Logo(VisualElement parent, float size, bool small = false)
        {
            var logo = OuUi.Div(small ? "ou-logo ou-logo-small" : "ou-logo", parent);
            var art = LogoArt;
            if (art)
            {
                var img = OuUi.Div("ou-logo-art", logo);
                float h = Mathf.Round(size * (small ? 1.2f : 1.15f));
                img.style.height = h;
                img.style.width = Mathf.Round(h * art.width / art.height);
                img.style.backgroundImage = new StyleBackground(art);
                img.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Contain);
                img.style.flexShrink = 0; img.style.flexGrow = 0;
                return logo;
            }
            string name = OpenReflex.Name.Trim();
            int cut = name.LastIndexOf(' ');
            string top = cut > 0 ? name.Substring(0, cut) : "", big = cut > 0 ? name.Substring(cut + 1) : name;
            var bar = new OuShape(); bar.AddToClassList("ou-logo-bar"); logo.Add(bar);
            bar.style.height = Mathf.Round(size * 0.16f);
            bar.style.width = Mathf.Round(size * 0.25f * Mathf.Max(3, big.Length));
            if (top.Length > 0)
            {
                var t = OuUi.Text(top, "ou-logo-top", logo);
                t.style.fontSize = Mathf.Round(size * (small ? 0.62f : 0.3f)); t.style.letterSpacing = Mathf.Round(size * 0.08f);
                if (!small) t.style.marginBottom = -Mathf.Round(size * 0.14f);
            }
            var b = OuUi.Text(big, "ou-logo-big", logo); b.style.fontSize = size;
            if (!small) { b.style.marginTop = -Mathf.Round(size * 0.06f); b.style.marginBottom = -Mathf.Round(size * 0.1f); }
            return logo;
        }

        static Texture2D s_logoArt; static bool s_logoLooked;
        static Texture2D LogoArt
        {
            get
            {
                if (!s_logoLooked) { s_logoLooked = true; s_logoArt = Resources.Load<Texture2D>(OuUi.ResourceRoot + "OpenReflexLogo"); }
                return s_logoArt;
            }
        }
    }

    /// A parallelogram (the front end's slanted bars and buttons), coloured by USS custom properties.
    [UxmlElement]
    public partial class OuShape : VisualElement
    {
        static readonly CustomStyleProperty<Color> FillProp = new CustomStyleProperty<Color>("--shape-fill");
        static readonly CustomStyleProperty<Color> StrokeProp = new CustomStyleProperty<Color>("--shape-stroke");
        static readonly CustomStyleProperty<float> StrokeWidthProp = new CustomStyleProperty<float>("--shape-stroke-width");
        static readonly CustomStyleProperty<float> SkewProp = new CustomStyleProperty<float>("--shape-skew");

        Color _fill = Color.clear, _stroke = Color.clear;
        float _strokeWidth, _skew = 16f;

        public OuShape()
        {
            pickingMode = PickingMode.Ignore;
            AddToClassList("ou-shape");
            generateVisualContent += Draw;
            RegisterCallback<CustomStyleResolvedEvent>(OnStyle);
        }

        void OnStyle(CustomStyleResolvedEvent e)
        {
            var s = e.customStyle;
            Color fill = s.TryGetValue(FillProp, out var f) ? f : Color.clear;
            Color stroke = s.TryGetValue(StrokeProp, out var st) ? st : Color.clear;
            float width = s.TryGetValue(StrokeWidthProp, out var w) ? w : 0f;
            float skew = s.TryGetValue(SkewProp, out var k) ? k : 16f;
            if (fill == _fill && stroke == _stroke && width == _strokeWidth && skew == _skew) return;
            _fill = fill; _stroke = stroke; _strokeWidth = width; _skew = skew;
            MarkDirtyRepaint();
        }

        void Draw(MeshGenerationContext ctx)
        {
            float w = layout.width, h = layout.height;
            if (w < 1f || h < 1f || (_fill.a <= 0f && (_stroke.a <= 0f || _strokeWidth <= 0f))) return;
            float k = Mathf.Clamp(_skew, -w * 0.5f, w * 0.5f);
            float inset = _strokeWidth * 0.5f;
            var p = ctx.painter2D;
            p.BeginPath();
            // positive skew leans right (like italic type): the top edge starts k further right than the bottom one
            p.MoveTo(new Vector2(Mathf.Max(k, 0f) + inset, inset));
            p.LineTo(new Vector2(w - Mathf.Max(-k, 0f) - inset, inset));
            p.LineTo(new Vector2(w - Mathf.Max(k, 0f) - inset, h - inset));
            p.LineTo(new Vector2(Mathf.Max(-k, 0f) + inset, h - inset));
            p.ClosePath();
            if (_fill.a > 0f) { p.fillColor = _fill; p.Fill(); }
            if (_stroke.a > 0f && _strokeWidth > 0f) { p.strokeColor = _stroke; p.lineWidth = _strokeWidth; p.lineJoin = LineJoin.Miter; p.Stroke(); }
        }
    }

    /// A five-point star filling the element (favourites), coloured by --shape-fill.
    [UxmlElement]
    public partial class OuStar : VisualElement
    {
        static readonly CustomStyleProperty<Color> FillProp = new CustomStyleProperty<Color>("--shape-fill");
        Color _fill = Color.white;

        public OuStar()
        {
            pickingMode = PickingMode.Ignore;
            AddToClassList("ou-star");
            generateVisualContent += Draw;
            RegisterCallback<CustomStyleResolvedEvent>(e =>
            {
                if (e.customStyle.TryGetValue(FillProp, out var f) && f != _fill) { _fill = f; MarkDirtyRepaint(); }
            });
        }

        void Draw(MeshGenerationContext ctx)
        {
            float w = layout.width, h = layout.height;
            if (w < 1f || h < 1f) return;
            var c = new Vector2(w * 0.5f, h * 0.53f);
            float r = Mathf.Min(w, h) * 0.5f, ri = r * 0.45f;
            var p = ctx.painter2D;
            p.BeginPath();
            for (int i = 0; i < 10; i++)
            {
                float a = -Mathf.PI * 0.5f + i * Mathf.PI / 5f;
                float rr = (i & 1) == 0 ? r : ri;
                var v = c + new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * rr;
                if (i == 0) p.MoveTo(v); else p.LineTo(v);
            }
            p.ClosePath();
            p.fillColor = _fill;
            p.Fill();
        }
    }
}
