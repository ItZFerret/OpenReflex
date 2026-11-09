// TEST ONLY (run.py --crowd "<Activity>|<Activity>/<Level>", w-npcs): the stands crowd (and the other spectators) of a stadium / National track
// in the open product. The open front end's event browser filters the activity, selects the level's card and starts it like a player does;
// then it only watches (holding the throttle once the race runs): while the race intro plays (OriginalPodium.scenario 3) it captures
// "crowd-intro-<n>.png" every 1.5 s, at the countdown and in the first seconds of the race "crowd-race-<n>.png", and logs what Unity draws
// of every ambient group (OriginalAmbient: the renderers enabled per entity class, the billboard crowd sections included, and the camera
// flashes of this frame: active records / quads drawn: w-screens). PASS when the
// race has run for 12 s. It writes no game state.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

[DefaultExecutionOrder(32000)]
public sealed class CrowdShotProbe : MonoBehaviour
{
    const string Tag = "[crowd-shot-probe] ";
    string activity, key, shotDirectory;
    double started, browserOpenedAt, lastSubmit, candidateSince, lastShot = -10, raceStarted = -1;
    int candidateFrame, paintedFrame = -1, introShots, raceShots, exceptions, throttle = -1;
    bool opened, filtered, selected, launched, introSeen, finished;
    bool platesMode, platesStarted, platesDone;   // run.py --plates (w-plates): the player's jersey name / number and the AI riders' on the gate, seen close up
    VisualElement candidate; Action<MeshGenerationContext> draw;

    public static void Boot()
    {
        string spec = Environment.GetEnvironmentVariable("REFLEX_CROWD_SHOT_PROBE");
        if (string.IsNullOrEmpty(spec) || FindAnyObjectByType<CrowdShotProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Crowd shots TEST ONLY"); DontDestroyOnLoad(go);
        var p = go.AddComponent<CrowdShotProbe>(); var parts = spec.Split('|'); p.activity = parts[0]; p.key = parts[1]; Application.runInBackground = true;
    }

    void Start()
    {
        started = Time.realtimeSinceStartupAsDouble;
        Application.logMessageReceived += (m, s, t) => { if (t == LogType.Exception) exceptions++; };
        shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrWhiteSpace(shotDirectory)) { Fail("no screenshot directory"); return; }
        Directory.CreateDirectory(shotDirectory);
        platesMode = Environment.GetEnvironmentVariable("REFLEX_PLATES_SHOTS") == "1";
        Debug.Log(Tag + "BEGIN " + key + (platesMode ? " (plates)" : ""));
    }

    void Update()
    {
        if (finished) return;
        try { Tick(); } catch (Exception e) { Fail(e.ToString()); }
    }

    static int Scan(RemapTable t, string name)
    {
        if (t == null) throw new InvalidOperationException("No copied controls.");
        int col = Array.IndexOf(t.columns, name);
        if (col < 0 || col >= t.kb.Length) throw new InvalidOperationException("Missing keyboard action " + name);
        var matches = Enumerable.Range(1, 255).Where(i => t.scan[i] == t.kb[col]).ToArray();
        if (t.kb[col] == 0 || matches.Length != 1) throw new InvalidOperationException("Nonunique scan code for " + name);
        return matches[0];
    }

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 900) throw new TimeoutException("Crowd shot wall budget exceeded.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s): " + exceptions);
        if (OpenShell.Instance == null && !launched) return;
        if (!opened)
        {
            if (now - started < 2) return;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (platesMode)
            {   // the garage RIDER tab's JERSEY NAME and NUMBER rows write exactly this (CoreGarageProfile.SetRiderText); the race's next session reads it
                Debug.Log(Tag + "SetRiderText Name FERRET " + CoreGarageProfile.SetRiderText("Name", "FERRET") + ", Number 998 " + CoreGarageProfile.SetRiderText("Number", "998") + " " + CoreGarageProfile.Error);
                ReflexProfile.Open();
            }
            OpenShell.Instance.ShowBrowser(false);
            opened = true; browserOpenedAt = now; Debug.Log(Tag + "BROWSER_OPENED " + key);
            return;
        }
        if (!filtered)
        {
            var filter = Find("ou-filter-" + activity.ToLowerInvariant());
            if (filter == null) { if (now - browserOpenedAt > 120) throw new TimeoutException("The event browser has no " + activity + " filter."); return; }
            if (Submit(filter, now)) { filtered = true; Debug.Log(Tag + "USER filter " + activity); }
            return;
        }
        if (!selected)
        {
            var card = FindCard(key);
            if (card == null) { if (now - browserOpenedAt > 180) throw new TimeoutException("The event browser has no " + key + " card."); return; }
            if (Submit(card, now)) { selected = true; Debug.Log(Tag + "USER selected " + key); }
            return;
        }
        if (!launched)
        {
            var start = Find("ou-setup-start");
            if (start == null) return;
            if (Submit(start, now)) { launched = true; Debug.Log(Tag + "USER START EVENT " + key); }
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
            if (now - lastShot >= 1.5 && introShots < 12) { lastShot = now; introShots++; StartCoroutine(Capture("crowd-intro-" + introShots, rt)); }
            return;
        }
        if (!introSeen && now - browserOpenedAt > 400) throw new TimeoutException("No intro scenario (scenario 3) reported for " + key);
        if (!introSeen) return;
        var race = rt.Race;
        if (platesMode && !platesStarted && race.valid != 0 && (race.state == 1 || race.state == 2)) { platesStarted = true; StartCoroutine(PlatesShots(rt)); }
        if (race.valid != 0 && race.state == 2)
        {
            if (throttle < 0) throttle = Scan(CoreRemapping.Read(false), "Throttle");
            rt.testHeldKeys = new[] { throttle };
            if (raceStarted < 0) { raceStarted = now; Debug.Log(Tag + "RACING " + key); }
            if (now - lastShot >= 3 && raceShots < 4) { lastShot = now; raceShots++; StartCoroutine(Capture("crowd-race-" + raceShots, rt)); }
            if (now - raceStarted > 12 && raceShots >= 3 && (!platesMode || platesDone)) { finished = true; StartCoroutine(Succeed(rt)); }
        }
        else if (race.valid != 0 && race.state == 1 && now - lastShot >= 2 && raceShots < 1) { lastShot = now; StartCoroutine(Capture("crowd-countdown", rt)); }
    }

    IEnumerator Capture(string name, OriginalWorldRuntime rt)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
        var cam = Camera.main;
        var root = GameObject.Find("Original ambient entities (posed by the running game)");
        string groups = "-";
        if (root != null)
            groups = string.Join(" ", Enumerable.Range(0, root.transform.childCount).Select(i => root.transform.GetChild(i)).Select(g => g.name + "=" + g.GetComponentsInChildren<Renderer>(false).Count(r => r.enabled) + "/" + g.childCount));
        var amb = FindAnyObjectByType<OriginalAmbient>();
        string flashes = amb ? " camera flashes active " + amb.flashesActive + " drawn " + amb.flashesDrawn : "";
        Debug.Log(Tag + "screenshot " + name + (cam ? " camera " + V(cam.transform.position) + " fov " + cam.fieldOfView.ToString("0.0", CultureInfo.InvariantCulture) : "") + " ambient drawn/total [" + groups + "]" + flashes);
    }
    // The level's in-level video screens up close (w-screens): every renderer that shows a video target (Reflex/Original Video Screen, or a
    // material that samples a "(video ...)" render target): the camera is put in front of up to four distinct ones for one frame
    // (CrowdShotProbe.LateUpdate, after the game's camera) and "screen-closeup-<n>.png" is captured; the log lists what each shows.
    IEnumerator Closeups(OriginalWorldRuntime rt)
    {
        var cam = Camera.main; if (!cam) yield break;
        var found = new List<(string label, Bounds bounds)>(); var seen = new HashSet<string>();
        foreach (var r in FindObjectsByType<Renderer>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            if (!r.enabled || !r.gameObject.activeInHierarchy) continue;
            foreach (var m in r.sharedMaterials)
            {
                if (!m || !m.shader) continue;
                string video = null;
                foreach (var n in m.GetTexturePropertyNames()) if (m.GetTexture(n) is RenderTexture t && t.name.Contains("(video ")) { video = t.name; break; }
                if (video == null && m.shader.name != "Reflex/Original Video Screen") continue;
                string key = m.shader.name + "|" + m.name + "|" + video;
                if (!seen.Add(key)) continue;
                found.Add((m.name + " [" + m.shader.name + "] " + video, r.bounds));
            }
        }
        Debug.Log(Tag + "video screens in the scene: " + found.Count + " distinct (" + string.Join("; ", found.Select(f => f.label)) + ")");
        int shot = 0;
        foreach (var f in found.Take(4))
        {
            var eye = cam.transform.position; var b = f.bounds;
            float size = Mathf.Max(b.extents.x, Mathf.Max(b.extents.y, b.extents.z)) * 2f;
            var dir = (eye - b.center); dir.y *= .25f; dir = dir.sqrMagnitude > 1e-4f ? dir.normalized : Vector3.back;
            var pos = b.center + dir * Mathf.Clamp(size * 1.4f, 6f, 60f); var rot = Quaternion.LookRotation(b.center - pos, Vector3.up);
            overridePose = (pos, rot);
            yield return null; yield return null; yield return new WaitForEndOfFrame();
            string name = "screen-closeup-" + (++shot);
            ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
            Debug.Log(Tag + "screenshot " + name + " " + f.label + " at " + V(pos) + " looking at " + V(b.center));
            yield return null;
            overridePose = null;
        }
    }
    (Vector3 position, Quaternion rotation)? overridePose;
    void LateUpdate() { var cam = Camera.main; if (overridePose.HasValue && cam) cam.transform.SetPositionAndRotation(overridePose.Value.position, overridePose.Value.rotation); }   // after the game's own camera (execution order)
    // The camera flashes up close (w-screens): six frames, each aimed at a seat that has a flash drawn (the flash mesh of OriginalAmbient)
    // from 40 units on the field side; "flash-closeup-<n>.png" with the number of flashes active / drawn in the capture frame in the log.
    IEnumerator FlashCloseups()
    {
        var cam = Camera.main; var amb = FindAnyObjectByType<OriginalAmbient>(); if (!cam || !amb) yield break;
        var eye = cam.transform.position; int shots = 0;
        for (int attempt = 0; attempt < 400 && shots < 6; attempt++)
        {
            var go = GameObject.Find("Crowd camera flashes"); var mf = go ? go.GetComponent<MeshFilter>() : null;
            if (!mf || !mf.sharedMesh || mf.sharedMesh.vertexCount < 4) { yield return null; continue; }
            var seat = mf.sharedMesh.vertices[0];
            var dir = eye - seat; dir.y = 0; dir = dir.sqrMagnitude > 1e-4f ? dir.normalized : Vector3.back;
            var pos = seat + dir * 40f + Vector3.up * 6f;
            overridePose = (pos, Quaternion.LookRotation(seat + Vector3.up * 2f - pos, Vector3.up));
            yield return null; yield return new WaitForEndOfFrame();
            string name = "flash-closeup-" + (++shots);
            ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
            Debug.Log(Tag + "screenshot " + name + " camera flashes active " + amb.flashesActive + " drawn " + amb.flashesDrawn + " at " + V(seat));
            for (int i = 0; i < 20; i++) yield return null;
        }
        overridePose = null;
    }
    // The riders on the gate seen close up (run.py --plates): the player from behind (the jersey's name over number, the chest protector) and from the side
    // (the bike's number plates), then the nearest computer rider the same way: plates-chase-<who>-<view>.png, and the plate text each model was given.
    // a posed wheel group's centre: the group's own transform does not move with the bike's node pose, its tyre meshes do
    static Vector3? WheelCentre(Transform wheel)
    {
        if (!wheel) return null;
        var rs = wheel.GetComponentsInChildren<Renderer>(false); if (rs.Length == 0) return null;
        var b = rs[0].bounds; foreach (var r in rs) b.Encapsulate(r.bounds); return b.center;
    }
    IEnumerator PlatesShots(OriginalWorldRuntime rt)
    {
        var cam = Camera.main; if (!cam || !rt.chassis || !rt.frontWheel || !rt.rearWheel) { Debug.Log(Tag + "plates: no camera / local bike"); platesDone = true; yield break; }
        var holders = FindObjectsByType<ReflexPlateTextures>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        foreach (var h in holders) Debug.Log(Tag + "plate textures on " + h.gameObject.name + ": key '" + h.key + "' bike " + (h.bike ? h.bike.name : "-") + " jersey " + (h.jersey ? h.jersey.name : "-") + " cp " + (h.chestProtector ? h.chestProtector.name : "-"));
        // live poses: the riders move once the race runs, the camera is placed from the bike's pose at each shot
        var who = new List<(string name, Func<(Vector3 centre, Vector3 forward)> pose)>();
        who.Add(("player", () => { var f = rt.frontWheel.position - rt.rearWheel.position; f.y = 0; return ((rt.frontWheel.position + rt.rearWheel.position) * 0.5f, f.normalized); }));
        if (rt.opponentRiders)
        {   // the computer riders are posed once the race has run a little: wait for the nearest two
            List<(string name, int slot, ReflexBikeModel bike, ReflexRiderModel rider, bool posed)> near = null;
            for (int wait = 0; wait < 600 && (near == null || near.Count < 2); wait++)
            {
                yield return null;
                near = rt.opponentRiders.Riders.Where(r => r.bike != null && r.posed && WheelCentre(r.bike.rearWheel) is Vector3 rw && Vector3.Distance(rw, rt.rearWheel.position) < 80f)
                    .OrderBy(r => Vector3.Distance(WheelCentre(r.bike.rearWheel).Value, rt.rearWheel.position)).Take(2).ToList();
            }
            int k = 0;
            foreach (var r in near ?? new List<(string name, int slot, ReflexBikeModel bike, ReflexRiderModel rider, bool posed)>())
            {
                var bike = r.bike;
                who.Add(("ai" + (++k) + "-" + r.name.Replace(' ', '_'), () => { var fw = WheelCentre(bike.frontWheel).Value; var rw = WheelCentre(bike.rearWheel).Value; var f = fw - rw; f.y = 0; return ((fw + rw) * 0.5f, f.normalized); }));
            }
        }
        foreach (var w in who)
        {
            // behind (the jersey's back), behind and close (the plate), the bike's left and right sides (the number plate and the panels), the front
            foreach (var view in new[] { "back", "back-close", "left", "right", "front" })
            {
                var (centre, forward) = w.pose();
                var right = Vector3.Cross(Vector3.up, forward);
                Vector3 pos, at;
                switch (view)
                {
                    case "back": pos = centre - forward * 9.5f + Vector3.up * 4.8f; at = centre + Vector3.up * 3.6f; break;
                    case "back-close": pos = centre - forward * 5.2f + Vector3.up * 4.4f; at = centre + Vector3.up * 4.0f; break;
                    case "left": pos = centre - right * 6.2f + Vector3.up * 2.6f; at = centre + Vector3.up * 2.4f; break;
                    case "right": pos = centre + right * 6.2f + Vector3.up * 2.6f; at = centre + Vector3.up * 2.4f; break;
                    default: pos = centre + forward * 7.0f + Vector3.up * 2.8f; at = centre + Vector3.up * 2.6f; break;
                }
                overridePose = (pos, Quaternion.LookRotation(at - pos, Vector3.up));
                yield return null; yield return null; yield return new WaitForEndOfFrame();
                string name = "plates-chase-" + w.name + "-" + view;
                ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png"));
                Debug.Log(Tag + "screenshot " + name + " at " + V(pos) + " looking at " + V(at) + " (rider at " + V(centre) + ")");
                yield return null;
            }
        }
        overridePose = null; platesDone = true;
    }
    IEnumerator Succeed(OriginalWorldRuntime rt)
    {
        yield return Capture("crowd-after", rt);
        yield return FlashCloseups();
        yield return Closeups(rt);
        yield return new WaitForSecondsRealtime(.5f);
        rt.testHeldKeys = Array.Empty<int>();
        Debug.Log(Tag + "PASS crowd " + key);
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
