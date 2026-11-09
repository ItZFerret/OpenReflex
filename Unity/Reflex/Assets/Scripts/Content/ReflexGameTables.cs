// Column reads of the game's tables (ReflexVehicleSpecs builds bikes and riders from them; KitPacks adds the gear / kit packs' rows through
// its overlay source). Pure (System only): tests/gearmods compiles it on .NET.
using System;
using System.Collections.Generic;

namespace Reflex.Port.Content
{
    /// Column reads of the game's tables (a source: the running world, OriginalWorld.ReflexRoster), cached per column.
    public sealed class ReflexGameTables
    {
        public interface ISource
        {
            int Rows(string table);                              // -1: no such table
            int[] Ints(string table, string column);              // null: no such int column
            string[] Strings(string table, string column);        // null: no such string column
        }
        readonly ISource src;
        readonly Dictionary<string, int[]> ints = new Dictionary<string, int[]>(StringComparer.Ordinal);
        readonly Dictionary<string, string[]> strings = new Dictionary<string, string[]>(StringComparer.Ordinal);
        readonly Dictionary<string, Dictionary<int, int>> index = new Dictionary<string, Dictionary<int, int>>(StringComparer.Ordinal);
        public ReflexGameTables(ISource src) { this.src = src; }

        public int Rows(string table) => src.Rows(table);
        public int[] Ints(string table, string column)
        {
            string k = table + "|" + column;
            if (!ints.TryGetValue(k, out var v)) ints[k] = v = src.Ints(table, column) ?? new int[0];
            return v;
        }
        public string[] Strings(string table, string column)
        {
            string k = table + "|" + column;
            if (!strings.TryGetValue(k, out var v)) strings[k] = v = src.Strings(table, column) ?? new string[0];
            return v;
        }
        /// The first row whose key column holds key, -1 for none.
        public int Find(string table, string keyColumn, int key)
        {
            string k = table + "|" + keyColumn;
            if (!index.TryGetValue(k, out var d))
            {
                index[k] = d = new Dictionary<int, int>();
                var col = Ints(table, keyColumn);
                for (int i = 0; i < col.Length; i++) if (!d.ContainsKey(col[i])) d[col[i]] = i;
            }
            return d.TryGetValue(key, out int r) ? r : -1;
        }
        public IEnumerable<int> Where(string table, string column, int value)
        {
            var col = Ints(table, column);
            for (int i = 0; i < col.Length; i++) if (col[i] == value) yield return i;
        }
        public int Int(string table, string column, int row, int fallback = -1)
        {
            var col = Ints(table, column);
            return row >= 0 && row < col.Length ? col[row] : fallback;
        }
        public string Str(string table, string column, int row)
        {
            var col = Strings(table, column);
            return row >= 0 && row < col.Length ? col[row] : null;
        }
        /// A package a table row names (AssetPackageDatabase -> AssetDatabase.DatabaseName / AssetPackage.PackageName), null for none (-1).
        public (string db, string pkg)? Package(int packageDatabaseId)
        {
            if (packageDatabaseId < 0) return null;
            int r = Find("AssetPackageDatabase", "PackageDatabaseId", packageDatabaseId);
            if (r < 0) return null;
            string db = Str("AssetDatabase", "DatabaseName", Find("AssetDatabase", "DatabaseId", Int("AssetPackageDatabase", "DatabaseId", r)));
            string pkg = Str("AssetPackage", "PackageName", Find("AssetPackage", "PackageId", Int("AssetPackageDatabase", "PackageId", r)));
            if (string.IsNullOrEmpty(db) || string.IsNullOrEmpty(pkg)) return null;
            return (db, pkg);
        }
    }
}
