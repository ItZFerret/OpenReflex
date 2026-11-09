// End-to-end acceptance of the in-client MX vs ATV Alive importer through the real UI (OpenShell.AliveImport.cs), ahead of
// AliveRideProbe's ride. Opt-in: REFLEX_ALIVE_IMPORT=1 (with REFLEX_ALIVE_TEST=1), REFLEX_ALIVE_IMPORT_SOURCES=<path>|<path>...,
// REFLEX_ALIVE_IMPORT_LEVELS=<level>,<level>, REFLEX_PREVIEW_SHOTS=<dir>. Drives hub GAME LIBRARY -> IMPORT ALIVE CONTENT -> the
// sources (each typed into the path field: a file is added, a folder is opened and added with ADD THIS FOLDER) -> FIND LEVELS -> NONE,
// then the chosen levels -> IMPORT, waits for the results (every chosen level must succeed), DONE -> the library -> DONE -> the hub. The
// hub then shows MX vs ATV Alive and AliveRideProbe (waiting for that entry) rides REFLEX_ALIVE_LEVEL.
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;
[DefaultExecutionOrder(32001)]
public sealed class AliveImportProbe : MonoBehaviour
{
    string shots;
    static bool On => Environment.GetEnvironmentVariable("REFLEX_ALIVE_TEST") == "1" && Environment.GetEnvironmentVariable("REFLEX_ALIVE_IMPORT") == "1";
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        if (!On || FindAnyObjectByType<AliveImportProbe>()) return;
        var go = new GameObject("Alive import acceptance"); DontDestroyOnLoad(go); go.AddComponent<AliveImportProbe>();
    }
    static void Say(string s) => Debug.Log("[alive-test] IMPORT " + s);
    void Fail(string reason) { Debug.LogError("[alive-test] FAIL import: " + reason); Application.Quit(1); }
    static VisualElement Find(string name) => FindObjectsByType<UIDocument>(FindObjectsSortMode.None).Select(d => d.rootVisualElement?.Q<VisualElement>(name)).FirstOrDefault(v => v != null && v.panel != null);
    bool failed;

    IEnumerator Wait(string name, float seconds)
    {
        float t = Time.realtimeSinceStartup; VisualElement v;
        while ((v = Find(name)) == null || !v.enabledInHierarchy || v.worldBound.width < 1)
        {
            if (Time.realtimeSinceStartup - t > seconds) { failed = true; Fail("Missing control " + name); yield break; }
            yield return null;
        }
    }

    IEnumerator Click(string name)
    {
        yield return Wait(name, 120); if (failed) yield break;
        yield return new WaitForSecondsRealtime(1);
        var v = Find(name);
        if (v == null || !v.enabledInHierarchy) { failed = true; Fail("Control went away " + name); yield break; }
        v.GetFirstAncestorOfType<ScrollView>()?.ScrollTo(v);
        yield return null;
        var trigger = new Event { type = EventType.MouseUp, button = 0, clickCount = 1, mousePosition = v.worldBound.center };
        using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = v; v.SendEvent(click); }
        Say("CLICK " + name); yield return new WaitForSecondsRealtime(1);
    }

    IEnumerator Type(string field, string text)
    {
        yield return Wait(field, 60); if (failed) yield break;
        var f = Find(field) as TextField;
        if (f == null) { failed = true; Fail("Not a text field " + field); yield break; }
        f.value = text; Say("TYPE " + field + " = " + text);
        yield return new WaitForSecondsRealtime(1);
    }

    IEnumerator Shot(string name)
    {
        // the UI is an overlay: an offscreen camera capture would not show it; the window capture works when presented (best effort)
        yield return new WaitForEndOfFrame();
        try { ScreenCapture.CaptureScreenshot(Path.Combine(shots, "import-" + name + ".png")); } catch (Exception) { }
        yield return new WaitForSecondsRealtime(1);
    }

    IEnumerator Start()
    {
        shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); Directory.CreateDirectory(shots);
        var sources = (Environment.GetEnvironmentVariable("REFLEX_ALIVE_IMPORT_SOURCES") ?? "").Split('|').Where(s => s.Length > 0).ToArray();
        var levels = (Environment.GetEnvironmentVariable("REFLEX_ALIVE_IMPORT_LEVELS") ?? "").Split(',').Where(s => s.Length > 0).ToArray();
        if (sources.Length == 0 || levels.Length == 0) { Fail("No sources / levels given"); yield break; }
        yield return Wait("ou-hub-library", 900); if (failed) yield break;
        yield return new WaitForSecondsRealtime(3);
        var shell = OpenShell.Instance;
        yield return Click("ou-hub-library"); if (failed) yield break;
        yield return Click("ou-library-alive-import"); if (failed) yield break;
        yield return Shot("sources");
        foreach (var s in sources)
        {
            yield return Type("ou-alive-path", s); if (failed) yield break;
            if (Directory.Exists(s)) { yield return Click("ou-alive-add"); if (failed) yield break; }
        }
        yield return Click("ou-alive-find"); if (failed) yield break;
        float t = Time.realtimeSinceStartup;
        while (Find("ou-alive-levels") == null)
        {
            if (Find("ou-alive-sources") != null && Time.realtimeSinceStartup - t > 5) { Fail("FIND LEVELS returned to the sources"); yield break; }
            if (Time.realtimeSinceStartup - t > 300) { Fail("FIND LEVELS timed out"); yield break; }
            yield return null;
        }
        Say("FOUND in " + (Time.realtimeSinceStartup - t).ToString("F1") + " s");
        yield return Shot("levels");
        yield return Click("ou-alive-none"); if (failed) yield break;
        foreach (var l in levels) { yield return Click("ou-alive-level-" + l); if (failed) yield break; }
        yield return Click("ou-alive-import"); if (failed) yield break;
        t = Time.realtimeSinceStartup;
        while (Find("ou-alive-results") == null)
        {
            if (Time.realtimeSinceStartup - t > 900) { Fail("IMPORT timed out"); yield break; }
            yield return null;
        }
        var results = shell.AliveResults;
        yield return Shot("results");
        if (results == null || results.Count != levels.Length) { Fail("Results for " + (results?.Count ?? 0) + " of " + levels.Length + " levels"); yield break; }
        foreach (var r in results) Say("RESULT " + r.level + " ok=" + r.ok + " " + r + " converted=" + r.converted + " files=" + r.files + " seconds=" + r.seconds.ToString("F1"));
        if (results.Any(r => !r.ok) || !levels.All(l => results.Any(r => r.level == l))) { Fail("Not every level imported: " + string.Join("; ", results.Select(r => r.ToString()))); yield break; }
        yield return Click("ou-alive-done"); if (failed) yield break;
        yield return Click("ou-library-done"); if (failed) yield break;
        Say("DONE " + string.Join(", ", results.Select(r => r.level)) + " in " + (Time.realtimeSinceStartup - t).ToString("F1") + " s; back on the hub");
        Destroy(gameObject);
    }
}
