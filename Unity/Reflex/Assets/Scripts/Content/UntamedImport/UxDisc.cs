// MX vs ATV Untamed converter, foundation, stage S0: an Untamed level read straight from the player's disc (the image or the extracted
// folder: the Alive importer's DiscVolume / FolderVolume), as untamed/tools/uxpkg.py + uxlzx.py do it.
//   UxLzx        the XMem native framing of one package chunk (uxlzx.xmem_decompress) on the client's LzxDecoder, and the resource
//                wrapper (uxpkg.decode_blob: u32 end, chunks {u32 65536, u32 zsize, data}, u32 size check).
//   UxDiscLevel  uxpkg.UxLevel: the .xenon.database (little-endian zlib BXML: Handles/Asset, Packages/Package(offset, blockOffset)/
//                Asset/{Heap, Compress}), every asset's bytes by name and type (read from the .xenon.package range by range: nothing but
//                the database stays in memory, plus a bounded cache of decoded assets), the names uxpkg's extract gives the files
//                (extract/<Level>/<type>/<name>.<type>, '@<package>' for a repeated name), the level / scene files and the race line.
// Reads are thread safe.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxLzx
    {
        public const int WindowBitsDefault = 17, Chunk = 0x10000;
        static readonly object s_tables = new object();

        /// uxlzx.xmem_decompress(data[start:start+length], out_size): one XMem native chunk, zero padded / cut to outSize bytes.
        public static byte[] XmemDecompress(byte[] data, int start, int length, int outSize, int windowBits = WindowBitsDefault)
        {
            LzxDecoder decoder;
            lock (s_tables) decoder = new LzxDecoder(windowBits);     // (its static tables are built on first use)
            var result = new byte[outSize];
            using (var output = new MemoryStream())
            {
                int p = start, end = start + length;
                while (p + 1 < end)
                {
                    int frame = 0x8000, block;
                    if (data[p] == 0xFF)
                    {
                        UxBin.Require(p + 5 <= end, "LZX: truncated frame header");
                        frame = data[p + 1] << 8 | data[p + 2]; block = data[p + 3] << 8 | data[p + 4]; p += 5;
                    }
                    else { block = data[p] << 8 | data[p + 1]; p += 2; }
                    if (block == 0 || frame == 0) break;
                    UxBin.Require(p + block <= end, "LZX: block overruns chunk");
                    int rc;
                    try { using (var input = new MemoryStream(data, p, block, false)) rc = decoder.Decompress(input, block, output, frame); }
                    catch (Exception e) when (!(e is OperationCanceledException) && !(e is UxException)) { throw new UxException("LZX: " + e.Message, e); }
                    UxBin.Require(rc == 0, "LZX: decoder error " + rc);
                    p += block;
                    if (output.Length >= outSize) break;
                }
                int n = (int)Math.Min(output.Length, outSize);
                output.Position = 0;
                int got = 0; while (got < n) { int r = output.Read(result, got, n - got); if (r <= 0) break; got += r; }
            }
            return result;
        }

        /// uxpkg.decode_blob(blob): (decoded bytes incl. chunk padding, logical size). The same rules as pkg.decode_blob.
        public static byte[] DecodeBlob(byte[] blob, out long logical, CancellationToken cancel = default)
        {
            UxBin.Require(blob.Length >= 8, "resource blob of " + blob.Length + " bytes");
            long fz = UxBin.U32LE(blob, 0);
            UxBin.Require(fz >= 4 && fz + 4 <= blob.Length, "resource blob: container end outside the blob");
            var parts = new List<byte[]>(); long total = 0; int p = 4;
            while (p < fz)
            {
                cancel.ThrowIfCancellationRequested();
                UxBin.Require(p + 8 <= fz, "resource blob: truncated chunk header");
                long size = UxBin.U32LE(blob, p), zsize = UxBin.U32LE(blob, p + 4); p += 8;
                UxBin.Require(zsize <= fz - p && size <= 1 << 24, "resource blob: chunk outside the container");
                byte[] part = zsize == size ? UxBin.Slice(blob, p, (int)zsize) : XmemDecompress(blob, p, (int)zsize, (int)size);
                p += (int)zsize;
                parts.Add(part); total += part.Length;
            }
            UxBin.Require(total <= int.MaxValue - 64, "resource too large");
            var o = new byte[total]; int at = 0;
            foreach (var part in parts) { Buffer.BlockCopy(part, 0, o, at, part.Length); at += part.Length; }
            long check = UxBin.U32LE(blob, (int)fz);
            // SIZE_CHECK: DE writes the whole logical size; stock blobs store the bytes used in the last 64 KiB chunk
            long nch = total / Chunk;
            if (nch > 1 && check <= (nch - 1) * Chunk) check += (nch - 1) * Chunk;
            logical = check;
            return o;
        }

        /// decode_blob(blob)[0][:n]: the logical bytes of a compressed resource.
        public static byte[] Decode(byte[] blob, CancellationToken cancel = default)
        {
            var d = DecodeBlob(blob, out long n, cancel);
            if (n >= d.Length) return d;
            return UxBin.Slice(d, 0, (int)Math.Max(0, n));
        }
    }

    /// uxpkg.Asset.
    public sealed class UxDiscAsset
    {
        public string name, type, package, codec;
        public long pkg_offset, offset, size;
        public bool compressed; public int index;
        /// The file uxpkg's extract writes for it, relative to extract/<Level>: '<type>/<stem>.<type>' ('/' separated).
        public string file;
        public long AbsOffset => pkg_offset + offset;
        public override string ToString() => "<" + name + "." + type + " [" + package + "] @" + AbsOffset + "+" + size + (compressed ? " lzx" : "") + ">";
    }

    /// uxpkg.UxLevel on a disc volume.
    public sealed class UxDiscLevel
    {
        public readonly UxLevelRow lv;
        public readonly Volume volume;
        /// The volume's folder of the level files ("Database", "<folder>/Database") and the folder that holds AI ("" = the volume's root).
        public readonly string directory, root;
        public readonly string name;
        public readonly UxBxml db;
        /// Packages/Package attributes in document order (name, file, offset, blockOffset, heapOffset).
        public readonly List<JObj> packages = new List<JObj>();
        public readonly List<UxDiscAsset> assets = new List<UxDiscAsset>();
        /// Handles/Asset (name, type).
        public readonly List<(string name, string type)> handles = new List<(string, string)>();
        readonly VolumeFile _package;
        readonly Dictionary<string, UxDiscAsset> _exact = new Dictionary<string, UxDiscAsset>(StringComparer.Ordinal);
        readonly Dictionary<string, UxDiscAsset> _lower = new Dictionary<string, UxDiscAsset>(StringComparer.Ordinal);
        readonly Dictionary<string, UxDiscAsset> _files = new Dictionary<string, UxDiscAsset>(StringComparer.Ordinal);

        // a bounded cache of decoded assets (least recently used out first)
        readonly object _gate = new object();
        readonly Dictionary<int, LinkedListNode<KeyValuePair<int, byte[]>>> _cache = new Dictionary<int, LinkedListNode<KeyValuePair<int, byte[]>>>();
        readonly LinkedList<KeyValuePair<int, byte[]>> _lru = new LinkedList<KeyValuePair<int, byte[]>>();
        long _cached;
        /// The most decoded bytes kept (0: nothing is kept).
        public long CacheBytes = 96L << 20;

        public UxDiscLevel(Volume volume, string directory, string root, UxLevelRow lv, CancellationToken cancel = default)
        {
            this.volume = volume; this.directory = directory ?? ""; this.root = root; this.lv = lv; name = lv.disc;
            db = new UxBxml(ReadFile("database", cancel));
            _package = FileOf("package");
            int pk = db.Find(0, "Packages");
            UxBin.Require(pk >= 0, name + ".xenon.database: no Packages");
            foreach (int p in db.FindAll(pk, "Package"))
            {
                string pname = db.AttrStr(p, "name"); long poff = db.AttrInt(p, "offset");
                var pd = new JObj();
                foreach (var kv in db.AttrItems(p)) pd.Set(kv.Value, db.AttrValue(kv.Key));
                packages.Add(pd);
                foreach (int a in db.FindAll(p, "Asset"))
                {
                    int heap = db.Find(a, "Heap"), comp = db.Find(a, "Compress");
                    UxBin.Require(heap >= 0, name + ".xenon.database: an asset without a heap region");
                    assets.Add(new UxDiscAsset
                    {
                        name = db.AttrStr(a, "name"), type = db.AttrStr(a, "type"), package = pname, pkg_offset = poff,
                        offset = db.AttrInt(heap, "offset"), size = db.AttrInt(heap, "size"),
                        compressed = comp >= 0 && db.AttrInt(comp, "enabled") != 0,
                        codec = comp >= 0 ? db.AttrStr(comp, "codec") : null, index = assets.Count,
                    });
                }
            }
            int hn = db.Find(0, "Handles");
            if (hn >= 0) foreach (int h in db.Children(hn)) handles.Add((db.AttrStr(h, "name"), db.AttrStr(h, "type")));
            // the names uxpkg.extract gives the files: a repeated (name, type) (ignoring case) gets '@<package>'
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var a in assets)
            {
                string stem = SafeName(a.name), t = SafeName(a.type);
                if (!seen.Add(UxCommon.PyLower(stem) + "\n" + a.type)) stem = stem + "@" + a.package;
                a.file = t + "/" + stem + "." + t;
                string fk = UxCommon.PyLower(a.file);
                if (!_files.ContainsKey(fk)) _files[fk] = a;
                string k = a.name + "\n" + a.type;
                if (!_exact.ContainsKey(k)) _exact[k] = a;
                string lk = UxCommon.PyLower(a.name) + "\n" + a.type;
                if (!_lower.ContainsKey(lk)) _lower[lk] = a;
            }
        }

        /// uxpkg.safe_name.
        public static string SafeName(string s)
        {
            var c = s.ToCharArray();
            for (int i = 0; i < c.Length; i++) if ("<>:\"/\\|?*".IndexOf(c[i]) >= 0 || c[i] < 32) c[i] = '_';
            return new string(c);
        }

        VolumeFile FileOf(string kind)
        {
            string path = (directory.Length == 0 ? "" : directory + "/") + name + ".xenon." + kind;
            return volume.Find(path) ?? throw new UxException("missing " + path + " in " + volume.displayName);
        }
        /// The level's own files whole: 'database', 'level', 'scene' (and 'package': 35-60 MB, avoid).
        public byte[] ReadFile(string kind, CancellationToken cancel = default) => volume.Read(FileOf(kind), cancel);
        public long FileSize(string kind) => FileOf(kind).size;
        /// The disc's race line AI/<Level>-<mode>.tdf07 (tdf_path(lv)).
        public byte[] ReadTdf(CancellationToken cancel = default)
        {
            UxBin.Require(root != null, "the source has no AI folder (choose the folder that holds both Database and AI)");
            string path = (root.Length == 0 ? "" : root + "/") + "AI/" + lv.tdf;
            var f = volume.Find(path) ?? throw new UxException("missing " + path + " in " + volume.displayName);
            return volume.Read(f, cancel);
        }

        /// get(name, type, package): the first asset of that name and type (then ignoring the name's case); KeyError -> UxException.
        public UxDiscAsset Get(string name, string type, string package = null) => TryGet(name, type, package) ?? throw new UxException("no asset " + name + "." + type + " in " + this.name);
        public UxDiscAsset TryGet(string name, string type, string package = null)
        {
            if (package == null)
            {
                if (_exact.TryGetValue(name + "\n" + type, out var a)) return a;
                return _lower.TryGetValue(UxCommon.PyLower(name) + "\n" + type, out a) ? a : null;
            }
            foreach (var a in assets) if (a.name == name && a.type == type && a.package == package) return a;
            string lo = UxCommon.PyLower(name);
            foreach (var a in assets) if (UxCommon.PyLower(a.name) == lo && a.type == type && a.package == package) return a;
            return null;
        }
        /// find(type, package).
        public List<UxDiscAsset> Find(string type = null, string package = null)
            => assets.FindAll(a => (type == null || a.type == type) && (package == null || a.package == package));

        /// The asset behind extract/<Level>/<type>/<name>.<type> (ux_common.extract_asset: the name as given, no safe_name; the file system
        /// the reference runs on ignores case), or null. A repeated name's later copies are '<name>@<package>'.
        public UxDiscAsset ExtractAsset(string name, string type)
            => _files.TryGetValue(UxCommon.PyLower(type + "/" + name + "." + type), out var a) ? a : null;
        public bool HasExtract(string name, string type) => ExtractAsset(name, type) != null;
        /// extract_asset(lv, name, typ).read_bytes().
        public byte[] ReadExtract(string name, string type, CancellationToken cancel = default)
            => Read(ExtractAsset(name, type) ?? throw new UxException(this.name + ": no extracted asset " + type + "/" + name + "." + type), cancel);

        /// The asset behind a path relative to extract/<Level> ('texture\\Foo.texture' or 'texture/Foo.texture': the 'file' of the
        /// extract index and of the stage manifests; case ignored, as the reference's file system does), or null.
        public UxDiscAsset AssetOfFile(string file) => _files.TryGetValue(UxCommon.PyLower(file.Replace('\\', '/')), out var a) ? a : null;
        /// (U.EXTRACT / lv.disc / file).read_bytes().
        public byte[] ReadExtractFile(string file, CancellationToken cancel = default)
            => Read(AssetOfFile(file) ?? throw new UxException(this.name + ": no extracted file " + file), cancel);
        /// (U.EXTRACT / lv.disc / file).exists().
        public bool HasExtractFile(string file) => AssetOfFile(file) != null;

        /// blob(a): the stored bytes.
        public byte[] Blob(UxDiscAsset a, CancellationToken cancel = default)
        {
            UxBin.Require(a.size >= 0 && a.size <= int.MaxValue - 64 && a.AbsOffset >= 0 && a.AbsOffset + a.size <= _package.size, a + ": outside the package file");
            return volume.ReadRange(_package, a.AbsOffset, (int)a.size, cancel);
        }
        /// read(a): the logical asset bytes (decompressed, chunk padding removed). The array may be shared with the cache: do not modify it.
        public byte[] Read(UxDiscAsset a, CancellationToken cancel = default)
        {
            lock (_gate)
                if (_cache.TryGetValue(a.index, out var node)) { _lru.Remove(node); _lru.AddFirst(node); return node.Value.Value; }
            byte[] b = Blob(a, cancel);
            byte[] d = a.compressed ? UxLzx.Decode(b, cancel) : b;
            if (CacheBytes > 0 && d.Length <= CacheBytes / 4)
                lock (_gate)
                    if (!_cache.ContainsKey(a.index))
                    {
                        _cache[a.index] = _lru.AddFirst(new KeyValuePair<int, byte[]>(a.index, d)); _cached += d.Length;
                        while (_cached > CacheBytes && _lru.Count > 1)
                        {
                            var last = _lru.Last; _lru.RemoveLast(); _cache.Remove(last.Value.Key); _cached -= last.Value.Value.Length;
                        }
                    }
            return d;
        }
        public byte[] Read(string name, string type, CancellationToken cancel = default) => Read(Get(name, type), cancel);
        /// read_padded(a): (decoded bytes incl. the chunk zero padding, logical size).
        public byte[] ReadPadded(UxDiscAsset a, out long logical, CancellationToken cancel = default)
        {
            byte[] b = Blob(a, cancel);
            if (!a.compressed) { logical = b.Length; return b; }
            return UxLzx.DecodeBlob(b, out logical, cancel);
        }
        public void ClearCache() { lock (_gate) { _cache.Clear(); _lru.Clear(); _cached = 0; } }

        /// type_counts(): package -> type -> count (first-seen order).
        public JObj TypeCounts()
        {
            var c = new JObj();
            foreach (var a in assets)
            {
                var p = c.Get(a.package) as JObj; if (p == null) { p = new JObj(); c.Set(a.package, p); }
                p.Set(a.type, (long)(p.Get(a.type) ?? 0L) + 1);
            }
            return c;
        }

        /// The index.json uxpkg.extract writes ({'level', 'packages', 'assets': [{name, type, package, offset, stored_size, compressed, file,
        /// size, md5}]}; file with the reference's '\\' separator). sizes: decodes every asset when true (size and md5), else leaves them out.
        public JObj ExtractIndex(bool sizes, CancellationToken cancel = default)
        {
            var list = new List<object>(assets.Count);
            foreach (var a in assets)
            {
                var e = new JObj().Set("name", a.name).Set("type", a.type).Set("package", a.package).Set("offset", a.AbsOffset).Set("stored_size", a.size)
                    .Set("compressed", a.compressed).Set("file", a.file.Replace('/', '\\'));
                if (sizes) { var d = Read(a, cancel); e.Set("size", (long)d.Length).Set("md5", UxBin.Md5(d)); }
                list.Add(e);
            }
            return new JObj().Set("level", name).Set("packages", packages).Set("assets", list);
        }
    }

    /// The player's Untamed source opened for the converter: the disc image (.iso) or the extracted folder (the one holding Database and AI).
    public sealed class UxDisc : IDisposable
    {
        public readonly Volume volume;
        public readonly string directory, root;

        public UxDisc(Volume volume, string directory, string root) { this.volume = volume; this.directory = directory; this.root = root; }

        /// Opens a disc image (a file) or an extracted folder and finds the folder of the level files and the one that holds AI.
        public static UxDisc Open(string path)
        {
            Volume v;
            if (File.Exists(path)) v = new DiscVolume(path) { displayName = Path.GetFileName(path) };
            else
            {
                UxBin.Require(Directory.Exists(path), "Not found: " + path);
                v = new FolderVolume(path, 3, n => n.IndexOf(".xenon.", StringComparison.OrdinalIgnoreCase) >= 0 || n.EndsWith(".tdf07", StringComparison.OrdinalIgnoreCase)
                    || string.Equals(n, "default.xex", StringComparison.OrdinalIgnoreCase)) { displayName = Path.GetFileName(path.TrimEnd('/', '\\')) };
            }
            try
            {
                string dir = null;
                foreach (var f in v.files)
                    if (f.Name.EndsWith("_STADIUM.xenon.database", StringComparison.OrdinalIgnoreCase)) { dir = f.Directory; break; }
                UxBin.Require(dir != null, v.displayName + ": no MX vs ATV Untamed levels found (choose the disc image, or the folder that holds Database and AI)");
                // the AI folder sits beside Database
                string parent = dir.LastIndexOf('/') < 0 ? "" : dir.Substring(0, dir.LastIndexOf('/'));
                string root = null;
                foreach (var f in v.files)
                    if (f.Name.EndsWith(".tdf07", StringComparison.OrdinalIgnoreCase) && string.Equals(f.Directory, parent.Length == 0 ? "AI" : parent + "/AI", StringComparison.OrdinalIgnoreCase)) { root = parent; break; }
                return new UxDisc(v, dir, root);
            }
            catch { v.Dispose(); throw; }
        }

        public UxDiscLevel Level(UxLevelRow lv, CancellationToken cancel = default) => new UxDiscLevel(volume, directory, root, lv, cancel);
        public UxDiscLevel Level(string name, CancellationToken cancel = default) => Level(UxCommon.Level(name), cancel);
        public bool Has(UxLevelRow lv) => volume.Find((directory.Length == 0 ? "" : directory + "/") + lv.disc + ".xenon.database") != null;
        public void Dispose() { volume.Dispose(); }
    }
}
