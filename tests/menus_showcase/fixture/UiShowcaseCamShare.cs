// TEST ONLY: CAMERA SHARING (w-camshare: Options/CameraShareCodec.cs, Options/ReflexCameraShare.cs, Options/ReflexFpvShare.cs,
// Options/ReflexOptionsMenu.CameraShare.cs). The clipboard is the fixture's own string (ReflexCameraShare.ClipboardGet / Set): the user's is never touched.
//   "camshare"  OPTIONS > CAMERA & HUD: SHARING (PASTE SHARE CODE, IMPORT .JSON FILES, OPEN CAMERA FOLDER), the camera editor's SHARE (AUTHOR, COPY SHARE
//               CODE, EXPORT), a pasted camera (a new preset, ON, its editor on USE THIS CAMERA; USE makes it the camera), garbage pasted, FIRST-PERSON
//               VIEW's rows, FIRST-PERSON SETUPS (SAVE CURRENT AS NEW SETUP, a setup's page, NAME with the pad's keyboard, USE THIS SETUP, a friend's
//               code pasted, DELETE with its question), a .json in Cameras/Import, the pad's B back through the pages
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseCamShare
    {
        const string Tag = "[ui-showcase] [camshare] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("camshare", Run);

        /// Muted from the start (the hub's music too): the listener silent and paused while this page is wanted.
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Mute()
        {
            if ((Environment.GetEnvironmentVariable("REFLEX_UI_SHOWCASE") ?? "").IndexOf("camshare", StringComparison.Ordinal) < 0) return;
            AudioListener.volume = 0f; AudioListener.pause = true;
            var go = new GameObject("camshare mute TEST ONLY"); UnityEngine.Object.DontDestroyOnLoad(go); go.AddComponent<KeepMuted>();
        }
        sealed class KeepMuted : MonoBehaviour { void LateUpdate() { AudioListener.volume = 0f; AudioListener.pause = true; } }

        static string s_clip = "";
        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "camshare", what); if (ok) Debug.Log(Tag + "ok " + what); }
        static VisualElement Q(string name) => ReflexOptions.Root?.Q<VisualElement>(name);
        static bool Press(string name) { var v = Q(name); if (v?.userData is OuNavItem it && it.submit != null) { Debug.Log(Tag + "press " + name); it.submit(); return true; } Debug.LogWarning(Tag + "no row " + name); return false; }
        static string Text(string name) => Q(name) is Label l ? l.text : Q(name)?.Q<Label>()?.text;
        static bool Shown(string name) { var v = Q(name); return v != null && v.resolvedStyle.display != DisplayStyle.None; }
        static string Focused() => (ReflexOptions.Root?.panel?.focusController?.focusedElement as VisualElement)?.name ?? "(none)";
        static ReflexOptionsMenu Menu()
        {
            var host = UnityEngine.Object.FindAnyObjectByType<ReflexOptionsHost>();
            return host ? typeof(ReflexOptionsHost).GetField("menu", BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(host) as ReflexOptionsMenu : null;
        }
        static IEnumerator Reveal(string name)
        {
            var v = Q(name); if (v == null) { Debug.LogWarning(Tag + "no element " + name); yield break; }
            v.Focus(); v.GetFirstAncestorOfType<ScrollView>()?.ScrollTo(v);
            yield return UiShowcase.Frames(12);
        }
        static float Fpv(string key, float d) => ReflexSave.GetFloat("ReflexFpv." + key, d);

        static IEnumerator Run()
        {
            // a clean start: the starter presets, GAME PLAY, no setups, the default FPV settings, the fixture's clipboard
            ReflexCameraShare.ClipboardGet = () => s_clip;
            ReflexCameraShare.ClipboardSet = t => s_clip = t;
            ReflexSave.Delete(ReflexCameraPresets.PresetsKey); ReflexSave.Delete(ReflexCameraPresets.CurrentKey);
            ReflexCameraPresets.ReloadForTest();
            ReflexCameraPresets.Select("game:1");
            ReflexSettings.ResetOnboard();
            Check(ReflexCameraShare.Folder.StartsWith(ReflexSave.Folder, StringComparison.Ordinal) && ReflexSave.DirArg.Length > 0, "the library is in the run's scratch save (" + ReflexCameraShare.Folder + ")");
            if (Directory.Exists(ReflexCameraShare.Folder)) Directory.Delete(ReflexCameraShare.Folder, true);
            ReflexCameraShare.EnsureRegistered();
            Check(CameraShare.Section("chase") != null && CameraShare.Section("fpv") != null && CameraShare.Section("fpv").Params.Count == ReflexFpvShare.Params().Length && CameraShare.Section("fpv").Version == ReflexFpvShare.Version, "chase and fpv sections registered (fpv: the ReflexFpv.* keys, " + ReflexFpvShare.Params().Length + " settings, version " + ReflexFpvShare.Version + ")");

            var shell = OpenShell.Instance;
            if (shell != null) shell.ShowOptions(ReflexOptions.CameraHud); else ReflexOptions.Open(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(40);
            var menu = Menu();
            Check(menu != null && Q("cam-paste") != null && Q("cam-import") != null && Q("cam-folder") != null, "CAMERA & HUD has SHARING: PASTE SHARE CODE, IMPORT .JSON FILES, OPEN CAMERA FOLDER");
            yield return Reveal("cam-paste");
            yield return UiShowcase.Shot("camshare-1-sharing");

            // ------------------------------------------------------------------ the editor's SHARE
            Press("cam-preset-3");
            yield return UiShowcase.Frames(30);
            var cine = ReflexCameraPresets.FindPreset(3);
            Check(menu.CameraEditorOpen && Q("cam-author") != null && Q("cam-share") != null && Q("cam-export") != null, "the camera editor has SHARE: AUTHOR, COPY SHARE CODE, EXPORT .JSON FILE");
            // AUTHOR with the pad: A opens the keyboard
            yield return Reveal("cam-author");
            yield return UiShowcaseNav.PadTap(GamepadButton.South);
            yield return UiShowcase.Frames(15);
            Check(menu.CameraKeyboardOpen, "the pad's A on AUTHOR opened the on-screen keyboard");
            foreach (string k in new[] { "osk-s", "osk-t", "osk-e", "osk-e", "osk-l", "osk-e", "osk-r" }) { if (Q(k)?.userData is OuNavItem it) it.submit(); }
            yield return UiShowcase.Frames(6);
            yield return UiShowcase.Shot("camshare-2-author-keyboard");
            Press("osk-done");
            yield return UiShowcase.Frames(15);
            Check(cine.author == "STEELER" && !menu.CameraKeyboardOpen && Focused() == "cam-author", "AUTHOR typed with the pad: " + cine.author + " (focus " + Focused() + ")");
            cine.distance = 3.75f; cine.shake = 1.35f; ReflexCameraPresets.Touch(cine);
            yield return Reveal("cam-share");
            Press("cam-share");
            yield return UiShowcase.Frames(8);
            var copied = CameraConfig.FromShareText(s_clip, out string cerr);
            Check(s_clip.StartsWith(CameraConfig.SharePrefix, StringComparison.Ordinal) && copied != null && copied.name == "CINEMATIC" && copied.author == "STEELER" &&
                  CameraConfig.SameValues(copied.Section("chase").Values, CameraShare.Section("chase").Complete(ReflexCameraShare.ChaseValues(cine), out _, out _)),
                  "COPY SHARE CODE put CINEMATIC's code on the clipboard (" + s_clip.Length + " chars" + (cerr != null ? ", " + cerr : "") + ")");
            Check(Shown("cam-share-status") && Text("cam-share-status").StartsWith("SHARE CODE COPIED", StringComparison.Ordinal), "the status line says so: " + Text("cam-share-status"));
            Debug.Log(Tag + "code: " + s_clip);
            yield return UiShowcase.Shot("camshare-3-editor-share");
            Press("cam-export");
            yield return UiShowcase.Frames(5);
            Check(File.Exists(Path.Combine(ReflexCameraShare.ExportFolder, "cinematic.json")), "EXPORT wrote Cameras/Export/cinematic.json (" + Text("cam-share-status") + ")");

            // B back to CAMERA & HUD, then PASTE: a new preset, its editor on USE THIS CAMERA
            yield return UiShowcaseNav.PadTap(GamepadButton.East);
            yield return UiShowcase.Frames(20);
            Check(!menu.CameraEditorOpen && Q("cam-paste") != null, "the pad's B left the editor");
            int before = ReflexCameraPresets.Presets.Count;
            Press("cam-paste");
            yield return UiShowcase.Frames(30);
            var pasted = ReflexCameraPresets.Presets.LastOrDefault();
            Check(ReflexCameraPresets.Presets.Count == before + 1 && pasted.name == "CINEMATIC 2" && pasted.on && pasted.author == "STEELER" && Mathf.Abs(pasted.distance - 3.75f) < 1e-6f && Mathf.Abs(pasted.shake - 1.35f) < 1e-6f,
                  "PASTE made CINEMATIC 2 (ON, by STEELER, its values)");
            Check(menu.CameraEditorOpen && Focused() == "cam-use" && Shown("cam-share-status") && Text("cam-share-status").StartsWith("PASTED CAMERA CINEMATIC 2", StringComparison.Ordinal),
                  "its editor opened on USE THIS CAMERA, saying " + Text("cam-share-status"));
            yield return UiShowcase.Shot("camshare-4-pasted-camera");
            Press("cam-use");
            yield return UiShowcase.Frames(10);
            Check(ReflexCameraPresets.CurrentId == pasted.Id && ReflexSettings.DrivingCamera == pasted.baseView, "USE THIS CAMERA: the pasted camera is the camera (" + ReflexCameraPresets.CurrentId + ")");
            yield return UiShowcaseNav.PadTap(GamepadButton.East);
            yield return UiShowcase.Frames(20);

            // garbage
            s_clip = "check out my camera lol ORCAM1:Zm9vYmFy";
            Press("cam-paste");
            yield return UiShowcase.Frames(10);
            Check(!menu.CameraEditorOpen && Shown("cam-paste-status") && Text("cam-paste-status").StartsWith("NOT A VALID CAMERA CONFIG", StringComparison.Ordinal) && ReflexCameraPresets.Presets.Count == before + 1,
                  "a damaged code is refused on the page: " + Text("cam-paste-status"));
            yield return Reveal("cam-paste");
            yield return UiShowcase.Shot("camshare-5-paste-refused");
            s_clip = "";
            Press("cam-paste"); yield return UiShowcase.Frames(5);
            Check(Text("cam-paste-status").StartsWith("NO CAMERA SHARE CODE FOUND", StringComparison.Ordinal), "an empty clipboard: " + Text("cam-paste-status"));

            // a .json file in Cameras/Import
            Directory.CreateDirectory(ReflexCameraShare.ImportFolder);
            var drone = new CameraConfig { name = "Drone Cam!", author = "a friend" }; drone.Put("chase", new Dictionary<string, double> { ["height"] = 12, ["distance"] = 20, ["pitch"] = 18 });
            File.WriteAllText(Path.Combine(ReflexCameraShare.ImportFolder, "drone.json"), drone.ToJson());
            File.WriteAllText(Path.Combine(ReflexCameraShare.ImportFolder, "junk.json"), "{ not json");
            Press("cam-import");
            yield return UiShowcase.Frames(20);
            var dp = ReflexCameraPresets.Presets.FirstOrDefault(p => p.name == "DRONE CAM");
            Check(dp != null && Mathf.Abs(dp.height - 12) < 1e-6f && Text("cam-paste-status") != null && Text("cam-paste-status").StartsWith("IMPORTED 1 CAMERA(S), 0 FPV SETUP(S), REFUSED 1", StringComparison.Ordinal),
                  "IMPORT .JSON FILES: DRONE CAM added, junk.json refused (" + Text("cam-paste-status") + ")");
            yield return UiShowcase.Shot("camshare-6-imported-files");

            // ------------------------------------------------------------------ FIRST-PERSON VIEW
            ReflexSave.SetFloat("ReflexFpv.fov", 100f); ReflexSave.SetInt("ReflexFpv.ca", 2); ReflexSave.SetFloat("ReflexFpv.offY", 0.25f); ReflexClientLive.FpvChanged();
            menu.ShowOptions(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(20);
            Check(Q("fpv-setups") != null && Q("fpv-share") != null && Q("fpv-paste") != null, "FIRST-PERSON VIEW has FIRST-PERSON SETUPS, COPY and PASTE SHARE CODE");
            Check(Q("fpv-setups")?.Q<Label>(className: "mxa-value")?.text == "NONE SAVED", "FIRST-PERSON SETUPS says NONE SAVED");
            yield return Reveal("fpv-share");
            Press("fpv-share");
            yield return UiShowcase.Frames(5);
            var fc = CameraConfig.FromShareText(s_clip, out _);
            Check(fc != null && fc.Has("fpv") && !fc.Has("chase") && fc.Section("fpv").Get("fov", 0) == 100 && fc.Section("fpv").Get("ca", 0) == 2 && Mathf.Abs((float)fc.Section("fpv").Get("offY", 0) - 0.25f) < 1e-6f,
                  "COPY SHARE CODE carries the FPV settings in use (fov 100, ca LEAN, eye up 0.25)");
            yield return UiShowcase.Shot("camshare-7-fpv-rows");

            Press("fpv-setups");
            yield return UiShowcase.Frames(20);
            Check(menu.FpvSetupsOpen && Focused() == "fpv-save-new", "FIRST-PERSON SETUPS opened on SAVE CURRENT AS NEW SETUP (" + Focused() + ")");
            yield return UiShowcase.Shot("camshare-8-setups-empty");
            Press("fpv-save-new");
            yield return UiShowcase.Frames(20);
            var saved = ReflexCameraShare.Setups();
            Check(saved.Count == 1 && saved[0].name == "FPV SETUP" && File.Exists(Path.Combine(ReflexCameraShare.Folder, saved[0].id + ".json")) && menu.FpvSetupOpen && Focused() == "fpv-name",
                  "SAVE CURRENT AS NEW SETUP wrote Cameras/" + (saved.Count > 0 ? saved[0].id : "?") + ".json and opened it on NAME");
            Check(Text("fpv-use") == "IN USE", "the new setup is IN USE");
            // NAME with the pad's keyboard
            yield return UiShowcaseNav.PadTap(GamepadButton.South);
            yield return UiShowcase.Frames(15);
            Check(menu.CameraKeyboardOpen, "the pad's A on NAME opened the keyboard");
            foreach (string k in new[] { "osk-delete", "osk-r", "osk-o", "osk-o", "osk-s", "osk-t", "osk-space", "osk-c", "osk-a", "osk-m" }) { if (Q(k)?.userData is OuNavItem it) it.submit(); }
            Press("osk-done");
            yield return UiShowcase.Frames(15);
            var mine = ReflexCameraShare.Setups().FirstOrDefault();
            Check(mine != null && mine.name == "ROOST CAM" && mine.id == saved[0].id && Text("fpv-setup-page") != null, "renamed ROOST CAM (the same file " + mine?.id + ".json)");
            yield return UiShowcase.Shot("camshare-9-setup");

            // other settings, then USE THIS SETUP brings them back
            ReflexSave.SetFloat("ReflexFpv.fov", 80f); ReflexSave.SetInt("ReflexFpv.ca", 0); ReflexClientLive.FpvChanged();
            menu.ShowFpvSetup(mine.id, "fpv-use");
            yield return UiShowcase.Frames(15);
            Check(Text("fpv-use") == "USE THIS SETUP", "the settings changed: USE THIS SETUP offered");
            Press("fpv-use");
            yield return UiShowcase.Frames(15);
            Check(Fpv("fov", 0) == 100f && ReflexSave.GetInt("ReflexFpv.ca", 0) == 2 && Text("fpv-use") == "IN USE" && Focused() == "fpv-use", "USE THIS SETUP: fov 100, ca LEAN again, IN USE (" + Text("fpv-setup-status") + ")");

            // a friend's code
            var friend = new CameraConfig { name = "Low And Wide", author = "STEELER" };
            var fv = CameraShare.Section("fpv").Defaults(); fv["fov"] = 130; fv["mode"] = 2; fv["free"] = 1; fv["tilt"] = 0.6;
            friend.Put("fpv", fv);
            s_clip = "try this: " + friend.ShareCode();
            yield return UiShowcaseNav.PadTap(GamepadButton.East);
            yield return UiShowcase.Frames(20);
            Check(menu.FpvSetupsOpen && Focused() == "fpv-setup-" + mine.id, "the pad's B went back to the list, on ROOST CAM (" + Focused() + ")");
            Press("fpv-list-paste");
            yield return UiShowcase.Frames(20);
            var low = ReflexCameraShare.Setups().FirstOrDefault(s => s.name == "LOW AND WIDE");
            Check(low != null && low.author == "STEELER" && low.Section("fpv").Get("fov", 0) == 130 && menu.FpvSetupOpen && Focused() == "fpv-use" && Fpv("fov", 0) == 100f,
                  "PASTE: LOW AND WIDE saved, its page on USE THIS SETUP, nothing applied yet (" + Text("fpv-setup-status") + ")");
            yield return UiShowcase.Shot("camshare-10-pasted-fpv");
            Press("fpv-use");
            yield return UiShowcase.Frames(10);
            Check(Fpv("fov", 0) == 130f && ReflexSave.GetInt("ReflexFpv.mode", 1) == 2 && ReflexSave.GetInt("ReflexFpv.free", 0) == 1 && Mathf.Abs(Fpv("tilt", 0) - 0.6f) < 1e-6f && Fpv("offY", 9) == 0f,
                  "USE: the friend's values (fov 130, TRUE, free look, tilt 0.6) and the keys it did not set at their defaults (eye up 0)");
            yield return UiShowcaseNav.PadTap(GamepadButton.East);
            yield return UiShowcase.Frames(20);
            Check(Q("fpv-setup-" + low.id)?.Q<Label>(className: "mxa-value")?.text == "IN USE" && Q("fpv-setup-" + mine.id)?.Q<Label>(className: "mxa-value")?.text != "IN USE", "the list marks LOW AND WIDE IN USE");
            // the pad moves through the list
            string f0 = Focused();
            yield return UiShowcaseNav.PadTap(GamepadButton.DpadDown);
            string f1 = Focused();
            yield return UiShowcaseNav.PadTap(GamepadButton.DpadDown);
            Check(f0 != f1 && Focused() != f1, "the d-pad moves the focus (" + f0 + " > " + f1 + " > " + Focused() + ")");
            yield return Reveal("fpv-setup-" + low.id);
            yield return UiShowcase.Shot("camshare-11-setups-list");

            // SAVE CURRENT SETTINGS INTO IT asks for a second press; DELETE asks first
            menu.ShowFpvSetup(mine.id, "fpv-overwrite");
            yield return UiShowcase.Frames(10);
            Press("fpv-overwrite"); yield return UiShowcase.Frames(8);
            Check(Text("fpv-overwrite") == "PRESS AGAIN TO REPLACE ITS VALUES" && ReflexCameraShare.FindSetup(mine.id).Section("fpv").Get("fov", 0) == 100, "SAVE CURRENT SETTINGS INTO IT: the first press only arms it");
            Press("fpv-overwrite"); yield return UiShowcase.Frames(8);
            Check(ReflexCameraShare.FindSetup(mine.id).Section("fpv").Get("fov", 0) == 130, "the second press saved the settings in use into ROOST CAM");
            yield return Reveal("fpv-delete");
            Press("fpv-delete");
            yield return UiShowcase.Frames(12);
            Check(Q("fpv-delete-popup") != null && Focused() == "fpv-delete-popup-no", "DELETE asks first, CANCEL focused (" + Focused() + ")");
            yield return UiShowcase.Shot("camshare-12-delete-ask");
            Press("fpv-delete-popup-yes");
            yield return UiShowcase.Frames(20);
            Check(ReflexCameraShare.FindSetup(mine.id) == null && !File.Exists(Path.Combine(ReflexCameraShare.Folder, mine.id + ".json")) && menu.FpvSetupsOpen && ReflexCameraShare.Setups().Count == 1,
                  "DELETE removed ROOST CAM's file and went back to the list");
            yield return UiShowcaseNav.PadTap(GamepadButton.East);
            yield return UiShowcase.Frames(20);
            Check(!menu.FpvSetupsOpen && Focused() == "fpv-setups" && Q("fpv-setups")?.Q<Label>(className: "mxa-value")?.text == "LOW AND WIDE  ·  IN USE", "B back to CAMERA & HUD on FIRST-PERSON SETUPS (" + Q("fpv-setups")?.Q<Label>(className: "mxa-value")?.text + ")");
            yield return UiShowcase.Shot("camshare-13-back");

            ReflexOptions.Close();
            yield return UiShowcase.Frames(20);
            ReflexSettings.ResetOnboard();
            ReflexCameraShare.ClipboardGet = () => GUIUtility.systemCopyBuffer; ReflexCameraShare.ClipboardSet = t => GUIUtility.systemCopyBuffer = t;
        }
    }
}
