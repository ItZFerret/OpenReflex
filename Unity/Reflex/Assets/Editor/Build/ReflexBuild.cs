// The player build entry (after Open Unleashed's MxaBuild; notes: _re/notes/client-platform.md). Local builds only: nothing here
// uploads or publishes.
//
//   Menu   Reflex > Build > macOS Player               REFLEX/builds/macOS/<product>.app
//          Reflex > Build > macOS Player (development) the same with a development player (profiler, script debugging)
//   Batch  Unity -batchmode -quit -projectPath <project> -executeMethod Reflex.Port.EditorTools.ReflexBuild.BuildMacBatch
//                [-reflexBuildOut <path.app>] [-reflexDevelopment] [-reflexSymbolsDir <dir>]
//          logs "[Reflex] build Succeeded: ..." / "[Reflex] build Failed: ..." and exits 0 / 1 (REFLEX/scripts/build_test.sh).
//
// The player: both scenes, FrontEnd first (build index 0) then Reflex (the race; ReflexEventSelect.Launch loads it by name), the
// plugins of Assets/Plugins/macOS (libreflexworld + libcapstone), arm64 only (the plugin is arm64-only). Nothing of the game
// is in the player: it reads the install at run time (ReflexInstall); the build fails if the player's StreamingAssets holds
// anything but the build stamp. Frame Timing Stats are turned on for the build (ReflexPerfLog's CPU / GPU times) and the
// project's settings are restored afterwards. ReflexBuildInfo stamps the player and keeps the plugin's symbols.
//
// Windows x64 (_re/notes/client-windows.md; built from a clone by REFLEX/scripts/package_windows_build.sh, which also puts the
// MinGW reflexworld.dll into the clone's Assets/Plugins/x86_64 -- the user's project never holds it):
//   Menu   Reflex > Build > Windows x64 Player (+ development)   REFLEX/builds/windows/<product>/<product>.exe
//   Batch  ... -buildTarget Win64 -executeMethod Reflex.Port.EditorTools.ReflexBuild.BuildWindowsBatch
//                [-reflexBuildOut <dir/product.exe>] [-reflexDevelopment] [-reflexSymbolsDir <dir>] [-reflexPluginDebug <reflexworld.dll.debug>]
//   The plugin's import settings are set here (ConfigureWindowsPlugins): reflexworld.dll Windows x64 player + Windows Editor only,
//   the macOS dylibs never in a Windows player. The JIT is off on Windows (the DLL's jit.cpp is the interpreter-only stub).
using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace Reflex.Port.EditorTools
{
    public static class ReflexBuild
    {
        public static readonly string[] Scenes = { "Assets/Scenes/FrontEnd.unity", "Assets/Scenes/Reflex.unity" };
        public static readonly string[] Plugins = { "Assets/Plugins/macOS/libreflexworld.dylib", "Assets/Plugins/macOS/libcapstone.5.dylib" };
        /// The Windows plugin (capstone and zlib are linked statically into it).
        public const string WindowsPlugin = "Assets/Plugins/x86_64/reflexworld.dll";

        /// REFLEX/ (REFLEX_REPO, else three folders above Assets: REFLEX/Unity/Reflex/Assets).
        public static string RepoRoot
        {
            get
            {
                string env = Environment.GetEnvironmentVariable("REFLEX_REPO");
                if (!string.IsNullOrEmpty(env) && Directory.Exists(env)) return Path.GetFullPath(env);
                return Path.GetFullPath(Path.Combine(Application.dataPath, "..", "..", ".."));
            }
        }

        public static string DefaultMacOutput => Path.Combine(RepoRoot, "builds", "macOS", PlayerSettings.productName + ".app");
        public static string DefaultWindowsOutput => Path.Combine(RepoRoot, "builds", "windows", PlayerSettings.productName, PlayerSettings.productName + ".exe");

        [MenuItem("Reflex/Build/macOS Player")]
        static void MenuMac() => Reveal(BuildMac(DefaultMacOutput, false));

        [MenuItem("Reflex/Build/macOS Player (development)")]
        static void MenuMacDev() => Reveal(BuildMac(DefaultMacOutput, true));

        [MenuItem("Reflex/Build/Windows x64 Player")]
        static void MenuWindows() => Reveal(BuildWindows(DefaultWindowsOutput, false));

        [MenuItem("Reflex/Build/Windows x64 Player (development)")]
        static void MenuWindowsDev() => Reveal(BuildWindows(DefaultWindowsOutput, true));

        static void Reveal(BuildReport r)
        {
            if (r != null && r.summary.result == BuildResult.Succeeded) EditorUtility.RevealInFinder(r.summary.outputPath);
        }

        static string Arg(string name)
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, name);
            return i >= 0 && i + 1 < a.Length && !a[i + 1].StartsWith("-") ? a[i + 1] : null;
        }

        /// -executeMethod entry (scripts/build_test.sh). Exits the Editor with 0 / 1.
        public static void BuildMacBatch()
        {
            string output = Arg("-reflexBuildOut") ?? DefaultMacOutput;
            bool dev = Environment.GetCommandLineArgs().Contains("-reflexDevelopment");
            BuildReport r = null;
            try { r = BuildMac(output, dev); }
            catch (Exception e) { Debug.LogError("[Reflex] build Failed: " + e); }
            EditorApplication.Exit(r != null && r.summary.result == BuildResult.Succeeded ? 0 : 1);
        }

        public static BuildReport BuildMac(string output, bool development)
        {
            foreach (var s in Scenes)
                if (!File.Exists(s)) { Debug.LogError("[Reflex] build Failed: missing scene " + s); return null; }
            foreach (var p in Plugins)
                if (!File.Exists(p)) { Debug.LogError("[Reflex] build Failed: missing plugin " + p); return null; }
            string uuid = ReflexBuildInfo.MachOUuid(Plugins[0]);
            Debug.Log($"[Reflex] build: {output} ({(development ? "development" : "release")}), scenes {string.Join(", ", Scenes)}, plugin {uuid ?? "?"}");

            bool timing = PlayerSettings.enableFrameTimingStats;
            object arch = GetMacArchitecture();
            try
            {
                PlayerSettings.enableFrameTimingStats = true;
                SetMacArchitecture("ARM64");
                Directory.CreateDirectory(Path.GetDirectoryName(output));
                var options = new BuildPlayerOptions
                {
                    scenes = Scenes, locationPathName = output, target = BuildTarget.StandaloneOSX, targetGroup = BuildTargetGroup.Standalone,
                    options = development ? BuildOptions.Development : BuildOptions.None,
                };
                ReflexBuildInfo.KeepSymbolsForThisBuild = true;
                var r = BuildPipeline.BuildPlayer(options);
                var sum = r.summary;
                string problem = sum.result == BuildResult.Succeeded ? CheckPlayer(output) : null;
                if (problem != null) Debug.LogError("[Reflex] build Failed: " + problem);
                else
                    Debug.Log($"[Reflex] build {sum.result}: {output}, {sum.totalSize / 1e6:0} MB, {sum.totalErrors} errors, {sum.totalTime.TotalSeconds:0} s, " +
                              $"stamp {ReflexBuildInfo.LastId}, symbols {(ReflexBuildInfo.LastSymbols.Length > 0 ? ReflexBuildInfo.LastSymbols : "(not kept)")}");
                return problem == null ? r : null;
            }
            finally
            {
                ReflexBuildInfo.KeepSymbolsForThisBuild = false;
                PlayerSettings.enableFrameTimingStats = timing;
                if (arch != null) SetMacArchitecture(arch.ToString());
            }
        }

        /// -executeMethod entry (scripts/package_windows_build.sh; start the Editor with -buildTarget Win64). Exits with 0 / 1.
        public static void BuildWindowsBatch()
        {
            string output = Arg("-reflexBuildOut") ?? DefaultWindowsOutput;
            bool dev = Environment.GetCommandLineArgs().Contains("-reflexDevelopment");
            BuildReport r = null;
            try { r = BuildWindows(output, dev); }
            catch (Exception e) { Debug.LogError("[Reflex] build Failed: " + e); }
            EditorApplication.Exit(r != null && r.summary.result == BuildResult.Succeeded ? 0 : 1);
        }

        /// A Windows x64 player at <output> (a path ending in .exe; Unity puts <name>_Data, UnityPlayer.dll, ... beside it).
        public static BuildReport BuildWindows(string output, bool development)
        {
            if (!output.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)) { Debug.LogError("[Reflex] build Failed: the Windows output must be an .exe path: " + output); return null; }
            foreach (var s in Scenes)
                if (!File.Exists(s)) { Debug.LogError("[Reflex] build Failed: missing scene " + s); return null; }
            if (!File.Exists(WindowsPlugin))
            {
                Debug.LogError("[Reflex] build Failed: missing plugin " + WindowsPlugin + " (scripts/windows/build_plugin_windows.sh; scripts/package_windows_build.sh copies it into its build clone)");
                return null;
            }
            if (!ConfigureWindowsPlugins()) return null;
            string id = ReflexBuildInfo.PeId(WindowsPlugin);
            Debug.Log($"[Reflex] build: {output} (Windows x64, {(development ? "development" : "release")}), scenes {string.Join(", ", Scenes)}, plugin {id ?? "?"}");

            bool timing = PlayerSettings.enableFrameTimingStats;
            object arch = GetArch(WinArchType);
            try
            {
                PlayerSettings.enableFrameTimingStats = true;
                SetArch(WinArchType, "x64");
                Directory.CreateDirectory(Path.GetDirectoryName(output));
                var options = new BuildPlayerOptions
                {
                    scenes = Scenes, locationPathName = output, target = BuildTarget.StandaloneWindows64, targetGroup = BuildTargetGroup.Standalone,
                    options = development ? BuildOptions.Development : BuildOptions.None,
                };
                ReflexBuildInfo.KeepSymbolsForThisBuild = true;
                var r = BuildPipeline.BuildPlayer(options);
                var sum = r.summary;
                string problem = sum.result == BuildResult.Succeeded ? CheckWindowsPlayer(output) : null;
                if (problem != null) Debug.LogError("[Reflex] build Failed: " + problem);
                else
                    Debug.Log($"[Reflex] build {sum.result}: {output}, {sum.totalSize / 1e6:0} MB, {sum.totalErrors} errors, {sum.totalTime.TotalSeconds:0} s, " +
                              $"stamp {ReflexBuildInfo.LastId}, symbols {(ReflexBuildInfo.LastSymbols.Length > 0 ? ReflexBuildInfo.LastSymbols : "(not kept)")}");
                return problem == null ? r : null;
            }
            finally
            {
                ReflexBuildInfo.KeepSymbolsForThisBuild = false;
                PlayerSettings.enableFrameTimingStats = timing;
                if (arch != null) SetArch(WinArchType, arch.ToString());
            }
        }

        /// Import settings: reflexworld.dll for the Windows x64 player and the Windows Editor only; the macOS dylibs never in a
        /// Windows player. Writes a .meta only when a setting differs.
        public static bool ConfigureWindowsPlugins()
        {
            var dll = AssetImporter.GetAtPath(WindowsPlugin) as PluginImporter;
            if (dll == null) { Debug.LogError("[Reflex] build Failed: " + WindowsPlugin + " is not imported as a native plugin"); return false; }
            bool changed = false;
            void Platform(PluginImporter imp, BuildTarget t, bool on) { if (imp.GetCompatibleWithPlatform(t) != on) { imp.SetCompatibleWithPlatform(t, on); changed = true; } }
            void Data(PluginImporter imp, BuildTarget t, string key, string v) { if (imp.GetPlatformData(t, key) != v) { imp.SetPlatformData(t, key, v); changed = true; } }
            void EditorData(PluginImporter imp, string key, string v) { if (imp.GetEditorData(key) != v) { imp.SetEditorData(key, v); changed = true; } }
            if (dll.GetCompatibleWithAnyPlatform()) { dll.SetCompatibleWithAnyPlatform(false); changed = true; }
            Platform(dll, BuildTarget.StandaloneWindows64, true);
            Data(dll, BuildTarget.StandaloneWindows64, "CPU", "x86_64");
            Platform(dll, BuildTarget.StandaloneWindows, false);
            Platform(dll, BuildTarget.StandaloneOSX, false);
            Platform(dll, BuildTarget.StandaloneLinux64, false);
            if (!dll.GetCompatibleWithEditor()) { dll.SetCompatibleWithEditor(true); changed = true; }
            EditorData(dll, "OS", "Windows");
            EditorData(dll, "CPU", "x86_64");
            if (changed) { dll.SaveAndReimport(); Debug.Log("[Reflex] build: import settings of " + WindowsPlugin + " set (Windows x64 player + Windows Editor)"); }
            foreach (var p in Plugins)
            {
                changed = false;
                var imp = AssetImporter.GetAtPath(p) as PluginImporter;
                if (imp == null) continue;
                if (imp.GetCompatibleWithAnyPlatform()) { imp.SetCompatibleWithAnyPlatform(false); imp.SetCompatibleWithPlatform(BuildTarget.StandaloneOSX, true); imp.SetCompatibleWithEditor(true); changed = true; }
                Platform(imp, BuildTarget.StandaloneWindows64, false);
                Platform(imp, BuildTarget.StandaloneWindows, false);
                if (changed) { imp.SaveAndReimport(); Debug.Log("[Reflex] build: " + p + " excluded from Windows players"); }
            }
            return true;
        }

        /// The Windows player holds reflexworld.dll under <name>_Data/Plugins/x86_64, no macOS plugin and no game data; null when fine.
        static string CheckWindowsPlayer(string exe)
        {
            string data = Path.Combine(Path.GetDirectoryName(exe), Path.GetFileNameWithoutExtension(exe) + "_Data");
            string plugins = Path.Combine(data, "Plugins");
            if (!File.Exists(Path.Combine(plugins, "x86_64", "reflexworld.dll")))
                return "the player has no reflexworld.dll under " + Path.GetFileName(data) + "/Plugins/x86_64 (OriginalWorldRuntime looks there)";
            if (Directory.Exists(plugins))
            {
                var mac = Directory.GetFiles(plugins, "*.dylib", SearchOption.AllDirectories);
                if (mac.Length > 0) return "a macOS plugin is in the Windows player: " + string.Join(", ", mac.Select(Path.GetFileName));
            }
            string sa = Path.Combine(data, "StreamingAssets");
            if (Directory.Exists(sa))
            {
                var extra = Directory.GetFiles(sa, "*", SearchOption.AllDirectories).Where(f => Path.GetFileName(f) != "ReflexBuildInfo.txt").ToArray();
                if (extra.Length > 0) return "StreamingAssets holds files other than the build stamp (no game data may ship): " + string.Join(", ", extra.Take(5).Select(Path.GetFileName));
            }
            return null;
        }

        // UnityEditor.WindowsStandalone.UserBuildSettings.architecture (x64 / ARM64) lives in the Windows build-support module.
        const string WinArchType = "UnityEditor.WindowsStandalone.UserBuildSettings";

        static System.Reflection.PropertyInfo ArchProperty(string type)
        {
            var t = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(type, false)).FirstOrDefault(x => x != null);
            return t?.GetProperty("architecture", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
        }

        static object GetArch(string type) { try { return ArchProperty(type)?.GetValue(null); } catch (Exception) { return null; } }

        static void SetArch(string type, string value)
        {
            try
            {
                var p = ArchProperty(type);
                if (p == null) { Debug.LogWarning("[Reflex] build: " + type + ".architecture not found (default architecture)"); return; }
                p.SetValue(null, Enum.Parse(p.PropertyType, value));
            }
            catch (Exception e) { Debug.LogWarning("[Reflex] build: could not set the architecture: " + e.Message); }
        }

        /// The player holds the plugin, both scenes and no game data; null when it is fine.
        static string CheckPlayer(string app)
        {
            string plugins = Path.Combine(app, "Contents", "PlugIns");
            if (!Directory.Exists(plugins) || Directory.GetFiles(plugins, "libreflexworld.dylib", SearchOption.AllDirectories).Length == 0)
                return "the player has no libreflexworld.dylib under Contents/PlugIns";
            if (Directory.GetFiles(plugins, "libcapstone.5.dylib", SearchOption.AllDirectories).Length == 0)
                return "the player has no libcapstone.5.dylib under Contents/PlugIns";
            string sa = Path.Combine(app, "Contents", "Resources", "Data", "StreamingAssets");
            if (Directory.Exists(sa))
            {
                var extra = Directory.GetFiles(sa, "*", SearchOption.AllDirectories).Where(f => Path.GetFileName(f) != "ReflexBuildInfo.txt" && !f.EndsWith(".DS_Store")).ToArray();
                if (extra.Length > 0) return "StreamingAssets holds files other than the build stamp (no game data may ship): " + string.Join(", ", extra.Take(5).Select(Path.GetFileName));
            }
            return null;
        }

        // UnityEditor.OSXStandalone.UserBuildSettings.architecture lives in the macOS build-support module: by reflection, so this
        // compiles without it.
        static System.Reflection.PropertyInfo ArchProperty()
        {
            var t = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("UnityEditor.OSXStandalone.UserBuildSettings", false)).FirstOrDefault(x => x != null);
            return t?.GetProperty("architecture", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
        }

        static object GetMacArchitecture() { try { return ArchProperty()?.GetValue(null); } catch (Exception) { return null; } }

        static void SetMacArchitecture(string value)
        {
            try
            {
                var p = ArchProperty();
                if (p == null) { Debug.LogWarning("[Reflex] build: macOS architecture setting not found (player may be universal)"); return; }
                p.SetValue(null, Enum.Parse(p.PropertyType, value));
            }
            catch (Exception e) { Debug.LogWarning("[Reflex] build: could not set the macOS architecture: " + e.Message); }
        }
    }
}
