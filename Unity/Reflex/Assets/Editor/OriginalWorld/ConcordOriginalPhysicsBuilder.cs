using System;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.Editor;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.OriginalWorldEditor
{
    // Builds Assets/Scenes/ConcordOriginalPhysics.unity: the shared original-physics rig plus the OriginalLevel component for
    // NAT_Concord. Nothing from the game is baked into the scene or the project: the scenery, the terrain material (nine level
    // textures, parameters, lighting, fog), the tyre-track textures and the start gate objects are all loaded from the player's
    // install when the scene runs (OriginalLevel, Assets/Scripts/Content/Reflex*.cs). Collision/physics come from the captured world image.
    //
    // Terrain mapping (verified, see _re/notes/concord-terrain-world.md):
    //   2048x2048 R32F NAT_CO_DISP, scene GridResolution.EnumValue = 0 -> spacing 1
    //   x = c - 1023.5, z = 1023.5 - r, y = raw*2048 + deformation offset
    public static class ConcordOriginalPhysicsBuilder
    {
        public const string ScenePath = "Assets/Scenes/ConcordOriginalPhysics.unity";
        public const string WorldFile = "concord-world.bin", DeformFile = "concord-deform.bin";
        public const string Level = "NAT_Concord";

        [MenuItem("Reflex/Build Concord original physics scene")]
        public static void Build()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Exit Play mode first.");
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var level = ReflexSceneryBuilder.EnsureLevel(Level);

            // Free-ride start anchor 1 (all twelve share one pose in NAT_Concord); only frames the scene before the plugin runs.
            Vector3 startPos = new Vector3(585.779f, 114.541f, -322.002f); Quaternion startRot = Quaternion.identity;
            try
            {
                var a = ReflexLevelContent.ReadSpawnAnchors(Level).First();
                startPos = a.position; startRot = a.rotation;
            }
            catch (Exception e) { Debug.LogWarning("Start pose fallback: " + e.Message); }

            var runtime = AlpineOriginalPhysicsBuilder.BuildRig(startPos, startRot, WorldFile, DeformFile, "REFLEX / CONCORD · ORIGINAL GAME PHYSICS");
            runtime.terrain = AlpineOriginalPhysicsBuilder.CreateOriginalTerrain("Concord");
            // OriginalLevel wires the terrain material/tracks and the start gate (DropGate, vtable 0x95fc24, id +0x14, matrix +0x30,
            // _re/notes/start-gate.md) into this runtime when the scene starts.
            level.runtime = runtime; level.terrain = runtime.terrain;

            EditorSceneManager.SaveScene(scene, ScenePath);
            if (!EditorBuildSettings.scenes.Any(s => s.path == ScenePath))
                EditorBuildSettings.scenes = EditorBuildSettings.scenes.Concat(new[] { new EditorBuildSettingsScene(ScenePath, true) }).ToArray();
            AssetDatabase.SaveAssets();
            Debug.Log($"Concord original physics scene built: level content is loaded from the install at run time. " +
                      $"Needs StreamingAssets/OriginalWorld/{WorldFile} and {DeformFile}.");
        }
    }
}
