using System;
using System.Collections.Generic;
using System.Linq;
namespace Reflex.Port.RxCore {
 // Copies only genuine externally captured rows; this model issues no commands.
 public sealed class MotoScoreRow {
  public readonly int Type,State,ScoreOne,ScoreTwo;public readonly float SortValue;public readonly string Name;
  public MotoScoreRow(int type,int state,int one,int two,float sort,string name){Type=type;State=state;ScoreOne=one;ScoreTwo=two;SortValue=sort;Name=name??throw new ArgumentNullException(nameof(name));}
 }
 public sealed class MotoScoredObservation {
  public readonly int Phase,NextPhase,SubEvent;public readonly bool PostComplete;public readonly IReadOnlyList<MotoScoreRow> Rows;
  public MotoScoredObservation(int phase,int next,int sub,bool complete,IEnumerable<MotoScoreRow> rows){
   Phase=phase;NextPhase=next;SubEvent=sub;PostComplete=complete;var copy=rows.ToArray();
   if(copy.Length!=12||copy.Any(r=>r==null||string.IsNullOrEmpty(r.Name)||float.IsNaN(r.SortValue)||float.IsInfinity(r.SortValue))||copy.Select(Key).Distinct().Count()!=12)throw new InvalidOperationException("Actual unique twelve-rider Moto rows required.");
   Rows=Array.AsReadOnly(copy);
  }
  static string Key(MotoScoreRow row)=>row.Type+"/"+row.Name;
  public static void RequireFirstGenuineMoto(MotoScoredObservation first){
   if(first==null||first.Phase!=6||first.NextPhase!=7||first.SubEvent!=0||first.PostComplete||!first.Rows.Select(r=>r.ScoreOne).OrderBy(x=>x).SequenceEqual(new[]{9,10,11,12,13,14,15,16,18,20,22,25})||first.Rows.Any(r=>r.SortValue!=25-r.ScoreOne))throw new InvalidOperationException("Genuine first Moto scored boundary missing.");
  }
  // The original keeps MXSeriesEventMoto+4 (SubEvent) at 0 for the whole event: the object is the static at 0x9e2410
  // (.bss, zero), MXSeriesManager_Construct 005b2370 / the event setup 005b6950 (vf4/vf5/vf15/vf16) / 005a87b0 never store
  // to +4, and no MXSeriesEvent* method or series-manager function does either; vf10 / vf12 / vf13 only read it.
  // The moto is told by the phase (+0xc): 6 = Moto one, 7 = Moto two (005a9290 copies +0x10 into +0xc; vf13 sets +0x10 = 7).
  public static void RequireTwoGenuineMotos(MotoScoredObservation first,MotoScoredObservation second){
   if(first==null||second==null||first.Phase!=6||first.NextPhase!=7||first.SubEvent!=0||first.PostComplete||second.Phase!=7||second.SubEvent!=0||!second.PostComplete)throw new InvalidOperationException("Actual first→second Moto lifecycle incomplete.");
   var points=new[]{9,10,11,12,13,14,15,16,18,20,22,25};
   if(!first.Rows.Select(r=>r.ScoreOne).OrderBy(x=>x).SequenceEqual(points)||!second.Rows.Select(r=>r.ScoreTwo).OrderBy(x=>x).SequenceEqual(points))throw new InvalidOperationException("Actual canonical twelve-place points distribution differs.");
   var previous=first.Rows.ToDictionary(Key);if(!second.Rows.Select(Key).OrderBy(x=>x).SequenceEqual(previous.Keys.OrderBy(x=>x)))throw new InvalidOperationException("Actual Moto roster changed.");
   foreach(var row in second.Rows){var prior=previous[Key(row)];if(prior.ScoreOne!=row.ScoreOne)throw new InvalidOperationException("ScoreOne was not carried unchanged.");if(prior.SortValue!=25-prior.ScoreOne||row.SortValue!=50-(row.ScoreOne+row.ScoreTwo))throw new InvalidOperationException("Actual copied Moto SortValue differs from canonical points sum.");}
  }
 }
}
