using System;
using System.IO;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class SkyProgramTests
    {
        static Type ContentType => AppDomain.CurrentDomain.GetAssemblies()
            .Select(a => a.GetType("Reflex.Port.Content.ReflexLevelContent")).FirstOrDefault(t => t != null);

        static byte[] Resource(string level)
        {
            // Original resources remain in the configured install, never in test fixtures / Assets.
            string install = Environment.GetEnvironmentVariable("REFLEX_GAME_DIR");
            if (string.IsNullOrEmpty(install)) Assert.Ignore("Set REFLEX_GAME_DIR to the existing extracted install for original sky contracts");
            string path = Path.Combine(install, "Assets", "shader", level, level + "Base", "SkyEntityComp.shader");
            Assert.IsTrue(File.Exists(path), "Original package-local sky resource: " + path);
            return File.ReadAllBytes(path);
        }

        static object Parse(byte[] resource)
        {
            Assert.IsNotNull(ContentType);
            var method = ContentType.GetMethod("ParseShaderInfo", BindingFlags.Static | BindingFlags.NonPublic);
            var info = method.Invoke(null, new object[] { "SkyEntityComp", resource });
            return info.GetType().GetField("skyProgram").GetValue(info);
        }

        static Vector4 Vector(object program, string field) => (Vector4)program.GetType().GetField(field).GetValue(program);
        static void Near(float actual, float expected) => Assert.That(actual, Is.EqualTo(expected).Within(0.000001f));

        [TestCase("NAT_DLC05_Dalton", .04f, .04f, .5f, .2f, .25f, .25f, 100f, 2f)]
        [TestCase("NAT_DLC06_DaltonB", .01f, .01f, .12f, .2f, .2f, .2f, 75f, 2f)]
        [TestCase("SX_DLC01_FreightYard", .03f, .03f, .03f, .1f, .1f, .1f, .6f, .5f)]
        [TestCase("SX_DLC02_FreightYardB", .02f, .02f, .2f, .2f, .2f, .2f, 40f, .2f)]
        public void SkyConstantsComeFromEachOriginalPackage(string level, float sx, float sy, float sz,
            float bx, float by, float bz, float lumScale, float lumBias)
        {
            var program = Parse(Resource(level));
            Assert.IsNotNull(program, "Recognized original SkyEntityComp program in " + level);
            var scale = Vector(program, "saturationScale"); var bias = Vector(program, "saturationBias");
            var lum = Vector(program, "luminance");
            Near(scale.x, sx); Near(scale.y, sy); Near(scale.z, sz);
            Near(bias.x, bx); Near(bias.y, by); Near(bias.z, bz);
            Near(lum.x, lumScale); Near(lum.y, lumBias);
        }

        [Test]
        public void SameShaderNameDoesNotReuseAnotherPackageConstants()
        {
            var freight = Parse(Resource("SX_DLC01_FreightYard"));
            var dalton = Parse(Resource("NAT_DLC05_Dalton"));
            var freightAgain = Parse(Resource("SX_DLC01_FreightYard"));
            Near(Vector(freight, "luminance").x, .6f);
            Near(Vector(dalton, "luminance").x, 100f);
            Near(Vector(freightAgain, "luminance").x, .6f);
        }

        static object Decode(byte[] resource)
        {
            var type = ContentType.Assembly.GetType("Reflex.Port.Content.ReflexSkyProgram");
            return type.GetMethod("Read", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new object[] { resource });
        }

        static int Operand(byte[] resource, int opcode, int occurrence, int operand)
        {
            int at = 8;
            for (int entry = 0; entry < BitConverter.ToInt32(resource, 4); entry++)
            {
                int kind = BitConverter.ToInt32(resource, at), nameBytes = BitConverter.ToInt32(resource, at + 4);
                at += 8 + nameBytes;
                int size = BitConverter.ToInt32(resource, at); at += 4;
                if (kind == 1)
                    for (int p = at + 4; p < at + size;)
                    {
                        uint token = BitConverter.ToUInt32(resource, p); int op = (int)(token & 65535);
                        if (op == 65535) break;
                        if (op == opcode && occurrence-- == 0) return p + operand * 4;
                        int args = op == 65534 ? (int)((token >> 16) & 32767) : (int)((token >> 24) & 15);
                        p += (args + 1) * 4;
                    }
                at += size;
            }
            Assert.Fail("Expected original instruction not found"); return -1;
        }

        [TestCase(66, 0, 2, 0, 0x40)] // TEXLD uses a different coordinate register.
        [TestCase(4, 0, 2, 0, 0x40)]  // Saturation MAD uses another temporary.
        [TestCase(5, 0, 2, 0, 0x40)]  // Final MUL uses another temporary.
        [TestCase(1, 0, 2, 0, 0x40)]  // Alpha MOV refers to an undefined literal register.
        [TestCase(4, 0, 2, 2, 1)] // MAD input swizzle differs even with the same temporary.
        public void ChangedProgramSourcesAreRejected(int opcode, int occurrence, int operand, int byteIndex, int mask)
        {
            var resource = Resource("SX_DLC01_FreightYard");
            int at = Operand(resource, opcode, occurrence, operand);
            resource[at + byteIndex] ^= (byte)mask;
            Assert.IsNull(Decode(resource));
        }

        [Test]
        public void MalformedResourceLengthsAreRejected()
        {
            var resource = Resource("SX_DLC01_FreightYard");
            Array.Resize(ref resource, resource.Length - 1);
            Assert.IsNull(Decode(resource));
            resource = Resource("SX_DLC01_FreightYard");
            Buffer.BlockCopy(BitConverter.GetBytes(int.MaxValue), 0, resource, 12, 4);
            Assert.IsNull(Decode(resource));
        }
    }
}
