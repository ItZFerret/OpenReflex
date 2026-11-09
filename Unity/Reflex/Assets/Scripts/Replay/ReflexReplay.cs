// REPLAY and PHOTO MODE (an OPEN REFLEX extra, ported from Open Unleashed's MxaReplay): the last REPLAY LENGTH seconds of the ride,
// always recorded, watched back with a director's cameras or frozen on any moment for a photo. Separate from the original game's own
// end-of-race replay (OwnReplayRenderer, the results page's HIGHLIGHT REEL), which this never touches.
//
// Recording (LateUpdate, after everything that poses the models and places the camera; the core's worker threads are not read: only
// the Unity transforms the runtime set from the presented frame, as the renderer sees them): one ReplayFrame (ReflexReplayBuffer) per
// game frame shown, keyed by the game's own clock (OriginalWorldRuntime.PresentedGameTime), so it stops with the game (pause) and the
// frames line up with the presented (blended) poses. Actors: the local rider (bike chassis, wheels, MX_Hierarchy parts, the rider's
// root and 69 bones, the capsule stand-in), every other rider of the race (OriginalOpponents: the same per slot), the scenery the game
// moves (OriginalAnimatedObjects: the start gate). Also the game camera, the riders' places / laps, and with REPLAY EFFECTS the frame's
// particles (OriginalParticles.Applied). It records from the countdown to the finish (not the intro / podium scenarios, the post-race
// pages or the original's own replay). The terrain's ruts, the mud on the riders and the crowds show the live moment's state.
//
// Viewing (Open): the world stays where it was (OriginalWorldRuntime.ViewerActive: paused, its camera and HUD hold off, the post-race
// frames wait), every frame LateUpdate poses the actors at PlayTime and places the camera (ReflexReplay.Cameras.cs). The live moment
// (every transform, flag, the particles and the camera) is snapshot exactly at Open and put back at Close, which returns where the
// viewer was opened from: the ride, the pause page or the results page. Time.timeScale follows the playback (0 when frozen: the
// water, grass and sky shaders hold still for a photo).
//
// Two faces, one timeline: REPLAY (transport, camera modes, the rider to follow, camera keyframes) and PHOTO MODE (the moment frozen,
// a free / orbit camera, lens, colour and frame settings, the OPEN REFLEX logo, the shot saved as a PNG: ReflexReplay.Photo.cs). The
// UI is ReflexReplayUi (Resources/OpenFrontEnd/OuReplay.uss, the OPEN REFLEX menu look).
// Opened from the pause page (REPLAY / PHOTO MODE rows: OwnRacePause, ReflexPauseExtras), the results page (REPLAY: OwnRaceResults)
// or while riding (R / P when the game's key map leaves them free). Diagnostics (players): REFLEX_REPLAY_PROBE (ReflexReplayProbe).
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.Replay
{
    [DefaultExecutionOrder(29000), DisallowMultipleComponent]   // records after every pose / camera pass; plays back after them too
    public sealed partial class ReflexReplay : MonoBehaviour
    {
        public enum View { Replay, Photo }
        public enum Origin { Live, Pause, Results }

        public OriginalWorldRuntime Runtime { get; private set; }
        public bool Active { get; private set; }
        public View Face { get; private set; }
        public Origin OpenedFrom { get; private set; }
        // The recording's rings (poses, particles, terrain deltas, sound commands) are kept from one world to the next (Storage): each race
        // made them again (the particle ring alone up to 192 MB in one allocation, the pose frames up to 512 MB as the ring filled), and
        // with the old world's still waiting for a collection the managed heap took both and never gave the space back (w-gcspikes).
        sealed class Storage
        {
            public readonly ReflexReplayBuffer buffer = new ReflexReplayBuffer();
            public readonly ReplayParticleRing particles = new ReplayParticleRing();
            public readonly TerrainRing terrain = new TerrainRing();
            public readonly AudioRing sound = new AudioRing();
            public float[] frozenH = Array.Empty<float>(), frozenD = Array.Empty<float>();   // the terrain as the recording stopped (ReflexReplay.Terrain)
            static Storage s_free;   // one set kept (a second world at the same time, the load arena's, makes its own)
            public static Storage Take() { var s = s_free; s_free = null; return s ?? new Storage(); }
            public static void Give(Storage s) { if (s == null) return; s.buffer.Clear(); s.particles.Clear(); s.terrain.Clear(); s.sound.Clear(); s_free ??= s; }
        }
        Storage storage;
        Storage Rings => storage ??= Storage.Take();
        public ReflexReplayBuffer Buffer => Rings.buffer;
        public ReplayParticleRing Particles => Rings.particles;
        /// The replay clock of the moment shown (s, Buffer.StartTime .. EndTime).
        public float PlayTime { get; private set; }
        public bool Playing { get; private set; }
        /// Playback speed (negative = backwards).
        public float Speed { get; private set; } = 1f;
        /// The rider followed: 0 the local rider, 1..n another rider (actor index).
        public int Target { get; private set; }
        public bool UiHidden { get; set; }
        /// The moment shown is the live one.
        public bool AtLive => Buffer.Count > 0 && PlayTime >= Buffer.EndTime - 0.004f;
        public ReflexReplayUi Ui { get; private set; }

        /// A viewer is open (any world): the pause / results pages leave the input alone.
        public static bool ViewerOpen { get; private set; }
        /// The frame a viewer closed (the key that closed it is not taken again by the page under it).
        public static int ClosedFrame { get; private set; } = -1;

        public static readonly float[] Speeds = { 0.05f, 0.1f, 0.25f, 0.5f, 1f, 2f, 4f };

        // ------------------------------------------------------------------------------------------- options

        /// REPLAY LENGTH (OPTIONS > OPEN REFLEX), seconds kept (a field that would not fit the memory budget keeps 30 frames a second).
        public static int LengthSeconds
        {
            get => Mathf.Clamp(ReflexSave.GetInt("OpenReflex.ReplaySeconds", 120), 15, 600);
            set { ReflexSave.SetInt("OpenReflex.ReplaySeconds", Mathf.Clamp(value, 15, 600)); ReflexSave.Save(); }
        }
        public static readonly int[] LengthChoices = { 30, 60, 120, 180, 300 };
        public static string LengthLabel(int s) => s < 60 ? s + " SECONDS" : (s / 60f).ToString("0.#") + " MINUTE" + (s > 60 ? "S" : "");

        /// REPLAY EFFECTS: the particles (dust, roost, splashes) are recorded too.
        public static bool RecordEffects
        {
            get => ReflexSave.GetInt("OpenReflex.ReplayEffects", 1) != 0;
            set { ReflexSave.SetInt("OpenReflex.ReplayEffects", value ? 1 : 0); ReflexSave.Save(); }
        }

        /// Bytes the poses may take: an eighth of the machine's memory, 128 .. 512 MB (the particles: half of that, 48 .. 192 MB).
        static long MemoryBudget => Math.Clamp(SystemInfo.systemMemorySize / 8L, 128L, 512L) * 1024L * 1024L;
        static int ParticleBudget => (int)(Math.Clamp(MemoryBudget / 2, 48L << 20, 192L << 20) / (ReplayParticleRing.Words * 2));

        public static ReflexReplay Of(OriginalWorldRuntime rt) => rt ? rt.GetComponent<ReflexReplay>() : null;

        /// The pause / results pages offer REPLAY / PHOTO MODE (a recording to show).
        public static bool Offered(OriginalWorldRuntime rt) { var r = Of(rt); return r && r.Buffer.Count > 1; }

        void Awake() { Runtime = GetComponent<OriginalWorldRuntime>(); }

        /// The viewer can open now.
        public bool CanOpen => Runtime && Runtime.Running && !Active && !ReflexOptions.IsOpen && ReflexOptions.ClosedFrame != Time.frameCount;

        // ============================================================================================= recording

        sealed class Actor
        {
            public string name;
            public int slot = -1;
            public bool local, props;
            public Transform[] full = Array.Empty<Transform>(), packed = Array.Empty<Transform>();
            /// Per packed transform: where its node's origin is in the transform's own space (a bike part's mesh is in model space: the wheel's axle is
            /// feet from the transform's origin). Zero for the rider's bones, whose transforms are at their joints. See ApplyPose.
            public Vector3[] pivots = Array.Empty<Vector3>();
            public int[] fullOrder = Array.Empty<int>();   // the full transforms by depth (parents first): the order they are posed in
            public GameObject[] flags = Array.Empty<GameObject>();
            public Renderer[] bikeRenderers = Array.Empty<Renderer>(), riderRenderers = Array.Empty<Renderer>();
            public OriginalRider rider;
            public float halfWidth = 1f;
            public ReflexBikeModel mudBike; public ReflexRiderModel mudRider;   // (ReflexMud.Applied: the models its mud goes on)
            public ReplayActorLayout layout;
        }

        readonly List<Actor> actors = new List<Actor>();
        long generation = long.MinValue;
        float clock, lastSample = -1f, lastGameTime = float.NaN, layoutCheckAt;
        bool layoutDirty = true;
        int opponentCount = -1;

        public int ActorCount => actors.Count;

        /// A new session (spawn, restart, the next race): the recording starts over.
        public void SessionReset()
        {
            if (Active) Close();
            layoutDirty = true;
            PackWait();
            Buffer.Clear(); Particles.Clear(); Terrain.Clear(); terrainPending = 0; Sound.Clear(); soundPending = 0;
            keys.Clear();
            clock = 0f; lastSample = -1f; lastGameTime = float.NaN; prevParticleTime = -1f;
            paths.Clear();
        }

        static int Depth(Transform t) { int d = 0; for (var p = t.parent; p; p = p.parent) d++; return d; }

        static Transform[] DepthSorted(List<Transform> list)
        {
            var seen = new HashSet<Transform>();
            var keep = new List<(Transform t, int d, int i)>();
            for (int i = 0; i < list.Count; i++) if (list[i] && seen.Add(list[i])) keep.Add((list[i], Depth(list[i]), i));
            keep.Sort((a, b) => a.d != b.d ? a.d.CompareTo(b.d) : a.i.CompareTo(b.i));
            var r = new Transform[keep.Count];
            for (int i = 0; i < r.Length; i++) r[i] = keep[i].t;
            return r;
        }

        static int[] DepthOrder(Transform[] list)
        {
            var order = new int[list.Length];
            for (int i = 0; i < order.Length; i++) order[i] = i;
            var depth = new int[list.Length];
            for (int i = 0; i < list.Length; i++) depth[i] = list[i] ? Depth(list[i]) : 0;
            Array.Sort(order, (x, y) => depth[x] != depth[y] ? depth[x].CompareTo(depth[y]) : x.CompareTo(y));
            return order;
        }

        static Actor MakeRider(string name, int slot, bool local, Transform holder, Transform chassis, Transform front, Transform rear, Transform proxy,
                               OriginalBikePose pose, OriginalRider rider)
        {
            var a = new Actor { name = name, slot = slot, local = local, rider = rider };
            // the anchor (the actor's position and heading for the cameras): the chassis where the runtime moves it (the local rider:
            // OriginalWorldRuntime.Apply); another rider's chassis stays at its holder's origin and only the pose parts move, so its anchor
            // is the part of the hierarchy's root node (the frame: the bike's world matrix times the part's inverse bind)
            Transform anchor = chassis;
            if (!local && pose && pose.parts != null && pose.nodes != null)
            {
                int best = int.MaxValue;
                for (int i = 0; i < pose.parts.Length && i < pose.nodes.Length; i++)
                    if (pose.parts[i] && pose.nodes[i] < best) { best = pose.nodes[i]; anchor = pose.parts[i]; }
            }
            var full = new List<Transform> { anchor, chassis, front, rear, proxy, rider ? rider.transform : null };
            full.RemoveAll(t => !t);
            for (int i = full.Count - 1; i > 0; i--) if (full.IndexOf(full[i]) < i) full.RemoveAt(i);
            var fullSet = new HashSet<Transform>(full);
            var packed = new List<Transform>();
            if (pose && pose.parts != null) foreach (var p in pose.parts) if (p && !fullSet.Contains(p)) packed.Add(p);
            if (rider && rider.bones != null) foreach (var b in rider.bones) if (b && !fullSet.Contains(b)) packed.Add(b);
            a.full = full.ToArray();   // (the anchor first)
            a.fullOrder = DepthOrder(a.full);
            a.packed = DepthSorted(packed);
            // the pivots of the bike's parts (OriginalBikePose.Apply: part = liveWorld(node) * inverse(bindWorld(node)), so the node's origin is the
            // bind world matrix's translation in the part's space)
            var pivotOf = new Dictionary<Transform, Vector3>();
            if (pose && pose.parts != null && pose.inverseBind != null)
                for (int i = 0; i < pose.parts.Length && i < pose.inverseBind.Length; i++)
                    if (pose.parts[i]) pivotOf[pose.parts[i]] = pose.inverseBind[i].inverse.GetColumn(3);
            a.pivots = new Vector3[a.packed.Length];
            for (int i = 0; i < a.packed.Length; i++) a.pivots[i] = pivotOf.TryGetValue(a.packed[i], out var pv) ? pv : Vector3.zero;
            var flags = new List<GameObject>();
            void F(Component c) { if (c && flags.Count < 64 && !flags.Contains(c.gameObject)) flags.Add(c.gameObject); }
            F(holder); foreach (var t in a.full) F(t);
            if (pose && pose.parts != null) foreach (var p in pose.parts) F(p);
            a.flags = flags.ToArray();
            var br = new List<Renderer>();
            foreach (var t in new[] { chassis, front, rear }) if (t) br.AddRange(t.GetComponentsInChildren<Renderer>(true));
            a.bikeRenderers = br.ToArray();
            var rr = new List<Renderer>();
            if (rider) rr.AddRange(rider.GetComponentsInChildren<Renderer>(true));
            if (proxy) rr.AddRange(proxy.GetComponentsInChildren<Renderer>(true));
            a.riderRenderers = rr.ToArray();
            // the vehicle's half width (the wheel camera's side offset): the parts' widest point beside the chassis
            float hw = 0.8f;
            if (anchor) foreach (var p in packed) if (p && p != anchor && !(rider && p.IsChildOf(rider.transform))) hw = Mathf.Max(hw, Mathf.Abs(anchor.InverseTransformPoint(p.position).x));
            a.halfWidth = Mathf.Min(hw, 4f);
            return a;
        }

        void CollectActors()
        {
            actors.Clear();
            var rt = Runtime;
            string localName = Reflex.Port.Content.ReflexProfile.HasPersona ? Reflex.Port.Content.ReflexProfile.Persona : rt.Roster?.Local?.name;   // the garage's NAME
            int localSlot = rt.Roster?.Local?.slot.slot ?? -1;
            if (rt.chassis)
                actors.Add(MakeRider(string.IsNullOrEmpty(localName) ? "YOU" : localName, localSlot, true, null, rt.chassis, rt.frontWheel, rt.rearWheel, rt.riderProxy, rt.bikePose, rt.rider));
            opponentCount = 0;
            if (rt.opponentRiders)
            {
                foreach (var (name, slot, bike, rider, _) in rt.opponentRiders.Riders)
                {
                    opponentCount++;
                    if (bike == null || !bike.chassis) continue;
                    var ch = bike.chassis.transform;
                    actors.Add(MakeRider(name, slot, false, ch.parent, ch, bike.frontWheel, bike.rearWheel, null, bike.pose, rider?.rider));
                }
            }
            if (rt.animatedObjects && rt.animatedObjects.entries != null)
            {
                var list = new List<Transform>();
                foreach (var e in rt.animatedObjects.entries) if (e.target) list.Add(e.target);
                if (list.Count > 0) { var props = DepthSorted(list); actors.Add(new Actor { name = "SCENERY", props = true, full = props, fullOrder = DepthOrder(props) }); }
            }
            int fs = 0, ns = 0;
            var layout = new ReplayActorLayout[actors.Count];
            for (int i = 0; i < actors.Count; i++)
            {
                var a = actors[i];
                a.layout = new ReplayActorLayout { fullStart = fs, fullCount = a.full.Length, nodeStart = ns, nodeCount = a.packed.Length };
                fs += a.full.Length; ns += a.packed.Length;
                layout[i] = a.layout;
            }
            PackWait();
            Buffer.Reset(layout, LengthSeconds, MemoryBudget);
            Particles.Reset(RecordEffects ? ParticleBudget : 0);
            TerrainReset(); SoundReset();
            lastSample = -1f; prevParticleTime = -1f;
            paths.Clear();
            keys.Clear();
            Debug.Log($"[Replay] recording {actors.Count} actors ({Buffer.FullTotal} full + {Buffer.NodeTotal} packed transforms), {Buffer.FrameBytes / 1024f:0.0} KB a frame at " +
                      $"{1f / Buffer.Interval:0} Hz: {Buffer.Seconds:0} s kept ({Buffer.FullBytes / (1024f * 1024f):0} MB), effects ring {Particles.Bytes / (1024f * 1024f):0} MB");
        }

        bool ActorsValid()
        {
            if (actors.Count == 0) return false;
            foreach (var a in actors)
            {
                foreach (var t in a.full) if (!t) return false;
                if (a.packed.Length > 0 && !a.packed[0]) return false;
            }
            return true;
        }

        void EnsureLayout()
        {
            var rt = Runtime;
            bool check = layoutDirty || Time.unscaledTime >= layoutCheckAt;
            if (!check) return;
            layoutCheckAt = Time.unscaledTime + 0.5f;
            bool changed = layoutDirty || !ActorsValid();
            if (!changed && rt.opponentRiders && rt.opponentRiders.Built != (actors.Count - 1 - (actors.Count > 0 && actors[actors.Count - 1].props ? 1 : 0))) changed = true;
            if (!changed && rt.opponentRiders && rt.opponentRiders.Count != opponentCount) changed = true;
            if (!changed) return;
            layoutDirty = false;
            CollectActors();
        }

        /// The moment on screen may be recorded: the race (countdown .. finish) or the free ride, not a scenario, the post-race pages or
        /// the original's own replay.
        bool Recordable(bool now = false)
        {
            var rt = Runtime;
            if (!rt || !rt.Running || !rt.chassis) return false;
            if (!now && (rt.paused || double.IsNaN(rt.PresentedGameFrame))) return false;   // (opening the viewer: the moment on screen, paused or not)
            if (rt.ScenarioKind != 0 || rt.OriginalReplayShowing || rt.OwnResultsPresentation || OriginalPostRace.Active) return false;
            if (rt.raceResults && rt.raceResults.Active) return false;
            var race = rt.Race;
            if (race.valid != 0 && race.state == 4) return false;   // Post: the race is over for the field (5 rides on: recorded)
            return true;
        }

        void LateUpdate() { var am = Reflex.Port.Platform.ReflexAllocMeter.Begin(); LateUpdateMetered(); Reflex.Port.Platform.ReflexAllocMeter.End(Reflex.Port.Platform.ReflexAllocMeter.Replay, am); }   // (the perf line's alloc share, w-gcspikes)
        void LateUpdateMetered()
        {
            if (!Runtime) return;
            if (Active) { ViewFrame(); return; }
            if (Runtime.WorldGeneration != generation) { generation = Runtime.WorldGeneration; SessionReset(); }
            if (!Recordable()) return;
            float gt = Runtime.PresentedGameTime;
            if (!float.IsNaN(lastGameTime))
            {
                float d = gt - lastGameTime;
                if (d < -0.5f) { SessionReset(); lastGameTime = gt; return; }   // the game's clock went back: a new run of the event
                if (d <= 0f) return;                                            // the same moment as last frame (held)
                clock += Mathf.Min(d, 0.25f);
            }
            lastGameTime = gt;
            EnsureLayout();
            if (actors.Count == 0) return;
            if (lastSample >= 0f && clock - lastSample < Buffer.Interval * 0.98f) return;
            Record();
        }

        void Record()
        {
            TerrainThaw();
            sw.Restart();
            var f = Buffer.Next();
            Fill(f);
            AttachDeltas(f, false);
            sw.Stop();
            recordMs += sw.Elapsed.TotalMilliseconds; records++;
            lastSample = f.time;
            if (logReplay && Time.unscaledTime - logAt > 10f)
            {
                logAt = Time.unscaledTime;
                Debug.Log($"[Replay] recorded {Buffer.Count} frames ({Buffer.Duration:0.0} s), {RecordMs:0.000} ms a frame, particles {f.particleCount} this frame");
            }
        }

        readonly System.Diagnostics.Stopwatch sw = new System.Diagnostics.Stopwatch();
        double recordMs, poseMs;
        int records, poses;
        float logAt;
        static readonly bool logReplay = Array.IndexOf(Environment.GetCommandLineArgs(), "-rxReplayLog") >= 0 || Environment.GetEnvironmentVariable("REFLEX_REPLAY_LOG") == "1";
        /// The average cost of a recorded frame / a replayed pose (ms, main thread).
        public double RecordMs => records > 0 ? recordMs / records : 0;
        public double PoseMs => poses > 0 ? poseMs / poses : 0;

        /// The live moment as the newest frame (opening the viewer): appended, or the newest frame refreshed if it is this very moment.
        void RecordNow()
        {
            if (!Recordable(true)) return;
            EnsureLayout();
            if (actors.Count == 0) return;
            var last = Buffer.Last;
            if (last != null && clock - last.time < 1e-4f) { Fill(last); AttachDeltas(last, true); return; }
            Record();
        }

        // the particles the runtime handed the renderer this frame (OriginalParticles.Applied), copied (the core reuses its array)
        ReflexParticle[] liveParticles = new ReflexParticle[0];
        int liveParticleCount;
        ReflexParticleInfo liveParticleInfo;
        bool liveParticlesNew;

        void OnEnable() { OriginalParticles.Applied += OnParticles; TerrainHooks(true); SoundHooks(true); }
        void OnDisable() { OriginalParticles.Applied -= OnParticles; TerrainHooks(false); SoundHooks(false); if (Active) Close(); }

        void OnParticles(OriginalParticles who, ReflexParticle[] p, int count, ReflexParticleInfo info)
        {
            if (Active || applyingParticles || !Runtime || who != Runtime.particles) return;
            if (liveParticles.Length < count) liveParticles = new ReflexParticle[Math.Max(count, 1024)];
            if (count > 0 && p != null) Array.Copy(p, liveParticles, Math.Min(count, p.Length));
            liveParticleCount = count; liveParticleInfo = info; liveParticlesNew = true;
        }

        /// The terrain deltas and sound commands that arrived since the last frame belong to this one (refresh: the newest frame again).
        void AttachDeltas(ReplayFrame f, bool refresh)
        {
            if (!refresh) { f.terrainStart = terrainPending; f.soundStart = soundPending; }
            f.terrainCount = (int)Math.Max(0, Terrain.Written - f.terrainStart); terrainPending = Terrain.Written;
            f.soundCount = (int)Math.Max(0, Sound.Written - f.soundStart); soundPending = Sound.Written;
        }

        void Fill(ReplayFrame f)
        {
            f.time = clock;
            MudFill(f);
            PackFill(f);
            for (int k = 0; k < actors.Count; k++)
            {
                var a = actors[k]; var l = a.layout;
                Vector3 anchor = Vector3.zero;
                for (int i = 0; i < a.full.Length; i++)
                {
                    a.full[i].GetPositionAndRotation(out Vector3 p, out Quaternion q);
                    if (i == 0) anchor = p;
                    ReflexReplayBuffer.PutFull(f.full, l.fullStart + i, p, q);
                }
                for (int i = 0; i < a.packed.Length; i++)
                {
                    a.packed[i].GetPositionAndRotation(out Vector3 p, out Quaternion q);
                    ReflexReplayBuffer.Pack(f.nodes, l.nodeStart + i, p, q, anchor);
                }
                ulong bits = 0;
                for (int i = 0; i < a.flags.Length; i++) if (a.flags[i] && a.flags[i].activeSelf) bits |= 1UL << i;
                f.flags[k] = bits;
                f.place[k] = 0; f.lap[k] = 0;
            }
            var rt = Runtime;
            var race = rt.Race;
            for (int d = 0; d < rt.DriverCount; d++)
            {
                var dr = rt.Drivers[d];
                for (int k = 0; k < actors.Count; k++)
                    if (!actors[k].props && actors[k].slot == dr.slot) { f.place[k] = (byte)Mathf.Clamp(dr.place, 0, 255); f.lap[k] = (byte)Mathf.Clamp(dr.laps_completed, 0, 255); }
            }
            if (race.valid != 0 && actors.Count > 0 && actors[0].local && f.place[0] == 0) { f.place[0] = (byte)Mathf.Clamp(race.place, 0, 255); f.lap[0] = (byte)Mathf.Clamp(race.laps_completed, 0, 255); }
            f.racers = race.valid != 0 ? race.riders : 0;
            f.lapsTotal = race.valid != 0 && !Reflex.Port.RxCore.RxChampWeekend.LapTotalUnknown(race.num_laps) ? race.num_laps : 0;   // (a timed championship stage's clock: no lap total yet)
            f.raceTime = race.valid != 0 ? race.race_time : 0f;
            f.speedMph = rt.TryGetBike(out _, out Vector3 vel) ? vel.magnitude * (3600f / 5280f) : 0f;
            var cam = rt.chaseCamera;
            if (cam)
            {
                f.camPos = cam.transform.position; f.camRot = cam.transform.rotation;
                f.camFov = cam.fieldOfView; f.camNear = cam.nearClipPlane;
            }
            // the particles: at most 30 sets a second (they are the big part of a frame)
            f.particleStart = -1; f.particleCount = 0;
            if (Particles.Capacity > 0 && liveParticleCount > 0 && liveParticlesNew && (prevParticleTime < 0f || f.time - prevParticleTime >= 1f / 30f - 1e-3f || f.time < prevParticleTime))
            {
                // the packing (a bounding pass and the halves, ~0.15 ms for 2400 particles) runs on a worker: here only a copy and the place
                int n = Math.Min(Particles.Clamp(liveParticleCount), liveParticles.Length);
                f.particleStart = Particles.Reserve(n);
                f.particleCount = f.particleStart >= 0 ? n : 0;
                if (n > 0) PackLater(f, f.particleStart, n);
                f.particleInfo = liveParticleInfo;
                prevParticleTime = f.time;
                liveParticlesNew = false;
                particleFrames++; particleTotal += f.particleCount;
            }
        }
        float prevParticleTime = -1f;

        // one worker chain packs the particle sets in order; viewing (and a reset) waits for it first
        System.Threading.Tasks.Task packChain = System.Threading.Tasks.Task.CompletedTask;
        void PackLater(ReplayFrame f, long at, int n)
        {
            var buf = System.Buffers.ArrayPool<ReflexParticle>.Shared.Rent(n);
            Array.Copy(liveParticles, buf, n);
            var ring = Particles;
            packChain = packChain.ContinueWith(_ =>
            {
                try { ring.PackInto(at, buf, n, out Vector3 o, out Vector3 st); f.particleOrigin = o; f.particleStep = st; }
                finally { System.Buffers.ArrayPool<ReflexParticle>.Shared.Return(buf); }
            }, System.Threading.Tasks.TaskScheduler.Default);
        }
        void PackWait() { try { packChain.Wait(); } catch (Exception e) { Debug.LogWarning("[Replay] particle packing: " + e.GetBaseException().Message); packChain = System.Threading.Tasks.Task.CompletedTask; } }
        long particleFrames, particleTotal;
        /// The particles recorded per kept set, on average (the effects ring's use).
        public float ParticlesPerSet => particleFrames > 0 ? particleTotal / (float)particleFrames : 0f;

        // =============================================================================================== viewing

        int openedFrame = -1;
        float lastApplied = float.NaN;
        float endHold;   // real seconds held on the last frame before the replay stops
        Action onClosed;
        bool pausedBefore, audioBefore, hudHiddenBefore;
        float timeScaleBefore = 1f;
        Snapshot live;

        /// Opens the viewer on this face (false if there is nothing to show / it cannot open now). onClosed runs when it closes.
        public bool Open(View face, Origin from, Action closed = null)
        {
            if (!CanOpen) return false;
            if (from != Origin.Results) RecordNow();
            if (Buffer.Count == 0 || actors.Count == 0) { Debug.Log("[Replay] nothing recorded yet"); return false; }
            var rt = Runtime;
            if (!rt.chaseCamera) return false;
            PruneKeys();
            PackWait();
            Active = true; ViewerOpen = true;
            OpenedFrom = from; onClosed = closed;
            openedFrame = Time.frameCount;
            live = Snapshot.Take(actors, rt.chaseCamera, rt.particles);
            pausedBefore = rt.paused; audioBefore = AudioListener.pause; hudHiddenBefore = ReflexSettings.HudHidden;
            timeScaleBefore = Time.timeScale;
            rt.GetComponent<OriginalOnboardAdjust>()?.End();
            rt.paused = true; rt.ClearMenuInput();
            rt.ViewerActive = true;
            AudioListener.pause = true;
            ReflexSettings.HudHidden = true;
            Reflex.Port.Platform.ReflexCursor.Want(this, true);
            if (rt.onboard) rt.onboard.Deactivate();
            riderShown = bikeShown = othersShown = true;
            cockpit = null;
            Target = 0;
            Speed = 1f;
            UiHidden = false;
            lastApplied = float.NaN; shownParticleFrame = -1; mudShown = -1; soundReplayed = 0;
            TerrainOpen();
            endHold = 0f;
            // REPLAY: the last 20 s (the whole race from the results); PHOTO MODE: the live moment
            float back = from == Origin.Results ? float.MaxValue : 20f;
            PlayTime = face == View.Photo ? Buffer.EndTime : Mathf.Max(Buffer.StartTime, Buffer.EndTime - back);
            if (!Ui)
            {
                Ui = new GameObject("Open Reflex replay").AddComponent<ReflexReplayUi>();
                Ui.transform.SetParent(transform, false);
                Ui.Build(this);
            }
            HideOtherUi(true);
            CameraOpen();
            SetFace(face);
            Ui.Show();
            Debug.Log($"[Replay] open {face} from {from}, {Buffer.Count} frames ({Buffer.Duration:0.0} s), {actors.Count} actors, at {PlayTime - Buffer.StartTime:0.00} s; recording {RecordMs:0.000} ms a frame");
            poseMs = 0; poses = 0; logAt = Time.unscaledTime;
            return true;
        }

        /// Back to where the viewer was opened from, with the live moment exactly as it was.
        public void Close()
        {
            if (!Active) return;
            var rt = Runtime;
            SoundStop();
            MudLive();
            TerrainClose();
            ShowModels(true, true, true);
            SetCockpit(null);
            PhotoClose();
            CinematicClear();
            live?.Restore(rt ? rt.particles : null, this);
            live = null;
            if (Ui) Ui.Hide();
            HideOtherUi(false);
            Active = false; ViewerOpen = false; ClosedFrame = Time.frameCount;
            Playing = false;
            Time.timeScale = timeScaleBefore;
            if (rt)
            {
                rt.ViewerActive = false;
                rt.paused = pausedBefore; rt.ClearMenuInput();
            }
            AudioListener.pause = audioBefore;
            ReflexSettings.HudHidden = hudHiddenBefore;
            Reflex.Port.Platform.ReflexCursor.Want(this, false);
            Debug.Log($"[Replay] closed (pose {PoseMs:0.000} ms a frame)");
            var done = onClosed; onClosed = null;
            try { done?.Invoke(); } catch (Exception e) { Debug.LogException(e); }
        }

        public void SetFace(View face)
        {
            Face = face;
            if (face == View.Photo) { Playing = false; PhotoOpen(); }
            else
            {
                Playing = !AtLive || OpenedFrom == Origin.Results;
                if (AtLive && Buffer.Duration > 1f && Playing) PlayTime = Mathf.Max(Buffer.StartTime, Buffer.EndTime - 20f);
            }
            CameraFaceChanged();
            if (Ui) Ui.FaceChanged();
        }

        public void TogglePlay()
        {
            if (Face == View.Photo) SetFace(View.Replay);
            if (!Playing && Speed > 0f && PlayTime >= Buffer.EndTime - 0.01f) Seek(Buffer.StartTime);   // at the end: from the start
            if (!Playing && Speed < 0f && PlayTime <= Buffer.StartTime + 0.01f) Seek(Buffer.EndTime);
            Playing = !Playing;
            endHold = 0f;
        }

        public void Pause() => Playing = false;

        public void Seek(float t)
        {
            float clamped = Mathf.Clamp(t, Buffer.StartTime, Buffer.EndTime);
            if (Mathf.Abs(clamped - PlayTime) > 0.25f) { CameraCut(); soundResync = true; }
            PlayTime = clamped;
            endHold = 0f;
            Reflex.Port.Rendering.ReflexCinematic.Cut();   // (the poses jumped: no motion blur from their vectors)
        }

        public void Step(int frames)
        {
            Playing = false;
            if (Buffer.Count == 0) return;
            int i = Mathf.Clamp(Buffer.IndexAt(PlayTime) + frames, 0, Buffer.Count - 1);
            PlayTime = Buffer[i].time;
            Reflex.Port.Rendering.ReflexCinematic.Cut();
        }

        public void ChangeSpeed(int d)
        {
            float a = Mathf.Abs(Speed);
            int i = 0;
            for (int k = 0; k < Speeds.Length; k++) if (Mathf.Abs(Speeds[k] - a) < Mathf.Abs(Speeds[i] - a)) i = k;
            i = Mathf.Clamp(i + d, 0, Speeds.Length - 1);
            Speed = Mathf.Sign(Speed == 0f ? 1f : Speed) * Speeds[i];
        }

        public void Reverse() { Speed = -Speed; if (!Playing) TogglePlay(); }

        public string SpeedLabel => (Speed < 0f ? "REV " : "") + (Mathf.Abs(Speed) < 1f ? Mathf.Abs(Speed).ToString("0.##") : Mathf.Abs(Speed).ToString("0")) + "X";

        // --------------------------------------------------------------------------------------- target rider

        /// The riders that can be followed (the scenery actor is not one).
        public int TargetCount { get { int n = 0; foreach (var a in actors) if (!a.props) n++; return Mathf.Max(1, n); } }

        public void CycleTarget(int d)
        {
            int n = TargetCount;
            Target = ((Target + d) % n + n) % n;
            CameraCut();
        }

        public void SetTarget(int bike)
        {
            Target = Mathf.Clamp(bike, 0, TargetCount - 1);
            CameraCut();
        }

        public string TargetName(int bike)
        {
            if (bike < 0 || bike >= actors.Count) return "";
            var a = actors[bike];
            string n = string.IsNullOrEmpty(a.name) ? (a.local ? "YOU" : "RIDER " + bike) : a.name.ToUpperInvariant();
            return a.local ? n + "  (YOU)" : n;
        }

        /// The followed rider's line for the card: place, lap, speed at the moment shown.
        public string TargetInfo()
        {
            if (Buffer.Count == 0 || Target >= actors.Count) return "";
            Buffer.Locate(PlayTime, out int a, out int b, out float w);
            var f = w < 0.5f ? Buffer[a] : Buffer[b];
            float mph;
            if (actors[Target].local) mph = Mathf.Lerp(Buffer[a].speedMph, Buffer[b].speedMph, w);
            else
            {
                TargetAt(PlayTime - 0.1f, Target, out Vector3 p0, out _);
                TargetAt(PlayTime + 0.1f, Target, out Vector3 p1, out _);
                float span = Mathf.Min(PlayTime + 0.1f, Buffer.EndTime) - Mathf.Max(PlayTime - 0.1f, Buffer.StartTime);
                mph = span > 1e-3f ? (p1 - p0).magnitude / span * (3600f / 5280f) : 0f;
            }
            int pos = f.place[Target];
            string s = "";
            if (pos > 0 && f.racers > 1) s += $"P{pos} OF {f.racers}  ·  ";
            if (f.lapsTotal > 0) s += $"LAP {Mathf.Clamp(f.lap[Target] + 1, 1, f.lapsTotal)} OF {f.lapsTotal}  ·  ";
            s += $"{ReflexSettings.Speed(mph):0} {ReflexSettings.SpeedUnit}";
            return s;
        }

        // ------------------------------------------------------------------------------------------- per frame

        void Update()
        {
            if (!Runtime) return;
            if (!Active) { LiveKeys(); ProbeTick(); return; }
            if (!Runtime.Running) { Close(); return; }
            float dt = Time.unscaledDeltaTime;
            if (Time.frameCount != openedFrame && !Capturing) HandleInput(dt);
            if (!Active) return;   // closed by the input
            float replayDt = 0f;
            if (Playing && Buffer.Count > 1)
            {
                float before = PlayTime;
                PlayTime = Mathf.Clamp(PlayTime + dt * Speed, Buffer.StartTime, Buffer.EndTime);
                replayDt = PlayTime - before;
                bool atEnd = Speed > 0f ? PlayTime >= Buffer.EndTime : PlayTime <= Buffer.StartTime;
                if (atEnd) { endHold += dt; if (endHold > 1.2f) Playing = false; }   // the end: a moment's hold, then the replay waits
            }
            frameReplayDt = replayDt + shuttleDt;
            shuttleDt = 0f;
            SoundTick(dt);
            Time.timeScale = Playing ? Mathf.Clamp(Mathf.Abs(Speed), 0f, 4f) : 0f;
            if (Ui) Ui.Frame();
            ProbeTick();
        }

        float frameReplayDt;

        /// The viewer's frame after every other LateUpdate: the actors posed at PlayTime, the particles, the camera.
        void ViewFrame()
        {
            sw.Restart();
            ShutterBegin();   // PHOTO MODE's motion blur: this frame's moment inside the shutter time (ReflexReplay.Cinematic.cs)
            ApplyPose(ViewTime, false);
            ApplyParticles(ViewTime);
            int shownFrame = Buffer.IndexAt(PlayTime);
            TerrainShow(shownFrame);
            MudShow(shownFrame);
            sw.Stop();
            poseMs += sw.Elapsed.TotalMilliseconds; poses++;
            if (logReplay && Time.unscaledTime - logAt > 3f)
            {
                logAt = Time.unscaledTime;
                Debug.Log($"[Replay] t {PlayTime - Buffer.StartTime:0.00} / {Buffer.Duration:0.00} s, {(Playing ? "playing" : "held")} x{Speed}, {CameraLabel}, target {Target}, pose {PoseMs:0.000} ms, frame {Time.unscaledDeltaTime * 1000f:0.0} ms");
            }
            UpdateCamera(Time.unscaledDeltaTime, frameReplayDt);
            frameReplayDt = 0f;
            ApplyVisibility();
            PhotoFrame();
            CinematicFrame();   // the lens and camera effects of this frame (ReflexReplay.Cinematic.cs)
        }

        /// Tests only: the old blend (each part's origin and rotation blended apart: the wheels left their axles between recorded frames).
        public static bool LegacyBlend;

        /// Poses every actor at t (blended between the two frames around it).
        void ApplyPose(float t, bool force)
        {
            if (Buffer.Count == 0) return;
            if (!force && t == lastApplied) return;
            if (!ActorsValid()) return;   // (a model was rebuilt under the viewer: the world changed; it closes next frame)
            lastApplied = t;
            Buffer.Locate(t, out int ia, out int ib, out float w);
            var fa = Buffer[ia]; var fb = Buffer[ib];
            var near = w < 0.5f ? fa : fb;
            for (int k = 0; k < actors.Count; k++)
            {
                var a = actors[k]; var l = a.layout;
                // the flags first (a hidden part stays hidden while posed)
                ulong bits = near.flags[k];
                for (int i = 0; i < a.flags.Length; i++)
                {
                    var go = a.flags[i];
                    bool on = (bits & (1UL << i)) != 0;
                    if (go && go.activeSelf != on) go.SetActive(on);
                }
                Vector3 anchorA = a.full.Length > 0 ? ReflexReplayBuffer.FullPos(fa.full, l.fullStart) : Vector3.zero;
                Vector3 anchorB = a.full.Length > 0 ? ReflexReplayBuffer.FullPos(fb.full, l.fullStart) : Vector3.zero;
                foreach (int i in a.fullOrder)
                {
                    int j = l.fullStart + i;
                    Vector3 pa = ReflexReplayBuffer.FullPos(fa.full, j), pb = ReflexReplayBuffer.FullPos(fb.full, j);
                    a.full[i].SetPositionAndRotation(Vector3.LerpUnclamped(pa, pb, w),
                        ReflexReplayBuffer.Nlerp(ReflexReplayBuffer.FullRot(fa.full, j), ReflexReplayBuffer.FullRot(fb.full, j), w));
                }
                for (int i = 0; i < a.packed.Length; i++)
                {
                    int j = l.nodeStart + i;
                    ReflexReplayBuffer.Unpack(fa.nodes, j, anchorA, out Vector3 pa, out Quaternion qa);
                    if (w <= 0f) { a.packed[i].SetPositionAndRotation(pa, Norm(qa)); continue; }
                    ReflexReplayBuffer.Unpack(fb.nodes, j, anchorB, out Vector3 pb, out Quaternion qb);
                    Vector3 pv = a.pivots[i];
                    if (pv == Vector3.zero || LegacyBlend) { a.packed[i].SetPositionAndRotation(Vector3.LerpUnclamped(pa, pb, w), ReflexReplayBuffer.Nlerp(qa, qb, w)); continue; }
                    // a bike part turns about its node (the wheel about its axle), not about its transform's origin (the model's): blending the
                    // origins and the rotations separately swings the axle through a chord of the turn (0.4 ft at 66 degrees a frame, measured up to
                    // 1.8 ft on a bike and 2.7 ft on a UTV at speed: the wheels came off the machine between two recorded frames). The node's origin travels in a straight line, the part turns about it.
                    qa = Norm(qa); qb = Norm(qb);
                    Quaternion q = Quaternion.SlerpUnclamped(qa, Quaternion.Dot(qa, qb) < 0f ? new Quaternion(-qb.x, -qb.y, -qb.z, -qb.w) : qb, w);
                    Vector3 node = Vector3.LerpUnclamped(pa + qa * pv, pb + qb * pv, w);
                    a.packed[i].SetPositionAndRotation(node - q * pv, q);
                }
            }
        }

        static Quaternion Norm(Quaternion q)
        {
            float n = Mathf.Sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            return n > 1e-6f ? new Quaternion(q.x / n, q.y / n, q.z / n, q.w / n) : Quaternion.identity;
        }

        // the particles of the moment shown: the nearest recorded set at or before it (none when not recorded / written over)
        ReflexParticle[] shownParticles;
        int shownParticleFrame = -1;
        bool applyingParticles;

        void ApplyParticles(float t)
        {
            var ps = Runtime.particles;
            if (!ps || Buffer.Count == 0) return;
            int i = Buffer.IndexAt(t);
            if (i == shownParticleFrame) return;
            shownParticleFrame = i;
            int n = 0; ReflexParticleInfo info = default; bool have = false;
            for (int k = i; k >= 0 && k > i - 4; k--)   // (particle sets are kept at 30 a second: the one before, if this frame has none)
            {
                var f = Buffer[k];
                if (f.particleStart < 0) continue;
                n = Particles.Read(f.particleStart, f.particleCount, f.particleOrigin, f.particleStep, ref shownParticles);
                info = f.particleInfo; have = true;
                break;
            }
            applyingParticles = true;
            try
            {
                if (have && n > 0) ps.Apply(shownParticles, n, info);
                else ps.Apply(shownParticles ?? (shownParticles = new ReflexParticle[OriginalParticles.MaxParticles]), 0, live != null ? live.particleInfo : info);
            }
            finally { applyingParticles = false; }
        }

        /// The followed rider at replay time t (from the frames: nothing is posed).
        public bool TargetAt(float t, int bike, out Vector3 p, out Quaternion q)
        {
            p = Vector3.zero; q = Quaternion.identity;
            if (Buffer.Count == 0 || bike < 0 || bike >= actors.Count || actors[bike].full.Length == 0) return false;
            Buffer.Locate(t, out int a, out int b, out float w);
            int j = actors[bike].layout.fullStart;
            p = Vector3.LerpUnclamped(ReflexReplayBuffer.FullPos(Buffer[a].full, j), ReflexReplayBuffer.FullPos(Buffer[b].full, j), w);
            q = ReflexReplayBuffer.Nlerp(ReflexReplayBuffer.FullRot(Buffer[a].full, j), ReflexReplayBuffer.FullRot(Buffer[b].full, j), w);
            return true;
        }

        /// The followed rider's eyes as posed now (the onboard camera), false when it has no rider skeleton.
        bool TargetEyes(int bike, out Vector3 eyes)
        {
            eyes = default;
            if (bike < 0 || bike >= actors.Count) return false;
            var r = actors[bike].rider;
            if (!r || r.bones == null || r.bones.Length <= 33 || !r.bones[32] || !r.bones[33]) return false;
            if (!r.gameObject.activeInHierarchy) return false;
            eyes = (r.bones[32].position + r.bones[33].position) * 0.5f;
            return true;
        }

        // ---------------------------------------------------------------------------------------- visibility

        bool riderShown = true, bikeShown = true, othersShown = true;

        /// PHOTO MODE's YOUR RIDER / YOUR MACHINE / OTHER RIDERS.
        void ApplyVisibility()
        {
            bool photo = Face == View.Photo;
            ShowModels(!photo || Photo.showRider, !photo || Photo.showBike, !photo || Photo.showOthers);
        }

        void ShowModels(bool rider, bool bike, bool others)
        {
            if (rider == riderShown && bike == bikeShown && others == othersShown) return;
            riderShown = rider; bikeShown = bike; othersShown = others;
            foreach (var a in actors)
            {
                if (a.props) continue;
                bool r = a.local ? rider : others, b = a.local ? bike : others;
                foreach (var x in a.riderRenderers) if (x) x.forceRenderingOff = !r;
                foreach (var x in a.bikeRenderers) if (x) x.forceRenderingOff = !b;
            }
        }

        // the onboard view of the local rider: the cockpit rider (no head and torso) while the camera is in his head
        OriginalRider cockpit;
        void SetCockpit(OriginalRider r)
        {
            if (r == cockpit) return;
            if (cockpit) OriginalOnboardCamera.SetCockpitView(cockpit, false);
            cockpit = r;
            if (r) OriginalOnboardCamera.SetCockpitView(r, true);
        }

        // ------------------------------------------------------------------------------- the other UI on screen

        readonly List<(UnityEngine.UIElements.UIDocument doc, UnityEngine.UIElements.StyleEnum<UnityEngine.UIElements.Visibility> was)> hiddenDocs =
            new List<(UnityEngine.UIElements.UIDocument, UnityEngine.UIElements.StyleEnum<UnityEngine.UIElements.Visibility>)>();
        readonly List<(CanvasGroup group, float alpha, bool added)> hiddenCanvases = new List<(CanvasGroup, float, bool)>();
        float hideScanAt;

        /// Every other menu / HUD document and canvas out of the way while the viewer shows (they keep their state: restored at Close).
        void HideOtherUi(bool hide)
        {
            if (!hide)
            {
                foreach (var (doc, was) in hiddenDocs) if (doc && doc.rootVisualElement != null) doc.rootVisualElement.style.visibility = was;
                foreach (var (group, alpha, added) in hiddenCanvases) { if (!group) continue; if (added) Destroy(group); else group.alpha = alpha; }
                hiddenDocs.Clear(); hiddenCanvases.Clear();
                return;
            }
            var mine = Ui ? Ui.Document : null;
            foreach (var doc in FindObjectsByType<UnityEngine.UIElements.UIDocument>(FindObjectsInactive.Exclude))
            {
                if (!doc || doc == mine || !doc.isActiveAndEnabled || doc.rootVisualElement == null) continue;
                if (doc.panelSettings && doc.panelSettings.targetTexture) continue;   // (a panel drawn into the world: a status board, a screen)
                bool known = false; foreach (var h in hiddenDocs) if (h.doc == doc) { known = true; break; }
                if (known) continue;
                hiddenDocs.Add((doc, doc.rootVisualElement.style.visibility));
                doc.rootVisualElement.style.visibility = UnityEngine.UIElements.Visibility.Hidden;
            }
            foreach (var c in FindObjectsByType<Canvas>(FindObjectsInactive.Exclude))
            {
                if (!c || !c.isRootCanvas || !c.isActiveAndEnabled || c.renderMode == RenderMode.WorldSpace || c.targetDisplay != 0) continue;   // (world-space canvases are scenery)
                var g = c.GetComponent<CanvasGroup>();
                bool known = false; foreach (var h in hiddenCanvases) if (h.group && h.group == g) { known = true; break; }
                if (known) continue;
                bool added = !g;
                if (added) g = c.gameObject.AddComponent<CanvasGroup>();
                hiddenCanvases.Add((g, added ? 1f : g.alpha, added));
                g.alpha = 0f;
            }
            hideScanAt = Time.unscaledTime + 0.5f;
        }

        // ------------------------------------------------------------------------------------------------ input

        float shuttleDt, scrubHeld;
        bool photoFromReplay;

        /// R / P while riding (the keys the game's own key map leaves free; the pad: the pause page).
        void LiveKeys()
        {
            var kb = Keyboard.current;
            if (kb == null) return;
            var rt = Runtime;
            if (rt.paused || rt.OwnMenuActive || rt.pauseMenu && rt.pauseMenu.IsOpen || ReflexOptions.IsOpen || OriginalOnboardAdjust.KeyboardCaptured) return;
            if (kb.rKey.wasPressedThisFrame && !OriginalInput.GameBinds(Key.R) && !OriginalInput.IsCameraKey(Key.R)) Open(View.Replay, Origin.Live);
            else if (kb.pKey.wasPressedThisFrame && !OriginalInput.GameBinds(Key.P) && !OriginalInput.IsCameraKey(Key.P)) { if (Open(View.Photo, Origin.Live)) photoFromReplay = false; }
        }

        void HandleInput(float dt)
        {
            var kb = Keyboard.current;
            var pad = Gamepad.current;
            bool Pressed(Key k) => kb != null && kb[k].wasPressedThisFrame;
            bool Btn(Func<Gamepad, UnityEngine.InputSystem.Controls.ButtonControl> b) => pad != null && b(pad).wasPressedThisFrame;
            bool shift = kb != null && (kb.leftShiftKey.isPressed || kb.rightShiftKey.isPressed);
            if (Time.unscaledTime >= hideScanAt) HideOtherUi(true);   // (a page that appeared under the viewer)

            // both faces
            if (Pressed(Key.Escape) || Btn(p => p.buttonEast) || Btn(p => p.startButton))
            {
                Reflex.Port.OpenFrontEnd.OuSound.Play("Back");
                if (Face == View.Photo && photoFromReplay) SetFace(View.Replay);
                else Close();
                return;
            }
            if (Pressed(Key.H) || Btn(p => p.buttonWest)) { UiHidden = !UiHidden; if (Ui) Ui.FaceChanged(); }
            if (Pressed(Key.P) || Pressed(Key.Tab) || Btn(p => p.selectButton))
            {
                Reflex.Port.OpenFrontEnd.OuSound.Play("Accept");
                if (Face == View.Photo) SetFace(View.Replay);
                else { photoFromReplay = true; SetFace(View.Photo); }
                return;
            }
            if (Pressed(Key.Comma)) Step(-1);
            if (Pressed(Key.Period)) Step(1);
            if (Pressed(Key.Home)) Seek(Buffer.StartTime);
            if (Pressed(Key.End)) Seek(Buffer.EndTime);

            if (Face == View.Replay)
            {
                if (Pressed(Key.Space) || Btn(p => p.buttonSouth)) TogglePlay();
                if (Pressed(Key.C) || Btn(p => p.buttonNorth)) CycleCamera(shift ? -1 : 1);
                if (Pressed(Key.RightBracket) || Pressed(Key.PageDown) || Btn(p => p.rightShoulder)) CycleTarget(1);
                if (Pressed(Key.LeftBracket) || Pressed(Key.PageUp) || Btn(p => p.leftShoulder)) CycleTarget(-1);
                if (Pressed(Key.UpArrow) || Btn(p => p.dpad.up)) ChangeSpeed(1);
                if (Pressed(Key.DownArrow) || Btn(p => p.dpad.down)) ChangeSpeed(-1);
                if (Pressed(Key.Backspace)) Reverse();
                // camera keys (ReflexReplay.Keyframes.cs): K adds one from the view now, Shift+K / Delete removes the one here; R3 does either
                if (Pressed(Key.K)) { if (shift) DeleteKey(); else AddKey(); }
                if (Pressed(Key.Delete)) DeleteKey();
                if (Btn(p => p.rightStickButton)) ToggleKey();
                // left / right: 1 s back / forward (shift: 5 s); while held, they scrub
                int dir = 0;
                if (kb != null) dir += (kb.rightArrowKey.isPressed ? 1 : 0) - (kb.leftArrowKey.isPressed ? 1 : 0);
                if (pad != null) dir += (pad.dpad.right.isPressed ? 1 : 0) - (pad.dpad.left.isPressed ? 1 : 0);
                bool tap = Pressed(Key.RightArrow) || Pressed(Key.LeftArrow) || Btn(p => p.dpad.right) || Btn(p => p.dpad.left);
                if (tap) { scrubHeld = 0f; if (dir != 0) Seek(PlayTime + Mathf.Sign(dir) * (shift ? 5f : 1f)); }
                else if (dir != 0) { scrubHeld += dt; if (scrubHeld > 0.35f) Shuttle(dir * (shift ? 8f : 3f) * dt); }
                else scrubHeld = 0f;
                // the triggers: a shuttle (rewind / fast forward, by how far they are pulled)
                if (pad != null)
                {
                    float sh = pad.rightTrigger.ReadValue() - pad.leftTrigger.ReadValue();
                    if (Mathf.Abs(sh) > 0.08f) Shuttle(Mathf.Sign(sh) * sh * sh * 6f * dt);
                }
            }
            else
            {
                if (Pressed(Key.Space) || Btn(p => p.buttonNorth) || Pressed(Key.F12)) TakePhoto();
            }
        }

        void Shuttle(float d)
        {
            if (Buffer.Count == 0) return;
            Playing = false;
            float before = PlayTime;
            PlayTime = Mathf.Clamp(PlayTime + d, Buffer.StartTime, Buffer.EndTime);
            shuttleDt += PlayTime - before;
        }

        /// Opens PHOTO MODE from the replay (the UI's button).
        public void PhotoFromReplay() { photoFromReplay = true; SetFace(View.Photo); }

        /// The photo face's BACK: to the replay it came from, else out of the viewer.
        public void PhotoBack()
        {
            if (photoFromReplay) SetFace(View.Replay);
            else Close();
        }

        /// PHOTO MODE opened directly (the pause page's PHOTO MODE, P): BACK leaves the viewer.
        public void OpenedPhotoDirect() => photoFromReplay = false;

        void OnDestroy()
        {
            PackWait();
            if (Active) Close();
            if (ViewerOpen && !Active) ViewerOpen = false;
            PhotoDestroy();
            var rings = storage; storage = null; Storage.Give(rings);   // (after PackWait: no packing job writes into its frames any more)
        }

        // ------------------------------------------------------------------------------- the live moment, exactly

        /// Every transform's local pose and flag of the live moment, the camera and the particles shown (put back at Close).
        sealed class Snapshot
        {
            readonly List<(Transform t, Vector3 p, Quaternion q)> poses = new List<(Transform, Vector3, Quaternion)>();
            readonly List<(GameObject g, bool on)> flags = new List<(GameObject, bool)>();
            Camera cam; Vector3 camPos; Quaternion camRot; float fov, near, far; Rect rect;
            ReflexParticle[] particles; int particleCount; public ReflexParticleInfo particleInfo; bool haveParticles;

            public static Snapshot Take(List<Actor> actors, Camera cam, OriginalParticles ps)
            {
                var s = new Snapshot();
                foreach (var a in actors)
                {
                    foreach (var t in a.full) if (t) s.poses.Add((t, t.localPosition, t.localRotation));
                    foreach (var t in a.packed) if (t) s.poses.Add((t, t.localPosition, t.localRotation));
                    foreach (var g in a.flags) if (g) s.flags.Add((g, g.activeSelf));
                }
                if (cam) { s.cam = cam; s.camPos = cam.transform.position; s.camRot = cam.transform.rotation; s.fov = cam.fieldOfView; s.near = cam.nearClipPlane; s.far = cam.farClipPlane; s.rect = cam.rect; }
                if (ps && ps.LastParticles != null)
                {
                    s.particleCount = Math.Min(ps.LastCount, ps.LastParticles.Length);
                    s.particles = new ReflexParticle[Math.Max(1, s.particleCount)];
                    Array.Copy(ps.LastParticles, s.particles, s.particleCount);
                    s.haveParticles = true;
                }
                if (ps) s.particleInfo = ps.LastInfo;
                return s;
            }

            public void Restore(OriginalParticles ps, ReflexReplay owner)
            {
                foreach (var (g, on) in flags) if (g && g.activeSelf != on) g.SetActive(on);
                foreach (var (t, p, q) in poses) if (t) t.SetLocalPositionAndRotation(p, q);
                if (cam) { cam.transform.SetPositionAndRotation(camPos, camRot); cam.fieldOfView = fov; cam.nearClipPlane = near; cam.farClipPlane = far; cam.rect = rect; }
                if (ps && haveParticles)
                {
                    owner.applyingParticles = true;
                    try { ps.Apply(particles, particleCount, particleInfo); } finally { owner.applyingParticles = false; }
                }
            }
        }
    }
}
