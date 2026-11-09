using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Replay;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.Content
{
    // Experimental launcher only. Rebuild the complete session so render and collision scales stay together.
    public sealed class ExperimentalMtxScale : MonoBehaviour
    {
        public static int DefaultFor(TrackMod track) => track?.id == "ParadiseValley" ? 180 : 160;
        static string SaveKey(TrackMod track) => "Experimental.Mtx.ScalePercent." + (track?.id ?? "McKennaRock") + "@" + DefaultFor(track);
        public static int PercentFor(TrackMod track) => RxMapScale.ClampPercent(ReflexSave.GetInt(SaveKey(track), DefaultFor(track)));
        public static int Percent => PercentFor(TrackMods.Bound);
        public static void SetPercentFor(TrackMod track, int percent) { ReflexSave.SetInt(SaveKey(track), RxMapScale.ClampPercent(percent)); ReflexSave.Save(); }
        public static void SetPercent(int percent) => SetPercentFor(TrackMods.Bound, percent);
        public string EventKey { get; set; }
        Label notice;
        OriginalWorldRuntime runtime;
        bool reloading;
        int shown = -1;

        void Start()
        {
            var panel = Resources.Load<PanelSettings>("OpenFrontEnd/OuPanelSettings");
            if (!panel) return;
            var overlay = new GameObject("MTX debug scale"); overlay.transform.SetParent(transform, false);
            var doc = overlay.AddComponent<UIDocument>(); doc.panelSettings = panel; doc.sortingOrder = 900;
            doc.rootVisualElement.pickingMode = PickingMode.Ignore;
            notice = new Label { pickingMode = PickingMode.Ignore };
            notice.style.position = Position.Absolute; notice.style.left = 24; notice.style.bottom = 24;
            notice.style.fontSize = 24; notice.style.color = Color.white;
            notice.style.backgroundColor = new Color(0, 0, 0, .8f);
            notice.style.paddingLeft = notice.style.paddingRight = 12;
            notice.style.paddingTop = notice.style.paddingBottom = 8;
            doc.rootVisualElement.Add(notice);
        }

        public void Apply(int percent)
        {
            percent = RxMapScale.ClampPercent(percent);
            if (reloading || percent == Percent || string.IsNullOrEmpty(EventKey)) return;
            SetPercent(percent);
            int ai=runtime && runtime.opponents>0 ? System.Array.FindIndex(ReflexWorldFiles.AiSkills,x=>x.skill==runtime.aiSkill)+1 : 0;
            int riders=runtime ? runtime.opponents+1 : 1, laps=runtime ? runtime.laps : 0;
            var garage=runtime ? runtime.garage : null;
            reloading = true; shown = -1; runtime = null;
            ReflexEventSelect.Launch(EventKey, ai, remember:false, mapScale:percent / 100f, laps:laps, riders:riders, garage:garage);
            Debug.Log($"[MTX] Debug scale reload: {percent} %");
        }

        void Update()
        {
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            bool running = runtime && runtime.Running;
            var select = runtime ? runtime.GetComponent<ReflexEventSelect>() : null;
            bool mine = select && ExperimentalTerrain.Active;
            if (mine) EventKey = select.Current;
            if (reloading && running && mine) { reloading = false; shown = -1; }
            bool visible = mine && !ReflexReplay.ViewerOpen && !runtime.OwnMenuActive;
            if (notice != null)
            {
                notice.style.display = visible || reloading ? DisplayStyle.Flex : DisplayStyle.None;
                if (shown != Percent)
                {
                    shown = Percent;
                    notice.text = $"MTX SCALE  {shown}%{(reloading ? "  ·  RELOADING…" : "")}\nCtrl+Shift+[ / ]  − / + 5%  ·  reloads track and returns to start";
                }
            }
            var kb = Keyboard.current;
            if (!running || !visible || reloading || runtime.paused || OriginalOnboardAdjust.KeyboardCaptured || kb == null || !kb.ctrlKey.isPressed || !kb.shiftKey.isPressed) return;
            if (kb.leftBracketKey.wasPressedThisFrame && !OriginalInput.IsCameraKey(Key.LeftBracket)) Apply(Percent - 5);
            else if (kb.rightBracketKey.wasPressedThisFrame && !OriginalInput.IsCameraKey(Key.RightBracket)) Apply(Percent + 5);
        }
    }
}
