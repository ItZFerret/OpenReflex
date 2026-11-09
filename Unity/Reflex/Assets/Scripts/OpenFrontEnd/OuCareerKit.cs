// The OPEN REFLEX career pages' look, INSPIRED by the original MX vs ATV Reflex front end (Race & Ride / Events / Location select) and
// modernised: the original's traits kept are listed in core/plan/QUEUE.md (w-careerui). Resources/OpenFrontEnd/OuCareer.uss is the style
// sheet; this file is what it cannot make:
//   OcTex   procedural pictures (the warm grey vignette stage, the halftone dot fields, the dot tile), built once at run time
//   OcArrow the gold navigation triangle with its drop shadow
//   OcUi    the page parts: stage, orange title-bar panel, page-title plaque, footer legend band, patch buttons, trophies, vehicle
//           silhouettes (the install's own IconVehicles / IconTrophy textures, read at run time, nothing of the game is in the project)
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OcTex
    {
        static Texture2D s_vignette, s_dots, s_tile;

        static Texture2D Make(int w, int h, string name, Color32[] px, FilterMode filter = FilterMode.Bilinear, TextureWrapMode wrap = TextureWrapMode.Clamp)
        {
            var t = new Texture2D(w, h, TextureFormat.RGBA32, false) { name = name, wrapMode = wrap, filterMode = filter, hideFlags = HideFlags.DontSave };
            t.SetPixels32(px); t.Apply(false, true);
            return t;
        }

        /// The stage: warm grey, light in the middle and dark at the edges (the original's Background01 vignette), dithered.
        public static Texture2D Vignette
        {
            get
            {
                if (s_vignette) return s_vignette;
                const int w = 384, h = 216;
                var px = new Color32[w * h]; var rng = new System.Random(11);
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                    {
                        float u = (x + 0.5f) / w, v = (y + 0.5f) / h;                   // v grows upwards
                        float dx = (u - 0.42f) / 0.78f, dy = (v - 0.50f) / 0.92f;
                        float r = Mathf.Sqrt(dx * dx + dy * dy);
                        float t = 1f - Mathf.SmoothStep(0f, 1f, r);
                        t = Mathf.Pow(t, 1.15f);
                        float top = Mathf.Lerp(0.8f, 1f, Mathf.SmoothStep(0f, 0.55f, v));   // the top edge a little darker
                        float c = Mathf.Lerp(44f, 196f, t * top) + rng.Next(-1, 2);
                        px[y * w + x] = new Color32((byte)Mathf.Clamp(c * 1.03f, 0, 255), (byte)Mathf.Clamp(c * 1.0f, 0, 255), (byte)Mathf.Clamp(c * 0.95f, 0, 255), 255);
                    }
                return s_vignette = Make(w, h, "oc_vignette", px);
            }
        }

        /// Big halftone blots (the original's DOTS01..04): white dots on a 45 degree grid whose size follows a slow noise; tinted by USS.
        public static Texture2D Dots
        {
            get
            {
                if (s_dots) return s_dots;
                const int w = 1024, h = 576; const float pitch = 15f;
                var px = new Color32[w * h];
                float c = Mathf.Cos(Mathf.PI / 4f), s = Mathf.Sin(Mathf.PI / 4f);
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                    {
                        float rx = (x * c - y * s) / pitch, ry = (x * s + y * c) / pitch;
                        float fx = rx - Mathf.Floor(rx) - 0.5f, fy = ry - Mathf.Floor(ry) - 0.5f;
                        float d = Mathf.Sqrt(fx * fx + fy * fy) * pitch;
                        float n = 0.5f + 0.5f * (Mathf.Sin(x * 0.0061f + 1.3f) * 0.6f + Mathf.Sin(y * 0.0083f + x * 0.0021f + 0.4f) * 0.5f + Mathf.Sin((x + y) * 0.0034f + 2.2f) * 0.4f) / 1.2f;
                        float cover = Mathf.Clamp01((n - 0.34f) * 2.2f);
                        float r = pitch * 0.62f * cover;
                        float a = Mathf.Clamp01((r - d) * 1.6f + 0.5f);
                        px[y * w + x] = new Color32(255, 255, 255, (byte)(a * 255f));
                    }
                return s_dots = Make(w, h, "oc_dots", px);
            }
        }

        /// A tile of one dot (the patch buttons' and plaque's halftone fill: background-repeat).
        public static Texture2D Tile
        {
            get
            {
                if (s_tile) return s_tile;
                const int n = 16; var px = new Color32[n * n];
                for (int y = 0; y < n; y++)
                    for (int x = 0; x < n; x++)
                    {
                        float d1 = Vector2.Distance(new Vector2(x + 0.5f, y + 0.5f), new Vector2(n * 0.5f, n * 0.5f));
                        float d2 = Mathf.Min(Mathf.Min(Vector2.Distance(new Vector2(x + 0.5f, y + 0.5f), Vector2.zero), Vector2.Distance(new Vector2(x + 0.5f, y + 0.5f), new Vector2(n, 0))),
                                             Mathf.Min(Vector2.Distance(new Vector2(x + 0.5f, y + 0.5f), new Vector2(0, n)), Vector2.Distance(new Vector2(x + 0.5f, y + 0.5f), new Vector2(n, n))));
                        float a = Mathf.Max(Mathf.Clamp01(3.4f - d1 + 0.5f), Mathf.Clamp01(3.4f - d2 + 0.5f));
                        px[y * n + x] = new Color32(255, 255, 255, (byte)(a * 255f));
                    }
                return s_tile = Make(n, n, "oc_tile", px, FilterMode.Bilinear, TextureWrapMode.Repeat);
            }
        }
    }

    /// The gold navigation triangle with the original's dark drop shadow.
    public sealed class OcArrow : VisualElement
    {
        readonly bool left;
        public OcArrow(bool left)
        {
            this.left = left; pickingMode = PickingMode.Ignore; AddToClassList("oc-arrow");
            generateVisualContent += Draw;
        }
        void Draw(MeshGenerationContext ctx)
        {
            float w = layout.width, h = layout.height; if (w < 2 || h < 2) return;
            var p = ctx.painter2D;
            void Tri(float ox, float oy, Color col)
            {
                p.BeginPath();
                if (left) { p.MoveTo(new Vector2(w * 0.1f + ox, h * 0.5f + oy)); p.LineTo(new Vector2(w * 0.9f + ox, oy + 2)); p.LineTo(new Vector2(w * 0.9f + ox, h - 2 + oy)); }
                else { p.MoveTo(new Vector2(w * 0.9f + ox, h * 0.5f + oy)); p.LineTo(new Vector2(w * 0.1f + ox, oy + 2)); p.LineTo(new Vector2(w * 0.1f + ox, h - 2 + oy)); }
                p.ClosePath(); p.fillColor = col; p.Fill();
            }
            Tri(-4f, 6f, new Color(0.055f, 0.06f, 0.06f, 0.78f));
            Tri(0f, 0f, new Color(0.85f, 0.64f, 0.25f, 1f));
        }
    }

    /// The career pages' parts.
    public static class OcUi
    {
        // ------------------------------------------------------------------------------------------------ the install's pictures

        static readonly Dictionary<string, Texture2D> s_install = new Dictionary<string, Texture2D>();
        static readonly Dictionary<string, Sprite> s_sprites = new Dictionary<string, Sprite>();

        /// A texture of the install's FrontEnd package (IconTrophyGold, LockIcon, IconVehicles0 ...); null when it cannot be read.
        public static Texture2D FrontEndTexture(string name)
        {
            if (s_install.TryGetValue(name, out var t) && t) return t;
            Texture2D value = null;
            try { var data = OriginalUiAssets.Data; value = data.Texture("ui/textures/" + data.Pkg("FrontEnd") + "/" + name + ".png"); }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] front end texture " + name + ": " + e.Message); }
            return s_install[name] = value;
        }

        /// 0 gold, 1 silver, 2 bronze (BestPlaceFinish); null for anything else.
        public static Texture2D Trophy(int place) => place == 0 ? FrontEndTexture("IconTrophyGold") : place == 1 ? FrontEndTexture("IconTrophySilver") : place == 2 ? FrontEndTexture("IconTrophyBronze") : null;
        public static Texture2D Lock => FrontEndTexture("LockIcon");
        public static Texture2D LockSmall => FrontEndTexture("StatusLockedMini");

        // the six silhouettes of IconVehicles0 (256 x 256, x / y from the top): MX, ATV, UTV, buggy, sport truck, pro 2 truck
        static readonly Dictionary<string, RectInt> Silhouettes = new Dictionary<string, RectInt>
        {
            { "MX", new RectInt(128, 128, 128, 64) }, { "ATV", new RectInt(128, 64, 128, 64) }, { "UTV", new RectInt(0, 64, 128, 64) },
            { "BUGGY", new RectInt(0, 128, 128, 64) }, { "TRUCK", new RectInt(128, 6, 128, 52) }, { "TRUCK2", new RectInt(0, 6, 128, 52) },
        };

        /// The silhouette kind of a VehicleType id (VehicleType: 2 MX Lite, 3 MX, 4 ATV, 11 UTV, 12 super buggy, 14 / 15 trucks, 16 ATV, 17 MX).
        public static string SilhouetteOf(int vehicleTypeId) =>
            vehicleTypeId == 2 || vehicleTypeId == 3 || vehicleTypeId == 17 ? "MX" : vehicleTypeId == 4 || vehicleTypeId == 16 ? "ATV" : vehicleTypeId == 11 ? "UTV" :
            vehicleTypeId == 12 ? "BUGGY" : vehicleTypeId == 14 ? "TRUCK" : vehicleTypeId == 15 ? "TRUCK2" : null;

        public static Background Silhouette(string kind)
        {
            if (kind == null || !Silhouettes.TryGetValue(kind, out var r)) return default;
            if (s_sprites.TryGetValue(kind, out var cached) && cached) return Background.FromSprite(cached);
            var tex = FrontEndTexture("IconVehicles0"); if (!tex) return default;
            var sprite = Sprite.Create(tex, new Rect(r.x, tex.height - r.y - r.height, r.width, r.height), new Vector2(0.5f, 0.5f), 100f);
            sprite.hideFlags = HideFlags.DontSave; s_sprites[kind] = sprite;
            return Background.FromSprite(sprite);
        }

        // ------------------------------------------------------------------------------------------------ parts

        /// The stage behind a page. opaque: the front end's warm grey vignette with halftone blots (the career list); otherwise a dark
        /// warm veil with the same blots for the pages drawn over a world (rules, gates, results, rewards).
        public static VisualElement Stage(VisualElement parent, bool opaque)
        {
            var stage = OuUi.Div(opaque ? "oc-stage" : "oc-stage oc-stage-dim", parent);
            stage.pickingMode = PickingMode.Ignore;
            if (opaque) { var glow = OuUi.Div("oc-stage-glow", stage); glow.pickingMode = PickingMode.Ignore; glow.style.backgroundImage = OcTex.Vignette; }
            var dots = OuUi.Div("oc-stage-dots", stage); dots.pickingMode = PickingMode.Ignore; dots.style.backgroundImage = OcTex.Dots;
            return stage;
        }

        /// The original's information panel: an orange title bar (dark condensed type) over a dark glass body; returns the panel, the body
        /// and the title label.
        public static VisualElement Panel(VisualElement parent, string title, out VisualElement body, out Label titleLabel, string cls = null)
        {
            var panel = OuUi.Div("oc-panel" + (string.IsNullOrEmpty(cls) ? "" : " " + cls), parent);
            var bar = OuUi.Div("oc-panel-bar", panel);
            titleLabel = OuUi.Text(title ?? "", "oc-panel-title", bar); titleLabel.enableRichText = false;
            body = OuUi.Div("oc-panel-body", panel);
            return panel;
        }

        /// The page title on a dotted patch at the bottom right (the original's "RACE & RIDE" plate).
        public static VisualElement Plaque(VisualElement parent, string text, string cls = null)
        {
            var wrap = OuUi.Div("oc-plaque-row" + (string.IsNullOrEmpty(cls) ? "" : " " + cls), parent);
            var patch = OuUi.Div("oc-plaque", wrap); patch.style.backgroundImage = OcTex.Tile;
            var l = OuUi.Text(text ?? "", "oc-plaque-text", patch); l.enableRichText = false;
            return wrap;
        }

        /// The footer legend: a dark band across the bottom with the button caps and what they do (the original's BACK / GARAGE legend).
        /// An item with an action can be clicked (the original's legend items were mouse targets too).
        public static VisualElement Legend(VisualElement parent, params (string key, string label, Action click)[] items)
        {
            var band = OuUi.Div("oc-legend", parent); band.pickingMode = PickingMode.Ignore;
            void Inset() { var i = OuLayout.Insets(); band.style.paddingLeft = i.x; band.style.paddingRight = i.z; }
            band.RegisterCallback<AttachToPanelEvent>(_ => Inset()); band.RegisterCallback<GeometryChangedEvent>(_ => Inset());
            var row = OuUi.Div("oc-legend-items", band); row.pickingMode = PickingMode.Ignore;
            var keys = new (string, string)[items.Length];
            for (int i = 0; i < items.Length; i++) keys[i] = (items[i].key, items[i].label);
            OuUi.Hints(row, keys);
            int n = 0;
            foreach (var e in row.Children())
            {
                var click = items[n++].click;
                if (click == null) { e.pickingMode = PickingMode.Ignore; continue; }
                e.pickingMode = PickingMode.Position; e.AddToClassList("oc-legend-click");
                e.RegisterCallback<ClickEvent>(ev => { if (ev.button != 0) return; OuSound.Play("Back"); click(); });
            }
            return band;
        }

        /// A patch button (the original's dotted plates with orange type; gold type and a lift when focused).
        public static VisualElement Button(string label, string name, Action submit, VisualElement parent, string cls = null)
        {
            var b = OuUi.Div("mxa-button oc-btn" + (string.IsNullOrEmpty(cls) ? "" : " " + cls), parent);
            b.style.backgroundImage = OcTex.Tile;
            var l = OuUi.Text(label, "mxa-button-label oc-btn-label", b); l.enableRichText = false;
            OuUi.Item(b, submit);
            if (!string.IsNullOrEmpty(name)) b.name = name;
            return b;
        }

        /// A small dark tag with warm type (LOCKED, a format ...).
        public static Label Tag(VisualElement parent, string text, string kind = null)
        {
            var l = OuUi.Text(text, "oc-tag" + (string.IsNullOrEmpty(kind) ? "" : " oc-tag-" + kind), parent); l.enableRichText = false;
            return l;
        }

        /// A medal square: the place's colour (gold 0 / silver 1 / bronze 2), a lock, or an empty slot with its number.
        public static VisualElement Slot(VisualElement parent, int number, int medal, bool locked, bool invalid = false, string cls = null)
        {
            var s = OuUi.Div("oc-slot" + (invalid ? " oc-slot-invalid" : locked ? " oc-slot-locked" : medal == 0 ? " oc-slot-gold" : medal == 1 ? " oc-slot-silver" : medal == 2 ? " oc-slot-bronze" : "") +
                             (string.IsNullOrEmpty(cls) ? "" : " " + cls), parent);
            s.pickingMode = PickingMode.Ignore;
            if (locked && !invalid) { var icon = OuUi.Div("oc-slot-lock", s); icon.pickingMode = PickingMode.Ignore; var t = LockSmall ?? Lock; if (t) icon.style.backgroundImage = t; else OuUi.Text("x", "oc-slot-num", icon); }
            else if (!invalid) OuUi.Text(number > 0 ? number.ToString() : "", "oc-slot-num", s);
            return s;
        }

        /// The page header of the pages over a world: a small orange line over the big title (the original's page names).
        public static VisualElement Header(VisualElement parent, string title, string crumb)
        {
            var h = OuUi.Div("oc-header", parent); h.pickingMode = PickingMode.Ignore;
            var copy = OuUi.Div("oc-header-copy", h); copy.pickingMode = PickingMode.Ignore;
            if (!string.IsNullOrEmpty(crumb)) OuUi.Text(crumb, "oc-eyebrow", copy).enableRichText = false;
            OuUi.Text(title ?? "", "oc-big", copy).enableRichText = false;
            return h;
        }

        /// The place number with its suffix and, for the podium, the trophy: "7 TH", "1 ST" + gold cup.
        public static VisualElement Place(VisualElement parent, int place, string suffix)
        {
            var p = OuUi.Div("oc-place" + (place >= 1 && place <= 3 ? " oc-place-" + place : ""), parent); p.pickingMode = PickingMode.Ignore;
            OuUi.Text(place.ToString(), "oc-place-num", p); OuUi.Text(suffix, "oc-place-suffix", p);
            var cup = place >= 1 && place <= 3 ? Trophy(place - 1) : null;
            if (cup) { var t = OuUi.Div("oc-trophy", p); t.style.backgroundImage = cup; t.pickingMode = PickingMode.Ignore; }
            return p;
        }

        /// A label over a big value tile; kind "gold" / "good".
        public static VisualElement Stat(VisualElement parent, string label, string value, string kind = null)
        {
            var s = OuUi.Div("oc-stat" + (string.IsNullOrEmpty(kind) ? "" : " oc-stat-" + kind), parent); s.pickingMode = PickingMode.Ignore;
            OuUi.Text(label, "oc-stat-label", s).enableRichText = false;
            OuUi.Text(value, "oc-stat-value", s).enableRichText = false;
            return s;
        }

        /// A wide label / value line (the rewards).
        public static VisualElement Reward(VisualElement parent, string label, string value, string kind = null)
        {
            var r = OuUi.Div("oc-reward" + (string.IsNullOrEmpty(kind) ? "" : " oc-reward-" + kind), parent); r.pickingMode = PickingMode.Ignore;
            OuUi.Text(label, "oc-reward-label", r).enableRichText = false;
            OuUi.Text(value, "oc-reward-value", r).enableRichText = false;
            return r;
        }

        /// The board's heading: an orange bar with the column names.
        public static VisualElement BoardHead(VisualElement parent, string rank, string name, string status, string time, string points)
        {
            var head = OuUi.Div("oc-board-head", parent); head.pickingMode = PickingMode.Ignore;
            OuUi.Text(rank, "oc-row-rank", head).style.fontSize = 22;
            OuUi.Text(name, "oc-row-cell oc-row-name", head).style.fontSize = 22;
            if (status != null) OuUi.Text(status, "oc-row-cell oc-row-status", head).style.fontSize = 22;
            if (time != null) OuUi.Text(time, "oc-row-cell oc-row-time", head).style.fontSize = 22;
            if (points != null) OuUi.Text(points, "oc-row-cell oc-row-points", head).style.fontSize = 22;
            foreach (var l in head.Query<Label>().ToList()) { l.enableRichText = false; l.style.color = new Color(0.055f, 0.06f, 0.06f); }
            return head;
        }

        /// One board row: rank slab (gold / silver / bronze on the podium), name, optional status / time / points; the local rider gold-edged.
        public static VisualElement BoardRow(VisualElement parent, int rank, string name, bool you, string status = null, string time = null, string points = null)
        {
            var row = OuUi.Item(OuUi.Div("oc-row" + (you ? " oc-row-you" : "") + (rank >= 1 && rank <= 3 ? " oc-row-" + rank : ""), parent), null);
            OuUi.Text(rank > 0 ? rank.ToString() : "-", "oc-row-rank", row).enableRichText = false;
            OuUi.Text(name, "oc-row-cell oc-row-name", row).enableRichText = false;
            if (status != null) OuUi.Text(status, "oc-row-cell oc-row-status", row).enableRichText = false;
            if (time != null) OuUi.Text(time, "oc-row-cell oc-row-time", row).enableRichText = false;
            if (points != null) OuUi.Text(points, "oc-row-cell oc-row-points", row).enableRichText = false;
            return row;
        }

        /// A label over a value (the event panel's facts).
        public static VisualElement Fact(VisualElement parent, string label, string value)
        {
            var f = OuUi.Div("oc-fact", parent); f.pickingMode = PickingMode.Ignore;
            OuUi.Text(label, "oc-fact-label", f).enableRichText = false;
            OuUi.Text(value ?? "", "oc-fact-value", f).enableRichText = false;
            return f;
        }
    }
}
