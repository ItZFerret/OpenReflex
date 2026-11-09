using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Reflex.Port.OpenFrontEnd;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // The menus' navigation core (OpenFrontEnd/OuInput.cs): the direction stepper (a stick flickering around its threshold, a diagonal, the
    // repeat and its speed up, the hold that survives a rebuilt page) and the glyph names that follow the device.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.MenuNavTests.Verify -quit
    // (the walk of the real screens with synthetic pads is the showcase's: tests/menus_showcase/run.py --walk)
    public static class MenuNavTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[MENUNAV] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }

        // steps over a time line: (time, stick) pairs, returns the times a step fired
        static List<float> Run(OuStepper s, params (float t, Vector2 v)[] line)
        {
            var fired = new List<float>();
            for (int i = 0; i < line.Length; i++) { var d = s.Step(line[i].v, line[i].t); if (d != Vector2Int.zero) fired.Add(line[i].t); }
            return fired;
        }
        static (float, Vector2)[] Hold(Vector2 v, float from, float to, float dt = 1f / 60f) { var l = new List<(float, Vector2)>(); for (float t = from; t < to; t += dt) l.Add((t, v)); return l.ToArray(); }

        public static void Verify()
        {
            report.Clear(); bad = 0;
            try
            {
                // 1. a held direction: one step, a first delay, repeats, faster after a while, stops when released
                var s = new OuStepper();
                var fired = Run(s, Hold(Vector2.down, 0, 3f));
                Log("held down 3 s: " + fired.Count + " steps, at " + string.Join(" ", fired.Take(6).Select(t => t.ToString("0.00"))));
                Check(fired[0] == 0f, "the step is at once");
                Check(fired[1] >= 0.39f && fired[1] <= 0.42f, "the first repeat comes after 0.4 s (" + fired[1].ToString("0.00") + ")");
                Check(fired[2] - fired[1] >= 0.12f && fired[2] - fired[1] <= 0.15f, "then every ~0.13 s (" + (fired[2] - fired[1]).ToString("0.000") + ")");
                Check(fired[fired.Count - 1] - fired[fired.Count - 2] <= 0.09f, "and faster after eight repeats (" + (fired[fired.Count - 1] - fired[fired.Count - 2]).ToString("0.000") + ")");
                Check(fired.Count >= 22 && fired.Count <= 34, "3 s of hold is " + fired.Count + " steps (22..34)");
                Check(s.Step(Vector2.zero, 3.1f) == Vector2Int.zero && s.Held == Vector2Int.zero, "released: no step and the hold is clear");
                Check(s.Step(Vector2.down, 3.2f) == Vector2Int.down, "a new press steps at once");

                // 2. a stick resting near the threshold: ONE step, not one per crossing (hysteresis 0.6 / 0.35)
                s = new OuStepper();
                var line = new List<(float, Vector2)>(); float t0 = 0;
                for (int i = 0; i < 20; i++) { line.Add((t0, new Vector2(0, i % 2 == 0 ? -0.62f : -0.48f))); t0 += 0.016f; }   // 0.62 / 0.48: flicker across the old 0.5 line
                fired = Run(s, line.ToArray());
                Check(fired.Count == 1, "a stick flickering between 0.48 and 0.62 steps once (" + fired.Count + ")");
                s = new OuStepper();
                line.Clear(); t0 = 0;
                for (int i = 0; i < 40; i++) { line.Add((t0, new Vector2(0, i % 2 == 0 ? -0.7f : -0.2f))); t0 += 0.016f; }    // dips below the release 0.35: a new press each time
                fired = Run(s, line.ToArray());
                Check(fired.Count == 20, "a stick that really lets go every other frame steps each time (" + fired.Count + ")");

                // 3. a diagonal does not flip between the axes
                s = new OuStepper();
                line.Clear(); t0 = 0;
                for (int i = 0; i < 30; i++) { line.Add((t0, i % 2 == 0 ? new Vector2(0.62f, -0.70f) : new Vector2(0.68f, -0.64f))); t0 += 0.016f; }
                fired = Run(s, line.ToArray());
                var dirs = new HashSet<Vector2Int>(); s = new OuStepper(); foreach (var (tt, v) in line) { var d = s.Step(v, tt); if (d != Vector2Int.zero) dirs.Add(d); }
                Check(dirs.Count == 1, "a diagonal stick steps along one axis (" + string.Join(",", dirs) + ")");
                // ... but a clear turn goes to the other axis
                s = new OuStepper(); s.Step(Vector2.down, 0);
                Check(s.Step(new Vector2(0.95f, -0.1f), 0.02f) == Vector2Int.right, "a clear turn from down to right steps right");

                // 4. the hold survives a rebuilt page: a new OuMenuInput (a screen that builds itself on a step) does not fire the direction again
                OuMenuInput.ResetHold();
                float clock = 0; int frame = 0;
                OuMenuInput.Clock = () => clock; OuMenuInput.Frame = () => frame;
                var a = new OuMenuInput();
                var st = new List<Vector2Int>();
                for (int i = 0; i < 12; i++)
                {
                    frame++; clock += 0.016f;
                    if (i == 5) { a.Dispose(); a = new OuMenuInput(); }   // the page rebuilt itself
                    st.Add(OuMenuInput.StepShared(Vector2.down));
                }
                Check(st.Count(x => x != Vector2Int.zero) == 1, "a hold handed to a new OuMenuInput keeps its delay (" + st.Count(x => x != Vector2Int.zero) + " steps in 12 frames)");
                frame++; clock += 0.016f; var x1 = OuMenuInput.StepShared(Vector2.up); var x2 = OuMenuInput.StepShared(Vector2.down);
                Check(x1 == x2, "two callers in a frame read the same step (the shell and the options)");
                a.Dispose();
                OuMenuInput.Clock = () => Time.unscaledTime; OuMenuInput.Frame = () => Time.frameCount; OuMenuInput.ResetHold();

                // 5. the names follow the device
                OuDevice.Injected = true;
                string pad = OuGlyphs.Retarget("Enter") + "/" + OuGlyphs.Retarget("Esc") + "/" + OuGlyphs.Retarget("Q / E");
                OuDevice.Injected = false;
                string keys = OuGlyphs.Retarget("A") + "/" + OuGlyphs.Retarget("B") + "/" + OuGlyphs.Retarget("LB / RB");
                OuDevice.Injected = null;
                Log("on a pad: " + pad + "; on the keyboard: " + keys);
                Check(keys == "Enter/Esc/Q / E", "a pad's hint names become the keyboard's when the keyboard is used");
                Check(OuGlyphs.CapClass("Cross") != null && OuGlyphs.CapClass("Enter") == null && OuGlyphs.CapClass("LT / RT") != null, "pad caps are drawn as pad buttons, keys as keys");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            Log(bad == 0 ? "PASS" : "FAIL " + bad);
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
