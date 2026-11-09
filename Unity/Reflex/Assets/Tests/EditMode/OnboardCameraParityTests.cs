using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class OnboardCameraParityTests
    {
        static Type CameraType => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType("Reflex.Port.OriginalWorld.OriginalOnboardCamera"))
            .First(t => t != null);

        // Numeric reference values evaluated from Open Unleashed's actual equation, rather than its Hor+ comment.
        [TestCase(116f, 4f / 3f, 100.400639f)]
        [TestCase(116f, 16f / 9f, 83.986337f)]
        [TestCase(116f, 9f / 16f, 100.400639f)]
        [TestCase(116f, 0f, 100.400639f)]
        [TestCase(140f, 16f / 9f, 114.189565f)]
        public void VerticalFovMatchesSourceEquation(float fov, float aspect, float expected)
        {
            float actual = (float)CameraType.GetMethod("VerticalFov").Invoke(null, new object[] { fov, aspect });
            Assert.That(actual, Is.EqualTo(expected).Within(0.0001f));
        }

        [TestCase(0)]
        [TestCase(1)]
        public void ExistingSavedFovAndHeadModeArePreserved(int mode)
        {
            const string fovKey = "ReflexFpv.fov", modeKey = "ReflexFpv.mode";
            bool hadFov = PlayerPrefs.HasKey(fovKey), hadMode = PlayerPrefs.HasKey(modeKey);
            float savedFov = PlayerPrefs.GetFloat(fovKey);
            int savedMode = PlayerPrefs.GetInt(modeKey);
            var go = new GameObject("FPV preference migration check"); go.SetActive(false);
            try
            {
                PlayerPrefs.SetFloat(fovKey, 140f); PlayerPrefs.SetInt(modeKey, mode);
                var onboard = go.AddComponent(CameraType);
                CameraType.GetMethod("Load").Invoke(onboard, null);
                Assert.AreEqual(140f, CameraType.GetField("fov").GetValue(onboard));
                Assert.AreEqual(mode, CameraType.GetField("headMode").GetValue(onboard));
                Assert.AreEqual(140f, PlayerPrefs.GetFloat(fovKey), "Loading must not reset existing preferences");
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(go);
                if (hadFov) PlayerPrefs.SetFloat(fovKey, savedFov); else PlayerPrefs.DeleteKey(fovKey);
                if (hadMode) PlayerPrefs.SetInt(modeKey, savedMode); else PlayerPrefs.DeleteKey(modeKey);
            }
        }

        static object Invoke(object instance, string method, params object[] args) => CameraType
            .GetMethod(method, BindingFlags.Instance | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public)
            .Invoke(instance, args);

        [TestCase(0.1f, 0f)]
        [TestCase(0.575f, 0.25f)]
        [TestCase(-0.575f, -0.25f)]
        public void FreeLookStickUsesSourceDeadzoneAndSquaredResponse(float input, float expected)
        {
            Assert.That((float)Invoke(null, "HeadingCurve", input), Is.EqualTo(expected).Within(0.00001f));
        }

        [Test]
        public void TrueRollLevelsUprightGazeWithoutChangingLookDirectionAndPreservesInversion()
        {
            var go = new GameObject("TRUE horizon test"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                Quaternion head = Quaternion.Euler(10, 30, 20);
                Quaternion leveled = (Quaternion)Invoke(onboard, "TrueRoll", head, 0f);
                Assert.That(Vector3.Angle(head * Vector3.forward, leveled * Vector3.forward), Is.LessThan(0.01f));
                Assert.That(Mathf.Abs((leveled * Vector3.right).y), Is.LessThan(0.0001f));
                Assert.That(Quaternion.Angle(head, (Quaternion)Invoke(onboard, "TrueRoll", head, 1f)), Is.LessThan(0.01f));
                Quaternion inverted = Quaternion.Euler(0, 0, 180);
                Assert.That(Quaternion.Angle(inverted, (Quaternion)Invoke(onboard, "TrueRoll", inverted, 0f)), Is.LessThan(0.01f));
                Quaternion vertical = Quaternion.Euler(90, 0, 20);
                Assert.That(Quaternion.Angle(vertical, (Quaternion)Invoke(onboard, "TrueRoll", vertical, 0f)), Is.LessThan(0.01f));
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [Test]
        public void TrueTravelAtFullWeightsUsesTravelHeadingAndDynamicLookDown()
        {
            var go = new GameObject("TRUE travel test"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                Quaternion actual = (Quaternion)Invoke(onboard, "TrueTravel", Quaternion.Euler(-10, 30, 20), Quaternion.identity, 1f, 1f, 21.2f);
                Vector3 expected = Quaternion.Euler(21.2f, 0, 0) * Vector3.forward;
                Assert.That(Vector3.Angle(expected, actual * Vector3.forward), Is.LessThan(0.01f));
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [Test]
        public void HeadGazeCalibratesNonCanonicalBoneAxesAndRetainsAnimatedTurn()
        {
            var go = new GameObject("Calibrated head test"); go.SetActive(false);
            var head = new GameObject("Non canonical head");
            try
            {
                var onboard = go.AddComponent(CameraType);
                var riderType = CameraType.Assembly.GetType("Reflex.Port.OriginalWorld.OriginalRider");
                var rider = go.AddComponent(riderType);
                var bind = new float[34 * 16];
                // Neutral bone points along +X, but physical eye nodes lie in front along +Z.
                Quaternion boneBasis = Quaternion.Euler(0, 90, 0);
                Vector3 f = boneBasis * Vector3.forward, u = boneBasis * Vector3.up;
                int start = 31 * 16;
                bind[start + 8] = f.x; bind[start + 9] = f.y; bind[start + 10] = f.z;
                bind[start + 4] = u.x; bind[start + 5] = u.y; bind[start + 6] = u.z;
                bind[32 * 16 + 12] = 0.1f; bind[33 * 16 + 12] = -0.1f;
                bind[32 * 16 + 14] = bind[33 * 16 + 14] = 0.3f;
                riderType.GetField("bindWorldMatrices").SetValue(rider, bind);
                CameraType.GetMethod("Bind").Invoke(onboard, new object[] { go.AddComponent<Camera>(), rider });
                head.transform.rotation = boneBasis;
                Quaternion neutral = (Quaternion)Invoke(onboard, "HeadGaze", head.transform, Vector3.forward * 0.3f, Quaternion.identity);
                Assert.That(Vector3.Angle(Vector3.forward, neutral * Vector3.forward), Is.LessThan(0.01f));
                head.transform.rotation = Quaternion.AngleAxis(25, Vector3.up) * boneBasis;
                Quaternion animated = (Quaternion)Invoke(onboard, "HeadGaze", head.transform, Vector3.forward * 0.3f, Quaternion.identity);
                Assert.That(Vector3.Angle(Quaternion.AngleAxis(25, Vector3.up) * Vector3.forward, animated * Vector3.forward), Is.LessThan(0.01f));
            }
            finally { UnityEngine.Object.DestroyImmediate(head); UnityEngine.Object.DestroyImmediate(go); }
        }

        [Test]
        public void LookInputIsConsumedOncePerRenderAndResnapStartsNewView()
        {
            var go = new GameObject("Look frame guard test"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                CameraType.GetMethod("Resnap").Invoke(onboard, null);
                object[] first = { false }; Invoke(onboard, "FrameTime", first);
                object[] repeated = { true }; float repeatedDt = (float)Invoke(onboard, "FrameTime", repeated);
                Assert.IsTrue((bool)first[0]); Assert.IsFalse((bool)repeated[0]); Assert.AreEqual(0f, repeatedDt);
                CameraType.GetMethod("Resnap").Invoke(onboard, null);
                object[] restarted = { false }; Invoke(onboard, "FrameTime", restarted);
                Assert.IsTrue((bool)restarted[0], "A restarted view must accept its first look update");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        // Native order 1 -> 3 -> 2 -> 0 -> 1, with Dynamic FPV inserted after native first person (3).
        [TestCase(0, 1, 1)]
        [TestCase(1, 1, 3)]
        [TestCase(3, 1, 4)]
        [TestCase(4, 1, 2)]
        [TestCase(2, 1, 0)]
        [TestCase(0, -1, 2)]
        [TestCase(2, -1, 4)]
        [TestCase(4, -1, 3)]
        [TestCase(3, -1, 1)]
        [TestCase(1, -1, 0)]
        public void RegularCameraCyclePreservesNativeOrderWithDynamicFpvInserted(int current, int direction, int expected)
        {
            var type = CameraType.Assembly.GetType("Reflex.Port.Options.ReflexClientLive");
            Assert.AreEqual(expected, type.GetMethod("NextCameraIndex").Invoke(null, new object[] { current, direction }));
        }

        static void FillFilter(object onboard, string field, Vector3 value)
        {
            object filter = CameraType.GetField(field, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(onboard);
            filter.GetType().GetMethod("Fill").Invoke(filter, new object[] { value });
        }

        [TestCase(false)]
        [TestCase(true)]
        public void MachineTravelKeepsBonnetForwardWhenReversing(bool reversing)
        {
            var go = new GameObject("Machine travel heading"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                FillFilter(onboard, "upBox", Vector3.up);
                Vector3 velocity = reversing ? Vector3.back * 50 : Quaternion.Euler(0, 30, 0) * Vector3.forward * 50;
                FillFilter(onboard, "velBox", velocity);
                Quaternion machine = (Quaternion)Invoke(onboard, "ViewFrame", Quaternion.identity, false, true);
                Quaternion bike = (Quaternion)Invoke(onboard, "ViewFrame", Quaternion.identity, false, false);
                Assert.That(Vector3.Angle(machine * Vector3.forward, reversing ? Vector3.forward : velocity.normalized), Is.LessThan(0.01f));
                Assert.That(Vector3.Angle(bike * Vector3.forward, velocity.normalized), Is.LessThan(0.01f));
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        [Test]
        public void TruncatedRouteStaysOpenAndBlendedTangentsMeetAtCorner()
        {
            var go = new GameObject("Open race route"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                float[] route = { 0, 0, 0, 0, 0, 10, 10, 0, 10 };
                CameraType.GetMethod("OnSimFrame").Invoke(onboard, new object[] { 0f, route, 3, false });
                Assert.AreEqual(-1, Invoke(onboard, "Next", 2), "A truncated route must not acquire a closing chord");
                Vector3 before = (Vector3)Invoke(onboard, "Tangent", 0, 1f);
                Vector3 after = (Vector3)Invoke(onboard, "Tangent", 1, 0f);
                Assert.That(Vector3.Angle(before, after), Is.LessThan(0.01f), "Heading must be continuous at the real route corner");
                Vector3 end = (Vector3)Invoke(onboard, "Tangent", 1, 1f);
                Assert.That(Vector3.Angle(Vector3.right, end), Is.LessThan(0.01f), "Route end must keep its last real segment heading");
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }

        sealed class FpvPreferenceScope : IDisposable
        {
            readonly string[] ints = { "mode", "leanPitch", "ca", "free", "shift" };
            readonly string[] floats = { "fov", "tilt", "pitch", "caScale", "leanHead", "shiftStrength", "shiftLong", "shiftLat", "shiftVert", "trueSmooth", "trueRoll", "trueYaw", "truePitch", "offX", "offY", "offZ" };
            readonly System.Collections.Generic.Dictionary<string, int> savedInts = new System.Collections.Generic.Dictionary<string, int>();
            readonly System.Collections.Generic.Dictionary<string, float> savedFloats = new System.Collections.Generic.Dictionary<string, float>();
            public FpvPreferenceScope()
            {
                foreach (string key in ints) if (PlayerPrefs.HasKey("ReflexFpv." + key)) savedInts[key] = PlayerPrefs.GetInt("ReflexFpv." + key);
                foreach (string key in floats) if (PlayerPrefs.HasKey("ReflexFpv." + key)) savedFloats[key] = PlayerPrefs.GetFloat("ReflexFpv." + key);
            }
            public void Dispose()
            {
                foreach (string key in ints) { if (savedInts.TryGetValue(key, out int value)) PlayerPrefs.SetInt("ReflexFpv." + key, value); else PlayerPrefs.DeleteKey("ReflexFpv." + key); }
                foreach (string key in floats) { if (savedFloats.TryGetValue(key, out float value)) PlayerPrefs.SetFloat("ReflexFpv." + key, value); else PlayerPrefs.DeleteKey("ReflexFpv." + key); }
                PlayerPrefs.Save();
            }
        }

        [Test]
        public void HeadShiftMigratesLegacyGainAndPersistsIndependentAxes()
        {
            using (new FpvPreferenceScope())
            {
                var go = new GameObject("FPV gain migration"); go.SetActive(false);
                try
                {
                    foreach (string axis in new[] { "shiftLong", "shiftLat", "shiftVert" }) PlayerPrefs.DeleteKey("ReflexFpv." + axis);
                    PlayerPrefs.SetFloat("ReflexFpv.shiftStrength", 1.7f);
                    var onboard = go.AddComponent(CameraType);
                    CameraType.GetMethod("Load").Invoke(onboard, null);
                    foreach (string axis in new[] { "shiftLong", "shiftLat", "shiftVert" }) Assert.AreEqual(1.7f, CameraType.GetField(axis).GetValue(onboard));
                    CameraType.GetField("shiftLong").SetValue(onboard, 0.4f);
                    CameraType.GetField("shiftLat").SetValue(onboard, 1.2f);
                    CameraType.GetField("shiftVert").SetValue(onboard, 2.3f);
                    CameraType.GetMethod("Save").Invoke(onboard, null);
                    CameraType.GetMethod("Load").Invoke(onboard, null);
                    Assert.AreEqual(0.4f, CameraType.GetField("shiftLong").GetValue(onboard));
                    Assert.AreEqual(1.2f, CameraType.GetField("shiftLat").GetValue(onboard));
                    Assert.AreEqual(2.3f, CameraType.GetField("shiftVert").GetValue(onboard));
                    Assert.AreEqual(1.7f, PlayerPrefs.GetFloat("ReflexFpv.shiftStrength"), "Legacy key must not override explicit axes");
                }
                finally { UnityEngine.Object.DestroyImmediate(go); }
            }
        }

        [Test]
        public void WreckAndRecoveryDiscardPreviousEyeSpringAndFilterHistory()
        {
            var go = new GameObject("FPV wreck recovery"); go.SetActive(false);
            try
            {
                var onboard = go.AddComponent(CameraType);
                const BindingFlags fields = BindingFlags.Instance | BindingFlags.NonPublic;
                foreach (bool wrecked in new[] { true, false })
                {
                    CameraType.GetField("primed", fields).SetValue(onboard, true);
                    CameraType.GetField("eyePrimed", fields).SetValue(onboard, true);
                    CameraType.GetField("truePrimed", fields).SetValue(onboard, true);
                    CameraType.GetField("eyeVel", fields).SetValue(onboard, Vector3.one * 20);
                    CameraType.GetField("trueVel", fields).SetValue(onboard, Vector3.one * 20);
                    CameraType.GetMethod("SetContext").Invoke(onboard, new object[] { false, false, true, wrecked, false, true, false, false, false, wrecked ? 1d : 2d, false });
                    float elapsed = (float)Invoke(onboard, "UpdateFilters", Vector3.zero, Quaternion.identity, Vector3.forward, Vector3.back, Vector3.zero, null);
                    Assert.AreEqual(0f, elapsed, "Availability transitions must prime the current state instead of integrating old history");
                    Assert.IsFalse((bool)CameraType.GetField("eyePrimed", fields).GetValue(onboard));
                    Assert.IsFalse((bool)CameraType.GetField("truePrimed", fields).GetValue(onboard));
                    Assert.AreEqual(Vector3.zero, CameraType.GetField("eyeVel", fields).GetValue(onboard));
                    Assert.AreEqual(Vector3.zero, CameraType.GetField("trueVel", fields).GetValue(onboard));
                }
            }
            finally { UnityEngine.Object.DestroyImmediate(go); }
        }
    }
}
