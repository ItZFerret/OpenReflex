using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Text;
using UnityEngine;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class AssetImportTests
    {
        // macOS /var is a symlink; production deliberately rejects linked ancestors.
        // Use the isolated test project's real workspace, never the user's install.
        static string FixtureRoot(string name) => Path.Combine(Path.GetFullPath(Path.Combine(Application.dataPath, "..")), "Temp", name + Guid.NewGuid().ToString("N"));
        static Type Importer => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType("Reflex.Port.Content.ReflexAssetImport")).First(t => t != null);
        static string Segment(string text) => (string)Importer.GetMethod("Segment").Invoke(null, new object[] { text });

        [TestCase("CON", "%43ON")]
        [TestCase("nul", "%6Eul")]
        [TestCase("COM1", "%43OM1")]
        [TestCase("lpt9", "%6Cpt9")]
        [TestCase("COM10", "COM10")]
        [TestCase("NAT_RenneB", "NAT_RenneB")]
        [TestCase("../A:B", "%2E%2E%2FA%3AB")]
        public void AssetSegmentsArePortable(string input, string expected) => Assert.AreEqual(expected, Segment(input));

        [Test]
        public void CaseOnlyCollisionFailsButExactDuplicateMayBeComparedByImporter()
        {
            var method = Importer.GetMethod("RegisterPortablePath", BindingFlags.Static | BindingFlags.NonPublic);
            var paths = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            method.Invoke(null, new object[] { paths, "Assets/model/A.model" });
            method.Invoke(null, new object[] { paths, "Assets/model/A.model" });
            var error = Assert.Throws<TargetInvocationException>(() => method.Invoke(null, new object[] { paths, "Assets/model/a.model" }));
            Assert.IsInstanceOf<InvalidDataException>(error.InnerException);
        }

        static void SourceFixture(string root)
        {
            string db = Path.Combine(root, "Database"); Directory.CreateDirectory(db);
            File.WriteAllBytes(Path.Combine(db, "Test.dx9.package"), new byte[] { 7, 8, 9 });
            string[] names = { "Database", "Package", "Asset", "Heap", "name", "file", "type", "offset", "size", "Pkg", "A", "texture" };
            byte[] strings = Encoding.UTF8.GetBytes(string.Join("\0", names) + "\0");
            using var bytes = new MemoryStream(); using var w = new BinaryWriter(bytes);
            w.Write(0x4c4d5842); w.Write(0x3ea); w.Write(names.Length); w.Write(strings.Length); w.Write(8); w.Write(6); w.Write(4); w.Write(0);
            w.Write(strings); w.Write(0); w.Write(3);
            void Attr(int name, int value, ushort count, ushort type) { w.Write(name); w.Write(value); w.Write(count); w.Write(type); }
            Attr(4, 9, 0, 1); Attr(5, 9, 0, 1); Attr(4, 10, 0, 1); Attr(6, 11, 0, 1); Attr(7, 0, 1, 4); Attr(8, 4, 1, 4);
            void Node(int name, int child, int count, int attribute, int attributes) { w.Write(name); w.Write(-1); w.Write(0); w.Write(0); w.Write(child); w.Write(count); w.Write(attribute); w.Write(attributes); }
            Node(0, 1, 1, 0, 0); Node(1, 2, 1, 0, 2); Node(2, 3, 1, 2, 2); Node(3, 0, 0, 4, 2); w.Flush();
            File.WriteAllBytes(Path.Combine(db, "Test.dx9.database"), bytes.ToArray());
            // Package.file is Pkg; this is synthetic data, never a copied game resource.
            File.Move(Path.Combine(db, "Test.dx9.package"), Path.Combine(db, "Pkg.dx9.package"));
        }
        static Task Import(string source, string destination, object progress, CancellationToken cancel) =>
            (Task)Importer.GetMethod("Import").Invoke(null, new object[] { source, destination, progress, cancel });

        [Test]
        public async Task SyntheticPackageCancellationCleansStageAndRetryPublishes()
        {
            string root = FixtureRoot("reflex-import-Å-");
            string source = Path.Combine(root, "source"), destination = Path.Combine(root, "game files");
            SourceFixture(source);
            try
            {
                using var cancel = new CancellationTokenSource();
                Action<object> progress = _ => cancel.Cancel();
                try { await Import(source, destination, progress, cancel.Token); Assert.Fail("Expected cancellation"); } catch (OperationCanceledException) { }
                Assert.IsFalse(Directory.Exists(destination));
                Assert.IsEmpty(Directory.GetDirectories(root, ".reflex-assets-import-*"));
                await Import(source, destination, null, CancellationToken.None);
                CollectionAssert.AreEqual(new byte[] { 7, 8, 9 }, File.ReadAllBytes(Path.Combine(destination, "Assets/texture/Test/Pkg/A.texture")));
                Assert.IsTrue(File.Exists(Path.Combine(destination, "reflex-extracted-v1.txt")));
                Assert.IsFalse(File.Exists(Path.Combine(destination, "Database/Pkg.dx9.package")));
                CollectionAssert.AreEqual(new byte[] { 7, 8, 9 }, File.ReadAllBytes(Path.Combine(source, "Database/Pkg.dx9.package")));
            }
            finally { Directory.Delete(root, true); }
        }

        [TestCase(false)]
        [TestCase(true)]
        public async Task SyntheticPackageConflictDoesNotPublish(bool caseOnly)
        {
            string root = FixtureRoot("reflex-import-conflict-");
            string source = Path.Combine(root, "source"), destination = Path.Combine(root, "game files");
            SourceFixture(source);
            string loose = Path.Combine(source, "Assets/texture/Test/Pkg/" + (caseOnly ? "a" : "A") + ".texture");
            Directory.CreateDirectory(Path.GetDirectoryName(loose)); File.WriteAllBytes(loose, new byte[] { 99 });
            try
            {
                try { await Import(source, destination, null, CancellationToken.None); Assert.Fail("Expected conflict"); } catch (InvalidDataException) { }
                Assert.IsFalse(Directory.Exists(destination)); Assert.IsEmpty(Directory.GetDirectories(root, ".reflex-assets-import-*"));
                CollectionAssert.AreEqual(new byte[] { 99 }, File.ReadAllBytes(loose));
            }
            finally { Directory.Delete(root, true); }
        }

        [Test]
        public async Task PreCancelledImportDoesNotPublishOrCreateStagingFolder()
        {
            string root = FixtureRoot("reflex-import-test-");
            Directory.CreateDirectory(root);
            try
            {
                using var cancel = new CancellationTokenSource(); cancel.Cancel();
                var task = (Task)Importer.GetMethod("Import").Invoke(null,
                    new object[] { Path.Combine(root, "source"), Path.Combine(root, "game files"), null, cancel.Token });
                try { await task; Assert.Fail("Expected cancellation"); } catch (OperationCanceledException) { }
                Assert.IsFalse(Directory.Exists(Path.Combine(root, "game files")));
                Assert.IsEmpty(Directory.GetFileSystemEntries(root));
            }
            finally { Directory.Delete(root, true); }
        }
    }
}
