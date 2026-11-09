// Session logs and crash capture for player builds (after Open Unleashed's MxaCrashLog; notes: _re/notes/client-platform.md).
//
// Where the files live (ReflexCrashLog.LogDir; ReflexSupport.OpenLogFolder opens it):
//   macOS    ~/Library/Logs/<company>/<product>/                (next to Unity's Player.log)
//   Windows  %USERPROFILE%\AppData\LocalLow\<company>\<product>\Logs\
//   session-<yyyyMMdd-HHmmss>.log  every Unity log message, warning, error and exception of a run (timestamps, full stacks for
//                                  errors), a header with the build stamp, the system, the graphics settings, the game folder
//                                  (home as ~) and the plugin (API version, struct check, crash handler), a block of the game's
//                                  state and a "context" line whenever the scene / event / race state changes, and the perf lines
//                                  of ReflexPerfLog
//   crash-<session>.txt            written by libreflexworld's crash handler (native/drafts/world_runtime/crash.inc)
//   mono_crash.*.json              Mono's own crash dump (macOS: the player's working directory is moved here)
//   running-<session>-<pid>-<nonce>.txt  process/start-identity owned marker; live concurrent sessions are not crashes
//   unclean-running-*.txt          retained original stale-marker evidence after one warning claim
//   running.txt                    legacy marker retained unless a same-session native crash report proves it unclean
// The last 10 sessions and 10 crashes are kept. The Editor logs nothing here (it has Editor.log; its process is never hooked).
//
// Hidden debug switches (command line only):
//   -reflexLogDir <folder>         another log folder (tests)
//   -reflexCrashTest native|abort|fpe|managed|exception   2 s after start: native = a bad write inside libreflexworld, abort =
//                                  abort() inside it, fpe = SIGFPE, managed = an unhandled C# exception then a Unity native crash,
//                                  exception = only the unhandled exception, then a normal quit
//   -reflexCreateReport [folder]   writes the support ZIP (ReflexSupport.CreateReport), logs its path and quits
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.Platform
{
    public static class ReflexCrashLog
    {
        public const int KeepSessions = 10, KeepCrashes = 10;
        const long MaxLogBytes = 32L << 20;

        /// The folder of the session logs and crash reports ("" before Init / in the Editor).
        public static string LogDir { get; private set; } = "";
        public static string SessionTag { get; private set; } = "";
        public static string SessionLogPath { get; private set; } = "";
        /// "2026-10-18 14.05 (macOS arm64, Unity 6000.6.1f1, version 0.1.0)" from the player's StreamingAssets/ReflexBuildInfo.txt.
        public static string BuildStamp { get; private set; } = "";
        public static string BuildId { get; private set; } = "";
        /// The previous session ended without a clean quit (a crash, a force quit or a kill).
        public static bool PreviousCrashed { get; private set; }
        public static string PreviousSession { get; private set; } = "";
        /// The previous session's native crash report (crash-<session>.txt), or null.
        public static string PreviousCrashReport { get; private set; }
        /// Set by a UI that showed the "closed unexpectedly" prompt and the player dismissed it.
        public static bool PromptDismissed;
        public static string LastContext { get; private set; } = "";
        public static bool Enabled => s_enabled;
        public static bool NativeHandler => s_native;

        static readonly object s_lock = new object();
        static StreamWriter s_log;
        static long s_logBytes;
        static bool s_enabled, s_native;
        static SessionDiagnosticsMarkers s_markers;
        static SessionDiagnosticsMarkers.Scan s_markerScan;
        static int s_mainThread = -1;

        static bool Mac => Application.platform == RuntimePlatform.OSXPlayer || Application.platform == RuntimePlatform.OSXEditor;

        static string Arg(string name, bool value = true)
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, name);
            if (i < 0) return null;
            if (!value) return "";
            return i + 1 < a.Length && !a[i + 1].StartsWith("-") ? a[i + 1] : "";
        }

        /// The per-OS log folder (also what a SUPPORT page in the Editor shows; the Editor writes nothing there).
        public static string DefaultLogDir()
        {
            string over = Arg("-reflexLogDir");
            if (!string.IsNullOrEmpty(over)) return Path.GetFullPath(over);
            string company = string.IsNullOrEmpty(Application.companyName) ? "DefaultCompany" : Application.companyName;
            string product = string.IsNullOrEmpty(Application.productName) ? "Reflex" : Application.productName;
            if (Mac) return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Library", "Logs", company, product);
            return Path.Combine(Application.persistentDataPath, "Logs");
        }

        /// Unity's Player.log folder (macOS: ~/Library/Logs/<company>/<product>; Windows: LocalLow\<company>\<product>).
        public static string PlayerLogDir()
        {
            try
            {
                string p = Application.consoleLogPath;
                if (!string.IsNullOrEmpty(p) && !Application.isEditor) return Path.GetDirectoryName(p);
            }
            catch (Exception) { }
            return Mac ? DefaultLogDir() : Application.persistentDataPath;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Init()
        {
            if (Application.isEditor || s_enabled) return;
            s_mainThread = System.Threading.Thread.CurrentThread.ManagedThreadId;
            try
            {
                LogDir = DefaultLogDir();
                Directory.CreateDirectory(LogDir);
                SessionTag = SessionDiagnosticsMarkers.CreateSessionTag();
                ReadBuildInfo();
                CheckPrevious();
                Prune();
                SessionLogPath = Path.Combine(LogDir, "session-" + SessionTag + ".log");
                s_log = new StreamWriter(new FileStream(SessionLogPath, FileMode.Create, FileAccess.Write, FileShare.ReadWrite), new UTF8Encoding(false)) { AutoFlush = true };
                s_enabled = true;
                WriteRaw(SystemHeader("session " + SessionTag));
                if (PreviousCrashed)
                    WriteRaw($"previous session {PreviousSession} ended unexpectedly" +
                             (PreviousCrashReport != null ? ": crash report " + ReflexSupport.Redact(PreviousCrashReport) : " (no native crash report)") + "\n\n");
                s_markers.Begin(SessionTag);
                if (s_markerScan.Live > 0 || s_markerScan.Unknown > 0 || s_markerScan.Clean > 0)
                    WriteRaw($"other session markers: {s_markerScan.Live} live, {s_markerScan.Unknown} unknown, {s_markerScan.Clean} clean retained (not reported as crashes)\n");
                Application.logMessageReceivedThreaded += OnLog;
                AppDomain.CurrentDomain.UnhandledException += OnUnhandled;
                System.Threading.Tasks.TaskScheduler.UnobservedTaskException += (s, e) => Exception("unobserved task exception", e.Exception);
                Application.quitting += OnQuit;
                SceneManager.sceneLoaded += (sc, mode) => { if (mode == LoadSceneMode.Single) UpdateContext(true, LastContext.Length > 0); };
                MoveWorkingDirectory();
                InstallNative();
                var go = new GameObject("ReflexCrashLog") { hideFlags = HideFlags.HideAndDontSave };
                UnityEngine.Object.DontDestroyOnLoad(go);
                go.AddComponent<ReflexCrashLogBehaviour>();
            }
            catch (Exception e)
            {
                Debug.LogWarning("[Reflex] crash log: could not start session logging in " + LogDir + ": " + e.Message);
            }
        }

        static void ReadBuildInfo()
        {
            string info = Path.Combine(Application.streamingAssetsPath, ReflexBuildInfoFile);
            string stamp = null, platform = null;
            try
            {
                if (File.Exists(info))
                    foreach (var line in File.ReadAllLines(info))
                    {
                        int eq = line.IndexOf('=');
                        if (eq <= 0) continue;
                        string k = line.Substring(0, eq).Trim(), v = line.Substring(eq + 1).Trim();
                        if (k == "stamp") stamp = v;
                        else if (k == "platform") platform = v;
                        else if (k == "id") BuildId = v;
                    }
            }
            catch (Exception) { }
            if (string.IsNullOrEmpty(BuildId)) BuildId = Application.buildGUID;
            BuildStamp = (stamp ?? "unstamped build") + " (" + (platform ?? Application.platform.ToString()) + ", Unity " + Application.unityVersion +
                         ", version " + Application.version + ")";
        }

        /// The build stamp's file name under StreamingAssets (written by Editor/Build/ReflexBuildInfo.cs into the player only).
        public const string ReflexBuildInfoFile = "ReflexBuildInfo.txt";

        /// A confirmed dead owner is unclean; live/unknown/legacy-unowned markers are preserved.
        static void CheckPrevious()
        {
            s_markers = new SessionDiagnosticsMarkers(LogDir);
            s_markerScan = s_markers.ClaimPrevious();
            var prior = s_markerScan.Previous;
            if (prior == null) return;
            PreviousSession = prior.Session;
            PreviousCrashed = true;
            PreviousCrashReport = prior.CrashReport;
        }

        static void Prune()
        {
            void Keep(string pattern, int n)
            {
                foreach (var f in new DirectoryInfo(LogDir).GetFiles(pattern).OrderByDescending(f => f.LastWriteTimeUtc).Skip(n))
                    try { f.Delete(); } catch (Exception) { }
            }
            try
            {
                Keep("session-*.log", KeepSessions - 1);
                Keep("crash-*.txt", KeepCrashes);
                Keep("crash-*.dmp", KeepCrashes);
                Keep("mono_crash.*", KeepCrashes);
            }
            catch (Exception) { }
        }

        /// macOS: Mono writes mono_crash.*.json into the working directory ("/" from Finder: read-only). The player's working
        /// directory becomes the log folder, unless the command line passes a relative path that exists.
        static void MoveWorkingDirectory()
        {
            if (!Mac) return;
            try
            {
                string cwd = Directory.GetCurrentDirectory();
                var args = Environment.GetCommandLineArgs();
                for (int i = 1; i < args.Length; i++)
                {
                    string a = args[i];
                    if (a.Length == 0 || a.StartsWith("-") || Path.IsPathRooted(a) || (a.IndexOf('/') < 0 && a.IndexOf('.') < 0)) continue;
                    if (File.Exists(Path.Combine(cwd, a)) || Directory.Exists(Path.Combine(cwd, a)) ||
                        (a.Contains("/") && Directory.Exists(Path.GetDirectoryName(Path.Combine(cwd, a)))))
                    {
                        WriteRaw($"working directory kept ({ReflexSupport.Redact(cwd)}): relative path \"{a}\" on the command line\n");
                        return;
                    }
                }
                Directory.SetCurrentDirectory(LogDir);
            }
            catch (Exception) { }
        }

        // -------------------------------------------------------------------------------------------- the session log

        static string Now => DateTime.Now.ToString("HH:mm:ss.fff", CultureInfo.InvariantCulture);

        static void OnLog(string message, string stack, LogType type)
        {
            if (!s_enabled) return;
            var sb = new StringBuilder(message.Length + 64);
            sb.Append('[').Append(Now).Append("] ");
            sb.Append(type == LogType.Log ? "" : type == LogType.Warning ? "WARNING " : type == LogType.Exception ? "EXCEPTION " :
                      type == LogType.Assert ? "ASSERT " : "ERROR ");
            sb.Append(message);
            if (type != LogType.Log && type != LogType.Warning && !string.IsNullOrEmpty(stack))
            {
                sb.Append('\n');
                foreach (var line in stack.TrimEnd().Split('\n')) sb.Append("    ").Append(line.TrimEnd('\r')).Append('\n');
            }
            else sb.Append('\n');
            WriteRaw(sb.ToString());
        }

        static void OnUnhandled(object sender, UnhandledExceptionEventArgs e) =>
            Exception(e.IsTerminating ? "unhandled exception (terminating)" : "unhandled exception", e.ExceptionObject as Exception);

        static void Exception(string what, Exception ex) => WriteRaw($"[{Now}] EXCEPTION {what}: {ex}\n");

        /// Appends text to the session log (thread-safe; nothing before Init, in the Editor, or past 32 MB).
        public static void WriteRaw(string text)
        {
            if (s_log == null) return;
            lock (s_lock)
            {
                if (s_log == null || s_logBytes > MaxLogBytes) return;
                try
                {
                    s_log.Write(text);
                    s_logBytes += text.Length;
                    if (s_logBytes > MaxLogBytes) s_log.Write("\n... session log truncated at 32 MB\n");
                }
                catch (Exception) { }
            }
        }

        /// A timestamped line in the session log ("[hh:mm:ss.fff] text").
        public static void WriteLine(string text) => WriteRaw("[" + Now + "] " + text + "\n");

        static void OnQuit()
        {
            WriteRaw($"[{Now}] clean exit\n");
            if (s_markers != null && !s_markers.CleanQuit())
                WriteRaw($"[{Now}] owned session marker retained: cleanup failed or ownership changed\n");
            lock (s_lock) { try { s_log?.Flush(); } catch (Exception) { } }
        }

        // -------------------------------------------------------------------------------------------- header + context

        static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

        /// The system / build / settings block of the session log and of the report's system-info.txt.
        public static string SystemHeader(string title)
        {
            var sb = new StringBuilder();
            sb.Append("MX vs ATV Reflex (Unity port) - ").Append(title).Append('\n');
            sb.Append("started:   ").Append(DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss zzz", CultureInfo.InvariantCulture)).Append('\n');
            sb.Append("build:     ").Append(string.IsNullOrEmpty(BuildStamp) ? (Application.isEditor ? "Editor" : "?") : BuildStamp)
              .Append(", id ").Append(string.IsNullOrEmpty(BuildId) ? "-" : BuildId).Append('\n');
            sb.Append("os:        ").Append(SystemInfo.operatingSystem).Append(" (").Append(SystemInfo.operatingSystemFamily).Append(")\n");
            sb.Append("cpu:       ").Append(SystemInfo.processorType).Append(Inv($", {SystemInfo.processorCount} threads, {SystemInfo.processorFrequency} MHz\n"));
            sb.Append("ram:       ").Append(Inv($"{SystemInfo.systemMemorySize} MB\n"));
            sb.Append("gpu:       ").Append(SystemInfo.graphicsDeviceName).Append(" (").Append(SystemInfo.graphicsDeviceVendor).Append("), ")
              .Append(SystemInfo.graphicsDeviceType).Append(", ").Append(SystemInfo.graphicsDeviceVersion).Append(Inv($", {SystemInfo.graphicsMemorySize} MB VRAM\n"));
            sb.Append("display:   ").Append(Inv($"{Screen.width} x {Screen.height} {Screen.fullScreenMode}, {Screen.currentResolution.refreshRateRatio.value:0.#} Hz, vsync {QualitySettings.vSyncCount}"))
              .Append(", quality level ").Append(SafeQualityName()).Append(Application.isBatchMode ? ", batch mode (headless)" : "")
              .Append(Time.frameCount < 1 && !Application.isEditor ? " (at start-up, before the window opened)" : "").Append('\n');
            sb.Append("logs:      ").Append(ReflexSupport.Redact(string.IsNullOrEmpty(LogDir) ? DefaultLogDir() : LogDir)).Append('\n');
            return sb.ToString() + "\n";
        }

        static string SafeQualityName()
        {
            try { return QualitySettings.names[QualitySettings.GetQualityLevel()]; } catch (Exception) { return "?"; }
        }

        /// The game's own state (main thread): game folder, the plugin, the scene, the race.
        public static string GameState()
        {
            var sb = new StringBuilder();
            void Line(string k, Func<string> v)
            {
                string s;
                try { s = v(); } catch (Exception e) { s = "(" + e.GetType().Name + ")"; }
                sb.Append(k.PadRight(11)).Append(s).Append('\n');
            }
            Line("game dir:", () => { string d = Reflex.Port.Content.ReflexInstall.Dir; return string.IsNullOrEmpty(d) ? "(not found)" : ReflexSupport.Redact(d); });
            Line("plugin:", PluginLine);
            Line("jit:", () => Environment.GetEnvironmentVariable("REFLEX_NO_JIT") == "1" ? "off (REFLEX_NO_JIT=1)" : "on");
            Line("scene:", () => SceneManager.GetActiveScene().name);
            Line("race:", RaceContext);
            return sb.ToString();
        }

        static string PluginLine()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
                return "OPEN REFLEX core; legacy crash handler unavailable";
            var c = ReflexWorldNative.Check();
            return (c.Compatible ? "" : "INCOMPATIBLE: ") + c.Summary + (s_native ? ", crash handler installed" : ReflexWorldNative.HasCrashHandler ? "" : ", no crash handler (older plugin)");
        }

        static OriginalWorldRuntime s_runtime;
        static float s_nextLook = -1f;

        /// The race's world runtime, if a race scene is loaded (looked up at most every half second while there is none).
        public static OriginalWorldRuntime Runtime
        {
            get
            {
                if (s_runtime) return s_runtime;
                float now = Time.realtimeSinceStartup;
                if (now < s_nextLook) return null;
                s_nextLook = now + 0.5f;
                return s_runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            }
        }

        static string RaceContext()
        {
            var r = Runtime;
            if (!r) return "-";
            var race = r.Race;
            string state = !r.Running ? "not running" : r.paused ? "paused" : race.valid == 0 ? "free ride" : "state " + race.state + Inv($", lap {race.laps_completed}/{race.num_laps}, place {race.place}/{race.riders}");
            return $"{(string.IsNullOrEmpty(r.level) ? "?" : r.level)}, {r.opponents} opponents, AI {r.aiSkill}, laps {r.laps}, pacing {r.Pacing}, {state}";
        }

        /// Called every second and on scene loads: a "context" line and the native crash context when it changed.
        internal static void UpdateContext(bool force = false, bool state = false)
        {
            if (!s_enabled) return;
            string scene = SceneManager.GetActiveScene().name, race = "-";
            try { race = RaceContext(); } catch (Exception) { }
            string ctx = scene + " | " + race;
            if (!force && ctx == LastContext) return;
            bool first = LastContext.Length == 0;
            LastContext = ctx;
            if (first || state) WriteRaw("\n" + GameState() + "\n");
            WriteLine($"context: scene {scene}; race {race}");
            if (s_native)
            {
                ReflexWorldNative.CrashContext("scene", scene);
                ReflexWorldNative.CrashContext("race", race);
            }
        }

        // -------------------------------------------------------------------------------------------- native

        /// The plugin check and its crash handler, once at start (the plugin is loaded here, before any scene uses it).
        static void InstallNative()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                WriteLine("simulation: OPEN REFLEX core; managed session logging enabled");
                return;
            }
            var c = ReflexWorldNative.Check();
            WriteLine("plugin: " + (c.Compatible ? "" : "INCOMPATIBLE: ") + c.Summary);
            if (!c.Compatible && !c.Loaded) return;
            s_native = ReflexWorldNative.CrashInstall(LogDir, SessionTag, BuildStamp + ", id " + BuildId, out string status);
            WriteLine($"native crash handler {(s_native ? "installed" : "NOT installed")} ({status})");
            if (s_native)
            {
                ReflexWorldNative.CrashContext("build", BuildStamp + ", id " + BuildId);
                ReflexWorldNative.CrashContext("plugin", c.Summary);
            }
        }

        /// A breadcrumb in the plugin's ring of recent notes (shown in a native crash report). Any owner may call it.
        public static void Note(string text) { if (s_native) ReflexWorldNative.CrashNote(text); }

        /// A context key / value shown in a native crash report (16 keys; an empty value removes it). Any owner may call it.
        public static void Context(string key, string value) { if (s_native) ReflexWorldNative.CrashContext(key, value); }

        internal static bool OnMainThread => System.Threading.Thread.CurrentThread.ManagedThreadId == s_mainThread;

        // -------------------------------------------------------------------------------------------- debug switches

        internal static IEnumerator DebugSwitches()
        {
            string crash = Arg("-reflexCrashTest"), report = Arg("-reflexCreateReport");
            if (crash == null && report == null) yield break;
            Application.runInBackground = true;   // the switch must fire even when the player is not the frontmost app
            float until = Time.realtimeSinceStartup + 2f;
            while (Time.realtimeSinceStartup < until) yield return null;
            if (report != null)
            {
                string zip = null;
                try { zip = ReflexSupport.CreateReport(report.Length > 0 ? report : null); }
                catch (Exception e) { Debug.LogError("[Reflex] create report failed: " + e); }
                Debug.Log("[Reflex] support report: " + (zip ?? "(failed)"));
                if (crash == null) { Application.Quit(zip != null ? 0 : 1); yield break; }
            }
            string kind = string.IsNullOrEmpty(crash) ? "native" : crash.ToLowerInvariant();
            Debug.Log("[Reflex] crash test: " + kind);
            if (kind == "native" || kind == "abort" || kind == "fpe")
            {
                Note("crash test " + kind);
                if (!ReflexWorldNative.CrashTest(kind == "abort" ? 2 : kind == "fpe" ? 3 : 1))
                {
                    Debug.LogError("[Reflex] crash test: the plugin has no reflex_world_crash_test");
                    Application.Quit(1);
                }
            }
            else
            {
                yield return null;
                ReflexCrashLogBehaviour.ThrowNext = true; // an unhandled exception from Update (full stack in the log)
                yield return null;
                yield return null;
                if (kind == "managed") UnityEngine.Diagnostics.Utils.ForceCrash(UnityEngine.Diagnostics.ForcedCrashCategory.AccessViolation);
                else Application.Quit(0);
            }
        }
    }

    /// The per-second context poll and the debug switches.
    sealed class ReflexCrashLogBehaviour : MonoBehaviour
    {
        internal static bool ThrowNext;
        float _next;

        void Start()
        {
            ReflexCrashLog.UpdateContext(true);
            StartCoroutine(ReflexCrashLog.DebugSwitches());
        }

        void Update()
        {
            if (ThrowNext) { ThrowNext = false; throw new InvalidOperationException("Reflex crash test: an unhandled managed exception"); }
            if (Time.realtimeSinceStartup < _next) return;
            _next = Time.realtimeSinceStartup + 1f;
            ReflexCrashLog.UpdateContext();
        }
    }
}
