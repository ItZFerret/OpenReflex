// Play-mode check of the original ambient animated entities (OriginalAmbient): opens the generic Reflex scene on an event, rides
// with throttle after the gate drop, logs the ambient counts and the simulation cost, and saves screenshots (the game camera, or
// fixed views). Batch use (no -quit; the method exits the editor itself):
//   AMBIENT_EVENT=Supercross/SX_Kingston AMBIENT_TAG=kingston AMBIENT_SHOTS=pre:6,4 AMBIENT_OUT=<dir>
//   [AMBIENT_VIEWS="px,py,pz,tx,ty,tz[,fov];..."  one fixed camera per shot, empty entry = the game camera]
//   [AMBIENT_COST=1: after the shots, average SimMilliseconds over 4 s with the ambient classes kept, then with them skipped]
//   Unity -batchmode -projectPath <bench> -executeMethod AmbientContentTests.Run -logFile <log>
// "pre:t" = t seconds after the world runs, "t" = t seconds after the race starts (or after the world runs in free ride).
// PlayerPrefs: nothing is written (the event goes through ReflexEventSelect.Pending); the user's keys are checked on exit and
// restored if anything changed them.
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class AmbientContentTests
{
    static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
    static (string key, bool had, string s, int i, float f)[] saved;
    static string outDir, tag; static (bool pre, float t)[] shots; static string[] views; static int shot;
    static double started = -1, raceStart = -1, runStart = -1, lastLog;
    static OriginalWorldRuntime runtime;
    static int costPhase; static double costStart; static double costSum; static int costN; static double costOn, costOff; static int lastSimFrame;

    static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }

    public static void Run()
    {
        string ev = Env("AMBIENT_EVENT", "National/NAT_Concord");
        tag = Env("AMBIENT_TAG", "concord");
        outDir = Env("AMBIENT_OUT", Path.Combine(Path.GetTempPath(), "ambient"));
        var list = Env("AMBIENT_SHOTS", "pre:6,4").Split(',');
        shots = list.Select(s => s.StartsWith("pre:") ? (true, float.Parse(s.Substring(4), CultureInfo.InvariantCulture)) : (false, float.Parse(s, CultureInfo.InvariantCulture))).ToArray();
        views = Env("AMBIENT_VIEWS", "").Split(';');
        Directory.CreateDirectory(outDir);
        saved = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
        ReflexEventSelect.Pending = ev; ReflexEventSelect.PendingAi = int.Parse(Env("AMBIENT_AI", "0"), CultureInfo.InvariantCulture);
        EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
        EditorApplication.update += Tick;
        started = EditorApplication.timeSinceStartup;
        EditorApplication.EnterPlaymode();
        Debug.Log($"[ambient] {tag}: {ev}, shots {string.Join(",", shots.Select(s => (s.pre ? "pre:" : "") + s.t))}");
    }

    static void Tick()
    {
        double now = EditorApplication.timeSinceStartup;
        if (now - started > Double.Parse(Env("AMBIENT_TIMEOUT", "480"), CultureInfo.InvariantCulture)) { Finish("timeout"); return; }
        if (!EditorApplication.isPlaying) return;
        if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
        if (!runtime || !runtime.Running) return;
        if (runStart < 0) runStart = now;
        runtime.cameraMode = 0;   // the field only: SetCameraMode would save the user's preference
        var race = (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(runtime);
        bool freeRide = Env("AMBIENT_EVENT", "").StartsWith("FreeRide", StringComparison.OrdinalIgnoreCase);
        bool riding = freeRide ? now - runStart > 2.0 : race.valid != 0 && (race.state == 2 || race.state == 5);
        runtime.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();   // W
        if (riding && raceStart < 0) raceStart = now;
        var a = runtime.ambient;
        if (now - lastLog > 1.0)
        {
            lastLog = now;
            Debug.Log($"[ambient] {tag} t {now - runStart:F1} race {race.state} sim {runtime.SimMilliseconds:F2} ms  ambient entities {a?.entities} drawn {a?.drawn} (crowd {a?.instancesDrawn}) bones {a?.bones} posed {a?.posedBones} main {a?.mainThreadMs:F3} ms (apply {a?.applyMs:F3} wait {a?.waitMs:F3} bake {a?.bakeMs:F3}) fps {1f / Mathf.Max(Time.unscaledDeltaTime, 1e-4f):F0}");
        }
        if (pendingShot != null)
        {
            if (Time.frameCount < pendingFrames + 3) return;   // player frames: the camera has rendered into the target
            var cam = ShotCamera(shot - 1);
            RenderTexture.active = target;
            var t = new Texture2D(1280, 720, TextureFormat.RGB24, false); t.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); t.Apply();
            RenderTexture.active = null; cam.targetTexture = null; if (cam == viewCam) viewCam.enabled = false;
            File.WriteAllBytes(Path.Combine(outDir, pendingShot + ".png"), t.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(t); target.Release(); UnityEngine.Object.DestroyImmediate(target);
            Debug.Log($"[ambient] shot {pendingShot}: drawn {a?.drawn} cam {cam.transform.position} -> {cam.transform.forward}");
            pendingShot = null;
            return;
        }
        if (shot < shots.Length)
        {
            var s = shots[shot];
            double since = s.pre ? now - runStart : raceStart < 0 ? -1 : now - raceStart;
            if (since >= s.t) { Shoot($"{tag}_{shot}_{(s.pre ? "pre" : "race")}{s.t:0.#}s", shot); shot++; }
            return;
        }
        if (Env("AMBIENT_COST", "") != "" && costPhase < 2)
        {
            // average simulation cost per original frame with the ambient classes kept (phase 0), then all skipped (phase 1)
            var simField = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.NonPublic | BindingFlags.Instance);
            var worldField = typeof(OriginalWorldRuntime).GetField("world", BindingFlags.NonPublic | BindingFlags.Instance);
            if (costStart == 0) { costStart = now; costSum = 0; costN = 0; }
            if (Time.frameCount != lastSimFrame) { lastSimFrame = Time.frameCount; costSum += runtime.SimMilliseconds; costN++; }
            if (now - costStart > 4.0)
            {
                double avg = costSum / Math.Max(1, costN);
                if (costPhase == 0) { costOn = avg; ReflexAmbientNative.reflex_world_ambient_mask((IntPtr)worldField.GetValue(runtime), 0); }
                else costOff = avg;
                Debug.Log($"[ambient] cost phase {costPhase}: sim {avg:F2} ms over {costN} frames (ambient classes {(costPhase == 0 ? "kept" : "skipped")}), main thread {a?.mainThreadMs:F3} ms");
                costPhase++; costStart = 0;
            }
            return;
        }
        if (costPhase == 2) Debug.Log($"[ambient] {tag} sim cost: {costOn:F2} ms with the ambient classes, {costOff:F2} ms without");
        Finish("done");
    }

    static string pendingShot; static int pendingFrames; static RenderTexture target;
    static Camera viewCam;
    static Camera ShotCamera(int index)
    {
        var v = index >= 0 && index < views.Length ? views[index] : "";
        if (v == "") return runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
        var f = v.Split(',').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray();
        if (!viewCam)
        {
            viewCam = new GameObject("ambient test view").AddComponent<Camera>();
            viewCam.nearClipPlane = 0.3f; viewCam.farClipPlane = 5000f;
            var main = runtime.chaseCamera;
            if (main) { viewCam.clearFlags = main.clearFlags; viewCam.backgroundColor = main.backgroundColor; }
        }
        viewCam.transform.position = new Vector3(f[0], f[1], f[2]);
        viewCam.transform.LookAt(new Vector3(f[3], f[4], f[5]));
        viewCam.fieldOfView = f.Length > 6 ? f[6] : 50f;
        if (runtime.ambient) runtime.ambient.viewer = viewCam;   // draw distances from this view
        return viewCam;
    }

    static void Shoot(string name, int index)
    {
        var cam = ShotCamera(index);
        if (!cam) return;
        target = new RenderTexture(1280, 720, 24); cam.enabled = true;
        cam.targetTexture = target; pendingShot = name; pendingFrames = Time.frameCount;
        if (cam != viewCam && runtime.ambient) runtime.ambient.viewer = cam;
    }

    static void Finish(string why)
    {
        EditorApplication.update -= Tick;
        if (runtime && runtime.ambient) Debug.Log("[ambient] report: " + runtime.ambient.report);
        foreach (var (key, had, s, i, f) in saved)
        {
            bool now = PlayerPrefs.HasKey(key);
            bool changed = now != had || (had && (PlayerPrefs.GetString(key, null) != s || PlayerPrefs.GetInt(key, int.MinValue) != i || !PlayerPrefs.GetFloat(key, float.NaN).Equals(f)));
            if (!changed) continue;
            Debug.Log($"[ambient] restoring PlayerPrefs {key}");
            if (!had) PlayerPrefs.DeleteKey(key);
            else if (s != null && s != "") PlayerPrefs.SetString(key, s);
            else if (i != int.MinValue) PlayerPrefs.SetInt(key, i);
            else PlayerPrefs.SetFloat(key, f);
        }
        PlayerPrefs.Save();
        Debug.Log("[ambient] finished: " + why);
        if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
        EditorApplication.delayCall += () => EditorApplication.Exit(why == "done" ? 0 : 1);
    }
}
