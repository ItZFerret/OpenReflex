// TEST ONLY (w-fb-hud; tests/menus_showcase fixtures, run with tests/fb_hud/fbhud_ride.py --batch): the four playtester items in the real
// flow, driven by a VIRTUAL PAD and KEYBOARD (Input System state events: the real bindings and pages are in the loop), on a scratch save.
//   REFLEX_FBHUD_PROBE=1, REFLEX_FBHUD_SCRIPT=all (run 1) | persist (run 2, on run 1's save: --seed-dir)
// run 1:
//   controls (menu)   OPTIONS > CONTROLS from the hub: A on BRAKE's key cell, the O key: saved (core-controls-v1.json)
//   freeride 1        FreeRide/FR_Alpine through the shell's launch:
//     gauge           the MODERN gauge in the ride: the presented frame carries the engine (TryGetEngine), the HUD model hasRpm, the ring is the rev
//                     band (REV GAUGE RPM); the pause page's OPTIONS (CAMERA & HUD) row "hud-rev-gauge" with the d-pad: SPEED -> the speed ring,
//                     OFF -> no ring, RPM -> the rev band again (each saved, read by the running HUD)
//     controls        the pause page's OPTIONS > CONTROLS: A on THROTTLE's key cell, the U key: saved, applied to the running session at once
//                     (rx_session_remap_apply answered 0; the session's table reads U), and holding U rides the bike (W is not held)
//     reset track     the pause page's RESET TRACK row (Free Ride): the deformation mirror back to its state at the world's start, the tyre
//                     tracks gone, the rider where he was; then the bindable control (ReflexSettings.ResetTrackKey on G) does it again
//     music           the race bag's first track and the game's own pick
//   menu              back in the hub: CONTROLS shows U / O
//   freeride 2        the same level again: the new session started with U (its table), the game's pick is the same as in ride 1 (the cause),
//                     the bag's first track is another one
//   races             National with 2 AI: no RESET TRACK (refused, not on the pause page); National alone (0 AI): RESET TRACK offered
//   finally           REV GAUGE set to SPEED on the hub's options page (run 2 checks it after the restart)
// run 2 ("persist", the same save after a restart): the controls file and the CONTROLS page still show U / O; REV GAUGE reads SPEED and the
//   ring in a Free Ride is the speed ring.
// Result lines "[FbHudProbe] PASS|FAIL ..." and "RESULT PASS|FAIL"; the player quits at the end.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    public sealed class FbHudProbe : MonoBehaviour
    {
        static string Script => Environment.GetEnvironmentVariable("REFLEX_FBHUD_SCRIPT") ?? "all";
        Gamepad pad; Keyboard kb; int fails; OriginalWorldRuntime rt;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_FBHUD_PROBE") != "1") return;
            OpenShell.SkipIntro = true;
            var g = new GameObject("fb-hud probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<FbHudProbe>();
        }

        void Start()
        {
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            StartCoroutine(Run());
        }
        void LateUpdate() { AudioListener.volume = 0f; }   // test players are muted

        void Log(string s) => Debug.Log("[FbHudProbe] " + s);
        bool Check(bool ok, string what) { Log((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; return ok; }
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }

        void Devices()
        {
            if (pad == null || !pad.added) pad = InputSystem.AddDevice<Gamepad>("fb-hud probe pad");
            if (kb == null || !kb.added) kb = InputSystem.AddDevice<Keyboard>("fb-hud probe keyboard");
        }
        IEnumerator Tap(GamepadButton b, int frames = 3)
        {
            Devices();
            InputSystem.QueueStateEvent(pad, new GamepadState(b)); yield return Frames(frames);
            InputSystem.QueueStateEvent(pad, new GamepadState()); yield return Frames(5);
        }
        IEnumerator Key(Key k, int frames = 3)
        {
            Devices();
            InputSystem.QueueStateEvent(kb, new KeyboardState(k)); yield return Frames(frames);
            InputSystem.QueueStateEvent(kb, new KeyboardState()); yield return Frames(5);
        }
        void Hold(Key k) { Devices(); InputSystem.QueueStateEvent(kb, k == UnityEngine.InputSystem.Key.None ? new KeyboardState() : new KeyboardState(k)); }
        void Throttle(bool on) { Devices(); var s = new GamepadState(); s.rightTrigger = on ? 1f : 0f; InputSystem.QueueStateEvent(pad, s); }

        OwnRacePause Pause => rt ? rt.racePause : null;
        string PauseFocus() { var f = Pause?.Root?.panel?.focusController?.focusedElement as VisualElement; return f?.name ?? "(none)"; }
        static VisualElement Opt(string name) => ReflexOptions.Root?.Q<VisualElement>(name);
        static string OptFocus() { var f = ReflexOptions.Root?.panel?.focusController?.focusedElement as VisualElement; return f?.name ?? "(none)"; }
        static string CellText(string name) => Opt(name)?.Q<Label>()?.text;
        static string RowValue(string name) => Opt(name)?.Q<Label>(className: "mxa-value")?.text;
        static IEnumerator Reveal(string name)
        {
            var v = Opt(name); if (v == null) yield break;
            v.Focus(); v.GetFirstAncestorOfType<ScrollView>()?.ScrollTo(v);
            for (int i = 0; i < 8; i++) yield return null;
        }
        float Speed => rt ? Mathf.Abs(rt.Bike.speed) * 0.681818f : 0f;
        Vector3 BikePos => rt && rt.Bike.position != null && rt.Bike.position.Length >= 3 ? new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]) : Vector3.zero;

        // the MODERN gauge's ring as drawn: ReflexHudView.ringKind (0 rev band, 1 speed, 2 none) and fillShown
        static readonly FieldInfo RingKindField = typeof(ReflexHudView).GetField("ringKind", BindingFlags.NonPublic | BindingFlags.Instance);
        static readonly FieldInfo FillField = typeof(ReflexHudView).GetField("fillShown", BindingFlags.NonPublic | BindingFlags.Instance);
        ReflexModernHud Hud => rt ? rt.GetComponent<ReflexModernHud>() ?? FindAnyObjectByType<ReflexModernHud>() : null;
        int Ring() { var h = Hud; return h && h.View != null ? (int)RingKindField.GetValue(h.View) : -9; }
        float Fill() { var h = Hud; return h && h.View != null ? (float)FillField.GetValue(h.View) : -9f; }

        static int Dik(Key k) { foreach (var kv in DirectInputMap.Table) if (kv.Key == k) return kv.Value; return 0; }

        // the running session's table (rx_session_remap_get on the worker)
        volatile int sessionKb0 = int.MinValue, sessionKb1 = int.MinValue;
        void ReadSessionTable()
        {
            sessionKb0 = sessionKb1 = int.MinValue;
            var open = rt ? rt.SimDriver?.Open : null; if (open == null) return;
            open.BetweenFrames(s => { var t = new RxRemapTable(); if (RxApiNative.rx_session_remap_get(s, ref t) == 0) { sessionKb1 = Kb(ref t, 1); sessionKb0 = Kb(ref t, 0); } else sessionKb0 = -1; });
        }
        static unsafe int Kb(ref RxRemapTable t, int i) { fixed (int* p = t.keyboard) return p[i]; }

        IEnumerator Run()
        {
            OpenShell shell = null;
            for (float t = 0; t < 180f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!Check(shell, "the shell started")) { Quit(); yield break; }
            Devices();
            yield return Seconds(3f);
            Log("script " + Script + ", music bag seed " + ReflexMusicBags.Seed + ", save " + CoreGarageProfile.SaveDirectory);
            if (Script == "persist") yield return Persist();
            else yield return All();
            Log(fails == 0 ? "RESULT PASS" : "RESULT FAIL (" + fails + ")");
            yield return Frames(30);
            Quit();
        }

        int KeyIdOf(Key k) { var t = CoreRemapping.Read(); return ReflexRemap.KeyId(t, Dik(k)); }

        // ------------------------------------------------------------------------------------------------ run 1
        IEnumerator All()
        {
            Check(!ReflexSave.Has("Options.HudRevGauge") && ReflexSettings.RevGauge == ReflexSettings.RevGaugeRpm, "a new save: REV GAUGE RPM (the default)");
            Check(ReflexSettings.HudStyle == HudStyle.Modern, "HUD STYLE is MODERN (the open front end's default)");
            int idU = KeyIdOf(UnityEngine.InputSystem.Key.U), idO = KeyIdOf(UnityEngine.InputSystem.Key.O);
            var t0 = CoreRemapping.Read();
            Log($"default keyboard row: throttle {t0.kbText[0]} ({t0.kb[0]}), brake {t0.kbText[1]} ({t0.kb[1]}); U = {idU}, O = {idO}");

            // 1. CONTROLS in the hub: BRAKE -> O
            ReflexOptions.Open(ReflexOptions.Controls); yield return Seconds(1.5f);
            yield return Rebind("remap-kb-1", UnityEngine.InputSystem.Key.O, "the hub");
            Check(CoreRemapping.Read().kb[1] == idO, $"the hub: BRAKE saved as O in {Path.GetFileName(CoreRemapping.FilePath)} ({CoreRemapping.Read().kbText[1]})");
            yield return CloseOptions();

            // 2. the first Free Ride
            yield return Launch("FreeRide/FR_Alpine", 0);
            if (!rt) yield break;
            var terrain = FindAnyObjectByType<OriginalTerrain>();
            float[] pristine = terrain && terrain.DeformMirror.Length > 0 ? (float[])terrain.DeformMirror.Clone() : null;
            Check(pristine != null, "the terrain's deformation mirror at the world's start (" + (pristine?.Length ?? 0) + " texels)");
            var music = FindAnyObjectByType<OriginalMusic>();

            // 2a. the gauge in the ride
            Throttle(true); yield return Seconds(1.5f);
            Check(rt.TryGetEngine(out float rpm, out float idle, out float max), $"the presented frame carries the engine (TryGetEngine: {rpm:0} rpm, idle {idle:0}, limiter {max:0})");
            var hud = Hud;
            Check(hud && hud.Model.hasRpm, "the MODERN HUD's model hasRpm (" + (hud ? hud.Model.rpm.ToString("0") + " rpm, gauge " + hud.Model.rpmGauge.ToString("0.00") + ", speed " + hud.Model.speedMph.ToString("0") + " mph" : "no HUD") + ")");
            Check(Ring() == 0, "REV GAUGE RPM: the ring is the rev band (ringKind " + Ring() + ", fill " + Fill().ToString("0.00") + ")");
            float maxGauge = 0f, maxFillGap = 0f;
            for (float t = 0; t < 2.5f; t += Time.unscaledDeltaTime) { if (hud) { maxGauge = Mathf.Max(maxGauge, hud.Model.rpmGauge); } yield return null; }
            if (hud) maxFillGap = Mathf.Abs(Fill() - hud.Model.rpmGauge);
            Throttle(false);
            Check(maxGauge > 0.2f, $"the rev band moves with the engine (gauge up to {maxGauge:0.00}; the fill {Fill():0.00} against the speed ring's {(hud ? hud.Model.speedMph / 90f : 0):0.00})");
            Log($"fill - rpm gauge at the end of the throttle: {maxFillGap:0.00}");
            yield return Seconds(4f);   // (coast to a stop for the reset's position check)

            // 2b. REV GAUGE from the pause page's OPTIONS (CAMERA & HUD)
            foreach (var (want, ring, label) in new[] { (ReflexSettings.RevGaugeSpeed, 1, "SPEED"), (ReflexSettings.RevGaugeOff, 2, "OFF"), (ReflexSettings.RevGaugeRpm, 0, "RPM") })
            {
                yield return OpenPause();
                yield return PauseRow("pause-options");
                yield return Tap(GamepadButton.South); yield return Seconds(1f);
                if (!Check(ReflexOptions.IsOpen && ReflexOptions.Tab == ReflexOptions.CameraHud, "the pause page's OPTIONS opens CAMERA & HUD (tab " + ReflexOptions.Tab + ")")) break;
                yield return Reveal("hud-rev-gauge");
                Check(OptFocus() == "hud-rev-gauge", "the REV GAUGE row has the focus (" + OptFocus() + ")");
                yield return StepTo("hud-rev-gauge", label);
                Check(ReflexSettings.RevGauge == want && ReflexSave.GetInt("Options.HudRevGauge", -1) == want && RowValue("hud-rev-gauge") == label, $"d-pad right: REV GAUGE {label}, saved {ReflexSave.GetInt("Options.HudRevGauge", -1)} (the row shows {RowValue("hud-rev-gauge")})");
                yield return CloseOptions();
                yield return Resume();
                yield return Seconds(0.8f);
                Check(Ring() == ring, $"REV GAUGE {label} in the ride: ringKind {Ring()} (want {ring}), fill {Fill():0.00}");
            }

            // 2c. CONTROLS from the pause page: THROTTLE -> U, applied to the running session
            yield return OpenPause();
            yield return PauseRow("pause-options");
            yield return Tap(GamepadButton.South); yield return Seconds(1f);
            ReflexOptions.ShowTab(ReflexOptions.Controls); yield return Seconds(1f);
            int applied0 = CoreRemapping.AppliedCount;
            yield return Rebind("remap-kb-0", UnityEngine.InputSystem.Key.U, "the pause page");
            Check(CoreRemapping.Read().kb[0] == idU, "the pause page: THROTTLE saved as U (" + CoreRemapping.Read().kbText[0] + ")");
            yield return CloseOptions();
            yield return Resume();
            for (float t = 0; t < 5f && CoreRemapping.AppliedCount == applied0; t += Time.unscaledDeltaTime) yield return null;
            Check(CoreRemapping.AppliedCount > applied0 && CoreRemapping.LastApplied == 0, "the running session took the bindings (rx_session_remap_apply " + CoreRemapping.LastApplied + ")");
            ReadSessionTable(); for (float t = 0; t < 3f && sessionKb0 == int.MinValue; t += Time.unscaledDeltaTime) yield return null;
            Check(sessionKb0 == idU && sessionKb1 == idO, $"the session's table: throttle {sessionKb0} (U {idU}), brake {sessionKb1} (O {idO})");
            float v0 = Speed; Vector3 p0 = BikePos;
            Hold(UnityEngine.InputSystem.Key.U); yield return Seconds(4f); float vU = Speed; Vector3 pU = BikePos; Hold(UnityEngine.InputSystem.Key.None);
            Check(vU > v0 + 5f && (pU - p0).magnitude > 10f, $"holding U rides the bike at once ({v0:0} -> {vU:0} mph, {(pU - p0).magnitude:0} ft)");
            yield return Seconds(1f);
            Hold(UnityEngine.InputSystem.Key.O); yield return Seconds(3f); Hold(UnityEngine.InputSystem.Key.None);
            Log($"after holding O (brake): {Speed:0} mph");
            yield return Stop();
            float vW0 = Speed; Vector3 pW0 = BikePos;
            Hold(UnityEngine.InputSystem.Key.W); yield return Seconds(2.5f); float vW = Speed; Vector3 pW = BikePos; Hold(UnityEngine.InputSystem.Key.None);
            Check(vW <= vW0 + 1.5f && (pW - pW0).magnitude < 8f, $"W (the old throttle) no longer rides it ({vW0:0} -> {vW:0} mph, {(pW - pW0).magnitude:0} ft in 2.5 s)");

            // 2d. ruts, then RESET TRACK from the pause page
            Throttle(true); yield return Seconds(5f); Throttle(false); yield return Stop();
            int changed = Diff(terrain, pristine); int tris = Tracks();
            Check(changed > 0, $"riding deformed the terrain ({changed} texels off the start, {tris} tyre-track triangles)");
            yield return OpenPause();
            Check(Pause.RowNames.Contains("pause-reset-track"), "Free Ride's pause page offers RESET TRACK (" + string.Join(", ", Pause.RowNames) + ")");
            Vector3 before = BikePos; int count0 = ReflexTrackReset.Count;
            yield return PauseRow("pause-reset-track");
            yield return Tap(GamepadButton.South);
            for (float t = 0; t < 5f && ReflexTrackReset.Count == count0; t += Time.unscaledDeltaTime) yield return null;
            yield return Frames(12);   // (the next frame clears it; the whole-grid patch reaches the terrain on the frame it is collected)
            Check(!Pause.IsOpen && !rt.paused, "RESET TRACK closes the pause page and the ride goes on");
            Check(ReflexTrackReset.Count == count0 + 1 && ReflexTrackReset.LastResult == 0, "the core armed the reset (rx_session_terrain_reset " + ReflexTrackReset.LastResult + ")");
            int after = Diff(terrain, pristine);
            Check(after == 0, $"the deformation is back to the world's start ({after} texels differ; {changed} before)");
            Check(Tracks() < Math.Max(1, tris), $"the tyre tracks are gone ({tris} -> {Tracks()} triangles)");
            Check((BikePos - before).magnitude < 4f, $"the rider stays where he is ({(BikePos - before).magnitude:0.0} ft)");
            // the bindable control (unbound by default): G
            Check(ReflexSettings.ResetTrackKey == 0 && ReflexSettings.ResetTrackPad == 0, "RESET TRACK's own control is unbound by default");
            ReflexSettings.ResetTrackKey = Dik(UnityEngine.InputSystem.Key.G); ReflexSettings.Save();
            Throttle(true); yield return Seconds(4f); Throttle(false); yield return Stop();
            int changed2 = Diff(terrain, pristine);
            yield return Key(UnityEngine.InputSystem.Key.G);
            for (float t = 0; t < 5f && ReflexTrackReset.Count == count0 + 1; t += Time.unscaledDeltaTime) yield return null;
            yield return Frames(12);
            Check(ReflexTrackReset.Count == count0 + 2 && Diff(terrain, pristine) == 0, $"the RESET TRACK control (G) resets it too ({changed2} -> {Diff(terrain, pristine)} texels)");
            ReflexSettings.ResetTrackKey = 0; ReflexSettings.Save();

            // 2e. the music of ride 1
            for (float t = 0; t < 60f && music && music.TracksPlayed.Count == 0; t += Time.unscaledDeltaTime) yield return null;
            int game1 = music && music.GameTracksPlayed.Count > 0 ? music.GameTracksPlayed[0] : -1, mine1 = music && music.TracksPlayed.Count > 0 ? music.TracksPlayed[0] : -1;
            Check(mine1 >= 0, $"ride 1's first track: the game's entry {game1}, the bag's track {mine1} (playlist [{(music ? string.Join(",", music.GamePlaylist) : "")}])");
            yield return QuitToMenu();

            // 3. the hub's CONTROLS show both
            ReflexOptions.Open(ReflexOptions.Controls); yield return Seconds(1.5f);
            Check(CellText("remap-kb-0") == "U" && CellText("remap-kb-1") == "O", $"the hub's CONTROLS: THROTTLE {CellText("remap-kb-0")}, BRAKE {CellText("remap-kb-1")}");
            yield return CloseOptions();

            // 4. the same level again: a new session with U; the music
            yield return Launch("FreeRide/FR_Alpine", 0);
            if (!rt) yield break;
            ReadSessionTable(); for (float t = 0; t < 3f && sessionKb0 == int.MinValue; t += Time.unscaledDeltaTime) yield return null;
            Check(sessionKb0 == idU && sessionKb1 == idO, $"the next session starts with them (throttle {sessionKb0}, brake {sessionKb1})");
            music = FindAnyObjectByType<OriginalMusic>();
            for (float t = 0; t < 60f && music && music.TracksPlayed.Count == 0; t += Time.unscaledDeltaTime) yield return null;
            int game2 = music && music.GameTracksPlayed.Count > 0 ? music.GameTracksPlayed[0] : -1, mine2 = music && music.TracksPlayed.Count > 0 ? music.TracksPlayed[0] : -1;
            Log($"ride 2's first track: the game's entry {game2}, the bag's track {mine2}");
            Check(game2 == game1 && game1 >= 0, $"the game's own first pick repeats on the same level ({game1} / {game2}: why the songs repeated)");
            Check(mine2 >= 0 && mine2 != mine1, $"the bag plays another first track ({mine1} -> {mine2})");
            yield return QuitToMenu();

            // 5. races: with AI no RESET TRACK; alone it is offered
            string national = ReflexEvents.All.First(e => e.spec.type == "National").key;
            yield return Launch(national, 2);
            if (rt)
            {
                Check(ReflexTrackReset.Unavailable(rt) == ReflexTrackReset.OnlyHere, $"a race with AI ({rt.Race.riders} riders): RESET TRACK refused ({ReflexTrackReset.Unavailable(rt)})");
                yield return OpenPause();
                Check(Pause && Pause.IsOpen && !Pause.RowNames.Contains("pause-reset-track"), "its pause page has no RESET TRACK (" + (Pause ? string.Join(", ", Pause.RowNames) : "") + ")");
                yield return Resume();
                Check(!ReflexTrackReset.Reset(rt), "ReflexTrackReset.Reset refuses there");
                yield return QuitToMenu();
            }
            yield return Launch(national, 0);
            if (rt)
            {
                Check(ReflexTrackReset.Unavailable(rt) == "", $"a race alone ({rt.Race.riders} rider): RESET TRACK offered ({ReflexTrackReset.Unavailable(rt)})");
                yield return OpenPause();
                Check(Pause && Pause.IsOpen && Pause.RowNames.Contains("pause-reset-track"), "its pause page has RESET TRACK");
                yield return Resume();
                yield return QuitToMenu();
            }

            // 6. REV GAUGE SPEED for run 2 (the hub's options page)
            ReflexOptions.Open(ReflexOptions.CameraHud); yield return Seconds(1.5f);
            yield return Reveal("hud-rev-gauge");
            yield return StepTo("hud-rev-gauge", "SPEED");
            Check(ReflexSettings.RevGauge == ReflexSettings.RevGaugeSpeed, "the hub: REV GAUGE SPEED (for the restart)");
            yield return CloseOptions();
            ReflexSave.Save();
        }

        // ------------------------------------------------------------------------------------------------ run 2 (after a restart)
        IEnumerator Persist()
        {
            int idU = KeyIdOf(UnityEngine.InputSystem.Key.U), idO = KeyIdOf(UnityEngine.InputSystem.Key.O);
            var t = CoreRemapping.Read();
            Check(t.kb[0] == idU && t.kb[1] == idO, $"after the restart the controls file holds THROTTLE {t.kbText[0]}, BRAKE {t.kbText[1]}");
            ReflexOptions.Open(ReflexOptions.Controls); yield return Seconds(1.5f);
            Check(CellText("remap-kb-0") == "U" && CellText("remap-kb-1") == "O", $"the CONTROLS page shows THROTTLE {CellText("remap-kb-0")}, BRAKE {CellText("remap-kb-1")}");
            yield return CloseOptions();
            Check(ReflexSettings.RevGauge == ReflexSettings.RevGaugeSpeed, "REV GAUGE reads SPEED after the restart (" + ReflexSettings.RevGaugeNames[ReflexSettings.RevGauge] + ")");
            yield return Launch("FreeRide/FR_Alpine", 0);
            if (!rt) yield break;
            Throttle(true); yield return Seconds(2f); Throttle(false);
            Check(Ring() == 1 && Hud && Hud.Model.hasRpm, $"in the ride: the speed ring (ringKind {Ring()}; the engine is read: {Hud?.Model.hasRpm})");
            ReadSessionTable(); for (float x = 0; x < 3f && sessionKb0 == int.MinValue; x += Time.unscaledDeltaTime) yield return null;
            Check(sessionKb0 == idU, $"the session rides with U ({sessionKb0})");
            var music = FindAnyObjectByType<OriginalMusic>();
            for (float x = 0; x < 60f && music && music.TracksPlayed.Count == 0; x += Time.unscaledDeltaTime) yield return null;
            Log($"this launch's first track: the game's entry {(music && music.GameTracksPlayed.Count > 0 ? music.GameTracksPlayed[0] : -1)}, the bag's track {(music && music.TracksPlayed.Count > 0 ? music.TracksPlayed[0] : -1)} (seed {ReflexMusicBags.Seed})");
            yield return QuitToMenu();
        }

        // ------------------------------------------------------------------------------------------------ helpers
        IEnumerator Rebind(string cell, Key key, string where)
        {
            var e = Opt(cell);
            if (!Check(e != null, where + ": the cell " + cell + " is on the CONTROLS page")) yield break;
            yield return Reveal(cell);
            Check(OptFocus() == cell, where + ": " + cell + " has the focus (" + OptFocus() + ")");
            yield return Tap(GamepadButton.South); yield return Seconds(0.4f);
            Check(CellText(cell) == "???", where + ": A starts the binding (" + CellText(cell) + ")");
            yield return Key(key); yield return Frames(6);
            string status = ReflexOptions.Root?.Query<Label>(className: "mxa-keep").ToList().Select(l => l.text).FirstOrDefault(x => !string.IsNullOrEmpty(x));
            Check(CellText(cell) == key.ToString().ToUpperInvariant(), where + ": the key " + key + " is bound (" + CellText(cell) + "; \"" + status + "\")");
        }

        // the d-pad right one press at a time until the row shows `label` (a held tap can repeat at the batch player's low frame rate)
        IEnumerator StepTo(string row, string label)
        {
            for (int i = 0; i < 4 && RowValue(row) != label; i++) { yield return Tap(GamepadButton.DpadRight, 1); yield return Frames(6); }
        }
        // brakes (O: BRAKE after the hub's rebind) until the bike stands, then lets go
        IEnumerator Stop()
        {
            for (float t = 0; t < 10f && Speed > 0.5f; t += Time.unscaledDeltaTime) { Hold(UnityEngine.InputSystem.Key.O); yield return null; }
            Hold(UnityEngine.InputSystem.Key.None); yield return Seconds(1f);
            Log($"stopped: {Speed:0.0} mph");
        }

        IEnumerator CloseOptions()
        {
            for (int i = 0; i < 4 && ReflexOptions.IsOpen; i++) { yield return Tap(GamepadButton.East); yield return Seconds(0.4f); }
            Check(!ReflexOptions.IsOpen, "the options pages closed");
            yield return Seconds(0.5f);
        }

        IEnumerator OpenPause()
        {
            for (int i = 0; i < 3 && !(Pause && Pause.IsOpen); i++) { yield return Tap(GamepadButton.Start); yield return Seconds(1f); }
            Check(Pause && Pause.IsOpen, "Start opened the pause page" + (Pause && !Pause.IsOpen ? " (" + Pause.WhyNot() + ")" : ""));
        }
        IEnumerator PauseRow(string row)
        {
            for (int i = 0; i < 12 && PauseFocus() != row; i++)
            {
                var rows = Pause.RowNames.ToList();
                yield return Tap(rows.IndexOf(row) > rows.IndexOf(PauseFocus()) ? GamepadButton.DpadDown : GamepadButton.DpadUp);
            }
            Check(PauseFocus() == row, "the d-pad reaches " + row + " (" + PauseFocus() + ")");
        }
        IEnumerator Resume()
        {
            if (Pause && Pause.IsOpen) { yield return PauseRow("pause-resume"); yield return Tap(GamepadButton.South); yield return Seconds(0.5f); }
            Check(!(Pause && Pause.IsOpen) && rt && !rt.paused, "back to the ride");
        }

        IEnumerator Launch(string key, int ai)
        {
            Log("== launching " + key + " (" + ai + " AI)");
            ReflexEventSelect.Launch(key, ai, false, "Reflex", 0, null);
            rt = null; bool popup = false;
            for (float t = 0; t < 300f; t += Time.unscaledDeltaTime)
            {
                rt = FindAnyObjectByType<OriginalWorldRuntime>();
                if (rt && rt.Running && rt.EventUi.race_state == 2 && rt.EventUi.known != 0) break;
                if (rt && rt.IntroCardShown && !popup) { yield return Seconds(1f); yield return Tap(GamepadButton.South); popup = true; }
                yield return null;
            }
            if (!Check(rt && rt.Running && rt.EventUi.race_state == 2, key + ": InProgress")) { rt = null; yield break; }
            yield return Seconds(1.5f);
        }

        IEnumerator QuitToMenu()
        {
            yield return OpenPause();
            yield return PauseRow("pause-quit");
            yield return Tap(GamepadButton.South); yield return Frames(20);
            yield return Tap(GamepadButton.DpadRight); yield return Frames(5);
            yield return Tap(GamepadButton.South);
            for (float t = 0; t < 60f; t += Time.unscaledDeltaTime) { if (OpenShell.Instance && !FindAnyObjectByType<OriginalWorldRuntime>()) break; yield return null; }
            if (FindAnyObjectByType<OriginalWorldRuntime>()) { OriginalPauseMenu.QuitToFrontEnd(); yield return Seconds(4f); }
            Check(OpenShell.Instance && !FindAnyObjectByType<OriginalWorldRuntime>(), "back in the hub");
            rt = null;
            yield return Seconds(3f);
        }

        static int Diff(OriginalTerrain t, float[] start)
        {
            if (!t || start == null) return -1;
            var now = t.DeformMirror; if (now.Length != start.Length) return -2;
            int n = 0; for (int i = 0; i < now.Length; i++) if (Mathf.Abs(now[i] - start[i]) > 1e-5f) n++;
            return n;
        }
        static int Tracks() { var tr = FindAnyObjectByType<OriginalTracks>(); return tr ? tr.Triangles : -1; }

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
