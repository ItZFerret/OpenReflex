// The camera's name for a moment on every switch (w-campresets) where neither HUD shows it: the MODERN HUD has its own toast (ReflexHudModel.cameraToast), the
// hand-built CLASSIC HUD its SinglePlayerCameraLabel (OriginalHud: MXUICameraNameComponent's 2 s, now on every switch and with a custom preset's name); this
// covers CLASSIC with the game's live HUD layers (OriginalLiveUi: the open core does not drive the game's camera name) and the HUD hidden (H, HUD VISIBILITY
// off: the switch was asked for, so it still says which camera it is). The same small panel as the MODERN toast (ReflexHudKit: the translucent strip,
// Barlow), bottom centre, 1.8 s. A built-in shows its name (VISUAL, GAME PLAY, USER, 1ST PERSON, DYNAMIC FPV), a custom preset the player's.
// Not over the menus, the pause pages, the replay viewer, the intro card or the post-race pages; OPTIONS > CAMERA & HUD > CAMERA NAME OFF hides both toasts.
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    [DisallowMultipleComponent, DefaultExecutionOrder(910)]
    public sealed class ReflexCameraToast : MonoBehaviour
    {
        const float Hold = 1.8f;
        OriginalWorldRuntime rt; ReflexModernHud modern;
        Canvas canvas; HudRect box; Text label; HudGroup group;
        int lastKey = -1; float until = -1f;

        /// On screen now, and what it says (tests).
        public bool Visible => group != null && group.Active;
        public string Text => label ? label.text : "";

        void Awake() { rt = GetComponent<OriginalWorldRuntime>(); }

        void Build()
        {
            if (group != null) return;
            var go = new GameObject("Camera toast");
            go.transform.SetParent(transform, false);
            var root = ReflexHudKit.CreateCanvas(go, 12, out canvas);   // (over the game's HUD, under the pause page: 50)
            box = ReflexHudKit.Panel(root, "Notice", new Vector2(0.5f, 0), new Vector2(0.5f, 0), new Vector2(0, 120), new Vector2(520, 48), 160f, 160f, 6f);
            label = ReflexHudKit.Fill(box.rectTransform, "Text", ReflexHudKit.Medium, ReflexHudKit.TypeBody, ReflexHudKit.White, TextAnchor.MiddleCenter);
            group = new HudGroup(box.rectTransform, new Vector2(0, -12), 0.22f, 0.25f);
            group.Snap(false);
        }

        void LateUpdate()
        {
            if (!rt) return;
            if (!modern) modern = GetComponent<ReflexModernHud>();
            float now = Time.unscaledTime;
            int key = ReflexCameraPresets.ViewKey(rt.cameraMode);
            if (lastKey >= 0 && key != lastKey && rt.Running && ReflexSettings.CameraToast)
            {
                Build();
                ReflexHudKit.SetText(label, ReflexCameraPresets.DisplayName(rt.cameraMode));
                until = now + Hold;
            }
            lastKey = key;
            if (group == null) return;
            bool menus = ReflexOptions.IsOpen || rt.OwnMenuActive || rt.pauseMenu && rt.pauseMenu.IsOpen || OriginalPostRace.Active || rt.IntroCardShown || rt.paused;
            bool show = now < until && rt.Running && !menus && !(modern && modern.Shown) && !(rt.originalHud && rt.originalHud.CameraLabelShown);
            group.Set(show, Time.unscaledDeltaTime, ReflexSettings.ReduceMotion);
            if (canvas.enabled != (show || group.Active)) canvas.enabled = show || group.Active;
        }
    }
}
