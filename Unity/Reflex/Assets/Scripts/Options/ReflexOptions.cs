// The options pages' one entry point (the shell's hub, the pause menus, F4 in a race): ReflexOptions.Open(tab, onClosed).
// A UI Toolkit document of its own (DontDestroyOnLoad, sorting over everything), Open Unleashed's options in its look
// (ReflexOptionsMenu: CONTROLS, AUDIO, VIDEO, GAMEPLAY, CAMERA & HUD, INTERFACE, SYSTEM, OPEN REFLEX). In a race it pauses the world while open (the
// world's frames stop, so no key or pad reaches the game) and gives the keys back only once they are released (an Esc that
// closes the pages does not also reach the game's own pause page).
using System;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public static class ReflexOptions
    {
        public static readonly string[] Tabs = ReflexOptionsMenu.OptionTabs;
        public const int Controls = 0, Audio = 1, Video = 2, Gameplay = 3, CameraHud = 4, Interface = 5, System = 6, OpenReflex = 7;

        static ReflexOptionsHost s_host;
        public static bool IsOpen => s_host && s_host.IsOpen;
        /// The frame the pages closed (callers that share the Esc key skip it that frame).
        public static int ClosedFrame { get; internal set; } = -1;
        public static int Tab => s_host ? s_host.Tab : 0;
        public static event Action Opened, Closed;
        /// The pages' document root (tests, screenshots); null before the first Open.
        public static VisualElement Root => s_host ? s_host.Root : null;
        public static UIDocument Document => s_host ? s_host.Doc : null;

        /// Shows the pages at tab (0 CONTROLS .. 6 SYSTEM, 7 OPEN REFLEX); onClosed runs when they close (Back / Esc / Close).
        public static void Open(int tab = 0, Action onClosed = null)
        {
            if (!s_host)
            {
                var go = new GameObject("Reflex options");
                UnityEngine.Object.DontDestroyOnLoad(go);
                s_host = go.AddComponent<ReflexOptionsHost>();
            }
            s_host.Show(tab, onClosed);
            Opened?.Invoke();
        }

        /// The SCREEN ADJUSTMENT page alone (OPTIONS > VIDEO > SCREEN ADJUSTMENT is the same page); onClosed runs when it closes.
        public static void OpenScreenAdjustment(Action onClosed = null)
        {
            if (!s_host)
            {
                var go = new GameObject("Reflex options");
                UnityEngine.Object.DontDestroyOnLoad(go);
                s_host = go.AddComponent<ReflexOptionsHost>();
            }
            s_host.ShowScreenAdjustment(onClosed);
            Opened?.Invoke();
        }

        public static void Close() { if (s_host) s_host.Hide(); }
        public static void ShowTab(int tab) { if (s_host && s_host.IsOpen) s_host.Show(tab, null, keepCallback: true); }

        internal static void RaiseClosed() { ClosedFrame = Time.frameCount; Closed?.Invoke(); }
    }

}
