// Play-mode frame pacing / cost check (OriginalPresentation, OriginalPacingProbe): opens the generic Reflex scene on an event, rides
// it and exits the editor when the probe is done. The probe reads its options from the environment:
//   REFLEX_PACING_PROBE=<seconds measured> [REFLEX_PROBE_FPS=120] [REFLEX_PROBE_VSYNC=0] [REFLEX_PACING=legacy] [REFLEX_PACING_LOG=<csv>]
//   PACING_EVENT=National/NAT_Concord PACING_AI=2 (0 off, 1.. ReflexWorldFiles.AiSkills: a full gate)
//   Unity -batchmode -projectPath <bench> -executeMethod PacingTests.Run -logFile <log>     (no -quit: the method exits the editor)
// PlayerPrefs: the event goes through ReflexEventSelect.Pending; nothing of the user's is written.
using System;
using System.Globalization;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class PacingTests
{
    static double started;
    static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }

    public static void Run()
    {
        string ev = Env("PACING_EVENT", "National/NAT_Concord");
        ReflexEventSelect.Pending = ev; ReflexEventSelect.PendingAi = int.Parse(Env("PACING_AI", "2"), CultureInfo.InvariantCulture);
        EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
        EditorApplication.update += Tick;
        started = EditorApplication.timeSinceStartup;
        EditorApplication.EnterPlaymode();
        Debug.Log($"[pacing] {ev} ai {ReflexEventSelect.PendingAi} probe {Env("REFLEX_PACING_PROBE", "(unset)")} fps {Env("REFLEX_PROBE_FPS", "-")} mode {Env("REFLEX_PACING", "interpolated")}");
    }

    static void Tick()
    {
        double now = EditorApplication.timeSinceStartup;
        if (OriginalPacingProbe.Done || now - started > double.Parse(Env("PACING_TIMEOUT", "600"), CultureInfo.InvariantCulture))
        {
            EditorApplication.update -= Tick;
            Debug.Log("[pacing] exit: " + (OriginalPacingProbe.Done ? OriginalPacingProbe.Summary : "timeout"));
            EditorApplication.Exit(OriginalPacingProbe.Done ? 0 : 1);
        }
    }

    /// Builds a release (non-development) macOS player of the Reflex scene: PACING_PLAYER=<path of the .app>.
    public static void BuildPlayer()
    {
        string path = Env("PACING_PLAYER", "Builds/ReflexPerf.app");
        var o = new BuildPlayerOptions { scenes = new[] { "Assets/Scenes/Reflex.unity" }, locationPathName = path, target = BuildTarget.StandaloneOSX, options = BuildOptions.None };
        var r = UnityEditor.BuildPipeline.BuildPlayer(o);
        Debug.Log($"[pacing] player build {r.summary.result}: {path} {r.summary.totalSize / 1e6:0} MB in {r.summary.totalTime.TotalSeconds:0} s, {r.summary.totalErrors} errors");
        EditorApplication.Exit(r.summary.result == UnityEditor.Build.Reporting.BuildResult.Succeeded ? 0 : 1);
    }
}
