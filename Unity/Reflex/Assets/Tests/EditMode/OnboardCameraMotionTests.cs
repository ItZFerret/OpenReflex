using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    // Regression checks for the source input contract and travel-frame separation missed by static FPV probes.
    public class OnboardCameraMotionTests
    {
        static Type CameraType => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("Reflex.Port.OriginalWorld.OriginalOnboardCamera")).First(t => t != null);
        const BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;
        static object Call(object camera, string method, params object[] args) => CameraType.GetMethod(method, Private).Invoke(camera, args);
        static void Field(object camera, string name, object value) => CameraType.GetField(name).SetValue(camera, value);
        static void Fill(object camera, string field, Vector3 value)
        {
            var box = CameraType.GetField(field, Private).GetValue(camera);
            box.GetType().GetMethod("Fill").Invoke(box, new object[] { value });
        }

        [TestCase(1f / 30f, 6)]
        [TestCase(1f / 60f, 12)]
        public void ForeAftCameraInputUsesSourceThreePointFivePerSecond(float dt, int frames)
        {
            var go = new GameObject("Source lean input contract"); go.SetActive(false);
            try
            {
                var camera = go.AddComponent(CameraType);
                Assert.AreEqual(0f, Call(camera, "FilterLeanForward", 0f, 0f));
                float value = 0;
                for (int i = 0; i < frames; i++) value = (float)Call(camera, "FilterLeanForward", 1f, dt);
                // MXVehicleMotoShared::vf36 YAxisRateLimit=YDPadRateLimit=3.5/s: 0.2s reaches0.7, not1.
                Assert.That(value, Is.EqualTo(.7f).Within(.00001f));
                Assert.That((float)Call(camera, "FilterLeanForward", -1f, 0f), Is.EqualTo(.7f).Within(.00001f), "Repeated render of one game clock must not change lean");
                for (int i = 0; i < frames; i++) value = (float)Call(camera, "FilterLeanForward", -1f, dt);
                Assert.That(value, Is.EqualTo(0f).Within(.00001f), "Reversing input must also retain the source rate limit");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [TestCase(.31f, 21.2f)]
        [TestCase(.65f, 28f)]
        public void ForwardLeanPitchUsesFilteredAxisAtDefaultAndUserPitch(float configuredPitch, float neutralPitch)
        {
            var go = new GameObject("Lean pitch source response"); go.SetActive(false);
            try
            {
                var camera = go.AddComponent(CameraType); Field(camera, "pitch", configuredPitch); Field(camera, "leanPitch", true);
                Call(camera, "FilterLeanForward", 0f, 0f);
                float lean = (float)Call(camera, "FilterLeanForward", 1f, 1f / 30f);
                Assert.That((float)Call(camera, "DynamicPitch", lean), Is.EqualTo(neutralPitch + 1.1666667f).Within(.00001f), "Raw input previously jumped the view by10degrees in one frame");
                Field(camera, "leanPitch", false);
                Assert.That((float)Call(camera, "DynamicPitch", lean), Is.EqualTo(neutralPitch).Within(.00001f));
                Field(camera, "fov", 140f); Field(camera, "offset", new Vector3(0f, .2f, -.6500001f));
                CameraType.GetMethod("Resnap").Invoke(camera, null);
                Assert.AreEqual(140f, CameraType.GetField("fov").GetValue(camera));
                Assert.AreEqual(new Vector3(0f, .2f, -.6500001f), CameraType.GetField("offset").GetValue(camera), "Lifecycle reset must not tune the user's view");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [TestCase(-65f)]
        [TestCase(65f)]
        public void DynamicWhipViewFollowsTravelWhileBodyTurnsUnderIt(float bodyYaw)
        {
            var go = new GameObject("Dynamic whip framing"); go.SetActive(false);
            try
            {
                var camera = go.AddComponent(CameraType);
                Fill(camera, "upBox", Vector3.up); Fill(camera, "velBox", Vector3.forward * 40f);
                Quaternion body = Quaternion.Euler(0, bodyYaw, 35);
                Quaternion view = (Quaternion)Call(camera, "ViewFrame", body, false, false);
                Assert.That(Vector3.Angle(view * Vector3.forward, Vector3.forward), Is.LessThan(.01f));
                Assert.That(Vector3.Angle(view * Vector3.forward, body * Vector3.forward), Is.GreaterThan(60f), "DYNAMIC must not turn its gaze with the whip's chassis heading at speed");
                Assert.That(Mathf.Abs((view * Vector3.right).y), Is.LessThan(.0001f));
                Assert.That((float)CameraType.GetProperty("TravelBlend").GetValue(camera), Is.EqualTo(1f));
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [Test]
        public void LowSpeedDynamicViewRetainsSourceChassisHeading()
        {
            var go = new GameObject("Low speed travel blend"); go.SetActive(false);
            try
            {
                var camera = go.AddComponent(CameraType);
                Fill(camera, "upBox", Vector3.up); Fill(camera, "velBox", Vector3.forward * 2f);
                Quaternion body = Quaternion.Euler(0, 65, 0);
                Quaternion view = (Quaternion)Call(camera, "ViewFrame", body, false, false);
                Assert.That(Vector3.Angle(view * Vector3.forward, body * Vector3.forward), Is.LessThan(.01f));
                Assert.That((float)CameraType.GetProperty("TravelBlend").GetValue(camera), Is.EqualTo(0f));
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }
        [Test]
        public void MissingAuthoredPoseIsReportedSeparatelyFromNativeWreck()
        {
            var go = new GameObject("FPV fallback evidence"); go.SetActive(false);
            var riderGo = new GameObject("Authored rider not yet posed"); riderGo.SetActive(false);
            try
            {
                var camera = go.AddComponent(CameraType); var lens = go.AddComponent<Camera>();
                Type riderType = AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType("Reflex.Port.OriginalWorld.OriginalRider")).First(t => t != null);
                var rider = riderGo.AddComponent(riderType);
                CameraType.GetMethod("Bind").Invoke(camera, new object[] { lens, rider });
                object[] pose = { Vector3.zero, Quaternion.identity, Vector3.forward, Vector3.back, Vector3.zero, 0f, 0f, 0f, null, false };
                CameraType.GetMethod("SetContext").Invoke(camera, new object[] { false, false, true, true, false, true, false, false, false, 1d, false });
                Assert.AreEqual(false, CameraType.GetMethod("Apply").Invoke(camera, pose));
                Assert.AreEqual("wrecked", CameraType.GetProperty("FallbackReason").GetValue(camera));
                CameraType.GetMethod("SetContext").Invoke(camera, new object[] { false, false, true, false, false, true, false, false, false, 2d, false });
                Assert.AreEqual(false, CameraType.GetMethod("Apply").Invoke(camera, pose));
                Assert.AreEqual("authored-pose-missing", CameraType.GetProperty("FallbackReason").GetValue(camera), "A pose publication gap must not be diagnosed as a crash");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); UnityEngine.Object.DestroyImmediate(riderGo); }
        }
    }
}
