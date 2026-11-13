// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_build_assets.py (the builder's helper: which
// converted assets go into <L>Base, where their bytes come from, name clashes with the base package, the group filter for dropped
// materials), plus the small types the other assembly parts share (UxOrdered, UxAssetMap, UxPaths).
//   UxBuildAssets.EntityRefs / RenameRefs      entity_refs / rename_refs
//   UxTemplateAssets                            TemplateAssets(ts): the base package's (name, type) keys, case-insensitive clash, bytes
//   UxBuildAssets.GlobalAssets                  global_assets(): the always-loaded game databases (AudioDependencies, EngineDependencies)
//   UxSources                                   Sources(lv, dirs): the index of the stage outputs of a work store
//   UxBuildAssets.FilterDropped                 filter_dropped(items, dropped, read_part, log, fallback_material)
// Nothing here writes files. Python's Path values are work-store relative paths here ('meshes/X.surface'); the absolute path TEXT that
// Python records (manifests[..].path, the source of an added asset) is made by UxPaths: with UxPaths.root = the reference's U.ROOT
// (the reference's own folder) the strings are Python's, with the default "" they are relative to that root. Pure C# (System only).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// An insertion-ordered dictionary (Python's OrderedDict / dict): a set of an existing key keeps its place.
    public sealed class UxOrdered<TK, TV> : IEnumerable<KeyValuePair<TK, TV>>
    {
        readonly List<TK> _keys = new List<TK>();
        readonly Dictionary<TK, TV> _map;
        public UxOrdered(IEqualityComparer<TK> comparer = null) { _map = new Dictionary<TK, TV>(comparer); }
        public int Count => _keys.Count;
        public IReadOnlyList<TK> Keys => _keys;
        public bool Has(TK key) => _map.ContainsKey(key);
        public bool TryGet(TK key, out TV value) => _map.TryGetValue(key, out value);
        public TV Get(TK key, TV missing = default) => _map.TryGetValue(key, out var v) ? v : missing;
        public TV this[TK key]
        {
            get => _map.TryGetValue(key, out var v) ? v : throw new UxException("KeyError: " + key);
            set { if (!_map.ContainsKey(key)) _keys.Add(key); _map[key] = value; }
        }
        public bool Remove(TK key) { if (!_map.Remove(key)) return false; _keys.Remove(key); return true; }
        public IEnumerator<KeyValuePair<TK, TV>> GetEnumerator() { foreach (var k in _keys) yield return new KeyValuePair<TK, TV>(k, _map[k]); }
        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }

    /// One asset a builder module adds: (bytes, source, origin) of the Python tuples (+ the tag ux_build_track._extra gives it).
    public sealed class UxAsset
    {
        public byte[] data; public string source, origin, tag;
        public UxAsset(byte[] data, string source, string origin, string tag = null) { this.data = data; this.source = source; this.origin = origin; this.tag = tag; }
    }

    /// {(name, type): (bytes, source, origin)} in insertion order: the order the assets enter the package.
    public sealed class UxAssetMap : IEnumerable<KeyValuePair<(string name, string type), UxAsset>>
    {
        readonly UxOrdered<(string name, string type), UxAsset> _m = new UxOrdered<(string, string), UxAsset>();
        public int Count => _m.Count;
        public IReadOnlyList<(string name, string type)> Keys => _m.Keys;
        public bool Has(string name, string type) => _m.Has((name, type));
        public UxAsset Get(string name, string type) => _m.Get((name, type));
        public UxAsset this[(string name, string type) key] { get => _m[key]; set => _m[key] = value; }
        public void Set(string name, string type, byte[] data, string source, string origin) => _m[(name, type)] = new UxAsset(data, source, origin);
        public IEnumerator<KeyValuePair<(string name, string type), UxAsset>> GetEnumerator() => _m.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
        /// The rows [name, type, md5, bytes, source, origin] (records and tests).
        public List<object> Rows()
        {
            var o = new List<object>();
            foreach (var kv in _m) o.Add(new List<object> { kv.Key.name, kv.Key.type, UxBin.Md5(kv.Value.data), (long)kv.Value.data.Length, kv.Value.source, kv.Value.origin });
            return o;
        }
    }

    /// The path TEXT of the reference's folders (ux_common.ROOT, GAME ...), which Python writes into records as Path.as_posix().
    /// No file is read through these: the bytes come from the disc, the library and the work store.
    public sealed class UxPaths
    {
        /// U.ROOT (the reference's own folder); "" = paths relative to it ('templates/stock-lib/assets/X').
        public string root = "";
        /// U.GAME (the Reflex install folder); "" = relative ('AnimationSets.bxml', 'Database/X').
        public string game = "";
        static string Join(string a, string b) => string.IsNullOrEmpty(a) ? b : a.TrimEnd('/') + "/" + b;
        public string Root(string rel) => Join(root, rel);
        public string Game(string rel) => Join(game, rel);
        public string Templates(string rel) => Root("templates/" + rel);
        /// U.extract_dir(lv) / rel.
        public string Extract(UxLevelRow lv, string rel = null) => Root("extract/" + lv.disc + (rel == null ? "" : "/" + rel));
        public string Extract(string level, string rel) => Root("extract/" + level + "/" + rel);
        /// U.work_dir(lv) / rel (rel: a work-store path).
        public string Work(UxLevelRow lv, string rel = null) => Root("work/" + lv.disc + (rel == null ? "" : "/" + rel));
        /// ux_stocklib.ASSETS / '<name>.<type>'.
        public string StockLib(string name, string type) => Templates("stock-lib/assets/" + name + "." + type);
    }

    /// ux_build_assets.TemplateAssets(ts): the base (template) package.
    public sealed class UxTemplateAssets
    {
        public readonly UxTrackSet ts;
        public readonly HashSet<(string name, string type)> keys = new HashSet<(string, string)>();
        /// (lower name, type) -> the FIRST asset of that spelling-insensitive name (setdefault).
        public readonly Dictionary<(string lower, string type), UxPcAsset> lower = new Dictionary<(string, string), UxPcAsset>();
        readonly Dictionary<(string, string), byte[]> _cache = new Dictionary<(string, string), byte[]>();
        public UxTemplateAssets(UxTrackSet ts)
        {
            this.ts = ts;
            foreach (var a in ts.assets)
            {
                keys.Add((a.name, a.type));
                var lk = (UxCommon.PyLower(a.name), a.type);
                if (!lower.ContainsKey(lk)) lower[lk] = a;
            }
        }
        public bool Has(string name, string typ) => keys.Contains((name, typ));
        public bool Has((string name, string type) key) => keys.Contains(key);
        /// clash(name, typ): the base asset with the same (name, type) ignoring case, or null.
        public UxPcAsset Clash(string name, string typ) => lower.TryGetValue((UxCommon.PyLower(name), typ), out var a) ? a : null;
        /// logical(a): the asset's logical bytes (cached).
        public byte[] Logical(UxPcAsset a)
        {
            lock (_cache)
            {
                if (!_cache.TryGetValue((a.name, a.type), out var b)) _cache[(a.name, a.type)] = b = ts.ReadLogical(a);
                return b;
            }
        }
    }

    /// One indexed stage output (Sources.assets[(name, type)]): file = the work-store path, md5 = the manifest's or null.
    public sealed class UxSourceAsset
    {
        public string name, type, file, md5, origin;
        /// materials: the materials.json entry. shaders: in_template (has_in_template: the key is present).
        public JObj entry; public object in_template; public bool has_in_template;
    }

    /// ux_build_assets.Sources(lv, dirs): the converted assets a build may pack. dirs: 'meshes', 'terrain', 'collision', 'materials',
    /// 'textures' -> the stage folder in the work store ('meshes', 'terrain', 'collision-t9', 'materials', 'textures').
    public sealed class UxSources
    {
        public readonly UxLevelRow lv; public readonly UxWork work; public readonly UxPaths paths;
        public readonly Dictionary<string, string> dirs = new Dictionary<string, string>(StringComparer.Ordinal);
        public readonly UxOrdered<(string name, string type), UxSourceAsset> assets = new UxOrdered<(string, string), UxSourceAsset>();
        public readonly List<object> problems = new List<object>(), warnings = new List<object>();
        /// manifests[key] = {path, md5}.
        public readonly JObj manifests = new JObj();
        public Dictionary<string, JObj> mesh_parts;
        public JObj terrain, collision, materials, textures;
        public UxOrdered<string, JObj> material_entries;
        public HashSet<string> dropped_materials;
        public UxOrdered<string, string> texture_renames;
        public HashSet<string> template_textures;

        public UxSources(UxLevelRow lv, UxWork work, IDictionary<string, string> dirs, UxPaths paths = null)
        {
            this.lv = lv; this.work = work; this.paths = paths ?? new UxPaths();
            foreach (var kv in dirs) this.dirs[kv.Key] = kv.Value.Trim('/');
            Meshes(); Terrain(); Collision(); Materials(); Textures();
        }

        /// The Python text of a work-store file's Path (as_posix).
        public string PyPath(string rel) => paths.Work(lv, rel);
        static string Join(string dir, string file) => dir.Length == 0 ? file : dir + "/" + file.Replace('\\', '/');

        void Add(string name, string typ, string file, string md5, string origin, JObj entry = null, object inTemplate = null, bool hasInTemplate = false)
        {
            var key = (name, typ);
            if (assets.TryGet(key, out var old))
            {
                if (old.file != file) problems.Add(name + "." + typ + " offered twice: " + PyPath(old.file) + " and " + PyPath(file));
                return;
            }
            assets[key] = new UxSourceAsset { name = name, type = typ, file = file, md5 = md5, origin = origin, entry = entry, in_template = inTemplate, has_in_template = hasInTemplate };
        }
        JObj Manifest(string key, string rel)
        {
            if (!work.Exists(rel)) { problems.Add(key + " manifest missing: " + PyPath(rel)); return null; }
            byte[] b = work.ReadBytes(rel);
            manifests.Set(key, new JObj().Set("path", PyPath(rel)).Set("md5", UxBin.Md5(b)));
            return UxJson.Obj(UxJson.ParseUtf8(b));
        }
        static List<KeyValuePair<string, object>> Sorted(JObj o)
        {
            var l = new List<KeyValuePair<string, object>>(o.items);
            UxNp.StableSort(l, (a, b) => string.CompareOrdinal(a.Key, b.Key));
            return l;
        }
        static string S(object v) => v as string;
        static bool Truthy(object v) => v is string s ? s.Length > 0 : v is bool b ? b : v is long l ? l != 0 : v is double d ? d != 0 : v is JObj j ? j.items.Count > 0 : v is List<object> a ? a.Count > 0 : v != null;

        void Meshes()
        {
            string d = dirs["meshes"];
            var mj = Manifest("meshes", Join(d, "meshes.json"));
            if (mj == null) return;
            mesh_parts = new Dictionary<string, JObj>(StringComparer.Ordinal);
            foreach (var kv in Sorted(mj))
            {
                if (!(kv.Value is List<object> parts)) continue;
                foreach (object po in parts)
                {
                    var p = UxJson.Obj(po);
                    foreach (string typ in new[] { "surface", "model" })
                        Add(S(p.Get(typ)), typ, Join(d, S(UxJson.Get(p, "files", typ))), S(UxJson.Get(p, "md5", typ)), "meshes:" + kv.Key);
                    mesh_parts[S(p.Get("part"))] = p;
                }
            }
        }
        void Terrain()
        {
            string d = dirs["terrain"];
            var tj = Manifest("terrain", Join(d, "terrain.json"));
            if (tj == null) return;
            terrain = tj;
            if (!(tj.Get("parts") is List<object> parts)) return;
            foreach (object po in parts)
            {
                var p = UxJson.Obj(po);
                var files = p.Get("files") as JObj;
                foreach (string typ in new[] { "surface", "model", "collis" })
                {
                    var f = files?.Get(typ) as JObj;
                    if (f == null || f.items.Count == 0) { problems.Add("terrain part " + PyStr(p.Get("name")) + " has no " + typ + " file"); continue; }
                    Add(S(p.Get(typ)), typ, Join(d, S(f.Get("file"))), S(f.Get("md5")), "terrain:" + PyStr(p.Get("name")));
                }
            }
        }
        void Collision()
        {
            string d = dirs["collision"];
            var cj = Manifest("collision", Join(d, "collision.json"));
            if (cj == null) return;
            collision = cj;
            if (!(cj.Get("sources") is JObj sources)) return;
            foreach (var kv in Sorted(sources))
            {
                var r = UxJson.Obj(kv.Value);
                if (Truthy(r.Get("file"))) Add(S(r.Get("asset")), "collis", Join(d, S(r.Get("file"))), S(r.Get("md5")), "collision:" + kv.Key);
            }
        }
        void Materials()
        {
            string d = dirs["materials"];
            var mj = Manifest("materials", Join(d, "materials.json"));
            if (mj == null) return;
            materials = mj;
            var meta = new HashSet<string>(StringComparer.Ordinal);
            if (mj.Get("meta_keys") is List<object> mk) foreach (object k in mk) if (k is string ks) meta.Add(ks);
            material_entries = new UxOrdered<string, JObj>(StringComparer.Ordinal);
            dropped_materials = new HashSet<string>(StringComparer.Ordinal);
            foreach (var kv in Sorted(mj))
            {
                if (meta.Contains(kv.Key) || !(kv.Value is JObj v) || !v.Has("action")) continue;
                material_entries[kv.Key] = v;
                string action = PyStr(v.Get("action"));
                if (action == "convert") Add(S(v.Get("asset")), "material", Join(d, S(v.Get("file"))), S(v.Get("md5")), "materials:" + kv.Key, entry: v);
                else if (action.StartsWith("drop", StringComparison.Ordinal))
                {
                    dropped_materials.Add(kv.Key);
                    if (Truthy(v.Get("asset"))) dropped_materials.Add(S(v.Get("asset")));
                }
            }
            if (mj.Get("shader_files") is JObj sf)
                foreach (var kv in Sorted(sf))
                {
                    var r = UxJson.Obj(kv.Value);
                    Add(kv.Key, "shader", Join(d, S(r.Get("file"))), S(r.Get("md5")), "materials:shader", inTemplate: r.Get("in_template"), hasInTemplate: true);
                }
            if (mj.Get("library_textures") is JObj lt)
                foreach (var kv in Sorted(lt))
                {
                    var r = UxJson.Obj(kv.Value);
                    Add(kv.Key, "texture", Join(d, S(r.Get("file"))), S(r.Get("md5")), "materials:library (" + PyStr(r.Get("source")) + ")");
                }
            if (mj.Get("generated_textures") is JObj gt)
                foreach (var kv in Sorted(gt))
                {
                    var r = UxJson.Obj(kv.Value);
                    string f = Truthy(r.Get("path")) ? S(r.Get("path")) : S(r.Get("file"));
                    if (string.IsNullOrEmpty(f)) { problems.Add("generated texture " + kv.Key + " has no path"); continue; }
                    // Python: U.ROOT / f when that exists (f = 'work/<Level>/materials/textures/X.texture'), else <terrain dir> / f
                    string pre = "work/" + lv.disc + "/", file;
                    f = f.Replace('\\', '/');
                    if (f.StartsWith(pre, StringComparison.Ordinal) && work.Exists(f.Substring(pre.Length))) file = f.Substring(pre.Length);
                    else file = Join(dirs["terrain"], f);
                    Add(kv.Key, "texture", file, S(r.Get("md5")), "materials:generated (" + (r.Has("provided_by") ? PyStr(r.Get("provided_by")) : "WP6") + ")");
                }
            template_textures = new HashSet<string>(StringComparer.Ordinal);
            if (mj.Get("template_textures") is List<object> tt) foreach (object t in tt) if (t is string ts) template_textures.Add(ts);
        }
        void Textures()
        {
            string d = dirs["textures"];
            var tj = Manifest("textures", Join(d, "textures.json"));
            if (tj == null) return;
            textures = tj;
            texture_renames = new UxOrdered<string, string>(StringComparer.Ordinal);
            if (!(tj.Get("textures") is JObj tex)) return;
            foreach (var kv in Sorted(tex))
            {
                var r = UxJson.Obj(kv.Value);
                if (Truthy(r.Get("asset")) && S(r.Get("asset")) != kv.Key && Truthy(r.Get("file"))) texture_renames[kv.Key] = S(r.Get("asset"));
                if (Truthy(r.Get("file"))) Add(S(r.Get("asset")), "texture", Join(d, S(r.Get("file"))), S(r.Get("md5")), "textures:" + kv.Key);
            }
        }
        static string PyStr(object v)
        {
            switch (v)
            {
                case null: return "None";
                case string s: return s;
                case bool b: return b ? "True" : "False";
                case long l: return l.ToString(CultureInfo.InvariantCulture);
                case double x: return PyFormat.Repr(x);
            }
            return Convert.ToString(v, CultureInfo.InvariantCulture);
        }

        public bool Has(string name, string type) => assets.Has((name, type));
        /// read(key, check_md5): the bytes of an indexed asset; a file that differs from its manifest md5 throws (stale stage output).
        public byte[] Read((string name, string type) key, bool checkMd5 = true)
        {
            var rec = assets[key];
            byte[] data = work.ReadBytes(rec.file);
            if (checkMd5 && !string.IsNullOrEmpty(rec.md5))
            {
                string got = UxBin.Md5(data);
                if (got != rec.md5) throw new UxException(key.name + "." + key.type + ": " + PyPath(rec.file) + " md5 " + got + " != manifest " + rec.md5 + " (stale stage output?)");
            }
            return data;
        }
        public byte[] Read(string name, string type, bool checkMd5 = true) => Read((name, type), checkMd5);
    }

    /// ux_build_assets.py: the module functions.
    public static class UxBuildAssets
    {
        public static readonly string[] TYPE_ORDER = { "texture", "shader", "material", "surface", "model", "collis", "tdf" };
        /// TYPE_ORDER.index(type), 99 for another type (the builder's asset sort key).
        public static int TypeOrder(string type) { int i = Array.IndexOf(TYPE_ORDER, type); return i < 0 ? 99 : i; }

        // ================================================================================================ elements
        /// entity_refs(e): [(asset name, type)] of every Asset element under an Entity Elem.
        public static List<(string name, string type)> EntityRefs(UxElem e)
        {
            var o = new List<(string, string)>();
            foreach (var x in e.Iter())
            {
                if (x.tag != "Asset") continue;
                string n = x.Attr("name") as string, t = x.Attr("type") as string;
                if (n != null && t != null) o.Add((n, t));
            }
            return o;
        }
        /// rename_refs(e, ren): rewrites Asset name attrs whose (name, type) is a key of ren; returns the number of changes.
        public static int RenameRefs(UxElem e, IDictionary<(string name, string type), string> ren)
        {
            int k = 0;
            foreach (var x in e.Iter())
            {
                if (x.tag != "Asset") continue;
                string n = x.Attr("name") as string, t = x.Attr("type") as string;
                if (n == null || t == null || !ren.TryGetValue((n, t), out string nn)) continue;
                foreach (var a in x.attrs) if (a.name == "name") { a.str = nn; k++; }
            }
            return k;
        }

        // ================================================================================================ globals
        /// global_assets(): {(name, type): database name} of the always-loaded game databases (AudioDependencies, EngineDependencies; a
        /// missing or unreadable database is left out). Read once per library by the caller.
        public static Dictionary<(string name, string type), string> GlobalAssets(IUxLibrary library)
        {
            var o = new Dictionary<(string, string), string>();
            foreach (string n in new[] { "AudioDependencies", "EngineDependencies" })
            {
                if (library == null || !library.HasLevel(n)) continue;
                UxBxml b;
                try { b = new UxBxml(library.ReadLevelFile(n, "database")); } catch (Exception) { continue; }
                for (int i = 0; i < b.nodes.Count; i++)
                {
                    if (b.Name(i) != "Asset") continue;
                    var key = (b.Attr(i, "name") as string, b.Attr(i, "type") as string);
                    if (!o.ContainsKey(key)) o[key] = n;
                }
            }
            return o;
        }

        // ================================================================================================ dropped materials
        /// The result of filter_dropped.
        public sealed class Filtered
        {
            public List<KeyValuePair<string, UxElem>> items = new List<KeyValuePair<string, UxElem>>();
            /// {(variant name, 'model' | 'surface'): bytes} in the order made.
            public UxOrdered<(string name, string type), byte[]> new_assets = new UxOrdered<(string, string), byte[]>();
            public JObj report;
        }

        static string Asset(object m) => (m as JObj)?.Get("asset") as string;

        /// filter_dropped(items, dropped, read_part, log, fallback_material): removes the groups that use dropped materials from the
        /// placed converted entities (changed in place). items = [(subscene, Entity Elem)]; readPart(name) -> (model bytes, surface
        /// bytes). See the Python doc: part variants '<part>_UXd<k>', render-only entities without a kept group are removed, collision
        /// entities keep their first group drawn with fallbackMaterial.
        public static Filtered FilterDropped(IList<KeyValuePair<string, UxElem>> items, ICollection<string> dropped, Func<string, (byte[] model, byte[] surface)> readPart,
            Action<string> log = null, string fallbackMaterial = null)
        {
            var variants = new Dictionary<string, string>(StringComparer.Ordinal);       // 'part\n0,1,2' -> variant name
            var perPart = new Dictionary<string, int>(StringComparer.Ordinal);
            var res = new Filtered();
            long changed = 0, groupsRemoved = 0;
            var removed = new List<object>(); var fallback = new List<object>(); var repVariants = new JObj();
            foreach (var it in items)
            {
                var e = it.Value;
                var P = UxEntities.PropsOf(e);
                var ml = P.Has("MaterialList") ? UxEntities.GetValue(P["MaterialList"]) as List<object> : null;
                bool any = false;
                if (ml != null) foreach (object m in ml) if (dropped.Contains(Asset(m))) { any = true; break; }
                if (ml == null || ml.Count == 0 || !any) { res.items.Add(it); continue; }
                string model = Asset(UxEntities.GetValue(P["ModelAsset"])), surface = Asset(UxEntities.GetValue(P["SurfaceAsset"]));
                string name = UxEntities.GetValue(P["Name"]) as string;
                if (model != surface) throw new UxException(name + ": model " + model + " != surface " + surface + " (the filter assumes WP3 part naming)");
                var keep = new List<int>();
                for (int i = 0; i < ml.Count; i++) if (!dropped.Contains(Asset(ml[i]))) keep.Add(i);
                bool hasColl = P.Has("FemaleCollisionAsset") && !string.IsNullOrEmpty(Asset(UxEntities.GetValue(P["FemaleCollisionAsset"])));
                if (keep.Count == 0)
                {
                    if (!hasColl) { removed.Add(name + " (" + model + "): every group uses a dropped material"); continue; }
                    if (fallbackMaterial == null) throw new UxException(name + ": every group uses a dropped material and no fallback material is available");
                    keep.Add(0);
                    var first = new JObj(); foreach (var kv in ((JObj)ml[0]).items) first.Set(kv.Key, kv.Value);
                    first.Set("asset", fallbackMaterial);
                    var ml2 = new List<object> { first }; for (int i = 1; i < ml.Count; i++) ml2.Add(ml[i]);
                    ml = ml2;
                    fallback.Add(name + " (" + model + "): collision entity drawn with " + fallbackMaterial);
                }
                string key = model + "\n" + string.Join(",", keep);
                if (!variants.TryGetValue(key, out string vn))
                {
                    perPart[model] = (perPart.TryGetValue(model, out int c) ? c : 0) + 1;
                    vn = model + "_UXd" + perPart[model].ToString(CultureInfo.InvariantCulture);
                    var (mb, sb) = readPart(model);
                    var m = UxRmesh.ReadModel(mb);
                    var groups = UxMeshes.ReadSurfacePc(sb);
                    if (groups.Count != m.materials.Count || groups.Count != ml.Count)
                        throw new UxException(model + ": model " + m.materials.Count + " groups, surface " + groups.Count + ", MaterialList " + ml.Count);
                    var mats = new List<string>(); foreach (int i in keep) mats.Add(m.materials[i]);
                    var nm = new UxPcModel(vn, mats, m.radius, m.center, m.extents, new List<uint[]> { new uint[] { (uint)keep.Count, 0, 0 } }, m.draw_distance, m.matrix);
                    nm.center4 = m.center4; nm.extents4 = m.extents4;
                    res.new_assets[(vn, "model")] = UxRmesh.WriteModel(nm);
                    var kg = new List<UxMeshes.PcGroup>(); foreach (int i in keep) kg.Add(groups[i]);
                    res.new_assets[(vn, "surface")] = UxMeshes.WriteSurfacePc(kg);
                    variants[key] = vn;
                    var kl = new List<object>(); foreach (int i in keep) kl.Add((long)i);
                    repVariants.Set(vn, new JObj().Set("part", model).Set("kept_groups", kl).Set("removed_groups", (long)(groups.Count - keep.Count)));
                }
                UxEntities.SetValue(P["ModelAsset"], new JObj().Set("asset", vn).Set("type", "model"));
                UxEntities.SetValue(P["SurfaceAsset"], new JObj().Set("asset", vn).Set("type", "surface"));
                var nl = new List<object>(); foreach (int i in keep) nl.Add(ml[i]);
                UxEntities.SetValue(P["MaterialList"], nl);
                changed++;
                groupsRemoved += ml.Count - keep.Count;
                res.items.Add(it);
            }
            res.report = new JObj().Set("entities_changed", changed).Set("entities_removed", removed).Set("fallback", fallback).Set("variants", repVariants)
                .Set("groups_removed", groupsRemoved);
            if ((changed > 0 || removed.Count > 0) && log != null)
                log("dropped-material filter: " + changed + " entities re-pointed to " + variants.Count + " group-filtered part variants, " + removed.Count +
                    " removed, " + fallback.Count + " fallback");
            return res;
        }
    }
}
