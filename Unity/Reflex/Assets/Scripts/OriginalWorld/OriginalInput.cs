using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // Input and settings of the running game beyond the keyboard and pad states OriginalWorldRuntime passes each frame
    // (native/drafts/world_runtime/input.cpp, _re/notes/input-settings.md):
    //   * the mouse as the PC game reads it: Raw Input WM_INPUT through the game's own window procedure between frames, so its
    //     RiderReflexMouseX / Y (rider lean) come from the game's own conversion (XenonInputManager::vf43 -> 00817340: dx / 1280 and
    //     dy / 720 per 1/30 s, times MouseSensitivity * 0.05, Y negated, clamped to [-1, 1]);
    //   * the rumble the game writes (XInputSetState) -> Gamepad.SetMotorSpeeds of the same pad (OriginalWorldRuntime.ReadPads order);
    //   * the game's UI settings it reads in the race: DrivingCamera (its own camera presets Visual / Game Play / User / 1st Person),
    //     MouseSensitivity and Vibration, pushed into each new world from the options the player chose (PlayerPrefs ReflexUI.<name>, the
    //     front end's and the pause page's), only those the player has set (else the game's defaults stay: 0 / 50 / 0);
    //   * the player's controller remapping (WP-C, native remap.cpp): the Controller Remap Page writes the game's ControllerRemapping
    //     table, which the save keeps; the original builds the local player's KeyMaps from it when the player is created
    //     (PlayerSlot_FromPlayerList 0054a7b0 -> 00547f30). An arcade world is booted with the default table and gets the save only when
    //     ReflexPersistence attaches it, so the KeyMaps are built again then ("apply", the same call with the same arguments): the
    //     remapped keys and pad inputs drive the race through the game's own KeyMaps.
    // Added by OriginalWorldRuntime; works with an older plugin too (without the exports nothing is sent).
    // Before OriginalWorldRuntime: this Unity frame's mouse is queued before the game frame it starts (the original's message pump runs
    // right before Engine::Frame).
    [DefaultExecutionOrder(-10)]
    public sealed class OriginalInput : MonoBehaviour
    {
        [Tooltip("Feed the mouse to the game (rider lean, as the PC game). Off while a menu is open, the world is paused, or the Dynamic FPV camera's free look has the mouse.")]
        public bool mouse = true;
        [Tooltip("Forward the game's rumble (XInputSetState) to the gamepads.")]
        public bool rumble = true;
        [Tooltip("Hide and lock the cursor while driving, as the game does (SetCursor(NULL) each frame, ClipCursor). Player builds only.")]
        public bool captureCursor = true;

        /// The UI settings pushed into each world (the race-time ones the game reads; the others are the front end's / boot's).
        public static readonly string[] PushedSettings = { "DrivingCamera", "MouseSensitivity", "Vibration" };

        OriginalWorldRuntime runtime;
        OriginalSimulation bound;            // the simulation the settings were pushed into
        ReflexEventSelect eventSelect;
        IntPtr world;                        // the running world (from a call on the simulation thread); IntPtr.Zero when none
        bool available = true;               // the plugin has the input exports
        int pendingDx, pendingDy, pendingWheel, pendingFlags, queued;   // mouse since the last delivery (Interlocked)
        Action<IntPtr> deliver;
        float carryX, carryY, carryWheel;
        bool cursorCaptured;
        // The mouse buttons the game holds (w-lookback): the UP of a button that went down while riding is never dropped when the feed stops
        // (the left button is the stock camera's look back: a lost UP kept the camera looking backwards). MouseButtonFeed.cs.
        readonly MouseButtonFeed buttons = new MouseButtonFeed();
        object mouseTarget;                  // the world / open session the buttons were handed to (a new one holds none)
        readonly ushort[] motors = new ushort[8]; readonly uint[] calls = new uint[4];
        readonly float[] applied = new float[8];
        readonly Gamepad[] appliedDevices = new Gamepad[4];
        int drivingCamera = -1;              // the game's DrivingCamera as last read (it changes it itself: the Camera key cycles presets)
        float nextRead;

        /// What was last sent to each pad's motors (low = left, high = right; 0..1), for tests.
        public float AppliedMotor(int pad, bool right) => applied[pad * 2 + (right ? 1 : 0)];
        /// XInputSetState calls the game made for each pad in this world.
        public uint RumbleCalls(int pad) => calls[pad];
        /// Mouse messages delivered to the game in this world.
        public int MouseMessages { get; private set; }
        /// The game's current DrivingCamera (0..3), -1 before the first read.
        public int DrivingCamera => drivingCamera;
        public bool Available => available;

        void Awake()
        {
            runtime = GetComponent<OriginalWorldRuntime>();
            eventSelect = GetComponent<ReflexEventSelect>();
            deliver = Deliver;
        }

        void OnDisable() { ReleaseCursor(); StopMotors(); }

        bool MenuOpen => runtime && (runtime.OwnMenuActive || runtime.optionsMenu && runtime.optionsMenu.IsOpen || runtime.pauseMenu && runtime.pauseMenu.IsOpen) ||
                         eventSelect && eventSelect.IsOpen || OriginalPostRace.Active;

        void Update()
        {
            if (!runtime) return;
            var sim = runtime.Running ? runtime.Simulation : null;
            if (sim != bound)
            {   // a new world (or none): push the settings, forget the old world
                bound = sim; world = IntPtr.Zero; Array.Clear(calls, 0, 4); MouseMessages = 0; drivingCamera = -1; cameraWasHeld = true;
                Interlocked.Exchange(ref pendingDx, 0); Interlocked.Exchange(ref pendingDy, 0); Interlocked.Exchange(ref pendingWheel, 0); Interlocked.Exchange(ref pendingFlags, 0);
                StopMotors();
                if (sim != null && available) PushSettings(sim);
            }
            bool driving = runtime.Running && !runtime.paused && !MenuOpen;
            CameraCycleTick(driving);
            CaptureCursor(driving);
            if (sim == null)
            {
                // the open core: the mouse reaches the game's Raw Input path and the pads' motors come from its XInputSetState
                // (MOUSE SENSITIVITY and RUMBLE are the game's UI settings: ReflexClientLive.SyncGameSettings puts them into the session)
                var open = runtime.Running ? runtime.SimDriver?.Open : null;
                if (open != null)
                {
                    // (MOUSE LEAN off: nothing is fed, but a button the game still holds gets its UP: w-lookback)
                    if (mouse) ReadMouse(null, driving && Reflex.Port.Options.ReflexSettings.MouseLean && !(runtime.cameraMode == 1 && runtime.onboard && runtime.onboard.freeLook), open);
                    if (rumble) { for (int i = 0; i < 4; i++) { open.Motors(i, out ushort l, out ushort r); motors[2 * i] = l; motors[2 * i + 1] = r; } ApplyRumble(driving); }
                    return;
                }
                if (!runtime.Running) ReleaseCursor();
                return;
            }
            if (mouse && available) ReadMouse(sim, driving && !(runtime.cameraMode == 1 && runtime.onboard && runtime.onboard.freeLook));
            if (rumble && available && world != IntPtr.Zero) Rumble(driving);
            if (available && Time.unscaledTime >= nextRead) { nextRead = Time.unscaledTime + 0.25f; sim.BetweenFrames(ReadSettings); }
        }

        // ------------------------------------------------------------------------------------------------ settings
        void PushSettings(OriginalSimulation sim)
        {
            foreach (var name in PushedSettings)
            {
                string key = "ReflexUI." + name;
                if (!PlayerPrefs.HasKey(key)) continue;   // never set by the player: the game's own default stays
                int v = PlayerPrefs.GetInt(key);
                if (name == "DrivingCamera") v = Mathf.Clamp(v, 0, 3);
                SetSetting(sim, name, v, 2);   // as the settings the world was made with (the race on the gate gets the DrivingCamera too)
            }
            sim.BetweenFrames(w => { world = w; ApplyRemap(w); });
        }

        // ------------------------------------------------------------------------------------------------ controller remapping (WP-C)
        /// The game's report of the last "apply" (JSON: the players whose KeyMaps were built, the table), for tests and logs.
        public static string LastRemap { get; private set; }
        static readonly HashSet<int> boundDik = new HashSet<int>();
        /// Whether the running game binds this key in its ControllerRemapping table (keyboard row, all 19 columns). The port's tool keys
        /// (F4..F12, M, Insert, Delete; OriginalWorldRuntime keeps them from the game) should yield such a key to the game.
        public static bool GameBinds(Key key)
        {
            foreach (var kv in DirectInputMap.Table) if (kv.Key == key) lock (boundDik) return boundDik.Contains(kv.Value);
            return false;
        }

        // On the simulation thread, before the world's next frame (the attach already loaded the save): Controller_BuildKeyMaps for the
        // local players from the table the game now has (a career world, booted with its save, gets the same KeyMaps again).
        void ApplyRemap(IntPtr w)
        {
            if (!remapAvailable) return;
            try
            {
                var buf = new byte[1 << 14];
                int n = ReflexInputNative.reflex_world_remap(w, "apply", buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = ReflexInputNative.reflex_world_remap(w, "apply", buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Input] remap apply: " + ReflexWorldNative.Error(w, "failed")); return; }
                LastRemap = System.Text.Encoding.UTF8.GetString(buf, 0, n);
                // the keys the game binds: the table's keyboard ids back to DIK through the game's scan-code table (00a95848)
                var scan = new byte[256]; ReflexWorldNative.reflex_world_read(w, 0xa95848, scan, 256);
                var m = System.Text.RegularExpressions.Regex.Match(LastRemap, "\"kb\":\\[([^\\]]*)\\]");
                var ids = new HashSet<int>(); if (m.Success) foreach (var x in m.Groups[1].Value.Split(',')) if (int.TryParse(x, out int id)) ids.Add(id);
                lock (boundDik) { boundDik.Clear(); for (int d = 0; d < 256; d++) if (scan[d] != 0 && ids.Contains(scan[d])) boundDik.Add(d); }
                CacheCameraBindings(LastRemap, scan);
            }
            catch (EntryPointNotFoundException) { remapAvailable = false; }
        }
        bool remapAvailable = true;
        static readonly HashSet<Key> cameraKeys = new HashSet<Key>();
        static int cameraPadInput, lookBackPadInput;
        public static bool LookBackHeld => CameraPadHeld(LocalGamepadRouting.ActiveLocalPad, lookBackPadInput);
        static readonly int[] CameraButtonMasks = { 0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0x10, 0x20, 0x40, 0x80 };
        static readonly int[] CameraDpadMasks = { 1, 8, 2, 4 };
        public static bool RightStickRides { get; private set; } = true;
        bool cameraWasHeld;
        public static bool IsCameraKey(Key key) { lock (cameraKeys) return cameraKeys.Contains(key); }
        static bool CameraPadHeld(Gamepad g, int id)
        {
            if (g == null) return false;
            switch (id)
            {
                case 1: return Mathf.Abs(g.leftStick.ReadUnprocessedValue().x) > .5f;
                case 2: return Mathf.Abs(g.leftStick.ReadUnprocessedValue().y) > .5f;
                case 3: return Mathf.Abs(g.rightStick.ReadUnprocessedValue().x) > .5f;
                case 4: return Mathf.Abs(g.rightStick.ReadUnprocessedValue().y) > .5f;
                case 9: return g.buttonSouth.isPressed; case 10: return g.buttonEast.isPressed;
                case 11: return g.buttonWest.isPressed; case 12: return g.buttonNorth.isPressed;
                case 13: return g.leftShoulder.isPressed; case 14: return g.rightShoulder.isPressed;
                case 15: return g.startButton.isPressed; case 16: return g.selectButton.isPressed;
                case 17: return g.leftStickButton.isPressed; case 18: return g.rightStickButton.isPressed;
                case 19: return g.leftTrigger.ReadUnprocessedValue() > .5f; case 20: return g.rightTrigger.ReadUnprocessedValue() > .5f;
                case 21: return g.dpad.up.isPressed; case 22: return g.dpad.right.isPressed;
                case 23: return g.dpad.down.isPressed; case 24: return g.dpad.left.isPressed;
                default: return false;
            }
        }
        void CameraCycleTick(bool driving)
        {
            bool held = false; var kb = Keyboard.current;
            if (kb != null && !OriginalOnboardAdjust.KeyboardCaptured)
                lock (cameraKeys) foreach (Key key in cameraKeys) held |= kb[key].isPressed;
            held |= CameraPadHeld(runtime.PadRouting.Pad(0), cameraPadInput);
            if (driving && held && !cameraWasHeld) Reflex.Port.Options.ReflexClientLive.CycleCamera();
            cameraWasHeld = held;
        }
        // Consume only the mapped local camera action. The native game must not
        // also advance its four-view camera cycle on the same button press.
        public static void FilterCameraPad(ref ReflexPad pad)
        {
            int id = cameraPadInput;
            int[] masks = CameraButtonMasks;
            if (id >= 9 && id <= 18) pad.buttons = (ushort)(pad.buttons & ~masks[id - 9]);
            else if (id == 19) pad.leftTrigger = 0; else if (id == 20) pad.rightTrigger = 0;
            else if (id >= 21 && id <= 24) pad.buttons = (ushort)(pad.buttons & ~CameraDpadMasks[id - 21]);
            else if (id == 1) pad.lx = 0; else if (id == 2) pad.ly = 0;
            else if (id == 3) pad.rx = 0; else if (id == 4) pad.ry = 0;
        }
        public static void ConfigureCameraBindings(int keyboardScan, int padInput, bool rightStickRides, int lookBackInput = 0)
        {
            lock (cameraKeys)
            {
                cameraKeys.Clear();
                foreach (var entry in DirectInputMap.Table) if (entry.Value == keyboardScan) cameraKeys.Add(entry.Key);
            }
            cameraPadInput = padInput;
            RightStickRides = rightStickRides; lookBackPadInput = lookBackInput;
        }
        static void CacheCameraBindings(string json, byte[] scan)
        {
            int[] Row(string name)
            {
                var match = System.Text.RegularExpressions.Regex.Match(json, "\"" + name + "\":\\[([^\\]]*)\\]");
                if (!match.Success) return Array.Empty<int>();
                return Array.ConvertAll(match.Groups[1].Value.Split(','), x => int.TryParse(x, out int n) ? n : 0);
            }
            int[] keyboard = Row("kb"), pad = Row("js");
            lock (cameraKeys)
            {
                cameraKeys.Clear();
                if (keyboard.Length > 10) foreach (var entry in DirectInputMap.Table)
                    if (scan[entry.Value] != 0 && scan[entry.Value] == keyboard[10]) cameraKeys.Add(entry.Key);
            }
            cameraPadInput = pad.Length > 10 ? pad[10] : 0;
            RightStickRides = false;
            for (int i = 0; i < pad.Length; i++) if (i != 10 && (pad[i] == 3 || pad[i] == 4)) RightStickRides = true;
            int lookBack = RightStickRides ? 17 : 18;
            lookBackPadInput = Array.IndexOf(pad, lookBack) < 0 ? lookBack : 0;
        }


        /// App.SetUISetting(name, value) in the running world between its frames, as the game's options pages (the game applies it:
        /// DrivingCamera switches the camera preset at once during the race; on the gate it applies from the next race, as in the game).
        public void SetSetting(string name, int value) { var sim = runtime && runtime.Running ? runtime.Simulation : null; if (sim != null) SetSetting(sim, name, value, 1); }

        void SetSetting(OriginalSimulation sim, string name, int value, int mode)
        {
            if (name == "DrivingCamera") drivingCamera = value;
            sim.BetweenFrames(w =>
            {
                if (!available) return;
                try { if (ReflexInputNative.reflex_world_ui_setting(w, name, value, -1, mode, out _) != 0) Debug.LogWarning("UI setting " + name + ": " + ReflexWorldNative.Error(w, "failed")); }
                catch (EntryPointNotFoundException) { available = false; }
            });
        }

        // The game's own DrivingCamera (the Camera key changes it); the player's choice follows it, as the game keeps it in its settings.
        void ReadSettings(IntPtr w)
        {
            if (!available) return;
            world = w;
            try
            {
                if (ReflexInputNative.reflex_world_ui_setting(w, "DrivingCamera", 0, -1, 0, out int v) == 0) drivingCamera = v;
            }
            catch (EntryPointNotFoundException) { available = false; }
        }

        void LateUpdate()
        {
            int v = drivingCamera;
            if (v >= 0 && v <= 3 && PlayerPrefs.GetInt("ReflexUI.DrivingCamera", 0) != v)
            { PlayerPrefs.SetInt("ReflexUI.DrivingCamera", v); PlayerPrefs.Save(); }
        }

        // ------------------------------------------------------------------------------------------------ mouse
        void ReadMouse(OriginalSimulation sim, bool feed, Reflex.Port.RxCore.OpenReflexSimulation open = null)
        {
            var m = Mouse.current;
            object target = open != null ? open : (object)sim;
            bool l = m != null && m.leftButton.isPressed, r = m != null && m.rightButton.isPressed, c = m != null && m.middleButton.isPressed;
            if (!ReferenceEquals(target, mouseTarget)) { mouseTarget = target; buttons.Reset(l, r, c); }   // a new world: it holds no button
            if (m == null || !feed)
            {
                carryX = carryY = carryWheel = 0;
                // the feed stops (a menu, the pause, a viewer, the free look) or the mouse is gone: the game lets go of every button it holds now,
                // as it would get the UP from the original's window procedure; nothing new goes down
                int up = buttons.Tick(l, r, c, false);
                if (up != 0) Send(sim, open, 0, 0, 0, up);
                return;
            }
            // Unity's pointer delta (+y up) as Raw Input relative counts (+y down); the fraction carries to the next frame.
            Vector2 d = m.delta.ReadValue();
            carryX += d.x; carryY -= d.y;
            int dx = (int)carryX, dy = (int)carryY; carryX -= dx; carryY -= dy;
            // Wheel: WHEEL_DELTA (120) per notch; the Input System reports a notch as 1 (its uniform scroll delta), macOS fractional.
            carryWheel += m.scroll.ReadValue().y * 120f;
            int wheel = (int)carryWheel; carryWheel -= wheel;
            int flags = buttons.Tick(l, r, c, true);   // RI_MOUSE_{LEFT,RIGHT,MIDDLE}_BUTTON_DOWN / UP of this frame's changes
            if (dx == 0 && dy == 0 && wheel == 0 && flags == 0) return;
            Send(sim, open, dx, dy, wheel, flags);
        }

        void Send(OriginalSimulation sim, Reflex.Port.RxCore.OpenReflexSimulation open, int dx, int dy, int wheel, int flags)
        {
            if (open != null) { open.AddMouse(dx, dy, wheel, (uint)flags); return; }
            if (sim == null) return;
            Interlocked.Add(ref pendingDx, dx); Interlocked.Add(ref pendingDy, dy); Interlocked.Add(ref pendingWheel, wheel);
            int old, upd; do { old = pendingFlags; upd = old | flags; } while (Interlocked.CompareExchange(ref pendingFlags, upd, old) != old);
            if (Interlocked.Exchange(ref queued, 1) == 0) sim.BetweenFrames(deliver);
        }

        /// Testing: mouse counts (Raw Input lLastX / lLastY, +y down) as if moved this frame.
        public void InjectMouse(int dx, int dy, int wheel = 0, int buttonFlags = 0)
        {
            var sim = runtime && runtime.Running ? runtime.Simulation : null; if (sim == null) return;
            Interlocked.Add(ref pendingDx, dx); Interlocked.Add(ref pendingDy, dy); Interlocked.Add(ref pendingWheel, wheel);
            int old, upd; do { old = pendingFlags; upd = old | buttonFlags; } while (Interlocked.CompareExchange(ref pendingFlags, upd, old) != old);
            if (Interlocked.Exchange(ref queued, 1) == 0) sim.BetweenFrames(deliver);
        }

        // On the simulation thread, before its next frame: one WM_INPUT with everything since the last one (the message pump runs
        // before Engine::Frame; the game sums the messages of a frame).
        void Deliver(IntPtr w)
        {
            Interlocked.Exchange(ref queued, 0);
            int dx = Interlocked.Exchange(ref pendingDx, 0), dy = Interlocked.Exchange(ref pendingDy, 0), wheel = Interlocked.Exchange(ref pendingWheel, 0), flags = Interlocked.Exchange(ref pendingFlags, 0);
            if (!available || (dx == 0 && dy == 0 && wheel == 0 && flags == 0)) return;
            world = w;
            try { if (ReflexInputNative.reflex_world_mouse(w, dx, dy, wheel, (uint)flags) == 0) MouseMessages++; }
            catch (EntryPointNotFoundException) { available = false; }
        }

        // ------------------------------------------------------------------------------------------------ rumble
        void Rumble(bool driving)
        {
            try { ReflexInputNative.reflex_world_rumble(world, motors, calls); }
            catch (EntryPointNotFoundException) { available = false; return; }
            ApplyRumble(driving);
        }

        void ApplyRumble(bool driving)
        {
            for (int i = 0; i < 4; i++)
            {
                float lo = driving ? motors[2 * i] / 65535f : 0f, hi = driving ? motors[2 * i + 1] / 65535f : 0f;   // paused: the game's pause stops it too
                var device = runtime.PadRouting.Pad(i);
                if (device == appliedDevices[i] && lo == applied[2 * i] && hi == applied[2 * i + 1]) continue;
                appliedDevices[i] = device;
                runtime.PadRouting.SetRumble(i, lo, hi);
                applied[2 * i] = lo; applied[2 * i + 1] = hi;
                // Routed device already received the native slot's motor speeds.   // XInput: left = low-frequency (large) motor, right = high-frequency
            }
        }

        void StopMotors()
        {
            if (runtime) runtime.PadRouting.StopRumble();
            Array.Clear(applied, 0, applied.Length);
            Array.Clear(appliedDevices, 0, appliedDevices.Length);
        }

        // ------------------------------------------------------------------------------------------------ cursor
        // Through the shared policy (ReflexCursor): it locks while driving, and on a pause / menu / the world closing it frees the cursor
        // only for a mouse player. Setting None + visible here moved a controller player's hidden cursor to the middle of the window.
        void CaptureCursor(bool driving)
        {
            bool want = captureCursor && driving;   // (focus: the policy frees it while the window is in the background)
            if (want == cursorCaptured) return;
            cursorCaptured = want;
            Reflex.Port.Platform.ReflexCursor.Capture(this, want);   // a test player / the editor: the policy never touches the mouse
        }
        void ReleaseCursor() { if (cursorCaptured) { cursorCaptured = false; Reflex.Port.Platform.ReflexCursor.Capture(this, false); } }
    }

    public static class ReflexInputNative
    {
        const string Library = ReflexWorldNative.Library;
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_mouse__fn(IntPtr world, int dx, int dy, int wheel, uint buttonFlags);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mouse__fn> reflex_world_mouse__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mouse__fn>("reflex_world_mouse");
        public static int reflex_world_mouse(IntPtr world, int dx, int dy, int wheel, uint buttonFlags) => reflex_world_mouse__p.Get()(world, dx, dy, wheel, buttonFlags);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_rumble__fn(IntPtr world, [Out] ushort[] motors, [Out] uint[] calls);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rumble__fn> reflex_world_rumble__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_rumble__fn>("reflex_world_rumble");
        public static int reflex_world_rumble(IntPtr world, [Out] ushort[] motors, [Out] uint[] calls) => reflex_world_rumble__p.Get()(world, motors, calls);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_remap__fn(IntPtr world, [MarshalAs(UnmanagedType.LPStr)] string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_remap__fn> reflex_world_remap__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_remap__fn>("reflex_world_remap");
        public static int reflex_world_remap(IntPtr world, [MarshalAs(UnmanagedType.LPStr)] string command, byte[] output, int size) => reflex_world_remap__p.Get()(world, command, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_setting__fn(IntPtr world, [MarshalAs(UnmanagedType.LPStr)] string name, int value, int device, int set, out int before);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_setting__fn> reflex_world_ui_setting__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_setting__fn>("reflex_world_ui_setting");
        public static int reflex_world_ui_setting(IntPtr world, [MarshalAs(UnmanagedType.LPStr)] string name, int value, int device, int set, out int before) => reflex_world_ui_setting__p.Get()(world, name, value, device, set, out before);
    }
}

namespace Reflex.Port.OriginalWorld
{
    // What the PC game draws for a UI image with a KeyMap (the button legends' accept / back / ... icons, Title's "press start", the
    // popups' button images): UIImageComponent::vf1 005cb3f0 looks the KeyMap up (00813690 over the registry 00d92f50), takes its
    // bound input of slot 0 (KeyMap::vf8(7, 0) = [keymap + 0x18]) and, when it is one of the pad's ten buttons of 009d3d20, sets the
    // image to "xenon_Controller_Button_N", else to UIEmpty (Base material, texture 1x1Transparent). It does this once, when the layer
    // is read; nothing changes it per device. On the PC every MapKey_UI_* the layers name is a CompoundKeyMap (vtable 00974c10) of
    // KeyBind_Core.xml's keyboard and pad maps (MapKey_UI_Accept = Accept1 Enter + Accept2 a; Back = Esc + b; Start = F3 + start;
    // Misc = F2 + y; Toggle = F1 + x; Select = Tab + back: the profile session's "remap glyph"), whose own slot 0 is empty (input 0),
    // so every KeyMap image is UIEmpty: the PC game shows no key or button glyph, only the legend's text (Engine::Startup reads the key
    // bindings before any UI layer is read). The port draws the same nothing; PlayerPrefs ReflexKeyHints = 1 brings back the port's
    // own hints (the KeyBind_Core.xml key name with the keyboard, the Xbox 360 glyph with a pad), labelled a port extra.
    public static class ReflexKeyMapGlyphs
    {
        /// The port's key-name / pad-glyph hints in place of the original's empty KeyMap images (port extra; off = as the original).
        public static bool PortHints => PlayerPrefs.GetInt("ReflexKeyHints", 0) != 0;

        /// The KeyMap images of a layer built from the install (OriginalLiveUi): drawn as the original's UIEmpty (nothing).
        public static int BlankKeyMapImages(OriginalUiTree tree, Dictionary<string, object> layerJson)
        {
            if (tree == null || layerJson == null || PortHints) return 0;
            int n = 0;
            void Walk(Dictionary<string, object> node, string path)
            {
                if (node.TryGetValue("props", out var po) && po is Dictionary<string, object> props && props.TryGetValue("KeyMap", out var km) && km is string s && s.Length > 0
                    && path != null && tree.byPath.TryGetValue(path, out var t) && t.graphic) { t.graphic.enabled = false; n++; }
                if (node.TryGetValue("children", out var co) && co is List<object> kids)
                    foreach (var c in kids)
                        if (c is Dictionary<string, object> cd)
                        {
                            string name = cd.TryGetValue("props", out var cp) && cp is Dictionary<string, object> cpd && cpd.TryGetValue("Name", out var nm) ? nm as string : null;
                            if (name == null && cd.TryGetValue("name", out var nn)) name = nn as string;
                            Walk(cd, path == null ? name : path + "/" + name);
                        }
            }
            if (layerJson.TryGetValue("root", out var r) && r is Dictionary<string, object> root) Walk(root, null);
            return n;
        }
    }
}
