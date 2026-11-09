// TEST ONLY (tests/perf_profile): records a Unity profiler capture of a window of the run into a binary log, for
// tests/perf_profile/ProfileReport.cs. REFLEX_PROFILE_FILE=<path.raw>, REFLEX_PROFILE_START=<s after start>, REFLEX_PROFILE_SECONDS=<s>.
using System;
using System.Globalization;
using UnityEngine;
using UnityEngine.Profiling;

public sealed class PerfCapture : MonoBehaviour
{
    float start, seconds; bool on, done;
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string file = Environment.GetEnvironmentVariable("REFLEX_PROFILE_FILE");
        if (string.IsNullOrEmpty(file)) return;
        var go = new GameObject("Perf capture") { hideFlags = HideFlags.HideAndDontSave };
        DontDestroyOnLoad(go);
        var c = go.AddComponent<PerfCapture>();
        c.start = float.Parse(Environment.GetEnvironmentVariable("REFLEX_PROFILE_START") ?? "200", CultureInfo.InvariantCulture);
        c.seconds = float.Parse(Environment.GetEnvironmentVariable("REFLEX_PROFILE_SECONDS") ?? "40", CultureInfo.InvariantCulture);
        Profiler.logFile = file;
    }
    void Update()
    {
        float t = Time.realtimeSinceStartup;
        if (!on && !done && t >= start)
        {
            Profiler.enableBinaryLog = true; Profiler.enabled = true; on = true;
            Debug.Log("[perf-capture] START t=" + t.ToString("0.0") + " file=" + Profiler.logFile);
        }
        else if (on && t >= start + seconds)
        {
            Profiler.enabled = false; Profiler.enableBinaryLog = false; on = false; done = true;
            Debug.Log("[perf-capture] STOP t=" + t.ToString("0.0"));
        }
    }
}
