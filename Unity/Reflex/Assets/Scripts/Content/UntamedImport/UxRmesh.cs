// MX vs ATV Untamed converter, foundation: the port of the reference's shared Reflex PC mesh codec (mw2-to-reflex scripts/rmesh.py):
// .surface (v4, the 44-byte static vertex), .model (v4 static), .collis kind 22 (triangle clusters), the greedy stripifier and the
// cluster split. Little-endian. numpy arrays are flat row-major arrays (int[] for index arrays, double[] / float[] for coordinates,
// byte[] for the structured vertex array's bytes).
using System;
using System.Collections.Generic;
using System.Text;

namespace Reflex.Port.Content.UntamedImport
{
    /// rmesh.SurfaceGroup: strip (uint16), verts (the VTX_DTYPE array's bytes, 44 per vertex), rec (the raw 0x160 header, or null).
    public sealed class UxSurfaceGroup
    {
        public ushort[] strip; public byte[] verts; public byte[] rec;
        public int VertexCount => verts.Length / UxRmesh.STRIDE;
        /// SurfaceGroup(strip, verts, rec): the strip indices are taken modulo 65536 (np.asarray(strip, np.uint16)).
        public UxSurfaceGroup(int[] strip, byte[] verts, byte[] rec = null)
        {
            this.strip = new ushort[strip.Length]; for (int i = 0; i < strip.Length; i++) this.strip[i] = unchecked((ushort)strip[i]);
            UxBin.Require(verts.Length % UxRmesh.STRIDE == 0, "surface group: vertex bytes are not 44-byte vertices");
            this.verts = verts; this.rec = rec;
        }
        public UxSurfaceGroup(ushort[] strip, byte[] verts, byte[] rec = null)
        {
            UxBin.Require(verts.Length % UxRmesh.STRIDE == 0, "surface group: vertex bytes are not 44-byte vertices");
            this.strip = strip; this.verts = verts; this.rec = rec;
        }
    }

    /// rmesh.Model (and its `raw` dict: the fields kept for byte-exact round trips; null = the key is absent).
    public sealed class UxPcModel
    {
        public string name; public uint draw_distance; public float[] matrix = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        public List<string> materials; public float radius; public float[] center, extents;     // center / extents: 3 values
        public List<uint[]> lods;                                                                 // (ngroups_in_lod, first_group, switch_distance)
        public float[] center4, extents4;                                                         // set by read_model (4 values); null otherwise
        // raw
        public uint[] raw_hdr; public byte[] raw_namebuf, raw_mid, raw_name2, raw_names_blob, raw_flags, raw_tail; public List<uint[]> raw_recs; public uint[] raw_order;

        /// Model(name, materials, radius, center, extents, lods=None, raw=None, draw_distance=0, matrix=identity).
        public UxPcModel(string name, IEnumerable<string> materials, float radius, float[] center, float[] extents, List<uint[]> lods = null, uint draw_distance = 0, float[] matrix = null)
        {
            this.name = name; this.materials = new List<string>(materials); this.radius = radius;
            this.center = new[] { center[0], center[1], center[2] }; this.extents = new[] { extents[0], extents[1], extents[2] };
            this.lods = lods != null && lods.Count > 0 ? lods : new List<uint[]> { new uint[] { (uint)this.materials.Count, 0, 0 } };
            this.draw_distance = draw_distance; if (matrix != null) this.matrix = matrix;
        }
    }

    public static class UxRmesh
    {
        public const uint SURF_MAGIC = 0x99BBCC00, MODEL_MAGIC = 0x88AABB00, COLLIS_MAGIC = 1234;
        public const int STRIDE = 44;
        /// DECL: the D3DVERTEXELEMENT9 list of every static Reflex surface; DECL_BLOCK: zero padded to 160 bytes.
        public static readonly byte[] DECL = UxBin.FromHex("0000000010000000" + "0000080002000300" + "000014000f000500" + "000018000f000501" + "00001c0004000a00" + "0000200002000600" + "ff00000011000000");
        public static readonly byte[] DECL_BLOCK = UxBin.Concat(DECL, new byte[160 - 56]);

        // ----------------------------------------------------------------------------------------- strips
        static long Key(int a, int b) => (long)a << 32 | (uint)b;

        /// stripify(tris): a triangle list (N,3) with Reflex front-face winding -> one triangle strip (degenerate joins).
        public static int[] Stripify(int[] tris)
        {
            int n = tris.Length / 3;
            if (n == 0) return new int[0];
            var edge = new Dictionary<long, List<int>>(n * 3);
            void AddEdge(int a, int b, int t) { long k = Key(a, b); if (!edge.TryGetValue(k, out var l)) edge[k] = l = new List<int>(2); l.Add(t); }
            for (int t = 0; t < n; t++)
            {
                int a = tris[3 * t], b = tris[3 * t + 1], c = tris[3 * t + 2];
                AddEdge(a, b, t); AddEdge(b, c, t); AddEdge(c, a, t);
            }
            var used = new bool[n];
            var nb = new List<int>();
            void Nbrs(int t)       // into nb: triangles across an edge (opposite direction = same winding)
            {
                nb.Clear();
                int a = tris[3 * t], b = tris[3 * t + 1], c = tris[3 * t + 2];
                if (edge.TryGetValue(Key(b, a), out var l)) foreach (int u in l) if (u != t) nb.Add(u);
                if (edge.TryGetValue(Key(c, b), out l)) foreach (int u in l) if (u != t) nb.Add(u);
                if (edge.TryGetValue(Key(a, c), out l)) foreach (int u in l) if (u != t) nb.Add(u);
            }
            int Third(int t, int x, int y)
            {
                int a = tris[3 * t], b = tris[3 * t + 1], c = tris[3 * t + 2];
                if (a == x && b == y) return c;
                if (b == x && c == y) return a;
                return b;
            }
            void Walk(int s0, int s1, int s2, List<int> seq, List<int> got)
            {
                seq.Clear(); got.Clear(); seq.Add(s0); seq.Add(s1); seq.Add(s2);
                var taken = new HashSet<int>();
                while (true)
                {
                    int k = seq.Count - 2, x = seq[seq.Count - 2], y = seq[seq.Count - 1];
                    int kx = k % 2 == 0 ? x : y, ky = k % 2 == 0 ? y : x;
                    int nxt = -1;
                    if (edge.TryGetValue(Key(kx, ky), out var l)) foreach (int t in l) if (!used[t] && !taken.Contains(t)) { nxt = t; break; }
                    if (nxt < 0) return;
                    taken.Add(nxt); got.Add(nxt); seq.Add(Third(nxt, kx, ky));
                }
            }
            var deg = new int[n];
            var heap = new MinHeap(n);
            for (int t = 0; t < n; t++) { Nbrs(t); deg[t] = nb.Count; heap.Push(deg[t], t); }
            var o = new List<int>(n * 3);
            List<int> seq = new List<int>(), got = new List<int>(), bestSeq = new List<int>(), bestGot = new List<int>();
            while (heap.Count > 0)
            {
                heap.Pop(out int d, out int t0);
                if (used[t0] || d != deg[t0]) continue;          // a stale entry (a fresher one was pushed when deg dropped)
                used[t0] = true;
                int a = tris[3 * t0], b = tris[3 * t0 + 1], c = tris[3 * t0 + 2];
                bestSeq.Clear(); bestGot.Clear();
                for (int r = 0; r < 3; r++)
                {
                    if (r == 0) Walk(a, b, c, seq, got); else if (r == 1) Walk(b, c, a, seq, got); else Walk(c, a, b, seq, got);
                    if (seq.Count > bestSeq.Count) { var ts = bestSeq; bestSeq = seq; seq = ts; var tg = bestGot; bestGot = got; got = tg; }   // max(): the first of the longest
                }
                foreach (int t in bestGot) used[t] = true;
                for (int i = -1; i < bestGot.Count; i++)
                {
                    Nbrs(i < 0 ? t0 : bestGot[i]);
                    foreach (int u in nb) if (!used[u]) { deg[u]--; heap.Push(deg[u], u); }
                }
                if (o.Count > 0)
                {
                    if (o.Count % 2 != 0) { o.Add(o[o.Count - 1]); o.Add(bestSeq[0]); o.Add(bestSeq[0]); }     // the next strip must start on an even position
                    else { o.Add(o[o.Count - 1]); o.Add(bestSeq[0]); }
                }
                o.AddRange(bestSeq);
            }
            return o.ToArray();
        }

        /// heapq of (deg, t) tuples (every tuple pushed is distinct, so the pop order does not depend on the heap's layout).
        sealed class MinHeap
        {
            long[] _a; int _n;
            public MinHeap(int capacity) { _a = new long[Math.Max(16, capacity * 2)]; }
            public int Count => _n;
            static long Pack(int d, int t) => (long)d << 32 | (uint)t;        // signed compare of d, then t (t >= 0)
            public void Push(int d, int t)
            {
                if (_n == _a.Length) Array.Resize(ref _a, _n * 2);
                long v = Pack(d, t); int i = _n++;
                while (i > 0) { int p = (i - 1) >> 1; if (_a[p] <= v) break; _a[i] = _a[p]; i = p; }
                _a[i] = v;
            }
            public void Pop(out int d, out int t)
            {
                long top = _a[0]; long v = _a[--_n]; int i = 0;
                while (true)
                {
                    int c = 2 * i + 1; if (c >= _n) break;
                    if (c + 1 < _n && _a[c + 1] < _a[c]) c++;
                    if (_a[c] >= v) break;
                    _a[i] = _a[c]; i = c;
                }
                if (_n > 0) _a[i] = v;
                d = (int)(top >> 32); t = (int)(uint)top;
            }
        }

        /// strip_to_tris(strip): front-facing triangles (degenerates removed).
        public static int[] StripToTris(int[] strip) => UxMesh.StripToTris(strip);
        public static int[] StripToTris(ushort[] strip) { var s = new int[strip.Length]; for (int i = 0; i < s.Length; i++) s[i] = strip[i]; return UxMesh.StripToTris(s); }

        /// canon(tris): each triangle rotated so that its smallest index is first (orientation kept), rows sorted.
        public static int[] Canon(int[] tris)
        {
            int n = tris.Length / 3; var t = new int[n * 3];
            for (int i = 0; i < n; i++)
            {
                int a = tris[3 * i], b = tris[3 * i + 1], c = tris[3 * i + 2];
                int r = a <= b && a <= c ? 0 : (b <= c ? 1 : 2);         // np.argmin: the first minimum
                for (int k = 0; k < 3; k++) t[3 * i + k] = tris[3 * i + (k + r) % 3];
            }
            var idx = UxNp.StableArgsort(new RowList(t, n), (x, y) => { for (int k = 0; k < 3; k++) { int c = t[3 * x + k].CompareTo(t[3 * y + k]); if (c != 0) return c; } return 0; });
            var o = new int[n * 3];
            for (int i = 0; i < n; i++) for (int k = 0; k < 3; k++) o[3 * i + k] = t[3 * idx[i] + k];
            return o;
        }
        sealed class RowList : IList<int>
        {
            readonly int _n; public RowList(int[] t, int n) { _n = n; }
            public int this[int i] { get => i; set => throw new NotSupportedException(); }
            public int Count => _n; public bool IsReadOnly => true;
            public void Add(int item) => throw new NotSupportedException(); public void Clear() => throw new NotSupportedException();
            public bool Contains(int item) => item >= 0 && item < _n; public void CopyTo(int[] array, int at) { for (int i = 0; i < _n; i++) array[at + i] = i; }
            public IEnumerator<int> GetEnumerator() { for (int i = 0; i < _n; i++) yield return i; }
            System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() => GetEnumerator();
            public int IndexOf(int item) => item; public void Insert(int index, int item) => throw new NotSupportedException();
            public bool Remove(int item) => throw new NotSupportedException(); public void RemoveAt(int index) => throw new NotSupportedException();
        }

        // ----------------------------------------------------------------------------------------- surface
        /// read_surface(data): (groups, tail).
        public static List<UxSurfaceGroup> ReadSurface(byte[] data, out byte[] tail)
        {
            UxBin.Require(data.Length >= 16 && UxBin.U32LE(data, 0) == SURF_MAGIC && UxBin.U32LE(data, 4) == 4, "not a surface");
            int ng = (int)UxBin.U32LE(data, 8);
            UxBin.Require(ng >= 0 && 0x10 + (long)ng * 0x160 <= data.Length, "surface: group records outside the file");
            int p = 0x10 + ng * 0x160;
            var groups = new List<UxSurfaceGroup>(ng);
            for (int i = 0; i < ng; i++)
            {
                int r = 0x10 + i * 0x160;
                int nv = (int)UxBin.U32LE(data, r + 16), ib = (int)UxBin.U32LE(data, r + 20), vb = (int)UxBin.U32LE(data, r + 0xBC);
                UxBin.Require(vb == nv * STRIDE && UxBin.Same(data, r + 0x18, DECL_BLOCK, 0, 160), "unsupported vertex format");
                UxBin.Require(ib >= 0 && (long)p + ib + vb <= data.Length, "surface: group data outside the file");
                var strip = new ushort[ib / 2]; for (int k = 0; k < strip.Length; k++) strip[k] = UxBin.U16LE(data, p + 2 * k);
                p += ib;
                groups.Add(new UxSurfaceGroup(strip, UxBin.Slice(data, p, vb), UxBin.Slice(data, r, 0x160)));
                p += vb;
            }
            tail = UxBin.PySlice(data, p, data.Length);
            return groups;
        }
        public static List<UxSurfaceGroup> ReadSurface(byte[] data) => ReadSurface(data, out _);

        /// write_surface(groups, tail).
        public static byte[] WriteSurface(IList<UxSurfaceGroup> groups, byte[] tail = null)
        {
            var head = new UxWriter(16 + groups.Count * 0x160); var body = new UxWriter(1 << 16);
            head.U32(SURF_MAGIC).U32(4).U32((uint)groups.Count).U32(0);
            foreach (var g in groups)
            {
                int nidx = g.strip.Length, nv = g.VertexCount;
                int max = 0; foreach (ushort s in g.strip) if (s > max) max = s;
                UxBin.Require(nv <= 65536 && (nidx == 0 || max < nv), "surface group: strip index outside its " + nv + " vertices");
                if (g.rec != null)
                {
                    UxBin.Require(g.rec.Length == 0x160, "surface group: record size");
                    var rec = (byte[])g.rec.Clone();
                    UxBin.PutU32(rec, 12, (uint)Math.Max(nidx - 2, 0)); UxBin.PutU32(rec, 16, (uint)nv); UxBin.PutU32(rec, 20, (uint)(nidx * 2));
                    UxBin.PutU32(rec, 0xBC, (uint)(nv * STRIDE));
                    head.Bytes(rec);
                }
                else head.U32(0).U32(0xFFFFFFFF).U32(0).U32((uint)Math.Max(nidx - 2, 0)).U32((uint)nv).U32((uint)(nidx * 2)).Bytes(DECL_BLOCK).U32(1).U32((uint)(nv * STRIDE)).Zeros(160);
                foreach (ushort s in g.strip) body.U16(s);
                body.Bytes(g.verts);
            }
            head.Bytes(body.ToArray());
            if (tail != null) head.Bytes(tail);
            return head.ToArray();
        }

        /// make_vertices(pos, nrm, uv, col, tan).tobytes(): pos (n,3), nrm (n,3), uv (n,2) float64; col (n,4 B G R A) or null = 255;
        /// tan (n,3) or null = (1,0,0). uv1 = uv0.
        public static byte[] MakeVertices(double[] pos, double[] nrm, double[] uv, byte[] col = null, double[] tan = null)
        {
            int n = pos.Length / 3; var v = new byte[n * STRIDE];
            for (int i = 0; i < n; i++)
            {
                int o = i * STRIDE;
                for (int c = 0; c < 3; c++) UxBin.PutU16(v, o + 2 * c, UxNp.DoubleToHalf(pos[3 * i + c]));
                UxBin.PutU16(v, o + 6, 0x3C00);
                for (int c = 0; c < 3; c++) UxBin.PutF32(v, o + 8 + 4 * c, (float)nrm[3 * i + c]);
                for (int c = 0; c < 2; c++) { ushort h = UxNp.DoubleToHalf(uv[2 * i + c]); UxBin.PutU16(v, o + 20 + 2 * c, h); UxBin.PutU16(v, o + 24 + 2 * c, h); }
                for (int c = 0; c < 4; c++) v[o + 28 + c] = col == null ? (byte)255 : col[4 * i + c];
                for (int c = 0; c < 3; c++) UxBin.PutF32(v, o + 32 + 4 * c, tan == null ? (c == 0 ? 1f : 0f) : (float)tan[3 * i + c]);
            }
            return v;
        }

        // ----------------------------------------------------------------------------------------- model
        /// read_model(data): a static v4 model (a skinned / boned layout is refused).
        public static UxPcModel ReadModel(byte[] data)
        {
            UxBin.Require(data.Length >= 0x118, "not a model");
            var h = UxBin.U32s(data, 0, 10, false);
            UxBin.Require(h[0] == MODEL_MAGIC && h[1] == 4, "not a model");
            int namelen = (int)h[4], nlods = (int)h[5], nbytes = (int)h[6], ng = (int)h[7];
            if (h[2] != 0 || h[3] != 1) throw new UxException("skinned/boned model layout not supported");
            float[] c4 = UxBin.F32s(data, 0xF8, 4, false), e4 = UxBin.F32s(data, 0x108, 4, false);
            int p = 0x118;
            UxBin.Require((long)p + namelen + 12L * nlods + 20L * ng + nbytes + 5L * ng <= data.Length, "model: tables outside the file");
            byte[] name2 = UxBin.Slice(data, p, namelen); p += namelen;
            var lods = new List<uint[]>(); for (int i = 0; i < nlods; i++) lods.Add(UxBin.U32s(data, p + 12 * i, 3, false)); p += 12 * nlods;
            var recs = new List<uint[]>(); for (int i = 0; i < ng; i++) recs.Add(UxBin.U32s(data, p + 20 * i, 5, false)); p += 20 * ng;
            byte[] blob = UxBin.Slice(data, p, nbytes); p += nbytes;
            byte[] flags = UxBin.Slice(data, p, ng); p += ng;
            uint[] order = UxBin.U32s(data, p, ng, false); p += 4 * ng;
            var mats = new List<string>();
            foreach (var r in recs) mats.Add(r[0] >= blob.Length ? "" : Encoding.UTF8.GetString(blob, (int)r[0], UxPcScene.NulAt(blob, (int)r[0], blob.Length - (int)r[0]) - (int)r[0]));
            var m = new UxPcModel(Encoding.UTF8.GetString(name2, 0, UxPcScene.NulAt(name2, 0, name2.Length)), mats, UxBin.F32LE(data, 0xF4), c4, e4, lods, UxBin.U32LE(data, 0xA8), UxBin.F32s(data, 0xAC, 16, false));
            m.raw_hdr = h; m.raw_namebuf = UxBin.Slice(data, 0x28, 0x80); m.raw_mid = UxBin.Slice(data, 0xA8, 0xF4 - 0xA8); m.raw_name2 = name2;
            m.raw_recs = recs; m.raw_names_blob = blob; m.raw_flags = flags; m.raw_order = order; m.raw_tail = UxBin.PySlice(data, p, data.Length);
            m.center4 = c4; m.extents4 = e4;
            return m;
        }

        /// write_model(m).
        public static byte[] WriteModel(UxPcModel m)
        {
            var enc = Encoding.UTF8;
            byte[] names = m.raw_names_blob;
            if (names == null) { var w = new UxWriter(); foreach (string s in m.materials) { w.Bytes(enc.GetBytes(s)); w.U8(0); } names = w.ToArray(); }
            var offs = new List<uint>(); uint at = 0;
            foreach (string s in m.materials) { offs.Add(at); at += (uint)enc.GetByteCount(s) + 1; }
            int ng = m.materials.Count;
            byte[] nm = m.raw_name2 != null && m.raw_name2.Length > 0 ? m.raw_name2 : UxBin.Concat(enc.GetBytes(m.name), new byte[1]);
            uint[] hdr = m.raw_hdr ?? new uint[] { MODEL_MAGIC, 4, 0, 1, (uint)nm.Length, (uint)m.lods.Count, (uint)names.Length, (uint)ng, (uint)ng, 0 };
            var o = new UxWriter(0x200 + names.Length);
            foreach (uint x in hdr) o.U32(x);
            byte[] nb = m.raw_namebuf != null && m.raw_namebuf.Length > 0 ? m.raw_namebuf : UxBin.PySlice(UxBin.Concat(enc.GetBytes(m.name), new byte[128]), 0, 128);
            o.Bytes(nb);
            if (m.raw_mid != null) o.Bytes(m.raw_mid);
            else { o.U32(m.draw_distance); foreach (float f in m.matrix) o.F32(f); o.U32(0).U32(0xFFFFFFFF); }
            float[] c4 = m.center4 ?? new[] { m.center[0], m.center[1], m.center[2], 1.0f };
            float[] e4 = m.extents4 ?? new[] { m.extents[0], m.extents[1], m.extents[2], 1.0f };
            o.F32(m.radius); foreach (float f in c4) o.F32(f); foreach (float f in e4) o.F32(f);
            o.Bytes(nm);
            foreach (var l in m.lods) { o.U32(l[0]); o.U32(l[1]); o.U32(l[2]); }
            if (m.raw_recs != null && m.raw_recs.Count > 0) foreach (var rec in m.raw_recs) { for (int k = 0; k < 5; k++) o.U32(rec[k]); }
            else for (int i = 0; i < ng; i++) o.U32(offs[i]).U32((uint)i).U32(0).U32(0).U32(0);
            o.Bytes(names);
            o.Bytes(m.raw_flags != null && m.raw_flags.Length > 0 ? m.raw_flags : new byte[ng]);
            if (m.raw_order != null && m.raw_order.Length > 0) { UxBin.Require(m.raw_order.Length == ng, "model: order length"); foreach (uint x in m.raw_order) o.U32(x); }
            else for (int i = 0; i < ng; i++) o.U32((uint)i);
            if (m.raw_tail != null) o.Bytes(m.raw_tail);
            return o.ToArray();
        }

        /// bounds(pos) for a float64 (n,3) array: (radius, centre, half extents) = (|e|, (lo + hi) / 2, (hi - lo) / 2); the radius is
        /// np.linalg.norm(e) (a BLAS dot: UxNp.Norm).
        public static (double radius, double[] c, double[] e) Bounds(double[] pos)
        {
            UxBin.Require(pos.Length >= 3, "bounds of no points");
            var lo = new[] { pos[0], pos[1], pos[2] }; var hi = new[] { pos[0], pos[1], pos[2] };
            for (int i = 3; i < pos.Length; i++) { int c = i % 3; if (pos[i] < lo[c]) lo[c] = pos[i]; if (pos[i] > hi[c]) hi[c] = pos[i]; }
            var cc = new double[3]; var e = new double[3];
            for (int k = 0; k < 3; k++) { cc[k] = (lo[k] + hi[k]) / 2; e[k] = (hi[k] - lo[k]) / 2; }
            return (UxNp.Norm(e), cc, e);
        }

        // ----------------------------------------------------------------------------------------- collis
        /// A kind-22 cluster: centre (3 float32) and its triangles relative to the centre ((ntris,3,3) float32).
        public sealed class Cluster { public float[] c; public float[] t; public int TriCount => t.Length / 9; }

        /// read_collis(data): (kind, clusters); the clusters are null for a kind other than 22 (Python returns the data then).
        public static List<Cluster> ReadCollis(byte[] data, out uint kind)
        {
            UxBin.Require(data.Length >= 16 && UxBin.U32LE(data, 0) == COLLIS_MAGIC && data[4] == 'c' && data[5] == 'o' && data[6] == 'l' && data[7] == 0, "not a collis");
            kind = UxBin.U32LE(data, 12);
            if (kind != 22) return null;
            UxBin.Require(data.Length >= 24, "collis kind 22: truncated");
            int n = (int)UxBin.U32LE(data, 20), p = 24;
            var clusters = new List<Cluster>(n);
            for (int i = 0; i < n; i++)
            {
                UxBin.Require(p + 16 <= data.Length, "collis kind 22: truncated");
                var c = UxBin.F32s(data, p, 3, false); p += 12;
                int nt = (int)UxBin.U32LE(data, p); p += 4;
                UxBin.Require(nt >= 0 && (long)p + 36L * nt <= data.Length, "collis kind 22: triangles outside the file");
                clusters.Add(new Cluster { c = c, t = UxBin.F32s(data, p, nt * 9, false) }); p += nt * 36;
            }
            UxBin.Require(p == data.Length, "collis kind 22: trailing bytes");
            return clusters;
        }

        /// write_collis(clusters).
        public static byte[] WriteCollis(IList<Cluster> clusters)
        {
            var o = new UxWriter(24 + clusters.Count * 64);
            o.U32(COLLIS_MAGIC).Bytes(new byte[] { (byte)'c', (byte)'o', (byte)'l', 0 }).U32(3).U32(22).U32(0).U32((uint)clusters.Count);
            foreach (var cl in clusters)
            {
                o.F32(cl.c[0]).F32(cl.c[1]).F32(cl.c[2]).U32((uint)cl.TriCount);
                foreach (float f in cl.t) o.F32(f);
            }
            return o.ToArray();
        }

        /// cluster_triangles(tris, max_tris, max_half): world-space triangles (N,3,3) float64 split into small spatial clusters
        /// (recursive median split on the longest axis of the centroid box). Returns [(centre, relative triangles)].
        public static List<Cluster> ClusterTriangles(double[] tris, int max_tris = 8, double max_half = 12.0)
        {
            int n = tris.Length / 9;
            var cen = new double[n * 3];
            for (int i = 0; i < n; i++) for (int c = 0; c < 3; c++) cen[3 * i + c] = ((tris[9 * i + c] + tris[9 * i + 3 + c]) + tris[9 * i + 6 + c]) / 3;      // tris.mean(1)
            var o = new List<Cluster>();
            var stack = new List<int[]>(); var all = new int[n]; for (int i = 0; i < n; i++) all[i] = i; stack.Add(all);
            while (stack.Count > 0)
            {
                int[] idx = stack[stack.Count - 1]; stack.RemoveAt(stack.Count - 1);
                if (idx.Length == 0) continue;
                double[] lo = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, hi = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                foreach (int t in idx) for (int k = 0; k < 9; k++) { double v = tris[9 * t + k]; int c = k % 3; if (v < lo[c]) lo[c] = v; if (v > hi[c]) hi[c] = v; }
                double hmax = Math.Max(Math.Max((hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2), (hi[2] - lo[2]) / 2);
                if (idx.Length <= max_tris && (hmax <= max_half || idx.Length == 1))
                {
                    double[] c = { (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2 };
                    var cl = new Cluster { c = new[] { (float)c[0], (float)c[1], (float)c[2] }, t = new float[idx.Length * 9] };
                    for (int i = 0; i < idx.Length; i++) for (int k = 0; k < 9; k++) cl.t[9 * i + k] = (float)(tris[9 * idx[i] + k] - c[k % 3]);
                    o.Add(cl);
                    continue;
                }
                double[] cl2 = { double.PositiveInfinity, double.PositiveInfinity, double.PositiveInfinity }, ch = { double.NegativeInfinity, double.NegativeInfinity, double.NegativeInfinity };
                foreach (int t in idx) for (int c = 0; c < 3; c++) { double v = cen[3 * t + c]; if (v < cl2[c]) cl2[c] = v; if (v > ch[c]) ch[c] = v; }
                int ax = 0; for (int c = 1; c < 3; c++) if (ch[c] - cl2[c] > ch[ax] - cl2[ax]) ax = c;              // np.argmax: the first maximum
                if (ch[ax] - cl2[ax] < 1e-6)
                {   // coincident centroids: split by count
                    int h2 = idx.Length / 2; var a2 = new int[h2]; var b2 = new int[idx.Length - h2];
                    Array.Copy(idx, a2, h2); Array.Copy(idx, h2, b2, 0, b2.Length);
                    stack.Add(a2); stack.Add(b2);
                    continue;
                }
                int axis = ax;
                var order = UxNp.StableArgsort(idx, (x, y) => cen[3 * x + axis].CompareTo(cen[3 * y + axis]));
                int m = idx.Length / 2; var first = new int[m]; var second = new int[idx.Length - m];
                for (int i = 0; i < m; i++) first[i] = idx[order[i]];
                for (int i = m; i < idx.Length; i++) second[i - m] = idx[order[i]];
                stack.Add(first); stack.Add(second);
            }
            return o;
        }
    }
}
