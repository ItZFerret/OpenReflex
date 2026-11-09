// Diagnostics of the MotoSkills tutorials (w-tutorials), off in a normal start: REFLEX_TUTORIAL_SCRIPT=<lessons> plays the lessons in turn through
// the real flow with a virtual keyboard: the hub's MOTOSKILLS page, Enter on the lesson's row (START), Enter on the PreTutorialPopup (CONTINUE),
// the lesson ridden with its keys until the PostTutorialPopup (or the FailedTutorialPopup: RETRY once), Enter (CONTINUE) to the lesson list in
// the world, the next lesson; at the end QUIT MOTOSKILLS and the MOTOSKILLS page again, which must show the lessons completed.
//   REFLEX_TUTORIAL_SCRIPT=1,2     the lessons (1..6); 1 RIDER REFLEX: the lean forward, back, left, right; 2 HOLD TIGHT: W, the lean back from 1 s on
//   REFLEX_TUTORIAL_SHOTS=<dir>    screenshots (hub, motoskills, pre-<n>, hud-<n>, ride-<n>-<k>, post-<n>, menu-<n>, back)
// The result lines are "[TutorialProbe] ..." in the player log; the player quits at the end. tests/menu3d_motion/run_player.py --env passes them.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using IKey = UnityEngine.InputSystem.Key;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OpenTutorialProbe : MonoBehaviour
    {
        static string Script => Environment.GetEnvironmentVariable("REFLEX_TUTORIAL_SCRIPT");
        string shots; Keyboard kb; int fails;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (string.IsNullOrEmpty(Script)) return;
            OpenShell.SkipIntro = true;
            var g = new GameObject("tutorial probe (diagnostics)"); DontDestroyOnLoad(g); g.AddComponent<OpenTutorialProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_TUTORIAL_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            StartCoroutine(Run());
        }

        void Log(string s) => Debug.Log("[TutorialProbe] " + s);
        void Shot(string name) { if (shots != null) ScreenCapture.CaptureScreenshot(Path.Combine(shots, name + ".png")); Log("shot " + name); }
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        IEnumerator Key(params Key[] keys)
        {
            kb ??= InputSystem.AddDevice<Keyboard>("tutorial probe keyboard");
            InputSystem.QueueStateEvent(kb, new KeyboardState(keys));
            yield return Frames(3);
            InputSystem.QueueStateEvent(kb, new KeyboardState());
            yield return Frames(3);
        }
        void Hold(params Key[] keys)
        {
            kb ??= InputSystem.AddDevice<Keyboard>("tutorial probe keyboard");
            InputSystem.QueueStateEvent(kb, new KeyboardState(keys));
        }
        void HoldSet(IEnumerable<IKey> keys)
        {
            kb ??= InputSystem.AddDevice<Keyboard>("tutorial probe keyboard");
            InputSystem.QueueStateEvent(kb, new KeyboardState(keys.ToArray()));
        }
        bool Check(bool ok, string what) { Log((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; return ok; }

        static OwnTutorial Tut() { var rt = FindAnyObjectByType<OriginalWorldRuntime>(); return rt ? rt.tutorial : null; }
        /// Enter on an in-world page; if the page did not change (the key did not reach it), the focused item's own submit.
        IEnumerator Accept(OwnTutorial tu)
        {
            var before = tu ? tu.Current : OwnTutorial.Page.None;
            yield return Key(UnityEngine.InputSystem.Key.Enter);
            yield return Frames(10);
            if (tu && tu.Current == before && before != OwnTutorial.Page.Hud && before != OwnTutorial.Page.None) { Log("Enter did not reach the page " + before + ": its focused item submitted"); tu.SubmitFocused(); yield return Frames(5); }
        }
        IEnumerator WaitPage(OwnTutorial.Page p, float seconds)
        {
            for (float t = 0; t < seconds; t += Time.unscaledDeltaTime) { var tu = Tut(); if (tu && tu.Current == p) yield break; yield return null; }
        }
        IEnumerator WaitOutcome(float seconds)
        {
            for (float t = 0; t < seconds; t += Time.unscaledDeltaTime)
            { var tu = Tut(); if (tu && (tu.Current == OwnTutorial.Page.Post || tu.Current == OwnTutorial.Page.Failed)) yield break; yield return null; }
        }

        /// REFLEX_TUTORIAL_SCRIPT=event:<key>: launches a Free Ride challenge from the hub as the front end would (TargetPractice/FR_Alpine ...), rides toward the
        /// nearest target / chevron object and takes screenshots (the challenges' Template_AnimChevron_NoFadeCtrl look).
        IEnumerator RunEvent(string key)
        {
            var ev = ReflexEvents.Find(key);
            if (!Check(ev != null, $"the event {key} exists")) yield break;
            var setup = new OpenSetup { eventKey = ev.key };
            Log($"launching {ev.key}");
            ReflexEventSelect.Launch(ev.key, 0, false, "Reflex", 0, setup.Garage(ev));
            OriginalWorldRuntime rt = null; OriginalLevel lv = null;
            for (float t = 0; t < 120f; t += Time.unscaledDeltaTime)
            {
                rt = FindAnyObjectByType<OriginalWorldRuntime>(); lv = FindAnyObjectByType<OriginalLevel>();
                if (rt && rt.Running && lv && lv.Content != null && rt.Bike.renderPosition != null && rt.Bike.frames > 120) break;
                yield return null;
            }
            if (!Check(rt && rt.Running, "the challenge world runs")) yield break;
            yield return Seconds(4f);
            Shot("event-0");
            var targets = new List<(string name, Transform t, string type)>();
            foreach (var inst in lv.Content.instances)
            {
                if (!inst.transform) continue;
                bool chevron = inst.entityType == "CollectibleTargetPracticeRSData" || (inst.materials != null && inst.materials.Any(m => m != null && (m.Contains("ChevronOrange") || m.Contains("TargetPractice"))));
                if (!chevron) continue;
                targets.Add((inst.name, inst.transform, inst.entityType));
                var mr = inst.transform.GetComponent<Renderer>(); var m0 = mr && mr.sharedMaterials.Length > 0 ? mr.sharedMaterials[0] : null;
                var pp = inst.transform.position; var mpb = new MaterialPropertyBlock(); if (mr) mr.GetPropertyBlock(mpb);
                Log($"  {inst.entityType} {inst.name}: at ({pp.x:F1}, {pp.y:F1}, {pp.z:F1}) active {inst.transform.gameObject.activeSelf} material {(m0 ? m0.name : "-")} mode {(m0 && m0.HasProperty("_Mode") ? m0.GetFloat("_Mode") : -1)} blend {(m0 ? m0.GetFloat("_SrcBlend") + "/" + m0.GetFloat("_DstBlend") : "-")} scrollParams {mpb.GetVector("_ScrollParams")} override {mpb.GetVector("_RfxScrollOverride")}");
            }
            Log($"{targets.Count} chevron / target objects");
            var pilot = new TutorialPilot(rt, lv.Content, 0);
            // ride toward the nearest active target and look at it
            float t0 = Time.realtimeSinceStartup; int shots = 0; float nextShot = 3f;
            while (Time.realtimeSinceStartup - t0 < pilot.Param("limit", 60f))
            {
                var pose = pilot.Read(); pilot.keys.Clear();
                if (pose.valid)
                {
                    Transform best = null; float bd = 1e9f;
                    foreach (var tg in targets) if (tg.t && tg.t.gameObject.activeInHierarchy && tg.type == "CollectibleTargetPracticeRSData")
                    { float d = Vector3.Distance(tg.t.position, pose.pos); if (d < bd) { bd = d; best = tg.t; } }
                    if (best)
                    {
                        var to = new Vector2(best.position.x - pose.pos.x, best.position.z - pose.pos.z);
                        pilot.SteerTo(to.normalized, pose.fwd);
                        pilot.Speed(pose.speed, bd > 40f ? pilot.Param("v", 14f) : pilot.Param("vNear", 5f));
                        if (Time.realtimeSinceStartup - t0 > nextShot) { nextShot += pilot.Param("shotEvery", 3f); Shot($"event-{++shots}"); Log($"  t {Time.realtimeSinceStartup - t0:F1} nearest target {best.name} at {bd:F1}"); }
                    }
                }
                HoldSet(pilot.keys);
                yield return null;
            }
            Hold();
            Shot("event-end");
            Log("RESULT PASS (the challenge looked at)");
        }

        IEnumerator Run()
        {
            OpenShell shell = null;
            for (float t = 0; t < 90f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!Check(shell, "the shell started")) { Quit(); yield break; }
            for (float t = 0; t < 60f && shell.MenuWorld == null; t += Time.unscaledDeltaTime) yield return null;
            ReflexProfile.Open();
            yield return Seconds(2f);
            Shot("hub");
            yield return Seconds(1f);
            if (Script.StartsWith("event:", StringComparison.Ordinal)) { yield return RunEvent(Script.Substring(6)); Quit(); yield break; }
            var lessons = Script.Split(',').Select(s => int.TryParse(s.Trim(), out int v) ? v - 1 : -1).Where(v => v >= 0 && v < OpenTutorial.Lessons).ToList();
            int before = OpenTutorial.Completed;
            Log("TutorialsCompleted at the start: " + before);
            Log("opening the MOTOSKILLS page");
            try { shell.ShowMotoSkills(lessons.Count > 0 ? lessons[0] : 0); }
            catch (Exception e) { Log("ShowMotoSkills threw: " + e); }
            Log("the MOTOSKILLS page was built");
            yield return Seconds(1.5f);
            Check(shell.Current == OpenShell.Screen.MotoSkills, "the MOTOSKILLS page is up");
            Shot("motoskills");
            float t0 = Time.realtimeSinceStartup;
            if (lessons.Count > 0 && lessons[0] > OpenTutorial.Completed)
            {   // a lesson the profile has not unlocked (a look at its furniture: lesson 4's arrows): started directly, as the lesson list would after the ones before it
                Log($"lesson {lessons[0] + 1} is locked on this profile: started directly");
                OpenTutorial.Begin(lessons[0]);
            }
            else
            {
            yield return Key(UnityEngine.InputSystem.Key.Enter);   // the focused lesson's row: START
            yield return Frames(20);
            if (OpenTutorial.Current == null)
            {   // the shell's menu input was made before the probe's keyboard existed: the focused row's own submit (a click on it)
                var f = shell.Nav.Focused;
                Log("Enter did not reach the shell's page; the focused item " + f?.name + " submitted directly");
                (f?.userData as OuNavItem)?.submit?.Invoke();
            }
            }
            for (float t = 0; t < 90f && OpenTutorial.Phase != "riding"; t += Time.unscaledDeltaTime) yield return null;
            if (!Check(OpenTutorial.Phase == "riding", $"riding {OpenTutorial.LastStartSeconds:F2} s after START")) { Quit(); yield break; }
            for (int k = 0; k < lessons.Count; k++)
            {
                int n = lessons[k];
                if (k > 0)
                {   // the lesson list in the world: the next lesson is focused after a pass
                    yield return WaitPage(OwnTutorial.Page.Menu, 10f);
                    Check(Tut() && Tut().Current == OwnTutorial.Page.Menu, "the lesson list (TutorialPauseMenu) is up");
                    yield return Seconds(1f);
                    Shot($"menu-{k}");
                    Tut()?.FocusItem("tutorial-lesson-" + (n + 1));
                    yield return Frames(5);
                    yield return Accept(Tut());
                }
                yield return WaitPage(OwnTutorial.Page.Pre, 15f);
                var tu = Tut();
                if (!Check(tu && tu.Current == OwnTutorial.Page.Pre, $"lesson {n + 1}: the PreTutorialPopup is up")) break;
                Log($"lesson {n + 1} pre: '{tu.PopupTitle}' / '{tu.PopupText}'");
                yield return Seconds(1.2f);
                Shot($"pre-{n + 1}");
                Log($"lesson {n + 1} pre video: {tu.VideoPath} frame {tu.VideoFrame} shown {tu.VideoHasFrame} failed {tu.VideoFailed}; crawl: text {tu.CrawlTextHeight:F0} px, offset {tu.CrawlOffset:F0}");
                yield return Seconds(4f);
                Shot($"pre-{n + 1}-b");
                Log($"lesson {n + 1} pre video: {tu.VideoPath} frame {tu.VideoFrame} shown {tu.VideoHasFrame} failed {tu.VideoFailed}; crawl: text {tu.CrawlTextHeight:F0} px, offset {tu.CrawlOffset:F0}");
                Check(tu.VideoHasFrame && tu.VideoFrame > 30, $"lesson {n + 1}: the PreTutorialPopup's video plays (frame {tu.VideoFrame})");
                yield return Accept(tu);   // CONTINUE (also: skips the video)
                yield return WaitPage(OwnTutorial.Page.Hud, 5f);
                Check(tu.Current == OwnTutorial.Page.Hud, $"lesson {n + 1}: the HUD is up ('{tu.HudLabel}' / '{tu.HudHint}')");
                Log($"lesson {n + 1} objects: {tu.ObjectsShown} of {tu.ObjectsTotal} shown: {tu.ObjectsShownNames}");
                {   // the lesson's chevron walls / arrows: their material (the transcribed Template_AnimChevron_Ctrl is _Mode 18) and the scroll the game gives them
                    var lv = FindAnyObjectByType<OriginalLevel>();
                    if (lv && lv.Content != null)
                        foreach (var inst in lv.Content.instances)
                        {
                            if (!inst.transform || inst.name == null || !inst.name.StartsWith($"TUT0{n + 1}_") || !inst.transform.gameObject.activeSelf) continue;
                            var mr = inst.transform.GetComponent<Renderer>(); if (!mr) continue;
                            var mpb = new MaterialPropertyBlock(); mr.GetPropertyBlock(mpb);
                            foreach (var m in mr.sharedMaterials)
                                Log($"  {inst.name}: material {(m ? m.name : "-")} shader {(m && m.shader ? m.shader.name : "-")} mode {(m && m.HasProperty("_Mode") ? m.GetFloat("_Mode") : -1)} blend {(m && m.HasProperty("_SrcBlend") ? m.GetFloat("_SrcBlend") + "/" + m.GetFloat("_DstBlend") : "-")} offset {mpb.GetVector("_RfxTextureOffset")}");
                        }
                }
                bool retried = false;
                for (int attempt = 0; attempt < 2; attempt++)
                {
                    yield return Seconds(0.6f);
                    Shot($"hud-{n + 1}" + (attempt > 0 ? "-retry" : ""));
                    float rt0 = Time.realtimeSinceStartup;
                    if (n == 0)
                    {   // RIDER REFLEX: the Rider Reflex stick in the four directions (the arrows)
                        foreach (var key in new[] { UnityEngine.InputSystem.Key.UpArrow, UnityEngine.InputSystem.Key.DownArrow, UnityEngine.InputSystem.Key.LeftArrow, UnityEngine.InputSystem.Key.RightArrow })
                        { Hold(key); yield return Seconds(0.9f); Hold(); yield return Seconds(0.3f); if (tu.Current != OwnTutorial.Page.Hud) break; }
                    }
                    else
                    {   // HOLD TIGHT (and the others): full throttle, the lean back from 1 s on, until the lesson's outcome
                        Hold(UnityEngine.InputSystem.Key.W); yield return Seconds(1f);
                        Hold(UnityEngine.InputSystem.Key.W, UnityEngine.InputSystem.Key.DownArrow);
                        for (int s = 0; s < 4 && tu.Current == OwnTutorial.Page.Hud; s++) { yield return Seconds(1.2f); Shot($"ride-{n + 1}-{s}" + (attempt > 0 ? "-retry" : "")); }
                    }
                    yield return WaitOutcome(25f);
                    Hold();
                    var st = tu.State;
                    Log($"lesson {n + 1}: {tu.Current} after {Time.realtimeSinceStartup - rt0:F1} s (hold {st?.holdTime:F2}, lean flags {st?.leanFlags}, wrecked {st?.wrecked}, boundary {st?.boundary})");
                    if (tu.Current == OwnTutorial.Page.Failed && !retried)
                    {
                        Log($"lesson {n + 1} failed: '{tu.PopupText}': RETRY");
                        Shot($"failed-{n + 1}");
                        retried = true;
                        yield return Seconds(1f);
                        yield return Accept(tu);   // RETRY
                        yield return Frames(30);
                        if (tu.Current == OwnTutorial.Page.Pre) { yield return Seconds(1f); yield return Accept(tu); }
                        yield return WaitPage(OwnTutorial.Page.Hud, 5f);
                        continue;
                    }
                    break;
                }
                if (!Check(tu.Current == OwnTutorial.Page.Post, $"lesson {n + 1}: PASSED (the PostTutorialPopup: '{tu.PopupText}')")) break;
                yield return Seconds(1.2f);
                Shot($"post-{n + 1}");
                yield return Accept(tu);   // CONTINUE: the lesson list
            }
            {   // QUIT MOTOSKILLS
                yield return WaitPage(OwnTutorial.Page.Menu, 10f);
                var tu = Tut();
                if (tu && tu.Current == OwnTutorial.Page.Menu)
                {
                    yield return Seconds(1f);
                    Shot("menu-end");
                    tu.FocusItem("tutorial-quit-motoskills");
                    yield return Frames(5);
                    yield return Accept(tu);
                }
            }
            for (float t = 0; t < 60f && OpenTutorial.Phase != "menu"; t += Time.unscaledDeltaTime) yield return null;
            yield return Seconds(2f);
            shell = OpenShell.Instance;
            Check(shell && shell.Current == OpenShell.Screen.MotoSkills, "back on the MOTOSKILLS page");
            ReflexProfile.Open();
            yield return Seconds(1f);
            int after = OpenTutorial.Completed;
            Check(after >= before + lessons.Count || after >= lessons.Max() + 1, $"TutorialsCompleted {before} -> {after} (saved in the profile)");
            if (shell && shell.Current == OpenShell.Screen.MotoSkills) shell.ShowMotoSkills(-1);
            yield return Seconds(1.5f);
            Shot("back");
            Log($"the run took {Time.realtimeSinceStartup - t0:F1} s");
            Log(fails == 0 ? "RESULT PASS" : $"RESULT FAIL ({fails})");
            yield return Frames(30);
            Quit();
        }

        void Quit()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}
