// RxTrackWriter.cs - a dependency-free writer of Reflex track archives (.rxtrack version 5: an RXAR container of kind TRAK), for track
// builders written in C# (.NET or Unity, C# 7.3+, no packages, no native library). The format is core/plan/RXARCHIVE.md; the Python
// reference is tools/rxtrack/rxtrack.py (same output: every file STORED, a SHA-256 on every entry, the same bytes stored once, the index
// as a zstd frame of raw blocks). The game reads it like any .rxtrack; check one with `rxpack verify <file>` or `rxtrack.py verify <file>`.
//
//   RxTrackWriter.PackFolder(@"C:\Tracks\my-track", @"C:\Out\my-track.rxtrack", name: "My Track");     // a track folder as the game reads it
//
//   // or from memory (a builder that never writes the loose files):
//   RxTrackWriter.Write(outPath, new List<KeyValuePair<string, byte[]>> {
//       Pair("Database/Beta_Nat_Track_Slot_5.dx9.database", db), ... , Pair("Mod/mod.ini", ini) }, RxTrackWriter.TrackMeta(stem, "My Track", fourFiles));
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

public static class RxTrackWriter
{
    public const uint KindTrack = 0x4B415254;   // "TRAK" little endian
    /// The four files of a track, in the content id's order.
    public static readonly string[] Exts = { ".dx9.level", ".dx9.database", ".dx9.package", ".dx9.scene" };
    static readonly string[] DressNames = { "mod.ini", "mod.json" }, DressExts = { ".png", ".jpg", ".jpeg", ".txt" };

    /// A track folder (<stem>.dx9.database / .level / .package / .scene, plus mod.ini, mod.json, pictures, .txt) into one .rxtrack.
    public static string PackFolder(string folder, string output, string stem = null, string name = null)
    {
        var names = Directory.GetFiles(folder).Select(Path.GetFileName).Where(n => !n.StartsWith(".")).OrderBy(n => n, StringComparer.Ordinal).ToList();
        if (stem == null)
        {
            var stems = names.Where(n => n.EndsWith(".dx9.level", StringComparison.OrdinalIgnoreCase)).Select(n => n.Substring(0, n.Length - 10)).ToList();
            if (stems.Count != 1) throw new InvalidDataException(folder + ": " + stems.Count + " tracks here (pass the stem)");
            stem = stems[0];
        }
        var files = new List<KeyValuePair<string, byte[]>>();
        var four = new byte[Exts.Length][];
        for (int k = 0; k < Exts.Length; k++)
        {
            string real = names.FirstOrDefault(n => string.Equals(n, stem + Exts[k], StringComparison.OrdinalIgnoreCase))
                          ?? throw new FileNotFoundException(stem + Exts[k] + " is missing (a track is its .dx9.database, .level, .package and .scene)");
            four[k] = File.ReadAllBytes(Path.Combine(folder, real));
            files.Add(new KeyValuePair<string, byte[]>("Database/" + stem + Exts[k], four[k]));
        }
        foreach (string n in names)
        {
            string lower = n.ToLowerInvariant();
            if (DressNames.Contains(lower) || DressExts.Any(e => lower.EndsWith(e)))
                files.Add(new KeyValuePair<string, byte[]>("Mod/" + n, File.ReadAllBytes(Path.Combine(folder, n))));
        }
        Write(output, files, TrackMeta(stem, name ?? Path.GetFileName(Path.GetFullPath(folder).TrimEnd('/', '\\')), four));
        return stem;
    }

    /// The archive's metadata (UTF-8 JSON): format, level (the stem), display name, source "mod", layout "packed", the four files listed.
    public static string TrackMeta(string stem, string name, byte[][] fourInExtsOrder)
    {
        var sb = new StringBuilder();
        sb.Append("{\"format\":\"reflex-track-archive\",\"version\":1,\"level\":").Append(Json(stem)).Append(",\"name\":").Append(Json(name))
          .Append(",\"source\":\"mod\",\"layout\":\"packed\",\"created\":").Append(Json(DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"))).Append(",\"files\":[");
        int[] order = { 1, 0, 3, 2 };   // database, level, scene, package (as the game's own tools list them)
        for (int i = 0; i < order.Length; i++)
        {
            byte[] d = fourInExtsOrder[order[i]];
            if (i > 0) sb.Append(',');
            sb.Append("{\"name\":").Append(Json(stem + Exts[order[i]])).Append(",\"bytes\":").Append(d.Length).Append(",\"sha256\":\"").Append(Hex(Sha256(d))).Append("\"}");
        }
        return sb.Append("]}").ToString();
    }

    /// Writes the archive: data region, index, then the header LAST (to <output>.partial, renamed when complete).
    public static void Write(string output, IList<KeyValuePair<string, byte[]>> files, string metaJson, uint kind = KindTrack)
    {
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var f in files)
        {
            if (!PathOk(f.Key)) throw new ArgumentException("bad archive path: " + f.Key);
            if (!seen.Add(f.Key)) throw new ArgumentException("two files named " + f.Key + " (paths are case-insensitive)");
        }
        string tmp = output + ".partial";
        var entries = new List<(byte[] path, ulong size, ulong offset, ulong stored, uint crc, byte[] sha)>();
        using (var fs = new FileStream(tmp, FileMode.Create, FileAccess.Write))
        {
            fs.Write(new byte[64], 0, 64);
            var storedOnce = new Dictionary<string, ulong>();            // sha256 hex -> offset: the same bytes are stored once
            foreach (var f in files)
            {
                byte[] sha = Sha256(f.Value);
                string key = Hex(sha);
                if (!storedOnce.TryGetValue(key, out ulong offset))
                {
                    offset = (ulong)fs.Position;
                    fs.Write(f.Value, 0, f.Value.Length);                // codec 0: stored
                    storedOnce[key] = offset;
                }
                entries.Add((Encoding.UTF8.GetBytes(f.Key), (ulong)f.Value.Length, offset, (ulong)f.Value.Length, Crc32(f.Value), sha));
            }
            ulong indexOffset = (ulong)fs.Position;
            byte[] meta = Encoding.UTF8.GetBytes(metaJson ?? "");
            var raw = new MemoryStream();
            var w = new BinaryWriter(raw);                                // BinaryWriter is little endian
            w.Write(Encoding.ASCII.GetBytes("RXIX")); w.Write((uint)entries.Count); w.Write((uint)meta.Length); w.Write(meta);
            foreach (var e in entries.OrderBy(e => e.path, ByteOrder.Instance))   // sorted by path bytes
            {
                w.Write((ushort)e.path.Length); w.Write(e.path);
                w.Write((byte)0); w.Write((byte)0); w.Write((ushort)1);   // codec 0 (stored), chunk_log2 0, flags bit 0: SHA-256 follows
                w.Write(e.size); w.Write(e.offset); w.Write(e.stored); w.Write(e.crc);
                w.Write(e.sha);
            }
            w.Flush();
            byte[] rawIndex = raw.ToArray(), packed = ZstdRawFrame(rawIndex);
            fs.Write(packed, 0, packed.Length);

            var header = new MemoryStream(64);
            var h = new BinaryWriter(header);
            h.Write(Encoding.ASCII.GetBytes("RXAR")); h.Write(2u); h.Write(kind); h.Write(0u);   // container version 2, kind, flags 0
            h.Write(indexOffset); h.Write((ulong)packed.Length); h.Write((ulong)rawIndex.Length);
            h.Write((uint)entries.Count); h.Write(Crc32(rawIndex)); h.Write(new byte[12]);
            h.Flush();
            byte[] head = header.ToArray();
            byte[] full = new byte[64];
            Buffer.BlockCopy(head, 0, full, 0, 60);
            BitConverter.GetBytes(Crc32(full, 0, 60)).CopyTo(full, 60);    // CRC-32 of header bytes 0..59 (BitConverter: little endian on every platform the game runs on)
            fs.Position = 0; fs.Write(full, 0, 64);
            fs.Flush(true);
        }
        if (File.Exists(output)) File.Delete(output);
        File.Move(tmp, output);
    }

    /// A valid zstd frame holding data uncompressed: magic, descriptor 0xE0 (single segment, 8-byte content size), raw blocks <= 128 KiB.
    public static byte[] ZstdRawFrame(byte[] data)
    {
        var o = new MemoryStream();
        o.Write(new byte[] { 0x28, 0xB5, 0x2F, 0xFD, 0xE0 }, 0, 5);
        o.Write(BitConverter.GetBytes((ulong)data.Length), 0, 8);
        if (data.Length == 0) o.Write(new byte[] { 1, 0, 0 }, 0, 3);
        for (int at = 0; at < data.Length; at += 128 * 1024)
        {
            int n = Math.Min(128 * 1024, data.Length - at);
            uint bh = ((uint)n << 3) | (at + n >= data.Length ? 1u : 0u);   // bit 0 last, bits 1-2 type 0 (raw), bits 3.. size
            o.WriteByte((byte)bh); o.WriteByte((byte)(bh >> 8)); o.WriteByte((byte)(bh >> 16));
            o.Write(data, at, n);
        }
        return o.ToArray();
    }

    public static bool PathOk(string p)
    {
        byte[] b = Encoding.UTF8.GetBytes(p ?? "");
        if (b.Length == 0 || b.Length > 1023 || p.StartsWith("/") || p.EndsWith("/")) return false;
        if (b.Any(c => c < 0x20 || c == 0x7F || c == (byte)'\\' || c == (byte)':')) return false;
        return p.Split('/').All(s => s.Length > 0 && s != "." && s != "..");
    }

    // ---- small helpers (no packages)
    static readonly uint[] CrcTable = Enumerable.Range(0, 256).Select(i => { uint c = (uint)i; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; return c; }).ToArray();
    public static uint Crc32(byte[] d) => Crc32(d, 0, d.Length);
    public static uint Crc32(byte[] d, int start, int n) { uint c = 0xFFFFFFFFu; for (int i = start; i < start + n; i++) c = CrcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8); return c ^ 0xFFFFFFFFu; }
    static byte[] Sha256(byte[] d) { using (var s = SHA256.Create()) return s.ComputeHash(d); }
    static string Hex(byte[] b) { var sb = new StringBuilder(b.Length * 2); foreach (byte x in b) sb.Append(x.ToString("x2")); return sb.ToString(); }
    static string Json(string s)
    {
        var sb = new StringBuilder("\"");
        foreach (char c in s ?? "")
            if (c == '"' || c == '\\') sb.Append('\\').Append(c);
            else if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4"));
            else sb.Append(c);
        return sb.Append('"').ToString();
    }
    sealed class ByteOrder : IComparer<byte[]>
    {
        public static readonly ByteOrder Instance = new ByteOrder();
        public int Compare(byte[] a, byte[] b) { int n = Math.Min(a.Length, b.Length); for (int i = 0; i < n; i++) if (a[i] != b[i]) return a[i] - b[i]; return a.Length - b.Length; }
    }
}
