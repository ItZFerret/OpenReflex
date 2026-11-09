// Water of the original game (WP-R2, _re/notes/water.md, Ghidra names _re/names/91_water.tsv): the level's WaterSystemRSData entities
// (rivers, lakes, the sea, pools on 19 levels), read at run time from the install, and the CPU side of the original's water renderer
// (XenonWater.cpp) ported 1:1: the water asset format, the geometry clipmap (vertex / index buffers byte-identical to the original's,
// the per-frame level placement of FUN_007e5940 and the per-draw constants of FUN_007e9490), and the ocean height map (the Phillips
// spectrum the game keeps in its ocean object and the 64 x 64 FFT of FUN_007eb4a0 / 007ea610 / 007ea4a0). The GPU side is
// Shaders/ReflexWater.shader; OriginalWorld/OriginalWater.cs draws it and reads the running game's state.
//
// Entity -> asset -> surface (WaterEntity::vf5 007e3fb0, XenonWaterAsset load 007f0320, PlatformWaterSurface 007ede50 / 007ee0a0):
//   WaterSystemRSData: WaterAsset (type "water"), MaterialAsset (shader Water / Water_Ocean / Water_Pool + usr_* parameters),
//   FlowRate (entity +0x9c: the river UV scroll per second). The entity's own Matrix is not used for drawing: the asset has its own.
//   water asset v5: 0x8c header (magic 0x77770360, version, name), the surface matrix (16 floats, D3D row vectors), the extents (x, z),
//   three flags (flow map, description map, description holds heights), then the flow map (G16R16F w x h: "river" programs with
//   UVVertSampler) and the description map (A8R8G8B8 w x h: mask / colour blend; with heights: a 16-bit height per texel scaled by
//   (a, c) = gRiverHeightValue c213; without: 4 bytes per texel of which bytes 1 and 2 become G and B, R = A = 0).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public sealed class ReflexWater
    {
        /// One WaterSystemRSData entity with its asset and material.
        public sealed class Surface
        {
            public int entityId; public string name, section;
            public string waterAsset, materialAsset;
            public float flowRate;                          // entity FlowRate (+0x9c)
            public WaterAsset asset;
            public string family;                           // material shader: "Water", "Water_Ocean", "Water_Pool"
            public readonly List<string> textures = new List<string>();   // material texture slots = pixel sampler registers s0, s1 ...
            public readonly Dictionary<string, float[]> parameters = new Dictionary<string, float[]>(StringComparer.Ordinal);
            public bool River => asset != null && asset.flow != null;   // the river programs (FUN_007eec20: surface +0x1c, the UV map)
        }

        public readonly List<Surface> surfaces = new List<Surface>();
        public string reflectionMap;                       // AmbientDirectionalLight _ReflectionMap: ReflectionSampler s14 of the Water programs
        public readonly List<string> warnings = new List<string>();

        /// The level's water (empty on dry levels). Thread-safe (file reading and decoding only).
        public static ReflexWater Load(ReflexLevelContent content)
        {
            var w = new ReflexWater();
            if (content?.entities == null) return w;
            w.reflectionMap = content.entities.FirstOrDefault(e => e.Type == "AmbientDirectionalLightRSData")?.Asset("_ReflectionMap");
            foreach (var e in content.entities)
            {
                if (e.Type != "WaterSystemRSData") continue;
                var s = new Surface { entityId = e.id, name = e.String("Name"), section = e.section, waterAsset = e.Asset("WaterAsset"), materialAsset = e.Asset("MaterialAsset"), flowRate = e.Float("FlowRate") };
                try
                {
                    s.asset = WaterAsset.Parse(content.ReadAsset(s.waterAsset, "water"));
                    var root = Rfx.Bxml(content.ReadAsset(s.materialAsset, "material"));
                    foreach (var n in (root.Child("Pass") ?? root).All())
                    {
                        if ((n.tag == "PixelShader" || n.tag == "Shaders") && s.family == null) s.family = n.S("asset");
                        else if (n.tag == "Texture") s.textures.Add(n.S("textureName") ?? "");
                        else if (n.tag == "Parameter" && n.S("name") != null)
                            s.parameters[n.S("name")] = n.value as float[] ?? (n.value is float f ? new[] { f, f, f, f } : null);
                    }
                    w.surfaces.Add(s);
                }
                catch (Exception ex) { lock (w.warnings) w.warnings.Add($"water {s.waterAsset} / {s.materialAsset}: {ex.Message}"); }
            }
            return w;
        }

        public string Describe() => surfaces.Count == 0 ? "no water" : string.Join("; ", surfaces.Select(s =>
            $"{s.name}#{s.entityId} {s.waterAsset} {s.family}{(s.River ? " river" : " lake")} {s.asset.sizeX:F0}x{s.asset.sizeZ:F0} at ({s.asset.matrix[12]:F1}, {s.asset.matrix[13]:F1}, {s.asset.matrix[14]:F1}) desc {s.asset.descW}x{s.asset.descH}{(s.asset.descHeights ? " heights" : "")} {ClipmapLevelCount(s.asset.sizeX, s.asset.sizeZ)} levels flow {s.flowRate}"));

        // ============================================================================================ water asset

        public sealed class WaterAsset
        {
            public int version; public string name;
            public float[] matrix = new float[16];         // D3D row vectors (rows 0..2 the axes, row 3 the position)
            public float sizeX, sizeZ;
            public int flowW, flowH; public byte[] flow;    // G16R16F texels, D3D row order (row 0 first)
            public int descW, descH; public byte[] desc;    // A8R8G8B8 texels as the device holds them (B, G, R, A bytes), row 0 first
            public bool descHeights; public float heightScale = 1, heightOffset;   // gRiverHeightValue (surface +0x490 / +0x494)

            /// FUN_007f0320 (XenonWaterAsset load). Versions below 5 have no maps (the loader stops after the extents).
            public static WaterAsset Parse(byte[] b)
            {
                Rfx.Check(b.Length >= 0xd4 && Rfx.U32(b, 0) == 0x77770360u, "water asset header");
                var a = new WaterAsset { version = Rfx.I32(b, 4) };
                Rfx.Check(a.version >= 5, "water asset version " + a.version);
                int nameChars = Math.Min(Math.Max(0, Rfx.I32(b, 0x0c)) / 2, (0x8c - 0x10) / 2);
                a.name = System.Text.Encoding.Unicode.GetString(b, 0x10, nameChars * 2).Split('\0')[0];
                for (int i = 0; i < 16; i++) a.matrix[i] = BitConverter.ToSingle(b, 0x8c + 4 * i);
                a.sizeX = BitConverter.ToSingle(b, 0xcc); a.sizeZ = BitConverter.ToSingle(b, 0xd0);
                int p = 0xd4;
                int hasFlow = Rfx.I32(b, p), hasDesc = Rfx.I32(b, p + 4), descFloat = Rfx.I32(b, p + 8); p += 12;
                if (hasFlow != 0)
                {
                    a.flowW = Rfx.I32(b, p); a.flowH = Rfx.I32(b, p + 4); p += 8;
                    int n = a.flowW * a.flowH * 4; Rfx.Check(a.flowW > 0 && a.flowH > 0 && p + n <= b.Length, "water flow map");
                    a.flow = new byte[n]; Buffer.BlockCopy(b, p, a.flow, 0, n); p += n;
                }
                if (hasDesc != 0)
                {
                    a.descW = Rfx.I32(b, p); a.descH = Rfx.I32(b, p + 4);
                    if (descFloat != 0) { a.descHeights = true; a.heightScale = BitConverter.ToSingle(b, p + 8); a.heightOffset = BitConverter.ToSingle(b, p + 12); p += 16; }
                    else p += 8;
                    int n = a.descW * a.descH * 4; Rfx.Check(a.descW > 0 && a.descH > 0 && p + n <= b.Length, "water description map");
                    a.desc = new byte[n];
                    if (descFloat != 0) Buffer.BlockCopy(b, p, a.desc, 0, n);   // FUN_007ee600: the texels as stored
                    else for (int i = 0; i < n; i += 4) { a.desc[i] = b[p + i + 2]; a.desc[i + 1] = b[p + i + 1]; }   // 007f0504: (src2, src1, 0, 0)
                    p += n;
                }
                return a;
            }
        }

        // ============================================================================================ reflection cubes

        /// A cube texture resource (header word 5 = 2): word 4 counts the level descriptors of all six faces (6 x levels, size / pitch
        /// each) and every face's levels follow the whole table, face after face (FR_AL_WorldReflectionMap2: 48 descriptors, face data
        /// from 56 + 384, checked against the device's faces in a recorded frame). FP16 (A16B16G16R16F), A8R8G8B8, DXT1 / DXT5.
        public sealed class Cube { public string name; public int size, levels, d3dFormat; public TextureFormat format; public byte[][][] faces; }

        public static Cube ReadCube(string name, byte[] b)
        {
            Rfx.Check(b.Length >= 56 && Rfx.U32(b, 0) == 0x77ddcc00 && Rfx.I32(b, 20) == 2, "cube texture header");
            int descriptors = Rfx.I32(b, 16), levels = Rfx.I32(b, 40), size = Rfx.I32(b, 44), fmt = Rfx.I32(b, 52);
            Rfx.Check(levels > 0 && levels <= 15 && descriptors == 6 * levels && size > 0 && size <= 4096, "cube texture levels");
            var c = new Cube { name = name, size = size, levels = levels, d3dFormat = fmt, faces = new byte[6][][] };
            c.format = fmt == 113 ? TextureFormat.RGBAHalf : fmt == 21 || fmt == 22 ? TextureFormat.BGRA32 : fmt == 0x31545844 ? TextureFormat.DXT1 : fmt == 0x35545844 ? TextureFormat.DXT5 : (TextureFormat)0;
            Rfx.Check(c.format != 0, "cube texture format " + fmt);
            int p = 56 + 8 * descriptors;
            for (int f = 0; f < 6; f++)
            {
                c.faces[f] = new byte[levels][];
                for (int l = 0; l < levels; l++)
                {
                    int n = Rfx.I32(b, 56 + 8 * (f * levels + l));
                    Rfx.Check(n > 0 && p + n <= b.Length, "cube level size");
                    c.faces[f][l] = new byte[n]; Buffer.BlockCopy(b, p, c.faces[f][l], 0, n); p += n;
                }
            }
            Rfx.Check(p == b.Length, "cube texture size");
            return c;
        }

        // ============================================================================================ geometry clipmap

        // PlatformWaterSurface mesh (FUN_007e4280, called by 007ee0a0 with EAX = 0x40, XMM0 = 1.0): an odd grid of N = 65 vertices
        // (0x40 made odd), cell 1, levels doubling until (N - 1) * cell * 2^(levels - 1) covers the larger extent. Five vertex buffers
        // (FLOAT2 per vertex) and four index buffers (16-bit triangle strips) shared by all levels; gMeshResolution scales them.
        public const int GridN = 65, GridH = GridN / 2, GridQ = GridH / 2, RingRows = (GridN - 1) >> 2;
        public const float Cell = 1f;

        public static int ClipmapLevelCount(float sizeX, float sizeZ)
        {
            int ext = Math.Max(RoundHalfAway(sizeX), RoundHalfAway(sizeZ)), levels = 1;   // ftol(copysign(0.5, s) + s)
            float e = (GridN - 1) * Cell;
            while ((float)ext > e) { levels++; e *= 2f; }
            return levels;
        }
        static int RoundHalfAway(float s) => (int)(s + (s < 0 ? -0.5f : 0.5f));

        /// The original's buffers, byte-identical to the device buffers (checked against a recorded frame: WaterTests.Verify).
        public sealed class ClipmapBuffers
        {
            public Vector2[][] vertices = new Vector2[5][];   // 0 inner 33 x 33, 1 / 2 the ring rows (bottom / top), 3 / 4 the ring columns
            public ushort[] inner, rows, columns, columnsOdd; // FUN_007e4570 / 007e4800 / 007e4d50 strips (mesh +0x2c / +0x38 / +0x44 / +0x48)
            public int[] rowPrims, columnPrims;               // primitives drawn for n ring rows (mesh +0x3c / +0x4c tables)
            public int innerPrims;                            // mesh +0x30
        }

        static ClipmapBuffers buffers;
        public static ClipmapBuffers Buffers => buffers ??= BuildBuffers();

        static ClipmapBuffers BuildBuffers()
        {
            var c = new ClipmapBuffers(); const int N = GridN, h = GridH, q = GridQ; float s = Cell;
            // inner patch: (h + 1)^2 vertices from -q s, row major
            var v0 = new List<Vector2>(); float y = -q * s;
            for (int j = 0; j <= h; j++) { float x = -q * s; for (int i = 0; i <= h; i++) { v0.Add(new Vector2(x, y)); x += s; } y += s; }
            c.vertices[0] = v0.ToArray();
            var ib0 = new List<int>();
            for (int r = 0; r < h; r++)
            {
                for (int i = 0; i <= h; i++) { ib0.Add(r * (h + 1) + i); ib0.Add((r + 1) * (h + 1) + i); }
                if (r < h - 1) { ib0.Add(ib0[ib0.Count - 1]); ib0.Add((r + 1) * (h + 1)); }
            }
            c.inner = ib0.Select(i => (ushort)i).ToArray(); c.innerPrims = c.inner.Length - 2;
            // ring rows: row 0 the coarse edge (h + 1 vertices, 2 s apart), rows 1..h fine (N vertices); odd rows pull their end
            // vertices onto the next row (stitching the sides); allocated (h + 1) * N, the tail stays zero
            var v1 = new List<Vector2>(); y = -h * s; float x0 = -h * s;
            for (int i = 0; i <= h; i++) { v1.Add(new Vector2(x0, y)); x0 += 2f * s; }
            y += s;
            for (int k = 1; k <= h; k++)
            {
                float x = -h * s;
                for (int i = 0; i < N; i++) { v1.Add(new Vector2(x, (k & 1) != 0 && (i == 0 || i == N - 1) ? y + s : y)); x += s; }
                y += s;
            }
            var rowsList = new List<Vector2[]> { v1.GetRange(0, h + 1).ToArray() };
            for (int k = 1; k <= h; k++) rowsList.Add(v1.GetRange(h + 1 + (k - 1) * N, N).ToArray());
            while (v1.Count < (h + 1) * N) v1.Add(Vector2.zero);
            c.vertices[1] = v1.ToArray();
            var v2 = new List<Vector2>();   // the top rows: each row reversed, y negated (x kept: +0 stays +0)
            foreach (var r in rowsList) for (int i = r.Length - 1; i >= 0; i--) v2.Add(new Vector2(r[i].x, -r[i].y));
            while (v2.Count < (h + 1) * N) v2.Add(Vector2.zero);
            c.vertices[2] = v2.ToArray();
            int R1 = h + 1;
            var ib1 = new List<int>();
            for (int cc = h; cc > 0; cc--)
            {
                int B = R1 + 2 * cc;
                if (cc == h) ib1.AddRange(new[] { B, cc, B - 1, cc - 1, B - 2 });
                else ib1.AddRange(new[] { B, B, cc, B - 1, cc - 1, B - 2 });
            }
            ib1.AddRange(new[] { R1, R1, R1 });
            for (int r = 1; r < h; r++)
            {
                int a0 = R1 + (r - 1) * N, b0 = R1 + r * N;
                for (int i = 0; i < N; i++) { ib1.Add(a0 + i); ib1.Add(b0 + i); }
                if (r < h - 1) { ib1.Add(b0 + N - 1); ib1.Add(b0); }
            }
            c.rows = ib1.Select(i => (ushort)i).ToArray();
            // ring columns: h + 1 columns of h + 1 vertices (x = -h s + c s, y from q s down); the right side negated
            var v3 = new List<Vector2>(); float cx = -h * s;
            for (int col = 0; col <= h; col++) { float yy = q * s; for (int j = 0; j <= h; j++) { v3.Add(new Vector2(cx, yy)); yy -= s; } cx += s; }
            c.vertices[3] = v3.ToArray();
            var v4 = new List<Vector2>();
            for (int col = 0; col <= h; col++) { float yy = -q * s; for (int j = 0; j <= h; j++) { v4.Add(new Vector2(-v3[col * (h + 1) + j].x, yy)); yy += s; } }
            c.vertices[4] = v4.ToArray();
            c.columns = ColumnStrip(false); c.columnsOdd = ColumnStrip(true);
            // drawn primitives for n rings rows / columns (tables at mesh +0x3c / +0x4c; n = 0 draws nothing)
            int stitchRows = 5 + 6 * (h - 1), stitchCols = 5 + 6 * (h / 2 - 1);
            c.rowPrims = new int[h + 1]; c.columnPrims = new int[h + 1];
            for (int n = 1; n <= h; n++)
            {
                c.rowPrims[n] = Math.Min(c.rows.Length - 2, stitchRows + (n - 1) * (2 * N + 2) - ((n - 1) & 1));
                c.columnPrims[n] = Math.Min(c.columns.Length - 2, stitchCols + (n - 1) * (2 * (h + 1) + 2));
            }
            return c;
        }

        static ushort[] ColumnStrip(bool odd)
        {
            const int h = GridH, C = GridH + 1;
            var ib = new List<int>();
            int c0 = h - (odd ? 1 : 0);
            ib.AddRange(new[] { C + h, c0, C + h - 1, c0 - 2, C + h - 2 });
            int B = C + h - 2, c = c0 - 2;
            while (B > C) { int cc = c - 2 >= 0 ? c - 2 : c; ib.AddRange(new[] { B, B, c, B - 1, cc, B - 2 }); B -= 2; c -= 2; }
            ib.AddRange(new[] { C, C, C });
            for (int r = 1; r < h; r++)
            {
                int a0 = C + (r - 1) * C, b0 = C + r * C;
                for (int i = 0; i < C; i++) { ib.Add(a0 + i); ib.Add(b0 + i); }
                if (r < h - 1) { ib.Add(b0 + C - 1); ib.Add(b0); }
            }
            return ib.Select(i => (ushort)i).ToArray();
        }

        /// The first prims primitives of a D3D triangle strip as a triangle list with the strip's winding (degenerates dropped).
        public static int[] StripToTriangles(ushort[] strip, int prims)
        {
            var t = new List<int>(prims * 3);
            for (int i = 0; i < prims && i + 2 < strip.Length; i++)
            {
                int a = strip[i], b = strip[i + 1], c = strip[i + 2];
                if (a == b || b == c || a == c) continue;
                if ((i & 1) == 0) { t.Add(a); t.Add(b); t.Add(c); } else { t.Add(b); t.Add(a); t.Add(c); }
            }
            return t.ToArray();
        }

        /// One clipmap level (the 0x50-byte record at mesh +4).
        public struct Level
        {
            public float scale, x, z, sideOffset;         // +0 2^L cell, +8 / +0xc centre (water space), +0x10 z offset of the column pieces
            public int bottom, top, left, right;          // +0x14 / +0x18 / +0x1c / +0x20 ring rows / columns per piece
        }

        public sealed class Clipmap
        {
            public readonly int count; public readonly float sizeX, sizeZ;
            public readonly Level[] levels;
            public int first;                              // mesh +0x68
            public Vector3 localCamera;
            public Clipmap(float sx, float sz)
            {
                sizeX = sx; sizeZ = sz; count = ClipmapLevelCount(sx, sz); levels = new Level[count];
                for (int l = 0; l < count; l++) levels[l].scale = (float)Math.Pow(2.0, l) * Cell;   // 007e4570: pow(2, L) * cell, centre 0
            }

            /// FUN_007e5940: the camera in water space, the first (finest) level drawn, the level centres snapped and clamped, the ring
            /// thicknesses. (Its frustum tests, which only skip pieces the camera cannot see, are left to the GPU.)
            public void Update(Vector3 camera, float[] m)
            {
                float dx = camera.x - m[12], dy = camera.y - m[13], dz = camera.z - m[14];
                float lx = (m[2] * dz + m[1] * dy) + dx * m[0];
                float ly = (m[4] * dx + m[6] * dz) + m[5] * dy;
                float lz = (m[8] * dx + m[10] * dz) + m[9] * dy;
                localCamera = new Vector3(lx, ly, lz);
                float ext = (GridN - 1) * Cell;
                first = count - 1;
                float halfTop = (float)Math.Pow(2.0, count - 1) * ext * 0.5f;
                float az = Math.Abs(lz), ay = Math.Abs(ly), ax = Math.Abs(lx), amp = 0f + 5f;   // mesh +0x24 + +0x20 (5.0)
                for (int L = count - 1; L >= 0; L--)
                {
                    float halfL = (float)Math.Pow(2.0, L) * ext * 0.5f;
                    float zz = halfTop > az ? halfTop : az, xx = halfTop > ax ? halfTop : ax, yy = ly > amp ? amp : ly;
                    float ddx = xx - halfTop, ddz = zz - halfTop, ddy = ay - yy;
                    float d = (float)Math.Sqrt(ddx * ddx + ddz * ddz + ddy * ddy);
                    if (d > halfL || L == 0) { first = L; break; }
                }
                // centres: each level snaps the camera to the next coarser level's cells (fmod chain, x87 then stored as floats)
                double cxr = lx, czr = lz;
                for (int L = count - 2; L >= first; L--)
                {
                    double sN = levels[L + 1].scale;
                    double rx = Fmod(cxr, sN), rz = Fmod(czr, sN);
                    levels[L].x = (float)(levels[L + 1].x + (cxr - rx)); cxr = (float)rx;
                    levels[L].z = (float)(levels[L + 1].z + (czr - rz)); czr = (float)rz;
                }
                float topHalf = (GridN - 1) * levels[count - 1].scale * 0.5f;
                for (int L = count - 2; L >= first; L--)
                {
                    float lim = topHalf - (GridN - 1) * levels[L].scale * 0.5f;
                    float x = levels[L].x; if (x >= lim) x = lim; if (-lim >= x) x = -lim; levels[L].x = x;
                    float z = levels[L].z; if (z >= lim) z = lim; if (-lim >= z) z = -lim; levels[L].z = z;
                }
                const int H = RingRows;
                levels[first].bottom = levels[first].top = levels[first].left = levels[first].right = H;
                levels[first].sideOffset = 0f;
                for (int L = count - 1; L > first; L--)
                {
                    int k = (int)((levels[L].x - levels[L - 1].x) / levels[L].scale);
                    if (k >= 0) { int kk = Math.Min(k, H); levels[L].left = H - kk; levels[L].right = kk + H; }
                    else { int kk = Math.Min(-k, H); levels[L].right = H - kk; levels[L].left = kk + H; }
                    int m2 = (int)((levels[L].z - levels[L - 1].z) / levels[L].scale);
                    levels[L].sideOffset = -0f - m2 * levels[L].scale;
                    if (m2 >= 0) { int mm = Math.Min(m2, H); levels[L].bottom = H - mm; levels[L].top = mm + H; }
                    else { int mm = Math.Min(-m2, H); levels[L].top = H - mm; levels[L].bottom = mm + H; }
                }
            }

            /// FUN_007e9490: the constants of the draw of piece p (0 inner, 1 / 2 ring rows, 3 / 4 ring columns) of level L.
            /// screen = the viewport (mesh +0xe0 / +0xe4), wake = the wake target's size (mesh +0xd8: 512, 256 split screen).
            public void DrawConstants(int L, int p, Vector2 screen, int wake, out Vector4 vertexOffset, out Vector4 uvOffset, out Vector4 meshResolution,
                                      out Vector4 textureWidth, out Vector4 tilingRate, out Vector4 uvMultiplier)
            {
                var lv = levels[L];
                float z = lv.z; if (p == 3 || p == 4) z = lv.sideOffset + z;
                var A = new Vector4(lv.x, z, sizeX * 0.5f, sizeZ * 0.5f);
                vertexOffset = A;                                                           // c201 gVertexOffset
                int n = count - L; if (p == 0) n++;
                float b0 = 0f, b1 = 0f;                                                     // DAT_00a95d40 / 44 (0)
                if (p == 3 || p == 4) b1 = 1f / (float)Math.Pow(2.0, n) * 0.5f;
                for (; n > 1; n--) { float t = 1f / (float)Math.Pow(2.0, n); b0 = b0 + t; b1 = t + b1; }
                float inv = 1f / ((GridN - 1) * levels[count - 1].scale);
                A = new Vector4(b0, b1, 0.5f - sizeX * inv * 0.5f, 0.5f - sizeZ * inv * 0.5f);
                uvOffset = A;                                                               // c202 gUVOffset
                float pL = (float)Math.Pow(2.0, L);
                A.x = pL; meshResolution = A;                                               // c204 gMeshResolution
                A.x = (GridN - 1) * Cell; A.y = L == 0 ? 0.5f : 1f; A.z = 1f / wake; textureWidth = A;   // c191 gTextureWidth
                A.x = (float)Math.Pow(2.0, count + 1); A.y = L == 0 ? 0.5f : L; A.z = L + 1; A.w = (GridN >> 1) * Cell * pL; tilingRate = A;   // c190 gTilingRate
                A.x = 1f / (float)Math.Pow(2.0, count - L - 1); A.y = screen.x; A.z = screen.y; uvMultiplier = A;   // c205 gUVMultiplier
            }

            /// FUN_007e9400: c203 gTileWidth (once per surface draw).
            public Vector4 TileWidth()
            {
                float t = levels[count - 1].scale * (GridN - 1);
                return new Vector4(t, t / sizeX, t / sizeZ, 0f);
            }

            /// The draws of FUN_007e98a0 in order: (level, piece, index buffer 0 inner / 1 rows / 2 columns / 3 odd columns, primitives).
            public IEnumerable<(int level, int piece, int buffer, int prims)> Draws()
            {
                var b = Buffers;
                for (int L = first; L < count; L++)
                {
                    var lv = levels[L];
                    if (L == first) yield return (L, 0, 0, b.innerPrims);
                    if (lv.bottom > 0) yield return (L, 1, 1, b.rowPrims[lv.bottom]);
                    if (lv.top > 0) yield return (L, 2, 1, b.rowPrims[lv.top]);
                    bool odd = (lv.bottom & 1) != 0 && lv.top > 0;
                    if (lv.left > 0) yield return (L, 3, odd ? 3 : 2, b.columnPrims[lv.left]);
                    if (lv.right > 0) yield return (L, 4, odd ? 3 : 2, b.columnPrims[lv.right]);
                }
            }
        }

        /// gRotationMatrix c208..c211 (FUN_007e5940 when the wake size > 1): world x / z -> the wake target's (u, v), the target
        /// centred on the camera snapped to whole metres (fmod 1): u = (x - sx) / W + 0.5, v = (z - sz) / W + 0.5, W = the wake size.
        public static Vector4[] WakeProjection(Vector3 camera, int wake)
        {
            float sx = (float)(camera.x - Fmod(camera.x, 1.0)), sz = (float)(camera.z - Fmod(camera.z, 1.0));   // x87 fmod(c, 1)
            float k = 1f / wake;
            return new[]
            {
                new Vector4(k, 0f, 0f, -sx * k + 0.5f),
                new Vector4(0f, 0f, k, -sz * k + 0.5f),
                new Vector4(0f, -1f / (wake - 1), 0f, 9f / (wake - 1)),   // depth row of the ortho view at y 10 (unused by the programs)
                new Vector4(0f, 0f, 0f, 1f),
            };
        }
        static double Fmod(double a, double b) => a - b * Math.Truncate(a / b);

        // ============================================================================================ ocean

        /// The shared ocean object (DAT_00a9578c, 0x14080 bytes, FUN_007e3b90 / 007eb420): N = 64; +0x8000 the dispersion w(k),
        /// +0xc000 / +0x10000 the spectrum h0 (real / imaginary, FUN_007ea9e0: Phillips spectrum, wind (-18, 0), g 32.2, Gaussian
        /// numbers from the CRT rand()), +0x14004 / +0x14008 the two 64 x 64 R32F height maps (used alternately), +0x14018 the time.
        /// Update is FUN_007eb4a0 once per drawn frame: h(k, t) = h0(k) e^{i w t} + conj(h0(-k)) e^{-i w t} with the original's
        /// polynomial cosine / sine, the 2D FFT (rows, then columns, radix 2 with the half-angle twiddle recurrence) and 0.2 x Re into
        /// the height map; then time += the frame's dt (wraps to 0 past 86400 s).
        public sealed class Ocean
        {
            public const int N = 64;
            public readonly float[] omega = new float[N * N], h0Re = new float[N * N], h0Im = new float[N * N];
            public float time;
            public readonly float[] heights = new float[N * N];   // the height map texels, D3D row order (row 0 first)
            readonly float[] re = new float[N * N], im = new float[N * N];

            /// From the running game's ocean object (bytes 0x8000..0x14000 and +0x14018).
            public static Ocean FromGame(byte[] block, int offset, float time)
            {
                var o = new Ocean { time = time };
                Buffer.BlockCopy(block, offset + 0x8000, o.omega, 0, N * N * 4);
                Buffer.BlockCopy(block, offset + 0xc000, o.h0Re, 0, N * N * 4);
                Buffer.BlockCopy(block, offset + 0x10000, o.h0Im, 0, N * N * 4);
                return o;
            }

            /// FUN_007ea9e0 without the running game: the same spectrum from the CRT generator (msvcr90 rand: x = x * 214013 + 2531011,
            /// (x >> 16) & 0x7fff) seeded with seed. The game draws its numbers on its level loader thread when the level's first water
            /// surface creates the ocean; the open core makes those draws and hands over the stream's state before the first one
            /// (rx_session_rng ocean_seed; 1 for the first ocean of a process). The draws go over the k grid in the ORDER of the
            /// spectrum's array (the FFT order: output row R, column C holds k of ((R + N/2) % N, (C + N/2) % N)), as 007ea9e0's loop
            /// over the shifted array does (w-rng: checked against the PX_Laguna world's h0, 4095 of 4096 bins within 1e-5).
            public static Ocean Generate(uint seed = 1)
            {
                var o = new Ocean(); uint x = seed;
                int Rand() { x = x * 214013u + 2531011u; return (int)((x >> 16) & 0x7fff); }
                const int h = N / 2;
                float windX = -18f, windY = 0f;
                float w2 = windY * windY + windX * windX, L = w2 * 0.0310559f;   // V^2 / g
                float wl = (float)Math.Sqrt(w2), wx = 1f / wl * windX, wy = 1f / wl * windY;
                for (int R = 0; R < N; R++)
                    for (int C = 0; C < N; C++)
                    {
                        int r = (R + h) % N, c = (C + h) % N;   // the k of output (R, C)
                        float kx, ky;
                        if ((r == 0 && c == h) || (c == 0 && r == h) || (c == h && r == h)) { kx = 0f; ky = 0f; }
                        else { float f = 1f / N; kx = (c - h) * f * 6.2831855f; ky = (r - h) * f * 6.2831855f; }
                        // the FFT order: the k grid is shifted by N/2 in both directions (the copies in 007ea9e0)
                        int rr = (r + h) % N, cc = (c + h) % N, i = rr * N + cc;
                        float k2 = kx * kx + ky * ky, kl = k2, nx = 0f, ny = 0f;
                        if (k2 != 0f) { kl = (float)Math.Sqrt(k2); float inv = 1f / kl; nx = kx * inv; ny = ky * inv; }
                        o.omega[i] = (float)Math.Sqrt(kl * 32.2f);
                        float kk = kl;
                        if (kk > 0f) { float dd = ny * wy + nx * wx + 1.5f; if (dd > 1f) dd = 1f; kk = dd * kk; }
                        double k4 = Math.Pow(kk, 4.0);
                        if ((float)k4 == 0f) { o.h0Re[i] = 0f; o.h0Im[i] = 0f; continue; }
                        double e = Math.Exp(-1.0 / (float)Math.Pow(kk * L, 2.0));
                        double dir = Math.Pow(ny * wy + nx * wx, 2.0);
                        float ph = (float)e / (float)k4 * (float)dir * 5e-06f;   // _DAT_009de358 (4.99999987e-06; was 0.0005 here: 10x the heights)
                        int a; do a = Rand(); while (a * 3.051851e-05f == 0f);
                        int b = Rand();
                        float g1 = (float)Math.Cos(b * 3.051851e-05f * 6.2831855f) * (float)Math.Sqrt((float)Math.Log(a * 3.051851e-05f) * -2f);
                        do a = Rand(); while (a * 3.051851e-05f == 0f);
                        b = Rand();
                        float g2 = (float)Math.Cos(b * 3.051851e-05f * 6.2831855f) * (float)Math.Sqrt((float)Math.Log(a * 3.051851e-05f) * -2f);
                        float sp = (float)Math.Sqrt(ph);
                        o.h0Re[i] = sp * 0.70710677f * g1; o.h0Im[i] = g2 * sp * 0.70710677f;
                    }
                return o;
            }

            static float Poly(float r)   // the SSE cosine polynomial of 007eb4a0 on the reduced angle r = pi - (|x| mod 2 pi)
            {
                float x2 = r * r;
                float v = (x2 * -2.197296e-07f + 2.420294e-05f) * x2 * x2 + (x2 * -0.001385879f + 0.04165978f);
                return v * x2 * x2 + (x2 * -0.4999942f + 0.9999992f);
            }
            static float Reduce(float a) { a = Math.Abs(a); return 3.1415927f - (a - (float)(int)(a * 0.15915494f) * 6.2831855f); }

            /// FUN_007eb4a0 (spectrum, FFT, height map), then the time step. Returns the time the heights were computed for.
            public float Update(float dt)
            {
                float t = time;
                for (int r = 0; r < N; r++)
                {
                    int mr = (N - r) % N;
                    for (int c = 0; c < N; c++)
                    {
                        int mc = (N - c) % N, i = r * N + c, m = mr * N + mc;
                        float wt = omega[i] * t;
                        float cs = -Poly(Reduce(wt)), sn = Poly(Reduce(wt + 1.5707964f));   // cos(w t), sin(w t)
                        float C = h0Re[i], D = h0Im[i], Cm = h0Re[m], Dm = h0Im[m];
                        re[i] = (cs * Cm + sn * Dm) + (sn * D + cs * C);
                        im[i] = (sn * Cm - cs * Dm) + (cs * D - sn * C);
                    }
                }
                Fft2();
                for (int i = 0; i < N * N; i++) heights[i] = re[i] * 0.2f;
                time = time + dt;
                if (time > 86400f) time = 0f;
                return t;
            }

            static readonly int[] BitReverse = Enumerable.Range(0, N).Select(i => { int r = 0; for (int b = 0; b < 6; b++) if ((i & (1 << b)) != 0) r |= 1 << (5 - b); return r; }).ToArray();
            readonly float[] lr = new float[N], li = new float[N];

            void Fft2()
            {
                for (int r = 0; r < N; r++)
                {
                    for (int c = 0; c < N; c++) { lr[BitReverse[c]] = re[r * N + c]; li[BitReverse[c]] = im[r * N + c]; }
                    Fft1(lr, li);
                    for (int c = 0; c < N; c++) { re[r * N + c] = lr[c]; im[r * N + c] = li[c]; }
                }
                for (int c = 0; c < N; c++)
                {
                    for (int r = 0; r < N; r++) { lr[BitReverse[r]] = re[r * N + c]; li[BitReverse[r]] = im[r * N + c]; }
                    Fft1(lr, li);
                    for (int r = 0; r < N; r++) { re[r * N + c] = lr[r]; im[r * N + c] = li[r]; }
                }
            }

            /// FUN_007ea4a0: radix-2 butterflies, twiddles w = e^{i k pi / span} by recurrence, the stage step from (cos, sin) =
            /// (-1, 0) by the half-angle square roots; t = b w, b = a - t, a = a + t (unnormalised, e^{+i}).
            static void Fft1(float[] re, float[] im)
            {
                float c = -1f, s = 0f;
                for (int span = 1; span < N; span *= 2)
                {
                    float wr = 1f, wi = 0f;
                    for (int k = 0; k < span; k++)
                    {
                        for (int i = k; i < N; i += 2 * span)
                        {
                            int j = i + span;
                            float ti = re[j] * wi + im[j] * wr, tr = re[j] * wr - im[j] * wi;
                            re[j] = re[i] - tr; im[j] = im[i] - ti;
                            re[i] = re[i] + tr; im[i] = im[i] + ti;
                        }
                        float nwr = wr * c - wi * s, nwi = wr * s + wi * c; wr = nwr; wi = nwi;
                    }
                    float nc = (float)Math.Sqrt((c + 1f) * 0.5f), ns = (float)Math.Sqrt((1f - c) * 0.5f); c = nc; s = ns;
                }
            }
        }
    }
}
