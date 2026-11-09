// Batch play-mode tests of the OPEN REFLEX shell (run headless: -executeMethod Reflex.Port.OpenFrontEndEditor.OpenFrontEndTests.<Test>,
// no -quit; each exits with 0 on PASS). Screenshots of the UI panel (and the original's canvases) go to OPEN_SHOTS or
// <temporaryCachePath>/open-shots, the report beside them.
//   Play    the hub, its navigation (pad / keys through OuMenuInput), RACE's browser (search, filters, favourites, LIST / GRID), the setup
//           page (AI OFF, the RIDER tab), START EVENT: the loading screen and the real hand-over (ReflexEventSelect.Launch, the race
//           scene's world running), QUIT back: the shell again on the event's setup page, the hub's CONTINUE card.
//   Pages   the crash card (DISMISS), OPTIONS (the client's pages over the shell, the shell's input waiting, SYSTEM > FRONT END and
//           GAME FOLDER -> GAME LIBRARY -> back), FRONT END ORIGINAL at the next menu (the original boots, held under the options pages,
//           its F10 key component) and back to OPEN, GAME LIBRARY (the world cache, REBUILD with its progress), the original's
//           OPTIONS page through OriginalEmbed and back,
//           CAREER into the original's own flow, the first-run CONNECT FOLDER (ReflexInstall.SimulateMissing) with the path field.
// The PlayerPrefs the other tests rely on (ReflexEvent, ReflexAI, ReflexOpponents, ReflexCameraMode), the shell's own (OpenReflex.*,
// ReflexFrontEnd) and ReflexGameDir are saved first and restored at the end.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEndEditor
{
    public static class OpenFrontEndTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad; static string shots, testName;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[OpenReflexTest] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }

        // ------------------------------------------------------------------------------------------- prefs
        static readonly string[] StringKeys = { "ReflexEvent", FrontEndSwitch.PrefsKey, ReflexInstall.PrefsKey };
        static readonly string[] IntKeys = { "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i)> saved;
        static Dictionary<string, (bool, string, int)> SavePrefs()
        {
            var keys = StringKeys.Concat(IntKeys).Concat(OpenHistory.Keys()).Concat(OpenContent.Events.Select(e => OpenReflex.Prefs + "Setup." + e.key)).Distinct();
            return keys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue)));
        }
        static void RestorePrefs()
        {
            foreach (var kv in saved)
            {
                if (!kv.Value.has) { PlayerPrefs.DeleteKey(kv.Key); continue; }
                if (IntKeys.Contains(kv.Key) || kv.Key == OpenReflex.Prefs + "View" || kv.Key == OpenReflex.Prefs + "Sort") PlayerPrefs.SetInt(kv.Key, kv.Value.i);
                else PlayerPrefs.SetString(kv.Key, kv.Value.s ?? "");
            }
            PlayerPrefs.Save();
            OpenHistory.ResetCache();
        }

        // ------------------------------------------------------------------------------------------- steps
        sealed class Step { public string name; public Func<bool> run; public double timeout; }
        static readonly List<Step> steps = new List<Step>();
        static int si; static double since; static bool ending;
        static double Age => EditorApplication.timeSinceStartup - since;
        static void S(string name, Func<bool> run, double timeout = 30) => steps.Add(new Step { name = name, run = run, timeout = timeout });
        static void Do(string name, Action a) => S(name, () => { a(); return true; });
        static void Wait(double seconds) => S("wait " + seconds, () => Age >= seconds, seconds + 5);

        static OpenShell Sh => OpenShell.Instance;
        static OuMenuInput In => Sh?.Input;
        static FrontEndManager Fm => UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(FindObjectsInactive.Include);

        static void Begin(string name, Action addSteps)
        {
            testName = name; report.Clear(); bad = 0; steps.Clear(); si = 0; ending = false;
            shots = Environment.GetEnvironmentVariable("OPEN_SHOTS"); if (string.IsNullOrEmpty(shots)) shots = Path.Combine(Application.temporaryCachePath, "open-shots");
            Directory.CreateDirectory(shots);
            saved = SavePrefs();
            Log(name + ": prefs saved (" + saved.Count(kv => kv.Value.has) + " set)");
            if (EditorBuildSettings.scenes.Length < 2 || EditorBuildSettings.scenes[0].path != "Assets/Scenes/FrontEnd.unity") Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.SetBuildOrder();
            EditorSceneManager.OpenScene("Assets/Scenes/FrontEnd.unity", OpenSceneMode.Single);
            FrontEndSwitch.InBatchMode = true;
            PlayerPrefs.SetString(FrontEndSwitch.PrefsKey, "OPEN"); PlayerPrefs.Save();
            addSteps();
            Do("exit play mode", () => { ending = true; EditorApplication.ExitPlaymode(); });
            since = EditorApplication.timeSinceStartup;
            EditorApplication.update += Tick;
            EditorApplication.EnterPlaymode();
        }

        static void Tick()
        {
            if (!EditorApplication.isPlaying)
            {
                if (ending || si >= steps.Count) { End(); return; }
                if (si > 0 || Age > 120) { Check(false, "play mode ended early at step " + (si < steps.Count ? steps[si].name : "?")); End(); }
                return;
            }
            if (si >= steps.Count) return;
            var s = steps[si];
            bool done;
            try { done = s.run(); }
            catch (Exception e) { Check(false, s.name + ": " + e); done = true; }
            if (!done && Age > s.timeout) { Check(false, s.name + ": timed out after " + s.timeout + " s"); done = true; }
            if (done) { si++; since = EditorApplication.timeSinceStartup; }
        }

        static void End()
        {
            EditorApplication.update -= Tick;
            FrontEndSwitch.InBatchMode = false;
            ReflexInstall.SimulateMissing = false;
            OpenSetup.LaunchOverride = null;
            if (Options.ReflexOptions.IsOpen) Options.ReflexOptions.Close();
            if (Options.OptUi.Panel) Options.OptUi.Panel.targetTexture = null;
            if (OuUi.Panel) OuUi.Panel.targetTexture = null;
            RestorePrefs();
            var after = SavePrefs();
            Check(after.All(kv => kv.Value.Equals(saved[kv.Key])), "PlayerPrefs restored");
            Log(testName + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, testName + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ------------------------------------------------------------------------------------------- shots
        static RenderTexture panelRt;
        /// The shell's UI panel into a texture from now on (headless runs have no screen).
        static void CapturePanel()
        {
            if (panelRt == null) { panelRt = new RenderTexture(1920, 1080, 24, RenderTextureFormat.ARGB32) { name = "open-shots" }; panelRt.Create(); }
            var p = OuUi.Panel; p.targetTexture = panelRt; p.clearColor = true; p.colorClearValue = Color.black;
        }
        static void Shot(string name)
        {
            S("shot " + name, () =>
            {
                CapturePanel();
                if (Age < 0.6) return false;
                var prev = RenderTexture.active; RenderTexture.active = panelRt;
                var tex = new Texture2D(1920, 1080, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1920, 1080), 0, 0); tex.Apply(); RenderTexture.active = prev;
                File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
                UnityEngine.Object.DestroyImmediate(tex);
                Log("shot " + name);
                return true;
            });
        }
        /// The original front end's canvases (uGUI) through a camera, as FrontEndContentTests.PlayShot.
        static void CanvasShot(string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + name);
        }

        // ------------------------------------------------------------------------------------------- helpers
        static bool ShellOn(OpenShell.Screen s, double min = 1.0) => Sh && Sh.Current == s && Age > min;
        static VisualElement El(string name) => Sh?.Root?.Q<VisualElement>(name);
        static void Submit(string name) { var e = El(name); if (e?.userData is OuNavItem it) { it.submit?.Invoke(); } else throw new Exception("no item " + name); }
        static void Adjust(string name, int d) { var e = El(name); if (e?.userData is OuNavItem it && it.adjust != null) it.adjust(d); else throw new Exception("no value row " + name); }
        static string Focused => Sh?.FocusedText;
        const string Concord = "National/NAT_Concord";

        // ------------------------------------------------------------------------------------------- Play
        public static void Play() => Begin("Play", () =>
        {
            int raceEvents = 0, loadLogs = 0;
            S("shell up on the hub", () =>
            {
                if (!ShellOn(OpenShell.Screen.Main, 2)) return false;
                var fm = Fm;
                Check(fm && !fm.enabled && fm.assets == null, "the original front end is in the scene and did not boot");
                Check(El("ou-hub-play") != null && El("ou-hub-online") == null && El("ou-hub-garage") != null && El("ou-hub-options") != null
                      && El("ou-hub-library") != null && El("ou-hub-physics") != null && El("ou-hub-credits") != null && El("ou-hub-quit") != null
                      && El("ou-hub-race") == null && El("ou-hub-career") == null, "hub: PLAY, GARAGE, OPTIONS, GAME LIBRARY, PHYSICS STUDIO, CREDITS, QUIT (the single player rows are on their own page)");
                Check(El("ou-continue") != null, "hub: CONTINUE / QUICK START card");
                Check(Focused == "PLAY", "hub focus starts on PLAY: " + Focused);
                raceEvents = OpenContent.Events.Count(e => !e.freeRide);
                Log($"events: {OpenContent.Events.Count} ({raceEvents} race, {OpenContent.Events.Count - raceEvents} free ride); types {string.Join(", ", OpenContent.Types(false).Select(t => t.name + " " + t.count))}");
                Check(OpenContent.Events.Count >= 31, "at least the 31 base events");
                Check(OuSound.Player != null && OuSound.Player.Count > 0, "Reflex's UI sounds from the install (" + (OuSound.Player?.Count ?? 0) + " UI sounds)");
                return true;
            }, 90);
            Shot("01_hub");
            Do("pad down", () => In.injectStep = new Vector2Int(0, -1));
            S("focus GARAGE", () => { if (Age < 0.3) return false; Check(Focused == "GARAGE", "down: " + Focused); return true; });
            Do("pad down x2", () => In.injectStep = new Vector2Int(0, -1));
            Wait(0.2); Do("", () => In.injectStep = new Vector2Int(0, -1)); Wait(0.3);
            S("down from OPTIONS lands on GAME LIBRARY", () => { Check(Focused == "GAME LIBRARY", "down x3: " + Focused); return true; });
            Do("up", () => In.injectStep = new Vector2Int(0, 1)); Wait(0.3);
            S("back on OPTIONS", () => { Check(Focused == "OPTIONS", "up: " + Focused); return true; });
            Do("cancel on the hub does nothing", () => In.injectCancel = true); Wait(0.3);
            S("still the hub", () => { Check(Sh.Current == OpenShell.Screen.Main, "Esc on the hub stays"); return true; });
            Do("PLAY", () => Submit("ou-hub-play"));
            S("single player page", () =>
            {
                if (!ShellOn(OpenShell.Screen.Play, 0.5)) return false;
                Check(El("ou-hub-career") != null && El("ou-hub-race") != null && El("ou-hub-freeride") != null && El("ou-hub-back") != null, "PLAY: CAREER, RACE, FREE RIDE, BACK");
                Check(Focused == "CAREER", "PLAY focus starts on CAREER: " + Focused);
                return true;
            });
            Do("cancel goes back to the hub", () => In.injectCancel = true); Wait(0.3);
            S("the hub again", () => { Check(Sh.Current == OpenShell.Screen.Main, "Esc on PLAY: back on the hub"); return true; });
            Do("PLAY", () => Submit("ou-hub-play"));
            Wait(0.3);
            Do("RACE", () => Submit("ou-hub-race"));
            S("browser", () =>
            {
                if (!ShellOn(OpenShell.Screen.Browser, 1.2)) return false;
                Sh.LoadAllThumbnails();
                Check(!Sh.FreeRide, "RACE browser");
                Log("browser filter " + Sh.Browse + ", " + Sh.VisibleCards + " cards, selected " + Sh.SelectedEvent?.key);
                return true;
            });
            Do("ALL EVENTS", () => { Sh.SelectBrowse("*all"); Sh.LoadAllThumbnails(); });
            S("all race events", () => { Check(Sh.VisibleCards == raceEvents, $"ALL EVENTS: {Sh.VisibleCards} cards of {raceEvents}"); return true; });
            Shot("02_browser_all");
            Do("search concord", () => Sh.SetQuery("concord"));
            S("search", () =>
            {
                if (Age < 0.3) return false;
                var evs = Sh.CardEvents.ToList();
                Check(evs.Count >= 1 && evs.All(e => e.haystack.Contains("concord")), "search 'concord': " + string.Join(", ", evs.Select(e => e.key)));
                return true;
            });
            Shot("03_search");
            Do("clear search", () => Sh.SetQuery(""));
            Do("NATIONAL", () => { Sh.SelectBrowse("National"); Sh.LoadAllThumbnails(); });
            S("national filter", () => { var evs = Sh.CardEvents.ToList(); Check(evs.Count > 0 && evs.All(e => e.activity == "National"), "NATIONAL filter: " + evs.Count + " cards"); return true; });
            Do("tab: next type", () => In.injectTab = 1); Wait(0.3);
            S("tab", () => { Check(Sh.Browse != "National" && !Sh.Browse.StartsWith("*"), "R1 steps the event type: " + Sh.Browse); return true; });
            Do("back to NATIONAL", () => Sh.SelectBrowse("National"));
            Do("select Concord", () => Check(Sh.SelectEvent(Concord), "Concord's card"));
            Do("favourite", () => In.injectAlt = true); Wait(0.3);
            S("favourite on", () => { Check(OpenHistory.IsFavourite("NAT_Concord"), "F / Y stars the location"); return true; });
            Do("FAVORITES", () => Sh.SelectBrowse("*fav"));
            S("favourites", () => { Check(Sh.CardEvents.Any(e => e.Location == "NAT_Concord"), "FAVORITES lists Concord's events: " + Sh.VisibleCards); return true; });
            Do("LIST", () => { Submit("ou-view"); Sh.LoadAllThumbnails(); });
            Shot("04_favourites_list");
            Do("GRID", () => Submit("ou-view"));
            Do("NATIONAL + Concord", () => { Sh.SelectBrowse("National"); Sh.SelectEvent(Concord); });
            Wait(0.3);
            Do("enter", () => In.injectSubmit = true);
            S("setup page", () =>
            {
                if (!ShellOn(OpenShell.Screen.Setup, 0.8)) return false;
                Check(Sh.SelectedEvent?.key == Concord, "setup page of " + Sh.SelectedEvent?.key);
                Check(El("ou-setup-ai") != null && El("ou-setup-start") != null, "AI OPPONENTS row and START EVENT");
                Log("setup: " + Sh.Setup.ToJson() + ", laps " + Sh.Setup.Laps(Sh.SelectedEvent.ev));
                return true;
            });
            Shot("05_setup_event");
            Do("AI off", () => { while (Sh.Setup.Ai(Sh.SelectedEvent.ev) != 0) Adjust("ou-setup-ai", -1); });
            S("AI is off", () => { Check(Sh.Setup.ai == 0, "AI OFF"); return true; });
            Do("RIDER tab", () => In.injectTab = 1); Wait(0.6);
            S("rider tab", () => { Check(El("ou-setup-vehicle") != null && El("ou-setup-rider") != null, "RIDER tab: VEHICLE and RIDER rows"); return true; });
            Shot("06_setup_rider");
            Do("vehicle: ATV and back", () =>
            {
                var e = Sh.SelectedEvent.ev;
                Adjust("ou-setup-vehicle", 1);
                Log("vehicle " + OpenContent.VehicleTypeName(Sh.Setup.Type(e)) + ", rider " + Sh.Setup.Rider(e) + ", garage " + Sh.Setup.Garage(e));
                Check(Sh.Setup.Garage(e) != null, "another vehicle: a Vehicle Select garage");
                while (Sh.Setup.Type(e) != OpenSetup.DefaultType(e)) Adjust("ou-setup-vehicle", 1);
                Check(Sh.Setup.Garage(e) == null, "the MX Lite with your rider: the profile's own (no garage)");
            });
            Do("START EVENT", () => Submit("ou-setup-start"));
            S("loading screen", () => { if (!OpenLoading.Current || Age < 0.8) return Age > 20 && Fail("no loading screen"); return true; });
            Shot("07_loading");
            S("race world running", () =>
            {
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var sel = UnityEngine.Object.FindAnyObjectByType<ReflexEventSelect>();
                if (OpenLoading.Current && OpenLoading.Current.SawBuild && (int)(Age / 10) > loadLogs) { loadLogs = (int)(Age / 10); Log("loading: " + OpenLoading.Current.Line); }
                if (!(rt && rt.Running && !OpenLoading.Current && Age > 3)) return false;
                Check(sel && sel.Current == Concord, "race scene event " + (sel ? sel.Current : "?"));
                Check(rt.opponents == 0, "race scene opponents " + rt.opponents + " (AI off)");
                if (!Options.ReflexSave.Has("Hud.Style")) Check(Options.ReflexSettings.HudStyle == Options.HudStyle.Modern, "HUD STYLE never chosen: MODERN with the OPEN REFLEX front end");
                else Log("HUD STYLE chosen in this save: " + Options.ReflexSettings.HudStyle);
                Check(PlayerPrefs.GetString("ReflexEvent", "") == Concord && PlayerPrefs.GetInt("ReflexAI", -1) == 0, "remembered like the original (ReflexEvent / ReflexAI)");
                return true;
            }, 300);
            Wait(3);
            Do("QUIT to the front end", () => OriginalPauseMenu.QuitToFrontEnd());
            S("back on the setup page", () =>
            {
                if (!ShellOn(OpenShell.Screen.Setup, 1.5)) return false;
                Check(Sh.SelectedEvent?.key == Concord, "QUIT comes back to the event's setup page: " + Sh.SelectedEvent?.key);
                var fm = Fm; Check(fm && !fm.enabled, "the original front end stays off");
                return true;
            }, 60);
            Shot("08_back_from_race");
            Do("back", () => In.injectCancel = true);
            S("browser again", () => ShellOn(OpenShell.Screen.Browser, 0.5));
            Do("back", () => In.injectCancel = true);
            S("hub again", () =>
            {
                if (!ShellOn(OpenShell.Screen.Main, 0.8)) return false;
                var cont = El("ou-continue");
                Check(cont?.Q<Label>()?.text == "RIDE NOW", "CONTINUE: RIDE NOW");
                string loc = OpenContent.Find(Concord)?.locationName;
                Check(Sh.Root.Query<Label>(className: "ou-continue-title").ToList().Any(l => l.text == loc), "CONTINUE shows Concord's location: " + loc);
                return true;
            });
            Shot("09_hub_continue");
            Do("FREE RIDE", () => { Sh.ShowPlay(); Submit("ou-hub-freeride"); });
            S("free ride browser", () =>
            {
                if (!ShellOn(OpenShell.Screen.Browser, 1)) return false;
                Sh.LoadAllThumbnails();
                Check(Sh.FreeRide && Sh.CardEvents.All(e => e.freeRide) && Sh.VisibleCards > 0, "FREE RIDE browser: " + Sh.VisibleCards + " free rides");
                return true;
            });
            Shot("10_freeride");
            Do("setup", () => In.injectSubmit = true);
            S("free ride setup", () =>
            {
                if (!ShellOn(OpenShell.Screen.Setup, 0.8)) return false;
                Check(El("ou-setup-ai") == null, "free ride: no AI row");
                return true;
            });
            Shot("11_freeride_setup");
            Do("favourite off", () => { if (OpenHistory.IsFavourite("NAT_Concord")) OpenHistory.ToggleFavourite("NAT_Concord"); });
        });

        static bool Fail(string why) { Check(false, why); return true; }

        static void SetCrashed(bool on)
        {
            typeof(Reflex.Port.Platform.ReflexCrashLog).GetProperty("PreviousCrashed").SetValue(null, on);
            Reflex.Port.Platform.ReflexCrashLog.PromptDismissed = false;
        }
        static VisualElement OptEl(string name) => Options.ReflexOptions.Root?.Q<VisualElement>(name);
        static void OptSubmit(string name) { if (OptEl(name)?.userData is Options.OptNavItem it && it.submit != null) it.submit(); else throw new Exception("no options item " + name); }
        static void OptAdjust(string name, int d) { if (OptEl(name)?.userData is Options.OptNavItem it && it.adjust != null) it.adjust(d); else throw new Exception("no options row " + name); }
        /// The options pages' own panel (Reflex.Port.Options.OptUi.Panel, sorted over the shell) into a texture.
        static void OptShot(string name)
        {
            S("shot " + name, () =>
            {
                var p = Options.OptUi.Panel;
                if (optRt == null || p.targetTexture != optRt) { p.targetTexture = optRt ??= new RenderTexture(1920, 1080, 24, RenderTextureFormat.ARGB32); p.clearColor = true; p.colorClearValue = Color.black; }
                if (Age < 0.8) return false;
                var prev = RenderTexture.active; RenderTexture.active = optRt;
                var tex = new Texture2D(1920, 1080, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1920, 1080), 0, 0); tex.Apply(); RenderTexture.active = prev;
                File.WriteAllBytes(Path.Combine(shots, name + ".png"), tex.EncodeToPNG());
                UnityEngine.Object.DestroyImmediate(tex);
                Log("shot " + name);
                return true;
            });
        }
        static RenderTexture optRt;


        // ------------------------------------------------------------------------------------------- Shell
        public static void Pages() => Begin("Pages", () =>
        {
            S("hub", () => ShellOn(OpenShell.Screen.Main, 2), 90);
            Do("crash card", () => SetCrashed(true));
            Do("hub again", () => Sh.ShowMain());
            S("crash card shown", () =>
            {
                if (Age < 0.5) return false;
                Check(El("ou-crash-card") != null && El("ou-crash-report") != null && El("ou-crash-dismiss") != null, "the previous session crashed: CREATE SUPPORT REPORT / DISMISS on the hub");
                return true;
            });
            Shot("19_crash_card");
            Do("DISMISS", () => Submit("ou-crash-dismiss"));
            S("crash card gone", () => { if (Age < 0.3) return false; Check(El("ou-crash-card") == null && Reflex.Port.Platform.ReflexCrashLog.PromptDismissed, "DISMISS"); SetCrashed(false); return true; });
            Do("OPTIONS", () => Submit("ou-hub-options"));
            S("the options pages", () =>
            {
                if (!(Options.ReflexOptions.IsOpen && Age > 0.6)) return false;
                Check(Options.ReflexOptionsHooks.OpenGameLibrary != null, "ReflexOptionsHooks.OpenGameLibrary set by the shell");
                Check(Options.ReflexOptions.Root?.Q<Label>(className: "mxa-row-label") != null, "the options pages up (ReflexOptions.Open)");
                return true;
            });
            Do("pad down under the options", () => In.injectStep = new Vector2Int(0, -1));
            S("the shell waits", () => { if (Age < 0.3) return false; Check((Focused == null || Focused == "OPTIONS") && Sh.Current == OpenShell.Screen.Main, "the shell's input waits while the options are up (the options panel has the focus; shell focus: " + (Focused ?? "none") + ")"); return true; });
            OptShot("20_options_controls");
            Do("SYSTEM", () => Options.ReflexOptions.ShowTab(Options.ReflexOptions.System));
            OptShot("20b_options_system");
            Do("FRONT END: ORIGINAL", () => OptAdjust("front-end", 1));
            S("saved", () => { Check(PlayerPrefs.GetString(FrontEndSwitch.PrefsKey) == "ORIGINAL" && Sh.Current == OpenShell.Screen.Main, "OPTIONS > SYSTEM > FRONT END ORIGINAL saved, applied next time"); return true; });
            Do("GAME FOLDER", () => OptSubmit("game-folder"));
            S("library from options", () =>
            {
                if (!ShellOn(OpenShell.Screen.Library, 0.6)) return false;
                Check(!Options.ReflexOptions.IsOpen, "GAME FOLDER closes the options and opens GAME LIBRARY");
                return true;
            });
            Do("DONE", () => Submit("ou-library-done"));
            S("options again", () =>
            {
                if (!(Options.ReflexOptions.IsOpen && Age > 0.5)) return false;
                Check(Options.ReflexOptions.Tab == Options.ReflexOptions.System && Sh.Current == OpenShell.Screen.Main, "DONE: back to OPTIONS > SYSTEM");
                Options.ReflexOptions.Close();
                return true;
            });
            Wait(0.3);
            Do("pad down", () => In.injectStep = new Vector2Int(0, -1));
            S("the shell's input back", () => { if (Age < 0.3) return false; Check(Focused == "GAME LIBRARY", "after the options close the shell navigates again: " + Focused); return true; });
            Do("menu again (ORIGINAL)", () => FrontEndSwitch.Reload());
            S("the original front end boots", () =>
            {
                var fm = Fm;
                if (!(fm && fm.enabled && fm.assets != null && fm.CurrentPage != null && Age > 2 && !fm.Busy)) return false;
                Check(OpenShell.Instance == null, "no shell in ORIGINAL");
                Check(fm.GetComponent<OriginalStyleKey>() != null, "the original's F10 key (OriginalStyleKey)");
                Log("original at " + fm.CurrentPage);
                CanvasShot("21_original");
                Options.ReflexOptions.Open(Options.ReflexOptions.Audio);
                return true;
            }, 60);
            S("the original held under the options", () =>
            {
                if (Age < 0.5) return false;
                var fm = Fm;
                Check(!fm.enabled && fm.GetComponent<OriginalOptionsGate>().Holding, "the original front end's input is held while the options are up");
                Options.ReflexOptions.Close();
                return true;
            });
            S("the original let go", () => { if (Age < 0.5) return false; Check(Fm.enabled && !Fm.GetComponent<OriginalOptionsGate>().Holding, "and let go after they close"); return true; });
            Do("F10: OPEN", () => FrontEndSwitch.Set(FrontEndStyle.Open, true));
            S("shell again", () => ShellOn(OpenShell.Screen.Main, 1.5), 60);
            Do("GAME LIBRARY", () => Submit("ou-hub-library"));
            OpenWorldCache.Entry concord = null;
            S("library", () =>
            {
                if (!ShellOn(OpenShell.Screen.Library, 0.6)) return false;
                var list = OpenWorldCache.List();
                Log("world cache: " + string.Join(", ", list.Select(x => x.name + " " + OpenWorldCache.SizeText(x.bytes))));
                concord = list.FirstOrDefault(x => x.Rebuildable && x.eventKey == Concord) ?? list.FirstOrDefault(x => x.Rebuildable);
                Check(El("ou-library-folder") != null, "the Reflex install's card");
                return true;
            });
            Shot("22_library");
            S("REBUILD", () =>
            {
                if (concord == null) { Log("no rebuildable world in the cache (run Play first): REBUILD not tried"); return true; }
                Sh.Rebuild(concord); return true;
            });
            S("building", () => { if (concord == null) return true; return Sh.BuildRunning && Age > 3; }, 20);
            Shot("23_rebuild_progress");
            S("rebuilt", () =>
            {
                if (concord == null || Sh.BuildRunning) return concord == null;
                Check(OpenWorldCache.List().Any(x => x.name == concord.name), "REBUILD made the world again: " + concord.name);
                return true;
            }, 240);
            Shot("24_library_after");
            Do("DONE", () => Submit("ou-library-done"));
            S("hub", () => ShellOn(OpenShell.Screen.Main, 0.5));
            Do("the original's options page (OriginalEmbed)", () => Sh.OpenOriginal(OriginalEmbed.Purpose.Options, "Options Page"));
            S("the original's options page", () =>
            {
                var fm = Fm;
                if (!(fm && fm.enabled && fm.assets != null && !fm.Busy && fm.CurrentPage == "Options Page" && Age > 1.5)) return false;
                Check(fm.GetComponent<OriginalEmbed>() != null, "OriginalEmbed on the original");
                CanvasShot("25_original_options");
                fm.DispatchKey("Back");
                return true;
            }, 60);
            S("back to the shell from the original", () => ShellOn(OpenShell.Screen.Main, 1), 60);
            Do("CAREER", () => { Sh.ShowPlay(); Submit("ou-hub-career"); });
            S("career: the original's own flow", () =>
            {
                var fm = Fm;
                if (!(fm && fm.enabled && fm.assets != null && fm.CurrentPage != null && Age > 3 && !fm.Busy)) return false;
                var emb = fm.GetComponent<OriginalEmbed>();
                Check(emb != null && emb.purpose == OriginalEmbed.Purpose.Career, "CAREER hands over to the original (" + fm.CurrentPage + (fm.TopPopup != null ? " / " + fm.TopPopup : "") + ")");
                CanvasShot("26_career_original");
                return true;
            }, 90);
            Wait(12);
            Do("career after the title", () =>
            {
                var fm = Fm;
                Log("career: the original at " + fm.CurrentPage + (fm.TopPopup != null ? " / " + fm.TopPopup : "") + ", profile " + (ReflexProfile.Current != null ? (ReflexProfile.Current.signedIn ? "signed in" : "not signed in") + (ReflexProfile.Current.MotocardSetup ? ", motocard set up" : ", no motocard") : "none"));
                Check(fm.CurrentPage != "Title Page" || fm.TopPopup != null, "the title's Accept went on (the original's sign-in / MOTOCARD flow)");
                CanvasShot("26b_career_after_title");
            });
            Do("back to the shell", () => { FrontEndManager.ReturnPage = null; FrontEndSwitch.Reload(); });
            S("shell", () => ShellOn(OpenShell.Screen.Main, 1), 60);
            Do("GARAGE", () => Submit("ou-hub-garage"));
            S("garage: the original", () =>
            {
                var fm = Fm;
                if (!(fm && fm.enabled && fm.assets != null && fm.CurrentPage != null && Age > 3 && !fm.Busy)) return false;
                var emb = fm.GetComponent<OriginalEmbed>();
                Check(emb != null && emb.purpose == OriginalEmbed.Purpose.Garage, "GARAGE hands over to the original (" + fm.CurrentPage + ")");
                return true;
            }, 90);
            Do("back to the shell", () => { FrontEndManager.ReturnPage = null; FrontEndSwitch.Reload(); });
            S("shell", () => ShellOn(OpenShell.Screen.Main, 1), 60);
            string dir = null;
            Do("no install (first run)", () => { dir = ReflexInstall.Dir; ReflexInstall.SimulateMissing = true; FrontEndSwitch.Reload(); });
            S("CONNECT FOLDER", () =>
            {
                if (!ShellOn(OpenShell.Screen.Install, 1)) return false;
                Check(El("ou-connect-folder") != null && El("ou-install-use") != null, "first run: CONNECT FOLDER");
                Sh.SetInstallPath(Path.GetTempPath());
                return true;
            }, 60);
            S("not an install", () => { if (Age < 0.3) return false; var st = Sh.Root.Q<Label>("ou-install-status"); Check(st != null && !st.ClassListContains("mxa-ok"), "a folder without the game: " + st?.text); Submit("ou-install-use"); return true; });
            S("still connect", () => { if (Age < 0.3) return false; Check(Sh.Current == OpenShell.Screen.Install && ReflexInstall.SimulateMissing, "USE THIS FOLDER refuses it"); Sh.SetInstallPath(Path.Combine(dir, "Database")); return true; });
            S("install found", () => { if (Age < 0.3) return false; var st = Sh.Root.Q<Label>("ou-install-status"); Check(st != null && st.ClassListContains("mxa-ok"), "the Database folder pasted: its install (" + st?.text + ")"); return true; });
            Shot("27_connect_folder");
            Do("USE THIS FOLDER", () => Submit("ou-install-use"));
            S("library after connect", () =>
            {
                if (!ShellOn(OpenShell.Screen.Library, 0.6)) return false;
                Check(!ReflexInstall.SimulateMissing && ReflexInstall.Dir != null && PlayerPrefs.GetString(ReflexInstall.PrefsKey, "") == Path.GetFullPath(dir), "saved through ReflexInstall.Set: " + PlayerPrefs.GetString(ReflexInstall.PrefsKey, ""));
                return true;
            });
            Shot("28_library_connected");
        });
    }
}
