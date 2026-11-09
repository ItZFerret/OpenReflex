using System.Globalization;
using System.Text;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // Local Player.log only. Bounded startup samples distinguish OS detection,
    // menu suppression, native slot routing and the simulation's consumed inputs.
    public static class ControllerInputDiagnostics
    {
        static OriginalWorldRuntime owner;
        static int samples;
        static long generation;
        static float next;
        static bool sawRaw, sawForwarded;
        static int localId;
        static string Number(float value) => value.ToString("0.###", CultureInfo.InvariantCulture);

        /// The world went away: the sample state lets go of it (a static reference kept the destroyed world's graph alive, w-gcspikes).
        public static void Forget(OriginalWorldRuntime runtime) { if (ReferenceEquals(owner, runtime)) owner = null; }

        public static void Record(OriginalWorldRuntime runtime, ReflexPad[] pads, uint mask, bool menu)
        {
            if (!runtime.Running) return;
            if (owner != runtime || generation != runtime.WorldGeneration)
            {
                owner = runtime; generation = runtime.WorldGeneration; samples = 0; next = 0; sawRaw = sawForwarded = false; localId = -1;
            }
            var pad = runtime.PadRouting.Pad(0);
            var left = pad != null ? pad.leftStick.ReadUnprocessedValue() : Vector2.zero;
            var right = pad != null ? pad.rightStick.ReadUnprocessedValue() : Vector2.zero;
            float lt = pad != null ? pad.leftTrigger.ReadUnprocessedValue() : 0;
            float rt = pad != null ? pad.rightTrigger.ReadUnprocessedValue() : 0;
            bool raw = left.sqrMagnitude > .04f || right.sqrMagnitude > .04f || lt > .1f || rt > .1f;
            var sent = pads[0];
            bool forwarded = (mask & 1) != 0 && (sent.lx != 0 || sent.ly != 0 || sent.rx != 0 || sent.ry != 0 || sent.leftTrigger != 0 || sent.rightTrigger != 0 || sent.buttons != 0);
            int id = pad != null ? pad.deviceId : 0;
            bool changed = localId != id || raw && !sawRaw || forwarded && !sawForwarded;
            if (!changed && (samples >= 12 || Time.unscaledTime < next)) return;
            localId = id; sawRaw |= raw; sawForwarded |= forwarded;
            samples++; next = Time.unscaledTime + 5f;
            var text = new StringBuilder("[ControllerInput] local=").Append(id)
                .Append(" device=").Append(pad != null ? pad.displayName : "none")
                .Append(" layout=").Append(pad != null ? pad.layout : "none")
                .Append(" enabled=").Append(pad != null && pad.enabled)
                .Append(" focus=").Append(Application.isFocused)
                .Append(" menu=").Append(menu).Append(" paused=").Append(runtime.paused)
                .Append(" rawSticks=").Append(Number(left.x)).Append(',').Append(Number(left.y)).Append(',').Append(Number(right.x)).Append(',').Append(Number(right.y))
                .Append(" rawTriggers=").Append(Number(lt)).Append(',').Append(Number(rt))
                .Append(" mask=").Append(mask).Append(" sentSticks=").Append(sent.lx).Append(',').Append(sent.ly).Append(',').Append(sent.rx).Append(',').Append(sent.ry)
                .Append(" sentTriggers=").Append(sent.leftTrigger).Append(',').Append(sent.rightTrigger)
                .Append(" nativeInputs=");
            var inputs = runtime.Bike.inputs;
            if (inputs != null) for (int i = 0; i < inputs.Length; i++) text.Append(i == 0 ? "" : ",").Append(Number(inputs[i]));
            else text.Append("pending");
            text.Append(" devices=");
            foreach (var device in Gamepad.all) text.Append('[').Append(device.deviceId).Append(':').Append(device.layout).Append(':').Append(device.enabled).Append(']');
            Debug.Log(text.ToString());
        }
    }
}
