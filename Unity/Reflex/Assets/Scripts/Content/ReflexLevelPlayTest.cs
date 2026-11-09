#if UNITY_EDITOR
#if UNITY_EDITOR
using System;
using System.Collections;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Content
{
    // Editor-only play-mode check: with REFLEX_PLAYTEST=<out dir> set, a scene that has an OriginalLevel is played for a number of
    // frames, then the main camera is rendered to a PNG and the loader report is written (see Assets/Editor/Tests/WorldContentTests.cs,
    // PlayTest). Used to verify the run-time wiring: OriginalLevel.Awake before OriginalWorldRuntime.Start, terrain material, tracks, gate.
    public sealed class ReflexLevelPlayTest : MonoBehaviour
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            string dir = Environment.GetEnvironmentVariable("REFLEX_PLAYTEST");
            if (string.IsNullOrEmpty(dir) || !UnityEngine.Object.FindAnyObjectByType<OriginalLevel>()) return;
            new GameObject("ReflexLevelPlayTest").AddComponent<ReflexLevelPlayTest>().dir = dir;
        }

        public string dir;

        IEnumerator Start()
        {
            int frames = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_PLAYTEST_FRAMES"), out var f) ? f : 300;
            var log = new StringBuilder();
            var level = FindAnyObjectByType<OriginalLevel>();
            log.AppendLine("OriginalLevel: " + (level ? level.report : "missing"));
            for (int i = 0; i < frames; i++) yield return null;
            var runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            log.AppendLine("runtime running: " + (runtime && runtime.Running) + "; terrain built: " + (runtime && runtime.terrain && runtime.terrain.Built));
            if (runtime && runtime.animatedObjects) log.AppendLine("animated objects: " + runtime.animatedObjects.entries.Length);
            var tracks = FindAnyObjectByType<OriginalTracks>();
            log.AppendLine("tracks triangles: " + (tracks ? tracks.Triangles : -1));
            var cam = Camera.main;
            if (cam)
            {
                Directory.CreateDirectory(dir);
                for (int shot = 0; shot < 2; shot++)
                {
                    yield return new WaitForEndOfFrame();
                    var rt = new RenderTexture(1600, 900, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                    var old = cam.targetTexture; cam.targetTexture = rt; cam.Render(); cam.targetTexture = old;
                    RenderTexture.active = rt;
                    var tex = new Texture2D(1600, 900, TextureFormat.RGB24, false);
                    tex.ReadPixels(new Rect(0, 0, 1600, 900), 0, 0); tex.Apply();
                    RenderTexture.active = null;
                    File.WriteAllBytes(Path.Combine(dir, $"playtest-{level.level}-{shot}.png"), tex.EncodeToPNG());
                    log.AppendLine($"camera at {cam.transform.position} rendered {shot}");
                    for (int i = 0; i < 90; i++) yield return null;
                }
            }
            File.WriteAllText(Path.Combine(dir, "playtest-" + level.level + ".txt"), log.ToString());
            Debug.Log(log.ToString());
            EditorApplication.Exit(0);
        }
    }
}
#endif

#endif
