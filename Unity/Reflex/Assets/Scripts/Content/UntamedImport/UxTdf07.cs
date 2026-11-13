// MX vs ATV Untamed converter, stage "race" (S8): the port of the Python reference converter's tdf07_to_reflex.py. An Untamed race line
// (AI/<Level>-<mode>.tdf07) becomes a Reflex PC `tdf` asset: the Reflex body without its size word, so the conversion is the little-endian
// size word + (by default) the node outer edges 44/45 (copies of 22/23), zero 8 x 4 grids 46-49 and a start gate line G8 synthesised
// behind the Untamed DropGate. Also the readers the race kit works with (sections, nodes, lines, the lap walk) and the line padding hygiene.
// Pure C# (System only). Function names follow the Python module's.
using System;
using System.Collections.Generic;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// One tagged stream item {u32 id, u32 size, payload}: Python's (i, s, raw) tuple (immutable; a changed item is a new one).
    public sealed class UxTdfItem
    {
        public readonly uint id, size; public readonly byte[] raw;
        public UxTdfItem(uint id, uint size, byte[] raw) { this.id = id; this.size = size; this.raw = raw; }
    }

    /// structure()'s section: {'head', 'fields', 'nodes'} (a node is the list of its items, the id-20 marker first).
    public sealed class UxTdfSection
    {
        public UxTdfItem head;
        public readonly List<UxTdfItem> fields = new List<UxTdfItem>();
        public readonly List<List<UxTdfItem>> nodes = new List<List<UxTdfItem>>();
    }

    /// split()'s result: (kind, hdr, path, items).
    public sealed class UxTdfSplit
    {
        public string kind; public uint[] hdr; public byte[] path; public List<UxTdfItem> items;
        public bool Reflex => kind == "reflex";
    }

    /// line_rec(raw): a 48-byte global line record.
    public sealed class UxTdfLine
    {
        public double[] pos, dir; public double width; public byte present; public uint a, b;
    }

    /// tdf07_to_reflex.py.
    public static class UxTdf07
    {
        public const uint VEC3 = 0x10;
        static readonly HashSet<uint> GRID_IDS = new HashSet<uint> { 6, 18, 19, 28, 29, 33, 34, 40, 46, 47, 48, 49 };
        static readonly HashSet<uint> VEC_IDS = new HashSet<uint> { 1, 2, 3, 21, 22, 23, 26, 44, 45 };
        static readonly HashSet<uint> WORD_IDS = new HashSet<uint> { 4, 5, 7, 13, 17, 24, 25, 27, 31, 38, 39, 41, 42 };
        /// sorted(LINE_IDS).
        public static readonly uint[] LINE_IDS = { 8, 9, 10, 35, 43 };
        static bool IsLine(uint i) => i == 8 || i == 9 || i == 10 || i == 35 || i == 43;

        // ---------------------------------------------------------------------------------------------- raw stream
        public static int Plen(uint size) => size == VEC3 ? 12 : checked((int)size);

        /// detect(data): 'reflex' (a size word first) or 'tdf07'.
        public static string Detect(byte[] data)
        {
            if (data.Length >= 0x20 && UxBin.U32LE(data, 0) == (uint)(data.Length - 4) && UxBin.U32BE(data, 0x18) == 260) return "reflex";
            if (data.Length >= 0x1c && UxBin.U32BE(data, 0x14) == 260) return "tdf07";
            throw new UxException("not a TDF: no 260-byte path field at 0x14 (tdf07) or 0x18 (Reflex)");
        }

        public static UxTdfSplit Split(byte[] data)
        {
            string kind = Detect(data);
            int b = kind == "reflex" ? 4 : 0;
            var hdr = UxBin.U32s(data, b, 6, true);
            UxBin.Require(hdr[5] <= int.MaxValue, "TDF path length");
            var path = UxBin.PySlice(data, b + 24, b + 24 + (int)hdr[5]);
            long p = b + 24 + (long)hdr[5];
            var items = new List<UxTdfItem>();
            while (p + 8 <= data.Length)
            {
                uint i = UxBin.U32BE(data, (int)p), s = UxBin.U32BE(data, (int)p + 4);
                long n = s == VEC3 ? 12 : s;
                if (p + 8 + n > data.Length) throw new UxException("truncated item " + i + " at 0x" + p.ToString("x"));
                items.Add(new UxTdfItem(i, s, UxBin.Slice(data, (int)p + 8, (int)n)));
                p += 8 + n;
            }
            if (p != data.Length) throw new UxException("stream does not end on an item (" + (data.Length - p) + " trailing bytes)");
            return new UxTdfSplit { kind = kind, hdr = hdr, path = path, items = items };
        }

        public static byte[] Join(uint[] hdr, byte[] path, IList<UxTdfItem> items, bool size_word = true)
        {
            int n = 24 + path.Length;
            foreach (var it in items) { UxBin.Require(it.raw.Length == Plen(it.size), "tdf item " + it.id + ": size " + it.size + " with " + it.raw.Length + " bytes"); n += 8 + it.raw.Length; }
            var w = new UxWriter(n + 4);
            if (size_word) w.U32((uint)n);
            for (int k = 0; k < 6; k++) w.U32(hdr[k], true);
            w.Bytes(path);
            foreach (var it in items) { w.U32(it.id, true); w.U32(it.size, true); w.Bytes(it.raw); }
            return w.ToArray();
        }

        public static void Structure(IList<UxTdfItem> items, out List<UxTdfItem> glob, out List<UxTdfSection> secs)
        {
            glob = new List<UxTdfItem>(); secs = new List<UxTdfSection>();
            UxTdfSection cur_s = null; List<UxTdfItem> cur_n = null;
            foreach (var it in items)
            {
                if (it.id == 11) { cur_s = new UxTdfSection { head = it }; secs.Add(cur_s); cur_n = null; }
                else if (it.id == 20)
                {
                    if (cur_s == null) throw new UxException("tdf: a node before the first section");
                    cur_n = new List<UxTdfItem> { it }; cur_s.nodes.Add(cur_n);
                }
                else if (cur_n != null) cur_n.Add(it);
                else if (cur_s != null) cur_s.fields.Add(it);
                else glob.Add(it);
            }
        }

        public static List<UxTdfItem> Flatten(IList<UxTdfItem> glob, IList<UxTdfSection> secs)
        {
            var o = new List<UxTdfItem>(glob);
            foreach (var s in secs)
            {
                o.Add(s.head); o.AddRange(s.fields);
                foreach (var n in s.nodes) o.AddRange(n);
            }
            return o;
        }

        /// first(items, i): the payload of the first item with that id, or null.
        public static byte[] First(IList<UxTdfItem> items, uint i)
        {
            foreach (var it in items) if (it.id == i) return it.raw;
            return null;
        }
        static byte[] Need(byte[] raw, string what) => raw ?? throw new UxException("tdf: no " + what);

        public static uint U32(byte[] raw) => UxBin.U32BE(raw, 0);
        public static double F32(byte[] raw) => UxBin.F32BE(raw, 0);
        /// vec3(raw): struct.unpack('>3f', raw) (the payload must be 12 bytes).
        public static double[] Vec3(byte[] raw)
        {
            UxBin.Require(raw != null && raw.Length == 12, "tdf: a vec3 needs 12 bytes");
            return new double[] { UxBin.F32BE(raw, 0), UxBin.F32BE(raw, 4), UxBin.F32BE(raw, 8) };
        }

        public static UxTdfLine LineRec(byte[] raw)
        {
            UxBin.Require(raw != null && raw.Length == 48, "tdf: a line record needs 48 bytes");
            return new UxTdfLine
            {
                pos = new double[] { UxBin.F32BE(raw, 0), UxBin.F32BE(raw, 4), UxBin.F32BE(raw, 8) },
                dir = new double[] { UxBin.F32BE(raw, 16), UxBin.F32BE(raw, 20), UxBin.F32BE(raw, 24) },
                width = UxBin.F32BE(raw, 32), present = raw[36], a = UxBin.U32BE(raw, 40), b = UxBin.U32BE(raw, 44),
            };
        }

        // ---------------------------------------------------------------------------------------------- validation
        /// walk_check(hdr, items): the port of Open Reflex AliveRace.WalkTdf plus the Reflex loader's needs. An empty list = OK.
        public static List<string> WalkCheck(uint[] hdr, IList<UxTdfItem> items)
        {
            var bad = new List<string>();
            uint nn = hdr[0], ns = hdr[1];
            if (!(0 < nn && nn <= 65536 && 0 < ns && ns <= 4096 && hdr[2] <= 4096 && hdr[3] <= 4096))
                bad.Add("header counts out of range [" + hdr[0] + ", " + hdr[1] + ", " + hdr[2] + ", " + hdr[3] + "]");
            long cn = 0, cs = 0;
            foreach (var it in items)
            {
                uint i = it.id, s = it.size; var raw = it.raw;
                if (GRID_IDS.Contains(i))
                {
                    UxBin.Require(raw.Length >= 8, "tdf: grid " + i + " shorter than its counts");
                    uint n = UxBin.U32BE(raw, 0), el = UxBin.U32BE(raw, 4);
                    if (n > 8 || el > 4) bad.Add("grid " + i + " larger than 8x4");
                    if (s != 8 + 4L * n * el) bad.Add("grid " + i + " size " + s + " != " + (8 + 4L * n * el));
                }
                else if (VEC_IDS.Contains(i)) { if (s != VEC3) bad.Add("vec3 id " + i + " with size " + s); }
                else if (WORD_IDS.Contains(i)) { if (s != 4) bad.Add("word id " + i + " with size " + s); }
                else if (IsLine(i)) { if (s != 48) bad.Add("line id " + i + " with size " + s); }
                else if (i == 11 || i == 20)
                {
                    if (s != 0) bad.Add("marker " + i + " with size " + s);
                    if (i == 20) cn++; else cs++;
                }
                else if (i == 12 || i == 15) { if (s != 4) bad.Add("count id " + i + " with size " + s); }
                else if (i == 14 || i == 16)
                {
                    UxBin.Require(raw.Length == 8, "tdf: section link " + i + " needs 8 bytes");
                    uint a = UxBin.U32BE(raw, 0), b = UxBin.U32BE(raw, 4);
                    if (s != 8 || a >= ns || b >= ns) bad.Add("section link " + i + " out of range (" + a + ", " + b + ")");
                }
                else if (i == 36)
                {
                    UxBin.Require(raw.Length >= 4, "tdf: class limits id 36 shorter than its count");
                    uint c = UxBin.U32BE(raw, 0);
                    if (c > 8 || s != 4 + 4L * c) bad.Add("class limits id 36 malformed");
                }
                // any other id is skipped by its size field, as WalkTdf's default branch does
            }
            if (cn != nn || cs != ns) bad.Add("walk counted " + cn + " nodes / " + cs + " sections, header says " + nn + " / " + ns);
            Structure(items, out var glob, out var secs);
            foreach (uint i in LINE_IDS)
            {
                var r = First(glob, i);
                if (r == null) continue;
                var L = LineRec(r);
                if (L.a >= secs.Count || L.b >= secs[(int)L.a].nodes.Count) bad.Add("line " + i + " points at missing section/node (" + L.a + ", " + L.b + ")");
            }
            var uniq = new List<string>();                           // list(dict.fromkeys(bad))
            foreach (string b in bad) if (!uniq.Contains(b)) uniq.Add(b);
            return uniq;
        }

        // ---------------------------------------------------------------------------------------------- geometry
        /// node_dict(n): {id: payload} (the last item of an id wins).
        public static Dictionary<uint, byte[]> NodeDict(IList<UxTdfItem> n)
        {
            var d = new Dictionary<uint, byte[]>();
            foreach (var it in n) d[it.id] = it.raw;
            return d;
        }
        public static byte[] Field(Dictionary<uint, byte[]> nd, uint id) => nd.TryGetValue(id, out var r) ? r : throw new UxException("tdf: a node has no field " + id);

        public static uint SectionKind(UxTdfSection s) => U32(Need(First(s.fields, 17), "section field 17 (kind)"));

        /// links(s, i): the second word of every section field i (14 = next sections).
        public static List<uint> Links(UxTdfSection s, uint i)
        {
            var o = new List<uint>();
            foreach (var it in s.fields)
                if (it.id == i) { UxBin.Require(it.raw.Length == 8, "tdf: section link " + i + " needs 8 bytes"); o.Add(UxBin.U32BE(it.raw, 4)); }
            return o;
        }

        /// lap_walk(glob, secs): from the finish node, the nodes in order and at a section end the FIRST 'next' link, until back at the
        /// finish node. Returns the (section, node) list and whether it closed.
        public static List<(int s, int n)> LapWalk(IList<UxTdfItem> glob, IList<UxTdfSection> secs, out bool closed)
        {
            var L = LineRec(Need(First(glob, 9), "finish line (global 9)"));
            int a = checked((int)L.a), b = checked((int)L.b);
            var o = new List<(int, int)>(); int si = a, ni = b;
            long total = 0; foreach (var s in secs) total += s.nodes.Count;
            closed = false;
            for (long k = 0; k < total + 2; k++)
            {
                o.Add((si, ni));
                ni++;
                if (ni >= secs[si].nodes.Count)
                {
                    var nx = Links(secs[si], 14);
                    if (nx.Count == 0) return o;
                    si = checked((int)nx[0]); ni = 0;
                }
                if (si == a && ni == b) { closed = true; return o; }
            }
            return o;
        }

        /// polyline_len(pts, closed) over x / z.
        public static double PolylineLen(IList<double[]> pts, bool closed)
        {
            double s = 0.0;
            for (int k = 0; k < pts.Count - (closed ? 0 : 1); k++)
            {
                var p = pts[k]; var q = pts[(k + 1) % pts.Count];
                s += UxRaceNum.PyHypot(q[0] - p[0], q[2] - p[2]);
            }
            return s;
        }

        // ---------------------------------------------------------------------------------------------- gate synthesis
        struct GateNode { public int ci, k; public Dictionary<uint, byte[]> nd; }

        /// make_gate(glob, secs, gate_xz, gate_fwd, behind): the 48-byte G8 record and its facts, or (null, the reason as info["reason"]).
        /// gate_xz / gate_fwd: null = not given.
        public static byte[] MakeGate(IList<UxTdfItem> glob, IList<UxTdfSection> secs, double[] gate_xz, double[] gate_fwd, out object info, double behind = 5.0)
        {
            var chutes = new List<int>();
            for (int k = 0; k < secs.Count; k++) if (SectionKind(secs[k]) == 4) chutes.Add(k);
            if (chutes.Count == 0) { info = "no start chute (kind-4 section)"; return null; }
            var nodes = new List<GateNode>();
            foreach (int ci in chutes) for (int k = 0; k < secs[ci].nodes.Count; k++) nodes.Add(new GateNode { ci = ci, k = k, nd = NodeDict(secs[ci].nodes[k]) });
            UxBin.Require(nodes.Count > 0, "tdf: the start chutes have no node");
            double[] pos, dv; double hw; string how; GateNode pick;
            if (gate_xz == null)
            {
                pick = nodes[0];                                       // min(nodes, key=(abs(k - 1), ci)): the first smallest
                foreach (var t in nodes)
                {
                    int a = Math.Abs(t.k - 1), b = Math.Abs(pick.k - 1);
                    if (a < b || a == b && t.ci < pick.ci) pick = t;
                }
                var c = Vec3(Field(pick.nd, 21)); var l = Vec3(Field(pick.nd, 22)); var r = Vec3(Field(pick.nd, 23)); dv = Vec3(Field(pick.nd, 26));
                pos = c; hw = 0.5 * UxRaceNum.PyHypot(l[0] - r[0], l[2] - r[2]); how = "chute node 1 (no DropGate given)";
            }
            else
            {
                double gx = gate_xz[0], gz = gate_xz[1];
                var near = MinBy(nodes, gx, gz);
                if (gate_fwd == null) dv = Vec3(Field(near.nd, 26));
                else
                {
                    dv = new[] { gate_fwd[0], 0.0, gate_fwd[1] };
                    var nd0 = Vec3(Field(near.nd, 26));
                    if (dv[0] * nd0[0] + dv[2] * nd0[2] < 0) dv = new[] { -dv[0], 0.0, -dv[2] };      // make it point down the chute
                }
                double y = Vec3(Field(near.nd, 21))[1];
                pos = new[] { gx - behind * dv[0], y, gz - behind * dv[2] };
                double lat0 = dv[2], lat1 = -dv[0];
                hw = 0.0;
                foreach (int ci in chutes)
                {
                    var ci_nodes = new List<GateNode>(); foreach (var t in nodes) if (t.ci == ci) ci_nodes.Add(t);
                    if (ci_nodes.Count == 0) throw new UxException("tdf: start chute " + ci + " has no node");
                    var t2 = MinBy(ci_nodes, pos[0], pos[2]);
                    var c2 = Vec3(Field(t2.nd, 21));
                    if (UxRaceNum.PyHypot(c2[0] - pos[0], c2[2] - pos[2]) > 150) continue;
                    foreach (uint e in new uint[] { 22, 23 })
                    {
                        var p = Vec3(Field(t2.nd, e));
                        double v = Math.Abs((p[0] - pos[0]) * lat0 + (p[2] - pos[2]) * lat1);
                        if (v > hw) hw = v;                            // max(hw, v)
                    }
                }
                pick = MinBy(nodes, pos[0], pos[2]);
                how = "DropGate entity, " + UxRaceNum.PyFixed(behind, 0) + " ft behind, spans " + chutes.Count + " chute(s)";
            }
            byte[] tmpl = null;
            foreach (uint i in new uint[] { 9, 10, 35, 43 }) { tmpl = First(glob, i); if (tmpl != null) break; }
            var raw = new byte[48];
            for (int k = 0; k < 3; k++) UxBin.PutF32(raw, 4 * k, (float)pos[k], true);
            if (tmpl != null) Buffer.BlockCopy(tmpl, 12, raw, 12, 4);          // padding words, copied from a line
            for (int k = 0; k < 3; k++) UxBin.PutF32(raw, 16 + 4 * k, (float)dv[k], true);
            if (tmpl != null) Buffer.BlockCopy(tmpl, 28, raw, 28, 4);
            UxBin.PutF32(raw, 32, (float)hw, true);
            raw[36] = 1;                                                        // the present flag (the loader does not byte-swap it)
            if (tmpl != null) Buffer.BlockCopy(tmpl, 37, raw, 37, 3);
            UxBin.PutU32(raw, 40, (uint)pick.ci, true); UxBin.PutU32(raw, 44, (uint)pick.k, true);
            var cc = Vec3(Field(pick.nd, 21));
            info = new JObj().Set("section", (long)pick.ci).Set("node", (long)pick.k)
                .Set("pos", new List<object> { UxNp.PyRound(pos[0], 2), UxNp.PyRound(pos[2], 2) })
                .Set("dir", new List<object> { UxNp.PyRound(dv[0], 3), UxNp.PyRound(dv[2], 3) })
                .Set("half_width", UxNp.PyRound(hw, 2))
                .Set("node_offset_ft", UxNp.PyRound(UxRaceNum.PyHypot(cc[0] - pos[0], cc[2] - pos[2]), 1)).Set("method", how);
            return raw;
        }
        /// min(nodes, key=hypot(centre - (x, z))): the first of the nearest.
        static GateNode MinBy(List<GateNode> nodes, double x, double z)
        {
            GateNode best = nodes[0]; double bd = 0; bool have = false;
            foreach (var t in nodes)
            {
                var c = Vec3(Field(t.nd, 21));
                double d = UxRaceNum.PyHypot(c[0] - x, c[2] - z);
                if (!have || d < bd) { best = t; bd = d; have = true; }
            }
            return best;
        }

        public static byte[] ZeroGrid()
        {
            var g = new byte[8 + 128];
            UxBin.PutU32(g, 0, 8, true); UxBin.PutU32(g, 4, 4, true);
            return g;
        }

        // ---------------------------------------------------------------------------------------------- conversion
        /// convert(data, outer, node_grids, gate, gate_xz, gate_fwd): the Reflex tdf and the info dict ('input_kind', 'added', 'gate',
        /// 'roundtrip'). gate: "auto" or "off".
        public static byte[] Convert(byte[] data, out JObj info, bool outer = true, bool node_grids = true, string gate = "auto", double[] gate_xz = null, double[] gate_fwd = null)
        {
            var sp = Split(data);
            var probs = WalkCheck(sp.hdr, sp.items);
            if (probs.Count > 0) throw new UxException("input fails the walk check: " + string.Join("; ", probs));
            Structure(sp.items, out var glob, out var secs);
            var added = new List<object>();
            info = new JObj().Set("input_kind", sp.kind).Set("added", added);
            // nodes: stock Reflex order 20,21,22,23,44,45,26,24,25,27,37,28,29,33,34,40,47,48,49,46
            foreach (var s in secs)
                foreach (var n in s.nodes)
                {
                    if (outer && IndexOf(n, 44) < 0 && IndexOf(n, 45) < 0)
                    {
                        int k = IndexOf(n, 23);
                        if (k < 0) throw new UxException("tdf: a node has no edge 23");
                        var e22 = First(n, 22); var e23 = First(n, 23);
                        UxBin.Require(e22 != null, "tdf: a node has no edge 22");
                        n.Insert(k + 1, new UxTdfItem(44, VEC3, e22)); n.Insert(k + 2, new UxTdfItem(45, VEC3, e23));
                    }
                    if (node_grids && IndexOf(n, 46) < 0 && IndexOf(n, 47) < 0 && IndexOf(n, 48) < 0 && IndexOf(n, 49) < 0)
                    {
                        int k = IndexOf(n, 40); k = k >= 0 ? k + 1 : n.Count;
                        int at = k;
                        foreach (uint g in new uint[] { 47, 48, 49, 46 }) n.Insert(at++, new UxTdfItem(g, 0x88, ZeroGrid()));
                    }
                }
            if (outer) added.Add("node 44/45 = copies of 22/23");
            if (node_grids) added.Add("node 46-49 = zero 8x4 grids");
            if (gate != "off" && First(glob, 8) == null)
            {
                var raw = MakeGate(glob, secs, gate_xz, gate_fwd, out object ginfo);
                if (raw != null)
                {
                    int k = IndexOf(glob, 9);                          // stock order ... 9, 8, 10, 35, 43
                    if (k < 0) throw new UxException("tdf: no finish line (global 9) to place the start gate line after");
                    glob.Insert(k + 1, new UxTdfItem(8, 0x30, raw));
                    added.Add("G8 start gate line synthesized");
                }
                info.Set("gate", ginfo);
            }
            var o = Join(sp.hdr, sp.path, Flatten(glob, secs), true);
            // self checks: walk, size word, and stripping the additions gives the input body back byte for byte
            var s2 = Split(o);
            UxBin.Require(s2.kind == "reflex" && UxBin.U32LE(o, 0) == (uint)(o.Length - 4), "tdf: the output is not a Reflex tdf");
            probs = WalkCheck(s2.hdr, s2.items);
            if (probs.Count > 0) throw new UxException("output fails the walk check: " + string.Join("; ", probs));
            var orig = new HashSet<uint>(); foreach (var it in sp.items) orig.Add(it.id);
            bool anyAdded = outer || node_grids;
            var stripped = new List<UxTdfItem>();
            foreach (var it in s2.items)
            {
                if (anyAdded && it.id >= 44 && it.id <= 49 && !orig.Contains(it.id)) continue;
                if (!orig.Contains(8) && it.id == 8) continue;
                stripped.Add(it);
            }
            var body = Join(s2.hdr, s2.path, stripped, false);
            UxBin.Require(sp.Reflex ? UxBin.Same(body, 0, data, 4, Math.Max(body.Length, data.Length - 4)) : UxBin.Same(body, data), "round trip failed");
            info.Set("roundtrip", "output minus additions == input body (byte-exact)");
            return o;
        }
        static int IndexOf(IList<UxTdfItem> n, uint id) { for (int k = 0; k < n.Count; k++) if (n[k].id == id) return k; return -1; }

        static readonly uint[] PAD_LINE_IDS = { 8, 9, 10, 0x23, 0x2b };     // global line records (48 B) whose words +0x0c / +0x1c are padding
        static readonly int[] PAD_OFFSETS = { 0x0c, 0x1c };

        /// zero_line_padding(data): the padding words +0x0c and +0x1c of the global line records 8, 9, 10, 0x23, 0x2b set to zero
        /// (Untamed junk, stock 0). Proves that only those words differ and that the result still walks. info: records, words_changed,
        /// bytes_changed, ids, offsets, before {id: [hex, hex]}.
        public static byte[] ZeroLinePadding(byte[] data, out JObj info)
        {
            var sp = Split(data);
            var out_items = new List<UxTdfItem>(sp.items.Count); var before = new JObj(); long nrec = 0, nwords = 0;
            foreach (var it in sp.items)
            {
                var raw = it.raw;
                if (Array.IndexOf(PAD_LINE_IDS, it.id) >= 0 && it.size == 48 && raw.Length == 48)
                {
                    var r = (byte[])raw.Clone(); var hex = new List<object>();
                    foreach (int o in PAD_OFFSETS) hex.Add(UxBin.Hex(UxBin.Slice(raw, o, 4)));
                    before.Set(it.id.ToString(System.Globalization.CultureInfo.InvariantCulture), hex);
                    foreach (int o in PAD_OFFSETS)
                    {
                        if (r[o] != 0 || r[o + 1] != 0 || r[o + 2] != 0 || r[o + 3] != 0) nwords++;
                        r[o] = r[o + 1] = r[o + 2] = r[o + 3] = 0;
                    }
                    raw = r; nrec++;
                }
                out_items.Add(new UxTdfItem(it.id, it.size, raw));
            }
            var res = Join(sp.hdr, sp.path, out_items, sp.Reflex);
            // proofs: same length, walk to EOF, only the padding words of those records differ
            UxBin.Require(res.Length == data.Length, "length changed");
            var s2 = Split(res);
            var probs = WalkCheck(s2.hdr, s2.items);
            if (probs.Count > 0) throw new UxException("output fails the walk check: " + string.Join("; ", probs));
            var allowed = new bool[data.Length];
            long p = (sp.Reflex ? 4 : 0) + 24 + (long)sp.hdr[5];
            foreach (var it in sp.items)
            {
                if (Array.IndexOf(PAD_LINE_IDS, it.id) >= 0 && it.size == 48)
                    foreach (int o in PAD_OFFSETS) for (int k = 0; k < 4; k++) allowed[p + 8 + o + k] = true;
                p += 8 + Plen(it.size);
            }
            long ndiff = 0;
            for (int j = 0; j < data.Length; j++)
                if (data[j] != res[j]) { ndiff++; UxBin.Require(allowed[j], "bytes outside the line padding words changed"); }
            var ids = new List<object>(); foreach (uint i in PAD_LINE_IDS) ids.Add((long)i);
            info = new JObj().Set("records", nrec).Set("words_changed", nwords).Set("bytes_changed", ndiff).Set("ids", ids)
                .Set("offsets", new List<object> { "+0x0c", "+0x1c" }).Set("before", before);
            return res;
        }

        /// summarize(data): the facts dict of a tdf (the keys of the Python function; 'lines' is a SortedDictionary {id: {...}}).
        public static JObj Summarize(byte[] data)
        {
            var sp = Split(data);
            Structure(sp.items, out var glob, out var secs);
            var kinds = new List<uint>(); foreach (var s in secs) kinds.Add(SectionKind(s));
            var lap = LapWalk(glob, secs, out bool closed);
            var pts = new List<double[]>(lap.Count); var f24 = new List<double>(lap.Count); var widths = new List<double>(lap.Count);
            foreach (var (a, b) in lap) pts.Add(Vec3(Field(NodeDict(secs[a].nodes[b]), 21)));
            foreach (var (a, b) in lap) f24.Add(F32(Field(NodeDict(secs[a].nodes[b]), 24)));
            foreach (var (a, b) in lap)
            {
                var nd = NodeDict(secs[a].nodes[b]); var l = Vec3(Field(nd, 22)); var r = Vec3(Field(nd, 23));
                widths.Add(UxRaceNum.PyHypot(l[0] - r[0], l[2] - r[2]));
            }
            widths.Sort();
            var lines = new SortedDictionary<long, object>();
            foreach (uint i in LINE_IDS)
            {
                var r = First(glob, i);
                if (r == null) continue;
                var L = LineRec(r);
                UxBin.Require(L.a < kinds.Count && L.b < secs[(int)L.a].nodes.Count, "tdf: line " + i + " points at a missing node");
                lines[i] = new JObj().Set("section", (long)L.a).Set("node", (long)L.b).Set("section_kind", (long)kinds[(int)L.a])
                    .Set("half_width", UxNp.PyRound(L.width, 2))
                    .Set("pos", new List<object> { UxNp.PyRound(L.pos[0], 2), UxNp.PyRound(L.pos[2], 2) })
                    .Set("dir", new List<object> { UxNp.PyRound(L.dir[0], 3), UxNp.PyRound(L.dir[2], 3) })
                    .Set("dist_to_finish", UxNp.PyRound(F32(Field(NodeDict(secs[(int)L.a].nodes[(int)L.b]), 25)), 1));
            }
            var extent = new List<object>();
            foreach (uint k in new uint[] { 1, 2, 3 }) { var v = Vec3(Need(First(glob, k), "global " + k)); extent.Add(new List<object> { v[0], v[1], v[2] }); }
            var lapSecs = new SortedSet<int>(); foreach (var (a, _) in lap) lapSecs.Add(a);
            var lapSecList = new List<object>(); foreach (int a in lapSecs) lapSecList.Add((long)a);
            var sk = new StringBuilder(); foreach (uint k in kinds) sk.Append(k.ToString(System.Globalization.CultureInfo.InvariantCulture));
            int e0 = 0; while (e0 < sp.path.Length && sp.path[e0] != 0) e0++;
            UxBin.Require(widths.Count > 0, "tdf: the lap has no node");
            return new JObj().Set("kind", sp.kind).Set("nodes", (long)sp.hdr[0]).Set("sections", (long)sp.hdr[1])
                .Set("header_h0c_h10_h14", new List<object> { (long)sp.hdr[2], (long)sp.hdr[3], (long)sp.hdr[4] })
                .Set("source_tga", UxBin.Latin1(sp.path, 0, e0)).Set("extent", extent)
                .Set("lap_length_G5", UxNp.PyRound(F32(Need(First(glob, 5), "global 5 (lap length)")), 1))
                .Set("start_chute_G4", UxNp.PyRound(F32(Need(First(glob, 4), "global 4 (start chute length)")), 1))
                .Set("lap_length_centreline", UxNp.PyRound(PolylineLen(pts, true), 1)).Set("lap_length_sum_f24", UxNp.PyRound(UxRaceNum.PySum(f24), 1))
                .Set("lap_closed", closed).Set("lap_nodes", (long)lap.Count).Set("lap_sections", lapSecList)
                .Set("lap_width_ft_min_median_max", new List<object> { UxNp.PyRound(widths[0], 1), UxNp.PyRound(widths[widths.Count / 2], 1), UxNp.PyRound(widths[widths.Count - 1], 1) })
                .Set("section_kinds", sk.ToString()).Set("lines", lines);
        }
    }
}
