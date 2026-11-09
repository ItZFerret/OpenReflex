// TEST ONLY (w-fpvfix; w-fpvrevert: the restored camera's rows): OPTIONS > CAMERA & HUD > FIRST-PERSON VIEW. The page's status line (which camera is
// on screen: these rows drive DYNAMIC FPV only), USE DYNAMIC FPV, the old camera's rows (TILT, PITCH, ...) and w-fpvrevert's (HEAD BOB, HEAD SMOOTHING,
// VIEW SMOOTHING), the rows the HEAD MODE does not use (greyed, the reason in their value), a row's change reaching its ReflexFpv.* key (what the
// camera reads), 0 on HEAD BOB / HEAD SHIFT STRENGTH, RESET. Page "fpvpage" (run.py --pages fpvpage); results "[ui-showcase] [fpvpage] ok ...".
using System;
using System.Collections;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseFpvPage
    {
        const string Tag = "[ui-showcase] [fpvpage] ";
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("fpvpage", Run);

        static void Check(bool ok, string what) { UiShowcaseNav.Check(ok, "fpvpage", what); if (ok) Debug.Log(Tag + "ok " + what); }
        static VisualElement Q(string name) => ReflexOptions.Root?.Q<VisualElement>(name);
        static bool Press(string name) { var v = Q(name); if (v?.userData is OuNavItem it && it.submit != null) { it.submit(); return true; } Debug.LogWarning(Tag + "no row " + name); return false; }
        static bool Adjust(string name, int d) { var v = Q(name); if (v?.userData is OuNavItem it && it.adjust != null) { it.adjust(d); return true; } Debug.LogWarning(Tag + "no value row " + name); return false; }
        static string Value(string name) => Q(name)?.Q<Label>(className: "mxa-value")?.text ?? "";
        static string Label(string name) => Q(name)?.Q<Label>(className: "mxa-row-label")?.text ?? "";
        static string Text(string name) => Q(name) is Label l ? l.text : "";
        static bool Shown(string name) { var v = Q(name); return v != null && v.resolvedStyle.display != DisplayStyle.None; }
        static float Opacity(string name) => Q(name)?.resolvedStyle.opacity ?? -1f;
        static float Key(string k, float d) => ReflexSave.GetFloat("ReflexFpv." + k, d);
        // the page's values, greying and status refresh every 250 ms (schedule.Every): a batch player's frames can be much shorter than that
        static IEnumerator Settle() { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < 0.6f) yield return null; yield return UiShowcase.Frames(4); }
        static IEnumerator Reveal(string name)
        {
            var v = Q(name); if (v == null) { Debug.LogWarning(Tag + "no element " + name); yield break; }
            v.Focus(); v.GetFirstAncestorOfType<ScrollView>()?.ScrollTo(v);
            yield return UiShowcase.Frames(12);
        }

        static IEnumerator Run()
        {
            AudioListener.volume = 0f;
            ReflexSettings.ResetOnboard();
            ReflexCameraPresets.Select("game:1");
            var shell = OpenShell.Instance;
            if (shell != null) shell.ShowOptions(ReflexOptions.CameraHud); else ReflexOptions.Open(ReflexOptions.CameraHud);
            yield return UiShowcase.Frames(40);
            Check(Q("fpv-status") != null && Text("fpv-status").StartsWith("These settings drive DYNAMIC FPV. Your camera: GAME PLAY", StringComparison.Ordinal) && Text("fpv-status").Contains("pick DYNAMIC FPV"),
                  "the status line on GAME PLAY: " + Text("fpv-status"));
            Check(Shown("fpv-use"), "USE DYNAMIC FPV is offered");
            Check(Label("fpv-tilt") == "TILT" && Value("fpv-tilt") == "0%  (LEVEL)" && Label("fpv-pitch") == "PITCH" && Value("fpv-pitch").StartsWith("0.31  (21.2° down)", StringComparison.Ordinal),
                  $"the old camera's defaults: TILT '{Value("fpv-tilt")}', PITCH '{Value("fpv-pitch")}', FIELD OF VIEW '{Value("fpv-fov")}'");
            Check(Value("fpv-fov") == "116°" && Value("fpv-ca") == "OFF" && Value("fpv-headbob") == "100%" && Value("fpv-headsmooth").EndsWith(" S", StringComparison.Ordinal) && Q("fpv-viewsmooth") != null,
                  $"FIELD OF VIEW '{Value("fpv-fov")}', CORNER ANTICIPATION '{Value("fpv-ca")}', HEAD BOB '{Value("fpv-headbob")}', HEAD SMOOTHING '{Value("fpv-headsmooth")}', VIEW SMOOTHING '{Value("fpv-viewsmooth")}'");
            Check(Q("fpv-feel") == null && Q("fpv-advanced") == null && Q("fpv-horizon") == null && Q("fpv-motion") == null, "no FEEL, ADVANCED, HORIZON LOCK or HEAD MOVEMENT row (the rewrite's)");
            yield return Reveal("fpv-status");
            yield return UiShowcase.Shot("fpvpage-1-gameplay");

            ReflexCameraPresets.Select("game:3");
            yield return Settle();
            Check(Text("fpv-status").Contains("1ST PERSON") && Text("fpv-status").Contains("the game's own first-person camera"), "the status line on 1ST PERSON: " + Text("fpv-status"));

            // a row's change reaches the key the camera reads
            Adjust("fpv-tilt", 1); Adjust("fpv-tilt", 1);
            yield return UiShowcase.Frames(4);
            Check(Mathf.Abs(Key("tilt", -1) - 0.1f) < 1e-4f && Value("fpv-tilt") == "10%", $"TILT's right arrow twice: ReflexFpv.tilt {Key("tilt", -1):0.00}, '{Value("fpv-tilt")}'");
            for (int i = 0; i < 25; i++) Adjust("fpv-tilt", 1);
            yield return UiShowcase.Frames(4);
            Check(Key("tilt", -1) == 1f && Value("fpv-tilt") == "100%  (WITH THE BIKE)", "TILT goes up to 100% (" + Value("fpv-tilt") + ")");
            Adjust("fpv-pitch", 5);
            Check(Mathf.Abs(Key("pitch", -9) - 0.36f) < 1e-4f && Value("fpv-pitch").StartsWith("0.36  (22.2° down)", StringComparison.Ordinal), "PITCH +5 steps: " + Value("fpv-pitch"));
            for (int i = 0; i < 25; i++) Adjust("fpv-headbob", -1);
            Check(Key("headBob", -1) == 0f && Value("fpv-headbob") == "0%", "HEAD BOB goes down to 0% (" + Value("fpv-headbob") + ")");
            for (int i = 0; i < 40; i++) Adjust("fpv-shiftstrength", -1);
            Check(Key("shiftLong", -1) == 0f && Key("shiftLat", -1) == 0f && Key("shiftVert", -1) == 0f && Value("fpv-shiftstrength") == "0.00", "HEAD SHIFT STRENGTH goes down to 0 (all three gains): " + Value("fpv-shiftstrength"));
            yield return Reveal("fpv-headbob");
            yield return UiShowcase.Shot("fpvpage-2-rows");

            // the HEAD MODE greys what it does not use
            Adjust("fpv-mode", 1);   // DYNAMIC -> TRUE
            yield return Settle();
            Check(ReflexSettings.OnboardHeadMode == 2 && Value("fpv-tilt").Contains("NOT USED IN TRUE") && Value("fpv-headbob").Contains("NOT USED IN TRUE") && !Value("fpv-truesmooth").Contains("·") && Opacity("fpv-tilt") < 0.6f,
                  $"TRUE: TILT '{Value("fpv-tilt")}', HEAD BOB '{Value("fpv-headbob")}', TRUE SMOOTHING '{Value("fpv-truesmooth")}' (opacity {Opacity("fpv-tilt"):0.00})");
            Adjust("fpv-mode", 1);   // TRUE -> CHASSIS
            yield return Settle();
            Check(ReflexSettings.OnboardHeadMode == 0 && Value("fpv-headbob").Contains("OFF IN CHASSIS") && !Value("fpv-tilt").Contains("·") && Value("fpv-truesmooth").Contains("TRUE ONLY"),
                  $"CHASSIS: HEAD BOB '{Value("fpv-headbob")}', TILT '{Value("fpv-tilt")}', TRUE SMOOTHING '{Value("fpv-truesmooth")}'");
            Adjust("fpv-mode", 1);   // CHASSIS -> DYNAMIC
            yield return Settle();
            Check(ReflexSettings.OnboardHeadMode == 1 && !Value("fpv-headbob").Contains("·") && Opacity("fpv-headbob") > 0.9f && Value("fpv-trueroll").Contains("TRUE ONLY"), "DYNAMIC: the main rows apply, the TRUE rows say TRUE ONLY");

            Press("fpv-use");
            yield return Settle();
            Check(ReflexSettings.CameraMode == 1 && ReflexCameraPresets.CurrentId == "fpv" && !Shown("fpv-use") && Text("fpv-status").Contains("Your camera: DYNAMIC FPV"),
                  "USE DYNAMIC FPV: the camera is DYNAMIC FPV, the button hides, the status: " + Text("fpv-status"));
            yield return Reveal("fpv-status");
            yield return UiShowcase.Shot("fpvpage-3-dynamic");

            Press("fpv-reset");
            yield return UiShowcase.Frames(30);
            Check(!ReflexSave.Has("ReflexFpv.tilt") && !ReflexSave.Has("ReflexFpv.headBob") && Value("fpv-tilt") == "0%  (LEVEL)" && Value("fpv-headbob") == "100%" && Value("fpv-shiftstrength") == "1.00",
                  $"RESET TO DEFAULTS: the keys deleted, TILT '{Value("fpv-tilt")}', HEAD BOB '{Value("fpv-headbob")}', HEAD SHIFT STRENGTH '{Value("fpv-shiftstrength")}'");
            ReflexSettings.ResetOnboard();
        }
    }
}
