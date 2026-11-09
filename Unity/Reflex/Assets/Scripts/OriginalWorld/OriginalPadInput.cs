using UnityEngine;
using UnityEngine.InputSystem;
using Reflex.Port.OpenFrontEnd;

namespace Reflex.Port.OriginalWorld
{
    /// One frame of a gamepad's menu buttons for the install-art pages (OriginalPauseMenu, OriginalPostRace and their confirmation popups), whose
    /// keyboard code never read a pad: the d-pad / left stick as steps (the menus' stepper: a first delay, then a repeat, hysteresis on the stick),
    /// A accepts, B goes back, Start is the game's Start (opens the pause menu, resumes, continues a results page).
    public struct OriginalPad
    {
        public bool up, down, left, right, accept, back, start;
        public bool Any => up || down || left || right || accept || back || start;
    }

    public static class OriginalPadInput
    {
        static readonly OuStepper stepper = new OuStepper();
        static int frame = -1;
        static OriginalPad cached;

        /// This frame's buttons of every connected pad (the same value however many callers ask in a frame).
        public static OriginalPad Read()
        {
            if (frame == Time.frameCount) return cached;
            frame = Time.frameCount;
            var pad = new OriginalPad(); Vector2 move = Vector2.zero;
            foreach (var g in Gamepad.all)
            {
                move += g.dpad.ReadValue() + g.leftStick.ReadValue();
                if (g.buttonSouth.wasPressedThisFrame) pad.accept = true;
                if (g.buttonEast.wasPressedThisFrame) pad.back = true;
                if (g.startButton.wasPressedThisFrame) pad.start = true;
            }
            var step = stepper.Step(Vector2.ClampMagnitude(move, 1f), Time.unscaledTime);
            pad.up = step.y > 0; pad.down = step.y < 0; pad.left = step.x < 0; pad.right = step.x > 0;
            return cached = pad;
        }
    }
}
