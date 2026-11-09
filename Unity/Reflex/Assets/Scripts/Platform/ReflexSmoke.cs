#if UNITY_EDITOR
// The player smoke test (REFLEX/scripts/build_test.sh; notes: _re/notes/client-platform.md). Command line only, off otherwise:
//   -reflexSmoke [seconds]        the first scene (the front end) must run 5 s without an exception, then the race of
//                                 -reflexSmokeEvent <event key> (default National/NAT_Concord) with -reflexSmokeAi <0..4> (default 0)
//                                 starts through ReflexEventSelect.Launch (remember: false, PlayerPrefs untouched), its world must
//                                 run, W is held once the gate drops, and after <seconds> (default 30) of running world the player
//                                 logs "[Reflex] smoke OK ..." and quits 0; "[Reflex] smoke FAIL ..." and 1 on a timeout or an
//                                 exception. -reflexPerfInterval 5 makes ReflexPerfLog write more lines in a short run.
//                                 A smoke run runs in the background and in a 1280 x 720 window (whatever graphics.cfg says), as
//                                 a guest (ReflexProfile.Disabled: the player's save is never read or written; only the derived world
//                                 cache under persistentDataPath/ReflexCache is used).
using System;
using System.Collections;
using System.Globalization;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.Platform
{
    public sealed class ReflexSmoke : MonoBehaviour
    {
        float seconds = 30f;
        string eventKey = "National/NAT_Concord";
        int ai;
        int exceptions; string firstException;

        static string Arg(string name, out bool present)
        {
            var a = Environment.GetCommandLineArgs();
            int i = Array.IndexOf(a, name);
            present = i >= 0;
            return present && i + 1 < a.Length && !a[i + 1].StartsWith("-") ? a[i + 1] : null;
        }

        // before the front end's Awake opens the profile session: the guest path, the race never writes the player's save
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { Arg("-reflexSmoke", out bool on); if (on) Reflex.Port.Content.ReflexProfile.Disabled = true; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            string s = Arg("-reflexSmoke", out bool on);
            if (!on) return;
            var go = new GameObject("Reflex smoke test") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            Application.runInBackground = true;   // a smoke run is usually not the frontmost app
            var t = go.AddComponent<ReflexSmoke>();
            if (float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out float v) && v > 0) t.seconds = v;
            t.eventKey = Arg("-reflexSmokeEvent", out _) ?? t.eventKey;
            if (int.TryParse(Arg("-reflexSmokeAi", out _), out int a)) t.ai = a;
        }

        void OnEnable() => Application.logMessageReceived += OnLog;
        void OnDisable() => Application.logMessageReceived -= OnLog;

        void OnLog(string message, string stack, LogType type)
        {
            if (type != LogType.Exception) return;
            exceptions++;
            firstException ??= message;
        }

        void Start() => StartCoroutine(Run());

        // windowed (the install's graphics.cfg may ask for fullscreen; a test run should not take the desktop over)
        void Update()
        {
            if (Screen.fullScreenMode != FullScreenMode.Windowed && Time.frameCount > 2) Screen.SetResolution(1280, 720, FullScreenMode.Windowed);
        }

        IEnumerator Run()
        {
            Debug.Log($"[Reflex] smoke: start ({seconds:0} s of {eventKey}, AI {ai}); scene {SceneManager.GetActiveScene().name}, session log {ReflexCrashLog.SessionLogPath}");
            float t0 = Time.realtimeSinceStartup;
            // the first scene (build index 0, the front end: the original FrontEndManager or another front end) runs a while: its
            // title, its video, its music
            while (Time.realtimeSinceStartup - t0 < 5f) yield return null;
            var scene = SceneManager.GetActiveScene();
            if (scene.buildIndex != 0) { Fail($"the first scene is {scene.name} (build index {scene.buildIndex}), not the front end"); yield break; }
            if (exceptions > 0) { Fail("the front end logged an exception: " + firstException); yield break; }
            bool original = FindAnyObjectByType<Reflex.Port.FrontEnd.FrontEndManager>();
            float up = Time.realtimeSinceStartup - t0;
            Debug.Log($"[Reflex] smoke: front end {scene.name} up ({(original ? "the original front end" : "no FrontEndManager: another front end")})");
            Debug.Log($"[Reflex] smoke: launching {eventKey}");
            ReflexEventSelect.Launch(eventKey, ai, remember: false);
            t0 = Time.realtimeSinceStartup;
            OriginalWorldRuntime rt = null;
            while (!(rt && rt.Running))
            {
                if (Time.realtimeSinceStartup - t0 > 300) { Fail("the race world did not run within 300 s"); yield break; }
                if (!rt) rt = FindAnyObjectByType<OriginalWorldRuntime>();
                yield return new WaitForSecondsRealtime(0.25f);
            }
            float load = Time.realtimeSinceStartup - t0;
            Debug.Log($"[Reflex] smoke: world running after {load:0.0} s");
            t0 = Time.realtimeSinceStartup;
            int lines0 = ReflexPerfLog.Lines;
            while (Time.realtimeSinceStartup - t0 < seconds)
            {
                if (!rt || !rt.Running) { Fail("the race world stopped"); yield break; }
                var race = rt.Race;
                bool freeRide = race.valid == 0 || race.num_laps == 0;
                bool riding = freeRide ? Time.realtimeSinceStartup - t0 > 2f : race.state == 2 || race.state == 5;
                rt.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();   // W
                yield return null;
            }
            rt.testHeldKeys = Array.Empty<int>();
            if (exceptions > 0) { Fail($"{exceptions} exception(s), the first: {firstException}"); yield break; }
            var r = rt.Race;
            Debug.Log(string.Format(CultureInfo.InvariantCulture,
                "[Reflex] smoke OK: front end {0:0.0} s, world {1:0.0} s, {2:0} s of race (state {3}, lap {4}/{5}), {6} perf lines, last: {7}",
                up, load, seconds, r.state, r.laps_completed, r.num_laps, ReflexPerfLog.Lines - lines0, ReflexPerfLog.LastLine));
            ReflexCrashLog.WriteLine("smoke OK");
            Application.Quit(0);
        }

        void Fail(string why)
        {
            Debug.LogError("[Reflex] smoke FAIL: " + why);
            Application.Quit(1);
        }
    }
}

#endif
