using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // The game modes (Content/ReflexModes.cs, OriginalWorld/OriginalLiveUi.cs, native modes.cpp; _re/notes/modes.md).
    //   Verify: edit mode. The mode events the install offers (Freestyle x 6 FMX levels, Section Race x 5, the 12 Free Ride challenges),
    //           their world specs (activity, EventTypeActivation, series, scene section), the challenges series of the Race & Ride
    //           challenges page, and every layer of the mode pages built from the install by name (the HUD packages, Shared popups).
    //   Play:   play mode. The race scene on $MODES_EVENT (Freestyle/FMX_BuenaVistaB): the world is built, the game's UI goes in-game
    //           (the hub's route), Enter accepts the game's popups (RulesPopup / PreChallengePopup), throttle + race-line steering (or
    //           a weave without a race line), screenshots at $MODES_TIMES seconds (pre: after the world runs) to $MODES_SHOTS with the
    //           pages and texts the game shows; $MODES_WAIT=post waits for the post-event / post-challenge page, then the port quits
    //           to the front end. PlayerPrefs: nothing written (ReflexEventSelect.Pending*); the user's keys are checked and restored.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.ModesTests.Verify -quit / .Play (no -quit)
    public static class ModesTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Modes] " + s); }
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

        // ---------------------------------------------------------------------------------------------- Verify
        public static void Verify()
        {
            report.Clear(); bad = 0;
            try
            {
                var all = ReflexEvents.All;
                var fs = all.Where(e => e.spec.type == "Freestyle").ToList();
                Check(fs.Count >= 6, $"Freestyle events: {fs.Count} ({string.Join(", ", fs.Select(e => e.key))})");
                foreach (var e in fs) Check(e.spec.activity == 5 && e.spec.typeId == 7 && e.spec.durationType == "Minutes" && (e.spec.vehicleClass == 25 || e.spec.vehicleClass == 24), $"{e.key}: activity {e.spec.activity}, class {e.spec.vehicleClass} ({string.Join("/", e.vehicleClasses)}), {e.spec.durationType} {e.spec.duration}, section {e.sceneSection}");
                var sr = all.Where(e => e.spec.type == "SectionRace").ToList();
                Check(sr.Count >= 5, $"Section Race events: {sr.Count} ({string.Join(", ", sr.Select(e => e.key))})");
                var ch = ReflexModes.Challenges;
                Check(ch.Count == 12, $"Free Ride challenges: {ch.Count}");
                foreach (var e in ch)
                {
                    string c = ReflexModes.ChallengeOf(e);
                    var found = ReflexEvents.Find(e.key);
                    Check(found == e && e.spec.type == "FreeRide" && e.spec.seriesChallenge >= 0 && (c == "HitDiscovery" ? e.spec.activation == "" : e.spec.activation == c),
                          $"{e.key}: activation '{e.spec.activation}', series {e.spec.seriesChallenge}, section {e.sceneSection}, Free Ride {ReflexModes.FreeRideOf(e.key)}");
                }
                var series = ReflexModes.ChallengeSeries();
                Check(series.Count == 3, $"challenges series: {string.Join(", ", series.Select(s => s.seriesId + " " + s.location))}");
                // Target Practice's targets as scenery of the challenge's section (OriginalLiveUi shows / hides them)
                try
                {
                    var tp = ReflexEvents.Find("TargetPractice/FR_Alpine");
                    var lc = ReflexLevelContent.Load(tp.location, new ReflexLevelContent.Options { activity = tp.SceneActivity, terrain = false, tracks = false });
                    var targets = lc.instances.Where(i => i.entityType == "CollectibleTargetPracticeRSData").ToList();
                    Check(targets.Count == 12, $"{tp.key}: section {tp.sceneSection}, {targets.Count} target instances ({string.Join(" ", targets.Select(i => i.entityId))}), {lc.instances.Count} instances, warnings {lc.warnings.Count}");
                }
                catch (Exception x) { Check(false, "targets: " + x.Message); }
                // every layer of the mode pages (InGamePageGraph, PopupPageStack) from the install
                string[] layers =
                {
                    "SinglePlayerFMX.lyr", "SinglePlayerTimer", "SinglePlayerHillClimb", "SinglePlayerHillClimbHeight.lyr", "SinglePlayerTrailBlazer.lyr",
                    "SinglePlayerTargetPractice.lyr", "SinglePlayerFreeride.lyr", "SinglePlayerSectionRace.lyr", "EventIntro.lyr", "PostEvent_ActivityDashboard",
                    "RulesPopup.lyr", "PreChallengePopup.lyr", "PostChallengePopup.lyr", "PostEventMenu.lyr", "PostEventResults.lyr", "PageWipe.lyr",
                };
                foreach (string l in layers)
                {
                    var (pkg, asset) = OriginalLiveUi.LayerPackages.Find(l);
                    int n = 0; string err = null;
                    if (pkg != null) try { n = OriginalUi.Arr(OriginalUiAssets.Data.BuildLayerOwn(pkg, asset), "elements")?.Count ?? 0; } catch (Exception x) { err = x.Message; }
                    Check(pkg != null && err == null && (n > 0 || l == "SinglePlayerTimer"), $"layer {l}: {pkg}/{asset}, {n} elements {err}");
                    if (pkg != null && err == null)   // materials without a texture (drawn white / not at all)
                    {
                        var missing = new List<string>();
                        void Walk(object o) { if (!(o is Dictionary<string, object> d)) return; var st = OriginalUi.Obj(d, "style"); if (OriginalUi.S(st, "material") is string m && OriginalUi.S(st, "texture") == null) missing.Add(OriginalUi.S(d, "name") + ":" + m); if (OriginalUi.Arr(d, "children") is List<object> k) foreach (var c in k) Walk(c); }
                        foreach (var e in OriginalUi.Arr(OriginalUiAssets.Data.BuildLayerOwn(pkg, asset), "elements") ?? new List<object>()) Walk(e);
                        if (missing.Count > 0) Log($"     {l}: materials without a texture: {string.Join(", ", missing.Take(20))}");
                    }
                }
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            Log(bad == 0 ? "VERIFY PASS" : $"VERIFY FAIL ({bad})");
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------- Play
        static string eventKey, outDir, tag, wait; static (bool pre, float t)[] times; static int shot;
        static bool materialsLogged; static double started, runStart = -1, lastLog, enterAt = -1, postAt = -1, frontEndAt = -1;
        static OriginalWorldRuntime runtime; static OriginalLiveUi live;
        static readonly HashSet<string> pagesSeen = new HashSet<string>();
        static string lastPages = ""; static double pageShot = -1; static int pageShots;

        public static void Play()
        {
            report.Clear(); bad = 0; shot = 0; runStart = -1; materialsLogged = false; pageShots = 0; pageShot = -1; enterAt = postAt = frontEndAt = -1; pagesSeen.Clear();
            eventKey = Env("MODES_EVENT", "Freestyle/FMX_BuenaVistaB"); tag = Env("MODES_TAG", eventKey.Replace('/', '_'));
            outDir = Env("MODES_SHOTS", Path.Combine(Application.temporaryCachePath, "modes-shots")); Directory.CreateDirectory(outDir);
            wait = Env("MODES_WAIT", "");
            times = Env("MODES_TIMES", "pre:6,pre:14,6,20").Split(',').Select(s => { bool pre = s.StartsWith("pre:"); return (pre, float.Parse(pre ? s.Substring(4) : s, CultureInfo.InvariantCulture)); }).ToArray();
            saved = SavePrefs();
            ReflexEventSelect.Pending = eventKey; ReflexEventSelect.PendingAi = 0;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += Tick;
            started = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }

        static double raceAt = -1; static bool sentHide;
        static void Tick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - started > double.Parse(Env("MODES_TIMEOUT", "900"), CultureInfo.InvariantCulture)) { Check(false, "timeout"); Finish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (frontEndAt > 0)   // the port went back to its front end (or the Free Ride) after the post pages
            {
                if (now - frontEndAt > 4) { Log("scene after leaving: " + UnityEngine.SceneManagement.SceneManager.GetActiveScene().name); Finish(); }
                return;
            }
            if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime) { if (postAt > 0) { frontEndAt = now; Log("left the race scene"); } return; }
            if (!live) live = runtime.GetComponent<OriginalLiveUi>();
            if (!runtime.Running || live == null || !live.Running) return;
            if (runStart < 0) { runStart = now; Log($"world running; the hub's route: {live.Route}"); Check(!string.IsNullOrEmpty(live.Route), "the game's UI went in-game"); }
            var f = live.Last;
            string pages = string.Join(" + ", f.layers.Where(l => (l.state & 0x10) != 0).Select(l => l.page + ":" + l.name).Distinct());
            foreach (var l in f.layers) if ((l.state & 0x10) != 0) pagesSeen.Add(l.graph + "/" + l.page);
            if (pages != lastPages)
            {
                lastPages = pages; Log($"t {now - runStart:F1} pages: {pages}");
                pageShot = now + 0.6;   // a shot of every new page (after its intro animation), at most 12
            }
            if (pageShot > 0 && now >= pageShot && pageShots < 12) { pageShot = -1; var top = f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").Select(l => l.page).LastOrDefault() ?? "none"; Capture($"{tag}_page{pageShots++:00}_{top.Replace(' ', '_')}"); }
            // the game's popups take Enter (RulesPopup, PreChallengePopup): a press 1.5 s after one shows (OriginalLiveUi forwards it)
            bool popup = f.layers.Any(l => l.graph == "PopupPageStack.pag" && (l.page == "RaceRulesPopup" || l.page == "PreChallengePopup"));
            if (popup && enterAt < 0) enterAt = now + 1.5;
            bool press = enterAt > 0 && now >= enterAt && now < enterAt + 0.15;
            if (enterAt > 0 && now >= enterAt + 0.15) enterAt = -1;
            if (press && !sentHide && f.layers.Any(l => l.page == "PreChallengePopup")) { sentHide = true; live.SendUIEvent("HidePreChallengePopup"); }   // once (each pops the stack's top)
            if (!popup) sentHide = false;
            bool riding = f.hasMode && f.mode.state == 2;
            if (riding && raceAt < 0) { raceAt = now; Log($"activity {f.mode.name} in progress"); }
            var held = new List<int>(); if (press) held.Add(0x1c);
            if (riding) held.AddRange(Steer(now));
            runtime.testHeldKeys = held.ToArray();
            if (now - lastLog > 3.0)
            {
                lastLog = now;
                var texts = f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").SelectMany(l => l.nodes.Where(n => n.text != null && n.text.Length > 0 && (n.state & 0x10) != 0 && !n.name.EndsWith("Shadow", StringComparison.OrdinalIgnoreCase)).Select(n => n.name + "='" + n.text.Replace("\n", " ").Substring(0, Math.Min(40, n.text.Length)) + "'")).Take(14);
                if (!materialsLogged && live.ObjectViews > 0)   // the targets' materials (template, blend) once
                {
                    materialsLogged = true;
                    var go = GameObject.FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None).FirstOrDefault(r => r.name.Contains("#") && f.objects.Any(o => r.name.EndsWith("#" + o.entityId)));
                    var lvl = GameObject.FindAnyObjectByType<OriginalLevel>();
                    if (lvl && lvl.Content != null) foreach (var line in lvl.Content.DescribeMaterials().Split('\n')) if (line.StartsWith("GL_Target")) Log("target material " + line);
                    if (go) foreach (var m in go.sharedMaterials) if (m) Log($"target {go.name}: material {m.name} shader {m.shader.name} queue {m.renderQueue} src {(m.HasProperty("_SrcBlend") ? m.GetFloat("_SrcBlend") : -1)} dst {(m.HasProperty("_DstBlend") ? m.GetFloat("_DstBlend") : -1)} test {(m.HasProperty("_AlphaTest") ? m.GetFloat("_AlphaTest") : -1)} alphaSrc {(m.HasProperty("_AlphaSource") ? m.GetFloat("_AlphaSource") : -1)} tex {(m.HasProperty("_BaseMap") && m.GetTexture("_BaseMap") ? m.GetTexture("_BaseMap").name : "-")}");
                }
                Log($"t {now - runStart:F1} {f.mode.name} state {f.mode.state} clock {f.mode.clock:F1} sim {runtime.SimMilliseconds:F2} ms" + (f.objects.Count > 0 ? $" objects {f.objects.Count} shown {live.ObjectsShown} drawn here {live.ObjectViews}" : "") + (f.mode.areaActive != 0 ? $" area {f.mode.area}" : "") + $" | {string.Join(" ", texts)}");
            }
            bool post = f.hasMode && f.mode.state == 4 || f.layers.Any(l => l.page == "PostChallengePopup" || l.page == "PostEventPage" || l.page == "PostEventResults");
            if (post && postAt < 0) { postAt = now; Log($"post: {pages}"); Capture($"{tag}_post"); }
            if (shot < times.Length)
            {
                var s = times[shot];
                double since = s.pre ? now - runStart : raceAt < 0 ? -1 : now - raceAt;
                if (since >= s.t) { Capture($"{tag}_{shot}_{(s.pre ? "pre" : "run")}{s.t:0.#}s"); shot++; }
                return;
            }
            if (wait == "post")
            {
                if (postAt < 0) return;
                if (now - postAt > 4)   // the post page's own accept (the game's Enter) leaves; the port quits to its front end
                {
                    if (now - postAt > double.Parse(Env("MODES_POSTWAIT", "30"), CultureInfo.InvariantCulture)) { Log("post pages stayed: quitting to the front end"); OriginalPauseMenu.QuitToFrontEnd(); frontEndAt = now; return; }
                    // Enter every 2 s ($MODES_EXIT=1: Down first, the second button: PostChallengePopup EXIT)
                    double ph = (now - postAt) % 2.0; bool exit = Env("MODES_EXIT", "0") == "1";
                    runtime.testHeldKeys = exit && ph >= 0.5 && ph < 0.6 ? new[] { 0xcd } : exit && ph >= 0.9 && ph < 1.0 ? new[] { 0xd0 } : ph < 0.1 && (!exit || now - postAt > 5) ? new[] { 0x1c } : Array.Empty<int>();
                }
                return;
            }
            Finish();
        }

        static void Finish()
        {
            EditorApplication.update -= Tick;
            Log("pages seen: " + string.Join(", ", pagesSeen.OrderBy(x => x)));
            RestorePrefs();
            Log(bad == 0 ? "PLAY PASS" : $"PLAY FAIL ({bad})");
            File.WriteAllText(Path.Combine(outDir, tag + ".txt"), report.ToString());
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------- Challenges
        // The Race & Ride challenges page in the port's front end (FrontEndChallenges.cs): title -> (the motocard setup of a new bench
        // save) -> main menu -> the page (its page-graph name: the target of "GotoRaceAndRideChallengesPage") -> its 3 series, right (a
        // locked one on a new save), left -> accept -> the series' Free Ride world with the game's UI live. Shots $MODES_SHOTS/challenges_*.png.
        static int cstage; static double cstageAt; static string challengesPage; static FrontEndManager cfm; static bool cnamed;
        public static void Challenges()
        {
            report.Clear(); bad = 0; cstage = 0; tag = "challenges"; cnamed = false; runtime = null; live = null;
            outDir = Env("MODES_SHOTS", Path.Combine(Application.temporaryCachePath, "modes-shots")); Directory.CreateDirectory(outDir);
            saved = SavePrefs(); ReflexProfile.Disabled = false;
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            cfm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (cfm) cfm.rememberChoice = false;
            started = cstageAt = EditorApplication.timeSinceStartup;
            EditorApplication.update += CTick; EditorApplication.EnterPlaymode();
        }
        static void CNext() { cstage++; cstageAt = EditorApplication.timeSinceStartup; }
        static void CFinish() { EditorApplication.update -= CTick; pagesSeen.Clear(); Finish(); }
        static void CTick()
        {
            double now = EditorApplication.timeSinceStartup, age = now - cstageAt;
            if (now - started > double.Parse(Env("MODES_TIMEOUT", "400"), CultureInfo.InvariantCulture)) { Check(false, $"timeout (stage {cstage}, page {cfm?.CurrentPage})"); CFinish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (cstage < 10 && !cfm) cfm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
            bool idle = cfm && cfm.assets != null && !cfm.Busy;
            switch (cstage)
            {
                case 0: if (idle && cfm.CurrentPage == "Title Page" && age > 2) { cfm.DispatchKey("Accept"); CNext(); } break;
                case 1:   // the profile loads; a new bench save goes through the motocard setup (accepted as shown)
                    if (!idle || age < 1.5) break;
                    if (cfm.CurrentPage == "Main Menu Page" && !ReflexProfile.Busy && ReflexProfile.Current != null)
                    {
                        var pages = (System.Collections.IDictionary)typeof(FrontEndManager).GetField("pages", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(cfm);
                        var names = pages.Keys.Cast<string>().Where(k => k.IndexOf("Challenge", StringComparison.OrdinalIgnoreCase) >= 0).ToList();
                        Log("pages with Challenge: " + string.Join(", ", names));
                        challengesPage = names.FirstOrDefault(k => k.IndexOf("Race", StringComparison.OrdinalIgnoreCase) >= 0) ?? names.FirstOrDefault();
                        Check(challengesPage != null, "the challenges page exists: " + challengesPage);
                        if (challengesPage == null) { CFinish(); return; }
                        cfm.vars["SeriesStoreProductId"] = 2904; ReflexCareer.SeriesStoreProductId = 2904;   // the Race & Ride page's accept of "Free Ride"
                        cfm.JumpTo(challengesPage); CNext();
                    }
                    else if (cfm.CurrentPage == "Motocard Setup Name Page" && !cnamed) { cnamed = true; (cfm.Layer("MotocardNameEntry.lyr").script as MotocardNameEntryScript)?.Input?.Type("Tester"); cfm.DispatchKey("Accept"); cstageAt = now; }
                    else if (cfm.CurrentPage != "Main Menu Page" && cfm.CurrentPage != "Load Game Page" && cfm.CurrentPage != "Title Page") { Log("setup page: " + cfm.CurrentPage); cfm.DispatchKey("Accept"); cstageAt = now; }
                    break;
                case 2:
                    if (idle && cfm.CurrentPage == challengesPage && age > 3)
                    {
                        CaptureFe("challenges_0_page");
                        var layers = (Dictionary<string, FeLayer>)typeof(FrontEndManager).GetField("layers", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(cfm);
                        var script = layers.Values.Where(l => l.active).Select(l => l.script).OfType<RaceAndRideChallengesScript>().FirstOrDefault();
                        Log("active layers: " + string.Join(", ", layers.Where(kv => kv.Value.active).Select(kv => kv.Key + "=" + (kv.Value.script?.GetType().Name ?? "-"))));
                        Log($"series rows (ReflexModes): {string.Join(", ", ReflexModes.ChallengeSeries().Select(x => x.seriesId + "@" + x.location))}; SeriesStoreProductId {ReflexCareer.SeriesStoreProductId}");
                        Check(script != null && script.Count == 3, "the page lists the 3 challenge series: " + (script == null ? "no script" : script.Count.ToString()));
                        cfm.Navigate(1, 0); CNext();
                    }
                    break;
                case 3: if (age > 1.5) { CaptureFe("challenges_1_right"); cfm.Navigate(-1, 0); CNext(); } break;   // the second is locked on a new save
                case 4: if (age > 1.5) { CaptureFe("challenges_2_first"); cfm.DispatchKey("Accept"); CNext(); } break;
                case 5:   // the Free Ride of the first series
                    if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                    if (runtime && !live) live = runtime.GetComponent<OriginalLiveUi>();
                    if (live && live.Running && runtime.Running && age > 12)
                    {
                        Log("world: " + live.eventKey + ", route " + live.Route + ", mode " + (live.Last?.mode.name ?? "-"));
                        Check(live.Last != null && live.Last.hasMode && live.Last.mode.name == "FreeRide", "the series' Free Ride runs with the game's UI");
                        Capture("challenges_3_freeride"); CFinish();
                    }
                    else if (age > 240) { Check(false, "no Free Ride world"); CFinish(); }
                    break;
            }
        }

        static void CaptureFe(string name)   // the front end's camera and overlay canvases (as CareerTests.Shot)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var main = Camera.main;
            if (main) { c.CopyFrom(main); c.targetTexture = target; c.enabled = false; c.cullingMask = main.cullingMask | 1; }
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(outDir, name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + name);
        }

        // throttle; steer (A 0x1e / D 0x20) toward the race line ~45 units ahead, or a slow weave without one
        static int[] Steer(double now)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(runtime);
            var onboard = typeof(OriginalWorldRuntime).GetField("onboard", BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Public)?.GetValue(runtime);
            float[] line = null; int n = 0;
            if (onboard != null) { line = (float[])onboard.GetType().GetField("raceLine", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard); n = (int)(onboard.GetType().GetField("raceLinePoints", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard) ?? 0); }
            if (st.position == null || line == null || n < 2)   // no race line: straight ahead, or a slow weave ($MODES_WEAVE=1)
                return Env("MODES_WEAVE", "0") != "1" ? new[] { 0x11 } : (now % 6.0) < 1.0 ? new[] { 0x11, 0x1e } : (now % 6.0) > 4.0 ? new[] { 0x11, 0x20 } : new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            Vector3 target = new Vector3(line[3 * (n - 1)], 0, line[3 * (n - 1) + 2]);
            for (int i = 0; i < n; i++) { var p = new Vector3(line[3 * i], 0, line[3 * i + 2]); if ((p - pos).magnitude > 45) { target = p; break; } }
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x, ang = Vector3.Angle(vel, d);
            int key = ang < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        static void Capture(string name)
        {
            const int W = 1600, H = 900;
            var c = runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            if (!c) return;
            var target = new RenderTexture(W, H, 24); target.antiAliasing = 4;
            var prevTarget = c.targetTexture; c.targetTexture = target;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy && x.name.IndexOf("Event list", StringComparison.OrdinalIgnoreCase) < 0).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases();
            c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(W, H, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, W, H), 0, 0); tex.Apply(); RenderTexture.active = prev;
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            c.targetTexture = prevTarget;
            File.WriteAllBytes(Path.Combine(outDir, name + ".png"), tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex); target.Release(); UnityEngine.Object.DestroyImmediate(target);
            Log("shot " + name);
        }
    }
}
