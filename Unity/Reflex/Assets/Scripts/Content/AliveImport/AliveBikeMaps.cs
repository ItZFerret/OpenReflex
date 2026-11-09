// The maps of an imported Alive bike (AliveBikes.cs): a converted PC texture resource (Textures.Convert: the 0x77ddcc00 v4 layout, DXT1 /
// DXT3 / DXT5 blocks or 32-bit BGRA rows, top row first) decoded to RGBA and written as a PNG. Its own deflate (fixed Huffman codes,
// greedy LZ77 over a 32 KiB window) instead of the runtime's DeflateStream: the same texture gives the same bytes on every machine and
// runtime, so a .rxmod folder made from the same install has the same content (RxModBikes' content hash).
using System;
using System.Collections.Generic;
using System.IO;

namespace Reflex.Port.Content.AliveImport
{
    internal static class AliveBikeMaps
    {
        const int Dxt1 = 0x31545844, Dxt3 = 0x33545844, Dxt5 = 0x35545844;

        /// The base level of a converted texture as RGBA rows (top first), or null for a format this does not read.
        public static (int w, int h, byte[] rgba)? Decode(byte[] b)
        {
            if (b == null || b.Length < 56 || Bin.U32LE(b, 0) != 0x77ddcc00 || Bin.U32LE(b, 4) != 4) return null;
            int levels = (int)Bin.U32LE(b, 40), w = (int)Bin.U32LE(b, 44), h = (int)Bin.U32LE(b, 48), fmt = (int)Bin.U32LE(b, 52);
            if (levels < 1 || levels > 15 || w < 1 || h < 1 || w > 8192 || h > 8192) return null;
            int size = (int)Bin.U32LE(b, 56), p = 56 + levels * 8;
            if (p + size > b.Length) return null;
            var o = new byte[w * h * 4];
            if (fmt == Dxt1 || fmt == Dxt3 || fmt == Dxt5)
            {
                int bpb = fmt == Dxt1 ? 8 : 16, bw = (w + 3) / 4, bh = (h + 3) / 4;
                if (size < bw * bh * bpb) return null;
                var block = new byte[64];
                for (int by = 0; by < bh; by++)
                    for (int bx = 0; bx < bw; bx++)
                    {
                        int at = p + (by * bw + bx) * bpb;
                        Block(b, at, fmt, block);
                        for (int y = 0; y < 4; y++)
                            for (int x = 0; x < 4; x++)
                            {
                                int px = bx * 4 + x, py = by * 4 + y; if (px >= w || py >= h) continue;
                                Buffer.BlockCopy(block, (y * 4 + x) * 4, o, (py * w + px) * 4, 4);
                            }
                    }
                return (w, h, o);
            }
            if (fmt == 21 || fmt == 22)   // A8R8G8B8 / X8R8G8B8: B G R A in memory
            {
                if (size < w * h * 4) return null;
                for (int i = 0; i < w * h; i++)
                {
                    o[i * 4] = b[p + i * 4 + 2]; o[i * 4 + 1] = b[p + i * 4 + 1]; o[i * 4 + 2] = b[p + i * 4]; o[i * 4 + 3] = fmt == 22 ? (byte)255 : b[p + i * 4 + 3];
                }
                return (w, h, o);
            }
            return null;
        }

        static void Colour(byte[] b, int at, bool four, byte[] o)
        {
            int c0 = b[at] | b[at + 1] << 8, c1 = b[at + 2] | b[at + 3] << 8;
            var pal = new int[4, 4];
            void Rgb(int c, int k) { pal[k, 0] = (c >> 11 & 31) * 255 / 31; pal[k, 1] = (c >> 5 & 63) * 255 / 63; pal[k, 2] = (c & 31) * 255 / 31; pal[k, 3] = 255; }
            Rgb(c0, 0); Rgb(c1, 1);
            bool opaque = four || c0 > c1;
            for (int ch = 0; ch < 3; ch++)
            {
                pal[2, ch] = opaque ? (2 * pal[0, ch] + pal[1, ch]) / 3 : (pal[0, ch] + pal[1, ch]) / 2;
                pal[3, ch] = opaque ? (pal[0, ch] + 2 * pal[1, ch]) / 3 : 0;
            }
            pal[2, 3] = 255; pal[3, 3] = opaque ? 255 : 0;
            uint bits = Bin.U32LE(b, at + 4);
            for (int i = 0; i < 16; i++) { int k = (int)(bits >> (2 * i) & 3); for (int ch = 0; ch < 4; ch++) o[i * 4 + ch] = (byte)pal[k, ch]; }
        }

        static void Block(byte[] b, int at, int fmt, byte[] o)
        {
            if (fmt == Dxt1) { Colour(b, at, false, o); return; }
            Colour(b, at + 8, true, o);
            if (fmt == Dxt3) { for (int i = 0; i < 16; i++) { int v = b[at + i / 2] >> (4 * (i & 1)) & 15; o[i * 4 + 3] = (byte)(v * 17); } return; }
            int a0 = b[at], a1 = b[at + 1];
            var a = new int[8]; a[0] = a0; a[1] = a1;
            if (a0 > a1) for (int k = 1; k < 7; k++) a[k + 1] = ((7 - k) * a0 + k * a1) / 7;
            else { for (int k = 1; k < 5; k++) a[k + 1] = ((5 - k) * a0 + k * a1) / 5; a[6] = 0; a[7] = 255; }
            ulong bits = 0; for (int k = 0; k < 6; k++) bits |= (ulong)b[at + 2 + k] << (8 * k);
            for (int i = 0; i < 16; i++) o[i * 4 + 3] = (byte)a[(int)(bits >> (3 * i) & 7)];
        }

        /// RGBA pixels as a PNG (RGB when every pixel is opaque).
        public static byte[] Png(int w, int h, byte[] px)
        {
            bool alpha = false; for (int i = 3; i < px.Length; i += 4) if (px[i] != 255) { alpha = true; break; }
            return Encode(w, h, px, alpha);
        }

        /// The pixel of `src` (sw x sh) at the place of (x, y) of a w x h image (nearest).
        static int At(int x, int y, int w, int h, int sw, int sh) => ((int)((long)y * sh / h) * sw + (int)((long)x * sw / w)) * 4;
        static byte B(float v) => (byte)Math.Max(0, Math.Min(255, (int)Math.Round(v * 255f)));

        /// Alive's dynamic-colour plastics (Template_MX_ATV_DynPlastic_FNL) baked for a .rxmod plastic: where the diffuse's alpha is set the
        /// program draws usr_userdyncolor (in place of the grey diffuse; 0.4 + 0.6 (N.V)^1.5 of it: the facing value is baked), where the
        /// combo map's R is set it blends to the number plate's usr_dynnumplate; the rest is the diffuse. Opaque.
        public static (int w, int h, byte[] px) BakePlastic((int w, int h, byte[] px) d, (int w, int h, byte[] px)? combo, float[] dyn, float[] plate)
        {
            var o = new byte[d.w * d.h * 4];
            for (int y = 0; y < d.h; y++)
                for (int x = 0; x < d.w; x++)
                {
                    int i = (y * d.w + x) * 4; float a = d.px[i + 3] / 255f, r = 0;
                    if (combo is var c && c != null) r = c.Value.px[At(x, y, d.w, d.h, c.Value.w, c.Value.h)] / 255f;
                    for (int k = 0; k < 3; k++)
                    {
                        float v = d.px[i + k] / 255f;
                        if (dyn != null) v += (dyn[k] - v) * a;
                        if (plate != null) v += (plate[k] - v) * r;
                        o[i + k] = B(v);
                    }
                    o[i + 3] = 255;
                }
            return (d.w, d.h, o);
        }

        /// Alive's rims / hubs (Template_MX_Wheel_Fnl: no diffuse; combo R the shading, B where usr_mxrimdyncolor x 0.92 tints, the hub
        /// usr_mxhubdyncolor) as a diffuse for the .rxmod wheel program.
        public static (int w, int h, byte[] px) BakeWheel((int w, int h, byte[] px) c, float[] colour, bool hub)
        {
            var o = new byte[c.w * c.h * 4];
            for (int i = 0; i < c.w * c.h; i++)
            {
                float r = c.px[i * 4] / 255f, b = c.px[i * 4 + 2] / 255f;
                for (int k = 0; k < 3; k++)
                {
                    float col = colour != null ? colour[k] : 0.6f;
                    o[i * 4 + k] = B(hub ? r * col : r * (1f + b * (col * 0.92f - 1f)));
                }
                o[i * 4 + 3] = 255;
            }
            return (c.w, c.h, o);
        }

        /// A combo map's R (the shading) times a colour: the diffuse of Alive's ATV programs that colour at run time (frames, rims, springs).
        public static (int w, int h, byte[] px) BakeTint((int w, int h, byte[] px) c, float[] colour)
        {
            var o = new byte[c.w * c.h * 4];
            for (int i = 0; i < c.w * c.h; i++)
            {
                float r = c.px[i * 4] / 255f;
                for (int k = 0; k < 3; k++) o[i * 4 + k] = B(r * (colour != null ? colour[k] : 0.6f));
                o[i * 4 + 3] = 255;
            }
            return (c.w, c.h, o);
        }

        /// A combo / SFX map's channels for a Reflex program: r, g, b each picked from the source's (0 R, 1 G, 2 B, -1 zero).
        public static (int w, int h, byte[] px) Channels((int w, int h, byte[] px) c, int r, int g, int b)
        {
            var o = new byte[c.px.Length];
            for (int i = 0; i < c.w * c.h; i++)
            {
                o[i * 4] = r < 0 ? (byte)0 : c.px[i * 4 + r]; o[i * 4 + 1] = g < 0 ? (byte)0 : c.px[i * 4 + g]; o[i * 4 + 2] = b < 0 ? (byte)0 : c.px[i * 4 + b];
                o[i * 4 + 3] = 255;
            }
            return (c.w, c.h, o);
        }

        // ------------------------------------------------------------------------------------- PNG

        public static byte[] Encode(int w, int h, byte[] rgba, bool alpha)
        {
            int ch = alpha ? 4 : 3, stride = w * ch;
            // rows with the Up filter (2) for every row but the first (None): predictable, and good on the maps' flat areas
            var raw = new byte[(stride + 1) * h];
            var prev = new byte[stride]; var row = new byte[stride];
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++) for (int c = 0; c < ch; c++) row[x * ch + c] = rgba[(y * w + x) * 4 + c];
                int at = y * (stride + 1);
                raw[at] = (byte)(y == 0 ? 0 : 2);
                for (int i = 0; i < stride; i++) raw[at + 1 + i] = (byte)(y == 0 ? row[i] : row[i] - prev[i]);
                var t = prev; prev = row; row = t;
            }
            var ms = new MemoryStream();
            ms.Write(new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A }, 0, 8);
            var ihdr = new byte[13];
            Bin.PutU32BE(ihdr, 0, (uint)w); Bin.PutU32BE(ihdr, 4, (uint)h); ihdr[8] = 8; ihdr[9] = (byte)(alpha ? 6 : 2);
            Chunk(ms, "IHDR", ihdr);
            Chunk(ms, "IDAT", Zlib(raw));
            Chunk(ms, "IEND", Array.Empty<byte>());
            return ms.ToArray();
        }

        static void Chunk(Stream s, string type, byte[] data)
        {
            var hdr = new byte[8]; Bin.PutU32BE(hdr, 0, (uint)data.Length);
            for (int i = 0; i < 4; i++) hdr[4 + i] = (byte)type[i];
            s.Write(hdr, 0, 8); s.Write(data, 0, data.Length);
            uint crc = Crc(Crc(0xffffffffu, hdr, 4, 4), data, 0, data.Length) ^ 0xffffffffu;
            var c = new byte[4]; Bin.PutU32BE(c, 0, crc); s.Write(c, 0, 4);
        }

        static uint[] crcTable;
        static uint Crc(uint c, byte[] b, int start, int n)
        {
            if (crcTable == null)
            {
                var t = new uint[256];
                for (uint i = 0; i < 256; i++) { uint v = i; for (int k = 0; k < 8; k++) v = (v & 1) != 0 ? 0xedb88320u ^ (v >> 1) : v >> 1; t[i] = v; }
                crcTable = t;
            }
            for (int i = start; i < start + n; i++) c = crcTable[(c ^ b[i]) & 0xff] ^ (c >> 8);
            return c;
        }

        // ------------------------------------------------------------------------------------- deflate (RFC 1951, fixed Huffman)

        sealed class Bits
        {
            public readonly MemoryStream o = new MemoryStream(); uint acc; int n;
            public void Put(uint v, int count) { acc |= v << n; n += count; while (n >= 8) { o.WriteByte((byte)acc); acc >>= 8; n -= 8; } }
            /// A Huffman code (stored most significant bit first).
            public void Code(uint code, int len) { uint r = 0; for (int i = 0; i < len; i++) r |= ((code >> i) & 1) << (len - 1 - i); Put(r, len); }
            public void Flush() { if (n > 0) { o.WriteByte((byte)acc); acc = 0; n = 0; } }
        }

        static void Literal(Bits b, int v)
        {
            if (v < 144) b.Code((uint)(0x30 + v), 8);
            else if (v < 256) b.Code((uint)(0x190 + v - 144), 9);
            else if (v < 280) b.Code((uint)(v - 256), 7);
            else b.Code((uint)(0xc0 + v - 280), 8);
        }

        static readonly int[] LenBase = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        static readonly int[] LenExtra = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        static readonly int[] DistBase = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
        static readonly int[] DistExtra = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

        static void Match(Bits b, int len, int dist)
        {
            int li = 28; while (LenBase[li] > len) li--;
            Literal(b, 257 + li); if (LenExtra[li] > 0) b.Put((uint)(len - LenBase[li]), LenExtra[li]);
            int di = 29; while (DistBase[di] > dist) di--;
            b.Code((uint)di, 5); if (DistExtra[di] > 0) b.Put((uint)(dist - DistBase[di]), DistExtra[di]);
        }

        public static byte[] Zlib(byte[] data)
        {
            var b = new Bits();
            b.o.WriteByte(0x78); b.o.WriteByte(0x01);
            const int Window = 32768, MaxChain = 48, HashBits = 15;
            var head = new int[1 << HashBits]; var prevPos = new int[Window];
            for (int i = 0; i < head.Length; i++) head[i] = -1;
            int Hash(int i) => ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & ((1 << HashBits) - 1);
            void Insert(int i) { if (i + 2 >= data.Length) return; int hsh = Hash(i); prevPos[i & (Window - 1)] = head[hsh]; head[hsh] = i; }
            b.Put(1, 1); b.Put(1, 2);   // one final block, fixed Huffman codes
            int pos = 0;
            while (pos < data.Length)
            {
                int bestLen = 0, bestDist = 0;
                if (pos + 2 < data.Length)
                {
                    int cand = head[Hash(pos)], chain = 0, max = Math.Min(258, data.Length - pos);
                    while (cand >= 0 && pos - cand <= Window - 1 && chain++ < MaxChain)
                    {
                        if (data[cand + bestLen] == data[pos + bestLen])
                        {
                            int l = 0; while (l < max && data[cand + l] == data[pos + l]) l++;
                            if (l > bestLen) { bestLen = l; bestDist = pos - cand; if (l == max) break; }
                        }
                        int next = prevPos[cand & (Window - 1)];
                        if (next >= cand) break;
                        cand = next;
                    }
                }
                if (bestLen >= 3)
                {
                    Match(b, bestLen, bestDist);
                    for (int k = 0; k < bestLen; k++) Insert(pos + k);
                    pos += bestLen;
                }
                else { Literal(b, data[pos]); Insert(pos); pos++; }
            }
            Literal(b, 256);
            b.Flush();
            var a = new byte[4]; Bin.PutU32BE(a, 0, Bin.Adler32(data, 0, data.Length)); b.o.Write(a, 0, 4);
            return b.o.ToArray();
        }
    }
}
