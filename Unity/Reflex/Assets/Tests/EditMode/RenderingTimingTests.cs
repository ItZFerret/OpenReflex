using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class RenderingTimingTests
    {
        static Type Pipeline => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType("Reflex.Port.Rendering.ReflexRenderPipeline"))
            .First(t => t != null);

        // The original steps the eye adaptation once per frame it renders (one per game frame, 1/60 s at most), with the authored
        // rate unscaled; Unity frames that show the same game frame again do not step it.
        static float Rate(float rate, float frames) =>
            (float)Pipeline.GetMethod("AdaptationRate", BindingFlags.Public | BindingFlags.Static).Invoke(null, new object[] { rate, frames });

        [Test]
        public void OneOriginalFrameIsTheAuthoredRate() => Assert.AreEqual(0.05f, Rate(0.05f, 1f));

        [Test]
        public void TheSameGameFrameDrawnAgainDoesNotAdapt() => Assert.AreEqual(0f, Rate(0.05f, 0f));

        [TestCase(2f)]
        [TestCase(0.5f)]
        [TestCase(3.25f)]
        public void SeveralOrPartialFramesComposeLikeSingleSteps(float frames)
        {
            float expected = 1f - Mathf.Pow(0.95f, frames);
            Assert.That(Rate(0.05f, frames), Is.EqualTo(expected).Within(1e-6f));
            if (frames == 2f) Assert.That(Rate(0.05f, 2f), Is.EqualTo(0.05f + 0.95f * 0.05f).Within(1e-6f));   // two original steps
        }

        [Test]
        public void PipelineExposesTheOriginalFrameSource()
        {
            Assert.IsNotNull(Pipeline.GetField("originalFrame"));
            Assert.IsNull(Pipeline.GetField("frameRateIndependentAdaptation"));
        }
    }
}
