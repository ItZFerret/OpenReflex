// TEST ONLY (run.py --arcade): one arcade National at NAT_Stillwater started from the open front end's event browser, ridden with
// ordinary mapped keys (MotoInputDriver, exhibition mode) to its natural finish, then the original's end-of-race replay and the
// results screen. Observation only: it clicks painted controls and presses mapped keys; it never sends replay receipts or writes
// race / replay state. PASS = the exhibition replay presenter bound (no career), a natural finish, the replay camera active over
// many native frames with its requests painted and drained, the results screen shown, then RETURN TO MENU and the front end again
// with the player's final save written.
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

public sealed class ArcadeReplayFlowProbe : MonoBehaviour
{
    const string Tag = "[arcade-replay-probe] ";
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
        if (Environment.GetEnvironmentVariable("REFLEX_ARCADE_REPLAY_PROBE") != "1" || FindAnyObjectByType<ArcadeReplayFlowProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Arcade Replay Flow TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<ArcadeReplayFlowProbe>(); Application.runInBackground = true;
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

    double introSeenAt; bool introSkipped, introEnded; int introSkipState;
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
            if (Environment.GetEnvironmentVariable("REFLEX_PLATES_SHOTS") == "1") { Debug.Log("[arcade-replay-probe] SetRiderText FERRET / 998 " + CoreGarageProfile.SetRiderText("Name", "FERRET") + CoreGarageProfile.SetRiderText("Number", "998")); ReflexProfile.Open(); }   // (run.py --plates)
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

        if (menuClicked && confirmClicked)
        {
            ClearInput();
            if (SceneManager.GetActiveScene().name != "FrontEnd" || rt != null || OpenShell.Instance == null) return;
            if (!Shot("return")) return;
            var after = Store().Read(saveMode).Generation;
            if (after != saveGenerationBefore + 1) throw new InvalidOperationException("The event's final save was not written once: generation " + saveGenerationBefore + " -> " + after);
            Debug.Log(Tag + "RETURN_SAVED_PASS generation=" + saveGenerationBefore + "->" + after);
            finished = true; StartCoroutine(Succeed()); return;
        }
        if (rt == null || open == null) return;

        if (!presenterSeen && rt.Running)
        {
            if (rt.OwnedCareerActive || open.Career != null) throw new InvalidOperationException("The arcade event started a career session.");
            if (rt.EventIdentity == null || !rt.EventIdentity.StartsWith(MotoInputDriver.ExhibitionEvent)) throw new InvalidOperationException("Unexpected event identity " + rt.EventIdentity);
            if (open.Replay == null || open.Replay.Generation == 0) throw new InvalidOperationException("The exhibition session has no bound replay presenter.");
            presenterSeen = true; presenterGeneration = open.Replay.Generation; worldIdentity = rt.EventIdentity;
            Debug.Log(Tag + "EXHIBITION_REPLAY_PRESENTER generation=" + presenterGeneration + " event=" + worldIdentity + " career=0");
            steering = new MotoInputDriver(Reflex.Port.FrontEnd.CoreRemapping.Read(false), exhibition: true);
        }
        if (!presenterSeen) return;
        if (rt.EventIdentity != worldIdentity) throw new InvalidOperationException("The world changed during the arcade event.");

        // the Event Intro page's title card (OwnEventIntro): shown over the intro fly-by, a shot with and without the skip legend, then the accept
        var intro = rt.GetComponent<Reflex.Port.OpenFrontEnd.OwnEventIntro>();
        if (intro != null)
        {
            if (intro.Shown)
            {
                if (introSeenAt == 0) { introSeenAt = now; Debug.Log(Tag + "INTRO_CARD event=\"" + intro.EventTextValue + "\" location=\"" + intro.LocationTextValue + "\" visibleLocation=" + intro.LocationVisible); }
                if (now - introSeenAt > 1.6) Shot("intro-card");
                if (now - introSeenAt > 4.0 && Shot("intro-card-hint") && !introSkipped) { introSkipped = true; introSkipState = rt.EventUi.race_state; intro.Skip(); Debug.Log(Tag + "INTRO_SKIP requested after " + (now - introSeenAt).ToString("F1") + " s"); }
            }
            else if (introSkipped && !introEnded) { introEnded = true; Debug.Log(Tag + "INTRO_ENDED race_state=" + rt.EventUi.race_state + " (was " + introSkipState + ") card shown " + (now - introSeenAt).ToString("F1") + " s"); }
        }
        steering.Attach(rt); rt.testHeldKeys = Array.Empty<int>();
        steering.Observe(rt, true, 0, 0, 0);
        rt.testHeldKeys = steering.Inputs(rt, true, now, 0, 0, 0);
        var race = rt.Race;
        if (race.valid != 0 && race.state == 2 && race.race_time > 3) Shot("riding");
        if (steering.ThreeLapsObserved && !finishLogged)
        {
            finishLogged = true;
            Debug.Log(Tag + "NATURAL_FINISH observedLaps=0.." + MotoInputDriver.ExpectedLaps + " nativeLaps=" + race.laps_completed + " state=" + race.state + " riders=" + race.riders);
        }

        replay.Sample(rt, open);
        replayDrained |= replay.PaintedPendingDrained && replay.DistinctActiveFrames >= 2;
        if (!replayLogged && replay.SawActive && replay.SawValidCamera && replay.DistinctActiveFrames >= RequiredReplayFrames)
        {
            if (!Shot("replay")) return;
            replayLogged = true;
            Debug.Log(Tag + "REPLAY_ACTIVE activeFrames=" + replay.DistinctActiveFrames + " cameraValid=1 pendingObserved=" + (replay.SawPending ? 1 : 0) + " paintedDrain=" + (replayDrained ? 1 : 0));
        }
        var results = rt.raceResults;
        if (!resultsLogged && rt.OwnRaceResultsSnapshot != null && results != null && results.Active && Find("results-menu") != null)
        {
            if (!Shot("results")) return;
            resultsLogged = true;
            var rows = rt.OwnRaceResultsSnapshot.Rows;
            var local = rows.FirstOrDefault(r => r.IsLocal);
            // the original's post-event pages are not a game pause: the replay behind them is heard (no AudioListener.pause, the game audio ticks)
            if (AudioListener.pause) throw new InvalidOperationException("The results page paused the audio (AudioListener.pause).");
            Debug.Log(Tag + "RESULTS_AUDIO listenerPause=" + AudioListener.pause + " voices=" + (rt.gameAudio ? rt.gameAudio.ActiveVoices : -1) + " events=" + (rt.gameAudio ? rt.gameAudio.ActiveEvents : -1));
            Debug.Log(Tag + "RESULTS_SHOWN rows=" + rows.Count + " localPlace=" + (local?.Rank ?? "?") + " localStatus=" + (local?.Status ?? "?") + " postState=" + rt.EventUi.post_state);
        }

        string diagnostic = "race=" + race.state + " clock=" + race.race_time + " laps=" + race.laps_completed + "/" + race.num_laps + " place=" + race.place
            + " eventUi=" + rt.EventUi.known + "/" + rt.EventUi.activity_type + "/" + rt.EventUi.race_state + "/" + rt.EventUi.post_state
            + " replayActive=" + replay.SawActive + " replayFrames=" + replay.DistinctActiveFrames + " results=" + resultsLogged;
        string key = race.state + "/" + rt.EventUi.post_state + "/" + replay.SawActive + "/" + resultsLogged + "/" + replayLogged;
        if (key != lastDiagnostic || now - lastLog > 10) { Debug.Log(Tag + diagnostic); lastDiagnostic = key; lastLog = now; }

        if (finishLogged && replayLogged && resultsLogged && replayDrained && replay.SawPending && !menuClicked)
        {
            var menu = Find("results-menu");
            if (menu != null && Submit(menu, now)) { menuClicked = true; Debug.Log(Tag + "USER RETURN TO MENU"); }
            return;
        }
        if (menuClicked && !confirmClicked)
        {
            var confirm = Find("results-confirm");
            if (confirm != null && Submit(confirm, now)) { confirmClicked = true; ClearInput(); Debug.Log(Tag + "USER confirm RETURN TO MENU"); }
        }
    }

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
