using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // WP-7 persistence end to end in play mode (ReflexPersistence; native persist.h), PNGs and a report to $PERSIST_SHOTS:
    //   Race    (a set-up motocard: CareerTests.Create first) title -> Load Game -> main menu -> an arcade event (PERSIST_EVENT, default
    //           National/NAT_Concord, PERSIST_LAPS laps, default 1, no AI) -> the race driven to its end (CareerTests' race-line
    //           autopilot) -> the post-race results (the game's own "Activity Results") -> CONTINUE -> the front end: the race's world
    //           left (the game's saves: RequestSaveGame, the records commit, the next save) -> the profile read from the new save has the
    //           race in PlayerRecords (the event's EventTypeLocationId and vehicle type) and its highlights / achievements.
    //   Verify  (a new editor process) the profile session: PlayerRecords, PlayerHighlight and the Steam achievements read back
    //           (PERSIST_EXPECT_EVENTS: at least that many TotalEvents for the event), then title -> Load Game -> main menu -> MOTOCARD:
    //           the Motocard Page's recent highlights (PlayerHighlight by UnlockedOrder) show what the race unlocked.
    // The user's PlayerPrefs keys are restored at the end; the bench's own productName keeps its save apart.
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.PersistenceTests.Race (or .Verify)
    public static class PersistenceTests
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_read__fn(IntPtr world, uint address, byte[] output, uint size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn> reflex_world_read__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_read__fn>("reflex_world_read");
        static int reflex_world_read(IntPtr world, uint address, byte[] output, uint size) => reflex_world_read__p.Get()(world, address, output, size);
        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage; static double stageAt, started, lastLog; static string shots, mode, eventKey;
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, string> prefs;
        static ProfileData before;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Persist] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }

        public static void Race() => Begin("race");
        public static void Verify() => Begin("verify");

        static void Begin(string m)
        {
            mode = m; report.Clear(); bad = 0; stage = 0; ReflexProfile.Disabled = false; eventKey = Env("PERSIST_EVENT", "National/NAT_Concord");
            prefs = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            shots = Env("PERSIST_SHOTS", Path.Combine(Application.temporaryCachePath, "persist-shots"));
            Directory.CreateDirectory(shots);
            Log("save " + ReflexProfile.SaveFile + (ReflexProfile.HasSave ? " (" + new FileInfo(ReflexProfile.SaveFile).Length + " bytes, " + File.GetLastWriteTime(ReflexProfile.SaveFile).ToString("HH:mm:ss") + ")" : " (none)"));
            Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (fm) fm.rememberChoice = false;
            stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Shot(string name)
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
            File.WriteAllBytes(Path.Combine(shots, mode + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + mode + "_" + name);
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static bool Idle(FrontEndManager fm) => fm && fm.assets != null && !fm.Busy;
        static bool At(FrontEndManager fm, string page, double after = 1.0, double timeout = 60)
        {
            if (Idle(fm) && fm.CurrentPage == page && Age > after) return true;
            if (Age > timeout) Fail("page " + page + " not reached (at " + fm?.CurrentPage + " / " + fm?.TopPopup + ")");
            return false;
        }

        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > 1500) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 100) End(); else if (Age > 120) { Check(false, "play mode never started"); End(); } return; }
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
                if (mode == "race") StepRace(fm); else StepVerify(fm);
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        // The event's PlayerRecords row (EventTypeLocationId of the event's EventTypeLocations row, its VehicleTypeId) in a profile.
        static int RecordRow(ProfileData p, out string text)
        {
            text = "none"; if (p == null) return -1;
            var t = p.T("PlayerRecords");
            var ev = ReflexEvents.Find(eventKey);
            int etl = ev != null ? ev.eventTypeLocationId : -1;
            for (int i = 0; i < t.rows; i++)
                if (t.Int("EventTypeLocationId", i) == etl || etl < 0 && t.Int("EventTypeLocationId", i) >= 0)
                {
                    text = $"EventTypeLocationId {t.Int("EventTypeLocationId", i)} VehicleTypeId {t.Int("VehicleTypeId", i)}: TotalEvents {t.Int("TotalEvents", i)}, TotalLaps {t.Int("TotalLaps", i)}, " +
                           $"TotalTime {t.Float("TotalTime", i):F2}, BestLapTime {t.Float("BestLapTime", i):F2}, BestPlace {t.Int("BestPlace", i)}, TotalPlacing {t.Int("TotalPlacing", i)}, " +
                           $"TotalAttemptedJumps {t.Int("TotalAttemptedJumps", i)}, TotalHoleshotsWon {t.Int("TotalHoleshotsWon", i)}, BestScore {t.Int("BestScore", i)}";
                    return i;
                }
            return -1;
        }
        static string Highlights(ProfileData p)
        {
            var hl = p?.T("PlayerHighlight"); if (hl == null) return "none";
            return string.Join(", ", Enumerable.Range(0, hl.rows).Select(i => $"#{hl.Int("HighlightId", i)} {hl.Int("GoalProgress", i)} (order {hl.Int("UnlockedOrder", i)})"));
        }

        static void StepRace(FrontEndManager fm)
        {
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage != "Title Page" && fm.CurrentPage != "Load Game Page" && Age > 1 && !ReflexProfile.Busy)
                    {
                        before = ReflexProfile.Current;
                        Check(fm.CurrentPage == "Main Menu Page", "a set-up motocard goes to the main menu: " + fm.CurrentPage);
                        RecordRow(before, out var rec); Log("before the race: records " + rec + "; highlights " + Highlights(before));
                        // ARCADE -> event / location / vehicle select end in this launch (FrontEndManager.LaunchEvent)
                        ReflexEventSelect.Launch(eventKey, 0, false, "Reflex", int.Parse(Env("PERSIST_LAPS", "1")));
                        Next();
                    }
                    else if (Age > 90) Fail("the profile did not load: " + ReflexProfile.Error);
                    break;
                case 2:
                    if (rt && rt.Running && Age > 3)
                    {
                        Log("arcade world running: " + rt.statusLabel?.text?.Split('\n')[0]);
                        Check(ReflexPersistence.LastAttach != null && ReflexPersistence.LastAttach.Contains("\"after\""), "the profile was loaded into the race: " + ReflexPersistence.LastAttach);
                        Next();
                    }
                    else if (Age > 400) Fail("arcade world not running: " + rt?.statusLabel?.text);
                    break;
                case 3:
                {
                    var race = RaceState(rt);
                    rt.testHeldKeys = race.state == 2 ? Steer(rt) : Array.Empty<int>();
                    if (EditorApplication.timeSinceStartup - lastLog > 10) { lastLog = EditorApplication.timeSinceStartup; Log($"race {race.state} place {race.place}/{race.riders} lap {race.laps_completed}/{race.num_laps} time {race.race_time:F1}"); }
                    if (race.state >= 4) { rt.testHeldKeys = Array.Empty<int>(); Log("race over: place " + race.place + " of " + race.riders + ", time " + race.race_time.ToString("F2") + ", best lap " + race.best_lap.ToString("F2")); Next(); }
                    else if (Age > 1200) Fail("the race did not end");
                    break;
                }
                case 4:   // the post-race results: the game's own table
                {
                    var hud = UnityEngine.Object.FindAnyObjectByType<OriginalHud>();
                    var post = hud ? typeof(OriginalHud).GetField("post", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(hud) as OriginalPostRace : null;
                    if (post == null || !post.IsOpen) { if (Age > 30) Fail("post-race pages not open"); break; }
                    if (Age < 3) break;
                    Shot("01_post_race");
                    Check(post.HasGameTable, "the results page shows the game's own Activity Results (PopulateResultsTable)");
                    var rows = typeof(OriginalPostRace).GetField("rows", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(post) as List<OriginalPostRace.Row>;
                    var heads = typeof(OriginalPostRace).GetField("headers", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(post) as string[];
                    Log("results: " + (heads != null ? string.Join(" | ", heads) : "(port columns)") + " :: " + string.Join("; ", (rows ?? new List<OriginalPostRace.Row>()).Select(r => $"{r.index} {r.name} {r.c0} {r.c1} {r.c2}")));
                    post.onContinue?.Invoke();   // PostEventMenu CONTINUE: back to the front end
                    Next();
                    break;
                }
                case 5:
                    if (ReflexPersistence.LastLeave != null && Idle(fm) && fm.CurrentPage != null && Age > 2)
                    {
                        string l = ReflexPersistence.LastLeave;
                        Check(l.Contains("\"saved\":[0,0]"), "the game saved twice (OnPostStateComplete, the front end's after the records commit): " + l.Substring(0, Math.Min(200, l.Length)));
                        Check(l.Contains("\"committed\":1"), "the race's PlayerRecords working row was committed (PlayerLeft)");
                        Log("front end: " + fm.CurrentPage);
                        ReflexProfile.Open(); Next();
                    }
                    else if (Age > 90) Fail("the race was not left / no front end: " + fm?.CurrentPage + ", leave " + ReflexPersistence.LastLeave);
                    break;
                case 6:
                    if (ReflexProfile.Current != null && ReflexProfile.Current != before && !ReflexProfile.Busy && Age > 1)
                    {
                        var p = ReflexProfile.Current;
                        int row = RecordRow(p, out var rec); RecordRow(before, out var recBefore);
                        Check(row >= 0, "the profile read from the new save has the race in PlayerRecords: " + rec);
                        int was = 0; if (before != null) { int b = RecordRow(before, out _); if (b >= 0) was = before.T("PlayerRecords").Int("TotalEvents", b); }
                        if (row >= 0) Check(p.T("PlayerRecords").Int("TotalEvents", row) == was + 1, $"TotalEvents {was} -> {p.T("PlayerRecords").Int("TotalEvents", row)}");
                        Log("highlights after the race: " + Highlights(p));
                        var a = ReflexPersistence.ReadAchievements();
                        Check(a != null, "the profile verb achievements answers: " + (a == null ? ReflexProfile.Error : a.unlocked + " highlights unlocked, " + a.achieved + " Steam achievements, file " + a.file));
                        if (a != null) foreach (var x in a.items.Where(x => x.unlocked || x.progress > 0 || x.achieved)) Log($"  highlight {x.id} {x.enumName} {x.progress}/{x.goal} order {x.unlockedOrder} -> {x.steamName} achieved {x.achieved} at {x.unlockTime}");
                        stage = 100; EditorApplication.ExitPlaymode();
                    }
                    else if (Age > 60) Fail("no profile after the race: " + ReflexProfile.Error);
                    break;
            }
        }

        static void StepVerify(FrontEndManager fm)
        {
            switch (stage)
            {
                case 0: if (At(fm, "Title Page", 2)) { fm.DispatchKey("Accept"); Next(); } break;
                case 1:
                    if (Idle(fm) && fm.CurrentPage != "Title Page" && fm.CurrentPage != "Load Game Page" && Age > 1 && !ReflexProfile.Busy)
                    {
                        var p = ReflexProfile.Current;
                        int row = RecordRow(p, out var rec);
                        int expect = int.Parse(Env("PERSIST_EXPECT_EVENTS", "1"));
                        Check(row >= 0 && p.T("PlayerRecords").Int("TotalEvents", row) >= expect, "PlayerRecords read back after the editor restart: " + rec);
                        Log("highlights: " + Highlights(p));
                        var a = ReflexPersistence.ReadAchievements();
                        Check(a != null && a.achieved > 0 && a.items.Where(x => x.achieved).All(x => x.unlocked), "Steam achievements read back: " + (a == null ? "none" : string.Join(", ", a.items.Where(x => x.achieved).Select(x => x.steamName + " (highlight " + x.id + ", " + DateTimeOffset.FromUnixTimeSeconds(x.unlockTime).ToString("u") + ")"))));
                        Next();
                    }
                    else if (Age > 90) Fail("the profile did not load");
                    break;
                case 2: if (At(fm, "Main Menu Page", 1.5)) { fm.Navigate(1, 0); fm.Navigate(1, 0); fm.DispatchKey("Accept"); Next(); } break;   // MOTOCARD
                case 3:
                    if (At(fm, "Motocard Page", 2))
                    {
                        Shot("01_motocard");
                        var hl = ReflexProfile.Current.T("PlayerHighlight");
                        int shown = Enumerable.Range(0, hl.rows).Count(i => hl.Int("UnlockedOrder", i, 0) > 0);
                        Check(shown > 0, "the motocard's recent highlights show " + shown + " unlocked highlight(s)");
                        stage = 100; EditorApplication.ExitPlaymode();
                    }
                    break;
            }
        }

        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => rt ? (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt) : default;
        static uint U32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToUInt32(b, 0); }
        static float F32(IntPtr w, uint a) { var b = new byte[4]; reflex_world_read(w, a, b, 4); return BitConverter.ToSingle(b, 0); }
        static double slowSince, reverseUntil;
        // CareerTests' autopilot (ShowcaseShots'): throttle and steer toward the race line ~45 units ahead (test only).
        static int[] Steer(OriginalWorldRuntime rt)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            var world = (IntPtr)typeof(OriginalWorldRuntime).GetField("world", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            if (st.position == null || world == IntPtr.Zero) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            uint G = U32(world, 0xa946b8), P = G + 0x7460, A = U32(world, P + 0x94), drivers = U32(world, A + 0x268), n = U32(world, A + 0x26c), D = 0;
            for (uint i = 0; i < n && i < 64; i++) { uint d = U32(world, drivers + 4 * i); if (d != 0 && U32(world, d + 0x74) == P) { D = d; break; } }
            if (D == 0) return new[] { 0x11 };
            Vector3 target = pos;
            uint node = U32(world, D + 0x1c), first = node;
            if (node != 0)
                for (int k = 0; node != 0 && k < 64; k++) { var p = new Vector3(F32(world, node), 0, F32(world, node + 8)); target = p; if ((p - pos).magnitude > 45) break; node = U32(world, node + 0x64); if (node == first) break; }
            else
            {
                uint gc = U32(world, A + 0xff074); if (gc == 0) return new[] { 0x11 };
                uint gi = (uint)(((int)U32(world, D + 0x10d4) + 1) % (int)gc), g = U32(world, U32(world, A + 0xff078) + 4 * gi);
                target = new Vector3(F32(world, g + 0x60), 0, F32(world, g + 0x68));
            }
            var dd = target - pos;
            double now = EditorApplication.timeSinceStartup;
            if (vel.magnitude >= 3f) slowSince = now;
            if (now - slowSince > 2.5) { reverseUntil = now + 1.5; slowSince = now + 1.5; }
            if (now < reverseUntil) return new[] { 0x2d, (vel.x * dd.z - vel.z * dd.x) > 0 ? 0x20 : 0x1e };
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * dd.z - vel.z * dd.x, ang = Vector3.Angle(vel, dd);
            int key = ang < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        static void Fail(string why) { Check(false, why); stage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); else End(); }

        static void End()
        {
            EditorApplication.update -= Step;
            ReflexProfile.Close();
            var after = UserKeys.ToDictionary(k => k, k => PlayerPrefs.HasKey(k) ? PlayerPrefs.GetString(k, "") + "|" + PlayerPrefs.GetInt(k, 0) : null);
            Check(after.All(kv => kv.Value == prefs[kv.Key]), "user PlayerPrefs keys untouched");
            Log("persistence " + mode + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(shots, "persist-" + mode + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
