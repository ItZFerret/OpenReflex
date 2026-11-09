using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // The game's random number streams (core/include/rx_rng.h, 64 bytes, version 1; w-rng): msvcr90 rand()'s per-thread seeds (main, the
    // level loader thread), the game's own generator, and the ocean spectrum's draws (the loader stream's seed before Ocean_InitSpectrum's
    // first draw: ReflexWater.Ocean.Generate(oceanSeed) is then the game's spectrum).
    public struct RxRngState
    {
        public const int Size = 64;
        public bool valid; public uint crtMain, crtLoader, gameSeed, gameCount; public bool oceanValid; public uint oceanSeed, oceanDraws;
    }

    public static class RxRngNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, byte[] value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_rng");
        /// rx_session_rng (between frames, the session pointer BetweenFrames hands out). Throws EntryPointNotFoundException on an older core.
        public static bool Read(IntPtr session, out RxRngState state)
        {
            var b = new byte[RxRngState.Size];
            b[0] = RxRngState.Size; b[4] = 1;
            state = default;
            if (read.Get()(session, b) != 0) return false;
            uint U(int o) => BitConverter.ToUInt32(b, o);
            state.valid = U(8) != 0; state.crtMain = U(12); state.crtLoader = U(16); state.gameSeed = U(20); state.gameCount = U(24);
            state.oceanValid = U(28) != 0; state.oceanSeed = U(32); state.oceanDraws = U(36);
            return true;
        }
    }
}
