using System;
using System.IO;
using System.Runtime.InteropServices;
using NUnit.Framework;
using Reflex.Port.RxCore;

namespace Reflex.Port.RxCore.Tests
{
    public class RxNativeLoaderPathTests
    {
        [Test]
        public void NativeBundleUsesHostPlatformFileAndDirectory()
        {
            bool windows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
            Assert.AreEqual(windows ? "rxcore.dll" : "librxcore.dylib", RxNativeLoader.FileName);
            string root = Path.Combine(Path.GetTempPath(), "Reflex loader path");
            Assert.AreEqual(Path.Combine(root, windows ? "Plugins/x86_64" : "PlugIns"), RxNativeLoader.PluginDirectory(root, false));
            Assert.AreEqual(Path.Combine(root, windows ? "Plugins/x86_64" : "Plugins/macOS"), RxNativeLoader.PluginDirectory(root, true));
        }
        [Test]
        public void ExplicitMissingLibraryDoesNotSilentlyUseBundledCore()
        {
            string previous = Environment.GetEnvironmentVariable(RxNativeLoader.EnvPath);
            string root = Path.Combine(Path.GetTempPath(), "Reflex-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                string bundled = Path.Combine(root, RxNativeLoader.FileName); File.WriteAllBytes(bundled, Array.Empty<byte>());
                Environment.SetEnvironmentVariable(RxNativeLoader.EnvPath, Path.Combine(root, "missing-core"));
                Assert.IsNull(RxNativeLoader.ResolvePath(root));
                Environment.SetEnvironmentVariable(RxNativeLoader.EnvPath, null);
                Assert.AreEqual(bundled, RxNativeLoader.ResolvePath(root));
                Environment.SetEnvironmentVariable(RxNativeLoader.EnvPath, bundled);
                Assert.AreEqual(bundled, RxNativeLoader.ResolvePath(null));
            }
            finally { Environment.SetEnvironmentVariable(RxNativeLoader.EnvPath, previous); Directory.Delete(root, true); }
        }
    }
}
