using System;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    /// The document and the input of the pages (one per session).
    public sealed class ReflexOptionsHost : MonoBehaviour
    {
        public UIDocument Doc { get; private set; }
        public VisualElement Root { get; private set; }
        VisualElement layer, hints;
        OptNav nav; OptMenuInput input; ReflexOptionsMenu menu;
        Action onClosed;
        public bool IsOpen { get; private set; }
        public int Tab => menu != null ? menu.Tab : 0;
        OriginalWorldRuntime pausedRuntime; bool pausedBefore, audioPausedBefore;

        void Build()
        {
            if (Root != null) return;
            OptUi.EnsureEventSystem();
            Root = OptUi.CreateDocument(gameObject, 0);
            Doc = GetComponent<UIDocument>();
            layer = OptUi.Div("mxa-layer", Root);
            hints = OptUi.Div("mxa-hints", Root);
            nav = new OptNav { Screen = layer };
            input = new OptMenuInput();
            menu = new ReflexOptionsMenu(this, layer, hints, nav, Hide);
            OptUi.Sound ??= PlayUiSound;
        }

        static void PlayUiSound(string name)
        {
            string ui = name == "UpDown" ? "UpDownDirectional" : name == "LeftRight" ? "LeftRightDirectional" : name;
            var fe = FindAnyObjectByType<Reflex.Port.FrontEnd.FrontEndManager>();
            if (fe) { fe.PlaySound(ui); return; }
            var rt = ReflexClientLive.Runtime;
            if (rt) OriginalHud.UiSound(rt, "play", ui);
        }

        public void Show(int tab, Action closed, bool keepCallback = false)
        {
            Build();
            if (!keepCallback) onClosed = closed;
            if (!IsOpen)
            {
                IsOpen = true;
                Root.style.display = DisplayStyle.Flex;
                input.Enable(true);
                PauseRace(true);
            }
            menu.ShowOptions(tab);
        }

        /// The SCREEN ADJUSTMENT page on its own (the first-run welcome): CONTINUE / Back close the pages.
        public void ShowScreenAdjustment(Action closed)
        {
            Show(ReflexOptions.Video, closed);
            menu.ShowScreenAdjustment(true);
        }

        public void Hide()
        {
            if (!IsOpen) return;
            IsOpen = false;
            menu.Closing();
            layer.Clear(); hints.Clear();
            Root.style.display = DisplayStyle.None;
            input.Enable(false);
            PauseRace(false);
            ReflexSettings.Save();
            ReflexOptions.RaiseClosed();
            var cb = onClosed; onClosed = null;
            try { cb?.Invoke(); } catch (Exception e) { Debug.LogException(e); }
        }

        // in a race: the world stops while the pages are up (as OriginalOptionsMenu / OriginalPauseMenu do)
        void PauseRace(bool open)
        {
            var rt = ReflexClientLive.Runtime;
            HideCanvases(open);
            if (open)
            {
                pausedRuntime = rt;
                if (!rt) return;
                pausedBefore = rt.paused; audioPausedBefore = AudioListener.pause;
                rt.paused = true; AudioListener.pause = true;
                Reflex.Port.Platform.ReflexCursor.Want(this, true);
                return;
            }
            Reflex.Port.Platform.ReflexCursor.Want(this, false);
            if (!pausedRuntime) return;
            pausedRuntime.paused = pausedBefore;
            if (!(pausedRuntime.pauseMenu && pausedRuntime.pauseMenu.IsOpen)) AudioListener.pause = audioPausedBefore;
            pausedRuntime.HoldKeysUntilReleased();
            pausedRuntime = null;
        }

        // the uGUI pages and HUDs (the original front end's, the race's) step aside while the pages are up
        readonly System.Collections.Generic.List<(CanvasGroup group, float alpha, bool added)> hiddenCanvases = new System.Collections.Generic.List<(CanvasGroup, float, bool)>();
        void HideCanvases(bool hide)
        {
            if (hide)
            {
                foreach (var c in FindObjectsByType<Canvas>(FindObjectsSortMode.None))
                {
                    if (!c || !c.isRootCanvas || c.renderMode != RenderMode.ScreenSpaceOverlay) continue;
                    var g = c.GetComponent<CanvasGroup>(); bool added = !g;
                    if (added) g = c.gameObject.AddComponent<CanvasGroup>();
                    hiddenCanvases.Add((g, g.alpha, added)); g.alpha = 0f;
                }
                return;
            }
            foreach (var (g, a, added) in hiddenCanvases) if (g) { if (added) Destroy(g); else g.alpha = a; }
            hiddenCanvases.Clear();
        }

        void Update()
        {
            if (!IsOpen) return;
            menu.Tick();
            nav.Update(input);
        }

        void OnDestroy() { input?.Dispose(); }
    }
}
