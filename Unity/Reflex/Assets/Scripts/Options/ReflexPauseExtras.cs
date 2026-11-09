// The pause menu's additions (Open Unleashed's QUICK SETTINGS: MxaRideMenus.ShowPause) where the pause page is the game's own,
// live (OriginalPauseMenu.GameOwnsPause: the arcade races and the modes run the game's "Paused Menu Page", driven by its Lua;
// its rows stay exactly the game's). A small panel beside it: CAMERA (cycle: the game's four presets and the Dynamic FPV view),
// HUD STYLE (MODERN / ORIGINAL), OPTIONS (the full pages). The game's page has the keyboard and the pad, so the panel is for the
// mouse (the cursor is free while it shows) and F4 (OPTIONS, a client key the game never sees). The port's own pause page (the
// worlds without the live UI) has the same three as rows of its list (OriginalPauseMenu).
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    [DisallowMultipleComponent, DefaultExecutionOrder(950)]
    public sealed class ReflexPauseExtras : MonoBehaviour
    {
        OriginalWorldRuntime rt; OriginalLiveUi live;
        GameObject host; VisualElement root, panel;
        bool shown;
        /// The panel shows (tests).
        public bool Shown => shown;
        /// The game's own pause page shows in the live UI.
        public bool GamePauseShows { get; private set; }

        void Awake() { rt = GetComponent<OriginalWorldRuntime>(); }

        void Build()
        {
            if (host) return;
            OptUi.EnsureEventSystem();
            host = new GameObject("Pause extras");
            host.transform.SetParent(transform, false);
            root = OptUi.CreateDocument(host, -1);
            panel = OptUi.Div("rx-pause-extras", root);
            root.style.display = DisplayStyle.None;
        }

        // the rows, made again each time the panel shows (their values as they are now)
        void Fill()
        {
            panel.Clear();
            OptUi.Text("QUICK SETTINGS", "mxa-pause-section", panel);
            var cam = OptUi.Spinner("CAMERA", () => ReflexClientLive.CameraName, d => ReflexClientLive.CycleCamera(d), panel);
            cam.AddToClassList("mxa-pause-row"); cam.name = "pause-camera";
            var hud = OptUi.Spinner("HUD STYLE", () => ReflexSettings.HudStyleNames[(int)ReflexSettings.HudStyle], _ => { ReflexSettings.HudStyle = ReflexSettings.HudStyle == HudStyle.Modern ? HudStyle.Original : HudStyle.Modern; ReflexSettings.Save(); }, panel);
            hud.AddToClassList("mxa-pause-row"); hud.name = "pause-hud-style";
            var opt = OptUi.Button("OPTIONS", "mxa-main-button", OpenOptions, panel);
            opt.name = "pause-options";
            if (Reflex.Port.Replay.ReflexReplay.Offered(rt))   // REPLAY / PHOTO MODE (w-replayphoto)
            {
                OptUi.Button("REPLAY", "mxa-main-button", () => Reflex.Port.Replay.ReflexReplay.Of(rt)?.Open(Reflex.Port.Replay.ReflexReplay.View.Replay, Reflex.Port.Replay.ReflexReplay.Origin.Pause), panel).name = "pause-replay";
                OptUi.Button("PHOTO MODE", "mxa-main-button", () => { var r = Reflex.Port.Replay.ReflexReplay.Of(rt); if (r && r.Open(Reflex.Port.Replay.ReflexReplay.View.Photo, Reflex.Port.Replay.ReflexReplay.Origin.Pause)) r.OpenedPhotoDirect(); }, panel).name = "pause-photo";
            }
            var hints = OptUi.Div("rx-pause-extras-hints", panel);
            OptUi.Hints(hints, ("F4", "OPTIONS"), ("CLICK", "CHANGE"));
        }

        // the game's "Paused Menu Page" among the live frame's visible layers (no garbage per frame)
        bool PausePage()
        {
            var f = live.Last; if (f == null) return false;
            for (int i = 0; i < f.layers.Count; i++) { var l = f.layers[i]; if (l.page == "Paused Menu Page") return true; }   // (OriginalLiveUi.Shows)
            return false;
        }

        public void OpenOptions() { ReflexOptions.Open(0); }

        void LateUpdate()
        {
            if (!rt) return;
            if (!live) live = rt.GetComponent<OriginalLiveUi>();
            GamePauseShows = live && live.enabled && live.Running && OriginalPauseMenu.GameOwnsPause(rt) && PausePage();
            bool want = GamePauseShows && !ReflexOptions.IsOpen && rt.Running && !Reflex.Port.Replay.ReflexReplay.ViewerOpen;
            if (want != shown)
            {
                if (want) { Build(); Fill(); }
                shown = want;
                if (root != null) root.style.display = want ? DisplayStyle.Flex : DisplayStyle.None;
            }
            // the mouse for the panel while the game's page shows (OriginalInput captures it while driving in a player build)
            Reflex.Port.Platform.ReflexCursor.Want(this, shown);   // (a pad player keeps it hidden: ReflexCursor)
        }

        void OnDisable() { if (root != null) root.style.display = DisplayStyle.None; shown = false; Reflex.Port.Platform.ReflexCursor.Want(this, false); }
    }
}
