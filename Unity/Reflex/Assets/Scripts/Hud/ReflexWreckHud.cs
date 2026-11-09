// The wreck-recovery prompt of the MODERN HUD (the original's "Arrows" component, MXUIReflexComponent: core/include/rx_reflex_hud.h): what the
// view draws, from the core's copy of the component (rx_session_reflex_hud): while a landing / a crash can still be saved, the direction the
// stick has to be flicked, how much of the window is left, how far the flick has gone, and the outcome (the "WreckRecoverySuccess" /
// "WreckRecoveryFailed" events, kept as data only). The prompt is up exactly while the core's component is shown: the original hides its arrows the
// frame the wreck kind P + 0xb44 is cleared (a save: 0054ec80 -> 0054e890; a fall: 0054ed10 -> 0054e890), with no outcome display and no fade-out
// (MXUIReflexComponent::vf11 005f7b80 has no success handler; the fail handler 005f7b00 only makes three arrows transparent). The control that does it is named as the game binds it: with a pad the
// stick the preset routes the wreck input to (ControllerHandedness / ControllerMotion: input_source), with a keyboard the keys of the
// remap table's ReflexLeft / Right / Up / Down columns (CONTROLLER GLYPHS decides which device is meant when both are plugged in).
using System;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.Hud
{
    public sealed class ReflexWreckHud
    {
        /// What names the control: an arrow key (the direction is the key), a key with a name, a stick, the tilt of the pad.
        public enum Cap { None, ArrowKey, NamedKey, LeftStick, RightStick, Tilt }

        public bool active;            // the prompt is up (the arrows are shown)
        public int direction;          // RxReflexHudState.DirUp / DirRight / DirDown / DirLeft
        public float remaining = 1f;   // the window left, 1 -> 0
        public float flick;            // the flick so far, 0 -> 1 (1 = enough: the recovery)
        public int result;             // the last outcome: 0 none (a prompt is up), 1 recovered, 2 failed - data only, never drawn
        public int resultSerial;       // changes with every outcome
        public int beginSerial;        // changes with every prompt
        public Cap cap; public string capName = "";
        public float seconds;          // the window left in seconds (the gallery's labels)

        float lastActiveAt = -10f; int lastDirection; bool synced; uint seenSuccess, seenFail, seenBegin;

        /// The prompt is on screen: only while the component is shown (the original hides it the frame of the save / the fall, nothing lingers).
        public bool Visible(float now) => active;

        /// From the runtime's last completed frame.
        public void Fill(OriginalWorldRuntime rt, float now)
        {
            if (!rt || !rt.HasReflexHud) { Apply(default, false, now, null); return; }
            Apply(rt.ReflexHud, true, now, null);
        }

        /// One state. capOverride: the control to name (the gallery); null = the game's binding.
        public void Apply(RxReflexHudState r, bool have, float now, string capOverride)
        {
            bool was = active || now - lastActiveAt < 0.3f;
            if (!have) { active = false; synced = false; return; }
            if (!synced || r.successEvents < seenSuccess || r.failEvents < seenFail || r.beginEvents < seenBegin)
            { synced = true; seenSuccess = r.successEvents; seenFail = r.failEvents; seenBegin = r.beginEvents; result = 0; }   // a new activity: the baseline
            bool shown = r.shown != 0;
            // the outcome first: it comes in the frame that hides the arrows
            if (r.successEvents > seenSuccess) { seenSuccess = r.successEvents; if (was) Finish(1, now); }
            if (r.failEvents > seenFail) { seenFail = r.failEvents; if (was) Finish(2, now); }
            if (shown)
            {
                int dir = r.direction != 0 ? r.direction : lastDirection;
                if (dir == 0) dir = RxReflexHudState.DirUp;
                if (!active)
                {   // a new prompt
                    beginSerial++; result = 0; lastDirection = dir; ResolveCap(r, dir, capOverride);
                }
                else if (dir != direction) ResolveCap(r, dir, capOverride);
                active = true; direction = dir; lastDirection = dir; lastActiveAt = now;
                float max = Mathf.Max(0.001f, r.timeMax);
                remaining = Mathf.Clamp01(r.timeLeft / max); seconds = Mathf.Max(0f, r.timeLeft);
                flick = r.threshold > 0f ? Mathf.Clamp01(r.progress / r.threshold) : 0f;
            }
            else active = false;
            seenBegin = r.beginEvents;
        }

        void Finish(int what, float now) { result = what; resultSerial++; if (what == 1) flick = 1f; }

        // ------------------------------------------------------------------------------------------------ the control's name
        void ResolveCap(RxReflexHudState r, int dir, string capOverride)
        {
            cap = Cap.None; capName = "";
            if (capOverride != null) { Parse(capOverride); return; }
            // the game's wreck maps (00548fc0 / 00549050): 0 the compound reflex input (keys, mouse, right stick), 1 the right stick, 2 the left stick, 3 the motion sensor
            uint source = r.inputSource;
            if (source == 3) { cap = Cap.Tilt; capName = "TILT"; return; }
            if (source == 1) { cap = Cap.RightStick; capName = "R STICK"; return; }
            if (source == 2) { cap = Cap.LeftStick; capName = "L STICK"; return; }
            if (OuGlyphs.Current != OuGlyphs.Device.Keyboard) { cap = Cap.RightStick; capName = "R STICK"; return; }
            KeyCap(dir);
        }

        // the key the remap table binds to ReflexLeft (column 6) / Right (7) / Up (8) / Down (9)
        void KeyCap(int dir)
        {
            string name = null;
            try
            {
                var table = Reflex.Port.FrontEnd.CoreRemapping.Read();
                int col = dir == RxReflexHudState.DirLeft ? 6 : dir == RxReflexHudState.DirRight ? 7 : dir == RxReflexHudState.DirUp ? 8 : 9;
                if (table != null && col < table.kbText.Length) name = table.kbText[col];
            }
            catch (Exception) { }
            Parse(string.IsNullOrEmpty(name) ? DefaultArrow(dir) : name);
        }

        static string DefaultArrow(int dir) => dir == RxReflexHudState.DirLeft ? "LeftArrow" : dir == RxReflexHudState.DirRight ? "RightArrow" : dir == RxReflexHudState.DirUp ? "UpArrow" : "DownArrow";

        /// "UpArrow" / "J" / "R STICK" / "TILT" into the cap.
        void Parse(string name)
        {
            switch (name)
            {
                case "UpArrow": case "DownArrow": case "LeftArrow": case "RightArrow": cap = Cap.ArrowKey; capName = name; return;
                case "R STICK": cap = Cap.RightStick; capName = name; return;
                case "L STICK": cap = Cap.LeftStick; capName = name; return;
                case "TILT": cap = Cap.Tilt; capName = name; return;
            }
            cap = Cap.NamedKey; capName = KeyLabel(name);
        }

        /// A key's name as a cap shows it ("LeftShift" -> "L SHIFT", "Digit5" -> "5", "Numpad4" -> "NUM 4").
        public static string KeyLabel(string key)
        {
            if (string.IsNullOrEmpty(key)) return "?";
            if (key.StartsWith("Digit", StringComparison.Ordinal)) return key.Substring(5);
            if (key.StartsWith("Numpad", StringComparison.Ordinal)) return "NUM " + key.Substring(6).ToUpperInvariant();
            if (key.StartsWith("Left", StringComparison.Ordinal) && key.Length > 4) return "L " + key.Substring(4).ToUpperInvariant();
            if (key.StartsWith("Right", StringComparison.Ordinal) && key.Length > 5) return "R " + key.Substring(5).ToUpperInvariant();
            return key.ToUpperInvariant();
        }
    }
}
