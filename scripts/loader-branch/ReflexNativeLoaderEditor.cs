// PROTOTYPE (_re/notes/client-platform.md, "Temp-copy loader"; goes to Assets/Editor/Build/ with the converted bindings): reloads the temp copy
// of libreflexworld when the plugin file changed, before play mode starts (the project keeps its domain across play mode, so the old
// image would otherwise stay bound), and from the menu. Plus the batch check of the hot swap:
//   -executeMethod Reflex.Port.EditorTools.ReflexNativeLoaderEditor.HotSwapCheck -reflexSwapWith <other libreflexworld.dylib>
// loads the plugin, reads its version, replaces Assets/Plugins/macOS/libreflexworld.dylib with the other file (atomic rename),
// reloads, reads the version again, puts the original back, reloads, and logs "verify: PASS" when each load saw its own file.
using System;
using System.IO;
using Reflex.Port.Platform;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.EditorTools
{
    [InitializeOnLoad]
    static class ReflexNativeLoaderEditor
    {
        static ReflexNativeLoaderEditor()
        {
            EditorApplication.playModeStateChanged += s =>
            {
                if (s == PlayModeStateChange.ExitingEditMode && ReflexNativeLoader.SourceChanged())
                {
                    Debug.Log("[Reflex] libreflexworld changed on disk: loading the new plugin");
                    ReflexNativeLoader.Reload();
                }
            };
        }

        [MenuItem("Reflex/Plugin/Reload libreflexworld (temp copy)")]
        static void MenuReload()
        {
            if (EditorApplication.isPlaying) { Debug.LogWarning("[Reflex] stop play mode first: the running world belongs to the loaded plugin"); return; }
            ReflexNativeLoader.Reload();
        }

        static int Version()
        {
            var p = ReflexNativeLoader.TrySymbol("reflex_world_version");
            if (p == IntPtr.Zero) return -1;   // an unnumbered plugin
            return System.Runtime.InteropServices.Marshal.GetDelegateForFunctionPointer<VersionFn>(p)();
        }
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int VersionFn();

        public static void HotSwapCheck()
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, "-reflexSwapWith");
            string other = i >= 0 && i + 1 < a.Length ? a[i + 1] : null;
            string plugin = ReflexNativeLoader.PluginFile();
            bool ok = other != null && File.Exists(other) && plugin != null;
            string backup = plugin + ".swapcheck";
            try
            {
                if (!ok) throw new Exception("need -reflexSwapWith <dylib> and the project's plugin");
                int v1 = Version(); string p1 = ReflexNativeLoader.LoadedPath; int g1 = ReflexNativeLoader.Generation;
                // a call through a converted binding, before and after
                var c1 = Reflex.Port.OriginalWorld.ReflexWorldNative.PortReport().Length;
                File.Copy(plugin, backup, true);
                File.Copy(other, plugin + ".new", true); File.Move(plugin + ".new", plugin + ".x"); File.Delete(plugin); File.Move(plugin + ".x", plugin);
                File.SetLastWriteTimeUtc(plugin, DateTime.UtcNow);
                bool changed = ReflexNativeLoader.SourceChanged();
                ReflexNativeLoader.Reload();
                int v2 = Version(); string p2 = ReflexNativeLoader.LoadedPath;
                File.Delete(plugin); File.Move(backup, plugin); File.SetLastWriteTimeUtc(plugin, DateTime.UtcNow.AddSeconds(1));
                ReflexNativeLoader.Reload();
                int v3 = Version(); string p3 = ReflexNativeLoader.LoadedPath;
                Debug.Log($"[Reflex] hot swap: gen {g1} v{v1} {p1}; changed={changed} -> gen {ReflexNativeLoader.Generation - 1} v{v2} {p2}; back -> v{v3} {p3}; port report {c1} chars");
                ok = changed && v1 != v2 && v3 == v1 && p1 != p2 && p2 != p3;
            }
            catch (Exception e) { Debug.LogError("[Reflex] hot swap: " + e); ok = false; if (File.Exists(backup)) { File.Copy(backup, plugin, true); File.Delete(backup); } }
            Debug.Log(ok ? "verify: PASS" : "verify: FAIL hot swap");
            if (Application.isBatchMode) EditorApplication.Exit(ok ? 0 : 1);
        }
    }
}
