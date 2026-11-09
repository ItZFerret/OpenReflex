// The original game's retained-mode UI (see _re/notes/ui.md) for the front end, rebuilt at run time from the install:
// UIComponent trees ("uicmpnt" layers of Database/MXUI, read by Content/ReflexUiData.Layer), their art (texture assets and
// texatlas sprites of FrontEnd.<LANG> / Shared.<LANG> / BaseLocalized.<LANG> / Base, streamed icons of MXUI_Streaming named
// by the Icon table), the bitmap fonts (cell assets, OriginalFont) and the localisation tables. Nothing here is game data.
//
// Engine rules reproduced (UIComponent / UIContainer behaviour of the original, as observed in the layers + Lua):
// * layout: Position = where the pivot (Justification) of the Size box goes; children relative to the parent's top-left;
//   RotationZ in degrees; UITextComponent ResizeToText (default true) makes the box string-width x TextHeight.
// * state colours: Focused > Highlighted > Focusable > Enabled > Visible (UIColorState 0x5EAFF0), the first defined.
// * focus: the active layer is focused; a list container (UIListContainer / UIDynamicList / UIScrollingDynamicListContainer)
//   focuses the child at its focus index; a UIFocusContainer passes its focus to every child; other containers pass it to
//   children whose own Focused bit is set (Lua SetFocused toggles it).
// * baked 30 fps animations: position offset (added to the pivot position), scale about the component's top-left (the authored
//   position keys compensate: MainMenu "Arcade_Focused" scales 1.2 with a (-28,-8) offset on a 280x80 centred box), colour
//   multipliers that also tint the children of containers; layer / page composite animations reference component animations.
// * key triggers (UIKeypressTrigger MapKey -> ScriptFunction) fire on focused components; list navigation (NavigationType
//   LeftRight / UpDown, AllowWrap, ScrollSoundEvent / NoScrollSoundEvent) is done by the deepest focused list that takes the key.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.FrontEnd
{
    // ------------------------------------------------------------------------------------------------ assets
    public sealed class FeAssets
    {
        public readonly ReflexUiData data;
        public readonly string[] packages;      // material / font search order
        public Material baseMaterial;
        readonly Dictionary<string, (Texture2D tex, Rect uv)> materials = new Dictionary<string, (Texture2D, Rect)>();
        readonly Dictionary<string, OriginalFont> fonts = new Dictionary<string, OriginalFont>();
        readonly Dictionary<int, (Texture2D tex, Rect uv)> icons = new Dictionary<int, (Texture2D, Rect)>();
        readonly Dictionary<string, Texture2D> linear = new Dictionary<string, Texture2D>();
        Dictionary<string, string> strings, products, intro;
        public FeTables tables;

        public FeAssets(Shader shader)
        {
            data = OriginalUiAssets.Data;
            packages = new[] { data.Pkg("FrontEnd"), data.Pkg("Shared"), data.Pkg("BaseLocalized"), "Base", data.Pkg("InGame"), data.Pkg("FrontEndIntro") };   // FrontEndIntro: the startup pages (FrontEndVideo.cs)
            baseMaterial = new Material(shader ? shader : Shader.Find("UI/Default")) { name = "Reflex FrontEnd UI", hideFlags = HideFlags.DontSave };
        }

        public Dictionary<string, string> Strings => strings ??= data.Strings("FrontEnd", "MXRaven_Default_Strings");
        /// Localised text of a key ("STR_*"; the store-product names "STR_<id>" live in Shared's MXTables_Default_StoreProducts).
        public string Str(string key)
        {
            if (string.IsNullOrEmpty(key)) return "";
            if (Strings.TryGetValue(key, out var s)) return s;
            products ??= data.Has(data.Pkg("Shared"), "localiz", "MXTables_Default_StoreProducts") ? data.Strings("Shared", "MXTables_Default_StoreProducts") : new Dictionary<string, string>();
            if (products.TryGetValue(key, out s)) return s;
            intro ??= data.Has(data.Pkg("FrontEndIntro"), "localiz", "MXRaven_Intro") ? data.Strings("FrontEndIntro", "MXRaven_Intro") : new Dictionary<string, string>();   // the legal screen's texts
            if (intro.TryGetValue(key, out s)) return s;
            return tables != null && tables.DlcStrings(data.language).TryGetValue(key, out s) ? s : key;   // DLC store products (FeTables.DlcStrings)
        }

        /// A layer of the first package that has it (FrontEnd, then Shared).
        public ReflexUiData.LayerData Layer(string name)
        {
            foreach (var p in new[] { data.Pkg("FrontEnd"), data.Pkg("Shared"), data.Pkg("InGame"), data.Pkg("FrontEndIntro") })
                if (data.Has(p, "uicmpnt", name)) return data.Layer(p, name);
            return null;
        }

        /// VisibleMaterial name -> texture + uv rect: a texture asset of that name, else a texatlas sprite of that name.
        public (Texture2D tex, Rect uv) Material(string name)
        {
            if (string.IsNullOrEmpty(name)) return (null, new Rect(0, 0, 1, 1));
            if (materials.TryGetValue(name, out var m)) return m;
            m = (null, new Rect(0, 0, 1, 1));
            foreach (string p in packages)
                if (data.Has(p, "texture", name))
                {   // a render-target placeholder (1280_720_VideoRenderTarget: the garage / video surface) has no pixels: no art
                    try { m = (OriginalUiAssets.Texture(data.TextureKeyOf(p, name)), new Rect(0, 0, 1, 1)); }
                    catch (System.IO.InvalidDataException e) { Debug.Log("[FrontEnd] texture " + name + " not drawn: " + e.Message); }
                    goto done;
                }
            foreach (string p in packages)
                foreach (string atlas in data.Names(p, "texatlas"))
                {
                    Dictionary<string, ReflexUiData.AtlasSprite> sprites;
                    try { sprites = data.Atlas(p, atlas); } catch (Exception) { continue; }
                    if (sprites.TryGetValue(name, out var sp) && data.Has(p, "texture", sp.texture))
                    { m = (OriginalUiAssets.Texture(data.TextureKeyOf(p, sp.texture)), new Rect(sp.u0, 1f - sp.v1, sp.u1 - sp.u0, sp.v1 - sp.v0)); goto done; }
                }
            done:
            return materials[name] = m;
        }

        /// A bitmap font by cell name (FrontEnd's own cells first).
        public OriginalFont Font(string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            if (fonts.TryGetValue(name, out var f)) return f;
            foreach (string p in packages)
                if (data.Has(p, "cell", name)) { f = OriginalUiAssets.Font("ui/fonts/" + p + "." + name + ".json"); break; }
            return fonts[name] = f;
        }

        /// A texture decoded as raw data (no sRGB): masks.
        public Texture2D Linear(string name)
        {
            if (linear.TryGetValue(name, out var t)) return t;
            foreach (string p in packages)
                if (data.Has(p, "texture", name))
                {
                    byte[] rgba = ReflexUiData.DecodeTexture(data.Read(p, "texture", name), out int w, out int h, out _);
                    t = MakeTexture(rgba, w, h, name, true); break;
                }
            return linear[name] = t;
        }

        static Texture2D MakeTexture(byte[] rgba, int w, int h, string name, bool lin)
        {
            var flipped = new byte[rgba.Length]; int row = w * 4;
            for (int y = 0; y < h; y++) Buffer.BlockCopy(rgba, y * row, flipped, (h - 1 - y) * row, row);
            var t = new Texture2D(w, h, TextureFormat.RGBA32, false, lin) { name = name, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            t.SetPixelData(flipped, 0); t.Apply(false, true);
            return t;
        }

        /// The icon's texture asset when it is a texture asset of an install database other than MXUI's (MXUI_Streaming: ArcadePosters_*,
        /// Locations_*, LocationSplines_*: the location pictures; Icon's second case), else null (an atlas sprite, a texture of MXUI, none). Main
        /// thread (the tables); the asset's pixels can then be read on any thread (DecodeIconAsset: w-menufps, the menus' pictures off the main thread).
        internal Rfx.Asset IconStreamAsset(int iconId)
        {
            try
            {
                if (tables == null || !tables.IconAsset(iconId, out string asset, out string package, out string database) || database == ReflexUiData.DatabaseName) return null;
                return ReflexInstall.Packages.Find(database, package, asset, "texture");
            }
            catch (Exception) { return null; }
        }

        /// A texture asset's pixels as Unity takes them (RGBA32, rows bottom first): read and decoded on the calling thread (thread-safe).
        internal static byte[] DecodeIconAsset(Rfx.Asset a, out int w, out int h)
        {
            byte[] rgba = ReflexUiData.DecodeTexture(ReflexInstall.Packages.Read(a), out w, out h, out _);
            var flipped = new byte[rgba.Length]; int row = w * 4;
            for (int y = 0; y < h; y++) Buffer.BlockCopy(rgba, y * row, flipped, (h - 1 - y) * row, row);
            return flipped;
        }

        /// MXUIStreamableImageIcon:SetIconId: Icon row -> AssetName + PackageDatabaseId -> (AssetPackage, AssetDatabase): a sprite of
        /// the package's texatlas in MXUI (ActivityTypePosters) or a texture asset of MXUI_Streaming (ArcadePosters_*, Locations_*,
        /// LocationSplines_*). Null for -1 / IconNone.
        public (Texture2D tex, Rect uv) Icon(int iconId)
        {
            if (icons.TryGetValue(iconId, out var r)) return r;
            r = (null, new Rect(0, 0, 1, 1));
            try
            {
                if (tables != null && tables.IconAsset(iconId, out string asset, out string package, out string database))
                {
                    if (database == ReflexUiData.DatabaseName)
                    {   // language-less packages (ActivityTypePosters): read the atlas directly, texture keys by the exact package name
                        foreach (string atlas in data.Names(package, "texatlas"))
                        {
                            var sp = ReflexUiData.ParseAtlas(data.Read(package, "texatlas", atlas)).FirstOrDefault(x => x.name == asset);
                            if (sp != null) { r = (OriginalUiAssets.Texture("ui/textures/" + package + "/" + sp.texture + ".png"), new Rect(sp.u0, 1f - sp.v1, sp.u1 - sp.u0, sp.v1 - sp.v0)); break; }
                        }
                        if (r.tex == null && data.Has(package, "texture", asset)) r = (OriginalUiAssets.Texture("ui/textures/" + package + "/" + asset + ".png"), new Rect(0, 0, 1, 1));
                    }
                    else
                    {
                        var inst = ReflexInstall.Packages;
                        var a = inst.Find(database, package, asset, "texture");
                        if (a != null)
                        {
                            byte[] rgba = ReflexUiData.DecodeTexture(inst.Read(a), out int w, out int h, out _);
                            r = (MakeTexture(rgba, w, h, database + "/" + package + "/" + asset, false), new Rect(0, 0, 1, 1));
                        }
                    }
                }
            }
            catch (Exception e) { Debug.LogWarning("Front end icon " + iconId + " not read: " + e.Message); }
            return icons[iconId] = r;
        }
    }

    // ------------------------------------------------------------------------------------------------ graphics
    /// DefaultIcon: a textured quad (texture x colour), optionally desaturated (DesaturationLevel / 255, via uv1.x).
    [RequireComponent(typeof(CanvasRenderer))]
    public sealed class FeImage : MaskableGraphic
    {
        public Texture texture; public Rect uvRect = new Rect(0, 0, 1, 1); public float desaturation;
        public override Texture mainTexture => texture ? texture : s_WhiteTexture;
        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            Rect r = rectTransform.rect; var c = (Color32)color; var u1 = new Vector4(desaturation, 0, 0, 0);
            Rect uv = uvRect;
            vh.AddVert(new Vector3(r.xMin, r.yMin), c, new Vector4(uv.xMin, uv.yMin), u1, Vector3.back, new Vector4(1, 0, 0, -1));
            vh.AddVert(new Vector3(r.xMin, r.yMax), c, new Vector4(uv.xMin, uv.yMax), u1, Vector3.back, new Vector4(1, 0, 0, -1));
            vh.AddVert(new Vector3(r.xMax, r.yMax), c, new Vector4(uv.xMax, uv.yMax), u1, Vector3.back, new Vector4(1, 0, 0, -1));
            vh.AddVert(new Vector3(r.xMax, r.yMin), c, new Vector4(uv.xMax, uv.yMin), u1, Vector3.back, new Vector4(1, 0, 0, -1));
            vh.AddTriangle(0, 1, 2); vh.AddTriangle(2, 3, 0);
        }
        public void Dirty() => SetAllDirty();   // a new texture needs the material pass (CanvasRenderer.SetTexture) too
    }

    /// TextIcon: a string in an original bitmap font. ResizeToText: the box becomes width x TextHeight (pivot fixed); otherwise the
    /// text is placed in the authored box by the component's justification (pivot), word-wrapped when AllowWordWrap, lines
    /// TextHeight + LineSpacing apart. Glyph metrics and the width rule are OriginalFont's (FUN_007FAF10).
    [RequireComponent(typeof(CanvasRenderer))]
    public sealed class FeText : MaskableGraphic
    {
        public OriginalFont font; public float textHeight = 48, lineSpacing; public bool resize = true, wrap; public Vector2 align;   // align = pivot (0 left/top .. 1)
        public float desaturation;
        string text = "";
        public override Texture mainTexture => font != null && font.atlas ? font.atlas : s_WhiteTexture;
        public string Text { get => text; set { value ??= ""; if (value == text) return; text = value; if (resize) Resize(); SetVerticesDirty(); } }
        public Action<Vector2> onResize;

        public void Resize()
        {
            if (font == null || !resize) return;
            int lines = 1; foreach (char c in text) if (c == '\n') lines++;
            var s = new Vector2(LineWidthMax(text), textHeight * lines + lineSpacing * (lines - 1));
            rectTransform.sizeDelta = s; onResize?.Invoke(s);
        }

        bool Glyph(char c, out OriginalFont.Glyph g) => font.glyphs.TryGetValue(c, out g) || font.glyphs.TryGetValue('?', out g);
        public float LineWidth(string line)
        {
            if (font == null) return 0;
            float s = textHeight / font.size, w = 0; bool first = true;
            foreach (char c in line) { if (!Glyph(c, out var g)) continue; if (!first) w += font.letterSpacing + g.left; w += g.w + g.right; first = false; }
            return w * s;
        }
        float LineWidthMax(string t) { float b = 0; foreach (var l in t.Split('\n')) b = Mathf.Max(b, LineWidth(l)); return b; }

        List<string> Lines(float maxWidth)
        {
            var o = new List<string>();
            foreach (var para in text.Split('\n'))
            {
                if (!wrap || maxWidth <= 0) { o.Add(para); continue; }
                string line = "";
                foreach (var word in para.Split(' '))
                {
                    string cand = line.Length == 0 ? word : line + " " + word;
                    if (line.Length > 0 && LineWidth(cand) > maxWidth) { o.Add(line); line = word; } else line = cand;
                }
                o.Add(line);
            }
            return o;
        }

        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            if (font == null || string.IsNullOrEmpty(text)) return;
            Rect r = rectTransform.rect; var c = (Color32)color; var u1 = new Vector4(desaturation, 0, 0, 0);
            var lines = Lines(r.width);
            float s = textHeight / font.size, adv = textHeight + lineSpacing, blockH = lines.Count * textHeight + (lines.Count - 1) * lineSpacing;
            float top = resize ? r.yMax : r.yMax - (r.height - blockH) * align.y;
            for (int li = 0; li < lines.Count; li++)
            {
                string line = lines[li];
                float x = r.xMin + (resize && lines.Count == 1 ? 0 : (r.width - LineWidth(line)) * align.x), y = top - li * adv; bool first = true;
                foreach (char ch in line)
                {
                    if (!Glyph(ch, out var g)) continue;
                    if (!first) x += (font.letterSpacing + g.left) * s;
                    if (g.w > 0 && g.h > 0)
                    {
                        float x0 = x, x1 = x + g.w * s, y1 = y - (font.size - g.bottom - g.h) * s, y0 = y1 - g.h * s;
                        int i = vh.currentVertCount;
                        vh.AddVert(new Vector3(x0, y0), c, new Vector4(g.uv.xMin, g.uv.yMin), u1, Vector3.back, new Vector4(1, 0, 0, -1));
                        vh.AddVert(new Vector3(x0, y1), c, new Vector4(g.uv.xMin, g.uv.yMax), u1, Vector3.back, new Vector4(1, 0, 0, -1));
                        vh.AddVert(new Vector3(x1, y1), c, new Vector4(g.uv.xMax, g.uv.yMax), u1, Vector3.back, new Vector4(1, 0, 0, -1));
                        vh.AddVert(new Vector3(x1, y0), c, new Vector4(g.uv.xMax, g.uv.yMin), u1, Vector3.back, new Vector4(1, 0, 0, -1));
                        vh.AddTriangle(i, i + 1, i + 2); vh.AddTriangle(i + 2, i + 3, i);
                    }
                    x += (g.w + g.right) * s; first = false;
                }
            }
        }
    }

    // ------------------------------------------------------------------------------------------------ animation
    public sealed class FeAnim
    {
        public sealed class Track { public string target; public List<string> refs = new List<string>(); public List<(float t, Dictionary<string, float> ch)> keys = new List<(float, Dictionary<string, float>)>(); }
        public string name; public float length; public readonly List<Track> tracks = new List<Track>();

        public static FeAnim Parse(Dictionary<string, object> a)
        {
            var o = new FeAnim { name = OriginalUi.S(a, "name"), length = OriginalUi.F(a, "length") };
            foreach (var tr in OriginalUi.Arr(a, "tracks") ?? new List<object>())
            {
                var t = new Track { target = OriginalUi.S(tr, "target") };
                foreach (var r in OriginalUi.Arr(tr, "refs") ?? new List<object>()) if (r is string s) t.refs.Add(s);
                foreach (var k in OriginalUi.Arr(tr, "keys") ?? new List<object>())
                {
                    var ch = new Dictionary<string, float>();
                    if (OriginalUi.Obj(k, "ch") is Dictionary<string, object> cd) foreach (var kv in cd) if (kv.Value is double d) ch[kv.Key] = (float)d;
                    t.keys.Add((OriginalUi.F(k, "t"), ch));
                }
                t.keys.Sort((x, y) => x.t.CompareTo(y.t));
                o.tracks.Add(t);
            }
            return o;
        }
    }

    // ------------------------------------------------------------------------------------------------ components
    public sealed class FeNode
    {
        public string type, name; public Dictionary<string, object> props; public FeNode parent; public FeLayer layer;
        public readonly List<FeNode> kids = new List<FeNode>();
        public RectTransform rt; public FeImage image; public FeText text; public FeText keyText;
        public Vector2 pos, size, pivot; public float rotZ;
        public bool visible = true, focusedFlag, focusable = true, enabled = true, highlighted;
        public int focusIndex = -1;                 // lists / focus containers
        public Vector2 aPos, aScale = Vector2.one; public Color aCol = Color.white;
        public bool effFocus;
        public Color? overrideColor;                 // set by scripts (SetColor)
        public bool hideArt;                         // a KeyMap image showing the key name instead of a glyph
        public readonly Dictionary<string, FeAnim> anims = new Dictionary<string, FeAnim>();
        public Material maskMaterial;                // UIMaskContainer
        public string keyMap;                        // UIImageComponent KeyMap (controller / key glyph)
        public List<object> triggers;                // UIKeypressTrigger ... (the layer json)
        // list state (UIListContainer / UIDynamicList / UIScrollingDynamicListContainer)
        public int numItems = -1, absIndex; public Action<FeNode, int> onListChanged;   // (list, old absolute index)

        public bool IsList => type == "UIListContainer" || type == "UIDynamicList" || type == "UIScrollingDynamicListContainer";
        public bool IsDynamic => type == "UIDynamicList" || type == "UIScrollingDynamicListContainer";
        public string P(string k) => props.TryGetValue(k, out var v) ? v as string : null;
        public bool Has(string k) => props.ContainsKey(k);
        public FeNode Kid(string n) { foreach (var k in kids) if (k.name == n) return k; return null; }
        /// GetComponent_UIComponent(name): a descendant by name (depth first).
        public FeNode Find(string n) { foreach (var k in kids) { if (k.name == n) return k; var d = k.Find(n); if (d != null) return d; } return null; }
        /// A '/' path relative to this node ("." = itself, ".." = parent).
        public FeNode Path(string path)
        {
            FeNode n = this;
            foreach (var part in path.Split('/'))
            {
                if (part == "." || part.Length == 0) continue;
                if (part == "..") { n = n.parent ?? n; continue; }
                var k = n.Kid(part); if (k == null) return null; n = k;
            }
            return n;
        }
        public bool Visible { get => visible; set { visible = value; if (rt && rt.gameObject.activeSelf != value) rt.gameObject.SetActive(value); } }
        public int Index => parent == null ? 0 : parent.kids.IndexOf(this);
        public void SetText(string s) { if (text) text.Text = s; }

        static Color? Col(object v) => v is List<object> l && l.Count >= 3 ? OriginalUi.Col(l, Color.white) : (Color?)null;
        /// UIColorState: the first defined colour scanning from the current state down (Focused > Highlighted > Focusable > Enabled > Visible).
        public Color StateColor()
        {
            if (overrideColor.HasValue) return overrideColor.Value;
            return RawStateColor();
        }
        public Color RawStateColor()
        {
            Color? c = null;
            if (effFocus) c = Col(props.GetValueOrDefault("FocusedColor"));
            if (c == null && highlighted) c = Col(props.GetValueOrDefault("HighlightedColor"));
            if (c == null && focusable) c = Col(props.GetValueOrDefault("FocusableColor"));
            if (c == null && enabled) c = Col(props.GetValueOrDefault("EnabledColor"));
            return c ?? Col(props.GetValueOrDefault("VisibleColor")) ?? Color.white;
        }
    }

    /// One layer (UILayer tree) on the front-end canvas.
    public sealed class FeLayer
    {
        public readonly FeAssets assets; public FeNode root; public string assetName; public RectTransform holder;
        public bool active, focused;
        public FeScript script;
        readonly List<(FeAnim anim, FeNode scope, float t, Action done)> playing = new List<(FeAnim, FeNode, float, Action)>();
        readonly Material canvasMaterial;

        public FeLayer(FeAssets assets, string assetName, RectTransform parent)
        {
            this.assets = assets; this.assetName = assetName; canvasMaterial = assets.baseMaterial;
            var ld = assets.Layer(assetName) ?? throw new InvalidOperationException("Layer " + assetName + " not in MXUI");
            var hgo = new GameObject(assetName, typeof(RectTransform)); hgo.transform.SetParent(parent, false);
            holder = (RectTransform)hgo.transform; holder.anchorMin = holder.anchorMax = new Vector2(0, 1); holder.pivot = new Vector2(0, 1); holder.sizeDelta = new Vector2(1280, 720); holder.anchoredPosition = Vector2.zero;
            root = Build(ld.root, holder, null);
            hgo.SetActive(false);
        }

        public string Name => root.name;
        public FeNode Find(string name) => root.name == name ? root : root.Find(name);

        FeNode Build(Dictionary<string, object> j, RectTransform parent, FeNode parentNode)
        {
            var pr = (Dictionary<string, object>)j["props"];
            var n = new FeNode { type = OriginalUi.S(j, "type"), props = pr, parent = parentNode, layer = this };
            n.name = OriginalUi.S(pr, "Name") ?? n.type;
            n.pos = OriginalUi.V2(pr.GetValueOrDefault("Position") as List<object>, Vector2.zero);
            n.size = OriginalUi.V2(pr.GetValueOrDefault("Size") as List<object>, Vector2.zero);
            var pv = ReflexUiData.PivotOf(pr); n.pivot = new Vector2((float)(double)pv[0], (float)(double)pv[1]);
            n.rotZ = OriginalUi.F(pr, "RotationZ");
            n.focusedFlag = pr.GetValueOrDefault("Focused") is bool fb && fb;
            n.focusable = !(pr.GetValueOrDefault("Focusable") is bool fo) || fo;
            n.enabled = !(pr.GetValueOrDefault("Enabled") is bool en) || en;
            n.focusIndex = pr.GetValueOrDefault("FocusedComponentIndex") is double fi ? (int)fi : (n.IsList ? 0 : -1);
            n.keyMap = n.P("KeyMap");
            var go = new GameObject(n.name, typeof(RectTransform)); go.transform.SetParent(parent, false);
            n.rt = (RectTransform)go.transform; n.rt.anchorMin = n.rt.anchorMax = new Vector2(0, 1);
            n.rt.pivot = new Vector2(n.pivot.x, 1f - n.pivot.y); n.rt.sizeDelta = n.size;
            if (n.type == "UITextComponent" || n.type == "UIScrollingTextComponent")
            {
                var t = go.AddComponent<FeText>(); t.raycastTarget = false; t.material = canvasMaterial;
                var font = assets.Font(n.P("VisibleFont"));
                t.font = font; t.textHeight = OriginalUi.F(pr, "TextHeight", -1); if (t.textHeight <= 0) t.textHeight = font != null ? font.size : 32;
                t.lineSpacing = OriginalUi.F(pr, "LineSpacing"); t.wrap = pr.GetValueOrDefault("AllowWordWrap") is bool ww && ww;
                t.resize = !(pr.GetValueOrDefault("ResizeToText") is bool rz) || rz; t.align = n.pivot;
                t.onResize = s => n.size = s;
                n.text = t;
                string key = n.P("LocalizationKey");
                if (key != null && !key.StartsWith("DYN_", StringComparison.Ordinal) && !key.StartsWith("EMB_", StringComparison.Ordinal) && key != "STR_EMPTY") t.Text = assets.Str(key);
            }
            else if (n.type == "UIImageComponent" || n.type == "MXUIStreamableImageIcon")
            {
                var img = go.AddComponent<FeImage>(); img.raycastTarget = false; img.material = canvasMaterial;
                var (tex, uv) = assets.Material(n.P("VisibleMaterial"));
                img.texture = tex;
                bool mx = pr.GetValueOrDefault("MirrorX") is bool b1 && b1, my = pr.GetValueOrDefault("MirrorY") is bool b2 && b2;
                img.uvRect = Mirror(uv, mx, my);
                img.desaturation = OriginalUi.F(pr, "DesaturationLevel") / 255f;
                n.image = img;
                if (n.keyMap != null)
                {
                    var k = new GameObject("key", typeof(RectTransform)); k.transform.SetParent(go.transform, false);
                    var kt = (RectTransform)k.transform; kt.anchorMin = kt.anchorMax = kt.pivot = new Vector2(0.5f, 0.5f); kt.anchoredPosition = Vector2.zero;
                    var t = k.AddComponent<FeText>(); t.raycastTarget = false; t.material = canvasMaterial; t.font = assets.Font("ParalucentCondDemBol48"); t.textHeight = Mathf.Min(24, n.size.y * 0.7f);
                    n.keyText = t;
                }
            }
            else if (n.type == "UIMaskContainer")
            {
                n.maskMaterial = new Material(canvasMaterial) { name = "mask " + n.name, hideFlags = HideFlags.DontSave };
                n.maskMaterial.SetTexture("_MaskTex", n.P("MaskType") == "TextureMask" ? assets.Linear(n.P("MaskingTexture")) : Texture2D.whiteTexture);
                n.maskMaterial.SetFloat("_MaskOn", 1);
            }
            if (pr.GetValueOrDefault("Visible") is bool vis && !vis) n.Visible = false;
            if (n.type == "UIVideoComponent") FeVideo.Attach(n, go, canvasMaterial);   // its Bink video's render target (FrontEndVideo.cs)
            if (n.type == "UIIconGeometryComponent") n.Visible = n.Visible;   // drawn as nothing (icon geometry)
            foreach (var a in OriginalUi.Arr(j, "animations") ?? new List<object>()) if (a is Dictionary<string, object> ad) { var fa = FeAnim.Parse(ad); if (fa.name != null) n.anims[fa.name] = fa; }
            foreach (var c in OriginalUi.Arr(j, "children") ?? new List<object>()) if (c is Dictionary<string, object> cd) n.kids.Add(Build(cd, n.rt, n));
            if (n.maskMaterial != null) ApplyMask(n, n.maskMaterial);
            n.triggers = OriginalUi.Arr(j, "triggers");
            return n;
        }

        static void ApplyMask(FeNode n, Material m)
        {
            foreach (var k in n.kids) { if (k.image) k.image.material = m; if (k.text) k.text.material = m; if (k.keyText) k.keyText.material = m; if (k.maskMaterial == null) ApplyMask(k, m); }
        }

        public static Rect Mirror(Rect uv, bool mx, bool my) => new Rect(mx ? uv.xMax : uv.xMin, my ? uv.yMax : uv.yMin, mx ? -uv.width : uv.width, my ? -uv.height : uv.height);

        // -------------------------------------------------------------------------------- animation
        /// PlayAnimation(name) on a component (its own animations; the layer root's for layer animations).
        readonly List<(FeAnim anim, FeNode scope, float t, Action done)> tickSnapshot = new List<(FeAnim anim, FeNode scope, float t, Action done)>();
        public bool Play(string name, FeNode scope = null, Action done = null)
        {
            scope ??= root;
            if (!scope.anims.TryGetValue(name, out var a)) return false;
            Stop(name, scope);
            playing.Add((a, scope, 0f, done));
            Apply(a, scope, 0f, true);
            return true;
        }
        public void Stop(string name, FeNode scope = null) { scope ??= root; playing.RemoveAll(p => p.anim.name == name && p.scope == scope); }
        public bool IsPlaying(string name, FeNode scope = null) { scope ??= root; foreach (var p in playing) if (p.anim.name == name && p.scope == scope) return true; return false; }
        public float Length(string name, FeNode scope = null) { scope ??= root; return scope.anims.TryGetValue(name, out var a) ? a.length : 0; }

        void Apply(FeAnim a, FeNode scope, float t, bool start)
        {
            foreach (var tr in a.tracks)
            {
                FeNode n = tr.target == "." || string.IsNullOrEmpty(tr.target) ? scope : scope.Path(tr.target) ?? (scope == root ? null : root.Path(tr.target));
                if (n == null) continue;
                if (start) foreach (var r in tr.refs) Play(r, n);
                if (tr.keys.Count == 0) continue;
                var k0 = tr.keys[0]; var k1 = k0; float u = 0;
                for (int i = 0; i < tr.keys.Count; i++)
                {
                    if (tr.keys[i].t <= t) k0 = k1 = tr.keys[i];
                    else { k1 = tr.keys[i]; u = k1.t > k0.t ? (t - k0.t) / (k1.t - k0.t) : 0; break; }
                }
                bool Ch(string c, out float v)
                {
                    v = 0; if (!k0.ch.TryGetValue(c, out float a0)) return false;
                    v = k1.ch.TryGetValue(c, out float a1) ? Mathf.Lerp(a0, a1, u) : a0; return true;
                }
                if (Ch("position_x", out float px)) n.aPos.x = px;
                if (Ch("position_y", out float py)) n.aPos.y = py;
                if (Ch("scale_x", out float sx)) n.aScale.x = sx;
                if (Ch("scale_y", out float sy)) n.aScale.y = sy;
                if (Ch("color_r", out float cr)) n.aCol.r = cr;
                if (Ch("color_g", out float cg)) n.aCol.g = cg;
                if (Ch("color_b", out float cb)) n.aCol.b = cb;
                if (Ch("color_a", out float ca)) n.aCol.a = ca;
            }
        }

        public void Tick(float dt)
        {
            // The game's frames never exceed Engine::UpdateTiming's DeltaSecondsClamp (0.1 s); and only the animations playing when the
            // tick starts advance: one started from a completion (a looping background restarting itself) begins with the next tick.
            // (With an unbounded dt from a long first frame, a loop otherwise completed and restarted within one tick forever.)
            dt = Mathf.Min(dt, 0.1f);
            tickSnapshot.Clear(); tickSnapshot.AddRange(playing);
            foreach (var entry in tickSnapshot)
            {
                int i = playing.IndexOf(entry);
                if (i < 0) continue;                       // stopped by an earlier completion in this tick
                var (a, scope, t, done) = entry;
                t += dt;
                Apply(a, scope, Mathf.Min(t, a.length), false);
                if (t >= a.length)
                {
                    playing.RemoveAt(i); done?.Invoke(); script?.OnAnimationComplete(a.name, scope);
                    // UIAnimationTrigger (AnimationName -> ScriptFunction) of the layer
                    if (scope == root && root.triggers != null && script != null)
                        foreach (var tr in root.triggers)
                            if (OriginalUi.S(tr, "type") == "UIAnimationTrigger" && OriginalUi.S(tr, "AnimationName") == a.name && OriginalUi.S(tr, "ScriptFunction") is string fn) script.Call(fn);
                }
                else playing[i] = (a, scope, t, done);
            }
            Refresh(root, Color.white, focused && active, true);
        }

        /// Focus propagation, colours (state colour x own and ancestors' animated colour) and transforms.
        void Refresh(FeNode n, Color mul, bool focus, bool isRoot)
        {
            n.effFocus = focus;
            Color own = n.aCol, m = mul * own;
            Vector2 sz = n.size;
            n.rt.anchoredPosition = new Vector2(n.pos.x + n.aPos.x + n.pivot.x * sz.x * (n.aScale.x - 1f), -(n.pos.y + n.aPos.y + n.pivot.y * sz.y * (n.aScale.y - 1f)));
            n.rt.localScale = new Vector3(n.aScale.x, n.aScale.y, 1);
            n.rt.localRotation = n.rotZ != 0 ? Quaternion.Euler(0, 0, -n.rotZ) : Quaternion.identity;
            if (n.image || n.text)
            {
                var c = n.StateColor() * m;
                if (n.image) { var ic = n.hideArt ? new Color(c.r, c.g, c.b, 0) : c; if (n.image.color != ic) n.image.color = ic; }
                if (n.text && n.text.color != c) n.text.color = c;
            }
            if (n.keyText) { var kc = new Color(1, 1, 1, (n.RawStateColor() * m).a); if (n.keyText.color != kc) n.keyText.color = kc; }
            if (!n.visible) return;
            for (int i = 0; i < n.kids.Count; i++)
            {
                var k = n.kids[i];
                bool f = n.IsList ? focus && i == n.focusIndex : n.type == "UIFocusContainer" ? focus : focus && k.focusedFlag;
                Refresh(k, m, f, false);
            }
        }

        /// Keeps the mask matrices of UIMaskContainers current (canvas space -> the container's 0..1 box).
        public void UpdateMasks(Transform canvasRoot) => UpdateMasks(root, canvasRoot);
        static void UpdateMasks(FeNode n, Transform canvasRoot)
        {
            if (!n.visible) return;
            if (n.maskMaterial != null)
            {
                Rect r = n.rt.rect;
                var toLocal = n.rt.worldToLocalMatrix * canvasRoot.localToWorldMatrix;
                var norm = Matrix4x4.TRS(new Vector3(-r.xMin / r.width, -r.yMin / r.height, 0), Quaternion.identity, new Vector3(1f / r.width, 1f / r.height, 1));
                n.maskMaterial.SetMatrix("_MaskMatrix", norm * toLocal);
            }
            foreach (var k in n.kids) UpdateMasks(k, canvasRoot);
        }

        // -------------------------------------------------------------------------------- input
        /// UIKeypressTrigger: every focused component's trigger for this map key calls its script function (ScriptComponent
        /// "." / "../../.." resolves to the scripted layer). Returns true when any fired.
        public bool FireKey(string mapKey)
        {
            if (!active || !focused || script == null) return false;
            Refresh(root, Color.white, true, true);   // focus as it is now (a list may have moved this frame)
            var hits = new List<string>();
            Collect(root, mapKey, hits);
            foreach (var fn in hits) if (fn.Length > 0 && fn[0] == '\u0001') script.fe?.SendUIEvent(fn.Substring(1)); else script.Call(fn);   // \u0001: the trigger's Event
            return hits.Count > 0;
        }
        void Collect(FeNode n, string mapKey, List<string> hits)
        {
            if (!n.visible || !(n.effFocus || n == root)) return;
            if (n.triggers != null)
                foreach (var t in n.triggers)
                    if (OriginalUi.S(t, "type") == "UIKeypressTrigger" && OriginalUi.S(t, "MapKey") == mapKey)
                    {
                        if (OriginalUi.S(t, "ScriptFunction") is string fn) hits.Add(fn);
                        else if (OriginalUi.S(t, "EventName") is string ev) hits.Add("\u0001" + ev);   // <Event name="..."/>: a UI event (Attract, Credits, the logo videos)
                    }
            foreach (var k in n.kids) Collect(k, mapKey, hits);
        }

        /// List navigation by the deepest focused list that navigates in that direction (NavigationType LeftRight, else UpDown).
        public bool Navigate(int dx, int dy, Action<string> sound)
        {
            if (!active || !focused) return false;
            Refresh(root, Color.white, true, true);
            var chain = new List<FeNode>();
            Lists(root, chain);
            for (int i = chain.Count - 1; i >= 0; i--)
            {
                var l = chain[i];
                bool lr = l.P("NavigationType") == "LeftRight";
                int d = lr ? dx : dy;
                if (d == 0) continue;
                MoveList(l, d, sound);
                return true;
            }
            return false;
        }
        static void Lists(FeNode n, List<FeNode> o)
        {
            if (!n.visible || !n.effFocus) return;
            if (n.IsList) o.Add(n);
            foreach (var k in n.kids) Lists(k, o);
        }

        public static void MoveList(FeNode l, int d, Action<string> sound)
        {
            int old = l.IsDynamic ? l.absIndex : l.focusIndex, count = l.IsDynamic ? Mathf.Max(0, l.numItems) : l.kids.Count;
            bool wrap = !l.IsDynamic && !(l.props.GetValueOrDefault("AllowWrap") is bool w && !w);
            int next = old;
            for (int step = 0; step < count; step++)
            {
                next += d;
                if (next < 0 || next >= count) { if (!wrap) { next = old; break; } next = (next + count) % count; }
                if (l.IsDynamic || (l.kids[next].focusable && l.kids[next].visible)) break;
            }
            if (next == old || count == 0) { sound?.Invoke(l.P("NoScrollSoundEvent")); return; }
            if (l.IsDynamic) l.absIndex = next; else l.focusIndex = next;
            sound?.Invoke(l.P("ScrollSoundEvent"));
            l.onListChanged?.Invoke(l, old);
        }
    }

    /// The C# port of a layer's Lua script (Initialize / Start / End / handlers named like the Lua functions).
    public abstract class FeScript
    {
        public FeLayer layer; public FrontEndManager fe;
        protected readonly Dictionary<string, Action> functions = new Dictionary<string, Action>();
        public void Call(string fn) { if (functions.TryGetValue(fn, out var f)) f(); }
        public virtual void Initialize() { }
        public virtual void Start() { }
        public virtual void End() { }
        public virtual void Update(float dt) { }
        public virtual void OnAnimationComplete(string anim, FeNode scope) { }
        public virtual void Present() { }
        public virtual void OnNavigated() { }
        protected FeNode C(string name) => layer.Find(name);
        protected string Str(string key) => fe.assets.Str(key);
    }
}
