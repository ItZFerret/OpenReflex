// tests/untamed_convert: the helper the converter tests share (build.sh compiles it into each part; tests/untamed_import uses it too):
// counting checks, the command line, byte comparisons, and the inputs (the disc, the Reflex library, a scratch folder). A local,
// git-ignored reference/ folder may extend it (UxTestReference.cs, compiled into the parts below reference/ only).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Numerics;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport.Tests
{
    public static partial class UxTest
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
            if (want == null) return "nothing to compare with";
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

        // ------------------------------------------------------------------------------------------ a stage test's inputs
        /// What a test works with: the disc, the Reflex library, the stock content and the scratch folder.
        public sealed partial class Env : IDisposable
        {
            public string scratch; public UxDisc disc; public UxDirectoryLibrary library; public UxStock stock;
            /// A stage context for one level; its work store is <scratch>/<workName>/<Level> (emptied first when fresh).
            public UxContext Level(UxLevelRow lv, string workName = "work", bool fresh = true)
            {
                string root = Path.Combine(scratch, workName);
                if (fresh && Directory.Exists(Path.Combine(root, lv.disc))) Directory.Delete(Path.Combine(root, lv.disc), true);
                return new UxContext { lv = lv, disc = disc?.Level(lv), library = library, stock = stock, work = UxWork.ForLevel(root, lv), log = Log };
            }
            public void Dispose() { disc?.Dispose(); library?.Dispose(); }
        }

        /// Opens the inputs named on the command line or in the environment: --folder / UX_DISC_FOLDER or --iso / UX_DISC_ISO (the disc),
        /// --reflex / UX_REFLEX or --client / UX_CLIENT_LIB (the library), --scratch / UX_SCRATCH. A missing input stays null.
        public static Env OpenEnv(string[] args)
        {
            var e = new Env { scratch = Arg(args, "scratch", "UX_SCRATCH", Path.Combine(Path.GetTempPath(), "untamed-port")) };
            Directory.CreateDirectory(e.scratch);
            string folder = Arg(args, "folder", "UX_DISC_FOLDER"), iso = Arg(args, "iso", "UX_DISC_ISO");
            if (folder != null && Directory.Exists(folder)) e.disc = UxDisc.Open(folder); else if (iso != null && File.Exists(iso)) e.disc = UxDisc.Open(iso);
            string reflex = Arg(args, "reflex", "UX_REFLEX"), client = Arg(args, "client", "UX_CLIENT_LIB");
            string lib = reflex != null && Directory.Exists(reflex) ? reflex : client != null && Directory.Exists(client) ? client : null;
            if (lib != null) { e.library = new UxDirectoryLibrary(lib); e.stock = new UxStock(e.library); }
            OpenMore(e, args);
            return e;
        }
        /// Implemented by the local reference/ helper when it is compiled in (nothing otherwise).
        static partial void OpenMore(Env e, string[] args);

        /// A fresh, empty folder below the scratch root (deleted first when it exists).
        public static string FreshFolder(string scratch, string name)
        {
            string p = Path.Combine(scratch, name);
            if (Directory.Exists(p)) Directory.Delete(p, true);
            Directory.CreateDirectory(p);
            return p;
        }

        /// md5sum-style list ('<hex> *<name>' per line, '#' comments) -> name -> hex.
        public static Dictionary<string, string> ReadMd5List(string path)
        {
            var o = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string raw in File.ReadAllLines(path))
            {
                string ln = raw.Trim(); if (ln.Length == 0 || ln[0] == '#') continue;
                int sp = ln.IndexOfAny(new[] { ' ', '\t' }); if (sp < 0) continue;
                o[ln.Substring(sp).Trim().TrimStart('*')] = ln.Substring(0, sp).ToLowerInvariant();
            }
            return o;
        }
    }
}
