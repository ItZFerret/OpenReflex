using System;
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.Content
{
    // SkyEntityComp is a shared asset NAME, not one program: each DLC level package has different DEF literals.
    // Read the two literal MADs from the actual package-resolved ps_3_0 resource, preserving their source swizzles.
    internal sealed class ReflexSkyProgram
    {
        public Vector4 saturationScale, saturationBias, luminance;

        [Serializable]
        sealed class CompositeFooter { public int type; public float[] minimums, ranges; public string companion; }

        internal static ReflexSkyProgram ReadAliveComposite(string path)
        {
            var footer = JsonUtility.FromJson<CompositeFooter>(RxFiles.ReadAllText(path));   // (packed with its level: RxFiles)
            if (footer == null || footer.type != 1 || footer.minimums?.Length != 4 || footer.ranges?.Length != 4 ||
                footer.companion == null || !footer.companion.EndsWith("_Lum.texture", StringComparison.OrdinalIgnoreCase))
                throw new System.IO.InvalidDataException("Invalid Alive HDR composite footer");
            for (int i = 0; i < 4; i++)
                if (!float.IsFinite(footer.minimums[i]) || !float.IsFinite(footer.ranges[i]) || footer.ranges[i] < 0)
                    throw new System.IO.InvalidDataException("Invalid Alive HDR range");
            return new ReflexSkyProgram {
                saturationScale = new Vector4(footer.ranges[0], footer.ranges[1], footer.ranges[2], 0),
                saturationBias = new Vector4(footer.minimums[0], footer.minimums[1], footer.minimums[2], 0),
                luminance = new Vector4(footer.ranges[3], footer.minimums[3], 0, 0),
            };
        }

        internal static ReflexSkyProgram Read(byte[] resource)
        {
            if (resource == null || resource.Length < 8 || BitConverter.ToInt32(resource, 0) != 2) return null;
            int count = BitConverter.ToInt32(resource, 4), at = 8;
            if (count < 1 || count > 64) return null;
            for (int i = 0; i < count; i++)
            {
                if (at > resource.Length - 8) return null;
                int kind = BitConverter.ToInt32(resource, at), nameBytes = BitConverter.ToInt32(resource, at + 4); at += 8;
                if (nameBytes < 1 || nameBytes > 256 || nameBytes > resource.Length - at - 4) return null;
                at += nameBytes;
                int size = BitConverter.ToInt32(resource, at); at += 4;
                if (size < 4 || size % 4 != 0 || size > resource.Length - at) return null;
                if (kind == 1) return ReadPixel(resource, at, size);
                at += size;
            }
            return null;
        }

        static int RegisterType(uint token) => (int)(((token >> 28) & 7) | ((token >> 8) & 24));
        static bool Register(uint token, int type, int index) => RegisterType(token) == type && (token & 2047) == index;
        static bool Source(uint token, int type, int index, uint swizzle) => Register(token, type, index) &&
            (token & 0x0f002000u) == 0 && ((token >> 16) & 255) == swizzle;
        static bool Destination(uint token, int type, int index, uint mask) => Register(token, type, index) &&
            ((token >> 16) & 15) == mask && (token & 0x0fd02000u) == 0; // allow only the partial-precision destination modifier
        static bool Literal(uint source, Dictionary<int, Vector4> definitions, out Vector4 value)
        {
            value = default;
            if (RegisterType(source) != 2 || (source & 0x0f002000u) != 0 || !definitions.TryGetValue((int)(source & 2047), out var raw)) return false;
            for (int lane = 0; lane < 4; lane++)
            {
                float component = raw[(int)((source >> (16 + 2 * lane)) & 3)];
                if (float.IsNaN(component) || float.IsInfinity(component)) return false;
                value[lane] = component;
            }
            return true;
        }

        static ReflexSkyProgram ReadPixel(byte[] b, int start, int bytes)
        {
            uint U(int p) => BitConverter.ToUInt32(b, p);
            if (U(start) != 0xffff0300u) return null;
            var definitions = new Dictionary<int, Vector4>();
            var result = new ReflexSkyProgram();
            int stage = 0, end = start + bytes;
            for (int at = start + 4; at + 4 <= end;)
            {
                uint instruction = U(at); int op = (int)(instruction & 65535);
                if (op == 65535) return stage == 6 ? result : null;
                int args = op == 65534 ? (int)((instruction >> 16) & 32767) : (int)((instruction >> 24) & 15);
                if (args < 1 || args > (end - at - 4) / 4 || (op != 65534 && (instruction & 0xf0ff0000u) != 0)) return null;
                if (op == 81 && args == 5 && stage == 0 && Destination(U(at + 4), 2, (int)(U(at + 4) & 2047), 15))
                    definitions[(int)(U(at + 4) & 2047)] = new Vector4(BitConverter.ToSingle(b, at + 8), BitConverter.ToSingle(b, at + 12), BitConverter.ToSingle(b, at + 16), BitConverter.ToSingle(b, at + 20));
                else if (op == 65534 || (op == 31 && stage == 0)) { } // comments / declarations
                else if ((stage == 0 || stage == 2) && op == 66 && args == 3 &&
                         Destination(U(at + 4), 0, stage == 0 ? 0 : 1, 15) && Source(U(at + 8), 1, 0, 0xe4) &&
                         Source(U(at + 12), 10, stage == 0 ? 1 : 0, 0xe4)) stage++;
                else if ((stage == 1 || stage == 3) && op == 4 && args == 4 && Destination(U(at + 4), 0, 0, stage == 1 ? 7u : 8u) &&
                         Source(U(at + 8), 0, stage == 1 ? 0 : 1, stage == 1 ? 0xe4u : 0u) &&
                         Literal(U(at + 12), definitions, out var scale) && Literal(U(at + 16), definitions, out var bias))
                {
                    if (stage == 1) { result.saturationScale = scale; result.saturationBias = bias; }
                    else result.luminance = new Vector4(scale.w, bias.w, 0, 0);
                    stage++;
                }
                else if (stage == 4 && op == 5 && args == 3 && Destination(U(at + 4), 8, 0, 7) &&
                         Source(U(at + 8), 0, 0, 0xff) && Source(U(at + 12), 0, 0, 0xe4)) stage++;
                else if (stage == 5 && op == 1 && args == 2 && Destination(U(at + 4), 8, 0, 8) &&
                         Literal(U(at + 8), definitions, out var alpha) && alpha.w == 1f) stage++;
                else return null; // Do not silently apply this translation to a different same-named program.
                at += (args + 1) * 4;
            }
            return null;
        }
    }
}
