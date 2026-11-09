using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Hud;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Editor.Options
{
    // The options agent's checks (_re/notes/client-options.md):
    //   SaveCheck  edit mode, with REFLEX_SAVE_DIR = an empty scratch folder: types round-trip, the old keys mirrored with PlayerPrefs
    //              (written through, read live, deleted, put back at start), the migration from PlayerPrefs (+ its backup), a backup
    //              and its restore at the next start (save.json, an old key, the game's save file), the .bak fallback, deletes that
    //              stay deleted. Its own test keys only (ReflexTest.*), removed at the end.
    //   Persist    OPT_PHASE=1 sets client settings and an old key and exits; OPT_PHASE=2 (a new Unity process, the same
    //              REFLEX_SAVE_DIR) checks they came back, then restores the old key.
    //   Play       play mode on $OPT_EVENT (National/NAT_Concord) with $OPT_AI: the race with the game's own HUD (ORIGINAL), HUD STYLE
    //              MODERN switched live (the MODERN HUD shows, the game's HUD layers step aside), H hides / shows, the game's own pause
    //              page (Esc to the game) with the QUICK SETTINGS panel beside it, the options pages opened from it (every tab;
    //              the world paused under them), closed, the pause resumed, back to ORIGINAL. $OPT_FALLBACK=1: the hand-built HUD and
    //              the port's own pause page (its rows unchanged + CAMERA / HUD / ALL OPTIONS). Shots to $OPT_SHOTS.
    //              $OPT_EXPECT_STYLE=MODERN: the race must start in MODERN (the Persist test's saved style).
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.Options.OptionsTests.<Name> (Play without -quit)
    public static class OptionsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Options] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static void Exit(string what)
        {
            Log(bad == 0 ? what + " PASS" : $"{what} FAIL ({bad})");
            try { File.WriteAllText(Path.Combine(Env("OPT_SHOTS", Application.temporaryCachePath), what + ".txt"), report.ToString()); } catch (Exception) { }
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ------------------------------------------------------------------------------------------------ SaveCheck
        static readonly string[] MigrateKeys = { "ReflexUI.CareerDifficulty", "ReflexAudio.music", "ReflexProfileName" };
        static readonly Dictionary<string, (bool, string, int, float)> migrateWas = new Dictionary<string, (bool, string, int, float)>();
        public static void SaveCheck()
        {
            report.Clear(); bad = 0;
            string dir = ReflexSave.DirArg;
            if (string.IsNullOrEmpty(dir)) { Check(false, "REFLEX_SAVE_DIR (an empty scratch folder) is required"); Exit("SAVECHECK"); return; }
            try
            {
                if (Directory.Exists(dir)) Directory.Delete(dir, true);
                // old keys the "earlier version" left in PlayerPrefs (this bench's own store: its product name keeps it apart from the
                // player's): the migration brings them over with their types
                foreach (var k in MigrateKeys) migrateWas[k] = (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN));
                PlayerPrefs.SetInt("ReflexUI.CareerDifficulty", 105); PlayerPrefs.SetFloat("ReflexAudio.music", 0.25f); PlayerPrefs.SetString("ReflexProfileName", "Mig Rator"); PlayerPrefs.Save();
                ReflexSave.ReloadForTest(); ReflexSave.Load();
                Check(ReflexSave.Writable && ReflexSave.Folder == Path.GetFullPath(dir), "the run's own save folder " + ReflexSave.Folder);
                Check(ReflexSave.Origin == "migrated" && ReflexSave.MigratedKeys >= 3, $"first start: migrated {ReflexSave.MigratedKeys} keys from PlayerPrefs ({ReflexSave.Origin})");
                var file = File.ReadAllText(ReflexSave.FilePath);
                Check(file.Contains("\"ReflexUI.CareerDifficulty\": 105") && file.Contains("\"ReflexProfileName\": \"Mig Rator\"") && file.Contains("\"ReflexAudio.music\": 0.25"), "save.json holds the migrated keys with their types");
                Check(ReflexSave.Backups().Any(b => b.EndsWith("before the save system.zip")), "the \"before the save system\" backup");
                using (var z = ZipFile.OpenRead(ReflexSave.Backups().First(b => b.EndsWith("before the save system.zip")))) Check(z.GetEntry("playerprefs.json") != null && z.GetEntry("save.json") != null, "it holds save.json + playerprefs.json");

                // types round-trip (the client's own keys: save.json only)
                ReflexSave.SetInt("Options.TestInt", 42); ReflexSave.SetFloat("Options.TestFloat", 1.5f); ReflexSave.SetString("Options.TestString", "héllo \"q\"\n");
                ReflexSave.Save(); ReflexSave.ReloadForTest();
                Check(ReflexSave.GetInt("Options.TestInt") == 42 && ReflexSave.GetFloat("Options.TestFloat") == 1.5f && ReflexSave.GetString("Options.TestString") == "héllo \"q\"\n", "int / float / string round-trip");
                Check(ReflexSave.Origin == "loaded", "second start: loaded");
                Check(!PlayerPrefs.HasKey("Options.TestInt"), "the client's own keys stay out of PlayerPrefs");

                // the old keys: PlayerPrefs is the live store
                ReflexSave.SetInt("ReflexTest.Mirror", 3);
                Check(PlayerPrefs.GetInt("ReflexTest.Mirror", -1) == 3, "an old key written through to PlayerPrefs");
                PlayerPrefs.SetInt("ReflexTest.Mirror", 9);   // the original front end writes it
                Check(ReflexSave.GetInt("ReflexTest.Mirror") == 9, "an old key read live from PlayerPrefs");
                ReflexSave.Save();
                Check(File.ReadAllText(ReflexSave.FilePath).Contains("\"ReflexTest.Mirror\": 9"), "the save's copy follows PlayerPrefs");
                PlayerPrefs.SetInt("ReflexTest.Mirror", 11); ReflexSave.Save();
                Check(File.ReadAllText(ReflexSave.FilePath).Contains("\"ReflexTest.Mirror\": 11"), "a write of the file takes PlayerPrefs' current value of a known old key");
                PlayerPrefs.DeleteKey("ReflexTest.Mirror");
                Check(!ReflexSave.Has("ReflexTest.Mirror") && ReflexSave.GetInt("ReflexTest.Mirror", -7) == -7, "deleted in PlayerPrefs: gone");
                ReflexSave.Save();
                Check(!File.ReadAllText(ReflexSave.FilePath).Contains("\"ReflexTest.Mirror\":"), "and gone from the save");
                // an old key PlayerPrefs lost comes back at the next start
                ReflexSave.SetInt("ReflexTest.Lost", 4); ReflexSave.Save();
                PlayerPrefs.DeleteKey("ReflexTest.Lost"); PlayerPrefs.Save();
                ReflexSave.ReloadForTest(); ReflexSave.Load();
                Check(PlayerPrefs.GetInt("ReflexTest.Lost", -1) == 4, "an old key PlayerPrefs lost is put back from the save at start");

                // deletes stay deleted
                ReflexSave.SetInt("Options.Gone", 1); ReflexSave.Save(); ReflexSave.Delete("Options.Gone"); ReflexSave.Save(); ReflexSave.ReloadForTest();
                Check(!ReflexSave.Has("Options.Gone"), "a deleted key stays deleted");

                // a backup and its restore at the next start
                string gameSave = Path.Combine(dir, "ReflexCache", "save", "remote"); Directory.CreateDirectory(gameSave);
                File.WriteAllText(Path.Combine(gameSave, "savegame.bxml"), "ONE");
                ReflexSave.SetInt("Options.Test", 1); ReflexSave.SetInt("ReflexTest.Restore", 7); ReflexSave.Save();
                string zip = ReflexSave.BackUp("manual");
                Check(File.Exists(zip) && zip.EndsWith(" manual.zip"), "BACK UP NOW: " + Path.GetFileName(zip));
                using (var z = ZipFile.OpenRead(zip)) Check(z.GetEntry("save.json") != null && z.GetEntry("ReflexCache/save/remote/savegame.bxml") != null, "the backup holds save.json and the game's own save");
                ReflexSave.SetInt("Options.Test", 2); ReflexSave.SetInt("ReflexTest.Restore", 8); ReflexSave.Save();
                File.WriteAllText(Path.Combine(gameSave, "savegame.bxml"), "TWO");
                Check(ReflexSave.Restore(zip) && ReflexSave.RestorePending, "RESTORE: pending for the next start");
                ReflexSave.SetInt("Options.Test", 3); ReflexSave.Save();   // this session's writes no longer land
                Check(ReflexSave.Backups().Any(b => b.EndsWith(" before restore.zip")), "the current save backed up first (\"before restore\")");
                ReflexSave.ReloadForTest(); ReflexSave.Load();
                Check(ReflexSave.RestoredThisStart != null && ReflexSave.GetInt("Options.Test") == 1, $"restored at the next start (Options.Test {ReflexSave.GetInt("Options.Test")})");
                Check(PlayerPrefs.GetInt("ReflexTest.Restore", -1) == 7, "the restored old key is back in PlayerPrefs (" + PlayerPrefs.GetInt("ReflexTest.Restore", -1) + ")");
                Check(File.ReadAllText(Path.Combine(gameSave, "savegame.bxml")) == "ONE", "the game's own save restored");

                // an unreadable save.json loads the .bak
                ReflexSave.SetInt("Options.Bak", 5); ReflexSave.Save(); ReflexSave.SetInt("Options.Bak", 6); ReflexSave.Save();
                File.WriteAllText(ReflexSave.FilePath, "{ broken");
                ReflexSave.ReloadForTest();
                Check(ReflexSave.GetInt("Options.Bak", -1) == 5 && ReflexSave.Origin == "loaded", "a broken save.json: save.json.bak (" + ReflexSave.GetInt("Options.Bak", -1) + ")");

                // the typed API
                ReflexSettings.HudStyle = HudStyle.Modern; ReflexSettings.ChaseFov = 500; ReflexSettings.RumbleStrength = 0.5f;
                Check(ReflexSettings.HudStyle == HudStyle.Modern && ReflexSettings.ChaseFov == ReflexSettings.ChaseFovMax && ReflexSettings.RumbleStrength == 0.5f, "ReflexSettings: HUD STYLE, CHASE FOV (clamped), RUMBLE STRENGTH");
                Check(Mathf.Abs(ReflexClientLive.ChaseFov(1f) - 2f * Mathf.Atan(Mathf.Tan(0.5f) * 1.5f)) < 1e-5f && ReflexClientLive.ChaseFov(0.8f) != 0.8f, "CHASE FOV scales the game's FOV in tangent space");
                ReflexSettings.ChaseFov = 100; Check(ReflexClientLive.ChaseFov(0.8f) == 0.8f, "CHASE FOV 100 %: the game's own");
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            finally
            {
                foreach (var k in new[] { "ReflexTest.Mirror", "ReflexTest.Lost", "ReflexTest.Restore" }) PlayerPrefs.DeleteKey(k);
                foreach (var kv in migrateWas)
                {
                    var (had, s, i, f) = kv.Value;
                    if (!had) PlayerPrefs.DeleteKey(kv.Key); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(kv.Key, s); else if (i != int.MinValue) PlayerPrefs.SetInt(kv.Key, i); else PlayerPrefs.SetFloat(kv.Key, f);
                }
                PlayerPrefs.Save();
            }
            Exit("SAVECHECK");
        }

        // ------------------------------------------------------------------------------------------------ Persist
        public static void Persist()
        {
            report.Clear(); bad = 0;
            string phase = Env("OPT_PHASE", "1");
            Check(!string.IsNullOrEmpty(ReflexSave.DirArg) && ReflexSave.Writable, "a run's own save folder (REFLEX_SAVE_DIR): " + ReflexSave.Folder);
            if (phase == "1")
            {
                File.WriteAllText(Path.Combine(ReflexSave.Folder, "persist-mouse.txt"), PlayerPrefs.HasKey("ReflexUI.MouseSensitivity") ? PlayerPrefs.GetInt("ReflexUI.MouseSensitivity").ToString() : "none");
                ReflexSettings.HudStyle = HudStyle.Modern; ReflexSettings.ChaseFov = 120; ReflexSettings.RumbleStrength = 0.7f; ReflexSettings.FpsCap = 90; ReflexSettings.MouseSensitivity = 70;
                ReflexSettings.Save();
                Log("phase 1: HUD STYLE MODERN, CHASE FOV 120, RUMBLE 70 %, FPS CAP 90, MOUSE SENSITIVITY 70 saved to " + ReflexSave.FilePath);
            }
            else
            {
                Check(ReflexSave.Origin == "loaded", "the save loaded at this start");
                Check(ReflexSettings.HudStyle == HudStyle.Modern, "HUD STYLE came back: " + ReflexSettings.HudStyle);
                Check(ReflexSettings.ChaseFov == 120 && Mathf.Approximately(ReflexSettings.RumbleStrength, 0.7f) && ReflexSettings.FpsCap == 90, $"CHASE FOV {ReflexSettings.ChaseFov}, RUMBLE {ReflexSettings.RumbleStrength}, FPS CAP {ReflexSettings.FpsCap}");
                Check(ReflexSettings.MouseSensitivity == 70 && PlayerPrefs.GetInt("ReflexUI.MouseSensitivity", -1) == 70, "the old key MOUSE SENSITIVITY: 70 (PlayerPrefs too)");
                string f = Path.Combine(ReflexSave.Folder, "persist-mouse.txt");
                string was = File.Exists(f) ? File.ReadAllText(f) : "none";
                if (was == "none") ReflexSave.Delete("ReflexUI.MouseSensitivity"); else ReflexSave.SetInt("ReflexUI.MouseSensitivity", int.Parse(was));
                ReflexSettings.Save();
                Log("the old key restored to " + was);
            }
            Exit("PERSIST" + phase);
        }

        // ------------------------------------------------------------------------------------------------ Play
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode", "ReflexOriginalHud", "ReflexUI.DrivingCamera" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static string outDir, tag; static bool fallback; static string expectStyle;
        static double started, raceAt, stepAt; static int step;
        static OriginalWorldRuntime runtime; static OriginalLiveUi live; static ReflexModernHud modern; static ReflexPauseExtras extras;
        static readonly List<(double t, int[] keys)> keyQueue = new List<(double, int[])>();
        static HudStyle styleBefore;

        public static void Play()
        {
            report.Clear(); bad = 0; step = 0; raceAt = -1; runtime = null; live = null; modern = null; extras = null; keyQueue.Clear(); pendingShot = null;
            string ev = Env("OPT_EVENT", "National/NAT_Concord");
            fallback = Env("OPT_FALLBACK", "0") == "1";
            expectStyle = Env("OPT_EXPECT_STYLE", "");
            tag = Env("OPT_TAG", (fallback ? "fallback_" : "live_") + ev.Replace('/', '_'));
            outDir = Env("OPT_SHOTS", Path.Combine(Application.temporaryCachePath, "options-shots")); Directory.CreateDirectory(outDir);
            saved = UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
            styleBefore = ReflexSettings.HudStyle;
            OriginalHud.LiveRaces = !fallback;
            ReflexEventSelect.Pending = ev; ReflexEventSelect.PendingAi = int.Parse(Env("OPT_AI", "2"));
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += Tick;
            started = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }

        static void Keys(double at, params int[] k) => keyQueue.Add((at, k));
        static void Next(double now) { step++; stepAt = now; }
        static double Age(double now) => now - stepAt;

        static void Tick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - started > double.Parse(Env("OPT_TIMEOUT", "900"), CultureInfo.InvariantCulture)) { Check(false, "timeout at step " + step); Finish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (TickShot()) return;
            if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime || !runtime.Running) return;
            if (!live) live = runtime.GetComponent<OriginalLiveUi>();
            if (!modern) modern = runtime.GetComponent<ReflexModernHud>();
            if (!extras) extras = runtime.GetComponent<ReflexPauseExtras>();
            var race = runtime.Race;
            bool riding = race.valid != 0 && race.state == 2;
            var held = new List<int>();
            foreach (var (t, k) in keyQueue) if (now >= t && now < t + 0.15) held.AddRange(k);
            if (riding && !runtime.paused && step < 9) held.AddRange(Steer());
            runtime.testHeldKeys = held.ToArray();
            switch (step)
            {
                case 0:
                    Check(modern && extras && runtime.GetComponent<ReflexRumbleScale>() && runtime.optionsMenu, "the client's parts are on the race (MODERN HUD, pause extras, rumble scale, options handle)");
                    if (expectStyle != "") Check(ReflexSettings.HudStyleNames[(int)ReflexSettings.HudStyle] == expectStyle, "the saved HUD STYLE at the race's start: " + ReflexSettings.HudStyle);
                    ReflexSettings.HudStyle = HudStyle.Original;
                    Next(now); break;
                case 1:   // racing
                    if (!riding) { if (Age(now) > 240) { Check(false, "the race did not start"); Finish(); } return; }
                    if (raceAt < 0) raceAt = now;
                    if (now - raceAt < 4) return;
                    Check(!modern.Shown, "ORIGINAL: the MODERN HUD hidden");
                    if (!fallback) Check(modern.LiveHudPage && !modern.LiveHudHidden, $"ORIGINAL: the game's own HUD page shows (live; hidden {modern.LiveHudHidden})");
                    else Check(runtime.originalHud && runtime.originalHud.Canvas && runtime.originalHud.Canvas.enabled, "ORIGINAL: the hand-built HUD shows");
                    Shot("01_original", () => ReflexSettings.HudStyle = HudStyle.Modern);   // as the options / pause rows switch it
                    Next(now); break;
                case 2:
                    if (Age(now) < 1.5) return;
                    Check(modern.Shown, "MODERN: the MODERN HUD shows");
                    if (!fallback) Check(modern.LiveHudHidden && LiveHudAlpha() == 0f, $"MODERN: the game's HUD layers step aside (alpha {LiveHudAlpha()})");
                    else Check(!runtime.originalHud.Canvas.enabled, "MODERN: the hand-built HUD steps aside");
                    var m = modern.Model;
                    Check(m.inRace && m.lap >= 1 && m.laps == race.num_laps && m.place == Mathf.Max(1, race.place), $"the model: lap {m.lap}/{m.laps}, place {m.place}/{m.riders}, speed {m.speedMph:F1} mph, gear {m.gear + 1}, {m.standings.Count} riders on the board");
                    Shot("02_modern", () => ReflexSettings.HudHidden = true);   // H
                    Next(now); break;
                case 3:
                    if (Age(now) < 0.5) return;
                    Check(!modern.Shown && (fallback || LiveHudAlpha() == 0f), "H: the HUD hidden");
                    Shot("03_hidden", () => ReflexSettings.HudHidden = false);
                    Next(now); break;
                case 4:   // the pause
                    if (Age(now) < 1.0) return;
                    Check(modern.Shown, "H again: the HUD back");
                    if (!fallback) { Check(OriginalPauseMenu.GameOwnsPause(runtime), "the game owns the pause (live UI)"); Keys(now + 0.1, 0x01); }
                    else { Check(!OriginalPauseMenu.GameOwnsPause(runtime), "the port's own pause page"); runtime.pauseMenu.Open(); }
                    Next(now); break;
                case 5:
                    if (!fallback)
                    {
                        if (!extras.Shown) { if (Age(now) > 6) { Check(false, "the game's pause page / the QUICK SETTINGS panel did not show (pause shows: " + extras.GamePauseShows + ")"); Next(now); } return; }
                        if (Age(now) < 1.2) return;
                        Check(extras.Shown && !modern.Shown, "the game's own pause page with the QUICK SETTINGS panel; the MODERN HUD hidden under it");
                        Shot("04_pause_live", extras.OpenOptions);
                    }
                    else
                    {
                        if (Age(now) < 1.0) return;
                        var pm = runtime.pauseMenu;
                        Check(pm.IsOpen && pm.RowKeys.Count >= 4 && !pm.RowKeys.Any(k => k.StartsWith("CAMERA")), "the port's pause rows unchanged: " + string.Join(",", pm.RowKeys));
                        Check(pm.ExtraRowTexts.Count == 3 && pm.ExtraRowTexts[1] == "HUD: MODERN" && pm.ExtraRowTexts[2] == "ALL OPTIONS", "the client's rows: " + string.Join(" | ", pm.ExtraRowTexts));
                        typeof(OriginalPauseMenu).GetMethod("Focus", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(pm, new object[] { pm.ExtraStart + 2 });
                        Shot("04_pause_port", () => ReflexOptions.Open(ReflexOptions.Controls));
                    }
                    Next(now); break;
                case 6:   // the options pages, every tab
                {
                    if (Age(now) < 1.0) return;
                    if (!openChecked) { openChecked = true; Check(ReflexOptions.IsOpen && runtime.paused, "OPTIONS open from the pause, the world paused under them"); Check(runtime.optionsMenu.IsOpen, "OriginalOptionsMenu.IsOpen follows the pages (the keys stay away from the game)"); }
                    int tab = shotTabs.Count;
                    if (tab < ReflexOptions.Tabs.Length)
                    {
                        if (ReflexOptions.Tab != tab) { ReflexOptions.ShowTab(tab); stepAt = now; return; }
                        shotTabs.Add(tab); CheckTab(tab);
                        Shot("05_options_" + tab + "_" + ReflexOptions.Tabs[tab].Replace(' ', '_').Replace("&", "and"));
                        return;
                    }
                    ReflexOptions.Close();
                    Next(now); break;
                }
                case 7:
                    if (Age(now) < 0.8) return;
                    Check(!ReflexOptions.IsOpen && runtime.paused == (fallback ? true : false) , $"OPTIONS closed: the world as before (paused {runtime.paused})");
                    if (!fallback) Check(extras.Shown, "back on the game's pause page (QUICK SETTINGS again)");
                    else Check(runtime.pauseMenu.IsOpen, "back on the port's pause page");
                    // HUD STYLE from the pause: back to ORIGINAL
                    if (fallback) { var pm = runtime.pauseMenu; typeof(OriginalPauseMenu).GetMethod("Focus", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(pm, new object[] { pm.ExtraStart + 1 }); typeof(OriginalPauseMenu).GetMethod("ToggleHudStyle", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, null); }
                    else ReflexSettings.HudStyle = HudStyle.Original;
                    Shot("06_back_to_pause", () => { if (!fallback) Keys(EditorApplication.timeSinceStartup + 0.3, 0x01); else runtime.pauseMenu.Close(); });
                    Next(now); break;
                case 8:
                    if (Age(now) < 2.0) return;
                    Check(!extras.Shown && !(runtime.pauseMenu && runtime.pauseMenu.IsOpen), "the pause closed (the race on)");
                    Check(ReflexSettings.HudStyle == HudStyle.Original && !modern.Shown, "ORIGINAL again: the MODERN HUD hidden");
                    if (!fallback) Check(!modern.LiveHudHidden && LiveHudAlpha() == 1f, "ORIGINAL again: the game's HUD layers back");
                    else Check(runtime.originalHud.Canvas.enabled, "ORIGINAL again: the hand-built HUD back");
                    Shot("07_original_again");
                    Next(now); break;
                default:
                    if (Age(now) < 0.5) return;
                    Finish(); break;
            }
        }
        static readonly HashSet<int> shotTabs = new HashSet<int>(); static bool openChecked;

        static void CheckTab(int tab)
        {
            var root = ReflexOptions.Root;
            int rows = root?.Query<VisualElement>(className: "mxa-item").ToList().Count ?? 0;
            string[] expect = { "rumble-strength", "effects-volume", "frame-rate-cap", "native-game", "hud-style", "save-backup" };
            Check(root != null && root.Q(expect[tab]) != null && rows > 3, $"OPTIONS {ReflexOptions.Tabs[tab]}: {rows} rows ({expect[tab]} there)");
        }

        static float LiveHudAlpha()
        {
            if (!live) return -1f;
            foreach (var c in live.GetComponentsInChildren<Canvas>(true))
                if (c.name.StartsWith("Original UI (live)") && c.transform.childCount > 0)
                {
                    var r = c.transform.GetChild(0); float min = 1f; bool any = false;
                    var f = live.Last;
                    for (int i = 0; i < r.childCount; i++)
                    {
                        var t = r.GetChild(i);
                        bool hud = f != null && f.layers.Any(l => l.name == t.name && (l.state & 0x10) != 0 && l.page != null && l.page.EndsWith("Single Player Page") && l.name != "PostEvent_ActivityDashboard");
                        if (!hud || !t.gameObject.activeInHierarchy) continue;
                        any = true; var g = t.GetComponent<CanvasGroup>(); min = Mathf.Min(min, g ? g.alpha : 1f);
                    }
                    return any ? min : -1f;
                }
            return -1f;
        }

        static void Finish()
        {
            EditorApplication.update -= Tick;
            OriginalHud.LiveRaces = true;
            ReflexSettings.HudHidden = false; ReflexSettings.HudStyle = styleBefore; ReflexSettings.Save();
            if (ReflexOptions.IsOpen) ReflexOptions.Close();
            if (saved != null)
                foreach (var kv in saved)
                {
                    var (had, s, i, f) = kv.Value; bool now = PlayerPrefs.HasKey(kv.Key);
                    bool changed = now != had || had && (PlayerPrefs.GetString(kv.Key, null) != s || PlayerPrefs.GetInt(kv.Key, int.MinValue) != i || !PlayerPrefs.GetFloat(kv.Key, float.NaN).Equals(f));
                    if (!changed) continue;
                    Log("a PlayerPrefs key changed in the run and was restored: " + kv.Key);
                    if (!had) PlayerPrefs.DeleteKey(kv.Key); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(kv.Key, s); else if (i != int.MinValue) PlayerPrefs.SetInt(kv.Key, i); else PlayerPrefs.SetFloat(kv.Key, f);
                }
            PlayerPrefs.Save();
            Log(bad == 0 ? "PLAY PASS" : $"PLAY FAIL ({bad})");
            File.WriteAllText(Path.Combine(outDir, tag + ".txt"), report.ToString());
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // throttle; steer toward the race line ~45 units ahead (as HudTests)
        static int[] Steer()
        {
            var st = runtime.Bike;
            var onboard = runtime.onboard;
            float[] line = null; int n = 0;
            if (onboard != null) { line = (float[])onboard.GetType().GetField("raceLine", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard); n = (int)(onboard.GetType().GetField("raceLinePoints", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard) ?? 0); }
            if (st.position == null || line == null || n < 2) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            Vector3 target = new Vector3(line[3 * (n - 1)], 0, line[3 * (n - 1) + 2]);
            for (int i = 0; i < n; i++) { var p = new Vector3(line[3 * i], 0, line[3 * i + 2]); if ((p - pos).magnitude > 45) { target = p; break; } }
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x, ang = Vector3.Angle(vel, d);
            int key = ang < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        // ------------------------------------------------------------------------------------------------ shots
        // the camera with the uGUI overlays (as HudTests), then the UI Toolkit panel (the options pages, the pause extras) drawn into a
        // texture of its own for two frames and laid over it
        static string pendingShot; static int shotFrames; static RenderTexture uiTarget; static Action afterShot;
        static void Shot(string name, Action then = null)
        {
            afterShot = then;
            const int W = 1280, H = 720;
            pendingShot = tag + "_" + name; shotFrames = 0;
            uiTarget = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32);
            var ps = OptUi.Panel; ps.targetTexture = uiTarget; ps.clearColor = true; ps.colorClearValue = new Color(0, 0, 0, 0);
        }

        static bool TickShot()
        {
            if (pendingShot == null) return false;
            if (++shotFrames < 4) return true;
            const int W = 1280, H = 720;
            var c = runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            var ps = OptUi.Panel;
            Texture2D ui = null;
            if (uiTarget) { var prevA = RenderTexture.active; RenderTexture.active = uiTarget; ui = new Texture2D(W, H, TextureFormat.RGBA32, false); ui.ReadPixels(new Rect(0, 0, W, H), 0, 0); ui.Apply(); RenderTexture.active = prevA; }
            ps.targetTexture = null; ps.clearColor = false;
            if (c)
            {
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
                if (ui != null)
                {
                    var sp = tex.GetPixels32(); var up = ui.GetPixels32(); int covered = 0;
                    for (int i = 0; i < sp.Length; i++)
                    {
                        var u = up[i]; if (u.a == 0) continue; covered++;
                        float a = u.a / 255f;   // UI Toolkit writes premultiplied colour into a cleared target
                        sp[i] = new Color32((byte)Mathf.Min(255, u.r + sp[i].r * (1 - a)), (byte)Mathf.Min(255, u.g + sp[i].g * (1 - a)), (byte)Mathf.Min(255, u.b + sp[i].b * (1 - a)), 255);
                    }
                    tex.SetPixels32(sp); tex.Apply();
                    Log($"shot {pendingShot} (UI Toolkit covers {covered * 100 / sp.Length}%)");
                }
                else Log("shot " + pendingShot);
                File.WriteAllBytes(Path.Combine(outDir, pendingShot + ".png"), tex.EncodeToPNG());
                UnityEngine.Object.DestroyImmediate(tex); target.Release(); UnityEngine.Object.DestroyImmediate(target);
            }
            if (ui) UnityEngine.Object.DestroyImmediate(ui);
            if (uiTarget) { uiTarget.Release(); UnityEngine.Object.DestroyImmediate(uiTarget); uiTarget = null; }
            pendingShot = null;
            var then = afterShot; afterShot = null; then?.Invoke(); stepAt = EditorApplication.timeSinceStartup;   // the next step waits from here (a first shot compiles shaders)
            return false;
        }
    }
}
