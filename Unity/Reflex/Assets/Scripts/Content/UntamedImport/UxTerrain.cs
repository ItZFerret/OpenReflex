// MX vs ATV Untamed converter, foundation: the port of untamed/tools/uxterrain.py: the mesh terrain of both games (.terrain render mesh,
// .trncol collision; Untamed big-endian, Reflex PC little-endian), the byte-order conversions, the rasteriser and the heightfield.
// numpy arrays are flat row-major arrays: double[] (float64), float[] (float32), int[] / uint[] (indices, words).
using System;
using System.Collections.Generic;

namespace Reflex.Port.Content.UntamedImport
{
    /// uxterrain.Material: one material of a terrain.
    public sealed class UxTerrainMaterial
    {
        public int nindices, nverts, ib_bytes, vb_bytes, ib_off, vb_off;
        public uint[] indices;
        /// pos (n,3), normal (n,3), tangent (n,3) float64; uv0 / uv1 (n,2) float32; color (n) uint32.
        public double[] pos, normal, tangent; public float[] uv0, uv1; public uint[] color;
        /// tris (m,3) and the zone of each triangle.
        public int[] tris; public int[] zone;
        public int TriCount => tris.Length / 3;
    }

    /// uxterrain.Terrain.
    public sealed class UxTerrainMesh
    {
        public readonly bool big; public readonly byte[] raw;
        public readonly int version, nmat, nzx, nzz, header_size, size;
        public readonly double[] centre, half; public readonly uint[] w30; public readonly byte[] decl_raw;
        /// zone_centre / zone_half (nz,3); ranges (nz, nmat, 2): (first index, primitive count).
        public readonly double[] zone_centre, zone_half; public readonly uint[] ranges;
        public readonly List<UxTerrainMaterial> materials = new List<UxTerrainMaterial>();

        public UxTerrainMesh(byte[] d)
        {
            raw = d;
            big = UxTerrain.Endian(d, UxTerrain.TERRAIN_MAGIC);
            UxBin.Require(d.Length >= 0x3C, "terrain truncated");
            version = (int)UxBin.U32(d, 4, big); nmat = (int)UxBin.U32(d, 12, big); nzx = (int)UxBin.U32(d, 16, big); nzz = (int)UxBin.U32(d, 20, big);
            centre = F64(d, 0x18, 3); half = F64(d, 0x24, 3);
            w30 = UxBin.U32s(d, 0x30, 3, big);
            UxBin.Require(w30[2] == nmat, "material count mismatch (" + w30[0] + ", " + w30[1] + ", " + w30[2] + ")");
            long nz = (long)nzx * nzz; int zo = big ? 0x1C4 : 0x194;
            UxBin.Require(nmat >= 0 && nz >= 0 && zo + 24 * nz + 8L * nmat * nz + 0x100L * nmat <= d.Length, "terrain tables outside the file");
            decl_raw = UxBin.Slice(d, 0xB4, zo - 0xB4);
            var za = F64(d, zo, (int)(6 * nz));
            zone_centre = new double[3 * nz]; zone_half = new double[3 * nz];
            for (int z = 0; z < nz; z++) for (int c = 0; c < 3; c++) { zone_centre[3 * z + c] = za[6 * z + c]; zone_half[3 * z + c] = za[6 * z + 3 + c]; }
            int ro = zo + 24 * (int)nz;
            ranges = UxBin.U32s(d, ro, (int)(2 * nmat * nz), big);
            int mo = ro + 8 * nmat * (int)nz;
            header_size = mo + 0x100 * nmat;
            int p = header_size, st = big ? 40 : 56;
            for (int k = 0; k < nmat; k++)
            {
                int ni = (int)UxBin.U32(d, mo + 0x100 * k, big), nv = (int)UxBin.U32(d, mo + 0x100 * k + 4, big), ib = (int)UxBin.U32(d, mo + 0x100 * k + 8, big), vb = (int)UxBin.U32(d, mo + 0x100 * k + 16, big);
                UxBin.Require(vb == (long)nv * st, "vertex stride is not " + st);
                UxBin.Require(ib >= 0 && (long)p + ib + vb <= d.Length, "terrain buffers outside the file");
                var m = new UxTerrainMaterial { nindices = ni, nverts = nv, ib_bytes = ib, vb_bytes = vb, ib_off = p, vb_off = p + ib };
                m.indices = UxBin.U32s(d, p, ib / 4, big);
                p += ib;
                m.pos = new double[nv * 3]; m.normal = new double[nv * 3]; m.tangent = new double[nv * 3]; m.uv0 = new float[nv * 2]; m.uv1 = new float[nv * 2]; m.color = new uint[nv];
                for (int i = 0; i < nv; i++)
                {
                    int q = p + i * st;
                    for (int c = 0; c < 3; c++) m.pos[3 * i + c] = UxBin.F32(d, q + 4 * c, big);
                    if (big)
                    {
                        UxTerrain.Dec3n(UxBin.U32BE(d, q + 12), m.normal, 3 * i);
                        m.uv0[2 * i] = UxBin.F32BE(d, q + 16); m.uv0[2 * i + 1] = UxBin.F32BE(d, q + 20); m.uv1[2 * i] = UxBin.F32BE(d, q + 24); m.uv1[2 * i + 1] = UxBin.F32BE(d, q + 28);
                        m.color[i] = UxBin.U32BE(d, q + 32);
                        UxTerrain.Dec3n(UxBin.U32BE(d, q + 36), m.tangent, 3 * i);
                    }
                    else
                    {
                        m.uv0[2 * i] = UxBin.F32LE(d, q + 12); m.uv0[2 * i + 1] = UxBin.F32LE(d, q + 16); m.uv1[2 * i] = UxBin.F32LE(d, q + 20); m.uv1[2 * i + 1] = UxBin.F32LE(d, q + 24);
                        for (int c = 0; c < 3; c++) { m.normal[3 * i + c] = UxBin.F32LE(d, q + 28 + 4 * c); m.tangent[3 * i + c] = UxBin.F32LE(d, q + 44 + 4 * c); }
                        m.color[i] = UxBin.U32LE(d, q + 40);
                    }
                }
                p += vb;
                var tl = new List<int>(); var zl = new List<int>();
                for (int z = 0; z < nz; z++)
                {
                    uint s = ranges[(z * nmat + k) * 2], c = ranges[(z * nmat + k) * 2 + 1];
                    if (c == 0) continue;
                    int before = tl.Count;
                    UxTerrain.StripTris(m.indices, s, c, tl);
                    for (int i = before; i < tl.Count; i += 3) zl.Add(z);
                }
                m.tris = tl.ToArray(); m.zone = zl.ToArray();
                materials.Add(m);
            }
            size = p;
            UxBin.Require(p == d.Length, "terrain size mismatch: parsed " + p + " of " + d.Length + " bytes");
        }
        double[] F64(byte[] d, int p, int n) { UxBin.Require(p >= 0 && (long)p + 4L * n <= d.Length, "terrain truncated"); var r = new double[n]; for (int i = 0; i < n; i++) r[i] = UxBin.F32(d, p + 4 * i, big); return r; }

        /// mesh(): all materials merged: (verts (n,3) ft, tris (m,3), material id per triangle).
        public void Mesh(out double[] V, out int[] F, out int[] M)
        {
            int nv = 0, nt = 0; foreach (var m in materials) { nv += m.pos.Length; nt += m.tris.Length; }
            V = new double[nv]; F = new int[nt]; M = new int[nt / 3];
            int vo = 0, to = 0, off = 0;
            for (int k = 0; k < materials.Count; k++)
            {
                var m = materials[k];
                Array.Copy(m.pos, 0, V, vo, m.pos.Length); vo += m.pos.Length;
                for (int i = 0; i < m.tris.Length; i++) F[to + i] = m.tris[i] + off;
                for (int i = 0; i < m.tris.Length / 3; i++) M[to / 3 + i] = k;
                to += m.tris.Length; off += m.pos.Length / 3;
            }
        }
    }

    /// uxterrain.TrnCol.
    public sealed class UxTrnCol
    {
        public readonly bool big; public readonly byte[] raw;
        public readonly int version, ntris, nverts, nlist, gx, gz;
        public readonly double cell; public readonly double[] bmin, bmax;
        /// verts (n,3) and vnormals (n,3) float64; vw (n) float32; vattr (n) uint32.
        public readonly double[] verts, vnormals; public readonly float[] vw; public readonly uint[] vattr;
        /// planes (m,4) float64; tris (m,3); flags (m) uint32; lists: the cell lists' words.
        public readonly double[] planes; public readonly int[] tris; public readonly uint[] flags, lists;

        public UxTrnCol(byte[] d)
        {
            raw = d;
            big = UxTerrain.Endian(d, UxTerrain.TRNCOL_MAGIC);
            UxBin.Require(d.Length >= 0x58, "trncol truncated");
            version = (int)UxBin.U32(d, 4, big); ntris = (int)UxBin.U32(d, 8, big); nverts = (int)UxBin.U32(d, 12, big); nlist = (int)UxBin.U32(d, 16, big);
            gx = (int)UxBin.U32(d, 20, big); gz = (int)UxBin.U32(d, 24, big);
            cell = UxBin.F32(d, 0x1C, big);
            bmin = new double[] { UxBin.F32(d, 0x20, big), UxBin.F32(d, 0x24, big), UxBin.F32(d, 0x28, big) };
            bmax = new double[] { UxBin.F32(d, 0x2C, big), UxBin.F32(d, 0x30, big), UxBin.F32(d, 0x34, big) };
            UxBin.Require(ntris >= 0 && nverts >= 0 && nlist >= 0 && 0x58 + 32L * nverts + 32L * ntris + 4L * nlist == d.Length,
                "trncol size mismatch " + (0x58 + 32L * nverts + 32L * ntris + 4L * nlist) + " vs " + d.Length);
            int p = 0x58;
            verts = new double[nverts * 3]; vnormals = new double[nverts * 3]; vw = new float[nverts]; vattr = new uint[nverts];
            for (int i = 0; i < nverts; i++)
            {
                int q = p + 32 * i;
                for (int c = 0; c < 3; c++) { verts[3 * i + c] = UxBin.F32(d, q + 4 * c, big); vnormals[3 * i + c] = UxBin.F32(d, q + 16 + 4 * c, big); }
                vw[i] = UxBin.F32(d, q + 12, big); vattr[i] = UxBin.U32(d, q + 28, big);
            }
            p += 32 * nverts;
            planes = new double[ntris * 4]; tris = new int[ntris * 3]; flags = new uint[ntris];
            for (int i = 0; i < ntris; i++)
            {
                int q = p + 32 * i;
                for (int c = 0; c < 4; c++) planes[4 * i + c] = UxBin.F32(d, q + 4 * c, big);
                for (int c = 0; c < 3; c++) tris[3 * i + c] = (int)UxBin.U32(d, q + 16 + 4 * c, big);
                flags[i] = UxBin.U32(d, q + 28, big);
            }
            p += 32 * ntris;
            lists = UxBin.U32s(d, p, nlist, big);
        }

        /// cells: the triangle list of every grid cell (cell index = ix * gz + iz).
        public List<uint[]> Cells()
        {
            var o = new List<uint[]>(); int k = 0;
            while (k < lists.Length)
            {
                int c = (int)lists[k]; int n = Math.Max(0, Math.Min(c, lists.Length - k - 1));
                var l = new uint[n]; Array.Copy(lists, k + 1, l, 0, n);
                o.Add(l); k += 1 + c;
            }
            return o;
        }
    }

    /// uxterrain.rasterize's result: (H, W) grids; NaN / -1 = no hit.
    public sealed class UxRaster
    {
        public int H, W; public double x0, z1, res;
        public float[] top, up_top, bottom, down_max, down_min, ny; public int[] nup; public long[] tri, tri_bottom;
    }

    /// uxterrain.heightfield's result.
    public sealed class UxHeightfield
    {
        public int size; public float[] height; public bool[] covered; public double x0, z1, res; public int[] excluded; public UxRaster raster;
    }

    public static class UxTerrain
    {
        public const uint TERRAIN_MAGIC = 0x1337C0DE, TRNCOL_MAGIC = 0x00BADA55;
        /// ZONE_OFF / VSTRIDE by byte order.
        public static int ZoneOff(bool big) => big ? 0x1C4 : 0x194;
        public static int VStride(bool big) => big ? 40 : 56;
        /// PC_DECL: the D3DVERTEXELEMENT9 list of the stock LD terrains.
        public static readonly byte[] PC_DECL = UxBin.FromHex("0000000002000000" + "00000c0001000500" + "0000140001000501" + "00001c0002000300" + "0000280004000a00" + "00002c0002000600" + "ff00000011000000");

        /// _endian(d, magic): true = big endian.
        public static bool Endian(byte[] d, uint magic)
        {
            UxBin.Require(d.Length >= 4, "bad magic (empty file)");
            if (UxBin.U32BE(d, 0) == magic) return true;
            if (UxBin.U32LE(d, 0) == magic) return false;
            throw new UxException("bad magic " + UxBin.U32BE(d, 0).ToString("x8") + " (want " + magic.ToString("x8") + ")");
        }

        /// dec3n(u): Xenon / D3D DEC3N (10:10:10 signed normalised, x in the low bits) -> 3 float64 at o[at..].
        public static void Dec3n(uint u, double[] o, int at)
        {
            for (int k = 0; k < 3; k++)
            {
                int v = (int)((u >> (10 * k)) & 0x3FF); if (v >= 512) v -= 1024;
                o[at + k] = Math.Max(v / 511.0, -1.0);
            }
        }

        /// strip_tris(idx, start, prims): the non-degenerate triangles of one strip draw, front face = even order (odd triangles are
        /// (a, c, b)); appended to `into` as index triples.
        public static void StripTris(uint[] idx, long start, long prims, List<int> into)
        {
            long end = Math.Min(idx.Length, start + prims + 2);          // the slice clamps
            for (long k = start; k + 2 < end; k++)
            {
                long a = idx[k], b = idx[k + 1], c = idx[k + 2];
                if (((k - start) & 1) == 1) { long t = b; b = c; c = t; }
                if (a != b && b != c && a != c) { into.Add((int)a); into.Add((int)b); into.Add((int)c); }
            }
        }

        public static UxTerrainMesh ReadTerrain(byte[] d) => new UxTerrainMesh(d);
        public static UxTrnCol ReadTrncol(byte[] d) => new UxTrnCol(d);

        /// trncol_swap(d): BE <-> LE .trncol (every field is a 32-bit word).
        public static byte[] TrncolSwap(byte[] d)
        {
            Endian(d, TRNCOL_MAGIC);
            UxBin.Require(d.Length % 4 == 0, "trncol length");
            var o = new byte[d.Length];
            for (int i = 0; i < d.Length; i += 4) { o[i] = d[i + 3]; o[i + 1] = d[i + 2]; o[i + 2] = d[i + 1]; o[i + 3] = d[i]; }
            return o;
        }

        /// terrain_to_pc(d, fill): an Untamed BE .terrain in the Reflex PC LE layout (56-byte vertices, the PC vertex declaration, the PC
        /// header offsets); a PC terrain is returned unchanged.
        public static byte[] TerrainToPc(byte[] d, byte fill = 0xCC)
        {
            var t = new UxTerrainMesh(d);
            if (!t.big) return (byte[])d.Clone();
            int nz = t.nzx * t.nzz;
            var o = new UxWriter(d.Length + d.Length / 2);
            o.U32(TERRAIN_MAGIC).U32((uint)t.version).U32(UxBin.U32BE(d, 8)).U32((uint)t.nmat).U32((uint)t.nzx).U32((uint)t.nzz);
            foreach (double v in t.centre) o.F32((float)v);
            foreach (double v in t.half) o.F32((float)v);
            foreach (uint v in t.w30) o.U32(v);
            o.Fill(fill, 120);                                              // 0x3C name scratch
            o.Bytes(PC_DECL).Fill(fill, 12 * 8 - PC_DECL.Length).Fill(fill, 128);
            UxBin.Require(o.Length == 0x194, "terrain_to_pc: header size");
            for (int z = 0; z < nz; z++) { for (int c = 0; c < 3; c++) o.F32((float)t.zone_centre[3 * z + c]); for (int c = 0; c < 3; c++) o.F32((float)t.zone_half[3 * z + c]); }
            foreach (uint v in t.ranges) o.U32(v);
            foreach (var m in t.materials)
            {
                o.U32((uint)m.nindices).U32((uint)m.nverts).U32((uint)m.ib_bytes).U32(1).U32((uint)(m.nverts * 56));
                o.Fill(fill == 0xCC ? (byte)0xCD : fill, 0x100 - 20);       // stock PC: 0xCD fill
            }
            foreach (var m in t.materials)
            {
                foreach (uint i in m.indices) o.U32(i);
                for (int i = 0; i < m.nverts; i++)
                {
                    for (int c = 0; c < 3; c++) o.F32((float)m.pos[3 * i + c]);
                    o.F32(m.uv0[2 * i]).F32(m.uv0[2 * i + 1]).F32(m.uv1[2 * i]).F32(m.uv1[2 * i + 1]);
                    for (int c = 0; c < 3; c++) o.F32((float)m.normal[3 * i + c]);
                    o.U32(m.color[i]);
                    for (int c = 0; c < 3; c++) o.F32((float)m.tangent[3 * i + c]);
                }
            }
            return o.ToArray();
        }

        // ---------------------------------------------------------------------------
        /// rasterize(verts, tris, res, origin, shape, up_min): a triangle soup rasterised onto an XZ grid (pixel centres). Row r = +Z edge
        /// first (world_z = z_max - (r + 0.5) * res), column c: world_x = x_min + (c + 0.5) * res. hasOrigin false = the grid is the
        /// soup's own bounds (origin None).
        public static UxRaster Rasterize(double[] V, int[] F, int ntris, double res = 1.0, bool hasOrigin = false, double originX = 0, double originZ1 = 0, int shapeH = 0, int shapeW = 0, double up_min = 0.05)
        {
            double x0, z0, x1, z1;
            if (!hasOrigin)
            {
                double xmin = double.PositiveInfinity, xmax = double.NegativeInfinity, zmin = double.PositiveInfinity, zmax = double.NegativeInfinity;
                for (int i = 0; i < V.Length; i += 3) { if (V[i] < xmin) xmin = V[i]; if (V[i] > xmax) xmax = V[i]; if (V[i + 2] < zmin) zmin = V[i + 2]; if (V[i + 2] > zmax) zmax = V[i + 2]; }
                x0 = Math.Floor(xmin / res) * res; z0 = Math.Floor(zmin / res) * res; x1 = Math.Ceiling(xmax / res) * res; z1 = Math.Ceiling(zmax / res) * res;
            }
            else { x0 = originX; z1 = originZ1; x1 = x0 + shapeW * res; z0 = z1 - shapeH * res; }
            int W = (int)UxNp.Round((x1 - x0) / res), H = (int)UxNp.Round((z1 - z0) / res);
            int n = H * W;
            var top = new double[n]; var upTop = new double[n]; var bot = new double[n]; var dmax = new double[n]; var dmin = new double[n];
            var nup = new int[n]; var nyg = new double[n]; var tri = new long[n]; var tbot = new long[n];
            for (int i = 0; i < n; i++) { top[i] = double.NegativeInfinity; upTop[i] = double.NegativeInfinity; bot[i] = double.PositiveInfinity; dmax[i] = double.NegativeInfinity; dmin[i] = double.PositiveInfinity; nyg[i] = double.NaN; tri[i] = -1; tbot[i] = -1; }
            double eps = -1e-9;
            for (int t = 0; t < ntris; t++)
            {
                int ia = 3 * F[3 * t], ib = 3 * F[3 * t + 1], ic = 3 * F[3 * t + 2];
                double ax = V[ia], ay = V[ia + 1], az = V[ia + 2], bx = V[ib], by = V[ib + 1], bz = V[ib + 2], cx = V[ic], cy = V[ic + 1], cz = V[ic + 2];
                double ux = bx - ax, uy = by - ay, uz = bz - az, vx = cx - ax, vy = cy - ay, vz = cz - az;
                double nx = uy * vz - uz * vy, nyv = uz * vx - ux * vz, nz = ux * vy - uy * vx;        // np.cross(B - A, C - A)
                double L = Math.Sqrt(nx * nx + nyv * nyv + nz * nz);
                double ny = L > 0 ? nyv / Math.Max(L, 1e-30) : 0;
                double xmin = Math.Min(Math.Min(ax, bx), cx), xmax = Math.Max(Math.Max(ax, bx), cx), zmin = Math.Min(Math.Min(az, bz), cz), zmax = Math.Max(Math.Max(az, bz), cz);
                long c0 = (long)Math.Ceiling((xmin - x0) / res - 0.5), c1 = (long)Math.Floor((xmax - x0) / res - 0.5);
                long r0 = (long)Math.Ceiling((z1 - zmax) / res - 0.5), r1 = (long)Math.Floor((z1 - zmin) / res - 0.5);
                if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c1 > W - 1) c1 = W - 1; if (r1 > H - 1) r1 = H - 1;
                if (!(c1 >= c0 && r1 >= r0 && Math.Abs(nyv) > 1e-12)) continue;
                double det = (bx - ax) * (cz - az) - (cx - ax) * (bz - az);
                if (Math.Abs(det) < 1e-12) continue;
                bool up = ny > up_min, down = ny < -up_min;
                for (long r = r0; r <= r1; r++)
                {
                    double pz = z1 - (r + 0.5) * res;
                    for (long c = c0; c <= c1; c++)
                    {
                        double px = x0 + (c + 0.5) * res;
                        double l1 = ((px - ax) * (cz - az) - (cx - ax) * (pz - az)) / det;
                        double l2 = ((bx - ax) * (pz - az) - (px - ax) * (bz - az)) / det;
                        double l0 = 1 - l1 - l2;
                        if (!(l0 >= eps && l1 >= eps && l2 >= eps)) continue;
                        double y = l0 * ay + l1 * by + l2 * cy;
                        int k = (int)(r * W + c);
                        if (y > top[k]) { top[k] = y; nyg[k] = ny; tri[k] = t; }
                        if (up)
                        {
                            if (y < bot[k]) tbot[k] = t;
                            if (y > upTop[k]) upTop[k] = y;       // np.maximum.at
                            if (y < bot[k]) bot[k] = y;
                            nup[k]++;
                        }
                        else if (down)
                        {
                            if (y > dmax[k]) dmax[k] = y;
                            if (y < dmin[k]) dmin[k] = y;
                        }
                    }
                }
            }
            float[] Fin(double[] g) { var o = new float[g.Length]; for (int i = 0; i < g.Length; i++) o[i] = double.IsInfinity(g[i]) || double.IsNaN(g[i]) ? UxNp.NaNf : (float)g[i]; return o; }
            var nyf = new float[n]; for (int i = 0; i < n; i++) nyf[i] = double.IsNaN(nyg[i]) ? UxNp.NaNf : (float)nyg[i];
            return new UxRaster { H = H, W = W, x0 = x0, z1 = z1, res = res, top = Fin(top), up_top = Fin(upTop), bottom = Fin(bot), down_max = Fin(dmax), down_min = Fin(dmin), nup = nup, ny = nyf, tri = tri, tri_bottom = tbot };
        }

        struct Key3 : IComparable<Key3>
        {
            public long a, b, c;
            public int CompareTo(Key3 o) => a != o.a ? a.CompareTo(o.a) : b != o.b ? b.CompareTo(o.b) : c.CompareTo(o.c);
        }
        static Key3[] Keys(double[] V, int[] F)
        {
            int n = F.Length / 3; var o = new Key3[n];
            for (int i = 0; i < n; i++)
            {
                long k0 = 0, k1 = 0, k2 = 0;
                for (int j = 0; j < 3; j++)
                {
                    int v = 3 * F[3 * i + j];
                    long x = (long)UxNp.Round(V[v] * 64), y = (long)UxNp.Round(V[v + 1] * 64), z = (long)UxNp.Round(V[v + 2] * 64);
                    long k = unchecked(x * (1000003L * 1000003L) + y * 1000003L + z);
                    if (j == 0) k0 = k; else if (j == 1) k1 = k; else k2 = k;
                }
                if (k0 > k1) { long t = k0; k0 = k1; k1 = t; }
                if (k1 > k2) { long t = k1; k1 = k2; k2 = t; }
                if (k0 > k1) { long t = k0; k0 = k1; k1 = t; }
                o[i] = new Key3 { a = k0, b = k1, c = k2 };
            }
            return o;
        }

        /// collision_materials(terrain, trncol): the render material index of every .trncol triangle (-1 = a collision-only triangle).
        /// Triangles are matched by their three vertex positions (1/64 ft).
        public static int[] CollisionMaterials(UxTerrainMesh terrain, UxTrnCol trncol)
        {
            terrain.Mesh(out var V, out var F, out var M);
            var vr = Keys(V, F); var vc = Keys(trncol.verts, trncol.tris);
            int[] o = UxNp.ArgQuickSort(vr.Length, (x, y) => vr[x].CompareTo(vr[y]));     // np.argsort(vr): numpy's (unstable) quicksort order
            var res = new int[vc.Length];
            for (int i = 0; i < vc.Length; i++)
            {
                int lo = 0, hi = vr.Length;           // np.searchsorted(vr[o], vc): the first position with vr[o][p] >= vc
                while (lo < hi) { int mid = (lo + hi) >> 1; if (vr[o[mid]].CompareTo(vc[i]) < 0) lo = mid + 1; else hi = mid; }
                int p = Math.Min(Math.Max(lo, 0), vr.Length - 1);
                res[i] = vr.Length > 0 && vr[o[p]].CompareTo(vc[i]) == 0 ? M[o[p]] : -1;
            }
            return res;
        }

        /// heightfield(trncol, tri_material, exclude, size, res, centre, y_offset, fill): a Reflex-style heightfield from the terrain
        /// collision: size x size samples at `res` ft centred on (cx, cz), row 0 = +Z edge; the highest remaining surface + y_offset;
        /// cells with no triangle take the height of the nearest covered cell (scipy distance_transform_edt's choice) when fill.
        public static UxHeightfield Heightfield(UxTrnCol trncol, int[] tri_material = null, ICollection<int> exclude = null, int size = 1024, double res = 1.0, double cx = 0.0, double cz = 0.0, double y_offset = 0.0, bool fill = true)
        {
            int nt = trncol.tris.Length / 3;
            var keep = new bool[nt]; for (int i = 0; i < nt; i++) keep[i] = true;
            if (exclude != null && exclude.Count > 0)
            {
                UxBin.Require(tri_material != null, "exclude needs tri_material (collision_materials())");
                for (int i = 0; i < nt; i++) keep[i] = !exclude.Contains(tri_material[i]);
            }
            double x0 = cx - size * res / 2, z1 = cz + size * res / 2;
            var kept = new List<int>(); var excluded = new List<int>();
            for (int i = 0; i < nt; i++) { if (keep[i]) { kept.Add(trncol.tris[3 * i]); kept.Add(trncol.tris[3 * i + 1]); kept.Add(trncol.tris[3 * i + 2]); } else excluded.Add(i); }
            var R = Rasterize(trncol.verts, kept.ToArray(), kept.Count / 3, res, true, x0, z1, size, size);
            int n = size * size;
            var h = new double[n]; var covered = new bool[n]; bool any = false, all = true;
            for (int i = 0; i < n; i++) { h[i] = R.top[i]; covered[i] = !float.IsNaN(R.top[i]) && !float.IsInfinity(R.top[i]); any |= covered[i]; all &= covered[i]; }
            if (fill && !all && any)
            {
                var notCovered = new bool[n]; for (int i = 0; i < n; i++) notCovered[i] = !covered[i];
                UxNd.DistanceTransformEdt(notCovered, size, size, out int[] ir, out int[] ic);
                var filled = new double[n];
                for (int i = 0; i < n; i++) filled[i] = h[ir[i] * size + ic[i]];
                h = filled;
            }
            var height = new float[n]; for (int i = 0; i < n; i++) { double v = h[i] + y_offset; height[i] = double.IsNaN(v) ? UxNp.NaNf : (float)v; }
            return new UxHeightfield { size = size, height = height, covered = covered, x0 = x0, z1 = z1, res = res, excluded = excluded.ToArray(), raster = R };
        }
    }

    /// scipy.ndimage pieces the converter uses, 2-D.
    public static class UxNd
    {
        /// ndimage.distance_transform_edt(mask, return_distances=False, return_indices=True) for a 2-D mask (h, w): for every element the
        /// row / column of the nearest element where mask is false (the "background"), by scipy's own algorithm (ni_morphology.c
        /// _VoronoiFT: the same nearest element when several are equally near). An element is its own nearest when mask is false there.
        /// A mask with no false element gives -1 everywhere.
        public static void DistanceTransformEdt(bool[] mask, int h, int w, out int[] row, out int[] col)
        {
            UxBin.Require(mask.Length == h * w, "distance_transform_edt: shape");
            row = new int[h * w]; col = new int[h * w];
            int len = Math.Max(h, w);
            var f0 = new long[len]; var f1 = new long[len]; var g = new long[len];
            // pass 1 (d = 0): along axis 0, one column at a time
            for (int c = 0; c < w; c++)
            {
                for (int r = 0; r < h; r++) { if (mask[r * w + c]) { row[r * w + c] = -1; col[r * w + c] = 0; } else { row[r * w + c] = r; col[r * w + c] = c; } }
                for (int r = 0; r < h; r++) { f0[r] = row[r * w + c]; f1[r] = col[r * w + c]; }
                Voronoi(f0, f1, g, h, 0, c);
                for (int r = 0; r < h; r++) { row[r * w + c] = (int)f0[r]; col[r * w + c] = (int)f1[r]; }
            }
            // pass 2 (d = 1): along axis 1, one row at a time
            for (int r = 0; r < h; r++)
            {
                for (int c = 0; c < w; c++) { f0[c] = row[r * w + c]; f1[c] = col[r * w + c]; }
                Voronoi(f0, f1, g, w, 1, r);
                for (int c = 0; c < w; c++) { row[r * w + c] = (int)f0[c]; col[r * w + c] = (int)f1[c]; }
            }
        }

        /// _VoronoiFT on one line: f0 / f1 = the feature (row, column) of each line element (f0 < 0: none); d = the line's axis; other =
        /// the line's coordinate on the other axis. The result is written back into f0 / f1.
        static void Voronoi(long[] f0, long[] f1, long[] g, int len, int d, long other)
        {
            int l = -1;
            for (int ii = 0; ii < len; ii++)
            {
                if (f0[ii] < 0) continue;
                double fd = d == 0 ? f0[ii] : f1[ii];
                double tw = (d == 0 ? f1[ii] : f0[ii]) - other;
                double wR = tw * tw;
                while (l >= 1)
                {
                    long idx1 = g[l], idx2 = g[l - 1];
                    double fa = d == 0 ? f0[idx1] : f1[idx1];
                    double a = fa - (d == 0 ? f0[idx2] : f1[idx2]);
                    double b = fd - fa;
                    double c = a + b;
                    double tu = (d == 0 ? f1[idx2] : f0[idx2]) - other, tv = (d == 0 ? f1[idx1] : f0[idx1]) - other;
                    double uR = tu * tu, vR = tv * tv;
                    if (c * vR - b * uR - a * wR - a * b * c <= 0.0) break;
                    --l;
                }
                ++l;
                g[l] = ii;
            }
            int maxl = l;
            if (maxl < 0) return;
            // the features are read through g while the line is rewritten: keep a copy of the originals
            var o0 = new long[maxl + 1]; var o1 = new long[maxl + 1];
            for (int k = 0; k <= maxl; k++) { o0[k] = f0[g[k]]; o1[k] = f1[g[k]]; }
            l = 0;
            for (int ii = 0; ii < len; ii++)
            {
                double Delta(int k)
                {
                    double t0 = d == 0 ? o0[k] - ii : o0[k] - other, t1 = d == 1 ? o1[k] - ii : o1[k] - other;
                    return t0 * t0 + t1 * t1;
                }
                double delta1 = Delta(l);
                while (l < maxl)
                {
                    double delta2 = Delta(l + 1);
                    if (delta1 <= delta2) break;
                    delta1 = delta2;
                    ++l;
                }
                f0[ii] = o0[l]; f1[ii] = o1[l];
            }
        }

        /// The distances of distance_transform_edt(mask) (float64): sqrt((r - row)^2 + (c - col)^2) of the nearest false element.
        public static double[] DistanceTransformEdtDistances(bool[] mask, int h, int w)
        {
            DistanceTransformEdt(mask, h, w, out var row, out var col);
            var o = new double[h * w];
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++)
                {
                    double dr = row[r * w + c] - r, dc = col[r * w + c] - c;
                    o[r * w + c] = Math.Sqrt(dr * dr + dc * dc);
                }
            return o;
        }

        /// ndimage.binary_dilation(mask, np.ones((3, 3), bool), iterations=n) / binary_dilation(mask, iterations=n) (cross = the default
        /// 4-connected structure) for a 2-D mask, border value 0.
        public static bool[] BinaryDilation(bool[] mask, int h, int w, int iterations = 1, bool cross = false)
        {
            var cur = (bool[])mask.Clone();
            for (int it = 0; it < iterations; it++)
            {
                var next = new bool[h * w];
                for (int r = 0; r < h; r++)
                    for (int c = 0; c < w; c++)
                    {
                        bool v = false;
                        for (int dr = -1; dr <= 1 && !v; dr++)
                            for (int dc = -1; dc <= 1 && !v; dc++)
                            {
                                if (cross && dr != 0 && dc != 0) continue;
                                int rr = r + dr, cc = c + dc;
                                if (rr >= 0 && rr < h && cc >= 0 && cc < w && cur[rr * w + cc]) v = true;
                            }
                        next[r * w + c] = v;
                    }
                cur = next;
            }
            return cur;
        }
    }
}
