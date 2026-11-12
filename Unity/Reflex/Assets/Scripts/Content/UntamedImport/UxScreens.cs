// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_screens.py (DESIGN 23.4): jumbotron screens as
// still images (ux_build_track --screens stills, the default of the sx builds).
//   UxScreens.Build(ctx, disc, materialEntries, gain, paths, log)   (record, {material asset: (bytes, [textures], template)},
//                                                                   {(texture, 'texture'): (bytes, source, origin)})
//   UxScreens.Layout()            the layout plan as data: the fields of the reference's pinned templates/screens/screens_layout.json
//                                 (md5 LAYOUT_MD5) that ux_screens reads, for the 19 screen materials of 11 levels (Lincoln has none)
//   UxScreens.H4 / StillReference / LightReference / MipChain / Encode / LightRecipe
// Where the pictures come from: nothing is stored. A still is generated from a texture of the player's Untamed disc: the MX vs ATV
// Untamed logo 'Weststone_AD04' of WestStone_STADIUM (the SX screens) or 'endurocrosslogo' of Fairview_STADIUM (the EC arena), read
// through the disc (so Build needs the whole disc, not only the level being converted), resized (Lanczos) and letterboxed with black
// bars into the UV0 rectangle the screen faces sample, mirrored where the plan says so, with a box-filtered mip chain, DXT1. The flat
// lights UXS_Light_<RRGGBB> are generated constants (A8R8G8B8).
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_screens.py.
    public static class UxScreens
    {
        public const string SCHEMA = "ux_screens/1";
        public const string LAYOUT_FILE = "screens/screens_layout.json", LAYOUT_MD5 = "6bd8b44f0e6efdcb6e2751ce4eafead2";
        public const double DEFAULT_LIGHT = 0.9;
        public const double STILL_TOL = 12.0, STILL_TOL_SMALL = 40.0; public const int SMALL_PX = 128;

        // ================================================================================================ the layout (data)
        /// One screen material of the plan (screens_layout.json 'layout'[level][material]: the fields ux_screens reads).
        sealed class Row
        {
            public string level, material, action = "still", reason, still, source = "Weststone_AD04", sourceLevel = "WestStone_STADIUM";
            public int w = 512, h = 256; public int[] dst; public bool flipU; public int addressU = 3;
            /// light: null = the existing lightmap; else the generated flat light's plan gain, size and lit texels.
            public bool generated; public double gain = 1.0; public int lightSize = 4; public int[] lit;
        }
        static Row Still(string level, string material, string still, int[] dst, bool generated) => new Row { level = level, material = material, still = still, dst = dst, generated = generated };
        static readonly Row[] ROWS =
        {
            Still("HarrisPark_STADIUM", "Harrispark_JumboScreen", "UXS_Weststone_AD04_91DE", new[] { 0, 33, 512, 223 }, false),
            Still("Norton_STADIUM", "Norton_Jumbo", "UXS_Weststone_AD04_4516", new[] { 1, 11, 512, 246 }, true),
            new Row { level = "Capital_City_STADIUM", material = "CapitalCity_Jumbotron01C", still = "UXS_Weststone_AD04_FD74", dst = new[] { 0, 33, 512, 224 }, addressU = 1,
                generated = true, gain = 0.85, lightSize = 128, lit = new[] { 84, 29, 110, 53 } },
            new Row { level = "Capital_City_STADIUM", material = "CapitalCity_Jumbotron02C", still = "UXS_Weststone_AD04_FB23", h = 512, dst = new[] { 0, 112, 512, 400 },
                generated = true, gain = 0.85 },
            new Row { level = "Columbus_STADIUM", material = "Columbus_Jumbo01JumboScreen", still = "UXS_Weststone_AD04_0A99", dst = new[] { 0, 27, 512, 229 }, flipU = true, generated = true },
            new Row { level = "Columbus_STADIUM", material = "Columbus_Jumbo02JumboScreen", still = "UXS_Weststone_AD04_0A99", dst = new[] { 0, 27, 512, 229 }, flipU = true, generated = true },
            Still("Columbus_STADIUM", "Columbus_Jumbo03JumboScreen", "UXS_Weststone_AD04_041D", new[] { 0, 10, 512, 246 }, true),
            new Row { level = "Columbus_STADIUM", material = "Columbus_Jumbo03Screens", action = "keep",
                reason = "no main face: every face is a thin strip / sliver (f0 thin (1.5 x 112.8 ft), UV0 rotated/skewed (7971439.11), UV0 span 0.90 x 0.01; " +
                    "f1 thin (11.5 x 2.1 ft), UV0 rotated/skewed (3859576.06), UV0 span 0.01 x 0.04; f2 thin (11.5 x 2.1 ft), UV0 rotated/skewed (3906250.00), " +
                    "UV0 span 0.01 x 0.04; f3 thin (2.0 x 238.5 ft), UV0 rotated/skewed (3906012.01), UV0 span 0.93 x 0.00; f4 thin (1.5 x 112.8 ft), " +
                    "UV0 rotated/skewed (7971439.11), UV0 span 0.90 x 0.01); keeps the release texture (plain dark)" },
            Still("Columbus_STADIUM", "Columbus_LogoJumboScreen", "UXS_Weststone_AD04_041D", new[] { 0, 10, 512, 246 }, true),
            new Row { level = "Denton_STADIUM", material = "Denton_Jumbo_01CCJumbo1C", still = "UXS_Weststone_AD04_AC2B", w = 256, dst = new[] { 0, 74, 256, 182 }, generated = true },
            Still("Denton_STADIUM", "Denton_Jumbo_02CCJumbo1C", "UXS_Weststone_AD04_C17D", new[] { 0, 14, 512, 242 }, false),
            Still("Denton_STADIUM", "Denton_Jumbo_03CCJumbo1C", "UXS_Weststone_AD04_C17D", new[] { 0, 14, 512, 242 }, false),
            Still("Foothills_STADIUM", "MrJumbo", "UXS_Weststone_AD04_225A", new[] { 0, 12, 512, 244 }, true),
            Still("WestStone_STADIUM", "MrJumbo", "UXS_Weststone_AD04_E3DA", new[] { 20, 0, 492, 256 }, true),
            Still("Jefferson_STADIUM", "Jefferson_JumboMrJumbo", "UXS_Weststone_AD04_C17D", new[] { 0, 14, 512, 242 }, false),
            Still("Jefferson_STADIUM", "Jefferson_LogoMrJumbo", "UXS_Weststone_AD04_C17D", new[] { 0, 14, 512, 242 }, false),
            Still("Wilford_STADIUM", "Wilford_JumboJumboB", "UXS_Weststone_AD04_CACB", new[] { 50, 36, 462, 220 }, false),
            Still("BridgePort_STADIUM", "Bridgeport_09_screen", "UXS_Weststone_AD04_04FA", new[] { 10, 25, 502, 231 }, false),
            new Row { level = "Fairview_STADIUM", material = "MrJumbo", still = "UXS_endurocrosslogo_18BA", source = "endurocrosslogo", sourceLevel = "Fairview_STADIUM",
                dst = new[] { 0, 11, 512, 245 }, generated = true },
        };
        static List<object> Ints(params int[] v) { var l = new List<object>(); foreach (int x in v) l.Add((long)x); return l; }
        static JObj Address(int u) => new JObj().Set("ADDRESSU", (long)u).Set("ADDRESSV", 3L).Set("MIPFILTER", 2L);

        /// layout(): the plan document restricted to what this module reads: {'overrides': {'<Level>/<material>': {'gain'}}, 'layout':
        /// {level: {material: {action, reason | still {name, recipe}, address, light {kind, gain, recipe}, light_address, mirror_u,
        /// flip_v, canvas, dst_px}}}} (equal to those fields of the pinned file).
        public static JObj Layout()
        {
            var overrides = new JObj(); var layout = new JObj();
            foreach (var r in ROWS)
            {
                if (!(layout.Get(r.level) is JObj lv)) layout.Set(r.level, lv = new JObj());
                var L = new JObj().Set("action", r.action);
                lv.Set(r.material, L);
                if (r.action != "still") { L.Set("reason", r.reason); continue; }
                var recipe = new JObj().Set("kind", "screen_still").Set("source", r.source).Set("source_level", r.sourceLevel).Set("size", Ints(r.w, r.h)).Set("dst", Ints(r.dst))
                    .Set("flip_u", r.flipU).Set("flip_v", false).Set("pad", Ints(0, 0, 0)).Set("filter", "lanczos").Set("grid", 0L);
                L.Set("still", new JObj().Set("name", r.still).Set("recipe", recipe));
                L.Set("address", Address(r.addressU));
                if (r.generated)
                {
                    L.Set("light", new JObj().Set("kind", "generated").Set("gain", r.gain)
                        .Set("recipe", new JObj().Set("size", Ints(r.lightSize, r.lightSize)).Set("lit_texels", r.lit == null ? null : new List<object> { Ints(r.lit) }).Set("dark_value", 0.0)));
                    L.Set("light_address", Address(3));
                    if (r.gain != 1.0) overrides.Set(r.level + "/" + r.material, new JObj().Set("gain", r.gain));
                }
                else { L.Set("light", new JObj().Set("kind", "existing")); L.Set("light_address", null); }
                L.Set("mirror_u", r.flipU).Set("flip_v", false).Set("canvas", Ints(r.w, r.h)).Set("dst_px", Ints(r.dst));
            }
            return new JObj().Set("overrides", overrides).Set("layout", layout);
        }

        // ================================================================================================ recipes and images
        /// h4(obj): the first 4 hex digits (upper case) of md5 of the object's ux_common JSON text.
        public static string H4(object obj) => UxBin.Md5(new System.Text.UTF8Encoding(false).GetBytes(UxJson.DumpsUx(obj, 1))).Substring(0, 4).ToUpperInvariant();
        static string OneLine(object obj) => UxJson.DumpsUx(obj, -1).Trim();
        static long Int(object v) => UxJson.Int(v);
        static double Num(object v) => v is double d ? d : v is long l ? l : throw new UxException("a number was expected");
        static bool Truthy(object v) => v is bool b ? b : v is long l ? l != 0 : v is double d ? d != 0 : v is string s ? s.Length > 0 : v is List<object> a ? a.Count > 0 : v is JObj j ? j.items.Count > 0 : v != null;

        /// source_rgb(level, name): RGB (h, w, 3) of an Untamed texture of any level of the disc (level 0 of its first surface).
        public static UxImage SourceRgb(UxDisc disc, string level, string name, System.Threading.CancellationToken cancel = default)
        {
            var dl = disc.Level(level, cancel);
            if (!dl.HasExtract(name, "texture")) throw new UxException("still source " + level + "/" + name + " not in extract/");
            var rgba = new UxTexture(dl.ReadExtract(name, "texture", cancel)).Rgba();
            if (rgba.c == 3) return rgba;
            var o = new byte[rgba.w * rgba.h * 3];
            for (int i = 0, j = 0; i < rgba.w * rgba.h; i++, j += 3) { o[j] = rgba.data[i * rgba.c]; o[j + 1] = rgba.data[i * rgba.c + 1]; o[j + 2] = rgba.data[i * rgba.c + 2]; }
            return new UxImage(o, rgba.h, rgba.w, 3);
        }

        /// still_reference(recipe): level 0 (uint8 H x W x 3) of a screen still from the source picture.
        public static UxImage StillReference(JObj recipe, UxImage src)
        {
            var size = UxJson.Arr(recipe.Get("size")); var dst = UxJson.Arr(recipe.Get("dst")); var pad = UxJson.Arr(recipe.Get("pad"));
            int W = (int)Int(size[0]), H = (int)Int(size[1]), x0 = (int)Int(dst[0]), y0 = (int)Int(dst[1]), x1 = (int)Int(dst[2]), y1 = (int)Int(dst[3]);
            var canvas = new byte[H * W * 3];
            for (int i = 0; i < H * W; i++) for (int k = 0; k < 3; k++) canvas[i * 3 + k] = (byte)Int(pad[k]);
            var im = UxPil.Resize(src, x1 - x0, y1 - y0, UxPil.Filter.Lanczos);
            UxBin.Require(x0 >= 0 && y0 >= 0 && x1 <= W && y1 <= H, "still recipe: dst outside the canvas");
            for (int y = y0; y < y1; y++) Buffer.BlockCopy(im.data, (y - y0) * im.w * 3, canvas, (y * W + x0) * 3, im.w * 3);
            bool fu = Truthy(recipe.Get("flip_u")), fv = Truthy(recipe.Get("flip_v"));
            if (fu || fv)
            {
                var f = new byte[canvas.Length];
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++)
                    {
                        int sy = fv ? H - 1 - y : y, sx = fu ? W - 1 - x : x;
                        f[(y * W + x) * 3] = canvas[(sy * W + sx) * 3]; f[(y * W + x) * 3 + 1] = canvas[(sy * W + sx) * 3 + 1]; f[(y * W + x) * 3 + 2] = canvas[(sy * W + sx) * 3 + 2];
                    }
                canvas = f;
            }
            return new UxImage(canvas, H, W, 3);
        }

        /// light_reference(recipe): the flat (or masked) light, uint8 (h, w, 3).
        public static UxImage LightReference(JObj recipe)
        {
            var size = UxJson.Arr(recipe.Get("size"));
            int nw = (int)Int(size[0]), nh = (int)Int(size[1]);
            byte v = (byte)(int)UxNp.Round(Num(recipe.Get("value")) * 255);
            var a = new byte[nh * nw * 3];
            for (int i = 0; i < a.Length; i++) a[i] = v;
            if (recipe.Get("lit_texels") is List<object> lit && lit.Count > 0)
            {
                byte dark = (byte)(int)UxNp.Round(Num(recipe.Get("dark_value")) * 255);
                for (int i = 0; i < a.Length; i++) a[i] = dark;
                foreach (object ro in lit)
                {
                    var r = UxJson.Arr(ro);
                    int x0 = (int)Int(r[0]), y0 = (int)Int(r[1]), x1 = (int)Int(r[2]), y1 = (int)Int(r[3]);
                    for (int y = Math.Max(0, y0); y < Math.Min(nh, y1 + 1); y++)
                        for (int x = Math.Max(0, x0); x < Math.Min(nw, x1 + 1); x++) { a[(y * nw + x) * 3] = v; a[(y * nw + x) * 3 + 1] = v; a[(y * nw + x) * 3 + 2] = v; }
                }
            }
            return new UxImage(a, nh, nw, 3);
        }

        /// One float64 image (h, w, 3).
        public sealed class F64Image { public double[] data; public int h, w; }
        /// mip_chain(level0): level 0 and its box-filtered levels (float64) down to 1 x 1. The sizes are powers of two.
        public static List<F64Image> MipChain(UxImage level0)
        {
            UxBin.Require(level0.c == 3, "mip_chain: RGB expected");
            var first = new F64Image { h = level0.h, w = level0.w, data = new double[level0.data.Length] };
            for (int i = 0; i < first.data.Length; i++) first.data[i] = level0.data[i];
            var o = new List<F64Image> { first };
            while (Math.Max(o[o.Count - 1].h, o[o.Count - 1].w) > 1)
            {
                var a = o[o.Count - 1];
                if (a.h > 1)
                {
                    UxBin.Require(a.h % 2 == 0, "mip_chain: odd height " + a.h);
                    var b = new F64Image { h = a.h / 2, w = a.w, data = new double[a.h / 2 * a.w * 3] };
                    for (int y = 0; y < b.h; y++) for (int i = 0; i < a.w * 3; i++) b.data[y * a.w * 3 + i] = (a.data[2 * y * a.w * 3 + i] + a.data[(2 * y + 1) * a.w * 3 + i]) / 2;
                    a = b;
                }
                if (a.w > 1)
                {
                    UxBin.Require(a.w % 2 == 0, "mip_chain: odd width " + a.w);
                    var b = new F64Image { h = a.h, w = a.w / 2, data = new double[a.h * (a.w / 2) * 3] };
                    for (int y = 0; y < a.h; y++) for (int x = 0; x < b.w; x++) for (int k = 0; k < 3; k++)
                        b.data[(y * b.w + x) * 3 + k] = (a.data[(y * a.w + 2 * x) * 3 + k] + a.data[(y * a.w + 2 * x + 1) * 3 + k]) / 2;
                    a = b;
                }
                o.Add(a);
            }
            return o;
        }

        static double MeanAbsRgb(UxImage dec, F64Image rf)
        {
            var d = new double[rf.h * rf.w * 3];
            for (int i = 0; i < rf.h * rf.w; i++) for (int k = 0; k < 3; k++) d[i * 3 + k] = Math.Abs((double)dec.data[i * dec.c + k] - rf.data[i * 3 + k]);
            return UxNp.Sum(d) / d.Length;
        }

        /// encode(level0, fmt): (PC texture bytes, the per-level mean |RGB| error rounded to 3 digits) of level0 and its box-filtered
        /// chain; fmt 'DXT1' (per level the better of the two encoders) or 'A8R8G8B8'.
        public static (byte[] data, List<double> errs) Encode(UxImage level0, string fmt)
        {
            byte[] pfmt = fmt == "DXT1" ? UxTex.PC_DXT1 : UxTex.PC_A8R8G8B8;
            var payload = new List<byte[]>(); var errs = new List<double>();
            int h = level0.h, w = level0.w;
            foreach (var rf in MipChain(level0))
            {
                int lh = rf.h, lw = rf.w;
                var q = new byte[lh * lw * 3]; var q4 = new byte[lh * lw * 4];
                for (int i = 0; i < lh * lw; i++)
                {
                    for (int k = 0; k < 3; k++)
                    {
                        double v = UxNp.Round(rf.data[i * 3 + k]);
                        byte c = (byte)(v < 0 ? 0 : v > 255 ? 255 : v);
                        q[i * 3 + k] = c; q4[i * 4 + k] = c;
                    }
                    q4[i * 4 + 3] = 255;
                }
                byte[] raw; double er;
                if (fmt == "A8R8G8B8")
                {
                    raw = new byte[lh * lw * 4];
                    for (int i = 0; i < lh * lw; i++) { raw[i * 4] = q4[i * 4 + 2]; raw[i * 4 + 1] = q4[i * 4 + 1]; raw[i * 4 + 2] = q4[i * 4]; raw[i * 4 + 3] = q4[i * 4 + 3]; }
                    er = 0.0;
                }
                else
                {
                    raw = UxPil.BcEncode(new UxImage(q, lh, lw, 3), false);
                    er = MeanAbsRgb(UxTextures.PcDecode("DXT1", lw, lh, raw), rf);
                    byte[] raw2 = UxTextures.Bc1Encode(new UxImage(q4, lh, lw, 4));
                    double er2 = MeanAbsRgb(UxTextures.PcDecode("DXT1", lw, lh, raw2), rf);
                    if (er2 < er) { raw = raw2; er = er2; }
                }
                payload.Add(raw); errs.Add(UxNp.PyRound(er, 3));
            }
            var table = new List<(int size, int pitch)>();
            for (int i = 0; i < payload.Count; i++) table.Add((payload[i].Length, UxTex.PcPitch(pfmt, Math.Max(1, w >> i))));
            return (UxTex.BuildPc(new List<(int mips, int w, int h, byte[] fmt)> { (payload.Count, w, h, pfmt) }, table, payload), errs);
        }

        /// light_recipe(L, gain): (name, recipe) of the generated flat light for a Template_Diffuse screen at `gain`.
        public static (string name, JObj recipe) LightRecipe(JObj L, double gain)
        {
            int g8 = (int)UxNp.Round(Math.Min(1.0, Math.Max(0.0, gain)) * 255);
            var pr = UxJson.Obj(UxJson.Get(L, "light", "recipe"));
            var size = new List<object>(UxJson.Arr(pr.Get("size")));
            var lrec = new JObj().Set("kind", "screen_light").Set("size", size).Set("value", UxNp.PyRound(g8 / 255.0, 4)).Set("lit_texels", pr.Get("lit_texels"))
                .Set("dark_value", pr.Has("dark_value") ? pr.Get("dark_value") : 0.0);
            string hex = g8.ToString("X2", CultureInfo.InvariantCulture);
            string name = "UXS_Light_" + hex + hex + hex + (Truthy(lrec.Get("lit_texels")) ? "_" + H4(lrec) : "");
            return (name, lrec);
        }

        // ================================================================================================ build
        /// One screen material override: the material bytes, its textures and its template.
        public sealed class ScreenMaterial { public byte[] data; public List<string> textures; public string template; }
        /// What Build returns.
        public sealed class Result
        {
            public JObj record;
            public UxOrdered<string, ScreenMaterial> materials = new UxOrdered<string, ScreenMaterial>(StringComparer.Ordinal);
            public UxAssetMap textures = new UxAssetMap();
        }
        static JObj CopyObj(object o) { var j = new JObj(); if (o is JObj s) foreach (var kv in s.items) j.Set(kv.Key, kv.Value); return j; }
        static JObj IntObj(object o) { var j = new JObj(); if (o is JObj s) foreach (var kv in s.items) j.Set(kv.Key, Int(kv.Value)); return j; }

        /// build(level, material_entries, gain, log). materialEntries(name): the S4 materials.json entry of an Untamed material
        /// (UxSources.material_entries) or null. disc: the whole Untamed disc (the stills are made from other levels' textures).
        public static Result Build(UxContext ctx, UxDisc disc, Func<string, JObj> materialEntries, double gain = DEFAULT_LIGHT, UxPaths paths = null, Action<string> log = null)
        {
            paths = paths ?? new UxPaths();
            var lv = ctx.lv; var plan = Layout();
            var res = new Result();
            var mrecs = new List<object>();
            var rec = new JObj().Set("schema", SCHEMA).Set("option", "stills").Set("layout", paths.Templates(LAYOUT_FILE)).Set("layout_md5", LAYOUT_MD5).Set("light_gain", gain)
                .Set("materials", mrecs);
            res.record = rec;
            var overridden = new HashSet<string>(StringComparer.Ordinal);
            foreach (var kv in UxJson.Obj(plan.Get("overrides")).items)
            {
                int s = kv.Key.IndexOf('/');
                if (s >= 0 && kv.Key.Substring(0, s) == lv.disc) overridden.Add(kv.Key.Substring(s + 1));
            }
            int stills = 0; var stillNames = new List<string>();
            if (UxJson.Get(plan, "layout", lv.disc) is JObj levelPlan)
                foreach (var kv in levelPlan.items)
                {
                    string mname = kv.Key; var L = (JObj)kv.Value;
                    var r = new JObj().Set("material", mname).Set("action", L.Get("action"));
                    mrecs.Add(r);
                    if (L.Get("action") as string != "still") { r.Set("reason", L.Get("reason")); continue; }
                    var e = materialEntries(mname);
                    if (e == null || e.Get("action") as string != "convert")
                    {
                        r.Set("action", "skipped").Set("reason", "S4 entry " + (e == null ? "None" : (e.Get("action") as string ?? "None")));
                        continue;
                    }
                    var recipe = UxJson.Obj(UxJson.Get(L, "still", "recipe"));
                    string sname = "UXS_" + recipe.Get("source") + "_" + H4(recipe);
                    if (sname != UxJson.Get(L, "still", "name") as string) throw new UxException(mname + ": still name " + sname + " != plan " + UxJson.Get(L, "still", "name") + " (recipe hash rule)");
                    if (!res.textures.Has(sname, "texture"))
                    {
                        string slevel = recipe.Get("source_level") as string, source = recipe.Get("source") as string;
                        var rf = StillReference(recipe, SourceRgb(disc, slevel, source, ctx.cancel));
                        var (data, errs) = Encode(rf, "DXT1");
                        var bad = new List<string>();
                        for (int i = 0; i < errs.Count; i++)
                            if (errs[i] > ((Math.Max(rf.h, rf.w) >> i) >= SMALL_PX ? STILL_TOL : STILL_TOL_SMALL)) bad.Add("(" + i + ", " + PyFormat.Repr(errs[i]) + ")");
                        if (bad.Count > 0) throw new UxException("still " + sname + " misses the DXT1 tolerance on levels [" + string.Join(", ", bad) + "]");
                        res.textures.Set(sname, "texture", data, "extract/" + slevel + "/texture/" + source,
                            "ux_screens.still_reference (letterboxed " + source + ", recipe " + OneLine(recipe) + ")");
                    }
                    var addr = IntObj(L.Get("address"));
                    var spIn = e.Get("sampler_patch") as List<object> ?? new List<object>();
                    var light = UxJson.Obj(L.Get("light"));
                    string lightName; JObj s0, s1; object g;
                    var etex = UxJson.Arr(e.Get("textures"));
                    if (light.Get("kind") as string == "existing")
                    {
                        if (e.Get("template") as string != "Template_Prelit") throw new UxException(mname + ": plan keeps the existing lightmap but S4 made " + e.Get("template"));
                        lightName = etex[0] as string;
                        s0 = spIn.Count > 0 && Truthy(spIn[0]) ? CopyObj(spIn[0]) : new JObj();
                        s1 = spIn.Count > 1 && Truthy(spIn[1]) ? CopyObj(spIn[1]) : new JObj();
                        g = null;
                    }
                    else
                    {
                        double gg = overridden.Contains(mname) ? (light.Has("gain") ? Num(light.Get("gain")) : 1.0) : gain;
                        g = gg;
                        var (ln, lrec) = LightRecipe(L, gg);
                        lightName = ln;
                        if (!res.textures.Has(lightName, "texture"))
                            res.textures.Set(lightName, "texture", Encode(LightReference(lrec), "A8R8G8B8").data, "generated", "ux_screens.light_reference " + OneLine(lrec));
                        s0 = IntObj(L.Get("light_address"));
                        s1 = spIn.Count > 0 && Truthy(spIn[0]) ? CopyObj(spIn[0]) : new JObj();
                    }
                    foreach (var a in addr.items) s1.Set(a.Key, a.Value);
                    var tex = new List<string> { lightName, sname };
                    byte[] mdata = UxMaterials.BuildMaterial(ctx.stock, "Template_Prelit", tex, e.Get("state_patch") as JObj, new List<JObj> { s0, s1 });
                    string asset = e.Get("asset") as string;
                    res.materials[asset] = new ScreenMaterial { data = mdata, textures = tex, template = "Template_Prelit" };
                    r.Set("asset", asset).Set("template_before", e.Get("template")).Set("textures_before", new List<object>(etex)).Set("template", "Template_Prelit")
                        .Set("textures", new List<object> { lightName, sname }).Set("light", new JObj().Set("kind", light.Get("kind")).Set("name", lightName).Set("gain", g))
                        .Set("sampler_patch", new List<object> { s0, s1 }).Set("still", sname).Set("mirror_u", L.Get("mirror_u")).Set("flip_v", L.Get("flip_v"))
                        .Set("canvas", L.Get("canvas")).Set("dst_px", L.Get("dst_px")).Set("md5", UxBin.Md5(mdata));
                    stills++; stillNames.Add(mname);
                }
            var tx = new JObj();
            foreach (var kv in res.textures)
                tx.Set(kv.Key.name + "." + kv.Key.type, new JObj().Set("md5", UxBin.Md5(kv.Value.data)).Set("bytes", (long)kv.Value.data.Length).Set("origin", kv.Value.origin));
            rec.Set("textures", tx);
            log?.Invoke("screens: " + stills + " still material(s) [" + string.Join(", ", stillNames) + "], " + res.textures.Count + " generated texture(s)");
            return res;
        }
    }
}
