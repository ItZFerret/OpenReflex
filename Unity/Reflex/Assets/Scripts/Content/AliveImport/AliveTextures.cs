// Xenos texture resources to PC texture v4 (alive_textures.py and the remaining texture paths of alive_convert.py): the tiled layout with
// its packed mip tail, BC1/BC2/BC3 (blocks kept), DXN decoded to A8R8G8B8 normals, tiled / linear ARGB, FP16 (2D / cube, tiled / linear),
// R16 heightfields to R32F.
using System;
using System.Collections.Generic;
using System.IO;

namespace Reflex.Port.Content.AliveImport
{
    internal static class Textures
    {
        // Xenos format word -> (block size in texels, bytes per block, PC D3DFORMAT, endian swap unit)
        static readonly Dictionary<uint, (int block, int bpb, uint pc, int unit)> Formats = new Dictionary<uint, (int, int, uint, int)>
        {
            { 0x1a200152, (4, 8, 0x31545844, 2) }, { 0x1a200052, (4, 8, 0x31545844, 2) },
            { 0x1a200153, (4, 16, 0x33545844, 2) }, { 0x1a200154, (4, 16, 0x35545844, 2) }, { 0x1a200054, (4, 16, 0x35545844, 2) },
        };

        public static long TiledOffset(long x, long y, long pitch, long bpb)
        {
            long outer = ((y / 32) * (pitch / 32) + x / 32) * 64;
            long inner = ((y / 2) % 8) * 8 + x % 8;
            long address = (outer + inner) * bpb;
            long bank = (y / 16) % 2;
            long pipe = ((x / 8) % 4) ^ (((y / 8) % 2) * 2);
            return (address % 16) + ((address / 16) % 2) * 32 + ((address / 32) % 8) * 256 + (address / 256) * 4096 + (y % 2) * 16 + pipe * 64 + bank * 2048;
        }

        static int Log2Ceil(int v) { int r = 0; while ((1L << r) < v) r++; return Math.Max(0, r); }

        public static (int x, int y)? PackedMipOffset(int width, int height, int mip)
        {
            int lw = Log2Ceil(width), lh = Log2Ceil(height), size = Math.Min(lw, lh);
            if (size > 4 + mip) return null;
            int b = size > 4 ? size - 4 : 0, packed = mip - b;
            if (packed < 3) return lw > lh ? (0, 16 >> packed) : (16 >> packed, 0);
            if (lw > lh) return ((1 << (lw - b)) >> (packed - 2), 0);
            return (0, (1 << (lh - b)) >> (packed - 2));
        }
        static int PackedMipBase(int width, int height) { int s = Math.Min(Log2Ceil(width), Log2Ceil(height)); return s > 4 ? s - 4 : 0; }
        static int Align(int v, int a) => (v + a - 1) / a * a;
        static (int w, int h) MipDims(int width, int height, int level) => (Math.Max(1, width >> level), Math.Max(1, height >> level));

        struct Plan { public int level, w, h, offset, x0, y0, pitch; }

        static List<Plan> Layout(int width, int height, int levels, int block, int bpb, out long total)
        {
            var o = new List<Plan>(); int offset = 0;
            int tail = PackedMipBase(width, height); int? tailOffset = null; int tailPitch = 0;
            for (int level = 0; level < levels; level++)
            {
                var (w, h) = MipDims(width, height, level);
                int bw = Math.Max(1, (w + block - 1) / block), bh = Math.Max(1, (h + block - 1) / block);
                var packed = PackedMipOffset(width, height, level);
                if (packed == null)
                {
                    int pitch = Align(bw, 32);
                    o.Add(new Plan { level = level, w = w, h = h, offset = offset, pitch = pitch });
                    offset += pitch * Align(bh, 32) * bpb;
                    continue;
                }
                if (tailOffset == null)
                {
                    var (pw, ph) = MipDims(width, height, tail);
                    int pbw = Math.Max(1, (pw + block - 1) / block), pbh = Math.Max(1, (ph + block - 1) / block);
                    tailOffset = offset; tailPitch = Align(pbw, 32);
                    offset += tailPitch * Align(pbh, 32) * bpb;
                }
                o.Add(new Plan { level = level, w = w, h = h, offset = tailOffset.Value, x0 = packed.Value.x / block, y0 = packed.Value.y / block, pitch = tailPitch });
            }
            total = offset;
            return o;
        }

        static uint[] Header(byte[] s)
        {
            Bin.Require(s.Length >= 56, "Truncated texture");
            var h = new uint[14]; for (int i = 0; i < 14; i++) h[i] = Bin.U32BE(s, 4 * i);
            Bin.Require(h[0] == 0x77ddcc00 && h[1] == 6 && h[2] == 0 && h[3] == 1, "Unsupported Alive texture header");
            return h;
        }

        /// Every level of a tiled 2D texture: (w, h, blocks bytes row-major (bh, bw, bpb)), endian swapped by `unit` bytes.
        static List<(int w, int h, int bw, int bh, byte[] blocks)> UntiledLevels(byte[] source, int block, int bpb, int unit, out uint[] h)
        {
            h = Header(source);
            int levels = (int)h[10], width = (int)h[11], height = (int)h[12];
            Bin.Require((h[5] & 2) == 0 && h[4] == levels, "Cube / array texture");
            Bin.Require(levels >= 1 && levels <= 15 && width >= 1 && width <= 8192 && height >= 1 && height <= 8192, "Texture dimensions");
            Bin.Require((width & (width - 1)) == 0 && (height & (height - 1)) == 0, "Non-power-of-two tiled texture");
            int start = 56 + (int)h[4] * 8, len = source.Length - start;
            var plan = Layout(width, height, levels, block, bpb, out long total);
            Bin.Require(total == len || (total < len && Bin.StartsWith(source, "BXML", start + (int)total)), $"Tiled storage size {total} differs from the resource ({len})");
            var o = new List<(int, int, int, int, byte[])>();
            foreach (var p in plan)
            {
                int bw = Math.Max(1, (p.w + block - 1) / block), bh = Math.Max(1, (p.h + block - 1) / block);
                var blocks = new byte[bw * bh * bpb];
                long maxAddr = 0;
                for (int y = 0; y < bh; y++)
                    for (int x = 0; x < bw; x++)
                    {
                        long a = p.offset + TiledOffset(x + p.x0, y + p.y0, p.pitch, bpb);
                        if (a > maxAddr) maxAddr = a;
                        Bin.Require(a + bpb <= len, "Tiled level outside the texture data");
                        int dst = (y * bw + x) * bpb;
                        Buffer.BlockCopy(source, start + (int)a, blocks, dst, bpb);
                    }
                for (int i = 0; i < blocks.Length; i += unit) Array.Reverse(blocks, i, unit);
                o.Add((p.w, p.h, bw, bh, blocks));
            }
            return o;
        }

        static byte[] PcTexture(uint[] src, uint pcFormat, List<(byte[] data, int pitch)> levels)
        {
            var h = (uint[])src.Clone();
            h[1] = 4; h[4] = (uint)levels.Count; h[5] = 0; h[10] = (uint)levels.Count; h[13] = pcFormat;
            return Assemble(h, levels);
        }
        static byte[] Assemble(uint[] h, List<(byte[] data, int pitch)> levels)
        {
            var o = new MemoryStream(); var w = new byte[8];
            foreach (var v in h) { Bin.PutU32LE(w, 0, v); o.Write(w, 0, 4); }
            foreach (var l in levels) { Bin.PutU32LE(w, 0, (uint)l.data.Length); Bin.PutU32LE(w, 4, (uint)l.pitch); o.Write(w, 0, 8); }
            foreach (var l in levels) o.Write(l.data, 0, l.data.Length);
            return o.ToArray();
        }

        /// A BC1 / BC2 / BC3 Xenos format word (ConvertBc's).
        public static bool IsBc(uint format) => Formats.ContainsKey(format);

        public static byte[] ConvertBc(byte[] source)
        {
            var hh = Header(source);
            Bin.Require(Formats.ContainsKey(hh[13]), "Not a BC texture format");
            var f = Formats[hh[13]];
            var levels = UntiledLevels(source, f.block, f.bpb, f.unit, out var h);
            var l = new List<(byte[], int)>();
            foreach (var lv in levels) l.Add((lv.blocks, lv.bw * f.bpb));
            return PcTexture(h, f.pc, l);
        }

        static double[] DecodeBc4(byte[] b, int at)
        {
            double a = b[at], beta = b[at + 1];
            var table = new double[8]; table[0] = a; table[1] = beta;
            for (int i = 2; i < 8; i++)
                table[i] = a > beta ? ((8 - i) * a + (i - 1) * beta) / 7 : (i < 6 ? ((6 - i) * a + (i - 1) * beta) / 5 : (i == 6 ? 0 : 255));
            ulong bits = 0; for (int i = 0; i < 6; i++) bits |= (ulong)b[at + 2 + i] << (8 * i);
            var o = new double[16]; for (int k = 0; k < 16; k++) o[k] = table[(bits >> (3 * k)) & 7];
            return o;
        }

        static byte U8(double v) { v = Math.Min(255, Math.Max(0, v)); return (byte)Math.Round(v, MidpointRounding.ToEven); }

        public static byte[] ConvertDxn(byte[] source)
        {
            var h0 = Header(source);
            Bin.Require(h0[13] == 0x1a200171, "Not a DXN texture");
            var levels = UntiledLevels(source, 4, 16, 2, out var h);
            var outL = new List<(byte[], int)>();
            foreach (var (w, hh, bw, bh, blocks) in levels)
            {
                var px = new byte[w * hh * 4];
                for (int by = 0; by < bh; by++)
                    for (int bx = 0; bx < bw; bx++)
                    {
                        int at = (by * bw + bx) * 16;
                        var x0 = DecodeBc4(blocks, at); var y0 = DecodeBc4(blocks, at + 8);
                        for (int k = 0; k < 16; k++)
                        {
                            int X = bx * 4 + (k & 3), Y = by * 4 + (k >> 2);
                            if (X >= w || Y >= hh) continue;
                            double xv = x0[k], yv = y0[k];
                            double x = xv / 127.5 - 1, y = yv / 127.5 - 1;
                            double z = Math.Sqrt(Math.Max(0, 1 - x * x - y * y)) * 127.5 + 127.5;
                            int o = (Y * w + X) * 4;
                            px[o] = U8(z); px[o + 1] = U8(yv); px[o + 2] = U8(xv); px[o + 3] = 255;
                        }
                    }
                outL.Add((px, w * 4));
            }
            return PcTexture(h, 21, outL);
        }

        public static byte[] ConvertDxnAny(byte[] source)
        {
            var h = Header(source);
            Bin.Require(h[13] == 0x1a200071 || h[13] == 0x1a200171, "Not a DXN texture");
            var p = (byte[])source.Clone(); Bin.PutU32BE(p, 52, 0x1a200171);
            return ConvertDxn(p);
        }

        public static byte[] ConvertArgbTiled(byte[] source)
        {
            var h0 = Header(source);
            Bin.Require(h0[13] == 0x18280186, "Not a tiled ARGB texture");
            var levels = UntiledLevels(source, 1, 4, 4, out var h);
            var l = new List<(byte[], int)>();
            foreach (var lv in levels) l.Add((lv.blocks, lv.bw * 4));
            return PcTexture(h, 21, l);
        }

        /// alive_textures.box_mips (uint8, 4 channels).
        static List<(byte[] px, int w, int h)> BoxMips(byte[] pixels, int w, int h)
        {
            var chain = new List<(byte[], int, int)> { (pixels, w, h) };
            while (chain[chain.Count - 1].Item3 > 1 || chain[chain.Count - 1].Item2 > 1)
            {
                var (src, cw, ch) = chain[chain.Count - 1];
                var p = new double[cw * ch * 4]; for (int i = 0; i < p.Length; i++) p[i] = src[i];
                int pw = cw, ph = ch;
                if (ph > 1)
                {
                    int nh = Math.Max(1, ph >> 1); var q = new double[pw * nh * 4];
                    for (int y = 0; y < nh; y++)
                        for (int i = 0; i < pw * 4; i++)
                        {
                            int r0 = 2 * y, r1 = Math.Min(2 * y + 1, ph - 1);   // odd: the last row repeated
                            q[y * pw * 4 + i] = (p[r0 * pw * 4 + i] + p[r1 * pw * 4 + i]) / 2;
                        }
                    p = q; ph = nh;
                }
                if (pw > 1)
                {
                    int nw = Math.Max(1, pw >> 1); var q = new double[nw * ph * 4];
                    for (int y = 0; y < ph; y++)
                        for (int x = 0; x < nw; x++)
                            for (int c = 0; c < 4; c++)
                            {
                                int c0 = 2 * x, c1 = Math.Min(2 * x + 1, pw - 1);
                                q[(y * nw + x) * 4 + c] = (p[(y * pw + c0) * 4 + c] + p[(y * pw + c1) * 4 + c]) / 2;
                            }
                    p = q; pw = nw;
                }
                var o = new byte[p.Length]; for (int i = 0; i < o.Length; i++) o[i] = (byte)Math.Round(p[i], MidpointRounding.ToEven);
                chain.Add((o, pw, ph));
            }
            return chain;
        }

        public static byte[] ConvertArgbLinear(byte[] source)
        {
            var h = Header(source);
            Bin.Require(h[13] == 0x18280086, "Not a linear ARGB texture");
            int levels = (int)h[10], width = (int)h[11], height = (int)h[12];
            Bin.Require(width >= 1 && width <= 8192 && height >= 1 && height <= 8192, "ARGB dimensions");
            int offset = 56 + (int)h[4] * 8, pitch = Math.Max(256, width * 4);
            Bin.Require(offset + pitch * (height - 1) + width * 4 <= source.Length, "Truncated ARGB base level");
            var rows = new byte[width * height * 4];
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    for (int c = 0; c < 4; c++) rows[(y * width + x) * 4 + c] = source[offset + y * pitch + x * 4 + (3 - c)];
            var l = new List<(byte[], int)>();
            if (levels > 1)
            {
                var chain = BoxMips(rows, width, height);
                for (int i = 0; i < Math.Min(levels, chain.Count); i++) l.Add((chain[i].px, chain[i].w * 4));
            }
            else l.Add((rows, width * 4));
            return PcTexture(h, 21, l);
        }

        static bool FiniteHalves(byte[] le)
        {
            for (int i = 0; i < le.Length; i += 2) if ((Bin.U16LE(le, i) & 0x7c00) == 0x7c00) return false;
            return true;
        }

        public static byte[] ConvertHalf(byte[] source)
        {
            var h = Header(source);
            Bin.Require(h[13] == 0x1a22ab5d || h[13] == 0x1a22aa5d, "Not an FP16 texture");
            int levels = (int)h[10], width = (int)h[11], height = (int)h[12];
            int faces = (h[5] & 2) != 0 ? 6 : 1;
            Bin.Require(h[4] == levels * faces && width >= 1 && width <= 8192 && height >= 1 && height <= 8192, "FP16 descriptors");
            int dataStart = 56 + (int)h[4] * 8, payloadLen = source.Length - dataStart;
            var outL = new List<(byte[] data, int pitch)>();
            h = (uint[])h.Clone();
            if (faces == 1 && h[13] == 0x1a22ab5d && (width & (width - 1)) == 0 && (height & (height - 1)) == 0)
            {
                var lv = UntiledLevels(source, 1, 8, 2, out _);
                foreach (var x in lv) outL.Add((x.blocks, x.bw * 8));
            }
            else
            {
                long stride = 0;
                for (int i = 0; i < levels; i++) stride += Math.Max(256, Math.Max(1, width >> i) * 8) * Math.Max(2, Math.Max(1, height >> i));
                Bin.Require(stride * faces == payloadLen, "FP16 allocation mismatch");
                bool tiled = h[13] == 0x1a22ab5d;
                for (int face = 0; face < faces; face++)
                {
                    long at = face * stride;
                    for (int level = 0; level < levels; level++)
                    {
                        var (w, hh) = MipDims(width, height, level);
                        int pitch = Math.Max(256, w * 8), rows = Math.Max(2, hh);
                        byte[] raw = null;
                        if (tiled && level == 0 && Math.Min(w, hh) >= 32)
                        {
                            raw = new byte[w * hh * 8]; int pa = Align(w, 32);
                            for (int y = 0; y < hh; y++)
                                for (int x = 0; x < w; x++)
                                {
                                    long a = at + TiledOffset(x, y, pa, 8);
                                    Bin.Require(a + 8 <= payloadLen, "FP16 face bounds");
                                    Buffer.BlockCopy(source, dataStart + (int)a, raw, (y * w + x) * 8, 8);
                                }
                        }
                        else if (!tiled)
                        {
                            raw = new byte[w * hh * 8];
                            for (int y = 0; y < hh; y++) Buffer.BlockCopy(source, dataStart + (int)at + y * pitch, raw, y * w * 8, w * 8);
                        }
                        at += (long)pitch * rows;
                        if (raw == null) continue;
                        for (int i = 0; i < raw.Length; i += 2) Array.Reverse(raw, i, 2);
                        Bin.Require(FiniteHalves(raw), "Nonfinite FP16");
                        outL.Add((raw, w * 8));
                    }
                }
            }
            h[1] = 4; h[4] = (uint)outL.Count; h[13] = 113; h[5] = faces == 6 ? 2u : 0u; h[10] = (uint)(outL.Count / faces);
            return Assemble(h, outL);
        }

        /// alive_convert.convert_r16_texture (every level: linear UNORM16 -> R32F).
        public static byte[] ConvertR16(byte[] source)
        {
            Bin.Require(source.Length >= 64, "Truncated texture");
            var h = new uint[14]; for (int i = 0; i < 14; i++) h[i] = Bin.U32BE(source, 4 * i);
            Bin.Require(h[0] == 0x77ddcc00 && h[1] == 6 && h[2] == 0 && h[3] == 1 && h[13] == 0x28000058, "Texture needs another Xbox format converter");
            int levels = (int)h[10], width = (int)h[11], height = (int)h[12];
            Bin.Require(levels >= 1 && levels <= 15 && width >= 1 && width <= 8192 && height >= 1 && height <= 8192, "Texture dimensions");
            Bin.Require((width & (width - 1)) == (height & (height - 1)) && (width & (width - 1)) == 0, "Texture dimensions are not powers of two");
            Bin.Require(56 + levels * 8 <= source.Length, "Truncated mip table");
            int cursor = 56 + levels * 8; var outL = new List<(byte[], int)>();
            for (int level = 0; level < levels; level++)
            {
                int w = Math.Max(1, width >> level), hh = Math.Max(1, height >> level);
                uint size = Bin.U32BE(source, 56 + level * 8), pitch = Bin.U32BE(source, 60 + level * 8);
                Bin.Require(size == w * hh * 2 && pitch == w * 2, "Unexpected R16 mip table");
                int sp = Math.Max(256, (int)pitch), sr = Math.Max(2, hh);
                Bin.Require(cursor + sp * sr <= source.Length, "Truncated R16 mip");
                var px = new byte[w * hh * 4];
                for (int y = 0; y < hh; y++)
                    for (int x = 0; x < w; x++)
                        Bin.PutF32LE(px, (y * w + x) * 4, (float)Bin.U16BE(source, cursor + y * sp + x * 2) / 65535f);
                outL.Add((px, w * 4));
                cursor += sp * sr;
            }
            Bin.Require(cursor == source.Length, "Unrecognized trailing texture data");
            h[1] = 4; h[5] = 0; h[13] = 114;
            return Assemble(h, outL);
        }

        /// alive_convert.convert_r16_base_texture (the full-resolution level only).
        public static byte[] ConvertR16Base(byte[] source)
        {
            var h = new uint[14]; for (int i = 0; i < 14; i++) h[i] = Bin.U32BE(source, 4 * i);
            Bin.Require(h[0] == 0x77ddcc00 && h[1] == 6 && h[2] == 0 && h[3] == 1 && h[13] == 0x28000058, "Unsupported R16 texture");
            int levels = (int)h[10], width = (int)h[11], height = (int)h[12];
            Bin.Require(levels >= 1 && levels <= 15 && width >= 1 && width <= 8192 && height >= 1 && height <= 8192, "R16 dimensions");
            int offset = 56 + levels * 8, pitch = Math.Max(256, width * 2);
            Bin.Require(offset + pitch * height <= source.Length, "Truncated R16 base mip");
            var px = new byte[width * height * 4];
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    Bin.PutF32LE(px, (y * width + x) * 4, (float)Bin.U16BE(source, offset + y * pitch + x * 2) / 65535f);
            h[1] = 4; h[4] = 1; h[5] = 0; h[10] = 1; h[13] = 114;
            return Assemble(h, new List<(byte[], int)> { (px, width * 4) });
        }

        // The Xenos 3D tiling of a 32x32x32 32-bit volume (the colour LUTs; alive_textures.LUT32_COORD_BITS): the texel-address bits each
        // coordinate bit x0..x4, y0..y4, z0..z4 sets (GF(2)-linear).
        static readonly int[][] Lut32X = { new[] { 0 }, new[] { 1 }, new[] { 3 }, new[] { 4 }, new[] { 5 } };
        static readonly int[][] Lut32Y = { new[] { 2 }, new[] { 6 }, new[] { 7 }, new[] { 5, 9 }, new[] { 11 } };
        static readonly int[][] Lut32Z = { new[] { 8 }, new[] { 10 }, new[] { 5, 9, 12 }, new[] { 13 }, new[] { 14 } };

        static int Lut32Address(int x, int y, int z)
        {
            int a = 0;
            for (int k = 0; k < 5; k++)
            {
                foreach (int b in Lut32X[k]) a ^= ((x >> k) & 1) << b;
                foreach (int b in Lut32Y[k]) a ^= ((y >> k) & 1) << b;
                foreach (int b in Lut32Z[k]) a ^= ((z >> k) & 1) << b;
            }
            return a;
        }

        /// alive_textures.convert_color_lut: an Alive colour LUT ("R_^S" v1, tiled A8R8G8B8 32^3) to the client's ALUT sidecar: 'ALUT', 1, 32,
        /// then 32^3 RGBA bytes, x fastest, then y, then z.
        public static byte[] ConvertColorLut(byte[] source)
        {
            Bin.Require(source.Length == 32 + 131072 && Bin.StartsWith(source, "R_^S"), "Colour LUT header");
            Bin.Require(Bin.U32BE(source, 4) == 1 && Bin.U32BE(source, 8) == 0x20000 && Bin.U32BE(source, 16) == 0x18280186 && Bin.U32BE(source, 20) == 0x00200020 &&
                        Bin.U32BE(source, 24) >> 16 == 32, "Colour LUT layout");
            var o = new byte[12 + 131072];
            o[0] = (byte)'A'; o[1] = (byte)'L'; o[2] = (byte)'U'; o[3] = (byte)'T';
            Bin.PutU32LE(o, 4, 1); Bin.PutU32LE(o, 8, 32);
            int at = 12;
            for (int z = 0; z < 32; z++)
                for (int y = 0; y < 32; y++)
                    for (int x = 0; x < 32; x++, at += 4)
                    {
                        int t = 32 + 4 * Lut32Address(x, y, z);
                        o[at] = source[t + 1]; o[at + 1] = source[t + 2]; o[at + 2] = source[t + 3]; o[at + 3] = source[t];
                    }
            return o;
        }

        /// alive_convert.convert_asset for textures: (bytes, method).
        public static (byte[], string) Convert(byte[] data)
        {
            uint? fmt = data.Length >= 56 ? Bin.U32BE(data, 52) : (uint?)null;
            if (fmt is uint f)
            {
                if (Formats.ContainsKey(f)) return (ConvertBc(data), "untiled-bc-full-mip-chain");
                if (f == 0x1a200171) return (ConvertDxn(data), "untiled-dxn-normal-rgb-full-mip-chain");
                if (f == 0x18280186) return (ConvertArgbTiled(data), "untiled-argb-full-mip-chain");
                if (f == 0x18280086) return (ConvertArgbLinear(data), "linear-argb-box-filtered-mips");
                if (f == 0x1a200071) return (ConvertDxnAny(data), "packed-dxn-normal-rgb-full-mip-chain");
                if (f == 0x1a22ab5d || f == 0x1a22aa5d) return (ConvertHalf(data), "half-float-levels");
                if (f == 0x28000058 && !(data[44] == data[48] && data[45] == data[49] && data[46] == data[50] && data[47] == data[51]))
                    return (ConvertR16Base(data), "linear-r16-full-resolution-mip");
            }
            return (ConvertR16(data), "linear-r16-to-r32f");
        }
    }
}
