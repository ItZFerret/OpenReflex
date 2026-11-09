// RowIndex (w-menufps; a tester: "FPS drops when navigating the menus"): the install tables' rows by an integer column, made once per table and
// column. The garage's lists and the setup page's vehicle lookups asked for one row by id with a scan of the whole table (StoreAssets, StoreProducts,
// VehicleModel, RiderGear, VehicleModelParts: thousands of rows), several scans per list item: 60 ms for the vehicle list, 200-700 ms for a garage
// tab on the developer's Mac. The answers are the scans' own: First is the first row whose column has the value (Rfx.Int, -1 when missing, as
// FirstOrDefault(r => Rfx.Int(r, col) == v)), All the rows that have it in the table's order (as Where). An index is made again when its table
// grows (the DLC tables merged in); the tables are never edited in place. Thread-safe.
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Reflex.Port.Content
{
    internal static class RowIndex
    {
        sealed class Index { public int count; public Dictionary<int, List<Dictionary<string, object>>> rows; }
        static readonly ConditionalWeakTable<List<Dictionary<string, object>>, Dictionary<string, Index>> s_tables = new ConditionalWeakTable<List<Dictionary<string, object>>, Dictionary<string, Index>>();
        static readonly List<Dictionary<string, object>> s_none = new List<Dictionary<string, object>>();

        static Dictionary<int, List<Dictionary<string, object>>> Of(List<Dictionary<string, object>> table, string col)
        {
            lock (s_tables)
            {
                var byCol = s_tables.GetValue(table, _ => new Dictionary<string, Index>());
                if (byCol.TryGetValue(col, out var ix) && ix.count == table.Count) return ix.rows;
                var rows = new Dictionary<int, List<Dictionary<string, object>>>();
                foreach (var r in table)
                {
                    int k = Rfx.Int(r, col);
                    if (!rows.TryGetValue(k, out var list)) rows[k] = list = new List<Dictionary<string, object>>(1);
                    list.Add(r);
                }
                byCol[col] = new Index { count = table.Count, rows = rows };
                return rows;
            }
        }

        /// The first row of the table whose column is v (null: none).
        public static Dictionary<string, object> First(List<Dictionary<string, object>> table, string col, int v)
        {
            if (table == null || table.Count == 0) return null;
            return Of(table, col).TryGetValue(v, out var list) ? list[0] : null;
        }

        /// The rows of the table whose column is v, in the table's order (an empty list: none; not to be changed).
        public static List<Dictionary<string, object>> All(List<Dictionary<string, object>> table, string col, int v)
        {
            if (table == null || table.Count == 0) return s_none;
            return Of(table, col).TryGetValue(v, out var list) ? list : s_none;
        }
    }
}
