// TEST ONLY (run.py --arrival <level>, w-frscene): the Free Ride arrival scene in the open product. The open front end's FREE RIDE at
// the level (FR_Alpine: the hovercraft, FR_Cheyenne: the sky crane, FR_Gibson: the box car plane and the parachute drop), started like a
// player does; then it only watches: while the core reports the intro scenario (OriginalPodium.scenario 3: FreerideIntroScenario step 1
// the arrival, 4 / 5 the hero shot) it captures "arrival-<step>-<n>.png" every 1.5 s of real time and logs what Unity draws of the
// scenario actors (OriginalAmbient's ScenarioAnimEntity group) and the cast; when the intro has ended and the Free Ride runs it captures
// "arrival-after.png" and quits with PASS. It presses no key during the scene and writes no game state.
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

public sealed class FreeRideArrivalProbe : MonoBehaviour
{
    const string Tag = "[freeride-arrival-probe] ";
    string level, key, shotDirectory;
    double started, browserOpenedAt, lastSubmit, candidateSince, lastShot = -10, introEnded = -1;
    int candidateFrame, paintedFrame = -1, lastStep = -1, shotsInStep, exceptions;
    bool opened, selected, launched, introSeen, finished;
    VisualElement candidate; Action<MeshGenerationContext> draw;

    public static void Boot()
    {
        string lv = Environment.GetEnvironmentVariable("REFLEX_FREERIDE_ARRIVAL_PROBE");
        if (string.IsNullOrEmpty(lv) || FindAnyObjectByType<FreeRideArrivalProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Free Ride arrival TEST ONLY"); DontDestroyOnLoad(go);
        var p = go.AddComponent<FreeRideArrivalProbe>(); p.level = lv; p.key = "FreeRide/" + lv; Application.runInBackground = true;
    }

    void Start()
    {
        started = Time.realtimeSinceStartupAsDouble;
        Application.logMessageReceived += (m, s, t) => { if (t == LogType.Exception) exceptions++; };
        shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrWhiteSpace(shotDirectory)) { Fail("no screenshot directory"); return; }
        Directory.CreateDirectory(shotDirectory);
        Debug.Log(Tag + "BEGIN " + key);
    }

    void Update()
    {
        if (finished) return;
        try { Tick(); } catch (Exception e) { Fail(e.ToString()); }
    }

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 900) throw new TimeoutException("Arrival wall budget exceeded.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s): " + exceptions);
        if (OpenShell.Instance == null && !launched) return;
        if (!opened)
        {
            if (now - started < 2) return;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenShell.Instance.ShowBrowser(true);
            opened = true; browserOpenedAt = now; Debug.Log(Tag + "BROWSER_OPENED free ride");
            return;
        }
        if (!selected)
        {
            var card = FindCard(key);
            if (card == null) { if (now - browserOpenedAt > 180) throw new TimeoutException("The Free Ride browser has no " + key + " card."); return; }
            if (Submit(card, now)) { selected = true; Debug.Log(Tag + "USER selected " + key); }
            return;
        }
        if (!launched)
        {
            var start = Find("ou-setup-start");
            if (start == null) return;
            if (Submit(start, now)) { launched = true; Debug.Log(Tag + "USER START RIDE " + key); }
            return;
        }
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        if (rt != null && rt.FailureMessage != null) throw new InvalidOperationException("World: " + rt.FailureMessage);
        if (rt == null || !rt.Running) return;
        var podium = rt.podium;
        int scenario = podium ? podium.scenario : 0, step = podium ? podium.step : 0;
        if (scenario == 3)
        {
            if (!introSeen) { introSeen = true; Debug.Log(Tag + "INTRO_STARTED " + key); }
            if (step != lastStep) { lastStep = step; shotsInStep = 0; lastShot = now - 0.7; Debug.Log(Tag + "INTRO_STEP " + step + " cast drawn " + podium.drawn); }
            if (now - lastShot >= 1.5 && shotsInStep < 8) { lastShot = now; shotsInStep++; StartCoroutine(Capture("arrival-" + step + "-" + shotsInStep, rt)); }
            return;
        }
        if (!introSeen) { if (now - browserOpenedAt > 400) throw new TimeoutException("No intro scenario (scenario 3) reported for " + key); return; }
        if (introEnded < 0) { introEnded = now; Debug.Log(Tag + "INTRO_ENDED " + key + " race state " + rt.Race.state); return; }
        if (now - introEnded > 2)
        {
            if (Environment.GetEnvironmentVariable("REFLEX_FREERIDE_HOVER_PROBE") == "1") { if (!hoverStarted) { hoverStarted = true; StartCoroutine(HoverScript(rt)); } return; }
            if (Environment.GetEnvironmentVariable("REFLEX_FREERIDE_WAKE_PROBE") == "1") { if (!hoverStarted) { hoverStarted = true; StartCoroutine(Guarded(WakeSteps(rt))); } return; }
            finished = true; StartCoroutine(Succeed(rt));
        }
    }
    bool hoverStarted;

    // w-water (run.py --arrival FR_Alpine --wake): after the arrival, the bike ridden with mapped keys (throttle, steer pulses at the heading
    // error) off the dock into the lake: the shore before, the bike entering and in the water every 0.4 s, and once it is out again; the
    // water's splats (OriginalWater.SplatsReceived), wake steps and per-frame cost are logged. Presses only the mapped keys.
    IEnumerator Guarded(IEnumerator inner)
    {
        while (true)
        {
            bool more;
            try { more = inner.MoveNext(); } catch (Exception e) { Fail(e.ToString()); yield break; }
            if (!more) break;
            yield return inner.Current;
        }
    }
    static int Scan(RemapTable t, string name)
    {
        int col = Array.IndexOf(t.columns, name);
        if (col < 0 || col >= t.kb.Length) throw new InvalidOperationException("Missing keyboard action " + name);
        var matches = Enumerable.Range(1, 255).Where(i => t.scan[i] == t.kb[col]).ToArray();
        if (t.kb[col] == 0 || matches.Length != 1) throw new InvalidOperationException("Nonunique scan code for " + name);
        return matches[0];
    }
    string WakeState(OriginalWorldRuntime rt)
    {
        var w = rt.GetComponentInChildren<OriginalWater>() ?? FindAnyObjectByType<OriginalWater>();
        var b = rt.Bike;
        string bike = b.position != null ? V(new Vector3(b.position[0], b.position[1], b.position[2])) + " speed " + b.speed.ToString("F1", CultureInfo.InvariantCulture) : "-";
        return "bike " + bike + (w ? " splats " + w.SplatsReceived + " wake steps " + w.WakeSteps + " tick " + w.Milliseconds.ToString("F3", CultureInfo.InvariantCulture) + " ms wake " + w.WakeMilliseconds.ToString("F3", CultureInfo.InvariantCulture) + " ms" : " no water")
             + " frame " + (Time.unscaledDeltaTime * 1000f).ToString("F1", CultureInfo.InvariantCulture) + " ms";
    }
    IEnumerator WakeShot(string name, OriginalWorldRuntime rt)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
        var cam = Camera.main;
        Debug.Log(Tag + "WAKE_SHOT " + name + " " + WakeState(rt) + (cam ? " camera " + V(cam.transform.position) : ""));
    }
    // the wake target the water samples (R32F heights): its largest |value|, the texels above 1e-3, and a picture (mid grey = 0, x 40)
    void WakeField(string name, OriginalWater water)
    {
        var tex = water.CurrentWake as RenderTexture;
        if (!tex) { Debug.Log(Tag + "WAKE_FIELD " + name + " no target"); return; }
        UnityEngine.Rendering.AsyncGPUReadback.Request(tex, 0, r =>
        {
            if (r.hasError) { Debug.Log(Tag + "WAKE_FIELD " + name + " readback failed"); return; }
            var d = r.GetData<float>(); float max = 0; int above = 0;
            var img = new Texture2D(tex.width, tex.height, TextureFormat.RGB24, false);
            var px = new Color32[d.Length];
            for (int i = 0; i < d.Length; i++)
            {
                float v = d[i]; if (float.IsNaN(v)) v = 0; float a = Mathf.Abs(v); if (a > max) max = a; if (a > 1e-3f) above++;
                byte g = (byte)Mathf.Clamp(128 + v * 40f * 127f, 0, 255); px[i] = new Color32(g, g, g, 255);
            }
            img.SetPixels32(px); img.Apply(false);
            File.WriteAllBytes(Path.Combine(shotDirectory, name + ".png"), img.EncodeToPNG()); Destroy(img);
            Debug.Log(Tag + "WAKE_FIELD " + name + " max |h| " + max.ToString("G4", CultureInfo.InvariantCulture) + " texels above 1e-3: " + above + " of " + d.Length);
        });
    }
    IEnumerator WakeSteps(OriginalWorldRuntime rt)
    {
        var controls = CoreRemapping.Read(false);
        int throttle = Scan(controls, "Throttle"), brake = Scan(controls, "Brake"), left = Scan(controls, "SteerLeft"), right = Scan(controls, "SteerRight");
        var water = FindAnyObjectByType<OriginalWater>();
        if (!water) throw new InvalidOperationException("no OriginalWater on " + key);
        Debug.Log(Tag + "WAKE_READY " + water.report.Replace("\n", " | ") + " " + WakeState(rt) + " pacing " + rt.Pacing);
        // how the ocean / flow time moves per Unity frame (the "choppy" check): 150 frames standing; the ocean time's step per frame
        // and the step the whole presented game frames give (the rule before w-water); their spread is the judder
        {
            var oceanSteps = new System.Collections.Generic.List<float>(); var frameSteps = new System.Collections.Generic.List<float>();
            float lastOcean = water.oceanTime; double lastFrame = Math.Floor(rt.PresentedGameFrame); float lastWholeClock = rt.PresentedGameTime;
            for (int i = 0; i < 150; i++)
            {
                yield return null;
                float o = water.oceanTime; double fr = Math.Floor(rt.PresentedGameFrame); float c = rt.PresentedGameTime;
                if (float.IsNaN(c) || double.IsNaN(fr)) continue;
                oceanSteps.Add(o - lastOcean); lastOcean = o;
                if (fr != lastFrame) { frameSteps.Add(c - lastWholeClock); lastWholeClock = c; lastFrame = fr; } else frameSteps.Add(0f);
            }
            string Stat(System.Collections.Generic.List<float> v)
            {
                if (v.Count == 0) return "-";
                float mean = v.Average(), sd = Mathf.Sqrt(v.Select(x => (x - mean) * (x - mean)).Average());
                return "mean " + (mean * 1000f).ToString("F2", CultureInfo.InvariantCulture) + " ms sd " + (sd * 1000f).ToString("F2", CultureInfo.InvariantCulture) + " ms zero " + v.Count(x => x == 0f) + "/" + v.Count;
            }
            Debug.Log(Tag + "WAKE_OCEAN_STEPS per Unity frame: ocean " + Stat(oceanSteps) + "; whole game frames " + Stat(frameSteps) + "; unity frame " + (Time.unscaledDeltaTime * 1000f).ToString("F1", CultureInfo.InvariantCulture) + " ms");
        }
        // the nearest lake point at least 4 ft deep (the hovercraft's ground: terrain / water planes)
        var b0 = rt.Bike; var p0 = new Vector2(b0.position[0], b0.position[2]); Vector2 target = new Vector2(1785f, -1657f);
        var g = OriginalHovercraft.Instance != null ? OriginalHovercraft.Instance.Ground : null;
        if (g != null)
        {
            bool found = false;
            for (float r = 20; r <= 600 && !found; r += 10)
                for (int a = 0; a < 72 && !found; a++)
                {
                    float ang = a * 5f * Mathf.Deg2Rad, x = p0.x + Mathf.Sin(ang) * r, z = p0.y + Mathf.Cos(ang) * r, w = g.Water(x, z);
                    if (!float.IsNaN(w) && g.Terrain(x, z) < w - 4f) { target = new Vector2(x, z); found = true; }
                }
        }
        Debug.Log(Tag + "WAKE_TARGET " + target.x.ToString("F1", CultureInfo.InvariantCulture) + "," + target.y.ToString("F1", CultureInfo.InvariantCulture) + " from " + V(new Vector3(p0.x, 0, p0.y)));
        yield return WakeShot("wake-0-shore", rt);
        int splats0 = water.SplatsReceived; double t0 = Time.realtimeSinceStartupAsDouble, inWater = -1, lastShot = 0, slowSince = -1, reverseUntil = -1; int shots = 0, wp = 0; bool beyond = false;
        // off the hovercraft's deck the way the core's own ride took (core/tools/water_splats_test.c alpine): back off the bow, round
        // to the north, then down the bank into the lake; then straight on
        var route = new[] { new Vector2(1700f, -1640f), new Vector2(1695f, -1605f), new Vector2(1720f, -1586f), new Vector2(1745f, -1600f), target };
        while (Time.realtimeSinceStartupAsDouble - t0 < 60)
        {
            var b = rt.Bike; var p = new Vector2(b.position[0], b.position[2]);
            var q = new Quaternion(b.orientation[0], b.orientation[1], b.orientation[2], b.orientation[3]);
            Vector3 f3 = q * Vector3.forward; var fwd = new Vector2(f3.x, f3.z);
            while (!beyond && wp < route.Length && Vector2.Distance(p, route[wp]) < (wp == route.Length - 1 ? 15f : 18f)) { Debug.Log(Tag + "WAKE_WAYPOINT " + wp + " at " + V(new Vector3(p.x, 0, p.y))); if (++wp == route.Length) beyond = true; }
            var aim = beyond ? p + fwd * 50f : route[wp];   // on past the target, straight ahead
            float angle = fwd.sqrMagnitude < .01f ? 0 : -Vector2.SignedAngle(fwd, aim - p), turn = Mathf.Abs(angle);
            var keys = new System.Collections.Generic.List<int>(2);
            double now = Time.realtimeSinceStartupAsDouble;
            if (inWater < 0 && b.speed < 2f) { if (slowSince < 0) slowSince = now; } else slowSince = -1;
            if (slowSince >= 0 && now - slowSince > 3) { reverseUntil = now + 1.5; slowSince = -1; Debug.Log(Tag + "WAKE_UNSTICK " + WakeState(rt)); }
            if (now < reverseUntil) { rt.testHeldKeys = new[] { brake, angle > 0 ? left : right }; yield return null; continue; }
            if (b.speed < (turn > 60 ? 14 : 30)) keys.Add(throttle);
            if (turn > 3f && Mathf.Repeat((float)now, .2f) < .2f * Mathf.Clamp(turn / 30, .15f, 1)) keys.Add(angle > 0 ? right : left);
            rt.testHeldKeys = keys.ToArray();
            float plane = g != null ? g.Water(p.x, p.y) : float.NaN;   // the bike's origin under the lake's plane (also without splats: a core before w-water)
            if (float.IsNaN(plane) && Vector2.Distance(p, target) < 120f) plane = 216f;   // FR_AL_RiverMain's plane (the core's water patch +0xa4)
            if (inWater < 0 && (water.SplatsReceived > splats0 || !float.IsNaN(plane) && b.position[1] < plane + 0.5f)) { inWater = now; Debug.Log(Tag + "WAKE_ENTERED " + WakeState(rt)); }
            if (inWater >= 0 && shots < 8 && now - lastShot >= 0.4)
            {
                lastShot = now; shots++;
                if (shots == 3 || shots == 6) WakeField("wake-" + shots + "-field", water);
                yield return WakeShot("wake-" + shots + "-water", rt); continue;
            }
            if (shots >= 8) break;
            yield return null;
        }
        rt.testHeldKeys = Array.Empty<int>();
        if (inWater < 0) throw new InvalidOperationException("the bike never reached the water: " + WakeState(rt));
        WakeField("wake-9-field", water);
        yield return new WaitForSecondsRealtime(2.5f);
        yield return WakeShot("wake-9-after", rt);
        Debug.Log(Tag + "WAKE_DONE splats " + (water.SplatsReceived - splats0) + " " + WakeState(rt));
        finished = true;
        yield return new WaitForSecondsRealtime(.5f);
        Debug.Log(Tag + "PASS wake");
        Application.Quit(0);
    }

    // w-hover (run.py --hover): after the arrival, the drivable hovercraft: the prompt, boarding, the inflation, driving over land and water, leaving.
    IEnumerator Shot(string name, OriginalWorldRuntime rt)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
        var h = OriginalHovercraft.Instance; var cam = Camera.main;
        Debug.Log(Tag + "HOVER_SHOT " + name + " " + (h != null ? h.Describe() : "no hovercraft") + (cam ? " camera " + V(cam.transform.position) : ""));
    }
    IEnumerator Until(Func<bool> done, float seconds, string what)
    {
        double t0 = Time.realtimeSinceStartupAsDouble;
        while (!done()) { if (Time.realtimeSinceStartupAsDouble - t0 > seconds) throw new TimeoutException("hover: " + what); yield return null; }
    }
    IEnumerator HoverScript(OriginalWorldRuntime rt)
    {
        IEnumerator inner = HoverSteps(rt);
        while (true)
        {
            bool more;
            try { more = inner.MoveNext(); } catch (Exception e) { Fail(e.ToString()); yield break; }
            if (!more) break;
            yield return inner.Current;
        }
    }
    IEnumerator HoverSteps(OriginalWorldRuntime rt)
    {
        yield return new WaitForSecondsRealtime(1f);
        var h = OriginalHovercraft.Instance;
        if (h == null || !h.Active) throw new InvalidOperationException("no hovercraft on this world");
        Debug.Log(Tag + "HOVER_READY " + h.Report);
        // the prompt appears next to the parked craft: the rider's bike stands at the dock beside it
        yield return Until(() => h.PromptVisible, 30, "prompt");
        yield return new WaitForSecondsRealtime(0.6f);
        Debug.Log(Tag + "HOVER_PROMPT text=" + h.PromptText + " glyph=" + h.GlyphText);
        yield return Shot("hover-1-prompt", rt);
        h.testBoard = true;
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Boarding, 5, "boarding");
        yield return new WaitForSecondsRealtime(0.2f);
        yield return Shot("hover-2-fade", rt);
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Inflating, 5, "inflating");
        Debug.Log(Tag + "HOVER_BOARDED " + h.Describe() + " body " + h.Body.Report);
        yield return new WaitForSecondsRealtime(0.25f);
        yield return Shot("hover-3-flat", rt);
        yield return new WaitForSecondsRealtime(1.0f);
        yield return Shot("hover-4-inflating", rt);
        yield return new WaitForSecondsRealtime(0.7f);
        yield return Shot("hover-5-rising", rt);
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Driving, 10, "driving");
        yield return new WaitForSecondsRealtime(1.5f);
        Debug.Log(Tag + "HOVER_INFLATED " + h.Describe());
        if (rt.gameAudio != null && rt.gameAudio.Stats.TryGetValue("scenarios_fr_al/freeride/vehicle", out var vs)) Debug.Log(Tag + "HOVER_BOARD_SOUND starts " + vs.starts + " voices " + vs.voices + " unknown " + vs.unknown + " active events " + rt.gameAudio.ActiveEvents);
        yield return Shot("hover-6-inflated", rt);
        // drive: toward the nearest deep water (the lake the arrival crossed), steering the heading to it
        var g = h.Ground; var p0 = h.Physics.position; Vector2 target = default; bool haveTarget = false;
        for (float r = 600; r <= 2600 && !haveTarget; r += 50)
            for (int a = 0; a < 90; a++)
            {
                float ang = a * 4f * Mathf.Deg2Rad; float x = p0.x + Mathf.Sin(ang) * r, z = p0.z + Mathf.Cos(ang) * r;
                float w = g.Water(x, z);
                if (!float.IsNaN(w) && g.Terrain(x, z) < w - 8f) { target = new Vector2(x, z); haveTarget = true; Debug.Log(Tag + "HOVER_WATER_TARGET deg " + (ang * Mathf.Rad2Deg).ToString("F0") + " at " + r + " ft"); break; }
            }
        float landStart = Time.realtimeSinceStartup; bool shotLand = false, shotWater = false, shotFast = false; float waterSince = -1;
        while (Time.realtimeSinceStartup - landStart < 70)
        {
            float err = 0;
            if (haveTarget) { var d = target - new Vector2(h.Physics.position.x, h.Physics.position.z); err = Mathf.DeltaAngle(h.Physics.yaw * Mathf.Rad2Deg, Mathf.Atan2(d.x, d.y) * Mathf.Rad2Deg); }
            h.forcedControls = new HoverControls { throttle = 1f, brake = 0, steer = Mathf.Clamp(err / 25f, -1f, 1f) };
            float t = Time.realtimeSinceStartup - landStart;
            bool farFromStart = (h.Physics.position - p0).sqrMagnitude > 250f * 250f;
            if (!shotLand && t > 3.5f && !h.Physics.onWater) { shotLand = true; Debug.Log(Tag + "HOVER_LAND " + h.Describe()); yield return Shot("hover-7-land", rt); }
            if (h.Physics.onWater && farFromStart && waterSince < 0) { waterSince = Time.realtimeSinceStartup; Debug.Log(Tag + "HOVER_WATER_ENTERED " + h.Describe()); }
            if (waterSince > 0 && !shotWater && Time.realtimeSinceStartup - waterSince > 2.5f) { shotWater = true; Debug.Log(Tag + "HOVER_WATER " + h.Describe()); yield return Shot("hover-8-water", rt); }
            if (waterSince > 0 && !shotFast && Time.realtimeSinceStartup - waterSince > 7f) { shotFast = true; Debug.Log(Tag + "HOVER_WATER_FAST " + h.Describe()); yield return Shot("hover-9-water-fast", rt); }
            if (shotFast) break;
            yield return null;
        }
        if (!shotWater) Debug.Log(Tag + "HOVER_NO_WATER " + h.Describe());
        for (float t = 0; t < 6f; t += Time.unscaledDeltaTime)   // on across the lake, well away from the dock
        {
            float err = 0;
            if (haveTarget) { var d = target - new Vector2(h.Physics.position.x, h.Physics.position.z); err = Mathf.DeltaAngle(h.Physics.yaw * Mathf.Rad2Deg, Mathf.Atan2(d.x, d.y) * Mathf.Rad2Deg); }
            h.forcedControls = new HoverControls { throttle = 1f, steer = Mathf.Clamp(err / 25f, -1f, 1f) };
            yield return null;
        }
        // a wall: the highest ground within 700 ft, driven at full throttle for 9 s (the craft must stop at a cliff, not climb through it)
        {
            var from = h.Physics.position; float best = 0f; Vector2 wall = default;
            for (int a = 0; a < 120; a++)
                for (float r = 200; r <= 900; r += 25)
                {
                    float ang = a * 3f * Mathf.Deg2Rad, x = from.x + Mathf.Sin(ang) * r, z = from.z + Mathf.Cos(ang) * r;
                    float th = g.Terrain(x, z); if (th < from.y + 12f) continue;
                    float steep = g.Slope(x, z, 4f).magnitude; if (steep > best) { best = steep; wall = new Vector2(x, z); }
                }
            Debug.Log(Tag + "HOVER_WALL_TARGET " + wall + " slope " + best.ToString("F2") + " ground " + g.Terrain(wall.x, wall.y).ToString("F1") + " craft at " + from.y.ToString("F1"));
            // put the craft 180 ft short of the slope, heading for it at speed (a test-only teleport of the craft's own state)
            { var dir = (new Vector2(from.x, from.z) - wall).normalized; var start = wall + dir * 180f; float sy = g.Height(start.x, start.y, out _);
              h.Physics.position = new Vector3(start.x, sy + h.Physics.hoverHeight, start.y); h.Physics.yaw = Mathf.Atan2(-dir.x, -dir.y); h.Physics.velocity = new Vector3(-dir.x, 0, -dir.y) * 60f; h.Physics.up = Vector3.up; }
            int hits0 = h.Physics.wallHits; float minY = float.MaxValue, maxSpeed = 0, endGap = 0; float t0 = Time.realtimeSinceStartup;
            while (Time.realtimeSinceStartup - t0 < 8f)
            {
                var d = wall - new Vector2(h.Physics.position.x, h.Physics.position.z);
                float err = Mathf.DeltaAngle(h.Physics.yaw * Mathf.Rad2Deg, Mathf.Atan2(d.x, d.y) * Mathf.Rad2Deg);
                h.forcedControls = new HoverControls { throttle = 1f, steer = Mathf.Clamp(err / 25f, -1f, 1f) };
                maxSpeed = Mathf.Max(maxSpeed, h.Physics.Speed * 0.6818f);
                float under = h.Physics.position.y - g.Height(h.Physics.position.x, h.Physics.position.z, out _);
                minY = Mathf.Min(minY, under);
                yield return null;
            }
            Debug.Log(Tag + "HOVER_WALL hits " + (h.Physics.wallHits - hits0) + " hardest " + h.Physics.maxImpact.ToString("F0") + " ft/s, lowest origin above ground " + minY.ToString("F1") + " ft, top speed " + maxSpeed.ToString("F0") + " mph, now " + h.Describe());
            yield return Shot("hover-9b-wall", rt);
        }
        // a hard turn and the brakes
        for (float t = 0; t < 2.5f; t += Time.unscaledDeltaTime) { h.forcedControls = new HoverControls { throttle = 1f, steer = 1f }; yield return null; }
        yield return Shot("hover-10-turn", rt);
        for (float t = 0; t < 3.5f; t += Time.unscaledDeltaTime) { h.forcedControls = new HoverControls { brake = 1f }; yield return null; }
        h.forcedControls = null;
        Debug.Log(Tag + "HOVER_BRAKED " + h.Describe());
        h.testLeave = true;
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Leaving, 5, "leaving");
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Parked, 8, "left");
        yield return new WaitForSecondsRealtime(1.2f);
        Debug.Log(Tag + "HOVER_LEFT " + h.Describe());
        yield return Shot("hover-11-left", rt);
        // the bike is at the dock beside the level's copy: boarding there brings the drivable craft back to it
        yield return Until(() => h.PromptVisible, 10, "prompt at the dock");
        h.testBoard = true;
        yield return Until(() => h.mode == OriginalHovercraft.Mode.Inflating, 8, "boarded again");
        yield return new WaitForSecondsRealtime(1.2f);
        Debug.Log(Tag + "HOVER_SUMMONED " + h.Describe());
        yield return Shot("hover-12-summoned", rt);
        finished = true;
        yield return new WaitForSecondsRealtime(.5f);
        Debug.Log(Tag + "PASS hover");
        Application.Quit(0);
    }

    IEnumerator Capture(string name, OriginalWorldRuntime rt)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
        var cam = Camera.main;
        var actors = GameObject.Find("Original ambient entities (posed by the running game)")?.transform.Find("ScenarioAnimEntity");
        string drawn = actors == null ? "-" : string.Join(",", actors.GetComponentsInChildren<Renderer>(false).Where(r => r.enabled).Select(r => r.gameObject.name + "@" + V(r.bounds.center)));
        Debug.Log(Tag + "screenshot " + name + (cam ? " camera " + V(cam.transform.position) + " fov " + cam.fieldOfView.ToString("0.0", CultureInfo.InvariantCulture) : "")
                  + " cast drawn " + (rt.podium ? rt.podium.drawn : 0) + " scenario actors drawn [" + drawn + "]");
    }
    IEnumerator Succeed(OriginalWorldRuntime rt)
    {
        yield return Capture("arrival-after", rt);
        yield return new WaitForSecondsRealtime(.5f);
        Debug.Log(Tag + "PASS arrival " + key);
        Application.Quit(0);
    }
    void Fail(string message)
    {
        if (finished) return; finished = true;
        Debug.LogError(Tag + "FAIL " + message); StartCoroutine(StopFailed());
    }
    IEnumerator StopFailed() { if (!string.IsNullOrEmpty(shotDirectory)) { yield return new WaitForEndOfFrame(); ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, "failure.png")); yield return new WaitForSecondsRealtime(.3f); } Application.Quit(1); }
    static string V(Vector3 p) => p.x.ToString("F1", CultureInfo.InvariantCulture) + "," + p.y.ToString("F1", CultureInfo.InvariantCulture) + "," + p.z.ToString("F1", CultureInfo.InvariantCulture);

    // ---------------------------------------------------------------------------------------------- UI helpers (as ActivationFlowProbe)
    static bool Available(VisualElement value)
    {
        if (value == null || value.panel == null || value.worldBound.width <= 0 || value.worldBound.height <= 0) return false;
        for (var p = value; p != null; p = p.parent)
            if (!p.visible || p.resolvedStyle.display == DisplayStyle.None || p.resolvedStyle.visibility == Visibility.Hidden || p.resolvedStyle.opacity <= 0) return false;
        return true;
    }
    static bool Visible(VisualElement value)
    {
        if (!Available(value)) return false;
        var bounds = value.worldBound; var clip = value.panel.visualTree.worldBound;
        if (float.IsNaN(bounds.x) || float.IsNaN(bounds.y) || float.IsInfinity(bounds.x) || float.IsInfinity(bounds.y)) return false;
        for (var p = value.parent; p != null; p = p.parent)
        {
            if (!(p is ScrollView scroll)) continue;
            var limit = scroll.contentViewport.worldBound;
            clip = Rect.MinMaxRect(Mathf.Max(clip.xMin, limit.xMin), Mathf.Max(clip.yMin, limit.yMin), Mathf.Min(clip.xMax, limit.xMax), Mathf.Min(clip.yMax, limit.yMax));
            if (clip.width <= 0 || clip.height <= 0) return false;
        }
        if (bounds.xMin < clip.xMin - 1 || bounds.yMin < clip.yMin - 1 || bounds.xMax > clip.xMax + 1 || bounds.yMax > clip.yMax + 1) return false;
        return !(value.userData is OuNavItem) || HitTestControl(value);
    }
    static bool HitTestPoint(VisualElement value, Vector2 point) { var picked = value.panel.Pick(point); return picked == value || (picked != null && value.Contains(picked)); }
    static bool HitTestControl(VisualElement value)
    {
        var bounds = value.worldBound;
        float inset = Mathf.Min(2f, Mathf.Min(bounds.width, bounds.height) * .25f);
        return HitTestPoint(value, bounds.center)
            && HitTestPoint(value, new Vector2(bounds.xMin + inset, bounds.yMin + inset))
            && HitTestPoint(value, new Vector2(bounds.xMax - inset, bounds.yMin + inset))
            && HitTestPoint(value, new Vector2(bounds.xMin + inset, bounds.yMax - inset))
            && HitTestPoint(value, new Vector2(bounds.xMax - inset, bounds.yMax - inset));
    }
    static bool Clickable(VisualElement value) => Visible(value) && value.enabledInHierarchy && HitTestControl(value);
    static VisualElement Find(string name)
    {
        VisualElement best = null; float order = float.MinValue;
        foreach (var doc in FindObjectsByType<UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            var value = doc.rootVisualElement?.Q<VisualElement>(name);
            if (Available(value) && doc.sortingOrder >= order) { best = value; order = doc.sortingOrder; }
        }
        return best;
    }
    static VisualElement FindCard(string key)
    {
        foreach (var doc in FindObjectsByType<UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            var root = doc.rootVisualElement; if (root == null) continue;
            var card = root.Query<VisualElement>(className: "ou-card").ToList().FirstOrDefault(x => x.tooltip == key && Available(x));
            if (card != null) return card;
        }
        return null;
    }
    bool Drawn(VisualElement value, double now)
    {
        if (!Visible(value))
        {
            Unwatch();
            if (value?.userData is OuNavItem) { value.Focus(); for (var p = value.parent; p != null; p = p.parent) if (p is ScrollView scroll) { scroll.ScrollTo(value); break; } }
            return false;
        }
        if (candidate != value)
        {
            Unwatch(); candidate = value; candidateSince = now; candidateFrame = Time.frameCount; paintedFrame = -1;
            draw = _ => { if (paintedFrame < 0) paintedFrame = Time.frameCount; }; candidate.generateVisualContent += draw; candidate.MarkDirtyRepaint(); return false;
        }
        return Visible(value) && paintedFrame >= candidateFrame && Time.frameCount > paintedFrame && now - candidateSince >= 1;
    }
    bool Submit(VisualElement value, double now)
    {
        if (!Drawn(value, now) || !Clickable(value) || now - lastSubmit < 2) return false;
        if (!(value.userData is OuNavItem nav) || nav.submit == null) throw new InvalidOperationException("Actual control has no OuNavItem submit: " + value.name);
        bool delivered = false;
        EventCallback<ClickEvent> observe = evt => { if (evt.button == 0 && evt.currentTarget == value) delivered = true; };
        value.RegisterCallback(observe, TrickleDown.TrickleDown);
        try
        {
            var trigger = new UnityEngine.Event { type = EventType.MouseUp, button = 0, clickCount = 1, mousePosition = value.worldBound.center };
            using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = value; value.SendEvent(click); }
        }
        finally { value.UnregisterCallback(observe, TrickleDown.TrickleDown); }
        if (!delivered) throw new InvalidOperationException("Left-click was not delivered to the actual control: " + value.name);
        lastSubmit = now; Debug.Log(Tag + "TEST USER left-click delivered " + (string.IsNullOrEmpty(value.name) ? value.tooltip : value.name));
        Unwatch(); return true;
    }
    void Unwatch() { if (candidate != null && draw != null) candidate.generateVisualContent -= draw; candidate = null; draw = null; }
}
