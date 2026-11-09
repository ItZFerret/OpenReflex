using System;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Reflex.Port.Editor;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.OriginalWorldEditor
{
    // Builds Assets/Scenes/Reflex.unity: the generic event scene. The original-physics rig, the level content component and
    // ReflexEventSelect, which picks the event (any single-player event of the install's tables; Insert lists them) and points the
    // level, the running world, game sound and music at it before they load. Nothing from the game is in the scene.
    public static class ReflexEventSceneBuilder
    {
        public const string ScenePath = "Assets/Scenes/Reflex.unity";

        [MenuItem("Reflex/Build Reflex event scene (any track)")]
        public static void Build()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Exit Play mode first.");
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var level = ReflexSceneryBuilder.EnsureLevel("NAT_Concord");
            var runtime = AlpineOriginalPhysicsBuilder.BuildRig(Vector3.zero, Quaternion.identity, "concord-world.bin", null, "REFLEX · ORIGINAL GAME");
            runtime.terrain = AlpineOriginalPhysicsBuilder.CreateOriginalTerrain("Level");
            level.runtime = runtime; level.terrain = runtime.terrain;
            var select = runtime.gameObject.AddComponent<ReflexEventSelect>();
            select.level = level; select.runtime = runtime; select.gameAudio = runtime.gameAudio;
            select.music = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>();
            EditorSceneManager.SaveScene(scene, ScenePath);
            var list = EditorBuildSettings.scenes.Where(s => s.path != ScenePath).ToList();
            list.Insert(0, new EditorBuildSettingsScene(ScenePath, true));   // the player starts here
            EditorBuildSettings.scenes = list.ToArray();
            // the original front end (when built) stays the first scene, the race second
            if (System.IO.File.Exists(Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.ScenePath)) Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.SetBuildOrder();
            Reflex.Port.Editor.ReflexShaderIncludes.Ensure();   // the run-time shaders (Shader.Find) in player builds
            AssetDatabase.SaveAssets();
            Debug.Log("Reflex event scene built: Insert lists every event of the install; each event's world is built from the install on first use.");
        }
    }
}
