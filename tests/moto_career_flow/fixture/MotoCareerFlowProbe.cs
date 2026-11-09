// Test-only driver: ordinary rendered controls and authored-route steering; never native acknowledgements.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public sealed class MotoCareerFlowProbe : MonoBehaviour
{
    const string Tag = "[moto-career-flow-probe] ";
    static readonly FieldInfo Simulation = typeof(OriginalWorldRuntime).GetField("simulation", BindingFlags.Instance | BindingFlags.NonPublic);
    readonly HashSet<string> shots = new HashSet<string>();
    PlayerSnapshotLease initial;PlayerSnapshotDocument rawInitial;
    CareerSnapshot lastOutcome;
    VisualElement candidate;
    Action<MeshGenerationContext> draw;
    string shotDirectory, lastDiagnostic;
    double started, candidateSince, lastSubmit, lastLog;
    int candidateFrame, paintedFrame = -1;
    bool opened, selected, continued, returned, finished, durable;
    int exceptions;
    bool observedRiding;
    MotoInputDriver steering;
    readonly ReplayFlowDiagnostics replayOne=new ReplayFlowDiagnostics(),replayTwo=new ReplayFlowDiagnostics();
    ReplayFlowDiagnostics replayDiagnostics;int drivingPhase;bool firstRacePassed,secondRacePassed,skipClicked;uint actualRewardBase,actualRewardBonus;bool rewardObserved;bool nativeStandingsLogged;bool hudOnePainted,hudTwoPainted;
    static readonly FieldInfo HudCanvas=typeof(OriginalHud).GetField("canvas",BindingFlags.Instance|BindingFlags.NonPublic);
    double missingContinueSince = -1,browserOpenedAt,firstEvidenceSince=-1;
    bool firstReplayDrained, secondReplayDrained;
    int rebuilds;
    Task<IReadOnlyList<CareerEventChoice>> finalCatalog;
    PlayerSnapshotLease finalDisk;
    static CareerEventChoice Choice(IReadOnlyList<CareerEventChoice> rows, int id) => rows.Single(x => x.Entry.SeriesId == id);
    string earnedDigest;
    bool identityObserved, garageOpened, garageVehicleTab, garageSelected, garageDone;
    bool garageTabClickSent, garageVehicleClickSent;double garageClickSince;
    int expectedOwner, expectedVehicle, expectedModel;
    bool PrepareUnlockedProfile()
    {
        if (initial != null) return true;
        if (OpenShell.Instance == null || Time.realtimeSinceStartupAsDouble - started < 2) return false;
        steering = new MotoInputDriver(Reflex.Port.FrontEnd.CoreRemapping.Read(false));
        var store = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory);
        earnedDigest = store.Read(PlayerProfileMode.Career).Digest;
        rawInitial=PlayerSnapshotDocument.Decode(File.ReadAllBytes(Path.Combine(CoreGarageProfile.SaveDirectory, "selected-moto-player24.bin")));
        store.CreateIfAbsent(PlayerProfileMode.Unlocked,rawInitial);
        CoreGarageProfile.Close();
        if (!CoreGarageProfile.Open() || !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
        if (Environment.GetEnvironmentVariable("REFLEX_PLATES_SHOTS") == "1") Debug.Log("[moto-career-flow-probe] SetRiderText FERRET / 998 " + CoreGarageProfile.SetRiderText("Name", "FERRET") + CoreGarageProfile.SetRiderText("Number", "998"));   // (run.py --plates)
        initial = store.Read(PlayerProfileMode.Unlocked);
        if (initial.Document.Tables.Count != 24) throw new InvalidOperationException("Full24 unlocked lease required.");
        expectedOwner = int.Parse(Environment.GetEnvironmentVariable("REFLEX_MOTO_OWNER"));
        expectedVehicle = int.Parse(Environment.GetEnvironmentVariable("REFLEX_MOTO_VEHICLE"));
        expectedModel = int.Parse(Environment.GetEnvironmentVariable("REFLEX_MOTO_MODEL"));
        if (store.Read(PlayerProfileMode.Career).Digest != earnedDigest) throw new InvalidOperationException("Mode selection changed earned profile.");

        return true;
    }

    // Called by the isolated ReflexShotProbe build hook, never enabled in production.
    public static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_MOTO_CAREER_FLOW_PROBE") != "1" || FindAnyObjectByType<MotoCareerFlowProbe>() != null) return;
        if(!Reflex.Port.Platform.ReflexSmoke.GuestRan||!ReflexProfile.Disabled)throw new InvalidOperationException("Existing registered Guest did not isolate before Awake.");
        Debug.Log(Tag+"REGISTERED_INIT_PASS GuestBeforeAwake=1");
        var go = new GameObject("Career Flow TEST ONLY"); DontDestroyOnLoad(go);
        go.AddComponent<MotoCareerFlowProbe>(); Application.runInBackground = true;
    }
    void Awake()
    {
        started = Time.realtimeSinceStartupAsDouble;replayDiagnostics=replayOne;
        Application.logMessageReceived += ObserveLog;Canvas.willRenderCanvases+=HudPainted;
        try
        {
            shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (string.IsNullOrWhiteSpace(shotDirectory) || string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR")))
                throw new InvalidOperationException("Detached save and screenshot directories are required.");
            Directory.CreateDirectory(shotDirectory);
            if (Environment.GetEnvironmentVariable("REFLEX_CAREER_FLOW_PROBE") == "1" || Environment.GetEnvironmentVariable("REFLEX_GARAGE_IDENTITY_FIXTURE") == "1")
                throw new InvalidOperationException("Mutually exclusive test drivers required.");
            Debug.Log(Tag + "BEGIN controlled unlocked Moto14 prestart skip + two real motos; independent earned profile preserved.");
        }
        catch (Exception e) { Fail(e.ToString()); }
    }
    void HudPainted(){
        if(drivingPhase!=6&&drivingPhase!=7)return;var rt=FindAnyObjectByType<OriginalWorldRuntime>();if(rt==null||!rt.OwnedCareerActive||rt.Race.state!=2||rt.EventUi.known==0||rt.EventUi.activity_type!=0)return;
        bool painted=false;foreach(var component in new MonoBehaviour[]{rt.GetComponent<OriginalHud>(),rt.GetComponent<OriginalLiveUi>(),rt.GetComponent<Reflex.Port.Hud.ReflexModernHud>()}){if(component==null)continue;var field=component.GetType().GetField("canvas",BindingFlags.Instance|BindingFlags.NonPublic);var canvas=field?.GetValue(component)as Canvas;if(canvas!=null&&canvas.enabled&&canvas.gameObject.activeInHierarchy){painted=true;break;}}if(!painted)return;
        if(drivingPhase==6&&!hudOnePainted){hudOnePainted=true;Debug.Log(Tag+"NATIVE_NATIONAL_HUD_PAINT moto=1 phase=6 actualCanvas=1");}if(drivingPhase==7&&!hudTwoPainted){hudTwoPainted=true;Debug.Log(Tag+"NATIVE_NATIONAL_HUD_PAINT moto=2 phase=7 actualCanvas=1");}
    }
    void ObserveLog(string message, string stack, LogType type) { if (type == LogType.Exception) exceptions++; }
    void ClearInput()
    { foreach (var rt in FindObjectsByType<OriginalWorldRuntime>(FindObjectsInactive.Include, FindObjectsSortMode.None)) rt.testHeldKeys = Array.Empty<int>(); }
    void Return()
    {
        if (!durable || !continued) { Fail("Return invoked before genuine saved completion."); return; }
        returned = true; ClearInput(); Debug.Log(Tag + "USER Return -> FrontEnd"); OpenShell.ReturnToOwnedCareer();
    }
    void Cancel()
    {
        // Evidence for an unrequested Leave: which path invoked it (pointer click vs menu Submit) and which real devices were live.
        var devices=string.Join("; ",UnityEngine.InputSystem.InputSystem.devices.Select(d=>d.displayName+"/"+d.layout+" enabled="+d.enabled+" updated="+(UnityEngine.InputSystem.LowLevel.InputState.currentTime-d.lastUpdateTime).ToString("0.00")+"s ago"));
        Debug.Log(Tag+"CANCEL_SOURCE focused="+Application.isFocused+" devices=["+devices+"] stack="+Environment.StackTrace.Replace("\n"," | "));
        Fail("Actual career cancel callback invoked.");
    }
    void Update()
    {
        if (finished) return;
        try { Tick(); } catch (Exception e) { Fail(e.ToString()); }
    }
    void Tick()
    {
        double now = Time.realtimeSinceStartupAsDouble;
        if (now - started > 12016) throw new TimeoutException("Two-Moto route-derived wall budget exceeded.");
        if(FindAnyObjectByType<OriginalWorldRuntime>() is OriginalWorldRuntime timed && drivingPhase!=0 && timed.Race.race_time>1427)throw new TimeoutException("Actual Moto exceeded authored route-derived racing budget.");
        if (exceptions != 0) throw new InvalidOperationException("Actual player logged exception(s): " + exceptions);
        if (!PrepareUnlockedProfile()) return;
        if (!garageDone)
        {
            if (!garageOpened) {
                if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) throw new InvalidOperationException("Production core garage required.");
                ReflexProfile.Disabled = false; // Detached full24 modes exist; ordinary core Garage can now open safely.
                OpenShell.Instance.ShowGarage(); garageOpened = true; return;
            }
            if (!garageVehicleTab) {
                // GARAGE > VEHICLE > MY RIDES > ALL VEHICLES, a left-click each (the drill-down, w-garageflow)
                if(Find("garage-vehicle-"+expectedVehicle)!=null){garageVehicleTab=true;Debug.Log(Tag+"GARAGE_TAB_CONSUMED selectedTab=1 vehicleList=1");return;}
                if(garageTabClickSent&&now-garageClickSince>30)throw new TimeoutException("Left-clicks through GARAGE > VEHICLE > MY RIDES did not show the vehicle list within 30 seconds.");
                var step=Find("garage-class-all")??Find("garage-my-rides")??Find("garage-h-vehicle");
                if(step!=null&&Submit(step,now)){if(!garageTabClickSent)garageClickSince=now;garageTabClickSent=true;}return;
            }
            if (!garageSelected) {
                var vehicle=Find("garage-vehicle-"+expectedVehicle);
                if(garageVehicleClickSent){
                    if(vehicle!=null&&vehicle.Query<Label>().ToList().Any(x=>x.text.Contains("EQUIPPED"))){garageSelected=true;Debug.Log(Tag+"GARAGE_VEHICLE_CONSUMED vehicle="+expectedVehicle+" equippedLabel=1");}
                    else if(now-garageClickSince>30)throw new TimeoutException("Left-click reached garage vehicle but its EQUIPPED state was not observed within 30 seconds.");
                    return;
                }
                if(vehicle!=null&&Submit(vehicle,now)){garageVehicleClickSent=true;garageClickSince=now;}return;
            }
            var equipped = Find("garage-vehicle-" + expectedVehicle);
            var preview = Find("garage-preview") as UnityEngine.UIElements.Image;
            if (equipped == null || preview == null || preview.image == null || !Drawn(equipped, now)) return;
            if (!VisibleActions().Contains("EQUIPPED")) throw new InvalidOperationException("Garage did not confirm equipped vehicle.");
            if (!Shot("garage")) return;
            var store = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory);
            initial = store.Read(PlayerProfileMode.Unlocked); // Baseline AFTER the ordinary Garage selection CAS.
            if (store.Read(PlayerProfileMode.Career).Digest != earnedDigest) throw new InvalidOperationException("Garage changed earned profile.");
            Debug.Log(Tag + "UNLOCKED_LEASE_PASS generation=" + initial.Generation + " controlledUnlockedClone=1 earnedPreserved=1 visibleGarageSelection=1 model=" + expectedModel);
            garageDone = true;
        }
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var driver = rt == null ? null : Simulation?.GetValue(rt) as IReflexSimDriver;
        var open = driver?.Open;
        var snapshot = open?.Career?.Snapshot;
        if(snapshot?.Moto!=null&&(snapshot.Moto.Phase==6||snapshot.Moto.Phase==7)&&drivingPhase!=(int)snapshot.Moto.Phase){
            if(drivingPhase==6&&!firstRacePassed)throw new InvalidOperationException("Moto two appeared before three natural first-Moto laps.");
            steering?.Dispose();steering=new MotoInputDriver(Reflex.Port.FrontEnd.CoreRemapping.Read(false));drivingPhase=(int)snapshot.Moto.Phase;replayDiagnostics=drivingPhase==6?replayOne:replayTwo;Debug.Log(Tag+"ACTUAL_MOTO phase="+drivingPhase+" sub="+snapshot.Moto.SubEvent);
        }
        replayDiagnostics.Sample(rt, open);
        if(snapshot?.Completion.HasValue==true&&snapshot.Completion.Value.complete_event_state==2&&snapshot.Status.reward_valid==1){
            if(rewardObserved&&(actualRewardBase!=snapshot.Status.reward_base||actualRewardBonus!=snapshot.Status.reward_bonus))throw new InvalidOperationException("Actual final reward changed.");rewardObserved=true;actualRewardBase=snapshot.Status.reward_base;actualRewardBonus=snapshot.Status.reward_bonus;
        }
        // Canonical completion clears the event/results. Retain the copied,
        // immutable actual outcome through that teardown for the reload checks.
        if (snapshot != null && snapshot.Status.series_id == 14 && snapshot.Status.results_count == 12 && snapshot.Completion.HasValue && snapshot.Completion.Value.post_state_observed==1)
        {
            int observedRank = unchecked((int)snapshot.Status.local_rank);
            if (observedRank < 0 || observedRank >= 12) throw new InvalidOperationException("Invalid actual outcome rank.");
            if (lastOutcome != null && (lastOutcome.Status.generation != snapshot.Status.generation || lastOutcome.Status.local_rank != snapshot.Status.local_rank))
                throw new InvalidOperationException("Actual outcome generation/rank changed after results publication.");
            lastOutcome = snapshot;
            if(snapshot.Completion.Value.complete_event_state==2&&!nativeStandingsLogged){nativeStandingsLogged=true;for(int i=0;i<(int)snapshot.Status.results_count;i++){var row=snapshot.Status.Result(i);Debug.Log(Tag+"ACTUAL_FINAL_STANDING rank="+row.rank+" type="+row.player_type+" points="+row.points+" model="+row.model_id+" name="+row.Name);}}

        }
        if (open != null && !string.IsNullOrEmpty(open.Failure)) throw new InvalidOperationException("Worker: " + open.Failure);
        if (rt != null)
        {
            var race = rt.Race;
            bool riding = rt.OwnedCareerActive && race.valid != 0 && (race.state == 2 || race.state == 5);
            bool authorized=snapshot?.Moto!=null&&(snapshot.Moto.Phase==6||snapshot.Moto.Phase==7)&&!snapshot.Request.HasValue&&snapshot.Status.pending_requests==0;
            if(drivingPhase!=0){steering.Attach(rt);rt.testHeldKeys=Array.Empty<int>();
                var imported=open?.Career?.ImportedVehicle;
                bool observationIdentity=snapshot?.Moto!=null&&snapshot.Status.series_id==14&&snapshot.Status.generation==snapshot.Moto.Generation&&snapshot.Status.Location=="NAT_Stillwater"&&imported.HasValue&&imported.Value.vehicle_type==3&&imported.Value.owner==expectedOwner&&imported.Value.owner_vehicle_id==expectedVehicle&&imported.Value.model_id==expectedModel&&imported.Value.authored_class==10&&imported.Value.resolved_class>=0&&snapshot.Status.selected_vehicle_class==imported.Value.resolved_class;
                steering.Observe(rt,observationIdentity,snapshot?.Moto?.Generation??0,snapshot?.Moto?.Phase??0,snapshot?.Moto?.SubEvent??0);
                rt.testHeldKeys=steering.Inputs(rt,authorized,now,snapshot?.Moto?.Generation??0,snapshot?.Moto?.Phase??0,snapshot?.Moto?.SubEvent??0);if(steering.ThreeLapsObserved){if(drivingPhase==6&&!firstRacePassed){firstRacePassed=true;Debug.Log(Tag+"NATURAL_MOTO_FINISH moto=1 observedLaps=0.."+MotoInputDriver.ExpectedLaps+" nativeLaps="+race.laps_completed+" state="+race.state);}if(drivingPhase==7&&!secondRacePassed){secondRacePassed=true;Debug.Log(Tag+"NATURAL_MOTO_FINISH moto=2 observedLaps=0.."+MotoInputDriver.ExpectedLaps+" nativeLaps="+race.laps_completed+" state="+race.state);}}}
            else rt.testHeldKeys=Array.Empty<int>();
            Shot("startup");
            if(riding&&race.state==2&&race.race_time>3&&drivingPhase!=0){
                var identity=open.Career.ImportedVehicle;if(!identity.HasValue||identity.Value.vehicle_type!=3||identity.Value.owner!=expectedOwner||identity.Value.owner_vehicle_id!=expectedVehicle||identity.Value.model_id!=expectedModel||identity.Value.authored_class!=10||identity.Value.resolved_class<0||snapshot.Status.selected_vehicle_class!=identity.Value.resolved_class||snapshot.Status.Location!="NAT_Stillwater")throw new InvalidOperationException("Actual imported Moto class10/type3 Garage identity differs.");
                var bike=rt.Bike;if(bike.position==null||bike.position.Take(3).Any(x=>float.IsNaN(x)||float.IsInfinity(x)))throw new InvalidOperationException("Actual native bike pose invalid.");
                if(!identityObserved)Debug.Log(Tag+"MOTO_NATIVE_PASS owner="+identity.Value.owner+" vehicle="+identity.Value.owner_vehicle_id+" model="+identity.Value.model_id+" type=3 class=10");identityObserved=observedRiding=true;Shot("moto"+(drivingPhase==6?1:2)+"-riding");
            }
        }
        string diagnostic = "scene=" + SceneManager.GetActiveScene().name + " runtime=" + (rt != null) + " running=" + (rt != null && rt.Running);
        if (rt != null) diagnostic += " race=" + rt.Race.state + " clock=" + rt.Race.race_time + " place=" + rt.Race.place;
        if (rt != null) diagnostic += " eventUi=" + rt.EventUi.known + "/" + rt.EventUi.activity_type + "/" + rt.EventUi.race_state + "/" + rt.EventUi.post_state;
        if (snapshot != null)
        {
            var s = snapshot.Status;
            diagnostic += " generation=" + s.generation + " phase=" + s.phase + " event=" + s.event_phase + "/" + s.event_next_phase + " activity=" + s.activity_state + " postSubstate=" + s.activity_substate + " rows=" + s.results_count + " rank=" + s.local_rank + " finalized=" + s.records_finalized + " continue=" + s.continue_allowed + "/" + s.continue_submitted + " postComplete=" + s.post_event_complete;
            if (snapshot.Request.HasValue) { var q = snapshot.Request.Value; diagnostic += " request=" + q.kind + "/" + q.id + "/" + q.Name; }
            if (snapshot.Completion.HasValue)
            {
                var c = snapshot.Completion.Value;
                diagnostic += " completion=" + c.complete_event_state + "/" + c.complete_series_state + " receipts=" + c.checkpoint_one_id + "/" + c.checkpoint_two_id + " durable=" + c.durable + " permissions=" + c.complete_event_allowed + "/" + c.frontend_ready_allowed + "/" + c.complete_series_allowed;
                durable = c.durable == 1 && c.in_memory_settled == 1 && c.records_state == 2 && c.complete_series_state == 2 && c.checkpoint_one_id != 0 && c.checkpoint_two_id != 0 && c.checkpoint_one_id != c.checkpoint_two_id && s.records_finalized == 1 && s.pending_requests == 0 && !snapshot.Request.HasValue && s.checkpoint_error == 0 && c.checkpoint_error == 0 && string.IsNullOrEmpty(snapshot.CheckpointFailure);
            }
            if (!string.IsNullOrEmpty(snapshot.CheckpointFailure)) diagnostic += " checkpointFailure=" + snapshot.CheckpointFailure;
        }
        if (snapshot != null && snapshot.Transitions.HasValue)
        {
            var transition = snapshot.Transitions.Value;
            diagnostic += " transitions=" + transition.rules_state + "/" + transition.loading_state;
        }
        // Race time and continuous motion belong in the periodic payload, not the change key.
        string changeKey = SceneManager.GetActiveScene().name + "/" + (rt != null && rt.Running);
        if (snapshot != null)
        {
            var state = snapshot.Status;
            changeKey += "/" + state.generation + "/" + state.phase + "/" + state.event_phase + "/" + state.event_next_phase + "/" + state.activity_state + "/" + state.records_finalized + "/" + state.checkpoint_error + "/" + state.continue_allowed + "/" + state.continue_submitted + "/" + state.post_event_complete + "/" + snapshot.CheckpointFailure;
            if (snapshot.Request.HasValue) { var request = snapshot.Request.Value; changeKey += "/request:" + request.id + "/" + request.kind + "/" + request.state + "/" + request.Name; }
            if (snapshot.Completion.HasValue)
            {
                var completion = snapshot.Completion.Value;
                changeKey += "/completion:" + completion.complete_event_state + "/" + completion.frontend_ready + "/" + completion.records_state + "/" + completion.complete_series_state + "/" + completion.checkpoint_one_id + "/" + completion.checkpoint_two_id + "/" + completion.durable + "/" + completion.complete_event_allowed + "/" + completion.frontend_ready_allowed + "/" + completion.complete_series_allowed;
            }
            if (snapshot.Transitions.HasValue) { var transition = snapshot.Transitions.Value; changeKey += "/transitions:" + transition.rules_state + "/" + transition.loading_state; }
        }
        if (changeKey != lastDiagnostic || now - lastLog > 10) { Debug.Log(Tag + diagnostic + " visible=" + VisibleActions()); lastDiagnostic = changeKey; lastLog = now; }
        if (returned)
        {
            ClearInput();
            if (SceneManager.GetActiveScene().name != "FrontEnd" || rt != null || OpenShell.Instance == null) return;
            var final = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory).Read(PlayerProfileMode.Unlocked);
            initial.Document.RequireSameSchema(final.Document);
            if (final.Generation != initial.Generation + 2 || final.Document.Tables.Count != 24) throw new InvalidOperationException("Returned profile is not validated relative final Player24.");
            // Invoke the exact production envelope decoder read-only; no alternate codec.
            var decode = typeof(PlayerSnapshotStore).GetMethod("Read", BindingFlags.Static | BindingFlags.NonPublic, null, new[] { typeof(string), typeof(PlayerProfileMode) }, null);
            if (decode == null) throw new MissingMethodException("Production snapshot envelope decoder.");
            var backup = (PlayerSnapshotLease)decode.Invoke(null, new object[] { final.Path + ".bak", PlayerProfileMode.Unlocked });
            initial.Document.RequireSameSchema(backup.Document);
            if (backup.Generation != initial.Generation + 1 || backup.Document.Tables.Count != 24) throw new InvalidOperationException("Backup is not validated relative backup Player24.");
            foreach (var milestone in new[] { "garage", "browser", "practice-choice", "startup", "gate1", "gate2", "moto1-riding", "moto1-results", "moto2-riding", "results", "rewards", "saved" })
                if (!File.Exists(Path.Combine(shotDirectory, milestone + ".png"))) throw new InvalidOperationException("Missing actual screenshot milestone: " + milestone);
            CheckDurableOutcome(final.Document,backup.Document);
            if(!hudOnePainted||!hudTwoPainted||!firstRacePassed||!secondRacePassed||!skipClicked||lastOutcome?.FirstMoto==null||lastOutcome.SecondMoto==null)throw new InvalidOperationException("Missing actual two-Moto progress/score boundaries.");
            MotoScoredObservation.RequireTwoGenuineMotos(lastOutcome.FirstMoto,lastOutcome.SecondMoto);
            if (!observedRiding || !identityObserved) throw new InvalidOperationException("No actual state2 riding window observed.");
            if (finalCatalog == null)
            {
                Debug.Log(Tag + "BACKUP_CODEC_PASS generation=" + backup.Generation + " schema=" + backup.Document.SchemaDigest + " digest=" + backup.Digest);
                if (lastOutcome == null || lastOutcome.Status.series_id != 14 || lastOutcome.Status.results_count != 12 || !lastOutcome.Completion.HasValue || lastOutcome.Completion.Value.complete_event_state != 2) throw new InvalidOperationException("Missing genuine Series14 twelve-rider outcome.");
                int rank = unchecked((int)lastOutcome.Status.local_rank);
                if (rank < 0 || rank >= 12) throw new InvalidOperationException("Invalid actual placing.");
                if (new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory).Read(PlayerProfileMode.Career).Digest != earnedDigest) throw new InvalidOperationException("Race changed independent earned profile.");
                finalDisk = final;
                if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
                finalCatalog = OpenReflexSimulation.ReadCareerCatalogAsync(ReflexInstall.Require(), CareerEventCatalog.Read(), CoreGarageProfile.CapturePersistence(),
                    new OwnCareerPresentationInstallation(RxCareerPresenter.NationalHud | (RxCareerPresenter)256, Return, Cancel).Capabilities, CancellationToken.None);
                return;
            }
            if (!finalCatalog.IsCompleted) return;
            var catalog = finalCatalog.GetAwaiter().GetResult(); var nextSeries = Choice(catalog, 14);
            if (!nextSeries.CanSelect || nextSeries.Locked) throw new InvalidOperationException("Controlled unlocked catalog4 eligibility changed.");
            var unchanged = new PlayerSnapshotStore(CoreGarageProfile.SaveDirectory).Read(PlayerProfileMode.Unlocked);
            if (unchanged.Digest != finalDisk.Digest) throw new InvalidOperationException("Read-only catalog changed durable file.");
            var browser3 = Find("career-series-14"); var browser4 = Find("career-series-14");
            if (browser3 == null || browser4 == null || !browser3.enabledInHierarchy) return;
            if (!browser4.enabledInHierarchy) throw new InvalidOperationException("Rendered unlocked career list differs from native eligibility.");
            if (!Drawn(browser3, now) || !Shot("return")) return;
            Debug.Log(Tag + "POST_RETURN_CATALOG_PASS series14Selectable=" + nextSeries.CanSelect + " actualRank=" + lastOutcome.Status.local_rank + " controlledUnlockedClone=1" + " readOnlyDigest=" + unchanged.Digest);
            if(new[]{replayOne,replayTwo}.Any(x=>!x.SawActive||!x.SawValidCamera||!x.SawPending||!x.SawDrain||x.DistinctActiveFrames<2))
                throw new InvalidOperationException("Missing actual Waypoint replay camera/request/paint-drain evidence.");
            Debug.Log(Tag + "TWO_MOTO_REPLAY_RETURN_PASS activeFrames=" + replayDiagnostics.DistinctActiveFrames + " cameraValid=1 pendingObserved=1 paintedDrain=1");
            finished = true; StartCoroutine(Succeed(final)); return;
        }
        if (!opened)
        {
            if (OpenShell.Instance == null || now - started < 2) return;
            if (!CoreGarageProfile.UnlockEverythingEnabled) throw new InvalidOperationException("Unlocked profile mode required.");
            OpenShell.Instance.ShowOwnedCareer(new OwnCareerPresentationInstallation(RxCareerPresenter.NationalHud | (RxCareerPresenter)256, Return, Cancel));
            opened = true;browserOpenedAt=now; Debug.Log(Tag + "Opened actual career browser."); return;
        }
        if (!selected)
        {
            var button = Find("career-series-14");
            if (button == null) {
                if (now-browserOpenedAt>120) {
                    string text=OpenShell.Instance?.Layer==null?"No career screen":string.Join(" | ",OpenShell.Instance.Layer.Query<Label>().ToList().ConvertAll(x=>x.text));
                    throw new InvalidOperationException("Career catalog did not expose Series14: "+text);
                }
                return;
            }
            if (!button.enabledInHierarchy) throw new InvalidOperationException("Actual Series14 is locked or unsupported; no bypass.");
            if (!Drawn(button, now) || !Shot("browser",button)) return;
            if (Submit(button, now)) { selected = true; Debug.Log(Tag + "USER selected actual Series14."); }
            return;
        }
        var skip=Find("career-moto-skip-practice");
        if(skip!=null&&!skipClicked){
            var choicePage=Find("career-moto-practice-choice");
            if(choicePage==null||!choicePage.Contains(skip)||skip.GetFirstAncestorOfType<ScrollView>()!=null||Find("career-series-14")!=null||Find("career-moto-practice-back")==null)
                throw new InvalidOperationException("Practice choice must replace the career list and expose Back.");
            if(!Drawn(skip,now)||!Clickable(skip)||!Shot("practice-choice",skip))return;
            if(Submit(skip,now)){skipClicked=true;Debug.Log(Tag+"USER prestart SkipPractice; practice riding NOT CLAIMED");}return;
        }
        if(snapshot?.Moto!=null){
            var moto=snapshot.Moto;var begin=Find("career-moto-gates");if(begin!=null){Submit(begin,now);return;}
            if(moto.GateActive&&moto.Device==0&&moto.Gates.Count>0){var gate=Find("career-moto-gate-"+moto.Gates[0]);if(gate!=null){if(!Shot(moto.NextPhase==6?"gate1":"gate2"))return;Submit(gate,now);return;}}
            var firstContinue=Find("career-moto-first-continue");if(firstContinue!=null){
                // Latched: the replay keeps cutting cameras with fades, so "drained and not fading" is only ever true for moments;
                // once the first replay has been observed painted-drained it stays proven (observation only, nothing is sent).
                firstReplayDrained|=replayOne.PaintedPendingDrained&&replayOne.DistinctActiveFrames>=2;
                if(!AwaitFirstEvidence(snapshot,now,firstReplayDrained,"first-continue"))return;
                if(!Shot("moto1-results"))return;Submit(firstContinue,now);return;}
            var between=Find("career-moto-next");if(between!=null){
                if(!AwaitFirstEvidence(snapshot,now,moto.FirstPostObserved&&replayOne.SawDrain&&replayOne.DistinctActiveFrames>=2,"inter-moto"))return;
                if(!Shot("moto1-results"))return;Submit(between,now);return;}
        }
        var accept = Find("career-rules-accept");
        if (accept != null) { Submit(accept, now); return; }
        var next = Find("career-moto-final");
        bool missingContinue = !continued && snapshot != null && snapshot.Status.results_count == 12 && snapshot.Status.continue_allowed != 0 && next == null;
        if (!missingContinue) missingContinueSince = -1;
        else if (missingContinueSince < 0) missingContinueSince = now;
        else if (now - missingContinueSince > 15)
            throw new InvalidOperationException("Native Continue allowed for15seconds but no visible career-continue control. " + VisibleActions());
        secondReplayDrained |= replayTwo.PaintedPendingDrained;   // latched, as the first replay's
        if (next != null && !continued && secondReplayDrained && secondRacePassed)
        {
            if (!Shot("results")) return;
            if (Submit(next, now)) { continued = true; Debug.Log(Tag + "USER Continue actual results."); }
            return;
        }
        if (snapshot != null && snapshot.Status.reward_valid != 0) Shot("rewards");
        var back = Find("career-return");
        if (back != null && durable)
        {
            if (!Shot("saved")) return;
            Submit(back, now);
        }
    }
    bool AwaitFirstEvidence(CareerSnapshot snapshot,double now,bool replayReady,string boundary)
    {
        bool ready=firstRacePassed&&snapshot?.FirstMoto!=null&&replayReady;
        if(ready){firstEvidenceSince=-1;return true;}
        ClearInput();
        if(firstEvidenceSince<0){firstEvidenceSince=now;Debug.Log(Tag+"WAIT_FIRST_EVIDENCE boundary="+boundary+" laps="+firstRacePassed+" immutableScores="+(snapshot?.FirstMoto!=null)+" replayDrain="+replayReady);}
        if(now-firstEvidenceSince>45)throw new TimeoutException("Actual first-Moto evidence absent after45seconds: boundary="+boundary+" naturalLaps="+firstRacePassed+" immutableScores="+(snapshot?.FirstMoto!=null)+" replayDrain="+replayReady);
        return false;
    }
    static int Cell(PlayerSnapshotDocument d, string table, int row, string column)
    { var value = d.Table(table).Get(row, column); if (value.Type != PlayerSnapshotType.Int) throw new InvalidOperationException("Expected integer " + column); return unchecked((int)value.Bits); }
    static int SeriesRow(PlayerSnapshotDocument d)
    { var rows = Enumerable.Range(0, d.Table("PlayerSeriesStatus").RowCount).Where(r => Cell(d,"PlayerSeriesStatus",r,"SeriesId") == 14).ToArray(); if (rows.Length > 1) throw new InvalidOperationException("Duplicate Series14 record."); return rows.Length == 0 ? -1 : rows[0]; }
    void CheckDurableOutcome(PlayerSnapshotDocument final, PlayerSnapshotDocument backup)
    {
        long expected = (long)actualRewardBase + actualRewardBonus;
        int beforeRow = SeriesRow(initial.Document), rank = lastOutcome.Status.local_rank;
        int priorPlace = beforeRow < 0 ? 1000 : Cell(initial.Document,"PlayerSeriesStatus",beforeRow,"BestPlaceFinish");
        // The prize depends on the place (005b1530): Reward of SeriesPoints[Series.SeriesPointId, Place = rank+1], plus its
        // BonusReward only for a new best within the top three. "table" derives it from the managed copy of the game tables
        // (independent of the core); an integer is an explicit expectation.
        string expectation = Environment.GetEnvironmentVariable("REFLEX_MOTO_EXPECT_CREDITS");
        long wanted;
        if (expectation == "table")
        {
            var tables = Reflex.Port.Content.ReflexEvents.Tables;
            var series = tables["Series"].Single(r => Rfx.Int(r, "SeriesId") == 14);
            int pointId = Rfx.Int(series, "SeriesPointId");
            var points = tables["SeriesPoints"].Where(r => Rfx.Int(r, "SeriesPointId") == pointId && Rfx.Int(r, "Place") == rank + 1).ToArray();
            long prize = points.Length == 0 ? 0 : Rfx.Int(points[0], "Reward", 0);
            bool improved = rank < priorPlace && rank < 3;
            long bonus = points.Length == 0 || !improved ? 0 : Rfx.Int(points[0], "BonusReward", 0);
            wanted = prize + bonus;
            Debug.Log(Tag + "REWARD_EXPECTATION table SeriesPointId=" + pointId + " place=" + (rank + 1) + " reward=" + prize + " bonus=" + bonus + " priorBest=" + priorPlace + " actual=" + actualRewardBase + "+" + actualRewardBonus);
            if (actualRewardBase != prize || actualRewardBonus != bonus) throw new InvalidOperationException("Actual native final reward differs from the table's prize for the actual place.");
        }
        else wanted = long.Parse(expectation);
        if(!rewardObserved||expected!=wanted)throw new InvalidOperationException("Actual native final reward differs from explicit fixture expectation: actual=" + expected + " wanted=" + wanted + " rank=" + rank);
        foreach (string column in new[] { "AccountBalance", "TotalEarnings" })
        {
            long delta = (long)Cell(final,"PlayerRider",0,column) - Cell(initial.Document,"PlayerRider",0,column);
            if(rawInitial==null||Cell(initial.Document,"PlayerRider",0,column)!=Cell(rawInitial,"PlayerRider",0,column))throw new InvalidOperationException("Garage baseline changed raw exported wallet.");
            if (delta != expected || Cell(final,"PlayerRider",0,column) != Cell(backup,"PlayerRider",0,column)) throw new InvalidOperationException("Durable wallet differs from actual reward: " + column);
        }
        int before = SeriesRow(initial.Document), after = SeriesRow(final), back = SeriesRow(backup);
        int priorBest = before < 0 ? 1000 : Cell(initial.Document,"PlayerSeriesStatus",before,"BestPlaceFinish");
        if (after < 0 || back < 0 || Cell(final,"PlayerSeriesStatus",after,"BestPlaceFinish") != Math.Min(priorBest,lastOutcome.Status.local_rank)) throw new InvalidOperationException("Durable Series14 best differs from actual placing.");
        foreach (var column in final.Table("PlayerSeriesStatus").Columns)
        { var x=final.Table("PlayerSeriesStatus").Get(after,column.Name); var y=backup.Table("PlayerSeriesStatus").Get(back,column.Name); if(x.Type!=y.Type || x.Bits!=y.Bits || (x.Type==PlayerSnapshotType.Text && !x.TextBytes.SequenceEqual(y.TextBytes))) throw new InvalidOperationException("Final/backup record differs: " + column.Name); }
        Debug.Log(Tag + "ACTUAL_OUTCOME_DURABLE_PASS rows=12 rank=" + lastOutcome.Status.local_rank + " rewardBase=" + actualRewardBase + " rewardBonus=" + actualRewardBonus + " completeEvent=2 exactWallet=1");
    }
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
        if(!Available(value))return false;
        var bounds=value.worldBound;var clip=value.panel.visualTree.worldBound;
        if(float.IsNaN(bounds.x)||float.IsNaN(bounds.y)||float.IsInfinity(bounds.x)||float.IsInfinity(bounds.y))return false;
        for(var p=value.parent;p!=null;p=p.parent){
            if(!(p is ScrollView scroll))continue;
            var limit=scroll.contentViewport.worldBound;
            clip=Rect.MinMaxRect(Mathf.Max(clip.xMin,limit.xMin),Mathf.Max(clip.yMin,limit.yMin),Mathf.Min(clip.xMax,limit.xMax),Mathf.Min(clip.yMax,limit.yMax));
            if(clip.width<=0||clip.height<=0)return false;
        }
        if(bounds.xMin<clip.xMin-1||bounds.yMin<clip.yMin-1||bounds.xMax>clip.xMax+1||bounds.yMax>clip.yMax+1)return false;
        return !(value.userData is OuNavItem)||HitTestControl(value);
    }
    static bool HitTestPoint(VisualElement value, Vector2 point)
    {
        var picked=value.panel.Pick(point);
        return picked==value||(picked!=null&&value.Contains(picked));
    }
    static bool HitTestControl(VisualElement value)
    {
        var bounds=value.worldBound;
        float inset=Mathf.Min(2f,Mathf.Min(bounds.width,bounds.height)*.25f);
        return HitTestPoint(value,bounds.center)
            &&HitTestPoint(value,new Vector2(bounds.xMin+inset,bounds.yMin+inset))
            &&HitTestPoint(value,new Vector2(bounds.xMax-inset,bounds.yMin+inset))
            &&HitTestPoint(value,new Vector2(bounds.xMin+inset,bounds.yMax-inset))
            &&HitTestPoint(value,new Vector2(bounds.xMax-inset,bounds.yMax-inset));
    }
    static bool Clickable(VisualElement value)
    {
        return Visible(value)&&value.enabledInHierarchy&&HitTestControl(value);
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
        if(!Visible(value)){
            Unwatch();
            if(value?.userData is OuNavItem){value.Focus();for(var p=value.parent;p!=null;p=p.parent)if(p is ScrollView scroll){scroll.ScrollTo(value);break;}}
            return false;
        }
        if (candidate != value)
        {
            // The same named control rebuilt by its page (career pages re-render on every snapshot change, e.g. during a replay):
            // keep the time it has been on screen, but require a fresh paint of the new instance.
            bool rebuilt = candidate != null && candidate.name == value.name && candidate.name.Length > 0;
            double since = rebuilt ? candidateSince : now;
            if (rebuilt && ++rebuilds % 100 == 1) Debug.Log(Tag + "CONTROL_REBUILT " + value.name + " count=" + rebuilds + " watchedFor=" + (now - since).ToString("0.0") + "s");
            Unwatch(); candidate = value; candidateSince = since; candidateFrame = Time.frameCount; paintedFrame = -1;
            // only note the paint: hit-testing (Visible) inside generateVisualContent re-enters layout during rendering, which UI Toolkit
            // forbids; Drawn checks Visible itself outside rendering
            draw = _ => { if (paintedFrame < 0) paintedFrame = Time.frameCount; }; candidate.generateVisualContent += draw; candidate.MarkDirtyRepaint(); return false;
        }
        return Visible(value) && paintedFrame >= candidateFrame && Time.frameCount > paintedFrame && now - candidateSince >= 1;
    }
    bool Submit(VisualElement value, double now)
    {
        if(!Drawn(value,now)||!Clickable(value)||now-lastSubmit<2)return false;
        if(!(value.userData is OuNavItem nav)||nav.submit==null)throw new InvalidOperationException("Actual control has no OuNavItem submit: "+value.name);
        bool delivered=false;
        EventCallback<ClickEvent> observe=evt=>{if(evt.button==0&&evt.currentTarget==value)delivered=true;};
        // Observe before the actual handler can rebuild and detach this control.
        value.RegisterCallback(observe,TrickleDown.TrickleDown);
        try {
            var trigger=new UnityEngine.Event{type=EventType.MouseUp,button=0,clickCount=1,mousePosition=value.worldBound.center};
            using(var click=PointerEventBase<ClickEvent>.GetPooled(trigger)){
                if(click.button!=0||click.clickCount!=1)throw new InvalidOperationException("Pooled click lost its left-button semantics.");
                click.target=value;value.SendEvent(click);
            }
        } finally {value.UnregisterCallback(observe,TrickleDown.TrickleDown);}
        if(!delivered)throw new InvalidOperationException("Left-click was not delivered to the actual control: "+value.name);
        lastSubmit=now;
        Debug.Log(Tag+"TEST USER left-click delivered "+value.name+" button=0 count=1 bounds="+value.worldBound+" generatedFrame="+paintedFrame+" hitTest=1");
        Unwatch();return true;
    }
    void Unwatch() { if (candidate != null && draw != null) candidate.generateVisualContent -= draw; candidate = null; draw = null; }
    bool Shot(string name, VisualElement required = null)
    {
        if (shots.Contains(name)) return File.Exists(Path.Combine(shotDirectory, name + ".png"));
        shots.Add(name); StartCoroutine(Capture(name,required)); return false;
    }
    IEnumerator Capture(string name, VisualElement required = null)
    {
        yield return new WaitForEndOfFrame();
        if(required!=null&&!Clickable(required)){shots.Remove(name);yield break;}
        if(required!=null)Debug.Log(Tag+"SCREENSHOT_CONTROL "+name+" control="+required.name+" bounds="+required.worldBound+" generatedFrame="+paintedFrame+" captureFrame="+Time.frameCount+" hitTest=1");
        ScreenCapture.CaptureScreenshot(Path.Combine(shotDirectory, name + ".png")); Debug.Log(Tag + "screenshot " + name);
    }
    IEnumerator Succeed(PlayerSnapshotLease final)
    { yield return new WaitForSecondsRealtime(.5f); Debug.Log(Tag + "PASS actual Return generation=" + final.Generation + " schema=" + final.Document.SchemaDigest + " two durable checkpoints; actual prestart skip and two real three-lap Moto races; earned preserved; relative generations"); Application.Quit(0); }
    void Fail(string message)
    {
        if (finished) return; finished = true; ClearInput(); Unwatch();
        Debug.LogError(Tag + "FAIL " + message); StartCoroutine(StopFailed());
    }
    IEnumerator StopFailed() { if (!string.IsNullOrEmpty(shotDirectory)) { yield return Capture("failure"); yield return new WaitForSecondsRealtime(.3f); } Application.Quit(1); }
    void OnDestroy() { Canvas.willRenderCanvases-=HudPainted;steering?.Dispose();Application.logMessageReceived -= ObserveLog; ClearInput(); Unwatch(); }
}
