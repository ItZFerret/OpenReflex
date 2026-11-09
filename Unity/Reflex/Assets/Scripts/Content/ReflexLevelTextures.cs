// Texture resources (0x77ddcc00 v4) of the level packages, read from the player's install.
// Rfx.ParseTexture covers DXT1/3/5 and A8R8G8B8; this adds what the level materials need on top of it: G16R16F normal maps stored
// 0..1 (RGHalf), A16B16G16R16F maps, R32F height/shadow maps, and an exact CPU decoder (the DXT rules of _re/formats/level_export.py
// decode_block_texture: 565 -> 8 bit by bit replication, (2a+b)/3 integer palettes) for verification and for the few block
// textures the GPU path cannot take (dimensions not a multiple of 4).
// Unity row order everywhere: the first row of a level is the image's BOTTOM row (as a PNG loaded with LoadImage), which makes
// mesh UVs (u, 1 - v_d3d) sample the picture upright. The original device disables sRGB sampling: textures here are linear data.
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.Content
{
    internal sealed class LevelTexture
    {
        public string name; public int width, height, d3dFormat;
        public TextureFormat format;
        public readonly List<byte[]> mips = new List<byte[]>();   // Unity row order
        public bool alpha;
        public bool cube;                                         // cube map: mips[face * levels + level], D3D face order and row order
        public int levels = 1;
        public int MipCount => mips.Count;
    }

    internal static class ReflexTex
    {
        /// Verification only: reproduce the 8-alpha interpolation of the Python exporter (it weights (6 - i) a0 + i a1 over 7; D3D/BC3 uses (7 - i)).
        public static bool PythonDxt5Alpha;
        public const int A8R8G8B8 = 21, X8R8G8B8 = 22, G16R16F = 112, A16B16G16R16F = 113, R32F = 114;
        const int Dxt1 = 0x31545844, Dxt3 = 0x33545844, Dxt5 = 0x35545844;

        public static int D3dFormat(byte[] b) { Rfx.Check(b.Length >= 56 && Rfx.U32(b, 0) == 0x77ddcc00 && Rfx.I32(b, 4) == 4, "texture header"); return Rfx.I32(b, 52); }

        /// A texture resource as GPU-ready mips. Null only for unsupported formats.
        public static LevelTexture Parse(string name, byte[] b)
        {
            int fmt = D3dFormat(b), levels = Rfx.I32(b, 40), w = Rfx.I32(b, 44), h = Rfx.I32(b, 48);
            if (Rfx.I32(b, 20) == 2) return ParseCube(name, b, fmt, levels, w);
            Rfx.Check(levels > 0 && levels <= 15 && w > 0 && h > 0 && w <= 8192 && h <= 8192, "texture size");
            if (fmt == G16R16F || fmt == A16B16G16R16F || fmt == R32F)
            {
                int bpp = fmt == G16R16F ? 4 : fmt == A16B16G16R16F ? 8 : 4;
                var t = new LevelTexture { name = name, width = w, height = h, d3dFormat = fmt, alpha = fmt == A16B16G16R16F,
                    format = fmt == G16R16F ? TextureFormat.RGHalf : fmt == A16B16G16R16F ? TextureFormat.RGBAHalf : TextureFormat.RFloat };
                int p = 56 + levels * 8;
                for (int l = 0; l < levels; l++)
                {
                    int lw = Math.Max(1, w >> l), lh = Math.Max(1, h >> l), size = Rfx.I32(b, 56 + l * 8);
                    Rfx.Check(size == lw * lh * bpp && p + size <= b.Length, "texture level size");
                    var flipped = new byte[size]; int row = lw * bpp;
                    for (int y = 0; y < lh; y++) Buffer.BlockCopy(b, p + y * row, flipped, (lh - 1 - y) * row, row);
                    t.mips.Add(flipped); p += size;
                }
                return t;
            }
            var d = Rfx.ParseTexture(b);
            if (d == null)
            {
                // block texture the GPU path refuses (dimension not a multiple of 4 ...) or an unobserved format: decode level 0 exactly
                if (fmt != Dxt1 && fmt != Dxt3 && fmt != Dxt5 && fmt != A8R8G8B8 && fmt != X8R8G8B8) return null;
                var rgba = DecodeExactRgba(b, out int dw, out int dh);
                var t = new LevelTexture { name = name, width = dw, height = dh, d3dFormat = fmt, format = TextureFormat.RGBA32, alpha = true };
                t.mips.Add(FlipRows(rgba, dw, dh, 4));
                return t;
            }
            var r = new LevelTexture { name = name, width = d.width, height = d.height, d3dFormat = fmt, format = d.format, alpha = d.alpha };
            r.mips.AddRange(d.mips);
            return r;
        }

        /// A cube texture (header word 5 = 2): the level table (56 + 8 l: size, pitch) describes one face; the faces follow each other,
        /// each with all its levels (6 x the level sizes after the table). FP16 (A16B16G16R16F), A8R8G8B8, DXT1 / DXT5.
        static LevelTexture ParseCube(string name, byte[] b, int fmt, int levels, int size)
        {
            Rfx.Check(levels > 0 && levels <= 15 && size > 0 && size <= 4096, "cube size");
            TextureFormat tf;
            if (fmt == A16B16G16R16F) tf = TextureFormat.RGBAHalf;
            else if (fmt == A8R8G8B8 || fmt == X8R8G8B8) tf = TextureFormat.BGRA32;
            else if (fmt == Dxt1) tf = TextureFormat.DXT1;
            else if (fmt == Dxt5) tf = TextureFormat.DXT5;
            else return null;
            var t = new LevelTexture { name = name, width = size, height = size, d3dFormat = fmt, format = tf, alpha = true, cube = true, levels = levels };
            // the level table has an entry per face and level (header word 4 = 6 x levels: NAT_CO_ReflectionMap 48, CarReflectStudio8_SM 24);
            // the faces' data follow it (checked against the cube a recorded frame of the original holds: render_capture rt/cube_*.raw)
            int entries = Rfx.I32(b, 16); Rfx.Check(entries >= levels && 56 + entries * 8 <= b.Length, "cube level table");
            int p = 56 + entries * 8;
            for (int f = 0; f < 6; f++)
                for (int l = 0; l < levels; l++)
                {
                    int n = Rfx.I32(b, 56 + l * 8);
                    Rfx.Check(n > 0 && p + n <= b.Length, "cube level size");
                    var d = new byte[n]; Buffer.BlockCopy(b, p, d, 0, n); t.mips.Add(d); p += n;
                }
            return t;
        }

        /// A cube texture as a Unity cubemap (D3D face order = Unity CubemapFace order).
        public static Cubemap ToCubemap(LevelTexture t)
        {
            if (t == null || !t.cube || !SystemInfo.SupportsTextureFormat(t.format)) return null;
            var c = new Cubemap(t.width, t.format, t.levels) { name = t.name, filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            for (int f = 0; f < 6; f++) for (int l = 0; l < t.levels; l++) c.SetPixelData(t.mips[f * t.levels + l], l, (CubemapFace)f);
            c.Apply(false, true);
            return c;
        }

        static byte[] FlipRows(byte[] src, int w, int h, int bpp)
        {
            var o = new byte[src.Length]; int row = w * bpp;
            for (int y = 0; y < h; y++) Buffer.BlockCopy(src, y * row, o, (h - 1 - y) * row, row);
            return o;
        }

        /// Creates the Unity texture (linear, mips from the file). Call on the main thread. graphics.cfg TextureQuality 0 / 1 drops top mips
        /// as XenonTextureAsset (007b2820) does (ReflexLaunchSettings.TextureMipDrop; database: the package's database, null for a level's).
        public static Texture2D ToTexture(LevelTexture t, TextureWrapMode wrapU, TextureWrapMode wrapV, FilterMode filter, int aniso = 1, string database = null)
        {
            if (t == null || t.mips.Count == 0 || t.cube) return null;
            if (!SystemInfo.SupportsTextureFormat(t.format)) return null;
            int drop = 0;
            try { drop = Mathf.Clamp(ReflexLaunchSettings.Current.TextureMipDrop(database ?? "", t.name, t.width, t.height, t.mips.Count), 0, t.mips.Count - 1); }
            catch (Exception) { drop = 0; }
            int w = Math.Max(1, t.width >> drop), h = Math.Max(1, t.height >> drop);
            var tex = new Texture2D(w, h, t.format, t.mips.Count - drop, true) { name = t.name, filterMode = filter, anisoLevel = aniso, hideFlags = HideFlags.DontSave };
            tex.wrapModeU = wrapU; tex.wrapModeV = wrapV;
            for (int i = drop; i < t.mips.Count; i++) tex.SetPixelData(t.mips[i], i - drop);
            tex.Apply(false, true);
            return tex;
        }

        // ------------------------------------------------------------------------------------------- CPU readers

        /// Level 0 of an R32F texture (the height / volume-shadow maps), rows in the file's D3D order (row 0 = top = +Z).
        public static float[] ReadR32F(byte[] b, out int w, out int h)
        {
            Rfx.Check(D3dFormat(b) == R32F, "R32F texture expected");
            int levels = Rfx.I32(b, 40); w = Rfx.I32(b, 44); h = Rfx.I32(b, 48);
            int p = 56 + levels * 8;
            Rfx.Check(Rfx.I32(b, 56) == w * h * 4 && p + w * h * 4 <= b.Length, "R32F level 0");
            var f = new float[w * h]; Buffer.BlockCopy(b, p, f, 0, w * h * 4);
            return f;
        }

        static void Rgb565(ushort c, out int r, out int g, out int bl)
        {
            int rr = c >> 11, gg = (c >> 5) & 63, bb = c & 31;
            r = (rr << 3) | (rr >> 2); g = (gg << 2) | (gg >> 4); bl = (bb << 3) | (bb >> 2);
        }

        /// Level 0 as RGBA bytes, rows in the file's D3D order (row 0 = top). The DXT rules of level_export.decode_block_texture;
        /// A8R8G8B8 / X8R8G8B8 swizzled to RGBA.
        public static byte[] DecodeExactRgba(byte[] b, out int w, out int h)
        {
            int fmt = D3dFormat(b), levels = Rfx.I32(b, 40); w = Rfx.I32(b, 44); h = Rfx.I32(b, 48);
            int p = 56 + levels * 8;
            var o = new byte[w * h * 4];
            if (fmt == A8R8G8B8 || fmt == X8R8G8B8)
            {
                Rfx.Check(p + w * h * 4 <= b.Length, "ARGB level 0");
                for (int i = 0; i < w * h; i++)
                {
                    o[i * 4] = b[p + i * 4 + 2]; o[i * 4 + 1] = b[p + i * 4 + 1]; o[i * 4 + 2] = b[p + i * 4]; o[i * 4 + 3] = fmt == X8R8G8B8 ? (byte)255 : b[p + i * 4 + 3];
                }
                return o;
            }
            Rfx.Check(fmt == Dxt1 || fmt == Dxt3 || fmt == Dxt5, "block texture expected");
            int block = fmt == Dxt1 ? 8 : 16, at = p;
            var pr = new int[4]; var pg = new int[4]; var pb = new int[4]; var alpha = new int[16]; var tab = new int[8];
            for (int by = 0; by < h; by += 4)
                for (int bx = 0; bx < w; bx += 4)
                {
                    for (int i = 0; i < 16; i++) alpha[i] = 255;
                    if (fmt == Dxt3)
                    {
                        ulong bits = BitConverter.ToUInt64(b, at);
                        for (int i = 0; i < 16; i++) alpha[i] = (int)((bits >> (4 * i)) & 15) * 17;
                    }
                    else if (fmt == Dxt5)
                    {
                        int a0 = b[at], a1 = b[at + 1]; tab[0] = a0; tab[1] = a1;
                        if (a0 > a1) for (int i = 1; i < 7; i++) tab[i + 1] = (((PythonDxt5Alpha ? 6 : 7) - i) * a0 + i * a1) / 7;   // (6 - i): level_export.decode_block_texture's weights, verification only
                        else { for (int i = 1; i < 5; i++) tab[i + 1] = ((5 - i) * a0 + i * a1) / 5; tab[6] = 0; tab[7] = 255; }
                        ulong bits = 0; for (int i = 0; i < 6; i++) bits |= (ulong)b[at + 2 + i] << (8 * i);
                        for (int i = 0; i < 16; i++) alpha[i] = tab[(int)((bits >> (3 * i)) & 7)];
                    }
                    ushort c0 = BitConverter.ToUInt16(b, at + block - 8), c1 = BitConverter.ToUInt16(b, at + block - 6);
                    uint idx = BitConverter.ToUInt32(b, at + block - 4);
                    Rgb565(c0, out pr[0], out pg[0], out pb[0]); Rgb565(c1, out pr[1], out pg[1], out pb[1]);
                    if (c0 > c1 || fmt != Dxt1)
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
                        int k = (int)((idx >> (2 * i)) & 3), a = alpha[i];
                        if (fmt == Dxt1 && c0 <= c1 && k == 3) a = 0;
                        int s = (y * w + x) * 4;
                        o[s] = (byte)pr[k]; o[s + 1] = (byte)pg[k]; o[s + 2] = (byte)pb[k]; o[s + 3] = (byte)a;
                    }
                    at += block;
                }
            return o;
        }
    }
}
