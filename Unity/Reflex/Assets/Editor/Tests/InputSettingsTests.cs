using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Editor
{
    // Input and settings (OriginalWorld/OriginalInput.cs, Content/ReflexLaunchSettings.cs, native input.cpp; _re/notes/input-settings.md).
    //   Verify: edit mode, on the National Concord world of the install (built into the bench's cache when missing), frames run on this
    //           thread: graphics.cfg / language.txt as the boot read them (the world's XenonCore +0x3c display settings); the mouse
    //           through WM_INPUT drives the game's RiderReflexMouseX / Y (the mouse axes = the game's formula, the rider-lean inputs
    //           S+0x34 / 0x38 move); the rumble the game writes (XInputSetState) with Vibration on; DrivingCamera 0..3 selects the
    //           game's presets (camera entity P+0x974[0 / 1 / 4 / 2], FirstPerson FOV 60 / near 0.5 as Cameras/MX_Camera.bxml);
    //           the port's tool keys are off every key the game binds (KeyBind_Core.xml keyboard, the ControllerRemapping defaults
    //           of 005b80b0); ControllerPresets.xml's inputs are all XInput fields OriginalWorldRuntime passes.
    //   Race:   play mode, the Reflex scene on National/NAT_Concord (no AI) with a virtual mouse and gamepad: OriginalInput is added, the
    //           player's ReflexUI.DrivingCamera 3 / Vibration 1 / MouseSensitivity 100 reach the game, mouse motion (Input System
    //           events) leans the rider, the game's rumble reaches the pad (SetMotorSpeeds), the pause page's Driving Camera row changes
    //           the preset. PlayerPrefs: the keys it sets are restored.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.InputSettingsTests.Verify -quit / .Race (no -quit)
    public static class InputSettingsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[INPUT] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Out => Environment.GetEnvironmentVariable("INPUT_REPORT") ?? Path.Combine(Application.temporaryCachePath, "input-settings.txt");

        static uint U(IntPtr w, uint a) { var b = new byte[4]; ReflexWorldNative.reflex_world_read(w, a, b, 4); return BitConverter.ToUInt32(b, 0); }
        static float F(IntPtr w, uint a) { var b = new byte[4]; ReflexWorldNative.reflex_world_read(w, a, b, 4); return BitConverter.ToSingle(b, 0); }

        static ReflexWorldFiles Concord()
        {
            var files = ReflexWorldFiles.For("National/NAT_Concord", 0);
            if (!files.Ready)
            {
                Log("building " + files.name + " from the install ...");
                var t = files.Build(); var start = DateTime.UtcNow;
                while (!t.IsCompleted && (DateTime.UtcNow - start).TotalSeconds < 600) System.Threading.Thread.Sleep(200);
            }
            return files;
        }

        // The port's tool keys (OriginalWorldRuntime F4..F12 / M / Left Alt, ReflexEventSelect Insert / Delete, OriginalOnboardCamera F8..F12).
        static readonly Key[] PortKeys = { Key.F4, Key.F5, Key.F6, Key.F7, Key.F8, Key.F9, Key.F10, Key.F11, Key.F12, Key.Insert, Key.Delete, Key.M, Key.LeftAlt };
        // The ControllerRemapping keyboard defaults (005b80b0 game input ids through 00816ba0's scan-code table).
        static readonly (string action, int dik)[] RemapDefaults = { ("Throttle", 0x11), ("Brake", 0x1f), ("SteerLeft", 0x1e), ("SteerRight", 0x20), ("TrickModifier", 0x39),
            ("Clutch", 0x2a), ("ReflexLeft", 0xcb), ("ReflexRight", 0xcd), ("ReflexUp", 0xc8), ("ReflexDown", 0xd0), ("Camera", 0x2e), ("SetResetPoint", 0x21),
            ("Reset", 0x13), ("UseResetPoint", 0x2f), ("Reverse", 0x2d), ("CameraTweakLeft", 0x24), ("CameraTweakRight", 0x26), ("CameraTweakUp", 0x17), ("CameraTweakDown", 0x25) };
        static readonly Dictionary<string, int> KeyBindNames = new Dictionary<string, int> { { "up", 0xc8 }, { "down", 0xd0 }, { "left", 0xcb }, { "right", 0xcd }, { "enter", 0x1c }, { "esc", 0x01 },
            { "tab", 0x0f }, { "f1", 0x3b }, { "f2", 0x3c }, { "f3", 0x3d }, { "delete", 0xd3 }, { "end", 0xcf } };

        public static void Verify()
        {
            report.Clear(); bad = 0;
            try
            {
                // ---- graphics.cfg / language.txt
                var ls = ReflexLaunchSettings.Load();
                Log("launch settings: " + ls);
                string cfg = File.ReadAllText(ReflexInstall.PathOf("graphics.cfg"));
                int Cfg(string k) { var m = Regex.Match(cfg, "^" + k + @"=(-?\d+)", RegexOptions.Multiline); return m.Success ? int.Parse(m.Groups[1].Value) : int.MinValue; }
                Check(ls.loaded && (ls.fullscreen ? 1 : 0) == Cfg("Fullscreen") && ls.width == Cfg("ScreenWidth") && ls.height == Cfg("ScreenHeight") && ls.refresh == Cfg("ScreenRefresh") &&
                      ls.antialiasing == Cfg("Antialiasing") && (ls.motionBlur ? 1 : 0) == Cfg("MotionBlur") && (ls.vsync ? 1 : 0) == Cfg("VSync") && ls.textureQuality == Cfg("TextureQuality") && ls.shadowQuality == Cfg("ShadowQuality"),
                      "graphics.cfg parsed as the file says");
                var files = Concord();
                Check(files.Ready, "the Concord world is built (" + files.World + ")");
                if (!files.Ready) { End(); return; }
                IntPtr w = ReflexWorldNative.reflex_world_create(ReflexInstall.Exe, Path.Combine(ReflexCrt.Dir, "msvcr90.dll"), files.World, files.Deform);
                Check(w != IntPtr.Zero, "world created");
                if (w == IntPtr.Zero) { End(); return; }
                try { RunWorld(w, ls); }
                finally { ReflexWorldNative.reflex_world_destroy(w); }

                // ---- tool keys vs the game's bindings
                var bound = new Dictionary<int, string>();
                foreach (var (a, d) in RemapDefaults) bound[d] = "ControllerRemapping " + a;
                string core = File.ReadAllText(ReflexInstall.PathOf("KeyBind_Core.xml"));
                var kb = Regex.Match(core, "<Actions>.*?<keyboard>(.*?)</keyboard>", RegexOptions.Singleline).Groups[1].Value;
                foreach (Match m in Regex.Matches(kb, @"<bind>\s*(\S+)\s+(\S+)\s*</bind>"))
                {
                    string k = m.Groups[1].Value, act = m.Groups[2].Value;
                    if (!act.StartsWith("MapKey_")) continue;   // the debug overlay binds (ctrl-d, ...) are dev only
                    if (KeyBindNames.TryGetValue(k, out int d)) bound[d] = "KeyBind_Core " + act;
                }
                Log("game key bindings: " + string.Join(", ", bound.OrderBy(kv => kv.Key).Select(kv => $"{kv.Key:x2} {kv.Value}")));
                var clash = PortKeys.Select(k => DirectInputMap.Table.FirstOrDefault(t => t.Key == k)).Where(t => bound.ContainsKey(t.Value)).Select(t => t.Key + " = " + bound[t.Value]).ToList();
                Check(clash.Count == 0, "the port's tool keys (" + string.Join(" ", PortKeys) + ") are off the game's bindings" + (clash.Count > 0 ? ": " + string.Join(", ", clash) : ""));
                // ---- ControllerPresets.xml inputs vs the XInput fields passed
                string presets = File.ReadAllText(ReflexInstall.PathOf("ControllerPresets.xml"));
                var inputs = Regex.Matches(presets, "button=\"(VIJoystick_[A-Za-z0-9]+)\"").Cast<Match>().Select(m => m.Groups[1].Value).Distinct().OrderBy(x => x).ToList();
                var xinput = new HashSet<string> { "VIJoystick_LeftXAxis", "VIJoystick_LeftYAxis", "VIJoystick_RightXAxis", "VIJoystick_RightYAxis" };
                for (int i = 1; i <= 16; i++) xinput.Add("VIJoystick_Button" + i);   // A B X Y LB RB Start Back L3 R3 LT RT Up Right Down Left
                var sixaxis = inputs.Where(x => x.StartsWith("VIJoystick_Sensor")).ToList();
                var missing = inputs.Where(x => !xinput.Contains(x) && !x.StartsWith("VIJoystick_Sensor")).ToList();
                Check(missing.Count == 0, $"ControllerPresets.xml: {inputs.Count} inputs, every XInput one passed (sticks, 14 buttons, 2 triggers); PS3 SIXAXIS only: {string.Join(" ", sixaxis)}" + (missing.Count > 0 ? "; missing " + string.Join(" ", missing) : ""));
            }
            catch (Exception e) { Check(false, "exception " + e); }
            End();
        }

        static void RunWorld(IntPtr w, ReflexLaunchSettings ls)
        {
            ReflexWorldNative.reflex_world_set_ambient_animation(w, 0);
            uint core = U(w, 0xa95960), st = core + 0x3c;
            uint b0 = U(w, st);
            Log($"world display settings (XenonCore+0x3c): fullscreen {b0 & 0xff} motion blur {(b0 >> 8) & 0xff} vp {U(w, st + 8)}x{U(w, st + 0xc)} back buffer {U(w, st + 0x14)}x{U(w, st + 0x18)} adapter {U(w, st + 0x1c)} refresh {U(w, st + 0x20)} vsync {U(w, st + 0x24) & 0xff} texture {U(w, st + 0x28)} shadow {U(w, st + 0x2c)} AA {U(w, st + 0x30)}; language [00a94078] {U(w, 0xa94078)}");
            var bb = ls.BackBuffer;
            Check((b0 & 0xff) == (ls.fullscreen ? 1u : 0u) && ((b0 >> 8) & 0xff) == (ls.motionBlur ? 1u : 0u) && (U(w, st + 0x24) & 0xff) == (ls.vsync ? 1u : 0u) && U(w, st + 0x28) == ls.textureQuality &&
                  U(w, st + 0x2c) == ls.shadowQuality && U(w, st + 0x30) == ls.antialiasing && U(w, st + 0x14) == bb.x && U(w, st + 0x18) == bb.y && U(w, st + 0x20) == ls.refresh && U(w, 0xa94078) == ls.languageIndex,
                  "ReflexLaunchSettings = what the booted game read (display struct, back buffer, language index)");
            uint mgr = U(w, core + 0x34), M = mgr + 0x3234;
            uint G = U(w, 0xa946b8), P = G + 0x7460 + U(w, G + 0x153ec) * 0x12a0;
            int Ui(string name) { ReflexInputNative.reflex_world_ui_setting(w, name, 0, -1, 0, out int v); return v; }
            void SetUi(string name, int v) { ReflexInputNative.reflex_world_ui_setting(w, name, v, -1, 1, out _); }
            Log($"UI settings: DrivingCamera {Ui("DrivingCamera")} MouseSensitivity {Ui("MouseSensitivity")} Vibration {Ui("Vibration")}; mouse sensitivity [009cb7b0] {F(w, 0x9cb7b0)}");
            var keys = new byte[256]; var pads = new ReflexPad[4]; var bike = ReflexBikeState.Create();
            var race = new ReflexRaceState();
            SetUi("Vibration", 1);
            int start = -1; int camStep = 0; uint lastCalls = 0; int rumbleNonZero = 0; var motors = new ushort[8]; var calls = new uint[4];
            bool noMouseLean = false; float leanX = 0, leanY = 0, axisX = float.NaN, axisY = float.NaN, expectX = 0, expectY = 0;
            int[] entityOf = { 0, 1, 4, 2 }; var camOk = new bool[4]; string camLog = "";
            for (int f = 0; f < 3200; f++)
            {
                ReflexWorldNative.reflex_world_race(w, ref race);
                if (race.state >= 2 && start < 0) start = f;
                int t = start < 0 ? -1 : f - start;
                keys[0x11] = (byte)(t >= 0 ? 0x80 : 0);
                pads[0] = new ReflexPad();
                // (before the first turn's wreck of a straight full-throttle run, ~t 160): down (lean back) at 20..30, right at 80..90
                if (t >= 80 && t < 90) { ReflexInputNative.reflex_world_mouse(w, 20, 0, 0, 0); }
                if (t >= 20 && t < 30) { ReflexInputNative.reflex_world_mouse(w, 0, 20, 0, 0); }
                if (t >= 0 && t % 300 == 150 && camStep < 4 && t > 500) { SetUi("DrivingCamera", camStep); }
                if (ReflexWorldNative.reflex_world_frame(w, 1f / 60, keys, pads, 1) != 0) { Check(false, "frame " + f + ": " + ReflexWorldNative.Error(w, "?")); return; }
                ReflexWorldNative.reflex_world_bike(w, ref bike);
                float sens = F(w, 0x9cb7b0), dt = F(w, 0xd728d0 + 0x18);   // the game's own frame time (Engine +0x18), what vf43 gets
                if (t == 85) { axisX = F(w, M + 0x134); expectX = Mathf.Clamp(20f / 1280f * (0.033333335f / dt) * sens, -1, 1); }
                if (t == 25) { axisY = F(w, M + 0x138); expectY = Mathf.Clamp(20f / 720f * (0.033333335f / dt) * sens * -1f, -1, 1); }
                if (t >= 80 && t < 100) leanX = Mathf.Max(leanX, bike.inputs[6]);
                if (t >= 20 && t < 40) leanY = Mathf.Min(leanY, bike.inputs[7]);
                if (t < 20 && t >= 0) { if (Mathf.Abs(bike.inputs[6]) > 1e-6f || Mathf.Abs(bike.inputs[7]) > 1e-6f) noMouseLean = true; }
                ReflexInputNative.reflex_world_rumble(w, motors, calls);
                if (calls[0] != lastCalls) { lastCalls = calls[0]; if (motors[0] != 0 || motors[1] != 0) rumbleNonZero++; }
                if (t >= 0 && t % 300 == 290 && t > 500 && camStep < 4)
                {
                    var cam = new ReflexCamera(); ReflexWorldNative.reflex_world_camera(w, ref cam);
                    uint sel = U(w, P + 0x988), cur = U(w, P + 0x9a8);
                    camOk[camStep] = sel == entityOf[camStep] && Ui("DrivingCamera") == camStep && (camStep != 3 || (Mathf.Abs(cam.fov * Mathf.Rad2Deg - 60f) < 0.01f && Mathf.Abs(cam.nearPlane - 0.5f) < 1e-4f && U(w, cur) == 0x95a80c));
                    camLog += $" [{camStep}: entity {sel} vtable {U(w, cur):x8} fov {cam.fov * Mathf.Rad2Deg:0.0} near {cam.nearPlane:0.00}]";
                    camStep++;
                }
            }
            Log($"mouse X: axis M+0x134 {axisX:0.000000} (game formula {expectX:0.000000}), max rider lean x {leanX:0.000}; mouse Y: axis M+0x138 {axisY:0.000000} (formula {expectY:0.000000}), min rider lean y {leanY:0.000}; lean before the mouse moved: {(noMouseLean ? "yes" : "none")}");
            Check(!noMouseLean, "no rider lean before the mouse moves");
            Check(Mathf.Abs(axisX - expectX) < 1e-5f * Mathf.Abs(expectX) && leanX > 0.2f, "mouse dx 20 counts / frame -> RiderReflexMouseX = the game's formula (dx / 1280 * (1/30) / dt * MouseSensitivity * 0.05), the rider leans right (S+0x34)");
            Check(Mathf.Abs(axisY - expectY) < 1e-5f * Mathf.Abs(expectY) && leanY < -0.2f, "mouse dy 20 counts / frame (down) -> RiderReflexMouseY = the game's formula (Y negated, / 720), the rider leans back (S+0x38)");
            Log($"rumble: {calls[0]} XInputSetState calls for pad 0, {rumbleNonZero} with the motors on");
            Check(calls[0] > 2 && rumbleNonZero > 0, "the game's rumble arrives (XInputSetState with nonzero motor speeds, Vibration on)");
            Log("DrivingCamera presets:" + camLog);
            Check(camOk.All(x => x), "DrivingCamera 0..3 -> the game's camera entity 0 / 1 / 4 / 2 (Visual / Game Play / User / 1st Person; FirstPerson FOV 60, near 0.5: MX_Camera.bxml)");
        }

        static void End()
        {
            Log(bad == 0 ? "INPUT-SETTINGS PASS" : "INPUT-SETTINGS FAIL " + bad);
            try { File.WriteAllText(Out, report.ToString()); } catch (Exception) { }
        }

        // ---------------------------------------------------------------------------------------------------- play mode
        static int stage; static double stageAt, runningAt, raceAt;
        static Mouse vmouse; static Gamepad vpad;
        static readonly string[] Keys = { "ReflexUI.DrivingCamera", "ReflexUI.Vibration", "ReflexUI.MouseSensitivity", "ReflexCameraMode" };
        static Dictionary<string, (bool has, int v)> saved;
        static (InputSettings.EditorInputBehaviorInPlayMode, InputSettings.BackgroundBehavior) inputBehavior;
        static float leanSeen, leanBack; static int frameLog;
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }

        public static void Race()
        {
            report.Clear(); bad = 0;
            saved = Keys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetInt(k, 0)));
            PlayerPrefs.SetInt("ReflexUI.DrivingCamera", 3); PlayerPrefs.SetInt("ReflexUI.Vibration", 1); PlayerPrefs.SetInt("ReflexUI.MouseSensitivity", 100); PlayerPrefs.SetInt("ReflexCameraMode", 0); PlayerPrefs.Save();
            ReflexEventSelect.Pending = "National/NAT_Concord"; ReflexEventSelect.PendingAi = 0; ReflexEventSelect.PendingGarage = null;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            // batchmode has no focused Game view: the virtual devices' events go to the player
            inputBehavior = (InputSystem.settings.editorInputBehaviorInPlayMode, InputSystem.settings.backgroundBehavior);
            InputSystem.settings.editorInputBehaviorInPlayMode = InputSettings.EditorInputBehaviorInPlayMode.AllDeviceInputAlwaysGoesToGameView;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            stage = 0; stageAt = EditorApplication.timeSinceStartup; runningAt = raceAt = -1; leanSeen = 0; leanBack = 0;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static ReflexBikeState Bike(OriginalWorldRuntime rt) => (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);

        static void Step()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) EndRace(); else if (Age > 180) { Check(false, "play mode never started"); EndRace(); } return; }
                if (vmouse == null) { vmouse = InputSystem.AddDevice<Mouse>("InputTestMouse"); vpad = InputSystem.AddDevice<Gamepad>("InputTestPad"); vmouse.MakeCurrent(); Log("virtual mouse and gamepad added"); }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                double now = EditorApplication.timeSinceStartup;
                if (rt && rt.Running && runningAt < 0) runningAt = now;
                var race = rt && rt.Running ? RaceState(rt) : default;
                if (rt && raceAt < 0 && race.valid != 0 && race.state >= 2) raceAt = now;
                if (rt) rt.testHeldKeys = raceAt >= 0 ? new[] { 0x11 } : Array.Empty<int>();
                var input = rt ? rt.GetComponent<OriginalInput>() : null;
                double inRace = raceAt < 0 ? -1 : now - raceAt;
                // short moves early in the run (a long full lean wrecks the bike, and a wreck disables the rider input until the reset)
                if (inRace > 0.5 && inRace < 0.7) InputSystem.QueueDeltaStateEvent(vmouse.delta, new Vector2(0, -10));  // pointer down (Unity -y)
                if (rt && inRace > 0.5 && inRace < 1.2) leanBack = Mathf.Min(leanBack, Bike(rt).inputs[7]);
                if (inRace > 1.5 && inRace < 1.7) InputSystem.QueueDeltaStateEvent(vmouse.delta, new Vector2(10, 0));   // pointer right (+x)
                if (rt && inRace > 1.5 && inRace < 2.2) leanSeen = Mathf.Max(leanSeen, Bike(rt).inputs[6]);
                switch (stage)
                {
                    case 0:
                        if (rt && rt.Running && now - runningAt > 3)
                        {
                            Check(input != null, "OriginalInput added to the race's OriginalWorldRuntime");
                            Check(input && input.Available, "the plugin has the input exports");
                            Next();
                        }
                        else if (Age > 600) { Check(false, "the world never ran"); Next(); stage = 100; EditorApplication.ExitPlaymode(); }
                        break;
                    case 1:
                        if (inRace > 6)
                        {
                            Log($"mouse messages {input.MouseMessages}, max lean x {leanSeen:0.000} (pointer right), min lean y {leanBack:0.000} (pointer down)");
                            Check(input.MouseMessages > 10 && leanSeen > 0.3f && leanBack < -0.3f, "Input System mouse motion -> WM_INPUT -> the rider leans (right, back) through the game's RiderReflexMouse");
                            Log($"DrivingCamera in the game {input.DrivingCamera}; camera near {rt.chaseCamera.nearClipPlane:0.00} fov {rt.chaseCamera.fieldOfView:0.0}; mode {rt.cameraMode}");
                            Check(input.DrivingCamera == 3 && rt.cameraMode == 0 && Mathf.Abs(rt.chaseCamera.nearClipPlane - 0.5f) < 1e-3f && Mathf.Abs(rt.chaseCamera.fieldOfView - 60f) < 0.5f,
                                  "the player's DrivingCamera 3 (1st Person) reached the game: its FirstPerson preset (near 0.5, FOV 60) on the game camera");
                            Log($"rumble calls {input.RumbleCalls(0)}, motors applied L {input.AppliedMotor(0, false):0.00} R {input.AppliedMotor(0, true):0.00}");
                            Next();
                        }
                        break;
                    case 2:
                        // the pause page's Driving Camera row: Visual
                        if (Age > 0.5)
                        {
                            var pm = rt.pauseMenu ? rt.pauseMenu : UnityEngine.Object.FindAnyObjectByType<OriginalPauseMenu>();
                            var m = typeof(OriginalPauseMenu).GetMethod("SetDrivingCamera", BindingFlags.NonPublic | BindingFlags.Instance);
                            Check(pm && m != null, "pause page with its Driving Camera row");
                            if (pm && m != null) m.Invoke(pm, new object[] { 0 });
                            Next();
                        }
                        break;
                    case 3:
                        if (Age > 2)
                        {
                            Log($"after the pause row Visual: game DrivingCamera {input.DrivingCamera}, camera near {rt.chaseCamera.nearClipPlane:0.00} fov {rt.chaseCamera.fieldOfView:0.0}; rumble calls {input.RumbleCalls(0)}");
                            Check(input.DrivingCamera == 0 && rt.chaseCamera.nearClipPlane > 1f, "the pause page's Driving Camera Visual -> the game's Visual preset (third person, near 2)");
                            Check(input.RumbleCalls(0) > 1, "XInputSetState calls arrive for the virtual pad (Vibration 1 from the player's setting)");
                            stage = 100; EditorApplication.ExitPlaymode();
                        }
                        break;
                }
                if (input && input.AppliedMotor(0, false) + input.AppliedMotor(0, true) > 0 && frameLog++ < 3) Log($"SetMotorSpeeds pad 0: {input.AppliedMotor(0, false):0.00} / {input.AppliedMotor(0, true):0.00}");
            }
            catch (Exception e) { Check(false, "exception " + e); stage = 100; EditorApplication.ExitPlaymode(); }
        }

        static void EndRace()
        {
            EditorApplication.update -= Step;
            foreach (var kv in saved) { if (kv.Value.has) PlayerPrefs.SetInt(kv.Key, kv.Value.v); else PlayerPrefs.DeleteKey(kv.Key); }
            PlayerPrefs.Save();
            InputSystem.settings.editorInputBehaviorInPlayMode = inputBehavior.Item1; InputSystem.settings.backgroundBehavior = inputBehavior.Item2;
            if (vmouse != null) { InputSystem.RemoveDevice(vmouse); vmouse = null; } if (vpad != null) { InputSystem.RemoveDevice(vpad); vpad = null; }
            End();
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
