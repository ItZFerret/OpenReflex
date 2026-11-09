using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class FrameRateSettingsTests
    {
        // Options lives in Assembly-CSharp. Exercise its actual precedence and
        // Unity application path without reading/writing any player's save.
        static Type Live => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType("Reflex.Port.Options.ReflexClientLive"))
            .First(t => t != null);
        static int Resolve(string launch, int saved) => (int)Live.GetMethod("ResolveFrameRateCap", BindingFlags.NonPublic | BindingFlags.Static)
            .Invoke(null, new object[] { launch, saved });
        static bool Apply(int cap, bool vsync) => (bool)Live.GetMethod("ApplyFrameRateLimit", BindingFlags.NonPublic | BindingFlags.Static,
            null, new[] { typeof(int), typeof(bool) }, null).Invoke(null, new object[] { cap, vsync });

        [TestCase("30", 144, true, 30, 0)]
        [TestCase(null, 30, true, 30, 0)]
        [TestCase(null, 0, true, 0, 1)]
        [TestCase(null, 0, false, 0, 0)]
        [TestCase("invalid", 0, true, 0, 1)]
        [TestCase("0", 60, true, 60, 0)]
        [TestCase("-10", 0, false, 0, 0)]
        [TestCase("999", 0, true, 240, 0)]
        [TestCase("1", 60, true, 15, 0)]
        public void LaunchThenSavedCapOverridesVsync(string launch, int saved, bool vsync, int expectedCap, int expectedSync)
        {
            int target = Application.targetFrameRate, sync = QualitySettings.vSyncCount;
            try
            {
                int cap = Resolve(launch, saved);
                Assert.AreEqual(expectedCap, cap);
                Assert.AreEqual(cap > 0, Apply(cap, vsync));
                Assert.AreEqual(expectedSync, QualitySettings.vSyncCount);
                if (cap > 0) Assert.AreEqual(cap, Application.targetFrameRate);
            }
            finally { Application.targetFrameRate = target; QualitySettings.vSyncCount = sync; }
        }

        [Test]
        public void TurningCapOffRestoresVsyncAfterItWasSuppressed()
        {
            int target = Application.targetFrameRate, sync = QualitySettings.vSyncCount;
            try
            {
                Assert.IsTrue(Apply(30, true));
                Assert.AreEqual(0, QualitySettings.vSyncCount);
                Assert.IsFalse(Apply(0, true));
                Assert.AreEqual(1, QualitySettings.vSyncCount);
            }
            finally { Application.targetFrameRate = target; QualitySettings.vSyncCount = sync; }
        }
    }
}
