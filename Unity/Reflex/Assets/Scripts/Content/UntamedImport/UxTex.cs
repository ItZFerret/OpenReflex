// MX vs ATV Untamed converter, foundation: the port of untamed/tools/uxtex.py (Untamed .texture version 6, big-endian, in Xbox 360 GPU
// memory layout -> raw levels, RGBA, Reflex PC .texture version 4) and of the reference's shared PC texture module (mw2-to-reflex
// scripts/rtex.py: Tex, build, hdr_size; its Pillow encoder bc_encode is not here). Images are flat row-major byte arrays.
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Reflex.Port.Content.UntamedImport
{
    /// One surface record of an Untamed texture: mips, w, h, fmt and the xfmt() fields.
    public sealed class UxTexSurface
    {
        public int mips, w, h; public uint fmt;
        public int gpufmt, endian, tiled, numfmt; public int[] signs, swizzle; public string name;
    }

    /// A raw level: (hb, wb, bpb) uint8 elements in PC byte order.
    public sealed class UxBlocks
    {
        public byte[] data; public int hb, wb, bpb;
        public UxBlocks(byte[] data, int hb, int wb, int bpb) { this.data = data; this.hb = hb; this.wb = wb; this.bpb = bpb; }
    }

    /// A decoded image: (h, w, c) uint8.
    public sealed class UxImage
    {
        public byte[] data; public int h, w, c;
        public UxImage(byte[] data, int h, int w, int c) { this.data = data; this.h = h; this.w = w; this.c = c; }
        /// img[:lh, :lw] (contiguous copy).
        public UxImage Crop(int lh, int lw)
        {
            lh = Math.Min(lh, h); lw = Math.Min(lw, w);
            if (lh == h && lw == w) return this;
            var o = new byte[lh * lw * c];
            for (int y = 0; y < lh; y++) Buffer.BlockCopy(data, y * w * c, o, y * lw * c, lw * c);
            return new UxImage(o, lh, lw, c);
        }
    }

    public static class UxTex
    {
        public const uint MAGIC = 0x77DDCC00;

        /// GPUFMT[gpufmt]: (name, block w, block h, bytes per block / element); name null = unknown.
        public static (string name, int bw, int bh, int bpb) GpuFmt(int f)
        {
            switch (f)
            {
                case 2: return ("8", 1, 1, 1); case 4: return ("5_6_5", 1, 1, 2); case 6: return ("8_8_8_8", 1, 1, 4); case 10: return ("8_8", 1, 1, 2);
                case 15: return ("4_4_4_4", 1, 1, 2); case 18: return ("DXT1", 4, 4, 8); case 19: return ("DXT2_3", 4, 4, 16); case 20: return ("DXT4_5", 4, 4, 16);
                case 24: return ("16", 1, 1, 2); case 25: return ("16_16", 1, 1, 4); case 26: return ("16_16_16_16", 1, 1, 8); case 29: return ("16_16_16_16_EXPAND", 1, 1, 8);
                case 30: return ("16_FLOAT", 1, 1, 2); case 31: return ("16_16_FLOAT", 1, 1, 4); case 32: return ("16_16_16_16_FLOAT", 1, 1, 8);
                case 36: return ("32_FLOAT", 1, 1, 4); case 49: return ("DXN", 4, 4, 16); case 59: return ("DXT5A", 4, 4, 8);
            }
            return (null, 0, 0, 0);
        }
        /// KNOWN_D3DFMT.get(f).
        public static string KnownD3dFmt(uint f)
        {
            switch (f)
            {
                case 0x1A200152: return "DXT1"; case 0x1A200154: return "DXT5"; case 0x1A200153: return "DXT3"; case 0x1A200171: return "DXN";
                case 0x1A200052: return "LIN_DXT1"; case 0x1A200054: return "LIN_DXT5"; case 0x18280186: return "A8R8G8B8"; case 0x18280086: return "LIN_A8R8G8B8";
                case 0x1A22AB5D: return "A16B16G16R16F_EXPAND"; case 0x1A22AB60: return "A16B16G16R16F";
            }
            return null;
        }
        // PC formats (the 4 bytes at 0x34 of a PC texture record)
        public static readonly byte[] PC_DXT1 = { (byte)'D', (byte)'X', (byte)'T', (byte)'1' }, PC_DXT5 = { (byte)'D', (byte)'X', (byte)'T', (byte)'5' };
        public static readonly byte[] PC_A8R8G8B8 = { 0x15, 0, 0, 0 }, PC_A16B16G16R16F = { 0x71, 0, 0, 0 }, PC_G16R16F = { 0x70, 0, 0, 0 }, PC_R32F = { 0x72, 0, 0, 0 };
        public static bool Fmt(byte[] a, byte[] b) => a != null && b != null && a.Length == 4 && b.Length == 4 && a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
        /// fmt.decode('latin-1').strip(chr(0)): 'DXT1', or the D3DFMT code's characters.
        public static string FmtText(byte[] fmt) => UxBin.Latin1(fmt, 0, 4).Trim('\0');

        /// xfmt(f): a Xenos D3DFORMAT split into fields.
        public static UxTexSurface Xfmt(uint f)
        {
            var s = new UxTexSurface { fmt = f, gpufmt = (int)(f & 63), endian = (int)((f >> 6) & 3), tiled = (int)((f >> 8) & 1), numfmt = (int)((f >> 17) & 1) };
            s.signs = new[] { (int)((f >> 9) & 3), (int)((f >> 11) & 3), (int)((f >> 13) & 3), (int)((f >> 15) & 3) };
            s.swizzle = new[] { (int)((f >> 18) & 7), (int)((f >> 21) & 7), (int)((f >> 24) & 7), (int)((f >> 27) & 7) };
            s.name = KnownD3dFmt(f) ?? "fmt" + (f & 63).ToString(CultureInfo.InvariantCulture);
            return s;
        }

        static int Align(int x, int a) => (x + a - 1) / a * a;
        /// _log2_ceil(x) = max(0, (x - 1).bit_length()).
        public static int Log2Ceil(int x) { int v = x - 1, n = 0; while (v > 0) { n++; v >>= 1; } return n; }

        // ---------------------------------------------------------------- Xenos tiling
        /// tiled_to_xy(width_aligned, height_aligned, bpb) for element index i: the (x, y) it represents (XGAddress2DTiledX/Y).
        public static void TiledToXy(long i, int widthAligned, int bpb, out long x, out long y)
        {
            int lb = (bpb >> 2) + ((bpb >> 1) >> (bpb >> 2));
            long aw = (widthAligned + 31) & ~31;
            long ob = i << lb;
            long ot = ((ob & ~4095L) >> 3) + ((ob & 1792) >> 2) + (ob & 63);
            long om = ot >> (7 + lb);
            long macroX = (om % (aw >> 5)) << 2;
            long tileX = (((ot >> (5 + lb)) & 2) + (ob >> 6)) & 3;
            x = ((macroX + tileX) << 3) + (((((ot >> 1) & ~15L) + (ot & 15)) & ((bpb << 3) - 1)) >> lb);
            long macroY = (om / (aw >> 5)) << 2;
            long tileY = ((ot >> (6 + lb)) & 1) + ((ob & 2048) >> 10);
            y = (((macroY + tileY) << 3) + ((((ot & (((bpb << 6) - 1) & ~31L)) + ((ot & 15) << 1)) >> (3 + lb)) & ~1L) + ((ot & 16) >> 4));
        }

        /// tiled_offset(x, y, aligned_w, log2bpp): element (x, y) -> element index in the tiled surface (XGAddress2DTiledOffset).
        public static long TiledOffset(long x, long y, long alignedW, int log2bpp)
        {
            long macro = ((x >> 5) + (y >> 5) * (alignedW >> 5)) << (log2bpp + 7);
            long micro = ((x & 7) + ((y & 0xE) << 2)) << log2bpp;
            long off = macro + ((micro & ~0xFL) << 1) + (micro & 0xF) + ((y & 1) << 4);
            return (((off & ~0x1FFL) << 3) + ((y & 16) << 7) + ((off & 0x1C0) << 2) + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (off & 0x3F)) >> log2bpp;
        }

        /// untile_region(raw, wa, ha, bpb, tiled): the bytes of one region (wa*ha*bpb) -> (ha, wa, bpb) in row-major order.
        public static byte[] UntileRegion(byte[] raw, int start, int wa, int ha, int bpb, bool tiled = true)
        {
            int n = wa * ha;
            UxBin.Require(start >= 0 && (long)start + (long)n * bpb <= raw.Length, "texture region outside the data");
            var o = new byte[n * bpb];
            if (!tiled) { Buffer.BlockCopy(raw, start, o, 0, n * bpb); return o; }
            for (int i = 0; i < n; i++)
            {
                TiledToXy(i, wa, bpb, out long x, out long y);
                if (x < wa && y < ha) Buffer.BlockCopy(raw, start + i * bpb, o, (int)(y * wa + x) * bpb, bpb);
            }
            return o;
        }

        /// tile_region(lin, tiled): the inverse of untile_region (round-trip tests).
        public static byte[] TileRegion(byte[] lin, int wa, int ha, int bpb, bool tiled = true)
        {
            if (!tiled) return (byte[])lin.Clone();
            int n = wa * ha; var o = new byte[n * bpb];
            for (int i = 0; i < n; i++)
            {
                TiledToXy(i, wa, bpb, out long x, out long y);
                UxBin.Require(x < wa && y < ha, "tile_region: element outside the region");
                Buffer.BlockCopy(lin, (int)(y * wa + x) * bpb, o, i * bpb, bpb);
            }
            return o;
        }

        /// endian_swap(a, endian) in place: 1 = bytes swapped in 16-bit words, 2 = in 32-bit words, 3 = 16-bit words swapped in 32-bit words.
        public static void EndianSwap(byte[] a, int endian)
        {
            if (endian == 0) return;
            if (endian == 1) { UxBin.Require(a.Length % 2 == 0, "endian swap size"); for (int i = 0; i < a.Length; i += 2) { byte t = a[i]; a[i] = a[i + 1]; a[i + 1] = t; } return; }
            UxBin.Require(a.Length % 4 == 0, "endian swap size");
            if (endian == 2) { for (int i = 0; i < a.Length; i += 4) { byte t = a[i]; a[i] = a[i + 3]; a[i + 3] = t; t = a[i + 1]; a[i + 1] = a[i + 2]; a[i + 2] = t; } return; }
            if (endian == 3) { for (int i = 0; i < a.Length; i += 4) { byte t = a[i]; a[i] = a[i + 2]; a[i + 2] = t; t = a[i + 1]; a[i + 1] = a[i + 3]; a[i + 3] = t; } return; }
            throw new UxException("endian " + endian);
        }

        /// packed_mip_offset(width, height, mip, bw, bh): the element (block) offset of `mip` inside the packed mip tail; false when the
        /// mip is not packed (Python: None).
        public static bool PackedMipOffset(int width, int height, int mip, int bw, int bh, out int x, out int y)
        {
            x = y = 0;
            int lw = Log2Ceil(width), lh = Log2Ceil(height), ls = Math.Min(lw, lh);
            if (ls > 4 + mip) return false;
            int baseMip = ls > 4 ? ls - 4 : 0, pm = mip - baseMip;
            if (pm < 3) { if (lw > lh) { x = 0; y = 16 >> pm; } else { x = 16 >> pm; y = 0; } }
            else { if (lw > lh) { x = (1 << (lw - baseMip)) >> (pm - 2); y = 0; } else { x = 0; y = (1 << (lh - baseMip)) >> (pm - 2); } }
            x /= bw; y /= bh;
            return true;
        }
        /// first_packed_mip(width, height).
        public static int FirstPackedMip(int width, int height) { int ls = Math.Min(Log2Ceil(width), Log2Ceil(height)); return ls > 4 ? ls - 4 : 0; }

        // ---------------------------------------------------------------- block decoders
        static void C565(int c, out int r, out int g, out int b)
        {
            r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31;
            r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
        }
        /// _bc1_colors of one block at blk[p..p+8) into px (16 pixels x 4, R G B A).
        static void Bc1Block(byte[] blk, int p, bool force4, byte[] px, int o)
        {
            int c0 = blk[p] | blk[p + 1] << 8, c1 = blk[p + 2] | blk[p + 3] << 8;
            uint idx = (uint)(blk[p + 4] | blk[p + 5] << 8 | blk[p + 6] << 16 | blk[p + 7] << 24);
            C565(c0, out int r0, out int g0, out int b0); C565(c1, out int r1, out int g1, out int b1);
            bool four = c0 > c1 || force4;
            int[] pal =
            {
                r0, g0, b0, 255, r1, g1, b1, 255,
                four ? (2 * r0 + r1) / 3 : (r0 + r1) / 2, four ? (2 * g0 + g1) / 3 : (g0 + g1) / 2, four ? (2 * b0 + b1) / 3 : (b0 + b1) / 2, 255,
                four ? (r0 + 2 * r1) / 3 : 0, four ? (g0 + 2 * g1) / 3 : 0, four ? (b0 + 2 * b1) / 3 : 0, four ? 255 : 0,
            };
            for (int k = 0; k < 16; k++)
            {
                int s = (int)((idx >> (2 * k)) & 3);
                px[o + 4 * k] = (byte)pal[4 * s]; px[o + 4 * k + 1] = (byte)pal[4 * s + 1]; px[o + 4 * k + 2] = (byte)pal[4 * s + 2]; px[o + 4 * k + 3] = (byte)pal[4 * s + 3];
            }
        }
        /// _bc4 of one block at blk[p..p+8): 16 values into px[o + k * step].
        static void Bc4Block(byte[] blk, int p, byte[] px, int o, int step)
        {
            int a0 = blk[p], a1 = blk[p + 1];
            ulong bits = 0; for (int k = 0; k < 6; k++) bits |= (ulong)blk[p + 2 + k] << (8 * k);
            var pal = new int[8]; pal[0] = a0; pal[1] = a1;
            if (a0 > a1) for (int k = 1; k < 7; k++) pal[1 + k] = ((7 - k) * a0 + k * a1) / 7;
            else { for (int k = 1; k < 5; k++) pal[1 + k] = ((5 - k) * a0 + k * a1) / 5; pal[6] = 0; pal[7] = 255; }
            for (int k = 0; k < 16; k++) px[o + k * step] = (byte)pal[(int)((bits >> (3 * k)) & 7)];
        }

        /// decode_blocks(blocks, name): (hb, wb, bpb) blocks in PC byte order -> image (hb*4, wb*4, 4) uint8 (DXN: R, G, 0, 255 raw).
        public static UxImage DecodeBlocks(UxBlocks blocks, string name)
        {
            int hb = blocks.hb, wb = blocks.wb, bpb = blocks.bpb;
            int need = name == "DXT1" || name == "DXT5A" ? 8 : 16;
            if (name != "DXT1" && name != "DXT4_5" && name != "DXT2_3" && name != "DXN" && name != "DXT5A") throw new UxException(name);
            UxBin.Require(bpb >= need, name + ": blocks of " + bpb + " bytes");
            var img = new byte[hb * 4 * wb * 4 * 4]; var px = new byte[64];
            int W = wb * 4;
            for (int by = 0; by < hb; by++)
                for (int bx = 0; bx < wb; bx++)
                {
                    int p = (by * wb + bx) * bpb;
                    switch (name)
                    {
                        case "DXT1": Bc1Block(blocks.data, p, false, px, 0); break;
                        case "DXT4_5": Bc1Block(blocks.data, p + 8, true, px, 0); Bc4Block(blocks.data, p, px, 3, 4); break;
                        case "DXT2_3":
                            Bc1Block(blocks.data, p + 8, true, px, 0);
                            for (int j = 0; j < 8; j++) { px[4 * (2 * j) + 3] = (byte)((blocks.data[p + j] & 15) * 17); px[4 * (2 * j + 1) + 3] = (byte)(((blocks.data[p + j] >> 4) & 15) * 17); }
                            break;
                        case "DXN":
                            Bc4Block(blocks.data, p, px, 0, 4); Bc4Block(blocks.data, p + 8, px, 1, 4);
                            for (int k = 0; k < 16; k++) { px[4 * k + 2] = 0; px[4 * k + 3] = 255; }
                            break;
                        default:    // DXT5A
                            Bc4Block(blocks.data, p, px, 0, 4);
                            for (int k = 0; k < 16; k++) { px[4 * k + 1] = px[4 * k]; px[4 * k + 2] = px[4 * k]; px[4 * k + 3] = 255; }
                            break;
                    }
                    for (int py = 0; py < 4; py++) Buffer.BlockCopy(px, py * 16, img, ((by * 4 + py) * W + bx * 4) * 4, 16);
                }
            return new UxImage(img, hb * 4, wb * 4, 4);
        }

        /// half_to_float(a): little-endian float16 values -> float32.
        public static float[] HalfToFloat(byte[] a, int start, int count)
        {
            var f = new float[count];
            for (int i = 0; i < count; i++) f[i] = UxNp.HalfToFloat(UxBin.U16LE(a, start + 2 * i));
            return f;
        }

        // ---------------------------------------------------------------- PC textures
        /// pc_pitch(fmt, w).
        public static int PcPitch(byte[] fmt, int w)
        {
            if (Fmt(fmt, PC_DXT1)) return Math.Max(1, (w + 3) / 4) * 8;
            if (Fmt(fmt, PC_DXT5)) return Math.Max(1, (w + 3) / 4) * 16;
            if (Fmt(fmt, PC_A16B16G16R16F)) return w * 8;
            if (Fmt(fmt, PC_G16R16F) || Fmt(fmt, PC_A8R8G8B8) || Fmt(fmt, PC_R32F)) return w * 4;
            throw new UxException("pc_pitch: format " + UxBin.Hex(fmt));
        }

        /// build_pc(recs, table, payload, kind, flags): a Reflex PC v4 texture: header (LE) + table + payload; the data starts right
        /// after the mip table. recs: (mips, w, h, fmt); table: (size, pitch) per entry.
        public static byte[] BuildPc(IList<(int mips, int w, int h, byte[] fmt)> recs, IList<(int size, int pitch)> table, IList<byte[]> payload, int kind = 0, int flags = 0)
        {
            int n = recs.Count, hdrEnd = 0x28 + 16 * n + 8 * table.Count;
            long total = hdrEnd; foreach (var p in payload) total += p.Length;
            UxBin.Require(total <= int.MaxValue - 64, "texture too large");
            var o = new byte[total];
            UxBin.PutU32(o, 0, MAGIC); UxBin.PutU32(o, 4, 4); UxBin.PutU32(o, 8, 0); UxBin.PutU32(o, 12, (uint)n); UxBin.PutU32(o, 16, (uint)table.Count);
            UxBin.PutU32(o, 20, (uint)kind); UxBin.PutU32(o, 24, (uint)flags);
            for (int i = 0; i < n; i++)
            {
                UxBin.PutU32(o, 0x28 + 16 * i, (uint)recs[i].mips); UxBin.PutU32(o, 0x2C + 16 * i, (uint)recs[i].w); UxBin.PutU32(o, 0x30 + 16 * i, (uint)recs[i].h);
                UxBin.Require(recs[i].fmt.Length == 4, "texture format bytes");
                Buffer.BlockCopy(recs[i].fmt, 0, o, 0x34 + 16 * i, 4);
            }
            for (int i = 0; i < table.Count; i++) { UxBin.PutU32(o, 0x28 + 16 * n + 8 * i, (uint)table[i].size); UxBin.PutU32(o, 0x28 + 16 * n + 8 * i + 4, (uint)table[i].pitch); }
            int at = hdrEnd;
            foreach (var p in payload) { Buffer.BlockCopy(p, 0, o, at, p.Length); at += p.Length; }
            return o;
        }

        /// header(data): the facts of surface 0 (version, frames, total, mips, w, h, fmt, pitch, data_off, kind).
        public static Reflex.Port.Content.AliveImport.JObj Header(byte[] data)
        {
            var t = new UxTexture(data); var s = t.surfaces[0];
            return new Reflex.Port.Content.AliveImport.JObj().Set("version", (long)t.version).Set("frames", (long)t.nsurf).Set("total", (long)t.nentry).Set("mips", (long)s.mips)
                .Set("w", (long)s.w).Set("h", (long)s.h).Set("fmt", (long)s.fmt).Set("pitch", t.table.Length > 0 ? (long)t.table[0].pitch : 0L).Set("data_off", (long)t.data_offset).Set("kind", (long)t.kind);
        }

        // ---------------------------------------------------------------- rtex.py
        public static readonly byte[] R32F = PC_R32F, DXT1 = PC_DXT1, DXT5 = PC_DXT5, A8R8G8B8 = PC_A8R8G8B8;
        /// rtex.hdr_size(mc) = max(0x80, 0x38 + mc * 8). NB: this data-offset rule is wrong below 10 mips; use PcTexture / BuildPc.
        public static int RtexHdrSize(int mc) => Math.Max(0x80, 0x38 + mc * 8);
        /// rtex._pitch(fmt, w).
        public static int RtexPitch(byte[] fmt, int w) => Fmt(fmt, DXT1) ? Math.Max(1, (w + 3) / 4) * 8 : Fmt(fmt, DXT5) ? Math.Max(1, (w + 3) / 4) * 16 : w * 4;
        /// rtex.build(template_header, fmt, w, h, mips): the first 0x38 bytes of a template header reused, the mip payloads after
        /// max(0x80, 0x38 + 8 * mips).
        public static byte[] RtexBuild(byte[] templateHeader, byte[] fmt, int w, int h, IList<byte[]> mips)
        {
            int mc = mips.Count, hs = RtexHdrSize(mc); long total = hs; foreach (var m in mips) total += m.Length;
            var o = new byte[total];
            Buffer.BlockCopy(templateHeader, 0, o, 0, Math.Min(0x38, templateHeader.Length));
            UxBin.PutU32(o, 0x10, (uint)mc); UxBin.PutU32(o, 0x28, (uint)mc); UxBin.PutU32(o, 0x2C, (uint)w); UxBin.PutU32(o, 0x30, (uint)h);
            Buffer.BlockCopy(fmt, 0, o, 0x34, 4);
            int at = hs;
            for (int i = 0; i < mc; i++)
            {
                UxBin.PutU32(o, 0x38 + i * 8, (uint)mips[i].Length); UxBin.PutU32(o, 0x38 + i * 8 + 4, (uint)RtexPitch(fmt, Math.Max(1, w >> i)));
                Buffer.BlockCopy(mips[i], 0, o, at, mips[i].Length); at += mips[i].Length;
            }
            return o;
        }
        /// rtex.make_r32f(template_header, arr, mips, point): an R32F texture of a (h, w) float32 grid with a point-sampled (or box
        /// averaged) mip chain; mips = 0 means int(log2(max(w, h))) + 1.
        public static byte[] RtexMakeR32F(byte[] templateHeader, float[] arr, int h, int w, int mips = 0, bool point = true)
        {
            int n = mips != 0 ? mips : Log2Floor(Math.Max(w, h)) + 1;
            var o = new List<byte[]>(); float[] cur = arr; int ch = h, cw = w;
            for (int i = 0; i < n; i++)
            {
                var b = new byte[cur.Length * 4]; for (int k = 0; k < cur.Length; k++) UxBin.PutF32(b, 4 * k, cur[k]);
                o.Add(b);
                if (i + 1 >= n) break;
                int nh, nw; float[] next;
                if (point)
                {
                    nh = (ch + 1) / 2; nw = (cw + 1) / 2; next = new float[nh * nw];
                    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) next[y * nw + x] = cur[2 * y * cw + 2 * x];
                }
                else throw new UxException("make_r32f: only point sampling is ported (the averaged chain goes through a multi-axis float32 mean)");
                cur = next; ch = nh; cw = nw;
            }
            return RtexBuild(templateHeader, R32F, w, h, o);
        }
        static int Log2Floor(int v) { int n = 0; while (v > 1) { v >>= 1; n++; } return n; }
    }

    /// rtex.Tex: a single-surface PC texture read with rtex's data-offset rule (max(0x80, 0x38 + 8 * mips)).
    public sealed class UxRtex
    {
        public readonly byte[] raw; public readonly int mc, w, h; public readonly byte[] fmt; public readonly List<byte[]> mips = new List<byte[]>(); public readonly int end;
        public UxRtex(byte[] data)
        {
            raw = data;
            UxBin.Require(data.Length >= 0x38, "texture truncated");
            mc = (int)UxBin.U32LE(data, 0x28); w = (int)UxBin.U32LE(data, 0x2C); h = (int)UxBin.U32LE(data, 0x30);
            fmt = UxBin.Slice(data, 0x34, 4);
            int p = UxTex.RtexHdrSize(mc);
            for (int i = 0; i < mc; i++)
            {
                UxBin.Require(0x38 + i * 8 + 8 <= data.Length, "texture truncated");
                int sz = (int)UxBin.U32LE(data, 0x38 + i * 8);
                mips.Add(UxBin.PySlice(data, p, p + sz)); p += sz;
            }
            end = p;
        }
        /// mip0_float(): the (h, w) float32 grid of an R32F texture.
        public float[] Mip0Float()
        {
            UxBin.Require(UxTex.Fmt(fmt, UxTex.R32F) && mips.Count > 0 && mips[0].Length >= w * h * 4, "not an R32F texture");
            var f = new float[w * h]; for (int i = 0; i < f.Length; i++) f[i] = UxBin.F32LE(mips[0], 4 * i);
            return f;
        }
    }

    /// uxtex.PcTexture: a minimal Reflex PC v4 reader (any nsurf / cube).
    public sealed class UxPcTexture
    {
        public readonly byte[] raw; public readonly int version, nsurf, nentry, kind, flags, data_offset, end;
        /// recs[i] = (mips, w, h, fmt bytes).
        public readonly List<(int mips, int w, int h, byte[] fmt)> recs = new List<(int, int, int, byte[])>();
        public readonly List<(int size, int pitch)> table = new List<(int, int)>();
        public readonly List<byte[]> entries = new List<byte[]>();
        public UxPcTexture(byte[] data)
        {
            raw = data;
            UxBin.Require(data.Length >= 0x28, "texture truncated");
            uint magic = UxBin.U32LE(data, 0);
            UxBin.Require(magic == UxTex.MAGIC, "0x" + magic.ToString("x"));
            version = (int)UxBin.U32LE(data, 4); nsurf = (int)UxBin.U32LE(data, 12); nentry = (int)UxBin.U32LE(data, 16); kind = (int)UxBin.U32LE(data, 20); flags = (int)UxBin.U32LE(data, 24);
            UxBin.Require(nsurf >= 0 && nentry >= 0 && 0x28 + 16L * nsurf + 8L * nentry <= data.Length, "texture tables outside the file");
            for (int i = 0; i < nsurf; i++) recs.Add(((int)UxBin.U32LE(data, 0x28 + 16 * i), (int)UxBin.U32LE(data, 0x2C + 16 * i), (int)UxBin.U32LE(data, 0x30 + 16 * i), UxBin.Slice(data, 0x34 + 16 * i, 4)));
            int tp = 0x28 + 16 * nsurf;
            for (int i = 0; i < nentry; i++) table.Add(((int)UxBin.U32LE(data, tp + 8 * i), (int)UxBin.U32LE(data, tp + 8 * i + 4)));
            int p = tp + 8 * nentry;
            data_offset = p;
            foreach (var (sz, _) in table) { entries.Add(UxBin.PySlice(data, p, p + sz)); p += sz; }
            end = p;
        }
    }

    /// uxtex.UxTexture.
    public sealed class UxTexture
    {
        public readonly byte[] raw;
        public readonly int version, nsurf, nentry, kind, flags, u08, data_offset, faces;
        public readonly List<UxTexSurface> surfaces = new List<UxTexSurface>();
        public readonly (uint a, uint pitch)[] table;
        /// level_loc[(s, f, m)] = (region offset, wa, ha, x0, y0, wb, hb).
        public readonly Dictionary<(int s, int f, int m), (int off, int wa, int ha, int x0, int y0, int wb, int hb)> level_loc = new Dictionary<(int, int, int), (int, int, int, int, int, int, int)>();
        /// The keys of level_loc in insertion order (Python dict order).
        public readonly List<(int s, int f, int m)> level_keys = new List<(int, int, int)>();
        public readonly List<(int start, int end)> surface_span = new List<(int, int)>();
        public int data_end;
        readonly Dictionary<(int, int, int), byte[]> _rcache = new Dictionary<(int, int, int), byte[]>();

        public UxTexture(byte[] data)
        {
            raw = data;
            UxBin.Require(data.Length >= 0x28, "not a texture (truncated)");
            uint magic = UxBin.U32BE(data, 0);
            if (magic != UxTex.MAGIC) throw new UxException("not a texture (magic " + magic.ToString("x8") + ")");
            version = (int)UxBin.U32BE(data, 4); u08 = (int)UxBin.U32BE(data, 8); nsurf = (int)UxBin.U32BE(data, 12); nentry = (int)UxBin.U32BE(data, 16);
            kind = (int)UxBin.U32BE(data, 20); flags = (int)UxBin.U32BE(data, 24);
            UxBin.Require(nsurf >= 0 && nentry >= 0 && 0x28 + 16L * nsurf + 8L * nentry <= data.Length, "texture tables outside the file");
            for (int s = 0; s < nsurf; s++)
            {
                var sf = UxTex.Xfmt(UxBin.U32BE(data, 0x28 + 16 * s + 12));
                sf.mips = (int)UxBin.U32BE(data, 0x28 + 16 * s); sf.w = (int)UxBin.U32BE(data, 0x28 + 16 * s + 4); sf.h = (int)UxBin.U32BE(data, 0x28 + 16 * s + 8);
                surfaces.Add(sf);
            }
            int tp = 0x28 + 16 * nsurf;
            table = new (uint, uint)[nentry];
            for (int i = 0; i < nentry; i++) table[i] = (UxBin.U32BE(data, tp + 8 * i), UxBin.U32BE(data, tp + 8 * i + 4));
            data_offset = tp + 8 * nentry;
            faces = kind == 10 ? 6 : 1;
            Layout();
        }

        /// _elem(s): (name, bw, bh, bpb) of the surface's GPUTEXTUREFORMAT.
        public (string name, int bw, int bh, int bpb) Elem(int s)
        {
            var e = UxTex.GpuFmt(surfaces[s].gpufmt);
            if (e.name == null) throw new UxException("GPUTEXTUREFORMAT " + surfaces[s].gpufmt + " (" + surfaces[s].fmt.ToString("x8") + ")");
            return e;
        }

        static int AlignUp(int x, int a) => (x + a - 1) / a * a;

        void Layout()
        {
            int p = data_offset;
            for (int s = 0; s < surfaces.Count; s++)
            {
                var sf = surfaces[s]; var (name, bw, bh, bpb) = Elem(s);
                int w = sf.w, h = sf.h, mips = Math.Max(1, sf.mips);
                int pw = 1 << UxTex.Log2Ceil(w), ph = 1 << UxTex.Log2Ceil(h);
                int fp = mips > 1 ? UxTex.FirstPackedMip(w, h) : 1;          // a single-level texture is never packed
                int start = p;
                int nreg = Math.Min(fp, mips) + (fp < mips ? 1 : 0);
                var regs = new (int wa, int ha)[nreg];
                for (int r = 0; r < nreg; r++)
                {
                    int sw = r == 0 ? w : Math.Max(1, pw >> r), sh = r == 0 ? h : Math.Max(1, ph >> r);
                    int swb = (sw + bw - 1) / bw, shb = (sh + bh - 1) / bh;
                    int wa = sf.tiled != 0 ? AlignUp(swb, 32) : AlignUp(AlignUp(swb, 32) * bpb, 256) / bpb;
                    regs[r] = (wa, AlignUp(shb, 32));
                }
                for (int r = 0; r < nreg; r++)          // level-major: [level r of faces 0..5] ...
                    for (int f = 0; f < faces; f++)
                    {
                        var (wa, ha) = regs[r];
                        if (r < fp)
                        {
                            int lw = Math.Max(1, w >> r), lh = Math.Max(1, h >> r);
                            Put((s, f, r), (p, wa, ha, 0, 0, (lw + bw - 1) / bw, (lh + bh - 1) / bh));
                        }
                        else
                            for (int mm = fp; mm < mips; mm++)
                            {
                                int mw = Math.Max(1, w >> mm), mh = Math.Max(1, h >> mm);
                                if (!UxTex.PackedMipOffset(w, h, mm, bw, bh, out int x0, out int y0)) throw new UxException("texture layout: mip " + mm + " is not in the packed tail");
                                Put((s, f, mm), (p, wa, ha, x0, y0, (mw + bw - 1) / bw, (mh + bh - 1) / bh));
                            }
                        long next = (long)p + (long)wa * ha * bpb;
                        UxBin.Require(next <= int.MaxValue - 64, "texture layout too large");
                        p = (int)next;
                    }
                surface_span.Add((start, p));
            }
            data_end = p;
        }
        void Put((int, int, int) key, (int, int, int, int, int, int, int) loc) { if (!level_loc.ContainsKey(key)) level_keys.Add(key); level_loc[key] = loc; }

        /// check_size(): "exact", "short-tail" (the file ends inside the last region but after every level's last element), or null (False).
        public string CheckSize()
        {
            if (data_end == raw.Length) return "exact";
            if (data_end > raw.Length)
            {
                long need = 0;
                foreach (var key in level_keys)
                {
                    var (off, wa, ha, x0, y0, wb, hb) = level_loc[key];
                    int bpb = Elem(key.s).bpb;
                    need = Math.Max(need, off + ((long)(y0 + hb - 1) * wa + x0 + wb) * bpb);
                }
                if (need <= raw.Length) return "short-tail";
            }
            return null;
        }

        byte[] Region(int s, int off, int wa, int ha)
        {
            var key = (off, wa, ha);
            lock (_rcache) if (_rcache.TryGetValue(key, out var hit)) return hit;
            var sf = surfaces[s]; int bpb = Elem(s).bpb;
            long size = (long)wa * ha * bpb;
            UxBin.Require(size <= int.MaxValue - 64, "texture region too large");
            byte[] chunk = raw; int start = off;
            if (off + size > raw.Length)
            {   // a region the file ends inside: zero padded
                chunk = new byte[size]; start = 0;
                if (off < raw.Length) Buffer.BlockCopy(raw, off, chunk, 0, raw.Length - off);
            }
            var reg = UxTex.UntileRegion(chunk, start, wa, ha, bpb, sf.tiled != 0);
            UxTex.EndianSwap(reg, sf.endian);
            lock (_rcache) _rcache[key] = reg;
            return reg;
        }
        /// Drops the untiled regions kept for level_blocks (they are as large as the texture).
        public void ClearCache() { lock (_rcache) _rcache.Clear(); }

        /// level_blocks(s, f, m): the untiled, endian-swapped elements of one level, (hb, wb, bpb) uint8 in PC byte order.
        public UxBlocks LevelBlocks(int s = 0, int f = 0, int m = 0)
        {
            int bpb = Elem(s).bpb;
            if (!level_loc.TryGetValue((s, f, m), out var loc)) throw new UxException("texture has no level (" + s + ", " + f + ", " + m + ")");
            var (off, wa, ha, x0, y0, wb, hb) = loc;
            var reg = Region(s, off, wa, ha);
            int rows = Math.Max(0, Math.Min(hb, ha - y0)), cols = Math.Max(0, Math.Min(wb, wa - x0));     // numpy slicing clamps
            var o = new byte[rows * cols * bpb];
            for (int y = 0; y < rows; y++) Buffer.BlockCopy(reg, ((y0 + y) * wa + x0) * bpb, o, y * cols * bpb, cols * bpb);
            return new UxBlocks(o, rows, cols, bpb);
        }

        /// rgba(s, f, m, nm_preview, exposure): the decoded level as uint8 RGBA (H, W, 4), cropped to the level size.
        /// (DXN: R, G and, with nm_preview, the reconstructed B; half-float formats: Reinhard tone mapped for display, which goes
        /// through powf and is therefore only as portable as the C library's.)
        public UxImage Rgba(int s = 0, int f = 0, int m = 0, bool nm_preview = true, double exposure = 1.0)
        {
            var sf = surfaces[s]; var (name, bw, bh, bpb) = Elem(s);
            int lw = Math.Max(1, sf.w >> m), lh = Math.Max(1, sf.h >> m);
            var b = LevelBlocks(s, f, m);
            UxImage img;
            if (bw == 4)
            {
                img = UxTex.DecodeBlocks(b, name);
                if (name == "DXN" && nm_preview)
                    for (int i = 0; i < img.data.Length; i += 4)
                    {
                        double x = img.data[i] / 127.5 - 1, y = img.data[i + 1] / 127.5 - 1;
                        double z = Math.Sqrt(UxNp.Clip(1 - x * x - y * y, 0, 1));
                        img.data[i + 2] = (byte)UxNp.Round((z * 0.5 + 0.5) * 255);
                    }
            }
            else if (name == "8_8_8_8")
            {   // after the 8in32 swap the bytes are B, G, R, A
                var o = new byte[b.data.Length];
                for (int i = 0; i < o.Length; i += 4) { o[i] = b.data[i + 2]; o[i + 1] = b.data[i + 1]; o[i + 2] = b.data[i]; o[i + 3] = b.data[i + 3]; }
                img = new UxImage(o, b.hb, b.wb, 4);
            }
            else if (name == "16_16_16_16_EXPAND" || name == "16_16_16_16_FLOAT")
            {
                var fl = UxTex.HalfToFloat(b.data, 0, b.hb * b.wb * 4);
                var o = new byte[b.hb * b.wb * 4];
                float ex = (float)exposure, g = (float)(1 / 2.2);
                for (int i = 0; i < fl.Length; i += 4)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        float v = fl[i + c] * ex; v = v / (1 + v);
                        v = v < 0 ? 0 : v > 1 ? 1 : v;                                   // np.clip (NaN stays NaN)
                        v = (float)Math.Pow(v, g);
                        o[i + c] = ToU8((float)UxNp.Round(v * 255f));
                    }
                    float a = fl[i + 3]; a = a < 0 ? 0 : a > 1 ? 1 : a;
                    o[i + 3] = ToU8(a * 255f);
                }
                img = new UxImage(o, b.hb, b.wb, 4);
            }
            else throw new UxException("rgba: " + name);
            return img.Crop(lh, lw);
        }
        static byte ToU8(float v) => float.IsNaN(v) ? (byte)0 : unchecked((byte)(int)v);       // astype(np.uint8) of an in-range float: truncation

        /// float_level(s, f, m): float32 (H, W, C) of a half-float format; C = bpb / 2.
        public float[] FloatLevel(int s = 0, int f = 0, int m = 0)
        {
            var b = LevelBlocks(s, f, m);
            return UxTex.HalfToFloat(b.data, 0, b.data.Length / 2);
        }

        static byte[] CropElements(UxBlocks b, int lh, int lw)
        {
            lh = Math.Min(lh, b.hb); lw = Math.Min(lw, b.wb);
            if (lh == b.hb && lw == b.wb) return b.data;
            var o = new byte[lh * lw * b.bpb];
            for (int y = 0; y < lh; y++) Buffer.BlockCopy(b.data, y * b.wb * b.bpb, o, y * lw * b.bpb, lw * b.bpb);
            return o;
        }

        /// pc_level_bytes(s, f, m, target): the raw PC payload of one level for the given PC format.
        public byte[] PcLevelBytes(int s, int f, int m, byte[] target)
        {
            var sf = surfaces[s]; var (name, bw, bh, bpb) = Elem(s);
            int lw = Math.Max(1, sf.w >> m), lh = Math.Max(1, sf.h >> m);
            var b = LevelBlocks(s, f, m);
            if (UxTex.Fmt(target, UxTex.PC_DXT1) || UxTex.Fmt(target, UxTex.PC_DXT5))
            {
                string want = UxTex.Fmt(target, UxTex.PC_DXT1) ? "DXT1" : "DXT4_5";
                if (name != want) throw new UxException(name + " cannot be copied losslessly to " + UxTex.FmtText(target));
                return b.data;                                  // the blocks are already in PC order
            }
            if (UxTex.Fmt(target, UxTex.PC_A8R8G8B8))
            {
                if (name == "8_8_8_8") return CropElements(b, lh, lw);
                var img = Rgba(s, f, m, false);
                var o = new byte[img.data.Length];
                for (int i = 0; i < o.Length; i += 4) { o[i] = img.data[i + 2]; o[i + 1] = img.data[i + 1]; o[i + 2] = img.data[i]; o[i + 3] = img.data[i + 3]; }
                return o;
            }
            if (UxTex.Fmt(target, UxTex.PC_A16B16G16R16F)) return CropElements(b, lh, lw);       // half floats R, G, B, A (LE after the 8in16 swap)
            if (UxTex.Fmt(target, UxTex.PC_G16R16F))
            {
                var img = Rgba(s, f, m, false);
                int n = img.h * img.w; var o = new byte[n * 4];
                for (int i = 0; i < n; i++) { UxBin.PutU16(o, 4 * i, UxNp.FloatToHalf(img.data[4 * i] / 255.0f)); UxBin.PutU16(o, 4 * i + 2, UxNp.FloatToHalf(img.data[4 * i + 1] / 255.0f)); }
                return o;
            }
            throw new UxException("pc_level_bytes: target " + UxBin.Hex(target));
        }

        /// default_pc_target(s).
        public byte[] DefaultPcTarget(int s = 0)
        {
            switch (Elem(s).name)
            {
                case "DXT1": return UxTex.PC_DXT1; case "DXT4_5": return UxTex.PC_DXT5; case "DXT2_3": return UxTex.PC_A8R8G8B8; case "DXN": return UxTex.PC_G16R16F;
                case "8_8_8_8": return UxTex.PC_A8R8G8B8; case "16_16_16_16_EXPAND": case "16_16_16_16_FLOAT": return UxTex.PC_A16B16G16R16F;
            }
            throw new UxException("no PC target for " + Elem(s).name);
        }

        /// to_pc(target, surfaces, max_mips): Reflex PC v4 .texture bytes (DXT1 / DXT5 blocks copied, A8R8G8B8 and half-float
        /// formats lossless, DXN -> G16R16F; 2D -> kind 0, cube -> kind 2 with the faces written face-major; flags 0).
        public byte[] ToPc(byte[] target = null, IList<int> surfaceList = null, int max_mips = -1)
        {
            var list = surfaceList ?? Range(nsurf);
            var recs = new List<(int, int, int, byte[])>(); var tab = new List<(int, int)>(); var payload = new List<byte[]>();
            foreach (int s in list)
            {
                var sf = surfaces[s];
                byte[] tg = target ?? DefaultPcTarget(s);
                int nm = max_mips < 0 ? sf.mips : Math.Min(max_mips, sf.mips);
                recs.Add((nm, sf.w, sf.h, tg));
                for (int f = 0; f < faces; f++)
                    for (int m = 0; m < nm; m++)
                    {
                        byte[] d = PcLevelBytes(s, f, m, tg);
                        tab.Add((d.Length, UxTex.PcPitch(tg, Math.Max(1, sf.w >> m))));
                        payload.Add(d);
                    }
            }
            int k = kind == 8 ? 0 : kind == 10 ? 2 : 0;
            return UxTex.BuildPc(recs, tab, payload, k, 0);
        }
        static List<int> Range(int n) { var l = new List<int>(n); for (int i = 0; i < n; i++) l.Add(i); return l; }

        /// info().
        public string Info()
        {
            var s0 = surfaces[0];
            return "v" + version + " kind " + kind + " flags 0x" + flags.ToString("x") + " nsurf " + nsurf + " nentry " + nentry + " | " + s0.w + "x" + s0.h + " mips " + s0.mips + " " + s0.name +
                " (" + s0.fmt.ToString("x8") + ") tiled " + s0.tiled + " endian " + s0.endian + " | data 0x" + data_offset.ToString("x") + "..0x" + data_end.ToString("x") + " file " + raw.Length + " " +
                (CheckSize() ?? "SIZE MISMATCH");
        }
    }
}
