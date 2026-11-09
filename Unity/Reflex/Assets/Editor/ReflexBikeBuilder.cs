using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.Content;

namespace Reflex.Port.Editor
{
    // The bike is no longer a prefab: Content/ReflexBikeModel builds the MarsF Lite at run time from the player's own Reflex
    // install (nothing from the game is copied into Assets any more). This class keeps the BikeDiscovery inspection scene
    // (a ReflexVehiclePreview that builds the bike and the rider when the scene runs), a validation, and the helpers of the
    // legacy scene builders.
    public static class ReflexBikeBuilder
    {
        public const string ScenePath = "Assets/Scenes/BikeDiscovery.unity";
        // The old prefab (Assets/ReflexRecovered/Bike, built from the Python exports). It is not built any more; the legacy scene
        // builders (PlayableBikeTestBuilder, ReflexRideBuilder) still name it and report it missing.
        public const string PrefabPath = "Assets/ReflexRecovered/Bike/MarsFLitePreview.prefab";

        [MenuItem("Reflex/Build recovered bike preview")]
        public static void Build()
        {
            if (EditorApplication.isPlaying) throw new InvalidOperationException("Exit Play mode before rebuilding the scene.");
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var preview = new GameObject("Reflex bike and rider (built at run time from the install)").AddComponent<ReflexVehiclePreview>();
            preview.vehicleShader = Shader.Find("Reflex/Vehicle");
            var camera = new GameObject("Bike inspection camera").AddComponent<Camera>();
            camera.tag = "MainCamera"; camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.08f, 0.11f, 0.15f);
            camera.nearClipPlane = 0.05f; camera.farClipPlane = 20000; camera.fieldOfView = 38;
            camera.GetUniversalAdditionalCameraData().renderPostProcessing = false;
            camera.gameObject.AddComponent<AudioListener>();
            camera.gameObject.AddComponent<DiscoveryCamera>();
            camera.transform.position = new Vector3(14, 6, 9); camera.transform.LookAt(new Vector3(-2, 1.5f, 0));
            AddLight("Preview key", new Vector3(42, -35, 0), 2.4f, new Color(1, 0.94f, 0.86f));
            AddLight("Preview fill", new Vector3(25, 135, 0), 1.1f, new Color(0.65f, 0.8f, 1));
            RenderSettings.ambientMode = AmbientMode.Flat;
            RenderSettings.ambientLight = new Color(0.6f, 0.6f, 0.6f);
            RenderSettings.skybox = null; RenderSettings.fog = false;
            EditorSceneManager.SaveScene(scene, ScenePath);
            Debug.Log("Reflex bike preview scene saved: the bike and rider are built from the install when it plays.");
        }

        // Builds the run-time bike and checks it against the original exporter's counts (BIKE_FORMAT.md: 24 meshes, 13,382 vertices, 10,866 triangles).
        [MenuItem("Reflex/Validate run-time bike")]
        public static void Validate()
        {
            var bike = ReflexBikeModel.Build();
            try
            {
                var filters = bike.chassis.GetComponentsInChildren<MeshFilter>(true).Concat(bike.frontWheel.GetComponentsInChildren<MeshFilter>(true)).Concat(bike.rearWheel.GetComponentsInChildren<MeshFilter>(true)).ToArray();
                int vertices = filters.Sum(f => f.sharedMesh.vertexCount), triangles = filters.Sum(f => f.sharedMesh.triangles.Length / 3);
                Debug.Log($"Reflex run-time bike: {filters.Length} meshes, {vertices} vertices, {triangles} triangles, {bike.pose.parts.Length} pose-bound parts");
                if (filters.Length != 24 || vertices != 13382 || triangles != 10866) throw new InvalidDataException("The run-time bike differs from the original export.");
                if (bike.pose.parts.Length != 24) throw new InvalidDataException("Not every part is bound to its original node.");
            }
            finally { bike.Destroy(); ReflexBikeModel.ReleaseTextures(); }
        }

        // -executeMethod entry point for batch mode
        public static void ValidateBatch() => Validate();

        public static Bounds BoundsOf(GameObject model)
        {
            var renderers = model.GetComponentsInChildren<Renderer>();
            if (renderers.Length == 0) throw new InvalidDataException("Model has no renderers.");
            var bounds = renderers[0].bounds;
            foreach (var renderer in renderers) bounds.Encapsulate(renderer.bounds);
            return bounds;
        }

        static void AddLight(string name, Vector3 rotation, float intensity, Color color)
        {
            var light = new GameObject(name).AddComponent<Light>();
            light.type = LightType.Directional; light.intensity = intensity; light.color = color;
            light.transform.rotation = Quaternion.Euler(rotation);
        }

    }
}
