// The menus' input core (one for every menu: the shell's screens, the options pages, the pause / results / challenge pages, the garage):
//   OuStepper   turns a direction (d-pad, stick, arrows) into discrete steps with a first delay and a repeat that speeds up. A stick
//               needs hysteresis (press at 0.6, hold down to 0.35) and an axis lock, or a stick resting near the threshold fires a step
//               every frame it flickers across it, and a diagonal flips between the two axes. The state is process-wide (see
//               OuMenuInput): a page that rebuilds itself on a step makes a new OuMenuInput, which used to start the hold over and step
//               again at once.
//   OuDevice    which device the player used last (keyboard or which pad), from real presses and stick movement, not from the devices'
//               lastUpdateTime (a pad's noise keeps it newest, so the hints stayed on pad glyphs while the keyboard was in use).
//               The hint bars follow it: a key cap changes when the device does.
//   OuMenuInput the actions: move, submit, cancel, tabs (bumpers), alt / find (Y / X), page (triggers), look (right stick) and zoom.
// OuNav (OuUi.cs) is what consumes them.
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Utilities;

namespace Reflex.Port.OpenFrontEnd
{
    /// Direction -> steps. Pure (no Unity input), so the repeat and the hysteresis are testable.
    public sealed class OuStepper
    {
        public float press = 0.6f, release = 0.35f;
        /// Seconds before the first repeat, between repeats, and between repeats once fastAfter repeats went by.
        public float firstDelay = 0.4f, interval = 0.13f, fastInterval = 0.07f;
        public int fastAfter = 8;
        Vector2Int held; float nextAt; int repeats;

        public Vector2Int Held => held;
        public void Reset() { held = Vector2Int.zero; repeats = 0; nextAt = 0; }

        /// This frame's step (zero while waiting): one step when the direction starts, then repeats while it is held.
        public Vector2Int Step(Vector2 v, float now)
        {
            var want = Want(v);
            if (want == Vector2Int.zero) { held = want; repeats = 0; return want; }
            if (want != held) { held = want; repeats = 0; nextAt = now + firstDelay; return want; }
            if (now >= nextAt)
            {
                repeats++;
                nextAt = now + (repeats >= fastAfter ? fastInterval : interval);
                return want;
            }
            return Vector2Int.zero;
        }

        Vector2Int Want(Vector2 v)
        {
            float ax = Mathf.Abs(v.x), ay = Mathf.Abs(v.y);
            if (held.y != 0)
            {
                float a = v.y * held.y;
                bool turn = ax >= press && ax > a + 0.2f;   // a clear turn onto the other axis
                if (a >= release && !turn) return held;
            }
            else if (held.x != 0)
            {
                float a = v.x * held.x;
                bool turn = ay >= press && ay > a + 0.2f;
                if (a >= release && !turn) return held;
            }
            if (ay >= press && ay >= ax) return new Vector2Int(0, v.y > 0 ? 1 : -1);
            if (ax >= press) return new Vector2Int(v.x > 0 ? 1 : -1, 0);
            return Vector2Int.zero;
        }
    }

    /// The device in use, for the glyphs. Keyboard and mouse count as the keyboard.
    public static class OuDevice
    {
        static bool pad; static Gamepad lastPad; static bool hooked, seeded; static int pollFrame = -1;
        static IDisposable subscription;
        /// Tests: an injected device (null = the real one).
        public static bool? Injected;
        /// Raised when the last device changed between keyboard and pad.
        public static event Action Changed;

        /// A pad was used after the keyboard (true), or the other way round.
        public static bool PadLast
        {
            get
            {
                if (Injected.HasValue) return Injected.Value;
                Poll();
                return pad;
            }
        }

        /// The pad used last (null: none yet).
        public static Gamepad LastPad => lastPad != null && lastPad.added ? lastPad : Gamepad.current;

        /// Notes a device use (tests, and the presses and sticks Poll sees).
        public static void Note(bool padUsed, Gamepad g = null)
        {
            if (g != null) lastPad = g;
            if (padUsed == pad) return;
            pad = padUsed;
            Changed?.Invoke();
        }

        static void Hook()
        {
            if (hooked) return;
            hooked = true;
            try
            {
                // any button press on any device: the control's device says which
                subscription = InputSystem.onAnyButtonPress.Call(control =>
                {
                    var d = control.device;
                    if (d is Gamepad g) Note(true, g);
                    else if (d is Keyboard) Note(false);
                });
            }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] device tracking: " + e.Message); }
        }

        /// Once a frame: hooks the presses, and a stick pushed hard counts as the pad.
        public static void Poll()
        {
            if (pollFrame == Time.frameCount) return;
            pollFrame = Time.frameCount;
            Hook();
            if (!seeded)
            {
                seeded = true;
                pad = Gamepad.current != null && Keyboard.current == null;
            }
            foreach (var g in Gamepad.all)
            {
                if (g.leftStick.ReadValue().sqrMagnitude > 0.49f || g.rightStick.ReadValue().sqrMagnitude > 0.49f || g.dpad.ReadValue().sqrMagnitude > 0.1f) { Note(true, g); break; }
            }
        }

        public static void Shutdown() { subscription?.Dispose(); subscription = null; hooked = false; }
    }

    /// Menu actions (keyboard + every gamepad). The direction repeat is shared by every instance.
    public class OuMenuInput : IDisposable
    {
        readonly InputActionMap _map;
        Gamepad submittedPad;
        readonly InputAction _move, _submit, _cancel, _tabPrev, _tabNext, _alt, _find, _pagePrev, _pageNext, _look, _zoomIn, _zoomOut;

        // One hold for the whole process: a rebuilt page (new OuMenuInput) goes on with the direction that is held instead of firing it
        // again, and the shell, the options pages and a pause page all read the same step in a frame.
        static readonly OuStepper s_stepper = new OuStepper();
        static int s_stepFrame = -1; static Vector2Int s_step;
        /// Tests: the clock (default Time.unscaledTime) and the frame (default Time.frameCount).
        public static Func<float> Clock = () => Time.unscaledTime;
        public static Func<int> Frame = () => Time.frameCount;
        /// Tests: a direction / button injected for the next frame (headless runs have no devices).
        public Vector2Int injectStep; public bool injectSubmit, injectCancel, injectAlt, injectFind; public int injectTab, injectPage;
        public Vector2 injectLook; public float injectZoom;

        public OuMenuInput()
        {
            _map = new InputActionMap("OpenReflexMenu");
            _move = _map.AddAction("Move", InputActionType.Value, expectedControlLayout: "Vector2");
            _move.AddCompositeBinding("2DVector(mode=2)")
                .With("Up", "<Keyboard>/upArrow").With("Down", "<Keyboard>/downArrow")
                .With("Left", "<Keyboard>/leftArrow").With("Right", "<Keyboard>/rightArrow");
            _move.AddBinding("<Gamepad>/dpad");
            _move.AddBinding("<Gamepad>/leftStick");
            _submit = _map.AddAction("Submit", InputActionType.Button);
            _submit.AddBinding("<Keyboard>/enter");
            _submit.AddBinding("<Keyboard>/numpadEnter");
            _submit.AddBinding("<Keyboard>/space");
            _submit.AddBinding("<Gamepad>/buttonSouth");
            _submit.performed += context => submittedPad = context.control.device as Gamepad;
            _cancel = _map.AddAction("Cancel", InputActionType.Button);
            _cancel.AddBinding("<Keyboard>/escape");
            _cancel.AddBinding("<Gamepad>/buttonEast");
            _tabPrev = _map.AddAction("TabPrev", InputActionType.Button);
            _tabPrev.AddBinding("<Keyboard>/q");
            _tabPrev.AddBinding("<Keyboard>/pageUp");
            _tabPrev.AddBinding("<Gamepad>/leftShoulder");
            _tabNext = _map.AddAction("TabNext", InputActionType.Button);
            _tabNext.AddBinding("<Keyboard>/e");
            _tabNext.AddBinding("<Keyboard>/pageDown");
            _tabNext.AddBinding("<Gamepad>/rightShoulder");
            _alt = _map.AddAction("Alt", InputActionType.Button);
            _alt.AddBinding("<Keyboard>/f");
            _alt.AddBinding("<Gamepad>/buttonNorth");
            _find = _map.AddAction("Find", InputActionType.Button);
            _find.AddBinding("<Keyboard>/slash");
            _find.AddBinding("<Keyboard>/tab");
            _find.AddBinding("<Gamepad>/buttonWest");
            // the triggers page through a long list (the garage zooms with them)
            _pagePrev = _map.AddAction("PagePrev", InputActionType.Button);
            _pagePrev.AddBinding("<Gamepad>/leftTrigger");
            _pageNext = _map.AddAction("PageNext", InputActionType.Button);
            _pageNext.AddBinding("<Gamepad>/rightTrigger");
            // the right stick looks around (the garage's camera); the keys , . turn it
            _look = _map.AddAction("Look", InputActionType.Value, expectedControlLayout: "Vector2");
            _look.AddBinding("<Gamepad>/rightStick");
            _look.AddCompositeBinding("2DVector(mode=2)")
                .With("Up", "<Keyboard>/k").With("Down", "<Keyboard>/j")
                .With("Left", "<Keyboard>/comma").With("Right", "<Keyboard>/period");
            _zoomIn = _map.AddAction("ZoomIn", InputActionType.Value, expectedControlLayout: "Axis");
            _zoomIn.AddBinding("<Gamepad>/rightTrigger");
            _zoomIn.AddBinding("<Keyboard>/equals");
            _zoomIn.AddBinding("<Keyboard>/numpadPlus");
            _zoomOut = _map.AddAction("ZoomOut", InputActionType.Value, expectedControlLayout: "Axis");
            _zoomOut.AddBinding("<Gamepad>/leftTrigger");
            _zoomOut.AddBinding("<Keyboard>/minus");
            _zoomOut.AddBinding("<Keyboard>/numpadMinus");
            _map.Enable();
        }

        public void Dispose() { _map.Disable(); _map.Dispose(); }
        public void Enable(bool on) { if (on) _map.Enable(); else _map.Disable(); }

        /// Clears the shared direction hold (a screen that must not inherit one, and tests).
        public static void ResetHold() { s_stepper.Reset(); s_stepFrame = -1; s_step = Vector2Int.zero; }

        /// This frame's direction step (-1..1 per axis, one axis at a time), with repeat while held. The same value however many
        /// callers ask in a frame.
        public Vector2Int Step()
        {
            if (injectStep != Vector2Int.zero) { var s = injectStep; injectStep = Vector2Int.zero; return s; }
            OuDevice.Poll();
            return StepShared(_move.ReadValue<Vector2>());
        }

        /// The process-wide step of this frame for the direction v (the same however many callers ask in a frame).
        public static Vector2Int StepShared(Vector2 v)
        {
            int frame = Frame();
            if (frame == s_stepFrame) return s_step;
            s_stepFrame = frame;
            return s_step = s_stepper.Step(v, Clock());
        }

        static bool Take(ref bool b) { bool v = b; b = false; return v; }
        public bool Submit
        {
            get
            {
                bool actual = _submit.WasPressedThisFrame();
                if (actual) Reflex.Port.OriginalWorld.LocalGamepadRouting.RememberMenuSubmit(submittedPad);
                return Take(ref injectSubmit) | actual;
            }
        }
        public bool Cancel => Take(ref injectCancel) | _cancel.WasPressedThisFrame();
        public int Tab { get { int t = injectTab; injectTab = 0; return t != 0 ? t : (_tabNext.WasPressedThisFrame() ? 1 : 0) - (_tabPrev.WasPressedThisFrame() ? 1 : 0); } }
        public bool Alt => Take(ref injectAlt) | _alt.WasPressedThisFrame();
        public bool Find => Take(ref injectFind) | _find.WasPressedThisFrame();
        /// The triggers: -1 = left, +1 = right, pressed this frame.
        public int Page { get { int p = injectPage; injectPage = 0; return p != 0 ? p : (_pageNext.WasPressedThisFrame() ? 1 : 0) - (_pagePrev.WasPressedThisFrame() ? 1 : 0); } }
        /// The right stick / look keys now, without taking an injected value (the garage reads Look itself).
        public Vector2 LookPeek => injectLook != Vector2.zero ? injectLook : _look.ReadValue<Vector2>();
        /// The right stick / , . J K (held): x turns, y tilts.
        public Vector2 Look { get { if (injectLook != Vector2.zero) { var l = injectLook; injectLook = Vector2.zero; return l; } return _look.ReadValue<Vector2>(); } }
        /// The triggers / - = (held): > 0 in, < 0 out.
        public float Zoom { get { if (injectZoom != 0) { float z = injectZoom; injectZoom = 0; return z; } return _zoomIn.ReadValue<float>() - _zoomOut.ReadValue<float>(); } }

        /// Whether a keyboard key or a gamepad was used last (for the button hints).
        public static bool GamepadLast => OuDevice.PadLast;
    }
}
