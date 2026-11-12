// MX vs ATV Untamed converter, foundation: the port of untamed/tools/uxmesh.py, the reader of Untamed's (Xbox 360, big-endian) mesh
// resources: .surface (v4 groups, Xenon vertex declarations), .model (static, skinned, rigid multi-node), .collis (kinds 0, 2, 11, 15,
// 17, 19), and the conversions to the Reflex PC layouts (model byte swap, primitive collision, 44-byte and crowd vertices).
// numpy arrays are flat row-major arrays here: float[] for float32, double[] for float64, int[] for index arrays (int64 in Python).
using System;
using System.Collections.Generic;

namespace Reflex.Port.Content.UntamedImport
{
    /// One element of a group's vertex declaration: (stream, offset, type, method, usage, usage index).
    public sealed class UxDeclElem
    {
        public int stream, offset, method, usage, usageIndex; public uint type;
        public string TypeName => UxMesh.XDECL.TryGetValue(type, out var d) ? d.name : "0x" + type.ToString("x");
    }

    /// decode_element's result: n rows of `comps` values, float32 (f) or uint8 (u8: D3DCOLOR as R G B A, UBYTE4).
    public sealed class UxVal
    {
        public float[] f; public byte[] u8; public int n, comps;
        public bool IsFloat => f != null;
        /// val[:, :count] as a new float32 array (the element must be a float kind).
        public float[] Floats(int count)
        {
            UxBin.Require(f != null, "vertex element is not a float kind");
            if (count == comps) return f;
            UxBin.Require(count <= comps, "vertex element has " + comps + " components, " + count + " wanted");
            var r = new float[n * count];
            for (int i = 0; i < n; i++) for (int c = 0; c < count; c++) r[i * count + c] = f[i * comps + c];
            return r;
        }
    }

    /// uxmesh.Group: one render group of a surface.
    public sealed class UxGroup
    {
        public uint flags; public List<UxDeclElem> decl; public int[] strip; public int npal; public byte[] palette;
        public int prim, nverts;
        /// pos (n,3), nrm (n,3), tan (n,3) float32; pos_w (n) when the position has 4 components; null when the group has no such element.
        public float[] pos, pos_w, nrm, tan;
        /// uv0 / uv1 (n, comps) as decoded (FLOAT16_2: 2 components), col (n,4 uint8 R G B A), bidx / bwgt (skinned groups).
        public UxVal uv0, uv1, col, bidx, bwgt;
        /// tris (m,3): the strip's front-facing triangles (parity applied, degenerates dropped).
        public int[] tris;
        public int[] strides;
        public int TriCount => tris.Length / 3;
    }

    /// uxmesh.Model.
    public sealed class UxModel
    {
        public bool skinned, boned, rigid;
        public uint[] hdr; public string name, name2; public int ngroups, nbones;
        public uint draw_distance; public float[] matrix; public float radius; public float[] center, extents;
        public uint[][] lods, recs; public byte[] flags; public uint[] order; public string[] materials; public byte[] tail;
        public List<float[]> node_matrices; public int[] node_words; public List<string> node_names;
    }

    /// A BVH node of a kind 0 / 2 collis: f volume, f3 centre, f3 half extents, u16 child0, child1.
    public struct UxCollisNode { public float vol, cx, cy, cz, hx, hy, hz; public ushort k0, k1; }

    /// A kind-15 primitive: type 5 box, 7 sphere, 8 cylinder, 9 (three floats, Untamed only).
    public sealed class UxCollisPrim
    {
        public uint type, m1; public double[] matrix; public double[] @params; public byte[] raw;
    }

    /// uxmesh.read_collis's dict.
    public sealed class UxCollis
    {
        public uint kind, version; public int size, tail;
        // kind 0: matrix (4,3), verts (nv,3), tri planes d / n and indices, tris / local_tris (nt,3,3)
        // kind 2: matrix (4,4), points / point_ref (n,3), nleaf
        public double[] matrix, verts, tris, local_tris, points, point_ref;
        public UxCollisNode[] nodes; public float[] tri_d, tri_n; public int[] tri_i; public uint root, z1, z2, nleaf;
        public int TriCount => tris == null ? 0 : tris.Length / 9;
        public double[] half; public byte[] pad;                 // kind 11
        public byte[] tags; public List<UxCollisPrim> prims;     // kind 15
        public double radius, half_height;                        // kind 17
        public uint count;                                       // kind 19
    }

    public static class UxMesh
    {
        public const uint SURF_MAGIC = 0x99BBCC00, MODEL_MAGIC = 0x88AABB00, COLLIS_MAGIC = 1234;

        /// XDECL: Xenon D3DDECLTYPE -> (name, components, decoder, bytes).
        public static readonly Dictionary<uint, (string name, int comps, string dt, int nb)> XDECL = new Dictionary<uint, (string, int, string, int)>
        {
            { 0x1A2360, ("FLOAT16_4", 4, ">f2", 8) }, { 0x2C235F, ("FLOAT16_2", 2, ">f2", 4) }, { 0x2A23B9, ("FLOAT3", 3, ">f4", 12) },
            { 0x1A23A6, ("FLOAT4", 4, ">f4", 16) }, { 0x2C23A5, ("FLOAT2", 2, ">f4", 8) }, { 0x2C83A4, ("FLOAT1", 1, ">f4", 4) },
            { 0x2A2187, ("DEC3N", 3, "dec3n", 4) }, { 0x1A2187, ("DEC4N", 4, "dec4n", 4) }, { 0x182886, ("D3DCOLOR", 4, "d3dcolor", 4) },
            { 0x1A2286, ("UBYTE4", 4, "u1", 4) }, { 0x1A2086, ("UBYTE4N", 4, "u1n", 4) }, { 0x1A235A, ("SHORT4", 4, ">i2", 8) },
            { 0x2C2359, ("SHORT2", 2, ">i2", 4) }, { 0x1A215A, ("SHORT4N", 4, ">i2n", 8) }, { 0x2C2159, ("SHORT2N", 2, ">i2n", 4) },
        };
        /// USAGE[usage].
        public static string UsageName(int usage)
        {
            switch (usage)
            {
                case 0: return "POSITION"; case 1: return "BLENDWEIGHT"; case 2: return "BLENDINDICES"; case 3: return "NORMAL"; case 4: return "PSIZE";
                case 5: return "TEXCOORD"; case 6: return "TANGENT"; case 7: return "BINORMAL"; case 10: return "COLOR";
            }
            return usage.ToString(System.Globalization.CultureInfo.InvariantCulture);
        }

        static int Sext10(uint v) { int x = (int)(v & 0x3FF); return x >= 512 ? x - 1024 : x; }

        /// decode_element(raw, kind) for n elements of `stride` bytes apart starting at data[start + off].
        public static UxVal DecodeElement(byte[] data, int start, int stride, int n, uint kind)
        {
            if (!XDECL.TryGetValue(kind, out var d)) throw new UxException("unknown Xenon decl type 0x" + kind.ToString("x"));
            var r = new UxVal { n = n, comps = d.comps };
            switch (d.dt)
            {
                case "dec3n": case "dec4n":
                {
                    r.f = new float[n * d.comps];
                    for (int i = 0; i < n; i++)
                    {
                        uint v = UxBin.U32BE(data, start + i * stride); int o = i * d.comps;
                        r.f[o] = Math.Max(Sext10(v) / 511.0f, -1.0f); r.f[o + 1] = Math.Max(Sext10(v >> 10) / 511.0f, -1.0f); r.f[o + 2] = Math.Max(Sext10(v >> 20) / 511.0f, -1.0f);
                        if (d.comps == 4) { int w = (int)(v >> 30); if (w >= 2) w -= 4; r.f[o + 3] = Math.Max(w, -1); }
                    }
                    return r;
                }
                case "d3dcolor":        // u32 ARGB big-endian = bytes A R G B -> R G B A
                    r.u8 = new byte[n * 4];
                    for (int i = 0; i < n; i++) { int p = start + i * stride; r.u8[4 * i] = data[p + 1]; r.u8[4 * i + 1] = data[p + 2]; r.u8[4 * i + 2] = data[p + 3]; r.u8[4 * i + 3] = data[p]; }
                    return r;
                case "u1":
                    r.u8 = new byte[n * 4];
                    for (int i = 0; i < n; i++) Buffer.BlockCopy(data, start + i * stride, r.u8, 4 * i, 4);
                    return r;
                case "u1n":
                    r.f = new float[n * 4];
                    for (int i = 0; i < n; i++) for (int c = 0; c < 4; c++) r.f[4 * i + c] = data[start + i * stride + c] / 255.0f;
                    return r;
                case ">f2":
                    r.f = new float[n * d.comps];
                    for (int i = 0; i < n; i++) for (int c = 0; c < d.comps; c++) r.f[i * d.comps + c] = UxNp.HalfToFloat(UxBin.U16BE(data, start + i * stride + 2 * c));
                    return r;
                case ">f4":
                    r.f = new float[n * d.comps];
                    for (int i = 0; i < n; i++) for (int c = 0; c < d.comps; c++) r.f[i * d.comps + c] = UxBin.F32BE(data, start + i * stride + 4 * c);
                    return r;
                case ">i2":
                    r.f = new float[n * d.comps];
                    for (int i = 0; i < n; i++) for (int c = 0; c < d.comps; c++) r.f[i * d.comps + c] = UxBin.I16(data, start + i * stride + 2 * c, true);
                    return r;
                case ">i2n":
                    r.f = new float[n * d.comps];
                    for (int i = 0; i < n; i++) for (int c = 0; c < d.comps; c++) r.f[i * d.comps + c] = Math.Max(UxBin.I16(data, start + i * stride + 2 * c, true) / 32767.0f, -1.0f);
                    return r;
            }
            throw new UxException("decl decoder " + d.dt);
        }

        /// strip_to_tris(strip): (m,3) front-facing triangles (odd triangles flipped, degenerates removed).
        public static int[] StripToTris(int[] s)
        {
            if (s.Length < 3) return new int[0];
            var t = new List<int>((s.Length - 2) * 3);
            for (int k = 0; k + 2 < s.Length; k++)
            {
                int a = s[k], b = s[k + 1], c = s[k + 2];
                int t0 = (k & 1) == 1 ? b : a, t1 = (k & 1) == 1 ? a : b;
                if (t0 != t1 && t1 != c && t0 != c) { t.Add(t0); t.Add(t1); t.Add(c); }
            }
            return t.ToArray();
        }

        /// read_surface(data): the groups of an Untamed v4 surface.
        public static List<UxGroup> ReadSurface(byte[] data)
        {
            UxBin.Require(data.Length >= 16, "not an Untamed v4 surface (truncated)");
            uint magic = UxBin.U32BE(data, 0), ver = UxBin.U32BE(data, 4); int ng = (int)UxBin.U32BE(data, 8);
            if (magic != SURF_MAGIC || ver != 4) throw new UxException("not an Untamed v4 surface (magic " + magic.ToString("x8") + " ver " + ver + ")");
            UxBin.Require(ng >= 0 && 0x10 + (long)ng * 0x160 <= data.Length, "surface: group records outside the file");
            int p = 0x10 + ng * 0x160;
            var groups = new List<UxGroup>(ng);
            for (int gi = 0; gi < ng; gi++)
            {
                int r = 0x10 + gi * 0x160;
                uint flags = UxBin.U32BE(data, r + 8); int prim = (int)UxBin.U32BE(data, r + 12), nv = (int)UxBin.U32BE(data, r + 16), ib = (int)UxBin.U32BE(data, r + 20);
                var decl = new List<UxDeclElem>();
                int q = 0x18;
                while (q + 12 <= 0x108)
                {
                    int st = UxBin.U16BE(data, r + q), off = UxBin.U16BE(data, r + q + 2); uint ty = UxBin.U32BE(data, r + q + 4);
                    if (st == 0xFF || ty == 0xFFFFFFFF) break;
                    decl.Add(new UxDeclElem { stream = st, offset = off, type = ty, method = data[r + q + 8], usage = data[r + q + 9], usageIndex = data[r + q + 10] });
                    q += 12;
                }
                int nstreams = (int)UxBin.U32BE(data, r + 0x108);
                UxBin.Require(nstreams >= 0 && 0x10C + 4 * (long)nstreams <= 0x160, "surface group " + gi + ": " + nstreams + " streams");
                var sbytes = new int[nstreams];
                for (int i = 0; i < nstreams; i++) sbytes[i] = (int)UxBin.U32BE(data, r + 0x10C + 4 * i);
                int npal = flags != 0 ? (int)UxBin.U32BE(data, r + 0x14C) : 0;
                if (ib != 2 * (prim + 2) && prim != 0) throw new UxException("group " + gi + ": index bytes " + ib + " do not match a strip of " + prim + " prims");
                UxBin.Require(npal >= 0 && ib >= 0 && (long)p + npal + ib <= data.Length, "surface group " + gi + ": indices outside the file");
                var g = new UxGroup { flags = flags, decl = decl, npal = npal, prim = prim, nverts = nv };
                g.palette = UxBin.Slice(data, p, npal); p += npal;
                g.strip = new int[ib / 2];
                for (int i = 0; i < g.strip.Length; i++) g.strip[i] = UxBin.U16BE(data, p + 2 * i);
                p += ib;
                var sstart = new int[nstreams];
                for (int i = 0; i < nstreams; i++)
                {
                    UxBin.Require(sbytes[i] >= 0 && (long)p + sbytes[i] <= data.Length, "surface group " + gi + ": vertex stream outside the file");
                    sstart[i] = p; p += sbytes[i];
                }
                foreach (var e in decl)
                {
                    if (!XDECL.TryGetValue(e.type, out var xd)) throw new UxException("unknown Xenon decl type 0x" + e.type.ToString("x"));
                    UxBin.Require(e.stream < nstreams, "surface group " + gi + ": element in stream " + e.stream);
                    int stride = nv != 0 ? sbytes[e.stream] / nv : 0;
                    UxBin.Require(nv == 0 || (stride * nv == sbytes[e.stream] && e.offset + xd.nb <= stride), "surface group " + gi + ": vertex stream size / element offset");
                    var val = DecodeElement(data, sstart[e.stream] + e.offset, stride, nv, e.type);
                    if (e.usage == 0) { g.pos = val.Floats(3); g.pos_w = null; if (val.comps == 4) { g.pos_w = new float[nv]; for (int i = 0; i < nv; i++) g.pos_w[i] = val.f[4 * i + 3]; } }
                    else if (e.usage == 3) g.nrm = val.Floats(3);
                    else if (e.usage == 5 && e.usageIndex == 0) g.uv0 = val;
                    else if (e.usage == 5 && e.usageIndex == 1) g.uv1 = val;
                    else if (e.usage == 10) g.col = val;
                    else if (e.usage == 6) g.tan = val.Floats(3);
                    else if (e.usage == 2) g.bidx = val;
                    else if (e.usage == 1) g.bwgt = val;
                }
                g.tris = StripToTris(g.strip);
                g.strides = new int[nstreams];
                for (int i = 0; i < nstreams; i++) g.strides[i] = nv != 0 ? sbytes[i] / nv : 0;
                groups.Add(g);
            }
            if (p != data.Length) throw new UxException((data.Length - p) + " unparsed bytes at the end of the surface");
            return groups;
        }

        // ------------------------------------------------------------------------------------------- model
        static uint[] U32s(byte[] d, int p, int n) { UxBin.Require(p >= 0 && (long)p + 4L * n <= d.Length, "model: data outside the file"); return UxBin.U32s(d, p, n, true); }
        static float[] F32s(byte[] d, int p, int n) { UxBin.Require(p >= 0 && (long)p + 4L * n <= d.Length, "model: data outside the file"); return UxBin.F32s(d, p, n, true); }
        static string BlobName(byte[] blob, long at) => at >= blob.Length ? "" : UxBin.CStr(blob, (int)at);

        /// read_model(data): a static model whole; a skinned / multi-node model up to its name (skinned = true).
        public static UxModel ReadModel(byte[] data)
        {
            var h = U32s(data, 0, 10);
            if (h[0] != MODEL_MAGIC || h[1] != 4) throw new UxException("not an Untamed v4 model");
            var m = new UxModel();
            m.skinned = h[2] != 0;
            m.boned = h[2] == 0 && h[3] != 1;
            if (m.boned) m.skinned = true;
            int namelen = (int)h[4], nlods = (int)h[5], nbytes = (int)h[6], ng = (int)h[7];
            m.hdr = h;
            m.name = UxBin.CStr(data, 0x28, 0x80);
            m.ngroups = ng;
            if (m.skinned) { m.nbones = (int)h[3]; return m; }
            m.draw_distance = U32s(data, 0xA8, 1)[0];
            m.matrix = F32s(data, 0xAC, 16);
            m.radius = F32s(data, 0xF4, 1)[0];
            m.center = F32s(data, 0xF8, 4);
            m.extents = F32s(data, 0x108, 4);
            int p = 0x118;
            m.name2 = UxBin.CStr(data, p, namelen); p += namelen;
            ReadModelTail(m, data, ref p, nlods, ng, nbytes);
            m.tail = UxBin.PySlice(data, p, data.Length);
            return m;
        }
        static void ReadModelTail(UxModel m, byte[] data, ref int p, int nlods, int ng, int nbytes)
        {
            m.lods = new uint[nlods][]; for (int i = 0; i < nlods; i++) m.lods[i] = U32s(data, p + 12 * i, 3); p += 12 * nlods;
            m.recs = new uint[ng][]; for (int i = 0; i < ng; i++) m.recs[i] = U32s(data, p + 20 * i, 5); p += 20 * ng;
            byte[] blob = UxBin.PySlice(data, p, p + nbytes); p += nbytes;
            m.flags = UxBin.PySlice(data, p, p + ng); p += ng;
            m.order = U32s(data, p, ng); p += 4 * ng;
            m.materials = new string[ng];
            for (int i = 0; i < ng; i++) m.materials[i] = BlobName(blob, m.recs[i][0]);
        }

        /// read_model_rigid(data): a rigid multi-node model (h[2] == 0, h[3] = n > 1 nodes) with the static fields; matrix = node 0.
        public static UxModel ReadModelRigid(byte[] data)
        {
            var h = U32s(data, 0, 10);
            if (h[0] != MODEL_MAGIC || h[1] != 4 || h[2] != 0 || h[3] < 2) throw new UxException("not a rigid multi-node v4 model");
            long n = h[3];
            long namelen = h[4], nlods = h[5], nbytes = h[6], ng = h[7];
            long tail = 12 * nlods + 20 * ng + nbytes + ng + 4 * ng;
            long p = data.Length - tail, q = p - namelen;
            long baseOff = 0xAC + 64 * n, rOff = baseOff + 4 * n;
            if (q < rOff + 36 || p > data.Length) throw new UxException("rigid model layout: names at " + q + " before the bounds block end " + (rOff + 36));
            var m = new UxModel { skinned = false, boned = true, rigid = true, hdr = h, name = UxBin.CStr(data, 0x28, 0x80), ngroups = (int)ng, nbones = (int)n };
            m.draw_distance = U32s(data, 0xA8, 1)[0];
            m.node_matrices = new List<float[]>();
            for (int k = 0; k < n; k++) m.node_matrices.Add(F32s(data, 0xAC + 64 * k, 16));
            var words = U32s(data, (int)baseOff, (int)n); m.node_words = new int[n]; for (int k = 0; k < n; k++) m.node_words[k] = (int)words[k];
            m.matrix = m.node_matrices[0];
            m.radius = F32s(data, (int)rOff, 1)[0];
            m.center = F32s(data, (int)rOff + 4, 4);
            m.extents = F32s(data, (int)rOff + 20, 4);
            var names = new List<string>();
            int start = (int)q;
            for (int i = (int)q; i <= p; i++)
                if (i == p || data[i] == 0) { if (i > start) names.Add(UxBin.Latin1(data, start, i - start)); start = i + 1; }
            if (names.Count != n) throw new UxException("rigid model layout: " + names.Count + " node names, header says " + n + " nodes");
            m.node_names = names; m.name2 = names[0];
            int pp = (int)p;
            ReadModelTail(m, data, ref pp, (int)nlods, (int)ng, (int)nbytes);
            if (pp != data.Length) throw new UxException("rigid model layout: " + (data.Length - pp) + " trailing bytes");
            m.tail = new byte[0];
            return m;
        }

        /// rigid_static_problems(m, groups, tol): why a rigid multi-node model cannot be drawn as a static model ([] = it can).
        public static List<string> RigidStaticProblems(UxModel m, List<UxGroup> groups, double tol = 1e-4)
        {
            var o = new List<string>();
            for (int k = 0; k < m.node_matrices.Count; k++)
            {
                bool close = true;
                for (int i = 0; i < 16 && close; i++)
                {   // np.allclose(M, eye(4), atol=tol): |a - b| <= atol + 1e-5 * |b|
                    double b = i % 5 == 0 ? 1.0 : 0.0, a = m.node_matrices[k][i];
                    close = !double.IsNaN(a) && !double.IsInfinity(a) && Math.Abs(a - b) <= tol + 1e-5 * Math.Abs(b);
                }
                if (!close) o.Add("node " + k + " (" + m.node_names[k] + ") matrix is not the identity");
            }
            if (groups.Count != m.ngroups) o.Add("surface has " + groups.Count + " groups, model " + m.ngroups);
            for (int gi = 0; gi < groups.Count; gi++)
                if (groups[gi].flags != 0 || groups[gi].bidx != null || groups[gi].bwgt != null) o.Add("surface group " + gi + " is skinned");
            return o;
        }

        /// model_to_reflex(data): a static Untamed .model byte-swapped into the Reflex PC (little-endian) layout.
        public static byte[] ModelToReflex(byte[] data)
        {
            var h = U32s(data, 0, 10);
            if (h[2] != 0 || h[3] != 1) throw new UxException("skinned or multi-node model");
            int namelen = (int)h[4], nlods = (int)h[5], nbytes = (int)h[6], ng = (int)h[7];
            var o = (byte[])data.Clone();
            void Sw(int off, int n)
            {
                UxBin.Require(off >= 0 && (long)off + 4L * n <= data.Length, "model: data outside the file");
                for (int i = 0; i < n; i++) { int p2 = off + 4 * i; o[p2] = data[p2 + 3]; o[p2 + 1] = data[p2 + 2]; o[p2 + 2] = data[p2 + 1]; o[p2 + 3] = data[p2]; }
            }
            Sw(0, 10);
            Sw(0xA8, (0x118 - 0xA8) / 4);
            int p = 0x118 + namelen;
            Sw(p, 3 * nlods); p += 12 * nlods;
            Sw(p, 5 * ng); p += 20 * ng;
            p += nbytes + ng;
            Sw(p, ng); p += 4 * ng;
            if (p != data.Length) throw new UxException("model tail " + (data.Length - p) + " bytes");
            return o;
        }

        // ------------------------------------------------------------------------------------------ collis
        /// PRIM_SIZE[type]: 5 box 88, 7 sphere 76, 8 cylinder 80, 9 (three floats) 84; 0 = unknown.
        public static int PrimSize(uint type) => type == 5 ? 88 : type == 7 ? 76 : type == 8 ? 80 : type == 9 ? 84 : 0;
        /// PRIM_NAME[type].
        public static string PrimName(uint type) => type == 5 ? "box" : type == 7 ? "sphere" : type == 8 ? "cylinder" : type == 9 ? "type9" : null;

        static UxCollisNode[] Nodes(byte[] d, int p, int n)
        {
            UxBin.Require(n >= 0 && (long)p + 32L * n <= d.Length, "collis: nodes outside the file");
            var a = new UxCollisNode[n];
            for (int i = 0; i < n; i++)
            {
                int q = p + 32 * i;
                a[i] = new UxCollisNode { vol = UxBin.F32BE(d, q), cx = UxBin.F32BE(d, q + 4), cy = UxBin.F32BE(d, q + 8), cz = UxBin.F32BE(d, q + 12),
                    hx = UxBin.F32BE(d, q + 16), hy = UxBin.F32BE(d, q + 20), hz = UxBin.F32BE(d, q + 24), k0 = UxBin.U16BE(d, q + 28), k1 = UxBin.U16BE(d, q + 30) };
            }
            return a;
        }
        static double[] F64s(byte[] d, int p, int n)
        {
            UxBin.Require(p >= 0 && n >= 0 && (long)p + 4L * n <= d.Length, "collis: data outside the file");
            var r = new double[n]; for (int i = 0; i < n; i++) r[i] = UxBin.F32BE(d, p + 4 * i); return r;
        }

        /// read_collis(data). Kinds 0 and 2 also give tris (world space (n,3,3)) or points.
        public static UxCollis ReadCollis(byte[] data)
        {
            UxBin.Require(data.Length >= 16, "not an Untamed collis");
            uint mag = UxBin.U32BE(data, 0), ver = UxBin.U32BE(data, 8), kind = UxBin.U32BE(data, 12);
            if (mag != COLLIS_MAGIC || data[4] != 'c' || data[5] != 'o' || data[6] != 'l' || data[7] != 0 || (ver != 2 && ver != 3)) throw new UxException("not an Untamed collis");
            var d = new UxCollis { kind = kind, size = data.Length, version = ver };
            if (kind == 0)
            {
                var m = F64s(data, 0x14, 12);
                UxBin.Require(data.Length >= 0x58, "collis kind 0: truncated");
                d.z1 = UxBin.U32BE(data, 0x44); int nv = (int)UxBin.U32BE(data, 0x48), nn = (int)UxBin.U32BE(data, 0x4C), nt = (int)UxBin.U32BE(data, 0x50); d.z2 = UxBin.U32BE(data, 0x54);
                int p = 0x58;
                var v = F64s(data, p, nv * 3); p += 12 * nv;
                UxBin.Require(p + 4 <= data.Length, "collis kind 0: truncated");
                d.root = UxBin.U32BE(data, p); p += 4;
                d.nodes = Nodes(data, p, nn); p += 32 * nn;
                UxBin.Require(nt >= 0 && (long)p + 22L * nt <= data.Length, "collis kind 0: triangles outside the file");
                d.tri_d = new float[nt]; d.tri_n = new float[nt * 3]; d.tri_i = new int[nt * 3];
                for (int i = 0; i < nt; i++)
                {
                    int q = p + 22 * i;
                    d.tri_d[i] = UxBin.F32BE(data, q);
                    for (int c = 0; c < 3; c++) { d.tri_n[3 * i + c] = UxBin.F32BE(data, q + 4 + 4 * c); d.tri_i[3 * i + c] = UxBin.U16BE(data, q + 16 + 2 * c); }
                }
                p += 22 * nt;
                d.matrix = m; d.verts = v; d.tail = data.Length - p;
                // world = v @ m[:3] + m[3]  (numpy: dgemm, fused multiply-add chain)
                var rot = new double[9]; Array.Copy(m, rot, 9);
                var world = UxNp.MatMul(v, nv, 3, rot, 3);
                for (int i = 0; i < nv; i++) for (int c = 0; c < 3; c++) world[3 * i + c] += m[9 + c];
                d.tris = new double[nt * 9]; d.local_tris = new double[nt * 9];
                for (int i = 0; i < nt * 3; i++)
                {
                    int vi = d.tri_i[i];
                    UxBin.Require(vi < nv, "collis kind 0: vertex index out of range");
                    for (int c = 0; c < 3; c++) { d.tris[3 * i + c] = world[3 * vi + c]; d.local_tris[3 * i + c] = v[3 * vi + c]; }
                }
            }
            else if (kind == 2)
            {
                d.matrix = F64s(data, 0x14, 16);
                UxBin.Require(data.Length >= 0x60, "collis kind 2: truncated");
                int npnt = (int)UxBin.U32BE(data, 0x54), nn = (int)UxBin.U32BE(data, 0x58); d.nleaf = UxBin.U32BE(data, 0x5C);
                int p = 0x60;
                d.nodes = Nodes(data, p, nn); p += 32 * nn;
                var pts = F64s(data, p, npnt * 6); p += 24 * npnt;
                d.points = new double[npnt * 3]; d.point_ref = new double[npnt * 3];
                for (int i = 0; i < npnt; i++) for (int c = 0; c < 3; c++) { d.point_ref[3 * i + c] = pts[6 * i + c]; d.points[3 * i + c] = pts[6 * i + 3 + c]; }
                d.tail = data.Length - p;
            }
            else if (kind == 11)
            {
                d.half = F64s(data, 0x14, 3); d.pad = UxBin.PySlice(data, 0x20, 0x24);
                d.tail = data.Length - 0x24;
            }
            else if (kind == 15)
            {
                if (ver == 2) throw new UxException("collis version 2 kind 15 is not decoded");
                UxBin.Require(data.Length >= 0x1C, "collis kind 15: truncated");
                int n = (int)UxBin.U32BE(data, 0x14), ntag = (int)UxBin.U32BE(data, 0x18); int p = 0x1C + ntag;
                d.tags = UxBin.PySlice(data, p - ntag, p);
                d.prims = new List<UxCollisPrim>();
                for (int i = 0; i < n; i++)
                {
                    UxBin.Require(p + 4 <= data.Length, "collis kind 15: truncated");
                    uint ty = UxBin.U32BE(data, p); int size = PrimSize(ty);
                    if (size == 0) throw new UxException("kind 15: unknown primitive type " + ty + " at 0x" + p.ToString("x"));
                    UxBin.Require(p + size <= data.Length, "collis kind 15: primitive outside the file");
                    d.prims.Add(new UxCollisPrim { type = ty, matrix = F64s(data, p + 4, 16), m1 = UxBin.U32BE(data, p + 68), @params = F64s(data, p + 72, (size - 72) / 4), raw = UxBin.Slice(data, p, size) });
                    p += size;
                }
                d.tail = data.Length - p;
            }
            else if (kind == 17)
            {
                var f = F64s(data, 0x14, 2); d.radius = f[0]; d.half_height = f[1]; d.tail = data.Length - 0x1C;
            }
            else if (kind == 19)
            {
                UxBin.Require(data.Length >= 0x18, "collis kind 19: truncated");
                d.count = UxBin.U32BE(data, 0x14);
            }
            return d;
        }

        /// swap_collis_kind2(data): Untamed kind 2 (male point set) -> Reflex PC little-endian kind 2.
        public static byte[] SwapCollisKind2(byte[] data)
        {
            UxBin.Require(data.Length >= 0x60 && UxBin.U32BE(data, 12) == 2, "not a kind-2 collis");
            var o = new byte[data.Length];
            for (int i = 0; i + 4 <= data.Length - data.Length % 4; i += 4) { o[i] = data[i + 3]; o[i + 1] = data[i + 2]; o[i + 2] = data[i + 1]; o[i + 3] = data[i]; }
            Buffer.BlockCopy(data, 4, o, 4, 4);
            int nn = (int)UxBin.U32BE(data, 0x58);
            for (int k = 0; k < nn; k++)
            {
                int q = 0x60 + 32 * k + 28;
                UxBin.Require(q + 4 <= data.Length, "kind-2 collis: nodes outside the file");
                o[q] = data[q + 1]; o[q + 1] = data[q]; o[q + 2] = data[q + 3]; o[q + 3] = data[q + 2];
            }
            return o;
        }

        static readonly double[] Eye4 = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        /// collis_to_reflex_primitive(data): kinds 11 and 15 -> little-endian bytes; kind 17 (cylinder) -> kind 15 with one type-8
        /// primitive. Kind-15 tag bytes are dropped; type 9 is refused.
        public static byte[] CollisToReflexPrimitive(byte[] data)
        {
            var d = ReadCollis(data);
            uint k = d.kind;
            if (k == 11)
            {
                var w = new UxWriter(36);
                w.U32(1234).Bytes(new byte[] { (byte)'c', (byte)'o', (byte)'l', 0 }).U32(3).U32(11).U32(0);
                for (int i = 0; i < 3; i++) w.F32((float)d.half[i]);
                return w.U32(1).ToArray();
            }
            if (k == 15 || k == 17)
            {
                var prims = k == 15 ? d.prims : new List<UxCollisPrim> { new UxCollisPrim { type = 8, matrix = Eye4, m1 = 0xFFFFFFFF, @params = new[] { d.radius, d.half_height } } };
                var w = new UxWriter(64);
                w.U32(1234).Bytes(new byte[] { (byte)'c', (byte)'o', (byte)'l', 0 }).U32(3).U32(15).U32(0).U32((uint)prims.Count).U32(0);
                foreach (var pr in prims)
                {
                    if (pr.type == 9) throw new UxException("primitive type 9 has no Reflex PC equivalent");
                    w.U32(pr.type);
                    for (int i = 0; i < 16; i++) w.F32((float)pr.matrix[i]);
                    w.U32(pr.m1);
                    if (pr.type == 5) { for (int i = 0; i < 3; i++) w.F32((float)pr.@params[i]); w.U32(0); }
                    else foreach (double x in pr.@params) w.F32((float)x);
                }
                return w.ToArray();
            }
            throw new UxException("kind " + k + " is not a primitive kind");
        }

        static readonly int[] BoxFaces = { 0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 4, 5, 0, 5, 1, 2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3 };

        /// box_triangles(matrix, half): the (12,3,3) triangles of an oriented box (row-major 4 x 4 matrix, row 3 = position).
        public static double[] BoxTriangles(double[] matrix, double[] half)
        {
            var c = new double[24]; int n = 0;
            for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int sz = -1; sz <= 1; sz += 2)
            { c[n++] = sx * half[0]; c[n++] = sy * half[1]; c[n++] = sz * half[2]; }
            var rot = new double[9];
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) rot[3 * i + j] = matrix[4 * i + j];
            var w = UxNp.MatMul(c, 8, 3, rot, 3);
            for (int i = 0; i < 8; i++) for (int j = 0; j < 3; j++) w[3 * i + j] += matrix[12 + j];
            var t = new double[12 * 9];
            for (int i = 0; i < 36; i++) for (int j = 0; j < 3; j++) t[3 * i + j] = w[3 * BoxFaces[i] + j];
            return t;
        }

        /// collis_triangles(data): object-space collision triangles (n,3,3) of any decodable kind (0: the mesh; 11 / 15 / 17: box
        /// proxies of the primitives); null for the other kinds (kind 2 is a point set).
        public static double[] CollisTriangles(byte[] data)
        {
            var d = ReadCollis(data);
            switch (d.kind)
            {
                case 0: return d.tris;
                case 11: return BoxTriangles(Eye4, d.half);
                case 15:
                {
                    var parts = new List<double[]>(); int total = 0;
                    foreach (var pr in d.prims)
                    {
                        if (pr.type == 5) parts.Add(BoxTriangles(pr.matrix, new[] { pr.@params[0], pr.@params[1], pr.@params[2] }));
                        else if (pr.type == 7 || pr.type == 8)
                        {
                            double r = pr.@params[0], h = pr.type == 8 ? pr.@params[1] : 2 * r;
                            parts.Add(BoxTriangles(pr.matrix, new[] { r, h / 2, r }));
                        }
                        else continue;
                        total += parts[parts.Count - 1].Length;
                    }
                    var o = new double[total]; int at = 0;
                    foreach (var p in parts) { Array.Copy(p, 0, o, at, p.Length); at += p.Length; }
                    return o;
                }
                case 17: return BoxTriangles(Eye4, new[] { d.radius, d.half_height / 2, d.radius });
            }
            return null;
        }

        // ------------------------------------------------------------------------------- conversion to Reflex PC
        public const double UNIT_EPS = 1e-6;

        /// unit_normals(n): (k,3) -> float64 unit vectors; rows shorter than UNIT_EPS or non-finite are returned unchanged.
        public static double[] UnitNormals(float[] n)
        {
            var o = new double[n.Length];
            for (int i = 0; i + 2 < n.Length; i += 3)
            {
                double x = n[i], y = n[i + 1], z = n[i + 2];
                double ln = Math.Sqrt(x * x + y * y + z * z);      // np.sum(n * n, axis=1): left to right
                bool ok = !double.IsNaN(ln) && !double.IsInfinity(ln) && ln >= UNIT_EPS;
                o[i] = ok ? x / ln : x; o[i + 1] = ok ? y / ln : y; o[i + 2] = ok ? z / ln : z;
            }
            return o;
        }
        public static double[] UnitNormals(double[] n)
        {
            var o = new double[n.Length];
            for (int i = 0; i + 2 < n.Length; i += 3)
            {
                double x = n[i], y = n[i + 1], z = n[i + 2];
                double ln = Math.Sqrt(x * x + y * y + z * z);
                bool ok = !double.IsNaN(ln) && !double.IsInfinity(ln) && ln >= UNIT_EPS;
                o[i] = ok ? x / ln : x; o[i + 1] = ok ? y / ln : y; o[i + 2] = ok ? z / ln : z;
            }
            return o;
        }

        /// The 44-byte Reflex PC static vertex (rmesh.VTX_DTYPE): field offsets.
        public const int VTX_STRIDE = 44, VTX_POS = 0, VTX_NRM = 8, VTX_UV0 = 20, VTX_UV1 = 24, VTX_COL = 28, VTX_TAN = 32;
        /// The 28-byte crowd vertex (reflex_crowd_vertices): POSITION @0, NORMAL @8, TEXCOORD0 @20, COLOR @24.
        public const int CROWD_STRIDE = 28, CROWD_POS = 0, CROWD_NRM = 8, CROWD_UV0 = 20, CROWD_COL = 24;

        static void PutUv(byte[] v, int at, UxVal uv, int i, bool clean)
        {
            for (int c = 0; c < 2; c++)
            {
                ushort h = UxNp.FloatToHalf(uv.f[i * uv.comps + c]);
                if (clean) h = UxNp.FloatToHalf(UxNp.NanToZero(UxNp.HalfToFloat(h)));
                UxBin.PutU16(v, at + 2 * c, h);
            }
        }
        static void PutCol(byte[] v, int at, UxVal col, int i)
        {
            if (col == null) { v[at] = v[at + 1] = v[at + 2] = v[at + 3] = 255; return; }
            UxBin.Require(col.u8 != null && col.comps == 4, "vertex colour is not a D3DCOLOR");
            v[at] = col.u8[4 * i + 2]; v[at + 1] = col.u8[4 * i + 1]; v[at + 2] = col.u8[4 * i]; v[at + 3] = col.u8[4 * i + 3];     // R G B A -> B G R A
        }

        /// reflex_vertices(g, origin).tobytes(): the group as Reflex PC 44-byte static vertices: FLOAT16_4 position (minus origin, w = 1),
        /// FLOAT3 unit normal, FLOAT16_2 uv0, FLOAT16_2 uv1 (uv0 when absent), D3DCOLOR (B G R A), FLOAT3 tangent ((1,0,0) when absent);
        /// non-finite normal / uv / tangent values become 0.
        public static byte[] ReflexVertices(UxGroup g, double ox = 0.0, double oy = 0.0, double oz = 0.0)
        {
            int n = g.nverts; var v = new byte[n * VTX_STRIDE];
            UxBin.Require(g.pos != null, "group without positions");
            UxBin.Require((g.uv0 == null || g.uv0.f != null && g.uv0.comps >= 2) && (g.uv1 == null || g.uv1.f != null && g.uv1.comps >= 2), "uv element kind");
            double[] nrm = g.nrm != null ? UnitNormals(g.nrm) : null;
            for (int i = 0; i < n; i++)
            {
                int o = i * VTX_STRIDE;
                UxBin.PutU16(v, o, UxNp.DoubleToHalf(g.pos[3 * i] - ox)); UxBin.PutU16(v, o + 2, UxNp.DoubleToHalf(g.pos[3 * i + 1] - oy));
                UxBin.PutU16(v, o + 4, UxNp.DoubleToHalf(g.pos[3 * i + 2] - oz)); UxBin.PutU16(v, o + 6, 0x3C00);
                for (int c = 0; c < 3; c++) UxBin.PutF32(v, o + VTX_NRM + 4 * c, UxNp.NanToZero(nrm != null ? (float)nrm[3 * i + c] : (c == 1 ? 1f : 0f)));
                if (g.uv0 != null) PutUv(v, o + VTX_UV0, g.uv0, i, true);
                if (g.uv1 != null) PutUv(v, o + VTX_UV1, g.uv1, i, true); else if (g.uv0 != null) PutUv(v, o + VTX_UV1, g.uv0, i, true);
                PutCol(v, o + VTX_COL, g.col, i);
                for (int c = 0; c < 3; c++) UxBin.PutF32(v, o + VTX_TAN + 4 * c, UxNp.NanToZero(g.tan != null ? g.tan[3 * i + c] : (c == 0 ? 1f : 0f)));
            }
            return v;
        }

        /// reflex_crowd_vertices(g).tobytes(): the Reflex PC crowd-section vertices (28 bytes; CROWD_DECL_PC).
        public static byte[] ReflexCrowdVertices(UxGroup g)
        {
            int n = g.nverts; var v = new byte[n * CROWD_STRIDE];
            UxBin.Require(g.pos != null && g.nrm != null && g.uv0 != null && g.uv0.f != null && g.uv0.comps >= 2, "crowd group without position / normal / uv0");
            double[] nrm = UnitNormals(g.nrm);
            for (int i = 0; i < n; i++)
            {
                int o = i * CROWD_STRIDE;
                for (int c = 0; c < 3; c++) UxBin.PutU16(v, o + 2 * c, UxNp.FloatToHalf(g.pos[3 * i + c]));
                UxBin.PutU16(v, o + 6, 0x3C00);
                for (int c = 0; c < 3; c++) UxBin.PutF32(v, o + CROWD_NRM + 4 * c, (float)nrm[3 * i + c]);
                PutUv(v, o + CROWD_UV0, g.uv0, i, false);
                PutCol(v, o + CROWD_COL, g.col, i);
            }
            return v;
        }

        /// CROWD_DECL_PC: the Reflex PC D3DVERTEXELEMENT9 list of the crowd vertex.
        public static readonly byte[] CROWD_DECL_PC = UxBin.FromHex("0000000010000000" + "0000080002000300" + "000014000f000500" + "0000180004000a00" + "ff00000011000000");
    }
}
