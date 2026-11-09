using System;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore {
 [StructLayout(LayoutKind.Sequential)]
 public struct RxPowercrossHudState {
  public uint size,version,valid;
  public int activity_type,activity_mode,activity_state,next_countdown_step,board_present;
  public float activity_clock;
  public uint component_bound,visible;
  public int displayed_step;
  public uint exited;
  public float hold_remaining;
  public uint event_count;
  public static RxPowercrossHudState Create()=>new RxPowercrossHudState{size=60,version=2};
 }
 internal static class RxPowercrossHudNative {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int ReadFn(IntPtr session,ref RxPowercrossHudState value);
  static readonly RxNativeLoader.Fn<ReadFn> read=new RxNativeLoader.Fn<ReadFn>("rx_session_powercross_hud");
  internal static int Read(IntPtr s,ref RxPowercrossHudState v)=>read.Get()(s,ref v);
 }
}
