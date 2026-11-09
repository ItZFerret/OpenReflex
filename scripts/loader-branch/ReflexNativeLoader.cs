// PROTOTYPE (not in the Unity project; _re/notes/client-platform.md, "Temp-copy loader"): the temp-copy loader of libreflexworld, after Open
// Unleashed's MxaNative. The Editor loads a private copy of Assets/Plugins/macOS/libreflexworld.dylib (and libcapstone.5.dylib, its
// @loader_path dependency) from Temp/ReflexPlugin/<stamp>/, so the plugin can be rebuilt and replaced while the Editor stays open:
// Unity never unloads a plugin it loaded through DllImport, so today a new plugin needs an Editor restart. A player loads its own
// bundled plugin (Contents/PlugIns, <app>_Data/Plugins/x86_64) in place.
//
// The bindings: every [DllImport("reflexworld")] extern becomes a delegate resolved here by name (convert_dllimports.py does it
// mechanically; delegates keep every MarshalAs / [Out] / ref / string marshalling of the DllImport, unlike unmanaged function
// pointers, and need no unsafe code). A missing export throws EntryPointNotFoundException and a missing library
// DllNotFoundException, as DllImport did, so the existing "older plugin" catches keep working.
//
// Reload(): loads the current file again (a new copy when it changed) and bumps Generation, which every binding checks, so the next
// call goes to the new image. Only call it with no world / profile / Bink handle alive (they belong to the old image: pointers into its
// heap, its statics). ReflexNativeLoaderEditor (Editor) calls it when entering play mode if the plugin file changed; the project's
// Enter Play Mode Options keep the domain (no domain reload), so nothing else would. The old image stays mapped (never dlclose'd:
// threads of an old world may still be finishing, and its signal handlers / atexit hooks point into it).
using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Reflex.Port.Platform
{
    public static class ReflexNativeLoader
    {
        /// Bumped by every (re)load; bindings re-resolve when it changed.
        public static int Generation { get; private set; }
        /// The file actually loaded (the temp copy in the Editor).
        public static string LoadedPath { get; private set; } = "";
        /// The plugin file the copy was made from, and its stamp (mtime ticks + size) when it was copied.
        public static string SourcePath { get; private set; } = "";
        public static string SourceStamp { get; private set; } = "";

        static IntPtr s_lib;
        static readonly object s_lock = new object();

#if UNITY_EDITOR_WIN || (UNITY_STANDALONE_WIN && !UNITY_EDITOR)
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, ExactSpelling = true)] static extern IntPtr LoadLibraryW(string path);
        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)] static extern IntPtr GetProcAddress(IntPtr h, string name);
        static IntPtr Open(string path) => LoadLibraryW(path);
        static IntPtr Find(IntPtr h, string name) => GetProcAddress(h, name);
        static string LastError() => new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()).Message;
        const string FileName = "reflexworld.dll";
        static readonly string[] Companions = { "capstone.dll" };
#else
        const int RTLD_NOW = 2, RTLD_LOCAL = 4;
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlopen(string path, int mode);
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlsym(IntPtr h, string name);
        [DllImport("/usr/lib/libSystem.B.dylib")] static extern IntPtr dlerror();
        static IntPtr Open(string path) => dlopen(path, RTLD_NOW | RTLD_LOCAL);
        static IntPtr Find(IntPtr h, string name) => dlsym(h, name);
        static string LastError() => Marshal.PtrToStringAnsi(dlerror()) ?? "?";
        const string FileName = "libreflexworld.dylib";
        static readonly string[] Companions = { "libcapstone.5.dylib" };
#endif

        /// The plugin file of this Editor / player (where Unity puts it); null if not found.
        public static string PluginFile()
        {
            string[] candidates = Application.isEditor
                ? new[] { Path.Combine(Application.dataPath, "Plugins", "macOS", FileName), Path.Combine(Application.dataPath, "Plugins", "x86_64", FileName) }
                : new[] { Path.Combine(Application.dataPath, "PlugIns", FileName), Path.Combine(Application.dataPath, "Plugins", "x86_64", FileName), Path.Combine(Application.dataPath, "Plugins", FileName) };
            foreach (var c in candidates) if (File.Exists(c)) return c;
            return null;
        }

        static string Stamp(string file) { var i = new FileInfo(file); return i.LastWriteTimeUtc.Ticks.ToString("x") + "-" + i.Length.ToString("x"); }

        /// True when the plugin file differs from the loaded one (rebuilt / installed since).
        public static bool SourceChanged()
        {
            string f = PluginFile();
            return f != null && s_lib != IntPtr.Zero && Stamp(f) != SourceStamp;
        }

        static IntPtr Lib
        {
            get
            {
                if (s_lib != IntPtr.Zero) return s_lib;
                lock (s_lock) { if (s_lib == IntPtr.Zero) LoadLocked(); }
                return s_lib;
            }
        }

        /// Loads the current plugin file again (see the header: no handle of the old image may be alive).
        public static void Reload()
        {
            lock (s_lock) { s_lib = IntPtr.Zero; LoadLocked(); }
        }

        static void LoadLocked()
        {
            string src = PluginFile() ?? throw new DllNotFoundException(FileName + " not found next to the player / in Assets/Plugins");
            string path = src, stamp = Stamp(src);
            if (Application.isEditor)
            {
                // Temp/ is the project's (cleared when the Editor quits); one folder per stamp, so the same file maps to the same
                // copy (dlopen of a loaded path returns that image: no second copy of its statics)
                string dir = Path.Combine(Path.GetDirectoryName(Application.dataPath), "Temp", "ReflexPlugin", stamp);
                Directory.CreateDirectory(dir);
                path = Path.Combine(dir, FileName);
                if (!File.Exists(path))
                {
                    foreach (var c in Companions)
                    {
                        string from = Path.Combine(Path.GetDirectoryName(src), c);
                        if (File.Exists(from)) File.Copy(from, Path.Combine(dir, c), true);
                    }
                    File.Copy(src, path + ".tmp", true);
                    File.Move(path + ".tmp", path);
                }
            }
            IntPtr h = Open(path);
            if (h == IntPtr.Zero) throw new DllNotFoundException("could not load " + path + ": " + LastError());
            s_lib = h; LoadedPath = path; SourcePath = src; SourceStamp = stamp;
            Generation++;
            Debug.Log($"[Reflex] libreflexworld loaded (generation {Generation}): {path}");
        }

        /// The address of an export, or IntPtr.Zero.
        public static IntPtr TrySymbol(string name) => Find(Lib, name);

        /// One export bound as a delegate, re-resolved after a Reload.
        public sealed class Fn<T> where T : Delegate
        {
            readonly string name; T d; int gen = -1;
            public Fn(string name) { this.name = name; }
            public T Get()
            {
                var cur = d;
                if (gen == Generation && cur != null) return cur;
                IntPtr p = Find(Lib, name);
                if (p == IntPtr.Zero) throw new EntryPointNotFoundException(name + " (libreflexworld " + LoadedPath + ")");
                cur = Marshal.GetDelegateForFunctionPointer<T>(p);
                d = cur; gen = Generation;
                return cur;
            }
        }
    }
}
