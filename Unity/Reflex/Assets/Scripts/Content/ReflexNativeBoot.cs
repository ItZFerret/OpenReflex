using System;
using Reflex.Port.Platform;

namespace Reflex.Port.Content
{
    // The native game in the plugin's own boots: the world builds (reflex_world_build, ReflexWorldFiles) and the profile session
    // (reflex_profile_open, ReflexProfile). Since 2026-10-18 libreflexworld boots them with the baton scheduler and installs the native
    // ports for the whole boot (native/game notes/loading.md section 9), with the same mode as the race worlds' native game
    // (OriginalWorldRuntime.NativeGame: REFLEX_NATIVE_GAME, then the ReflexNativeGame setting, else Native) and filter (REFLEX_NATIVE_PORTS,
    // else "all"). An older plugin has no reflex_world_boot_ports: its boots run the original code under the exact-slice scheduler,
    // and its worlds keep the old cache folder (Version).
    public static class ReflexNativeBoot
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_boot_ports__fn([System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPUTF8Str)] string filter, int mode);
        static readonly ReflexNativeLoader.Fn<reflex_world_boot_ports__fn> reflex_world_boot_ports__p = new ReflexNativeLoader.Fn<reflex_world_boot_ports__fn>("reflex_world_boot_ports");

        /// The plugin boots with baton scheduling (it has reflex_world_boot_ports): its worlds are a different kind (another cache folder).
        public static bool PluginBootsBaton
        {
            get { try { return ReflexNativeLoader.TrySymbol("reflex_world_boot_ports") != IntPtr.Zero; } catch (Exception) { return false; } }
        }

        /// 0 the original code, 1 the natives (default), 2 verify: as OriginalWorldRuntime.NativeGame without a scene's override.
        public static int Mode
        {
            get
            {
                string e = Environment.GetEnvironmentVariable("REFLEX_NATIVE_GAME");
                if (int.TryParse(e, out int v)) return Math.Max(0, Math.Min(2, v));
                try { if (Reflex.Port.Options.ReflexSave.Has("ReflexNativeGame") && (v = Reflex.Port.Options.ReflexSave.GetInt("ReflexNativeGame")) >= 0) return Math.Min(2, v); }
                catch (Exception) { }
                return 1;
            }
        }
        public static string Filter
        {
            get { string f = Environment.GetEnvironmentVariable("REFLEX_NATIVE_PORTS"); return string.IsNullOrEmpty(f) ? "all" : f; }
        }

        /// Sets the ports of the next boots (call on the main thread before a build / a profile open). False with an older plugin.
        public static bool Apply()
        {
            try { reflex_world_boot_ports__p.Get()(Filter, Mode); return true; }
            catch (Exception e) when (e is EntryPointNotFoundException || e is DllNotFoundException) { return false; }
        }
    }
}
