using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class CoreStuntCatalogTests
    {
        static Type Catalog => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("Reflex.Port.Content.CoreStuntCatalog")).First(t => t != null);
        static List<(int tier, string name, string key)> Read(params (string name, object physics, object tier, string key)[] rows)
            => (List<(int, string, string)>)Catalog.GetMethod("FromRows", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { rows });

        [Test]
        public void PhysicsFalseIsSuccessfulReadAndDoesNotDiscardAnimationStunts()
        {
            var result = Read(("FALSE_FLAG", false, 1, "KEY_A"), ("STRING_FALSE", "false", "2", "KEY_B"), ("TRUE_FLAG", true, 3, "KEY_C"));
            CollectionAssert.AreEqual(new[] { "false_flag", "string_false", "true_flag" }, result.Select(x => x.name).ToArray());
        }
        [Test]
        public void MissingEmptyOrWrongTypedPhysicsIsRejectedButNonemptyStringUsesNativeReadSuccess()
        {
            var result = Read(("MISSING", null, 1, "A"), ("EMPTY", "", 1, "B"), ("INTEGER", 1, 1, "C"), ("NATIVE_STRING", "anything", 1, "D"));
            Assert.AreEqual(1, result.Count); Assert.AreEqual("native_string", result[0].name);
        }
        [Test]
        public void TierBucketsPreserveSourceOrderAndDuplicateLocalizationKeys()
        {
            var result = Read(("FOUR", false, 4, "SHARED"), ("TWO_A", false, 2, "SHARED"), ("ONE", false, 1, "ONE"), ("TWO_B", false, 2, "SHARED"), ("THREE", false, 3, "THREE"));
            CollectionAssert.AreEqual(new[] { "one", "two_a", "two_b", "three", "four" }, result.Select(x => x.name).ToArray());
            Assert.AreEqual(3, result.Count(x => x.key == "SHARED"));
        }
        [Test]
        public void TierReaderMatchesTypedIntAndNativeStringPrefixes()
        {
            var result = Read(("DEC", false, " 2trailing", "A"), ("HEX", false, "0x3", "B"), ("OCT", false, "04", "C"), ("TRUE", false, "true", "D"), ("OUTSIDE", false, 5, "E"), ("UINT", false, 1u, "F"));
            CollectionAssert.AreEqual(new[] { 1, 2, 3, 4 }, result.Select(x => x.tier).ToArray());
        }
        [Test]
        public void DescriptorNameUsesOriginalThirtyCharacterLimitWithoutChangingLocalizationKey()
        {
            string name = new string('A', 35);
            var result = Read((name, false, 1, "STR_MixedCase"));
            Assert.AreEqual(new string('a', 30), result[0].name); Assert.AreEqual("STR_MixedCase", result[0].key);
        }
        [Test]
        public void InstalledAssetMatchesAcceptedNativeSixtyFourRowsAndTierCounts()
        {
            string path = Environment.GetEnvironmentVariable("REFLEX_STUNT_TEST_ASSET");
            if (string.IsNullOrEmpty(path)) path = Path.GetFullPath(Path.Combine(Application.dataPath, "../../..", "builds/sol_garage/game files/Assets/bxml/StuntDatabase/StuntData/StuntTable.bxml"));
            if (!File.Exists(path)) Assert.Ignore("Set REFLEX_STUNT_TEST_ASSET to the existing extracted StuntTable.bxml for installed-content acceptance.");
            var result = (List<(int tier, string name, string key)>)Catalog.GetMethod("Parse", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, new object[] { File.ReadAllBytes(path) });
            Assert.AreEqual(64, result.Count);
            CollectionAssert.AreEqual(new[] { 18, 21, 11, 14 }, Enumerable.Range(1, 4).Select(t => result.Count(x => x.tier == t)).ToArray());
            Assert.AreEqual(43, result.Select(x => x.key).Distinct().Count());
            Assert.AreEqual((1, "barhop_air", "STR_BARHOP"), result[0]);
            Assert.AreEqual((4, "buzzsaw_air", "STR_BUZZSAW"), result[result.Count - 1]);
        }
    }
}
