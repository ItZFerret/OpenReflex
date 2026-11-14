// MX vs ATV Untamed converter, foundation: Reflex PC .dx9.database / .dx9.package access, the port of the reference's shared codec
// (mw2-to-reflex scripts/pkg.py), and the player's Reflex library behind one interface.
//   UxPkg        decode_blob / decode_head (the PC wrapper is the Untamed one: UxLzx) and encode_blob. The reference compresses with
//                Microsoft's XMemCompress, which the client cannot ship: EncodeBlob writes every 64 KiB chunk STORED (zsize == size, the
//                last chunk zero padded), the form pkg.encode_blob itself writes for an incompressible chunk. The core
//                (core/src/fmt_lzx.c fmt_xmem_resource: "a stored chunk") and the client (RfxData.Lzx) read it; UxPkg.RfxLzx is the
//                Unity-free copy of the client's reader, used to prove that.
//   UxTrackSet   pkg.TrackSet: the database's packages and assets, blob / read / read_logical / read_head, rebuild and
//                rebuild_with_additions. Backed by a package file (range reads, nothing held), by package bytes in memory, or by an
//                extracted library (the client's game files hold every asset decompressed in its own file).
//   IUxLibrary   the player's own Reflex content: a Database file by name, a level opened as a UxTrackSet. UxDirectoryLibrary reads an
//                original install (Database/*.dx9.package) and the client's "game files" (Assets.rxdata + Tracks/*.rxtrack, or loose).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxPkg
    {
        public const int CHUNK = 0x10000;

        /// decode_blob(blob): (decoded bytes incl. chunk padding, logical size).
        public static byte[] DecodeBlob(byte[] blob, out long logical, CancellationToken cancel = default) => UxLzx.DecodeBlob(blob, out logical, cancel);
        /// decode_blob(blob)[0][:n].
        public static byte[] Decode(byte[] blob, CancellationToken cancel = default) => UxLzx.Decode(blob, cancel);

        /// decode_head(blob, n): the first n (<= 65536) logical bytes, decoding only the first chunk.
        public static byte[] DecodeHead(byte[] blob, int n)
        {
            long size = UxBin.U32LE(blob, 4), zsize = UxBin.U32LE(blob, 8);
            byte[] c = zsize == size ? UxBin.PySlice(blob, 12, 12 + (int)zsize) : UxLzx.XmemDecompress(blob, 12, (int)zsize, (int)size);
            return UxBin.PySlice(c, 0, n);
        }

        /// encode_blob(data, size_check) with every chunk stored: u32 end, chunks {u32 65536, u32 65536, 65536 bytes (zero padded)},
        /// u32 size_check (the whole logical size, as DE writes it).
        public static byte[] EncodeBlob(byte[] data, long sizeCheck = -1)
        {
            if (sizeCheck < 0) sizeCheck = data.Length;
            int n = (int)Math.Min(sizeCheck, data.Length);
            int chunks = (n + CHUNK - 1) / CHUNK;
            var o = new byte[4 + (long)chunks * (8 + CHUNK) + 4];
            int p = 4;
            for (int at = 0; at < n; at += CHUNK)
            {
                UxBin.PutU32(o, p, CHUNK); UxBin.PutU32(o, p + 4, CHUNK); p += 8;
                Buffer.BlockCopy(data, at, o, p, Math.Min(CHUNK, n - at)); p += CHUNK;
            }
            UxBin.PutU32(o, 0, (uint)p);
            UxBin.PutU32(o, p, (uint)sizeCheck);
            return o;
        }

        /// The client's reader of a resource's LZX chunks (Content/RfxData.cs Rfx.Lzx; core/src/fmt_lzx.c fmt_xmem_resource reads the same
        /// way), copied here without Unity. Not used by the converter: the tests run it over what EncodeBlob writes.
        public static byte[] RfxLzx(byte[] data)
        {
            int limit = (int)UxBin.U32LE(data, 0); UxBin.Require(limit <= data.Length && limit >= 4, "LZX container");
            int tail = Math.Min(4, data.Length - limit), last = 0;
            for (int i = 0; i < tail; i++) last |= data[limit + i] << (8 * i);
            bool noLast = tail == 0;
            using (var output = new MemoryStream())
            {
                int p = 4;
                while (p < limit)
                {
                    int capacity = (int)UxBin.U32LE(data, p), packed = (int)UxBin.U32LE(data, p + 4); p += 8;
                    UxBin.Require(capacity > 0 && capacity <= 65536 && packed > 0 && p + packed <= limit, "LZX chunk");
                    int end = p + packed, size = end == limit && last > 0 ? last : capacity;
                    if (end == limit && last > capacity && last > output.Length && last - output.Length <= capacity) size = (int)(last - output.Length);
                    UxBin.Require(output.Length + size <= 64L << 20, "LZX output");
                    if (packed == capacity) { output.Write(data, p, Math.Min(size, packed)); p = end; continue; }   // a stored chunk
                    var decoder = new LzxDecoder(17); long before = output.Length;
                    while (p < end - 5 || p < end && !Zeros(data, p, end - p))
                    {
                        int frameSize = 32768, frameBytes;
                        if (data[p] == 255) { frameSize = data[p + 1] << 8 | data[p + 2]; frameBytes = data[p + 3] << 8 | data[p + 4]; p += 5; }
                        else { frameBytes = data[p] << 8 | data[p + 1]; p += 2; }
                        frameSize = (int)Math.Min(frameSize, size - (output.Length - before));
                        UxBin.Require(frameSize > 0 && frameSize <= 32768 && frameBytes > 0 && p + frameBytes <= end, "LZX frame");
                        using (var input = new MemoryStream(data, p, frameBytes, false))
                            UxBin.Require(decoder.Decompress(input, frameBytes, output, frameSize) == 0, "LZX decoding");
                        p += frameBytes;
                    }
                    if (noLast && end == limit && output.Length - before < size) size = (int)(output.Length - before);
                    UxBin.Require(output.Length - before == size, "LZX length");
                    p = end;
                }
                return output.ToArray();
            }
        }
        static bool Zeros(byte[] b, int p, int n) { for (int i = 0; i < n; i++) if (b[p + i] != 0) return false; return true; }

        /// int(x or 0) of a database attribute value (a pool integer, or a string of digits).
        public static long PyInt(object v)
        {
            switch (v)
            {
                case null: return 0;
                case long l: return l;
                case double d: return (long)d;
                case string s: return s.Length == 0 ? 0 : long.Parse(s.Trim(), System.Globalization.CultureInfo.InvariantCulture);
            }
            throw new UxException("not an integer attribute");
        }
    }

    /// pkg.Asset: an asset of a PC database (name, type, its package / asset / Heap nodes).
    public sealed class UxPcAsset
    {
        public readonly UxBxml db; public readonly int pkg_node, node, heap;
        public readonly string name, type; public readonly bool compressed;
        public readonly int index;
        internal UxPcAsset(UxBxml db, int pkgNode, int node, int index)
        {
            this.db = db; pkg_node = pkgNode; this.node = node; this.index = index;
            name = db.AttrStr(node, "name"); type = db.AttrStr(node, "type");
            heap = db.Find(node, "Heap");
            compressed = db.Find(node, "Compress") >= 0;
        }
        public long PkgOffset => UxPkg.PyInt(db.Attr(pkg_node, "offset"));
        public long Offset => UxPkg.PyInt(db.Attr(heap, "offset"));
        public long Size => UxPkg.PyInt(db.Attr(heap, "size"));
        public long AbsOffset => PkgOffset + Offset;
        public string PackageName => db.AttrStr(pkg_node, "name");
        public string PackageFile => db.AttrStr(pkg_node, "file");
        public (string name, string type) Key => (name, type);
        public override string ToString() => "<" + name + "." + type + " @" + AbsOffset + "+" + Size + ">";
    }

    /// pkg.TrackSet.
    public sealed class UxTrackSet
    {
        public readonly string name;                 // the database's name (the level)
        public readonly UxBxml db;
        public readonly List<int> packages = new List<int>();
        public readonly List<UxPcAsset> assets = new List<UxPcAsset>();
        readonly Dictionary<(string, string), UxPcAsset> _by = new Dictionary<(string, string), UxPcAsset>();

        // one of: the package bytes, a range reader of the package file(s), or a reader of extracted (logical) assets
        byte[] _package;
        readonly Func<UxPcAsset, byte[]> _blob;
        readonly Func<UxPcAsset, byte[]> _logical;
        /// True: the stored blobs do not exist (an extracted library); Blob(a) then gives the asset re-encoded (EncodeBlob for an asset
        /// with a Compress element, the bytes themselves otherwise).
        public bool Extracted => _logical != null;
        /// True: Blob(a) re-encodes also when the stored blob exists, so a track built from an original install and one built from the
        /// client's library carry the same bytes.
        public bool Reencode;

        /// TrackSet(database, package) on the files' bytes.
        public UxTrackSet(string name, byte[] database, byte[] package) : this(name, new UxBxml(database)) { _package = package; }
        /// A set whose stored blobs are read on demand (blob: the asset's stored bytes).
        public UxTrackSet(string name, byte[] database, Func<UxPcAsset, byte[]> blob) : this(name, new UxBxml(database)) { _blob = blob; }
        /// A set over an extracted library (logical: the asset's decompressed bytes).
        public static UxTrackSet OverExtracted(string name, byte[] database, Func<UxPcAsset, byte[]> logical) => new UxTrackSet(name, new UxBxml(database), logical);
        UxTrackSet(string name, UxBxml db, Func<UxPcAsset, byte[]> logical) : this(name, db) { _logical = logical; }
        UxTrackSet(string name, UxBxml db)
        {
            this.name = name; this.db = db;
            int pk = db.Find(0, "Packages");
            UxBin.Require(pk >= 0, name + ".dx9.database: no Packages");
            packages.AddRange(db.FindAll(pk, "Package"));
            foreach (int p in packages)
                foreach (int a in db.FindAll(p, "Asset"))
                {
                    var x = new UxPcAsset(db, p, a, assets.Count);
                    assets.Add(x);
                    if (!_by.ContainsKey(x.Key)) _by[x.Key] = x;
                }
        }

        /// get(name, type): the first asset of that name and type (exact); KeyError -> UxException.
        public UxPcAsset Get(string name, string type) => TryGet(name, type) ?? throw new UxException("no asset " + name + "." + type + " in " + this.name);
        public UxPcAsset TryGet(string name, string type) => _by.TryGetValue((name, type), out var a) ? a : null;
        public bool Has(string name, string type) => _by.ContainsKey((name, type));

        /// The package bytes, when the set was made from them (rebuild replaces them).
        public byte[] Package => _package;

        /// blob(a): the stored bytes.
        public byte[] Blob(UxPcAsset a)
        {
            if (_logical != null || Reencode) { var d = ReadLogical(a); return a.compressed ? UxPkg.EncodeBlob(d) : d; }
            if (_package != null)
            {
                long o = a.AbsOffset, n = a.Size;
                UxBin.Require(o >= 0 && n >= 0 && o + n <= _package.Length, a + ": outside the package");
                return UxBin.Slice(_package, (int)o, (int)n);
            }
            return _blob(a);
        }
        byte[] Stored(UxPcAsset a)
        {
            if (_package != null) { long o = a.AbsOffset, n = a.Size; UxBin.Require(o >= 0 && n >= 0 && o + n <= _package.Length, a + ": outside the package"); return UxBin.Slice(_package, (int)o, (int)n); }
            return _blob(a);
        }
        /// read(a): (decoded bytes incl. chunk padding, logical size).
        public byte[] Read(UxPcAsset a, out long logical)
        {
            if (_logical != null) { var d = _logical(a); logical = d.Length; return d; }
            byte[] b = Stored(a);
            if (!a.compressed) { logical = b.Length; return b; }
            return UxPkg.DecodeBlob(b, out logical);
        }
        /// read_logical(a).
        public byte[] ReadLogical(UxPcAsset a)
        {
            if (_logical != null) return _logical(a);
            byte[] b = Stored(a);
            return a.compressed ? UxPkg.Decode(b) : b;
        }
        public byte[] ReadLogical(string name, string type) => ReadLogical(Get(name, type));
        /// read_head(a, n): the first n logical bytes (n <= 65536).
        public byte[] ReadHead(UxPcAsset a, int n)
        {
            if (_logical != null) return UxBin.PySlice(_logical(a), 0, n);
            byte[] b = Stored(a);
            return !a.compressed ? UxBin.PySlice(b, 0, n) : UxPkg.DecodeHead(b, n);
        }

        /// rebuild(replacements): re-lays the package out in the original order with replaced stored blobs, patching Heap.offset / size
        /// and Package.offset / heapOffset in the database pool (the database structure otherwise unchanged). Returns the new package
        /// bytes (also kept as Package); the database is db (ToBytes()).
        public byte[] Rebuild(IDictionary<(string, string), byte[]> replacements)
        {
            var o = new UxWriter(1 << 20);
            var plan = new List<(UxPcAsset a, byte[] blob, int pos)>();
            foreach (int p in packages)
            {
                var list = assets.FindAll(a => a.pkg_node == p);
                UxNp.StableSort(list, (x, y) => x.Offset.CompareTo(y.Offset));
                int newPo = o.Length;
                foreach (var a in list)
                {
                    byte[] b = replacements != null && replacements.TryGetValue(a.Key, out var r) && r != null ? r : Blob(a);
                    plan.Add((a, b, o.Length - newPo));
                    o.Bytes(b);
                }
                foreach (var (a, b, pos) in plan)
                    if (a.pkg_node == p)
                    {
                        db.SetPoolAttr(db.AttrIndex(a.heap, "offset"), pos);
                        db.SetPoolAttr(db.AttrIndex(a.heap, "size"), b.Length);
                    }
                foreach (string k in new[] { "offset", "heapOffset" })
                {
                    int ai = db.AttrIndex(p, k);
                    if (ai >= 0 && db.attrs[ai][3] != 1) db.SetPoolAttr(ai, newPo);
                }
            }
            _package = o.ToArray();
            return _package;
        }

        /// rebuild_with_additions(ts, replacements, additions, target_package): like Rebuild, and appends new assets (a structural database
        /// rewrite through to_tree / encode_tree). additions: (name, type, stored blob, compressed), appended to target_package (default:
        /// the last package) and to the Handles list. Returns (database bytes, package bytes). storedDatabase: stored deflate for the
        /// database instead of zlib level 6.
        public (byte[] database, byte[] package) RebuildWithAdditions(IDictionary<(string, string), byte[]> replacements,
            IList<(string name, string type, byte[] blob, bool compressed)> additions, string targetPackage = null, bool storedDatabase = false)
        {
            var root = db.ToTree();
            var handles = root.Find("Handles");
            var pkgs = root.Find("Packages").FindAll("Package");
            UxElem tp = null;
            if (targetPackage == null) tp = pkgs[pkgs.Count - 1];
            else foreach (var p in pkgs) if (p.AttrStr("name") == targetPackage) { tp = p; break; }
            UxBin.Require(tp != null, "no package " + targetPackage);
            UxElem tmplC = null;
            foreach (var p in pkgs) { foreach (var a in p.FindAll("Asset")) if (a.Find("Compress") != null) { tmplC = a; break; } if (tmplC != null) break; }
            UxBin.Require(tmplC != null && handles != null && handles.children.Count > 0, "database without a compressed asset / handle to copy");
            var tmplH = handles.children[0];
            var newElems = new List<(UxElem e, byte[] blob)>();
            foreach (var (name, typ, blob, compressed) in additions)
            {
                if (_by.ContainsKey((name, typ))) throw new UxException("asset already exists: " + name + "." + typ);
                var e = tmplC.DeepCopy();
                e.attrs = new List<UxAttr> { new UxAttr("name", name), new UxAttr("type", typ) };
                if (!compressed) e.children.RemoveAll(c => c.tag == "Compress");
                tp.children.Add(e); newElems.Add((e, blob));
                var h = tmplH.DeepCopy(); h.attrs = new List<UxAttr> { new UxAttr("name", name), new UxAttr("type", typ) };
                handles.children.Add(h);
            }
            var old = new Dictionary<(int, string, string), UxPcAsset>();
            foreach (var a in assets) old[(a.pkg_node, a.name, a.type)] = a;          // (a later asset of the same name wins, as the dict)
            var o = new UxWriter(1 << 20);
            void SetU(UxElem el, string key, long val)
            {
                foreach (var at in el.attrs)
                    if (at.name == key)
                    {
                        var b = new byte[4];
                        if (at.type == 4) UxBin.PutU32(b, 0, checked((uint)val)); else UxBin.PutI32(b, 0, checked((int)val));
                        at.raw = b; at.str = null;
                        return;
                    }
                throw new UxException("no attribute " + key);
            }
            for (int pi = 0; pi < pkgs.Count; pi++)
            {
                var p = pkgs[pi]; int newPo = o.Length;
                var items = new List<(double order, UxElem e, byte[] blob)>();
                foreach (var e in p.FindAll("Asset"))
                {
                    byte[] nb = null;
                    foreach (var (ne, b) in newElems) if (ReferenceEquals(ne, e)) { nb = b; break; }
                    if (nb != null) items.Add((double.PositiveInfinity, e, nb));
                    else
                    {
                        var a = old[(packages[pi], e.AttrStr("name"), e.AttrStr("type"))];
                        byte[] b = replacements != null && replacements.TryGetValue(a.Key, out var r) && r != null ? r : Blob(a);
                        items.Add((a.Offset, e, b));
                    }
                }
                UxNp.StableSort(items, (x, y) => x.order.CompareTo(y.order));
                foreach (var (_, e, b) in items)
                {
                    var heap = e.Find("Heap");
                    SetU(heap, "offset", o.Length - newPo); SetU(heap, "size", b.Length);
                    o.Bytes(b);
                }
                foreach (string k in new[] { "offset", "heapOffset" }) if (p.Attr(k) != null) SetU(p, k, newPo);
            }
            return (UxBxml.EncodeTree(root, db.version, db.unknown, storedDatabase), o.ToArray());
        }
    }

    /// The player's own Reflex content, as the converter reads it.
    public interface IUxLibrary : IDisposable
    {
        /// What it is, for messages ("original install C:/...", "game library C:/...").
        string Describe { get; }
        /// Database/<level>.dx9.database exists.
        bool HasLevel(string level);
        /// Database/<level>.dx9.<kind>: 'database', 'level' or 'scene' (the whole file).
        byte[] ReadLevelFile(string level, string kind);
        /// A file of the game's root folder ("AnimationSets.bxml"), or null when it is not there.
        byte[] ReadRootFile(string relative);
        /// pkg.TrackSet of a stock level (or any database of the Database folder), cached.
        UxTrackSet OpenLevel(string level);
        /// The names of every *.dx9.database, sorted (sorted(GAME_DB.glob('*.dx9.database'))).
        List<string> Levels();
    }

    /// A Reflex library in a folder: an original install (the folder with MXReflex.exe and Database, or the Database folder itself), or
    /// the Open Reflex client's "game files" (every asset extracted: loose, or packed in Assets.rxdata and Tracks/<level>.rxtrack).
    public sealed class UxDirectoryLibrary : IUxLibrary
    {
        public const string ExtractedMarker = "reflex-extracted-v1.txt";
        readonly UxVfs _vfs; readonly string _root; readonly bool _marker;
        readonly Dictionary<string, UxTrackSet> _sets = new Dictionary<string, UxTrackSet>(StringComparer.OrdinalIgnoreCase);

        public UxDirectoryLibrary(string folder)
        {
            string f = Path.GetFullPath(folder);
            // the Database folder itself was given
            if (!Directory.Exists(Path.Combine(f, "Database")) && string.Equals(Path.GetFileName(f.TrimEnd(Path.DirectorySeparatorChar, '/')), "Database", StringComparison.OrdinalIgnoreCase))
                f = Path.GetDirectoryName(f.TrimEnd(Path.DirectorySeparatorChar, '/'));
            UxBin.Require(Directory.Exists(Path.Combine(f, "Database")), "no Reflex Database folder in " + folder);
            _root = f; _vfs = new UxVfs(f);
            _marker = File.Exists(Path.Combine(f, ExtractedMarker));
        }
        public string Root => _root;
        public UxVfs Files => _vfs;
        /// The library holds extracted assets (the client's import), not the original packages.
        public bool IsExtracted => _marker;
        public string Describe => (_marker ? "game library " : "original install ") + _root;

        public bool HasLevel(string level) => _vfs.Exists("Database/" + level + ".dx9.database");
        public byte[] ReadLevelFile(string level, string kind) => _vfs.ReadAllBytes("Database/" + level + ".dx9." + kind);
        public byte[] ReadRootFile(string relative) => _vfs.Exists(relative) ? _vfs.ReadAllBytes(relative) : null;
        public List<string> Levels()
        {
            var r = new List<string>();
            foreach (string n in _vfs.ListDir("Database"))
                if (n.EndsWith(".dx9.database", StringComparison.OrdinalIgnoreCase)) r.Add(n.Substring(0, n.Length - ".dx9.database".Length));
            r.Sort((a, b) => string.CompareOrdinal(UxCommon.PyLower(a), UxCommon.PyLower(b)));
            return r;
        }

        /// Content/ReflexAssetImport.Segment: a path segment of an extracted asset.
        public static string Segment(string value)
        {
            var sb = new StringBuilder();
            foreach (byte b in Encoding.UTF8.GetBytes(value ?? ""))
                if (b >= 65 && b <= 90 || b >= 97 && b <= 122 || b >= 48 && b <= 57 || b == 95 || b == 45) sb.Append((char)b); else sb.Append('%').Append(b.ToString("X2"));
            UxBin.Require(sb.Length > 0, "Empty asset path segment.");
            string segment = sb.ToString(), upper = segment.ToUpperInvariant();
            bool device = upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
                upper.Length == 4 && (upper.StartsWith("COM", StringComparison.Ordinal) || upper.StartsWith("LPT", StringComparison.Ordinal)) && upper[3] >= '1' && upper[3] <= '9';
            return device ? "%" + ((byte)segment[0]).ToString("X2") + segment.Substring(1) : segment;
        }
        /// Content/ReflexAssetImport.AssetPath.
        public static string AssetPath(string database, string package, string asset, string type)
            => "Assets/" + Segment(type) + "/" + Segment(database) + "/" + Segment(package) + "/" + Segment(asset) + "." + Segment(type);

        public UxTrackSet OpenLevel(string level)
        {
            lock (_sets)
            {
                if (_sets.TryGetValue(level, out var ts)) return ts;
                UxBin.Require(HasLevel(level), "stock level " + level + " not in " + Describe);
                byte[] database = ReadLevelFile(level, "database");
                // the rule of Rfx.Install.Read: the extracted copy, unless the package itself is there (packed in a track archive, or an
                // original install); an original install reads a track archive's extracted assets when it has no package
                string PackagePath(UxPcAsset a) => "Database/" + (a.PackageFile ?? level) + ".dx9.package";
                bool ExtractedFor(UxPcAsset a)
                {
                    string package = PackagePath(a);
                    if (_marker) return !_vfs.Packed(package);
                    return !_vfs.Exists(package) && _vfs.Packed(AssetPath(level, a.PackageName, a.name, a.type));
                }
                UxTrackSet set = null;
                bool? extracted = null;
                {
                    var probe = new UxTrackSet(level, database, a => null);
                    if (probe.assets.Count > 0) extracted = ExtractedFor(probe.assets[0]);
                }
                if (extracted == true)
                    set = UxTrackSet.OverExtracted(level, database, a =>
                    {
                        string file = AssetPath(level, a.PackageName, a.name, a.type);
                        long length = _vfs.Length(file);
                        UxBin.Require(length >= 0, "extracted asset missing: " + file);
                        UxBin.Require(length <= 64 << 20, "extracted asset size");
                        return _vfs.ReadAllBytes(file);
                    });
                else
                    set = new UxTrackSet(level, database, a =>
                    {
                        string package = PackagePath(a);
                        long length = _vfs.Length(package);
                        UxBin.Require(length >= 0, "package file missing: " + package);
                        long o = a.AbsOffset, n = a.Size;
                        UxBin.Require(n >= 0 && n <= 64 << 20 && o >= 0 && o + n <= length, a + ": outside its package file");
                        return _vfs.ReadRange(package, o, (int)n);
                    });
                _sets[level] = set;
                return set;
            }
        }

        public void Dispose() { _vfs.Dispose(); }
    }
}
