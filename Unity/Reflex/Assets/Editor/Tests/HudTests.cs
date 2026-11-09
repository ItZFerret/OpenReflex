using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Unity.Profiling;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // WP-3: the race HUDs, the game's own live (OriginalLiveUi for the arcade races, OriginalHud.LiveRace; _re/notes/hud.md).
    //   Play: play mode on $HUD_EVENT (National/NAT_Concord) with $HUD_AI (the AI choice index, 0 off), throttle + race-line steering.
    //         Checks: the hub's route, the activity's HUD page and layer ($HUD_LAYER), the rank text against every rider's standing
    //         (reflex_world_race_drivers: the shown place is one the game had in the last 2 s, it applies a change 0.75 s late), the lap
    //         counter against laps completed, the lap timer against the race clock; that OriginalHud's hand-built HUD stays hidden. Measures
    //         ui_frame + the parse on the simulation thread and the mirror on the main thread (ms, GC bytes per frame) and the main
    //         thread's GC allocations per frame (ProfilerRecorder "GC Allocated In Frame"). Shots at $HUD_TIMES (pre: after the world
    //         runs, else after the race starts) and on every new page to $HUD_SHOTS. $HUD_SECONDS: run time after the start;
    //         $HUD_WAIT=post: on to race state Post (the port's post-race pages must show, the live canvas must hide).
    //         $HUD_OFF=1: HUD VISIBILITY off for the run (the dashboard must hide; the player's pref restored).
    //         PlayerPrefs: the user's keys are checked and restored (the bench's own product name keeps them apart anyway).
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.HudTests.Play (no -quit)
    public static class HudTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Hud] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode", "ReflexOriginalHud" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static void RestorePrefs()
        {
            if (saved == null) return;
            foreach (var kv in saved)
            {
                var (had, s, i, f) = kv.Value; bool now = PlayerPrefs.HasKey(kv.Key);
                bool changed = now != had || had && (PlayerPrefs.GetString(kv.Key, null) != s || PlayerPrefs.GetInt(kv.Key, int.MinValue) != i || !PlayerPrefs.GetFloat(kv.Key, float.NaN).Equals(f));
                if (!changed) continue;
                if (kv.Key != "ReflexOriginalHud" || Env("HUD_OFF", "0") != "1") { Log("FAIL a PlayerPrefs key changed: " + kv.Key + " (restored)"); bad++; }
                if (!had) PlayerPrefs.DeleteKey(kv.Key); else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(kv.Key, s); else if (i != int.MinValue) PlayerPrefs.SetInt(kv.Key, i); else PlayerPrefs.SetFloat(kv.Key, f);
            }
            PlayerPrefs.Save();
        }

        static string eventKey, outDir, tag, wait, expectLayer; static (bool pre, float t)[] times; static int shot;
        static double started, runStart = -1, raceAt = -1, postAt = -1, lastLog, endAt;
        static OriginalWorldRuntime runtime; static OriginalLiveUi live;
        static readonly HashSet<string> pagesSeen = new HashSet<string>();
        static string lastPages = ""; static double pageShot = -1; static int pageShots;
        static readonly List<(double t, int place)> places = new List<(double, int)>();
        static int rankChecks, rankBad, lapChecks, lapBad, timerChecks, timerBad, hudShown, dashboardChecks, dashboardBad;
        static string lastRankText, lastLapText, firstBadRank; static int rankExact; static readonly StringBuilder rankChanges = new StringBuilder();
        static ProfilerRecorder gcRec; static long gcSum; static int gcFrames; static double simSum; static int simFrames; static bool statsReset;
        static bool hudOff, rankSeen, simFail; static int lastFrame; static float gcFrom = 3, gcTo = 13;

        public static void Play()
        {
            report.Clear(); bad = 0; shot = 0; runStart = raceAt = postAt = -1; pageShots = 0; pageShot = -1; pagesSeen.Clear(); places.Clear();
            rankSeen = false; rankExact = 0; rankChanges.Clear(); rankChecks = rankBad = lapChecks = lapBad = timerChecks = timerBad = hudShown = dashboardChecks = dashboardBad = 0; lastRankText = lastLapText = firstBadRank = null;
            gcSum = 0; gcFrames = 0; simSum = 0; simFrames = 0; statsReset = false; runtime = null; live = null; lastPages = "";
            eventKey = Env("HUD_EVENT", "National/NAT_Concord"); tag = Env("HUD_TAG", eventKey.Replace('/', '_'));
            outDir = Env("HUD_SHOTS", Path.Combine(Application.temporaryCachePath, "hud-shots")); Directory.CreateDirectory(outDir);
            wait = Env("HUD_WAIT", ""); expectLayer = Env("HUD_LAYER", "");
            times = Env("HUD_TIMES", "pre:4,pre:12,3,20").Split(',').Select(s => { bool pre = s.StartsWith("pre:"); return (pre, float.Parse(pre ? s.Substring(4) : s, CultureInfo.InvariantCulture)); }).ToArray();
            saved = UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
            hudOff = Env("HUD_OFF", "0") == "1";
            { var w = Env("HUD_GCWIN", "3,13").Split(','); gcFrom = float.Parse(w[0], CultureInfo.InvariantCulture); gcTo = float.Parse(w[1], CultureInfo.InvariantCulture); }
            OriginalHud.LiveRaces = Env("HUD_FALLBACK", "0") != "1";   // A/B: the hand-built HUD
            simFail = OriginalLiveUi.SimulateUnavailable = Env("HUD_SIMFAIL", "0") == "1";   // the live UI unavailable: the fallback must show
            if (hudOff) PlayerPrefs.SetInt("ReflexOriginalHud", 0);
            lock (OriginalHud.UiSoundLog) OriginalHud.UiSoundLog.Clear();
            ReflexEventSelect.Pending = eventKey; ReflexEventSelect.PendingAi = int.Parse(Env("HUD_AI", "0"));
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            EditorApplication.update += Tick;
            started = EditorApplication.timeSinceStartup;
            EditorApplication.EnterPlaymode();
        }

        static int Ordinal(string s) { if (string.IsNullOrEmpty(s)) return -1; int n = 0, i = 0; while (i < s.Length && char.IsDigit(s[i])) n = n * 10 + (s[i++] - '0'); return i > 0 ? n : -1; }
        static float ParseTime(string s)   // "MM:SS.cc"
        {
            if (string.IsNullOrEmpty(s) || s.Length < 8 || s[2] != ':') return float.NaN;
            return float.TryParse(s.Substring(0, 2), NumberStyles.Integer, CultureInfo.InvariantCulture, out float m) && float.TryParse(s.Substring(3), NumberStyles.Float, CultureInfo.InvariantCulture, out float sec) ? m * 60 + sec : float.NaN;
        }

        static void Tick()
        {
            double now = EditorApplication.timeSinceStartup;
            if (now - started > double.Parse(Env("HUD_TIMEOUT", "900"), CultureInfo.InvariantCulture)) { Check(false, "timeout"); Finish(); return; }
            if (!EditorApplication.isPlaying) return;
            if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            if (!runtime) return;
            if (!live) live = runtime.GetComponent<OriginalLiveUi>();
            if (live == null && !OriginalHud.LiveRaces) { FallbackTick(now); return; }
            if (live == null) { if (now - started > 120) { Check(false, "no OriginalLiveUi on the race (OriginalHud.LiveRace)"); Finish(); } return; }
            if (simFail) { SimFailTick(now); return; }
            if (!runtime.Running || !live.Running) { if (live.Unavailable && now - started > 60) { Check(false, "the live UI is unavailable"); Finish(); } return; }
            if (runStart < 0)
            {
                runStart = now; Log($"world running; the hub's route: {live.Route}; race {live.race}");
                Check(!string.IsNullOrEmpty(live.Route) && live.race, "the game's UI went in-game for the race");
                gcRec = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame");
            }
            // the GC / time window (race + {gcFrom}..{gcTo} s): only the steering and the samples, the same in the fallback run (the checks allocate)
            if (raceAt > 0 && now - raceAt > gcFrom && now - raceAt < gcTo) { QuietTick(); return; }
            var f = live.Last;
            string pages = string.Join(" + ", f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").Select(l => l.page + ":" + l.name).Distinct());
            foreach (var l in f.layers) if ((l.state & 0x10) != 0) pagesSeen.Add(l.graph + "/" + l.page + "/" + l.name);
            if (pages != lastPages) { lastPages = pages; Log($"t {now - runStart:F1} pages: {pages}"); pageShot = now + 0.8; }
            if (pageShot > 0 && now >= pageShot && pageShots < 8) { pageShot = -1; var top = f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").Select(l => l.page).LastOrDefault() ?? "none"; Capture($"{tag}_page{pageShots++:00}_{top.Replace(' ', '_')}"); }

            var race = runtime.Race;
            bool riding = f.hasMode && f.mode.state == 2;
            if (riding && raceAt < 0) { raceAt = now; Log($"race in progress ({f.mode.name})"); }
            runtime.testHeldKeys = riding ? Steer(now) : Array.Empty<int>();
            if (runtime.originalHud && runtime.originalHud.Canvas && runtime.originalHud.Canvas.enabled) hudShown++;

            // the shown values against the running game's
            int local = -1; for (int i = 0; i < runtime.DriverCount; i++) if (runtime.Drivers[i].local != 0) local = i;
            int place = local >= 0 ? runtime.Drivers[local].place : race.place;
            if (race.valid != 0) places.Add((now, race.state <= 1 ? 1 : place));
            places.RemoveAll(p => now - p.t > 2.5);
            var hudLayer = f.layers.FirstOrDefault(l => (l.state & 0x10) != 0 && l.page.EndsWith("Single Player Page", StringComparison.Ordinal) && l.name != "PostEvent_ActivityDashboard");
            if (hudLayer != null && riding)
            {
                bool Vis(int i) { for (int k = i; k >= 0; k = hudLayer.nodes[k].parent) if ((hudLayer.nodes[k].state & 0x10) == 0) return false; return true; }
                var nodes = hudLayer.nodes;
                int ri = nodes.FindIndex(n => n.name == "SinglePlayerRankComponent"); if (ri >= 0) rankSeen = true;
                if (ri >= 0 && Vis(ri) && now - raceAt > 2)
                {
                    // MXUIPlayerRankComponent (005f90e0) commits a new place once it has been stable for 0.75 s (a change restarts the wait):
                    // each change of the shown rank must be a place the game had in the last 1.5 s
                    string rt = nodes[ri].text; int shown = Ordinal(rt);
                    if (rt != lastRankText)
                    {
                        rankChecks++;
                        if (lastRankText != null && places.Any(p => p.place == shown && now - p.t <= 1.5)) rankExact++;
                        // the component ranks by the activity's ranking (A+0x2d0), the driver list's place D+0x78 (the reference here) can differ by
                        // one for a moment (side by side): a change must be within one place of the game's places of the last 1.5 s
                        if (lastRankText != null && (shown < 1 || !places.Any(p => Math.Abs(p.place - shown) <= 1 && now - p.t <= 1.5))) { rankBad++; firstBadRank ??= $"t {now - raceAt:F1}: shown '{rt}', the game's places {string.Join(",", places.Where(p => now - p.t <= 1.5).Select(p => p.place).Distinct())}"; }
                        rankChanges.Append(rt).Append(' ');
                    }
                    lastRankText = rt;
                }
                int li = nodes.FindIndex(n => n.name == "SinglePlayer LapCounterComponent");
                if (li >= 0 && Vis(li) && race.num_laps > 0 && race.state == 2)
                {
                    string lt = nodes[li].text; lapChecks++; lastLapText = lt;
                    string want = $"{Mathf.Clamp(race.laps_completed + 1, 1, race.num_laps)}/{race.num_laps}";
                    if (lt != want) { int prev = Mathf.Clamp(race.laps_completed, 1, race.num_laps); if (lt != $"{prev}/{race.num_laps}") lapBad++; }   // a lap just completed: one frame behind
                }
                int ti = nodes.FindIndex(n => n.name == "Timer Dashboard Component" && !float.IsNaN(ParseTime(n.text)));
                if (ti >= 0 && Vis(ti) && race.state == 2 && now - raceAt > 1)
                {
                    float shownT = ParseTime(nodes[ti].text); timerChecks++;
                    float want = race.lap_time, alt = race.last_lap;
                    if (float.IsNaN(shownT) || Mathf.Abs(shownT - want) > 0.12f && Mathf.Abs(shownT - alt) > 0.02f) timerBad++;
                }
                if (hudOff) { int di = nodes.FindIndex(n => n.vtable == 0x951f14); if (di >= 0) { dashboardChecks++; if (Vis(di)) dashboardBad++; } }
            }
            if (now - lastLog > 3.0)
            {
                lastLog = now;
                var texts = f.layers.Where(l => (l.state & 0x10) != 0 && l.graph != "OverlayPageGraph.pag").SelectMany(l => l.nodes.Where(n => n.text != null && n.text.Length > 0 && (n.state & 0x10) != 0 && !n.name.EndsWith("Shadow", StringComparison.OrdinalIgnoreCase)).Select(n => n.name + "='" + n.text.Replace("\n", " ").Substring(0, Math.Min(30, n.text.Length)) + "'")).Take(16);
                Log($"t {now - runStart:F1} state {race.state} place {place} laps {race.laps_completed}/{race.num_laps} lap {race.lap_time:F2} sim {runtime.SimMilliseconds:F2} ms | {string.Join(" ", texts)}");
            }
            bool post = race.valid != 0 && race.state == 4;
            if (post && postAt < 0) { postAt = now; Log("race state Post"); }
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
                if (now - postAt < 5) return;
                Check(OriginalPostRace.Active, "race state Post: the port's post-race pages show");
                var canvas = live.GetComponentsInChildren<Canvas>(true).FirstOrDefault(c => c.name.StartsWith("Original UI (live)"));
                Check(canvas == null || !canvas.enabled, "race state Post: the live canvas is hidden");
                Capture($"{tag}_post");
                Finish(); return;
            }
            if (raceAt > 0 && now - raceAt > double.Parse(Env("HUD_SECONDS", "25"), CultureInfo.InvariantCulture)) Finish();
        }

        // HUD_SIMFAIL=1: the live UI unavailable -> OriginalHud's hand-built HUD shows, the live canvas does not
        static void SimFailTick(double now)
        {
            if (!runtime.Running) return;
            if (runStart < 0) { runStart = now; Log("world running, the live UI made unavailable"); }
            var race = runtime.Race; bool riding = race.valid != 0 && race.state == 2;
            if (riding && raceAt < 0) raceAt = now;
            runtime.testHeldKeys = riding ? Steer(now) : Array.Empty<int>();
            if (runtime.originalHud && runtime.originalHud.Canvas && runtime.originalHud.Canvas.enabled) hudShown++;
            if (shot < times.Length) { var s = times[shot]; double since = s.pre ? now - runStart : raceAt < 0 ? -1 : now - raceAt; if (since >= s.t) { Capture($"{tag}_{shot}_{(s.pre ? "pre" : "run")}{s.t:0.#}s"); shot++; } return; }
            if (raceAt > 0 && now - raceAt > 6)
            {
                var canvas = live.GetComponentsInChildren<Canvas>(true).FirstOrDefault(c => c.name.StartsWith("Original UI (live)"));
                Check(live.Unavailable, "the live UI reports Unavailable");
                Check(hudShown > 0, $"the hand-built HUD shows ({hudShown} frames)");
                Check(canvas == null || !canvas.enabled, "the live canvas stays hidden");
                runStart = -1; Finish();
            }
        }

        static void QuietTick()
        {
            if (Time.frameCount == lastFrame) return;
            lastFrame = Time.frameCount;
            if (!statsReset) { statsReset = true; OriginalLiveUi.Stats.Reset(); gcSum = 0; gcFrames = 0; return; }
            gcSum += gcRec.Valid ? gcRec.LastValue : 0; gcFrames++; simSum += runtime.SimMilliseconds; simFrames++;
            runtime.testHeldKeys = Steer(0);
        }

        // HUD_FALLBACK=1: the hand-built HUD (OriginalHud) on the same race, for the A/B of the main thread's GC and the shots
        static void FallbackTick(double now)
        {
            if (!runtime.Running) return;
            if (runStart < 0) { runStart = now; Log("world running with the hand-built HUD (fallback)"); gcRec = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame"); }
            var race = runtime.Race; bool riding = race.valid != 0 && race.state == 2;
            if (riding && raceAt < 0) raceAt = now;
            runtime.testHeldKeys = riding ? Steer(now) : Array.Empty<int>();
            if (raceAt > 0 && now - raceAt > gcFrom && now - raceAt < gcTo) { QuietTick(); return; }
            if (shot < times.Length) { var s = times[shot]; double since = s.pre ? now - runStart : raceAt < 0 ? -1 : now - raceAt; if (since >= s.t) { Capture($"{tag}_{shot}_{(s.pre ? "pre" : "run")}{s.t:0.#}s"); shot++; } return; }
            if (raceAt > 0 && now - raceAt > double.Parse(Env("HUD_SECONDS", "25"), CultureInfo.InvariantCulture))
            {
                Log($"fallback: main thread GC {(gcFrames > 0 ? gcSum / gcFrames : 0)} bytes/frame over {gcFrames} frames (race + {gcFrom}..{gcTo} s); sim {(simFrames > 0 ? simSum / simFrames : 0):F2} ms");
                runStart = -1; Finish();
            }
        }

        static void Finish()
        {
            OriginalHud.LiveRaces = true; OriginalLiveUi.SimulateUnavailable = false;
            EditorApplication.update -= Tick;
            if (runStart > 0)
            {
                double c = OriginalLiveUi.Stats.collects > 0 ? OriginalLiveUi.Stats.collectMs / OriginalLiveUi.Stats.collects : 0, a = OriginalLiveUi.Stats.applies > 0 ? OriginalLiveUi.Stats.applyMs / OriginalLiveUi.Stats.applies : 0;
                Log($"perf: ui_frame + parse {c:F3} ms/frame (max {OriginalLiveUi.Stats.collectMax:F3}, {OriginalLiveUi.Stats.collects} frames); " +
                    $"mirror (Apply + tree Tick + sounds) {a:F3} ms/frame (max {OriginalLiveUi.Stats.applyMax:F3}); " +
                    $"main thread GC {(gcFrames > 0 ? gcSum / gcFrames : 0)} bytes/frame over {gcFrames} frames (race + {gcFrom}..{gcTo} s); sim {(simFrames > 0 ? simSum / simFrames : 0):F2} ms");
                Check(hudOff || !rankSeen || rankChecks > 0 && rankBad == 0, $"rank: {rankChecks} changes of the shown rank ({rankExact} after the first equal to a place of the driver list in the last 1.5 s), {rankBad} more than one place off{(firstBadRank != null ? " (first: " + firstBadRank + ")" : "")}; shown: {rankChanges}");
                Check(lapChecks == 0 || lapBad == 0, $"lap counter: {lapChecks} frames, {lapBad} off; last '{lastLapText}'");
                Check(timerChecks == 0 || timerBad <= 2, $"lap timer: {timerChecks} frames, {timerBad} off by more than 0.12 s");
                Check(hudShown == 0, $"the hand-built HUD stayed hidden ({hudShown} frames shown)");
                if (!string.IsNullOrEmpty(expectLayer)) Check(pagesSeen.Any(p => p.EndsWith("/" + expectLayer)), "the HUD layer " + expectLayer + " showed");
                if (hudOff) Check(dashboardChecks > 0 && dashboardBad == 0, $"HUD VISIBILITY off: the dashboard hidden in {dashboardChecks - dashboardBad}/{dashboardChecks} frames");
                lock (OriginalHud.UiSoundLog) Log("UI sounds: " + (OriginalHud.UiSoundLog.Count == 0 ? "(none)" : string.Join("; ", OriginalHud.UiSoundLog)));
            }
            if (live) foreach (var (layer, nodes, unmapped, failed) in live.LayerStats) { Check(!failed && unmapped == 0, $"layer {layer}: {nodes} live nodes, {unmapped} without a node in the install's layer{(failed ? ", NOT BUILT" : "")}"); }
            if (gcRec.Valid) gcRec.Dispose();
            Log("pages seen: " + string.Join(", ", pagesSeen.OrderBy(x => x)));
            RestorePrefs();
            Log(bad == 0 ? "PLAY PASS" : $"PLAY FAIL ({bad})");
            File.WriteAllText(Path.Combine(outDir, tag + ".txt"), report.ToString());
            if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
            EditorApplication.delayCall += () => EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------------------------------------- Bench
        // Edit mode, no Unity frame loop: the world of $HUD_EVENT ($HUD_OPPONENTS AI, built by an earlier Play) run directly, twice from its
        // image: 1500 frames without the game's UI, then 1500 with it (ui_activate at frame 1, OriginalLiveUi.Collect after every frame).
        // Reports the world frame's cost without / with the in-game UI (frames 900..1500: the race), Collect's cost and its GC
        // allocations (GC.GetTotalMemory deltas around Collect alone: one thread), and the bike state of both runs (equal: the UI does not
        // change the simulation).
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_activate__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string route, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_activate__fn> reflex_world_ui_activate__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_activate__fn>("reflex_world_ui_activate");
        static int reflex_world_ui_activate(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string route, byte[] output, int size) => reflex_world_ui_activate__p.Get()(world, route, output, size);
        public static void Bench()
        {
            report.Clear(); bad = 0;
            string key = Env("HUD_EVENT", "National/NAT_Concord"); int opp = int.Parse(Env("HUD_OPPONENTS", "0"));
            outDir = Env("HUD_SHOTS", Path.Combine(Application.temporaryCachePath, "hud-shots")); Directory.CreateDirectory(outDir);
            try
            {
                var files = ReflexWorldFiles.For(key, opp, ReflexWorldFiles.AiSkillNormal);
                Check(files.Ready, $"{key} world {files.World} ready");
                if (!files.Ready) throw new InvalidOperationException("run Play on it first");
                string exe = ReflexInstall.Exe, crt = Path.Combine(ReflexCrt.Dir, "msvcr90.dll");
                double[] frameMs = new double[2]; string[] hash = new string[2]; double collectMs = 0, collectMax = 0; long gcBytes = 0, gcBytesMono = 0; int collects = 0, gcs = 0;
                for (int run = 0; run < 2; run++)
                {
                    var w = ReflexWorldNative.reflex_world_create(exe, crt, files.World, files.Deform);
                    if (w == IntPtr.Zero) throw new InvalidOperationException("reflex_world_create: " + ReflexWorldNative.Error(IntPtr.Zero, "?"));
                    ReflexWorldNative.reflex_world_set_ambient_animation(w, 0);
                    var keys = new byte[256]; var sw = new System.Diagnostics.Stopwatch(); string route = null;
                    for (int fr = 0; fr < 1500; fr++)
                    {
                        if (run == 1 && fr == 1) { var b = new byte[64]; int n = reflex_world_ui_activate(w, null, b, 64); route = n > 0 ? Encoding.ASCII.GetString(b, 0, n) : null; }
                        keys[0x11] = (byte)(fr >= 1100 ? 0x80 : 0);
                        if (fr >= 900) sw.Start();
                        int code = ReflexWorldNative.reflex_world_frame(w, 1f / 60f, keys, null, 0);
                        if (fr >= 900) sw.Stop();
                        if (code != 0) throw new InvalidOperationException("frame " + fr + ": " + ReflexWorldNative.Error(w, "?"));
                        if (run == 1 && fr >= 2)
                        {
                            long m0 = GC.GetTotalMemory(false), p0 = UnityEngine.Profiling.Profiler.GetMonoUsedSizeLong(); int g0 = GC.CollectionCount(0);
                            long t0 = System.Diagnostics.Stopwatch.GetTimestamp();
                            var f = OriginalLiveUi.Collect(w, false, true);
                            double ms = (System.Diagnostics.Stopwatch.GetTimestamp() - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
                            long m1 = GC.GetTotalMemory(false), p1 = UnityEngine.Profiling.Profiler.GetMonoUsedSizeLong();
                            OriginalLiveUi.Recycle(f);
                            if (fr >= 900) { collectMs += ms; collectMax = Math.Max(collectMax, ms); collects++; if (GC.CollectionCount(0) == g0) { gcBytes += Math.Max(0, m1 - m0); gcBytesMono += Math.Max(0, p1 - p0); gcs++; } }
                            if (fr == 1499) Log($"last frame's pages: {string.Join(", ", f.layers.Where(l => (l.state & 0x10) != 0).Select(l => l.page + ":" + l.name).Distinct())}");
                        }
                    }
                    frameMs[run] = sw.Elapsed.TotalMilliseconds / 600;
                    var st = new ReflexBikeState(); ReflexWorldNative.reflex_world_bike(w, ref st);
                    hash[run] = $"{st.position[0]:R},{st.position[1]:R},{st.position[2]:R} v {st.velocity[0]:R},{st.velocity[1]:R},{st.velocity[2]:R}";
                    ReflexWorldNative.reflex_world_destroy(w);
                    Log(run == 0 ? $"without the UI: {frameMs[0]:F3} ms/frame (frames 900..1500)" : $"with the UI ({route}): {frameMs[1]:F3} ms/frame; Collect {collectMs / Math.Max(1, collects):F3} ms (max {collectMax:F3}); GC {gcBytes / Math.Max(1, gcs)} B/frame (GetTotalMemory), {gcBytesMono / Math.Max(1, gcs)} B/frame (MonoUsedSize) over {gcs} frames");
                }
                Check(hash[0] == hash[1], $"the bike state is the same with and without the game's UI ({hash[0]} / {hash[1]})");
            }
            catch (Exception e) { Check(false, "bench: " + e); }
            Log(bad == 0 ? "BENCH PASS" : $"BENCH FAIL ({bad})");
            File.WriteAllText(Path.Combine(outDir, "bench_" + key.Replace('/', '_') + "_" + opp + ".txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // throttle; steer (A 0x1e / D 0x20) toward the race line ~45 units ahead (as ModesTests)
        static int[] Steer(double now)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(runtime);
            var onboard = typeof(OriginalWorldRuntime).GetField("onboard", BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Public)?.GetValue(runtime);
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

        static void Capture(string name)
        {
            const int W = 1280, H = 720;
            var c = runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            if (!c) return;
            var target = new RenderTexture(W, H, 24); target.antiAliasing = 4;
            var prevTarget = c.targetTexture; c.targetTexture = target;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>().Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy && x.name.IndexOf("Event list", StringComparison.OrdinalIgnoreCase) < 0).OrderBy(x => x.sortingOrder).ToList();
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
