// tests/untamed_convert: the helper every part's Program.cs shares (build.sh compiles it into each part). It compares what a C# stage
// produced with what the Python reference wrote: bytes first, then (for JSON) the documents with exact numbers, and reports the first
// differences. Nothing here weakens a comparison: a file that differs is a failure unless the caller passes an explicit, counted allowance.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport.Tests
{
    public static class UxTest
    {
        public static int Checks, Failures, Skipped;
        public static int MaxReports = 12;                 // lines printed per comparison before the rest is only counted
        static readonly Stopwatch s_clock = Stopwatch.StartNew();

        public static void Log(string line) { Console.WriteLine(line); Console.Out.Flush(); }
        public static void Section(string title) => Log("== " + title + "  [" + s_clock.Elapsed.TotalSeconds.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " s]");
        public static bool Check(bool ok, string what)
        {
            Checks++;
            if (!ok) { Failures++; Log("FAIL " + what); }
            return ok;
        }
        /// Check, and the line is printed also when it holds (summary lines).
        public static bool Report(bool ok, string what)
        {
            Checks++;
            if (!ok) Failures++;
            Log((ok ? "ok   " : "FAIL ") + what);
            return ok;
        }
        public static void Fail(string what) { Checks++; Failures++; Log("FAIL " + what); }
        /// A part of the test that could not run (an input is not on this machine). Reported, and it makes the result INCOMPLETE.
        public static void Skip(string what) { Skipped++; Log("SKIP " + what); }
        /// Prints the result line and returns the process exit code (0 only when nothing failed and nothing was skipped).
        public static int Result(string name)
        {
            string r = Failures > 0 ? "FAIL" : Skipped > 0 ? "INCOMPLETE" : "PASS";
            Log("RESULT " + name + ": " + r + " (" + Checks + " checks, " + Failures + " failed, " + Skipped + " skipped, " +
                s_clock.Elapsed.TotalSeconds.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " s)");
            return Failures > 0 ? 1 : Skipped > 0 ? 3 : 0;
        }

        /// --name value from the command line, else the environment variable, else the default.
        public static string Arg(string[] args, string name, string env = null, string fallback = null)
        {
            for (int i = 0; i + 1 < args.Length; i++) if (args[i] == "--" + name) return args[i + 1];
            string e = env != null ? Environment.GetEnvironmentVariable(env) : null;
            return string.IsNullOrEmpty(e) ? fallback : e;
        }
        public static bool Flag(string[] args, string name) => Array.IndexOf(args, "--" + name) >= 0;

        // ------------------------------------------------------------------------------------------ bytes
        /// "identical" or where two byte strings first differ.
        public static string Difference(byte[] got, byte[] want)
        {
            if (got == null) return "nothing produced";
            if (want == null) return "no reference";
            int n = Math.Min(got.Length, want.Length);
            for (int i = 0; i < n; i++)
                if (got[i] != want[i])
                    return "first difference at byte " + i + " (0x" + i.ToString("x") + "): got " + Hex(got, i, 8) + " want " + Hex(want, i, 8) +
                        (got.Length != want.Length ? "; sizes " + got.Length + " vs " + want.Length : "") + "; " + CountDiffs(got, want) + " bytes differ";
            return got.Length == want.Length ? "identical" : "sizes " + got.Length + " vs " + want.Length + " (the common " + n + " bytes are equal)";
        }
        static string Hex(byte[] b, int at, int n) { var sb = new StringBuilder(); for (int i = at; i < at + n && i < b.Length; i++) sb.Append(b[i].ToString("x2")); return sb.ToString(); }
        static int CountDiffs(byte[] a, byte[] b) { int n = Math.Min(a.Length, b.Length), c = Math.Abs(a.Length - b.Length); for (int i = 0; i < n; i++) if (a[i] != b[i]) c++; return c; }

        public static bool SameBytes(string what, byte[] got, byte[] want)
        {
            string d = Difference(got, want);
            return Check(d == "identical", what + ": " + d);
        }
        public static bool SameMd5(string what, byte[] got, string wantMd5, long wantSize = -1)
        {
            if (got == null) return Check(false, what + ": nothing produced");
            string m = UxBin.Md5(got);
            return Check(m == wantMd5 && (wantSize < 0 || got.Length == wantSize), what + ": md5 " + m + " (" + got.Length + " B), want " + wantMd5 + (wantSize < 0 ? "" : " (" + wantSize + " B)"));
        }

        // ------------------------------------------------------------------------------------------ JSON
        /// The differences between two JSON documents (UxJson.Parse trees), exact: an int is not a float, floats compare by value
        /// (bit for bit, +0 and -0 apart), strings ordinally, object keys as sets (and in order when keyOrder). Each is 'path: got X, want Y'.
        public static List<string> JsonDifferences(object got, object want, bool keyOrder = false, int limit = 50)
        {
            var o = new List<string>();
            Walk("$", got, want, keyOrder, o, limit);
            return o;
        }
        static string Show(object v)
        {
            switch (v)
            {
                case null: return "null";
                case string s: return "\"" + (s.Length > 60 ? s.Substring(0, 60) + "..." : s) + "\"";
                case JObj j: return "{" + j.items.Count + " keys}";
                case List<object> l: return "[" + l.Count + " items]";
                case double d: return PyFormat.Repr(d) + " (float)";
                case bool b: return b ? "true" : "false";
                default: return Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture) + " (int)";
            }
        }
        static void Walk(string path, object a, object b, bool keyOrder, List<string> o, int limit)
        {
            if (o.Count >= limit) return;
            if (a is JObj ja && b is JObj jb)
            {
                var bk = new Dictionary<string, object>(StringComparer.Ordinal); foreach (var kv in jb.items) bk[kv.Key] = kv.Value;
                var ak = new HashSet<string>(StringComparer.Ordinal);
                foreach (var kv in ja.items)
                {
                    ak.Add(kv.Key);
                    if (!bk.TryGetValue(kv.Key, out var bv)) { if (o.Count < limit) o.Add(path + "." + kv.Key + ": not in the reference (got " + Show(kv.Value) + ")"); continue; }
                    Walk(path + "." + kv.Key, kv.Value, bv, keyOrder, o, limit);
                }
                foreach (var kv in jb.items) if (!ak.Contains(kv.Key) && o.Count < limit) o.Add(path + "." + kv.Key + ": missing (want " + Show(kv.Value) + ")");
                if (keyOrder && o.Count < limit && ja.items.Count == jb.items.Count)
                    for (int i = 0; i < ja.items.Count; i++)
                        if (ja.items[i].Key != jb.items[i].Key) { o.Add(path + ": key order differs at " + i + ": got " + ja.items[i].Key + ", want " + jb.items[i].Key); break; }
                return;
            }
            if (a is List<object> la && b is List<object> lb)
            {
                if (la.Count != lb.Count) o.Add(path + ": " + la.Count + " items, want " + lb.Count);
                for (int i = 0; i < Math.Min(la.Count, lb.Count) && o.Count < limit; i++) Walk(path + "[" + i + "]", la[i], lb[i], keyOrder, o, limit);
                return;
            }
            bool same;
            if (a == null || b == null) same = a == null && b == null;
            else if (a is double da && b is double db) same = BitConverter.DoubleToInt64Bits(da) == BitConverter.DoubleToInt64Bits(db) || double.IsNaN(da) && double.IsNaN(db);
            else if (a is long la2 && b is long lb2) same = la2 == lb2;
            else if (a is BigInteger ba && b is BigInteger bb) same = ba == bb;
            else if (a is string sa && b is string sb) same = string.Equals(sa, sb, StringComparison.Ordinal);
            else if (a is bool xa && b is bool xb) same = xa == xb;
            else same = false;
            if (!same) o.Add(path + ": got " + Show(a) + ", want " + Show(b));
        }

        /// A produced JSON file against the reference's: byte for byte; when the bytes differ, the documents are compared and the
        /// first differences printed. Returns 0 identical bytes, 1 equal documents (bytes differ: a failure unless allowSemantic), 2 different.
        public static int SameJsonFile(string what, byte[] got, byte[] want, bool allowSemantic = false)
        {
            if (got != null && want != null && UxBin.Same(got, want)) { Checks++; return 0; }
            if (got == null || want == null) { Fail(what + ": " + Difference(got, want)); return 2; }
            List<string> d;
            try { d = JsonDifferences(UxJson.ParseUtf8(got), UxJson.ParseUtf8(want)); }
            catch (Exception e) { Fail(what + ": not comparable as JSON (" + e.Message + "); " + Difference(got, want)); return 2; }
            if (d.Count == 0)
            {
                if (allowSemantic) { Checks++; Log("note " + what + ": the documents are equal, the bytes differ (" + Difference(got, want) + ")"); }
                else Fail(what + ": the documents are equal but the bytes differ: " + Difference(got, want));
                return 1;
            }
            Fail(what + ": " + d.Count + (d.Count >= 50 ? "+" : "") + " JSON differences");
            for (int i = 0; i < d.Count && i < MaxReports; i++) Log("     " + d[i]);
            return 2;
        }

        // ------------------------------------------------------------------------------------------ folders
        public sealed class FolderResult
        {
            public int reference, identical, jsonEqual, different, missing, extra;
            public readonly List<string> differentFiles = new List<string>(), missingFiles = new List<string>(), extraFiles = new List<string>();
            public bool Ok => different == 0 && missing == 0 && jsonEqual == 0;
            public override string ToString() => reference + " reference files: " + identical + " identical, " + jsonEqual + " equal as JSON only, " + different +
                " different, " + missing + " missing, " + extra + " not in the reference";
        }

        /// Every file of the Python output folder against the produced folder (same relative paths; names compared ignoring case, as the
        /// reference's file system does). include: which reference files count (null = all). allowJsonEqual: a JSON file whose document
        /// is equal but whose bytes differ is counted apart instead of failing. Files only in the produced folder are listed, not failed
        /// (the Python folders hold previews and logs a port need not write) unless failExtra.
        public static FolderResult CompareFolder(string what, string produced, string reference, Func<string, bool> include = null, bool allowJsonEqual = false, bool failExtra = false)
        {
            var r = new FolderResult();
            if (!Directory.Exists(reference)) { Fail(what + ": no reference folder " + reference); return r; }
            var have = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (Directory.Exists(produced))
                foreach (string f in Directory.GetFiles(produced, "*", SearchOption.AllDirectories)) have[f.Substring(produced.TrimEnd('/', '\\').Length + 1).Replace('\\', '/')] = f;
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var files = Directory.GetFiles(reference, "*", SearchOption.AllDirectories);
            Array.Sort(files, StringComparer.OrdinalIgnoreCase);
            int reported = 0;
            foreach (string f in files)
            {
                string rel = f.Substring(reference.TrimEnd('/', '\\').Length + 1).Replace('\\', '/');
                if (include != null && !include(rel)) continue;
                r.reference++; seen.Add(rel);
                if (!have.TryGetValue(rel, out string mine)) { r.missing++; r.missingFiles.Add(rel); continue; }
                byte[] got = File.ReadAllBytes(mine), want = File.ReadAllBytes(f);
                if (UxBin.Same(got, want)) { r.identical++; continue; }
                if (rel.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                {
                    List<string> d = null;
                    try { d = JsonDifferences(UxJson.ParseUtf8(got), UxJson.ParseUtf8(want)); } catch (Exception) { }
                    if (d != null && d.Count == 0) { r.jsonEqual++; if (!allowJsonEqual && reported++ < MaxReports) Log("     " + rel + ": equal as JSON, bytes differ: " + Difference(got, want)); continue; }
                    r.different++; r.differentFiles.Add(rel);
                    if (reported++ < MaxReports)
                    {
                        Log("     " + rel + ": " + (d == null ? "not JSON; " + Difference(got, want) : d.Count + (d.Count >= 50 ? "+" : "") + " JSON differences"));
                        if (d != null) for (int i = 0; i < d.Count && i < 6; i++) Log("        " + d[i]);
                    }
                    continue;
                }
                r.different++; r.differentFiles.Add(rel);
                if (reported++ < MaxReports) Log("     " + rel + ": " + Difference(got, want));
            }
            foreach (var kv in have) if (!seen.Contains(kv.Key) && (include == null || include(kv.Key))) { r.extra++; r.extraFiles.Add(kv.Key); }
            for (int i = 0; i < r.missingFiles.Count && i < MaxReports; i++) Log("     missing: " + r.missingFiles[i]);
            if (failExtra) for (int i = 0; i < r.extraFiles.Count && i < MaxReports; i++) Log("     not in the reference: " + r.extraFiles[i]);
            bool ok = r.different == 0 && r.missing == 0 && (allowJsonEqual || r.jsonEqual == 0) && (!failExtra || r.extra == 0);
            Checks++;
            if (!ok) Failures++;
            Log((ok ? "ok   " : "FAIL ") + what + ": " + r);
            return r;
        }

        // ------------------------------------------------------------------------------------------ a stage test's inputs
        /// What a stage test works with: the disc, the Reflex library, the stock content, the Python reference folder and the scratch folder.
        public sealed class Env : IDisposable
        {
            public string py, scratch; public UxDisc disc; public UxDirectoryLibrary library; public UxStock stock;
            /// The Python stage outputs of a level: <py>/work/<Level>.
            public string PyWork(UxLevelRow lv) => Path.Combine(py, "work", lv.disc);
            /// A stage context for one level; its work store is <scratch>/<workName>/<Level> (emptied first when fresh).
            public UxContext Level(UxLevelRow lv, string workName = "work", bool fresh = true)
            {
                string root = Path.Combine(scratch, workName);
                if (fresh && Directory.Exists(Path.Combine(root, lv.disc))) Directory.Delete(Path.Combine(root, lv.disc), true);
                return new UxContext { lv = lv, disc = disc?.Level(lv), library = library, stock = stock, work = UxWork.ForLevel(root, lv), log = Log };
            }
            /// Copies upstream Python outputs (files or folders, relative to work/<Level>) into the context's work store, so a stage can
            /// be tested alone on the reference's own inputs. Returns the number of files copied.
            public int Seed(UxContext ctx, params string[] relative)
            {
                int n = 0;
                foreach (string rel in relative)
                {
                    string src = Path.Combine(PyWork(ctx.lv), rel.Replace('/', Path.DirectorySeparatorChar));
                    if (File.Exists(src)) { ctx.work.WriteBytes(rel, File.ReadAllBytes(src)); n++; }
                    else if (Directory.Exists(src))
                    {
                        string top = Path.GetFullPath(src).TrimEnd(Path.DirectorySeparatorChar);
                        foreach (string f in Directory.GetFiles(top, "*", SearchOption.AllDirectories))
                        { ctx.work.WriteBytes(rel.TrimEnd('/') + "/" + f.Substring(top.Length + 1).Replace(Path.DirectorySeparatorChar, '/'), File.ReadAllBytes(f)); n++; }
                    }
                    else throw new UxException("no reference output " + src);
                }
                return n;
            }
            public void Dispose() { disc?.Dispose(); library?.Dispose(); }
        }

        /// Opens the inputs named on the command line or in the environment: --folder / UX_DISC_FOLDER or --iso / UX_DISC_ISO (the disc),
        /// --reflex / UX_REFLEX or --client / UX_CLIENT_LIB (the library), --py / UX_PY_ROOT, --scratch / UX_SCRATCH. The template name
        /// tables (UxStock.templateNames 'sx' and 'fr') are read from the reference's templates folder. A missing input stays null.
        public static Env OpenEnv(string[] args)
        {
            var e = new Env { py = Arg(args, "py", "UX_PY_ROOT", ""), scratch = Arg(args, "scratch", "UX_SCRATCH", Path.Combine(Path.GetTempPath(), "untamed-port")) };
            Directory.CreateDirectory(e.scratch);
            string folder = Arg(args, "folder", "UX_DISC_FOLDER"), iso = Arg(args, "iso", "UX_DISC_ISO");
            if (folder != null && Directory.Exists(folder)) e.disc = UxDisc.Open(folder); else if (iso != null && File.Exists(iso)) e.disc = UxDisc.Open(iso);
            string reflex = Arg(args, "reflex", "UX_REFLEX"), client = Arg(args, "client", "UX_CLIENT_LIB");
            string lib = reflex != null && Directory.Exists(reflex) ? reflex : client != null && Directory.Exists(client) ? client : null;
            if (lib != null) { e.library = new UxDirectoryLibrary(lib); e.stock = new UxStock(e.library); }
            if (e.stock != null)
                foreach (var (kind, dir, slot) in new[] { ("sx", "sx-sketch-slot1", "Beta_Sx_Track_Slot_1"), ("fr", "huckit-freeride-slot1", "Beta_Track_Slot_1") })
                {
                    string db = Path.Combine(e.py, "templates", dir, slot + ".dx9.database");
                    if (File.Exists(db)) e.stock.templateNames[kind] = new UxTemplateNames(slot, File.ReadAllBytes(db));
                }
            return e;
        }

        /// A fresh, empty folder below the scratch root (deleted first when it exists).
        public static string FreshFolder(string scratch, string name)
        {
            string p = Path.Combine(scratch, name);
            if (Directory.Exists(p)) Directory.Delete(p, true);
            Directory.CreateDirectory(p);
            return p;
        }

        /// md5sum-style list ('<hex> *<name>' per line) -> name -> hex (ux_common.read_md5_list).
        public static Dictionary<string, string> ReadMd5List(string path)
        {
            var o = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string raw in File.ReadAllLines(path))
            {
                string ln = raw.Trim(); if (ln.Length == 0) continue;
                int sp = ln.IndexOfAny(new[] { ' ', '\t' }); if (sp < 0) continue;
                o[ln.Substring(sp).Trim().TrimStart('*')] = ln.Substring(0, sp).ToLowerInvariant();
            }
            return o;
        }
    }
}
