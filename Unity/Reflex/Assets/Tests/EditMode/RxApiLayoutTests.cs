using System;
using System.Runtime.InteropServices;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    // The C# structs against the C layout: sizes and every field offset of core/include/rx_api.h as printed by
    // builds/unity/rx_api_layout.c (RxApiLayoutExpected.cs is generated from it). No native library needed.
    public class RxApiLayoutTests
    {
        [Test]
        public void ApiVersionMatchesTheHeader() => Assert.AreEqual(RxApiLayoutExpected.ApiVersion, RxApiNative.ExpectedApiVersion);

        [Test]
        public void StructSizesMatchC()
        {
            foreach (var (name, size) in RxApiLayoutExpected.Sizes)
            {
                var type = RxApiLayout.TypeOf(name);
                Assert.IsNotNull(type, "no managed type for " + name);
                Assert.AreEqual(size, Marshal.SizeOf(type), name);
            }
        }

        [Test]
        public void EveryStructIsCovered()
        {
            foreach (var (name, _) in RxApiLayout.Structs)
                Assert.IsTrue(Array.Exists(RxApiLayoutExpected.Sizes, e => e.name == name), "the C layout program does not print " + name);
        }

        [Test]
        public void FieldOffsetsMatchC()
        {
            foreach (var (field, offset) in RxApiLayoutExpected.Offsets)
            {
                var parts = field.Split('.');
                var type = RxApiLayout.TypeOf(parts[0]);
                Assert.IsNotNull(type, "no managed type for " + parts[0]);
                Assert.AreEqual(offset, (int)Marshal.OffsetOf(type, parts[1]), field);
            }
        }

        [Test]
        public void EventSpecStringsRoundTrip()
        {
            var spec = new RxEventSpec();
            spec.Location = "NAT_Concord"; spec.EventType = "National"; spec.PlayerName = "Player 1";
            Assert.AreEqual("NAT_Concord", spec.Location);
            Assert.AreEqual("National", spec.EventType);
            Assert.AreEqual("Player 1", spec.PlayerName);
            spec.Location = new string('x', 200);   // truncated to the 64-byte field, still terminated
            Assert.AreEqual(63, spec.Location.Length);
        }
    }
}
