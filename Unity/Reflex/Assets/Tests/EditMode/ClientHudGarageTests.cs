using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class ClientHudGarageTests
    {
        static Type RuntimeType(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        [TestCase(false, false, true, true, 0)]
        [TestCase(false, true, true, true, 1)]
        [TestCase(false, true, false, true, 1)]
        [TestCase(false, true, true, false, 0)]
        [TestCase(true, false, true, true, 2)]
        [TestCase(true, true, true, true, 2)]
        [TestCase(true, true, true, false, 0)]
        [TestCase(true, true, false, true, 0)]
        public void SpeedometerSupplementDoesNotReplaceClassicOrShowThroughMenus(bool modern, bool supplement, bool lapRace, bool unobstructed, int expected)
        {
            var method = RuntimeType("Reflex.Port.Hud.ReflexModernHud").GetMethod("Presentation", BindingFlags.Static | BindingFlags.NonPublic);
            Assert.AreEqual(expected, method.Invoke(null, new object[] { modern, supplement, lapRace, unobstructed }));
        }

        [TestCase(40, 40, 20, 20, 1, true, 25, 15)]
        [TestCase(40, 40, 20, 20, -1, true, 15, 25)]
        [TestCase(80, 40, 20, 20, 1, false, 20, 20)]
        [TestCase(40, 80, 20, 20, -1, false, 20, 20)]
        [TestCase(40, 40, 0, 20, -1, false, 0, 20)]
        [TestCase(40, 40, 20, 0, 1, false, 20, 0)]
        [TestCase(40, 40, 0, 0, 1, false, 0, 0)]
        [TestCase(40, 40, 20, 20, 0, false, 20, 20)]
        public void GarageTuningPreservesEquippedPartPointsAndBaseLimits(int b1, int b2, int one, int two, int dir, bool changed, int out1, int out2)
        {
            var method = RuntimeType("Reflex.Port.OpenFrontEnd.OpenShell").GetMethod("MoveGarageTuning", BindingFlags.Static | BindingFlags.NonPublic);
            object[] args = { b1, b2, one, two, dir, 0, 0 };
            Assert.AreEqual(changed, method.Invoke(null, args));
            Assert.AreEqual(out1, args[5]); Assert.AreEqual(out2, args[6]);
            Assert.AreEqual(one + two, (int)args[5] + (int)args[6]);
        }
    }
}
