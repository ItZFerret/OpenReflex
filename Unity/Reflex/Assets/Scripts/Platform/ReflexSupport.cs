// The support report: one redacted ZIP for a bug report (after Open Unleashed's MxaSupport; notes: _re/notes/client-platform.md).
//
// ENTRY POINTS for a SUPPORT page / crash prompt (the options agent's OPTIONS > SUPPORT, any front end):
//   string ReflexSupport.CreateReport(string folder = null)   writes the ZIP, copies its path to the clipboard, returns the path
//                                                             (throws on an I/O error)
//   string ReflexSupport.CreateReportForPlayer()              the same, never throws: a status line for the page ("Saved ~/Desktop/
//                                                             Reflex-Report-....zip (path copied to the clipboard)" or the error),
//                                                             and shows the ZIP in Finder / Explorer
//   void   ReflexSupport.OpenLogFolder()                      opens ReflexSupport.LogDir in Finder / Explorer
//   string ReflexSupport.LogDir, ReflexCrashLog.BuildStamp, .SessionTag, .PreviousCrashed, .PreviousCrashReport, .PromptDismissed
// The options pages' hooks (Reflex.Port.Options.ReflexOptionsHooks.CreateSupportReport / OpenLogFolder) are set at start-up by
// reflection, so this compiles with or without the options code.
//
// The ZIP (Reflex-Report-<yyyyMMdd-HHmmss>.zip on the Desktop, else Downloads, else the log folder) holds
//   system-info.txt  build, OS, CPU / GPU / RAM, graphics, game folder, plugin (API, struct check, crash handler), scene, race
//   sessions/        the current and previous session logs (ReflexCrashLog)
//   crashes/         the newest native crash reports (crash-*.txt), the previous session's first, and Mono's mono_crash.*.json
//   unity/           Player.log and Player-prev.log; Windows: the newest Unity Crashes\Crash_* folder; macOS: the newest system
//                    crash reports of the player (.ips, DiagnosticReports)
// Privacy: the home folder is written as ~, the user name as <user> (binary dumps: overwritten in place), and passwords, tokens
// and keys are masked. Nothing is uploaded: the player sends the file themselves. The Editor can write a report too (no session
// log of its own: its Editor.log is added instead).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

namespace Reflex.Port.Platform
{
    public static class ReflexSupport
    {
        const long MaxFileBytes = 96L << 20;

        /// The last report written this session (null = none yet).
        public static string LastReport { get; private set; }

        static bool Windows => Application.platform == RuntimePlatform.WindowsPlayer || Application.platform == RuntimePlatform.WindowsEditor;
        static bool Mac => Application.platform == RuntimePlatform.OSXPlayer || Application.platform == RuntimePlatform.OSXEditor;

        static string s_home;
        static string Home
        {
            get
            {
                if (s_home != null) return s_home;
                string h = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                if (string.IsNullOrEmpty(h)) h = Environment.GetEnvironmentVariable(Windows ? "USERPROFILE" : "HOME") ?? "";
                return s_home = h.TrimEnd('/', '\\');
            }
        }

        static readonly Regex Secrets = new Regex(
            "(?i)(\"?(?:password|passwd|pass|token|api[_-]?key|admin[_-]?key|secret|bearer[_-]?token)\"?\\s*[:=]\\s*)(\"[^\"]*\"|[^\\s,;}&]+)",
            RegexOptions.Compiled);
        static readonly Regex Bearer = new Regex("(?i)(Bearer\\s+)[A-Za-z0-9._~+/=-]+", RegexOptions.Compiled);
        static readonly Regex GitHubToken = new Regex("\\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\\b", RegexOptions.Compiled);

        /// Text with the home folder as ~, the user name as <user> and secrets masked.
        public static string Redact(string text)
        {
            if (string.IsNullOrEmpty(text)) return text ?? "";
            string h = Home;
            if (h.Length > 1)
            {
                text = text.Replace(h, "~", StringComparison.OrdinalIgnoreCase);
                if (Windows) text = text.Replace(h.Replace('\\', '/'), "~", StringComparison.OrdinalIgnoreCase);
                if (h.Contains("\\")) text = text.Replace(h.Replace("\\", "\\\\"), "~", StringComparison.OrdinalIgnoreCase);
            }
            string user = Path.GetFileName(h);
            if (string.IsNullOrEmpty(user)) user = Environment.UserName;
            if (!string.IsNullOrEmpty(user) && user.Length >= 3)
                text = Regex.Replace(text, "(?<![A-Za-z0-9_])" + Regex.Escape(user) + "(?![A-Za-z0-9_])", "<user>", RegexOptions.IgnoreCase);
            text = Secrets.Replace(text, m => m.Groups[1].Value + (m.Groups[2].Value.StartsWith("\"") ? "\"<redacted>\"" : "<redacted>"));
            text = Bearer.Replace(text, "$1<redacted>");
            text = GitHubToken.Replace(text, "<redacted>");
            return text;
        }

        /// A binary dump with the user name overwritten in place (same length, so the file's structure is unchanged).
        static byte[] RedactBinary(byte[] data)
        {
            string h = Home;
            string user = Path.GetFileName(h);
            if (string.IsNullOrEmpty(user) || user.Length < 2) return data;
            string masked = new string('_', user.Length);
            foreach (var enc in new Encoding[] { Encoding.UTF8, Encoding.Unicode })
                foreach (char sep in new[] { '\\', '/' })
                {
                    string prefix = Path.GetDirectoryName(h)?.Replace('\\', sep).Replace('/', sep) + sep;
                    ReplaceAll(data, enc.GetBytes(prefix + user), enc.GetBytes(prefix + masked));
                    ReplaceAll(data, enc.GetBytes((prefix + user).ToUpperInvariant()), enc.GetBytes((prefix + masked).ToUpperInvariant()));
                }
            return data;
        }

        static void ReplaceAll(byte[] data, byte[] find, byte[] repl)
        {
            if (find.Length == 0 || find.Length != repl.Length) return;
            for (int i = 0; i <= data.Length - find.Length; i++)
            {
                if (data[i] != find[0]) continue;
                int k = 1;
                while (k < find.Length && data[i + k] == find[k]) k++;
                if (k == find.Length) { Buffer.BlockCopy(repl, 0, data, i, repl.Length); i += find.Length - 1; }
            }
        }

        // ------------------------------------------------------------------------------------------------ the options hooks

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void HookOptions()
        {
            try
            {
                Type hooks = null;
                foreach (var a in AppDomain.CurrentDomain.GetAssemblies())
                    if ((hooks = a.GetType("Reflex.Port.Options.ReflexOptionsHooks", false)) != null) break;
                if (hooks == null) return;
                const System.Reflection.BindingFlags F = System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static;
                var create = hooks.GetField("CreateSupportReport", F);
                if (create != null && create.FieldType == typeof(Func<string>) && create.GetValue(null) == null)
                    create.SetValue(null, (Func<string>)(() => CreateReport()));
                var open = hooks.GetField("OpenLogFolder", F);
                if (open != null && open.FieldType == typeof(Action) && open.GetValue(null) == null)
                    open.SetValue(null, (Action)OpenLogFolder);
            }
            catch (Exception e) { UnityEngine.Debug.LogWarning("[Reflex] support: the options hooks were not set: " + e.Message); }
        }

        // ------------------------------------------------------------------------------------------------ folders

        public static string LogDir => string.IsNullOrEmpty(ReflexCrashLog.LogDir) ? ReflexCrashLog.DefaultLogDir() : ReflexCrashLog.LogDir;

        /// Opens the log folder in Finder / Explorer (creating it if needed).
        public static void OpenLogFolder()
        {
            string dir = LogDir;
            try { Directory.CreateDirectory(dir); } catch (Exception) { }
            Reveal(dir, false);
        }

        /// Shows a file selected in Finder / Explorer (or opens a folder).
        public static void Reveal(string path, bool select = true)
        {
            try
            {
                if (Mac) { Process.Start("open", (select ? "-R " : "") + Quote(path)); return; }
                if (Windows) { Process.Start("explorer.exe", (select ? "/select," : "") + Quote(path.Replace('/', '\\'))); return; }
            }
            catch (Exception e) { UnityEngine.Debug.LogWarning("[Reflex] could not open " + Redact(path) + ": " + e.Message); }
            try { Application.OpenURL(new Uri(select ? Path.GetDirectoryName(path) : path).AbsoluteUri); } catch (Exception) { }
        }

        static string Quote(string p) => "\"" + p.Replace("\"", "\\\"") + "\"";

        static string ReportFolder(string requested)
        {
            if (!string.IsNullOrEmpty(requested)) { Directory.CreateDirectory(requested); return requested; }
            foreach (var d in new[]
            {
                Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                Path.Combine(Home, "Desktop"),
                Path.Combine(Home, "Downloads"),
            })
                if (!string.IsNullOrEmpty(d) && Directory.Exists(d)) return d;
            Directory.CreateDirectory(LogDir);
            return LogDir;
        }

        // ------------------------------------------------------------------------------------------------ the report

        /// CREATE REPORT for a UI: writes the ZIP, shows it in Finder / Explorer, and returns a status line (never throws).
        public static string CreateReportForPlayer()
        {
            try
            {
                string zip = CreateReport();
                Reveal(zip);
                return "Saved " + Redact(zip) + "  (path copied to the clipboard)";
            }
            catch (Exception e)
            {
                UnityEngine.Debug.LogWarning("[Reflex] create report: " + e);
                return "Could not create the report: " + e.Message;
            }
        }

        /// Writes the support ZIP and copies its path to the clipboard; returns the path.
        public static string CreateReport(string folder = null)
        {
            string dir = ReportFolder(folder);
            string zipPath = Path.Combine(dir, "Reflex-Report-" + DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + ".zip");
            var included = new List<string>();
            string logDir = LogDir;
            using (var fs = new FileStream(zipPath, FileMode.Create, FileAccess.Write))
            using (var zip = new ZipArchive(fs, ZipArchiveMode.Create))
            {
                foreach (var f in Files(logDir, "session-*.log").Take(2)) AddText(zip, "sessions/" + f.Name, f.FullName, included);

                foreach (var f in Files(logDir, "unclean-*.txt").Take(10)) AddText(zip, "sessions/" + f.Name, f.FullName, included);

                var crashTxt = Files(logDir, "crash-*.txt").ToList();
                if (ReflexCrashLog.PreviousCrashReport != null)
                    crashTxt = crashTxt.OrderBy(f => f.FullName == ReflexCrashLog.PreviousCrashReport ? 0 : 1).ToList();
                foreach (var f in crashTxt.Take(3))
                {
                    AddText(zip, "crashes/" + f.Name, f.FullName, included);
                    string dmp = Path.ChangeExtension(f.FullName, ".dmp");
                    if (File.Exists(dmp)) AddBinary(zip, "crashes/" + Path.GetFileName(dmp), dmp, included);
                }
                foreach (var f in Files(logDir, "mono_crash.*.json").Take(2)) AddText(zip, "crashes/" + f.Name, f.FullName, included);

                var unityLogs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                if (!Application.isEditor)
                    foreach (var d in new[] { ReflexCrashLog.PlayerLogDir(), Mac ? logDir : Application.persistentDataPath }.Distinct())
                        foreach (var n in new[] { "Player.log", "Player-prev.log" })
                        {
                            string p = Path.Combine(d, n);
                            if (File.Exists(p) && unityLogs.Add(Path.GetFullPath(p))) AddText(zip, "unity/" + n, p, included);
                        }
                try
                {
                    string cur = Application.consoleLogPath; // a -logFile run logs elsewhere; the Editor: Editor.log
                    if (!string.IsNullOrEmpty(cur) && File.Exists(cur) && unityLogs.Add(Path.GetFullPath(cur)))
                        AddText(zip, "unity/" + Path.GetFileName(cur), cur, included);
                }
                catch (Exception) { }

                if (Windows)
                {
                    string crashes = Path.Combine(Path.GetTempPath(), Application.companyName, Application.productName, "Crashes");
                    var newest = Directory.Exists(crashes)
                        ? new DirectoryInfo(crashes).GetDirectories().OrderByDescending(d => d.LastWriteTimeUtc).FirstOrDefault() : null;
                    if (newest != null)
                        foreach (var f in newest.GetFiles())
                        {
                            string entry = "unity/" + newest.Name + "/" + f.Name;
                            string ext = f.Extension.ToLowerInvariant();
                            if (ext == ".dmp") AddBinary(zip, entry, f.FullName, included);
                            else if (ext == ".log" || ext == ".txt" || ext == ".xml") AddText(zip, entry, f.FullName, included);
                        }
                }
                if (Mac && !Application.isEditor)
                {
                    string diag = Path.Combine(Home, "Library", "Logs", "DiagnosticReports");
                    if (Directory.Exists(diag))
                        foreach (var f in new DirectoryInfo(diag).GetFiles(Application.productName + "*")
                                     .Where(f => f.LastWriteTimeUtc > DateTime.UtcNow.AddDays(-14)).OrderByDescending(f => f.LastWriteTimeUtc).Take(2))
                            AddText(zip, "unity/" + f.Name, f.FullName, included);
                }

                var info = new StringBuilder();
                info.Append(ReflexCrashLog.SystemHeader("support report " + Path.GetFileName(zipPath)));
                try { info.Append(ReflexCrashLog.GameState()); } catch (Exception e) { info.Append("game state: ").Append(e.Message).Append('\n'); }
                info.Append("\nprevious session: ").Append(ReflexCrashLog.PreviousCrashed
                    ? ReflexCrashLog.PreviousSession + " ended unexpectedly" + (ReflexCrashLog.PreviousCrashReport != null ? " (native crash report included)" : "")
                    : Application.isEditor ? "(Editor)" : "quit normally").Append('\n');
                info.Append("current session log: ").Append(string.IsNullOrEmpty(ReflexCrashLog.SessionLogPath) ? "(none: Editor)" : Redact(ReflexCrashLog.SessionLogPath)).Append('\n');
                info.Append("\nfiles in this report:\n");
                foreach (var f in included) info.Append("  ").Append(f).Append('\n');
                info.Append("\nPrivacy: the home folder is shown as ~, user names are masked, passwords, tokens and keys are removed.\n");
                var e0 = zip.CreateEntry("system-info.txt", System.IO.Compression.CompressionLevel.Optimal);
                using (var w = new StreamWriter(e0.Open(), new UTF8Encoding(false))) w.Write(Redact(info.ToString()));
            }
            LastReport = zipPath;
            try { GUIUtility.systemCopyBuffer = zipPath; } catch (Exception) { }
            ReflexCrashLog.WriteLine($"support report written: {Redact(zipPath)} ({included.Count} files)");
            return zipPath;
        }

        static IEnumerable<FileInfo> Files(string dir, string pattern)
        {
            try { return Directory.Exists(dir) ? new DirectoryInfo(dir).GetFiles(pattern).OrderByDescending(f => f.LastWriteTimeUtc).ToList() : new List<FileInfo>(); }
            catch (Exception) { return new List<FileInfo>(); }
        }

        /// Reads a file another writer may still hold open (the running session's log, Player.log).
        static byte[] ReadShared(string path)
        {
            using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            {
                long n = Math.Min(fs.Length, MaxFileBytes);
                var b = new byte[n];
                int off = 0;
                while (off < n) { int r = fs.Read(b, off, (int)(n - off)); if (r <= 0) break; off += r; }
                if (off < n) Array.Resize(ref b, off);
                return b;
            }
        }

        static void AddText(ZipArchive zip, string entry, string path, List<string> included)
        {
            try
            {
                string text = Redact(Encoding.UTF8.GetString(ReadShared(path)));
                var e = zip.CreateEntry(entry, System.IO.Compression.CompressionLevel.Optimal);
                using (var w = new StreamWriter(e.Open(), new UTF8Encoding(false))) w.Write(text);
                included.Add(entry);
            }
            catch (Exception ex) { included.Add(entry + " (skipped: " + ex.GetType().Name + ")"); }
        }

        static void AddBinary(ZipArchive zip, string entry, string path, List<string> included)
        {
            try
            {
                var data = RedactBinary(ReadShared(path));
                var e = zip.CreateEntry(entry, System.IO.Compression.CompressionLevel.Optimal);
                using (var s = e.Open()) s.Write(data, 0, data.Length);
                included.Add(entry);
            }
            catch (Exception ex) { included.Add(entry + " (skipped: " + ex.GetType().Name + ")"); }
        }
    }
}
