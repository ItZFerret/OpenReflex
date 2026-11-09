// The original game's UI data read at run time from the player's own install (Database/MXUI.dx9.{database,package}).
// Port of _re/formats/ui_export.py + uitex.py (the Python outputs are the reference this is verified against; nothing here
// is shipped with the project). The UI is the game's own retained-mode system (see _re/notes/ui.md): UIComponent trees
// stored as BXML "uicmpnt" assets (+ a dependency trailer), bitmap fonts ("cell" + a same-named "texture"), "texture"
// assets (A8R8G8B8 / DXT1/3/5, not always power of two) and "localiz" string tables, grouped in packages
// (Base, BaseLocalized.<LANG>, InGame.<LANG>, Shared.<LANG>, Hud<Mode>SinglePlayer.<LANG> ...).
//
// The in-memory model is the JSON shape the exporter wrote (Dictionary<string, object> / List<object> / string / double /
// bool / null) so OriginalUiTree consumes it unchanged:
//   Layer(...)       the parsed UIComponent tree  { type, props, children, animations, triggers, edges, [style], name }
//   Describe(...)    the renderer's view of a node { path, name, class, layout, style{ color, font, texture, locText ... }, children }
//   Texture(key)     key "ui/textures/<package>/<name>.png" -> Texture2D (mip 0, RGBA32, clamp, bilinear, as the PNG import was)
//   FontData(key)    key "ui/fonts/<package>.<name>.json"   -> glyph metrics (cell) + atlas texture
// Keys are opaque ids that keep the exporter's spelling so Describe() output compares 1:1 with hud.json / pause.json.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.Content
{
    public sealed class ReflexUiData
    {
        public const string DatabaseName = "MXUI";

        /// One bitmap font ("cell" asset): header f32 3.1, u16, u16 letterSpacing, f32 native pixel size, u32 count; 36-byte glyph records.
        public sealed class Cell
        {
            public struct Glyph { public float u0, v0, u1, v1; public int w, left, right, h, bottom; }
            public float size, letterSpacing;
            public readonly Dictionary<int, Glyph> glyphs = new Dictionary<int, Glyph>();
        }
        public sealed class FontResource { public string key, name, package, texturePackage; public Cell cell; public Texture2D atlas; }
        public sealed class LayerData { public string package, name; public Dictionary<string, object> root; public List<object> deps; }

        // ------------------------------------------------------------------------------------------- language / packages

        static ReflexUiData _shared;
        /// The UI of the install in the game's language (UIRegistry.xml ForceLanguage, else ENG).
        public static ReflexUiData Shared => _shared ??= new ReflexUiData(DetectLanguage());
        public static void ResetShared() { _shared = null; }

        public readonly string language;
        readonly Rfx.Install install;
        readonly Dictionary<string, byte[]> assets = new Dictionary<string, byte[]>();
        readonly Dictionary<string, LayerData> layers = new Dictionary<string, LayerData>();
        readonly Dictionary<string, Cell> cells = new Dictionary<string, Cell>();
        readonly Dictionary<string, Texture2D> textures = new Dictionary<string, Texture2D>();
        readonly Dictionary<string, FontResource> fonts = new Dictionary<string, FontResource>();
        readonly Dictionary<string, Dictionary<string, string>> strings = new Dictionary<string, Dictionary<string, string>>();

        public ReflexUiData(string language = "ENG")
        {
            this.language = language;
            install = ReflexInstall.Packages;
        }

        /// UIRegistry.xml: "<ForceLanguage>FRE</ForceLanguage>" (commented out in the shipped file) forces a language.
        /// The text languages the install carries and the shell's type can draw (Latin scripts), for OPTIONS > INTERFACE > LANGUAGE.
        public static readonly string[] SelectableLanguages = { "ENG", "GER", "FRE", "SPA", "ITA" };
        public static string[] AvailableLanguages()
        {
            var list = new List<string>();
            try
            {
                var db = ReflexInstall.Packages.Db(DatabaseName);
                foreach (var l in SelectableLanguages) if (db != null && db.Find("InGame." + l) != null) list.Add(l);
            }
            catch (Exception) { }
            if (list.Count == 0) list.Add("ENG");
            return list.ToArray();
        }

        static string DetectLanguage()
        {
            try
            {
                string chosen = Reflex.Port.Options.ReflexSettings.Language;   // OPTIONS > INTERFACE > LANGUAGE
                if (!string.IsNullOrEmpty(chosen) && Array.IndexOf(AvailableLanguages(), chosen) >= 0) return chosen;
            }
            catch (Exception) { }
            try
            {
                string xml = File.ReadAllText(ReflexInstall.PathOf("UIRegistry.xml"));
                xml = Regex.Replace(xml, "<!--.*?-->", "", RegexOptions.Singleline);
                var m = Regex.Match(xml, @"<ForceLanguage>\s*([A-Z]{3})\s*</ForceLanguage>");
                if (m.Success)
                {
                    string lang = m.Groups[1].Value;
                    var db = ReflexInstall.Packages.Db(DatabaseName);
                    if (db != null && db.Find("InGame." + lang) != null) return lang;
                }
            }
            catch (Exception) { }
            return "ENG";
        }

        /// A package name from its language-less base ("Base" has no language).
        public string Pkg(string baseName) => baseName == "Base" || baseName.IndexOf('.') >= 0 ? baseName : baseName + "." + language;

        // The packages the HUD / pause menu use, in the exporter's PACKAGES order.
        string[] FontSearch => First(new[] { Pkg("HudNationalSinglePlayer"), Pkg("InGame"), Pkg("BaseLocalized"), Pkg("Shared") });
        string[] MaterialSearch => First(new[] { Pkg("HudNationalSinglePlayer"), Pkg("InGame"), Pkg("Shared"), Pkg("BaseLocalized"), "Base" });
        // BuildLayerOwn: the layer's own package is searched first (a mode HUD's fonts / art live in its Hud<Mode>SinglePlayer package)
        string firstPackage;
        string[] First(string[] order) => firstPackage == null || order.Contains(firstPackage) && order[0] == firstPackage ? order : new[] { firstPackage }.Concat(order.Where(p => p != firstPackage)).ToArray();
        string[] TextureOfFontSearch(string pkg) => new[] { pkg, Pkg("BaseLocalized"), Pkg("InGame"), Pkg("Shared"), Pkg("HudNationalSinglePlayer") };

        // ------------------------------------------------------------------------------------------- raw assets

        public bool Has(string package, string type, string name) => FindAsset(package, type, name) != null;

        /// An asset of a package. The language packages other than ENG name their string tables "<table>.<LANG>" (InGame.GER:
        /// "MXRaven_Stunts.GER", "MXRaven_Default_Strings.GER"); ENG's are bare, DLC004 / 005's ENG ones carry ".ENG". A "localiz" asset is
        /// found by its bare name, else by the name with its package's language (a German install's HUD froze on the bare name, 2026-11-05).
        Rfx.Asset FindAsset(string package, string type, string name)
        {
            var a = install.Find(DatabaseName, package, name, type);
            if (a == null && type == "localiz")
            {
                int dot = package.LastIndexOf('.');
                if (dot > 0 && dot < package.Length - 1) a = install.Find(DatabaseName, package, name + package.Substring(dot), type);
            }
            return a;
        }

        /// A decoded (LZX) asset of the MXUI database; "script" assets lose their u32 length prefix.
        public byte[] Read(string package, string type, string name)
        {
            string k = package + "|" + type + "|" + name;
            if (assets.TryGetValue(k, out var data)) return data;
            var a = FindAsset(package, type, name);
            Rfx.Check(a != null, $"{DatabaseName}/{package}/{name}.{type} missing");
            data = install.Read(a);
            if (type == "script")
            {
                Rfx.Check(data.Length >= 4 && Rfx.U32(data, 0) == data.Length - 4, "Lua length prefix");
                var s = new byte[data.Length - 4]; Buffer.BlockCopy(data, 4, s, 0, s.Length); data = s;
            }
            return assets[k] = data;
        }

        public string Script(string package, string name) => Encoding.UTF8.GetString(Read(package, "script", name));

        public IEnumerable<string> Names(string package, string type)
        {
            var p = install.Pkg(DatabaseName, package);
            if (p != null) foreach (var a in p.OfType(type)) yield return a.name;
        }

        // ------------------------------------------------------------------------------------------- strings (localiz)

        /// key -> text of a localisation table (UTF-16 strings; DYN_ keys hold '?' placeholders of their capacity).
        public Dictionary<string, string> Strings(string package = "InGame", string table = "MXRaven_Default_Strings")
        {
            package = Pkg(package);
            string k = package + "|" + table;
            if (!strings.TryGetValue(k, out var d)) strings[k] = d = Rfx.Strings(Read(package, "localiz", table));
            return d;
        }

        // ------------------------------------------------------------------------------------------- uicmpnt layers

        /// uicmpnt = BXML (0x103EA, zlib) + a dependency trailer: u32 count, then count * (char name[64], char type[8]).
        public LayerData Layer(string package, string name)
        {
            package = Pkg(package);
            string k = package + "|" + name;
            if (layers.TryGetValue(k, out var l)) return l;
            byte[] data = Read(package, "uicmpnt", name);
            int comp = Rfx.I32(data, 32);
            Rfx.Check(comp > 0 && 36 + comp <= data.Length, "uicmpnt size");
            var xml = new byte[36 + comp]; Buffer.BlockCopy(data, 0, xml, 0, xml.Length);
            var deps = new List<object>();
            int tail = data.Length - xml.Length;
            if (tail > 0)
            {
                int n = Rfx.I32(data, xml.Length);
                Rfx.Check(n >= 0 && 4 + n * 72 == tail, "uicmpnt dependency trailer size");
                for (int i = 0; i < n; i++)
                {
                    int at = xml.Length + 4 + i * 72;
                    deps.Add(new Dictionary<string, object> { ["name"] = CStr(data, at, 64), ["type"] = CStr(data, at + 64, 8) });
                }
            }
            l = new LayerData { package = package, name = name, deps = deps, root = NodeToJson(ToEl(BxmlData.Xml(xml))) };
            return layers[k] = l;
        }

        /// The exporter's layers/<package>/<name>.json document.
        public Dictionary<string, object> LayerJson(string package, string name)
        {
            var l = Layer(package, name);
            return new Dictionary<string, object> { ["package"] = l.package, ["name"] = l.name, ["dependencies"] = l.deps, ["root"] = l.root };
        }

        static string CStr(byte[] b, int at, int max)
        {
            int e = at; while (e < at + max && b[e] != 0) e++;
            return Encoding.UTF8.GetString(b, at, e - at);
        }

        // An ElementTree-like view of a BXML tree (BxmlData.Node keeps typed values; ui_export.py works on bxml.Document.to_element()).
        sealed class El { public string tag, text; public Dictionary<string, string> attr = new Dictionary<string, string>(); public List<El> kids = new List<El>(); public string Get(string k) => attr.TryGetValue(k, out var v) ? v : null; }

        static string F9(double v)   // Python format(v, ".9g")
        {
            string s = v.ToString("G9", CultureInfo.InvariantCulture).ToLowerInvariant();
            return s;
        }
        static string Text(object v, out string prefix)
        {
            prefix = "";
            switch (v)
            {
                case null: return null;
                case string s: return s;
                case float f: prefix = "_float:"; return F9(f);
                case int i: prefix = "_int:"; return i.ToString(CultureInfo.InvariantCulture);
                case uint u: prefix = "_uint:"; return u.ToString(CultureInfo.InvariantCulture);
                case bool b: prefix = "_bool:"; return b ? "true" : "false";
                case float[] a when a.Length == 3: prefix = "_vector3:"; return F9(a[0]) + "," + F9(a[1]) + "," + F9(a[2]);
                default: throw new InvalidDataException("Unsupported UI BXML value " + v.GetType().Name);
            }
        }
        static El ToEl(Node n)
        {
            var e = new El { tag = n.tag };
            foreach (var kv in n.attributes)
            {
                string t = Text(kv.Value, out var pre);
                e.attr[kv.Key] = t == null ? "" : pre + t;
            }
            e.text = Text(n.value, out _);
            foreach (var c in n.children) e.kids.Add(ToEl(c));
            return e;
        }

        // ------------------------------------------------------------------------------------------- node_to_json

        static readonly Regex Num = new Regex(@"^-?\d+(\.\d+)?$", RegexOptions.Compiled);
        static double Dbl(string s) => double.Parse(s, NumberStyles.Float, CultureInfo.InvariantCulture);

        /// parse_val: numbers (int/float), comma lists of numbers, booleans, else the trimmed string.
        public static object ParseVal(string text)
        {
            if (text == null) return null;
            string t = text.Trim();
            if (Num.IsMatch(t)) return Dbl(t);
            if (t.IndexOf(',') >= 0)
            {
                var parts = t.Split(',');
                bool all = true; foreach (var p in parts) if (!Num.IsMatch(p.Trim())) { all = false; break; }
                if (all) { var l = new List<object>(); foreach (var p in parts) l.Add(Dbl(p.Trim())); return l; }
            }
            if (t == "True" || t == "TRUE" || t == "true") return true;
            if (t == "False" || t == "FALSE" || t == "false") return false;
            return t;
        }

        static double? FloatAttr(string s) => s == null ? (double?)null : Dbl(s.Trim());

        static Dictionary<string, object> ParseAnimation(El ca)
        {
            var tracks = new List<object>();
            foreach (var co in ca.kids)
            {
                var keys = new List<object>(); var refs = new List<object>();
                foreach (var kf in co.kids)
                {
                    if (kf.tag == "AnimationReference") { refs.Add(kf.text); continue; }
                    if (kf.tag != "KeyFrame") continue;
                    var ch = new Dictionary<string, object>();
                    foreach (var c in kf.kids) if (c.tag == "Channel") ch[c.Get("type")] = Dbl(c.text.Trim());
                    keys.Add(new Dictionary<string, object> { ["t"] = Dbl(kf.Get("keyFrameIndex").Trim()), ["ch"] = ch });
                }
                tracks.Add(new Dictionary<string, object> { ["target"] = co.Get("targetComponent"), ["refs"] = refs, ["keys"] = keys });
            }
            return new Dictionary<string, object> { ["name"] = ca.Get("name"), ["fps"] = FloatAttr(ca.Get("fps")), ["length"] = FloatAttr(ca.Get("length")), ["tracks"] = tracks };
        }

        static Dictionary<string, object> NodeToJson(El e)
        {
            var node = new Dictionary<string, object>
            {
                ["type"] = e.Get("type"), ["props"] = new Dictionary<string, object>(), ["children"] = new List<object>(),
                ["animations"] = new List<object>(), ["triggers"] = new List<object>(), ["edges"] = new List<object>(),
            };
            var props = (Dictionary<string, object>)node["props"];
            if (!string.IsNullOrEmpty(e.Get("style"))) node["style"] = e.Get("style");
            foreach (var c in e.kids)
            {
                if (c.tag == "UIComponent") ((List<object>)node["children"]).Add(NodeToJson(c));
                else if (c.tag == "CompositeAnimation") ((List<object>)node["animations"]).Add(ParseAnimation(c));
                else if (c.tag == "Trigger" || c.tag == "Edge")
                {
                    var d = new Dictionary<string, object> { ["type"] = c.Get("type") };
                    foreach (var x in c.kids) { d[x.tag] = ParseVal(x.text); if (x.tag == "Event" && x.Get("name") is string en) d["EventName"] = en; }   // a trigger's <Event name="..."/> (UI event it sends)
                    ((List<object>)node[c.tag == "Trigger" ? "triggers" : "edges"]).Add(d);
                }
                else if (props.TryGetValue(c.tag, out var old))
                {
                    var l = new List<object>();
                    if (old is List<object> ol && c.tag == "Layer") l.AddRange(ol); else l.Add(old);
                    l.Add(ParseVal(c.text));
                    props[c.tag] = l;
                }
                else props[c.tag] = ParseVal(c.text);
            }
            if (props.TryGetValue("Layer", out var lay) && !(lay is List<object>)) props["Layer"] = new List<object> { lay };
            node["name"] = props.TryGetValue("Name", out var nm) ? nm : null;
            return node;
        }

        // ------------------------------------------------------------------------------------------- cell (bitmap font)

        /// Header: f32 3.1, u16 0, u16 letterSpacing, f32 size, u32 count (CellAsset loader FUN_007fa7d0); records of 36 bytes:
        /// u32 code, f32 u0 v0 u1 v1, u32 code, u16 w, i16 left, i16 right, u16 h, u16 bottom (signed), u16 kern_count (0).
        public static Cell ParseCell(byte[] d)
        {
            Rfx.Check(d.Length >= 16, "cell header");
            float ver = Rfx.F32(d, 0); int count = (int)Rfx.U32(d, 12);
            Rfx.Check(Math.Abs(ver - 3.1f) < 1e-5f && count >= 0 && 16 + 36L * count == d.Length, "cell layout");
            var c = new Cell { size = Rfx.F32(d, 8), letterSpacing = BitConverter.ToUInt16(d, 6) };
            for (int i = 0; i < count; i++)
            {
                int p = 16 + 36 * i;
                uint code = Rfx.U32(d, p), code2 = Rfx.U32(d, p + 20);
                Rfx.Check(code == code2 && BitConverter.ToUInt16(d, p + 34) == 0, "cell glyph record");
                c.glyphs[(int)code] = new Cell.Glyph
                {
                    u0 = Rfx.F32(d, p + 4), v0 = Rfx.F32(d, p + 8), u1 = Rfx.F32(d, p + 12), v1 = Rfx.F32(d, p + 16),
                    w = BitConverter.ToUInt16(d, p + 24), left = BitConverter.ToInt16(d, p + 26), right = BitConverter.ToInt16(d, p + 28),
                    h = BitConverter.ToUInt16(d, p + 30), bottom = BitConverter.ToInt16(d, p + 32),
                };
            }
            return c;
        }

        Cell CellOf(string package, string name)
        {
            string k = package + "|" + name;
            if (!cells.TryGetValue(k, out var c)) cells[k] = c = ParseCell(Read(package, "cell", name));
            return c;
        }

        // ------------------------------------------------------------------------------------------- textures

        /// uitex.parse: header 0x77ddcc00, 4, 0, 1, levels, 0*5, levels, width, height, D3D format (21 = A8R8G8B8, or a DXT FourCC).
        public static bool TextureHeader(byte[] d, out int w, out int h, out string format, out int levels, out int offset)
        {
            w = h = levels = offset = 0; format = null;
            if (d.Length < 56) return false;
            uint[] hd = new uint[14]; for (int i = 0; i < 14; i++) hd[i] = Rfx.U32(d, i * 4);
            if (hd[0] != 0x77ddcc00 || hd[1] != 4 || hd[2] != 0 || hd[3] != 1) return false;
            levels = (int)hd[10]; w = (int)hd[11]; h = (int)hd[12]; uint fmt = hd[13];
            if (hd[4] != hd[10] || levels < 1 || levels > 14) return false;
            // (w-joincrash) a size Unity can take: 1..8192 (14 levels), so w * h * 4 never overflows and no Texture2D is asked for 0 or a huge size
            if (hd[11] < 1 || hd[11] > 8192 || hd[12] < 1 || hd[12] > 8192) return false;
            format = fmt == 0x31545844 ? "DXT1" : fmt == 0x33545844 ? "DXT3" : fmt == 0x35545844 ? "DXT5" : fmt == 21 ? "A8R8G8B8" : null;
            offset = 56 + 8 * levels;
            return format != null;
        }

        static void Rgb565(int v, out int r, out int g, out int b)
        {
            int rr = v >> 11, gg = (v >> 5) & 63, bb = v & 31;
            r = (rr << 3) | (rr >> 2); g = (gg << 2) | (gg >> 4); b = (bb << 3) | (bb >> 2);
        }

        /// uitex.decode: mip 0 -> RGBA bytes, rows top to bottom (as the exported PNG).
        public static byte[] DecodeTexture(byte[] data, out int w, out int h, out string format)
        {
            Rfx.Check(TextureHeader(data, out w, out h, out format, out _, out int off), "UI texture header");
            var o = new byte[w * h * 4];
            if (format == "A8R8G8B8")
            {
                Rfx.Check(off + w * h * 4 <= data.Length, "truncated pixels");
                for (int i = 0; i < w * h; i++) { o[i * 4] = data[off + i * 4 + 2]; o[i * 4 + 1] = data[off + i * 4 + 1]; o[i * 4 + 2] = data[off + i * 4]; o[i * 4 + 3] = data[off + i * 4 + 3]; }
                return o;
            }
            bool dxt1 = format == "DXT1"; int block = dxt1 ? 8 : 16;
            var alpha = new int[16]; var tb = new int[8]; var pr = new int[4]; var pg = new int[4]; var pb = new int[4];
            for (int by = 0; by < h; by += 4)
                for (int bx = 0; bx < w; bx += 4)
                {
                    Rfx.Check(off + block <= data.Length, "truncated blocks");
                    for (int i = 0; i < 16; i++) alpha[i] = 255;
                    if (format == "DXT3")
                    {
                        ulong bits = BitConverter.ToUInt64(data, off);
                        for (int i = 0; i < 16; i++) alpha[i] = (int)((bits >> (4 * i)) & 15) * 17;
                    }
                    else if (format == "DXT5")
                    {
                        int a0 = data[off], a1 = data[off + 1];
                        tb[0] = a0; tb[1] = a1;
                        if (a0 > a1) for (int i = 1; i <= 6; i++) tb[1 + i] = ((6 - i) * a0 + i * a1) / 7;
                        else { for (int i = 1; i <= 4; i++) tb[1 + i] = ((5 - i) * a0 + i * a1) / 5; tb[6] = 0; tb[7] = 255; }
                        ulong bits = 0; for (int i = 0; i < 6; i++) bits |= (ulong)data[off + 2 + i] << (8 * i);
                        for (int i = 0; i < 16; i++) alpha[i] = tb[(int)((bits >> (3 * i)) & 7)];
                    }
                    int c0 = BitConverter.ToUInt16(data, off + block - 8), c1 = BitConverter.ToUInt16(data, off + block - 6);
                    uint idx = BitConverter.ToUInt32(data, off + block - 4);
                    Rgb565(c0, out pr[0], out pg[0], out pb[0]); Rgb565(c1, out pr[1], out pg[1], out pb[1]);
                    if (c0 > c1 || !dxt1)
                    {
                        pr[2] = (2 * pr[0] + pr[1]) / 3; pg[2] = (2 * pg[0] + pg[1]) / 3; pb[2] = (2 * pb[0] + pb[1]) / 3;
                        pr[3] = (pr[0] + 2 * pr[1]) / 3; pg[3] = (pg[0] + 2 * pg[1]) / 3; pb[3] = (pb[0] + 2 * pb[1]) / 3;
                    }
                    else
                    {
                        pr[2] = (pr[0] + pr[1]) / 2; pg[2] = (pg[0] + pg[1]) / 2; pb[2] = (pb[0] + pb[1]) / 2;
                        pr[3] = pg[3] = pb[3] = 0;
                    }
                    for (int i = 0; i < 16; i++)
                    {
                        int x = bx + (i & 3), y = by + (i >> 2);
                        if (x >= w || y >= h) continue;
                        int ci = (int)((idx >> (2 * i)) & 3), al = alpha[i];
                        if (dxt1 && c0 <= c1 && ci == 3) al = 0;
                        int p = (y * w + x) * 4;
                        o[p] = (byte)pr[ci]; o[p + 1] = (byte)pg[ci]; o[p + 2] = (byte)pb[ci]; o[p + 3] = (byte)al;
                    }
                    off += block;
                }
            return o;
        }

        // key "ui/textures/<package>/<name>.png"
        static string TextureKey(string package, string name) => "ui/textures/" + package + "/" + name + ".png";
        static bool SplitTextureKey(string key, out string package, out string name)
        {
            package = name = null;
            if (key == null || !key.StartsWith("ui/textures/", StringComparison.Ordinal) || !key.EndsWith(".png", StringComparison.Ordinal)) return false;
            string rest = key.Substring(12, key.Length - 16); int s = rest.LastIndexOf('/');
            if (s < 0) return false;
            package = rest.Substring(0, s); name = rest.Substring(s + 1); return true;
        }

        /// A UI texture as a Unity texture: RGBA32, rows bottom up (what the PNG import gave), no mips, clamped, bilinear.
        public Texture2D Texture(string key)
        {
            if (key == null) return null;
            if (textures.TryGetValue(key, out var t)) return t;
            if (!SplitTextureKey(key, out string package, out string name) || !Has(package, "texture", name)) return textures[key] = null;
            byte[] rgba = DecodeTexture(Read(package, "texture", name), out int w, out int h, out _);
            var flipped = new byte[rgba.Length]; int row = w * 4;
            for (int y = 0; y < h; y++) Buffer.BlockCopy(rgba, y * row, flipped, (h - 1 - y) * row, row);
            t = new Texture2D(w, h, TextureFormat.RGBA32, false, false) { name = package + "/" + name, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            t.SetPixelData(flipped, 0); t.Apply(false, false);
            return textures[key] = t;
        }

        /// Width / height of a texture asset from its header (no decode); false when absent.
        public bool TextureSize(string package, string name, out int w, out int h)
        {
            w = h = 0;
            if (!Has(package, "texture", name)) return false;
            return TextureHeader(Read(package, "texture", name), out w, out h, out _, out _, out _);
        }

        // ------------------------------------------------------------------------------------------- fonts

        static string FontKey(string package, string name) => "ui/fonts/" + package + "." + name + ".json";

        /// Resolves "ui/fonts/<package>.<name>.json": the cell asset plus the atlas texture of the same name (searched in
        /// the font's package, then BaseLocalized, InGame, Shared, HudNationalSinglePlayer).
        public FontResource FontData(string key)
        {
            if (key == null) return null;
            if (fonts.TryGetValue(key, out var f)) return f;
            if (!key.StartsWith("ui/fonts/", StringComparison.Ordinal) || !key.EndsWith(".json", StringComparison.Ordinal)) return fonts[key] = null;
            string rest = key.Substring(9, key.Length - 14);
            // packages contain a dot ("InGame.ENG"): the cell name is whatever follows the longest package prefix that has it
            string pkg = null, nm = null;
            for (int i = 0; i < rest.Length; i++)
                if (rest[i] == '.' && Has(rest.Substring(0, i), "cell", rest.Substring(i + 1))) { pkg = rest.Substring(0, i); nm = rest.Substring(i + 1); break; }
            if (pkg == null) return fonts[key] = null;
            f = new FontResource { key = key, name = nm, package = pkg, cell = CellOf(pkg, nm) };
            foreach (string p in TextureOfFontSearch(pkg)) if (Has(p, "texture", nm)) { f.texturePackage = p; break; }
            if (f.texturePackage != null) f.atlas = Texture(TextureKey(f.texturePackage, nm));
            return fonts[key] = f;
        }

        // ------------------------------------------------------------------------------------------- texatlas (sprite rects)

        /// One sprite of a "texatlas": uv rect (D3D v down) in the named texture of the same package, size in pixels.
        public sealed class AtlasSprite { public string name, texture; public float u0, u1, v0, v1; public int w, h; }

        /// texatlas: u32 4, u32 ntex, u32 total, char name[64] per texture; then per texture u16 w, u16 h (its size), u32 count and
        /// records of (char name[64] (NUL padded, junk after the NUL), f32 u0, u1, v0, v1, u16 w, u16 h) = 84 bytes (88 in a few files:
        /// the stride is the one under which every record validates). Replaces the exporter's name scan, which misses these records.
        public static List<AtlasSprite> ParseAtlas(byte[] d)
        {
            Rfx.Check(d.Length >= 16 && Rfx.U32(d, 0) == 4, "texatlas header");
            int nt = (int)Rfx.U32(d, 4); Rfx.Check(nt >= 1 && nt <= 8, "texatlas texture count");
            var texNames = new string[nt];
            for (int i = 0; i < nt; i++) texNames[i] = CStr(d, 12 + i * 64, 64);
            int p = 12 + 64 * nt;   // per texture: u16 w, u16 h, u32 count, count records
            var all = new List<AtlasSprite>();
            for (int ti = 0; ti < nt; ti++)
            {
                Rfx.Check(p + 8 <= d.Length, "texatlas count");
                int count = (int)Rfx.U32(d, p + 4); p += 8;
                int stride = 0;
                foreach (int s in new[] { 84, 88 })
                {
                    if (p + (long)s * count > d.Length) continue;
                    bool ok = true;
                    for (int i = 0; i < count && ok; i++)
                    {
                        int at = p + i * s; string name = CStr(d, at, 64);
                        float u0 = Rfx.F32(d, at + 64), u1 = Rfx.F32(d, at + 68), v0 = Rfx.F32(d, at + 72), v1 = Rfx.F32(d, at + 76);
                        ok = name.Length >= 3 && 0 <= u0 && u0 < u1 && u1 <= 1.0001f && 0 <= v0 && v0 < v1 && v1 <= 1.0001f;
                    }
                    if (ok) { stride = s; break; }
                }
                Rfx.Check(stride != 0, "texatlas record stride");
                for (int i = 0; i < count; i++)
                {
                    int at = p + i * stride;
                    all.Add(new AtlasSprite { name = CStr(d, at, 64), texture = texNames[ti], u0 = Rfx.F32(d, at + 64), u1 = Rfx.F32(d, at + 68), v0 = Rfx.F32(d, at + 72), v1 = Rfx.F32(d, at + 76),
                                              w = BitConverter.ToUInt16(d, at + 80), h = BitConverter.ToUInt16(d, at + 82) });
                }
                p += count * stride;
            }
            return all;
        }

        readonly Dictionary<string, Dictionary<string, AtlasSprite>> atlases = new Dictionary<string, Dictionary<string, AtlasSprite>>();

        /// A package's texatlas by sprite name ("BaseLocalized" "ControllerButtons", "InGame" "HUDTextures" ...).
        public Dictionary<string, AtlasSprite> Atlas(string package, string name)
        {
            package = Pkg(package); string k = package + "|" + name;
            if (!atlases.TryGetValue(k, out var d))
            {
                d = new Dictionary<string, AtlasSprite>();
                foreach (var s in ParseAtlas(Read(package, "texatlas", name))) d[s.name] = s;
                atlases[k] = d;
            }
            return d;
        }

        /// The key of a texture asset ("ui/textures/<package>/<name>.png") for Texture().
        public string TextureKeyOf(string package, string name) => TextureKey(Pkg(package), name);

        // ------------------------------------------------------------------------------------------- describe (layout_of / describe)

        static readonly Dictionary<string, double> HPivot = new Dictionary<string, double> { ["Left"] = 0, ["Center"] = 0.5, ["Right"] = 1 };
        static readonly Dictionary<string, double> VPivot = new Dictionary<string, double> { ["Top"] = 0, ["Center"] = 0.5, ["Bottom"] = 1 };

        static object P(Dictionary<string, object> pr, string k) => pr.TryGetValue(k, out var v) ? v : null;
        static bool Truthy(object v) => v is null ? false : v is bool b ? b : v is double d ? d != 0 : v is string s ? s.Length > 0 : v is List<object> l ? l.Count > 0 : true;

        /// UIComponent::vf1 (0x5c6520): Justification / HorizontalJustification / VerticalJustification set the pivot in {0, .5, 1};
        /// Position is where that pivot point of the Size box goes.
        public static List<object> PivotOf(Dictionary<string, object> props)
        {
            double px = 0, py = 0;
            if (P(props, "Justification") is string j)
            {
                foreach (string tok in j.Split('_'))
                {
                    if (VPivot.ContainsKey(tok) && tok != "Center") py = VPivot[tok];
                    else if (HPivot.ContainsKey(tok) && tok != "Center") px = HPivot[tok];
                    else if (tok == "Center") { double v = j == "Center" ? 0.5 : (j.Contains("Left") || j.Contains("Right") ? px : 0.5); px = py = v; }
                }
            }
            if (P(props, "HorizontalJustification") is string hj && HPivot.TryGetValue(hj, out var hx)) px = hx;
            if (P(props, "VerticalJustification") is string vj && VPivot.TryGetValue(vj, out var vy)) py = vy;
            return new List<object> { px, py };
        }

        static Dictionary<string, object> Props(Dictionary<string, object> node) => (Dictionary<string, object>)node["props"];
        static List<object> Kids(Dictionary<string, object> node) => (List<object>)node["children"];

        public static Dictionary<string, object> LayoutOf(Dictionary<string, object> pr)
        {
            var o = new Dictionary<string, object> { ["position"] = P(pr, "Position"), ["size"] = P(pr, "Size"), ["pivot"] = PivotOf(pr) };
            if (Truthy(P(pr, "RotationZ"))) o["rotationZ_deg"] = pr["RotationZ"];
            foreach (var k in new[] { "RotationX", "RotationY" }) if (Truthy(P(pr, k))) o[k] = pr[k];
            return o;
        }

        Dictionary<string, object> FontInfo(string name)
        {
            foreach (string pkg in FontSearch)
            {
                if (!Has(pkg, "cell", name)) continue;
                var f = FontData(FontKey(pkg, name));
                var d = new Dictionary<string, object> { ["name"] = name, ["package"] = pkg, ["size"] = (double)f.cell.size };
                bool tex = f.texturePackage != null && TextureSize(f.texturePackage, name, out _, out _);
                d["atlas"] = tex ? TextureKey(f.texturePackage, name) : null;
                if (tex) { TextureSize(f.texturePackage, name, out int w, out int h); d["atlasSize"] = new List<object> { (double)w, (double)h }; } else d["atlasSize"] = null;
                d["metrics"] = FontKey(pkg, name);
                return d;
            }
            return new Dictionary<string, object> { ["name"] = name };
        }

        string TextureForMaterial(string name, out int w, out int h)
        {
            foreach (string pkg in MaterialSearch)
                if (TextureSize(pkg, name, out w, out h)) return TextureKey(pkg, name);
            w = h = 0; return null;
        }

        /// The renderer's description of a UIComponent subtree (the exporter's describe()).
        public Dictionary<string, object> Describe(Dictionary<string, object> node, string path)
        {
            var pr = Props(node);
            var d = new Dictionary<string, object> { ["path"] = path, ["name"] = node["name"], ["class"] = node["type"], ["layout"] = LayoutOf(pr) };
            var st = new Dictionary<string, object>();
            if (P(pr, "VisibleColor") != null) st["color"] = pr["VisibleColor"];
            foreach (var k in new[] { "FocusedColor", "FocusableColor", "EnabledColor", "HighlightedColor" }) if (pr.ContainsKey(k)) st[k] = pr[k];
            if (Truthy(P(pr, "VisibleFont")))
            {
                var fi = FontInfo(pr["VisibleFont"] as string);
                st["font"] = fi;
                st["textHeight"] = pr.ContainsKey("TextHeight") ? pr["TextHeight"] : (fi.TryGetValue("size", out var sz) ? sz : null);
                st["textHeightExplicit"] = pr.ContainsKey("TextHeight");
                st["resizeToText"] = pr.ContainsKey("ResizeToText") ? pr["ResizeToText"] : true;
            }
            if (Truthy(P(pr, "VisibleMaterial")))
            {
                string mat = pr["VisibleMaterial"] as string;
                string png = TextureForMaterial(mat, out int w, out int h);
                st["material"] = mat;
                st["texture"] = png;
                st["textureSize"] = png != null ? new List<object> { (double)w, (double)h } : null;
                st["mirrorX"] = Truthy(P(pr, "MirrorX"));
                st["mirrorY"] = Truthy(P(pr, "MirrorY"));
            }
            if (Truthy(P(pr, "LocalizationKey")))
            {
                string key = pr["LocalizationKey"] as string;
                st["locKey"] = pr["LocalizationKey"];
                st["locTable"] = P(pr, "Localization");
                string text = null;
                if (key != null && !(key.StartsWith("DYN_", StringComparison.Ordinal) || key.StartsWith("EMB_", StringComparison.Ordinal)))
                    Strings("InGame", "MXRaven_Default_Strings").TryGetValue(key, out text);
                st["locText"] = text;
            }
            if (pr.ContainsKey("DynamicTextLength")) st["dynamicTextLength"] = pr["DynamicTextLength"];
            if (describeIcons && Truthy(P(pr, "IconGeometry")) && IconGeometryOf(pr["IconGeometry"] as string) != null) st["iconGeometry"] = pr["IconGeometry"];   // WP-3 (BuildLayerOwn)
            if (P(pr, "Visible") is bool vis && !vis) st["initiallyHidden"] = true;
            d["style"] = st;
            var kids = Kids(node);
            if (kids.Count > 0)
            {
                var list = new List<object>();
                foreach (Dictionary<string, object> c in kids) list.Add(Describe(c, path + "/" + c["name"]));
                d["children"] = list;
            }
            return d;
        }

        /// A layer's description: Describe() of its root.
        public Dictionary<string, object> DescribeLayer(string package, string name) => Describe(Layer(package, name).root, name);

        static List<object> AnimationSummary(List<object> anims)
        {
            var list = new List<object>();
            foreach (Dictionary<string, object> a in anims)
            {
                var tracks = new List<object>();
                foreach (Dictionary<string, object> t in (List<object>)a["tracks"]) tracks.Add(new Dictionary<string, object> { ["target"] = t["target"], ["keys"] = t["keys"] });
                list.Add(new Dictionary<string, object> { ["name"] = a["name"], ["fps"] = a["fps"], ["length"] = a["length"], ["tracks"] = tracks });
            }
            return list;
        }

        // ------------------------------------------------------------------------------------------- the National HUD / pause menu

        /// The National single-player HUD (hud.json without the hand-written research notes): "elements" = the children of the
        /// layer's UIDashboard "National Race Hud" (drawOrder, animations, ownAnimations), "layerAnimations" (HUDIntro, HUDOutro),
        /// "layerJson" = the full tree document for the components' own animations.
        public Dictionary<string, object> BuildHud() => BuildRaceHud("HudNationalSinglePlayer", "SinglePlayerNational.lyr", "National Race Hud/");
        public Dictionary<string, object> BuildSupercrossHud() => BuildRaceHud("HudSupercrossSinglePlayer", "SinglePlayerSX_NAT.lyr", "SX_NAT Race Hud/");
        public Dictionary<string, object> BuildPowercrossHud() => BuildRaceHud("HudPowercrossSinglePlayer", "SinglePlayerPowercross.lyr", "Power Cross Dashboard/");
        /// The HUD of another race route: the package is the route's name (HudTerracrossSinglePlayer ...), its layer and dashboard.
        public Dictionary<string, object> BuildPackageHud(string package, string layerName, string dashboard) => BuildRaceHud(package, layerName, dashboard);
        Dictionary<string, object> BuildRaceHud(string package, string layerName, string dashboard)
        {
            var layer = Layer(package, layerName);
            Dictionary<string, object> dash = null;
            foreach (Dictionary<string, object> c in Kids(layer.root)) if ((string)c["type"] == "UIDashboard") { dash = c; break; }
            var elements = new List<object>(); int i = 0;
            if (dash == null) throw new InvalidDataException("Authored race HUD has no dashboard.");
            foreach (Dictionary<string, object> c in Kids(dash))
            {
                var e = Describe(c, dashboard + c["name"]);
                e["drawOrder"] = (double)i++;
                var names = new List<object>(); var own = new List<object>();
                foreach (Dictionary<string, object> a in (List<object>)c["animations"])
                {
                    names.Add(a["name"]);
                    var targets = new List<object>(); foreach (Dictionary<string, object> t in (List<object>)a["tracks"]) targets.Add(t["target"]);
                    own.Add(new Dictionary<string, object> { ["name"] = a["name"], ["fps"] = a["fps"], ["length"] = a["length"], ["targets"] = targets });
                }
                e["animations"] = names; e["ownAnimations"] = own;
                elements.Add(e);
            }
            return new Dictionary<string, object>
            {
                ["referenceResolution"] = new List<object> { 1280.0, 720.0 },
                ["layerAnimations"] = AnimationSummary((List<object>)layer.root["animations"]),
                ["elements"] = elements,
                ["dependenciesDeclaredByLayer"] = layer.deps,
                ["layerJson"] = LayerJson(package, layerName),
            };
        }

        /// The pause menu page (pause.json without the research notes): "layout" = Describe() of each PauseMenu.lyr child,
        /// "header" (Header.lyr), "buttonLegend" (ButtonLegend.lyr), "confirmPopups" (Quit/Restart Event popups of Shared).
        public Dictionary<string, object> BuildPause()
        {
            var layer = Layer("InGame", "PauseMenu.lyr");
            var top = new List<object>(); int i = 0;
            foreach (Dictionary<string, object> c in Kids(layer.root))
            {
                var e = Describe(c, (string)c["name"]);
                e["drawOrder"] = (double)i++;
                top.Add(e);
            }
            var popups = new Dictionary<string, object>();
            foreach (string nm in new[] { "QuitEventConfirmationPopup.lyr", "RestartEventConfirmationPopup.lyr" })
                popups[nm] = DescribeLayer("Shared", nm);
            return new Dictionary<string, object>
            {
                ["referenceResolution"] = new List<object> { 1280.0, 720.0 },
                ["layout"] = top,
                ["header"] = DescribeLayer("InGame", "Header.lyr"),
                ["buttonLegend"] = DescribeLayer("InGame", "ButtonLegend.lyr"),
                ["confirmPopups"] = popups,
                ["animations"] = SummaryOfAnimations((List<object>)layer.root["animations"]),
                ["dependenciesDeclaredByLayer"] = layer.deps,
            };
        }

        /// A layer as a set of top-level elements (paths relative to the layer root, as the HUD's and the pause menu's),
        /// its layer animations and the full tree document for the components' own animations.
        /// BuildLayer with the layer's own package searched first for its fonts and materials (OriginalLiveUi: any page's layer).
        public Dictionary<string, object> BuildLayerOwn(string package, string name)
        {
            string was = firstPackage; firstPackage = Pkg(package); describeIcons = true;
            try
            {
                var doc = BuildLayer(package, name);
                foreach (var e in (List<object>)doc["elements"]) ResolveAtlasMaterials(e as Dictionary<string, object>, Pkg(package));
                return doc;
            }
            finally { firstPackage = was; describeIcons = false; }
        }

        // ------------------------------------------------------------------------------------------- icon geometry (WP-3)
        // UIIconGeometryComponent draws an "icongeom" asset of the MXUI database (IconGeometry: EventIntro's TireNub and ScenarioSlugBacking,
        // the front end's gradients and borders) with its VisibleMaterial (1x1White) and colour: a triangle list in the asset's own box,
        // centred, y down. Layout (decoded; every shipped asset parses exactly): u32 3, u32 4, f32 width, f32 height, u32 1, u16 n, u32 0,
        // u32 0, u16 n, u8 1, then n vertices of 12 bytes: half x, half y, u32 0, u32 colour A8R8G8B8 (per vertex: the gradients).
        public sealed class IconGeometry { public float width, height; public float[] xy; public uint[] argb; }
        bool describeIcons;
        readonly Dictionary<string, IconGeometry> icons = new Dictionary<string, IconGeometry>(StringComparer.Ordinal);
        /// The icon geometry of that name (the layer's own package first, then InGame, Shared, FrontEnd, BaseLocalized), null when none.
        public IconGeometry IconGeometryOf(string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            lock (icons)
            {
                if (icons.TryGetValue(name, out var g)) return g;
                foreach (string p in First(new[] { Pkg("InGame"), Pkg("Shared"), Pkg("FrontEnd"), Pkg("BaseLocalized"), "Base" }))
                {
                    if (!Has(p, "icongeom", name)) continue;
                    try { g = ParseIconGeometry(Read(p, "icongeom", name)); } catch (Exception e) { Debug.LogWarning("icon geometry " + p + "/" + name + ": " + e.Message); }
                    break;
                }
                return icons[name] = g;
            }
        }
        public static IconGeometry ParseIconGeometry(byte[] d)
        {
            Rfx.Check(d.Length >= 33 && Rfx.U32(d, 0) == 3 && Rfx.U32(d, 4) == 4, "icongeom header");
            int n = BitConverter.ToUInt16(d, 20);
            Rfx.Check(BitConverter.ToUInt16(d, 30) == n && d.Length == 33 + 12 * n && n % 3 == 0, "icongeom vertex count");
            var g = new IconGeometry { width = BitConverter.ToSingle(d, 8), height = BitConverter.ToSingle(d, 12), xy = new float[2 * n], argb = new uint[n] };
            for (int i = 0; i < n; i++)
            {
                int o = 33 + 12 * i;
                g.xy[2 * i] = Mathf.HalfToFloat(BitConverter.ToUInt16(d, o)); g.xy[2 * i + 1] = Mathf.HalfToFloat(BitConverter.ToUInt16(d, o + 2));
                g.argb[i] = Rfx.U32(d, o + 8);
            }
            return g;
        }

        // A material that is no texture of its own is a sprite of a texatlas (InGame HUDTextures: the compass, TargetPracticeIcon,
        // HitDiscoveryIcon ...; Shared "Shared": ChallengeIconGold ...): style "texture" = the atlas texture, "atlasUv" = its rect in Unity
        // uv (x, y bottom, w, h; the atlas rects are D3D, v down).
        void ResolveAtlasMaterials(Dictionary<string, object> node, string own)
        {
            if (node == null) return;
            if (node.TryGetValue("style", out var so) && so is Dictionary<string, object> st && st.TryGetValue("material", out var mo) && mo is string mat
                && (!st.TryGetValue("texture", out var tx) || tx == null))
            {
                foreach (string p in new[] { own, Pkg("InGame"), Pkg("Shared"), Pkg("BaseLocalized") }.Distinct())
                {
                    AtlasSprite sp = null;
                    foreach (string a in Names(p, "texatlas")) { try { if (Atlas(p, a).TryGetValue(mat, out sp)) break; } catch (Exception) { } }
                    if (sp == null) continue;
                    string tp = Has(p, "texture", sp.texture) ? p : new[] { Pkg("InGame"), Pkg("Shared"), Pkg("BaseLocalized"), "Base" }.FirstOrDefault(q => Has(q, "texture", sp.texture));
                    if (tp == null) continue;
                    st["texture"] = TextureKey(tp, sp.texture);
                    st["textureSize"] = new List<object> { (double)sp.w, (double)sp.h };
                    st["atlasUv"] = new List<object> { (double)sp.u0, (double)(1 - sp.v1), (double)(sp.u1 - sp.u0), (double)(sp.v1 - sp.v0) };
                    break;
                }
            }
            if (node.TryGetValue("children", out var k) && k is List<object> kids) foreach (var c in kids) ResolveAtlasMaterials(c as Dictionary<string, object>, own);
        }

        public Dictionary<string, object> BuildLayer(string package, string name)
        {
            var layer = Layer(package, name);
            var elements = new List<object>(); int i = 0;
            foreach (Dictionary<string, object> c in Kids(layer.root))
            {
                var e = Describe(c, (string)c["name"]);
                e["drawOrder"] = (double)i++;
                elements.Add(e);
            }
            return new Dictionary<string, object>
            {
                ["elements"] = elements,
                ["layerAnimations"] = AnimationSummary((List<object>)layer.root["animations"]),
                ["layerJson"] = LayerJson(package, name),
            };
        }

        static List<object> SummaryOfAnimations(List<object> anims)
        {
            var list = new List<object>();
            foreach (Dictionary<string, object> a in anims)
            {
                var targets = new List<object>(); foreach (Dictionary<string, object> t in (List<object>)a["tracks"]) targets.Add(t["target"]);
                list.Add(new Dictionary<string, object> { ["name"] = a["name"], ["length"] = a["length"], ["targets"] = targets });
            }
            return list;
        }
    }
}
