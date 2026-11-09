using System;
using System.Runtime.InteropServices;
using System.Text;
namespace Reflex.Port.RxCore
{
    // The race results page's table (core/include/rx_results.h, 4008 bytes, version 2: 32 rows (w-rc-core; version 1 had 16); w-resultsrng): the game's "Activity Results" as
    // App.PopulateResultsTable left it, which the core runs where the original's results page starts (14 frames after "Show Activity
    // Results"). Its TOTAL holds the PROJECTED time of a rider still racing when the race ended (00613880), the BEST LAP a projected lap
    // for a rider without one; cells are the game's own text ("MM:SS.cc"), sort values -t.
    public sealed class RxResultsPage
    {
        public const int Size = 4008, RowSize = 120, MaxRows = 32, Version = 2;
        public sealed class Row
        {
            public int index, slot; public bool local; public string name;
            public readonly string[] cells = new string[3]; public readonly float[] sort = new float[3];
        }
        public uint populated; public int activityType, columns;
        public readonly string[] titles = new string[3];
        public Row[] rows = new Row[0];
        // the column whose title key ends in the name (TOTALTIME, BESTLAPTIME, PENALTYTIME), -1 absent
        public int Column(string name)
        {
            for (int k = 0; k < columns && k < 3; k++)
                if (titles[k] != null && titles[k].EndsWith("_" + name, StringComparison.OrdinalIgnoreCase)) return k;
            return -1;
        }
        public Row BySlot(int slot) { foreach (var r in rows) if (r.slot == slot) return r; return null; }
    }

    public static class RxResultsNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, byte[] value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_results_page");
        /// rx_session_results_page (between frames). Null while the page has not started; the previous object when nothing changed.
        /// Throws EntryPointNotFoundException on an older core.
        public static RxResultsPage Read(IntPtr session, byte[] scratch, RxResultsPage previous)
        {
            Array.Clear(scratch, 0, RxResultsPage.Size);
            BitConverter.GetBytes(RxResultsPage.Size).CopyTo(scratch, 0); scratch[4] = (byte)RxResultsPage.Version;
            if (read.Get()(session, scratch) != 0) return null;
            uint U(int o) => BitConverter.ToUInt32(scratch, o);
            int I(int o) => BitConverter.ToInt32(scratch, o);
            string S(int o, int n) { int e = Array.IndexOf(scratch, (byte)0, o, n); return Encoding.UTF8.GetString(scratch, o, (e < 0 ? o + n : e) - o); }
            uint populated = U(8);
            if (populated == 0) return null;
            if (previous != null && previous.populated == populated && previous.activityType == I(12)) return previous;
            var p = new RxResultsPage { populated = populated, activityType = I(12), columns = I(16) };
            int rows = Math.Max(0, Math.Min(RxResultsPage.MaxRows, I(20)));
            for (int k = 0; k < 3; k++) p.titles[k] = S(24 + 48 * k, 48);
            p.rows = new RxResultsPage.Row[rows];
            for (int i = 0; i < rows; i++)
            {
                int o = 168 + RxResultsPage.RowSize * i;
                var r = new RxResultsPage.Row { index = I(o), slot = I(o + 4), local = I(o + 8) != 0, name = S(o + 12, 48) };
                for (int k = 0; k < 3; k++) { r.cells[k] = S(o + 60 + 16 * k, 16); r.sort[k] = BitConverter.ToSingle(scratch, o + 108 + 4 * k); }
                p.rows[i] = r;
            }
            return p;
        }
    }
}
