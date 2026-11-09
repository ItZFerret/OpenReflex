// TEST ONLY (tests/cinematic_showcase, w-rmphoto): launches one event from the open front end, rides the local bike with the throttle held, opens REPLAY
// / PHOTO MODE (Reflex.Port.Replay.ReflexReplay) and takes matched pictures with the cinematic effects off / on at fixed settings
// (ReflexCinematic.Override). It presses nothing else and writes no game state; the save is the run's scratch directory.
// Plan (REFLEX_CINE_PLAN, JSON):
//   { "event": "National/NAT_Stillwater", "ai": 0, "ride": 12, "prefix": "c", "settle": 2, "fps": 60,
//     "shots": [ { "name": "chase_off", "face": "replay", "cam": "Chase", "back": 6, "fx": "off", "play": 0.5 },
//                { "name": "chase_all", "face": "replay", "cam": "Chase", "back": 6, "fx": "all", "play": 0.5 },
//                { "name": "photo_mb", "face": "photo", "cam": "Orbit", "back": 6, "fx": "auto", "mb": 0.5, "focus": 1, "aperture": 2 } ] }
// "back": seconds before the end of the recording; "play": seconds played at 1x before the picture (fps frames a second, fixed: Time.captureFramerate)
// so the motion vectors are those of a real playing frame; 0 = held. "fx": off | auto (the viewer's own settings) | a mix of dof, mb, lens, flare
// (e.g. "dof+mb", "all") with fixed values; "ap" / "shutter" override the aperture / the shutter angle. A replay picture is the screen
// (the viewer's UI hidden); a PHOTO MODE picture is the photo the feature itself saves (TakePhoto).
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using Reflex.Port.Replay;
using UnityEngine;

[DefaultExecutionOrder(32000)]
public sealed class CinematicShowcase : MonoBehaviour
{
    const string Tag = "[cine] ";
    [Serializable] public sealed class Shot
    {
        public string name = "shot", face = "replay", cam = "Chase", fx = "off", fx2 = "";   // fx2: a second picture of the next frame with these effects (name_b)
        public float back = 6f, play = 0f, ap = 0f, shutter = 0f, focus = 0f, mb = -1f, lens = -1f;
        public int target = 0, focusMode = -1, aperture = -1, frames = 4;
        public bool ui;        // the viewer's own UI stays on the picture
        public bool sun;       // the FREE camera behind the rider looking at the sun, sunLow degrees below it
        public float sunLow = 12f, sunYaw = 0f;
        public float fov = 0f;
    }
    [Serializable] public sealed class Plan
    {
        public string @event = "National/NAT_Stillwater", prefix = "c";
        public int ai = 0, fps = 60;
        public float ride = 12f, settle = 2f;
        public string bike = "";
        public bool enhanced;       // LIGHTING: ENHANCED (the modern post chain) instead of ORIGINAL
        public bool scan = false;   // WHEEL SCAN: live offsets of the wheels against the chassis, then the same moments in the replay
        public Shot[] shots = Array.Empty<Shot>();
    }

    Plan plan; string dir;

    // WHEEL SCAN: where the wheels (the holders and every tyre part) are in the chassis' frame, live and in the replay at the same recorded moment
    static Vector3[] WheelProbe(OriginalWorldRuntime rt, List<string> names = null)
    {
        // every part's renderer bounds centre in the chassis' frame (a spinning wheel's bounds centre stays on its axle; a detached part shows)
        var list = new List<Vector3>(); var c = rt.chassis;
        list.Add(c.InverseTransformPoint(rt.frontWheel.position)); list.Add(c.InverseTransformPoint(rt.rearWheel.position));
        names?.Add("front holder"); names?.Add("rear holder");
        if (rt.bikePose && rt.bikePose.parts != null)
            foreach (var p in rt.bikePose.parts)
            {
                var r = p ? p.GetComponent<Renderer>() : null;
                bool on = r && r.enabled && p.gameObject.activeInHierarchy;
                list.Add(on ? c.InverseTransformPoint(r.bounds.center) : Vector3.zero);
                names?.Add(p ? p.name : "?");
            }
        return list.ToArray();
    }
    int lastLiveKey = int.MinValue;
    readonly Dictionary<int, Vector3[]> liveWheels = new Dictionary<int, Vector3[]>();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string p = Environment.GetEnvironmentVariable("REFLEX_CINE_PLAN"), d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(p) || string.IsNullOrEmpty(d) || FindAnyObjectByType<CinematicShowcase>()) return;
        var go = new GameObject("Cinematic showcase (test)"); DontDestroyOnLoad(go);
        var s = go.AddComponent<CinematicShowcase>();
        s.plan = JsonUtility.FromJson<Plan>(File.ReadAllText(p)); s.dir = d;
        Application.runInBackground = true;
    }

    static ReflexCinematicSettings Fx(Shot s)
    {
        if (s.fx == "auto") return null;
        var c = new ReflexCinematicSettings();
        if (s.fx == "off") return c;
        bool all = s.fx.Contains("all");
        if (all || s.fx.Contains("dof")) { c.dof = true; c.aperture = s.ap > 0 ? s.ap : 2.8f; c.focusDistance = s.focus > 0 ? s.focus : 24f; }
        if (all || s.fx.Contains("mb")) { c.motionBlur = true; c.shutterAngle = s.shutter > 0 ? s.shutter : 270f; }
        if (all || s.fx.Contains("lens")) { c.chromatic = 0.35f; c.vignette = 0.3f; c.grain = 0.25f; }
        if (all || s.fx.Contains("flare")) { c.flare = 0.6f; c.dirt = 0.6f; }
        return c;
    }

    IEnumerator Start()
    {
        Directory.CreateDirectory(dir);
        Debug.Log(Tag + "BEGIN " + plan.@event + " shots " + string.Join(",", plan.shots.Select(v => v.name)));
        float t0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - t0 < 4f) yield return null;   // the front end settles
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
        if (plan.enhanced) ReflexEnhancedLighting.Override = 1;

        // ride: throttle held once the gate is down (a race) or after a moment (free ride), `ride` seconds of game time
        double f0 = double.NaN; float r0 = Time.realtimeSinceStartup;
        while (true)
        {
            var race = rt.Race;
            bool freeRide = race.valid == 0 || race.num_laps == 0;
            bool riding = freeRide ? Time.realtimeSinceStartup - r0 > plan.settle : race.state == 2 || race.state == 5;
            rt.testHeldKeys = riding ? new[] { 0x11 } : Array.Empty<int>();
            if (riding && double.IsNaN(f0)) f0 = rt.PresentedGameFrame;
            if (plan.scan && ReflexReplay.Of(rt) is ReflexReplay rr0 && rr0.Buffer.Count > 0)
            {
                int key = Mathf.RoundToInt(rr0.Buffer.Last.time * 1000f);
                if (key != lastLiveKey) { lastLiveKey = key; liveWheels[key] = WheelProbe(rt); }   // (only a frame the replay recorded this Unity frame)
            }
            if (!double.IsNaN(f0) && rt.PresentedGameFrame - f0 >= plan.ride * 60.0) break;
            if (Time.realtimeSinceStartup - r0 > 400) { Fail("the ride did not run"); yield break; }
            yield return null;
        }
        rt.testHeldKeys = Array.Empty<int>();
        var rep = ReflexReplay.Of(rt);
        Debug.Log(Tag + $"RIDE done: recorded {rep.Buffer.Duration:0.0} s, {rep.Buffer.Count} frames, chassis {rt.chassis.position}");
        yield return null; yield return null;

        bool opened = false;
        if (plan.scan)
        {
            if (!rep.Open(ReflexReplay.View.Replay, ReflexReplay.Origin.Live)) { Fail("the viewer would not open"); yield break; }
            opened = true;
            yield return null; yield return null;
            rep.Pause();
            float worst = 0f, sum = 0f; int n = 0, missing = 0;
            for (int i = 0; i < rep.Buffer.Count; i += 7)
            {
                float t = rep.Buffer[i].time;
                rep.Seek(t); rep.Pause();
                yield return null; yield return null;
                if (!liveWheels.TryGetValue(Mathf.RoundToInt(t * 1000f), out var live)) { missing++; continue; }
                var names = new List<string>();
                var now = WheelProbe(rt, names);
                if (now.Length != live.Length) { Debug.Log(Tag + "SCAN length mismatch"); break; }
                float d = 0f; int worstK = 0;
                for (int k = 0; k < now.Length; k++) { float e = (now[k] - live[k]).magnitude; if (e > d) { d = e; worstK = k; } }
                worst = Mathf.Max(worst, d); sum += d; n++;
                if (d > 0.02f || i % 77 == 0) Debug.Log(Tag + $"SCAN t {t - rep.Buffer.StartTime:0.00} off {d:0.0000} ft at {names[worstK]} live {live[worstK]} replay {now[worstK]}");
            }
            Debug.Log(Tag + $"SCAN DONE {n} moments compared ({missing} without a live record): worst {worst:0.0000} ft, mean {(n > 0 ? sum / n : 0):0.0000} ft");
            // MID SCAN: between two recorded frames (the replay playing blends them): every part against the mean of the two live states
            foreach (bool legacy in new[] { true, false })
            {
                ReflexReplay.LegacyBlend = legacy;
                float mworst = 0f, msum = 0f; int mn = 0; string mname = "";
                for (int i = 0; i + 1 < rep.Buffer.Count; i += 5)
                {
                    float ma = rep.Buffer[i].time, mbT = rep.Buffer[i + 1].time;
                    if (!liveWheels.TryGetValue(Mathf.RoundToInt(ma * 1000f), out var a0) || !liveWheels.TryGetValue(Mathf.RoundToInt(mbT * 1000f), out var a1)) continue;
                    rep.Seek((ma + mbT) * 0.5f); rep.Pause();
                    yield return null; yield return null;
                    var names = new List<string>(); var now = WheelProbe(rt, names);
                    if (now.Length != a0.Length || a1.Length != a0.Length) continue;
                    float d = 0f; int wk = 0;
                    for (int k = 0; k < now.Length; k++) { float e = (now[k] - (a0[k] + a1[k]) * 0.5f).magnitude; if (e > d) { d = e; wk = k; } }
                    if (d > mworst) { mworst = d; mname = names[wk]; }
                    msum += d; mn++;
                }
                Debug.Log(Tag + $"MIDSCAN {(legacy ? "OLD blend" : "NEW blend")}: {mn} mid-frame moments, worst {mworst:0.0000} ft ({mname}), mean {(mn > 0 ? msum / mn : 0):0.0000} ft");
            }
            ReflexReplay.LegacyBlend = false;
            rep.Close(); opened = false;
            yield return null; yield return null;
        }
        foreach (var s in plan.shots)
        {
            ReflexCinematic.Override = Fx(s);
            bool photo = s.face == "photo";
            var face = photo ? ReflexReplay.View.Photo : ReflexReplay.View.Replay;
            if (!rep.Active)
            {
                if (!rep.Open(face, ReflexReplay.Origin.Live)) { Fail("the viewer would not open"); yield break; }
                opened = true;
                yield return null; yield return null;
            }
            if (rep.Face != face) { rep.SetFace(face); yield return null; }
            rep.SetTarget(s.target);
            if (Enum.TryParse(s.cam, out ReflexReplay.Cam cm) && !photo) rep.SetCamera(cm);
            ReflexReplay.Photo.ResetLook();
            if (photo)
            {
                if (Enum.TryParse(s.cam, out cm)) rep.SetPhotoOrbit(cm == ReflexReplay.Cam.Orbit);
                var p = ReflexReplay.Photo;
                ApplyPhoto(p, s);
            }
            rep.Seek(rep.Buffer.EndTime - s.back);
            rep.Pause();
            if (s.fov > 1) rep.UserFov = s.fov;
            if (s.sun)
            {
                // a free camera behind the rider looking at the sun (the flare test): `sunLow` degrees below the sun
                if (rep.TargetAt(rep.Buffer.EndTime - s.back, s.target, out Vector3 tp, out _))
                {
                    Vector3 toSun = ((Vector3)level.Content.lighting.sunDirection).normalized;
                    Vector3 pos = tp + Vector3.up * 2.5f - Vector3.ProjectOnPlane(toSun, Vector3.up).normalized * 6f;
                    Vector3 aim = Quaternion.AngleAxis(s.sunYaw, Vector3.up) * (Quaternion.AngleAxis(s.sunLow, Vector3.Cross(toSun, Vector3.up)) * toSun);
                    rep.FreeCameraAt(pos, pos + aim * 100f);
                    if (s.fov <= 1) rep.UserFov = 62f;
                }
            }
            rep.UiHidden = !s.ui; rep.Ui?.FaceChanged();
            for (int i = 0; i < 4; i++) yield return null;
            if (s.play > 0f)
            {
                rep.TogglePlay();
                int n = Mathf.RoundToInt(s.play * plan.fps);
                for (int i = 0; i < n; i++) yield return null;
            }
            else for (int i = 0; i < Mathf.Max(2, s.frames); i++) yield return null;
            string file = Path.Combine(dir, $"{plan.prefix}_{s.name}.png");
            if (photo && !s.ui)
            {
                string dest = file, before = rep.LastPhotoPath;
                rep.TakePhoto();
                float w0 = Time.realtimeSinceStartup;
                yield return null;
                while ((rep.Capturing || rep.LastPhotoPath == before) && Time.realtimeSinceStartup - w0 < 120f) yield return null;
                if (rep.LastPhotoPath != null && rep.LastPhotoPath != before && File.Exists(rep.LastPhotoPath)) File.Copy(rep.LastPhotoPath, dest, true);
                else Debug.LogWarning(Tag + "no photo for " + s.name);
            }
            else
            {
                yield return new WaitForEndOfFrame();
                ScreenCapture.CaptureScreenshot(file);
                yield return null; yield return null;
                if (!string.IsNullOrEmpty(s.fx2))
                {
                    var s2 = JsonUtility.FromJson<Shot>(JsonUtility.ToJson(s)); s2.fx = s.fx2;
                    ReflexCinematic.Override = Fx(s2);
                    yield return new WaitForEndOfFrame();
                    ScreenCapture.CaptureScreenshot(Path.Combine(dir, $"{plan.prefix}_{s.name}_b.png"));
                    yield return null; yield return null;
                }
            }
            if (s.play > 0f) rep.Pause();
            Debug.Log(Tag + $"SHOT {s.name} {file} cam {rep.CameraLabel} t {rep.PlayTime - rep.Buffer.StartTime:0.00} fov {rep.ViewCamera.fieldOfView:0.0} subject {rep.SubjectDistance:0.0} ft fx {s.fx}");
        }
        if (opened) rep.Close();
        Debug.Log(Tag + "DONE");
        yield return new WaitForSeconds(1f);
        Application.Quit(0);
    }

    static void ApplyPhoto(ReflexReplay.PhotoSettings p, Shot s)
    {
        if (s.focusMode >= 0) p.focus = s.focusMode;
        if (s.aperture >= 0) p.aperture = s.aperture;
        if (s.focus > 0) p.focusDistance = s.focus;
        if (s.mb >= 0) p.motionBlur = s.mb;
        if (s.lens >= 0) { p.lens = s.lens > 0.5f; }
    }

    void Fail(string why)
    {
        Debug.LogError(Tag + "FAIL " + why);
        Application.Quit(3);
    }
}
