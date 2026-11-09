using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    // ProfileData is a display/edit view; the document remains raw-byte authority.
    public static class PlayerProfileSnapshotAdapter
    {
        static readonly Encoding Text = new UTF8Encoding(false, false);
        public static ProfileData ToView(PlayerSnapshotDocument document)
        {
            var p = new ProfileData { signedIn = true, storage = true, gameLoaded = true };
            foreach (var source in document.Tables)
            {
                var t = new ProfileData.Table { rows = source.RowCount };
                foreach (var c in source.Columns)
                {
                    var values = new List<object>();
                    for (int r = 0; r < source.RowCount; r++)
                    {
                        var v = source.Get(r, c.Name);
                        values.Add(c.Type == PlayerSnapshotType.Text ? (object)Text.GetString(v.TextBytes) : c.Type == PlayerSnapshotType.Int ? (double)unchecked((int)v.Bits) : (double)BitConverter.ToSingle(BitConverter.GetBytes(v.Bits), 0));
                    }
                    t.columns.Add(c.Name, values);
                }
                p.tables.Add(source.Name, t);
            }
            return p;
        }
        public static ProfileData CloneView(ProfileData source)
        {
            var p = new ProfileData { signedIn = source.signedIn, storage = source.storage, gameLoaded = source.gameLoaded, careerCompletion = source.careerCompletion };
            foreach (var item in source.tables) { var t = new ProfileData.Table { rows = item.Value.rows }; foreach (var c in item.Value.columns) t.columns.Add(c.Key, new List<object>(c.Value)); p.tables.Add(item.Key, t); }
            foreach (var item in source.products) p.products.Add(item.Key, item.Value);
            foreach (var item in source.events) p.events.Add(item.Key, item.Value);
            foreach (var item in source.uiSettings) p.uiSettings.Add(item.Key, item.Value);
            return p;
        }
        static object Cell(ProfileData.Table t, string name, int row)
        {
            if (!t.columns.TryGetValue(name, out var values) || row < 0 || row >= values.Count || values[row] == null) throw new InvalidDataException("Missing complete Player value: " + name + " row " + row);
            return values[row];
        }
        static PlayerSnapshotValue Value(PlayerSnapshotColumn c, object value)
        {
            if (c.Type == PlayerSnapshotType.Text) { if (!(value is string s)) throw new InvalidDataException("Player string type mismatch: " + c.Name); return PlayerSnapshotValue.TextUtf8(s); }
            if (!(value is double || value is int || value is float || value is long)) throw new InvalidDataException("Player number type mismatch: " + c.Name);
            double n = Convert.ToDouble(value, System.Globalization.CultureInfo.InvariantCulture);
            if (c.Type == PlayerSnapshotType.Int)
            { if (double.IsNaN(n) || double.IsInfinity(n) || n < int.MinValue || n > int.MaxValue || n != Math.Truncate(n)) throw new InvalidDataException("Invalid Player integer: " + c.Name); return PlayerSnapshotValue.Int((int)n); }
            float f = (float)n;
            if (double.IsNaN(n) || double.IsInfinity(n) || float.IsInfinity(f)) throw new InvalidDataException("Invalid edited/migrated Player float: " + c.Name);
            return PlayerSnapshotValue.FloatBits(BitConverter.ToUInt32(BitConverter.GetBytes(f), 0));
        }
        static void CheckColumns(PlayerSnapshotTable schema, ProfileData.Table view)
        {
            foreach (string name in view.columns.Keys) if (!schema.Columns.Any(c => c.Name == name)) throw new InvalidDataException("Unknown Player column: " + schema.Name + "." + name);
            if (view.rows < 0 || view.rows > 65536) throw new InvalidDataException("Invalid Player row count.");
        }
        // v1 has no raw/schema fidelity. Missing nonempty fields cannot be inferred.
        // The real worker baseline supplies schemas and two historically omitted camera cells only.
        public static PlayerSnapshotDocument Migrate(PlayerSnapshotDocument nativeBaseline, ProfileData legacy)
        {
            var result = nativeBaseline;
            foreach (string name in legacy.tables.Keys) nativeBaseline.Table(name); // reject unknown table, including empty
            foreach (var schema in nativeBaseline.Tables)
            {
                if (!legacy.tables.TryGetValue(schema.Name, out var t))
                { if (schema.RowCount != 0) throw new InvalidDataException("Legacy profile is missing nonempty table " + schema.Name); continue; }
                CheckColumns(schema, t);
                var rows = new List<IReadOnlyDictionary<string, PlayerSnapshotValue>>();
                for (int r = 0; r < t.rows; r++)
                {
                    var row = new Dictionary<string, PlayerSnapshotValue>();
                    foreach (var c in schema.Columns)
                    {
                        // The shipped v1 serializer omitted exactly these two columns.
                        // Copy only an existing real baseline row; never invent a value.
                        bool omittedCamera = schema.Name == "PlayerRider" &&
                            (c.Name == "UserCameraHeading" || c.Name == "UserCameraPitch") &&
                            !t.columns.ContainsKey(c.Name);
                        if (omittedCamera && r < schema.RowCount) row.Add(c.Name, schema.Get(r, c.Name));
                        else row.Add(c.Name, Value(c, Cell(t, c.Name, r)));
                    }
                    rows.Add(row);
                }
                result = result.WithRows(schema.Name, rows);
            }
            return result;
        }
        static bool SameRow(PlayerSnapshotTable schema, ProfileData.Table a, int ar, ProfileData.Table b, int br)
        { foreach (var c in schema.Columns) if (!object.Equals(Cell(a, c.Name, ar), Cell(b, c.Name, br))) return false; return true; }
        public static PlayerSnapshotDocument ApplyDiff(PlayerSnapshotDocument document, ProfileData before, ProfileData after)
        {
            var result = document;
            foreach (string name in after.tables.Keys) document.Table(name);
            foreach (var schema in document.Tables)
            {
                if (!before.tables.TryGetValue(schema.Name, out var old) || !after.tables.TryGetValue(schema.Name, out var next)) throw new InvalidDataException("Player schema table was removed: " + schema.Name);
                CheckColumns(schema, old); CheckColumns(schema, next);
                if (old.rows != schema.RowCount) throw new InvalidDataException("Stale Player view: " + schema.Name);
                if (old.rows == next.rows)
                {
                    for (int r = 0; r < next.rows; r++) foreach (var c in schema.Columns)
                    { object a = Cell(old, c.Name, r), b = Cell(next, c.Name, r); if (!object.Equals(a, b)) result = result.WithCell(schema.Name, r, c.Name, Value(c, b)); }
                    continue;
                }
                // Preserve byte-exact cells of unchanged rows even when deletion shifts
                // their index. New/replaced rows must explicitly supply every column.
                var rows = new List<IReadOnlyDictionary<string, PlayerSnapshotValue>>(); var used = new HashSet<int>();
                for (int r = 0; r < next.rows; r++)
                {
                    int match = -1; for (int i = 0; i < old.rows; i++) if (!used.Contains(i) && SameRow(schema, old, i, next, r)) { match = i; used.Add(i); break; }
                    var row = new Dictionary<string, PlayerSnapshotValue>(); foreach (var c in schema.Columns) row.Add(c.Name, match >= 0 ? schema.Get(match, c.Name) : Value(c, Cell(next, c.Name, r))); rows.Add(row);
                }
                result = result.WithRows(schema.Name, rows);
            }
            return result;
        }
    }

}
