using System;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// Explicit command-line window requests are process-only and take priority over saved display preferences.
    public static class ReflexWindowOverride
    {
        static string Argument(string name)
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; i++) if (args[i].Equals(name, StringComparison.OrdinalIgnoreCase)) return args[i + 1];
            return null;
        }
        /// The run is an automated test player (-reflexTestWindow): it must never lock, hide or move the user's mouse.
        /// (Read once: the input asks every frame, and Environment.GetCommandLineArgs copies the arguments each call, w-gcspikes.)
        public static bool TestWindow => s_testWindow ??= !Application.isEditor && HasTestWindowArg();
        static bool? s_testWindow;
        static bool HasTestWindowArg()
        {
            foreach (string arg in Environment.GetCommandLineArgs()) if (arg.Equals("-reflexTestWindow", StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }
        public static bool Enabled
        {
            get
            {
                if (Application.isEditor) return false;
                if (TestWindow) return true;
                return Argument("-screen-fullscreen") == "0";
            }
        }
        public static bool TryApply()
        {
            if (!Enabled) return false;
            int width = int.TryParse(Argument("-screen-width"), out int w) ? Mathf.Clamp(w, 320, 7680) : 1280;
            int height = int.TryParse(Argument("-screen-height"), out int h) ? Mathf.Clamp(h, 180, 4320) : 720;
            Screen.SetResolution(width, height, FullScreenMode.Windowed);
            return true;
        }
    }
}
