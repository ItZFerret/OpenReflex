// MX vs ATV Untamed converter, foundation: the port of the reference's shared BXML codec (mw2-to-reflex scripts/bxml.py), the document
// form every stage works on: Bxml (the tables as stored: strings, pool, attrs, nodes), Elem (the generic tree) with to_tree / encode_tree
// (byte-exact on DE / Reflex files once the attr flag words are copied back), _pack (zlib.compress at level 6: UxZlib.Compress) and
// stored_size. Untamed's .xenon.database / .level / scene chunks and its .material files are the same little-endian format.
// The Alive importer's Bxml (AliveBxml.cs) is a strict validating reader of another shape; this one mirrors bxml.py call for call.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Reflex.Port.Content.UntamedImport
{
    /// attr_value / node_value of a type-2 value: ('blob', offset).
    public sealed class UxBlobRef
    {
        public readonly long offset;
        public UxBlobRef(long offset) { this.offset = offset; }
    }

    /// bxml.Bxml: a decoded document. attrs[i] = [name, value, flag, type]; nodes[i] = [name, inner, flag, type, first_child, nchildren,
    /// attr_index, nattrs] (inner is signed: -1 = no value), the indices bxml.py uses.
    public sealed class UxBxml
    {
        public const uint Signature = 0x4C4D5842;   // 'BXML'
        public long version, unknown;
        public List<string> strings;
        public byte[] pool;
        public List<long[]> attrs;
        public List<long[]> nodes;
        public byte[] raw;
        long[] _poolOffsets;   // _pool_len's sorted offsets (cached; dropped by Invalidate)

        /// SIZES[type] (2 = None: a blob, sized by the next pool offset).
        public static int Size(long type)
        {
            switch (type) { case 3: case 4: case 5: case 11: return 4; case 7: return 16; case 8: return 64; case 10: return 12; case 9: return 8; }   // 9: two floats (particle documents; ux_water._bx)
            throw new UxException("BXML: no size for value type " + type);
        }

        UxBxml() { }

        /// Bxml(data).
        public UxBxml(byte[] data)
        {
            UxBin.Require(data.Length >= 32, "not BXML (truncated)");
            UxBin.Require(UxBin.U32LE(data, 0) == Signature, "not BXML");
            version = UxBin.U32LE(data, 4); unknown = UxBin.U32LE(data, 28);
            // version bit 16 set = zlib body after a 36-byte header; clear = raw body after a 32-byte header
            byte[] body;
            if ((version & 0x10000) != 0)
            {
                long z = UxBin.U32LE(data, 32);
                body = UxZlib.Decompress(data, 36, (int)Math.Min(z, data.Length - 36));
            }
            else body = UxBin.PySlice(data, 32, data.Length);
            int ns = (int)UxBin.U32LE(data, 8), pp = (int)UxBin.U32LE(data, 12), ps = (int)UxBin.U32LE(data, 16), na = (int)UxBin.U32LE(data, 20), nn = (int)UxBin.U32LE(data, 24);
            UxBin.Require(pp >= 0 && ps >= 0 && (long)pp + ps + (long)na * 12 + (long)nn * 32 <= body.Length, "BXML: tables outside the document");
            strings = new List<string>(ns);
            int start = 0;
            for (int i = 0; i < pp && strings.Count < ns; i++)
                if (body[i] == 0) { strings.Add(Encoding.UTF8.GetString(body, start, i - start)); start = i + 1; }
            if (strings.Count < ns && start <= pp) strings.Add(Encoding.UTF8.GetString(body, start, pp - start));   // split()'s last piece (no final NUL)
            pool = UxBin.Slice(body, pp, ps);
            int ap = pp + ps;
            attrs = new List<long[]>(na);
            for (int i = 0; i < na; i++)
                attrs.Add(new long[] { UxBin.U32LE(body, ap + i * 12), UxBin.U32LE(body, ap + i * 12 + 4), UxBin.U16LE(body, ap + i * 12 + 8), UxBin.U16LE(body, ap + i * 12 + 10) });
            int np = ap + na * 12;
            nodes = new List<long[]>(nn);
            for (int i = 0; i < nn; i++)
            {
                int p = np + i * 32;
                nodes.Add(new long[] { UxBin.U32LE(body, p), UxBin.I32LE(body, p + 4), UxBin.U32LE(body, p + 8), UxBin.U32LE(body, p + 12),
                    UxBin.U32LE(body, p + 16), UxBin.U32LE(body, p + 20), UxBin.U32LE(body, p + 24), UxBin.U32LE(body, p + 28) });
            }
            raw = body;
        }
        public static UxBxml Load(string path) => new UxBxml(System.IO.File.ReadAllBytes(path));

        // ---- accessors
        public string Name(int ni) => strings[(int)nodes[ni][0]];
        /// children(ni): the child node indices.
        public IEnumerable<int> Children(int ni) { var n = nodes[ni]; for (long c = n[4]; c < n[4] + n[5]; c++) yield return (int)c; }
        public int ChildCount(int ni) => (int)nodes[ni][5];
        public int FirstChild(int ni) => (int)nodes[ni][4];
        /// attr_items(ni): (attr index, name).
        public IEnumerable<KeyValuePair<int, string>> AttrItems(int ni)
        {
            var n = nodes[ni];
            for (long ai = n[6]; ai < n[6] + n[7]; ai++) yield return new KeyValuePair<int, string>((int)ai, strings[(int)attrs[(int)ai][0]]);
        }

        /// struct.unpack_from(FMT[t], pool, off): long (types 3, 4, 11), double (5: the float32 value), double[] (7, 8, 10).
        object PoolValue(long t, long off)
        {
            int o = (int)off;
            switch (t)
            {
                case 3: return (long)UxBin.I32LE(pool, o);
                case 4: return (long)UxBin.U32LE(pool, o);
                case 5: return (double)UxBin.F32LE(pool, o);
                case 11: return (long)pool[o];
                case 7: case 8: case 10:
                {
                    int n = t == 7 ? 4 : t == 8 ? 16 : 3; var r = new double[n];
                    for (int k = 0; k < n; k++) r[k] = UxBin.F32LE(pool, o + 4 * k);
                    return r;
                }
            }
            throw new UxException("BXML: no format for value type " + t);
        }
        /// attr_value(ai): string (type 1), UxBlobRef (type 2), long / double / double[] (pool types).
        public object AttrValue(int ai)
        {
            var a = attrs[ai];
            if (a[3] == 1) return strings[(int)a[1]];
            if (a[3] == 2) return new UxBlobRef(a[1]);
            return PoolValue(a[3], a[1]);
        }
        /// attr(ni, key): null when the node has no such attribute.
        public object Attr(int ni, string key)
        {
            foreach (var kv in AttrItems(ni)) if (kv.Value == key) return AttrValue(kv.Key);
            return null;
        }
        public string AttrStr(int ni, string key) => Attr(ni, key) as string;
        /// attr(ni, key) of an integer attribute (types 3, 4, 11); `missing` when absent or not an integer.
        public long AttrInt(int ni, string key, long missing = 0) => Attr(ni, key) is long l ? l : missing;
        /// attr_index(ni, key): -1 when absent (Python: None).
        public int AttrIndex(int ni, string key)
        {
            foreach (var kv in AttrItems(ni)) if (kv.Value == key) return kv.Key;
            return -1;
        }
        /// set_pool_attr(ai, value): struct.pack_into(FMT[type], pool, offset, value).
        public void SetPoolAttr(int ai, params double[] value)
        {
            var a = attrs[ai];
            UxBin.Require(a[3] != 1, "BXML: set_pool_attr on a string attribute");
            int o = (int)a[1];
            switch (a[3])
            {
                case 3: UxBin.PutI32(pool, o, checked((int)value[0])); break;
                case 4: UxBin.PutU32(pool, o, checked((uint)value[0])); break;
                case 5: UxBin.PutF32(pool, o, (float)value[0]); break;
                case 11: pool[o] = checked((byte)value[0]); break;
                case 7: case 8: case 10:
                    UxBin.Require(value.Length == (a[3] == 7 ? 4 : a[3] == 8 ? 16 : 3), "BXML: component count");
                    for (int k = 0; k < value.Length; k++) UxBin.PutF32(pool, o + 4 * k, (float)value[k]);
                    break;
                default: throw new UxException("BXML: no format for value type " + a[3]);
            }
        }
        /// node_value(ni): null, string (types 0, 1), UxBlobRef (2) or the pool value.
        public object NodeValue(int ni)
        {
            var n = nodes[ni];
            if (n[1] < 0) return null;
            if (n[3] == 0 || n[3] == 1) return strings[(int)n[1]];
            if (n[3] == 2) return new UxBlobRef(n[1]);
            return PoolValue(n[3], n[1]);
        }
        /// find(ni, *path): the node reached by child names; -1 when a step is missing (Python: None).
        public int Find(int ni, params string[] path)
        {
            foreach (string name in path)
            {
                int next = -1;
                foreach (int c in Children(ni)) if (Name(c) == name) { next = c; break; }
                if (next < 0) return -1;
                ni = next;
            }
            return ni;
        }
        public List<int> FindAll(int ni, string name)
        {
            var r = new List<int>();
            foreach (int c in Children(ni)) if (Name(c) == name) r.Add(c);
            return r;
        }
        /// replace_strings(mapping): in place, the string indices stay valid.
        public void ReplaceStrings(IDictionary<string, string> mapping)
        {
            for (int i = 0; i < strings.Count; i++) if (mapping.TryGetValue(strings[i], out string s)) strings[i] = s;
        }
        public void Invalidate() { _poolOffsets = null; }

        /// to_bytes(): the tables re-packed (zlib.compress for a version with bit 16).
        public byte[] ToBytes(bool stored = false)
        {
            var w = new UxWriter(pool.Length + attrs.Count * 12 + nodes.Count * 32 + 1024);
            foreach (string s in strings) { w.Bytes(Encoding.UTF8.GetBytes(s)); w.U8(0); }
            int pp = w.Length;
            w.Bytes(pool);
            foreach (var a in attrs) { w.U32((uint)a[0]); w.U32((uint)a[1]); w.U16((ushort)a[2]); w.U16((ushort)a[3]); }
            foreach (var n in nodes) { w.U32((uint)n[0]); w.I32((int)n[1]); w.U32((uint)n[2]); w.U32((uint)n[3]); w.U32((uint)n[4]); w.U32((uint)n[5]); w.U32((uint)n[6]); w.U32((uint)n[7]); }
            return Pack(version, strings.Count, pp, pool.Length, attrs.Count, nodes.Count, unknown, w.ToArray(), stored);
        }

        /// _pack: header + zlib.compress(raw) (version bit 16) or the 32-byte header + raw. stored = stored deflate blocks instead of
        /// level 6 (decodes to the same document; not the bytes Python writes).
        public static byte[] Pack(long version, int ns, int pp, int ps, int na, int nn, long unknown, byte[] rawBody, bool stored = false)
        {
            var h = new UxWriter(36 + rawBody.Length);
            h.U32(Signature).U32((uint)version).U32((uint)ns).U32((uint)pp).U32((uint)ps).U32((uint)na).U32((uint)nn).U32((uint)unknown);
            if ((version & 0x10000) != 0)
            {
                byte[] comp = stored ? UxZlib.Stored(rawBody) : UxZlib.Compress(rawBody);
                h.U32((uint)comp.Length).Bytes(comp);
            }
            else h.Bytes(rawBody);
            return h.ToArray();
        }

        /// stored_size(data, p): the size of the BXML document starting at data[p].
        public static int StoredSize(byte[] data, int p = 0)
        {
            long ver = UxBin.U32LE(data, p + 4);
            if ((ver & 0x10000) != 0) return 36 + (int)UxBin.U32LE(data, p + 32);
            return 32 + (int)UxBin.U32LE(data, p + 12) + (int)UxBin.U32LE(data, p + 16) + (int)UxBin.U32LE(data, p + 20) * 12 + (int)UxBin.U32LE(data, p + 24) * 32;
        }

        // ---- debug XML dump (to_xml): used by the reference to compare documents
        /// format(v, '.7g').
        public static string G7(double v)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            string s = v.ToString("G7", CultureInfo.InvariantCulture);
            int e = s.IndexOf('E');
            if (e < 0) return s;
            int n = int.Parse(s.Substring(e + 1), CultureInfo.InvariantCulture);
            return s.Substring(0, e) + "e" + (n < 0 ? "-" : "+") + Math.Abs(n).ToString("00", CultureInfo.InvariantCulture);
        }
        static string XmlFmt(object v)
        {
            switch (v)
            {
                case double d: return G7(d);
                case UxBlobRef b: return "blob@" + b.offset.ToString(CultureInfo.InvariantCulture);
                case double[] a: return string.Join(",", Array.ConvertAll(a, G7));
                case long l: return l.ToString(CultureInfo.InvariantCulture);
                default: return Convert.ToString(v, CultureInfo.InvariantCulture);
            }
        }
        static string XmlEsc(string s) => s.Replace("&", "&amp;").Replace("\"", "&quot;").Replace("<", "&lt;").Replace(">", "&gt;");
        public string ToXml()
        {
            var o = new StringBuilder();
            void Rec(int ni, int ind)
            {
                var n = nodes[ni];
                var s = new StringBuilder(); s.Append(' ', ind).Append('<').Append(Name(ni));
                foreach (var kv in AttrItems(ni)) s.Append(' ').Append(kv.Value).Append("=\"").Append(XmlEsc(XmlFmt(AttrValue(kv.Key)))).Append('"');
                object v = NodeValue(ni);
                if (n[5] == 0 && v == null) { o.Append(s).Append("/>\n"); return; }
                s.Append('>');
                if (v != null) s.Append(XmlEsc(XmlFmt(v)));
                if (n[5] == 0) { o.Append(s).Append("</").Append(Name(ni)).Append(">\n"); return; }
                o.Append(s).Append('\n');
                foreach (int c in Children(ni)) Rec(c, ind + 2);
                o.Append(' ', ind).Append("</").Append(Name(ni)).Append(">\n");
            }
            Rec(0, 0);
            return o.ToString();
        }

        // ---- generic tree
        /// _pool_len(b, typ, off, count).
        public int PoolLen(long typ, long off, long count)
        {
            if (typ == 2)
            {
                if (_poolOffsets == null)
                {
                    var set = new SortedSet<long> { pool.Length };
                    foreach (var n in nodes) if (n[3] != 0 && n[3] != 1 && n[1] >= 0) set.Add(n[1]);
                    foreach (var a in attrs) if (a[3] != 1) set.Add(a[1]);
                    _poolOffsets = new long[set.Count]; set.CopyTo(_poolOffsets);
                }
                int lo = 0, hi = _poolOffsets.Length;     // first offset > off
                while (lo < hi) { int mid = (lo + hi) >> 1; if (_poolOffsets[mid] > off) hi = mid; else lo = mid + 1; }
                long next = lo < _poolOffsets.Length ? _poolOffsets[lo] : off;
                return (int)(next - off);
            }
            return Size(typ) * (int)Math.Max(1, count);
        }

        /// to_tree(b).
        public UxElem ToTree() => ToTree(0);
        public UxElem ToTree(int i)
        {
            var n = nodes[i];
            var at = new List<UxAttr>((int)n[7]);
            for (long ai = n[6]; ai < n[6] + n[7]; ai++)
            {
                var a = attrs[(int)ai];
                if (a[3] == 1) at.Add(new UxAttr(strings[(int)a[0]], strings[(int)a[1]]));
                else at.Add(new UxAttr(strings[(int)a[0]], (int)a[3], UxBin.PySlice(pool, (int)a[1], (int)a[1] + PoolLen(a[3], a[1], a[2]))));
            }
            long vt = n[3], cnt = n[2]; object val = null;
            if (n[1] >= 0)
            {
                if (vt == 0 || vt == 1) { vt = 1; val = strings[(int)n[1]]; }
                else val = UxBin.PySlice(pool, (int)n[1], (int)n[1] + PoolLen(vt, n[1], cnt));
            }
            var kids = new List<UxElem>((int)n[5]);
            for (long c = n[4]; c < n[4] + n[5]; c++) kids.Add(ToTree((int)c));
            return new UxElem(strings[(int)n[0]], at, (int)(n[1] >= 0 ? vt : n[3]), cnt, val, kids);
        }

        /// encode_tree(root, version, unknown)[0]: the document bytes.
        public static byte[] EncodeTree(UxElem root, long version = 66538, long unknown = 0, bool stored = false) => EncodeTree(root, version, unknown, out _, stored);
        /// encode_tree(root, version, unknown): (document bytes, raw body).
        public static byte[] EncodeTree(UxElem root, long version, long unknown, out byte[] rawBody, bool stored = false)
        {
            var strings = new List<string>(); var sidx = new Dictionary<string, int>(StringComparer.Ordinal);
            int S(string s)
            {
                if (!sidx.TryGetValue(s, out int i)) { i = strings.Count; sidx[s] = i; strings.Add(s); }
                return i;
            }
            // node table: a node's children are contiguous; each child's subtree follows recursively (pre-order of sibling lists)
            // (first is keyed by the element object, as Python's first[id(e)])
            var order = new List<UxElem> { root };
            var first = new Dictionary<UxElem, int>(ReferenceComparer.Instance);
            void Layout(UxElem e)
            {
                first[e] = order.Count;
                order.AddRange(e.children);
                foreach (var c in e.children) Layout(c);
            }
            Layout(root);
            var pool = new UxWriter(1024); var attrs = new UxWriter(1024); var nodes = new UxWriter(1024);
            int nattrs = 0;
            for (int idx = 0; idx < order.Count; idx++)
            {
                var e = order[idx];
                int ni = S(e.tag), ai0 = nattrs;
                foreach (var a in e.attrs)
                {
                    int nmi = S(a.name);
                    if (a.type == 1) { attrs.U32((uint)nmi).U32((uint)S(a.str)).U16(0).U16(1); }
                    else { attrs.U32((uint)nmi).U32((uint)pool.Length).U16(0).U16((ushort)a.type); pool.Bytes(a.raw); }
                    nattrs++;
                }
                int inner;
                if (e.value == null) inner = -1;
                else if (e.vtype == 1) inner = S((string)e.value);
                else { inner = pool.Length; pool.Bytes((byte[])e.value); }
                nodes.U32((uint)ni).I32(inner).U32((uint)e.vcount).U32((uint)e.vtype).U32((uint)first[e]).U32((uint)e.children.Count).U32((uint)ai0).U32((uint)e.attrs.Count);
            }
            var w = new UxWriter(pool.Length + attrs.Length + nodes.Length + 1024);
            foreach (string s in strings) { w.Bytes(Encoding.UTF8.GetBytes(s)); w.U8(0); }
            int pp = w.Length;
            w.Bytes(pool.ToArray()).Bytes(attrs.ToArray()).Bytes(nodes.ToArray());
            rawBody = w.ToArray();
            return Pack(version, strings.Count, pp, pool.Length, nattrs, order.Count, unknown, rawBody, stored);
        }

        sealed class ReferenceComparer : IEqualityComparer<UxElem>
        {
            public static readonly ReferenceComparer Instance = new ReferenceComparer();
            public bool Equals(UxElem a, UxElem b) => ReferenceEquals(a, b);
            public int GetHashCode(UxElem e) => System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(e);
        }

        /// flags_walk(b, start): the attr 'flag' words in Elem pre-order (to_tree / encode_tree order) from node `start`.
        public List<long[]> FlagsWalk(int start = 0)
        {
            var o = new List<long[]>();
            void Rec(int i)
            {
                var n = nodes[i]; var f = new long[n[7]];
                for (int k = 0; k < f.Length; k++) f[k] = attrs[(int)n[6] + k][2];
                o.Add(f);
                for (long c = n[4]; c < n[4] + n[5]; c++) Rec((int)c);
            }
            Rec(start);
            return o;
        }
        /// apply_flags(b, flags): writes the flag words back in the same order.
        public void ApplyFlags(List<long[]> flags)
        {
            int k = 0;
            void Rec(int i)
            {
                var n = nodes[i]; var f = flags[k++];
                UxBin.Require(f.Length == n[7], "BXML: attr flag list does not fit the node");
                for (int j = 0; j < f.Length; j++) attrs[(int)n[6] + j][2] = f[j];
                for (long c = n[4]; c < n[4] + n[5]; c++) Rec((int)c);
            }
            Rec(0);
            UxBin.Require(k == flags.Count, "BXML: attr flag lists left over");
        }
    }

    /// One attribute of an Elem: [name, type, value] (value: str for type 1, raw pool bytes otherwise).
    public sealed class UxAttr
    {
        public string name; public int type; public string str; public byte[] raw;
        public UxAttr(string name, string value) { this.name = name; type = 1; str = value; }
        public UxAttr(string name, int type, byte[] raw) { this.name = name; this.type = type; this.raw = raw; }
        /// a[2].
        public object Value => type == 1 ? (object)str : raw;
        public UxAttr Copy() => type == 1 ? new UxAttr(name, str) : new UxAttr(name, type, (byte[])raw.Clone());
        public static UxAttr U32(string name, uint v) { var b = new byte[4]; UxBin.PutU32(b, 0, v); return new UxAttr(name, 4, b); }
        public static UxAttr I32(string name, int v) { var b = new byte[4]; UxBin.PutI32(b, 0, v); return new UxAttr(name, 3, b); }
    }

    /// bxml.Elem: tag, attrs, vtype (0 none, 1 string, else pool type), vcount (the node 'flag': element count of pool values), value
    /// (string for type 1, raw bytes for pool types, null), children.
    public sealed class UxElem
    {
        public string tag; public List<UxAttr> attrs; public int vtype; public long vcount; public object value; public List<UxElem> children;
        public UxElem(string tag, List<UxAttr> attrs = null, int vtype = 0, long vcount = 0, object value = null, List<UxElem> children = null)
        {
            this.tag = tag; this.attrs = attrs ?? new List<UxAttr>(); this.vtype = vtype; this.vcount = vcount; this.value = value;
            this.children = children ?? new List<UxElem>();
        }
        public UxElem Find(string tag) { foreach (var c in children) if (c.tag == tag) return c; return null; }
        public List<UxElem> FindAll(string tag) { var r = new List<UxElem>(); foreach (var c in children) if (c.tag == tag) r.Add(c); return r; }
        /// attr(name): the value (string or byte[]) of the first attribute of that name, or null.
        public object Attr(string name) { foreach (var a in attrs) if (a.name == name) return a.Value; return null; }
        public string AttrStr(string name) => Attr(name) as string;
        public UxAttr AttrObj(string name) { foreach (var a in attrs) if (a.name == name) return a; return null; }
        /// iter(): this element and its descendants, depth first.
        public IEnumerable<UxElem> Iter()
        {
            yield return this;
            foreach (var c in children) foreach (var d in c.Iter()) yield return d;
        }
        /// copy.deepcopy(e).
        public UxElem DeepCopy()
        {
            var a = new List<UxAttr>(attrs.Count); foreach (var x in attrs) a.Add(x.Copy());
            var k = new List<UxElem>(children.Count); foreach (var c in children) k.Add(c.DeepCopy());
            return new UxElem(tag, a, vtype, vcount, value is byte[] b ? (object)b.Clone() : value, k);
        }
        public byte[] Bytes => (byte[])value;
        public string Text => (string)value;
    }
}
