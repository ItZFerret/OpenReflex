// TEST ONLY (run.py --reflex-arrows [success|fail]): one arcade National at NAT_Stillwater started from the open front end's event browser, then
// ridden with ordinary mapped keys: throttle, and at speed a hard left throws the bike over, which starts the game's own wreck recovery
// (Player_UpdateWreckRecovery). The probe watches the core's copy of MXUIReflexComponent (rx_session_reflex_hud, OriginalWorldRuntime.ReflexHud),
// takes screenshots of the prompt in the MODERN HUD and in the ORIGINAL HUD (the settings are in the run's scratch save), then either flicks the
// asked direction with the arrow key of the game's keyboard reflex input (success) or does nothing (fail) and shoots the outcome.
// PASS = a prompt with a direction, the screenshots, and the event of the mode. Observation and mapped keys only; no race state is written.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public sealed class ReflexArrowsRideProbe : MonoBehaviour
{
    const string Tag = "[reflex-arrows-probe] ";
    const string EventKey = "National/NAT_Stillwater";
    const int RequiredReplayFrames = 120;   // distinct native frames with the replay camera active
    static readonly FieldInfo Simulation = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.Instance | BindingFlags.NonPublic);
    readonly HashSet<string> shots = new HashSet<string>();
    readonly ReplayFlowDiagnostics replay = new ReplayFlowDiagnostics();
    MotoInputDriver steering;
    VisualElement candidate;
    Action<MeshGenerationContext> draw;
    string shotDirectory, lastDiagnostic;
    double started, candidateSince, lastSubmit, lastLog, browserOpenedAt;
    int candidateFrame, paintedFrame = -1, exceptions;
    bool opened, filtered, selected, launched, presenterSeen, finishLogged, replayLogged, resultsLogged, menuClicked, confirmClicked, finished;
    bool replayDrained;
    ulong presenterGeneration;
    ulong saveGenerationBefore;
    PlayerProfileMode saveMode;
    string worldIdentity;

    // Called by the isolated ReflexShotProbe build hook, never enabled in production.
    public static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_REFLEX_ARROWS_PROBE") != "1" || FindAnyObjectByType<ReflexArrowsRideProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Arcade Replay Flow TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<ReflexArrowsRideProbe>(); Application.runInBackground = true;
    }

    void Awake()
    {
        started = Time.realtimeSinceStartupAsDouble;
        Application.logMessageReceived += ObserveLog;
        try
        {
            shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (string.IsNullOrWhiteSpace(shotDirectory) || string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR")))
                throw new InvalidOperationException("Detached save and screenshot directories are required.");
            Directory.CreateDirectory(shotDirectory);
            if (Environment.GetEnvironmentVariable("REFLEX_MOTO_CAREER_FLOW_PROBE") == "1" || Environment.GetEnvironmentVariable("REFLEX_CAREER_FLOW_PROBE") == "1")
                throw new InvalidOperationException("Mutually exclusive test drivers required.");
            Debug.Log(Tag + "BEGIN arcade " + EventKey + " from the event browser, natural finish, replay and results.");
        }
        catch (Exception e) { Fail(e.ToString()); }
    }

    void ObserveLog(string message, string stack, LogType type)
    {
        if (type == LogType.Exception) exceptions++;
        if (message != null && message.Contains("was not saved")) { Debug.Log(Tag + "SAVE_FAILURE_LOGGED"); exceptions++; }
    }

    void Update()
    {
        if (finished) return;
        try { Tick(); } catch (Exception e) { Fail(e.ToString()); }
    }

    void ClearInput()
    { foreach (var rt in FindObjectsByType<OriginalWorldRuntime>(FindObjectsInactive.Include, FindObjectsSortMode.None)) rt.testHeldKeys = Array.Empty<int>(); }

    PlayerSnapshotStore Store() => new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory);

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 3600) throw new TimeoutException("Arcade replay flow wall budget exceeded.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s) or a save failure: " + exceptions);
        if (OpenShell.Instance == null && !launched) return;
        if (!opened)
        {
            if (now - started < 2) return;
            // The detached save copy is the player's profile for this test (the guest isolated the registered one before Awake).
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            saveMode = CoreGarageProfile.UnlockEverythingEnabled ? PlayerProfileMode.Unlocked : PlayerProfileMode.Career;
            saveGenerationBefore = Store().Read(saveMode).Generation;
            OpenShell.Instance.ShowBrowser(false);
            opened = true; browserOpenedAt = now;
            Debug.Log(Tag + "BROWSER_OPENED save=" + saveMode + " generation=" + saveGenerationBefore);
            return;
        }
        if (!filtered)
        {
            var filter = Find("ou-filter-national");
            if (filter == null) { if (now - browserOpenedAt > 120) throw new TimeoutException("The event browser has no National filter. " + VisibleActions()); return; }
            if (Submit(filter, now)) { filtered = true; Debug.Log(Tag + "USER filter National"); }
            return;
        }
        if (!selected)
        {
            var card = FindCard(EventKey);
            if (card == null) { if (now - browserOpenedAt > 180) throw new TimeoutException("The event browser has no " + EventKey + " card. " + VisibleActions()); return; }
            if (!Drawn(card, now) || !Shot("browser", card)) return;
            if (Submit(card, now)) { selected = true; Debug.Log(Tag + "USER selected " + EventKey); }
            return;
        }
        if (!launched)
        {
            var start = Find("ou-setup-start");
            if (start == null) return;
            if (!Drawn(start, now) || !Shot("setup", start)) return;
            if (Submit(start, now)) { launched = true; Debug.Log(Tag + "USER START EVENT " + EventKey); }
            return;
        }

        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var driver = rt == null ? null : Simulation?.GetValue(rt) as IReflexSimDriver;
        var open = driver?.Open;
        if (open != null && !string.IsNullOrEmpty(open.Failure)) throw new InvalidOperationException("Worker: " + open.Failure);
        if (rt != null && rt.FailureMessage != null) throw new InvalidOperationException("World: " + rt.FailureMessage);
        if (rt == null || open == null || !rt.Running) return;
        if (rt.EventIdentity == null || !rt.EventIdentity.StartsWith(MotoInputDriver.ExhibitionEvent)) throw new InvalidOperationException("Unexpected event identity " + rt.EventIdentity);
        var race = rt.Race; var bike = rt.Bike;
        if (race.valid == 0 || race.state != 2) return;
        float mph = bike.speed * 0.68182f;
        // the ride: throttle (W); at speed a hard left (A) throws the bike over; the prompt (the core's wreck-recovery copy) then runs its window
        const int W = 0x11, A = 0x1E, Up = 0xC8, Down = 0xD0, Left = 0xCB, Right = 0xCD;
        var keys = new List<int> { W };
        var h = rt.HasReflexHud ? rt.ReflexHud : default;
        if (rt.HasReflexHud && (h.beginEvents != lastBegin || h.successEvents != lastSuccess || h.failEvents != lastFail || (h.shown != 0) != lastShown))
        {
            Debug.Log(Tag + "STATE shown=" + h.shown + " lit=" + h.litMask + " dir=" + h.direction + " mode=" + h.mode + " type=" + h.wreckType + " time=" + h.timeLeft.ToString("F3") + "/" + h.timeMax.ToString("F3")
                + " source=" + h.inputSource + " begin=" + h.beginEvents + " success=" + h.successEvents + " fail=" + h.failEvents + " hud=" + Reflex.Port.Options.ReflexSettings.HudStyle);
            lastBegin = h.beginEvents; lastSuccess = h.successEvents; lastFail = h.failEvents; lastShown = h.shown != 0;
        }
        if (phase == 0)
        {
            if (mph > 33f) { phase = 1; swerveFrames = 0; Debug.Log(Tag + "SWERVE at " + mph.ToString("F1") + " mph"); }
        }
        if (phase == 1)
        {
            keys.Add(A); swerveFrames++;
            if (rt.HasReflexHud && h.shown != 0 && h.direction != 0)
            {
                phase = 2; promptDir = h.direction; promptAt = now; modernShot = false; originalShot = false; resultShot = false;
                Debug.Log(Tag + "PROMPT direction=" + h.direction + " time=" + h.timeLeft.ToString("F3") + "/" + h.timeMax.ToString("F3") + " sourceInput=" + h.inputSource + " mph=" + mph.ToString("F1"));
            }
            else if (swerveFrames > 400) { phase = 0; Debug.Log(Tag + "no wreck prompt after a long swerve; trying again"); swerveFrames = 0; }
        }
        if (phase == 2)
        {
            if (!modernShot) { modernShot = true; Reflex.Port.Options.ReflexSettings.HudStyle = Reflex.Port.Options.HudStyle.Modern; StartCoroutine(Capture("prompt-modern")); }
            else if (!originalShot && now - promptAt > 0.12) { originalShot = true; Reflex.Port.Options.ReflexSettings.HudStyle = Reflex.Port.Options.HudStyle.Original; StartCoroutine(Capture("prompt-original")); }
            // the flick (success) after the shots: the arrow key of the asked direction (the game's keyboard reflex input), else nothing (fail)
            if (mode == "success" && originalShot && now - promptAt > 0.35)
                keys.Add(promptDir == 1 ? Up : promptDir == 3 ? Down : promptDir == 4 ? Left : Right);
            if (h.successEvents > baseSuccess || h.failEvents > baseFail)
            {
                phase = 3; resultAt = now; resultKind = h.successEvents > baseSuccess ? "success" : "fail";
                Reflex.Port.Options.ReflexSettings.HudStyle = Reflex.Port.Options.HudStyle.Modern;
                Debug.Log(Tag + "RESULT " + resultKind + " after " + (now - promptAt).ToString("F2") + " s");
            }
        }
        if (phase == 3)
        {
            if (!resultShot && now - resultAt > 0.18) { resultShot = true; StartCoroutine(Capture("result-modern")); }
            if (resultShot && now - resultAt > 1.4)
            {
                if (resultKind != mode) throw new InvalidOperationException("Expected " + mode + ", got " + resultKind);
                finished = true; Debug.Log(Tag + "PASS reflex " + mode + " direction=" + promptDir); StartCoroutine(Succeed2()); return;
            }
        }
        if (phase == 1 && baseSuccess == 0 && baseFail == 0 && rt.HasReflexHud) { baseSuccess = h.successEvents; baseFail = h.failEvents; }
        if (now - started > 900) throw new TimeoutException("No wreck-recovery prompt in 900 s.");
        rt.testHeldKeys = keys.ToArray();
    }
    int phase, swerveFrames; uint lastBegin, lastSuccess, lastFail, baseSuccess, baseFail; bool lastShown, modernShot, originalShot, resultShot; int promptDir; double promptAt, resultAt; string resultKind;
    readonly string mode = Environment.GetEnvironmentVariable("REFLEX_REFLEX_ARROWS_MODE") == "fail" ? "fail" : "success";
    IEnumerator Succeed2() { ClearInput(); yield return new WaitForSecondsRealtime(.5f); Application.Quit(0); }

    // ---------------------------------------------------------------------------------------------------------- UI helpers
    // (the same observation rules as MotoCareerFlowProbe: a control counts only once painted, visible and hit-testable)
    static string VisibleActions()
    {
        var text = new List<string>();
        foreach (var doc in FindObjectsByType<UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            if (doc.rootVisualElement == null) continue;
            doc.rootVisualElement.Query<VisualElement>().ForEach(value =>
            {
                if (!Visible(value)) return;
                if (value.userData is OuNavItem) text.Add("action:" + value.name + ":enabled=" + value.enabledInHierarchy);
                else if (value is Label label && !string.IsNullOrWhiteSpace(label.text)) text.Add("label:" + label.text);
            });
        }
        return string.Join(" | ", text);
    }
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
    static bool HitTestPoint(VisualElement value, Vector2 point)
    {
        var picked = value.panel.Pick(point);
        return picked == value || (picked != null && value.Contains(picked));
    }
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
    // A browser card carries its event key as the tooltip (OpenShell.Browser BuildCard).
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
            bool rebuilt = candidate != null && candidate.name == value.name && candidate.name.Length > 0;
            double since = rebuilt ? candidateSince : now;
            Unwatch(); candidate = value; candidateSince = since; candidateFrame = Time.frameCount; paintedFrame = -1;
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
            using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger))
            {
                if (click.button != 0 || click.clickCount != 1) throw new InvalidOperationException("Pooled click lost its left-button semantics.");
                click.target = value; value.SendEvent(click);
            }
        }
        finally { value.UnregisterCallback(observe, TrickleDown.TrickleDown); }
        if (!delivered) throw new InvalidOperationException("Left-click was not delivered to the actual control: " + value.name);
        lastSubmit = now;
        Debug.Log(Tag + "TEST USER left-click delivered " + (string.IsNullOrEmpty(value.name) ? value.tooltip : value.name) + " button=0 count=1 bounds=" + value.worldBound + " hitTest=1");
        Unwatch(); return true;
    }
    void Unwatch() { if (candidate != null && draw != null) candidate.generateVisualContent -= draw; candidate = null; draw = null; }
    bool Shot(string name, VisualElement required = null)
    {
        if (shots.Contains(name)) return File.Exists(Path.Combine(shotDirectory, name + ".png"));
        shots.Add(name); StartCoroutine(Capture(name, required)); return false;
    }
    IEnumerator Capture(string name, VisualElement required = null)
    {
        yield return new WaitForEndOfFrame();
        if (required != null && !Clickable(required)) { shots.Remove(name); yield break; }
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png")); Debug.Log(Tag + "screenshot " + name);
    }
    IEnumerator Succeed()
    {
        yield return new WaitForSecondsRealtime(.5f);
        Debug.Log(Tag + "PASS arcade " + EventKey + ": exhibition replay presenter " + presenterGeneration + ", natural " + MotoInputDriver.ExpectedLaps + "-lap finish, replay "
            + replay.DistinctActiveFrames + " active frames painted and drained, results shown, returned to the menu with the final save");
        Application.Quit(0);
    }
    void Fail(string message)
    {
        if (finished) return; finished = true; ClearInput(); Unwatch();
        Debug.LogError(Tag + "FAIL " + message); StartCoroutine(StopFailed());
    }
    IEnumerator StopFailed() { if (!string.IsNullOrEmpty(shotDirectory)) { yield return Capture("failure"); yield return new WaitForSecondsRealtime(.3f); } Application.Quit(1); }
    void OnDestroy() { steering?.Dispose(); Application.logMessageReceived -= ObserveLog; ClearInput(); Unwatch(); }
}
