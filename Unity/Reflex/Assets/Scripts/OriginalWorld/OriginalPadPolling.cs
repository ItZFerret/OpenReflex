using System;
using System.Globalization;
using System.Linq;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    /// How often Unity samples the pads that need polling. On Windows only the XInput pads are polled (InputSystem.pollingFrequency docs: the
    /// default is "at least 60 Hz"; every other device type, and macOS HID, is event driven). The original asks XInput for the state
    /// once per frame, at the frame start (XenonInputManager::vf43 -> Pad_Update 008157a0 -> XInputGetState), so each frame sees the stick
    /// where it is right then. At 60 Hz polling against a 60 Hz game frame the two clocks drift: a frame sometimes sees no new sample (the stick
    /// seems frozen) and the next one two (a double step). The core's air control works on the per-frame change of the stick (bike_air.c
    /// air_flick: (now - before) / dt, the strongest rate kept), so the stick's per-frame step is part of the feel. Sampling faster keeps
    /// the latest value close to the frame start. Parity fix, reasoned from the original's per-frame read (effect not measured on hardware).
    /// PlayerPrefs ReflexPadPollHz overrides (0 = Unity's default); so do -reflexPadPollHz <hz> / REFLEX_PAD_POLL_HZ (w-joincrash: a tester
    /// can try another rate from a shortcut, without the registry).
    ///
    /// (w-joincrash) The input devices in the log: the polling rate and every device at the start (Unity's device id, layout, interface,
    /// product, state size), then each add / remove / reconnect / disconnect. A tester's crash dump held an Input System state event
    /// ('STAT', 64 bytes, device 8) written into freed memory; the next report says which device that id is.
    public static class OriginalPadPolling
    {
        public const float DefaultHz = 500f;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Apply()
        {
            float hz = DefaultHz; string from = "the default";
            try { hz = PlayerPrefs.GetFloat("ReflexPadPollHz", DefaultHz); if (hz != DefaultHz) from = "PlayerPrefs ReflexPadPollHz"; } catch { }
            string arg = Arg("-reflexPadPollHz") ?? Environment.GetEnvironmentVariable("REFLEX_PAD_POLL_HZ");
            if (!string.IsNullOrEmpty(arg) && float.TryParse(arg, NumberStyles.Float, CultureInfo.InvariantCulture, out float o)) { hz = o; from = "-reflexPadPollHz / REFLEX_PAD_POLL_HZ"; }
            if (hz > 0f) try { InputSystem.pollingFrequency = Mathf.Clamp(hz, 60f, 1000f); } catch { }
            try
            {
                Debug.Log($"[input] pad polling {InputSystem.pollingFrequency:0} Hz ({(hz > 0f ? from : "Unity's own rate: " + from)}); devices: " +
                          (InputSystem.devices.Count == 0 ? "none yet" : string.Join("; ", InputSystem.devices.Select(Describe))));
                InputSystem.onDeviceChange += (d, change) =>
                {
                    if (change == InputDeviceChange.Added || change == InputDeviceChange.Removed || change == InputDeviceChange.Reconnected || change == InputDeviceChange.Disconnected)
                        Debug.Log($"[input] device {change.ToString().ToLowerInvariant()}: {Describe(d)}");
                };
            }
            catch (Exception e) { Debug.LogWarning("[input] the device list: " + e.Message); }
        }

        /// "#8 XInputControllerWindows (XInput, Controller (Xbox One For Windows), state 16 bytes)".
        static string Describe(InputDevice d)
        {
            if (d == null) return "?";
            try
            {
                var desc = d.description;
                string product = string.IsNullOrEmpty(desc.product) ? d.displayName : desc.product;
                string maker = string.IsNullOrEmpty(desc.manufacturer) ? "" : desc.manufacturer + " ";
                return $"#{d.deviceId} {d.layout} ({(string.IsNullOrEmpty(desc.interfaceName) ? "-" : desc.interfaceName)}, {maker}{product}, state {(d.stateBlock.sizeInBits + 7) / 8} bytes{(d.native ? "" : ", made by the game")})";
            }
            catch (Exception) { return "#" + d.deviceId + " ?"; }
        }

        static string Arg(string name) { var a = Environment.GetCommandLineArgs(); int i = Array.IndexOf(a, name); return i >= 0 && i + 1 < a.Length ? a[i + 1] : null; }
    }
}
