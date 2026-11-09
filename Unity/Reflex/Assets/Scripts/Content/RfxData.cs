// Readers for MX vs ATV Reflex's PC data (Rainbow Studios' DX9 engine, 2009): the player's own install supplies everything,
// nothing from the game enters the Unity project or a player package. The research and the format notes are in the
// Reflex RE project (_re/formats/BIKE_FORMAT.md, RIDER_FORMAT.md; _documentation/reflex.md here).
//  - Database/<name>.dx9.database: BXML (the Untamed reader: 0x103ea, zlib) listing packages (name, file, offset) and their
//    assets (name, type, Heap offset / size, Compress LZX); the bytes are in <file>.dx9.package (XMem LZX chunks, the
//    Untamed reader again).
//  - tbldata: BXML tables (MXTables_Default + the DLC tables): <Table><Name/><Columns/><Rows><Row col="value"/>...
//  - localiz: u32 2, key count, key pool size, the keys (NUL-terminated), u32 lengths, u32 total, UTF-16 strings.
//  - model (0x88aabb00 v4) / surface (0x99bbcc00 v4): little-endian, row-major D3D matrices (row vectors, world = local *
//    parentWorld), DX9 vertex declarations (FLOAT16 positions and UVs, FLOAT3 normals, UBYTE4 blend indices through a
//    per-surface bone palette, UBYTE4N weights), 16-bit triangle strips. Units: feet, Y up, x the rider's right, z forward
//    (the Unleashed game frame).
//  - material: BXML 0x3ea (MaterialEditorMaterial: the pixel shader, the textures in sampler order, the parameters, the
//    render states as D3DRENDERSTATETYPE pairs).
//  - shader: entries of SM3 bytecode; only the pixel shader's constant table is read (the sampler names say what each
//    texture is: diffuse, normal map, fork sticker, number decal ...).
//  - texture (0x77ddcc00 v4): DXT1 / DXT5 / A8R8G8B8 / G16R16F mip chains.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.Content
{
    internal static class Rfx
    {
        internal static void Check(bool ok, string detail) { if (!ok) throw new InvalidDataException("Unsupported or damaged Reflex data: " + detail); }
        internal static int I32(byte[] b, int p) { Check(p >= 0 && p <= b.Length - 4, "file range"); return BitConverter.ToInt32(b, p); }
        internal static uint U32(byte[] b, int p) { Check(p >= 0 && p <= b.Length - 4, "file range"); return BitConverter.ToUInt32(b, p); }
        internal static float F32(byte[] b, int p) { Check(p >= 0 && p <= b.Length - 4, "file range"); return BitConverter.ToSingle(b, p); }
        internal static float F16(byte[] b, int p) { Check(p >= 0 && p <= b.Length - 2, "file range"); return HalfToFloat(BitConverter.ToUInt16(b, p)); }
        /// IEEE binary16 -> binary32, exact for every value (as Mathf.HalfToFloat; managed, so the readers also run outside Unity: tests/alive_bikes).
        internal static float HalfToFloat(ushort h)
        {
            uint s = (uint)(h & 0x8000) << 16, m = (uint)(h & 0x3ff); int e = (h >> 10) & 31; uint bits;
            if (e == 31) bits = s | 0x7f800000u | m << 13;
            else if (e != 0) bits = s | (uint)(e + 112) << 23 | m << 13;
            else if (m == 0) bits = s;
            else { e = 1; while ((m & 0x400) == 0) { m <<= 1; e--; } bits = s | (uint)(e + 112) << 23 | (m & 0x3ff) << 13; }
            return BitConverter.Int32BitsToSingle((int)bits);
        }
        static string CString(byte[] b, int p, int end)
        {
            Check(p >= 0 && p <= end && end <= b.Length, "string range");
            int e = Array.IndexOf(b, (byte)0, p, end - p); if (e < 0) e = end;
            return Encoding.ASCII.GetString(b, p, e - p);
        }

        /// The Untamed readers' messages name Untamed: say Reflex.
        static T Wrap<T>(Func<T> f)
        {
            try { return f(); }
            catch (InvalidDataException e) { throw new InvalidDataException(e.Message.Replace("Untamed", "Reflex"), e); }
        }
        internal static Node Bxml(byte[] b) => Wrap(() => BxmlData.Xml(b));

        /// A resource's LZX chunks (the PC build's XMem layout, _re/formats/package_extract.py): u32 region end (= size - 4),
        /// then chunks of (u32 allocation 65536, u32 compressed size, XMem frames: a 2-byte big-endian frame size for a
        /// 32 KiB frame, or 0xFF + 2-byte output size + 2-byte frame size; a 5-byte zero terminator), the last chunk's logical
        /// length in the final u32. One LZX stream per chunk (window 17).
        internal static byte[] Lzx(byte[] data, CancellationToken cancel)
        {
            int limit = I32(data, 0); Check(limit <= data.Length && limit >= 4, "LZX container");   // < : MXUI FrontEndIntro.ENG PublisherVideo.lyr's heap entry has 156 bytes past its container
            // the final u32: all four bytes in the game's files; some community track tools (Mods/Tracks "2014 Monster Energy Cup") give the
            // resource a heap size that cuts its zero high byte off: the missing bytes read as zeros; none at all: the last chunk is what its
            // frames hold (core/src/fmt_lzx.c the same)
            int tail = Math.Min(4, data.Length - limit), last = 0;
            for (int i = 0; i < tail; i++) last |= data[limit + i] << (8 * i);
            bool noLast = tail == 0;
            using var output = new MemoryStream();
            int p = 4;
            while (p < limit)
            {
                cancel.ThrowIfCancellationRequested();
                int chunkStart = p;
                int capacity = I32(data, p), packed = I32(data, p + 4); p += 8;
                Check(capacity > 0 && capacity <= 65536 && packed > 0 && p + packed <= limit, "LZX chunk");
                int end = p + packed, size = end == limit && last > 0 ? last : capacity;
                // the community's track tools (Mods/Tracks) write the resource's whole size in the final u32 (the game's files: the last
                // chunk's): a size no chunk can hold is the total, the last chunk holds the rest (core/src/fmt_lzx.c the same)
                if (end == limit && last > capacity && last > output.Length && last - output.Length <= capacity) size = (int)(last - output.Length);
                Check(output.Length + size <= 64L << 20, "LZX output");
                if (packed == capacity) { output.Write(data, p, Math.Min(size, packed)); p = end; continue; }   // a stored chunk
                var decoder = new LzxDecoder(17); long before = output.Length;
                while (p < end - 5 || p < end && !Zeros(data, p, end - p))
                {
                    int frameSize = 32768, frameBytes;
                    if (data[p] == 255) { frameSize = data[p + 1] << 8 | data[p + 2]; frameBytes = data[p + 3] << 8 | data[p + 4]; p += 5; }
                    else { frameBytes = data[p] << 8 | data[p + 1]; p += 2; }
                    frameSize = (int)Math.Min(frameSize, size - (output.Length - before));
                    Check(frameSize > 0 && frameSize <= 32768 && frameBytes > 0 && p + frameBytes <= end, "LZX frame");
                    using (var input = new MemoryStream(data, p, frameBytes, false))
                        Check(decoder.Decompress(input, frameBytes, output, frameSize) == 0, "LZX decoding");
                    p += frameBytes;
                }
                if (noLast && end == limit && output.Length - before < size) size = (int)(output.Length - before);
                Check(output.Length - before == size, "LZX length: chunkOffset=" + chunkStart + ", expected=" + size + ", decoded=" + (output.Length - before) + ", capacity=" + capacity + ", packed=" + packed + ", finalLength=" + last + ", containerEnd=" + limit);
                p = end;
            }
            return output.ToArray();
        }
        static bool Zeros(byte[] b, int p, int n) { for (int i = 0; i < n; i++) if (b[p + i] != 0) return false; return true; }

        // ------------------------------------------------------------------------------------------- databases

        internal sealed class Asset
        {
            public string name, type; public long offset; public int size; public bool lzx;
            public Package package;
        }
        internal sealed class Package
        {
            public string name, file; public Database database;
            public readonly List<Asset> assets = new List<Asset>();
            readonly Dictionary<string, Asset> _by = new Dictionary<string, Asset>(StringComparer.OrdinalIgnoreCase);
            internal void Add(Asset a) { assets.Add(a); _by[a.type + "|" + a.name] = a; }
            public Asset Find(string asset, string type) => _by.TryGetValue(type + "|" + asset, out var a) ? a : null;
            public IEnumerable<Asset> OfType(string type) { foreach (var a in assets) if (a.type == type) yield return a; }
        }
        internal sealed class Database
        {
            public string name, dir; public int skippedResources;
            /// The database's own name (its files' name): name without a mod namespace ("mod:<id>>MXs" -> "MXs").
            public string baseName;
            /// A bike mod's database (BikeMods: read straight from the mod folder's package files, never from an extracted install).
            public bool mod;
            /// A namespaced database over the import's extracted copy (KitPacks: the built-in GEICO kits): its assets are read where the import
            /// extracted them, whichever install reads them (the game's own install reads a pack's assets through ModRouter).
            public bool extracted;
            public readonly Dictionary<string, Package> packages = new Dictionary<string, Package>(StringComparer.OrdinalIgnoreCase);
            public Package Find(string package) => package != null && packages.TryGetValue(package, out var p) ? p : null;
        }

        /// A number attribute of a database / table BXML, however the file's writer stored it (w-gearmods): the game's own files hold
        /// scalars (UInt32 offsets and sizes, Boolean flags), but community tools write some of them as an empty value (count 0, which the
        /// BXML reader gives as an empty float[]), a float array, a raw byte blob or text. Convert.ToInt64 on an array or a blob throws
        /// InvalidCastException ("Specified cast is not valid." under Mono) - the import's "While reading MXTables_DLC008.dx9.database"
        /// failure. An empty value is def; an array its first element; a blob of 1 / 2 / 4 / 8 bytes a little-endian integer, else its text.
        internal static long Num(object v, long def = 0)
        {
            switch (v)
            {
                case null: return def;
                case bool b: return b ? 1 : 0;
                case string s:
                    s = s.Trim();
                    if (long.TryParse(s, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture, out long l)) return l;
                    if (double.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out double d) && !double.IsNaN(d) && !double.IsInfinity(d)) return (long)d;
                    if (bool.TryParse(s, out bool bb)) return bb ? 1 : 0;
                    return def;
                case float[] f: return f.Length == 0 || float.IsNaN(f[0]) || float.IsInfinity(f[0]) ? def : (long)f[0];
                case ArraySegment<byte> a:
                    if (a.Array == null || a.Count == 0) return def;
                    if (a.Count == 1) return a.Array[a.Offset];
                    if (a.Count == 2) return BitConverter.ToUInt16(a.Array, a.Offset);
                    if (a.Count == 4) return BitConverter.ToUInt32(a.Array, a.Offset);
                    if (a.Count == 8) return BitConverter.ToInt64(a.Array, a.Offset);
                    return Num(Encoding.ASCII.GetString(a.Array, a.Offset, a.Count).TrimEnd('\0'), def);
                case IConvertible c:
                    try { return c.ToInt64(System.Globalization.CultureInfo.InvariantCulture); }
                    catch (Exception e) when (e is FormatException || e is InvalidCastException || e is OverflowException) { return def; }
                default: return def;
            }
        }
        /// A flag attribute (Compress enabled ...), as Num reads it.
        internal static bool Flag(object v, bool def = false) => v == null ? def : Num(v, def ? 1 : 0) != 0;

        /// The install's Database folder: MXReflex.exe's folder + Database (or the Database folder itself).
        internal static string DatabaseDir(string folder)
        {
            if (string.IsNullOrEmpty(folder)) return null;
            foreach (string d in new[] { Path.Combine(folder, "Database"), folder })
                if (RxFiles.Exists(Path.Combine(d, "MXTables_Default.dx9.database")) && RxFiles.Exists(Path.Combine(d, "Rider.dx9.database"))) return d;
            return null;
        }

        /// The databases of an install, read on demand (a database's file is ~100 KB of BXML).
        internal sealed class Install
        {
            public readonly string dir;
            readonly Dictionary<string, Database> _dbs = new Dictionary<string, Database>(StringComparer.OrdinalIgnoreCase);
            readonly Dictionary<string, FileStream> _files = new Dictionary<string, FileStream>(StringComparer.OrdinalIgnoreCase);
            public Install(string dir) { this.dir = dir; }

            /// A bike mod's install (BikeMods.Mod): its databases are named namespace + file name ("mod:<id>>MXs"), so nothing of the
            /// mod is cached under an original name; files: the folder's database / package files by lower-case file name (the mod's
            /// own case, on any file system).
            readonly string nameSpace; readonly Dictionary<string, string> modFiles;
            internal Install(string dir, string nameSpace, Dictionary<string, string> files) { this.dir = dir; this.nameSpace = nameSpace; modFiles = files; }
            /// (w-gearmods) A namespaced install over databases of the game's own (imported) Database folder: a gear / kit pack the player put
            /// there the original way (KitPacks). Its assets are read where the import extracted them (the database's own name), or from the
            /// package files when the folder has them.
            internal Install(string dir, string nameSpace, Dictionary<string, string> files, bool extracted) : this(dir, nameSpace, files) { this.extracted = extracted; }
            readonly bool extracted;
            /// The original install's router to the mods' installs (BikeMods.InstallOf: a namespaced database name -> its mod's install).
            internal static Func<string, Install> ModRouter;
            /// The original install's router to the custom tracks (TrackMods.Route): a track's namespaced database name ("modtrack:<id>>
            /// Beta_Track_Slot_1"), or the plain level name of the track bound for the event ("Beta_Track_Slot_1"), -> (the track's install,
            /// the namespaced name); (null, null) for any other name.
            internal static Func<string, (Install install, string name)> TrackRouter;

            public bool Has(string database)
            {
                if (nameSpace == null && database != null && TrackRouter != null && TrackRouter(database).install != null) return true;   // a custom track (TrackMods)
                return RxFiles.Exists(Path.Combine(dir, database + ".dx9.database"));   // (a level packed in Tracks/<level>.rxtrack too: RxFiles)
            }

            /// Thread-safe (the audio loader and other run-time loaders read databases from worker threads).
            public Database Db(string name)
            {
                if (nameSpace == null && name != null && name.StartsWith(BikeMods.NamespaceMark, StringComparison.Ordinal))
                    return ModRouter?.Invoke(name)?.Db(name);   // a mod's database (BikeMods): the mod's own install
                if (nameSpace == null && name != null && TrackRouter != null)
                {   // a custom track (TrackMods): its own install under its namespace, never cached here under the slot's name
                    var (track, full) = TrackRouter(name);
                    if (track != null) return track.Db(full);
                }
                lock (_dbs) return LoadDb(name);
            }

            /// A file of this install's folder: the mod's file of that name in any case, else dir + name.
            string FileOf(string fileName) => modFiles != null ? (modFiles.TryGetValue(fileName.ToLowerInvariant(), out var f) ? f : null) : Path.Combine(dir, fileName);

            Database LoadDb(string name)
            {
                if (_dbs.TryGetValue(name, out var d)) return d;
                string baseName = name;
                if (nameSpace != null) { if (!name.StartsWith(nameSpace, StringComparison.Ordinal)) { _dbs[name] = null; return null; } baseName = name.Substring(nameSpace.Length); }
                string file = FileOf(baseName + ".dx9.database");
                if (file == null || !RxFiles.Exists(file)) { _dbs[name] = null; return null; }
                d = new Database { name = name, baseName = baseName, dir = dir, mod = nameSpace != null, extracted = extracted };
                var root = Bxml(RxFiles.ReadAllBytes(file));
                foreach (var pn in root.All())
                {
                    if (pn.tag != "Package") continue;
                    string pf = pn.S("file");
                    Check(!string.IsNullOrEmpty(pf) && Path.GetFileName(pf) == pf && pf.IndexOf('\\') < 0, "package file name");
                    var p = new Package { name = pn.S("name"), file = pf + ".dx9.package", database = d };
                    if (d.mod && !extracted) { string real = FileOf(p.file); Check(real != null, "a mod's package file is missing: " + p.file); p.file = Path.GetFileName(real); }
                    long pkgOffset = Num(pn.attributes.TryGetValue("offset", out var po) ? po : null);
                    foreach (var an in pn.children)
                    {
                        if (an.tag != "Asset") continue;
                        var heap = an.Child("Heap"); var block = an.Child("Block"); var comp = an.Child("Compress");
                        if (heap == null) { d.skippedResources++; continue; }
                        if (block != null && block.attributes.TryGetValue("size", out var bs) && Num(bs) != 0) { d.skippedResources++; continue; } // unobserved block resources
                        bool lzx = comp != null && comp.attributes.TryGetValue("enabled", out var en) && Flag(en, comp.S("codec") == "LZX");   // (an empty flag: as its codec says)
                        if (lzx) Check(comp.S("codec") == "LZX", "compression codec");
                        object ho = null, hs = null;
                        Check(heap.attributes.TryGetValue("offset", out ho) && heap.attributes.TryGetValue("size", out hs), "asset heap without offset / size");
                        long heapSize = Num(hs, -1); Check(heapSize >= 0 && heapSize <= int.MaxValue, "asset heap size");
                        p.Add(new Asset { name = an.S("name"), type = an.S("type"), offset = pkgOffset + Num(ho), size = (int)heapSize, lzx = lzx, package = p });
                    }
                    if (p.name != null && !d.packages.ContainsKey(p.name)) d.packages[p.name] = p;
                }
                _dbs[name] = d;
                return d;
            }

            public Package Pkg(string database, string package) => Db(database)?.Find(package);
            public Asset Find(string database, string package, string asset, string type) => Pkg(database, package)?.Find(asset, type);

            public byte[] Read(Asset a, CancellationToken cancel = default)
            {
                Check(a != null, "asset");
                cancel.ThrowIfCancellationRequested();
                string root = Path.GetDirectoryName(a.package.database.extracted ? a.package.database.dir : dir);
                if (a.package.database.mod && !extracted && !a.package.database.extracted) return ReadPackaged(a, Path.Combine(a.package.database.dir, a.package.file), cancel);   // a bike mod (BikeMods): its own folder
                // the extracted copy (an imported library), unless the package itself is packed in a track archive (a level converted from
                // an original install: core/plan/RXARCHIVE.md); an original install reads a packed track's extracted assets when it has no package
                string package = Path.Combine(dir, a.package.file);
                string file = Path.Combine(root, ReflexAssetImport.AssetPath(a.package.database.baseName ?? a.package.database.name, a.package.name, a.name, a.type).Replace('/', Path.DirectorySeparatorChar));
                bool marker = File.Exists(Path.Combine(root, ReflexAssetImport.ExtractedMarker));
                if (marker && !RxFiles.Packed(package) || !marker && !RxFiles.Exists(package) && RxFiles.Packed(file))
                {
                    long length = RxFiles.Length(file);
                    Check(length >= 0, "extracted asset missing: " + file);
                    Check(length <= 64 << 20, "extracted asset size");
                    return RxFiles.ReadAllBytes(file);
                }
                return ReadPackaged(a, package, cancel);
            }
            byte[] ReadPackaged(Asset a, string path, CancellationToken cancel)
            {
                Check(a.size >= 0 && a.size <= 64 << 20, "asset size");
                byte[] raw = new byte[a.size];
                if (!File.Exists(path))
                {   // a package in an archive (a .rxtrack: RxFiles)
                    long length = RxFiles.Length(path);
                    Check(length >= 0, "package file missing: " + path);
                    Check(a.offset >= 0 && a.offset + a.size <= length, "asset outside its package file");
                    RxFiles.ReadAt(path, a.offset, raw, 0, a.size);
                    return a.lzx ? Lzx(raw, cancel) : raw;
                }
                lock (_files)
                {
                    if (!_files.TryGetValue(path, out var fs)) _files[path] = fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 16);
                    Check(a.offset >= 0 && a.offset + a.size <= fs.Length, "asset outside its package file");
                    fs.Position = a.offset;
                    int got = 0; while (got < raw.Length) { int n = fs.Read(raw, got, raw.Length - got); Check(n > 0, "truncated package file"); got += n; }
                }
                return a.lzx ? Lzx(raw, cancel) : raw;
            }
            public byte[] Read(string database, string package, string asset, string type, CancellationToken cancel = default)
            {
                var a = Find(database, package, asset, type);
                Check(a != null, $"{database}/{package}/{asset}.{type} missing");
                return Read(a, cancel);
            }
            public void Close()
            {
                lock (_files) { foreach (var f in _files.Values) f.Dispose(); _files.Clear(); }
            }
        }

        // ------------------------------------------------------------------------------------------- tables

        /// tbldata: table name -> rows (column -> value: int, float or string).
        internal static Dictionary<string, List<Dictionary<string, object>>> Tables(byte[] b)
        {
            var root = Bxml(b);
            var result = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
            foreach (var t in root.All())
            {
                if (t.tag != "Table") continue;
                string name = t.Child("Name")?.value as string;
                var rows = t.Child("Rows");
                if (name == null) continue;
                var list = new List<Dictionary<string, object>>();
                if (rows != null)
                    foreach (var r in rows.children)
                        if (r.tag == "Row") list.Add(new Dictionary<string, object>(r.attributes, StringComparer.Ordinal));
                result[name] = list;
            }
            return result;
        }
        internal static int Int(Dictionary<string, object> row, string col, int def = -1)
        {
            if (row == null || !row.TryGetValue(col, out var v) || v == null) return def;
            try { return Convert.ToInt32(v); } catch (Exception e) when (e is FormatException || e is InvalidCastException || e is OverflowException) { return def; }
        }
        internal static string Str(Dictionary<string, object> row, string col) => row != null && row.TryGetValue(col, out var v) ? v as string ?? v?.ToString() : null;

        /// localiz: key -> string.
        internal static Dictionary<string, string> Strings(byte[] b)
        {
            var d = new Dictionary<string, string>(StringComparer.Ordinal);
            Check(I32(b, 0) == 2, "string table version");
            int n = I32(b, 4), pool = I32(b, 8), p = 12;
            Check(n >= 0 && n < 200000 && pool >= 0 && 12 + pool <= b.Length, "string table counts");
            var keys = new string[n];
            for (int i = 0; i < n; i++)
            {
                int e = Array.IndexOf(b, (byte)0, p, 12 + pool - p); Check(e >= 0, "string table key");
                keys[i] = Encoding.ASCII.GetString(b, p, e - p); p = e + 1;
            }
            Check(p == 12 + pool, "string table key pool");
            p += 4 * n + 4;
            string Next()
            {
                int s = p;
                while (p + 1 < b.Length && (b[p] != 0 || b[p + 1] != 0)) p += 2;
                Check(p + 1 < b.Length, "string table text");
                string v = Encoding.Unicode.GetString(b, s, p - s); p += 2;
                return v;
            }
            for (int i = 0; i < n; i++)
            {
                string a = Next();
                if (keys[i].StartsWith("EMB_", StringComparison.Ordinal) && a.Trim('?').Length == 0) Next();
                d[keys[i]] = a;
            }
            return d;
        }

        // ------------------------------------------------------------------------------------------- models

        internal sealed class ModelNode { public string name; public int parent; public Matrix4x4 local, world; }
        internal sealed class Binding { public string material; public int surface, node; }
        internal sealed class Model
        {
            public string name;
            public ModelNode[] nodes;
            public (int count, int first, int distance)[] lods;
            public Binding[] bindings;
            public int surfaceCount;
            public int NodeIndex(string n) { for (int i = 0; i < nodes.Length; i++) if (string.Equals(nodes[i].name, n, StringComparison.OrdinalIgnoreCase)) return i; return -1; }
            public IEnumerable<Binding> Lod(int lod)
            {
                if (lods.Length == 0) yield break;
                var l = lods[Mathf.Clamp(lod, 0, lods.Length - 1)];
                for (int i = 0; i < l.count; i++) yield return bindings[l.first + i];
            }
        }

        /// A row-major D3D matrix at p as Unity's column-vector matrix (the transpose).
        static Matrix4x4 Mat(byte[] b, int p)
        {
            var m = new Matrix4x4();
            for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) m[c, r] = F32(b, p + (r * 4 + c) * 4);
            return m;
        }

        internal static Model ParseModel(byte[] b)
        {
            Check(b.Length >= 172 && U32(b, 0) == 0x88aabb00 && I32(b, 4) == 4, "model header");
            int count = I32(b, 12), namesSize = I32(b, 16), lodCount = I32(b, 20), matSize = I32(b, 24), bindCount = I32(b, 28), refCount = I32(b, 32);
            Check(count > 0 && count <= 1024 && lodCount >= 0 && lodCount <= 16 && bindCount >= 0 && bindCount <= 4096 && namesSize >= 0 && matSize >= 0 && refCount >= 0 && refCount <= 4096, "model counts");
            long expected = 172L + count * 108L + namesSize + lodCount * 12L + bindCount * 20L + matSize + refCount * 5L;
            Check(expected == b.Length, "model record lengths");
            var m = new Model { name = CString(b, 40, 72), nodes = new ModelNode[count], surfaceCount = refCount };
            int records = 172 + count * 64, names = records + count * 44;
            for (int i = 0; i < count; i++)
            {
                var local = Mat(b, 172 + i * 64);
                int nameAt = I32(b, records + i * 44); uint parent = U32(b, records + i * 44 + 4);
                Check(parent == 0xffffffff || parent < i, "model hierarchy order");
                int pi = parent == 0xffffffff ? -1 : (int)parent;
                var n = new ModelNode { name = CString(b, names + nameAt, names + namesSize), parent = pi, local = local };
                n.world = pi < 0 ? local : m.nodes[pi].world * local;
                m.nodes[i] = n;
            }
            int p = names + namesSize;
            m.lods = new (int, int, int)[lodCount];
            for (int i = 0; i < lodCount; i++, p += 12) { m.lods[i] = (I32(b, p), I32(b, p + 4), I32(b, p + 8)); Check(m.lods[i].Item1 >= 0 && m.lods[i].Item2 >= 0 && m.lods[i].Item1 + m.lods[i].Item2 <= bindCount, "model LOD"); }
            int matAt = p + bindCount * 20;
            m.bindings = new Binding[bindCount];
            for (int i = 0; i < bindCount; i++, p += 20)
            {
                int mo = I32(b, p), s = I32(b, p + 4), nd = I32(b, p + 8);
                Check(s >= 0 && s < refCount && nd >= 0 && nd < count && mo >= 0 && mo < matSize, "model binding");
                m.bindings[i] = new Binding { material = CString(b, matAt + mo, matAt + matSize), surface = s, node = nd };
            }
            return m;
        }

        internal sealed class Surface
        {
            public bool skinned;
            public Vector3[] positions, normals;
            public Vector2[] uv0, uv1;
            public Color32[] colors;       // D3DCOLOR (usage 10) when declared (scenery vertex colours; alpha = vertex alpha), else null
            public Vector3[] tangents;     // FLOAT3 tangents (usage 6) when declared, else null
            public int[] bones;      // 4 per vertex: node indices of the model (through the bone palette), -1 unused
            public float[] weights;  // 4 per vertex
            public int[] strip;
            /// A triangle list in place of the strip (null: the strip): a bike mod's .rxmod mesh (Content/RxModBikes.cs), clockwise front faces.
            public int[] triangles;
            /// Vertices whose stored position is not finite (null when none): a few shipped FLOAT16 positions are NaN (the Freestyle arena
            /// ramps BV_Ramp_360 / BV_Ramp4_360 / BV_Ramp_SKicker: 4 or 8 vertices of one surface). The original hands them to D3D9 as
            /// they are and the rasterizer drops every triangle with a NaN vertex, the rest of the model draws; here those positions are
            /// zeroed (finite bounds) and Triangles leaves out the triangles that use them.
            public bool[] invalid;
            /// The strip as triangles (D3D order; degenerate ones and ones with an invalid vertex dropped). Orient them with Orient.
            public List<int> Triangles()
            {
                if (triangles != null) return new List<int>(triangles);
                var t = new List<int>(strip.Length * 3);
                for (int i = 2; i < strip.Length; i++)
                {
                    int a = strip[i - 2], c = strip[i - 1], d = strip[i];
                    if ((i & 1) != 0) (a, c) = (c, a);
                    if (a == c || c == d || a == d) continue;
                    if (invalid != null && (invalid[a] || invalid[c] || invalid[d])) continue;
                    t.Add(a); t.Add(c); t.Add(d);
                }
                return t;
            }
        }

        internal static Surface[] ParseSurfaces(byte[] b)
        {
            Check(b.Length >= 12 && U32(b, 0) == 0x99bbcc00 && I32(b, 4) == 4, "surface header");
            int count = I32(b, 8); Check(count >= 0 && count <= 4096, "surface count");
            var result = new Surface[count];
            if (count == 0) return result;
            int p = 12 + count * 352; Check(I32(b, p) == 0, "surface data start"); p += 4;
            for (int s = 0; s < count; s++)
            {
                int q = 12 + s * 352;
                uint flags = U32(b, q + 12); int prims = I32(b, q + 16), nv = I32(b, q + 20), ib = I32(b, q + 24), pal = I32(b, q + 256);
                // vertex streams: header word 47 their count (1; 2 on FMX_Dover_RoughLayout, an editor layout model the shipped Dover levels
                // draw: its FLOAT3 positions in stream 1, the rest in stream 0, the original's draws_f910 binding both), word 48 + k the size
                // of stream k; the streams follow the index buffer in order
                int streams = I32(b, q + 188); if (streams == 0) streams = 1;
                Check(streams >= 1 && streams <= 4, "surface vertex streams");
                var sSize = new int[streams]; var sStride = new int[streams]; var sAt = new int[streams]; long vbTotal = 0;
                for (int k = 0; k < streams; k++) { sSize[k] = I32(b, q + 192 + 4 * k); vbTotal += sSize[k]; }
                bool sizesOk = true; foreach (int z in sSize) sizesOk &= z > 0 && nv > 0 && z % nv == 0;
                Check(nv > 0 && nv <= 65535 && ib == (prims + 2) * 2 && sizesOk && pal >= 0 && pal <= 256, "surface buffers");
                for (int k = 0; k < streams; k++) sStride[k] = sSize[k] / nv;
                int stride = sStride[0];
                Check(p + (long)pal + ib + vbTotal <= b.Length, "surface payload");
                var palette = new byte[pal]; Buffer.BlockCopy(b, p, palette, 0, pal); p += pal;
                var surf = new Surface { skinned = flags == 0x40003, strip = new int[ib / 2] };
                for (int i = 0; i < surf.strip.Length; i++) { surf.strip[i] = BitConverter.ToUInt16(b, p + i * 2); Check(surf.strip[i] < nv, "strip index"); }
                p += ib;
                for (int k = 0, at = p; k < streams; at += sSize[k], k++) sAt[k] = at;
                surf.positions = new Vector3[nv]; surf.normals = new Vector3[nv]; surf.uv0 = new Vector2[nv]; surf.uv1 = new Vector2[nv];
                bool pos = false, nrm = false;
                int boneAt = -1, weightAt = -1;
                for (int d = 0; d < 20; d++)
                {
                    int e = q + 28 + d * 8;
                    int stream = BitConverter.ToUInt16(b, e), offset = BitConverter.ToUInt16(b, e + 2), type = b[e + 4], usage = b[e + 6], index = b[e + 7];
                    if (stream == 255) break;
                    Check(stream < streams && offset < sStride[stream], "vertex declaration");
                    int sp = sAt[stream], sst = sStride[stream];
                    Check(stream == 0 || usage == 0, "vertex declaration (a second stream other than positions)");
                    if (usage == 0) { Check(type == 16 || type == 2, "position type"); pos = true; for (int v = 0; v < nv; v++) { int at = sp + v * sst + offset; surf.positions[v] = type == 2 ? new Vector3(F32(b, at), F32(b, at + 4), F32(b, at + 8)) : new Vector3(F16(b, at), F16(b, at + 2), F16(b, at + 4)); } }
                    else if (usage == 3) { Check(type == 2, "normal type"); nrm = true; for (int v = 0; v < nv; v++) { int at = p + v * stride + offset; surf.normals[v] = new Vector3(F32(b, at), F32(b, at + 4), F32(b, at + 8)); } }
                    else if (usage == 5 && index <= 1)
                    {
                        Check(type == 15 || type == 1, "UV type");
                        var uv = index == 0 ? surf.uv0 : surf.uv1;
                        for (int v = 0; v < nv; v++) { int at = p + v * stride + offset; uv[v] = type == 1 ? new Vector2(F32(b, at), F32(b, at + 4)) : new Vector2(F16(b, at), F16(b, at + 2)); }
                    }
                    else if (usage == 10 && index == 0 && type == 4)
                    {
                        // D3DCOLOR: a little-endian packed ARGB word
                        surf.colors = new Color32[nv];
                        for (int v = 0; v < nv; v++) { int at = p + v * stride + offset; surf.colors[v] = new Color32(b[at + 2], b[at + 1], b[at], b[at + 3]); }
                    }
                    else if (usage == 6 && index == 0 && type == 2)
                    {
                        surf.tangents = new Vector3[nv];
                        for (int v = 0; v < nv; v++) { int at = p + v * stride + offset; surf.tangents[v] = new Vector3(F32(b, at), F32(b, at + 4), F32(b, at + 8)); }
                    }
                    else if (usage == 2) { Check(type == 5, "blend index type"); boneAt = offset; }
                    else if (usage == 1) { Check(type == 8, "blend weight type"); weightAt = offset; }
                }
                Check(pos && nrm, "vertex positions and normals");
                if (surf.skinned)
                {
                    Check(boneAt >= 0 && weightAt >= 0 && pal > 0, "skinned vertex layout");
                    surf.bones = new int[nv * 4]; surf.weights = new float[nv * 4];
                    for (int v = 0; v < nv; v++)
                        for (int k = 0; k < 4; k++)
                        {
                            int bi = b[p + v * stride + boneAt + k]; float w = b[p + v * stride + weightAt + k] / 255f;
                            if (w > 0) { Check(bi < pal, "blend index outside the bone palette"); surf.bones[v * 4 + k] = palette[bi]; surf.weights[v * 4 + k] = w; }
                            else surf.bones[v * 4 + k] = -1;
                        }
                }
                // a few original normals are NaN or not unit (slivers): the adjacent faces' normal, else up
                for (int v = 0; v < nv; v++)
                {
                    var n = surf.normals[v];
                    if (!(float.IsFinite(n.x) && float.IsFinite(n.y) && float.IsFinite(n.z)) || Mathf.Abs(n.sqrMagnitude - 1f) > 2e-3f) surf.normals[v] = Vector3.zero;
                    if (!float.IsFinite(surf.positions[v].x + surf.positions[v].y + surf.positions[v].z))
                    {
                        (surf.invalid ??= new bool[nv])[v] = true;   // drawn by no triangle (see Surface.invalid)
                        surf.positions[v] = Vector3.zero; surf.normals[v] = Vector3.up;
                    }
                }
                RepairNormals(surf);
                p += (int)vbTotal;
                result[s] = surf;
            }
            Check(p == b.Length, "surface trailing data");
            return result;
        }

        static void RepairNormals(Surface s)
        {
            bool any = false; foreach (var n in s.normals) if (n == Vector3.zero) { any = true; break; }
            if (!any) return;
            var sum = new Vector3[s.normals.Length]; var t = s.Triangles();
            for (int i = 0; i < t.Count; i += 3)
            {
                var f = Vector3.Cross(s.positions[t[i + 1]] - s.positions[t[i]], s.positions[t[i + 2]] - s.positions[t[i]]);
                sum[t[i]] += f; sum[t[i + 1]] += f; sum[t[i + 2]] += f;
            }
            for (int v = 0; v < s.normals.Length; v++)
                if (s.normals[v] == Vector3.zero) s.normals[v] = sum[v].sqrMagnitude > 1e-20f ? sum[v].normalized : Vector3.up;
        }

        /// Turns each triangle (a, b, c) so that its face normal Cross(b - a, c - a) agrees with its vertex normals (strip
        /// windings alternate and connector steps flip them; the normals are the reliable side). In place.
        internal static void Orient(List<int> tri, IList<Vector3> pos, IList<Vector3> nrm)
        {
            for (int i = 0; i + 2 < tri.Count; i += 3)
            {
                int a = tri[i], b = tri[i + 1], c = tri[i + 2];
                var f = Vector3.Cross(pos[b] - pos[a], pos[c] - pos[a]);
                if (Vector3.Dot(f, nrm[a] + nrm[b] + nrm[c]) < 0) { tri[i + 1] = c; tri[i + 2] = b; }
            }
        }

        // ------------------------------------------------------------------------------------------- materials

        internal sealed class Material
        {
            public string name, pixelShader;
            public readonly List<string> textures = new List<string>();   // in sampler order
            public readonly Dictionary<string, float[]> parameters = new Dictionary<string, float[]>(StringComparer.Ordinal);
            public bool alphaBlend, alphaTest; public int alphaRef, cull = 3;
            public float[] Param(string n) => parameters.TryGetValue(n, out var v) ? v : null;
        }

        internal static Material ParseMaterial(string name, byte[] b)
        {
            var root = Bxml(b);
            var m = new Material { name = name };
            foreach (var n in root.All())
            {
                if (n.tag == "PixelShader" && m.pixelShader == null) m.pixelShader = n.S("asset");
                else if (n.tag == "Texture") m.textures.Add(n.S("textureName") ?? "");
                else if (n.tag == "Parameter" && n.S("name") != null) m.parameters[n.S("name")] = n.value as float[] ?? (n.value is float f ? new[] { f, f, f, f } : null);
                else if (n.tag == "XenonStates" && n.value is ArraySegment<byte> st)
                {
                    // D3DRENDERSTATETYPE / value pairs: 15 alpha test, 24 its reference, 22 cull mode, 27 alpha blending
                    for (int i = 0; i + 7 < st.Count; i += 8)
                    {
                        int key = BitConverter.ToInt32(st.Array, st.Offset + i), val = BitConverter.ToInt32(st.Array, st.Offset + i + 4);
                        if (key == 15) m.alphaTest = val != 0; else if (key == 24) m.alphaRef = val; else if (key == 22) m.cull = val; else if (key == 27) m.alphaBlend = val != 0;
                    }
                }
            }
            return m;
        }

        /// The pixel shader's sampler names by register (s0..), from its constant table.
        internal static Dictionary<int, string> PixelSamplers(byte[] b)
        {
            var result = new Dictionary<int, string>();
            Check(b.Length >= 8 && I32(b, 0) == 2, "shader resource");
            int count = I32(b, 4), p = 8;
            Check(count > 0 && count <= 64, "shader entries");
            for (int e = 0; e < count; e++)
            {
                int kind = I32(b, p), len = I32(b, p + 4); p += 8; Check(len > 0 && len <= 256 && p + len <= b.Length, "shader entry name"); p += len;
                int size = I32(b, p); p += 4; Check(size > 0 && p + size <= b.Length, "shader entry size");
                int code = p; p += size;
                if (kind != 1) continue;
                for (int q = code + 4; q + 4 <= code + size;)
                {
                    uint tok = U32(b, q);
                    if ((tok & 0xffff) != 0xfffe) break;
                    int n = (int)((tok >> 16) & 0x7fff), body = q + 4;
                    if (n >= 7 && body + 4 <= b.Length && b[body] == 'C' && b[body + 1] == 'T' && b[body + 2] == 'A' && b[body + 3] == 'B')
                    {
                        int t = body + 4, end = body + n * 4;
                        int consts = I32(b, t + 12), info = I32(b, t + 16);
                        for (int i = 0; i < consts; i++)
                        {
                            int ci = t + info + i * 20; Check(ci + 20 <= end, "shader constant table");
                            int nameAt = I32(b, ci), set = BitConverter.ToUInt16(b, ci + 4), reg = BitConverter.ToUInt16(b, ci + 6);
                            if (set == 3) result[reg] = CString(b, t + nameAt, end);
                        }
                        return result;
                    }
                    q = body + n * 4;
                }
            }
            return result;
        }

        // ------------------------------------------------------------------------------------------- textures

        internal sealed class TextureData
        {
            public int width, height; public TextureFormat format; public bool raw;   // raw: block data for the GPU, else RGBA32
            public readonly List<byte[]> mips = new List<byte[]>();
            public bool linear, alpha;
        }

        const int Dxt1 = 0x31545844, Dxt5 = 0x35545844, Dxt3 = 0x33545844;

        /// The texture's mip chain in Unity's row order (the first row at v = 0 is the image's bottom, as a PNG loaded with
        /// LoadImage: glTF / OBJ UVs with v flipped sample it right). DXT blocks are flipped as blocks; null for formats
        /// this reader does not take (the env maps' A16B16G16R16F).
        internal static TextureData ParseTexture(byte[] b, bool normalMap = false)
        {
            Check(b.Length >= 56 && U32(b, 0) == 0x77ddcc00 && I32(b, 4) == 4 && I32(b, 8) == 0 && I32(b, 12) == 1, "texture header");
            int levels = I32(b, 40), w = I32(b, 44), h = I32(b, 48), fmt = I32(b, 52);
            Check(levels > 0 && levels <= 15 && w > 0 && h > 0 && w <= 8192 && h <= 8192, "texture size");
            int p = 56 + levels * 8; Check(p <= b.Length, "texture mip table");
            var t = new TextureData { width = w, height = h };
            bool dxt = fmt == Dxt1 || fmt == Dxt5 || fmt == Dxt3;
            for (int l = 0; l < levels; l++)
            {
                int size = I32(b, 56 + l * 8), lw = Math.Max(1, w >> l), lh = Math.Max(1, h >> l);
                int bw = (lw + 3) / 4, bh = (lh + 3) / 4;
                int want = dxt ? bw * bh * (fmt == Dxt1 ? 8 : 16) : fmt == 21 || fmt == 22 || fmt == 112 ? lw * lh * 4 : -1;
                if (want < 0 || size != want || p + size > b.Length) return null;   // unobserved formats; placeholders (Numbers)
                var data = new byte[size]; Buffer.BlockCopy(b, p, data, 0, size); p += size;
                if (fmt == Dxt3) data = DecodeBlocks(data, lw, lh, Dxt3);
                else if (dxt) FlipBlocks(data, lw, lh, fmt == Dxt5);
                else if (fmt == 112) data = NormalG16R16F(data, lw, lh);
                else data = Bgra(data, lw, lh, fmt == 22);
                t.mips.Add(data);   // levels under 4 px are one block each, as Unity lays them out
            }
            t.raw = dxt && fmt != Dxt3;
            t.format = !t.raw ? TextureFormat.RGBA32 : fmt == Dxt1 ? TextureFormat.DXT1 : TextureFormat.DXT5;
            t.linear = fmt == 112 || normalMap;
            t.alpha = fmt == Dxt5 || fmt == Dxt3 || fmt == 21;
            // DXT1 with transparent texels (3-colour blocks using index 3: the chain's holes): decoded, Unity's DXT1 has no alpha
            if (fmt == Dxt1 && PunchThrough(t.mips[0]))
            {
                for (int l = 0; l < t.mips.Count; l++) { int lw = Math.Max(1, w >> l), lh = Math.Max(1, h >> l); FlipBlocks(t.mips[l], lw, lh, false); t.mips[l] = DecodeBlocks(t.mips[l], lw, lh, Dxt1); }
                t.raw = false; t.format = TextureFormat.RGBA32; t.alpha = true;
            }
            if (t.raw && (w % 4 != 0 || h % 4 != 0)) return null;
            return t;
        }

        static bool PunchThrough(byte[] d)
        {
            for (int i = 0; i + 7 < d.Length; i += 8)
            {
                ushort c0 = BitConverter.ToUInt16(d, i), c1 = BitConverter.ToUInt16(d, i + 2);
                if (c0 > c1) continue;
                uint idx = BitConverter.ToUInt32(d, i + 4);
                for (int k = 0; k < 16; k++) if (((idx >> (2 * k)) & 3) == 3) return true;
            }
            return false;
        }

        /// The first level as RGBA32 (Unity's row order, as the uploaded data): block levels decoded.
        internal static byte[] Rgba(TextureData t)
        {
            if (!t.raw) return t.mips[0];
            var d = (byte[])t.mips[0].Clone();
            FlipBlocks(d, t.width, t.height, t.format == TextureFormat.DXT5);   // back to the file's row order
            return DecodeBlocks(d, t.width, t.height, t.format == TextureFormat.DXT5 ? Dxt5 : Dxt1);
        }

        /// Reverses the rows of a DXT1 / DXT5 level: the block rows, and the four pixel rows inside each block (a level under
        /// four pixels tall keeps its rows at the block's top, reversed among themselves).
        static void FlipBlocks(byte[] d, int w, int h, bool dxt5)
        {
            int bs = dxt5 ? 16 : 8, bw = (w + 3) / 4, bh = (h + 3) / 4, rows = Math.Min(h, 4);
            var tmp = new byte[bs];
            void FlipInside(int at)
            {
                // colour indices: one byte per row
                int ci = at + (dxt5 ? 12 : 4);
                for (int r = 0; r < rows / 2; r++) { byte x = d[ci + r]; d[ci + r] = d[ci + rows - 1 - r]; d[ci + rows - 1 - r] = x; }
                if (!dxt5) return;
                // alpha indices: 48 bits, 12 per row
                ulong bits = 0; for (int i = 0; i < 6; i++) bits |= (ulong)d[at + 2 + i] << (8 * i);
                ulong o = bits;
                for (int r = 0; r < rows; r++)
                {
                    ulong row = (bits >> (12 * r)) & 0xfff; int to = rows - 1 - r;
                    o &= ~(0xfffUL << (12 * to)); o |= row << (12 * to);
                }
                for (int i = 0; i < 6; i++) d[at + 2 + i] = (byte)(o >> (8 * i));
            }
            for (int y = 0; y < bh / 2; y++)
                for (int x = 0; x < bw; x++)
                {
                    int a = (y * bw + x) * bs, c = ((bh - 1 - y) * bw + x) * bs;
                    Buffer.BlockCopy(d, a, tmp, 0, bs); Buffer.BlockCopy(d, c, d, a, bs); Buffer.BlockCopy(tmp, 0, d, c, bs);
                }
            for (int i = 0; i < bw * bh; i++) FlipInside(i * bs);
        }

        /// A8R8G8B8 / X8R8G8B8 (B, G, R, A bytes) -> RGBA32, rows bottom up.
        static byte[] Bgra(byte[] d, int w, int h, bool opaque)
        {
            var o = new byte[d.Length];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int s = (y * w + x) * 4, t = ((h - 1 - y) * w + x) * 4;
                    o[t] = d[s + 2]; o[t + 1] = d[s + 1]; o[t + 2] = d[s]; o[t + 3] = opaque ? (byte)255 : d[s + 3];
                }
            return o;
        }

        /// G16R16F tangent-space normal maps (x, y in -1..1, DirectX green) -> RGBA32 (glTF: +y up, z rebuilt), rows bottom up.
        static byte[] NormalG16R16F(byte[] d, int w, int h)
        {
            var o = new byte[w * h * 4];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    int s = (y * w + x) * 4, t = ((h - 1 - y) * w + x) * 4;
                    float nx = Mathf.Clamp(Mathf.HalfToFloat(BitConverter.ToUInt16(d, s)), -1, 1), ny = -Mathf.Clamp(Mathf.HalfToFloat(BitConverter.ToUInt16(d, s + 2)), -1, 1);
                    float nz = Mathf.Sqrt(Mathf.Max(0, 1 - nx * nx - ny * ny));
                    o[t] = (byte)Mathf.RoundToInt((nx * .5f + .5f) * 255); o[t + 1] = (byte)Mathf.RoundToInt((ny * .5f + .5f) * 255); o[t + 2] = (byte)Mathf.RoundToInt((nz * .5f + .5f) * 255); o[t + 3] = 255;
                }
            return o;
        }

        /// DXT1 / DXT3 / DXT5 blocks (the file's row order) -> RGBA32, rows bottom up.
        static byte[] DecodeBlocks(byte[] d, int w, int h, int fmt)
        {
            var o = new byte[w * h * 4]; int bw = (w + 3) / 4, bh = (h + 3) / 4, bs = fmt == Dxt1 ? 8 : 16, co = fmt == Dxt1 ? 0 : 8;
            var col = new Color32[4]; var alpha = new byte[8];
            Color32 C(ushort c) => new Color32((byte)((c >> 11 & 31) * 255 / 31), (byte)((c >> 5 & 63) * 255 / 63), (byte)((c & 31) * 255 / 31), 255);
            for (int by = 0; by < bh; by++)
                for (int bx = 0; bx < bw; bx++)
                {
                    int at = (by * bw + bx) * bs;
                    if (at + bs > d.Length) return o;
                    ushort c0 = BitConverter.ToUInt16(d, at + co), c1 = BitConverter.ToUInt16(d, at + co + 2);
                    col[0] = C(c0); col[1] = C(c1);
                    if (fmt != Dxt1 || c0 > c1) { col[2] = Color32.Lerp(col[0], col[1], 1 / 3f); col[3] = Color32.Lerp(col[0], col[1], 2 / 3f); }
                    else { col[2] = Color32.Lerp(col[0], col[1], .5f); col[3] = new Color32(0, 0, 0, 0); }
                    uint idx = BitConverter.ToUInt32(d, at + co + 4);
                    ulong abits = 0;
                    if (fmt == Dxt5)
                    {
                        alpha[0] = d[at]; alpha[1] = d[at + 1];
                        for (int i = 2; i < 8; i++) alpha[i] = alpha[0] > alpha[1] ? (byte)(((8 - i) * alpha[0] + (i - 1) * alpha[1]) / 7) : i < 6 ? (byte)(((6 - i) * alpha[0] + (i - 1) * alpha[1]) / 5) : i == 6 ? (byte)0 : (byte)255;
                        for (int i = 0; i < 6; i++) abits |= (ulong)d[at + 2 + i] << (8 * i);
                    }
                    for (int py = 0; py < 4; py++)
                        for (int px = 0; px < 4; px++)
                        {
                            int x = bx * 4 + px, y = by * 4 + py; if (x >= w || y >= h) continue;
                            int k = py * 4 + px; var c = col[(idx >> (2 * k)) & 3];
                            byte a = fmt == Dxt3 ? (byte)(((d[at + py * 2 + (px >> 1)] >> ((px & 1) * 4)) & 15) * 17) : fmt == Dxt5 ? alpha[(abits >> (3 * k)) & 7] : c.a;
                            int t = ((h - 1 - y) * w + x) * 4; o[t] = c.r; o[t + 1] = c.g; o[t + 2] = c.b; o[t + 3] = a;
                        }
                }
            return o;
        }

        /// A parsed texture as a Unity texture (mipmapped; block data straight to the GPU when it samples DXT).
        internal static Texture2D ToTexture(TextureData t, string name)
        {
            if (t == null || t.mips.Count == 0) return null;
            bool raw = t.raw && SystemInfo.SupportsTextureFormat(t.format);
            if (t.raw && !raw) return null;   // no DXT sampling: every Mac and PC the port supports has it
            var tex = new Texture2D(t.width, t.height, t.format, t.mips.Count, t.linear) { name = name, wrapMode = TextureWrapMode.Repeat, anisoLevel = 4, filterMode = FilterMode.Trilinear, hideFlags = HideFlags.DontSave };
            for (int i = 0; i < t.mips.Count; i++) tex.SetPixelData(t.mips[i], i);
            tex.Apply(false, true);
            return tex;
        }
    }
}
