using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.InputSystem.Utilities;

namespace Reflex.Port.Editor
{
    // The input value pipeline's managed stage (agent w-input, 2026-10-28): a physical stick / trigger position through the REAL Unity layout
    // of each device family to the XINPUT_GAMEPAD the core takes (OriginalPadConvert, the code OriginalWorldRuntime.ReadPads runs), and what the
    // core's own conversion (rx_pad_axis 008156e0: dead zone 7864, then (v - 7864) / 24903) makes of it, against the ideal for that physical
    // position. Also shows what the Input System's processed ReadValue() would have done (the default stick deadzone 0.125..0.925).
    //   Unity -batchmode -nographics -projectPath <clone> -executeMethod Reflex.Port.Editor.InputPipelineTests.Run -logFile <log> -quit
    //   env REFLEX_INPUT_REPORT = the TSV to write.
    // Devices: Gamepad (generic float layout, what the 27-case conversion test used), a replica of XInputControllerWindows (the Windows pads;
    // the class itself is Windows-only, so its state struct and attributes are copied), DualShock 4 / DualSense (HID bytes, Y inverted),
    // XboxGamepadMacOS (the macOS Xbox layout), Switch Pro (HID).
    public static class InputPipelineTests
    {
        [StructLayout(LayoutKind.Explicit, Size = 12)]
        struct ReplicaXInputState : IInputStateTypeInfo
        {
            public FourCC format => new FourCC('R', 'X', 'I', 'N');
            [InputControl(name = "dpad", layout = "Dpad", sizeInBits = 4, bit = 0)]
            [InputControl(name = "dpad/up", bit = 0)] [InputControl(name = "dpad/down", bit = 1)] [InputControl(name = "dpad/left", bit = 2)] [InputControl(name = "dpad/right", bit = 3)]
            [InputControl(name = "start", bit = 4)] [InputControl(name = "select", bit = 5)] [InputControl(name = "leftStickPress", bit = 6)] [InputControl(name = "rightStickPress", bit = 7)]
            [InputControl(name = "leftShoulder", bit = 8)] [InputControl(name = "rightShoulder", bit = 9)]
            [InputControl(name = "buttonSouth", bit = 12)] [InputControl(name = "buttonEast", bit = 13)] [InputControl(name = "buttonWest", bit = 14)] [InputControl(name = "buttonNorth", bit = 15)]
            [FieldOffset(0)] public ushort buttons;
            [InputControl(name = "leftTrigger", format = "BYTE")] [FieldOffset(2)] public byte leftTrigger;
            [InputControl(name = "rightTrigger", format = "BYTE")] [FieldOffset(3)] public byte rightTrigger;
            [InputControl(name = "leftStick", layout = "Stick", format = "VC2S")]
            [InputControl(name = "leftStick/x", offset = 0, format = "SHRT", parameters = "clamp=false,invert=false,normalize=false")]
            [InputControl(name = "leftStick/left", offset = 0, format = "SHRT")] [InputControl(name = "leftStick/right", offset = 0, format = "SHRT")]
            [InputControl(name = "leftStick/y", offset = 2, format = "SHRT", parameters = "clamp=false,invert=false,normalize=false")]
            [InputControl(name = "leftStick/up", offset = 2, format = "SHRT")] [InputControl(name = "leftStick/down", offset = 2, format = "SHRT")]
            [FieldOffset(4)] public short leftStickX;
            [FieldOffset(6)] public short leftStickY;
            [InputControl(name = "rightStick", layout = "Stick", format = "VC2S")]
            [InputControl(name = "rightStick/x", offset = 0, format = "SHRT", parameters = "clamp=false,invert=false,normalize=false")]
            [InputControl(name = "rightStick/left", offset = 0, format = "SHRT")] [InputControl(name = "rightStick/right", offset = 0, format = "SHRT")]
            [InputControl(name = "rightStick/y", offset = 2, format = "SHRT", parameters = "clamp=false,invert=false,normalize=false")]
            [InputControl(name = "rightStick/up", offset = 2, format = "SHRT")] [InputControl(name = "rightStick/down", offset = 2, format = "SHRT")]
            [FieldOffset(8)] public short rightStickX;
            [FieldOffset(10)] public short rightStickY;
        }
        [InputControlLayout(stateType = typeof(ReplicaXInputState), displayName = "Replica XInput (Windows)")]
        class ReplicaXInputPad : Gamepad { }

        static StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[InputPipeline] " + s); }

        // the core's conversion of an XInput stick short (rx_pad_axis 008156e0)
        static float CoreAxis(int v)
        {
            const float k = 4.0155803e-05f;
            if (v > 7864) return (v - 7864) * k;
            if (v < -7864) return ((v + 7864) + 1.0f) * k;
            return 0f;
        }
        static int Ideal(float p) => Mathf.Clamp(Mathf.RoundToInt(p * 32767f), -32768, 32767);

        sealed class Rig
        {
            public string name; public Gamepad pad; public Action<float, float, float, float> setSticks; public Action<int, int> setTriggers; public string note;
        }

        public static void Run()
        {
            report.Clear(); bad = 0;
            string outPath = Environment.GetEnvironmentVariable("REFLEX_INPUT_REPORT") ?? "/tmp/reflex_input_pipeline.tsv";
            try
            {
                var prevBg = InputSystem.settings.backgroundBehavior;
                InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
                InputSystem.RegisterLayout<ReplicaXInputPad>();
                var rigs = new List<Rig>();

                // 1. generic Gamepad (float layout)
                {
                    var g = InputSystem.AddDevice<Gamepad>("PipeGeneric");
                    rigs.Add(new Rig { name = "Gamepad (generic float)", pad = g, note = "GamepadState floats", setSticks = (lx, ly, rx, ry) => { var s = new GamepadState { leftStick = new Vector2(lx, ly), rightStick = new Vector2(rx, ry) }; InputState.Change(g, s); },
                        setTriggers = (lt, rt) => { InputState.Change(g.leftTrigger, lt / 255f); InputState.Change(g.rightTrigger, rt / 255f); } });
                }
                // 2. replica of XInputControllerWindows
                {
                    var g = InputSystem.AddDevice<ReplicaXInputPad>("PipeXInput");
                    var st = new ReplicaXInputState();
                    rigs.Add(new Rig { name = "XInput Windows (replica of XInputControllerWindows)", pad = g, note = "SHRT sticks, BYTE triggers; physical position -> raw short = round(p*32767)",
                        setSticks = (lx, ly, rx, ry) => { st.leftStickX = (short)Ideal(lx); st.leftStickY = (short)Ideal(ly); st.rightStickX = (short)Ideal(rx); st.rightStickY = (short)Ideal(ry); InputState.Change(g, st); },
                        setTriggers = (lt, rt) => { st.leftTrigger = (byte)lt; st.rightTrigger = (byte)rt; InputState.Change(g, st); } });
                }
                // 3..5. HID / macOS layouts: their state structs are internal, so the state is built by reflection
                foreach (var spec in new[] {
                    ("DualShock4GamepadHID", "UnityEngine.InputSystem.DualShock.LowLevel.DualShock4HIDInputReport", "byte", "BYTE sticks 0..255 (rest 128), Y inverted by the layout; physical byte = round(128 + p*127.5) (up = low byte)"),
                    ("DualSenseGamepadHID", "UnityEngine.InputSystem.DualShock.LowLevel.DualSenseHIDInputReport", "byte", "BYTE sticks 0..255 (rest 128), Y inverted by the layout; physical byte = round(128 + p*127.5) (up = low byte)"),
                    ("XboxGamepadMacOSNative", "UnityEngine.InputSystem.XInput.LowLevel.XInputControllerNativeOSXState", "short", "SHRT sticks, BYTE triggers (macOS Xbox, native); physical position -> raw short = round(p*32767)") })
                {
                    try
                    {
                        var g = InputSystem.AddDevice(spec.Item1) as Gamepad;
                        var asm = typeof(Gamepad).Assembly;
                        var t = asm.GetType(spec.Item2);
                        if (g == null || t == null) { Log("# " + spec.Item1 + ": layout or state type not available (" + (t == null) + ")"); continue; }
                        var change = null as System.Reflection.MethodInfo;
                        foreach (var m in typeof(InputState).GetMethods())
                            if (m.Name == "Change" && m.IsGenericMethodDefinition && m.GetParameters().Length >= 2 && m.GetParameters()[0].ParameterType == typeof(InputControl) && !m.GetParameters()[1].ParameterType.IsByRef) { change = m.MakeGenericMethod(t); break; }
                        object st = Activator.CreateInstance(t);
                        void F(string f, object v) { var fi = t.GetField(f); fi.SetValue(st, Convert.ChangeType(v, fi.FieldType)); }
                        void Push() { var ps = change.GetParameters(); var args = new object[ps.Length]; args[0] = g; args[1] = st; for (int i = 2; i < ps.Length; i++) args[i] = ps[i].HasDefaultValue ? ps[i].DefaultValue : null; change.Invoke(null, args); }
                        bool bytes = spec.Item3 == "byte";
                        int B(float p) => Mathf.Clamp(Mathf.RoundToInt(128f + p * 127.5f), 0, 255);
                        if (bytes) { F("leftStickX", 128); F("leftStickY", 128); F("rightStickX", 128); F("rightStickY", 128); }
                        rigs.Add(new Rig { name = spec.Item1, pad = g, note = spec.Item4,
                            setSticks = (lx, ly, rx, ry) =>
                            {
                                if (bytes) { F("leftStickX", B(lx)); F("leftStickY", B(-ly)); F("rightStickX", B(rx)); F("rightStickY", B(-ry)); }   // the byte is "down = high"
                                else { F("leftStickX", Ideal(lx)); F("leftStickY", Ideal(ly)); F("rightStickX", Ideal(rx)); F("rightStickY", Ideal(ry)); }
                                Push();
                            },
                            setTriggers = (lt, rt) => { F("leftTrigger", lt); F("rightTrigger", rt); Push(); } });
                    }
                    catch (Exception e) { Log("# " + spec.Item1 + ": " + e.Message); }
                }

                Log("device\tgroup\tphysical\traw_short_to_core\tunity_processed_ReadValue\tcore_axis(unprocessed path)\tcore_axis(if processed were used)\tideal_core_axis\tdiff");
                float[] positions = new float[0];
                var list = new List<float>();
                for (float p = -1f; p <= 1.0001f; p += 1f / 32f) list.Add(p);
                foreach (float p in new[] { 0.2f, 0.23f, 0.2399f, 0.2401f, 0.25f, 0.26f, 0.27f, 0.3f, 0.125f, 0.925f, 0.95f, 0.01f, 0.05f, 0.1f }) { list.Add(p); list.Add(-p); }
                list.Sort(); positions = list.ToArray();

                foreach (var rig in rigs)
                {
                    float maxDiff = 0f, maxDiffProcessed = 0f; int rows = 0;
                    // centre: no input
                    rig.setSticks(0, 0, 0, 0); rig.setTriggers(0, 0);
                    var c = OriginalPadConvert.Convert(rig.pad, 0);
                    Log($"{rig.name}\tcentre\t0\tL({c.lx},{c.ly}) R({c.rx},{c.ry}) LT {c.leftTrigger} RT {c.rightTrigger}\t\t\t\t\t");
                    foreach (string axis in new[] { "LX", "LY", "RX", "RY" })
                    {
                        foreach (float p in positions)
                        {
                            float lx = 0, ly = 0, rx = 0, ry = 0;
                            switch (axis) { case "LX": lx = p; break; case "LY": ly = p; break; case "RX": rx = p; break; default: ry = p; break; }
                            rig.setSticks(lx, ly, rx, ry);
                            var cv = OriginalPadConvert.Convert(rig.pad, 0);
                            short v = axis == "LX" ? cv.lx : axis == "LY" ? cv.ly : axis == "RX" ? cv.rx : cv.ry;
                            Vector2 proc = axis[0] == 'L' ? rig.pad.leftStick.ReadValue() : rig.pad.rightStick.ReadValue();
                            float pv = axis[1] == 'X' ? proc.x : proc.y;
                            float core = CoreAxis(v), coreIfProcessed = CoreAxis(Ideal(pv)), ideal = CoreAxis(Ideal(p));
                            float diff = core - ideal; rows++;
                            if (Mathf.Abs(diff) > maxDiff) maxDiff = Mathf.Abs(diff);
                            if (Mathf.Abs(coreIfProcessed - ideal) > maxDiffProcessed) maxDiffProcessed = Mathf.Abs(coreIfProcessed - ideal);
                            Log($"{rig.name}\t{axis}\t{p:0.####}\t{v}\t{pv:0.######}\t{core:0.######}\t{coreIfProcessed:0.######}\t{ideal:0.######}\t{diff:0.######}");
                        }
                    }
                    rig.setSticks(0, 0, 0, 0);
                    // triggers: every byte
                    int trigBad = 0, trigMax = 0;
                    for (int b = 0; b <= 255; b++)
                    {
                        rig.setTriggers(b, 255 - b);
                        var cv = OriginalPadConvert.Convert(rig.pad, 0);
                        int d = Math.Max(Math.Abs(cv.leftTrigger - b), Math.Abs(cv.rightTrigger - (255 - b)));
                        if (d != 0) trigBad++; if (d > trigMax) trigMax = d;
                        if (b % 15 == 0 || b == 255 || (b >= 28 && b <= 32)) Log($"{rig.name}\tTRIG\tLT {b} RT {255 - b}\t{cv.leftTrigger},{cv.rightTrigger}\t{rig.pad.leftTrigger.ReadValue():0.######},{rig.pad.rightTrigger.ReadValue():0.######}\t\t\t\t{d}");
                    }
                    rig.setTriggers(0, 0);
                    Log($"#SUMMARY\t{rig.name}\t{rig.note}\tstick points {rows}\tmax |core axis - ideal| unprocessed {maxDiff:0.######}\tif Input System processed values were used {maxDiffProcessed:0.######}\ttrigger bytes off {trigBad} (max {trigMax})");
                    if (maxDiff > 0.01f && !rig.name.StartsWith("Dual") && !rig.name.StartsWith("Switch")) bad++;
                    if (trigBad > 0 && !rig.name.StartsWith("Dual")) bad++;
                }
                foreach (var rig in rigs) InputSystem.RemoveDevice(rig.pad);
                InputSystem.settings.backgroundBehavior = prevBg;
            }
            catch (Exception e) { Log("EXCEPTION " + e); bad++; }
            File.WriteAllText(outPath, report.ToString());
            Debug.Log("[InputPipeline] " + (bad == 0 ? "PASS" : "FAIL " + bad) + " -> " + outPath);
            EditorApplicationExit(bad == 0 ? 0 : 1);
        }
        static void EditorApplicationExit(int code) { UnityEditor.EditorApplication.Exit(code); }
    }
}
