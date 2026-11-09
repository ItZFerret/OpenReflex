// TEST ONLY (tests/gc_spikes, run.py): managed allocations, GC count and the managed heap of real races, back to back in one player
// (w-gcspikes). REFLEX_GCSPIKE_SCRIPT is a comma list of steps:
//   ride:<event>       the event with REFLEX_GCSPIKE_AI computer riders, ridden to its natural finish with ordinary mapped keys
//                      (MotoInputDriver, exhibition mode: National/NAT_Stillwater only), the end-of-race replay and the results page held
//                      REFLEX_GCSPIKE_RESULTS seconds, then RETURN TO MENU (the results page's own control)
//   sit:<event>:<s>    the event loaded and run <s> seconds from the start with the throttle held, then QUIT to the front end
// After each step, back in the front end: 5 s, then a forced full collection and "[gcspike] HEAP ..." (the managed heap in use after the
// collection, Unity objects by type). Every frame: GC Allocated In Frame (a development player), the GC count and the frame time, summed per
// phase ("ride" = racing, "post" = finished / results, "menu"); "[gcspike] PHASE ..." lines at each phase change.
// REFLEX_GCSPIKE_PROFILE=<dir>: profiler captures (binary logs, GC.Alloc call stacks) of the first ride step: ride.raw (riding, from 40 s of
// race time, REFLEX_GCSPIKE_PROFILE_SECONDS long) and results.raw (the results page from 3 s). The world and the save are the run's scratch
// copies; it presses only mapped keys and the results page's control.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Unity.Profiling;
using UnityEngine;
using UnityEngine.Profiling;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

[DefaultExecutionOrder(32000)]
public sealed class GcSpikeProbe : MonoBehaviour
{
    const string Tag = "[gcspike] ";
    static string Script => Environment.GetEnvironmentVariable("REFLEX_GCSPIKE_SCRIPT");
    static int EnvInt(string n, int d) => int.TryParse(Environment.GetEnvironmentVariable(n), out var v) ? v : d;
    static string Inv(FormattableString f) => f.ToString(CultureInfo.InvariantCulture);

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Guest()
    {
        if (string.IsNullOrEmpty(Script)) return;
        Reflex.Port.Content.ReflexProfile.Disabled = true;   // the guest profile (the scratch save only)
        OpenShell.SkipIntro = true;
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        if (string.IsNullOrEmpty(Script) || FindAnyObjectByType<GcSpikeProbe>()) return;
        var go = new GameObject("GC spike probe (test only)"); DontDestroyOnLoad(go); go.AddComponent<GcSpikeProbe>();
        Application.runInBackground = true;
    }

    // ------------------------------------------------------------------------------------------------ per-frame accounting
    sealed class Acc
    {
        public int frames, gcs, over2x, gcFrames; public double seconds, worst, alloc, monoGrowth, gcFrameSum, gcFrameMax; public long allocMax;
        public void Add(double dt, long a, int gc, double growth)
        {
            frames++; seconds += dt; if (dt > worst) worst = dt; alloc += a; if (a > allocMax) allocMax = a; gcs += gc; monoGrowth += growth;
            if (gc > 0) { gcFrames++; gcFrameSum += dt; if (dt > gcFrameMax) gcFrameMax = dt; }   // the frame a collection ended in
        }
        public string Line()
        {
            double mean = frames > 0 ? seconds / frames : 0, min = seconds / 60.0;
            return Inv($"frames {frames} over {seconds:0.0} s, mean {mean * 1000:0.00} ms, worst {worst * 1000:0.0} ms, frames over 2x mean {over2x}, gc alloc {(frames > 0 ? alloc / frames / 1024 : 0):0.00} KB/frame ({(seconds > 0 ? alloc / seconds / 1048576 : 0):0.000} MB/s, max {allocMax / 1024.0:0.0} KB), mono growth {(seconds > 0 ? monoGrowth / seconds / 1048576 : 0):0.000} MB/s, gc {gcs} ({(min > 0 ? gcs / min : 0):0.00}/min), frames a collection ended in: mean {(gcFrames > 0 ? gcFrameSum / gcFrames * 1000 : 0):0.0} ms, max {gcFrameMax * 1000:0.0} ms");
        }
    }
    ProfilerRecorder gcAlloc;
    readonly Dictionary<string, Acc> phases = new Dictionary<string, Acc>();
    readonly Dictionary<string, Acc> totals = new Dictionary<string, Acc>();
    string phase = "boot", step = "boot";
    int gc0; long mono0; double meanDt = 1 / 60.0;

    void Awake()
    {
        gcAlloc = ProfilerRecorder.StartNew(ProfilerCategory.Memory, "GC Allocated In Frame");
        gc0 = GC.CollectionCount(0); mono0 = Profiler.GetMonoUsedSizeLong();
        Debug.Log(Tag + "BEGIN script=" + Script + " gcAllocRecorder=" + gcAlloc.Valid + " incremental=" + UnityEngine.Scripting.GarbageCollector.isIncremental
                  + " slice=" + UnityEngine.Scripting.GarbageCollector.incrementalTimeSliceNanoseconds + " ns, development=" + Debug.isDebugBuild + " batch=" + Application.isBatchMode);
        AudioListener.volume = 0f;
        StartCoroutine(Run());
    }

    void OnDestroy() { gcAlloc.Dispose(); }

    void SetPhase(string p)
    {
        if (p == phase) return;
        if (phases.TryGetValue(phase, out var a) && a.frames > 0) Debug.Log(Tag + "PHASE " + step + " " + phase + ": " + a.Line());
        phases.Remove(phase);
        phase = p;
    }

    void Update()
    {
        AudioListener.volume = 0f;   // the test player is silent (the person at the computer; the game's own volume settings are left alone)
        double dt = Time.unscaledDeltaTime;
        int gc = GC.CollectionCount(0); long mono = Profiler.GetMonoUsedSizeLong();
        long a = gcAlloc.Valid ? gcAlloc.LastValue : 0;
        int dgc = gc - gc0; double growth = dgc == 0 && mono > mono0 ? mono - mono0 : 0;
        gc0 = gc; mono0 = mono;
        if (!phases.TryGetValue(phase, out var acc)) phases[phase] = acc = new Acc();
        string tk = (phase == "ride" || phase == "post") ? phase : "other";
        if (!totals.TryGetValue(tk, out var tot)) totals[tk] = tot = new Acc();
        if (Time.frameCount > 10)
        {
            if (dt > 2 * meanDt && acc.frames > 30) { acc.over2x++; tot.over2x++; }
            acc.Add(dt, a, dgc, growth); tot.Add(dt, a, dgc, growth);
            meanDt = meanDt * 0.98 + Math.Min(dt, 0.1) * 0.02;
        }
        if (phase == "post" && dt > 0.1) Debug.Log(Tag + Inv($"HITCH {dt * 1000:0} ms in {step} post at frame {Time.frameCount}, gc +{dgc}"));
    }

    // A batch player draws nothing: the end-of-race replay's fade layer (OwnReplayRenderer) waits for its UI Toolkit paint before the
    // replay goes on, so its paint callback is called here as a drawn frame would (the batch run only; a windowed player paints it).
    static readonly System.Reflection.FieldInfo RendererField = typeof(OriginalWorldRuntime).GetField("replayRenderer", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
    static readonly System.Reflection.FieldInfo FadeField = typeof(OwnReplayRenderer).GetField("fade", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
    RenderTexture batchTarget;
    void LateUpdate()
    {
        // (only after the finish: no reflection or extra rendering in the measured ride)
        if (!Application.isBatchMode || phase != "post" && phase != "quit") return;
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        if (!rt) return;
        PaintReplayFade(rt);
        // the world camera drawn (a batch player draws nothing by itself): the replay's paint receipts come from its end of rendering
        var cam = rt.chaseCamera;
        if (!cam) return;
        if (!batchTarget) batchTarget = new RenderTexture(320, 180, 24);
        var prior = cam.targetTexture; cam.targetTexture = batchTarget;
        try { cam.Render(); } finally { cam.targetTexture = prior; }
    }

    void PaintReplayFade(OriginalWorldRuntime rt)
    {
        if (!rt || RendererField == null || FadeField == null) return;
        var r = RendererField.GetValue(rt); if (r == null) return;
        var fade = FadeField.GetValue(r) as VisualElement;
        if (fade == null || fade.parent == null) return;
        var t = fade.GetType();
        var painted = t.GetField("Painted")?.GetValue(fade) as Action<ulong>;
        var epoch = t.GetField("Epoch")?.GetValue(fade);
        if (painted != null && epoch is ulong e) painted(e);
    }

    // ------------------------------------------------------------------------------------------------ the script
    IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }

    IEnumerator Run()
    {
        float t0 = Time.realtimeSinceStartup;
        while (!OpenShell.Instance) { if (Time.realtimeSinceStartup - t0 > 300) { Fail("no shell"); yield break; } yield return null; }
        SetPhase("menu");
        yield return Seconds(5);
        yield return Heap("start");
        var steps = Script.Split(',').Select(s => s.Trim()).Where(s => s.Length > 0).ToArray();
        for (int i = 0; i < steps.Length; i++)
        {
            var parts = steps[i].Split(':');
            step = (i + 1) + "-" + parts[0] + "-" + parts[1].Split('/').Last();
            if (parts[0] == "ride") yield return Ride(parts[1], i == 0 || i == FirstRide(steps));
            else if (parts[0] == "sit") yield return Sit(parts[1], parts.Length > 2 ? float.Parse(parts[2], CultureInfo.InvariantCulture) : 30);
            else { Fail("unknown step " + steps[i]); yield break; }
            if (failed) yield break;
            SetPhase("menu");
            float tq = Time.realtimeSinceStartup;
            while (FindAnyObjectByType<OriginalWorldRuntime>() || SceneManager.GetActiveScene().name != "FrontEnd" || !OpenShell.Instance)
            {
                if (Time.realtimeSinceStartup - tq > 120) { Fail("did not return to the front end"); yield break; }
                yield return null;
            }
            yield return Seconds(5);
            yield return Heap(step);
        }
        SetPhase("end");
        foreach (var kv in totals) Debug.Log(Tag + "TOTAL " + kv.Key + ": " + kv.Value.Line());
        Debug.Log(Tag + "RESULT PASS");
        yield return Seconds(1);
        Application.Quit(0);
    }

    static int FirstRide(string[] steps) { for (int i = 0; i < steps.Length; i++) if (steps[i].StartsWith("ride:")) return i; return -1; }

    IEnumerator Heap(string label)
    {
        long before = Profiler.GetMonoUsedSizeLong();
        float c0 = Time.realtimeSinceStartup;
        GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
        float cms = (Time.realtimeSinceStartup - c0) * 1000;
        yield return null;
        long used = Profiler.GetMonoUsedSizeLong(), heap = Profiler.GetMonoHeapSizeLong();
        string Count<T>() where T : UnityEngine.Object
        {
            var all = Resources.FindObjectsOfTypeAll<T>(); long bytes = 0; foreach (var o in all) bytes += Profiler.GetRuntimeMemorySizeLong(o);
            return Inv($"{typeof(T).Name} {all.Length} ({bytes / 1048576.0:0.0} MB)");
        }
        retained.Clear();
        Debug.Log(Tag + "RETAINED destroyed objects still reachable after the collection: " + Alive());
        if (retained.Count > 0 && Environment.GetEnvironmentVariable("REFLEX_GCSPIKE_HOLDERS") == "1")
        {
            float h0 = Time.realtimeSinceStartup;
            string holders; try { holders = Holders(retained); } catch (Exception e) { holders = "walk failed: " + e.Message; }
            Debug.Log(Tag + Inv($"HOLDERS ({Time.realtimeSinceStartup - h0:0.0} s): ") + holders);
        }
        retained.Clear();
        Debug.Log(Tag + Inv($"HEAP after {label}: mono used {used / 1048576.0:0.0} MB after a full collection ({cms:0} ms; {before / 1048576.0:0.0} MB before), mono heap {heap / 1048576.0:0.0} MB, unity alloc {Profiler.GetTotalAllocatedMemoryLong() / 1048576.0:0} MB | ")
                  + string.Join(", ", Count<GameObject>(), Count<Mesh>(), Count<Texture2D>(), Count<RenderTexture>(), Count<Material>(), Count<AudioClip>()));
    }

    // the previous world / shell after it went away: still reachable after a full collection = retained by something (a leak)
    readonly List<(string what, WeakReference wr)> watched = new List<(string, WeakReference)>();
    readonly List<object> retained = new List<object>();
    void Watch(string what, object o) { if (o != null) watched.Add((what, new WeakReference(o))); }
    void WatchWorld(OriginalWorldRuntime rt)
    {
        Watch(step + " world", rt);
        foreach (var c in rt.GetComponents<Component>()) if (c is Reflex.Port.Replay.ReflexReplay) Watch(step + " replay", c);
        var aud = rt.GetComponentInChildren<OriginalAudio>(true); Watch(step + " world audio", aud);
    }
    void WatchShell() { var sh = OpenShell.Instance; if (sh) { Watch("shell#" + Time.frameCount, sh); foreach (var a in sh.GetComponents<OriginalAudio>()) Watch("shell audio#" + Time.frameCount, a); } }
    // who holds a retained object: a breadth-first walk from every static field of the loaded assemblies (and the live Unity objects'
    // fields: a MonoBehaviour holding it is reported as such) through instance fields, arrays and delegates; the first path to each target
    sealed class RefEq : IEqualityComparer<object> { public new bool Equals(object a, object b) => ReferenceEquals(a, b); public int GetHashCode(object o) => System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(o); }
    static readonly Dictionary<Type, System.Reflection.FieldInfo[]> s_fields = new Dictionary<Type, System.Reflection.FieldInfo[]>();
    static System.Reflection.FieldInfo[] Fields(Type t)
    {
        if (s_fields.TryGetValue(t, out var f)) return f;
        var list = new List<System.Reflection.FieldInfo>();
        for (var b = t; b != null && b != typeof(object); b = b.BaseType)
            foreach (var fi in b.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.DeclaredOnly))
                if (Walkable(fi.FieldType)) list.Add(fi);
        return s_fields[t] = list.ToArray();
    }
    static readonly Dictionary<Type, bool> s_walk = new Dictionary<Type, bool>();
    // a value can lead to another object: not a primitive / string / reflection object, and a struct or an array element type only when it
    // holds such a field itself
    static bool Walkable(Type t)
    {
        if (s_walk.TryGetValue(t, out bool w)) return w;
        s_walk[t] = false;   // (a cycle through value types cannot happen; a guard for recursion while working it out)
        if (t.IsPrimitive || t.IsEnum || t.IsPointer || t == typeof(string) || t == typeof(IntPtr) || t == typeof(UIntPtr) || t == typeof(decimal) || t == typeof(DateTime)
            || typeof(System.Reflection.MemberInfo).IsAssignableFrom(t) || t == typeof(Type)) w = false;
        else if (t.IsArray) w = Walkable(t.GetElementType());
        else if (t.IsValueType)
        {
            w = false;
            foreach (var fi in t.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic))
                if (Walkable(fi.FieldType)) { w = true; break; }
        }
        else w = true;
        return s_walk[t] = w;
    }
    string Holders(List<object> targets)
    {
        var parent = new Dictionary<object, (object from, string edge)>(new RefEq());
        var queue = new Queue<object>();
        var found = new List<string>();
        var want = new HashSet<object>(targets, new RefEq());
        void Visit(object o, object from, string edge)
        {
            if (o == null || o is string || o is Type || o is System.Reflection.MemberInfo || o is System.Reflection.Assembly || parent.ContainsKey(o)) return;
            var t = o.GetType(); if (!Walkable(t) && !want.Contains(o)) return;
            parent[o] = (from, edge);
            if (want.Contains(o))
            {
                var path = new List<string>(); object at = o; int guard = 0;
                while (at != null && guard++ < 40) { var (f, e) = parent[at]; path.Add(e); at = f; }
                path.Reverse(); found.Add(o.GetType().Name + " <= " + string.Join(" . ", path)); want.Remove(o);
            }
            queue.Enqueue(o);
        }
        int roots = 0;
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            string an = asm.GetName().Name;
            if (an.StartsWith("System") || an == "mscorlib" || an == "netstandard" || an.StartsWith("Mono.") || an.StartsWith("nunit")) continue;
            Type[] types; try { types = asm.GetTypes(); } catch (System.Reflection.ReflectionTypeLoadException e) { types = e.Types.Where(x => x != null).ToArray(); }
            foreach (var t in types)
            {
                if (t.IsGenericTypeDefinition) continue;
                System.Reflection.FieldInfo[] sf; try { sf = t.GetFields(System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.DeclaredOnly); } catch { continue; }
                foreach (var fi in sf)
                {
                    if (fi.IsLiteral || !Walkable(fi.FieldType)) continue;
                    object v; try { v = fi.GetValue(null); } catch { continue; }
                    roots++; Visit(v, null, "static " + t.FullName + "." + fi.Name);
                }
            }
        }
        foreach (var live in FindObjectsByType<MonoBehaviour>(FindObjectsInactive.Include, FindObjectsSortMode.None))
            if (live && live != this) Visit(live, null, "live " + live.GetType().Name + " '" + live.name + "'");
        int visited = 0;
        while (queue.Count > 0 && want.Count > 0 && visited < 30000000)
        {
            var o = queue.Dequeue(); visited++;
            if (o is Array arr)
            {
                if (arr.Rank != 1) continue;
                int n = Math.Min(arr.Length, 200000);
                for (int i = 0; i < n; i++) { object e; try { e = arr.GetValue(i); } catch { break; } if (e != null && !e.GetType().IsPrimitive) Visit(e, o, "[" + i + "]"); }
                continue;
            }
            if (o is Delegate d)
            {
                foreach (var inv in d.GetInvocationList()) { Visit(inv.Target, o, "target of " + inv.Method.DeclaringType?.Name + "." + inv.Method.Name); }
                continue;
            }
            foreach (var fi in Fields(o.GetType()))
            {
                object v; try { v = fi.GetValue(o); } catch { continue; }
                Visit(v, o, fi.DeclaringType.Name + "." + fi.Name);
            }
        }
        return (found.Count == 0 ? "no path found" : string.Join(" || ", found)) + " (" + roots + " static roots, " + visited + " objects walked)";
    }

    string Alive()
    {
        var alive = new List<string>();
        foreach (var (what, wr) in watched)
        {
            object t = wr.Target;
            if (t == null) continue;
            bool destroyed = t is UnityEngine.Object uo && !uo;
            if (destroyed) { alive.Add(what); retained.Add(t); }
        }
        return alive.Count == 0 ? "none" : string.Join(", ", alive);
    }

    bool failed;
    void Fail(string why) { failed = true; Debug.LogError(Tag + "FAIL " + why); Application.Quit(1); }

    OriginalWorldRuntime World()
    {
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        if (rt && rt.FailureMessage != null) Fail("world: " + rt.FailureMessage);
        return rt;
    }

    IEnumerator LaunchEvent(string key)
    {
        int ai = EnvInt("REFLEX_GCSPIKE_AI", 5);
        Debug.Log(Tag + "STEP " + step + ": launch " + key + " ai " + ai);
        WatchShell();
        SetPhase("load");
        ReflexEventSelect.Launch(key, ai, false, "Reflex", EnvInt("REFLEX_TEST_LAPS", 3), null);
        float t0 = Time.realtimeSinceStartup;
        OriginalWorldRuntime rt = null;
        while (!(rt = World()) || !rt.Running)
        {
            if (failed) yield break;
            if (Time.realtimeSinceStartup - t0 > 240) { Fail("the world did not run"); yield break; }
            yield return null;
        }
        Debug.Log(Tag + Inv($"STEP {step}: the world runs after {Time.realtimeSinceStartup - t0:0.0} s"));
        WatchWorld(rt);
        SetPhase("intro");
        bool skipped = false;
        while (rt && rt.Running && rt.Race.state < 2)
        {
            var intro = rt.GetComponent<OwnEventIntro>();
            if (!skipped && intro && intro.Shown && Time.realtimeSinceStartup - t0 > 3) { intro.Skip(); skipped = true; }
            if (Time.realtimeSinceStartup - t0 > 300) { Fail("no race start"); yield break; }
            yield return null;
        }
    }

    IEnumerator Sit(string key, float seconds)
    {
        yield return LaunchEvent(key);
        if (failed) yield break;
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        SetPhase("ride");
        var held = new[] { ThrottleScan() };
        float t0 = Time.realtimeSinceStartup;
        while (rt && rt.Running && Time.realtimeSinceStartup - t0 < seconds) { rt.testHeldKeys = held; yield return null; }
        if (rt) rt.testHeldKeys = Array.Empty<int>();
        SetPhase("quit");
        OriginalPauseMenu.QuitToFrontEnd();
    }

    static int ThrottleScan()
    {
        var t = Reflex.Port.FrontEnd.CoreRemapping.Read(false);
        int col = Array.IndexOf(t.columns, "Throttle");
        for (int i = 1; i < 256; i++) if (t.scan[i] == t.kb[col]) return i;
        return 0;
    }

    IEnumerator Ride(string key, bool profile)
    {
        yield return LaunchEvent(key);
        if (failed) yield break;
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var steering = new MotoInputDriver(Reflex.Port.FrontEnd.CoreRemapping.Read(false), exhibition: true);
        string dir = profile ? Environment.GetEnvironmentVariable("REFLEX_GCSPIKE_PROFILE") : null;
        float profSeconds = EnvInt("REFLEX_GCSPIKE_PROFILE_SECONDS", 15);
        int capture = 0; float captureEnd = 0; float resultsAt = -1; float hold = EnvInt("REFLEX_GCSPIKE_RESULTS", 25);
        float t0 = Time.realtimeSinceStartup;
        SetPhase("ride");
        while (true)
        {
            if (failed || !rt) { Fail("the world went away"); yield break; }
            if (Time.realtimeSinceStartup - t0 > 1200) { Fail("the ride did not finish"); yield break; }
            try
            {
                steering.Attach(rt); rt.testHeldKeys = Array.Empty<int>();
                steering.Observe(rt, true, 0, 0, 0);
                rt.testHeldKeys = steering.Inputs(rt, true, Time.realtimeSinceStartupAsDouble, 0, 0, 0);
            }
            catch (Exception e) { Fail("steering: " + e.Message + " (laps " + rt.Race.num_laps + ", riders " + rt.Race.riders + ")"); yield break; }
            var race = rt.Race;
            if (race.valid != 0 && race.state >= 3) SetPhase("post");
            // profiler captures
            if (dir != null)
            {
                float now = Time.realtimeSinceStartup;
                if (capture == 0 && race.state == 2 && race.race_time > 40) { Begin(Path.Combine(dir, "ride.raw")); capture = 1; captureEnd = now + profSeconds; }
                else if (capture == 1 && now >= captureEnd) { End(); capture = 2; }
                else if (capture == 2 && resultsAt > 0 && now - resultsAt > 3) { Begin(Path.Combine(dir, "results.raw")); capture = 3; captureEnd = now + profSeconds; }
                else if (capture == 3 && now >= captureEnd) { End(); capture = 4; }
            }
            if (resultsAt < 0 && rt.raceResults && rt.raceResults.Active)
            {
                resultsAt = Time.realtimeSinceStartup;
                Debug.Log(Tag + Inv($"STEP {step}: results shown after {resultsAt - t0:0.0} s (race state {race.state}, laps {race.laps_completed}/{race.num_laps}, place {race.place})"));
            }
            if (resultsAt > 0 && Time.realtimeSinceStartup - resultsAt > hold && (capture == 0 || capture == 4 || dir == null)) break;
            yield return null;
        }
        rt.testHeldKeys = Array.Empty<int>();
        steering.Dispose();
        SetPhase("quit");
        // RETURN TO MENU on the results page (its own control), then its confirm; the pause menu's quit if the page has none
        if (!Press("results-menu")) { Debug.Log(Tag + "no results-menu control: quitting through the pause menu"); OriginalPauseMenu.QuitToFrontEnd(); yield break; }
        float tc = Time.realtimeSinceStartup;
        while (FindAnyObjectByType<OriginalWorldRuntime>() && Time.realtimeSinceStartup - tc < 20)
        {
            if (Press("results-confirm")) break;
            yield return null;
        }
        if (Time.realtimeSinceStartup - tc >= 20) { Debug.Log(Tag + "no results-confirm: quitting through the pause menu"); OriginalPauseMenu.QuitToFrontEnd(); yield break; }
        // (a batch player: the page's exit can wait for a drawn frame that never comes) the pause menu's quit after 15 s
        tc = Time.realtimeSinceStartup;
        while (FindAnyObjectByType<OriginalWorldRuntime>() && SceneManager.GetActiveScene().name != "FrontEnd" && Time.realtimeSinceStartup - tc < 15) yield return null;
        if (FindAnyObjectByType<OriginalWorldRuntime>() && SceneManager.GetActiveScene().name != "FrontEnd") { Debug.Log(Tag + "RETURN TO MENU did not leave in 15 s: quitting through the pause menu"); OriginalPauseMenu.QuitToFrontEnd(); }
    }

    static bool Press(string name)
    {
        foreach (var doc in FindObjectsByType<UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            var v = doc.rootVisualElement?.Q<VisualElement>(name);
            if (v?.userData is OuNavItem nav && nav.submit != null && v.enabledInHierarchy) { Debug.Log(Tag + "submit " + name); nav.submit(); return true; }
        }
        return false;
    }

    static void Begin(string file)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(file));
        Profiler.logFile = file; Profiler.enableAllocationCallstacks = true; Profiler.enableBinaryLog = true; Profiler.enabled = true;
        Debug.Log(Tag + "PROFILE START " + file);
    }
    static void End()
    {
        Profiler.enabled = false; Profiler.enableBinaryLog = false; Profiler.logFile = "";
        Debug.Log(Tag + "PROFILE END");
    }
}
