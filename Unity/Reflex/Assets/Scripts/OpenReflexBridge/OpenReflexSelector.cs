using System;
using System.Collections.Generic;
using Reflex.Port.RxCore;
using Reflex.Port.OriginalWorld;

// Which simulation drives the Unity world: the old lifted / emulator plugin (libreflexworld, OriginalSimulation) or the OPEN
// REFLEX core (librxcore, OpenReflexSimulation).
//
// Ordinary editor/legacy builds default to the old path. The dedicated open player carries a packaged backend marker.
// REFLEX_SIM explicitly overrides either default:
//
//   REFLEX_SIM=legacy              OriginalSimulation, libreflexworld
//   REFLEX_SIM=open                OpenReflexSimulation (librxcore), results adapted to the old Reflex* types
//
// `OpenReflexSelector.Mode` reads it (also "1"/"true"/"on" and "rx"/"core" for open, anything else is legacy; unknown text
// logs once and stays on legacy). A caller that wants an explicit choice uses Force(ReflexSimMode.Open) before creating the
// driver (an options toggle, a test); Force(null) returns to the environment.
//
// Both drivers implement IReflexSimDriver, whose Collect() returns the SAME OriginalSimulation.Results the renderers already
// read (the open path fills it through OpenReflexResultsAdapter). To use the core, a consumer constructs its simulation through
// this selector instead of `new OriginalSimulation(world, terrain != null)`:
//
//   var sim = OpenReflexSelector.Create(installRoot, eventSpec, world, terrain != null);   // legacyWorld is unused for open
//
// Core-only startup additionally requests managed terrain/appearance snapshots before constructing the presentation.
// The renderers keep consuming OriginalSimulation.Results. See core/notes/unity-api-surface.md.
namespace Reflex.Port.RxCore.Bridge
{
    public enum ReflexSimMode { Legacy = 0, Open = 1 }

    /// The simulation surface the project's renderers already use (OriginalSimulation's public contract), implemented by the
    /// old plugin path and by the open core path.
    public interface IReflexSimDriver : IDisposable
    {
        ReflexSimMode Mode { get; }

        // ---- frames (the old signatures: ReflexPad, byte[256] DirectInput keys, pad mask)
        bool Submit(float dt, byte[] keys, ReflexPad[] pads, uint mask);
        void SubmitFrames(int frames, float step, byte[] keys, ReflexPad[] pads, uint mask);
        double Requested { get; }
        OriginalSimulation.Results Collect();
        void KeepResults();
        void Recycle(OriginalSimulation.Results results);

        bool Idle { get; }
        bool WaitIdle(float timeoutMs);
        bool RealClock { get; set; }
        double LastFrameStart { get; }
        void ResetClock();

        // ---- riders / slots
        void TrackSlots(int[] slots);
        void SetRiderDetail(bool[] near);
        int farRiderInterval { get; set; }

        // ---- old-plugin-only extras (the open path has no export yet; each returns false / does nothing there)
        void TrackObjects(OriginalAnimatedObjects.Entry[] entries);
        bool AttachAudio(System.Collections.Concurrent.ConcurrentQueue<(uint wrapper, uint serial)> finished);
        bool AttachParticles();
        void AttachAmbient();
        void BetweenFrames(Action<IntPtr> call);

        // ---- view selection (the game's own Camera setter: rx_session_set_camera / OpenReflexSimulation.SetCamera)
        /// 0 = the default third person (GamePlay), 2 = first person; the open path queues it before the next frame, the legacy
        /// path ignores it (the old plugin's camera choice is its own). The selected view is visible in Camera / Results.camera.
        void SetCamera(int mode);

        // ---- diagnostics
        /// The session's state: the core's (Loading..Finished/Error) for Open; Racing once the legacy world exists.
        RxSessionState SessionState { get; }
        /// The open worker's phase (legacy: Ready after construction, or Failed).
        OpenReflexSimulation.Phase Phase { get; }
        /// Why the session failed (null while fine).
        string Failure { get; }
        /// Waits for create + event load (legacy is synchronous: true at once).
        bool WaitReady(float timeoutMs);
        /// The open session (null for legacy), for the exports the driver does not wrap.
        OpenReflexSimulation Open { get; }
        /// The legacy simulation (null for open), for the old plugin's own extras.
        OriginalSimulation Legacy { get; }
    }

    public static class OpenReflexSelector
    {
        public const string EnvironmentVariable = "REFLEX_SIM";
        static ReflexSimMode? forced;

        /// Messages that are not results (the worker, and an unknown REFLEX_SIM value).
        public static Action<string> Log = m => System.Diagnostics.Debug.WriteLine("[OpenReflex] " + m);

        /// Explicit choice (an options toggle / a test); null reads REFLEX_SIM again.
        public static void Force(ReflexSimMode? mode) { forced = mode; fromEnvironment = null; }

        // REFLEX_SIM read once (Mode is asked every frame by the perf log and the world: the read and ToLowerInvariant were garbage each
        // time, w-gcspikes); Force(null) reads it again
        static ReflexSimMode? fromEnvironment;
        public static ReflexSimMode Mode { get { if (forced.HasValue) return forced.Value; return fromEnvironment ??= FromEnvironment(); } }

        /// True for an explicit open choice or the dedicated packaged open player.
        public static bool UseOpenReflex => Mode == ReflexSimMode.Open;
        public static bool UseLegacy => !UseOpenReflex;

        // The opt-in open player carries this marker; ordinary editor/legacy builds retain their previous default.
#if UNITY_5_3_OR_NEWER
        static readonly bool PackagedOpen = !UnityEngine.Application.isEditor && System.IO.File.Exists(
            System.IO.Path.Combine(UnityEngine.Application.dataPath, "Resources", "reflex-open-core.txt"));
#else
        const bool PackagedOpen = false;
#endif
        static bool warned;
        static ReflexSimMode FromEnvironment()
        {
            string v = Environment.GetEnvironmentVariable(EnvironmentVariable);
            if (string.IsNullOrEmpty(v)) return PackagedOpen ? ReflexSimMode.Open : ReflexSimMode.Legacy;
            switch (v.Trim().ToLowerInvariant())
            {
                case "open": case "rx": case "core": case "rxcore": case "1": case "true": case "on": case "yes":
                    return ReflexSimMode.Open;
                case "legacy": case "old": case "plugin": case "0": case "false": case "off": case "no":
                    return ReflexSimMode.Legacy;
            }
            if (!warned) { warned = true; Log($"REFLEX_SIM='{v}' is not 'open' or 'legacy': staying on the legacy plugin."); }
            return ReflexSimMode.Legacy;
        }

        /// "open (REFLEX_SIM=open)" / "legacy (REFLEX_SIM=...)" for a log line.
        public static string Describe
        {
            get
            {
                string v = Environment.GetEnvironmentVariable(EnvironmentVariable) ?? "";
                return (Mode == ReflexSimMode.Open ? "open" : "legacy") + (forced.HasValue ? " (forced)" : " (" + EnvironmentVariable + "='" + v + "')");
            }
        }

        public static IReflexSimDriver CreateLegacy(IntPtr world, bool trackTerrain) => new LegacyReflexDriver(world, trackTerrain);

        /// RxNativeLoader.Load must have run (OpenReflexSimulation checks the ABI on its worker).
        public static IReflexSimDriver CreateOpen(string installRoot, RxEventSpec spec, bool trackTerrain, bool prepareStartup = false, RxProfileCell[] profile = null, IPlayerSessionPersistence persistence = null, CareerLaunch career = null, bool replayPresentation = false, bool inCareerMode = false, bool testDrive = false, bool tutorial = false, bool arenaImage = false) => new OpenReflexDriver(installRoot, spec, trackTerrain, prepareStartup, profile, persistence, career, replayPresentation, inCareerMode, testDrive, tutorial, arenaImage);

        /// The selected path. `legacyWorld` is the libreflexworld world (ReflexWorldNative.reflex_world_create), used only by
        /// the legacy path; `installRoot` / `spec` only by the open one.
        public static IReflexSimDriver Create(string installRoot, RxEventSpec spec, IntPtr legacyWorld, bool trackTerrain)
        {
            return UseOpenReflex ? CreateOpen(installRoot, spec, trackTerrain) : CreateLegacy(legacyWorld, trackTerrain);
        }
    }

    /// The old lifted/emulator plugin, unchanged: every call is forwarded to OriginalSimulation.
    sealed class LegacyReflexDriver : IReflexSimDriver
    {
        readonly OriginalSimulation sim;
        public LegacyReflexDriver(IntPtr world, bool trackTerrain) { sim = new OriginalSimulation(world, trackTerrain); }
        public ReflexSimMode Mode => ReflexSimMode.Legacy;

        public bool Submit(float dt, byte[] keys, ReflexPad[] pads, uint mask) => sim.Submit(dt, keys, pads, mask);
        public void SubmitFrames(int frames, float step, byte[] keys, ReflexPad[] pads, uint mask) => sim.SubmitFrames(frames, step, keys, pads, mask);
        public double Requested => sim.Requested;
        public OriginalSimulation.Results Collect() => sim.Collect();
        public void KeepResults() => sim.KeepResults();
        public void Recycle(OriginalSimulation.Results results) => sim.Recycle(results);
        public bool Idle => sim.Idle;
        public bool WaitIdle(float timeoutMs) => sim.WaitIdle(timeoutMs);
        public bool RealClock { get => sim.RealClock; set => sim.RealClock = value; }
        public double LastFrameStart => sim.LastFrameStart;
        public void ResetClock() => sim.ResetClock();
        public void TrackSlots(int[] slots) => sim.TrackSlots(slots);
        public void SetRiderDetail(bool[] near) => sim.SetRiderDetail(near);
        public int farRiderInterval { get => sim.farRiderInterval; set => sim.farRiderInterval = value; }
        public void TrackObjects(OriginalAnimatedObjects.Entry[] entries) => sim.TrackObjects(entries);
        public bool AttachAudio(System.Collections.Concurrent.ConcurrentQueue<(uint, uint)> finished) => TrackAudio(sim, finished);
        public bool AttachParticles() => sim.AttachParticles();
        public void AttachAmbient() => sim.AttachAmbient();
        public void BetweenFrames(Action<IntPtr> call) => sim.BetweenFrames(call);
        // The old plugin has no view selector in its result surface: the game's own Camera key still drives its cameras.
        public void SetCamera(int mode) { }
        public RxSessionState SessionState => RxSessionState.Racing;   // the old plugin exposes no activity state
        public OpenReflexSimulation.Phase Phase => OpenReflexSimulation.Phase.Ready;
        public string Failure => null;
        public bool WaitReady(float timeoutMs) => true;
        public OpenReflexSimulation Open => null;
        public OriginalSimulation Legacy => sim;
        public void Dispose() => sim.Dispose();

        // OriginalSimulation.AttachAudio returns void; keep the interface's true = attached.
        static bool TrackAudio(OriginalSimulation s, System.Collections.Concurrent.ConcurrentQueue<(uint wrapper, uint serial)> finished)
        { s.AttachAudio(finished); return true; }
    }

    /// The OPEN REFLEX core: OpenReflexSimulation's results adapted to the old Reflex* types. Everything is copied on the main
    /// thread inside Collect (the worker keeps its own buffers), so keeping several results for interpolation works exactly as
    /// with the old path (call KeepResults first, Recycle when done).
    sealed class OpenReflexDriver : IReflexSimDriver
    {
        readonly OpenReflexSimulation sim;
        readonly RxPad[] pads = new RxPad[4];
        readonly Stack<OriginalSimulation.Results> pool = new Stack<OriginalSimulation.Results>();
        OriginalSimulation.Results a, b;
        bool owned, flip;

        public OpenReflexDriver(string installRoot, RxEventSpec spec, bool trackTerrain, bool prepareStartup = false, RxProfileCell[] profile = null, IPlayerSessionPersistence persistence = null, CareerLaunch career = null, bool replayPresentation = false, bool inCareerMode = false, bool testDrive = false, bool tutorial = false, bool arenaImage = false)
        {
            a = OpenReflexResultsAdapter.Create();
            b = OpenReflexResultsAdapter.Create();
            Reflex.Port.Options.ReflexClientLive.SyncGameSettings();   // the game's own UI settings (mouse sensitivity, vibration, difficulty, ...) go into the new session
            sim = new OpenReflexSimulation(installRoot, spec, trackTerrain, prepareStartup, profile, Reflex.Port.FrontEnd.CoreRemapping.StartupValues(), persistence, career, replayPresentation, inCareerMode, testDrive, tutorial, arenaImage);
        }

        public ReflexSimMode Mode => ReflexSimMode.Open;

        public bool Submit(float dt, byte[] keys, ReflexPad[] padArray, uint mask) => sim.Submit(dt, keys, ToRx(padArray), mask);
        public void SubmitFrames(int frames, float step, byte[] keys, ReflexPad[] padArray, uint mask) => sim.SubmitFrames(frames, step, keys, ToRx(padArray), mask);

        RxPad[] ToRx(ReflexPad[] padArray)
        {
            if (padArray == null) return null;
            for (int i = 0; i < pads.Length; i++)
                pads[i] = i < padArray.Length
                    ? new RxPad { buttons = padArray[i].buttons, left_trigger = padArray[i].leftTrigger, right_trigger = padArray[i].rightTrigger, lx = padArray[i].lx, ly = padArray[i].ly, rx = padArray[i].rx, ry = padArray[i].ry }
                    : default;
            return pads;
        }

        public double Requested => sim.Requested;

        public OriginalSimulation.Results Collect()
        {
            var src = sim.Collect();
            if (src == null) return null;
            var dst = owned ? (pool.Count > 0 ? pool.Pop() : OpenReflexResultsAdapter.Create()) : Next();
            OpenReflexResultsAdapter.Apply(dst, src);
            return dst;
        }

        OriginalSimulation.Results Next()
        {
            flip = !flip;
            return flip ? a : b;
        }

        public void KeepResults() => owned = true;

        public void Recycle(OriginalSimulation.Results results)
        {
            if (results == null) return;
            if (!owned) return;
            OpenReflexResultsAdapter.Reset(results);
            if (pool.Count < 8) pool.Push(results);
        }

        public bool Idle => sim.Idle;
        public bool WaitIdle(float timeoutMs) => sim.WaitIdle(timeoutMs);
        public bool RealClock { get => sim.RealClock; set => sim.RealClock = value; }
        public double LastFrameStart => sim.LastFrameStart;
        public void ResetClock() => sim.ResetClock();
        public void TrackSlots(int[] slots) => sim.TrackSlots(slots);
        public void SetRiderDetail(bool[] near) => sim.SetRiderDetail(near);
        public int farRiderInterval { get => sim.farRiderInterval; set => sim.farRiderInterval = value; }

        // Animated scenery objects (the start gate, ...): the core finds them through its scene entity list by class vtable and
        // entity id and returns the matrix at the entry's offset (no game address crosses the API).
        public void TrackObjects(OriginalAnimatedObjects.Entry[] entries)
        {
            int n = entries?.Length ?? 0;
            var vt = new uint[n]; var id = new int[n]; var off = new uint[n];
            for (int i = 0; i < n; i++) { vt[i] = entries[i].vtable; id[i] = unchecked((int)entries[i].id); off[i] = entries[i].matrixOffset; }
            sim.TrackObjects(vt, id, off);
        }
        public bool AttachAudio(System.Collections.Concurrent.ConcurrentQueue<(uint wrapper, uint serial)> finished) { WarnOnce(ref audioWarned, "audio commands are not exported by the core yet"); return false; }
        public bool AttachParticles() => true;   // no enable needed: the managers always run in the core
        public void AttachAmbient() => sim.AttachAmbient();
        public void BetweenFrames(Action<IntPtr> call) => sim.BetweenFrames(call);
        public void SetCamera(int mode) => sim.SetCamera(mode);

        bool audioWarned;
        static void WarnOnce(ref bool flag, string message)
        {
            if (flag) return;
            flag = true;
            OpenReflexSelector.Log(message);
        }

        public RxSessionState SessionState => sim.State;
        public OpenReflexSimulation.Phase Phase => sim.CurrentPhase;
        public string Failure => sim.Failure;
        public bool WaitReady(float timeoutMs) => sim.WaitReady(timeoutMs);
        public OpenReflexSimulation Open => sim;
        public OriginalSimulation Legacy => null;
        public void Dispose() => sim.Dispose();
    }
}
