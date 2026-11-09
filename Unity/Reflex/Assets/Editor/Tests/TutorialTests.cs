using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // The MotoSkills tutorials (WP-T: Content/ReflexTutorials.cs, OriginalWorld/OriginalTutorial.cs, FrontEnd/FrontEndTutorials.cs, native
    // tutorial.cpp; _re/notes/tutorials.md).
    //   Verify:   edit mode. The tutorial event (ReflexEvents.Find), its scene section in UI_Compound.dx9.scene, the world's name; every
    //             tutorial layer built from the install by name (the live mirror's packages); the strings; Tutorial1..6.bik open through the
    //             install's binkw32.dll (frames, size); the tutorial voice-over of TutorialAudioMappings resolved to its FEV project.
    //   FrontEnd: edit mode. The guest front end: the main menu shows TutorialFirstTimeEntryPopup (TutorialsEntered FALSE), SKIP closes it,
    //             MOTOSKILLS (Misc) hands the tutorial event over (intercepted: nothing launched, no PlayerPrefs); the popup's START does too.
    //   Play:     play mode, the race scene on the tutorial (built from the install the first time): the game's UI attached in game, the
    //             intro popup (and its narration), Enter -> the Tutorial Pause Menu Page, RIDER REFLEX -> PreTutorialPopup with its video
    //             (Tutorial1.bik playing), Enter -> the lesson: the rider leaned forward / back / left / right (arrow keys) until the game's
    //             PostTutorialPopup, Enter -> the pause menu again (two lessons unlocked), the game's save written, QUIT MOTOSKILLS -> the
    //             game's Main Menu Page -> the port's front end; the save read back (TutorialsCompleted). $TUT_LESSON=n plays lesson n's
    //             start and its failure instead (n >= 2 needs n - 1 completed). Shots and a report to $TUT_SHOTS.
    // The user's PlayerPrefs keys are checked and restored; the bench's productName keeps its save apart.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.TutorialTests.Verify -quit / .FrontEnd -quit / .Play (no -quit)
    public static class TutorialTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static string outDir;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Tutorial] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static Dictionary<string, (bool has, string s, int i, float f)> SavePrefs() => UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        static void RestorePrefs()
        {
            if (saved == null) return;
            foreach (var kv in saved)
            {
                var (had, s, i, f) = kv.Value; bool now = PlayerPrefs.HasKey(kv.Key);
                bool changed = now != had || had && (PlayerPrefs.GetString(kv.Key, null) != s || PlayerPrefs.GetInt(kv.Key, int.MinValue) != i || !PlayerPrefs.GetFloat(kv.Key, float.NaN).Equals(f));
                if (!changed) continue;
                Log("FAIL a user PlayerPrefs key changed: " + kv.Key + " (restored)"); bad++;
                if (!had) PlayerPrefs.DeleteKey(kv.Key); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(kv.Key, s); else if (i != int.MinValue) PlayerPrefs.SetInt(kv.Key, i); else PlayerPrefs.SetFloat(kv.Key, f);
            }
            PlayerPrefs.Save();
        }
        static void Write(string name)
        {
            Log(name + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            Directory.CreateDirectory(outDir); File.WriteAllText(Path.Combine(outDir, name + ".txt"), report.ToString());
        }

        // ---------------------------------------------------------------------------------------------- Verify
        static readonly string[] Layers = { "TutorialIntroductionPopup", "PreTutorialPopup", "PostTutorialPopup", "FailedTutorialPopup", "TutorialConclusionPopup", "TutorialActivation", "TutorialPauseMenu" };
        public static void Verify()
        {
            report.Clear(); bad = 0; outDir = Env("TUT_SHOTS", Path.Combine(Application.temporaryCachePath, "tut")); saved = SavePrefs();
            try
            {
                var e = ReflexEvents.Find(ReflexTutorials.Key);
                Check(e != null && e == ReflexTutorials.Event && ReflexTutorials.IsTutorial(e), "the tutorial event: " + e?.key);
                Check(e.location == "UI_Compound" && e.sceneSection == "UI_CompoundFreeRide" && e.SceneActivity == "FreeRide" && e.spec.type == "Tutorial" && e.spec.activity == ReflexEvents.ActivityIndex("FreeRide"),
                      $"level {e.location}, section {e.sceneSection} (activity {e.SceneActivity}), spec type {e.spec.type} activity {e.spec.activity}");
                var sections = ReflexEvents.SceneSectionsOf(e.location);
                Check(sections.Contains(e.sceneSection), "UI_Compound.dx9.scene sections: " + string.Join(", ", sections));
                var files = ReflexWorldFiles.For(e.key);
                Check(files.name == "Tutorial_UI_Compound" && files.level == "UI_Compound", "world files: " + files.name + " (ready " + files.Ready + ")");
                Check(OriginalLiveUi.IsLive(e), "the tutorial world runs the game's UI live");
                // the tutorial layers as the live mirror finds them (the install's MXUI packages by layer name)
                foreach (var l in Layers.Concat(new[] { "TutorialFirstTimeEntryPopup" }))
                {
                    var (pkg, asset) = OriginalLiveUi.LayerPackages.Find(l);
                    Check(pkg != null, $"layer {l}: {pkg} / {asset}");
                }
                var data = new FeAssets(Shader.Find("Reflex/FrontEnd UI"));
                foreach (string k in new[] { "STR_TUTORIAL_POPUP_TITLE", "STR_TUTORIAL_POPUP_DESCRIPTION", "STR_TUTORIAL_POPUPBUTTON_ONE", "STR_TUTORIAL_POPUPBUTTON_TWO", "STR_TUTORIAL_INTRO_TITLE", "STR_TUTORIAL_CONCLUSION_TITLE", "STR_TUTORIALS" }
                         .Concat(Enumerable.Range(1, 6).SelectMany(i => new[] { ReflexTutorials.TitleKey(i), "STR_TUTORIAL_" + i + "_BRIEF_DESC", "STR_TUTORIAL_" + i + "_DESCRIPTION", "STR_TUTORIAL_" + i + "_COMPLETION" })))
                {
                    string s = data.Str(k);
                    Check(!string.IsNullOrEmpty(s) && s != k, $"{k} = '{(s ?? "").Replace("\n", " ").Substring(0, Math.Min(60, (s ?? "").Length))}'");
                }
                // Tutorial1..6.bik through the install's binkw32.dll
                for (int i = 1; i <= ReflexTutorials.Lessons; i++)
                {
                    string v = ReflexTutorials.Video(i), path = ReflexBink.PathOf(v);
                    IntPtr p = ReflexBink.reflex_bink_open(ReflexBink.Dll, path, 0, out var info);
                    Check(p != IntPtr.Zero && info.width == 512 && info.height == 256 && info.frames > 300, $"{v}: {info.width}x{info.height}, {info.frames} frames, {info.Fps:F2} fps, audio {info.audioChannels}" + (p == IntPtr.Zero ? " " + ReflexBink.Error(IntPtr.Zero) : ""));
                    if (p != IntPtr.Zero) ReflexBink.reflex_bink_close(p);
                }
                // the voice-over the world asks for (TutorialAudioMappings / the popups' Lua): the FEV project of the path's first segment
                var vo = new List<string> { "TUTORIAL_INTRO/VO/INTRO", "TUTORIAL_OUTRO/VO/OUTRO", "TUTORIAL_01/VO/TUTORIAL_[PLATFORM]", "TUTORIAL_01/VO/TUTORIAL_COMPLETED",
                                            "TUTORIAL_02/VO/TUTORIAL_FAIL", "TUTORIAL_03/VO/TUTORIAL_BOUNDARY", "TUTORIAL_04/VO/TUTORIAL", "TUTORIAL_05/VO/TUTORIAL_FAIL_WRECK_[PLATFORM]", "TUTORIAL_06/VO/TUTORIAL_COMPLETED" };
                foreach (string p in vo)
                {
                    string r = FeSounds.ResolveLocalized("AudioLocalizedTutorial", "Audio", p, out string project);
                    string want = p.Substring(0, p.IndexOf('/')) + "_";
                    Check(r != null && project != null && project.StartsWith(want, StringComparison.OrdinalIgnoreCase), $"VO {p} -> {r} (project {project})");
                }
            }
            catch (Exception ex) { Check(false, "exception " + ex); }
            RestorePrefs(); Write("verify");
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        /// Diagnostics: the colour styles of the tutorial layers' components (the install's layers, as the live mirror builds them).
        public static void Styles()
        {
            report.Clear(); bad = 0; outDir = Env("TUT_SHOTS", Path.Combine(Application.temporaryCachePath, "tut"));
            foreach (var l in new[] { "TutorialIntroductionPopup", "TutorialPauseMenu", "PreTutorialPopup" })
            {
                var (pkg, asset) = OriginalLiveUi.LayerPackages.Find(l);
                var doc = OriginalUiAssets.Data.BuildLayerOwn(pkg, asset);
                void Walk(Dictionary<string, object> e, string path)
                {
                    string name = OriginalUi.S(e, "name") ?? "?"; var st = OriginalUi.Obj(e, "style");
                    if (st != null) Log($"{l} {path}/{name}: " + string.Join(" ", st.Where(kv => kv.Key.EndsWith("Color") || kv.Key == "color").Select(kv => kv.Key + "=" + (kv.Value is List<object> c ? string.Join(",", c.Select(x => Convert.ToString(x, CultureInfo.InvariantCulture))) : kv.Value))));
                    foreach (var k in OriginalUi.Arr(e, "children") ?? new List<object>()) if (k is Dictionary<string, object> d) Walk(d, path + "/" + name);
                }
                foreach (var e in OriginalUi.Arr(doc, "elements")) Walk((Dictionary<string, object>)e, "");
            }
            Write("styles"); if (Application.isBatchMode) EditorApplication.Exit(0);
        }

        // ---------------------------------------------------------------------------------------------- FrontEnd
        static FrontEndManager fe; static Camera cam; static RenderTexture rt;
        static void Run(float seconds) { for (float t = 0; t < seconds; t += 1f / 30f) fe.Step(1f / 30f); }
        static void FeShot(string name)
        {
            Canvas.ForceUpdateCanvases(); cam.Render();
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0); tex.Apply();
            RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(outDir, name + ".png"), tex.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(tex);
            Log($"shot {name}: page '{fe.CurrentPage}' popup '{fe.TopPopup}'");
        }
        public static void FrontEnd()
        {
            report.Clear(); bad = 0; outDir = Env("TUT_SHOTS", Path.Combine(Application.temporaryCachePath, "tut")); Directory.CreateDirectory(outDir); saved = SavePrefs();
            ReflexProfile.Disabled = true;   // the guest: the default profile's TutorialsEntered FALSE
            FeTutorialScripts.FirstTimePopupInBatchMode = true;
            ReflexEvents.Event launched = null;
            FrontEndManager.LaunchOverride = (ev, ai) => launched = ev;
            try
            {
                rt = new RenderTexture(1280, 720, 24);
                cam = new GameObject("bench camera").AddComponent<Camera>();
                cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black; cam.targetTexture = rt; cam.orthographic = true; cam.enabled = false;
                fe = new GameObject("Reflex front end").AddComponent<FrontEndManager>();
                fe.uiShader = Shader.Find("Reflex/FrontEnd UI"); fe.rememberChoice = false;
                fe.Boot();
                fe.canvas.renderMode = RenderMode.ScreenSpaceCamera; fe.canvas.worldCamera = cam; fe.canvas.planeDistance = 1;
                Run(1.0f);
                fe.DispatchKey("Accept"); Run(0.5f);                          // title -> profile reminder (guest)
                fe.DispatchKey("Accept"); Run(0.4f); Run(0.8f);               // CONTINUE -> main menu
                Check(fe.CurrentPage == "Main Menu Page", "main menu: " + fe.CurrentPage);
                Check(fe.TopPopup == "TutorialFirstTimeEntryPopup", "MainMenu:Present: TutorialsEntered FALSE -> the first-visit popup: " + fe.TopPopup);
                FeShot("fe_01_first_time_entry_popup");
                var pl = fe.Layer("TutorialFirstTimeEntryPopup.lyr");
                string Text(string n) => pl?.Find(n)?.text?.Text;
                Log($"popup texts: title '{Text("TitleText")}' button one '{pl?.Find("ButtonOne")?.Find("ButtonText")?.text?.Text}' two '{pl?.Find("ButtonTwo")?.Find("ButtonText")?.text?.Text}'");
                fe.DispatchKey("Back"); Run(0.6f);                            // OnBackKeypress: SKIP
                Check(fe.TopPopup == null && fe.CurrentPage == "Main Menu Page" && FeTutorialScripts.TutorialsEntered, "Back (SKIP): the popup closes, TutorialsEntered TRUE in memory: " + fe.TopPopup);
                Check(launched == null, "SKIP launches nothing");
                FeShot("fe_02_main_menu");
                fe.DispatchKey("Misc"); Run(1.0f);                            // MOTOSKILLS (F2 / Y: MainMenu:OnTutorialKeypress)
                Check(launched != null && launched.key == ReflexTutorials.Key, "MOTOSKILLS hands over the tutorial: " + launched?.key);
                Check(FrontEndManager.ReturnPage == "Main Menu Page", "back on the Main Menu Page after: " + FrontEndManager.ReturnPage);
            }
            catch (Exception ex) { Check(false, "exception " + ex); }
            FrontEndManager.LaunchOverride = null; FrontEndManager.ReturnPage = null; FeTutorialScripts.FirstTimePopupInBatchMode = false;
            if (fe) UnityEngine.Object.DestroyImmediate(fe.gameObject);
            RestorePrefs(); Write("frontend");
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------- Flow
        // The real hand-over, play mode from FrontEnd.unity (the guest: no profile session): title -> profile reminder -> main menu with the
        // first-visit popup -> START -> the Tutorial Load Page over the scene load -> the tutorial world (intro popup) -> Enter -> the pause
        // menu -> QUIT MOTOSKILLS -> the game's Main Menu Page -> the port's front end on its Main Menu Page, without the popup again.
        static int fstage; static double fstageAt; static FrontEndManager ffm; static bool loadingSeen;
        public static void Flow()
        {
            report.Clear(); bad = 0; fstage = 0; shots = 0; runtime = null; live = null; tut = null; loadingSeen = false;
            outDir = Env("TUT_SHOTS", Path.Combine(Application.temporaryCachePath, "tut")); Directory.CreateDirectory(outDir);
            saved = SavePrefs();
            ReflexProfile.Disabled = true;
            FeTutorialScripts.FirstTimePopupInBatchMode = true;
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            ffm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (ffm) ffm.rememberChoice = false;
            started = fstageAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += FTick; EditorApplication.EnterPlaymode();
        }
        static void FNext() { fstage++; fstageAt = EditorApplication.timeSinceStartup; }
        static void FTick()
        {
            double now = EditorApplication.timeSinceStartup, age = now - fstageAt;
            if (now - started > 900) { Check(false, $"timeout at flow stage {fstage}"); FFinish(); return; }
            if (!EditorApplication.isPlaying) return;
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
            switch (fstage)
            {
                case 0: if (fm && fm.assets != null && age > 2) { fm.DispatchKey("Accept"); FNext(); } break;   // title
                case 1: if (age > 1) { fm.DispatchKey("Accept"); FNext(); } break;                                // profile reminder: CONTINUE
                case 2:
                    if (fm && !fm.Busy && fm.CurrentPage == "Main Menu Page" && fm.TopPopup == "TutorialFirstTimeEntryPopup" && age > 1.5) { Shot("flow_01_first_time_popup", null); fm.DispatchKey("Accept"); FNext(); }
                    else if (age > 30) { Check(false, $"no first-visit popup: {fm?.CurrentPage} / {fm?.TopPopup}"); FFinish(); }
                    break;
                case 3:   // the Tutorial Load Page, then the tutorial world
                    if (!loadingSeen && FrontEndLoadingOverlay.Current) { loadingSeen = true; Shot("flow_02_tutorial_load_page", null); }
                    if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                    if (runtime && !live) live = runtime.GetComponent<OriginalLiveUi>();
                    if (runtime && !tut) tut = runtime.GetComponent<OriginalTutorial>();
                    if (live && live.Running && Top(live.Last) == "TutorialIntroductionPopup")
                    {
                        Check(loadingSeen, "the Tutorial Load Page showed over the scene load");
                        Check(tut != null, "START handed over the tutorial (OriginalTutorial on the world)");
                        Shot("flow_03_tutorial_world", live.Last); runtime.testHeldKeys = new[] { 0x1c }; FNext();
                    }
                    else if (age > 600) { Check(false, "the tutorial world did not come up"); FFinish(); }
                    break;
                case 4:
                    runtime.testHeldKeys = age < 0.12 ? new[] { 0x1c } : Array.Empty<int>();
                    if (live.Last != null && Shows(live.Last, "Tutorial Pause Menu Page") && age > 2) { downs = 6; nextDown = now + 0.3; FNext(); }
                    else if (age > 30) { Check(false, "no pause menu"); FFinish(); }
                    break;
                case 5:
                    if (downs > 0) { runtime.testHeldKeys = now >= nextDown && now < nextDown + 0.12 ? new[] { 0xd0 } : Array.Empty<int>(); if (now >= nextDown + 0.12) { downs--; nextDown = now + 0.3; } break; }
                    runtime.testHeldKeys = now >= nextDown + 0.3 && now < nextDown + 0.42 ? new[] { 0x1c } : Array.Empty<int>();
                    if (now > nextDown + 0.5) FNext();
                    break;
                case 6:
                    if (fm && fm.assets != null && !fm.Busy && age > 4)
                    {
                        Check(fm.CurrentPage == "Main Menu Page" && fm.TopPopup == null, $"back on the port's Main Menu Page, no first-visit popup again: {fm.CurrentPage} / {fm.TopPopup}");
                        Shot("flow_04_back_main_menu", null); FFinish();
                    }
                    else if (age > 60) { Check(false, "no front end after QUIT MOTOSKILLS"); FFinish(); }
                    break;
            }
        }
        static void FFinish()
        {
            EditorApplication.update -= FTick;
            FeTutorialScripts.FirstTimePopupInBatchMode = false;
            RestorePrefs(); Write("flow");
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------- Play
        static OriginalWorldRuntime runtime; static OriginalLiveUi live; static OriginalTutorial tut;
        static double started, stageAt, runStart; static int stage, shots, lesson; static string lastPages = "";
        static readonly HashSet<string> pagesSeen = new HashSet<string>();
        static int completedBefore = -1; static double introAt = -1; static bool lessonShot;
        public static void Play()
        {
            report.Clear(); bad = 0; stage = 0; shots = 0; runStart = -1; introAt = -1; lessonShot = false; lastPages = ""; pagesSeen.Clear(); runtime = null; live = null; tut = null;
            outDir = Env("TUT_SHOTS", Path.Combine(Application.temporaryCachePath, "tut")); Directory.CreateDirectory(outDir);
            lesson = int.Parse(Env("TUT_LESSON", "1"), CultureInfo.InvariantCulture);
            saved = SavePrefs();
            ReflexProfile.Disabled = false;
            completedBefore = ReflexProfile.HasSave ? -2 : 0;   // read from the world (progress) once it runs
            ReflexEventSelect.Pending = ReflexTutorials.Key; ReflexEventSelect.PendingAi = 0;
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();   // the build order (FrontEnd 0, Reflex 1): the way back
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += Tick;
            started = stageAt = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static string Top(OriginalLiveUi.Frame f) => f.layers.Where(l => l.graph == "PopupPageStack.pag" && (l.state & 0x10) != 0).Select(l => l.page).LastOrDefault();
        static bool Shows(OriginalLiveUi.Frame f, string page) => f.layers.Any(l => l.page == page && (l.state & 0x10) != 0);
        static string Txt(OriginalLiveUi.Frame f, string layer, string node) => f.layers.Where(l => l.name == layer).SelectMany(l => l.nodes).FirstOrDefault(n => n.name == node && n.text != null && (n.state & 0x10) != 0)?.text;
        static string state, progress; static bool asking;
        static void Ask() { if (asking || !tut) return; asking = true; tut.Ask((s, p) => { state = s; progress = p; asking = false; }); }
        static int[] keys = Array.Empty<int>(); static double keysUntil;
        static void Press(params int[] k) { keys = k; keysUntil = EditorApplication.timeSinceStartup + 0.12; }
        static int downs; static double nextDown;

        static void Tick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - started > double.Parse(Env("TUT_TIMEOUT", "900"), CultureInfo.InvariantCulture)) { Check(false, $"timeout at stage {stage}"); Finish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (stage >= 100)   // back in the front end
            {
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                if (fm && fm.assets != null && Age > 4 && !fm.Busy)
                {
                    Check(fm.CurrentPage == "Main Menu Page", "the port's front end on its Main Menu Page: " + fm.CurrentPage);
                    Shot("play_99_front_end", null);
                    var open = ReflexProfile.Open(); open.Wait(120000);
                    int done = ReflexProfile.Current?.T("PlayerRider").Int("TutorialsCompleted", 0, -1) ?? -1, entered = ReflexProfile.Current?.T("PlayerRider").Int("TutorialsEntered", 0, -1) ?? -1;
                    Check(lesson != 1 || done >= Math.Max(1, completedBefore), $"the save read back: TutorialsCompleted {done} (before {completedBefore}), TutorialsEntered {entered}");
                    Finish();
                }
                else if (Age > 60) { Check(false, "no front end after leaving: " + UnityEngine.SceneManagement.SceneManager.GetActiveScene().name); Finish(); }
                return;
            }
            if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime) return;
            if (!live) live = runtime.GetComponent<OriginalLiveUi>();
            if (!tut) tut = runtime.GetComponent<OriginalTutorial>();
            if (tut && tut.Leaving) { Log("the game left the tutorial; scene " + UnityEngine.SceneManagement.SceneManager.GetActiveScene().name); stage = 100; stageAt = now; return; }
            if (!runtime.Running || live == null || !live.Running) { if (Age > 600) { Check(false, "the world did not run"); Finish(); } return; }
            var f = live.Last;
            if (runStart < 0) { runStart = now; Log("tutorial world running; attach: " + tut?.AttachAnswer); Check(tut && tut.AttachAnswer != null && tut.AttachAnswer.StartsWith("attached"), "the game's UI attached in game"); Ask(); }
            string pages = string.Join(" + ", f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").Select(l => l.page + ":" + l.name).Distinct());
            foreach (var l in f.layers) if ((l.state & 0x10) != 0) pagesSeen.Add(l.graph + "/" + l.page);
            if (pages != lastPages) { lastPages = pages; Log($"t {now - runStart:F1} pages: {pages}"); Ask(); }
            runtime.testHeldKeys = now < keysUntil ? keys : LeanKeys(now);
            string top = Top(f);
            switch (stage)
            {
                case 0:   // TutorialIntroductionPopup (2.5 s after it shows: its narration is in)
                    if (top == "TutorialIntroductionPopup" && introAt < 0) introAt = now;
                    if (top == "TutorialIntroductionPopup" && now - introAt > 2.5)
                    {
                        Shot("play_01_intro", f);
                        Check(Txt(f, "Tutorial Introduction Popup", "TitleText") == "MotoSkill Training!", "intro title: " + Txt(f, "Tutorial Introduction Popup", "TitleText"));
                        bool vo; lock (OriginalEventIntro.VoiceLog) vo = OriginalEventIntro.VoiceLog.Any(v => v.Contains("TUTORIAL_INTRO/VO/INTRO") && !v.Contains("no such package"));
                        Check(vo, "the intro narration played: " + string.Join(" | ", OriginalEventIntro.VoiceLog.TakeLast(4)));
                        if (progress != null && completedBefore == -2) { completedBefore = int.Parse(progress.Split(' ')[0], CultureInfo.InvariantCulture); Log("progress at start: " + progress); }
                        Press(0x1c); Next();
                    }
                    else if (Age > 60) { Check(false, "no intro popup: " + pages); Finish(); }
                    break;
                case 1:   // the Tutorial Pause Menu Page
                    if (Shows(f, "Tutorial Pause Menu Page") && Age > 2)
                    {
                        Shot("play_02_pause_menu", f);
                        var rows = f.layers.Where(l => l.name == "TutorialPauseMenu.lyr").SelectMany(l => l.nodes).Where(n => n.name == "Text" && n.text != null).Select(n => n.text).ToList();
                        var locks = string.Concat(f.layers.Where(l => l.name == "TutorialPauseMenu.lyr").SelectMany(l => l.nodes).Where(n => n.name == "Lock").Select(n => (n.state & 0x10) != 0 ? "L" : "-"));
                        Log("pause menu rows: " + string.Join(", ", rows) + "; locks " + locks);
                        Check(rows.Contains("RIDER REFLEX") && rows.Contains("QUIT MOTOSKILLS"), "the lessons and QUIT MOTOSKILLS");
                        if (completedBefore < 0 && progress != null) completedBefore = int.Parse(progress.Split(' ')[0], CultureInfo.InvariantCulture);
                        downs = Math.Max(0, lesson - 1); nextDown = now + 0.3; Next();
                    }
                    else if (Age > 30) { Check(false, "no tutorial pause menu: " + pages); Finish(); }
                    break;
                case 2:   // the lesson's row
                    if (downs > 0) { if (now >= nextDown) { Press(0xd0); downs--; nextDown = now + 0.4; } break; }
                    if (now >= nextDown + 0.3) { Press(0x1c); Next(); }
                    break;
                case 3:   // PreTutorialPopup and its video
                    if (top == "PreTutorialPopup" && Age > 3)
                    {
                        Shot($"play_03_pre_tutorial_{lesson}", f);
                        Check(Txt(f, "PreTutorialPopup", "Tutorial Popup Title") != null, "lesson title: " + Txt(f, "PreTutorialPopup", "Tutorial Popup Title"));
                        Check(tut.PlayingVideo == ReflexTutorials.Video(lesson) && tut.Player && tut.Player.HasFrame, $"the lesson's video plays: {tut.PlayingVideo} frame {(tut.Player ? tut.Player.Frame : -1)}");
                        Press(0x1c); Next();
                    }
                    else if (Age > 30) { Check(false, "no PreTutorialPopup: " + pages); Finish(); }
                    break;
                case 4:   // the lesson: lean (lesson 1) until the game's outcome popup
                    if (Age > 2 && !lessonShot) { lessonShot = true; Shot($"play_04_lesson_{lesson}", f); Log("hint: " + Txt(f, "TutorialActivation", "TutorialLabel") + " / " + Txt(f, "TutorialActivation", "TutorialActivationString"));
                        var lvl = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
                        var names = tut.ObjectsShown.Select(id => lvl?.Content?.instances.FirstOrDefault(i => i.entityId == id)?.name ?? id.ToString()).ToList();
                        Check(tut.ObjectsKnown >= 20 && names.Count > 0 && names.All(n => n.StartsWith("TUT0" + lesson, StringComparison.Ordinal)), $"the lesson's own scene objects shown ({tut.ObjectsKnown} known): {string.Join(", ", names)}"); }
                    if (top == "PostTutorialPopup" || top == "FailedTutorialPopup")
                    {
                        Shot($"play_05_{top}", f);
                        Log($"{top}: " + string.Join(" / ", f.layers.Where(l => l.page == top).SelectMany(l => l.nodes).Where(n => n.text != null && (n.state & 0x10) != 0).Select(n => n.text.Replace("\n", " "))));
                        Check(lesson != 1 || top == "PostTutorialPopup", "lesson 1 passed by the game's own check (the rider leaned four ways)");
                        nextDown = now;
                        if (top == "FailedTutorialPopup") { stage = 20; stageAt = now; break; }   // RETRY (ButtonOne, focused)
                        Next();
                    }
                    else if (Age > 90) { Check(false, "no outcome popup: " + pages); Finish(); }
                    break;
                case 5: if (now >= nextDown + 0.6) { Press(0x1c); Next(); } break;
                case 6:   // the pause menu again, the save written
                    if (Shows(f, "Tutorial Pause Menu Page") && Age > 3 && tut.Saves > 0)
                    {
                        var locks = string.Concat(f.layers.Where(l => l.name == "TutorialPauseMenu.lyr").SelectMany(l => l.nodes).Where(n => n.name == "Lock").Select(n => (n.state & 0x10) != 0 ? "L" : "-"));
                        Shot("play_06_pause_menu_after", f);
                        Log("locks after: " + locks + ", saves " + tut.Saves + ", progress " + progress);
                        Check(lesson != 1 || locks.StartsWith("--"), "two lessons unlocked after the first: " + locks);
                        downs = 6; nextDown = now + 0.3; Next();
                    }
                    else if (Age > 30) { Check(false, $"no pause menu / save after the lesson (saves {tut.Saves}): {pages}"); Finish(); }
                    break;
                case 7:   // QUIT MOTOSKILLS
                    if (downs > 0) { if (now >= nextDown) { Press(0xd0); downs--; nextDown = now + 0.35; } break; }
                    if (now >= nextDown + 0.3) { Shot("play_07_quit_row", f); Press(0x1c); Next(); }
                    break;
                case 8: if (Age > 20) { Check(false, "the game did not leave the tutorial: " + pages); Finish(); } break;
                // a failed lesson: RETRY (the TutorialManager's RetryTutorial: the lesson again from its PreTutorialPopup), then Esc (the
                // pause key in tutorial mode: MXUIManager_OnPauseRequest's GotoTutorialPauseMenuPage) and QUIT MOTOSKILLS
                case 20: if (now >= nextDown + 0.8) { Press(0x1c); stage = 21; stageAt = now; } break;
                case 21:
                    if (top == "PreTutorialPopup" && Age > 1.5) { Shot($"play_08_retry_{lesson}", f); Check(true, "RETRY: the lesson's PreTutorialPopup again"); Press(0x1c); stage = 22; stageAt = now; }
                    else if (Age > 20) { Check(false, "RETRY did not restart the lesson: " + pages); Finish(); }
                    break;
                case 22: if (Age > 2) { Press(0x01); stage = 23; stageAt = now; } break;
                case 23:
                    if (Shows(f, "Tutorial Pause Menu Page") && Age > 1.5) { Shot($"play_09_pause_key_{lesson}", f); Check(true, "Esc in a lesson: the Tutorial Pause Menu Page"); downs = 6; nextDown = now + 0.3; stage = 7; stageAt = now; }
                    else if (Age > 15) { Check(false, "Esc did not pause the lesson: " + pages); Finish(); }
                    break;
            }
        }
        // lesson 1 (Rider Reflex): forward, back, left, right on the arrow keys (the rider lean the game reads from the keyboard)
        static int[] LeanKeys(double now)
        {
            if (stage == 4 && lesson != 1 && Age > 3) return new[] { 0x11 };   // the other lessons: throttle only, until the game's verdict
            if (stage != 4 || lesson != 1 || Age < 3) return Array.Empty<int>();
            int k = (int)((Age - 3) / 0.7) % 4; double ph = (Age - 3) % 0.7;
            return ph < 0.5 ? new[] { new[] { 0xc8, 0xd0, 0xcb, 0xcd }[k] } : Array.Empty<int>();
        }

        // the scene through the game camera (with the port's post chain), the overlay canvases through a camera of their own (no HDR / post,
        // as the screen draws them after the post chain), composited (PagesTests.Shot)
        static void Shot(string name, OriginalLiveUi.Frame f)
        {
            if (shots++ > 30) return;
            const int W = 1280, H = 720;
            var c = runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            Texture2D a;
            if (c)
            {
                var scene = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                var prevTarget = c.targetTexture; c.targetTexture = scene; c.Render(); c.targetTexture = prevTarget;
                a = Read(scene, W, H); scene.Release();
            }
            else { a = new Texture2D(W, H, TextureFormat.RGBA32, false); var px = new Color32[W * H]; a.SetPixels32(px); a.Apply(); }
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy && x.name.IndexOf("Event list", StringComparison.OrdinalIgnoreCase) < 0).OrderBy(x => x.sortingOrder).ToList();
            var ui = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            var cgo = new GameObject("tutorial shot ui camera"); var uc = cgo.AddComponent<Camera>();
            uc.clearFlags = CameraClearFlags.SolidColor; uc.backgroundColor = new Color(0, 0, 0, 0); uc.cullingMask = 1 << 31; uc.allowHDR = false; uc.targetTexture = ui;
            var layers = new List<(GameObject, int)>();
            foreach (var o in overlays) { foreach (var t in o.GetComponentsInChildren<Transform>(true)) { layers.Add((t.gameObject, t.gameObject.layer)); t.gameObject.layer = 31; } o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = uc; o.planeDistance = 1f - o.sortingOrder * 0.0005f; }
            Canvas.ForceUpdateCanvases(); uc.Render();
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            foreach (var (go, l) in layers) go.layer = l;
            var b = Read(ui, W, H); ui.Release(); UnityEngine.Object.DestroyImmediate(cgo);
            var pa = a.GetPixels32(); var pb = b.GetPixels32();
            for (int i = 0; i < pa.Length; i++) { float t = pb[i].a / 255f; pa[i] = new Color32((byte)Mathf.Min(255, pb[i].r + pa[i].r * (1 - t)), (byte)Mathf.Min(255, pb[i].g + pa[i].g * (1 - t)), (byte)Mathf.Min(255, pb[i].b + pa[i].b * (1 - t)), 255); }
            a.SetPixels32(pa); a.Apply();
            File.WriteAllBytes(Path.Combine(outDir, name + ".png"), a.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(a); UnityEngine.Object.DestroyImmediate(b);
            Log("shot " + name + (f != null ? " | " + string.Join(" ", f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").SelectMany(l => l.nodes.Where(n => n.text != null && n.text.Length > 0 && (n.state & 0x10) != 0 && !n.name.EndsWith("Shadow", StringComparison.OrdinalIgnoreCase)).Select(n => n.name + "='" + n.text.Replace("\n", " ").Substring(0, Math.Min(40, n.text.Length)) + "'")).Take(12)) : ""));
        }
        static Texture2D Read(RenderTexture rt, int W, int H)
        {
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(W, H, TextureFormat.RGBA32, false); tex.ReadPixels(new Rect(0, 0, W, H), 0, 0); tex.Apply(); RenderTexture.active = prev;
            return tex;
        }

        static void Finish()
        {
            EditorApplication.update -= Tick;
            Log("pages seen: " + string.Join(", ", pagesSeen.OrderBy(x => x)));
            Log("state " + state + " progress " + progress);
            ReflexProfile.Close();
            RestorePrefs(); Write("play" + (lesson != 1 ? "_lesson" + lesson : ""));
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
