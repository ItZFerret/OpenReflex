// TEST ONLY (tests/menus_showcase fixtures, run with tests/waypoint_arrow/wp_arrow.py): the Waypoint race's direction cue in a real ride.
// The original's waypoint "arrow" is its HUD compass (MXUICompassComponent, UI kind 6, 005ff150): the Center Indicator chevron over the strip,
// the next gate's flag icon at its bearing (pinned to the strip's edge past 45 degrees: the way to turn) with its distance under it, and the gate
// after it at 0.35. With REFLEX_WPARROW_PROBE=1 the player, for each level of REFLEX_WPARROW_LEVELS (default FR_Alpine,FR_Cheyenne), muted:
//   1. launches the level's Waypoint race (no AI), skips the intro with a virtual pad and waits for InProgress;
//   2. shoots HUD STYLE ORIGINAL and MODERN at the start (<level>-original-start / -modern-start) and logs the compass' state (the gate slots up,
//      their alpha and x, the label; the MODERN model's next / following gate);
//   3. rides to the next gate with an autopilot (steers at the gate the compass marks; a virtual pad) until the core's last gate (rx_session_compass
//      waypoint_gate) moves on, then shoots both styles again (<level>-original-after / -modern-after) and logs the state.
// Checks: the gates are exported (one mark per gate), the ORIGINAL compass shows slot gate+1 at alpha 1 with the distance label and slot gate+2 at
// 0.35, both move on after the gate, the MODERN compass shows its markers. -batchmode: the shots are composed offscreen (UiShowcaseOffscreen).
// Result lines "[WpArrow] PASS|FAIL ..." and "RESULT PASS|FAIL"; the player quits at the end.
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    public sealed class WaypointArrowProbe : MonoBehaviour
    {
        static bool Wanted => Environment.GetEnvironmentVariable("REFLEX_WPARROW_PROBE") == "1";
        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
        string shots; Gamepad pad; int fails; OriginalWorldRuntime rt;
        Vector3 bikePos, bikeFwd; float speed; bool have, drive; float stuck, reverse;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { if (Wanted) { ReflexProfile.Disabled = true; OpenShell.SkipIntro = true; } }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (!Wanted || FindAnyObjectByType<WaypointArrowProbe>()) return;
            var g = new GameObject("waypoint arrow probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<WaypointArrowProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            if (Application.isBatchMode)
            {   // as UiShowcaseOffscreen.Init: the shot's size for the layouts and the canvases' scalers
                var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
                if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var p && p.Length == 2 && int.TryParse(p[0], out int w) && int.TryParse(p[1], out int h) && w > 0 && h > 0)
                    UiShowcaseOffscreen.Size = new Vector2Int(w, h);
                OuLayout.ScreenOverride = UiShowcaseOffscreen.Size;
                try { Screen.SetResolution(UiShowcaseOffscreen.Size.x, UiShowcaseOffscreen.Size.y, FullScreenMode.Windowed); } catch (Exception) { }
            }
            OriginalWorldRuntime.ViewApplied += OnView;
            StartCoroutine(Run());
        }
        void OnDestroy() { OriginalWorldRuntime.ViewApplied -= OnView; }
        void LateUpdate() { AudioListener.volume = 0f; }   // test players are muted

        static void Log(string s) => Debug.Log("[WpArrow] " + s);
        bool Check(bool ok, string what) { Log((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; return ok; }
        static string F(float v) => v.ToString("0.0", Inv);
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }

        void OnView(OriginalWorldRuntime world, OriginalSimulation.Results v)
        {
            if (v.bikePose == null || v.bikeNodes < 1) return;
            bikePos = new Vector3(v.bikePose[12], v.bikePose[13], v.bikePose[14]); bikeFwd = new Vector3(v.bikePose[8], v.bikePose[9], v.bikePose[10]);
            speed = v.bike.speed; have = true;
        }

        IEnumerator Shot(string name)
        {
            if (string.IsNullOrEmpty(shots)) yield break;
            string path = Path.Combine(shots, name + ".png");
            if (Application.isBatchMode) yield return UiShowcaseOffscreen.Shot(path);
            else { yield return new WaitForEndOfFrame(); ScreenCapture.CaptureScreenshot(path); yield return Frames(2); }
            Log("shot " + name);
        }
        IEnumerator Tap(GamepadButton b)
        {
            InputSystem.QueueStateEvent(pad, new GamepadState(b)); yield return Frames(3);
            InputSystem.QueueStateEvent(pad, new GamepadState()); yield return Frames(4);
        }

        // the next gate's world position (the compass' slot gate + 1), false when none
        bool NextGate(out Vector3 pos, out int last)
        {
            pos = default; last = -2;
            if (!rt || !rt.HasCompass) return false;
            var c = rt.Compass; last = c.waypointGate;
            for (int i = 0; i < Mathf.Min((int)c.count, RxCompassState.Marks); i++)
                if (c.Type(i) == RxCompassState.TypeGate && c.Index(i) == c.waypointGate + 1) { var p = c.Position(i); pos = new Vector3(p.x, p.y, p.z); return true; }
            return false;
        }

        // the autopilot: at the next gate, a speed by the turn (GripRideProbe's), backing out when stuck
        void Update()
        {
            if (pad == null || !pad.added) return;
            if (!drive || !have || !NextGate(out var gate, out _)) return;
            float err = Mathf.DeltaAngle(Mathf.Atan2(bikeFwd.x, bikeFwd.z) * Mathf.Rad2Deg, Mathf.Atan2(gate.x - bikePos.x, gate.z - bikePos.z) * Mathf.Rad2Deg) * Mathf.Deg2Rad;
            float steer = Mathf.Clamp(err * 2.2f, -1f, 1f), turn = Mathf.Abs(err);
            float desired = turn > 1.2f ? 8f : turn > 0.6f ? 14f : turn > 0.3f ? 20f : 30f;
            float dt = Time.unscaledDeltaTime;
            if (speed < 1f) stuck += dt; else stuck = 0;
            if (stuck > 4f) { reverse = 1.5f; stuck = 0; }
            var s = new GamepadState();
            if (reverse > 0) { reverse -= dt; s.leftTrigger = 1f; s.leftStick = new Vector2(-steer, 0); }
            else { s.leftStick = new Vector2(steer, 0); if (speed < desired) s.rightTrigger = 1f; else if (speed > desired + 6f) s.leftTrigger = 0.63f; else s.rightTrigger = 0.47f; }
            InputSystem.QueueStateEvent(pad, s);
        }

        ReflexModernHud Hud => rt ? rt.GetComponentInChildren<ReflexModernHud>(true) ?? FindAnyObjectByType<ReflexModernHud>() : null;

        // the compass of the ORIGINAL page and the MODERN model, logged and checked
        bool LogState(string level, string when, bool original)
        {
            var c = rt.Compass; int last = c.waypointGate, gates = 0;
            for (int i = 0; i < Mathf.Min((int)c.count, RxCompassState.Marks); i++) if (c.Type(i) == RxCompassState.TypeGate) gates++;
            Log($"{level} {when}: compass valid {c.valid} gates {gates} last gate {last} finished {c.waypointFinished}; race gate_index {rt.Waypoint.gate_index} / {rt.Waypoint.gate_count}; bike {F(bikePos.x)},{F(bikePos.z)} speed {F(speed)}");
            var hud = Hud; bool ok = true;
            if (original)
            {
                var v = hud ? hud.OriginalFreeRide : null; var cv = v?.Compass;
                ok &= Check(v != null && v.Current == OriginalFreeRideHud.Page.Waypoint && cv != null, $"{level} {when}: the ORIGINAL HUD shows the waypoint layer's compass (page {(v != null ? v.Current.ToString() : "none")})");
                if (cv == null) return false;
                string up = string.Join(" ", Enumerable.Range(0, OriginalFreeRideHud.GateSlots).Where(k => cv.gateUp[k]).Select(k => $"WaypointGate{k + 1}(alpha {cv.gateAlpha[k]:0.00}, x {F(cv.gateX[k])})"));
                Log($"{level} {when}: ORIGINAL compass heading {F(cv.heading)}: {up}; label \"{cv.labelText}\"");
                int next = last + 1;
                ok &= Check(next < OriginalFreeRideHud.GateSlots && cv.gateUp[next] && Mathf.Approximately(cv.gateAlpha[next], 1f), $"{level} {when}: WaypointGate{next + 1} (the next gate) is up at alpha 1");
                bool hasAfter = next + 1 < gates;
                ok &= Check(!hasAfter || cv.gateUp[next + 1] && Mathf.Abs(cv.gateAlpha[next + 1] - 0.35f) < 1e-3f, $"{level} {when}: the gate after it is {(hasAfter ? "up at 0.35" : "none")}");
                ok &= Check(Enumerable.Range(0, OriginalFreeRideHud.GateSlots).Count(k => cv.gateUp[k]) == (hasAfter ? 2 : 1), $"{level} {when}: no other gate is up");
                ok &= Check(!string.IsNullOrEmpty(cv.labelText), $"{level} {when}: the distance label reads \"{cv.labelText}\"");
            }
            else
            {
                var m = hud ? hud.Model : null;
                if (m != null) Log($"{level} {when}: MODERN heading {F(m.heading)} next gate {m.nextGateShown} at {F(m.nextGateAngle)} ({F(m.nextGateDistance)} ft) after {m.afterGateShown} at {F(m.afterGateAngle)}");
                ok &= Check(m != null && m.nextGateShown, $"{level} {when}: the MODERN compass marks the next gate");
            }
            return ok;
        }

        IEnumerator Run()
        {
            pad = InputSystem.AddDevice<Gamepad>("waypoint arrow probe pad");
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!Check(shell, "the shell started")) { Quit(); yield break; }
            yield return Seconds(2f);
            var levels = (Environment.GetEnvironmentVariable("REFLEX_WPARROW_LEVELS") ?? "FR_Alpine,FR_Cheyenne").Split(',').Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();
            foreach (var level in levels)
            {
                var ev = ReflexEvents.All.FirstOrDefault(e => e.spec.type == "WaypointRace" && e.location == level);
                if (!Check(ev != null, level + ": a Waypoint race event")) continue;
                Log("== " + level + " (" + ev.key + ")");
                ReflexSettings.HudStyle = HudStyle.Original;
                ReflexEventSelect.Launch(ev.key, 0, false, "Reflex", 0, null);
                rt = null;
                for (float t = 0; t < 400f; t += Time.unscaledDeltaTime)
                {
                    rt = FindAnyObjectByType<OriginalWorldRuntime>();
                    if (rt && rt.FailureMessage != null) { Check(false, level + ": world " + rt.FailureMessage); break; }
                    if (rt && rt.Running && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                    yield return null;
                }
                if (!Check(rt && rt.Running, level + ": the world runs")) continue;
                if (rt.IntroCardShown) { yield return Seconds(1f); yield return Tap(GamepadButton.South); }
                for (float t = 0; t < 120f && !(rt && rt.Race.state == 2); t += Time.unscaledDeltaTime) yield return null;
                if (!Check(rt && rt.Race.state == 2, level + ": the race is InProgress")) continue;
                yield return Seconds(2f);
                Check(rt.IsWaypoint && rt.HasCompass && rt.Compass.valid != 0, level + ": a Waypoint race with the compass export");
                LogState(level, "start", true);
                yield return Shot(level + "-original-start");
                ReflexSettings.HudStyle = HudStyle.Modern; yield return Seconds(1.2f);
                LogState(level, "start", false);
                yield return Shot(level + "-modern-start");
                ReflexSettings.HudStyle = HudStyle.Original; yield return Seconds(0.5f);

                // ride to the next gate
                int before = rt.Compass.waypointGate; float t0 = Time.realtimeSinceStartup; bool passed = false, midShot = false;
                drive = true;
                while (Time.realtimeSinceStartup - t0 < 240f)
                {
                    if (rt.Compass.waypointGate != before) { passed = true; break; }
                    if (!midShot && Time.realtimeSinceStartup - t0 > 6f) { midShot = true; LogState(level, "riding", true); yield return Shot(level + "-original-riding"); }
                    yield return null;
                }
                drive = false; InputSystem.QueueStateEvent(pad, new GamepadState { leftTrigger = 1f });
                if (Check(passed, $"{level}: the next gate was taken (last gate {before} -> {rt.Compass.waypointGate}) in {F(Time.realtimeSinceStartup - t0)} s"))
                {
                    yield return Seconds(1.5f);
                    InputSystem.QueueStateEvent(pad, new GamepadState());
                    LogState(level, "after", true);
                    yield return Shot(level + "-original-after");
                    ReflexSettings.HudStyle = HudStyle.Modern; yield return Seconds(1.2f);
                    LogState(level, "after", false);
                    yield return Shot(level + "-modern-after");
                    ReflexSettings.HudStyle = HudStyle.Original;
                }
                InputSystem.QueueStateEvent(pad, new GamepadState());
                OriginalPauseMenu.QuitToFrontEnd();
                for (float t = 0; t < 60f && FindAnyObjectByType<OriginalWorldRuntime>(); t += Time.unscaledDeltaTime) yield return null;
                yield return Seconds(2.5f);
            }
            Log(fails == 0 ? "RESULT PASS" : "RESULT FAIL (" + fails + ")");
            Log("DONE");
            yield return Frames(30);
            Quit();
        }

        static void Quit()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}
