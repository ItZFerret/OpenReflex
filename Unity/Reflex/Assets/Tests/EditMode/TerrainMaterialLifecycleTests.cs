using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class TerrainMaterialLifecycleTests
    {
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType(name)).First(t => t != null);

        // Exercise Build itself, unlike the visual fixtures that construct their own
        // TerrainData and override basemapDistance before rendering.
        [TestCase(true)]
        [TestCase(false)]
        public void RuntimeBuildKeepsAuthoredFarMaterialAndOrdinaryTerrainBasemap(bool original)
        {
            var type = Find("Reflex.Port.OriginalWorld.OriginalTerrain");
            var infoType = Find("Reflex.Port.OriginalWorld.ReflexTerrainInfo");
            var go = new GameObject("Terrain lifecycle test");
            var shader = Shader.Find(original ? "Reflex/Original HFTerrain" : "Universal Render Pipeline/Terrain/Lit");
            Assert.IsNotNull(shader);
            var material = new Material(shader);
            Component component = null;
            Terrain terrain = null;
            Material liveMaterial = null;
            TerrainData data = null;
            try
            {
                component = go.AddComponent(type);
                type.GetField("materialTemplate").SetValue(component, material);
                type.GetField("basemapDistance").SetValue(component, 2500f);
                object info = Activator.CreateInstance(infoType);
                void Set(string field, object value) => infoType.GetField(field).SetValue(info, value);
                Set("samples", 32u); Set("stride", 32u); Set("spacing", 100f);
                Set("min_x", 0f); Set("min_z", 0f); Set("max_x", 3100f); Set("max_z", 3100f);
                type.GetMethod("Build", new[] { infoType, typeof(float[]), typeof(float[]) })
                    .Invoke(component, new object[] { info, new float[32 * 32], null });
                terrain = go.GetComponentInChildren<Terrain>();
                Assert.IsNotNull(terrain);
                data = terrain.terrainData;
                liveMaterial = terrain.materialTemplate;
                Assert.AreSame(shader, liveMaterial.shader);
                if (original)
                {
                    Assert.That(terrain.basemapDistance, Is.GreaterThan(1e20f), "No Unity base pass is available for this material");
                    Assert.That(data.terrainLayers, Is.Empty);
                    Assert.AreEqual(new Vector4(0f, 3100f, 3100f, 0f), liveMaterial.GetVector("_GridOrigin"));
                    Assert.AreEqual(new Vector4(384f, 128f, 0f, 0f), liveMaterial.GetVector("_DetailFade"), "Preserve original detail fade");
                }
                else Assert.AreEqual(2500f, terrain.basemapDistance);
            }
            finally
            {
                // Release is a play-mode lifecycle method using deferred Destroy.
                // Own the objects explicitly here so EditMode cleanup is immediate.
                if (component)
                    foreach (string field in new[] { "terrain", "data", "runtimeMaterial" })
                        type.GetField(field, BindingFlags.NonPublic | BindingFlags.Instance).SetValue(component, null);
                if (go) UnityEngine.Object.DestroyImmediate(go);
                if (liveMaterial && liveMaterial != material) UnityEngine.Object.DestroyImmediate(liveMaterial);
                if (data) UnityEngine.Object.DestroyImmediate(data);
                UnityEngine.Object.DestroyImmediate(material);
            }
        }
    }
}
