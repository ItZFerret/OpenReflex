using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;
using Reflex.Port.Options;

namespace Reflex.Port.OriginalWorld
{
    // Open Unleashed's live eye adjustment; the gamepad continues riding.
    [DefaultExecutionOrder(-20)]
    public sealed class OriginalOnboardAdjust : MonoBehaviour
    {
        public static bool Active { get; private set; }
        static int capturedFrame = -1;
        public static bool KeyboardCaptured => Active || capturedFrame == Time.frameCount;
        OriginalWorldRuntime runtime;
        ReflexEventSelect eventSelect;
        bool changed;
        UIDocument document;
        GameObject overlay;
        Label notice;
        void Awake() { runtime = GetComponent<OriginalWorldRuntime>(); eventSelect = GetComponent<ReflexEventSelect>(); }
        void OnDisable() { End(); }
        void OnDestroy() { End(); if (overlay) Destroy(overlay); }
        public void End()
        {
            if (!Active) return;
            Active = false; capturedFrame = Time.frameCount;
            if (changed) ReflexSettings.Save();
            changed = false;
            if (notice != null) notice.style.display = DisplayStyle.None;
        }
        void Update()
        {
            if (!runtime) return;
            var kb = Keyboard.current;
            bool menu = runtime.OwnMenuActive || runtime.optionsMenu && runtime.optionsMenu.IsOpen || runtime.pauseMenu && runtime.pauseMenu.IsOpen || eventSelect && eventSelect.IsOpen;
            bool can = runtime.Running && !runtime.paused && !menu && runtime.cameraMode == 1 && runtime.onboard && runtime.onboard.Active;
            bool toggle = kb != null && kb.f2Key.wasPressedThisFrame && !OriginalInput.IsCameraKey(Key.F2);
            if (!Active)
            {
                if (can && toggle) { Active = true; changed = false; capturedFrame = Time.frameCount; Show(); }
                return;
            }
            capturedFrame = Time.frameCount;
            if (!can || toggle || kb != null && (kb.escapeKey.wasPressedThisFrame || kb.enterKey.wasPressedThisFrame)) { End(); return; }
            if (kb == null) return;
            Vector3 delta = new Vector3((kb.dKey.isPressed ? 1 : 0) - (kb.aKey.isPressed ? 1 : 0),
                (kb.eKey.isPressed ? 1 : 0) - (kb.qKey.isPressed ? 1 : 0),
                (kb.wKey.isPressed ? 1 : 0) - (kb.sKey.isPressed ? 1 : 0));
            Vector3 offset = ReflexSettings.OnboardOffset;
            if (kb.rKey.wasPressedThisFrame) { offset = Vector3.zero; changed = true; }
            if (delta != Vector3.zero) { offset += delta * (.75f * (kb.leftShiftKey.isPressed || kb.rightShiftKey.isPressed ? .25f : 1f) * Time.unscaledDeltaTime); changed = true; }
            if (changed)
            {
                ReflexSettings.SetOnboardOffset(offset, false);
                runtime.onboard.offset = ReflexSettings.OnboardOffset;
            }
            Show();
        }
        void Show()
        {
            if (!document)
            {
                var panel = Resources.Load<PanelSettings>("OpenFrontEnd/OuPanelSettings");
                if (!panel) return;
                overlay = new GameObject("First-person adjustment"); overlay.transform.SetParent(transform, false);
                document = overlay.AddComponent<UIDocument>(); document.panelSettings = panel; document.sortingOrder = 900;
                var root = document.rootVisualElement; root.pickingMode = PickingMode.Ignore;
                notice = new Label(); notice.pickingMode = PickingMode.Ignore;
                notice.style.position = Position.Absolute; notice.style.bottom = 80; notice.style.left = 60; notice.style.right = 60;
                notice.style.whiteSpace = WhiteSpace.Normal; notice.style.unityTextAlign = TextAnchor.MiddleCenter;
                notice.style.fontSize = 22; notice.style.color = Color.white; notice.style.backgroundColor = new Color(0, 0, 0, .8f);
                notice.style.paddingTop = 12; notice.style.paddingBottom = 12; root.Add(notice);
            }
            notice.style.display = DisplayStyle.Flex;
            Vector3 o = ReflexSettings.OnboardOffset;
            notice.text = $"FIRST-PERSON ADJUST  ·  UP {o.y:+0.00;-0.00;0.00} FT   FORWARD {o.z:+0.00;-0.00;0.00} FT   RIGHT {o.x:+0.00;-0.00;0.00} FT\nW/S FORWARD/BACK · Q/E DOWN/UP · A/D LEFT/RIGHT · SHIFT FINE · R RESET · F2 / ENTER / ESC DONE";
        }
    }
}
