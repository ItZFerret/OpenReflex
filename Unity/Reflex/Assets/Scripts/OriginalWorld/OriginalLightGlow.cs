using System;
using System.Collections.Generic;
using UnityEngine;
using Reflex.Port.Content;

namespace Reflex.Port.OriginalWorld
{
    // WP-R3 (_re/notes/fx.md): the light glows, light rays / god rays, fog planes, HDR-safe cards and the sun billboard of a level, on the
    // hand-ported programs of Shaders/ReflexLightGlow.shader (Content/ReflexLightGlow.cs). One call registers it after the scenery is
    // built: OriginalLightGlow.Attach(level) (OriginalLevel.LoadNow). It converts the glow-family scenery materials in place and keeps
    // doing so for materials made later (ambient ScenarioAnimEntity models such as the stadium spot-light trailers, LOD materials), and
    // builds the camera-facing sun billboards. Nothing is read from the running game: these models have no per-frame state besides
    // what the scenery components already apply (OriginalProps' gTextureOffset for the scrolling light rays, OriginalSceneryLod's fade).
    [DisallowMultipleComponent]
    public sealed class OriginalLightGlow : MonoBehaviour
    {
        public OriginalLevel level;

        [Header("Result (read only)")]
        public int convertedMaterials; public int billboards;
        [TextArea(2, 6)] public string report;
        public double Milliseconds => milliseconds;

        readonly HashSet<string> done = new HashSet<string>(StringComparer.Ordinal);
        Shader shader; GameObject root; int seenMaterials = -1; double milliseconds;

        /// Registers the glow content of a loaded level (no-op when the level has none). Main thread; never stops the level load.
        public static OriginalLightGlow Attach(OriginalLevel lvl)
        {
            try
            {
                if (!lvl || lvl.Content == null) return null;
                if (Environment.GetEnvironmentVariable("REFLEX_R3_OFF") == "1") return null;   // A/B renders (FxTests): the level without it
                var g = lvl.GetComponent<OriginalLightGlow>() ?? lvl.gameObject.AddComponent<OriginalLightGlow>();
                g.level = lvl;
                g.Begin();
                return g;
            }
            catch (Exception e) { Debug.LogWarning("OriginalLightGlow: " + e.Message + "\n" + e.StackTrace, lvl); return null; }
        }

        void Begin()
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            done.Clear(); seenMaterials = -1;
            Release();
            shader = Shader.Find(ReflexLightGlow.ShaderName);
            if (!shader) { report = "shader " + ReflexLightGlow.ShaderName + " not found (Always Included Shaders)"; Debug.LogWarning("OriginalLightGlow: " + report, this); return; }
            var content = level.Content;
            root = new GameObject("Original camera-facing models (sun)") { hideFlags = HideFlags.DontSave };
            root.transform.SetParent(level.SceneryRoot ? level.SceneryRoot.transform : transform, false);
            var made = content.BuildCameraFacing(root.transform, shader);
            billboards = made.Count;
            Convert();
            sw.Stop();
            report = $"{level.level}: {convertedMaterials} glow materials converted, {billboards} camera-facing models ({string.Join(", ", made.ConvertAll(b => b.model + " #" + b.entityId))}), {sw.Elapsed.TotalMilliseconds:F1} ms";
            Debug.Log("[OriginalLightGlow] " + report, this);
        }

        void Convert()
        {
            var content = level ? level.Content : null;
            if (content == null || !shader) return;
            convertedMaterials += content.ConvertGlowMaterials(shader, done);
            seenMaterials = content.SceneryMaterials.Count;
        }

        // materials created after the level load (ambient models built when the world starts, extra LOD materials): converted when the
        // level's material table grows (a count compare per frame otherwise)
        void LateUpdate()
        {
            var content = level ? level.Content : null;
            if (content == null) return;
            if (content.SceneryMaterials.Count == seenMaterials) { milliseconds = 0; return; }
            var sw = System.Diagnostics.Stopwatch.StartNew();
            Convert();
            milliseconds = sw.Elapsed.TotalMilliseconds;
        }

        void Release() { if (root) { if (Application.isPlaying) Destroy(root); else DestroyImmediate(root); } root = null; }
        void OnDestroy() { Release(); }
    }
}
