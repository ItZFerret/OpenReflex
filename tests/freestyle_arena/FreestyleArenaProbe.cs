// Test only (tests/freestyle_arena, w-fsfix): rendered checks of a Freestyle arena (the event or its Free Ride) in a real player.
// REFLEX_FSARENA_TEST=1, REFLEX_FSARENA_EVENT=<event key> ("Freestyle/FMX_BuenaVista", "FreeRide/FMX_BuenaVista"), REFLEX_FSARENA_AI (the setup's
// AI index 0 off .. 4, default 0), REFLEX_FSARENA_DURATION (the setup's Event Duration index, default -1 = the page's default), REFLEX_PREVIEW_SHOTS (dir).
// From the hub it launches the event as the setup page does (ReflexEventSelect.Launch), then: "spawn" (the chase camera on the roll-in),
// "ride" after 4 s of throttle, an "overview" from above the arena and up to ten close views of the arena's ramps and roll-ins (the level's
// scenery objects named *Ramp*, *Roll_in*, *Rollin*), each listing the object's renderer, materials and textures in the log ([fsarena-test] lines).
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

[DefaultExecutionOrder(32000)]
public sealed class FreestyleArenaProbe : MonoBehaviour
{
    string shots; int errors; Camera cam; bool hold; Vector3 camPos, camTarget;
    static bool Offscreen => Application.isBatchMode || Environment.GetEnvironmentVariable("REFLEX_FSARENA_OFFSCREEN") == "1";
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Guest() { if (Environment.GetEnvironmentVariable("REFLEX_FSARENA_TEST") == "1") ReflexProfile.Disabled = true; }
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        if (Environment.GetEnvironmentVariable("REFLEX_FSARENA_TEST") != "1" || FindAnyObjectByType<FreestyleArenaProbe>()) return;
        var go = new GameObject("Freestyle arena probe"); DontDestroyOnLoad(go); go.AddComponent<FreestyleArenaProbe>(); Application.runInBackground = true;
    }
    void OnEnable() { Application.logMessageReceived += Log; }
    void OnDisable() { Application.logMessageReceived -= Log; }
    void Log(string s, string trace, LogType type) { if (type == LogType.Exception) errors++; }
    void LateUpdate() { if (hold && cam) { cam.transform.position = camPos; cam.transform.LookAt(camTarget); } }
    static void Say(string s) => Debug.Log("[fsarena-test] " + s);
    void Fail(string s) { Say("FAIL " + s); Application.Quit(1); }
    VisualElement Find(string name) => FindObjectsByType<UIDocument>(FindObjectsSortMode.None).Select(d => d.rootVisualElement?.Q<VisualElement>(name)).FirstOrDefault(v => v != null && v.panel != null);

    IEnumerator Shot(string name)
    {
        yield return new WaitForEndOfFrame();
        string path = Path.Combine(shots, name + ".png");
        if (!Offscreen) { ScreenCapture.CaptureScreenshot(path); yield return new WaitForSecondsRealtime(1); yield break; }
        var rt = RenderTexture.GetTemporary(1280, 720, 24, RenderTextureFormat.ARGB32);
        foreach (var c in FindObjectsByType<Camera>(FindObjectsSortMode.None).Where(c => c.isActiveAndEnabled).OrderBy(c => c.depth))
        { var prior = c.targetTexture; c.targetTexture = rt; c.Render(); c.targetTexture = prior; }
        var prev = RenderTexture.active; RenderTexture.active = rt;
        var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply();
        RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
        File.WriteAllBytes(path, tex.EncodeToPNG()); Destroy(tex);
        yield return new WaitForSecondsRealtime(1);
    }

    static int Env(string n, int d) => int.TryParse(Environment.GetEnvironmentVariable(n), out int v) ? v : d;

    IEnumerator Start()
    {
        shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); Directory.CreateDirectory(shots);
        string key = Environment.GetEnvironmentVariable("REFLEX_FSARENA_EVENT") ?? "Freestyle/FMX_BuenaVista";
        float t = Time.realtimeSinceStartup;
        while (Find("ou-hub-play") == null) { if (Time.realtimeSinceStartup - t > 180) { Fail("no hub"); yield break; } yield return null; }
        yield return new WaitForSecondsRealtime(2);
        var ev = ReflexEvents.Find(key);
        if (ev == null) { Fail("no event " + key + " (Freestyle events: " + string.Join(", ", ReflexEvents.All.Where(e => e.spec.type == "Freestyle").Select(e => e.key)) + ")"); yield break; }
        Say("EVENT " + ev.key + " type " + ev.spec.type + " duration " + ev.spec.duration + " " + ev.spec.durationType + " section " + ev.sceneSection);
        // the setup page's own launch (OpenSetup.Launch: its Event Duration index and AI choice), as the player starts the event
        var setup = new Reflex.Port.OpenFrontEnd.OpenSetup { eventKey = ev.key, ai = Env("REFLEX_FSARENA_AI", 0), duration = Env("REFLEX_FSARENA_DURATION", -1) };
        Say("SETUP duration index " + setup.duration + " -> " + setup.EventDuration(ev) + " " + ev.spec.durationType + ", AI " + setup.ai);
        setup.Launch();
        OriginalWorldRuntime rt = null; t = Time.realtimeSinceStartup;
        while (!(rt && rt.Running))
        {
            rt = FindAnyObjectByType<OriginalWorldRuntime>();
            if (rt && !string.IsNullOrEmpty(rt.FailureMessage)) { Fail(rt.FailureMessage); yield break; }
            if (Time.realtimeSinceStartup - t > 300) { Fail("load timed out"); yield break; }
            yield return null;
        }
        while (rt.Bike.frames < 150) { if (Time.realtimeSinceStartup - t > 400) { Fail("no frames"); yield break; } yield return null; }
        yield return Shot("spawn");
        Say($"SPAWN frames={rt.Bike.frames} pos={string.Join(",", rt.Bike.position.Take(3))}");
        uint start = rt.Bike.frames; rt.testHeldKeys = new[] { 0x11 };
        while (rt.Bike.frames - start < 240) { if (!rt.Running) { Fail(rt.FailureMessage); yield break; } yield return null; }
        yield return Shot("ride");
        rt.testHeldKeys = Array.Empty<int>();
        // the arena's scenery
        var level = FindObjectsByType<OriginalLevel>(FindObjectsSortMode.None).FirstOrDefault(x => x.level == ev.location);
        var renderers = FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None);
        bool Arena(string n) => n.IndexOf("Ramp", StringComparison.OrdinalIgnoreCase) >= 0 || n.IndexOf("Roll_in", StringComparison.OrdinalIgnoreCase) >= 0 || n.IndexOf("Rollin", StringComparison.OrdinalIgnoreCase) >= 0;
        var ramps = renderers.Where(r => Arena(r.name) && r.name.IndexOf("Anchor", StringComparison.OrdinalIgnoreCase) < 0)
                             .OrderBy(r => r.name, StringComparer.Ordinal).ToList();
        Say($"SCENERY renderers={renderers.Length} ramps={ramps.Count} level={(level ? level.level + "/" + level.activity : "none")}");
        foreach (var r in ramps)
        {
            var mats = r.sharedMaterials;
            Say($"RAMP {r.name} enabled={r.enabled && r.gameObject.activeInHierarchy} bounds={r.bounds.center} size={r.bounds.size} materials=" +
                string.Join(" | ", mats.Select(m => m == null ? "NULL" : m.name + "(" + m.shader.name + " tex=" + string.Join("/", m.GetTexturePropertyNames().Select(n => m.GetTexture(n)).Where(x => x).Select(x => x.name + ":" + x.width + "x" + x.height)) + ")")));
        }
        // w-fsshaders: every scenery material drawn by the scenery shader's generic path (its program not in ReflexSceneryMaterial's
        // template table): "FALLBACK <program> materials=... models=..." (renderer "<name> [<model>] #id")
        var content = level ? level.Content : null;
        var tplField = typeof(ReflexSceneryMaterial).GetField("Templates", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
        var tpls = tplField?.GetValue(null) as System.Collections.IDictionary;
        var special = new[] { "FR_AL_Rock2", "FR_AL_EoW_Base_PreLit", "FR_AL_EoW_Base_PreLit_Alpha", "FR_AL_EoW_RivBirchLrg", "Template_ATV_MX_Engine_FNL" };
        var fallback = new System.Collections.Generic.SortedDictionary<string, (System.Collections.Generic.SortedSet<string> mats, System.Collections.Generic.SortedSet<string> models)>(StringComparer.Ordinal);
        var shadersUsed = new System.Collections.Generic.SortedDictionary<string, int>(StringComparer.Ordinal);
        foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
            foreach (var m in r.sharedMaterials)
            {
                if (!m) continue;
                shadersUsed[m.shader.name] = shadersUsed.TryGetValue(m.shader.name, out int k) ? k + 1 : 1;
                if (content == null || m.shader.name != "Reflex/Original Scenery" || !content.sceneMaterials.TryGetValue(m.name, out var lm)) continue;
                string ps = lm.pixelShader ?? "(none)";
                if ((tpls != null && tpls.Contains(ps)) || special.Contains(ps) || m.IsKeywordEnabled("_ALIVE_VM") || m.IsKeywordEnabled("_ALIVE_VM_GENERAL")) continue;
                if (!fallback.TryGetValue(ps, out var e)) fallback[ps] = e = (new System.Collections.Generic.SortedSet<string>(StringComparer.Ordinal), new System.Collections.Generic.SortedSet<string>(StringComparer.Ordinal));
                e.mats.Add(m.name);
                int a = r.name.IndexOf('['), b = r.name.IndexOf(']');
                e.models.Add(a >= 0 && b > a ? r.name.Substring(a + 1, b - a - 1) : r.name);
            }
        Say("SHADERS " + string.Join(", ", shadersUsed.Select(kv => kv.Key + "=" + kv.Value)));
        foreach (var kv in fallback) Say($"FALLBACK {kv.Key} materials={string.Join(",", kv.Value.mats)} models={string.Join(",", kv.Value.models)}");
        cam = rt.chaseCamera; hold = true;
        var all = new Bounds(new Vector3(rt.Bike.position[0], rt.Bike.position[1], rt.Bike.position[2]), Vector3.one);
        foreach (var r in ramps) all.Encapsulate(r.bounds);
        camTarget = all.center; camPos = all.center + new Vector3(0.01f, Mathf.Max(80f, all.extents.magnitude * 1.2f), -all.extents.z * 0.6f);
        yield return null; yield return null; yield return Shot("overview");
        // w-fsshaders: REFLEX_FSARENA_PERF=n: frame times over n frames at the overview camera (run.py --perf; uncapped to 240 fps)
        int perfFrames = Env("REFLEX_FSARENA_PERF", 0);
        if (perfFrames > 0)
        {
            for (int k = 0; k < 30; k++) yield return null;
            var dts = new System.Collections.Generic.List<float>(perfFrames);
            for (int k = 0; k < perfFrames; k++) { yield return null; dts.Add(Time.unscaledDeltaTime * 1000f); }
            dts.Sort();
            Say($"PERF overview frames={perfFrames} mean={dts.Average():F3}ms median={dts[dts.Count / 2]:F3}ms p95={dts[(int)(dts.Count * 0.95f)]:F3}ms");
        }
        int n = 0;
        foreach (var r in ramps.GroupBy(r => r.name.Split(' ')[0]).Select(g => g.First()).Take(10))
        {
            var b = r.bounds; float d = Mathf.Max(12f, b.extents.magnitude * 2.2f);
            camTarget = b.center; camPos = b.center + new Vector3(d * 0.7f, d * 0.45f, d * 0.7f);
            yield return null; yield return null; yield return Shot("ramp-" + (n++) + "-" + r.name.Split(' ')[0]);
        }
        // w-fsshaders: REFLEX_FSARENA_FOCUS="a,b,..." close views of the first renderer whose name contains each entry ("focus-<k>-<entry>")
        string focus = Environment.GetEnvironmentVariable("REFLEX_FSARENA_FOCUS");
        if (!string.IsNullOrEmpty(focus))
        {
            var all2 = FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None).Where(r => r.enabled && r.gameObject.activeInHierarchy).OrderBy(r => r.name, StringComparer.Ordinal).ToList();
            int k = 0;
            foreach (string f in focus.Split(',').Select(s => s.Trim()).Where(s => s.Length > 0))
            {
                var r = all2.FirstOrDefault(x => x.name.IndexOf(f, StringComparison.Ordinal) >= 0);
                if (!r) { Say("FOCUS " + f + " not found"); continue; }
                var b = r.bounds; float d = Mathf.Clamp(b.extents.magnitude * 1.8f, 6f, 400f);
                camTarget = b.center; camPos = b.center + new Vector3(d * 0.7f, d * 0.35f, d * 0.7f);
                Say($"FOCUS {f} -> {r.name} bounds={b.center} size={b.size} materials={string.Join(" | ", r.sharedMaterials.Select(m => m ? m.name : "NULL"))}");
                yield return null; yield return null; yield return Shot("focus-" + (k++) + "-" + f);
            }
        }
        hold = false; yield return null;
        Say("PASS exceptions=" + errors);
        Application.Quit(0);
    }
}
