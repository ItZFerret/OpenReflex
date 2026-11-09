using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Reflex.Port.Platform
{
    // One owned marker per process/session. Unknown liveness is never called a crash.
    // Stale markers are archived byte-for-byte, not deleted or rewritten.
    public sealed class SessionDiagnosticsMarkers
    {
        public enum ProcessState { Live, Dead, Unknown }
        public sealed class Previous
        {
            public string Session, EvidencePath, CrashReport;
        }
        public sealed class Scan
        {
            public Previous Previous;
            public int Live, Unknown, Clean;
        }
        readonly string directory;
        readonly Func<int, long, ProcessState> probe;
        readonly Action<string> deleteFile;
        string ownedPath, ownedText;
        public string OwnedPath => ownedPath;

        public SessionDiagnosticsMarkers(string directory, Func<int, long, ProcessState> probe = null, Action<string> deleteFile = null)
        {
            this.directory = Path.GetFullPath(directory);
            this.probe = probe ?? ProbeProcess;
            this.deleteFile = deleteFile ?? File.Delete;
        }

        public static string CreateSessionTag()
        {
            using (var process = Process.GetCurrentProcess())
                return DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + "-" + process.Id.ToString(CultureInfo.InvariantCulture) + "-" + Guid.NewGuid().ToString("N").Substring(0, 12);
        }

        public static ProcessState ProbeProcess(int pid, long startUtcTicks)
        {
            try
            {
                using (var p = Process.GetProcessById(pid))
                {
                    if (p.HasExited) return ProcessState.Dead;
                    // PID reuse is a stale owner, even if the reused PID is live.
                    return p.StartTime.ToUniversalTime().Ticks == startUtcTicks ? ProcessState.Live : ProcessState.Dead;
                }
            }
            catch (ArgumentException) { return ProcessState.Dead; }
            catch (InvalidOperationException) { return ProcessState.Dead; }
            catch (Exception) { return ProcessState.Unknown; } // access denied/platform restriction is not death
        }

        public Scan ClaimPrevious()
        {
            Directory.CreateDirectory(directory);
            var scan = new Scan();
            foreach (var path in Directory.GetFiles(directory, "running-*.txt").OrderByDescending(File.GetLastWriteTimeUtc))
            {
                string text;
                try { text = File.ReadAllText(path); } catch (Exception) { scan.Unknown++; continue; }
                var lines = text.Split('\n');
                int pid; long start;
                if ((lines.Length != 5 && lines.Length != 6) || lines[0] != "session-marker-v1" ||
                    !int.TryParse(lines[1], NumberStyles.None, CultureInfo.InvariantCulture, out pid) || pid <= 0 ||
                    !long.TryParse(lines[2], NumberStyles.None, CultureInfo.InvariantCulture, out start) || start <= 0 ||
                    !SafeTag(lines[3]) || !Guid.TryParseExact(lines[4], "N", out _) || (lines.Length == 6 && lines[5] != "clean"))
                { scan.Unknown++; continue; }
                if (lines.Length == 6) { scan.Clean++; continue; }
                var state = probe(pid, start);
                if (state == ProcessState.Live) { scan.Live++; continue; }
                if (state == ProcessState.Unknown) { scan.Unknown++; continue; }
                Claim(path, text, lines[3], scan);
            }
            // Legacy running.txt has no PID/start identity. Preserve it as unknown unless an actual
            // same-session native crash report exists. A live old player may still own that file.
            string legacy = Path.Combine(directory, "running.txt");
            if (File.Exists(legacy))
            {
                try
                {
                    string text = File.ReadAllText(legacy), tag = text.Trim();
                    if (SafeTag(tag) && File.Exists(Path.Combine(directory, "crash-" + tag + ".txt")))
                        Claim(legacy, text, tag, scan);
                    else scan.Unknown++;
                }
                catch (Exception) { scan.Unknown++; }
            }
            return scan;
        }

        void Claim(string path, string expected, string session, Scan scan)
        {
            // Only a successful move claims this warning; concurrent readers cannot both consume it.
            // The original bytes remain in the same diagnostic folder for support reports.
            string archived = Path.Combine(directory, "unclean-" + session + "-" + Guid.NewGuid().ToString("N") + "-" + Path.GetFileName(path));
            try
            {
                if (File.ReadAllText(path) != expected) { scan.Unknown++; return; }
                File.Move(path, archived);
                if (scan.Previous == null)
                {
                    string crash = Path.Combine(directory, "crash-" + session + ".txt");
                    scan.Previous = new Previous { Session = session, EvidencePath = archived,
                        CrashReport = File.Exists(crash) ? crash : null };
                }
            }
            catch (Exception) { scan.Unknown++; } // retain inaccessible/racing evidence, do not invent a crash
        }

        public void Begin(string session)
        {
            if (ownedPath != null) throw new InvalidOperationException("Session marker already owned");
            if (!SafeTag(session)) throw new ArgumentException("Unsafe session tag", nameof(session));
            Directory.CreateDirectory(directory);
            using (var process = Process.GetCurrentProcess())
            {
                string nonce = Guid.NewGuid().ToString("N");
                string text = "session-marker-v1\n" + process.Id.ToString(CultureInfo.InvariantCulture) + "\n" +
                    process.StartTime.ToUniversalTime().Ticks.ToString(CultureInfo.InvariantCulture) + "\n" + session + "\n" + nonce;
                string path = Path.Combine(directory, "running-" + session + "-" + process.Id.ToString(CultureInfo.InvariantCulture) + "-" + nonce + ".txt");
                using (var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
                {
                    var bytes = new UTF8Encoding(false).GetBytes(text);
                    stream.Write(bytes, 0, bytes.Length); stream.Flush();
                }
                ownedPath = path; ownedText = text;
            }
        }

        public bool CleanQuit()
        {
            if (ownedPath == null) return true;
            try
            {
                // Never delete a marker replaced by another owner. This unique filename is not shared
                // with legacy running.txt or another Begin; nonce+exact content establishes ownership.
                if (!File.Exists(ownedPath)) return false;
                string actual = File.ReadAllText(ownedPath), clean = ownedText + "\nclean";
                if (actual != ownedText && actual != clean) return false;
                // Persist clean state before deletion. A failed delete must not cause a false crash
                // on the next launch, even when the session log could not be flushed.
                if (actual != clean) File.WriteAllText(ownedPath, clean);
                deleteFile(ownedPath);
                ownedPath = null; ownedText = null;
                return true;
            }
            catch (Exception) { return false; } // retain marker on failure for diagnosis
        }

        static bool SafeTag(string tag) => !string.IsNullOrEmpty(tag) && tag.Length <= 100 &&
            tag.All(c => (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_');
    }
}
