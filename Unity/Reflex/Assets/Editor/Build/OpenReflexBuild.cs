using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace Reflex.Port.EditorTools
{
    // Dedicated macOS / Windows x64 open-core builds. Legacy build entries remain available for their supported features.
    public static class OpenReflexBuild
    {
        // Supply REFLEX_RXCORE_PATH=<Windows x64 rxcore.dll> and REFLEX_OPEN_PLAYER=<directory/name.exe>.
        public static void BuildWindowsBatch()
        {
            int code = 1;
            var arch = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("UnityEditor.WindowsStandalone.UserBuildSettings", false))
                .FirstOrDefault(t => t != null)?.GetProperty("architecture", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            object previousArch = arch?.GetValue(null);
            var changed = new List<(PluginImporter importer, bool any, bool windows)>();
            try
            {
                string core = Environment.GetEnvironmentVariable("REFLEX_RXCORE_PATH") ?? Path.Combine(ReflexBuild.RepoRoot, "core/build-windows/rxcore.dll");
                string output = Path.GetFullPath(Environment.GetEnvironmentVariable("REFLEX_OPEN_PLAYER") ?? Path.Combine(ReflexBuild.RepoRoot, "builds/OpenReflexWindows/OpenReflex.exe"));
                if (!File.Exists(core)) throw new FileNotFoundException("Build the Windows x64 open core first.", core);
                ValidateWindowsCore(core);
                if (!string.Equals(Path.GetExtension(output), ".exe", StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Windows player output must end in .exe.");
                foreach (string path in AssetDatabase.GetAllAssetPaths())
                {
                    string name = Path.GetFileName(path);
                    if (name.IndexOf("reflexworld", StringComparison.OrdinalIgnoreCase) < 0 && name.IndexOf("capstone", StringComparison.OrdinalIgnoreCase) < 0) continue;
                    if (!(AssetImporter.GetAtPath(path) is PluginImporter importer)) continue;
                    changed.Add((importer, importer.GetCompatibleWithAnyPlatform(), importer.GetCompatibleWithPlatform(BuildTarget.StandaloneWindows64)));
                    importer.SetCompatibleWithAnyPlatform(false);
                    importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, false);
                    importer.SaveAndReimport();
                }
                if (arch == null) throw new InvalidOperationException("The Windows build architecture setting is unavailable.");
                arch.SetValue(null, Enum.Parse(arch.PropertyType, "x64"));
                Directory.CreateDirectory(Path.GetDirectoryName(output));
                // CPU / GPU frame times in players' perf logs (ReflexPerfLog: "cpu main / render, gpu" need Frame Timing Stats)
                PlayerSettings.enableFrameTimingStats = true;
                var r = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
                    scenes = ReflexBuild.Scenes, locationPathName = output, target = BuildTarget.StandaloneWindows64,
                    targetGroup = BuildTargetGroup.Standalone, options = BuildOptions.None
                });
                if (r.summary.result != BuildResult.Succeeded) throw new InvalidOperationException("Open Windows player build failed: " + r.summary.totalErrors + " errors.");
                string data = Path.Combine(Path.GetDirectoryName(output), Path.GetFileNameWithoutExtension(output) + "_Data");
                string plugins = Path.Combine(data, "Plugins", "x86_64");
                Directory.CreateDirectory(plugins);
                foreach (string path in Directory.GetFiles(Path.GetDirectoryName(output), "*", SearchOption.AllDirectories))
                {
                    string name = Path.GetFileName(path);
                    if (name.IndexOf("reflexworld", StringComparison.OrdinalIgnoreCase) >= 0 || name.IndexOf("capstone", StringComparison.OrdinalIgnoreCase) >= 0)
                        throw new InvalidOperationException("Legacy library unexpectedly packaged: " + path);
                }
                string dest = Path.Combine(plugins, "rxcore.dll");
                File.Copy(core, dest + ".tmp", true);
                if (File.Exists(dest)) File.Delete(dest);
                File.Move(dest + ".tmp", dest);
                Directory.CreateDirectory(Path.Combine(data, "Resources"));
                File.WriteAllText(Path.Combine(data, "Resources", "reflex-open-core.txt"), "open\n");
                Debug.Log($"[open-build] Succeeded Windows x64: {output}, rxcore.dll bundled, open backend marker, no legacy plugin, {r.summary.totalErrors} errors.");
                code = 0;
            }
            catch (Exception e) { Debug.LogError("[open-build] " + e); }
            finally
            {
                if (previousArch != null) arch.SetValue(null, previousArch);
                foreach (var entry in changed)
                {
                    entry.importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, entry.windows);
                    entry.importer.SetCompatibleWithAnyPlatform(entry.any);
                    entry.importer.SaveAndReimport();
                }
            }
            EditorApplication.Exit(code);
        }

        static void ValidateWindowsCore(string path)
        {
            using var input = File.OpenRead(path);
            using var reader = new BinaryReader(input);
            if (input.Length < 0x40 || reader.ReadUInt16() != 0x5a4d) throw new InvalidDataException("Open Windows core must be a PE DLL: " + path);
            input.Position = 0x3c; uint pe = reader.ReadUInt32();
            if (pe > input.Length - 26) throw new InvalidDataException("Truncated Windows core PE header.");
            input.Position = pe;
            if (reader.ReadUInt32() != 0x00004550 || reader.ReadUInt16() != 0x8664) throw new InvalidDataException("Open Windows core must target AMD64.");
            input.Position = pe + 22;
            if ((reader.ReadUInt16() & 0x2000) == 0 || reader.ReadUInt16() != 0x20b) throw new InvalidDataException("Open Windows core must be a PE32+ DLL.");
        }

        public static void BuildMacBatch()
        {
            int code = 1;
            var arch = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("UnityEditor.OSXStandalone.UserBuildSettings", false))
                .FirstOrDefault(t => t != null)?.GetProperty("architecture", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            object previousArch = arch?.GetValue(null);
            var changed = new List<(PluginImporter importer, bool any, bool mac)>();
            try
            {
                string core = Environment.GetEnvironmentVariable("REFLEX_RXCORE_PATH") ?? Path.Combine(ReflexBuild.RepoRoot, "core/build/librxcore.dylib");
                string output = Environment.GetEnvironmentVariable("REFLEX_OPEN_PLAYER") ?? Path.Combine(ReflexBuild.RepoRoot, "builds/OpenReflex.app");
                if (!File.Exists(core)) throw new FileNotFoundException("Build the open core first.", core);
                foreach (string path in ReflexBuild.Plugins)
                {
                    if (!(AssetImporter.GetAtPath(path) is PluginImporter importer)) continue;
                    changed.Add((importer, importer.GetCompatibleWithAnyPlatform(), importer.GetCompatibleWithPlatform(BuildTarget.StandaloneOSX)));
                    importer.SetCompatibleWithAnyPlatform(false);
                    importer.SetCompatibleWithPlatform(BuildTarget.StandaloneOSX, false);
                    importer.SaveAndReimport();
                }
                if (arch == null) throw new InvalidOperationException("The macOS build architecture setting is unavailable.");
                arch.SetValue(null, Enum.Parse(arch.PropertyType, "ARM64"));
                // CPU / GPU frame times in players' perf logs (ReflexPerfLog: "cpu main / render, gpu" need Frame Timing Stats)
                PlayerSettings.enableFrameTimingStats = true;
                var r = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
                    scenes = ReflexBuild.Scenes, locationPathName = output, target = BuildTarget.StandaloneOSX, options = BuildOptions.None
                });
                if (r.summary.result != BuildResult.Succeeded) throw new InvalidOperationException("Open player build failed: " + r.summary.totalErrors + " errors.");
                string plugins = Path.Combine(output, "Contents/PlugIns");
                Directory.CreateDirectory(plugins);
                foreach (string path in Directory.GetFiles(plugins, "*", SearchOption.AllDirectories))
                    if (Path.GetFileName(path).Contains("reflexworld") || Path.GetFileName(path).Contains("capstone"))
                        throw new InvalidOperationException("Legacy library unexpectedly packaged: " + path);
                string dest = Path.Combine(plugins, "librxcore.dylib");
                File.Copy(core, dest + ".tmp", true);
                if (File.Exists(dest)) File.Delete(dest);
                File.Move(dest + ".tmp", dest);
                File.WriteAllText(Path.Combine(output, "Contents/Resources/reflex-open-core.txt"), "open\n");
                Debug.Log($"[open-build] Succeeded: {output}, core bundled, no legacy plugin, {r.summary.totalErrors} errors.");
                code = 0;
            }
            catch (Exception e) { Debug.LogError("[open-build] " + e); }
            finally
            {
                if (previousArch != null) arch.SetValue(null, previousArch);
                foreach (var entry in changed)
                {
                    entry.importer.SetCompatibleWithPlatform(BuildTarget.StandaloneOSX, entry.mac);
                    entry.importer.SetCompatibleWithAnyPlatform(entry.any);
                    entry.importer.SaveAndReimport();
                }
            }
            EditorApplication.Exit(code);
        }
    }
}
