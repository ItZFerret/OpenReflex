// The display's refresh rate, resolution and DPI, read from Unity at most once a second (and at once when the window's size or mode
// changes). Screen.currentResolution / Screen.dpi ask the window system every time: on macOS each call went to the window server
// (a mach message round trip: ~14 % of the main thread during a level load in tests/perf_profile's `sample`), and ReflexPerfLog and the
// race pacing asked every frame. Main thread only (Screen).
using UnityEngine;

namespace Reflex.Port.Platform
{
    public static class ReflexDisplayInfo
    {
        static Resolution s_res; static float s_dpi; static float s_next = float.NegativeInfinity;
        static int s_w = -1, s_h = -1; static FullScreenMode s_mode;

        /// Screen.currentResolution (the monitor's mode), at most a second old.
        public static Resolution Current { get { Refresh(); return s_res; } }
        /// The monitor's refresh rate in Hz (0 when unknown).
        public static double RefreshHz { get { Refresh(); return s_res.refreshRateRatio.value; } }
        /// Screen.dpi, at most a second old.
        public static float Dpi { get { Refresh(); return s_dpi; } }

        static void Refresh()
        {
            float now = Time.realtimeSinceStartup;
            int w = Screen.width, h = Screen.height; var mode = Screen.fullScreenMode;
            if (now < s_next && w == s_w && h == s_h && mode == s_mode) return;
            s_next = now + 1f; s_w = w; s_h = h; s_mode = mode;
            s_res = Screen.currentResolution;
            s_dpi = Screen.dpi;
        }
    }
}
