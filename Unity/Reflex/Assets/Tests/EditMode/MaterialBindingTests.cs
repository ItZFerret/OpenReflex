using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class MaterialBindingTests
    {
        // Content lives in Assembly-CSharp; this test assembly references the core only.
        static List<string> Merge(IList<string> bindings, IList<string> overrides)
        {
            var type = AppDomain.CurrentDomain.GetAssemblies()
                .Select(a => a.GetType("Reflex.Port.Content.ReflexLevelContent"))
                .FirstOrDefault(t => t != null);
            Assert.IsNotNull(type, "Level content assembly must be loaded");
            var method = type.GetMethod("BindingMaterials", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.IsNotNull(method);
            return (List<string>)method.Invoke(null, new object[] { bindings, overrides });
        }

        [Test]
        public void PartialSceneListPreservesUnspecifiedBindings()
        {
            CollectionAssert.AreEqual(new[] { "scene", "rope", "pole" },
                Merge(new[] { "default", "rope", "pole" }, new[] { "scene" }));
        }

        [Test]
        public void NullAndEmptySlotsPreserveModelBindings()
        {
            CollectionAssert.AreEqual(new[] { "flag", "rope", "scene" },
                Merge(new[] { "flag", "rope", "pole" }, new[] { null, "", "scene" }));
        }

        [Test]
        public void ExcessOverridesDoNotCreateMeshBindings()
        {
            CollectionAssert.AreEqual(new[] { "scene" }, Merge(new[] { "flag" }, new[] { "scene", "extra" }));
        }

        [Test]
        public void NoSceneListReturnsIndependentDefaults()
        {
            var original = new List<string> { "flag", "rope" };
            var selected = Merge(original, null);
            CollectionAssert.AreEqual(original, selected);
            selected[0] = "changed";
            Assert.AreEqual("flag", original[0]);
        }
        static List<string> MergeLod(IList<string> lod0, IList<string> bindings, IList<string> overrides)
        {
            var type = AppDomain.CurrentDomain.GetAssemblies()
                .Select(a => a.GetType("Reflex.Port.Content.ReflexLevelContent")).First(t => t != null);
            return (List<string>)type.GetMethod("LodBindingMaterials", BindingFlags.Static | BindingFlags.NonPublic)
                .Invoke(null, new object[] { lod0, bindings, overrides });
        }

        [Test]
        public void HigherLodUsesBindingSlotsWhenNamesRepeatOrChange()
        {
            // A name-keyed map loses distinct overrides for the repeated default and misses renamed LOD bindings.
            CollectionAssert.AreEqual(new[] { "left", "right" },
                MergeLod(new[] { "same", "same" }, new[] { "other", "same" }, new[] { "left", "right" }));
        }

        [Test]
        public void HigherLodStopsAfterLod0DifferenceCount()
        {
            CollectionAssert.AreEqual(new[] { "scene", "other" },
                MergeLod(new[] { "default", "same" }, new[] { "default", "other" }, new[] { "scene", "same" }));
        }

        [Test]
        public void HigherLodSkipsNullWritesButCountsTheirLod0Differences()
        {
            CollectionAssert.AreEqual(new[] { "kept", "scene", "same" },
                MergeLod(new[] { "default", "default", "same" }, new[] { "kept", "default", "other" },
                    new[] { null, "scene", "same" }));
        }

    }
}
