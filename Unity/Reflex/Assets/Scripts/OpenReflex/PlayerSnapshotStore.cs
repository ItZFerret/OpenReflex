using System;
using System.IO;
using System.Text;
using System.Security.Cryptography;

namespace Reflex.Port.RxCore
{
    public enum PlayerProfileMode { Career = 0, Unlocked = 1 }
    public sealed class PlayerSnapshotLease
    {
        public PlayerProfileMode Mode { get; }
        public string Path { get; }
        public ulong Generation { get; }
        public string Digest { get; }
        public PlayerSnapshotDocument Document { get; }
        internal PlayerSnapshotLease(PlayerProfileMode mode, string path, ulong generation, string digest, PlayerSnapshotDocument document)
        { Mode = mode; Path = path; Generation = generation; Digest = digest; Document = document; }
    }
    // One durable authority per mode. No native session or Unity API is needed to
    // read an existing snapshot. Native schema/application validation is external.
    public sealed class PlayerSnapshotStore
    {
        const int HeaderSize = 88;
        static readonly object gate = new object();
        readonly string directory;
        readonly Action<string> faultPoint;
        public PlayerSnapshotStore(string directory) : this(directory, null) { }
        internal PlayerSnapshotStore(string directory, Action<string> faultPoint)
        { this.directory = System.IO.Path.GetFullPath(directory ?? throw new ArgumentNullException(nameof(directory))); this.faultPoint = faultPoint; }
        public string FilePath(PlayerProfileMode mode)
        {
            if (mode != PlayerProfileMode.Career && mode != PlayerProfileMode.Unlocked) throw new ArgumentOutOfRangeException(nameof(mode));
            return System.IO.Path.Combine(directory, mode == PlayerProfileMode.Career ? "core-player-career-v2.bin" : "core-player-unlocked-v2.bin");
        }
        public bool TryRead(PlayerProfileMode mode, out PlayerSnapshotLease lease)
        {
            lock (gate) { string path = FilePath(mode); if (!File.Exists(path)) { lease = null; return false; } lease = Read(path, mode); return true; }
        }
        public PlayerSnapshotLease OpenExisting(PlayerProfileMode mode)
        { if (!TryRead(mode, out var value)) throw new FileNotFoundException("Player snapshot does not exist.", FilePath(mode)); return value; }
        public PlayerSnapshotLease Read(PlayerProfileMode mode) => OpenExisting(mode);
        // The caller must first native-import/re-export this migration/default seed.
        // This method never replaces a file which appeared during that validation.
        public PlayerSnapshotLease CreateIfAbsent(PlayerProfileMode mode, PlayerSnapshotDocument validatedDocument)
        { if (validatedDocument == null) throw new ArgumentNullException(nameof(validatedDocument)); return Commit(new PlayerSnapshotLease(mode, FilePath(mode), 0, null, validatedDocument), validatedDocument); }
        public PlayerSnapshotLease Commit(PlayerSnapshotLease lease, PlayerSnapshotDocument document)
        {
            if (lease == null || document == null) throw new ArgumentNullException();
            string path = FilePath(lease.Mode);
            if (!string.Equals(path, lease.Path, StringComparison.Ordinal)) throw new InvalidOperationException("Lease belongs to another profile store.");
            lease.Document.RequireSameSchema(document);
            byte[] blob = document.Encode();
            lock (gate)
            {
                Directory.CreateDirectory(directory);
                // FileShare.None also serializes cooperating processes. No game/global
                // lease is acquired, and this file lock never changes mode selection.
                using (var fileLock = new FileStream(path + ".lock", FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
                {
                    PlayerSnapshotLease current = File.Exists(path) ? Read(path, lease.Mode) : null;
                    if (current == null ? lease.Generation != 0 || lease.Digest != null : current.Generation != lease.Generation || current.Digest != lease.Digest)
                        throw new InvalidOperationException("Player profile changed since this lease was captured.");
                    if (lease.Generation == ulong.MaxValue) throw new InvalidOperationException("Player profile generation exhausted.");
                    ulong next = lease.Generation + 1;
                    byte[] envelope = Envelope(lease.Mode, next, document, blob);
                    var published = new PlayerSnapshotLease(lease.Mode, path, next, PlayerSnapshotDocument.Hash(envelope), document);
                    string temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
                    try
                    {
                        faultPoint?.Invoke("before-write");
                        using (var output = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                        { output.Write(envelope, 0, envelope.Length); output.Flush(true); }
                        faultPoint?.Invoke("before-replace");
                        // Decode the actual flushed bytes before publication, including
                        // both digests and all structural/schema bounds.
                        var check = Read(temporary, lease.Mode);
                        if (check.Generation != next || check.Document.SchemaDigest != document.SchemaDigest) throw new IOException("Temporary Player snapshot validation failed.");
                        if (current == null) File.Move(temporary, path); else File.Replace(temporary, path, path + ".bak");
                        return published;
                    }
                    finally { if (File.Exists(temporary)) File.Delete(temporary); }
                }
            }
        }
        static byte[] Hex(string text)
        { var value = new byte[32]; for (int i = 0; i < 32; i++) value[i] = Convert.ToByte(text.Substring(2 * i, 2), 16); return value; }
        static byte[] Envelope(PlayerProfileMode mode, ulong generation, PlayerSnapshotDocument document, byte[] blob)
        {
            using (var stream = new MemoryStream(HeaderSize + blob.Length)) using (var w = new BinaryWriter(stream))
            { w.Write(0x32535052u); w.Write(2u); w.Write((uint)mode); w.Write(generation); w.Write((uint)blob.Length); w.Write(Hex(document.SchemaDigest)); w.Write(new byte[32]); w.Write(blob); w.Flush(); byte[] envelope=stream.ToArray();Buffer.BlockCopy(Checksum(envelope),0,envelope,56,32);return envelope; }
        }
        static PlayerSnapshotLease Read(string path, PlayerProfileMode expected)
        {
            byte[] envelope;
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete))
            {
                if (stream.Length < HeaderSize + 32 || stream.Length > HeaderSize + PlayerSnapshotDocument.MaximumBytes) throw new InvalidDataException("Invalid durable Player file size.");
                envelope = new byte[(int)stream.Length]; int offset = 0;
                while (offset < envelope.Length) { int n = stream.Read(envelope, offset, envelope.Length - offset); if (n == 0) throw new EndOfStreamException(); offset += n; }
            }
            using (var r = new BinaryReader(new MemoryStream(envelope, false)))
            {
                if (r.ReadUInt32() != 0x32535052 || r.ReadUInt32() != 2 || r.ReadUInt32() != (uint)expected) throw new InvalidDataException("Unsupported Player envelope or wrong mode.");
                ulong generation = r.ReadUInt64(); uint count = r.ReadUInt32(); byte[] schema = r.ReadBytes(32), hash = r.ReadBytes(32);
                if (generation == 0 || count != envelope.Length - HeaderSize) throw new InvalidDataException("Invalid Player generation/length.");
                byte[] blob = r.ReadBytes((int)count); var document = PlayerSnapshotDocument.Decode(blob);
                if (!Equal(schema, Hex(document.SchemaDigest)) || !Equal(hash, Checksum(envelope))) throw new InvalidDataException("Player snapshot digest mismatch.");
                return new PlayerSnapshotLease(expected, System.IO.Path.GetFullPath(path), generation, PlayerSnapshotDocument.Hash(envelope), document);
            }
        }
        static byte[] Checksum(byte[] envelope)
        { using(var sha=SHA256.Create()){sha.TransformBlock(envelope,0,56,envelope,0);sha.TransformFinalBlock(envelope,HeaderSize,envelope.Length-HeaderSize);return sha.Hash;} }
        static bool Equal(byte[] a, byte[] b) { if (a.Length != b.Length) return false; int difference = 0; for (int i = 0; i < a.Length; i++) difference |= a[i] ^ b[i]; return difference == 0; }
    }
}
