// The run-time number plate and jersey name / number textures of MX vs ATV Reflex, made 1:1 as the game makes them
// (_re/notes/plates.md): the game draws them on the CPU with its own bitmap fonts from <install>/data.fpack (not the UI's
// cell fonts) into a float RGBA image, then builds the mip chain itself and fills an A8R8G8B8 texture:
//   bike   FUN_005520d0(this = &P->b00, number = P+0x210, dark, hr)       -> P+0xb00, bound to the vehicle model's "Numbers"
//   rider  FUN_00551110(this = P+0xb04, name = P+0x200, number = P+0x210, onJersey, invert, hr)
//                                                                         -> P+0xb24 "Rider_NamNum_Plate" (jersey),
//                                                                            P+0xb28 "Rider_CP_NamNum_Plate" (chest protector)
//   text   FUN_006d5af0 (pass 1: the glyph's outline bitmap, colour 1, alpha = max) then FUN_006d6000 (pass 2: the glyph's
//          core bitmap, colour 2, alpha kept); glyphs scaled by FUN_006d64c0's box filter; widths FUN_006d5a80
//   mips   each level from the previous float level: ((S(2x,2y) + S(2x+1,2y)) + S(2x,2y+1) + S(2x+1,2y)) / 4 inside a
//          1-texel border (the border stays 0), levels of height <= 2 stay 0; texels floor(clamp(v) x 255)
// The inputs (name, number, colours) are the running game's own (P = G+0x7460 + slot*0x12a0, G = [0xa946b8]): ReflexPlateText.
// Everything float is rounded per operation as the original's SSE code does (x87 parts: double, the interpreter's PC 53).
using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Text;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
using Reflex.Port.OriginalWorld;
#endif

namespace Reflex.Port.Content
{
    /// data.fpack (loaded by FUN_006d59a0 into DAT_00a7f668, relocated by FUN_006d6990 / FUN_006d6940): u32 count, u32 font
    /// table offset, u32 relocated; a font: u32 hash bits, hash table, u32 glyphs, glyph array (20 bytes: u32 code, core
    /// bitmap, outline bitmap (offsets from the font), u8 w, u8 h, i8 y offset, i8 pre, i8 post), +0x10 u32, +0x14 u32 line
    /// height, +0x18 u32 space advance. Bitmaps: 4 bits per texel (low nibble first), (w + 1) / 2 bytes a row.
    /// Font 0 (DAT_00a7f660): names, 0-9 A-Z, 30 high. Font 1 (DAT_00a7f664): the plate digits, 156 high.
    public sealed class ReflexFontPack
    {
        public sealed class Glyph { public int code, w, h, yoff, pre, post, core, outline; }
        public sealed class Font
        {
            public int lineHeight, space;
            public Glyph[] glyphs;
            readonly Dictionary<int, Glyph> byCode = new Dictionary<int, Glyph>();
            internal void Index() { foreach (var g in glyphs) if (!byCode.ContainsKey(g.code)) byCode[g.code] = g; }
            /// FUN_006d68c0: the glyph of a (signed char) code, else '*', else the first glyph.
            public Glyph Lookup(int code) => byCode.TryGetValue(code, out var g) ? g : code != 0x2a ? Lookup(0x2a) : glyphs[0];
        }
        public readonly byte[] data;
        public readonly Font[] fonts;

        public ReflexFontPack(byte[] b)
        {
            data = b;
            int U(int p) { if (p < 0 || p + 4 > b.Length) throw new InvalidDataException("data.fpack: truncated"); return BitConverter.ToInt32(b, p); }
            int n = U(0), table = U(4);
            if (n < 2 || n > 16) throw new InvalidDataException("data.fpack: " + n + " fonts");
            fonts = new Font[n];
            for (int i = 0; i < n; i++)
            {
                int fo = U(table + 4 * i), count = U(fo + 8), arr = fo + U(fo + 12);
                var f = new Font { lineHeight = U(fo + 0x14), space = U(fo + 0x18), glyphs = new Glyph[count] };
                for (int k = 0; k < count; k++)
                {
                    int o = arr + k * 20; U(o + 16);
                    f.glyphs[k] = new Glyph { code = U(o), core = fo + U(o + 4), outline = fo + U(o + 8), w = b[o + 12], h = b[o + 13], yoff = (sbyte)b[o + 14], pre = (sbyte)b[o + 15], post = (sbyte)b[o + 16] };
                }
                f.Index();
                fonts[i] = f;
            }
        }
    }

    /// The game's plate rasteriser (engine-free; levels are A8R8G8B8 texels, D3D row order: row 0 on top).
    public static class ReflexPlateRaster
    {
        sealed class Img { public int w, h; public float[] p; public Img(int w, int h) { this.w = w; this.h = h; p = new float[w * h * 4]; } }

        /// FUN_006d5a80: a string's width in font texels (unscaled).
        static int Measure(byte[] s, ReflexFontPack.Font f)
        {
            int w = 0;
            for (int i = 0; s[i] != 0; i++)
            {
                int c = (sbyte)s[i], next = (sbyte)s[i + 1];
                if (c == 0x20) { w += f.space; continue; }
                var g = f.Lookup(c);
                if (i != 0) w += g.pre;
                w += g.w;
                if (next != 0 && next != 0x20) w += g.post;
            }
            return w;
        }

        /// FUN_006d64c0: the box-filtered value of the glyph bitmap under destination texel (dx, dy); false when every source
        /// texel under it is 0.
        static bool Sample(byte[] d, int dx, int dy, float stepx, float stepy, ReflexFontPack.Glyph g, int bitmap, int stride, out float value)
        {
            float x0 = (float)((double)dx * stepx), y0 = (float)((double)dy * stepy);
            float wm1 = (float)((float)g.w - 1f), x1 = (float)(x0 + stepx); if (x1 > wm1) x1 = wm1;
            float hm1 = (float)((float)g.h - 1f), y1 = (float)(y0 + stepy); if (y1 > hm1) y1 = hm1;
            float fx0 = (float)(1f - (float)(x0 - (float)Math.Floor(x0))), fx1 = (float)(1f - (float)(x1 - (float)Math.Floor(x1)));
            float fy0 = (float)(1f - (float)(y0 - (float)Math.Floor(y0))), fy1 = (float)(1f - (float)(y1 - (float)Math.Floor(y1)));
            int ix0 = (int)x0, ix1 = (int)x1, iy0 = (int)y0, iy1 = (int)y1;
            float acc = 0, wsum = 0; bool found = false;
            for (int y = iy0; y < iy1; y++)
            {
                int row = bitmap + y * stride;
                for (int x = ix0; x < ix1; x++)
                {
                    int bt = d[row + x / 2], nib = (x & 1) != 0 ? bt >> 4 : bt & 15;
                    float v = (float)((float)nib / 15f);
                    if (!(v > 0f)) continue;
                    float val = v, wt = 1f;
                    if (y == iy0) { val = (float)(fy0 * v); wt = fy0; }
                    if (y == iy1 - 1) { val = (float)(fy1 * val); wt = (float)(wt * fy1); }
                    if (x == ix0) { val = (float)(fx0 * val); wt = (float)(wt * fx0); }
                    if (x == ix1 - 1) { wt = (float)(wt * fx1); val = (float)(fx1 * val); }
                    wsum = (float)(wt + wsum); acc = (float)(val + acc); found = true;
                }
            }
            value = !found ? 0f : wsum == acc ? 1f : (float)(acc / wsum);
            return found;
        }

        /// FUN_006d5af0 (pass 1: outline bitmap, rgb lerp to the colour, alpha max) / FUN_006d6000 (pass 2: core bitmap, rgb
        /// lerp, alpha kept). rect: pen x, y, box w, h; clip: x0, y0, x1, y1; flags: &3 0 centre / 2 right, &12 0 centre / 8 bottom.
        static void Draw(bool pass2, ReflexFontPack pack, int font, Img img, int[] rect, int[] clip, byte[] s, float col, int flags)
        {
            var f = pack.fonts[font]; byte[] d = pack.data;
            int W = Measure(s, f), H = f.lineHeight;
            double qy = (double)rect[3] / H;
            float sy = (float)qy;
            if (pass2 ? !(1.0 > qy) : !(1f > sy)) sy = 1f;
            float Wf = W;
            double qx = (double)rect[2] / W;
            float sx = (float)qx; double st;   // st: the x87 stack's scale (pass 1 reloads the float, pass 2 keeps the quotient)
            if (pass2 ? 1.0 > qx : 1f > sx) st = pass2 ? qx : sx; else { sx = 1f; st = 1.0; }
            int x = rect[0], y = rect[1];
            if ((flags & 3) == 0) x -= (int)(float)(Wf * sx) / 2; else if ((flags & 3) == 2) x -= (int)(float)(Wf * sx);
            if ((flags & 0xc) == 0) y -= (int)((uint)H >> 1); else if ((flags & 0xc) == 8) y -= H;
            if (x <= clip[0]) x = clip[0];
            if (y <= clip[1]) y = clip[1];
            int space = (int)((double)f.space * st);
            for (int i = 0; s[i] != 0; i++)
            {
                int c = (sbyte)s[i], next = (sbyte)s[i + 1];
                if (c == 0x20) { x += space; continue; }
                var g = f.Lookup(c);
                if (i != 0) x += (int)(float)((float)g.pre * sx);
                float hF = g.h, wF = g.w;
                int a = (int)(float)(hF * sy), dw = (int)(float)(wF * sx);
                float dwF = dw, k = (float)((float)(dwF / wF) * 1.5f); if (k > 1f) k = 1f;
                float dhF = (float)((float)a * k), stepx = (float)((float)(1f / dwF) * wF);
                int dh = (int)dhF;
                float stepy = (float)((float)(1f / (float)dh) * hF);
                if (dw >= clip[2] - x) dw = clip[2] - x;
                if (dh >= clip[3] - y) dh = clip[3] - y;
                int stride = (g.w + 1) / 2, bitmap = pass2 ? g.core : g.outline;
                int row0 = y - g.yoff - g.h + H; if (row0 < 0) row0 = 0;
                for (int r = 0; r < dh; r++)
                {
                    if (dw <= 0) continue;
                    int row = row0 + r;
                    for (int cx = 0; cx < dw; cx++)
                    {
                        if (!Sample(d, cx, r, stepx, stepy, g, bitmap, stride, out float v)) continue;
                        int px = x + cx;
                        if (px < 0 || row < 0 || px >= img.w || row >= img.h) continue;
                        int o = (row * img.w + px) * 4;
                        float one = (float)(1f - v), sc = (float)(v * col);
                        float pa = img.p[o + 3];
                        img.p[o] = (float)((float)(one * img.p[o]) + sc);
                        img.p[o + 1] = (float)((float)(one * img.p[o + 1]) + sc);
                        img.p[o + 2] = (float)((float)(one * img.p[o + 2]) + sc);
                        if (!pass2) img.p[o + 3] = pa > v ? pa : v;
                    }
                    st = sx;
                }
                x += (int)(long)Math.Truncate((double)g.w * st);
                if (next != 0 && next != 0x20) x += (int)(long)Math.Truncate((double)g.post * st);
            }
        }

        /// The mip chain and the A8R8G8B8 texels (FUN_005520d0 / FUN_00551110 after the draws).
        static byte[][] Levels(Img img)
        {
            int n = 0;
            for (int w0 = img.w, h0 = img.h; w0 > 0 && h0 > 0; w0 >>= 1, h0 >>= 1) n++;
            var levels = new byte[n][];
            float[] cur = img.p; int w = img.w, h = img.h;
            for (int L = 0; L < n; L++)
            {
                if (L > 0)
                {
                    int nh = h >> 1, nw = w >> 1; var nb = new float[nw * nh * 4];
                    if (nh - 1 > 1 && nw - 1 > 1)
                        for (int y = 1; y < nh - 1; y++)
                            for (int x = 1; x < nw - 1; x++)
                            {
                                int a = ((2 * y) * w + 2 * x) * 4, b = a + 4, c = ((2 * y + 1) * w + 2 * x) * 4, o = (y * nw + x) * 4;
                                for (int j = 0; j < 4; j++) nb[o + j] = (float)((float)((float)((float)(cur[a + j] + cur[b + j]) + cur[c + j]) + cur[b + j]) * 0.25f);
                            }
                    cur = nb; w = nw; h = nh;
                }
                var t = new byte[w * h * 4];
                for (int i = 0; i < w * h; i++)
                {
                    int r = Texel(cur[i * 4]), g = Texel(cur[i * 4 + 1]), b = Texel(cur[i * 4 + 2]), a = Texel(cur[i * 4 + 3]);
                    t[i * 4] = (byte)b; t[i * 4 + 1] = (byte)g; t[i * 4 + 2] = (byte)r; t[i * 4 + 3] = (byte)a;   // A8R8G8B8 in memory: B G R A
                }
                levels[L] = t;
            }
            return levels;
        }
        static int Texel(float v) { float c = v < 1f ? v : 1f; c = c > 0f ? c : 0f; return (int)(float)Math.Floor((float)(c * 255f)); }

        static byte[] Z(string s, int max) { var b = Encoding.ASCII.GetBytes(s ?? ""); int n = Math.Min(b.Length, max); var z = new byte[n + 2]; Array.Copy(b, z, n); return z; }
        static int Len(byte[] z) { int n = 0; while (z[n] != 0) n++; return n; }

        /// FUN_005520d0: the bike's number plate (MXPlayer + 0xb00). dark: colour 0.1 (else 0.9); hr: the garage's 256 x 256.
        /// outlined (not the original's: a mod's front plate, ReflexBikeModel.modPlateDark): the first pass (the glyphs' outline bitmap)
        /// in the other colour, as the name plates draw theirs, so the digits stay readable on a plate of both colours.
        public static byte[][] Bike(string number, bool dark, bool hr = false, bool outlined = false)
        {
            var s = Z(number, 31); int len = Len(s);
            float col = dark ? 0.1f : 0.9f, edge = outlined ? (dark ? 0.9f : 0.1f) : col;
            int size; int[] rect;
            if (hr) { size = 256; rect = new[] { 0x80, 0x82, 0xdc, 0xff }; }
            else { size = 128; rect = len == 2 ? new[] { 0x40, 0x68, 0x5a, 0x6e } : len == 3 ? new[] { 0x40, 0x68, 0x6e, 0x8c } : new[] { 0x40, 0x68, 0x32, 0x50 }; }
            var clip = new[] { 0, 0, size - 9, size - 9 };
            var img = new Img(size, size);
            Draw(false, Pack, 1, img, rect, clip, s, edge, 0);
            Draw(true, Pack, 1, img, rect, clip, s, col, 0);
            return Levels(img);
        }

        /// FUN_00551110: the rider's name over number plates. Returns (jersey = MXPlayer + 0xb24, chest protector + 0xb28): the
        /// text goes on the jersey when onJersey (else on the chest protector), the other stays clear. invert (the gear's
        /// InvertTextColors): outline 0.9 / core 0.1, else outline 0.1 / core 0.9.
        public static (byte[][] jersey, byte[][] chestProtector) Rider(string name, string number, bool onJersey, bool invert, bool hr = false)
        {
            var nm = Z(name, 0x1f); var num = Z(number, 31);
            int size = hr ? 256 : 128;
            int[] nameRect, numRect;
            if (hr) { nameRect = new[] { 0x80, 0x2d, 0xaa, 0x50 }; numRect = new[] { 0x80, 0x48, 0xb4, 0x8c }; }
            else
            {
                int nl = Len(nm) * 13; if (nl >= 0x55) nl = 0x55;
                int len = Len(num);
                nameRect = new[] { 0x40, 0x28, nl, 0x14 };
                numRect = len == 2 ? new[] { 0x40, 0x25, 0x5a, 0x50 } : len == 3 ? new[] { 0x40, 0x25, 0x6e, 0x5a } : new[] { 0x40, 0x25, 0x2d, 0x50 };
            }
            int[] nameClip = { 9, 0, size - 9, size }, numClip = { 0, 0, size - 9, size - 9 };
            float c1 = invert ? 0.9f : 0.1f, c2 = invert ? 0.1f : 0.9f;
            byte[][] Make(bool text)
            {
                var img = new Img(size, size);
                if (text)
                {
                    Draw(false, Pack, 1, img, numRect, numClip, num, c1, 4);
                    Draw(true, Pack, 1, img, numRect, numClip, num, c2, 4);
                    Draw(false, Pack, 0, img, nameRect, nameClip, nm, c1, 8);
                    Draw(true, Pack, 0, img, nameRect, nameClip, nm, c2, 8);
                }
                return Levels(img);
            }
            return (Make(onJersey), Make(!onJersey));
        }

        static ReflexFontPack pack;
        /// The install's data.fpack (set by the Unity side; engine-free callers assign it).
        public static ReflexFontPack Pack
        {
            get
            {
#if UNITY_5_3_OR_NEWER
                if (pack == null) pack = new ReflexFontPack(File.ReadAllBytes(ReflexInstall.PathOf("data.fpack")));
#endif
                return pack ?? throw new InvalidOperationException("data.fpack not loaded");
            }
            set => pack = value;
        }
    }

#if UNITY_5_3_OR_NEWER
    /// A rider's plate inputs as the running game holds them (read before the simulation thread owns the plugin):
    ///   P+0x200 name (15 chars), P+0x210 number (3); bike colour: the vehicle (G+0x154d8 list, entry+4 == P+0x1b0) def
    ///   (entry+8): VehicleType (+0xc) not MotocrossBike / Atv, or VehicleEngineClass (+0x10) not 125cc -> dark (FUN_0054bec0);
    ///   rider: the rider (G+0x15518 list, entry+4 == P+0x1b8): its ChestProtector gear (entry+0x20) with a model package
    ///   (its std::string size +0x30 > 0) takes the text, else the Jersey (entry+0x24); InvertTextColors = that gear's +0x44
    ///   (FUN_0054bf50; no entry: the jersey, inverted).
    public sealed class ReflexPlateText
    {
        public string name = "", number = "";
        public bool bikeDark = true, onJersey = true, invert = true;
        /// The player's HIDE NUMBER choices (GARAGE > RIDER > NAME & NUMBER): the bike's plates / the jersey or chest protector without the
        /// number (the jersey name stays). Only the player's own rider carries them (the garage previews, the local race rider); every other
        /// rider keeps the original's plates.
        public bool hideBikeNumber, hideRiderNumber;
        public string Key => name + "|" + number + "|" + bikeDark + onJersey + invert + (hideBikeNumber ? "|nobike" : "") + (hideRiderNumber ? "|norider" : "");
        public override string ToString() => $"'{name}' #{number} plate {(bikeDark ? "dark" : "light")} text on {(onJersey ? "jersey" : "chest protector")}{(invert ? " inverted" : "")}"
            + (hideBikeNumber ? ", bike number hidden" : "") + (hideRiderNumber ? ", gear number hidden" : "");

        /// The client save's HIDE NUMBER choices (off by default: the original shows the number everywhere).
        public const string HideBikeKey = "Garage.HideBikeNumber", HideRiderKey = "Garage.HideRiderNumber";
        public static bool HideBikeSetting { get => Reflex.Port.Options.ReflexSave.GetBool(HideBikeKey, false); set { Reflex.Port.Options.ReflexSave.SetBool(HideBikeKey, value); Reflex.Port.Options.ReflexSave.Save(); } }
        public static bool HideRiderSetting { get => Reflex.Port.Options.ReflexSave.GetBool(HideRiderKey, false); set { Reflex.Port.Options.ReflexSave.SetBool(HideRiderKey, value); Reflex.Port.Options.ReflexSave.Save(); } }
        /// The player's own plates: the HIDE NUMBER choices applied (bike: not on a pro's vehicle; gear: not on a pro's gear).
        public ReflexPlateText Own(bool bike = true, bool rider = true) { hideBikeNumber = bike && HideBikeSetting; hideRiderNumber = rider && HideRiderSetting; return this; }

        /// The plate flags as one number (tests): 1 dark bike plate, 2 text on the jersey, 4 inverted,
        /// 8 bike number hidden, 16 gear number hidden. WithFlags reads them back (without the new bits both numbers show).
        public int Flags => (bikeDark ? 1 : 0) | (onJersey ? 2 : 0) | (invert ? 4 : 0) | (hideBikeNumber ? 8 : 0) | (hideRiderNumber ? 16 : 0);
        public ReflexPlateText WithFlags(int flags)
        {
            bikeDark = (flags & 1) != 0; onJersey = (flags & 2) != 0; invert = (flags & 4) != 0; hideBikeNumber = (flags & 8) != 0; hideRiderNumber = (flags & 16) != 0;
            return this;
        }

        /// The plate text of a garage / menu preview (FUN_00761df0 and 0054bec0 / 0054bf50 for the model and gear shown): the rider's
        /// jersey name and number (the profile's PlayerRider Name / Number, or the pro rider's AIRider row when spec.plateRider names one),
        /// the plate colour of the bike's model and the gear that carries the text. The race takes them from the running core
        /// (ReflexRoster / rx_session_appearance); both follow the same rules.
        public static ReflexPlateText ForSpecs(BikeSpec bike, RiderSpec rider)
        {
            var t = new ReflexPlateText();
            string name = null, number = null;
            if (rider != null && rider.plateRider > 0)
            {
                var row = Find("AIRider", "AIRiderId", rider.plateRider);
                name = Rfx.Str(row, "Name"); number = Rfx.Str(row, "Number");
            }
            else
            {
                var table = ReflexProfile.Current?.T("PlayerRider");
                name = table?.Str("Name", 0); number = table?.Str("Number", 0);
            }
            t.name = Clip(name ?? "RIDER", 15); t.number = Clip(number ?? "998", 3);
            if (rider == null || rider.plateRider <= 0) t.Own();   // the player's own rider: the HIDE NUMBER choices (a pro's preview keeps his number)
            if (bike != null && bike.vehicleModelId >= 0)
            {   // 0054bec0: dark unless a motocross bike / ATV of the 125cc engine class
                var model = Find("VehicleModel", "VehicleModelId", bike.vehicleModelId);
                string type = Rfx.Str(Find("VehicleType", "VehicleTypeId", Rfx.Int(model, "VehicleTypeId", -1)), "FactoryName");
                string engine = Rfx.Str(Find("VehicleEngineClass", "VehicleEngineClassId", Rfx.Int(model, "VehicleEngineClassId", -1)), "FactoryName");
                bool light = (string.Equals(type, "MotocrossBike", StringComparison.OrdinalIgnoreCase) || string.Equals(type, "Atv", StringComparison.OrdinalIgnoreCase))
                             && string.Equals(engine, "125cc", StringComparison.OrdinalIgnoreCase);
                t.bikeDark = !light;
            }
            if (rider?.gearIds != null)
            {   // 0054bf50: the chest protector with a model carries the text, else the jersey; InvertTextColors of that gear
                Dictionary<string, object> cp = null, jersey = null;
                foreach (int id in rider.gearIds)
                {
                    var gear = Find("RiderGear", "RiderGearId", id);
                    string kind = Rfx.Str(Find("RiderGearType", "RiderGearTypeId", Rfx.Int(gear, "RiderGearTypeId", -1)), "FactoryName");
                    if (string.Equals(kind, "ChestProtector", StringComparison.OrdinalIgnoreCase)) cp = gear;
                    else if (string.Equals(kind, "Jersey", StringComparison.OrdinalIgnoreCase)) jersey = gear;
                }
                var source = jersey;
                if (cp != null && Rfx.Int(cp, "ModelPackageDatabaseId", -1) >= 0) { t.onJersey = false; source = cp; }
                if (source != null) t.invert = Rfx.Int(source, "InvertTextColors", 0) != 0;
            }
            return t;
        }
        static string Clip(string s, int n) => s.Length <= n ? s : s.Substring(0, n);
        static Dictionary<string, object> Find(string table, string column, int value)
        {
            if (KitPacks.IsKitId(value))
            {   // a gear / kit pack's piece (its renumbered row: InvertTextColors of a pack's jersey)
                var kit = KitPacks.Current.Rows(table);
                return kit?.FirstOrDefault(r => Rfx.Int(r, column, int.MinValue) == value);
            }
            if (!ReflexEvents.Tables.TryGetValue(table, out var rows)) return null;
            foreach (var r in rows) if (Rfx.Int(r, column, int.MinValue) == value) return r;
            return null;
        }

        public static ReflexPlateText Read(IntPtr world, int slot)
        {
            var b = new byte[16];
            uint U(uint a) => ReflexWorldNative.reflex_world_read(world, a, b, 4) == 0 ? BitConverter.ToUInt32(b, 0) : throw new InvalidOperationException($"guest read {a:x8}");
            string S(uint a) { var s = new byte[16]; if (ReflexWorldNative.reflex_world_read(world, a, s, 16) != 0) return ""; int n = Array.IndexOf(s, (byte)0); return Encoding.ASCII.GetString(s, 0, n < 0 ? 15 : Math.Min(n, 15)); }
            uint G = U(0xa946b8); if (G == 0) return null;
            uint P = G + 0x7460 + (uint)slot * 0x12a0;
            var t = new ReflexPlateText { name = S(P + 0x200), number = S(P + 0x210) };
            uint vid = U(P + 0x1b0), rid = U(P + 0x1b8);
            uint list = U(G + 0x154d8); int n = (int)U(G + 0x154dc);
            for (int k = 0; k < n && k < 64; k++)
            {
                uint e = U(list + 4 * (uint)k);
                if (e == 0 || U(e + 4) != vid) continue;
                uint def = U(e + 8); uint type = U(def + 0xc);
                t.bikeDark = type != 0 && type != 1 || U(def + 0x10) != 1;
                break;
            }
            list = U(G + 0x15518); n = (int)U(G + 0x1551c);
            for (int k = 0; k < n && k < 64; k++)
            {
                uint e = U(list + 4 * (uint)k);
                if (e == 0 || U(e + 4) != rid) continue;
                uint cp = U(e + 0x20), src = U(e + 0x24);
                if (cp != 0 && (int)U(cp + 0x30) > 0) { t.onJersey = false; src = cp; }
                if (src != 0) t.invert = U(src + 0x44) != 0;
                break;
            }
            return t;
        }
    }

    /// The plate textures of one bike or rider (destroyed with it).
    public sealed class ReflexPlateTextures : MonoBehaviour
    {
        public string key;
        public Texture2D bike, jersey, chestProtector;
        void OnDestroy() { foreach (var t in new[] { bike, jersey, chestProtector }) if (t) Destroy(t); }
    }

    /// Makes the run-time plates and binds them to the materials ReflexVehicleMaterials tagged (Tag = "base:" / "decal:" +
    /// the placeholder's name + ":" addressU ":" addressV ":" mip filter): "Numbers" (the plastics' usr_sampler_number decal) ->
    /// the bike plate; "Rider_NamNum_Plate" -> the jersey's; "Rider_CP_NamNum_Plate" -> the chest protector's.
    public static class ReflexPlates
    {
        public const string Tag = "ReflexPlate";

        public static Texture2D ToTexture(byte[][] levels, int size, string name)
        {
            var t = new Texture2D(size, size, TextureFormat.BGRA32, levels.Length, true) { name = name, hideFlags = HideFlags.DontSave };   // linear: sampled raw, as the original device
            for (int L = 0; L < levels.Length; L++)
            {
                int w = Math.Max(1, size >> L), row = w * 4; var src = levels[L]; var flipped = new byte[src.Length];
                for (int y = 0; y < w; y++) Buffer.BlockCopy(src, y * row, flipped, (w - 1 - y) * row, row);   // D3D row 0 on top
                t.SetPixelData(flipped, L);
            }
            t.wrapMode = TextureWrapMode.Clamp; t.filterMode = FilterMode.Trilinear;
            t.Apply(false, true);
            return t;
        }

        /// The plates for a bike and its rider (either may be null). text null: they stay clear. hr: the garage's high-resolution models take
        /// the 256 x 256 plates (the hr argument of FUN_005520d0 / FUN_00551110, MXGarage's HR flag); a race takes 128 x 128.
        public static void Apply(ReflexBikeModel bike, ReflexRiderModel rider, ReflexPlateText text, bool hr = false)
        {
            if (text == null) return;
            try
            {
                if (bike?.chassis) Bind(bike.chassis, text, true, hr, bike.modPlateDark);
                if (rider?.root) Bind(rider.root, text, false, hr);
            }
            catch (Exception e) { Debug.LogWarning("Reflex plates not made: " + e.Message); }
        }

        static void Bind(GameObject root, ReflexPlateText text, bool isBike, bool hr, bool? modDark = null)
        {
            if (!root.TryGetComponent<ReflexPlateTextures>(out var holder)) holder = root.AddComponent<ReflexPlateTextures>();
            string key = text.Key + (hr ? "|hr" : "") + (modDark is bool md ? (md ? "|mod-dark" : "|mod-light") : "");
            if (holder.key != key)
            {
                foreach (var old in new[] { holder.bike, holder.jersey, holder.chestProtector }) if (old) UnityEngine.Object.Destroy(old);
                holder.bike = holder.jersey = holder.chestProtector = null;
                int size = hr ? 256 : 128;
                string bikeNumber = text.hideBikeNumber ? "" : text.number, riderNumber = text.hideRiderNumber ? "" : text.number;   // HIDE NUMBER: a clear plate / no number under the name
                if (isBike && modDark is bool dark) holder.bike = ToTexture(ReflexPlateRaster.Bike(bikeNumber, dark, hr, true), size, "Numbers " + bikeNumber + " (mod plate)");
                else if (isBike) holder.bike = ToTexture(ReflexPlateRaster.Bike(bikeNumber, text.bikeDark, hr), size, "Numbers " + bikeNumber);
                else
                {
                    var (j, cp) = ReflexPlateRaster.Rider(text.name, riderNumber, text.onJersey, text.invert, hr);
                    holder.jersey = ToTexture(j, size, "Rider_NamNum_Plate " + text.name);
                    holder.chestProtector = ToTexture(cp, size, "Rider_CP_NamNum_Plate " + text.name);
                }
                holder.key = key;
            }
            foreach (var r in root.GetComponentsInChildren<Renderer>(true))
                foreach (var m in r.sharedMaterials)
                {
                    string tag = m ? m.GetTag(Tag, false, "") : "";
                    if (tag.Length == 0) continue;
                    var p = tag.Split(':');
                    string tex = p.Length > 1 ? p[1].ToLowerInvariant() : "";
                    var t = tex == "numbers" ? holder.bike : tex.Contains("cp_namnum") ? holder.chestProtector : tex.Contains("namnum") ? holder.jersey : null;
                    if (!t) continue;
                    if (p.Length > 4 && int.TryParse(p[2], out int u) && int.TryParse(p[3], out int v) && int.TryParse(p[4], out int mip))
                    {
                        TextureWrapMode Wrap(int a) => a == 3 ? TextureWrapMode.Clamp : a == 2 ? TextureWrapMode.Mirror : TextureWrapMode.Repeat;
                        t.wrapModeU = Wrap(u); t.wrapModeV = Wrap(v); t.filterMode = mip >= 2 ? FilterMode.Trilinear : FilterMode.Bilinear;
                    }
                    if (p[0].StartsWith("_Tex", StringComparison.Ordinal)) m.SetTexture(p[0], t);   // Reflex/Vehicle: the plate's sampler (ReflexObjectShading.Build)
                    else if (p[0] == "decal") { m.SetTexture("_DecalMap", t); m.SetFloat("_DecalOn", 1); }
                    else m.SetTexture("_BaseMap", t);
                }
        }
    }
#endif
}
