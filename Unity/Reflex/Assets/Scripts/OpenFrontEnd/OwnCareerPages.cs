using System;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
namespace Reflex.Port.OpenFrontEnd {
 // Actual own pages, never original UI trees or fabricated native state.
 public sealed class OwnCareerPages:IOwnCareerExternalPages,IDisposable {
  readonly OwnCareerRuntimeContext context;readonly OwnCareerPageView view;readonly Action returnToMenu,cancelToMenu;
  bool motoContinueShown;MotoWorkerSnapshot motoShown;
  // Heat (rx_career_heat.h): the state the page was painted for, the CONTINUE permission it showed, the AAA loading page between the
  // rounds and the hub whose loading entry was accepted.
  HeatWorkerSnapshot heatShown;bool heatContinueShown;OwnAaaLoading heatLoading;int heatLoadingFrame;ulong heatLoadingEntered;
  OwnCareerPresentationNeed need;OwnCareerPresenter coordinator;ulong epoch;bool reported,disposed,careerSaved,hiddenForError;
  public RxCareerPresenter Capabilities=>RxCareerPresenter.Hub|RxCareerPresenter.Career|RxCareerPresenter.Rules|RxCareerPresenter.Heat;
  public bool BlocksInput=>!disposed&&view.Visible;
  public OwnCareerPages(OwnCareerRuntimeContext context,Action returnToMenu,Action cancelToMenu){this.context=context??throw new ArgumentNullException(nameof(context));this.returnToMenu=returnToMenu??throw new ArgumentNullException(nameof(returnToMenu));this.cancelToMenu=cancelToMenu??throw new ArgumentNullException(nameof(cancelToMenu));view=new OwnCareerPageView(context.Parent,context.IsCurrent);view.GateFocus=g=>context.Gate?.SetFocus(g);view.GateEnded=()=>context.Gate?.End();}
  // Bind at installation, even when this event never requests an external page.
  // The exact context reference prevents reuse by a later world/generation.
  internal void BindCoordinator(OwnCareerRuntimeContext owner,OwnCareerPresenter presenter){
   if(!ReferenceEquals(owner,context)||!Current())throw new InvalidOperationException("Career page binding belongs to another runtime.");
   if(presenter==null)throw new ArgumentNullException(nameof(presenter));
   if(coordinator!=null&&!ReferenceEquals(coordinator,presenter))throw new InvalidOperationException("Career pages already have a coordinator.");
   coordinator=presenter;
  }
  bool Current()=>!disposed&&context.IsCurrent();
  bool Cancel(){if(!Current())return false;cancelToMenu();return true;}
  public void Present(OwnCareerPresentationNeed request,OwnCareerPresenter presenter){
   if(!Current()||request==null||!ReferenceEquals(request.Token.World,context.WorldIdentity)||request.Token.Generation!=context.Generation)throw new InvalidOperationException("Career page request belongs to another runtime.");
   if(coordinator==null||!ReferenceEquals(coordinator,presenter))throw new InvalidOperationException("Career page request belongs to another coordinator.");
   need=request;reported=false;hiddenForError=false;careerSaved=false;
   switch(request.Kind){
    case OwnCareerNeed.RulesShow:
     gatePage=false;var text=OwnCareerRulesText.Read(request.RulesType);view.Show(text.Title,"MOTOCAREER · RULES");if(text.Format!=null)view.Heading(text.Format);view.Text(text.Description);
     view.Button(OpenContent.Str("STR_ACCEPT","ACCEPT"),"career-rules-accept",()=>{var ok=coordinator.AcceptRules(request.Token,false);if(ok)StopRulesVoice();return ok;});
     view.Button("DON'T SHOW AGAIN","career-rules-skip",()=>{var ok=coordinator.AcceptRules(request.Token,true);if(ok)StopRulesVoice();return ok;});LeaveButton();PlayRulesVoice(request.RulesType);break;
    case OwnCareerNeed.RulesHide:StopRulesVoice();view.Hide();break;
    case OwnCareerNeed.LoadingHub:ReleaseHeatLoading();ShowHub();break;   // the AAALoading wipe covers the screen: the loading page goes under it
    case OwnCareerNeed.Hub:ShowHub();break;
    case OwnCareerNeed.CareerRoute:ShowCareer(false);break;
    default:throw new NotSupportedException("Unknown owned career page request: "+request.Kind);
   }
   epoch=view.Epoch;
  }
  // RulesPopup:Start's narration (App.PlayUILocalizedSoundEvent( "AudioLocalizedActivity", ... RULE ) for the RulesType, stopped at End) on the race's own
  // event player, whose EffectsVolume channel (VoiceOver) the options move; a type without a voice, or a missing package, is silent.
  bool rulesVoice;
  OriginalAudio RaceAudio(){var rt=context.Parent!=null?context.Parent.GetComponent<OriginalWorldRuntime>():null;return rt!=null?rt.gameAudio:null;}
  void PlayRulesVoice(string rulesType){
   var vo=ReflexUiVoice.RulesVoice(rulesType);var audio=RaceAudio();if(!vo.HasValue||audio==null)return;
   rulesVoice=ReflexUiVoice.Play(audio,vo.Value.db,vo.Value.package,vo.Value.eventPath,ReflexUiVoice.RulesWrapper);
  }
  void StopRulesVoice(){if(!rulesVoice)return;rulesVoice=false;ReflexUiVoice.Stop(RaceAudio(),ReflexUiVoice.RulesWrapper);}
  bool gatePage;   // the Gate Selection page is up (the world is the page: no explanatory text)
  void LeaveButton(){if(!gatePage)view.Text("Leaving closes the current event. Any completed player checkpoint stays saved.");view.Button("LEAVE EVENT","career-leave",Cancel);}
  static string Place(int n)=>n+OuWidgets.Ordinal(n);
  string SeriesName(){
   var snapshot=coordinator.CurrentSnapshot;if(snapshot==null)throw new InvalidOperationException("Career snapshot unavailable.");
   if(!ReflexEvents.Tables.TryGetValue("Series",out var series))throw new InvalidOperationException("Authored career table missing.");
   var rows=series.Where(r=>Rfx.Int(r,"SeriesId")==snapshot.Status.series_id).ToArray();if(rows.Length!=1)throw new InvalidOperationException("Authored current series is not unique.");
   var key=FeTables.Shared.TitleKey(Rfx.Int(rows[0],"StoreProductId"));if(string.IsNullOrEmpty(key))throw new InvalidOperationException("Authored career title missing.");var title=OpenContent.Str(key);if(string.IsNullOrWhiteSpace(title)||title==key)throw new InvalidOperationException("Authored career title translation missing.");return title;
  }
  void Describe(){
   var snapshot=coordinator.CurrentSnapshot;if(snapshot==null)throw new InvalidOperationException("Career snapshot unavailable.");string series=SeriesName(),place=null;
   // Authored display name only. Internal filenames, series IDs and numeric
   // vehicle classes belong in diagnostics, not the player's event hub.
   var entry=CareerEventCatalog.Read().SingleOrDefault(e=>e.SeriesId==snapshot.Status.series_id);
   if(entry!=null){var key=FeTables.Shared.LocalizationKey(entry.TrackLocalizationId);if(!string.IsNullOrEmpty(key)){var text=OpenContent.Str(key);if(!string.IsNullOrWhiteSpace(text)&&text!=key)place=text;}}
   view.Hero(series,place);
  }
  bool StartupHubDeparted(){
   if(!reported||need==null||coordinator==null||coordinator.Failure!=null)return false;
   if(need.Kind!=OwnCareerNeed.LoadingHub&&need.Kind!=OwnCareerNeed.Hub)return false;
   var snapshot=coordinator.CurrentSnapshot;if(snapshot==null||snapshot.Status.generation!=need.Token.Generation||snapshot.Status.phase!=(uint)RxCareerPhase.Started||snapshot.Status.session_state==(uint)RxSessionState.Error)return false;
   if(need.Kind==OwnCareerNeed.LoadingHub){
    // Exact native receipt from LoadingHubEntered for THIS wipe. Hide under
    // its still-covering curtain before BeginOut exposes the real world.
    if(!snapshot.Transitions.HasValue)return false;var t=snapshot.Transitions.Value;
    return t.generation==need.Token.Generation&&t.loading_wipe_id==need.Token.Id&&
     (t.loading_state==(uint)RxCareerLoadingState.HubEntered||t.loading_state==(uint)RxCareerLoadingState.Finished);
   }
   // A plain Hub has no loading-wipe receipt. Require its consumed delivered
   // request plus a genuine initial activity; never hide a post-results page.
   return motoShown==null&&heatShown==null&&!snapshot.Request.HasValue&&snapshot.Status.pending_requests==0&&
    snapshot.Status.activity_present==1&&snapshot.Status.activity_state<=2&&snapshot.Status.post_event_complete==0;
  }
  void ShowHub(){
   motoShown=coordinator.CurrentSnapshot?.Moto;
   if(motoShown!=null){ShowMotoHub(motoShown);return;}
   heatShown=need.Kind==OwnCareerNeed.Hub?coordinator.CurrentSnapshot?.Heat:null;
   if(heatShown!=null){ShowHeatHub(heatShown);return;}
   gatePage=false;view.Show("MOTOCAREER","EVENT HUB");Describe();LeaveButton();
  }
  void MotoButton(string text,string id,uint action,MotoWorkerSnapshot expected,int gate=-1){
   var token=need.Token;view.Button(text,id,()=>{var painted=epoch;return Current()&&view.Rendered(painted)&&coordinator.MotoPageAction(token,action,expected,gate,()=>Current()&&need!=null&&ReferenceEquals(need.Token,token)&&view.Rendered(painted));});
  }
  // The hand-off after the gate pick ("Preparing the actual activity…" until the moto's / round's activity is in Pre): no page is drawn
  // (w-uibatch, the user: "cut the preparing race screen"). The page stays up blank (OwnCareerPageView.ShowBlank) so the hand-off keeps its
  // render report and its input hold; the release below (Pre / Countdown / InProgress) is unchanged. Should the hand-off ever stall, the
  // page with its text and LEAVE EVENT is painted after BlankStall seconds (in the game it lasts a few frames: core Pre 6 frames after the pick).
  const float BlankStall=10f;float blankSince=-1f;bool blankStalled;
  bool ShowBlank(){if(blankStalled)return false;if(!view.Blank)blankSince=UnityEngine.Time.unscaledTime;view.ShowBlank();epoch=view.Epoch;return true;}
  static bool MotoPreparing(MotoWorkerSnapshot moto)=>!moto.Complete&&!(moto.GateActive&&moto.Device==0)&&!(moto.Phase==6&&moto.NextPhase==7&&moto.SubEvent==0&&moto.Gates.Count==0)&&!(moto.NextPhase==6||moto.NextPhase==7);
  void ShowMotoHub(MotoWorkerSnapshot moto){
   motoContinueShown=coordinator.CurrentSnapshot.Status.continue_allowed==1;
   // The original never changes SubEvent (+4, stays 0); the phase tells the motos apart (6 = one, 7 = two).
   gatePage=!moto.Complete&&moto.GateActive&&moto.Device==0;
   if(MotoPreparing(moto)){if(ShowBlank())return;}else blankStalled=false;
   view.Show(gatePage?"GATE SELECTION":"MOTOCAREER",moto.Phase==1?"PRACTICE AND GATE SELECTION":moto.Phase==7?"MOTO TWO":"MOTO ONE",gatePage);Describe();
   if(moto.Complete){
    view.Heading("BOTH MOTOS COMPLETE");
    var token=need.Token;if(coordinator.CurrentSnapshot.Status.continue_allowed==1)view.Button("CONTINUE","career-moto-final",()=>{var painted=epoch;return coordinator.MotoFinalContinue(token,()=>Current()&&need!=null&&ReferenceEquals(need.Token,token)&&view.Rendered(painted));});
    int rank=0;foreach(var row in moto.Scores.Rows.OrderBy(r=>r.SortValue))view.Row(++rank,row.Name,row.Type==0,row.ScoreOne+" + "+row.ScoreTwo,(row.ScoreOne+row.ScoreTwo)+" PTS");

   }else if(moto.GateActive&&moto.Device==0){
    context.Gate?.Begin(moto.Gates);view.Heading("CHOOSE YOUR START GATE");foreach(var gate in moto.Gates)MotoButton("GATE "+(gate+1),"career-moto-gate-"+gate,2,moto,gate);
   // Canonical NextSubEvent prepares gates without changing phase/sub-event.
   }else if(moto.Phase==6&&moto.NextPhase==7&&moto.SubEvent==0&&moto.Gates.Count==0){
    view.Heading("MOTO ONE RESULTS");{int rank=0;foreach(var row in moto.Scores.Rows.OrderBy(r=>r.SortValue))view.Row(++rank,row.Name,row.Type==0,null,row.ScoreOne+" PTS");}if(!moto.FirstContinued)MotoButton("CONTINUE","career-moto-first-continue",4,moto);else if(moto.FirstPostObserved)MotoButton("CONTINUE TO MOTO TWO","career-moto-next",3,moto);else view.Text("Completing results…");
   }else if(moto.NextPhase==6||moto.NextPhase==7){
    view.Heading(moto.Phase==1?"PRACTICE COMPLETE · SELECT YOUR START GATE":"SELECT YOUR START GATE");MotoButton("CHOOSE START GATE","career-moto-gates",1,moto);
   }else view.Text("Preparing the actual activity…");
   LeaveButton();epoch=view.Epoch;
  }
  void HeatButton(string text,string id,RxHeatAction action,HeatWorkerSnapshot expected,int gate=-1){
   var token=need.Token;view.Button(text,id,()=>{var painted=epoch;return Current()&&view.Rendered(painted)&&coordinator.HeatPageAction(token,action,expected,gate,()=>Current()&&need!=null&&ReferenceEquals(need.Token,token)&&view.Rendered(painted));});
  }
  static bool HeatContinueAllowed(RxCareerStatus s)=>s.activity_present==1&&s.activity_state==4&&s.activity_substate==2;
  // The Heat event's pages (InGame PostEventMenu / PostEventOverlay / AAALoading): the gate selection of every round, the heat / LCQ
  // results with their CONTINUE and the next round, the AAA loading page while the riders of the next round are re-created.
  void ShowHeatHub(HeatWorkerSnapshot heat){
   var status=coordinator.CurrentSnapshot.Status;heatContinueShown=HeatContinueAllowed(status);
   uint round=heat.RoundResults?heat.Phase:heat.NextPhase==3||heat.NextPhase==4||heat.NextPhase==5?heat.NextPhase:heat.Phase;
   if(heat.ActivityAreaRequired){
    gatePage=false;view.Show("MOTOCAREER","LOADING · "+HeatWorkerSnapshot.RoundName(heat.NextPhase));Describe();view.Heading("LOADING");view.Text("Preparing the riders of the next round…");
    if(!heatLoading){heatLoading=OwnAaaLoading.ShowHeld();heatLoadingFrame=UnityEngine.Time.frameCount;}
    epoch=view.Epoch;return;
   }
   gatePage=heat.GateActive&&heat.Device==(int)status.controller;
   if(!gatePage&&!heat.RoundResults&&!(heat.GateReady&&status.activity_state!=2)){if(ShowBlank())return;}else blankStalled=false;
   view.Show(gatePage?"GATE SELECTION":"MOTOCAREER",HeatWorkerSnapshot.RoundName(round),gatePage);Describe();
   if(gatePage){
    context.Gate?.Begin(heat.Gates);view.Heading("CHOOSE YOUR START GATE");foreach(var gate in heat.Gates)HeatButton("GATE "+(gate+1),"career-heat-gate-"+gate,RxHeatAction.GateSelect,heat,gate);
   }else if(heat.RoundResults){
    view.Heading(HeatWorkerSnapshot.RoundName(heat.Phase)+" RESULTS");
    foreach(var row in heat.Results)view.Row(row.Rank+1,row.Name+(row.Local?"  (YOU)":""),row.Local,(row.Dnf?"DNF":"")+(row.Dnf&&row.Outcome.Length>0?"  ":"")+(row.Outcome.Length>0?"→ "+row.Outcome:""));
    if(!heat.RoundContinued){if(heatContinueShown)HeatButton("CONTINUE","career-heat-continue",RxHeatAction.RoundContinue,heat);else view.Text("Completing results…");}
    else if(heat.RoundPostObserved)HeatButton("CONTINUE TO THE "+HeatWorkerSnapshot.RoundName(heat.NextPhase),"career-heat-next",RxHeatAction.Next,heat);
    else view.Text("Completing results…");
   }else if(heat.GateReady&&status.activity_state!=2){
    view.Heading(heat.Phase==1?"PRACTICE COMPLETE · SELECT YOUR START GATE":"SELECT YOUR START GATE");HeatButton("CHOOSE START GATE","career-heat-gates",RxHeatAction.GateBegin,heat);
   }else view.Text("Preparing the actual activity…");
   LeaveButton();epoch=view.Epoch;
  }
  void ReleaseHeatLoading(){if(heatLoading)heatLoading.Release();heatLoading=null;}
  void TickHeat(){
   if(need?.Kind!=OwnCareerNeed.Hub||heatShown==null)return;
   var state=coordinator.CurrentSnapshot;var live=state?.Heat;if(state==null||live==null)return;
   if(!live.Same(heatShown)||(live.RoundResults&&!live.RoundContinued&&heatContinueShown!=HeatContinueAllowed(state.Status))){
    if(!live.ActivityAreaRequired)ReleaseHeatLoading();
    heatShown=live;ShowHeatHub(live);
   }
   // The AAA loading page is painted: its entry re-creates the activity area (the session loads; the AAALoading wipe follows).
   if(reported&&live.ActivityAreaRequired&&heatLoading&&heatLoading.Built&&UnityEngine.Time.frameCount>heatLoadingFrame+1&&heatLoadingEntered!=need.Token.Id&&state.Transitions.HasValue&&
      (state.Transitions.Value.loading_state==(uint)RxCareerLoadingState.Inactive||state.Transitions.Value.loading_state==(uint)RxCareerLoadingState.Finished)){
    var token=need.Token;ulong painted=epoch;
    coordinator.HeatEnterLoading(token,live,()=>Current()&&need!=null&&ReferenceEquals(need.Token,token)&&view.Rendered(painted),()=>heatLoadingEntered=token.Id);
   }
   // The round's activity is set up (its gate handoff ended): release the page, as the Moto page below (Pre and Countdown play over the world).
   if(reported&&live.NextPhase==8&&!live.GateActive&&!live.ActivityAreaRequired&&state.Status.activity_present==1&&state.Status.activity_state<=2&&state.Status.pending_requests==0&&!state.Request.HasValue){view.Hide();need=null;heatShown=null;reported=false;}
  }
  void ShowCareer(bool saved){
   gatePage=false;view.Show("MOTOCAREER","CAREER RACE AND RIDE");Describe();view.Text(saved?"PROGRESS SAVED":"Saving the completed event's player progress…");
   if(saved)view.Button("RETURN TO CAREER","career-return",()=>coordinator.RequestReturnToCareer());LeaveButton();careerSaved=saved;epoch=view.Epoch;
  }
  void TickBlank(){
   if(!view.Blank||need?.Kind!=OwnCareerNeed.Hub||UnityEngine.Time.unscaledTime-blankSince<BlankStall)return;
   blankStalled=true;UnityEngine.Debug.LogWarning("[Career] the hand-off to the race has taken "+BlankStall+" s: the page is shown");
   if(motoShown!=null)ShowMotoHub(motoShown);else if(heatShown!=null)ShowHeatHub(heatShown);
  }
  public void Tick(){
   if(disposed)return;if(!Current()){Dispose();return;}if(coordinator==null){view.Tick(false);return;}
   TickBlank();
   if(coordinator.Failure!=null){if(view.Visible)view.Hide(false);hiddenForError=true;view.Tick(false);return;}
   if(hiddenForError){hiddenForError=false;if(need?.Kind==OwnCareerNeed.CareerRoute)ShowCareer(false);}
   TickHeat();
   if(need?.Kind==OwnCareerNeed.Hub&&motoShown!=null){var live=coordinator.CurrentSnapshot?.Moto;if(live!=null&&(live.Cursor!=motoShown.Cursor||live.Gates.Count!=motoShown.Gates.Count||live.GateActive!=motoShown.GateActive||live.Device!=motoShown.Device||live.Phase!=motoShown.Phase||live.SubEvent!=motoShown.SubEvent||live.FirstContinued!=motoShown.FirstContinued||live.FirstPostObserved!=motoShown.FirstPostObserved||live.NextPhase!=motoShown.NextPhase||(live.Complete&&motoContinueShown!=(coordinator.CurrentSnapshot.Status.continue_allowed==1)))){motoShown=live;ShowMotoHub(live);}}
   // Final native Continue now owns post/rewards presentation. Release the
   // old aggregate page before the existing real reward route can be painted.
   if(need?.Kind==OwnCareerNeed.Hub&&motoShown?.Complete==true&&coordinator.CurrentSnapshot.Status.continue_submitted==1){view.Hide();need=null;motoShown=null;reported=false;}
   // Real gate handoff ended: release the page once the moto's activity is set up (Pre / Countdown / InProgress), with no pending
   // native UI. Keep manual gate/inter-Moto pages otherwise. The original's GateSelection page leaves at AdvanceGateSelection; the
   // race's Pre (intro scenario, EventIntro page) and Countdown (the start board, the HUD) play over the world, so the player sees the
   // gate and can clutch-start. Moto 1 hid this page early only because its RulesPopup replaced it; moto 2 has no rules popup, and the
   // old InProgress (2) condition kept it up until GO (w-moto2prep). Post (4) / Inactive (5) are the previous race's activity.
   if(need?.Kind==OwnCareerNeed.Hub&&motoShown!=null){var state=coordinator.CurrentSnapshot;var live=state?.Moto;if(reported&&live!=null&&(live.Phase==6||live.Phase==7)&&live.NextPhase==8&&!live.GateActive&&state.Status.activity_present==1&&state.Status.activity_state<=2&&state.Status.pending_requests==0&&!state.Request.HasValue){view.Hide();need=null;motoShown=null;reported=false;}}
   if(StartupHubDeparted()){view.Hide();need=null;reported=false;}
   if(need?.Kind==OwnCareerNeed.CareerRoute&&coordinator.CanReturnToCareer&&!careerSaved)ShowCareer(true);
   view.Tick(!coordinator.CurtainBlocksInput);if(need==null||reported)return;
   switch(need.Kind){
    case OwnCareerNeed.RulesShow:if(view.Rendered(epoch))reported=coordinator.ReportRulesShown(need.Token);break;
    case OwnCareerNeed.RulesHide:if(view.HiddenRendered(epoch))reported=coordinator.ReportRulesHidden(need.Token);break;
    case OwnCareerNeed.LoadingHub:if(view.Rendered(epoch))reported=coordinator.ReportLoadingHubRendered(need.Token);break;
    case OwnCareerNeed.Hub:if(view.Rendered(epoch))reported=coordinator.ReportHubRendered(need.Token);break;
    case OwnCareerNeed.CareerRoute:
     if(view.Rendered(epoch)){var token=need.Token;ulong rendered=epoch;reported=coordinator.ReportCareerRouteRendered(token,()=>Current()&&ReferenceEquals(need.Token,token)&&view.Rendered(rendered));}break;
   }
  }
  public void CancelEvent(){if(Current())cancelToMenu();}
  public void ReturnToCareer(){if(Current()&&coordinator?.CanReturnToCareer==true)returnToMenu();}
  public void Dispose(){if(disposed)return;StopRulesVoice();disposed=true;need=null;coordinator=null;ReleaseHeatLoading();view.Dispose();}
 }
}
