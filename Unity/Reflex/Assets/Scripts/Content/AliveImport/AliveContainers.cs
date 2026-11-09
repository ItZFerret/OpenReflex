// The player's MX vs ATV Alive sources, read in place and never modified: the disc image (XDVDFS, _re/formats/alive_xdvdfs.py), LIVE /
// PIRS / CON packages (STFS with the full SHA-1 hash tree, alive_stfs.py), extracted folders, and the XMem LZX resource wrapper of the
// package heaps (package_extract.decode_resource, decoded with the client's LzxDecoder).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content.AliveImport
{
    /// One file of a source volume ('/'-separated path inside the volume).
    public sealed class VolumeFile
    {
        public string path; public long size;
        internal long sector;                 // XDVDFS
        internal int start, blocks; internal bool consecutive;   // STFS
        public string Name => path.Substring(path.LastIndexOf('/') + 1);
        public string Directory => path.LastIndexOf('/') < 0 ? "" : path.Substring(0, path.LastIndexOf('/'));
    }

    /// A read-only set of files: a disc image, a LIVE package or a folder.
    public abstract class Volume : IDisposable
    {
        public string origin, kind, displayName = "";
        public readonly List<VolumeFile> files = new List<VolumeFile>();
        public abstract byte[] Read(VolumeFile f, CancellationToken cancel);
        /// `count` bytes at `offset` of the file (package heap resources).
        public abstract byte[] ReadRange(VolumeFile f, long offset, int count, CancellationToken cancel);
        public virtual void Dispose() { }
        public VolumeFile Find(string path) => files.FirstOrDefault(f => string.Equals(f.path, path, StringComparison.OrdinalIgnoreCase));
    }

    /// An extracted folder (the disc's files, or a LIVE package's files).
    public sealed class FolderVolume : Volume
    {
        readonly string _root;
        public FolderVolume(string root, int maxDepth = 3)
        {
            _root = Path.GetFullPath(root); origin = _root; kind = "folder";
            Walk(_root, "", maxDepth);
        }
        void Walk(string dir, string prefix, int depth)
        {
            string[] entries, dirs;
            try { entries = System.IO.Directory.GetFiles(dir); dirs = depth > 0 ? System.IO.Directory.GetDirectories(dir) : Array.Empty<string>(); }
            catch (UnauthorizedAccessException) { return; }
            catch (IOException) { return; }
            foreach (var p in entries.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                var name = Path.GetFileName(p);
                if (name.IndexOf(".xenon.", StringComparison.OrdinalIgnoreCase) < 0) continue;
                files.Add(new VolumeFile { path = prefix + name, size = new FileInfo(p).Length });
            }
            foreach (var d in dirs.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                if (Path.GetFileName(d).StartsWith(".", StringComparison.Ordinal) || ReflexFs.IsLink(d)) continue;
                Walk(d, prefix + Path.GetFileName(d) + "/", depth - 1);
            }
        }
        public override byte[] Read(VolumeFile f, CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();
            var b = File.ReadAllBytes(Path.Combine(_root, f.path.Replace('/', Path.DirectorySeparatorChar)));
            Bin.Require(b.Length == f.size, f.path + " changed while importing");
            return b;
        }
        public override byte[] ReadRange(VolumeFile f, long offset, int count, CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();
            Bin.Require(offset >= 0 && count >= 0 && offset + count <= f.size, f.path + ": range outside the file");
            using (var s = new FileStream(Path.Combine(_root, f.path.Replace('/', Path.DirectorySeparatorChar)), FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                Bin.Require(s.Length == f.size, f.path + " changed while importing");
                var b = new byte[count]; s.Position = offset;
                int got = 0; while (got < count) { int n = s.Read(b, got, count - got); if (n <= 0) break; got += n; }
                Bin.Require(got == count, f.path + ": truncated");
                return b;
            }
        }
    }

    /// An Xbox 360 game disc image (XGD2 / XGD3 / XGD1, or a bare game partition).
    public sealed class DiscVolume : Volume
    {
        const int Sector = 2048;
        static readonly (string, long)[] Partitions = { ("XGD2", 0x0FD90000), ("XGD3", 0x02080000), ("XGD1", 0x18300000), ("game partition", 0) };
        readonly FileStream _file; readonly long _base, _size; readonly object _gate = new object();
        public string layout;

        public DiscVolume(string path)
        {
            origin = Path.GetFullPath(path); kind = "disc image";
            _file = new FileStream(origin, FileMode.Open, FileAccess.Read, FileShare.Read);
            _size = _file.Length; _base = -1;
            byte[] head = null;
            foreach (var (name, b) in Partitions)
            {
                if (b + 33L * Sector > _size) continue;
                head = ReadAt(b + 32L * Sector, Sector);
                if (Bin.StartsWith(head, "MICROSOFT*XBOX*MEDIA") && Bin.StartsWith(head, "MICROSOFT*XBOX*MEDIA", 0x7EC)) { layout = name; _base = b; break; }
            }
            if (_base < 0) { _file.Dispose(); throw new AliveImportException("Not an Xbox 360 game disc image (no XDVDFS volume descriptor)"); }
            uint rootSector = Bin.U32LE(head, 20), rootSize = Bin.U32LE(head, 24);
            Bin.Require(rootSize > 0 && _base + rootSector * (long)Sector + rootSize <= _size, "Root directory outside the image");
            WalkDir(rootSector, rootSize, "", 0);
        }

        byte[] ReadAt(long offset, int size)
        {
            lock (_gate)
            {
                var b = new byte[size];
                _file.Position = offset;
                int got = 0; while (got < size) { int n = _file.Read(b, got, size - got); if (n <= 0) break; got += n; }
                Bin.Require(got == size, "Read past the end of the image");
                return b;
            }
        }

        void WalkDir(long sector, long size, string prefix, int depth)
        {
            Bin.Require(depth < 64, "Directory nesting");
            if (size == 0) return;
            Bin.Require(size <= 64 * 1024 * 1024, "Directory too large");
            var data = ReadAt(_base + sector * Sector, (int)size);
            var entries = new List<(string name, uint first, uint length, bool dir)>();
            var seen = new HashSet<int>(); var stack = new Stack<int>(); stack.Push(0);
            while (stack.Count > 0)
            {
                int offset = stack.Pop();
                Bin.Require(seen.Add(offset), "Directory tree cycle");
                Bin.Require(offset + 14 <= data.Length, "Directory entry outside its directory");
                int left = Bin.U16LE(data, offset), right = Bin.U16LE(data, offset + 2);
                uint first = Bin.U32LE(data, offset + 4), length = Bin.U32LE(data, offset + 8);
                int attr = data[offset + 12], namelen = data[offset + 13];
                if (left == 0xFFFF && right == 0xFFFF) continue;
                Bin.Require(namelen > 0 && offset + 14 + namelen <= data.Length, "Directory entry name");
                Bin.Require(offset % Sector + 14 + namelen <= Sector, "Directory entry crosses a sector");
                string name = Encoding.ASCII.GetString(data, offset + 14, namelen);
                Bin.Require(name != "." && name != ".." && !name.Contains("/") && !name.Contains("\\") && !name.Contains("\0"), "Unsafe file name " + name);
                entries.Add((name, first, length, (attr & 0x10) != 0));
                if (right != 0) stack.Push(right * 4);
                if (left != 0) stack.Push(left * 4);
            }
            foreach (var e in entries.OrderBy(e => e.name.ToLowerInvariant(), StringComparer.Ordinal))
            {
                string path = prefix + e.name;
                if (e.dir) WalkDir(e.first, e.length, path + "/", depth + 1);
                else
                {
                    Bin.Require(_base + e.first * (long)Sector + e.length <= _size, path + " outside the image");
                    files.Add(new VolumeFile { path = path, size = e.length, sector = e.first });
                }
            }
        }

        public override byte[] Read(VolumeFile f, CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();
            Bin.Require(f.size <= int.MaxValue - 64, f.path + " is too large");
            var o = new byte[f.size];
            long at = _base + f.sector * Sector; int done = 0;
            while (done < o.Length)
            {
                cancel.ThrowIfCancellationRequested();
                int n = Math.Min(8 << 20, o.Length - done);
                var chunk = ReadAt(at + done, n); Buffer.BlockCopy(chunk, 0, o, done, n); done += n;
            }
            return o;
        }

        public override byte[] ReadRange(VolumeFile f, long offset, int count, CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();
            Bin.Require(offset >= 0 && count >= 0 && offset + count <= f.size, f.path + ": range outside the file");
            return ReadAt(_base + f.sector * Sector + offset, count);
        }

        public override void Dispose() { _file.Dispose(); }
    }

    /// An Xbox 360 STFS content package (LIVE / PIRS / CON): the hash tree is validated from the volume descriptor's top hash when the
    /// package is opened, and every data block read against its level-0 SHA-1 entry. The RSA signature is not checked.
    public sealed class StfsVolume : Volume
    {
        const int Block = 4096, PerTable = 170, PerL1 = PerTable * PerTable;
        readonly FileStream _file; readonly object _gate = new object();
        readonly int _sex, _levels; readonly int[] _steps; readonly long _base; readonly int _count, _tableBlocks, _tableStart; readonly byte[] _topHash;
        readonly Dictionary<(int, int), byte[]> _tables = new Dictionary<(int, int), byte[]>();
        public uint titleId, contentType; public string magic;
        public int VerifiedBlocks { get; private set; }

        public StfsVolume(string path)
        {
            origin = Path.GetFullPath(path); kind = "LIVE package";
            _file = new FileStream(origin, FileMode.Open, FileAccess.Read, FileShare.Read);
            try
            {
                Bin.Require(_file.Length >= 0x971a, "Not an STFS package");
                var raw = ReadAt(0, 0x971a);
                magic = Encoding.ASCII.GetString(raw, 0, 4);
                Bin.Require(magic == "LIVE" || magic == "PIRS" || magic == "CON ", "Not an STFS package");
                magic = magic.Trim();
                uint headerSize = Bin.U32BE(raw, 0x340);
                contentType = Bin.U32BE(raw, 0x344); titleId = Bin.U32BE(raw, 0x360);
                Bin.Require(Bin.U32BE(raw, 0x3a9) == 0, "Not an STFS volume (SVOD packages are not supported)");
                Bin.Require(raw[0x379] == 0x24, "STFS volume descriptor size");
                int separation = raw[0x37b];
                _sex = (separation & 1) != 0 ? 0 : 1;
                _steps = _sex == 0 ? new[] { 0xAB, 0x718F } : new[] { 0xAC, 0x723A };
                _tableBlocks = raw[0x37c] | raw[0x37d] << 8;
                _tableStart = raw[0x37e] | raw[0x37f] << 8 | raw[0x380] << 16;
                _topHash = Bin.Slice(raw, 0x381, 20);
                _count = (int)Bin.U32BE(raw, 0x395);
                Bin.Require(_count > 0 && _count <= PerL1 * PerTable, "Allocated block count");
                _base = (headerSize + 0xFFFL) & ~0xFFFL;
                _levels = _count <= PerTable ? 0 : _count <= PerL1 ? 1 : 2;
                var dn = Encoding.BigEndianUnicode.GetString(raw, 0x411, 0x100); int z = dn.IndexOf('\0');
                displayName = z >= 0 ? dn.Substring(0, z) : dn;
                ValidateTree();
                ReadFileTable();
            }
            catch { _file.Dispose(); throw; }
        }

        byte[] ReadAt(long offset, int size)
        {
            lock (_gate)
            {
                var b = new byte[size];
                _file.Position = offset;
                int got = 0; while (got < size) { int n = _file.Read(b, got, size - got); if (n <= 0) break; got += n; }
                Bin.Require(got == size, "Read past the end of the package");
                return b;
            }
        }

        long DataBacking(long n)
        {
            int shift = _sex;
            long result = (((n + PerTable) / PerTable) << shift) + n;
            if (n < PerTable) return result;
            if (n < PerL1) return result + (((n + PerL1) / PerL1) << shift);
            return (1L << shift) + result + (((n + PerL1) / PerL1) << shift);
        }

        long TableBacking(int level, long n)
        {
            int shift = _sex;
            if (level == 0)
            {
                if (n < PerTable) return 0;
                long num = (n / PerTable) * _steps[0] + (((n / PerL1) + 1) << shift);
                return n / PerL1 == 0 ? num : num + (1L << shift);
            }
            if (level == 1)
            {
                if (n < PerL1) return _steps[0];
                return (1L << shift) + (n / PerL1) * _steps[1];
            }
            return _steps[1];
        }

        byte[] ReadBacking(long backing, int copy = 0)
        {
            long pos = _base + (backing + copy) * Block;
            Bin.Require(pos + Block <= _file.Length, $"Block {backing} outside the package");
            return ReadAt(pos, Block);
        }

        static bool Same(byte[] a, byte[] b, int bAt) { for (int i = 0; i < 20; i++) if (a[i] != b[bAt + i]) return false; return true; }

        byte[] Table(int level, long n, byte[] expected, int expectedAt)
        {
            long span = 1; for (int i = 0; i <= level; i++) span *= PerTable;
            var key = (level, (int)(n / span));
            if (_tables.TryGetValue(key, out var t)) return t;
            long backing = TableBacking(level, n);
            using (var sha = SHA1.Create())
                for (int copy = 0; copy < 1 + _sex; copy++)
                {
                    var data = ReadBacking(backing, copy);
                    if (Same(sha.ComputeHash(data), expected, expectedAt)) { _tables[key] = data; return data; }
                }
            throw new AliveImportException($"Hash table mismatch (level {level}, block {n}): the package is damaged or not an original download");
        }

        void ValidateTree()
        {
            var top = Table(_levels, 0, _topHash, 0);
            void Walk(int level, byte[] table, long first)
            {
                long span = 1; for (int i = 0; i < level; i++) span *= PerTable;
                for (int i = 0; i < PerTable; i++)
                {
                    long n = first + i * span;
                    if (n >= _count) break;
                    if (level == 0) continue;
                    var child = Table(level - 1, n, table, i * 24);
                    Walk(level - 1, child, n);
                }
            }
            Walk(_levels, top, 0);
        }

        (byte[] data, int next) BlockAt(int n, SHA1 sha)
        {
            Bin.Require(n >= 0 && n < _count, $"Block {n} outside the allocated blocks");
            var table = _tables[(0, n / PerTable)];
            int e = (n % PerTable) * 24;
            var data = ReadBacking(DataBacking(n));
            Bin.Require(Same(sha.ComputeHash(data), table, e), $"Block hash mismatch: {n} (the package is damaged or not an original download)");
            VerifiedBlocks++;
            return (data, table[e + 21] << 16 | table[e + 22] << 8 | table[e + 23]);
        }

        void ReadFileTable()
        {
            var table = new MemoryStream(); int n = _tableStart;
            using (var sha = SHA1.Create())
                for (int i = 0; i < _tableBlocks; i++) { var (d, next) = BlockAt(n, sha); table.Write(d, 0, d.Length); n = next; }
            var t = table.ToArray();
            var records = new List<(int index, string name, bool dir, bool consecutive, int blocks, int start, int parent, long size)>();
            for (int i = 0; i < t.Length / 64; i++)
            {
                int e = i * 64, length = t[e + 40] & 0x3F;
                if (length == 0) continue;
                string name = Encoding.ASCII.GetString(t, e, length);
                Bin.Require(name != "." && name != ".." && !name.Contains("/") && !name.Contains("\\") && !name.Contains("\0"), "Unsafe file name " + name);
                records.Add((i, name, (t[e + 40] & 0x80) != 0, (t[e + 40] & 0x40) != 0, t[e + 41] | t[e + 42] << 8 | t[e + 43] << 16,
                             t[e + 47] | t[e + 48] << 8 | t[e + 49] << 16, t[e + 50] << 8 | t[e + 51], Bin.U32BE(t, e + 52)));
            }
            var byIndex = records.ToDictionary(r => r.index);
            string PathOf((int index, string name, bool dir, bool consecutive, int blocks, int start, int parent, long size) r, int depth)
            {
                Bin.Require(depth < 64, "Directory nesting");
                if (r.parent == 0xFFFF) return r.name;
                Bin.Require(byIndex.TryGetValue(r.parent, out var p) && p.dir, "File entry with an unknown parent directory");
                return PathOf(p, depth + 1) + "/" + r.name;
            }
            foreach (var r in records)
                if (!r.dir)
                    files.Add(new VolumeFile { path = PathOf(r, 0), size = r.size, start = r.start, blocks = r.blocks, consecutive = r.consecutive });
        }

        readonly Dictionary<VolumeFile, int[]> _chains = new Dictionary<VolumeFile, int[]>();

        /// The file's data blocks in order (the chain follows the level-0 hash entries, already validated).
        int[] Chain(VolumeFile f)
        {
            lock (_chains)
            {
                if (_chains.TryGetValue(f, out var c)) return c;
                Bin.Require(f.blocks == (f.size + Block - 1) / Block, f.path + ": allocated block count");
                c = new int[f.blocks]; var seen = new HashSet<int>(); int n = f.start;
                for (int i = 0; i < f.blocks; i++)
                {
                    Bin.Require(n >= 0 && n < _count, $"Block {n} outside the allocated blocks");
                    Bin.Require(seen.Add(n), f.path + ": block chain cycle");
                    c[i] = n;
                    var table = _tables[(0, n / PerTable)]; int e = (n % PerTable) * 24;
                    n = f.consecutive ? n + 1 : table[e + 21] << 16 | table[e + 22] << 8 | table[e + 23];
                }
                return _chains[f] = c;
            }
        }

        public override byte[] Read(VolumeFile f, CancellationToken cancel)
        {
            Bin.Require(f.size <= int.MaxValue - 64, f.path + " is too large");
            return ReadRange(f, 0, (int)f.size, cancel);
        }

        public override byte[] ReadRange(VolumeFile f, long offset, int count, CancellationToken cancel)
        {
            Bin.Require(offset >= 0 && count >= 0 && offset + count <= f.size, f.path + ": range outside the file");
            var chain = Chain(f);
            var o = new byte[count];
            if (count == 0) return o;
            int first = (int)(offset / Block), last = (int)((offset + count - 1) / Block);
            using (var sha = SHA1.Create())
                for (int i = first; i <= last; i++)
                {
                    if (((i - first) & 255) == 0) cancel.ThrowIfCancellationRequested();
                    var (d, _) = BlockAt(chain[i], sha);
                    long blockStart = (long)i * Block;
                    long from = Math.Max(offset, blockStart), to = Math.Min(offset + count, blockStart + Block);
                    Buffer.BlockCopy(d, (int)(from - blockStart), o, (int)(from - offset), (int)(to - from));
                }
            return o;
        }

        public override void Dispose() { _file.Dispose(); }
    }

    /// XMem LZX resource wrapper (package_extract.decode_resource / split_frames), strict as the reference.
    internal static class XMem
    {
        const int MaxResource = 64 * 1024 * 1024;

        public static uint MetadataUint(string value)
        {
            Bin.Require(value != null && value.StartsWith("_uint:", StringComparison.Ordinal), "Expected typed unsigned metadata");
            Bin.Require(long.TryParse(value.Substring(6), System.Globalization.NumberStyles.AllowLeadingSign, System.Globalization.CultureInfo.InvariantCulture, out long n), "Invalid unsigned metadata");
            Bin.Require(n >= 0 && n <= 0xFFFFFFFFL, "Unsigned metadata out of range");
            return (uint)n;
        }

        public static byte[] Decode(byte[] raw, CancellationToken cancel)
        {
            Bin.Require(raw.Length >= 16 && raw.Length <= MaxResource, "Invalid resource size");
            long regionEnd = Bin.U32LE(raw, 0);
            Bin.Require(regionEnd + 4 == raw.Length, "Resource wrapper length differs from heap metadata");
            uint lastLength = Bin.U32LE(raw, (int)regionEnd);
            Bin.Require(lastLength > 0 && lastLength <= 65536, "Invalid final chunk logical length");
            int position = 4; var output = new MemoryStream(); int chunks = 0;
            while (position < regionEnd)
            {
                cancel.ThrowIfCancellationRequested();
                Bin.Require(position + 8 <= regionEnd, "Truncated resource chunk header");
                uint capacity = Bin.U32LE(raw, position), compressed = Bin.U32LE(raw, position + 4);
                position += 8;
                Bin.Require(capacity == 65536, "Unverified resource chunk allocation size");
                Bin.Require(compressed > 0 && position + (long)compressed <= regionEnd, "Invalid compressed chunk size");
                int start = position, end = position + (int)compressed;
                position = end;
                Bin.Require(compressed != capacity, "Unverified raw resource chunk");
                // split_frames
                var frames = new List<(int at, int input, int output)>(); int p = start, decoded = 0;
                while (p < end - 5)
                {
                    int outSize, inSize;
                    if (raw[p] == 0xFF)
                    {
                        Bin.Require(p + 5 <= end, "Truncated long XMem frame header");
                        outSize = Bin.U16BE(raw, p + 1); inSize = Bin.U16BE(raw, p + 3); p += 5;
                    }
                    else
                    {
                        Bin.Require(p + 2 <= end, "Truncated XMem frame header");
                        inSize = Bin.U16BE(raw, p); outSize = 32768; p += 2;
                    }
                    Bin.Require(outSize > 0 && outSize <= 32768 && inSize > 0, "Invalid XMem frame sizes");
                    Bin.Require(p + inSize <= end - 5, "Truncated XMem frame data");
                    frames.Add((p, inSize, outSize));
                    p += inSize; decoded += outSize;
                    Bin.Require(decoded <= 65536, "Unverified chunk larger than 64 KiB");
                }
                bool terminator = frames.Count > 0 && end - p == 5;
                for (int k = p; k < end && terminator; k++) if (raw[k] != 0) terminator = false;
                Bin.Require(terminator, "Expected five-byte XMem terminator");
                long expected = position == regionEnd ? lastLength : capacity;
                Bin.Require(decoded == expected, "XMem length differs from resource chunk length");
                Bin.Require(output.Length + decoded <= MaxResource, "Resource output exceeds limit");
                var decoder = new LzxDecoder(17); long before = output.Length;
                foreach (var (at, input, outSize) in frames)
                    using (var s = new MemoryStream(raw, at, input, false))
                    {
                        int r;
                        try { r = decoder.Decompress(s, input, output, outSize); }
                        catch (Exception ex) when (!(ex is OperationCanceledException)) { throw new AliveImportException("LZX error: " + ex.Message); }
                        Bin.Require(r == 0, "LZX error " + r);
                    }
                Bin.Require(output.Length - before == decoded, "Decoder returned the wrong length");
                chunks++;
            }
            Bin.Require(position == regionEnd && chunks > 0, "Resource chunk boundary mismatch");
            return output.ToArray();
        }
    }
}
