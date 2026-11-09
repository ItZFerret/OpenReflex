using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.Editor;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.OriginalWorldEditor
{
    public static class AlpineOriginalPhysicsBuilder
    {
        public const string ScenePath = "Assets/Scenes/AlpineOriginalPhysics.unity";

        [MenuItem("Reflex/Build Alpine original physics scene")]
        public static void Build()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Exit Play mode first.");
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            // Level content (scenery, hovercraft, terrain material, track textures) is read from the player's install at run time.
            var level = ReflexSceneryBuilder.EnsureLevel("FR_Alpine");

            // Starting pose (only frames the scene before the plugin runs): the level's first free-ride spawn anchor at run time.
            Vector3 startPos = Vector3.zero; Quaternion startRot = Quaternion.identity;

            var runtime = BuildRig(startPos, startRot, null, null, "REFLEX / ALPINE · ORIGINAL GAME PHYSICS");
            runtime.terrain = CreateOriginalTerrain("Alpine");
            level.runtime = runtime; level.terrain = runtime.terrain;

            EditorSceneManager.SaveScene(scene, ScenePath);
            if (!EditorBuildSettings.scenes.Any(s => s.path == ScenePath))
                EditorBuildSettings.scenes = EditorBuildSettings.scenes.Concat(new[] { new EditorBuildSettingsScene(ScenePath, true) }).ToArray();
            AssetDatabase.SaveAssets();
            Debug.Log("Alpine original physics scene built. Everything (world, terrain, scenery, vehicles, sound, UI) is read from the player's Reflex install at run time.");
        }

        // The level terrain, built at play time by OriginalTerrain from the running original world's collision
        // heights (Unity Terrain with GPU LOD; follows the deformation surface). The scene only holds the component:
        // OriginalLevel (added by ReflexSceneryBuilder.EnsureLevel) reads the level material from the install at run time
        // (ReflexTerrainMaterial.cs: "Reflex/Original HFTerrain" with the nine level textures, usr_* parameters and scene
        // sun/fog values; the tyre-track textures and softness mask for OriginalTracks) and hands it to this component
        // before OriginalWorldRuntime builds the terrain.
        internal static OriginalTerrain CreateOriginalTerrain(string level)
        {
            var go = new GameObject(level + " terrain (original collision heights, built at play time)");
            return go.AddComponent<OriginalTerrain>();
        }

        // Shared by the Alpine and Concord scene builders: bike, rider, chase camera, light, HUD, runtime.
        // worldFile / deformFile (StreamingAssets/OriginalWorld) are only overridden when non-null.
        internal static OriginalWorldRuntime BuildRig(Vector3 startPos, Quaternion startRot, string worldFile, string deformFile, string hudTitle)
        {
            var runtime = new GameObject("Original physics runtime").AddComponent<OriginalWorldRuntime>();
            if (worldFile != null) runtime.worldFile = worldFile;
            if (deformFile != null) runtime.deformFile = deformFile;
            // The bike (MarsF Lite chassis, wheels, OriginalBikePose) and the rider (MXRider_Hierarchy, OriginalRider) are built at
            // run time from the player's Reflex install by OriginalWorldRuntime.EnsureVehicle: the scene holds none of them.
            runtime.vehicleShader = Shader.Find("Reflex/Vehicle");
            // The race's other riders (AI): built at run time from the running game's tables and posed by it (OriginalOpponents).
            runtime.opponentRiders = runtime.gameObject.AddComponent<OriginalOpponents>();
            try { CreateMusic(worldFile != null && worldFile.StartsWith("concord", StringComparison.Ordinal) ? "NAT_Concord" : "FR_Alpine"); }
            catch (Exception e) { Debug.LogWarning("Original music not set up: " + e.Message); }
            try { runtime.gameAudio = CreateGameAudio(worldFile != null && worldFile.StartsWith("concord", StringComparison.Ordinal) ? "NAT_Concord" : "FR_Alpine"); }
            catch (Exception e) { Debug.LogWarning("Original game sound not set up: " + e.Message); }

            var camera = new GameObject("Chase camera").AddComponent<Camera>(); camera.tag = "MainCamera"; runtime.chaseCamera = camera;
            runtime.onboard = camera.gameObject.AddComponent<OriginalOnboardCamera>();   // Dynamic FPV (F7)
            try { runtime.originalHud = CreateOriginalHud(runtime.gameObject); }
            catch (Exception e) { Debug.LogWarning("Original HUD not set up: " + e.Message); }
            runtime.optionsMenu = runtime.gameObject.AddComponent<OriginalOptionsMenu>();   // F4
            runtime.optionsMenu.runtime = runtime;
            camera.nearClipPlane = 0.15f; camera.farClipPlane = 14000; camera.fieldOfView = 47;
            camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = new Color(0.47f, 0.61f, 0.73f);
            camera.GetUniversalAdditionalCameraData().renderPostProcessing = false;
            camera.gameObject.AddComponent<AudioListener>();
            Vector3 fwd = startRot * Vector3.forward; fwd.y = 0; fwd = fwd.sqrMagnitude < 1e-4f ? Vector3.forward : fwd.normalized;
            camera.transform.position = startPos + Vector3.up * 8 - fwd * 16 + Vector3.Cross(Vector3.up, fwd) * 3;
            camera.transform.LookAt(startPos + Vector3.up * 1.1f);

            var sun = new GameObject("Sun").AddComponent<Light>(); sun.type = LightType.Directional; sun.intensity = 2.3f; sun.color = new Color(1, 0.95f, 0.86f);
            sun.shadows = LightShadows.Soft; sun.transform.rotation = Quaternion.Euler(48, -25, 0); RenderSettings.sun = sun;
            RenderSettings.ambientMode = AmbientMode.Trilight;
            RenderSettings.ambientSkyColor = new Color(0.55f, 0.65f, 0.8f); RenderSettings.ambientEquatorColor = new Color(0.4f, 0.44f, 0.46f); RenderSettings.ambientGroundColor = new Color(0.23f, 0.22f, 0.2f);
            RenderSettings.skybox = null; RenderSettings.fog = false;
            AddHud(runtime, hudTitle);
            return runtime;
        }

        // The original in-race HUD and pause menu: components only (layouts, fonts, art and strings come from the install at run time).
        internal static OriginalHud CreateOriginalHud(GameObject host)
        {
            // Nothing is copied from the game: OriginalHud / OriginalPauseMenu read their layouts, fonts, art and strings at run
            // time from the player's install (ReflexUiData over Database/MXUI.dx9.*).
            var hud = host.AddComponent<OriginalHud>();
            var pause = host.AddComponent<OriginalPauseMenu>();
            var rt = host.GetComponent<OriginalWorldRuntime>();
            if (rt) { pause.runtime = rt; rt.pauseMenu = pause; }
            return hud;
        }

        // The licensed in-race soundtrack (MUSIC_IN_GAME). Nothing is stored in the project: OriginalMusic reads the track list from the
        // Reflex install's MusicAudioMap.bxml and streams each track's MPEG out of its FSB bank at run time (see OriginalMusic).
        internal static OriginalMusic CreateMusic(string level = "")
        {
            var music = new GameObject("Original in-race music (MUSIC_IN_GAME)").AddComponent<OriginalMusic>();
            music.level = level;
            return music;
        }

        // All game sound for a level. Nothing is stored in the project: OriginalAudio builds the level's FMOD event library from the
        // Reflex install's FEV/FSB data at run time (ReflexAudioSet), see _re/notes/game-audio.md.
        internal static OriginalAudio CreateGameAudio(string level)
        {
            var go = new GameObject("Original game sound (" + level + ")");
            var audio = go.AddComponent<OriginalAudio>();
            audio.level = level;
            return audio;
        }

        internal static void AddHud(OriginalWorldRuntime runtime, string title = "REFLEX / ALPINE · ORIGINAL GAME PHYSICS")
        {
            var canvas = new GameObject("HUD", typeof(Canvas), typeof(UnityEngine.UI.CanvasScaler)); canvas.GetComponent<Canvas>().renderMode = RenderMode.ScreenSpaceOverlay;
            var scaler = canvas.GetComponent<UnityEngine.UI.CanvasScaler>(); scaler.uiScaleMode = UnityEngine.UI.CanvasScaler.ScaleMode.ScaleWithScreenSize; scaler.referenceResolution = new Vector2(1440, 900); scaler.matchWidthOrHeight = 0.5f;
            var font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
            Panel(canvas.transform, "Top backdrop", false, 230); Panel(canvas.transform, "Bottom backdrop", true, 60);
            Text(canvas.transform, font, "Title", title, new Vector2(28, -20), new Vector2(1300, 34), 24, Color.white, false);
            runtime.statusLabel = Text(canvas.transform, font, "Status", "Starting", new Vector2(28, -58), new Vector2(1380, 110), 16, new Color(0.65f, 0.9f, 0.72f), false);
            runtime.telemetryLabel = Text(canvas.transform, font, "Telemetry", "", new Vector2(28, -150), new Vector2(1380, 70), 26, Color.white, false);
            runtime.raceLabel = Text(canvas.transform, font, "Race", "", new Vector2(1040, -58), new Vector2(380, 150), 20, Color.white, false);
            runtime.raceLabel.alignment = TextAnchor.UpperRight; runtime.raceLabel.supportRichText = true;
            runtime.helpLabel = Text(canvas.transform, font, "Help", "", new Vector2(28, 18), new Vector2(1380, 30), 15, new Color(0.78f, 0.82f, 0.88f), true);
        }
        static void Panel(Transform parent, string name, bool bottom, float height)
        {
            var item = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Image)); item.transform.SetParent(parent, false);
            var rect = item.GetComponent<RectTransform>(); rect.anchorMin = new Vector2(0, bottom ? 0 : 1); rect.anchorMax = new Vector2(1, bottom ? 0 : 1); rect.pivot = new Vector2(0.5f, bottom ? 0 : 1); rect.sizeDelta = new Vector2(0, height);
            var image = item.GetComponent<UnityEngine.UI.Image>(); image.color = new Color(0.035f, 0.052f, 0.07f, 0.85f); image.raycastTarget = false;
        }
        static UnityEngine.UI.Text Text(Transform parent, Font font, string name, string text, Vector2 point, Vector2 size, int fontSize, Color color, bool bottom)
        {
            var go = new GameObject(name, typeof(RectTransform), typeof(UnityEngine.UI.Text)); go.transform.SetParent(parent, false);
            var rect = go.GetComponent<RectTransform>(); rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0, bottom ? 0 : 1); rect.anchoredPosition = point; rect.sizeDelta = size;
            var label = go.GetComponent<UnityEngine.UI.Text>(); label.font = font; label.text = text; label.fontSize = fontSize; label.color = color; label.raycastTarget = false; label.horizontalOverflow = HorizontalWrapMode.Wrap; label.verticalOverflow = VerticalWrapMode.Overflow; label.supportRichText = true;
            return label;
        }
    }
}
