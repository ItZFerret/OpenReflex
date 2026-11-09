using System;
using System.Collections.Generic;
using System.Threading;

namespace Reflex.Port.OriginalWorld
{
    // Runs original frames (libreflexworld) on a dedicated thread so rendering never waits for the
    // simulation. The main thread submits input every frame: when the simulation is busy the input (and the real time
    // passed) is queued and the next original frame starts as soon as the current one ends, so a frame that takes
    // longer than the display interval does not halve the simulation rate. Completed results wait in `ready` until
    // collected; one not collected before the next completes is merged into it (audio commands, terrain patches and
    // track updates accumulate, the game time adds up). Only this thread calls into the plugin while it is running.
    public sealed class OriginalSimulation : IDisposable
    {
        public struct TerrainPatch { public int x0, z0, w, h; public float[] heights, deform; }

        public sealed class Results
        {
            public Reflex.Port.RxCore.ReplaySnapshot replay;
            public ulong nativeFrame;
            public ReflexBikeState bike = ReflexBikeState.Create();
            public Reflex.Port.RxCore.RxOnboardState onboardState;
            public bool raceLineClosed;
            public float[] riderPose = new float[OriginalRider.OriginalBoneCount * 16];
            public int riderBones;
            public ReflexRaceState race;
            public Reflex.Port.RxCore.RxEventUiState eventUi;
            public ReflexDriverResult[] drivers = new ReflexDriverResult[Reflex.Port.RxCore.RxRiderCap.Max]; public int driverCount;   // every rider's standing
            public float[] bikePose = new float[MaxSlotNodes * 16];   // MX_Hierarchy 18 nodes, ATV_Hierarchy 50
            public Reflex.Port.Content.ReflexVehicleInfo vehicle; public bool hasVehicle;   // reflex_world_vehicle (ReflexAtv.cs): the class, an ATV's six bodies
            public int bikeNodes;
            public ReflexCamera camera; public bool hasCamera;
            public float[] tracks; public int trackTriangles; public bool tracksChanged;
            public float[] objects = new float[0]; public bool[] objectsValid = new bool[0];
            public readonly List<ReflexAudioCommand> audio = new List<ReflexAudioCommand>();
            public float dt;                                        // game time this frame covered
            public ReflexHudState hud; public bool hasHud;
            public Reflex.Port.RxCore.RxWaypointState waypoint; public bool hasWaypoint;
            public Reflex.Port.RxCore.RxPowercrossHudState powercross; public bool hasPowercross;
            public Reflex.Port.RxCore.RxResultsPage resultsPage;    // the open path's results page table (rx_session_results_page), null before the page starts
            public Reflex.Port.RxCore.RxActivationState activation; public bool hasActivation;   // the open path's Free Ride activation area
            public Reflex.Port.RxCore.RxChallengeState challenge; public bool hasChallenge;       // the open path's challenge medal state
            public Reflex.Port.RxCore.RxCompassState compass; public bool hasCompass;             // the open path's HUD compass markers (rx_session_compass)
            public Reflex.Port.RxCore.RxRaceFormatState raceFormat; public bool hasRaceFormat;    // the open path's race format HUD values
            public Reflex.Port.RxCore.RxModeResultsState modeResults; public bool hasModeResults;   // the open path's Waypoint / Freestyle / Section Race standings' result values (rx_session_mode_results)
            public Reflex.Port.RxCore.RxRaceHudState raceHud; public bool hasRaceHud;             // the open path's Eliminator / Outrun / Section Race / Freestyle HUD values
            public Reflex.Port.RxCore.RxStuntHudState stuntHud; public bool hasStuntHud;           // the open path's stunt feedback overlay (trick names)
            public Reflex.Port.RxCore.RxReflexHudState reflexHud; public bool hasReflexHud;        // the open path's wreck-recovery arrows
            public readonly Reflex.Port.RxCore.RxVehicleAudioState vehicleAudio = new Reflex.Port.RxCore.RxVehicleAudioState();   // the open path's vehicle sound layer input (rx_session_vehicle_audio)
            public readonly Reflex.Port.RxCore.RxSoundState sound = new Reflex.Port.RxCore.RxSoundState();   // the open path's other sound managers' input (rx_session_sound, w-sfx)
            public float[] raceLine = new float[3 * 64]; public int raceLinePoints;
            public readonly List<TerrainPatch> terrain = new List<TerrainPatch>();
            public string error;
            public double milliseconds;
            // The other riders of the race (OriginalOpponents): per tracked slot its bike model nodes (the chassis package's
            // MX_Hierarchy) and its riding rider's 69 bones; riderBones 0 = not evaluated this frame (keep the last pose).
            public int slotCount; public int[] slots = new int[0];
            public float[][] slotBikePose = new float[0][], slotRiderPose = new float[0][];
            public int[] slotBikeNodes = new int[0], slotRiderBones = new int[0];
            // The vehicle model's node flags (ReflexVehicles.NodeFlags: bit 0x10 hidden), the local's and per tracked slot; 0: none read.
            public byte[] bikeNodeFlags = new byte[MaxSlotNodes]; public int bikeNodeFlagCount;
            public byte[][] slotNodeFlags = new byte[0][]; public int[] slotNodeFlagCounts = new int[0];
            public double slotMilliseconds;                         // what the slot pose exports cost on this thread
            // The original particles of this frame (OriginalParticles; reflex_world_particles), when attached.
            public ReflexParticle[] particles; public int particleCount; public ReflexParticleInfo particleInfo;
            // The original ambient animated entities of this frame (OriginalAmbient; reflex_world_ambient), when attached.
            public OriginalAmbient.Frame ambient;
            // The post-race presentation's cast (OriginalPodium; rx_session_podium), while the activity is in Post. Null / !valid otherwise.
            public OriginalPodium.Frame podium;
            // The static scenery models' drawn flags / instance matrices (OriginalSceneryModels; open core rx_session_scenario_models). Null / !valid otherwise.
            public OriginalSceneryModels.Frame sceneryModels;
            // Mud on every rider (reflex_world_mud, Content/ReflexMud.cs): [0] the local player, [1 + i] the tracked slots.
            public Reflex.Port.Content.ReflexMudState[] mud = new Reflex.Port.Content.ReflexMudState[17]; public int mudCount;
            // Presentation timeline (OriginalPresentation): the virtual clock this frame's state belongs to = the total time submitted up
            // to and including it (s, the main thread's clock); frames run for it (> 1: back to back; merged: one frame of several steps).
            public double simTime; public int framesRun; public bool merged;
            public double inputClock;                               // Clock when the input this frame ran with was read (input-to-display lag)
            // Opt-in pacing diagnostics. Stopwatch clocks are scheduling observations, not photon latency.
            public double frameStartClock, collectionClock;
            public string appliedPadCsv, corneringMetadata;

            public float gameDt, gameTime;                          // the game's own clock after it: Engine 0xd728d0 +0x18 dt, +0x3c time
            public double frameMilliseconds;                        // the original frame(s) alone (no exports)
            public double capWaitMilliseconds;                      // RealClock (open path): the frame's wait for the 1/60 s cap before it ran
            // LAP LOG (open path, rx_session_lap_log, w-lb-practice): the local human's laps logged in this result's frames (oldest first; every
            // record reaches one collected result once), and the local vehicle's physics class (rx_session_local_phys_class, -1 unknown)
            public readonly List<Reflex.Port.RxCore.RxLapRecord> laps = new List<Reflex.Port.RxCore.RxLapRecord>();
            public int physClass = -1;
        }

        readonly IntPtr world;
        readonly Thread thread;
        readonly AutoResetEvent go = new AutoResetEvent(false);
        readonly bool pacingDiagnostics = !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_PACING_LOG"));
        volatile bool stopping, busy, completed;
        float jobDt; readonly byte[] jobKeys = new byte[256]; readonly ReflexPad[] jobPads = new ReflexPad[4]; uint jobMask;
        Results front = new Results(), back = new Results(), ready = new Results();
        bool readyValid, queued; float queuedDt;
        // Whole-frame submission (SubmitFrames): jobFrames frames of jobDt each; requested = the main thread's clock (s), stamped on the
        // results. Results handed out by Collect belong to the caller until Recycle (owned = true: the caller keeps several for
        // interpolation; without Recycle the old triple-buffer reuse applies).
        int jobFrames = 1, queuedFrames; double requested, jobStamp, queuedStamp, jobInput, queuedInput; bool owned;
        /// The game's own clock (OriginalWorldRuntime's Original pacing): each frame measures its dt when it starts, as
        /// Engine::UpdateTiming 0084ea80 does with QueryPerformanceCounter: the real time since the last frame started, after waiting
        /// (spinning, as the original) until at least the frame cap engine+0x14 = 1/60 s has passed; the dt Submit passes is unused.
        /// A frame submitted while one runs starts right after it with the newest input (the original's next frame on a slow machine).
        public bool RealClock;
        public const double FrameCap = 1.0 / 60.0;
        double lastStart = double.NaN;
        /// RealClock: Clock when the last frame started (NaN: none since ResetClock).
        public double LastFrameStart { get { lock (gate) return lastStart; } }
        /// RealClock: the next frame starts the clock again (its dt is one cap): after a pause, which the game's clock must not see.
        public void ResetClock() { lock (gate) lastStart = double.NaN; }
        /// A monotonic clock (s) for input timestamps.
        public static double Clock => System.Diagnostics.Stopwatch.GetTimestamp() / (double)System.Diagnostics.Stopwatch.Frequency;
        readonly Stack<Results> pool = new Stack<Results>();
        double frameMsAverage = 2;                               // cost of one original frame (ms), smoothed
        readonly float[] engineScratch = new float[10];
        /// Back-to-back frames of one job (larger backlogs merge into one frame of up to MaxMergedSteps steps, the rest is dropped:
        /// the vehicle simulation does at most 6 fixed steps of 7.5 ms per update, so a merged 50 ms frame loses at most 5 ms of
        /// vehicle time, as the original does on a machine that cannot hold 60 Hz; the game clamps any frame at 0.1 s).
        public const int MaxSeparateFrames = 4, MaxMergedSteps = 3, MaxQueuedFrames = 15;
        readonly object gate = new object();
        ReflexTerrainInfo terrainInfo;
        readonly bool trackTerrain;
        public const int MaxTrackTriangles = 65536;
        uint tracksVersion; int framesSinceTracks;
        bool hasCameraExport = true, hasBikePoseExport = true, hasTracksExport = true, hasRaceLineExport = true, hasHudExport = true, hasSlotExports = true, hasDriversExport = true;
        // Riders tracked for OriginalOpponents (set from the main thread; read here once per frame).
        volatile int[] trackedSlots = new int[0];
        volatile bool[] slotRiderNear = new bool[0];
        int slotFrame;
        public const int MaxSlotNodes = 64;
        /// Every frame: the bike nodes of these slots; their riders' bones every frame when near (SetRiderDetail), else every
        /// farRiderInterval-th frame (each bone is completed by the game's own node evaluator: original code, interpreted).
        public void TrackSlots(int[] slots) { trackedSlots = slots ?? new int[0]; slotRiderNear = new bool[trackedSlots.Length]; for (int i = 0; i < slotRiderNear.Length; i++) slotRiderNear[i] = true; }
        public void SetRiderDetail(bool[] near) { if (near != null && near.Length == trackedSlots.Length) slotRiderNear = near; }
        public int farRiderInterval = 3;
        // Animated original objects (OriginalAnimatedObjects): located on the first frame, then read every frame.
        OriginalAnimatedObjects.Entry[] objectEntries = new OriginalAnimatedObjects.Entry[0];
        uint[] objectAddresses; readonly float[] objectScratch = new float[16];
        // Engine sound client (OriginalEngineAudio): commands out, finished one-shots in.
        System.Collections.Concurrent.ConcurrentQueue<(uint wrapper, uint serial)> audioFinished;
        const int AudioScratchCommands = 256; readonly byte[] audioScratch = new byte[AudioScratchCommands * ReflexAudioCommand.NativeSize];
        public void AttachAudio(System.Collections.Concurrent.ConcurrentQueue<(uint, uint)> finished)
        {
            try { if (ReflexWorldNative.reflex_world_audio_enable(world, 1) == 0) audioFinished = finished; }
            catch (EntryPointNotFoundException) { audioFinished = null; }
        }
        public void TrackObjects(OriginalAnimatedObjects.Entry[] entries) { objectEntries = entries ?? new OriginalAnimatedObjects.Entry[0]; }
        // Original particles (OriginalParticles): the managers keep running with ambient animation off; exported every frame.
        volatile bool particlesOn;
        public bool AttachParticles()
        {
            try { particlesOn = ReflexParticlesNative.reflex_world_particles_enable(world, 1) == 0; }
            catch (EntryPointNotFoundException) { particlesOn = false; }
            return particlesOn;
        }

        // Ambient animated entities (OriginalAmbient): exported every frame once attached.
        volatile bool ambientOn;
        public void AttachAmbient() => ambientOn = true;

        // Calls into the running world between two of its frames, on this thread (MOTOCAREER's in-race pages: OriginalCareerRace).
        readonly System.Collections.Concurrent.ConcurrentQueue<Action<IntPtr>> betweenFrames = new System.Collections.Concurrent.ConcurrentQueue<Action<IntPtr>>();
        /// Runs `call` (given the world) on the simulation thread before its next frame.
        public void BetweenFrames(Action<IntPtr> call) { if (call != null) betweenFrames.Enqueue(call); }

        public OriginalSimulation(IntPtr world, bool trackTerrain)
        {
            this.world = world; this.trackTerrain = trackTerrain;
            macQos = HighQos && (UnityEngine.Application.platform == UnityEngine.RuntimePlatform.OSXEditor || UnityEngine.Application.platform == UnityEngine.RuntimePlatform.OSXPlayer);
            if (trackTerrain) ReflexWorldNative.reflex_world_terrain(world, ref terrainInfo);
            thread = new Thread(Loop) { IsBackground = true, Name = "Reflex original simulation", Priority = ThreadPriority.AboveNormal };
            thread.Start();
        }

        public bool Idle => !busy;

        /// Waits until no frame is running or queued (at most timeoutMs); true when idle.
        public bool WaitIdle(float timeoutMs)
        {
            var watch = System.Diagnostics.Stopwatch.StartNew();
            lock (gate)
                while (busy)
                {
                    int left = (int)Math.Ceiling(timeoutMs - watch.Elapsed.TotalMilliseconds);
                    if (left <= 0 || stopping) return false;
                    Monitor.Wait(gate, left);
                }
            return true;
        }

        // Queues one original frame; false while the previous one is still running.
        // Queues one original frame (the input of the next frame and the real time it covers); always accepted.
        public bool Submit(float dt, byte[] keys, ReflexPad[] pads, uint mask)
        {
            lock (gate)
            {
                requested += dt;
                if (busy)
                {
                    queuedDt = Math.Min(queuedDt + dt, 0.25f); queued = true; queuedFrames = 1; queuedStamp = requested; queuedInput = Clock;
                    Buffer.BlockCopy(keys, 0, queuedKeys, 0, 256); Array.Copy(pads, queuedPads, 4); queuedMask = mask;
                    return true;
                }
                jobDt = dt; jobFrames = 1; jobStamp = requested; jobInput = Clock; Buffer.BlockCopy(keys, 0, jobKeys, 0, 256); Array.Copy(pads, jobPads, 4); jobMask = mask;
                busy = true;
            }
            go.Set();
            return true;
        }
        readonly byte[] queuedKeys = new byte[256]; readonly ReflexPad[] queuedPads = new ReflexPad[4]; uint queuedMask;

        /// Queues `frames` whole original frames of `step` seconds each (the game's own 1/60 s cadence: it floors its frame time at
        /// 1/60 s, so shorter frames would run it fast) with the latest input; frames requested while one runs wait and start right
        /// after it (back to back while the simulation keeps up, else merged). The results are stamped with the requested clock.
        public void SubmitFrames(int frames, float step, byte[] keys, ReflexPad[] pads, uint mask)
        {
            if (frames <= 0) return;
            lock (gate)
            {
                requested += frames * (double)step;
                if (busy)
                {
                    queuedFrames = Math.Min(queuedFrames + frames, MaxQueuedFrames); queuedDt = step; queuedStamp = requested; queuedInput = Clock; queued = true;
                    Buffer.BlockCopy(keys, 0, queuedKeys, 0, 256); Array.Copy(pads, queuedPads, 4); queuedMask = mask;
                    return;
                }
                jobFrames = frames; jobDt = step; jobStamp = requested; jobInput = Clock;
                Buffer.BlockCopy(keys, 0, jobKeys, 0, 256); Array.Copy(pads, jobPads, 4); jobMask = mask;
                busy = true;
            }
            go.Set();
        }
        /// The clock SubmitFrames has requested so far (s).
        public double Requested { get { lock (gate) return requested; } }

        // The results of the frames completed since the last call (null if none).
        public Results Collect()
        {
            lock (gate)
            {
                if (!readyValid) return null;
                readyValid = false;
                var r = ready;
                if (owned) ready = pool.Count > 0 ? pool.Pop() : new Results();   // r stays the caller's until Recycle
                else { ready = front; front = r; }
                return r;
            }
        }
        /// Keeps collected results until Recycle (several held at once: OriginalPresentation interpolates between two).
        public void KeepResults() { lock (gate) owned = true; }
        /// Hands collected results back for reuse.
        public void Recycle(Results r) { if (r != null) lock (gate) if (owned && pool.Count < 8) pool.Push(r); }

        // a completed frame: older uncollected results merge into it; then the next queued frame starts at once
        bool Complete(Results r)
        {
            lock (gate)
            {
                if (readyValid)
                {
                    var o = ready;
                    r.audio.InsertRange(0, o.audio); r.terrain.InsertRange(0, o.terrain); r.dt += o.dt;
                    if (o.tracksChanged && !r.tracksChanged) { (r.tracks, o.tracks) = (o.tracks, r.tracks); r.trackTriangles = o.trackTriangles; r.tracksChanged = true; }
                    if (r.error == null) r.error = o.error;
                }
                back = ready; ready = r; readyValid = true; completed = true;
                if (!queued) { busy = false; Monitor.PulseAll(gate); return false; }
                jobDt = queuedDt; Buffer.BlockCopy(queuedKeys, 0, jobKeys, 0, 256); Array.Copy(queuedPads, jobPads, 4); jobMask = queuedMask;
                jobFrames = Math.Max(1, queuedFrames); jobStamp = queuedStamp; jobInput = queuedInput;
                queued = false; queuedDt = 0; queuedFrames = 0;
                return true;
            }
        }

        // macOS: a managed thread starts at the default QoS, which Apple Silicon may run on its efficiency cores (the original frames
        // took ~1.7x longer there in the editor: 16.6 ms against 9 ms for a 12-rider National); user-interactive keeps it on the
        // performance cores like the main and render threads.
        [System.Runtime.InteropServices.DllImport("/usr/lib/libSystem.dylib")] static extern int pthread_set_qos_class_self_np(int qos, int relativePriority);
        const int QosUserInteractive = 0x21;
        readonly bool macQos;
        public static bool HighQos = Environment.GetEnvironmentVariable("REFLEX_SIM_QOS") != "default";

        void Loop()
        {
            if (macQos) try { pthread_set_qos_class_self_np(QosUserInteractive, 0); } catch (Exception) { }
            bool next = false;
            while (true)
            {
                if (!next) go.WaitOne();
                if (stopping) return;
                var r = back; r.error = null; r.terrain.Clear(); r.audio.Clear();
                var watch = System.Diagnostics.Stopwatch.StartNew();
                try
                {
                    while (betweenFrames.TryDequeue(out var call)) { try { call(world); } catch (Exception e) { UnityEngine.Debug.LogWarning("between frames: " + e.Message); } }
                    if (audioFinished != null) while (audioFinished.TryDequeue(out var f)) ReflexWorldNative.reflex_world_audio_finished(world, f.wrapper, f.serial);
                    // jobFrames whole frames of jobDt: back to back while they fit in about one frame period at the measured cost, else one
                    // frame covering MaxMergedSteps of them (the original on a slower machine: a longer measured frame); the rest dropped.
                    int frames = jobFrames, calls = frames; float dt = jobDt; double stamp = jobStamp;
                    if (RealClock) { frames = calls = 1; dt = StartRealFrame(out stamp); }
                    else if (frames > 1 && (frames > MaxSeparateFrames || frameMsAverage * frames > 0.8 * 1000.0 * jobDt)) { calls = 1; dt = jobDt * Math.Min(frames, MaxMergedSteps); }
                    r.framesRun = calls; r.merged = calls == 1 && frames > 1; r.simTime = stamp; r.inputClock = jobInput; r.dt = 0;
                    r.frameStartClock = Clock; r.collectionClock = double.NaN;
                    if (pacingDiagnostics)
                    {
                        var pad = jobPads[0];
                        r.appliedPadCsv = FormattableString.Invariant($"{jobMask},{pad.lx},{pad.ly},{pad.rx},{pad.ry},{pad.leftTrigger},{pad.rightTrigger},{pad.buttons}");
                    }
                    int code = 0; var frameWatch = System.Diagnostics.Stopwatch.StartNew();
                    for (int c = 0; c < calls && code == 0; c++)
                    {
                        code = ReflexWorldNative.reflex_world_frame(world, dt, jobKeys, jobMask != 0 ? jobPads : null, jobMask);
                        r.dt += dt;
                        if (code == 0 && audioFinished != null)
                        {
                            int n;
                            while ((n = ReflexWorldNative.reflex_world_audio_raw(world, audioScratch, AudioScratchCommands)) > 0)
                                for (int i = 0; i < n; i++) r.audio.Add(ReflexAudioCommand.Decode(audioScratch, i));
                        }
                    }
                    r.frameMilliseconds = frameWatch.Elapsed.TotalMilliseconds;
                    frameMsAverage += (r.frameMilliseconds / calls - frameMsAverage) * 0.1;
                    if (code == 0 && ReflexWorldNative.reflex_world_read_floats(world, 0xd728d0 + 0x18, engineScratch, 40) == 0) { r.gameDt = engineScratch[0]; r.gameTime = engineScratch[9]; }
                    if (code != 0) r.error = "reflex_world_frame returned " + code + ": " + ReflexWorldNative.Error(world, "unknown");
                    else if ((code = ReflexWorldNative.reflex_world_bike(world, ref r.bike)) != 0) r.error = "reflex_world_bike returned " + code + ": " + ReflexWorldNative.Error(world, "unknown");
                    else
                    {
                        r.riderBones = ReflexWorldNative.reflex_world_rider_pose(world, r.riderPose, (uint)OriginalRider.OriginalBoneCount);
                        ReflexWorldNative.reflex_world_race(world, ref r.race);
                        if (hasDriversExport) try { r.driverCount = Math.Max(0, ReflexWorldNative.reflex_world_race_drivers(world, r.drivers, (uint)r.drivers.Length)); } catch (EntryPointNotFoundException) { hasDriversExport = false; r.driverCount = 0; }
                        // Optional exports: an older plugin (editor not restarted since an update) lacks them.
                        if (hasHudExport) try { r.hasHud = ReflexWorldNative.reflex_world_hud(world, ref r.hud) == 0 && r.hud.valid != 0; } catch (EntryPointNotFoundException) { hasHudExport = false; r.hasHud = false; }
                        if (hasRaceLineExport) try { r.raceLinePoints = Math.Max(0, ReflexWorldNative.reflex_world_race_line(world, r.raceLine, (uint)(r.raceLine.Length / 3))); } catch (EntryPointNotFoundException) { hasRaceLineExport = false; r.raceLinePoints = 0; }
                        if (hasCameraExport) try { r.hasCamera = ReflexWorldNative.reflex_world_camera(world, ref r.camera) == 0; } catch (EntryPointNotFoundException) { hasCameraExport = false; r.hasCamera = false; }
                        if (hasBikePoseExport) try { r.bikeNodes = ReflexWorldNative.reflex_world_bike_pose(world, r.bikePose, (uint)MaxSlotNodes); } catch (EntryPointNotFoundException) { hasBikePoseExport = false; r.bikeNodes = 0; }
                        r.hasVehicle = Reflex.Port.Content.ReflexAtvNative.Read(world, -1, ref r.vehicle);
                        r.bikeNodeFlagCount = r.hasVehicle ? Reflex.Port.Content.ReflexVehicles.NodeFlags(world, r.vehicle.vehicle, r.bikeNodeFlags, flagScratch) : 0;
                        if (trackTerrain) { CollectTerrain(r); CollectTracks(r); }
                        CollectObjects(r);
                        CollectSlots(r);
                        r.mudCount = Reflex.Port.Content.ReflexMudNative.Collect(world, trackedSlots, r.mud);
                        if (particlesOn) try { OriginalParticles.Collect(world, ref r.particles, ref r.particleInfo, out r.particleCount); } catch (EntryPointNotFoundException) { particlesOn = false; r.particleCount = 0; }
                        if (ambientOn) try { r.ambient ??= new OriginalAmbient.Frame(); OriginalAmbient.Collect(world, r.ambient); } catch (EntryPointNotFoundException) { ambientOn = false; }
                    }
                }
                catch (Exception e) { r.error = e.Message; }
                r.milliseconds = watch.Elapsed.TotalMilliseconds;
                next = Complete(r);
            }
        }

        // RealClock: the frame cap and the dt of the frame starting now; the results are stamped with the game time so far.
        float StartRealFrame(out double stamp)
        {
            double last; lock (gate) last = lastStart;
            double t = Clock, dt = FrameCap;
            if (!double.IsNaN(last))
            {
                while (t - last < FrameCap) { Thread.SpinWait(32); t = Clock; }
                dt = Math.Min(t - last, 0.25);
            }
            lock (gate) { lastStart = t; requested += dt; stamp = requested; }
            return (float)dt;
        }

        void CollectSlots(Results r)
        {
            var slots = trackedSlots; var near = slotRiderNear;
            r.slotCount = 0; r.slotMilliseconds = 0;
            if (!hasSlotExports || slots.Length == 0) return;
            var watch = System.Diagnostics.Stopwatch.StartNew();
            int n = slots.Length; slotFrame++;
            if (r.slots.Length != n)
            {
                r.slots = new int[n]; r.slotBikeNodes = new int[n]; r.slotRiderBones = new int[n];
                r.slotBikePose = new float[n][]; r.slotRiderPose = new float[n][];
                r.slotNodeFlags = new byte[n][]; r.slotNodeFlagCounts = new int[n]; for (int i = 0; i < n; i++) r.slotNodeFlags[i] = new byte[MaxSlotNodes];
                for (int i = 0; i < n; i++) { r.slotBikePose[i] = new float[MaxSlotNodes * 16]; r.slotRiderPose[i] = new float[OriginalRider.OriginalBoneCount * 16]; }
            }
            try
            {
                for (int i = 0; i < n; i++)
                {
                    r.slots[i] = slots[i];
                    r.slotBikeNodes[i] = ReflexWorldNative.reflex_world_bike_pose_slot(world, slots[i], r.slotBikePose[i], MaxSlotNodes);
                    r.slotNodeFlagCounts[i] = Reflex.Port.Content.ReflexVehicles.NodeFlags(world, SlotVehicle(slots[i]), r.slotNodeFlags[i], flagScratch);
                    bool due = i >= near.Length || near[i] || farRiderInterval <= 1 || (slotFrame + i) % farRiderInterval == 0;
                    r.slotRiderBones[i] = due ? ReflexWorldNative.reflex_world_rider_pose_slot(world, slots[i], r.slotRiderPose[i], (uint)OriginalRider.OriginalBoneCount) : 0;
                }
                r.slotCount = n;
            }
            catch (EntryPointNotFoundException) { hasSlotExports = false; r.slotCount = 0; }
            r.slotMilliseconds = watch.Elapsed.TotalMilliseconds;
        }

        readonly byte[] flagScratch = new byte[MaxSlotNodes * 16];
        /// A slot's vehicle V = [P+0x1ac], P = [0xa946b8] + 0x7460 + slot * 0x12a0 (0: none).
        uint SlotVehicle(int slot)
        {
            uint U(uint a) => ReflexWorldNative.reflex_world_read(world, a, flagScratch, 4) == 0 ? BitConverter.ToUInt32(flagScratch, 0) : 0u;
            uint g = U(0xa946b8); return g == 0 || slot < 0 || slot >= 16 ? 0u : U(g + 0x7460 + (uint)slot * 0x12a0 + 0x1ac);
        }

        void CollectObjects(Results r)
        {
            int n = objectEntries.Length;
            if (n == 0) return;
            if (objectAddresses == null)
            {
                objectAddresses = new uint[n];
                for (int i = 0; i < n; i++)
                {
                    var e = objectEntries[i];
                    try { objectAddresses[i] = ReflexWorldNative.reflex_world_find_object(world, e.vtable, e.idOffset, e.id, 0x00400000, 0x40000000); }
                    catch (EntryPointNotFoundException) { objectAddresses[i] = 0; }
                    if (objectAddresses[i] == 0) UnityEngine.Debug.LogWarning($"Original object {e.vtable:x} #{e.id} not found; it stays static.");
                }
            }
            if (r.objects.Length != n * 16) { r.objects = new float[n * 16]; r.objectsValid = new bool[n]; }
            for (int i = 0; i < n; i++)
            {
                uint a = objectAddresses[i];
                r.objectsValid[i] = a != 0 && ReflexWorldNative.reflex_world_read_floats(world, a + objectEntries[i].matrixOffset, objectScratch, 64) == 0;
                if (r.objectsValid[i]) Array.Copy(objectScratch, 0, r.objects, i * 16, 16);
            }
        }

        // Tyre-track ribbons: re-read when the game flushed new vertices (at most every 4th frame).
        void CollectTracks(Results r)
        {
            r.tracksChanged = false; framesSinceTracks++;
            if (!hasTracksExport) return;
            uint version;
            try { if (ReflexWorldNative.reflex_world_tracks(world, null, 0, out version) < 0 || version == tracksVersion || framesSinceTracks < 4) return; }
            catch (EntryPointNotFoundException) { hasTracksExport = false; return; }
            if (r.tracks == null) r.tracks = new float[MaxTrackTriangles * 18];
            int n = ReflexWorldNative.reflex_world_tracks(world, r.tracks, MaxTrackTriangles, out version);
            if (n < 0) return;
            r.trackTriangles = n; r.tracksChanged = true; tracksVersion = version; framesSinceTracks = 0;
        }

        void CollectTerrain(Results r)
        {
            uint previous = terrainInfo.deform_version;
            if (ReflexWorldNative.reflex_world_terrain(world, ref terrainInfo) != 0 || terrainInfo.deform_version == previous) return;
            int n = (int)terrainInfo.samples;
            int x0 = Math.Max(0, terrainInfo.dirty_x0), x1 = Math.Min(n, terrainInfo.dirty_x1);
            int z0 = Math.Max(0, terrainInfo.dirty_z0), z1 = Math.Min(n, terrainInfo.dirty_z1);
            if (x1 <= x0 || z1 <= z0) return;
            // pooled (OriginalTerrain.ApplyPatch returns them): rented arrays may be longer than w * h
            var patch = new TerrainPatch { x0 = x0, z0 = z0, w = x1 - x0, h = z1 - z0, heights = Reflex.Port.RxCore.TerrainPatchArrays.Rent((x1 - x0) * (z1 - z0)) };
            if (ReflexWorldNative.reflex_world_terrain_heights(world, patch.heights, x0, z0, patch.w, patch.h) != 0) { Reflex.Port.RxCore.TerrainPatchArrays.Return(patch.heights); return; }
            int dx1 = Math.Min(x1, n), dz1 = Math.Min(z1, n);
            if (dx1 == x1 && dz1 == z1)
            {
                patch.deform = Reflex.Port.RxCore.TerrainPatchArrays.Rent(patch.w * patch.h);
                if (ReflexWorldNative.reflex_world_terrain_deform(world, patch.deform, x0, z0, patch.w, patch.h) != 0) { Reflex.Port.RxCore.TerrainPatchArrays.Return(patch.deform); patch.deform = null; }
            }
            r.terrain.Add(patch);
        }

        public void Dispose()
        {
            stopping = true; go.Set();
            if (!thread.Join(5000)) UnityEngine.Debug.LogWarning("Original simulation thread did not stop in time.");
            go.Dispose();
        }
    }
}
