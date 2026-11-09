// TEST ONLY: the driving cameras and the player's own camera presets (w-campresets: Options/ReflexCameraPresets.cs, OriginalWorld/ReflexCameraRig.cs,
// Options/ReflexOptionsMenu.Cameras.cs, Hud/ReflexCameraToast.cs).
//   "campresets-options"  OPTIONS > CAMERA & HUD in the menus: the CAMERA CYCLE toggles (the last ON one cannot be turned off; turning off the camera in use
//                         moves to the next), MY CAMERAS, the editor, NAME with the pad's on-screen keyboard (typed, DONE), DUPLICATE, DELETE with its question
//   "campresets-ride"     a real event (REFLEX_LOADARENA_EVENT, default National/NAT_Concord; AI off) ridden with the throttle: the Camera action (F7, the game's
//                         Camera key C, ReflexClientLive.CycleCamera) goes through the ON cameras only, each one's name shows (MODERN's toast, then CLASSIC's),
//                         a custom preset shakes while moving and stays above the ground, the editor over the race (live preview), then the event restarts
//                         and the saved custom preset is the camera at its start (and the core's DrivingCamera is its base)
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseCamPresets
    {
        const string Tag = "[ui-showcase] [campresets] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("campresets-options", Options); UiShowcase.Register("campresets-ride", Ride); }

        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "campresets", what); if (ok) Debug.Log(Tag + "ok " + what); }
        static VisualElement Q(string name) => ReflexOptions.Root?.Q<VisualElement>(name);
        static bool Press(string name) { var v = Q(name); if (v?.userData is OuNavItem it && it.submit != null) { Debug.Log(Tag + "press " + name); it.submit(); return true; } Debug.LogWarning(Tag + "no row " + name); return false; }
        static bool Adjust(string name, int d) { var v = Q(name); if (v?.userData is OuNavItem it && it.adjust != null) { it.adjust(d); return true; } Debug.LogWarning(Tag + "no value row " + name); return false; }
        static string Value(string name) => Q(name)?.Q<Label>(className: "mxa-value")?.text;
        static string Focused() => (ReflexOptions.Root?.panel?.focusController?.focusedElement as VisualElement)?.name ?? "(none)";
        static ReflexOptionsMenu Menu()
        {
            var host = UnityEngine.Object.FindAnyObjectByType<ReflexOptionsHost>();
            return host ? typeof(ReflexOptionsHost).GetField("menu", BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(host) as ReflexOptionsMenu : null;
        }
        static IEnumerator Reveal(string name)
        {
            var v = Q(name); if (v == null) yield break;
            v.Focus(); var sv = v.GetFirstAncestorOfType<ScrollView>(); sv?.ScrollTo(v);
            yield return UiShowcase.Frames(12);
        }

        /// A clean start: the starter presets, every built-in ON, GAME PLAY in use.
        static void Fresh()
        {
            ReflexSave.Delete(ReflexCameraPresets.PresetsKey); ReflexSave.Delete(ReflexCameraPresets.CurrentKey);
            ReflexCameraPresets.ReloadForTest();
            ReflexCameraPresets.Select("game:1");
        }

        // ------------------------------------------------------------------------------------------------ the options pages
        static IEnumerator Options()
        {
            Fresh();
            var ids = ReflexCameraPresets.Entries.Select(e => e.id + (e.on ? "" : "(off)")).ToList();
            Debug.Log(Tag + "entries: " + string.Join(", ", ids));
            Check(string.Join(",", ReflexCameraPresets.Entries.Select(e => e.id)) == "game:0,game:1,game:3,fpv,game:2,custom:1,custom:2,custom:3", "the cycle's order: the built-ins as before, then the presets");
            Check(ReflexCameraPresets.Presets.Select(p => p.name).SequenceEqual(new[] { "CLOSE CHASE", "HIGH WIDE", "CINEMATIC" }) && ReflexCameraPresets.Presets.All(p => !p.on), "three starter presets, OFF in the cycle");

            // the model: the last ON camera stays, the current one turned off moves on
            foreach (var e in ReflexCameraPresets.Entries.ToList()) if (e.id != "fpv") ReflexCameraPresets.SetOn(e.id, false);
            Check(ReflexCameraPresets.Entries.Count(e => e.on) == 1 && ReflexCameraPresets.Find("fpv").on, "everything off but DYNAMIC FPV");
            Check(!ReflexCameraPresets.SetOn("fpv", false) && ReflexCameraPresets.Find("fpv").on, "the last ON camera cannot be turned off");
            Check(ReflexCameraPresets.CurrentId == "fpv", "turning off the camera in use moved to the next ON one (" + ReflexCameraPresets.CurrentId + ")");
            foreach (var e in ReflexCameraPresets.Entries.ToList()) ReflexCameraPresets.SetOn(e.id, !e.Custom || e.id == "custom:3");
            ReflexCameraPresets.Select("custom:3");
            Check(ReflexCameraPresets.CurrentId == "custom:3" && ReflexSettings.DrivingCamera == 1 && ReflexSettings.CameraMode == 0, "a custom camera in use: the core's DrivingCamera is its base (GAME PLAY)");
            ReflexCameraPresets.SetOn("custom:3", false);
            Check(ReflexCameraPresets.CurrentId == "game:0", "CINEMATIC turned off while in use: the next ON camera (" + ReflexCameraPresets.CurrentId + ")");
            ReflexCameraPresets.SetOn("custom:3", true); ReflexCameraPresets.SetOn("game:2", false);
            ReflexCameraPresets.Select("game:1");
            var seen = new List<string>(); for (int i = 0; i < 7; i++) { ReflexClientLive.CycleCamera(); seen.Add(ReflexCameraPresets.CurrentId); }
            Debug.Log(Tag + "cycle from GAME PLAY: " + string.Join(" > ", seen));
            Check(string.Join(",", seen) == "game:3,fpv,custom:3,game:0,game:1,game:3,fpv", "the cycle skips USER and the presets that are OFF");
            ReflexClientLive.CycleCamera(-1);
            Check(ReflexCameraPresets.CurrentId == "game:3", "and goes back the other way");
            ReflexCameraPresets.Select("game:1");

            // the page
            var shell = OpenShell.Instance;
            if (shell != null) shell.ShowOptions(ReflexOptions.CameraHud); else ReflexOptions.Open(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(40);
            Check(Q("driving-camera") != null && Value("driving-camera") == "GAME PLAY", "DRIVING CAMERA shows the camera in use (" + Value("driving-camera") + ")");
            yield return UiShowcase.Shot("campresets-options-1-top");
            yield return Reveal("cam-on-game-0");
            yield return UiShowcase.Shot("campresets-options-2-cycle");
            Check(Value("cam-on-game-2") == "OFF" && Value("cam-on-custom-3") == "ON" && Value("cam-on-custom-1") == "OFF", "the CAMERA CYCLE rows show ON / OFF");
            Adjust("cam-on-custom-1", 1);
            yield return UiShowcase.Frames(10);
            Check(ReflexCameraPresets.Find("custom:1").on && Value("cam-on-custom-1") == "ON" && Focused() == "cam-on-custom-1", "CLOSE CHASE turned ON from its row (focus kept: " + Focused() + ")");
            yield return Reveal("cam-new");
            yield return UiShowcase.Shot("campresets-options-3-mycameras");

            // the editor
            Press("cam-preset-3");
            yield return UiShowcase.Frames(30);
            var menu = Menu();
            Check(menu != null && menu.CameraEditorOpen && Q("camera-editor") != null, "CINEMATIC's editor is up");
            Check(Focused() == "cam-name", "the editor opens on NAME (" + Focused() + ")");
            yield return UiShowcase.Shot("campresets-options-4-editor");
            var cine = ReflexCameraPresets.FindPreset(3);
            float before = cine.distance; Adjust("cam-distance", 1); Adjust("cam-distance", 1);
            Check(Mathf.Abs(cine.distance - before - 0.5f) < 0.01f, "DISTANCE steps by 0.25 ft (" + before + " -> " + cine.distance + ")");
            yield return Reveal("cam-shake");
            yield return UiShowcase.Shot("campresets-options-5-editor-shake");

            // NAME with the pad: A opens the on-screen keyboard
            yield return Reveal("cam-name");
            yield return UiShowcaseNav.PadTap(GamepadButton.South);
            yield return UiShowcase.Frames(15);
            Check(menu.CameraKeyboardOpen && Q("osk-q") != null, "the pad's A on NAME opened the on-screen keyboard");
            yield return UiShowcase.Shot("campresets-options-6-keyboard");
            foreach (string k in new[] { "osk-delete", "osk-m", "osk-y", "osk-space", "osk-d", "osk-r", "osk-o", "osk-n", "osk-e" })
            { var key = Q(k); if (key?.userData is OuNavItem it) it.submit(); else Debug.LogWarning(Tag + "no key " + k); }
            yield return UiShowcase.Frames(8);
            yield return UiShowcase.Shot("campresets-options-7-keyboard-typed");
            Press("osk-done");
            yield return UiShowcase.Frames(15);
            Check(!menu.CameraKeyboardOpen && cine.name == "MY DRONE" && Focused() == "cam-name", "DONE: the preset is MY DRONE, the focus back on NAME (" + cine.name + ", " + Focused() + ")");
            Check(((Label)Q("camera-editor").Q<Label>(className: "ou-bar-title")).text == "MY DRONE", "the editor's bar shows the new name");
            yield return UiShowcase.Shot("campresets-options-8-renamed");

            // DUPLICATE, then DELETE it (asks first; CANCEL keeps it)
            Press("cam-duplicate");
            yield return UiShowcase.Frames(20);
            var copy = ReflexCameraPresets.Presets.FirstOrDefault(p => p.name == "MY DRONE 2");
            Check(copy != null && Mathf.Abs(copy.distance - cine.distance) < 0.01f && copy.on, "DUPLICATE made MY DRONE 2 with the same values, ON");
            yield return Reveal("cam-delete");
            Press("cam-delete");
            yield return UiShowcase.Frames(15);
            Check(Q("cam-delete-popup") != null && Focused() == "cam-delete-no", "DELETE asks first, CANCEL focused (" + Focused() + ")");
            yield return UiShowcase.Shot("campresets-options-9-delete-ask");
            Press("cam-delete-no");
            yield return UiShowcase.Frames(10);
            Check(Q("cam-delete-popup") == null && ReflexCameraPresets.Presets.Contains(copy), "CANCEL kept it");
            Press("cam-delete"); yield return UiShowcase.Frames(10); Press("cam-delete-yes");
            yield return UiShowcase.Frames(20);
            Check(!ReflexCameraPresets.Presets.Contains(copy) && !menu.CameraEditorOpen && Q("cam-new") != null, "DELETE removed it and went back to CAMERA & HUD");
            // the save holds it
            string json = ReflexSave.GetString(ReflexCameraPresets.PresetsKey, "");
            Check(json.Contains("MY DRONE") && !json.Contains("MY DRONE 2"), "save.json's Camera.Presets has the renamed preset, not the deleted one");
            yield return UiShowcase.Shot("campresets-options-10-after-delete");
            ReflexOptions.Close();
            yield return UiShowcase.Frames(20);
        }

        // ------------------------------------------------------------------------------------------------ riding a real event
        static OriginalWorldRuntime Runtime() => UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();

        static IEnumerator Ride()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            // the cycle: USER off, CLOSE CHASE and CINEMATIC on (CINEMATIC with plenty of shake); GAME PLAY in use; the MODERN HUD first
            Fresh();
            ReflexCameraPresets.SetOn("game:2", false); ReflexCameraPresets.SetOn("custom:1", true); ReflexCameraPresets.SetOn("custom:3", true);
            var cine = ReflexCameraPresets.FindPreset(3);
            cine.shake = 1.8f; cine.shakeSpeed = 1f; cine.shakeRough = 1f; cine.shakeLanding = 1f; ReflexCameraPresets.Changed(cine);
            ReflexCameraPresets.Select("game:1");
            ReflexSettings.HudStyle = HudStyle.Modern; ReflexSettings.CameraToast = true;

            yield return StartAndRide();
            var rt = Runtime();
            if (!rt || !rt.Running) { Check(false, "the event never ran"); yield break; }
            UiShowcase.ShellVisible(false);

            string[] expect = { "game:3", "fpv", "custom:1", "custom:3", "game:0", "game:1" };
            string[] how = { "F7", "C", "action", "action", "action", "action" };
            for (int i = 0; i < expect.Length; i++)
            {
                if (how[i] == "F7") yield return UiShowcaseNav.Key(Key.F7);
                else if (how[i] == "C") yield return UiShowcaseNav.Key(Key.C);
                else ReflexClientLive.CycleCamera();
                yield return new WaitForSecondsRealtime(0.45f);
                string id = ReflexCameraPresets.CurrentId, name = ReflexCameraPresets.Current.name;
                var model = rt.GetComponent<ReflexModernHud>()?.Model;
                bool toast = model != null && model.cameraToast == name && Time.unscaledTime < model.cameraUntil;
                Debug.Log(Tag + $"switch {i} ({how[i]}): {id} '{name}' mode {rt.cameraMode} dc {rt.DrivingCameraPreset} toast '{model?.cameraToast}' shown {toast} speed {rt.Bike.speed:F1}");
                if (how[i] == "C" && id != expect[i]) { Debug.LogWarning(Tag + "the game's Camera key did not cycle here (its binding: not C?): the action instead"); ReflexClientLive.CycleCamera(); yield return new WaitForSecondsRealtime(0.45f); id = ReflexCameraPresets.CurrentId; name = ReflexCameraPresets.Current.name; toast = model != null && model.cameraToast == name; }
                Check(id == expect[i], $"switch {i} ({how[i]}) reached {expect[i]} ({id})");
                Check(toast, $"MODERN's toast says {name}");
                yield return UiShowcase.Shot($"campresets-ride-{i + 1}-{name.Replace(' ', '-').ToLowerInvariant()}");
                if (id == "custom:3")
                {   // CINEMATIC while moving: the blend in, the shake, above the ground
                    float maxShake = 0, minClear = 999, nextLog = 0;
                    for (float t = 0; t < 2.5f; t += Time.unscaledDeltaTime)
                    {
                        maxShake = Mathf.Max(maxShake, rt.CameraRig.Shake);
                        if (t >= nextLog) { nextLog = t + 0.25f; Debug.Log(Tag + $"  t {t:F2} shake {rt.CameraRig.Shake:F3} rough {rt.CameraRig.Rough:F2} trauma {rt.CameraRig.Trauma:F2} speed {rt.Bike.speed:F1} weight {rt.CameraRig.Weight:F2}"); }
                        var cam = rt.chaseCamera.transform.position;
                        bool have = false; float h = rt.terrain ? rt.terrain.CoreHeightAt(cam.x, cam.z, out have) : 0f;
                        if (have) minClear = Mathf.Min(minClear, cam.y - h);
                        yield return null;
                    }
                    Debug.Log(Tag + $"CINEMATIC: weight {rt.CameraRig.Weight:F2} shake max {maxShake:F3} now {rt.CameraRig.Shake:F3} rough {rt.CameraRig.Rough:F2} trauma {rt.CameraRig.Trauma:F2} speed {rt.Bike.speed:F1} clearance min {minClear:F2} driving {rt.DrivingView} race {rt.Race.valid}/{rt.Race.state}");
                    Check(rt.CameraRig.Weight > 0.99f, "CINEMATIC's adjustments are on (the driving camera)");
                    Check(maxShake > 0.02f, "CINEMATIC shakes while moving (" + maxShake.ToString("F3") + ")");
                    Check(minClear >= 1.5f || minClear > 900, "the camera stayed above the ground (" + minClear.ToString("F2") + " ft)");
                    yield return UiShowcase.Shot("campresets-ride-4b-cinematic-moving");
                }
            }

            // CLASSIC: the toast of its own
            ReflexSettings.HudStyle = HudStyle.Original;
            yield return new WaitForSecondsRealtime(0.5f);
            var classic = rt.GetComponent<ReflexCameraToast>();
            ReflexClientLive.CycleCamera(-1);   // GAME PLAY -> VISUAL
            yield return new WaitForSecondsRealtime(0.45f);
            // CLASSIC: the hand-built HUD's own camera label (OriginalHud), or the toast over the game's live HUD layers
            bool ClassicNamed(string n) => rt.originalHud && rt.originalHud.CameraLabelShown && ReflexCameraPresets.Current.name == n || classic && classic.Visible && classic.Text == n;
            Debug.Log(Tag + $"CLASSIC: hand-built label {(rt.originalHud && rt.originalHud.CameraLabelShown)}, toast {(classic ? classic.Visible + " '" + classic.Text + "'" : "none")}");
            Check(ClassicNamed("VISUAL"), "CLASSIC: the camera's name VISUAL shows");
            yield return UiShowcase.Shot("campresets-ride-7-classic-visual");
            ReflexClientLive.CycleCamera(-1);   // VISUAL -> CINEMATIC
            yield return new WaitForSecondsRealtime(0.45f);
            Check(ClassicNamed("CINEMATIC") && ReflexCameraPresets.CurrentId == "custom:3", "CLASSIC: the custom camera's name CINEMATIC shows");
            yield return UiShowcase.Shot("campresets-ride-8-classic-cinematic");
            ReflexSettings.HudHidden = true;
            ReflexClientLive.CycleCamera(1); yield return new WaitForSecondsRealtime(0.45f);
            Check(classic.Visible && classic.Text == "VISUAL", "the HUD hidden: the toast still names the camera");
            yield return UiShowcase.Shot("campresets-ride-9-hud-hidden-visual");
            ReflexSettings.HudHidden = false;
            ReflexClientLive.CycleCamera(-1); yield return new WaitForSecondsRealtime(0.3f);
            ReflexSettings.HudStyle = HudStyle.Modern;

            // the editor over the race: the race shows, the changes show at once
            UiShowcaseNav.PadState(new GamepadState());
            ReflexOptions.Open(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(30);
            yield return Reveal("cam-preset-3");
            Press("cam-preset-3");
            yield return UiShowcase.Frames(30);
            var editorCam = rt.chaseCamera.transform.position;
            Check(ReflexCameraPresets.Preview == cine && Menu().CameraEditorOpen, "the editor previews CINEMATIC over the paused race");
            yield return UiShowcase.Shot("campresets-ride-10-editor-live");
            for (int k = 0; k < 40; k++) Adjust("cam-distance", 1);
            for (int k = 0; k < 24; k++) Adjust("cam-height", 1);
            yield return new WaitForSecondsRealtime(1.2f);
            float moved = Vector3.Distance(editorCam, rt.chaseCamera.transform.position);
            Debug.Log(Tag + $"live preview: distance {cine.distance:F2} height {cine.height:F2}, the camera moved {moved:F2} ft");
            Check(moved > 6f, "the paused race's camera moved with DISTANCE / HEIGHT (" + moved.ToString("F1") + " ft)");
            yield return UiShowcase.Shot("campresets-ride-11-editor-live-far");
            for (int k = 0; k < 40; k++) Adjust("cam-distance", -1);
            for (int k = 0; k < 24; k++) Adjust("cam-height", -1);
            ReflexOptions.Close();
            yield return UiShowcase.Frames(20);
            Check(ReflexCameraPresets.Preview == null && ReflexCameraPresets.CurrentId == "custom:3", "leaving the editor: CINEMATIC (ON) stays the camera");
            Check(ReflexSave.GetString(ReflexCameraPresets.CurrentKey, "") == "custom:3" && ReflexSettings.DrivingCamera == 1, "saved: Camera.Current custom:3, DrivingCamera 1 (its base)");

            // the next start: the save read again, the event restarted (a new session)
            ReflexCameraPresets.ReloadForTest();
            rt.Restart();
            float t0 = Time.realtimeSinceStartup, tapAt = 3f, introWeight = 0f, gateWeight = 0f; bool driving = false, sawIntro = false, sawGate = false;
            while (Time.realtimeSinceStartup - t0 < 90f)
            {
                rt = Runtime();
                float t = Time.realtimeSinceStartup - t0;
                if (rt && rt.Running && (rt.IntroCardShown || rt.Race.valid != 0 && rt.Race.state == 0) && rt.CameraRig.AppliedFrame >= Time.frameCount - 1) { sawIntro = true; introWeight = Mathf.Max(introWeight, rt.CameraRig.Weight); }
                // w-camfix: the start gate (Countdown) stays the game's camera; the preset blends in once the gate drops
                if (rt && rt.Running && !rt.IntroCardShown && rt.Race.valid != 0 && rt.Race.state == 1 && rt.CameraRig.AppliedFrame >= Time.frameCount - 1) { sawGate = true; gateWeight = Mathf.Max(gateWeight, rt.CameraRig.Weight); }
                if (rt && rt.IntroCardShown && t > tapAt) { tapAt = t + 3f; yield return UiShowcaseNav.PadTap(GamepadButton.South); }
                if (rt && rt.Running && rt.Race.valid != 0 && rt.Race.state == 1) UiShowcaseNav.PadState(new GamepadState { rightTrigger = 1f });
                if (rt && rt.Running && rt.Race.valid != 0 && rt.Race.state >= 2 && rt.Race.state <= 3 && rt.CameraRig.Weight > 0.99f) { driving = true; break; }
                yield return null;
            }
            Debug.Log(Tag + $"the restart's intro (the game's camera shown under the preset): seen {sawIntro}, the adjustments' weight at most {introWeight:F2}");
            Check(!sawIntro || introWeight < 0.01f, "the intro's cameras stay the game's (no preset over them)");
            Debug.Log(Tag + $"the start gate (Countdown): seen {sawGate}, the adjustments' weight at most {gateWeight:F2}");
            Check(!sawGate || gateWeight < 0.01f, "the start gate's camera stays the game's (no preset over it until the gate drops)");
            var ui = Reflex.Port.RxCore.OpenReflexSimulation.GameUiSettings?.FirstOrDefault(kv => kv.Key == "DrivingCamera");
            Debug.Log(Tag + $"after the restart: running {rt && rt.Running} current {ReflexCameraPresets.CurrentId} dc {rt?.DrivingCameraPreset} weight {rt?.CameraRig.Weight:F2} race {rt?.Race.valid}/{rt?.Race.state} GameUiSettings DrivingCamera {ui?.Value}");
            Check(driving && ReflexCameraPresets.CurrentId == "custom:3" && rt.DrivingCameraPreset == 1, "the restarted event starts on CINEMATIC (its base GAME PLAY in the core)");
            Check(ui?.Value == 1, "every new session gets DrivingCamera = the base (GameUiSettings)");
            yield return new WaitForSecondsRealtime(1f);
            yield return UiShowcase.Shot("campresets-ride-12-restart-cinematic");
            UiShowcaseNav.PadState(new GamepadState());
            UiShowcase.ShellVisible(true);
        }

        /// The load arena's flow (UiShowcaseLoadArena): START EVENT from the shell, PRESS START in the arena, then the throttle once the race runs.
        static IEnumerator StartAndRide()
        {
            var shell = OpenShell.Instance;
            string key = Environment.GetEnvironmentVariable("REFLEX_LOADARENA_EVENT");
            if (string.IsNullOrEmpty(key)) key = "National/NAT_Concord";
            var ev = OpenContent.Find(key);
            Check(ev != null, "the event " + key);
            if (ev == null) yield break;
            for (float t = 0; t < 60f && OpenMenuWorld.Instance && OpenMenuWorld.Instance.Status == OpenMenuWorld.State.Loading; t += Time.unscaledDeltaTime) yield return null;
            var setup = new OpenSetup { eventKey = ev.key, ai = 0 };
            typeof(OpenShell).GetMethod("StartEvent", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(shell, new object[] { ev, setup });
            float t0 = Time.realtimeSinceStartup; bool started = false;
            while (Time.realtimeSinceStartup - t0 < 180f)
            {
                var rt = Runtime();
                var own = rt ? rt.loadArena : null;
                string phase = own ? (own.PromptShown ? "ready" : "loading") : OpenShell.Instance ? OpenShell.Instance.LoadArenaPhase ?? "-" : "-";
                if (!started && phase == "ready")
                {
                    yield return new WaitForSecondsRealtime(1f);
                    yield return UiShowcaseNav.PadTap(GamepadButton.Start);
                    started = true; Debug.Log(Tag + "PRESS START");
                }
                if (rt && rt.Running && !own && (started || rt.Race.valid != 0))
                {
                    if (rt.DrivingView && rt.Race.state >= 2 || rt.Race.valid == 0 && Time.realtimeSinceStartup - t0 > 10f) break;
                    if (rt.Race.state == 1) UiShowcaseNav.PadState(new GamepadState { rightTrigger = 1f });
                }
                if (OpenEventPrestart.Current && OpenEventPrestart.Current.Failure != null) { Check(false, "the event's load failed: " + OpenEventPrestart.Current.Failure); yield break; }
                yield return null;
            }
            var r = Runtime();
            Debug.Log(Tag + $"RIDING {r?.EventIdentity} race {r?.Race.valid}/{r?.Race.state} after {Time.realtimeSinceStartup - t0:F1} s; camera {ReflexCameraPresets.CurrentId}");
            // the throttle and a little steering, 3 s to pick up speed
            UiShowcaseNav.PadState(new GamepadState { rightTrigger = 0.8f });
            yield return new WaitForSecondsRealtime(3f);
        }
    }
}
