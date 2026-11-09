// The event's core session started in the front end (w-loadarena), for the shell's event load in the test-ride arena (OpenShell.LoadArena.cs):
// the same session the race scene's OriginalWorldRuntime would start (OriginalWorldRuntime.OpenEventSpec, the same persistence, replay
// presenter and challenges mode) is created at START EVENT while the front end's 3D menu world (the compound: the garage's TEST RIDE arena)
// stays on screen, and its event load runs there: the worker creates the session and loads the event, then the event's load states run as
// frames (pumped here exactly as OriginalWorldRuntime.PollOpenStartup does) until the startup snapshot is captured. PRESS START then loads
// the race scene, whose runtime takes this session over (Take: the spec and the settings must be the ones it would have made) instead of
// starting its own.
//   threading  nothing changes: the session belongs to its OpenReflexSimulation worker from create to destroy; the main thread only
//              submits frames and collects results (as the runtime does), and the object moves from the front end to the race scene on
//              the main thread. One session at a time (the core's one arena): the front end runs no other session meanwhile, and a
//              session closing before still holds the arena until it is done (OpenReflexSimulation's ownership wait).
//   memory     the race scene's Unity world is not loaded before START (the front-end scene goes with the scene change as before), so the
//              front end's compound and the event's core session are the only worlds up while loading: no two Unity worlds at once.
//   lifetime   a session nobody takes (the race scene never asked: another scene, an exception) is disposed 60 s after the launch; quitting
//              disposes it too.
using System;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OpenEventPrestart : MonoBehaviour
    {
        static OpenEventPrestart s_current;
        IReflexSimDriver sim;
        string key, eventKey;
        float startedAt, launchedAt = -1f, readyAt = -1f, framesAt = -1f;
        readonly byte[] keys = new byte[256];
        readonly ReflexPad[] pads = new ReflexPad[4];
        string failure;

        /// The pre-started session of the event being loaded (null: none).
        public static OpenEventPrestart Current => s_current;
        public string EventKey => eventKey;
        /// The session could not start or load (the reason; null: fine so far).
        public string Failure => failure ?? (sim?.Phase == OpenReflexSimulation.Phase.Failed ? sim.Failure ?? "the session failed" : null);
        /// The event is loaded and its load states are over: the world exists (the startup snapshot is waiting for the race scene).
        public bool Ready => failure == null && sim?.Open != null && sim.Open.StartupReady;
        /// Seconds since START EVENT.
        public float Age => Time.realtimeSinceStartup - startedAt;
        /// Seconds from START EVENT to Ready (-1: not yet).
        public float ReadySeconds => readyAt < 0 ? -1f : readyAt - startedAt;
        /// Where the load is, 0..1: the session's create and the event load (the worker's Starting), then its load states, then ready. An estimate
        /// of the time left (the core reports no progress of its own).
        public float Progress
        {
            get
            {
                if (Ready) return 1f;
                if (sim == null || sim.Phase == OpenReflexSimulation.Phase.Starting) return 0.6f * (1f - Mathf.Exp(-Age / 3.5f));
                float t = framesAt < 0 ? 0f : Time.realtimeSinceStartup - framesAt;
                return 0.6f + 0.38f * (1f - Mathf.Exp(-t / 2.5f));
            }
        }

        /// The shell's events this can start: the open core's exhibition events (races, Free Ride, the challenges' Free Ride); not the career,
        /// the test ride or MotoSkills (each starts its session its own way). why: the reason it cannot.
        public static bool Supported(OpenEvent ev, out string why)
        {
            why = null;
            if (Environment.GetEnvironmentVariable("REFLEX_LOADARENA") == "0") why = "REFLEX_LOADARENA=0";
            else if (ev?.ev == null) why = "no event";
            else if (!OpenReflexSelector.UseOpenReflex) why = "not the open core";
            else if (ReflexGarage.TestDrive.Active || ReflexTutorials.Open.Active || ReflexCareer.Current != null) why = "a test ride / tutorial / career is active";
            else if (!string.IsNullOrEmpty(ev.ev.spec.activation)) why = "a challenge";
            return why == null;
        }

        /// START EVENT: the event's session as the race scene would start it (the setup as OpenSetup.Launch hands it over). Null when it cannot
        /// (why: the reason; the caller launches the old way).
        public static OpenEventPrestart Begin(OpenEvent ev, OpenSetup setup, out string why)
        {
            Release("a new start");
            if (!Supported(ev, out why)) return null;
            var e = ev.ev;
            try
            {
                string path = RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor));
                if (path == null) { why = "the core library is missing"; return null; }
                RxNativeLoader.Load(path, Application.isEditor ? System.IO.Path.Combine(System.IO.Path.GetDirectoryName(Application.dataPath) ?? Application.dataPath, "Temp", "OpenReflex") : null);
                // what ReflexEventSelect.Launch and Awake give the world: MAP SCALE, a custom track's files, the AI, the duration, the vehicle
                RxMapScale.Set(OpenSetup.MapScaleOf(e), OpenSetup.MapScaleBase(e));   // (BASE SCALE: the track's default, an Alive race level's)
                RxPracticeMode.Set(setup.practice);   // PRACTICE (ReflexEventSelect.Launch sets the same at PRESS START)
                TrackMods.Bind(e.trackMod);
                ReflexEventSelect.LaunchAi(setup.Ai(e), out int opponents, out int aiSkill, setup.LaunchRiders(e));
                var garage = ReflexEventSelect.LaunchGarage(e, setup.Garage(e));
                var spec = OriginalWorldRuntime.OpenEventSpec(e, opponents, aiSkill, setup.EventDuration(e), garage);
                bool challenges = ReflexModes.InChallengesFreeRide(e);
                var persistence = CoreGarageProfile.CapturePersistence(garage);
                var go = new GameObject("OPEN REFLEX event prestart");
                DontDestroyOnLoad(go);
                var p = go.AddComponent<OpenEventPrestart>();
                p.eventKey = e.key; p.key = Key(spec, challenges); p.startedAt = Time.realtimeSinceStartup;
                p.sim = OpenReflexSelector.CreateOpen(ReflexInstall.Require(), spec, true, true, persistence: persistence, replayPresentation: true, inCareerMode: challenges);
                s_current = p;
                Debug.Log($"[LoadArena] {e.key}: the event's session starts in the front end ({spec.EventType}/{spec.Location}, riders {spec.riders}, AI {spec.ai_difficulty}, duration {spec.duration} {spec.DurationType}, map scale {RxMapScale.Current:0.##})");
                return p;
            }
            catch (Exception x)
            {
                why = x.Message;
                Debug.LogWarning("[LoadArena] the session could not be started in the front end: " + x.Message);
                Release("start failed");
                return null;
            }
        }

        /// The race scene's runtime: the pre-started session when it is the one it would start (the same spec, challenges mode and MAP SCALE),
        /// handed over (null: none, or another one, which is disposed).
        public static IReflexSimDriver Take(RxEventSpec spec, bool challenges)
        {
            var p = s_current;
            if (!p || p.sim == null) return null;
            string want = Key(spec, challenges);
            if (want != p.key)
            {
                Debug.LogWarning("[LoadArena] the pre-started session is not this world's event; it is closed\n  started: " + p.key + "\n  wanted:  " + want);
                Release("another event");
                return null;
            }
            var sim = p.sim; p.sim = null;
            Debug.Log($"[LoadArena] {p.eventKey}: the race scene takes the pre-started session over ({(p.Ready ? "loaded" : sim.Phase.ToString().ToLowerInvariant())}, {p.Age:F2} s after START EVENT{(p.launchedAt > 0 ? $", {Time.realtimeSinceStartup - p.launchedAt:F2} s after PRESS START" : "")})");
            s_current = null;
            Destroy(p.gameObject);
            return sim;
        }

        /// PRESS START: the race scene is loading (from now on a session nobody takes is closed after a while).
        public void Launched() { if (launchedAt < 0) launchedAt = Time.realtimeSinceStartup; }

        /// Closes the pre-started session (the arena was left, the app quits, nobody took it).
        public static void Release(string why)
        {
            var p = s_current; s_current = null;
            if (!p) return;
            if (p.sim != null)
            {
                Debug.Log($"[LoadArena] {p.eventKey}: the pre-started session is closed ({why})");
                try { p.sim.Dispose(); } catch (Exception e) { Debug.LogWarning("[LoadArena] close: " + e.Message); }
                p.sim = null;
            }
            Destroy(p.gameObject);
        }

        static string Key(RxEventSpec s, bool challenges) => string.Join("|", s.EventType, s.event_type_id, s.EventActivation, s.Location, s.location_id, s.track_id,
            s.duration, s.DurationType, s.riders, s.ai_difficulty, s.vehicle_class, s.PlayerMode, s.PlayerName, challenges ? "challenges" : "-",
            RxMapScale.Current.ToString("R", System.Globalization.CultureInfo.InvariantCulture), RxMapScale.Base.ToString("R", System.Globalization.CultureInfo.InvariantCulture),
            RxCustomTrack.For(s.Location) ? RxCustomTrack.Folder : "-",
            RxPracticeMode.Current ? "practice" : "-");

        // The event's load states: frames as OriginalWorldRuntime.PollOpenStartup submits them (no input, the fixed step), until the snapshot.
        void Update()
        {
            if (sim == null) return;
            if (launchedAt > 0 && Time.realtimeSinceStartup - launchedAt > 60f) { Release("the race scene never took it"); return; }
            if (failure != null || sim.Phase == OpenReflexSimulation.Phase.Failed) return;
            try
            {
                var done = sim.Collect();
                if (done != null) { string error = done.error; sim.Recycle(done); if (error != null) { failure = error; Debug.LogWarning("[LoadArena] " + eventKey + ": " + error); return; } }
                if (sim.Open.StartupReady)
                {
                    if (readyAt < 0) { readyAt = Time.realtimeSinceStartup; Debug.Log($"[LoadArena] {eventKey}: loaded in {ReadySeconds:F2} s (session and event {(framesAt > 0 ? framesAt - startedAt : 0):F2} s, load states {(framesAt > 0 ? readyAt - framesAt : 0):F2} s)"); }
                    return;
                }
                if (sim.Phase == OpenReflexSimulation.Phase.Ready && sim.Idle)
                {
                    if (framesAt < 0) framesAt = Time.realtimeSinceStartup;
                    sim.Submit(OriginalPresentation.Step, keys, pads, 0);
                }
            }
            catch (Exception e) { failure = e.Message; Debug.LogWarning("[LoadArena] " + eventKey + ": " + e.Message); }
        }

        void OnApplicationQuit() { if (s_current == this) Release("quit"); }
        void OnDestroy()
        {
            if (sim != null) { try { sim.Dispose(); } catch (Exception) { } sim = null; }
            if (s_current == this) s_current = null;
        }
    }
}
