using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.Rendering;

// Test only (tests/moto_career_flow run.py --light-log, w-light): watches the HDR eye adaptation and the water while a probe rides.
// Per Unity frame into <shots>/light-water.csv: the real time, the Unity frame time, the original frame on screen
// (OriginalWorldRuntime.PresentedGameFrame), the original frames the adaptation step covered, the adapted luminance and the frame's
// measured (1 x 1) luminance read back from the post chain, and the water's ocean time / game clock / wake steps / splats / largest
// ocean height / first surface's flow offset. Every 6 s (at most 12) a screenshot lw-<n>.png. It presses nothing and writes no game state.
public sealed class LightWaterProbe : MonoBehaviour
{
    const string Tag = "[light-water-probe] ";
    string directory; StreamWriter csv; float nextShot; int shots; bool headerLogged;
    readonly StringBuilder line = new StringBuilder();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(d) || Environment.GetEnvironmentVariable("REFLEX_LIGHT_PROBE") != "1") return;
        var go = new GameObject("Light / water probe (test)"); DontDestroyOnLoad(go);
        go.AddComponent<LightWaterProbe>().directory = d;
    }

    static string F(double v) => v.ToString("0.######", CultureInfo.InvariantCulture);

    void OnDestroy() { csv?.Flush(); csv?.Dispose(); csv = null; }
    void OnApplicationQuit() { csv?.Flush(); }

    void Update()
    {
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var level = FindAnyObjectByType<OriginalLevel>();
        var pipe = level ? level.Pipeline : null;
        if (!rt || !rt.Running || !pipe) return;
        pipe.keepIntermediates = true;
        var water = level.GetComponent<OriginalWater>();
        if (csv == null)
        {
            csv = new StreamWriter(Path.Combine(directory, "light-water.csv"), false, Encoding.ASCII);
            csv.WriteLine("frame,t,dt,game_frame,adapt_frames,adapted,current,ocean_time,game_clock,wake_steps,splats,ocean_max,flow0,cam_x,cam_y,cam_z");
            nextShot = Time.realtimeSinceStartup + 6f;
        }
        if (!headerLogged)
        {
            headerLogged = true;
            var h = pipe.hdr;
            Debug.Log(Tag + $"level {level.level} hdr key {F(h.keyValue)} rate {F(h.adaptationRate)} adapt {F(h.minAdaptation)}..{F(h.maxAdaptation)} attn {F(h.bloomAttenuation)}"
                      + $" vsync {QualitySettings.vSyncCount} targetFrameRate {Application.targetFrameRate} display {Screen.currentResolution.refreshRateRatio.value:0.#} Hz pacing {rt.Pacing}"
                      + (water ? " water: " + water.report : " no water"));
        }
        int frame = Time.frameCount; float t = Time.realtimeSinceStartup, dt = Time.unscaledDeltaTime;
        double game = rt.PresentedGameFrame; float steps = pipe.LastAdaptationFrames;
        float oceanMax = float.NaN, flow0 = float.NaN;
        if (water && frame % 10 == 0)
        {
            var oc = typeof(OriginalWater).GetField("ocean", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(water);
            if (oc?.GetType().GetField("heights")?.GetValue(oc) is float[] hs) { oceanMax = 0; foreach (var v in hs) oceanMax = Mathf.Max(oceanMax, Mathf.Abs(v)); }
            if (typeof(OriginalWater).GetField("states", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(water) is IList states && states.Count > 0)
                flow0 = (float)states[0].GetType().GetField("flowOffset").GetValue(states[0]);
        }
        var cam = rt.chaseCamera ? rt.chaseCamera.transform.position : Vector3.zero;
        string rest = $"{F(water ? water.oceanTime : float.NaN)},{F(water ? water.gameClock : float.NaN)},{(water ? water.WakeSteps : 0)},{(water ? water.SplatsReceived : 0)},{F(oceanMax)},{F(flow0)},{F(cam.x)},{F(cam.y)},{F(cam.z)}";
        var adapted = pipe.AdaptedLuminance; var inter = pipe.Intermediates;
        if (adapted != null && adapted.rt != null && inter != null && inter.Length > 3 && inter[3] != null)
        {
            float a = float.NaN;
            var w = csv;
            AsyncGPUReadback.Request(adapted.rt, 0, r => { if (!r.hasError) a = r.GetData<float>()[0]; });
            AsyncGPUReadback.Request(inter[3].rt, 0, r =>
            {
                if (r.hasError || w == null || csv == null) return;
                var c = r.GetData<Vector4>()[0];
                csv.WriteLine($"{frame},{F(t)},{F(dt)},{F(game)},{F(steps)},{F(a)},{F(c.x)},{rest}");
            });
        }
        if (t >= nextShot && shots < 12)
        {
            nextShot = t + 6f; shots++;
            StartCoroutine(Capture("lw-" + shots.ToString("00")));
        }
    }

    IEnumerator Capture(string name)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(directory, name + ".png"));
        var cam = Camera.main;
        Debug.Log(Tag + "screenshot " + name + (cam ? " camera " + cam.transform.position + " fwd " + cam.transform.forward : ""));
    }
}
