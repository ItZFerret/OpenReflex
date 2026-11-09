// The one cursor policy (w-cursor). Every page and the world's input go through here instead of setting Cursor.visible / lockState.
//
// Why: the world locks the cursor while driving (OriginalInput, as the game's SetCursor(NULL) + ClipCursor), and every menu used to
// "free" it on open with lockState = None + visible = true. Unity moves the cursor to the middle of the window on the Locked -> None
// switch, so a controller-only player saw the pointer pop up in the middle of the screen on every pause, every results / weekend /
// challenge page, and on going back to the main menu, as if the window had been alt-tabbed.
//
// The policy, applied once a frame (LateUpdate) and on every change:
//   driving (a Capture owner, no page wants a pointer, window focused)  -> Locked, hidden
//   a page wants a pointer, or nothing is driving:
//     the mouse was used last (moved, clicked, scrolled)              -> None, shown
//     a pad was used last                                             -> hidden, and the lock state is left as it is (no recentre)
//   a page that Needs the pointer (a keyboard/mouse page)             -> None, shown, whatever was used last
// The mouse counts once it moves a few pixels, clicks or scrolls; a pad once a button is pressed or a stick / d-pad is pushed. The
// keyboard does not change it. A lock change, a focus change and the first frames ignore the mouse's delta (a recentre can report one).
// The editor and test players (-reflexTestWindow) never touch the cursor: they must not lock, hide or move the user's mouse.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Utilities;

namespace Reflex.Port.Platform
{
    public static class ReflexCursor
    {
        static readonly HashSet<object> wants = new HashSet<object>(), needs = new HashSet<object>(), captures = new HashSet<object>();
        static bool pointer, seeded;
        static int ignoreMouseUntil = 10;
        static IDisposable presses;
        static Runner runner;
        static bool quitting;

        /// The mouse was used after the pads (true), or a pad after the mouse.
        public static bool PointerLast { get { Seed(); return pointer; } }

        /// A page shows a pointer while it is up (shown only when the mouse is in use; a pad player keeps it hidden).
        public static void Want(object owner, bool on) { if (owner != null && (on ? wants.Add(owner) : wants.Remove(owner))) Apply(); }

        /// A keyboard/mouse page that shows the pointer even after a pad was used.
        public static void Need(object owner, bool on) { if (owner != null && (on ? needs.Add(owner) : needs.Remove(owner))) Apply(); }

        /// The world is driving: lock and hide the cursor (unless a page wants a pointer).
        public static void Capture(object owner, bool on) { if (owner != null && (on ? captures.Add(owner) : captures.Remove(owner))) Apply(); }

        /// Notes a device use (the runner's polling; tests).
        public static void NotePointer(bool used)
        {
            Seed();
            if (pointer == used) return;
            pointer = used;
            Apply();
        }

        static bool Hands => !Application.isEditor && !Reflex.Port.Content.ReflexWindowOverride.TestWindow;

        static void Seed()
        {
            if (seeded) return;
            seeded = true;
            pointer = Gamepad.all.Count == 0 && Joystick.all.Count == 0;   // a pad plugged in at start: hidden until the mouse moves
        }

        static readonly Predicate<object> Gone = o => o is UnityEngine.Object u && !u;

        public static void Apply()
        {
            Ensure();
            if (!Hands) return;
            Seed();
            wants.RemoveWhere(Gone); needs.RemoveWhere(Gone); captures.RemoveWhere(Gone);
            bool focused = Application.isFocused;
            if (needs.Count > 0) { Set(CursorLockMode.None, true); return; }
            if (captures.Count > 0 && wants.Count == 0) { if (focused) Set(CursorLockMode.Locked, false); else Set(CursorLockMode.None, pointer); return; }
            if (pointer) { Set(CursorLockMode.None, true); return; }
            // a pad: hidden where it is. Leaving Locked here is what keeps it from jumping to the middle of the window.
            Set(focused ? Cursor.lockState : CursorLockMode.None, false);
        }

        static void Set(CursorLockMode lockMode, bool visible)
        {
            if (Cursor.lockState != lockMode) { Cursor.lockState = lockMode; IgnoreMouse(); }
            if (Cursor.visible != visible) Cursor.visible = visible;
        }

        static void IgnoreMouse() => ignoreMouseUntil = Math.Max(ignoreMouseUntil, Time.frameCount + 2);

        static void Ensure()
        {
            if (runner || quitting || !Application.isPlaying) return;
            var go = new GameObject("ReflexCursor") { hideFlags = HideFlags.HideAndDontSave };
            UnityEngine.Object.DontDestroyOnLoad(go);
            runner = go.AddComponent<Runner>();
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot() { Application.quitting += () => quitting = true; Ensure(); Apply(); }

        static void Poll()
        {
            if (presses == null)
            {
                try
                {
                    presses = InputSystem.onAnyButtonPress.Call(control =>
                    {
                        var d = control.device;
                        if (d is Gamepad || d is Joystick) NotePointer(false);
                        else if (d is Mouse && Time.frameCount > ignoreMouseUntil) NotePointer(true);
                    });
                }
                catch (Exception e) { presses = new Nothing(); Debug.LogWarning("[OpenReflex] cursor device tracking: " + e.Message); }
            }
            var m = Mouse.current;
            if (m != null && Time.frameCount > ignoreMouseUntil &&
                (m.delta.ReadValue().sqrMagnitude >= 9f || m.scroll.ReadValue().sqrMagnitude > 0f ||
                 m.leftButton.wasPressedThisFrame || m.rightButton.wasPressedThisFrame || m.middleButton.wasPressedThisFrame))
                NotePointer(true);
            foreach (var g in Gamepad.all)
                if (g.leftStick.ReadValue().sqrMagnitude > 0.49f || g.rightStick.ReadValue().sqrMagnitude > 0.49f || g.dpad.ReadValue().sqrMagnitude > 0.1f) { NotePointer(false); break; }
        }

        sealed class Nothing : IDisposable { public void Dispose() { } }

        sealed class Runner : MonoBehaviour
        {
            void Update() => Poll();
            void LateUpdate() => Apply();
            void OnApplicationFocus(bool focus) { IgnoreMouse(); Apply(); }
            void OnDestroy() { if (runner == this) { runner = null; presses?.Dispose(); presses = null; } }
        }
    }
}
