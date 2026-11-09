using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Editor
{
    // Scenery is no longer baked into the project: Assets/Scripts/OriginalWorld/OriginalLevel.cs reads the level's models, textures,
    // materials and scene placements from the player's own Reflex install at run time (Assets/Scripts/Content/ReflexScenery.cs) and
    // renders them with the hand-ported "Reflex/Original Scenery" shader. This builder only adds that component to the scene.
    public static class ReflexSceneryBuilder
    {
        // Members kept only so the legacy validation scripts (ReflexSceneryValidation, ReflexRockShaderValidation ...), which still
        // inspect the former baked assets, keep compiling. They describe files that are no longer generated.
        public const string RootName = "Original Alpine scenery — authored world placement";
        public const string EdgeShaderName = "Reflex/Recovered Alpine Edge";
        public const string RockShaderName = "Reflex/Recovered Alpine Rock Color";
        public static bool IsRecoveredEdge(string name) => name == "FR_AL_EoW_Base_PreLit" || name == "FR_AL_EoW_Base_PreLit_Alpha";

        const string SceneryShaderPath = "Assets/Shaders/ReflexOriginalScenery.shader", SkyShaderPath = "Assets/Shaders/ReflexOriginalSky.shader", TerrainShaderPath = "Assets/Shaders/ReflexTerrain.shader",
                     TrackShaderPath = "Assets/Shaders/ReflexTrackDecal.shader";

        /// The scene's OriginalLevel (created on first use) for a level database name ("NAT_Concord", "FR_Alpine" ...). The component
        /// holds only the level name and references to our own shaders; every game resource is loaded when the scene runs.
        public static OriginalLevel EnsureLevel(string level)
        {
            var existing = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
            if (existing && existing.level == level) return existing;
            var go = new GameObject($"Original level {level} (content read from the install at run time)");
            var c = go.AddComponent<OriginalLevel>();
            c.level = level;
            c.sceneryShader = AssetDatabase.LoadAssetAtPath<Shader>(SceneryShaderPath);
            c.skyShader = AssetDatabase.LoadAssetAtPath<Shader>(SkyShaderPath);
            c.terrainShader = AssetDatabase.LoadAssetAtPath<Shader>(TerrainShaderPath);
            c.trackShader = AssetDatabase.LoadAssetAtPath<Shader>(TrackShaderPath);
            return c;
        }

        /// Adds the level's scenery to the scene: the OriginalLevel component (Alpine, for the legacy discovery scenes).
        public static GameObject AddOriginalScenery() => EnsureLevel("FR_Alpine").gameObject;
    }
}
