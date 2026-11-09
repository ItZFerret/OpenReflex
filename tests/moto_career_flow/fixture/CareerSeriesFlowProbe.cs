// Test-only driver: one MOTOCAREER series of any Single-class format (REFLEX_CAREER_SERIES=<id>: SingleEvent, Eliminator, Faceoff, Outrun,
// Overtake, SectionRace, of every activity) through the open front end's own pages. Like MotoCareerFlowProbe it only clicks painted
// controls and presses mapped keys: the Garage selection of the series' vehicle (REFLEX_MOTO_VEHICLE), the career list, the rules popup,
// the race ridden along the copied race line (CareerSeriesPilot), the results' CONTINUE, the rewards, the durable save and RETURN TO
// CAREER. A Heat series (41-46) adds its own pages: the practice choice (REFLEX_HEAT_PRACTICE=1: PRACTICE, else SKIP), the gate page of
// every round, each heat / LCQ's results with CONTINUE and the next round, and the AAA loading page while the next round's riders are
// re-created (one screenshot per round's results). It writes no race, finish, score or acknowledgement state. PASS = the series completed with two durable checkpoints, the
// profile's wallet moved by exactly the native reward and the return reached the catalog again. REFLEX_CAREER_RESTART=1 adds the pause page's
// RESTART once, a few seconds into the race (the pause key's page, then its RESTART row and the confirmation's RESTART): the activity must
// restart in place (the same world, the career still active, the race back in Pre and then racing again) and the series must still finish
// with exactly one result (the generation / wallet checks above).
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public sealed class CareerSeriesFlowProbe : MonoBehaviour
{
    const string Tag = "[career-series-flow-probe] ";
    static readonly FieldInfo Simulation = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.Instance | BindingFlags.NonPublic);
    readonly HashSet<string> shots = new HashSet<string>();
    PlayerSnapshotLease initial;
    PlayerSnapshotDocument rawInitial;
    VisualElement candidate;
    Action<MeshGenerationContext> draw;
    string shotDirectory, lastDiagnostic, earnedDigest;
    double started, candidateSince, lastSubmit, lastLog, browserOpenedAt, garageClickSince;
    int candidateFrame, paintedFrame = -1, exceptions, series, expectedVehicle;
    bool opened, selected, rulesAccepted, continued, returned, finished, durable, raced, finishedRace;
    bool garageOpened, garageVehicleTab, garageSelected, garageDone, garageTabClickSent, garageVehicleClickSent;
    bool outcomeLogged; uint rewardBase, rewardBonus; bool rewardObserved; int finalRank = -1; string finalStatus;
    CareerSeriesPilot pilot;
    // REFLEX_CAREER_RESTART: 0 wait for the race, 1 pause page open, 2 RESTART asked, 3 confirmed, 4 back in Pre, 5 racing again
    bool startConfirmed, restartOpenedLogged;
    int restartStep; long restartGeneration; readonly bool restartWanted = Environment.GetEnvironmentVariable("REFLEX_CAREER_RESTART") == "1";
    // Heat: the rounds whose results were shown, the gates picked, the AAA loading pages seen
    int heatResultsShown, heatGates, heatLoadingPages, ridingShotFrame = int.MaxValue - 10; uint heatLastResultsPhase; bool heatPracticeChosen, heatLoadingVisible;

    public static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_CAREER_SERIES_FLOW_PROBE") != "1" || FindAnyObjectByType<CareerSeriesFlowProbe>() != null) return;
        if (!Reflex.Port.Platform.ReflexSmoke.GuestRan || !ReflexProfile.Disabled) throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag + "REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Career Series Flow TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<CareerSeriesFlowProbe>(); Application.runInBackground = true;
    }
    void Awake()
    {
        started = Time.realtimeSinceStartupAsDouble;
        Application.logMessageReceived += ObserveLog;
        try
        {
            shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (string.IsNullOrWhiteSpace(shotDirectory)) throw new InvalidOperationException("Screenshot directory required.");
            Directory.CreateDirectory(shotDirectory);
            series = int.Parse(Environment.GetEnvironmentVariable("REFLEX_CAREER_SERIES"));
            expectedVehicle = int.Parse(Environment.GetEnvironmentVariable("REFLEX_MOTO_VEHICLE"));
            Debug.Log(Tag + "BEGIN series " + series + " vehicle " + expectedVehicle);
        }
        catch (Exception e) { Fail(e.ToString()); }
    }
    void ObserveLog(string message, string stack, LogType type) { if (type == LogType.Exception) exceptions++; }
    void ClearInput() { foreach (var rt in FindObjectsByType<OriginalWorldRuntime>(FindObjectsInactive.Include, FindObjectsSortMode.None)) rt.testHeldKeys = Array.Empty<int>(); }
    void Update() { if (finished) return; try { Tick(); } catch (Exception e) { Fail(e.ToString()); } }

    bool PrepareUnlockedProfile()
    {
        if (initial != null) return true;
        if (OpenShell.Instance == null || Time.realtimeSinceStartupAsDouble - started < 2) return false;
        pilot = new CareerSeriesPilot(Reflex.Port.FrontEnd.CoreRemapping.Read(false));
        var store = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory);
        earnedDigest = store.Read(PlayerProfileMode.Career).Digest;
        rawInitial = PlayerSnapshotDocument.Decode(File.ReadAllBytes(Path.Combine(CoreGarageProfile.SaveDirectory, "selected-moto-player24.bin")));
        store.CreateIfAbsent(PlayerProfileMode.Unlocked, rawInitial);
        CoreGarageProfile.Close();
        if (!CoreGarageProfile.Open() || !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
        initial = store.Read(PlayerProfileMode.Unlocked);
        return true;
    }

    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 3000) throw new TimeoutException("Career series wall budget exceeded.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s): " + exceptions);
        if (!PrepareUnlockedProfile()) return;
        if (!garageDone) { TickGarage(now); return; }
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var open = rt == null ? null : (Simulation?.GetValue(rt) as IReflexSimDriver)?.Open;
        var snapshot = open?.Career?.Snapshot;
        if (open != null && !string.IsNullOrEmpty(open.Failure)) throw new InvalidOperationException("Worker: " + open.Failure);
        if (snapshot != null)
        {
            var s = snapshot.Status;
            if (snapshot.Completion.HasValue && snapshot.Completion.Value.complete_event_state == 2 && !rewardObserved)
            { rewardObserved = true; rewardBase = s.reward_base; rewardBonus = s.reward_bonus; Debug.Log(Tag + "ACTUAL_REWARD base=" + rewardBase + " bonus=" + rewardBonus + " valid=" + s.reward_valid); }
            if (snapshot.FinalResults) { finalRank = s.local_rank; finalStatus = s.EventStatus; }
            if (snapshot.Completion.HasValue)
            {
                var c = snapshot.Completion.Value;
                durable = c.durable == 1 && c.in_memory_settled == 1 && c.records_state == 2 && c.complete_series_state == 2 && c.checkpoint_one_id != 0 && c.checkpoint_two_id != 0
                          && s.records_finalized == 1 && s.pending_requests == 0 && !snapshot.Request.HasValue && s.checkpoint_error == 0 && string.IsNullOrEmpty(snapshot.CheckpointFailure);
            }
        }
        // Heat "Failed" (the local rider did not qualify from the heat / LCQ): the original goes back to the events page without
        // CompleteSeriesEvent, so no reward is paid; the durable save still happens (two checkpoints, wallet unchanged).
        if (snapshot != null && !rewardObserved && durable && snapshot.Heat != null && finalStatus == "Failed")
        { rewardObserved = true; rewardBase = 0; rewardBonus = 0; Debug.Log(Tag + "ACTUAL_REWARD base=0 bonus=0 (Heat Failed: no CompleteSeriesEvent)"); }
        if (rt != null)
        {
            var race = rt.Race;
            bool authorized = snapshot != null && !snapshot.Request.HasValue && snapshot.Status.pending_requests == 0 && rt.OwnedCareerActive;
            pilot.Attach(rt);
            rt.testHeldKeys = pilot.Inputs(rt, authorized, now);
            if (race.valid != 0 && race.state == 2 && race.race_time > 3 && !raced) { raced = true; ridingShotFrame = Time.frameCount; Debug.Log(Tag + "RACE_RUNNING laps=" + race.num_laps + " riders=" + race.riders + " event=" + rt.EventIdentity); Shot("riding"); }
            if (raced && race.valid != 0 && race.state >= 3 && !finishedRace) { finishedRace = true; Debug.Log(Tag + "NATURAL_FINISH state=" + race.state + " laps=" + race.laps_completed + "/" + race.num_laps + " place=" + race.place + " time=" + race.race_time); }
            if (restartWanted && restartStep < 5) TickRestart(rt, now);
            var heatLive = snapshot?.Heat;
            // one capture per frame (ScreenCapture keeps the last request of a frame): the round's shot comes after the first "riding"
            if (heatLive != null && race.valid != 0 && race.state == 2 && race.race_time > 4 && Time.frameCount > ridingShotFrame + 5)
            {
                string round = HeatWorkerSnapshot.RoundName(heatLive.Phase).ToLowerInvariant().Replace(' ', '-');
                if (!shots.Contains("riding-" + round)) { Debug.Log(Tag + "HEAT_RACE_RUNNING round=" + round + " riders=" + race.riders + " place=" + race.place); Shot("riding-" + round); }
            }
        }
        Diagnostic(rt, snapshot, now);
        if (returned) { TickReturned(rt, now); return; }
        if (!opened)
        {
            if (OpenShell.Instance == null) return;
            OpenShell.Instance.ShowOwnedCareer(new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, Return, Cancel));
            opened = true; browserOpenedAt = now; Debug.Log(Tag + "BROWSER_OPENED"); return;
        }
        if (!selected)
        {
            var button = Find("career-series-" + series);
            if (button == null) { if (now - browserOpenedAt > 180) throw new InvalidOperationException("Career catalog did not list series " + series + ": " + VisibleActions()); return; }
            if (!button.enabledInHierarchy) throw new InvalidOperationException("Series " + series + " is not selectable (locked, unsupported or ineligible): " + VisibleActions());
            if (!Drawn(button, now) || !Shot("browser", button)) return;
            if (Submit(button, now)) { selected = true; Debug.Log(Tag + "USER selected series " + series); }
            return;
        }
        if (TickHeat(snapshot, now)) return;
        // the StartRacePopup (START EVENT / YES / NO) every format but Moto and Heat shows before the event starts
        var startYes = Find("popup-start-yes");
        if (startYes != null && !startConfirmed)
        {
            if (!Shot("start-popup")) return;
            if (Submit(startYes, now)) { startConfirmed = true; Debug.Log(Tag + "USER confirmed START EVENT"); }
            return;
        }
        var accept = Find("career-rules-accept");
        if (accept != null) { if (!Shot("rules")) return; if (Submit(accept, now)) { rulesAccepted = true; Debug.Log(Tag + "USER accepted the rules"); } return; }
        var next = Find("career-continue");
        if (next != null && !continued)
        {
            if (!Shot("results")) return;
            if (Submit(next, now)) { continued = true; Debug.Log(Tag + "USER Continue results rank=" + finalRank + " status=" + finalStatus); }
            return;
        }
        if (snapshot != null && snapshot.Status.reward_valid != 0) Shot("rewards");
        var back = Find("career-return");
        if (back != null && durable)
        {
            if (!Shot("saved")) return;
            if (Submit(back, now)) { returned = true; ClearInput(); Debug.Log(Tag + "USER Return -> FrontEnd"); }
        }
    }

    void TickRestart(OriginalWorldRuntime rt, double now)
    {
        var race = rt.Race; var pause = rt.racePause;
        switch (restartStep)
        {
            case 0:
                if (raced && race.valid != 0 && race.state == 2 && race.race_time > 6 && pause && pause.CanOpen)
                { restartGeneration = rt.WorldGeneration; pause.Open(); restartStep = 1; }
                break;
            case 1:
            {
                if (!pause.IsOpen || pause.Page != "main") break;
                var rows = pause.RowNames.ToList();
                if (!restartOpenedLogged) { restartOpenedLogged = true; Debug.Log(Tag + "PAUSE_RESTART_OPENED rows=" + string.Join(",", rows) + " time=" + race.race_time); }
                if (!rows.Contains("pause-restart")) throw new InvalidOperationException("The career event's pause page has no RESTART: " + string.Join(",", rows));
                var row = Find("pause-restart");
                if (row != null && Submit(row, now)) restartStep = 2;
                break;
            }
            case 2:
            {
                if (pause.Page != "restart") break;
                var yes = Find("pause-confirm");
                if (yes != null && Submit(yes, now)) { restartStep = 3; Debug.Log(Tag + "PAUSE_RESTART_CONFIRMED"); }
                break;
            }
            case 3:
                if (!pause.IsOpen && race.valid != 0 && race.state <= 1)
                {
                    if (rt.WorldGeneration != restartGeneration || !rt.OwnedCareerActive) throw new InvalidOperationException("The career RESTART made a new world or left the career.");
                    Debug.Log(Tag + "PAUSE_RESTART_PRE state=" + race.state + " time=" + race.race_time + " laps=" + race.laps_completed + " world=" + rt.WorldGeneration);
                    restartStep = 4;
                }
                break;
            case 4:
                if (race.valid != 0 && race.state == 2 && race.race_time > 1)
                { Debug.Log(Tag + "PAUSE_RESTART_RACING time=" + race.race_time); restartStep = 5; Shot("restarted"); }
                break;
        }
    }

    // The Heat pages: true when a Heat control was handled (or is waiting for its screenshot) this frame.
    string gateBrowseRound; int gateBrowseStep; double gateBrowseAt; bool gateDumped;
    // TEST: which UI layers are up over the gate page (the dotted stage of a page shows through the world)
    static void DumpStages()
    {
        foreach (var c in FindObjectsByType<Canvas>(FindObjectsInactive.Exclude, FindObjectsSortMode.None)) Debug.Log("[gate-dump] canvas " + c.name + " mode " + c.renderMode + " order " + c.sortingOrder + " images " + c.GetComponentsInChildren<UnityEngine.UI.Graphic>().Count(g => g.enabled && g.canvasRenderer.GetAlpha() > 0.01f));
        foreach (var cam in Camera.allCameras) Debug.Log("[gate-dump] camera " + cam.name + " depth " + cam.depth + " enabled " + cam.enabled);
        foreach (var doc in FindObjectsByType<UIDocument>(FindObjectsInactive.Include, FindObjectsSortMode.None))
        {
            var root = doc.rootVisualElement; if (root == null) continue;
            int stages = root.Query<VisualElement>(className: "oc-stage").ToList().Count, dots = root.Query<VisualElement>(className: "oc-stage-dots").ToList().Count;
            foreach (var e in root.Query<VisualElement>().ToList())
            {
                var bg = e.resolvedStyle.backgroundImage; if (bg.texture == null && bg.sprite == null && bg.renderTexture == null && bg.vectorImage == null) continue;
                bool shown = true; float op = 1; for (var q = e; q != null; q = q.parent) { if (q.resolvedStyle.display == DisplayStyle.None || q.resolvedStyle.visibility != Visibility.Visible) shown = false; op *= q.resolvedStyle.opacity; }
                var wb = e.worldBound; if (!shown || wb.width * wb.height < 200000) continue;
                Debug.Log("[gate-dump]   big background: " + doc.gameObject.name + " / " + e.name + " [" + string.Join(" ", e.GetClasses()) + "] " + wb.width + "x" + wb.height + " opacity " + op);
            }
            Debug.Log("[gate-dump] doc " + doc.gameObject.name + " order " + doc.sortingOrder + " display " + root.resolvedStyle.display + " children " + root.childCount + " stages " + stages + " dots " + dots);
        }
    }
    bool TickHeat(CareerSnapshot snapshot, double now)
    {
        bool practiceWanted = Environment.GetEnvironmentVariable("REFLEX_HEAT_PRACTICE") == "1";
        var choice = Find(practiceWanted ? "career-heat-practice" : "career-heat-skip-practice");
        if (choice != null && !heatPracticeChosen)
        {
            if (!Drawn(choice, now) || !Shot("practice-choice")) return true;
            if (Submit(choice, now)) { heatPracticeChosen = true; Debug.Log(Tag + "USER heat practice choice " + (practiceWanted ? "PRACTICE" : "SKIP")); }
            return true;
        }
        var loading = OwnAaaLoading.Current;
        bool loadingUp = loading != null && loading.Held && loading.Built;
        if (loadingUp && !heatLoadingVisible) { heatLoadingPages++; Debug.Log(Tag + "HEAT_AAA_LOADING page=" + heatLoadingPages + " line=" + loading.Line); Shot("aaa-" + heatLoadingPages); }
        heatLoadingVisible = loadingUp;
        var heat = snapshot?.Heat;
        if (heat == null) return false;
        foreach (var gate in heat.Gates)
        {
            var pick = Find("career-heat-gate-" + gate);
            if (pick == null) continue;
            string round = HeatWorkerSnapshot.RoundName(heat.NextPhase == 3 || heat.NextPhase == 4 || heat.NextPhase == 5 ? heat.NextPhase : heat.Phase).ToLowerInvariant().Replace(' ', '-');
            if (!Drawn(pick, now) || !Shot("gate-" + round)) return true;
            if (!gateDumped) { gateDumped = true; DumpStages(); }
            // the page's 3D camera follows the focused gate (OwnGateView): look at two other gates before choosing
            var list = heat.Gates;
            int[] targets = list.Count < 2 || Environment.GetEnvironmentVariable("REFLEX_GATE_BROWSE") == "0" ? new int[0] : new[] { 1, list.Count - 1 };
            if (gateBrowseRound != round) { gateBrowseRound = round; gateBrowseStep = 0; gateBrowseAt = 0; }
            if (gateBrowseStep < targets.Length)
            {
                if (gateBrowseAt == 0) { Find("career-heat-gate-" + list[targets[gateBrowseStep]])?.Focus(); gateBrowseAt = now; return true; }
                if (now - gateBrowseAt < 1.4) return true;
                if (!Shot("gate-" + round + "-view" + (gateBrowseStep + 1))) return true;
                gateBrowseStep++; gateBrowseAt = 0; return true;
            }
            if (Submit(pick, now)) { heatGates++; Debug.Log(Tag + "USER heat gate " + (gate + 1) + " round=" + round); Debug.Log(Tag + "HEAT_GATE_SELECTED round=" + round + " gate=" + (gate + 1)); }
            return true;
        }
        var gates = Find("career-heat-gates");
        if (gates != null) { if (Submit(gates, now)) Debug.Log(Tag + "USER heat choose start gate"); return true; }
        var roundContinue = Find("career-heat-continue");
        if (roundContinue != null)
        {
            string round = HeatWorkerSnapshot.RoundName(heat.Phase).ToLowerInvariant().Replace(' ', '-');
            if (!Drawn(roundContinue, now) || !Shot(round + "-results")) return true;
            if (Submit(roundContinue, now))
            {
                if (heatLastResultsPhase != heat.Phase) { heatLastResultsPhase = heat.Phase; heatResultsShown++; }
                var local = heat.Results.FirstOrDefault(r => r.Local);
                Debug.Log(Tag + "HEAT_ROUND_RESULTS round=" + round + " riders=" + heat.Results.Count + " local=" + (local == null ? "-" : (local.Rank + 1) + "->" + local.Outcome) + " USER Continue round");
            }
            return true;
        }
        var nextRound = Find("career-heat-next");
        if (nextRound != null) { if (Submit(nextRound, now)) Debug.Log(Tag + "USER heat next round " + HeatWorkerSnapshot.RoundName(heat.NextPhase)); return true; }
        return false;
    }

    void TickGarage(double now)
    {
        if (!garageOpened) { ReflexProfile.Disabled = false; OpenShell.Instance.ShowGarage(); garageOpened = true; return; }
        if (!garageVehicleTab)
        {
            // GARAGE > VEHICLE > MY RIDES > ALL VEHICLES, a click each (the drill-down, w-garageflow)
            if (Find("garage-vehicle-" + expectedVehicle) != null) { garageVehicleTab = true; Debug.Log(Tag + "GARAGE_TAB_CONSUMED"); return; }
            if (garageTabClickSent && now - garageClickSince > 30) throw new TimeoutException("The Garage vehicle list did not open.");
            var step = Find("garage-class-all") ?? Find("garage-my-rides") ?? Find("garage-h-vehicle");
            if (step != null && Submit(step, now)) { if (!garageTabClickSent) garageClickSince = now; garageTabClickSent = true; }
            return;
        }
        if (!garageSelected)
        {
            var vehicle = Find("garage-vehicle-" + expectedVehicle);
            if (garageVehicleClickSent)
            {
                if (vehicle != null && vehicle.Query<Label>().ToList().Any(x => x.text.Contains("EQUIPPED"))) { garageSelected = true; Debug.Log(Tag + "GARAGE_VEHICLE_CONSUMED vehicle=" + expectedVehicle); }
                else if (now - garageClickSince > 30) throw new TimeoutException("The Garage vehicle was not equipped.");
                return;
            }
            if (vehicle != null && Submit(vehicle, now)) { garageVehicleClickSent = true; garageClickSince = now; }
            return;
        }
        var equipped = Find("garage-vehicle-" + expectedVehicle);
        if (equipped == null || !Drawn(equipped, now)) return;
        if (!Shot("garage")) return;
        var store = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory);
        initial = store.Read(PlayerProfileMode.Unlocked);   // the baseline after the Garage selection
        if (store.Read(PlayerProfileMode.Career).Digest != earnedDigest) throw new InvalidOperationException("Garage changed the earned profile.");
        Debug.Log(Tag + "GARAGE_DONE generation=" + initial.Generation);
        garageDone = true;
    }

    void TickReturned(OriginalWorldRuntime rt, double now)
    {
        ClearInput();
        if (SceneManager.GetActiveScene().name != "FrontEnd" || rt != null || OpenShell.Instance == null) return;
        var final = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory).Read(PlayerProfileMode.Unlocked);
        initial.Document.RequireSameSchema(final.Document);
        if (final.Generation != initial.Generation + 2) throw new InvalidOperationException("The completion did not publish two checkpoints: generation " + initial.Generation + " -> " + final.Generation);
        if (!rewardObserved) throw new InvalidOperationException("No native reward observed.");
        long expected = (long)rewardBase + rewardBonus;
        foreach (string column in new[] { "AccountBalance", "TotalEarnings" })
        {
            long delta = (long)Cell(final.Document, "PlayerRider", 0, column) - Cell(initial.Document, "PlayerRider", 0, column);
            if (delta != expected) throw new InvalidOperationException("Durable wallet " + column + " moved by " + delta + ", the native reward was " + expected + ".");
        }
        int row = SeriesRow(final.Document);
        if (row < 0) throw new InvalidOperationException("No PlayerSeriesStatus row for series " + series + " after the completion.");
        int best = Cell(final.Document, "PlayerSeriesStatus", row, "BestPlaceFinish");
        if (!outcomeLogged) Debug.Log(Tag + "ACTUAL_OUTCOME_DURABLE_PASS series=" + series + " rank=" + finalRank + " status=" + finalStatus + " reward=" + rewardBase + "+" + rewardBonus + " best=" + best
            + (heatGates > 0 ? " heat_gates=" + heatGates + " heat_rounds=" + heatResultsShown + " aaa_pages=" + heatLoadingPages : ""));
        if (new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory).Read(PlayerProfileMode.Career).Digest != earnedDigest) throw new InvalidOperationException("The race changed the independent earned profile.");
        outcomeLogged = true;
        var listed = Find("career-series-" + series);
        if (listed == null || !Drawn(listed, now) || !Shot("return")) return;
        Debug.Log(Tag + "POST_RETURN_CATALOG_PASS series=" + series);
        finished = true; StartCoroutine(Succeed(final));
    }

    void Diagnostic(OriginalWorldRuntime rt, CareerSnapshot snapshot, double now)
    {
        string text = "scene=" + SceneManager.GetActiveScene().name + " runtime=" + (rt != null);
        if (rt != null) text += " running=" + rt.Running + " valid=" + rt.Race.valid + " race=" + rt.Race.state + " clock=" + rt.Race.race_time + " laps=" + rt.Race.laps_completed + "/" + rt.Race.num_laps + " place=" + rt.Race.place;
        string key = text.Substring(0, Math.Min(text.Length, 40));
        if (snapshot != null)
        {
            var s = snapshot.Status;
            string state = " phase=" + s.phase + " event=" + s.event_phase + "/" + s.event_next_phase + " status=" + s.EventStatus + " activity=" + s.activity_state + "/" + s.activity_substate
                + " rows=" + s.results_count + " rank=" + s.local_rank + " continue=" + s.continue_allowed + "/" + s.continue_submitted + " finalized=" + s.records_finalized + " durable=" + durable;
            if (snapshot.Request.HasValue) state += " request=" + snapshot.Request.Value.kind + "/" + snapshot.Request.Value.Name;
            if (snapshot.Heat != null) { var h = snapshot.Heat; state += " heat=" + h.Phase + "/" + h.NextPhase + " gate=" + h.GateActive + "/" + h.Gates.Count + " routing=" + h.RoutingMessage + " area=" + h.ActivityAreaRequired + " round=" + h.RoundContinued + "/" + h.RoundPostObserved; }
            text += state; key += state;
        }
        if (key != lastDiagnostic || now - lastLog > 10)
        {
            // A request that stays pending: say what the player shows and whether the native post event finished.
            if (snapshot != null && snapshot.Request.HasValue && key == lastDiagnostic)
                text += " post_complete=" + snapshot.Status.post_event_complete + " ui=[" + VisibleActions() + "]";
            Debug.Log(Tag + text); lastDiagnostic = key; lastLog = now;
        }
    }

    void Return() { if (!durable || !continued) { Fail("Return invoked before the durable completion."); return; } OpenShell.ReturnToOwnedCareer(); }
    void Cancel() { Fail("Actual career cancel callback invoked."); }
    static int Cell(PlayerSnapshotDocument d, string table, int row, string column)
    { var value = d.Table(table).Get(row, column); if (value.Type != PlayerSnapshotType.Int) throw new InvalidOperationException("Expected integer " + column); return unchecked((int)value.Bits); }
    int SeriesRow(PlayerSnapshotDocument d)
    { var rows = Enumerable.Range(0, d.Table("PlayerSeriesStatus").RowCount).Where(r => Cell(d, "PlayerSeriesStatus", r, "SeriesId") == series).ToArray(); if (rows.Length > 1) throw new InvalidOperationException("Duplicate series record."); return rows.Length == 0 ? -1 : rows[0]; }

    // ---- the painted-control helpers (the same rules as MotoCareerFlowProbe: visible, painted, hit-tested, a real click) ----
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
        var b = value.worldBound; float inset = Mathf.Min(2f, Mathf.Min(b.width, b.height) * .25f);
        return HitTestPoint(value, b.center) && HitTestPoint(value, new Vector2(b.xMin + inset, b.yMin + inset)) && HitTestPoint(value, new Vector2(b.xMax - inset, b.yMax - inset));
    }
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
        if (!Drawn(value, now) || !(Visible(value) && value.enabledInHierarchy) || now - lastSubmit < 2) return false;
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
        Debug.Log(Tag + "TEST USER left-click delivered " + value.name);
        Unwatch(); return true;
    }
    void Unwatch() { if (candidate != null && draw != null) candidate.generateVisualContent -= draw; candidate = null; draw = null; }
    bool Shot(string name, VisualElement required = null)
    {
        if (shots.Contains(name)) return File.Exists(Path.Combine(shotDirectory, name + ".png"));
        shots.Add(name); StartCoroutine(Capture(name)); return false;
    }
    IEnumerator Capture(string name)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png")); Debug.Log(Tag + "screenshot " + name);
    }
    IEnumerator Succeed(PlayerSnapshotLease final)
    { yield return new WaitForSecondsRealtime(.5f); Debug.Log(Tag + "PASS series " + series + " generation=" + final.Generation + " rank=" + finalRank + " status=" + finalStatus); Application.Quit(0); }
    void Fail(string message)
    {
        if (finished) return; finished = true; ClearInput(); Unwatch();
        Debug.LogError(Tag + "FAIL " + message); StartCoroutine(StopFailed());
    }
    IEnumerator StopFailed() { if (!string.IsNullOrEmpty(shotDirectory)) { yield return Capture("failure"); yield return new WaitForSecondsRealtime(.3f); } Application.Quit(1); }
    void OnDestroy() { pilot?.Dispose(); Application.logMessageReceived -= ObserveLog; ClearInput(); Unwatch(); }
}

// TEST ONLY: steering along the copied race line with the mapped keys (Throttle, Brake, SteerLeft, SteerRight). Reads the copied pose
// and race line; writes nothing but the held keys.
internal sealed class CareerSeriesPilot : IDisposable
{
    readonly int throttle, brake, left, right;
    readonly Vector3[] line = new Vector3[64];
    int count; OriginalWorldRuntime owner; bool attached; float stuckClock;
    // REFLEX_PILOT_SCALE (run.py --pilot-scale): the target speeds scaled (iteration: a faster rider reaches the Heat's later rounds)
    static readonly float Scale = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_PILOT_SCALE"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var scale) && scale > 0.2f && scale < 5f ? scale : 1f;
    public CareerSeriesPilot(RemapTable controls)
    {
        throttle = Scan(controls, "Throttle"); brake = Scan(controls, "Brake"); left = Scan(controls, "SteerLeft"); right = Scan(controls, "SteerRight");
    }
    static int Scan(RemapTable t, string name)
    {
        int col = Array.IndexOf(t.columns, name);
        if (col < 0) throw new InvalidOperationException("Missing keyboard action " + name);
        var matches = Enumerable.Range(1, 255).Where(i => t.scan[i] == t.kb[col]).ToArray();
        if (t.kb[col] == 0 || matches.Length != 1) throw new InvalidOperationException("Nonunique scan code for " + name);
        return matches[0];
    }
    public void Attach(OriginalWorldRuntime runtime)
    {
        if (attached && ReferenceEquals(owner, runtime)) return;
        Dispose(); owner = runtime; attached = true; runtime.testRaceLineObserver = Capture;
    }
    void Capture(float[] values, int points, bool closed, uint frame)
    {
        count = 0;
        if (values == null || points < 2) return;
        for (int i = 0; i < points && i < line.Length; i++) line[i] = new Vector3(values[3 * i], 0, values[3 * i + 2]);
        count = Math.Min(points, line.Length);
    }
    public int[] Inputs(OriginalWorldRuntime runtime, bool authorized, double now)
    {
        var race = runtime.Race;
        if (!authorized || !runtime.Running || runtime.paused || race.valid == 0 || race.state != 2) return Array.Empty<int>();
        var bike = runtime.Bike;
        if (bike.position == null || bike.orientation == null || count < 2) return new[] { throttle };
        var position = new Vector3(bike.position[0], 0, bike.position[2]);
        var q = new Quaternion(bike.orientation[0], bike.orientation[1], bike.orientation[2], bike.orientation[3]);
        var forward = q * Vector3.forward; forward.y = 0;
        var velocity = bike.velocity != null ? new Vector3(bike.velocity[0], 0, bike.velocity[2]) : Vector3.zero;
        float speed = velocity.magnitude;
        int nearest = 0; float best = float.MaxValue;
        for (int i = 0; i < Math.Min(count, 6); i++) { float d = (line[i] - position).sqrMagnitude; if (d < best) { best = d; nearest = i; } }
        float look = 18f + speed * 0.6f; int target = nearest;
        for (int i = nearest; i < count; i++) { target = i; if ((line[i] - position).magnitude >= look) break; }
        var delta = line[target] - position;
        float angle = forward.sqrMagnitude < .01f || delta.sqrMagnitude < .01f ? 0 : Vector3.SignedAngle(forward, delta, Vector3.up);
        float turn = Mathf.Abs(angle);
        float desired = (turn > 70 ? 8 : turn > 35 ? 14 : turn > 17 ? 20 : 30) * Scale;
        stuckClock = speed < 1 ? stuckClock + Time.unscaledDeltaTime : 0;
        var keys = new List<int>(2);
        if (speed < desired) keys.Add(throttle); else if (speed > desired + 6) keys.Add(brake);
        float duty = Mathf.Clamp(turn / 30f, .15f, 1f);
        if (turn > 3f && Mathf.Repeat(race.race_time, .2f) < .2f * duty) keys.Add(angle > 0 ? right : left);
        return keys.ToArray();
    }
    public void Dispose() { if (owner != null && owner.testRaceLineObserver == Capture) owner.testRaceLineObserver = null; owner = null; attached = false; }
}
