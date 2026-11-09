using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    public static class RxCareerCheckpointNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReceiptFn(IntPtr session, ulong generation, ulong id);
        static readonly RxNativeLoader.Fn<ReceiptFn> committed = new RxNativeLoader.Fn<ReceiptFn>("rx_session_career_checkpoint_committed");
        static readonly RxNativeLoader.Fn<ReceiptFn> failed = new RxNativeLoader.Fn<ReceiptFn>("rx_session_career_checkpoint_failed");
        public static RxCareerResult Committed(IntPtr session, ulong generation, ulong id) => (RxCareerResult)committed.Get()(session, generation, id);
        public static RxCareerResult Failed(IntPtr session, ulong generation, ulong id) => (RxCareerResult)failed.Get()(session, generation, id);
    }
}
