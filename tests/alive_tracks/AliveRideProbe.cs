// Rendered real-player ride acceptance for converted MX vs ATV Alive levels (generalized tests/sliderock_canyon/SliderockRideProbe.cs).
// Opt-in: REFLEX_ALIVE_TEST=1, REFLEX_ALIVE_LEVEL=<level>, REFLEX_PREVIEW_SHOTS=<dir>. Drives the actual menu (hub "MX vs ATV Alive" ->
// the level's card -> setup -> start), rides with real mapped keys for 480 game frames (throttle, then brake), counts resets, then takes
// terrain-clamped inspection views (spawn surroundings, sky toward / away from the sun, nearest tree and model, a vista and a top view).
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;
[DefaultExecutionOrder(32000)]
public sealed class AliveRideProbe : MonoBehaviour
{
    string shots, levelName; int errors; Camera inspectionCamera; Vector3 inspectPosition, inspectTarget;
    void LateUpdate() { if (inspectionCamera) { inspectionCamera.transform.position = inspectPosition; inspectionCamera.transform.LookAt(inspectTarget); } }
    static bool On => Environment.GetEnvironmentVariable("REFLEX_ALIVE_TEST") == "1";
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Guest() { if (On) ReflexProfile.Disabled = true; }
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        if (!On || FindAnyObjectByType<AliveRideProbe>()) return;
        var go = new GameObject("Alive ride acceptance"); DontDestroyOnLoad(go); go.AddComponent<AliveRideProbe>(); Application.runInBackground = true;
        // A player window that is never activated is often fully occluded (another app or space in front): the window server then
        // returns its drawables slowly and every presented frame waits (~1 fps). Offscreen mode presents only one frame in 30; the game
        // loop still runs every frame and the screenshots render the cameras into their own target.
        // Time.captureFramerate: game time advances 1/60 s per frame whatever the wall clock (the menu world waits for 10 frames under
        // 50 ms before its intro; an occluded window's slow frames would never qualify), as OpenMenuWorldProbe's recorder does.
        Time.captureFramerate = 60;
        if (Offscreen) UnityEngine.Rendering.OnDemandRendering.renderFrameInterval = 30;
    }
    float nextRate; int rateFrames;
    void Update()
    {
        if (Time.realtimeSinceStartup < nextRate) return;
        if (nextRate > 0) Say($"RATE {(Time.frameCount - rateFrames) / 10f:F1} frames/s (renderFrameInterval {UnityEngine.Rendering.OnDemandRendering.renderFrameInterval}, capture {Time.captureFramerate}) shell={OpenShell.Instance?.Current} intro={OpenShell.Instance?.IntroState} world={OpenMenuWorld.Instance?.Status} settled={OpenMenuWorld.Instance?.SceneSettled} time={Time.time:F0}");
        nextRate = Time.realtimeSinceStartup + 10f; rateFrames = Time.frameCount;
    }
    void OnEnable() { Application.logMessageReceived += Log; }
    void OnDisable() { Application.logMessageReceived -= Log; }
    void Log(string s, string trace, LogType type) { if (type == LogType.Exception) errors++; }
    static void Say(string s) => Debug.Log("[alive-test] " + s);
    VisualElement Find(string name) => FindObjectsByType<UIDocument>(FindObjectsSortMode.None).Select(d => d.rootVisualElement?.Q<VisualElement>(name)).FirstOrDefault(v => v != null && v.panel != null);
    // Screenshots: the window's back buffer, or (batch mode / REFLEX_ALIVE_OFFSCREEN=1: a player that is never presented, e.g. while
    // another full-screen app owns the display) the enabled cameras rendered by depth into an offscreen target (no UI overlay).
    static bool Offscreen => Application.isBatchMode || Environment.GetEnvironmentVariable("REFLEX_ALIVE_OFFSCREEN") == "1";
    static readonly System.Reflection.FieldInfo OnboardField = typeof(OriginalWorldRuntime).GetField("onboardState", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
    IEnumerator Shot(string name)
    {
        yield return new WaitForEndOfFrame();
        string path = Path.Combine(shots, name + ".png");
        if (!Offscreen) { ScreenCapture.CaptureScreenshot(path); yield return new WaitForSecondsRealtime(1); yield break; }
        var rt = RenderTexture.GetTemporary(1280, 720, 24, RenderTextureFormat.ARGB32);
        foreach (var cam in FindObjectsByType<Camera>(FindObjectsSortMode.None).Where(c => c.isActiveAndEnabled).OrderBy(c => c.depth))
        {
            var prior = cam.targetTexture; cam.targetTexture = rt; cam.Render(); cam.targetTexture = prior;
        }
        var prev = RenderTexture.active; RenderTexture.active = rt;
        var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply();
        RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
        if (Environment.GetEnvironmentVariable("REFLEX_SHOT_FLIP") != "0")   // the target-texture render of the current pipeline comes out upside down
        {
            var px = tex.GetPixels32(); var flipped = new Color32[px.Length];
            for (int y = 0; y < 720; y++) Array.Copy(px, y * 1280, flipped, (719 - y) * 1280, 1280);
            tex.SetPixels32(flipped); tex.Apply();
        }
        File.WriteAllBytes(path, tex.EncodeToPNG()); Destroy(tex);
        yield return new WaitForSecondsRealtime(1);
    }
    IEnumerator Click(string name)
    {
        float t = Time.realtimeSinceStartup; VisualElement v;
        while ((v = Find(name)) == null || !v.enabledInHierarchy || v.worldBound.width < 1)
        {
            if (Time.realtimeSinceStartup - t > 120) { Fail("Missing control " + name); yield break; }
            yield return null;
        }
        yield return new WaitForSecondsRealtime(2);
        while ((v = Find(name)) == null || v.panel == null || !v.enabledInHierarchy || v.worldBound.width < 1)
        {
            if (Time.realtimeSinceStartup - t > 120) { Fail("Missing control " + name); yield break; }
            yield return null;
        }
        var picked = v.panel.Pick(v.worldBound.center);
        if (picked != v && !v.Contains(picked)) { Fail("Obscured control " + name); yield break; }
        var trigger = new Event { type = EventType.MouseUp, button = 0, clickCount = 1, mousePosition = v.worldBound.center };
        using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = v; v.SendEvent(click); }
        Say("CLICK " + name); yield return new WaitForSecondsRealtime(2);
    }
    IEnumerator Start()
    {
        shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); Directory.CreateDirectory(shots);
        levelName = Environment.GetEnvironmentVariable("REFLEX_ALIVE_LEVEL");
        yield return new WaitForSecondsRealtime(10);
        float t0 = Time.realtimeSinceStartup;
        while (Find("ou-hub-play") == null) { if (Time.realtimeSinceStartup - t0 > 900) { Fail("No PLAY hub entry"); yield break; } yield return null; }
        yield return new WaitForSecondsRealtime(3);
        Say("REGISTRY " + string.Join(", ", AliveLevels.All.Select(e => e.level + "=" + e.DisplayName)));
        yield return Shot("main-menu"); yield return Click("ou-hub-play"); yield return Click("ou-hub-freeride");   // (the Alive tracks: FREE RIDE's MX vs ATV Alive category)
        var shell = OpenShell.Instance; shell.SelectBrowse("*alive"); yield return new WaitForSecondsRealtime(1);
        int expected = AliveLevels.All.Count(e => ReflexInstall.Packages.Has(e.level));
        if (shell.Browse != "*alive" || shell.VisibleCards != expected) { Fail("Alive category lists " + shell.VisibleCards + " of " + expected + " levels"); yield break; }
        if (!shell.SelectEvent("FreeRide/" + levelName)) { Fail("No card for " + levelName); yield break; }
        yield return new WaitForSecondsRealtime(2);
        if (shell.SelectedEvent?.Location != levelName) { Fail("Selection mismatch"); yield break; }
        Say("CARD " + shell.SelectedEvent.locationName);
        yield return Shot("alive-category"); yield return Click("ou-setup-next"); yield return Shot("setup"); yield return Click("ou-setup-start");
        float t = Time.realtimeSinceStartup; OriginalWorldRuntime rt = null;
        while (!(rt && rt.Running))
        {
            rt = FindAnyObjectByType<OriginalWorldRuntime>();
            if (rt && !string.IsNullOrEmpty(rt.FailureMessage)) { Fail(rt.FailureMessage); yield break; }
            if (Time.realtimeSinceStartup - t > 400) { Fail("Ride load timed out"); yield break; }
            yield return null;
        }
        while (rt.Bike.frames < 200) { if (Time.realtimeSinceStartup - t > 400) { Fail("Countdown stalled"); yield break; } yield return null; }
        yield return Shot("spawn");
        var level = FindObjectsByType<OriginalLevel>(FindObjectsSortMode.None).FirstOrDefault(x => x.level == levelName);
        if (level == null) { Fail("No OriginalLevel for " + levelName); yield break; }
        Say("ENV forest=" + level.Content.forest?.treeCount + " grass=" + level.Content.grass?.cells + " water=" + ReflexWater.Load(level.Content).Describe());
        Vector3 start = new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]);
        float maxDistance = 0, maxSpeed = 0, priorReset = rt.Race.reset_timer; int resets = 0, wreckResets = 0, lastWreck = -1, logged = -1; float[] lastPos = { 0, 0, 0 }; uint driveStart = rt.Bike.frames; t = Time.realtimeSinceStartup; bool throttling = true;
        while (rt.Bike.frames - driveStart < 480)
        {
            if (!rt.Running) { Fail(rt.FailureMessage); yield break; }
            if (Time.realtimeSinceStartup - t > 300) { Fail("Ride frame progression stalled"); yield break; }
            if (throttling && (rt.Bike.frames - driveStart >= 120 || rt.Bike.speed >= 58f)) throttling = false;
            rt.testHeldKeys = throttling ? new[] { 0x11 } : new[] { 0x1f }; var b = rt.Bike;
            int sec = (int)(Time.realtimeSinceStartup - t); if (sec != logged) { logged = sec; Say("DRIVE t=" + sec + " frames=" + b.frames + " speed=" + b.speed + " pos=" + string.Join(",", b.position)); }
            var onboard = (Reflex.Port.RxCore.RxOnboardState)OnboardField.GetValue(rt);
            if (onboard.valid != 0 && onboard.wrecked != 0) lastWreck = (int)(b.frames - driveStart);
            float reset = rt.Race.reset_timer;
            if (reset > priorReset + 0.1f)
            {
                // a wreck (the rider thrown by a collision with authored scenery: spectators, stadium walls) is a legitimate respawn;
                // any other reset (terrain zone, bounds) without a wreck in the last 2 s fails the ride
                int at = (int)(b.frames - driveStart);
                if (lastWreck >= 0 && at - lastWreck <= 120) { wreckResets++; Say("RESET at frame " + at + " (wreck respawn, wrecked at " + lastWreck + ")"); }
                else { resets++; Say("RESET at frame " + at + " pos=" + lastPos[0] + "," + lastPos[1] + "," + lastPos[2]); }
            }
            priorReset = reset; lastPos = new[] { b.position[0], b.position[1], b.position[2] };
            if (b.position.Any(x => float.IsNaN(x) || float.IsInfinity(x))) { Fail("Nonfinite bike pose"); yield break; }
            maxDistance = Mathf.Max(maxDistance, Vector3.Distance(start, new Vector3(b.position[0], b.position[1], b.position[2]))); maxSpeed = Mathf.Max(maxSpeed, b.speed);
            if (rt.Bike.frames - driveStart > 180 && !File.Exists(Path.Combine(shots, "ride.png"))) yield return Shot("ride");
            yield return null;
        }
        rt.testHeldKeys = Array.Empty<int>(); yield return Shot("ride-end");
        // resets other than wreck respawns are classified by run.py (the authored reset material under the logged position)
        if (errors > 0 || maxDistance < 20 || maxSpeed < 3) { Fail("Ride evidence: exceptions=" + errors + " resets=" + resets + " distance=" + maxDistance + " speed=" + maxSpeed); yield break; }
        inspectionCamera = rt.chaseCamera;
        Vector3 bike = new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]);
        Vector3 sun = Shader.GetGlobalVector("_RfxSunDir"); sun.y = 0; sun = sun.sqrMagnitude > 1e-6f ? sun.normalized : Vector3.forward;
        Vector3 side = Vector3.Cross(Vector3.up, sun);
        yield return View("around-sun", bike + new Vector3(0, 6, 0) - sun * 12, bike + sun * 80);
        yield return View("around-away", bike + new Vector3(0, 6, 0) + sun * 12, bike - sun * 80);
        yield return View("around-left", bike + new Vector3(0, 6, 0) - side * 12, bike + side * 80);
        yield return View("around-right", bike + new Vector3(0, 6, 0) + side * 12, bike - side * 80);
        yield return View("ground", bike + new Vector3(0, 4, 0) - sun * 8, bike + new Vector3(0, 0, 0) + sun * 14);
        yield return View("sky-sun", bike + new Vector3(0, 8, 0), bike + new Vector3(0, 40, 0) + sun * 100);
        yield return View("sky-away", bike + new Vector3(0, 8, 0), bike + new Vector3(0, 30, 0) - sun * 100);
        var trees = level.Content.forest?.trees;
        if (trees != null && trees.Length >= 8)
        {
            int nearest = 0; float best = float.MaxValue;
            for (int i = 0; i < trees.Length; i += 8) { float d = Vector3.Distance(bike, new Vector3(trees[i], trees[i + 1], trees[i + 2])); if (d < best) { best = d; nearest = i; } }
            Vector3 tree = new Vector3(trees[nearest], trees[nearest + 1], trees[nearest + 2]);
            yield return View("vegetation", tree + new Vector3(0, 13, -35), tree + new Vector3(0, 5, 0));
            yield return View("vegetation-close", tree + new Vector3(-6, 7, -16), tree + new Vector3(0, 6, 0));
        }
        var lods = FindAnyObjectByType<OriginalSceneryLod>();
        var model = lods?.Instances?.Where(x => x.model != null && x.renderer && x.renderer.bounds.extents.magnitude > 4f)
            .OrderBy(x => Vector3.Distance(x.renderer.bounds.center, bike)).FirstOrDefault();
        if (model != null)
        {
            Vector3 c = model.renderer.bounds.center, toward = (bike - c); toward.y = 0; toward = toward.sqrMagnitude > 1e-4f ? toward.normalized : Vector3.forward;
            Say("MODEL nearest " + model.model.name);
            yield return View("model", c + toward * (model.renderer.bounds.extents.magnitude * 0.9f + 20f) + Vector3.up * 8f, c);
        }
        yield return CrowdViews(bike);
        yield return MapShadowView(bike, sun);
        var parts = FindAnyObjectByType<OriginalParticles>();
        Say("PARTICLES live=" + (parts ? parts.Live : -1) + " normal=" + (parts ? parts.Normal : -1) + " additive=" + (parts ? parts.Additive : -1));
        yield return View("vista", bike + new Vector3(0, 45, 0) - sun * 20, bike + new Vector3(0, 20, 0) - sun * 400);
        yield return View("vista-sun", bike + new Vector3(0, 45, 0) + sun * 20, bike + new Vector3(0, 20, 0) + sun * 400);
        yield return View("top", bike + new Vector3(1, 600, 0), bike);
        inspectionCamera = null;
        Say("PASS actual menu + FreeRide " + levelName + "; resets=" + resets + " wreck-respawns=" + wreckResets + " frames=" + (rt.Bike.frames - driveStart) + "; distance=" + maxDistance + " speed=" + maxSpeed);
        Application.Quit(0);
    }
    // --- lighting gap probes (w-alivelight): the crowds and the objects under the level's MapShadow
    static bool IsCrowdish(Renderer r) => r.name.IndexOf("rowd", StringComparison.OrdinalIgnoreCase) >= 0 || (r.transform.parent && r.transform.parent.name.IndexOf("rowd", StringComparison.OrdinalIgnoreCase) >= 0)
        || r.sharedMaterials.Any(m => m && m.name.IndexOf("rowd", StringComparison.OrdinalIgnoreCase) >= 0);
    IEnumerator CrowdViews(Vector3 bike)
    {
        var rs = FindObjectsByType<Renderer>(FindObjectsSortMode.None).Where(r => r.enabled && r.gameObject.activeInHierarchy && r.sharedMaterials.Any(m => m && m.shader)).ToArray();
        var stands = rs.Where(r => r.sharedMaterials.Any(m => m && m.shader.name == "Reflex/Original Crowd Billboard")).ToArray();
        var walkers = rs.Where(r => !stands.Contains(r) && r.sharedMaterials.Any(m => m && m.shader.name == "Reflex/Vehicle") && IsCrowdish(r)).ToArray();
        Say("CROWD stands=" + stands.Length + " walkers=" + walkers.Length + " (" + string.Join(",", walkers.Take(6).Select(r => r.name + ":" + r.sharedMaterials[0]?.name)) + ")");
        yield return CrowdView("crowd-stands", stands, bike, 26f, 6f);
        yield return CrowdView("crowd-walkers", walkers, bike, 12f, 3f);
    }
    IEnumerator CrowdView(string name, Renderer[] set, Vector3 bike, float distance, float height)
    {
        if (set.Length == 0 || (ViewFilter.Length > 0 && Array.IndexOf(ViewFilter, name) < 0)) yield break;
        var best = set.OrderBy(r => (r.bounds.ClosestPoint(bike) - bike).sqrMagnitude).First();
        Vector3 p = best.bounds.ClosestPoint(bike), d = bike - p; d.y = 0; d = d.sqrMagnitude > 1 ? d.normalized : Vector3.forward;
        Say("VIEW " + name + " target=" + best.name + " at " + p + " dist=" + Vector3.Distance(p, bike));
        yield return View(name, p + d * distance + Vector3.up * height, p + Vector3.up * (height * 0.5f));
    }
    IEnumerator MapShadowView(Vector3 bike, Vector3 sun)
    {
        if (ViewFilter.Length > 0 && Array.IndexOf(ViewFilter, "mapshadow") < 0) yield break;
        var par = Shader.GetGlobalVector("_RfxMapShadowParams"); var map = Shader.GetGlobalTexture("_RfxMapShadowMap");
        if (par.x < 0.5f || !map) { Say("MAPSHADOW none"); yield break; }
        var rt = RenderTexture.GetTemporary(512, 512, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear); Graphics.Blit(map, rt);
        var prev = RenderTexture.active; RenderTexture.active = rt;
        var tex = new Texture2D(512, 512, TextureFormat.RGBA32, false, true); tex.ReadPixels(new Rect(0, 0, 512, 512), 0, 0); tex.Apply();
        RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
        float K = par.y; Func<float, float, float> val = (x, z) => tex.GetPixelBilinear(0.5f + x * K, 0.5f + z * K).r;
        var hist = new int[5]; foreach (var c in tex.GetPixels32()) hist[Mathf.Min(4, c.r / 52)]++;
        Say("MAPSHADOW histogram (0..0.2, .2..0.4, .4..0.6, .6..0.8, .8..1) " + string.Join(" ", hist) + " extent=" + (1f / K).ToString("F0") + " bike value=" + val(bike.x, bike.z).ToString("F2"));
        // the pair of points (within 18 ft of each other, within 500 ft of the bike) with the largest difference of the map: the dark one and the lit one
        Vector3 dark = default, lit = default; float bestScore = 0; bool found = false;
        var ascii = new System.Text.StringBuilder("MAPSHADOW around the bike (10 ft cells, x right, z up; 0..9 = the map value x 9.99)\n");
        for (float z = 200; z >= -200; z -= 10) { for (float x = -200; x <= 200; x += 10) ascii.Append((char)('0' + Mathf.Clamp((int)(val(bike.x + x, bike.z + z) * 9.99f), 0, 9))); ascii.Append('\n'); }
        Say(ascii.ToString());
        for (float x = -250; x <= 250; x += 5) for (float z = -250; z <= 250; z += 5)
        {
            float px = bike.x + x, pz = bike.z + z, v0 = val(px, pz); if (v0 < 0.6f) continue;
            for (float dx = -16; dx <= 16; dx += 4f) for (float dz = -16; dz <= 16; dz += 4f)
            {
                float diff = v0 - val(px + dx, pz + dz) - (x * x + z * z) * 5e-6f;
                if (diff > bestScore) { bestScore = diff; lit = new Vector3(px, 0, pz); dark = new Vector3(px + dx, 0, pz + dz); found = true; }
            }
        }
        if (!found || bestScore < 0.3f) { Say("MAPSHADOW no dark / lit pair near the bike (best " + bestScore.ToString("F2") + ")"); Destroy(tex); yield break; }
        var terrain = Terrain.activeTerrain; Func<Vector3, Vector3> ground = v => { v.y = terrain ? terrain.SampleHeight(v) + terrain.transform.position.y : bike.y; return v; };
        dark = ground(dark); lit = ground(lit);
        Say("MAPSHADOW dark " + dark + " map=" + val(dark.x, dark.z).ToString("F2") + " lit " + lit + " map=" + val(lit.x, lit.z).ToString("F2"));
        var made = new GameObject[2]; var at = new[] { dark, lit };
        for (int i = 0; i < 2; i++)
        {
            var g = GameObject.CreatePrimitive(PrimitiveType.Sphere); Destroy(g.GetComponent<Collider>()); g.name = i == 0 ? "map-dark" : "map-lit";
            g.transform.position = at[i] + Vector3.up * 3f; g.transform.localScale = Vector3.one * 5f;
            g.GetComponent<Renderer>().sharedMaterial = ReflexObjectShading.Fallback(Color.gray, "mapshadow test"); made[i] = g;
        }
        Vector3 mid = (dark + lit) * 0.5f + Vector3.up * 3f, towards = (lit - dark); towards.y = 0; Vector3 side = Vector3.Cross(Vector3.up, towards).normalized;
        if (Vector3.Dot(side, sun) < 0) side = -side;
        yield return View("mapshadow", mid + side * 20f + Vector3.up * 6f, mid);
        foreach (var g in made) Destroy(g);
        Destroy(tex);
    }
    // REFLEX_ALIVE_VIEWS=ride,ground,vista limits the inspection views (comparison renders); unset: all of them
    static readonly string[] ViewFilter = (Environment.GetEnvironmentVariable("REFLEX_ALIVE_VIEWS") ?? "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
    IEnumerator View(string name, Vector3 position, Vector3 target)
    {
        if (ViewFilter.Length > 0 && Array.IndexOf(ViewFilter, name) < 0) yield break;
        var terrain = Terrain.activeTerrain;
        if (terrain) { float g = terrain.SampleHeight(new Vector3(position.x, 0, position.z)) + terrain.transform.position.y; if (position.y < g + 4f) position.y = g + 4f; }
        inspectPosition = position; inspectTarget = target;
        int f0 = Time.frameCount; float t0 = Time.realtimeSinceStartup;
        while (Time.frameCount - f0 < 180 && Time.realtimeSinceStartup - t0 < 20) yield return null;
        yield return LogAdaptation(name);
        yield return Shot(name);
    }
    IEnumerator LogAdaptation(string name)
    {
        var pipe = FindObjectsByType<Reflex.Port.Rendering.ReflexRenderPipeline>(FindObjectsSortMode.None).FirstOrDefault(x => x.isActiveAndEnabled && x.AdaptedLuminance != null);
        var rt = pipe?.AdaptedLuminance?.rt; if (!rt) yield break;
        var req = UnityEngine.Rendering.AsyncGPUReadback.Request(rt); float t0 = Time.realtimeSinceStartup;
        while (!req.done && Time.realtimeSinceStartup - t0 < 5) yield return null;
        if (req.done && !req.hasError) Say("ADAPT " + name + " luminance=" + req.GetData<float>()[0].ToString("G4") + " key=" + pipe.hdr.keyValue + " min=" + pipe.hdr.minAdaptation + " max=" + pipe.hdr.maxAdaptation);
    }
    void Fail(string reason) { Debug.LogError("[alive-test] FAIL " + reason); Application.Quit(1); }
}
