using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Newtonsoft.Json.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using System.Reflection;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Networking;

namespace Reflex.Port.EditorTests
{
    // Verification of the run-time FMOD readers against the Python reference (_re/formats/out/audio). Batch entry points:
    //   Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.EditorTests.AudioContentTests.<Method> -logFile ...
    // with REFLEX_GAME_DIR = the install and REFLEX_REPO = the repository root (for the Python outputs).
    //   RaceProjects          (edit mode) writes $AUDIO_RACES (one line per race: level activity json-name projects...) for the
    //                          Python reference (unity_audio_library.py <level> <out> <projects...>) and checks the race specs.
    //   CompareRaceLibraries  (edit mode) the libraries of those project sets against the Python outputs in $AUDIO_REF.
    //   Race                  (play mode) the race scene on $AUDIO_EVENT ("Supercross/SX_Kingston"), $AUDIO_AI (0 none, 2 Amateur),
    //                          $AUDIO_ATV=1 on an ATV: drives 25 s with a pause in the middle and reports every event the game
    //                          played (resolved / silent / unknown), the projects and engines, voices, ducking.
    public static class AudioContentTests
    {
        static string AudioOut => Path.Combine(Environment.GetEnvironmentVariable("REFLEX_REPO") ?? "", "_re/formats/out/audio");

        // ---------------------------------------------------------------- library equivalence (edit mode, synchronous)
        public static void CompareLibraries()
        {
            int bad = 0;
            try
            {
                foreach (var level in new[] { "NAT_Concord", "FR_Alpine" })
                {
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    var set = ReflexAudioSet.Build(level);
                    double t = sw.Elapsed.TotalSeconds;
                    string ours = JsonUtility.ToJson(set.library);
                    var refJson = JObject.Parse(File.ReadAllText(Path.Combine(AudioOut, "unity", level + ".json")));
                    var diffs = new List<string>();
                    Compare(refJson, JObject.Parse(ours), "", diffs);
                    Debug.Log($"AUDIO-TEST {level}: built in {t:F2}s events={set.library.events.Length} (python {((JArray)refJson["events"]).Count}) files={set.library.files.Length} (python {((JArray)refJson["files"]).Count}) waves={set.waves.Count} unresolved={set.unresolved.Distinct().Count()} diffs={diffs.Count} leavesCompared={leaves}");
                    foreach (var d in diffs.Take(25)) Debug.Log("AUDIO-TEST   diff " + d);
                    foreach (var u in set.unresolved.Distinct().Take(10)) Debug.Log("AUDIO-TEST   unresolved " + u);
                    bad += diffs.Count;
                    var codecs = set.waves.Values.GroupBy(w => w.codec).Select(g => g.Key + "=" + g.Count());
                    Debug.Log("AUDIO-TEST   codecs " + string.Join(" ", codecs));
                }
            }
            catch (Exception e) { Debug.LogError("AUDIO-TEST exception " + e); bad++; }
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------- the race specs (edit mode)
        static readonly (string level, string activity, string lang)[] Races =
        {
            ("NAT_Concord", "National", "ENG"), ("SX_Kingston", "Supercross", "ENG"), ("TRX_Verde", "Terracross", "ENG"), ("FR_Alpine", "WaypointRace", "ENG"),
            ("FR_Alpine", "FreeRide", "ENG"), ("NAT_Renne", "National", "ENG"), ("SX_Kingston", "Supercross", "FRE"),
        };
        public static void RaceProjects()
        {
            int bad = 0;
            try
            {
                var lines = new List<string>();
                foreach (var (level, act, lang) in Races)
                {
                    var r = new ReflexAudioSet.Race { level = level, activity = act, language = lang };
                    var pr = r.Projects();
                    Debug.Log($"AUDIO-TEST race {level} {act} {lang}: {string.Join(" ", pr)}");
                    lines.Add($"{level} {act} {level}_{act}_{lang}.json {lang} {string.Join(" ", pr)}");
                }
                Debug.Log("AUDIO-TEST engine sizes (AudioPackagesList order): " + string.Join(", ", ReflexAudioSet.EngineSizes().Select(e => e.size + "=" + e.project)));
                var fielded = ReflexAudioSet.EngineProjects(ReflexAudioSet.FieldedVehicleTypes);
                Debug.Log("AUDIO-TEST fielded engines: " + string.Join(" ", fielded));
                bool engOk = fielded.SequenceEqual(new[] { "MX_125R_01", "MX_250R_TEST_05", "MX_250F_01", "MX_450F_01" });
                if (!engOk) bad++;
                Debug.Log("AUDIO-TEST " + (engOk ? "ok" : "FAIL") + " the MX Lite / MX / ATV engine projects in AudioPackagesList order");
                foreach (int m in new[] { 11, 6, 1, 20, 4, 5, 2, 3 }) Debug.Log($"AUDIO-TEST vehicle model {m}: {ReflexAudioSet.ModelEngine(m)} -> {ReflexAudioSet.EngineProjectOfModel(m)}");
                var fre = new ReflexAudioSet.Race { level = "SX_Kingston", activity = "Supercross", language = "FRE" }.Projects();
                var ger = new ReflexAudioSet.Race { level = "NAT_Concord", activity = "National", language = "GER" }.Projects();
                bool langOk = fre.Contains("SUPERCROSS_FRE") && ger.Contains("NATIONAL_ENG");
                if (!langOk) bad++;
                Debug.Log("AUDIO-TEST " + (langOk ? "ok" : "FAIL") + " voice language: FRE -> SUPERCROSS_FRE, GER -> NATIONAL_ENG");
                var vc = ReflexVolumeChannels.Load();
                foreach (ReflexChannel c in Enum.GetValues(typeof(ReflexChannel)))
                    Debug.Log($"AUDIO-TEST channel {c}: master {vc[c].master} pausedWithGame {vc[c].pausedWithGame} duck {vc[c].duckWhenPaused} over {vc[c].duckTime}s setting {ReflexVolumeChannels.SettingOf(c)}");
                string outp = Environment.GetEnvironmentVariable("AUDIO_RACES");
                if (!string.IsNullOrEmpty(outp)) File.WriteAllLines(outp, lines);
            }
            catch (Exception e) { Debug.LogError("AUDIO-TEST exception " + e); bad++; }
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------- race libraries vs the Python reference
        // C#: Build(level, projects) (the python-identical path: GLOBAL_COMMON + GLOBAL_BY_KIND + the race's projects);
        // Python: unity_audio_library.py <level> <out> <the same projects>. Then the race library itself (Build(Race)) must hold
        // exactly the race's projects + the level database's.
        public static void CompareRaceLibraries()
        {
            int bad = 0;
            string refDir = Environment.GetEnvironmentVariable("AUDIO_REF");
            try
            {
                foreach (var line in File.ReadAllLines(Path.Combine(refDir, "races.txt")))
                {
                    var f = line.Split(' '); string level = f[0], act = f[1], json = f[2], lang = f[3]; var projects = f.Skip(4).ToArray();
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    var set = ReflexAudioSet.Build(level, projects);
                    double t = sw.Elapsed.TotalSeconds;
                    var refJson = JObject.Parse(File.ReadAllText(Path.Combine(refDir, json)));
                    var diffs = new List<string>(); leaves = 0;
                    Compare(refJson, JObject.Parse(JsonUtility.ToJson(set.library)), "", diffs);
                    Debug.Log($"AUDIO-TEST {level} {act} {lang}: built in {t:F2}s events={set.library.events.Length} (python {((JArray)refJson["events"]).Count}) files={set.library.files.Length} (python {((JArray)refJson["files"]).Count}) waves={set.waves.Count} unresolved={set.unresolved.Distinct().Count()} diffs={diffs.Count} leavesCompared={leaves} missingProjects={string.Join(",", set.missing)}");
                    foreach (var d in diffs.Take(15)) Debug.Log("AUDIO-TEST   diff " + d);
                    bad += diffs.Count + set.missing.Count;
                    var race = ReflexAudioSet.Build(new ReflexAudioSet.Race { level = level, activity = act, language = lang });
                    var want = new HashSet<string>(projects, StringComparer.OrdinalIgnoreCase);
                    var extra = race.projects.Where(p => !want.Contains(p) && !set.projects.Contains(p)).ToList();
                    var lacking = projects.Where(p => !race.projects.Contains(p, StringComparer.OrdinalIgnoreCase)).ToList();
                    var chans = race.library.events.GroupBy(e => (ReflexChannel)e.channel).Select(g => g.Key + "=" + g.Count());
                    var pools = race.library.events.Where(e => e.maxPlaybacks > 0).GroupBy(e => e.maxPlaybacks + "/" + e.maxPlaybacksBehavior).OrderBy(g => g.Key).Select(g => g.Key + ":" + g.Count());
                    Debug.Log($"AUDIO-TEST   race library: projects {race.projects.Count} [{string.Join(" ", race.projects)}] events {race.library.events.Length} waves {race.waves.Count}; lacking [{string.Join(",", lacking)}] unexpected [{string.Join(",", extra)}]");
                    Debug.Log($"AUDIO-TEST   channels {string.Join(" ", chans)}; max playbacks/behaviour {string.Join(" ", pools)}");
                    if (lacking.Count > 0 || extra.Count > 0) bad++;
                }
            }
            catch (Exception e) { Debug.LogError("AUDIO-TEST exception " + e); bad++; }
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------- a race in the event scene (play mode)
        static int raceStage; static double raceStageAt, raceRunningAt, raceDropAt; static float duckSeen = 1f; static int voVoicesInPause, pausedVoices, peakVirtual;
        static string raceKey, voPath; static int raceBad;
        public static void Race()
        {
            raceKey = Environment.GetEnvironmentVariable("AUDIO_EVENT") ?? "National/NAT_Concord";
            int ai = int.Parse(Environment.GetEnvironmentVariable("AUDIO_AI") ?? "0");
            bool atv = Environment.GetEnvironmentVariable("AUDIO_ATV") == "1";
            var ev = ReflexEvents.Find(raceKey);
            if (ev == null) { Debug.LogError("AUDIO-TEST no event " + raceKey); EditorApplication.Exit(1); return; }
            ReflexEventSelect.Pending = raceKey; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = atv ? ev.AtvGarage() : null;
            Debug.Log($"AUDIO-TEST race {raceKey} AI {ai} {(atv ? "ATV " + ReflexEventSelect.PendingGarage : "MX Lite")}");
            EditorSettings.enterPlayModeOptionsEnabled = true; EditorSettings.enterPlayModeOptions = EnterPlayModeOptions.DisableDomainReload;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            raceStage = 0; raceStageAt = EditorApplication.timeSinceStartup; raceRunningAt = raceDropAt = -1; raceBad = 0;
            EditorApplication.update += RaceStep;
            EditorApplication.EnterPlaymode();
        }

        static int[] Steer(OriginalWorldRuntime rt)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            var onboard = rt.onboard;
            if (onboard == null || st.position == null) return new[] { 0x11 };
            var line = (float[])onboard.GetType().GetField("raceLine", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard);
            int n = (int)(onboard.GetType().GetField("raceLinePoints", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard) ?? 0);
            if (line == null || n < 2) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            Vector3 target = new Vector3(line[3 * (n - 1)], 0, line[3 * (n - 1) + 2]);
            for (int i = 0; i < n; i++) { var p = new Vector3(line[3 * i], 0, line[3 * i + 2]); if ((p - pos).magnitude > 45f) { target = p; break; } }
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x;
            int key = Vector3.Angle(vel, d) < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return key == 0 ? new[] { 0x11 } : new[] { 0x11, key };
        }

        static void RaceStep()
        {
            try
            {
                double now = EditorApplication.timeSinceStartup, age = now - raceStageAt;
                if (!EditorApplication.isPlaying) { if (raceStage >= 100) { EditorApplication.update -= RaceStep; Debug.Log("AUDIO-TEST done code=" + (raceBad == 0 ? 0 : 1)); EditorApplication.Exit(raceBad == 0 ? 0 : 1); } else if (age > 180) { Debug.LogError("AUDIO-TEST play mode never started"); EditorApplication.Exit(1); } return; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                var audio = UnityEngine.Object.FindAnyObjectByType<OriginalAudio>();
                var music = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>();
                if (rt && rt.Running && raceRunningAt < 0) raceRunningAt = now;
                var race = rt && rt.Running ? (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt) : default;
                if (rt && raceDropAt < 0 && race.valid != 0 && race.state >= 2) raceDropAt = now;
                if (rt) rt.testHeldKeys = raceDropAt >= 0 && raceStage < 3 ? Steer(rt) : Array.Empty<int>();
                if (audio) peakVirtual = Math.Max(peakVirtual, audio.Virtualized);
                switch (raceStage)
                {
                    case 0:
                        if (rt && rt.Running && audio && audio.ClipsReady && now - raceRunningAt > 3)
                        {
                            var spec = audio.Spec;
                            Debug.Log($"AUDIO-TEST world running; library {audio.LibrarySeconds:F2}s, clips {audio.ClipsLoaded}/{audio.ClipsTotal}; spec level {spec?.level} activity {spec?.Activity} voice {spec?.Voice}");
                            Debug.Log("AUDIO-TEST projects: " + string.Join(" ", audio.Projects.OrderBy(x => x)));
                            Debug.Log("AUDIO-TEST roster engines: " + string.Join(" ", audio.RaceEngines) + " | roster: " + string.Join("; ", rt.Roster?.entries.Select(e => e.name + " model " + e.vehicleModelId) ?? new string[0]));
                            var cfg = AudioSettings.GetConfiguration();
                            Debug.Log($"AUDIO-TEST voices: real {cfg.numRealVoices} virtual {cfg.numVirtualVoices}; effects {audio.masterVolume} engine {audio.engineVolume}");
                            raceStage = 1; raceStageAt = now;
                        }
                        else if (age > 600) { Debug.LogError("AUDIO-TEST world not running after 600 s"); raceBad++; raceStage = 100; EditorApplication.ExitPlaymode(); }
                        break;
                    case 1:   // race 12 s, then pause 3 s
                        if (raceDropAt >= 0 && now - raceDropAt > 12)
                        {
                            Debug.Log($"AUDIO-TEST before pause: events {audio.ActiveEvents} voices {audio.ActiveVoices} (peak {audio.PeakVoices}) virtualized {audio.Virtualized} music '{music?.NowPlaying}' playing {music?.CurrentClip != null}");
                            if (rt.pauseMenu) rt.pauseMenu.Open(); else { rt.paused = true; AudioListener.pause = true; }
                            raceStage = 2; raceStageAt = now;
                        }
                        else if (age > 300) { Debug.LogError("AUDIO-TEST the gate did not drop"); raceBad++; raceStage = 100; EditorApplication.ExitPlaymode(); }
                        break;
                    case 2:
                        if (age > 3)
                        {
                            duckSeen = music ? music.Duck : -1f;
                            var srcs = audio.GetComponentsInChildren<AudioSource>();
                            voVoicesInPause = srcs.Count(x => x.isPlaying && x.ignoreListenerPause);
                            pausedVoices = srcs.Count(x => x.clip && !x.ignoreListenerPause);
                            var ms = music ? music.GetComponent<AudioSource>() : null;
                            Debug.Log($"AUDIO-TEST in pause (AudioListener.pause {AudioListener.pause}): music duck {duckSeen:F2} music source playing {ms?.isPlaying} volume {ms?.volume:F3}; voices playing through the pause {voVoicesInPause}, held {pausedVoices}");
                            if (music && Mathf.Abs(duckSeen - 0.5f) > 0.01f) { raceBad++; Debug.Log("AUDIO-TEST FAIL music not ducked to 50 %"); }
                            if (rt.pauseMenu && rt.pauseMenu.IsOpen) rt.pauseMenu.Close(); else { rt.paused = false; AudioListener.pause = false; }
                            raceStage = 3; raceStageAt = now;
                        }
                        break;
                    case 3: if (age > 1.0) { Debug.Log($"AUDIO-TEST after pause: music duck {(music ? music.Duck : -1f):F2}"); raceStage = 4; raceStageAt = now; } break;
                    case 4:   // race on
                        if (rt) rt.testHeldKeys = Steer(rt);
                        if (age > 12)
                        {
                            Report(audio, rt);
                            // a VoiceOver line and an Effects event started, then the pause menu: the VO plays on, the effect is held
                            var vo = audio.CurrentLibrary.events.FirstOrDefault(e => e.channel == (int)ReflexChannel.VoiceOver && e.defs.Any(d => d.files.Any(f => f.Length > 0)));
                            if (vo != null)
                            {
                                audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 0xfeed0, serial = 1, path = vo.path });
                                audio.Tick(0.016f, rt.chaseCamera ? rt.chaseCamera.transform.position : Vector3.zero);
                                if (rt.pauseMenu) rt.pauseMenu.Open(); else { rt.paused = true; AudioListener.pause = true; }
                                raceStage = 5; raceStageAt = now; voPath = vo.path;
                            }
                            else { raceStage = 100; EditorApplication.ExitPlaymode(); }
                        }
                        break;
                    case 5:
                        if (age > 0.5)
                        {
                            var node = audio.transform.Cast<Transform>().FirstOrDefault(t => t.name == voPath);
                            var src = node ? node.GetComponentsInChildren<AudioSource>().FirstOrDefault(x => x.clip) : null;
                            bool ok = src && src.isPlaying && src.ignoreListenerPause && AudioListener.pause;
                            Debug.Log($"AUDIO-TEST {(ok ? "ok" : "FAIL")} VoiceOver {voPath} plays through the pause menu: playing {src?.isPlaying} ignoreListenerPause {src?.ignoreListenerPause} time {src?.time:F2}");
                            if (!ok) raceBad++;
                            if (rt.pauseMenu && rt.pauseMenu.IsOpen) rt.pauseMenu.Close(); else { rt.paused = false; AudioListener.pause = false; }
                            raceStage = 100; EditorApplication.ExitPlaymode();
                        }
                        break;
                }
            }
            catch (Exception e) { Debug.LogError("AUDIO-TEST exception " + e); raceBad++; raceStage = 100; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); }
        }

        static void Report(OriginalAudio audio, OriginalWorldRuntime rt)
        {
            var lib = audio.CurrentLibrary;
            var byPath = lib.events.ToDictionary(e => e.path);
            int unknown = 0, silent = 0, played = 0;
            foreach (var kv in audio.Stats.OrderBy(k => k.Key))
            {
                var st = kv.Value; string tag;
                bool music = kv.Key.StartsWith("music_", StringComparison.Ordinal);
                if (st.unknown > 0 && st.starts == 0) { tag = music ? "MUSIC (OriginalMusic)" : "UNKNOWN"; if (!music) unknown++; }
                else if (st.voices > 0) { tag = "sound"; played++; }
                else
                {
                    bool anyFile = byPath.TryGetValue(kv.Key, out var d) && d.defs.Any(x => x.files.Any(f => f.Length > 0));
                    tag = !anyFile ? "null entries (silent in the data)" : st.failed + st.stoppedBeforePlay >= st.starts ? "not played (max playbacks fail / stopped by the game before a mix)" : "SILENT";
                    if (tag == "SILENT") silent++;
                }
                Debug.Log($"AUDIO-TEST event {kv.Key}: {tag} starts {st.starts} unknown {st.unknown} voices {st.voices} null {st.nullVoices} stolen {st.stolen} failed {st.failed} stoppedBeforePlay {st.stoppedBeforePlay} channel {st.channel}");
            }
            Debug.Log($"AUDIO-TEST summary {raceKey}: events played {played}, unknown {unknown}, silent {silent}, on-demand [{string.Join(",", audio.OnDemand)}], peak voices {audio.PeakVoices}, peak virtualized {peakVirtual}, duck {duckSeen:F2}, occlusion commands {audio.OcclusionCommands}");
            if (unknown > 0 || silent > 0) raceBad++;
        }

        static int leaves;
        static void Compare(JToken a, JToken b, string path, List<string> diffs)
        {
            if (diffs.Count > 200) return;
            if (a.Type == JTokenType.Object)
            {
                if (b.Type != JTokenType.Object) { diffs.Add(path + ": type " + a.Type + " vs " + b.Type); return; }
                foreach (var p in ((JObject)a).Properties())
                {
                    var o = ((JObject)b)[p.Name];
                    if (o == null) { diffs.Add(path + "/" + p.Name + ": missing in C#"); continue; }
                    Compare(p.Value, o, path + "/" + p.Name, diffs);
                }
                foreach (var p in ((JObject)b).Properties()) if (((JObject)a)[p.Name] == null) diffs.Add(path + "/" + p.Name + ": extra in C#");
            }
            else if (a.Type == JTokenType.Array)
            {
                var x = (JArray)a; var y = b as JArray;
                if (y == null) { diffs.Add(path + ": not an array"); return; }
                if (x.Count != y.Count) { diffs.Add($"{path}: length {x.Count} vs {y.Count}"); return; }
                for (int i = 0; i < x.Count; i++) Compare(x[i], y[i], path + "[" + i + "]", diffs);
            }
            else if (a.Type == JTokenType.Float || a.Type == JTokenType.Integer)
            {
                leaves++;
                float fa = (float)a, fb = (float)b;
                if (fa != fb && !(float.IsNaN(fa) && float.IsNaN(fb))) diffs.Add($"{path}: {fa:R} vs {fb:R}");
            }
            else if (a.Type == JTokenType.Boolean) { leaves++; if ((bool)a != (bool)b) diffs.Add($"{path}: {a} vs {b}"); }
            else if (++leaves > 0 && (string)a != (string)b) diffs.Add($"{path}: '{(string)a}' vs '{(string)b}'");
        }

        // ---------------------------------------------------------------- music: the clean MPEG streams byte for byte against the Python export
        public static void MusicBytes()
        {
            int bad = 0;
            try
            {
                var info = ReflexMusic.Load();
                var py = JObject.Parse(File.ReadAllText(Path.Combine(AudioOut, "events.json")))["music"]["tracks"];
                string cache = Path.Combine(Application.temporaryCachePath, "musictest"); Directory.CreateDirectory(cache);
                int same = 0, total = 0; long bytes = 0;
                foreach (var t in info.tracks)
                {
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    string path = ReflexMusic.Extract(t, cache);
                    var mine = File.ReadAllBytes(path);
                    var row = py.FirstOrDefault(x => (string)x["BankName"] == t.bank);
                    string refPath = row == null ? null : Path.Combine(AudioOut, (string)row["mp3"]);
                    bool eq = refPath != null && File.Exists(refPath) && mine.SequenceEqual(File.ReadAllBytes(refPath));
                    bool titleOk = row != null && t.title == (string)row["ArtistName"] + " - " + (string)row["SongName"];
                    total++; if (eq) same++; bytes += mine.Length; if (!eq || !titleOk) bad++;
                    Debug.Log($"AUDIO-TEST music {t.bank} '{t.title}' bytes={mine.Length} identicalToPython={eq} titleMatches={titleOk} ({sw.ElapsedMilliseconds} ms)");
                }
                Debug.Log($"AUDIO-TEST music: {same}/{total} tracks byte-identical, {bytes / 1e6:F1} MB; start delays National={info.startDelay["National"]} FreeRide={info.startDelay["FreeRide"]} Supercross={info.startDelay["Supercross"]}; transition National={info.transitionTime["National"]}; uiTier={info.uiTierTransitionTime}; jukebox={info.jukeboxEvent}");
            }
            catch (Exception e) { Debug.LogError("AUDIO-TEST exception " + e); bad++; }
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------- IMA / PCM against the Python decoder
        // The vectors are produced by the test script _re/formats (fsb_export.encode_ima_block/decode_ima) into REFLEX_IMA_VECTOR (raw bytes + expected pcm).
        public static void ImaVector()
        {
            string dir = Environment.GetEnvironmentVariable("REFLEX_IMA_VECTOR");
            int bad = 0;
            foreach (int ch in new[] { 1, 2 })
            {
                var raw = File.ReadAllBytes(Path.Combine(dir, $"ima{ch}.raw")); var exp = File.ReadAllBytes(Path.Combine(dir, $"ima{ch}.pcm"));
                int n = int.Parse(File.ReadAllText(Path.Combine(dir, $"ima{ch}.n")));
                var got = ReflexFmod.DecodeIma(raw, ch, n);
                var bytes = new byte[got.Length * 2]; Buffer.BlockCopy(got, 0, bytes, 0, bytes.Length);
                bool same = bytes.SequenceEqual(exp);
                Debug.Log($"AUDIO-TEST ima ch={ch} samples={got.Length / ch} equal={same}");
                if (!same) bad++;
            }
            var pcm16 = new byte[] { 1, 0, 2, 0, 0xff, 0xff, 0, 0x80 };
            var d16 = ReflexFmod.DecodePcm(pcm16, ReflexFmod.M_16BITS);
            var dd = ReflexFmod.DecodePcm(pcm16, ReflexFmod.M_16BITS | ReflexFmod.M_DELTA);
            var d8 = ReflexFmod.DecodePcm(new byte[] { 128, 255, 0 }, ReflexFmod.M_8BITS | ReflexFmod.M_UNSIGNED);
            Debug.Log($"AUDIO-TEST pcm16={string.Join(",", d16)} delta={string.Join(",", dd)} pcm8u={string.Join(",", d8)}");
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        // ---------------------------------------------------------------- play mode: decode every wave through Unity and compare with the WAVs
        public static void PlayMode(string mode)
        {
            var o = EditorSettings.enterPlayModeOptionsEnabled; EditorSettings.enterPlayModeOptionsEnabled = true;
            EditorSettings.enterPlayModeOptions = EnterPlayModeOptions.DisableDomainReload | EnterPlayModeOptions.DisableSceneReload;
            EditorApplication.playModeStateChanged += st =>
            {
                if (st != PlayModeStateChange.EnteredPlayMode) return;
                var go = new GameObject("audio-test"); var d = go.AddComponent<AudioTestDriver>(); d.mode = mode;
            };
            EditorApplication.EnterPlaymode();
        }
        public static void DecodeCompare() => PlayMode("decode");
        public static void RuntimeAudio() => PlayMode("audio");
        public static void RuntimeMusic() => PlayMode("music");

        public sealed class AudioTestDriver : MonoBehaviour
        {
            public string mode;
            void Start() { StartCoroutine(Run()); }

            IEnumerator Run()
            {
                int code = 0;
                if (mode == "decode") yield return Decode(c => code = c);
                else if (mode == "audio") yield return RuntimeAudioTest(c => code = c);
                else if (mode == "music") yield return RuntimeMusicTest(c => code = c);
                Debug.Log("AUDIO-TEST done code=" + code);
                EditorApplication.Exit(code);
            }

            static float[] ReadWav(string path, out int rate, out int ch)
            {
                var b = File.ReadAllBytes(path);
                int p = 12; rate = 0; ch = 0; int dataAt = -1, dataLen = 0;
                while (p + 8 <= b.Length)
                {
                    string id = Encoding.ASCII.GetString(b, p, 4); int len = BitConverter.ToInt32(b, p + 4);
                    if (id == "fmt ") { ch = BitConverter.ToInt16(b, p + 10); rate = BitConverter.ToInt32(b, p + 12); }
                    if (id == "data") { dataAt = p + 8; dataLen = len; break; }
                    p += 8 + len + (len & 1);
                }
                var f = new float[dataLen / 2];
                for (int i = 0; i < f.Length; i++) f[i] = BitConverter.ToInt16(b, dataAt + i * 2) / 32768f;
                return f;
            }

            IEnumerator Decode(Action<int> done)
            {
                int bad = 0;
                foreach (var level in new[] { "NAT_Concord", "FR_Alpine" })
                {
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    var go = new GameObject("audio-" + level); var audio = go.AddComponent<OriginalAudio>(); audio.level = level;
                    float t0 = Time.realtimeSinceStartup;
                    while (!audio.LibraryReady && Time.realtimeSinceStartup - t0 < 120) yield return null;
                    float tLib = Time.realtimeSinceStartup - t0;
                    while (!audio.ClipsReady && Time.realtimeSinceStartup - t0 < 300) yield return null;
                    float tClips = Time.realtimeSinceStartup - t0;
                    Debug.Log($"AUDIO-TEST {level}: library ready after {tLib:F2}s, clips ready after {tClips:F2}s, clips {audio.ClipsLoaded}/{audio.ClipsTotal}, events {audio.CurrentLibrary.events.Length}, error={audio.LoadError}");
                    var set = ReflexAudioSet.Build(level);   // (same data again: for the per-wave FSB facts)
                    int missing = 0, lenBad = 0, rateBad = 0, chBad = 0, checkedN = 0; double worstRms = 0, sumRms = 0; string worst = ""; int pcmExact = 0; var rows = new List<(string key, double rel, double refRms, double errRms)>();
                    foreach (var kv in set.waves)
                    {
                        if (!audio.Clips.TryGetValue(kv.Key, out var clip) || !clip) { missing++; Debug.Log("AUDIO-TEST   missing clip " + kv.Key); continue; }
                        var w = kv.Value;
                        if (clip.samples != w.lengthSamples) { lenBad++; if (lenBad <= 8) Debug.Log($"AUDIO-TEST   length {kv.Key}: unity {clip.samples} fsb {w.lengthSamples}"); }
                        if (clip.frequency != w.rate) rateBad++;
                        if (clip.channels != w.channels) chBad++;
                        string wavPath = Path.Combine(AudioOut, kv.Key);
                        if (!File.Exists(wavPath)) continue;
                        var wav = ReadWav(wavPath, out int wrate, out int wch);
                        var data = new float[clip.samples * clip.channels]; clip.GetData(data, 0);
                        int n = Math.Min(data.Length, wav.Length); double se = 0, sw2 = 0;
                        for (int i = 0; i < n; i++) { double dd = data[i] - wav[i]; se += dd * dd; sw2 += (double)wav[i] * wav[i]; }
                        double rms = Math.Sqrt(se / Math.Max(1, n)), ref2 = Math.Sqrt(sw2 / Math.Max(1, n));
                        double rel = ref2 > 1e-6 ? rms / ref2 : rms;
                        rows.Add((kv.Key, rel, ref2, rms));
                        checkedN++; sumRms += rel; if (rel > worstRms) { worstRms = rel; worst = kv.Key; }
                        if (w.codec != "mpeg" && rms == 0) pcmExact++;
                    }
                    string dump = Environment.GetEnvironmentVariable("REFLEX_DUMP");
                    if (!string.IsNullOrEmpty(dump) && level == "NAT_Concord")
                        foreach (var key in new[] { "ACTIVITY_NATIONALS/ENG_111.wav", "ACTIVITY_NATIONALS/AWARD.wav", "CROWD_OUT_LRG/Crowd_Out_Lrg_Amb_Neg_05(+6dB).wav", "engine/MX_250F_bank00/MXB_250F_FLOODING_02.wav", "SCENARIOS_GLOBAL/FW_FOUNTAIN_02_OCT.wav", "NAT_CO_Collision/ChainLinkHard(+6dB).wav" })
                            if (audio.Clips.TryGetValue(key, out var dc))
                            {
                                var dd = new float[dc.samples * dc.channels]; dc.GetData(dd, 0);
                                var bb = new byte[dd.Length * 4]; Buffer.BlockCopy(dd, 0, bb, 0, bb.Length);
                                File.WriteAllBytes(Path.Combine(dump, key.Replace('/', '_') + $".{dc.channels}ch.f32"), bb);
                            }
                    Debug.Log($"AUDIO-TEST {level}: waves={set.waves.Count} compared={checkedN} missing={missing} lengthMismatch={lenBad} rateMismatch={rateBad} channelMismatch={chBad} meanRelRms={sumRms / Math.Max(1, checkedN):E3} worstRelRms={worstRms:E3} ({worst})");
                    foreach (var r in rows.OrderByDescending(x => x.rel).Take(6)) Debug.Log($"AUDIO-TEST   worst {r.key}: relErr={r.rel:E2} refRms={r.refRms:E2} errRms={r.errRms:E2}");
                    var sortedRel = rows.Select(x => x.rel).OrderBy(x => x).ToList();
                    Debug.Log($"AUDIO-TEST   relErr percentiles: p50={sortedRel[sortedRel.Count / 2]:E2} p90={sortedRel[sortedRel.Count * 9 / 10]:E2} p99={sortedRel[sortedRel.Count * 99 / 100]:E2}; max errRms={rows.Max(x => x.errRms):E2}");
                    bad += missing + rateBad + chBad;
                    Destroy(go);
                }
                done(bad == 0 ? 0 : 1);
            }

            int audioBad;
            IEnumerator RuntimeAudioTest(Action<int> done)
            {
                var go = new GameObject("audio-run"); var audio = go.AddComponent<OriginalAudio>(); audio.level = "NAT_Concord";
                var listener = new GameObject("listener").AddComponent<AudioListener>();
                // commands arrive before the library is ready: they must be queued and replayed
                audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 1, serial = 1, path = "NAT_Concord/FAKE/none" });
                float t0 = Time.realtimeSinceStartup;
                while (!audio.ClipsReady && Time.realtimeSinceStartup - t0 < 300) yield return null;
                Debug.Log($"AUDIO-TEST runtime: library {audio.LibrarySeconds:F2}s, clips {audio.ClipsLoaded}/{audio.ClipsTotal} after {Time.realtimeSinceStartup - t0:F2}s");
                yield return null; yield return null;       // Update flushes the queued command
                audio.Tick(0.016f, Vector3.zero);
                bool finished = audio.Finished.TryDequeue(out var fin);
                Debug.Log($"AUDIO-TEST unknown event finished at once: {finished} wrapper={fin.wrapper}");
                var paths = audio.CurrentLibrary.events.Select(e => e.path).ToList();
                string eng = paths.FirstOrDefault(p => p.StartsWith("mx_250f_01"));
                Debug.Log("AUDIO-TEST engine event sample: " + eng + " (events " + paths.Count + ")");
                // play: a one-shot UI event and the engine, drive the parameters
                string one = paths.FirstOrDefault(p => p.StartsWith("activity_nationals/activity/holeshot_won"));
                if (one != null)
                {
                    audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 2, serial = 7, path = one });
                    for (int i = 0; i < 5; i++) { audio.Tick(0.016f, Vector3.zero); yield return null; }
                    Debug.Log($"AUDIO-TEST one-shot {one}: activeEvents={audio.ActiveEvents} voices={audio.ActiveVoices}");
                }
                if (eng != null)
                {
                    var e = audio.CurrentLibrary.events.First(x => x.path == eng);
                    audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 3, serial = 9, path = eng, has_position = 0 });
                    foreach (var p in e.parameters) audio.Handle(new ReflexAudioCommand { sub = 4, wrapper = 3, serial = 9, param = p.name, value = p.min + (p.max - p.min) * 0.5f });
                    for (int i = 0; i < 30; i++) { audio.Tick(0.016f, Vector3.zero); yield return null; }
                    Debug.Log($"AUDIO-TEST engine {eng}: params={string.Join(",", e.parameters.Select(q => q.name))} voices={audio.ActiveVoices}");
                    var srcs = go.GetComponentsInChildren<AudioSource>().Where(s => s.clip != null).Select(s => $"{s.clip.name}({s.clip.length:F2}s,{s.clip.frequency}Hz,{s.clip.channels}ch,playing={s.isPlaying},vol={s.volume:F2},pitch={s.pitch:F2})");
                    Debug.Log("AUDIO-TEST   sources: " + string.Join(" | ", srcs));
                    // the Engine channel: EngineVolume, occlusion (sub-command 10), against an Effects event (EffectsVolume)
                    string idle = paths.First(p => p.EndsWith("mxb_250f_idle"));
                    audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 4, serial = 11, path = idle });
                    for (int i = 0; i < 5; i++) { audio.Tick(0.016f, Vector3.zero); yield return null; }
                    AudioSource EngineSrc() { var n = go.transform.Cast<Transform>().FirstOrDefault(t => t.name == idle); return n ? n.GetComponentsInChildren<AudioSource>().FirstOrDefault(x => x.clip) : null; }
                    audio.followSettings = false; audio.engineVolume = 1f; audio.masterVolume = 1f;
                    audio.Tick(0.016f, Vector3.zero); float v0 = EngineSrc() ? EngineSrc().volume : -1f;
                    audio.Handle(new ReflexAudioCommand { sub = 10, wrapper = 4, serial = 11, value = 0.6f });
                    audio.Tick(0.016f, Vector3.zero); var es = EngineSrc(); float v1 = es ? es.volume : -1f; var lp = es ? es.GetComponent<AudioLowPassFilter>() : null; float cut1 = lp && lp.enabled ? lp.cutoffFrequency : -1f;
                    audio.Handle(new ReflexAudioCommand { sub = 10, wrapper = 4, serial = 11, value = 0f });
                    audio.engineVolume = 0.3f; audio.masterVolume = 0f; audio.Tick(0.016f, Vector3.zero); float v2 = EngineSrc() ? EngineSrc().volume : -1f;
                    bool occOk = v0 > 0 && Mathf.Abs(v1 - v0 * 0.4f) < 1e-3f && Mathf.Abs(cut1 - 22000f * 0.4f) < 1f;
                    bool volOk = v0 > 0 && Mathf.Abs(v2 - Mathf.Min(1f, v0 * 0.3f)) < 1e-3f;
                    Debug.Log($"AUDIO-TEST {(occOk ? "ok" : "FAIL")} occlusion 0.6: engine volume {v0:F3} -> {v1:F3}, low-pass {cut1:F0} Hz (the event's own Lowpass Simple envelope: ~20000 Hz)");
                    Debug.Log($"AUDIO-TEST {(volOk ? "ok" : "FAIL")} EngineVolume 0.3 with EffectsVolume 0: engine volume {v0:F3} -> {v2:F3} (the Engine channel follows EngineVolume only, as 0x5b9e00)");
                    if (!occOk || !volOk) audioBad++;
                    audio.engineVolume = 1f; audio.masterVolume = 1f;
                }
                // every event of the library: start it and tick a while, count voices that really start
                int started = 0, silent = 0, id = 100;
                foreach (var ev in audio.CurrentLibrary.events.Take(40))
                {
                    audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = (uint)++id, serial = (uint)id, path = ev.path });
                    audio.Tick(0.016f, Vector3.zero);
                    if (audio.ActiveVoices > 0) started++; else silent++;
                    audio.StopAll();
                }
                Debug.Log($"AUDIO-TEST first 40 events: started with a voice={started} without={silent}");
                // the roster hook's path (OriginalAudio.UseRoster -> AddProjects): a car engine merged into the running library
                int before = audio.CurrentLibrary.events.Length, clipsBefore = audio.ClipsTotal;
                audio.AddProjects(new[] { ReflexAudioSet.EngineProjectOfModel(4) });          // VehicleModel 4: a UTV -> RHINO_ENGINE_02
                float tx = Time.realtimeSinceStartup;
                while (!audio.CurrentLibrary.events.Any(e => e.path.StartsWith("rhino_engine_02/")) && Time.realtimeSinceStartup - tx < 60) yield return null;
                while (audio.ClipsLoaded < audio.ClipsTotal && Time.realtimeSinceStartup - tx < 60) yield return null;
                var rhino = audio.CurrentLibrary.events.FirstOrDefault(e => e.path.StartsWith("rhino_engine_02/") && !e.path.EndsWith("_scn"));
                bool merged = rhino != null && audio.Projects.Contains("RHINO_ENGINE_02");
                int rv = 0;
                if (rhino != null)
                {
                    audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = 500, serial = 1, path = rhino.path });
                    foreach (var p in rhino.parameters) audio.Handle(new ReflexAudioCommand { sub = 4, wrapper = 500, serial = 1, param = p.name, value = p.min + (p.max - p.min) * 0.5f });
                    for (int i = 0; i < 5; i++) { audio.Tick(0.016f, Vector3.zero); yield return null; }
                    rv = audio.Stats.TryGetValue(rhino.path, out var rs) ? rs.voices : 0;
                }
                Debug.Log($"AUDIO-TEST {(merged && rv > 0 ? "ok" : "FAIL")} AddProjects(RHINO_ENGINE_02): events {before} -> {audio.CurrentLibrary.events.Length}, clips {clipsBefore} -> {audio.ClipsTotal} in {Time.realtimeSinceStartup - tx:F2}s; {rhino?.path} voices {rv} channel {(ReflexChannel)(rhino?.channel ?? 0)}");
                if (!merged || rv == 0) audioBad++;
                done(audio.ClipsLoaded > 0 && audio.ClipsLoaded == audio.ClipsTotal && audioBad == 0 ? 0 : 1);
            }

            IEnumerator RuntimeMusicTest(Action<int> done)
            {
                var go = new GameObject("music-run"); var m = go.AddComponent<OriginalMusic>(); m.level = "NAT_Concord"; m.useActivityStartDelay = false;
                new GameObject("listener").AddComponent<AudioListener>();
                float t0 = Time.realtimeSinceStartup;
                while (m.CurrentClip == null && string.IsNullOrEmpty(m.LoadError) && Time.realtimeSinceStartup - t0 < 120) yield return null;
                Debug.Log($"AUDIO-TEST music: first track after {Time.realtimeSinceStartup - t0:F2}s tracks={m.TrackCount} now='{m.NowPlaying}' error={m.LoadError}");
                var clip = m.CurrentClip;
                if (clip != null) Debug.Log($"AUDIO-TEST music clip {clip.name}: {clip.length:F1}s {clip.frequency}Hz {clip.channels}ch samples={clip.samples}");
                for (int i = 0; i < 60; i++) yield return null;
                var src = go.GetComponent<AudioSource>();
                Debug.Log($"AUDIO-TEST music playing={src.isPlaying} time={src.time:F2}");
                done(clip != null ? 0 : 1);
            }
        }
    }
}
