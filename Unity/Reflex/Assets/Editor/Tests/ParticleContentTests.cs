// Play-mode check of the original particles (OriginalParticles): opens the generic Reflex scene on an event, rides with throttle
// after the gate drop (as the native world tests do: W from race state InProgress), and saves screenshots of the game camera.
// Batch use (no -quit; the method exits the editor itself):
//   PARTICLE_EVENT=Supercross/SX_Kingston PARTICLE_TAG=kingston PARTICLE_SHOTS=pre:4,pre:9,3,6 PARTICLE_OUT=<dir>
//   Unity -batchmode -projectPath <bench> -executeMethod ParticleContentTests.Run -logFile <log>
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

public static class ParticleContentTests
{
    static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
    static (string key, bool had, string s, int i, float f)[] saved;
    static string outDir, tag; static (bool pre, float t)[] shots; static float[] debugModes; static int shot, cameraMode;
    static double started = -1, raceStart = -1, runStart = -1, lastLog;
    static OriginalWorldRuntime runtime;

    static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }

    public static void Run()
    {
        string ev = Env("PARTICLE_EVENT", "National/NAT_Concord");
        tag = Env("PARTICLE_TAG", "concord");
        outDir = Env("PARTICLE_OUT", Path.Combine(Path.GetTempPath(), "particles"));
        cameraMode = int.Parse(Env("PARTICLE_CAMERA", "0"), CultureInfo.InvariantCulture);
        // "pre:t@d" / "t@d": shader debug mode d for that shot (1 opaque quads, 2 colour only, 3 texture only, 4 no soft term, 9 hidden)
        var list = Env("PARTICLE_SHOTS", "pre:5,3,6,10").Split(',');
        debugModes = list.Select(s => s.Contains("@") ? float.Parse(s.Substring(s.IndexOf('@') + 1), CultureInfo.InvariantCulture) : float.Parse(Env("PARTICLE_DEBUG", "0"), CultureInfo.InvariantCulture)).ToArray();
        shots = list.Select(s => s.Contains("@") ? s.Substring(0, s.IndexOf('@')) : s).Select(s => s.StartsWith("pre:") ? (true, float.Parse(s.Substring(4), CultureInfo.InvariantCulture)) : (false, float.Parse(s, CultureInfo.InvariantCulture))).ToArray();
        Directory.CreateDirectory(outDir);
        saved = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
        ReflexEventSelect.Pending = ev; ReflexEventSelect.PendingAi = int.Parse(Env("PARTICLE_AI", "0"), CultureInfo.InvariantCulture);
        EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
        EditorApplication.update += Tick;
        started = EditorApplication.timeSinceStartup;
        EditorApplication.EnterPlaymode();
        Debug.Log($"[particles] {tag}: {ev}, shots {string.Join(",", shots.Select(s => (s.pre ? "pre:" : "") + s.t))}");
    }

    static void Tick()
    {
        double now = EditorApplication.timeSinceStartup;
        if (now - started > Double.Parse(Env("PARTICLE_TIMEOUT", "420"), CultureInfo.InvariantCulture)) { Finish("timeout"); return; }
        if (!EditorApplication.isPlaying) return;
        if (!runtime) runtime = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
        if (!runtime || !runtime.Running) return;
        if (runStart < 0) runStart = now;
        runtime.cameraMode = cameraMode;   // the field only: SetCameraMode would save the user's preference
        var race = (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(runtime);
        bool freeRide = Env("PARTICLE_EVENT", "").StartsWith("FreeRide", StringComparison.OrdinalIgnoreCase);
        bool riding = freeRide ? now - runStart > 2.0 : race.valid != 0 && (race.state == 2 || race.state == 5);
        runtime.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();   // W
        if (riding && raceStart < 0) raceStart = now;
        var p = runtime.particles;
        if (now - lastLog > 1.0)
        {
            lastLog = now;
            Debug.Log($"[particles] {tag} t {now - runStart:F1} race {race.state} sim {runtime.SimMilliseconds:F2} ms  particles normal {p?.Normal} additive {p?.Additive} live {p?.Live} atlas '{p?.TextureName}' fps {1f / Mathf.Max(Time.unscaledDeltaTime, 1e-4f):F0}");
        }
        if (pendingShot != null)
        {
            // the camera rendered into the target in the player loop (with the frame's RenderPrimitives draws): read it
            if (Time.frameCount < pendingFrames + 3) return;   // player frames: the camera has rendered into the target
            var cam = ShotCamera();
            RenderTexture.active = target;
            var t = new Texture2D(1280, 720, TextureFormat.RGB24, false); t.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); t.Apply();
            RenderTexture.active = null; cam.targetTexture = null; if (cam == viewCam) viewCam.enabled = false;
            File.WriteAllBytes(Path.Combine(outDir, pendingShot + ".png"), t.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(t); target.Release(); UnityEngine.Object.DestroyImmediate(target);
            var pp = runtime.particles;
            Debug.Log($"[particles] shot {pendingShot}: normal {pp?.Normal} additive {pp?.Additive} live {pp?.Live} cam {cam.transform.position}");
            pendingShot = null;
            return;
        }
        if (shot < shots.Length)
        {
            var s = shots[shot];
            double since = s.pre ? now - runStart : raceStart < 0 ? -1 : now - raceStart;
            if (since >= s.t) { Shoot($"{tag}_{shot}_{(s.pre ? "pre" : "race")}{s.t:0.#}s" + (debugModes[shot] != 0 ? "_debug" + debugModes[shot] : "")); shot++; }
        }
        else Finish("done");
    }

    static string pendingShot; static int pendingFrames; static RenderTexture target;
    static Camera viewCam;
    // PARTICLE_VIEW="px,py,pz,tx,ty,tz[,fov]": shots from a fixed extra camera (e.g. at the stadium's pyro) instead of the game's.
    static Camera ShotCamera()
    {
        var v = Env("PARTICLE_VIEW", "");
        if (v == "") return runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
        if (!viewCam)
        {
            var f = v.Split(',').Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray();
            viewCam = new GameObject("particle test view").AddComponent<Camera>();
            viewCam.transform.position = new Vector3(f[0], f[1], f[2]);
            viewCam.transform.LookAt(new Vector3(f[3], f[4], f[5]));
            viewCam.fieldOfView = f.Length > 6 ? f[6] : 50f; viewCam.nearClipPlane = 0.5f; viewCam.farClipPlane = 5000f;
            var main = runtime.chaseCamera;
            if (main) { viewCam.clearFlags = main.clearFlags; viewCam.backgroundColor = main.backgroundColor; }
        }
        return viewCam;
    }

    static void Shoot(string name)
    {
        var cam = ShotCamera();
        if (!cam) return;
        target = new RenderTexture(1280, 720, 24); cam.enabled = true;
        cam.targetTexture = target; pendingShot = name; pendingFrames = Time.frameCount;
        Shader.SetGlobalFloat("_RfxParticleDebug", debugModes[shot] == 9 ? 0f : debugModes[shot]);
        if (runtime.particles) runtime.particles.visible = debugModes[shot] != 9;   // 9: particles hidden (comparison)
    }

    static void Finish(string why)
    {
        EditorApplication.update -= Tick;
        foreach (var (key, had, s, i, f) in saved)
        {
            bool now = PlayerPrefs.HasKey(key);
            bool changed = now != had || (had && (PlayerPrefs.GetString(key, null) != s || PlayerPrefs.GetInt(key, int.MinValue) != i || !PlayerPrefs.GetFloat(key, float.NaN).Equals(f)));
            if (!changed) continue;
            Debug.Log($"[particles] restoring PlayerPrefs {key}");
            if (!had) PlayerPrefs.DeleteKey(key);
            else if (s != null && s != "") PlayerPrefs.SetString(key, s);
            else if (i != int.MinValue) PlayerPrefs.SetInt(key, i);
            else PlayerPrefs.SetFloat(key, f);
        }
        PlayerPrefs.Save();
        Debug.Log("[particles] finished: " + why);
        if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode();
        EditorApplication.delayCall += () => EditorApplication.Exit(why == "done" ? 0 : 1);
    }
}
