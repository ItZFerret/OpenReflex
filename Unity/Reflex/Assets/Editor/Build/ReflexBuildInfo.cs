// Stamps every player build (after Open Unleashed's MxaBuildInfo): <player>/.../StreamingAssets/ReflexBuildInfo.txt, written into
// the build output only, never into Assets. ReflexCrashLog shows it in session logs, crash reports and support reports. It then
// keeps the build's native symbols: the player's libreflexworld.dylib (with its local symbol table; dSYM when the plugin was built
// with -g), its LC_UUID (plugin-id.txt, what crash-*.txt prints as "id") and the stamp, under <symbols root>/<build id>/ for
// REFLEX/scripts/symbolicate_crash.sh (Windows: reflexworld.dll, its "pe-..." id and, with -reflexPluginDebug, its DWARF
// reflexworld.dll.debug). The symbols root is -reflexSymbolsDir <dir>, else REFLEX_SYMBOLS_DIR, else REFLEX/builds/symbols.
// Symbols are kept for builds made through ReflexBuild (the menu, build_test.sh) or when a symbols folder is named; other player
// builds (bench tools) only get the stamp.
using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.EditorTools
{
    class ReflexBuildInfo : IPostprocessBuildWithReport
    {
        public int callbackOrder => 100;

        /// Set by ReflexBuild around its BuildPlayer call: keep this build's symbols.
        public static bool KeepSymbolsForThisBuild;

        /// The id of the last player stamped in this Editor session ("" before any).
        public static string LastId { get; private set; } = "";
        public static string LastSymbols { get; private set; } = "";

        public void OnPostprocessBuild(BuildReport report)
        {
            var target = report.summary.platform;
            string output = report.summary.outputPath, data, platform, plugin;
            if (target == BuildTarget.StandaloneOSX)
            {
                data = Path.Combine(output, "Contents", "Resources", "Data");
                platform = "macOS arm64";
                plugin = FindPlugin(Path.Combine(output, "Contents", "PlugIns"), "libreflexworld.dylib");
            }
            else if (target == BuildTarget.StandaloneWindows64)
            {
                data = Path.Combine(Path.GetDirectoryName(output) ?? ".", Path.GetFileNameWithoutExtension(output) + "_Data");
                platform = "Windows x64";
                plugin = FindPlugin(Path.Combine(data, "Plugins"), "reflexworld.dll");
            }
            else return;
            var now = DateTime.Now;
            string id = now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + (target == BuildTarget.StandaloneOSX ? "-mac" : "-win");
            string pluginId = plugin == null ? null : target == BuildTarget.StandaloneOSX ? MachOUuid(plugin) : PeId(plugin);
            string dir = Path.Combine(data, "StreamingAssets");
            Directory.CreateDirectory(dir);
            string info = "stamp=" + now.ToString("yyyy-MM-dd HH.mm", CultureInfo.InvariantCulture) + "\n" +
                          "platform=" + platform + "\n" +
                          "id=" + id + "\n" +
                          "unity=" + Application.unityVersion + "\n" +
                          "version=" + PlayerSettings.bundleVersion + "\n" +
                          "development=" + ((report.summary.options & BuildOptions.Development) != 0 ? 1 : 0) + "\n" +
                          "plugin=" + (pluginId ?? "?") + "\n" +
                          "guid=" + report.summary.guid + "\n";
            File.WriteAllText(Path.Combine(dir, "ReflexBuildInfo.txt"), info);
            LastId = id;
            Debug.Log("[Reflex] build stamp " + id + " -> " + dir + (pluginId != null ? " (plugin " + pluginId + ")" : " (no plugin found in the player)"));
            KeepSymbols(id, plugin, pluginId, info);
        }

        static string FindPlugin(string root, string name)
        {
            if (!Directory.Exists(root)) return null;
            foreach (var f in Directory.GetFiles(root, name, SearchOption.AllDirectories)) return f;
            return null;
        }

        public static string SymbolsRoot()
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, "-reflexSymbolsDir");
            if (i >= 0 && i + 1 < a.Length) return Path.GetFullPath(a[i + 1]);
            string env = Environment.GetEnvironmentVariable("REFLEX_SYMBOLS_DIR");
            if (!string.IsNullOrEmpty(env)) return Path.GetFullPath(env);
            return Path.Combine(ReflexBuild.RepoRoot, "builds", "symbols");
        }

        static string Arg(string name)
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, name);
            return i >= 0 && i + 1 < a.Length ? Path.GetFullPath(a[i + 1]) : null;
        }

        static bool SymbolsRequested() => KeepSymbolsForThisBuild || Array.IndexOf(Environment.GetCommandLineArgs(), "-reflexSymbolsDir") >= 0 ||
                                           !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_SYMBOLS_DIR"));

        static void KeepSymbols(string id, string plugin, string pluginId, string info)
        {
            LastSymbols = "";
            if (plugin == null || !SymbolsRequested()) return;
            try
            {
                string sym = Path.Combine(SymbolsRoot(), id);
                Directory.CreateDirectory(sym);
                string dst = Path.Combine(sym, Path.GetFileName(plugin));
                File.Copy(plugin, dst, true);
                File.WriteAllText(Path.Combine(sym, "plugin-id.txt"), (pluginId ?? "?") + "\n");
                File.WriteAllText(Path.Combine(sym, "build.txt"), info);
                // a dSYM holds line tables only when the plugin was compiled with -g; without, atos still resolves the local symbols
                if (Application.platform == RuntimePlatform.OSXEditor && dst.EndsWith(".dylib"))
                    Run("/usr/bin/dsymutil", $"\"{dst}\" -o \"{dst}.dSYM\"");
                // Windows: the MinGW DLL ships stripped; its DWARF (build_plugin_windows.sh's reflexworld.dll.debug) is named on the
                // command line by package_windows_build.sh
                string debug = Arg("-reflexPluginDebug");
                if (dst.EndsWith(".dll") && debug != null && File.Exists(debug)) File.Copy(debug, Path.Combine(sym, Path.GetFileName(debug)), true);
                LastSymbols = sym;
                Debug.Log("[Reflex] symbols kept: " + sym);
            }
            catch (Exception e) { Debug.LogWarning("[Reflex] symbols not kept: " + e.Message); }
        }

        static void Run(string exe, string args)
        {
            try
            {
                using var p = Process.Start(new ProcessStartInfo(exe, args) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true });
                p.StandardOutput.ReadToEnd(); p.StandardError.ReadToEnd();
                p.WaitForExit(120000);
            }
            catch (Exception) { }
        }

        /// LC_UUID of a 64-bit Mach-O (thin, or the arm64 slice of a fat file) as crash.inc prints it, or null.
        public static string MachOUuid(string path)
        {
            try
            {
                byte[] b = File.ReadAllBytes(path);
                int off = 0;
                uint magic = BitConverter.ToUInt32(b, 0);
                if (magic == 0xBEBAFECA) // FAT_MAGIC, big endian
                {
                    int n = BigEndian(b, 4);
                    for (int i = 0; i < n; i++)
                    {
                        int at = 8 + i * 20;
                        if (BigEndian(b, at) == 0x0100000C) { off = BigEndian(b, at + 8); break; } // CPU_TYPE_ARM64
                    }
                }
                if (BitConverter.ToUInt32(b, off) != 0xFEEDFACF) return null;
                int ncmds = BitConverter.ToInt32(b, off + 16), p = off + 32;
                for (int i = 0; i < ncmds; i++)
                {
                    int cmd = BitConverter.ToInt32(b, p), size = BitConverter.ToInt32(b, p + 4);
                    if (cmd == 0x1B) // LC_UUID
                    {
                        var s = BitConverter.ToString(b, p + 8, 16).Replace("-", "");
                        return $"{s.Substring(0, 8)}-{s.Substring(8, 4)}-{s.Substring(12, 4)}-{s.Substring(16, 4)}-{s.Substring(20)}";
                    }
                    p += size;
                }
            }
            catch (Exception) { }
            return null;
        }

        /// A PE image's id as crash.inc's Windows branch prints it: "pe-<TimeDateStamp %08X>-<SizeOfImage %X>", or null.
        public static string PeId(string path)
        {
            try
            {
                byte[] b = File.ReadAllBytes(path);
                if (b.Length < 0x40 || b[0] != 'M' || b[1] != 'Z') return null;
                int nt = BitConverter.ToInt32(b, 0x3C);
                if (nt < 0 || nt + 24 + 60 > b.Length || BitConverter.ToUInt32(b, nt) != 0x00004550) return null; // "PE\0\0"
                uint stamp = BitConverter.ToUInt32(b, nt + 8);           // IMAGE_FILE_HEADER.TimeDateStamp
                uint size = BitConverter.ToUInt32(b, nt + 24 + 56);      // IMAGE_OPTIONAL_HEADER(64).SizeOfImage
                return "pe-" + stamp.ToString("X8") + "-" + size.ToString("X");
            }
            catch (Exception) { return null; }
        }

        static int BigEndian(byte[] b, int o) => (b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3];
    }
}
