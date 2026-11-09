// TEST ONLY (tests/replayterrain, w-replayterrain): launches one event from the open front end, rides the local bike (throttle held) with the
// computer riders, renders the live gameplay camera, then opens REPLAY / PHOTO MODE (Reflex.Port.Replay.ReflexReplay) and renders planned shots,
// in a -batchmode player (no window, never takes the focus): the game camera is rendered into a target and read back. The game's sound is muted
// (AudioListener.volume 0 every frame). It presses nothing else and writes no game state; the save is the run's scratch directory.
// Plan (REFLEX_RT_PLAN, JSON):
//   { "event": "National/NAT_Concord", "ai": 5, "ride": 40, "prefix": "concord", "fps": 60,
//     "shots": [ { "name": "game", "kind": "game", "mode": "original" },
//                { "name": "high", "kind": "replay", "cam": "Free", "back": 8, "pos": [0, 70, -60], "look": [0, 0, 20], "fov": 60, "fx": "auto" },
//                { "name": "chase", "kind": "replay", "cam": "Chase", "back": 6, "play": 0.5, "fx": "auto" },
//                { "name": "photo", "kind": "photo", "back": "live", "pos": [12, 4, 30], "look": [0, 2.5, 0], "fov": 9, "focusMode": 2, "focus": 40,
//                  "aperture": 8, "mb": 0.25, "frames": 40, "saved": true } ] }
// "back": seconds before the end of the recording, or "live" (the end), "air" (the local rider highest above the ground), "fast" (fastest);
// "pos" / "look": offsets from the followed rider in his heading's frame (x right, y up, z ahead); "mode": original / enhanced (both when
// omitted: one picture each); "fx": auto (the viewer's own settings) or off; "frames": frames rendered before the picture (the photo's shutter
// preview builds up one sub-frame a frame); "saved": also the photo as TAKE PHOTO renders it (every sub-frame in one frame, no TAA).
// Pictures: <prefix>_<name>_<mode>.png (+ _saved, + _old with "pair"). REFLEX_SHOTS_TAA=1: ENHANCED's temporal AA on the offscreen target as on screen.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using Reflex.Port.Replay;
using UnityEngine;
using UnityEngine.Rendering;

[DefaultExecutionOrder(32000)]
public sealed class ReplayTerrainShowcase : MonoBehaviour
{
    const string Tag = "[rt-showcase] ";
    [Serializable] public sealed class Shot
    {
        public string name = "shot", kind = "replay", cam = "Free", fx = "auto", back = "6", mode = "";
        public float[] pos, look;
        public float play = 0f, fov = 0f, focus = 0f, mb = -1f;
        public int target = 0, focusMode = -1, aperture = -1, frames = 6;
        public bool saved;
        public bool pair;   // also the old behaviour at the same moment (<name>_<mode>_old.png): the RT_Bump layout of the last riding frame
                            // (ReflexRTBump.FreezeHold), the replay's old lens (f/2.8 chase, 4 ft zone, 180 degree shutter on every frame), the
                            // photo's old shutter (centred on the moment, TAA history kept, riders skinned once: ReflexReplay.LegacyShutter)
    }
    [Serializable] public sealed class Plan
    {
        public string @event = "National/NAT_Concord", prefix = "rt";
        public int ai = 5, fps = 60;
        public float ride = 40f, gameAt = 20f;
        public Shot[] shots = Array.Empty<Shot>();
    }

    Plan plan; string dir; int W = 1600, H = 900; int lastRender = -1;
    int autoRenders; Camera counted;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string p = Environment.GetEnvironmentVariable("REFLEX_RT_PLAN"), d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(p) || string.IsNullOrEmpty(d) || FindAnyObjectByType<ReplayTerrainShowcase>()) return;
        var go = new GameObject("Replay terrain showcase (test)"); DontDestroyOnLoad(go);
        var s = go.AddComponent<ReplayTerrainShowcase>();
        s.plan = JsonUtility.FromJson<Plan>(File.ReadAllText(p)); s.dir = d;
        var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
        if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var q && q.Length == 2) { int.TryParse(q[0], out s.W); int.TryParse(q[1], out s.H); }
        Application.runInBackground = true;
        AudioListener.volume = 0f;
    }

    void OnEnable() { RenderPipelineManager.beginCameraRendering += OnCam; }
    void OnDisable() { RenderPipelineManager.beginCameraRendering -= OnCam; }
    void OnCam(ScriptableRenderContext c, Camera cam) { if (cam == counted) autoRenders++; }
    void Update() { AudioListener.volume = 0f; }   // muted test player (the user's sound)

    static void SetStatic(Type t, string name, object v)
    {
        var f = t.GetField(name, BindingFlags.Public | BindingFlags.Static);
        if (f != null) f.SetValue(null, v); else Debug.Log(Tag + "no " + t.Name + "." + name + " in this build");
    }

    IEnumerator Start()
    {
        Directory.CreateDirectory(dir);
        Debug.Log(Tag + "BEGIN " + plan.@event + " shots " + string.Join(",", plan.shots.Select(v => v.name)));
        float t0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - t0 < 4f) yield return null;
        try
        {
            var ev = ReflexEvents.Find(plan.@event) ?? ReflexEvents.All.FirstOrDefault(e => e.key.StartsWith(plan.@event, StringComparison.OrdinalIgnoreCase));
            if (ev == null) throw new InvalidOperationException("no event " + plan.@event);
            Debug.Log(Tag + "LAUNCH " + ev.key);
            ReflexEventSelect.Launch(ev.key, plan.ai, false, "Reflex", 0, null);
        }
        catch (Exception e) { Fail(e.ToString()); yield break; }

        OriginalWorldRuntime rt = null; OriginalLevel level = null;
        float wait = Time.realtimeSinceStartup;
        while (true)
        {
            rt = FindAnyObjectByType<OriginalWorldRuntime>(); level = FindAnyObjectByType<OriginalLevel>();
            if (rt && rt.FailureMessage != null) { Fail("world: " + rt.FailureMessage); yield break; }
            if (rt && level && level.Loaded && rt.Running && !double.IsNaN(rt.PresentedGameFrame)) break;
            if (Time.realtimeSinceStartup - wait > 600) { Fail("the event did not start"); yield break; }
            yield return null;
        }
        Debug.Log(Tag + "RUNNING " + level.level + " load " + (Time.realtimeSinceStartup - wait).ToString("0.0") + " s");
        Time.captureFramerate = Mathf.Max(10, plan.fps);
        var cam = rt.chaseCamera;
        counted = cam; autoRenders = 0;
        for (int i = 0; i < 10; i++) yield return null;
        bool auto = autoRenders >= 5;
        Debug.Log(Tag + $"AUTO renders in 10 frames: {autoRenders} -> {(auto ? "the camera renders by itself into the target" : "manual renders")}");
        counted = null;
        Reflex.Port.Options.ReflexSettings.HudHidden = true;

        // ride: throttle held once the gate is down (a race) or after a moment (free ride); the gameplay shots on the way
        double f0 = double.NaN; float r0 = Time.realtimeSinceStartup;
        bool gameDone = false;
        while (true)
        {
            Reflex.Port.Options.ReflexSettings.HudHidden = true;
            var race = rt.Race;
            bool freeRide = race.valid == 0 || race.num_laps == 0;
            bool riding = freeRide ? Time.realtimeSinceStartup - r0 > 3f : race.state == 2 || race.state == 5;
            rt.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();
            if (riding && double.IsNaN(f0)) f0 = rt.PresentedGameFrame;
            if (!gameDone && !double.IsNaN(f0) && rt.PresentedGameFrame - f0 >= plan.gameAt * 60.0)
            {
                gameDone = true;
                foreach (var s in plan.shots.Where(q => q.kind == "game"))
                    foreach (var mode in Modes(s))
                    {
                        ReflexEnhancedLighting.Override = mode == "enhanced" ? 1 : 0;
                        yield return Render(cam, auto, Math.Max(6, s.frames), $"{plan.prefix}_{s.name}_{mode}.png", mode == "enhanced");
                        Debug.Log(Tag + $"SHOT {s.name} {mode} gameplay camera {cam.transform.position} fwd {cam.transform.forward} fov {cam.fieldOfView:0.0} framesHeld {Held(rt)}");
                    }
            }
            if (!double.IsNaN(f0) && rt.PresentedGameFrame - f0 >= plan.ride * 60.0) break;
            if (Time.realtimeSinceStartup - r0 > 900) { Fail("the ride did not run"); yield break; }
            yield return null;
        }
        rt.testHeldKeys = Array.Empty<int>();
        // the last riding frame's RT_Bump layout (the old viewer kept it: the "old" pictures)
        ReflexRTBump.Fit staleFit = default; bool haveStale = false;
        if (level.RTBump)
        {
            yield return Render(cam, auto, 2, null, false);
            staleFit = level.RTBump.LastFit; haveStale = true;
            Debug.Log(Tag + $"STALE fit origin {staleFit.origin} fwd {staleFit.forward} (camera {cam.transform.position})");
        }
        var rep = ReflexReplay.Of(rt);
        Debug.Log(Tag + $"RIDE done: recorded {rep.Buffer.Duration:0.0} s, {rep.Buffer.Count} frames, chassis {rt.chassis.position}");
        yield return null; yield return null;

        foreach (var s in plan.shots.Where(q => q.kind != "game"))
            foreach (var mode in Modes(s))
            foreach (bool old in s.pair ? new[] { false, true } : new[] { false })
            {
                Legacy(old, s.kind == "photo", level, staleFit, haveStale);
                ReflexEnhancedLighting.Override = mode == "enhanced" ? 1 : 0;
                bool photo = s.kind == "photo";
                var face = photo ? ReflexReplay.View.Photo : ReflexReplay.View.Replay;
                if (!rep.Active)
                {
                    if (!rep.Open(face, ReflexReplay.Origin.Live)) { Fail("the viewer would not open"); yield break; }
                    yield return null; yield return null;
                }
                if (rep.Face != face) { rep.SetFace(face); yield return null; }
                ReflexCinematic.Override = s.fx == "off" ? new ReflexCinematicSettings() : old && !photo ? OldLook(s.cam, s.play > 0f) : null;
                rep.SetTarget(s.target);
                ReflexReplay.Photo.ResetLook();
                if (photo)
                {
                    var p = ReflexReplay.Photo;
                    if (s.focusMode >= 0) p.focus = s.focusMode;
                    if (s.aperture >= 0) p.aperture = s.aperture;
                    if (s.focus > 0) p.focusDistance = s.focus;
                    p.motionBlur = s.mb >= 0 ? s.mb : 0f;
                    p.lens = false;
                    rep.SetPhotoOrbit(s.cam == "Orbit");
                }
                else if (Enum.TryParse(s.cam, out ReflexReplay.Cam cm)) rep.SetCamera(cm);
                float t = When(rep, s);
                if (photo)
                {   // the followed rider around the moment: the recorded frames' times and the rider's speed over each step (the shutter's motion)
                    var sb = new System.Text.StringBuilder();
                    int ia = rep.Buffer.IndexAt(t);
                    for (int k = Math.Max(1, ia - 4); k <= Math.Min(rep.Buffer.Count - 1, ia + 2); k++)
                    {
                        float ta = rep.Buffer[k - 1].time, tb = rep.Buffer[k].time;
                        rep.TargetAt(ta, s.target, out Vector3 p0, out _); rep.TargetAt(tb, s.target, out Vector3 p1, out _);
                        sb.Append($" [{k} {tb - rep.Buffer.StartTime:0.0000} dt {(tb - ta) * 1000f:0.0} ms {(tb > ta ? (p1 - p0).magnitude / (tb - ta) : 0f):0.0} ft/s]");
                    }
                    Debug.Log(Tag + $"FRAMES {s.name} at {t - rep.Buffer.StartTime:0.0000} of {rep.Buffer.Count}:{sb}");
                }
                rep.Seek(t); rep.Pause();
                if (s.pos != null && s.pos.Length == 3 && rep.TargetAt(t, s.target, out Vector3 tp, out Quaternion tq))
                {
                    var fw = tq * Vector3.forward; fw.y = 0; fw = fw.sqrMagnitude > 1e-4f ? fw.normalized : Vector3.forward;
                    var rt3 = Vector3.Cross(Vector3.up, fw);
                    Vector3 Off(float[] o) => tp + rt3 * o[0] + Vector3.up * o[1] + fw * o[2];
                    rep.FreeCameraAt(Off(s.pos), Off(s.look != null && s.look.Length == 3 ? s.look : new[] { 0f, 2.5f, 0f }));
                }
                if (s.fov > 1) rep.UserFov = s.fov;
                rep.UiHidden = true; rep.Ui?.FaceChanged();
                yield return Render(cam, auto, 3, null, mode == "enhanced");   // (rendered as on screen: every frame the viewer poses is drawn)
                if (s.play > 0f)
                {
                    rep.TogglePlay();
                    yield return Render(cam, auto, Mathf.RoundToInt(s.play * plan.fps), null, mode == "enhanced");
                }
                if (ReflexCinematic.Override != null && old && !photo && s.fx != "off") ReflexCinematic.Override.focusDistance = Mathf.Max(1f, rep.SubjectDistance);
                string sfx = old ? "_old" : "";
                string file = Path.Combine(dir, $"{plan.prefix}_{s.name}_{mode}{sfx}.png");
                yield return Render(cam, auto, Math.Max(2, s.frames), Path.GetFileName(file), mode == "enhanced");
                var fit = level.RTBump ? level.RTBump.LastFit : default;
                Debug.Log(Tag + $"SHOT {s.name}{sfx} {mode} {rep.CameraLabel} t {rep.PlayTime - rep.Buffer.StartTime:0.00}/{rep.Buffer.Duration:0.00} camera {cam.transform.position} fwd {cam.transform.forward} fov {cam.fieldOfView:0.0} subject {rep.SubjectDistance:0.0} ft fx {s.fx} rtbump origin {fit.origin} fwd {fit.forward} near {fit.near:0.0} framesHeld {Held(rt)} heldFit {Stat(typeof(ReflexRTBump), "HeldFitMs")} ms");
                if (s.play > 0f) rep.Pause();
                if (photo && s.saved)
                {
                    // TAKE PHOTO's render (ReflexReplay.Photo Capture: the camera into a target, ShutterRender: every sub-frame in this frame)
                    var rtx = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32) { antiAliasing = 1 }; rtx.Create();
                    var prior = cam.targetTexture;
                    SetStatic(typeof(ReflexTemporalAA), "OffscreenTest", false);
                    cam.targetTexture = rtx;
                    try { typeof(ReflexReplay).GetMethod("ShutterRender", BindingFlags.NonPublic | BindingFlags.Instance).Invoke(rep, null); }
                    catch (Exception e) { Debug.Log(Tag + "ShutterRender " + e); }
                    cam.targetTexture = prior;
                    Save(rtx, Path.Combine(dir, $"{plan.prefix}_{s.name}_{mode}{sfx}_saved.png"));
                    rtx.Release(); Destroy(rtx);
                    for (int i = 0; i < 3; i++) yield return null;
                }
            }
        Legacy(false, false, level, staleFit, haveStale);
        if (rep.Active) rep.Close();
        ReflexCinematic.Override = null;
        Debug.Log(Tag + "DONE");
        for (int i = 0; i < 30; i++) yield return null;
        Application.Quit(0);
    }

    // the old behaviour on / off (test flags of this build; an older build without them logs it)
    static void Legacy(bool on, bool photo, OriginalLevel level, ReflexRTBump.Fit stale, bool haveStale)
    {
        if (level.RTBump && haveStale)
        {
            ReflexRTBump.FreezeHold = on;
            var b = level.RTBump; var t = typeof(ReflexRTBump); const BindingFlags F = BindingFlags.NonPublic | BindingFlags.Instance;
            t.GetField("frozen", F)?.SetValue(b, on);
            if (on) t.GetField("frozenFit", F)?.SetValue(b, stale);
        }
        SetStatic(typeof(ReflexReplay), "LegacyShutter", on && photo ? 5 : 0);
        SetStatic(typeof(ReflexCinematic), "KeepHistory", on && photo);
    }

    // the replay's lens before w-replayterrain (ReflexReplay.Cinematic.cs LookFor / CinematicFrame as they were)
    ReflexCinematicSettings OldLook(string cam, bool playing)
    {
        var s = new ReflexCinematicSettings { dof = true, aperture = 4f, focalScale = 2.4f, maxBlur = 0.025f, highlights = 1.5f, focusZone = 4f,
            motionBlur = true, shutterAngle = 180f, worldDt = playing ? 1f / Mathf.Max(10, plan.fps) : 0f, maxMotion = 0.04f,
            chromatic = 0.2f, vignette = 0.2f, grain = 0.12f, flare = 0.4f, dirt = 0.35f };
        switch (cam)
        {
            case "Trackside": case "Helicopter": s.focalScale = 1.4f; break;
            case "Chase": s.aperture = 2.8f; break;
            case "Onboard": s.dof = false; s.shutterAngle = 220f; s.vignette = 0.26f; s.chromatic = 0.28f; break;
            case "Wheel": s.dof = false; s.shutterAngle = 220f; s.vignette = 0.24f; break;
            case "Game": s.aperture = 5.6f; break;
        }
        return s;
    }

    static string Stat(Type t, string name) { var f = t.GetField(name, BindingFlags.Public | BindingFlags.Static); return f != null ? string.Format(System.Globalization.CultureInfo.InvariantCulture, "{0:0.000}", f.GetValue(null)) : "n/a"; }

    static string Held(OriginalWorldRuntime rt)
    {
        var p = typeof(OriginalWorldRuntime).GetProperty("FramesHeld");
        return p != null ? p.GetValue(rt).ToString() : "n/a";
    }

    static IEnumerable<string> Modes(Shot s) => string.IsNullOrEmpty(s.mode) ? new[] { "original", "enhanced" } : s.mode.Split('+');

    // the moment: seconds before the end, the end ("live"), the local rider's highest moment above the ground ("air"), the fastest ("fast")
    static float When(ReflexReplay rep, Shot s)
    {
        float end = rep.Buffer.EndTime, start = rep.Buffer.StartTime;
        if (s.back == "live") return end;
        if (s.back == "air" || s.back == "fast")
        {
            float best = -1e9f, bestT = end - 2f; Vector3 prev = default; float prevT = 0;
            for (int i = 0; i < rep.Buffer.Count; i++)
            {
                float t = rep.Buffer[i].time;
                if (t < start + 1f || t > end - 1f || !rep.TargetAt(t, s.target, out Vector3 p, out _)) continue;
                float score;
                if (s.back == "air") { float g = ReflexRTBump.UnityTerrainHeight(p.x, p.z); score = float.IsInfinity(g) ? -1e9f : p.y - g; }
                else
                {
                    if (prevT > 0f && t - prevT < 0.1f) continue;   // (over a tenth of a second: the recorded frames can share a moment)
                    score = prevT > 0f ? (p - prev).magnitude / (t - prevT) : 0f;
                    if (score > 120f) score = 0f;   // (a reset: the rider put back on the track, not riding speed)
                    prev = p; prevT = t;
                }
                if (score > best) { best = score; bestT = t; }
            }
            Debug.Log(Tag + $"MOMENT {s.back}: t {bestT - start:0.00} score {best:0.00}");
            return bestT;
        }
        float.TryParse(s.back, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float back);
        return Mathf.Max(start, end - back);
    }

    // `frames` frames rendered into a target (the camera by itself when the player renders it, else once a frame by hand), the last read back
    IEnumerator Render(Camera cam, bool auto, int frames, string name, bool enhanced)
    {
        var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
        var prior = cam.targetTexture;
        bool taa = enhanced && Environment.GetEnvironmentVariable("REFLEX_SHOTS_TAA") == "1";
        SetStatic(typeof(ReflexTemporalAA), "OffscreenTest", taa);
        cam.targetTexture = rtx;
        if (lastRender == Time.frameCount) yield return null;   // (one render a frame, as on screen)
        for (int i = 0; i < frames; i++)
        {
            if (!auto) cam.Render();
            lastRender = Time.frameCount;
            if (i < frames - 1) yield return null;
        }
        cam.targetTexture = prior;
        SetStatic(typeof(ReflexTemporalAA), "OffscreenTest", false);
        if (name != null) Save(rtx, Path.Combine(dir, name));
        RenderTexture.ReleaseTemporary(rtx);
    }

    static void Save(RenderTexture rtx, string file)
    {
        var prev = RenderTexture.active; RenderTexture.active = rtx;
        int w = rtx.width, h = rtx.height;
        var t = new Texture2D(w, h, TextureFormat.RGB24, false);
        t.ReadPixels(new Rect(0, 0, w, h), 0, 0); t.Apply(false);
        RenderTexture.active = prev;
        if (System.Environment.GetEnvironmentVariable("REFLEX_SHOT_FLIP") == "1")   // (offscreen renders come back upright: the display gamma ramp no longer turns them, ReflexGammaOutput)
        {   // (Metal / D3D: the target's rows come back top first)
            var px = t.GetPixels32(); var o = new Color32[px.Length];
            for (int y = 0; y < h; y++) Array.Copy(px, y * w, o, (h - 1 - y) * w, w);
            t.SetPixels32(o); t.Apply(false);
        }
        File.WriteAllBytes(file, t.EncodeToPNG()); Destroy(t);
        Debug.Log(Tag + "WROTE " + file);
    }

    void Fail(string why)
    {
        Debug.LogError(Tag + "FAIL " + why);
        Application.Quit(3);
    }
}
