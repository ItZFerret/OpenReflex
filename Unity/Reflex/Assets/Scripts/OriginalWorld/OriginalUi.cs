using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The original game's own UI system rebuilt in Unity from the layouts read at run time from the player's install
    // (ReflexUiData = the run-time port of _re/formats/ui_export.py; layout rules in _re/notes/ui.md): components in the game's 1280x720 reference space (Position = where the pivot of the Size
    // box goes; children relative to the parent's top-left; text resized to its string), the original bitmap fonts
    // (cell glyph metrics + atlas, glyph scale TextHeight / font size, the string-width rule of FUN_007FAF10),
    // the original art, colours, and the baked 30 fps animations (position offsets, scale, colour multipliers).
    public static class OriginalUi
    {
        // ------------------------------------------------------------------------------------ JSON (minimal)
        public static object ParseJson(string s) { int i = 0; return Value(s, ref i); }
        static void Ws(string s, ref int i) { while (i < s.Length && char.IsWhiteSpace(s[i])) i++; }
        static object Value(string s, ref int i)
        {
            Ws(s, ref i);
            char c = s[i];
            if (c == '{')
            {
                var d = new Dictionary<string, object>(); i++;
                while (true)
                {
                    Ws(s, ref i); if (s[i] == '}') { i++; return d; }
                    string k = Str(s, ref i); Ws(s, ref i); i++; // ':'
                    d[k] = Value(s, ref i); Ws(s, ref i);
                    if (s[i] == ',') { i++; continue; }
                    i++; return d;
                }
            }
            if (c == '[')
            {
                var l = new List<object>(); i++;
                while (true)
                {
                    Ws(s, ref i); if (s[i] == ']') { i++; return l; }
                    l.Add(Value(s, ref i)); Ws(s, ref i);
                    if (s[i] == ',') { i++; continue; }
                    i++; return l;
                }
            }
            if (c == '"') return Str(s, ref i);
            if (s.Length - i >= 4 && string.CompareOrdinal(s, i, "true", 0, 4) == 0) { i += 4; return true; }
            if (s.Length - i >= 5 && string.CompareOrdinal(s, i, "false", 0, 5) == 0) { i += 5; return false; }
            if (s.Length - i >= 4 && string.CompareOrdinal(s, i, "null", 0, 4) == 0) { i += 4; return null; }
            int st = i; while (i < s.Length && "+-0123456789.eE".IndexOf(s[i]) >= 0) i++;
            return double.Parse(s.Substring(st, i - st), CultureInfo.InvariantCulture);
        }
        static string Str(string s, ref int i)
        {
            var sb = new StringBuilder(); i++;
            while (s[i] != '"')
            {
                if (s[i] == '\\')
                {
                    i++;
                    switch (s[i])
                    {
                        case 'n': sb.Append('\n'); break; case 't': sb.Append('\t'); break; case 'r': sb.Append('\r'); break;
                        case 'u': sb.Append((char)Convert.ToInt32(s.Substring(i + 1, 4), 16)); i += 4; break;
                        default: sb.Append(s[i]); break;
                    }
                    i++;
                }
                else sb.Append(s[i++]);
            }
            i++; return sb.ToString();
        }
        public static Dictionary<string, object> Obj(object o, string k) => o is Dictionary<string, object> d && d.TryGetValue(k, out var v) ? v as Dictionary<string, object> : null;
        public static List<object> Arr(object o, string k) => o is Dictionary<string, object> d && d.TryGetValue(k, out var v) ? v as List<object> : null;
        public static string S(object o, string k) => o is Dictionary<string, object> d && d.TryGetValue(k, out var v) ? v as string : null;
        public static float F(object o, string k, float def = 0) => o is Dictionary<string, object> d && d.TryGetValue(k, out var v) && v is double x ? (float)x : def;
        public static Vector2 V2(List<object> l, Vector2 def) => l != null && l.Count >= 2 && l[0] is double x && l[1] is double y ? new Vector2((float)x, (float)y) : def;
        public static Color Col(List<object> l, Color def)
        {
            if (l == null || l.Count < 3) return def;
            float a = l.Count > 3 ? (float)(double)l[3] : 255f;
            return new Color((float)(double)l[0] / 255f, (float)(double)l[1] / 255f, (float)(double)l[2] / 255f, a / 255f);
        }

        /// MM:SS.cc as the game's timer components print it (a leading '-' if negative; 0 and FLT_MAX -> --:--.--).
        public static string Time(float t)
        {
            if (t == 0f || t >= 3.0e38f || float.IsNaN(t) || float.IsInfinity(t)) return "--:--.--";
            string sign = t < 0 ? "-" : ""; t = Mathf.Abs(t);
            int cs = Mathf.FloorToInt(t * 100f + 1e-4f), m = cs / 6000, sec = cs / 100 % 60, c = cs % 100;
            return string.Format(CultureInfo.InvariantCulture, "{0}{1:00}:{2:00}.{3:00}", sign, m, sec, c);
        }
    }

    // The run-time store of the UI's textures and fonts (read from the install, cached).
    public static class OriginalUiAssets
    {
        static readonly Dictionary<string, OriginalFont> fonts = new Dictionary<string, OriginalFont>();
        public static Reflex.Port.Content.ReflexUiData Data => Reflex.Port.Content.ReflexUiData.Shared;
        public static Texture2D Texture(string key) => key == null ? null : Data.Texture(key);
        public static OriginalFont Font(string key)
        {
            if (key == null) return null;
            if (!fonts.TryGetValue(key, out var f)) fonts[key] = f = OriginalFont.Create(Data.FontData(key));
            return f;
        }
        public static void Reset() { fonts.Clear(); Reflex.Port.Content.ReflexUiData.ResetShared(); }
    }

    // An original bitmap font: the cell asset's glyph metrics (code -> uv rect, w, left, right, h, bottom) and atlas.
    public sealed class OriginalFont
    {
        public struct Glyph { public Rect uv; public float w, left, right, h, bottom; }
        public readonly Dictionary<int, Glyph> glyphs = new Dictionary<int, Glyph>();
        public float size, letterSpacing;
        public Texture2D atlas;

        /// A font read from the install at run time (ReflexUiData.FontData: the cell asset + its atlas).
        public static OriginalFont Create(Reflex.Port.Content.ReflexUiData.FontResource r)
        {
            if (r == null) return null;
            var f = new OriginalFont { size = r.cell.size, letterSpacing = r.cell.letterSpacing, atlas = r.atlas };
            foreach (var kv in r.cell.glyphs)
            {
                var g = kv.Value;
                // D3D v grows downwards; Unity's texture v upwards.
                f.glyphs[kv.Key] = new Glyph { uv = Rect.MinMaxRect(g.u0, 1f - g.v1, g.u1, 1f - g.v0), w = g.w, left = g.left, right = g.right, h = g.h, bottom = g.bottom };
            }
            return f;
        }

        /// From the exporter's fonts/*.json (reference for the run-time reader; verification only).
        public static OriginalFont Load(string metricsJson, Texture2D atlas)
        {
            var m = OriginalUi.ParseJson(metricsJson);
            var f = new OriginalFont { size = OriginalUi.F(m, "size", 48), letterSpacing = OriginalUi.F(m, "letterSpacing"), atlas = atlas };
            foreach (var kv in OriginalUi.Obj(m, "glyphs"))
            {
                var g = kv.Value;
                float u0 = OriginalUi.F(g, "u0"), v0 = OriginalUi.F(g, "v0"), u1 = OriginalUi.F(g, "u1"), v1 = OriginalUi.F(g, "v1");
                // D3D v grows downwards; Unity's texture v upwards.
                f.glyphs[int.Parse(kv.Key, CultureInfo.InvariantCulture)] = new Glyph
                {
                    uv = Rect.MinMaxRect(u0, 1f - v1, u1, 1f - v0),
                    w = OriginalUi.F(g, "w"), left = OriginalUi.F(g, "left"), right = OriginalUi.F(g, "right"),
                    h = OriginalUi.F(g, "h"), bottom = OriginalUi.F(g, "bottom"),
                };
            }
            return f;
        }

        bool Get(char c, out Glyph g) => glyphs.TryGetValue(c, out g) || glyphs.TryGetValue('?', out g);

        /// String width at a text height (FUN_007FAF10); the widest line of a multi-line string.
        public float Width(string text, float textHeight)
        {
            float best = 0;
            foreach (var line in (text ?? "").Split('\n')) best = Mathf.Max(best, LineWidth(line, textHeight));
            return best;
        }

        float LineWidth(string line, float textHeight)
        {
            float s = textHeight / size, w = 0; bool first = true;
            foreach (char c in line)
            {
                if (!Get(c, out var g)) continue;
                if (!first) w += letterSpacing + g.left;
                w += g.w + g.right; first = false;
            }
            return w * s;
        }

        /// Breaks text into lines no wider than maxWidth (words kept whole).
        public string Wrap(string text, float textHeight, float maxWidth)
        {
            var sb = new StringBuilder(); string line = "";
            foreach (var word in (text ?? "").Split(' '))
            {
                if (word.Length == 0) { if (line.Length > 0) line += " "; continue; }
                string cand = line.Length == 0 ? word : line.TrimEnd() + " " + word;
                if (line.Length > 0 && LineWidth(cand, textHeight) > maxWidth) { sb.Append(line.TrimEnd()).Append('\n'); line = word; }
                else line = cand;
            }
            return sb.Append(line).ToString();
        }

        /// Glyph quads from the top-left of the text box (y down), in reference pixels; lines TextHeight apart,
        /// each line centred in the box when `center`.
        public void Layout(string text, float textHeight, List<(Rect rect, Rect uv)> quads, bool center = false)
        {
            quads.Clear();
            float s = textHeight / size, boxW = Width(text, textHeight), y = 0;
            foreach (var line in (text ?? "").Split('\n'))
            {
                float x = center ? (boxW - LineWidth(line, textHeight)) * 0.5f : 0; bool first = true;
                foreach (char c in line)
                {
                    if (!Get(c, out var g)) continue;
                    if (!first) x += (letterSpacing + g.left) * s;
                    if (g.w > 0 && g.h > 0) quads.Add((new Rect(x, y + (size - g.bottom - g.h) * s, g.w * s, g.h * s), g.uv));
                    x += (g.w + g.right) * s; first = false;
                }
                y += textHeight;
            }
        }
    }

    // A text component drawn with an original bitmap font (TextIcon: the atlas texel modulated by the colour).
    [RequireComponent(typeof(CanvasRenderer))]
    public sealed class OriginalText : MaskableGraphic
    {
        public OriginalFont font;
        public float textHeight = 48;
        public bool centerLines;   // multi-line text: each line centred (horizontal justification centre)
        string text = "";
        readonly List<(Rect rect, Rect uv)> quads = new List<(Rect, Rect)>();

        public override Texture mainTexture => font != null && font.atlas ? font.atlas : s_WhiteTexture;

        public string Text
        {
            get => text;
            set
            {
                value ??= "";
                if (value == text) return;
                text = value; Resize(); SetVerticesDirty();
            }
        }

        /// ResizeToText: the box becomes the string's width x TextHeight, the pivot point staying in place.
        public void Resize()
        {
            if (font == null) return;
            int lines = 1; foreach (char c in text) if (c == '\n') lines++;
            rectTransform.sizeDelta = new Vector2(font.Width(text, textHeight), textHeight * lines);
        }

        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            if (font == null || string.IsNullOrEmpty(text)) return;
            font.Layout(text, textHeight, quads, centerLines);
            Rect r = rectTransform.rect;   // local, y up; the text box's top-left is (r.xMin, r.yMax)
            foreach (var (q, uv) in quads)
            {
                float x0 = r.xMin + q.x, x1 = x0 + q.width, y1 = r.yMax - q.y, y0 = y1 - q.height;
                int i = vh.currentVertCount;
                vh.AddVert(new Vector3(x0, y0), color, new Vector2(uv.xMin, uv.yMin));
                vh.AddVert(new Vector3(x0, y1), color, new Vector2(uv.xMin, uv.yMax));
                vh.AddVert(new Vector3(x1, y1), color, new Vector2(uv.xMax, uv.yMax));
                vh.AddVert(new Vector3(x1, y0), color, new Vector2(uv.xMax, uv.yMin));
                vh.AddTriangle(i, i + 1, i + 2); vh.AddTriangle(i + 2, i + 3, i);
            }
        }
    }

    // A built UI tree: nodes by path and name, their base layout/colour, and the animation player.
    public sealed class OriginalUiTree
    {
        public sealed class Node
        {
            public string path, name, cls; public RectTransform rect; public Graphic graphic; public OriginalText text;
            public Vector2 basePos; public Vector3 baseScale = Vector3.one; public Color baseColor = Color.white;
            public readonly List<Node> children = new List<Node>(); public Node parent;
            public Dictionary<string, object> json;
            /// A container (UIColorContainer) whose animated colour tints its descendants' graphics (the game's container colour is inherited by its children).
            public bool tintChildren;
            public readonly Dictionary<string, Dictionary<string, object>> anims = new Dictionary<string, Dictionary<string, object>>();
        }
        public readonly Dictionary<string, Node> byPath = new Dictionary<string, Node>();
        public readonly List<Node> roots = new List<Node>();
        readonly Func<string, Texture2D> textures; readonly Func<string, OriginalFont> fonts;
        readonly Dictionary<string, Dictionary<string, object>> anims = new Dictionary<string, Dictionary<string, object>>();
        readonly List<(Dictionary<string, object> anim, Node scope, float t, bool hold)> playing = new List<(Dictionary<string, object>, Node, float, bool)>();

        public OriginalUiTree(Func<string, Texture2D> textures, Func<string, OriginalFont> fonts) { this.textures = textures; this.fonts = fonts; }

        public Node Find(string pathOrName) { if (byPath.TryGetValue(pathOrName, out var n)) return n; foreach (var x in byPath.Values) if (x.name == pathOrName) return x; return null; }
        public Node Child(Node n, string name) { if (n == null) return null; foreach (var c in n.children) if (c.name == name) return c; return null; }

        public Node Build(Dictionary<string, object> e, RectTransform parent, Node parentNode)
        {
            var lay = OriginalUi.Obj(e, "layout"); var st = OriginalUi.Obj(e, "style") ?? new Dictionary<string, object>();
            var go = new GameObject(OriginalUi.S(e, "name") ?? "node", typeof(RectTransform));
            go.transform.SetParent(parent, false);
            var rt = (RectTransform)go.transform;
            Vector2 pos = OriginalUi.V2(OriginalUi.Arr(lay, "position"), Vector2.zero);
            Vector2 size = OriginalUi.V2(OriginalUi.Arr(lay, "size"), Vector2.zero);
            Vector2 pivot = OriginalUi.V2(OriginalUi.Arr(lay, "pivot"), Vector2.zero);
            rt.anchorMin = rt.anchorMax = new Vector2(0, 1);                  // relative to the parent's top-left
            rt.pivot = new Vector2(pivot.x, 1f - pivot.y);
            rt.anchoredPosition = new Vector2(pos.x, -pos.y);
            rt.sizeDelta = size;
            float rot = OriginalUi.F(lay, "rotationZ_deg");
            if (rot != 0) rt.localRotation = Quaternion.Euler(0, 0, -rot);   // the game's y-down screen space
            var n = new Node { path = OriginalUi.S(e, "path"), name = OriginalUi.S(e, "name"), cls = OriginalUi.S(e, "class"), rect = rt, basePos = rt.anchoredPosition, json = e, parent = parentNode };
            Color col = OriginalUi.Col(OriginalUi.Arr(st, "color"), Color.white);
            var font = OriginalUi.Obj(st, "font");
            string tex = OriginalUi.S(st, "texture");
            if (font != null)
            {
                var t = go.AddComponent<OriginalText>();
                t.font = fonts(OriginalUi.S(font, "metrics"));
                t.textHeight = OriginalUi.F(st, "textHeight", OriginalUi.F(font, "size", 48));
                if (t.textHeight <= 0) t.textHeight = OriginalUi.F(font, "size", 48);   // TextHeight -1 = unset: the font's own size
                t.raycastTarget = false; t.color = col; t.centerLines = pivot.x == 0.5f;
                string key = OriginalUi.S(st, "locKey"), loc = OriginalUi.S(st, "locText");
                t.Text = loc != null && (key == null || !key.StartsWith("DYN_", StringComparison.Ordinal)) ? loc : "";
                n.text = t; n.graphic = t;
            }
            else if (tex != null)
            {
                var img = go.AddComponent<RawImage>();
                img.texture = textures(tex); img.color = col; img.raycastTarget = false;
                if (size == Vector2.zero) rt.sizeDelta = OriginalUi.V2(OriginalUi.Arr(st, "textureSize"), new Vector2(64, 64));
                bool mx = st.TryGetValue("mirrorX", out var a) && a is bool b1 && b1, my = st.TryGetValue("mirrorY", out var c) && c is bool b2 && b2;
                img.uvRect = new Rect(mx ? 1 : 0, my ? 1 : 0, mx ? -1 : 1, my ? -1 : 1);
                if (OriginalUi.Arr(st, "atlasUv") is List<object> au && au.Count == 4)   // a texatlas sprite (ReflexUiData.BuildLayerOwn)
                {
                    var r = new Rect(Convert.ToSingle(au[0]), Convert.ToSingle(au[1]), Convert.ToSingle(au[2]), Convert.ToSingle(au[3]));
                    img.uvRect = new Rect(mx ? r.xMax : r.x, my ? r.yMax : r.y, mx ? -r.width : r.width, my ? -r.height : r.height);
                }
                n.graphic = img;
            }
            n.baseColor = col;
            if (n.path != null) byPath[n.path] = n;
            var kids = OriginalUi.Arr(e, "children");
            if (kids != null) foreach (var k in kids) if (k is Dictionary<string, object> kd) n.children.Add(Build(kd, rt, n));
            if (parentNode == null) roots.Add(n);
            return n;
        }

        public void AddAnimations(List<object> list)
        {
            if (list == null) return;
            foreach (var a in list) if (a is Dictionary<string, object> d && OriginalUi.S(d, "name") is string nm) anims[nm] = d;
        }

        /// The length in seconds of a layer animation (0 when the layer has none of that name).
        public float Length(string name) => anims.TryGetValue(name, out var a) ? OriginalUi.F(a, "length", 0f) : 0f;

        public bool HasAnimation(string name, Node scope = null) => scope != null && scope.anims.ContainsKey(name) || anims.ContainsKey(name);

        /// Registers the components' own animations from the full layer tree (.lyr.json: root/children... with
        /// "name" and "animations"), matched to the built nodes by name.
        public void AddComponentAnimations(object layerNode)
        {
            if (!(layerNode is Dictionary<string, object> d)) return;
            if (OriginalUi.S(d, "name") is string nm && OriginalUi.Arr(d, "animations") is List<object> list)
            {
                var n = Find(nm);
                if (n != null) foreach (var a in list) if (a is Dictionary<string, object> ad && OriginalUi.S(ad, "name") is string an && OriginalUi.Arr(ad, "tracks") != null) n.anims[an] = ad;
            }
            if (OriginalUi.Arr(d, "children") is List<object> kids) foreach (var k in kids) AddComponentAnimations(k);
            if (OriginalUi.Obj(d, "root") is Dictionary<string, object> r) AddComponentAnimations(r);
        }

        /// Plays a baked animation (the scope component's own first, else a layer animation); track targets are
        /// absolute paths, or names relative to `scope` ("." = scope).
        public void Play(string name, Node scope = null, bool holdLastFrame = true)
        {
            Dictionary<string, object> a = null;
            if (scope != null) scope.anims.TryGetValue(name, out a);
            if (a == null && !anims.TryGetValue(name, out a)) return;
            playing.RemoveAll(p => p.anim == a && p.scope == scope);
            playing.Add((a, scope, 0f, holdLastFrame));
        }

        /// Whether the animation (the scope component's own first, else a layer animation) is playing now (UICompositeAnimation state).
        public bool IsPlaying(string name, Node scope = null)
        {
            Dictionary<string, object> a = null;
            if (scope != null) scope.anims.TryGetValue(name, out a);
            if (a == null && !anims.TryGetValue(name, out a)) return false;
            return playing.Exists(p => p.anim == a && p.scope == scope);
        }

        /// Runs an animation to its end at once (the game's CompositeAnimation stopped with state 3 runs to completion, FUN_005c5ea0):
        /// its last frame stays applied.
        public void Finish(string name, Node scope = null)
        {
            Dictionary<string, object> a = null;
            if (scope != null) scope.anims.TryGetValue(name, out a);
            if (a == null && !anims.TryGetValue(name, out a)) return;
            playing.RemoveAll(p => p.anim == a && p.scope == scope);
            Apply(a, scope, OriginalUi.F(a, "length", 0f));
        }

        public void Tick(float dt)
        {
            for (int i = playing.Count - 1; i >= 0; i--)
            {
                var (a, scope, t, hold) = playing[i];
                t += dt;
                float len = OriginalUi.F(a, "length", 0f);
                bool done = t >= len;
                Apply(a, scope, Mathf.Min(t, len));
                if (done) { playing.RemoveAt(i); if (!hold) Apply(a, scope, -1f); }
                else playing[i] = (a, scope, t, hold);
            }
        }

        void Apply(Dictionary<string, object> a, Node scope, float t)
        {
            var tracks = OriginalUi.Arr(a, "tracks");
            if (tracks == null) return;
            foreach (var tr in tracks)
            {
                string target = OriginalUi.S(tr, "target");
                Node n = target == "." ? scope : scope != null ? (FindIn(scope, target) ?? Find(target)) : Find(target);
                if (n == null) continue;
                if (t < 0f) { Reset(n); continue; }
                var keys = OriginalUi.Arr(tr, "keys");
                if (keys == null || keys.Count == 0) continue;
                Dictionary<string, object> k0 = keys[0] as Dictionary<string, object>, k1 = k0; float u = 0;
                for (int i = 0; i < keys.Count; i++)
                {
                    var k = keys[i] as Dictionary<string, object>;
                    if (OriginalUi.F(k, "t") <= t) k0 = k1 = k;
                    else { k1 = k; float t0 = OriginalUi.F(k0, "t"), t1 = OriginalUi.F(k1, "t"); u = t1 > t0 ? (t - t0) / (t1 - t0) : 0f; break; }
                }
                var c0 = OriginalUi.Obj(k0, "ch"); var c1 = OriginalUi.Obj(k1, "ch");
                float Ch(string ch, float def) => Mathf.Lerp(OriginalUi.F(c0, ch, def), OriginalUi.F(c1, ch, def), u);
                n.rect.anchoredPosition = n.basePos + new Vector2(Ch("position_x", 0), -Ch("position_y", 0));
                if (OriginalUi.Obj(k0, "ch") is Dictionary<string, object> rc && rc.ContainsKey("rotation_x"))
                {   // a 2x2 key [scale_x rotation_x; rotation_y scale_y] (row vectors): the turn atan2(rotation_y, scale_x) and the lengths (WP-5:
                    // EventIntro's ScenarioHeaderIn turns the R logo 15 degrees and the star bursts -45)
                    float sx = Ch("scale_x", 1), sy = Ch("scale_y", 1), rx = Ch("rotation_x", 0), ry = Ch("rotation_y", 0);
                    float sxl = Mathf.Sign(sx == 0 ? 1 : sx) * Mathf.Sqrt(sx * sx + ry * ry), syl = Mathf.Sign(sy == 0 ? 1 : sy) * Mathf.Sqrt(sy * sy + rx * rx);
                    float deg = Mathf.Atan2(ry, Mathf.Abs(sx) < 1e-6f && Mathf.Abs(ry) < 1e-6f ? 1 : sx) * Mathf.Rad2Deg;
                    n.rect.localScale = new Vector3(n.baseScale.x * sxl, n.baseScale.y * syl, 1);
                    n.rect.localRotation = Quaternion.Euler(0, 0, -OriginalUi.F(OriginalUi.Obj(n.json, "layout"), "rotationZ_deg") + deg);
                }
                else n.rect.localScale = new Vector3(n.baseScale.x * Ch("scale_x", 1), n.baseScale.y * Ch("scale_y", 1), 1);   // baseScale: the component's own (OriginalLiveUi)
                if (n.graphic) n.graphic.color = new Color(n.baseColor.r * Ch("color_r", 1), n.baseColor.g * Ch("color_g", 1), n.baseColor.b * Ch("color_b", 1), n.baseColor.a * Ch("color_a", 1));
                else if (n.tintChildren) Tint(n, new Color(Ch("color_r", 1), Ch("color_g", 1), Ch("color_b", 1), Ch("color_a", 1)));
            }
        }

        // the container's colour over its descendants (each keeps its own base colour)
        static void Tint(Node n, Color tint)
        {
            foreach (var c in n.children)
            {
                if (c.graphic) c.graphic.color = new Color(c.baseColor.r * tint.r, c.baseColor.g * tint.g, c.baseColor.b * tint.b, c.baseColor.a * tint.a);
                Tint(c, tint);
            }
        }

        /// A container's colour over its descendants at once (a UIColorContainer's colour set by code: the reflex arrows' "WreckRecoveryFailed" handler).
        public static void SetTint(Node n, Color tint) { if (n != null) Tint(n, tint); }

        static Node FindIn(Node scope, string name)
        {
            foreach (var c in scope.children) { if (c.name == name) return c; var d = FindIn(c, name); if (d != null) return d; }
            return null;
        }

        static void Reset(Node n) { n.rect.anchoredPosition = n.basePos; n.rect.localScale = n.baseScale; if (n.graphic) n.graphic.color = n.baseColor; else if (n.tintChildren) Tint(n, Color.white); }

        /// UIColorState (0x5EAFF0): the first colour the component defines scanning Focused > Focusable > Visible (Highlighted /
        /// Enabled colours do not occur in the layers used so far), as a node's described style gives them.
        public static Color StateColor(Node n, bool focused, bool focusable)
        {
            var st = OriginalUi.Obj(n.json, "style");
            if (focused && OriginalUi.Arr(st, "FocusedColor") is List<object> f) return OriginalUi.Col(f, Color.white);
            if (focusable && OriginalUi.Arr(st, "FocusableColor") is List<object> a) return OriginalUi.Col(a, Color.white);
            return OriginalUi.Col(OriginalUi.Arr(st, "color"), Color.white);
        }

        /// Sets a node's colour (the base the animations multiply).
        public static void SetColor(Node n, Color c) { if (n == null || !n.graphic) return; n.baseColor = c; n.graphic.color = c; }
        public static void SetText(Node n, string s) { if (n != null && n.text) n.text.Text = s; }
        public static void SetVisible(Node n, bool v) { if (n != null && n.rect.gameObject.activeSelf != v) n.rect.gameObject.SetActive(v); }
    }
}
