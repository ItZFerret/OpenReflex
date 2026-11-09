#if UNITY_EDITOR
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Reflex.Port.Rendering
{
    // Diagnostics only, off unless REFLEX_SHOT_AT is set (players and play mode): saves screenshots at the given seconds after start
    // into REFLEX_SHOT_OUT (default: the temp folder) and logs the scene and the front end's page at each one.
    //   REFLEX_SHOT_AT=10,30,60 REFLEX_SHOT_OUT=<dir> <player>
    public sealed class ReflexShotProbe : MonoBehaviour
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Init()
        {
            string at = Environment.GetEnvironmentVariable("REFLEX_SHOT_AT");
            if (string.IsNullOrEmpty(at)) return;
            Application.runInBackground = true;   // launched from a shell the window may never get focus (the front end would not tick)
            var go = new GameObject("Reflex shot probe") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            go.AddComponent<ReflexShotProbe>().times = Array.ConvertAll(at.Split(','), s => float.Parse(s, CultureInfo.InvariantCulture));
        }

        float[] times = Array.Empty<float>();

        IEnumerator Start()
        {
            string dir = Environment.GetEnvironmentVariable("REFLEX_SHOT_OUT");
            if (string.IsNullOrEmpty(dir)) dir = Path.GetTempPath();
            Directory.CreateDirectory(dir);
            float t0 = Time.realtimeSinceStartup;
            foreach (float t in times)
            {
                while (Time.realtimeSinceStartup - t0 < t) yield return null;
                yield return new WaitForEndOfFrame();
                string path = Path.Combine(dir, $"shot_{t:0.#}s.png");
                ScreenCapture.CaptureScreenshot(path);
                var fe = FindAnyObjectByType<Reflex.Port.FrontEnd.FrontEndManager>();
                Debug.Log($"[shot] {t:0.#} s: {path}  scene {SceneManager.GetActiveScene().name}" + (fe ? "  page " + fe.CurrentPage : ""));
            }
        }
    }
}

#endif
