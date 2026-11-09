using System;
using System.Linq;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Ansi)]
 public struct RxMotoRow { public int row,player_type,player_state,score_one,score_two;public float sort_value;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string name; }
 [StructLayout(LayoutKind.Sequential)]
 public struct RxMotoStatus {
  public uint size,version;public ulong generation;public uint series_id,sub_event,phase,next_phase,post_complete,row_count,gate_active,gate_cursor,gate_count;public int device;public uint first_continue_submitted,first_post_observed;
  [MarshalAs(UnmanagedType.ByValArray,SizeConst=16)]public int[] gates;
  [MarshalAs(UnmanagedType.ByValArray,SizeConst=12)]public RxMotoRow[] rows;
  public static RxMotoStatus Create()=>new RxMotoStatus{size=(uint)Marshal.SizeOf(typeof(RxMotoStatus)),version=2,gates=new int[16],rows=new RxMotoRow[12]};
 }
 // Immutable copy: arrays in the native marshalling value never escape the worker.
 public sealed class MotoWorkerSnapshot {
  public readonly ulong Generation;public readonly uint Phase,NextPhase,SubEvent,Cursor;public readonly bool Complete,GateActive,FirstContinued,FirstPostObserved;public readonly int Device;
  public readonly System.Collections.Generic.IReadOnlyList<int> Gates;
  public readonly MotoScoredObservation Scores;
  internal MotoWorkerSnapshot(RxMotoStatus v,ulong generation){
   if(v.size!=RxMotoStatus.Create().size||v.version!=2||v.generation!=generation||v.series_id==0||v.row_count!=12||v.gate_count>16||v.gate_cursor>12||v.rows==null||v.rows.Length!=12||!v.rows.Select(r=>r.row).OrderBy(x=>x).SequenceEqual(Enumerable.Range(0,12)))throw new InvalidOperationException("Moto copied boundary identity differs.");
   FirstContinued=v.first_continue_submitted==1;FirstPostObserved=v.first_post_observed==1;Generation=generation;Phase=v.phase;NextPhase=v.next_phase;SubEvent=v.sub_event;Cursor=v.gate_cursor;Complete=v.post_complete==1;GateActive=v.gate_active==1;Device=v.device;
   Gates=Array.AsReadOnly(v.gates.Take((int)v.gate_count).Where(g=>g>=0&&g!=int.MaxValue).ToArray());
   Scores=new MotoScoredObservation((int)v.phase,(int)v.next_phase,(int)v.sub_event,Complete,v.rows.Select(r=>new MotoScoreRow(r.player_type,r.player_state,r.score_one,r.score_two,r.sort_value,r.name)));
  }
 }
 internal interface ICareerMotoWorkerNative {RxCareerResult MotoStatus(IntPtr s,ulong g,ref RxMotoStatus value);RxCareerResult MotoAction(IntPtr s,ulong g,ulong hub,uint action,uint phase,uint sub,uint cursor,int gate);}
 internal static class RxCareerMotoNative {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int StatusFn(IntPtr s,ulong g,ref RxMotoStatus v);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int ActionFn(IntPtr s,ulong g,ulong hub,uint action,uint phase,uint sub,uint cursor,int gate);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int PracticeFn(IntPtr s,ulong g,uint skip);
  static readonly RxNativeLoader.Fn<PracticeFn> practice=new RxNativeLoader.Fn<PracticeFn>("rx_session_career_moto_practice_choice");
  internal static RxCareerResult PracticeChoice(IntPtr s,ulong g,bool skip)=>(RxCareerResult)practice.Get()(s,g,skip?1u:0u);
  static readonly RxNativeLoader.Fn<StatusFn> status=new RxNativeLoader.Fn<StatusFn>("rx_session_career_moto_status");
  static readonly RxNativeLoader.Fn<ActionFn> action=new RxNativeLoader.Fn<ActionFn>("rx_session_career_moto_action");
  internal static RxCareerResult Status(IntPtr s,ulong g,ref RxMotoStatus v)=>(RxCareerResult)status.Get()(s,g,ref v);
  internal static RxCareerResult Action(IntPtr s,ulong g,ulong hub,uint a,uint p,uint sub,uint c,int gate)=>(RxCareerResult)action.Get()(s,g,hub,a,p,sub,c,gate);
 }
}
