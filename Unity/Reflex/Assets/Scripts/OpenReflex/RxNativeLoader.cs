using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // Loads librxcore (core/build/librxcore.dylib) and binds its exports as delegates, the way Platform/ReflexNativeLoader does for the
    // old plugin (this is a separate loader: the old plugin path stays untouched and selectable). The caller says where the library is
    // (RxNativeLoader.Load); this assembly does not depend on UnityEngine so it can be compiled and tested outside the editor.
    //
    // Editor use: pass a temp root and the library is copied to <tempRoot>/<stamp>/ first, so a rebuilt dylib never overwrites the
    // image the editor has mapped. Never dlclose: the core owns one process-wide arena and signal-guard state.
    //
    // The ARENA image (w-loadarena): a second loaded copy of the same library (LoadArena: copied to a temp folder under another file
    // name, so the OS maps a second image with its own globals: its own arena, statics and session). The core allows one session per
    // image; the client's test-ride arena rides in this second image while the event the player chose loads in the main one. Which
    // image a call goes to is the calling THREAD's (ThreadImage, thread-static): a simulation worker sets it once at its start, so
    // every binding (RxApiNative and the other *Native classes, all through Fn<T>) reaches its own session's image; every other
    // thread stays on the main image.
    // macOS and Windows x64 use the same Cdecl API; platform loaders resolve exports without loading the legacy plugin.
    public static class RxNativeLoader
    {
        public const string EnvPath = "REFLEX_RXCORE_PATH";
        static bool Windows => RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
        public static string FileName => Windows ? "rxcore.dll" : "librxcore.dylib";
        public static string PluginDirectory(string dataPath, bool editor) => Path.Combine(dataPath,
            Windows ? "Plugins/x86_64" : editor ? "Plugins/macOS" : "PlugIns");

        const int RTLD_NOW = 2, RTLD_LOCAL = 4;
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlopen(string path, int mode);
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlsym(IntPtr handle, string name);
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlerror();

        const uint LOAD_WITH_ALTERED_SEARCH_PATH = 0x8;
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)]
        static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        static extern IntPtr GetProcAddress(IntPtr handle, string name);
        static IntPtr Open(string path) => Windows ? LoadLibraryExW(path, IntPtr.Zero, LOAD_WITH_ALTERED_SEARCH_PATH) : dlopen(path, RTLD_NOW | RTLD_LOCAL);
        static IntPtr Symbol(IntPtr handle, string name) => Windows ? GetProcAddress(handle, name) : dlsym(handle, name);
        static string LoadError() => Windows ? new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()).Message : (Marshal.PtrToStringAnsi(dlerror()) ?? "?");

        static IntPtr lib, arenaLib;
        static readonly object gate = new object();
        [ThreadStatic] static int t_image;
        /// The image this thread's calls reach: 0 the main image, 1 the arena image (a simulation worker sets it at its start).
        public static int ThreadImage { get => t_image; set => t_image = value == 1 ? 1 : 0; }
        /// The arena image is loaded (LoadArena).
        public static bool ArenaLoaded => arenaLib != IntPtr.Zero;
        public static string ArenaPath { get; private set; } = "";
        static int ArenaGeneration; static string arenaSource;

        [DllImport("/usr/lib/libSystem.B.dylib")] static extern int removexattr(string path, string name, int options);

        /// The arena image: a copy of the loaded main library in folder (a file of its own name: the OS maps it as a second image), loaded
        /// once. False with why when it cannot be (no main image, the copy or the load failed, or the OS gave back the main image).
        public static bool LoadArena(string folder, out string why)
        {
            why = null;
            lock (gate)
            {
                if (arenaLib != IntPtr.Zero && arenaSource == LoadedPath) return true;   // (the editor loads a rebuilt main library: its copy again)
                if (lib == IntPtr.Zero || !File.Exists(LoadedPath)) { why = "the main core library is not loaded"; return false; }
                try
                {
                    var info = new FileInfo(LoadedPath);
                    string dir = Path.Combine(folder, info.LastWriteTimeUtc.Ticks.ToString("x") + "-" + info.Length.ToString("x"));
                    Directory.CreateDirectory(dir);
                    string load = Path.Combine(dir, Windows ? "rxcore-arena.dll" : "librxcore-arena.dylib");
                    if (!File.Exists(load) || new FileInfo(load).Length != info.Length)
                    {
                        File.Copy(LoadedPath, load + ".tmp", true);
                        if (File.Exists(load)) File.Delete(load);
                        File.Move(load + ".tmp", load);
                    }
                    if (!Windows) try { removexattr(load, "com.apple.quarantine", 0); } catch (Exception) { }   // (a copy of a downloaded app's library)
                    IntPtr h = Open(load);
                    if (h == IntPtr.Zero) { why = "could not load " + load + ": " + LoadError(); return false; }
                    IntPtr a = Symbol(lib, "rx_session_create"), b = Symbol(h, "rx_session_create");
                    if (h == lib || a == b) { why = "the OS mapped the main library again (no second image)"; return false; }
                    arenaLib = h; ArenaPath = load; arenaSource = LoadedPath; ArenaGeneration++;
                    return true;
                }
                catch (Exception e) { why = e.Message; return false; }
            }
        }

        /// Bumped by every load; bindings re-resolve when it changes.
        public static int Generation { get; private set; }
        public static string LoadedPath { get; private set; } = "";
        public static bool IsLoaded => lib != IntPtr.Zero;

        /// REFLEX_RXCORE_PATH if set, else <pluginDir>/librxcore.dylib; null when neither file exists.
        public static string ResolvePath(string pluginDir)
        {
            string env = Environment.GetEnvironmentVariable(EnvPath);
            if (!string.IsNullOrEmpty(env)) return File.Exists(env) ? env : null;
            if (string.IsNullOrEmpty(pluginDir)) return null;
            string p = Path.Combine(pluginDir, FileName);
            return File.Exists(p) ? p : null;
        }

        /// Loads the library (a no-op for the same file when it is already loaded). tempRoot: copy it there first (editor).
        public static void Load(string path, string tempRoot = null)
        {
            if (string.IsNullOrEmpty(path) || !File.Exists(path)) throw new DllNotFoundException(FileName + " not found: " + path);
            lock (gate)
            {
                path = Path.GetFullPath(path);
                string load = path;
                if (!string.IsNullOrEmpty(tempRoot))
                {
                    var info = new FileInfo(path);
                    string dir = Path.Combine(tempRoot, info.LastWriteTimeUtc.Ticks.ToString("x") + "-" + info.Length.ToString("x"));
                    Directory.CreateDirectory(dir);
                    load = Path.Combine(dir, FileName);
                    if (!File.Exists(load))
                    {
                        File.Copy(path, load + ".tmp", true);
                        File.Move(load + ".tmp", load);
                    }
                }
                if (lib != IntPtr.Zero && load == LoadedPath) return;
                IntPtr h = Open(load);
                if (h == IntPtr.Zero) throw new DllNotFoundException("could not load " + load + ": " + LoadError());
                lib = h; LoadedPath = load; Generation++;
            }
        }

        /// One export bound as a delegate; re-resolved after a new Load. EntryPointNotFoundException when the export is missing.
        public sealed class Fn<T> where T : Delegate
        {
            readonly string name; T d, arenaD; int gen = -1, arenaGen = -1;
            public Fn(string name) { this.name = name; }
            public T Get()
            {
                if (t_image == 1) return GetArena();
                var cur = d;
                if (gen == Generation && cur != null) return cur;
                if (lib == IntPtr.Zero) throw new DllNotFoundException(FileName + " is not loaded (RxNativeLoader.Load)");
                IntPtr p = Symbol(lib, name);
                if (p == IntPtr.Zero) throw new EntryPointNotFoundException(name + " (" + LoadedPath + ")");
                cur = Marshal.GetDelegateForFunctionPointer<T>(p);
                d = cur; gen = Generation;
                return cur;
            }
            T GetArena()
            {
                var cur = arenaD;
                if (arenaGen == ArenaGeneration && cur != null) return cur;
                if (arenaLib == IntPtr.Zero) throw new DllNotFoundException(FileName + ": the arena image is not loaded (RxNativeLoader.LoadArena)");
                IntPtr p = Symbol(arenaLib, name);
                if (p == IntPtr.Zero) throw new EntryPointNotFoundException(name + " (" + ArenaPath + ")");
                cur = Marshal.GetDelegateForFunctionPointer<T>(p);
                arenaD = cur; arenaGen = ArenaGeneration;
                return cur;
            }
        }
    }
}
