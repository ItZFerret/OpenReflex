using System;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // The garage's TEST DRIVE (core/include/rx_test_drive.h, w-testride): the original rides it in the front end's own world UI_Compound (the
    // MXUIWorld every session builds) with its FreeRide activity: rx_session_test_drive_start instead of rx_session_event_load (EnableTestDrive,
    // SyncDatabaseWithManagers, MXGarage_TestDriveStart), the world's own frames spawn the profile's equipped vehicle and rider;
    // rx_session_test_drive_end is the pause request's end (EnableTestDrive( FALSE ), MXGarage_TestDriveEnd).
    [StructLayout(LayoutKind.Sequential)]
    public struct RxTestDriveState
    {
        public uint size, version;
        public int active, spawned, vehicle_type, enabled, players_idle;
        public uint frames;
        public static RxTestDriveState Create() => new RxTestDriveState { size = 32, version = 1 };
    }

    /// The profile of a test drive: imported as for a ride (the garage's selection applied), never written back (the session ends unsettled
    /// on purpose: no record, no checkpoint, no "not saved" notice).
    public sealed class TestDrivePersistence : IPlayerSessionPersistence
    {
        readonly IPlayerSessionPersistence inner;
        public TestDrivePersistence(IPlayerSessionPersistence inner) { this.inner = inner; }
        public byte[] Prepare(byte[] nativeDefaults) => inner.Prepare(nativeDefaults);
        public void Imported(byte[] validatedSnapshot) => inner.Imported(validatedSnapshot);
        public void Complete(byte[] settledSnapshot) { }
        public void Failed(string error) { }
    }

    /// The test-ride arena while an event loads (w-loadarena: the test drive in the core's second image, beside the event's own session):
    /// the profile is read as for a test ride, and nothing at all is written, not even the first import's snapshot file (the event's
    /// session, starting at the same time, is the one that creates it: two first imports would race for it).
    public sealed class ArenaRidePersistence : IPlayerSessionPersistence
    {
        readonly IPlayerSessionPersistence inner;
        public ArenaRidePersistence(IPlayerSessionPersistence inner) { this.inner = inner; }
        public byte[] Prepare(byte[] nativeDefaults) => inner.Prepare(nativeDefaults);
        public void Imported(byte[] validatedSnapshot) { }
        public void Complete(byte[] settledSnapshot) { }
        public void Failed(string error) { }
    }

    internal static class RxTestDriveNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StartFn(IntPtr session, ref RxEventSpec spec);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int EndFn(IntPtr session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, ref RxTestDriveState state);
        static readonly RxNativeLoader.Fn<StartFn> start = new RxNativeLoader.Fn<StartFn>("rx_session_test_drive_start");
        static readonly RxNativeLoader.Fn<EndFn> end = new RxNativeLoader.Fn<EndFn>("rx_session_test_drive_end");
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_test_drive");
        /// Instead of rx_session_event_load, on a fresh session (after the profile / player import, the remap and the UI settings).
        internal static int Start(IntPtr session, ref RxEventSpec spec) => start.Get()(session, ref spec);
        internal static int End(IntPtr session) => end.Get()(session);
        internal static int Read(IntPtr session, ref RxTestDriveState state) => read.Get()(session, ref state);
    }
}
