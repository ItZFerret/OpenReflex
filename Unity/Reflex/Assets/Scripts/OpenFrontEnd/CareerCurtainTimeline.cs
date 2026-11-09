using System;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.RxCore;
namespace Reflex.Port.OpenFrontEnd {
 public sealed class CareerCurtainToken {
  public readonly object World; public readonly ulong Generation,Id; public readonly RxCareerRequestKind Kind; public readonly string Name; public readonly float Seconds; public readonly uint Color;
  internal CareerCurtainToken(object world,RxCareerRequest r){World=world;Generation=r.generation;Id=r.id;Kind=(RxCareerRequestKind)r.kind;Name=r.Name;Seconds=r.seconds;Color=r.color;}
 }
 public sealed class CareerWipeCurve {
  public readonly double Duration;readonly double[] time;readonly float[] alpha;
  public CareerWipeCurve(double duration,double[] times,float[] values){
   if(!Finite(duration)||duration<=0||times==null||values==null||times.Length==0||times.Length!=values.Length)throw new ArgumentException("Invalid authored wipe curve.");
   time=(double[])times.Clone();alpha=(float[])values.Clone();Duration=duration;
   for(int i=0;i<time.Length;i++)if(!Finite(time[i])||time[i]<0||time[i]>duration||(i>0&&time[i]<=time[i-1])||float.IsNaN(alpha[i])||float.IsInfinity(alpha[i])||alpha[i]<0||alpha[i]>1)throw new ArgumentException("Invalid authored wipe key.");
  }
  internal static bool Finite(double x)=>!double.IsNaN(x)&&!double.IsInfinity(x);
  public float Sample(double elapsed){if(elapsed<=time[0])return alpha[0];for(int i=1;i<time.Length;i++)if(elapsed<time[i])return alpha[i-1]+(alpha[i]-alpha[i-1])*(float)((elapsed-time[i-1])/(time[i]-time[i-1]));return alpha[alpha.Length-1];}
 }
 public sealed class CareerWipeAnimation {
  public readonly CareerWipeCurve In,Out; public readonly uint Argb;
  public CareerWipeAnimation(CareerWipeCurve enter,CareerWipeCurve leave,uint argb){Argb=argb;In=enter??throw new ArgumentNullException(nameof(enter));Out=leave??throw new ArgumentNullException(nameof(leave));if(In.Sample(In.Duration)!=1||Out.Sample(0)!=1||Out.Sample(Out.Duration)!=0)throw new ArgumentException("Wipe must cover before transition and uncover at completion.");}
 }
 public interface ICareerCurtainCommands {
  Task<RxCareerResult> FadeComplete(CareerCurtainToken token);
  Task<RxCareerResult> WipeMilestone(CareerCurtainToken token,uint milestone);
 }
 // Main-thread protocol only. Rendered endpoint is separate from clock advancement.
 public sealed class CareerCurtainTimeline : IDisposable {
  public enum Stage {Idle,Fade,FadePaint,FadeReceipt,FadeHeld,In,InPaint,TransitionReceipt,Covered,Out,OutPaint,OutReceipt,Done,Failed,Disposed}
  readonly int owner=Thread.CurrentThread.ManagedThreadId;readonly object world;readonly ulong generation;readonly ICareerCurtainCommands commands;
  readonly Func<string,CareerWipeAnimation> resolve;readonly Action<CareerCurtainToken> covered;readonly Action<string> failure;
  Task<RxCareerResult> pending;CareerWipeAnimation wipe;double start,now,lastTime=double.NegativeInfinity,duration;bool painted,updating;
  public Stage Phase{get;private set;}public CareerCurtainToken Token{get;private set;}public float Alpha{get;private set;}public uint Argb{get;private set;}public string Error{get;private set;}
  public CareerCurtainTimeline(object world,ulong generation,ICareerCurtainCommands commands,Func<string,CareerWipeAnimation> resolve,Action<CareerCurtainToken> covered,Action<string> failure){this.world=world??throw new ArgumentNullException(nameof(world));if(generation==0)throw new ArgumentException("Missing session generation.");this.generation=generation;this.commands=commands??throw new ArgumentNullException(nameof(commands));this.resolve=resolve;this.covered=covered;this.failure=failure;}
  void Owner(){if(Thread.CurrentThread.ManagedThreadId!=owner)throw new InvalidOperationException("Curtain must run on its main-thread owner.");}
  bool Current(CareerCurtainToken t)=>t!=null&&ReferenceEquals(t,Token)&&ReferenceEquals(t.World,world)&&t.Generation==generation;
  void Fail(string message){Error=message;Phase=Stage.Failed;pending=null;failure?.Invoke(message);}
  public bool Present(object currentWorld,RxCareerRequest request,double clock){
   Owner();if(updating)return false;if((Phase==Stage.Disposed||Phase==Stage.Failed)||!ReferenceEquals(world,currentWorld)||request.generation!=generation)return false;
   if(request.kind!=(uint)RxCareerRequestKind.FadeOut&&request.kind!=(uint)RxCareerRequestKind.PageWipe)return false;
   if(Token!=null&&request.id==Token.Id){if(Token.Kind!=(RxCareerRequestKind)request.kind||Token.Name!=request.Name||Token.Seconds!=request.seconds||Token.Color!=request.color){Fail("A delivered curtain token changed its payload.");return false;}return true;} // Polling the same delivered request never restarts animation.
   if(Phase!=Stage.Idle&&Phase!=Stage.Done){Fail("A different curtain request arrived before the current presentation completed.");return false;}
   if(request.id==0||request.delivered!=1||request.state!=1||!CareerWipeCurve.Finite(clock)){Fail("Invalid delivered curtain request.");return false;}
   Token=new CareerCurtainToken(world,request);start=now=lastTime=clock;painted=false;pending=null;Error=null;
   if(Token.Kind==RxCareerRequestKind.FadeOut){
    if(float.IsNaN(request.seconds)||float.IsInfinity(request.seconds)||request.seconds<0){Fail("Invalid native fade duration.");return false;}
    duration=request.seconds;Argb=request.color;Alpha=0;Phase=Stage.Fade;
   }else{
    if(Token.Name!="SeriesEvent"&&Token.Name!="RulesPopup"&&Token.Name!="AAALoading"){Fail("Unsupported career page wipe: "+Token.Name);return false;}
    try{wipe=resolve?.Invoke(Token.Name);if(wipe==null)throw new InvalidOperationException("The install's PageWipe animation is unavailable.");}
    catch(Exception e){Fail("Page wipe unavailable: "+e.Message);return false;}
    Argb=wipe.Argb;Alpha=wipe.In.Sample(0);Phase=Stage.In;
   }return true;
  }
  // Called only after the actual UI draw callback emitted the current endpoint.
  public void Painted(){Owner();if(Phase==Stage.FadePaint||Phase==Stage.InPaint||Phase==Stage.OutPaint)painted=true;}
  public bool BeginOut(CareerCurtainToken token){Owner();if(Phase!=Stage.Covered||!Current(token))return false;start=now;painted=false;Phase=Stage.Out;return true;}
  public bool ReleaseFade(CareerCurtainToken token){Owner();if(Phase!=Stage.FadeHeld||!Current(token))return false;Alpha=0;Phase=Stage.Done;return true;}
  public void Tick(double clock){Owner();if(updating)throw new InvalidOperationException("Curtain tick is reentrant.");updating=true;try{Step(clock);}finally{updating=false;}}
  void Step(double clock){
   Owner();if(Phase==Stage.Disposed||Phase==Stage.Failed)return;
   if(!CareerWipeCurve.Finite(clock)||clock<lastTime){Fail("Curtain clock is not monotonic.");return;}now=lastTime=clock;
   if(Phase==Stage.Fade){Alpha=duration==0?1:(float)Math.Min(1,(clock-start)/duration);if(Alpha>=1){Phase=Stage.FadePaint;painted=false;}}
   else if(Phase==Stage.In){Alpha=wipe.In.Sample(clock-start);if(clock-start>=wipe.In.Duration){Phase=Stage.InPaint;painted=false;}}
   else if(Phase==Stage.Out){Alpha=wipe.Out.Sample(clock-start);if(clock-start>=wipe.Out.Duration){Phase=Stage.OutPaint;painted=false;}}
   else if(painted&&(Phase==Stage.FadePaint||Phase==Stage.InPaint||Phase==Stage.OutPaint)){
    var phase=Phase;painted=false;
    try{pending=phase==Stage.FadePaint?commands.FadeComplete(Token):commands.WipeMilestone(Token,phase==Stage.InPaint?1u:2u);if(pending==null)throw new InvalidOperationException("Presenter command did not return a receipt.");Phase=phase==Stage.FadePaint?Stage.FadeReceipt:phase==Stage.InPaint?Stage.TransitionReceipt:Stage.OutReceipt;}
    catch(Exception e){Fail("Curtain command failed: "+e.Message);}
   }else if(pending!=null&&pending.IsCompleted){
    var phase=Phase;var task=pending;pending=null;
    if(task.IsCanceled||task.IsFaulted){Fail("Curtain command failed: "+(task.Exception?.GetBaseException().Message??"cancelled"));return;}
    var result=task.Result;if(result!=RxCareerResult.Ok&&result!=RxCareerResult.Repeat){Fail("Curtain command rejected: "+result);return;}
    if(phase==Stage.FadeReceipt){Phase=Stage.FadeHeld;try{covered?.Invoke(Token);}catch(Exception e){Fail("Covered fade continuation failed: "+e.Message);}}
    else if(phase==Stage.TransitionReceipt){Phase=Stage.Covered;try{covered?.Invoke(Token);}catch(Exception e){Fail("Covered transition failed: "+e.Message);}}
    else if(phase==Stage.OutReceipt)Phase=Stage.Done;
   }
  }
  public void Dispose(){Owner();Phase=Stage.Disposed;pending=null;Token=null;Alpha=0;}
 }
}
