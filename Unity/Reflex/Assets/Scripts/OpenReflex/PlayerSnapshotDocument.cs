using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace Reflex.Port.RxCore
{
    public enum PlayerSnapshotType : uint { Int = 1, Float = 2, Text = 4 }
    public sealed class PlayerSnapshotValue
    {
        readonly byte[] text;
        public PlayerSnapshotType Type { get; }
        public uint Bits { get; }
        public byte[] TextBytes => text == null ? null : (byte[])text.Clone();
        internal byte[] RawText => text;
        PlayerSnapshotValue(PlayerSnapshotType type, uint bits, byte[] bytes) { Type = type; Bits = bits; text = bytes == null ? null : (byte[])bytes.Clone(); }
        public static PlayerSnapshotValue Int(int value) => new PlayerSnapshotValue(PlayerSnapshotType.Int, unchecked((uint)value), null);
        public static PlayerSnapshotValue FloatBits(uint bits) => new PlayerSnapshotValue(PlayerSnapshotType.Float, bits, null);
        public static PlayerSnapshotValue Text(byte[] bytes)
        {
            if (bytes == null || bytes.Length > 1024 * 1024) throw new InvalidDataException("Player text exceeds native bound.");
            return new PlayerSnapshotValue(PlayerSnapshotType.Text, 0, bytes);
        }
        public static PlayerSnapshotValue TextUtf8(string value) => Text(new UTF8Encoding(false, true).GetBytes(value ?? throw new ArgumentNullException(nameof(value))));
    }
    public sealed class PlayerSnapshotColumn
    {
        public string Name { get; }
        public PlayerSnapshotType Type { get; }
        internal byte[] NameBytes { get; }
        internal PlayerSnapshotColumn(string name, PlayerSnapshotType type, byte[] raw) { Name = name; Type = type; NameBytes = (byte[])raw.Clone(); }
    }
    public sealed class PlayerSnapshotTable
    {
        readonly ReadOnlyCollection<PlayerSnapshotColumn> columns;
        readonly PlayerSnapshotValue[][] rows;
        public string Name { get; }
        public IReadOnlyList<PlayerSnapshotColumn> Columns => columns;
        public int RowCount => rows.Length;
        internal byte[] NameBytes { get; }
        internal PlayerSnapshotTable(string name, byte[] raw, IList<PlayerSnapshotColumn> cols, PlayerSnapshotValue[][] values)
        { Name = name; NameBytes = (byte[])raw.Clone(); columns = new List<PlayerSnapshotColumn>(cols).AsReadOnly(); rows = values; }
        public PlayerSnapshotValue Get(int row, string column) => Get(row, ColumnIndex(column));
        public PlayerSnapshotValue Get(int row, int column)
        {
            if (row < 0 || row >= RowCount || column < 0 || column >= columns.Count) throw new ArgumentOutOfRangeException();
            return rows[row][column]; // values are immutable, including text ownership
        }
        internal int ColumnIndex(string name)
        { for (int i = 0; i < columns.Count; i++) if (columns[i].Name == name) return i; throw new InvalidDataException("Unknown Player column " + Name + "." + name); }
        internal PlayerSnapshotValue[][] CopyRows()
        { var result = new PlayerSnapshotValue[rows.Length][]; for (int r = 0; r < rows.Length; r++) result[r] = (PlayerSnapshotValue[])rows[r].Clone(); return result; }
        internal PlayerSnapshotTable WithValues(PlayerSnapshotValue[][] values) => new PlayerSnapshotTable(Name, NameBytes, columns, values);
    }
    // Structural/schema codec only. Canonical native import remains authority for
    // install schema, indexed-column uniqueness and application to the database.
    public sealed class PlayerSnapshotDocument
    {
        public const int MaximumBytes = 64 * 1024 * 1024;
        readonly ReadOnlyCollection<PlayerSnapshotTable> tables;
        public IReadOnlyList<PlayerSnapshotTable> Tables => tables;
        PlayerSnapshotDocument(IList<PlayerSnapshotTable> value) { tables = new List<PlayerSnapshotTable>(value).AsReadOnly(); }
        static uint U(byte[] b, int p) => (uint)(b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24);
        static int Count(uint n, uint max, string label) { if (n > max) throw new InvalidDataException("Invalid " + label); return (int)n; }
        static byte[] Part(byte[] b, int p, int n) { var value = new byte[n]; Buffer.BlockCopy(b, p, value, 0, n); return value; }
        static string Name(byte[] raw, bool table)
        {
            int end = Array.IndexOf(raw, (byte)0); if (end <= 0) throw new InvalidDataException("Unterminated or empty Player name.");
            for (int i = 0; i < end; i++) if (raw[i] < 32 || raw[i] > 126) throw new InvalidDataException("Non-ASCII schema name.");
            string name = Encoding.ASCII.GetString(raw, 0, end);
            if (table && !name.StartsWith("Player", StringComparison.Ordinal)) throw new InvalidDataException("Non-Player table.");
            return name;
        }
        public static PlayerSnapshotDocument Decode(byte[] input)
        {
            if (input == null || input.Length < 32 || input.Length > MaximumBytes) throw new InvalidDataException("Invalid Player snapshot length.");
            byte[] b = (byte[])input.Clone();
            if (U(b, 0) != 0x31535052 || U(b, 4) != 1 || U(b, 8) != b.Length || U(b, 28) != 0) throw new InvalidDataException("Invalid Player snapshot header.");
            int nt = Count(U(b, 12), 100, "table count"), nc = Count(U(b, 16), 102400, "column count"), nv = Count(U(b, 20), 1000000, "cell count");
            uint text = U(b, 24); long co = 32L + nt * 64L, vo = co + nc * 36L, to = vo + nv * 12L;
            if (nt == 0 || to + text != b.Length) throw new InvalidDataException("Invalid Player payload ranges.");
            int columnNext = 0, cellNext = 0; uint textNext = 0; var result = new List<PlayerSnapshotTable>(); var tableNames = new HashSet<string>(StringComparer.Ordinal);
            for (int t = 0; t < nt; t++)
            {
                int tp = 32 + 64 * t; byte[] name = Part(b, tp, 48); string tn = Name(name, true);
                if (!tableNames.Add(tn)) throw new InvalidDataException("Duplicate Player table.");
                int nr = Count(U(b, tp + 48), 65536, "row count"), cols = Count(U(b, tp + 56), 1024, "table columns");
                if (cols == 0 || U(b, tp + 52) != columnNext || U(b, tp + 60) != cellNext || (long)columnNext + cols > nc || (long)cellNext + (long)nr * cols > nv) throw new InvalidDataException("Invalid dense table layout.");
                var columns = new List<PlayerSnapshotColumn>(); var names = new HashSet<string>(StringComparer.Ordinal);
                for (int c = 0; c < cols; c++)
                {
                    int cp = (int)co + (columnNext + c) * 36; byte[] raw = Part(b, cp, 32); string cn = Name(raw, false); uint type = U(b, cp + 32);
                    if (!names.Add(cn) || type != 1 && type != 2 && type != 4) throw new InvalidDataException("Unsupported/duplicate Player column.");
                    columns.Add(new PlayerSnapshotColumn(cn, (PlayerSnapshotType)type, raw));
                }
                var rows = new PlayerSnapshotValue[nr][];
                for (int r = 0; r < nr; r++)
                {
                    rows[r] = new PlayerSnapshotValue[cols];
                    for (int c = 0; c < cols; c++)
                    {
                        int vp = (int)vo + (cellNext++) * 12; uint bits = U(b, vp), offset = U(b, vp + 4), len = U(b, vp + 8);
                        if (columns[c].Type == PlayerSnapshotType.Text)
                        {
                            if (bits != 0 || offset != textNext || len > 1024 * 1024 || (ulong)offset + len > text) throw new InvalidDataException("Invalid Player text range.");
                            rows[r][c] = PlayerSnapshotValue.Text(Part(b, (int)to + (int)offset, (int)len)); textNext += len;
                        }
                        else
                        {
                            if (offset != 0 || len != 0) throw new InvalidDataException("Numeric Player value contains text.");
                            rows[r][c] = columns[c].Type == PlayerSnapshotType.Int ? PlayerSnapshotValue.Int(unchecked((int)bits)) : PlayerSnapshotValue.FloatBits(bits);
                        }
                    }
                }
                result.Add(new PlayerSnapshotTable(tn, name, columns, rows)); columnNext += cols;
            }
            if (columnNext != nc || cellNext != nv || textNext != text) throw new InvalidDataException("Unreferenced Player payload.");
            return new PlayerSnapshotDocument(result);
        }
        public PlayerSnapshotTable Table(string name)
        { foreach (var t in tables) if (t.Name == name) return t; throw new InvalidDataException("Unknown Player table " + name); }
        public PlayerSnapshotDocument Clone() => Decode(Encode());
        public PlayerSnapshotDocument WithCell(string table, int row, string column, PlayerSnapshotValue value)
        {
            var t = Table(table); int c = t.ColumnIndex(column); t.Get(row, c);
            if (value == null || value.Type != t.Columns[c].Type) throw new InvalidDataException("Player cell type mismatch.");
            var rows = t.CopyRows(); rows[row][c] = value; return Replace(t, rows);
        }
        public PlayerSnapshotDocument WithRows(string table, IReadOnlyList<IReadOnlyDictionary<string, PlayerSnapshotValue>> values)
        {
            var t = Table(table); if (values == null || values.Count > 65536) throw new InvalidDataException("Player row count invalid.");
            var rows = new PlayerSnapshotValue[values.Count][];
            for (int r = 0; r < rows.Length; r++)
            {
                if (values[r] == null || values[r].Count != t.Columns.Count) throw new InvalidDataException("Complete typed row required.");
                rows[r] = new PlayerSnapshotValue[t.Columns.Count];
                for (int c = 0; c < rows[r].Length; c++) if (!values[r].TryGetValue(t.Columns[c].Name, out rows[r][c]) || rows[r][c] == null || rows[r][c].Type != t.Columns[c].Type) throw new InvalidDataException("Player row schema mismatch.");
            }
            return Replace(t, rows);
        }
        PlayerSnapshotDocument Replace(PlayerSnapshotTable old, PlayerSnapshotValue[][] rows)
        { var next = new List<PlayerSnapshotTable>(tables); next[next.IndexOf(old)] = old.WithValues(rows); var d = new PlayerSnapshotDocument(next); d.Encode(); return d; }
        public string SchemaDigest
        {
            get
            {
                using (var stream = new MemoryStream()) using (var w = new BinaryWriter(stream))
                { foreach (var t in tables) { w.Write(t.Name); w.Write(t.Columns.Count); foreach (var c in t.Columns) { w.Write(c.Name); w.Write((uint)c.Type); } } w.Flush(); return Hash(stream.ToArray()); }
            }
        }
        public void RequireSameSchema(PlayerSnapshotDocument other)
        { if (other == null || SchemaDigest != other.SchemaDigest) throw new InvalidDataException("Player schema differs."); }
        public static string Hash(byte[] bytes)
        { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant(); }
        public byte[] Encode()
        {
            long nc = 0, nv = 0, text = 0;
            foreach (var t in tables) { nc += t.Columns.Count; nv += (long)t.RowCount * t.Columns.Count; for (int r = 0; r < t.RowCount; r++) for (int c = 0; c < t.Columns.Count; c++) if (t.Get(r, c).Type == PlayerSnapshotType.Text) text += t.Get(r, c).RawText.Length; }
            long total = 32 + tables.Count * 64L + nc * 36 + nv * 12 + text;
            if (nv > 1000000 || total > MaximumBytes) throw new InvalidDataException("Player snapshot exceeds native bound.");
            using (var stream = new MemoryStream((int)total)) using (var w = new BinaryWriter(stream))
            {
                w.Write(0x31535052u); w.Write(1u); w.Write((uint)total); w.Write((uint)tables.Count); w.Write((uint)nc); w.Write((uint)nv); w.Write((uint)text); w.Write(0u);
                uint ci = 0, vi = 0;
                foreach (var t in tables) { w.Write(t.NameBytes); w.Write((uint)t.RowCount); w.Write(ci); w.Write((uint)t.Columns.Count); w.Write(vi); ci += (uint)t.Columns.Count; vi += (uint)(t.RowCount * t.Columns.Count); }
                foreach (var t in tables) foreach (var c in t.Columns) { w.Write(c.NameBytes); w.Write((uint)c.Type); }
                uint offset = 0;
                foreach (var t in tables) for (int r = 0; r < t.RowCount; r++) for (int c = 0; c < t.Columns.Count; c++) { var v = t.Get(r, c); w.Write(v.Bits); w.Write(v.Type == PlayerSnapshotType.Text ? offset : 0u); w.Write(v.Type == PlayerSnapshotType.Text ? (uint)v.RawText.Length : 0u); if (v.Type == PlayerSnapshotType.Text) offset += (uint)v.RawText.Length; }
                foreach (var t in tables) for (int r = 0; r < t.RowCount; r++) for (int c = 0; c < t.Columns.Count; c++) { var v = t.Get(r, c); if (v.Type == PlayerSnapshotType.Text) w.Write(v.RawText); }
                w.Flush(); return stream.ToArray();
            }
        }
    }
}
