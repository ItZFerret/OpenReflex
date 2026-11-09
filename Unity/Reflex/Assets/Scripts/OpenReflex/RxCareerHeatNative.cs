// The Heat format's boundary (core/include/rx_career_heat.h, version 1): Series rows 41-46, MXSeriesEventHeat. A Heat event is up to
// four races in one world: the practice qualifier, the heat, the LCQ and the main event; every race starts with the gate selection and
// between the rounds the core re-creates the activity area behind the AAA loading page ("Select Activity Area").
using System;
using System.Linq;
using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Ansi)]
 public struct RxHeatRow { public int row,player_type,player_local,player_state,requested_state;public float sort_value;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string name; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Ansi)]
 public struct RxHeatResult { public int rank,player_type,dnf,requested_state;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)]public string name; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Ansi)]
 public struct RxHeatStatus {
  public uint size,version;public ulong generation;
  public uint series_id,sub_event,phase,next_phase,post_complete,advance_pending,round_continue_submitted,round_post_observed,gate_active,gate_cursor,gate_count;
  public int device;public uint activity_area_required,row_count;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string event_status;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string routing_message;
  [MarshalAs(UnmanagedType.ByValArray,SizeConst=16)]public int[] gates;
  [MarshalAs(UnmanagedType.ByValArray,SizeConst=24)]public RxHeatRow[] rows;
  public uint result_count;
  [MarshalAs(UnmanagedType.ByValArray,SizeConst=16)]public RxHeatResult[] results;
  public const int ByteSize=5136;
  public static RxHeatStatus Create()=>new RxHeatStatus{size=(uint)Marshal.SizeOf(typeof(RxHeatStatus)),version=1,gates=new int[16],rows=new RxHeatRow[24],results=new RxHeatResult[16]};
 }
 public enum RxHeatAction : uint { GateBegin=1, GateSelect=2, Next=3, RoundContinue=4 }
 public sealed class HeatResultRow {
  public readonly int Rank,PlayerType;public readonly bool Dnf;public readonly int RequestedState;public readonly string Name;
  internal HeatResultRow(RxHeatResult r){Rank=r.rank;PlayerType=r.player_type;Dnf=r.dnf!=0;RequestedState=r.requested_state;Name=r.name??"";}
  public bool Local=>PlayerType==0;
  /// Where the round sent the rider (RequestedPlayerState): 8 the main event, 6 / 7 the LCQ, 9 / 10 out.
  public string Outcome=>RequestedState==8?"MAIN EVENT":RequestedState==6||RequestedState==7?"LCQ":RequestedState==9||RequestedState==10?"OUT":"";
 }
 // Immutable copy of one rx_session_career_heat_status.
 public sealed class HeatWorkerSnapshot {
  public readonly ulong Generation;public readonly uint Phase,NextPhase,SubEvent,Cursor;
  public readonly bool Complete,GateActive,RoundContinued,RoundPostObserved,ActivityAreaRequired;public readonly int Device;
  public readonly string EventStatus,RoutingMessage;
  public readonly System.Collections.Generic.IReadOnlyList<int> Gates;
  public readonly System.Collections.Generic.IReadOnlyList<HeatResultRow> Results;
  internal HeatWorkerSnapshot(RxHeatStatus v,ulong generation){
   if(v.size!=RxHeatStatus.ByteSize||v.version!=1||v.generation!=generation||v.series_id==0||v.row_count>24||v.gate_count>16||v.gate_cursor>24||v.result_count>16||v.gates==null||v.rows==null||v.results==null)
    throw new InvalidOperationException("Heat copied boundary identity differs.");
   Generation=generation;Phase=v.phase;NextPhase=v.next_phase;SubEvent=v.sub_event;Cursor=v.gate_cursor;Complete=v.post_complete==1;GateActive=v.gate_active==1;
   RoundContinued=v.round_continue_submitted==1;RoundPostObserved=v.round_post_observed==1;ActivityAreaRequired=v.activity_area_required==1;Device=v.device;
   EventStatus=v.event_status??"";RoutingMessage=v.routing_message??"";
   Gates=Array.AsReadOnly(v.gates.Take((int)v.gate_count).Where(g=>g>=0&&g!=int.MaxValue).ToArray());
   Results=Array.AsReadOnly(v.results.Take((int)v.result_count).Select(r=>new HeatResultRow(r)).ToArray());
  }
  /// The round's results page after a heat / LCQ ("HeatOrLCQ", routing GotoPostEvent).
  public bool RoundResults=>EventStatus=="HeatOrLCQ"&&(Phase==3||Phase==4)&&!GateActive&&RoutingMessage=="GotoPostEvent";
  /// The next race's gate page (routing "Goto GateSelection") before the gates were opened.
  public bool GateReady=>!GateActive&&Gates.Count>0&&RoutingMessage=="Goto GateSelection"&&(NextPhase==3||NextPhase==4||NextPhase==5);
  public static string RoundName(uint phase)=>phase==1?"PRACTICE":phase==3?"HEAT":phase==4?"LAST CHANCE QUALIFIER":phase==5?"MAIN EVENT":"HEAT RACING";
  public bool Same(HeatWorkerSnapshot o)=>o!=null&&o.Generation==Generation&&o.Phase==Phase&&o.NextPhase==NextPhase&&o.SubEvent==SubEvent&&o.Cursor==Cursor&&o.GateActive==GateActive&&
   o.Device==Device&&o.Complete==Complete&&o.RoundContinued==RoundContinued&&o.RoundPostObserved==RoundPostObserved&&o.ActivityAreaRequired==ActivityAreaRequired&&
   o.EventStatus==EventStatus&&o.RoutingMessage==RoutingMessage&&o.Gates.Count==Gates.Count&&o.Results.Count==Results.Count;
 }
 internal interface ICareerHeatWorkerNative {RxCareerResult HeatStatus(IntPtr s,ulong g,ref RxHeatStatus value);RxCareerResult HeatAction(IntPtr s,ulong g,ulong hub,uint action,uint phase,uint next,uint cursor,int gate);}
 internal static class RxCareerHeatNative {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int StatusFn(IntPtr s,ulong g,ref RxHeatStatus v);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int ActionFn(IntPtr s,ulong g,ulong hub,uint action,uint phase,uint next,uint cursor,int gate);
  static readonly RxNativeLoader.Fn<StatusFn> status=new RxNativeLoader.Fn<StatusFn>("rx_session_career_heat_status");
  static readonly RxNativeLoader.Fn<ActionFn> action=new RxNativeLoader.Fn<ActionFn>("rx_session_career_heat_action");
  internal static RxCareerResult Status(IntPtr s,ulong g,ref RxHeatStatus v)=>(RxCareerResult)status.Get()(s,g,ref v);
  internal static RxCareerResult Action(IntPtr s,ulong g,ulong hub,uint a,uint p,uint next,uint c,int gate)=>(RxCareerResult)action.Get()(s,g,hub,a,p,next,c,gate);
 }
}
