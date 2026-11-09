// TEST ONLY (w-menufps; a tester: "FPS drops when navigating the menus"): the menus walked with a synthetic pad as a player browses them, every
// frame's time recorded by phase, so a build's menu frame times can be compared before / after. Page "menufps". The profile is the seeded save
// (run.py --seed-dir testdata/moto_career_flow) so the 3D menu shows the player's own rider and vehicle and the load arena's session is made in
// the menus as on a player's machine (OpenArenaWarm). Nothing is bought, saved or ridden.
//   REFLEX_MENUFPS_WORKERS=<n>   Unity's job workers limited to n (a weaker CPU: fewer cores for the frame's jobs)
//   REFLEX_MENUFPS_DWELL=<s>     the seconds the cursor rests on an event card of another vehicle (default 2.6: past OpenArenaWarm's 2 s, so
//                                the arena's session is made again for it, as when a player reads a card)
// Each phase's line: "[ui-showcase] [menufps] phase <name>: N frames, mean / p50 / p99 / max ms, >33 / >50 / >100 ms, gc +n" and a total.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Unity.Jobs.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    [DefaultExecutionOrder(-31000)]
    sealed class MenuFpsRecorder : MonoBehaviour
    {
        public string phase;
        public readonly Dictionary<string, List<float>> frames = new Dictionary<string, List<float>>();
        public readonly Dictionary<string, int> gc = new Dictionary<string, int>();
        public readonly List<string> order = new List<string>();
        int lastGc = -1; bool skip = true;
        // (the frame a phase begins in carries its first action, a page opened by the coroutine: it is the phase's next sample, not skipped)
        public void Begin(string p) { phase = p; if (!frames.ContainsKey(p)) { frames[p] = new List<float>(); gc[p] = 0; order.Add(p); } }
        public void End() { phase = null; }
        void Update()
        {
            int g = GC.CollectionCount(0);
            if (phase != null && !skip) { frames[phase].Add(Time.unscaledDeltaTime * 1000f); if (lastGc >= 0) gc[phase] += g - lastGc; }
            skip = false; lastGc = g;
        }
    }

    static class UiShowcaseMenuFps
    {
        const string Tag = "[ui-showcase] [menufps] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("menufps", Run);

        static MenuFpsRecorder rec;
        static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

        static string Stats(string name, List<float> f, int gcs)
        {
            if (f.Count == 0) return name + ": no frames";
            var s = f.OrderBy(x => x).ToArray();
            float P(float q) => s[Mathf.Clamp((int)(s.Length * q), 0, s.Length - 1)];
            return Inv($"{name}: {f.Count} frames over {f.Sum() / 1000f:0.0} s, mean {f.Average():0.00} ms ({1000f / f.Average():0} fps), p50 {P(0.5f):0.00}, p99 {P(0.99f):0.0}, max {s[s.Length - 1]:0.0} ms, >33 ms {f.Count(x => x > 33.3f)}, >50 ms {f.Count(x => x > 50f)}, >100 ms {f.Count(x => x > 100f)}, gc +{gcs}");
        }

        static IEnumerator Phase(string name, IEnumerator body)
        {
            Debug.Log(Tag + "begin " + name);
            rec.Begin(name);
            yield return body;
            rec.End();
            Debug.Log(Tag + "phase " + Stats(name, rec.frames[name], rec.gc[name]));
        }

        static IEnumerator Wait(float seconds) { for (float t = 0; t < seconds; t += Time.unscaledDeltaTime) yield return null; }

        static IEnumerator Tap(GamepadButton b, float after) { yield return UiShowcaseNav.PadTap(b); yield return Wait(after); }

        static IEnumerator Run()
        {
            var shell = OpenShell.Instance;
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_MENUFPS_WORKERS") ?? "", out int workers) && workers >= 0)
            {
                Debug.Log(Tag + $"job workers {JobsUtility.JobWorkerCount} -> {workers} (of {JobsUtility.JobWorkerMaximumCount})");
                JobsUtility.JobWorkerCount = workers;
            }
            float dwell = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_MENUFPS_DWELL") ?? "", NumberStyles.Float, CultureInfo.InvariantCulture, out float d) ? d : 2.6f;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open().Wait();
            Debug.Log(Tag + $"cpu {SystemInfo.processorType} x{SystemInfo.processorCount}, memory {SystemInfo.systemMemorySize} MB, gpu {SystemInfo.graphicsDeviceName}, {Screen.width}x{Screen.height}, vsync {QualitySettings.vSyncCount}, target {Application.targetFrameRate}, arena warm {(OpenArenaWarm.Enabled ? "on" : "off")}");
            var go = new GameObject("menufps recorder TEST ONLY"); UnityEngine.Object.DontDestroyOnLoad(go); rec = go.AddComponent<MenuFpsRecorder>();
            for (float t = 0; t < 90f && OpenMenuWorld.Instance && OpenMenuWorld.Instance.Status == OpenMenuWorld.State.Loading; t += Time.unscaledDeltaTime) yield return null;
            Debug.Log(Tag + "menu world " + (OpenMenuWorld.Instance ? OpenMenuWorld.Instance.Status.ToString() : "none"));
            yield return UiShowcase.Frames(30);

            // the hub with the player's own rider and vehicle (the profile came after the shell: the hero is built now), then a player's first seconds
            yield return Phase("hub-enter", Enter(shell));
            yield return Phase("hub-idle", Wait(10f));
            // RACE: the browser, the cursor moved over the cards, the event types stepped with RB, a rest on some cards (the arena's ride follows the vehicle)
            yield return Phase("race-browse", RaceBrowse(shell, dwell));
            // the setup page of the card under the cursor and back
            yield return Phase("setup", Setup(shell));
            // FREE RIDE's browser
            yield return Phase("freeride-browse", FreeRide(shell, dwell));
            // back to the hub, a rest
            yield return Phase("hub-return", HubReturn(shell));
            // the garage's tabs
            yield return Phase("garage", Garage(shell));
            yield return Phase("hub-final", HubReturn(shell));
            // the setup page's RIDER tab of an event ridden with the equipped vehicle, then the hub: the 3D hero is the same rider and vehicle
            yield return Phase("setup-rider", SetupRider(shell));

            var all = rec.order.SelectMany(p => rec.frames[p]).ToList();
            Debug.Log(Tag + "phase " + Stats("ALL", all, rec.gc.Values.Sum()));
            Debug.Log(Tag + "arena warm at the end: " + (OpenArenaWarm.Current ? (OpenArenaWarm.Current.Ready ? "ready" : "starting") : "none"));
        }

        static IEnumerator Enter(OpenShell shell)
        {
            shell.ShowMain();
            yield return Wait(4f);
        }

        static IEnumerator RaceBrowse(OpenShell shell, float dwell)
        {
            shell.ShowBrowser(false);
            yield return Wait(1.5f);
            var seen = new HashSet<int>();
            for (int round = 0; round < 6; round++)
            {
                for (int i = 0; i < 5; i++) yield return Tap(GamepadButton.DpadRight, 0.3f);
                // a rest on a card whose vehicle the arena has not ridden yet (another class / type: the session is made again)
                var g = shell.SelectedEvent != null && shell.Setup != null ? OpenLoadArena.Selection(shell.SelectedEvent, shell.Setup) : null;
                int v = g?.vehicleId ?? -1;
                Debug.Log(Tag + $"rest on {shell.SelectedEvent?.key} (vehicle {v}{(seen.Add(v) ? ", new" : "")})");
                yield return Wait(dwell);
                yield return Tap(GamepadButton.RightShoulder, 0.6f);   // the next event type
            }
            for (int i = 0; i < 12; i++) yield return Tap(GamepadButton.DpadDown, 0.25f);
        }

        static IEnumerator Setup(OpenShell shell)
        {
            if (shell.Current != OpenShell.Screen.Browser) { shell.ShowBrowser(false); yield return Wait(1f); }
            yield return Tap(GamepadButton.South, 1.2f);
            Debug.Log(Tag + "setup page: " + shell.Current);
            for (int i = 0; i < 6; i++) yield return Tap(GamepadButton.DpadDown, 0.3f);
            for (int i = 0; i < 3; i++) yield return Tap(GamepadButton.DpadRight, 0.4f);
            yield return Tap(GamepadButton.RightShoulder, 1f);
            yield return Tap(GamepadButton.LeftShoulder, 1f);
            yield return Tap(GamepadButton.East, 1f);
        }

        static IEnumerator SetupRider(OpenShell shell)
        {
            var w = OpenMenuWorld.Instance;
            string key = Environment.GetEnvironmentVariable("REFLEX_MENUFPS_EVENT") ?? "National/NAT_Renne";
            shell.ShowBrowser(false); yield return Wait(1f);
            Debug.Log(Tag + "select " + key + ": " + shell.SelectEvent(key));
            yield return Wait(0.5f);
            yield return Tap(GamepadButton.South, 1f);
            yield return Tap(GamepadButton.RightShoulder, 2f);   // EVENT -> RIDER (the setup phase left the page on EVENT)
            Debug.Log(Tag + $"setup RIDER: hero {w?.HeroKey}, last build {w?.LastHeroBuildMs:F0} ms");
            yield return Wait(2f);
            yield return Tap(GamepadButton.East, 1f);
            shell.ShowMain();
            yield return Wait(3f);
            Debug.Log(Tag + $"hub again: hero {w?.HeroKey}, last build {w?.LastHeroBuildMs:F0} ms");
        }

        static IEnumerator FreeRide(OpenShell shell, float dwell)
        {
            shell.ShowBrowser(true);
            yield return Wait(1.5f);
            for (int round = 0; round < 3; round++)
            {
                for (int i = 0; i < 6; i++) yield return Tap(GamepadButton.DpadRight, 0.3f);
                yield return Wait(dwell);
                yield return Tap(GamepadButton.RightShoulder, 0.6f);
            }
        }

        static IEnumerator HubReturn(OpenShell shell)
        {
            shell.ShowMain();
            yield return Wait(6f);
        }

        static IEnumerator Garage(OpenShell shell)
        {
            shell.ShowGarage(0);
            yield return Wait(2f);
            for (int tab = 0; tab < OpenShell.GarageTabCount; tab++)
            {
                for (int i = 0; i < 4; i++) yield return Tap(GamepadButton.DpadDown, 0.35f);
                yield return Tap(GamepadButton.RightShoulder, 1f);
            }
        }
    }
}
