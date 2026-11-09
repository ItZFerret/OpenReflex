using System;
using System.Runtime.InteropServices;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class RxPropLayoutTests
    {
        [Test]
        public void PropSnapshotMatchesNativeLayout()
        {
            Assert.AreEqual(80, Marshal.SizeOf<RxProp>());
            Assert.AreEqual(0, Marshal.OffsetOf<RxProp>(nameof(RxProp.vtable)).ToInt32());
            Assert.AreEqual(4, Marshal.OffsetOf<RxProp>(nameof(RxProp.entity_id)).ToInt32());
            Assert.AreEqual(8, Marshal.OffsetOf<RxProp>(nameof(RxProp.kind)).ToInt32());
            Assert.AreEqual(12, Marshal.OffsetOf<RxProp>(nameof(RxProp.flags)).ToInt32());
            Assert.AreEqual(16, Marshal.OffsetOf<RxProp>(nameof(RxProp.matrix)).ToInt32());
        }

        [Test]
        public unsafe void PropMatrixCopiesAllSixteenLanes()
        {
            RxProp prop = default;
            for (int i = 0; i < 16; i++) prop.matrix[i] = i * 0.25f - 2;
            var output = new float[16];
            prop.CopyMatrix(output);
            for (int i = 0; i < 16; i++) Assert.AreEqual(i * 0.25f - 2, output[i]);
        }
    }
}
