using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;
namespace Reflex.Port.OpenFrontEnd {
 // Main-thread coordinator. Every mutation is an explicit queued CareerWorker operation.
 public sealed class OwnCareerPresenter : IDisposable {
  readonly int owner=Thread.CurrentThread.ManagedThreadId;
  readonly object world;readonly ulong generation;readonly CareerWorker worker;readonly Func<bool> current;
  readonly Action cancelEvent;readonly Action wake;readonly Action<OwnCareerPresentationNeed> runtimeNeed;readonly Action<string> error;readonly Action returnToCareer;
  readonly OwnCareerPanels panels;readonly CareerCurtainPresenter curtain;readonly HashSet<ulong> accepted=new HashSet<ulong>(),saved=new HashSet<ulong>();
  Task<RxCareerResult> pending;Action completed;Action retry;string operation;bool disposed,ticking,continueIntent,continueSent,failed,savedShown,resultsContinueShown;
  ulong completeEventTicket,frontendTicket,completeSeriesTicket,shownRequest,panelEpoch,routeReadyId,resultsHubId;
  OwnCareerUiToken requestToken,rulesToken,runtimeToken;Func<bool> externalRouteReady;
  public bool BlocksInput=>!disposed&&(panels.Visible||curtain.BlocksInput);
  public string Failure{get;private set;}
  public CareerSnapshot CurrentSnapshot=>View();
  public bool CurtainBlocksInput=>!disposed&&curtain.BlocksInput;
  public bool CanReturnToCareer=>pending==null&&HasDurableProof(View());
  public OwnCareerPresenter(Transform parent,VisualElement overlayRoot,object worldIdentity,ulong generation,CareerWorker worker,
   Func<bool> isCurrent,Action wakeWorker,Action<OwnCareerPresentationNeed> runtimeNeed,Action<string> error,Action returnToCareer,Action cancelEvent=null,Func<bool> replayAvailable=null){
   world=worldIdentity??throw new ArgumentNullException(nameof(worldIdentity));if(generation==0)throw new ArgumentException("Missing native generation.");this.generation=generation;
   this.worker=worker??throw new ArgumentNullException(nameof(worker));current=isCurrent??throw new ArgumentNullException(nameof(isCurrent));wake=wakeWorker??throw new ArgumentNullException(nameof(wakeWorker));
   this.runtimeNeed=runtimeNeed??throw new ArgumentNullException(nameof(runtimeNeed));this.error=error;this.returnToCareer=returnToCareer;this.cancelEvent=cancelEvent;
   panels=new OwnCareerPanels(parent,IsCurrent,replayAvailable);
   try{curtain=new CareerCurtainPresenter(overlayRoot,world,generation,worker,Covered,Fail);}catch{panels.Dispose();throw;}
  }
  void Owner(){if(Thread.CurrentThread.ManagedThreadId!=owner)throw new InvalidOperationException("Career UI belongs to the Unity main thread.");}
  bool IsCurrent()=>!disposed&&current();
  CareerSnapshot View(){var value=worker.Snapshot;return IsCurrent()&&value!=null&&value.Status.generation==generation?value:null;}
  bool Exact(OwnCareerUiToken token)=>token!=null&&ReferenceEquals(token.World,world)&&token.Generation==generation&&IsCurrent();
  bool PendingToken(OwnCareerUiToken token){var v=View();return Exact(token)&&v?.Request.HasValue==true&&v.Request.Value.generation==token.Generation&&v.Request.Value.id==token.Id&&v.Request.Value.kind==(uint)token.Kind&&v.Request.Value.Name==token.Name;}
  void Fail(string message){if(disposed)return;Failure=message;failed=true;panels.ShowStatus("CAREER NEEDS ATTENTION",message,null,cancelEvent);error?.Invoke(message);}
  void Need(OwnCareerNeed kind,OwnCareerUiToken token,string rules=null){runtimeToken=token;runtimeNeed(new OwnCareerPresentationNeed(kind,token,rules));}
  bool Queue(string label,Func<Task<RxCareerResult>> submit,Action success=null,Action retryAction=null){
   if(!IsCurrent()||pending!=null)return false;
   try{var task=submit();if(task==null)throw new InvalidOperationException("Worker returned no command receipt.");pending=task;completed=success;retry=retryAction;operation=label;wake();return true;}
   catch(Exception e){Fail(label+": "+e.Message);return false;}
  }
  void PollCommand(){
   if(pending==null||!pending.IsCompleted)return;var task=pending;var done=completed;var again=retry;var label=operation;pending=null;completed=null;retry=null;
   if(!IsCurrent())return;
   string problem=task.IsCanceled?"The operation was cancelled.":task.IsFaulted?task.Exception.GetBaseException().Message:null;
   // Busy is a pre-dispatch refusal, never an accepted receipt. Retain only
   // the already-clicked final Moto intent; gate buttons await another user click.
   if(problem==null&&task.Result==RxCareerResult.Busy&&View()?.Moto!=null&&(label=="Moto page action"||label=="Actual results Continue")){if(label=="Actual results Continue")continueIntent=true;return;}
   // Heat: a busy queue refuses the action before dispatch; the page keeps its control (the loading entry is retried by the page).
   if(problem==null&&task.Result==RxCareerResult.Busy&&View()?.Heat!=null&&(label=="Heat page action"||label=="AAA loading page entered"||label=="Actual results Continue")){if(label=="Actual results Continue")continueIntent=true;return;}
   if(problem==null&&task.Result!=RxCareerResult.Ok&&task.Result!=RxCareerResult.Repeat)problem="Core rejected the operation: "+task.Result;
   if(problem!=null){
    if(again!=null){failed=true;Failure=View()?.CheckpointFailure??("Checkpoint not confirmed: "+problem);panels.ShowStatus("CHECKPOINT NEEDS ATTENTION",Failure,()=>{if(!IsCurrent()||pending!=null)return;failed=false;Failure=null;again();},cancelEvent);error?.Invoke(Failure);}
    else Fail(label+": "+problem);
    return;
   }
   done?.Invoke();
  }
  void Covered(CareerCurtainToken token){
   if(!IsCurrent()||token.Generation!=generation||!ReferenceEquals(token.World,world))return;
   var copy=new OwnCareerUiToken(token);
   if(token.Kind==RxCareerRequestKind.FadeOut)Need(OwnCareerNeed.FadeHandoff,copy);
   else if(token.Name=="SeriesEvent")Need(OwnCareerNeed.SeriesWipeHandoff,copy);
   else if(token.Name=="RulesPopup")Need(OwnCareerNeed.RulesHide,copy);
   else if(token.Name=="AAALoading")Need(OwnCareerNeed.LoadingHub,copy);
   else Fail("Unsupported covered career transition: "+token.Name);
  }
  public bool ReportFadeHandoffReady(OwnCareerUiToken token){Owner();return Exact(token)&&ReferenceEquals(runtimeToken,token)&&token.Kind==RxCareerRequestKind.FadeOut&&curtain.ReleaseFade(token.Curtain);}
  public bool ReportSeriesWipeReady(OwnCareerUiToken token){Owner();return Exact(token)&&ReferenceEquals(runtimeToken,token)&&token.Name=="SeriesEvent"&&curtain.BeginOut(token.Curtain);}
  // Reports are real runtime readiness attestations, never elapsed-time defaults.
  public bool ReportRulesShown(OwnCareerUiToken token){
   Owner();var v=View();if(!ReferenceEquals(rulesToken,token)||!PendingToken(token)||v?.Transitions.HasValue!=true)return false;
   var t=v.Transitions.Value;if(t.rules_request_id!=token.Id||t.rules_state!=(uint)RxCareerRulesState.Requested)return false;
   return Ack(token);
  }
  public bool AcceptRules(OwnCareerUiToken token,bool doNotShowAgain){
   Owner();var v=View();if(!ReferenceEquals(rulesToken,token)||!Exact(token)||v?.Transitions.HasValue!=true)return false;
   var t=v.Transitions.Value;if(t.rules_request_id!=token.Id||t.rules_state!=(uint)RxCareerRulesState.Shown)return false;
   return Queue("Accept rules",()=>worker.AcceptRules(generation,token.Id,doNotShowAgain));
  }
  public bool ReportRulesHidden(OwnCareerUiToken token){
   Owner();var v=View();if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Name!="RulesPopup"||v?.Transitions.HasValue!=true)return false;
   var t=v.Transitions.Value;if(t.rules_wipe_id!=token.Id||t.rules_state!=(uint)RxCareerRulesState.HideRequested||t.RequiredUiEvent!="HideRaceRulesPopup")return false;
   return Queue("Rules page ended",()=>worker.RulesPopupEnded(generation,token.Id),()=>{if(!curtain.BeginOut(token.Curtain))Fail("Rules curtain changed before its Out animation.");});
  }
  public bool ReportLoadingHubRendered(OwnCareerUiToken token){
   Owner();var v=View();if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Name!="AAALoading"||v?.Transitions.HasValue!=true)return false;
   var t=v.Transitions.Value;if(t.loading_wipe_id!=token.Id||t.loading_state!=(uint)RxCareerLoadingState.RouteHub||t.RequiredUiEvent!="RoutedGotoHub")return false;
   return Queue("Loading Hub rendered",()=>worker.LoadingHubEntered(generation,token.Id),()=>{if(!curtain.BeginOut(token.Curtain))Fail("Loading curtain changed before its Out animation.");});
  }
  public bool ReportHubRendered(OwnCareerUiToken token){Owner();return ReferenceEquals(runtimeToken,token)&&PendingToken(token)&&token.Kind==RxCareerRequestKind.Hub&&Ack(token);}
  // The actual owned page must still be painted when the user presses a button.
  // Queue only after the exact Hub receipt was accepted by ReportHubRendered.
  public bool MotoPageAction(OwnCareerUiToken token,uint action,MotoWorkerSnapshot expected,int gate,Func<bool> stillPainted){
   Owner();var current=View()?.Moto;
   if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Kind!=RxCareerRequestKind.Hub||!accepted.Contains(token.Id)||expected==null||current==null||(current.FirstContinued!=expected.FirstContinued||current.FirstPostObserved!=expected.FirstPostObserved||current.Generation!=expected.Generation||current.Phase!=expected.Phase||current.SubEvent!=expected.SubEvent||current.Cursor!=expected.Cursor||current.NextPhase!=expected.NextPhase||current.GateActive!=expected.GateActive)||stillPainted==null||!stillPainted())return false;
   return Queue("Moto page action",()=>worker.MotoAction(generation,token.Id,action,expected,gate));
  }
  public bool MotoFinalContinue(OwnCareerUiToken token,Func<bool> stillPainted){
   Owner();var v=View();if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Kind!=RxCareerRequestKind.Hub||!accepted.Contains(token.Id)||v?.SecondMoto==null||v.Moto==null||v.Moto.Phase!=7||v.Moto.SubEvent!=0||!v.Moto.Complete||v.Status.continue_allowed!=1||v.Status.continue_submitted!=0||continueSent||stillPainted==null||!stillPainted())return false;
   MotoScoredObservation.RequireTwoGenuineMotos(v.FirstMoto,v.SecondMoto);continueIntent=true;return true;
  }
  // The Heat pages (rx_career_heat.h): an action of the painted page, for the exact Heat state it was painted for.
  public bool HeatPageAction(OwnCareerUiToken token,RxHeatAction action,HeatWorkerSnapshot expected,int gate,Func<bool> stillPainted){
   Owner();var current=View()?.Heat;
   if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Kind!=RxCareerRequestKind.Hub||!accepted.Contains(token.Id)||expected==null||!expected.Same(current)||stillPainted==null||!stillPainted())return false;
   return Queue("Heat page action",()=>worker.HeatAction(generation,token.Id,action,expected,gate));
  }
  // The AAA loading page is up for the hub whose routing is "Select Activity Area": the original's MXUIAAALoadingPage entry (the
  // activity area is re-created for the next round; rx_session_career_loading_enter).
  public bool HeatEnterLoading(OwnCareerUiToken token,HeatWorkerSnapshot expected,Func<bool> stillPainted,Action entered){
   Owner();var v=View();var current=v?.Heat;
   if(!ReferenceEquals(runtimeToken,token)||!Exact(token)||token.Kind!=RxCareerRequestKind.Hub||!accepted.Contains(token.Id)||expected==null||!expected.ActivityAreaRequired||!expected.Same(current)||
      v.Status.pending_requests!=0||v.Request.HasValue||stillPainted==null||!stillPainted())return false;
   return Queue("AAA loading page entered",()=>worker.EnterLoading(generation),entered);
  }
  public bool ReportCareerRouteRendered(OwnCareerUiToken token,Func<bool> stillRendered){
   Owner();if(!ReferenceEquals(runtimeToken,token)||!PendingToken(token)||token.Name!="GotoCareerRaceAndRidePage"||stillRendered==null||!stillRendered())return false;
   externalRouteReady=stillRendered;routeReadyId=token.Id;return Ack(token);
  }
  bool Ack(OwnCareerUiToken token)=>Queue("Present "+token.Kind,()=>worker.Acknowledge(generation,token.Id,token.Kind,true),()=>accepted.Add(token.Id));
  void ContinueClicked(ulong shown){
   var v=View();if(shown!=panelEpoch||!panels.Rendered(shown)||!resultsContinueShown||v==null||v.Status.continue_allowed!=1||v.Status.continue_submitted!=0||continueSent)return;
   continueIntent=true;
  }
  void ShowResults(RxCareerStatus status){
   ulong shown=0;
   panels.ShowResults(status,()=>ContinueClicked(shown));shown=panels.ShowEpoch;panelEpoch=shown;
   resultsContinueShown=status.continue_allowed==1&&status.continue_submitted==0;
  }
  void RefreshResultsPermission(CareerSnapshot view){
   // The initial real Hub must be painted/acknowledged before native Continue
   // can become allowed. Re-render only that same accepted results page when
   // permission arrives; never acknowledge or submit Continue here.
   if(resultsContinueShown||continueSent||continueIntent||pending!=null||resultsHubId==0||panels.Page!="results"||
      !accepted.Contains(resultsHubId)||requestToken==null||requestToken.Id!=resultsHubId||requestToken.Kind!=RxCareerRequestKind.Hub||!Exact(requestToken)||
      view.Status.generation!=generation||view.Request.HasValue||view.Status.pending_requests!=0||!view.FinalResults||
      view.Status.continue_allowed!=1||view.Status.continue_submitted!=0)return;
   ShowResults(view.Status); // new epoch; its button requires a fresh actual paint and input release
  }
  void Save(OwnCareerUiToken token){
   if(!PendingToken(token))return;
   Queue("Save actual player checkpoint",()=>worker.SaveCheckpoint(generation,token.Id),()=>{saved.Add(token.Id);accepted.Add(token.Id);Failure=null;failed=false;},()=>Save(token));
  }
  void Request(CareerSnapshot view,double clock){
   var r=view.Request.Value;if(r.generation!=generation||r.id==0||r.state!=1||r.delivered!=1){Fail("Career request does not belong to the current delivered queue.");return;}
   if(accepted.Contains(r.id))return;
   var kind=(RxCareerRequestKind)r.kind;
   if(kind==RxCareerRequestKind.FadeOut||kind==RxCareerRequestKind.PageWipe){
    if(curtain.Token!=null&&curtain.Token.Id!=r.id&&curtain.Phase!=CareerCurtainTimeline.Stage.Idle&&curtain.Phase!=CareerCurtainTimeline.Stage.Done)return;
    curtain.Present(world,r,clock);return;
   }
   if(requestToken==null||requestToken.Id!=r.id){requestToken=new OwnCareerUiToken(world,r);shownRequest=0;}
   var token=requestToken;
   if(kind==RxCareerRequestKind.Hub){
    if(view.Moto!=null){if(shownRequest!=r.id){shownRequest=r.id;Need(OwnCareerNeed.Hub,token);}}else if(view.FinalResults){
     if(shownRequest!=r.id){resultsHubId=r.id;ShowResults(view.Status);shownRequest=r.id;}
     if(panels.Rendered(panelEpoch)&&pending==null)Ack(token);
    }else if(shownRequest!=r.id){shownRequest=r.id;Need(OwnCareerNeed.Hub,token);}
   }else if(kind==RxCareerRequestKind.UiEvent&&token.Name=="ShowRaceRulesPopup"){
    if(!view.Transitions.HasValue||view.Transitions.Value.rules_request_id!=r.id){Fail("Rules request has no matching native transition.");return;}
    if(shownRequest!=r.id){shownRequest=r.id;rulesToken=token;Need(OwnCareerNeed.RulesShow,token,view.Transitions.Value.RulesType);}
   }else if(kind==RxCareerRequestKind.UiRoute){
    if(!view.Completion.HasValue){Fail("Career route has no completion identity.");return;}var c=view.Completion.Value;
    if(c.frontend_request_id!=r.id||c.FrontendRoute!=token.Name||c.ticket==0){Fail("Unexpected career frontend route.");return;}
    if(token.Name=="GotoRewardsPage"){
     if(view.Status.reward_valid!=1){Fail("Native rewards are unavailable.");return;}
     if(shownRequest!=r.id){panels.ShowRewards(view.Status,false,null);shownRequest=r.id;panelEpoch=panels.ShowEpoch;routeReadyId=r.id;externalRouteReady=null;}
     if(panels.Rendered(panelEpoch)&&pending==null)Ack(token);
    }else if(token.Name=="GotoCareerRaceAndRidePage"){
     if(shownRequest!=r.id){panels.Hide();shownRequest=r.id;Need(OwnCareerNeed.CareerRoute,token);}
    }else Fail("Unsupported career route: "+token.Name);
   }else if(kind==RxCareerRequestKind.Checkpoint){
    if(!view.Completion.HasValue){Fail("Checkpoint has no actual completion identity.");return;}var c=view.Completion.Value;
    if((r.id!=c.checkpoint_one_id&&r.id!=c.checkpoint_two_id)||r.controller!=view.Status.controller){Fail("Unexpected completion checkpoint identity.");return;}
    if(pending==null)Save(token);
   }else Fail("Unsupported career presentation: "+kind+" / "+token.Name);
  }
  bool RouteRendered(ulong id)=>routeReadyId==id&&(externalRouteReady!=null?externalRouteReady():panels.Rendered(panelEpoch));
  void Completion(CareerSnapshot v){
   if(continueIntent&&!continueSent&&v.Status.continue_allowed==1&&v.Status.pending_requests==0){
    if(Queue("Actual results Continue",()=>worker.Continue(generation),()=>continueSent=true)){continueIntent=false;panels.ShowStatus("EVENT COMPLETE","Preparing the actual results continuation…");}return;
   }
   if(!continueSent||!v.Completion.HasValue)return;var c=v.Completion.Value;
   if(c.generation!=generation||c.ticket==0)return;
   if(c.complete_event_allowed==1&&completeEventTicket!=c.ticket){if(Queue("Complete actual event",()=>worker.CompleteEvent(generation,c.ticket),()=>completeEventTicket=c.ticket))return;}
   if(c.frontend_ready_allowed==1&&frontendTicket!=c.ticket&&accepted.Contains(c.frontend_request_id)&&RouteRendered(c.frontend_request_id)){
    Queue("Rendered frontend ready",()=>worker.FrontendReady(generation,c.ticket,c.frontend_request_id),()=>frontendTicket=c.ticket);return;
   }
   if(c.complete_series_allowed==1&&completeSeriesTicket!=c.ticket&&frontendTicket==c.ticket){Queue("Complete actual series",()=>worker.CompleteSeries(generation,c.ticket),()=>completeSeriesTicket=c.ticket);return;}
   bool durable=HasDurableProof(v);
   if(durable&&!savedShown){
    savedShown=true;if(v.Status.reward_valid==1)panels.ShowRewards(v.Status,true,ReturnClicked);else panels.ShowStatus("PROGRESS SAVED","The actual player checkpoint has been saved.",ReturnClicked,null,"RETURN TO CAREER");
   }
  }
  bool HasDurableProof(CareerSnapshot v){
   if(v==null||!v.Completion.HasValue||v.Status.generation!=generation||!IsCurrent())return false;var c=v.Completion.Value;
   return c.generation==generation&&c.ticket!=0&&c.durable==1&&c.in_memory_settled==1&&c.complete_series_state==2&&c.records_state==2&&c.checkpoint_error==0&&
    c.checkpoint_one_id!=0&&c.checkpoint_two_id!=0&&c.checkpoint_one_id!=c.checkpoint_two_id&&saved.Contains(c.checkpoint_one_id)&&saved.Contains(c.checkpoint_two_id)&&
    v.Status.records_finalized==1&&v.Status.checkpoint_error==0&&v.Status.pending_requests==0&&!v.Request.HasValue&&v.CheckpointFailure==null&&Failure==null;
  }
  public bool RequestReturnToCareer(){Owner();if(!savedShown||!CanReturnToCareer||returnToCareer==null)return false;returnToCareer();return true;}
  void ReturnClicked(){RequestReturnToCareer();}
  public void Tick(object actualWorld,ulong actualGeneration,double clock){
   Owner();if(disposed)return;if(!ReferenceEquals(world,actualWorld)||actualGeneration!=generation||!current()){Dispose();return;}
   if(ticking)throw new InvalidOperationException("Career UI tick is reentrant.");ticking=true;
   try{
    curtain.Tick(world,generation,clock);
    var waiting=curtain.Phase;if(waiting==CareerCurtainTimeline.Stage.FadeReceipt||waiting==CareerCurtainTimeline.Stage.TransitionReceipt||waiting==CareerCurtainTimeline.Stage.OutReceipt)wake();
    panels.Tick(!curtain.BlocksInput);PollCommand();if(failed||pending!=null)return;
    var view=View();if(view==null)return;
    if(view.Request.HasValue)Request(view,clock);else {RefreshResultsPermission(view);Completion(view);}

   }catch(Exception e){Fail("Career presentation failed: "+e.Message);}finally{ticking=false;}
  }
  public void Dispose(){Owner();if(disposed)return;disposed=true;pending=null;completed=null;retry=null;runtimeToken=requestToken=rulesToken=null;curtain.Dispose();panels.Dispose();}
 }
}
