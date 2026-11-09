// TEST ONLY (run.py --hudrest <HillClimb|Trailblazer|TargetPractice>): the ORIGINAL HUD of the open core's Free Ride and a challenge, ridden for real. The
// open front end's FREE RIDE at FR_Alpine (the Race & Ride challenges Free Ride: OwnActivationArea, InCareerMode) with HUD STYLE ORIGINAL, ridden with
// ordinary mapped keys along an authored route to the level's area of the chosen challenge; screenshots of the compass with the challenge icons (the
// areas' bearings and fades), the H key hiding the HUD, MODERN replacing it; then the pause key starts the challenge (AAA loading, the rules popup
// accepted with Enter) and the challenge's own layer is watched while it is ridden (the Trailblazer's finish and distance, the Target Practice's
// target icons and Target Hit count, the Hill Climb's height; the event timer on each); the pause page hides the HUD. Observation only: it presses
// keys and sets the HUD STYLE preference of its own sandbox; it writes no game or native state.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public sealed class HudRestRideProbe : MonoBehaviour
{
    const string Tag = "[hudrest-ride-probe] ";
    const string FreeRideKey = "FreeRide/FR_Alpine";
    static readonly string Activity = Environment.GetEnvironmentVariable("REFLEX_HUDREST_PROBE") ?? "Trailblazer";
    static string ChallengeKey => Activity + "/FR_Alpine";
    // The authored routes from the Free Ride's start (1738, -1652) to the level's StartActivity triggers (core/tools/activation_native_test.c plan):
    // x, z waypoints over the collision terrain, passed within 40 units.
    static readonly Dictionary<string, Vector2[]> Routes = new Dictionary<string, Vector2[]>
    {
        ["HillClimb"] = new[]
        {
            new Vector2(1745, -1521), new Vector2(1745, -1393), new Vector2(1729, -1265), new Vector2(1713, -1137), new Vector2(1697, -1009),
            new Vector2(1697, -881), new Vector2(1697, -753), new Vector2(1681, -625), new Vector2(1633, -497), new Vector2(1633, -369),
            new Vector2(1617, -241), new Vector2(1553, -113), new Vector2(1553, 15), new Vector2(1553, 143), new Vector2(1553, 271),
            new Vector2(1457, 399), new Vector2(1329, 479), new Vector2(1201, 575), new Vector2(1073, 623), new Vector2(945, 639),
            new Vector2(817, 687), new Vector2(689, 799), new Vector2(561, 863), new Vector2(433, 927), new Vector2(390.4f, 953.9f),
        },
        ["Trailblazer"] = new[] { new Vector2(1617, -1601), new Vector2(1489, -1569), new Vector2(1361, -1569), new Vector2(1233, -1569), new Vector2(1105, -1569), new Vector2(977, -1553), new Vector2(947.4f, -1547.2f) },
        ["TargetPractice"] = new[]
        {
            new Vector2(1617, -1521), new Vector2(1505, -1393), new Vector2(1441, -1265), new Vector2(1313, -1137), new Vector2(1233, -1009), new Vector2(1233, -881),
            new Vector2(1185, -753), new Vector2(1169, -625), new Vector2(1121, -497), new Vector2(1089, -369), new Vector2(1073, -241), new Vector2(1056.6f, -222.8f),
        },
    };
    static Vector2[] Route => Routes[Activity];
    static readonly FieldInfo Simulation = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.Instance | BindingFlags.NonPublic);
    readonly HashSet<string> shots = new HashSet<string>();
    readonly Queue<Key> keyQueue = new Queue<Key>();
    VisualElement candidate;
    Action<MeshGenerationContext> draw;
    string shotDirectory, lastDiagnostic;
    double started, candidateSince, lastSubmit, lastLog, browserOpenedAt, stuckSince = -1, reverseUntil, stepAt, challengeRunningSince = -1, ridingSince = -1, stateAt;
    int candidateFrame, paintedFrame = -1, exceptions, waypoint, keyPhase;
    int throttle, brake, left, right;
    bool opened, selected, launched, areaLogged, pausePressed, challengeSeen, finished, preSeen, preAccepted, challengeRunningLogged;
    int phase; double preSeenAt = -1;
    Key pendingKey; bool keyDown;
    Vector2? aim;   // a target the challenge steers to (the compass' nearest target / the finish), else the route

    public static void Boot()
    {
        if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_HUDREST_PROBE")) || FindAnyObjectByType<HudRestRideProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("HUD rest ride TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<HudRestRideProbe>(); Application.runInBackground = true;
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
            ReflexSettings.HudStyle = HudStyle.Original; ReflexSettings.HudHidden = false;   // HUD STYLE ORIGINAL (the sandbox's own preferences)
            Debug.Log(Tag + "BEGIN " + FreeRideKey + " -> the " + Activity + " area -> " + ChallengeKey + "; HUD STYLE ORIGINAL; keys throttle=" + throttle + " brake=" + brake + " left=" + left + " right=" + right);
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


    // ---------------------------------------------------------------------------------------------- the HUD's state (what is on screen)
    static ReflexModernHud Hud => FindAnyObjectByType<ReflexModernHud>();
    static OriginalFreeRideHud Orig => Hud?.OriginalFreeRide;
    static bool OriginalUp => Orig != null && Orig.Canvas.enabled;
    string Describe()
    {
        var o = Orig; if (o == null) return "no original HUD";
        var c = o.Compass;
        string icons = "";
        if (c != null)
        {
            var up = new List<string>();
            for (int i = 0; i < 10; i++) if (c.challengeUp[i]) up.Add("area" + i + "@" + c.challenge[i].rect.anchoredPosition.x.ToString("F0", CultureInfo.InvariantCulture) + "/a" + c.challenge[i].graphic.color.a.ToString("F2", CultureInfo.InvariantCulture));
            for (int i = 0; i < 16; i++) if (c.targetUp[i]) up.Add("target" + i + "@" + c.target[i].rect.anchoredPosition.x.ToString("F0", CultureInfo.InvariantCulture));
            if (c.finishUp) up.Add("finish@" + c.finish.rect.anchoredPosition.x.ToString("F0", CultureInfo.InvariantCulture) + " '" + c.labelText + "'");
            icons = " heading " + c.heading.ToString("F1", CultureInfo.InvariantCulture) + " icons [" + string.Join(" ", up) + "]";
        }
        return "page " + o.Current + " canvas " + o.Canvas.enabled + icons + (o.Shown ? " counter '" + o.Text + "'" : "");
    }

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 3600) throw new TimeoutException("HUD rest ride wall budget exceeded.");
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
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var driver = rt == null ? null : Simulation?.GetValue(rt) as IReflexSimDriver;
        var open = driver?.Open;
        if (open != null && !string.IsNullOrEmpty(open.Failure)) throw new InvalidOperationException("Worker: " + open.Failure);
        if (rt != null && rt.FailureMessage != null) throw new InvalidOperationException("World: " + rt.FailureMessage);
        if (rt == null || !rt.Running) return;
        var area = rt.GetComponent<OwnActivationArea>();
        if (area == null) throw new InvalidOperationException("The world " + rt.level + " has no activation areas (OwnActivationArea).");

        // ---- the Free Ride: the compass and the challenge icons while riding to the area
        if (!pausePressed)
        {
            if (area.IsChallenge || area.Event.key != FreeRideKey) throw new InvalidOperationException("Unexpected world " + area.Event.key);
            var a = rt.Activation;
            bool inArea = rt.HasActivation && a.active != 0 && a.Activity == Activity;
            if (!inArea)
            {
                rt.testHeldKeys = Steer(rt, now);
                if (rt.Race.state == 2 || rt.Bike.speed > 1) { if (ridingSince < 0) ridingSince = now; }
                if (ridingSince < 0) return;
                double riding = now - ridingSince;
                // 1: the Free Ride's compass with its icons, 2: a while later, 3: H hides the HUD, 4: H shows it, 5: MODERN, 6: ORIGINAL again
                switch (phase)
                {
                    case 0: if (riding > 1.5 && Shot("hudrest-1-freeride")) { Debug.Log(Tag + "FREE_RIDE_HUD " + Describe()); if (!OriginalUp || Orig.Current != OriginalFreeRideHud.Page.FreeRide) throw new InvalidOperationException("The ORIGINAL Free Ride page is not up: " + Describe()); phase = 1; stateAt = now; } break;
                    case 1: if (riding > 5 && Shot("hudrest-2-riding")) { Debug.Log(Tag + "FREE_RIDE_HUD riding " + Describe()); phase = 2; stateAt = now; } break;
                    case 2: if (now - stateAt > 0.6 && KeysIdle) { Press(Key.H); phase = 3; stateAt = now; Debug.Log(Tag + "USER H"); } break;
                    case 3:
                        if (now - stateAt > 1.2 && Shot("hudrest-3-hidden")) { Debug.Log(Tag + "HUD_HIDDEN_BY_H originalUp=" + OriginalUp + " hidden=" + ReflexSettings.HudHidden); if (OriginalUp || !ReflexSettings.HudHidden) throw new InvalidOperationException("H did not hide the ORIGINAL HUD: " + Describe()); phase = 4; stateAt = now; }
                        break;
                    case 4: if (KeysIdle) { Press(Key.H); phase = 5; stateAt = now; Debug.Log(Tag + "USER H again"); } break;
                    case 5:
                        if (now - stateAt > 1.2 && OriginalUp) { Debug.Log(Tag + "HUD_SHOWN_BY_H " + Describe()); ReflexSettings.HudStyle = HudStyle.Modern; phase = 6; stateAt = now; }
                        break;
                    case 6:
                        if (now - stateAt > 1.5 && Shot("hudrest-4-modern")) { Debug.Log(Tag + "MODERN originalUp=" + OriginalUp); if (OriginalUp) throw new InvalidOperationException("MODERN left the ORIGINAL HUD up: " + Describe()); ReflexSettings.HudStyle = HudStyle.Original; phase = 7; stateAt = now; }
                        break;
                    case 7: if (now - stateAt > 1.0 && OriginalUp) { Debug.Log(Tag + "ORIGINAL_AGAIN " + Describe()); phase = 8; } break;
                }
                return;
            }
            rt.testHeldKeys = Array.Empty<int>();
            if (!areaLogged) { areaLogged = true; Debug.Log(Tag + "AREA_ENTERED " + a.Activity + " track=" + a.track_id + " waypoint=" + waypoint); Debug.Log(Tag + "AREA_HUD " + Describe()); }
            if (!area.NotificationShown) return;
            if (phase < 8) { Debug.Log(Tag + "NOTE the area was reached before the HUD journey ended (phase " + phase + ")"); phase = 8; }
            if (!KeysIdle || !Shot("hudrest-5-area")) return;
            Press(Key.Escape); pausePressed = true; stepAt = now;
            Debug.Log(Tag + "USER pause in the area");
            return;
        }

        // ---- the challenge
        if (!challengeSeen)
        {
            if (!area.IsChallenge) { if (now - stepAt > 60) throw new TimeoutException("The pause in the area started no challenge (still " + area.Event.key + ")."); return; }
            if (area.Event.key != ChallengeKey) throw new InvalidOperationException("The area started " + area.Event.key);
            challengeSeen = true;
            Debug.Log(Tag + "CHALLENGE_STARTED " + area.Event.key + " event=" + rt.EventIdentity);
        }
        if (!preSeen)
        {
            var pre = rt.preChallenge;
            if (pre == null) throw new InvalidOperationException("The challenge world has no OwnPreChallenge.");
            if (!pre.Active || !pre.Rendered) { if (now - stepAt > 300) throw new TimeoutException("The challenge never showed its rules popup."); return; }
            if (preSeenAt < 0) preSeenAt = now;
            if (now - preSeenAt < 1.5 || !Shot("hudrest-6-rules")) return;
            preSeen = true; Debug.Log(Tag + "PRE_POPUP HUD " + Describe());
        }
        if (!preAccepted)
        {
            if (!KeysIdle) return;
            Press(Key.Enter); preAccepted = true; stepAt = now; Debug.Log(Tag + "USER ACCEPT pre popup (Enter)");
            return;
        }
        if (!challengeRunningLogged && rt.preChallenge.Active) { if (now - stepAt > 20) throw new TimeoutException("Enter did not accept the rules popup."); return; }
        var race = rt.Race;
        if (!challengeRunningLogged)
        {
            if (!rt.HasChallenge || rt.Challenge.state != 2) { Diagnose("challenge state " + (rt.HasChallenge ? rt.Challenge.state.ToString() : "none"), now); return; }
            if (challengeRunningSince < 0) challengeRunningSince = now;
            if (now - challengeRunningSince < 0.6 || !Shot("hudrest-7-challenge-start")) return;
            challengeRunningLogged = true; phase = 20; stateAt = now;
            Debug.Log(Tag + "CHALLENGE_RUNNING kind=" + rt.Challenge.kind + " state=" + rt.Challenge.state);
            Debug.Log(Tag + "CHALLENGE_HUD " + Describe());
            var o = Orig; if (o == null || !OriginalUp) throw new InvalidOperationException("The challenge shows no ORIGINAL HUD: " + Describe());
            var want = Activity == "HillClimb" ? OriginalFreeRideHud.Page.HillClimb : Activity == "Trailblazer" ? OriginalFreeRideHud.Page.Trailblazer : OriginalFreeRideHud.Page.TargetPractice;
            if (o.Current != want) throw new InvalidOperationException("The challenge page is " + o.Current + ", expected " + want);
            return;
        }
        // the challenge ridden: the throttle (HillClimb: straight on), the compass' targets / the finish steered to
        if (rt.Challenge.state == 2 && !rt.paused && !rt.OwnMenuActive && !(rt.pauseMenu && rt.pauseMenu.IsOpen))
        {
            aim = null;
            if (rt.HasCompass && Activity != "HillClimb")
            {
                var st = rt.Compass; var p = new Vector2(rt.Bike.position[0], rt.Bike.position[2]); float best = float.MaxValue;
                for (int i = 0; i < (int)st.count; i++)
                {
                    int ty = st.Type(i); if (ty == 2 && st.Aux(i) != 0) continue; if (ty != 2 && ty != 3) continue;
                    var m = st.Position(i); var v = new Vector2(m.x, m.z); float d = (v - p).sqrMagnitude;
                    if (d < best) { best = d; aim = v; }
                }
            }
            rt.testHeldKeys = aim.HasValue ? Steer(rt, now, aim.Value) : new[] { throttle };
        }
        double t = now - stateAt;
        switch (phase)
        {
            case 20: if (t > 4 && Shot("hudrest-8-challenge-riding")) { Debug.Log(Tag + "CHALLENGE_HUD riding " + Describe() + " value " + rt.Challenge.value.ToString("F1", CultureInfo.InvariantCulture)); phase = 21; stateAt = now; } break;
            case 21: if (t > 6 && Shot("hudrest-9-challenge-later")) { Debug.Log(Tag + "CHALLENGE_HUD later " + Describe() + " value " + rt.Challenge.value.ToString("F1", CultureInfo.InvariantCulture)); phase = 22; stateAt = now; } break;
            case 22: if (t > 0.5 && KeysIdle && rt.HasChallenge && rt.Challenge.state == 2) { rt.testHeldKeys = Array.Empty<int>(); Press(Key.Escape); phase = 23; stateAt = now; Debug.Log(Tag + "USER pause in the challenge (Esc)"); } break;
            case 23:
                if (!(rt.OwnMenuActive || (rt.pauseMenu && rt.pauseMenu.IsOpen))) { if (t > 20) throw new TimeoutException("Esc did not open the pause page in the challenge."); break; }
                if (t > 1.5 && Shot("hudrest-10-pause"))
                {
                    Debug.Log(Tag + "PAUSE_HUD " + Describe() + " ownMenu=" + rt.OwnMenuActive);
                    if (OriginalUp) throw new InvalidOperationException("The pause page left the ORIGINAL HUD up: " + Describe());
                    phase = 24; finished = true; ClearInput(); StartCoroutine(Succeed());
                }
                break;
        }
        Diagnose("challenge " + Describe(), now);
    }

    void Diagnose(string text, double now)
    {
        if (text == lastDiagnostic && now - lastLog < 10) return;
        lastDiagnostic = text; lastLog = now; Debug.Log(Tag + text);
    }


    // ---------------------------------------------------------------------------------------------- the authored route
    // Mapped keys only: throttle / brake for the speed, steering pulses toward the next waypoint (the copied bike pose).
    int[] Steer(OriginalWorldRuntime rt, double now, Vector2? overrideTarget = null)
    {
        if (rt.paused || rt.OwnMenuActive || (rt.pauseMenu && rt.pauseMenu.IsOpen)) return Array.Empty<int>();
        var bike = rt.Bike;
        if (bike.position == null || bike.velocity == null || bike.orientation == null || bike.frames == 0) return Array.Empty<int>();
        var p = new Vector2(bike.position[0], bike.position[2]);
        while (!overrideTarget.HasValue && waypoint < Route.Length - 1 && Vector2.Distance(p, Route[waypoint]) < 40) { waypoint++; Debug.Log(Tag + "WAYPOINT " + waypoint + " at " + V(p)); }
        var target = overrideTarget ?? Route[waypoint];
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
                + " speed=" + speed.ToString("F1", CultureInfo.InvariantCulture) + " scans=" + string.Join(",", keys));
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
        Debug.Log(Tag + "PASS hudrest " + Activity + ": the ORIGINAL Free Ride page (compass, challenge icons), H hid it, MODERN replaced it, the " + ChallengeKey + " layer ran in the challenge, the pause page hid it");
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
