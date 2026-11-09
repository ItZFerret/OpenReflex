using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The water wake's pressure splats (core/include/rx_water.h, 568 bytes, version 1; w-water): the splats the game queued on each frame
    // (Wake_AddSplatQuad 007ec280 / Wake_AddSplat 007ecd80), taken by the session at the render's place as the original's Wake_Step 007ed300
    // takes them. Vertices are 5 floats {x, y, strength, u, v}, centres 2 floats {world x, world z}, one centre per 6 vertices.
    public struct RxWaterSplats
    {
        public const int Size = 568, MaxFrames = 32;
        public uint frames, vertices, centres, primitives, truncated, dropped, views;
        public ulong lastFrame;
        public ulong[] frame; public uint[] frameVertices, frameCentres;
    }

    public static class RxWaterNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, [In, Out] byte[] io, [Out] float[] vertices, uint maxVertices, [Out] float[] centres, uint maxCentres);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_water_splats");
        [ThreadStatic] static byte[] io;
        /// rx_session_water_splats (between frames, the session pointer BetweenFrames hands out): the frames after `afterFrame` into the
        /// buffers (vertices.Length / 5, centres.Length / 2). Reads only. Throws EntryPointNotFoundException on an older core.
        public static bool Read(IntPtr session, ulong afterFrame, float[] vertices, float[] centres, ref RxWaterSplats value)
        {
            io ??= new byte[RxWaterSplats.Size];
            Array.Clear(io, 0, io.Length);
            void Put(int o, ulong v, int n) { for (int i = 0; i < n; i++) io[o + i] = (byte)(v >> (8 * i)); }
            Put(0, RxWaterSplats.Size, 4); Put(4, 1, 4); Put(8, afterFrame, 8);
            if (read.Get()(session, io, vertices, vertices != null ? (uint)(vertices.Length / 5) : 0u, centres, centres != null ? (uint)(centres.Length / 2) : 0u) != 0) return false;
            uint U(int o) => BitConverter.ToUInt32(io, o);
            value.frames = U(16); value.vertices = U(20); value.centres = U(24); value.primitives = U(28);
            value.truncated = U(32); value.dropped = U(36); value.views = U(40); value.lastFrame = BitConverter.ToUInt64(io, 48);
            value.frame ??= new ulong[RxWaterSplats.MaxFrames]; value.frameVertices ??= new uint[RxWaterSplats.MaxFrames]; value.frameCentres ??= new uint[RxWaterSplats.MaxFrames];
            for (int k = 0; k < RxWaterSplats.MaxFrames; k++)
            {
                value.frame[k] = BitConverter.ToUInt64(io, 56 + 8 * k);
                value.frameVertices[k] = U(312 + 4 * k); value.frameCentres[k] = U(440 + 4 * k);
            }
            return true;
        }
    }
}
