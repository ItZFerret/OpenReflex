using System;
using System.Linq;
using Reflex.Port.FrontEnd;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.FrontEndEditor
{
    // Builds Assets/Scenes/FrontEnd.unity: the original front end (Reflex.Port.FrontEnd.FrontEndManager: title, main menu, arcade
    // event / location select, loading page) read at run time from the install, handing the chosen event to Reflex.unity.
    // Build settings: FrontEnd.unity at index 0 (the player starts there), Reflex.unity at index 1, every other scene kept after them.
    public static class ReflexFrontEndSceneBuilder
    {
        public const string ScenePath = "Assets/Scenes/FrontEnd.unity", RaceScenePath = "Assets/Scenes/Reflex.unity";

        [MenuItem("Reflex/Build front end scene (FrontEnd.unity)")]
        public static void Build()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Exit Play mode first.");
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var cam = new GameObject("Main Camera", typeof(Camera), typeof(AudioListener)).GetComponent<Camera>();
            cam.tag = "MainCamera"; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black; cam.cullingMask = 0; cam.orthographic = true;
            var fe = new GameObject("Reflex front end").AddComponent<FrontEndManager>();
            fe.uiShader = Shader.Find("Reflex/FrontEnd UI");
            if (!fe.uiShader) Debug.LogWarning("Shader Reflex/FrontEnd UI not found (Assets/Shaders/ReflexFrontEndUi.shader)");
            EditorSceneManager.SaveScene(scene, ScenePath);
            SetBuildOrder();
            AssetDatabase.SaveAssets();
            Debug.Log("Front end scene built: " + ScenePath + " (build index 0), race scene " + RaceScenePath + " (index 1).");
        }

        /// FrontEnd.unity first, Reflex.unity second, the rest of the list as it was.
        public static void SetBuildOrder()
        {
            var rest = EditorBuildSettings.scenes.Where(s => s.path != ScenePath && s.path != RaceScenePath).ToList();
            rest.Insert(0, new EditorBuildSettingsScene(ScenePath, true));
            rest.Insert(1, new EditorBuildSettingsScene(RaceScenePath, true));
            EditorBuildSettings.scenes = rest.ToArray();
        }
    }
}
