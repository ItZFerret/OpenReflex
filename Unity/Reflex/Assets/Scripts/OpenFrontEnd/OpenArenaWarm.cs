// The test-ride arena's session kept warm in the menus (w-arenafast; a player's report: "The ridable loading screen takes too long to load,
// is there a way to make that faster?"). START EVENT used to start all of the arena's ride behind the black at once: the core's second image
// (RxNativeLoader.LoadArena: a copy of the library loaded under another name), then, once the race scene had loaded, its session, the profile's
// import and the test drive's start in the UI world with its first frames, beside the event's own session starting at the same moment (two
// sessions reading the install, the race scene's setup and its audio on the CPU and the disk together). Now the arena's session is made BEFORE:
// while the player is in the menus (the front end idle, the 3D menu world up) the second image is loaded on a worker, the test drive's session
// is created in it and run to its startup snapshot (the vehicle and rider spawned, the world's first state captured), then it waits on its
// worker costing no CPU. START EVENT claims it and the race scene's runtime takes it over (OriginalWorldRuntime.CreateOpen) as it would a new
// session of its own: the ride starts at the race scene's first frames, and the event's load has the CPU and the disk to itself.
//   staleness  the warm session is the one the race scene would start only for the same inputs, its key: the spec (the test drive's event with
//              the vehicle the event in view would ride: OpenLoadArena.Selection), the saved profile (CoreGarageProfile.SessionStamp: a garage
//              change commits a new snapshot) and the client's settings a session takes at its start (OpenReflexSimulation.StartupSettingsStamp:
//              the game's UI settings, the air helpers, the borders, the controller bindings). Any change makes a new one (the old one is
//              closed on a worker); START EVENT with another key (or none warm) closes it and starts one at once, before the fade.
//   lifetime   one session in the second image (the core's one arena per image; the main image's sessions are not affected: the garage's
//              profile edits wait only for those): kept while the front end is up, closed when the front end goes any other way (a race,
//              the career, the garage's TEST RIDE), made again back in the menus. Claimed and never taken: closed after 60 s.
//   cost       the second image's arena with the compound's session stays resident while the menus are up: about 120 MB on the
//              developer's Mac (808 -> 926 MB); no CPU while it waits. Not warmed with less than 8 GB of memory (Enabled).
//   off        REFLEX_LOADARENA_WARM=0 (the session starts at START EVENT, before the fade); where the arena cannot ride at all
//              (OpenLoadArena.Begin: REFLEX_LOADARENA_RIDE=0, a custom track, the second image unavailable) nothing is warmed; a profile
//              without a saved snapshot yet (the first session migrates it) is not warmed either.
using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OpenArenaWarm : MonoBehaviour
    {
        static OpenArenaWarm s_current;
        static Task<string> s_image;            // the second image's copy and load on a worker (once a run): null when loaded, else why not
        static double s_imageMs = -1, s_nextCheck, s_wantSince;
        static string s_wantKey;
        static int s_failures;
        IReflexSimDriver sim;
        string key, specKey, failure;
        bool claimed, readyLogged;
        double startedAt, readyAt = -1, claimedAt = -1, backgroundAt;
        readonly byte[] keys = new byte[256];
        readonly ReflexPad[] pads = new ReflexPad[4];

        /// The arena's session made ahead (warm in the menus, or claimed by START EVENT until the race scene takes it); null: none.
        public static OpenArenaWarm Current => s_current;
        /// REFLEX_LOADARENA_WARM=0: nothing is made in the menus (the arena's session starts at START EVENT); nor on a PC with less than 8 GB
        /// of memory (the warm session keeps the second image's arena resident: about 120 MB on the developer's Mac).
        public static bool Enabled => Environment.GetEnvironmentVariable("REFLEX_LOADARENA_WARM") != "0" && (SystemInfo.systemMemorySize <= 0 || SystemInfo.systemMemorySize >= 7600)
                                      && !Reflex.Port.Content.ReflexDataPack.Running;   // (not while PACK GAME FILES moves the library's files into archives)
        /// Its startup snapshot is captured (the vehicle and rider are in the world): the race scene starts the ride at once.
        public bool Ready => failure == null && sim?.Open != null && sim.Open.StartupReady;
        public bool Claimed => claimed;
        public string Failure => failure ?? (sim?.Phase == OpenReflexSimulation.Phase.Failed ? sim.Failure ?? "the session failed" : null);
        /// Seconds since it was made / from made to ready (-1: not yet).
        public double Age => Time.realtimeSinceStartupAsDouble - startedAt;
        public double ReadySeconds => readyAt < 0 ? -1 : readyAt - startedAt;

        internal static string ImageFolder => Path.Combine(Application.isEditor ? Path.Combine(Path.GetDirectoryName(Application.dataPath) ?? Application.dataPath, "Temp") : Application.temporaryCachePath, "OpenReflexArena");

        /// The main core library (the second image is a copy of it); false when it is missing.
        static bool MainLibrary()
        {
            if (RxNativeLoader.IsLoaded) return true;
            string path = RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor));
            if (path == null) return false;
            RxNativeLoader.Load(path, Application.isEditor ? Path.Combine(Path.GetDirectoryName(Application.dataPath) ?? Application.dataPath, "Temp", "OpenReflex") : null);
            return true;
        }

        /// START EVENT: the second image (loaded in the menus, else now as before). False with why when it cannot be; how: for the log.
        internal static bool LoadImage(out string why, out string how)
        {
            bool menus = s_image != null; double waited = 0;
            if (menus && !s_image.IsCompleted) { var w0 = Stopwatch.StartNew(); s_image.Wait(); waited = w0.Elapsed.TotalMilliseconds; }
            var w = Stopwatch.StartNew();
            bool ok = RxNativeLoader.LoadArena(ImageFolder, out why);   // (at once when loaded: the same main library)
            double ms = w.Elapsed.TotalMilliseconds;
            how = ok && menus && ms < 5 ? $"loaded in the menus ({s_imageMs:F0} ms on a worker{(waited > 0 ? $", {waited:F0} ms waited for its end" : "")})"
                                       : $"{(ok ? "copied and loaded" : "not loaded")} now in {ms:F0} ms";
            if (!menus) s_image = Task.FromResult(ok ? null : why ?? "?");
            return ok;
        }

        /// The test drive's spec and session key for the ride START EVENT would start now with this vehicle (selection: OpenLoadArena.Selection);
        /// key null when it cannot be compared (no saved profile snapshot yet). specKey: what the race scene's runtime checks at Take.
        static RxEventSpec SpecFor(ReflexWorldFiles.Garage selection, out string key, out string specKey)
        {
            var ev = ReflexGarage.TestDrive.EventOf("arena");   // (its key names the save: not part of the spec)
            var garage = ReflexEventSelect.LaunchGarage(ev, selection);   // (the race scene's runtime: its AI off, no laps, this garage)
            var spec = OriginalWorldRuntime.OpenEventSpec(ev, 0, 0, 0, garage);
            specKey = SpecKey(spec, garage);
            Reflex.Port.Options.ReflexClientLive.SyncGameSettings();   // (what OpenReflexDriver gives every new session)
            string profile = CoreGarageProfile.SessionStamp(), settings = OpenReflexSimulation.StartupSettingsStamp(Reflex.Port.FrontEnd.CoreRemapping.StartupValues());
            key = profile == null ? null : specKey + "\n" + profile + "\n" + settings;
            return spec;
        }

        static string SpecKey(RxEventSpec s, ReflexWorldFiles.Garage g) => string.Join("|", s.EventType, s.event_type_id, s.Location, s.location_id, s.track_id, s.duration,
            s.DurationType, s.riders, s.ai_difficulty, s.vehicle_class, s.PlayerMode, s.PlayerName,
            g == null ? "-" : $"{g.riderOwner}/{g.riderId}/{g.riderGearSet}/{g.vehicleOwner}/{g.vehicleId}/{g.vehicleTypeId}/{g.vehicleClass}");

        /// Tick would check now (it checks once a second): the caller looks the selection up only then.
        public static bool Due => Time.realtimeSinceStartupAsDouble >= s_nextCheck;

        /// The front end is idle (OpenShell.ArenaWarmUpdate): keep the arena's session warm for the ride START EVENT would start (selection:
        /// the vehicle of the event in view). Checked once a second; a change of its key makes a new one.
        public static void Tick(ReflexWorldFiles.Garage selection)
        {
            double now = Time.realtimeSinceStartupAsDouble;
            if (now < s_nextCheck) return;
            s_nextCheck = now + 1.0;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("arena warm key");
            if (!Enabled || s_failures >= 2 || !OpenLoadArena.CanRide(out _)) return;
            if (s_current && s_current.claimed) return;
            try
            {
                if (s_image == null)
                {
                    if (!MainLibrary()) return;
                    string folder = ImageFolder;
                    s_image = Task.Run(() =>
                    {
                        var w = Stopwatch.StartNew();
                        bool ok = RxNativeLoader.LoadArena(folder, out string why);
                        s_imageMs = w.Elapsed.TotalMilliseconds;
                        return ok ? null : why ?? "?";
                    });
                    return;
                }
                if (!s_image.IsCompleted) return;
                if (s_image.Result != null) { if (s_failures++ == 0) Debug.Log("[LoadArena] not kept warm: the core's second image: " + s_image.Result); s_failures = 2; return; }
                var spec = SpecFor(selection, out string key, out string specKey);
                if (key == null) return;   // (no saved profile snapshot yet)
                // the same ride wanted for 2 s (browsing the events' cards of other vehicles does not make and close a session for each)
                if (key != s_wantKey) { s_wantKey = key; s_wantSince = now; }
                if (now - s_wantSince < 2.0) return;
                var c = s_current;
                if (c)
                {
                    if (c.key == key && c.Failure == null) return;
                    if (c.Failure != null) s_failures++;
                    Release(c.Failure != null ? "it failed: " + c.Failure : "the vehicle, the profile or the settings changed");
                    return;   // (made again at the next check)
                }
                Create(key, specKey, spec, selection, false, "made in the menus (the core's second image loaded in " + s_imageMs.ToString("F0") + " ms on a worker)");
            }
            catch (Exception e) { s_failures++; Debug.LogWarning("[LoadArena] the arena could not be kept warm: " + e.Message); }
        }

        /// START EVENT (OpenLoadArena.Begin, the second image loaded): the arena's session for this ride: the warm one when it is this ride's,
        /// else one started now. False (why) when none can be started: the race scene's runtime then starts its own (as before).
        internal static bool Claim(ReflexWorldFiles.Garage selection, out string how)
        {
            try
            {
                var spec = SpecFor(selection, out string key, out string specKey);
                var c = s_current;
                if (c && !c.claimed && key != null && c.key == key && c.Failure == null)
                {
                    c.claimed = true; c.claimedAt = Time.realtimeSinceStartupAsDouble;
                    c.sim?.Open?.SetBackground(false);   // (the ride is wanted now: its worker's own priority again)
                    how = c.Ready ? $"warm: made in the menus {c.Age:F1} s ago, ready {c.ReadySeconds * 1000:F0} ms after it was made" : $"warm: made {c.Age * 1000:F0} ms ago, still starting";
                    return true;
                }
                string why = !Enabled ? "REFLEX_LOADARENA_WARM=0" : !c ? (s_failures >= 2 ? "warming failed earlier" : key == null ? "no saved profile snapshot to compare" : "none warm yet")
                           : c.claimed ? "the warm one is another launch's" : key == null ? "no saved profile snapshot to compare"
                           : c.Failure != null ? "the warm one failed: " + c.Failure : "the warm one was for another vehicle, profile or settings";
                if (c) Release("START EVENT: " + why);
                Create(key ?? specKey, specKey, spec, selection, true, "started at START EVENT (" + why + ")");
                how = "started now (" + why + ")";
                return true;
            }
            catch (Exception e) { how = "none: " + e.Message; Debug.LogWarning("[LoadArena] the arena's session could not be started: " + e.Message); return false; }
        }

        static void Create(string key, string specKey, RxEventSpec spec, ReflexWorldFiles.Garage selection, bool claim, string what)
        {
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("arena session made");
            Reflex.Port.Platform.ReflexMenuPerf.Note("arena session made");
            var persistence = new ArenaRidePersistence(CoreGarageProfile.CapturePersistence(selection));
            var go = new GameObject("OPEN REFLEX load arena session");
            DontDestroyOnLoad(go);
            var w = go.AddComponent<OpenArenaWarm>();
            w.key = key; w.specKey = specKey; w.startedAt = Time.realtimeSinceStartupAsDouble; w.backgroundAt = Reflex.Port.Platform.ReflexMenuPerf.BackgroundSeconds;
            if (claim) { w.claimed = true; w.claimedAt = w.startedAt; }
            s_current = w;
            // what the race scene's runtime makes for the arena's ride (OriginalWorldRuntime.CreateOpen: the test drive in the second image)
            w.sim = OpenReflexSelector.CreateOpen(ReflexInstall.Require(), spec, true, true, persistence: persistence, testDrive: true, arenaImage: true);
            if (!claim) w.sim.Open?.SetBackground(true);   // (made ahead: nothing waits for it, not above the menus' threads; w-menufps)
            Debug.Log("[LoadArena] the arena's session is " + what + (selection != null ? ", vehicle " + selection.vehicleId : ", the equipped vehicle"));
        }

        /// The race scene's runtime (the arena's ride): the claimed session when it is this ride's (the spec, the vehicle), handed over (null:
        /// none, or another one, which is closed).
        public static IReflexSimDriver Take(RxEventSpec spec, ReflexWorldFiles.Garage garage)
        {
            var c = s_current;
            if (!c || !c.claimed || c.sim == null) return null;
            string want = SpecKey(spec, garage);
            if (want != c.specKey)
            {
                Debug.LogWarning("[LoadArena] the arena's session made ahead is not this ride's; it is closed\n  made:   " + c.specKey + "\n  wanted: " + want);
                Release("another ride");
                return null;
            }
            var sim = c.sim; c.sim = null;
            Debug.Log($"[LoadArena] the race scene takes the arena's session over ({(c.Ready ? "ready" : sim.Phase.ToString().ToLowerInvariant())}, {(Time.realtimeSinceStartupAsDouble - c.claimedAt) * 1000:F0} ms after START EVENT)");
            s_current = null;
            Destroy(c.gameObject);
            return sim;
        }

        /// Closes the session made ahead (on a worker: its destroy never holds a frame).
        public static void Release(string why)
        {
            var c = s_current; s_current = null;
            if (!c) return;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("arena session closed");
            Reflex.Port.Platform.ReflexMenuPerf.Note("arena session closed");
            var sim = c.sim; c.sim = null;
            if (sim != null)
            {
                Debug.Log("[LoadArena] the arena's session made ahead is closed (" + why + ")");
                Task.Run(() => { try { sim.Dispose(); } catch (Exception e) { Debug.LogWarning("[LoadArena] close: " + e.Message); } });
            }
            Destroy(c.gameObject);
        }

        // Its startup's frames as the race scene's runtime submits them (OriginalWorldRuntime.PollOpenStartup: no input, the fixed step) until
        // the startup snapshot; then it waits for the race scene.
        void Update()
        {
            if (sim == null) return;
            if (!claimed && !OpenShell.Instance) { Release("the front end closed"); return; }   // (a race, the career, a test ride)
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("arena session frames");
            if (claimed && Time.realtimeSinceStartupAsDouble - claimedAt > 60) { Release("the race scene never took it"); return; }
            if (failure != null || sim.Phase == OpenReflexSimulation.Phase.Failed) return;
            try
            {
                var done = sim.Collect();
                if (done != null) { string error = done.error; sim.Recycle(done); if (error != null) { failure = error; Debug.LogWarning("[LoadArena] the arena's session: " + error); return; } }
                if (sim.Open.StartupReady)
                {
                    if (readyAt < 0) readyAt = Time.realtimeSinceStartupAsDouble;
                    if (!readyLogged)
                    {
                        readyLogged = true; var o = sim.Open;
                        // (the window in the background: the game does not run there, so the frames the main thread submits wait; w-menufps: a report's
                        // "first frames 674765 ms" was a player in another window for eleven minutes)
                        double away = Reflex.Port.Platform.ReflexMenuPerf.BackgroundSeconds - backgroundAt;
                        Debug.Log($"[LoadArena] the arena's session is ready in {ReadySeconds * 1000:F0} ms (session create {o.StartupCreateMs:F0} ms, test drive start {o.StartupLoadMs:F0} ms, its first frames {o.StartupFramesMs:F0} ms){(away >= 0.5 ? $"; {away:F1} s of it with the game in the background" : "")}{(claimed ? "" : ": it waits for START EVENT")}");
                    }
                    return;
                }
                if (sim.Phase == OpenReflexSimulation.Phase.Ready && sim.Idle) sim.Submit(OriginalPresentation.Step, keys, pads, 0);
            }
            catch (Exception e) { failure = e.Message; Debug.LogWarning("[LoadArena] the arena's session: " + e.Message); }
        }

        void OnApplicationQuit() { if (s_current == this) { s_current = null; var s = sim; sim = null; try { s?.Dispose(); } catch (Exception) { } } }
        void OnDestroy()
        {
            if (sim != null) { var s = sim; sim = null; Task.Run(() => { try { s.Dispose(); } catch (Exception) { } }); }
            if (s_current == this) s_current = null;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Reset() { s_current = null; s_nextCheck = 0; s_failures = 0; s_wantKey = null; }   // (domain reload is off; the image stays loaded)
    }
}
