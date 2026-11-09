// BXML (version flags 0x000103EA) as the Alive converter needs it: the strict reader of _re/formats/bxml.py (Document) and
// scene_read.py (SceneDocument: raw 0x3EA documents, opaque / typed-array values), the records view (as_record), the element view
// (to_element) and its re-encoding (alive_install.encode_element), and serialization with stored deflate blocks.
using System;
using System.Collections.Generic;
using System.Text;

namespace Reflex.Port.Content.AliveImport
{
    internal sealed class BxAttr { public uint name, refv; public ushort count, kind; }
    internal sealed class BxNode
    {
        public uint name, refv, count, kind, childStart, childCount, attrStart, attrCount;
        public BxNode Clone() => (BxNode)MemberwiseClone();
    }

    /// An opaque byte value (kind 2), as SceneDocument returns {"opaque_bytes": hex}.
    internal sealed class Opaque { public byte[] bytes; }

    /// SceneDocument.as_record: name, value, attributes (in order), children.
    internal sealed class Record
    {
        public string name; public object value;
        public readonly List<KeyValuePair<string, object>> attributes = new List<KeyValuePair<string, object>>();
        public readonly List<Record> children = new List<Record>();
        public object Attr(string key) { foreach (var kv in attributes) if (kv.Key == key) return kv.Value; return null; }
        public bool HasAttr(string key) { foreach (var kv in attributes) if (kv.Key == key) return true; return false; }
    }

    /// An ElementTree element as bxml.Document.to_element builds it (attribute values as their XML text, or the typed value it stands for).
    internal sealed class Elem
    {
        public string tag;
        public readonly List<KeyValuePair<string, EVal>> attrib = new List<KeyValuePair<string, EVal>>();
        public string text;
        public readonly List<Elem> children = new List<Elem>();
        public Elem(string tag) { this.tag = tag; }
        public string Get(string key)
        {
            foreach (var kv in attrib) if (kv.Key == key) return kv.Value.Text;
            return null;
        }
        public Elem Set(string key, EVal v)
        {
            for (int i = 0; i < attrib.Count; i++) if (attrib[i].Key == key) { attrib[i] = new KeyValuePair<string, EVal>(key, v); return this; }
            attrib.Add(new KeyValuePair<string, EVal>(key, v)); return this;
        }
        public Elem Find(string tag) { foreach (var c in children) if (c.tag == tag) return c; return null; }
        public IEnumerable<Elem> FindAll(string tag) { foreach (var c in children) if (c.tag == tag) yield return c; }
        /// Element.iter(): this element and its descendants, depth first.
        public IEnumerable<Elem> Iter()
        {
            yield return this;
            foreach (var c in children) foreach (var d in c.Iter()) yield return d;
        }
        public IEnumerable<Elem> Iter(string tag) { foreach (var e in Iter()) if (e.tag == tag) yield return e; }
    }

    /// An attribute value: a plain string, or a typed value ("_uint:5" ...) kept as its numbers.
    internal sealed class EVal
    {
        public int kind;           // 1 string, 3 int, 4 uint, 5 float, 10 vector3, 11 bool
        public string str;         // kind 1
        public double[] numbers;   // typed
        public static EVal S(string s) => new EVal { kind = 1, str = s };
        public static EVal Typed(int kind, params double[] v) => new EVal { kind = kind, numbers = v };
        public static EVal Uint(long v) => Typed(4, v);
        /// The XML text this value has in the element tree ("_uint:5", "_bool:true", ...).
        public string Text
        {
            get
            {
                if (kind == 1) return str;
                string t;
                switch (kind)
                {
                    case 3: case 4: t = ((long)numbers[0]).ToString(System.Globalization.CultureInfo.InvariantCulture); break;
                    case 11: t = numbers[0] != 0 ? "true" : "false"; break;
                    case 5: t = PyFormat.G9(numbers[0]); break;
                    default: t = string.Join(",", Array.ConvertAll(numbers, x => PyFormat.G9(x))); break;
                }
                return "_" + Bxml.TypeName(kind) + ":" + t;
            }
        }
    }

    internal sealed class Bxml
    {
        public const uint VersionFlags = 0x000103EA, None = 0xFFFFFFFF;
        public List<string> strings = new List<string>();
        public List<byte[]> stringBytes = new List<byte[]>();
        public byte[] blob = Array.Empty<byte>();
        public List<BxAttr> attrs = new List<BxAttr>();
        public List<BxNode> nodes = new List<BxNode>();
        public bool scene;     // SceneDocument rules
        public int float2;     // kind 9 values: 0 not allowed, 1 forest ([x, y] pairs), 2 particle (flat list)

        public static string TypeName(int kind)
        {
            switch (kind) { case 0: return "empty"; case 1: return "string"; case 3: return "int"; case 4: return "uint"; case 5: return "float"; case 10: return "vector3"; case 11: return "bool"; }
            return "kind" + kind;
        }

        public static Bxml Parse(byte[] raw, bool scene = false, int float2 = 0)
        {
            var d = new Bxml { scene = scene, float2 = float2 };
            Bin.Require(raw.Length >= 32, scene ? "Truncated scene BXML" : "Truncated BXML header");
            Bin.Require(Bin.StartsWith(raw, "BXML"), "Invalid BXML magic");
            uint flags = Bin.U32LE(raw, 4);
            byte[] payload; int ns, ss, bs, na, nn;
            if (scene && flags == 0x3ea)
            {
                ns = Bin.I32LE(raw, 8); ss = Bin.I32LE(raw, 12); bs = Bin.I32LE(raw, 16); na = Bin.I32LE(raw, 20); nn = Bin.I32LE(raw, 24);
                uint reserved = Bin.U32LE(raw, 28);
                long total = (long)ss + bs + (long)na * 12 + (long)nn * 32;
                Bin.Require(reserved == 0 && total == raw.Length - 32, "Raw BXML table sizes differ from resource size");
                Bin.Require(raw.Length <= 32 * 1024 * 1024, "Raw BXML exceeds inspection limit");
                Bin.Require(total > 0, "Decoded size exceeds configured limit or is zero");
                payload = Bin.Slice(raw, 32, (int)total);
            }
            else
            {
                Bin.Require(raw.Length >= 36, "Truncated BXML header");
                Bin.Require(flags == VersionFlags, $"Unsupported version/flags: 0x{flags:x8}");
                ns = Bin.I32LE(raw, 8); ss = Bin.I32LE(raw, 12); bs = Bin.I32LE(raw, 16); na = Bin.I32LE(raw, 20); nn = Bin.I32LE(raw, 24);
                Bin.Require(Bin.U32LE(raw, 28) == 0, "Unverified nonzero header field at byte 28");
                Bin.Require(Bin.U32LE(raw, 32) == raw.Length - 36, "Compressed length mismatch");
                long size = (long)ss + bs + (long)na * 12 + (long)nn * 32;
                long max = scene ? 32L * 1024 * 1024 : 256L * 1024 * 1024;
                Bin.Require(size > 0 && size <= max, "Decoded size exceeds configured limit or is zero");
                payload = Bin.Inflate(raw, 36, raw.Length - 36, (int)size);
            }
            Bin.Require(ss >= 1 && payload[ss - 1] == 0, "String pool lacks terminating NUL");
            int start = 0;
            for (int i = 0; i < ss; i++)
                if (payload[i] == 0) { d.stringBytes.Add(Bin.Slice(payload, start, i - start)); start = i + 1; }
            Bin.Require(d.stringBytes.Count == ns, "String count mismatch");
            var utf8 = new UTF8Encoding(false, true);
            try { foreach (var sbs in d.stringBytes) d.strings.Add(utf8.GetString(sbs)); }
            catch (Exception) { throw new AliveImportException("Unverified non-UTF-8 string pool"); }
            d.blob = Bin.Slice(payload, ss, bs);
            int ap = ss + bs, np = ap + na * 12;
            for (int i = 0; i < na; i++)
                d.attrs.Add(new BxAttr { name = Bin.U32LE(payload, ap + i * 12), refv = Bin.U32LE(payload, ap + i * 12 + 4), count = Bin.U16LE(payload, ap + i * 12 + 8), kind = Bin.U16LE(payload, ap + i * 12 + 10) });
            for (int i = 0; i < nn; i++)
            {
                int p = np + i * 32;
                d.nodes.Add(new BxNode { name = Bin.U32LE(payload, p), refv = Bin.U32LE(payload, p + 4), count = Bin.U32LE(payload, p + 8), kind = Bin.U32LE(payload, p + 12),
                    childStart = Bin.U32LE(payload, p + 16), childCount = Bin.U32LE(payload, p + 20), attrStart = Bin.U32LE(payload, p + 24), attrCount = Bin.U32LE(payload, p + 28) });
            }
            d.Validate();
            return d;
        }

        static int PackSize(uint kind) { switch (kind) { case 3: case 4: case 5: case 11: return 4; case 10: return 12; case 7: return 16; case 8: return 64; } return -1; }

        object ScalarAt(uint kind, int at)
        {
            switch (kind)
            {
                case 3: return (long)(int)Bin.U32LE(blob, at);
                case 4: return (long)Bin.U32LE(blob, at);
                case 11: { uint v = Bin.U32LE(blob, at); return (long)v; }
                case 5: return (double)Bin.F32LE(blob, at);
                default:
                {
                    int n = PackSize(kind) / 4; var r = new double[n];
                    for (int k = 0; k < n; k++) r[k] = Bin.F32LE(blob, at + 4 * k);
                    return r;
                }
            }
        }

        static bool Finite(object v)
        {
            if (v is double d) return !double.IsNaN(d) && !double.IsInfinity(d);
            if (v is double[] a) { foreach (var x in a) if (double.IsNaN(x) || double.IsInfinity(x)) return false; }
            return true;
        }

        public object Value(uint kind, uint refv, uint count, bool isNode)
        {
            if (kind == 0)
            {
                Bin.Require(isNode && refv == None && count == 0, "Invalid empty value");
                return null;
            }
            if (kind == 1)
            {
                Bin.Require(count == 0 && refv < strings.Count, "Invalid string value reference/count");
                return strings[(int)refv];
            }
            if (!scene)
            {
                Bin.Require(kind == 3 || kind == 4 || kind == 5 || kind == 10 || kind == 11, $"Unknown value type {kind}");
                Bin.Require(count == 1, "Unverified typed array count (only scalar/vector values observed)");
                Bin.Require(refv + (long)PackSize(kind) <= blob.Length, "Typed value exceeds blob");
                var v = ScalarAt(kind, (int)refv);
                if (kind == 11) { long b = (long)v; Bin.Require(b == 0 || b == 1, "Invalid bool32 value"); return b == 1; }
                Bin.Require(Finite(v), "Non-finite typed value");
                return v;
            }
            if (kind == 2)
            {
                Bin.Require(refv + (long)count <= blob.Length, "Opaque BXML bytes exceed blob");
                return new Opaque { bytes = Bin.Slice(blob, (int)refv, (int)count) };
            }
            if (kind == 9 && float2 != 0)
            {
                Bin.Require(refv + (long)count * 8 <= blob.Length, float2 == 1 ? "Forest float2 bounds" : "Particle float2 bounds");
                if (float2 == 1)
                {
                    var l = new List<object>();
                    for (int i = 0; i < count; i++) l.Add(new double[] { Bin.F32LE(blob, (int)refv + i * 8), Bin.F32LE(blob, (int)refv + i * 8 + 4) });
                    return l;
                }
                var f = new double[count * 2];
                for (int i = 0; i < f.Length; i++) f[i] = Bin.F32LE(blob, (int)refv + i * 4);
                return f;
            }
            int size = PackSize(kind);
            Bin.Require(size > 0, $"Unsupported scene BXML type {kind}");
            Bin.Require(count <= 65536 && refv + (long)count * size <= blob.Length, "Scene typed array exceeds blob or inspection limit");
            var values = new List<object>();
            for (int i = 0; i < count; i++)
            {
                var v = ScalarAt(kind, (int)refv + i * size);
                Bin.Require(Finite(v), "Nonfinite scene value");
                if (kind == 11) { long b = (long)v; Bin.Require(b == 0 || b == 1, "Invalid scene bool"); }
                values.Add(v);
            }
            return count == 1 ? values[0] : values;
        }
        public object Value(BxNode n) => Value(n.kind, n.refv, n.count, true);
        public object Value(BxAttr a) => Value(a.kind, a.refv, a.count, false);

        public void Validate()
        {
            Bin.Require(nodes.Count > 0, "Missing root node");
            var owners = new int[nodes.Count]; var attrOwners = new int[attrs.Count];
            for (int i = 0; i < nodes.Count; i++)
            {
                var n = nodes[i];
                Bin.Require(n.name < strings.Count, $"Invalid node name at {i}");
                Value(n);
                Bin.Require(n.childStart <= nodes.Count && n.childStart + (long)n.childCount <= nodes.Count, $"Invalid child range at node {i}");
                Bin.Require(n.attrStart <= attrs.Count && n.attrStart + (long)n.attrCount <= attrs.Count, $"Invalid attribute range at node {i}");
                for (uint c = n.childStart; c < n.childStart + n.childCount; c++) { Bin.Require(c > i, "Child reference is backward/cyclic"); owners[c]++; }
                var names = new HashSet<string>();
                for (uint a = n.attrStart; a < n.attrStart + n.attrCount; a++)
                {
                    attrOwners[a]++;
                    var item = attrs[(int)a];
                    Bin.Require(item.name < strings.Count, $"Invalid attribute at {a}");
                    Bin.Require(names.Add(strings[(int)item.name]), $"Duplicate attribute at node {i}");
                    Value(item);
                }
            }
            Bin.Require(owners[0] == 0, "Tree contains orphan/shared nodes");
            for (int i = 1; i < owners.Length; i++) Bin.Require(owners[i] == 1, "Tree contains orphan/shared nodes");
            foreach (var o in attrOwners) Bin.Require(o == 1, "Tree contains orphan/shared attributes");
        }

        public IEnumerable<int> Children(int i) { var n = nodes[i]; for (uint c = n.childStart; c < n.childStart + n.childCount; c++) yield return (int)c; }
        public IEnumerable<int> AttrIndices(int i) { var n = nodes[i]; for (uint a = n.attrStart; a < n.attrStart + n.attrCount; a++) yield return (int)a; }
        public string Tag(int i) => strings[(int)nodes[i].name];

        public List<KeyValuePair<string, object>> AttributesAt(int i)
        {
            var r = new List<KeyValuePair<string, object>>();
            foreach (int a in AttrIndices(i)) r.Add(new KeyValuePair<string, object>(strings[(int)attrs[a].name], Value(attrs[a])));
            return r;
        }
        public object AttributeAt(int i, string key) { foreach (var kv in AttributesAt(i)) if (kv.Key == key) return kv.Value; return null; }

        public Record AsRecord(int i = 0)
        {
            var r = new Record { name = Tag(i), value = Value(nodes[i]) };
            r.attributes.AddRange(AttributesAt(i));
            foreach (int c in Children(i)) r.children.Add(AsRecord(c));
            return r;
        }

        /// pool + blob + attributes + nodes (Document.payload), and the pool length.
        public byte[] Payload(out int poolLength)
        {
            var o = new System.IO.MemoryStream();
            foreach (var s in stringBytes) { o.Write(s, 0, s.Length); o.WriteByte(0); }
            if (stringBytes.Count == 0) o.WriteByte(0);   // b"\0".join([]) + b"\0"
            poolLength = (int)o.Length;
            o.Write(blob, 0, blob.Length);
            var rec = new byte[32];
            foreach (var a in attrs)
            {
                Bin.PutU32LE(rec, 0, a.name); Bin.PutU32LE(rec, 4, a.refv); Bin.PutU16LE(rec, 8, a.count); Bin.PutU16LE(rec, 10, a.kind);
                o.Write(rec, 0, 12);
            }
            foreach (var n in nodes)
            {
                Bin.PutU32LE(rec, 0, n.name); Bin.PutU32LE(rec, 4, n.refv); Bin.PutU32LE(rec, 8, n.count); Bin.PutU32LE(rec, 12, n.kind);
                Bin.PutU32LE(rec, 16, n.childStart); Bin.PutU32LE(rec, 20, n.childCount); Bin.PutU32LE(rec, 24, n.attrStart); Bin.PutU32LE(rec, 28, n.attrCount);
                o.Write(rec, 0, 32);
            }
            return o.ToArray();
        }

        /// alive_convert.serialize_scene_document: header + stored zlib payload; the result is parsed again.
        public byte[] Serialize()
        {
            var payload = Payload(out int pool);
            var z = Bin.ZlibStored(payload);
            var h = Header(strings.Count, pool, blob.Length, attrs.Count, nodes.Count, z.Length);
            var result = new byte[h.Length + z.Length];
            Buffer.BlockCopy(h, 0, result, 0, h.Length); Buffer.BlockCopy(z, 0, result, h.Length, z.Length);
            Parse(result, scene, float2);
            return result;
        }

        public static byte[] Header(int ns, int ss, int bs, int na, int nn, int compressed)
        {
            var h = new byte[36];
            h[0] = (byte)'B'; h[1] = (byte)'X'; h[2] = (byte)'M'; h[3] = (byte)'L';
            Bin.PutU32LE(h, 4, VersionFlags); Bin.PutU32LE(h, 8, (uint)ns); Bin.PutU32LE(h, 12, (uint)ss); Bin.PutU32LE(h, 16, (uint)bs);
            Bin.PutU32LE(h, 20, (uint)na); Bin.PutU32LE(h, 24, (uint)nn); Bin.PutU32LE(h, 28, 0); Bin.PutU32LE(h, 32, (uint)compressed);
            return h;
        }

        // ------------------------------------------------------------------------------------- element view

        public Elem ToElement()
        {
            var els = new Elem[nodes.Count];
            for (int i = 0; i < nodes.Count; i++)
            {
                var n = nodes[i];
                var e = new Elem(Tag(i));
                foreach (int a in AttrIndices(i))
                {
                    var item = attrs[a]; var v = Value(item);
                    e.attrib.Add(new KeyValuePair<string, EVal>(strings[(int)item.name], ToEVal(item.kind, v)));
                }
                if (n.kind != 0 && n.kind != 1) e.attrib.Add(new KeyValuePair<string, EVal>("_valuetype", EVal.S(TypeName((int)n.kind))));
                if (n.kind != 0) e.text = FormatValue((int)n.kind, Value(n));
                els[i] = e;
            }
            for (int i = 0; i < nodes.Count; i++) foreach (int c in Children(i)) els[i].children.Add(els[c]);
            return els[0];
        }

        static EVal ToEVal(int kind, object v)
        {
            switch (kind)
            {
                case 1: return EVal.S((string)v);
                case 3: case 4: return EVal.Typed(kind, (long)v);
                case 5: return EVal.Typed(5, (double)v);
                case 10: return EVal.Typed(10, (double[])v);
                case 11: return EVal.Typed(11, (bool)v ? 1 : 0);
            }
            throw new AliveImportException("Element view of value type " + kind);
        }

        static string FormatValue(int kind, object v)
        {
            switch (kind)
            {
                case 1: return (string)v;
                case 11: return (bool)v ? "true" : "false";
                case 5: return PyFormat.G9((double)v);
                case 10: return string.Join(",", Array.ConvertAll((double[])v, x => PyFormat.G9(x)));
                default: return ((long)v).ToString(System.Globalization.CultureInfo.InvariantCulture);
            }
        }

        /// alive_install.encode_element.
        public static byte[] EncodeElement(Elem root)
        {
            var strings = new List<string>(); var index = new Dictionary<string, int>(StringComparer.Ordinal);
            var blob = new System.IO.MemoryStream();
            var attrs = new List<uint[]>(); var nodes = new List<uint[]>();
            int St(string s) { if (!index.TryGetValue(s, out int i)) { i = strings.Count; strings.Add(s); index[s] = i; } return i; }
            uint[] Val(EVal v)
            {
                if (v == null) return new uint[] { None, 0, 0 };
                int kind = v.kind; double[] nums = v.numbers;
                if (kind == 1)
                {
                    string s = v.str;
                    if (s.StartsWith("_") && s.Contains(":") && TypedKind(s.Substring(1).Split(':')[0]) is int k)
                    {
                        string x = s.Substring(s.IndexOf(':') + 1);
                        var parts = x.Split(',');
                        nums = new double[parts.Length];
                        for (int i = 0; i < parts.Length; i++)
                            nums[i] = k == 5 || k == 10 ? double.Parse(parts[i], System.Globalization.CultureInfo.InvariantCulture)
                                     : k == 11 ? (parts[i] == "true" ? 1 : 0) : long.Parse(parts[i], System.Globalization.CultureInfo.InvariantCulture);
                        kind = k;
                    }
                    else return new uint[] { (uint)St(s), 0, 1 };
                }
                uint p = (uint)blob.Length; var b = new byte[4];
                int n = kind == 10 ? 3 : 1;
                Bin.Require(nums.Length == n, "Typed value component count");
                for (int i = 0; i < n; i++)
                {
                    if (kind == 5 || kind == 10) Bin.PutF32LE(b, 0, (float)nums[i]);
                    else if (kind == 3) Bin.PutU32LE(b, 0, (uint)(int)(long)nums[i]);
                    else Bin.PutU32LE(b, 0, (uint)(long)nums[i]);
                    blob.Write(b, 0, 4);
                }
                return new uint[] { p, 1, (uint)kind };
            }
            void Build(Elem e, int i)
            {
                int a = attrs.Count;
                foreach (var kv in e.attrib) { int nm = St(kv.Key); var v = Val(kv.Value); attrs.Add(new[] { (uint)nm, v[0], v[1], v[2] }); }
                int ac = attrs.Count - a, cs = nodes.Count;
                for (int k = 0; k < e.children.Count; k++) nodes.Add(null);
                int tag = St(e.tag);
                string t = e.text != null && e.text.Trim().Length > 0 ? e.text.Trim() : null;
                var tv = Val(t == null ? null : EVal.S(t));
                nodes[i] = new uint[] { (uint)tag, tv[0], tv[1], tv[2], (uint)cs, (uint)e.children.Count, (uint)a, (uint)ac };
                for (int j = 0; j < e.children.Count; j++) Build(e.children[j], cs + j);
            }
            nodes.Add(null); Build(root, 0);
            var d = new Bxml { strings = strings, blob = blob.ToArray() };
            foreach (var s in strings) d.stringBytes.Add(Encoding.UTF8.GetBytes(s));
            foreach (var x in attrs) d.attrs.Add(new BxAttr { name = x[0], refv = x[1], count = (ushort)x[2], kind = (ushort)x[3] });
            foreach (var x in nodes) d.nodes.Add(new BxNode { name = x[0], refv = x[1], count = x[2], kind = x[3], childStart = x[4], childCount = x[5], attrStart = x[6], attrCount = x[7] });
            var payload = d.Payload(out int pool);
            var z = Bin.ZlibStored(payload);
            var h = Header(strings.Count, pool, d.blob.Length, d.attrs.Count, d.nodes.Count, z.Length);
            var result = new byte[h.Length + z.Length];
            Buffer.BlockCopy(h, 0, result, 0, h.Length); Buffer.BlockCopy(z, 0, result, h.Length, z.Length);
            return result;
        }

        static int? TypedKind(string name)
        {
            switch (name) { case "int": return 3; case "uint": return 4; case "float": return 5; case "bool": return 11; case "vector3": return 10; }
            return null;
        }
    }
}
