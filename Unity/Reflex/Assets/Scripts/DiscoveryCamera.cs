using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port
{
    // Inspection controls only. No claim of recovered vehicle movement or physics.
    public sealed class DiscoveryCamera : MonoBehaviour
    {
        public float moveSpeed = 90f;
        public Vector3 overviewPosition = new Vector3(2750, 2900, -3350);
        public Vector3 overviewTarget = new Vector3(0, 300, 0);
        public Vector3 startPosition = new Vector3(1850, 315, -1780);
        public Vector3 startTarget = new Vector3(1450, 240, -1300);
        public bool hasSceneryView;
        public Vector3 sceneryPosition;
        public Vector3 sceneryTarget;

        void Awake()
        {
            Application.runInBackground = true;
            Application.targetFrameRate = 60;
        }

        public void Overview() => Look(overviewPosition, overviewTarget);
        public void StartingArea() => Look(startPosition, startTarget);
        public void Scenery() { if (hasSceneryView) Look(sceneryPosition, sceneryTarget); }
        public void Look(Vector3 position, Vector3 target)
        {
            transform.position = position;
            transform.LookAt(target);
        }

        void Update()
        {
            var keyboard = Keyboard.current;
            var mouse = Mouse.current;
            if (keyboard == null) return;
            if (keyboard.digit1Key.wasPressedThisFrame) Overview();
            if (keyboard.digit2Key.wasPressedThisFrame) StartingArea();
            if (keyboard.digit3Key.wasPressedThisFrame) Scenery();
            if (mouse == null || !mouse.rightButton.isPressed) return;
            var delta = mouse.delta.ReadValue() * 0.12f;
            var angles = transform.eulerAngles;
            float pitch = angles.x > 180 ? angles.x - 360 : angles.x;
            transform.rotation = Quaternion.Euler(Mathf.Clamp(pitch - delta.y, -88, 88), angles.y + delta.x, 0);
            Vector3 move = Vector3.zero;
            if (keyboard.wKey.isPressed) move += transform.forward;
            if (keyboard.sKey.isPressed) move -= transform.forward;
            if (keyboard.aKey.isPressed) move -= transform.right;
            if (keyboard.dKey.isPressed) move += transform.right;
            if (keyboard.eKey.isPressed) move += Vector3.up;
            if (keyboard.qKey.isPressed) move -= Vector3.up;
            transform.position += move * (moveSpeed * Time.unscaledDeltaTime * (keyboard.leftShiftKey.isPressed ? 4 : 1));
        }
    }
}
