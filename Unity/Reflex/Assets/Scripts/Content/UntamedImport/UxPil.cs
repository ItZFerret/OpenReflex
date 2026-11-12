// MX vs ATV Untamed converter, foundation: the two Pillow operations the reference's outputs depend on, bit for bit (checked against
// Pillow 13.0.0, tests/untamed_convert/foundation):
//   Resize    Image.resize(size, BOX | BILINEAR | LANCZOS) for 8-bit L / RGB / RGBA images: libImaging/Resample.c, two passes (horizontal,
//             then vertical) with 22-bit fixed-point coefficients; an RGBA image is resized premultiplied (RGBa) and converted back, as
//             Image.resize does. Only the coefficients go through floating point (sin for LANCZOS).
//   BcEncode  rtex.bc_encode(img, DXT1 | DXT5): Pillow's DDS writer (libImaging/BcnEncode.c): BC1 colour blocks from the block's smallest
//             and largest RGB565 value, BC3 alpha blocks from the smallest and largest alpha.
// Images are UxImage: (h, w, c) uint8 rows, c = 1 (L), 3 (RGB) or 4 (RGBA).
using System;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxPil
    {
        public enum Filter { Box, Bilinear, Lanczos }
        const int PrecisionBits = 32 - 8 - 2;

        static double Sinc(double x) { if (x == 0.0) return 1.0; x = x * Math.PI; return Math.Sin(x) / x; }
        static double Weight(Filter f, double x)
        {
            switch (f)
            {
                case Filter.Box: return x > -0.5 && x <= 0.5 ? 1.0 : 0.0;
                case Filter.Bilinear: if (x < 0.0) x = -x; return x < 1.0 ? 1.0 - x : 0.0;
                default: return -3.0 <= x && x < 3.0 ? Sinc(x) * Sinc(x / 3) : 0.0;        // truncated sinc
            }
        }
        static double Support(Filter f) => f == Filter.Box ? 0.5 : f == Filter.Bilinear ? 1.0 : 3.0;

        /// precompute_coeffs + normalize_coeffs_8bpc for the whole input (box = the full image): per output element the first input
        /// element and its integer weights.
        static void Coeffs(int inSize, int outSize, Filter filter, out int[] xmin, out int[][] kk)
        {
            double scale = (double)inSize / outSize, filterscale = scale < 1.0 ? 1.0 : scale;
            double support = Support(filter) * filterscale;
            xmin = new int[outSize]; kk = new int[outSize][];
            for (int xx = 0; xx < outSize; xx++)
            {
                double center = (xx + 0.5) * scale, ss = 1.0 / filterscale, ww = 0.0;
                int x0 = (int)(center - support + 0.5); if (x0 < 0) x0 = 0;
                int x1 = (int)(center + support + 0.5); if (x1 > inSize) x1 = inSize;
                int n = x1 - x0; if (n < 0) n = 0;
                var k = new double[n];
                for (int x = 0; x < n; x++) { double w = Weight(filter, (x + x0 - center + 0.5) * ss); k[x] = w; ww += w; }
                var ki = new int[n];
                for (int x = 0; x < n; x++)
                {
                    double v = ww != 0.0 ? k[x] / ww : k[x];
                    ki[x] = v < 0 ? (int)(-0.5 + v * (1 << PrecisionBits)) : (int)(0.5 + v * (1 << PrecisionBits));
                }
                xmin[xx] = x0; kk[xx] = ki;
            }
        }
        static byte Clip8(int v) { v >>= PrecisionBits; return (byte)(v < 0 ? 0 : v > 255 ? 255 : v); }

        static UxImage ResizeRaw(UxImage im, int ow, int oh, Filter filter)
        {
            int h = im.h, w = im.w, c = im.c; byte[] cur = im.data; int cw = w, ch = h;
            void Horizontal()
            {
                Coeffs(w, ow, filter, out var xmin, out var kk);
                var tmp = new byte[ch * ow * c];
                for (int y = 0; y < ch; y++)
                    for (int xx = 0; xx < ow; xx++)
                    {
                        var k = kk[xx]; int x0 = xmin[xx];
                        for (int b = 0; b < c; b++)
                        {
                            int ss = 1 << (PrecisionBits - 1);
                            for (int x = 0; x < k.Length; x++) ss += cur[(y * w + x0 + x) * c + b] * k[x];
                            tmp[(y * ow + xx) * c + b] = Clip8(ss);
                        }
                    }
                cur = tmp; cw = ow;
            }
            void Vertical()
            {
                Coeffs(h, oh, filter, out var ymin, out var kk);
                var tmp = new byte[oh * cw * c];
                for (int yy = 0; yy < oh; yy++)
                {
                    var k = kk[yy]; int y0 = ymin[yy];
                    for (int x = 0; x < cw; x++)
                        for (int b = 0; b < c; b++)
                        {
                            int ss = 1 << (PrecisionBits - 1);
                            for (int y = 0; y < k.Length; y++) ss += cur[((y0 + y) * cw + x) * c + b] * k[y];
                            tmp[(yy * cw + x) * c + b] = Clip8(ss);
                        }
                }
                cur = tmp; ch = oh;
            }
            // Pillow 13.0.0 runs the vertical pass first when it shrinks the image and removes more than twice as many rows as the
            // horizontal pass removes columns (measured against Image.resize: 5,400 sizes, every filter and mode; the intermediate
            // image is 8-bit, so the order decides bytes). Otherwise horizontal, then vertical.
            bool verticalFirst = ow != w && oh < h && (h - oh) > 2 * (w - ow);
            if (verticalFirst) Vertical();
            if (ow != w) Horizontal();
            if (!verticalFirst && oh != h) Vertical();
            if (ReferenceEquals(cur, im.data)) cur = (byte[])cur.Clone();
            return new UxImage(cur, oh, ow, c);
        }

        /// Image.resize((ow, oh), filter) of an L, RGB or RGBA image (RGBA: premultiplied, resized, converted back). The same size
        /// gives a copy.
        public static UxImage Resize(UxImage im, int ow, int oh, Filter filter)
        {
            UxBin.Require(im.c == 1 || im.c == 3 || im.c == 4, "resize: " + im.c + " channels");
            UxBin.Require(ow > 0 && oh > 0 && im.w > 0 && im.h > 0, "resize: empty image");
            if (ow == im.w && oh == im.h) return new UxImage((byte[])im.data.Clone(), im.h, im.w, im.c);
            if (im.c != 4) return ResizeRaw(im, ow, oh, filter);
            // RGBA -> RGBa: MULDIV255(colour, alpha)
            var pre = new byte[im.data.Length];
            for (int i = 0; i < pre.Length; i += 4)
            {
                int a = im.data[i + 3];
                for (int b = 0; b < 3; b++) { int t = im.data[i + b] * a + 128; pre[i + b] = (byte)(((t >> 8) + t) >> 8); }
                pre[i + 3] = (byte)a;
            }
            var o = ResizeRaw(new UxImage(pre, im.h, im.w, 4), ow, oh, filter);
            // RGBa -> RGBA: CLIP8(255 * colour / alpha) unless alpha is 0 or 255
            for (int i = 0; i < o.data.Length; i += 4)
            {
                int a = o.data[i + 3];
                if (a == 255 || a == 0) continue;
                for (int b = 0; b < 3; b++) { int v = 255 * o.data[i + b] / a; o.data[i + b] = (byte)(v > 255 ? 255 : v); }
            }
            return o;
        }

        // ---------------------------------------------------------------- BCn
        static int Encode565(int r, int g, int b) => (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
        static void Decode565(int x, int[] c)
        {
            int r = (x & 0xf800) >> 8; r |= r >> 5;
            int g = (x & 0x7e0) >> 3; g |= g >> 6;
            int b = (x & 0x1f) << 3; b |= b >> 5;
            c[0] = r; c[1] = g; c[2] = b;
        }

        /// encode_bc1_color (no punch-through alpha): 8 bytes at dst.
        static void Bc1Color(UxImage im, int x0, int y0, byte[] dst, int at)
        {
            var block = new int[48]; bool first = true; int cmin = 0, cmax = 0;
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                {
                    int x = x0 + i, y = y0 + j, o = (i + j * 4) * 3;
                    if (x >= im.w || y >= im.h) continue;                 // the block reaches past the image's edge: zeros
                    int p = (y * im.w + x) * im.c;
                    block[o] = im.data[p]; block[o + 1] = im.data[p + (im.c == 1 ? 0 : 1)]; block[o + 2] = im.data[p + (im.c == 1 ? 0 : 2)];
                    int color = Encode565(block[o], block[o + 1], block[o + 2]);
                    if (first || color < cmin) cmin = color;
                    if (first || color > cmax) cmax = color;
                    first = false;
                }
            dst[at] = (byte)cmax; dst[at + 1] = (byte)(cmax >> 8); dst[at + 2] = (byte)cmin; dst[at + 3] = (byte)(cmin >> 8);
            var mn = new int[3]; var mx = new int[3];
            Decode565(cmin, mn); Decode565(cmax, mx);
            for (int i = 0; i < 4; i++)
            {
                int l = 0;
                for (int j = 3; j > -1; j--)
                {
                    int o = (i * 4 + j) * 3;
                    float distance = 0; int total = 0;
                    for (int k = 0; k < 3; k++)
                    {
                        float denom = Math.Abs(mx[k] - mn[k]);
                        if (denom != 0) { distance = (float)(distance + (float)(Math.Abs(block[o + k] - mn[k]) / denom)); total += 1; }
                    }
                    if (total == 0) continue;
                    distance = (float)(distance * (6 / total));
                    if (distance < 1) l |= 1 << (j * 2);              // colour 1 (the smaller)
                    else if (distance < 3) l |= 3 << (j * 2);         // 1/3 colour 0 + 2/3 colour 1
                    else if (distance < 5) l |= 2 << (j * 2);         // 2/3 colour 0 + 1/3 colour 1
                }
                dst[at + 4 + i] = (byte)l;
            }
        }

        /// encode_bc3_alpha of channel `ch`: 8 bytes at dst.
        static void Bc3Alpha(UxImage im, int x0, int y0, int ch, byte[] dst, int at)
        {
            var block = new int[16]; bool first = true; int amin = 0, amax = 0;
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                {
                    int x = x0 + i, y = y0 + j;
                    if (x >= im.w || y >= im.h) continue;
                    int a = im.data[(y * im.w + x) * im.c + ch];
                    block[i + j * 4] = a;
                    if (first || a < amin) amin = a;
                    if (first || a > amax) amax = a;
                    first = false;
                }
            dst[at] = (byte)amin; dst[at + 1] = (byte)amax;
            float denom = Math.Abs(amax - amin);
            for (int i = 0; i < 2; i++)
            {
                uint l = 0;
                for (int j = 7; j > -1; j--)
                {
                    int a = block[i * 8 + j];
                    if (a == 0) { l |= 6u << (j * 3); continue; }
                    if (a == 255) { l |= 7u << (j * 3); continue; }
                    float distance = denom == 0 ? 0 : (float)((float)(Math.Abs(a - amin) / denom) * 10);
                    if (distance < 3) l |= 2u << (j * 3);
                    else if (distance < 5) l |= 3u << (j * 3);
                    else if (distance < 7) l |= 4u << (j * 3);
                    else l |= 5u << (j * 3);
                }
                dst[at + 2 + 3 * i] = (byte)l; dst[at + 3 + 3 * i] = (byte)(l >> 8); dst[at + 4 + 3 * i] = (byte)(l >> 16);
            }
        }

        /// rtex.bc_encode(img, fmt): DXT1 (dxt5 = false: the image as RGB) or DXT5 (the image as RGBA; an image without alpha gets 255)
        /// blocks of one image, ((w + 3) / 4) * ((h + 3) / 4) blocks; an image narrower or lower than 4 is first resized to 4 with BOX.
        public static byte[] BcEncode(UxImage img, bool dxt5)
        {
            if (img.w < 4 || img.h < 4) img = Resize(img, Math.Max(4, img.w), Math.Max(4, img.h), Filter.Box);
            int bw = (img.w + 3) / 4, bh = (img.h + 3) / 4, bs = dxt5 ? 16 : 8;
            var o = new byte[bw * bh * bs];
            for (int by = 0; by < bh; by++)
                for (int bx = 0; bx < bw; bx++)
                {
                    int at = (by * bw + bx) * bs;
                    if (dxt5)
                    {
                        if (img.c == 4) Bc3Alpha(img, bx * 4, by * 4, 3, o, at); else EncodeOpaqueAlpha(img, bx * 4, by * 4, o, at);
                        at += 8;
                    }
                    Bc1Color(img, bx * 4, by * 4, o, at);
                }
            return o;
        }
        /// The alpha block of an image without an alpha channel after convert('RGBA'): alpha 255 inside the image, 0 past its edge.
        static void EncodeOpaqueAlpha(UxImage img, int x0, int y0, byte[] dst, int at)
        {
            var rgba = new byte[16 * 4]; int w = Math.Min(4, img.w - x0), h = Math.Min(4, img.h - y0);
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) rgba[(j * w + i) * 4 + 3] = 255;
            Bc3Alpha(new UxImage(rgba, h, w, 4), 0, 0, 3, dst, at);
        }
    }
}
