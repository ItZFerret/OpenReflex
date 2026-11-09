using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    // The bridge's error contract and (optionally) the real core. The native tests only run when REFLEX_RXCORE_PATH points at a
    // librxcore.dylib built from this tree (nothing is installed into the project; the editor maps the library once loaded).
    public class OpenReflexSimulationTests
    {
        [Test]
        public void BadInstallFailsWithAMessageInsteadOfThrowing()
        {
            // With no library loaded the worker reports DllNotFound as a failure; with one loaded, rx_session_create("") fails.
            using (var sim = new OpenReflexSimulation("", new RxEventSpec(), false))
            {
                Assert.IsFalse(sim.WaitReady(30000));
                Assert.AreEqual(OpenReflexSimulation.Phase.Failed, sim.CurrentPhase);
                Assert.IsNotEmpty(sim.Failure);
                sim.Submit(1f / 60f, new byte[256], new RxPad[4], 0);
                Assert.IsTrue(sim.WaitIdle(30000));
                var r = sim.Collect();
                Assert.IsNotNull(r);
                Assert.IsNotEmpty(r.error);
                Assert.AreEqual(RxSessionState.Error, r.sessionState);
            }
        }

        [Test]
        public void NativeAbiMatches()
        {
            string path = RxNativeLoader.ResolvePath(null);
            if (path == null) Assert.Ignore("REFLEX_RXCORE_PATH is not set");
            RxNativeLoader.Load(path);
            var check = RxApiNative.Check();
            Assert.IsTrue(check.Compatible, check.Summary);
        }
    }
}
