// TEST ONLY (run.py --activation): the Free Ride activation areas in the open product. The open front end's FREE RIDE at FR_Alpine
// (the Race & Ride challenges Free Ride: OwnActivationArea, InCareerMode), ridden with ordinary mapped keys along an authored route to the
// level's Hill Climb area; the game marks the area (MXPlayer +0xa0 / +0xa4 / +0x1a8, rx_session_activation) and the AAA notification
// shows; the pause key (Esc, a real keyboard event) starts the area's challenge behind the AAA loading page (OwnAaaLoading); the challenge
// runs natively (its activity kind 1 = HillClimb in the race export, InProgress) and is ridden with the throttle (an ordinary mapped key)
// until it ends: the end-of-challenge page (OwnPostChallenge, the install's PostChallengePopup) shows the core's medal state (a medal or
// none); RESTART (Enter) restarts the activity in place (same world, the state machine runs again); the challenge ends again and the
// page's EXIT (Right, Enter) goes back to the level's Free Ride. Also: the challenge's rules popup (OwnPreChallenge: the install's
// PreChallengePopup) holds the challenge in Pre until Enter; the KeyMap glyphs of the three pages (ReflexKeyHints is switched on for this
// test player: the port's hints are an opt-in extra); the popup / voice sounds the original's Lua plays; the PageWipe at the end of the AAA
// loading page; and the pause page's RESTART in the challenge, which restarts the activity in place. Observation only: it clicks painted
// controls and presses keys; it writes no game or native state.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public sealed class ActivationFlowProbe : MonoBehaviour
{
    const string Tag = "[activation-flow-probe] ";
    const string FreeRideKey = "FreeRide/FR_Alpine", ChallengeKey = "HillClimb/FR_Alpine", Area = "HillClimb";
    // The authored route from the Free Ride's start (1738, -1652) to the level's "HillClimb" StartActivity trigger (FR_AlpineFreeRide,
    // 390.4, 953.9): x, z waypoints over the collision terrain (core/tools/activation_native_test.c plan), passed within 40 units.
    static readonly Vector2[] Route =
    {
        new Vector2(1745, -1521), new Vector2(1745, -1393), new Vector2(1729, -1265), new Vector2(1713, -1137), new Vector2(1697, -1009),
        new Vector2(1697, -881), new Vector2(1697, -753), new Vector2(1681, -625), new Vector2(1633, -497), new Vector2(1633, -369),
        new Vector2(1617, -241), new Vector2(1553, -113), new Vector2(1553, 15), new Vector2(1553, 143), new Vector2(1553, 271),
        new Vector2(1457, 399), new Vector2(1329, 479), new Vector2(1201, 575), new Vector2(1073, 623), new Vector2(945, 639),
        new Vector2(817, 687), new Vector2(689, 799), new Vector2(561, 863), new Vector2(433, 927), new Vector2(390.4f, 953.9f),
    };
    static readonly FieldInfo Simulation = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.Instance | BindingFlags.NonPublic);
    readonly HashSet<string> shots = new HashSet<string>();
    readonly Queue<Key> keyQueue = new Queue<Key>();
    VisualElement candidate;
    Action<MeshGenerationContext> draw;
    string shotDirectory, lastDiagnostic;
    double started, candidateSince, lastSubmit, lastLog, browserOpenedAt, stuckSince = -1, reverseUntil, stepAt, challengeRunningSince = -1;
    int candidateFrame, paintedFrame = -1, exceptions, waypoint, keyPhase;
    int throttle, brake, left, right;
    bool opened, selected, launched, freeRideSeen, areaLogged, notificationLogged, pausePressed, challengeSeen, challengeRunningLogged;
    bool aaaLogged, popupSeen, restartPressed, restartedSeen, popup2Seen, exitPressed, backSeen, finished;
    bool preSeen, preAccepted, wipeLogged, pauseRestartDone, voiceLogged, glyphNoteLogged;
    int pauseStep; double preSeenAt = -1, pauseStepAt; string worldBeforePauseRestart; bool pauseRestartLogged;
    string worldBeforeRestart; double popupWaitSince = -1;
    Key pendingKey; bool keyDown;

    public static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_ACTIVATION_FLOW_PROBE") != "1" || FindAnyObjectByType<ActivationFlowProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Activation Flow TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<ActivationFlowProbe>(); Application.runInBackground = true;
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
            PlayerPrefs.SetInt("ReflexKeyHints", 1);   // the port's KeyMap glyphs (the PC game draws none): the test player's own preference domain
            var controls = Reflex.Port.FrontEnd.CoreRemapping.Read(false);
            throttle = Scan(controls, "Throttle"); brake = Scan(controls, "Brake"); left = Scan(controls, "SteerLeft"); right = Scan(controls, "SteerRight");
            Debug.Log(Tag + "BEGIN " + FreeRideKey + " -> the " + Area + " area -> " + ChallengeKey + " -> EXIT; keys throttle=" + throttle + " brake=" + brake + " left=" + left + " right=" + right);
        }
        catch (Exception e) { Fail(e.ToString()); }
    }
    static int Scan(Reflex.Port.FrontEnd.RemapTable t, string name)
    {
        int col = Array.IndexOf(t.columns, name);
        if (col < 0 || col >= t.kb.Length) throw new InvalidOperationException("Missing keyboard action " + name);
        var matches = Enumerable.Range(1, 255).Where(i => t.scan[i] == t.kb[col]).ToArray();
        if (t.kb[col] == 0 || matches.Length != 1) throw new InvalidOperationException("Nonunique scan code for " + name);
        return matches[0];
    }

    void ObserveLog(string message, string stack, LogType type)
    {
        if (type == LogType.Exception) exceptions++;
        if (message != null && message.Contains("was not saved")) { Debug.Log(Tag + "SAVE_FAILURE_LOGGED"); exceptions++; }
        if (message != null && message.Contains("[Challenge] pause RESTART")) pauseRestartLogged = true;
    }

    void Update()
    {
        if (finished) return;
        try { PumpKeys(); Tick(); } catch (Exception e) { Fail(e.ToString()); }
    }

    // ---------------------------------------------------------------------------------------------- keys (real keyboard events)
    // One key at a time: its down state for a frame, then the release, then the next one.
    void Press(Key key) { keyQueue.Enqueue(key); }
    void PumpKeys()
    {
        var kb = Keyboard.current;
        if (kb == null) { if (keyQueue.Count > 0) throw new InvalidOperationException("No keyboard device for the test's key presses."); return; }
        if (keyDown) { InputSystem.QueueStateEvent(kb, new KeyboardState()); keyDown = false; keyPhase = Time.frameCount; return; }
        if (keyQueue.Count == 0 || Time.frameCount - keyPhase < 3) return;
        pendingKey = keyQueue.Dequeue();
        InputSystem.QueueStateEvent(kb, new KeyboardState(pendingKey)); keyDown = true;
        Debug.Log(Tag + "TEST USER key " + pendingKey);
    }
    bool KeysIdle => keyQueue.Count == 0 && !keyDown && Time.frameCount - keyPhase >= 3;

    void ClearInput() { foreach (var rt in FindObjectsByType<OriginalWorldRuntime>(FindObjectsInactive.Include, FindObjectsSortMode.None)) rt.testHeldKeys = Array.Empty<int>(); }

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 3600) throw new TimeoutException("Activation flow wall budget exceeded.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s) or a save failure: " + exceptions);
        if (OpenShell.Instance == null && !launched) return;
        if (!opened)
        {
            if (now - started < 2) return;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenShell.Instance.ShowBrowser(true);
            opened = true; browserOpenedAt = now;
            Debug.Log(Tag + "BROWSER_OPENED free ride");
            return;
        }
        if (!selected)
        {
            var card = FindCard(FreeRideKey);
            if (card == null) { if (now - browserOpenedAt > 180) throw new TimeoutException("The Free Ride browser has no " + FreeRideKey + " card."); return; }
            if (!Drawn(card, now) || !Shot("browser", card)) return;
            if (Submit(card, now)) { selected = true; Debug.Log(Tag + "USER selected " + FreeRideKey); }
            return;
        }
        if (!launched)
        {
            var start = Find("ou-setup-start");
            if (start == null) return;
            if (!Drawn(start, now) || !Shot("setup", start)) return;
            if (Submit(start, now)) { launched = true; Debug.Log(Tag + "USER START RIDE " + FreeRideKey); }
            return;
        }

        if (pausePressed && !aaaLogged)
        {   // the AAA loading page of the challenge start (the install's AAALoadingLayer: black, LOADING, the turning arrow)
            var aaa = OwnAaaLoading.Current;
            if (aaa != null && aaa.Built && aaa.SpinnerDegrees > 30)
            {
                if (aaa.Line == null || !aaa.Line.StartsWith("LOADING", StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("The AAA loading page shows \"" + aaa.Line + "\".");
                if (Shot("aaa")) { aaaLogged = true; Debug.Log(Tag + "AAA_LOADING AAALoadingLayer text=\"" + aaa.Line + "\" spinner=" + aaa.SpinnerDegrees.ToString("F0", CultureInfo.InvariantCulture)); }
            }
        }
        if (pausePressed && !wipeLogged && OwnAaaLoading.Current != null && OwnAaaLoading.Current.WipeStage != 0) Shot("wipe");   // optional: the wipe's frames
        if (pausePressed && !wipeLogged && OwnAaaLoading.WipesCompleted > 0)
        {
            wipeLogged = true;
            Debug.Log(Tag + "PAGE_WIPE AAALoading transitions=" + OwnAaaLoading.WipeTransitions + " completed=" + OwnAaaLoading.WipesCompleted);
        }
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var driver = rt == null ? null : Simulation?.GetValue(rt) as IReflexSimDriver;
        var open = driver?.Open;
        if (open != null && !string.IsNullOrEmpty(open.Failure)) throw new InvalidOperationException("Worker: " + open.Failure);
        if (rt != null && rt.FailureMessage != null) throw new InvalidOperationException("World: " + rt.FailureMessage);
        if (rt == null || !rt.Running) return;
        var area = rt.GetComponent<OwnActivationArea>();
        if (area == null) throw new InvalidOperationException("The world " + rt.level + " has no activation areas (OwnActivationArea).");

        // ---- the Free Ride, then the area
        if (!pausePressed)
        {
            if (area.IsChallenge || area.Event.key != FreeRideKey) throw new InvalidOperationException("Unexpected world " + area.Event.key);
            if (!freeRideSeen)
            {
                freeRideSeen = true;
                Debug.Log(Tag + "FREE_RIDE_CHALLENGES " + FreeRideKey + " event=" + rt.EventIdentity + " activation=" + rt.HasActivation + " active=" + rt.Activation.active);
            }
            var a = rt.Activation;
            if (rt.HasActivation && a.active != 0 && a.Activity == Area)
            {
                rt.testHeldKeys = Array.Empty<int>();
                if (!areaLogged) { areaLogged = true; Debug.Log(Tag + "AREA_ENTERED " + a.Activity + " track=" + a.track_id + " requiredClass=" + a.required_class + " waypoint=" + waypoint); }
                if (!area.NotificationShown) return;
                if (!notificationLogged)
                {
                    if (!Shot("area")) return;
                    notificationLogged = true; Debug.Log(Tag + "NOTIFICATION_SHOWN SinglePlayer_AAA_Activation NotificationContainer");
                    var g = area.Glyphs;
                    if (g == null || g.Count < 1 || g.Shown < 1) throw new InvalidOperationException("The AAA notification shows no Start glyph (" + (g == null ? "no glyphs" : g.Count + " KeyMap images, " + g.Shown + " shown") + ").");
                    Debug.Log(Tag + "GLYPHS notification keymaps=" + g.Count + " shown=" + g.Shown + " " + g.Describe());
                }
                if (!voiceLogged)
                {   // AAANotification:ToggleNotification: the CHALLENGE_APPEAR narration on entering the area (the race's event player)
                    if (!area.VoicePlaying) throw new InvalidOperationException("The area's narration was not requested.");
                    string vo; lock (FeSounds.Log) vo = FeSounds.Log.LastOrDefault(l => l.Contains("CHALLENGE_APPEAR"));
                    if (vo == null) throw new InvalidOperationException("No CHALLENGE_APPEAR voice request was logged.");
                    voiceLogged = true; Debug.Log(Tag + "VOICE " + vo);
                }
                if (!KeysIdle) return;
                Press(Key.Escape); pausePressed = true; stepAt = now;
                Debug.Log(Tag + "USER pause in the area");
                return;
            }
            if (rt.HasActivation && a.active != 0) Debug.Log(Tag + "AREA_PASSED " + a.Activity);
            rt.testHeldKeys = Steer(rt, now);
            if (rt.Race.state == 2 || rt.Bike.speed > 1) Shot("riding");
            return;
        }

        // ---- the challenge
        if (!challengeSeen)
        {
            if (!area.IsChallenge) { if (now - stepAt > 60) throw new TimeoutException("The pause in the area started no challenge (still " + area.Event.key + ")."); return; }
            if (area.Event.key != ChallengeKey || OwnActivationArea.LastStarted != ChallengeKey) throw new InvalidOperationException("The area started " + area.Event.key);
            challengeSeen = true;
            Debug.Log(Tag + "CHALLENGE_STARTED " + area.Event.key + " activation=" + area.Event.spec.activation + " track=" + area.Event.spec.trackId + " event=" + rt.EventIdentity);
        }
        // ---- PreChallengePopup: the core holds the challenge in Pre until the page's accept
        if (!preSeen)
        {
            var pre = rt.preChallenge;
            if (pre == null) throw new InvalidOperationException("The challenge world has no OwnPreChallenge.");
            if (!pre.Active || !pre.Rendered)
            {
                if (popupWaitSince < 0) popupWaitSince = now;
                if (now - popupWaitSince > 300) throw new TimeoutException("The challenge never showed its rules popup (held=" + rt.PreChallengeHeld + " state=" + rt.Challenge.state + ").");
                Diagnose("waiting for the rules popup: held=" + rt.PreChallengeHeld + " challenge valid=" + rt.HasChallenge + " state=" + rt.Challenge.state, now);
                return;
            }
            CheckPre(pre);
            if (preSeenAt < 0) preSeenAt = now;
            if (rt.Challenge.state != 0) throw new InvalidOperationException("The challenge left Pre (state " + rt.Challenge.state + ") while its rules popup was up.");
            if (now - preSeenAt < 3 || !Shot("pre")) return;   // held for 3 s of real time (at least 180 game frames) before the accept
            preSeen = true; popupWaitSince = -1;
            var g = pre.Glyphs;
            if (g == null || g.Count < 1 || g.Shown < 1) throw new InvalidOperationException("The rules popup shows no accept glyph (" + (g == null ? "no glyphs" : g.Count + " KeyMap images, " + g.Shown + " shown") + ").");
            Debug.Log(Tag + "PRE_POPUP title=\"" + pre.Title + "\" message=\"" + pre.Message.Replace("\n", " / ") + "\" criteria=" + string.Join("|", pre.Criteria) + " record=" + (pre.Record ?? "none") + " button=" + pre.Button
                + " kind=" + pre.Shown.kind + " state=" + pre.Shown.state);
            Debug.Log(Tag + "PRE_HELD state=" + rt.Challenge.state + " seconds=" + (now - preSeenAt).ToString("F1", CultureInfo.InvariantCulture) + " bikeFrames=" + rt.Bike.frames);
            Debug.Log(Tag + "GLYPHS pre keymaps=" + g.Count + " shown=" + g.Shown + " " + g.Describe());
        }
        if (!preAccepted)
        {
            if (!KeysIdle) return;
            Press(Key.Enter); preAccepted = true; stepAt = now;
            Debug.Log(Tag + "USER ACCEPT pre popup (Enter)");
            return;
        }
        if (!challengeRunningLogged && rt.preChallenge.Active) { if (now - stepAt > 20) throw new TimeoutException("Enter did not accept the rules popup."); return; }
        if (!challengeRunningLogged)
        {
            var race = rt.Race;
            // the race export of a challenge: A+0x21c the state, A+0xff070 the challenge kind (1 = HillClimb)
            if (race.state != 2 || race.num_laps != 1) { Diagnose("challenge race valid=" + race.valid + " state=" + race.state + " kind=" + race.num_laps, now); return; }
            if (challengeRunningSince < 0) challengeRunningSince = now;
            if (now - challengeRunningSince < 0.5 || !Shot("challenge")) return;   // pause early in the climb: a held throttle can finish it within seconds at REFLEX_TEST_SPEED
            challengeRunningLogged = true;
            Debug.Log(Tag + "CHALLENGE_RUNNING kind=" + race.num_laps + " state=" + race.state + " valid=" + race.valid);
            return;
        }
        if (!aaaLogged) throw new InvalidOperationException("The challenge started without the AAA loading page being seen.");
        if (!pauseRestartDone && !PauseRestart(rt, now)) return;

        if (exitPressed)
        {
            if (!backSeen)
            {
                if (area.IsChallenge) { if (now - stepAt > 60) throw new TimeoutException("QUIT did not leave the challenge."); return; }
                if (area.Event.key != FreeRideKey) throw new InvalidOperationException("EXIT went to " + area.Event.key);
                backSeen = true; stepAt = now;
                Debug.Log(Tag + "BACK_IN_FREE_RIDE " + area.Event.key + " event=" + rt.EventIdentity);
            }
            if (now - stepAt < 3 || !Shot("exit")) return;
            CheckSounds();
            finished = true; ClearInput(); StartCoroutine(Succeed());
        }

        // ---- the challenge ridden to its end: the end-of-challenge page
        var post = rt.postChallenge;
        if (post == null) throw new InvalidOperationException("The challenge world has no OwnPostChallenge.");
        if (!popupSeen)
        {
            if (!post.Active || !post.Rendered)
            {
                if (popupWaitSince < 0) popupWaitSince = now;
                if (now - popupWaitSince > 600) throw new TimeoutException("The challenge never reached its end page (state " + rt.Challenge.state + " value " + rt.Challenge.value + ").");
                if (!rt.paused && !rt.OwnMenuActive) rt.testHeldKeys = new[] { throttle };
                Diagnose("challenge state=" + rt.Challenge.state + " value=" + rt.Challenge.value.ToString("F1", CultureInfo.InvariantCulture), now);
                return;
            }
            rt.testHeldKeys = Array.Empty<int>();
            CheckPopup(post, 1);
            if (!Shot("post")) return;
            popupSeen = true; popupWaitSince = -1; worldBeforeRestart = rt.EventIdentity;
            var pg = post.Glyphs;
            if (pg == null || pg.Count < 1 || pg.Shown < 1) throw new InvalidOperationException("The end page shows no accept glyph (" + (pg == null ? "no glyphs" : pg.Count + " KeyMap images, " + pg.Shown + " shown") + ").");
            Debug.Log(Tag + "GLYPHS post keymaps=" + pg.Count + " shown=" + pg.Shown + " " + pg.Describe());
            Debug.Log(Tag + "POST_POPUP medal=" + post.Shown.medal + " kind=" + post.Shown.kind + " state=" + post.Shown.state + " value=" + post.Shown.value.ToString("F2", CultureInfo.InvariantCulture)
                + " title=\"" + post.Title + "\" message=\"" + post.Message.Replace("\n", " / ") + "\" buttons=" + string.Join(",", post.ButtonLabels) + " focus=" + post.FocusedButton);
            return;
        }
        if (!restartPressed)
        {
            if (!KeysIdle) return;
            if (post.FocusedButton != 0) throw new InvalidOperationException("RESTART is not the page's first focus.");
            Press(Key.Enter); restartPressed = true; stepAt = now;
            Debug.Log(Tag + "USER RESTART (Enter on the page's first button)");
            return;
        }
        if (!restartedSeen)
        {
            if (post.RestartCount < 1) { if (now - stepAt > 20) throw new TimeoutException("Enter did not choose RESTART."); return; }
            var c = rt.Challenge;
            if (post.Active || !rt.HasChallenge || c.state == 4) { if (now - stepAt > 60) throw new TimeoutException("The restart did not leave Post (state " + c.state + ")."); return; }
            if (rt.EventIdentity != worldBeforeRestart) throw new InvalidOperationException("RESTART made a new world: " + worldBeforeRestart + " -> " + rt.EventIdentity);
            restartedSeen = true; stepAt = now;
            Debug.Log(Tag + "CHALLENGE_RESTARTED state=" + c.state + " medal=" + c.medal + " value=" + c.value.ToString("F1", CultureInfo.InvariantCulture) + " sameWorld=1 event=" + rt.EventIdentity);
        }
        if (!popup2Seen)
        {
            if (!post.Active || !post.Rendered || post.ShownCount < 2)
            {
                if (popupWaitSince < 0) popupWaitSince = now;
                if (now - popupWaitSince > 600) throw new TimeoutException("The restarted challenge never reached its end page again (state " + rt.Challenge.state + ").");
                if (!rt.paused && !rt.OwnMenuActive) rt.testHeldKeys = new[] { throttle };
                Diagnose("restarted challenge state=" + rt.Challenge.state + " value=" + rt.Challenge.value.ToString("F1", CultureInfo.InvariantCulture), now);
                return;
            }
            rt.testHeldKeys = Array.Empty<int>();
            CheckPopup(post, 2);
            if (!Shot("post2")) return;
            popup2Seen = true; stepAt = now;
            Debug.Log(Tag + "POST_POPUP_AGAIN medal=" + post.Shown.medal + " value=" + post.Shown.value.ToString("F2", CultureInfo.InvariantCulture) + " counts=" + post.Shown.count0 + "," + post.Shown.count1 + "," + post.Shown.count2
                + " record=" + (post.Shown.recordValid != 0 ? post.Shown.record.ToString("F2", CultureInfo.InvariantCulture) : "none"));
            return;
        }
        if (!exitPressed)
        {
            if (!KeysIdle) return;
            if (post.FocusedButton != 1) { Press(Key.RightArrow); return; }
            Press(Key.Enter); exitPressed = true; stepAt = now;
            Debug.Log(Tag + "USER EXIT (Right, Enter on the page's second button)");
            return;
        }
    }

    // The page against the core's state it was opened with: the texts the original's Lua puts in (title, the medal's sentence, RESTART / EXIT),
    // the Post state, the criteria; the medal sentence is looked for by its medal word (the strings are the install's).
    void CheckPopup(OwnPostChallenge post, int round)
    {
        var c = post.Shown;
        if (c.valid == 0 || c.state != 4) throw new InvalidOperationException("The page opened outside Post: valid=" + c.valid + " state=" + c.state);
        if (c.kind != 1) throw new InvalidOperationException("The Hill Climb's page has kind " + c.kind);
        if (c.medal < 0 || c.medal > 3) throw new InvalidOperationException("Medal " + c.medal);
        if (!(c.criteria0 > 0 && c.criteria1 > c.criteria0 && c.criteria2 > c.criteria1)) throw new InvalidOperationException("Criteria " + c.criteria0 + " / " + c.criteria1 + " / " + c.criteria2);
        if (c.medal > 0 && (c.recordValid == 0 || c.Count(c.medal - 1) < 1)) throw new InvalidOperationException("A medal " + c.medal + " without a stored record (valid " + c.recordValid + ", counts " + c.count0 + "," + c.count1 + "," + c.count2 + ")");
        if (string.IsNullOrWhiteSpace(post.Title) || !post.Title.ToUpperInvariant().Contains("HILL")) throw new InvalidOperationException("Title \"" + post.Title + "\"");
        string word = c.medal == 3 ? "Gold" : c.medal == 2 ? "Silver" : c.medal == 1 ? "Bronze" : "did not win";
        if (post.Message == null || !post.Message.Contains(word)) throw new InvalidOperationException("Message \"" + post.Message + "\" for medal " + c.medal + " lacks \"" + word + "\"");
        var labels = post.ButtonLabels;
        if (labels[0] != "RESTART" || labels[1] != "EXIT") throw new InvalidOperationException("Buttons " + labels[0] + " / " + labels[1]);
        if (post.ShownCount != round) throw new InvalidOperationException("The page was shown " + post.ShownCount + " times in round " + round);
    }

    // The rules popup against the core's Pre state: the install's strings and the level's criteria.
    void CheckPre(OwnPreChallenge pre)
    {
        var c = pre.Shown;
        if (c.valid == 0 || c.state != 0) throw new InvalidOperationException("The rules popup opened outside Pre: valid=" + c.valid + " state=" + c.state);
        if (c.kind != 1) throw new InvalidOperationException("The Hill Climb's rules popup has kind " + c.kind);
        if (!(c.criteria0 > 0 && c.criteria1 > c.criteria0 && c.criteria2 > c.criteria1)) throw new InvalidOperationException("Criteria " + c.criteria0 + " / " + c.criteria1 + " / " + c.criteria2);
        if (string.IsNullOrWhiteSpace(pre.Title) || !pre.Title.ToUpperInvariant().Contains("HILL")) throw new InvalidOperationException("Title \"" + pre.Title + "\"");
        if (string.IsNullOrWhiteSpace(pre.Message) || !pre.Message.Contains("Hill Climb")) throw new InvalidOperationException("Description \"" + pre.Message + "\"");
        var cr = pre.Criteria;
        string[] want = { OwnPostChallenge.Format(1, c.criteria2), OwnPostChallenge.Format(1, c.criteria1), OwnPostChallenge.Format(1, c.criteria0) };
        for (int i = 0; i < 3; i++) if (cr[i] != want[i]) throw new InvalidOperationException("Criterion " + i + " is \"" + cr[i] + "\", expected \"" + want[i] + "\"");
        if (pre.Button != "CONTINUE") throw new InvalidOperationException("The button reads \"" + pre.Button + "\"");
        if (pre.ShownCount != 1) throw new InvalidOperationException("The rules popup was shown " + pre.ShownCount + " times");
    }

    // The pause page's RESTART in the challenge (PauseMenu:OnAcceptRestart -> the RestartEvent popup -> App.RestartActivity): the core restarts
    // the activity in place (rx_session_challenge_restart): the same world, the state machine runs again from Pre, no rules popup again.
    bool PauseRestart(OriginalWorldRuntime rt, double now)
    {
        switch (pauseStep)
        {
            case 0:
                if (!KeysIdle || rt.paused || rt.OwnMenuActive) return false;
                Press(Key.Escape); pauseStep = 1; pauseStepAt = now; worldBeforePauseRestart = rt.EventIdentity;
                Debug.Log(Tag + "USER pause in the challenge (Esc)");
                return false;
            case 1:
                if (!(rt.pauseMenu && rt.pauseMenu.IsOpen)) { if (now - pauseStepAt > 20) throw new TimeoutException("Esc did not open the pause page in the challenge."); return false; }
                if (now - pauseStepAt < 1.5 || !Shot("pause")) return false;
                Debug.Log(Tag + "PAUSE_PAGE open in the challenge");
                Press(Key.DownArrow); pauseStep = 2; pauseStepAt = now; return false;
            case 2:
                if (!KeysIdle || now - pauseStepAt < .6) return false;
                Press(Key.Enter); pauseStep = 3; pauseStepAt = now; Debug.Log(Tag + "USER pause page RESTART (Down, Enter)"); return false;
            case 3:
                if (!KeysIdle || now - pauseStepAt < 1.2) return false;
                if (!Shot("pause-restart")) return false;
                Press(Key.LeftArrow); pauseStep = 4; pauseStepAt = now; return false;   // the confirmation popup focuses NO first: YES is on the left
            case 4:
                if (!KeysIdle || now - pauseStepAt < .6) return false;
                Press(Key.Enter); pauseStep = 5; pauseStepAt = now; Debug.Log(Tag + "USER restart-challenge popup YES (Left, Enter)"); return false;
            case 5:
                if (!pauseRestartLogged) { if (now - pauseStepAt > 30) throw new TimeoutException("YES did not restart the challenge in place."); return false; }
                if (rt.EventIdentity != worldBeforePauseRestart) throw new InvalidOperationException("The pause RESTART made a new world: " + worldBeforePauseRestart + " -> " + rt.EventIdentity);
                pauseStep = 6; pauseStepAt = now; return false;
            case 6:
                // the state machine ran again from Pre (the activity left InProgress) and reaches InProgress again without a rules popup
                if (rt.HasChallenge && rt.Challenge.state != 2) pauseStep = 7;
                else if (now - pauseStepAt > 30) throw new TimeoutException("The restarted challenge never left InProgress (state " + rt.Challenge.state + ").");
                return false;
            case 7:
                if (!(rt.HasChallenge && rt.Challenge.state == 2)) { if (now - pauseStepAt > 120) throw new TimeoutException("The restarted challenge never reached InProgress (state " + rt.Challenge.state + ")."); return false; }
                if (rt.preChallenge.ShownCount != 1 || rt.preChallenge.Active) throw new InvalidOperationException("The rules popup came back after the pause RESTART.");
                if (rt.pauseMenu && rt.pauseMenu.IsOpen) throw new InvalidOperationException("The pause page is still open after RESTART.");
                pauseRestartDone = true;
                Debug.Log(Tag + "PAUSE_RESTART sameWorld=1 event=" + rt.EventIdentity + " state=" + rt.Challenge.state + " popupShown=" + rt.preChallenge.ShownCount);
                return true;
        }
        return false;
    }

    // The sounds the original's Lua plays, through the race's UI sound path (OriginalHud.UiSound): PopupShow / Accept of the rules and end pages
    // and of the pause page's challenge popup.
    void CheckSounds()
    {
        string[] log; lock (OriginalHud.UiSoundLog) log = OriginalHud.UiSoundLog.ToArray();
        int shows = log.Count(l => l.Contains("play PopupShow -> ")), accepts = log.Count(l => l.Contains("play Accept -> "));
        if (shows < 3) throw new InvalidOperationException("PopupShow played " + shows + " times (rules popup, pause restart popup, end pages expected >= 3); log: " + string.Join(" | ", log.TakeLast(12)));
        if (accepts < 3) throw new InvalidOperationException("Accept played " + accepts + " times (rules popup, restart popup, RESTART, EXIT expected >= 3); log: " + string.Join(" | ", log.TakeLast(12)));
        Debug.Log(Tag + "SOUNDS popupShow=" + shows + " accept=" + accepts + " voice=" + (voiceLogged ? "CHALLENGE_APPEAR" : "none") + " last=" + string.Join(" | ", log.TakeLast(4)));
    }

    void Diagnose(string text, double now)
    {
        if (text == lastDiagnostic && now - lastLog < 10) return;
        lastDiagnostic = text; lastLog = now; Debug.Log(Tag + text);
    }

    // ---------------------------------------------------------------------------------------------- the authored route
    // Mapped keys only: throttle / brake for the speed, steering pulses toward the next waypoint (the copied bike pose).
    int[] Steer(OriginalWorldRuntime rt, double now)
    {
        if (rt.paused || rt.OwnMenuActive || (rt.pauseMenu && rt.pauseMenu.IsOpen)) return Array.Empty<int>();
        var bike = rt.Bike;
        if (bike.position == null || bike.velocity == null || bike.orientation == null || bike.frames == 0) return Array.Empty<int>();
        var p = new Vector2(bike.position[0], bike.position[2]);
        while (waypoint < Route.Length - 1 && Vector2.Distance(p, Route[waypoint]) < 40) { waypoint++; Debug.Log(Tag + "WAYPOINT " + waypoint + " at " + V(p)); }
        var target = Route[waypoint];
        var v = new Vector2(bike.velocity[0], bike.velocity[2]);
        float speed = v.magnitude;
        var raw = bike.orientation;
        var q = new Quaternion(raw[0], raw[1], raw[2], raw[3]);
        Vector3 f3 = q * Vector3.forward; var forward = new Vector2(f3.x, f3.z);
        var delta = target - p;
        float angle = forward.sqrMagnitude < .01f || delta.sqrMagnitude < .01f ? 0 : -Vector2.SignedAngle(forward, delta);
        float turn = Mathf.Abs(angle);
        float desired = turn > 90 ? 12 : turn > 50 ? 24 : turn > 25 ? 38 : 68;
        bool upright = Vector3.Dot(q * Vector3.up, Vector3.up) > .35f;
        var keys = new List<int>(2);
        // stuck (no speed for 6 s): back off with the brake for 1.5 s
        if (speed < 2) { if (stuckSince < 0) stuckSince = now; } else stuckSince = -1;
        if (stuckSince >= 0 && now - stuckSince > 6) { reverseUntil = now + 1.5; stuckSince = -1; Debug.Log(Tag + "UNSTICK at " + V(p)); }
        if (now < reverseUntil) { keys.Add(brake); keys.Add(angle > 0 ? left : right); return keys.ToArray(); }
        if (upright)
        {
            if (speed < desired - 2) keys.Add(throttle); else if (speed > desired + 4) keys.Add(brake);
            float duty = Mathf.Clamp(turn / 30, .15f, 1);
            if (turn > 2.5f && Mathf.Repeat((float)now, .2f) < .2f * duty) keys.Add(angle > 0 ? right : left);
        }
        if (now - lastLog >= 2)
        {
            lastLog = now;
            Debug.Log(Tag + "INPUT position=" + V(p) + " waypoint=" + waypoint + " target=" + V(target) + " headingError=" + angle.ToString("F1", CultureInfo.InvariantCulture)
                + " speed=" + speed.ToString("F1", CultureInfo.InvariantCulture) + " area=" + rt.Activation.active + " \"" + rt.Activation.Activity + "\" scans=" + string.Join(",", keys));
        }
        return keys.ToArray();
    }
    static string V(Vector2 p) => p.x.ToString("F1", CultureInfo.InvariantCulture) + "," + p.y.ToString("F1", CultureInfo.InvariantCulture);

    // ---------------------------------------------------------------------------------------------- UI helpers (as ArcadeReplayFlowProbe)
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
            using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = value; value.SendEvent(click); }
        }
        finally { value.UnregisterCallback(observe, TrickleDown.TrickleDown); }
        if (!delivered) throw new InvalidOperationException("Left-click was not delivered to the actual control: " + value.name);
        lastSubmit = now;
        Debug.Log(Tag + "TEST USER left-click delivered " + (string.IsNullOrEmpty(value.name) ? value.tooltip : value.name));
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
        Debug.Log(Tag + "PASS activation " + FreeRideKey + ": rode into the " + Area + " area (route waypoint " + waypoint + "), the AAA notification, the pause started "
            + ChallengeKey + " natively behind the AAA loading page, the challenge ended on the end page (medal or none), RESTART ran it again in place, EXIT went back to the Free Ride");
        Application.Quit(0);
    }
    void Fail(string message)
    {
        if (finished) return; finished = true; ClearInput(); Unwatch();
        Debug.LogError(Tag + "FAIL " + message); StartCoroutine(StopFailed());
    }
    IEnumerator StopFailed() { if (!string.IsNullOrEmpty(shotDirectory)) { yield return Capture("failure"); yield return new WaitForSecondsRealtime(.3f); } Application.Quit(1); }
    void OnDestroy() { Application.logMessageReceived -= ObserveLog; ClearInput(); Unwatch(); }
}
