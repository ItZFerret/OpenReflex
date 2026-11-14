// MX vs ATV Untamed converter, foundation: scene containers (magic 0xC0FFFEEE; sub-scene directory; back-to-back zlib BXML SceneChunk
// documents). Two ports:
//   UxPcScene   mw2-to-reflex scripts/scene.py: a Reflex PC .dx9.scene as Elem trees (load, save, ns, prop, get_matrix, set_matrix),
//               the form the builder edits and writes.
//   UxXScene    untamed/tools/uxscene.py: an Untamed .xenon.scene (big-endian container words) or a PC scene as read-only entities with
//               Python-shaped property values (value_of), merged_entities.
using System;
using System.Collections.Generic;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// scene.SubScene: name, chunks (SceneChunk Elem trees), nent (header count), versions ((version, unknown) per chunk).
    public sealed class UxSubScene
    {
        public string name; public List<UxElem> chunks; public long nent; public List<(long version, long unknown)> versions;
        public UxSubScene(string name, List<UxElem> chunks, long nent, List<(long, long)> versions) { this.name = name; this.chunks = chunks; this.nent = nent; this.versions = versions; }
        /// entities(): every chunk's children in order.
        public IEnumerable<UxElem> Entities() { foreach (var c in chunks) foreach (var e in c.children) yield return e; }
    }

    /// scene.py.
    public static class UxPcScene
    {
        public const uint MAGIC = 0xC0FFFEEE;

        /// load(path) on the file's bytes.
        public static List<UxSubScene> Load(byte[] d)
        {
            UxBin.Require(d.Length >= 8 && UxBin.U32LE(d, 0) == MAGIC, "not a scene file");
            int n = (int)UxBin.U32LE(d, 4);
            var subs = new List<UxSubScene>(n);
            for (int i = 0; i < n; i++)
            {
                int o = 8 + i * 264;
                string name = Encoding.UTF8.GetString(d, o, NulAt(d, o, 260) - o);
                int off = (int)UxBin.U32LE(d, o + 260);
                int nch = (int)UxBin.U32LE(d, off); long nent = UxBin.U32LE(d, off + 4);
                int p = off + 8; var chunks = new List<UxElem>(nch); var vers = new List<(long, long)>(nch);
                for (int k = 0; k < nch; k++)
                {
                    int size = 36 + (int)UxBin.U32LE(d, p + 32);
                    var b = new UxBxml(UxBin.Slice(d, p, size));
                    chunks.Add(b.ToTree()); vers.Add((b.version, b.unknown));
                    p += size;
                }
                subs.Add(new UxSubScene(name, chunks, nent, vers));
            }
            return subs;
        }
        internal static int NulAt(byte[] d, int o, int max) { int e = o; while (e < o + max && d[e] != 0) e++; return e; }

        /// The raw BXML documents of every chunk: {sub-scene name: [chunk bytes]} (ux_stocklib._prototype_bytes reads them this way).
        public static List<KeyValuePair<string, List<byte[]>>> RawChunks(byte[] d)
        {
            UxBin.Require(d.Length >= 8 && UxBin.U32LE(d, 0) == MAGIC, "not a scene file");
            int n = (int)UxBin.U32LE(d, 4);
            var r = new List<KeyValuePair<string, List<byte[]>>>(n);
            for (int i = 0; i < n; i++)
            {
                int o = 8 + i * 264;
                string name = Encoding.UTF8.GetString(d, o, NulAt(d, o, 260) - o);
                int off = (int)UxBin.U32LE(d, o + 260);
                int nch = (int)UxBin.U32LE(d, off), p = off + 8;
                var list = new List<byte[]>(nch);
                for (int k = 0; k < nch; k++) { int size = 36 + (int)UxBin.U32LE(d, p + 32); list.Add(UxBin.Slice(d, p, size)); p += size; }
                r.Add(new KeyValuePair<string, List<byte[]>>(name, list));
            }
            return r;
        }

        /// save(path, subs): the file's bytes. The entity count of each sub-scene is recounted; chunks are encoded with encode_tree (attr
        /// flags 0; `stored` = stored deflate instead of zlib level 6).
        public static byte[] Save(List<UxSubScene> subs, bool stored = false)
        {
            var head = new UxWriter(8 + 264 * subs.Count); var body = new UxWriter(1 << 16);
            head.U32(MAGIC).U32((uint)subs.Count);
            int start = 8 + 264 * subs.Count;
            foreach (var s in subs)
            {
                byte[] nm = Encoding.UTF8.GetBytes(s.name);
                UxBin.Require(nm.Length < 260, "scene: sub-scene name too long");
                head.Bytes(nm).Zeros(260 - nm.Length).U32((uint)(start + body.Length));
                int nent = 0; foreach (var c in s.chunks) nent += c.children.Count;
                body.U32((uint)s.chunks.Count).U32((uint)nent);
                int count = Math.Min(s.chunks.Count, s.versions.Count);      // zip(chunks, versions)
                for (int i = 0; i < count; i++) body.Bytes(UxBxml.EncodeTree(s.chunks[i], s.versions[i].version, s.versions[i].unknown, stored));
            }
            return UxBin.Concat(head.ToArray(), body.ToArray());
        }

        // ---- RSData helpers
        /// ns(entity): 'PhysicsRSData'.
        public static string Ns(UxElem entity) => entity.children[0].AttrStr("namespace").Replace(UxCommon.NS_PREFIX, "");
        /// prop(entity, name): the property RSData element, or null.
        public static UxElem Prop(UxElem entity, string name)
        {
            foreach (var c in entity.children[0].children) if (c.Attr("name") as string == name) return c;
            return null;
        }
        /// get_matrix(entity): the 16 floats (as Python floats).
        public static double[] GetMatrix(UxElem entity)
        {
            var m = Prop(entity, "Matrix").children[0]; var b = (byte[])m.value; var r = new double[16];
            for (int i = 0; i < 16; i++) r[i] = UxBin.F32LE(b, 4 * i);
            return r;
        }
        /// set_matrix(entity, vals): struct.pack('<16f', *vals).
        public static void SetMatrix(UxElem entity, IList<double> vals)
        {
            UxBin.Require(vals.Count == 16, "matrix needs 16 values");
            var b = new byte[64];
            for (int i = 0; i < 16; i++) UxBin.PutF32(b, 4 * i, (float)vals[i]);
            Prop(entity, "Matrix").children[0].value = b;
        }
        /// The entity's id (attribute entityId, a u32), or -1.
        public static long EntityId(UxElem entity)
        {
            var a = entity.AttrObj("entityId");
            return a != null && a.raw != null && a.raw.Length >= 4 ? UxBin.U32LE(a.raw, 0) : -1;
        }
    }

    /// uxscene's ('raw', type, offset): a pool value of a type without a format.
    public sealed class UxRawRef
    {
        public readonly long type, offset;
        public UxRawRef(long type, long offset) { this.type = type; this.offset = offset; }
    }

    /// uxscene.SubScene: name, chunks (documents), nent, offset.
    public sealed class UxXSubScene
    {
        public string name; public List<UxBxml> chunks; public long nent, offset;
        List<UxEntity> _ents;
        public List<UxEntity> Entities()
        {
            if (_ents == null)
            {
                var l = new List<UxEntity>();
                foreach (var b in chunks) foreach (int ni in b.Children(0)) l.Add(new UxEntity(b, ni));
                _ents = l;
            }
            return _ents;
        }
        /// short(level): 'Norton_STADIUMBase' -> 'Base'.
        public string Short(string level) => !string.IsNullOrEmpty(level) && name.StartsWith(level, StringComparison.Ordinal) ? name.Substring(level.Length) : name;
    }

    /// uxscene.Entity. Property values are Python-shaped: JObj (dict / OrderedDict), List<object> (list), string, long, double, null,
    /// UxRawRef; a multi-component pool value (Matrix, Vector3 ...) is a List<object> of double.
    public sealed class UxEntity
    {
        public readonly UxBxml b; public readonly int ni, rs;
        public readonly object id;                 // entityId (long), or null
        public readonly string ns, type; public readonly object version;
        public readonly JObj props = new JObj(), prop_types = new JObj();

        public UxEntity(UxBxml b, int ni)
        {
            this.b = b; this.ni = ni;
            id = b.Attr(ni, "entityId");
            rs = b.FirstChild(ni);
            UxBin.Require(b.ChildCount(ni) > 0, "scene: an Entity without RSData");
            ns = b.AttrStr(rs, "namespace");
            type = ns.Replace(UxXScene.PFX, "");
            version = b.Attr(rs, "version");
            foreach (int c in b.Children(rs))
            {
                string nm = b.Attr(c, "name") as string;
                if (string.IsNullOrEmpty(nm)) nm = "?";
                props.Set(nm, UxXScene.ValueOf(b, c));
                prop_types.Set(nm, (b.Attr(c, "namespace") as string ?? "").Replace(UxXScene.PFX, ""));
            }
        }
        public long Id => id is long l ? l : -1;
        /// name: props.get('Name').
        public object NameValue => props.Get("Name");
        public string Name => props.Get("Name") as string;
        /// matrix: the 16 floats, or null.
        public double[] Matrix
        {
            get
            {
                if (!(props.Get("Matrix") is List<object> m) || m.Count != 16) return null;
                var r = new double[16]; for (int i = 0; i < 16; i++) r[i] = UxJson.Num(m[i]);
                return r;
            }
        }
        /// position: matrix[12:15], or null.
        public double[] Position { get { var m = Matrix; return m == null ? null : new[] { m[12], m[13], m[14] }; } }

        /// assets(): [(prop, name, type)].
        public List<(string prop, string name, string type)> Assets()
        {
            var o = new List<(string, string, string)>();
            void Rec(string k, object v)
            {
                if (v is JObj d)
                {
                    if (d.Has("asset") && d.Has("type")) o.Add((k, d.Get("asset") as string, d.Get("type") as string));
                    else foreach (var kv in d.items) Rec(k + "." + kv.Key, kv.Value);
                }
                else if (v is List<object> l) foreach (var x in l) Rec(k, x);
            }
            foreach (var kv in props.items) Rec(kv.Key, kv.Value);
            return o;
        }
        /// to_dict(): OrderedDict(id, type, version, props, prop_types).
        public JObj ToDict() => new JObj().Set("id", id).Set("type", type).Set("version", version).Set("props", props).Set("prop_types", prop_types);
    }

    /// uxscene.py.
    public static class UxXScene
    {
        public const uint MAGIC = 0xC0FFFEEE;
        public const string PFX = "RSToolsNS_RSDataNS_";

        /// detect_endian(d): true = big endian ('>').
        public static bool DetectBigEndian(byte[] d)
        {
            if (UxBin.U32BE(d, 0) == MAGIC) return true;
            if (UxBin.U32LE(d, 0) == MAGIC) return false;
            throw new UxException("not a scene container (magic " + UxBin.U32BE(d, 0).ToString("x8") + ")");
        }

        /// load(path) on the file's bytes.
        public static List<UxXSubScene> Load(byte[] d)
        {
            bool big = DetectBigEndian(d);
            int n = (int)UxBin.U32(d, 4, big);
            var subs = new List<UxXSubScene>(n);
            for (int i = 0; i < n; i++)
            {
                int o = 8 + i * 264;
                string name = UxBin.CStr(d, o, 260);
                int off = (int)UxBin.U32(d, o + 260, big);
                int nch = (int)UxBin.U32(d, off, big); long nent = UxBin.U32(d, off + 4, big);
                int p = off + 8; var chunks = new List<UxBxml>(nch);
                for (int k = 0; k < nch; k++)
                {
                    UxBin.Require(UxBin.U32LE(d, p) == UxBxml.Signature, "chunk is not BXML at " + p);
                    int size = 36 + (int)UxBin.U32LE(d, p + 32);
                    chunks.Add(new UxBxml(UxBin.Slice(d, p, size)));
                    p += size;
                }
                subs.Add(new UxXSubScene { name = name, chunks = chunks, nent = nent, offset = off });
            }
            return subs;
        }

        static object Unpack(UxBxml b, long t, long off, long cnt, bool arrays)
        {
            int o = (int)off;
            if (arrays && cnt > 1 && (t == 3 || t == 4 || t == 5 || t == 11))
            {
                var l = new List<object>((int)cnt);
                for (int i = 0; i < cnt; i++)
                    l.Add(t == 3 ? (object)(long)UxBin.I32LE(b.pool, o + 4 * i) : t == 4 ? (long)UxBin.U32LE(b.pool, o + 4 * i) : t == 5 ? (object)(double)UxBin.F32LE(b.pool, o + 4 * i) : (long)b.pool[o + i]);
                return l;
            }
            switch (t)
            {
                case 3: return (long)UxBin.I32LE(b.pool, o);
                case 4: return (long)UxBin.U32LE(b.pool, o);
                case 5: return (double)UxBin.F32LE(b.pool, o);
                case 11: return (long)b.pool[o];
                case 7: case 8: case 10:
                {
                    int n = t == 7 ? 4 : t == 8 ? 16 : 3; var l = new List<object>(n);
                    for (int i = 0; i < n; i++) l.Add((double)UxBin.F32LE(b.pool, o + 4 * i));
                    return l;
                }
            }
            return new UxRawRef(t, off);
        }

        /// _node_val(b, ni).
        public static object NodeVal(UxBxml b, int ni)
        {
            var n = b.nodes[ni];
            if (n[1] < 0) return null;
            long t = n[3];
            if (t == 0 || t == 1) return b.strings[(int)n[1]];
            return Unpack(b, t, n[1], Math.Max(1, n[2]), true);
        }

        /// _attrs(b, ni): attribute name -> value.
        public static JObj Attrs(UxBxml b, int ni)
        {
            var o = new JObj();
            foreach (var kv in b.AttrItems(ni))
            {
                var a = b.attrs[kv.Key];
                o.Set(kv.Value, a[3] == 1 ? b.strings[(int)a[1]] : Unpack(b, a[3], a[1], 1, false));
            }
            return o;
        }

        /// value_of(b, ni): the Python value of a property RSData node (named or list element).
        public static object ValueOf(UxBxml b, int ni)
        {
            var kids = new List<int>(b.Children(ni));
            var rs = kids.FindAll(c => b.Name(c) == "RSData");
            if (rs.Count > 0 && rs.Count == kids.Count)
            {   // list (AssetList etc.) or struct of named props
                var attrs = rs.ConvertAll(c => Attrs(b, c));
                if (attrs.TrueForAll(a => a.Has("name")))
                {
                    var d = new JObj();
                    for (int i = 0; i < rs.Count; i++) d.Set(KeyOf(attrs[i].Get("name")), ValueOf(b, rs[i]));
                    return d;
                }
                var l = new List<object>(rs.Count);
                foreach (int c in rs) l.Add(ValueOf(b, c));
                return l;
            }
            if (kids.Count == 0) return null;
            if (kids.Count == 1)
            {
                int c = kids[0]; string tag = b.Name(c); var a = Attrs(b, c); object v = NodeVal(b, c);
                if (tag == "Asset") return new JObj().Set("asset", a.Get("name")).Set("type", a.Get("type"));
                if (tag == "Enum") { a.Set("value", v); return new JObj().Set("enum", a); }
                if (b.ChildCount(c) > 0)
                {
                    var l = new List<object>();
                    foreach (int g in b.Children(c)) l.Add(ValueOfElem(b, g));
                    return new JObj().Set(tag, l);
                }
                if (a.items.Count > 0) { a.Set("value", v); return new JObj().Set(tag, a); }
                return v;
            }
            var o = new JObj();
            foreach (int c in kids) o.Set(b.Name(c), ValueOfElem(b, c));
            return o;
        }
        static string KeyOf(object k) => k as string ?? Convert.ToString(k, System.Globalization.CultureInfo.InvariantCulture);

        /// value_of_elem(b, ni).
        public static object ValueOfElem(UxBxml b, int ni)
        {
            var a = Attrs(b, ni); object v = NodeVal(b, ni);
            if (b.ChildCount(ni) > 0)
            {
                var l = new List<object>();
                foreach (int c in b.Children(ni)) l.Add(ValueOfElem(b, c));
                var o = new JObj().Set(b.Name(ni), l);
                foreach (var kv in a.items) o.Set(kv.Key, kv.Value);
                return o;
            }
            if (a.items.Count > 0) { a.Set("value", v); return a; }
            return v;
        }

        /// inventory(sub): RSData type -> count, in first-seen order.
        public static JObj Inventory(UxXSubScene sub)
        {
            var o = new JObj();
            foreach (var e in sub.Entities()) o.Set(e.type, (long)(o.Get(e.type) ?? 0L) + 1);
            return o;
        }

        /// merged_entities(subs, level): the union of all sub-scenes by entityId, first occurrence kept: [(id, sub-scene short name, entity)]
        /// in first-seen order (the OrderedDict's).
        public static List<(object id, string sub, UxEntity entity)> MergedEntities(List<UxXSubScene> subs, string level = "")
        {
            var o = new List<(object, string, UxEntity)>(); var seen = new HashSet<long>(); bool seenNull = false;
            foreach (var s in subs)
                foreach (var e in s.Entities())
                {
                    if (e.id is long l) { if (!seen.Add(l)) continue; }
                    else { if (seenNull) continue; seenNull = true; }
                    o.Add((e.id, s.Short(level), e));
                }
            return o;
        }

        /// _jsonable(o): floats rounded to 6 decimals (Python round), containers copied.
        public static object Jsonable(object o)
        {
            switch (o)
            {
                case double d: return UxNp.PyRound(d, 6);
                case JObj j: { var r = new JObj(); foreach (var kv in j.items) r.Set(kv.Key, Jsonable(kv.Value)); return r; }
                case List<object> l: { var r = new List<object>(l.Count); foreach (var x in l) r.Add(Jsonable(x)); return r; }
                case UxRawRef raw: return new List<object> { "raw", raw.type, raw.offset };      // the ('raw', type, offset) tuple
                default: return o;
            }
        }
    }
}
