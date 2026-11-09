using System;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Options
{
    /// RUMBLE STRENGTH: the motor speeds OriginalInput sent this frame (the game's XInputSetState values) scaled. Runs after
    /// OriginalInput (its Update); sends only when the scaled value changes. Added to the race by ReflexClientLive.OnWorldStart.
    [DefaultExecutionOrder(1000)]
    public sealed class ReflexRumbleScale : MonoBehaviour
    {
        OriginalInput input;
        OriginalWorldRuntime runtime;
        readonly UnityEngine.InputSystem.Gamepad[] devices = new UnityEngine.InputSystem.Gamepad[4];
        readonly float[] sent = new float[8], seen = new float[8];
        float lastScale = 1f;
        /// What was last sent to each pad's motors after the scale (0..1), for tests.
        public float Sent(int pad, bool right) => sent[pad * 2 + (right ? 1 : 0)];

        void LateUpdate() => ApplyScale(ReflexSettings.RumbleStrength);

        void ApplyScale(float k)
        {
            if (!input) input = GetComponent<OriginalInput>();
            if (!input) return;
            if (!runtime) runtime = GetComponent<OriginalWorldRuntime>();
            if (!runtime) return;
            runtime.PadRouting.Refresh();
            for (int i = 0; i < 4; i++)
            {
                float lo = input.AppliedMotor(i, false), hi = input.AppliedMotor(i, true);
                var device = runtime.PadRouting.Pad(i);
                bool deviceChanged = devices[i] != device; devices[i] = device;
                bool changed = deviceChanged || lo != seen[2 * i] || hi != seen[2 * i + 1] || k != lastScale;
                seen[2 * i] = lo; seen[2 * i + 1] = hi;
                if (!changed) continue;
                float slo = Mathf.Clamp01(lo * k), shi = Mathf.Clamp01(hi * k);
                sent[2 * i] = slo; sent[2 * i + 1] = shi;
                // the game's own value stands at 1 (OriginalInput sent it); anything else is sent over it
                if (k != 1f) runtime.PadRouting.SetRumble(i, slo, shi);
                else if (k == 1f && (lastScale != 1f || deviceChanged)) runtime.PadRouting.SetRumble(i, lo, hi);
            }
            lastScale = k;
        }

        void OnDisable() { if (runtime) runtime.PadRouting.StopRumble(); Array.Clear(sent, 0, 8); Array.Clear(devices, 0, devices.Length); }
    }
}
