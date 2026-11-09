using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // WP-5: the in-race pages (_re/notes/in-race-pages.md).
    //   Play: play mode on $PAGES_EVENT (National/NAT_Concord). $PAGES_LIVE=1 (default): the game's own UI live (OriginalLiveUi) - the Event
    //         Intro page, no pause on it (App.AllowPause), Esc to the game (OriginalPauseMenu.GameOwnsPause) which shows its own Paused Menu
    //         Page (rows, TRICK LIBRARY with the stunt list, OPTIONS) and resumes. $PAGES_LIVE=0: the race without the live UI (as a
    //         MOTOCAREER race): the port's Event Intro page, its pause page with the world's rows (PauseMenu:Start in the world) and the
    //         trick library. $PAGES_POST=1: on to the finish (1 lap, race-line steering): the post-race pages open only after the post
    //         scenarios (the Event Outro page). $PAGES_FREERIDE=1 with FreeRide/FR_Alpine: the game's VEHICLE LIST -> the reload.
    //         Shots to $PAGES_SHOTS. PlayerPrefs: the user's keys are checked and restored.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.PagesTests.Play (no -quit)
    public static class PagesTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Pages] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;

        static string eventKey, outDir; static bool liveRun, postRun, freeRide;
        static double started, runAt = -1, stepAt; static int step;
        static OriginalWorldRuntime runtime; static OriginalEventIntro pages; static OriginalLiveUi live;
        static readonly List<(double t, int[] keys)> keyQueue = new List<(double, int[])>();
        static double postAt = -1; static bool introSeen, introPauseBlocked; static OriginalWorldRuntime reloadFrom;

        public static void Play()
        {
            report.Clear(); bad = 0; step = 0; runAt = -1; postAt = -1; introSeen = introPauseBlocked = false; runtime = null; pages = null; live = null; keyQueue.Clear();
            eventKey = Env("PAGES_EVENT", "National/NAT_Concord"); liveRun = Env("PAGES_LIVE", "1") == "1"; postRun = Env("PAGES_POST", "0") == "1"; freeRide = Env("PAGES_FREERIDE", "0") == "1";
            outDir = Env("PAGES_SHOTS", Path.Combine(Application.temporaryCachePath, "pages-shots")); Directory.CreateDirectory(outDir);
            saved = UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
            OriginalHud.LiveRaces = liveRun;
            lock (OriginalEventIntro.VoiceLog) OriginalEventIntro.VoiceLog.Clear();
            ReflexEventSelect.Pending = eventKey; ReflexEventSelect.PendingAi = 0; ReflexEventSelect.PendingLaps = postRun ? 1 : 0;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += Tick;
            started = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }

        static string Tag => (liveRun ? "live" : "port") + "_" + eventKey.Replace('/', '_');
        static void Keys(double at, params int[] k) => keyQueue.Add((at, k));

        static void Tick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - started > double.Parse(Env("PAGES_TIMEOUT", "900"), CultureInfo.InvariantCulture)) { Check(false, "timeout at step " + step); Finish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime || !runtime.Running) return;
            if (!pages) pages = runtime.GetComponent<OriginalEventIntro>();
            if (!live) live = runtime.GetComponent<OriginalLiveUi>();
            if (!pages || !pages.HasState) return;
            if (runAt < 0) { runAt = now; stepAt = now; Log("world running; live UI " + (live ? "attached" : "none")); }
            // queued key presses (held for 0.15 s each, as the original reads DirectInput state)
            var held = new List<int>();
            foreach (var (t, k) in keyQueue) if (now >= t && now < t + 0.15) held.AddRange(k);
            bool steering = step >= 100 && runtime.Race.state >= 2 && runtime.Race.state < 4;
            if (steering) held.AddRange(Steer());
            runtime.testHeldKeys = held.ToArray();
            var st = pages.Current;
            if (st.race == 0 && !introSeen && now - runAt > double.Parse(Env("PAGES_INTROAT", "6"), CultureInfo.InvariantCulture))
            {
                introSeen = true; introPauseBlocked = !pages.PauseAllowed;
                if (liveRun) Check(live && live.Shows("Event Intro Page"), "the game's Event Intro page shows (live)");
                else Check(pages.IntroShown && !string.IsNullOrEmpty(pages.IntroEventText), $"the port's Event Intro page: '{pages.IntroEventText}' / '{pages.IntroLocationText}'");
                Check(introPauseBlocked, "no pause on the Event Intro page (App.AllowPause)");
                Shot("01_event_intro");
            }
            double age = now - stepAt;
            void Next() { step++; stepAt = now; }
            switch (step)
            {
                case 0: if (st.race >= 1 && age > 1) { Next(); } break;
                case 1:
                    if (age < 3) break;
                    Check(pages.PauseAllowed, "pause allowed once the HUD page runs (countdown)");
                    Check(pages.PauseRowsReady, "PauseMenu:Start's rows from the world: " + string.Join(", ", pages.PauseRows));
                    Check(pages.StuntsReady && pages.Stunts.Count > 20, "the trick library from the world: " + pages.Stunts.Count + " stunts");
                    if (liveRun)
                    {
                        Check(OriginalPauseMenu.GameOwnsPause(runtime), "Esc goes to the game (its UI runs live)");
                        Keys(now + 0.2, 0x01);   // Esc: MapKey_UI_Pause1
                    }
                    else
                    {
                        Check(!OriginalPauseMenu.GameOwnsPause(runtime), "Esc opens the port's pause page (no live UI)");
                        runtime.pauseMenu.Open();
                    }
                    Next(); break;
                case 2:
                    if (age < 1.5) break;
                    if (liveRun) Check(live.Shows("Paused Menu Page"), "the game's own Paused Menu Page shows (live)");
                    else
                    {
                        var expect = eventKey.StartsWith("FreeRide") ? "STR_RESUME_GAME,STR_GARAGE_VEHICLE_TUNE,STR_QUICKVIEW,STR_STUNTS,STR_OPTIONS,STR_QUIT" : "STR_RESUME_GAME,STR_RESTART_GAME,STR_STUNTS,STR_OPTIONS,STR_QUIT";
                        Check(runtime.pauseMenu.IsOpen && string.Join(",", runtime.pauseMenu.RowKeys) == expect, "the port's pause rows: " + string.Join(",", runtime.pauseMenu.RowKeys));
                    }
                    Shot("02_pause");
                    if (liveRun) { int n = eventKey.StartsWith("FreeRide") ? 3 : 2; for (int i = 0; i < n; i++) Keys(now + 0.4 * (i + 1), 0xd0); Keys(now + 0.4 * (n + 1), 0x1c); }
                    else
                    {   // highlight TRICK LIBRARY, then accept it (OnAcceptStunts)
                        typeof(OriginalPauseMenu).GetMethod("Focus", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(runtime.pauseMenu, new object[] { runtime.pauseMenu.RowKeys.ToList().IndexOf("STR_STUNTS") });
                        typeof(OriginalPauseMenu).GetMethod("FocusStunts", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(runtime.pauseMenu, new object[] { true });
                    }
                    Next(); break;
                case 3:
                    if (age < 2.5) break;
                    if (liveRun)
                    {
                        string tier = live.Text("PauseMenuLayer", "StuntTierComponent");
                        Check(tier == "Tier 1", "TRICK LIBRARY (the game's StuntPreview) shows, tier '" + tier + "'");
                    }
                    else Check(runtime.pauseMenu.StuntsFocused && runtime.pauseMenu.StuntCount > 20 && runtime.pauseMenu.Tier == "Tier 1", $"TRICK LIBRARY: {runtime.pauseMenu.StuntCount} stunts, tier '{runtime.pauseMenu.Tier}'");
                    Shot("03_trick_library");
                    if (liveRun) { Keys(now + 0.3, 0x01); Keys(now + 0.8, 0xd0); }   // back, OPTIONS
                    else typeof(OriginalPauseMenu).GetMethod("Focus", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(runtime.pauseMenu, new object[] { runtime.pauseMenu.RowKeys.ToList().IndexOf("STR_OPTIONS") });
                    Next(); break;
                case 4:
                    if (age < 2) break;
                    Shot("04_options");
                    if (liveRun && freeRide)   // up to VEHICLE LIST, open the quick view, accept the first vehicle
                    {
                        for (int i = 0; i < 2; i++) Keys(now + 0.4 * (i + 1), 0xc8);
                        Keys(now + 1.4, 0x1c);
                        step = 50; stepAt = now; break;
                    }
                    if (liveRun) Keys(now + 0.3, 0x01); else runtime.pauseMenu.Close();
                    Next(); break;
                case 5:
                    if (age < 2) break;
                    if (liveRun) Check(!live.Shows("Paused Menu Page"), "Esc resumed (the HUD page again)");
                    else Check(!runtime.pauseMenu.IsOpen, "the port's pause page closed");
                    if (!postRun) { Finish(); return; }
                    step = 100; stepAt = now; break;
                case 50:   // the game's quick view
                    if (age < 2) break;
                    Check(live.Shows("VehiclesQuickViewPopup"), "VEHICLE LIST: the game's VehiclesQuickViewPopup (live)");
                    Shot("05_vehicle_list");
                    Keys(now + 0.5, 0xd0); Keys(now + 1.2, 0x1c);
                    Next(); break;
                case 51:
                    if (age < 4) break;
                    Check(OriginalQuickView.LastChoice != null, "the quick view's choice reached the port: " + OriginalQuickView.LastChoice + " (" + OriginalLiveUi.LastRequest + ")");
                    Next(); break;
                case 52:   // the Free Ride again, booted with the chosen vehicle
                    if (runtime.garage == null && age < 300) break;   // the world of the reload (the scene loads again)
                    if (age < 6) break;
                    var gsel = runtime.garage;
                    Check(gsel != null && gsel.vehicleId == 42 && gsel.vehicleOwner == ReflexWorldFiles.Garage.AI, "the Free Ride world again with the quick view's vehicle: " + (gsel == null ? "no garage" : "owner " + gsel.vehicleOwner + " vehicle " + gsel.vehicleId + " rider " + gsel.riderId));
                    Shot("06_after_vehicle_list"); Finish(); return;
                case 100:   // to the finish, then the post scenarios before the port's post pages
                    if (st.race == 4 && postAt < 0) { postAt = now; Log("race state Post; post sub-state " + st.post); }
                    if (postAt > 0 && OriginalPostRace.Active)
                    {
                        double wait = now - postAt;
                        Check(wait > 3.5, $"the post-race pages opened {wait:F1} s after Post (after the Event Outro page's post scenarios)");
                        Shot("06_post_results"); Finish(); return;
                    }
                    if (postAt > 0 && now - postAt > 2 && now - postAt < 2.1) Shot("05_event_outro");
                    if (postAt > 0 && now - postAt > 40) { Check(false, "no post-race pages 40 s after Post"); Finish(); return; }
                    break;
            }
        }

        static int[] Steer()
        {
            var t = typeof(HudTests);
            t.GetField("runtime", BindingFlags.NonPublic | BindingFlags.Static).SetValue(null, runtime);
            return (int[])t.GetMethod("Steer", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { EditorApplication.timeSinceStartup });
        }

        // The 3D view through the game camera (its post chain), then the overlay canvases alone through a camera without post, composited:
        // a ScreenSpaceCamera canvas on the game camera would go through the Reflex tonemap (it draws before the post chain).
        static void Shot(string name)
        {
            const int W = 1280, H = 720;
            var c = runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            if (!c) return;
            var scene = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            var prevTarget = c.targetTexture; c.targetTexture = scene; c.Render(); c.targetTexture = prevTarget;
            var a = Read(scene, W, H); scene.Release();
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>().Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy && x.name.IndexOf("Event list", StringComparison.OrdinalIgnoreCase) < 0).OrderBy(x => x.sortingOrder).ToList();
            var ui = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            var cgo = new GameObject("pages shot ui camera"); var uc = cgo.AddComponent<Camera>();
            uc.clearFlags = CameraClearFlags.SolidColor; uc.backgroundColor = new Color(0, 0, 0, 0); uc.cullingMask = 1 << 31; uc.allowHDR = false; uc.targetTexture = ui;
            var layers = new List<(GameObject, int)>();
            foreach (var o in overlays) { foreach (var t in o.GetComponentsInChildren<Transform>(true)) { layers.Add((t.gameObject, t.gameObject.layer)); t.gameObject.layer = 31; } o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = uc; o.planeDistance = 1f - o.sortingOrder * 0.0005f; }
            Canvas.ForceUpdateCanvases(); uc.Render();
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            foreach (var (go, l) in layers) go.layer = l;
            var b = Read(ui, W, H); ui.Release(); UnityEngine.Object.DestroyImmediate(cgo);
            var pa = a.GetPixels32(); var pb = b.GetPixels32();
            for (int i = 0; i < pa.Length; i++) { float t = pb[i].a / 255f; pa[i] = new Color32((byte)(pb[i].r + pa[i].r * (1 - t)), (byte)(pb[i].g + pa[i].g * (1 - t)), (byte)(pb[i].b + pa[i].b * (1 - t)), 255); }
            a.SetPixels32(pa); a.Apply();
            string path = Path.Combine(outDir, Tag + "_" + name + ".png");
            File.WriteAllBytes(path, a.EncodeToPNG());
            Log("shot " + path);
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
            OriginalHud.LiveRaces = true;
            lock (OriginalEventIntro.VoiceLog) if (OriginalEventIntro.VoiceLog.Count > 0) Log("voice: " + string.Join("; ", OriginalEventIntro.VoiceLog));
            foreach (var kv in saved)
            {
                var (had, s, i, f) = kv.Value; bool now = PlayerPrefs.HasKey(kv.Key);
                bool changed = now != had || had && (PlayerPrefs.GetString(kv.Key, null) != s || PlayerPrefs.GetInt(kv.Key, int.MinValue) != i || !PlayerPrefs.GetFloat(kv.Key, float.NaN).Equals(f));
                if (!changed) continue;
                Log("FAIL a PlayerPrefs key changed: " + kv.Key + " (restored)"); bad++;
                if (!had) PlayerPrefs.DeleteKey(kv.Key); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(kv.Key, s); else if (i != int.MinValue) PlayerPrefs.SetInt(kv.Key, i); else PlayerPrefs.SetFloat(kv.Key, f);
            }
            PlayerPrefs.Save();
            Log(bad == 0 ? "PAGES PASS" : "PAGES FAIL (" + bad + ")");
            File.WriteAllText(Path.Combine(outDir, Tag + "_report.txt"), report.ToString());
            EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
