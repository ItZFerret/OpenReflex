using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Editor
{
    // The port's hand-written shaders (Assets/Shaders: vehicle, scenery, sky, terrain, particles, front-end UI ...) are
    // found at run time by name (Shader.Find), so player builds must include them: they go into Graphics Settings'
    // Always Included Shaders. The scene builders call Ensure().
    public static class ReflexShaderIncludes
    {
        [MenuItem("Reflex/Include port shaders in builds")]
        public static void Ensure()
        {
            var shaders = AssetDatabase.FindAssets("t:Shader", new[] { "Assets/Shaders" })
                .Select(g => AssetDatabase.LoadAssetAtPath<Shader>(AssetDatabase.GUIDToAssetPath(g))).Where(s => s).ToList();
            var settings = AssetDatabase.LoadAssetAtPath<Object>("ProjectSettings/GraphicsSettings.asset");
            if (!settings) { Debug.LogWarning("GraphicsSettings.asset not found"); return; }
            var so = new SerializedObject(settings);
            var list = so.FindProperty("m_AlwaysIncludedShaders");
            var have = Enumerable.Range(0, list.arraySize).Select(i => list.GetArrayElementAtIndex(i).objectReferenceValue).ToList();
            int added = 0;
            foreach (var s in shaders)
            {
                if (have.Contains(s)) continue;
                list.InsertArrayElementAtIndex(list.arraySize);
                list.GetArrayElementAtIndex(list.arraySize - 1).objectReferenceValue = s;
                added++;
            }
            if (added > 0) { so.ApplyModifiedProperties(); AssetDatabase.SaveAssets(); }
            Debug.Log($"Reflex shaders always included: {shaders.Count} ({added} added)");
        }
    }
}
