using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Text;

namespace Reflex.Port.RxCore
{
    // Native implementations execute exclusively on the simulation owner.
    internal interface ICareerWorkerNative
    {
        RxCareerResult Configure(IntPtr s, ref RxCareerConfig config, out ulong generation);
        RxCareerResult Series(IntPtr s, ulong generation, uint id, ref RxCareerSeriesInfo info);
        RxCareerResult Load(IntPtr s, ulong generation, uint id);
        RxCareerResult SelectVehicle(IntPtr s, ulong generation, uint type);
        RxCareerResult Initialize(IntPtr s, ulong generation);
        RxCareerResult Start(IntPtr s, ulong generation);
        RxCareerResult Status(IntPtr s, ulong generation, ref RxCareerStatus status);
        RxCareerResult Next(IntPtr s, ulong generation, ref RxCareerRequest request);
        RxCareerResult Ack(IntPtr s, ulong generation, ulong id, RxCareerRequestKind kind, bool accepted);
        RxCareerResult Wipe(IntPtr s, ulong generation, ulong id, string name, uint milestone);
        RxCareerResult Continue(IntPtr s, ulong generation);
        RxCareerResult CheckpointCommitted(IntPtr s, ulong generation, ulong id);
        RxCareerResult CheckpointFailed(IntPtr s, ulong generation, ulong id);
    }

    internal interface ICareerVehicleWorkerNative
    {
        RxCareerResult Vehicle(IntPtr s, ulong generation, ref RxCareerVehicleIdentity identity);
    }
    public sealed class CareerLaunch
    {
        public bool? MotoSkipPractice { get; }
        public uint SeriesId { get; }
        public RxCareerPresenter Presenters { get; }
        public bool ReplayPresentation { get; }
        /// The series' SeriesFormat ("Moto": the practice choice and the inter-moto pages), its race HUD route and the presenter
        /// capabilities its races need (rx_session_career_series_presentation).
        public string Format { get; }
        public string HudRoute { get; }
        public RxCareerPresenter RequiredPresenters { get; }
        public bool IsMoto => Format == "Moto";
        /// The Heat format (Series 41-46): the practice choice (practice or skip), the rounds' gate pages, results and AAA loading pages.
        public bool IsHeat => Format == "Heat";
        public bool HasPracticeChoice => IsMoto || IsHeat;
        /// The series' authored Riders (Series table): the roster the core builds for its races, so the startup waits for exactly
        /// that many slots (a Faceoff has 2, a Section Race 6, an Outrun 1; Moto14 12).
        public int Riders { get; }
        public CareerLaunch(uint seriesId, RxCareerPresenter presenters, bool replayPresentation = false, bool? motoSkipPractice = null,
            string format = null, string hudRoute = null, RxCareerPresenter requiredPresenters = 0, int riders = 12)
        {
            SeriesId = seriesId; Presenters = presenters; ReplayPresentation = replayPresentation;
            Format = format ?? (seriesId == 14 ? "Moto" : "SingleEvent"); HudRoute = hudRoute; RequiredPresenters = requiredPresenters;
            // A Heat series authors both heats' riders (NumRiders 24); each of its races has half of them (vf18 005aedd0 doubles the
            // class limit for the two heats): the heat, the LCQ (6 + 6) and the main event (5 + 5 + 2) all have 12.
            Riders = Math.Min(RxRiderCap.Max, Math.Max(1, IsHeat ? (riders + 1) / 2 : riders));
            MotoSkipPractice = HasPracticeChoice ? motoSkipPractice : null;
        }
    }

    // Struct properties return copies, including their inline name/result buffers.
    public sealed class CareerSnapshot
    {
        public MotoScoredObservation FirstMoto { get; }
        public MotoScoredObservation SecondMoto { get; }
        public MotoWorkerSnapshot Moto { get; }
        public HeatWorkerSnapshot Heat { get; }
        public RxCareerStatus Status { get; }
        public RxCareerRequest? Request { get; }
        public string CheckpointFailure { get; }
        public RxCareerTransitionStatus? Transitions { get; }
        public RxCareerCompletionStatus? Completion { get; }
        internal CareerSnapshot(RxCareerStatus status, RxCareerRequest? request, string checkpointFailure = null, RxCareerTransitionStatus? transitions = null, RxCareerCompletionStatus? completion = null, MotoWorkerSnapshot moto = null, MotoScoredObservation firstMoto = null, MotoScoredObservation secondMoto = null, HeatWorkerSnapshot heat = null)
        { Status = status; Request = request; CheckpointFailure = checkpointFailure; Transitions = transitions; Completion = completion; Moto = moto; FirstMoto=firstMoto; SecondMoto=secondMoto; Heat = heat; }
        /// The results that end the event: the final race's rows with its Post complete, or (Heat) the heat / LCQ the local rider did
        /// not qualify from ("Failed": the original sets no PostEventComplete there).
        public bool FinalResults => Status.results_count > 0 && (Status.post_event_complete == 1 || (Heat != null && Status.EventStatus == "Failed"));
    }

    public sealed class CareerWorker
    {
        enum ActionKind { Moto, Heat, Ack, Wipe, Continue, Checkpoint, RulesAccept, RulesEnded, LoadingEnter, LoadingHubEntered, CompleteEvent, FrontendReady, CompleteSeries }
        sealed class Command
        {
            public ActionKind action;
            public ulong generation, id, routeId;
            public RxCareerRequestKind kind;
            public string name;
            public uint milestone,motoAction,motoPhase,motoSub,motoCursor;public int motoGate;
            public bool accepted;
            public readonly TaskCompletionSource<RxCareerResult> completion =
                new TaskCompletionSource<RxCareerResult>(TaskCreationOptions.RunContinuationsAsynchronously);
        }
        readonly object gate = new object();
        readonly Queue<Command> commands = new Queue<Command>();
        readonly ICareerWorkerNative native;
        readonly ICareerTransitionWorkerNative transitions;
        readonly ICareerCompletionWorkerNative completion;
        readonly Action checkpoint;
        readonly Action<string> checkpointFailure;
        readonly HashSet<ulong> committedCheckpointIds = new HashSet<ulong>();
        string lastCheckpointFailure;
        int owner;
        bool started, closed, launchIsMoto, launchIsHeat;
        ulong generation;
        MotoScoredObservation firstMoto,secondMoto;
        CareerSnapshot snapshot;
        public CareerSnapshot Snapshot { get { lock (gate) return snapshot; } }
        internal CareerWorker(ICareerWorkerNative native, Action checkpoint = null, Action<string> checkpointFailure = null)
        { this.native = native; this.transitions = native as ICareerTransitionWorkerNative; this.completion = native as ICareerCompletionWorkerNative; this.checkpoint = checkpoint; this.checkpointFailure = checkpointFailure; }
        public CareerWorker(Action checkpoint = null, Action<string> checkpointFailure = null)
            : this(new NativeCareerWorker(), checkpoint, checkpointFailure) { }

        static void Require(RxCareerResult result, string operation)
        {
            if (result != RxCareerResult.Ok)
                throw new InvalidOperationException("Career " + operation + " failed: " + result);
        }
        void Owner()
        {
            if (owner == 0 || owner != Thread.CurrentThread.ManagedThreadId)
                throw new InvalidOperationException("Career calls require the simulation owner thread.");
        }
        // Profile import and controller remapping must already have succeeded.
        internal void StartWorker(IntPtr session, CareerLaunch launch, Action<IntPtr, ulong> configured = null)
        {
            if (launch == null || launch.HasPracticeChoice != launch.MotoSkipPractice.HasValue)
                throw new InvalidOperationException("This event requires its actual visible Skip Practice choice and owns no implicit practice setting.");
            lock (gate)
            {
                if (started || closed) throw new InvalidOperationException("Career session already started or closed.");
                started = true; owner = Thread.CurrentThread.ManagedThreadId; launchIsMoto = launch.IsMoto; launchIsHeat = launch.IsHeat;
            }
            try
            {
                if ((launch.Presenters & RxCareerPresenter.Rules) != 0 && transitions == null)
                    throw new InvalidOperationException("Career Rules transition bindings are unavailable.");
                var config = RxCareerConfig.Create(launch.Presenters);
                Require(native.Configure(session, ref config, out generation), "configuration");
                configured?.Invoke(session, generation);
                var info = RxCareerSeriesInfo.Create();
                Require(native.Series(session, generation, launch.SeriesId, ref info), "eligibility lookup");
                if (info.eligible == 0) throw new InvalidOperationException(info.locked != 0 ? "This career event is locked." : "This event is unavailable with the current vehicle or presentation. Choose a compatible vehicle in Garage.");
                Require(native.Load(session, generation, launch.SeriesId), "load");
                var identity = RxCareerVehicleIdentity.Create();
                var vehicleNative = native as ICareerVehicleWorkerNative;
                if (vehicleNative == null) throw new InvalidOperationException("Career vehicle identity API is unavailable.");
                Require(vehicleNative.Vehicle(session, generation, ref identity), "imported vehicle identity");
                // The core checked the equipped type against the series' VehicleClassType (eligible); the copy must be this session's.
                if (identity.generation != generation || identity.vehicle_type < 0)
                    throw new InvalidOperationException("Choose a supported vehicle in Garage before entering this career event.");
                Require(native.SelectVehicle(session, generation, (uint)identity.vehicle_type), "actual garage vehicle selection");
                var resolvedIdentity = RxCareerVehicleIdentity.Create();
                Require(vehicleNative.Vehicle(session, generation, ref resolvedIdentity), "canonical selected vehicle identity");
                if (resolvedIdentity.generation!=identity.generation || resolvedIdentity.player_row!=identity.player_row || resolvedIdentity.vehicle_list_id!=identity.vehicle_list_id || resolvedIdentity.owner!=identity.owner || resolvedIdentity.owner_vehicle_id!=identity.owner_vehicle_id || resolvedIdentity.model_id!=identity.model_id || resolvedIdentity.vehicle_type!=identity.vehicle_type || resolvedIdentity.authored_class!=identity.authored_class || resolvedIdentity.resolved_class<0)
                    throw new InvalidOperationException("Canonical vehicle selection changed imported identity or failed to resolve a class.");
                Require(native.Initialize(session, generation), "initialization");
                if(launch.HasPracticeChoice){if(!launch.MotoSkipPractice.HasValue)throw new InvalidOperationException("A rendered practice choice is required.");Require(RxCareerMotoNative.PracticeChoice(session,generation,launch.MotoSkipPractice.Value),"actual practice choice");}
                Require(native.Start(session, generation), "start");
                RefreshWorker(session);
            }
            catch { CloseWorker(); throw; }
        }
        Task<RxCareerResult> Post(Command command)
        {
            lock (gate)
            {
                if (closed || !started || snapshot == null) return Task.FromResult(RxCareerResult.Inactive);
                if (command.generation != snapshot.Status.generation) return Task.FromResult(RxCareerResult.Stale);
                if (commands.Count >= 32) return Task.FromResult(RxCareerResult.Full);
                commands.Enqueue(command); return command.completion.Task;
            }
        }
        public Task<RxCareerResult> Acknowledge(ulong generation, ulong id, RxCareerRequestKind kind, bool accepted) =>
            Post(new Command { action = ActionKind.Ack, generation = generation, id = id, kind = kind, accepted = accepted });
        public Task<RxCareerResult> WipeCompleted(ulong generation, ulong id, string name, uint milestone)
        {
            if (string.IsNullOrEmpty(name) || name.IndexOf('\0') >= 0 || Encoding.UTF8.GetByteCount(name) >= 256 || (milestone != 1 && milestone != 2))
                return Task.FromResult(RxCareerResult.Invalid);
            return Post(new Command { action = ActionKind.Wipe, generation = generation, id = id, name = name, milestone = milestone });
        }
        public Task<RxCareerResult> MotoAction(ulong generation,ulong hub,uint action,MotoWorkerSnapshot expected,int gateValue = -1) => expected==null||expected.Generation!=generation ? Task.FromResult(RxCareerResult.Stale) : Post(new Command{action=ActionKind.Moto,generation=generation,id=hub,motoAction=action,motoPhase=expected.Phase,motoSub=expected.SubEvent,motoCursor=expected.Cursor,motoGate=gateValue});
        public Task<RxCareerResult> HeatAction(ulong generation,ulong hub,RxHeatAction action,HeatWorkerSnapshot expected,int gateValue = -1) => expected==null||expected.Generation!=generation ? Task.FromResult(RxCareerResult.Stale) : Post(new Command{action=ActionKind.Heat,generation=generation,id=hub,motoAction=(uint)action,motoPhase=expected.Phase,motoSub=expected.NextPhase,motoCursor=expected.Cursor,motoGate=gateValue});
        public Task<RxCareerResult> Continue(ulong generation) =>
            Post(new Command { action = ActionKind.Continue, generation = generation });

        public Task<RxCareerResult> SaveCheckpoint(ulong generation, ulong id) =>
            Post(new Command { action = ActionKind.Checkpoint, generation = generation, id = id });

        // Explicit presenter/user milestones only. These calls enqueue on the owner;
        // they never manufacture shown, In/Out, hidden, Hub entry, or save receipts.
        public Task<RxCareerResult> AcceptRules(ulong generation, ulong requestId, bool doNotShowAgain) =>
            Post(new Command { action = ActionKind.RulesAccept, generation = generation, id = requestId, accepted = doNotShowAgain });
        public Task<RxCareerResult> RulesPopupEnded(ulong generation, ulong wipeId) =>
            Post(new Command { action = ActionKind.RulesEnded, generation = generation, id = wipeId });
        // Native currently returns Unsupported; no activation fallback is supplied.
        public Task<RxCareerResult> EnterLoading(ulong generation) =>
            Post(new Command { action = ActionKind.LoadingEnter, generation = generation });
        public Task<RxCareerResult> LoadingHubEntered(ulong generation, ulong wipeId) =>
            Post(new Command { action = ActionKind.LoadingHubEntered, generation = generation, id = wipeId });

        public Task<RxCareerResult> CompleteEvent(ulong generation, ulong ticket) => ticket == 0
            ? Task.FromResult(RxCareerResult.Invalid)
            : Post(new Command { action = ActionKind.CompleteEvent, generation = generation, id = ticket });
        public Task<RxCareerResult> FrontendReady(ulong generation, ulong ticket, ulong routeId) => ticket == 0 || routeId == 0
            ? Task.FromResult(RxCareerResult.Invalid)
            : Post(new Command { action = ActionKind.FrontendReady, generation = generation, id = ticket, routeId = routeId });
        public Task<RxCareerResult> CompleteSeries(ulong generation, ulong ticket) => ticket == 0
            ? Task.FromResult(RxCareerResult.Invalid)
            : Post(new Command { action = ActionKind.CompleteSeries, generation = generation, id = ticket });

        internal bool DurablySettled
        {
            get
            {
                var view = Snapshot;
                if (view == null || !view.Completion.HasValue || view.CheckpointFailure != null) return false;
                var value = view.Completion.Value;
                return value.generation == view.Status.generation && value.ticket != 0
                    && value.durable == 1 && value.in_memory_settled == 1 && value.complete_series_state == 2
                    && value.records_state == 2 && value.checkpoint_one_id != 0 && value.checkpoint_two_id != 0
                    && value.checkpoint_one_id != value.checkpoint_two_id && value.checkpoint_error == 0
                    && committedCheckpointIds.Contains(value.checkpoint_one_id) && committedCheckpointIds.Contains(value.checkpoint_two_id)
                    && view.Status.records_finalized == 1 && view.Status.checkpoint_error == 0
                    && view.Status.pending_requests == 0 && !view.Request.HasValue;
            }
        }

        RxCareerResult CheckpointWorker(IntPtr session, Command command)
        {
            if (command.generation != generation) return RxCareerResult.Stale;
            var status = RxCareerStatus.Create();
            var result = native.Status(session, generation, ref status);
            if (result != RxCareerResult.Ok) return result;
            if (status.generation != generation) return RxCareerResult.Stale;
            if (!committedCheckpointIds.Contains(command.id))
            {
            var request = RxCareerRequest.Create();
                result = native.Next(session, generation, ref request);
                if (result != RxCareerResult.Ok) return result;
                if (request.generation != generation || request.id != command.id) return RxCareerResult.OutOfOrder;
                if (request.kind != (uint)RxCareerRequestKind.Checkpoint) return RxCareerResult.WrongKind;
                if (request.delivered != 1 || request.state != 1) return RxCareerResult.NotDelivered;
                if (request.controller != status.controller) return RxCareerResult.Invalid;
                if (checkpoint == null) return RxCareerResult.Unsupported;
                // Bounded retry receipts: refuse before any new disk mutation.
                if (committedCheckpointIds.Count >= 32) return RxCareerResult.Full;
                try { checkpoint(); }
                catch (Exception error)
                {
                    lastCheckpointFailure = "Player progress was not saved: " + error.Message;
                    var failed = native.CheckpointFailed(session, generation, command.id);
                    if (failed == RxCareerResult.Fault) CloseWorker();
                    throw;
                }
                committedCheckpointIds.Add(command.id);
            }
            // Already committed IDs retry ONLY the native receipt, never export/CAS again.
            try { result = native.CheckpointCommitted(session, generation, command.id); }
            catch (Exception error)
            {
                lastCheckpointFailure = "Player progress was saved, but its core acknowledgement failed: " + error.Message + ". Retry acknowledgement without saving again.";
                checkpointFailure?.Invoke(lastCheckpointFailure);
                throw;
            }
            if (result == RxCareerResult.Ok || result == RxCareerResult.Repeat)
            {
                lastCheckpointFailure = null;
                checkpointFailure?.Invoke(null);
            }
            else
            {
                lastCheckpointFailure = "Player progress was saved, but its core acknowledgement failed: " + result + ". Retry acknowledgement without saving again.";
                checkpointFailure?.Invoke(lastCheckpointFailure);
            }
            return result;
        }

        // Called once before a frame job. Native request order remains authoritative.
        internal void CommandsWorker(IntPtr session)
        {
            Owner();
            for (int count = 0; count < 32; count++)
            {
                Command command;
                lock (gate) { if (closed || commands.Count == 0) return; command = commands.Dequeue(); }
                try
                {
                    RxCareerResult result;
                    if(command.action==ActionKind.Moto)
                        result=native is ICareerMotoWorkerNative moto ? moto.MotoAction(session,command.generation,command.id,command.motoAction,command.motoPhase,command.motoSub,command.motoCursor,command.motoGate) : RxCareerResult.Unsupported;
                    else if(command.action==ActionKind.Heat)
                        result=native is ICareerHeatWorkerNative heat ? heat.HeatAction(session,command.generation,command.id,command.motoAction,command.motoPhase,command.motoSub,command.motoCursor,command.motoGate) : RxCareerResult.Unsupported;
                    else if (command.action == ActionKind.Checkpoint)
                        result = CheckpointWorker(session, command);
                    else if (command.action == ActionKind.Ack)
                        result = native.Ack(session, command.generation, command.id, command.kind, command.accepted);
                    else if (command.action == ActionKind.Wipe)
                        result = native.Wipe(session, command.generation, command.id, command.name, command.milestone);
                    else if (command.action == ActionKind.Continue)
                        result = native.Continue(session, command.generation);
                    else if (command.action == ActionKind.CompleteEvent)
                        result = completion == null ? RxCareerResult.Unsupported : completion.CompleteEvent(session, command.generation, command.id);
                    else if (command.action == ActionKind.FrontendReady)
                        result = completion == null ? RxCareerResult.Unsupported : completion.FrontendReady(session, command.generation, command.id, command.routeId);
                    else if (command.action == ActionKind.CompleteSeries)
                        result = completion == null ? RxCareerResult.Unsupported : completion.CompleteSeries(session, command.generation, command.id);
                    else if (transitions == null) result = RxCareerResult.Unsupported;
                    else if (command.action == ActionKind.RulesAccept)
                        result = transitions.RulesAccept(session, command.generation, command.id, command.accepted);
                    else if (command.action == ActionKind.RulesEnded)
                        result = transitions.RulesPopupEnded(session, command.generation, command.id);
                    else if (command.action == ActionKind.LoadingEnter)
                        result = transitions.LoadingEnter(session, command.generation);
                    else if (command.action == ActionKind.LoadingHubEntered)
                        result = transitions.LoadingHubEntered(session, command.generation, command.id);
                    else result = RxCareerResult.Invalid;
                    command.completion.TrySetResult(result);
                    if (result == RxCareerResult.Fault) { CloseWorker(); throw new InvalidOperationException("The career core reported a fault."); }
                }
                catch (Exception error)
                {
                    command.completion.TrySetException(error);
                    if (command.action == ActionKind.Checkpoint)
                    {
                        // A refused durable commit leaves the native request pending for explicit retry.
                        lock (gate) { if (closed) return; }
                    }
                    else { CloseWorker(); throw; }
                }
            }
        }
        internal void RefreshWorker(IntPtr session)
        {
            Owner();
            lock (gate) { if (closed) return; }
            var status = RxCareerStatus.Create();
            Require(native.Status(session, generation, ref status), "status");
            if (status.generation != generation) throw new InvalidOperationException("Career snapshot belongs to another session.");
                MotoWorkerSnapshot motoSnapshot=null;
            if(launchIsMoto&&status.event_present==1&&native is ICareerMotoWorkerNative motoNative){var value=RxMotoStatus.Create();Require(motoNative.MotoStatus(session,generation,ref value),"Moto boundary");motoSnapshot=new MotoWorkerSnapshot(value,generation);
             if(motoSnapshot.Phase==6&&motoSnapshot.NextPhase==7&&motoSnapshot.SubEvent==0&&!motoSnapshot.Complete&&firstMoto==null){MotoScoredObservation.RequireFirstGenuineMoto(motoSnapshot.Scores);firstMoto=motoSnapshot.Scores;}
             if(motoSnapshot.Phase==7&&motoSnapshot.Complete&&secondMoto==null){MotoScoredObservation.RequireTwoGenuineMotos(firstMoto,motoSnapshot.Scores);secondMoto=motoSnapshot.Scores;}}

            HeatWorkerSnapshot heatSnapshot=null;
            if(launchIsHeat&&status.event_present==1&&native is ICareerHeatWorkerNative heatNative){
             // Unsupported: no started Heat event (yet); anything else must succeed.
             var value=RxHeatStatus.Create();var heatResult=heatNative.HeatStatus(session,generation,ref value);
             if(heatResult==RxCareerResult.Ok)heatSnapshot=new HeatWorkerSnapshot(value,generation);else if(heatResult!=RxCareerResult.Unsupported)Require(heatResult,"Heat boundary");}
            RxCareerCompletionStatus? completionSnapshot = null;
            if (completion != null)
            {
                var value = RxCareerCompletionStatus.Create();
                Require(completion.Completion(session, generation, ref value), "completion status");
                if (value.generation != generation || value.size != RxCareerCompletionStatus.ByteSize || value.version != 1)
                    throw new InvalidOperationException("Career completion snapshot belongs to another session or ABI.");
                completionSnapshot = value;
            }
            RxCareerTransitionStatus? transitionSnapshot = null;
            if (transitions != null)
            {
                var value = RxCareerTransitionStatus.Create();
                Require(transitions.Transitions(session, generation, ref value), "transition status");
                if (value.generation != generation || value.size != RxCareerTransitionStatus.ByteSize || value.version != 1)
                    throw new InvalidOperationException("Career transition snapshot belongs to another session or ABI.");
                transitionSnapshot = value;
            }
            var request = RxCareerRequest.Create();
            var result = native.Next(session, generation, ref request);
            if (result != RxCareerResult.Ok && result != RxCareerResult.Empty) Require(result, "presentation request");
            if (result == RxCareerResult.Ok && request.generation != generation) throw new InvalidOperationException("Career presentation belongs to another session.");
            lock (gate) { if (!closed) snapshot = new CareerSnapshot(status, result == RxCareerResult.Ok ? (RxCareerRequest?)request : null, lastCheckpointFailure, transitionSnapshot, completionSnapshot, motoSnapshot, firstMoto, secondMoto, heatSnapshot); }
        }
        // Invalidate before session destroy. No implicit animation or save acknowledgement.
        internal void CloseWorker()
        {
            if (owner != 0) Owner();
            lock (gate)
            {
                closed = true; snapshot = null; firstMoto=null;secondMoto=null;
                while (commands.Count > 0) commands.Dequeue().completion.TrySetResult(RxCareerResult.Inactive);
            }
        }
    }

    internal sealed class NativeCareerWorker : ICareerWorkerNative, ICareerTransitionWorkerNative, ICareerCompletionWorkerNative, ICareerVehicleWorkerNative, ICareerMotoWorkerNative, ICareerHeatWorkerNative
    {
        public RxCareerResult HeatStatus(IntPtr s,ulong g,ref RxHeatStatus v)=>RxCareerHeatNative.Status(s,g,ref v);
        public RxCareerResult HeatAction(IntPtr s,ulong g,ulong hub,uint a,uint p,uint next,uint cursor,int gate)=>RxCareerHeatNative.Action(s,g,hub,a,p,next,cursor,gate);
        public RxCareerResult MotoStatus(IntPtr s,ulong g,ref RxMotoStatus v)=>RxCareerMotoNative.Status(s,g,ref v);
        public RxCareerResult MotoAction(IntPtr s,ulong g,ulong hub,uint a,uint p,uint sub,uint cursor,int gate)=>RxCareerMotoNative.Action(s,g,hub,a,p,sub,cursor,gate);
        public RxCareerResult Configure(IntPtr s, ref RxCareerConfig c, out ulong g) => RxCareerNative.Configure(s, ref c, out g);
        public RxCareerResult Series(IntPtr s, ulong g, uint id, ref RxCareerSeriesInfo i) => RxCareerNative.Series(s, g, id, ref i);
        public RxCareerResult Load(IntPtr s, ulong g, uint id) => RxCareerNative.Load(s, g, id);
        public RxCareerResult Vehicle(IntPtr s, ulong g, ref RxCareerVehicleIdentity value) => RxCareerNative.Vehicle(s, g, ref value);
        public RxCareerResult SelectVehicle(IntPtr s, ulong g, uint type) => RxCareerNative.SelectVehicle(s, g, type);
        public RxCareerResult Initialize(IntPtr s, ulong g) => RxCareerNative.Initialize(s, g);
        public RxCareerResult Start(IntPtr s, ulong g) => RxCareerNative.Start(s, g);
        public RxCareerResult Status(IntPtr s, ulong g, ref RxCareerStatus value) => RxCareerNative.Status(s, g, ref value);
        public RxCareerResult Next(IntPtr s, ulong g, ref RxCareerRequest value) => RxCareerNative.Next(s, g, ref value);
        public RxCareerResult Ack(IntPtr s, ulong g, ulong id, RxCareerRequestKind kind, bool accepted) => RxCareerNative.Ack(s, g, id, kind, accepted);
        public RxCareerResult Wipe(IntPtr s, ulong g, ulong id, string name, uint milestone) => RxCareerNative.Wipe(s, g, id, name, milestone);
        public RxCareerResult Continue(IntPtr s, ulong g) => RxCareerNative.Continue(s, g);
        public RxCareerResult CheckpointCommitted(IntPtr s, ulong g, ulong id) => RxCareerCheckpointNative.Committed(s, g, id);
        public RxCareerResult CheckpointFailed(IntPtr s, ulong g, ulong id) => RxCareerCheckpointNative.Failed(s, g, id);
        public RxCareerResult Completion(IntPtr s, ulong g, ref RxCareerCompletionStatus value) => RxCareerCompletionNative.Status(s, g, ref value);
        public RxCareerResult CompleteEvent(IntPtr s, ulong g, ulong ticket) => RxCareerCompletionNative.CompleteEvent(s, g, ticket);
        public RxCareerResult FrontendReady(IntPtr s, ulong g, ulong ticket, ulong routeId) => RxCareerCompletionNative.FrontendReady(s, g, ticket, routeId);
        public RxCareerResult CompleteSeries(IntPtr s, ulong g, ulong ticket) => RxCareerCompletionNative.CompleteSeries(s, g, ticket);
        public RxCareerResult Transitions(IntPtr s, ulong g, ref RxCareerTransitionStatus value) => RxCareerTransitionsNative.Status(s, g, ref value);
        public RxCareerResult RulesAccept(IntPtr s, ulong g, ulong id, bool skip) => RxCareerTransitionsNative.RulesAccept(s, g, id, skip);
        public RxCareerResult RulesPopupEnded(IntPtr s, ulong g, ulong id) => RxCareerTransitionsNative.RulesPopupEnded(s, g, id);
        public RxCareerResult LoadingEnter(IntPtr s, ulong g) => RxCareerTransitionsNative.LoadingEnter(s, g);
        public RxCareerResult LoadingHubEntered(IntPtr s, ulong g, ulong id) => RxCareerTransitionsNative.LoadingHubEntered(s, g, id);
    }
}
